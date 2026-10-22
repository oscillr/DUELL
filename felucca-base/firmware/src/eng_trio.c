/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* TRIO: three oscillators with ring modulation and hard sync, in the style of the sound chips of early 8-bit home computers. Felucca's own design.
 * No filter of its own since the engine filters left (TWIN_FILTER_DESIGN_2, cloud job J): its filter lives on as filter 2's LP, BP, HP and NOTCH (chain_f2.c f2_svf: the same SVF, the
 * same knees and damping, the slow drift from a generator of its own); filter 1 is the MULTI FILTER (voice.c uf_run).
 *
 * Oscillators: triangle, saw, pulse (PW), pitched noise (a new random value 16 times a cycle)
 * and two combined waves (saw AND triangle, pulse AND saw: the bitwise AND of the 16-bit
 * waves, as such chips mix two waveforms). WAVE picks one of 16 sets: the three waves, their
 * levels and the interactions:
 *   RING  osc 1's output flips sign with osc 3's top phase bit (the XOR ring of the triangle;
 *         osc 3 may be silent and only modulate),
 *   SYNC  osc 2 restarts whenever osc 1 starts a cycle.
 * EDIT: WAVE, INT2 / INT3 (osc 2 / osc 3 intervals, semitones), DTN (osc 2 up, osc 3
 * down, cents), PW. The track's ENV / LFO -> SHP move PW.
 *
 * Band limiting: every step of a saw or a pulse, the sync restarts of osc 2 and the ring
 * flips of osc 1 get a polyBLEP correction. The naive waves are computed and each step adds
 * its residual to the sample before it and the one after it (the voice runs one sample late
 * for that). Triangle and noise have no step correction; the combined waves alias like the
 * originals.
 */
enum { TW_OFF, TW_TRI, TW_SAW, TW_PLS, TW_NOI, TW_ST, TW_PS };
#define TRIO_RING 1u
#define TRIO_SYNC 2u
#define TRIO_G3 11000            /* osc level with three oscillators sounding */
#define TRIO_G2 15000            /* two */
#define TRIO_G1 24000            /* one */

typedef struct {
    uint8_t w[3];                /* TW_* of osc 1..3 */
    uint8_t flags;               /* TRIO_RING | TRIO_SYNC */
    int16_t g[3];                /* Q15 levels (0: silent; osc 3 still runs for RING) */
} trio_set_t;

static const char *const N_TRIO_WAVE[] = {"SAW3", "PLS3", "PPT", "SST", "TRI3", "S&T", "P&S", "P+N",
                                          "NOIS", "SYNC", "SYNCP", "SYNC3", "RING", "RING3", "R+S", "R+SP"};
