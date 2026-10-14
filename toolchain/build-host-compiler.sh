#!/usr/bin/env bash
# Build and locally install the patched host (macOS arm64) swift-frontend + clang.
# No macOS stdlib, no benchmarks, link jobs capped for a 16 GB machine.
set -euo pipefail
source "$(dirname "$0")/common.sh"
cd "$SWIFT_SOURCE"
exec ./swift/utils/build-script "${COMMON_ARGS[@]}" "$@"
