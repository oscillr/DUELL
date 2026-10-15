#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# DX7 compatibility harness, OPTIONAL: DX7_BANKS=<dir of 32-voice DX7 .syx banks> ./tests/dx7_compat.sh   (run from felucca-base/, build/gen from DUELL/gen_only.py)
# No banks are shipped in the repo; without DX7_BANKS this prints 'skipped: no banks' and passes. Output: build/dx7_compat/.
cd "$(dirname "$0")/.." || exit 1
if [ -z "$DX7_BANKS" ] || ! ls "$DX7_BANKS"/*.syx >/dev/null 2>&1; then echo "dx7compat: skipped: no banks (DX7_BANKS not set or no .syx files in it)"; exit 0; fi
OUT=build/dx7_compat; mkdir -p "$OUT" build/host
NODE=node; for n in "$HOME"/emsdk/node/*/bin/node; do [ -x "$n" ] && NODE=$n; done
cc -O2 -w -DDUELL_TEST_MODE=1 -Ibuild/gen -Ifirmware/src -Itests -o build/host/dx7_compat tests/dx7_compat.c -lm || exit 1
cc -O2 -o build/host/dx7_ref tests/dx7_ref.c -lm || exit 1
build/host/dx7_compat $OUT/render "$DX7_BANKS"/*.syx || exit 1
build/host/dx7_ref $OUT/ref "$DX7_BANKS"/*.syx || exit 1
"$NODE" tests/dx7_js_parse.mjs web/editor.html $OUT/js.tsv "$DX7_BANKS"/*.syx || exit 1
python3 -I tests/dx7_analyze.py "$DX7_BANKS" $OUT/render $OUT/ref $OUT/js.tsv $OUT/results.json || exit 1
echo "dx7compat: done, results in $OUT/results.json"
