# TWIN FILTER: a Virus-style second filter for DUELL (design study, 2026-10-19)

Private. Research and design only: no code was changed and no branch was made. INDEX.md was not touched. I wrote no sounds. Every page, label and knob below is a proposal for you to approve, not a decision.

Marks:
- **CONFIRMED**: I read it today in DUELL's code (the newest merged line, `wt/join0233`) or in its documents.
- **HYPOTHESIS**: my reading, an estimate of behaviour, or general knowledge about other synths.
- **est.**: a number I worked out that has not been measured.
- **measured**: a DUELL number from your unit or the computer.
- CPU is given in **points**: 1 point = 1 % of one core's time, as in docs/CPU_PLAN.md. A note costs about 4 to 8 points today.

Words used here that are not yet in GLOSSARY.md (explained once, here):
- **Series / parallel**: in series the sound goes through filter 1 and then through filter 2. In parallel the sound is split, goes through both filters side by side, and the two results are mixed.
- **Drive (saturation)**: the signal is made louder into a curve that rounds off (or cuts off) its peaks. That adds overtones, from warm to raspy.
- **Slope (12 dB / 24 dB)**: how steeply a filter removes what is beyond its cutoff. 24 dB is steeper and darker than 12 dB.
- **Resonance**: a lift of the tones right at the cutoff; at high settings it whistles.
- **Self-oscillation**: resonance so high that the filter sings a tone by itself.
- **State-variable filter (SVF)**: the filter type DUELL uses everywhere today. One small circuit gives low-pass, band-pass and high-pass at once.
- **Ladder**: the Moog-style filter: four gentle filter steps in a row with the output fed back to the input. At high resonance it thins the bass, which is part of its famous sound.
- **Comb filter**: a very short echo fed back into itself. It rings at one pitch and gives a metallic, plucked-string or tube colour. Positive feedback rings at the note; negative feedback gives a hollow tone an octave lower.
- **Formant (vowel) filter**: two or three narrow band-passes placed where a voice's vowel resonances are, so the sound says "aah", "ooh", "eeh".
- **Notch**: the opposite of a band-pass: it removes a narrow band and keeps the rest.
- **Aliasing**: harsh extra tones that a digital process makes when the overtones it creates pass the top of the sample rate. Drive and folding create them.
- **Sidebands**: new tones that appear above and below the original ones when something wobbles very fast (at audio speed).
- **Pre-amp / post-amp**: before or after the amplitude stage (the AMP ENV and velocity that set each note's volume).
- **Engine slot (E1..E8)**: the eight stored values every engine has for its own controls.

---

## Verdict (half a page)

**The proposal: TWIN FILTER, a second filter on every note of every engine, in series or in parallel with today's MULTI FILTER, with a DRIVE between them, and the whole chain moved before the amp, the Virus way.**

- **Filter 1 stays the MULTI FILTER**, unchanged, on all ten engines.
- **Filter 2 is new, chosen from a list of TYPES.** First version (est.): LP, BP, HP and NOTCH (12 dB, the math of ANALOG's VCF plus TRIO's notch), LADDER (24 dB, Moog-style) and COMB (tuned to the note, + and - feedback). FORMANT, a driven 12 dB (MS-20-style), TRIO's gritty CHIP filter and FILTER FM come after a listening test.
- **DRIVE** sits between the two in SERIES, and before both in PARALLEL. 0 is an exact bypass.
- **Engines are not touched.** ANALOG's VCF, TRIO FILTER and NOISE FILT stay inside their engines, bit for bit. I recommend **against lifting them out**: ANALOG's VCF and NOISE FILT are the same state-variable filter as the MULTI FILTER (CONFIRMED: `dsp.c` `tsvf_*`), so lifting them gives no new sound. Lifting would also change every ANALOG, TRIO and NOISE sound. Their characters come back as copies in the TYPE list instead.
- **Every sound of today plays bit for bit** while TYPE is OFF (the default).

**What the player gets:**
- the Virus bass: low-pass, drive, then a second filter;
- vocal leads from two band-passes in parallel;
- metallic and string-like colours from COMB, on FM6, VOICE, WHEEL and ANALOG alike.

**The cost:**
- about +1.3 (COMB with DRIVE) to +2.5 (LADDER with DRIVE) points per note (est.);
- that is about 15 to 30 % more per note, so a part that plays 8 notes plays about 6 to 7;
- that is roughly what the second core gave back (14 points on M07, measured);
- about 1.2 to 1.5 KB of core-1 code;
- no new RAM for the comb lines (they use the engine memory that 8 of the 10 engines leave empty).

**The main risks:**
1. Core 1's code RAM is full today (pool tail 2,840 B against the 2,816 B temporary rule; hot RAM exactly at its 228 B limit, CONFIRMED MERGE_JOIN1). The RAM shrink must land first.
2. The patch format: PARALLEL needs about 6 stored values. Only 4 parameter ids are free, so PARALLEL needs the extension block before the alpha. A series-only version fits the 4 free ids with no format change (plan B).
3. Whether it sounds worth 2 notes. Only your ears and your Snow can say that, so a cheap offline render test comes first (section 5).

---

## 1. Today's filters, engine by engine

### 1.1 Where each filter sits (CONFIRMED, `voice.c` `track_render`, `uf_voice`, `uf_run`; the `eng_*.c` renders)

**Why the MULTI FILTER is after the amp.**
- Every engine applies the note's volume itself, inside its own sample loop: `voice_amp(s, m, i)` in ANALOG, TRIO, NOISE, LOFI, VOICE, WHEEL, WAVE and PHYS, and the same ramp written out in PHASE and FM6 (`m->amp0`, `m->amp1`). The volume is the AMP ENV x velocity x LFO AMP x the LAYER XFADE.
- `uf_voice` lets the engine render a finished, volume-shaped note into a scratch block, and then runs `uf_run` on it.
- So the MULTI FILTER (since 0.2.2) was added in the one place that needed no change to ten engines: after the render, which means after the amp. That also made its bypass exact (the defaults render straight, bit for bit).
- The engines' own filters (ANALOG, TRIO, NOISE) are inside the render loop, before `voice_amp`: **before the amp**.

**What that means for a drive.** A drive put after today's MULTI FILTER would be after the amp: loud attacks would distort more, quiet tails less, and velocity would change the distortion. That is an overdriven amplifier, not a Virus voice. The Virus puts its saturation after filter 1 (Virus manual and SOS, both read in earlier passes); that its filters come before its amp is likely but unverified (HYPOTHESIS). Section 5 includes a 2-minute test on your Snow that settles it.

### 1.2 The table

| Engine | Own filter? | Before / after the amp | Topology and slope | State per note | Cost per note | Stored values (ids) | Matrix |
|---|---|---|---|---|---|---|---|
| ANALOG | **VCF** | before | Simper state-variable, **12 dB**, LP / BP / HP (TYPE); damping 2.0 to 0.145; tanh DRIVE **before** it; soft knee at 16000 after it; KEY TRACK around note 60 | 8 B (`s[0..1]`) | est. 0.4 to 0.6 (not measured alone) | TYPE `P_FTYPE` (own id), CUT E5, RES E6, DRV E7, KTR E8 (`P_E4..P_E7`) | CUT and every FLT route, the accent, CC74/71 move it; E5..E8 by label |
| TRIO | **TRIO FILTER** | before | the same SVF, **12 dB**, LP / BP / HP / NOTCH; damping 2.0 to 0.25 (never sings); input soft knee; band-pass state squeezed above +-49152 (grit); a slow random cutoff drift drawn from the note's noise generator; runs one sample late (anti-aliasing) | 12 B (`s[0..1]`, drift `s[5]`) | est. 0.6 to 0.8 | MODE E5, CUT E6, RES E7 | as ANALOG; MODE cross-fades in MORPH (`MX_E0+4`) |
| NOISE | **NOISE FILT** | before | the same SVF, **12 dB**, morphing from LP (RES 0) to a narrow BP (RES 127); level make-up computed from cutoff, colour and mode; after it come the CRSH and META's DC blocker | 8 B | est. 0.7 to 0.9 | FREQ E3, RES E4, TRK E5, DRFT E7 moves it | as ANALOG |
| PHASE, LOFI, VOICE, WHEEL, PHYS, FM6, WAVE | no (LOFI has a one-pole TONE inside; PHYS's filters are part of the string model; VOICE's resonators are the vowel itself) | - | - | - | - | - | CUT and the FLT routes move the MULTI FILTER instead |
| **all ten** | **MULTI FILTER** | **after** | the same SVF, **12 or 24 dB** (24 = two stages, Butterworth pair), MODE blends LP > BP > HP, kill zones at the ends, soft knee at 48000 | 25 B (`uf[4]`, `ufw[3]`, `uft`, `msl`) | **measured** host: 12 dB +46.6, MODE blend +58.6, 24 dB LP +77.3, 24 dB HP +104 instructions a sample; device est. +1.8 (12 dB) and +2.6 (24 dB) points | CUT 45, RES 46, MODE 47, SLOPE 48 (`P_FREEA..D`), KEY TRK 27 | ufilt 1 engines: CUT, FRES (38), FMODE (46). ANALOG / TRIO / NOISE: FRES and FMODE only, no CUT (SPEC 2.5) |

Note (CONFIRMED, `budget.c`): KEY TRK's default of 32 keeps the MULTI FILTER out of its bypass, so most factory sounds already run filter 1 at 12 dB. Its cost is already paid; the TWIN cost below comes on top of it.

### 1.3 Can the engines' own filters be lifted out?

**What is engine-specific in each (CONFIRMED by reading the renders):**
- **ANALOG VCF.**
  - Its DRIVE runs before the filter, inside the oscillator loop.
  - Its cutoff is E5 plus the routes plus KTR, in the engine's own cutoff units. The MULTI FILTER's dial step is 1.55 of those units, so the same route moves the two by different amounts.
  - The accent opens it.
  - MORPH cross-fades its TYPE inside the engine (`analog_render_m`).
  - Its soft knee works at the engine's level, before the amp.
- **TRIO FILTER.**
  - Its cutoff drift takes random numbers from the same generator as the pitched-noise oscillator. Moving the filter out changes the noise of every NOIS set (no longer bit for bit).
  - It runs one sample late together with the oscillators' step corrections.
  - Its resonance is limited on purpose (the chip character).
- **NOISE FILT.**
  - The filter and the level make-up are one piece of code.
  - CRSH and the DC blocker come after the filter. Lifted out after the engine, the crusher would come first: a different sound.

**What would change if they were lifted:**
- every ANALOG, TRIO and NOISE sound, factory and yours;
- their E slots;
- the routes (CUT and the FLT depths would need a new target);
- the MORPH rules for those engines (TYPE and MODE fades, the "no dead filter path" rules).

**What lifting would gain in sound: almost nothing.**
- ANALOG's VCF and NOISE FILT are the same Simper filter as the MULTI FILTER, with another damping range and level.
- Only TRIO's has a character of its own: grit, notch and drift.

**The one real reason to lift** would be to free E slots. On ANALOG, CUT, RES and KTR take three of the eight; they could hold, say, a third oscillator. That is a separate engine redesign and not part of this feature.

**My recommendation:** keep all three inside their engines, bit for bit, and offer their characters as **copies** among filter 2's TYPES:
- the SVF types use ANALOG's damping range;
- NOTCH is TRIO's tap;
- a later CHIP type copies TRIO's grit and drift.

**What must stay bit for bit:**
- every stored sound, factory sound and multi while TWIN is OFF;
- the three engines' own filters always;
- `uf_run` itself. TWIN calls it unchanged for filter 1, so its hot code is not edited.

---

## 2. The new arrangement

### 2.1 The chain

```
TWIN OFF (today, bit for bit):
  engine (own filter on ANALOG/TRIO/NOISE) -> amp -> MULTI FILTER -> part

TWIN on, SERIES:
  engine at full level -> MULTI FILTER -> DRIVE -> FILTER 2 -> amp -> part

TWIN on, PARALLEL:
                                     +-> MULTI FILTER -+
  engine at full level -> DRIVE -----+                 +-> BLEND -> amp -> part
                                     +-> FILTER 2 -----+
```

**How the amp moves without touching the engines.**
- Every engine scales by the same volume ramp (`m.amp0` to `m.amp1`, CONFIRMED in all ten).
- With TWIN on, the voice hands the engine a copy of that ramp set to full level, runs the chain, and then applies the real ramp after filter 2.
- Cost: one multiply a sample (est. +0.2 points a note).
- So the drive no longer depends on the envelope or velocity; a VEL route into DRIVE brings that back when you want it.

**Side effect.** With TWIN on, filter 1 runs before the amp, so its soft knee sees the full note level. A very resonant filter 1 may sound a little more compressed on quiet notes than it did before. With TWIN OFF nothing changes.

**On ANALOG, TRIO and NOISE** the engine's own filter stays first, inside the engine. Bypass the MULTI FILTER (its default) and the chain is exactly the Virus one: VCF with its envelope, then DRIVE, then FILTER 2.

**Switching TWIN on or off while a note sounds** cross-fades one block, as the MULTI FILTER's bypass edge does (`uf_edge`, 0.2.25). The new filter starts from rest, charged with its input. No click.

### 2.2 Filter 2's cutoff: linked by sharing the routes

- **CUTOFF 2 is its own knob** on the same dial as filter 1 (30 Hz to 16 kHz).
- **Every cutoff route moves it too**: ENV 1 / ENV 2 / LFO 1 / LFO 2 FLT, the matrix's CUT, and KEY TRK. So two cutoffs set apart keep their distance while the envelope sweeps both. That is what the Virus calls cutoff link (HYPOTHESIS on the Virus's exact behaviour).
- On ANALOG, TRIO and NOISE the routes are scaled so that filter 2 moves by the same number of octaves as the engine's own filter. Without that scaling, the 1.55 difference above would make it move further.
- **A new matrix destination TWIN CUTOFF** moves filter 2 alone, when you want them apart.
- **COMB follows the note instead of the cutoff.** Its knob becomes TUNE, in semitones around the note.

