#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Host tests of the Felucca sources (no hardware). Run from the repo root after ./build.sh:
#   tests/run_tests.sh
#
# Regression suite (tests/regress.c, tests/target_budget.py; details at the top of regress.c):
#   golden renders  every engine x preset, voice modes, FX sends, a 4-track mix: one hash
#                   each in tests/golden.txt. A change of the sound fails with the list of renders.
#   health          clipping, DC, peak level, voices free after the release, silence at the end.
#   CPU             instructions / sample per preset and mix (tests/cpu_baseline.txt, +25 %), ns printed;
#                   target: loop instructions of the render functions in the pi32v2 disassembly
#                   (tests/target_budget.txt, +10 %; exact, static).
#   voices          the budget of 8, steal fades, MONO / LEGATO / UNISON keep their note, the VOICE cap,
#                   no hanging notes on any MIDI / key routing.
# ENGINE MEMORY (tests/union_test.c, 1.5): engine switches on a part (every pair and A -> B -> A of the engines that
#                   share the part's state region, with others), hashed against 1.4's (tests/union_golden.txt).
# UI renders (tests/ui_render.c): every screen in every palette from the real drawing code: the layout lint (no text
#                   off the screen, cut, hidden, overlapping or spilling out of its cell / card; only free text
#                   ellipsised), GREY gray, MONO neutral, the draw cost, the text audit (build/ui_new/text_audit.tsv);
#                   PNGs of GREY MONO GREEN PAPER NIGHT in build/ui_new (tests/ui_render.py), the findings in build/ui_new/report.txt;
#                   FM6's 32 algorithm charts as drawn (no box overlapping, no route through a box or crossing another), again
#                   on OPERATOR with each operator picked and its frequency text at its widest (1.2, #76);
#                   every frame of the rolling digits (lint, GREY, MONO), their filmstrips in build/ui_slot;
#                   the alignment: every text / icon / keycap meant to be centred, or on its neighbours' line, by its
#                   ink against its box (cells, chips, buttons, rows, knobs, the roll's strip, the keycaps' pills), every
#                   screen in FLAT and LINE and every palette, over every value it can show; 1 px off or more fails
#                   (build/ui_new/align.txt). MENU > LARGE: every screen again (FLAT, LINE, every palette) and the page / value
#                   sweep with the tall cards, the same lint and alignment; sheet_LARGE_GREY.png, sheet_LARGE_MONO.png.
# UI (tests/ui_test.c): the UI sources against stub display / buttons / knobs: sound loads keep the steps and
#                   the track's ARP / SCL, the SAVE > PHRASES loader and its REPLACE? dialog, the
#                   one-step undo of both (SAVE held), REC on TRACKS / SEQ / ARP, STEP and ARP while recording,
#                   MIDI IN ROUT, saves refused while playing and the OVERWRITE? dialog, MUTE on TRACKS KNOB 1, FM6's operator
#                   pages (each byte alone, SLOT OWN, the sound, undo, the macros, projects / user presets).
# Audio / persistence / editor: the real C paths against simulated DMA and NOR flash: bounded overload
#                   fades, shared-voice limits, deferred settings and retries, failed-save rollback, the autosave
#                   (1.2: when it writes, the power-on restore, damaged copies, a write cut short, wear over a session),
#                   malformed transfers, transport-stop timeouts, MIDI and UART recovery; the MENU settings over the
#                   editor (MENU_DESC / MENU_SET: every item, clamping, unknown ids, saving, USB SERIAL applied later).
# FM6 BANK (tests/fm6bank_test.c, 1.4.1): the voice bank (src/fm6_vbank.c) and its editor commands against simulated NOR:
#                   written whole and read back, its sectors (0xE7000, staged in the autosave's older copy, nothing else
#                   touched), a power cut after any programmed byte or in any erase (the old bank or the new one, the
#                   autosave kept), a wrong CRC, an autosave between requests, SLOT B1..B32 loading the exact records,
#                   empty bank slots skipped / refused, projects and user presets keeping a bank voice, stored SLOT values
#                   (projects, user presets, motion) loading as before.
# CHORD (tests/chord_test.c): the chord keys (src/chord.c): diatonic triads / sevenths of several scales and roots,
#                   the fixed shapes and voicings (at most 4 notes), names, MONO plays the root, a release ends
#                   exactly what its key / MIDI note started, recording, the ARP, MIDI IN, kits ignore CHRD.
# FUN10 (tests/fun10_test.c, 1.2): LFO 2 (SYNC one cycle per note value at several tempi and on the external clock, OFF bit
#                   for bit as before; TRIG NOTE / FREE; POL UNI 0..+1, the matrix's AMP), NUDGE (live recording keeps a
#                   note's time, QUANTIZE ON plays as before, OFF plays the nudges late and early once a pass, never twice
#                   with live notes, RATCH on its start, nudges 0 play the same either way, a tempo change of the external
#                   clock, SEQ > AUTOMATION's QUANTIZE and NUDGE rows, STEP steps held + PRESETS), 128 motion records, FUN10 round trip, FUN9 /
#                   FUN8 / FUN7 as 1.1 / 1.0 wrote them (tests/old_pack.h) loading exactly, user presets of 99 / 103.
# RATCH (tests/ratchet_test.c): a step's ratchet (x1..x4): its parts in the sequencer (equal, gated, chords and drum
#                   hits whole, one chance roll, swing, no slide or tie out, STOP), FUN8 round trip and older projects x1,
#                   user preset patterns, SEQ > AUTOMATION's RATCH rows and the roll / grid drawing.
# STEP 1.5 (tests/step15_test.c, #178 #173): SEQ > STEP's KNOB 3 LEN on a note (clean TIEs written after it and taken
#                   back, stopping at the next note and at LEN, chords, undo, TIME elsewhere, the DRUM grid untouched,
#                   the longer note played), a step's VEL as it plays (96 by default, ACC 127, drum hits) and
#                   SEQ > AUTOMATION's VEL rows (SHOW / HIDE, KNOB 4, EDIT, undo).
# MOD (tests/mod_test.c): the modulation matrix: slots that do nothing are bit-identical, every source on each
#                   kind of destination, clamping, MIDI CC1 / CC11 / aftertouch routing, the cost of 4 active
#                   slots (at most +5 %), demos in build/mod_demo/.
# SOUND (tests/sound14_test.c; #132 #104 #148): the matrix's S&H / SLEW sources (a value per LFO cycle, the glide) and
#                   DEPTH destination (the LFO's gain; none = untouched), ANALOG WAVE SYNC / SUB (pitch, no DC, bounded,
#                   the band-limited restart), SPREAD (SPRD 0 / MONO / LEGATO bit for bit, a note hard left / right, the
#                   mono sum, UNISON, the mute key on both sides, mono sends), the cost; demos in
#                   build/sound14_demo/.
# INSERT (tests/insert_test.c, 1.5; #78 #177): the per-track INSERT (src/fx.c track_insert): TYPE OFF and MIX 0 bit for
#                   bit (POLY, SPREAD), each TYPE at its corners on 3 and 8 voices (no DC, bounded, MIX 50 % halfway
#                   to the wet), the MIX glide and TYPE changes, MIX / TYPE as automation and locks, FUN10 and user
#                   presets of 109 and 104, the pages' labels, the cost per type; demos in build/insert_demo/.
# SOUND 1.5 (tests/sound15_test.c; #104 #175): ANALOG's filter TYPE (LP bit for bit, every other engine ignores it, BP / HP
#                   responses around the cutoff, RES, bounded at the corners), ENV SYNC (OFF bit for bit, the note values
#                   on the knob, attack / decay / release times at 60 / 120 / 173 BPM and on the external clock), the cost;
#                   demos in build/sound15_demo/ (analog_bp, analog_hp, env_sync).
# REVERB (tests/reverb_test.c): REVERB TYPE (src/fx.c): ROOM bit for bit as before, SPRING's decay against SIZE,
#                   its chirp (group delay rising with frequency), stability at the corners, level, a model change
#                   without a click, its cost against ROOM (+30 % at most); demos in build/fx_demo/. 1.2's HALL: SPRING
#                   bit for bit as before, decay against SIZE and DAMP, stability, exact silence after the tail (27
#                   settings and levels), no offset, level, stereo, echo density / ringing / flutter against ROOM, SPRING
#                   and COMB8 (8 combs + 4 allpasses, host only, for comparison), model changes both ways, cost; the
#                   listening set in build/reverb_demo/ (drums, pluck, piano: dry and each model at SIZE 40 / 90 / 127).
# PLATE (tests/plate_test.c, 0.2.14): the sixth reverb (G_RTYPE 5): determinism, loud input, RT60 against SIZE, stability at the corners,
#                   exact silence after the tail, no offset, stereo, SPLIT buses A / B, model changes to and from it, cost; demos in build/plate_demo/.
# (SLICE, SAMPLE: their tests, tests/slice_test.c and tests/sample_test.c, went with the engines: DUELL 0.2.9)
# INPUT (tests/input_test.c): the key / button debounce of hal/fm1_input.h against the TIMER5 scan and bouncing
#                   contacts: a press within 2 scans (<= 2.3 ms), one note per bouncy press, no early or hanging
#                   release, stray samples ignored, fast repeats, the encoders' detents; the LED scan: lit LEDs every
#                   frame, dim ones a short pulse (the second line write) every frame, each only on its own column;
#                   the breath (#119): dark .. ~60 % of lit (DIM LO ~30 %), smooth, no dark run over ~10 ms near its peak;
#                   the power-on sweep (1.1, hal/fm1_led_anim.h): its length, every LED every frame at its level, the
#                   head left to right with its tail, the buttons, the end on the idle glow, the tick as before after;
#                   the idle animation (1.2, MENU > ANIM IDLE): endless, every LED every frame, the light there and back,
#                   the buttons breathing, a key down ends it before its next frame.
# CLICK (tests/click_test.c, 1.1): the metronome (src/click.c) and the count-in (src/seq.c) through audio.c: each beat at
#                   the first sample of its block, with a 1/16 track's steps 1 5 9 13 (internal and external clock, DIV 1/8
#                   and SWING too), the accent, the frequencies, length, LEVEL, MASTER, OFF / REC / ON, USB audio bit for bit
#                   without it; COUNT-IN 1 / 2 BARS (step 1 exactly N bars after PLAY, STOP cancels, PLAY again nothing,
#                   never with an external clock), notes in its last eighth onto step 1; build/click_demo/.
# USB audio (tests/uac_test.c): the UAC1 descriptors as a host parses them (with and without CDC; 44100 and
#                   48000 Hz), the ring and packetiser: 44.1 frames per packet, every frame in order (bit for bit),
#                   underrun / overrun, restart; SET_CUR; the 44.1 -> 48 kHz resampler (every frame against a
#                   double-precision one, response, SNR, cost); the stream at 48 kHz (47..49 frames per packet,
#                   fast and slow I2S clocks, switching rates mid-stream).
# web (web/test_web.mjs): the editor protocol against its mock device, whose tables must equal the
#                   firmware's (tests/descdump.c -> build/host/desc.json; the MENU settings: tests/editor_test.c -> build/host/menu.json),
#                   the package builder, the updater.
# Browser emulator (web/emu, when emcc is there): the firmware in WebAssembly (build/emu) boots, plays keys and MIDI,
#                   draws, lights its LEDs, keeps a save across instances, plays a song bit for bit as the same file
#                   built with cc (web/emu/native_check.c); the cost of 1 s of a heavy song against real time.
# PHYS (tests/phys_test.c): stability over the whole parameter and pitch range, the worst-case cost against
#                   the heaviest factory preset, demos in build/phys_demo/; tests/phys_ref.cpp compares the
#                   fixed-point models with DaisySP's float originals when DaisySP is there (DAISYSP=path).
# DRUM (tests/drum_test.c) and DIGITAL -> FM6 (tests/fm4_test.c, digital_test.c, fm4_div0_test.c): removed in 0.2.14 with the engines (core.h ENGI_RETIRED).
# NOISE (tests/noise_test.c): the engine (src/eng_noise.c): COLR's slope (white, pink, brown), the filter and the
#                   register clock following the key, META periodic at the key, no DC, no clipping at the
#                   corners, a note from silence the same twice, the cost per voice; demos in build/noise_demo/.
# FM6 (tests/fm6_test.c): the 6-operator FM engine (src/eng_fm6.c, src/fm6_core.c): the 32 algorithms' carriers, the
#                   operator envelopes (stages, rates, the voice ending), bit-stable notes, a click-free retrigger, no DC /
#                   clipping over the factory patches, the macros' directions, PTCH, pack / unpack and the SysEx
#                   layouts, the 6-voice cap, the cost per voice; demos in build/fm6_demo/.
# ROBUST (tests/robust_test.c): damaged or crafted stored data and editor requests: engine numbers past the last refused, a retained older
#                   RAM project bounded, user preset patterns inside their fields, malformed requests never stop PLAY.
# Sanitizers (ASan + UBSan, when the compiler has them; SANITIZE=0 skips): the stored-data and protocol tests again
#                   (loader, M-UPGRADE entry, editor, projects, backup, ROBUST) and three short fuzz runs with fixed
#                   seeds: tests/fuzz_ed.c (editor SysEx and raw USB-MIDI packets), tests/fuzz_proj.c (mutated project
#                   stores -> import -> restore -> render). Longer runs: build/host/asan/fuzz_ed 300000 7 (iterations, seed), the same
#                   for the others. (tests/fuzz_smp.c, the user sample slots, went with them: DUELL 0.2.9)
#                   UBSan leaves out the DSP's intended wraps (signed overflow, shifts) and the XIP rebase of a user zone's
#                   offset (bounds, object-size, pointer-overflow: correct on the device's flat flash, not in C's model).
# Change baseline entries only for reviewed, intentional differences in sound or cost;
# retain every unaffected golden / CPU / target entry. VERBOSE=1: every render.
set -e
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT"
CC="${CC:-cc} -O1 -Wall -Wno-unused-function"
fail=0
run() { echo "== $1"; shift; "$@" || fail=1; }

