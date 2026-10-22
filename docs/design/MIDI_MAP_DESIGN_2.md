# MIDI MAP, version 2: a complete proposed CC table that keeps the industry-standard names and follows the Access Virus' layout (design proposal, 2026-10-20)

Private. Research and writing only: no code changed, no branch made, no picture drawn (plain text tables). INDEX.md and the other documents were not touched. Every number, name and default below is a proposal for you to approve; nothing is decided except what is under "Decided by the owner".

Version 1 is MIDI_MAP_DESIGN.md (today's CC list, how MIDI LEARN works, the table design, the unit page). This version 2 replaces its **number choices** and adds the standard-name rule; the table design, the unit page and the editor tab of version 1 stay, and section 5 here lists only what changes.

Marks, as in the other studies: **CONFIRMED** (read today in a source or in the firmware), **est.** (worked out, not measured), **HYPOTHESIS** (my reading, not proven).
Sources read: the firmware in `felucca-base/firmware/src` (`midi_control.c`, `params.c`, `core.h`, `seq.c`; READ ONLY; EXTENSION_BLOCK_PLAN.md says the newest line is join0234, I read join0233 as told), your Virus file "Virus TI MIDI CC List - Layout.pdf" (one page, read as data), the Virus Editor manual (only the MIDI-page and learn passages), the MIDI list at midi.org and at cmtext.com, the Roland P-6 manual page, the Akai Force release notes 3.1.1 and forum reports, and the DUELL documents named in the ticket.

## Decided by the owner (his words, 2026-10-19 and 2026-10-20)

> "Everything can change today. Until alpha, everything can change. Global map. Five default."

His answers to questions 1, 2, 3 of MIDI_MAP_DESIGN.md: today's built-in numbers may all move; one global map; own CCs for the new values CUTOFF 2, RES 2, SUB LEVEL, CYCLE RISE, CYCLE FALL, with the numbers to be re-chosen as below.

> "The new we need new CCs, but I want the CCs to be somehow respecting industry standards, because there are a lot of these CC codes that already have a name to them. On other devices, like breath control and send and so on. So I would like to respect those industry standard namings for the CC code, so it's easier, because, for example, on the Akai Force the list is not super clear, and it's difficult to set it up if we don't use MIDI learn, which I don't like to use. I find it annoying. When I'm making the Q-Link assignments it's annoying to find what I want to control. I would like those [standard meanings] to hold whenever we want that to be controlled, so as long as we make an exception, I would like to make that call. And the other thing I would like us to take ideas from for the CC build-out is the Access Virus itself. Because they have a similar structure for the filters and stuff. And I think it would be just easy to follow that method."

The four rules I built from it: (1) a DUELL parameter that does what a standard CC is defined to do gets that number; (2) every deviation is an EXCEPTION, listed separately for you to approve or refuse (section 6); (3) a parameter with no standard CC follows the Virus' layout principle, in numbers the standard leaves undefined; (4) no dependence on MIDI learn: every important sound parameter can be reached by a DIRECT CC, learn stays only for the rest.

**One thing in the brief that pulls two ways, which I have not hidden:** your answer 3 says "five default" own CCs for the new values; rule 4 says every important sound parameter direct. Version 1 had only 29 clean numbers, so five was the sensible cap. With the new layout the 20 new values fit on clean numbers (section 4), so this document gives **all 20 a direct CC** and keeps the five as the ones that can never depend on an exception. If you want exactly five, question 1 says how. Nothing else in the ticket contradicted what I found; the small differences (CC24, the "21 reserved") are noted where they occur.

## Words used (the rest are in GLOSSARY.md)

- **MIDI CC (control change)**: a message "controller number N has value V", N and V both 0 to 127. A knob on a controller sends one.
- **Standard name**: the name the MIDI specification gives a CC number (7 is "Channel Volume", 74 is "Sound Controller 5, default Brightness"). Synths, DAWs and controllers use these names in their lists.
- **Undefined / General Purpose number**: a CC number the specification leaves empty (undefined), or marks "General Purpose Controller" (16 to 19 and 80 to 83: meant to be assigned to anything). These are the free numbers.
- **Fine half (LSB)**: numbers 32 to 63 are, in the specification, the second half of a "14-bit" pair for controllers 0 to 31 (one message for the coarse value, one for the fine part). The specification calls this rarely implemented. Almost every knob sends only the coarse 7-bit CC, so the fine halves are in practice free, but a controller set to 14-bit mode would clash.
- **Sound controller 1 to 10 (70 to 79), Effects 1 to 5 depth (91 to 95), Effect control 1 and 2 (12, 13)**: groups of numbers the specification names but gives only loose default meanings (74 brightness, 91 reverb send, ...).
- **Q-Link**: the Akai Force's assignable knobs (also on the MPC line).
- **SysEx (system exclusive)**: a long MIDI message for one maker's own data. The DUELL editor talks to the unit with it and can set any parameter this way.
- **NRPN (non-registered parameter number)**: the standard way to address more than 127 parameters with CC pairs (CC99 and CC98 pick the parameter, CC6 and CC38 carry the value). Not built, see section 3.3.
- **Own CC, learn-only, target, part rule, learn slot**: as in version 1.
- **Part, channel**: DUELL has two playable parts (four tracks inside, the last two hidden and without a MIDI channel). Each part has its own MIDI channel (default 1 and 2, GLO > MIDI). Section 4.1.

---

## 1. Verdict (half a page)

**The principle.** One global table, one line per CC number (version 1). The numbers are filled in three layers, in this order: **STANDARD** (a standard name that DUELL's parameter truly matches), **VIRUS-LIKE** (no standard: a free number, in the Virus' section order), **EXCEPTION** (anything that bends a standard meaning or uses a standard-defined number for something else). The last layer is yours to approve; if you refuse one, only its rows fall back to learn-only, nothing else moves.

**How many parameters get a direct CC (counted from the table in section 4).**
- **88 direct CCs** if you approve everything: 12 on a standard number that matches exactly, 8 on a soft-match standard number, 44 on free numbers, 24 on the fine-half block.
- **56 direct CCs** with no exception at all (the 12 exact standard ones and the 44 on free numbers: all 20 new values, the morph, the mode, the effects block).
- A further **3 numbers are inputs, not parameters** (1 modulation wheel, 11 expression, 64 sustain pedal).
- About **91 parameters stay learn-only**: matrix source and destination choices (16), the 8 matrix amounts, EQ (7), ARP and SEQ setup (15), DUCK (7), the lesser LFO and envelope settings, chords, mute, trim, the master compressor, width, bass mono, tune. Every one of them can still be set from the editor (SysEx reaches all parameters, as with the Virus' page B) and by learn.
- **Learn gets a small pool:** 19 numbers stay unused (3 of them clean: 3, 9, 119, which I keep for the planned later MIDI options). That is the price of shipping 88: in version 1 learn had 83 free numbers. You can free more any time by clearing a row in the table.

**What happens to what cannot have a direct CC.** Learn (as today, now also for global values and for the extension values); the editor (SysEx, all parameters); pages of the unit. I do not use page-switch tricks (the same CC meaning different things in different modes): it breaks "a control does only what its label says". NRPN could cover everything for controllers that send it, but I do not expect the Force to (HYPOTHESIS) and I leave it out of this map (section 3.3).

**The EXCEPTIONS that need your call** (details in section 6):
1. **X1** BALANCE on CC8 (global balance of the two parts, standard name "Balance").
2. **X2** DELAY send on CC92 ("Effects 2 Depth", no default meaning; Roland's own P-6 puts its delay level there). DETUNE then takes 94, the number its standard name says. Today's delay send on 94 moves.
3. **X3** LFO 1 as the vibrato: CC76 rate, CC77 depth (to pitch), CC78 delay (the fade-in).
4. **X4** The fine-half block: 24 numbers (34, 35, 36, 41 and 44 to 63) used as ordinary CCs, as the Virus itself does. This carries the engine values E1 to E8, ENV 2, the LFO 2 and others. The biggest decision; refuse it and 24 rows become learn-only.
5. **X5** INSERT as the "Effects" CCs: CC12 and CC13 (Effect Control 1 and 2) = INSERT A and B, CC95 (Effects 5 Depth) = INSERT MIX.
6. **X6** CC1 (modulation wheel) also moves the MORPH fader (today's behaviour, kept).
7. **X7** CC11 (expression) stays a matrix source and does not change the loudness by itself (today's behaviour, kept).
8. **X8** CC70 (Sound Variation) for the MORPH fader: I recommend NO.

Numbers that move against today: all morph and mode numbers (20 to 25, 111 to 118 become 102 to 113 and 8), the delay send (94 to 92). **Unchanged:** 5, 7, 10, 71, 72, 73, 74, 75, 91, 93.

---

## 2. The standard list: every standard CC used, its official name, DUELL's match

Source for the names: the MIDI 1.0 control change list as published by midi.org (CONFIRMED; the same numbers and names also in the Indiana University table at cmtext.com, which has two older wording differences, noted below). "Soft" means a match I judge fair but not exact: it needs your yes (section 6).

### 2.1 Numbers DUELL uses with their standard meaning

| CC | Official name | DUELL parameter | Match | Today |
| --- | --- | --- | --- | --- |
| 1 | Modulation Wheel or Lever | input: matrix source MODW; also moves the MORPH fader | exact (source); morph part is X6 | 1 |
| 5 | Portamento Time | GLIDE | exact | 5 |
| 7 | Channel Volume | LEVEL (the mixer level of the part) | exact | 7 |
| 8 | Balance | BALANCE (global balance between part 1 and part 2) | soft, X1 | 116 |
| 10 | Pan | PAN | exact | 10 |
| 11 | Expression Controller | input: matrix source EXPR | partial, X7 | 11 |
| 12 | Effect Control 1 | INSERT A (the part's insert effect, first value) | soft, X5 | none |
| 13 | Effect Control 2 | INSERT B | soft, X5 | none |
| 64 | Damper Pedal (Sustain) | sustain pedal (input) | exact | 64 |
| 71 | Sound Controller 2 (default Timbre / Harmonic Intensity) | FILTER 1 RESONANCE | exact (the common use) | 71 |
| 72 | Sound Controller 3 (default Release Time) | AMP RELEASE | exact | 72 |
| 73 | Sound Controller 4 (default Attack Time) | AMP ATTACK | exact | 73 |
| 74 | Sound Controller 5 (default Brightness) | FILTER 1 CUTOFF | exact | 74 |
| 75 | Sound Controller 6 (default Decay Time, RP-021) | AMP DECAY | exact | 75 |
| 76 | Sound Controller 7 (default Vibrato Rate, RP-021) | LFO 1 RATE | soft, X3 | none |
| 77 | Sound Controller 8 (default Vibrato Depth) | LFO 1 to PITCH depth | soft (exact in effect), X3 | none |
| 78 | Sound Controller 9 (default Vibrato Delay) | LFO 1 FADE (the fade-in before it is heard) | soft, X3 | none |
| 79 | Sound Controller 10 (no default meaning) | AMP SUSTAIN level | free standard slot, no exception | none |
| 91 | Effects 1 Depth (default Reverb Send Level, RP-023) | REVERB send | exact | 91 |
| 92 | Effects 2 Depth (formerly Tremolo; no default now) | DELAY send | soft, X2 (Roland P-6: 92 = Delay Level) | none |
| 93 | Effects 3 Depth (default Chorus Send Level, RP-023) | CHORUS send | exact | 93 |
| 94 | Effects 4 Depth (formerly Celeste / Detune Depth) | DETUNE (the voice detune) | exact by the old name | DELAY send |
| 95 | Effects 5 Depth (formerly Phaser Depth) | INSERT MIX (the insert's depth) | soft, X5 | none |

All rows CONFIRMED for the standard name; "exact / soft" is my judgement. Wording differences: the cmtext table writes "Breath Contoller" and gives 75 to 79 "no defaults"; midi.org (newer) gives 75 to 78 their RP-021 defaults and 79 none. I used midi.org.

Not used for DUELL's own parameters but handled by the firmware as the standard says: **0 and 32** bank select (for program change), **6 and 38** data entry (bend range), **96 to 101** data increment / decrement, NRPN / RPN select, **120 to 127** channel mode messages (All Sound Off 120, Reset All Controllers 121, All Notes Off 123; 122 and 124 to 127 not acted on but reserved). These are the **21 locked numbers** of version 1 (counting 1, 11 and 64, which DUELL uses as inputs; the truly locked ones are 18).

### 2.2 Standard numbers DUELL cannot use for their standard meaning (nothing matches): they stay unused

| CC | Official name | Why no match | Note |
| --- | --- | --- | --- |
| 2 | Breath Controller | DUELL has no breath source | Question 3 offers it as a matrix source, like CC11 |
| 4 | Foot Controller | no foot source | same |
| 65 | Portamento On/Off | GLIDE has no on/off (GLIDE 0 is off; GLIDE MODE is RATE / TIME) | |
| 66 | Sostenuto | not in DUELL | |
| 67 | Soft Pedal | not in DUELL | |
| 68 | Legato Footswitch | VOICE has LEG as one of four voice modes, not a switch | |
| 69 | Hold 2 | ARP HOLD latches; Hold 2 lengthens the release: different | |
| 70 | Sound Controller 1 (default Sound Variation) | the MORPH could be called "variation", but too loose; X8 | |
| 84 | Portamento Control | needs a note number, not a level | |
| 88 | High Resolution Velocity Prefix | a velocity extension, never a parameter | |
| 33, 37, 39, 40, 42, 43 | fine halves of 1, 5, 7, 8, 10, 11 | the pairs of numbers DUELL itself uses; a controller that sends 14-bit volume or pan would send these | kept clear on purpose |
| 3, 9 | Undefined | clean, kept in reserve | for the later MIDI options |
| 119 | Undefined | clean, kept in reserve | same |

That is the 19 numbers left over (2, 3, 4, 9, 33, 37, 39, 40, 42, 43, 65 to 70, 84, 88, 119).

---

## 3. The Virus layout and how DUELL follows it

### 3.1 The Virus' CC layout (read from your PDF; only the sections that matter; number, parameter)

The chart prints sections in this order: Oscillator 1, Oscillator 2, Oscillator Global, Filter 1, Filter 2, Filter Global, Filter Envelope, Amp Envelope, Voice / Pitch, Global Mixer, LFO Settings, LFO Routing, Effects Global, Chorus, Delay, Reverb, then Utility, Misc and Expression controllers.

| Section | CC: parameter |
| --- | --- |
| Oscillator 1 | 17 waveform, 18 pulsewidth, 19 wave select, 20 semitone, 21 key tracking |
| Oscillator 2 | 22 waveform, 23 pulsewidth, 24 wave select, 25 semitone, 26 detune, 27 FM amount, 28 sync, 29 filter envelope, 31 key tracking |
| Oscillator Global | 33 balance, 34 sub volume, 35 sub shape, 36 main volume, 37 noise volume, 38 ring mod volume, 39 noise colour |
| Filter 1 | 40 cutoff, 42 resonance, 44 envelope amount, 46 key tracking, 51 mode |
| Filter 2 | 41 cutoff, 43 resonance, 45 envelope amount, 47 key tracking, 52 mode |
| Filter Global | 48 balance, 49 saturation curve, 53 routing, 30 FM to filter envelope |
| Filter Envelope | 54 attack, 55 decay, 56 sustain, 57 sustain time, 58 release |
| Amp Envelope | 59 attack, 60 decay, 61 sustain, 62 sustain time, 63 release |
| Voice / Pitch | 5 portamento time, 93 transpose, 94 keyboard mode, 97 unison mode, 98 unison detune, 99 unison spread, 100 unison LFO phase |
| Global Mixer | 7 part volume, 8 part balance, 10 part pan, 91 patch volume, 101 input mode, 102 input select |
| LFO 1 | 67 rate, 68 shape, 69 envelope mode, 70 LFO mode, 71 symmetry, 72 key tracking, 73 key trigger |
| LFO 1 routing | 74 to osc 1, 75 to osc 2, 76 to pulsewidth, 77 to resonance, 78 to filter gain |
| LFO 2 | 79 rate, 80 shape, 81 envelope mode, 82 LFO mode, 83 symmetry, 84 key tracking, 85 key trigger |
| LFO 2 routing | 86 to oscillator shape, 87 to FM amount, 88 to filter 1 cutoff, 89 to filter 2 cutoff, 90 to pan |
| Effects Global | 112 delay / reverb mode, 113 effect send |
| Chorus | 105 mix, 106 rate, 107 depth, 108 delay, 109 feedback, 110 LFO shape |
| Delay | 114 time, 115 feedback, 116 rate, 117 depth, 118 LFO shape, 119 colour |
| Reverb | 116 decay time, 117 room size, 118 damping |
| Utility / expression | 0 and 32 bank select, 6 data slider, 122 keyboard on/off, 123 all notes off; 1 mod wheel, 2 breath, 4 foot, 11 expression, 64 hold pedal, 65 portamento pedal, 66 sostenuto; "controller N" with no name: 3, 9, 12 to 16, 50, 92, 95, 96, 103, 104, 111, 120, 121, 124 to 127 |

Four findings from the chart itself (CONFIRMED from the text; the reading of the third is HYPOTHESIS):
1. The Virus keeps the standard meaning only for **1, 2, 4, 5, 7, 8, 10, 11, 64, 65, 66** and bank / data / notes-off. It uses **70 to 78** for LFO settings, **91** for patch volume, **93 and 94** for transpose and keyboard mode, and **33 to 63** as ordinary parameters. So **copying its numbers would break the standard names you want kept** (its cutoff is 40, the standard cutoff is 74; its 91 is patch volume, the standard 91 is reverb send).
2. It has **no per-part scheme beyond the channel**: 16 parts, one map, a part is addressed by its MIDI channel. DUELL does the same (section 4.1).
3. Delay and reverb are **printed on the same numbers 116 to 118**, next to a "Delay / Reverb Mode" CC (112). The chart does not say so, but the likely reading is that those numbers change meaning with the mode. DUELL does not do this (a control does only what its label says).
4. For more than 127 values it uses **pages and SysEx**: the Virus Editor manual's setup steps say "MIDI Control LoPage = Controller Data, HiPage = SysEx" (CONFIRMED, Virus Editor manual, setup pages). So the CC set is the "playable" page and the rest of the parameters are reached by SysEx. How exactly the Virus' own pages A and B split is not in the file (HYPOTHESIS: the CC chart is page A).

### 3.2 What DUELL takes from the Virus (the principle, not the numbers)

| Virus principle | How DUELL follows it |
| --- | --- |
| Sections in a fixed order: oscillator, filters, envelopes, LFOs, effects, then utility | Free numbers are handed out in that order, low to high: sound source (SUB, engine values), filter 1, filter 2, filter global, ENV 2, LFOs, then the effects block, morph, mode, cycle |
| Filter 1 and filter 2 are twins with the same five controls in the same order: cutoff, resonance, envelope amount, key tracking, mode | Filter 1 keeps the standard CUTOFF 74 and RESONANCE 71 (a standard name beats adjacency); the rest of the filter 1 block (ENV AMT, KEY TRK, MODE) is followed by filter 2's in the same order: CUTOFF 2, RES 2, ENV AMT 2, KEY TRK 2, TYPE |
| "Filter Global": balance, saturation, routing | BLEND (= the Virus' Filter Balance), DRIVE (= saturation), ROUTE (= Filter Routing), then LINK (DUELL's own) |
| Filter envelope before amp envelope, ADSR order | ENV 2 (the filter envelope) A D S R in the fine-half block, in order; the amp envelope is on the standard 73 / 75 / 79 / 72 |
| An LFO block with its routing amounts listed per LFO (rate, shape ...; then "to pitch", "to cutoff", ...) | LFO 1 and LFO 2, each: rate, shape, then the amounts to pitch, cutoff, amp, shape |
| Effects block: chorus, delay, reverb in that order | the FX block 80 to 90 in the same order; the sends sit right after it at 91 to 93 |
| Utility / expression on the standard numbers | same |

### 3.3 Beyond 127 controllers: pages, SysEx, NRPN, learn

- **Virus way:** CC for the playable page, SysEx for the rest (3.1 finding 4). DUELL already has the second half: the editor protocol (SysEx) reaches every parameter id (CONFIRMED in version 1, `web/EDITOR_PROTOCOL.md`).
- **Not used: page switching by a CC** (CC X selects a page and then CC Y means different things). The unit's rule is that a control does only what its label says; a hidden page state breaks it.
- **NRPN:** it would reach every parameter and costs no CC numbers. I do not expect the Force's Q-Link to send it (HYPOTHESIS: I found no source that it can; the Track Edit passage below speaks only of "continuous controllers"). Left out of the map; est. 1 round to add later, using the same table (the NRPN number would be the parameter's id).
- **Learn:** stays for the rest, and in the new table it also reaches the global values and the extension values (version 1, verdict).

---

## 4. THE FULL PROPOSED MAP

### 4.1 Parts, channels, and the locked numbers

- **One global map, two parts.** The table has one line per CC number; a line says "channel's part" (P) or "global" (G). A P row moves the part that the CC's channel plays: part 1 on its channel (default 1), part 2 on its channel (default 2); in MIDI IN SELECTED any channel moves the selected part; in LAYER, part 1's channel (CONFIRMED, version 1 section 1.1). So the same numbers work for both parts; you pick the part by the channel your controller sends on. The two hidden tracks have no channel and no CC.
- A **G row** (the effects block, MODE, BALANCE, XFADE, SPLIT, TARGET, SMART, ARP, PITCH) acts on any channel, as CC112 does today.
- **Never used:** 0, 32, 6, 38, 96 to 101, 120 to 127 (18 numbers), and 1, 11, 64 only as the inputs listed in 2.1.
- **Kinds** in the notes: CONT (continuous), BIP (bipolar, centre 64), SW (switch, 64 and up = on), STEP n (n choices over the range), ACT (fires once at 64 or more, re-arms below 64).
- **Source** column: **STANDARD**, **VIRUS-LIKE**, **EXCEPTION** (with its X number) or **LEARN-ONLY** (section 4.8). "Today" is the number it has now (version 1), `-` if none.
- Parameter names are the unit's own; engine values E1 to E8 take the labels of the current engine.

### 4.2 Standard-number rows (sorted by CC)

| CC | Name | Part | Source | Today | Notes |
| --- | --- | --- | --- | --- | --- |
| 1 | MOD WHEEL (input) | P | STANDARD / X6 | 1 | matrix source MODW; also the MORPH fader |
| 5 | GLIDE | P | STANDARD | 5 | CONT |
| 7 | LEVEL | P | STANDARD | 7 | CONT; the mixer level, not the patch TRIM |
| 8 | BALANCE | G | EXCEPTION X1 | 116 | BIP; 64 = 0, -100 to +100 |
| 10 | PAN | P | STANDARD | 10 | BIP |
| 11 | EXPRESSION (input) | P | STANDARD / X7 | 11 | matrix source EXPR |
| 12 | INSERT A | P | EXCEPTION X5 | - | CONT; the insert type's first value |
| 13 | INSERT B | P | EXCEPTION X5 | - | CONT; second value |
| 64 | SUSTAIN (input) | P | STANDARD | 64 | pedal |
| 71 | RESONANCE (filter 1) | P | STANDARD | 71 | CONT; until the engine filters leave (TWIN plan) it is the engine's own RES on ANALOG, TRIO, NOISE |
| 72 | AMP RELEASE | P | STANDARD | 72 | CONT |
| 73 | AMP ATTACK | P | STANDARD | 73 | CONT |
| 74 | CUTOFF (filter 1) | P | STANDARD | 74 | CONT; same remark as 71 |
| 75 | AMP DECAY | P | STANDARD | 75 | CONT |
| 76 | LFO 1 RATE | P | EXCEPTION X3 | - | CONT |
| 77 | LFO 1 to PITCH | P | EXCEPTION X3 | - | BIP |
| 78 | LFO 1 FADE | P | EXCEPTION X3 | - | CONT |
| 79 | AMP SUSTAIN | P | STANDARD | - | CONT; Sound Controller 10 has no default |
| 91 | REVERB send | P | STANDARD | 91 | CONT |
| 92 | DELAY send | P | EXCEPTION X2 | 94 | CONT |
| 93 | CHORUS send | P | STANDARD | 93 | CONT |
| 94 | DETUNE | P | STANDARD | - | CONT; the voice detune of a UNISON / POLY sound |
| 95 | INSERT MIX | P | EXCEPTION X5 | - | CONT |

### 4.3 The clean block 14 to 31: sound source, filter 1, filter 2, filter global (VIRUS-LIKE)

All P, all on numbers the standard leaves undefined (14, 15, 20 to 31) or "General Purpose 1 to 4" (16 to 19). None needs an exception. These carry the owner's five and the other fifteen new values.

| CC | Name | Section | Source | Today | Notes |
| --- | --- | --- | --- | --- | --- |
| 14 | SUB LEVEL | SUB | VIRUS-LIKE (Osc Global: sub volume) | - | CONT; 0 = not run; owner's five. extension slot S6 |
| 15 | SUB OCTAVE | SUB | VIRUS-LIKE | - | STEP 3 (-2, -1, 0) |
| 16 | SUB WAVE | SUB | VIRUS-LIKE (sub shape) | - | STEP 4 (SINE, TRI, SQUARE, PULSE) |
| 17 | SUB HARMONICS | SUB | VIRUS-LIKE | - | CONT |
| 18 | F1 ENV AMT | FILTER 1 | VIRUS-LIKE (Filter 1: env amount) | - | BIP; ENV 2 to cutoff |
| 19 | F1 KEY TRK | FILTER 1 | VIRUS-LIKE (key tracking) | - | BIP |
| 20 | F1 MODE | FILTER 1 | VIRUS-LIKE (filter mode) | - | CONT; LP at 0, BP at 64, HP at 127 |
| 21 | CUTOFF 2 | FILTER 2 | VIRUS-LIKE (Filter 2: cutoff) | - | CONT; owner's five. Spans offset (LINK on) or absolute (LINK off), whichever is active. Id 106 |
| 22 | RES 2 | FILTER 2 | VIRUS-LIKE (resonance) | - | CONT; owner's five. Feedback on COMB. Id 25 |
| 23 | F2 ENV AMT | FILTER 2 | VIRUS-LIKE | - | BIP. extension slot S3 |
| 24 | F2 KEY TRK | FILTER 2 | VIRUS-LIKE | - | BIP. slot S2 |
| 25 | F2 TYPE | FILTER 2 | VIRUS-LIKE (filter mode) | - | STEP 7 (OFF, LP, BP, HP, NOTCH, LADDER, COMB). A sweep flips the type at the middle of a step, audibly: use it as a switch. Id 42 |
| 26 | DRIVE | FILTER GLOBAL | VIRUS-LIKE (saturation) | - | CONT; 0 = not run; INERT NOW while F2 is OFF. Id 26 |
| 27 | BLEND | FILTER GLOBAL | VIRUS-LIKE (= Filter Balance) | - | BIP; INERT NOW in SERIES. Id 113 |
| 28 | ROUTE | FILTER GLOBAL | VIRUS-LIKE (Filter Routing) | - | SW (SERIES / PARALLEL). slot S0 |
| 29 | LINK | FILTER 2 | VIRUS-LIKE (DUELL's own) | - | SW; converts CUTOFF 2 between its two meanings, not for a CC mid-note. slot S1 |
| 30 | LFO 1 to CUTOFF 2 | FILTER 2 | VIRUS-LIKE (LFO routing) | - | BIP. slot S4 |
| 31 | LFO 2 to CUTOFF 2 | FILTER 2 | VIRUS-LIKE (LFO routing) | - | BIP. slot S5 |

(S0 to S14 are the extension-block slots X0 to X14 of EXTENSION_BLOCK_PLAN.md; I write S so they are not mistaken for the exceptions X1 to X8 of section 6.)

### 4.4 The fine-half block (EXCEPTION X4): sound source, filter 1, ENV 2, LFOs

All P. Numbers 34, 35, 36, 41, 44 to 63. I left out 33, 37, 39, 40, 42, 43 on purpose (2.2). The rule for the 24 I chose: their coarse partner (the number minus 32) is not a controller DUELL or a normal sender uses as 14-bit: 2, 3, 4, 9, 12, 13 are unused or only soft-used, and 14 to 31 are DUELL's own.

| CC | Name | Section | Source | Today | Notes |
| --- | --- | --- | --- | --- | --- |
| 34 | SPREAD | VOICE | EXCEPTION X4 | - | CONT; stereo spread of the voices |
| 35 | SATURATION | FX | EXCEPTION X4 | - | CONT (the part's SAT) |
| 36 | INSERT C | FX | EXCEPTION X4 | - | CONT; the type's third value |
| 41 | F1 SLOPE | FILTER 1 | EXCEPTION X4 | - | STEP 2 (12, 24 dB) |
| 44 | E1 | ENGINE | EXCEPTION X4 | - | the engine's first edit value; meaning changes with the engine |
| 45 | E2 | ENGINE | EXCEPTION X4 | - | |
| 46 | E3 | ENGINE | EXCEPTION X4 | - | |
| 47 | E4 | ENGINE | EXCEPTION X4 | - | |
| 48 | E5 | ENGINE | EXCEPTION X4 | - | |
| 49 | E6 | ENGINE | EXCEPTION X4 | - | |
| 50 | E7 | ENGINE | EXCEPTION X4 | - | on the WAVE engine this is WAVE STEPS (OFF, 2 to 19, STEP 19) |
| 51 | E8 | ENGINE | EXCEPTION X4 | - | |
| 52 | ENV 2 ATTACK | ENV 2 | EXCEPTION X4 | - | CONT |
| 53 | ENV 2 DECAY | ENV 2 | EXCEPTION X4 | - | CONT |
| 54 | ENV 2 SUSTAIN | ENV 2 | EXCEPTION X4 | - | CONT |
| 55 | ENV 2 RELEASE | ENV 2 | EXCEPTION X4 | - | CONT |
| 56 | LFO 1 SHAPE | LFO 1 | EXCEPTION X4 | - | STEP 5 (SIN, TRI, SAW, SQR, S&H) |
| 57 | LFO 1 to CUTOFF | LFO 1 | EXCEPTION X4 | - | BIP |
| 58 | LFO 1 to AMP | LFO 1 | EXCEPTION X4 | - | CONT; the tremolo (the old name of 92) |
| 59 | LFO 2 RATE | LFO 2 | EXCEPTION X4 | - | CONT |
| 60 | LFO 2 SHAPE | LFO 2 | EXCEPTION X4 | - | STEP 5 |
| 61 | LFO 2 to CUTOFF | LFO 2 | EXCEPTION X4 | - | BIP |
| 62 | LFO 2 to PITCH | LFO 2 | EXCEPTION X4 | - | BIP |
| 63 | LFO 2 to SHAPE | LFO 2 | EXCEPTION X4 | - | BIP |

**Engine values, how they behave.** E1 to E8 always mean "the current engine's value n"; the CC scales 0 to 127 over that value's own range, and a stepped value (WAVE on ANALOG, STEPS on WAVE) takes steps across the range. The editor table shows the current engine's name for each (version 1 section 2.4). A CC to an engine value that the current engine does not have is shown as INERT NOW, not hidden. After TWIN lands, CC74 and CC71 are always filter 1 and the "smart per engine" rule of today (`MIDI_CC_ENG`) goes away.

**Editable B.** No CC: it is a way to edit End B, not a value. As before (EDITABLE_B_DESIGN.md, via version 1) a CC always writes End A. The existing morph CCs below already serve B (fader, ROLL B, UNDO).

### 4.5 The effects block: global (VIRUS-LIKE)

All G, any channel. Numbers 80 to 83, 85 to 87, 89 and 90 (84 and 88 skipped: 2.2). Today none of these has a CC or can be learned.

| CC | Name | Source | Notes |
| --- | --- | --- | --- |
| 80 | CHORUS RATE | VIRUS-LIKE (Chorus) | CONT |
| 81 | CHORUS DEPTH | VIRUS-LIKE | CONT |
| 82 | DELAY TIME | VIRUS-LIKE (Delay) | STEP (note values); tempo-based, as the knob |
| 83 | DELAY FEEDBACK | VIRUS-LIKE | CONT |
| 85 | DELAY COLOUR | VIRUS-LIKE | CONT |
| 86 | DELAY MIX | VIRUS-LIKE | CONT |
| 87 | REVERB SIZE | VIRUS-LIKE (Reverb) | CONT |
| 89 | REVERB DAMP | VIRUS-LIKE | CONT |
| 90 | REVERB LOW CUT | VIRUS-LIKE | CONT, 20 Hz to 2 kHz |

The sends (91 reverb, 92 delay, 93 chorus) follow in 4.2, so a Q-Link row reads: chorus, delay, reverb parameters, then the three sends.

### 4.6 Morph, mode, cycle (VIRUS-LIKE; DUELL's own features, no standard, no Virus equivalent)

Moved together into one run each. Today's numbers shown, so you can see the moves.

| CC | Name | Part | Source | Today | Notes |
| --- | --- | --- | --- | --- | --- |
| 102 | MORPH FADER | P | VIRUS-LIKE | 20 | CONT; CC1 moves it too (X6) |
| 103 | MORPH ROLL A | P | VIRUS-LIKE | 21 | ACT |
| 104 | MORPH ROLL B | P | VIRUS-LIKE | 22 | ACT |
| 105 | MORPH UNDO | P | VIRUS-LIKE | 23 | ACT; 1 to 63 End A, 64 to 127 End B, 0 re-arms |
| 106 | MORPH AMOUNT | P | VIRUS-LIKE | 25 | CONT, 0 to 100 % |
| 107 | MORPH TARGET | G | VIRUS-LIKE | 111 | STEP 3 (PART 1, PART 2, BOTH) |
| 108 | MORPH SMART | G | VIRUS-LIKE | 113 | SW |
| 109 | MORPH ARP | G | VIRUS-LIKE | 114 | SW |
| 110 | MORPH PITCH | G | VIRUS-LIKE | 115 | SW |
| 111 | MODE | G | VIRUS-LIKE | 112 | STEP 3 (SPLIT, STEREO, LAYER) |
| 112 | XFADE | G | VIRUS-LIKE | 117 | CONT, 0 to 100 % |
| 113 | SPLIT NOTE | G | VIRUS-LIKE | 118 | 0 = OFF, 1 to 127 = the note |
| 114 | CYCLE MODE | P | VIRUS-LIKE | - | SW (LOOP / ONCE). slot S10 |
| 115 | CYCLE RISE | P | VIRUS-LIKE | - | CONT; owner's five; note values when SYNC is on. slot S11 |
| 116 | CYCLE FALL | P | VIRUS-LIKE | - | CONT; owner's five. slot S12 |
| 117 | CYCLE CURVE | P | VIRUS-LIKE | - | BIP. slot S13 |
| 118 | CYCLE SYNC | P | VIRUS-LIKE | - | SW. slot S14 |

Notes: CC24 (empty today) and CC116 (BALANCE today) become free or reused. The morph's fire-once logic must use the row, not the number (`cc - 21` today): version 1 risk 1. The unbuilt plan in SPEC.md 4.4 (page-knob CCs 26 to 41, matrix amounts 102 to 109, arp 110) is superseded by this table; SPEC.md still lists them and would need correcting when you decide (I did not touch it).

### 4.7 Reserved for later, and the pool left for learn

- **Reserved by me:** 3, 9, 119 (clean). Purpose: the planned later MIDI options (SEQ / ARP / external START-STOP switches, AUTO clock; memory "Later MIDI options").
- **Free for learn / your own use:** 2, 4, 65 to 70, 84, 88, 33, 37, 39, 40, 42, 43 (16 numbers, all with a standard meaning DUELL does not use, so a learn onto them shows the standard name as a warning) plus 3, 9, 119 if you release them.
- Every number you clear in the map joins this pool.

### 4.8 Learn-only (no direct CC in this proposal)

Counted per part unless marked G. 76 per-part rows and 15 global rows = 91. Why each group: a choice you set once, a setup, or a value of which there are no spare numbers.

| Group | Rows | Why |
| --- | --- | --- |
| MIXER (2) | MUTE, TRIM (patch level) | MUTE is a switch you might want live; if so, release a reserved number (question 2) |
| VOICE (5) | VOICE mode, GLIDE MODE, PRIORITY, CHORD, VOICING | setup |
| ENV 1 / ENV 2 destinations (5) | ENV 1 to CUTOFF, PITCH, SHAPE; ENV 2 to SHAPE; ENV SYNC | the classic arrangement; the matrix does the same |
| LFO 1 (5) | PHASE, SYNC, TRIGGER, POLARITY, to SHAPE | settings of the LFO, not played |
| LFO 2 (5) | FADE, PHASE, SYNC, TRIGGER, POLARITY | same |
| MATRIX (24) | 8 x SOURCE, DESTINATION, AMOUNT (24) | the amounts are the ones you may want live (question 2); source and destination are choices |
| EQ (7) | LOW, LOW HZ, MID, MID HZ, MID Q, HIGH, HIGH HZ (7) | out of numbers; the three gains are the first to get one if numbers appear |
| INSERT (1) | TYPE (A, B, C and MIX have CCs) | TYPE is a choice |
| DUCK (7) | CURVE, DEPTH, ATTACK, RELEASE, SOURCE, RATE, THRESHOLD (7) | setup |
| ARP (9) | MODE, RATE, OCTAVES, GATE, SWING, PROBABILITY, HOLD, ORDER, TARGET (9) | performance settings; first in line for numbers (question 2) |
| SEQ and key (6) | LENGTH, DIVISION, SWING, GATE, QUANTIZE, TRANSPOSE (6) | setup |
| GLOBAL (15, G) | SWING, TUNE, DELAY MODE, DELAY MOD RATE, DELAY MOD DEPTH, PING-PONG SPREAD, REVERB TYPE, SHIMMER PITCH, SHIMMER MOD, COMPRESSOR THRESHOLD, RATIO, ATTACK, RELEASE, WIDTH, BASS MONO | set once; TEMPO and the clock source get no CC at all, they follow MIDI clock |

(The exact counts depend on the ids at the time of building; est. 91 plus or minus 4. The count that matters is the direct one, which I counted from the tables above.)

### 4.9 Check of the numbers (counted)

| | Count |
| --- | --- |
| Direct parameter rows | 88 = 12 standard-exact (5, 7, 10, 71 to 75, 79, 91, 93, 94) + 8 soft (8, 12, 13, 76, 77, 78, 92, 95) + 18 in 14 to 31 + 9 effects + 17 morph, mode, cycle + 24 fine-half |
| Inputs on standard numbers | 3 (1, 11, 64) |
| Locked | 18 |
| Unused | 19 |
| Sum | 88 + 3 + 18 + 19 = 128 |
| Direct without any exception | 56 |
| Direct without X4 | 64 |

---

## 5. How the unit and the editor present it

Same table and pages as version 1 (sections 2.2 to 2.5). Only the changes:

### 5.1 The unit: MIDI MAP page (proposal; layout for you to approve before building)

- **Groups on K1** now follow the Virus-like order of this document: MORPH, MIXER and SENDS, VOICE, SUB, FILTER 1, FILTER 2, FILTER GLOBAL, ENGINE, ENV, LFO 1, LFO 2, CYCLE, FX (insert), EFFECTS (global), MODE, and ASSIGNED. 15 groups of 4 to 24 rows.
- **A view EXCEPTIONS** (one more option on K1's first position, next to BY PARAMETER, BY CC, FREE CCS): lists only the rows tagged X1 to X8 with their X number. Footer: `CC92 DELAY SEND  X2  STD: EFFECTS 2 DEPTH`.
- **Standard name in the footer** for the 61 numbers that have a standard meaning (the 21 locked plus the named ones like 71, 74, 91): `CC74 CUTOFF  STANDARD  STD: BRIGHTNESS`. For an undefined number the footer says `STD: UNDEFINED` or `GENERAL PURPOSE`.
- **K3** as before. A number with a standard meaning that DUELL does not use (the 16 in 4.7) shows its standard name when you land on it, with the same question as a taken number: `CC66 IS SOSTENUTO. TAKE IT? OCT+ YES OCT- NO`.
- **RESET MAP** = exactly this proposed map (the shipped image); a confirm as in version 1.
- **The exceptions are not hidden on the unit**: refusing X4 (for example) is done in the editor table by clearing the 24 rows with one click (a filter + "clear shown"), not by a separate setting; the map is what it shows.

### 5.2 The editor: MIDI MAP table

Version 1 columns, plus two: **STANDARD NAME** and **SOURCE**.

| Column | Content |
| --- | --- |
| Section | the Virus-like group |
| Parameter | the unit's name; engine values show the current engine's label |
| CC | editable number or NONE |
| STANDARD NAME | the MIDI name of that number (all 128 are known to the editor, also for numbers with no DUELL row): "Sound Controller 5 (Brightness)" |
| SOURCE | STANDARD, VIRUS-LIKE, EXCEPTION X1 to X8, LEARN-ONLY, YOURS, LEARNED |
| Part, Kind, In, Saved | as version 1 |

- **Filters:** "show only exceptions" (the rows with an X), "only standard", "only learn-only", "only changed from the shipped map", "conflicts" (always empty by construction), by section. Sort by CC or by section.
- **A second table, BY NUMBER:** 0 to 127, each with its standard name and what DUELL does with it (parameter, input, locked, unused). This is the answer to "what is CC 66 here?".
- **Export / import** `midi-map.json` as in version 1. **New: `duell-cc-list.csv`** (number; standard name; DUELL parameter; section; part; kind; source) and a printable text list, both made from the map as it stands (so they stay true when you change a row). They are for reading and for typing into the Force; they are not a file the Force reads (5.3).
- RESET MAP as in version 1.

### 5.3 The Akai Force angle (what I found)

- **CONFIRMED (Akai Force 3.1.1 release notes):** "From MIDI Track > Track Edit you can now edit the continuous controller and string assigned to each control. Your custom parameter names/assignments can be saved and recalled with a track." and "Assign the continuous controllers in the order you want and label them according to the synth parameter they control." So on the Force you type a CC number and a label per control and save it as a track you can load in any project. That is the place your DUELL list belongs.
- **Forum reports (HYPOTHESIS: user posts, I could not open most threads, two returned 403):** the Force's MIDI-mode Q-Links come preset to pitch bend, modulation, breath, volume, pan, expression and so on (named standard controllers) and that preset could not be changed on firmware 3.2.3; the Track Edit screen lists all 128 CCs but there are only 16 controllable parameters, and the 128-line list cannot assign to the knobs. So: **the Force knows some standard names, it has 16 Q-Link slots per track, and the CC and the label are typed by you.** Whether its 128-line list shows a standard name for every number is **HYPOTHESIS: unclear**.
- **No Force-readable file exists that I could confirm** (HYPOTHESIS): I found no published track-file format. So the export is a **sheet to type from**, built to match the Force: pages of 16.
- **The Force sheet, proposed (a page = 16 Q-Link slots; the number, then the label ≤ 8 characters):**

| Page | Slots (CC label) |
| --- | --- |
| 1 PLAY | 74 CUTOFF, 71 RES, 21 CUT 2, 22 RES 2, 73 ATTACK, 75 DECAY, 79 SUSTAIN, 72 RELEASE, 14 SUB, 102 MORPH, 5 GLIDE, 94 DETUNE, 91 REVERB, 92 DELAY, 93 CHORUS, 7 LEVEL |
| 2 FILTER | 18 F1 ENV, 19 F1 KEY, 20 F1 MODE, 41 F1 SLOPE, 23 F2 ENV, 24 F2 KEY, 25 F2 TYPE, 26 DRIVE, 27 BLEND, 28 ROUTE, 29 LINK, 30 LFO1>C2, 31 LFO2>C2, 12 INS A, 13 INS B, 95 INS MIX |
| 3 ENGINE | 44 to 51 E1 to E8, 52 to 55 ENV2 A D S R, 15 SUB OCT, 16 SUB WAV, 17 SUB HARM, 34 SPREAD |
| 4 MOD | 76 LFO1 RAT, 56 LFO1 SHP, 77 LFO1 PIT, 57 LFO1 CUT, 58 LFO1 AMP, 78 LFO1 FAD, 59 LFO2 RAT, 60 LFO2 SHP, 61 LFO2 CUT, 62 LFO2 PIT, 63 LFO2 SHP, 114 to 118 CYCLE (MODE RISE FALL CURVE SYNC) |
| 5 GLOBAL | 80 CHO RAT, 81 CHO DEP, 82 DLY TIME, 83 DLY FBK, 85 DLY COL, 86 DLY MIX, 87 REV SIZE, 89 REV DAMP, 90 REV LOCT, 103 ROLL A, 104 ROLL B, 105 UNDO, 106 MORPH AM, 111 MODE, 8 BALANCE, 112 XFADE |

Not on any page: PAN 10, SATURATION 35, INSERT C 36, the morph settings 107 to 110, SPLIT NOTE 113. Page 1 is the one for playing; pages 2 to 5 follow the Virus-like section order. If you refuse X4, pages 3 and 4 shrink (the fine-half rows leave them).

---

## 6. EXCEPTION list: for you to approve or refuse, one by one

A standard-defined number used for something the standard does not say, or a standard meaning only partly kept. Each with the conflict, the options, my recommendation. If you refuse, the effect on the map is given.

**X1. BALANCE on CC8.**
- Standard: CC8 "Balance" is a left / right balance of one channel's stereo signal. DUELL's BALANCE sets the level balance between part 1 and part 2 and is global (any channel). The Virus puts "Part Balance" on 8 too.
- Conflict: a DAW that sends CC8 on a part's channel expecting stereo balance would move the global part balance instead.
- Options: (a) CC8 (recommended: the name is the same, and the Virus does it); (b) keep BALANCE off CC8 and put it on a free number, losing the name match.
- If refused: BALANCE goes to a reserved number (3 or 9); the mode group becomes MODE, BALANCE, XFADE, SPLIT.

**X2. DELAY send on CC92 (and DETUNE on 94).**
- Standard: 92 is "Effects 2 Depth" (formerly Tremolo; no default meaning today); 94 is "Effects 4 Depth" (formerly Celeste / Detune). Today's DELAY send is on 94.
- Evidence: Roland's own P-6 manual page lists CC91 Reverb Level and CC92 Delay Level (CONFIRMED). Yamaha charts use 94 for a "Variation send" (HYPOTHESIS: seen only in a snippet).
- Options: (a) DELAY 92, DETUNE 94 (recommended: both names fit, and the three sends sit in a row 91, 92, 93); (b) keep DELAY on 94 and move DETUNE to a free number (DETUNE then has no standard number; DELAY on 94 bends "detune").
- If refused: option (b).

**X3. LFO 1 as the vibrato: 76 rate, 77 depth, 78 delay.**
- Standard: sound controllers 7, 8, 9 (RP-021 defaults vibrato rate, depth, delay). DUELL's LFO 1 is a general LFO whose rate also serves its other destinations; "to PITCH" is exactly vibrato depth; "FADE" is the fade-in, which is a vibrato delay in effect.
- Conflict: CC76 changes the speed of everything LFO 1 drives, not only a vibrato.
- Options: (a) all three (recommended); (b) 77 only (exact); (c) none, then they join the free pool and LFO 1 takes numbers from the fine-half block.
- If refused: those rows move into free numbers of X4 or become learn-only.

**X4. The fine-half block, 24 numbers (34, 35, 36, 41, 44 to 63).**
- Standard: 32 to 63 are the fine halves of controllers 0 to 31. The Virus uses these numbers as ordinary parameters (its filter, envelopes and oscillator volumes are all there).
- Conflict: only for a controller that sends a 14-bit pair on one of the partner numbers (2, 3, 4, 9, 12, 13, 14 to 31): it would move a DUELL parameter on the fine number as well. The six dangerous partners (volume, pan, expression, balance, modulation, portamento) are NOT used. The Force knobs send plain 7-bit CCs (HYPOTHESIS; the release note speaks of "continuous controllers").
- Options: (a) approve all 24 (recommended: it is what gives E1 to E8, ENV 2 and LFO 2 a direct CC); (b) approve part: the engine values and ENV 2 only (44 to 55, 12 rows); SPREAD, SATURATION, INSERT C, F1 SLOPE and the LFO rows go learn-only; (c) refuse: all 24 rows become learn-only.
- If refused: the 24 rows in 4.4 become learn-only; pages 3 and 4 of the Force sheet shrink; nothing else moves.

**X5. INSERT as the "Effects" CCs: 12, 13 = INSERT A, B; 95 = INSERT MIX.**
- Standard: 12 and 13 "Effect Control 1 and 2" are generic controls for the effect of a channel; 95 "Effects 5 Depth" is the depth of an effect (formerly phaser).
- Conflict: the INSERT is a slot with 10 types; "phaser" is only one of them (PHASR). The values A, B change meaning with the type (DRIVE TONE LEVEL, CRUSH BITS RATE, ...).
- Options: (a) yes (recommended: the INSERT is the part's effect, and these three numbers have no other DUELL candidate); (b) no, A, B, C, MIX all in the fine-half block (4 numbers more from X4: there is no room, so they would be learn-only).
- If refused: INSERT A, B, MIX become learn-only.

**X6. CC1 (modulation wheel) also moves the MORPH fader.**
- Standard: "Modulation Wheel or Lever" is a general modulation amount. Today's behaviour: CC1 is the matrix source MODW and also the morph fader.
- Options: (a) keep (recommended: today, and the morph fader is a modulation amount); (b) CC1 only as the matrix source.
- If refused: the fader answers only on 102.

**X7. CC11 (expression) is a matrix source and does not change the loudness by itself.**
- Standard (GM2): expression scales the channel's volume. DUELL: a source you route yourself (to AMP, for example).
- Options: (a) keep (recommended: your matrix design; nothing is loud-changed behind your back); (b) wire CC11 to the part's loudness.
- If refused: option (b), a small change in the audio path (not designed here).

**X8. CC70 (Sound Variation) for the MORPH fader.**
- Standard: Sound Controller 1, "default Sound Variation", no stricter definition. The morph blends sound A into sound B, which is a variation of a sound.
- Options: (a) no (recommended: too loose a name, and CC1 already moves the fader); (b) yes, the fader on 70 and 102 is freed.
- If approved: 70 leaves the unused list (18 left), 102 becomes free.

---

## 7. Cost deltas against the first version

| Item | Version 1 | Version 2 | Delta |
| --- | --- | --- | --- |
| Stored (settings record) | 258 B (128 lines x 2 B + 2) | 258 B | 0. The table is the same; only its shipped content grows from 24 to 88 lines. The exact-size rule of the settings record: same single format round as the extension plan's reserved tail |
| Shipped image in flash | about 60 B (24 lines) | 256 B (the whole default table, for RESET MAP) | +200 B, est. |
| Standard names on the unit | none | names of the 61 numbers that have a standard meaning, 5 to 12 characters each | about +0.8 to 1 KB flash, est. |
| Standard names in the editor | none | all 128 names, a constant list | about 3 KB of editor code, no firmware cost |
| RAM | about 260 B | about 260 B | 0 |
| Hot RAM | 0 | 0 | 0 |
| Code (firmware) | 2 to 3 KB | 3 to 4 KB | +1 KB: more target kinds (global effects, engine values scaled by the engine's own range, stepped values), the exceptions view |
| CPU | one table read per CC | same | 0 |
| Rounds | 5 to 7 | 7 to 9 | +2: (1) building the shipped image and a host test that walks all 88 lines (every number unique, every target exists, every standard-numbered row equals section 2.1 of this document, every row in range), (2) editor: STANDARD NAME column, BY NUMBER table, exceptions filter, CSV and printable list |
| Cheap first step | editor-only read-only table of today's CCs, 1 round | the same, and it can also show the proposed map beside it (read-only) | 0 to +0.5 round |

New risks added by v2:
1. **Drift between this document and the code**: 88 shipped lines are 88 places to be wrong; the host test above is the guard.
2. **Fewer free numbers for later features** (19 left, 3 clean). Future features must either take reserved numbers or ride on learn. I reserve 3, 9, 119 for the planned later MIDI options.
3. **Moving the morph and mode numbers** breaks any controller template you already made for them (20 to 25, 111 to 118). Your answer 1 allows it.
4. **A refused exception leaves holes** in the groups; the table keeps the numbers of the other rows (no renumbering), so the holes stay visible. Better than shuffling.
5. Version 1's risks 1 to 8 still hold.

---

## 8. Questions for you (at most five, musical / UX, each with my recommendation)

1. **All 20 new values direct, or only your five?** Your answer 3 said five default; this map gives all 20 a direct CC (14 to 31 and 114 to 118) because there are enough clean numbers. *Recommend: all 20; the five are already the ones that never depend on an exception. If you want exactly five, I free 15 numbers (they join the learn pool).*
2. **When numbers run short, what comes first?** Nothing more fits. If you want direct CCs for the three EQ gains, the four matrix amounts or ARP HOLD / RATE, they must take (a) the three reserved numbers 3, 9, 119, or (b) numbers freed by making the morph's four setting switches (TARGET, SMART, ARP, PITCH) learn-only. *Recommend: neither; keep the morph switches (today's behaviour), keep 3, 9, 119 reserved for the later MIDI options, and give EQ and ARP learn.*
3. **Breath (CC2) and Foot (CC4) as new matrix sources**, next to MODW and EXPR? They would give those two standard names a job, so a breath controller or a foot pedal could move cutoff or anything in the matrix. Small (the same code as CC11). *Recommend: yes, later, as its own small feature; this map keeps 2 and 4 unused until then.*
4. **Still open from version 1: a taken number and CC out.** Typing or learning a number that already moves something: ask first (default no), as I proposed. And CC out (knob moves sent as CCs): the spec promises it, the firmware does not do it. *Recommend: ask first; CC out later as its own feature, keep the spare bit.*
5. **The Force sheet in the editor**: the five pages of 16 above as an export (printable and CSV), ordered so Q-Link page 1 is the playing page. *Recommend: yes; change the page contents when you have tried it on the Force.*

---

## Evidence list

| Claim | Source |
| --- | --- |
| Standard CC names, numbers 0 to 127, the 70 to 79 and 91 to 95 meanings and RP-021 / RP-023 defaults | midi.org "MIDI 1.0 Control Change Messages" (https://midi.org/midi-1-0-control-change-messages, fetched today); cross-check https://www.cmtext.com/MIDI/chapter3_controller_change2.php; the renaming of CC91 / CC93: https://midi.org/renaming-of-cc91-and-cc93 (title seen in a search, not opened) |
| Roland P-6: CC91 Reverb Level, CC92 Delay Level, CC71 to 74 resonance, release, attack, cutoff | https://static.roland.com/manuals/p-6/en-US/144786699145437451.html (fetched today) |
| Yamaha: CC94 "Variation Send Level" | search snippet of the PSR-S650 data list (not opened): HYPOTHESIS |
| The Virus CC chart | the owner's copy of the Virus TI MIDI CC List (Layout) PDF (one page, text extracted today) |
| Virus: CC page and SysEx page setup | Access Virus Editor manual, setup steps ("MIDI Control LoPage = Controller Data, HiPage = SysEx"; for the later model "Page A = Controller Data, Page B = SysEx") |
| Force 3.1.1: edit the CC and string of each control in MIDI Track > Track Edit, saved with the track | https://cdn.inmusicbrands.com/akai/Force/eO8S4/Force3.1.1FirmwareUpdate.pdf (text extracted today) |
| Force: preset Q-Link names, 16 slots, 128-line list not assignable to knobs | mpc-forums and gearspace threads, as reported in search results; two thread pages returned 403 (HYPOTHESIS) |
| Firmware: parts' channels, ROUT parts / selected, CC dispatch, the 11 sound CCs, `MIDI_CC_ENG`, globals and their labels | `midi_control.c`, `seq.c` (`midi_pch`), `params.c`, `core.h` (NPART 4, G ids) in `felucca-base/firmware/src` |
| Engine values E1 to E8 and their labels per engine | the `.edit` tables in `eng_*.c` |
| The 20 new values, their slots X0 to X14, the ids 25, 26, 42, 106, 113, WAVE STEPS on engine slot E7 | EXTENSION_BLOCK_PLAN.md section 2A; TWIN_FILTER_DESIGN_2.md; FILTER_PAGES_PROPOSAL.md; SUB_AND_CYCLE_DESIGN.md; FEATURE_LIST.md |
| Today's CC list, learn, the 45 taken numbers, the table and unit page design | MIDI_MAP_DESIGN.md |
| Counts in 4.9 | worked out by me from the tables of section 4 |
| UI rules used: propose and ask; a control does only what its label says; hide inert controls; no sticky knobs; scope marked | memory files feedback-ask-before-ui-changes, feedback-ui-label-law, feedback-hide-inert-controls, feedback-no-sticky-knobs, feedback-ui-clarity-principle |
