/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* Tests of KEY SENSE, after Velo's method (Charles Vestal, github.com/charlesvestal/fm1-velocity, GPL-3.0-only; see firmware/src/sense.c). */
/* The simulated key for the KEY SENSE tests (tests/velo_test.c, tests/velo_tick_test.c): a rubber dome with a carbon pill,
 * and a hand. It extends the contact model of tests/input_test.c (a contact that closes at T, bounces, stays closed, opens,
 * seen once a scan tick) by what the converter reads:
 *   - open: 1023. The dome snaps: ~0.25 ms from open to seated, then a landed key can read open for ONE sample (bounce);
 *   - seated, row pulled up (undriven): the landing depth falls from ~185 (a hit) .. 360 (a gentle press) to the key's rest
 *     level (170 .. 200) over ~30 ms; a soft and a hard HOLD read within a few counts of each other;
 *   - seated, row driven: the pill's resistance. reading = hard + span x (1 - strength): per key the hardest reading is
 *     389 .. 510 and the span 100 .. 150 counts (Velo's table, one key: softest 515 .. 584, hardest 389 .. 448); the strength
 *     of the landing relaxes over ~15 ms to a share of itself (the finger rests); a later squeeze adds to it (aftertouch);
 *     noise of a few counts; a slow drift (temperature, age) added to the reading;
 *   - the release: the reading rises to open in ~0.6 ms.
 * The numbers are Velo's (docs/design/VELO_STUDY.md) and guesses; the unit's own recording will replace them. Host only (doubles). */
#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "../firmware/src/sense_cal.h"
#define NK SC_NKEY

static uint32_t pl_rng = 0x2545F491u;
static uint32_t pl_rnd(uint32_t n)
{
    pl_rng ^= pl_rng << 13;
    pl_rng ^= pl_rng >> 17;
    pl_rng ^= pl_rng << 5;
    return pl_rng % n;
}
static double pl_u(void) { return (pl_rnd(1000000u) + 0.5) / 1000000.0; }
static double pl_gauss(void) { return sqrt(-2.0 * log(pl_u())) * cos(6.283185307179586 * pl_u()); }
static double pl_beta(int a, int b)       /* Beta(a, b) for small integers: the a-th smallest of a+b-1 uniforms */
{
    double u[16], t;
    int n = a + b - 1, i, j;
    for (i = 0; i < n; i++)
        u[i] = pl_u();
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && u[j] < u[j - 1]; j--)
            t = u[j], u[j] = u[j - 1], u[j - 1] = t;
    return u[a - 1];
}

typedef struct {
    double hard, span, rest;              /* driven reading at strength 1; counts from strength 1 to 0; undriven rest level */
    double drift_rate;                    /* counts per second added to the driven reading */
} pill_key_t;
typedef struct {
    uint32_t tc, tr;                      /* contact closes, contact opens (us) */
    double s;                             /* the landing's strength 0 .. 1 */
    double hold;                          /* share of s the finger rests at after the landing */
    double sq_t0, sq_amt;                 /* a squeeze from sq_t0 us after tc: strength added over 100 ms (0 = none) */
    int nb;                               /* up to 3 short windows after the snap in which the contact reads open (bounce) */
    double bs[3], bl[3];                  /* their start (us after the snap) and length (us) */
} pill_press_t;

#define PL_SNAP_US 250.0
#define PL_REL_US 600.0

static void pill_key_init(pill_key_t *k)
{
    k->hard = 389.0 + 121.0 * pl_u();
    k->span = 100.0 + 50.0 * pl_u();
    k->rest = 170.0 + 30.0 * pl_u();
    k->drift_rate = 0.0;
}
/* the dome's bounce: 0 .. 3 windows of 40 .. 500 us in the 3 ms after the snap, as the contact model of tests/input_test.c (runs of 30 .. 700 us) */
static void pill_bounce(pill_press_t *p)
{
    int b;
    p->nb = (int)pl_rnd(4);
    for (b = 0; b < p->nb; b++) {
        p->bs[b] = 100.0 + 2800.0 * pl_u();
        p->bl[b] = 40.0 + 460.0 * pl_u();
    }
}
/* one reading of key k at time t (us) while press p (may be NULL) is the one in progress; driven = the row driven */
static uint32_t pill_read(const pill_key_t *k, const pill_press_t *p, uint32_t t, int driven, double drift_counts)
{
    double v, dt, seat, s_eff, un;
    if (!p || (int32_t)(t - p->tc) < 0)
        return 1023u;
    dt = (double)(uint32_t)(t - p->tc);
    if (dt >= (double)(p->tr - p->tc) + PL_REL_US)
        return 1023u;
    {
        int b;
        for (b = 0; b < p->nb; b++)
            if (dt >= PL_SNAP_US + p->bs[b] && dt < PL_SNAP_US + p->bs[b] + p->bl[b])
                return 1023u;
    }
    s_eff = p->s * (p->hold + (1.0 - p->hold) * exp(-dt / 15000.0));
    if (p->sq_amt > 0.0 && dt > p->sq_t0) {
        double x = (dt - p->sq_t0) / 100000.0;
        s_eff += p->sq_amt * (x > 1.0 ? 1.0 : x);
    }
    s_eff = s_eff < 0.0 ? 0.0 : s_eff > 1.2 ? 1.2 : s_eff;
    un = k->rest + (185.0 + 175.0 * (1.0 - p->s) - k->rest) * exp(-dt / 30000.0);
    if (driven)
        v = k->hard + k->span * (1.0 - s_eff) + drift_counts + 4.0 * pl_gauss();
    else
        v = un + 3.0 * pl_gauss();
    seat = v;
    if (dt < PL_SNAP_US)                                  /* the snap: from open to seated */
        v = 1023.0 - (1023.0 - seat) * dt / PL_SNAP_US;
    else if (dt >= (double)(p->tr - p->tc))               /* the release */
        v = seat + (1023.0 - seat) * (dt - (double)(p->tr - p->tc)) / PL_REL_US;
    return (uint32_t)(v < 0.0 ? 0.0 : v > 1023.0 ? 1023.0 : v + 0.5);
}

