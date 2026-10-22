/* SPDX-License-Identifier: GPL-3.0-only
 * Host experiment (not firmware, not in any default test list): a tempo-synced random-event / granular delay in integer,
 * heard against DUELL's own ping-pong delay (with and without its modulation), with and without a plate reverb after it.
 *
 * The algorithm follows the granular delay of the CHOMPI sampler's TEMPO firmware
 * (firmware/chompi-tempo/code/src/granularDelay.h, the compiled one: a tempo-synced delay whose two voices, on a random
 * share of the clock edges, retrigger, play reversed, an octave up or an octave down, with a random pan; plus the stereo
 * Haas offsets and a lock) and of granularDelay2.h (the alternative that is shipped but not compiled: 12 half-sine grains).
 * This file is a rewrite in integer C (Q8 read heads, Q15 gains, int16 buffer) of what those files do; no source line is
 * copied, and the upstream code is float. CHOMPI is MIT: Copyright (c) 2026 CHOMPI Club; see felucca-base/LICENSES/MIT-CHOMPI.txt
 * (the same text follows, as the licence asks for the notice with a "substantial portion"; the file stays under the MIT
 * licence for that part, the rest of Felucca is GPL-3.0-only).
 *
 *   MIT License
 *   Copyright (c) 2026 CHOMPI Club
 *   Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 *   documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
 *   rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 *   permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *   The above copyright notice and this permission notice shall be included in all copies or substantial portions of
 *   the Software.
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 *   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 *   COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 *   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The plate reverb after it (the "+plate" renders) is revcompare_test.c's integer Rings / Clouds reverb (MIT, Emilie Gillet,
 * see that file's header), included here.
 *
 *   build (from felucca-base, after host_tests.sh has made build/gen):
 *     cc -O2 -w -DDUELL_TEST_MODE=1 -Ibuild/gen -Ifirmware/src -Itests -o build/host/graindelay_test tests/graindelay_test.c -lm
 *   build/host/graindelay_test OUTDIR      (WAVs, index.html, report.txt into OUTDIR)
 *
 * Memory model: the delay buffer here is gd_buf, 65536 int16 = fx.c's dly_buf (128 KiB, 1.49 s at 44.1 kHz): the granular
 * mode would borrow dly_buf while it is selected (no new buffer). Its own state is sizeof(gd_t) (printed in the report). */
#define REVCOMPARE_NO_MAIN
#include "revcompare_test.c"

/* ---------------------------------------------------------------------------------------------- tables */
static int16_t GD_QSIN[257];     /* sin(pi/2 * i/256), Q15: the 1024-sample fades of a voice (index = count >> 2) */
static int16_t GD_HSIN[257];     /* sin(pi * i/256), Q15: a grain's window */
static void gd_tables(void)
{
    int i;
    for (i = 0; i <= 256; i++) {
        GD_QSIN[i] = (int16_t)(sin(1.5707963267948966 * i / 256.0) * 32767.0 + 0.5);
        GD_HSIN[i] = (int16_t)(sin(3.14159265358979 * i / 256.0) * 32767.0 + 0.5);
    }
}
/* equal-power pan: gains sqrt((1 -+ pan) / 2), pan Q15 (-32768 .. 32767), gains Q15 */
static uint32_t gd_isqrt(uint32_t x)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > x) b >>= 2;
    while (b) {
        if (x >= r + b) { x -= r + b; r = (r >> 1) + b; } else r >>= 1;
        b >>= 2;
    }
    return r;
}
static void gd_pan(int32_t pan, int32_t *gl, int32_t *gr)
{
    int32_t p = pan < -32768 ? -32768 : pan > 32767 ? 32767 : pan;
    *gl = (int32_t)gd_isqrt((uint32_t)((32768 - p) << 14));   /* sqrt(0.5 (1 - p)) * 32768 = sqrt((32768 - p) * 2^14) */
    *gr = (int32_t)gd_isqrt((uint32_t)((32768 + p) << 14));
    if (*gl > 32767) *gl = 32767;
    if (*gr > 32767) *gr = 32767;
}