$CC -o "$OUT/storage_test" tests/storage_test.c
run "flash storage (A/B, torn writes)" "$OUT/storage_test"

$CC -o "$OUT/upreset_test" tests/upreset_test.c
run "user presets (UP_PUT parser, bank round trip, versions, grid records read as empty, retired engines kept)" "$OUT/upreset_test"

$CC -o "$OUT/input_test" tests/input_test.c
run "keys and buttons: fast press, long release, bouncy contacts (one note each), glitches, encoders" "$OUT/input_test"

$CC -o "$OUT/midi_uart_test" tests/midi_uart_test.c
run "TRS MIDI parser" "$OUT/midi_uart_test"

HALF=$(sed -n 's/^#define HALF_FRAMES \([0-9]*\).*/\1/p' firmware/src/core.h)
$CC -O2 -DT_CDC=1 -DHALF_FRAMES=$HALF -o "$OUT/uac_test" tests/uac_test.c -lm
run "USB audio input: descriptors (with CDC), ring and packets, 44.1 -> 48 kHz resampler, 48 kHz stream" "$OUT/uac_test"
$CC -O2 -DT_CDC=0 -DHALF_FRAMES=$HALF -o "$OUT/uac_test_nocdc" tests/uac_test.c -lm
run "USB audio input: descriptors (without CDC), ring and packets, resampler, 48 kHz stream" "$OUT/uac_test_nocdc"
# USB descriptor layouts (#67): CDC UAC LAYOUT CDC-presented 48K; layout 0 and the console left out = 1.0's bytes
# (+ the 48 kHz rate with 48K = 1, the default; 48K = 0: byte for byte)
for v in 1.1.0.1.1 1.1.1.1.1 1.1.2.1.1 1.1.3.1.1 1.1.0.0.1 1.1.2.0.1 0.1.0.1.1 1.1.0.1.0 1.1.0.0.0 1.1.2.1.0 0.1.0.1.0 \
         1.0.0.1.1 1.0.1.1.1 1.0.0.0.1 0.0.0.1.1; do
    IFS=. read -r t_cdc t_uac t_lay t_on t_48 <<EOF
