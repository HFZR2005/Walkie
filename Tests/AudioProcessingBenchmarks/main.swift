// Tests/AudioProcessingBenchmarks/main.swift
//
// How to add a benchmark
//
// 1. Add a `benchmark("your name") { ... }` block below.
// 2. Put only the code you want measured inside the closure body — setup
//    that shouldn't count (constructing AudioBridge, building sample data)
//    goes outside it if it's shared across iterations, or inside if you're
//    deliberately measuring a fresh construction each time.
// 3. Run with:  swift run -c release AudioProcessingBenchmarks
//    (NOT `swift run` alone — debug builds give meaningless timings.)


import Benchmark
import AudioProcessingCpp

func ramp(_ count: Int, start: Int16 = 0) -> [Int16] {
    (0..<count).map { start + Int16($0) }
}

var passthroughBridge = AudioBridge()
passthroughBridge.setPassthrough(true)

var aec3Bridge = AudioBridge()
aec3Bridge.setPassthrough(false)

let nearSamples = ramp(Int(kSamplesPerFrame))
let farSamples = ramp(Int(kSamplesPerFrame), start: 500)

benchmark("RingBuffer push, one frame") {
    nearSamples.withUnsafeBufferPointer { ptr in
        _ = passthroughBridge.pushNearEnd(ptr.baseAddress, Int32(nearSamples.count))
    }
    var s: Int16 = 0
    while passthroughBridge.popOutput(&s) {}  // keep buffers from growing across iterations, but outside old combined measurement
}

benchmark("process one frame, passthrough — push+process only") {
    nearSamples.withUnsafeBufferPointer { nearPtr in
        farSamples.withUnsafeBufferPointer { farPtr in
            _ = passthroughBridge.pushNearEnd(nearPtr.baseAddress, Int32(nearSamples.count))
            _ = passthroughBridge.pushFarEnd(farPtr.baseAddress, Int32(farSamples.count))
        }
    }
    _ = passthroughBridge.processAvailableFrames()
}

benchmark("drain output buffer only, passthrough") {
    var s: Int16 = 0
    while passthroughBridge.popOutput(&s) {}
}

benchmark("process one frame, full AEC3 — push+process only") {
    nearSamples.withUnsafeBufferPointer { nearPtr in
        farSamples.withUnsafeBufferPointer { farPtr in
            _ = aec3Bridge.pushNearEnd(nearPtr.baseAddress, Int32(nearSamples.count))
            _ = aec3Bridge.pushFarEnd(farPtr.baseAddress, Int32(farSamples.count))
        }
    }
    _ = aec3Bridge.processAvailableFrames()
}

benchmark("drain output buffer only, full AEC3") {
    var s: Int16 = 0
    while aec3Bridge.popOutput(&s) {}
}

Benchmark.main()
