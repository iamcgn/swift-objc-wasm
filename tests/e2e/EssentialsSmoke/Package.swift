// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "EssentialsSmoke",
    targets: [
        .executableTarget(
            name: "EssentialsSmoke",
            swiftSettings: [.unsafeFlags(["-Xfrontend", "-disable-objc-attr-requires-foundation-module"])]
        ),
    ]
)
