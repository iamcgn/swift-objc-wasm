#!/usr/bin/env bash
# Build libobjc.a for wasm32-unknown-wasip1.
#   CLANG   compiler to use (the patched clang is needed for the .m sources)
#   BUNDLE  the Swift SDK's wasm32-unknown-wasip1 directory (WASI.sdk + clang resource dir)
#   OUT     output directory (default build)
#   WITH_OBJC=1  also compile src/foundation-lite/*.m and src/blocks (needs the patched clang)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
CLANG=${CLANG:-$HOME/Library/Developer/Toolchains/swift-6.3.3-RELEASE.xctoolchain/usr/bin/clang}
TOOLBIN="$(dirname "$CLANG")"
BUNDLE=${BUNDLE:-$HOME/.swiftpm/swift-sdks/swift-6.3.3-RELEASE_wasm.artifactbundle/swift-6.3.3-RELEASE_wasm/wasm32-unknown-wasip1}
OUT=${OUT:-$HERE/build}
WITH_OBJC=${WITH_OBJC:-0}
AR=${AR:-$TOOLBIN/llvm-ar}
CFLAGS_COMMON=(-target wasm32-unknown-wasip1 --sysroot "$BUNDLE/WASI.sdk"
  -resource-dir "$BUNDLE/swift.xctoolchain/usr/lib/swift_static/clang"
  -O2 -g -fno-exceptions -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
  -I "$HERE/include" -I "$HERE/src")
mkdir -p "$OUT/obj"
objs=()
for f in "$HERE"/src/*.c; do
  o="$OUT/obj/$(basename "${f%.c}").o"
  "$CLANG" "${CFLAGS_COMMON[@]}" -std=c11 -c "$f" -o "$o"
  objs+=("$o")
done
if [[ "$WITH_OBJC" == 1 ]]; then
  for f in "$HERE"/src/foundation-lite/*.m "$HERE"/src/blocks/*.c; do
    [[ -e "$f" ]] || continue
    o="$OUT/obj/$(basename "${f%.*}").o"
    case "$f" in
      *.m) "$CLANG" "${CFLAGS_COMMON[@]}" -fobjc-runtime=ios-7.0 -fno-objc-arc -c "$f" -o "$o";;
      *.c) "$CLANG" "${CFLAGS_COMMON[@]}" -std=c11 -c "$f" -o "$o";;
    esac
    objs+=("$o")
  done
fi
rm -f "$OUT/libobjc.a"
"$AR" rcs "$OUT/libobjc.a" "${objs[@]}"
echo "built $OUT/libobjc.a (${#objs[@]} objects)"