### 2.3 The TYPES of filter 2

| Type | Character in one line | est. cost per note (points) | First version? |
|---|---|---|---|
| OFF | today's sound, bit for bit | 0 | yes |
| LP | 12 dB low-pass, ANALOG's VCF math: round, warm; after a 24 dB filter 1 it gives the Virus 36 dB | 0.9 to 1.2 | yes |
| BP | 12 dB band-pass: nasal, telephone; two in parallel give vowels | 0.9 to 1.2 | yes |
| HP | 12 dB high-pass: thins the low end, the classic "LP then HP" band | 0.9 to 1.2 | yes |
| NOTCH | removes one band (TRIO's notch tap): phaser-like when swept | 0.9 to 1.2 | yes |
| LADDER | 24 dB Moog-style with a saturating feedback: fat, the bass thins at high RES, sings at the top | 1.4 to 1.8 | yes |
| COMB | tuned to the note, FEEDBACK bipolar (+ rings, - hollow), with damping | 0.5 to 0.7 | yes (not on WAVE and PHYS: section 4.3) |
| FORMANT | two band-passes on a vowel path A-E-I-O-U; CUTOFF becomes VOWEL | 1.7 to 2.1 | after the listening test |
| MS-style (working name) | 2-pole with drive in its feedback: screaming, aggressive | 1.0 to 1.3 | after the test |
| CHIP | TRIO's gritty filter as a copy: compressed resonance, drift | 0.9 to 1.2 | after the test |
| DIODE | 303-like 4-pole | 1.4 to 1.8 | later |

All costs are est. (section 4). Every SVF type is 12 dB, as the Virus's second filter in the research note ("a 4-pole in series with a 2-pole"). A steeper sound comes from filter 1 at 24 dB.

**Why each SVF mode is its own TYPE** (LP, BP, HP, NOTCH as four list entries, not a MODE knob):
- it saves one stored value and one knob;
- filter 1 already has the blends;
- one turn of TYPE reaches LADDER and COMB too.

**No phaser type.** The part's INSERT PHASR exists, and NOTCH swept by an LFO covers the per-note version.

**On the brief's "lift the engine filters" idea, in short:**
- ANALOG's VCF is the LP / BP / HP types;
- TRIO's is NOTCH now and CHIP later;
- NOISE FILT's LP-to-BP morph is filter 1's MODE blend, which exists, so it is not repeated.

### 2.4 FILTER FM in plain words

- An LFO moving the cutoff a few times a second gives a "wah".
- Make the wobble as fast as the sound itself (hundreds of times a second) and you no longer hear a movement. You hear **new tones (sidebands)** that thicken the sound: a growl or bark on a resonant low-pass, a metallic edge when the two are not in tune.
- The Waldorf Microwave XT and Q, and the Nord Lead 3, have it.

**The sources:**
- **SELF**: the note's own signal (filter 2's input) wobbles filter 2's cutoff.
  - Works on all ten engines, is always in tune with the note (a growl, not noise), and needs no new oscillator.
  - Recommended.
