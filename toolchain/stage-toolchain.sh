#!/usr/bin/env bash
# Stage a unified toolchain directory from the build outputs:
#   $TOOLCHAIN_DESTDIR/usr/bin   patched swift-frontend + clang + lld + binutils, stock swift-driver as swiftc
#   $TOOLCHAIN_DESTDIR/usr/lib   lib/swift/host (compiler libs), lib/clang resource dir, stock macro plugins
set -euo pipefail
source "$(dirname "$0")/common.sh"
STOCK_TOOLCHAIN=${STOCK_TOOLCHAIN:-$HOME/Library/Developer/Toolchains/swift-6.3.3-RELEASE.xctoolchain}
SWIFT_BUILD="$SWIFT_SOURCE/build/$BUILD_SUBDIR/swift-macosx-arm64"
LLVM_BUILD="$SWIFT_SOURCE/build/$BUILD_SUBDIR/llvm-macosx-arm64"
U="$TOOLCHAIN_DESTDIR/usr"
rm -rf "$U" && mkdir -p "$U/bin" "$U/lib/swift"
cp "$SWIFT_BUILD/bin/swift-frontend" "$U/bin/"
for alias in swift swift-autolink-extract swift-symbolgraph-extract swift-api-extract swift-cache-tool swift-api-digester; do ln -f "$U/bin/swift-frontend" "$U/bin/$alias"; done
cp "$STOCK_TOOLCHAIN/usr/bin/swift-driver" "$U/bin/swift-driver" && ln -f "$U/bin/swift-driver" "$U/bin/swiftc"
cp "$STOCK_TOOLCHAIN/usr/bin/swift-help" "$U/bin/" 2>/dev/null || true
cp "$LLVM_BUILD/bin/clang-21" "$U/bin/clang-21" && ln -sf clang-21 "$U/bin/clang" && ln -sf clang-21 "$U/bin/clang++" && ln -sf clang-21 "$U/bin/clang-cl"
cp "$LLVM_BUILD/bin/lld" "$U/bin/lld" && for t in wasm-ld ld.lld ld64.lld lld-link; do ln -sf lld "$U/bin/$t"; done
for t in llvm-ar llvm-nm llvm-ranlib llvm-objdump llvm-dwarfdump llvm-lipo; do [[ -x "$LLVM_BUILD/bin/$t" ]] && cp "$LLVM_BUILD/bin/$t" "$U/bin/"; done
[[ -x "$LLVM_BUILD/bin/llvm-ranlib" ]] || ln -sf llvm-ar "$U/bin/llvm-ranlib"
cp -R "$SWIFT_BUILD/lib/swift/host" "$U/lib/swift/"
cp -R "$STOCK_TOOLCHAIN/usr/lib/swift/host/plugins" "$U/lib/swift/host/" 2>/dev/null || true
cp -R "$LLVM_BUILD/lib/clang" "$U/lib/"
ln -sfn ../clang/21 "$U/lib/swift/clang"
echo "staged $U"; ls "$U/bin" | tr '\n' ' '; echo
