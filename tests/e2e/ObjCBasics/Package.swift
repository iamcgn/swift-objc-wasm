// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "ObjCBasics",
    targets: [
        .executableTarget(
            name: "ObjCBasics",
            swiftSettings: [.unsafeFlags(["-Xfrontend", "-disable-objc-attr-requires-foundation-module"])]
        ),
    ]
)
