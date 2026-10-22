/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Phase distortion. The waveforms are a C port of the oscillator of
 * CrispyZebra (Leo Kuroshita, GPL-3.0, github.com/hugelton/CrispyZebra), the
 * author's own phase-distortion core: a -cos table read through a bent phase whose
 * bend is DCW. WAVE2 alternates with WAVE every other cycle. A second
 * line (DTN) can be mixed or ring-modulated (its own SUB, a sine an octave down, went in cloud job J: the global SUB LAYER, chain_sub.c, replaces it).
 * Envelopes are Felucca's own (ADSR, ENV -> DCW).
 *
 * The render is built for a core with no branch prediction (pi32v2): per voice and block the bends are set up once
 * (pd_setup: break points and Q16 slopes; a division only where a slope needs one), each line is then rendered into a
 * block buffer by a loop with no call, no switch and no data-dependent branch (a sample's segment is counted from three
 * compares), and one more loop mixes the lines and adds the amplitude. The numbers are the same as the
 * one-sample-at-a-time original's (tests/regress.c's goldens). */
static const char *const N_PD_WAVE[] = {"SAW", "SQR", "PLS", "DSIN", "SPLS", "RSAW", "RTRI", "RTRP"};
static const char *const N_PD_WAVE2[] = {"-", "SAW", "SQR", "PLS", "DSIN", "SPLS", "RSAW", "RTRI", "RTRP"};
static const char *const N_PD_LINE[] = {"MIX", "RING"};

/* -cos(2 pi a16 / 65536), Q15, from a 16-bit phase: the table (1024 points, a 1/4 turn in: sine_i of (a16 << 16) +
 * 0x40000000) read at the point (a16 + 0x4000) >> 6 and interpolated by the 6 bits below it (SINE[1024] = SINE[0] = 0) */
static inline int32_t pd_cos(uint32_t a16)
{
    uint32_t t = a16 + 0x4000u, i = (t >> 6) & 1023u;
    int32_t a = SINE[i], b = SINE[i + 1u];
    return -(a + (((b - a) * (int32_t)(t & 63u)) >> 6));
}

/* one wave's bend for a block. The bent phase of a 16-bit phase p is piecewise linear: up to four segments, told apart by
 * three break points b[] (a phase p is in segment (p >= b[0]) + (p >= b[1]) + (p >= b[2]); unused ones sit at 65536,
 * never reached), and in a segment s: off + ((p - st) * k >> 16), k the Q16 slope (0: a flat piece). A division only
 * for a slope of the block (the bent phase comes out the same or 1 lower in 65536 than a division per sample) */
typedef struct {
    uint32_t st, off, k, pad;
} pds_t;
typedef struct {
    uint32_t b[3];
    uint32_t w, rez, pad;                                    /* the wave; RESONANCE waves: the core's pitch rez */
    pds_t s[4];
} pdw_t;

static inline uint32_t pd_slope(uint32_t span, uint32_t len) { return (span << 16) / (len ? len : 1u); }
static inline void pd_seg(pdw_t *b, uint32_t i, uint32_t st, uint32_t off, uint32_t k)
{
    b->s[i].st = st;
    b->s[i].off = off;
    b->s[i].k = k;
}