$v
EOF
    $CC -DT_CDC="$t_cdc" -DT_UAC="$t_uac" -DT_LAYOUT="$t_lay" -DT_ON="$t_on" -DT_48K="$t_48" -o "$OUT/usb_desc_test" \
        tests/usb_desc_test.c
    run "USB descriptors: CDC $t_cdc (presented $t_on), UAC $t_uac (48 kHz $t_48), layout $t_lay" "$OUT/usb_desc_test"
done
# DUELL 0.2.14 (from Felucca 1.5.1, #67): the default lists the audio streaming interface first in the AC collection;
# FELUCCA_UAC_AS_FIRST=0 is the order up to 0.2.13 (MIDI first), 1.0's bytes, where the AppleUSBAudio model stops
for v in 1.1.0.1.1 1.1.0.0.1 1.1.0.1.0 1.1.0.0.0 0.1.0.1.1; do
    IFS=. read -r t_cdc t_uac t_lay t_on t_48 <<EOF
$v
EOF
    $CC -DT_CDC="$t_cdc" -DT_UAC="$t_uac" -DT_LAYOUT="$t_lay" -DT_ON="$t_on" -DT_48K="$t_48" -DT_ASF=0 \
        -o "$OUT/usb_desc_test" tests/usb_desc_test.c
    run "USB descriptors (AC collection MIDI first): CDC $t_cdc (presented $t_on), UAC $t_uac (48 kHz $t_48)" \
        "$OUT/usb_desc_test"