/* ---------------------------------------------------------------------------------------------- state */
#define GD_N 65536u
#define GD_MASK (GD_N - 1u)
#define GD_POSMASK ((GD_N << 8) - 1u)            /* read heads: Q8 sample positions in a power-of-two ring */
#define GD_OFFL ((uint32_t)(FS * 0.020 + 0.5))    /* the stereo offsets: 20 ms left, 10 ms right (upstream: 960 / 480 samples at 48 kHz) */
#define GD_OFFR ((uint32_t)(FS * 0.010 + 0.5))
#define GD_FADE 1024u                            /* a voice's fade in / out */
#define GD_NGR 12
#define GE_ANALOG 0u
#define GE_NOISE 11u
enum { EV_RETRIG, EV_REVERSE, EV_UP, EV_DOWN, EV_NONE };
typedef struct {
    uint8_t active, fin, fout, ev, nev;
    int16_t npan;
    int32_t gl, gr;
    uint32_t rh;                                 /* Q8 */
    uint32_t cnt;
} gv_t;
typedef struct {
    uint8_t on;
    uint32_t rh, ph, inc, spd;                   /* Q8 read head, window phase Q24 and its step, speed Q8 */
    int32_t gl, gr;
} gg_t;
typedef struct {
    uint32_t w;                                  /* write head */
    uint32_t dl16, dlt16;                        /* delay (smoothed, target), Q16 samples */
    int32_t fbq, col, lp, mixq;                  /* feedback, colour, the feedback's low-pass state, return level (all Q15) */
    int32_t chance;                              /* RANDOMNESS (upstream alt control), Q15: an edge fires an event with p = chance / 2 */
    uint32_t beat;                               /* samples per quarter note */
    uint32_t rng;
    /* events mode */
    gv_t v[2];
    uint8_t cur, nxt, ev0, ev1, side;            /* side 0: random event of four (upstream knob < .5), 1: octave up with a random pan (> .5) */
    uint32_t ecnt, eper;                         /* samples since the last clock edge, edge period (here a 1/8 note) */
    /* grains mode */
    gg_t g[GD_NGR];
    uint32_t pcnt, pulse;                        /* 12 ppqn timer, pulses since the last grain trial */
    int32_t gknob;                               /* the MAIN knob's right half, 0 .. 1000 (upstream .55 .. 1) */
    /* lock (events mode) */
    uint8_t lstate;                              /* 0 off, 1 fading down, 2 locked, 3 fading up */
    uint32_t lcnt, lw, lpos;
} gd_t;
static int16_t gd_buf[GD_N];
static gd_t gd;

static uint32_t gd_rnd(void)
{
    gd.rng ^= gd.rng << 13;
    gd.rng ^= gd.rng >> 17;
    gd.rng ^= gd.rng << 5;
    return gd.rng >> 8;                          /* 24 bits */
}
static void gd_reset(void)
{
    memset(gd_buf, 0, sizeof gd_buf);
    memset(&gd, 0, sizeof gd);
    gd.rng = 0x2545F491u;
    gd.ev0 = gd.ev1 = EV_NONE;
    gd.cur = 0;
    gd.nxt = 1;
    gd.v[0].active = 1;
    gd.v[0].ev = gd.v[0].nev = EV_NONE;
    gd.v[1].ev = gd.v[1].nev = EV_NONE;
    gd_pan(0, &gd.v[0].gl, &gd.v[0].gr);
    gd_pan(0, &gd.v[1].gl, &gd.v[1].gr);
}

/* the line read at Q8 position p, linear between two samples */
static inline int32_t gd_tap(uint32_t p)
{
    uint32_t i = (p >> 8) & GD_MASK, j = (i + 1u) & GD_MASK;
    int32_t a = gd_buf[i], b = gd_buf[j];
    return a + (((b - a) * (int32_t)(p & 255u)) >> 8);
}

/* ---------------------------------------------------------------------------------------------- events mode */
static void gv_in(gd_t *g, gv_t *v)
{
    if (v->fin || v->fout || v->active) return;
    v->active = 1;
    v->fin = 1;
    v->cnt = 0;
    v->ev = v->nev;
    v->nev = EV_NONE;
    gd_pan(v->npan, &v->gl, &v->gr);
    v->npan = 0;
    {
        uint32_t back = g->dl16 >> 8;            /* Q8 */
        if (v->ev == EV_UP && g->dl16 < (g->beat * 3u / 2u + 1u) * 65536u)    /* a short delay (up to 3/8 bar): the octave starts two beats back */
            back = (g->beat * 2u > GD_N - 4096u ? GD_N - 4096u : g->beat * 2u) << 8;
        v->rh = ((g->w << 8) - back) & GD_POSMASK;
    }
}
static void gv_out(gv_t *v)
{
    if (v->fin || v->fout || !v->active) return;
    v->fout = 1;
    v->cnt = GD_FADE;
}
/* one voice's contribution (Q15 sample units in *l, *r) */
static inline void gv_read(gd_t *g, gv_t *v, int32_t *l, int32_t *r)
{
    int32_t env = 32767;
    uint32_t p;
    if (!v->active) return;
    if (v->fin) {
        if (++v->cnt > GD_FADE) { v->cnt = GD_FADE; v->fin = 0; }
        env = GD_QSIN[v->cnt >> 2];
    } else if (v->fout) {
        if (--v->cnt < 1u) { v->fout = 0; v->active = 0; }
        env = GD_QSIN[v->cnt >> 2];
    }
    switch (v->ev) {
    case EV_NONE:   v->rh = ((g->w << 8) - (g->dl16 >> 8)) & GD_POSMASK; break;
    case EV_RETRIG: v->rh = ((g->w << 8) - (g->dl16 >> 8) - (g->dl16 >> 11)) & GD_POSMASK; break;   /* 1 1/8 of the delay */
    case EV_REVERSE: v->rh = (v->rh - 256u) & GD_POSMASK; break;
    case EV_UP:     v->rh = (v->rh + 512u) & GD_POSMASK; break;
    default:        v->rh = (v->rh + 128u) & GD_POSMASK; break;
    }
    p = v->rh;
    {
        int32_t sl = gd_tap((p - (GD_OFFL << 8)) & GD_POSMASK), sr = gd_tap((p - (GD_OFFR << 8)) & GD_POSMASK);
        sl = (sl * env) >> 15;
        sr = (sr * env) >> 15;
        *l += (sl * v->gl) >> 15;
        *r += (sr * v->gr) >> 15;
    }
}
static void gd_edge(gd_t *g)
{
    int32_t half = g->chance >> 1;
    g->ev0 = g->ev1;
    if ((int32_t)(gd_rnd() & 32767u) < half) {
        int32_t pan = 0;
        g->cur = g->nxt;
        g->nxt = (uint8_t)(g->cur ^ 1u);
        if (!g->side)
            g->ev1 = (uint8_t)(gd_rnd() & 3u);
        else {
            int32_t range = 16384 + (g->chance >> 1);     /* upstream: 0.5 + alt * 0.5 */
            g->ev1 = EV_UP;
            pan = (int32_t)(((int64_t)((int32_t)(gd_rnd() & 65535u) - 32768) * range) >> 15);
        }
        g->v[g->cur].nev = g->ev1;
        g->v[g->cur].npan = (int16_t)pan;
    } else {
        g->ev1 = EV_NONE;
        if (g->ev0 != EV_NONE) {
            g->cur = g->nxt;
            g->nxt = (uint8_t)(g->cur ^ 1u);
            g->v[g->cur].nev = EV_NONE;
            g->v[g->cur].npan = 0;
        }
    }
}