static const trio_set_t TRIO_SETS[16] = {
    {{TW_SAW, TW_SAW, TW_SAW}, 0, {TRIO_G3, TRIO_G3, TRIO_G3}},                 /* SAW3 */
    {{TW_PLS, TW_PLS, TW_PLS}, 0, {TRIO_G3, TRIO_G3, TRIO_G3}},                 /* PLS3 */
    {{TW_PLS, TW_PLS, TW_TRI}, 0, {TRIO_G3, TRIO_G3, TRIO_G3 + 3000}},          /* PPT */
    {{TW_SAW, TW_SAW, TW_TRI}, 0, {TRIO_G3, TRIO_G3, TRIO_G3 + 3000}},          /* SST */
    {{TW_TRI, TW_TRI, TW_TRI}, 0, {TRIO_G3 + 2000, TRIO_G3 + 2000, TRIO_G3 + 2000}},   /* TRI3 */
    {{TW_ST, TW_ST, TW_TRI}, 0, {TRIO_G3 + 3000, TRIO_G3 + 3000, TRIO_G3}},     /* S&T */
    {{TW_PS, TW_PS, TW_TRI}, 0, {TRIO_G3 + 3000, TRIO_G3 + 3000, TRIO_G3}},     /* P&S */
    {{TW_PLS, TW_OFF, TW_NOI}, 0, {TRIO_G2, 0, TRIO_G2 - 4000}},                /* P+N */
    {{TW_NOI, TW_OFF, TW_OFF}, 0, {TRIO_G1, 0, 0}},                             /* NOIS */
    {{TW_SAW, TW_SAW, TW_OFF}, TRIO_SYNC, {TRIO_G2 - 5000, TRIO_G2 + 5000, 0}}, /* SYNC */
    {{TW_PLS, TW_PLS, TW_OFF}, TRIO_SYNC, {TRIO_G2 - 5000, TRIO_G2 + 5000, 0}}, /* SYNCP */
    {{TW_SAW, TW_SAW, TW_TRI}, TRIO_SYNC, {TRIO_G3 - 3000, TRIO_G3 + 4000, TRIO_G3}},  /* SYNC3 */
    {{TW_TRI, TW_OFF, TW_OFF}, TRIO_RING, {TRIO_G1, 0, 0}},                     /* RING */
    {{TW_TRI, TW_OFF, TW_TRI}, TRIO_RING, {TRIO_G2 + 2000, 0, TRIO_G2 - 3000}}, /* RING3 */
    {{TW_TRI, TW_SAW, TW_OFF}, TRIO_RING | TRIO_SYNC, {TRIO_G2 + 2000, TRIO_G2, 0}},   /* R+S */
    {{TW_TRI, TW_PLS, TW_OFF}, TRIO_RING | TRIO_SYNC, {TRIO_G2 + 2000, TRIO_G2, 0}},   /* R+SP */
};
static const char *const N_TRIO_MODE[] = {"LP", "BP", "HP", "NOT"};   /* (the names of filter 2's LP BP HP NOTCH: icons.c, params.c) */
/* filter 2's output = (in * a + bp * b + lp * c) >> 10 (chain_f2.c f2_svf); HP and notch take k * bp off (k: per block) */
static const int16_t TRIO_MIX[4][3] = {{0, 0, 1024}, {0, 1400, 0}, {1024, 0, -1024}, {1024, 0, 0}};

/* polyBLEP of a step j (the value after it minus the one before, at level g Q15) that lies a
 * fraction x (Q15) of a sample before sample i: half the step is spread over the sample before
 * it (cr[0]) and sample i (cr[1]), with the usual quadratic residual; cr = the block's
 * corrections + i */
static inline void trio_blep(int32_t j, int32_t x, int32_t g, int32_t *cr)
{
    int32_t h = ((j >> 1) * g) >> 15, y = 32767 - x;
    cr[0] += (h * ((x * x) >> 15)) >> 15;
    cr[1] -= (h * ((y * y) >> 15)) >> 15;
}

/* how far (Q15 of one sample) a phase is past a step it crossed: past / inc */
static inline int32_t trio_frac(uint32_t past, uint32_t inc)
{
    uint32_t d = inc >> 15, x;
    if (!d)
        return 0;
    x = past / d;
    return x > 32767u ? 32767 : (int32_t)x;
}

typedef struct {
    uint32_t pw;                 /* pulse width as a phase */
    int32_t pdc, psm;            /* means of the naive pulse and of pulse AND saw (taken off) */
} trio_pw_t;

static inline int32_t trio_tri16(uint32_t ph)          /* the 16-bit triangle of a combined wave */
{
    return (int32_t)(((ph & 0x80000000u) ? ~ph : ph) >> 15) & 0xFFFF;
}
static inline int32_t trio_st(uint32_t ph) { return ((int32_t)(ph >> 16) & trio_tri16(ph)) - 16384; }
static inline int32_t trio_ps(uint32_t ph, const trio_pw_t *q)
{
    return (ph >= q->pw ? (int32_t)(ph >> 16) : 0) - q->psm;
}

/* naive value of a wave at phase ph (the step of a sync restart) */
static int32_t trio_naive(uint32_t w, uint32_t ph, const trio_pw_t *q)
{
    switch (w) {
    case TW_TRI:
        return osc_tri(ph);
    case TW_SAW:
        return (int32_t)(ph >> 16) - 32768;
    case TW_PLS:
        return (ph < q->pw ? 32767 : -32768) - q->pdc;
    case TW_ST:
        return trio_st(ph);
    case TW_PS:
        return trio_ps(ph, q);
    default:
        return 0;
    }
}

