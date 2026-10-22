# SUB LAYER, CYCLING ENVELOPE and WAVE STEPS: design study (2026-10-19)

Private. Research and design only: no code was changed, no branch was made, INDEX.md was not touched. I wrote and retuned no sounds. Every page, label and knob below is a proposal for you to approve, not a decision.

Marks (as in the TWIN FILTER studies):
- **CONFIRMED**: I read it today in the code (`wt/join0233/felucca-base/firmware/src`) or in the named document.
- **HYPOTHESIS**: my reading, general knowledge about other synths, or a forum source.
- **est.**: worked out by me, not measured.
- **measured**: a DUELL number from the computer tests or your unit.
- CPU in **points**: 1 point = 1 % of one core's time. 33 host instructions a sample = 1 point (CPU_PLAN). A note costs about 5 to 8 points today, PHYS about 16.

New words (not yet in GLOSSARY.md or the TWIN FILTER studies):
- **Sub oscillator**: an extra plain oscillator that plays the note one or two octaves lower, under the main sound, for weight in the bass.
- **Harmonic / overtone**: the tones above a note's pitch that give it its colour. The 2nd harmonic is an octave up, the 3rd an octave and a fifth, the 4th two octaves.
- **Band-limited**: an oscillator made so that it does not create aliasing (the harsh extra tones of design 1). **PolyBLEP** is the small correction DUELL already uses for that on saw and pulse waves.
- **Phase**: where in its cycle an oscillator is. Two waves that start their cycles together are "in phase".
- **Function generator**: the Eurorack module (for example Make Noise MATHS) that rises and falls once when triggered, or over and over when set to cycle. It is the classic cycling envelope.
- **Rise / fall / curve**: the time it takes to go up, the time to come down, and how bent the line is (straight, or fast-then-slow like a plucked string's decay).
- **Per voice / per part**: one copy for every sounding note, or one shared copy for the whole part. DUELL's LFOs are per part; its envelopes are per voice.
- **Quantiser**: something that rounds a moving value to the nearest of a few fixed places, so it moves in steps.
- **Hysteresis**: a small dead zone that stops a quantiser from flickering between two steps when the value sits right on the edge.
- **Extension block**: the planned extra section of the patch record (HEADLINE_FEATURES.md stage A), holding values that do not fit the 127 parameter ids.

---

## Verdict (half a page)

**A. SUB LAYER.** A plain sub oscillator on every note of every engine. It is mixed into the note before filter 1, so the TWIN chain (filter 1, DRIVE, filter 2) shapes it the classic way. Four controls: WAVE (SINE, TRI, SQUARE, PULSE), OCTAVE (-2, -1, 0), HARMONICS (adds the sub's own octave overtones so it is heard on small speakers) and LEVEL. It follows everything the note does (glide, bend, vibrato, the PITCH routes, master tune), because it reads the same pitch the engine gets once per block.
- **What you get:** a solid low end under FM6, WAVE, VOICE, PHASE and NOISE sounds, Juno/SH-101-style square subs under ANALOG, without giving up an engine oscillator.
- **Cost:** about 0.7 points a note for a SINE sub. That number is calibrated on WAVE's own SINE sub, measured at +22 host instructions a sample. A SQUARE sub costs about 1 point, and full HARMONICS on a SQUARE about 1.8 (est.). A part that plays 8 notes then plays about 7. **Your worry is the other way round:** the sub costs the same on every engine, so it hurts the lightest engine (LOFI) most and PHYS least (+4 %). No engine needs to be excluded, and the NOTES counter shows the cost honestly. In UNISON there is one sub per key, not one per detuned voice.
- **Main risk:** core 1's code RAM. The sub should ride on TWIN's new voice chain instead of adding its own branch to the hot block, so it is best built right after TWIN step 1. FM6 needs a small extra piece: its notes follow their own operator envelopes, not the AMP ENV.

**B. CYCLING ENVELOPE ("CYCLE").** A new, separate per-note generator: it rises, falls and starts again while the note sounds, as on a Eurorack function generator. Four controls: MODE (LOOP or ONCE), RISE, FALL and CURVE. With ENV SYNC on, RISE and FALL become note values. It is a new matrix source, CYCLE.
- **What you get beyond the LFOs:** every note of a chord pulses from its own key-down (the LFOs are per part, CONFIRMED). Rise and fall are independent (lopsided shapes). CURVE gives percussive, bouncing decays. ONCE gives you a third AD envelope.
- **Honest overlap:** with straight lines and equal times on a mono lead, it is a triangle LFO with TRIG NOTE.
- **Cost:** about 0.02 to 0.05 points a note, and only while a matrix slot reads it. 8 B a note, 4 values in the extension block, and no hot-RAM bytes if it is ticked inside the matrix's voice step.
- **Main risk:** it needs the extension block, like TWIN does. Plan B is Clipper's way: a LOOP switch on ENV 2. That is cheaper, but it turns your filter envelope into the loop.

**C. WAVE STEPS (your addition).** One new control on WAVE's empty EDIT 1 card: STEPS (OFF, 2 to 19). It rounds POS to that many places, so every movement of POS (ENV 2, an LFO, CYCLE, the matrix, RAND per note) becomes a staircase, the Microwave/PPG sound. Each step is cross-faded over one or two blocks (0.7 to 1.5 ms), so it has no click. Steps per beat come from a synced LFO; steps per note come from RAND or KEY.
- **Cost:** practically nothing: about 0.01 points a note, 0 bytes of note memory, no new id and no format change (it uses WAVE's unused slot E7).
- **Main risk:** none to speak of. Flicker at a step edge is handled by hysteresis.

---

## A. SUB LAYER

### A1. Where the sub goes in the voice

**How the pitch reaches the engines today (CONFIRMED, `voice.c` `track_render`, lines ~1132-1158; `mod.c` `mod_voice`):**
- Each note has `pitch16` (the key, in 1/16 semitones) and `pitch_cur` (the same with glide: RATE or TIME mode, `glide_set`).
- Once a block (32 samples), `track_render` builds the note's pitch: `pitch_cur` + master tune + pitch bend (smoothed, `midi_bend_q8`) + LFO 1 PIT + AMP DEST PIT + LFO 2 PIT. It turns that into a phase step `m.inc` with a fine factor `m.fine` (the unison detune `v->fine`, the rest of the tune and the bend).
- Then `mod_voice` adds the matrix's PITCH routes and recomputes `m.inc` (CONFIRMED, `mod.c` lines 555-560).
- The engine receives all of this as one `vmod_t m`.
- **So `m.inc` after `mod_voice` already contains every pitch movement:** glide, bend, vibrato, AMP DEST / LFO / matrix PITCH, master tune and unison detune.
- **Microtuning:** none in DUELL. A search finds only a comment that msfa's microtuning was removed from FM6. The global SCALE moves note numbers before they reach the voice (SPEC 1.x, "the step sequencer snaps a step's notes"), so the sub follows it for free (HYPOTHESIS that nothing re-tunes after the voice).
- **ARP TARGET** moves only the engine's masked units (`m.amask`, `m.adelta`). The sub stays on the held root, which is what a bass under an arp wants.

**A separate oscillator can share it exactly.** The sub's phase step is `m.inc >> 1` for -1 octave and `m.inc >> 2` for -2. That costs nothing per sample, and it is what WAVE's own SUB already does: `ph2 += inc >> 1` (CONFIRMED, `eng_wave.c` `wave_render`, line 892).

**Unison:** `mono_play` gives each of up to 8 voices a detune `v->fine` from -7 to +7 steps. There is no undetuned voice when the count is even (CONFIRMED, `voice.c` lines 488-491).
- Proposal: in UNISON only voice 0 plays the sub, with the detune removed. Its step is recomputed once a block from `m.pitch16` and the tune and bend fine factors, without `v->fine` (one table lookup a block).
- So one key gives one centred, undetuned sub. Eight detuned subs would smear the low end and cost 8 times as much.
- This is where your idea "only when not polyphonic" fits best: UNISON is the one place where a sub per voice would be wrong.

**Before or after filter 1: before (recommended).**
- In the TWIN chain of design 2 (engine at full level, filter 1, DRIVE, filter 2, then the amp), the sub is added to the engine's block right after the engine renders. So it goes through both filters and the DRIVE.
- That is the analog-mixer position of the Virus, the Juno and the SH-101. A closing filter darkens the sub too, and the DRIVE turns a sine sub into a growl together with the note.
- The cost of that position: at high RES the LADDER thins the bass (design 1). A "direct" option, where the sub joins after filter 2, can come later as one more value if you miss a firm low end. It is not in the first version.

**Its envelope: the part's AMP ENV, by sharing the note's volume ramp.**
- With the chain before the amp, the sub is added at full level and the one amp step after filter 2 shapes both the note and the sub. The sub then follows the AMP ENV, velocity, LFO AMP, the matrix's AMP, the LAYER XFADE and the steal fade-out, all for free.
- If TWIN is not built yet (today's order: the engine applies the amp itself), the sub applies the same ramp itself (`voice_amp`, as WAVE's SUB does).
- **FM6 is the exception (CONFIRMED, `eng_fm6.c` `.ownenv = 1`; `voice.c` lines 1107-1116).**
  - FM6's operators are the amplitude. The AMP ENV is forced to full and its cards are hidden on FM6 (`ui_draw.c` line 770).
  - A sub on FM6 would therefore hold full level until the voice ends, and then stop with a click.
  - Proposal: a tiny engine hook that reports the loudest carrier's level once a block. `fm6_note_t` keeps each operator's `gain_out` (CONFIRMED, `fm6_core.c` `fm6_note_done`). The sub then follows FM6's own envelopes.
  - That it is a clean linear level is HYPOTHESIS. Plan B: no SUB page on FM6.

**Note-off, retrigger, stealing (CONFIRMED, `voice_start`, `env_tick`, `voice_kill`):**
- **Note-off:** the release is the AMP ENV's release (or FM6's), so the sub ends with the note.
- **Retrigger in place:** `voice_start` keeps the oscillator phases of a sounding voice so it does not click. The sub's phase must be kept the same way.
- **New note:** the sub starts at phase 0 (a sine starts at its zero crossing), which gives the same punch on every note. The amp ramp starts from 0 on a fresh voice, so a SQUARE that starts high does not click either.
- **Stealing:** a voice taken for another part fades over one block (stage 4). The sub is inside the same ramp.
- **Silent sustain:** a held note at SUS 0 is skipped (line 1139), and its sub is skipped with it.

**The voice pool and NOTES:**
- The sub is part of the note, so it takes no voice slot of its own.
- `budget.c` adds a cost row for it (section A3).
- In UNISON it is counted once per part, not once per voice.

**State per note slot (est.):**
- the sub phase, 4 B;
- the gliding level (LEVEL turned or modulated without zipper noise), 2 B;
- the last WAVE and OCTAVE, for a cross-fade when they switch, 1 B;
- padding, 1 B.

That is **8 B a slot**: 32 slots (4 tracks x 8) = 256 B of variable RAM. Design 2 leaves about 0.95 KB of the 2.1 KB headroom once TWIN's 1.15 KB is in (est.), so it fits.

### A2. The design

**OCTAVE: -2, -1, 0 (recommended).**
- -1 and -2 are the classic sub positions: the Juno's square at -1, the SH-101's square at -1 and -2 and its pulse at -2 (HYPOTHESIS, general knowledge).
- **0** is not a sub but a "body": a pure SINE or TRI at the note's own pitch under thin sounds (FM6 bells, VOICE, NOISE, high WAVE frames). The same oscillator does it for free.
- **-3 is left out:** from C3 down it falls under 20 Hz and only moves the speaker.
- **+1 is left out:** an octave up is what HARMONICS gives.

**WAVE: SINE, TRI, SQUARE, PULSE (25 %).**

| Wave | Sound | Band-limiting |
|---|---|---|
| SINE | the modern 808-style weight, invisible in a mix | none needed (`sine_i`, CONFIRMED `dsp.c`) |
| TRI | almost a sine, a touch of edge | `osc_tri` is plain (not corrected). Its overtones fall steeply, so at sub pitches it needs no correction; at OCTAVE 0 on top notes a faint alias is possible (est.) |
| SQUARE | the Juno/101 hollow, woody sub | `osc_pulse` with polyBLEP (CONFIRMED `dsp.c`). The correction's divide runs only on the 2 samples a cycle at the edges, so it is negligible for a low tone |
| PULSE 25 % | thinner and more nasal, the 101's -2 pulse | the same `osc_pulse`, width 1/4 |

A SAW sub can be added later; it acts more like a second oscillator than a sub.

**HARMONICS (the shaping knob, recommended over a separate saturation).**
- **What it does:** it mixes in the same wave an octave above the sub (0 to 64) and then also two octaves above (64 to 127). On a SINE that is the 2nd and 4th harmonics: pure octaves, always in tune with the note.
- **Why it matters:** a sub on a phone or laptop speaker vanishes, and its octave overtones make it heard. This is the standard "make the sub audible" trick.
- **Odd overtones of a -1 sub are a different matter.** Saturation makes them: the 3rd harmonic of a -1 sub sits a fifth above the note. That is the hollow square sound. You already get it from the SQUARE wave, and from the TWIN DRIVE, which the sub passes through.
- So a sub-only saturation would mostly repeat the TWIN DRIVE (question 2).
- **Cost:** one or two extra wave reads (the octave copies at twice and four times the phase), +8 to +36 host instructions a sample depending on the wave (est.). At 0 nothing extra runs.

**LEVEL:** 0 to 127. 0 means the sub is not run at all (an exact bypass). At 127 the sub is about as loud as a full-scale engine oscillator (est.; your ears set the curve in the offline test). It glides per sample, so turning it makes no zipper noise.

**The stored values:**

| Value | Range, default | Notes |
|---|---|---|
| LEVEL | 0..127, 0 | 0 = not run |
| OCTAVE | -2, -1, 0; -1 | |
| WAVE | SINE, TRI, SQUARE, PULSE; SINE | append-only list |
| HARMONICS | 0..127, 0 | 0 = not run |

**WAVE's own SUB (CONFIRMED, `eng_wave.c` E5 "SUB": a sine an octave below).**
- Two controls called SUB on one engine would break "one word, one meaning".
- Proposal: WAVE's own SUB goes, and the sounds that use it get SUB LAYER SINE -1 at the matching level. A converter does that, like the TWIN conversion table. No backward compatibility is needed before the alpha.
- This also frees WAVE's slot E5 and about 60 to 100 B of WAVE code (est.).

### A3. Cost

**Per note (host instructions a sample, then points):**

| Setting | host instr. a sample | points a note | Mark |
|---|---|---|---|
| SINE, the sub inside the amp loop | +22 | 0.67 | **measured** on WAVE (SPEC 2.6: plain 59, SUB 81) |
| TRI | +18 to 20 | about 0.6 | est. |
| SQUARE / PULSE (polyBLEP) | +30 to 34 | about 1.0 | est. |
| HARMONICS up (one / two octave copies) | +8 to 18 on SINE, +18 to 36 on SQUARE | +0.3 to 1.1 | est. |
| the per-block work (the step, the unison rule, the FM6 hook) | under 1 | under 0.03 | est. |

Device factor: code in RAM x 0.85 (CPU_PLAN). The NOTES table adds its own 1.15 margin.

**Per engine** (one note today from `bench_baseline.txt` voice rows, CONFIRMED: LOFI 159, PHASE 210, NOISE 232, WAVE 234, ANALOG 235, TRIO 238, WHEEL 253, FM6 266, VOICE 270, PHYS 511 host instructions a sample). "8 →" is a part that plays 8 notes today, scaled by the cost ratio (est.):

| Engine | SINE (+22): share, 8 → | SQUARE (+32) | SQUARE + full HARMONICS (+60) |
|---|---|---|---|
| LOFI | +14 %, 7.0 | +20 %, 6.7 | +38 %, 5.8 |
| PHASE | +10 %, 7.2 | +15 %, 6.9 | +29 %, 6.2 |
| NOISE, WAVE, ANALOG, TRIO | +9 %, 7.3 | +14 %, 7.0 | +25 %, 6.4 |
| WHEEL, FM6, VOICE | +8 to 9 %, 7.4 | +12 to 13 %, 7.1 | +22 to 24 %, 6.5 |
| PHYS (capped at 3, one part) | +4 %: stays 3 | +6 %: stays 3 | +12 %: stays 3 |

**So no engine is "too heavy for the sub".** The cost is the same on every engine, so the lightest engine loses the most in proportion. In practice: about one note fewer per part with a sub, two with a SQUARE and full HARMONICS. With TWIN on as well (+1.1 to +2.6 a note), a part of 8 lands at about 5 to 6.

**Your two fallbacks, weighed:**
- **(i) Not on the heaviest engines.** The numbers do not support it: PHYS is where the sub costs least in proportion, and its 3-note cap does not move. Hiding the sub on an engine would only be right for a reason other than CPU, which is FM6's plan B.
- **(ii) Only when not polyphonic.** This would make the sub at most one per part (about 1 point), but it takes away poly basses and FM6/WAVE chords with weight. The NOTES counter already makes poly safe ("never crackle"). The part of this idea that is right is UNISON: one sub per key there.
- **Recommendation:** neither as a rule. The sub is everywhere, NOTES counts it, and UNISON gets one sub per key. If you still want a hard limit, (ii) is the cleaner of the two (plan B).

**RAM:** 8 B a note slot, 256 B in all (A1).

**Code (est.):**
- About 0.4 to 0.7 KB of core-1 code: the oscillator with four waves, the octave copies, the unison rule, the FM6 hook, and the switch cross-fade.
- About 0.8 to 1.2 KB in flash: the page, morph rules, the matrix destination, the format and the NOTES row.
- **Where it must live (CONFIRMED, MERGE_JOIN1 and design 1 section 4.4):** core 1 runs only from RAM. The hot block is at its 228 B limit, and range Z is at its temporary rule.
- The cheapest placement is inside TWIN's new chain function. That function already has its own branch out of `track_render`: the branch fires when TWIN is on or LEVEL is above 0, so the sub adds **0 hot bytes**. It can also use the range-X room that design 2 frees when the engine filters go (1.0 to 1.4 KB, est.).
- Without TWIN, the sub needs its own branch in `track_render` (16 to 24 hot bytes, est.), which needs the RAM shrink first.
- Removing WAVE's own SUB gives back about 60 to 100 B.

### A4. Fit

**Ids:**
- All free ids are taken by TWIN (design 2: 25, 26, 42, 106 and ANALOG's freed 113, CONFIRMED in design 2 section 3.2).
- So the sub's 4 values go into the **extension block** (stage A; about 2 B each when not at default).
- This needs one design rule from stage A: values in the extension block also get a place in the part's value array in RAM, after the 127 ids. Then the morph, the matrix's per-block destinations and the pages reach them like any value.
- MIDI LEARN, automation steps and parameter locks name values by a 7-bit id (CONFIRMED, `core.h` motion records), so they cannot reach the sub's values unless stage A widens them too (HYPOTHESIS: design choice open).
- The engines' freed E slots do not help: those ids are shared by every engine.

**Matrix destinations (append-only; 51 used of 128, CONFIRMED `mod.c` static assert):**
- **SUB LEVEL**, per block, gliding (like TRIM). Recommended for the first version.
  - A per-note SUB LEVEL (velocity into the sub per note) would need the per-voice path, which today covers only PITCH, CUT, SHP and AMP (`d <= MD_AMP`, CONFIRMED `mod_begin`).
  - The sub already follows velocity through the amp, so per block is enough.
- **SUB HARMONICS**, per block. Later, if wanted.
- **No SUB PITCH:** a detuned sub fights the note. The sub follows every PITCH route already.

**MORPH:**
- LEVEL and HARMONICS blend along the fader.
- OCTAVE and WAVE are never rolled in SMART (the sound's identity, like INSERT TYPE). When the two ends differ (WILD, later editable B), they switch at the middle of the fader. Old and new sub then run together for 4 blocks (2.9 ms, like an engine choice's `XF_BLOCKS`), the old one fading out and the new one in, so there is no click. The double cost lasts only those 4 blocks.
- **The SMART rule:** a roll never switches a sub on when End A's LEVEL is 0 (as with TWIN OFF), and it never lifts LEVEL far above the original (loudness).

**DUEL, STEREO, LAYER:**
- Everything is per note and per part, so all modes behave alike.
- In LAYER with a sub on both parts, a key gets two subs and pays twice. NOTES counts it.
- **SPREAD:** the sub belongs to its voice, so with SPREAD it goes left or right with that voice.
  - In STEREO and LAYER, the master BASS MONO (on by default at 200 Hz, CONFIRMED `core.h` `BM_DEF`) folds that back to the middle.
  - In DUEL each part is one side anyway.
  - No special rule is needed.
- **Two cores:** the sub's state is per note and per part, so the per-part split is unaffected.

**Hide-inert rules (only a selection hides):**
- LEVEL 0 hides nothing: it is a value.
- On FM6, the page is skipped only if the FM6 hook does not work out (plan B).
- No other selection makes a sub control inert.

**UI options for you to approve:**
- **Option A (recommended): one page "SUB OSC"** in the EDIT family, after the engine's pages and before MULTI FILTER 1/2 (signal order: oscillators, sub, filter 1). K1 WAVE, K2 OCTAVE, K3 HARMONICS, K4 LEVEL. The type comes first and the mix last (your page-order rule). The graph area draws the sub's wave small under the note, with its octave copies when HARMONICS is up.
- **Option B: the title "SUB"** alone. It is shorter, but it is then the same word as WAVE's old SUB card until that card goes.
- **Editor:** one SUB OSC card beside the engine's card, grouped by function.
- **Words checked:** HARMONICS is new. OCTAVE is a new card name; the ARP's OCT card is a different control, so the word written out keeps them apart. WAVE as a card name already exists on ANALOG, TRIO and LOFI and on the LFO pages, where it means the same thing (which wave), so it stays.

### A5. What to prove first, cheaply

1. **Offline Mac render (no unit):** the host DSP the tests already use (as TWIN step 0), plus a prototype sub stage.
   - Render **three sounds you name** (for example a bass, a lead and a pad of your choice).
   - Each one: without a sub; SINE -1; SQUARE -1; TRI -2; PULSE -2; HARMONICS at 0, 64 and 127; and with the TWIN DRIVE if TWIN's prototype exists by then.
   - The same run writes the cost row of each wave and the aliasing of TRI and SQUARE at OCTAVE 0 on C6.
2. **A listening page:** the versions in random order. You keep what you like and drop what you do not.
3. **Gates:**
   - G1: you keep a sub version on 2 of the 3 sounds.
   - G2: SINE costs at most +0.8 points a note in the host row.
   - G3: no click on note start, retrigger, steal or a WAVE/OCTAVE switch (the existing step tests).

Effort: about 1 round, plus your listening session.

---

## B. CYCLING ENVELOPE (CYCLE)

### B1. What it is

**In plain words:** an envelope that does not stop. It rises to the top in RISE time, falls back in FALL time, and starts rising again, for as long as the note sounds. Set it to ONCE and it rises and falls once per note, a simple attack-decay envelope.

**The classic forms:**
- **Eurorack function generators (MATHS and others):** rise, fall, a curve from logarithmic through straight to exponential, a CYCLE switch, and a trigger that restarts the rise (HYPOTHESIS, general knowledge).
- **Clipper's ENV 2 LOOP (CONFIRMED by reading `clipper-fm1` `firmware/src/voice.c` through the web fetch tool):** a switch on its ENV 2 (`P_E2LOOP`). When the decay comes within 1/64 of the sustain level, the attack starts again from there. It loops per voice until the key is let go, then the release runs. SUS sets the floor of the loop. The README says: "past its decay it starts its attack again, as long as the note is held". HYBRID's own ENV2 has no loop.
- **Waldorf Microwave:** an 8-stage wave envelope with loop points (SOS, CONFIRMED in HEADLINE_FEATURES_2). The Virus has no looping envelope (as the brief says; I did not check it).

**What the player controls (proposal):**

| Control | Range | Note |
|---|---|---|
| MODE | LOOP, ONCE | LOOP is the cycling envelope; ONCE is a third AD envelope |
| RISE | 0..127, the same time table as ATK | with ENV SYNC on, note values (as ATK2) |
| FALL | 0..127, the same table as DEC | with ENV SYNC on, note values |
| CURVE | -64..+63, 0 = straight | + slow start then fast (exponential feel), - fast then slow (the plucked-decay feel). Both segments mirror each other |

- **Tempo sync: it follows the existing ENV SYNC switch** (AMP DEST K4, which already syncs ENV 1 and ENV 2, CONFIRMED `voice.c` `esync_coefs`). One cycle is RISE + FALL, so RISE 0 and FALL 1/8 pulses in eighths. The catch: ENV SYNC also syncs the AMP ENV (question 4).
- **Retrigger:** each new note starts the rise from where the generator stands, so it never jumps. A legato note in MONO or LEGATO keeps it running.
- **Per voice, starting at the key.** Free-running is what LFO TRIG FREE already is.
- **Unipolar 0..1, like ENV.** The matrix's negative AMT inverts it.
- **During the release it keeps cycling** until the AMP ENV ends the note, so a pad's tail keeps pulsing. Clipper stops it at note-off instead. This is my design choice; ONCE never repeats anyway.

**Compared with what DUELL has (CONFIRMED, `voice.c`, `mod.c`, `params.c`):**

| | ENV 1 (AMP) / ENV 2 | LFO 1 / LFO 2 | CYCLE |
|---|---|---|---|
| per voice? | yes | **no**: one per part (`lfo_t` in `track_t`); TRIG NOTE restarts it for all notes | yes |
| repeats? | no | yes | LOOP: yes; ONCE: no |
| shape | ATK straight, DEC and REL exponential, fixed | SINE, TRI, SAW (ramp up), SQUARE, S&H; symmetric, no skew | rise and fall times free, CURVE |
| tempo | ENV SYNC | SYNC note values | ENV SYNC |

**What it gives that the others do not:**
- every note of a chord breathes on its own clock, started by its own key (strummed or arpeggiated chords pulse unevenly, like real players);
- lopsided shapes: a quick swell and a long fall, or a ramp that snaps back;
- rhythmic "bouncing" plucks: FALL with a negative CURVE into CUT or AMP gives a re-plucked note on each cycle;
- ONCE: a third envelope, for example for WAVE's POS or the twin DRIVE, without giving up ENV 2 as the filter envelope.

**Honestly:** on a mono lead with equal straight times, CYCLE is a TRI LFO with TRIG NOTE, and RISE 0 is a SAW-down LFO. The new value lies in per-note timing, free asymmetry and the curves.

### B2. Where it fits

**ENV 2 LOOP or a separate generator: a close call. I recommend the separate generator.**
- **For ENV 2 LOOP (the Clipper way):**
  - 1 stored value;
  - about 20 B of code, since DUELL's `env2_tick` is the same shape as Clipper's;
  - no new RAM;
  - SUS becomes the floor, which is musical.
- **Against ENV 2 LOOP:**
  - ENV 2 is DUELL's filter envelope: ENV 2 DEST FLT, the ENV 2 card on MULTI FILTER 2/2, and TWIN's filter-2 ENV 2 amount (CONFIRMED `params.c`; design 2). Looping it loops the filter sweep too, so a sound cannot have both a one-shot filter envelope and a cycling modulation.
  - Its attack is always straight and its decay always exponential.
  - It stops at note-off.
- You asked for an **added** mod source, which tips it.
- **ENV 2 LOOP stays plan B.** Even plan B needs a place for its one value in the extension block, because no id is free after TWIN.

**Per voice, not per part:**
- One generator per part would be an LFO with another shape: more duplication, and no per-note timing.
- Per voice costs 8 B a note slot and a few instructions per note and block (B3).

**Matrix source (CONFIRMED, `mod.c`, `params.c`):**
- The source list has 15 entries (`MS_OFF` .. `MS_DUCK`). A slot's SRC value can hold far more, so **CYCLE = 15 is appended** with no new id.
- Changes needed:
  - `N_MSRC` gets "CYCLE" (5 letters, like MORPH);
  - `MSRC_ORDER` places it after ENV2 (the knob order: OFF AENV ENV2 CYCLE LFO1 ...);
  - `MS_VOICE` counts it as per voice;
  - `mod_voice` reads the note's value for it;
  - `t->m_cycle` holds the latest note's value for per-block destinations (as `m_env2`);
  - `mod_begin` sets a "read" flag (as `mod->e2`);
  - the morph's WILD source list (`morph.c` line 520) may include it;
  - the editor's source list gets it too.
- **8 slots are enough:** the matrix stays at 8. CYCLE can reach every destination.
  - On a per-note destination (PITCH, CUT, SHP, AMP) each note uses its own cycle.
  - On a per-block destination (PAN, sends, E1..E8, TWIN values) it uses the latest note's cycle, as ENV does today (CONFIRMED, the `mod.c` header).

**Zero cost when unused, zero hot bytes:**
- Like ENV 2, CYCLE ticks only while an active slot reads it.
- Best of all, it ticks **inside `mod_voice`**, which already runs once per note and block whenever the matrix is on, and which lives outside the hot block (CONFIRMED: `noinline`, called from `track_render` line 1158). The hot block (at its 228 B limit) is not touched.
- The new note's start (setting the rise) goes in `voice_start` next to `v->stage2 = 1`, a few bytes (HYPOTHESIS that `voice_start` is not in the hot block; the hot list in `tools/hot_fns.py` decides).

**Stored values:** MODE, RISE, FALL, CURVE: 4 values in the extension block (no free ids).

**Hide-inert:**
- MODE ONCE hides nothing: RISE, FALL and CURVE all still act.
- The page shows on every engine. CYCLE does work on FM6, because it is a modulation source, not the amp.

**MORPH:**
- RISE, FALL and CURVE blend (like ATK2 and DEC2).
- MODE is never rolled in SMART. When the ends differ, it switches at the middle; a running cycle simply stops repeating after its current fall, so there is no jump.

### B3. Cost

- **CPU:** per note and block, one rise or fall step, a turn check, and the curve (one multiply-add: `y = x + c·(x² − x)`). That is about 15 to 30 instructions a block, under 1 a sample: **about 0.02 to 0.05 points a note**, only while read (est., calibrated on `env2_tick`, which is the same size; `BD_MX_SLOT` 8 and `BD_LFO` 5 host instructions a sample are the NOTES rows of similar things, CONFIRMED `budget.c`). The NOTES counter gets a row like `BD_LFO`.
- **RAM:** level (4 B), stage (1 B), padding: **8 B a note slot**, 256 B for the 32 slots (est.). Together with the sub that is 512 B against about 0.95 KB left after TWIN (est.), which is tight but fits. Holding the new state only for the 2 parts that play (NLIVE) would halve it.
- **Code:** about 150 to 250 B of core-1 code (range Z, beside `mod_voice`), plus about 0.5 to 0.8 KB in flash for the page, its graph, the morph rules and the format (est.).

### B4. UI options for you to approve

- **Option A (recommended): one page "CYCLE ENV"** in the ENV family, after ENV 2 DEST and before the ENV family's MATRIX pages. K1 MODE, K2 RISE, K3 FALL, K4 CURVE: the type first (your page-order rule). The graph area draws the cycle (two or three rise-fall shapes, or one for ONCE), as the ADSR graph does for ENV 2.
- **Option B:** K1 RISE, K2 FALL, K3 CURVE, K4 MODE, matching the time-first order of AMP ENV and ENV 2.
- **Names:**
  - matrix source **CYCLE** (5 letters, fits the list like MORPH);
  - page **CYCLE ENV** (9 characters, within the 11-character title limit, CONFIRMED `params.c` comment);
  - MODE values **LOOP** and **ONCE**.
- **Why not "LOOP" as the generator's name:** LOOP is the value; the thing is CYCLE. "ENV 3" would also work, but it says nothing about cycling.
- **Editor:** a CYCLE ENV card in the envelopes group, with the same picture.

---

## C. WAVE STEPS (stepped wave scan)

### C1. What it is for the player

- **The sound:** a wavetable that jumps from wave to wave instead of gliding. A slow sweep becomes a staircase of distinct timbres; a fast one becomes a rhythmic, "digital" chatter. On tables with a weak fundamental the steps seem to move the pitch as well (SOS on the Microwave: stepped movement changes the sound "drastically", CONFIRMED in HEADLINE_FEATURES_2).
- **What the classics did (HYPOTHESIS, forum sources, no official spec):**
  - the original PPG Wave did not blend neighbouring waves; it changed the wave only when a cycle of the oscillator started, so its sweeps stepped audibly;
  - smooth blending came with Waldorf's later machines;
  - SOS says most Microwave tables are made with fairly smooth changes between steps, and that stepped modulation sources change the sound a lot.
  - I found no proof that the Microwave has a dedicated step switch.
- **What the player wants to set (your list), and where each comes from in the proposal:**
  - **number of steps:** STEPS, 2 to 19 places across the scan (19 = every frame of a table);
  - **steps per beat / tempo:** the speed of whatever moves POS. LFO 1 or 2 with SYNC (note values up to 4 bars, CONFIRMED) gives N steps per bar. CYCLE with ENV SYNC gives a staircase per note in tempo;
  - **steps per note:** RAND into SHP picks a new step at each note (a random value per note-on, CONFIRMED `mod.c`). KEY into SHP gives a step by key position;
  - **order:** up = LFO SAW; down = SAW with a negative AMT, or CYCLE with RISE 0; up-down = TRI; random = S&H;
  - **hard steps or glide:** a short fixed cross-fade (C2). A longer "step glide" is an option for later.

### C2. How WAVE works today, and the best place for steps

**WAVE today (CONFIRMED, `eng_wave.c` header and `wave_render`; SPEC 2.6):**
- TABLE picks one of 26 factory tables and 3 user slots. Each table has 19 frames. Each frame is stored in 5 band-limited versions plus a sine.
- POS (E2) is the place along the 19 frames. Two neighbouring frames are mixed; all frames are in sine phase, so the mix is a mix of spectra with no level dips.
- POS is moved by the **SHP routes**: AMP DEST SHP, ENV 2 DEST SHP, LFO 1 / LFO 2 SHP and the matrix's SHP, as an offset (63 steps = the whole scan), per note. The matrix can also move E2 POS once a block.
- Each block, the target place `fp1` (in 1/65536 frames) is computed, and the read ramps from last block's place `fp0` to it over the 32 samples (`v->s[0]`). That ramp is what makes POS smooth today.
- BEND warps the read (a pulse width for any wave), SYNC restarts it 1 to 8 times per cycle, SUB adds a sine an octave down, and DTN adds a second read. E7 and E8 are unused ("-").
- There is a table called **STEPS** (a sine held in 64 to 3 steps per cycle). That is steps inside one wave, not a stepped scan. The card label would be the same word, so the new control needs another name, or the table gets renamed (C3).

**Three ways to build it, weighed:**
1. **A rounding of POS inside WAVE (recommended).**
   - After `fp1` is computed, round it to one of STEPS places. The existing block ramp then becomes the cross-fade between two steps: one block, 0.73 ms. Two blocks (1.5 ms) are possible by halving the ramp.
   - Since all frames are in sine phase, the cross-fade between steps has no click (est.; the step tests confirm it).
   - **Hysteresis:** a new step is taken only when the target passes the edge by about 1/4 step, so an LFO that hovers on an edge does not flicker.
   - **Cost:** about 5 to 10 instructions a block and note (about 0.01 points), 0 B of note memory (the held place is `v->s[0]`, which exists), and about 40 to 80 B of code in WAVE (est.).
   - **Storage:** WAVE's unused slot **E7**: **no new id, no extension block, 0 extra patch bytes**, because every patch already stores all eight engine slots.
2. **A quantiser on the SHP destination for every engine.** It would also step PHASE's DCW, LOFI's pulse width and FM6's feedback, which means something different on each engine. It is less clear, and its value would need a global id or the extension block.
3. **A separate stepper source** (its own RATE, STEPS and ORDER). That is 3 or 4 new values, and it duplicates an LFO followed by a rounding. It belongs to the parked "stepped modulation" idea, not to the stepped scan.

**The PPG alternative for the cross-fade:** changing the frame exactly when the read starts a new cycle (the PPG way, HYPOTHESIS) needs a check inside the per-sample read loop, so it costs more. It is worth hearing in the prototype (C4); the block cross-fade is the default.

**Hide-inert:** STEPS OFF hides nothing, since it is a value. A later STEP GLIDE would be hidden while STEPS is OFF (a selection).

**MORPH:**
- STEPS blends between two step counts (rounded).
- Between OFF and a count it switches at the middle of the fader.
- POS keeps blending as today; it just lands on steps.

**Matrix:** today E7 shows as faded UNUSED (`mod_e_why`). It becomes a working per-block destination, "WAVE STEPS", so an LFO or the MORPH can change the step count.

**What CYCLE could drive:**
- **CYCLE (LOOP) into SHP with STEPS 8:** each note climbs or falls through 8 timbres in its own rhythm. That is the Microwave's looping wave envelope in two controls.
- **CYCLE ONCE into SHP with STEPS:** a one-shot staircase per note, which works for plucks.
- The SUB LAYER can sit under it at -1, so the stepping stays in the upper sound and the low end stays steady.

**CPU and RAM (WAVE is a mid-weight engine, `bench_baseline` 234):** STEPS adds about 0.01 points a note and no RAM (est.). Moving POS costs nothing more than today.

### C3. UI options for you to approve

- **Option A (recommended): STEPS on EDIT 1 K4.**
  - Today WAVE's ENGINE page holds TABLE, POS and BEND, and EDIT 1 holds SYNC, SUB, DTN and an empty card (slot E7), CONFIRMED: no `uiord` on WAVE, so the slots show in order.
  - STEPS fills the empty card and nothing else moves (your no-rearranging rule).
  - If WAVE's own SUB goes (A2), its card on EDIT 1 is hidden and nothing moves either.
- **Option B:** STEPS beside POS on the ENGINE page. That would push BEND and SYNC along, which breaks the no-rearranging rule. Not recommended.
- **The name:**
  - STEPS is also the name of table 24, so one word would have two meanings. Options:
    - (a) the control is **STEPS** and table 24 is renamed **STAIR** or **LOFI** (5 letters; LOFI is also an engine name, so STAIR is better);
    - (b) the control is **STEP** (singular), next to a table called STEPS: too close;
    - (c) the control is **SCAN**, with values SMOOTH, 2 .. 19.
  - **I recommend (a), STEPS with table 24 renamed STAIR.** Your call.
- **The graph area:** WAVE's page could draw the scan as a ramp, or as a staircase with N treads.
- **Editor:** a STEPS slider beside POS on the WAVE card. The editor groups by function, so it may sit there even though the device keeps it on EDIT 1.

### C4. Effort and the cheapest prototype

- **Prototype (a Mac render):**
  - the host WAVE renderer with the rounding of `fp1`;
  - three WAVE sounds you name, each with an LFO SAW synced to 1 bar into SHP, at STEPS OFF / 4 / 8 / 19;
  - plus RAND per note at STEPS 8;
  - plus the PPG-style frame change at the cycle start, for comparison.
  - Effort: about half a round.
- **The feature:** about 1 round (the rounding, hysteresis, the E7 descriptor, the matrix destination, the morph kind, the page card and the editor), plus your layout approval.
- **Order:** it needs no format work, so it can come before or after TWIN.

---

## Effort, cloud and local, risks, plan B

| Item | Rounds (est.) | Depends on | Cloud (simulation-testable) | Local (needs the unit or you) |
|---|---|---|---|---|
| A. SUB proof (A5) | 1 + your listening | nothing | the renders, cost rows, aliasing, step tests | your listening |
| A. SUB LAYER | 2 to 3 | the extension block (stage A); best after TWIN step 1 (its chain and branch); the FM6 hook | the oscillator, octave and unison rules, the FM6 hook on the host, morph switching, the NOTES row, the matrix destination, the format | RAM placement (range X / Z, hot block), timing on the unit, the SUB OSC page after your layout approval, ears |
| B. CYCLE | 1 to 2 | the extension block | the generator, curves, sync, the matrix source plumbing, morph, NOTES row | the page and graph on the device, ears |
| C. WAVE STEPS | 0.5 prototype + 1 | nothing | the rounding, hysteresis, cross-fade click tests, morph | the card on the device, ears |

**Risks:**
1. **Core-1 RAM.** The SUB's 0.4 to 0.7 KB competes with TWIN's 1.5 to 1.8 KB. The way out is the same as TWIN's: the code the engine filters free, plus the RAM shrink. CYCLE (in range Z) and STEPS (inside WAVE) are small.
2. **Format.** SUB and CYCLE need the extension block, and also a rule that extension values live in the part's RAM array, so the morph and the matrix reach them. Whether MIDI LEARN and automation can reach them is an open stage-A choice.
3. **FM6 and the sub.** The carrier-level hook is HYPOTHESIS until tried. Plan B: no SUB on FM6.
4. **Loudness.** A sub adds level. The PATCH LEVEL of sounds you give a sub is yours to set. The offline test sets LEVEL's curve.
5. **RAM per note.** SUB plus CYCLE add 16 B a slot (512 B). That is tight after TWIN's state, and the fix is the NLIVE-only state.

**Plan B for each:**
- **SUB:** (1) SINE, TRI and SQUARE only, no HARMONICS (3 values); (2) only in MONO, LEGATO and UNISON (your fallback ii), at most one sub per part; (3) not on FM6.
- **CYCLE:** a LOOP switch on ENV 2, the Clipper way (1 value, about 20 B).
- **STEPS:** fixed at 19 (every frame), so STEPS is a simple ON/OFF on the same slot.

---

## Questions for you (each with my recommendation)

1. **SUB everywhere, paid for by the NOTES counter.** About 0.7 points a note for a SINE sub (one note fewer at 8), and one centred sub per key in UNISON. No ban on heavy engines and no "mono only" rule, because the numbers show the sub is relatively cheapest on PHYS and dearest on LOFI.
   **Recommended: yes.**
2. **The sub's fourth knob: HARMONICS (octave overtones, always in tune, makes the sub heard on small speakers) rather than a saturation on the sub alone.** The sub already passes through the TWIN DRIVE, and SQUARE gives the hollow, odd-overtone sound.
   **Recommended: HARMONICS.** A sub DRIVE could be added later as a fifth value if you miss it.
3. **The sub's lists: OCTAVE -2, -1, 0 and WAVE SINE, TRI, SQUARE, PULSE.** WAVE's own SUB card is then retired, and its sounds are converted to SUB LAYER SINE -1, so there is one SUB on the unit.
   **Recommended: yes to both.**
4. **CYCLE as a new, separate per-note envelope** (MODE LOOP / ONCE, RISE, FALL, CURVE; it keeps cycling through the release; tempo through the existing ENV SYNC switch), instead of a LOOP switch on ENV 2, which would make your filter envelope loop. The catch of sharing ENV SYNC: syncing CYCLE also syncs the AMP ENV and ENV 2 times. Its own SYNC switch would be one more value.
   **Recommended: separate CYCLE, sharing ENV SYNC.**
5. **WAVE STEPS as one control on WAVE's empty EDIT 1 card (OFF, 2 to 19 steps), with a fixed 1.5 ms cross-fade and hysteresis.** The motion comes from what already moves POS: a synced LFO for steps per beat, RAND for a step per note, CYCLE for a staircase per note. Table 24 STEPS would be renamed STAIR so the word means one thing.
   **Recommended: yes.**

---

## Evidence list

| Claim | Source | Mark |
|---|---|---|
| Pitch per note and block: glide, master tune, bend, LFO 1/2 and AMP DEST PIT, unison fine, then the matrix PITCH recomputes `m.inc` | `voice.c` `track_render` (1132-1158), `glide_set`; `mod.c` `mod_voice` (555-560) | CONFIRMED |
| Unison detune per voice, no undetuned voice at even counts; random start phases | `voice.c` `mono_play` | CONFIRMED |
| Retrigger keeps phases; new voice from rest; steal = one-block fade (stage 4); silent-sustain skip | `voice.c` `voice_start`, `env_tick`, line 1139 | CONFIRMED |
| FM6 owns its amplitude (`ownenv`), AMP ENV forced to full, cards hidden; carriers' `gain_out` per operator | `eng_fm6.c` (`.ownenv`, `fm6_done`), `voice.c` 1107-1116, `ui_draw.c` 770, `fm6_core.c` `fm6_note_done` | CONFIRMED (the hook's use: HYPOTHESIS) |
| No microtuning; scale acts on note numbers | search of `src` (only `fm6_core.c` comment); SPEC 1.x scale text | CONFIRMED (no code found) / HYPOTHESIS (nothing re-tunes later) |
| Oscillators: `sine_i`, `osc_tri` (plain), `osc_saw` / `osc_pulse` with polyBLEP; `voice_amp`, `VOICE_FS` 24000 | `dsp.c` | CONFIRMED |
| WAVE's own SUB: sine an octave down, `inc >> 1`, level E5 x 200 | `eng_wave.c` `wave_render` 888-896, `.edit` | CONFIRMED |
| WAVE costs: plain 59, SUB 81 host instructions a sample a voice | SPEC 2.6 (`tests/wave_test.c`) | measured (host) |
| Voice rows ANALOG 235 ... PHYS 511; NOTES formula, BUDGET_LIMIT 80, MARGIN 115, `BD_LFO` 5, `BD_MX_SLOT` 8 | `tests/bench_baseline.txt`, `budget.c`, `budget.h` | CONFIRMED (host figures) |
| 33 host instructions = 1 point; RAM x 0.85 | docs/CPU_PLAN.md | CONFIRMED |
| PHYS cap 3; FM6 6; VOICE 4 | `eng_phys.c` `PHYS_POLY`, `eng_fm6.c`, `eng_formant.c` | CONFIRMED |
| Matrix: 15 sources, 51 destinations (append-only), per-voice destinations only PITCH CUT SHP AMP, latest-note rule, ENV 2 ticked only when read, `mod_voice` noinline | `mod.c` (enums, static asserts, `mod_begin`, `mod_voice`), `params.c` `N_MSRC`, `MSRC_ORDER` | CONFIRMED |
| ENV 2 = ATK/DEC/SUS/REL per voice, stops at release; ENV SYNC covers ENV 1 and ENV 2 | `voice.c` `env2_tick`, `esync_coefs` | CONFIRMED |
| LFOs per part (`lfo_t` in `track_t`), waves SINE TRI SAW SQUARE S&H, TRIG NOTE/FREE, SYNC | `core.h`, `voice.c` `lfo_wave_of`, `lfo_tick` | CONFIRMED |
| Page list, ENV 2 DEST / MULTI FILTER 2/2 cards, 11-character titles | `params.c` `PAGES` | CONFIRMED |
| Free ids all used by TWIN (25, 26, 42, 106, 113); 6 TWIN values in the extension block | TWIN_FILTER_DESIGN_2.md 3.2 | CONFIRMED there |
| Extension block, 13 B of room today, stage A | HEADLINE_FEATURES.md | CONFIRMED there |
| Hot block at its 228 B limit; range Z rule; RAM headroom | MERGE_JOIN1 via TWIN_FILTER_DESIGN.md 4.4 | CONFIRMED (build numbers) |
| BASS MONO default on at 200 Hz | `core.h` `BM_DEF`, `BM_HZ` | CONFIRMED |
| WAVE: 19 frames, sine phase, POS moved by SHP routes, block ramp `fp0` → `fp1` in `v->s[0]`, E7/E8 unused, table 24 STEPS | `eng_wave.c`, SPEC 2.6 | CONFIRMED |
| Clipper ENV2 LOOP: `P_E2LOOP`, restart near SUS, per voice, ends at release; HYBRID ENV2 has no loop and no sub; README "SUB a sine an octave down" (a WAVE feature) | github.com/isod89/clipper-fm1: README, `firmware/src/voice.c`, `firmware/src/eng_hybrid.c` (read through the web fetch tool's summary) | CONFIRMED in those files (through the fetch summary) |
| Virus: 3 oscillators + sub, saturation after filter 1 | research note "Virus-style one-part firmware" | CONFIRMED there |
| Microwave: stepped movement changes the sound drastically; 8-stage wave envelope with loops | HEADLINE_FEATURES_2 (SOS) | CONFIRMED there |
| PPG: no blending between waves, the wave changed at the cycle start | [Gearspace: summarizing wavetable methods](https://gearspace.com/threads/summarizing-the-different-wavetable-synth-methods.1157742/), [Gearspace: Modal 002 thread](https://gearspace.com/threads/modal-002.938173/post-12530650), [KVR: PPG, Wave and Microwave](https://www.kvraudio.com/forum/viewtopic.php?p=1416986), [PPG Wave (Wikipedia)](https://en.wikipedia.org/wiki/PPG_Wave), [SOS: Getting creative with the Microwave](https://www.soundonsound.com/techniques/getting-creative-waldorf-microwave-synths) | HYPOTHESIS (forum recollection) |
| Juno / SH-101 sub octaves and pulse; MATHS controls; Virus sub waves | general knowledge | HYPOTHESIS |
| Every SUB / CYCLE / STEPS cost except the measured WAVE SUB; code sizes; RAM per slot | my counts against `env2_tick`, WAVE's SUB and the MULTI FILTER | est. |

**Not verified:**
- every cost figure except WAVE's measured SUB row;
- that FM6's `gain_out` gives a usable loudness for the sub;
- whether `voice_start` is in the hot block;
- how much room range X really frees when the engine filters go;
- whether the Microwave has its own step switch;
- the loudness curve LEVEL needs.