static void pd_setup(pdw_t *b, uint32_t w, uint32_t dcw)
{
    uint32_t x, pk;
    b->w = w;
    b->b[0] = b->b[1] = b->b[2] = 65536u;
    switch (w) {
    case 0:                                                  /* SAW */
        x = 32768u + ((dcw * 32767u) >> 16);
        b->b[0] = x;
        pd_seg(b, 0, 0, 0, pd_slope(32768u, x));
        pd_seg(b, 1, x, 32768u, pd_slope(32767u, 65535u - x));
        break;
    case 1:                                                  /* SQUARE */
        x = 32768u - ((dcw * 31120u) >> 16);
        b->b[0] = x;
        b->b[1] = 32768u;
        b->b[2] = 32768u + x;
        pd_seg(b, 0, 0, 0, pd_slope(32768u, x));
        pd_seg(b, 1, 0, 32768u, 0);
        pd_seg(b, 2, 32768u, 32768u, pd_slope(32767u, x));
        pd_seg(b, 3, 0, 65535u, 0);
        break;
    case 2:                                                  /* PULSE */
        x = 65535u - ((dcw * 63487u) >> 16);
        b->b[0] = x;
        pd_seg(b, 0, 0, 0, pd_slope(65535u, x));
        pd_seg(b, 1, 0, 65535u, 0);
        break;
    case 3:                                                  /* DOUBLE SINE */
        x = 65535u - ((dcw * 49151u) >> 16);
        b->b[0] = x;
        pd_seg(b, 0, 0, 0, pd_slope(65535u, x));
        pd_seg(b, 1, x, 0, pd_slope(65535u, 65535u - x));
        break;
    case 4:                                                  /* SAW-PULSE */
        x = 65535u - ((dcw * 32767u) >> 16);
        pk = (x * (32768u + ((dcw * 29491u) >> 16))) >> 16;
        if (!pk)
            pk = 1;
        b->b[0] = pk;
        b->b[1] = x;
        pd_seg(b, 0, 0, 0, pd_slope(32768u, pk));
        pd_seg(b, 1, pk, 32768u, pd_slope(32767u, x - pk));
        pd_seg(b, 2, 0, 65535u, 0);
        break;
    default:                                                 /* RESONANCE */
        b->rez = (dcw * 44000u) >> 16;
        break;
    }
}

static inline uint32_t pd_bend1(const pdw_t *b, uint32_t p)     /* a wave with one break point (SAW, PULSE, DOUBLE SINE) */
{
    const pds_t *s = &b->s[p >= b->b[0]];
    return s->off + (((p - s->st) * s->k) >> 16);
}
static inline uint32_t pd_bend(const pdw_t *b, uint32_t p)
{
    const pds_t *s = &b->s[(p >= b->b[0]) + (p >= b->b[1]) + (p >= b->b[2])];
    return s->off + (((p - s->st) * s->k) >> 16);
}

/* RESONANCE: a windowed sine, rez = the pitch of the core; the window by wave (5 saw, 6 triangle, 7 trapezoid) */
static inline uint32_t pd_win(uint32_t w, uint32_t p)       /* (selects, no branches: all three are cheap) */
{
    uint32_t a = 65535u - p, m = p < a ? p : a, c = p << 2, r;
    c = p > 49152u ? a << 2 : c < 65535u ? c : 65535u;
    r = w == 6u ? m << 1 : c;
    return w == 5u ? a : r;
}
static inline int32_t pd_rez(const pdw_t *b, uint32_t p, uint32_t win)
{
    int32_t core = (pd_cos((p + ((p * b->rez) >> 16) * 7u) & 0xFFFFu) * 5) >> 3;
    return (core * (int32_t)win) >> 16;                      /* |core| <= 20479: fits 32 bits */
}

/* one sample at phase p (16 bit): bipolar Q15 (any wave: the generic one, used where the loops below do not apply) */
static inline __attribute__((always_inline)) int32_t pd_wave_i(const pdw_t *b, uint32_t p)
{
    if (b->w < 5u)
        return pd_cos(pd_bend(b, p));
    return pd_rez(b, p, pd_win(b->w, p));
}
static int32_t pd_wave(const pdw_t *b, uint32_t p) { return pd_wave_i(b, p); }

/* hunt4 (range X has 24 B free in the BENCHH4 build, 76 before): pd_sl and pd_pair are out of line (they were inlined 6 and 5 times; one copy each, called, is 160 B smaller), both in range X
 * (tools/ram_fns_x.py). They are called a few times a voice and block: no measurable cost.
 * pedal hunt: a stored DCW, ENV or SUB that jumps (the MORPH fader moved by several steps, a knob, a preset) moves at most `lim` a block, not at once: the bent wave and the
 * sub sine step otherwise (a click). st holds the value + 1 (0 = a new note: starts at the value); a value that stays is passed unchanged */