- **A sine at the note's pitch**: a cleaner, bell-like FM. One phase per note. Later, if wanted.
- **The other part's audio**: rejected.
  - It would tie the two cores together every block (part 2 runs on core 1).
  - Two chords give noise.

**The cost.** The filter's settings must be recomputed every sample instead of once a block. That is about +0.3 to 0.5 points a note (est.).
- The cheap stable way: one correction step per sample around the block's value, with the swing capped to about +-1 octave.
- An exact divide per sample is the fallback if the correction step proves unstable.
- FM 0 runs the block path, unchanged.

**Include it?** Yes as an option on LP / BP / HP / NOTCH / LADDER, but **after** the listening test, not in the first version.
- It is the riskiest part: per-sample settings, aliasing, stability.
- It is also the most distinctive, since no FM-1 firmware has it (CONFIRMED for Clipper's code, CLIPPER_STUDY section 3).
- If it wins by ear in the offline test, it is the first addition.

### 2.5 DRIVE

- **What it does:** it raises the level into a curve.
  - After a low-pass filter 1 the curve adds fresh upper harmonics, and filter 2 then shapes them.
  - "A little saturation at a low cutoff gives a heavy, aggressive sound" (Virus manual, quoted in the research note).
- **0 = exact bypass.** The shaper is not run, so with DRIVE at 0 the chain is just the two filters.
- **No step when it leaves 0.** Even a gentle curve squeezes loud peaks by about 2 dB.
  - So values 1 to 11 blend dry and driven, as SAT does since 0.2.14 (SPEC 2.4: `dry + (wet - dry) x n / 12`).
  - The level is kept roughly constant as DRIVE rises, as TAPE's SAT does (gain^-0.75).
  - The value glides per sample (no zipper).
- **Curves:**
  - SOFT: tanh, the curve of ANALOG's DRV and SAT, warm. First version.
  - Later, as a CURVE choice:
    - HARD: buzzy, more aliasing;
    - FOLD: the peaks fold back, metallic, the most aliasing;
    - ASYM: tube-like even harmonics; needs a small DC filter per note.
- **In PARALLEL** the drive sits before both filters, so both get the extra harmonics. A band-pass pair on a plain FM6 sine then has material to make vowels from. Where the Virus puts it in parallel I did not find (HYPOTHESIS).
- **Aliasing:** filter 1's low-pass before the drive is also the cheapest anti-aliasing: it removes the highs before the curve multiplies them. There is no oversampling (running the drive at double speed) in the first version: it would cost about +1 point a note. The offline test measures the aliasing of each curve at C5 and C7.

### 2.6 Stability

Every part of the chain is bounded:
- **Series:** there is no feedback loop across the two filters (the drive is outside any loop).
- **SVF types:** stable at any setting (CONFIRMED in SPEC 2.5 for the same filter).
- **LADDER:** its feedback goes through tanh, so it is bounded.
- **COMB:** the feedback is capped below 100 % and damped.
- **FM:** the swing is capped.
- **Output:** a soft knee at the note's full level, as `uf_run` has.

The test plan (section 5) checks RES 127 + DRIVE 127 + loud chords, and one minute of loud noise with every value jumping each block (the Clipper `filter_test.c` idea, CLIPPER_STUDY).

---

## 3. How it fits DUELL

### 3.1 Stored values and ids

| Value | Range | First version? |
|---|---|---|
| TYPE | OFF, LP, BP, HP, NOTCH, LADDER, COMB (+ later types, append-only) | yes |
| CUTOFF 2 (TUNE on COMB, VOWEL on FORMANT) | 0..82 dial like CUT; COMB -24..+24 semitones | yes |
| RES 2 (FEEDBACK on COMB) | 0..127; COMB -64..+63 | yes |
| DRIVE | 0..127 | yes |
| ROUTE | SERIES, PARALLEL | yes, if the format room exists |
| BLEND | filter 1 <-> filter 2, -64..+63 (parallel only) | with ROUTE |
| CURVE | SOFT, HARD, FOLD, ASYM | later |
| FM | 0..127 | later |

That is 6 values in the first version and 8 in all. KEY TRK and the cutoff routes are shared with filter 1 (section 2.2), so no tracking value is needed.

**Ids** (CONFIRMED, `core.h`, `params.c`):
- P_COUNT is 127 (the 7-bit limit).
- Four ids are read by nothing today:
  - 25 (ROOT) and 26 (SCALE): kept as placeholders since the scale became global. Careful: the global SCALE page borrows id 26's description, so it would need its own.
  - 42 (ALLOC): voice allocation, never read since T0227.
  - 106 (the retired DUCK NOTE, free again in 0.2.25).
- **Four ids are enough for TYPE, CUTOFF 2, RES 2 and DRIVE: a series-only TWIN FILTER with no format change at all.** The patch record's worst case already counts every id (`upreset.c` `PT_WORST`), so reused ids add 0 bytes.
- **ROUTE and BLEND (and later CURVE, FM) need the extension block** proposed in HEADLINE_FEATURES.md stage A (a short sparse list of extra values in the record, multi and project).
- After the alpha, adding it would need a conversion of saves. So **PARALLEL in the first version means the extension block must be in the alpha format** (question 2).

**Engine slots:** none used. TWIN is part of the voice, not of an engine, so all ten engines get it at once.

### 3.2 Matrix destinations (appended; the list is append-only, 51 of 128 used, CONFIRMED `mod.c`)

| New destination | DEST card (two lines, as FILTER / RES) | Kind | Note |
|---|---|---|---|
| TWIN CUTOFF | TWIN / CUTOFF | per note | adds to filter 2 only (the shared routes already move both) |
| TWIN RES | TWIN / RES | per block | as FRES |
| TWIN DRIVE | TWIN / DRIVE | per block, gliding | VEL here brings back "harder notes distort more" |
| TWIN BLEND | TWIN / BLEND | per block, gliding | parallel only (hidden in SERIES) |
| later: TWIN FM | TWIN / FM | per block | |

The existing destinations keep their meaning. CUT stays the main cutoff route, which now also reaches filter 2 by the link.

### 3.3 MORPH

- **CUTOFF 2, RES 2, DRIVE, BLEND, FM:** continuous; they blend along the fader like every value. CUTOFF 2 blends in octaves, as CUT does.
- **TYPE, ROUTE, CURVE:** never rolled in SMART, as INSERT TYPE (rule 3, MK_EX). The type is the sound's identity.
  - In WILD, and later with editable B, they may differ between the ends.
  - Then they **switch at the middle of the fader**, with both types running for about 8 blocks (6 ms) while the outgoing one fades and the incoming one starts from rest. No click.
  - The double cost lasts only during those 8 blocks.
  - Designed now, so editable B does not force a redesign.
- **SMART safety rules** (copies of today's):
  - CUTOFF 2 never lands in a dead or silent band (as `morph_uf_safe`);
  - a roll that lifts RES 2 above 100 does not also lift DRIVE (ANALOG's existing rule);
  - DRIVE never rises far above the original (MF_RISE, loudness).
- **TWIN OFF is never switched on by a roll**, unlike the MULTI FILTER's half-chance exception. It is a deliberate design choice of the sound.

### 3.4 DUEL, STEREO, LAYER, two cores

- Everything is per note and per part, so all three modes behave alike.
- **LAYER** plays each key on both parts, so with TWIN on both parts a key pays twice. The NOTES budget counts it.
- **Two cores:** part 2's notes run on core 1 with their own state. The comb lines sit in the part's own engine memory. Nothing is shared between the cores, so the per-part split is unaffected.
- **PHYS** keeps its one-part rule; TWIN adds about 13 % to a PHYS note (est.), so PHYS stays at 2 to 3 notes.

### 3.5 Hide-inert rules (owner's rule: only a selection hides, a value never does)

- TYPE OFF: only the TYPE card shows; the second page (below) is skipped.
- ROUTE SERIES: BLEND is hidden.
- COMB: the labels change to TUNE and FEEDBACK. Later, FM is hidden on COMB and FORMANT.
- COMB is not in the TYPE list on WAVE and PHYS (no memory for its lines, section 4.3). The list skips it there, as the matrix list skips inert destinations.
- Nothing fades for being at 0.

### 3.6 The user interface: options for you to approve

EDIT's round today (CONFIRMED, `params.c` PAGES): ENGINE, EDIT 1, EDIT 2, [VCF / TRIO FILTER / NOISE FILT], MULTI FILTER 1/2, MULTI FILTER 2/2, [FM6 pages], VOICE 1/2, VOICE 2/2. TWIN would come right after MULTI FILTER 2/2, in signal order.

**Option A (recommended): two pages, the playing knobs together**

| Page | K1 | K2 | K3 | K4 |
|---|---|---|---|---|
| TWIN FILTER | TYPE | CUTOFF (TUNE on COMB) | RES (FEEDBACK on COMB) | DRIVE |
| TWIN ROUTE (skipped while TYPE is OFF) | ROUTE | (later: CURVE) | (later: FM) | BLEND (hidden in SERIES) |

- The Virus gesture (cutoff against drive) is on one page.
- The empty K2 and K3 on TWIN ROUTE are kept empty on purpose, so that CURVE and FM can arrive later without moving BLEND (your "never rearrange neighbours" rule). BLEND is a mix, so it is last.
- Graph area: the signal flow drawn small (ENGINE > FILTER 1 > DRIVE > FILTER 2 > AMP, or the split for PARALLEL), with filter 2's curve. This uses the empty middle of the screen, as the EQ curve does.

**Option B: one page only (series-only plan B)**
- TWIN FILTER: TYPE, CUTOFF, RES, DRIVE.
- No ROUTE, no BLEND, no second page.
- Fits the 4 free ids.

**Option C: other names**
- "FILTER 2" instead of TWIN. It reads close to "MULTI FILTER 2/2" (page 2 of filter 1), which is why I prefer TWIN.
- Renaming the MULTI FILTER pages "FILTER 1 1/2" / "1 2/2" would fix it but changes pages you know.

**Labels, checked against today's words** (CONFIRMED, `params.c`, `ui_draw.c`):
- BAL is taken: it is the MODE page's part balance. So the mix of the two filters is **BLEND**, a word not used anywhere today.
- CURVE is taken by DUCK's release shape, so the later curve choice needs another word. Proposal: **CLIP**, with SOFT / HARD / FOLD / ASYM.
- DRIVE is already the long name of SAT and of ANALOG's DRV, but it means the same thing (a drive amount of the stage on that page), so I would keep it.
- "COMB" is also the name of one WAVE table (a comb-like spectrum). That is the same meaning, so I would keep it too. Your call.

**Editor:**
- one card "TWIN FILTER" next to the MULTI FILTER card, grouped by function (your editor rule), with the same small flow picture;
- the ROUTE switch draws the series or parallel picture;
- hidden values are hidden there too.

---

## 4. CPU and RAM

### 4.1 Cost per note (est., calibrated on the measured MULTI FILTER)

**How the estimates were made.**
- The calibration is today's 12 dB MULTI FILTER: measured at 46.6 host instructions a sample, of which about 30 are the filter itself and the rest is the scratch block and the soft knee.
- The new parts were counted against that, operation by operation, from their code shape.
- 33 instructions a sample = 1 point.
- Code in RAM runs about 0.85 x, code in flash about 1.1 x (docs/TESTING.md factors).
- None of this is measured.

| Piece | host instructions a sample (est.) | points per note (est.) |
|---|---|---|
| the TWIN path itself (scratch, the amp after the chain), filter 1 already running | 6 to 10 | 0.2 to 0.3 |
| ... filter 1 bypassed (still skipped, but the scratch is new) | 14 to 18 | 0.4 to 0.5 |
| DRIVE (SOFT) | 12 to 16 | 0.4 to 0.5 |
| LP / BP / HP / NOTCH | 30 to 36 | 0.9 to 1.2 |
| LADDER | 45 to 55 | 1.4 to 1.8 |
| COMB | 16 to 22 (+ clearing its line at a note start) | 0.5 to 0.7 |
| PARALLEL instead of SERIES | +2 to 4 | +0.1 |
| later: FORMANT / MS-style / CHIP | 55 to 65 / 32 to 40 / 30 to 38 | 1.7 to 2.1 / 1.0 to 1.3 / 0.9 to 1.2 |
| later: FM | 10 to 16 | 0.3 to 0.5 |

**Typical first-version settings, on top of today's note:**

| Setting | Extra per note | Notes for a part that plays 8 today (est.) |
|---|---|---|
| COMB + DRIVE | +1.1 to 1.5 | about 7 |
| LP + DRIVE (the Virus 36 dB with filter 1 at 24 dB) | +1.5 to 2.0 | 6 to 7 |
| LADDER + DRIVE | +2.0 to 2.6 | about 6 |
| LADDER + DRIVE + FM (later) | +2.3 to 3.1 | 5 to 6 |

**How the right-hand column is worked out** (est.):
- A note today costs about 4.3 to 8.6 points, plus 1.8 for filter 1 (device est., CPU_PLAN).
- Example: ANALOG's mean note is 6.1 + 1.8 = 7.9 points. LADDER + DRIVE makes it about 10.2: 29 % more. The same budget then holds 8 x 7.9 / 10.2 = 6.2 notes.
- The NOTES counter does this arithmetic itself once TWIN has its cost rows. Nothing crackles; the part simply shows fewer notes.

**Against the second core.** The two cores gave about 14 points back on M07 (measured, BENCHR2_RUN1). TWIN on 6 notes of one part costs about the same. Said plainly: TWIN spends most of what the second core won, on the part that uses it.

**A correction to HEADLINE_FEATURES.md.** That file said TWIN costs "+0.5 to 1 a note over a 24 dB sound", because filter 2 could replace the 24 dB's second stage. That is only true if you turn filter 1 down to 12 dB when you add filter 2. Here the two filters are independent, so the full price applies.

### 4.2 RAM per note

- **State:** about 24 to 28 B per note (a union: the SVF types 8 B, LADDER 16 B, COMB 8 B, plus the drive and blend glides and the type of the last block).
- A note slot grows from 124 B to about 150 B. Over the 4 tracks x 8 slots that is about 0.8 to 0.9 KB of variable RAM.
- The headroom is 6,220 B against a rule of 4,096 B (CONFIRMED, MERGE_JOIN1), so it fits.
- It would be half that if the state lived only with the two parts that play.

### 4.3 The comb lines: the engines' memory that is already there (CONFIRMED, `engines.c`)

**The memory.**
- Every part has an engine memory region of 12,888 B, sized for PHYS.
- ANALOG, TRIO, NOISE, LOFI, PHASE, VOICE and WHEEL do not use it at all.
- FM6 uses about 1.8 KB (6 notes of about 304 B).
- WAVE uses about 12.3 KB (its 19 frames). PHYS uses all of it.

**How long the lines can be.**
- On the seven engines that leave it empty, the region gives each of 8 notes a line of about 800 samples.
- That rings down to about 55 Hz (A1); with 10 notes a part, about 68 Hz (C#2).
- A lower note gets its comb an octave up. That is still in tune (it rings on the note's even harmonics), only a little hollower.
- FM6 leaves about 11 KB, so the same comb works there.

**So COMB needs no new RAM on eight engines, and is left out on WAVE and PHYS.** PHYS is a resonator already.

This answers the first pass's "a per-note comb is 13 to 47 KB: impossible". It is possible, because the memory already exists and sits empty.

### 4.4 Code and where it must live

- **Core-1 code:** about 1.2 to 1.5 KB (est.). That covers the chain and the amp after it, the drive, the SVF types (a copy of the filter loop with its own state: `uf_run` itself stays untouched), LADDER and COMB.
- **Elsewhere, in flash** (not run by core 1): about 1 to 1.5 KB for the pages, morph rules, matrix and formats.

**Where it must sit** (CONFIRMED, MERGE_JOIN1):
- Core 1 runs only from RAM. The new code goes in range Z, in its own functions outside the hot block.
- Range Z shares room with the pool tail, which is 2,840 B on the bench build against a temporary rule of 2,816 B. 1.5 KB more does not fit until the RAM shrink lands.
- The hot block is exactly at its 228 B limit, and the one branch that sends a note into the TWIN path must sit in `track_render` (hot): about 16 to 24 B. That also needs the shrink, or a cold piece moved out of the hot block.

### 4.5 What must be measured on the bench before committing

- **Host cost rows** (the `uf_test` method): every TYPE x SERIES / PARALLEL x DRIVE 0 / 64 / 127, with and without filter 1 running. These replace every "est." above and become the NOTES cost rows (measured worst + 10 %).
- **Stability:**
  - RES 127 + DRIVE 127 + a full-level chord on every type: no runaway, no DC, peak bounded;
  - one minute of noise with all values jumping each block;
  - COMB at FEEDBACK +-max.
- **Aliasing** of each drive curve at C5 and C7, the same measure that gave WAVE its -48 dB median.
- **Clicks:** a jump of every new value and TYPE switches while notes sound, using the existing step-under-modulation tests.
- **On the unit:** the per-stage timer for TWIN, and a recorded bass scene for clicks and gaps, once with TWIN on part 2 (core 1).

---

## 5. What to prove first, cheaply

The idea: hear it before building it, with DUELL's own filter code, on your patches, against your Snow. This is phase 0 and phase 3 of your Virus feasibility note, cut down to the twin filter.

### Step 1: your Snow, about 30 to 45 minutes

Recorded dry at 44.1 kHz, as in the research note.
- **Three reference patches you name.** The research proposes a driven bass, a sync lead and a resonant filter-sweep pad. Notes C2, C3, C4 and one held chord each, with every setting written down.
- **Where the drive sits:**
  - one note with a slow amp attack and high saturation. If the distortion stays the same while the note swells, the saturation is before the amp;
  - one long release with the filter closing during it.
- **The saturation curves:** a sine through each saturation type at 0, 64 and 127 (gives the curves).

### Step 2: an offline renderer (no unit, no emulator)

- A small program built from DUELL's own computer-side code (the engines and `track_render`, as the tests use them), plus a prototype TWIN stage with **all** candidate types, curves and FM.
- It writes WAV files of the three DUELL patches you name, set as close to the Snow patches as you like. Each patch is rendered:
  - as it is today;
  - with LP + DRIVE;
  - with LADDER + DRIVE;
  - with COMB;
  - with two BPs in parallel;
  - with FORMANT;
  - with FM on and off.
- The same run gives the cost row of each type, the stability tests and the aliasing numbers.

### Step 3: a blind listening page

- For each patch: the Snow, DUELL today and DUELL with its best TWIN setting, in random order, 5 trials.
- You pick the closest to the Snow and the one you like best.
- Plus a free page of all the TWIN versions, to choose the TYPE list.

### The gates

| Gate | Passes when | If it fails |
|---|---|---|
| G1 taste | you keep at least one TWIN version on 2 of the 3 patches | stop; the feature is not worth 2 notes |
| G2 Snow | a TWIN version is closer to the Snow than today's DUELL on 2 of 3 (informative: G1 decides) | check the drive curve and the pre-amp order first |
| G3 cost | the first-version set costs at most +2.5 points a note (host rows) | drop LADDER to the later list, or ship series-only |
| G4 stability | no runaway, DC or level beyond the soft knee in any test | fix before anything else |

The types that pass G1 become the first-version list; FM goes in only if you choose it here.

Effort: step 2 is about 1 to 2 rounds and can be done off the unit; step 3 is 1 round; plus your two sessions (Snow, listening).

---

## 6. Staging, effort, risks, plan B

### 6.1 Stages

| Stage | What | Effort (est.) | Depends on |
|---|---|---|---|
| 0. Proof | Section 5 | 2 to 3 rounds + your 2 sessions | nothing |
| A. Format room | the extension block in the alpha format (only if PARALLEL is wanted in the first version) | 2 to 3 rounds (already planned in HEADLINE_FEATURES.md) | your answer to question 2 |
| **1. TWIN FILTER (the headline)** | all ten engines; TYPES LP BP HP NOTCH LADDER COMB; DRIVE SOFT; SERIES / PARALLEL + BLEND; pre-amp chain; linked cutoff; four matrix destinations; morph rules; NOTES rows; two pages + editor card after your layout approval | about 4 to 6 rounds + 2 of your sessions (layout, by ear) | 0 passed; the RAM shrink; A for PARALLEL |
| 2. Additions | FILTER FM (SELF); CLIP curves; FORMANT; MS-style; CHIP: each only if it won in stage 0 | 1 to 2 rounds each | 1 |
| Later | DIODE; FM from a sine at the note's pitch; double-speed drive against aliasing | as needed | |

**What is already a headline at stage 1:** a second filter with drive, in series or parallel, on FM6, WAVE, VOICE and WHEEL as much as on ANALOG. "A DX bass through a driven ladder" is a 30-second demo no FM-1 firmware can make today. Clipper's twin filters live in one engine (CONFIRMED, CLIPPER_STUDY).

**Off the unit and on it** (the questions ask; kept short):
- the DSP, the routing, the morph rules, the matrix and the format can all be built and tested in simulation;
- the RAM placement, the timing on the unit, the recorded scenes and the pages need the unit.

### 6.2 Risks

1. **RAM on core 1** (the real limit). The 1.2 to 1.5 KB do not fit until the shrink lands; the hot branch needs about 20 B that do not exist today.
2. **Patch format.** PARALLEL needs the extension block before the alpha, or a conversion later.
3. **Sound change.**
   - None with TWIN OFF.
   - With TWIN on, filter 1 runs before the amp, so its soft knee acts on the full level.
   - The drive no longer follows velocity unless you route VEL to it.
4. **CPU.** About 1 to 2 notes fewer per part where TWIN is on. The NOTES counter shows it honestly.
5. **Stability at the extremes.** Bounded by design (2.6), and still the first thing tested.
6. **Aliasing.** Drive and COMB on high notes. FOLD is the worst; SOFT after a low-pass filter 1 is mild. Measured in stage 0.
7. **UI pressure.** EDIT's round grows by 1 page (2 with ROUTE). TWIN ROUTE is skipped while TYPE is OFF.
8. **The comb's memory.** Correct only while no engine except WAVE, PHYS and FM6 claims its region. A future engine that uses the region must leave room, or hide COMB.

### 6.3 Plan B (each smaller than the last)

1. **Series only, on the 4 free ids.** TYPE, CUTOFF 2, RES 2, DRIVE on one page. No format change, no extension block.
2. **Without LADDER and COMB:** the SVF types reuse DUELL's filter code. The core-1 code shrinks to about 0.6 to 0.8 KB, and the cost stays at about +1.5 to 2 points a note.
3. **DRIVE 2 only** (the second pass's cheap form): a drive between the engine's own filter and the MULTI FILTER. About +0.5 a note, one value. It is not the Virus voice, but it gives the snarl on ANALOG, TRIO and NOISE.

---

## 7. Questions for you (each with my recommendation)

1. **Filter 1 = today's MULTI FILTER on every engine, and the engines' own filters stay inside their engines, unchanged.** Their characters come back as copies in filter 2's TYPE list: LP / BP / HP from ANALOG's VCF, NOTCH (and later CHIP) from TRIO. This replaces lifting them out, which would change every ANALOG, TRIO and NOISE sound and give no new filter sound.
   **Recommended: yes.**
2. **Should the first version have PARALLEL with BLEND?** If yes, the patch format needs the extension block before the alpha. If not, a series-only TWIN fits the 4 free ids with no format change, and PARALLEL needs the block later.
   **Recommended: yes, PARALLEL in the first version, with the room reserved in the alpha format.** If the alpha cannot take it, series-only first.
3. **The first TYPE list: OFF, LP, BP, HP, NOTCH (12 dB), LADDER (24 dB) and COMB** (tuned to the note, + and - feedback; not on WAVE and PHYS). FORMANT, MS-style, CHIP, the drive curves and FILTER FM are decided by your ears in the offline test.
   **Recommended: yes.**
4. **The TWIN chain runs before the amp (the Virus way), and filter 2 follows the same envelopes, LFOs and KEY TRK as filter 1 (linked), with its own matrix destination to move it alone.** Then the drive no longer depends on how loud or late in its envelope a note is; route VEL to DRIVE where you want it to.
   **Recommended: yes to both.** Your Snow test (section 5, step 1) checks the pre-amp order.
5. **The pages: Option A.** TWIN FILTER (TYPE, CUTOFF, RES, DRIVE), then TWIN ROUTE (ROUTE, empty, empty, BLEND), after MULTI FILTER 2/2. The second page is skipped while TYPE is OFF. Names TWIN, BLEND, and later CLIP for the curve.
   **Recommended: Option A with these names.** Tell me any word you want different before anything is drawn.

---

## Evidence list

| Claim | Source | Mark |
|---|---|---|
| The MULTI FILTER runs after the engine's render, which already applied the amp | `voice.c` `uf_voice` (engine into a scratch block, then `uf_run`), `track_render` (m.amp0 / amp1 handed to the render) | CONFIRMED |
| Every engine applies the volume ramp inside its own loop | `voice_amp` in ANALOG, TRIO, NOISE, LOFI, VOICE, WHEEL, WAVE, PHYS renders; the ramp written out in `eng_phase.c` and `eng_fm6.c` (m->amp0 / amp1) | CONFIRMED |
| ANALOG's VCF: SVF 12 dB, LP/BP/HP, drive before it, knee 16000, KTR around note 60, TYPE cross-fade in MORPH | `eng_analog.c` `analog_osc`, `analog_tap`, `analog_render_m`, `ANALOG_K` | CONFIRMED |
| TRIO's filter: notch, damping to 0.25, input knee, squeezed band-pass, drift from the noise generator, one sample late | `eng_trio.c` `trio_render`, `TRIO_MIX` | CONFIRMED |
| NOISE's filter: LP to BP morph, level make-up, CRSH and DC blocker after it | `eng_noise.c` `noise_render` | CONFIRMED |
| The three own filters and the MULTI FILTER share the Simper SVF | `dsp.c` `tsvf_coef_k`, `tsvf_lp`; `voice.c` `uf_run` | CONFIRMED |
| ufilt 1 / 2 per engine; routes go to the own filter on ANALOG, TRIO, NOISE | engine tables in `eng_*.c`; `core.h` engine_t.ufilt; SPEC 2.5 | CONFIRMED |
| MULTI FILTER costs +46.6 / +58.6 / +77.3 / +104 host instructions a sample; device est. +1.8 / +2.6 points | SPEC 2.5 (uf_test.c), docs/CPU_PLAN.md section 2 | measured / device est. |
| Most factory sounds run the MULTI FILTER (KEY TRK 32) | `budget.c` comment at BD_F12 | CONFIRMED |
| Voice costs 4.3 to 8.6 points, PHYS 15.7 to 20.8; a note slot 124 B | docs/CPU_PLAN.md section 2 | measured host / device est. |
| Two cores: M07 49.6 to 35.3 % | release_drafts/BENCHR2_RUN1.md | measured |
| Free ids 25, 26, 42, 106; P_COUNT 127 | `core.h` (P_ROOT / P_SCALE note, P_ALLOC), `params.c` (PFREE at 106, ALLOC "never read") | CONFIRMED |
| The global SCALE page borrows id 26's description | `ui_input.c`, `ui_layer.c`, `ui_draw.c` (TP[P_SCALE]) | CONFIRMED |
| The patch record's worst case counts every id (bitmap form) | `upreset.c` PT_W_SEQ / PT_WORST | CONFIRMED |
| Matrix destinations append-only, 51 used | `mod.c` static assert (MD_N == 51) | CONFIRMED |
| Morph kinds: continuous, cross-faded choice, never-rolled (insert type) | `morph.c` header, MK_* | CONFIRMED |
| EDIT page order and the MULTI FILTER 2/2 cards | `params.c` PAGES | CONFIRMED |
| BAL (part balance), CURVE (DUCK), DRIVE as a long name, COMB as a WAVE table name | `ui_draw.c`, `params.c`, `eng_wave.c` | CONFIRMED |
| Engine memory 12,888 B a part; FM6, WAVE, PHYS use it; the others do not | `engines.c`, `eng_fm6.c`, `eng_wave.c` (19 x 646 B frames), `eng_phys.c` | CONFIRMED |
| Hot RAM 228 B free (at its limit), pool tail 2,840 B against a 2,816 B temporary bench rule, RAM headroom 6,220 B, range Z | release_drafts/MERGE_JOIN1.md | CONFIRMED (build numbers) |
| SAT's 1..11 blend rule; TAPE's level-keeping gain | SPEC 2.4 | CONFIRMED |
| Clipper has twin filters only in HYBRID, no comb, no filter FM in HYBRID and dsp.c | release_drafts/CLIPPER_STUDY.md section 3 | CONFIRMED there |
| Virus: saturation after filter 1; filters in series, parallel, split; filter 2 as a 2-pole after a 4-pole | research note, HEADLINE_FEATURES_2 (SOS) | CONFIRMED in those sources |
| Virus filters before its amp; where its saturation sits in parallel; its cutoff link details | not found | HYPOTHESIS (Snow test, step 1) |
| Every cost of TWIN's pieces, RAM per note, code sizes | my operation counts against the measured MULTI FILTER | est. |

**Not verified:**
- every TWIN cost number (no code exists yet);
- whether a per-sample FM correction step stays stable at full swing;
- the Virus's amp order and its parallel drive placement;
- the exact size of FM6's note on the unit (304 B is the computer build's figure);
- whether the pi32v2's divide is cheap enough for an exact per-sample FM fallback.
