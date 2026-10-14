# swift-objc-wasm

An Objective-C runtime for WebAssembly (`libobjc-wasm`) that implements the Apple objc4 ABI,
plus the small compiler/stdlib patches and SDK packaging needed to build Swift for
`wasm32-unknown-wasip1` with `-enable-objc-interop` (`@objc`, `#selector`, `NSObject`,
`perform(_:)`, `@objc protocol`).

Goal: let [swiftui-web](https://github.com/iamcgn/swiftui-web) build unmodified upstream iOS
apps whose entry point is a `UIApplicationDelegate` using selectors.

Layout:

- `runtime/`   libobjc-wasm: objc4-ABI runtime, NSObject, Foundation-lite, blocks runtime, tests (WasmKit)
- `patches/`   `git format-patch` series against `swift-6.3.3-RELEASE` for swiftlang/swift and swiftlang/llvm-project
- `toolchain/` checkout and build scripts (host compiler + wasm stdlib with interop)
- `sdk/`       assembly of the `swift-6.3.3-RELEASE_wasm-objc` Swift SDK artifactbundle
- `tests/`     link-level experiments and end-to-end SwiftPM test packages

The design plan lives in `PLAN.md`.