/* ---------------------------------------------------------------------------------------------- grains mode */
static void gd_grain_trial(gd_t *g)
{
    int32_t k = g->gknob, norm = (k - 550) * 1000 / 450, prob;   /* norm 0 .. 1000 */
    uint32_t pulses = k > 830 ? 3u : 6u, i;
    if (g->pulse < pulses) return;
    g->pulse = 0;
    if (k > 830) prob = 300 + (k - 830) * 450 / 170; else prob = 100 + norm * 800 / 1000;       /* per mille */
    if ((int32_t)(gd_rnd() % 1000u) >= prob) return;
    for (i = 0; i < GD_NGR; i++) {
        gg_t *q = &g->g[i];
        uint32_t spd, size, minoff, maxoff, off, r;
        int32_t alt = g->chance, t1, t2, base, jit;
        if (q->on) continue;
        r = gd_rnd() & 32767u;                                  /* speed: 1, 1/2 or 2 by RANDOMNESS */
        t1 = 32768 - (alt >> 1);
        t2 = t1 + (alt >> 2);
        spd = alt == 0 ? 256u : (int32_t)r < t1 ? 256u : (int32_t)r < t2 ? 128u : 512u;
        base = (int32_t)(FS * 1.0) - norm * (int32_t)(FS * 0.75) / 1000;    /* 1 s at the left of the range, 0.25 s at the right */
        jit = (int32_t)((int64_t)base * ((int32_t)(gd_rnd() & 65535u) - 32768) * 6554 >> 30);   /* +- 20 % */
        size = (uint32_t)(base + jit);
        if (spd <= 128u) size >>= 1;
        maxoff = spd >= 512u ? GD_N - 2048u : (uint32_t)FS < GD_N - 2048u ? (uint32_t)FS : GD_N - 2048u;   /* upstream: 1 s (2 s for the octave up); here capped by the line */
        if (size * spd / 256u > maxoff) size = maxoff * 256u / spd;      /* the buffer limit: a grain must fit inside its read-back */
        minoff = size * spd / 256u;
        off = minoff + (maxoff > minoff ? gd_rnd() % (maxoff - minoff + 1u) : 0u);
        q->on = 1;
        q->rh = ((g->w - off) & GD_MASK) << 8;
        q->spd = spd;
        q->ph = 0;
        q->inc = (uint32_t)(((uint64_t)spd << 24) / ((uint64_t)size << 8));
        {
            int32_t range = 16384 + (g->chance >> 1), pan = (int32_t)(((int64_t)((int32_t)(gd_rnd() & 65535u) - 32768) * range) >> 15);
            gd_pan(pan, &q->gl, &q->gr);
            q->gl = q->gl * 3 / 4;              /* polyFactor .75 */
            q->gr = q->gr * 3 / 4;
        }
        return;
    }
}
static inline void gg_read(gg_t *q, int32_t *l, int32_t *r)
{
    int32_t s, win;
    if (!q->on) return;
    win = GD_HSIN[q->ph >> 16];
    s = (gd_tap(q->rh) * win) >> 15;
    *l += (s * q->gl) >> 15;
    *r += (s * q->gr) >> 15;
    q->ph += q->inc;
    q->rh = (q->rh + q->spd) & GD_POSMASK;
    if (q->ph >= (1u << 24)) q->on = 0;
}

