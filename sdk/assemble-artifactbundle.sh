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
# Host tools and compiler libraries: the staged toolchain from toolchain/stage-toolchain.sh.
STAGED=${STAGED:-$HOME/Development/swift-source/build/objc-wasm/toolchain-macosx-arm64/usr}
cp -R "$STAGED/bin" "$T/swift.xctoolchain/usr/"
mkdir -p "$T/swift.xctoolchain/usr/lib/swift"
cp -R "$STAGED/lib/swift/host" "$T/swift.xctoolchain/usr/lib/swift/"
cp -R "$STAGED/lib/clang" "$T/swift.xctoolchain/usr/lib/"
ln -sfn "$(readlink "$STAGED/lib/swift/macosx")" "$T/swift.xctoolchain/usr/lib/swift/macosx"
# Bundle metadata.
printf '{\n  "artifacts": {\n    "%s": { "type": "swiftSDK", "version": "0.0.1",\n      "variants": [ { "path": "%s/wasm32-unknown-wasip1/swift-sdk.json" } ] }\n  },\n  "schemaVersion": "1.0"\n}\n' "$NAME" "$NAME" > "$OUT/info.json"
cp "$STOCK/swift-${VERSION}-RELEASE_wasm/wasm32-unknown-wasip1/swift-sdk.json" "$T/"
cp "$HERE/sdk/toolset.json" "$T/toolset.json"
echo "assembled $OUT"
echo "install with: swift sdk install $OUT"
