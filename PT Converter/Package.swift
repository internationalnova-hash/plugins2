// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "PTConverter",
    platforms: [.macOS(.v13)],
    targets: [
        .executableTarget(
            name: "PTConverter",
            path: "Sources/PTConverter",
            swiftSettings: [
                .unsafeFlags(["-framework", "AVFoundation", "-framework", "AppKit"])
            ]
        )
    ]
)