/* ---------------------------------------------------------------------------------------------- one block */
/* mode 0 events, 1 grains. in: the delay send (as fx_bus.c's dly_in); wl, wr: the return (before the mix level). */
static void gd_run(int mode, const int32_t *in, int32_t *wl, int32_t *wr, uint32_t n)
{
    uint32_t i, k;
    for (i = 0; i < n; i++) {
        int32_t l = 0, r = 0, m, x;
        gd.dl16 += (int32_t)(gd.dlt16 - gd.dl16) >> 10;                   /* upstream's one-pole .001 */
        if (mode == 0) {
            if (++gd.ecnt >= gd.eper) { gd.ecnt = 0; if (gd.lstate == 0) gd_edge(&gd); }
            if (gd.lstate == 0 || gd.lstate == 1 || gd.lstate == 3) {
                if (gd.ev0 != EV_NONE || gd.ev1 != EV_NONE) {
                    gv_in(&gd, &gd.v[gd.cur]);
                    gv_out(&gd.v[gd.nxt]);
                }
                gv_read(&gd, &gd.v[0], &l, &r);
                gv_read(&gd, &gd.v[1], &l, &r);
            }
        } else {
            if (++gd.pcnt >= gd.beat / 12u) { gd.pcnt = 0; gd.pulse++; }
            gd_grain_trial(&gd);
            for (k = 0; k < GD_NGR; k++) gg_read(&gd.g[k], &l, &r);
        }
        if (gd.lstate) {                                                  /* the lock: a loop of the last delay time, no new input */
            int32_t env = 32767;
            if (gd.lstate == 1) { if (gd.lcnt > 0) gd.lcnt--; env = (int32_t)gd.lcnt * 128; if (!gd.lcnt) { gd.lstate = 2; gd.lw = gd.w; gd.lpos = 0; } }
            else if (gd.lstate == 3) { if (++gd.lcnt >= 256u) { gd.lstate = 0; } env = (int32_t)gd.lcnt * 128; }
            if (gd.lstate == 2 || gd.lstate == 3) {
                uint32_t dls = gd.dl16 >> 16, p = ((gd.lw - dls + gd.lpos) & GD_MASK) << 8;
                int32_t e2 = 32767, sl, sr;
                if (gd.lpos < 128u) e2 = (int32_t)gd.lpos * 256; else if (gd.lpos + 128u > dls) e2 = (int32_t)(dls - gd.lpos) * 256;
                if (e2 < 0) e2 = 0;
                if (++gd.lpos >= dls) gd.lpos = 0;
                sl = gd_tap((p - (GD_OFFL << 8)) & GD_POSMASK);
                sr = gd_tap((p - (GD_OFFR << 8)) & GD_POSMASK);
                l = (sl * e2) >> 15;
                r = (sr * e2) >> 15;
                if (gd.lstate == 3) { l = (l * env) >> 15; r = (r * env) >> 15; }
            } else if (gd.lstate == 1) { l = (l * env) >> 15; r = (r * env) >> 15; }
        }
        /* feedback from the mid, low-passed like DUELL's, and the write (input halved as fx_bus.c, so a loop has headroom) */
        m = (l + r) >> 1;
        gd.lp += mulq15(m - gd.lp, gd.col);
        x = (in[i] >> 1) + mulq15(gd.lp, mode == 0 ? gd.fbq : gd.fbq * 3 / 5);   /* (upstream: grains feed back .6 of the setting) */
        if (gd.lstate == 0 || gd.lstate == 3 || gd.lstate == 1) {
            gd_buf[gd.w & GD_MASK] = (int16_t)clamp(x, -32768, 32767);
            gd.w++;
        }
        wl[i] = mulq15(l << 1, gd.mixq);
        wr[i] = mulq15(r << 1, gd.mixq);
    }
}

/* ---------------------------------------------------------------------------------------------- the sources */
typedef struct { const char *tag, *what; } gs_t;
static const gs_t GS[4] = {
    {"pluck", "ANALOG / PLUCK: three plucked chords (C, Am, F) in one bar, looped twice, then the tail. 100 BPM, delay 1/8"},
    {"pad", "ANALOG / SOFT PAD: one held note (4 s), then the tail. 60 BPM, delay 1/4"},
    {"chord", "WAVE / HARM KEYS: Cmaj7, then Am7 a half bar later, then the tail. 100 BPM, delay 1/8"},
    {"beat", "PHYS / HAND DRUM (low hits) + NOISE / METAL (hats) + ANALOG / PLUCK (short stabs): two bars of a groove, then the tail. 100 BPM, delay 1/8"}};
