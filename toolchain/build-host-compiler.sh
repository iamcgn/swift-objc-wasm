#!/usr/bin/env bash
# Build the patched host (macOS arm64) swift-frontend + clang. No macOS stdlib, no benchmarks,
# link jobs capped for a 16 GB machine. Reuses build/objc-wasm across runs.
set -euo pipefail
SWIFT_SOURCE=${SWIFT_SOURCE:-$HOME/Development/swift-source}
export DEVELOPER_DIR=${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}
JOBS=${JOBS:-6}
cd "$SWIFT_SOURCE"
exec ./swift/utils/build-script --release --assertions --build-subdir objc-wasm \
  --skip-ios --skip-tvos --skip-watchos --skip-xros --skip-build-benchmarks \
  --skip-test-swift --skip-test-cmark \
  --build-swift-dynamic-stdlib=0 --build-swift-dynamic-sdk-overlay=0 \
  --build-swift-static-stdlib=0 --build-swift-static-sdk-overlay=0 \
  --llvm-targets-to-build="AArch64;X86;WebAssembly" \
  --extra-cmake-options="-DLLVM_PARALLEL_LINK_JOBS=1 -DSWIFT_PARALLEL_LINK_JOBS=1" \
  --jobs "$JOBS" "$@"