/* one oscillator over the block: mix[i] += its sample at level g, its steps into cr; one loop per
 * wave, so the wave is not looked at per sample. wx (osc 1 of a SYNC set: saw or pulse): where it
 * starts a cycle, how far before sample i (Q15), else -1 */
static void trio_pass(uint32_t w, uint32_t *php, uint32_t inc, int32_t g, const trio_pw_t *q, int32_t *nz,
                      int32_t *nst, int32_t *mix, int32_t *cr, int16_t *wx, uint32_t n)
{
    uint32_t ph = *php, o, i, pw = q->pw;
    int32_t pdc = q->pdc, z = *nz;
    switch (w) {
    case TW_TRI:
        for (i = 0; i < n; i++) {
            ph += inc;
            mix[i] += (osc_tri(ph) * g) >> 15;
        }
        break;
    case TW_SAW:
        for (i = 0; i < n; i++) {
            o = ph;
            ph += inc;
            if (ph < o) {
                int32_t x = trio_frac(ph, inc);
                trio_blep(-65536, x, g, cr + i);
                if (wx)
                    wx[i] = (int16_t)x;
            }
            mix[i] += (((int32_t)(ph >> 16) - 32768) * g) >> 15;
        }
        break;
    case TW_PLS:
        for (i = 0; i < n; i++) {
            o = ph;
            ph += inc;
            if (ph < o) {
                int32_t x = trio_frac(ph, inc);
                trio_blep(65535, x, g, cr + i);
                if (wx)
                    wx[i] = (int16_t)x;
            }
            if (ph - pw < o - pw)                       /* crossed pw */
                trio_blep(-65535, trio_frac(ph - pw, inc), g, cr + i);
            mix[i] += (((ph < pw ? 32767 : -32768) - pdc) * g) >> 15;
        }
        break;
    case TW_NOI:
        for (i = 0; i < n; i++) {
            o = ph;
            ph += inc;
            if ((o ^ ph) & 0x08000000u)
                z = (int32_t)(noise32(nst) >> 16) - 32768;
            mix[i] += (z * g) >> 15;
        }
        break;
    case TW_ST:
        for (i = 0; i < n; i++) {
            ph += inc;
            mix[i] += (trio_st(ph) * g) >> 15;
        }
        break;
    case TW_PS:
        for (i = 0; i < n; i++) {
            ph += inc;
            mix[i] += (trio_ps(ph, q) * g) >> 15;
        }
        break;
    default:                                            /* silent: the phase still runs (RING) */
        ph += inc * n;
        break;
    }
    *php = ph;
    *nz = z;
}

/* osc 1 as a triangle whose sign osc 3's top phase bit flips (RING; osc 3 starts at p3); each flip
 * is a step of twice the triangle's value. wx as in trio_pass */
static void trio_ring(uint32_t *php, uint32_t inc, int32_t g, uint32_t p3, uint32_t inc3, int32_t *mix, int32_t *cr,
                      int16_t *wx, uint32_t n)
{
    uint32_t ph = *php, o, o3, i;
    for (i = 0; i < n; i++) {
        int32_t s, sg;
        o = ph;
        ph += inc;
        o3 = p3;
        p3 += inc3;
        if (wx && ph < o)
            wx[i] = (int16_t)trio_frac(ph, inc);
        s = osc_tri(ph);
        sg = (p3 & 0x80000000u) ? -g : g;
        if ((o3 ^ p3) & 0x80000000u)                    /* flipped: a step of 2 s */
            trio_blep(2 * s, trio_frac(p3 & 0x7FFFFFFFu, inc3), sg, cr + i);
        mix[i] += (s * sg) >> 15;
    }
    *php = ph;
}

/* osc 2, saw or pulse, restarted where osc 1 starts a cycle (wx, from trio_pass / trio_ring): the
 * restart lies x before sample i, so osc 2 is x of its own cycle in; the jump from where it was
 * to its start is one step */
