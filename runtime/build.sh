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
# Blocks runtime (from swift-corelibs-blocksruntime), with weak exports so that a
# CoreFoundation that also carries one does not clash at link time.
for f in "$HERE"/src/blocks/*.c; do
  o="$OUT/obj/blocks_$(basename "${f%.c}").o"
  "$CLANG" "${CFLAGS_COMMON[@]}" -std=gnu11 -Wno-unused-variable -Wno-cast-function-type-mismatch \
    '-DBLOCK_EXPORT=extern __attribute__((weak, visibility("default")))' -c "$f" -o "$o"
  objs+=("$o")
done
# Foundation-lite C parts always; Objective-C parts need the patched clang.
"$CLANG" "${CFLAGS_COMMON[@]}" -std=c11 -c "$HERE/src/foundation-lite/CFLite.c" -o "$OUT/obj/CFLite.o"
objs+=("$OUT/obj/CFLite.o")
if [[ "$WITH_OBJC" == 1 ]]; then
  for f in "$HERE"/src/foundation-lite/*.m; do
    o="$OUT/obj/$(basename "${f%.m}").o"
    "$CLANG" "${CFLAGS_COMMON[@]}" -fobjc-runtime=ios-7.0 -fno-objc-arc -c "$f" -o "$o"
    objs+=("$o")
  done
fi
rm -f "$OUT/libobjc.a"
"$AR" rcs "$OUT/libobjc.a" "${objs[@]}"
echo "built $OUT/libobjc.a (${#objs[@]} objects)"

# Install layout consumed by the stdlib build (SWIFT_OBJC_WASM_RUNTIME) and the SDK assembly.
INSTALL="$OUT/install"
rm -rf "$INSTALL" && mkdir -p "$INSTALL/lib" "$INSTALL/include"
cp "$OUT/libobjc.a" "$INSTALL/lib/"
cp -R "$HERE/include/objc" "$HERE/include/Block" "$HERE/include/_stdlib_shims" "$INSTALL/include/"
echo "installed to $INSTALL"
