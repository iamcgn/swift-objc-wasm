#!/usr/bin/env bash
# Build each e2e package with the interop SDK and run it on WasmKit. A package whose directory
# ends in `.xfail` must fail to compile (a documented unsupported case).
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SDK=${SDK:-swift-6.3.3-RELEASE_wasm-objc}
WASMKIT=${WASMKIT:-$HOME/Library/Developer/Toolchains/swift-6.3.3-RELEASE.xctoolchain/usr/bin/wasmkit}
status=0
for pkg in "$HERE"/*/; do
  dir="$(basename "$pkg")"; name="${dir%.xfail}"
  log=$(cd "$pkg" && swift build --swift-sdk "$SDK" --scratch-path .build 2>&1); built=$?
  if [[ "$dir" == *.xfail ]]; then
    if [[ $built -ne 0 ]] && grep -q "error:" <<<"$log"; then echo "XFAIL $name (does not compile, as documented): $(grep -m1 'error:' <<<"$log" | sed -E 's/\x1b\[[0-9;]*m//g' | sed 's/.*error: //')"; else echo "FAIL $name compiled but must not"; status=1; fi
    continue
  fi
  if [[ $built -ne 0 ]]; then echo "FAIL $name (build)"; grep -m3 "error:" <<<"$log"; status=1; continue; fi
  if "$WASMKIT" run "$pkg/.build/debug/$name.wasm"; then echo "PASS $name"; else echo "FAIL $name (run)"; status=1; fi
done
exit $status