static __attribute__((noinline)) int32_t pd_sl(int32_t *st, int32_t tg, int32_t lim)
{
    int32_t c = *st - 1;
    if (!*st || (tg - c <= lim && c - tg <= lim))
        c = tg;
    else
        c += tg > c ? lim : -lim;
    *st = c + 1;
    return c;
}

static __attribute__((noinline)) int32_t pd_gap(const pdw_t *const *a, const pdw_t *const *b, uint32_t ph, uint32_t tg)   /* a's wave minus b's at the phase ph (16 bit in the top), the toggle tg picks WAVE / WAVE2 */
{
    return pd_wave(a[tg], ph >> 16) - pd_wave(b[tg], ph >> 16);
}
#ifndef PD_BRIDGE_MIN
#define PD_BRIDGE_MIN 128                                    /* of DCW's 0..56000: the attack of #31 moves it by 1,700 a block */
#endif
static void phase_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->s[2] = v->s[3] = v->s[4] = v->s[5] = 0;
    v->ph[1] = 0;
    v->s[0] = 0;                 /* WAVE / WAVE2 toggle, line 1 */
    v->s[1] = 0;                 /* line 2 */
}

/* One line (a phase accumulator ph, its step inc, the toggle tg that picks wp[0] / wp[1] every other cycle) into dst[n].
 * Branch-free per sample: the toggle flips when the phase wraps. Four loops by the kind of the two waves */
#define PD_LINE_BEGIN                                                                                              \
    uint32_t ph = *php, tg = *tgp, i;                                                                              \
    for (i = 0; i < n; i++) {                                                                                      \
        const pdw_t *w = wp[tg];                                                                                   \
        uint32_t p = ph >> 16, old = ph;
#define PD_LINE_END                                                                                                \
        ph += inc;                                                                                                 \
        tg ^= ph < old;                                                                                            \
    }                                                                                                              \
    *php = ph;                                                                                                     \
    *tgp = tg;

static void pd_line_bent1(const pdw_t *const *wp, uint32_t *php, uint32_t inc, uint32_t *tgp, int32_t *dst, uint32_t n)
{
    PD_LINE_BEGIN
    dst[i] = pd_cos(pd_bend1(w, p));
    PD_LINE_END
}

static void pd_line_bent(const pdw_t *const *wp, uint32_t *php, uint32_t inc, uint32_t *tgp, int32_t *dst, uint32_t n)
{
    PD_LINE_BEGIN
    dst[i] = pd_cos(pd_bend(w, p));
    PD_LINE_END
}

#define PD_LINE_REZ(NAME, WIN)                                                                                     \
    static void NAME(const pdw_t *const *wp, uint32_t *php, uint32_t inc, uint32_t *tgp, int32_t *dst, uint32_t n)  \
    {                                                                                                              \
        PD_LINE_BEGIN                                                                                              \
        dst[i] = pd_rez(w, p, WIN);                                                                                \
        PD_LINE_END                                                                                                \
    }
PD_LINE_REZ(pd_line_r5, 65535u - p)
PD_LINE_REZ(pd_line_r6, (p < 32768u ? p : 65535u - p) << 1)
PD_LINE_REZ(pd_line_r7, p > 49152u ? (65535u - p) << 2 : p < 16384u ? p << 2 : 65535u)
PD_LINE_REZ(pd_line_rx, pd_win(w->w, p))                    /* two RESONANCE waves with windows of their own */

static void pd_line_gen(const pdw_t *const *wp, uint32_t *php, uint32_t inc, uint32_t *tgp, int32_t *dst, uint32_t n)
{
    PD_LINE_BEGIN
    dst[i] = pd_wave_i(w, p);                                /* one bent, one RESONANCE */
    PD_LINE_END
}

