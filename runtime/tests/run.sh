#!/usr/bin/env bash
# Build and run the runtime tests on WasmKit. Set WITH_OBJC=1 (with CLANG=<patched clang>)
# to include the Objective-C tests.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
CLANG=${CLANG:-$HOME/Library/Developer/Toolchains/swift-6.3.3-RELEASE.xctoolchain/usr/bin/clang}
TOOLBIN="$(dirname "$CLANG")"
WASMKIT=${WASMKIT:-$HOME/Library/Developer/Toolchains/swift-6.3.3-RELEASE.xctoolchain/usr/bin/wasmkit}
BUNDLE=${BUNDLE:-$HOME/.swiftpm/swift-sdks/swift-6.3.3-RELEASE_wasm.artifactbundle/swift-6.3.3-RELEASE_wasm/wasm32-unknown-wasip1}
OUT=${OUT:-$HERE/../build}
WITH_OBJC=${WITH_OBJC:-0}
FLAGS=(-target wasm32-unknown-wasip1 --sysroot "$BUNDLE/WASI.sdk"
  -resource-dir "$BUNDLE/swift.xctoolchain/usr/lib/swift_static/clang" -O1 -g -I "$HERE/../include")
mkdir -p "$OUT/tests"
status=0
for t in "$HERE"/*.c $( [[ "$WITH_OBJC" == 1 ]] && ls "$HERE"/*.m 2>/dev/null ); do
  name="$(basename "${t%.*}")"
  case "$t" in
    *.m) "$CLANG" "${FLAGS[@]}" -fobjc-runtime=ios-7.0 -fno-objc-arc -c "$t" -o "$OUT/tests/$name.o";;
    *.c) "$CLANG" "${FLAGS[@]}" -std=c11 -c "$t" -o "$OUT/tests/$name.o";;
  esac
  "$CLANG" "${FLAGS[@]}" -Wl,--gc-sections "$OUT/tests/$name.o" "$OUT/libobjc.a" -o "$OUT/tests/$name.wasm"
  if "$WASMKIT" run "$OUT/tests/$name.wasm"; then echo "PASS $name"; else echo "FAIL $name"; status=1; fi
done
exit $status
