# DUELL quick start

**DUELL**
MultiMorphing Bitimbral Synthesizer
by OSCILL/R

DUELL turns the FM-1 into two synths in one box. This page is the short tour; [SPEC.md](../SPEC.md) is the full design. DUELL 1.3 runs on the FM-1 ([INSTALL.md](INSTALL.md)) ([web emulator](https://oscillr.github.io/DUELL/emu/)).


## The idea
- **Two parts**, 1 and 2. Each has its own sound, effects, EQ, ducker and arp. The **ALGORITHM** knob picks which part you are editing.
- **Three modes** (SETTINGS > PLAYING > MODE):
  - **DUEL**: part 1 hard left, part 2 hard right, kept apart, effects included. For playing two sounds into two channels.
  - **STEREO**: both parts in one stereo field, for headphones.
  - **LAYER** (default): both parts play on the keys together. SETTINGS > PLAYING has XFADE (velocity crossfade) and SPLIT (a key split point) for it.
- **MORPH**: every sound has two ends, A and B, and one fader between them.

## The first start
The first time DUELL starts, SETUP asks two things. **SETUP 1/5:** what you play DUELL with: KEYS OR OTHER MIDI, CIRCUIT TRACKS or MPC/FORCE (this sets the MIDI PRESET). **SETUP 2/5:** the mode, with the one for your device recommended. Turn any knob or SELECT to choose, **OCT+** goes on, **OCT-** goes back, **HOME** skips. Three short GUIDE pages follow. The keys play while it is shown. To see the guide again: SETTINGS > SYSTEM > GUIDE.

## The controls in one minute
- **SELECT** switches sub-pages (and moves rows in lists). It never changes the tempo. **Knobs 1 to 4** adjust the four columns on screen. Every control does what its label says.
- **PRESETS** loads sounds for the selected part. A sound brings its own loudness (its **PATCH LEVEL**, saved with the patch, on EDIT > VOICE 2/2 knob 4); your volume on the mixer (the **LEVEL** cards of the GLO MIXER page, and MIDI CC7) is yours: loading a sound never changes it, and a multi remembers it. The part plays at the patch level and the level together.
- **HOME** opens the MORPH page; its round is MORPH, MACROS 1/2, MACROS 2/2. Hold HOME for SETTINGS (SYSTEM > ABOUT shows the version and credits).
- **MIXER** (GLO's first page): both parts side by side, part 1 left and part 2 right, with the level meters between them. Knob 1 is LEVEL 1, knob 2 PAN 1, knob 3 LEVEL 2, knob 4 PAN 2 (in DUEL the pans are fixed and not shown). Each part's cell reads MUTE and is lit in the part's colour while the part is muted; hold GLO and press **OCT-** or **OCT+** to mute or unmute part 1 or part 2. SPACE is on the PERFORM page.
- **TEMPO** is on the GLO TEMPO page, the one after MIXER (a tap of GLO opens MIXER first). To set the levels and pans of both parts from any page hold GLO: the keys play notes, knob 1 is LEVEL 1, knob 2 PAN 1, knob 3 LEVEL 2, knob 4 PAN 2 (in DUEL the pans are fixed and not shown). With GLO held, **OCT-** mutes or unmutes part 1 and **OCT+** part 2; each cell reads MUTE, lit in the part's colour while muted.
- **OCT-/OCT+** change octave when you let go of them; in dialogs OCT- means back and OCT+ means do it. Hold **OCT-** and turn a knob to reset that knob's card to its default (the octave then does not change).

## SETTINGS
Hold **HOME**. SETTINGS opens on its seven sections: **PLAYING** (MODE, XFADE, SPLIT in LAYER, MULTI LOAD, KEY VELOCITY, AFTERTOUCH, TUNE), **RECORDING** (CLICK, CLICK LEVEL, COUNT-IN, STEP KEYS, STEP PREVIEW), **MORPH** (SMART, ARP, PITCH), **PERFORM** (PERFORM KNOB 2 to 4, QUICK MACRO KNOB 1 to 4, LEAVING PERFORM), **MODULES**, **MIDI** (PART 1 CHANNEL, PART 2 CHANNEL, MIDI IN, MIDI PRESET, MIDI LEARN, SIDECHAIN) and **SYSTEM** (COLOR, LIGHTS, SCALE LIGHTS, ANIMATION, SCREEN OFF, NOTES LEFT, KNOB ACCELERATION, BUTTON HOLD, HOME OPENS, RESTORE LAST, GUIDE, ABOUT, ERASE DATA). The same rule on every page: **SELECT** moves, **OCT+** opens, **OCT-** goes back, the knobs change values; HOME closes SETTINGS. TUNE is the frequency of A4, 415.3 to 466.2 Hz in steps of 0.1 Hz, 440.0 at first. **BASS MONO** is a sound setting, on GLO's **STEREO MASTER** page (KNOB 1; 200 Hz on a new unit): the output is mono below the frequency you pick (40 to 400 Hz) so wide effects do not smear the bass; it belongs to the unit, works in STEREO and LAYER, and is not shown in DUEL.
- Defaults worth knowing: KNOB ACCELERATION ON, CLICK REC (the metronome only while you record on the sequencer pages). MULTI LOAD KEEPS MODE: loading a multi keeps your MODE, XFADE and SPLIT; set SETS MODE to take the multi's.

## Make a first sound
1. Press **PRESETS** and turn it to hear sounds. Press **ALGORITHM** to select part 2 and load another.
2. Hold HOME, open PLAYING and try MODE: DUEL, STEREO, LAYER.
3. Press **HOME** for the MORPH page. Knob 1 moves the morph from A to B. Knob 2 rolls a new random end with one click (turn left = A, right = B). Knob 3 undoes the last roll of A or B. Knob 4 is AMOUNT, the one percentage: how far a roll may go. On this page **ALGORITHM** has three positions, PART 1, BOTH and PART 2: with BOTH the morph moves both parts and both part numbers in the header light up.
4. Press **SAVE**. The first page is **PATCHES**: 128 slots, #1 to #128, each with a patch. **Hold SAVE** asks "OVERWRITE #12 <name>?" (the slot your sound came from); turn **knob 1** in the dialog to pick another slot, turn **knob 2 (RENAME)** to give the sound a new name first (the NAME screen opens; OCT+ there brings you back to the dialog with the new name), OCT+ says yes. Whatever was in that slot is replaced. Knob 4 **RESTORE** takes your sound out of the slot again and brings back the patch the slot came with.
   **Tip:** a short press of SAVE opens the page that fits where you are: PATTERNS when you are on a sequencer page (STEP, GRID, AUTOMATION), otherwise MULTIS in LAYER and PATCHES in DUEL and STEREO. SELECT moves between PATCHES, PATTERNS, MULTIS and TOOLS. Hold SAVE on PATTERNS or MULTIS saves over the slot you picked ("SAVE OVER <name>?", OCT+ yes); an empty multi slot asks for a name first.

## Saving things
- **PATCHES**: one list of 128 slots. A slot holds the sound you saved there, or else the patch it came with. Knob 1 steps the slots, knob 2 filters by type, knob 3 marks favourites, knob 4 RESTORE brings back the patch the slot came with (the card is not shown when the slot holds it already). **SAVE > TOOLS** has RESTORE ALL (every slot gets the patch it came with). MIDI program change n loads slot n. Saving needs the transport stopped.
- **PATTERNS**: 32 named, overwritable step patterns (P01 to P32).
- **MULTIS**: 16 slots that hold both parts, effects, arps, patterns, mode and morph settings. M01 to M15 are multis to start from; M16 STRESS TEST is a CPU test, not music, and free to overwrite. Loading a multi sets its own levels, pan, mute and sends and the effects it stores (chorus, delay, reverb with their settings), whatever the mixer was; it leaves your MIDI settings, the clock source, TUNE and the master effect BASS MONO as they are (BASS MONO is a setting of your unit, not of the multi).
- To start from scratch (erases settings, patches, multis, patterns; keeps the FM6 voice bank): SETTINGS (hold HOME) > SYSTEM > ERASE DATA. A reinstall alone keeps your data.
- **WAVE user tables:** WAVE's TABLE knob has three places after its own tables, USR1 to USR3, named EMPTY until you fill them (an empty one plays a sine). You make a table in the editor from a WAV file (a single-cycle wave, or a wavetable file like the ones Serum and Vital export) and send it to a slot; the unit keeps it, ERASE DATA erases it, an update does not. **A user table is reduced to 19 frames and 64 harmonics and the phases are not kept**, so it sounds like the file's spectrum, not exactly like the file. A patch that uses USR2 on a unit where USR2 is empty plays a sine. The editor's WAVETABLES tab does this; see [EDITOR_LIBRARIAN.md](EDITOR_LIBRARIAN.md).
- **Patch library in the editor:** the editor's PATCHES tab exports one patch, a selection or the whole bank to a file, imports patch files and **DX7 `.syx` files** (each voice becomes an FM6 patch) with a preview and your choice of slots, copies and swaps slots by dragging, and loads a dropped patch into a part without saving. The WAVETABLES tab takes dropped WAV files and shares tables as small files. See [EDITOR_LIBRARIAN.md](EDITOR_LIBRARIAN.md).
- The device backup (installer page, and the editor's Backup page) holds patches, MULTIs, PATTERNs, the user wavetables, settings and the current music. **Back up before you update.**

## Playing and modulation
- **ARP**, **SEQ**, **PLAY/STOP**, **REC**: each part has an arp; the SEQUENCER arp mode plays a pattern of 1 to 64 steps (16 in a new pattern). REC arms recording on the sequencer pages (STEP, AUTOMATION) and in the SEQ layer; on every other page a tap of REC is the HOLD switch for the part or parts the keys play (both in LAYER; its light is bright when HOLD is on). While a part is armed for recording, a red dot and REC show in the bottom row of every page, next to the page number. PLAY/STOP lights green when stopped and red when playing (it blinks red on each beat of the count-in). When stopped, a short press of PLAY/STOP starts on its release; holding it for 0.6 s (or pressing it twice) is STOP ALL: it silences everything, the delay, chorus and reverb tails too (a single press of STOP leaves the tails to ring out).
- Two envelopes, two LFOs and an 8-slot modulation matrix per part. ENV 1 is the amplitude envelope, FILTER ENV the filter envelope.
- **Filters:** the engines' own filter (ANALOG's VCF, TRIO FILTER, NOISE FILT) has its page in EDIT, and every engine has the multimode filter after it: **MULTI FILTER 1/2** is CUT, RES, MODE and SLOPE; **MULTI FILTER 2/2** is KTRK (the cutoff follows the note you play, -100 to +100 %; a new or initialised sound and most patches start at +51 %, the basses, percussion and kicks at 0), and where FILTER ENV, LFO 1 and LFO 2 drive this filter their three amounts (FILTER ENV, LFO 1, LFO 2); the same values are on the DEST pages of those modules (FILTER ENV DEST, LFO 1 DEST, LFO 2 DEST).
- **Quick edit:** hold **EDIT**, **LFO**, **ENV**, **ARP** or **GLO** for a moment and the four knobs become performance controls (header QUICK MULTI, QUICK LFO, QUICK ENV, QUICK ARP, QUICK GLO with a small bolt; the header shows the one part they edit). EDIT: cutoff, resonance, filter mode (the multimode filter, the same on every engine), release. LFO: each LFO's rate and its amount on the cutoff. ENV: attack, release, the filter envelope's amount and decay. ARP: mode, rate, gate, octaves. GLO: level and pan of part 1 and part 2 (the mutes on OCT- / OCT+), with the level meters, also over PERFORM. In every quick layer the keys still play. Let go and you are back on your page; a short press opens the page as always.
- **FX** (the part's own pages): **SEND FX** (its distortion and its sends to the chorus, delay and reverb), **INSERT FX** (TAPE, VINYL, FOLD, HARD, CRUSH and more), **EQ** (low, mid and high band; a band at 0 dB is off) and **DUCK** (SRC: OFF, PUMP, the other part, or a MIDI note). The chorus, delay and reverb belong to both parts and are edited on **GLO**: CHORUS (stereo in STEREO and LAYER), DELAY 1/2 and 2/2 (MODE DIGITAL, PING-PONG, RANDOM or OCT UP), REVERB (ROOM, SPRING, HALL, SHIMMER, RESONATOR, PLATE, with SIZE, DAMP and LOCUT), SHIMMER (with REVERB TYPE SHIMMER: PITCH UP or DOWN and DRIFT). The SHIMMER and RESONATOR reverbs come from JIANT.

**MACROS.** HOME's round is MORPH, MACROS 1/2 and MACROS 2/2: TIMBRE, ENV MACRO, FILTER MACRO, CHARACTER and MOTION, with MORPH, SPACE and KILL. Each turns several sound settings at once; the middle is always the sound as saved, and loading a sound or a multi puts the macros back to the middle. Hold **SEL** for QUICK MACRO: the four knobs become the four macros chosen in SETTINGS > PERFORM. MIDI CC 56 to 62 move them too (64 is the middle).

**PERFORM.** Hold **FX** for a moment (or hold FX and press a white key) to open the PERFORM page. The 16 white keys are effects that last while you hold them, four per group of keys: REPEAT 1/8, 1/16, 1/32 and REVERSE; TAPE STOP, FREEZE, OCT UP and OCT DN; FLANGER, PHASER, DRIVE and PUMP; ROLL A and ROLL B (a new MORPH end), MUTE 1 and MUTE 2. The black keys play notes in the scale. **KNOB 1 is DEPTH** (how strong the held effects are); **KNOBS 2, 3 and 4 are yours**: choose them in SETTINGS > PERFORM (MORPH, SPACE and KILL to start with). ALGORITHM picks PART 1, BOTH or PART 2, and SELECT is BALANCE between the two parts. Tap FX to go back: SPACE and KILL fade back to neutral, unless SETTINGS > PERFORM > LEAVING PERFORM says KEEP.

**MODULES.** SETTINGS > MODULES has four switches that turn whole parts of DUELL off for the unit: ARP, SEQ, SENDS and PART FX, each ON (the default) or OFF. SENDS OFF (no chorus, delay or reverb) saves a lot of CPU. They are kept when you power off and are not part of a patch or a multi.

**SIDECHAIN.** SETTINGS > MIDI > SIDECHAIN sets the MIDI note that ducks each part whose DUCK source is MIDI: PART 1 CHANNEL and NOTE, PART 2 CHANNEL and NOTE (channel 10 and C2 to start with). They belong to the unit; a patch or a multi never changes them.

**COMPRESSOR.** A compressor on the whole output, the GLO page after STEREO MASTER: RATIO (OFF to INF), THRESH, ATTACK and RELEASE. It starts at RATIO OFF, and the other three appear when you turn RATIO up. The makeup gain is automatic. It works in STEREO and LAYER and belongs to the unit, not to a multi.

**STEREO MASTER.** BASS MONO (KNOB 1) and WIDTH (KNOB 2, 0 to 200 %: 100 % is off, 0 % is mono; the bass is never spread). Both belong to the unit and work in STEREO and LAYER.

**Piano roll.** In the web editor each part has a PIANO ROLL tab for its step pattern. See [EDITOR_ROLL.md](EDITOR_ROLL.md).

## MIDI
Part 1 listens on channel 1 and part 2 on channel 2 (SETTINGS > MIDI). **MIDI PRESET** sets DUELL up for KEYS OR OTHER MIDI, Novation Circuit Tracks or Akai MPC / Force. Clock in, program change and bank select; MIDI notes follow the part's TRANSPOSE; the mod wheel (CC1) moves the MORPH fader and is also a matrix source. CC 70 morph, 102 roll (0 to 42 rolls End A, 85 to 127 rolls End B), 103 amount, 56 to 62 the MACROS. In LAYER, program change 0 to 15 on part 1's channel recalls MULTIs M01 to M16. MIDI LEARN is in SETTINGS > MIDI.

## Next
[TESTING.md](TESTING.md) lists what to try and report. [TROUBLESHOOTING.md](TROUBLESHOOTING.md) if something misbehaves.
