#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Patch refinement pass (release_drafts/PATCH_REFINE_RECIPE.md): regenerate the sound table from tools/duell_sounds.py, build the sound audit
# (tests/sound_audit.c) and print the rows of the named sounds (peak, loudness, End B).
#   tools/refine_audit.sh "NAME" [trim|bseed|all]
#     (no mode)  the audit row of that sound (SOUND_ONLY): PEAK LOUD MED TAP, END B dB / brightness (STEREO / SPLIT), VERDICT
#     trim       also SOUND_TRIM=1: the TRIM the sound would need for -18 dBFS (copy it into "TRIM": n; prefer the 1/7 number within 1 dB)
#     bseed      SOUND_BSEED=1: the End B seed search; copy a passing seed into "bseed": n
#     all        the whole bank (128 rows, exit 1 on a FAIL), for the final check
# Run from anywhere; needs build/gen (copy it once from a built worktree, the recipe step 0).
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host build/gen_test
python3 tools/refine_dupkeys.py >/dev/null || { python3 tools/refine_dupkeys.py; exit 1; }
python3 tools/gen_duell_sounds.py build/gen/duell_sounds.h >/dev/null
python3 tools/gen_duell_multis.py build/gen/duell_multis.h >/dev/null
python3 tools/gen_duell_multis.py build/gen_test/duell_multis.h --test-mode >/dev/null
cc -O2 -w -DHOST_MIX_FOLLOWS_MASTER=1 -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests -o build/host/sound_audit tests/sound_audit.c -lm
name=${1:-all}; mode=${2:-}
if [ "$name" = all ] || [ "$mode" = all ]; then build/host/sound_audit; exit $?; fi
case "$mode" in
  trim)  SOUND_ONLY="$name" SOUND_TRIM=1 build/host/sound_audit ;;
  bseed) SOUND_ONLY="$name" SOUND_BSEED=1 build/host/sound_audit ;;
  *)     SOUND_ONLY="$name" build/host/sound_audit ;;
esac
