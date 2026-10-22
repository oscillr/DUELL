# TWIN FILTER, design 2: the engine filters go, two filters for every engine (2026-10-19)

Private. Research and design only. No code was changed, no branch made. TWIN_FILTER_DESIGN.md (design 1) and INDEX.md were not touched. I wrote no sounds and retuned none. The conversion table in section 1.5 is a mechanical mapping for you to review, not a sound edit. Every page and label is a proposal for you to approve.

Marks: **CONFIRMED** (read today in the code, `wt/join0233`, or the docs), **HYPOTHESIS** (my reading or a web excerpt), **est.** (worked out, not measured), **measured**. CPU in points: 1 point = 1 % of one core's time.

New words (the rest are in GLOSSARY.md or design 1):
- **Link / offset**: with LINK on, filter 2's cutoff follows filter 1's cutoff, and filter 2's own knob is an OFFSET (how far above or below filter 1 it sits). One turn of filter 1's CUTOFF moves both.
- **Converter**: a one-off tool that rewrites the stored values of a sound into the new layout.
- **Damping**: the inverse of resonance inside the filter's math: high damping is soft, low damping rings.

---

## Decided by the owner (2026-10-19)

1. **The engines' own filters go:** ANALOG's VCF, TRIO's filter and NOISE FILT are removed from their engines. The player keeps the same amount of filter: filter 1 (the MULTI FILTER) and filter 2 (TWIN), on every engine. Factory sounds are updated; a sound change is fine as long as the same character can be reached. No backwards compatibility before the alpha.
2. **TRIO's filter stays as a filter-2 option on every engine**, no longer tied to TRIO.
3. **Filter 1 is the MULTI FILTER** (design 1, question 1).
4. **PARALLEL with BLEND in the first version**, so the extension block is reserved in the patch format before the alpha (design 1, question 2).
5. **First TYPE list: OFF, LP, BP, HP, NOTCH, LADDER, COMB** (design 1, question 3).
6. **Filter 2's cutoff is LINKED to filter 1 by default**, the Virus way: its knob is then an OFFSET. Filter 2 has its own four mod amounts (ENV 2, KEY TRK, LFO 1, LFO 2) and its own matrix destination CUTOFF 2. This answers your "different mod options" and "key track, LFO, ENV" (design 1, questions 4 and 5, first part).

What the Virus does (from its manual excerpts):
- **Cutoff Link** is on by default, and filter 2 then follows filter 1 (HYPOTHESIS: Virus TI quickstart excerpt).
- **One Env Amount** moves cutoff 1 and 2 together (HYPOTHESIS: same source). I found no separate amount for filter 2.
- **Filter Balance**: the middle is both filters (in series: the chain), fully left is filter 1 alone (HYPOTHESIS: same source).

So DUELL's design gives filter 2 more of its own than the Virus does: it follows filter 1 like the Virus, and its own four amounts add on top.

---

## Verdict (half a page)

**The plan:**
- The three engine filters come out.
- Their characters land where they fit best:
  - **ANALOG's VCF and NOISE FILT become filter 1.** They are the same filter as the MULTI FILTER (CONFIRMED, `dsp.c` `tsvf_*`), so they convert almost exactly.
  - **TRIO's filter becomes filter 2's LP, BP, HP and NOTCH**, on every engine.
- So the two filters have two distinct characters, as on the Virus:
  - filter 1: clean, wide, rings up to Q 6.9;
  - filter 2: TRIO's gritty one, gentler at the top, with its slow drift.
- LADDER and COMB complete filter 2's list.

**The 43 affected factory sounds and 10 multis convert by a fixed table (section 1.5):**
- ANALOG: 19 sounds. Exact apart from rounding the cutoff to filter 1's coarser dial (about a tenth of an octave at most).
- NOISE: 11 sounds. Exact apart from loudness, which the existing sound audit re-measures.
- TRIO: 13 sounds. Exact apart from the random drift's new sequence.

This needs three small rules (question 2):
- filter 1's resonance gets ANALOG's softer floor;
- ANALOG keeps its DRV (it is an oscillator drive);
- NOISE keeps its loudness rule.

**What it costs:**
- Not CPU-neutral, which I expected it to be. The general filter stage measures about 3 times the cost of the small filter written inside an engine. So ANALOG, TRIO and NOISE notes get about +0.9 to +1.6 points dearer (est.): about 1 note fewer at 8.
- TWIN on top costs what design 1 said (+1.1 to +2.6 a note).
- The modulation of filter 2 is almost free: about +0.05 a note.
- Code: the engines shrink by about 1.0 to 1.4 KB. That may make room for TWIN's 1.5 to 1.8 KB in core 1's engine RAM (HYPOTHESIS).

