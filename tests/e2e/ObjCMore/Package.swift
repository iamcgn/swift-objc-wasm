// swift-tools-version: 5.9
import PackageDescription
let package = Package(name: "ObjCMore", targets: [
    .target(name: "CBlocks"),
    .executableTarget(name: "ObjCMore", dependencies: ["CBlocks"]),
])