static const double GS_STOP[4] = {4.7, 3.9, 4.7, 4.7};
static const uint8_t GS_DTIME[4] = {1, 0, 1, 1};
static void gs_track(uint32_t t, uint32_t e, uint32_t p)
{
    host_preset(&trk[t], e, p);
    trk[t].p[P_REV] = 0;
    trk[t].p[P_CHOR] = 0;
    trk[t].p[P_DLY] = 100;
}
static void gs_setup(int src)
{
    static const uint8_t C1[3] = {60, 64, 67}, K1[4] = {60, 64, 67, 71}, K2[4] = {57, 60, 64, 67};
    uint32_t i;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    song.g[G_BPM] = src == 1 ? 60 : 100;
    if (src == 0) {
        static const uint8_t A1[3] = {57, 60, 64}, F1[3] = {53, 57, 60};
        gs_track(0, GE_ANALOG, 8);                        /* PLUCK */
        for (i = 0; i < 16u; i++) {
            const uint8_t *cd = i == 0 ? C1 : i == 6 ? A1 : F1;
            put_step(&trk[0], i, i == 0 || i == 6 || i == 10 ? 3u : 0u, cd, i == 0 || i == 6 || i == 10 ? ST_NOTE : ST_REST, 0);
        }
    } else if (src == 1) {
        const uint8_t N0 = 55;
        gs_track(0, GE_ANALOG, 1);                        /* SOFT PAD */
        for (i = 0; i < 16u; i++) put_step(&trk[0], i, i == 0 ? 1u : 0u, &N0, i == 0 ? ST_NOTE : i < 14u ? ST_TIE : ST_REST, 0);
    } else if (src == 2) {
        gs_track(0, ENGI_WAVE, 2);                          /* HARM KEYS */
        for (i = 0; i < 16u; i++) {
            uint32_t n = i == 0 ? 4u : i == 8 ? 4u : 0u;
            put_step(&trk[0], i, n, i == 0 ? K1 : K2, n ? ST_NOTE : ST_REST, 0);
        }
    } else {
        static const uint8_t HD[3] = {43, 50, 55}, HAT = 72, ST[2] = {60, 67};
        gs_track(0, ENGI_PHYS, 5);                          /* HAND DRUM */
        gs_track(1, GE_NOISE, 3);                         /* METAL */
        gs_track(2, GE_ANALOG, 8);                        /* PLUCK */
        for (i = 0; i < 16u; i++) {
            uint32_t hd = i == 0 ? 0u : i == 3 ? 1u : i == 6 ? 0u : i == 8 ? 2u : i == 11 ? 1u : i == 14 ? 0u : 9u;
            if (hd < 9u) put_step(&trk[0], i, 1, &HD[hd], ST_NOTE, 0); else put_step(&trk[0], i, 0, HD, ST_REST, 0);
            if (i % 2u == 1u) put_step(&trk[1], i, 1, &HAT, ST_NOTE, 0); else put_step(&trk[1], i, 0, &HAT, ST_REST, 0);
            if (i == 2u || i == 7u || i == 10u || i == 15u) put_step(&trk[2], i, 1, &ST[i == 7u || i == 15u], ST_NOTE, 0); else put_step(&trk[2], i, 0, ST, ST_REST, 0);
        }
    }
}
#define GN (10u * FS)
static int32_t gc_l[GN], gc_r[GN], gc_d[GN];
static int gc_src = -1;
static void gs_capture(int src, FILE *rp)
{
    uint32_t t, i;
    int32_t pk = 0;
    blockstate_t bs;
    gs_setup(src);
    model_reset(M_ROOM, 90, 60);
    transport_req = 1;
    for (t = 0; t < GN; t += CTL) {
        if (t <= (uint32_t)(GS_STOP[src] * FS) && t + CTL > (uint32_t)(GS_STOP[src] * FS)) transport_req = 2;
        for (i = 0; i < CTL; i++) send_c[i] = send_d[i] = send_r[i] = mix_l[i] = mix_r[i] = 0;
        events_block(CTL);
        blk_snap(&bs, sp_tap);
        for (i = 0; i < NLIVE; i++) mix_part(&trk[i], CTL, &bs);
        mix_gather(CTL, &bs);
        for (i = 0; i < CTL; i++) {
            int32_t a = send_d[i] < 0 ? -send_d[i] : send_d[i];
            if (a > pk) pk = a;
            gc_l[t + i] = mix_l[i];
            gc_r[t + i] = mix_r[i];
            gc_d[t + i] = send_d[i];
        }
    }
    gc_src = src;
    fprintf(rp, "graindelay: source %s captured: the delay send peaks at %d\n", GS[src].tag, (int)pk);
    printf("graindelay: source %s captured: the delay send peaks at %d\n", GS[src].tag, (int)pk);
}

/* ---------------------------------------------------------------------------------------------- settings and models */
typedef struct { const char *name; int dfdbk, dmdep; int32_t chance; int gknob; } gset_t;
static const gset_t SETS[3] = {{"light", 40, 60, 9830, 600}, {"medium", 60, 60, 19661, 750}, {"heavy", 100, 60, 32767, 950}};
enum { GM_PP, GM_PPM, GM_EVL, GM_EVR, GM_GRN, GM_LOCK, GM_N };
static const char *const GM_NAME[GM_N] = {"PINGPONG", "PINGPONG_MOD", "EVENTS_RND", "EVENTS_UP", "GRAINS", "EVENTS_LOCK"};
static const char *const GM_DESC[GM_N] = {
    "DUELL ping-pong delay (fx_bus.c), the repeats alternate left and right",
    "the same with DEPTH 60 RATE 44 (the delay time wobbles: the modulation)",
    "random-event delay, left half of the reference knob: on a random share of the clock edges a voice retriggers, plays reversed, an octave up or down",
    "random-event delay, right half: octave up with a random pan",
    "12 random half-sine grains of 0.25 .. 1 s read back from the line (the alternative file), random pan, speed 1 / 1/2 / 2",
    "random-event delay with the buffer locked (a loop of one delay time) from the marked moment on"};