static void pd_line(const pdw_t *const *wp, uint32_t *php, uint32_t inc, uint32_t *tgp, int32_t *dst, uint32_t n)
{
    uint32_t a = wp[0]->w, b = wp[1]->w;
    if (a < 5u && b < 5u) {
        if ((1u << a | 1u << b) & ~13u)                      /* (13: the waves 0, 2, 3 have one break point) */
            pd_line_bent(wp, php, inc, tgp, dst, n);
        else
            pd_line_bent1(wp, php, inc, tgp, dst, n);
    } else if (a < 5u || b < 5u)
        pd_line_gen(wp, php, inc, tgp, dst, n);
    else if (a != b)
        pd_line_rx(wp, php, inc, tgp, dst, n);
    else if (a == 5u)
        pd_line_r5(wp, php, inc, tgp, dst, n);
    else if (a == 6u)
        pd_line_r6(wp, php, inc, tgp, dst, n);
    else
        pd_line_r7(wp, php, inc, tgp, dst, n);
}

/* the lines (s1, s2) mixed (L 1) or ring-modulated (L 2) or line 1 alone (L 0), and the amplitude ramp (dsp.c amp_at, voice_amp, by adding the step instead of multiplying) */
#define PD_MIX(L)                                                                                                  \
    for (i = 0; i < n; i++) {                                                                                      \
        int32_t s = s1[i], amp;                                                                                    \
        if (L == 1)                                                                                                \
            s = (s + s2[i]) >> 1;                         /* both within +-32767 */                                \
        if (L == 2)                                                                                                \
            s = mulq15(s, s2[i]);                                                                                  \
        amp = m->amp0 + ((x + ((x >> 31) & (CTL - 1))) >> CTL_LOG2);                                              \
        x = (int32_t)((uint32_t)x + (uint32_t)d);                                                                  \
        out[i] += mulq15(mulq15(s, amp), VOICE_FS) << 1;                                                           \
    }

static void pd_mix(int32_t *out, const int32_t *s1, const int32_t *s2, uint32_t n, const vmod_t *m, uint32_t line)    /* line: 0 one, 1 mix, 2 ring */
{
    uint32_t i;
    int32_t x = 0, d = m->amp1 - m->amp0;
    if (line == 0)
        PD_MIX(0)
    else if (line == 1)
        PD_MIX(1)
    else
        PD_MIX(2)
}

/* the wave pair of a block: wp[0] is WAVE, wp[1] WAVE2 (the same bend as WAVE's when WAVE2 is off or the same wave) */
static __attribute__((noinline)) void pd_pair(const pdw_t **wp, pdw_t *b1, pdw_t *b2, uint32_t w1, uint32_t w2, uint32_t dcw)
{
    uint32_t wb = w2 ? w2 - 1u : w1;
    pd_setup(b1, w1, dcw);
    wp[0] = b1;
    if (wb == w1) {
        wp[1] = b1;
    } else {
        pd_setup(b2, wb, dcw);
        wp[1] = b2;
    }
}

/* DUELL MORPH (morph.c, core.h mxf): WAVE / WAVE2 between the two ends: each line reads both ends' bent waves at the same
 * phase and cross-fades them (WAVE2 "-" follows its end's WAVE). Only while a fade is under way: its own copy of the loop */
