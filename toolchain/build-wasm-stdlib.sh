#!/usr/bin/env bash
# Build the wasm32-unknown-wasip1 standard library with Objective-C interop against
# libobjc-wasm, using the host compiler from build-host-compiler.sh.
#   SWIFT_OBJC_WASM_RUNTIME  install dir of libobjc-wasm (default runtime/build/install)
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
SWIFT_SOURCE=${SWIFT_SOURCE:-$HOME/Development/swift-source}
export DEVELOPER_DIR=${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}
MACOS_SDK=${MACOS_SDK:-/Library/Developer/CommandLineTools/SDKs/MacOSX26.sdk}
export SWIFT_BUILD_MACOS_SYSROOT="$MACOS_SDK"
export SWIFT_OBJC_WASM_RUNTIME=${SWIFT_OBJC_WASM_RUNTIME:-$HERE/runtime/build/install}
[[ -f "$SWIFT_OBJC_WASM_RUNTIME/lib/libobjc.a" ]] || { echo "libobjc.a not found under $SWIFT_OBJC_WASM_RUNTIME; run runtime/build.sh first"; exit 1; }
JOBS=${JOBS:-6}
cd "$SWIFT_SOURCE"
exec ./swift/utils/build-script --release --assertions --build-subdir objc-wasm \
  --skip-ios --skip-tvos --skip-watchos --skip-xros --skip-build-benchmarks \
  --skip-test-swift --skip-test-cmark --skip-build-compiler-rt \
  --build-swift-dynamic-stdlib=0 --build-swift-dynamic-sdk-overlay=0 \
  --build-swift-static-stdlib=0 --build-swift-static-sdk-overlay=0 \
  --llvm-targets-to-build="AArch64;X86;WebAssembly" \
  --build-wasm-stdlib --skip-test-wasm-stdlib \
  --extra-cmake-options="-DLLVM_PARALLEL_LINK_JOBS=1 -DSWIFT_PARALLEL_LINK_JOBS=1 -DCMAKE_OSX_SYSROOT:PATH=$MACOS_SDK" \
  --jobs "$JOBS" "$@"
