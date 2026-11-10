#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL)
# The repo's golden renders against the WebAssembly build: tests/golden.txt (regress.c: presets, the 128 DUELL sounds, voice modes, sends, the song) and
# tests/multi_render_golden.txt (the factory multis M01..M16 and the test scenes, fader 0 / 63 / 127). The same job code and flags as host_tests.sh, compiled
# with Emscripten (-ffp-contract=off) and run in Node, one fresh instance per render. Prints every difference; exits 1 if there is one.
#   web/emu/golden_compare.sh [FILTER]       (FILTER: only golden names containing it, e.g. "sound/" or "M0")
# Needs build/gen (python3 gen_only.py at the repo root, or ./build.sh) and emcc. Writes build/emu_golden/ (not committed).
set -e
cd "$(dirname "$0")/../.."
[ -f build/gen/felucca_tables.h ] || { echo "golden: build/gen is missing (run gen_only.py or ./build.sh)"; exit 1; }
command -v emcc >/dev/null 2>&1 || { echo "golden: no emcc (Emscripten)"; exit 1; }
mkdir -p build/gen_test build/emu_golden
python3 tools/gen_duell_multis.py build/gen_test/duell_multis.h --test-mode >/dev/null
python3 tools/gen_midi_map.py --csv tests/midimap_legacy.csv build/gen_test/midi_map_defaults.h >/dev/null
python3 tools/gen_midi_map.py --csv tests/midimap_legacy.csv --sym MIDI_MAP_LEGACY build/gen_test/midi_map_legacy.h >/dev/null
python3 tools/gen_midi_map.py --sym MIDI_MAP_SHIPPED build/gen_test/midi_map_shipped.h >/dev/null
python3 tools/gen_version.py build/gen/duell_version.h >/dev/null
for t in golden multi; do
    if [ $t = golden ]; then NP=-DNPART=4; EX=_g_count,_g_name,_g_run,_g_hash_lo,_g_hash_hi,_g_peak   # (regress is built with NPART=4 by host_tests.sh, the multis' test without)
    else NP=; EX=_m_count,_m_scene,_m_name,_m_run,_m_hash_lo,_m_hash_hi; fi
    emcc -O2 -ffp-contract=off -std=gnu99 -w $NP -DHOST_MIX_FOLLOWS_MASTER=1 -DDUELL_TEST_MODE=1 '-DFELUCCA_VERSION="DUELL 1.0"' \
        -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests \
        --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_FUNCTIONS=$EX \
        -o build/emu_golden/$t.wasm web/emu/${t}_wasm.c
done
node web/emu/golden_compare.mjs build/emu_golden tests/golden.txt tests/multi_render_golden.txt "$@"
