# DUELL feature list (owner's choices; kept current by Claude)

Started 2026-10-19 in the feature-research thread. Only features the owner has chosen or parked. Design studies are named where they exist. Nothing here is built yet. UI proposals are always for the owner to approve first.

## For the next version (chosen by the owner)

| # | Feature | What it is | Status / document |
|---|---|---|---|
| 1 | **VELOCITY and AFTERTOUCH from the keys** | The FM-1's keys play a fixed velocity of 100 today. Velocity and aftertouch (pressure) from the keys, as in Charles Vestal's Velo (GPL-3.0). Aftertouch into the morph is only worth doing together with velocity. | Study done: VELO_STUDY.md (10-14 files, 600-900 lines, scan cost 1-3 % CPU; plan B: velocity only, 20 keys). Needs a first raw key recording from the unit. |
| 2 | **TWIN FILTER (Virus-style second filter)** | A second filter per note, in series or parallel with the first (the existing MULTI FILTER), a DRIVE between the two. Filter 2 TYPES: OFF, LP, BP, HP, NOTCH, LADDER, COMB (selectable filter, not an insert); the TRIO filter solution is a type too. The engines' own filters (ANALOG, TRIO, NOISE) leave the engines; their characters live on as filter-2 types; patches get updated. Filter 2 cutoff is LINKED to filter 1 by default (offset), with its own ENV / KEY TRACK / LFO 1 / LFO 2 amounts and a CUTOFF 2 matrix destination. PARALLEL with BLEND in the first version. | Design: TWIN_FILTER_DESIGN.md and TWIN_FILTER_DESIGN_2.md. DECIDED (owner, 2026-10-19): filter 2's LP/BP/HP/NOTCH are the TRIO-style filter, filter 1 stays the clean one; conversion rules yes (43 sounds, 10 multis, he listens before); chain before the amp on every engine; engine filters go BEFORE the alpha; LINK default on. PAGES (FILTER_PAGES_PROPOSAL.md, owner answers 2026-10-19): DRIVE hidden and not run while filter 2 is OFF; a new FILTER tab in the editor; page names TWIN FILTER / TWIN MOD / TWIN CHAIN; page set = option B (owner confirmed 2026-10-19: TWIN FILTER, TWIN MOD, TWIN CHAIN after MULTI FILTER 1/2 and 2/2). QUICK MULTI hold layer DECIDED (2026-10-19): filter 2 off = CUT, RES, MODE, AMP REL; linked = CUT, RES, DRIVE, AMP REL; unlinked = CUT 1, RES 1, CUT 2, RES 2. |
| 3 | **EDITABLE B** | Set the morph's B end by hand (his own earlier request); "take B from another slot" is a helper idea. | Design: EDITABLE_B_DESIGN.md (owner's words 2026-10-13, recommended answers 1 to 5). BUILT in cloud job G (cloud/feat-editb): host tests pass; the feel and the header letter on the unit are the owner's to check. |
| 4 | **CYCLING ENVELOPE (CYCLE) as a mod source** | A separate per-note looping envelope (MODE LOOP/ONCE, RISE, FALL, CURVE), a new matrix source. | Design: SUB_AND_CYCLE_DESIGN.md. Decided: a separate envelope (not a loop switch on ENV 2). DECIDED (2026-10-19): CYCLE gets its OWN SYNC switch (the amp envelope stays in milliseconds); one more stored value in the extension block. |
| 5 | **SUB LAYER oscillator** | An engine-independent sub oscillator per note: selectable octave and waveform, maybe harmonics or saturation. If too heavy for the CPU: not on the heaviest engines, or only when not polyphonic. | Design: SUB_AND_CYCLE_DESIGN.md. DECIDED (owner, 2026-10-19): SUB on every engine, paid for by the notes counter (no engine ban, no mono-only rule); fourth knob HARMONICS (a sub drive may come later); OCTAVE -2/-1/0 and WAVE SINE/TRI/SQUARE/PULSE; WAVE's own SUB card is retired and converted to SUB LAYER SINE -1. |
| 6 | **STEPPING mode in the WAVE engine** | A stepped wave scan like the Microwave's: the WAVE engine's table position (POS) moves in steps instead of smoothly (steps in time, or per note, maybe tempo-synced). Today POS moves smoothly (envelope, LFO, matrix); a sample-and-hold LFO on POS only gives random steps. | Chosen by the owner 2026-10-19. Design: SUB_AND_CYCLE_DESIGN.md section C. DECIDED: one STEPS control on the WAVE engine (OFF, 2 to 19 steps) with a fixed short cross-fade; the existing WAVE table 24 'STEPS' is renamed STAIR. |

| 7 | **MIDI MAP: see and decide which parameters have their own MIDI CC and which are learn-only** | A visible map (unit and editor) of every parameter's MIDI CC; the owner decides which get a fixed CC of their own and which can only be set through MIDI LEARN. Covers the new extension-block values too. | Owner request 2026-10-19. Design: MIDI_MAP_DESIGN.md; DECIDED (owner 2026-10-19): everything may change until the alpha; one global map; the new values need new CCs that RESPECT INDUSTRY-STANDARD CC names (breath, sends, etc.; exceptions only by his call); follow the Access Virus' CC layout for filters etc.; he dislikes MIDI learn (Akai Force Q-Link assignment is hard without clear CC names). Version 2 in progress: MIDI_MAP_DESIGN_2.md. |

### Patch-format decisions (EXTENSION_BLOCK_PLAN.md, owner answers 2026-10-19)

- 6 records per chunk (limit 596 B, up to 32 hand edits in editable B): taken as recommended (the owner had no opinion on the technical choice).
- MIDI LEARN / locks for the 15 extension values: reserve the room now; the owner wants to SEE and DECIDE which parameters have their own CC and which are learn-only (item 7).
- Backup of the patch store: one patch at a time, plus a "select all and back up at once" function.
- Extension slots held in memory: undecided ('we shall see'); 16 is the default.
- Ids 25 and 26: cleaned up so they carry TWIN's RES 2 and DRIVE (yes).

## Parked (not rejected)

- **Ring modulation between the two parts** (part 1 x part 2): new; chords on both parts tend to turn into noise; decide after a listening test. TRIO already has ring modulation between its own oscillators.
- **FILTER FM** (the sound itself, or another signal, wobbles the cutoff at audio speed: growl, metallic overtones): an option after a listening test.
- **LISTEN** (the other part's loudness as a matrix source): small; not asked for.
- Other filter-2 types (FORMANT, MS-style, CHIP) and drive curves: decided by ear in the offline test.

## Rejected by the owner (helper inventions)

- A morph played from the keys, velocity or aftertouch; a morph split into groups (MORPH GROUPS); one morph fader across two different engines (BRIDGE). Reason: morph is already playable and there is a mod matrix.

## Sources

HEADLINE_FEATURES.md, HEADLINE_FEATURES_2.md, TWIN_FILTER_DESIGN.md, CLIPPER_STUDY.md, VELO_STUDY.md, and the owner's notes in DUELL/research/.
