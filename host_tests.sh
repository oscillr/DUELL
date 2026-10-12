#!/bin/sh
# DUELL host tests that run on a Mac without the JieLi toolchain (subset of felucca-base/tests/run_tests.sh).
# Usage: ./host_tests.sh [name ...]   names: duck regress chord mod ui select step3 engine morph morphui mode hang atgt patslot multi release patch splash sounds loadcost uf
#   uitest project fun10 motion persist backup editor midictl audio erase emu   (default: all; emu only when ~/emsdk exists)
# emu: web/emu/emu_test.mjs, the browser build (Emscripten from ~/emsdk, its own node: the Homebrew one is broken) incl. a bit-for-bit check against the native build
# A test that does not compile counts as a failure (it used to be skipped silently).
# DUELL_TEST_MODE: the release default is OFF (no MENU > SYSTEM > TEST, no CPU test multis M13..M16). Every firmware test below is built WITH it on (-DDUELL_TEST_MODE=1 and
# build/gen_test/duell_multis.h, which holds M13..M16) so it can exercise the TEST screen and the test multis; "release" is built with it off and checks they are gone.
# The default build/gen/duell_multis.h is regenerated here with the switch off (the emulator step builds the release).
# uitest: tests/ui_test.c (also the helper library the other UI tests include); its Felucca-era groups are parked under -DUI_TEST_STALE (not run)
# motion: tests/motion_test.c, its DRUM-grid / SAVE-held-undo groups are parked under -DMOTION_TEST_STALE (not run)
# uf: tests/uf_test.c, DUELL 0.2.2's universal per-voice filter (CUT / RES on the engines with no filter of their own): centre bit for bit, spectra, routes, CC74 / CC71, stability, roll, old records, cost
# loadcost: tests/loadcost_test.c, the cost of a CHANGE (a patch / multi load, a reverb or mode switch) in the audio half buffer, counted with the device's word-wise memset (build: -O1 -fno-builtin); LOADCOST_FULL=1 prints the whole table
# sounds: the factory sound audit (tests/sound_audit.c: 10 s hold, level at 1 s / 3 s, sustained median, rise time, tap, hit peak; FAIL silent / clipping / NaN / quiet / slow / falls away / End B); SOUND_TRIM=1 adds the
# suggested LEVEL per sound (copy into felucca-base/tools/duell_sounds.py). Needs build/gen/duell_sounds.h (DUELL/gen_only.py)
cd "$(dirname "$0")/felucca-base" || exit 1
OUT=build/host; mkdir -p "$OUT" build/mod_demo build/ui_new build/ui_slot
unset DUELL_TEST_MODE                                  # (the environment switch is for device / emulator builds; the tests set theirs below)
CCR="cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests"      # the release configuration (switch off)
CC="cc -O2 -w -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests"   # test mode ON
mkdir -p build/gen_test
python3 tools/gen_duell_multis.py build/gen/duell_multis.h >/dev/null || exit 1                 # release header: M01..M04
python3 tools/gen_duell_multis.py build/gen_test/duell_multis.h --test-mode >/dev/null || exit 1   # test header: + M13..M16
python3 tools/gen_version.py build/gen/duell_version.h || exit 1   # VERSION file -> build/gen/duell_version.h
FV=$(sed -n 's/^#define DUELL_VERSION_STR "\(.*\)"/\1/p' build/gen/duell_version.h)
[ -f "$HOME/emsdk/emsdk_env.sh" ] && EMU=emu || EMU=
want="${*:-regress chord mod ui select step3 engine morph morphui mode hang atgt patslot multi release patch splash sounds loadcost uf uitest project fun10 motion persist backup editor midictl audio erase $EMU}"
fail=0
run() { echo "== $1"; shift; "$@" > "$OUT/last.log" 2>&1 && echo "   PASS" || { echo "   FAIL"; tail -15 "$OUT/last.log"; fail=1; }; }
for t in $want; do case $t in
  regress) $CC -DNLIVE=4 -o $OUT/regress tests/regress.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run regress $OUT/regress tests/golden.txt tests/cpu_baseline.txt ;;
  chord)   $CC -o $OUT/chord_test tests/chord_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run chord $OUT/chord_test ;;
  mod)     $CC -o $OUT/mod_test tests/mod_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run mod $OUT/mod_test build/mod_demo ;;
  ui)      $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/ui_render tests/ui_render.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run ui $OUT/ui_render build/ui_new build/ui_slot ;;
  select)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/select_test tests/select_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run select $OUT/select_test ;;
  step3)   $CC -o $OUT/step3_test tests/step3_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run step3 $OUT/step3_test ;;
  engine)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/engine_page_test tests/engine_page_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run engine $OUT/engine_page_test ;;
  morph)   $CC -o $OUT/morph_test tests/morph_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run morph $OUT/morph_test ;;
  morphui) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/morph_ui_test tests/morph_ui_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run morphui $OUT/morph_ui_test ;;
  duck)    $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/duck_test tests/duck_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run duck $OUT/duck_test ;;
  mode)    $CC "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/mode_test tests/mode_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run mode $OUT/mode_test ;;
  hang)    $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/hang_test tests/hang_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run hang $OUT/hang_test ;;
  atgt)    $CC -o $OUT/atgt_test tests/atgt_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run atgt $OUT/atgt_test ;;
  patslot) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/patslot_test tests/patslot_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run patslot $OUT/patslot_test ;;
  sounds)  $CC -o $OUT/sound_audit tests/sound_audit.c -lm || { echo "== sounds"; echo "   FAIL (does not compile)"; fail=1; false; } && { echo "== sounds"; $OUT/sound_audit || fail=1; } ;;
  release) $CCR -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/release_test tests/release_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run release $OUT/release_test ;;
  patch)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/patch_test tests/patch_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run patch $OUT/patch_test ;;
  splash)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/splash_test tests/splash_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run splash $OUT/splash_test ;;
  loadcost) $CC -O1 -fno-builtin -D_FORTIFY_SOURCE=0 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/loadcost_test tests/loadcost_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run loadcost $OUT/loadcost_test ;;
  uf)      $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/uf_test tests/uf_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run uf $OUT/uf_test ;;
  multi)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/multi_test tests/multi_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run multi $OUT/multi_test ;;
  uitest)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/ui_test tests/ui_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run uitest $OUT/ui_test ;;
  project) $CC -o $OUT/project_test tests/project_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run project $OUT/project_test ;;
  fun10)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/fun10_test tests/fun10_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run fun10 $OUT/fun10_test ;;
  motion)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/motion_test tests/motion_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run motion $OUT/motion_test ;;
  persist) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/persistence_test tests/persistence_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run persist $OUT/persistence_test ;;
  erase)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/erase_test tests/erase_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run erase $OUT/erase_test ;;
  backup)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/backup_test tests/backup_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run backup $OUT/backup_test ;;
  editor)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/editor_test tests/editor_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run editor $OUT/editor_test ;;
  midictl) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/midi_control_test tests/midi_control_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run midictl $OUT/midi_control_test ;;
  audio)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/audio_test tests/audio_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run audio $OUT/audio_test ;;
  emu)     { . "$HOME/emsdk/emsdk_env.sh" >/dev/null 2>&1; sh web/emu/build.sh >"$OUT/emu_build.log" 2>&1 && $CCR -ffp-contract=off -o build/emu/native_check web/emu/native_check.c -lm && build/emu/native_check build/emu/native.f32; } || { echo "== emu"; echo "   FAIL (build)"; tail -5 "$OUT/emu_build.log"; fail=1; false; } && run emu "$EMSDK_NODE" web/emu/emu_test.mjs build/emu/felucca.wasm build/emu/native.f32 ;;
esac; done
exit $fail
