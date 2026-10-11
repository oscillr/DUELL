#!/bin/sh
# DUELL host tests that run on a Mac without the JieLi toolchain (subset of felucca-base/tests/run_tests.sh).
# Usage: ./host_tests.sh [name ...]   names: duck regress chord mod perform ui select step3 engine morph morphui mode hang atgt patslot multi patch splash sounds
#   uitest project fun10 motion persist backup editor midictl audio emu   (default: all; emu only when ~/emsdk exists)
# emu: web/emu/emu_test.mjs, the browser build (Emscripten from ~/emsdk, its own node: the Homebrew one is broken) incl. a bit-for-bit check against the native build
# A test that does not compile counts as a failure (it used to be skipped silently).
# uitest: tests/ui_test.c (also the helper library the other UI tests include); its Felucca-era groups are parked under -DUI_TEST_STALE (not run)
# motion: tests/motion_test.c, its DRUM-grid / SAVE-held-undo groups are parked under -DMOTION_TEST_STALE (not run)
# sounds: the factory sound audit (tests/sound_audit.c: 10 s hold, level at 1 s / 3 s, sustained median, rise time, tap, hit peak; FAIL silent / clipping / NaN / quiet / slow / falls away / End B); SOUND_TRIM=1 adds the
# suggested LEVEL per sound (copy into felucca-base/tools/duell_sounds.py). Needs build/gen/duell_sounds.h (DUELL/gen_only.py)
cd "$(dirname "$0")/felucca-base" || exit 1
OUT=build/host; mkdir -p "$OUT" build/mod_demo build/perform_demo build/fx_demo build/ui_new build/ui_slot
CC="cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests"
FV=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c)
[ -f "$HOME/emsdk/emsdk_env.sh" ] && EMU=emu || EMU=
want="${*:-regress chord mod perform ui select step3 engine morph morphui mode hang atgt patslot multi patch splash sounds uitest project fun10 motion persist backup editor midictl audio $EMU}"
fail=0
run() { echo "== $1"; shift; "$@" > "$OUT/last.log" 2>&1 && echo "   PASS" || { echo "   FAIL"; tail -15 "$OUT/last.log"; fail=1; }; }
for t in $want; do case $t in
  regress) $CC -DNLIVE=4 -o $OUT/regress tests/regress.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run regress $OUT/regress tests/golden.txt tests/cpu_baseline.txt ;;
  chord)   $CC -o $OUT/chord_test tests/chord_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run chord $OUT/chord_test ;;
  mod)     $CC -o $OUT/mod_test tests/mod_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run mod $OUT/mod_test build/mod_demo ;;
  perform) $CC -o $OUT/perform_test tests/perform_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run perform $OUT/perform_test build/perform_demo build/fx_demo ;;
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
  patch)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/patch_test tests/patch_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run patch $OUT/patch_test ;;
  splash)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/splash_test tests/splash_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run splash $OUT/splash_test ;;
  multi)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/multi_test tests/multi_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run multi $OUT/multi_test ;;
  uitest)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/ui_test tests/ui_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run uitest $OUT/ui_test ;;
  project) $CC -o $OUT/project_test tests/project_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run project $OUT/project_test ;;
  fun10)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/fun10_test tests/fun10_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run fun10 $OUT/fun10_test ;;
  motion)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/motion_test tests/motion_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run motion $OUT/motion_test ;;
  persist) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/persistence_test tests/persistence_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run persist $OUT/persistence_test ;;
  backup)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/backup_test tests/backup_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run backup $OUT/backup_test ;;
  editor)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/editor_test tests/editor_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run editor $OUT/editor_test ;;
  midictl) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/midi_control_test tests/midi_control_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run midictl $OUT/midi_control_test ;;
  audio)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/audio_test tests/audio_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run audio $OUT/audio_test ;;
  emu)     { . "$HOME/emsdk/emsdk_env.sh" >/dev/null 2>&1; sh web/emu/build.sh >"$OUT/emu_build.log" 2>&1 && $CC -ffp-contract=off -o build/emu/native_check web/emu/native_check.c -lm && build/emu/native_check build/emu/native.f32; } || { echo "== emu"; echo "   FAIL (build)"; tail -5 "$OUT/emu_build.log"; fail=1; false; } && run emu "$EMSDK_NODE" web/emu/emu_test.mjs build/emu/felucca.wasm build/emu/native.f32 ;;
esac; done
exit $fail
