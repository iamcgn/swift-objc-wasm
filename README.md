# swift-objc-wasm

An Objective-C runtime for WebAssembly (`libobjc-wasm`) that implements Apple's objc4 ABI,
plus the small compiler and standard-library patches and the SDK packaging needed to build
Swift for `wasm32-unknown-wasip1` with `-enable-objc-interop`: `@objc`, `#selector`,
`NSObject`, `perform(_:)`, `@objc protocol`, `@objc enum`, blocks.

Why: [swiftui-web](https://github.com/iamcgn/swiftui-web) builds unmodified upstream iOS apps
for the browser, and some of those apps start from a `UIApplicationDelegate` that uses
selectors. On Darwin every Swift class is an Objective-C class; this project keeps that model
on wasm instead of inventing a foreign-class one. The design, the alternatives considered
(GNUstep's libobjc2 among them) and the full plan are in [PLAN.md](PLAN.md).

## How it works

- **Compilers emit objc4-format metadata on wasm.** clang already does for ELF; two small
  patches (`patches/`) let it and Swift's IRGen do the same for wasm, keep the metadata
  through `wasm-ld --gc-sections` via `llvm.used`, and replace `objc_msgSend` (a variadic
  trampoline wasm cannot express) with `imp = objc_msgLookup(receiver, sel); imp(...)`.
- **libobjc-wasm** (`runtime/`, C with a few Objective-C files) loads that metadata from the
  `__start_`/`__stop_` section symbols, realizes classes, categories and protocols, and
  provides dispatch with per-class caches, reference counting (forwarding to `swift_retain`
  for Swift classes), weak references, associated objects, autorelease pools, a typed
  `performSelector`, and a Foundation-lite: the `NSObject` root class, `NSString` for
  `@""` literals and `description`, `NSNull`, and the few CoreFoundation entry points the
  Swift runtime calls. Single-threaded, static linking only, no exceptions, no tagged
  pointers.
- **The Swift standard library is rebuilt for WASI with interop on**, with a port of the
  historical `ObjectiveC` overlay (`ObjCBool`, `Selector`, `NSZone`, `autoreleasepool`).
- **The result is a Swift SDK artifactbundle**, `swift-6.3.3-RELEASE_wasm-objc`, usable with
  `swift build --swift-sdk swift-6.3.3-RELEASE_wasm-objc` from a stock swiftly toolchain of
  the same version. It ships the patched compilers (macOS arm64 host), the interop stdlib,
  FoundationEssentials and its support libraries, and libobjc. It does not ship corelibs
  Foundation, CoreFoundation, XCTest or Swift Testing (see Limits).

## Status (2026-10-15)

Working end to end on macOS arm64: the SDK builds and runs `tests/e2e/*` on WasmKit, and
swiftui-web's acceptance case, the unmodified SwiftUI-2048 app (`@UIApplicationMain`,
`@objc func newGame`, `#selector`, `UIKeyCommand`), runs in Chromium with ⌘N starting a new
game (five consecutive probe passes against the SDK as assembled here; swiftui-web branch
`objc-runtime-wasm`, decision 0023).

Verified on the same date, with the commands under Verify below:

| Check | Outcome |
|---|---|
| `runtime/tests/run.sh` (C) and with `WITH_OBJC=1` (Objective-C) | pass |
| `tests/e2e/run.sh` (ObjCBasics, ObjCMore, EssentialsSmoke; BridgingNegative must not compile) | pass, XFAIL as documented |
| `tests/compiler-checks.sh`: patched compilers emit the same IR as the stock toolchain with interop off (`llvm-diff`, `-O` and `-Onone`) | identical |
| `tests/compiler-checks.sh`: Darwin `swift/test/IRGen/objc_*` lit with the patched frontend | 60 passed, 1 expected failure, 25 unsupported on macOS arm64 |
| `tests/provenance-check.sh` | 0 verbatim objc4 comment lines |

The shipped `libswiftCore.a` defines `OBJC_CLASS_$__TtCs12_SwiftObject` and its metaclass.
The `Swift.swiftinterface` flag line carries `-disable-objc-interop` (from the stdlib's
CMake) before this project's `-enable-objc-interop`; the last one wins.

## Limits

- No bridging of `String`, `Array`, `Dictionary` or `Error` to Foundation classes:
  `String as NSString` is a compile error. Planned next (v1.5): `String` bridging in
  swiftui-web's WebFoundation over the runtime's `NSString`.
- No corelibs Foundation, CoreFoundation, XCTest or Swift Testing in the SDK; test suites run
  on the stock SDK. The eventual direction is corelibs Foundation rebuilt with interop on.
- No Objective-C exceptions (`@throw` aborts), no tagged pointers or non-pointer isa, no KVO,
  KVC or `NSInvocation`, no `objc_msgSend` callable from C or JavaScript, no dynamic
  libraries, no threads.
- `NSObject.description` overrides from Swift need `NSString`, which the Foundation-lite
  only provides for literals and `UTF8String`.
- Host toolset for macOS arm64 only. The toolset passes `-validate-tbd-against-ir=none`
  (TBDGen knows no Objective-C class symbols off Mach-O) and
  `-disable-objc-attr-requires-foundation-module`.

## Requirements to build

- macOS on Apple silicon with the Command Line Tools and their macOS 26 SDK
  (`/Library/Developer/CommandLineTools/SDKs/MacOSX26.sdk`; the LLVM 21 sources do not build
  against the macOS 27 SDK's libc++).
- [swiftly](https://www.swift.org/install/) with Swift 6.3.3 and the stock
  `swift-6.3.3-RELEASE_wasm` SDK installed (`swift sdk list`).
- `cmake`, `ninja`, `python3`, `git`; 16 GB RAM is enough with the link-job caps in
  `toolchain/common.sh`; about 60 GB of disk for the checkout and build.
- The host compiler build takes 2 to 4 hours; the wasm stdlib about 30 minutes.

## Build

```sh
toolchain/checkout.sh                       # clone swiftlang/swift + companions at swift-6.3.3-RELEASE
toolchain/apply-patches.py                  # compiler patches (clang + swift), idempotent
toolchain/apply-stdlib-patches.py           # stdlib/runtime/build patches, idempotent
toolchain/build-host-compiler.sh            # patched swift-frontend + clang
toolchain/stage-toolchain.sh                # a unified usr/bin + usr/lib for the SDK
CLANG=$SWIFT_SOURCE/build/objc-wasm/llvm-macosx-arm64/bin/clang WITH_OBJC=1 runtime/build.sh
toolchain/build-wasm-stdlib.sh              # wasm stdlib + FoundationEssentials with interop
sdk/assemble-artifactbundle.sh && swift sdk install build/swift-6.3.3-RELEASE_wasm-objc.artifactbundle
```

`SWIFT_SOURCE` defaults to `~/Development/swift-source`. Releases carry the assembled bundle
as a `.tar.gz` with a SHA-256 checksum, for `swift sdk install <url> --checksum <sha256>`.

## Verify

```sh
CLANG=... runtime/tests/run.sh              # runtime tests on WasmKit (C)
CLANG=... WITH_OBJC=1 runtime/tests/run.sh  # and Objective-C
tests/e2e/run.sh                            # Swift end-to-end packages on WasmKit
tests/compiler-checks.sh                    # baseline IR vs stock toolchain + Darwin IRGen objc_* lit
tests/provenance-check.sh                   # no verbatim objc4 text (shallow-clones objc4 into a temp dir)
```

`tests/e2e/run.sh` expects WasmKit from the swiftly toolchain (`WASMKIT=` to override).

## Layout

- `runtime/`   libobjc-wasm: objc4-ABI runtime, NSObject, Foundation-lite, blocks runtime, overlay, tests
- `patches/`   `git format-patch` series against `swift-6.3.3-RELEASE` for swiftlang/swift and swiftlang/llvm-project ([patches/README.md](patches/README.md))
- `toolchain/` checkout and build scripts (host compiler + wasm stdlib with interop)
- `sdk/`       assembly of the `swift-6.3.3-RELEASE_wasm-objc` Swift SDK artifactbundle
- `tests/`     link-level experiments, end-to-end SwiftPM packages, compiler and provenance checks

## Reporting problems

Open an issue with the output of the failing command from Verify, `swift --version`, and
`swift sdk list`. Problems in apps built with the SDK through swiftui-web belong in that
repository.

## License

Apache License 2.0 with the Runtime Library Exception (`LICENSE.txt`, `NOTICE.txt`), so
apps that link libobjc-wasm need not carry attribution. The third-party files in this
repository and the components the SDK bundle redistributes are listed with their licenses
in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). libobjc-wasm implements the ABI and
public API of Apple's objc4 for compatibility and contains no objc4 code. Apple, Objective-C
and Swift are trademarks of Apple Inc.; this project is not affiliated with or endorsed by
Apple.