static uint32_t gs_lock_on[4], gs_lock_off[4];
static void gs_ref_reset(void)
{
    memset(dly_buf, 0, sizeof dly_buf);
    memset(cho_buf, 0, sizeof cho_buf);
    memset(&fx, 0, sizeof fx);
    rev_clear();
    song.g[G_RTYPE] = 0;
}
static void gs_model_reset(int src, int md, const gset_t *st, int plate)
{
    int32_t z[CTL];
    (void)z;
    song.g[G_DTIME] = GS_DTIME[src];
    song.g[G_DFDBK] = (int16_t)st->dfdbk;
    song.g[G_DCOLOR] = 70;
    song.g[G_DMIX] = 90;
    song.g[G_DMODE] = DMD_PING;
    song.g[G_PINGSPREAD] = 100;
    song.g[G_DMRATE] = 44;
    song.g[G_DMDEP] = md == GM_PPM ? (int16_t)st->dmdep : 0;
    gs_ref_reset();
    if (md >= GM_EVL) {
        uint32_t dl = div_samples((uint32_t)GS_DTIME[src]);
        gd_reset();
        gd.beat = beat_samples();
        gd.dl16 = gd.dlt16 = dl << 16;
        gd.fbq = st->dfdbk * 230;
        gd.col = 2000 + 70 * 240;
        gd.mixq = 90 * 258;
        gd.chance = st->chance;
        gd.gknob = st->gknob;
        gd.eper = gd.beat / 2u;                 /* assumed: the decision clock is a 1/8 note (upstream: the clock engine's first division) */
        gd.side = md == GM_EVR;
        gd.v[0].active = 1;
    }
    if (plate) {
        chi_clear(&chi[0]);
        chi_params(&chi[0], CH_KNOB, 0.616);   /* the loop gain that gave HALL's tail at SIZE 90 in revcompare */
    }
}
static int32_t gs_clips, gs_peak;
/* the model's return for one block: left / right wet (plate added when plate) */
static void gs_wet(int src, int md, const gset_t *st, int plate, uint32_t t, const int32_t *in, int32_t *wl, int32_t *wr)
{
    uint32_t i;
    (void)st;
    if (md <= GM_PPM) {
        static int32_t zero[CTL];
        song.g[G_DMODE] = DMD_PING;
        for (i = 0; i < CTL; i++) wet[i] = 0;
        fx_buses(zero, in, zero, wet, CTL);
        for (i = 0; i < CTL; i++) {
            wl[i] = wet[i] + dly_sd[i];
            wr[i] = wet[i] - dly_sd[i];
        }
    } else {
        if (md == GM_LOCK && gd.lstate == 0 && t >= gs_lock_on[src] && t < gs_lock_off[src] && !gd.lcnt) { gd.lstate = 1; gd.lcnt = 256; }
        if (md == GM_LOCK && gd.lstate == 2 && t >= gs_lock_off[src]) { gd.lstate = 3; gd.lcnt = 0; }
        gd_run(md == GM_GRN ? 1 : 0, in, wl, wr, CTL);
    }
    if (plate) {
        static int32_t pin[CTL], pl[CTL], pr[CTL];
        for (i = 0; i < CTL; i++) pin[i] = (wl[i] + wr[i]) / 2;
        chi_run(&chi[0], pin, pl, pr, CTL);
        for (i = 0; i < CTL; i++) {
            wl[i] += pl[i];
            wr[i] += pr[i];
        }
        gs_clips = (int32_t)chi[0].clips;
        gs_peak = chi[0].peak;
    }
}
/* one render; writes a WAV when f; returns the wet's energy */
static double gs_render(int src, int md, const gset_t *st, int plate, int32_t gq, FILE *f)
{
    static int32_t wl[CTL], wr[CTL], in[CTL];
    uint32_t t, i;
    int32_t mg = (int32_t)song.master_q12;
    double e = 0;
    if (gc_src != src) { FILE *n = fopen("/dev/null", "w"); gs_capture(src, n); fclose(n); }
    gs_model_reset(src, md < 0 ? GM_PP : md, st, plate);
    if (f) wav_hdr(f, GN);
    for (t = 0; t < GN; t += CTL) {
        for (i = 0; i < CTL; i++) { in[i] = gc_d[t + i]; wl[i] = wr[i] = 0; }
        if (md >= 0) gs_wet(src, md, st, plate, t, in, wl, wr);
        for (i = 0; i < CTL; i++) {
            int32_t a = (wl[i] * gq) >> 10, b = (wr[i] * gq) >> 10, l, r;
            e += (double)a * a + (double)b * b;
            l = (((gc_l[t + i] + a) >> 2) * mg) >> 10;
            r = (((gc_r[t + i] + b) >> 2) * mg) >> 10;
            master_out(&l, &r);
            if (f) wav_put(f, l, r);
        }
    }
    return e;
}

