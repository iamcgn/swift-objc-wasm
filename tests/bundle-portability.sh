#!/usr/bin/env bash
# swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
# SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
# The assembled artifactbundle must run on another Mac: every Mach-O in it may load dylibs
# only from inside the bundle (@rpath, @loader_path, @executable_path) or from macOS
# (/usr/lib, /System), its rpaths may not point into this machine, and no symlink may leave
# the bundle. Usage: tests/bundle-portability.sh [bundle dir]
set -uo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
B=${1:-$HERE/build/swift-6.3.3-RELEASE_wasm-objc.artifactbundle}
[[ -d "$B" ]] || { echo "no bundle at $B"; exit 2; }
bad=0
while IFS= read -r f; do
  if [[ -L "$f" ]]; then
    target=$(cd "$(dirname "$f")" && readlink -f "$(readlink "$f")" 2>/dev/null || true)
    case "$target" in "$B"/*) ;; *) echo "symlink leaves the bundle: $f -> $(readlink "$f")"; bad=1;; esac
    continue
  fi
  [[ -f "$f" ]] || continue
  file -b "$f" | command grep -q 'Mach-O' || continue
  id=$(otool -D "$f" 2>/dev/null | sed -n 2p)   # a dylib's own install name is not a load
  while read -r dep; do
    [[ "$dep" == "$id" ]] && continue
    case "$dep" in @rpath/*|@loader_path/*|@executable_path/*|/usr/lib/*|/System/*) ;; *) echo "$f loads $dep"; bad=1;; esac
  done < <(otool -L "$f" | command grep $'^\t' | awk '{print $1}')   # fat binaries print a header per slice
  while read -r rp; do
    case "$rp" in @rpath*|@loader_path*|@executable_path*|/usr/lib/*|/System/*) ;; *) echo "$f has rpath $rp"; bad=1;; esac
  done < <(otool -l "$f" | awk '/LC_RPATH/ {getline; getline; print $2}')
done < <(command find "$B/swift-6.3.3-RELEASE_wasm-objc/wasm32-unknown-wasip1/swift.xctoolchain" \( -type f -o -type l \) \( -path '*/usr/bin/*' -o -path '*/usr/lib/*' \) -not -path '*/swift_static/*' -not -path '*/lib/clang/*')
[[ $bad -eq 0 ]] && echo "PASS bundle portability: nothing loads from outside the bundle or macOS" || { echo "FAIL bundle portability"; exit 1; }
