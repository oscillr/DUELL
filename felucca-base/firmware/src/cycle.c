/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The CYCLE envelope (cloud job F, docs/design/SUB_AND_CYCLE_DESIGN.md B): a per-note generator that rises, falls and starts again, a matrix source (MS_CYCLE, mod.c). Included by voice.c
 * (after the ENV SYNC code it borrows from, before track_render).
 *
 *   MODE   LOOP rises in RISE, falls in FALL and starts again; ONCE rises and falls once and rests at 0
 *   RISE   the ATK time table (ENV_LIN: the same time as ATK's value); FALL the same table (the time of DEC's value), a straight line that CURVE bends
 *   SYNC   CYCLE's own switch (XV X_CYSYNC, not ENV SYNC): RISE and FALL are note values (esync_one, as ENV SYNC's ATK)
 *   CURVE  -64..+63: y = x + c (x^2 - x), c = CURVE / 64, x the progress along the segment. The fall is the rise's shape flipped in value (y -> 1 - y), so both segments bend the same way: 0 is straight,
 *          + starts slowly (the rise lingers low, the fall lingers high) and then goes fast, - starts fast (the rise jumps, the fall drops: a plucked decay is a fall with a negative CURVE) and then slows
 * Unipolar 0..1 (Q15 0..32767) like ENV: the matrix's negative amount inverts it. It follows no gate: it keeps cycling through the release until the AMP ENV ends the note (ONCE never repeats).
 * A new note starts the rise from where the generator stands (cycle_start, called by voice_start; a legato note in MONO / LEGATO is no voice_start and keeps it running). Ticked once a block
 * and note, only while an active matrix slot reads it (t->cy, mod.c mod_begin), by track_render, outside mod_voice (hot RAM, closed); cycle_value gives the value the tick made, the same on
 * every call in a block (mod_cycle_voice and mod_f2_voice both read it).
 * State per voice (the part's own: one core writes a part's row, no lock): the progress (Q24, 0 .. 1 along the segment as the rise sees it, the fall counts it down), the value, the stage: 7 B */
static int32_t cy_lin[NLIVE][NVOICE];                   /* the progress, Q24 */
static uint16_t cy_y[NLIVE][NVOICE];                    /* the value the last tick made, Q15 */
static uint8_t cy_st[NLIVE][NVOICE];                    /* 0 at rest (ONCE, ended: the value is 0), 1 rising, 2 falling */
typedef struct { uint32_t q, kr, kf; uint8_t rv, fv; } cy_memo_t;
static cy_memo_t cy_memo[NLIVE];   /* SYNC on: the two steps for this tempo and these values (their divides run when one changes) */

/* y = x + c (x^2 - x), x Q15 (0..32768), c Q15 (the curve / 64) */
static int32_t cy_curve(int32_t x, int32_t c)
{
    return x + ((c * ((x * x >> 15) - x)) >> 15);
}
/* the x with cy_curve(x, c) = y: (1 - c) x + c x^2 = y, x = (sqrt((1 - c)^2 + 4 c y) - (1 - c)) / (2 c), by magnitudes (numerator and denominator keep one sign). Runs at a retrigger only */
static int32_t cy_inv(int32_t y, int32_t c)
{
    int64_t a = 32768 - c;
    uint32_t s, num, den;
    if (!c)
        return y;
    s = (uint32_t)wv_isqrt((uint64_t)(a * a + 4 * (int64_t)c * y));   /* Q15 */
    num = s > (uint32_t)a ? s - (uint32_t)a : (uint32_t)a - s;
    den = (uint32_t)(c < 0 ? -2 * c : 2 * c);
    return clamp((int32_t)((num << 15) / den), 0, 32768);
}
/* RISE's and FALL's step a block along the segment (Q24): the tables' (ATK's time of the value), or the note values' with SYNC on */
static void cy_steps(const track_t *t, uint32_t *kr, uint32_t *kf)
{
    uint32_t r = (uint32_t)XV(t, X_CYRISE) & 127u, f = (uint32_t)XV(t, X_CYFALL) & 127u;
    if (XV(t, X_CYSYNC)) {
        uint32_t q = beat_samples();
        cy_memo_t *m = &cy_memo[t - trk];
        if (m->q != q || m->rv != (uint8_t)r || m->fv != (uint8_t)f || !m->kr) {
            m->q = q;
            m->rv = (uint8_t)r;
            m->fv = (uint8_t)f;
            m->kr = esync_one(q, (int32_t)r, 0);
            m->kf = esync_one(q, (int32_t)f, 0);
        }
        *kr = m->kr;
        *kf = m->kf;
        return;
    }
    *kr = ENV_LIN[r];
    *kf = ENV_LIN[f];
}

/* one tick (a block) of voice v's cycle, then its value */
static __attribute__((noinline)) void cycle_tick(track_t *t, voice_t *v)
{
    uint32_t ti = (uint32_t)(t - trk), vi = (uint32_t)(v - t->v), st = cy_st[ti][vi], kr, kf;
    int32_t lin = cy_lin[ti][vi], x, y, c = (int32_t)XV(t, X_CYCURVE) * 512;
    if (!st) {                                          /* at rest (ONCE, ended) */
        cy_y[ti][vi] = 0;
        return;
    }
    cy_steps(t, &kr, &kf);
    if (st == 1u) {
        lin += (int32_t)kr;
        if (lin >= (1 << 24)) {
            lin = 1 << 24;
            st = 2;
        }
    } else {
        lin -= (int32_t)kf;
        if (lin <= 0) {
            lin = 0;
            st = XV(t, X_CYMODE) ? 0u : 1u;             /* LOOP: the rise again; ONCE: at rest */
        }
    }
    x = lin >> 9;                                       /* Q15 */
    y = st == 2u ? 32768 - cy_curve(32768 - x, c) : cy_curve(x, c);
    if (st == 0u)
        y = 0;
    cy_lin[ti][vi] = lin;
    cy_st[ti][vi] = (uint8_t)st;
    cy_y[ti][vi] = (uint16_t)clamp(y, 0, 32767);
}
/* the CYCLE source of voice v (Q15, 0..32767): what the tick made, the same on every call of a block */
__attribute__((noinline)) int32_t cycle_value(track_t *t, voice_t *v)
{
    uint32_t ti = (uint32_t)(t - trk), vi = (uint32_t)(v - t->v);
    return ti < NLIVE && vi < NVOICE ? cy_y[ti][vi] : 0;          /* (a track of a test's own is none of the parts') */
}
/* a note starts on voice v (voice_start; sounding: the voice already played, a retrigger or a steal): the rise from where the generator stands (no jump), from 0 on a fresh voice */
static __attribute__((noinline)) void cycle_start(track_t *t, voice_t *v, int sounding)
{
    uint32_t ti = (uint32_t)(t - trk), vi = (uint32_t)(v - t->v);
    if (ti >= NLIVE || vi >= NVOICE)
        return;
    cy_lin[ti][vi] = sounding && cy_st[ti][vi] ? cy_inv((int32_t)cy_y[ti][vi], (int32_t)XV(t, X_CYCURVE) * 512) << 9 : 0;
    cy_st[ti][vi] = 1;
}
