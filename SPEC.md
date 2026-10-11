# DUELL: specification (draft 1, 2026-10-10)

Bitimbral polyphonic synth firmware for the M-VAVE FM-1. Fork of Felucca (GPL-3.0, so DUELL is GPL-3.0). Developed and auditioned in the browser emulator first; flashed to the device only when every feature is done.

Items marked **[verify]** are assumptions about Felucca's code that must be checked before building on them. Items marked **[decide]** need an answer from the owner.

---

## 1. Design rules (these override everything below)

### 1.1 The label law
Every control does what its printed label says, on every page, in every mode.

- A label is never reused for something else. A control that has no honest job in DUELL does nothing and the screen says so.
- Felucca breaks this in one place that DUELL fixes: SELECT sets the tempo. DUELL keeps two Felucca habits by the owner's choice: ALGORITHM selects the track (here: the part), and OCT-/OCT+ mean "back / do it" in dialogs and menus.
- Holding a button may show a temporary version of the same function (for example hold FX = performance effects). A hold never changes the meaning of the label.
- No hidden state changes what a control does. The meaning of KNOB 1-4 is always on screen, in the part's colour.

### 1.1a Navigation principle (owner, 2026-10-10)
**SELECT selects, the knobs adjust.** One method of navigation, everywhere:
- SELECT turns through whatever there is to select: the sub-pages of a screen, the rows of a list, the rows of the menu **continuously across its tabs** (after the last row of a tab it goes on to the first row of the next tab; it stops at the very ends of the menu and never jumps back to the top).
- KNOB 1-4 adjust the values on the screen shown. In the menu, any of the four knobs adjusts the selected row (a knob with no dedicated job does the same as the others). **Where the screen marks a knob with a job, it keeps that job**: on the SAVE lists KNOB 1 is marked as the row picker and stays so, and SELECT turns the SAVE sub-pages there.
- ALGORITHM and PRESETS never navigate pages, tabs or rows. They always do their own job (ALGORITHM: the part; PRESETS: the sound).
- OCT-/OCT+ keep their octave job, and may step a value or answer a dialog as in Felucca, but no screen advertises them as the way to navigate or adjust. On-screen hints name SELECT and the knobs.

### 1.2 Fixed panel map (hardware, from Felucca's control chart)

