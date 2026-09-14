// swift-tools-version: 6.3
import PackageDescription
let package = Package(
    name: "Walkie",
    platforms: [
        .macOS(.v12)
    ],
    dependencies: [
        .package(url: "https://github.com/google/swift-benchmark", from: "0.1.0")
    ],
    targets: [
        .target(
            name: "AudioProcessingCpp",
            path: "Sources/AudioProcessingCpp",
            publicHeadersPath: "include",
            cxxSettings: [
                .unsafeFlags([
                "-std=c++17",
                "-I", "/Users/hifzur/Projects/webrtc-audio-processing/pre_install/mac/arm64/include/webrtc-audio-processing-1",
                "-I", "/Users/hifzur/Projects/webrtc-audio-processing/pre_install/mac/arm64/include",
                ])
            ],
            linkerSettings: [
                .linkedFramework("CoreFoundation"),
                .unsafeFlags([
                "-L", "/Users/hifzur/Projects/webrtc-audio-processing/pre_install/mac/arm64/lib",
                "-lwebrtc-audio-processing-1",
            ])
    ]
        ),
        .executableTarget(
            name: "Walkie",
            dependencies: ["AudioProcessingCpp"],
            swiftSettings: [
                .interoperabilityMode(.Cxx)
            ]
        ),
        // C++ tests: `swift run AudioProcessingTests`
        // (Command Line Tools has no XCTest/Testing module for `swift test`.)
        .executableTarget(
            name: "AudioProcessingTests",
            dependencies: ["AudioProcessingCpp"],
            path: "Tests/AudioProcessingTests",
            cxxSettings: [
                .unsafeFlags(["-std=c++17"])
           ],
            linkerSettings: [
                .linkedFramework("CoreFoundation")
            ]
        ),
        // C++ benchmarks: `swift run -c release AudioProcessingBenchmarks`
        // (release build required — debug timings are meaningless.)
        .executableTarget(
            name: "AudioProcessingBenchmarks",
            dependencies: [
                "AudioProcessingCpp",
                .product(name: "Benchmark", package: "swift-benchmark")
            ],
            path: "Tests/AudioProcessingBenchmarks",
            cxxSettings: [
                .unsafeFlags(["-std=c++17"])
            ],
            swiftSettings: [
                .interoperabilityMode(.Cxx)
            ],
            linkerSettings: [
                .linkedFramework("CoreFoundation")
            ]
        ),
    ],
    swiftLanguageModes: [.v6],
)
