#!/usr/bin/env bash
# Assemble the swift-6.3.3-RELEASE_wasm-objc Swift SDK artifactbundle from:
#   - the stock swift-6.3.3-RELEASE_wasm bundle (WASI.sdk, clang resource dir, layout)
#   - the wasm stdlib built with interop (STDLIB_INSTALL: .../usr/lib/swift_static)
#   - libobjc-wasm (OBJC_INSTALL: lib/libobjc.a + include/objc, include/Block)
#   - the patched host tools (HOST_BUILD: build/objc-wasm)
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
VERSION=${VERSION:-6.3.3}
STOCK=${STOCK:-$HOME/.swiftpm/swift-sdks/swift-${VERSION}-RELEASE_wasm.artifactbundle}
OUT=${OUT:-$HERE/build/swift-${VERSION}-RELEASE_wasm-objc.artifactbundle}
STDLIB_INSTALL=${STDLIB_INSTALL:?path to the interop stdlib install's swift_static dir}
OBJC_INSTALL=${OBJC_INSTALL:-$HERE/runtime/build/install}
HOST_BUILD=${HOST_BUILD:-$HOME/Development/swift-source/build/objc-wasm}
STOCK_TOOLCHAIN=${STOCK_TOOLCHAIN:-$HOME/Library/Developer/Toolchains/swift-${VERSION}-RELEASE.xctoolchain}
NAME="swift-${VERSION}-RELEASE_wasm-objc"
T="$OUT/$NAME/wasm32-unknown-wasip1"

rm -rf "$OUT" && mkdir -p "$T"
cp -R "$STOCK/swift-${VERSION}-RELEASE_wasm/wasm32-unknown-wasip1/WASI.sdk" "$T/"
mkdir -p "$T/swift.xctoolchain/usr/lib" "$T/swift.xctoolchain/usr/bin"
cp -R "$STOCK/swift-${VERSION}-RELEASE_wasm/wasm32-unknown-wasip1/swift.xctoolchain/usr/lib/swift_static" "$T/swift.xctoolchain/usr/lib/"
# Replace the WASI stdlib with the interop build.
rm -rf "$T/swift.xctoolchain/usr/lib/swift_static/wasi"
cp -R "$STDLIB_INSTALL/wasi" "$T/swift.xctoolchain/usr/lib/swift_static/"
cp -R "$STDLIB_INSTALL/shims" "$T/swift.xctoolchain/usr/lib/swift_static/" 2>/dev/null || true
# Objective-C runtime: headers next to the sysroot's includes, library on the link path.
cp -R "$OBJC_INSTALL/include/objc" "$OBJC_INSTALL/include/Block" "$T/WASI.sdk/include/"
cp "$OBJC_INSTALL/lib/libobjc.a" "$T/swift.xctoolchain/usr/lib/swift_static/wasi/"
cp "$OBJC_INSTALL/lib/libobjc.a" "$T/WASI.sdk/lib/wasm32-wasip1/"
# Host tools: patched frontend and clang, stock driver and binutils.
B="$T/swift.xctoolchain/usr/bin"
cp "$HOST_BUILD/swift-macosx-arm64/bin/swift-frontend" "$B/"
for alias in swift swift-autolink-extract swift-symbolgraph-extract swift-api-extract swift-cache-tool; do ln -f "$B/swift-frontend" "$B/$alias"; done
cp "$STOCK_TOOLCHAIN/usr/bin/swift-driver" "$B/swift-driver" && ln -f "$B/swift-driver" "$B/swiftc"
cp "$STOCK_TOOLCHAIN/usr/bin/swift-help" "$B/" 2>/dev/null || true
cp "$HOST_BUILD/llvm-macosx-arm64/bin/clang-21" "$B/clang-21" && ln -sf clang-21 "$B/clang" && ln -sf clang-21 "$B/clang++"
for t in wasm-ld lld llvm-ar llvm-ranlib llvm-nm llvm-objdump; do cp "$STOCK_TOOLCHAIN/usr/bin/$t" "$B/" 2>/dev/null || ln -sf lld "$B/$t"; done
# Compiler support libraries and macro plugins next to the frontend.
mkdir -p "$T/swift.xctoolchain/usr/lib/swift"
cp -R "$HOST_BUILD/swift-macosx-arm64/lib/swift/host" "$T/swift.xctoolchain/usr/lib/swift/"
cp -R "$STOCK_TOOLCHAIN/usr/lib/swift/host/plugins" "$T/swift.xctoolchain/usr/lib/swift/host/" 2>/dev/null || true
cp -R "$HOST_BUILD/llvm-macosx-arm64/lib/clang" "$T/swift.xctoolchain/usr/lib/" 2>/dev/null || true
# Bundle metadata.
cat > "$OUT/info.json" <<JSON
{
  "artifacts": {
    "$NAME": { "type": "swiftSDK", "version": "0.0.1",
      "variants": [ { "path": "$NAME/wasm32-unknown-wasip1/swift-sdk.json" } ] }
  },
  "schemaVersion": "1.0"
}
JSON
cp "$STOCK/swift-${VERSION}-RELEASE_wasm/wasm32-unknown-wasip1/swift-sdk.json" "$T/"
cp "$HERE/sdk/toolset.json" "$T/toolset.json"
echo "assembled $OUT"
echo "install with: swift sdk install $OUT"
