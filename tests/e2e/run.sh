#!/usr/bin/env bash
# Build each e2e package with the interop SDK and run it on WasmKit.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SDK=${SDK:-swift-6.3.3-RELEASE_wasm-objc}
WASMKIT=${WASMKIT:-$HOME/Library/Developer/Toolchains/swift-6.3.3-RELEASE.xctoolchain/usr/bin/wasmkit}
status=0
for pkg in "$HERE"/*/; do
  name="$(basename "$pkg")"
  (cd "$pkg" && swift build --swift-sdk "$SDK" --scratch-path .build 2>&1 | tail -5)
  if "$WASMKIT" run "$pkg/.build/debug/$name.wasm"; then echo "PASS $name"; else echo "FAIL $name"; status=1; fi
done
exit $status
