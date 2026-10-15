// swift-tools-version: 5.9
// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
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
