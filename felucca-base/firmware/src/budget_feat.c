/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* NOTES budget: the new features' items (budget_feat.h). Three sections, one for each job; add to r->eng (a voice's cost), r->ext (a voice's extras) or r->misc (the part's fixed cost) */
/* SUB (job F): host instructions a sample a voice (the unit of the other rows). est.: the figures of SUB_AND_CYCLE_DESIGN A3, calibrated on WAVE's own SINE sub (+22, measured, SPEC 2.6): this
 * cloud job had no host instruction counter (the one of tests/uf_test.c is Apple's), so they are NOT measured here; the owner's Mac bench replaces them */
#define BD_SUB_SINE 22u         /* est.: a SINE sub (WAVE's own SUB, measured +22) */
#define BD_SUB_TRI 19u          /* est.: +18 to 20 */
#define BD_SUB_PLS 32u          /* est.: SQUARE and PULSE (polyBLEP at the edges), +30 to 34 */
#define BD_SUB_H1_S 8u          /* est.: HARMONICS up to 64 (one octave above), SINE / TRI: +8 .. 18; SQUARE / PULSE: +18 .. 36 */
#define BD_SUB_H2_S 18u         /* .. above 64 (two octaves) */
#define BD_SUB_H1_Q 18u
#define BD_SUB_H2_Q 36u
#define BD_SUB_BLK 1u           /* the per-block work (the step, the unison rule, the FM6 follow): under 1 */
/* CYCLE (job F): est.: 15 to 30 host instructions a block and voice while an active slot reads it (the tick, the curve, one multiply-add) = 0.5 to 0.9 a sample, rounded up */
#define BD_CYCLE 1u
/* WAVE STEPS (job F): est. +0.3 a sample and voice (5 to 10 instructions a block): 2 for the part's 8 voices (the table's unit is the whole instruction) */
#define BD_STEPS 2u
/* TWIN (job E) cost rows, host instructions a sample and voice, in the Mac bench's units. Measured with uf_test's TWIN rows (valgrind callgrind instruction counts, the Linux host has no
 * counter: worst of PHASE and ANALOG, 8 voices, filter 1 running and bypassed), times the Mac / Linux ratio of filter 1 12 dB (46.6 / 69.0 = 0.675), plus 10 %, rounded up: (lin ratio).
 * The design table (30..36, 45..55, 16..22, 12..16, +2..4, 0.2..0.3 points) was an estimate and is about half of what was measured */
#define BD_TW_CHAIN 20u                 /* a voice in the chain, filter 1 running (the chain path: the stages, the cross-fade, the amp ramp) (lin ratio) */
#define BD_TW_CHAIN_BYP 26u             /* the same with filter 1 bypassed (the chain runs its own block loop) (lin ratio) */
#define BD_TW_SVF 72u                   /* LP, BP, HP, NOTCH (TRIO's filter) (lin ratio) */
#define BD_TW_LADDER 66u                /* LADDER (lin ratio) */
#define BD_TW_COMB 77u                  /* COMB (lin ratio) */
#define BD_TW_DRIVE 41u                 /* DRIVE above 0 (the worst is mid range, where the blend and the level law both run) (lin ratio) */
#define BD_TW_PAR 14u                   /* ROUTE PARALLEL with its BLEND (lin ratio) */
#define BD_TW_MOD 4u                    /* filter 2's modulation (KEY TRK 2, ENV 2, LFO 1, LFO 2, a matrix CUTOFF 2): the per voice cutoff sum (lin ratio) */
typedef struct { uint32_t ty, drive, par, mod; } bd_tw_t;   /* the dearest TYPE's cost, and whether DRIVE, PARALLEL, the modulation run in any of the states */
static void bd_tw_add(bd_tw_t *w, const engine_t *e, int ty, int drv, const int8_t *x, int mat)   /* x: the extension values (the live ones, or a MORPH end's) */
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
static void bd_feat(const track_t *t, bd_part_t *r)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    const int16_t *p = t->p;
    uint32_t pi = (uint32_t)(t - trk), i;
    bd_tw_t w = {0, 0, 0, 0};
    int8_t x[NX];
    int mat = 0;

    /* TWIN (job E): filter 2 by TYPE and ROUTE, DRIVE, the cutoff sum; a voice in the chain. The live values, and with a MORPH on the part both ends' (the sound moves between them) */
    for (i = 0; i < NMSLOT; i++) {
        const int16_t *q = &p[MSLOT_ID(i)];
        mat |= q[0] && q[1] == MD_CUT2 && q[2];
    }
    for (i = 0; i < NX; i++)
        x[i] = (int8_t)XV(t, i);
    bd_tw_add(&w, e, p[P_TWTYPE], p[P_DRV2], x, mat);
    if (pi < NSHOWN && (mph[pi].nid || mph[pi].fnid) && !morph_stale(t, &mph[pi])) {
        const morph_part_t *mp = &mph[pi];
        for (i = 0; i < 2u; i++)
            bd_tw_add(&w, e, morph_cur(mp, i)[P_TWTYPE], morph_cur(mp, i)[P_DRV2], morph_curx(mp, i), mat);
    }
    if (w.ty)
        r->ext += (r->filt ? BD_TW_CHAIN : BD_TW_CHAIN_BYP) + w.ty + (w.drive ? BD_TW_DRIVE : 0) + (w.par ? BD_TW_PAR : 0) + (w.mod ? BD_TW_MOD : 0);
    else if (r->filt && (CHAIN_ALL || ACCENT_BOTH(e)))   /* cloud job J: with CHAIN_ALL (voice_chain.c) every note whose filter 1 runs goes through the chain: the chain path's cost (est.: the measured TWIN row, BD_TW_CHAIN) */
        r->ext += BD_TW_CHAIN;

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
