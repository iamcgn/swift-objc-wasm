# Third-party notices

swift-objc-wasm is licensed as described in `LICENSE.txt` and `NOTICE.txt`. The files and
components below come from other projects and keep their own copyright notices and
licenses. The SDK artifactbundle built by `sdk/assemble-artifactbundle.sh` redistributes the
components in the second table in binary form; this file is placed at the bundle root.

## Files in this repository

| Files | Origin | License |
|---|---|---|
| `runtime/src/blocks/{Block.h,Block_private.h,data.c,runtime.c}`, `runtime/include/Block.h`, `runtime/include/Block/Block.h` | Unmodified copies of `src/BlocksRuntime/` from [swift-corelibs-libdispatch](https://github.com/swiftlang/swift-corelibs-libdispatch) at commit `ffcd1b88af2f923501b16d9718c53275408e0dbf` (`swift-6.3-DEVELOPMENT-SNAPSHOT-2026-06-07-a`). Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors. | Apache License 2.0 with Runtime Library Exception (text below) |
| `runtime/overlay/ObjectiveC.swift`, `runtime/overlay/CMakeLists.txt` | Modified from `stdlib/public/Darwin/ObjectiveC/` of [swift](https://github.com/swiftlang/swift) at tag `swift-5.2-RELEASE`. Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors; modifications as stated in the file headers. | Apache License 2.0 with Runtime Library Exception |
| `patches/swift/*.patch`, `toolchain/apply-patches.py`, `toolchain/apply-stdlib-patches.py` (the swift hunks) | Modifications to [swift](https://github.com/swiftlang/swift) at tag `swift-6.3.3-RELEASE`; the patch context and the replaced lines are Apple's and the Swift project authors'. | Apache License 2.0 with Runtime Library Exception |
| `patches/llvm-project/*.patch`, `toolchain/apply-patches.py` (the clang hunks) | Modifications to [llvm-project](https://github.com/swiftlang/llvm-project) at tag `swift-6.3.3-RELEASE`; the patch context is the LLVM Project's. | Apache License 2.0 with LLVM Exceptions (text below) |
| `tests/wasm-sections/`, `runtime/src/image.c` | The `__start_`/`__stop_` section-discovery idiom follows `stdlib/public/runtime/SwiftRT-ELF-WASM.cpp` of swift (a few lines of C, rewritten). | Apache License 2.0 with Runtime Library Exception |

`runtime/src/abi.h`, `runtime/include/objc/*.h` and `runtime/src/foundation-lite/*` are
original work that reproduces the metadata layouts, type names, declarations and macros of
Apple's [objc4](https://github.com/apple-oss-distributions/objc4) public API, so that code
compiled for that runtime links and runs on WebAssembly. No objc4 implementation code or
documentation text is used; objc4 is under the Apple Public Source License 2.0 and is not
redistributed here. `tests/provenance-check.sh` verifies the absence of verbatim objc4 text.

## Components redistributed in the SDK artifactbundle

| Component | Origin | License |
|---|---|---|
| Swift compiler, driver, host libraries, macro plugins, the wasm32 standard library and runtime (patched as in `patches/`) | [swiftlang/swift](https://github.com/swiftlang/swift), [swift-driver](https://github.com/swiftlang/swift-driver), [swift-syntax](https://github.com/swiftlang/swift-syntax), tag `swift-6.3.3-RELEASE` | Apache License 2.0 with Runtime Library Exception |
| clang, clang++, lld/wasm-ld, llvm-ar, llvm-ranlib, the clang resource directory and compiler-rt builtins | [swiftlang/llvm-project](https://github.com/swiftlang/llvm-project), tag `swift-6.3.3-RELEASE` | Apache License 2.0 with LLVM Exceptions |
| WASI sysroot: wasi-libc, libc++, libc++abi (the `WASI.sdk` directory, taken from swift.org's `swift-6.3.3-RELEASE_wasm` bundle) | [wasi-libc](https://github.com/WebAssembly/wasi-libc) via [wasi-sdk](https://github.com/WebAssembly/wasi-sdk); libc++/libc++abi from the LLVM Project | wasi-libc: Apache License 2.0 with LLVM Exceptions, Apache License 2.0, or MIT, at your option, with the third-party portions it lists (musl: MIT; cloudlibc: BSD-2-Clause; dlmalloc: CC0; emmalloc: MIT; musl-fts: BSD-3-Clause). libc++/libc++abi: Apache License 2.0 with LLVM Exceptions |
| FoundationEssentials, FoundationInternationalization, _FoundationCShims, _FoundationCollections | [swift-foundation](https://github.com/swiftlang/swift-foundation), [swift-collections](https://github.com/apple/swift-collections) | Apache License 2.0 with Runtime Library Exception |
| _FoundationICU | [swift-foundation-icu](https://github.com/swiftlang/swift-foundation-icu), which packages ICU | Apache License 2.0 with Runtime Library Exception; the ICU sources and data are © 2016 and later Unicode, Inc. and others, under the Unicode License (https://www.unicode.org/copyright.html; text below) |
| libobjc-wasm (`libobjc.a`, `objc/` and `Block/` headers) and the ObjectiveC overlay | this repository | Apache License 2.0 with Runtime Library Exception |

## License texts

### Apache License 2.0 with Runtime Library Exception

The Apache License 2.0 text is in `LICENSE.txt`. The Runtime Library Exception reads:

> As an exception, if you use this Software to compile your source code and portions of this
> Software are embedded into the binary product as a result, you may redistribute such product
> without providing attribution as would otherwise be required by Sections 4(a), 4(b) and 4(d)
> of the License.

### Apache License 2.0 with LLVM Exceptions

The Apache License 2.0 text is in `LICENSE.txt`. The LLVM Exceptions read:

> As an exception, if, as a result of your compiling your source code, portions of this
> Software are embedded into an Object form of such source code, you may redistribute such
> embedded portions in such Object form without complying with the conditions of Sections
> 4(a), 4(b) and 4(d) of the License.
>
> In addition, if you combine or link compiled forms of this Software with software that is
> licensed under the GPLv2 ("Combined Software") and if a court of competent jurisdiction
> determines that the patent provision (Section 3), the indemnity provision (Section 9) or
> other Section of the License conflicts with the conditions of the GPLv2, you may
> retroactively and prospectively choose to deem waived or otherwise exclude such Section(s)
> of the License, but only in their entirety and only with respect to the Combined Software.

### MIT License (wasi-libc, musl, emmalloc)

> Permission is hereby granted, free of charge, to any person obtaining a copy of this
> software and associated documentation files (the "Software"), to deal in the Software
> without restriction, including without limitation the rights to use, copy, modify, merge,
> publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
> to whom the Software is furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all copies or
> substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
> INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
> PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
> FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
> OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
> DEALINGS IN THE SOFTWARE.

### Unicode License (ICU)

From https://www.unicode.org/license.txt:

> UNICODE LICENSE V3
>
> COPYRIGHT AND PERMISSION NOTICE
>
> Copyright © 1991-2026 Unicode, Inc.
>
> NOTICE TO USER: Carefully read the following legal agreement. BY
> DOWNLOADING, INSTALLING, COPYING OR OTHERWISE USING DATA FILES, AND/OR
> SOFTWARE, YOU UNEQUIVOCALLY ACCEPT, AND AGREE TO BE BOUND BY, ALL OF THE
> TERMS AND CONDITIONS OF THIS AGREEMENT. IF YOU DO NOT AGREE, DO NOT
> DOWNLOAD, INSTALL, COPY, DISTRIBUTE OR USE THE DATA FILES OR SOFTWARE.
>
> Permission is hereby granted, free of charge, to any person obtaining a
> copy of data files and any associated documentation (the "Data Files") or
> software and any associated documentation (the "Software") to deal in the
> Data Files or Software without restriction, including without limitation
> the rights to use, copy, modify, merge, publish, distribute, and/or sell
> copies of the Data Files or Software, and to permit persons to whom the
> Data Files or Software are furnished to do so, provided that either (a)
> this copyright and permission notice appear with all copies of the Data
> Files or Software, or (b) this copyright and permission notice appear in
> associated Documentation.
>
> THE DATA FILES AND SOFTWARE ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
> KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
> MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
> THIRD PARTY RIGHTS.
>
> IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS NOTICE
> BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES,
> OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
> WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
> ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE DATA
> FILES OR SOFTWARE.
>
> Except as contained in this notice, the name of a copyright holder shall
> not be used in advertising or otherwise to promote the sale, use or other
> dealings in these Data Files or Software without prior written
> authorization of the copyright holder.

