#!/usr/bin/env bash
# Build the wasm32-unknown-wasip1 standard library with Objective-C interop against
# libobjc-wasm, using the locally installed patched host compiler.
#   SWIFT_OBJC_WASM_RUNTIME  install dir of libobjc-wasm (default runtime/build/install)
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
source "$(dirname "$0")/common.sh"
export SWIFT_OBJC_WASM_RUNTIME=${SWIFT_OBJC_WASM_RUNTIME:-$HERE/runtime/build/install}
[[ -f "$SWIFT_OBJC_WASM_RUNTIME/lib/libobjc.a" ]] || { echo "libobjc.a not found under $SWIFT_OBJC_WASM_RUNTIME; run runtime/build.sh first"; exit 1; }
cd "$SWIFT_SOURCE"
exec ./swift/utils/build-script "${COMMON_ARGS[@]}" --build-wasm-stdlib --skip-test-wasm-stdlib "$@"
