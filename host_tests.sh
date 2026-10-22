#!/bin/sh
# DUELL host tests that run on a Mac without the JieLi toolchain (subset of felucca-base/tests/run_tests.sh).
# Usage: ./host_tests.sh [name ...]   names: level benchsim benchrun benchaudio stereochorus chorusskip bassmono comp master bench shimmer locut duck dlymod regress chord mod mxstep modinert halltail perform ui select step3 engine morph morphui mode dlymod stereochorus dlyev hang atgt patslot multi release patch patchraw dx7import dx7compat splash sounds mrender fmbass fm6 loadcost cpuscen uf wave hold guide wtab wavetable sidechain
#   mixsplit partindep duckpart twocore tcmeas tcstage1 tcbench poolrule budget modules uitest project fun10 motion persist backup editor editorjson midictl audio erase flashled usb insert rollbrowser xvalues twin subcyc pages scope2 editb velo midimap midimapmut emu   (default: all; emu only when ~/emsdk exists)
# chorusskip: tests/chorusskip_test.c (0.2.21), built twice (as it is, and -DCHO_NOSKIP = the firmware without fx_bus.c cho_zrun's skip): the plain bus leaves the chorus out of a block whose chorus input is 0 while the whole chorus
#   line is 0 (a send of 0 or nothing playing: ~46 host instructions a sample saved in STEREO and LAYER); both runs hash every output sample and the chorus lines and states over silence, notes starting and stopping, DEPTH / RATE changes,
#   the delay (plain, modulation, PING-PONG, RANDOM), the reverb alone and a random fuzz, in STEREO, LAYER and DUEL; the hash lists must be equal
# tune: tests/tune_test.c (0.2.22), MASTER TUNE in Hz (G_TUNE = A4 in 0.1 Hz, 415.3 .. 466.2, default 440.0; voice.c tune_get): 440.0 bit for bit the TUNE 0 of before (a value out of range is no tuning), the offset in cents of all 510 values against 1200 log2(f / 440)
#   (within the 1/4096 grid's half step), the pitch of a rendered sine at 432.0 / 442.0 / 415.3 / 466.2 Hz, the texts ('440.0' Hz); the menu row, the editor, the project / autosave / ERASE DATA are in uitest, editor, persist, erase
# stereochorus: tests/stereochorus_test.c (0.2.21), the chorus bus in STEREO / LAYER reads two taps (a quarter turn apart) and sends a side: left and right differ (correlation < 0.6) on every bus path, the left is the old mono
#   chorus (+-1 LSB), the mono sum still moves, DEPTH 0 and DUEL / SPLIT (buses A and B) and a send of 0 bit for bit (hashes of 0.2.20), DUEL isolation, no click, exact silence, SPACE +100, cost
# dlyev: tests/dlyev_test.c, the delay MODE (0.2.16 G_DMODE: DIGITAL / PING-PONG / RANDOM / OCT UP; fx_bus.c dly_ev): DIGITAL and PING-PONG bit for bit (hashes of 02f849e, a stored 7 and DUEL too), determinism, the clock (first edge one delay time after
#   PLAY), no click switching modes (also with PING-PONG on), silence after the tails, loud input (FDBK 120), SENDS OFF / STOP ALL clear the state, the pages (MODE first, MIX last) and every mode's faded cards with their reasons (CHANCE, SPREAD / RATE, SPREAD in DUEL), the header's full names, DIGITAL <-> PING-PONG switching, cost;
#   0.2.22 DUEL: every mode per bus, mono: PING = DIGI bit for bit, RAND / OCT+ determinism, each part in its own channel (the other exactly 0, both parts = each alone), events centred, no click, SENDS OFF / STOP ALL / output-mode change clear both buses, events that do not fit the half line, cost
# project / step3 / insert / persist / multi / backup (0.2.14): the old stored formats (FUN1..FUN10, FUN11 revision 0, multi versions 1..2, patches without PTF_CB, PER1..PER3) are REFUSED, not converted
# reverb: tests/reverb_test.c (ROOM / SPRING bit for bit, SPRING and HALL decay, stability, silence, stereo, model changes, cost; 0.2.14: its two stale references re-baselined)
# halltail: tests/hall_tail_test.c (0.2.29), the HALL's tail without a gate (owner, T0226: M07 FINAL REEL "cut like a noise gate"; docs/TAIL_CUT_FINDINGS.md): above -72 dBFS no 100 ms window falls more than 8 dB
#   (the old quiet-line rounding fell 16 .. 33 dB there), the tail reaches -75 dBFS, it still ends in exact silence (126 renders: SIZE 0..150, DAMP, noise / 100 Hz / impulse), ROOM SPRING SHIM RESO PLATE bit for bit (0.2.28's hashes).
#   tests/m07_tail_probe.c (not run): M07's tail per 100 ms, output / dry / wet
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
# sidechain: tests/sidechain_test.c (0.2.22), GLO > SIDECHAIN's MIDI setup (P1 CH, P1 NOTE, P2 CH, P2 NOTE: fx.c sc_midi, the settings record's bytes 14..17), a DEVICE setting: the defaults (10, C2), saved ~2 s later, read back, an older record and every byte value read safely, ERASE DATA, a patch / multi / project load leaves it, a multi record and a project are bit for bit the same whatever it is, DUCK SRC PUMP / PART / MIDI (the old CH1..16 clamped), editable with every SRC
# comp: tests/comp_test.c, 0.2.21's COMP, the master compressor (fx.c comp_stage; the page COMPRESSOR in the GLO round after STEREO MASTER, 0.2.25 layout round: K1 RATIO (OFF lowest), K2 THRESH, K3 ATTACK, K4 RELEASE; core.h G_CT..G_CL / song.cp; a DEVICE setting in the settings record's rsv word, not in multis or the project): RATIO 1:1 bit for bit (the sources-before hash;
#   1:1 after a ratio returns to it; zero code run), the static curve of 480 steady tones within 0.1 dB of the ideal (soft knee, automatic makeup), ATTACK 0.1 ms..100 ms and RELEASE 20 ms..2 s measured within 15 %, the stereo link, silence, overshoot,
#   no click (1:1 <-> a ratio, the settings walked), DUEL bypassed (and cleared at a mode change), the page, its cards and hide rules, the stored word (every byte value, an older record, ERASE DATA, not in multis / project), the stage's place in mix_block, cost
# master: tests/master_test.c (0.2.23), the master WIDTH (fx.c width_stage; 0.2.25 layout round: the GLO page STEREO MASTER, K1 BASS MONO, K2 WIDTH; core.h G_WD / song.ms; a DEVICE setting in the settings
#   record's bytes [15][18..21], ui.c ms_sync): 100 % bit for bit (the sources-before hash; zero code run), WIDTH: the mid exactly untouched, 0 % mono, the side against frequency (only above the 125 Hz crossover
#   when wider, all of it when narrower), BASS MONO after it, no click, back to rest; DUEL bypassed (and cleared at a mode change), LAYER runs it; the pages, card, hide rule, stored bytes (a 0.2.24 record's DRIVE byte is ignored),
#   ERASE, not in multis / project; cost. 0.2.25: the master saturator (and its tests) is removed
# mxstep: tests/mx_step_test.c (0.2.25, the matrix round; release_drafts/MATRIX_DESTINATIONS.md): the source slew (mod.c mx_slew: a MODW jump -> CUT, an LFO at RATE's top and a 10 ms attack pass unchanged),
#   the in-block ramps of PAN, PATCH LEVEL and the sends when the matrix moves them (fx.c mix_ramp: the jump, an 8 Hz LFO, a route switched off), SAT's blend zone (fx.c track_dist_ramp); 2nd differences in 1/1000 of the peak
#   hunt1 (release_drafts/CLICK_HUNT1.md): the same for the MORPH fader (mod.c mod_mixmv counts the open window), SAT out of / into 0, a SPREAD part (fx.c spread_run / spread_post: jumps, LFO, the way back, the fader; steady
#   SPREAD renders bit for bit; the mid is the plain mix's, the side within 2 LSB), and LFO 1 / LFO 2 for their own depths (voice.c: a SAW / SQUARE edge on the VOICE engine's SHAPE, seam ratio). tools/hunt_probe.py: the click hunt's trace
# modinert: tests/mod_inert_test.c (0.2.25, docs/MOD_INERT_TABLE.md): the table of what a wave leaves without effect proved by rendering (SHAPE on the ten engines and every ANALOG / LOFI / TRIO wave,
#   TRIO INT2 INT3 DTN PW and LOFI DUTY end to end), and the list of removed matrix destinations
# mixsplit: tests/mixsplit_test.c (MIXSPLIT, job 1 of the second-core work), fx.c's mix_block split into stages (mix_events, part_voice / part_rest / part_fx / part_mix / part_end, mix_sends, mix_master): five scenes (STEREO, DUEL, LAYER; SPREAD, EQ, SAT, DUCK SRC PART, MUTE,
#   PART FX / SENDS OFF, SPACE, the master stages, a MODE change, a silent tail) rendered by the old combined path (tests/mixsplit_ref.h, verbatim), by mix_block and by the stages called in sequence: every output sample equal, bit for bit
# partindep: tests/partindep_test.c (MIXSPLIT job 2, partstate): the two parts' stages are independent: six scenes (STEREO, DUEL, LAYER; SPREAD, EQ, SAT, PART FX crossfade, PAN step, MUTE, MODE change, silent parts, MORPH on both parts, the matrix's bus offsets of both parts;
#   no DUCK SRC PART, no FORMANT: the job 3 blockers) each rendered with the parts' stages in six schedules (1 then 2, 2 then 1, and four stage-interleaved orders): all bit for bit equal to the old combined path (tests/mixsplit_ref.h), to mix_block and to the
#   hashes of the commit before the work. Self-check (name partindep_mut, in the same run): the test is built against a copy of the firmware with ONE shared write put back (the parts' block scratch, morph window, matrix window made one) and must FAIL
# duckpart: tests/duckpart_test.c (job 3, one-sided ducking): DUCK SRC PART on one part only; the ducked part reads the other part's key of the block before. Renders the old path (shared key read live) and the new one and reports the largest sample
#   difference and the RMS of the difference in dB: a ducked part 2 differs slightly (a block older key), a ducked part 1 is bit for bit equal
# twocore: tests/twocore_sim.c (job 3, rewritten in job 5 to the unit's protocol): core 1 is a pthread running the real tc_c1_step loop (separate 32-bit words, a sequence number, core 1 decides who plays a job) with the computer hal, its job the real
#   tc_job_run (part 2's stages); core 0 is the main thread running the firmware's own mix_block with DUELL_TWOCORE on (the runner: deadline, post, wait, take, the late cases, pausing; a half = 4 blocks of 32). Nine scenes of partindep_scn.h,
#   each in a child process with one injected fault: torn reads (once: re-read; persistently: refused), late jobs (before core 1 looks: take / yld; after its claim: done in the half), a run of late blocks (pause, resume), a wrong check value,
#   a stale done word, a done word ahead, a misread snapshot, a hung core 1 before and after its claim (HANG after 100 ms simulated), a flash park (in play; with core 1 stuck: FLASH after 20 ms), a core 1 that finishes after its half, no start
#   (unarmed, NOANSWER). The output must equal the old combined path (tests/mixsplit_ref.h) bit for bit in every run, except the hung core 1: equal up to the hang, then equal to a one-core run that leaves out exactly the counted silent
#   blocks. Also: the late path taken and counted, no bus B state written by core 1 (stage 1), a deliberate shared write seen, the fade at once when the jobs pause or core 1 stops (no step over 1.5 x the calm before), counters per scene.
#   Self-check (twocore_mut): copies of the firmware with ONE shared write put back, or the runner changed, must make it FAIL. twocore_tsan (not in the default list, slow): the runs without a silent window under ThreadSanitizer,
#   TSAN_SCENES="0 8" picks the scenes; no data race may be reported
# tcmeas (BENCHT2): the same simulation built with -DTC_MEAS=1 (core 1 timestamps its jobs, core 0 collects start delay / run time / finish margin, the own split, the clear, the runtime switches' variables): in every run the own split adds up to
#   own, the three histograms to the jobs measured, the very late jobs by block to the very late jobs; no fault: every job posted measured, margins never negative, all own blocks 'deadline already past'; late_pre: the taken-back jobs have no figures;
#   late_post: every very late job measured with a run time and a negative margin; park, pause, strike and one-core runs show in the own split; the clear (tcmclr) restarts the figures at the block after the request. Five mutants of it
#   (a job end stamped before the job, the margin's sign, deadline-past counted as paused, the very late block not counted, the clear ignored) must make it FAIL. The bench words and columns are in benchsim / benchrun.
# tcstage1: tests/twocore_stage1_test.c (two-core stage 1, branch cloud/stage1), the hand-over's words and core 0's safety net of
#   firmware/src/twocore.{h,c} with the computer's hal (tests/twocore_hal_host.h), one thread, core 1's step called by hand: claim / done, take before and after the claim (yld / done),
#   a check value read wrong (refused, BADJOB) and one that fits at the re-read, a wrong job number, a stale and a too-new done word, the heartbeat (HANG), the read check (MISREAD),
#   the flash park (at once; a job left open: FLASH after 20 ms), the record across a reset and the armed one-shot start, which reasons are strikes, the stack mark. docs/TWOCORE_STAGE1.md
# budget: tests/budget_test.c (job 4, branch cloud/notes), the NOTES budget (firmware/src/budget.c, docs/MIXSPLIT_NOTES.md section 8): how many notes each part may play, from the settings, one core, 80 % of the time with a margin of 1.15.
#   (a) the cost table reproduces tests/bench_baseline.txt (the bench's figures; no counter needed): every engine's voice, INSERT type, SAT, EQ, DUCK, bus effect in STEREO and DUEL, master stage within 5 %, the worst-case scene of each MODE (fixed part within 5 %,
#   totals never below the bench and within 12 %); (b) the 16 factory multis: NOTES a part, what limits it, the VOICE CAP of 0.2.11 beside it (printed, as loaded and in cpuscen's MORPH state); (c) switching on an INSERT, 24 dB, SAT, an EQ band, the ducker, a matrix slot,
#   RANDOM, SHIM DOWN, COMP, WIDTH or the chorus never raises that part's number (16 multis and 5000 random settings); (d) 12 note-ons on a part with a budget of 4 (also 1, and one worked out from settings): at most that many voices, no step at a steal over 1.5 x
#   the calm before it; 8 notes held, the number dropped to 3: the next note-on fades the rest out; (e) the same settings give the same numbers in any order of calls and after any history; bit for bit: a multi with the notes its budget allows is the same with the
#   budget on and off (every render in a child process: the firmware keeps generator states a load does not reset)
# sat: tests/sat_test.c (0.2.25), the part SAT (P_DIST; fx.c dist_follow, mix_part) without a zipper: a held note on ANALOG, PHASE and FM6, SAT stepped (0 -> 100, 20 -> 127, 127 -> 20, 100 -> 0, 5 -> 11, 0 -> 127): the largest sample step
#   within 1.5 x the steady renders', no block boundary stepping more than 1.5 x the steady step there + 8 % of the peak (45 % before the fix), settled bit for bit on the new SAT
# benchsim: tests/bench_sim.c (the developer bench, ported from wip/bench0226 in bench suite stage 0), the device BENCH (firmware/src/bench.c, MENU > SYSTEM > BENCH of the
#   DUELL_BENCH builds; docs/BENCH_GUIDE.md) run on the Mac against a simulated audio interrupt: the row list, a whole run from the menu, the table pages, the CSV log (with the
#   NOTES, RAM code and restart columns), the cost rows holding the NOTES budget open and the M16 rows playing with it, nothing written, the session put back, OCT- stops, K3 the
#   STRESS row, the model header firmware/src/bench_model.h current (BENCH_MODEL_UPDATE=1 build/host/bench_sim rewrites it). The normal build has none of it
#   Bench suite stage 1 (also in benchsim): the console words (`bench reset | load | mode | set | scene | ev | measure | run | stop | end | save-test`, their replies and errors, every
#   line under the console's 64 characters), the timeline player (an event plays at the half it falls in, the CPU window, the ramps, the flash event, 256 events streamed while a test
#   runs) and the per-test reset list: every factory patch, every factory multi and every row of the bench list gives the same samples when it is played after others (forward, then
#   backward), and with the reset switched off most do not (the check can fail). `build/host/bench_sim --serve` is the twin as the unit for the next test
# benchrun: tests/bench_run_test.py, the Mac's runner tools/bench_run.py (bench suite stage 1): the suite parser and the suite OWNER-1 (tools/bench_suites; every patch number and name
#   against docs/PATCH_NUMBERS.md), the runner against the twin (a whole short suite: the result files, a wrong name SKIPPED, the CPU window, the flash test, the soak's checks, a unit that
#   restarts mid-test is a FAIL with its crash record and the run goes on) and against a pseudo-terminal that speaks as the unit's console does (the serial path)
# benchaudio: tests/bench_analyze_test.py, bench suite stage 2 (tools/bench_analyze.py, tools/bench_record.py): synthetic signals with planted faults (clean sine / sweep / naive saw and square / noise / a noise hit: nothing found; a spike, a step, 5 spikes,
#   a burst: exactly the planted clicks; zero runs of 16..4410 frames and a repeated frame: exactly the planted gaps, their edges not counted as clicks; DC, clipping, silence, an abrupt end, a decaying tail), the
#   same results for any chunk size, the self-check, the tone proof (FELUCCA_UAC_TONE: a lost / repeated / zeroed frame, chunk edges), the recorder's pieces (the 44.1 kHz refusal with the setup steps, sox's command and fault lines,
#   disk hygiene: the audio of the last 5 runs and the baselines kept), apply_audio and prove_chain with a fake recorder, and the whole recorded path against the Mac twin (bench_sim --serve writes its own audio): a WAV per test,
#   the columns, planted faults in the twin's render found exactly, the same test twice bit for bit equal, --record refused at 48 kHz, --keep
#   BENCHR1 join: the suite RECORD-1 (each scene's one-core reference = `tune slack 400000`, `onecore-of`), the comparison of clicks and gaps against the reference (excess = FAIL, equal = WARN, a clean scene stays
#   strict, a reference that still got jobs is no reference, a capture fault = no comparison), the audio column beside the core 1 column of the summary, --prove-chain parsed by the merged runner
# bench: tests/cpu_bench.c (0.2.23), the LOAD BENCH (report-only): host instructions per sample of every engine's voice, INSERT type, SAT, reverb type, delay mode (DUEL too), chorus, comp, width, shimmer and a
#   worst-case scene per MODE with the estimated polyphony that fits 100 %; fails only when a figure grows more than 10 % over tests/bench_baseline.txt (BENCH_UPDATE=1 rewrites it). docs/TESTING.md "Load bench"
# roll (0.2.19): web/test_roll.mjs, the piano roll's model (step / tie rules, edit functions, undo, live writes against the simulated unit) runs with editorjson; rollbrowser: web/test_rollbrowser.mjs, the roll in headless Chrome
#   (pointer and keyboard editing, the overlay, no clipped text at 1920x1080 and 1280x720); only when Chrome is installed
# sliders (0.2.24): web/test_sliders.mjs, headless Chrome drags EVERY slider of the editor (part 1's tabs, the global column; the relabelled ENV 2 / LFO 1 / LFO 2 > CUT depth cards too): it lands, the mock unit gets the parameter, the fill bar is right (bipolar: from the zero point)
# flashled: tests/flash_led_test.c (0.2.14, from Felucca 1.6 #236): the LED rows dark during a flash erase, in order (duck, LEDs off, erase, LEDs back, unduck)
# usb: tests/usb_desc_test.c over the descriptor variants of tests/run_tests.sh (0.2.14, from Felucca 1.5.1 #67: the AC collection lists audio streaming first)
# emu: web/emu/emu_test.mjs, the browser build (Emscripten from ~/emsdk, its own node: the Homebrew one is broken) incl. a bit-for-bit check against the native build
# A test that does not compile counts as a failure (it used to be skipped silently).
# DUELL_TEST_MODE: the release default is OFF (no MENU > SYSTEM > TEST). Every firmware test below is built WITH it on (-DDUELL_TEST_MODE=1 and
# build/gen_test/duell_multis.h) so it can exercise the TEST screen; "release" is built with it off and checks it is gone. Since 0.2.23 both headers hold the same sixteen
# factory multis (tools/factory_multis.py); the CPU TEST multis of before are test scenes (tests/scenes/, tests/scenes.h).
# The default build/gen/duell_multis.h is regenerated here with the switch off (the emulator step builds the release).
# uitest: tests/ui_test.c (also the helper library the other UI tests include); its Felucca-era groups are parked under -DUI_TEST_STALE (not run)
# motion: tests/motion_test.c, its DRUM-grid / SAVE-held-undo groups are parked under -DMOTION_TEST_STALE (not run)
# modules: tests/modules_test.c, 0.2.14's GLO > MODULES (ARP SEQ SENDS PART FX): the settings bits (fresh, saved, ERASE, an older record), the page, ARP OFF's keys,
#   SEQ OFF's PLAY / Start / count-in, SENDS OFF and PART FX OFF bit for bit after their fades, PERFORM with all OFF, the faded cards and the footer's line
# wtab: tests/wtab_test.c, 0.2.14's USER WAVETABLES USR1..USR3 (wtab.c, editor_wtab.c, backup ids 24..26) on simulated NOR: the flash places (0xA0000..0xA5FFF, nothing else written), the commit record and
#   CRC, power-on, a damaged copy, a failing write, refusal while the sequencer plays, stale / malformed transfers, erase, ERASE DATA, the backup round trip
# wavetable: tests/wavetable_fixture.c (the factory tables as played and as encoded bytes: web/fixtures/wavetable_factory.json must equal a fresh run), then web/test_wavetable.mjs (the WAV converter
#   web/editor/wavetable.js: WAV parsing, FFT, saw / square / sine sweeps, a Serum-like 2048 x 256 file, a factory render converted back, the protocol client against the mock unit)
# mrender: tests/multi_render_test.c (0.2.17), every factory multi (0.2.23: M01..M16) and the 14 test scenes (tests/scenes.h: 0.2.22's multis, their 0.2.22 hashes) rendered at fader 0 / 63 / 127 and hashed (tests/multi_render_golden.txt)
# converttwin: tests/convert_twin_test.py (cloud job J): the conversion tool's table test, idempotence guard and the committed sounds and multis == its output
# ownerimport: tests/owner_import_test.py (0.2.24), tools/import_owner_edits.py on a synthetic PATCHES export made by the firmware's own code (tests/owner_export_gen.c): the 128 factory sounds saved and read back import as equal to the recipes; three turned knobs change exactly three sounds; twice = the same files; the unchanged export undoes it; a changed multi in a backup file is kept byte for byte; a changed engine and a taken name are refused; the editor's parser reads the file
# fm6: tests/fm6_test.c (the 6-operator engine; 0.2.17: the SLOT list with F9..F11 behind the bank)
# patch: tests/patch_test.c (0.2.17: ONE list of 128 patch slots, a slot = its saved record or else the factory sound at that browse position): the store (sparse records, 16 chunks, a power cut in a write, the 128-slot limit with no 'full', RAM directory), a fresh unit shows the factory sounds in all 128 slots, SAVE then reload, RESTORE bit for bit, RESTORE ALL, MIDI program change = slot, FAV by slot, a power cut at every step of a SAVE / RESTORE / RESTORE ALL, the PATCHES page and its dialogs
# bank: tests/bank_test.c (DUELL 1.0): the 128 factory patches are the owner's bank (tools/factory_bank.duell-patches.json via tools/import_bank.py, tests/factory_bank_records.txt): each factory slot loaded and saved through the firmware equals the file's record in every field (TRIM the balanced one), the arp OFF written; the arp recall (a patch restores its own arp and chord keys, OFF too; an old record without the block = OFF)
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
# pan: tests/pan_test.c (0.2.24), PAN without a zipper (fx.c pan_follow): the gain of the plain mix ramps within the block (a step of the knob or the editor: no gain jump over 1 % a sample), the spread mix steps 1/8 of what is left a block;
#   lands exactly (bit for bit the render at the new pan), a silent part takes it at once, the STEREO scenario (matrix LFO -> PAN) bit for bit as before
# eqon: tests/eqon_test.c (0.2.24; 0.2.25 layout round): the EQ without its ON / OFF switch (id 106, the retired DUCK NOTE, is a free id again, reads 0): a flat EQ runs no code, the bands work, a stored 1 changes nothing;
#   the pages (EQ 1/2 LOW FREQ, LOW GAIN, MID FREQ, MID GAIN; EQ 2/2 MID Q, HIGH FREQ, HIGH GAIN, two-line labels); the panel's EQ curve against the exact response of the same coefficients (within 0.25 dB)
# eq32: tests/eq32_test.c (0.2.25), the part EQ's loop in 32-bit multiplies (fx.c eq_run): every setting's coefficients fit int32 and cannot overflow the 64-bit sums, full scale bit for bit a 128-bit copy (no wrap),
#   against 0.2.24's 64-bit loop and the ideal filter (LSB, dB), the response against the exact RBJ curve, silence comes back to exactly 0 (no stuck DC, no limit cycle); prints the figures
# ovlguard: tests/ovl_test.c (0.2.25), the OVERLOAD GUARD (fx.c ovl_step, audio.c shed_check): the real ISR with a simulated clock forces the load: two halves over 85 % engage it, 3 s under 70 % (the meter's) release it, hysteresis,
#   single slow halves never; engaged SHIM BOTH runs as UP and RANDOM / OCT UP as the plain delay (STEREO, LAYER, DUEL), the fades' lengths, no click (1.3 x the steady steps + 50), the cost saved; built twice (-DOVL_NOGUARD: compiled out), the HASH lines of renders under the threshold must be equal
# matrixpack: tests/matrix_pack_test.c (0.2.24), the matrix stays packed (mod.c mod_compact): 200 random matrices with gaps render the same packed or not, the knob's SRC -> OFF moves the slots up, MATRIX 5-8 only with slot 4 used, a load packs
# engswitch: tests/engine_switch_test.c (0.2.24), an engine change keeps every common parameter and the slot, carries CUT RES DTN .. over, restores the engine before's eight, plays after every switch; INIT still resets all
# resetwalk: tests/reset_walk_test.c (0.2.24), the reset gesture over EVERY page and knob with a value (every engine's EDIT pages, ENV SYNC), and under a LIVE MORPH (the sounding value lands on the default)
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
#   0.2.23: then the factory multis (tools/factory_multis.py check: the sixteen records against the list, version, names, sounds, MODE, split, LOW CUT, and the multi
#   similarity check: no two multis with the same two sounds, or one sound in the same place with the same MODE and split; selftest: the check catches such pairs)
# ldrscreen: tests/ldr_test.c (the update loader against a fake host and flash) built twice, plain and with -DLDR_SCREEN_TEST=1 (the loader's screen, firmware/loader/ldr_screen.c, drawing on a fake
#   ST7789): the same checks pass both ways, the flash after the whole test hashes the same (the write is bit for bit the same with the screen on and off), the percent goes 0 -> 100 without
#   going back, the bar has the right segments at 0 / 45 / 100 percent (PPMs in build/host/ldr_screen_*.ppm), a failing or a very slow panel switches the screen off for good and the install
#   is still the package's, bit for bit. Needs two packages: LDR_OLD and LDR_NEW (default: the newest two .fwsc under build/ or the bench site)
# midimap: tests/midimap_test.c (job C, the MIDI MAP: docs/design/MIDI_MAP_DESIGN_2.md, docs/MIDI_MAP_DEFAULTS.csv, firmware/src/midi_map.h): cloud/base3's midi_control.c / midi_learn.c
#   (tests/midimap_ref_*.c) under the prefix ref_ (tests/midimap_refgen.py) and the table are fed the same CC script (every CC x 13 values x 3 channels, PARTS / SEL, SPLIT / STEREO / LAYER, every engine, with and
#   without learned CCs): identical parameter writes; the shipped map writes the same for each of today's 24 targets on its NEW number; the morph's triggers follow the target; a taken number is refused / forced; learn
#   writes a line (16 at most); the settings record at its exact size (cloud/base3's size is no record); the extension values, the effects block, map_target_info. Built twice: the tests' power-on map is the legacy list
#   (tests/midimap_legacy.csv), and once with the shipped list as the firmware's own (the two must pass); midimapgen: the generator's failures (tests/midimap_gen_test.py)
# midimapmut: the safety rules' mutations, run once each (midimap_test must FAIL): the lock check removed, two parameters allowed on one CC, the morph's trigger back on the CC number
cd "$(dirname "$0")/felucca-base" || exit 1
OUT=build/host; mkdir -p "$OUT" build/mod_demo build/ui_new build/ui_slot
unset DUELL_TEST_MODE                                  # (the environment switch is for device / emulator builds; the tests set theirs below)
CCR="cc -O2 -w -DHOST_MIX_FOLLOWS_MASTER=1 -Ibuild/gen -Ifirmware/src -Itests"      # the release configuration (switch off)
CCD="cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests"      # the device's own mix level (no HOST_MIX_FOLLOWS_MASTER): the browser build, web/emu, has none
CC="cc -O2 -w -DHOST_MIX_FOLLOWS_MASTER=1 -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests"   # test mode ON
mkdir -p build/gen_test build/gen
python3 tools/gen_duell_multis.py build/gen/duell_multis.h >/dev/null || exit 1                 # release header: M01..M16
python3 tools/gen_duell_multis.py build/gen_test/duell_multis.h --test-mode >/dev/null || exit 1   # test header: the same sixteen
python3 tools/gen_midi_map.py build/gen/midi_map_defaults.h >/dev/null || exit 1                          # the shipped MIDI map (docs/MIDI_MAP_DEFAULTS.csv): the release firmware's table
python3 tools/gen_midi_map.py --csv tests/midimap_legacy.csv build/gen_test/midi_map_defaults.h >/dev/null || exit 1   # the test builds start on today's 24 CCs (tests/midimap_legacy.csv): the old MIDI tests run unchanged
python3 tools/gen_midi_map.py --csv tests/midimap_legacy.csv --sym MIDI_MAP_LEGACY build/gen_test/midi_map_legacy.h >/dev/null || exit 1   # (tests/midimap_test.c: both lists by name)
python3 tools/gen_midi_map.py --sym MIDI_MAP_SHIPPED build/gen_test/midi_map_shipped.h >/dev/null || exit 1
python3 tools/gen_version.py build/gen/duell_version.h || exit 1   # VERSION file -> build/gen/duell_version.h
FV=$(sed -n 's/^#define DUELL_VERSION_STR "\(.*\)"/\1/p' build/gen/duell_version.h)
[ -f "$HOME/emsdk/emsdk_env.sh" ] && EMU=emu || EMU=
want="${*:-regress chord mod perform ui select step3 engine morph morphui mode dlymod dlyev hang atgt patslot multi release menuproduct patch patchraw bank dx7import dx7compat splash sounds mrender fmbass fm6 loadcost cpuscen uf wave hold reset resetwalk pan mixsplit partindep duckpart twocore tcmeas tcstage1 tcbench poolrule budget sat mxstep modinert eqon eq32 ovlguard matrixpack engswitch level guide wtab wavetable modules uitest project fun10 motion persist backup editor editorjson midictl audio erase flashled usb elfcheck reverb halltail plate locut shimmer panic insert bassmono comp master stereochorus chorusskip sidechain tune bench benchsim benchrun benchaudio ownerimport rollbrowser sliders xb xbmut fuzz xvalues twin subcyc pages scope2 editb velo midimap midimapmut ldrscreen $EMU converttwin}"
fail=0
# mxstep's PHASE self-check (hunt4): tests/mx_step_test.c built against a COPY of firmware/ and tests/ in which eng_phase.c's bridge of a moved DCW is switched off (the bend steps at
# every block seam again); the PHASE check must FAIL, a pass means it cannot see the seam
mxstep_mut() {
  echo "== mxstep_mut PHASE: the bridge of a moved DCW off (eng_phase.c phase_render_0)"
  rm -rf build/mxstep_mut; mkdir -p build/mxstep_mut
  cp -R firmware tests build/mxstep_mut/ || { echo "   FAIL (copy)"; fail=1; return; }
  f=build/mxstep_mut/firmware/src/eng_phase.c; sed 's/if (v->s\[5\] && (uint32_t)(dcw/if (0 \&\& (uint32_t)(dcw/' "$f" > "$f.new" && mv "$f.new" "$f"
  if cmp -s "$f" firmware/src/eng_phase.c; then echo "   FAIL (the mutation changed nothing)"; fail=1; return; fi
  if ! $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/mx_step_mut build/mxstep_mut/tests/mx_step_test.c -lm > $OUT/last.log 2>&1; then
    echo "   FAIL (the mutated build does not compile)"; tail -5 $OUT/last.log; fail=1; return
  fi
  if $OUT/mx_step_mut > $OUT/last.log 2>&1; then echo "   FAIL (the mutation was NOT detected)"; fail=1; else echo "   PASS (detected: $(grep -c 'FAIL' $OUT/last.log) checks failed)"; fi
}
# partindep's self-check: build tests/partindep_test.c against a COPY of firmware/ and tests/ in which one shared write has been put back (sed on the copy,
# name FILE SEDEXPR [FILE SEDEXPR ..]); the test must FAIL, a pass means it cannot see that kind of dependency
partindep_mut() {
  name=$1; shift
  echo "== partindep_mut $name"
  rm -rf build/partindep_mut; mkdir -p build/partindep_mut
  cp -R firmware tests build/partindep_mut/ || { echo "   FAIL (copy)"; fail=1; return; }
  while [ $# -ge 2 ]; do
    f=build/partindep_mut/$1; sed "$2" "$f" > "$f.new" && mv "$f.new" "$f"
    if cmp -s "$f" "$1"; then echo "   FAIL (the mutation changed nothing in $1)"; fail=1; return; fi
    shift 2
  done
  if ! $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/partindep_mut build/partindep_mut/tests/partindep_test.c -lm > $OUT/last.log 2>&1; then
    echo "   FAIL (the mutated build does not compile)"; tail -5 $OUT/last.log; fail=1; return
  fi
  if $OUT/partindep_mut > $OUT/last.log 2>&1; then echo "   FAIL (the mutation was NOT detected)"; fail=1; else echo "   PASS (detected: $(grep -c 'partindep: FAIL' $OUT/last.log) checks failed)"; fi
}
# twocore's self-check: the same, for tests/twocore_sim.c (threads): a copy of the firmware with ONE shared write put back must make the simulation FAIL
twocore_mut() {
  name=$1; shift
  echo "== twocore_mut $name"
  rm -rf build/twocore_mut; mkdir -p build/twocore_mut
  cp -R firmware tests build/twocore_mut/ || { echo "   FAIL (copy)"; fail=1; return; }
  while [ $# -ge 2 ]; do
    f=build/twocore_mut/$1; sed "$2" "$f" > "$f.new" && mv "$f.new" "$f"
    if cmp -s "$f" "$1"; then echo "   FAIL (the mutation changed nothing in $1)"; fail=1; return; fi
    shift 2
  done
  if ! $CC -O1 $MUTFLAGS "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/twocore_mut build/twocore_mut/tests/twocore_sim.c -lm -lpthread > $OUT/last.log 2>&1; then
    echo "   FAIL (the mutated build does not compile)"; tail -5 $OUT/last.log; fail=1; return
  fi
  # two runs: the ownership audit over ALL nine scenes (no race: core 1's job must change nothing of part 1's state; catches the shared-state mutants every time), then
  # scene 0 with every fault (the output compare; catches the runner mutants). The mutation must fail at least one of them.
  if $OUT/twocore_mut -audit > $OUT/last.log 2>&1; then
    if $OUT/twocore_mut 0 -nomut > $OUT/last.log 2>&1; then echo "   FAIL (the mutation was NOT detected)"; fail=1; else echo "   PASS (detected by the scene-0 faults: $(grep -c 'twocore: FAIL' $OUT/last.log) checks failed)"; fi
  else echo "   PASS (detected by the ownership audit: $(grep -c 'twocore: FAIL' $OUT/last.log) checks failed)"; fi
}
run() { echo "== $1"; shift; "$@" > "$OUT/last.log" 2>&1 && echo "   PASS" || { echo "   FAIL"; tail -15 "$OUT/last.log"; fail=1; }; }
for t in $want; do case $t in
  regress) $CC -DNPART=4 -o $OUT/regress tests/regress.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run regress $OUT/regress tests/golden.txt tests/cpu_baseline.txt ;;
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
  chorusskip) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/chorusskip_test tests/chorusskip_test.c -lm && $CC -O1 -DCHO_NOSKIP "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/chorusskip_ref tests/chorusskip_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } &&
           run chorusskip sh -c "$OUT/chorusskip_test > $OUT/cs_skip.txt && $OUT/chorusskip_ref > $OUT/cs_ref.txt && grep HASH $OUT/cs_skip.txt > $OUT/cs_skip.h && grep HASH $OUT/cs_ref.txt > $OUT/cs_ref.h && cmp $OUT/cs_skip.h $OUT/cs_ref.h && tail -3 $OUT/cs_skip.txt" ;;
  stereochorus)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/stereochorus_test tests/stereochorus_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run stereochorus $OUT/stereochorus_test ;;
  dlyev)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/dlyev_test tests/dlyev_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run dlyev $OUT/dlyev_test ;;
  mode)    $CC "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/mode_test tests/mode_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run mode $OUT/mode_test ;;
  hang)    $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/hang_test tests/hang_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run hang $OUT/hang_test ;;
  atgt)    $CC -o $OUT/atgt_test tests/atgt_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run atgt $OUT/atgt_test ;;
  patslot) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/patslot_test tests/patslot_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run patslot $OUT/patslot_test ;;
  sounds)  $CC -o $OUT/sound_audit tests/sound_audit.c -lm || { echo "== sounds"; echo "   FAIL (does not compile)"; fail=1; false; } && { echo "== sounds"; $OUT/sound_audit || fail=1; python3 tools/factory_multis.py check || fail=1; python3 tools/factory_multis.py selftest || fail=1; } ;;
  release) $CCR -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/release_test tests/release_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run release $OUT/release_test ;;
  menuproduct) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/menu_product_test tests/menu_product_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run menuproduct $OUT/menu_product_test ;;
  bank)    $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/bank_test tests/bank_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run bank $OUT/bank_test tests/factory_bank_records.txt ;;
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
  ownerimport) run ownerimport python3 tests/owner_import_test.py ;;
  converttwin) run converttwin python3 -I tests/convert_twin_test.py ;;   # cloud job J: tools/convert_twin.py against the 43 rows of docs/design/TWIN_FILTER_DESIGN_2.md 1.5, the committed data == the tool's output, the guards
  mrender) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/multi_render_test tests/multi_render_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run mrender $OUT/multi_render_test tests/multi_render_golden.txt ;;
  uf)      $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/uf_test tests/uf_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run uf $OUT/uf_test ;;
  level)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/level_test tests/level_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run level $OUT/level_test ;;
  reset)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/reset_test tests/reset_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run reset $OUT/reset_test ;;
  pan)     $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/pan_test tests/pan_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run pan $OUT/pan_test ;;
  mixsplit) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/mixsplit_test tests/mixsplit_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run mixsplit $OUT/mixsplit_test ;;
  partindep) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/partindep_test tests/partindep_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run partindep $OUT/partindep_test &&
             partindep_mut "the parts share one block scratch (fx.c part_voice)" firmware/src/fx.c 's/int32_t \*b = pscr\[PART_OF(t)\]\.buf;/int32_t *b = pscr[0].buf;/' &&
             partindep_mut "the parts share one morph window (morph.c)" firmware/src/morph.c 's/t->mwon/trk[0].mwon/g; s/t->mwpi/trk[0].mwpi/g' &&
             partindep_mut "the parts share one matrix window (mod.c MODW)" firmware/src/core.h 's/#define MODW(t) ((t)->mdw)/#define MODW(t) (trk[0].mdw)/' ;;
  duckpart) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/duckpart_test tests/duckpart_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run duckpart $OUT/duckpart_test ;;
  twocore) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/twocore_sim tests/twocore_sim.c -lm -lpthread || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run twocore $OUT/twocore_sim &&
             twocore_mut "the parts share one block scratch (fx.c part_voice)" firmware/src/fx.c 's/int32_t \*b = pscr\[PART_OF(t)\]\.buf;/int32_t *b = pscr[0].buf;/' &&
             twocore_mut "the parts share one matrix window (core.h MODW)" firmware/src/core.h 's/#define MODW(t) ((t)->mdw)/#define MODW(t) (trk[0].mdw)/' &&
             twocore_mut "the parts share the envelope's ENV SYNC steps (voice.c)" firmware/src/voice.c 's/((track_t \*)t)->esk)/trk[0].esk)/' &&
             twocore_mut "mix_gather reads part 2 while it is silent (twocore_run.c's skip)" firmware/src/fx.c 's/if (p == 1u \&\& tcr.skip2)/if (0)/' &&
             twocore_mut "a stopped core 1's voices are not faded out (twocore_run.c fade2)" firmware/src/twocore_run.c 's/tcr.fade2 = 1;/tcr.fade2 = 0;/' &&
             twocore_mut "BENCHT4: a very late block whose job finished pauses again (twocore_run.c TCR_VLATE_RUN 1)" firmware/src/twocore_run.c 's/#define TCR_VLATE_RUN 0u/#define TCR_VLATE_RUN 1u/' &&
             twocore_mut "BENCHT4: the window rule is on by default again (twocore_run.c TCR_VWIN_K 8)" firmware/src/twocore_run.c 's/#define TCR_VWIN_K 0u/#define TCR_VWIN_K 8u/' &&
             twocore_mut "BENCHT4: a real fault (a job that gave no answer to the take) no longer pauses the jobs (twocore_run.c)" firmware/src/twocore_run.c 's|tcr_pause(TC_TICKS(), TCR_PW_FAULT); /\* \[no answer\] \*/||' &&
             twocore_mut "BENCHT4: no real fault pauses the jobs (twocore_run.c tcr_pause)" firmware/src/twocore_run.c 's/    if (!tcr.paused) {/    if (!tcr.paused \&\& why != TCR_PW_FAULT) {/' &&
             twocore_mut "BENCHT4: a job not finished by its wait limit (a silent block) no longer pauses the jobs (twocore_run.c)" firmware/src/twocore_run.c 's|tcr_pause(TC_TICKS(), TCR_PW_FAULT); /\* \[silent\] \*/||' &&
             twocore_mut "BENCHT4: core 0 waits for core 1 to the end of the half, not to the wait limit (twocore_run.c tcr_part2)" firmware/src/twocore_run.c 's/a, lim = tcr.lim, t1;/a, lim = tcr.t0 + TCR_HALF_TICKS, t1;/' ;;
  twocore_tsan) $CC -O1 -g -fsanitize=thread "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/twocore_tsan tests/twocore_sim.c -lm -lpthread || { echo "== compile"; echo "   FAIL (does not compile; ThreadSanitizer missing?)"; fail=1; false; } && run twocore_tsan sh -c "for s in \${TSAN_SCENES:-0 8}; do TSAN_OPTIONS=halt_on_error=1 $OUT/twocore_tsan \$s -tsan || exit 1; done" ;;
  tcmeas) $CC -O1 -DTC_MEAS=1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/twocore_meas tests/twocore_sim.c -lm -lpthread || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run tcmeas $OUT/twocore_meas;
             MUTFLAGS=-DTC_MEAS=1
             twocore_mut "BENCHT2: core 1 stamps the end of a job before it ran it (twocore.h t_end)" firmware/src/twocore.h 's/te = TC_TICKS();/te = ts;/';
             twocore_mut "BENCHT2: the finish margin has the wrong sign (twocore_run.c tcm_collect)" firmware/src/twocore_run.c 's/mg = (int32_t)(tcr.dl - te);/mg = (int32_t)(te - tcr.dl);/';
             twocore_mut "BENCHT2: a deadline already past is counted as paused (twocore_run.c own_why)" firmware/src/twocore_run.c 's/tcr.own_why = TCM_OWN_DL;/tcr.own_why = TCM_OWN_PAUSE;/';
             twocore_mut "BENCHT2: the block of a very late job is not counted (twocore_run.c vblk)" firmware/src/twocore_run.c 's/tcm.vblk\[tcj.blk % TCR_BLOCKS\]++;//';
             twocore_mut "BENCHT2: the clear request is ignored (twocore_run.c tcm.clr)" firmware/src/twocore_run.c 's/if (tcm.clr)/if (0)/';
             twocore_mut "BENCHT3: the deadline's floor removed (twocore_run.c tcr_deadline)" firmware/src/twocore_run.c 's/if (TCR_FLOOR_NOW) {/if (0) {/';
             twocore_mut "BENCHT4: the run rule (vrun 4, switched on) is ignored (twocore_run.c tcr_vlate_done)" firmware/src/twocore_run.c 's/if (TCR_VLATE_NOW \&\& ++tcr.vlate_run >= TCR_VLATE_NOW)/if (0)/';
             twocore_mut "BENCHT4: the window rule (vwin 8, switched on) is ignored (twocore_run.c tcr_vlate_done)" firmware/src/twocore_run.c 's/else if (win)/else if (0)/';
             twocore_mut "BENCHT3: the floor never says 'does not fit' (twocore_run.c nofit)" firmware/src/twocore_run.c 's/\*nofit = 1;/*nofit = 0;/';
             twocore_mut "BENCHT3: the floor ignores the cap (twocore_run.c: can be later than core 0's own latest moment)" firmware/src/twocore_run.c 's/dl = (int32_t)(tgt - cap) > 0 ? cap : tgt;/dl = tgt;/'; MUTFLAGS= ;;
  tcbench) $CC -O1 -o $OUT/twocore_bench_test tests/twocore_bench_test.c || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run tcbench $OUT/twocore_bench_test ;;
  poolrule) run poolrule python3 tests/pool_rule_test.py ;;
  tcstage1) $CC -O1 -o $OUT/twocore_stage1_test tests/twocore_stage1_test.c || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run tcstage1 $OUT/twocore_stage1_test ;;
  budget)  $CC -O1 -fno-builtin -D_FORTIFY_SOURCE=0 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/budget_test tests/budget_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run budget $OUT/budget_test ;;
  sat)     $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/sat_test tests/sat_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run sat $OUT/sat_test ;;
  mxstep)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/mx_step_test tests/mx_step_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run mxstep $OUT/mx_step_test &&
           mxstep_mut ;;
  modinert) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/mod_inert_test tests/mod_inert_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run modinert $OUT/mod_inert_test ;;
  eqon)    $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/eqon_test tests/eqon_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run eqon $OUT/eqon_test ;;
  eq32)    $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/eq32_test tests/eq32_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run eq32 sh -c "$OUT/eq32_test > $OUT/eq32.txt; r=\$?; grep -v ' ok ' $OUT/eq32.txt | grep -v 'eq32: ok' ; exit \$r" ;;
  ovlguard) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/ovl_test tests/ovl_test.c -lm && $CC -O1 -DOVL_NOGUARD "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/ovl_ref tests/ovl_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } &&
           run ovlguard sh -c "$OUT/ovl_test > $OUT/ovl_test.txt && $OUT/ovl_ref > $OUT/ovl_ref.txt && grep HASH $OUT/ovl_test.txt > $OUT/ovl_test.h && grep HASH $OUT/ovl_ref.txt > $OUT/ovl_ref.h && cmp $OUT/ovl_test.h $OUT/ovl_ref.h && grep -v ' ok\$' $OUT/ovl_test.txt | grep -v '^HASH'" ;;
  matrixpack) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/matrix_pack_test tests/matrix_pack_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run matrixpack $OUT/matrix_pack_test ;;
  engswitch) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/engine_switch_test tests/engine_switch_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run engswitch $OUT/engine_switch_test ;;
  resetwalk) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/reset_walk_test tests/reset_walk_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run resetwalk $OUT/reset_walk_test ;;
  modules) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/modules_test tests/modules_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run modules $OUT/modules_test ;;
  guide)   $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/guide_test tests/guide_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run guide $OUT/guide_test ;;
  reverb)  mkdir -p build/fx_demo build/reverb_demo; $CC -O2 -o $OUT/reverb_test tests/reverb_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run reverb $OUT/reverb_test build/fx_demo build/reverb_demo ;;
  bassmono) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/bassmono_test tests/bassmono_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run bassmono $OUT/bassmono_test ;;
  sidechain) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/sidechain_test tests/sidechain_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run sidechain $OUT/sidechain_test ;;
  tune)    $CC -O1 -o $OUT/tune_test tests/tune_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run tune $OUT/tune_test ;;
  benchsim) $CC -O1 -fno-builtin -D_FORTIFY_SOURCE=0 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/bench_sim tests/bench_sim.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } &&
           run benchsim sh -c "BENCH_SIM_LOG=$OUT/bench_sim.log $OUT/bench_sim && python3 tools/bench_log.py --from $OUT/bench_sim.log --out $OUT/bench_sim.csv" ;;
  benchrun) $CC -O1 -fno-builtin -D_FORTIFY_SOURCE=0 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/bench_sim tests/bench_sim.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } &&
           run benchrun python3 tests/bench_run_test.py ;;
  benchaudio) $CC -O1 -fno-builtin -D_FORTIFY_SOURCE=0 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/bench_sim tests/bench_sim.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } &&
           PYN=$(for p in python3 /opt/homebrew/bin/python3 /usr/local/bin/python3 /Library/Frameworks/Python.framework/Versions/Current/bin/python3; do "$p" -c 'import numpy' 2>/dev/null && { command -v "$p" || echo "$p"; break; }; done)
           if [ -n "$PYN" ]; then run benchaudio "$PYN" tests/bench_analyze_test.py; else run benchaudio echo "skipped: no python3 with numpy on this machine (the DUELL venv has none; tools/bench_analyze.py needs it)"; fi ;;
  comp) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/comp_test tests/comp_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run comp $OUT/comp_test ;;
  bench) $CC -O1 -fno-builtin -D_FORTIFY_SOURCE=0 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/cpu_bench tests/cpu_bench.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } &&
         { echo "== bench (report only: fails when a figure grows > 10 % over tests/bench_baseline.txt)"; $OUT/cpu_bench > $OUT/bench.log 2>&1 && { sed 's/^bench: /   /' $OUT/bench.log; echo "   PASS"; } || { echo "   FAIL"; grep -i 'fail' $OUT/bench.log; fail=1; }; } ;;
  master) $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/master_test tests/master_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run master $OUT/master_test ;;
  shimmer) $CC -O2 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/shimmer_test tests/shimmer_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run shimmer $OUT/shimmer_test ;;
  locut)   $CC -O2 -o $OUT/locut_test tests/locut_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run locut $OUT/locut_test ;;
  halltail) $CC -O2 -o $OUT/hall_tail_test tests/hall_tail_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run halltail $OUT/hall_tail_test ;;
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
             $OUT/descdump > $OUT/desc.json && FELUCCA_DESC=$OUT/desc.json $NODE web/test_layout.mjs && $NODE web/test_editor.mjs && $NODE web/test_editor_0225.mjs && $NODE web/test_duellbackup.mjs && $NODE web/test_librarian.mjs && $NODE web/test_roll.mjs" ;;
  rollbrowser) NODE=node; for n in "$HOME"/emsdk/node/*/bin/node; do [ -x "$n" ] && NODE=$n; done
           run rollbrowser "$NODE" web/test_rollbrowser.mjs ;;   # (skips itself when no Chrome is installed)
  sliders) NODE=node; for n in "$HOME"/emsdk/node/*/bin/node; do [ -x "$n" ] && NODE=$n; done
           run sliders "$NODE" web/test_sliders.mjs ;;   # (0.2.24: every slider of the editor dragged in headless Chrome; skips itself when no Chrome is installed)
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
  emu)     { . "$HOME/emsdk/emsdk_env.sh" >/dev/null 2>&1; sh web/emu/build.sh >"$OUT/emu_build.log" 2>&1 && $CCD -ffp-contract=off -o build/emu/native_check web/emu/native_check.c -lm && build/emu/native_check build/emu/native.f32; } || { echo "== emu"; echo "   FAIL (build)"; tail -5 "$OUT/emu_build.log"; fail=1; false; } && run emu "$EMSDK_NODE" web/emu/emu_test.mjs build/emu/felucca.wasm build/emu/native.f32 ;;
  # ---- the cloud plan's job A (the store): the extension block, the 22 chunks, the velocity store (tests/xb_test.c; patch, patchraw, backup, multi, project, persist, erase, fuzz_ed and fuzz_proj are extended in place)
  xb)      awk '/^#define FL_/{p=1} p{print} p&&!/\\$/{p=0}' firmware/hal/fm1_flash.h | grep -v 'FL_FAR\|FL_XIP' > $OUT/fl_macros.h     # (the real FL_* range macros of the device header)
           if cc -O1 -g -w -fsanitize=address,undefined -I$OUT -Ifirmware/src -Itests -o $OUT/xb_test tests/xb_test.c 2>/dev/null; then :; else echo "   (no AddressSanitizer here: the bounds checks run without it)"; $CC -O1 -I$OUT -o $OUT/xb_test tests/xb_test.c || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; }; fi && run xb $OUT/xb_test ;;
  xbmut)   xb_mut() {   # xb_mut "name" test.c file 'old' 'new' [file 'old' 'new' ...]: the safety rule broken in a copy of the firmware; the test must FAIL (a build that stops on a _Static_assert counts, said so)
             name=$1; tsrc=$2; shift 2
             echo "== xbmut $name"
             awk '/^#define FL_/{p=1} p{print} p&&!/\\$/{p=0}' firmware/hal/fm1_flash.h | grep -v 'FL_FAR\|FL_XIP' > $OUT/fl_macros.h
             rm -rf build/xb_mut; mkdir -p build/xb_mut
             cp -R firmware tests build/xb_mut/ || { echo "   FAIL (copy)"; fail=1; return; }
             while [ $# -ge 3 ]; do
               f=build/xb_mut/$1
               python3 -I -c 'import sys; p,a,b=sys.argv[1:4]; s=open(p).read(); assert s.count(a)==1, "pattern not found once"; open(p,"w").write(s.replace(a,b))' "$f" "$2" "$3" || { echo "   FAIL (the mutation does not apply to $1)"; fail=1; return; }
               shift 3
             done
             if ! $CC -O1 -I$OUT "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/xb_mut_t build/xb_mut/tests/$tsrc -lm > $OUT/last.log 2>&1; then
               if grep -q "static assertion failed" $OUT/last.log; then echo "   PASS (detected at build time: $(grep -m1 'static assertion failed' $OUT/last.log | sed 's/.*static assertion failed: //'))"; else echo "   FAIL (the mutated build does not compile)"; tail -5 $OUT/last.log; fail=1; fi
               return
             fi
             if $OUT/xb_mut_t > $OUT/last.log 2>&1; then echo "   FAIL (the mutation was NOT detected)"; fail=1; else echo "   PASS (detected: $(grep -c ' FAIL$' $OUT/last.log) checks failed)"; fi
           }
           xb_mut "the commit record is programmed BEFORE the payload (storage.c st_save_to_x)" xb_test.c firmware/src/storage.c \
              '    for (off = 0; off < len; off += 256u) {
        uint32_t n = len - off > 256u ? 256u : len - off;
        if ((rc = st_prog(base + ST_PAYLOAD_OFF + off, st_buf + off, n)) != 0)' \
              '    {
        st_hdr_t m0;
        m0.magic = ST_MAGIC; m0.type = (uint16_t)obj; m0.slot = (uint16_t)to; m0.seq = seq + 1u; m0.len = len; m0.crc = st_crc32(st_buf, len);
        m0.rsv[0] = m0.rsv[1] = 0xFFFFFFFFu; m0.hcrc = st_crc32(&m0, sizeof m0 - 4u);
        if ((rc = st_prog(base, &m0, sizeof m0)) != 0)
            return rc;
    }
    for (off = 0; off < len; off += 256u) {
        uint32_t n = len - off > 256u ? 256u : len - off;
        if ((rc = st_prog(base + ST_PAYLOAD_OFF + off, st_buf + off, n)) != 0)'
           xb_mut "PT_K 7 records a chunk, the _Static_assert still in place (upreset.c)" patch_test.c firmware/src/upreset.c '#define PT_K ((PT_CAP - PT_HDR) / PT_WORST)' '#define PT_K 7u'
           xb_mut "PT_K 7 records a chunk with the _Static_assert taken out: the 128-slot guarantee must break at run time (upreset.c)" patch_test.c firmware/src/upreset.c '#define PT_K ((PT_CAP - PT_HDR) / PT_WORST)' '#define PT_K 7u' \
              firmware/src/upreset.c '_Static_assert(PT_K == 6u && PT_WORST * PT_K + PT_HDR <= PT_CAP && PT_MAX >= UP_SLOTS' '_Static_assert(PT_MAX >= UP_SLOTS'
           xb_mut "the extension block's length check removed (upreset.c pt_rec_ok)" patch_test.c firmware/src/upreset.c '    if ((p[4] & PTF_XB) && xb_block_ok(p + a, rl - a) != 1u + p[a])' '    if (0)'
           xb_mut "PATCH_RAW's length check removed (editor_patchraw.c BEGIN: PT_FIX .. PT_WORST)" patchraw_test.c firmware/src/editor_patchraw.c 'if (n != 9u || a[8] > 15u || len < PT_FIX || len > PT_WORST)' 'if (n != 9u || a[8] > 15u)' ;;
  fuzz)    SAN=; cc -fsanitize=address,undefined -fno-sanitize=shift -w -x c -o /dev/null - 2>/dev/null <<'EOF2' && SAN="-fsanitize=address,undefined -fno-sanitize=shift"
int main(void) { return 0; }
EOF2
           [ -n "$SAN" ] || echo "   (no AddressSanitizer here: the fuzzers run without it)"
           # (the editor SysEx / patch record fuzzer and the project / multi store fuzzer; the negative left shifts UBSan finds in fx.c's audio code are old and left out of the check)
           cc -O1 -g -w $SAN -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/fuzz_ed tests/fuzz_ed.c -lm && cc -O1 -g -w $SAN -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/fuzz_proj tests/fuzz_proj.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } &&
           { ASAN_OPTIONS=detect_leaks=0 run "fuzz_ed: SysEx, raw packets, patch record sessions (60000, seed 7)" $OUT/fuzz_ed 60000 7
             ASAN_OPTIONS=detect_leaks=0 run "fuzz_ed: only patch record sessions through PATCH_RAW and the backup's object 40 (30000, seed 11)" $OUT/fuzz_ed 30000 11 1
             ASAN_OPTIONS=detect_leaks=0 run "fuzz_proj: project stores and multi records with the slots and the edit list (8000, seed 13)" $OUT/fuzz_proj 8000 13; } ;;
  xvalues) # cloud job B (the values, the extension slots, the chain stub): tests/xvalues_test.c at the default XH and at XH 3; the loader: tools/gen_duell_sounds.py makes a header from the factory list plus one sound that names slots (build/gen_xv), the test reads it back
           echo "== xvalues"; xb=0
           mkdir -p build/gen_xv
           python3 -I - <<'PY' || { echo "   FAIL (loader: gen_duell_sounds)"; xb=1; }