done

[ -f build/felucca.fwsc ] || { echo "run ./build.sh first"; exit 1; }

$CC -DOWN_PKG=1 -o "$OUT/ota_test" tests/ota_test.c
run "M-UPGRADE entry (own loader)" "$OUT/ota_test" build/felucca.fwsc

head -c 200000 build/felucca.bin > "$OUT/old_app.bin"
python3 tools/fm1pkg_make.py "$OUT/old_app.bin" build/loader/ota.bin "$OUT/old.fwsc" >/dev/null
$CC -o "$OUT/ldr_test" tests/ldr_test.c
run "update loader: other app -> this build" "$OUT/ldr_test" "$OUT/old.fwsc" build/felucca.fwsc

if [ -f build/gen/felucca_tables.h ]; then
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/hostsim" tests/hostsim.c -lm
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/scale_test" tests/scale_test.c -lm
    run "scales: white-key mapping and note lifecycle" "$OUT/scale_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/chord_test" tests/chord_test.c -lm
    run "chord keys: diatonic and fixed chords, voicings, MONO root, releases, recording, ARP, MIDI IN, kits" "$OUT/chord_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/speaker_test" tests/speaker_test.c -lm
    run "SPEAKER EQ: FLAT / LOWCUT / BASS+ responses, BASS+ harmonics of the bass, the sub cut, no offset after, headroom on a full-scale kick (#180)" "$OUT/speaker_test"
    run "DSP render (ANALOG preset 0)" "$OUT/hostsim" 0 0 1 "$OUT/render.wav"
    mkdir -p build/tracks_demo
    run "TRACKS: 4-track pattern, live recording (lengths, swing), voice budget, engine switch, cost" env TRACKS=build/tracks_demo "$OUT/hostsim" 0 0 1 "$OUT/tracks.wav"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/project_test" tests/project_test.c -lm
    run "project formats (FUN1..FUN5 -> FUN6, the song chain; tracks on retired engines kept as stored)" "$OUT/project_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/motion_test" tests/motion_test.c -lm
    run "motion, whole-step chance, FUN7 migration, song restore, ARP repeat and the 1.2 ARP modes" "$OUT/motion_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/ratchet_test" tests/ratchet_test.c -lm
    run "RATCH: x1..x4 in a step (notes, chords), gates, chance, swing, projects, user presets, AUTOMATION RATCH rows" "$OUT/ratchet_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/step15_test" tests/step15_test.c -lm
    run "1.5 STEP: a note's LEN (TIEs on KNOB 3), a step's VEL (playback, AUTOMATION's VEL rows)" "$OUT/step15_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fun10_test" tests/fun10_test.c -lm
    run "1.2 (FUN10): LFO 2 SYNC / TRIG / POL, NUDGE (record, QUANTIZE, play, controls), 128 motion records, FUN10 / FUN9 / FUN8 / FUN7, user presets" "$OUT/fun10_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/midi_control_test" tests/midi_control_test.c -lm
    run "USB/TRS clock, bend, sustain, ownership and panic recovery" "$OUT/midi_control_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/theme_test" tests/theme_test.c -lm
    run "themes: contrast, text blending and font metrics" "$OUT/theme_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/text_ref_test" tests/text_ref_test.c -lm
    run "text: pens, kerning and pixels equal the reference renderer" "$OUT/text_ref_test"
    if python3 -c "import PIL" 2>/dev/null; then
        run "text spacing: glyph gaps against the font's own (16x supersampled), S M <= 0.25 px, L <= 0.5 px" \
            python3 tests/text_spacing_test.py build/gen/ui_fonts.h
    fi
    $CC -O1 -w -Ibuild/gen -o "$OUT/settings_test" tests/settings_test.c
    run "settings: PER1..PER4 migration, palette ids and preference preservation" "$OUT/settings_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/ui_test" tests/ui_test.c -lm
    run "UI: sounds keep steps, undo, recording, MIDI overflow, pending saves, panel recovery, song chain, GREY gray, MONO neutral" "$OUT/ui_test"
    FV=$(sed -n 's/^#define DUELL_VERSION_STR "\(.*\)"/\1/p' build/gen/duell_version.h)   # (the splash, ABOUT: the real version)
    $CC -O1 -w "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/ui_render" tests/ui_render.c -lm
    mkdir -p build/ui_new/ppm build/ui_slot
    run "UI renders: layout lint (every screen and palette, every page, engine and column value), GREY gray, MONO neutral, alignment by ink (1 px fails), draw cost" \
        "$OUT/ui_render" build/ui_new build/ui_slot
    if python3 -c "import PIL" 2>/dev/null; then python3 tests/ui_render.py build/ui_new build/ui_slot; fi
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/audio_test" tests/audio_test.c -lm
    run "audio: overload protection, bounded fades and DMA diagnostics" "$OUT/audio_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/click_test" tests/click_test.c -lm
    run "metronome and count-in: beats sample for sample (internal and external clock), accent, level, MASTER, not in USB, count-in timing, notes onto step 1" "$OUT/click_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/persistence_test" tests/persistence_test.c -lm
    run "persistence: deferred settings, retry and failed-save rollback" "$OUT/persistence_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/backup_test" tests/backup_test.c -lm
    run "full backup: CRC before writes, stale runtime, USB reset / timeout, malformed objects, older projects" "$OUT/backup_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/fm6bank_test" tests/fm6bank_test.c -lm
    run "FM6 voice bank (1.4.1): write / read, power cuts, CRC, SLOT B1..B32, stored SLOT values as before" "$OUT/fm6bank_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/editor_test" tests/editor_test.c -lm
    run "editor: real C protocol, malformed transfers, queue recovery, MENU settings (writes build/host/menu.json)" \
        env MENU_JSON="$OUT/menu.json" "$OUT/editor_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/robust_test" tests/robust_test.c -lm
    run "robustness: crafted sample slots, engine numbers, retained old projects, preset patterns, malformed requests" \
        "$OUT/robust_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/mod_test" tests/mod_test.c -lm
    mkdir -p build/mod_demo
    run "modulation matrix: off = bit-identical, the math, MIDI CC1 / CC11 / aftertouch, cost, demos" "$OUT/mod_test" build/mod_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/sound14_test" tests/sound14_test.c -lm
    mkdir -p build/sound14_demo
    run "sound items: matrix S&H / SLEW / DEPTH, ANALOG SYNC / SUB, SPREAD (bit for bit at 0, L / R, mono sum), cost, demos" \
        "$OUT/sound14_test" build/sound14_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/insert_test" tests/insert_test.c -lm
    mkdir -p build/insert_demo
    run "INSERT (1.5): OFF / MIX 0 bit for bit, each TYPE (no DC, bounded, dry / wet), MIX glide, automation, stored, cost, demos" \
        "$OUT/insert_test" build/insert_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/sound15_test" tests/sound15_test.c -lm
    mkdir -p build/sound15_demo
    run "1.5 sound items: ANALOG filter TYPE LP / BP / HP (responses, LP and other engines bit for bit), ENV SYNC (times at several tempi), cost, demos" \
        "$OUT/sound15_test" build/sound15_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/swing_test" tests/swing_test.c -lm
    run "SWING: track + global at most 100, sequencer step lengths, the SWG display" "$OUT/swing_test"
    mkdir -p build/fx_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/reverb_test" tests/reverb_test.c -lm
    mkdir -p build/reverb_demo
    run "REVERB TYPE: ROOM / SPRING bit-identical, SPRING and HALL decay / stability / level, HALL silence / stereo / ringing, model changes, cost, demos" "$OUT/reverb_test" build/fx_demo build/reverb_demo
    mkdir -p build/plate_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/plate_test" tests/plate_test.c -lm
    run "PLATE (the sixth reverb): determinism, loud input, decay, corners, exact silence, no offset, stereo, SPLIT buses, model changes, cost" "$OUT/plate_test" build/plate_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/regress" tests/regress.c -lm
    run "regression: golden renders, health, voices, CPU budget" "$OUT/regress" tests/golden.txt tests/cpu_baseline.txt
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/union_test" tests/union_test.c -lm
    run "engine memory (1.5): every switch between the engines with per-part state sounds as in 1.4 (720 sequences)" \
        "$OUT/union_test" tests/union_golden.txt
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/descdump" tests/descdump.c -lm
    echo "== parameter and engine tables as JSON (for the editor mock test)"
    "$OUT/descdump" > "$OUT/desc.json" || fail=1
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/phys_test" tests/phys_test.c -lm
    mkdir -p build/phys_demo
    run "PHYS: stability C-1..G9 over the parameter corners, worst-case cost against PHASE WIRE, demos" "$OUT/phys_test" build/phys_demo
    D=${DAISYSP:-vendor/DaisySP}/Source
    if [ -d "$D/PhysicalModeling" ] && command -v c++ >/dev/null 2>&1; then
        $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -c -o "$OUT/phys_fixed.o" tests/phys_fixed.c
        c++ -O2 -std=c++14 -w -I"$D" -I"$D/Utility" -o "$OUT/phys_ref" tests/phys_ref.cpp \
            "$D/PhysicalModeling/modalvoice.cpp" "$D/PhysicalModeling/resonator.cpp" "$D/PhysicalModeling/stringvoice.cpp" \
            "$D/PhysicalModeling/KarplusString.cpp" "$D/Filters/svf.cpp" "$D/Utility/dcblock.cpp" \
            "$D/Dynamics/crossfade.cpp" "$OUT/phys_fixed.o"
        run "PHYS: the fixed-point models against DaisySP's float originals (mode frequencies, decays, Svf)" "$OUT/phys_ref"
    else
        echo "== skip PHYS reference test (no DaisySP: set DAISYSP to a checkout)"
    fi
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/noise_test" tests/noise_test.c -lm
    mkdir -p build/noise_demo
    run "NOISE: colour slopes, key-tracked filter and clock, META period, DC, clipping, retrigger, cost, demos" "$OUT/noise_test" build/noise_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fm6_test" tests/fm6_test.c -lm
    mkdir -p build/fm6_demo
    run "FM6: algorithms, envelopes, retrigger, DC, clipping, macros, patch formats, voices, cost, demos" "$OUT/fm6_test" build/fm6_demo
