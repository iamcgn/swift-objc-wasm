#!/usr/bin/env bash
# swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
# SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
# The two compiler-level checks from PLAN.md Phases 0 and 1, run against the patched host build:
#  1. baseline: with interop off (the default on wasm) the patched swift-frontend and clang emit
#     the same IR as the stock swift-6.3.3-RELEASE toolchain for a wasm32 input (llvm-diff);
#  2. lit: swift/test/IRGen/objc_*.swift for Darwin (macOS arm64) pass with the patched frontend.
# The host build has no macOS stdlib (--build-swift-*-stdlib=0), so lit borrows the stock
# toolchain's resource dir; the build's lit.site.cfg names Xcode's macOS 27 SDK, whose Foundation
# interface the 6.3.3 compiler cannot read (`-target-arch-variant`), so a copy pointing at the
# Command Line Tools' macOS 26 SDK is used. objc_properties.swift uses the bare `%swift`
# substitution, which carries no resource dir, so its two RUN lines are repeated by hand.
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/../toolchain/common.sh"
STOCK=${STOCK:-$HOME/Library/Developer/Toolchains/swift-6.3.3-RELEASE.xctoolchain/usr}
STOCK_WASM_SDK=${STOCK_WASM_SDK:-$(ls -d "$HOME"/Library/org.swift.swiftpm/swift-sdks/swift-6.3.3-RELEASE_wasm.artifactbundle/*/wasm32-unknown-wasip1 | head -1)}
BUILD="$SWIFT_SOURCE/build/$BUILD_SUBDIR"
PATCHED_FE="$BUILD/swift-macosx-arm64/bin/swift-frontend"
PATCHED_CLANG="$BUILD/llvm-macosx-arm64/bin/clang"
LLVM_DIFF="$BUILD/llvm-macosx-arm64/bin/llvm-diff"
FILECHECK="$BUILD/llvm-macosx-arm64/bin/FileCheck"
WORK=${WORK:-$(mktemp -d)}; mkdir -p "$WORK"
status=0

echo "== 1. baseline IR, interop off (stock vs patched)"
RES="$STOCK_WASM_SDK/swift.xctoolchain/usr/lib/swift_static"; SYSROOT="$STOCK_WASM_SDK/WASI.sdk"
cat > "$WORK/sample.swift" <<'SWIFT'
open class Base { open var x = 1; open func f(_ y: Int) -> Int { x + y } }
final class Derived: Base { override func f(_ y: Int) -> Int { super.f(y) * 2 } }
protocol P { func g() -> String }
extension Derived: P { func g() -> String { "d\(f(3))" } }
weak var w: Base? = nil
public func run() -> String { let d = Derived(); w = d; return d.g() + String(describing: w == nil) }
print(run())
SWIFT
cat > "$WORK/sample.c" <<'C'
struct S { int a; float b; }; static int (*fp)(struct S); int call(struct S s) { return fp ? fp(s) : s.a; }
C
for opt in -O -Onone; do
  for t in stock patched; do
    fe="$STOCK/bin/swift-frontend"; [[ $t == patched ]] && fe="$PATCHED_FE"
    "$fe" -frontend -emit-ir -target wasm32-unknown-wasip1 -resource-dir "$RES" -sdk "$SYSROOT" $opt -module-name sample "$WORK/sample.swift" -o "$WORK/$t$opt.ll" || { echo "FAIL $t swift-frontend $opt"; status=1; }
  done
  if "$LLVM_DIFF" "$WORK/stock$opt.ll" "$WORK/patched$opt.ll"; then echo "PASS swift IR identical (llvm-diff) at $opt"; else echo "FAIL swift IR differs at $opt"; status=1; fi
done
for t in stock patched; do
  cc="$STOCK/bin/clang"; [[ $t == patched ]] && cc="$PATCHED_CLANG"
  "$cc" -target wasm32-unknown-wasip1 --sysroot "$SYSROOT" -O2 -S -emit-llvm "$WORK/sample.c" -o "$WORK/c$t.ll" || { echo "FAIL $t clang"; status=1; }
done
# (llvm-diff ignores value names, which only assertion builds keep)
if "$LLVM_DIFF" "$WORK/cstock.ll" "$WORK/cpatched.ll"; then echo "PASS clang IR identical (llvm-diff)"; else echo "FAIL clang IR differs"; status=1; fi

echo "== 2. Darwin lit: swift/test/IRGen/objc_*"
SITE="$WORK/litsite"; mkdir -p "$SITE/IRGen"
sed -e "s#/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX[0-9.]*sdk#$MACOS_SDK#g" \
  "$BUILD/swift-macosx-arm64/test-macosx-arm64/lit.site.cfg" > "$SITE/lit.site.cfg"
cp "$BUILD/swift-macosx-arm64/test-macosx-arm64/lit.swift-features.cfg" "$SITE/"
python3 "$SWIFT_SOURCE/llvm-project/llvm/utils/lit/lit.py" -s --no-progress-bar -j "$JOBS" \
  --param test_resource_dir="$STOCK/lib/swift" "$SITE/IRGen" --filter 'objc_' > "$WORK/lit.log" 2>&1
grep -E '^(FAIL|XFAIL):' "$WORK/lit.log" | sort
grep -E '^  (Passed|Unsupported|Expectedly Failed|Failed) *:' "$WORK/lit.log"
failed=$(grep -E '^FAIL:' "$WORK/lit.log" | grep -v 'objc_properties.swift' | wc -l | tr -d ' ')
[[ $failed -eq 0 ]] || { echo "FAIL lit: $failed unexpected failure(s), see $WORK/lit.log"; status=1; }
T="$SWIFT_SOURCE/swift/test/IRGen/objc_properties.swift"
for pair in "10.11 CHECK-NEW" "10.10 CHECK-OLD"; do
  ver=${pair%% *}; prefix=${pair#* }
  if "$PATCHED_FE" -frontend -target "arm64-apple-macosx$ver" -resource-dir "$STOCK/lib/swift" "$T" -disable-target-os-checking -emit-ir -disable-objc-attr-requires-foundation-module -module-name objc_properties 2>/dev/null \
     | "$FILECHECK" -check-prefix=CHECK -check-prefix="$prefix" "$T"; then echo "PASS objc_properties.swift (by hand, macosx$ver $prefix)"; else echo "FAIL objc_properties.swift macosx$ver"; status=1; fi
done
echo "work dir: $WORK"
exit $status
