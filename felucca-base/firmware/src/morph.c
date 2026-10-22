/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL MORPH (SPEC 3): one fader, two random ends, one percentage. Build step 4a: the engine, no page yet.
 *
 * Per part (A and B, NSHOWN): the Original (the part's t->p as stored, saved, edited), End A (fader 0) and End B (fader 127),
 * both random variations of the Original rolled with ONE percentage AMOUNT; the fader blends A .. B.
 *
 * Tags (MT[] below, the engines' MET[]; SPEC appendix B with the review list as settled, REVIEW_LIST.md):
 *   MK_CONT  continuous: an end = the original + an offset uniform over 0 .. AMOUNT % of the range (truncated, so never
 *            more; at least one step), random sign (owner, 2026-10-11: every parameter moves, "a lot of little changes",
 *            audible at AMOUNT 10-20 %; replaces the bell, whose moves were mostly tiny; morph_move; MF_CAP: at most the cap).
 *            A move that would leave the allowed interval (the range and
 *            the limits below) goes the other way; only when neither way fits is it clipped (toward the side with more room).
 *            The stored value is the perceptual domain already:
 *            times, cutoffs and LFO rates are indices into exponential tables (MF_LOG marks them), so a linear offset and a
 *            linear blend of the index are log-domain ones. Flags: MF_ZERO marks a pitch item (rule 3a; PITCH lifts it, so a zero is rolled like any value),
 *            MF_STEREO only in STEREO (rule 3b), MF_CAP at most lim units from the original (fine detunes: 10 cents),
 *            MF_FLOOR / MF_CEIL an end never below min(original, lim) / above max(original, lim), MF_RISE never more than
 *            lim above the original (loudness), MF_WONLY rolled only
 *            in WILD (the EQ: a mix tool; its limits stay in WILD); MF_DUCK: the DUCK values roll in SMART only while the ducker is on (DEPTH > 0), in WILD always. Every limit keeps |offset| <= AMOUNT %
 *   MK_FADE  a choice that the audio path cross-fades (mxf, core.h): AMOUNT % chance that an end differs; then any other
 *            allowed value (MF_NFADE: only 0..lim-1, an original outside that is never rolled)
 *   MK_SLOT  a matrix slot (SRC DST AMT as one route). SMART (owner, 2026-10-12): an assigned route stays as it is, SRC and
 *            DST never change, no route is added or removed; only AMT rolls (continuous, by the one percentage), also of a route whose destination the roll does not move. A slot that is
 *            not active (SRC, DST or AMT 0) is left. WILD: SRC and DST each change with AMOUNT % chance (never to OFF, never a
 *            new route to PITCH or VIB, PAN only in STEREO, never onto an engine parameter that is not continuous, no LFO
 *            self-loop, never to or from MODW / AT / EXPR) and a roll may ADD routes (morph_add_routes: at most 2 new ones, at
 *            most 4 active routes in all, moderate amounts). A route to PITCH / VIB keeps its route
 *   MK_HARD  a choice that cannot be faded: never rolled (rule 2)
 *   MK_EX    never rolled at all (rule 3): engine, level, tuning, voice mode, arp, LFO polarity, insert type, ...
 * Mode-dependent tags (B.4) are read with the original's mode (morph_tag); a roll never changes a mode.
 * Smart rules (rule 4, ideas of the Teensy randomizer, simply; morph_rules, morph_open): no dead filter path (ANALOG,
 * TRIO: low-pass never below ~130 Hz; a band-pass / high-pass end keeps its band at ~200 Hz..3 kHz / below ~1.3 kHz over the
 * ENV -> FLT sweep and half the LFO -> FLT one, lower for ANALOG SIN / TRI; when only a move beyond AMOUNT would do that,
 * the end keeps the original's TYPE; NOISE / LOFI tone floors), resonance against drive (ANALOG: a
 * roll that lifts RES above 100 does not lift DRV), ATK never above max(original, ~1 s) (never silent), source levels and
 * depths that only rise a little (MF_RISE: noise, sub, DIST send, LFO -> AMP), SUS floor, ceilings for PHYS DAMP and BOW and
 * VOICE BRTH (no endless notes, no lost voicing). New matrix routes only by morph_add_routes, no self-loops (see MK_SLOT).
 * FM6's sound is its patch (fm6_patch), not t->p: morph_fm_* moves 65 of its bytes too (operator OL and EG rates / levels, pitch EG,
 * LFO speed / delay / depths), blended per block into fm6_eff (eng_fm6.c fm6_morph hook), never into the stored patch. Not done: the spec's
 * render of a test note at roll time (an engine's state is the part's own: a test render would disturb the live voices);
 * tests/morph_test.c renders 500 rolls x 6 engines on the host instead.
 *
 * The fader (morph_apply): continuous values blend linearly (Q8, rounded: fader 0 is End A and 127 End B exactly); a choice
 * that differs fades through mxf (the other end's value and its weight: LFO WAVE 1/2 cross-fade both shapes, ANALOG WAVE 0..4
 * and filter TYPE, PHASE WAVE / WAVE2, TRIO MODE, WHEEL REG, LOFI WAVE PLS / TRI / SAW); a matrix slot whose route is the same
 * at both ends only moves its amount, a slot whose route differs fades the A route out over the first half of the fader and
 * the B route in over the second (the switch happens at amount 0: no jump; nothing needs a ninth slot).
 *
 * The hook (where the blend reaches the sound): morph_begin / morph_end, around each part's block (fx.c mix_block) and around
 * each note-on (voice.c trk_note_on: what an engine or the voice reads at note-on: DTUNE, LFO PHASE, GLIDE, ..). As the matrix
 * does with its per-block destinations (mod.c mod_begin), morph_begin writes the blended values into t->p for that window
 * only and morph_end puts the stored ones back; the matrix then modulates on top of the morph. Both run in the audio ISR, so
 * the UI, the editor, presets and projects never see a morphed value: t->p stays the Original. A per-part cache keeps the
 * blend while nothing changes (fader, ends, live values): such a block only writes the values that differ.
 * Manual edits keep working: the morph is an offset on top of the Original. base[] is the Original the ends were rolled
 * from; a continuous value sounds as live + (blend - base), so turning a knob moves the morphed sound by what it moved; a
 * choice or a route the user changed (live != base) is the user's and is not faded. A sound load (ui.c load_end, the sound
 * undo, project_load) re-bases the part's morph (ends = the new Original, history cleared); until then, a part whose
 * engine, preset or user slot differs from the morph's plays unmorphed (and the next action re-bases it).
 *
 * Nothing rolled (both ends equal the Original), AMOUNT 0, or the ends equal to it: the part's list is empty and
 * morph_begin returns at once: the sound is bit-identical (golden renders).
 *
 * Controls (SPEC 3.4, 3.5): ROLL A / ROLL B roll that end from the Original; history of MORPH_HIST - 1 = 2 earlier rolls per
 * end (9 deep up to memory trim 1); UNDO goes back one roll of the end it is given (knob: left A, right B). There is no KEEP (v2). Replacing an end that is heard (ROLL,
 * UNDO) ramps over ~50 ms: continuous values glide, a changed choice / route dips its end's weight to 0 and back
 * (the other end's value in the middle: two values at a time, continuous). One ramp per part: an action while one runs
 * completes it at once (a roll of the other end). TARGET SELECTED (the part ALGORITHM picked) or
 * BOTH (one fader, each part its own independent ends, the same AMOUNT). 0.2.9: TARGET is ALGORITHM's position on the MORPH page
 * and in PERFORM (PART 1 / BOTH / PART 2, ui_input.c algo_step), RAM only: the stored TARGET bytes (settings, multi, this block) are written as
 * before and never read. Seeds: 1..65535, shown; morph_roll_seed with the
 * same Original, AMOUNT and seed gives the same end. The per-part seed is the shown one hashed with the part.
 *
 * v2 (SPEC 3.5): a sound load (morph_rebase_part) puts the fader at A and rolls End B (fresh, at the AMOUNT, or the B saved with
 * the patch: morph_ld, MB_N bytes = seed, AMOUNT, MC_* bits, which morph_roll_cfg regenerates exactly). Settings (morph.cfg,
 * ui_input.c morph_settings): SMART off = WILD (MC_WILD: the tags lose every exclusion, cap and fade, MK_STEP flips at the
 * middle of the fader, morph_rules is skipped; MF_NEVER items, the engine and the globals stay out), ARP (MF_ARP items: the
 * arp's mode / rate / octaves / gate, never its on / off), PITCH (MF_PIT and MF_ZERO items: out unless PITCH is on). The
 * roll reads them through mrc (morph_cfg_tag, fm_tag); every end remembers the bits and AMOUNT it was rolled with.
 *
 * Persistence (decided for this step, documented in the report): the morph is RAM state of the parts (mph[]), not in
 * t->p (no P_* id is used: the 6 free ids stay free). morph_pack / morph_unpack give the block a project would store
 * (MORPH_PACK_SIZE bytes: target, per part fader, AMOUNT, the two seeds and the two ends as absolute values; not the
 * history): the storage step adds it to the project file (a new format after FUN11). Until then a power cycle or a project
 * load starts each part with the fader at A and a fresh B (v2). A user preset carries End B as seed + AMOUNT + bits (SAVE held: upreset.c patch_save_do). */

#define MORPH_HIST 3u                /* the current end + 2 earlier rolls, per end (memory trim 1: was 9, +8: 6,224 B of ordinary RAM; the owner: one UNDO is enough, 3 deep at most) */
#ifndef XH
#define XH 3u                        /* the roll history of the extension slots (ext.h XV), per end: the current end + XH - 1 earlier rolls. 3 = the ids' depth (memory trim 1: MORPH_HIST 3). XH 1 is the lean setting:
                                      * an UNDO past XH rolls brings the ids' older end back with the slots at the Original (morph_undo_part). Must divide MORPH_HIST (the ring index of a slot is the ids' index mod XH) */
#endif
_Static_assert(XH >= 1u && XH <= MORPH_HIST && MORPH_HIST % XH == 0u, "XH: 1 or 3");
#define MORPH_AMOUNT_DEF 15u          /* the player's AMOUNT at first use (owner, 2026-10-11: 25 before) */
#define MORPH_RAMP_N (FS / 20u)      /* 50 ms */
#define MORPH_RAMP_STEP ((32768u * CTL + MORPH_RAMP_N / 2u) / MORPH_RAMP_N)   /* Q15 a block */
enum { MT_SELECTED, MT_BOTH };
/* cloud job G, EDITABLE B (docs/design/EDITABLE_B_DESIGN.md): at the fader's B end (127) a knob on a value the morph can move sets End B's value for good ("set by hand", pinned): the value is kept as
 * an absolute number in End B's current history entry and marked in pin[] (a bit per wide id). A pinned value ignores the Original's later edits (the other B values follow them as before); a roll drops
 * the marks (its new entry has none), UNDO brings them back with the entry. EDITB_PINH: how many of End B's history entries keep their marks (MORPH_HIST = all; 1 = only the current one: 36 bytes
 * a part less, but an UNDO then leaves the earlier entries' hand-set values without their marks, they follow the Original like rolled ones and a SAVE after such an UNDO does not keep them) */
#ifndef EDITB_PINH
#define EDITB_PINH MORPH_HIST
#endif
_Static_assert(EDITB_PINH >= 1u && EDITB_PINH <= MORPH_HIST && MORPH_HIST % EDITB_PINH == 0u, "EDITB_PINH: 1 or 3");
#define PIN_BYTES (EXT_P_LEN / 8)                        /* 18: a bit for each wide id */
#define MK_PIN 0x40u                                     /* ment_t.kind: the value is End B's by hand (MK_STEREO 0x80 is the other flag) */

enum { MK_EX, MK_HARD, MK_CONT, MK_FADE, MK_SLOT, MK_STEP };   /* MK_STEP (v2): a choice that flips at the middle of the fader */
#define MF_LOG 1u
#define MF_ZERO 2u
#define MF_STEREO 4u
#define MF_CAP 8u
#define MF_FLOOR 16u
#define MF_CEIL 32u
#define MF_RISE 64u
#define MF_NFADE 128u
#define MF_PIT 256u                                      /* v2: a pitch item (octave, interval, transpose, ..; MF_ZERO ones are too) */
#define MF_NEVER 512u                                    /* v2: never, not even in WILD (mix level, mute, the sequencer pattern, ..) */
#define MF_DRAW 2048u                                    /* v2: an excluded item that used to roll: still draws, so the streams of old seeds stay */
#define MF_ARP 1024u                                     /* v2: an arp setting: only with ARP ON (any mode), never the arp's on / off */
#define MF_WONLY 8192u                                   /* rolled only in WILD (SMART: never); its limits kept there */
#define MF_DUCK 32768u                                   /* a DUCK value: SMART rolls it only while the ducker is on (DEPTH > 0); WILD always */
#define MF_GONE 16384u                                   /* a retired id (the SLICER's, core.h P_FREEA..D): never moves, in no mode;
                                                          * draws what its old tag drew (lim 1: a continuous one, else a switch),
                                                          * so the streams of old seeds (a patch's saved End B) stay */
typedef struct { uint8_t kind; uint16_t flags; int8_t lim; } mtag_t;
#define T_EX {MK_EX, 0, 0}
#define T_HARD {MK_HARD, 0, 0}
#define T_C {MK_CONT, 0, 0}
#define T_CL {MK_CONT, MF_LOG, 0}
#define T_CF(f, l) {MK_CONT, (f), (l)}
#define T_F {MK_FADE, 0, 0}
#define T_FN(n) {MK_FADE, MF_NFADE, (n)}
#define T_SLOT {MK_SLOT, 0, 0}
#define T_PIT {MK_EX, MF_PIT, 0}
#define T_NEV {MK_EX, MF_NEVER, 0}
#define T_GONE(c) {MK_EX, MF_GONE, (c)}
#define T_ARPS {MK_HARD, MF_ARP, 0}
#define T_W(f, l) {MK_CONT, MF_WONLY | (f), (l)}                /* WILD only, continuous */
#define MORPH_SEND_CEIL 114                              /* sends: a roll never above 90 % (a send cannot drown the sound) */

/* v2 roll configuration (SPEC 3.5): the bits a roll is made with, kept with the end (saved with a patch). mrc is the one the
 * tag functions read: morph_roll_cfg sets it around a roll, morph_build uses the permissive one (every end, whatever it was
 * rolled with, may be listed). Not set (tests): SMART, no ARP, no PITCH, SPLIT */
#define MC_WILD 1u
#define MC_ARP 2u
#define MC_PIT 4u
#define MC_STEREO 8u
#define MC_PART 16u                                      /* the part the end's stream belongs to (0 / 1) */
#define MC_SET 7u                                        /* the three settings */
#define MC_LIST 32u                                      /* (mrc only, morph_build: a WILD-only item is listed as continuous) */
static uint8_t mrc;

/* the common parameters (B.1); not listed = MK_EX */
static const mtag_t MT[P_E0] = {
    [P_LEVEL] = T_NEV,
    [P_ATK] = T_CF(MF_LOG | MF_CEIL, 96), [P_DEC] = T_CL,                                   /* ATK: never silent (~1 s) */ [P_SUS] = T_CF(MF_FLOOR, 24), [P_REL] = T_CL,
    [P_ED_FLT] = T_C, [P_ED_PIT] = T_CF(MF_ZERO, 0), [P_ED_SHP] = T_C, [P_EQLG] = T_W(MF_CAP, 6),
    [P_LRATE] = T_CL, [P_LWAVE] = T_F, [P_LPHASE] = T_C, [P_LFADE] = T_CL,
    [P_LD_PIT] = T_CF(MF_ZERO, 0), [P_LD_FLT] = T_CF(MF_CAP, 32), [P_LD_SHP] = T_C, [P_LD_AMP] = T_CF(MF_CAP, 12),
    [P_AMODE] = T_ARPS, [P_ARATE] = T_ARPS, [P_AOCT] = T_ARPS, [P_AGATE] = T_ARPS,           /* v2: only with ARP ON (review 1: out) */
    [P_ASWING] = T_NEV, [P_APROB] = T_NEV, [P_AHOLD] = T_NEV, [P_AORDER] = T_NEV,
    [P_RES2] = T_CF(MF_CEIL, 108), [P_DRV2] = T_CF(MF_RISE, 32), [P_KTRK] = T_NEV, [P_TRANS] = T_PIT,   /* TWIN FILTER: RES 2 (no new self-oscillation: at most max(original, 108)) and DRIVE (never far above the original: loudness) on the ids of ROOT and SCALE; their old draws: morph_old_draw */
    [P_SLEN] = T_NEV, [P_SDIV] = T_NEV, [P_SSWING] = T_NEV, [P_SGATE] = T_NEV,
    [P_DIST] = T_CF(MF_RISE, 40), [P_CHOR] = T_CF(MF_CEIL, MORPH_SEND_CEIL), [P_DLY] = T_CF(MF_CEIL, MORPH_SEND_CEIL),
    [P_REV] = T_CF(MF_CEIL, MORPH_SEND_CEIL),
    [P_VOICE] = T_EX, [P_GLIDE] = T_CF(MF_LOG | MF_ZERO, 0), [P_PAN] = T_CF(MF_STEREO, 0), [P_MUTE] = T_NEV,
    [P_GLMODE] = T_HARD, [P_PRIO] = T_HARD, [P_TWTYPE] = T_HARD, [P_DETUNE] = T_CF(MF_CAP, 32),   /* 127 ~ 40 ct: 10 ct */
    [P_FREEA] = T_GONE(0), [P_FREEB] = T_GONE(0), [P_FREEC] = T_GONE(0), [P_FREED] = T_GONE(1),   /* free (the SLICER's
                                       * SLCR PAT RATE DEPTH, removed): never, as T_NEV, with the draws they had (MF_GONE) */
    [P_M1SRC] = T_SLOT, [P_M1DST] = T_SLOT, [P_M1AMT] = T_SLOT, [P_M2SRC] = T_SLOT, [P_M2DST] = T_SLOT, [P_M2AMT] = T_SLOT,
    [P_M3SRC] = T_SLOT, [P_M3DST] = T_SLOT, [P_M3AMT] = T_SLOT, [P_M4SRC] = T_SLOT, [P_M4DST] = T_SLOT, [P_M4AMT] = T_SLOT,
    [P_M5SRC] = T_SLOT, [P_M5DST] = T_SLOT, [P_M5AMT] = T_SLOT, [P_M6SRC] = T_SLOT, [P_M6DST] = T_SLOT, [P_M6AMT] = T_SLOT,
    [P_M7SRC] = T_SLOT, [P_M7DST] = T_SLOT, [P_M7AMT] = T_SLOT, [P_M8SRC] = T_SLOT, [P_M8DST] = T_SLOT, [P_M8AMT] = T_SLOT,
    [P_E2ATK] = T_CL, [P_E2DEC] = T_CL, [P_E2SUS] = T_C, [P_E2REL] = T_CL,
    [P_L2RATE] = T_CL, [P_L2WAVE] = T_F, [P_L2PHASE] = T_C, [P_L2FADE] = T_CL,
    [P_CHRD] = T_NEV, [P_VOIC] = T_NEV, [P_L2SYNC] = T_HARD, [P_L2TRIG] = T_HARD, [P_L2POL] = T_EX,
    [P_ATGT] = T_NEV, [P_EQLF] = T_W(0, 0), [P_EQMG] = T_W(MF_CAP, 6), [P_EQMF] = T_W(0, 0), [P_EQMQ] = T_W(0, 0),   /* the EQ (SPEC
                                       * 5c.3): a mix tool, so SMART leaves it; WILD moves it (gains at most 6 dB from the original) */
    [P_LSYNC] = T_HARD, [P_LTRIG] = T_HARD, [P_LPOL] = T_EX, [P_SQNT] = T_NEV, [P_SPRD] = T_CF(MF_STEREO, 0),
    [P_ITYPE] = T_EX, [P_IA] = T_C, [P_IB] = T_C, [P_IC] = T_C, [P_IMIX] = T_C,               /* 3c: the type stays */
    [P_BLEND] = T_C,                                                                         /* the TWIN FILTER's BLEND (was ANALOG's filter TYPE, a fade) */
    [P_ESYNC] = T_HARD,
    [P_EQHG] = T_W(MF_CAP, 6), [P_EQHF] = T_W(0, 0),
    [P_DKCRV] = T_NEV, [P_CUT2] = T_C, [P_DKSRC] = T_NEV, [P_DKRATE] = T_NEV,   /* DUCK (SPEC 5c.4): a mix tool; the sidechain INPUT never rolls (not in WILD either); */
    [P_DKDEP] = T_CF(MF_DUCK | MF_CEIL, 50), [P_DKATK] = T_CF(MF_DUCK, 0), [P_TRIM] = T_CF(MF_DUCK, 0), [P_DKRLS] = T_CF(MF_DUCK, 0),   /* ducker on: SMART rolls them; off: only WILD */
    [P_DKTHR] = T_CF(MF_DUCK, 0),                              /* its times, THR and DEPTH (never above 50 %): SMART only with the ducker on, WILD also from off; SRC, NOTE, RATE, CURVE never (T_NEV, also in WILD) */
    [P_E2FLT] = T_C, [P_E2SHP] = T_C,                   /* ENV 2 DEST, LFO 2 DEST (SPEC 2.4): as ENV 1's and LFO 1's (the pitch one */
    [P_L2PIT] = T_CF(MF_ZERO, 0), [P_L2FLT] = T_CF(MF_CAP, 32), [P_L2SHP] = T_C,   /* a pitch item, zero stays zero); drawn from a stream of their own */
};

/* The TWIN FILTER, SUB and CYCLE values with no id (ext.h slots X0..X14; EXTENSION_BLOCK_PLAN 2B, TWIN_FILTER_DESIGN 3.3 and _2 3.5, SUB_AND_CYCLE_DESIGN A4, B2). Together with the five TWIN ids
 * (above: CUTOFF 2 in dial steps = octaves, RES 2, DRIVE, BLEND continuous; TYPE a switch):
 *   continuous (they blend, a roll moves them by the one AMOUNT): the four amounts (KEY TRK 2, ENV 2, LFO 1, LFO 2: like filter 1's FLT depths, a LFO's at most 32 steps; morph_twin_gate: they, like
 *     all TWIN values, stay put while TYPE is OFF in SMART), SUB LEVEL and HARMONICS (never more than 48 above the original: loudness), CYCLE RISE and FALL (the ATK / DEC time tables: log), CURVE
 *   switches, never rolled in SMART, a flip in WILD, and at the middle of the fader when the ends differ (MK_HARD -> MK_STEP in morph_build): TYPE, ROUTE, LINK, SUB OCTAVE, SUB WAVE, CYCLE MODE, CYCLE SYNC
 *   safety rules (SMART, morph_twin_rules): no dead cutoff 2, RES 2 above 100 never with more DRIVE; a roll never switches TWIN on (TYPE OFF stays OFF, in WILD too) nor a sub (LEVEL 0 stays 0, the
 *   other sub values with it); the amounts and the TWIN values stay put while TYPE is OFF */
static const mtag_t MTX[NX] = {
    [X_ROUTE] = T_HARD, [X_LINK] = T_HARD,
    [X_KTRK2] = T_CF(MF_CAP, 32), [X_ENV2] = T_C, [X_LFO1] = T_CF(MF_CAP, 32), [X_LFO2] = T_CF(MF_CAP, 32),
    [X_SUBLVL] = T_CF(MF_RISE, 48), [X_SUBOCT] = T_HARD, [X_SUBWAVE] = T_HARD, [X_SUBHARM] = T_CF(MF_RISE, 48),
    [X_CYMODE] = T_HARD, [X_CYRISE] = T_CL, [X_CYFALL] = T_CL, [X_CYCURVE] = T_C, [X_CYSYNC] = T_HARD,
};                                                      /* (X15, the spare: T_EX) */
#define MORPH_OLD(id) ((id) == P_RES2 || (id) == P_DRV2 || (id) == P_TWTYPE || (id) == P_BLEND || (id) == P_CUT2)   /* the five ids the TWIN FILTER took over */

/* P_E0..P_E7 per engine (B.2); an engine not listed (DIGITAL, DRUM: not in DUELL; SAMPLE, GRAIN, SLICE: retired in 0.2.9) has every
 * E excluded */
typedef struct { const engine_t *e; mtag_t t[8]; } metag_t;
static const metag_t MET[] = {
    {&ENG_ANALOG, {T_FN(5), T_CF(MF_CAP, 10), T_C, T_CF(MF_RISE, 48), T_EX, T_EX, T_C, T_EX}},   /* review 2; CUT RES KTR (E5 E6 E8) are unused slots since cloud job J: filter 1 rolls as on every engine */
    {&ENG_PHASE, {T_F, T_F, T_C, T_C, T_CF(MF_ZERO | MF_CAP, 10), T_EX, T_EX, T_EX}},   /* review 5, 6; E7 (its own SUB) is an unused slot since cloud job J */
    {&ENG_LOFI, {T_HARD, T_FN(3), T_HARD, T_C, T_CF(MF_ZERO, 0), T_CF(MF_ZERO, 0), T_HARD, T_C}},   /* review 7, 8, 9 */
    {&ENG_FORMANT, {T_C, T_C, T_CL, T_CF(MF_CAP, 3), T_C, T_CF(MF_CEIL, 100), T_C, T_C}},      /* review 10 */
    {&ENG_TRIO, {T_EX, T_PIT, T_PIT, T_CF(MF_CAP, 10), T_EX, T_EX, T_EX, T_C}},   /* MODE CUT RES (E5 E6 E7) are unused slots since cloud job J: filter 2's TYPE CUTOFF 2 RES 2 roll on the TWIN stream */
    {&ENG_WHEEL, {T_F, T_EX, T_C, T_C, T_HARD, T_C, T_C, T_HARD}},                               /* review 11; E2 (its own SUB) is an unused slot since cloud job J */
    {&ENG_PHYS, {T_HARD, T_C, T_C, T_CF(MF_CEIL, 110), T_C, T_C, T_CF(MF_CEIL, 100), T_C}},   /* review 12 */
    {&ENG_NOISE, {T_HARD, T_C, T_EX, T_EX, T_EX, T_C, T_C, T_C}},   /* FREQ RES TRK (E3 E4 E5) are unused slots since cloud job J: filter 1 */
    {&ENG_FM6, {T_HARD, T_CF(MF_CEIL, 4), T_CF(MF_RISE | MF_CAP, 12), T_PIT, T_C, T_C, T_CF(MF_CAP, 35), T_EX}},                    /* FB, MLVL, MEG, VMOD move; ALG, MRAT, SLOT not */
    /* WAVE (DUELL): TABLE never (not in WILD either: a new table is built, the part silent meanwhile: no fade can hide it); POS
     * continuous; BEND and SYNC continuous, a move at most 32 in SMART (both speed the read up, and a sweep of both across the
     * band-limited levels took a sparse table's top partials away within a few % of the fader: a cliff, rule 3-bis), and SMART
     * keeps a SYNC of 0 at 0 (morph_tag: sync is a distinctive sound, rule 3g); (SUB: an unused slot since cloud job J); DTN
     * as PHASE's (zero stays zero, 10 ct); STEPS (E7: OFF, 2..19 places) is never rolled in SMART and flips in WILD (as before it was
     * a slot of no use: the draws of every End B are kept); what a roll or an edit made of it blends in the morph_build: between two counts continuously (rounded), OFF against a count it flips at the middle */
    {&ENG_WAVE, {T_NEV, T_C, T_CF(MF_CAP, 32), T_CF(MF_CAP, 32), T_EX, T_CF(MF_ZERO | MF_CAP, 10), T_EX, T_EX}},
};

/* parameter id's tag for engine e with the original's values o (the mode-dependent ones, B.4); stereo: STEREO mode.
 * v2 (SPEC 3.5): the table's tag, then the roll configuration mrc (morph_cfg_tag): ARP, PITCH, WILD */
static mtag_t morph_cfg_tag(mtag_t g, const engine_t *e, uint32_t id, uint32_t stereo)
{
    uint32_t pit = g.flags & (MF_ZERO | MF_PIT);
    if (g.flags & MF_GONE)                              /* a retired id: excluded everywhere, its draws kept (morph_roll_pure) */
        return g;
    if (g.flags & MF_NEVER)
        return (mtag_t)T_EX;
    if (g.flags & MF_WONLY) {                           /* the EQ, the DUCK: WILD only, its limits kept */
        if (!(mrc & (MC_WILD | MC_LIST)))
            return (mtag_t)T_EX;
        g.flags &= (uint16_t)~MF_WONLY;
        return g;
    }
    if (g.flags & MF_DUCK) {                            /* the DUCK values: WILD as tagged (limits kept; the off-lock is in morph_tag). SMART (owner,
                                                         * 2026-10-13): with the ducker on only DEPTH rolls; ATK, RLS, THR (and the legacy HOLD) keep the
                                                         * pump as tuned, but draw as they did (MF_DRAW: one draw each, so the streams of old seeds stay) */
        if (id != P_DKDEP && !(mrc & (MC_WILD | MC_LIST))) {
            mtag_t x = T_EX;
            x.flags = MF_DRAW;
            return x;
        }
        return g;
    }
    if (g.flags & MF_ARP) {                             /* the arp's mode / rate / octaves / gate: ARP ON, smart or wild */
        g.kind = !(mrc & MC_ARP) ? MK_EX : id == P_AGATE ? MK_CONT : MK_STEP;
        g.flags &= (uint16_t)~MF_ARP;
        return g;
    }
    if (pit && !(mrc & MC_PIT)) {                       /* PITCH OFF: nothing that moves pitch, also in WILD */
        mtag_t x = T_EX;
        if (g.kind == MK_CONT)
            x.flags = MF_DRAW;
        return x;
    }
    if (mrc & MC_WILD) {                                /* WILD: no exclusion, cap, floor, fade or rule; switches flip */
        if (g.kind == MK_EX || g.kind == MK_HARD || g.kind == MK_FADE)
            g.kind = MK_STEP;
        g.flags &= MF_LOG;
        g.lim = 0;
        return g;
    }
    if ((g.flags & MF_STEREO) && !stereo)
        g.kind = MK_EX;
    if (pit) {                                          /* PITCH ON, smart: the whole-number ones step, zero stays zero is lifted */
        if (g.kind == MK_EX)
            g.kind = MK_STEP;
        g.flags &= (uint16_t)~MF_ZERO;
    }
    return g;
}
/* the TWIN FILTER / SUB / CYCLE tags g of id (a TWIN id or an extension slot) with what the Original o says about whether they may move: a roll never switches TWIN on (TYPE OFF is never rolled, WILD too:
 * TWIN_FILTER_DESIGN 3.3), never a sub (SUB LEVEL 0 keeps the sub's other values too), and in SMART leaves the TWIN values alone while TYPE is OFF (nothing of them is heard; an amount at 0
 * stays 0 as the FLT depths do while the filter is bypassed) and KEY TRK 2's 0. A value left alone still draws (MF_DRAW), so the stream does not depend on it. morph_build (MC_LIST) lists
 * whatever differs */
static mtag_t morph_twin_gate(mtag_t g, const int8_t *o, uint32_t id)
{
    mtag_t x = T_EX;
    x.flags = MF_DRAW;
    if (id == P_TWTYPE) {
        mtag_t n = T_EX;
        return o[P_TWTYPE] ? g : n;
    }
    if (id >= XW_BASE + X_SUBLVL && id <= XW_BASE + X_SUBHARM)
        return !o[XW_BASE + X_SUBLVL] && !(mrc & MC_LIST) ? x : g;
    if (id >= XW_BASE + X_CYMODE)
        return g;
    if (!(mrc & (MC_WILD | MC_LIST)) && (!o[P_TWTYPE] || (id == XW_BASE + X_KTRK2 && !o[id])))
        return x;
    return g;
}
static mtag_t morph_tag(const engine_t *e, const int8_t *o, uint32_t id, uint32_t stereo)
{
    mtag_t g = T_EX;
    uint32_t i, k;
    if (id >= P_COUNT) {                                 /* an extension slot (ext.h): MTX; 127 and the spare X15: never */
        if (id < XW_BASE || id > XW_BASE + X_CYSYNC)
            return g;
        return morph_twin_gate(morph_cfg_tag(MTX[id - XW_BASE], e, id, stereo), o, id);
    }
    if (id >= P_FREEA && id <= P_FREED && e->ufilt) {    /* the multimode filter (DUELL 0.2.9): CUT, RES, MODE continuous like any other, SLOPE a choice that flips,
                                                         * in SMART and WILD, on every engine, bypassed (MODE LP, CUT open) or not: SMART may switch it on (owner
                                                         * override of rule 3g, 0.2.9; 0.2.11: with half the chance, morph_wake_half). Drawn from a stream of their
                                                         * own (morph_roll_pure); morph_uf_safe keeps every result audible */
        g.kind = id == P_FREED ? MK_FADE : MK_CONT;   /* (SLOPE: a choice cross-faded, mxf MX_SLOPE: voice.c uf_run blends the two slopes, no cliff) */
        return g;
    }
    if (id == P_KTRK && e->ufilt) {                     /* KEY TRK (0.2.21), the multimode filter's: continuous like CUT / RES, at most 32 steps (half the range) from the original's; SMART leaves a 0
                                                         * (off) alone, WILD rolls it too. Its draws are its own stream (morph_roll_pure rs5), so no seed of before moves; it never switches the filter on (there) */
        if (!o[id] && !(mrc & (MC_WILD | MC_LIST)))
            return (mtag_t)T_EX;
        g.kind = MK_CONT;
        g.flags = MF_CAP;
        g.lim = 32;
        return g;
    }
    if (id < P_E0) {
        g = morph_cfg_tag(MT[id], e, id, stereo);
        if ((MT[id].flags & MF_DUCK) && !o[P_DKDEP] && !(mrc & (MC_WILD | MC_LIST)))
            return (mtag_t)T_EX;                        /* SMART: a ducker that is off stays off (SRC, NOTE, RATE, CURVE never roll: T_NEV) */
        if (id >= P_IA && id <= P_IMIX && !o[P_ITYPE] && !(mrc & MC_WILD))
            return (mtag_t)T_EX;                        /* SMART: an INSERT that is off stays off (its values and MIX are not rolled) */
        if (MORPH_OLD(id))
            return morph_twin_gate(g, o, id);           /* the TWIN FILTER's five ids */
        return g;
    }
    k = id - P_E0;
    for (i = 0; i < NELEM(MET); i++)
        if (MET[i].e == e) {
            g = MET[i].t[k];
            break;
        }
    if (e == &ENG_ANALOG && k == 1u && o[P_E0] == AW_SYNC)
        g.flags &= (uint16_t)~MF_CAP;                   /* SYNC: the sync interval, not a detune */
    else if (e == &ENG_LOFI && k == 3u && o[P_E0] == BITS_STEP)
        g.kind = MK_EX;                                 /* CRSH is the stepped envelope there (review 9) */
    else if (e == &ENG_PHYS && k == 1u && o[P_E0] == PM_SYMP)
        g.kind = MK_HARD;                               /* CHRD tunes the sympathetic strings */
    else if (e == &ENG_PHYS && k == 6u && o[P_E0] == PM_MEMB)
        g.flags |= MF_ZERO;                             /* BEND: a pitch bend of the strike (rule 3a) */
    else if (e == &ENG_NOISE && k == 5u && o[P_E0] >= NZ_LFSR)
        g.kind = MK_HARD;                               /* LEN: the register */
    else if (e == &ENG_NOISE && k == 7u && o[P_E0] >= NZ_LFSR) {
        g.kind = MK_EX;                                 /* CLK: a pitch offset */
        g.flags |= MF_PIT;
    }
    if (g.kind == MK_FADE && (g.flags & MF_NFADE) && (o[id] < 0 || o[id] >= g.lim))
        g.kind = MK_HARD;                               /* outside the faded values (ANALOG SYNC / SUB, LOFI NOIS / WRAM) */
    g = morph_cfg_tag(g, e, id, stereo);
    if (e == &ENG_PHYS && k == 6u && !o[id] && !(mrc & MC_WILD))
        return (mtag_t)T_EX;                            /* SMART: BOW 0 (struck) stays struck: any BOW > 0 replaces the strike by a bowed excitation */
    if (e == &ENG_WAVE && k == 3u && !o[id] && !(mrc & MC_WILD))
        return (mtag_t)T_EX;                            /* SMART: WAVE's SYNC 0 (no sync) stays 0 (rule 3g); a SYNC above 0 rolls */
    return g;
}

/* the range of id for engine e (the edit table, as the matrix: mod.c) */
static const param_desc_t *morph_desc(const engine_t *e, uint32_t id) { return id >= XW_BASE ? xdesc(0, id - XW_BASE) : id >= P_COUNT ? &XFREE_D : id >= P_E0 ? &e->edit[id - P_E0] : &TP[id]; }
static const param_desc_t MORPH_E7_OLD = {"-", F_INT, 0, 0, 0, 0, 0};   /* WAVE's E7 as it was before STEPS (an unused slot, range 0..0): a WILD roll draws it as it did, so the End B of every stored seed is what it was, and never turns STEPS on */

/* ----------------------------------------------------------------- the roll (pure) --- */
static uint32_t mr_next(uint32_t *s)                    /* xorshift32 */
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}
static uint32_t mr_below(uint32_t *s, uint32_t n) { return (uint32_t)(((uint64_t)mr_next(s) * n) >> 32); }
/* a continuous move from v (one draw; owner, 2026-10-11): |offset| uniform over 1 .. maxoff (= AMOUNT % of the range: a
 * uniform 0..AMOUNT rounded up to a step, so a value that is rolled moves), random sign; the end stays in [alo, ahi]
 * (which holds v): a move that leaves it goes the other way by as much, and only when neither way fits does it stop at
 * the bound with more room. |end - v| <= maxoff always */
static int32_t morph_move(uint32_t *s, int32_t v, int32_t maxoff, int32_t alo, int32_t ahi)
{
    uint32_t r = mr_next(s);
    int32_t m, x;
    if (maxoff <= 0)
        return v;
    m = 1 + (int32_t)(((uint64_t)(r & 0x7FFFFFFFu) * (uint32_t)maxoff) >> 31);
    x = (r >> 31) ? v - m : v + m;
    if (x < alo || x > ahi) {
        x = 2 * v - x;                                  /* the other way */
        if (x < alo || x > ahi)
            x = ahi - v >= v - alo ? ahi : alo;
    }
    return x;
}
static uint32_t morph_state(uint32_t seed, uint32_t part)   /* the generator of seed on part */
{
    uint32_t x = (seed & 0xFFFFu) * 0x9E3779B1u ^ (part + 1u) * 0x85EBCA77u;
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    x *= 0x297A2D39u;
    x ^= x >> 15;
    return x ? x : 0x6D2B79F5u;
}

#define MS_PERF(s) ((s) == MS_MODW || (s) == MS_AT || (s) == MS_EXPR)   /* performance controllers: never rolled */
static int morph_loop(uint32_t s, uint32_t d)          /* an LFO that would modulate itself */
{
    return ((s == MS_LFO || s == MS_SH || s == MS_SLEW) && (d == MD_RATE || d == MD_DEPTH)) ||
           (s == MS_LFO2 && (d == MD_RATE2 || d == MD_DEPTH2));
}
#define MS_N_OLD (MS_DUCK + 1)                           /* the sources cloud/base3 had: a WILD roll picks a route's new source among these only, so a stored seed (a patch's saved End B) rolls the End B it did;
                                                          * the CYCLE source (cloud job B) is not drawn for (a roll's draws would move), a stored route from it is left whole like MORPH's */
static int morph_src_ok(uint32_t s) { return s > MS_OFF && s < MS_N_OLD && !MS_PERF(s) && s != MS_MORPH; }   /* (4b: never MORPH) */
static int morph_dst_ok(const engine_t *e, const int8_t *o, uint32_t s, uint32_t d, uint32_t stereo)
{
    if (d == MD_OFF || d >= MD_N_026 || d == MD_PITCH || d == MD_VIB || morph_loop(s, d))
        return 0;                                       /* (0.2.7: a WILD roll moves a route only among 0.2.6's destinations, so a
                                                         * saved seed rolls the same End B; a route INTO a new one keeps its rules) */
    if (d == MD_PAN)
        return (int)stereo;
    if (MD_ENGINE(d))
        return morph_tag(e, o, P_E0 + d - MD_E1, stereo).kind == MK_CONT;
    return 1;
}

/* a route the morph leaves whole (only its amount moves, or nothing): to PITCH / VIB (3a), from a performance controller,
 * onto an engine parameter that is not continuous (FM6 MRAT, a SET, ..) */
static int morph_route_fixed(const engine_t *e, const int8_t *o, uint32_t s, uint32_t d)
{
    return d == MD_PITCH || d == MD_VIB || MS_PERF(s) ||
           (MD_ENGINE(d) && morph_tag(e, o, P_E0 + d - MD_E1, 1).kind != MK_CONT);
}

/* a continuous value of id (tag g, range pd) from the original value v */
static int32_t morph_cont(uint32_t *rs, mtag_t g, const param_desc_t *pd, int32_t v, uint32_t amount)
{
    int32_t lo = pd->min, hi = pd->max, maxoff = (int32_t)amount * (hi - lo) / 100;   /* (truncated: |offset| <= maxoff) */
    v = clamp(v, lo, hi);
    if ((g.flags & MF_ZERO) && !v) {
        (void)mr_next(rs);                              /* (a pitch item reaching here is never zero-locked: PITCH lifts MF_ZERO; kept as a guard) */
        return v;
    }
    if ((g.flags & MF_CAP) && maxoff > g.lim)
        maxoff = g.lim;
    if ((g.flags & MF_FLOOR) && lo < (v < g.lim ? v : g.lim))
        lo = v < g.lim ? v : g.lim;
    if ((g.flags & MF_CEIL) && hi > (v > g.lim ? v : g.lim))
        hi = v > g.lim ? v : g.lim;
    if ((g.flags & MF_RISE) && hi > v + g.lim)
        hi = v + g.lim;
    return morph_move(rs, v, maxoff, lo, hi);
}

/* a different value in [lo, n) than v, uniformly (v itself when there is none) */
static int32_t morph_other(uint32_t *rs, int32_t lo, int32_t n, int32_t v)
{
    int32_t k;
    if (n - lo < 2)
        return v;
    k = lo + (int32_t)mr_below(rs, (uint32_t)(n - lo - 1));
    return k >= v ? k + 1 : k;
}

/* matrix slot k: the route as one */
static void morph_slot(uint32_t *rs, const engine_t *e, const int8_t *o, int8_t *end, uint32_t k, uint32_t amount,
                       uint32_t stereo)
{
    uint32_t id = MSLOT_ID(k), s = (uint32_t)o[id], d = (uint32_t)o[id + 1u], i, n;
    int32_t a = o[id + 2u];
    uint8_t cand[MD_N];
    if (!s || !d || !a || s >= MS_N || d >= MD_N)
        return;                                         /* not active: never rolled (the morph adds no routes) */
    if (!(mrc & MC_WILD)) {                             /* SMART (owner, 2026-10-12): every active route's AMT rolls by the one %, also one whose */
        end[id] = (int8_t)s;                            /* destination the roll does not move (BOW 0, a pitch item, PAN, ..); SRC, DST stay */
        end[id + 1u] = (int8_t)d;
        if ((d == MD_PITCH || d == MD_VIB) && !(mrc & MC_PIT)) {   /* owner: pitch stays while the PITCH roll is OFF: the amount too */
            (void)mr_next(rs);
            end[id + 2u] = (int8_t)a;
            return;
        }
        end[id + 2u] = (int8_t)morph_cont(rs, (mtag_t)T_C, &TP[id + 2u], a, amount);
        return;
    }
    if (s == MS_MORPH)
        return;                                         /* (4b) a route from the fader itself: left whole (no feedback) */
    if (d == MD_PAN && !stereo)
        return;                                         /* rule 3b, the route too */
    if (MD_ENGINE(d) && morph_tag(e, o, P_E0 + d - MD_E1, 1).kind != MK_CONT)
        return;                                         /* into a parameter the morph does not move: left alone */
    if ((d == MD_PITCH || d == MD_VIB) && !(mrc & MC_PIT)) {
        (void)mr_next(rs);                              /* v2: PITCH OFF: pitch modulation is left as it is (the draw it had) */
        return;
    }
    if (!morph_route_fixed(e, o, s, d)) {
        if (mr_below(rs, 100u) < amount) {
            for (i = 1, n = 0; i < MS_N; i++)
                if (i != s && morph_src_ok(i) && !morph_loop(i, d))
                    cand[n++] = (uint8_t)i;
            if (n)
                s = cand[mr_below(rs, n)];
        }
        if (mr_below(rs, 100u) < amount) {
            for (i = 1, n = 0; i < MD_N; i++)
                if (i != d && morph_dst_ok(e, o, s, i, stereo))
                    cand[n++] = (uint8_t)i;
            if (n)
                d = cand[mr_below(rs, n)];
        }
    }
    if (!(mrc & MC_WILD)) {                             /* SMART (owner, 2026-10-12): the route stays as it is, only its amount rolls */
        s = (uint32_t)o[id];
        d = (uint32_t)o[id + 1u];
    }
    end[id] = (int8_t)s;
    end[id + 1u] = (int8_t)d;
    end[id + 2u] = (int8_t)morph_cont(rs, (mtag_t)T_C, &TP[id + 2u], a, amount);
}

/* ----------------------------------------------------------- new matrix routes (B) --- */
/* A roll may add routes into inactive slots, so a default patch (empty matrix) can move. Per end: at most 2 new routes,
 * each with 2 x AMOUNT % chance, at most 4 active routes in all; sources LFO, LFO 2, ENV (not where the engine's own envelopes
 * rule: FM6), ENV 2, VEL, KEY, RAND; destinations only what the engine answers to: CUT, SHP, AMP, PAN (STEREO), the
 * continuous E1..E8 (no pitch-like, capped or loudness-limited one, no stepped one), RATE / DEPTH of an LFO that is used.
 * Moderate amounts (6 .. 24 of 64 with AMOUNT; a cutoff up to 16, unipolar sources only opening it; SHP up to 20). The route fades in as any route absent at one end (morph_apply) */
static int morph_resp(const engine_t *e, const int8_t *end, uint32_t d)
{
    if (d == MD_CUT)                                    /* DUELL 0.2.5: a multimode-filter engine answers to CUT even bypassed (WHEEL too), but a roll never
                                                         * switches that filter on by a new route: only when it is not bypassed (0.2.9) */
        return 1;                                           /* (DUELL 0.2.9 owner override: the multimode filter is on every engine, bypassed or not: a route to CUT may be added) */
    if (d == MD_SHP)
        return e == &ENG_ANALOG || e == &ENG_FM6 || e == &ENG_FORMANT || e == &ENG_NOISE ||
               e == &ENG_PHASE || e == &ENG_TRIO || e == &ENG_WAVE;   /* (WAVE, DUELL: SHP is its POS; a new engine, no saved seed rolls differently) */
    return 1;
}
static int morph_cutlike(const engine_t *e, uint32_t k)   /* engine parameter k is a filter cutoff */
{
    return e == &ENG_LOFI && k == 7u;
}
static int morph_route_e_ok(const engine_t *e, const int8_t *o, uint32_t k, uint32_t stereo)
{
    mtag_t g = morph_tag(e, o, P_E0 + k, stereo);
    if (g.kind != MK_CONT || (g.flags & (MF_ZERO | MF_CAP | MF_RISE | MF_CEIL | MF_FLOOR)))
        return 0;
    if (e == &ENG_LOFI && k == 3u)
        return 0;                                       /* CRSH: the hold / bit steps */
    if (e == &ENG_NOISE && k == 7u)
        return 0;                                       /* CRSH */
    if (e == &ENG_FM6 && k != 1u && k != 2u)
        return 0;                                       /* only FB and MLVL answer smoothly */
    return 1;
}
static void morph_add_routes(uint32_t *rs, const engine_t *e, const int8_t *o, int8_t *end, uint32_t amount, uint32_t stereo)
{
    uint32_t tries, k, i, n, active = 0;
    for (k = 0; k < NMSLOT; k++)
        active += end[MSLOT_ID(k)] && end[MSLOT_ID(k) + 1u] && end[MSLOT_ID(k) + 2u];
    for (tries = 0; tries < 2u && active < 4u; tries++) {
        uint8_t ss[8], dd[24];
        uint32_t ns = 0, nd = 0, s, d, q, lfo1 = end[P_LD_FLT] || end[P_LD_SHP] || end[P_LD_AMP], lfo2 = end[P_L2FLT] || end[P_L2SHP], m;
        int32_t a;
        if (mr_below(rs, 100u) >= 2u * amount)          /* (2 x AMOUNT % a try: at 20 %, ~0.8 new routes an end) */
            continue;
        for (k = 0; k < NMSLOT; k++) {
            uint32_t es = (uint32_t)end[MSLOT_ID(k)];
            if (end[MSLOT_ID(k) + 1u] && end[MSLOT_ID(k) + 2u]) {
                lfo1 |= es == MS_LFO;
                lfo2 |= es == MS_LFO2;
            }
        }
        for (s = MS_LFO; s < MS_N; s++)
            if (s == MS_LFO || s == MS_LFO2 || (s == MS_ENV && !e->ownenv) || s == MS_ENV2 || s == MS_VEL || s == MS_KEY ||
                s == MS_RAND)
                ss[ns++] = (uint8_t)s;
        s = ss[mr_below(rs, ns)];
        for (d = MD_CUT; d < MD_N_026; d++) {           /* (0.2.7's destinations: never a new route, as the sends) */
            if (d == MD_CUT || d == MD_SHP || d == MD_AMP) {
                if (!morph_resp(e, end, d))
                    continue;
            } else if (d == MD_PAN) {
                if (!stereo)
                    continue;
            } else if (MD_ENGINE(d)) {
                if (!morph_route_e_ok(e, o, d - MD_E1, stereo))
                    continue;
            } else if (d == MD_RATE || d == MD_DEPTH) {
                if (!lfo1 || morph_loop(s, d))
                    continue;
            } else if (d == MD_RATE2 || d == MD_DEPTH2) {
                if (!lfo2 || morph_loop(s, d))
                    continue;
            } else {
                continue;                               /* sends: not here */
            }
            for (q = 0; q < NMSLOT; q++)                /* (the same route twice: no) */
                if ((uint32_t)end[MSLOT_ID(q)] == s && (uint32_t)end[MSLOT_ID(q) + 1u] == d &&
                    end[MSLOT_ID(q) + 2u])
                    break;
            if (q == NMSLOT)
                dd[nd++] = (uint8_t)d;
        }
        if (!nd)
            continue;
        d = dd[mr_below(rs, nd)];
        m = 6u + mr_below(rs, 1u + amount * 18u / 100u);   /* 6 .. 24 (heard: CUT 6 is +-0.4 octave) */
        if (d == MD_CUT || (MD_ENGINE(d) && morph_cutlike(e, d - MD_E1)))
            m = m > 16u ? 16u : m;                      /* a cutoff: ~1 octave at most (a dead filter is no sound) .. */
        else if (d == MD_SHP)
            m = m > 20u ? 20u : m;
        a = (mr_below(rs, 2u) ? -(int32_t)m : (int32_t)m);
        if (a < 0 && !MS_BIPOLAR(s) && (d == MD_CUT || (MD_ENGINE(d) && morph_cutlike(e, d - MD_E1))))
            a = -a;                                     /* .. and an envelope / VEL never closes it below the floor */
        for (i = 0, n = 0; i < NMSLOT; i++)
            if (!(end[MSLOT_ID(i)] && end[MSLOT_ID(i) + 1u] && end[MSLOT_ID(i) + 2u])) {
                n = i + 1u;
                break;
            }
        if (!n)
            break;
        k = n - 1u;
        end[MSLOT_ID(k)] = (int8_t)s;
        end[MSLOT_ID(k) + 1u] = (int8_t)d;
        end[MSLOT_ID(k) + 2u] = (int8_t)clamp(a, -64, 63);
        active++;
    }
}

/* ------------------------------------------------- FM6: the patch bytes (A) --- */
/* An FM6 sound is its patch (fm6_patch, 155 bytes), not t->p. The morph moves 65 of its bytes: per operator the four EG
 * rates and four EG levels and the output level OL (54), the pitch EG's rates and levels 1..3 (7), the LFO's speed, delay,
 * pitch depth and amp depth (4). The ends hold them (absolute, as the t->p ones), the blend goes into fm6_eff's copy of the
 * patch (eng_fm6.c fm6_sync, through fm6_morph), as live + (blend - base): the stored patch is never touched. Left out
 * (pitch-like or not fadable): algorithm, operator ratio / fine / detune / fixed-frequency mode, keyboard level and rate
 * scaling, sensitivities, LFO wave / sync, transpose. FB, MEG, MLVL, VMOD are the macros (t->p, tagged above) */
#define NFM 65u
static uint32_t fm_byte(uint32_t i)
{
    if (i < 54u)
        return (i / 9u) * FP_OP + ((i % 9u) < 4u ? FP_R1 + i % 9u : (i % 9u) < 8u ? FP_L1 + i % 9u - 4u : FP_OL);
    return i < 58u ? FP_PR1 + i - 54u : i < 61u ? FP_PL1 + i - 58u : FP_LFS + i - 61u;
}
typedef struct { uint8_t ex, zero, lo, rise, cap, pg; } fmtag_t;   /* lo: not below min(orig, lo); rise: at most rise above; cap: |offset|; pg: left by PITCH OFF (still draws) */
static fmtag_t fm_tag(uint32_t i, const uint8_t *bp, uint32_t car)
{
    fmtag_t g = {0, 0, 0, 99, 99, 0};
    uint32_t pitem = (i >= 54u && i < 61u) || i == 63u;   /* the pitch EG and the LFO's pitch depth (v2: pitch items) */
    if (mrc & MC_WILD) {
        /* no rule */
    } else if (i < 54u) {
        uint32_t j = i % 9u;
        if ((car >> (i / 9u)) & 1u) {                   /* a carrier: it must stay heard, and not much louder */
            if (j == 7u)
                g.ex = 1;                               /* L4: the rest level */
            else if (j == 0u || j == 3u)
                g.lo = 40;                              /* R1, R4: no endless attack or release (owner 2026-10-11: faster may be) */
            else if (j < 3u)
                g.cap = bp[(i / 9u) * 9u + 6u] < 80u ? 3 : 8;   /* R2, R3: how fast the body decays = its loudness where it
                                                         * decays (L3 under 80: a pluck's R2 +5 was -3 dB, +19 -9 dB) */
            else if (j < 7u) {
                g.lo = 60;                              /* L1 .. L3: the loudness of the note's body: +-1 (~0.75 dB; at 99,
                                                         * the usual, every move is down) */
                g.rise = g.cap = 1;
            } else {
                g.lo = 75;                              /* OL of a carrier: the loudness: +-2 steps (~1.5 dB) at most, either */
                g.rise = 2;                             /* way (a fixed AMOUNT move with "never louder" was always ~-14 dB;
                                                         * +-6 still left -5 dB on three carriers at 99). The timbre is the
                                                         * modulators' (+-12) */
                g.cap = 2;
            }
        } else if (j > 3u) {
            g.rise = g.cap = 12;                        /* a modulator's levels: brighter not without bounds; +-12 either way */
        }
    } else if (i < 61u) {
        /* (PITCH OFF leaves the pitch EG below: pitem. A test here read bp[FP_PL1..] = patch offsets 130..133 of this
         * 65-byte fm_byte-order copy, i.e. the next end's history: whether these items drew depended on it, so a seed
         * did not always give the same end. Gone: PITCH OFF they always draw (as nearly every time before)) */
        if (i >= 58u)
            g.cap = 8;                                  /* its levels: a few semitones at most */
    } else if (i == 63u) {
        g.zero = !(mrc & MC_PIT);                       /* the LFO's pitch depth (3a; PITCH ON lifts it) */
    } else if (i == 64u) {
        g.rise = 40;
    }
    if (pitem && !(mrc & MC_PIT)) {                     /* PITCH OFF: never touched, also in WILD */
        g.pg = !g.ex;
        g.ex = 1;
    }
    return g;
}
static void morph_fm_roll(const uint8_t *bp, uint32_t car, uint8_t *end, uint32_t amount, uint32_t seed, uint32_t part)
{
    uint32_t rs = morph_state(seed, part + 4u), i;
    memcpy(end, bp, NFM);
    if (!amount)
        return;
    amount = amount > 100u ? 100u : amount;
    for (i = 0; i < NFM; i++) {
        fmtag_t g = fm_tag(i, bp, car);
        int32_t v = bp[i] > 99 ? 99 : bp[i], maxoff = (int32_t)amount * 99 / 100, lo, hi;
        if (g.ex && !g.pg)
            continue;
        if (g.ex || (g.zero && !v)) {
            (void)mr_next(&rs);                         /* (the draw it had) */
            continue;
        }
        if (maxoff > g.cap)
            maxoff = g.cap;
        lo = v < g.lo ? v : g.lo;
        hi = v + g.rise < 99 ? v + g.rise : 99;
        end[i] = (uint8_t)morph_move(&rs, v, maxoff, lo, hi);   /* (the other way when blocked) */
    }
}

/* rule 4, simply: values moved back toward the original only (the AMOUNT bound holds) */
/* a cutoff floor (0..127): a move below min(o, fl) goes the other way by as much (the move stays whole), else the floor */
static int32_t reflect_floor(int32_t x, int32_t o, int32_t fl)
{
    int32_t m = o < fl ? o : fl, y = 2 * o - x;
    return x >= m ? x : y <= 127 ? y : m;
}
#define MORPH_CUT_FLOOR 30                               /* cutoffs: 30 Hz .. 16 kHz over 0..127, ~14 a octave: */
#define MORPH_HP_CEIL 70                                 /* LP never below ~130 Hz, HP never above ~1.3 kHz, BP within */
#define MORPH_BP_LO 40                                   /* ~200 Hz .. ~3 kHz (see morph_open) */
#define MORPH_BP_HI 90
#define MORPH_CUT_DOWN 32                                /* a roll's net cutoff closing at most (morph_cut_steer) */
#define MORPH_UF_LO 45                                   /* the multimode filter: a low-pass / band-pass share never below this dial step (~1.3 kHz) ... */
#define MORPH_UF_HI 25                                   /* ... a high-pass / band-pass one never above this (~210 Hz); a pure band-pass up to MORPH_UF_BP */
#define MORPH_UF_RESBP 40                                /* the band-pass side: RES at most this (or the original's) */
#define MORPH_UF_BP0 36                                  /* the band-pass side's CUT window */
#define MORPH_UF_BP1 50
#define MORPH_UF_RES 60                                  /* the multimode filter's RES: a roll never above max(original, this) (SMART)  */
#define MORPH_RES_CEIL 108                               /* ANALOG RES: a roll never above max(original, this) (SMART) */
/* "no dead path": a band-pass / high-pass end keeps its band where the notes are over the sweeps of ENV -> FLT and LFO -> FLT
 * (each moves the cutoff up to +-63; the LFO's counted half: it passes, a wah is no dead path): CUT + those inside the window, else ENV depth, LFO depth, then CUT go back toward the
 * original's; still outside (only reachable beyond AMOUNT): the TYPE stays the original's. sine: a source without
 * harmonics (ANALOG SIN / TRI), the window lower. Low-pass: the floor only */
static int morph_band_ok(const int8_t *end, uint32_t cut, int32_t wlo, int32_t whi)
{
    int32_t ed = end[P_ED_FLT], e2 = end[P_E2FLT];      /* (ENV 2 DEST's FLT sweeps as ENV 1's; LFO 2's as LFO 1's) */
    int32_t lf = ((end[P_LD_FLT] < 0 ? -end[P_LD_FLT] : end[P_LD_FLT]) + (end[P_L2FLT] < 0 ? -end[P_L2FLT] : end[P_L2FLT])) >> 1;   /* (an LFO: half) */
    return end[cut] + (ed < 0 ? ed : 0) + (e2 < 0 ? e2 : 0) - lf >= wlo && end[cut] + (ed > 0 ? ed : 0) + (e2 > 0 ? e2 : 0) + lf <= whi;
}
static void morph_open(int8_t *end, const int8_t *o, uint32_t cut, uint32_t ftid, int sine)
{
    int32_t ft = end[ftid], wlo = ft == 1 ? MORPH_BP_LO : -255, whi = ft == 1 ? (sine ? 60 : MORPH_BP_HI) : sine ? 50 : MORPH_HP_CEIL;
    if (ft == 1 || ft == 2) {
        while (!morph_band_ok(end, cut, wlo, whi) && end[P_ED_FLT] != o[P_ED_FLT])
            end[P_ED_FLT] = (int8_t)(end[P_ED_FLT] + (o[P_ED_FLT] > end[P_ED_FLT] ? 1 : -1));
        while (!morph_band_ok(end, cut, wlo, whi) && end[P_E2FLT] != o[P_E2FLT])
            end[P_E2FLT] = (int8_t)(end[P_E2FLT] + (o[P_E2FLT] > end[P_E2FLT] ? 1 : -1));
        while (!morph_band_ok(end, cut, wlo, whi) && end[P_LD_FLT] != o[P_LD_FLT])
            end[P_LD_FLT] = (int8_t)(end[P_LD_FLT] + (o[P_LD_FLT] > end[P_LD_FLT] ? 1 : -1));
        while (!morph_band_ok(end, cut, wlo, whi) && end[P_L2FLT] != o[P_L2FLT])
            end[P_L2FLT] = (int8_t)(end[P_L2FLT] + (o[P_L2FLT] > end[P_L2FLT] ? 1 : -1));
        while (!morph_band_ok(end, cut, wlo, whi) && end[cut] != o[cut])
            end[cut] = (int8_t)(end[cut] + (o[cut] > end[cut] ? 1 : -1));
        if (morph_band_ok(end, cut, wlo, whi) || ft == o[ftid])
            return;
        end[ftid] = o[ftid];                            /* (the TYPE back: then as the original's) */
        if (o[ftid] == 1 || o[ftid] == 2)
            return;
    }
    end[cut] = (int8_t)reflect_floor(end[cut], o[cut], MORPH_CUT_FLOOR);
}
/* ------------------------------------------------ loudness rules (owner, 2026-10-11) --- */
/* A SMART roll keeps the loudness near the Original's (tests/roll_loudness.c: at AMOUNT 20, 90 % of the rolls within +-4.5 dB,
 * 99 % within +-8 dB). Measured there, the big swings came from: the DIST insert switched on from 0 (its clipper and make-up,
 * ~-3 dB at any amount; owner 2026-10-12: a DIST from 0 rises only by the AMOUNT, up to its +40 limit), FM6 carriers' levels and decay rates (fm_tag), PHASE's resonant waves (~9 dB under
 * the others), the filter's net cutoff closing (CUT plus the envelopes' FLT depths), the sustain level and, on a sound that
 * decays, its decay time. The rules steer before they cap: a move that would push the loudness too far goes the other way
 * by as much (the move stays whole); only when that does not fit is it shortened. Tone parameters keep the full window.
 * The LFOs' -> FLT depths (LFO 1, LFO 2) move at most 32 steps (MT: at AMOUNT 100 a deep LFO trough, an ENV 2 sweep and a matrix route
 * together closed a pad to -26 dB for the first 0.6 s: tests/morph_test.c never silent) */
/* the net shift of the cutoff as the note's body hears it: CUT, and the ENV / ENV 2 -> FLT depths weighted toward their
 * peak (3/4 of the way from the sustain level: an onset gone dark is heard). Q7 */
static int morph_cut_route(const int8_t *o, const int8_t *x, uint32_t k)
{
    uint32_t id = MSLOT_ID(k);
    return o[id] && o[id] == x[id] && o[id + 1u] == MD_CUT && x[id + 1u] == MD_CUT && !MS_BIPOLAR((uint32_t)o[id]) &&
           !MS_PERF((uint32_t)o[id]);
}
static int32_t morph_cut_net(const int8_t *o, const int8_t *x, uint32_t cut)
{
    int32_t s1 = 96 + (x[P_SUS] >> 2), s2 = 96 + (x[P_E2SUS] >> 2), n, k;
    n = ((x[cut] - o[cut]) << 7) + (x[P_ED_FLT] - o[P_ED_FLT]) * s1 + (x[P_E2FLT] - o[P_E2FLT]) * s2;
    for (k = 0; k < (int32_t)NMSLOT; k++)                /* a route to CUT from a unipolar source (VEL, ENV, ..) the same at */
        if (morph_cut_route(o, x, (uint32_t)k))         /* both: its amount, at half (its mean) */
            n += (x[MSLOT_ID(k) + 2u] - o[MSLOT_ID(k) + 2u]) << 6;
    return n;
}
/* the low-pass's net cutoff shift kept within -win / 2 .. +win (win: the AMOUNT window, steps; closing costs loudness
 * faster than opening; closing never beyond MORPH_CUT_DOWN steps, ~2.3 octaves): the moves that push it out turn the other
 * way (the largest first), then are shortened. ft: the filter's type, 0 LP, 1 BP, 2 HP (the directions swap: a lower band
 * lets the note's body through), 3 notch (as LP); a band-pass within -win / 2 .. +win / 2 (lower was up to +12 dB on a
 * bell, higher as much quieter). A route to CUT from a unipolar source counts with its amount at half */
static void morph_cut_steer(int8_t *end, const int8_t *o, uint32_t cut, int32_t win, int32_t ft)
{
    int32_t dn = win / 2 < MORPH_CUT_DOWN ? win / 2 : MORPH_CUT_DOWN, lo = -(dn << 7), hi = ft == 1 ? dn << 7 : win << 7;
    int32_t n, k, pass, ni = 3, sg = ft == 1 || ft == 2 ? -1 : 1;
    uint32_t id[3 + NMSLOT];
    id[0] = cut;
    id[1] = P_ED_FLT;
    id[2] = P_E2FLT;
    for (k = 0; k < (int32_t)NMSLOT; k++)
        if (morph_cut_route(o, end, (uint32_t)k))
            id[ni++] = MSLOT_ID(k) + 2u;
    for (pass = 0; pass < 2; pass++)
        for (k = 0; k < ni; k++) {
            int32_t best = -1, bm = 0, j;
            n = morph_cut_net(o, end, cut) * sg;
            if (n >= lo && n <= hi)
                return;
            for (j = 0; j < ni; j++) {                  /* the largest move pushing out (weighted as in the net); first */
                int32_t m = (end[id[j]] - o[id[j]]) * sg * (j ? (j < 3 ? 112 : 64) : 128), r = 2 * o[id[j]] - end[id[j]];
                if (!pass && (r < (j ? -64 : 0) || r > (j ? 63 : 127)))
                    continue;                           /* (only those that can turn: first) */
                if ((n < lo ? -m : m) > bm) {
                    bm = n < lo ? -m : m;
                    best = j;
                }
            }
            if (best < 0)
                break;
            if (!pass) {
                end[id[best]] = (int8_t)(2 * o[id[best]] - end[id[best]]);   /* the other way, as far */
            } else {                                    /* shortened toward the Original until inside */
                while (end[id[best]] != o[id[best]]) {
                    end[id[best]] = (int8_t)(end[id[best]] + (o[id[best]] > end[id[best]] ? 1 : -1));
                    n = morph_cut_net(o, end, cut) * sg;
                    if (n >= lo && n <= hi)
                        return;
                }
            }
        }
}
/* the envelope's loudness: SUS within about -2 / +2 dB of the Original's (a level, not a tone); on a sound that decays
 * (SUS under 48), DEC within ~28 % of the AMOUNT window (its length is the body's loudness there: a pluck's DEC 1 s ->
 * 0.4 s was -4 dB) */
static void morph_env_level(int8_t *end, const int8_t *o, uint32_t amount)
{
    int32_t s = o[P_SUS], lo = s - s / 5, hi = s + s / 4 + 2, d = (int32_t)amount * 127 / 450;
    if (s >= 24 && end[P_SUS] < lo)
        end[P_SUS] = (int8_t)(2 * s - end[P_SUS] <= (hi < 127 ? hi : 127) ? 2 * s - end[P_SUS] : lo);
    if (end[P_SUS] > hi && hi < 127)
        end[P_SUS] = (int8_t)(2 * s - end[P_SUS] >= (s >= 24 ? lo : 0) ? 2 * s - end[P_SUS] : hi);
    if (s < 48 && d > 0 && (end[P_DEC] > o[P_DEC] + d || end[P_DEC] < o[P_DEC] - d))
        end[P_DEC] = (int8_t)(end[P_DEC] > o[P_DEC] ? o[P_DEC] + d : o[P_DEC] - d);
}
/* PHASE: the resonant waves (RSAW RTRI RTRP) sound ~9 dB under the others: a roll stays on the Original's side */
#define PD_RESO(w) ((w) >= 5)
static int32_t morph_pd_side(int32_t w, int32_t o)      /* w moved into o's side (never o itself when there is another) */
{
    int32_t lo = PD_RESO(o) ? 5 : 0, n = PD_RESO(o) ? 3 : 5, k;
    if (PD_RESO(w) == PD_RESO(o))
        return w;
    k = lo + (w + o) % (n - 1);
    return k >= o ? k + 1 : k;
}
/* the multimode filter's rolled CUT / RES / MODE / SLOPE (SMART and WILD, DUELL 0.2.9): never silent or dead. The output is the low-pass, band-pass and
 * high-pass shares of the rolled MODE (voice.c uf_run: lw, bw, hw in 1/64); a share of at least a quarter needs its CUT where the content is:
 * band-pass share: CUT 30 .. 50 (~500 Hz .. 2 kHz) and SLOPE 12 dB, RES at most MORPH_UF_RESBP (a thin resonant slice otherwise); low-pass share
 * also CUT >= 45 (~1.3 kHz); high-pass share also CUT <= 40 (with a band-pass share) or <= 25 (~210 Hz, without one). So no kill zone, no closed end,
 * and the level stays within ~26 dB of the original's. The original's own CUT is kept where it is already beyond and its MODE is near (within 8).
 * RES never above max(original, MORPH_UF_RES), no self-oscillation level */
static void morph_uf_range(const int8_t *o, int32_t m, int32_t *lo, int32_t *hi, int32_t *bp)
{
    int32_t oc = o[P_FREEA];
    int32_t bw = m <= 64 ? m : (127 - m) * 64 / 63, lw = m < 64 ? 64 - m : 0, hw = m > 64 ? (m - 64) * 64 / 63 : 0;
    *lo = 0;
    *hi = UF_STEPS;
    *bp = bw >= 16;
    if (bw >= 16) {
        *lo = lw >= 16 ? MORPH_UF_LO : hw >= 16 ? 24 : MORPH_UF_BP0;
        *hi = hw >= 16 ? 30 : MORPH_UF_BP1;
    } else if (lw >= 16) {
        *lo = MORPH_UF_LO;
    } else if (hw >= 16) {
        *hi = MORPH_UF_HI;
    }
    if (m - o[P_FREEC] <= 8 && o[P_FREEC] - m <= 8) {
        *lo = oc < *lo ? oc : *lo;
        *hi = oc > *hi ? oc : *hi;
    }
}
static void morph_uf_safe(const int8_t *o, int8_t *end, uint32_t amount)
{
    int32_t m = end[P_FREEC], c = end[P_FREEA], oc = o[P_FREEA], lo, hi, bp, cc, lim = (int32_t)amount * UF_STEPS / 100, it;
    for (it = 0; it < 64; it++) {                         /* MODE comes back toward the original's until the CUT it needs is within AMOUNT of the original's */
        morph_uf_range(o, m, &lo, &hi, &bp);
        cc = c < lo ? lo : c > hi ? hi : c;
        if ((cc - oc <= lim && oc - cc <= lim) || m == o[P_FREEC])
            break;
        m += m < o[P_FREEC] ? 2 : -2;
        if ((m - o[P_FREEC]) * (end[P_FREEC] - o[P_FREEC]) < 0)
            m = o[P_FREEC];
    }
    end[P_FREEC] = (int8_t)m;
    morph_uf_range(o, m, &lo, &hi, &bp);
    end[P_FREEA] = (int8_t)(c < lo ? lo : c > hi ? hi : c);
    if (bp) {
        end[P_FREED] = 1;
        if (end[P_FREEB] > MORPH_UF_RESBP && end[P_FREEB] > o[P_FREEB])
            end[P_FREEB] = (int8_t)(o[P_FREEB] > MORPH_UF_RESBP ? o[P_FREEB] : MORPH_UF_RESBP);
    }
    if (end[P_FREEB] > (o[P_FREEB] > MORPH_UF_RES ? o[P_FREEB] : MORPH_UF_RES))
        end[P_FREEB] = (int8_t)(o[P_FREEB] > MORPH_UF_RES ? o[P_FREEB] : MORPH_UF_RES);
}
/* filter 1 on ANALOG and TRIO (they had a low-pass / their own types with the "no dead path" rule, morph_open, before cloud job J): a band-pass or high-pass share keeps its band where the notes
 * are over the sweeps of ENV 1 / ENV 2 / LFO -> FLT (dial steps: a depth of 58 moves the cutoff 58 steps, 7 octaves: the band left the sound). The depths go back toward the original's, then CUT
 * (inside morph_uf_safe's window again). A sine or triangle source has no harmonics above the note: its window is the lower one */
static void morph_uf_open(const int8_t *o, int8_t *end, const engine_t *e)
{
    static const uint8_t DEP[4] = {P_ED_FLT, P_E2FLT, P_LD_FLT, P_L2FLT};
    int32_t m = end[P_FREEC], bw = m <= 64 ? m : (127 - m) * 64 / 63, lw = m < 64 ? 64 - m : 0, hw = m > 64 ? (m - 64) * 64 / 63 : 0, wlo, whi, k;
    int sine = e == &ENG_ANALOG && (end[P_E0] == 2 || end[P_E0] == 3);   /* (WAVE 2 TRI, 3 SIN: eng_analog.c N_ANALOG_WAVE) */
    if (bw < 16 && hw < 16)
        return;
    wlo = bw >= 16 && hw < 16 ? MORPH_UF_BP0 - 6 : -255;
    whi = bw >= 16 ? (sine ? MORPH_UF_BP1 : MORPH_UF_BP1 + 10) : MORPH_UF_HI + 15;
    for (k = 0; k < 4; k++)
        while (!morph_band_ok(end, P_FREEA, wlo, whi) && end[DEP[k]] != o[DEP[k]])
            end[DEP[k]] = (int8_t)(end[DEP[k]] + (o[DEP[k]] > end[DEP[k]] ? 1 : -1));
}
static void morph_rules(const engine_t *e, const int8_t *o, int8_t *end, uint32_t amount)
{
    int32_t win = (int32_t)amount * 127 / 100;
    morph_env_level(end, o, amount);
    if (e == &ENG_ANALOG || e == &ENG_TRIO) {           /* their own cutoff steering (morph_cut_steer, with ENV 1 / ENV 2 / matrix routes to CUT) went with their own filters in cloud job J and
                                                         * left a hole: filter 1 closed by a rolled ENV -> FLT depth and a lower CUT silenced the sound (28 of 500 ANALOG SOFT PAD rolls under x0.1,
                                                         * the smallest x0.05; wave5: 1). Steered again, on filter 1's CUT with filter 1's MODE as the type (morph_uf_safe then keeps its windows) */
        int32_t m = end[P_FREEC], bw = m <= 64 ? m : (127 - m) * 64 / 63, lw = m < 64 ? 64 - m : 0, hw = m > 64 ? (m - 64) * 64 / 63 : 0;
        morph_cut_steer(end, o, P_FREEA, win, bw >= lw && bw >= hw && bw >= 16 ? 1 : hw > lw ? 2 : 0);
    }
    if (e == &ENG_PHASE) {
        int32_t wa = o[P_E1] ? o[P_E1] - 1 : o[P_E0];   /* WAVE2 "-" plays WAVE */
        end[P_E0] = (int8_t)morph_pd_side(end[P_E0], o[P_E0]);
        if (end[P_E1] && PD_RESO(end[P_E1] - 1) != PD_RESO(wa))
            end[P_E1] = (int8_t)(morph_pd_side(end[P_E1] - 1, wa) + 1);
    }
    if (e == &ENG_LOFI) {
        end[P_E7] = (int8_t)reflect_floor(end[P_E7], o[P_E7], 20);
        if (o[P_E0] == BITS_8BIT) {                     /* CRSH > 64 drops the 8-bit chip to a 4-bit staircase (eng_lofi.c): */
            if (o[P_E3] > 64 && end[P_E3] < 65)         /* the end stays on its side of it (3-bis: no cliff) */
                end[P_E3] = 65;
            else if (o[P_E3] <= 64 && end[P_E3] > 64)
                end[P_E3] = 64;
        }
    }
}

/* DUELL 0.2.11 (owner, 2026-10-14; SPEC 3.3 rule 3g, docs/CPU_ANALYSIS.md): SMART switches ON a stage that is off in the Original
 * with HALF the chance it had. Each of these wakes a stage that costs nothing while off (the multimode filter alone: +17 % at
 * 4 + 4 voices): the multimode filter when bypassed (MODE LP, CUT open: a roll that moves CUT or MODE switches it on), DIST, the
 * CHO / DLY / REV sends and SPREAD from 0, the FLT depths (ENV, ENV 2, LFO, LFO 2 -> FLT) and the LFO depths (LFO -> AMP, LFO -> SHP,
 * LFO 2 -> SHP) from 0. A coin each (the filter one coin for its four values) from a stream of its own (rs4: always drawn, all
 * 13 in this order, so the end stays a pure function of the seed and every other value draws as before): tails puts the
 * Original's value back (off). A stage already on in the Original rolls as before. WILD: unchanged (not called) */
static const uint8_t MORPH_WAKE[] = {P_DIST, P_CHOR, P_DLY, P_REV, P_SPRD, P_ED_FLT, P_E2FLT, P_LD_FLT, P_L2FLT, P_LD_AMP, P_LD_SHP, P_L2SHP};
static void morph_wake_half(const int8_t *o, int8_t *end, const engine_t *e, uint32_t seed, uint32_t part)
{
    uint32_t rs4 = morph_state(seed, part + 24u), k, id;
    if (!(mr_next(&rs4) >> 31) && e->ufilt && o[P_FREEA] >= UF_STEPS && o[P_FREEC] <= 0)
        for (id = P_FREEA; id <= P_FREED; id++)
            end[id] = o[id];
    for (k = 0; k < NELEM(MORPH_WAKE); k++)
        if (!(mr_next(&rs4) >> 31) && !o[MORPH_WAKE[k]])
            end[MORPH_WAKE[k]] = 0;
}

/* The five ids the TWIN FILTER took over (RES 2 25 and DRIVE 26 were ROOT and SCALE, TYPE 43 was ALLOC, CUTOFF 2 106 the DUCK NOTE, BLEND 101 ANALOG's filter TYPE) keep the draws their old
 * tags made on the roll's stream, so every other id of a stored seed rolls the End B it did (a patch's saved End B is regenerated from its seed): ROOT and SCALE (a pitch item) a step only
 * with PITCH on, ALLOC (a hard switch) only in WILD, ANALOG's TYPE (a fade) on ANALOG in both, the DUCK NOTE (never) none; a step is a coin, then, when it comes up, a second draw for the
 * other value. Their new values roll from a stream of their own (morph_roll_pure rsx) */
static void morph_old_draw(uint32_t *r, const engine_t *e, uint32_t id, uint32_t amount)
{
    uint32_t step = id == P_RES2 || id == P_DRV2 ? (mrc & MC_PIT) : id == P_TWTYPE ? (mrc & MC_WILD) : id == P_BLEND ? e == &ENG_ANALOG : 0u;
    if (step && mr_below(r, 100u) < amount)
        (void)mr_next(r);
}

/* The TWIN FILTER's five ids and the extension slots (ext.h) roll from a stream of their own (part + 40), in id order and then slot order, after every other value: the draws of every
 * other id are the ones they were. A value rolls by its tag (morph_tag: continuous, or a switch that flips in WILD only); TYPE never to OFF nor, on WAVE and PHYS, to COMB */
static void morph_twin_roll(const int8_t *o, int8_t *end, const engine_t *e, uint32_t amount, uint32_t seed, uint32_t part)
{
    static const uint8_t TWI[5] = {P_RES2, P_DRV2, P_TWTYPE, P_BLEND, P_CUT2};
    uint32_t rsx = morph_state(seed, part + 40u), k, comb = end[P_TWTYPE] == TT_COMB;
    for (k = 0; k < 5u + X_CYSYNC + 1u; k++) {
        uint32_t id = k < 5u ? TWI[k] : XW_BASE + k - 5u;
        mtag_t g = morph_tag(e, o, id, 1);
        const param_desc_t *pd = morph_desc(e, id);
        if (g.kind == MK_CONT) {
            end[id] = (int8_t)morph_cont(&rsx, g, pd, o[id], amount);
        } else if (g.kind == MK_STEP) {
            if (mr_below(&rsx, 100u) < amount)
                end[id] = (int8_t)(id == P_TWTYPE ? morph_other(&rsx, TT_LP, (e == &ENG_WAVE || e == &ENG_PHYS ? TT_LADDER : TT_COMB) + 1, o[id])
                                                  : morph_other(&rsx, pd->min, pd->max + 1, o[id]));
        } else if (g.kind == MK_EX && (g.flags & MF_DRAW)) {
            (void)mr_next(&rsx);
        }
    }
    comb = end[P_TWTYPE] == TT_COMB;                    /* each value in the range of its meaning at this end (CUTOFF 2: OFFSET / CUTOFF / TUNE, RES 2: RES / FDBK) */
    if (end[P_CUT2] != o[P_CUT2])
        end[P_CUT2] = (int8_t)clamp(end[P_CUT2], comb ? -24 : end[XW_BASE + X_LINK] ? -TW_OFF_MAX : 0, comb ? 24 : end[XW_BASE + X_LINK] ? TW_OFF_MAX : UF_STEPS);
    if (end[P_RES2] != o[P_RES2])
        end[P_RES2] = (int8_t)clamp(end[P_RES2], comb ? -64 : 0, comb ? 63 : 127);
}
/* SMART's safety rules for filter 2 (TWIN_FILTER_DESIGN 3.3, the multimode filter's morph_uf_range / ANALOG's resonance rule): RES 2 above 100 is never rolled with more DRIVE; no dead cutoff 2: the
 * cutoff it works at (filter 1's, plus OFFSET, with LINK on; CUTOFF 2 itself with LINK off) stays where the type is heard: a low-pass or LADDER at or above min(the original's, MORPH_UF_LO), a
 * high-pass at or below max(the original's, MORPH_UF_HI), a band-pass within min .. max of its own and MORPH_UF_BP0 .. MORPH_UF_BP1. CUTOFF 2 itself moves the least that is needed (filter 1's own
 * end has its rules). DRIVE's "never far above the original" is the tag (MF_RISE). A notch (never silent) and COMB (TUNE follows the key) have no window. TYPE OFF: nothing */
static void morph_twin_rules(const int8_t *o, int8_t *end)
{
    int32_t ty = end[P_TWTYPE], link = end[XW_BASE + X_LINK], be = link ? end[P_FREEA] : 0, bo = o[XW_BASE + X_LINK] ? o[P_FREEA] : 0, eo = bo + (o[XW_BASE + X_LINK] ? TW_OFF_STEPS(clamp(o[P_CUT2], -TW_OFF_MAX, TW_OFF_MAX)) : o[P_CUT2]), lo = -255, hi = 255, c, cd;   /* (1.0: with LINK on CUTOFF 2 is whole semitones, the window is in dial steps) */
    if (ty <= TT_OFF)
        return;
    if (ty != TT_COMB && end[P_RES2] > (o[P_RES2] > 100 ? o[P_RES2] : 100) && end[P_DRV2] > o[P_DRV2])
        end[P_DRV2] = o[P_DRV2];
    if (ty == TT_LP || ty == TT_LADDER)
        lo = eo < MORPH_UF_LO ? eo : MORPH_UF_LO;
    else if (ty == TT_HP)
        hi = eo > MORPH_UF_HI ? eo : MORPH_UF_HI;
    else if (ty == TT_BP) {
        lo = eo < MORPH_UF_BP0 ? eo : MORPH_UF_BP0;
        hi = eo > MORPH_UF_BP1 ? eo : MORPH_UF_BP1;
    } else
        return;
    c = end[P_CUT2];
    cd = link ? TW_OFF_STEPS(clamp(c, -TW_OFF_MAX, TW_OFF_MAX)) : c;
    if (cd + be < lo)
        c = link ? TW_STEPS_OFF(lo - be) : lo - be;
    else if (cd + be > hi)
        c = link ? TW_STEPS_OFF(hi - be) : hi - be;
    end[P_CUT2] = (int8_t)clamp(c, link ? -TW_OFF_MAX : 0, link ? TW_OFF_MAX : UF_STEPS);
}

/* an end from the original o (EXT_P_LEN values of the part: the ids, a gap at 127, the extension slots; its engine e): AMOUNT 0..100, seed 1..65535, the part (its
 * own stream), stereo: STEREO mode. Pure: the same arguments give the same end */
static void morph_roll_full(const int8_t *o, int8_t *end, const engine_t *e, uint32_t amount, uint32_t seed,
                            uint32_t part, uint32_t stereo)
{
    uint32_t rs = morph_state(seed, part), rs2 = morph_state(seed, part + 8u), rs3 = morph_state(seed, part + 16u), rs5 = morph_state(seed, part + 32u), id;
    for (id = 0; id < EXT_P_LEN; id++)
        end[id] = o[id];
    if (!amount)
        return;
    amount = amount > 100u ? 100u : amount;
    for (id = 0; id < P_COUNT; id++) {
        mtag_t g = morph_tag(e, o, id, stereo);
        const param_desc_t *pd = e == &ENG_WAVE && id == P_E6 ? &MORPH_E7_OLD : morph_desc(e, id);
        uint32_t *r = id >= P_E2FLT && id <= P_L2SHP ? &rs2 : &rs;   /* (ENV 2 / LFO 2 DEST: their own stream, so a seed of
                                                                       * before draws every other value as it did) */
        if (MORPH_OLD(id)) {                             /* the TWIN FILTER's ids: the draws they had (task 3 rolls their new values) */
            morph_old_draw(r, e, id, amount);
            continue;
        }
        if (id == P_KTRK) {                              /* 0.2.21: KEY TRK, on a stream of its own (morph_tag) */
            if (g.kind == MK_CONT)
                end[id] = (int8_t)morph_cont(&rs5, g, pd, o[id], amount);
            continue;
        }
        if (id >= P_FREEA && id <= P_FREED && e->ufilt) {   /* DUELL 0.2.2: the filter's CUT, RES (0.2.9: MODE, SLOPE): rs3 (so a seed of before draws every */
            if (id == P_FREED)                           /* other value as it did); the SLICER's draws they had stay on rs */
                (void)mr_next(r);
            else if ((mrc & MC_WILD) && mr_below(r, 100u) < amount)
                (void)mr_next(r);
            if (g.kind == MK_CONT)
                end[id] = (int8_t)morph_cont(&rs3, g, pd, o[id], amount);
            else if (g.kind == MK_FADE && mr_below(&rs3, 100u) < amount)
                end[id] = (int8_t)morph_other(&rs3, pd->min, pd->max + 1, o[id]);
        } else if (g.kind == MK_CONT) {
            end[id] = (int8_t)morph_cont(r, g, pd, o[id], amount);
        } else if (g.kind == MK_FADE) {
            if (mr_below(r, 100u) < amount)
                end[id] = (int8_t)morph_other(r, 0, (g.flags & MF_NFADE) ? g.lim : pd->max + 1, o[id]);
        } else if (g.kind == MK_EX && (g.flags & MF_DRAW)) {
            (void)mr_next(r);                           /* (v2: a pitch item PITCH OFF leaves alone: the draw it had) */
        } else if (g.kind == MK_EX && (g.flags & MF_GONE)) {   /* the SLICER's retired ids: the draws they had, nothing set */
            if (g.lim)
                (void)mr_next(r);                       /* DEPTH (continuous): its move, SMART and WILD */
            else if ((mrc & MC_WILD) && mr_below(r, 100u) < amount)
                (void)mr_next(r);                       /* SLCR PAT RATE (WILD: a switch that flips, the pick's draw) */
        } else if (g.kind == MK_STEP) {                 /* v2: a choice that flips (WILD, PITCH, ARP) */
            if (mr_below(r, 100u) < amount && !(id == P_AMODE && !o[id]))   /* (an arp that is off stays off) */
                end[id] = (int8_t)morph_other(r, id == P_AMODE ? 1 : pd->min, pd->max + 1, o[id]);
        } else if (g.kind == MK_SLOT && id >= P_M1SRC && (id - P_M1SRC) % 3u == 0u) {
            morph_slot(&rs, e, o, end, (id - P_M1SRC) / 3u, amount, stereo);
        }
    }
    if (e->ufilt == 1u && o[P_FREEA] >= UF_STEPS && o[P_FREEC] <= 0 && !(mrc & (MC_WILD | MC_LIST)))   /* DUELL 0.2.5: a FLT depth at 0 stays 0 while the original's filter is bypassed (MODE LP, CUT open): a route is not a
                                                         * way to switch it on (0.2.9: SMART switches it on by CUT / RES / MODE / SLOPE only; the rolls and draws are as ever, only this value is put back) */
        for (id = P_ED_FLT; id <= P_L2FLT; id++)
            if ((id == P_ED_FLT || id == P_LD_FLT || id == P_E2FLT || id == P_L2FLT) && !o[id])
                end[id] = 0;
    if (!(mrc & (MC_WILD | MC_LIST)))
        morph_wake_half(o, end, e, seed, part);
    end[P_TRIM] = o[P_TRIM];                            /* DUELL 0.2.19: the PATCH TRIM never moves (a morph keeps the sound's loudness; the MIXER level, P_LEVEL, is T_NEV). Its table
                                                         * entry and draw are those of the legacy DUCK HOLD (0.2.5: never read, value never moved) it replaced, so the streams of old seeds
                                                         * (a patch's saved End B) stay */
    if (mrc & MC_WILD)                                  /* SMART adds no route (owner, 2026-10-12) */
        morph_add_routes(&rs, e, o, end, amount, stereo);
    if (!(mrc & MC_WILD))
        morph_rules(e, o, end, amount);                 /* (WILD: no dead-filter, resonance, tone or loudness rule) */
    if (e->ufilt)
        morph_uf_safe(o, end, amount);                   /* (the multimode filter, SMART and WILD: never a silent or dead combination) */
    if ((e == &ENG_ANALOG || e == &ENG_TRIO) && !(mrc & MC_WILD))
        morph_uf_open(o, end, e);
    if (e->ufilt && end[P_FREEA] >= UF_STEPS && end[P_FREEC] <= 0)   /* KEY TRK never switches the filter on: an End B whose filter is bypassed keeps the original's KEY TRK */
        end[P_KTRK] = o[P_KTRK];
    morph_twin_roll(o, end, e, amount, seed, part);     /* the TWIN FILTER's, the sub's and CYCLE's values: their own stream, after everything else */
    if (!(mrc & MC_WILD))
        morph_twin_rules(o, end);
}
/* the ids only (P_COUNT values in and out; the extension slots at their defaults): the roll of the values every store has had since before the slots, the one the tests of the old ids use */
static void morph_roll_pure(const int8_t *o, int8_t *end, const engine_t *e, uint32_t amount, uint32_t seed,
                            uint32_t part, uint32_t stereo)
{
    int8_t of[EXT_P_LEN], ef[EXT_P_LEN];
    uint32_t k;
    memcpy(of, o, P_COUNT);
    of[P_COUNT] = 0;
    for (k = 0; k < NX; k++)
        of[XW_BASE + k] = (int8_t)XTP[k].def;
    morph_roll_full(of, ef, e, amount, seed, part, stereo);
    memcpy(end, ef, P_COUNT);
}

/* ------------------------------------------------------------------ the parts' state --- */
typedef struct { uint8_t id, kind, mx; int8_t lo, hi; } ment_t;   /* a value the fader moves (morph_build) */
#define MK_STEREO 0x80u                                  /* ment_t.kind: PAN / SPREAD, only while duell_stereo */
#define MV_N (P_COUNT + NX)                              /* values the fader can move at once: every id and every extension slot */
typedef struct {
    int8_t base[EXT_P_LEN];                              /* the Original the ends belong to (by wide id: the ids, a gap at 127, the extension slots) */
    int8_t h[2][MORPH_HIST][P_COUNT];                    /* each end's rolls; h[e][top[e]] is the end */
    int8_t hx[2][XH][NX];                                /* the extension slots' (XH deep, ring index = the ids' index mod XH): hx[e][top[e] % XH] is the end's */
    uint8_t xv[2];                                       /* .. how many of the entries ending at top[e] hold a roll's slots (1 .. XH; fewer after an UNDO past XH rolls: the slots at the Original) */
    uint16_t seed[2][MORPH_HIST];                        /* .. their seeds (0: not a roll: the Original) */
    uint8_t rcfg[2][MORPH_HIST], ramt[2][MORPH_HIST];    /* v2: .. the MC_* bits and the AMOUNT each was rolled with */
    uint8_t top[2], n[2];                                /* n: entries kept (1 .. MORPH_HIST) */
    uint8_t fader, amount;                               /* 0..127, 0..100 */
    uint8_t aset;                                        /* the AMOUNT the player last chose (knob, MIDI, settings): what the settings keep; a patch load changes mp->amount only */
    uint8_t apre;                                        /* v2: amount was set from the settings before the first use */
    uint8_t pin[EDITB_PINH][PIN_BYTES];                  /* EDITABLE B: End B's values set by hand, a bit per wide id, for each of its history entries (index = the entry's index mod EDITB_PINH) */
    uint8_t bstay;                                       /* .. End B's current entry was made by a stay at the B end (the edits since arriving go into it; a roll, UNDO or leaving 127 ends it) */
    uint8_t eng, preset, user;                           /* the sound base came from (t->eng_req, preset, user) */
    uint8_t nid;                                         /* the values the fader moves (ent): 0 = idle */
    uint8_t fb[NFM];                                     /* FM6: the Original's patch bytes (fm_byte order) the ends belong to */
    uint8_t fh[2][MORPH_HIST][NFM];                      /* .. each end's (parallel to h) */
    uint8_t fsm[NFM];                                    /* .. the ramp's old end */
    uint8_t fnid;                                        /* some FM6 patch byte differs: the hook (fm6_morph) blends */
    uint16_t fser;                                       /* .. the list's serial (the hook's key) */
    ment_t ent[MV_N];
    int8_t sm_old[P_COUNT];                              /* the ~50 ms ramp of an end replaced while heard: it was */
    int8_t smx_old[NX];                                  /* .. (its extension slots) */
    uint8_t sm_end;                                      /* .. that end */
    uint16_t sm_r;                                       /* .. Q15 progress, 32768 = none */
    /* the hook's cache (morph_begin): the values it writes (cid: those that differ from the live ones, and every faded
     * choice and route), the live values they replace (clive, put back by morph_end) and the blend (cval, cxf). Valid while
     * the list, the fader, the stereo mode and those live values stay (no ramp): a block then only writes cval */
    uint8_t cvalid, cfader, cst, cn;
    uint8_t cid[MV_N];
    int16_t clive[MV_N], cval[MV_N];
    mxf_t cxf;
} morph_part_t;
static morph_part_t mph[NSHOWN];
static struct {
    uint8_t cfg;                                         /* v2 (SPEC 3.5): MC_WILD (SMART off), MC_ARP, MC_PIT: the settings */
    uint8_t target;                                      /* MT_SELECTED / MT_BOTH */
    uint16_t seed;                                       /* the last roll's (shown) */
    uint32_t gen;                                        /* the seed generator */
} morph;

static const int8_t *morph_cur(const morph_part_t *mp, uint32_t e) { return mp->h[e][mp->top[e]]; }
static const int8_t *morph_curx(const morph_part_t *mp, uint32_t e) { return mp->hx[e][mp->top[e] % XH]; }   /* end e's extension slots (X0 .. X15) */
static int32_t morph_wb(uint32_t f) { return f >= 127u ? 32768 : (int32_t)((f * 32768u + 63u) / 127u); }   /* B's weight */
static uint32_t morph_mx(uint32_t id)
{
    return id == P_LWAVE ? MX_LWAVE : id == P_L2WAVE ? MX_L2WAVE : id == P_FREED ? MX_SLOPE : id >= P_E0 ? MX_E0 + id - P_E0 : 0xFFu;
}

static int morph_isfm(const morph_part_t *mp) { return mp->eng == ENGI_FM6; }
static uint32_t morph_fm_car(uint32_t pi, const int8_t *base)   /* the carriers of the part's algorithm (macro or patch) */
{
    uint32_t alg = base[P_E0] >= 1 && base[P_E0] <= 32 ? (uint32_t)base[P_E0] - 1u : fm6_patch[pi % NTRK][FP_ALG] & 31u;
    return fm6_carriers(alg);
}
/* FM6 patch byte i (fm_byte order) as the fader sounds it: the ends blended as the t->p values are (Q8, rounded, the ramp) */
static int32_t morph_fm_v(const morph_part_t *mp, uint32_t i, int32_t w, int ramp)
{
    const uint8_t *A = mp->fh[0][mp->top[0]], *B = mp->fh[1][mp->top[1]], *O = mp->fsm;
    int32_t a8 = A[i] << 8, b8 = B[i] << 8, r = mp->sm_r;
    uint32_t X = mp->sm_end;
    if (ramp && r < 32768 && O[i] != (X ? B : A)[i]) {
        int32_t x8 = (O[i] << 8) + ((((X ? B : A)[i] - O[i]) * 256 * r) >> 15);
        if (X)
            b8 = x8;
        else
            a8 = x8;
    }
    return (a8 + (((b8 - a8) * w) >> 15) + 128) >> 8;
}
static uint32_t morph_fm6(uint32_t tr, uint8_t *p);
static int morph_stale(const track_t *t, const morph_part_t *mp);
static void morph_fm_live(uint32_t pi, uint8_t *live);

#if defined(FM1_IRQ_TARGET) || defined(HOST_IRQ_HOOK)    /* (HOST_IRQ_HOOK: tests/loadcost_test.c counts the windows) */
#define MORPH_LOCK() fm1_irq_off()
#define MORPH_UNLOCK() fm1_irq_on()
#else
#define MORPH_LOCK() ((void)0)                           /* (host builds: one thread) */
#define MORPH_UNLOCK() ((void)0)
#endif

/* the list of values the fader moves: those where an end (or the ramp's old end) differs from the Original */
static void morph_build(morph_part_t *mp)
{
    const engine_t *e = ENGINES[eng_idx(mp->eng)];
    const int8_t *a = morph_cur(mp, 0), *b = morph_cur(mp, 1), *s = mp->sm_old;
    const int8_t *ax = morph_curx(mp, 0), *bx = morph_curx(mp, 1), *sx = mp->smx_old;   /* (the extension slots: the same by their slot number) */
    uint32_t ii, id, q, n = 0, ramp = mp->sm_r < 32768u;
    const uint8_t *pn = mp->pin[mp->top[1] % EDITB_PINH];   /* End B's values set by hand */
    uint8_t sv = mrc;
    mrc = MC_ARP | MC_PIT | MC_LIST;                     /* v2: smart, permissive: whatever an end was rolled with is listed */
    for (ii = 0; ii < MV_N; ii++) {
        mtag_t g;
        uint32_t w, diff = 0, x = ii >= P_COUNT, ix = x ? ii - P_COUNT : ii, wq = x ? 1u : P_COUNT - ii < 3u ? P_COUNT - ii : 3u;
        const int8_t *A = x ? ax : a, *B = x ? bx : b, *S = x ? sx : s, *O = x ? mp->base + XW_BASE : mp->base;
        id = x ? XW_BASE + ix : ii;
        for (q = 0; q < wq; q++)                        /* nothing differs here (a slot is 3 wide): listed whatever the tag says, so */
            diff |= A[ix + q] != O[ix + q] || B[ix + q] != O[ix + q] ||   /* skip the tag lookup (most values: it */
                    (ramp && S[ix + q] != O[ix + q]) ||  /* cost ~15 k instructions a build, with interrupts off) */
                    (pn[(id + q) >> 3] >> ((id + q) & 7u) & 1u);   /* (a value set by hand is listed even where it equals the Original: the Original may move) */
        if (!diff)
            continue;
        diff = 0;
        g = morph_tag(e, mp->base, id, 1);
        w = g.kind == MK_SLOT ? 3u : 1u;
        if (g.kind == MK_EX || g.kind == MK_HARD ||     /* (a WILD end: a switch flips; a fade outside its values flips) */
            (g.kind == MK_FADE && (g.flags & MF_NFADE) &&
             (A[ix] < 0 || A[ix] >= g.lim || B[ix] < 0 || B[ix] >= g.lim || S[ix] < 0 || S[ix] >= g.lim)))
            g.kind = MK_STEP;
        if ((id == P_CUT2 && ax[X_LINK] != bx[X_LINK]) ||     /* the two meanings of CUTOFF 2 (OFFSET / CUTOFF) and of RES 2 (RES / FEEDBACK) do not blend: when the ends differ in LINK / in being COMB */
            (id == P_RES2 && (a[P_TWTYPE] == TT_COMB) != (b[P_TWTYPE] == TT_COMB)))   /* the value steps with the switch, at the middle of the fader */
            g.kind = MK_STEP;
        if (e == &ENG_WAVE && id == P_E6 && A[ix] && B[ix] && (!ramp || S[ix]))
            g.kind = MK_CONT;                           /* WAVE's STEPS between two counts: a blend (rounded); OFF against a count stays a flip at the middle (the count would pass through 2 places at the OFF end) */
        if (g.kind != MK_CONT && g.kind != MK_FADE && g.kind != MK_SLOT && g.kind != MK_STEP)
            continue;
        if (g.kind == MK_SLOT && (id - P_M1SRC) % 3u)
            continue;                                   /* (the slot's DST and AMT go with its SRC) */
        for (q = 0; q < w; q++)
            diff |= A[ix + q] != O[ix + q] || B[ix + q] != O[ix + q] ||
                    (ramp && S[ix + q] != O[ix + q]) || (pn[(id + q) >> 3] >> ((id + q) & 7u) & 1u);
        if (!diff)
            continue;
        mp->ent[n].id = (uint8_t)id;
        q = g.kind == MK_SLOT ? id + 2u : id;           /* (a slot's amount is the value that can be set by hand) */
        mp->ent[n].kind = (uint8_t)(g.kind | ((g.flags & MF_STEREO) ? MK_STEREO : 0u) | ((pn[q >> 3] >> (q & 7u) & 1u) ? MK_PIN : 0u));
        mp->ent[n].mx = (uint8_t)morph_mx(id);
        mp->ent[n].lo = (int8_t)morph_desc(e, id)->min;
        mp->ent[n].hi = (int8_t)morph_desc(e, id)->max;
        n++;
    }
    mrc = sv;
    mp->nid = (uint8_t)n;
    mp->cvalid = 0;
    mp->fnid = 0;
    if (morph_isfm(mp)) {
        for (id = 0; id < NFM; id++)
            mp->fnid |= mp->fh[0][mp->top[0]][id] != mp->fb[id] || mp->fh[1][mp->top[1]][id] != mp->fb[id] ||
                        (ramp && mp->fsm[id] != mp->fb[id]);
        mp->fser++;
        if (mp->fnid)
            fm6_morph = morph_fm6;
    }
}

/* end X's weight in the ramp's dip: the B weight w with X's share scaled by |1 - 2r| */
static int32_t morph_dip(int32_t w, uint32_t x, int32_t r)
{
    int32_t d = r < 16384 ? 32768 - 2 * r : 2 * r - 32768;
    return x ? (w * d) >> 15 : 32768 - (((32768 - w) * d) >> 15);
}

/* the blend into p (in place: each value is read, then written): B's weight w (Q15, 0..32768), ramp: the ~50 ms ramp
 * applies; the faded choices into xf. p holds the live values (the Original as edited) */
static void morph_apply(const morph_part_t *mp, int16_t *p, int32_t w, int ramp, mxf_t *xf)
{
    const int8_t *A0 = morph_cur(mp, 0), *B0 = morph_cur(mp, 1), *base0 = mp->base, *O0 = mp->sm_old;
    const int8_t *Ax = morph_curx(mp, 0), *Bx = morph_curx(mp, 1), *basex = mp->base + XW_BASE, *Ox = mp->smx_old;   /* (the extension slots, by slot number) */
    uint32_t j, X = mp->sm_end;
    int32_t r = mp->sm_r;
    ramp = ramp && r < 32768;
    for (j = 0; j < mp->nid; j++) {
        const ment_t *en = &mp->ent[j];
        uint32_t id = en->id, kind = en->kind, x = id >= XW_BASE, ix = x ? id - XW_BASE : id;
        const int8_t *A = x ? Ax : A0, *B = x ? Bx : B0, *O = x ? Ox : O0, *base = x ? basex : base0;
        uint32_t pin = kind & MK_PIN;                    /* End B's value was set by hand: it is the sound at the B end, whatever the Original has become */
        if ((kind & MK_STEREO) && !duell_stereo)
            continue;
        kind &= 0x3Fu;
        if (kind == MK_CONT) {
            int32_t a8 = A[ix] << 8, b8 = B[ix] << 8, v;
            if (ramp && O[ix] != (X ? B : A)[ix]) {      /* the replaced end glides from what it was */
                int32_t o8 = O[ix] << 8, x8 = o8 + ((((X ? B : A)[ix] - O[ix]) * 256 * r) >> 15);
                if (X)
                    b8 = x8;
                else
                    a8 = x8;
            }
            if (pin)
                a8 += (p[id] - base[ix]) << 8;           /* (A's side still follows the Original) */
            v = (a8 + (((b8 - a8) * w) >> 15) + 128) >> 8;
            p[id] = (int16_t)clamp(pin ? v : p[id] + v - base[ix], en->lo, en->hi);
        } else if (kind == MK_FADE) {
            int32_t a = A[ix], b = B[ix], we = w;
            if (p[id] != base[ix]) {
                if (!pin)
                    continue;                           /* the user's choice */
                a = p[id];                              /* (a pinned B: the B end is B's, the rest is the user's) */
            }
            if (ramp && O[ix] != (X ? B : A)[ix]) {      /* dip: the old value out, the new one in */
                if (r < 16384) {
                    if (X)
                        b = O[ix];
                    else
                        a = O[ix];
                }
                we = morph_dip(w, X, r);
            }
            if (a == b || we <= 0) {
                p[id] = (int16_t)a;
            } else if (we >= 32768) {
                p[id] = (int16_t)b;
            } else {
                p[id] = (int16_t)a;
                xf->b[en->mx] = (uint8_t)b;
                xf->w[en->mx] = (uint16_t)we;
                xf->on = 1;
            }
        } else if (kind == MK_STEP) {                   /* v2: a switch flips at the middle of the fader */
            if (p[id] != base[ix] && (!pin || w < 16384))
                continue;                               /* the user's choice */
            p[id] = (int16_t)(w < 16384 ? A[ix] : B[ix]);
        } else {                                        /* MK_SLOT: SRC DST AMT at id .. id + 2 */
            int32_t sa = A[ix], da = A[id + 1u], aa = A[id + 2u] << 8, sb = B[ix], db = B[id + 1u], ab = B[id + 2u] << 8;
            int32_t we = w, s, d, v8;
            if (p[id] != base[ix] || p[id + 1u] != base[id + 1u])
                continue;                               /* the user's route */
            if (ramp && (O[ix] != (X ? B : A)[ix] || O[id + 1u] != (X ? B : A)[id + 1u] ||
                         O[id + 2u] != (X ? B : A)[id + 2u])) {
                const int8_t *N = X ? B : A;
                if (O[ix] == N[id] && O[id + 1u] == N[id + 1u]) {   /* the same route: its amount glides */
                    int32_t x8 = (O[id + 2u] << 8) + (((N[id + 2u] - O[id + 2u]) * 256 * r) >> 15);
                    if (X)
                        ab = x8;
                    else
                        aa = x8;
                } else {                                /* another route: dip */
                    if (r < 16384) {
                        if (X) {
                            sb = O[ix];
                            db = O[id + 1u];
                            ab = O[id + 2u] << 8;
                        } else {
                            sa = O[ix];
                            da = O[id + 1u];
                            aa = O[id + 2u] << 8;
                        }
                    }
                    we = morph_dip(w, X, r);
                }
            }
            if (pin)
                aa += (p[id + 2u] - base[id + 2u]) << 8; /* (a pinned amount: A's side follows the Original) */
            if (sa == sb && da == db) {                 /* one route: the amount moves */
                s = sa;
                d = da;
                v8 = aa + (((ab - aa) * we) >> 15);
            } else if (we < 16384) {                    /* A's route fades out .. */
                s = sa;
                d = da;
                v8 = (aa * (32768 - 2 * we)) >> 15;
            } else {                                    /* .. B's fades in */
                s = sb;
                d = db;
                v8 = (ab * (2 * we - 32768)) >> 15;
            }
            p[id] = (int16_t)s;
            p[id + 1u] = (int16_t)d;
            p[id + 2u] = (int16_t)clamp(pin ? (v8 + 128) >> 8 : p[id + 2u] + ((v8 + 128) >> 8) - base[id + 2u], -64, 63);
        }
    }
}

/* eng_fm6.c fm6_sync (audio ISR): the key of part tr's FM6 blend (p null; 0 = none), or the blend into the patch copy p */
static uint32_t morph_fm6(uint32_t tr, uint8_t *p)
{
    const morph_part_t *mp = tr < NSHOWN ? &mph[tr] : 0;
    uint32_t i, key;
    int32_t w;
    if (!mp || !mp->fnid || !morph_isfm(mp) || morph_stale(&trk[tr], mp))
        return 0;
    key = 0x80000000u | (uint32_t)mp->fser << 16 | (mp->sm_r < 32768u ? (uint32_t)(mp->sm_r >> 7) + 1u : 0u) << 7 | mp->fader;
    if (!p)
        return key;
    w = morph_wb(mp->fader);
    for (i = 0; i < NFM; i++) {
        uint32_t b = fm_byte(i);
        p[b] = (uint8_t)clamp((int32_t)p[b] + morph_fm_v(mp, i, w, 1) - (int32_t)mp->fb[i], 0, 99);
    }
    return key;
}

/* ------------------------------------------------------------- the hook (audio ISR) --- */
/* a part's window (MIXSPLIT job 2): its state is in track_t (mwon: the window is open, mwmp: its morph_part_t, mxw: the cross-fade), so morph_end finds it at a fixed
 * offset from t. The blend's work arrays are the part's own too (static: off the ISR's stack; used inside one morph_begin only) */
typedef struct {
    uint8_t tid[MV_N], chk[MV_N];                        /* morph_begin's blend */
    int16_t tlive[MV_N];
} mwork_t;
static mwork_t mwork[NLIVE];

static int morph_stale(const track_t *t, const morph_part_t *mp)   /* another sound than the morph's */
{
    return t->engine != mp->eng || t->eng_req != mp->eng || t->preset != mp->preset || t->user != mp->user;
}

/* before part t's block (adv: a block passes, the ramp moves) or note-on: the blend into t->p. 1 = applied */
static int morph_begin(track_t *t, int adv)
{
    uint32_t pi = (uint32_t)(t - trk), j, q, m = 0, n;
    morph_part_t *mp;
    mxf_t *mxp = &t->mxw;
    if (pi >= NSHOWN)
        return 0;
    mp = &mph[pi];
    t->mf = mp->fader;                                  /* (4b) the matrix's MORPH source for this block (mod.c) */
    if ((!mp->nid && !mp->fnid) || t->mwon || morph_stale(t, mp))
        return 0;
    t->mwon = 1;
    t->mwpi = (uint8_t)pi;
    if (mp->cvalid && mp->cfader == mp->fader && mp->cst == duell_stereo) {   /* as the block before: write the values */
        n = mp->cn;
        for (j = 0; j < n; j++) {
            uint32_t id = mp->cid[j];
            if (t->p[id] != mp->clive[j])
                break;                                  /* (an edit: blend again) */
            t->p[id] = mp->cval[j];
        }
        if (j == n) {
            if (mp->cxf.on)
                *mxp = mp->cxf;
            return 1;
        }
        while (j--)
            t->p[mp->cid[j]] = mp->clive[j];
    }
    {
        uint8_t *tid = mwork[pi].tid, *chk = mwork[pi].chk;
        int16_t *tlive = mwork[pi].tlive;
        for (j = 0; j < mp->nid; j++) {
            const ment_t *en = &mp->ent[j];
            uint32_t kind = en->kind & 0x3Fu;
            for (q = 0; q < (kind == MK_SLOT ? 3u : 1u); q++) {
                int16_t v = t->p[en->id + q];
                tid[m] = (uint8_t)(en->id + q);
                chk[m] = (en->kind & MK_PIN) || kind != MK_CONT || v <= en->lo || v >= en->hi;   /* (see below; a pinned value does not depend on the live one) */
                tlive[m++] = v;
            }
        }
        morph_apply(mp, t->p, morph_wb(mp->fader), 1, mxp);
        /* the cache keeps what was written, and what depends on its live value without a write now: a faded choice or
         * route (the user's once edited), a continuous value at the edge of its range (clamped to itself) */
        for (j = 0, n = 0; j < m; j++)
            if (t->p[tid[j]] != tlive[j] || chk[j]) {
                mp->cid[n] = tid[j];
                mp->clive[n] = tlive[j];
                mp->cval[n++] = t->p[tid[j]];
            }
        mp->cn = (uint8_t)n;
        mp->cxf = *mxp;
        mp->cfader = mp->fader;
        mp->cst = duell_stereo;
        mp->cvalid = mp->sm_r >= 32768u;                /* (a ramp: every block anew) */
    }
    if (adv && mp->sm_r < 32768u)
        mp->sm_r = (uint16_t)(mp->sm_r + MORPH_RAMP_STEP < 32768u ? mp->sm_r + MORPH_RAMP_STEP : 32768u);
    return 1;
}

/* after it: the stored values back (nothing else writes these in the window: mod_end puts its own back before, the
 * engines write only P_E7 of FM6, which the morph never moves) */
static void morph_end(track_t *t)
{
    const morph_part_t *mp = &mph[t->mwpi];
    uint32_t j;
    if (!t->mwon)
        return;
    for (j = 0; j < mp->cn; j++)
        t->p[mp->cid[j]] = mp->clive[j];
    t->mwon = 0;
    if (t->mxw.on)
        memset(&t->mxw, 0, sizeof t->mxw);
}

/* ------------------------------------------------------------- the display (main loop) --- */
/* The parameter pages show what the fader sounds like (the blend morph_begin writes), not the stored Original. Pure: nothing is
 * written. morph_view_on: part pi has a live morph (a list of moved values, its sound the morph's). morph_view(pi, id): the
 * value of id as the audio path has it at this fader = morph_apply's result on a copy of the stored values (a continuous
 * value: live + (blend - Original), so a knob edit of the Original shows at once; a switch: A below the fader's middle, B from
 * it; a faded choice and a matrix route: the dominant end's); the stored value for anything the morph never moves (EX, HARD,
 * never-tagged, the MORPH pages, a value the user edited away from the Original: the user's choice stays) */
static int morph_view_on(uint32_t pi)
{
    const morph_part_t *mp = pi < NSHOWN ? &mph[pi] : 0;
    return mp && mp->n[0] && mp->nid && !morph_stale(&trk[pi], mp);
}
static int32_t morph_view(uint32_t pi, uint32_t id)
{
    int16_t v[EXT_P_LEN];
    mxf_t xf;
    uint32_t j;
    if (id >= EXT_P_LEN || (id >= P_COUNT && id < XW_BASE))
        return 0;
    if (!morph_view_on(pi))
        return trk[pi].p[id];
    memcpy(v, trk[pi].p, sizeof v);
    memset(&xf, 0, sizeof xf);
    morph_apply(&mph[pi], v, morph_wb(mph[pi].fader), 1, &xf);
    for (j = 0; j < mph[pi].nid; j++) {                  /* a faded choice: the dominant end (the audio path crossfades) */
        const ment_t *en = &mph[pi].ent[j];
        if ((en->kind & 0x3Fu) == MK_FADE && en->id == id && xf.on && xf.w[en->mx] >= 16384u)
            v[id] = xf.b[en->mx];
    }
    return v[id];
}
/* the card of id marks a value the morph is moving now (shown differs from the stored Original) */
static int morph_view_moved(uint32_t pi, uint32_t id)
{
    return morph_view_on(pi) && morph_view(pi, id) != trk[pi].p[id];
}
/* FM6 patch byte b (the fm6_patch index) of part pi as the fader sounds it (the stored byte where the morph leaves it) */
static int32_t morph_view_fm(uint32_t pi, uint32_t b)
{
    const morph_part_t *mp = pi < NSHOWN ? &mph[pi] : 0;
    uint32_t i;
    int32_t s = fm6_patch[pi % NTRK][b];
    if (!mp || !mp->n[0] || !mp->fnid || !morph_isfm(mp) || morph_stale(&trk[pi], mp))
        return s;
    for (i = 0; i < NFM; i++)
        if (fm_byte(i) == b)
            return clamp(s + morph_fm_v(mp, i, morph_wb(mp->fader), 1) - (int32_t)mp->fb[i], 0, 99);
    return s;
}

/* ------------------------------------------------------------- actions (main loop) --- */
static void morph_fm_live(uint32_t pi, uint8_t *live)     /* part pi's FM6 patch bytes as stored */
{
    uint32_t i;
    for (i = 0; i < NFM; i++)
        live[i] = fm6_patch[pi % NTRK][fm_byte(i)];
}

static uint32_t morph_new_seed(void);
static void morph_roll_cfg(uint32_t pi, uint32_t x, uint32_t seed, uint32_t amount, uint32_t cfg);
static uint32_t morph_cfg_of(uint32_t pi) { return (morph.cfg & MC_SET) | (duell_stereo ? MC_STEREO : 0u) | (pi ? MC_PART : 0u); }
/* v2: a patch's saved End B (MB_N bytes in the user preset record, upreset.c): 'M', seed (16 bit), AMOUNT, MC_* bits, 0,
 * a check byte (DUELL: byte 5 = the part's AMOUNT knob + 1 at saving, 0 = not stored: older patches; a load sets the part's AMOUNT to it, else to the roll's). morph_ld: the block of the sound being loaded (set by up_load before load_end, read by morph_rebase_part) */
#define MB_N 7u
#define MB_MARK 0x4Du
static const uint8_t *morph_ld;
static const xb_edit_t *morph_ld_e;                      /* .. and the values set by hand that come with it (cloud job G; up_load, multi, project): morph_ld_n of them */
static uint32_t morph_ld_n;
static uint16_t mb_said;                                 /* the last message of the B edits (kind << 8 | id), once each until the fader leaves 127 / the sound changes */
static uint8_t morph_blk_sum(const uint8_t *b) { return (uint8_t)(b[0] ^ b[1] ^ b[2] ^ b[3] ^ b[4] ^ b[5] ^ 0x5Au); }
static int morph_blk_ok(const uint8_t *b)
{
    return b && b[0] == MB_MARK && (b[1] | b[2] << 8) && b[3] <= 100u && b[4] < 32u && b[5] <= 101u && b[6] == morph_blk_sum(b);
}

static void morph_pin_load(morph_part_t *mp, const xb_edit_t *ed, uint32_t ne);
static void morph_rebase_part(uint32_t pi)               /* ends = the Original, no history; then (v2) the fader at A and a B */
{
    morph_part_t *mp = &mph[pi];
    const track_t *t = &trk[pi];
    uint32_t id, e;
    MORPH_LOCK();
    for (id = 0; id < EXT_P_LEN; id++)
        mp->base[id] = (int8_t)clamp(t->p[id], -128, 127);
    mp->eng = t->eng_req;
    if (morph_isfm(mp))
        morph_fm_live(pi, mp->fb);
    for (e = 0; e < 2u; e++) {
        memcpy(mp->fh[e][0], mp->fb, NFM);
        memcpy(mp->h[e][0], mp->base, P_COUNT);
        memcpy(mp->hx[e][0], mp->base + XW_BASE, NX);
        mp->xv[e] = 1;
        mp->seed[e][0] = 0;
        mp->rcfg[e][0] = mp->ramt[e][0] = 0;
        mp->top[e] = 0;
        mp->n[e] = 1;
    }
    memset(mp->pin, 0, sizeof mp->pin);                  /* (EDITABLE B: a new sound has no values set by hand) */
    mp->bstay = 0;
    mb_said = 0;
    mp->eng = t->eng_req;
    mp->preset = t->preset;
    mp->user = t->user;
    mp->sm_r = 32768;
    mp->nid = 0;
    mp->fnid = 0;
    mp->fader = 0;                                       /* a loaded sound always starts at A (SPEC 3.5) */
    mp->fser++;
    MORPH_UNLOCK();
    if (morph_blk_ok(morph_ld)) {                        /* End B as saved with the patch: the same roll again */
        mp->amount = morph_ld[5] ? (uint8_t)(morph_ld[5] - 1u) : morph_ld[3];   /* the patch's own AMOUNT (the knob as saved; else the roll's) */
        morph_roll_cfg(pi, 1u, (uint32_t)(morph_ld[1] | morph_ld[2] << 8), morph_ld[3], morph_ld[4]);
    } else if (morph_ld_n) {                             /* EDITABLE B: no roll saved, but values set by hand: End B is the Original with them on top ("seed 0 plus a list"; the roll below is for a sound with neither) */
                                                         /* (nothing to roll: B = entry 0, the Original) */
    } else if (!t->user && t->preset < ds_count(t->eng_req)) {   /* DUELL: a factory sound (sounds.c): its own B, always
                                                         * the same: its seed and AMOUNT, SMART / PITCH / ARP off, part 1's
                                                         * stream (on either part); PAN only where the mode lets it (3b).
                                                         * The part's AMOUNT knob stays the player's (a setting, kept over
                                                         * power cycles): it is the next ROLL's */
        morph_roll_cfg(pi, 1u, ds_seed(t->eng_req, t->preset), ds_amount(t->eng_req, t->preset),
                       duell_stereo ? MC_STEREO : 0u);
        mp->amount = (uint8_t)ds_amount(t->eng_req, t->preset);   /* ... and the part's AMOUNT knob follows the sound's category amount */
    } else {                                             /* none saved: a fresh roll at the current AMOUNT */
        morph_roll_cfg(pi, 1u, morph_new_seed(), mp->amount, morph_cfg_of(pi));
    }
    if (morph_ld_n)                                      /* the values set by hand, on top of what the roll made (or of the Original) */
        morph_pin_load(mp, morph_ld_e, morph_ld_n);
}
static void morph_ready_first(uint32_t pi);
static void morph_rebase(const track_t *t)               /* a sound was loaded into t (ui.c, project.c) */
{
    uint32_t pi = (uint32_t)(t - trk);
    if (pi < NSHOWN && mph[pi].n[0])
        morph_rebase_part(pi);
    else if (pi < NSHOWN && (morph_ld || morph_ld_n))                    /* a part whose morph was never used: the first use would find no block (cloud job G, task 0: the saved End B was lost until a ROLL) */
        morph_ready_first(pi);
    morph_ld = 0;
    morph_ld_e = 0;
    morph_ld_n = 0;
}
static void morph_rebase_all(void)
{
    uint32_t pi;
    for (pi = 0; pi < NSHOWN; pi++)
        morph_rebase(&trk[pi]);
}

/* the part ready for an action: first use (power-on: AMOUNT's default), another sound (a load the hooks missed) */
static void morph_ready_first(uint32_t pi)
{
    morph_part_t *mp = &mph[pi];
    if (!mp->apre)
        mp->aset = mp->amount = MORPH_AMOUNT_DEF;
    mp->n[0] = 1;                                        /* (morph_roll_cfg asks for the part ready: no loop) */
    morph_rebase_part(pi);
}
static morph_part_t *morph_ready(uint32_t pi)
{
    morph_part_t *mp = &mph[pi];
    if (!mp->n[0]) {
        morph_ready_first(pi);
    } else if (trk[pi].eng_req != mp->eng || trk[pi].preset != mp->preset || trk[pi].user != mp->user) {
        morph_rebase_part(pi);          /* (the sound's identity only: morph_stale also asks t->engine, which stays the old
                                         * engine through an engine-change fade, where rebase -> roll -> ready recursed) */
    }
    return mp;
}

/* end e of part pi as it sounds with the live values (e at its extreme, no ramp) into out (EXT_P_LEN: the ids, the gap, the extension slots) */
static void morph_effective_x(uint32_t pi, uint32_t e, int16_t *out)
{
    mxf_t xf;
    memcpy(out, trk[pi].p, EXT_P_LEN * sizeof trk[pi].p[0]);
    morph_apply(&mph[pi], out, e ? 32768 : 0, 0, &xf);
}
/* .. the ids only (out: P_COUNT) */
static void morph_effective(uint32_t pi, uint32_t e, int16_t *out)
{
    int16_t v[EXT_P_LEN];
    morph_effective_x(pi, e, v);
    memcpy(out, v, P_COUNT * sizeof v[0]);
}

/* the edits made since the ends were rolled go into them: each end = what it sounds like now, the Original = the live
 * values (nothing heard changes). Called locked */
static void morph_fold(uint32_t pi)
{
    morph_part_t *mp = &mph[pi];
    int16_t v[EXT_P_LEN];
    uint32_t e, id;
    for (e = 0; e < 2u; e++) {
        int8_t *c = mp->h[e][mp->top[e]], *cx = mp->hx[e][mp->top[e] % XH];
        morph_effective_x(pi, e, v);
        for (id = 0; id < P_COUNT; id++)
            c[id] = (int8_t)clamp(v[id], -128, 127);
        for (id = 0; id < NX; id++)
            cx[id] = (int8_t)clamp(v[XW_BASE + id], -128, 127);
    }
    for (id = 0; id < EXT_P_LEN; id++)
        mp->base[id] = (int8_t)clamp(trk[pi].p[id], -128, 127);
    if (morph_isfm(mp)) {                                /* the FM6 patch bytes the same way */
        uint8_t live[NFM];
        morph_fm_live(pi, live);
        for (e = 0; e < 2u; e++) {
            uint8_t *c = mp->fh[e][mp->top[e]];
            for (id = 0; id < NFM; id++)
                c[id] = (uint8_t)clamp((int32_t)live[id] + c[id] - (int32_t)mp->fb[id], 0, 99);
        }
        memcpy(mp->fb, live, NFM);
    }
    mp->sm_r = 32768;
    morph_build(mp);
}

/* before end x of mp is replaced: a ramp when it is heard */
static void morph_ramp(morph_part_t *mp, uint32_t x)
{
    int32_t w = morph_wb(mp->fader), wx = x ? w : 32768 - w;
    if (wx <= 0) {
        mp->sm_r = 32768;
        return;
    }
    memcpy(mp->sm_old, morph_cur(mp, x), P_COUNT);
    memcpy(mp->smx_old, morph_curx(mp, x), NX);
    memcpy(mp->fsm, mp->fh[x][mp->top[x]], NFM);
    mp->sm_end = (uint8_t)x;
    mp->sm_r = 0;
}

/* ROLL end x (0 A, 1 B) of part pi: seed 1..65535, AMOUNT, cfg (MC_*: the settings the roll is made with, STEREO mode, the part
 * whose stream it is). The same Original, seed, AMOUNT and cfg give the same end (the FM6 bytes too) */
static void morph_end_pure(morph_part_t *mp, int8_t *end, uint8_t *fend, uint32_t pi, uint32_t seed, uint32_t amount, uint32_t cfg)
{
    uint32_t part = cfg & MC_PART ? 1u : 0u;
    uint8_t sv = mrc;
    mrc = (uint8_t)(cfg & (MC_SET | MC_STEREO));
    morph_roll_full(mp->base, end, ENGINES[eng_idx(mp->eng)], amount, seed, part, (cfg & MC_STEREO) != 0u);
    memcpy(fend, mp->fb, NFM);
    if (morph_isfm(mp))
        morph_fm_roll(mp->fb, morph_fm_car(pi, mp->base), fend, amount, seed, part);
    mrc = sv;
}
static void morph_roll_cfg(uint32_t pi, uint32_t x, uint32_t seed, uint32_t amount, uint32_t cfg)
{
    morph_part_t *mp;
    int8_t end[EXT_P_LEN];
    uint8_t fend[NFM];
    uint32_t k;
    if (pi >= NSHOWN)
        return;
    mp = morph_ready(pi);
    x &= 1u;
    amount = amount > 100u ? 100u : amount;
    MORPH_LOCK();
    morph_fold(pi);
    MORPH_UNLOCK();
    morph_end_pure(mp, end, fend, pi, seed, amount, cfg);
    MORPH_LOCK();
    morph_ramp(mp, x);
    k = (mp->top[x] + 1u) % MORPH_HIST;
    memcpy(mp->h[x][k], end, P_COUNT);
    memcpy(mp->hx[x][k % XH], end + XW_BASE, NX);
    mp->xv[x] = (uint8_t)(mp->xv[x] < XH ? mp->xv[x] + 1u : XH);
    memcpy(mp->fh[x][k], fend, NFM);
    mp->seed[x][k] = (uint16_t)seed;
    mp->rcfg[x][k] = (uint8_t)cfg;
    mp->ramt[x][k] = (uint8_t)amount;
    mp->top[x] = (uint8_t)k;
    if (mp->n[x] < MORPH_HIST)
        mp->n[x]++;
    if (x) {                                             /* EDITABLE B: a new B has nothing set by hand (UNDO brings the earlier entry's back) */
        memset(mp->pin[k % EDITB_PINH], 0, PIN_BYTES);
        mp->bstay = 0;
    }
    morph_build(mp);
    MORPH_UNLOCK();
}
static void morph_roll_seed(uint32_t pi, uint32_t x, uint32_t seed)
{
    morph_roll_cfg(pi, x, seed, pi < NSHOWN ? morph_ready(pi)->amount : 0u, morph_cfg_of(pi));
}

/* ------------------------------------------------------------- EDITABLE B (cloud job G) --- */
static uint8_t *morph_pins(morph_part_t *mp) { return mp->pin[mp->top[1] % EDITB_PINH]; }   /* the marks of End B's current entry */
static uint32_t pin_get(const uint8_t *pn, uint32_t wid) { return pn[wid >> 3] >> (wid & 7u) & 1u; }
static uint32_t pin_count(const uint8_t *pn)
{
    uint32_t i, n = 0;
    for (i = 0; i < PIN_BYTES; i++)
        n += (uint32_t)__builtin_popcount(pn[i]);
    return n;
}
static int8_t *morph_bval(morph_part_t *mp, uint32_t wid)   /* End B's current value of wide id wid (0..126, 128 ..) */
{
    return wid < P_COUNT ? &mp->h[1][mp->top[1]][wid] : &mp->hx[1][mp->top[1] % XH][wid - XW_BASE];
}

/* Whether the morph can move wide id wid at all, i.e. End B may differ from A in it (EDITABLE_B_DESIGN 3.2): every continuous value, faded choice and matrix AMOUNT, plus the switches with a planned cross-fade
 * (TWIN TYPE, ROUTE, LINK, SUB OCTAVE / WAVE, CYCLE MODE, WAVE's STEPS). The tags are the roll's (morph_tag) read as morph_build reads them: whatever an end may hold is editable, and the player's PITCH / ARP /
 * stereo settings decide the pitch and arp items (strict; a loaded list is read with every setting permitted, lenient). The rest (engine, voice mode, arp on / off, insert TYPE, WAVE TABLE, PHYS MODEL, PATCH LEVEL,
 * CYCLE SYNC, matrix SRC / DST, ..) is shared by both ends. FM6's operator bytes are not in this version (they are no ids; the FM6 patch is a separate array) */
static int morph_editable(const morph_part_t *mp, uint32_t wid, int strict)
{
    const engine_t *e = ENGINES[eng_idx(mp->eng)];
    uint8_t sv = mrc;
    uint32_t k;
    mtag_t g;
    if (wid >= EXT_P_LEN || wid == P_COUNT || (wid > P_COUNT && wid < XW_BASE) || (wid >= XW_BASE + NX))
        return 0;
    if (wid == P_TWTYPE || wid == XW_BASE + X_ROUTE || wid == XW_BASE + X_LINK || wid == XW_BASE + X_SUBOCT || wid == XW_BASE + X_SUBWAVE ||
        wid == XW_BASE + X_CYMODE || (e == &ENG_WAVE && wid == P_E6))
        return 1;
    mrc = (uint8_t)(MC_LIST | (strict ? morph.cfg & (MC_ARP | MC_PIT) : MC_ARP | MC_PIT));
    g = morph_tag(e, mp->base, wid, strict ? duell_stereo : 1u);
    mrc = sv;
    k = g.kind;
    if (g.flags & MF_GONE)
        return 0;
    if (k == MK_SLOT)
        return (wid - P_M1SRC) % 3u == 2u;
    return k == MK_CONT || k == MK_FADE;
}

/* The knob of the UI at sound value wid of part pi: 0 = an ordinary edit (the Original); 1 = End B's (the fader is at the B end and the morph is live: an End B that differs from A); 2 = a value
 * both ends share: an ordinary edit that moves A and B together (the message line says so) */
static uint32_t morph_b_mode(uint32_t pi, uint32_t wid)
{
    const morph_part_t *mp = pi < NSHOWN ? &mph[pi] : 0;
    if (!mp || !mp->n[0] || mp->fader != 127u || (!mp->nid && !mp->fnid) || morph_stale(&trk[pi], mp) || wid >= EXT_P_LEN || wid == P_COUNT)
        return 0;
    return morph_editable(mp, wid, 1) ? 1u : 2u;
}
static uint32_t morph_b_say(uint32_t kind, uint32_t wid)   /* 1 = say it now (not said for this value yet) */
{
    uint16_t k = (uint16_t)(kind << 8 | (kind == 3u ? 0u : wid));
    if (k == mb_said)
        return 0;
    mb_said = k;
    return 1;
}

/* a stay at the B end begins: End B's current entry is copied into a new one (the edits of the stay go into it; UNDO takes them all back in one step). Called locked */
static void morph_b_stay(morph_part_t *mp)
{
    uint32_t t = mp->top[1], k = (t + 1u) % MORPH_HIST;
    memcpy(mp->h[1][k], mp->h[1][t], P_COUNT);
    if (k % XH != t % XH)                                /* (the device's libc has no memmove; the two rows are the same row only at XH 1) */
        memcpy(mp->hx[1][k % XH], mp->hx[1][t % XH], NX);
    memcpy(mp->fh[1][k], mp->fh[1][t], NFM);
    mp->seed[1][k] = mp->seed[1][t];
    mp->rcfg[1][k] = mp->rcfg[1][t];
    mp->ramt[1][k] = mp->ramt[1][t];
    if (k % EDITB_PINH != t % EDITB_PINH)
        memcpy(mp->pin[k % EDITB_PINH], mp->pin[t % EDITB_PINH], PIN_BYTES);
    mp->xv[1] = (uint8_t)(mp->xv[1] < XH ? mp->xv[1] + 1u : XH);
    mp->top[1] = (uint8_t)k;
    if (mp->n[1] < MORPH_HIST)
        mp->n[1]++;
    mp->bstay = 1;
}
/* End B's value of wide id wid := v, set by hand. 0 = done; 1 = the list is full (XB_MAX values, the id not one of them: nothing changed). The caller checked morph_b_mode == 1 */
static int morph_b_set(uint32_t pi, uint32_t wid, int32_t v)
{
    morph_part_t *mp = &mph[pi];
    const param_desc_t *d = morph_desc(ENGINES[eng_idx(mp->eng)], wid);
    if (!pin_get(morph_pins(mp), wid) && pin_count(morph_pins(mp)) >= XB_MAX)
        return 1;
    MORPH_LOCK();
    if (!mp->bstay)
        morph_b_stay(mp);
    *morph_bval(mp, wid) = (int8_t)clamp(v, d->min, d->max);
    morph_pins(mp)[wid >> 3] |= (uint8_t)(1u << (wid & 7u));
    morph_build(mp);
    MORPH_UNLOCK();
    return 0;
}
/* the values set by hand ed[0..ne) (a loaded list; ids this part cannot move are left out) onto End B's current entry */
static void morph_pin_load(morph_part_t *mp, const xb_edit_t *ed, uint32_t ne)
{
    uint32_t i, n = 0;
    uint8_t *pn = morph_pins(mp);
    MORPH_LOCK();
    for (i = 0; i < ne && n < XB_MAX; i++) {
        uint32_t wid = ed[i].wid;
        const param_desc_t *d;
        if (!morph_editable(mp, wid, 0) || pin_get(pn, wid))
            continue;
        d = morph_desc(ENGINES[eng_idx(mp->eng)], wid);
        *morph_bval(mp, wid) = (int8_t)clamp(ed[i].val, d->min, d->max);
        pn[wid >> 3] |= (uint8_t)(1u << (wid & 7u));
        n++;
    }
    morph_build(mp);
    MORPH_UNLOCK();
}
/* part pi's values set by hand, ascending by wide id, into out (XB_MAX): their number */
static uint32_t morph_edits(uint32_t pi, xb_edit_t *out)
{
    morph_part_t *mp;
    const uint8_t *pn;
    uint32_t wid, n = 0;
    if (pi >= NSHOWN || !mph[pi].n[0])
        return 0;
    mp = &mph[pi];
    pn = morph_pins(mp);
    for (wid = 0; wid < EXT_P_LEN && n < XB_MAX; wid++)
        if (pin_get(pn, wid)) {
            out[n].wid = (uint8_t)wid;
            out[n].val = *morph_bval(mp, wid);
            n++;
        }
    return n;
}
/* what the autosave hashes of the morph: End B's roll and AMOUNT, and the values set by hand */
static uint32_t morph_sig(void)
{
    uint32_t pi, wid, h = 2166136261u;
    for (pi = 0; pi < NSHOWN; pi++) {
        morph_part_t *mp = &mph[pi];
        uint32_t e = mp->top[1], v;
        v = mp->n[0] ? (uint32_t)mp->seed[1][e] | (uint32_t)mp->rcfg[1][e] << 16 | (uint32_t)mp->ramt[1][e] << 24 : 0u;
        h = (h ^ v) * 16777619u;
        h = (h ^ (mp->n[0] ? mp->amount : 0u)) * 16777619u;
        if (!mp->n[0])
            continue;
        for (wid = 0; wid < EXT_P_LEN; wid++)
            if (pin_get(morph_pins(mp), wid))
                h = (h ^ (wid | (uint32_t)(uint8_t)*morph_bval(mp, wid) << 8)) * 16777619u;
    }
    return h;
}

/* part pi's End B as the record block, as it is now (no re-roll: the autosave takes it every few minutes; a patch's SAVE re-makes B first, morph_blk_make). 0 = no block (End B is no roll) */
static int morph_blk_peek(uint32_t pi, uint8_t *b)
{
    const morph_part_t *mp;
    uint32_t e;
    if (pi >= NSHOWN || !mph[pi].n[0])
        return 0;
    mp = &mph[pi];
    e = mp->top[1];
    if (!mp->seed[1][e])
        return 0;
    b[0] = MB_MARK;
    b[1] = (uint8_t)mp->seed[1][e];
    b[2] = (uint8_t)(mp->seed[1][e] >> 8);
    b[3] = mp->ramt[1][e];
    b[4] = mp->rcfg[1][e];
    b[5] = (uint8_t)(mp->amount + 1u);
    b[6] = morph_blk_sum(b);
    return 1;
}

/* v2: part pi's End B as a record block (MB_N bytes) for SAVE held (the patch saved). It is re-rolled first when it is not what its seed
 * regenerates from the Original as it is now (edits since the roll folded into it): then what is heard is what a load will
 * give. 0 = no block (End B is no roll: UNDO went back to the Original) */
static int morph_blk_make(uint32_t pi, uint8_t *b)
{
    morph_part_t *mp = morph_ready(pi);
    uint32_t seed = mp->seed[1][mp->top[1]], cfg = mp->rcfg[1][mp->top[1]], amt = mp->ramt[1][mp->top[1]];
    int8_t end[EXT_P_LEN];
    uint8_t fend[NFM];
    if (!seed)
        return 0;
    MORPH_LOCK();
    morph_fold(pi);
    MORPH_UNLOCK();
    morph_end_pure(mp, end, fend, pi, seed, amt, cfg);
    {   /* EDITABLE B: the roll again from the Original as it is, the values set by hand put back on top (a save keeps them; the rolled rest follows the Original) */
        uint8_t pn[PIN_BYTES];
        uint32_t wid, any = 0;
        memcpy(pn, morph_pins(mp), PIN_BYTES);
        for (wid = 0; wid < EXT_P_LEN; wid++)
            if (pin_get(pn, wid)) {
                end[wid] = *morph_bval(mp, wid);
                any = 1;
            }
        if (memcmp(end, morph_cur(mp, 1), P_COUNT) || memcmp(end + XW_BASE, morph_curx(mp, 1), NX) || (morph_isfm(mp) && memcmp(fend, mp->fh[1][mp->top[1]], NFM))) {
            morph_roll_cfg(pi, 1u, seed, amt, cfg);
            if (any) {
                MORPH_LOCK();
                memcpy(morph_pins(mp), pn, PIN_BYTES);
                for (wid = 0; wid < EXT_P_LEN; wid++)
                    if (pin_get(pn, wid))
                        *morph_bval(mp, wid) = end[wid];
                morph_build(mp);
                MORPH_UNLOCK();
            }
        }
    }
    b[0] = MB_MARK;
    b[1] = (uint8_t)seed;
    b[2] = (uint8_t)(seed >> 8);
    b[3] = (uint8_t)amt;
    b[4] = (uint8_t)cfg;
    b[5] = (uint8_t)(mp->amount + 1u);                   /* the AMOUNT knob as it is now (End B itself needs the roll's, b[3]) */
    b[6] = morph_blk_sum(b);
    return 1;
}

/* a MULTI loaded (project.c multi_load), after the parts' sounds: part pi's AMOUNT, and End B as the multi stored it (blk: an
 * MB_N block as morph_blk_make writes; none or damaged: a fresh roll at that AMOUNT), the fader at A: the rule of a patch load */
static void morph_load_part(uint32_t pi, const uint8_t *blk, uint32_t amount, const xb_edit_t *ed, uint32_t ne)
{
    morph_part_t *mp;
    if (pi >= NSHOWN)
        return;
    mp = &mph[pi];
    amount = amount > 100u ? 100u : amount;
    mp->amount = (uint8_t)amount;
    mp->apre = 1;
    if (!mp->n[0])
        mp->n[0] = 1;                                    /* (ready: morph_roll_cfg asks for the part, no first-use rebase) */
    morph_ld = morph_blk_ok(blk) ? blk : 0;
    morph_ld_e = ed;
    morph_ld_n = ne;
    morph_rebase_part(pi);
    morph_ld = 0;
    morph_ld_e = 0;
    morph_ld_n = 0;
    mp->amount = (uint8_t)amount;
}

/* the parts the controls act on: SELECTED the selected one, BOTH A and B */
static uint32_t morph_parts(uint32_t *first)
{
    if (morph.target == MT_BOTH) {
        *first = 0;
        return NSHOWN;
    }
    *first = song.sel < NSHOWN ? song.sel : 0u;
    return *first + 1u;
}
static uint32_t morph_new_seed(void)
{
    uint32_t s;
    morph.gen = morph.gen * 1664525u + 1013904223u + fm1_ms;
    s = (morph.gen >> 16) & 0xFFFFu;
    return s ? s : 1u;
}

/* ROLL A (0) / ROLL B (1): returns the seed (shown) */
static uint32_t morph_roll(uint32_t x)
{
    uint32_t pi, end = morph_parts(&pi), seed = morph_new_seed();
    for (; pi < end; pi++)
        morph_roll_seed(pi, x, seed);
    morph.seed = (uint16_t)seed;
    return seed;
}

/* UNDO on part pi: end x's (0 A, 1 B) previous roll. 0 = nothing to go back to */
static int morph_undo_part(uint32_t pi, uint32_t x)
{
    morph_part_t *mp;
    if (pi >= NSHOWN)
        return 0;
    x &= 1u;
    mp = morph_ready(pi);
    if (mp->n[x] <= 1u)
        return 0;
    MORPH_LOCK();
    morph_ramp(mp, x);
    mp->top[x] = (uint8_t)((mp->top[x] + MORPH_HIST - 1u) % MORPH_HIST);
    mp->n[x]--;
    if (x) {
        mp->bstay = 0;
        if (EDITB_PINH == 1u)                            /* (the lean setting keeps one entry's marks: the earlier entries' are not known) */
            memset(mp->pin, 0, sizeof mp->pin);
    }
    if (mp->xv[x] > 1u)
        mp->xv[x]--;
    else
        memcpy(mp->hx[x][mp->top[x] % XH], mp->base + XW_BASE, NX);   /* (XH under 9: the roll's slots are gone from the ring: at the Original) */
    morph_build(mp);
    MORPH_UNLOCK();
    return 1;
}

static int morph_undo(uint32_t x)
{
    uint32_t pi, end = morph_parts(&pi);
    int r = 0;
    for (; pi < end; pi++)
        r |= morph_undo_part(pi, x);
    return r;
}
static void morph_set_fader(uint32_t f)                  /* the MORPH fader, 0..127 (CC 20 later) */
{
    uint32_t pi, end = morph_parts(&pi);
    for (; pi < end; pi++) {
        morph_part_t *mp = morph_ready(pi);
        mp->fader = (uint8_t)(f > 127u ? 127u : f);
        if (mp->fader != 127u) {
            mp->bstay = 0;                               /* (EDITABLE B: the next arrival at the B end starts a new stay) */
            mb_said = 0;
        }
    }
}
static void morph_set_amount(uint32_t a)                 /* AMOUNT 0..100 % */
{
    uint32_t pi, end = morph_parts(&pi);
    for (; pi < end; pi++)
        morph_ready(pi)->aset = morph_ready(pi)->amount = (uint8_t)(a > 100u ? 100u : a);
}
static void morph_set_target(uint32_t both)
{
    morph.target = both ? MT_BOTH : MT_SELECTED;
}
static void morph_set_cfg(uint32_t bit, uint32_t on)      /* v2: SMART (on = bit MC_WILD clear), ARP, PITCH */
{
    morph.cfg = (uint8_t)(on ? morph.cfg | bit : morph.cfg & ~bit);
}

/* ------------------------------------------------- the page, MIDI, the matrix (4b) --- */
/* The MORPH page (ui_input.c morph_knob), MIDI (midi_control.c midi_morph) and the matrix's MORPH source act through
 * these (the matrix reads the fader as t->mf, set by morph_begin before the part's block). A control acts on the parts of a mask: SELECTED the one part (the selected one from the page, the channel's
 * part from MIDI), BOTH every shown part (morph_mask). MIDI arrives in the audio ISR (events_block): a roll is too
 * heavy for it and the actions take the lock, so the ISR only queues (morph_push) and the main loop runs the queue in
 * order (morph_pop, ui_input.c morph_poll). */
enum { MQ_FADER, MQ_AMOUNT, MQ_TARGET, MQ_ROLL, MQ_UNDO, MQ_SMART, MQ_ARP, MQ_PITCH };   /* (v2: no KEEP) */
#define MQ_N 16u
static uint32_t mq_buf[MQ_N];                            /* kind | part << 8 | value << 16 */
static volatile uint8_t mq_w, mq_r;                      /* (the ISR writes mq_w, the main loop mq_r) */
static volatile uint8_t mq_lost;                         /* actions dropped (the queue full) */

static uint32_t morph_mask(uint32_t pi)                  /* the parts a control of part pi acts on */
{
    return morph.target == MT_BOTH ? (1u << NSHOWN) - 1u : 1u << (pi < NSHOWN ? pi : 0u);
}
/* what the page shows of part pi without touching it (a part never used: AMOUNT's default, ends at the Original) */
static uint32_t morph_fader_of(uint32_t pi) { return pi < NSHOWN ? mph[pi].fader : 0u; }
static uint32_t morph_amount_of(uint32_t pi) { return pi < NSHOWN && mph[pi].n[0] ? mph[pi].amount : MORPH_AMOUNT_DEF; }
static uint32_t morph_seed_of(uint32_t pi, uint32_t e)  /* end e's seed, 0: no roll (the Original) */
{
    const morph_part_t *mp = &mph[pi < NSHOWN ? pi : 0u];
    return mp->n[0] ? mp->seed[e & 1u][mp->top[e & 1u]] : 0u;
}
static uint32_t morph_hist_of(uint32_t pi, uint32_t e)  /* end e's rolls UNDO can go back over */
{
    const morph_part_t *mp = &mph[pi < NSHOWN ? pi : 0u];
    return mp->n[e & 1u] > 1u ? mp->n[e & 1u] - 1u : 0u;
}

/* an action on the parts of mask, main loop. MQ_ROLL / MQ_UNDO v: the end (0 A, 1 B); returns the seed (UNDO: 1 = some part
 * went back). MQ_FADER 0..127, MQ_AMOUNT 0..100 %, MQ_TARGET 0 PART 1 / 1 PART 2 / 2 BOTH (ui_input.c morph_act picks the part),
 * MQ_SMART / MQ_ARP / MQ_PITCH 0 / 1 */
static uint32_t morph_do(uint32_t kind, uint32_t mask, uint32_t v)
{
    uint32_t pi, r = 0, seed = 0;
    if (kind == MQ_TARGET) {                             /* v2: 0 PART 1, 1 PART 2, 2 BOTH */
        morph_set_target(v >= 2u);
        return 0;
    }
    if (kind >= MQ_SMART) {
        morph_set_cfg(kind == MQ_SMART ? MC_WILD : kind == MQ_ARP ? MC_ARP : MC_PIT, kind == MQ_SMART ? !v : v);
        return 0;
    }
    if (kind == MQ_ROLL)
        seed = morph_new_seed();
    for (pi = 0; pi < NSHOWN; pi++) {
        if (!((mask >> pi) & 1u))
            continue;
        switch (kind) {
        case MQ_FADER:
            morph_ready(pi)->fader = (uint8_t)(v > 127u ? 127u : v);
            if (mph[pi].fader != 127u) {
                mph[pi].bstay = 0;                       /* (EDITABLE B: the next arrival at the B end starts a new stay) */
                mb_said = 0;
            }
            break;
        case MQ_AMOUNT: morph_ready(pi)->aset = morph_ready(pi)->amount = (uint8_t)(v > 100u ? 100u : v); break;
        case MQ_ROLL: morph_roll_seed(pi, v & 1u, seed); break;
        default: r |= (uint32_t)morph_undo_part(pi, v); break;   /* MQ_UNDO */
        }
    }
    if (kind == MQ_ROLL) {
        morph.seed = (uint16_t)seed;
        return seed;
    }
    return r;
}

/* audio ISR (MIDI): queue an action of part pi (its channel's). A fader or AMOUNT right after the same one of the same
 * part still waiting replaces its value (a CC stream takes one slot) */
static void morph_push(uint32_t kind, uint32_t pi, uint32_t v)
{
    uint32_t w = mq_w, e = kind | pi << 8 | v << 16;
    if (w != mq_r && (kind == MQ_FADER || kind == MQ_AMOUNT) && (mq_buf[(w - 1u) % MQ_N] & 0xFFFFu) == (kind | pi << 8)) {
        mq_buf[(w - 1u) % MQ_N] = e;                     /* (the main loop takes the queue with the ISR off) */
        return;
    }
    if ((uint8_t)(w - mq_r) >= MQ_N) {
        mq_lost = 1;
        return;
    }
    mq_buf[w % MQ_N] = e;
    mq_w = (uint8_t)(w + 1u);
}
/* main loop: the queued actions into q (at most MQ_N), in order; their number */
static uint32_t morph_pop(uint32_t *q)
{
    uint32_t n = 0;
    MORPH_LOCK();
    while (mq_r != mq_w && n < MQ_N) {
        q[n++] = mq_buf[mq_r % MQ_N];
        mq_r = (uint8_t)(mq_r + 1u);
    }
    MORPH_UNLOCK();
    return n;
}

/* ------------------------------------------------------------- persistence block --- */
#define MORPH_PACK_PART (8u + 2u * P_COUNT)
#define MORPH_PACK_SIZE (4u + NSHOWN * MORPH_PACK_PART)
/* the morph as a project would keep it: 'M', version 1, TARGET, parts; per part FADER, AMOUNT, seed A, seed B (16 bit),
 * 2 spare, end A, end B (absolute, P_COUNT bytes each, P_* order). Returns its size */
static uint32_t morph_pack(uint8_t *b)
{
    uint32_t pi, e;
    b[0] = 'M';
    b[1] = 1;
    b[2] = morph.target;
    b[3] = NSHOWN;
    for (pi = 0; pi < NSHOWN; pi++) {
        morph_part_t *mp = morph_ready(pi);
        uint8_t *q = b + 4u + pi * MORPH_PACK_PART;
        q[0] = mp->fader;
        q[1] = mp->amount;
        for (e = 0; e < 2u; e++) {
            q[2u + 2u * e] = (uint8_t)mp->seed[e][mp->top[e]];
            q[3u + 2u * e] = (uint8_t)(mp->seed[e][mp->top[e]] >> 8);
            memcpy(q + 8u + e * P_COUNT, morph_cur(mp, e), P_COUNT);
        }
        q[6] = q[7] = 0;
    }
    return MORPH_PACK_SIZE;
}
/* after the parts' sounds are loaded: the Original = each part's values, the ends as stored. 0 = not a morph block */
static int morph_unpack(const uint8_t *b, uint32_t n)
{
    uint32_t pi, e, np;
    if (n < 4u || b[0] != 'M' || b[1] != 1u)
        return 0;
    np = b[3] < NSHOWN ? b[3] : NSHOWN;
    if (n < 4u + np * MORPH_PACK_PART)
        return 0;
    /* b[2] TARGET: reserved (0.2.9: ALGORITHM's BOTH is not saved; morph_pack still writes it) */
    for (pi = 0; pi < np; pi++) {
        morph_part_t *mp = morph_ready(pi);
        const uint8_t *q = b + 4u + pi * MORPH_PACK_PART;
        morph_rebase_part(pi);
        MORPH_LOCK();
        mp->fader = q[0] > 127u ? 127u : q[0];
        mp->amount = q[1] > 100u ? 100u : q[1];
        for (e = 0; e < 2u; e++) {
            mp->top[e] = 0;                              /* (v2: the rebase rolled a B: back to one entry) */
            mp->n[e] = 1;
            mp->xv[e] = 1;
            memcpy(mp->hx[e][0], mp->base + XW_BASE, NX);   /* (the pack holds the ids: the slots at the Original) */
            mp->rcfg[e][0] = 0;
            mp->ramt[e][0] = mp->amount;
            mp->seed[e][0] = (uint16_t)(q[2u + 2u * e] | q[3u + 2u * e] << 8);
            memcpy(mp->h[e][0], q + 8u + e * P_COUNT, P_COUNT);
            if (morph_isfm(mp) && mp->seed[e][0])        /* (the FM6 bytes are not stored: the seed regenerates them) */
                morph_fm_roll(mp->fb, morph_fm_car(pi, mp->base), mp->fh[e][0], mp->amount, mp->seed[e][0], pi);
        }
        morph_build(mp);
        MORPH_UNLOCK();
    }
    return 1;
}