else
    echo "== skip hostsim (run ./build.sh once)"
fi

# ASan + UBSan: the stored-data and protocol paths, and short fuzz runs (fixed seeds: the same inputs every run)
SAN="-O1 -g -fsanitize=address,undefined -fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow"
SAN="$SAN -fno-sanitize-recover=undefined -w"
san_ok() {
    printf 'int main(void){return 0;}\n' > "$OUT/san_probe.c" &&
        ${CC%% *} $SAN -o "$OUT/san_probe" "$OUT/san_probe.c" 2>/dev/null && "$OUT/san_probe"
}
if [ "${SANITIZE:-1}" = 0 ]; then
    echo "== skip the sanitizer runs (SANITIZE=0)"
elif [ ! -f build/gen/felucca_tables.h ]; then
    echo "== skip the sanitizer runs (run ./build.sh once)"
elif ! san_ok; then
    echo "== skip the sanitizer runs (the compiler has no ASan / UBSan)"
else
    mkdir -p "$OUT/asan"
    A="$OUT/asan"
    SCC="${CC%% *} $SAN -Ibuild/gen -Ifirmware/src"
    export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
    $SCC -o "$A/ldr_test" tests/ldr_test.c
    run "ASan/UBSan: update loader (other app -> this build)" "$A/ldr_test" "$OUT/old.fwsc" build/felucca.fwsc
    $SCC -DOWN_PKG=1 -o "$A/ota_test" tests/ota_test.c
    run "ASan/UBSan: M-UPGRADE entry (own loader)" "$A/ota_test" build/felucca.fwsc
    for t in editor_test project_test backup_test robust_test fun10_test fm6bank_test; do
        $SCC -o "$A/$t" tests/$t.c -lm
        run "ASan/UBSan: $t" "$A/$t"
    done
    $SCC -o "$A/fuzz_ed" tests/fuzz_ed.c -lm
    run "ASan/UBSan fuzz: editor SysEx and raw USB-MIDI packets (20000, seed 7)" "$A/fuzz_ed" 20000 7
    $SCC -o "$A/fuzz_proj" tests/fuzz_proj.c -lm
    run "ASan/UBSan fuzz: project stores -> import -> restore -> render (5000, seed 13)" "$A/fuzz_proj" 5000 13