static void trio_sync(uint32_t w, uint32_t *php, uint32_t inc, int32_t g, const trio_pw_t *q, const int16_t *wx,
                      int32_t *mix, int32_t *cr, uint32_t n)
{
    uint32_t ph = *php, o, i, pw = q->pw;
    int32_t pdc = q->pdc, pls = w == TW_PLS;
    for (i = 0; i < n; i++) {
        o = ph;
        ph += inc;
        if (wx[i] >= 0) {
            uint32_t back = (uint32_t)wx[i] * (inc >> 15);
            trio_blep(trio_naive(w, 0, q) - trio_naive(w, ph - back, q), wx[i], g, cr + i);
            ph = o = back;                              /* no step of its own in this sample */
        }
        if (pls) {
            if (ph < o)
                trio_blep(65535, trio_frac(ph, inc), g, cr + i);
            if (ph - pw < o - pw)
                trio_blep(-65535, trio_frac(ph - pw, inc), g, cr + i);
            mix[i] += (((ph < pw ? 32767 : -32768) - pdc) * g) >> 15;
        } else {
            if (ph < o)
                trio_blep(-65536, trio_frac(ph, inc), g, cr + i);
            mix[i] += (((int32_t)(ph >> 16) - 32768) * g) >> 15;
        }
    }
    *php = ph;
}

/* osc 2 / 3 increment: the base pitch + semitones + cents, with the voice's fine factor (unison,
 * TUNE) that m->inc carries */
static uint32_t trio_inc(int32_t pitch16, int32_t semi, int32_t ct, int32_t fine)
{
    return cents_inc(pitch16 + semi * 16, ct, fine);
}

static void trio_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->ph[0] = 0;
    v->ph[1] = 0x1C000000u;      /* start phases a little apart (a third apart would cancel all but
                                  * every third harmonic of equal-pitched saws until DTN drifts them) */
    v->ph[2] = 0x38000000u;
    if (!v->s[2])
        v->s[2] = 0x2468ACE + (int32_t)v->age;   /* noise state */
    v->s[3] = 0;                 /* held noise value */
    v->s[4] = 0;                 /* the sample waiting for its step corrections */
}

static void trio_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    const trio_set_t *ws = &TRIO_SETS[(uint32_t)p[P_E0] & 15u];
    uint32_t w1 = ws->w[0], w2 = ws->w[1], w3 = ws->w[2], i;
    uint32_t ring = ws->flags & TRIO_RING, sync = ws->flags & TRIO_SYNC;
    int32_t g1 = ws->g[0], g2 = ws->g[1], g3 = ws->g[2];
    uint32_t base = pitch_inc(m->pitch16);
    int32_t fine = (base >> 12) ? (int32_t)(m->inc - base) / (int32_t)(base >> 12) : 0;
    uint32_t inc1 = m->inc;
    uint32_t inc2 = trio_inc(m->pitch16, p[P_E1], p[P_E3], fine);
    uint32_t inc3 = trio_inc(m->pitch16, p[P_E2], -p[P_E3], fine);
    int32_t duty = clamp((p[P_E7] << 8) + m->shape - (64 << 8), 768, 32000);   /* Q15 */
    trio_pw_t q;
    int32_t cur;
    int32_t mix[CTL], cr[CTL + 1];                      /* blocks are CTL long (dsp.c amp_at) */
    int16_t wx[CTL];
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2], ph3s = ph2;   /* state in locals: out[] may alias v->s[] */
    int32_t nst = v->s[2], nz = v->s[3], prev = v->s[4];
    if (n > CTL)
        n = CTL;
    if (m->amask) {                                     /* ARP TARGET (SPEC 2.3): OSC k on the arp's pitch (bit k - 1), */
        if (m->amask & 1u) {                            /* the arp's gate on its level */
            inc1 = atg_inc(m);
            g1 = mulq15(g1, m->again);
        }
        if (m->amask & 2u) {
            inc2 = trio_inc(atg_pitch(m, 1), p[P_E1], p[P_E3], fine);
            g2 = mulq15(g2, m->again);
        }
        if (m->amask & 4u) {
            inc3 = trio_inc(atg_pitch(m, 2), p[P_E2], -p[P_E3], fine);
            g3 = mulq15(g3, m->again);
        }
    }
    q.pw = (uint32_t)duty << 17;
    q.pdc = 2 * duty - 32768;
    q.psm = 32768 - ((duty * duty) >> 15);
    for (i = 0; i < n; i++) {
        mix[i] = cr[i] = 0;
        wx[i] = -1;
    }
    cr[n] = 0;
    trio_pass(w3, &ph2, inc3, g3, &q, &nz, &nst, mix, cr, 0, n);   /* first: RING reads its start phase */
    if (ring)
        trio_ring(&ph0, inc1, g1, ph3s, inc3, mix, cr, sync ? wx : 0, n);
    else
        trio_pass(w1, &ph0, inc1, g1, &q, &nz, &nst, mix, cr, sync ? wx : 0, n);
    if (sync)
        trio_sync(w2, &ph1, inc2, g2, &q, wx, mix, cr, n);
    else
        trio_pass(w2, &ph1, inc2, g2, &q, &nz, &nst, mix, cr, 0, n);
    cur = prev;
    for (i = 0; i < n; i++) {
        int32_t x = cur + cr[i];                        /* one sample late: its corrections are complete */
        cur = mix[i];
        out[i] += voice_amp(x, m, i) << 1;
    }
    prev = cur + cr[n];
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[2] = nst;
    v->s[3] = nz;
    v->s[4] = prev;
}

