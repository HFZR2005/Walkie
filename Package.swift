// swift-tools-version: 6.3
import PackageDescription

let webrtcRoot = "/Users/hifzur/Projects/webrtc-audio-processing/pre_install"

let package = Package(
    name: "Walkie",
    platforms: [
        .macOS(.v13),
        .iOS(.v16),
    ],
    products: [
        .library(name: "WalkieCore", targets: ["WalkieCore"]),
        .executable(name: "Walkie", targets: ["Walkie"]),
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
                    "-I", "\(webrtcRoot)/mac/arm64/include/webrtc-audio-processing-1",
                    "-I", "\(webrtcRoot)/mac/arm64/include",
                ], .when(platforms: [.macOS])),
                .unsafeFlags([
                    "-std=c++17",
                    "-I", "\(webrtcRoot)/ios/arm64/include/webrtc-audio-processing-1",
                    "-I", "\(webrtcRoot)/ios/arm64/include",
                ], .when(platforms: [.iOS])),
            ],
            linkerSettings: [
                .linkedFramework("CoreFoundation"),
                .unsafeFlags([
                    "-L", "\(webrtcRoot)/mac/arm64/lib",
                    "-lwebrtc-audio-processing-1",
                ], .when(platforms: [.macOS])),
                .unsafeFlags([
                    "-L", "\(webrtcRoot)/ios/arm64/lib",
                    "-lwebrtc-audio-processing-1",
                ], .when(platforms: [.iOS])),
            ]
        ),
        .target(
            name: "WalkieCore",
            dependencies: ["AudioProcessingCpp"],
            swiftSettings: [
                .interoperabilityMode(.Cxx)
            ]
        ),
        .executableTarget(
            name: "Walkie",
            dependencies: ["WalkieCore"],
            swiftSettings: [
                .interoperabilityMode(.Cxx)
            ]
        ),
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
