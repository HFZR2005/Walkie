import AVFoundation
import AudioProcessingCpp

// Discovery later only has to produce these three values. The rest of the
// app should keep taking (listenPort, remote Endpoint), not a hostname lookup.
guard CommandLine.arguments.count == 4 else {
    print("usage: Walkie <listenPort> <remoteHost> <remotePort>")
    print("  same machine:  Walkie 5001 localhost 5002")
    print("                 Walkie 5002 localhost 5001")
    print("  two devices:   Walkie 5000 <other-ip> 5000")
    exit(1)
}

let listenPort = CommandLine.arguments[1]
let remoteHost = CommandLine.arguments[2]
let remotePort = CommandLine.arguments[3]
let remote = Endpoint(remoteHost, remotePort)

let frameSamples = Int(kSamplesPerFrame)

let engine = AVAudioEngine()
let inputNode = engine.inputNode
let inputFormat = inputNode.outputFormat(forBus: 0)

let desiredFormat = AVAudioFormat(
    commonFormat: .pcmFormatFloat32, sampleRate: 16000, channels: 1, interleaved: false)!

guard inputFormat.sampleRate > 0, inputFormat.channelCount > 0 else {
    print("input format is invalid: \(inputFormat)")
    exit(1)
}

let nearEndConverter = AVAudioConverter(from: inputFormat, to: desiredFormat)!

nonisolated(unsafe) var audioBridge = AudioBridge()
nonisolated(unsafe) var network = Network(&audioBridge, listenPort, remote)

let micSinkNode = AVAudioSinkNode {
    @Sendable (_, frameCount, audioBufferList) -> OSStatus in
    let inputBuffer = AVAudioPCMBuffer(pcmFormat: inputFormat, bufferListNoCopy: audioBufferList)!
    inputBuffer.frameLength = frameCount
    let outputFrameCapacity = AVAudioFrameCount(
        Double(frameCount) * 16000.0 / inputFormat.sampleRate)
    guard
        let outputBuffer = AVAudioPCMBuffer(
            pcmFormat: desiredFormat, frameCapacity: outputFrameCapacity)
    else {
        return noErr
    }

    var error: NSError?
    var hasProvidedData = false
    var status: AVAudioConverterOutputStatus
    repeat {
        status = nearEndConverter.convert(to: outputBuffer, error: &error) {
            _, outStatus in
            if hasProvidedData {
                outStatus.pointee = .noDataNow
                return nil
            }
            hasProvidedData = true
            outStatus.pointee = .haveData
            return inputBuffer
        }

        let frameCountOut = Int(outputBuffer.frameLength)
        guard let floatData = outputBuffer.floatChannelData else { break }
        var samples = [Int16](repeating: 0, count: frameCountOut)
        for i in 0..<frameCountOut {
            let clamped = max(-1.0, min(1.0, floatData[0][i]))
            samples[i] = Int16(clamped * 32767.0)
        }
        samples.withUnsafeBufferPointer { ptr in
            audioBridge.pushNearEnd(ptr.baseAddress, Int32(frameCountOut))
        }
    } while status == .haveData

    return noErr
}

// PlayerNode, not SourceNode: pulling C++/Swift from the HAL render callback
// trapped in CoreAudio on the IO thread. The player is filled off-thread at
// the 10 ms frame rate. play() with no data still renders silence, so the
// mixer tap keeps feeding far-end and AEC does not stall.
nonisolated(unsafe) let playerNode = AVAudioPlayerNode()

engine.attach(micSinkNode)
engine.connect(inputNode, to: micSinkNode, format: inputFormat)

engine.attach(playerNode)
engine.connect(playerNode, to: engine.mainMixerNode, format: desiredFormat)

let mixerFormat = engine.mainMixerNode.outputFormat(forBus: 0)
let farEndConverter = AVAudioConverter(from: mixerFormat, to: desiredFormat)!

engine.mainMixerNode.installTap(onBus: 0, bufferSize: 1024, format: mixerFormat) {
    @Sendable buffer, _ in
    let outputFrameCapacity = AVAudioFrameCount(
        Double(buffer.frameLength) * 16000.0 / buffer.format.sampleRate)
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
        status = farEndConverter.convert(to: outputBuffer, error: &error) {
            _, outStatus in
            if hasProvidedData {
                outStatus.pointee = .noDataNow
                return nil
            }
            hasProvidedData = true
            outStatus.pointee = .haveData
            return buffer
        }

        let frameCountOut = Int(outputBuffer.frameLength)
        guard let floatData = outputBuffer.floatChannelData else { break }
        var samples = [Int16](repeating: 0, count: frameCountOut)
        for i in 0..<frameCountOut {
            let clamped = max(-1.0, min(1.0, floatData[0][i]))
            samples[i] = Int16(clamped * 32767.0)
        }
        samples.withUnsafeBufferPointer { ptr in
            audioBridge.pushFarEnd(ptr.baseAddress, Int32(frameCountOut))
        }
    } while status == .haveData
}

@Sendable
func makePlaybackBuffer() -> AVAudioPCMBuffer? {
    guard
        let buffer = AVAudioPCMBuffer(
            pcmFormat: desiredFormat, frameCapacity: AVAudioFrameCount(frameSamples))
    else {
        return nil
    }
    buffer.frameLength = AVAudioFrameCount(frameSamples)
    guard let data = buffer.floatChannelData?[0] else { return nil }
    for i in 0..<frameSamples {
        var sample: Int16 = 0
        if audioBridge.popPlayback(&sample) {
            data[i] = Float(sample) / 32767.0
        } else {
            data[i] = 0
        }
    }
    return buffer
}

let pollTimerSend = Timer.scheduledTimer(withTimeInterval: 0.001, repeats: true) { _ in
    network.send()
}

let pollTimerPlayback = Timer.scheduledTimer(withTimeInterval: 0.010, repeats: true) { _ in
    if let buffer = makePlaybackBuffer() {
        playerNode.scheduleBuffer(buffer)
    }
}

engine.prepare()
try engine.start()

network.start()
audioBridge.start()
playerNode.play()

for _ in 0..<3 {
    if let buffer = makePlaybackBuffer() {
        playerNode.scheduleBuffer(buffer)
    }
}

print("listening on \(listenPort), sending to \(remoteHost):\(remotePort)")
print("input \(inputFormat.sampleRate) Hz / \(inputFormat.channelCount) ch")
print("mixer \(mixerFormat.sampleRate) Hz / \(mixerFormat.channelCount) ch")
print("Ctrl-C to quit")

RunLoop.current.run()