**Ids:** 11 stored values in all. 5 fit on free ids (four unused today, plus ANALOG's freed TYPE id). The other 6 go in the extension block. The freed engine slots do not give ids: every engine shares those ids.

**The main risk is timing.** Removing the filters changes what ANALOG, TRIO and NOISE patches store. Before the alpha that is free. After it, the firmware itself would have to convert old saves when it loads them (question 4).

---

## 1. Engine by engine, when the own filter goes

### 1.1 Stored values that become free (CONFIRMED, the `.edit` tables)

| Engine | Its 8 slots (E1..E8) | Filter values that leave | What stays | Freed |
|---|---|---|---|---|
| ANALOG | WAVE DTN MIX NOIS **CUT RES** DRV **KTR**, plus TYPE on its own id (`P_FTYPE`, 113) | CUT (E5), RES (E6), KTR (E8), TYPE (id 113) | WAVE DTN MIX NOIS DRV. DRV stays: it drives the oscillator mix before any filter, inside the oscillator loop (the Minimoog's overdriven mixer, per the recipe notes) | slots E5 E6 E8; **id 113** |
| TRIO | WAVE INT2 INT3 DTN **MODE CUT RES** PW | MODE (E5), CUT (E6), RES (E7) | WAVE INT2 INT3 DTN PW | slots E5 E6 E7 |
| NOISE | MODE COLR **FREQ RES TRK** DENS DRFT CRSH | FREQ (E3), RES (E4), TRK (E5) | MODE COLR DENS DRFT CRSH. DRFT keeps moving the clock and level; its cutoff wander moves to filter 1 (1.3) | slots E3 E4 E5 |

**What the freed slots are and are not good for.** E1..E8 are eight ids that every engine shares; only their meaning differs per engine. So a freed slot on ANALOG is not a free id: FM6 still uses that id. The freed slots become:
- empty cards on those three engines (hidden);
- room for future engine controls (for example a third oscillator level on ANALOG).

The only id truly freed is ANALOG's TYPE (113).

### 1.2 Routes and destinations: the precise mapping

Today on ANALOG, TRIO and NOISE every cutoff route drives the engine's own filter. The MULTI FILTER after it takes only its CUT knob and KEY TRK (CONFIRMED, `voice.c` `track_render`, the ufilt 2 branch). After the change, these three engines behave like the other seven (ufilt 1): the routes drive filter 1, and filter 2 through LINK.

| Old route or control (ANALOG / TRIO / NOISE) | Drove | New target, ANALOG and NOISE | New target, TRIO |
|---|---|---|---|
| AMP DEST FLT, ENV 2 FLT, LFO 1 FLT, LFO 2 FLT (`P_ED_FLT`, `P_E2FLT`, `P_LD_FLT`, `P_L2FLT`) | the engine's cutoff | filter 1's same four depths (the same ids), depth x 0.65 | filter 2's own ENV 2 / LFO 1 / LFO 2 amounts (x 0.65); filter 1's set to 0 (AMP FLT: a matrix route AENV > CUTOFF 2) |
| matrix CUT (any source: AT, VEL, LFO ...) | the engine's cutoff | CUT (filter 1, and filter 2 through LINK), AMT x 0.65 | CUTOFF 2 (the new destination), AMT x 0.65 |
| matrix to the engine's RES slot (ANALOG E6 "RES", NOISE E4) | the engine's resonance | FILTER RES (destination 38, filter 1), the same AMT on ANALOG, x 1.03 on NOISE | TWIN RES |
| matrix to E5..E8 that were CUT / KTR / MODE / FREQ / TRK | those slots | CUT; MODE goes to FILTER MODE (46); KTR and TRK have no destination (none in the factory sounds) | MODE: none needed (filter 2's TYPE) |
| engine KTR (ANALOG) / TRK (NOISE) | the engine's key follow | filter 1's KEY TRK (id 27): ANALOG KTR x 0.211, NOISE TRK x 0.496 | (TRIO had no key follow) filter 2 KEY TRK 0 |
| the accent (velocity over 110 opens the filter with the envelope) | the engine's filter | filter 1, and filter 2 through LINK. Proposed rule for every engine whose filter was removed: the accent lifts **both** filters, so TRIO keeps it with LINK off | both filters |
| CC74 / CC71 | the engine's CUT / RES | filter 1's CUT / RES, as on the other seven | the same; on a converted TRIO sound filter 1 is open, so CC74 then opens or closes filter 1 only |
| FILTER RES (38), FILTER MODE (46) | the MULTI FILTER, usually open on these engines, so almost inert | filter 1, now the main filter | filter 1 |

**Why 0.65.** The engines' cutoff runs on a scale of 127 steps (0.071 octave each). The MULTI FILTER's dial has 82 steps (0.11 octave each; CONFIRMED in `core.h`, one dial step = 1.547 table steps). A route depth means steps, so 256 / 396 = 0.646 keeps the same sweep in octaves.

**Key follow factors** (CONFIRMED arithmetic from the code):
- ANALOG's KTR 127 is 42.5 % of full key follow, and KEY TRK 63 is 100 %, so ANALOG KTR x 63 / 299.
- NOISE's TRK 127 is 100 %, so TRK x 63 / 127.
- All three centre on the same note, MIDI 60.

### 1.3 What each removed filter had that is engine-specific

| Engine | Character piece | Where it goes |
|---|---|---|
| ANALOG | a Simper SVF, 12 dB, LP / BP / HP | filter 1 (MODE 0 / 64 / 127, SLOPE 12 dB). The same filter |
| | damping 2.0 (soft) to 0.145 | filter 1's damping today runs 1.414 to 0.145. **Proposal: widen it to 2.0 at the bottom (question 2)**: every ANALOG RES then converts 1 : 1 |
| | DRIVE before the filter | stays in ANALOG (DRV). Exact |
| | soft knee at 16,000, before the amp | filter 1's knee at 48,000, before the amp (pre-amp chain). At full level the two land at almost the same place (16,000 inside the engine is about 46,900 at the voice's level). Close, not proven: measure |
| | BP level (ANALOG's band-pass peaks like its LP) | filter 1's BP is level-normalised. No factory ANALOG sound uses BP (all LP, CONFIRMED); a user BP sound changes level |
| TRIO | its SVF with an input that saturates above half scale, a band-pass state squeezed above +-49,152 (grit at loud resonance), damping only down to 0.25 (never sings), modes LP / BP / HP / NOTCH | **filter 2's LP, BP, HP, NOTCH, as they are** (section 2) |
| | a slow random cutoff drift, drawn from the note's noise generator, which the pitched-noise oscillator shares | kept in filter 2's types, with a random generator of its own. **Lost: the exact random sequence**, so TRIO's NOIS sets are no longer bit for bit |
| | runs one sample late (its step corrections) | that is the oscillators' affair and stays in TRIO. Nothing lost |
| NOISE | the SVF morphing from LP to a narrow BP as RES rises | filter 1: MODE = RES x 64 / 127 (LP towards BP), RES as damping. The same filter, the same blend form |
| | damping 2.0 to 0.10 | NOISE RES 117 and above clamps at filter 1's 0.145 (none in the factory sounds: the highest is 112) |
| | a loudness make-up that follows the cutoff, the colour and the mode (up to x16) | **stays in NOISE**, now reading filter 1's cutoff, which the voice already computes before the engine renders (a small hand-over per block). Without it, a noise swept low gets much quieter |
| | the band-pass level scaled by 1.5 times the square root of the damping | filter 1's BP is level-normalised, so resonant noise sounds change level (SEA ORGAN by about 8 dB, est.). The PATCH LEVEL is re-measured by the existing sound audit |
| | DRFT also wanders the cutoff (up to +-16 semitones, about 0.7 s lag) | the engine hands the wander to filter 1, one block late (inaudible at that speed) |
| | CRSH (sample and hold, fewer bits) **after** the filter | now before filter 1, so the filter smooths the steps. No factory sound loses anything: every ANLG / DUST sound has CRSH 0, and on LFSR / META that slot is CLK |
| | META's DC blocker | stays in NOISE |

### 1.4 The factory sounds affected (CONFIRMED: `tools/gen_duell_sounds.py --check`, today's numbers)

| Engine | Sounds |
|---|---|
| ANALOG, 19 | #01 BEEHIVE, #02 FOLD BASS, #03 LADDER BASS, #04 RUBBER BAND, #05 SQUELCH, #06 TIDAL SUB, #25 FUNFARE, #26 RAZORWIRE, #27 SQUARE LEAD, #28 TORCH SONG, #29 WALL OF SAW, #44 EMBER GLOW, #45 SILK CHORUS, #46 VELVET BOW, #65 ARP PLUCK, #66 PIZZICATO, #79 SFORZANDO, #102 STORM FRONT, #117 GLASS SHARDS |
| TRIO, 13 | #15 BREADBIN, #16 SID BASS, #17 SYNC BASS, #34 BANSHEE, #35 OBOE-ISH, #53 CHIP CHAPEL, #54 STRING MACH, #73 PEBBLE POP, #86 TIN TOLL, #97 FIELD CLANK, #108 CIRCUIT BEND, #123 DIAL TONE, #124 GRAVEL SNARE |
| NOISE, 11 | #20 GEARBOX, #38 HORNET, #57 SEA ORGAN, #99 OPEN SIZZLE, #100 STATIC SNARE, #101 TIZZ HAT, #112 MOOR WIND, #113 REV CYMBAL, #114 WHITE OUT, #127 GEIGER, #128 MODEM |

**43 of 128 sounds.**
- None sets the MULTI FILTER (all at its open default), and every ANALOG sound uses TYPE LP (CONFIRMED).
- Their MORPH End B seeds roll different values after the change, so the seeds are re-picked by the sound audit tool, as was done in 0.2.9.

**Multis with such a part (CONFIRMED, `tools/factory_multis.py`, `owner_multis.py`):**
- M01 REESE SWAMP (#05 SQUELCH)
- M03 PAD SPLIT (#44 EMBER GLOW; the multi opens its CUT to 71)
- M04 DUB CHORD (#06 TIDAL SUB)
- M06 TINE STRINGS (#54 STRING MACH)
- M07 FINAL REEL (#54 STRING MACH)
- M08 HAZY TENSOR (#03 LADDER BASS, #20 GEARBOX)
- M09 ACID ETHER (#04 RUBBER BAND, #03 LADDER BASS)
- M10 WIRED SNAIL (#79 SFORZANDO, #117 GLASS SHARDS)
- M12 DEAD BUFFER (#128 MODEM)
- M14 GARGLE ACID (#05 SQUELCH)

That is **10 multis, 13 parts**. A multi stores its own copy of the part's values, so the converter must rewrite the records, not only the sounds. **Eight of the ten are marked as yours** (M03, M04, M06, M07, M08, M09, M12, M14), and the rule so far is that your multis are never changed. Question 2 asks for an exception: convert them with the same table, and you listen before anything ships.

### 1.5 Conversion table (for your review; old values -> new)

**Rules:**
- ANALOG: own filter -> filter 1, filter 2 OFF.
- NOISE: own filter -> filter 1, filter 2 OFF.
- TRIO: own filter -> filter 2 (TYPE = its MODE, LINK OFF), filter 1 open (bypassed) with its depths and KEY TRK at 0.
- Cutoff: dial = (256 x CUT - 40) / 396, rounded.
- Depths x 0.65.
- RES 1 : 1 if question 2 is accepted (NOISE x 1.03).
- DRIVE 0 and ROUTE SERIES everywhere; BLEND at its middle (the chain).

| Sound | Old filter | New | Routes (old -> new) |
|---|---|---|---|
| #01 BEEHIVE | CUT 99 RES 30 KTR 64 | F1 LP CUT 64 RES 30 KEY TRK 13 | ENV 2 10 -> 6; AT > RES 13 -> FILTER RES 13 |
| #02 FOLD BASS | CUT 60 RES 20 DRV 20 KTR 64 | F1 LP CUT 39 RES 20 KEY TRK 13; DRV 20 stays | ENV 2 22 -> 14 |
| #03 LADDER BASS | CUT 32 RES 12 DRV 24 KTR 64 | F1 LP CUT 21 RES 12 KEY TRK 13; DRV 24 | ENV 2 26 -> 17; AT > CUT 16 -> 10; VEL > CUT 10 -> 6 |
| #04 RUBBER BAND | CUT 32 RES 48 DRV 18 KTR 64 | F1 LP CUT 21 RES 48 KEY TRK 13; DRV 18 | ENV 2 61 -> 39; AT > CUT 16 -> 10; VEL > CUT 13 -> 8 |
| #05 SQUELCH | CUT 40 RES 95 DRV 30 KTR 64 | F1 LP CUT 26 RES 95 KEY TRK 13; DRV 30 | ENV 2 32 -> 21; AT > CUT 19 -> 12; VEL > RES 8 -> FILTER RES 8 |
| #06 TIDAL SUB | CUT 60 RES 0 DRV 16 KTR 64 | F1 LP CUT 39 RES 0 KEY TRK 13; DRV 16 | ENV 2 6 -> 4 |
| #25 FUNFARE | CUT 56 RES 18 KTR 64 | F1 LP CUT 36 RES 18 KEY TRK 13 | ENV 2 50 -> 32; AT > CUT 19 -> 12; VEL > CUT 16 -> 10 |
| #26 RAZORWIRE | CUT 95 RES 20 DRV 36 KTR 64 | F1 LP CUT 61 RES 20 KEY TRK 13; DRV 36 | ENV 2 6 -> 4 |
| #27 SQUARE LEAD | CUT 84 RES 16 KTR 64 | F1 LP CUT 54 RES 16 KEY TRK 13 | ENV 2 16 -> 10; AT > CUT 16 -> 10; VEL > CUT 10 -> 6 |
| #28 TORCH SONG | CUT 95 RES 8 KTR 127 | F1 LP CUT 61 RES 8 KEY TRK 27 | ENV 2 16 -> 10; AT > CUT 16 -> 10; VEL > CUT 10 -> 6 |
| #29 WALL OF SAW | CUT 95 RES 10 KTR 64 | F1 LP CUT 61 RES 10 KEY TRK 13 | ENV 2 13 -> 8; AT > CUT 16 -> 10; LFO 2 > CUT 3 -> 2 |
| #44 EMBER GLOW | CUT 55 RES 18 KTR 127 | F1 LP CUT 35 RES 18 KEY TRK 27 | ENV 2 19 -> 12; AT > CUT 16 -> 10; LFO 2 > CUT 5 -> 3 |
| #45 SILK CHORUS | CUT 60 RES 10 KTR 64 | F1 LP CUT 39 RES 10 KEY TRK 13 | ENV 2 10 -> 6; AT > CUT 16 -> 10 |
| #46 VELVET BOW | CUT 70 RES 0 KTR 64 | F1 LP CUT 45 RES 0 KEY TRK 13 | ENV 2 7 -> 5; AT > CUT 13 -> 8; LFO 1 > CUT 6 -> 4 |
| #65 ARP PLUCK | CUT 44 RES 26 KTR 64 | F1 LP CUT 28 RES 26 KEY TRK 13 | ENV 2 42 -> 27; VEL > CUT 13 -> 8 |
| #66 PIZZICATO | CUT 50 RES 20 KTR 64 | F1 LP CUT 32 RES 20 KEY TRK 13 | ENV 2 42 -> 27; AT > CUT 13 -> 8; VEL > CUT 16 -> 10 |
| #79 SFORZANDO | CUT 45 RES 15 KTR 64 | F1 LP CUT 29 RES 15 KEY TRK 13 | ENV 2 48 -> 31; AT > CUT 16 -> 10; VEL > CUT 16 -> 10 |
| #102 STORM FRONT | CUT 56 RES 30 KTR 0 | F1 LP CUT 36 RES 30 KEY TRK 0 | ENV 2 48 -> 31; AT > CUT 16 -> 10 |
| #117 GLASS SHARDS | CUT 70 RES 90 DRV 127 KTR 64 | F1 LP CUT 45 RES 90 KEY TRK 13; DRV 127 | ENV 2 19 -> 12; AT > CUT 19 -> 12 |
| #15 BREADBIN | LP CUT 58 RES 41 | F2 LP CUTOFF 37 RES 41 | ENV 2 29 -> F2 ENV 2 19; AT / VEL > CUT 16 / 10 -> CUTOFF 2 10 / 6 |
| #16 SID BASS | LP CUT 72 RES 24 | F2 LP CUTOFF 46 RES 24 | ENV 2 26 -> F2 17; AT / VEL > CUTOFF 2 10 / 6 |
| #17 SYNC BASS | LP CUT 62 RES 30 | F2 LP CUTOFF 40 RES 30 | ENV 2 22 -> F2 14; AT / VEL > CUTOFF 2 10 / 6 |
| #34 BANSHEE | LP CUT 66 RES 30 | F2 LP CUTOFF 43 RES 30 | ENV 2 10 -> F2 6; VEL > CUTOFF 2 6 |
| #35 OBOE-ISH | LP CUT 96 RES 27 | F2 LP CUTOFF 62 RES 27 | ENV 2 7 -> F2 5; VEL > CUTOFF 2 7 |
| #53 CHIP CHAPEL | BP CUT 66 RES 85 | F2 BP CUTOFF 43 RES 85 | ENV 2 13 -> F2 8; AT > CUTOFF 2 10; LFO 1 > CUTOFF 2 2 |
| #54 STRING MACH | LP CUT 103 RES 11 | F2 LP CUTOFF 66 RES 11 | ENV 2 7 -> F2 5; AT > CUTOFF 2 8; LFO 2 > CUTOFF 2 2 |
| #73 PEBBLE POP | LP CUT 74 RES 28 | F2 LP CUTOFF 48 RES 28 | ENV 2 35 -> F2 23; AT / VEL > CUTOFF 2 10 / 10 |
| #86 TIN TOLL | LP CUT 108 RES 22 | F2 LP CUTOFF 70 RES 22 | ENV 2 16 -> F2 10; AT / VEL > CUTOFF 2 8 / 10 |
| #97 FIELD CLANK | BP CUT 91 RES 27 | F2 BP CUTOFF 59 RES 27 | ENV 2 19 -> F2 12; AT / VEL > CUTOFF 2 8 / 10 |
| #108 CIRCUIT BEND | LP CUT 92 RES 45 | F2 LP CUTOFF 59 RES 45 | ENV 2 19 -> F2 12; LFO 2 19 -> F2 LFO 2 12; AT > CUTOFF 2 10 |
| #123 DIAL TONE | LP CUT 69 RES 0 | F2 LP CUTOFF 45 RES 0 | AT > CUTOFF 2 7 -> 5 |
| #124 GRAVEL SNARE | LP CUT 100 RES 30 | F2 LP CUTOFF 65 RES 30 | ENV 2 26 -> F2 17; AT / VEL > CUTOFF 2 8 / 10 |
| #20 GEARBOX | FREQ 52 RES 28 TRK 127 | F1 MODE 14 CUT 34 RES 29 KEY TRK 63 | ENV 2 26 -> 17; AT > CUT 10; VEL > CUT 6 |
| #38 HORNET | FREQ 104 RES 22 TRK 127 | F1 MODE 11 CUT 67 RES 23 KEY TRK 63 | ENV 2 13 -> 8; VEL > CUT 6 |
| #57 SEA ORGAN | FREQ 72 RES 112 TRK 127 | F1 MODE 56 CUT 46 RES 115 KEY TRK 63 | ENV 2 10 -> 6; LFO 2 6 -> 4; AT > CUT 8 |
| #99 OPEN SIZZLE | FREQ 117 RES 41 TRK 0 | F1 MODE 21 CUT 76 RES 42 KEY TRK 0 | AT / VEL > CUT 8 / 10 |
| #100 STATIC SNARE | FREQ 61 RES 71 TRK 0 | F1 MODE 36 CUT 39 RES 73 KEY TRK 0 | ENV 2 45 -> 29; AT / VEL > CUT 8 / 10 |
| #101 TIZZ HAT | FREQ 116 RES 52 TRK 0 | F1 MODE 26 CUT 75 RES 53 KEY TRK 0 | ENV 2 10 -> 6; AT / VEL > CUT 8 / 10 |
| #112 MOOR WIND | FREQ 44 RES 70 TRK 0 | F1 MODE 35 CUT 28 RES 72 KEY TRK 0 | ENV 2 8 -> 5; LFO 2 19 -> 12; AT > CUT 10; LFO 1 > RES 6 -> FILTER RES 6 |
| #113 REV CYMBAL | FREQ 107 RES 21 TRK 17 | F1 MODE 11 CUT 69 RES 22 KEY TRK 8 | ENV 2 20 -> 13; AT > CUT 7 |
| #114 WHITE OUT | FREQ 34 RES 40 TRK 0 | F1 MODE 20 CUT 22 RES 41 KEY TRK 0 | ENV 2 63 -> 41; AT > CUT 10 |
| #127 GEIGER | FREQ 98 RES 92 TRK 0 | F1 MODE 46 CUT 63 RES 94 KEY TRK 0 | LFO 2 13 -> 8 |
| #128 MODEM | FREQ 110 RES 30 TRK 127 | F1 MODE 15 CUT 71 RES 31 KEY TRK 63 | ENV 2 13 -> 8; AT > CUT 10 |

(Unlisted values are the engine's defaults, which the table shows filled in: ANALOG CUT 90 RES 30 DRV 0 KTR 64; TRIO LP CUT 80 RES 40; NOISE FREQ 80 RES 30 TRK 64.)

**Where the result is not exact (est.):**
- the cutoff rounding: at most half a dial step, 0.055 octave;
- the depth rounding: up to about 8 % on the smallest depths;
- NOISE loudness (re-measured);
- TRIO's drift sequence;
- the old MULTI FILTER's KEY TRK 32 on the non-bass sounds, which closed notes below C3 to about 11 kHz and is dropped (est. inaudible).

**Low cutoffs are safe.** The lowest new filter-1 cutoff is 21. Filter 1 silences itself only within its last 4 steps (CONFIRMED, its "kill zones"), so no converted sound reaches that.

---

## 2. The TYPE list, from the three removed filters

| Removed filter | Becomes | Distinct from the others? |
|---|---|---|
| ANALOG VCF | **filter 1** (the same SVF; with the widened RES floor, exactly) | it *is* filter 1 |
| NOISE FILT | **filter 1** (MODE LP > BP and RES) | it *is* filter 1; its loudness rule stays with NOISE |
| TRIO filter | **filter 2's LP, BP, HP, NOTCH**: the gritty input, the squeezed resonance, the gentle top (Q 4, never sings), the slow drift | yes: unlike filter 1, it gets rough rather than whistling when pushed, and it moves a little by itself |

**So the final first list** (as you approved it; one meaning each):

| TYPE | Character |
|---|---|
| OFF | filter 2 not used |
| LP / BP / HP / NOTCH | TRIO's filter, 12 dB: warm, a little rough when driven, never whistles |
| LADDER | 24 dB Moog-style: fat, its bass thins at high RES, it can sing |
| COMB | tuned to the note; FEEDBACK + rings, - hollow (not on WAVE and PHYS, design 1 section 4.3) |

**Why no separate CHIP entries.** A second set of LP / BP / HP / NOTCH (one clean, one TRIO) would double the list. Filter 1 already is the clean one.

**Names.** The names stay LP, BP, HP and NOTCH. The help line and the editor say "TRIO-style" (question 1). The drift is part of the type's character, not a hidden control. It is the same few-hundredths-of-an-octave wander TRIO always had.

**Later, after the listening test:** FORMANT, MS-style, FILTER FM, and drive curves (CLIP).

---

## 3. Filter 2's controls, link and modulation (decided; the details designed here)

### 3.1 Stored values

| Value | Range, default | Note |
|---|---|---|
| TYPE | list above, OFF | |
| CUTOFF 2 | LINK on: OFFSET -63..+63 dial steps, 0. LINK off: CUTOFF 0..82 | one stored value, two labels. **Turning LINK keeps the sound**: the value is converted (offset = cutoff 2 - filter 1's cutoff) |
| RES 2 | 0..127 (TRIO's damping law for LP/BP/HP/NOTCH; FEEDBACK -64..+63 on COMB) | |
| DRIVE | 0..127, 0 = not run | design 1 section 2.5 |
| ROUTE | SERIES, PARALLEL | |
| BLEND | -64..+63, 0 | Virus-style, in both routes: -64 filter 1 alone, 0 both (SERIES: the chain; PARALLEL: equal mix), +63 filter 2 alone. Always shown. Cheap in series: filter 2's input slides from filter 1's output towards the unfiltered input, still one run |
| LINK | ON, OFF; **ON** | |
| KEY TRK 2 | -63..+63, 0 | |
| ENV 2 (to CUTOFF 2) | -64..+63, 0 | |
| LFO 1 (to CUTOFF 2) | -64..+63, 0 | |
| LFO 2 (to CUTOFF 2) | -64..+63, 0 | |

That is 11 values. Later: CLIP and FM, 13 in all.

**How the cutoff is worked out, per note and block** (the same form as filter 1's today, CONFIRMED `track_render`):

> filter 2's cutoff = (LINK on: filter 1's effective cutoff, including its knob, its four depths, KEY TRK, the matrix's CUT and the accent; LINK off: CUTOFF 2) + OFFSET + filter 2's own KEY TRK, ENV 2, LFO 1 and LFO 2 + the matrix's CUTOFF 2

The result is clamped to the dial.

So with LINK on and every amount at 0, one envelope and one knob move both filters: the Virus default. Filter 2's own amounts then add movement to filter 2 alone, which the Virus does not offer.

**On COMB:**
- CUTOFF becomes TUNE (semitones around the note; the comb follows the key by itself);
- LINK and KEY TRK 2 are hidden;
- ENV 2 and LFO 1 / 2 move TUNE (the classic comb sweep).

### 3.2 Ids and the extension block (CONFIRMED ids; the split is a proposal)

**On ids (5): TYPE, CUTOFF 2, RES 2, DRIVE, BLEND.**
- These go on ids 25 and 26 (once the global SCALE page has its own description), 42 (ALLOC, never read), 106 (free) and 113 (ANALOG's TYPE, freed).
- On ids they get MIDI LEARN, automation and the editor with no extra work, and the patch record does not grow: its worst case already counts every id.

**In the extension block (6): ROUTE, LINK, KEY TRK 2, ENV 2, LFO 1, LFO 2** (later CLIP and FM).
- These are switches and depths that are rarely played live.
- Sparse, about 2 B each.
- With 13 B of room in today's record, this needs the larger record of HEADLINE_FEATURES.md stage A (6 records a chunk), which you approved.

**Freed engine slots do not reduce this:** they are shared ids (1.1).

### 3.3 Matrix destinations (appended; 51 used of 128)

| Destination | Kind | Status |
|---|---|---|
| CUTOFF 2 (DEST card TWIN / CUTOFF) | per note | decided |
| TWIN / RES | per block | proposed |
| TWIN / DRIVE | per block, gliding | proposed (VEL here gives "harder notes distort more" back) |
| TWIN / BLEND | per block, gliding | proposed |

The existing CUT keeps meaning filter 1. With LINK on, it reaches filter 2 too.

### 3.4 Hide-inert rules (only a selection hides; a value never does)

- **TYPE OFF:** only the TYPE card shows; the TWIN MOD and TWIN ROUTE pages are skipped.
- **LINK:** changes the CUTOFF card's label (OFFSET / CUTOFF); hides nothing.
- **COMB:** hides LINK and KEY TRK 2; renames CUTOFF to TUNE and RES to FEEDBACK.
- **WAVE and PHYS:** COMB is not in the list.
- **BLEND:** always shown, since it works in both routes.

### 3.5 MORPH

- **Continuous:** CUTOFF 2 / OFFSET, RES 2, DRIVE, BLEND and the four amounts. They blend; cutoff blends in octaves.
- **The four amounts roll like filter 1's FLT depths** (an amount at 0 stays 0 while filter 2 is OFF).
- **Never rolled in SMART:** TYPE, ROUTE, LINK (like INSERT TYPE).
  - When the ends differ (WILD, or later editable B), they switch at the middle of the fader.
  - Both types run for 8 blocks (6 ms) while one fades out and the other starts from rest.
  - A LINK switch glides the cutoff over the same 8 blocks.
- **The safety rules of design 1 section 3.3 apply:**
  - no dead cutoff;
  - RES 2 above 100 never with more DRIVE;
  - DRIVE never far above the original.

### 3.6 Cost of filter 2's own modulation

- Filter 1 receives its routes once per note and block: about 5 multiply-adds, a clamp, and one coefficient (with one divide) for the block (CONFIRMED, `track_render` and `uf_coef`).
- Filter 2 adds the same again plus the link sum: about 40 to 70 instructions per note and block, about 1.5 to 2 instructions a sample. That is **about +0.05 points a note** (est.).
- ENV 2 and LFO 2 must also run when only filter 2 uses them. The conditions `e2on` / `l2on` in `track_render` gain filter 2's amounts. That costs nothing extra beyond what they cost when filter 1 uses them.

### 3.7 Pages (options for your approval; ask-before-UI rule)

**Option A (recommended): three pages after MULTI FILTER 2/2.**

| Page | K1 | K2 | K3 | K4 |
|---|---|---|---|---|
| TWIN FILTER | TYPE | CUTOFF / OFFSET (TUNE) | RES (FEEDBACK) | DRIVE |
| TWIN MOD | KEY TRK | ENV 2 | LFO 1 | LFO 2 |
| TWIN ROUTE | ROUTE | LINK | (later: FM) | BLEND |

- TWIN MOD is in the same order as MULTI FILTER 2/2 (K1 KEY TRK, K2 ENV 2, K3 LFO 1, K4 LFO 2, CONFIRMED `params.c`). One rule for both filters.
- The cutoff and drive gesture is on one page.
- BLEND is a mix, so it is last.
- CLIP (later) has no slot yet. It is decided after the listening test.

**Option B:** TWIN FILTER = TYPE, CUTOFF, RES, LINK (LINK beside the card whose label it changes); TWIN ROUTE = ROUTE, DRIVE, FM later, BLEND; TWIN MOD as in A. The cost: cutoff and drive are on different pages.

**The three engines' own pages shrink** (the VCF, TRIO FILTER and NOISE FILT pages disappear). Using the existing card-order list (`engine_t.uiord`), no id moves:
- ANALOG: ENGINE (WAVE, DTN, MIX), EDIT 1 (NOISE, DRIVE).
- TRIO: ENGINE (WAVE, INT2, INT3), EDIT 1 (DTN, PW).
- NOISE: ENGINE (MODE, COLR, DENS), EDIT 1 (DRFT, CRSH).
- Their HOME knobs that were CUT and RES become the MULTI FILTER's, as on the other seven.

**Editor:** one TWIN FILTER card (type, cutoff, res, drive, route, link, blend, the four amounts) beside the MULTI FILTER card, with the flow picture.

---

## 4. Revised cost, RAM and code

### 4.1 Per engine, TWIN off (est.)

**Calibration:**
- The MULTI FILTER is measured at +46.6 host instructions a sample (device est. +1.8 points) at 12 dB, and +58.6 with a MODE blend.
- The removed engine filters are estimated at 0.4 to 0.9 points: they are short loops written inside the engines, not measured on their own.

**Why it is not neutral.** The general filter stage carries a scratch block, ramped mode weights and a soft knee on top of the same filter math.

| Engine | Removed | Added | Net per note | Notes from 8 |
|---|---|---|---|---|
| ANALOG | own VCF, 0.4 to 0.6 | filter 1 now on every note, 1.8; the amp after the chain, 0.2 | **+1.0 to +1.6** | about 7 |
| TRIO | own filter, 0.6 to 0.8 | filter 2 (TRIO-style), 1.0 to 1.2; the TWIN path with filter 1 open, 0.4 to 0.5; amp 0.2 | **+0.9 to +1.2** | about 7 |
| NOISE | own filter, 0.7 to 0.9 | filter 1 with the LP > BP blend, 2.0 to 2.2; amp 0.2 | **+1.2 to +1.6** | about 7 |
| the other seven | - | the amp after filter 1, only if the chain moves before the amp everywhere (question 3) | +0.2 where filter 1 runs | 8 |

**TWIN on, on any engine:** add design 1's figures (+1.1 to +2.6) and +0.05 for filter 2's own modulation.

**To try:** a lean filter-1 path for a steady 12 dB low-pass (no weight ramps) might win back 0.3 to 0.5 a note (HYPOTHESIS). The host cost rows decide.

### 4.2 RAM

- **Per note:** filter 2's state about 28 B, plus TRIO-style drift and its random state 8 B, so about 36 B. Over the 32 note slots of the 4 tracks that is about 1.15 KB of variable RAM, against 2.1 KB spare (headroom 6,220 B, rule 4,096 B, CONFIRMED). It fits, but it tightens; the state could live only with the 2 parts that play (about 0.6 KB).
- **Engine state:** ANALOG, TRIO and NOISE no longer use their filter words. The 8-word engine state per note is fixed, so this saves no RAM.
- **COMB lines:** in the engine memory, as in design 1.

### 4.3 Code

- **The engines shrink by about 1.0 to 1.4 KB (est.):**
  - ANALOG loses its filter taps and the whole BP / HP copy of its loop (`analog_render_f`) and the filter part of its morph loop;
  - TRIO loses its filter loop;
  - NOISE loses its filter, keeping its loudness rule.
- **The engines are core 1's code in range X**, which has 1,264 B free (CONFIRMED, MERGE_JOIN1). With the freed 1.0 to 1.4 KB, range X could hold TWIN's 1.5 to 1.8 KB, which now includes LINK, the four amounts and the TRIO-style types. Range Z and its pool tail would then stay untouched.
  - HYPOTHESIS: that the hot audio code may call into range X needs the link map.
- **Hot block:** the ufilt 2 branch in `track_render` goes, which gives back a few bytes towards the TWIN branch (est. 20 to 40 B). HYPOTHESIS until built.

---

## 5. Staging (revised)

| Step | What | Sound |
|---|---|---|
| 0. Proof | Offline renders, as design 1 section 5, **plus the converter run on all 43 sounds and the 13 multi parts: old / new pairs** for you to compare. New gate **G5**: you accept the 43 pairs, or name the ones you will fix yourself | nothing changes |
| A. Format room | the extension block and the larger record (6 a chunk), multi and project too | **bit for bit** |
| 1. TWIN core | filter 2 (LP / BP / HP / NOTCH TRIO-style, LADDER, COMB), DRIVE, ROUTE + BLEND, LINK + the four amounts, the four matrix destinations, morph rules, NOTES rows, pages and editor card after your layout approval. The chain moves before the amp only while TWIN is on | **bit for bit** while TYPE is OFF |
| 2. Engine filters out + conversion | the three engines become like the other seven; filter 1's RES floor widened; NOISE's loudness and drift hand-over; the accent rule; the shrunk engine pages; the converter applied to the factory bank; the factory multis rebuilt and your multis converted; End B seeds re-picked; patch levels re-measured; test fingerprints re-made | **changes**: the 43 sounds and 10 multis by the table; the other 85 sounds only through the RES re-mapping (same sound) and, if question 3 says so, the move before the amp (resonant peaks on loud notes, envelope edges) |
| 3. Additions | FILTER FM, CLIP, FORMANT, MS-style, after the listening test | new types only |

**Order:**
- Step 2 needs step 1 (TRIO's sounds need filter 2).
- Step 2 must come before the alpha, or the firmware must carry the converter for old saves (question 4).

**Effort (est.):**
- step 0: 2 to 3 rounds + your sessions;
- step A: 2 to 3 rounds;
- step 1: 4 to 6 rounds;
- step 2: 3 to 4 rounds (the converter and its table test are simulation work; the pages and the unit checks are local) + one listening session of yours.

**Risks:**
- not CPU-neutral (4.1);
- your multis change (1.4);
- timing against the alpha;
- NOISE loudness;
- the RAM placement (4.3).

**Plan B if step 2 slips past the alpha:**
- keep the three engine filters for the alpha;
- ship TWIN on top (design 1);
- remove them in a later release together with a load-time converter.

---

## 6. Questions for you (design only, each with my recommendation)

1. **Filter 2's LP, BP, HP and NOTCH are TRIO's filter** (rough when pushed, never whistles, a slow drift), while filter 1 stays the clean one that ANALOG's and NOISE's filters become. The names stay LP / BP / HP / NOTCH; the help says "TRIO-style".
   **Recommended: yes.**
2. **Conversion rules, so the 43 sounds stay close:**
   - (a) filter 1's RES gets ANALOG's softer floor at the bottom (today's filter-1 values convert, RES 0 -> 40, same sound);
   - (b) ANALOG's DRV stays in the engine, as the oscillator drive;
   - (c) NOISE keeps its loudness rule and its drift, now following filter 1;
   - (d) your own multis M03, M04, M06, M07, M08, M09, M12 and M14 are converted by the same table, and you listen before.

   **Recommended: yes to all four.**
3. **The chain before the amp on every engine**, as one rule. All test fingerprints are re-made once; the seven other engines change only at loud resonant peaks and envelope edges. The alternative: before the amp only where TWIN is on or the engine lost its filter.
   **Recommended: every engine.**
4. **When the engine filters go:** before the alpha (no converter needed in the firmware), or after it, with a converter in the firmware for old saves.
   **Recommended: before the alpha, together with TWIN step 1.** If that is too much for the alpha, plan B.
5. **Pages: Option A**: TWIN FILTER (TYPE, CUTOFF, RES, DRIVE), TWIN MOD (KEY TRK, ENV 2, LFO 1, LFO 2), TWIN ROUTE (ROUTE, LINK, -, BLEND), and the shrunk pages of ANALOG, TRIO and NOISE (3.7). Option B: LINK beside CUTOFF.
   **Recommended: A.**

---

## Evidence

| Claim | Source | Mark |
|---|---|---|
| E1..E8 labels and defaults of ANALOG, TRIO, NOISE; ANALOG TYPE on id 113 | `eng_analog.c`, `eng_trio.c`, `eng_noise.c` `.edit`; `core.h` P_FTYPE | CONFIRMED |
| Routes drive the own filter on ufilt 2 engines; the MULTI FILTER takes CUT and KEY TRK only; filter 1's route sum per note and block | `voice.c` `track_render` | CONFIRMED |
| ANALOG drive before the filter, knee 16,000, KTR 42.5 % at 127 around note 60 | `eng_analog.c` `analog_osc`; `tools/duell_sounds.py` header | CONFIRMED (code and the tool's notes) |
| TRIO drift from the shared noise generator; damping 2.0 to 0.25; squeeze at +-49,152; input knee | `eng_trio.c` `trio_render` | CONFIRMED |
| NOISE LP > BP morph, make-up gain, BP scale, TRK 127 = 100 %, CRSH only in ANLG / DUST | `eng_noise.c` `noise_render` | CONFIRMED |
| Dial step = 1.547 table steps; KEY TRK 63 = 100 %; kill zones in the last 4 steps | `core.h` UF_* comments; SPEC 2.5 | CONFIRMED |
| The 43 sounds and their numbers; their filter values and routes | `tools/gen_duell_sounds.py --check`, `tools/duell_sounds.py` (read with a script) | CONFIRMED |
| The 10 multis and which are yours | `tools/factory_multis.py`, `tools/owner_multis.py` | CONFIRMED |
| Free ids 25, 26, 42, 106 | `core.h`, `params.c` | CONFIRMED |
| MULTI FILTER 2/2 card order | `params.c` PAGES | CONFIRMED |
| Engine card order without moving ids | `core.h` engine_t.uiord | CONFIRMED |
| Hot RAM, range X / Z, RAM headroom | release_drafts/MERGE_JOIN1.md | CONFIRMED (build numbers) |
| MULTI FILTER costs | SPEC 2.5, docs/CPU_PLAN.md | measured / device est. |
| Cost of the removed engine filters; every net figure; code sizes | my operation counts | est. |
| Virus: Cutoff Link on by default, one Env Amount for cutoff 1 and 2, Filter Balance semantics | [Virus TI quickstart, p. 28](https://manualzilla.com/doc/5804536/virus-ti-series?page=28), [p. 160](https://manualzilla.com/doc/5804536/virus-ti-series?page=160) (search excerpts) | HYPOTHESIS |

**Not verified:**
- every cost figure (nothing exists to measure);
- that the hot code may call range X;
- how close filter 1's knee lands to ANALOG's;
- NOISE's new loudness on each sound;
- whether the Virus has a separate envelope amount for filter 2 (the excerpt names one Env Amount);
- the multis' stored values (read from the list that builds them, not from the binary records; your own multis may hold edits the converter will meet).
