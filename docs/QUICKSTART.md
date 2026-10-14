# DUELL quick start

**DUELL**
MultiMorphing Bitimbral Synthesizer
by OSCILL/R

DUELL beta turns the FM-1 into two synths in one box. This page is the short tour; [SPEC.md](../SPEC.md) is the full design. DUELL 0.2.13 beta runs on the FM-1 ([INSTALL.md](INSTALL.md)) and in the [hosted browser emulator](https://oscillr.github.io/DUELL/emu/) (click the page once to start the sound); the controls below are the same in both.

In the emulator your saved patches, MULTIs and PATTERNs stay in the browser. Press "Download saved data" in the emulator page to back them up, and clear the site data after an emulator update if saved data misbehaves.

## The idea
- **Two parts**, 1 and 2. Each has its own sound, effects, EQ, ducker and arp. The **ALGORITHM** knob picks which part you are editing.
- **Three modes** (HOME, second sub-page, first knob: MODE):
  - **DUEL** (default): part 1 hard left, part 2 hard right, kept apart, effects included. For playing two sounds into two channels.
  - **STEREO**: both parts in one stereo field, for headphones.
  - **LAYER**: both parts play on the keys together. The MODE page has BALANCE, XFADE (velocity crossfade) and SPLIT (a key split point).
- **MORPH**: every sound has two ends, A and B, and one fader between them.

## The first-start guide
The first time DUELL starts (and once after an update from an older build) a short guide follows the logo: six pages on the modes, MORPH, the long presses, REC as HOLD, the OCT- reset, STOP ALL and SAVE OVER. Turn **SELECT** (or press **OCT+** / **OCT-**) to go through it; **HOME** closes it, and any page button closes it and opens its page. The keys play while it is shown. To see it again: MENU (hold HOME) > SYSTEM > GUIDE.

## The controls in one minute
- **SELECT** switches sub-pages (and moves rows in lists). It never changes the tempo. **Knobs 1 to 4** adjust the four columns on screen. Every control does what its label says.
- **PRESETS** loads sounds for the selected part.
- **HOME** opens the MORPH page. Hold HOME for the menu (ABOUT shows the version and credits).
- **TEMPO** is on the GLO TEMPO page, knob 1. On the GLO page the keys play notes, knob 1 mutes part 1 and knob 2 mutes part 2 with one click (turn again to unmute), knobs 3 and 4 are the levels.
- **OCT-/OCT+** change octave when you let go of them; in dialogs OCT- means back and OCT+ means do it. Hold **OCT-** and turn a knob to reset that knob's card to its default (the octave then does not change).

## Make a first sound
1. Press **PRESETS** and turn it to hear sounds. Press **ALGORITHM** to select part 2 and load another.
2. Go to the MODE page and try DUEL, STEREO, LAYER.
3. Press **HOME** for the MORPH page. Knob 1 moves the morph from A to B. Knob 2 rolls a new random end with one click (turn left = A, right = B). Knob 3 undoes the last roll of A or B. Knob 4 is AMOUNT, the one percentage: how far a roll may go. On this page **ALGORITHM** has three positions, PART 1, BOTH and PART 2: with BOTH the morph moves both parts and both part numbers in the header light up.
4. Press **SAVE**. The first page is **PATCHES**. **Hold SAVE** asks "SAVE OVER <name>?"; OCT+ says yes. Saving over a factory sound makes your own copy; erasing it brings the factory sound back. On an empty place, hold SAVE opens the NAME screen and stores a new patch.
   **Tip:** a short press of SAVE opens the page that fits where you are: PATTERNS when you are on a sequencer page (STEP, GRID, AUTOMATION), otherwise MULTIS in LAYER and PATCHES in DUEL and STEREO. SELECT moves between PATCHES, PATTERNS, MULTIS and TOOLS. Hold SAVE on PATTERNS or MULTIS saves over the slot you picked ("SAVE OVER <name>?", OCT+ yes); an empty multi slot asks for a name first.

## Saving things
- **PATCHES**: one list of factory and your own sounds (up to 48 stored patches; the card shows FREE nn). Knob 2 filters by type, knob 3 marks favourites, knob 4 erases. Saving needs the transport stopped.
- **PATTERNS**: 16 named, overwritable step patterns.
- **MULTIS**: 16 slots that hold both parts, effects, arps, patterns, mode and morph settings. M01 to M10 are factory multis.
- To start from scratch (erases settings, patches, multis, patterns; keeps the FM6 voice bank): MENU (hold HOME) > SYSTEM > ERASE DATA. A reinstall alone keeps your data.
- The device backup (installer page) does not include MULTIs or PATTERNs. Back up before you update. This beta's patches, MULTIs and PATTERNs may not load in other betas.

## Playing and modulation
- **ARP**, **SEQ**, **PLAY/STOP**, **REC**: each part has an arp; the SEQUENCER arp mode plays a 16-step pattern. REC arms recording on the sequencer pages (STEP, AUTOMATION) and in the SEQ layer; on every other page a tap of REC is the HOLD switch for the part or parts the keys play (both in LAYER; its light is bright when HOLD is on). While a part is armed for recording, a red dot and REC show in the bottom row of every page, next to the page number. PLAY/STOP lights green when stopped and red when playing (it blinks red on each beat of the count-in). When stopped, a short press of PLAY/STOP starts on its release; holding it for 0.6 s (or pressing it twice) is STOP ALL.
- Two envelopes, two LFOs and an 8-slot modulation matrix per part. ENV 1 is the amplitude envelope, ENV 2 the filter envelope.
- **Quick edit:** hold **EDIT**, **LFO**, **ENV**, **ARP** or **GLO** for a moment and the four knobs become performance controls (header QUICK FILTER, QUICK LFO, QUICK ENV, QUICK ARP, QUICK GLO with a small bolt; the header shows the one part they edit). EDIT: cutoff, resonance, filter mode, release. LFO: each LFO's rate and its amount on the cutoff. ENV: attack, release, the filter envelope's amount and decay. ARP: mode, rate, gate, octaves. GLO: mute and level of part 1 and part 2, with the level meters, also over PERFORM. In every quick layer the keys still play. Let go and you are back on your page; a short press opens the page as always.
- **FX**: insert, per-part EQ, per-part **ducker** (sidechain from a MIDI note, a built-in pulse or the other part), chorus, delay (with ping-pong), reverbs. SHIM and RESO reverbs come from JIANT.

**PERFORM.** Hold **FX** for a moment (or hold FX and press a white key) to open the PERFORM page. Now the 16 white keys are effects that last while you hold them, four per group of keys: REPEAT 1/8, 1/16, 1/32 and REVERSE; TAPE STOP, FREEZE, OCT UP and OCT DN; FLANGER, PHASER, DRIVE and PUMP; ROLL A and ROLL B (a new MORPH end, as on the MORPH page), MUTE 1 and MUTE 2. The black keys still play notes. Knob 1 is MORPH, knob 2 SPACE (left a smaller room, right a huge one), knob 3 FILTER (left low-pass, right high-pass, OFF in the middle), knob 4 DEPTH (how strong the held effects are). Here ALGORITHM picks PART 1, BOTH or PART 2, the same setting as on the MORPH page (PRESETS changes the sound of that part, or of both, each from its own sound), and SELECT is BALANCE between the two parts. Knob 2 SPACE is the same SPACE as the third knob of the MIXER page. Tap FX to go back to where you were.

## MIDI
Part 1 listens on channel 1 and part 2 on channel 2 (editable). In the emulator, a WebMIDI keyboard or controller sends notes and controllers (the browser asks for MIDI access; clock is not passed on). On the device: clock in, program change and bank select, the mod wheel (CC1) moves the MORPH fader and is also a matrix source. CC 20 morph, 21 roll A, 22 roll B, 23 undo, 25 amount, 111 morph target, 112 mode. In LAYER, program change 0 to 15 on part 1's channel recalls MULTIs M01 to M16. Not yet tested with a range of real MIDI devices.

## Next
[TESTING.md](TESTING.md) lists what to try and report. [TROUBLESHOOTING.md](TROUBLESHOOTING.md) if something misbehaves.
