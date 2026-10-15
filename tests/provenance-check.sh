#!/usr/bin/env bash
# swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
# SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
#
# Provenance check: the runtime's headers and sources must not carry verbatim text from
# Apple's objc4 (Apple Public Source License), beyond the API declarations and metadata
# field names that compatibility dictates. objc4 is cloned shallowly into a scratch
# directory for the comparison only; it is never vendored. Reports every comment line of
# ours that appears verbatim in objc4 (or the macOS SDK's objc headers, when present) and
# fails if there are any; also prints, for information, the count of non-comment lines in
# common per file.
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK=${WORK:-$(mktemp -d)}; mkdir -p "$WORK"
OBJC4="$WORK/objc4"
[[ -d "$OBJC4/runtime" ]] || git clone -q --depth 1 https://github.com/apple-oss-distributions/objc4 "$OBJC4"
SDK_OBJC=$(ls -d /Library/Developer/CommandLineTools/SDKs/MacOSX*.sdk/usr/include/objc 2>/dev/null | head -1)
python3 -I - "$HERE/../runtime" "$OBJC4/runtime" "${SDK_OBJC:-/nonexistent}" <<'PY'
import sys, os, re, glob
ours, objc4, sdk = sys.argv[1:4]
def norm(l): return re.sub(r'\s+', ' ', l.strip().lstrip('/*! ').rstrip('*/ '))
def is_comment(s): return s.startswith(('//', '/*', '*'))
def wordy(n): return len(re.findall(r'[A-Za-z]{2,}', n)) >= 3  # not a separator line
def interesting(l): return len(l) >= 30 and not l.startswith(('#include', '#import', '#if', '#endif', '#else'))
theirs_c, theirs_code = {}, {}
for root in (objc4, sdk):
    for p in glob.glob(root + '/**/*', recursive=True):
        if not p.endswith(('.h', '.m', '.mm', '.c', '.cpp')): continue
        for l in open(p, errors='replace'):
            s = l.strip(); n = norm(l)
            if is_comment(s) and len(n) >= 25 and wordy(n): theirs_c.setdefault(n, os.path.basename(p))
            elif interesting(n): theirs_code.setdefault(n, os.path.basename(p))
bad = 0
for p in sorted(glob.glob(ours + '/**/*', recursive=True)):
    if not p.endswith(('.h', '.m', '.c', '.swift')) or '/blocks/' in p or '/include/Block' in p or '/build/' in p: continue
    code_hits, lines = 0, 0
    for i, l in enumerate(open(p, errors='replace'), 1):
        s = l.strip(); n = norm(l)
        if is_comment(s):
            if len(n) >= 25 and wordy(n) and n in theirs_c:
                print(f"VERBATIM objc4 comment {os.path.relpath(p, ours)}:{i}: {s}  (objc4 {theirs_c[n]})"); bad += 1
        elif interesting(n):
            lines += 1
            if n in theirs_code: code_hits += 1
    if code_hits: print(f"info: {os.path.relpath(p, ours)}: {code_hits}/{lines} non-comment lines also appear in objc4 (declarations, field names)")
print("FAIL" if bad else "PASS", f"provenance: {bad} verbatim objc4 comment line(s)")
sys.exit(1 if bad else 0)
PY