static __attribute__((noinline)) void phase_render_m(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t w1 = (uint32_t)p[P_E0] & 7u, w2 = (uint32_t)p[P_E1], i;
    uint32_t x1 = MXF(t).w[MX_E0] ? (uint32_t)MXF(t).b[MX_E0] & 7u : w1, x2 = MXF(t).w[MX_E0 + 1u] ? (uint32_t)MXF(t).b[MX_E0 + 1u] : w2;
    int32_t g1 = MXF(t).w[MX_E0], g2 = MXF(t).w[MX_E0 + 1u] ? MXF(t).w[MX_E0 + 1u] : !w2 && !x2 ? g1 : 0;   /* the WAVE2 slot's weight */
    int32_t depth = pd_sl(&v->s[2], (p[P_E2] << 8) + m->shape - (64 << 8) + 65536, 256) - 65536 + m->cutoff + mulq15(m->envq15, pd_sl(&v->s[3], p[P_E3] * 256, 256));
    uint32_t dcw, inc = m->inc, inc2;
    int32_t det = p[P_E4], line2 = det != 0 || p[P_E5], ring = p[P_E5];
    int32_t d16 = det * 16 / 100, rem = det * 16 - d16 * 100;
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1];
    int32_t tg0 = v->s[0], tg1 = v->s[1];
    pdw_t b1, b2, c1, c2;
    const pdw_t *bp[2], *cp[2];
    depth = clamp(depth, 0, 127 << 8);
    dcw = (uint32_t)depth * 65535u / (127u << 8);
    dcw = (dcw * 56000u) >> 16;
    v->s[5] = (int32_t)dcw + 1;                              /* (phase_render_0 bridges a DCW that moved since the last block: keep it current here too) */
    inc2 = pitch_inc(clamp(m->pitch16 + d16, 0, 2047));
    inc2 += (uint32_t)((int32_t)(inc2 >> 12) * (rem * 2367 / 16000));
    pd_pair(bp, &b1, &b2, w1, w2, dcw);
    pd_pair(cp, &c1, &c2, x1, x2, dcw);                      /* the other end's */
    for (i = 0; i < n; i++) {
        uint32_t old = ph0;
        int32_t s, g = tg0 ? g2 : g1;
        s = pd_wave(bp[tg0], ph0 >> 16);
        if (g)
            s += ((pd_wave(cp[tg0], ph0 >> 16) - s) * g) >> 15;
        ph0 += inc;
        if (ph0 < old)
            tg0 ^= 1;
        if (line2) {
            uint32_t old2 = ph1;
            int32_t s2 = pd_wave(bp[tg1], ph1 >> 16);
            g = tg1 ? g2 : g1;
            if (g)
                s2 += ((pd_wave(cp[tg1], ph1 >> 16) - s2) * g) >> 15;
            ph1 += inc2;
            if (ph1 < old2)
                tg1 ^= 1;
            s = ring ? mulq15(s, s2) : (s + s2) >> 1;
        }
        out[i] += voice_amp(s, m, i) << 1;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->s[0] = tg0;
    v->s[1] = tg1;
}

static void phase_render_0(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t w1 = (uint32_t)p[P_E0] & 7u, w2 = (uint32_t)p[P_E1];
    int32_t depth = pd_sl(&v->s[2], (p[P_E2] << 8) + m->shape - (64 << 8) + 65536, 256) - 65536 + m->cutoff + mulq15(m->envq15, pd_sl(&v->s[3], p[P_E3] * 256, 256));
    int32_t det = p[P_E4], ring = p[P_E5];
    uint32_t dcw, line = det != 0 || ring ? 1u + (uint32_t)(ring != 0) : 0u;   /* 0 one line, 1 mixed, 2 ring */
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], tg0 = (uint32_t)v->s[0], tg1 = (uint32_t)v->s[1];   /* WAVE / WAVE2 toggles */
    pdw_t b1, b2;
    const pdw_t *wp[2];
    int32_t s1[CTL], s2[CTL], c1 = 0, c2 = 0;
    uint32_t i;
    depth = clamp(depth, 0, 127 << 8);
    dcw = (uint32_t)depth * 65535u / (127u << 8);
    dcw = (dcw * 56000u) >> 16;                              /* the classic range of the bend */
    pd_pair(wp, &b1, &b2, w1, w2, dcw);                      /* WAVE2 (every other cycle) */
    if (v->s[5] && (uint32_t)(dcw - (uint32_t)v->s[5] + 1u + PD_BRIDGE_MIN) > 2u * PD_BRIDGE_MIN) {   /* DCW (the envelope's sweep) moved by more than PD_BRIDGE_MIN since the last block: the bend
                                                              * steps at the block seam. The old bend's value at this block's first phase minus the new one's is added to each line as an offset that dies away in a few
                                                              * samples (below), so the step is a short glide and the block-rate buzz (HUNT4.md) goes. A step under it (an LFO's slow sweep) is 22 dB under the attack's: left alone */
        pdw_t o1, o2;
        const pdw_t *ow[2];
        pd_pair(ow, &o1, &o2, w1, w2, (uint32_t)v->s[5] - 1u);
        c1 = pd_gap(ow, wp, ph0, tg0);
        if (line)
            c2 = pd_gap(ow, wp, ph1, tg1);
    }
    v->s[5] = (int32_t)dcw + 1;                              /* (+ 1: 0 is a new note, nothing to bridge) */
    pd_line(wp, &ph0, m->inc, &tg0, s1, n);
    if (line) {                                              /* the second line, DTN cents off (cents_inc, in 1/16 semitones + the rest) */
        uint32_t inc2;
        int32_t d16 = (det * 16 * 5243) >> 19, rem = det * 16 - d16 * 100;   /* (det 0..127: / 100 and / 16000 as multiplies) */
        inc2 = pitch_inc(clamp(m->pitch16 + d16, 0, 2047));
        inc2 += (uint32_t)((int32_t)(inc2 >> 12) * ((rem * 155132) >> 20));
        pd_line(wp, &ph1, inc2, &tg1, s2, n);
    }
    if (c1 | c2)
        for (i = 0; i < n; i++) {                            /* (8 samples a time constant: 1/8 of what is left goes each sample) */
            s1[i] += c1;
            c1 -= c1 >> 3;
            if (line) {
                s2[i] += c2;
                c2 -= c2 >> 3;
            }
        }
    pd_mix(out, s1, s2, n, m, line);
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->s[0] = (int32_t)tg0;
    v->s[1] = (int32_t)tg1;
}

