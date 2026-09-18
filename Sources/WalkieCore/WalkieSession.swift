import AVFoundation
import AudioProcessingCpp
import Foundation

public enum WalkieError: Error {
    case invalidInputFormat
    case converterFailed
}

/// Mic → AEC → UDP send, and UDP recv → pback → speaker → tap → far-end.
/// Same session model as the Mac CLI: listen port + remote host:port.
public final class WalkieSession: @unchecked Sendable {
    public let listenPort: String
    public let remoteHost: String
    public let remotePort: String

    private var audioBridge = AudioBridge()
    private var network: Network?
    private var engine: AVAudioEngine?
    private var playerNode: AVAudioPlayerNode?
    private var sendTimer: Timer?
    private var playbackTimer: Timer?
    private var running = false

    public init(listenPort: String, remoteHost: String, remotePort: String) {
        self.listenPort = listenPort
        self.remoteHost = remoteHost
        self.remotePort = remotePort
    }

    public func start() throws {
        stop()

        #if os(iOS)
        let session = AVAudioSession.sharedInstance()
        try session.setCategory(
            .playAndRecord,
            mode: .voiceChat,
            options: [.defaultToSpeaker, .allowBluetoothHFP]
        )
        try session.setActive(true)
        #endif

        let engine = AVAudioEngine()
        let inputNode = engine.inputNode
        let inputFormat = inputNode.outputFormat(forBus: 0)
        guard inputFormat.sampleRate > 0, inputFormat.channelCount > 0 else {
            throw WalkieError.invalidInputFormat
        }

        guard
            let desiredFormat = AVAudioFormat(
                commonFormat: .pcmFormatFloat32,
                sampleRate: 16000,
                channels: 1,
                interleaved: false
            ),
            let nearEndConverter = AVAudioConverter(from: inputFormat, to: desiredFormat)
        else {
            throw WalkieError.converterFailed
        }

        audioBridge = AudioBridge()
        let remote = Endpoint(remoteHost, remotePort)
        var network = Network(&audioBridge, listenPort, remote)
        self.network = network

        let playerNode = AVAudioPlayerNode()
        let frameSamples = Int(kSamplesPerFrame)

        let micSinkNode = AVAudioSinkNode {
            [audioBridge = self.audioBridge] _, frameCount, audioBufferList -> OSStatus in
            let inputBuffer = AVAudioPCMBuffer(
                pcmFormat: inputFormat, bufferListNoCopy: audioBufferList)!
            inputBuffer.frameLength = frameCount
            Self.convert(
                inputBuffer, through: nearEndConverter, to: desiredFormat
            ) { ptr, count in
                var bridge = audioBridge
                bridge.pushNearEnd(ptr, count)
            }
            return noErr
        }

        engine.attach(micSinkNode)
        engine.connect(inputNode, to: micSinkNode, format: inputFormat)
        engine.attach(playerNode)
        engine.connect(playerNode, to: engine.mainMixerNode, format: desiredFormat)

        func makePlaybackBuffer() -> AVAudioPCMBuffer? {
            guard
                let buffer = AVAudioPCMBuffer(
                    pcmFormat: desiredFormat,
                    frameCapacity: AVAudioFrameCount(frameSamples)
                )
            else {
                return nil
            }
            buffer.frameLength = AVAudioFrameCount(frameSamples)
            guard let data = buffer.floatChannelData?[0] else { return nil }
            for i in 0..<frameSamples {
                var sample: Int16 = 0
                if self.audioBridge.popPlayback(&sample) {
                    data[i] = Float(sample) / 32767.0
                } else {
                    data[i] = 0
                }
            }
            return buffer
        }

        engine.prepare()
        try engine.start()

        // Mixer format is only final after start. Tapping/converting with the
        // pre-start format is what triggered FillComplexProc.
        let mixerFormat = engine.mainMixerNode.outputFormat(forBus: 0)
        guard mixerFormat.sampleRate > 0,
            let farEndConverter = AVAudioConverter(from: mixerFormat, to: desiredFormat)
        else {
            throw WalkieError.converterFailed
        }

        engine.mainMixerNode.installTap(onBus: 0, bufferSize: 1024, format: mixerFormat) {
            [audioBridge = self.audioBridge] buffer, _ in
            Self.convert(buffer, through: farEndConverter, to: desiredFormat) { ptr, count in
                var bridge = audioBridge
                bridge.pushFarEnd(ptr, count)
            }
        }

        network.start()
        audioBridge.start()
        playerNode.play()

        for _ in 0..<3 {
            if let buffer = makePlaybackBuffer() {
                playerNode.scheduleBuffer(buffer)
            }
        }

        self.engine = engine
        self.playerNode = playerNode
        running = true

        sendTimer = Timer.scheduledTimer(withTimeInterval: 0.001, repeats: true) { [weak self] _ in
            self?.network?.send()
        }
        playbackTimer = Timer.scheduledTimer(withTimeInterval: 0.010, repeats: true) { [weak self] _ in
            guard let self, let playerNode = self.playerNode else { return }
            if let buffer = makePlaybackBuffer() {
                playerNode.scheduleBuffer(buffer)
            }
        }
    }