import importlib.util, sys, types
sys.path.insert(0, "tools")
g = importlib.util.spec_from_file_location("gen", "tools/gen_duell_sounds.py"); gen = importlib.util.module_from_spec(g); g.loader.exec_module(gen)
d = gen.load_data("tools/duell_sounds.py")
n = 1 + max(s["idx"] for s in d.SOUNDS if s["engine"] == "ANALOG")
mk = lambda p: types.SimpleNamespace(NO_OWNER=True, SOUNDS=list(d.SOUNDS) + [{"engine": "ANALOG", "idx": n, "name": "XLOADTEST", "cat": "BASS", "p": p}])
sounds, lst, engines, cats, idname = gen.build(mk({"SUB LEVEL": 80, "CYCLE RISE": 33, "TWIN LINK": "OFF", "CYCLE MODE": "ONCE"}))
x = [s for s in sounds if s["name"] == "XLOADTEST"][0]
assert dict(x["pairs"])[128 + 6] == 80 and dict(x["pairs"])[128 + 11] == 33 and dict(x["pairs"])[128 + 1] == 0 and dict(x["pairs"])[128 + 10] == 1, x["pairs"]
gen.emit("build/gen_xv/duell_sounds.h", sounds, lst, engines, cats, idname)
for bad in ({"SUB LEVEL": 128}, {"SUB NOPE": 1}, {"CYCLE MODE": "SPIN"}):      # out of range / unknown / unknown name: the generator refuses
    try:
        gen.build(mk(bad)); sys.exit("accepted " + repr(bad))
    except SystemExit as e:
        assert e.code not in (0, None) and "accepted" not in str(e.code), e.code
