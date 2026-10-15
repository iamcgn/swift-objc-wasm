#!/usr/bin/env bash
# swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
# SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
# Clone swiftlang/swift and companions at the pinned release tag into $SWIFT_SOURCE (default
# ~/Development/swift-source), then apply the patch series from ../patches.
set -euo pipefail
TAG=${TAG:-swift-6.3.3-RELEASE}
SWIFT_SOURCE=${SWIFT_SOURCE:-$HOME/Development/swift-source}
HERE="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$SWIFT_SOURCE" && cd "$SWIFT_SOURCE"
[ -d swift ] || git clone https://github.com/swiftlang/swift
./swift/utils/update-checkout --clone --tag "$TAG"
for repo in swift llvm-project; do
  shopt -s nullglob
  patches=("$HERE"/patches/$repo/*.patch)
  [ ${#patches[@]} -gt 0 ] || continue
  git -C "$repo" am --3way "${patches[@]}"
done