static const preset_t TRIO_PRESETS[] = {
    /* name, {WAVE, INT2, INT3, DTN, -, -, -, PW} (MODE CUT RES are filter 2's since cloud job J), {A D S R}, fenv, mono */
    {"FAT BASS", {0, 0, -12, 9, 0, 0, 0, 64}, {0, 62, 60, 25}, 40, 1, FX(10, 0, 10, 10), PAT(2)},
    {"ARP LEAD", {2, 12, 0, 4, 0, 0, 0, 32}, {0, 60, 90, 30}, 8, 0, FX(0, 0, 35, 20), PAT(13)},
    {"SYNC LEAD", {9, 9, 0, 0, 0, 0, 0, 64}, {2, 70, 100, 40}, 18, 1, FX(0, 10, 40, 25), PAT(4)},
    {"RING BELL", {13, 0, 18, 6, 0, 0, 0, 64}, {0, 92, 0, 80}, 0, 0, FX(0, 20, 35, 55), PAT(7)},
    {"CHIP CHOIR", {1, 0, 12, 7, 0, 0, 0, 40}, {70, 90, 110, 85}, 30, 0, FX(0, 50, 20, 65), PAT(5)},
};

static const uint8_t TRIO_UIORD[8] = {0, 1, 2, 3, 7, 4, 5, 6};   /* card position -> E index: ENGINE WAVE INT2 INT3; EDIT 1 DTN PW (MODE CUT RES are unused slots, hidden); the stored ids never move */

static const engine_t ENG_TRIO = {
    .name = "TRIO",
    .page_title = {"OSC", "TONE"},
    .edit = {
        {"WAVE", F_ENUM, 0, 15, 0, N_TRIO_WAVE, 0},
        {"INT2", F_SEMI, -24, 24, 0, 0, 0},
        {"INT3", F_SEMI, -24, 24, -12, 0, 0},
        {"DTN", F_INT, 0, 50, 6, 0, "ct"},
        {"-", F_INT, 0, 0, 0, 0, 0},                 /* was MODE: filter 2's TYPE */
        {"-", F_INT, 0, 0, 0, 0, 0},                 /* was CUT: filter 2's CUTOFF */
        {"-", F_INT, 0, 0, 0, 0, 0},                 /* was RES: filter 2's RES */
        {"PW", F_PCT, 0, 127, 64, 0, 0},
    },
    .presets = TRIO_PRESETS,
    .npresets = NELEM(TRIO_PRESETS),
    .note_on = trio_note_on,
    .render = trio_render,
    .knob = {P_FREEA, P_FREEB, P_E7, P_E1},   /* filter 1's CUT and RES (the engine's own CUT and RES were here) */
    .uiord = TRIO_UIORD,
    .atgt = ATG_TRIO,            /* ARP TARGET: OSC1..OSC3 (SPEC 2.3) */
    .ufilt = 1,                  /* no filter of its own since cloud job J: the multimode filter (voice.c uf_run), and filter 2 (TWIN) */
};
