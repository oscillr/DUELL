#!/bin/sh
# DUELL host tests that run on a Mac without the JieLi toolchain (subset of felucca-base/tests/run_tests.sh).
# Usage: ./host_tests.sh [name ...]   names: level bassmono shimmer locut duck dlymod regress chord mod perform ui select step3 engine morph morphui mode dlymod dlyev hang atgt patslot multi release patch patchraw dx7import dx7compat splash sounds mrender fmbass fm6 loadcost cpuscen uf wave hold guide wtab wavetable
#   modules uitest project fun10 motion persist backup editor editorjson midictl audio erase flashled usb insert rollbrowser emu   (default: all; emu only when ~/emsdk exists)
# dlyev: tests/dlyev_test.c, the delay MODE (0.2.16 G_DMODE: DIGITAL / PING-PONG / RANDOM / OCT UP; fx_bus.c dly_ev): DIGITAL and PING-PONG bit for bit (hashes of 02f849e, a stored 7 and DUEL too), determinism, the clock (first edge one delay time after
#   PLAY), no click switching modes (also with PING-PONG on), silence after the tails, loud input (FDBK 120), SENDS OFF / STOP ALL clear the state, the pages (MODE first, MIX last) and every mode's faded cards with their reasons (CHANCE, SPREAD / RATE, MODE and SPREAD in DUEL), the header's full names, DIGITAL <-> PING-PONG switching, cost
# project / step3 / insert / persist / multi / backup (0.2.14): the old stored formats (FUN1..FUN10, FUN11 revision 0, multi versions 1..2, patches without PTF_CB, PER1..PER3) are REFUSED, not converted
# reverb: tests/reverb_test.c (ROOM / SPRING bit for bit, SPRING and HALL decay, stability, silence, stereo, model changes, cost; 0.2.14: its two stale references re-baselined)
# plate: tests/plate_test.c, 0.2.14's sixth reverb PLATE (determinism, loud input, decay, corners, exact silence, no DC, stereo, SPLIT buses A / B, model changes, cost)
# locut: tests/locut_test.c, 0.2.14's REVERB LOW CUT (LOCUT, REVERB K4; fx_bus.c rev_locut): 0 = the reverb as before bit for bit (all six models, buses A / B), the control, the -3 dB corner and slope,
#   every model's bass against LOCUT 0, no click, silence, buses / SPACE / SENDS OFF, cost
# insert: tests/insert_test.c (1.5's INSERT, + 0.2.14's CRUSH BITS knob end to end, TAPE and VINYL: test_lofi); in the default list again (wip/prune2: its FUN11 byte-count check, stale since P_COUNT grew to 127, is rewritten for P_COUNT 127 / 1618 spare bytes; the CRUSH migration checks became refusal checks)
# elfcheck: tests/elfcheck_test.py, tools/elfcheck.py (the start-up layout and RAM-code check build.py runs after every link) on tiny fake ELFs: the release's
#   .noinit table passes, bench B's LTO split (bootguard.0), a shrunk magic word, a wrong .noinit size / entry / image size fail (ELFCHECK_ELF=<elf>: a real one too)
# shimmer: tests/shimmer_test.c (0.2.19), SHIM's PITCH (UP / DOWN / BOTH) and MOD (G_SHPITCH, G_SHMOD; fx_bus.c rev_shimx; REVERB page SHIMMER): UP + MOD 0 bit for bit the SHIM of 0.2.18 (the bus alone, A and B, and the whole mix in
#   STEREO / LAYER / DUEL), the pitch of a sine through the return (octave down, up, both), MOD's drift in cents and its bounds, no click on a change, silence after the tails, bus A = B and DUEL isolation, STOP ALL / SENDS OFF
#   clear the state, the SHIMMER page (in the page cycle only for TYPE SHIM), the cards, cost; an argument DEMODIR writes the listening set (WAV + index.html)
# panic: tests/panic_test.c (0.2.14), STOP ALL flushes the delay, chorus and reverb lines (fx.c snd_panic, the SENDS OFF machinery as a one-shot): M02, SPACE +100, DUEL / STEREO / LAYER x
#   ROOM SPRING HALL SHIM RESO: quiet after it, no click, the sends back (4 s window vs the lines cleared at once), a stopped unit's double press, single STOP leaves tails, user SENDS OFF stays OFF
# editorjson: tests/editordump.c writes the web editor's layout from the firmware tables (PAGES[], every descriptor with its values formatted by param_format, the engines);
#   the committed web/editor/layout.json must equal a fresh run, web/test_layout.mjs compares it with descdump.c's output (a second exporter), then web/test_editor.mjs (the editor against the
#   simulated unit) and web/test_duellbackup.mjs run with the emsdk node. After a firmware table change: cp felucca-base/build/host/layout.json felucca-base/web/editor/layout.json
# bassmono: tests/bassmono_test.c, 0.2.19's BASS MONO (fx.c bass_mono; the page FX > MASTER, core.h G_BM / song.bm; stored in multi byte 5 and project byte 65, no MENU row): OFF bit for bit (the sources-before hash; ON then OFF returns to it), L + R untouched,
#   a 40 Hz out-of-phase tone made mono while 2 kHz passes, the -3 dB corner at all 11 settings, no click (settings walked, ON / OFF), exact silence, DUEL bypassed (and cleared at a mode change), the row (hidden in DUEL, SELECT passes it), the byte, cost
# roll (0.2.19): web/test_roll.mjs, the piano roll's model (step / tie rules, edit functions, undo, live writes against the simulated unit) runs with editorjson; rollbrowser: web/test_rollbrowser.mjs, the roll in headless Chrome
#   (pointer and keyboard editing, the overlay, no clipped text at 1920x1080 and 1280x720); only when Chrome is installed
# flashled: tests/flash_led_test.c (0.2.14, from Felucca 1.6 #236): the LED rows dark during a flash erase, in order (duck, LEDs off, erase, LEDs back, unduck)
# usb: tests/usb_desc_test.c over the descriptor variants of tests/run_tests.sh (0.2.14, from Felucca 1.5.1 #67: the AC collection lists audio streaming first)
# emu: web/emu/emu_test.mjs, the browser build (Emscripten from ~/emsdk, its own node: the Homebrew one is broken) incl. a bit-for-bit check against the native build
# A test that does not compile counts as a failure (it used to be skipped silently).
# DUELL_TEST_MODE: the release default is OFF (no MENU > SYSTEM > TEST, no CPU test multis M13..M16). Every firmware test below is built WITH it on (-DDUELL_TEST_MODE=1 and
# build/gen_test/duell_multis.h, which holds M13..M16) so it can exercise the TEST screen and the test multis; "release" is built with it off and checks they are gone.
# The default build/gen/duell_multis.h is regenerated here with the switch off (the emulator step builds the release).
# uitest: tests/ui_test.c (also the helper library the other UI tests include); its Felucca-era groups are parked under -DUI_TEST_STALE (not run)
# motion: tests/motion_test.c, its DRUM-grid / SAVE-held-undo groups are parked under -DMOTION_TEST_STALE (not run)
# modules: tests/modules_test.c, 0.2.14's GLO > MODULES (ARP SEQ SENDS PART FX): the settings bits (fresh, saved, ERASE, an older record), the page, ARP OFF's keys,
#   SEQ OFF's PLAY / Start / count-in, SENDS OFF and PART FX OFF bit for bit after their fades, PERFORM with all OFF, the faded cards and the footer's line
# wtab: tests/wtab_test.c, 0.2.14's USER WAVETABLES USR1..USR3 (wtab.c, editor_wtab.c, backup ids 24..26) on simulated NOR: the flash places (0xA0000..0xA5FFF, nothing else written), the commit record and
#   CRC, power-on, a damaged copy, a failing write, refusal while the sequencer plays, stale / malformed transfers, erase, ERASE DATA, the backup round trip
# wavetable: tests/wavetable_fixture.c (the factory tables as played and as encoded bytes: web/fixtures/wavetable_factory.json must equal a fresh run), then web/test_wavetable.mjs (the WAV converter
#   web/editor/wavetable.js: WAV parsing, FFT, saw / square / sine sweeps, a Serum-like 2048 x 256 file, a factory render converted back, the protocol client against the mock unit)
# mrender: tests/multi_render_test.c (0.2.17), every factory multi (M01..M10, M13..M16) rendered at fader 0 / 63 / 127 and hashed: bit for bit what 0.2.16 (114 sounds) rendered (tests/multi_render_golden.txt)
# fm6: tests/fm6_test.c (the 6-operator engine; 0.2.17: the SLOT list with F9..F11 behind the bank)
# patch: tests/patch_test.c (0.2.17: ONE list of 128 patch slots, a slot = its saved record or else the factory sound at that browse position): the store (sparse records, 16 chunks, a power cut in a write, the 128-slot limit with no 'full', RAM directory), a fresh unit shows the factory sounds in all 128 slots, SAVE then reload, RESTORE bit for bit, RESTORE ALL, MIDI program change = slot, FAV by slot, a power cut at every step of a SAVE / RESTORE / RESTORE ALL, the PATCHES page and its dialogs
# patchraw: tests/patchraw_test.c (0.2.19), PATCH_RAW (cmd 84, editor_patchraw.c): a slot's stored record read and written whole: the worst record round trip bit for bit into another slot (decodes and loads the same), only the chunk's sectors written, a rename through the bytes,
#   refusal while playing (nothing written), malformed / stale transfers, records the store would not read, a flash failure, a power cut at every step of a write (old or new whole, the neighbours untouched), all 128 slots filled with the worst record
# dx7import: tests/dx7_import_test.c (0.2.19) + web/test_librarian.mjs: the editor converts the 63 voices of two SYNTHETIC DX7 banks (web/test_dx7_synth.mjs, generated in the test; DX7_BANKS=dir adds real ones) into FM6 patch records; the firmware stores them (up_put_raw), loads them (up_load) and the part renders bit for bit what the FM6 engine
#   renders when it loads the same voice with the same parameters. web/test_librarian.mjs (the librarian, the patch / wavetable files, the DX7 conversion) also runs in editorjson, and test_librarian_ui.mjs (headless Chrome) is run by hand: see docs/TESTING.md
# dx7compat: tests/dx7_compat.sh (optional, local: DX7_BANKS=<dir of 32-voice .syx banks> runs the engine / reference / parser comparison; without it 'skipped: no banks', PASS)
# fmbass: tests/fm_bass_diff_test.c (0.2.17), FRETLESS and SLAP DOCTOR (both FM6 basses) must not sound alike: the energy above 250 Hz at 200 ms differs >= 6 dB, the transient peak over the body >= 4 dB
# guide: tests/guide_test.c, 0.2.11's first-start GUIDE: the settings bit (fresh, seen, an older record, MENU's replay), its keys, playing under it, no stuck state
# level: tests/level_test.c (0.2.19, the level split: PATCH TRIM P_TRIM, stored with a patch, and MIXER LEVEL P_LEVEL, the player's, kept with a multi and the project): a sound load (factory, stored, engine change, INIT, UNDO) keeps the mixer level, SAVE stores the
#   trim only, RESTORE brings the factory trim, a record of before the split reads its LEVEL as the trim, a multi / the project hold both (revisions), the factory multis' mixer is unity, CC7 is the mixer, the audio gain is trim x mixer
#   (bit for bit across splits of one product, silence at 0, the +7.5 dB ceiling), VOICE 2/2 K4 TRIM / MIXER K4 LEVEL
# reset: tests/reset_test.c, 0.2.9's OCT- held + a knob = that card's default (once, no creeping) and OCT-/OCT+ moving the octave on release
# perform: tests/perform_test.c, DUELL 0.2.5's PERFORM mode (FX held): entry and exit, its keys, knobs, PRESETS / ALGORITHM, ROLL, MUTE, LEDs, idle bit for bit, the effects
# wave: tests/wave_test.c, DUELL's WAVE engine (eng_wave.c): the table build (deterministic, shape, level), band-limiting (FFT: off-harmonic energy), fades and
#   silence while a table is built, POS ramps, TABLE changed every block, an engine switch mid-build, two parts, MORPH tags, cost (WAVE_VERBOSE=1: the whole table);
#   0.2.7's tables 14..25: the list, the 14 older tables' fingerprint, a fade into each new one, their aliasing, every table's build cost
# uf: tests/uf_test.c, the per-voice multimode filter (0.2.9; 0.2.2's universal filter: CUT RES MODE SLOPE on the engines with no filter of their own): the bypass bit for bit,
#   the response table (UF_TABLE=1: RES 80 % too), 0.2.8's low-pass bit for bit, click-free sweeps, routes, CC74 / CC71, stability, records of today kept (0.2.14: no conversion of old ones), roll, cost
# cpuscen: tests/cpu_scenarios_test.c (0.2.11), the STEADY-STATE load of held notes in the loadcost scale: the factory multis with the MORPH fader at 0 / 63 / 127 (guards on 63);
#   C the VOICE CAP (voice.c vcap: the caps of heavy pairs, measured, never below 4, new notes only); CPU_SCEN=QCPEMXWKF for more (P the cap's per-voice
#   table, engine pairs x rolls, what End B moves, the stages a move wakes, WAVE, fades, bursts); build -O1 -fno-builtin
# loadcost: tests/loadcost_test.c, the cost of a CHANGE (a patch / multi load, a reverb or mode switch) in the audio half buffer, counted with the device's word-wise memset (build: -O1 -fno-builtin); LOADCOST_FULL=1 prints the whole table
# sounds: the factory sound audit (tests/sound_audit.c: 10 s hold, level at 1 s / 3 s, sustained median, rise time, tap, hit peak; FAIL silent / clipping / NaN / quiet / slow / falls away / End B); SOUND_TRIM=1 adds the
# suggested LEVEL per sound (copy into felucca-base/tools/duell_sounds.py). Needs build/gen/duell_sounds.h (DUELL/gen_only.py)
cd "$(dirname "$0")/felucca-base" || exit 1
OUT=build/host; mkdir -p "$OUT" build/mod_demo build/ui_new build/ui_slot
unset DUELL_TEST_MODE                                  # (the environment switch is for device / emulator builds; the tests set theirs below)
CCR="cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests"      # the release configuration (switch off)
CC="cc -O2 -w -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests"   # test mode ON
mkdir -p build/gen_test
python3 tools/gen_duell_multis.py build/gen/duell_multis.h >/dev/null || exit 1                 # release header: M01..M10
python3 tools/gen_duell_multis.py build/gen_test/duell_multis.h --test-mode >/dev/null || exit 1   # test header: + M13..M16
python3 tools/gen_version.py build/gen/duell_version.h || exit 1   # VERSION file -> build/gen/duell_version.h
FV=$(sed -n 's/^#define DUELL_VERSION_STR "\(.*\)"/\1/p' build/gen/duell_version.h)
[ -f "$HOME/emsdk/emsdk_env.sh" ] && EMU=emu || EMU=
want="${*:-regress chord mod perform ui select step3 engine morph morphui mode dlymod dlyev hang atgt patslot multi release patch patchraw dx7import dx7compat splash sounds mrender fmbass fm6 loadcost cpuscen uf wave hold reset level guide wtab wavetable modules uitest project fun10 motion persist backup editor editorjson midictl audio erase flashled usb elfcheck reverb plate locut shimmer panic insert bassmono rollbrowser $EMU}"
fail=0
run() { echo "== $1"; shift; "$@" > "$OUT/last.log" 2>&1 && echo "   PASS" || { echo "   FAIL"; tail -15 "$OUT/last.log"; fail=1; }; }
for t in $want; do case $t in
  regress) $CC -DNLIVE=4 -o $OUT/regress tests/regress.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run regress $OUT/regress tests/golden.txt tests/cpu_baseline.txt ;;
  chord)   $CC -o $OUT/chord_test tests/chord_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run chord $OUT/chord_test ;;
  mod)     $CC -o $OUT/mod_test tests/mod_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run mod $OUT/mod_test build/mod_demo ;;
  perform) $CC "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/perform_test tests/perform_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run perform $OUT/perform_test ;;
  ui)      $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/ui_render tests/ui_render.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run ui $OUT/ui_render build/ui_new build/ui_slot ;;
  select)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/select_test tests/select_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run select $OUT/select_test ;;
  step3)   $CC -o $OUT/step3_test tests/step3_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run step3 $OUT/step3_test ;;
  engine)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/engine_page_test tests/engine_page_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run engine $OUT/engine_page_test ;;
  morph)   $CC -o $OUT/morph_test tests/morph_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run morph $OUT/morph_test ;;
  morphui) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/morph_ui_test tests/morph_ui_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run morphui $OUT/morph_ui_test ;;
  duck)    $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/duck_test tests/duck_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run duck $OUT/duck_test ;;
  dlymod)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/dlymod_test tests/dlymod_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run dlymod $OUT/dlymod_test ;;
  dlyev)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/dlyev_test tests/dlyev_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run dlyev $OUT/dlyev_test ;;
  mode)    $CC "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/mode_test tests/mode_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run mode $OUT/mode_test ;;
  hang)    $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/hang_test tests/hang_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run hang $OUT/hang_test ;;
  atgt)    $CC -o $OUT/atgt_test tests/atgt_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run atgt $OUT/atgt_test ;;
  patslot) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/patslot_test tests/patslot_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run patslot $OUT/patslot_test ;;
  sounds)  $CC -o $OUT/sound_audit tests/sound_audit.c -lm || { echo "== sounds"; echo "   FAIL (does not compile)"; fail=1; false; } && { echo "== sounds"; $OUT/sound_audit || fail=1; } ;;
  release) $CCR -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/release_test tests/release_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run release $OUT/release_test ;;
  patchraw) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/patchraw_test tests/patchraw_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run patchraw $OUT/patchraw_test ;;
  dx7import) NODE=node; for n in "$HOME"/emsdk/node/*/bin/node; do [ -x "$n" ] && NODE=$n; done
           $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/dx7_import_test tests/dx7_import_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } &&
           run dx7import sh -c "DX7_FIXTURE=$OUT/dx7_fixture.txt $NODE web/test_librarian.mjs > $OUT/librarian_js.log 2>&1 || { tail -5 $OUT/librarian_js.log; exit 1; }; $OUT/dx7_import_test $OUT/dx7_fixture.txt" ;;
  dx7compat) run dx7compat sh tests/dx7_compat.sh ;;   # optional: DX7_BANKS=<dir of .syx banks>, else 'skipped: no banks' and PASS
  patch)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/patch_test tests/patch_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run patch $OUT/patch_test ;;
  splash)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/splash_test tests/splash_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run splash $OUT/splash_test ;;
  loadcost) $CC -O1 -fno-builtin -D_FORTIFY_SOURCE=0 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/loadcost_test tests/loadcost_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run loadcost $OUT/loadcost_test ;;
  cpuscen) $CC -O1 -fno-builtin -D_FORTIFY_SOURCE=0 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/cpu_scenarios_test tests/cpu_scenarios_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run cpuscen $OUT/cpu_scenarios_test ;;
  fmbass)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/fm_bass_diff_test tests/fm_bass_diff_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run fmbass $OUT/fm_bass_diff_test ;;
  fm6)     $CC -O2 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/fm6_test tests/fm6_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && { mkdir -p build/fm6_demo; run fm6 $OUT/fm6_test build/fm6_demo; } ;;
  mrender) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/multi_render_test tests/multi_render_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run mrender $OUT/multi_render_test tests/multi_render_golden.txt ;;
  uf)      $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/uf_test tests/uf_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run uf $OUT/uf_test ;;
  level)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/level_test tests/level_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run level $OUT/level_test ;;
  reset)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/reset_test tests/reset_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run reset $OUT/reset_test ;;
  modules) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/modules_test tests/modules_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run modules $OUT/modules_test ;;
  guide)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/guide_test tests/guide_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run guide $OUT/guide_test ;;
  reverb)  mkdir -p build/fx_demo build/reverb_demo; $CC -O2 -o $OUT/reverb_test tests/reverb_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run reverb $OUT/reverb_test build/fx_demo build/reverb_demo ;;
  bassmono) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/bassmono_test tests/bassmono_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run bassmono $OUT/bassmono_test ;;
  shimmer) $CC -O2 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/shimmer_test tests/shimmer_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run shimmer $OUT/shimmer_test ;;
  locut)   $CC -O2 -o $OUT/locut_test tests/locut_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run locut $OUT/locut_test ;;
  plate)   mkdir -p build/plate_demo; $CC -O2 -o $OUT/plate_test tests/plate_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run plate $OUT/plate_test build/plate_demo ;;
  insert)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/insert_test tests/insert_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run insert $OUT/insert_test ;;
  panic)   $CC -o $OUT/panic_test tests/panic_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run panic $OUT/panic_test ;;
  hold)    $CC -o $OUT/hold_test tests/hold_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run hold $OUT/hold_test ;;
  wave)    $CC -o $OUT/wave_test tests/wave_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run wave $OUT/wave_test ;;
  multi)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/multi_test tests/multi_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run multi $OUT/multi_test ;;
  uitest)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/ui_test tests/ui_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run uitest $OUT/ui_test ;;
  project) $CC -o $OUT/project_test tests/project_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run project $OUT/project_test ;;
  fun10)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/fun10_test tests/fun10_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run fun10 $OUT/fun10_test ;;
  motion)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/motion_test tests/motion_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run motion $OUT/motion_test ;;
  persist) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/persistence_test tests/persistence_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run persist $OUT/persistence_test ;;
  erase)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/erase_test tests/erase_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run erase $OUT/erase_test ;;
  wtab)    $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/wtab_test tests/wtab_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run wtab $OUT/wtab_test ;;
  wavetable) $CC -o $OUT/wavetable_fixture tests/wavetable_fixture.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run wavetable-fixture sh -c "$OUT/wavetable_fixture $OUT/wavetable_factory.json && cmp $OUT/wavetable_factory.json web/fixtures/wavetable_factory.json"
             NODE=node; for n in "$HOME"/emsdk/node/*/bin/node; do [ -x "$n" ] && NODE=$n; done
             run wavetable-js "$NODE" web/test_wavetable.mjs ;;
  backup)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/backup_test tests/backup_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run backup $OUT/backup_test ;;
  editor)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/editor_test tests/editor_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run editor $OUT/editor_test ;;
  editorjson) NODE=node; for n in "$HOME"/emsdk/node/*/bin/node; do [ -x "$n" ] && NODE=$n; done
           $CCR -o $OUT/editordump tests/editordump.c -lm && $CCR -o $OUT/descdump tests/descdump.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } &&
           run editorjson sh -c "$OUT/editordump > $OUT/layout.json && { cmp -s $OUT/layout.json web/editor/layout.json || { echo 'web/editor/layout.json is stale: cp build/host/layout.json web/editor/layout.json'; exit 1; }; } &&
             $OUT/descdump > $OUT/desc.json && FELUCCA_DESC=$OUT/desc.json $NODE web/test_layout.mjs && $NODE web/test_editor.mjs && $NODE web/test_duellbackup.mjs && $NODE web/test_librarian.mjs && $NODE web/test_roll.mjs" ;;
  rollbrowser) NODE=node; for n in "$HOME"/emsdk/node/*/bin/node; do [ -x "$n" ] && NODE=$n; done
           run rollbrowser "$NODE" web/test_rollbrowser.mjs ;;   # (skips itself when no Chrome is installed)
  midictl) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/midi_control_test tests/midi_control_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run midictl $OUT/midi_control_test ;;
  audio)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/audio_test tests/audio_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run audio $OUT/audio_test ;;
  elfcheck) run elfcheck python3 -I tests/elfcheck_test.py ;;
  flashled) $CC -o $OUT/flash_led_test tests/flash_led_test.c || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run flashled $OUT/flash_led_test ;;
  usb)     echo "== usb"; ub=0
           for v in 1.1.0.1.1.1 1.1.1.1.1.1 1.1.2.1.1.1 1.1.3.1.1.1 1.1.0.0.1.1 1.1.2.0.1.1 0.1.0.1.1.1 1.1.0.1.0.1 1.1.0.0.0.1 1.1.2.1.0.1 0.1.0.1.0.1 \
                    1.0.0.1.1.1 1.0.1.1.1.1 1.0.0.0.1.1 0.0.0.1.1.1 1.1.0.1.1.0 1.1.0.0.1.0 1.1.0.1.0.0 1.1.0.0.0.0 0.1.0.1.1.0; do
               set -- $(echo "$v" | tr . ' ')     # CDC UAC LAYOUT CDC-presented 48K AS-first
               cc -O2 -w -DT_CDC=$1 -DT_UAC=$2 -DT_LAYOUT=$3 -DT_ON=$4 -DT_48K=$5 -DT_ASF=$6 -o $OUT/usb_desc_test tests/usb_desc_test.c \
                   && $OUT/usb_desc_test > "$OUT/last.log" 2>&1 || { echo "   FAIL ($v)"; tail -15 "$OUT/last.log"; ub=1; }
           done
           for c in 1 0; do                       # the USB audio input's descriptors, ring and packets, with and without CDC
               $CC -DT_CDC=$c -DHALF_FRAMES=$(sed -n 's/^#define HALF_FRAMES \([0-9]*\).*/\1/p' firmware/src/core.h) -o $OUT/uac_test tests/uac_test.c -lm \
                   && $OUT/uac_test > "$OUT/last.log" 2>&1 || { echo "   FAIL (uac_test CDC $c)"; tail -15 "$OUT/last.log"; ub=1; }
           done
           [ $ub = 0 ] && echo "   PASS" || fail=1 ;;
  emu)     { . "$HOME/emsdk/emsdk_env.sh" >/dev/null 2>&1; sh web/emu/build.sh >"$OUT/emu_build.log" 2>&1 && $CCR -ffp-contract=off -o build/emu/native_check web/emu/native_check.c -lm && build/emu/native_check build/emu/native.f32; } || { echo "== emu"; echo "   FAIL (build)"; tail -5 "$OUT/emu_build.log"; fail=1; false; } && run emu "$EMSDK_NODE" web/emu/emu_test.mjs build/emu/felucca.wasm build/emu/native.f32 ;;
esac; done
exit $fail
