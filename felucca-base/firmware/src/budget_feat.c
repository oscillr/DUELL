/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* NOTES budget: the new features' items (budget_feat.h). Three sections, one for each job; add to r->eng (a voice's cost), r->ext (a voice's extras) or r->misc (the part's fixed cost) */
/* SUB (job F): host instructions a sample a voice (the unit of the other rows). est.: the figures of SUB_AND_CYCLE_DESIGN A3, calibrated on WAVE's own SINE sub (+22, measured, SPEC 2.6): this
 * cloud job had no host instruction counter (the one of tests/uf_test.c is Apple's), so they are NOT measured here; the owner's Mac bench replaces them */
#define BD_SUB_PLS 72u          /* SQUARE and PULSE (polyBLEP at the edges): COST-CAL item 116, SQ 71.9 (GOOD, 8 runs), PULSE 67.4 */
#define BD_SUB_SINE 51u         /* COST-CAL item 116: measured 50.9 (GOOD) */
#define BD_SUB_TRI 46u          /* COST-CAL item 116: measured 46.0 (GOOD) */
#define BD_SUB_H1_S 8u          /* est.: HARMONICS up to 64 (one octave above), SINE / TRI: +8 .. 18; SQUARE / PULSE: +18 .. 36 */
#define BD_SUB_H2_S 18u         /* .. above 64 (two octaves) */
#define BD_SUB_H1_Q 18u
#define BD_SUB_H2_Q 36u
#define BD_SUB_BLK 1u           /* the per-block work (the step, the unison rule, the FM6 follow): under 1 */
/* CYCLE (job F): est.: 15 to 30 host instructions a block and voice while an active slot reads it (the tick, the curve, one multiply-add) = 0.5 to 0.9 a sample, rounded up */
#define BD_CYCLE 6u              /* COST-CAL item 116: a slot reading CYCLE 5.7 a voice (GOOD) */
/* WAVE STEPS (job F): est. +0.3 a sample and voice (5 to 10 instructions a block): 2 for the part's 8 voices (the table's unit is the whole instruction) */
#define BD_STEPS 2u
/* TWIN (job E) cost rows, host instructions a sample and voice, in the Mac bench's units. Measured with uf_test's TWIN rows (valgrind callgrind instruction counts, the Linux host has no
 * counter: worst of PHASE and ANALOG, 8 voices, filter 1 running and bypassed), times the Mac / Linux ratio of filter 1 12 dB (46.6 / 69.0 = 0.675), plus 10 %, rounded up: (lin ratio).
 * The design table (30..36, 45..55, 16..22, 12..16, +2..4, 0.2..0.3 points) was an estimate and is about half of what was measured */
/* DUELL 1.1 item 90: re-measured with tests/poly_calib.c (the Mac's instruction counter, the dearest paired difference over every factory sound of every engine): filter 2 after a
 * running MULTI (whose BD_F12 already holds the chain path), and the extra of the chain when filter 1 is bypassed. DRIVE + PARALLEL + ENV 2 together measured 54 .. 66: split by the old ratios */
