// swift-tools-version: 6.3
import PackageDescription
let package = Package(
    name: "swift_test",
    platforms: [
        .macOS(.v12)
    ],
    targets: [
        // The C++ library target
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
        // Your Swift target, now depending on the C++ one
        .executableTarget(
            name: "swift_test",
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
    ],
    swiftLanguageModes: [.v6],
)
