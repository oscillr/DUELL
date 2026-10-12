# DUELL quick start

**DUELL**
MultiMorphing Bitimbral Synthesizer
by OSCILL/R

DUELL beta turns the FM-1 into two synths in one box. This page is the short tour; [SPEC.md](../SPEC.md) is the full design. DUELL 0.2.4 beta runs on the FM-1 ([INSTALL.md](INSTALL.md)) and in the [hosted browser emulator](https://oscillr.github.io/DUELL/emu/) (click the page once to start the sound); the controls below are the same in both.

In the emulator your saved patches, MULTIs and PATTERNs stay in the browser. Press "Download saved data" in the emulator page to back them up, and clear the site data after an emulator update if saved data misbehaves.

## The idea
- **Two parts**, 1 and 2. Each has its own sound, effects, EQ, ducker and arp. The **ALGORITHM** knob picks which part you are editing.
- **Three modes** (HOME, second sub-page, first knob: MODE):
  - **SPLIT** (default): part 1 hard left, part 2 hard right, kept apart, effects included. For playing two sounds into two channels.
  - **STEREO**: both parts in one stereo field, for headphones.
  - **LAYER**: both parts play on the keys together. The MODE page has BALANCE, XFADE (velocity crossfade) and SPLIT (a key split point).
- **MORPH**: every sound has two ends, A and B, and one fader between them.

## The controls in one minute
- **SELECT** switches sub-pages (and moves rows in lists). It never changes the tempo. **Knobs 1 to 4** adjust the four columns on screen. Every control does what its label says.
- **PRESETS** loads sounds for the selected part.
- **HOME** opens the MORPH page. Hold HOME for the menu (ABOUT shows the version and credits).
- **TEMPO** is on the GLO TEMPO page, knob 1. On the GLO page the keys play notes, knob 1 mutes part 1 and knob 2 mutes part 2 with one click (turn again to unmute), knobs 3 and 4 are the levels.
- **OCT-/OCT+** change octave; in dialogs OCT- means back and OCT+ means do it.

## Make a first sound
1. Press **PRESETS** and turn it to hear sounds. Press **ALGORITHM** to select part 2 and load another.
2. Go to the MODE page and try SPLIT, STEREO, LAYER.
3. Press **HOME** for the MORPH page. Knob 1 moves the morph from A to B. Knob 2 rolls a new random end with one click (turn left = A, right = B). Knob 3 undoes the last roll of A or B. Knob 4 is AMOUNT, the one percentage: how far a roll may go.
4. Press **SAVE**. The first page is **PATCHES**. **Hold SAVE** asks "SAVE OVER <name>?"; OCT+ says yes. Saving over a factory sound makes your own copy; erasing it brings the factory sound back. On an empty place, hold SAVE opens the NAME screen and stores a new patch.
   **Tip:** a short press of SAVE opens the page that fits where you are: PATTERNS when you are on a sequencer page (STEP, GRID, AUTOMATION), otherwise MULTIS in LAYER and PATCHES in SPLIT and STEREO. SELECT moves between PATCHES, PATTERNS, MULTIS and TOOLS. Hold SAVE on PATTERNS or MULTIS saves over the slot you picked ("SAVE OVER <name>?", OCT+ yes); an empty multi slot asks for a name first.

## Saving things
- **PATCHES**: one list of factory and your own sounds (up to 48 stored patches; the card shows FREE nn). Knob 2 filters by type, knob 3 marks favourites, knob 4 erases. Saving needs the transport stopped.
- **PATTERNS**: 16 named, overwritable step patterns.
- **MULTIS**: 16 slots that hold both parts, effects, arps, patterns, mode and morph settings. M01 to M04 are factory multis.
- To start from scratch (erases settings, patches, multis, patterns; keeps your user samples): MENU (hold HOME) > SYSTEM > ERASE DATA. A reinstall alone keeps your data.
- The device backup (installer page) does not include MULTIs or PATTERNs. Back up before you update. This beta's patches, MULTIs and PATTERNs may not load in other betas.

## Playing and modulation
- **ARP**, **SEQ**, **PLAY/STOP**, **REC**: each part has an arp; the SEQUENCER arp mode plays a 16-step pattern.
- Two envelopes, two LFOs and an 8-slot modulation matrix per part. ENV 1 is the amplitude envelope, ENV 2 the filter envelope.
- **FX**: insert, per-part EQ, per-part **ducker** (sidechain from a MIDI note, a built-in pulse or the other part), chorus, delay (with ping-pong), reverbs. SHIM and RESO reverbs come from JIANT.

## MIDI
Part 1 listens on channel 1 and part 2 on channel 2 (editable). In the emulator, a WebMIDI keyboard or controller sends notes and controllers (the browser asks for MIDI access; clock is not passed on). On the device: clock in, program change and bank select, the mod wheel (CC1) moves the MORPH fader and is also a matrix source. CC 20 morph, 21 roll A, 22 roll B, 23 undo, 25 amount, 111 morph target, 112 mode. In LAYER, program change 0 to 15 on part 1's channel recalls MULTIs M01 to M16. Not yet tested with a range of real MIDI devices.

## Next
[TESTING.md](TESTING.md) lists what to try and report. [TROUBLESHOOTING.md](TROUBLESHOOTING.md) if something misbehaves.