    public func stop() {
        sendTimer?.invalidate()
        playbackTimer?.invalidate()
        sendTimer = nil
        playbackTimer = nil

        playerNode?.stop()
        engine?.mainMixerNode.removeTap(onBus: 0)
        engine?.stop()
        engine = nil
        playerNode = nil

        network?.stop()
        network = nil
        audioBridge.stop()
        running = false
    }

    deinit {
        stop()
    }

    private static func convert(
        _ buffer: AVAudioPCMBuffer,
        through converter: AVAudioConverter,
        to desiredFormat: AVAudioFormat,
        push: (UnsafePointer<Int16>, Int32) -> Void
    ) {
        guard buffer.frameLength > 0 else { return }
        let input = staged(buffer, as: converter.inputFormat) ?? buffer

        let outputFrameCapacity = AVAudioFrameCount(
            max(Double(input.frameLength) * 16000.0 / input.format.sampleRate, 1))
        guard
            let outputBuffer = AVAudioPCMBuffer(
                pcmFormat: desiredFormat, frameCapacity: outputFrameCapacity)
        else {
            return
        }

        var error: NSError?
        var hasProvidedData = false
        var status: AVAudioConverterOutputStatus
        repeat {
            status = converter.convert(to: outputBuffer, error: &error) { _, outStatus in
                if hasProvidedData {
                    outStatus.pointee = .noDataNow
                    return nil
                }
                hasProvidedData = true
                outStatus.pointee = .haveData
                return input
            }

            let frameCountOut = Int(outputBuffer.frameLength)
            guard frameCountOut > 0, let floatData = outputBuffer.floatChannelData else { break }
            var samples = [Int16](repeating: 0, count: frameCountOut)
            for i in 0..<frameCountOut {
                let clamped = max(-1.0, min(1.0, floatData[0][i]))
                samples[i] = Int16(clamped * 32767.0)
            }
            samples.withUnsafeBufferPointer { ptr in
                if let base = ptr.baseAddress {
                    push(base, Int32(frameCountOut))
                }
            }
        } while status == .haveData
    }

    /// Converter FillComplexProc requires the fed buffer's format object to
    /// `isEqual` the converter input format, not merely the same rate/channels.
    private static func staged(
        _ buffer: AVAudioPCMBuffer, as format: AVAudioFormat
    ) -> AVAudioPCMBuffer? {
        if buffer.format.isEqual(format) {
            return buffer
        }
        guard buffer.format.sampleRate == format.sampleRate,
            let out = AVAudioPCMBuffer(pcmFormat: format, frameCapacity: buffer.frameLength),
            let src = buffer.floatChannelData,
            let dst = out.floatChannelData
        else {
            return nil
        }
        out.frameLength = buffer.frameLength
        let channels = Int(min(buffer.format.channelCount, format.channelCount))
        let frames = Int(buffer.frameLength)
        for channel in 0..<channels {
            dst[channel].update(from: src[channel], count: frames)
        }
        return out
    }
}
