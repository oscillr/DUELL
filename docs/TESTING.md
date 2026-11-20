# What to test: DUELL 1.3

Thank you for testing. You can test, at your own risk, on an FM-1 with DUELL 1.3 (see [INSTALL.md](INSTALL.md)). Web emulator: [https://oscillr.github.io/DUELL/emu/](https://oscillr.github.io/DUELL/emu/). It has been run on the author's two units, so reports from other units are especially welcome.

## What to test

- [ ] **DUEL / STEREO / LAYER**: DUEL keeps part 1 only on the left and part 2 only on the right (with effects on). STEREO is full stereo. LAYER plays both parts from the keys; try XFADE and the key SPLIT.
- [ ] **MORPH**: move the fader slowly A to B, ROLL A and B, UNDO, change AMOUNT. Any click, dropout, silence or painful sound? Does a patch always load at A?
- [ ] **Matrix**: assign a few of the 8 slots (LFO 2, ENV 2, velocity, aftertouch, mod wheel). Do ENV 2 and LFO 2 DEST pages do what they say?
- [ ] **MULTIs**: load the 16 multis (M01 to M16), then save, rename and erase your own. Power-cycle the unit and load again.
- [ ] **PATTERNS**: load a pattern, edit and save over a slot, erase it, use it as the SEQUENCER arp pattern.
- [ ] **PATCHES**: browse by TYPE, mark favourites, hold SAVE (OVERWRITE #NN: turn knob 1 to pick another slot, knob 2 to RENAME: the NAME screen, OCT+ back to the dialog, OCT+ saves under the new name), RESTORE the slot (knob 4), RESTORE ALL on SAVE > TOOLS, MIDI program change = slot. Fill all 128 slots by saving over each: there is no "full". Power-cycle the unit and check it all came back.
- [ ] **MIDI** (if your browser has WebMIDI, e.g. Chrome or Edge): part 1 on channel 1, part 2 on channel 2, no stuck notes; mod wheel (CC1) moves the morph; program change and bank select.
- [ ] **Sound palette**: audition the 128 patches, across the engines. Levels between sounds, anything harsh, stuck or silent?
- [ ] **Ducker / effects**: depth, attack, release; SHIMMER, RESONATOR, PLATE (switch between all six reverb types while a tail rings: no click), ping-pong; no clicks.
- [ ] **Delay MODE** (GLO > DELAY 1/2, K1): DIGITAL (the old delay, PING off), PING-PONG (the old PING on), RANDOM and OCT UP. The pages: DELAY 1/2 MODE TIME FDBK COLR, DELAY 2/2 SPREAD MRATE MDEPTH MIX. DIGITAL does not show SPREAD (K1 stays empty), RANDOM / OCT UP do not show MRATE (K2 stays empty) and make K3 CHANCE. Turning MODE into RANDOM with CHANCE at 0: the header says CHANCE 40 % and K3 shows 40; back to DIGITAL or PING-PONG the MDEPTH is 0 again. The repeats do not ping-pong in RANDOM / OCT UP. Plucks and a beat with FDBK 60..90, CHANCE 25..100: do the events land on the beat (an edge per delay time), is the octave-up climb tamed by COLR, does turning MODE while the delay rings click (also DIGITAL <-> PING-PONG), does it sound better than the ping-pong (your ears), PLAY restarts the beat. In DUEL (0.2.22) MODE is shown and all four modes work, mono on each part's side: PING sounds like DIGI, RAND and OCT UP stay in the middle of the part's side (the other channel stays silent), SPREAD is not shown. Demo files: host renders by `tests/dlyev_render.c`.
- [ ] **MACROS** (HOME, MACROS 1/2 and 2/2; hold SEL for QUICK MACRO): does each one change the sound, is the middle the sound as saved, does loading a sound put them back?
- [ ] **MIDI PRESET** (SETTINGS > MIDI): with a Novation Circuit Tracks or an Akai MPC / Force, does the preset for it work out of the box?
- [ ] **Stability**: play for a while with an arp running and many effects on; report any freeze or screen glitch.

## How to report a bug

Open an issue with the [bug report form on GitHub](https://github.com/oscillr/DUELL/issues/new/choose). Choose where it happened and give the version shown in ABOUT, your browser and computer if it is the editor or installer, the steps, what you expected and what happened. Please do not paste private information. No warranty; DUELL is not affiliated with M-VAVE.

# For people with hardware

The checklist items below need a real FM-1 or real MIDI gear. Flash only after reading [INSTALL.md](INSTALL.md) and [RECOVERY.md](RECOVERY.md); you install at your own risk.

Hardware checklist:

- [ ] **Install** (INSTALL.md): did it finish, did the unit restart, does ABOUT show DUELL?
- [ ] **MIDI channels**: part 1 on channel 1, part 2 on channel 2; no stuck notes.
- [ ] **MIDI program change and bank select**: bank 0 and bank 1; in LAYER, program 0 to 15 recalls MULTIs.
- [ ] **MIDI clock**: arp, LFO and delay follow an external clock; start/stop.
- [ ] **CLK AUTO** (GLO > TEMPO, KNOB 4): start and stop a DAW or a Circuit Tracks clock on USB, then on TRS: does DUELL switch (header CLOCK USB / TRS), follow, and go back to its own tempo about 1 s after the clock stops, without stopping or stumbling?
- [ ] **USB audio input**: on macOS 15 or older (and on Windows, Linux, iOS if you have them), is DUELL listed as an audio input and does it record? MIDI still there? (The computer may set the unit up as a new device once.)
- [ ] **LEDs during a save**: save a patch while LEDs are lit: the keys and buttons should go dark for a moment, not flash bright.
- [ ] **Mod wheel** (CC1) moves the morph; CC 56 to 62, 70, 102, 103 and 114 to 118 behave.
- [ ] **Sidechain / ducker**: sources PUMP, PART and MIDI channel/note; depth, attack, release; no clicks.
- [ ] **Stability**: play for a while with an arp running and many effects on; report any freeze, reboot or screen glitch.

## Voices and CPU

Each part's AVAILABLE VOICES in the header shows how many more notes that part can play right now; that number is what really plays. If you hear a click or a dropout, report the patch or multi, the mode, how many keys you held and what you turned.