/* ---------------------------------------------------------------------------------------------- cost */
static double gs_cost(int md, const gset_t *st, int plate)
{
    static int32_t in[CTL], wl[CTL], wr[CTL];
    uint32_t b, i, nb = 3u * FS / CTL;
    uint64_t i0;
    host_tracks_init();
    gs_model_reset(1, md, st, plate);
    for (b = 0; b < 400; b++) {                  /* warm up: fill the line, let the voices and grains run */
        for (i = 0; i < CTL; i++) in[i] = noise(12000);
        gs_wet(1, md, st, plate, 0, in, wl, wr);
    }
    for (i = 0; i < CTL; i++) in[i] = noise(12000);
    i0 = instr_now();
    for (b = 0; b < nb; b++) gs_wet(1, md, st, plate, 0, in, wl, wr);
    return i0 ? (double)(instr_now() - i0) / (nb * CTL) : 0;
}
static double gs_cost_rev_only(void)
{
    static int32_t in[CTL], wl[CTL];
    uint32_t b, i, nb = 3u * FS / CTL;
    uint64_t i0;
    gs_ref_reset();
    for (b = 0; b < 40; b++) { for (i = 0; i < CTL; i++) in[i] = noise(12000); for (i = 0; i < CTL; i++) wl[i] = 0; rev_run(0, in, wl, CTL); }
    i0 = instr_now();
    for (b = 0; b < nb; b++) { for (i = 0; i < CTL; i++) wl[i] = 0; rev_run(0, in, wl, CTL); }
    return i0 ? (double)(instr_now() - i0) / (nb * CTL) : 0;
}
/* the plain delay (PING off, DEPTH 0) through fx_buses, for the baseline */
static double gs_cost_ref(int pp, int dep)
{
    static int32_t in[CTL], wl[CTL], zero[CTL];
    uint32_t b, i, nb = 3u * FS / CTL;
    uint64_t i0;
    host_tracks_init();
    song.g[G_DTIME] = 1; song.g[G_DFDBK] = 60; song.g[G_DCOLOR] = 70; song.g[G_DMIX] = 90;
    song.g[G_DMODE] = (int16_t)(pp ? DMD_PING : DMD_DIGITAL); song.g[G_DMDEP] = (int16_t)dep; song.g[G_DMRATE] = 44;
    gs_ref_reset();
    for (b = 0; b < 400; b++) { for (i = 0; i < CTL; i++) { in[i] = noise(12000); wl[i] = 0; } fx_buses(zero, in, zero, wl, CTL); }
    for (i = 0; i < CTL; i++) in[i] = noise(12000);
    i0 = instr_now();
    for (b = 0; b < nb; b++) { for (i = 0; i < CTL; i++) wl[i] = 0; fx_buses(zero, in, zero, wl, CTL); }
    return i0 ? (double)(instr_now() - i0) / (nb * CTL) : 0;
}

