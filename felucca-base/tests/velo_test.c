/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* Tests of KEY SENSE, after Velo's method (Charles Vestal, github.com/charlesvestal/fm1-velocity, GPL-3.0-only; see firmware/src/sense.c). */
/* KEY SENSE core (firmware/src/sense.c, DUELL_VELO) on a simulated key population and hand (tests/velo_pill.h). Built plain:
 *   cc -O2 -Ifirmware/src -Itests tests/velo_test.c -lm        (host_tests.sh velo; ./velo_test --csv FILE writes a recording)
 * The scan is the one of hal/fm1_input.h: one column every 100 us, 11 columns a frame, an analog key once every two frames
 * (rows PA7 on even frames, PA8 on odd ones), the converter reading taken at the tick after it was started; the event step
 * (sense_gate) runs once per audio half (2.9 ms). The numbers of the simulated keys are Velo's (VELO_STUDY.md) and guesses: the
 * gates below are for THIS simulated hand, not for anyone's keys.
 *   1  the simulated hand: velocity median 50 .. 70, at most 3 % at 110 or more, at most 5 % below 10 (cold start, no stored
 *      calibration; and again from a stored one), the velocity follows the strength (rank correlation)
 *   2  the added latency of the note (against the digital scan alone)
 *   3  calibration moves both ways (a drift up, a drift down), is clamped, has a reset
 *   4  gaps: a 40 ms gap is never a release, the open samples either side of it are not "in a row", a landing waiting for its
 *      strength gets the fixed velocity, a key seen seated after a gap has an unknown landing; a gap in the middle of the hand
 *   5  a landed key that reads open for one sample is not released; two open samples release
 *   6  a chord: notes inside the window take the first note's velocity, outside they do not; several fingers
 *   7  aftertouch: zero at the landing, up to 127 on a squeeze, back to zero
 *   8  setting OFF: no gating, velocity 100, no drive, no events; the curve; the walk; the store (CRC, bad data); state RAM */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "velo_pill.h"
#include "../firmware/src/sense.c"

/* the calibration store of the patch store job: a RAM stub */
static uint8_t stub_buf[SENSE_CAL_BYTES];
static int stub_have, stub_saves;
int velo_cal_load(uint8_t *buf, uint32_t n)
{
    if (!stub_have)
        return -1;
    memcpy(buf, stub_buf, n < SENSE_CAL_BYTES ? n : SENSE_CAL_BYTES);
    return (int)SENSE_CAL_BYTES;
}
int velo_cal_save(const uint8_t *buf, uint32_t n)
{
    memcpy(stub_buf, buf, n);
    stub_have = 1;
    stub_saves++;
    return 0;
}

