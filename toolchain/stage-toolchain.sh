#!/usr/bin/env bash
# swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
# SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
# Stage a unified toolchain directory from the build outputs:
#   $TOOLCHAIN_DESTDIR/usr/bin   patched swift-frontend + clang + lld + binutils, stock swift-driver as swiftc
#   $TOOLCHAIN_DESTDIR/usr/lib   lib/swift/host (compiler libs), lib/clang resource dir, stock macro plugins,
#                                lib/swift/macosx (the stock driver's dylibs), libzstd
# Everything the staged tools load at run time is inside the staged tree or in macOS itself
# (/usr/lib, /System): no path into this machine's home, build tree or Homebrew survives,
# so the bundle works on another Mac. tests/bundle-portability.sh checks that.
set -euo pipefail
source "$(dirname "$0")/common.sh"
STOCK_TOOLCHAIN=${STOCK_TOOLCHAIN:-$HOME/Library/Developer/Toolchains/swift-6.3.3-RELEASE.xctoolchain}
SWIFT_BUILD="$SWIFT_SOURCE/build/$BUILD_SUBDIR/swift-macosx-arm64"
LLVM_BUILD="$SWIFT_SOURCE/build/$BUILD_SUBDIR/llvm-macosx-arm64"
U="$TOOLCHAIN_DESTDIR/usr"
STOCK_MACOSX="$STOCK_TOOLCHAIN/usr/lib/swift/macosx"
rm -rf "$U" && mkdir -p "$U/bin" "$U/lib/swift/macosx"

# Patched compilers and LLVM tools. They were built against Homebrew's zstd (LLVM picks it up
# when it is installed); the dylib is copied next to them and the references rewritten so a
# machine without Homebrew runs them. Done before the hard-linked aliases are made.
cp "$SWIFT_BUILD/bin/swift-frontend" "$U/bin/"
cp "$LLVM_BUILD/bin/clang-21" "$U/bin/clang-21" && ln -sf clang-21 "$U/bin/clang" && ln -sf clang-21 "$U/bin/clang++" && ln -sf clang-21 "$U/bin/clang-cl"
cp "$LLVM_BUILD/bin/lld" "$U/bin/lld" && for t in wasm-ld ld.lld ld64.lld lld-link; do ln -sf lld "$U/bin/$t"; done
for t in llvm-ar llvm-nm llvm-ranlib llvm-objdump llvm-dwarfdump llvm-lipo; do [[ -x "$LLVM_BUILD/bin/$t" ]] && cp "$LLVM_BUILD/bin/$t" "$U/bin/"; done
[[ -x "$LLVM_BUILD/bin/llvm-ranlib" ]] || ln -sf llvm-ar "$U/bin/llvm-ranlib"
for f in "$U"/bin/*; do
  [[ -f "$f" && ! -L "$f" ]] || continue
  zstd=$(otool -L "$f" | awk '/libzstd/ {print $1}' | head -1)
  [[ -n "$zstd" ]] || continue
  if [[ ! -f "$U/lib/libzstd.1.dylib" ]]; then
    cp "$(readlink -f "$zstd")" "$U/lib/libzstd.1.dylib" && chmod u+w "$U/lib/libzstd.1.dylib"
    install_name_tool -id @rpath/libzstd.1.dylib "$U/lib/libzstd.1.dylib"
    codesign -f -s - "$U/lib/libzstd.1.dylib" 2>/dev/null
  fi
  install_name_tool -change "$zstd" @rpath/libzstd.1.dylib "$f"
  otool -l "$f" | command grep -q '@executable_path/../lib$' || install_name_tool -add_rpath @executable_path/../lib "$f"
  codesign -f -s - "$f" 2>/dev/null
done
for alias in swift swift-autolink-extract swift-symbolgraph-extract swift-api-extract swift-cache-tool swift-api-digester; do ln -f "$U/bin/swift-frontend" "$U/bin/$alias"; done

# The stock driver (its rpath is @executable_path/../lib/swift/macosx) with the closure of the
# @rpath dylibs it loads from the stock toolchain; libswift_* resolve in /usr/lib/swift.
# (The build's early swift-driver is linked by absolute path into the build tree.)
cp "$STOCK_TOOLCHAIN/usr/bin/swift-driver" "$U/bin/swift-driver"
ln -f "$U/bin/swift-driver" "$U/bin/swiftc"
cp "$STOCK_TOOLCHAIN/usr/bin/swift-help" "$U/bin/" 2>/dev/null || true
queue=("$U/bin/swift-driver" "$U/bin/swift-help")
while ((${#queue[@]})); do
  f=${queue[0]}; queue=("${queue[@]:1}")
  [[ -f "$f" ]] || continue
  for dep in $(otool -L "$f" | command grep $'^\t@rpath/' | awk '{print $1}' | sed 's|@rpath/||' | sort -u); do
    [[ "$dep" == libswift* ]] && continue   # the OS stdlib in the dyld cache (/usr/lib/swift is an rpath)
    [[ -f "$U/lib/swift/macosx/$dep" ]] && continue
    [[ -f "$STOCK_MACOSX/$dep" ]] || { echo "cannot find $dep (needed by $f)"; exit 1; }
    cp "$STOCK_MACOSX/$dep" "$U/lib/swift/macosx/$dep"; queue+=("$U/lib/swift/macosx/$dep")
  done
done

cp -R "$SWIFT_BUILD/lib/swift/host" "$U/lib/swift/"
cp -R "$STOCK_TOOLCHAIN/usr/lib/swift/host/plugins" "$U/lib/swift/host/" 2>/dev/null || true
cp -R "$LLVM_BUILD/lib/clang" "$U/lib/"
ln -sfn ../clang/21 "$U/lib/swift/clang"
echo "staged $U"; ls "$U/bin" | tr '\n' ' '; echo; ls "$U/lib/swift/macosx" | tr '\n' ' '; echo
