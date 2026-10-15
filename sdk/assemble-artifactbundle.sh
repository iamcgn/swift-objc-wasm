#!/usr/bin/env bash
# swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
# SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
# Assemble the swift-6.3.3-RELEASE_wasm-objc Swift SDK artifactbundle from:
#   - the stock swift-6.3.3-RELEASE_wasm bundle (WASI.sdk, clang resource dir, layout)
#   - the wasm stdlib built with interop (STDLIB_INSTALL: a swift_static dir)
#   - FoundationEssentials & co. from the SDK-stage build dir (FOUNDATION_BUILD), if present
#   - libobjc-wasm (OBJC_INSTALL: lib/libobjc.a + include/objc, include/Block)
#   - the staged host toolchain (STAGED: toolchain-macosx-arm64/usr from stage-toolchain.sh)
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
VERSION=${VERSION:-6.3.3}
STOCK=${STOCK:-$HOME/.swiftpm/swift-sdks/swift-${VERSION}-RELEASE_wasm.artifactbundle}
OUT=${OUT:-$HERE/build/swift-${VERSION}-RELEASE_wasm-objc.artifactbundle}
BUILD_ROOT=${BUILD_ROOT:-$HOME/Development/swift-source/build/objc-wasm}
STDLIB_INSTALL=${STDLIB_INSTALL:-$BUILD_ROOT/wasmstdlib-macosx-arm64/lib/swift_static}
FOUNDATION_BUILD=${FOUNDATION_BUILD:-$BUILD_ROOT/wasmswiftsdk-macosx-arm64/foundation/wasm32-unknown-wasip1}
OBJC_INSTALL=${OBJC_INSTALL:-$HERE/runtime/build/install}
STAGED=${STAGED:-$BUILD_ROOT/toolchain-macosx-arm64/usr}
NAME="swift-${VERSION}-RELEASE_wasm-objc"
STOCK_T="$STOCK/swift-${VERSION}-RELEASE_wasm/wasm32-unknown-wasip1"
T="$OUT/$NAME/wasm32-unknown-wasip1"
WASI_LIB="$T/swift.xctoolchain/usr/lib/swift_static/wasi"

[[ -d "$STDLIB_INSTALL/wasi" ]] || { echo "no stdlib at $STDLIB_INSTALL"; exit 1; }
[[ -f "$OBJC_INSTALL/lib/libobjc.a" ]] || { echo "no libobjc.a at $OBJC_INSTALL"; exit 1; }
[[ -x "$STAGED/bin/swift-frontend" ]] || { echo "no staged toolchain at $STAGED"; exit 1; }

rm -rf "$OUT" && mkdir -p "$T/swift.xctoolchain/usr/lib/swift"
cp -R "$STOCK_T/WASI.sdk" "$T/"
cp -R "$STOCK_T/swift.xctoolchain/usr/lib/swift_static" "$T/swift.xctoolchain/usr/lib/"

# The WASI stdlib and shims from the interop build replace the stock ones. The stock
# Foundation/XCTest/Testing libraries were built against the non-interop stdlib: drop them.
rm -rf "$WASI_LIB" "$T/swift.xctoolchain/usr/lib/swift_static/shims"
cp -R "$STDLIB_INSTALL/wasi" "$WASI_LIB"
cp -R "$STDLIB_INSTALL/shims" "$T/swift.xctoolchain/usr/lib/swift_static/"

# FoundationEssentials and its support libraries from the SDK-stage build.
if [[ -d "$FOUNDATION_BUILD/lib" ]]; then
  for name in FoundationEssentials FoundationInternationalization _FoundationCollections; do
    rm -rf "$WASI_LIB/$name.swiftmodule"
    mkdir -p "$WASI_LIB/$name.swiftmodule"
    for ext in swiftmodule swiftdoc swiftinterface private.swiftinterface swiftsourceinfo; do
      [[ -f "$FOUNDATION_BUILD/swift/$name.$ext" ]] && cp "$FOUNDATION_BUILD/swift/$name.$ext" "$WASI_LIB/$name.swiftmodule/wasm32-unknown-wasip1.$ext"
    done
    [[ -f "$FOUNDATION_BUILD/lib/lib$name.a" ]] && cp "$FOUNDATION_BUILD/lib/lib$name.a" "$WASI_LIB/"
  done
  for name in _FoundationCShims _FoundationICU; do
    [[ -f "$FOUNDATION_BUILD/lib/lib$name.a" ]] && cp "$FOUNDATION_BUILD/lib/lib$name.a" "$WASI_LIB/"
  done
fi

# Objective-C runtime: headers in the sysroot, library on both link paths.
cp -R "$OBJC_INSTALL/include/objc" "$OBJC_INSTALL/include/Block" "$T/WASI.sdk/include/"
cp "$OBJC_INSTALL/include/Block.h" "$T/WASI.sdk/include/"
cp "$OBJC_INSTALL/lib/libobjc.a" "$WASI_LIB/"
cp "$OBJC_INSTALL/lib/libobjc.a" "$T/WASI.sdk/lib/wasm32-wasip1/"

# Host tools and compiler libraries.
cp -R "$STAGED/bin" "$T/swift.xctoolchain/usr/"
cp -R "$STAGED/lib/swift/host" "$T/swift.xctoolchain/usr/lib/swift/"
cp -R "$STAGED/lib/clang" "$T/swift.xctoolchain/usr/lib/"
ln -sfn "$(readlink "$STAGED/lib/swift/macosx")" "$T/swift.xctoolchain/usr/lib/swift/macosx"

# Bundle metadata.
cat > "$OUT/info.json" <<JSON
{
  "artifacts": {
    "$NAME": {
      "type": "swiftSDK",
      "version": "0.0.1",
      "variants": [ { "path": "$NAME/wasm32-unknown-wasip1/swift-sdk.json" } ]
    }
  },
  "schemaVersion": "1.0"
}
JSON
cp "$STOCK_T/swift-sdk.json" "$T/"
cp "$HERE/sdk/toolset.json" "$T/toolset.json"

# License and provenance at the bundle root (see THIRD_PARTY_NOTICES.md for what the
# bundle redistributes), and the exact sources this bundle was built from.
cp "$HERE/LICENSE.txt" "$HERE/NOTICE.txt" "$HERE/THIRD_PARTY_NOTICES.md" "$OUT/"
{
  echo "swift-objc-wasm $(git -C "$HERE" describe --tags --always --dirty 2>/dev/null) ($(git -C "$HERE" rev-parse HEAD 2>/dev/null))"
  echo "swift / llvm-project base: swift-6.3.3-RELEASE"
  echo "patch series sha256: $(cat "$HERE"/patches/*/*.patch | shasum -a 256 | cut -d' ' -f1)"
  echo "assembled: $(date -u +%Y-%m-%dT%H:%M:%SZ) on $(uname -m)-$(uname -s | tr '[:upper:]' '[:lower:]')"
} > "$OUT/VERSION"
echo "assembled $OUT"
ls "$WASI_LIB" | tr '\n' ' '; echo
