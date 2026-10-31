# DUELL 1.1

**DUELL**
**MultiMorphing Bitimbral Synthesizer**
**by OSCILL/R**

DUELL 1.1 is an update for the M-VAVE FM-1. The MORPH rolls on from where it is, a chord can be strummed, parameter locks are recorded by turning a knob, and many pages have been redrawn so you see more at a glance.

## What is new

**LAYER, SPLIT, STEREO**
- The part selector starts on BOTH on a new unit.
- SPLIT starts on the middle key: turn it up from OFF and the first step lands on F#4.
- PERFORM SELECT is a balance between the two parts. The middle is exactly the levels you set, turning to one side lowers the other part, and turning back returns to the middle. The position is kept after you leave PERFORM.
- TRANSPOSE is one setting, from -24 to +24 semitones, saved with the patch (VOICE 1 / 2, K4).

**MORPH and the smart randomizer**
- ROLL is chained: each roll continues from the sound that end has now. UNDO goes back up to two rolls.
- End B is saved and loaded exactly as it sounds. You can edit B and it is saved with the patch.
- SMART never rolls a switch (slope, engine choices, ARP mode, octaves and rate, LFO shapes, whole-note pitch values, TWIN link). In the mod matrix SMART rolls the amounts that are already set; WILD may wake the zero ones.
- SMART now also rolls smooth values that sit at zero, such as SUB level, key tracking and the TWIN FILTER depths.

**Modulation**
- The matrix and the CYCLE, SUB and TWIN MOD cards show each value as it sounds at the current morph position.
- Matrix names are written out in full, with SPACE as its own category.

**Effects**
- The new SENDS page shows each send in big values, without a gap before the unit, and shows which sends are inactive.
- The new MODULES page is a 2 by 2 page for the modules of your sound.
- REVERB, CHORUS, SHIMMER, DELAY, DUCK, VOICE and TEMPO are drawn as big cells that fill the screen.

**MIDI and play**
- Program change shows "#NN NAME" in the header, in the colour of the part.
- The header capsule shows the available voices in POLY, and M, L or U in MONO, LEGATO and UNISON.
- The quick pages GLO, SEQ, REC, ENV, LFO and EDIT look and behave the same in PERFORM as outside it. While you hold one, the white keys follow that page.
- STRUM on the CHORD page (K3): OFF, or 1 to 100 ms between the notes of a chord, saved with the patch.
- On SCALE and CHORD a keyboard shows the notes. In chord mode the chord lights on the screen and on the key LEDs, and the notes of the scale are dimmer so the chord stands out.

**Sequencer**
- Parameter locks are recorded by turning a knob while REC is armed. The PARAMETER LOCKS page is the list where you change, delete and switch what was recorded.
- New tools TRANS + and TRANS - move the whole pattern by one semitone. SHIFT < and SHIFT > move it in time.
- Swing runs from 50 to 75 percent on ARP, QUICK SEQ and TEMPO.
- QUICK REC has four cells: CLICK, COUNT, LEVEL and CLEAR.

**The rest**
- New area pages and card layouts for ARP, SCALE, CHORD, PERFORM, TOOLS, PATCHES, MULTIS and PATTERNS.
- A page with nothing to adjust is skipped.
- Choice cards show a segment line at the place of the chosen option.
- Units are written without a gap: 12dB, 40%, 400Hz.
- PATCHES, MULTIS and PATTERNS have a SAVE knob.

## Install
Open the installer page in **Chrome or Edge** on a computer: <https://oscillr.github.io/DUELL/install/>. Tick the checklist, save a backup, connect the FM-1, press Install and wait for Done; then unplug and replug the unit. Make a new backup with 1.1. Steps and details: [docs/INSTALL.md](https://github.com/oscillr/DUELL/blob/main/docs/INSTALL.md).

The web editor (Chrome or Edge, unit connected over USB): <https://oscillr.github.io/DUELL/editor/>.

## Credits
Special thanks to: Felucca and its author Leo Kuroshita, JIANT (reverbs), CHOMPI Club (plate reverb and delay ideas), Emilie Gillet of Mutable Instruments (reverb design), aquanodemusic (a CC0 FM voice), Baud Girl (for starting it all), Charles Vestal (X0X, Velo), keremimo (Melodee), Quixotic7 (ChoralRoot), zvenson (zp12), isod89 (SLOOP), and the whole community.