print("xvalues: loader: the generator takes slot labels (SUB LEVEL, CYCLE RISE, TWIN LINK, CYCLE MODE) and refuses a bad one")
PY
           for xh in 3; do   # (XH 3 = MORPH_HIST; XH 1 compiles but the rebase check expects two ring entries: not run)
               $CC -O1 -DXH=$xh "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/xvalues_test_$xh tests/xvalues_test.c -lm && $OUT/xvalues_test_$xh > "$OUT/last.log" 2>&1 || { echo "   FAIL (XH $xh)"; grep -v " ok$" "$OUT/last.log" | tail -25; xb=1; }
           done
           cc -Ibuild/gen_xv ${CC#cc } -O1 -DXV_LOADER "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/xvalues_loader tests/xvalues_test.c -lm && $OUT/xvalues_loader > "$OUT/last.log" 2>&1 || { echo "   FAIL (loader)"; grep -v " ok$" "$OUT/last.log" | tail -25; xb=1; }
           [ $xb = 0 ] && echo "   PASS" || fail=1 ;;
  twin) # cloud job E (the TWIN FILTER's filter 2 and DRIVE): tests/twin_test.c (built with -DDUELL_TWIN_COUNT: chain_f2.c counts the runs of DRIVE); the partindep and two-core scenes run again with SCN_TWIN=1 (both parts play with filter 2 on: every
        # schedule of the stages, and the second core, must give the one-core output); then one mutation per safety rule, each copy of firmware/ and tests/ must make twin_test FAIL: DRIVE runs at 0, LADDER's tanh removed, the TYPE cross-fade removed, chain_on true at TYPE OFF
           tb=0
           $CC -O1 -DDUELL_TWIN_COUNT "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/twin_test tests/twin_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; tb=1; }
           [ $tb = 0 ] && run twin $OUT/twin_test
           $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/partindep_test tests/partindep_test.c -lm && $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/twocore_sim tests/twocore_sim.c -lm -lpthread || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; tb=1; }
           [ $tb = 0 ] && run twin_partindep env SCN_TWIN=1 $OUT/partindep_test
           [ $tb = 0 ] && run twin_twocore env SCN_TWIN=1 $OUT/twocore_sim
           for m in 1 2 3 4; do
             case $m in
               1) mn="DRIVE runs at 0 (chain_f2.c: the shaper is called whatever DRIVE is)"; mf=firmware/src/chain_f2.c; ms='s/if (q\.d0 | q\.d1)/if (1)/g' ;;
               2) mn="LADDER's tanh removed (chain_f2.c f2_lad: the sum enters the cascade unsaturated)"; mf=firmware/src/chain_f2.c; ms='s/int32_t u = softclip((in\[i\] >> 1) - ((kf \* (y4 >> 6)) >> 12)) << 6,/int32_t u = ((in[i] >> 1) - ((kf * (y4 >> 6)) >> 12)) << 6,/' ;;
               3) mn="the TYPE cross-fade removed (chain_f2.c: the new type at once)"; mf=firmware/src/chain_f2.c; ms='s/b\[i\] = a\[i\] + (((b\[i\] - a\[i\]) \* (int32_t)(base + i + 1u)) >> 8);/b[i] = b[i] + 0 * base;/' ;;
               4) mn="chain_on true at TYPE OFF (chain_f2.c chain_f2_on)"; mf=firmware/src/chain_f2.c; ms='s/return v->chn == VCH_RUN \&\& (s->ty || s->fade);/return 1;/' ;;
             esac
             echo "== twin_mut $mn"
             rm -rf build/twin_mut; mkdir -p build/twin_mut
             cp -R firmware tests build/twin_mut/ || { echo "   FAIL (copy)"; fail=1; continue; }
             f=build/twin_mut/$mf; sed "$ms" "$f" > "$f.new" && mv "$f.new" "$f"
             if cmp -s "$f" "$mf"; then echo "   FAIL (the mutation changed nothing in $mf)"; fail=1; continue; fi
             if ! $CC -O1 -DDUELL_TWIN_COUNT "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/twin_mut build/twin_mut/tests/twin_test.c -lm > $OUT/last.log 2>&1; then
               echo "   FAIL (the mutated build does not compile)"; tail -5 $OUT/last.log; fail=1; continue
             fi
             if $OUT/twin_mut > $OUT/last.log 2>&1; then echo "   FAIL (the mutation was NOT detected)"; fail=1; else echo "   PASS (detected: $(grep -c ' FAIL' $OUT/last.log) checks failed)"; fi
           done ;;
  # ---- cloud job F: the SUB LAYER (chain_sub.c), the CYCLE envelope (cycle.c) and WAVE STEPS (eng_wave.c wave_steps); docs/design/SUB_AND_CYCLE_DESIGN.md. The engine's side of STEPS (places, hysteresis, cross-fade, morph) is in wave, the
  # routes' step tests in mxstep, the DUEL / LAYER / STEREO scenes with all three on both parts in partindep (scenes 9..11) and twocore (the same scenes: the two-core runs equal the one-core output)
  subcyc)  # tests/subcyc_test.c: REF (bit for bit cloud/base3-wave2: tests/subcyc_ref.h, SUB LEVEL 0, CYCLE unread, STEPS OFF), SUB (frequency, spectrum, level curve, phase, clicks, UNISON, glide and bend, FM6 follow,
           # aliasing report, NOTES rows), CYCLE (model, times, SYNC, retrigger, legato, per voice, read flag). Then the four safety rules broken in a copy of the firmware: each run must FAIL
           echo "== subcyc"; sc=0
           $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/subcyc_test tests/subcyc_test.c -lm && $OUT/subcyc_test > "$OUT/last.log" 2>&1 || { echo "   FAIL"; grep -v " ok$" "$OUT/last.log" | tail -25; sc=1; }
           grep "REPORT" "$OUT/last.log" | cut -c1-300
           sc_mut() {   # sc_mut "name" test.c file 'old' 'new' [file 'old' 'new' ..]: the safety rule broken in a copy of the firmware; the test must FAIL
             name=$1; tsrc=$2; shift 2
             echo "== subcyc mutation: $name"
             rm -rf build/sc_mut; mkdir -p build/sc_mut
             cp -R firmware tests build/sc_mut/ || { echo "   FAIL (copy)"; sc=1; return; }
             while [ $# -ge 3 ]; do
               f=build/sc_mut/$1
               python3 -I -c 'import sys; p,a,b=sys.argv[1:4]; s=open(p).read(); assert s.count(a)==1, "pattern not found once"; open(p,"w").write(s.replace(a,b))' "$f" "$2" "$3" || { echo "   FAIL (the mutation does not apply to $1)"; sc=1; return; }
               shift 3
             done
             if ! $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/sc_mut_t build/sc_mut/tests/$tsrc -lm > $OUT/last.log 2>&1; then echo "   FAIL (the mutated build does not compile)"; tail -5 $OUT/last.log; sc=1; return; fi
             if $OUT/sc_mut_t > $OUT/last.log 2>&1; then echo "   FAIL (the mutation was NOT detected)"; sc=1; else echo "   PASS (detected: $(grep -c ' FAIL$' $OUT/last.log) checks failed)"; fi
           }
           sc_mut "the SUB runs at LEVEL 0 (chain_sub.c chain_sub_on)" subcyc_test.c firmware/src/chain_sub.c 'return XV(t, X_SUBLVL) > 0 && (t->p[P_VOICE] != V_UNISON || v == t->v);' 'return t->p[P_VOICE] != V_UNISON || v == t->v;'
           sc_mut "the 4-block cross-fade of a WAVE / OCTAVE switch removed (chain_sub.c)" subcyc_test.c firmware/src/chain_sub.c 'c = g0 ? SUB_XFB : 0;' 'c = 0;'
           sc_mut "the CYCLE tick moved into mod_voice (hot RAM): ticked whenever the matrix is on, read or not (mod.c, voice.c)" subcyc_test.c \
              firmware/src/mod.c 'static __attribute__((noinline)) void mod_voice(track_t *t, voice_t *v, vmod_t *m, int32_t fine)' 'static void cycle_tick(track_t *t, voice_t *v);
static __attribute__((noinline)) void mod_voice(track_t *t, voice_t *v, vmod_t *m, int32_t fine)' \
              firmware/src/mod.c 'int32_t pit = mod->pit, cut = mod->cut, shp = mod->shp, gain = mod->gain, env = m->envq15;' 'int32_t pit = mod->pit, cut = mod->cut, shp = mod->shp, gain = mod->gain, env = m->envq15;
    cycle_tick(t, v);' \
              firmware/src/voice.c '            if (t->cy)                                  /* CYCLE (cycle.c), read by an active matrix slot: ticked here, not in mod_voice (hot RAM) */
                cycle_tick(t, v);
' ''
           sc_mut "STEPS' hysteresis removed (eng_wave.c wave_steps: the nearest place every block)" wave_test.c firmware/src/eng_wave.c 'if (wv_stn[ti][vi] != n1 || 4 * (d < 0 ? -d : d) > 3 * (127 << 8)) {' 'if (1) {'
           [ $sc = 0 ] && echo "   PASS" || fail=1 ;;
  # ---- cloud job H: the pages of the TWIN FILTER (TWIN FILTER, TWIN MOD, TWIN CHAIN), SUB OSC and CYCLE ENV 1/2, 2/2, the graphs of both filters, QUICK MULTI's four cards (docs/design/FILTER_PAGES_PROPOSAL.md Option B,
  # SUB_AND_CYCLE_DESIGN.md A4 / B4 / C3; the owner's answers of 2026-10-19). tests/pages_test.c: (a) every label and value of every new card in every state against the unit's font table (labels 50 px, values 49 px) and the
  # titles in the header, (b) the pages: skipped while TYPE is OFF, the footer's count, the fall back when the page vanishes (a write, a multi load, INIT SOUND, the knob), COMB not offered on WAVE and PHYS, LINK and TYPE through
  # twin_link_set / twin_type_set, (c) which cards show and where (hide-inert, moved left), the centre-filled gauges, DRIVE OFF, BLEND's ends, WAVE's STEPS card, (d) QUICK MULTI for filter 2 OFF / LINKED / UNLINKED / COMB, (e) the
  # matrix names, (f) the pictures, (g) the SUB OSC and CYCLE ENV cards. (The layout lint over the new pages in both shapes of filter 2 is in `ui`: tests/ui_render.c twin_pass.) The existing UI renders: tests/pages_fp.c draws every
  # page of every family on every engine and the QUICK layers in four parts (header, cards, panel, footer) and hashes them; the base (git ref cloud/base3-wave3, or PAGES_BASE_REF) is built the same way from `git archive`, and
  # tests/pages_fp_diff.py lists what differs: by design only the footer's page counter on the EDIT and ENV pages and the new pages (never regenerated: it only prints; exit 1 on anything else)
  pages)   echo "== pages"; pg=0
           $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/pages_test tests/pages_test.c -lm && $OUT/pages_test > "$OUT/last.log" 2>&1 || { echo "   FAIL"; tail -25 "$OUT/last.log"; pg=1; }
           grep "^pages: \(widest\|title\)" "$OUT/last.log" | cut -c1-200
           ref=${PAGES_BASE_REF:-cloud/base3-wave3}
           if $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/pages_fp tests/pages_fp.c -lm && $OUT/pages_fp > $OUT/pages_fp_new.txt 2>$OUT/last.log; then
             if git -C .. rev-parse --verify -q "$ref^{commit}" > /dev/null; then
               rm -rf build/pages_base; mkdir -p build/pages_base
               if git -C .. archive "$ref" felucca-base | tar -x -C build/pages_base && mkdir -p build/pages_base/felucca-base/build && cp -R build/gen build/gen_test build/pages_base/felucca-base/build/ && cp tests/pages_fp.c build/pages_base/felucca-base/tests/ &&
                  (cd build/pages_base/felucca-base && $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o ../../host/pages_fp_base tests/pages_fp.c -lm) && $OUT/pages_fp_base > $OUT/pages_fp_base.txt 2>$OUT/last.log; then
                 echo "   the existing UI renders against $ref:"
                 # the work with the A / B letter (job G) off against the base: nothing but the footer's page counter may change; then the work as it is against itself with the letter off: only the header
                 $CC -O1 -DDUELL_TEST_NO_AB "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/pages_fp_noab tests/pages_fp.c -lm && $OUT/pages_fp_noab > $OUT/pages_fp_noab.txt 2>$OUT/last.log || { echo "   FAIL (pages_fp with the letter off does not build or run)"; pg=1; }
                 python3 tests/pages_fp_diff.py $OUT/pages_fp_base.txt $OUT/pages_fp_noab.txt --allow-foot EDIT,ENV > $OUT/pages_diff.txt; r=$?
                 sed 's/^/   /' $OUT/pages_diff.txt
                 [ $r = 0 ] || { echo "   FAIL (a render changed in more than the footer's page counter)"; pg=1; }
                 python3 tests/pages_fp_diff.py $OUT/pages_fp_noab.txt $OUT/pages_fp_new.txt --head-only > $OUT/pages_diff2.txt || { echo "   FAIL (the letter changes more than the header)"; pg=1; }
                 sed 's/^/   /' $OUT/pages_diff2.txt
               else echo "   FAIL (the base could not be built from $ref)"; tail -5 $OUT/last.log; pg=1; fi
             else echo "   (no git ref $ref here: the renders are not compared with the base)"; fi
           else echo "   FAIL (pages_fp does not build or run)"; tail -5 $OUT/last.log; pg=1; fi
           [ $pg = 0 ] && echo "   PASS" || fail=1 ;;
  scope2)  $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/editor_scope2_test tests/editor_scope2_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } && run scope2 $OUT/editor_scope2_test ;;   # cloud job I: the editor protocol's scope 2 (the extension slots: GET, SET, DESC, CHANGED, TRACK_PARAM's scope byte; web/EDITOR_PROTOCOL.md)
  # ---- cloud job G: EDITABLE B (docs/design/EDITABLE_B_DESIGN.md; morph.c morph_b_*, ui_input.c edit_param, ui_draw.c head_ab, PROJ_REV 7). The behaviour is tested in morph (the engine: marks, UNDO / ROLL orders, save re-make, the stored seeds' hash of
  # cloud/base3-wave3), morphui (the knob at 127, the letter, B FULL, SAME ON A AND B, B: NOT RECORDED, a patch round trip, the undo of loads), midictl and editor (a CC, a learned CC and an editor write at 127 go to A), multi, persist (power cycle),
  # project, uitest (the letter's room). This block: the three safety rules broken in a copy of the firmware, one run each; each must FAIL a test
  editb)   echo "== editb"; eb=0
           eb_mut() {   # eb_mut "name" test.c file 'old' 'new' [file 'old' 'new' ..]
             name=$1; tsrc=$2; shift 2
             echo "== editb mutation: $name"
             rm -rf build/eb_mut; mkdir -p build/eb_mut
             cp -R firmware tests build/eb_mut/ || { echo "   FAIL (copy)"; eb=1; return; }
             while [ $# -ge 3 ]; do
               f=build/eb_mut/$1
               python3 -I -c 'import sys; p,a,b=sys.argv[1:4]; s=open(p).read(); assert s.count(a)==1, "pattern not found once"; open(p,"w").write(s.replace(a,b))' "$f" "$2" "$3" || { echo "   FAIL (the pattern was not found once in $1)"; eb=1; return; }
               shift 3
             done
             if ! $CC -O1 "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/eb_mut_t build/eb_mut/tests/$tsrc -lm > $OUT/last.log 2>&1; then echo "   FAIL (the mutated build does not compile)"; tail -5 $OUT/last.log; eb=1; return; fi
             $OUT/eb_mut_t > $OUT/last.log 2>&1
             if grep -q ' FAIL$' $OUT/last.log; then echo "   PASS (detected: $(grep -c ' FAIL$' $OUT/last.log) checks failed)"; else echo "   FAIL (the mutation was NOT detected)"; eb=1; fi
           }
           eb_mut "edits at 126 go to B (morph.c morph_b_mode: the B end starts at 126)" morph_ui_test.c firmware/src/morph.c 'mp->fader != 127u || (!mp->nid' 'mp->fader < 126u || (!mp->nid'
           eb_mut "edits at 126 go to B (the same, caught by the engine test)" morph_test.c firmware/src/morph.c 'mp->fader != 127u || (!mp->nid' 'mp->fader < 126u || (!mp->nid'
           eb_mut "a CC writes B (midi_control.c midi_cc_set goes through morph_b_set at the B end)" midi_control_test.c \
              firmware/src/midi_control.c 'static void midi_cc_set(track_t *t, uint32_t id, uint32_t value)
{' 'static uint32_t morph_b_mode(uint32_t, uint32_t);
static int morph_b_set(uint32_t, uint32_t, int32_t);
static void midi_cc_set(track_t *t, uint32_t id, uint32_t value)
{' \
              firmware/src/midi_control.c '    t->p[id] = (int16_t)v;
    (void)motion_capture(t, id, (int16_t)v);' '    if (morph_b_mode((uint32_t)(t - trk), id) == 1u) {
        morph_b_set((uint32_t)(t - trk), id, v);
        return;
    }
    t->p[id] = (int16_t)v;
    (void)motion_capture(t, id, (int16_t)v);'
           eb_mut "SAVE loses the hand-set values (upreset.c patch save writes no edit list)" morph_ui_test.c firmware/src/upreset.c 'up_xb_nin = morph_edits(pi, ed);' 'up_xb_nin = 0u * morph_edits(pi, ed);'
           eb_mut "SAVE loses the hand-set values (morph.c morph_blk_make forgets the marks on its re-roll)" morph_test.c firmware/src/morph.c '                memcpy(morph_pins(mp), pn, PIN_BYTES);' '                memset(morph_pins(mp), 0, PIN_BYTES);'
           [ $eb = 0 ] && echo "   PASS" || fail=1 ;;
  velo)    # KEY SENSE (DUELL_VELO; docs/design/VELO_STUDY.md, docs/VELO_CSV.md): the sense core on a simulated hand (tests/velo_test.c, velo_pill.h), the converter stage of the scan tick against a
           # simulated pill (tests/velo_tick_test.c; the real converter's timing, the LED lines against the conversion and the pull-up's recovery CANNOT be tested here), the non-blocking converter's register
           # sequence (velo_adc_test.c), the hooks in the notes path with the real sound code (velo_hook_test.c) built with the flag off and with the flag on + the setting OFF: every rendered sample and the key edges
           # hash equal; the replay of a recording (tools/velo_replay.py) equals the core's own run; the flag-off preprocessor output of felucca.c equals the tree before KEY SENSE (VELO_BASE=<ref>, release and test mode; skipped when VELO_BASE is not set: a later merge changes flag-off code by design); and three mutations
           # (the gap guard removed, a release accepted on one open sample, the calibration made one-way) must each FAIL velo_test.
           mkdir -p $OUT/velo
           VC="cc -O2 -w -Ifirmware/hal -Ifirmware/src -Itests"
           velo_mut() {
             name=$1; shift
             echo "== velo_mut $name"
             rm -rf build/velo_mut; mkdir -p build/velo_mut/firmware/src build/velo_mut/tests
             cp firmware/src/sense.c firmware/src/sense.h firmware/src/sense_cal.h build/velo_mut/firmware/src/ && cp tests/velo_test.c tests/velo_pill.h build/velo_mut/tests/ || { echo "   FAIL (copy)"; fail=1; return; }
             while [ $# -ge 2 ]; do
               f=build/velo_mut/$1; sed "$2" "$f" > "$f.new" && mv "$f.new" "$f"
               if cmp -s "$f" "$1"; then echo "   FAIL (the mutation changed nothing in $1)"; fail=1; return; fi
               shift 2
             done
             if ! cc -O2 -w -Ibuild/velo_mut/firmware/src -Ibuild/velo_mut/tests -o $OUT/velo/velo_mut build/velo_mut/tests/velo_test.c -lm > $OUT/last.log 2>&1; then
               echo "   FAIL (the mutated build does not compile)"; tail -5 $OUT/last.log; fail=1; return
             fi
             if $OUT/velo/velo_mut > $OUT/last.log 2>&1; then echo "   FAIL (the mutation was NOT detected)"; fail=1; else echo "   PASS (detected: $(grep -c ' FAIL$' $OUT/last.log) checks failed)"; fi
           }
           $VC -o $OUT/velo/velo_test tests/velo_test.c -lm &&
           $VC -DDUELL_VELO=1 -o $OUT/velo/vtick1 tests/velo_tick_test.c -lm && $VC -DDUELL_VELO=0 -o $OUT/velo/vtick0 tests/velo_tick_test.c -lm &&
           $VC -o $OUT/velo/vadc tests/velo_adc_test.c &&
           $CC -O1 -w "-DFELUCCA_VERSION=\"$FV\"" -DDUELL_VELO=1 -Ifirmware/hal -o $OUT/velo/vhook1 tests/velo_hook_test.c -lm &&
           $CC -O1 -w "-DFELUCCA_VERSION=\"$FV\"" -DDUELL_VELO=0 -Ifirmware/hal -o $OUT/velo/vhook0 tests/velo_hook_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } &&
           run "velo: the sense core: the simulated hand (median 50..70, <= 3 % at 110+, <= 5 % below 10), gaps, bounce, chords, calibration both ways, aftertouch, the setting, the store" $OUT/velo/velo_test &&
           run "velo: the converter stage of the tick (flag on) and the digital scan of the tick as it was (flag off): the same edges" sh -c "$OUT/velo/vtick1 > $OUT/velo/vtick1.txt; r=\$?; cat $OUT/velo/vtick1.txt | tail -5; $OUT/velo/vtick0 > $OUT/velo/vtick0.txt || exit 1; [ \$r = 0 ] && grep HASH $OUT/velo/vtick1.txt > $OUT/velo/vtick1.h && grep HASH $OUT/velo/vtick0.txt > $OUT/velo/vtick0.h && cmp $OUT/velo/vtick1.h $OUT/velo/vtick0.h" &&
           run "velo: fm1_adc.h non-blocking read: the register sequence of the blocking one" $OUT/velo/vadc &&
           run "velo: the hooks (key_on / keyboard_block / aftertouch) with the real sound code; the setting OFF renders every sample as the flag-off build" sh -c "$OUT/velo/vhook1 > $OUT/velo/vhook1.txt; r=\$?; tail -22 $OUT/velo/vhook1.txt; $OUT/velo/vhook0 > $OUT/velo/vhook0.txt || exit 1; [ \$r = 0 ] && [ \$(grep -c HASH $OUT/velo/vhook0.txt) = 6 ] && grep HASH $OUT/velo/vhook1.txt > $OUT/velo/vhook1.h && grep HASH $OUT/velo/vhook0.txt > $OUT/velo/vhook0.h && cmp $OUT/velo/vhook1.h $OUT/velo/vhook0.h" &&
           run "velo: the replay of a recording (tools/velo_replay.py) = the core's own run" sh -c "$OUT/velo/velo_test --csv $OUT/velo/hand.csv $OUT/velo/ref.csv && python3 -I tools/velo_replay.py $OUT/velo/hand.csv --landings $OUT/velo/out.csv > $OUT/velo/replay.txt && tail -n +2 $OUT/velo/out.csv | cmp - $OUT/velo/ref.csv && grep -q '^     all    600 ' $OUT/velo/replay.txt && tail -3 $OUT/velo/replay.txt" &&
           run "velo: the firmware builds with DUELL_VELO=1 (syntax; the device's assembler is not here)" sh -c "cc -fsyntax-only -w \$([ \$(uname) = Darwin ] && echo '-target x86_64-unknown-linux-gnu') -Ifirmware/hal -Ifirmware/src -Ibuild/gen -DDUELL_VELO=1 '-DFELUCCA_ID=\"FM-1_900\"' firmware/src/felucca.c" &&
           velo_mut "the gap guard removed (sense.c sk: a gap is no gap)" firmware/src/sense.c 's#gap = (k->fl & K_SEEN) && (uint32_t)(t - k->t_last) > SC_GAP_US;#gap = 0;#' &&
           velo_mut "a release accepted on one open sample (SC_REL_N 1)" firmware/src/sense.c 's#if (++k->nopen >= SC_REL_N)#if (++k->nopen >= 1u)#' &&
           velo_mut "the calibration made one-way (the floor only down, the ceiling only up)" firmware/src/sense.c 's#: lo + eta \* SC_Q_LO / 1000u;#: lo;#' firmware/src/sense.c 's#: hi - eta \* SC_Q_HI / 1000u;#: hi;#'
           echo "== velo: flag-off preprocessor output of felucca.c equals the tree before KEY SENSE (VELO_BASE)"
           pb=; [ -n "$VELO_BASE" ] && pb=$(git rev-parse --verify -q "$VELO_BASE")
           if [ -z "$pb" ]; then echo "   skipped: VELO_BASE not set (e.g. VELO_BASE=cloud/base3-wave4 on the merge commit of KEY SENSE)"; else
             rm -rf build/velo_base; mkdir -p build/velo_base
             git -C .. archive $pb felucca-base/firmware | tar -x -C build/velo_base && vb=0
             for m in "" "-DDUELL_TEST_MODE=1"; do
               (cd build/velo_base/felucca-base && cc -E -P -Ifirmware/hal -Ifirmware/src -I"$OLDPWD/build/gen" '-DFELUCCA_ID="FM-1_900"' $m firmware/src/felucca.c > "$OLDPWD/$OUT/velo/pp_base.i" 2>/dev/null)
               cc -E -P -Ifirmware/hal -Ifirmware/src -Ibuild/gen '-DFELUCCA_ID="FM-1_900"' $m firmware/src/felucca.c > $OUT/velo/pp_new.i 2>/dev/null
               if cmp -s $OUT/velo/pp_base.i $OUT/velo/pp_new.i && [ -s $OUT/velo/pp_new.i ]; then echo "   same ($(wc -l < $OUT/velo/pp_new.i) lines) ${m:-release}"; else echo "   DIFFERENT ${m:-release}"; vb=1; fi
             done
             [ "$vb" = 0 ] && echo "   PASS" || { echo "   FAIL"; fail=1; }
           fi ;;
  ldrscreen) P=$(ls -t build/duell-*.fwsc 2>/dev/null); O=${LDR_OLD:-$(echo "$P" | sed -n 2p)}; N=${LDR_NEW:-$(echo "$P" | sed -n 1p)}; [ -n "$O" ] || O=$N   # (LDR_OLD and LDR_NEW, else the newest two packages in build/; one package is used as both)
           $CC -O1 -DLDR_SCREEN_TEST=1 -o $OUT/ldr_test_screen tests/ldr_test.c -lm && $CC -O1 -o $OUT/ldr_test_plain tests/ldr_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } &&
           run ldrscreen sh -c "$OUT/ldr_test_screen $O $N > $OUT/ldr_s.log && $OUT/ldr_test_plain $O $N > $OUT/ldr_p.log && [ \"\$(grep FINAL $OUT/ldr_s.log)\" = \"\$(grep FINAL $OUT/ldr_p.log)\" ] && grep -q 'loader test passed' $OUT/ldr_s.log && grep -q 'loader test passed' $OUT/ldr_p.log || { grep -E 'FAIL|FINAL' $OUT/ldr_s.log $OUT/ldr_p.log; exit 1; }" ;;
  midimap) mkdir -p build/gen_test_shipped
           python3 tools/gen_midi_map.py build/gen_test_shipped/midi_map_defaults.h >/dev/null && python3 tests/midimap_refgen.py build/gen_test >/dev/null || { echo "== midimap generators"; echo "   FAIL"; fail=1; false; }
           MMF="-w -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests"; MMS="-w -DDUELL_TEST_MODE=1 -Ibuild/gen_test_shipped -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests"
           cc -O1 $MMF "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/midimap_test tests/midimap_test.c -lm && cc -O1 $MMS "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/midimap_test_shipped tests/midimap_test.c -lm || { echo "== compile"; echo "   FAIL (does not compile)"; fail=1; false; } &&
           { run midimap $OUT/midimap_test; run midimap_shipped_default $OUT/midimap_test_shipped; run midimapgen python3 -I tests/midimap_gen_test.py; } ;;
  midimapmut) mmm() {                                  # midimapmut's self-check: a COPY of firmware/ and tests/ with one safety rule broken must make tests/midimap_test.c FAIL
             name=$1; shift
             echo "== midimap_mut $name"
             rm -rf build/midimap_mut; mkdir -p build/midimap_mut
             cp -R firmware tests build/midimap_mut/ || { echo "   FAIL (copy)"; fail=1; return; }
             while [ $# -ge 2 ]; do
               f=build/midimap_mut/$1; sed "$2" "$f" > "$f.new" && mv "$f.new" "$f"
               if cmp -s "$f" "$1"; then echo "   FAIL (the mutation changed nothing in $1)"; fail=1; return; fi
               shift 2
             done
             if ! cc -O1 -w -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests "-DFELUCCA_VERSION=\"$FV\"" -o $OUT/midimap_mut build/midimap_mut/tests/midimap_test.c -lm > $OUT/last.log 2>&1; then
               echo "   FAIL (the mutated build does not compile)"; tail -5 $OUT/last.log; fail=1; return
             fi
             if $OUT/midimap_mut > $OUT/last.log 2>&1; then echo "   FAIL (the mutation was NOT detected)"; fail=1; else echo "   PASS (detected: $(grep -c ' FAIL' $OUT/last.log) checks failed)"; fi
           }
           python3 tests/midimap_refgen.py build/gen_test >/dev/null
           mmm "the lock check removed (map_set and map_clear take a standard number)" firmware/src/midi_learn.c 's/if (MAP_LOCKED_CC(cc))$/if (0 \&\& MAP_LOCKED_CC(cc))/'
           mmm "two parameters allowed on one CC (a taken number is no conflict)" firmware/src/midi_learn.c 's/ \&\& !force) {/ \&\& 0) {/'
           mmm "the morph trigger back on the CC number (bit cc - 21)" firmware/src/midi_control.c 's/uint32_t pi, uint32_t code, uint32_t value)$/uint32_t pi, uint32_t code, uint32_t value, uint32_t cc)/' \
               firmware/src/midi_control.c 's/midi_morph(ch, trk_index(t), code, value);/midi_morph(ch, trk_index(t), code, value, cc);/' \
               firmware/src/midi_control.c 's/b = 1u << (code - MAP_T_ROLL_A);/b = 1u << (cc - 21u);/'
           mmm "ROLL's halves moved (End B from 64 instead of 85: the middle rolls)" firmware/src/midi_control.c 's/if (value < 85u)$/if (value < 64u)/' ;;
esac; done
exit $fail
