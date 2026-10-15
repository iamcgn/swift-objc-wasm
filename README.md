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

## Status (2026-10-15)

Working end to end on this machine: the interop SDK `swift-6.3.3-RELEASE_wasm-objc` builds and
runs `tests/e2e/*` on WasmKit, and the unmodified SwiftUI-2048 app (swiftui-web's acceptance
case: `@UIApplicationMain`, `@objc func newGame`, `#selector`, `UIKeyCommand`) runs in Chromium
with ⌘N starting a new game (swiftui-web branch `objc-runtime-wasm`, decision 0023; five
consecutive probe passes on 2026-10-15 against the SDK as assembled here, which ships
FoundationEssentials and libobjc but no corelibs Foundation or CoreFoundation). The runtime's
own tests: `runtime/tests/run.sh` (C) and `WITH_OBJC=1 CLANG=<bundle clang> runtime/tests/run.sh`
(Objective-C).

Compiler-level checks from PLAN.md Phases 0 and 1, `tests/compiler-checks.sh` (2026-10-15):
with interop off, the patched `swift-frontend` and `clang` emit IR that `llvm-diff` finds
identical to the stock swift-6.3.3-RELEASE toolchain's for a wasm32 input (`-O` and `-Onone`;
the only textual differences are declaration order and the value names an assertion build
keeps); the Darwin `swift/test/IRGen/objc_*` lit tests pass with the patched frontend (60
passed, 1 expected failure, 25 unsupported on macOS arm64; `objc_properties.swift` passes when
its two RUN lines are given a resource dir, since the host build carries no macOS stdlib). The
shipped `libswiftCore.a` defines `OBJC_CLASS_$__TtCs12_SwiftObject` and its metaclass. Not
covered: the plan's curated `swift/test/Interpreter/objc_*` list is empty, as every one of those
tests imports Foundation directly or through its Inputs header; `Swift.swiftinterface`'s flag
line carries `-disable-objc-interop` (from `SwiftSource.cmake`) before our
`-enable-objc-interop`, and the last one wins.

## Build

```sh
toolchain/checkout.sh                       # clone swiftlang/swift + companions at swift-6.3.3-RELEASE
toolchain/apply-patches.py                  # compiler patches (clang + swift), idempotent
toolchain/apply-stdlib-patches.py           # stdlib/runtime/build patches, idempotent
toolchain/build-host-compiler.sh            # patched swift-frontend + clang (2–4 h, 16 GB OK)
toolchain/stage-toolchain.sh                # a unified usr/bin + usr/lib for the SDK
CLANG=$SWIFT_SOURCE/build/objc-wasm/llvm-macosx-arm64/bin/clang WITH_OBJC=1 runtime/build.sh
CLANG=... WITH_OBJC=1 runtime/tests/run.sh  # runtime tests on WasmKit
toolchain/build-wasm-stdlib.sh              # wasm stdlib + FoundationEssentials with interop
sdk/assemble-artifactbundle.sh && swift sdk install build/swift-6.3.3-RELEASE_wasm-objc.artifactbundle
tests/e2e/run.sh                            # Swift end-to-end tests on WasmKit
tests/compiler-checks.sh                    # baseline IR vs stock toolchain + Darwin IRGen objc_* lit
```

Known limits of v1: no `String`/`Array`/`Dictionary`/`Error` bridging to Foundation classes
(`String as NSString` is a compile error), no Objective-C exceptions, no corelibs Foundation,
XCTest or Testing in the SDK, single-threaded only. The toolset passes
`-validate-tbd-against-ir=none` (TBDGen knows no Objective-C class symbols off Mach-O) and
`-disable-objc-attr-requires-foundation-module`.