/* ---------------------------------------------------------------------------------------------- main */
#ifndef GD_NO_MAIN
int main(int argc, char **argv)
{
    char path[600], dir[500];
    FILE *rp, *ix;
    int s, m, c, pl;
    snprintf(dir, sizeof dir, "%s", argc > 1 ? argv[1] : "build/graindelay");
    mkdir(dir, 0755);
    snprintf(path, sizeof path, "%s/report.txt", dir);
    rp = fopen(path, "w");
    if (!rp) return 1;
#define OUT(...) do { printf(__VA_ARGS__); fprintf(rp, __VA_ARGS__); } while (0)
    gd_tables();
    chi_setup(&chi[0], 0, 1.0, chi_mem_full);
    OUT("graindelay: FS %u, CTL %u; line %u int16 = %u bytes (fx.c dly_buf: %u); sizeof(gd_t) = %u bytes; tables %u bytes\n",
        (unsigned)FS, (unsigned)CTL, (unsigned)GD_N, (unsigned)(GD_N * 2u), (unsigned)(DLY_LEN * 2u), (unsigned)sizeof(gd_t), (unsigned)sizeof(GD_QSIN) + (unsigned)sizeof(GD_HSIN));
    {
        const double lk[4][2] = {{2.6, 8.0}, {2.5, 8.0}, {2.5, 8.0}, {2.4, 8.0}};
        for (s = 0; s < 4; s++) { gs_lock_on[s] = (uint32_t)(lk[s][0] * FS); gs_lock_off[s] = (uint32_t)(lk[s][1] * FS); }
    }
    ix = NULL;
    {
        char ip[600];
        snprintf(ip, sizeof ip, "%s/index.html", dir);
        ix = fopen(ip, "w");
    }
    fprintf(ix, "<!doctype html><meta charset=utf-8><title>DUELL delay: ping-pong against a random-event / granular delay</title>"
                "<style>body{font:15px sans-serif;max-width:78em;margin:1em auto}td,th{padding:3px 8px;text-align:left;vertical-align:middle}"
                "h2{margin-top:1.6em}audio{width:250px;height:30px}.n{color:#555;font-size:13px}</style>"
                "<h1>DUELL ping-pong delay against a random-event / granular delay</h1>"
                "<p>Host renders, 44.1 kHz stereo 16-bit, integer rendition on a 65536-sample int16 line (the size of DUELL's delay line, 1.49 s). "
                "Every source goes through the same delay send; the delay time is the same note value and the feedback the same setting in every row. "
                "<b>The wet level of every file is matched to the ping-pong's in its column</b> (same wet energy), so you hear character, not loudness. "
                "Settings: <b>light</b> = FDBK 40, low randomness; <b>medium</b> = FDBK 60 (DUELL's default), medium randomness; <b>heavy</b> = FDBK 100, full randomness.</p>"
                "<p><b>Listen first:</b> pluck, medium: PINGPONG, then EVENTS_RND, then GRAINS. Then pad heavy (GRAINS), then beat medium (EVENTS_UP and EVENTS_RND), then the +plate section.</p>"
                "<ul class=n>");
    for (m = 0; m < GM_N; m++) fprintf(ix, "<li><b>%s</b>: %s</li>", GM_NAME[m], GM_DESC[m]);
    fprintf(ix, "</ul><p class=n>Random events: the randomness is fixed per file (the same seed every render), so a file is repeatable; in the device it would differ every time.</p>");
    for (pl = 0; pl < 2; pl++) {
        if (pl) fprintf(ix, "<h1>With a plate reverb after the delay</h1><p>The delay's return goes into the integer Rings / Clouds plate (loop gain fitted to HALL's tail at SIZE 90), at full rate. "
                            "Same rows as above; the wet level (delay plus plate) is matched to the ping-pong's again.</p>");
        for (s = 0; s < 4; s++) {
            fprintf(ix, "<h2>%s%s</h2><p>%s</p><table><tr><th></th><th>light</th><th>medium</th><th>heavy</th></tr>", GS[s].tag, pl ? " + plate" : "", GS[s].what);
            if (!pl) fprintf(ix, "<tr><td>dry</td><td><audio controls preload=none src=\"%s_dry.wav\"></audio></td><td></td><td></td></tr>", GS[s].tag);
            for (m = 0; m < GM_N; m++) {
                if (pl && m == GM_LOCK) continue;
                fprintf(ix, "<tr><td><b>%s</b></td>", GM_NAME[m]);
                for (c = 0; c < 3; c++) {
                    if (m == GM_LOCK && c != 1) { fprintf(ix, "<td></td>"); continue; }
                    fprintf(ix, "<td><audio controls preload=none src=\"%s_%s_%s%s.wav\"></audio></td>", GS[s].tag, SETS[c].name, GM_NAME[m], pl ? "_plate" : "");
                }
                fprintf(ix, "</tr>");
            }
            fprintf(ix, "</table>");
        }
    }
    for (s = 0; s < 4; s++) {
        FILE *f;
        double e0;
        gs_capture(s, rp);
        snprintf(path, sizeof path, "%s/%s_dry.wav", dir, GS[s].tag);
        f = fopen(path, "wb");
        gs_render(s, -1, &SETS[0], 0, 1024, f);
        fclose(f);
        for (pl = 0; pl < 2; pl++)
            for (c = 0; c < 3; c++) {
                double en[GM_N];
                for (m = 0; m < GM_N; m++) {
                    if (m == GM_LOCK && (c != 1 || pl)) continue;
                    en[m] = gs_render(s, m, &SETS[c], pl, 1024, NULL);
                    if (pl && gs_clips) OUT("graindelay: WARNING plate clips %d (%s %s %s)\n", (int)gs_clips, GS[s].tag, SETS[c].name, GM_NAME[m]);
                }
                OUT("graindelay: %s %s%s: wet before matching vs PINGPONG:", GS[s].tag, SETS[c].name, pl ? " +plate" : "");
                for (m = 0; m < GM_N; m++) {
                    double g, db;
                    int32_t gq;
                    if (m == GM_LOCK && (c != 1 || pl)) continue;
                    db = 10 * log10((en[m] + 1e-9) / (en[GM_PP] + 1e-9));
                    g = sqrt(en[GM_PP] / (en[m] + 1e-9));
                    g = g < 0.02 ? 0.02 : g > 30 ? 30 : g;
                    gq = (int32_t)(g * 1024.0 + 0.5);
                    OUT(" %s %+.1f dB%s", GM_NAME[m], db, m == GM_N - 1 ? "\n" : ",");
                    snprintf(path, sizeof path, "%s/%s_%s_%s%s.wav", dir, GS[s].tag, SETS[c].name, GM_NAME[m], pl ? "_plate" : "");
                    f = fopen(path, "wb");
                    e0 = gs_render(s, m, &SETS[c], pl, gq, f);
                    fclose(f);
                    (void)e0;
                }
                if (m == GM_N) OUT("\n");
            }
    }
    /* cost */
    {
        double rev = gs_cost_rev_only(), pp = gs_cost_ref(1, 0), ppm = gs_cost_ref(1, 60), plain = gs_cost_ref(0, 0), plainm = gs_cost_ref(0, 60);
        double plate;
        OUT("graindelay: cost, host instructions / sample (device estimate 1.7 %% CPU per 100, +-30 %%); the fx_buses figures include the chorus bus, the ROOM reverb on silence (%.0f alone) and the block loop:\n", rev);
        OUT("graindelay:   fx_buses total, delay plain (ROOM running)        %6.0f   minus the reverb alone: %5.0f\n", plain, plain - rev);
        OUT("graindelay:   fx_buses total, delay + modulation                %6.0f   minus the reverb alone: %5.0f\n", plainm, plainm - rev);
        OUT("graindelay:   fx_buses total, PING-PONG                         %6.0f   minus the reverb alone: %5.0f\n", pp, pp - rev);
        OUT("graindelay:   fx_buses total, PING-PONG + modulation            %6.0f   minus the reverb alone: %5.0f\n", ppm, ppm - rev);
        for (c = 0; c < 3; c++) {
            for (m = GM_EVL; m <= GM_GRN; m++) {
                double x = gs_cost(m, &SETS[c], 0);
                OUT("graindelay:   %-12s %-6s (the delay alone, no reverb)       %6.0f  ~%.1f %%   %+.0f vs PING-PONG's own (%.0f)\n", GM_NAME[m], SETS[c].name, x, x * 0.017, x - (pp - rev), pp - rev);
            }
        }
        plate = gs_cost(GM_EVL, &SETS[1], 1) - gs_cost(GM_EVL, &SETS[1], 0);
        OUT("graindelay:   plate reverb after it (integer, full rate)        %6.0f  ~%.1f %%\n", plate, plate * 0.017);
    }
    fprintf(ix, "<p><a href=report.txt>report.txt</a> (levels, cost)</p>");
    fclose(ix);
    fclose(rp);
    printf("graindelay: wrote %s\n", dir);
    return 0;
}
#endif
