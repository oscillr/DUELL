# What to test: DUELL beta

Thank you for testing. You can test in the **browser emulator** (<https://oscillr.github.io/DUELL/emu/>; click the page once to start the sound) or, at your own risk, on an FM-1 with the firmware beta (0.2.20; see [INSTALL.md](INSTALL.md)). It has been run on one real unit only, the author's, so reports from other units are especially welcome.

## What to test in the emulator

- [ ] **DUEL / STEREO / LAYER**: DUEL keeps part 1 only on the left and part 2 only on the right (with effects on). STEREO is full stereo. LAYER plays both parts from the keys; try BALANCE, XFADE and the key SPLIT.
- [ ] **MORPH**: move the fader slowly A to B, ROLL A and B, UNDO, change AMOUNT. Any click, dropout, silence or painful sound? Does a patch always load at A?
- [ ] **Matrix**: assign a few of the 8 slots (LFO 2, ENV 2, velocity, aftertouch, mod wheel). Do ENV 2 and LFO 2 DEST pages do what they say?
- [ ] **MULTIs**: load the 10 factory multis (M01 to M10), then save, rename and erase your own. Reload the page and load again.
- [ ] **PATTERNS**: load a factory pattern, edit and save over a slot, erase it, use it as the SEQUENCER arp pattern.
- [ ] **PATCHES**: browse by TYPE, mark favourites, hold SAVE (OVERWRITE #NN: turn knob 1 to pick another slot), RESTORE the slot (knob 4), RESTORE ALL on SAVE > TOOLS, MIDI program change = slot. Fill all 128 slots by saving over each: there is no "full". Reload the page and check it all came back.
- [ ] **MIDI** (if your browser has WebMIDI, e.g. Chrome or Edge; the emulator takes notes and controllers, not clock): part 1 on channel 1, part 2 on channel 2, no stuck notes; mod wheel (CC1) moves the morph; program change and bank select.
- [ ] **Sound palette**: audition the factory palette (about a hundred sounds, across the engines). Levels between sounds, anything harsh, stuck or silent?
- [ ] **Ducker / effects**: depth, attack, release; SHIM, RESO, PLATE (switch between all six reverb types while a tail rings: no click), ping-pong; no clicks.
- [ ] **Delay MODE** (FX > DELAY 1/2, K1; PING-PONG, RANDOM and OCT UP in STEREO and LAYER): DIGITAL (the old delay, PING off), PING-PONG (the old PING on), RANDOM and OCT UP. The pages: DELAY 1/2 MODE TIME FDBK COLR, DELAY 2/2 SPREAD MRATE MDEPTH MIX. DIGITAL does not show SPREAD (K1 stays empty), RANDOM / OCT UP do not show MRATE (K2 stays empty) and make K3 CHANCE. Turning MODE into RANDOM with CHANCE at 0: the header says CHANCE 40 % and K3 shows 40; back to DIGITAL or PING-PONG the MDEPTH is 0 again. The repeats do not ping-pong in RANDOM / OCT UP. Plucks and a beat with FDBK 60..90, CHANCE 25..100: do the events land on the beat (an edge per delay time), is the octave-up climb tamed by COLR, does turning MODE while the delay rings click (also DIGITAL <-> PING-PONG), does it sound better than the ping-pong (your ears), PLAY restarts the beat, DUEL does not show MODE and SPREAD. Demo files: host renders by `tests/dlyev_render.c`.
- [ ] **Stability**: play for a while with an arp running and many effects on; report any freeze or screen glitch.

The emulator keeps your saved data in the browser. Press **Download saved data** to back it up, and clear the site data after an emulator update if saved data from an older build misbehaves.

## How to report a bug

Open an issue with the [bug report form on GitHub](https://github.com/oscillr/DUELL/issues/new/choose). Choose "Browser emulator" under "Where did it happen?" and give the version shown on the emulator page, your browser and computer, the steps, what you expected and what happened. Please do not paste private information. No warranty; DUELL is not affiliated with M-VAVE.

# For people with hardware

The checklist items below need a real FM-1 or real MIDI gear. Flash only after reading [INSTALL.md](INSTALL.md) and [RECOVERY.md](RECOVERY.md); you install at your own risk.

Hardware checklist:

- [ ] **Install** (INSTALL.md): did it finish, did the unit restart, does ABOUT show DUELL?
- [ ] **MIDI channels**: part 1 on channel 1, part 2 on channel 2; no stuck notes.
- [ ] **MIDI program change and bank select**: bank 0 and bank 1; in LAYER, program 0 to 15 recalls MULTIs.
- [ ] **MIDI clock**: arp, LFO and delay follow an external clock; start/stop.
- [ ] **CLK AUTO** (GLO > TEMPO, KNOB 4): start and stop a DAW or a Circuit Tracks clock on USB, then on TRS: does DUELL switch (header CLOCK USB / TRS), follow, and go back to its own tempo about 1 s after the clock stops, without stopping or stumbling?
- [ ] **USB audio input** (0.2.14): on macOS 15 or older (and on Windows, Linux, iOS if you have them), with MENU > SYSTEM > USB SERIAL OFF, is DUELL listed as an audio input and does it record? MIDI still there? (The computer may set the unit up as a new device once.)
- [ ] **LEDs during a save** (0.2.14): save a patch while LEDs are lit: the keys and buttons should go dark for a moment, not flash bright.
- [ ] **Mod wheel** (CC1) moves the morph; CC 20 to 25 and 111 to 118 behave.
- [ ] **Sidechain / ducker**: sources PUMP, PART and MIDI channel/note; depth, attack, release; no clicks.
- [ ] **Stability**: play for a while with an arp running and many effects on; report any freeze, reboot or screen glitch.

## CPU test (developer builds only: MENU > SYSTEM > TEST and the four TEST multis)

**The official beta has no TEST screen and no test multis** (M13 to M16 are empty there; ABOUT still shows the CPU line). This section is for developer builds, made with the test mode on (`DUELL_TEST_MODE=1`, off by default):

- Device: `DUELL_TEST_MODE=1 ./build.sh` in `felucca-base/` (it generates the factory multis with M13 to M16 and compiles with `-DDUELL_TEST_MODE=1`).
- Emulator: `DUELL_TEST_MODE=1 .venv/bin/python gen_only.py`, then `DUELL_TEST_MODE=1 sh felucca-base/web/emu/build.sh`.
- Host tests: `./host_tests.sh` builds its test binaries with the test mode on (the `release` group builds with it off and checks that the TEST page and M13 to M16 are absent and M01 to M10 intact).
- DX7 import tests use synthetic banks generated in the test code (`web/test_dx7_synth.mjs`); no real banks are in the repository. To also run your own banks, set `DX7_BANKS=<folder of .syx banks>` (`web/test_librarian.mjs` converts them too; `./host_tests.sh dx7compat` runs the local engine / reference comparison; without the variable it prints `skipped: no banks` and passes).

We will need to know how hard the audio works on real units (hardware only). Hold **HOME** for the menu, go to the SYSTEM tab (SELECT) and open **TEST** (K1 right or OCT+); OCT- goes back, HOME or any page button leaves the menu. It shows the loaded MULTI, the ENGINE and PATCH of part 1 and part 2, and the CPU meter:

- **AVERAGE 1 S**: how much of the time budget the audio used, averaged over the last second (the bar has a mark at 80 %).
- **PEAK HOLD**: the worst single block of the last seconds. Above 100 % a block was late.
- **MISSED**: audio blocks that were late since the last RESET. Any number above 0 may be heard as a click or dropout.
- The word: OK, NEAR THE LIMIT (80 % and up), AT THE LIMIT (95 % and up), BLOCKS MISSED JUST NOW (a miss in the last 5 s).

Knobs: **K1 MORPH** and **K2 ROLL** as on the MORPH page, **K3 LOAD** steps through your multis and loads the one it stops on at once, **K4 RESET** (turn it) clears the peak hold and the missed count and starts a fresh average.

The test multis are M13 to M16 (SAVE > MULTIS or K3 on the TEST screen; they come with the firmware, ERASE brings them back):

1. **TEST 1 BASE** (WHEEL JAZZ CLUB + ANALOG SILK CHORUS, LAYER): the baseline.
2. **TEST 2 PHASE** (PHASE CRYSTAL CAVE + ANALOG SILK CHORUS, LAYER): a heavy PHASE sound.
3. **TEST 3 HEAVY** (FM6 TINE ROOM + PHASE CRYSTAL CAVE, DUEL, SHIM reverb, PING-PONG, FLANGERs): the worst case. In DUEL the keys play the selected part; use a MIDI keyboard on channels 1 and 2 (or switch the part with ALGORITHM) to load both.
4. **TEST 4 MIX** (NOISE SEA ORGAN + LOFI WOW FLUTTER, LAYER, all sends).

All four have both arps on with 4-note chords, VOICE POLY, release about 500 ms, 138 BPM, and all effects on. How to test each: load it, go to TEST, press RESET, then hold a key (or several keys one after another) for about 30 seconds, so all 8 voices sound. Read AVERAGE, PEAK HOLD and MISSED.

**How to report**: for each multi, the firmware version, your unit (and whether it is dual core off, the default), the final AVERAGE, PEAK HOLD, MISSED and the state word, whether you heard clicks or dropouts. A photo of the TEST page works. Please also report if the page itself seems to make things worse (the meter only reads while the page is open).

Do not turn on dual core if you see the option: it is off by default and untested.