static void phase_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    if (MXF(t).on && (MXF(t).w[MX_E0] | MXF(t).w[MX_E0 + 1u]))
        phase_render_m(t, v, out, n, m);                 /* DUELL MORPH: a WAVE / WAVE2 cross-fade */
    else
        phase_render_0(t, v, out, n, m);
}

static const preset_t PHASE_PRESETS[] = {
    /* name, {WAVE, WAVE2, DCW, ENV, DTN, LINE, -, -}, {A D S R}, fenv, mono (E7 was the engine's own SUB until cloud job J) */
    {"BRASS", {0, 0, 30, 90, 0, 0, 0, 0}, {8, 70, 90, 40}, 0, 0, FX(0, 20, 20, 40), PAT(6)},
    {"ORGAN", {3, 0, 40, 0, 0, 0, 0, 0}, {0, 127, 127, 30}, 0, 0, FX(0, 40, 0, 30), PAT(6)},
    {"STRING", {0, 4, 50, 40, 12, 0, 0, 0}, {40, 90, 100, 70}, 0, 0, FX(0, 50, 20, 60), PAT(5)},
    {"RESO", {5, 0, 60, 60, 0, 0, 0, 0}, {0, 70, 30, 60}, 0, 0, FX(0, 0, 40, 40), PAT(1)},
    {"BELL", {6, 0, 80, 50, 0, 0, 0, 0}, {0, 95, 0, 90}, 0, 0, FX(0, 0, 30, 70), PAT(7)},
    {"WIRE", {4, 7, 70, 40, 7, 0, 0, 0}, {10, 80, 80, 60}, 0, 0, FX(15, 30, 30, 40), PAT(4)},
};

static const engine_t ENG_PHASE = {
    .name = "PHASE",
    .page_title = {"PHS", "LINE"},
    .edit = {
        {"WAVE 1", F_ENUM, 0, 7, 0, N_PD_WAVE, 0},
        {"WAVE 2", F_ENUM, 0, 8, 0, N_PD_WAVE2, 0},
        {"DCW", F_PCT, 0, 127, 60, 0, 0},
        {"ENV", F_PCT, 0, 127, 64, 0, 0},
        {"DTN", F_INT, 0, 127, 0, 0, "ct"},
        {"LINE", F_ENUM, 0, 1, 0, N_PD_LINE, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},                 /* was SUB: the global SUB LAYER (chain_sub.c) replaces it */
        {"-", F_INT, 0, 0, 0, 0, 0},
    },
    .presets = PHASE_PRESETS,
    .npresets = NELEM(PHASE_PRESETS),
    .note_on = phase_note_on,
    .render = phase_render,
    .knob = {P_E2, P_E3, P_E4, P_REL},
    .ufilt = 1,                 /* no filter of its own: the multimode filter (DUELL 0.2.9; voice.c uf_run: CUT RES MODE SLOPE, P_FREEA..D) */
};
