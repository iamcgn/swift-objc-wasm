# Compiler and standard-library patches

Each series is a set of `git format-patch` diffs against tag `swift-6.3.3-RELEASE` of the
named tree. `toolchain/apply-patches.py` and `toolchain/apply-stdlib-patches.py` apply the
same changes idempotently to a checkout made by `toolchain/checkout.sh`.

| Series | Tree | What it does |
|---|---|---|
| `llvm-project/0001-objc-wasm-lookup-dispatch-and-sections.patch` | [swiftlang/llvm-project](https://github.com/swiftlang/llvm-project) | clang: Objective-C metadata sections on wasm as on ELF, lists kept through `llvm.used`, message sends through `objc_msgLookup` + indirect call, no vtable-dispatch selectors |
| `swift/0001-objc-wasm-lookup-dispatch.patch` | [swiftlang/swift](https://github.com/swiftlang/swift) | IRGen: `objc_msgLookup`/`objc_msgLookupSuper2` dispatch on wasm, no stret, dynamic-method branches; SILGen: Swift `UIApplicationMain` entry |
| `swift/0002-stdlib-objc-interop-on-wasi.patch` | swiftlang/swift | Build interop into the WASI stdlib: flags, the ObjectiveC overlay, `performSelector` via typed lookup, dlsym-free `FoundationHelpers`, wasm32 weak-reference marker and spare bits |
| `swift/0003-build-script-macos-sysroot-override.patch` | swiftlang/swift | Host build against a chosen macOS SDK (`SWIFT_BUILD_MACOS_SYSROOT`) |

## Licensing

The patches are modifications of the upstream files, and their context lines are upstream
code. They are offered under the license of the tree they modify, so they can be submitted
upstream as they are:

- swiftlang/swift: Apache License 2.0 with Runtime Library Exception
  (https://swift.org/LICENSE.txt). Copyright Apple Inc. and the Swift project authors;
  modifications Copyright 2026 Corey Earwood and the swift-objc-wasm contributors.
- swiftlang/llvm-project: Apache License 2.0 with LLVM Exceptions
  (https://llvm.org/LICENSE.txt). Modifications Copyright 2026 Corey Earwood and the
  swift-objc-wasm contributors.

The rest of this repository is under Apache License 2.0 with Runtime Library Exception; see
`LICENSE.txt`, `NOTICE.txt` and `THIRD_PARTY_NOTICES.md`.

## Rebasing

Apply the series to the next release tag with `git am` (or rerun the two apply scripts,
which patch by anchor text and report any hunk whose anchor is gone), rebuild with
`toolchain/build-host-compiler.sh` and `toolchain/build-wasm-stdlib.sh`, and run
`tests/compiler-checks.sh`, `runtime/tests/run.sh` and `tests/e2e/run.sh`.
