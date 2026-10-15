#!/usr/bin/env bash
# swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
# SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
# Phase 0 gate: metadata discovery through __start_/__stop_ symbols survives wasm-ld --gc-sections.
set -euo pipefail
TC=${TC:-$HOME/Library/Developer/Toolchains/swift-6.3.3-RELEASE.xctoolchain/usr/bin}
SDK=${SDK:-$HOME/.swiftpm/swift-sdks/swift-6.3.3-RELEASE_wasm.artifactbundle/swift-6.3.3-RELEASE_wasm/wasm32-unknown-wasip1/WASI.sdk}
RES=${RES:-$HOME/.swiftpm/swift-sdks/swift-6.3.3-RELEASE_wasm.artifactbundle/swift-6.3.3-RELEASE_wasm/wasm32-unknown-wasip1/swift.xctoolchain/usr/lib/swift_static/clang}
cd "$(dirname "$0")"
for f in a b main; do "$TC/clang" -target wasm32-unknown-wasip1 --sysroot "$SDK" -resource-dir "$RES" -O2 -c $f.c -o $f.o; done
"$TC/clang" -target wasm32-unknown-wasip1 --sysroot "$SDK" -resource-dir "$RES" -Wl,--gc-sections a.o b.o main.o -o sections.wasm
"$TC/wasmkit" run sections.wasm