#define BD_TW_CHAIN 0u                  /* the chain path with filter 1 running: inside BD_F12 now */
#define BD_TW_CHAIN_BYP 36u             /* .. with filter 1 bypassed (TWIN LP alone 128 over the bypass, less the LP's 86) */
#define BD_TW_SVF 61u                   /* LP, BP, HP, NOTCH (TRIO's filter) */
#define BD_TW_LADDER 55u                /* LADDER */
#define BD_TW_COMB 70u                  /* COMB */
#define BD_TW_DRIVE 18u                 /* DRIVE above 0 */
#define BD_TW_PAR 6u                    /* ROUTE PARALLEL with its BLEND */
#define BD_TW_MOD 5u                    /* filter 2's modulation (KEY TRK 2, ENV 2, LFO 1, LFO 2, a matrix CUTOFF 2) */
/* bd_tw_t (budget_feat.h): the dearest TYPE's cost, and whether DRIVE, PARALLEL, the modulation run in any of the states */
static void bd_tw_add(bd_tw_t *w, const engine_t *e, int ty, int drv, const int16_t *x, int mat)   /* x: the extension values (the live ones, or a MORPH end's as it sounds) */
{
    uint32_t c;
    if (ty <= (int)TT_OFF || ty > (int)TT_COMB)
        return;                                         /* OFF, or out of range (reads OFF) */
    if (ty == (int)TT_COMB && (e == ENGINES[ENGI_WAVE] || e == ENGINES[ENGI_PHYS]))
        ty = TT_LP;                                     /* (chain_f2.c f2_ty: a stored COMB there runs as LP) */
    c = ty == (int)TT_COMB ? BD_TW_COMB : ty == (int)TT_LADDER ? BD_TW_LADDER : BD_TW_SVF;
    w->ty = c > w->ty ? c : w->ty;
    w->drive |= drv != 0;
    w->par |= x[X_ROUTE] != 0;
    w->mod |= x[X_KTRK2] != 0 || x[X_ENV2] != 0 || x[X_LFO1] != 0 || x[X_LFO2] != 0 || mat;
}
/* the TWIN of one set of values q (EXT_P_LEN: the live values, or a MORPH end as it sounds, budget.c bd_end): its TYPE, DRIVE, extension values and its own matrix slots to CUTOFF 2 */
static void bd_tw_of(bd_tw_t *w, const engine_t *e, const int16_t *q)
{
    uint32_t i;
    int mat = 0;
    for (i = 0; i < NMSLOT; i++) {
        const int16_t *s = &q[MSLOT_ID(i)];
        mat |= s[0] && s[1] == MD_CUT2 && s[2];
    }
    bd_tw_add(w, e, q[P_TWTYPE], q[P_DRV2], &q[XW_BASE], mat);
}
/* wm: the TWIN of the part's MORPH ends as they sound (budget.c bd_part, item 142; all 0 without a morph) */
static void bd_feat(const track_t *t, bd_part_t *r, const bd_tw_t *wm)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    const int16_t *p = t->p;
    uint32_t i;
    bd_tw_t w = *wm;

    /* TWIN (job E): filter 2 by TYPE and ROUTE, DRIVE, the cutoff sum; a voice in the chain. The live values, and with a MORPH on the part both ends' as they sound (the sound moves between them) */
    bd_tw_of(&w, e, p);
    if (w.ty)
        r->ext += (r->filt ? BD_TW_CHAIN : BD_TW_CHAIN_BYP) + w.ty + (w.drive ? BD_TW_DRIVE : 0) + (w.par ? BD_TW_PAR : 0) + (w.mod ? BD_TW_MOD : 0);
    /* (CHAIN_ALL's chain path for a note whose filter 1 runs: measured inside BD_F12 since item 90) */

    /* SUB (job F): the SUB LAYER by WAVE and HARMONICS, a voice; UNISON plays one sub a key (voice 0 only): the part's fixed cost */
    if (XV(t, X_SUBLVL) > 0) {
        uint32_t sw = (uint32_t)XV(t, X_SUBWAVE) & 3u, h = 0u /* SUB HARMONICS disabled (owner, 2026-10-21) */, q = sw >= 2u, c = sw == 0u ? BD_SUB_SINE : sw == 1u ? BD_SUB_TRI : BD_SUB_PLS;
        if (h)
            c += h > 64u ? (q ? BD_SUB_H2_Q : BD_SUB_H2_S) : (q ? BD_SUB_H1_Q : BD_SUB_H1_S);
        c += BD_SUB_BLK;
        if (p[P_VOICE] == V_UNISON)
            r->misc += c;
        else
            r->ext += c;
    }

    /* CYCLE (job F): the CYCLE envelope while an active slot reads it (t->cy) */
    for (i = 0; i < NMSLOT; i++) {
        const int16_t *s = &p[MSLOT_ID(i)];
        if (s[0] == MS_CYCLE && s[1] && s[2]) {
            r->ext += BD_CYCLE;
            break;
        }
    }

    /* WAVE STEPS (job F): the rounding of the scan place, a voice, on WAVE with STEPS above OFF */
    if (ENGINES[eng_idx(t->eng_req)] == &ENG_WAVE && p[P_E6] > 0)
        r->misc += BD_STEPS;
}
