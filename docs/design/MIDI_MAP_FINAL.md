# MIDI MAP, FINAL version: the complete CC table with the owner's answers applied, the standard names kept, and what the Akai Force and MPC really show (2026-10-20, updated by the second round of the same day)

Private. Research and writing only: no code changed, no branch made, no picture drawn (plain text tables). INDEX.md and the other documents were not touched. This replaces the number choices of MIDI_MAP_DESIGN_2.md ("version 2") and applies the owner's answers; the table design, the unit page and the editor tab of MIDI_MAP_DESIGN.md ("version 1") stay, and section 6 lists only what changes. The owner's second round of decisions on 2026-10-20 is applied throughout and summarised in section 2b.

Marks, as in the other studies: **CONFIRMED** (read today in a manual, a release note, a licence file or the firmware), **est.** (worked out, not measured), **HYPOTHESIS** (my reading, not proven).

## Words used (the rest are in GLOSSARY.md)

- **MIDI CC (control change)**: a message "controller number N has value V", N and V both 0 to 127. A knob on a controller sends one.
- **Standard name**: the name the MIDI specification (the MIDI Association's list at midi.org) gives a CC number: 7 is "Channel Volume", 74 is "Sound Controller 5, default Brightness". Synths, DAWs and controllers use these names in their lists.
- **Direct CC**: a CC number that is built in and always works, without teaching it. **Learn-only**: a parameter with no CC of its own; it answers a CC only after you teach one with MIDI LEARN (or type one into the MIDI MAP).
- **Input number**: a CC that DUELL reads as a performance controller for the modulation matrix (CC1 mod wheel, CC11 expression, CC64 sustain pedal, and now CC2 breath and CC4 foot), not as a parameter. **Locked number**: a CC the MIDI standard owns for system jobs (bank select, data entry, notes off ...) that DUELL never gives away.
- **Matrix source**: the "from" side of a modulation matrix slot (LFO 1, an envelope, velocity, the mod wheel ...). It moves a destination such as the cutoff.
- **Fine half (LSB)**: numbers 32 to 63 are, in the specification, the second half of a "14-bit" pair for controllers 0 to 31. Almost every knob sends only the coarse CC, so the fine halves are in practice free; a controller set to 14-bit mode would clash.
- **Exception (X2 to X7)**: a place where DUELL bends a standard meaning or uses a standard-defined number for something else. Each is the owner's call. The numbers X1 and X8 are retired (X1 was BALANCE on CC8, now removed; X8 was "CC70 is not the morph", now decided the other way), and the other numbers keep their old names so that older notes still match.
- **Value halves**: the convention that one CC carries two actions: low values mean the first, high values mean the second, and the middle means nothing (see the ROLL rule below). The standard on/off switches already work this way (63 and below off, 64 and above on). The new ROLL CC (below) uses it.
- **Matrix destination**: the "to" side of a modulation matrix slot (the cutoff, the pitch ...); a source moves it.
- **Q-Link**: the Akai Force's assignable knobs (also on the MPC line). **Part, channel, scope P / G**: DUELL has two playable parts, each with its own MIDI channel (default 1 and 2). A "P" row moves the part that the CC's channel plays; a "G" row is global and answers on any channel. Since the second round of 2026-10-20, a setting that belongs to both parts together (BALANCE, XFADE, SPLIT note, MODE) has no direct CC at all, so no CC row can change it.
- **Kinds**: CONT (continuous), BIP (bipolar, centre 64), SW (switch, 64 and up = on), STEP n (n choices over the range), ACT (fires once at 64 or more, re-arms below 64).

---

## 1. Verdict (half a page)

**The rules, now decided.** One global map with one line per CC number. Every number is filled in three layers: **STANDARD** (a standard name that DUELL's parameter truly matches), **VIRUS-LIKE** (no standard: a free number, in the Access Virus' section order), **EXCEPTION** (anything that bends a standard meaning; X2 to X7 are approved by the owner, X1 and X8 no longer exist). Where a MIDI standard name exists, the number keeps it. Everything may still change until the alpha. A taken number is never replaced silently: the unit and the editor ask first, default no.

**The principle of the second round (2026-10-20).** A MIDI channel belongs to one part, so a CC on a part's channel only sets things of that one part. Anything that belongs to both parts together (BALANCE, XFADE, SPLIT note, MODE) has no CC; it is learn-only and global. Someone who sends a CC from outside will therefore only change the volume of the channels, never the two-part settings.

**What the Force and the MPC really show (section 4).** CONFIRMED in the MPC user guides 3.7 and 3.9: the MIDI track's CC list names exactly 13 CC numbers (1, 2, 3, 4, 5, 7, 10, 11, 64, 65, 66, 67, 68, for example "Breath", "Foot", "Portamento", "Volume", "Sustain On/Off"). I found no manual that names the other numbers; the owner saw "some" names on his Force, which fits. The names differ from the MIDI Association's in wording only, never in meaning. Nothing in DUELL has to be renamed; the MIDI MAP shows both names side by side.

**Changes of the second round (details in section 2b).**
1. **CC8 is unused.** BALANCE, XFADE, SPLIT note and MODE are learn-only; exception X1 is gone.
2. **The morph fader sits on CC70**, the standard "Sound Controller 1 (Sound Variation)"; exception X8 is gone because it is now decided yes.
3. **ROLL is one CC (102), centred like the knob on the unit**: 0 to 42 rolls End A, 43 to 84 does nothing (the middle, 64 is the rest position), 85 to 127 rolls End B. **MORPH AMOUNT is CC103.** UNDO and the four morph setting switches (TARGET, SMART/WILD, ARP, PITCH) are learn-only.
4. **X6 stays** (the mod wheel CC1 moves the morph fader): checked in the firmware, it is a separate, direct path and takes no matrix slot. **X7 stays** (the expression pedal CC11 only feeds the matrix).
5. **BREATH (CC2) and FOOT (CC4)** stay matrix sources. **MORPH becomes a matrix destination** (FEATURE_LIST item 9, the next edition); no change to the CC map.

**Earlier changes against version 2 (still valid).** BREATH and FOOT as inputs on their standard numbers; LFO rows beyond the vibrato learn-only (X4 shrinks to 16 numbers); no Force sheet; EQ and ARP learn-only; INSERT direct (X5).

**The counts (section 3.4, checked by a script: every number 0 to 127 appears once, no parameter twice).**
- **70 direct CCs** if you approve everything: 13 on a standard number that matches exactly, 34 on free numbers (Virus-like order), 23 on exception numbers (X2 1, X3 3, X4 16, X5 3).
- **47 direct CCs with no exception at all** (the 13 exact standard ones and the 34 free numbers, which include all 20 new values, the morph fader and the roll).
- **5 input numbers** (1, 2, 4, 11, 64), **18 locked**, **32 unused** (free for learn), **3 reserved** (3, 9, 119, for the planned later MIDI options). 70 + 5 + 18 + 32 + 3 = 128.
- **About 108 parameters stay learn-only** (est. plus or minus 4): the matrix choices and amounts, EQ, ARP and SEQ setup, DUCK, the LFOs beyond the vibrato, the voice setup, the master settings, the four two-part settings and five morph settings (UNDO and the four switches). Every one can still be set from the editor (SysEx reaches all parameters) and by learn.
- **Open questions: none.**

---

## 2. Decided by the owner (his words, 2026-10-19 and 2026-10-20)

> "Everything can change today. Until alpha, everything can change. Global map. Five default."

> "The new we need new CCs, but I want the CCs to be somehow respecting industry standards, because there are a lot of these CC codes that already have a name to them. On other devices, like breath control and send and so on. So I would like to respect those industry standard namings for the CC code, so it's easier, because, for example, on the Akai Force the list is not super clear, and it's difficult to set it up if we don't use MIDI learn, which I don't like to use. [...] And the other thing I would like us to take ideas from for the CC build-out is the Access Virus itself. Because they have a similar structure for the filters and stuff."

> "I didn't say that the Force can import anything. It's shitty, the implementation. But I scrolled through a list of 128 and some have names already there. That I guess they are some MIDI standard names. And I would like to keep those names wherever there is something that exists already in the MIDI standards. So we don't mess things up."

His answers to the exceptions and the open points (2026-10-20):
- **X1** BALANCE on CC8: "balance means pan basically, right? I think that's okay." (first round; **withdrawn in the second round**, see section 2b)
- **X2** DELAY send on 92 (DETUNE on 94): ok. **X3** LFO 1 as the vibrato (76 to 78): ok. **X4** the fine-half block: ok. **X5** INSERT on 12, 13, 95: ok ("I'm also okay if the inserts are just on learn").
- "We need to also have a CC for the morph, for the roll also."
- All 20 new values get direct CCs.
- "EQ, the LFO, the ARP I think can also be in learn."
- Breath (CC2) and foot (CC4) as matrix sources: "let's use the breath and foot also."
- A taken number: ask first.
- "The Force is the same as the MPC, the firmware we are using on it, so you can also look at MPC, not just Force."
- "I don't mind if we take stuff from other open source projects. Don't waste energy coding something somebody else did successfully and well; if tested and tried, let's go with that."
- No Force sheet: he never said the Force imports anything; he wants the standard names kept because the Force/MPC list of 128 shows them.

**Answer to his question on X1 (first round, kept for the record).** The standard **Balance (CC8)** is the left/right balance of a stereo channel, a different job from DUELL's **BALANCE**, the level balance between part 1 and part 2. In the second round the owner settled it by a bigger rule (section 2b): two-part settings get no CC on a part's channel at all, so CC8 is simply unused.

**Exceptions X6 and X7 were open in the first round; both are now decided yes** (section 2b). X8 is gone.

**Two readings I took, flagged because the answers pull two ways:**
1. "LFO beyond the vibrato are learn-only" and "all 20 new values direct" meet at two rows: LFO 1 to CUTOFF 2 (CC30) and LFO 2 to CUTOFF 2 (CC31) are LFO routing rows and also two of the 20 new values. I kept them **direct** (the specific answer, "all 20 new values", wins over the general one). The other 8 LFO rows (LFO 1 SHAPE, to CUTOFF, to AMP; LFO 2 RATE, SHAPE, to CUTOFF, to PITCH, to SHAPE) are learn-only. If you want CC30 and CC31 learn-only as well, two more numbers join the free pool; nothing else moves.
2. "The morph setting switches" (MORPH TARGET, SMART, ARP, PITCH) were kept direct (107 to 110) in the first round. **Superseded:** in the second round the owner made them learn-only (section 2b).

---

## 2c. ROLL is centred (owner, 2026-10-20, third round)

The owner pointed out that with 0 meaning "nothing", a knob swept up to reach End B would pass through the End A values and roll A on the way. So "does nothing" sits in the MIDDLE, exactly like the ROLL knob on the unit (twist left = roll A, twist right = roll B, middle = nothing): **0 to 42 rolls End A, 43 to 84 does nothing, 85 to 127 rolls End B.** A roll fires once when the value ENTERS its zone; to roll again the value has to leave the zone (for example back through the middle). One thing to know: a pad or button that sends 127 when pressed and 0 when released would roll B and then A. For such buttons, two separate action CCs would be needed later; knobs and faders with a centre work as designed.

## 2b. Changes 2026-10-20 (second round)

The owner's second round of decisions on the same day. Everything below is applied in the tables of section 3.

**1. The two-part settings get no CC on a part's channel.** BALANCE (how loud part 1 is against part 2), XFADE, the SPLIT note and MODE all belong to both parts together. His words:

> "balance between the two parts is a two-part thing, and MIDI channels are for one part, so whatever setting is for both parts shouldn't be set like this... we shouldn't add any global settings to part one or part two MIDI setting... if somebody changes them from outside with CC, they will change the volume of the two channels, and that's it."

Result: these four are learn-only and global (a controller can still be taught to them with MIDI LEARN). **CC8 is now UNUSED**, because the standard Balance has no DUELL parameter that matches it. Exception X1 is removed. The numbers 111, 112 and 113 (MODE, XFADE, SPLIT NOTE) and 8 become free.

**2. The morph numbers.**
- The morph fader goes on **CC70**, the standard "Sound Controller 1 (Sound Variation)". His words: "I'm fine with CC70". Exception X8 ("CC70 is not the morph") is removed because the answer is now yes, and CC70 becomes an exact standard match.
- ROLL A and ROLL B become **one CC named ROLL**. His words: "good old A and B, why does it need a different CC? isn't it like a toggle kind of CC message?" The value decides: **0 to 42 rolls End A, 43 to 84 does nothing (the middle), 85 to 127 rolls End B**, the same value-halves convention as the standard on/off switches. A pad that always sends 127 can therefore only roll End B, and a knob or fader range is needed to roll End A; the owner can decide later whether that is enough. This is a roll action (it fires once and re-arms when the value returns to 0), not an A/B selection that stays set.
- **UNDO gets no CC**; learn-only if wanted.
- **AMOUNT keeps a direct CC.**
- The four morph setting switches (TARGET, SMART/WILD, ARP, PITCH) are **learn-only**.
- **New numbers: ROLL on CC102, AMOUNT on CC103.** Why: the numbers beside 70 are not free, because 65 to 69 and 71 to 79 carry standard meanings and 80 to 95 hold the effect rows, so the nearest numbers that no standard names and nothing else uses are the 102 and 103 that the morph fader has left; they keep the morph group at the front of the old morph run (fader 70, roll 102, amount 103) and leave the Virus-like filter run 14 to 31 whole. The numbers 104 to 113 become unused.

**3. X6 stays (decided yes): the mod wheel CC1 also moves the morph fader.** His question: "Is it moving it because we put it in the matrix, or is it separate from the matrix?" Answer, read in the firmware (read only, `felucca-base/firmware/src`): **it is separate and direct, and it does not use the matrix.** In `midi_control.c`, the handler for CC1 stores the wheel value (the value the matrix source MODW later reads) and, in the same place, calls `morph_push(MQ_FADER, ...)` for the part of the channel (for both parts in LAYER mode). That queue entry goes straight to the part's morph fader in `morph.c`. No matrix slot is read or needed, so the wheel moves the fader with every slot empty and takes no space in the matrix, which is exactly what he asked for ("it should be just moving it without occupying a space in the matrix"). **Nothing has to change.** (The same wheel value is also available as the matrix source MODW, but that is a second, independent use.)

**4. X7 stays (decided yes): the expression pedal CC11 only feeds the matrix as a source and does not change the loudness by itself.**

**5. BREATH (CC2) and FOOT (CC4) stay matrix sources, as decided. New request: MORPH becomes a matrix destination** (FEATURE_LIST item 9; his words: "the matrix should have a destination for the morph fader... that will be the next edition"). That is the way breath, foot, an LFO or CYCLE can later move the morph fader. It needs no change to the CC map. Today the morph fader is not a destination (the firmware says so in `mod.c`), so section 5 still says BREATH and FOOT do not reach the morph until that edition is made.

**6. Unchanged:** the standard-name rules, the approved exceptions X2 (DELAY send 92, DETUNE 94), X3 (LFO 1 as vibrato, 76 to 78), X4 (the fine-half block) and X5 (INSERT on 12, 13, 95), and the rest of the map.

---

## 3. THE FINAL MAP

### 3.1 How the table reads

- One global table, 128 lines, one per CC number. A line says what the number moves, for which part, where the number comes from, and its state.
- **State:** DIRECT (a parameter with its own CC), INPUT (a matrix source, not editable, like CC1 and CC11), LOCKED (the standard's, never used), UNUSED (free; typing or learning a number here shows its standard name first), RESERVED (kept clean for the later MIDI options: SEQ / ARP / external START-STOP switches, AUTO clock).
- **Force/MPC shows:** the name the MPC user guides 3.7 and 3.9 print for that number (CONFIRMED, section 4). A dash means no manual names it; I do not guess.
- The parameter names are the unit's own. Engine values E1 to E8 take the labels of the current engine.
- Against version 2: CC2 and CC4 are inputs now, 56 to 63 are free, and (second round) CC8 and 104 to 113 are free, the morph fader sits on 70, ROLL on 102 and AMOUNT on 103.

### 3.2 The full table, CC 0 to 127

| CC | Standard name (midi.org) | Force/MPC shows | DUELL parameter | Scope | Source of number | State | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | Bank Select (MSB) | - | bank select (for program change) | G | STANDARD | LOCKED |  |
| 1 | Modulation Wheel or Lever | Modulation | MOD WHEEL (matrix source MODW); also moves the MORPH fader directly, not through the matrix | P | STANDARD (X6) | INPUT |  |
| 2 | Breath Controller | Breath | BREATH (NEW matrix source) | P | STANDARD | INPUT |  |
| 3 | Undefined | Undefined | kept for the later MIDI options | - | - | RESERVED |  |
| 4 | Foot Controller | Foot | FOOT (NEW matrix source) | P | STANDARD | INPUT |  |
| 5 | Portamento Time | Portamento | GLIDE | P | STANDARD | DIRECT | CONT |
| 6 | Data Entry (MSB) | - | data entry (bend range) | G | STANDARD | LOCKED |  |
| 7 | Channel Volume | Volume | LEVEL (mixer level of the part) | P | STANDARD | DIRECT | CONT |
| 8 | Balance | - | not used (BALANCE of the two parts is learn-only) | - | - | UNUSED |  |
| 9 | Undefined | - | kept for the later MIDI options | - | - | RESERVED |  |
| 10 | Pan | Pan | PAN | P | STANDARD | DIRECT | BIP |
| 11 | Expression Controller | Expression | EXPRESSION (matrix source EXPR) | P | STANDARD (X7) | INPUT |  |
| 12 | Effect Control 1 | - | INSERT A | P | EXCEPTION X5 | DIRECT | CONT |
| 13 | Effect Control 2 | - | INSERT B | P | EXCEPTION X5 | DIRECT | CONT |
| 14 | Undefined | - | SUB LEVEL | P | VIRUS-LIKE | DIRECT | CONT; 0 = not run |
| 15 | Undefined | - | SUB OCTAVE | P | VIRUS-LIKE | DIRECT | STEP 3 |
| 16 | General Purpose Controller 1 | - | SUB WAVE | P | VIRUS-LIKE | DIRECT | STEP 4 |
| 17 | General Purpose Controller 2 | - | SUB HARMONICS | P | VIRUS-LIKE | DIRECT | CONT |
| 18 | General Purpose Controller 3 | - | F1 ENV AMT | P | VIRUS-LIKE | DIRECT | BIP |
| 19 | General Purpose Controller 4 | - | F1 KEY TRK | P | VIRUS-LIKE | DIRECT | BIP |
| 20 | Undefined | - | F1 MODE | P | VIRUS-LIKE | DIRECT | CONT, LP 0 / BP 64 / HP 127 |
| 21 | Undefined | - | CUTOFF 2 | P | VIRUS-LIKE | DIRECT | CONT |
| 22 | Undefined | - | RES 2 | P | VIRUS-LIKE | DIRECT | CONT |
| 23 | Undefined | - | F2 ENV AMT | P | VIRUS-LIKE | DIRECT | BIP |
| 24 | Undefined | - | F2 KEY TRK | P | VIRUS-LIKE | DIRECT | BIP |
| 25 | Undefined | - | F2 TYPE | P | VIRUS-LIKE | DIRECT | STEP 7 |
| 26 | Undefined | - | DRIVE | P | VIRUS-LIKE | DIRECT | CONT |
| 27 | Undefined | - | BLEND | P | VIRUS-LIKE | DIRECT | BIP |
| 28 | Undefined | - | ROUTE | P | VIRUS-LIKE | DIRECT | SW |
| 29 | Undefined | - | LINK | P | VIRUS-LIKE | DIRECT | SW |
| 30 | Undefined | - | LFO 1 to CUTOFF 2 | P | VIRUS-LIKE | DIRECT | BIP |
| 31 | Undefined | - | LFO 2 to CUTOFF 2 | P | VIRUS-LIKE | DIRECT | BIP |
| 32 | Bank Select (LSB) | - | bank select (fine) | G | STANDARD | LOCKED |  |
| 33 | LSB for CC 1 | - | clear on purpose (fine half of 1, 5, 7, 8, 10, 11) | - | - | UNUSED |  |
| 34 | LSB for CC 2 | - | SPREAD | P | EXCEPTION X4 | DIRECT | CONT |
| 35 | LSB for CC 3 | - | SATURATION | P | EXCEPTION X4 | DIRECT | CONT |
| 36 | LSB for CC 4 | - | INSERT C | P | EXCEPTION X4 | DIRECT | CONT |
| 37 | LSB for CC 5 | - | clear on purpose (fine half of 1, 5, 7, 8, 10, 11) | - | - | UNUSED |  |
| 38 | Data Entry (LSB) | - | data entry (fine) | G | STANDARD | LOCKED |  |
| 39 | LSB for CC 7 | - | clear on purpose (fine half of 1, 5, 7, 8, 10, 11) | - | - | UNUSED |  |
| 40 | LSB for CC 8 | - | clear on purpose (fine half of 1, 5, 7, 8, 10, 11) | - | - | UNUSED |  |
| 41 | LSB for CC 9 | - | F1 SLOPE | P | EXCEPTION X4 | DIRECT | STEP 2 |
| 42 | LSB for CC 10 | - | clear on purpose (fine half of 1, 5, 7, 8, 10, 11) | - | - | UNUSED |  |
| 43 | LSB for CC 11 | - | clear on purpose (fine half of 1, 5, 7, 8, 10, 11) | - | - | UNUSED |  |
| 44 | LSB for CC 12 | - | E1 (engine value 1) | P | EXCEPTION X4 | DIRECT | scaled by the engine |
| 45 | LSB for CC 13 | - | E2 (engine value 2) | P | EXCEPTION X4 | DIRECT | scaled by the engine |
| 46 | LSB for CC 14 | - | E3 (engine value 3) | P | EXCEPTION X4 | DIRECT | scaled by the engine |
| 47 | LSB for CC 15 | - | E4 (engine value 4) | P | EXCEPTION X4 | DIRECT | scaled by the engine |
| 48 | LSB for CC 16 | - | E5 (engine value 5) | P | EXCEPTION X4 | DIRECT | scaled by the engine |
| 49 | LSB for CC 17 | - | E6 (engine value 6) | P | EXCEPTION X4 | DIRECT | scaled by the engine |
| 50 | LSB for CC 18 | - | E7 (engine value 7) | P | EXCEPTION X4 | DIRECT | scaled by the engine |
| 51 | LSB for CC 19 | - | E8 (engine value 8) | P | EXCEPTION X4 | DIRECT | scaled by the engine |
| 52 | LSB for CC 20 | - | ENV 2 ATTACK | P | EXCEPTION X4 | DIRECT | CONT |
| 53 | LSB for CC 21 | - | ENV 2 DECAY | P | EXCEPTION X4 | DIRECT | CONT |
| 54 | LSB for CC 22 | - | ENV 2 SUSTAIN | P | EXCEPTION X4 | DIRECT | CONT |
| 55 | LSB for CC 23 | - | ENV 2 RELEASE | P | EXCEPTION X4 | DIRECT | CONT |
| 56 | LSB for CC 24 | - | free (was an LFO row, now learn-only) | - | - | UNUSED |  |
| 57 | LSB for CC 25 | - | free (was an LFO row, now learn-only) | - | - | UNUSED |  |
| 58 | LSB for CC 26 | - | free (was an LFO row, now learn-only) | - | - | UNUSED |  |
| 59 | LSB for CC 27 | - | free (was an LFO row, now learn-only) | - | - | UNUSED |  |
| 60 | LSB for CC 28 | - | free (was an LFO row, now learn-only) | - | - | UNUSED |  |
| 61 | LSB for CC 29 | - | free (was an LFO row, now learn-only) | - | - | UNUSED |  |
| 62 | LSB for CC 30 | - | free (was an LFO row, now learn-only) | - | - | UNUSED |  |
| 63 | LSB for CC 31 | - | free (was an LFO row, now learn-only) | - | - | UNUSED |  |
| 64 | Damper Pedal (Sustain) | Sustain On/Off | SUSTAIN pedal | P | STANDARD | INPUT |  |
| 65 | Portamento On/Off | Pmento On/Off | not used | - | - | UNUSED |  |
| 66 | Sostenuto | Sost On/Off | not used | - | - | UNUSED |  |
| 67 | Soft Pedal | Soft Ped On/Off | not used | - | - | UNUSED |  |
| 68 | Legato Footswitch | Legato Switch | not used | - | - | UNUSED |  |
| 69 | Hold 2 | - | not used | - | - | UNUSED |  |
| 70 | Sound Controller 1 (Sound Variation) | - | MORPH FADER | P | STANDARD | DIRECT | CONT; CC1 moves it too |
| 71 | Sound Controller 2 (Timbre / Harmonic Intensity) | - | FILTER 1 RESONANCE | P | STANDARD | DIRECT | CONT |
| 72 | Sound Controller 3 (Release Time) | - | AMP RELEASE | P | STANDARD | DIRECT | CONT |
| 73 | Sound Controller 4 (Attack Time) | - | AMP ATTACK | P | STANDARD | DIRECT | CONT |
| 74 | Sound Controller 5 (Brightness) | - | FILTER 1 CUTOFF | P | STANDARD | DIRECT | CONT |
| 75 | Sound Controller 6 (Decay Time) | - | AMP DECAY | P | STANDARD | DIRECT | CONT |
| 76 | Sound Controller 7 (Vibrato Rate) | - | LFO 1 RATE | P | EXCEPTION X3 | DIRECT | CONT |
| 77 | Sound Controller 8 (Vibrato Depth) | - | LFO 1 to PITCH | P | EXCEPTION X3 | DIRECT | BIP |
| 78 | Sound Controller 9 (Vibrato Delay) | - | LFO 1 FADE | P | EXCEPTION X3 | DIRECT | CONT |
| 79 | Sound Controller 10 (no default) | - | AMP SUSTAIN | P | STANDARD | DIRECT | CONT |
| 80 | General Purpose Controller 5 | - | CHORUS RATE | G | VIRUS-LIKE | DIRECT | CONT |
| 81 | General Purpose Controller 6 | - | CHORUS DEPTH | G | VIRUS-LIKE | DIRECT | CONT |
| 82 | General Purpose Controller 7 | - | DELAY TIME | G | VIRUS-LIKE | DIRECT | STEP (note values) |
| 83 | General Purpose Controller 8 | - | DELAY FEEDBACK | G | VIRUS-LIKE | DIRECT | CONT |
| 84 | Portamento Control | - | not used (needs a note number) | - | - | UNUSED |  |
| 85 | Undefined | - | DELAY COLOUR | G | VIRUS-LIKE | DIRECT | CONT |
| 86 | Undefined | - | DELAY MIX | G | VIRUS-LIKE | DIRECT | CONT |
| 87 | Undefined | - | REVERB SIZE | G | VIRUS-LIKE | DIRECT | CONT |
| 88 | High Resolution Velocity Prefix | - | not used | - | - | UNUSED |  |
| 89 | Undefined | - | REVERB DAMP | G | VIRUS-LIKE | DIRECT | CONT |
| 90 | Undefined | - | REVERB LOW CUT | G | VIRUS-LIKE | DIRECT | CONT |
| 91 | Effects 1 Depth (Reverb Send) | - | REVERB send | P | STANDARD | DIRECT | CONT |
| 92 | Effects 2 Depth | - | DELAY send | P | EXCEPTION X2 | DIRECT | CONT |
| 93 | Effects 3 Depth (Chorus Send) | - | CHORUS send | P | STANDARD | DIRECT | CONT |
| 94 | Effects 4 Depth | - | DETUNE | P | STANDARD | DIRECT | CONT |
| 95 | Effects 5 Depth | - | INSERT MIX | P | EXCEPTION X5 | DIRECT | CONT |
| 96 | Data Increment | - | data increment / decrement, NRPN / RPN select | G | STANDARD | LOCKED |  |
| 97 | Data Decrement | - | data increment / decrement, NRPN / RPN select | G | STANDARD | LOCKED |  |
| 98 | NRPN (LSB) | - | data increment / decrement, NRPN / RPN select | G | STANDARD | LOCKED |  |
| 99 | NRPN (MSB) | - | data increment / decrement, NRPN / RPN select | G | STANDARD | LOCKED |  |
| 100 | RPN (LSB) | - | data increment / decrement, NRPN / RPN select | G | STANDARD | LOCKED |  |
| 101 | RPN (MSB) | - | data increment / decrement, NRPN / RPN select | G | STANDARD | LOCKED |  |
| 102 | Undefined | - | MORPH ROLL (0-42 rolls End A, 43-84 nothing, 85-127 rolls End B) | P | VIRUS-LIKE | DIRECT | ACT, value halves |
| 103 | Undefined | - | MORPH AMOUNT | P | VIRUS-LIKE | DIRECT | CONT |
| 104 | Undefined | - | not used (free for learn) | - | - | UNUSED |  |
| 105 | Undefined | - | not used (free for learn) | - | - | UNUSED |  |
| 106 | Undefined | - | not used (free for learn) | - | - | UNUSED |  |
| 107 | Undefined | - | not used (free for learn) | - | - | UNUSED |  |
| 108 | Undefined | - | not used (free for learn) | - | - | UNUSED |  |
| 109 | Undefined | - | not used (free for learn) | - | - | UNUSED |  |
| 110 | Undefined | - | not used (free for learn) | - | - | UNUSED |  |
| 111 | Undefined | - | not used (free for learn) | - | - | UNUSED |  |
| 112 | Undefined | - | not used (free for learn) | - | - | UNUSED |  |
| 113 | Undefined | - | not used (free for learn) | - | - | UNUSED |  |
| 114 | Undefined | - | CYCLE MODE | P | VIRUS-LIKE | DIRECT | SW |
| 115 | Undefined | - | CYCLE RISE | P | VIRUS-LIKE | DIRECT | CONT |
| 116 | Undefined | - | CYCLE FALL | P | VIRUS-LIKE | DIRECT | CONT |
| 117 | Undefined | - | CYCLE CURVE | P | VIRUS-LIKE | DIRECT | BIP |
| 118 | Undefined | - | CYCLE SYNC | P | VIRUS-LIKE | DIRECT | SW |
| 119 | Undefined | - | kept for the later MIDI options | - | - | RESERVED |  |
| 120 | All Sound Off | - | channel mode: sound off, reset, notes off ... | G | STANDARD | LOCKED |  |
| 121 | Reset All Controllers | - | channel mode: sound off, reset, notes off ... | G | STANDARD | LOCKED |  |
| 122 | Local Control On/Off | - | channel mode: sound off, reset, notes off ... | G | STANDARD | LOCKED |  |
| 123 | All Notes Off | - | channel mode: sound off, reset, notes off ... | G | STANDARD | LOCKED |  |
| 124 | Omni Mode Off | - | channel mode: sound off, reset, notes off ... | G | STANDARD | LOCKED |  |
| 125 | Omni Mode On | - | channel mode: sound off, reset, notes off ... | G | STANDARD | LOCKED |  |
| 126 | Mono Mode On | - | channel mode: sound off, reset, notes off ... | G | STANDARD | LOCKED |  |
| 127 | Poly Mode On | - | channel mode: sound off, reset, notes off ... | G | STANDARD | LOCKED |  |

### 3.3 Learn-only parameters (no direct CC; reachable by learn, by typing a number in the MIDI MAP, and by the editor)

About 108 rows (est. plus or minus 4: the exact count follows the ids at the time of making). 85 per part and 23 global.

| Group | Rows | Why |
| --- | --- | --- |
| MIXER (2) | MUTE, TRIM (patch level) | MUTE is a switch you might want live; if so, release a reserved number |
| VOICE (5) | VOICE mode, GLIDE MODE, PRIORITY, CHORD, VOICING | setup |
| ENV destinations (5) | ENV 1 to CUTOFF, PITCH, SHAPE; ENV 2 to SHAPE; ENV SYNC | the matrix does the same |
| LFO 1 (8) | SHAPE, to CUTOFF, to AMP, PHASE, SYNC, TRIGGER, POLARITY, to SHAPE | **owner: LFO beyond the vibrato is learn-only** (SHAPE, to CUTOFF, to AMP are the 3 rows that left the fine-half block) |
| LFO 2 (10) | RATE, SHAPE, to CUTOFF, to PITCH, to SHAPE, FADE, PHASE, SYNC, TRIGGER, POLARITY | same (RATE, SHAPE, to CUTOFF, to PITCH, to SHAPE left the fine-half block). LFO 2 to CUTOFF 2 (CC31) stays direct, see section 2 |
| MATRIX (24) | 8 x SOURCE, DESTINATION, AMOUNT | choices set once; the amounts are the ones you might want live |
| EQ (7) | LOW, LOW HZ, MID, MID HZ, MID Q, HIGH, HIGH HZ | **owner: EQ is learn-only** |
| INSERT (1) | TYPE (A, B, C and MIX have CCs) | TYPE is a choice |
| DUCK (7) | CURVE, DEPTH, ATTACK, RELEASE, SOURCE, RATE, THRESHOLD | setup |
| ARP (9) | MODE, RATE, OCTAVES, GATE, SWING, PROBABILITY, HOLD, ORDER, TARGET | **owner: ARP is learn-only** |
| SEQ and key (6) | LENGTH, DIVISION, SWING, GATE, QUANTIZE, TRANSPOSE | setup |
| TWO-PART (4, G) | BALANCE (level between part 1 and part 2), XFADE, SPLIT NOTE, MODE | **owner, second round:** settings for both parts together get no CC on a part's channel |
| MORPH (5) | UNDO (P); TARGET, SMART/WILD, ARP, PITCH (G) | **owner, second round:** UNDO and the four morph setting switches are learn-only (fader 70, ROLL 102 and AMOUNT 103 stay direct) |
| GLOBAL (15, G) | SWING, TUNE, DELAY MODE, DELAY MOD RATE, DELAY MOD DEPTH, PING-PONG SPREAD, REVERB TYPE, SHIMMER PITCH, SHIMMER MOD, COMPRESSOR THRESHOLD, RATIO, ATTACK, RELEASE, WIDTH, BASS MONO | set once; TEMPO and the clock source get no CC at all, they follow MIDI clock |

Total 2 + 5 + 5 + 8 + 10 + 24 + 7 + 1 + 7 + 9 + 6 + 4 + 5 + 15 = 108.

The 32 UNUSED numbers are the learn pool: 8 (the standard Balance, no DUELL match), 33, 37, 39, 40, 42, 43 (fine halves of the numbers DUELL itself uses), 56 to 63 (free since the LFO rows left), 65 to 69 (pedals and switches DUELL does not have), 84 and 88 (need a note number / a velocity extension), 104 to 113 (free since the second round).

### 3.4 The count, checked by a script

| | Count | Numbers |
| --- | --- | --- |
| DIRECT, STANDARD, matches exactly | 13 | 5, 7, 10, 70, 71, 72, 73, 74, 75, 79, 91, 93, 94 |
| DIRECT, VIRUS-LIKE (free numbers) | 34 | 14 to 31 (18), 80 to 83, 85 to 87, 89, 90 (9), 102, 103, 114 to 118 (7) |
| DIRECT, EXCEPTION | 23 | X2: 92. X3: 76, 77, 78. X4: 34, 35, 36, 41, 44 to 55 (16). X5: 12, 13, 95 |
| **DIRECT total** | **70** | |
| INPUT | 5 | 1, 2, 4, 11, 64 |
| LOCKED | 18 | 0, 6, 32, 38, 96 to 101, 120 to 127 |
| UNUSED | 32 | see 3.3 |
| RESERVED | 3 | 3, 9, 119 |
| **Sum** | **128** | 70 + 5 + 18 + 32 + 3 |
| Direct without any exception | 47 | 13 + 34 |
| Direct without X4 | 54 | |
| Learn-only parameters (not numbers) | about 108 | 3.3 |

**The 20 new values, all direct (CHECKED):** SUB LEVEL 14, SUB OCTAVE 15, SUB WAVE 16, SUB HARMONICS 17, CUTOFF 2 21, RES 2 22, ENV 2 to CUTOFF 2 (F2 ENV AMT) 23, KEY TRK 2 24, FILTER 2 TYPE 25, DRIVE 26, BLEND 27, ROUTE 28, LINK 29, LFO 1 to CUTOFF 2 30, LFO 2 to CUTOFF 2 31, CYCLE MODE 114, RISE 115, FALL 116, CURVE 117, SYNC 118. The owner's earlier "five" (CUTOFF 2, RES 2, SUB LEVEL, CYCLE RISE, CYCLE FALL) are all on numbers that depend on no exception.

**Morph and roll (CHECKED, second round):** MORPH FADER 70 (standard Sound Variation), ROLL 102 (one CC, centred: 0-42 End A, 43-84 nothing, 85-127 End B), AMOUNT 103. UNDO and the four morph settings (TARGET, SMART/WILD, ARP, PITCH) are learn-only. CC1 (mod wheel) still moves the fader too, directly and not through the matrix (X6, section 2b).

**Duplicate check:** a script (Python) reads the table of section 3.2 from this file, one entry per number. It stops if a number is missing or entered twice, it checks that no parameter name appears on two numbers, and it counts the states. Result after the second round: 128 numbers each once, 0 duplicate parameters, the 20 new values on distinct direct numbers, counts as in the table above (70 direct, 5 input, 18 locked, 32 unused, 3 reserved).

### 3.5 Rules

- **Taken number, ask first (the owner's rule).** Typing or learning a number that already moves something shows the owner and asks, default no: `CC92 IS DELAY SEND. TAKE IT? OCT+ YES OCT- NO`. If you say yes, the old parameter loses its number (it becomes learn-only, shown CLEARED); nothing else moves. A number with a standard meaning that DUELL does not use asks the same way with the standard name: `CC66 IS SOSTENUTO. TAKE IT?`. Input and locked numbers are refused outright (`CC NOT LEARNABLE`); that list gains **2 and 4** because they are inputs now. The learn of a two-part setting (BALANCE, XFADE, SPLIT NOTE, MODE) is still allowed; it is the owner's own choice, not a shipped number. The editor asks the same question. RESET MAP puts the shipped map back, after a confirm.
- **Standard names win over order.** Filter 1 keeps CUTOFF 74 and RESONANCE 71 (Virus order would put them at 40 and 42).
- **No page tricks.** One number never means two things depending on a mode (a control does only what its label says).
- **Moves against today's numbers** (the owner allows everything to change before the alpha). Direct CCs that move: MORPH FADER 20 to 70, ROLL A 21 and ROLL B 22 merge into one ROLL on 102, AMOUNT 25 to 103, DELAY send 94 to 92 (94 becomes DETUNE). Direct CCs that disappear (learn-only from now): UNDO (today 23), TARGET (111), SMART (113), ARP (114), PITCH (115), MODE (112), BALANCE (116), XFADE (117), SPLIT NOTE (118). **Careful: CC111 to CC115 and CC94 change meaning or become unused or different** (CC111, 112, 113 become unused; CC114 and 115 become CYCLE MODE and RISE; CC116 to 118 become CYCLE FALL, CURVE and SYNC; CC94 becomes DETUNE); any controller template made for the old numbers must be redone. Unchanged: 1, 5, 7, 10, 11, 64, 71 to 75, 91, 93.
- **Why the morph numbers move at all:** today's 20 to 25 and 111 to 118 are all on numbers the standard leaves undefined, so the standard-name rule does not force a move. The fader moves to the standard Sound Variation (CC70) by the owner's choice; ROLL and AMOUNT move to 102 and 103 so that the Virus-like filter run (14 to 31) stays whole and the 20 to 25 numbers are not left half used.

---

## 4. What the Akai Force and the MPC show (exact names)

### 4.1 What I found

| Finding | Mark | Source |
| --- | --- | --- |
| The MPC's per-track **MIDI CC Filter** names these numbers: CC1 Modulation, CC2 Breath, CC3 Undefined, CC4 Foot, CC5 Portamento, CC7 Volume, CC10 Pan, CC11 Expression, CC65 Pmento (Portamento) On/Off, CC66 Sost (Sostenuto) On/Off, CC67 Soft Ped (Soft Pedal) On/Off, CC68 Legato Switch, CC64 Sustain On/Off (plus non-CC items: CC128 Pitchbend, CC129 Channel Pressure, CC130 Program Change, CC131 Aftertouch). 13 CC numbers. | CONFIRMED (printed text) | MPC Standalone OS User Guide v3.9 and v3.7, Track View, "MIDI CC Filter" (cdn.inmusicbrands.com) |
| The Preferences > MIDI list of the MPC Key writes the mod wheel's default as "CC 001: Modulation" (and the touch strip text says "MIDI CC1 modulation", "MIDI CC11 expression"). | CONFIRMED | MPC Standalone OS User Guide v3.9, Pedal Settings and Touch Strip |
| For a **MIDI track** the Track Edit page is "an overview of all available MIDI CCs with a slider for each", six tabs; you pick a CC per control from a drop-down or "a list of assignments" and can rename it; the macro menu says "MIDI Track: Edit: Standard MIDI control change assignments". The manuals do **not** print the names in that list. | CONFIRMED (the text); the names in the list: not documented | MPC Standalone OS User Guide v3.9, MIDI Tracks and Q-Link Edit; Force User Guide v1.0, MIDI Tracks and Track Knobs ("MIDI CC (MIDI tracks only): standard MIDI CC parameters") |
| Force firmware 3.1.1: from MIDI Track > Track Edit you can edit the continuous controller **and a string** (label) for each control, saved with the track and reloadable in any project. | CONFIRMED | Force 3.1.1 release notes |
| Force 3.0.5: "MIDI Tracks now have level (CC7) and pan (CC10) controls." | CONFIRMED | Force 3.0.5 release notes |
| MPC 3.7: "All values for MIDI CC parameters are now available to enter" (List Edit). | CONFIRMED | MPC 3.7 release notes |
| The Force and the MPC standalone share the firmware family (owner: "the Force is the same as the MPC"). I read the MPC 3.x guides and the Force guide v1.0 and 3.x release notes; I did **not** find a Force 3.x user guide with the "MIDI CC Filter" list. | HYPOTHESIS that the Force prints the same 13 names | |
| Forum reports (loading an external MIDI program pre-labels Q-Links "CC1 Modulation, CC2 Breath, CC3" and such labels are "a universal map ... often not right for a given synth"; the Force MIDI-mode Q-Links come preset to named controllers). | HYPOTHESIS: read only as search-result summaries; mpc-forums.com, gearspace.com and modwiggler.com refused my requests (403) | |
| Names for the other numbers (8, 12, 13, 70 to 79, 91 to 95 ...) in the Track Edit drop-down. | **not found**; HYPOTHESIS: a plain "CCnn" or the MIDI Association's wording | |

The owner's own observation ("I scrolled through a list of 128 and some have names already") fits: only a small set is named.

**To close the open part, once:** open the Force's CC list and read the names it shows at 8, 12, 71, 74, 91 and 94; tell me, and the "Force/MPC shows" column of the editor gets them. Until then those cells stay a dash.

### 4.2 Comparison with the names used in version 2

| CC | MIDI Association name (version 2) | MPC/Force prints | Difference | DUELL parameter | Proposed name in the MIDI MAP |
| --- | --- | --- | --- | --- | --- |
| 1 | Modulation Wheel or Lever | Modulation (also "CC 001: Modulation") | wording | MOD WHEEL (matrix source MODW) | MOD WHEEL, standard line "MODULATION" |
| 2 | Breath Controller | Breath | wording | BREATH (new source) | **BREATH**, the same word as the Force |
| 3 | Undefined | Undefined | none | reserved | reserved (the Force shows it as Undefined too) |
| 4 | Foot Controller | Foot | wording | FOOT (new source) | **FOOT**, the same word as the Force |
| 5 | Portamento Time | Portamento | "Time" missing | GLIDE | GLIDE, footer `STD: PORTAMENTO TIME`, editor column `PORTAMENTO` |
| 7 | Channel Volume | Volume | "Channel" missing | LEVEL (mixer level of the part) | LEVEL, `STD: VOLUME`. Fits the Force: its MIDI track Level slider sends CC7 |
| 10 | Pan | Pan | none | PAN | PAN. Fits: the MIDI track Pan sends CC10 |
| 11 | Expression Controller | Expression | wording | EXPRESSION (matrix source EXPR) | EXPRESSION |
| 64 | Damper Pedal (Sustain) | Sustain On/Off | wording | SUSTAIN pedal (input) | SUSTAIN |
| 65 | Portamento On/Off | Pmento On/Off | abbreviated | not used | UNUSED |
| 66 | Sostenuto | Sost On/Off | abbreviated | not used | UNUSED |
| 67 | Soft Pedal | Soft Ped On/Off | abbreviated | not used | UNUSED |
| 68 | Legato Footswitch | Legato Switch | "Footswitch" becomes "Switch" | not used | UNUSED |

**Differences in meaning: none.** Every difference is wording or abbreviation. **Proposal:** do not rename DUELL's panel words (GLIDE, LEVEL ...): the unit's label rule and the no-wording-changes rule say the labels stay. The MIDI MAP shows the DUELL name first and the standard word next to it (section 6), so on the Force you look for "Portamento" and read GLIDE. The two new sources take the Force's own words, BREATH and FOOT.

**For the numbers the Force may not name** (8, 12, 13, 71 to 79, 91 to 95), the MIDI MAP shows the MIDI Association's word (for example `STD: BRIGHTNESS` on CC74, `STD: EFFECTS 1 DEPTH (REVERB SEND)` on CC91). The example from the ticket: the standard calls CC74 "Brightness"; DUELL's parameter is CUTOFF; the MIDI MAP row reads `CC74 CUTOFF   STD: BRIGHTNESS`. A name list of the Force's own for these numbers would only be a guess today (HYPOTHESIS), so the map does not claim one.

**What this means for the exceptions:** if the Force names only 13 numbers, then 12, 13, 76 to 78, 92, 95 (the soft matches) are not visible as "wrong names" on the Force at all. They still matter for DAWs and other controllers that use the MIDI Association's meanings, which is why the exceptions stay listed and approved one by one.

---

## 5. BREATH (CC2) and FOOT (CC4) as matrix sources

The owner said yes (FEATURE_LIST item 8) and confirmed it in the second round. Made exactly like CC11 (EXPRESSION). Facts read in the firmware (mod.c, midi_control.c, params.c, core.h of the newest line):

- **Source ids.** The matrix has 15 sources today, ids 0 to 14 (OFF, LFO1, AENV, VEL, KEY, RAND, MODW, AT, EXPR, S&H, SLEW, ENV2, LFO2, MORPH, DUCK). The list is append-only (stored patches keep their meaning) and the plan gives CYCLE id 15. So **BREATH = id 16 and FOOT = id 17** (CYCLE first, as planned; if BREATH and FOOT are made first they take 15 and 16 and CYCLE moves to 17: ids follow the order of making). The list then has 18 of 128 places.
- **What each does.** BREATH is the value of CC2 and FOOT of CC4 on the channel of the part, 0 to 127 scaled to the matrix range 0 to 1, same as MODW (CC1). It is unipolar. Nothing is stored in patches except the source id (one byte, as now).
- **Default.** With no controller connected the value is 0 (like MODW), so a missing breath controller cannot open a filter by itself. CC121 (Reset All Controllers) sets both to 0. EXPRESSION keeps its own MIDI rule (full until a CC11 arrives).
- **Part rule.** As CC1 and CC11 today: the part that the channel plays; in LAYER both parts receive it (the code that feeds CC1, CC11 and aftertouch to part 2 in LAYER also feeds these).
- **They do not move the morph fader yet.** Only CC1 does (X6, directly, not through the matrix). BREATH and FOOT would reach the morph only through the matrix, which has MORPH as a source but no morph destination today. The owner decided (2026-10-20, FEATURE_LIST item 9) that MORPH becomes a matrix destination in the next edition; then breath, foot, an LFO and CYCLE can move the morph fader through a slot. No change to the CC map.
- **How they appear.** In the unit's matrix source list the names are **BREATH** and **FOOT**, placed in the knob order right after EXPR (the MIDI controllers together: MODW, AT, EXPR, BREATH, FOOT, then S&H, SLEW, MORPH, DUCK). In the editor the labels file gets two entries: BREATH = "BREATH CONTROLLER", FOOT = "FOOT CONTROLLER". The MIDI MAP shows rows 2 and 4 as INPUT lines with the text "BREATH (matrix source)" / "FOOT (matrix source)", not editable, like 1 and 11.
- **Hide-inert rules.** A source is a choice in a list, not a control, so nothing is hidden or faded: BREATH and FOOT are always selectable. A slot that uses a source nobody sends is simply 0, exactly as MODW without a wheel (no silent control in the honesty-rule sense: the source is named on the slot). Hide rules that stay as today: a slot whose destination is hidden (for example DRIVE while filter 2 is OFF) keeps hiding the same way, whichever source it uses. The learn refuses 2 and 4.
- **Cost (est.).** Firmware about +150 B of program (two cases in the source reader, two in the MIDI input), +16 B of names; RAM +2 B per part (the part record is padded to a multiple of 16 B, so 0 to 64 B in all); hot RAM 0 expected (HYPOTHESIS: the matrix reader's placement was not checked); CPU nothing unless a slot uses them. Editor: two labels. About **1 round** including the automatic check (CC2 and CC4 reach the source, reset on CC121, LAYER feeds both parts).

---

## 6. What the unit and the editor show (changes against version 2, sections 5.1 and 5.2)

**On the unit (MIDI MAP page, layout still for the owner to approve before it is made):**
- Footer for any number gets the standard word: `CC5 GLIDE  STANDARD  STD: PORTAMENTO TIME`, `CC74 CUTOFF  STANDARD  STD: BRIGHTNESS`, `CC70 MORPH  STANDARD  STD: SOUND VARIATION`, for an undefined number `STD: UNDEFINED`.
- A view EXCEPTIONS (rows tagged X2 to X7), BY PARAMETER, BY CC, FREE CCS, ASSIGNED, as version 2.
- The unit does **not** carry the Force/MPC names (little room; the editor does).

**In the editor (MIDI MAP table):**

| Column | Content |
| --- | --- |
| Section | the Virus-like group |
| Parameter | the unit's name; engine values show the current engine's label |
| CC | editable number or NONE |
| **STANDARD NAME** | the MIDI Association's name of that number, all 128 known, also for numbers with no DUELL row: "Sound Controller 5 (Brightness)" |
| **FORCE/MPC SHOWS** | the name the MPC/Force prints, for the 13 numbers confirmed in section 4; a dash for the rest (editable by the owner once he has read his Force) |
| SOURCE | STANDARD, VIRUS-LIKE, EXCEPTION X2 to X5, LEARN-ONLY, YOURS, LEARNED |
| Part, Kind, In, Saved | as version 1 |

- Filters: only exceptions, only standard, only learn-only, changed from the shipped map, by section; sort by CC or by section.
- A second table, **BY NUMBER**: 0 to 127 with the standard name, the Force/MPC name where confirmed, and what DUELL does with the number (parameter, input, locked, unused, reserved). It answers "what is CC 66 here?" on screen.
- **Dropped from version 2:** `duell-cc-list.csv`, the printable list and the five-page Force sheet. Kept: the `midi-map.json` export and import of version 1; it is a backup of the map, not a sheet.
- The Force side is typed by hand when wanted (CONFIRMED way: edit the number and the label per control in MIDI Track > Track Edit, saved with the track); no file is made for it.

---

## 7. Reused open-source pieces

Rule from the owner (memory "reuse open source"): take tested pieces with a licence that fits GPL-3.0, give credit, and tell him before copying a lot from a small project. Searched today: CC name tables, MIDI libraries, synth firmwares.

| Piece | Licence (how checked) | Fits GPL-3.0? | What it gives | Use |
| --- | --- | --- | --- | --- |
| **MIDI Association, "MIDI 1.0 Control Change Messages"** (midi.org) | the standard itself; I take numbers and names only, no prose | n/a | the official 128 names and default meanings | the base of the STANDARD NAME column; cited |
| **pencilresearch/midi, `MIDI Association/Generic MIDI 1.0.csv`** (the data of midi.guide) | **CC-BY-SA-4.0** (LICENSE file read; the README says device parts may belong to their makers; this file is the generic standard) | yes: Creative Commons states that BY-SA 4.0 adaptations may be released under GPLv3 (one way only); needs credit | 55 rows, every standard-named number as a small CSV (numbers match my table, no extra numbers) | a ready machine-readable list to generate the editor's name list and to cross-check it by script; no retyping. Credit in NOTICE.md. Small data file, so I only tell the owner here |
| **pencilresearch/midi, `Access/Virus TI.csv`** | CC-BY-SA-4.0 (same) | yes (same) | the Virus TI CC layout, 126 lines, as data | a second source to cross-check the Virus order I read from the owner's PDF. It agrees in section order; it differs by one number in the chorus run (CSV: 104 chorus mix, 105 classic mix; my version-2 note: 105 mix). DUELL takes only the order, so nothing changes. Not copied |
| **WebMidi.js, `src/Enumerations.js`** (djipco) | **Apache-2.0** (API license check and LICENSE.txt read) | yes (Apache-2.0 into GPL-3.0 is allowed) | all CC names as JavaScript identifiers ("breathcontrollercoarse", "effect1depth") | not needed: identifiers, not display names, and a coarse/fine split. Mentioned so it is not searched again |
| **JUCE `MidiMessage::getControllerName`** | the file header offers **AGPLv3** or the JUCE commercial licence (header read) | heavy, and not wanted | a name list with old names ("Tremolo Level", "Celeste Level", "Sound Control 6") | not used |
| Synth firmwares with a CC table | searched (MiniDexed, mbed and Arduino synth headers, others) | - | none found with a table that has a fitting licence file; one found forbids redistribution ("personal use only") | **none found** |
| The Force/MPC names | Akai manuals (read, not copied) | n/a | the 13 names of section 4 | 13 short strings typed in the editor, source named |

**Recommendation:** use the pencilresearch generic CSV (credited in NOTICE.md) to produce the editor's 128-name list instead of typing it, and add the 14 "default meaning" words (Brightness, Release Time ...) from midi.org on top. The unit's 61 names are short strings taken from the same list. Not a lawyer's opinion; the licence line is what Creative Commons itself publishes.

---

## 8. Cost deltas against version 2

| Item | Version 2 | Final | Delta |
| --- | --- | --- | --- |
| Direct rows | 88 | 70 | -18 (8 LFO rows; second round: BALANCE, UNDO, ROLL B merged, 4 morph switches, MODE, XFADE, SPLIT NOTE, net of the fader's move to CC70) |
| Exception X4 (fine-half numbers used) | 24 | 16 | -8: fewer clashes with a 14-bit controller |
| Free numbers (unused + reserved) | 19 | 35 | +16 (+8 LFO numbers 56 to 63, -2 for the new inputs 2 and 4, +10 from the second round: CC8 and 104 to 113 freed, CC70 now used) |
| Stored (settings record) | 258 B | 258 B | 0 (the table is the same size) |
| Shipped image in flash | 256 B | 256 B | 0 |
| Standard names on the unit | about 0.8 to 1 KB | same | 0 |
| Standard names in the editor | about 3 KB, no firmware cost | same, now generated from the CSV above | 0; plus 13 Force/MPC strings (under 0.3 KB, editor only) |
| CSV, printable list, Force sheet | in version 2 | dropped | about -0.5 KB editor code and about -0.5 round |
| Firmware, ROLL as one CC | two CCs (today 21 and 22) | one CC with value halves | small change in `midi_control.c` (the morph CC handler), about 0.3 round with the automatic check; the morph destination in the matrix is a separate item (FEATURE_LIST 9), not costed here |
| Matrix sources BREATH, FOOT | not in version 2 | +150 B program, +16 B names, +2 B RAM per part (0 to 64 B), hot RAM 0 expected | new, section 5 |
| CPU | one table read per CC | same | 0 |
| Work | map 7 to 9 rounds | map 6.5 to 8.5 rounds, plus about 1 round for BREATH and FOOT | net about +0.5 to +1 |
| Cheap first step | editor-only read-only table of today's CCs, beside the proposed map | same | 0 |

Risks added or changed: (1) **CC94 and CC111 to CC118 change meaning or become unused** against today (section 3.5): old controller templates must be redone, allowed before the alpha. (2) The 70 shipped lines are 70 places to be wrong: the automatic check walks every line (every number once, every target exists, every STANDARD row equals the MIDI Association's name). (3) The Force/MPC names beyond the 13 are still unknown (section 4). (4) A refused exception leaves a hole in its group; numbers are not renumbered. Version 1's risks 1 to 8 still hold.

---

## 9. Open questions: none

Every question of the first round is answered (2026-10-20, section 2b). Nothing is waiting for the owner. Two small things stay as notes, not questions: a pad that always sends 127 can only roll End B with the single ROLL CC (a knob or fader range is needed for End A), and the Force's own names for numbers other than the 13 confirmed ones are still unknown (section 4).

---

## Evidence list

| Claim | Source |
| --- | --- |
| The MPC MIDI CC Filter names (CC1 to CC68, CC128 to CC131) | MPC Standalone OS User Guide v3.9: https://cdn.inmusicbrands.com/Software/15JM26PSBC/MPC%20Standalone%20OS%20-%20User%20Guide%20-%20v3.9.pdf (Track View, Track MIDI Perform Settings); the same list in v3.7: https://cdn.inmusicbrands.com/Software/37/MPC%20Standalone%20OS%20-%20User%20Guide%20-%20v3.7.pdf. Text read today |
| "CC 001: Modulation", the touch strip texts, "Standard MIDI control change assignments", "overview of all available MIDI CCs" | the same v3.9 guide (Preferences, Touch Strip, Q-Link Edit, MIDI Tracks) |
| Force: "MIDI CC (MIDI tracks only): standard MIDI CC parameters" | Force User Guide v1.0: http://akai-pro.jp/force/data/Force-UserGuide-v1.0c.pdf |
| Force 3.1.1: edit the CC and string per control, saved with the track | https://cdn.inmusicbrands.com/akai/Force/eO8S4/Force3.1.1FirmwareUpdate.pdf |
| Force 3.0.5: MIDI tracks level (CC7) and pan (CC10) | https://cdn.inmusicbrands.com/akai/Force/Force-3_0_5ReleaseNotes.pdf |
| MPC 3.7: all values for MIDI CC parameters can be entered | https://cdn.inmusicbrands.com/Software/37/MPC%203.7%20Update%20Release%20Notes.pdf |
| Forum reports on Q-Link names and the 128-line list | mpc-forums.com and gearspace.com threads as shown in search results; the pages returned 403: HYPOTHESIS |
| Standard CC names and defaults | https://midi.org/midi-1-0-control-change-messages (as in version 2) |
| Generic MIDI 1.0 CSV, Virus TI CSV and their licence (CC-BY-SA-4.0) | https://github.com/pencilresearch/midi (LICENSE, `MIDI Association/Generic MIDI 1.0.csv`, `Access/Virus TI.csv`); viewer https://midi.guide |
| CC BY-SA 4.0 adaptations may be licensed under GPLv3, one way | https://creativecommons.org/share-your-work/licensing-considerations/compatible-licenses/ |
| WebMidi.js Apache-2.0 and its CC enumeration | https://github.com/djipco/webmidi (LICENSE.txt, src/Enumerations.js) |
| JUCE `getControllerName` and its licence header | https://github.com/juce-framework/JUCE (modules/juce_audio_basics/midi/juce_MidiMessage.cpp) |
| Matrix sources 0 to 14, `mod_midi` (CC1, CC11, CC121), part rule in LAYER, source name and knob-order tables, track record padded to 16 B | `felucca-base/firmware/src` (`mod.c`, `midi_control.c`, `params.c`, `core.h`); editor labels in `web/editor/labels.js` |
| CYCLE as source id 15, source list 15 to 16 | EXTENSION_BLOCK_PLAN.md section 3.4 |
| Today's CC list, learn, table and unit page | MIDI_MAP_DESIGN.md |
| The proposed map, exceptions X1 to X8, the Virus layout, the 20 new values | MIDI_MAP_DESIGN_2.md |
| The owner's answers | FEATURE_LIST.md items 7, 8 and 9; memory file project-headline-features-direction; the second round of 2026-10-20 in section 2b |
| CC1 moves the morph fader directly (not through the matrix); the morph CC handler; the morph queue `MQ_FADER`; MORPH not a destination | `felucca-base/firmware/src` (`midi_control.c` CC1 case and `midi_morph`, `morph.c` `morph_push`, `mod.c` header comment), read only |
| Counts and duplicate check | a small script run today (table held as one entry per number; counts in 3.4) |