| Control | DUELL function (label = job) |
| --- | --- |
| MASTER | Output volume. Always. |
| SELECT (knob) | Turn left / right: previous / next sub-page of the screen shown, on every screen, always. In lists and menus it moves the row. Never tempo, never anything else. |
| PRESETS | The selected part's patch (a sound). On HOME and on SAVE > PATCHES. |
| ALGORITHM | Selects the part, A or B (Felucca's track select; kept by the owner's choice). Works on every page. |
| KNOB 1-4 | The four columns of the page shown. |
| FX | Effects page (per-part insert + global sends). Again = next sub-page (as Felucca, for now). |
| SCL | Scale, root and chord keys of the selected part (Felucca's SCL page: SCALE, ROOT, CHORD, VOICING). The button is printed SEL on the hardware; the spec calls it SCL as Felucca does. |
| ENV | Envelope pages. |
| LFO | LFO pages and modulation matrix. |
| EDIT | **First sub-page: ENGINE** (KNOB 1 ENGINE, KNOB 2 SOUND within the engine, KNOB 3 FAV, KNOB 4 INIT), then the engine's parameter pages. Engine selection is never hidden behind a hold: Felucca's hold-EDIT engine keys are gone (decided 2026-10-10); the hold-EDIT layer keeps KNOB 1-3, INIT on the first white key (F3) and the engine's quick actions on the black keys. |
| GLO | Global. A tap opens the **main GLO page** (the parts' LEVEL on KNOB 1 and 2, SOLO / MUTE / ALL / TAP on the keys, a level meter per part); it is a page, not a held layer (since 2026-10-12). Tapped again / SELECT: **all the MIDI settings, and only here (MENU has none)**: CHANNELS (P1 CH, P2 CH, MIDI IN), SIDECHAIN (SC SRC, SC NOTE), MIDI LEARN (LEARN, CLEAR), then CLOCK (tempo, swing, clock source), then **TEST** (the last page: the CPU test, section 6.1; K1 MORPH, K2 ROLL, K3 LOAD MULTI, K4 RESET). GLO held + PLAY = restart from the top. |
| HOME | Home screen. Hold = menu. |
| SAVE | Everything that is saved: presets, patches, the morph. Hold = undo last load. |
| ARP | Arpeggiator of the selected part (as Felucca's), plus ARP TARGET (section 2.3). |
| SEQ | Opens the step pattern of the arp's SEQUENCER mode: 16 steps, each with gate, accent and a note offset (scale degrees, or semitones when no scale) (section 2.3). |
| PLAY/STOP | Start/stop the arp and tempo-synced modulation; follows MIDI start/stop. **Pressed twice (second press within 300 ms) or held 0.6 s: STOP ALL** (owner, 2026-10-12): the transport stopped and reset, both parts' arps and patterns off with their latches / holds cleared, every held and releasing voice faded out within about 3 ms (effect tails run on), the keys' and MIDI's held-note state cleared, All Sound Off (CC120) and All Notes Off (CC123) sent on both parts' channels (DUELL sends no MIDI clock, so there is no transport stop to send), and "STOP ALL" shown for about 0.8 s. A single short press is the plain toggle and acts at once (the second press upgrades it); a hold acts once while the button is down and the release does nothing more. Code: `ui_input.c play_stop_all / play_hold`, `seq.c stop_all` (transport_req 4); help: HOME's hint `[PLAY]x2: STOP ALL`; test: midi_control_test `stop_all_test`. |
| REC | Latch the held notes into the arp. |
| OCT-/OCT+ | As in Felucca: octave down / up of the keys, both together = reset; in action pages, dialogs and menus: back / do it. |
| Keys | Play the selected part (or both, see GLO > KEYS). |

### 1.3 Consistency rules
1. Same page = same four columns, always. A page with more than four parameters has sub-pages, reached with the SELECT knob or by pressing the page button again (as Felucca, kept for now; to be reviewed after trying it in the emulator), with a dot indicator.
2. Dialogs and menus follow Felucca: OCT- = back, OCT+ = do it, with an on-screen hint.
3. The tempo is one parameter, on GLO's CLOCK page, KNOB 1, labelled TEMPO (built in step 2). The only other way to set it is the key labelled TAP on the main GLO page (tap tempo, internal clock only; the tempo itself is on GLO's CLOCK page; GLO's pages in order: GLO, CHANNELS, SIDECHAIN, MIDI LEARN, CLOCK, TEST), which says what it does. With external MIDI clock it shows EXT and the knob is inert. No other control changes the tempo.
4. Part colours: see section 5a (each part has its own colour in every palette; the whole screen takes the colour of the selected part).
5. Every page in the emulator is screenshot-tested against a layout lint (Felucca already has this; DUELL's pages join it).

---

## 2. Architecture

### 2.1 Two parts
- Two parts, A and B. Each part has its own engine, preset, level, pan, 2 LFOs, 2 envelopes, matrix, insert effect, arpeggiator and MIDI channel.
- Felucca has 4 tracks and 8 voices shared. DUELL keeps the 8-voice pool in the first build and splits it per part after the CPU measurement (below). **[verify]** how Felucca allocates voices and how many engines can run at once.
- Felucca's engines are available on both parts: ANALOG, FM6, PHASE, LOFI, SAMPLE, VOICE, TRIO, WHEEL, GRAIN, PHYS, NOISE, SLICE. **DRUM is removed** (DUELL is a synthesizer, not a drum machine): hidden the way Felucca hides its retired DIGITAL engine, so stored data layouts stay valid. **SLICE stays** (owner's decision). NOISE stays (owner: "very cool"). **[decide later]** after measuring, heavy engines (FM6, PHYS, GRAIN) may get a per-part voice limit. This is a measurement result, not a promise.
- No drum/sequencer tracks in the first build: the product is two synth parts played from the keys or from any MIDI source (sequencer, DAW, keyboard, controller).

### 2.2 Stereo / Split mode (superseded by section 5b.1: three modes SPLIT, STEREO, LAYER, on HOME sub-page 2)
- **SPLIT** (default): part A is hard left, part B is hard right. Each part's own pan and stereo spread are forced to the centre of its side (stereo effects fold to the part's side). This is the dawless mode.
- **STEREO**: both parts are full stereo; each has PAN and WIDTH. For headphones and for playing a single sound.
- Switching is click-free (50 ms crossfade of the pan law).

### 2.3 What SEQ, PLAY/STOP and REC do (decided: option A)
Felucca's sequencer is not part of the product, but the three buttons exist and may not lie. Options considered:

| Option | SEQ | PLAY/STOP | REC |
| --- | --- | --- | --- |
| **A (chosen)** | Per-part 16-step pattern of the arpeggiator (a rhythm/gate pattern for the ARP). | Start/stop the arp and tempo-synced modulation (restart from the top). Follows external MIDI start/stop. | Record the played notes into the arp's held notes (latch). |
| B | Full Felucca sequencer, trimmed to 2 parts. | Transport. | Record. |
| C | Unused, screen says "NOT IN DUELL". | Start/stop arp only. | Unused. |

#### Arp, chord keys and scale (SCL, ARP, SEQ)
- **The arp is Felucca's arp**, per part (the part picked with ALGORITHM): modes, rate, gate, octaves, swing, latch. **[verify]** against `seq.c`.
- **Chord keys (SCL > CHORD, VOICING)** are kept as in Felucca (`chord.c`). With chord keys on, one held key becomes a chord built from the part's SCALE and ROOT (diatonic triad or seventh, or a fixed shape), and **the arp plays those chord notes**. So one held key gives a scale-aware arp. With chord keys off, the arp plays the keys you hold.
- **SEQUENCER is an arp mode** (decided): the arp's MODE list is SEQUENCER, UP, DOWN, UP-DOWN, ... with **SEQUENCER first**, so it is not missed. Choosing it makes the arp play the 16-step pattern: each step has gate (on/off), accent and a note offset (scale degrees when a scale is set, semitones when CHROMATIC), applied to the held or chord notes. The SEQ button opens the pattern editor of the selected part (16 steps, KNOB 1-4 = the selected step's gate, accent, offset, length). The pattern is saved with the preset.

#### ARP TARGET: which parts of the sound the arp moves (new)
A voice has one root pitch (the held key) and an arp pitch (the note the arp plays now). ARP TARGET chooses, per part, which of the engine's **pitch units** follow the arp pitch; the rest keep following the root pitch.

- **Pitch units per engine** (each engine declares its own; an engine with no separable units has ALL only). Checked against the code (appendix A): FM6 = OP1..OP6; TRIO = OSC1..OSC3; ANALOG = OSC A and OSC B only (the SUB is OSC B an octave down); WHEEL = 9 partials; PHASE = main and SUB. **ALL only**: LOFI, SAMPLE, VOICE, GRAIN (one pitch unit). **No arp target at all**: DRUM and SLICE (the note picks a lane or slice, not a pitch). Two reads of the root pitch stay on the root by design: ANALOG's filter key tracking and VOICE's formant tracking. ANALOG's OSC B ignores unison detune, TUNE and bend today (it drops `m->fine`); fixing that is part of the arp-target work.
- **ARP TARGET** is a bit mask on the ARP page: ALL (classic arp), or any combination of units. Set on the ARP page, KNOB 4 steps through the combinations; the screen shows the units as a row of boxes, lit = follows the arp.
- **Behaviour**:
  - Mask = ALL: the normal arp (a new note-on per step).
  - Mask = some units: **one voice per held key** keeps sounding while the key is held; each arp step changes the pitch of the masked units only (with the part's glide setting, so a step can jump or slide), and the unmasked units stay on the root. In FM6 this means a modulator can step through the arp while the carrier holds the root: the pitch stays, the timbre moves. The arp step is also a matrix source (STEP) so the user can shape the movement.
  - Gate and accent from the arp still apply as a pulse on the masked units' level, so rhythm remains audible.
- **Engineering [verify]**: voices get a second pitch (`arp_pitch16`) next to `pitch16` (core.h); each engine's note computation takes the pitch per unit. FM6 currently computes all operator frequencies from one value (`fm6_note_logfreq`, called once per block in `eng_fm6.c`); it needs one value per operator. TRIO already derives osc 2/3 from the base pitch (`trio_inc`). Cost is a few extra frequency lookups per voice per block. No extra voices.
- Tests: with mask = ALL the render is bit-identical to the plain arp (golden hashes); with a partial mask the unmasked units' frequencies are measured to stay on the root over 100 arp steps.

### 2.4 Modulation (more than Felucca)
Felucca has per track (checked in `core.h` / `params.c`): **1 ADSR envelope** (with FLT / PIT / SHP depth amounts), **1 LFO** (SIN TRI SAW SQR S&H, 4 destination amounts, rate or tempo sync, NOTE/FREE trigger, BI/UNI, fade), and a **4-slot matrix** (sources LFO ENV VEL KEY RAND MODW AT EXPR S&H SLEW; destinations PITCH CUT SHP AMP PAN DIST CHO DLY REV RATE VIB E1-E8 DEPTH). DUELL per part, as the owner asked for "another envelope and LFO":
- Envelopes: ENV 1 (as Felucca; shown as **AMP ENV**, see below) + **ENV 2** (new, same stages and ENV SYNC, also a matrix source).
- LFOs: LFO 1 (as Felucca) + **LFO 2** (new, same shapes, S&H, SLEW, trigger, polarity, tempo sync, also a matrix source).
- **Classic arrangement (owner, 2026-10-11):** ENV 1 is the **AMP envelope** (ATK DEC SUS REL shape every note's volume; ENV 1 DEST adds its FLT / PIT / SHP depths). ENV 2 is the **FILTER envelope**: page ENV 2 DEST with FLT (cutoff depth) and SHP (shape depth). LFO 2 gets page LFO 2 DEST with PIT (vibrato), FLT and SHP. Same units, ranges (-64..+63) and behaviour as ENV 1 DEST / LFO 1 DEST, added in the same place (voice.c track_render: ENV 2's level x FLT into the cutoff input, LFO 2's value (FADE, DEPTH2 applied) x PIT / FLT / SHP into pitch, cutoff, shape). Defaults 0 (every stored sound unchanged); new factory sounds set ENV 2 FLT (+40) themselves. ENV 2 PITCH and LFO 2 AMP (tremolo) stay matrix routes. Page order: ENV: AMP ENV, AMP DEST, ENV 2, ENV 2 DEST, MOD 1, MOD 2; LFO: LFO 1, LFO 1 SYNC, LFO 1 DEST, LFO 2, LFO 2 SYNC, LFO 2 DEST, MOD 1, MOD 2. Ids 114..118 (P_E2FLT P_E2SHP P_L2PIT P_L2FLT P_L2SHP; P_COUNT 127, the 7-bit maximum; stores load older ones by count). MORPH: the FLT / SHP depths roll as continuous values (like their ENV 1 / LFO 1 siblings, in the filter "no dead path" rule too), LFO 2 PIT is a pitch item (zero stays zero, PITCH ON lifts it); they draw from a stream of their own so the other values of a seed roll as before.
- **Naming (2026-10-11):** ENV 1's pages are titled **AMP ENV** and **AMP DEST** ("AMP ENV DEST" is 12 characters; a page title holds 11: the footer, ui_graph.c page_parts), the matrix source ENV1 is **AENV** (stored value 2 unchanged, its place in the list too: OFF AENV ENV2 LFO1 LFO2 ...). The printed button stays ENV (hardware); ENV 2 / ENV 2 DEST keep their names (series: AMP ENV, ENV 2). The help hints say AMP ENVELOPE / FILTER ENVELOPE. The automation list keeps "ENV ATK" etc. (a 5-character prefix does not fit its column).
- **SHP (the shape input) on every engine (2026-10-11):** the SHP depths (AMP DEST, ENV 2 DEST, LFO 1 / 2 DEST) and the matrix's SHP move each engine's timbre control, as an offset (centre = the knob as set, bit for bit: the goldens unchanged). ANALOG pulse width / sync detune, PHASE distortion depth, FM6 feedback, TRIO duty, NOISE colour, GRAIN position, FORMANT buzz (as before), and new: LOFI the pulse width around DUTY (smooth, +-12.5 % of the period as ANALOG's PW, 6.25..93.75 %; PLS and STEP's pulse; not the WRAM table, a stepped switch the morph would make a cliff of), PHYS STRC (SYMP: BUZZ, its structure input), SAMPLE DRV (its drive; CUT is FLT's), SLICE TONE (its only tone control, the same one FLT moves; DCAY acts before the render, where the shape input is not known), WHEEL brightness (the upper bars toward full and the sub bars toward silence, or the reverse, smooth, renormalised; PERC's silent 1' stays silent). The morph's new routes still add SHP only to the engines that had it (adding the five would change what a saved End B seed rolls into).
- **LFO polarity in the matrix (owner bug report, 2026-10-12: a sine LFO on PAN seemed to only go to one side):** per-voice destinations (PITCH +-11.8 st, CUT / SHP +-63 steps at AMT 63) were always centred on the base value, as the LFO 1 / 2 DEST pages (PIT FLT SHP, also centred; those pages have no PAN). AMP (tremolo, matrix and LFO DEST AMP) is deliberately one-sided: a bipolar LFO is read as 0..1 and AMT 64 swings the level from 0 up to the base, never above it; a negative AMT inverts the phase. The per-block destinations (PAN, sends, RATE, VIB, E1..E8) were the inconsistency: a bipolar source (LFO 1/2, S&H, SLEW, KEY, RAND) at AMT 63 moved them by +-the whole range, so on PAN (-64..63, base 0) the sine was clipped flat at both ends for most of the cycle and sounded like a pan that jumps right (or left at a negative AMT) and stays. Now a bipolar source swings +-half the range (its full swing = the whole range, so PAN at AMT 63 sweeps full left to full right around its own value, AMT -64 the same inverted); a unipolar source (ENV, VEL, MODW, AT, EXPR, MORPH, ENV 2, a UNI LFO) still moves it by the whole range one way. Remember TRIG NOTE restarts the LFO at PHS on each fresh phrase: a slow LFO on short notes only covers the start of its cycle (set TRIG FREE for a continuous sweep). No factory sound routes an LFO to a per-block destination, so none changed.
- Matrix: 8 slots per part (from 4). Sources: LFO 1-2, ENV 1-2, velocity, aftertouch, mod wheel, pitch bend, key, random-per-note, **MORPH**. Destinations: every knob parameter, plus LFO depth and rate (LFO to LFO, fixed evaluation order, one block of delay, no loops).
- CPU: all new modulation is computed per block, with per-sample smoothing only where needed. Cost is measured in the emulator's instruction counts (Felucca's `tests/cpu_baseline.txt` method) and re-measured on the device.

---

## 3. MORPH (the signature feature)

One fader, two random end states, one percentage.

### 3.1 Concepts
- **Original**: the patch as saved. The reference for all random changes.
- **End A** (fader at 0) and **End B** (fader at 127): two random variations of the original.
- **MORPH fader**: 0..127, stays where it is left. It is a CC, a matrix source, and a knob on screen. Output sound = the blend between end A and end B at the fader position.
- **AMOUNT**: the one percentage, 0-100%. There is no other randomness control.

### 3.2 How an end is rolled
For every parameter that is allowed to be randomised:
- **Continuous parameter** (level, cutoff, times, depths, ...): the new value is the original value plus a random offset, at most AMOUNT percent of the parameter's range, clipped to the range. At 20%, nothing moves more than 20% of its range from the original. The offset's size is drawn uniformly from 0 to AMOUNT (owner, 2026-10-11: replaces the bell-shaped draw, whose moves were mostly tiny), with a random sign, in the parameter's perceptual domain (log for frequencies and times); a move that would leave the range (or a safety limit) goes the other way instead, so nearly every rolled parameter moves. The EQ rolls only in WILD. The DUCK values (DEPTH at most 50 %, attack, hold, release, threshold) roll in SMART only while the ducker is on (DEPTH above 0) and in WILD always; its SRC, NOTE, RATE and CURVE never roll (rules 3g, 3h).
- **Switch / choice parameter** (waveform inside an engine, filter type, LFO shape; in WILD also matrix source/destination): AMOUNT percent chance that it differs from the original. If it does, a different valid choice is picked at random (never a different engine). At 20%, one in five switches changes.
- A roll never changes the original.

### 3.3 Smart rules (nothing "stupid")
1. **Everything that switches is faded, never flipped.**
   - Continuous parameters blend linearly (log for frequencies and times).
   - LFO shapes: both shapes run, their outputs are cross-faded along the fader.
   - Filter type: the state-variable outputs are mixed (LP/BP/HP), cross-faded.
   - Matrix routes: in SMART an assigned route keeps its source and destination (rule 3f), so only its amount moves. In WILD a route that differs between A and B fades its amount out and the other in.
   - Waveform / oscillator shape inside an engine (for example ANALOG's wave and PHASE's wave; **not** TRIO's wave, which would leave no source and click): both shapes are generated and cross-faded along the fader. Cheap, because it is one engine.
   - The engine itself is **never** changed by the morph.
2. **Parameters that cannot be faded are never randomised**: poly/mono/legato mode, tempo sync mode switches, clock source, MIDI settings, ARP on/off. The list is explicit per parameter (appendix B).
3. **Never randomised at all**: the engine, MASTER, tempo, every octave / semitone / transpose / interval parameter (melodies stay in key; fine detune is allowed within 10 cents), MIDI channel, poly / mono / legato mode, LFO polarity, the arp's on/off, the original patch's name and the morph's own settings.
3-bis. **No cliffs in smart mode** (owner's rule, found on LOFI: near 80% of the travel the sound went crazy): in smart mode a few percent of fader travel never makes a large sonic jump. Any parameter or switch where a small step changes the sound abruptly (mode switches inside an engine, bit-depth / sample-rate steps, noise or wave-RAM modes, sync on/off, anything that restarts or reroutes a generator) is either cross-faded or never rolled in smart mode. Only WILD (SMART off) may contain cliffs. Acceptance test: for every engine, over many rolls at high AMOUNT, sweep the fader in fine steps (at least 256) on a rendered note and measure loudness and brightness (spectral centroid) per step; a step whose change is far larger than its neighbours' (a cliff) fails the test and names the parameter responsible; fix = cross-fade it or tag it never-rolled.
3a. **Pitch stays untouched** (owner's rule, so the pitch stays musical): with the PITCH setting off, the envelope-to-pitch depth, the LFO-to-pitch (vibrato) depth, glide, every octave / semitone / transpose / interval parameter and any matrix route to PITCH or VIB are never changed (WILD: not at all; SMART: source and destination stay, and the route's amount stays whole too, owner 2026-10-12: pitch stays; with PITCH on the amount rolls like every route's, rule 3f). Only the fine DETUNE items may move (within 10 cents in SMART). With PITCH on, those items roll like any other. A zero is not special (rule 3e).
3b. **Pan and stereo spread follow the stereo mode**: in SPLIT mode (hard pan) PAN, SPREAD and the per-voice stereo parameters are never changed; in STEREO mode they may change.
3c. **An insert that is off stays off** (owner, 2026-10-12): INSERT TYPE never changes in SMART. While the type is OFF, its values A B C and MIX are not rolled either. With an insert on, A B C and MIX roll within the type. WILD may change the type and switch an insert on.
3d. **Removed engines**: DRUM is not part of DUELL (section 2.1), so the morph never meets them.
3e. **Zero is not a lock** (owner, 2026-10-12): an ordinary parameter at 0 rolls like any other value; for example a DIST of 0 at AMOUNT 11 becomes 0..11 (a subtle change, DIST never rises more than 40 above its original). The only values that stay put at 0 are the mode flips of rule 3g.
3f. **Modulation matrix in SMART** (owner, 2026-10-12): the roll never adds a route, removes one or changes a route's source or destination. Only the amount of an assigned route is rolled, by the one percentage, and **every** assigned route's amount rolls, including a route whose destination parameter the roll itself does not move (e.g. into PHYS BOW while BOW is 0, a non-continuous engine item, PAN in MONO, or from the MORPH fader); the one exception is a route into PITCH / VIB while the PITCH setting is OFF, which keeps its amount whole (rule 3a; with PITCH on it rolls like the others). WILD may add routes and change sources and destinations.
3g. **Distinctive things that are off are not switched on in SMART** (owner, 2026-10-12): insert effects (rule 3c); the ARP (its on/off never; mode, rate, octaves, gate only with the ARP on, and an off arp stays off); the PITCH setting (rule 3a); PHYS BOW (0 = struck; any BOW above 0 replaces the strike by a bowed excitation, so BOW 0 stays 0 and BOW above 0 rolls normally); the ducker (DUCK: if DEPTH is 0 the whole ducker stays off, if it is on DEPTH, attack, hold, release and threshold roll by the one percentage). WILD may switch all of these on. Left out on purpose (no equivalent mode flip): NOISE impulse density, PHYS POS, FORMANT RAND, PHASE WAVE2 "-", sends at 0, LFO and ENV depths at 0, EQ gains (the EQ is rolled in WILD only). The SLICE engine's STUTTER / GATE modes are engine choices that never roll; no insert or other part parameter named SHATTER exists in the firmware.
3h. **The ducker's sidechain input never rolls, in SMART or WILD** (owner, 2026-10-12): SRC (PUMP / PART / CHn), the NOTE / channel selection, the PUMP rate and the release curve are never changed.
4. **Never silent, never painful**: per rule set carried over from the Teensy randomizer (`randomizer.h`): sum of audible source levels capped, filter cutoff floor and no dead filter path, resonance versus drive coupling, no route into an off block, at most a few matrix routes with moderate amounts, no LFO self-loops, output peak checked by rendering a test note in the emulator and in firmware at roll time (quick: 1 block).
5. **Seeded**: each roll uses a seed, so a good roll can be reproduced (saved with the patch; since v3 of the MORPH page, 2026-10-11, the seed number is no longer shown on the panel).

### 3.4 Controls (all four live on one MORPH page, see 3.5)
| Item | What it does |
| --- | --- |
| MORPH | The fader (0..127, A to B). |
| AMOUNT | The one percentage. |
| ROLL A | Roll a new random end A from the original. |
| ROLL B | Roll a new random end B from the original. |
| UNDO | Step back to the previous roll of End A or End B, chosen like ROLL (knob: twist left = A, twist right = B; CC 23: 0..63 = A, 64..127 = B). History of 8 rolls per end, so repeated UNDO walks back. |
| ~~KEEP~~ | Removed in v2 (3.5): the ends simply stay; saving is SAVE held (3.5). |

**End choice** (owner's revision, supersedes the old far-end rule): UNDO names its end exactly like ROLL (left = A, right = B); it no longer depends on the fader. Rolling or undoing the end you are hearing is smoothed over about 50 ms so it does not click.

### 3.4a MORPH TARGET (owner's addition)
A setting decides what the morph works on:
- **SELECTED**: only the part picked with ALGORITHM. Each part has its own fader, ends, history and AMOUNT; the other part's morph is left where it is.
- **BOTH**: one fader moves both parts. Each part keeps its **own** random ends (rolled independently from its own original, with the same AMOUNT), so the two parts vary differently but move together. ROLL A / ROLL B roll that end on both parts; UNDO A / UNDO B act on that end of both parts.
- Over MIDI the fader is CC 20 on each part's channel: in SELECTED mode it moves only that part; in BOTH mode a CC 20 on either channel moves both.
- The setting is saved with the device settings. Where: the MORPH SET page (section 3.5), KNOB 1 = TARGET (PART 1 / PART 2 / BOTH). CC 111 sets it (0-63 selected part, 64-127 BOTH).

### 3.5 Where it lives, v2 (owner's revision after trying v0.1, 2026-10-10)
**MORPH is the main page of the device**: HOME opens on it. The HOME round is MORPH, MORPH SET, MODE, MIXER, OVERVIEW (the old HOME overview: engine, cutoff, resonance, attack, release of the selected part, kept as a quick-edit page).

**MORPH page**
| Knob | Job |
| --- | --- |
| KNOB 1 | MORPH, the fader A to B |
| KNOB 2 | ROLL: twist left = ROLL A, twist right = ROLL B (as built) |
| KNOB 3 | UNDO A\|B: twist left = undo End A's last roll, twist right = undo End B's (as ROLL); the card shows "A\|B" at rest and "<A" / "B>" while a twist builds up. There is **no KEEP**: both ends simply stay as they are until you roll them again or load a sound. |
| KNOB 4 | AMOUNT, the one percentage; **each patch carries its own** (see "AMOUNT follows the patch" below) |

(v3, owner 2026-10-11: ROLL and UNDO moved to KNOB 2 / 3, AMOUNT to KNOB 4; the MIDI CCs are unchanged.) **The panel (v3)** is an animated picture of the morph instead of the old A..B bar, seed numbers and UNDO counts: layered ribbon waveforms that get wilder as the fader moves from A to B (2 ribbons growing to 6, larger, faster, from smooth sines into angular, stepped, glitchy shapes), coloured from the part's A colour to its B colour (High Contrast: each ribbon solid A or B, no blends), breathing with the part's output level (a slow gentle motion in silence); a cursor line at the fader with its value in a thumb on an A..B track, "A" and "B" at the ends; TARGET BOTH: one ribbon set per part in its colours (part 2 phase-shifted). About 16 frames/s while the page is shown, still with MENU > ANIM OFF. "HOLD SAVE = SAVE PATCH" stays as a small dim line at the bottom.

**A rolled B from the start, and always back at A.** Whenever a patch is loaded (and at power-on) the **fader goes to A (0)**: End A is the patch's sound, and End B is the B saved with the patch, or, for a patch with none saved, a **fresh roll** (default AMOUNT, seeded), so the fader does something at once.

**AMOUNT follows the patch** (owner, 2026-10-12). AMOUNT is held per part (settings bytes: one per part, unchanged format). Loading a patch sets that part's AMOUNT to the patch's own, so the KNOB 4 display changes when presets are switched: a factory sound uses its category amount (the sound's bamt override if it has one); a user patch and a MULTI part use the AMOUNT saved with them (the patch's End B block carries it in its previously spare byte 5, as AMOUNT + 1; 0 = not stored, as in older patches: the AMOUNT End B was rolled with, itself the knob value when saved); a patch without any End B block keeps the part's current AMOUNT. Loads are: BROWSE / PATCHES, MIDI program change, MULTI recall, power-on. Turning KNOB 4 (or CC 25) changes the AMOUNT of the part(s) of the TARGET until another patch is loaded; saving the patch stores it. The settings keep only the AMOUNT the player last chose with the knob (a patch load does not rewrite the settings, so no flash write per preset change); it is the AMOUNT for a sound that carries none. Format note: patches saved by this version put a non-zero byte 5 in the End B block; older firmware reads such a block as damaged (it then rolls a fresh End B); this firmware reads old blocks fine.

**Saving (2026-10-11, replaces SAVE x2).** A press of SAVE always opens the SAVE pages, on the MORPH pages too; it saves nothing by itself. **Holding SAVE (about 0.6 s) on any page opens a confirmation, "SAVE OVER <name>?" (OCT+ YES, OCT- NO): OCT+ saves the selected part's sound over the patch it has loaded.** With TARGET = BOTH and the MORPH pages shown, it asks "SAVE OVER BOTH PATCHES?" and saves both parts, each over its own patch ("SAVED 1+2"). What is saved: the sound (End A, the original), **End B as well** (seed + AMOUNT + roll settings in the record, regenerated exactly on load) and AMOUNT; the fader position is not saved (a patch always loads at A). A sound with no patch (after INIT SOUND, or its stored sound erased) goes to a new place named after the engine ("SAVE AS NEW SOUND?"); no place or no room left: "STORE FULL" (section 3.6). Hold SAVE no longer undoes the last load (that undo is gone from SAVE; the EDIT layer's OCT- still puts back its loads).
**No split between built-in and stored sounds: built-in sounds can be saved over (the PATCHES page, section 3.6).** A built-in sound is in the firmware image, so saving over one stores a copy that **shadows** it: the stored record names its engine + 1 and its sound index + 1 (a marker in the record, no format change). The list (PATCHES page, PRESETS knob, the EDIT layer's sound list, FAV and the type filters) shows the copy **in the sound's place** (same position, its engine tag, not a slot number) and hides the original; saving again overwrites the copy. The copy is a normal stored patch otherwise (EDIT renames it). **Erasing it (K4 ERASE) brings the built-in sound back**, and the part that played the copy plays the built-in sound again. FAV of a copy is its sound's flag (per engine and sound), so it survives save and erase. Not redirected to the copy: the editor's preset load and an engine change (ENGINE knob: the engine's first built-in sound), which load the built-in sound. The SAVE pages, in this order and named the same in the footer and the header, are PATCHES (section 3.6), PATTERNS, MULTIS (section 5b.4; was PROJECT), TOOLS. On these pages only, the header's centre shows that title (in the tempo's text style) instead of the transport and tempo; the part badge and REC mark stay, the slot number and octave are dropped when the title leaves no room. A SAVE press opens PATCHES (the first page; again = the next).
Storage requirement: End B must be reproduced exactly. The build chooses how (a compact seed + AMOUNT that regenerates the same end from the saved original, or a stored list of the differences from the original) within the limits of the user-preset record and the format rules (section 6, storage decision), and says which and why; hearing the same sound after a reload is the acceptance test.

**MORPH SET page (every knob has a job)**
| Knob | Job |
| --- | --- |
| KNOB 1 | TARGET: the card shows **PART 1**, **PART 2** (the part ALGORITHM has selected) or **BOTH**; never the word SELECTED. The panel says it in plain words: "THE MORPH MOVES PART 1 ONLY" / "... BOTH PARTS TOGETHER". |
| KNOB 2 | SMART: ON (default) or OFF. OFF = **WILD**: every parameter and every switch can change, none of the smart rules (exclusions, caps, dead-filter and level safety, fades) apply, switches simply flip at the middle of the fader. The engine still never changes, and the master volume, tempo, system settings and (see PITCH) pitch stay untouched. The master limiter stays on. |
| KNOB 3 | ARP: OFF (default) or ON. ON = a roll also shuffles the arpeggiator settings: its mode (algorithm), rate, octaves, gate and, once it exists, the SEQUENCER step pattern. |
| KNOB 4 | PITCH: OFF (default) or ON, separate from WILD. OFF = the morph never changes anything that moves pitch: octave, semitone and interval settings, transpose, glide, pitch-envelope and vibrato depth (also in WILD). ON = those may change too (the whole-number ones step at the middle of the fader; "zero stays zero" is lifted for pitch modulation). Added at the owner's request in place of a fader-speed setting (the speed of the fader is the player's hand). |
All four settings are saved with the device settings (not the patch). CC 111 = TARGET as before; new CCs 113 SMART, 114 ARP, 115 PITCH (**[verify]** no clash).


### 3.6 PATCHES (owner's design, built 2026-10-12; replaces SAVE > PRESETS and SAVE > USER PATCHES)
**One list.** SAVE's first page, **PATCHES**, is the only list of sounds: the **factory sounds** first (category-major browse order: BASS, LEAD, PAD, PLUCK, KEYS, PERC, FX, OTHER, then engine order, then name), then the **user patches** (in the order they were saved), then the **empty places** at the end: exactly as many as the store still has free (**FREE nn**, shown on the card under ERASE). The store holds **at most 48 patches** (stored copies of factory sounds and patches of your own together); a factory sound costs a place only once it is overwritten, so the list can be longer than 48 (97 factory sounds + yours + the free places), but only 48 of them can ever be stored at the same time. At 48 stored the empty places are gone (FREE 0), and a save that needs a new place (a patch of its own, a factory sound's first copy) says **NO FREE PLACE (ERASE ONE)** at once, before any dialog; saving over a patch that is already stored always works. Every factory sound can be overwritten (hold SAVE: a shadow copy, section 3.5); erasing the copy restores the factory sound. The header and the footer say PATCHES; the SAVE family is PATCHES, PATTERNS, MULTIS, TOOLS. The old USER PATCHES page and its slot numbers are gone.

| Control | Job on PATCHES |
| --- | --- |
| KNOB 1 PATCH | Steps through the list shown (the card: place / places in the list shown, e.g. "42 /145"). A sound loads at once, as the PRESETS knob loads it (the sound, plus the arp, keys and SEQ pattern the patch carries, see "Arp, keys and SEQ pattern" below; mix, ROOT and the other steps stay; MORPH at A). Stepping on to an **empty place** only selects it (the list shows EMPTY there; nothing is loaded, the part keeps playing what it played). The PRESETS knob steps the same list (occupied places only). |
| KNOB 2 TYPE | A filter, never a load and never an engine change: ALL, FAV, BASS, LEAD, PAD, PLUCK, KEYS, PERC, FX, OTHER, then **one entry per engine** (that engine's sounds only). Empty places are listed under ALL only. Kept with the settings. |
| KNOB 3 FAV | Marks the selected sound (a copy shares its factory sound's mark). Marks cover every place (stored patches by slot 0..127, of which at most 48 are used; factory sounds per engine and index). |
| KNOB 4 ERASE | A turn right asks, as INIT does: a factory sound with no copy says ALREADY FACTORY; its copy asks "RESTORE FACTORY?" (OCT+ yes, OCT- no); a patch of its own asks "ERASE PATCH?" and leaves its place empty (the cursor goes to the first empty place); an empty place says EMPTY PLACE. Stopped transport only (STOP TO SAVE). |
| SELECT | Turns the SAVE sub-pages only (PATCHES, PATTERNS, MULTIS, TOOLS), as on every page; it never moves in the list (label law). |
| SAVE (tap) | Opens PATCHES (from any page); on PATCHES again, the next SAVE page. |
| SAVE (hold) | On a sound or a copy: "SAVE OVER <name>?" (OCT+ yes), the dialog of section 3.5 (a factory sound: its copy). **On an empty place: the NAME screen (a random name offered), then OCT+ stores the part's sound as a NEW patch.** On any other page: as 3.5 (the patch the part plays). |
| EDIT | Renames the selected stored patch (a factory sound with no copy says NOT STORED). |

**Storage.** The PATCH store lives in **six chunks**, six storage objects (`OBJ_PATCH0..5`), an A/B sector pair each, one chunk of records each (`upreset.c`, 3836 usable bytes a chunk): chunks 0..2 at 0xC8000..0xCDFFF (24 KiB, USR3's old place: owner's choice, user sample slots USR1 and USR2 stay as they were; **USR3 is retired**: it stays in the sample-set numbering and always reads empty, and the editor cannot write it), chunks 3 and 4 at 0xDC000..0xDFFFF (Felucca's retired 32-slot preset banks, 16 KiB; nothing else uses those sectors: they lie in the user-data range below the OTA staging at 0xE0000) and chunk 5 in the retired FM6 patch bank's pair, 0x9F000 / 0xFE000. **The limit is guaranteed, not a hope:** the biggest record there can be is **435 bytes** (an FM6 sound with its 112-byte patch, a SEQ-mode arp with all sixteen values and a full 16-step pattern, End B, a shadow marker and every one of the 127 values off its default); 8 of them fit a chunk with its 4-byte header, so 6 chunks always hold **48 patches of any kind** (`PT_MAX`; before: 3 chunks, 3 x 7 = 21 worst-case records guaranteed, 128 places listed, typical sounds filled the store at about 60 big ones). A chunk takes a new record only under 8 records; a record that is rewritten stays in its chunk (it always fits). The save at 48 never fails half way: **NO FREE PLACE (ERASE ONE)** is said first; "STORE FULL" is only a guard for a store written under other rules (a backup restored from elsewhere). A store that already holds more than 48 records stays readable; it takes no new patch until it is under 48. A record is **sparse** like the factory sound format: engine, category, name, the shadow marker, End B's 7-byte block, then only the values that differ from the engine's INIT ((id, value) pairs, or from 17 values on a 16-byte bitmap and one byte each: whichever is shorter; the AK block's values likewise from 3 on), the 16-step pattern if it has one, an FM6 sound's patch (112 bytes) and the **AK block** (arp, keys, SEQ pattern; below) inside the record. A save rewrites only the chunk that changed, with the standard commit-last write (erase the other sector, payload, commit record last, read back): a power cut leaves the old chunk. A record that no longer fits its chunk is written to another one first (its revision byte is higher) and cut from the old one after; if the cut falls between, the newer revision wins at the next boot. A typical sound takes 55 to 90 bytes (an FM6 sound about 190; an ordinary arp adds about 9, a SEQ-mode arp with a 12-step pattern about 120), so a real store has far more room than 48 patches need; the 48 is what is guaranteed whatever they hold. Felucca's 32-slot banks and its FM6-patch object are no longer used as such.
**Arp, keys and SEQ pattern travel with a patch** (owner, 2026-10-12). Saving a patch (hold SAVE over a patch, a new place, a factory sound's shadow copy) stores what the part holds now: the **arp** (when ON: mode incl. SEQ, rate, octaves, gate, swing, probability, hold, order), the **keys** (SCALE, CHRD, VOIC when not default; ROOT stays the player's) and, when the arp mode is **SEQ**, the **SEQ pattern** (the first 16 steps with note offset, accent, slide, ratchet, nudge, velocity, chance, and LEN at most 16, DIV, SWING, GATE, QUANTIZE). ARP TARGET is the sound's own and travels as it always did. A shadow copy stores the part's arp, not the factory sound's (a factory arp the player switched off stays off; an edited rate is what loads). Loading a patch (PATCHES K1, the PRESETS knob, MIDI program change; MULTI is separate and unchanged) applies what it carries under the factory sounds' **lend rule**: the part's own arp, keys and pattern go aside, the next sound loaded gives them back (unless they were edited since: the edit stays), and a patch that carries them replaces the lent ones. A patch **without** the block (every older patch, and one saved with the arp off, default keys and no SEQ arp) leaves the part's arp, keys and steps as they are. Steps 17..64 are never touched. Format: an additive block at the end of the record (flag 32; `upreset.c` "the AK block"), sparse against the defaults, steps in the packing of the MULTI and the pattern slots; a record without it loads as before. The editor's patch write (UP_PUT) carries no block (it drops one); RENAME and the FM6 patch write keep it. Firmware before this reads a record with the block as damaged (nothing released yet).
**Program change:** bank 1 program n = stored patch n (slot 0..127; the slot is the record's number, not a list place; at most 48 slots are in use, a new patch takes the first free slot after the last one, so numbers stay as they are when others are erased); bank 0 as before (the sounds in list order, ALL).
**Not changed:** the editor protocol (slots 0..127, the wire format as before, section "user presets" of `web/EDITOR_PROTOCOL.md`); the list total byte saturates at 127.

---

### 3.7 PATTERNS: DUELL's own 16 factory patterns (built 2026-10-12; replaces Felucca's 13)

SAVE > PATTERNS has 16 slots, P01..P16, all overwritable; a slot never saved (or erased) is its **factory pattern**, so no slot is empty by default (ERASE restores the factory pattern; on a factory slot it says ALREADY FACTORY). The set is our own: generic rhythmic and melodic idioms of dance, funk and dub music, written as one bar of 16 steps (LEN 16, 1/16 notes), none copying a recognisable melody. Defined in `engines.c` (`PATTERNS[]`), names at most 12 characters.

**Offsets, not absolute notes.** Every note is **C4 (60) + an offset in semitones**, 0 = rest. That is exactly what the SEQUENCER arp mode reads (note - 60 per step), so a pattern loaded into a part also works as the SEQ arp pattern on the held notes, on any engine. In the step sequencer the patterns play from C4 (OCT- or SCL TRANS move them). Flags used: accent, slide, tie (holds the previous note; the SEQ arp treats a tie as a rest). A pattern keeps the part's DIV, SWING and GATE (a factory pattern sets LEN 16 only).

| Slot | Name | Idiom |
|---|---|---|
| P01 | MORODER OCT | Disco / Italo bass: 16ths jumping root and octave, accent on each beat, the last beat walks up to the 5th and b7. |
| P02 | ACID LINE | 303-style acid line: 16ths with rests, octave jumps, accents and slides. |
| P03 | OFFBEAT | House bass: the root on the off-beat 8ths only, the last one an octave up. |
| P04 | TECHNO RUMBL | Techno rumble: three 16ths after every beat, the beat empty (the kick sits there). |
| P05 | TRANCE ROLL | Trance / psy rolling bass: unbroken 16ths, the octave on the off-beat 8ths. |
| P06 | 8TH PULSE | Straight 8th-note pulse (new wave / driving rock bass), the last 8th on the 5th. |
| P07 | ARP UPDOWN | Minor-triad arpeggio climbing two octaves and falling back, 16ths. |
| P08 | BERLIN RUN | Berlin-school sequencer run: the root as a pedal on every other 16th, a figure climbing and falling between. |
| P09 | DETROIT STAB | Detroit / house chord-stab rhythm: syncopated hits (gaps 3-3-4-3-3), root, minor 3rd, 5th. |
| P10 | FUNK 16TH | Funk bass groove: the one, ghost 16ths, octave pops, a walk-down fill. |
| P11 | DUB HALFTIME | Dub half-time: few long tied notes, the root, a 5th below after the space. |
| P12 | SKANK | Reggae skank: the off-beat of beats 2 and 4, a bubbling 5th in between. |
| P13 | GALLOP | Galloping bass: an 8th and two 16ths on every beat, the last beat up a minor 3rd. |
| P14 | PENTA RIFF | Minor-pentatonic riff: root, b3, 4, 5, b7 (and the b7 below), a slide into the 4th. |
| P15 | BREAKBEAT | Breakbeat syncopation: the hits of a broken kick / snare pattern, accents on the backbeats. |
| P16 | FOUR FLOOR | Four on the floor: one hit on every beat, a kick-style gate for any sound. |

**Random pattern (not built).** A one-percentage roll of a pattern (the morph AMOUNT) is cheap in code but has no honest control: the PATTERNS page uses all four knobs (SLOT, LOAD, ERASE, SAVE) and the label law forbids a second meaning. Open decision: where ROLL lives (see Appendix C).

## 4. MIDI (any MIDI device)

Goal: nothing to configure, everything automatable, no stuck or lost notes.

### 4.1 Channels
- Part A on channel 1, part B on channel 2 (default). **Each part's channel is settable 1..16 on GLO > CHANNELS (KNOB 1 P1 CH, KNOB 2 P2 CH; built 2026-10-12)** and is what the part receives (notes, bend, CCs, program change, bank select) and what the keys send; channels no part uses are ignored. Both parts on one channel: part 1 takes it. In LAYER part 1's channel plays both parts and part 2's is not heard. **MIDI IN** (GLO > CHANNELS, KNOB 3; no longer in MENU; renamed 2026-10-12 from PARTS / SEL, which nobody could read): **OWN CH** (each part listens on its own channel, set with P1 CH / P2 CH) or **SELECTED** (every channel plays the part chosen with ALGORITHM, for a single keyboard; the card says SELECT, the line under it says what that means). Stored as before (0 / 1, the project's `G_ROUTE`); the former blocks CH5-8 .. CH13-16 are replaced by the settable channels. Kept in the settings ([15][0] part 1, [15][1] part 2: 0 = the default, else the channel 1..16; older settings load with 1 and 2), not in the MULTIs.
- The sidechain's trigger channel and note are the part's DUCK SRC (CH1..CH16) and NOTE (4.x ducker): the same two parameters are on GLO > SIDECHAIN (KNOB 1 SC SRC, KNOB 2 SC NOTE, the selected part's; ALGORITHM picks the part) and on the FX > DUCK pages.
- MIDI LEARN is on GLO > MIDI LEARN (KNOB 1: right = ON, left = OFF; KNOB 2 right = CLEAR every learned CC, the card counts them; OCT+ done, OCT- clears the picked CC; MENU's LEARN CLEAR row is gone); it was GLO held + D4.
- Keys on the FM-1 send on the selected part's channel.

### 4.2 Messages in
- Notes with velocity; note-on velocity 0 = note-off; running status; correct all-notes-off (CC123), all-sound-off (CC120), reset-all-controllers (CC121), sustain (CC64).
- Pitch bend with RPN 0 bend range (default 2 semitones, max 24). Channel aftertouch and polyphonic key pressure (aftertouch as matrix source).
- Program change plus bank select (CC0/CC32): recall, without audio dropouts (see 4.2a).
- MIDI clock, start, continue, stop, song position pointer. Clock from USB or TRS MIDI; external clock locks tempo-synced LFOs, envelopes and the arp. Start/stop only restarts what the PLAY/STOP option (2.3) says. Clock jitter smoothing.
- USB MIDI class-compliant, so any USB host (computer, hardware sequencer, controller with a USB host port) sees DUELL as a device with 2 channels. TRS MIDI IN kept (type per Felucca; **[verify]** type A/B).

### 4.2a Program change, bank select, mod wheel (built 2026-10-11)
- Channels: as for the notes. MIDI IN OWN CH: each part's channel (GLO > CHANNELS, default 1 and 2), other channels ignored; MIDI IN SELECTED: any channel, the selected part. LAYER: part 1's channel only (SELECTED: any).
- BANK SELECT: CC0 (MSB) and CC32 (LSB) are remembered per channel and applied at that channel's next PC: bank = MSB x 128 + LSB (power-on 0; CC121 does not reset it). They are never parameter CCs nor MIDI LEARN targets.
- PC in SPLIT and STEREO: a SOUND for the channel's part only (the other part and this part's own settings, LEVEL, PAN, sends, stay; the arp, keys and SEQ pattern change only when the patch carries them (lend rule, section 3.6), otherwise they and the steps stay; the selection stays). Bank 0: program n = entry n (0-based) of the PATCHES list in its ALL order (occupied places only) (every engine's factory presets in engine order, a stored copy of a preset in that preset's place, then the other stored sounds), whatever the TYPE filter (FAV, a category) shows, so a program number never moves with a browsing filter. Bank 1: program n = stored patch n (slot 0..127: the numbers are the patches' slots, at most 48 patches are stored, section 3.6). Other banks, programs past the list, empty slots: ignored.
- PC in LAYER: bank 0, program 0..15 = MULTI M01..M16, loaded exactly as the MULTI page's LOAD (both parts, MODE and globals the multi holds, each MORPH at fader A with its End B). An empty slot, programs 16..127 and any other bank: ignored.
- A recall is queued by the audio interrupt (8 waiting at most; more are dropped) and run by the main loop, as the MORPH's CC actions. It loads as the PRESETS knob / MULTI LOAD does: the part's notes off, an engine change fades, the MORPH starts again at A; a playing transport keeps running. No PROGRAM CHANGE is sent out.
- MOD WHEEL (CC1): always (no switch) also moves the MORPH fader of the channel's part, fader = CC value (0..127), through the same queue and TARGET rules as CC 20 (TARGET BOTH: both parts); LAYER: both parts' faders. It stays the matrix's MODW source unchanged.

### 4.3 Messages out
- The keys, and every knob move, send a CC on the part's channel, so any recorder or sequencer can record knob automation and play it back.
- No echo of received CC (no loops). MIDI THRU of notes off by default.

### 4.4 CC map (identical on both channels; fixed, no learn needed; MIDI LEARN kept for custom)
Standard: 0 / 32 bank select (4.2a), 1 mod wheel (MODW matrix source and the MORPH fader, 4.2a), 5 glide, 7 level, 10 pan, 64 sustain, 71 resonance, 72 release, 73 attack, 74 brightness (cutoff), 75 decay, 91 reverb send, 93 chorus send, 94 delay send.
DUELL block (all 0-127, never 14-bit in the first build):

| CC | Function | CC | Function |
| --- | --- | --- | --- |
| 20 | MORPH fader | 24 | KEEP (>=64 fires) |
| 21 | ROLL A (>=64 fires) | 25 | AMOUNT |
| 22 | ROLL B (>=64 fires) | 26-29 | EDIT page KNOB 1-4 |
| 23 | UNDO (1..63 = End A, 64..127 = End B, fires once; 0 re-arms) | 30-33 | ENV page KNOB 1-4 |
| 34-37 | LFO page KNOB 1-4 | 38-41 | FX page KNOB 1-4 |
| 102-109 | Matrix amounts slot 1-8 | 110 | Arp on/off |

As built (2026-10-11, firmware midi_control.c): 0 / 32 bank select; 1 MODW + MORPH fader; 20 MORPH fader; 21 ROLL A, 22 ROLL B (>=64 fires); 23 UNDO; 24 unassigned (KEEP removed); 25 AMOUNT; 111 TARGET (0-42 PART 1, 43-85 PART 2, 86-127 BOTH); 112 MODE (0-42 SPLIT, 43-85 STEREO, 86-127 LAYER, any channel); 113 SMART, 114 ARP, 115 PITCH (>=64 on); 116 BALANCE, 117 XFADE, 118 SPLIT note (any channel); 26-41 and 102-110 not built. Program change: 4.2a.

Page knobs give every parameter a CC through the page structure: a controller's "page" can follow a DUELL page. **[verify]** against Felucca's `midi_control.c` and the parameter table; numbers may shift if they clash with Felucca's learn defaults.

### 4.5 Acceptance tests (host tests, no hardware)
1. 2000 random notes across both channels: no hanging notes, correct polyphony and steal order.
2. Clock at 30-300 BPM, with jitter: tempo lock within 1 BPM.
3. Every CC round-trips: in produces the parameter value; the knob move produces the CC out.
4. PC + bank: preset loads in under one audio block of silence.
5. MPE is out of scope.

---

## 5. UI details

- HOME page 1: both parts side by side: name, engine, level; the active part highlighted. KNOB 1 LEVEL A, KNOB 2 LEVEL B, KNOB 3 PAN / WIDTH of the selected part (inert and greyed in SPLIT), KNOB 4 CUTOFF of the selected part. HOME page 2: MORPH (section 3.5).
- Chord keys and scale: SCL page as in Felucca (section 2.3).
- Header: part A / B indicator, tempo (always read-only except on GLO), morph position as a small bar.
- Every knob card shows its parameter name and value; a changed-by-morph value shows a small marker.
- No page changes meaning with hidden state. The tempo bug (SELECT changing tempo on some pages) is a spec violation of 1.1 and has an emulator test: turn SELECT on every page, assert the tempo is unchanged.

---

## 5a. Part colours and the part badge (owner's request, 2026-10-10)

**Naming:** on the screen and in the manual the parts are **1** and **2**. Where this spec says part A / part B it means part 1 / part 2. The morph's two ends keep the letters **A** and **B**, so the two never mix.

**Part colour.** Every palette gets a second theme colour. A palette today has five colours (BG, SURF, TEXT, THEME, ACCENT; `tools/gen_ui_palettes.py`). DUELL adds THEME_2, so part 1 uses THEME (values, gauges, curves, selection) and part 2 uses THEME_2. Swapping the part with ALGORITHM changes the whole screen's theme colour, so it is obvious which part you are on. The pairs are chosen to be clearly different from each other and to look good together, tuned to the unit's shell colours (the owner's unit is purple):

| Palette | Part 1 | Part 2 |
| --- | --- | --- |
| VIOLET | purple | pink |
| ICE | blue | teal / cyan |
| AMBER | amber | red-orange |
| GREEN | green | lime / yellow |
| ROSE | pink | purple |
| NIGHT | phosphor green | cyan |
| PAPER | dark green | terracotta |
| HI-CON | yellow | cyan |
| GREY | white | warm grey |
| MONO | solid white | white outline (no colour, so the badge is solid against outlined) |

The exact values are a design item: the build produces a preview sheet (PNG) of every palette in both part colours, on the HOME, EDIT, GLO and MORPH pages, for the owner to judge and adjust. Contrast checks of `gen_ui_palettes.py --report` apply to both colours.

**Part badge.** In the top-left corner of the screen, two numbers side by side: **1 2**. The selected part's number is bright, in that part's colour (a filled badge, as the single track badge today); the other is faded (the palette's DIM). Swapping the part swaps which is lit.
- With the morph TARGET set to BOTH (section 3.4a) both numbers are lit, so you can see that the morph moves both parts; with SELECTED, only the selected part's number is lit.
- The badge never carries a second meaning; it only says which part(s) the screen is about.

---

## 5b. Stereo modes, two-part screens, and more panorama (owner's requests, 2026-10-10)

### 5b.1 Three output modes (replaces the two-mode switch of section 2.2)
One setting, **MODE**, with three choices. It is one of the most important settings, so it is on **HOME's second sub-page** (first knob), always one SELECT turn from HOME, and saved with the settings:
- **SPLIT** (default, the dawless mode): part 1 is hard left, part 2 hard right, exactly. Everything stereo inside a part (chorus, insert, reverb sends, spread) is **folded to that part's side**: nothing of part 1 may leak to the right channel or the other way round. A test sends a loud part 1 and asserts the right channel is digital silence (and the reverse), with every effect on.
- **STEREO**: both parts are full stereo with free pan, width and the extra panorama controls below.
- **LAYER** (new): the two parts play together as one instrument: every note (keys and MIDI on channel 1) triggers both parts at once, their sounds are summed into a full stereo field, for a much more complex sound. Part 2 follows the same notes, velocity and bend; each part keeps its own engine, level, pan, envelopes, LFOs, matrix and insert. The MIDI channel of part 2 is ignored in LAYER (or follows the setting OMNI); **each part runs its own arp** (revised 2026-10-10): the notes you play (keys, MIDI channel 1) go to BOTH parts as held notes, and each part handles them by its own settings: a part with its arp on arpeggiates them (with its own arp mode, rate, octaves, SEQ pattern and ARP TARGET), a part with its arp off plays them directly. So one layer can be arpeggiated while the other holds the chord, or each layer can run a different arp. The morph TARGET can be BOTH. Voice budget: each note uses a voice in each part, so the polyphony halves; the screen shows the number.
Switching between modes is click-free (crossfade of the pan law, 50 ms).
- **MODE page, KNOB 2-4 (owner's request, 2026-10-11; saved in the settings ([15][5..7]) and in each MULTI, version 2):** BALANCE -100..+100 (0 = off; linear level balance at the part level stage, right fades part 1, left part 2; all modes; CC 116, 64 = 0), XFADE 0..100 % (0 = off, bit for bit; LAYER only: a note's velocity v gives part 1 cos and part 2 sin of pi/2 x v/127, blended to 1 by XFADE; a per-voice gain; keys count as velocity 100; CC 117), SPLIT OFF or a note C-1..G9 (LAYER only: below it part 1 plays alone, at or above it part 2; keys and MIDI channel 1; a held note ends as it started; CC 118, 0 = OFF; split first, then XFADE).

### 5b.2 More panorama in STEREO and LAYER mode (not in SPLIT)
- Each part's PAN and WIDTH are modulation destinations (matrix: PAN already exists; add WIDTH), and LFO 1 and LFO 2 can drive them through the matrix.
- **Panorama inside the engines**: wherever an engine has a mixer of several sources, each source gets its own pan, so a part spreads across the soundscape. Survey (to do in the build, per engine, from the code): ANALOG (osc A, osc B), TRIO (osc 1-3), PHASE (main, sub), WHEEL (the partials, a pan law across the drawbars), FM6 (carriers: spread the carrier operators), PHYS, NOISE, GRAIN (grains scattered), VOICE, LOFI (voices), SAMPLE. Result: a table of "pan points" per engine and which are cheap. Each pan point is a continuous parameter and morph-randomisable in STEREO and LAYER mode only. Felucca already has SPREAD (voices of a POLY/UNISON part around PAN) and per-voice stereo in some engines: reuse.
- Goal: in STEREO and LAYER mode the sound should be "very stereo" without sounding phasey; every new pan point is checked for mono compatibility (mono sum must not cancel more than 3 dB).

### 5b.3 Redesign of the two-part screens
Felucca's mixer, global and song screens were drawn for four tracks. With two parts they leave empty space. Redesign: the **HOME mixer** with two wide, readable strips (name/engine, level meter, pan, mute, solo), **GLO's main page** (step 6, below) and the SONG/track lanes as two large lanes, all at readable sizes; layout lint (0 findings) in every palette; the badge and part colours of section 5a.

**GLO main page (owner's feedback, 2026-10-12; built):** the former hold-GLO layer is now the page a tap of GLO opens (`graph GR_GLO`; ui_layer.c keeps its keys and knobs). Top: the two parts' LEVEL cards (KNOB 1, 2; KNOB 3, 4 nothing). Panel: per part a column SOLO (white key F / G, while held) over MUTE (black key F# / G#, latched), C = ALL (unmute all), F = TAP (tempo from 3 taps; INT clock only); **between the columns a dB level meter per part**: its own level before MASTER (what the part adds to the mix after LEVEL, DIST, INSERT, EQ and DUCK; in SPLIT the part's bus with its delay / reverb return too), -48..0 dBFS, the bar follows the peak up at once and falls at 36 dB/s, a 2 px tick holds the highest peak 1.2 s, the part's colour up to -6 dBFS, the accent from -6, red from -1 dBFS (a colour like the part's own gives way to the text colour: HI-CON's red part 1), a lamp over the bar stays on 1.5 s after a peak past -1 dBFS. Flat colours only. Cost: nothing new in STEREO / LAYER (the per-part peak exists since the MIXER's meter); in SPLIT two compares per sample while this page is shown. The keys are the page's while it shows (no notes on it), as in a locked layer; SELECT, the page buttons, OCT work as on every page. The header shows the parts only (the multi number is not shown on this page).

### 5b.4 MULTI (owner's request, 2026-10-11; replaces SAVE > PROJECT)
Two-part playing is the heart of DUELL, so the multitimbral setup is saved whole in **16 slots, M01..M16** (SAVE > MULTIS, after PATTERNS; the four PROJECT slots A..D are gone). Each slot has a name (up to 12 characters, as projects had: EDIT renames, the NAME screen opens on SAVE).
- **Held, per part (1 and 2):** the sound: engine, preset and every part parameter (filter, envelopes, LFO 1 / 2, ENV 2, the matrix, INSERT, EQ, DUCK, voice); the FM6 patch (an FM6 part only); the morph's AMOUNT and End B (as a user preset holds it: seed + AMOUNT + roll bits, 7 bytes, regenerated exactly on load; not the end's values); the arp; the SEQUENCER pattern (all 64 steps, LEN DIV SWING GATE QUANTIZE) and its automation / parameter locks; the mix (LEVEL, PAN, MUTE, the sends).
- **Held, global:** TEMPO and SWING, CHORUS, DLY 1 / DLY 2 (incl. PING-PONG and its SPREAD), REVERB (incl. TYPE); the output MODE (SPLIT / STEREO / LAYER); MORPH SET's TARGET, SMART, ARP, PITCH; the selected part.
- **Not held (device settings):** colour theme, MIDI channels and MIDI IN routing, clock source, TUNE, display / LED / USB settings.
- **Page:** KNOB 1 SLOT (M01..M16; the list shows the stored name, "--" when empty), KNOB 2 LOAD, KNOB 3 ERASE (asks ERASE Mnn?), KNOB 4 SAVE (over a used slot asks OVERWRITE Mnn?, then NAME); OCT+ does the picked action. LOAD asks nothing (as PROJECT's did) and replaces everything held; each part's MORPH goes to fader A with the multi's End B (the patch-load rule). Loading while playing keeps the transport running: all notes off, an engine change fades as usual. SAVE, ERASE and renames need the transport stopped (STOP TO SAVE). The header shows the number of the multi the selected part's pattern came from (with * once edited).
- **Storage (built):** a compact record (version, the values as bytes, the steps sparse: a 64-bit map plus 9 bytes per used step, the FM6 patch only for FM6, End B as its 7-byte block; at most 1916 bytes; the sound and steps always fit, automation beyond the room is cut and the save says AUTOMATION CUT). Two multis per storage object, each object an A/B sector pair with the commit record written last: a save rewrites the pair's other sector, so a power cut keeps both multis of the pair as they were. Flash: M01..M08 at 0x97000..0x9EFFF (the old project sectors), M09..M16 at 0xCE000..0xD5FFF (user sample slot USR3 shrinks from 56 to 24 KiB). Old PROJECT data is not migrated: it reads as empty slots.
- **Factory multis (owner, 2026-10-11).** M01 **VESPER CLUB** (#2 VESPERS on VOICE + #3 JAZZ CLUB on WHEEL), M02 **SHARD STAB** (#13 SFORZANDO + #15 GLASS SHARDS, both ANALOG), M03 **MODEM DRIFT** (#2 STRETCH on GRAIN + #9 MODEM on NOISE), M04 **ELASTIC LOW** (#3 RUBBER BAND + #0 LADDER BASS, ANALOG) are built into the firmware. Source: the raw version-2 records `felucca-base/tools/factory_multis/M01.bin..M04.bin` (built in the emulator, extracted with `tools/flash_dump.py --extract-multis`, renamed in place; each holds its End B block, AMOUNT, steps and settings); `tools/gen_duell_multis.py` checks them (version, name, and that each part's engine + factory index is still the intended sound of `tools/duell_sounds.py`, so a changed sound list stops the build) and writes `build/gen/duell_multis.h` (2029 bytes of flash image, none of the multi store). A slot whose stored record is empty shows and loads its factory multi (first boot, after a flash erase). SAVE over it, like over any slot, stores an ordinary record that wins from then on and is not touched by firmware updates (the ROM copy is only the default); a rename of an unchanged factory slot stores its record under the new name. ERASE of a saved overwrite deletes the record, so the factory multi is back (after a power cycle too); ERASE of an unchanged factory slot says ALREADY FACTORY and writes nothing (as PATTERNS' factory slots). M05..M12 have no factory content (empty, "--"); M13..M16 are the four CPU TEST multis (section 6.1). A part stores its sound's values in full, so the multi sounds as built even when a sound is later edited in the list; the engine + index only name the sound.

## 5c. Effects additions (owner's requests, 2026-10-10)

1. **Reverb: lusher stereo space.** Improve the HALL and the other reverb models' stereo image (decorrelated left/right tank outputs, wider early reflections, slow modulation to avoid ringing, stereo width control and a mono-compatibility check). Judged by listening in the emulator and by measurements (inter-channel correlation, decay, no ringing, CPU within +30% of the current model). In SPLIT mode the reverb return is folded to each part's side (5b.1).
2. **Delay: ping-pong option.** A PING-PONG switch on the delay (the repeats alternate left and right; in SPLIT mode each part's repeats stay on its own side, so ping-pong only applies in STEREO and LAYER, and the screen says so).
3. **EQ module** (new FX): low shelf, parametric mid (frequency, gain, Q) and high shelf, each with gain ±12 dB, to clean up a mushy mix. Placement (decided): **one EQ per part only**, after the part's insert effect, no master EQ. Parameters per band are modulation destinations.
4. **MIDI-triggered sidechain compressor.** A sidechain "ducker", **per part** (decided; each part has its own trigger settings), that is triggered by a MIDI note on a chosen channel, with no audio input and no drum machine needed: setting TRIG CH (off, 1-16) and TRIG NOTE (any note or one note), DEPTH (how far the level ducks), ATTACK, HOLD and RELEASE (or a tempo-synced release), SHAPE. A note-on starts the duck, the release is the pumping. The trigger notes never play a sound. Optionally also a built-in tempo-synced trigger (4 on the floor) for use without MIDI, off by default. The ducker is also a matrix source (DUCK) so it can move other parameters. Test: an exact gain curve on a rendered note after a trigger, no clicks, correct latency of less than one audio block.
   **Built (2026-10-11):** per part DUCK CH (OFF, 1-16), NOTE (ANY or one note), DEPTH, ATK (1-100 ms), HOLD (0-500 ms), RLS (20 ms-2 s, log) on pages DUCK and DUCK 2 (ids P_DKCH..P_DKRLS, P_COUNT 119, P_E0 111; not morph, not motion); matrix source DUCK; linear gain curve; applied before level, pan and sends (fx.c duck_*). A note matching a part's trigger is swallowed when NOTE is one note or the channel plays no part; with NOTE ANY on a channel that plays a part, the notes play and also trigger. Not built: tempo-synced release, SHAPE, the built-in 4-on-the-floor trigger.

5. **SLICER insert removed (2026-10-11).** The per-part SLICER (GATE / STUT, page FX > SLICER) is gone: no page, no processing (slicer.c keeps only the FX hold layer's 32 KB loop buffer, sl_buf, and the ramp constants; perform.c's REPEAT / REVERSE / TAPE / FREEZE / OCT effects are unchanged). FX family order: FX, INSERT 1, INSERT 2, EQ 1, EQ 2, DUCK 1, DUCK 2, CHORUS, DLY 1, DLY 2, REVERB. Its four ids (45..48) are **free**: core.h P_FREEA..P_FREED, label "-", range 0..0, no page, never morphed, not kept on a sound load (param_kept), P_COUNT 127 and every other id in place. **Reserved for the universal filter (CUT / RES / TYPE / KTR), next SPEC version.** Stored values there (old projects, user presets, motion) load as 0 (param_fit clamps to the 0..0 range) and nothing reads them; the filter must not take its values from stores written before it (they may hold the SLICER's 1, 1, 127 from a store whose loader skipped param_fit).

## 5d. Dual core (owner's request: use both cores of the device)
**Status (updated 2026-10-10): known to work in principle, device-only to prove.** The FM-1 chip has a second core (core 1). Two community firmwares already use it: **ChoralRoot** (github.com/Quixotic7/ChoralRootFM1, v0.15/0.16, doc `docs/DUALCORE.md`: it renders its bass part on core 1) and Melodee / fm1-x0x (the HAL it was ported from; GPL-3.0). What they teach:
- **Start-up:** core 1 is started from `firmware/hal/fm1_cpu1.{h,S}`: entry address written at the top of RAM (0x01C7FFF8), its own interrupt bank, a private 2816 B stack, its code placed in RAM (`.c1_text`) so flash being off never stops it. It must be started **before** `audio_init` and the timers, with the PC-limit guard opened for the start and re-armed after.
- **Work split per audio block:** core 0 does events, matrix and LFO ticks, voice allocation and renders part 0; core 1 renders part 1 into its own buffer; core 0 then waits (max 8 ms) and mixes everything (sends, FX buses, master) in one core's order, so the output is **bit-identical to a single-core render**. Hand-over by single-writer flags and a job function pointer with a checked sequence number, no locks; caches are shared (`csync`).
- **Stability lessons (important):** other firmwares crashed after playing notes. Cause found by Melodee: the chip runs above the vendor's tested frequency on the boot loader's rails, and with two cores running the supply margin is too small on some units (core 1 misreads its mailbox, core 0 data corrupts). Fixes: raise the core and DC rails (SYSVDD 1.35 V, VDC14 1.45 V) before starting core 1 (more on a misread), validate every job (sequence + check word, refuse a null function), 8 ms stall timeout with core 0 rendering the block itself and core 1 held, core 1 faults handled without a reset, and a **per-device Dual Core On/Off option** (effective at next power-on). The margin differs from chip to chip.
- **Gain:** for ChoralRoot the benefit is mainly the voice budget (its bass part is light). For DUELL the two parts are about equal in weight, so core 1 can take **part 2 and core 0 part 1**: close to half of the render time, which pays for the morph cross-fades, LAYER mode and the effects.
- **Risk:** ChoralRoot's author reports the code "built and verified on the host, not run on hardware, because the FM-1 unit is bricked". It must be tested on the real unit with the recovery path ready.
- **DUELL plan:** (1) device toolchain first (Docker, JieLi toolchain, 3 SDK files; section 6); (2) port the HAL (GPL-3.0, credit Quixotic7, Melodee and fm1-x0x) and the per-part job split in `mix_block` (our part 1 / part 2 split is the same shape as theirs), single-threaded and bit-identical on the host and in the emulator; (3) ship with **Dual Core default OFF** in the first builds, an option to turn it on, the rail boost and all fail-safes of ChoralRoot 0.16; (4) the owner tests on the unit and reports before it becomes the default. The emulator cannot prove stability, only logic.

**Other things learned from JIANT** (github.com/juanjiant-bit/JIANT, a Felucca fork): a visible **CPU load meter** on the screen; "keep divisions and LFO maths out of the audio interrupt's inner loop, compute once per block" (we already do it per block); a master **DUCK** and a slow leveler (about 1.5 s) in the master processing (check how DUCK is triggered before building our MIDI sidechain); five reverb models; static build-time memory only; a note that a big-buffer pool at 92% left only 25 KB (our SPLIT bus doubling added 16.7 KB, so watch the pool on the device).

---

## 6. CPU and engineering plan

- Measure first: in the emulator (Felucca's instruction counts per sample per preset) and on the device when flashing is set up. The first deliverable is a table: cost per engine per voice, cost of the new LFO/envelope/matrix, cost of a waveform cross-fade.
- **Parts are tracks 1 and 2 of Felucca's 4-track structure.** Changing the track count to 2 breaks the saved-data layouts (static asserts in `core.h`) and about 120 test references, so tracks 3 and 4 are disabled instead: not shown, no voices, their MIDI channels ignored. Storage formats and the regression suite stay valid. (Tried and reverted: NPART 2.)
- **Storage decision (2026-10-10, from the step-3 finding):** the project file has room for 4 more parameters per track, step 3 needs 23, and parameter ids are 7-bit in motion, user presets, MIDI LEARN and the editor protocol (limit 127). DUELL (a) stores only parts A and B in the project file (new format FUN11; older projects still load by count), and (b) reuses the 29 dead parameter ids (P_FM1_ATK..P_FM4_LEVEL, P_LN0..7, P_ED_FX) for the new parameters, so no format grows. This ends the FELUCCA_FM4 and FELUCCA_DRUM build options and Felucca-project compatibility, which DUELL does not need. About 22 ids stay free for the morph settings and the arp target.
- Build order: (1) rename and rebuild in the emulator, tests green; (2) two parts and Stereo/Split; (3) UI fixes (1.1-1.3) with the SELECT/tempo test; (4) second LFO, second envelope, 8-slot matrix (needs the storage decision below); (5) MORPH with undo; (6) MIDI map and the host tests; (7) polish, CPU report; (8) device build (needs Docker, the JieLi toolchain and 3 SDK files).
- Every step ends with the host regression suite green (`tests/run_tests.sh`), golden hashes updated only for intended sound changes.

### 6.1 CPU load meter, splash, ABOUT, version name (owner's requests, built 2026-10-12)

- **CPU load meter** (moved to **GLO > TEST**, section below, in the 0.1-beta test build; MENU > SYSTEM > INFO keeps its FIRMWARE / USB / CPU rows with a line pointing to GLO > TEST, and ABOUT keeps its CPU line). The description that follows is the meter's, as it now appears on the TEST page. Placement under the label law: INFO is a read-only screen reached with SELECT (rows) and KNOB 1 right or OCT+ (open) like ABOUT (section 6.2); no new button, no knob changes anything. It shows **AVERAGE 1 S** (percent of the half buffer's time, with a bar and a mark at 80 %), **PEAK HOLD** (the longest half of the last second, held 4 s, then it falls to the latest), **MISSED SINCE POWER-ON** (halves that took longer than their time, or that the DMA overtook) and a word: OK, NEAR THE LIMIT (average 80 % and up, amber), AT THE LIMIT (95 % and up, red), BLOCKS MISSED JUST NOW (a miss in the last 5 s, red). Flat fills only (HI-CON included); GREY and MONO have no colours, the word carries it. No always-visible indicator and no footer dot (open question).
- **What it measures.** Device: `fm1_ticks()` (TIMER4, 24 MHz) across one I2S half's render in the audio ISR (`audio.c fm1_alnk0_irq`), minus the TIMER5 ticks nested in it (the key scan): the audio's own time against the half's budget (`CPU_BUDGET_US` = 128 frames / 44.1 kHz). The ISR only adds to `cpu_sum` and `cpu_n`, keeps `cpu_pk` and `cpu_miss` (`core.h`): three adds, one compare, no division. The UI reads them once a second **only while TEST, INFO or ABOUT is shown** (`ui_menu.c cpu_meter_poll`). The older 93 ms IIR (`song.cpu_q8`, console `cpu_pct`) is unchanged. Emulator (`FELUCCA_METER_EST`, `web/emu/felucca_web.c`): the browser has no clock in the audio worklet, so the meter shows an **estimate**: 16 % for the buses plus 10 % per sounding voice (8 voices read 96 %), labelled EMULATOR ESTIMATE on the screen; only the device measures. Host tests drive the counters directly (`tests/audio_test.c`, `tests/splash_test.c`).
- **Splash** (`ui_draw.c splash_frame`): at power-on and after 'Switch on' in the emulator, 1.2 s and then straight to the normal screen, any button, key or knob ends it (and is used up by that); the synth is already running (the first frame is drawn at `lcd_init`, the rest by the main loop, no delay), MIDI notes play through it, the keys after 0.1 s. The title block is a **graphic**, the art director's lockup `felucca-base/assets/splash_logo.svg` (source of truth; viewBox 480 = the screen at 2x: `DUELL` as an embedded PNG, `MultiMorphing` / `Bitimbral Synthesizer` and `by OSCILL/R` as vector outlines), used exactly as designed (at 1x: ink from x = 22, `DUELL` at y = 64, the tagline from y = 119, `by OSCILL/R` at y about 174..187), not drawn from a font. `tools/gen_splash_logo.py` (run by `build.py` and `gen_only.py`) rasterises it at build time to a 1x coverage mask (paths flattened and filled even-odd at 8x8 supersampling, the PNG's alpha resampled by 0.5, box-filtered), cropped to its ink box (166 x 124) and stored as `build/gen/splash_logo.h`: 4-bit coverage as a byte run list (about 4.3 KB, decoded in `splash_logo_draw` without RAM). It is alpha-blended in the theme's one text colour over the lines (HI-CON: its text colour), nothing behind it. The only background is 15 thin lines in the MORPH page's ribbon style (smooth on the left, overtones and ripples toward the right as the morph sweeps, no centre line; they run on unbroken behind the text, which is drawn on top: no box, gap or halo, the background is the theme's everywhere off the lines), coloured from the selected theme's part 1 colour (top line) to its part 2 colour (violet to pink in VIOLET). HI-CON: flat red lines above, flat blue below, no blend. MENU > ANIM OFF: the first frame stays still.
- **Default theme:** VIOLET (`UI_DEFAULT_INDEX` 0, the first palette; a unit with no valid settings starts in it; saved settings keep their theme).
- **ABOUT** (MENU > SYSTEM > ABOUT): the **first view is the splash animation**, drawn by the splash's own code (`ui_draw.c splash_frame`): the thin morphing lines in the theme's part 1 to part 2 colours with the lockup on top in the text colour, animating continuously (a step every 66 ms, no timeout, nothing ends it; MENU > ANIM OFF freezes the first frame, HI-CON keeps its flat red and blue lines; it is redrawn only while the page is shown). The footer hint stays `SELECT SCROLL, K1 BACK`. SELECT scrolls on to the information over the plain themed background: the title block, `DUELL 0.1 beta`, the build date, the CPU line, `based on Felucca (GPL-3.0)`, the Felucca copyright and links, the credits; scrolling back to the top returns to the animation. OCT-, KNOB 1 left and HOME leave as before. The power-on splash itself is unchanged (1.2 s).
- **Version name:** `FELUCCA_VERSION` is `DUELL 0.1 beta` (splash-free places: ABOUT, INFO, console, editor info). `tools/build.py --release 0.1-beta` passes `DUELL 0.1 beta` and writes `duell-0.1-beta.fwsc` (identity still `FM-1_901`, the loader marker unchanged); the installer page loads `duell-0.1-beta.fwsc` and still accepts the old `felucca-0.1-beta.fwsc`.

- **GLO > TEST (the CPU test page, 0.1-beta test build; the last page of GLO after CLOCK; a tap of GLO or SELECT reaches it like the others; keys play notes, the page is not locked).** The panel shows the loaded **MULTI** (`Mnn NAME`, or NO MULTI when the music did not come from one), for **part 1 and part 2 the ENGINE name and the PATCH name** (the stored patch's name or the factory sound's), then the CPU meter exactly as described above, moved from INFO: AVERAGE 1 S with the bar, the 80 % mark and the peak tick, PEAK HOLD, MISSED (since the last RESET), the state word OK / NEAR THE LIMIT / AT THE LIMIT / BLOCKS MISSED JUST NOW, the same flat colours as before (HI-CON text colour at OK). Knobs, each doing what its label says: **K1 MORPH** and **K2 ROLL** are the MORPH page's (the same code: the morph fader of the part(s) named by MORPH TARGET; ROLL A|B with the twist-past-the-detent gesture and its rest rule), **K3 LOAD** (card label LOAD; the multi's number below it) steps through the used multis M01..M16 (empty slots skipped, clamped at both ends, several detents in one turn load only the one it stops on) and **loads the one it stops on at once**, as the MULTIS page's LOAD, with the message `LOADED Mnn NAME` (no dialog; playing goes on), **K4 RESET** (any turn) clears the peak hold and the missed count of this page and restarts the average window (`RESET`); INFO and ABOUT keep their since-power-on count. Host tests: `tests/multi_test.c` (group 3b), `tests/select_test.c` (page order).
- **The four CPU TEST multis M13..M16** (factory multis in ROM by the same mechanism as M01..M04: an empty slot shows and loads them, SAVE over one is an overwrite, ERASE restores; M05..M12 stay empty for users). Records in `felucca-base/tools/factory_multis/M13.bin..M16.bin`, built by `felucca-base/tests/gen_test_multis.c` with the firmware's own code (sound loader, parameters, multi.c's packer) and checked by `tools/gen_duell_multis.py` (names, the intended sounds, the output MODE). All four: both parts' arp UP at 1/16, CHRD MAJ7 (a 4-note chord from one key), VOICE POLY, REL about 500 ms (511 ms), tempo 138, chorus + delay + reverb sends, an INSERT on each part, EQ (LOW +3, MID +2, HIGH -2), the PUMP ducker (depth 40), TEMPO 138. **M13 TEST 1 BASE**: #64 JAZZ CLUB (WHEEL) + #31 SILK CHORUS (ANALOG), LAYER, HALL, PHASER / CHORUS inserts (the 58 % baseline). **M14 TEST 2 PHASE**: #75 CRYSTAL CAVE (PHASE) + #31 SILK CHORUS (ANALOG), the same (the 73 % / 27 missed blocks case). **M15 TEST 3 HEAVY**: #59 TINE ROOM (FM6) + #75 CRYSTAL CAVE (PHASE), SPLIT, SHIM reverb, PING-PONG on, FLANGER inserts, sends 50. **M16 TEST 4 MIX**: #43 SEA ORGAN (NOISE) + #35 WOW FLUTTER (LOFI), LAYER, sends 60 (PHYS and GRAIN have only 3 voices per part, so they cannot fill the 8 voices and were not used). Numbers are PATCHES-list numbers. Verified in the host sim: a held key reaches 8 busy voices in each (TEST 3 in SPLIT: key on part 1 plus MIDI channel 2 for part 2, since SPLIT's keys play the selected part only). BASS+ is the device-wide SPEAKER EQ setting (MENU > AUDIO), not stored in a multi, so the multis cannot carry it: testers set it by hand.

### 6.2 MENU audit, MIDI in one place, first start, read-only screens (owner's feedback after the first flash, 2026-10-12)
- **MENU after the audit** (4 tabs; the MIDI tab is gone): DISPLAY: COLOR, STYLE, LARGE, ANIM, LEDS, SCREEN OFF, SCOPE, **OVERVIEW** (was HOME: what HOME's last page shows under its cards, SCOPE or PARTS; Felucca's LEVELS, four tracks' levels, did not exist with two parts and is gone; a stored 2 reads as PARTS). CONTROL: HOLD, KNOB ACCEL, FX LATCH, SCALE LEDS, STEP PREVIEW, CHORD ENTRY, **HOME OPENS** (new: the page HOME opens from another page, at power-on and when the menu closes: MORPH (default), MODE, MIXER or OVERVIEW; HOME tapped on HOME's own pages still goes round MORPH, MORPH SET, MODE, MIXER, OVERVIEW; settings byte [15][2], 0 = MORPH). AUDIO: SPEAKER EQ, USB LEVEL, CLICK, CLICK LEVEL, COUNT-IN, TUNE. SYSTEM: USB SERIAL, RESTORE LAST, HELP, CALIBRATION, INFO, ABOUT. Gone: MIDI IN and LEARN CLEAR (now GLO > CHANNELS K3 and GLO > MIDI LEARN K2). Kept on purpose: STEP PREVIEW and CHORD ENTRY (the STEP page and its recording are still Felucca's), CLICK / COUNT-IN (REC still arms and records), TUNE (the project's value; not in a MULTI), USB SERIAL (macOS audio needs it OFF).
- **MIDI in one place.** Every MIDI setting is on a GLO page: CHANNELS (K1 P1 CH, K2 P2 CH, K3 MIDI IN OWN CH / SELECTED, with where each part is heard and a line saying what MIDI IN does), SIDECHAIN (K1 SC SRC, K2 SC NOTE), MIDI LEARN (K1 LEARN, K2 CLEAR), CLOCK (K1 TEMPO, K2 SWG, K3 CLK = INT / USB / TRS). Nothing else in the firmware is a MIDI setting (the keys always send on the selected part's channel; USB and TRS share the channel state; there is no THRU, CC-send or transport switch). Editor protocol: MENU item id 21 (MIDI IN) is retired (never reused), id 25 HOME OPENS is new, id 23 now names OVERVIEW with two values (SCOPE, PARTS); stored formats unchanged.
- **First start** (a unit with no valid settings: new, stock firmware's data or garbage in the settings sector, a flash erase; `project.c persist_boot` sees `settings_import` refuse the record): the theme is VIOLET (`UI_DEFAULT_INDEX` 0), MODE is LAYER and factory multi M01 is loaded exactly as SAVE > MULTIS' LOAD does (`ui_input.c first_boot_poll` -> `project_load(0)`, read from the factory multi path: tempo, globals and mode are what M01 holds; the owner's replacement of M01 changes the first start with no code change). Not done when an autosave was restored. The settings are saved at once, so the next start is ordinary (autosave / saved settings); any valid settings record, old or short, keeps its theme and mode untouched. Host test: `multi_test` group 4.
- **Read-only screens (INFO, ABOUT).** SELECT picks the row; KNOB 1 turned right opens the screen (any knob does, as on every menu row; OCT+ still does); KNOB 1 left or OCT- closes it, HOME closes the menu. The hint row says it (SELECT ROW, K1 OPEN; ABOUT: SELECT SCROLL, K1 BACK). CALIBRATION is a setup, not read-only: OCT+ only.

---

## Appendix A: ARP TARGET pitch units per engine
Not used here: ALGORITHM selects the part. FM6's algorithm 1-32 stays an EDIT parameter and is excluded from the morph (it cannot be faded).

DRUM is removed from DUELL (its row is dropped). SLICE stays in this table until its removal is confirmed. Read from `felucca-base/firmware/src` (2026-10-10). **Common path**: `voice.c:track_render` builds one pitch per voice (`v->pitch_cur` with glide, plus TUNE, bend, LFO/ENV pitch) into `vmod_t.pitch16` (1/16 semitone), `.inc` (`pitch_inc`) and `.fine`; `mod.c:mod_voice` then adds the matrix PITCH route and re-derives all three; then `engine->render(t, v, out, n, &m)`. ARP TARGET adds a second set (arp pitch16/inc/fine) built at that same point; each engine picks per unit which set it uses. "Root" = the held key's pitch.

| Engine | Units (mask bits) | Where each unit's pitch is derived from the voice pitch (file:function) |
| --- | --- | --- |
| ANALOG | OSC A, OSC B (2 units; **no separate SUB**, see findings) | A: `m->inc` = `inc1` in `eng_analog.c:analog_osc` and `analog_render_x`. B: `cents_inc(m->pitch16, DTN, 0)` = `inc2` in `analog_osc`; in WAVE SUB it is `cents_inc(...) >> 1` (octave below A, `analog_render_x`); in WAVE SYNC it is `pitch_inc(m->pitch16 + DTN*3 + shape)` (`analog_render_x`). |
| FM6 | OP1..OP6 | One value per block: `eng_fm6.c:fm6_render` computes `lf = fm6_note_logfreq(m->pitch16 + (TRNSP-24)*16, m->fine)` and passes it down; `fm6_core.c:fm6_note_compute` then sets per operator `op[k].freq = fm6_freq(logfreq + base[k] + pitch_mod + dt[k])`. Splitting = pass a second `logfreq` and pick it per k. Patch index 0 is OP6 (OP1 = index 5). `base[k]` is fixed at note-on from `v->note` (`fm6_note_init`, `fm6_osc_base`). |
| PHASE | LINE 1, LINE 2, SUB | `eng_phase.c:phase_render`: line 1 `inc = m->inc`; line 2 `inc2 = pitch_inc(m->pitch16 + DTN)` (runs only if DTN != 0 or LINE = RING); SUB is `ph2 += inc >> 1`, so it follows line 1 and needs its own `inc` to be a unit. WAVE and WAVE2 alternate per cycle on each line; they are not units. |
| LOFI | ALL only | `eng_lofi.c:lofi_render`: one oscillator, `pitch = m->pitch16 - sweep + ARPS[E6][step]*16`, then `pitch_inc` (the engine has its own chord arp, ARP E6). |
| SAMPLE | ALL only | `eng_sample.c:sample_render`: one playback rate, `d16 = m->pitch16 + TUNE*16 - z->root16` -> `pow2_q16`. The zone is picked once at `sample_note_on` from `v->note`, not from the arp pitch. Missing-data sine: `smp_sine`. |
| VOICE | ALL only (F0 only) | `eng_formant.c:formant_render`: glottal pulse `inc = m->inc`. The formant filters do not follow the pitch except a fixed key tracking (FOLOW 30/127) and an output gain, both read from `v->pitch_cur` directly (not from `m`). |
| TRIO | OSC1, OSC2, OSC3 | `eng_trio.c:trio_render`: `inc1 = m->inc`; `inc2 = trio_inc(m->pitch16, INT2, +DTN, fine)`; `inc3 = trio_inc(m->pitch16, INT3, -DTN, fine)`; `trio_inc` -> `cents_inc`. `fine` is recovered from `m->inc` vs `pitch_inc(m->pitch16)`. Sets with RING (osc1 flipped by osc3's phase) or SYNC (osc2 restarts with osc1) couple units; some sets mute an oscillator (NOIS, RING, SYNC, SYNCP, P+N). |
| WHEEL | SUB (16', 5 1/3'), BODY (8', 4'), TOP (2 2/3'..1'): the SUB/BODY/TOP knob groups of `drw_level`; or 9 drawbars | `eng_wheel.c:wheel_render`: per partial k, `inc = drw_ratio(k, x)` with `x = m->inc` (folded down an octave above ~6 kHz, `DRW_FOLD_LIM`). Each partial has its own phase (`drw_vc_t.ph[k]`), so any split works. The percussion partial rides partial 3 or 4 (`wheel_block`, `T->pk`); the key click is unpitched. |
| GRAIN | ALL only | `eng_grain.c:gr_spawn`: `d16 = m->pitch16 - z->root16 + PTCH*16 + random`, read once per grain (a running grain keeps its rate; a change is heard from the next grain). Zone from `v->note` at `grain_note_on`. |
| PHYS | BODY (main resonator/string), SYMP strings (MODEL SYMP only) | `eng_phys.c:phys_render`: `f0 = m->inc` into `px_modal_block` / `px_memb_block` / `px_string_block`. `phys_symp_pitches`: `m->pitch16 + PHYS_CHORDS[CHRD]` cents -> `pitch_inc` per sympathetic string; CHRD = ROOT uses the track ROOT (does not follow pitch at all). MODAL and MEMB are partial banks of one body (STRC stretches them): ALL only there. |
| NOISE | CLOCK (LFSR and META modes), FILTER (key tracking) | `eng_noise.c:noise_render`: LFSR `inc = noise_inc(m->pitch16 + CLK*16 + drift)`; META the same, or `m->inc` when CLK and drift are 0; filter `cut += (m->pitch16 - 60*16) * TRK`. ANLG and DUST have no pitch except the filter tracking: ALL only there. |
| SLICE | ALL only; the key must not move it | `eng_slice.c:slice_render`: `stepq` uses `m->pitch16 - v->pitch_cur + PTCH*16`, i.e. the key pitch is cancelled (bend, LFO and matrix still move the speed); the key selects a slice at `slice_note_on` (`slc_note_slice(v->note)`). Only the missing-data sine (`smp_sine`) plays the key's pitch. |

Findings that change the plan in 2.3:
- **ANALOG has two oscillators, not three.** SUB exists only as WAVE SUB, where it *is* OSC B (an octave below A). Mask = A, B. In WAVE SYNC, B on the arp while A holds the root gives a sync sweep.
- **ANALOG OSC B ignores `m->fine`** (`cents_inc(..., 0)`): unison detune, TUNE and bend reach only OSC A today. A per-unit pitch must carry its own `fine`.
- **SLICE cannot take a split pitch**: it cancels the key pitch (the note chooses the slice). If `arp_pitch16` replaced `pitch_cur` inside `m->pitch16`, SLICE's subtraction of `v->pitch_cur` would have to use the same value.
- **VOICE reads `v->pitch_cur` directly** (formant tracking, output gain) and **ANALOG KTR reads `v->pitch16`** (`analog_osc`, the `cut` term), bypassing `m`: they follow the root, never the arp, unless changed on purpose.
- **FM6**: a fixed-frequency operator (`FP_MODE` = 1) ignores pitch, so a mask bit on it does nothing. Keyboard level/rate scaling and `base[k]` come from `v->note` at note-on only. Per-operator pitch is a small change in `fm6_note_compute` (the sum is already per k).
- **Set at note-on**: SAMPLE, GRAIN and SLICE pick zone/slice from `v->note` when the voice starts; with a partial mask the one held voice keeps its root zone.

## Appendix B: randomisable and excluded parameters
Read from `core.h` (enum P_*, G_*), `params.c` (TP[], descriptors, pages) and each engine's `edit[]` and `desc` hook. Tags: **continuous** = random offset, blends; **choice-fadable** = a choice inside an engine / LFO wave / filter type / matrix src+dst that can be cross-faded (3.3); **choice-hard** = cannot be faded, never rolled; **excluded** = never rolled (rule 3). Owner decisions of 2026-10-10 are applied; DRUM is removed from DUELL (its engine table and lane levels P_LN0..7 are gone from these tables) and SLICE stays. A **?** now appears only on an item that is also in the Review list below. A family row (x4, x8, x20) is one row for identical parameters. New DUELL parameters (ENV 2, LFO 2, matrix slots 5-8) take the tag of their Felucca twin.

### Review list (open questions) — SETTLED 2026-10-10: all 13 as recommended (see REVIEW_LIST.md)
Each item is a parameter whose class is not settled. Items are numbered; the table rows carry a **?** only for these.

1. **Arp rhythm: P_AGATE, P_ASWING, P_APROB (GATE, SWG, PROB).** Gate = how long each arp note is held; swing = how late every second note is pushed (a shuffle feel); probability = the chance each step actually plays. They change the feel of the pattern, not the tone. Options: excluded, or continuous. Recommendation: excluded, so a morph never changes the groove you set (a morph is about sound; the groove is performance).
2. **ANALOG WAVE (E0).** The oscillator shape: saw, square, triangle, sine, pulse-width, hard-sync, sub-octave. The first five share one code path and can be cross-faded; SYNC and SUB use a different one. Options: fade among the first five only, or fade among all seven, or exclude. Recommendation: fade among the first five, never roll to or from SYNC or SUB.
3. **FM6 macros FB and MEG (E1, E4).** FB is the feedback amount (more = brighter, harsher, noisier tone); it moves in whole steps -7..7. MEG scales all the envelope speeds at once (it changes how fast sounds attack and fade, heard from the next note). The earlier safety analysis of these two is not in this brief and was not available to me. Options: continuous, or excluded in the first build. Recommendation: FB continuous with a limit of 2 steps from the original, MEG continuous but small, both only after you hear them in the emulator; until then excluded.
4. **FM6 own patch, later step (FP_*, needs the `fm6_set_patch` path).** Per-operator envelope rates and levels (how each of the six sine-wave partners swells and fades), per-operator fine frequency (FF) and detune (DET) (tiny pitch offsets between operators), keyboard and rate scaling (how the sound changes up the keyboard), and the voice LFO speed, delay and depth items. Options: continuous with caps, or exclude. Recommendation: later step only; envelope rates/levels and LFO items continuous, FF and DET excluded (they are pitch-like), scaling items excluded (they are switch-like).
5. **PHASE DTN (E4) from zero.** Detune of the second oscillator line. At 0 line 2 is off; any value above 0 turns it on and adds thickness. Options: continuous (can switch line 2 on), or continuous only if the original is not 0. Recommendation: only if the original is not 0 (same spirit as "zero stays zero"), so the morph never adds a second line.
6. **PHASE LINE (E5).** Chooses MIX (two lines added) or RING (two lines multiplied: metallic, bell-like). RING with DTN at 0 squares the signal and sounds wrong. Options: choice-fadable, or excluded. Recommendation: excluded.
7. **LOFI WAVE (E1).** The chip waveform: pulse, triangle, saw, noise, wave-RAM. Noise and wave-RAM carry their own state, so a cross-fade can click or glitch. Options: fade among pulse, triangle, saw only, or exclude. Recommendation: fade among pulse, triangle, saw; never to or from noise or wave-RAM.
8. **LOFI DUTY (E2).** Pulse width of the chip pulse wave (only four true settings), or the wave-table number in WRAM mode. Options: excluded, or continuous in pulse mode only. Recommendation: excluded (four fixed widths do not blend).
9. **LOFI CRSH (E3).** How coarsely the chip sound is stepped (bit-crush feel); in STEP chip mode it is a decay time instead. Options: continuous, or excluded. Recommendation: continuous except in STEP mode, where it is excluded.
10. **VOICE SHIFT (E3).** Moves the vowel colour up or down in semitones without changing the sung pitch (a smaller or bigger mouth). Not a pitch parameter, but it is in semitones. Options: continuous, or excluded. Recommendation: continuous, capped at 3 semitones from the original.
11. **WHEEL ROTR (E7).** Rotary speaker: OFF, SLOW, FAST. The speeds ease by themselves, so a change is smooth already. Options: choice-hard (never rolled), or choice with a normal AMOUNT chance. Recommendation: choice-hard (never rolled), because a speaker that suddenly switches to fast is a gesture, not a variation.
12. **PHYS DAMP and BOW/BEND/BUZZ (E3, E6).** DAMP is how fast the body stops ringing (at the top it rings forever). BOW/BEND/BUZZ is how hard the string is bowed, bent or buzzed, which keeps the sound sustained. Options: continuous with a ceiling, or excluded. Recommendation: continuous, with a ceiling so a roll cannot give a note that never ends.
13. **Global effects G_DTIME .. G_CDEPTH (delay, reverb, chorus settings).** Shared by both parts, so morphing one part would change the other. Options: excluded, or morph them only when both parts share the same morph. Recommendation: excluded.

### B.1 Shared per-track parameters (P_* in core.h)
| Parameter | Label | Tag | Reason |
| --- | --- | --- | --- |
| P_LEVEL | LVL | excluded | level |
| P_ATK, P_DEC, P_REL | ATK DEC REL | continuous | times, log domain; with P_ESYNC on they are note values (stay on the grid). No effect on FM6 (own envelopes) |
| P_SUS | SUS | continuous | level of the sustain; keep above a floor (never silent) |
| P_ED_FLT, P_ED_SHP | FLT SHP (ENV DEST) | continuous | env depth |
| P_ED_PIT | PIT (ENV DEST) | continuous, only if original != 0 (zero stays zero) | env to pitch depth (rule 3a) |
| P_ED_FX | FX | excluded | unused, kept for formats |
| P_LRATE | RATE (LFO) | continuous | log Hz; only heard when P_LSYNC is OFF |
| P_LWAVE | WAVE (LFO) | choice-fadable | both shapes run (3.3) |
| P_LPHASE | PHS | continuous | start phase |
| P_LFADE | FADE | continuous | time |
| P_LD_FLT, P_LD_SHP, P_LD_AMP | FLT SHP AMP (LFO DEST) | continuous | depths |
| P_LD_PIT | PIT (LFO DEST) | continuous, only if original != 0 (zero stays zero) | vibrato depth (rule 3a) |
| P_AMODE | MODE (ARP) | choice-hard | arp on/off and order |
| P_ARATE | RATE (ARP) | choice-hard | tempo division |
| P_AOCT | OCT | excluded | octave range of the arp (rule 3) |
| P_AGATE, P_ASWING, P_APROB | GATE SWG PROB | excluded ? (Review 1) | rhythm of the arp, not the sound |
| P_AHOLD, P_AORDER | HOLD ORD | choice-hard | latch, note order |
| P_ROOT, P_SCALE, P_QUANT, P_TRANS | ROOT SCL QNT TRN | excluded | key, scale, transpose (rule 3) |
| P_SLEN, P_SDIV, P_SSWING, P_SGATE | LEN DIV SWG GATE (SEQ) | excluded | pattern timing |
| P_DIST, P_CHOR, P_DLY, P_REV | DST CHO DLY REV | continuous | per-part send levels (the effects themselves are global, B.3) |
| P_VOICE | VCE | excluded | POLY/MONO/LEG/UNI (rule 3) |
| P_GLIDE | GLD | continuous | time |
| P_GLMODE, P_PRIO, P_ALLOC | GLMOD PRIO ALLOC | choice-hard | voice handling modes |
| P_DETUNE | DTUN | continuous | unison detune; cap at 10 cents (rule 3: DTUN 127 is about 40 ct) |
| P_PAN | PAN | continuous in STEREO mode only; excluded in SPLIT | rule 3b |
| P_MUTE | MUTE | excluded | performance switch |
| P_FREEA..P_FREED (ids 45..48) | - | excluded (never; MF_GONE) | free since the SLICER insert was removed (5c.5): reserved for the universal filter. They still take the random draws the SLICER's tags took (SLCR PAT RATE: a switch's in WILD; DEPTH: one move), so every saved End B seed rolls the rest as before |
| P_M1SRC..P_M4SRC (x4) | SRC1..4 | choice-fadable | route fades out/in (3.3) |
| P_M1DST..P_M4DST (x4) | DST1..4 | choice-fadable | as above. Routes to PITCH or VIB: only if the original route is non-zero (rule 3a). Never roll a route onto an excluded or choice-hard parameter (E1..E8 of FM6 ALG/SLOT, SAMPLE SET, ...) |
| P_M1AMT..P_M4AMT (x4) | AMT1..4 | continuous | amount; for a route to PITCH or VIB: continuous, only if original != 0 (zero stays zero) |
| P_FM1_ATK..P_FM4_LEVEL (x20) | ATK DEC SUS REL LVL per OP1-4 | excluded | inert: DIGITAL (FELUCCA_FM4=0) is not built; on no page |
| P_CHRD, P_VOIC | CHRD VOIC | choice-hard | chord keys change which notes play |
| P_LSYNC | SYNC (LFO) | choice-hard | tempo sync mode |
| P_LTRIG | TRIG | choice-hard | NOTE/FREE restart |
| P_LPOL | POL | excluded | LFO polarity BI/UNI (not poly/mono; rule 3) |
| P_SQNT | QNTZ | excluded | step timing |
| P_SPRD | SPRD | continuous in STEREO mode only; excluded in SPLIT | voice stereo spread around PAN (rule 3b) |
| P_ITYPE | INSRT | excluded (owner: insert effects stay as they are) | The insert is a per-part effect with types OFF, SOFT, HARD, FOLD, FUZZ, CRUSH, PHASR, FLANG, CHOR, three values A B C and MIX (`fx.c` `ins_desc`) |
| P_IA, P_IB, P_IC | INS A B C | continuous within the set type | meaning set by the type (`ins_desc`); the type never changes |
| P_IMIX | MIX | continuous | dry/wet |
| P_FTYPE | TYPE (FILTER) | choice-fadable | ANALOG only: LP/BP/HP SVF outputs mixed (3.3); unused by other engines |
| P_ESYNC | ESYNC | choice-hard | ENV time mode (ms or note values) |
| P_E0..P_E7 | engine labels | see B.2 | engine-specific |

### B.2 Engine-specific parameters (P_E0..P_E7)
Type: enum(n) = F_ENUM with n values; pct, bipct, time, cutoff, semi, int = the F_* format. Same tag words as above. The engine itself (G_ENGSEL) is never rolled.

**ANALOG** (`eng_analog.c`)
| E | Label | Type | Tag |
| --- | --- | --- | --- |
| 0 | WAVE | enum(7) SAW SQR TRI SIN PWM SYNC SUB | choice-fadable ? (Review 2; fade only among 0-4: same loop; SYNC and SUB run `analog_render_x`, never roll to or from them) |
| 1 | DTN (shown SYNC in WAVE SYNC) | int, ct 0-127 | continuous (fine detune, cap 10 ct; in WAVE SYNC it is the sync interval, free) |
| 2 | MIX (shown SUB in WAVE SUB) | pct | continuous |
| 3 | NOIS | pct | continuous |
| 4 | CUT | cutoff | continuous |
| 5 | RES | pct | continuous (resonance/drive coupling, rule 4) |
| 6 | DRV | pct | continuous |
| 7 | KTR | pct | continuous |

**FM6** (`eng_fm6.c`: macros on top of the track's patch)
| E | Label | Type | Tag |
| --- | --- | --- | --- |
| 0 | ALG | int 0-32 (PAT, 1-32) | choice-hard (cannot be faded) |
| 1 | FB | int -7..7 | continuous ? (Review 3; macro, first build only if cleared) |
| 2 | MLVL | bipct | continuous (macro, first build; live each block) |
| 3 | MRAT | int -16..16 | excluded (integer ratio steps are semitone-like coarse tuning) |
| 4 | MEG | bipct | continuous ? (Review 3; macro, envelope rates, heard from the next note) |
| 5 | VMOD | int -7..7 | excluded (first build morphs only the macros MLVL, DTUN, FB, MEG; taken at note-on) |
| 6 | DTUN | pct | continuous (macro, first build; capped at about 10 ct between the carriers) |
| 7 | SLOT | enum F1-F8, OWN, B1-B32 | excluded (loads a whole patch, `fm6_poll`) |

FM6's own patch (`fm6_core.c` FP_*, 155 bytes in `fm6_patch[]`, **not** in `t->p[]`: not automatable, not in the matrix, not in motion). The morph only reaches it through a new patch-blend path (`fm6_set_patch`, which makes SLOT = OWN and takes one undo). Until that exists: **leave all of it alone**; the first build morphs only the macros above. Playing safe (owner decision): per-operator output levels and the rest below are a **later** step. Tags for that step:
| FP_* (per operator x6, OP6 first) | Tag |
| --- | --- |
| R1-R4, L1-L4 (envelope rates, levels) | later: continuous ? (Review 4) |
| OL (output level) | later: continuous (keep carriers audible; needs `fm6_set_patch`) |
| FF (fine freq), DET (detune -7..+7) | later: ? (Review 4; recommended excluded) |
| FC (coarse), MODE (ratio/fixed) | excluded (coarse tuning; ratio/fixed cannot be faded) |
| BP, LD, RD, LC, RC (level scaling), RS (rate scaling), AMS, KVS | later: ? (Review 4; recommended excluded) |

| FP_* (voice) | Tag |
| --- | --- |
| PR1-4, PL1-4 (pitch envelope) | later: continuous, only if original != 0 (zero stays zero) |
| FB, LFS (speed), LFD (delay), LAMD (amp depth) | later: continuous ? (Review 4) |
| LPMD (pitch depth) | later: continuous, only if original != 0 (zero stays zero) |
| ALG | choice-hard (as E0) |
| OKS (osc key sync), LKS (LFO key sync), LFW (LFO wave, 6), LPMS (pitch mod sens) | choice-hard (never rolled) |
| TRNSP | excluded (transpose) |
| NAME | excluded |

**PHASE** (`eng_phase.c`)
| E | Label | Type | Tag |
| --- | --- | --- | --- |
| 0 | WAVE | enum(8) SAW SQR PLS DSIN SPLS RSAW RTRI RTRP | choice-fadable |
| 1 | WAVE2 | enum(9), 0 = "-" (same as WAVE) | choice-fadable |
| 2 | DCW | pct | continuous |
| 3 | ENV | pct | continuous |
| 4 | DTN | int, ct 0-127 | continuous ? (Review 5; fine detune, cap 10 ct; from 0 it switches line 2 on) |
| 5 | LINE | enum(2) MIX RING | choice-fadable ? (Review 6; RING with DTN 0 squares the signal) |
| 6 | SUB | pct | continuous |
| 7 | "-" | int 0..0 | excluded (unused) |

**LOFI** (`eng_lofi.c`; E2 and E3 change meaning with CHIP/WAVE via `lofi_desc`)
| E | Label | Type | Tag |
| --- | --- | --- | --- |
| 0 | CHIP | enum(5) 4BIT 4B/2 8BIT 1BIT STEP | choice-hard (bit depth; STEP replaces the ADSR) |
| 1 | WAVE | enum(5) PLS TRI SAW NOIS WRAM | choice-fadable ? (Review 7; NOIS and WRAM carry state) |
| 2 | DUTY (WAV# with WAVE = WRAM) | int 0-127 | choice-hard ? (Review 8; only 4 pulse duties; table number for WRAM) |
| 3 | CRSH (DCY with CHIP = STEP) | pct | continuous ? (Review 9; steps; choice-hard in STEP) |
| 4 | SWP | pct | continuous |
| 5 | VIB | pct | continuous |
| 6 | ARP | enum(4) OFF OCT MAJ MIN | choice-hard (engine's own arp) |
| 7 | TONE | pct | continuous |

**SAMPLE** (`eng_sample.c`)
| E | Label | Type | Tag |
| --- | --- | --- | --- |
| 0 | SET | enum(SMP_NALL) | choice-hard (source material, zones) |
| 1 | TUNE | semi -24..24 | excluded (tuning, rule 3) |
| 2 | BITS | int 0-127 | continuous |
| 3 | LOOP | enum(2) OFF ON | choice-hard |
| 4 | CUT | int 0-127 | continuous |
| 5, 7 | "-" | int 0..0 | excluded (unused) |
| 6 | DRV | pct | continuous |

**VOICE** (`eng_formant.c`)
| E | Label | Type | Tag |
| --- | --- | --- | --- |
| 0 | VOWL | int 0-127 | continuous (position in the vowel table, interpolated) |
| 1 | VOWL2 | int 0-127 | continuous |
| 2 | TALK | time | continuous |
| 3 | SHIFT | semi -12..12 | continuous ? (Review 10; formant shift, not the pitch) |
| 4 | BUZZ | pct | continuous |
| 5 | BRTH | pct | continuous (at the top the voicing fades out: never silent) |
| 6 | Q | pct | continuous |
| 7 | RAND | pct | continuous |

**TRIO** (`eng_trio.c`)
| E | Label | Type | Tag |
| --- | --- | --- | --- |
| 0 | WAVE | enum(16) SAW3 .. R+SP | excluded (a wave change leaves no source and would click; sets also switch RING/SYNC and mute oscillators) |
| 1, 2 | INT2, INT3 | semi -24..24 | excluded (oscillator intervals are coarse tuning; in SYNC sets INT2 is the sweep) |
| 3 | DTN | int, ct 0-50 | continuous (cap 10 ct) |
| 4 | MODE | enum(4) LP BP HP NOT | choice-fadable (SVF taps mixed, `TRIO_MIX`) |
| 5 | CUT | cutoff | continuous |
| 6 | RES | pct | continuous |
| 7 | PW | pct | continuous |

**WHEEL** (`eng_wheel.c`)
| E | Label | Type | Tag |
| --- | --- | --- | --- |
| 0 | REG | enum(16) FLUTE .. FULL | choice-fadable (bar gains already ramp per block: `wheel_block`) |
| 1, 2, 3 | SUB, BODY, TOP | int -8..8 | continuous |
| 4 | PERC | enum(7) OFF 2ND 3RD 2SOFT 3SOFT 2SLOW 3SLOW | choice-hard (single trigger, changes bar level) |
| 5 | CLICK | pct | continuous |
| 6 | DRV | pct | continuous |
| 7 | ROTR | enum(3) OFF SLOW FAST | choice-hard ? (Review 11; rotor speeds ease by themselves) |

**GRAIN** (`eng_grain.c`)
| E | Label | Type | Tag |
| --- | --- | --- | --- |
| 0 | SRC | enum(SMP_NALL) | choice-hard (source; the seek index is rebuilt) |
| 1 | POS | pct | continuous |
| 2 | SIZE | pct | continuous |
| 3 | DENS | pct | continuous |
| 4 | PTCH | semi -24..24 | excluded (transpose, rule 3) |
| 5 | SPRD | pct | continuous |
| 6 | RAND | pct | continuous |
| 7 | TONE | pct | continuous |

**PHYS** (`eng_phys.c`; labels follow MODEL via `phys_desc`)
| E | Label (MODAL/STRNG; MEMB; SYMP) | Type | Tag |
| --- | --- | --- | --- |
| 0 | MODEL | enum(4) MODAL STRNG MEMB SYMP | choice-hard (different DSP and state, `phys_reset`) |
| 1 | STRC; HARM; CHRD | pct; pct; int with 8 names | continuous; SYMP: choice-hard (CHRD tunes the sympathetic strings) |
| 2 | BRIT | pct | continuous |
| 3 | DAMP | pct | continuous ? (Review 12; 127 rings on; keep a ceiling) |
| 4 | POS; POS; SYMP | pct | continuous |
| 5 | ACC | pct | continuous |
| 6 | BOW; BEND; BUZZ | pct | continuous ? (Review 12; BOW sustains the sound) |
| 7 | EXC | pct | continuous |

DRUM is removed from DUELL: its engine table and the lane levels P_LN0..7 are not listed.

**NOISE** (`eng_noise.c`; E5 and E7 change with MODE via `noise_desc`)
| E | Label | Type | Tag |
| --- | --- | --- | --- |
| 0 | MODE | enum(4) ANLG DUST LFSR META | choice-hard (generator) |
| 1 | COLR | pct | continuous |
| 2 | FREQ | cutoff | continuous |
| 3 | RES | pct | continuous |
| 4 | TRK | pct | continuous |
| 5 | DENS (ANLG, DUST); LEN (LFSR, META) | pct; int with names | continuous; LFSR/META: choice-hard (register) |
| 6 | DRFT | pct | continuous |
| 7 | CRSH (ANLG, DUST); CLK (LFSR, META) | pct; int, st | continuous; LFSR/META: excluded (clock = pitch offset, rule 3) |

**SLICE** (`eng_slice.c`; stays)
| E | Label | Type | Tag |
| --- | --- | --- | --- |
| 0 | SRC | enum(SLC_NSRC) BREAK USR1-3 PIANO | choice-hard |
| 1 | DIV | enum(6) 4 8 16 32 AUTO MAN | choice-hard (slice count) |
| 2 | START | int 0-31 | choice-hard (which slice each key plays) |
| 3 | PTCH | semi -24..24 | excluded (varispeed, transpose) |
| 4 | MODE | enum(3) ONE GATE LOOP | choice-hard |
| 5 | REV | enum(2) OFF ON | choice-hard |
| 6 | DCAY | time | continuous |
| 7 | TONE | int 0-127 | continuous |

### B.3 Global parameters (G_* in core.h, for completeness)
All **excluded**: they are shared by both parts or are system, tempo or file items. G_BPM, G_SWING, G_CLOCK, G_TUNE (tempo, clock, tuning); G_MIDI, G_SYNC, G_ROUTE (MIDI); G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX, G_RSIZE, G_RDAMP, G_RTYPE, G_CRATE, G_CDEPTH (global delay, reverb, chorus: a morph of one part would change the other; excluded, see Review 13); G_SLOT, G_NAME, G_LOAD, G_SAVE, G_CLRSEQ, G_INITSND (slots, names, actions); G_ENGSEL, G_ENGGO (engine selection); G_INFO, G_DRLVL, G_DRREV (readings and inert).

### B.4 Not classified, or classified on weak evidence
- Every parameter has a tag. The doubtful ones are exactly the 13 items of the Review list at the top of this appendix.
- Mode-dependent parameters (LOFI E2/E3, PHYS E1/E4/E6, NOISE E5/E7, ANALOG E1/E2) need the tag looked up with the current mode; a roll must not change the mode that decides the tag.
- DUELL's new parameters do not exist in Felucca (ENV 2, LFO 2, matrix slots 5-8): tagged by analogy only.

## Appendix C: open decisions
1. (Settled: SLICE stays, DRUM is removed.)
2. Section 3.5: the ROLL/UNDO/KEEP twist gesture (knob past a detent) is chosen; judge it in the emulator, four white keys are the fallback.
3. Appendix B Review list: 13 open questions awaiting the owner's answers.
4. Arp scale: check Felucca's scale list and its arp code (`seq.c`, `chord.c`) for reuse.
5. FM6 per-operator output levels and the rest of the FM6 patch: a later step, needs the `fm6_set_patch` path.
6. Section 3.7: a random-pattern ROLL (one percentage, the morph AMOUNT) needs a control on the PATTERNS page (all four knobs are used). Owner to choose; also whether the bass patterns should sit an octave lower in the step sequencer (they are C4-based so the SEQ arp reads them as offsets).