static int fails;
static int check(const char *what, int ok)
{
    printf("%-78s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        fails++;
    return ok;
}

/* ---------------------------------------------------------------- the scan ---- */
/* the analog key of (frame parity, column): rows PA7 on even frames (notes 9 .. 19), PA8 on odd (notes 1,0,3,2,5,4,6,7,8 on columns 2 .. 10) */
static const int8_t SNOTE[2][11] = {{9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19}, {-1, -1, 1, 0, 3, 2, 5, 4, 6, 7, 8}};

#define NK SC_NKEY
typedef struct {
    uint32_t note, vel, t_on, t_on0, t_c;
    double s;
    int dup;
} noteon_t;
static noteon_t non_[4096];
static uint32_t nnon;
typedef struct {
    uint32_t gap_every;                   /* a 40 ms stop of the scan and the event step every this many ms (0: none) */
    double drift_per_s;                   /* counts per second, all keys, on the driven readings */
    double drift_start, drift_end;        /* the drift is only applied between these times (us) */
    int csv;                              /* write the raw log of the readings */
} simopt_t;
static FILE *csv_f, *csv_ref;

static uint32_t dig_on(const pill_press_t *p) { return p->tc + 1100u + 1100u * (1u + 0u); }   /* two closed frames (~2.2 ms) */
/* run the scan and the event step over the hand; the notes that started are in non_ */
static void sim_run(const hand_t *h, const simopt_t *o)
{
    uint32_t t, tick = 0, next_gate = 1500u, prev = 0, k;
    uint32_t block = 2902u;
    uint32_t gap_end = 0, gap_next = o->gap_every ? o->gap_every * 1000u : 0xFFFFFFFFu;
    nnon = 0;
    for (t = 0; t < h->t_end; t = tick * 100u) {
        uint32_t col, par, jit = (uint32_t)pl_rnd(15);
        int in_gap = t >= gap_next && t < gap_next + 40000u;
        if (t >= gap_next + 40000u && gap_next != 0xFFFFFFFFu)
            gap_next += o->gap_every * 1000u;
        (void)gap_end;
        /* the event step (an audio half) up to this tick */
        while (next_gate <= t) {
            uint32_t cur = 0, eff, rise;
            if (!(next_gate >= gap_next && next_gate < gap_next + 40000u)) {
                for (k = 0; k < NK; k++) {
                    uint32_t i;
                    for (i = 0; i < h->n[k]; i++) {
                        const pill_press_t *p = &h->p[k][i];
                        if ((int32_t)(next_gate - dig_on(p)) >= 0 && (int32_t)(next_gate - (p->tr + 8u * 1100u)) < 0)
                            cur |= 1u << k;
                    }
                }
                eff = sense_gate(cur, prev);
                rise = eff & ~prev;
                for (k = 0; k < NK; k++)
                    if ((rise >> k) & 1u && nnon < 4096u) {
                        noteon_t *n = &non_[nnon++];
                        uint32_t i, best = 0xFFFFFFFFu;
                        memset(n, 0, sizeof *n);
                        n->note = k;
                        n->vel = sense_vel(k);
                        n->t_on = next_gate;
                        for (i = 0; i < h->n[k]; i++) {          /* the press it belongs to: the last contact before now */
                            if ((int32_t)(next_gate - h->p[k][i].tc) >= 0)
                                best = i;
                        }
                        if (best != 0xFFFFFFFFu) {
                            const pill_press_t *p = &h->p[k][best];
                            uint32_t d = dig_on(p), g = 1500u + ((d > 1500u ? d - 1500u + block - 1u : 0u) / block) * block;
                            n->t_c = p->tc;
                            n->s = p->s;
                            n->t_on0 = g;                         /* the first half boundary after the digital press, no velocity */
                            if (nnon > 1u && non_[nnon - 2].note == k && non_[nnon - 2].t_c == p->tc)
                                n->dup = 1;
                        }
                    }
                prev = eff;
            }
            next_gate += block;
        }
        if (!in_gap) {
            col = tick % 11u;
            par = (tick / 11u) & 1u;
            if (SNOTE[par][col] >= 0) {
                uint32_t note = (uint32_t)SNOTE[par][col], drv = sense_want_drive(note), v;
                const pill_press_t *p = press_at(h, note, t);
                double dr = o->drift_per_s * ((double)(int32_t)(t - (uint32_t)o->drift_start) / 1e6);
                if (t < (uint32_t)o->drift_start)
                    dr = 0.0;
                if (o->drift_end && t > (uint32_t)o->drift_end)
                    dr = o->drift_per_s * ((o->drift_end - o->drift_start) / 1e6);
                v = pill_read(&keys[note], p, t + jit, (int)drv, dr);
                sense_update(note, v, drv, t + jit);
                if (csv_f) {                                   /* the logger's rows: from 10 ms before a contact to 10 ms after its release */
                    uint32_t i;
                    for (i = 0; i < h->n[note]; i++)
                        if ((int32_t)(t - (h->p[note][i].tc - 10000u)) >= 0 && (int32_t)(t - (h->p[note][i].tr + 10000u)) < 0) {
                            fprintf(csv_f, "%u,%u,%u,%u\n", t + jit, note, v, drv);
                            break;
                        }
                }
                if (csv_ref) {                                 /* what the core made of it, for the replay to equal */
                    sense_ev_t e;
                    while (sense_event_pop(&e))
                        if (e.type == SE_LAND)
                            fprintf(csv_ref, "%u,%u,%u,%u\n", e.t, e.key, e.a, e.b);
                }
            }
        }
        tick++;
    }
}

/* ---------------------------------------------------------------- statistics ---- */
static int cmpu(const void *a, const void *b) { return (int)*(const uint32_t *)a - (int)*(const uint32_t *)b; }
static void stats(uint32_t *v, uint32_t n, uint32_t *q1, uint32_t *med, uint32_t *q3, double *hi, double *lo)
{
    uint32_t i, h = 0, l = 0;
    qsort(v, n, sizeof *v, cmpu);
    *q1 = v[n / 4];
    *med = v[n / 2];
    *q3 = v[(3 * n) / 4];
    for (i = 0; i < n; i++) {
        h += v[i] >= 110u;
        l += v[i] < 10u;
    }
    *hi = 100.0 * h / n;
    *lo = 100.0 * l / n;
}
static double rank_corr(const double *x, const double *y, uint32_t n)
{
    double sx = 0, sy = 0, sxy = 0, sxx = 0, syy = 0;
    uint32_t i, j;
    static double rx[4096], ry[4096];
    for (i = 0; i < n; i++) {
        uint32_t lx = 0, ly = 0;
        for (j = 0; j < n; j++) {
            lx += x[j] < x[i];
            ly += y[j] < y[i];
        }
        rx[i] = lx;
        ry[i] = ly;
    }
    for (i = 0; i < n; i++) {
        sx += rx[i];
        sy += ry[i];
    }
    sx /= n;
    sy /= n;
    for (i = 0; i < n; i++) {
        sxy += (rx[i] - sx) * (ry[i] - sy);
        sxx += (rx[i] - sx) * (rx[i] - sx);
        syy += (ry[i] - sy) * (ry[i] - sy);
    }
    return sxy / sqrt(sxx * syy);
}
static void note_stats(const char *name, uint32_t *n_out, int gate, double *corr)
{
    static uint32_t v[4096];
    static double xs[4096], ys[4096];
    uint32_t i, n = 0, q1, med, q3;
    double hi, lo;
    for (i = 0; i < nnon; i++)
        if (!non_[i].dup) {
            v[n] = non_[i].vel;
            xs[n] = non_[i].s;
            ys[n] = non_[i].vel;
            n++;
        }
    stats(v, n, &q1, &med, &q3, &hi, &lo);
    *n_out = n;
    *corr = rank_corr(xs, ys, n);
    printf("   %-34s %4u notes  quartiles %3u / %3u / %3u   >=110: %4.1f %%   <10: %4.1f %%   rank corr %.2f\n", name, n, q1, med, q3, hi, lo, *corr);
    if (gate) {
        check("  median within 50 .. 70", med >= 50u && med <= 70u);
        check("  at most 3 % at 110 or more", hi <= 3.0);
        check("  at most 5 % below 10", lo <= 5.0);
    }
}

/* ---------------------------------------------------------------- scripted samples ---- */
static uint32_t T;                                  /* the script's clock (us) */
static void feed(uint32_t note, uint32_t adc, uint32_t drv, uint32_t dt)
{
    T += dt;
    sense_update(note, adc, drv, T);
}
static void fresh(void)
{
    sense_enable(0);
    sense_at_enable(0);
    sense_reset();
    sense_enable(1);
    sense_at_enable(1);                                 /* (DUELL 1.0: AFTERTOUCH is its own line; these scenes use both) */
    sense_curve_set(100);
    T = 1000000u;
    stub_have = 0;
}
static uint32_t pop_lands(uint32_t *vel_out, uint32_t *unk_out, uint32_t *lifts)
{
    sense_ev_t e;
    uint32_t n = 0;
    *lifts = 0;
    while (sense_event_pop(&e)) {
        if (e.type == SE_LAND) {
            *vel_out = e.a;
            *unk_out = 0;
            n++;
        }
        if (e.type == SE_LIFT)
            (*lifts)++;
    }
    return n;
}
/* land key k: an undriven landing reading, then the driven one (d) 2.2 ms later; returns the velocity of the event */
static uint32_t land(uint32_t k, uint32_t u, uint32_t d)
{
    uint32_t v = 0, unk = 0, lifts, n;
    feed(k, 1023, 0, 2200);
    feed(k, u, 0, 2200);
    feed(k, d, 1, 2200);
    n = pop_lands(&v, &unk, &lifts);
    return n == 1 ? v : 9999u;
}
static void release(uint32_t k)
{
    feed(k, 1023, 0, 2200);
    feed(k, 1023, 0, 2200);
}

/* one whole press of key k with the driven landing reading d, through the event step: the velocity it started with, 0 if it did not start */
static uint32_t press_vel(uint32_t k, uint32_t d)
{
    uint32_t cur = 1u << k, g, v;
    feed(k, 1023, 0, 2200);
    feed(k, 1023, 0, 2200);                        /* (released: two open samples) */
    feed(k, 200, 0, 2200);
    feed(k, d, 1, 2200);
    g = sense_gate(cur, 0);
    v = (g & cur) ? sense_vel(k) : 0u;
    sense_gate(0, g);
    return v;
}

/* give key k a settled range: 14 landings between 400 (hard) and 580 (soft), then released, events drained */
static void prime(uint32_t k)
{
    uint32_t i, v, unk, lifts;
    for (i = 0; i < 14u; i++) {
        feed(k, 1023, 0, 2200);
        feed(k, 200, 0, 2200);
        feed(k, 400u + (i * 53u) % 181u, 1, 2200);
        feed(k, 1023, 0, 2200);
        feed(k, 1023, 0, 2200);
    }
    pop_lands(&v, &unk, &lifts);
}

static void test_scripted(void)
{
    uint32_t v, unk, lifts, n, k = 5, i;
    sense_stat_t st;
    printf("-- a key, scripted\n");
    fresh();
    /* the first landing of a key: a mid velocity, the key's range unknown */
    v = land(k, 200, 480);
    check("first landing of a new key reads mid-scale (velocity 40 .. 90)", v >= 40u && v <= 90u);
    release(k);
    pop_lands(&v, &unk, &lifts);

    /* a landed key reading open for ONE sample is not released: no second landing, no lift */
    fresh();
    feed(k, 1023, 0, 2200);
    feed(k, 200, 0, 2200);
    feed(k, 1023, 1, 2200);                        /* the bounce: open for one sample */
    feed(k, 200, 0, 2200);
    feed(k, 470, 1, 2200);
    n = pop_lands(&v, &unk, &lifts);
    check("one open sample after the landing: not a release", lifts == 0u && n == 1u);
    sense_stat_get(&st);
    check("  .. counted as a bounce", st.bounces == 1u && st.lifts == 0u);
    feed(k, 1023, 0, 2200);
    feed(k, 1023, 0, 2200);
    n = pop_lands(&v, &unk, &lifts);
    check("two open samples in a row: the release", lifts == 1u);
    /* the same with a driven open sample (the row driven, key open: ~1023) */
    fresh();
    feed(k, 200, 0, 2200);
    feed(k, 470, 1, 2200);
    feed(k, 1023, 0, 2200);
    feed(k, 1023, 1, 2200);
    n = pop_lands(&v, &unk, &lifts);
    check("an undriven and a driven open sample: the release", lifts == 1u);

    /* the GAP: 40 ms without a sample */
    fresh();
    feed(k, 1023, 0, 2200);
    feed(k, 200, 0, 2200);
    feed(k, 470, 1, 2200);
    pop_lands(&v, &unk, &lifts);
    feed(k, 1023, 0, 2200);                        /* one open sample .. */
    feed(k, 1023, 0, 40000);                       /* .. the gap .. one more: NOT two in a row */
    n = pop_lands(&v, &unk, &lifts);
    check("a gap: the open samples either side of it are not 'two in a row'", lifts == 0u);
    feed(k, 1023, 0, 2200);
    n = pop_lands(&v, &unk, &lifts);
    check("  .. two open samples after the gap do release", lifts == 1u);
    fresh();
    feed(k, 200, 0, 2200);
    feed(k, 470, 1, 2200);
    pop_lands(&v, &unk, &lifts);
    feed(k, 200, 0, 40000);                        /* held through a gap, still seated after it */
    feed(k, 470, 1, 2200);
    feed(k, 200, 0, 2200);
    n = pop_lands(&v, &unk, &lifts);
    check("a held key through a gap: no release, no second landing", lifts == 0u && n == 0u);
    check("  .. and sense_vel / gate see it as still held (aftertouch can go on)", sense_at_value() <= 20u);
    /* the landing's strength waited for, then the gap: the strength is unknown */
    fresh();
    feed(k, 1023, 0, 2200);
    feed(k, 200, 0, 2200);                         /* landed: waiting for the driven reading */
    feed(k, 300, 1, 40000);                        /* the reading comes 40 ms late */
    n = pop_lands(&v, &unk, &lifts);
    check("a landing whose strength reading comes after a gap: the fixed velocity", n == 1u && v == SC_VEL_UNKNOWN);
    sense_stat_get(&st);
    check("  .. counted as unknown", st.unknown == 1u && st.gaps >= 1u);
    /* a key seen seated after a gap, state open: its landing is unknown */
    fresh();
    feed(k, 1023, 0, 2200);
    feed(k, 200, 0, 40000);
    n = pop_lands(&v, &unk, &lifts);
    check("seated after a gap with nothing before: unknown landing, fixed velocity", n == 1u && v == SC_VEL_UNKNOWN);
    /* the same landing without the gap takes a measured velocity */
    fresh();
    v = land(k, 200, 420);
    check("the same landing without a gap is measured, not the fixed velocity", v != SC_VEL_UNKNOWN);
    /* a gap in a touch: back to open, no landing */
    fresh();
    feed(k, 1023, 0, 2200);
    feed(k, 700, 0, 2200);                         /* touching */
    feed(k, 1023, 0, 40000);
    n = pop_lands(&v, &unk, &lifts);
    check("a touch, a gap, then open: nothing", n == 0u);

    /* the pill outside any sane range is rejected */
    fresh();
    feed(k, 1023, 0, 2200);
    feed(k, 200, 0, 2200);
    feed(k, 100, 1, 2200);                         /* a driven reading of 100: not a pill */
    n = pop_lands(&v, &unk, &lifts);
    check("a driven reading outside the pill's range waits (no velocity from it)", n == 0u);
    feed(k, 480, 1, 2200);
    n = pop_lands(&v, &unk, &lifts);
    check("  .. and the next good reading gives the velocity", n == 1u);
    (void)i;
}

static void test_chord(void)
{
    uint32_t v, unk, lifts, k0 = 3, k1 = 7, k2 = 12, vs[3] = {0}, n, got = 0;
    sense_ev_t e;
    printf("-- chords\n");
    fresh();
    prime(k0);
    prime(k1);
    prime(k2);
    T += 100000;
    /* three keys landing 0, 3 and 4 ms apart, strengths soft, hard, hard: all take the first one's velocity */
#define AT(note, adc, drv, us) sense_update((note), (adc), (drv), T + (us))
    AT(k0, 1023, 0, 0);
    AT(k0, 300, 0, 100);                            /* k0 lands, soft */
    AT(k1, 1023, 0, 500);
    AT(k2, 1023, 0, 1000);
    AT(k0, 560, 1, 2400);                           /* its driven reading: soft */
    AT(k1, 190, 0, 3100);                           /* 3 ms after the first landing */
    AT(k2, 190, 0, 4100);                           /* 4 ms after */
    AT(k1, 400, 1, 5400);                           /* hard */
    AT(k2, 400, 1, 6400);                           /* hard */
    while (sense_event_pop(&e))
        if (e.type == SE_LAND && got < 3u)
            vs[got++] = e.a;
    if (getenv("VELO_DEBUG"))
        printf("   chord velocities %u %u %u\n", vs[0], vs[1], vs[2]);
    check("three notes inside the chord window: three landings", got == 3u);
    check("  .. all take the first note's velocity", vs[0] == vs[1] && vs[1] == vs[2] && vs[0] < 40u);
    /* outside the window: their own */
    fresh();
    prime(k0);
    prime(k1);
    T += 100000;
    AT(k0, 1023, 0, 0);
    AT(k0, 300, 0, 100);
    AT(k1, 1023, 0, 500);
    AT(k0, 560, 1, 2500);
    AT(k1, 1023, 0, 8000);
    AT(k1, 190, 0, 10000);                          /* 10 ms after the first */
    AT(k1, 400, 1, 12000);
    got = 0;
    while (sense_event_pop(&e))
        if (e.type == SE_LAND && got < 3u)
            vs[got++] = e.a;
    check("a second note 10 ms after the first: its own velocity (harder)", got == 2u && vs[1] > vs[0] + 30u);
    /* a chord leader that is let go before its strength is in must not hold its followers up for ever */
    fresh();
    prime(k0);
    prime(k1);
    T += 100000;
    AT(k0, 1023, 0, 0);
    AT(k0, 300, 0, 100);
    AT(k1, 1023, 0, 500);
    AT(k1, 190, 0, 2300);
    AT(k0, 1023, 0, 2600);                          /* k0 lifted at once */
    AT(k0, 1023, 0, 4800);
    AT(k1, 400, 1, 4500);
    got = 0;
    while (sense_event_pop(&e))
        if (e.type == SE_LAND && got < 3u)
            vs[got++] = e.a;
    check("a leader let go early: both notes still get a velocity", got == 2u);
#undef AT
    (void)v; (void)unk; (void)lifts; (void)n;
}

static void test_aftertouch(void)
{
    uint32_t k = 4, i, max_hold = 0, max_after = 0;
    printf("-- aftertouch\n");
    fresh();
    feed(k, 1023, 0, 2200);
    feed(k, 220, 0, 2200);
    feed(k, 500, 1, 2200);                         /* landing reading */
    for (i = 0; i < 12; i++) {
        feed(k, 220, 0, 2200);
        feed(k, 500, 1, 2200);
    }
    check("a light hold: aftertouch about zero", sense_at_value() <= 8u);
    for (i = 0; i < 30; i++) {                     /* squeeze: the driven reading falls 70 counts */
        feed(k, 220, 0, 2200);
        feed(k, 500 - (uint32_t)(70.0 * (i + 1) / 30.0), 1, 2200);
        if (sense_at_value() > max_hold)
            max_hold = sense_at_value();
    }
    check("a squeeze of 70 counts: aftertouch reaches 100 .. 127", max_hold >= 100u && max_hold <= 127u);
    for (i = 0; i < 20; i++) {                     /* let the squeeze off */
        feed(k, 220, 0, 2200);
        feed(k, 500, 1, 2200);
    }
    check("relaxed again: aftertouch back to about zero", sense_at_value() <= 8u);
    feed(k, 1023, 0, 2200);
    feed(k, 1023, 0, 2200);
    check("released: aftertouch zero", sense_at_value() == 0u);
    (void)max_after;
}

/* ---------------------------------------------------------------- the hand ---- */
static void test_hand(void)
{
    static hand_t h;
    simopt_t o;
    uint32_t n, i, added_max = 0, added_n = 0;
    double corr, added_sum = 0, mean;
    sense_ev_t e;
    sense_stat_t st;
    uint8_t cal[SENSE_CAL_BYTES];
    printf("-- the simulated hand (%u keys, Velo's spread of floors, noise, bounce, 2.2 ms sampling with jitter)\n", NK);
    pl_rng = 0x2545F491u;
    keys_make();
    hand_make(&h, 600, 2, 3, 0.12);
    fresh();
    memset(&o, 0, sizeof o);
    o.drift_per_s = 0.0;
    sim_run(&h, &o);
    note_stats("cold start, no stored calibration", &n, 1, &corr);
    check("  every press started exactly one note", n >= 590u && n <= 600u);
    check("  the velocity follows the strength (rank correlation >= 0.60: chord followers and unknown landings are not their own)", corr >= 0.60);
    while (sense_event_pop(&e))
        ;
    sense_stat_get(&st);
    printf("   events: %u landings (%u from a driven reading, %u unknown), %u bounces, %u lifts\n", st.lands, st.lands_driven, st.unknown, st.bounces, st.lifts);
    {
        static uint32_t add[4096];
        double sum_c = 0, sum_c0 = 0;
        for (i = 0; i < nnon; i++)
            if (!non_[i].dup && non_[i].t_on >= non_[i].t_on0) {
                uint32_t d = non_[i].t_on - non_[i].t_on0;
                add[added_n++] = d;
                added_sum += d;
                sum_c += non_[i].t_on - non_[i].t_c;
                sum_c0 += non_[i].t_on0 - non_[i].t_c;
                if (d > added_max)
                    added_max = d;
            }
        qsort(add, added_n, sizeof add[0], cmpu);
        mean = added_n ? added_sum / added_n : 0;
        printf("   contact to note-on: %.2f ms mean without a velocity (digital scan + the audio half), %.2f ms with it\n", sum_c0 / added_n / 1000.0, sum_c / added_n / 1000.0);
        {
            uint32_t zero = 0, q;
            for (q = 0; q < added_n; q++)
                zero += add[q] == 0u;
            printf("   note latency ADDED by the velocity: mean %.2f ms, median %.2f, 90th %.2f, 99th %.2f, max %.2f ms (one audio half is 2.9 ms; %.0f %% of the notes wait no extra half)\n",
                   mean / 1000.0, add[added_n / 2] / 1000.0, add[added_n * 9 / 10] / 1000.0, add[added_n * 99 / 100] / 1000.0, added_max / 1000.0, 100.0 * zero / added_n);
        }
    }
    check("  the added latency is at most two audio halves", added_max <= 2u * 2902u + 100u);
    (void)sense_cal_export(cal);

    /* the same hand again from the stored calibration (a new key population would break this; the same keys do not) */
    pl_rng = 0x2545F491u;
    keys_make();
    hand_make(&h, 600, 2, 3, 0.12);
    pl_rng = 0x777u;
    sense_enable(0);
    sense_at_enable(0);
    sense_reset();
    sense_enable(1);
    sense_at_enable(1);                                 /* (DUELL 1.0: AFTERTOUCH is its own line; these scenes use both) */
    check("a stored calibration imports", sense_cal_import(cal) == 1u);
    sim_run(&h, &o);
    note_stats("warm start, calibration stored", &n, 1, &corr);

    /* another hand and the same keys: not a gate, printed */
    pl_rng = 0x1234567u;
    hand_make(&h, 600, 3, 2, 0.12);
    fresh();
    sim_run(&h, &o);
    note_stats("a hard player (Beta 3,2), cold", &n, 0, &corr);
    hand_make(&h, 600, 2, 5, 0.12);
    fresh();
    sim_run(&h, &o);
    note_stats("a soft player (Beta 2,5), cold", &n, 0, &corr);

    /* a 40 ms gap every 3 s through the session: never a release (every press has exactly one lift), a few unknowns */
    pl_rng = 0x2545F491u;
    keys_make();
    hand_make(&h, 600, 2, 3, 0.12);
    fresh();
    memset(&o, 0, sizeof o);
    o.gap_every = 3000;
    sim_run(&h, &o);
    note_stats("with a 40 ms gap every 3 s", &n, 1, &corr);
    sense_stat_get(&st);
    printf("   gaps seen by keys: %u, unknown landings: %u, lifts %u, landings %u\n", st.gaps, st.unknown, st.lifts, st.lands);
    check("  a gap never ended a press: lifts never more than landings", st.lifts <= st.lands);
    {
        uint32_t i, dups = 0;
        for (i = 0; i < nnon; i++)
            dups += non_[i].dup;
        check("  and no press started two notes", dups == 0u);
    }
}

static void test_calibration(void)
{
    static hand_t h;
    simopt_t o;
    uint32_t f0, c0, f1, c1, f2, c2, f3, c3, k, i, round;
    double corr;
    uint32_t n;
    printf("-- calibration\n");
    /* one key, 150 presses at a fixed strength spread; the key then drifts SOFTER (readings up), then HARDER (readings down) */
    pl_rng = 0xBEEF1u;
    keys_make();
    k = 8;
    memset(&h, 0, sizeof h);
    for (i = 0; i < MAXP; i++)
        press_add(&h, k, 300000u + i * 300000u, 120000u, pl_beta(2, 3), 0.7);
    h.t_end = 300000u + MAXP * 300000u + 500000u;
    fresh();
    memset(&o, 0, sizeof o);
    sim_run(&h, &o);                               /* steady */
    sense_cal_get(k, &f0, &c0);
    printf("   steady:            floor %u ceiling %u\n", f0, c0);
    check("  floor and ceiling are inside the clamps", f0 >= SC_FLOOR_MIN && f0 <= SC_FLOOR_MAX && c0 >= SC_CEIL_MIN && c0 <= SC_CEIL_MAX);
    check("  a span of at least SC_SPAN_MIN", c0 >= f0 + SC_SPAN_MIN);
    /* drift up: +50 counts over the second run's session, then the same key again */
    for (round = 0; round < 2; round++) {
        keys[k].hard += 25.0;
        sim_run(&h, &o);
    }
    sense_cal_get(k, &f1, &c1);
    printf("   keys drift +50:    floor %u ceiling %u\n", f1, c1);
    check("  the calibration followed a drift UP (both ends)", f1 > f0 + 10u && c1 > c0 + 10u);
    for (round = 0; round < 4; round++) {
        keys[k].hard -= 25.0;
        sim_run(&h, &o);
    }
    sense_cal_get(k, &f2, &c2);
    printf("   keys drift -50:    floor %u ceiling %u\n", f2, c2);
    check("  and DOWN again, from the far side (floor and ceiling both lower)", f2 < f1 - 10u && c2 < c1 - 10u);
    {   /* the clamp: a key that reads absurdly */
        for (round = 0; round < 6; round++) {
            keys[k].hard -= 60.0;
            sim_run(&h, &o);
        }
        sense_cal_get(k, &f3, &c3);
        printf("   keys read absurdly low: floor %u ceiling %u\n", f3, c3);
        check("  clamped (floor >= SC_FLOOR_MIN, span >= SC_SPAN_MIN)", f3 >= SC_FLOOR_MIN && c3 >= f3 + SC_SPAN_MIN);
    }
    sense_reset();
    sense_cal_get(k, &f3, &c3);
    check("sense_reset: the key is back at the typical range (a never-played key)",
          f3 + SC_SPAN_MIN <= c3 && f3 >= SC_FLOOR_PRIOR - 80u && f3 <= SC_FLOOR_PRIOR + 10u && c3 >= SC_CEIL_PRIOR - 10u && c3 <= SC_CEIL_PRIOR + 80u);
    {
        uint8_t buf[SENSE_CAL_BYTES], z[SENSE_CAL_BYTES];
        uint32_t nb = sense_cal_export(buf);
        check("a reset calibration exports as 'never played' for every key", nb == SENSE_CAL_BYTES && !buf[4] && !buf[5] && !buf[6] && !buf[7]);
        (void)z;
    }
    (void)n; (void)corr;
}

static void test_settings(void)
{
    uint32_t cur, i, k, v100, v50, v0, n;
    uint8_t buf[SENSE_CAL_BYTES], b2[SENSE_CAL_BYTES];
    sense_ev_t e;
    printf("-- the setting, the curve, the walk, the store\n");
    sense_enable(0);
    sense_at_enable(0);
    sense_reset();
    check("OFF can be set", sense_enabled() == 0u);
    /* OFF: nothing happens */
    pl_rng = 0x99u;
    sense_enable(0);
    {
        int same = 1;
        for (i = 0; i < 4000; i++) {
            uint32_t c = pl_rnd(1u << 27), p = pl_rnd(1u << 27);
            if (sense_gate(c, p) != c)
                same = 0;
        }
        check("OFF: sense_gate returns its input for any key bits", same);
    }
    check("OFF: velocity 100 for every note (analog and top)", sense_vel(0) == 100u && sense_vel(19) == 100u && sense_vel(26) == 100u);
    check("OFF: no drive requested, no aftertouch", sense_want_drive(3) == 0u && sense_at_value() == 0u);
    sense_update(3, 1023, 0, 1000);
    sense_update(3, 200, 0, 3200);
    sense_update(3, 450, 1, 5400);
    check("OFF: readings change nothing (no events)", !sense_event_pop(&e));
    /* ON: the top keys keep the fixed velocity */
    fresh();
    check("ON: a note of the top 7 keys plays the fixed velocity", sense_vel(20) == SC_TOP_VEL && sense_vel(26) == SC_TOP_VEL);
    check("ON: the top 7 keys are not gated", sense_gate(1u << 22, 0) == (1u << 22));
    /* the curve: flat 100 .. the measured range. A hard landing (driven reading 405) on a key whose range is 400 .. 580 */
    fresh();
    prime(6);
    cur = 1u << 6;
    sense_curve_set(100);
    v100 = press_vel(6, 372);
    check("ON: the key that landed starts (gate lets it through)", v100 != 0u);
    check("curve 100: the measured range (a hard landing is well above 100)", v100 >= 115u);
    sense_curve_set(50);
    check("curve get/set", sense_curve_get() == 50u);
    v50 = press_vel(6, 372);
    sense_curve_set(0);
    v0 = press_vel(6, 372);
    check("curve 0: flat 100", v0 == 100u);
    check("curve 50: half way between 100 and the measured velocity (+-1)",
          (int)v50 - 100 - ((int)v100 - 100) / 2 >= -1 && (int)v50 - 100 - ((int)v100 - 100) / 2 <= 1);
    sense_curve_set(100);
    v100 = press_vel(6, 610);
    check("curve 100: a soft landing is well below 100", v100 <= 20u);

    /* a note whose velocity is not in: held back; released while waiting: plays once for a block */
    fresh();
    sense_update(2, 1023, 0, T += 2200);
    sense_update(2, 200, 0, T += 2200);
    cur = 1u << 2;
    k = sense_gate(cur, 0);
    check("a closed key whose strength is not in yet is held back", k == 0u);
    sense_update(2, 430, 1, T += 2200);
    k = sense_gate(cur, 0);
    check("  .. and starts when it is", k == cur);
    fresh();
    sense_update(2, 1023, 0, T += 2200);
    sense_update(2, 200, 0, T += 2200);
    k = sense_gate(1u << 2, 0);
    check("let go before the strength came: the key bit is kept for one block", sense_gate(0, 0) == (1u << 2) && sense_gate(0, 1u << 2) == 0u && k == 0u);
    fresh();
    sense_update(2, 1023, 0, T += 2200);
    k = sense_gate(1u << 2, 0);
    sense_update(2, 1023, 0, T += 7000);
    sense_update(2, 1023, 0, T += 7000);
    k = sense_gate(1u << 2, 0);
    check("the analog never saw a landing: after SC_GATE_MAX_US the note starts with the fixed velocity", k == (1u << 2) && sense_vel(2) == SC_VEL_UNKNOWN);

    /* the walk */
    fresh();
    sense_walk_start();
    check("walk: starts on key 0, not done", sense_walk_step(NULL, NULL) == 0 && !sense_walk_done());
    for (k = 0; k < NK; k++) {
        uint32_t soft = 0, hard = 0, p;
        int32_t w = sense_walk_step(&soft, &hard);
        if (w != (int32_t)k) {
            check("walk: the key to light follows", 0);
            break;
        }
        for (p = 0; p < SC_WALK_SOFT + SC_WALK_HARD; p++) {
            uint32_t d = p < SC_WALK_SOFT ? 540u + 8u * p : 400u + 6u * p;
            feed(k, 1023, 0, 2200);
            feed(k, 200, 0, 2200);
            feed(k, d, 1, 2200);
            feed(k, 1023, 0, 2200);
            feed(k, 1023, 0, 2200);
            if (p == SC_WALK_SOFT - 1u) {
                sense_walk_step(&soft, &hard);
                if (k == 0u)
                    check("walk: counts soft presses first", soft == SC_WALK_SOFT && hard == 0u);
            }
        }
    }
    check("walk: done after 20 keys x (soft + hard) presses", sense_walk_done() && sense_walk_step(NULL, NULL) == -1);
    sense_cal_get(7, &v100, &v50);
    check("walk: the key's floor and ceiling bracket what was pressed", v100 <= 400u + 6u * 3u && v50 >= 540u + 16u);
    stub_saves = 0;
    sense_service(1000, 0);
    check("  sense_service does not store while a key is held", stub_saves == 0);
    sense_service(2000, 1);
    check("  .. and stores once idle", stub_saves == 1);
    sense_service(3000, 1);
    check("  .. only once", stub_saves == 1);
    sense_cal_export(buf);
    check("the stored record is the header + 80 bytes + CRC", SENSE_CAL_BYTES == 4u + 80u + 2u && !memcmp(buf, stub_buf, SENSE_CAL_BYTES));

    /* the store: restore, bad data refused */
    sense_reset();
    sense_cal_restore();
    sense_cal_export(b2);
    check("velo_cal_load / sense_cal_restore bring the calibration back", !memcmp(buf, b2, SENSE_CAL_BYTES));
    memcpy(b2, buf, sizeof b2);
    b2[10] ^= 0x55;
    n = sense_cal_import(b2);
    check("a flipped byte fails the CRC and is refused", n == 0u);
    memcpy(b2, buf, sizeof b2);
    b2[2] = 9;
    check("an unknown version is refused", sense_cal_import(b2) == 0u);
    {
        uint32_t c;
        memcpy(b2, buf, sizeof b2);
        for (i = 4; i < 4u + 4u * NK; i++)
            b2[i] = 0xFF;                          /* absurd values with a valid CRC */
        c = sc_crc16(b2, 4u + 4u * NK);
        b2[4u + 4u * NK] = (uint8_t)c;
        b2[5u + 4u * NK] = (uint8_t)(c >> 8);
        check("absurd values with a valid CRC are accepted but clamped", sense_cal_import(b2) == 1u);
        for (k = 0; k < NK; k++) {
            uint32_t f, cc;
            sense_cal_get(k, &f, &cc);
            if (f < SC_FLOOR_MIN || cc > SC_CEIL_MAX || cc < f + SC_SPAN_MIN)
                break;
        }
        check("  every key inside the clamps", k == NK);
    }
    sense_reset();
    stub_have = 0;
    sense_cal_restore();
    sense_cal_get(3, &v100, &v50);
    check("nothing stored: restore leaves the typical range", v100 < SC_FLOOR_PRIOR + 10u && v50 > SC_CEIL_PRIOR - 10u);
    check("state RAM at most 1 KB", sense_state_bytes() <= 1024u);
    printf("   sense state: %u bytes for %u keys\n", sense_state_bytes(), NK);
}

int main(int argc, char **argv)
{
    check("OFF is the default state of the module (owner 2026-10-21: default OFF; the state is all zero at power-on)", sense_enabled() == 0u);
    sense_enable(1);                                          /* (the scenes below test the sense core: ON) */
    if (argc >= 3 && !strcmp(argv[1], "--csv")) {             /* the raw log of the simulated hand: ts_us,key,adc,drive */
        static hand_t h;
        simopt_t o;
        csv_f = fopen(argv[2], "w");
        if (!csv_f)
            return 2;
        if (argc >= 4)
            csv_ref = fopen(argv[3], "w");
        fprintf(csv_f, "ts_us,key,adc,drive\n");
        pl_rng = 0x2545F491u;
        keys_make();
        hand_make(&h, 600, 2, 3, 0.12);
        fresh();
        memset(&o, 0, sizeof o);
        sim_run(&h, &o);
        fclose(csv_f);
        if (csv_ref)
            fclose(csv_ref);
        printf("velo_test: %u notes in the session the CSV holds\n", nnon);
        return 0;
    }
    test_scripted();
    test_chord();
    test_aftertouch();
    test_calibration();
    test_hand();
    test_settings();
    printf("velo: %d failed\n", fails);
    return fails != 0;
}