fi

run "regression: target cost of the render loops (pi32v2 disassembly)" python3 tests/target_budget.py \
    build/felucca.dis tests/target_budget.txt

run "installer CLI (fm1_install.py) against a simulated FM-1" python3 tests/install_test.py

if command -v node >/dev/null 2>&1; then
    run "web pages: editor protocol + samples, package builder, update protocol" node web/test_web.mjs
    run "web backup: capture, validation before writes, restore order" node web/test_backup.mjs
    run "web duell-backup: file, checks, restore plan / order / verify against a simulated unit" node web/test_duellbackup.mjs
    run "web wavetable: WAV converter (FFT, saw / square / Serum-like files, factory renders back), protocol client against a simulated unit" node web/test_wavetable.mjs
else
    echo "== skip web tests (no node)"
fi

if ! command -v emcc >/dev/null 2>&1; then
    echo "== skip the browser emulator (no emcc: Emscripten builds web/emu)"
elif ! command -v node >/dev/null 2>&1 || [ ! -f build/gen/felucca_tables.h ]; then
    echo "== skip the browser emulator (needs node and build/gen)"
else
    run "browser emulator: the firmware to WebAssembly (web/emu/build.sh -> build/emu)" sh web/emu/build.sh
    cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o "$OUT/emu_native" web/emu/native_check.c -lm
    run "browser emulator: the same song from the native build" "$OUT/emu_native" "$OUT/emu_native.f32"
    run "browser emulator: boot, keys, MIDI, screen, LEDs, PLAY, a save kept across instances, = native, cost" \
        node web/emu/emu_test.mjs build/emu/felucca.wasm "$OUT/emu_native.f32"
fi

[ $fail -eq 0 ] && echo "ALL HOST TESTS PASSED" || { echo "HOST TESTS FAILED"; exit 1; }