/* ---- a hand: the presses of every key, in time order ---- */
#define MAXP 160
typedef struct {
    pill_press_t p[NK][MAXP];
    uint32_t n[NK];
    uint32_t t_end;
} hand_t;
static pill_key_t keys[NK];

static void keys_make(void)
{
    uint32_t k;
    for (k = 0; k < NK; k++)
        pill_key_init(&keys[k]);
}
static void press_add(hand_t *h, uint32_t k, uint32_t tc, uint32_t dur, double s, double hold)
{
    pill_press_t *p;
    if (h->n[k] >= MAXP)
        return;
    p = &h->p[k][h->n[k]++];
    memset(p, 0, sizeof *p);
    p->tc = tc;
    p->tr = tc + dur;
    p->s = s;
    p->hold = hold;
    pill_bounce(p);
}
/* a session: gestures 150 .. 700 ms apart, a note or (12 %) a chord of three with the fingers up to 12 ms apart; strengths Beta(a, b) */
static void hand_make(hand_t *h, uint32_t npress, int a, int b, double chord_share)
{
    uint32_t t = 200000u, made = 0, free_at[NK] = {0}, k, i;
    memset(h, 0, sizeof *h);
    while (made < npress) {
        uint32_t nn = pl_u() < chord_share ? 3u : 1u, kk[3], dt[3] = {0, 0, 0}, ok = 1;
        for (i = 0; i < nn; i++) {
            kk[i] = pl_rnd(NK);
            if (i)
                dt[i] = pl_rnd(12000u);
            if (free_at[kk[i]] > t + dt[i])
                ok = 0;
        }
        for (i = 0; i < nn; i++)
            for (k = 0; k < i; k++)
                if (kk[i] == kk[k])
                    ok = 0;
        if (ok) {
            for (i = 0; i < nn; i++) {
                uint32_t dur = 80000u + pl_rnd(400000u);
                press_add(h, kk[i], t + dt[i], dur, pl_beta(a, b), 0.55 + 0.3 * pl_u());
                free_at[kk[i]] = t + dt[i] + dur + 30000u;
                made++;
            }
        }
        t += 150000u + pl_rnd(550000u);
    }
    h->t_end = t + 600000u;
}


/* the press of key k in progress at time t (us), or NULL */
static const pill_press_t *press_at(const hand_t *h, uint32_t k, uint32_t t)
{
    uint32_t i;
    for (i = 0; i < h->n[k]; i++)
        if ((int32_t)(t - h->p[k][i].tc) >= 0 && (int32_t)(t - (h->p[k][i].tr + 2000u)) < 0)
            return &h->p[k][i];
    return NULL;
}
/* does the contact of key k read closed at time t (the digital scan's view): from the snap to the release, not in a bounce window */
static int contact_closed(const hand_t *h, uint32_t k, uint32_t t)
{
    uint32_t i;
    int b;
    for (i = 0; i < h->n[k]; i++) {
        const pill_press_t *p = &h->p[k][i];
        double dt;
        if ((int32_t)(t - p->tc) < 0 || (int32_t)(t - p->tr) >= 0)
            continue;
        dt = (double)(uint32_t)(t - p->tc);
        if (dt < PL_SNAP_US * 0.5)
            return 0;
        for (b = 0; b < p->nb; b++)
            if (dt >= PL_SNAP_US + p->bs[b] && dt < PL_SNAP_US + p->bs[b] + p->bl[b])
                return 0;
        return 1;
    }
    return 0;
}
