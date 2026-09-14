import AVFoundation
import AudioProcessingCpp

let engine = AVAudioEngine()
let inputNode = engine.inputNode
let inputFormat = inputNode.outputFormat(forBus: 0)

let desiredFormat = AVAudioFormat(
    commonFormat: .pcmFormatFloat32, sampleRate: 16000, channels: 1, interleaved: false)!

let nearEndConverter = AVAudioConverter(from: inputFormat, to: desiredFormat)!
let farEndConverter = AVAudioConverter(
    from: engine.mainMixerNode.outputFormat(forBus: 0), to: desiredFormat)!

let playerNode = AVAudioPlayerNode()
let testFileURL = URL(fileURLWithPath: "swift_test/test_mono.wav")
let audioFile = try AVAudioFile(forReading: testFileURL)

var audioBridge = AudioBridge()


let listenPort = CommandLine.arguments[1]
var network = Network(&audioBridge, listenPort)

func extractInt16Samples(fromFloatBuffer buffer: AVAudioPCMBuffer) -> [Int16] {
    let frameCount = Int(buffer.frameLength)
    guard let floatData = buffer.floatChannelData else { return [] }
    var samples = [Int16](repeating: 0, count: frameCount)
    for i in 0..<frameCount {
        let clamped = max(-1.0, min(1.0, floatData[0][i]))
        samples[i] = Int16(clamped * 32767.0)
    }
    return samples
}

let micSinkNode = AVAudioSinkNode {
    @Sendable (timestamp, frameCount, audioBufferList) -> OSStatus in
    let inputBuffer = AVAudioPCMBuffer(pcmFormat: inputFormat, bufferListNoCopy: audioBufferList)!
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
            inNumPackets, outStatus in
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

engine.attach(micSinkNode)
engine.connect(inputNode, to: micSinkNode, format: inputFormat)

engine.attach(playerNode)
engine.connect(playerNode, to: engine.mainMixerNode, format: audioFile.processingFormat)

// Far-end capture: installTap instead of a second sink-node connection —
// this observes mainMixerNode's audio without altering the render graph's
// existing connection to outputNode.
engine.mainMixerNode.installTap(onBus: 0, bufferSize: 1024, format: nil) { @Sendable buffer, time in
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
            inNumPackets, outStatus in
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

playerNode.scheduleFile(audioFile, at: nil)

let pollTimer = Timer.scheduledTimer(withTimeInterval: 0.001, repeats: true) { _ in
    network.poll()
}

engine.prepare()
try engine.start()
audioBridge.start()
playerNode.play()

RunLoop.current.run(until: Date().addingTimeInterval(5))
audioBridge.stop()
network.flush()
