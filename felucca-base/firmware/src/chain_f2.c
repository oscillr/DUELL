/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The chain's filter 2 and DRIVE (cloud job B's stub, filled by job E: TWIN_FILTER_DESIGN.md, TWIN_FILTER_DESIGN_2.md 2 and 3).
 * chain_f2_on: 1 when filter 2 runs for voice v this block (TYPE not OFF); chain_f2: the stage, called by voice_chain.c chain_stages after filter 1 on the voice's scratch block tmp (n samples, the
 * engine's signal at full level, before the amp), m the voice's modulation, uc filter 1's effective cutoff (Q8 dial steps). The stage keeps its state per voice (start from rest when v->chn ==
 * VCH_NEW, or when TYPE comes on while the voice is in the chain).
 *
 * TYPES (P_TWTYPE; TT_* in params.c). LP, BP, HP, NOTCH are TRIO's filter as eng_trio.c trio_render has it: the Simper SVF of dsp.c with an input that saturates above half scale (16000 of TRIO's
 * own scale), a band-pass state squeezed above +-49152, the damping 2.0 .. 0.25 over RES 0..127 (12 dB, never sings), and TRIO's slow random cutoff drift, from a generator of its own per voice
 * (TRIO's sequence came from the pitched-noise oscillator's: gone by design). The chain's scratch holds the engine at the voice's level (VOICE_FS 24000 for a full oscillator), TRIO's filter works
 * on 0.683 of it (a full TRIO voice peaks at 48000 here, 33000 there): in and out of the filter the signal is scaled by that, so the knees sit where TRIO's sit. LADDER: 24 dB, four trapezoidal
 * one-poles with the feedback and the input saturated by one tanh (bounded at any RES; the passband falls as 1 / (1 + k): the bass thins; it sings from RES ~120). COMB: tuned to the note (a
 * feedback delay of one period, damped by a one-pole in the loop, FEEDBACK -64..+63 = -0.95 .. +0.94, so the loop gain stays under 1), its lines in the part's engine memory (below)
 * Every type ends in a soft knee (TRIO's 16000, 48000 for LADDER and COMB as filter 1's UF_KNEE) so the output is bounded whatever the input and the settings */
#define F2_DSLEW 32                                     /* DRIVE moves at most this much a block (127 in 4 blocks, 3 ms) */
#define F2_BSLEW 16                                     /* BLEND moves at most this much a block (-64 to +63 in 8 blocks) */
#define F2_FADE 8u                                      /* blocks a TYPE / ROUTE / LINK switch takes (6 ms) */
typedef struct { int32_t s[5]; } f2k_t;                 /* a type's state: SVF ic1, ic2; LADDER s1..s4 and the last output; COMB the loop's low-pass, the write index, the samples since the start, the high-pass */
typedef struct {                                        /* one voice's filter 2 (the arrays below: v->chn VCH_NEW starts it) */
    f2k_t k, o;                                         /* the running type's state; the outgoing type's while it fades */
    int32_t drift, nst;                                 /* TRIO's cutoff drift (table steps Q8) and its generator */
    int32_t cl, cf, dly;                                /* the cutoff of the last block (Q8 dial steps), where a LINK switch's glide starts, the comb's delay of the last block (Q8 samples) */
    uint8_t ty, oty, fade, par, dv, lk, lg, dk;         /* TYPE run last block, the outgoing; fade blocks left; ROUTE weight 0..8; DRIVE; LINK; its glide's blocks left; what dry holds */
    int8_t bv;                                          /* BLEND of the last block */
    uint8_t rsv;
    uint16_t dcp;                                       /* DRIVE's level factor for dv (F2_COMP's law), 0 = not worked out */
} f2s_t;
static f2s_t f2s[NLIVE][NVOICE];   /* 72 B a voice, 1,152 B: ordinary RAM (.bss), not the pool (pool-tail rule: wave 5; it was in the pool in waves 3 and 4) */
static inline f2s_t *f2_st(const track_t *t, const voice_t *v) { return &f2s[PART_OF(t)][(uint32_t)(v - t->v)]; }

/* TYPE (P_TWTYPE) as the stage runs it: 0 OFF .. 6 COMB (params.c TT_*). A stored value out of range reads OFF; COMB on WAVE and PHYS (no engine memory for its lines: TWIN_FILTER_DESIGN 4.3) falls
 * back to LP at the same cutoff */
static inline uint32_t f2_ty(const track_t *t)
{
    uint32_t ty = (uint32_t)t->p[P_TWTYPE];
    if (ty > (uint32_t)TT_COMB)
        return TT_OFF;
    if (ty == (uint32_t)TT_COMB && (t->engine == ENGI_WAVE || t->engine == ENGI_PHYS))
        return TT_LP;
    return ty;
}
/* filter 2 runs for voice v this block: TYPE not OFF. OFF is the old path, bit for bit: chain_on is 0 (unless the SUB LAYER runs) and the voice never enters the chain. A voice that is in the chain
 * when TYPE goes to OFF stays in it until filter 2 has faded out, then leaves it by the one-block cross-fade of voice_chain.c chain_block (the engine and the amp are then the same signal) */
__attribute__((noinline)) int chain_f2_on(track_t *t, voice_t *v)
{
    const f2s_t *s;
    if (f2_ty(t) != TT_OFF)
        return 1;
    s = f2_st(t, v);
    return v->chn == VCH_RUN && (s->ty || s->fade);     /* TYPE went to OFF while the voice sounds: it stays in the chain until filter 2 has faded out (8 blocks); a voice on the plain path has no state (the rows are stale there) */
}

/* ----------------------------------------------------------------------------------------- the types --- */
/* LP BP HP NOTCH (mode 0..3 = TRIO's MODE): idx the cutoff as the SVF table index (Q8, drift in), k the damping (Q12) */
static __attribute__((noinline)) void f2_svf(f2k_t *s, const int32_t *in, int32_t *out, uint32_t n, int32_t idx, int32_t k, uint32_t mode)
{
    tsvf_t f;
    int32_t ic1 = s->s[0], ic2 = s->s[1], ma = TRIO_MIX[mode][0], mb = TRIO_MIX[mode][1], mc = TRIO_MIX[mode][2], mba, dmb;
    uint32_t i;
    if (mode >= 2u)
        mb = -(k >> 2);                                 /* HP / notch: in - k bp (- lp) */
    mba = s->s[2] << 5;                                 /* (k * bp in the output moves in a line from last block's k: a step of RES would step the output) */
    dmb = mb - s->s[2];
    tsvf_coef_k(&f, idx, k);
    for (i = 0; i < n; i++) {
        int32_t x = soft_knee(mulq15(in[i], 22369), 16000) >> 1;   /* (the input saturates above half scale) */
        int32_t v3 = x - ic2, v1 = (f.a1 * ic1 + f.a2 * v3 + 4096) >> 13, v2 = ic2 + ((f.a2 * ic1 + f.a3 * v3 + 4096) >> 13), a, y;   /* (rounded: a truncated shift leaves a DC offset of 0.5 / g) */
        ic1 = 2 * v1 - ic1;
        ic2 = clamp(2 * v2 - ic2, -150000, 150000);
        a = clamp(ic1, -49152, 49152);                  /* beyond +-49152 a quarter of the excess */
        ic1 = a + ((ic1 - a) >> 2);
        mba += dmb;
        y = soft_knee((x * ma + v1 * (mba >> 5) + v2 * mc) >> 10, 16000);
        out[i] = (y * 3000) >> 10;                      /* back to the chain's scale (2.93: trio_render's voice_amp(y << 1) << 1 at unity) */
    }
    s->s[0] = ic1;
    s->s[1] = ic2;
    s->s[2] = mb;
}
/* the SVF table's g (Q12, tan of the cutoff) at the table index idx (Q8), as tsvf_coef_k reads it */
static inline int32_t f2_g(int32_t idx)
{
    int32_t i, g;
    idx = clamp(idx, 0, 127 << 8);
    i = idx >> 8;
    g = SVF_G[i];
    if (i < 127)
        g += ((SVF_G[i + 1] - g) * (idx & 255)) >> 8;
    return g;
}
/* LADDER: G the one-pole's coefficient g / (1 + g) (Q16), kf the feedback (Q12, 0 .. 4.4): the loop is one sample late, tanh on the sum that enters the cascade bounds it. The stages keep 6 more
 * fractional bits (Q6) and round: at 30 Hz a stage moves by 1 / 500 of its error a sample, and a truncated Q0 state would stick (and leave a DC offset of 0.5 / G) */
static __attribute__((noinline)) void f2_lad(f2k_t *s, const int32_t *in, int32_t *out, uint32_t n, int32_t G, int32_t kf)
{
    int32_t s1 = s->s[0], s2 = s->s[1], s3 = s->s[2], s4 = s->s[3], y4 = s->s[4];
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t u = softclip((in[i] >> 1) - ((kf * (y4 >> 6)) >> 12)) << 6, v, y1, y2, y3;
        v = (int32_t)(((int64_t)(u - s1) * G + 32768) >> 16);
        y1 = v + s1;
        s1 = y1 + v;
        v = (int32_t)(((int64_t)(y1 - s2) * G + 32768) >> 16);
        y2 = v + s2;
        s2 = y2 + v;
        v = (int32_t)(((int64_t)(y2 - s3) * G + 32768) >> 16);
        y3 = v + s3;
        s3 = y3 + v;
        v = (int32_t)(((int64_t)(y3 - s4) * G + 32768) >> 16);
        y4 = v + s4;
        {   /* clamp to +-2^22 by magnitude: a literal -2^22 is 0xFFC00000, which the build's ROM-address check reads as a pointer into the chip ROM */
            int32_t neg = y4 < 0, a = neg ? -y4 : y4;
            if (a > 65536 * 64) a = 65536 * 64;
            y4 = neg ? -a : a;
        }
        s4 = y4 + v;
        out[i] = soft_knee(((y4 + 32) >> 6) << 1, UF_KNEE);
    }
    s->s[0] = s1;
    s->s[1] = s2;
    s->s[2] = s3;
    s->s[3] = s4;
    s->s[4] = y4;
}
/* COMB: the lines. A part's engine memory is empty on every engine but FM6, WAVE and PHYS (engines.c): each voice gets a line of (ENG_MEM_PART - base) / 16 samples (805 from 0, about 55 Hz), base the
 * end of FM6's notes while FM6's state is in the region. WAVE's and PHYS's state is never used by a comb (they run LP instead); a region still holding theirs from an engine switch is wiped first */
static int16_t *f2_line(const track_t *t, const voice_t *v, uint32_t *len)
{
    uint32_t pi = PART_OF(t), base = eng_mem_owner[pi] == ENGI_FM6 + 1u ? (ENG_FM6.mem + 3u) & ~3u : 0u, l = (ENG_MEM_PART - base) / (2u * NVOICE);
    *len = l;
    return (int16_t *)((uint8_t *)ENG_MEM(pi) + base) + (uint32_t)(v - t->v) * l;
}
/* dly the delay in Q8 samples (this block's end; the last block's value in s->dly is where it starts), fb the feedback (Q12, +-3900). The line starts empty, so the first echo would arrive as a step
 * whatever the signal is doing: the feedback is 0 until one delay has passed since the start and grows to its value over the next delay (s->s[2] counts the samples), and moves in a line from last
 * block's to this block's. The output is the knee of the loop's sum (the line holds the same, half) at 1 - 0.7 |feedback| of it */
static __attribute__((noinline)) void f2_comb(f2k_t *s, int16_t *line, uint32_t len, const int32_t *in, int32_t *out, uint32_t n, int32_t d0, int32_t d1, int32_t fb)
{
    int32_t lp = s->s[0], wr = s->s[1], t0 = s->s[2], dcs = s->s[3], fb0 = s->s[4], da = d0 << 5, dd = d1 - d0, dr = d1 >> 8 ? d1 >> 8 : 1, inv = 65536 / dr, tend = 2 * dr;
    int32_t fe = fb, nm = 4096 - ((((fb < 0 ? -fb : fb) * 2867) >> 12));
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t d, rp, ix, ix1, del, y, ys;
        if (t0 < tend || fb0 != fb) {                   /* (a start, or the feedback moving: not the steady case) */
            fe = fb0 + (((fb - fb0) * (int32_t)(i + 1u)) >> 5);
            if (t0 < tend) {
                fe = t0 > dr ? (fe * (((t0 - dr) * inv) >> 8)) >> 8 : 0;
                t0++;
            }
            nm = 4096 - ((((fe < 0 ? -fe : fe) * 2867) >> 12));
        }
        da += dd;
        d = da >> 5;
        rp = (wr << 8) - d;
        if (rp < 0)
            rp += (int32_t)len << 8;
        ix = rp >> 8;
        ix1 = ix + 1 == (int32_t)len ? 0 : ix + 1;
        del = (line[ix] + (((line[ix1] - line[ix]) * (rp & 255)) >> 8)) << 1;
        lp += ((del - lp) * 3) >> 2;                    /* the loop's damping */
        dcs += ((lp << 12) - dcs) >> 11;                /* .. and a high-pass at 3.4 Hz: a feedback comb has a gain of 1 / (1 - feedback) at 0 Hz, 16 at the most, and the line's clip would make an offset of it */
        y = in[i] + ((fe * (lp - ((dcs + 2048) >> 12))) >> 12);
        ys = soft_knee(y, UF_KNEE);
        line[wr] = (int16_t)((ys + 1) >> 1);            /* (the line holds half: 64383 / 2 at the most) */
        if (++wr == (int32_t)len)
            wr = 0;
        out[i] = (ys * nm) >> 12;
    }
    s->s[0] = lp;
    s->s[1] = wr;
    s->s[2] = t0;
    s->s[3] = dcs;
    s->s[4] = fb;
}

/* the comb's delay (Q8 samples) for voice v: one period of the note moved by tune16 (1/16 semitone), less the loop's own delay (the damping one-pole: 1/3 sample); an octave up for a note the line
 * cannot hold (below about 55 Hz) */
static int32_t f2_dly(const vmod_t *m, int32_t tune16, uint32_t len)
{
    uint32_t inc = pitch_inc(clamp(m->pitch16 + tune16, 0, 2047)), d;
    if (m->fine)
        inc += (uint32_t)((int32_t)(inc >> 12) * m->fine);
    d = 0xFFFFFFFFu / ((inc >> 8) | 1u);
    while (d > ((len - 2u) << 8))
        d >>= 1;
    return d > 512u + 85u ? (int32_t)d - 85 : 512;
}

/* a type starts from rest, charged with its input x (its first sample): no step. SVF: the low-pass at the input, LADDER: the cascade at the DC solution of its loop, COMB: an empty line */
static void f2_start(const track_t *t, const voice_t *v, f2s_t *s, f2k_t *k, uint32_t ty, int32_t x, int32_t res)
{
    uint32_t i;
    for (i = 0; i < 5u; i++)
        k->s[i] = 0;
    if (ty >= TT_LP && ty <= TT_NOTCH) {
        k->s[1] = soft_knee(mulq15(x, 22369), 16000) >> 1;
        k->s[2] = ty >= TT_HP ? -((8192 - clamp(res, 0, 127) * 7168 / 127) >> 2) : TRIO_MIX[ty - TT_LP][1];   /* (k * bp's weight in the output, where it starts) */
        s->drift = 0;
        if (!s->nst)
            s->nst = 0x2468ACE + (int32_t)v->age;
    } else if (ty == TT_LADDER) {
        int32_t y = ((x >> 1) * 4096) / (4096 + clamp(res, 0, 127) * 142);
        k->s[0] = k->s[1] = k->s[2] = k->s[3] = k->s[4] = y << 6;
    } else if (ty == TT_COMB) {
        uint32_t pi = PART_OF(t), len;
        int16_t *line;
        if (eng_mem_owner[pi] == ENGI_WAVE + 1u || eng_mem_owner[pi] == ENGI_PHYS + 1u) {
            memset(eng_mem[pi].b, 0, ENG_MEM_PART);
            eng_mem_owner[pi] = 0;
        }
        line = f2_line(t, v, &len);
        memset(line, 0, len * 2u);
        s->dly = 0;
    }
}

/* ----------------------------------------------------------------------------------------- DRIVE --- */
/* DRIVE d 0..127 (TWIN_FILTER_DESIGN 2.5): 0 is not run. SOFT, the curve of ANALOG's DRV and SAT and TAPE's SAT (fx.c ins_tape_sat): tanh of the signal times the gain g (Q12, 1x .. 8.9x, 4096 + 2 d^2),
 * the level kept by g^-0.75 (c, Q12: fx.c ins_tape_comp's law); 1 .. 11 blend the dry and the driven signal as SAT does since 0.2.14 (dry + (wet - dry) x d / 12; w, Q12). g, c and w glide
 * in a line from last block's d0 to this block's d1: no zipper, and 0 -> 1 starts from the dry signal */
static uint32_t ins_isqrt(uint32_t v);                  /* (fx.c) */
static int32_t f2_comp(uint32_t d)                      /* 4096 x gain^-0.75 of g = 4096 + 2 d^2 (fx.c ins_tape_comp's law): two integer roots, worked out when d changes (the state keeps the last) */
{
    uint32_t q, q4;
    if (!d)
        return 4096;
    q = ins_isqrt((4096u + d * d * 2u) << 16);          /* 256 x sqrt(g) */
    q4 = ins_isqrt(q << 8);                             /* 256 x g^0.25 */
    return (int32_t)(0x80000000u / ((q * q4) >> 6));
}
#ifdef DUELL_TWIN_COUNT                                 /* (tests/twin_test.c: how often DRIVE runs; DRIVE 0 must not) */
static uint32_t twin_drive_runs;
#define TWIN_COUNT_DRIVE() twin_drive_runs++
#else
#define TWIN_COUNT_DRIVE() ((void)0)
#endif
/* c0: the level factor of d0 as the state kept it (0: not known); returns that of d1 */
static __attribute__((noinline)) int32_t f2_drv(int32_t *b, uint32_t n, uint32_t d0, uint32_t d1, int32_t c0)
{
    int32_t g0 = 4096 + (int32_t)(d0 * d0 * 2u), g1 = 4096 + (int32_t)(d1 * d1 * 2u), c1, w0 = d0 < 12u ? (int32_t)((d0 * 5461u) >> 4) : 4096, w1 = d1 < 12u ? (int32_t)((d1 * 5461u) >> 4) : 4096;
    int32_t ga, ca, wa, dg, dc, dw;
    uint32_t i;
    TWIN_COUNT_DRIVE();
    c0 = c0 ? c0 : f2_comp(d0);
    c1 = d1 == d0 ? c0 : f2_comp(d1);
    ga = g0 << 5;
    ca = c0 << 5;
    wa = w0 << 5;
    dg = g1 - g0;
    dc = c1 - c0;
    dw = w1 - w0;
    for (i = 0; i < n; i++) {
        int32_t x = b[i], wet;
        ga += dg;
        ca += dc;
        wa += dw;
        wet = (softclip(((clamp(x, -57000, 57000) >> 1) * (ga >> 5)) >> 12) * (ca >> 5)) >> 11;
        b[i] = x + (((wet - x) * (wa >> 5)) >> 12);
    }
    return c1;
}

/* ------------------------------------------------------------------------------------------ the plan --- */
/* a block's decisions for voice v, made the same way by chain_f2_pre and chain_f2 (the state is committed at the end of chain_f2): ty the TYPE; fresh filter 2 starts now (it was off); par0 .. par1 the
 * ROUTE's weight over the block (0 SERIES .. F2_FADE PARALLEL, one step a block: a ROUTE switch slides the signals over F2_FADE blocks); d0 .. d1 DRIVE and b0 .. b1 BLEND, last block's to this
 * block's (they glide in a line over the block). TYPE OFF is a route of 0, DRIVE 0 and BLEND 0 */
typedef struct {
    uint32_t ty, fresh, par0, par1, d0, d1;
    int32_t b0, b1, res, cab, tune, dc0, dc1;
} f2q_t;
static __attribute__((noinline)) void f2_plan(const track_t *t, const f2s_t *s, f2q_t *q)
{
    uint32_t pt;
    q->ty = f2_ty(t);
    q->fresh = !s->ty && q->ty;
    pt = q->ty && (XV(t, X_ROUTE) & 1) ? F2_FADE : 0u;
    q->par0 = q->fresh ? pt : s->par;
    q->par1 = q->par0 < pt ? q->par0 + 1u : q->par0 > pt ? q->par0 - 1u : q->par0;
    q->d1 = q->ty ? (uint32_t)clamp(t->p[P_DRV2], 0, 127) : 0u;
    q->d0 = q->fresh ? q->d1 : s->dv;
    q->dc0 = q->fresh ? 0 : s->dcp;
    q->dc1 = q->dc0;
    q->d1 = (uint32_t)((int32_t)q->d0 + clamp((int32_t)q->d1 - (int32_t)q->d0, -F2_DSLEW, F2_DSLEW));   /* (a jump takes up to 4 blocks: the curve is not linear, a slope that stops at a block's end is heard) */
    if (q->d1 != q->d0)
        q->dc1 = 0;
    q->b1 = q->ty ? clamp(t->p[P_BLEND], -64, 63) : 0;
    q->b0 = q->fresh ? q->b1 : s->bv;
    q->b1 = q->b0 + clamp(q->b1 - q->b0, -F2_BSLEW, F2_BSLEW);   /* (the mix of two different signals: a jump is crossfaded over 8 blocks, a line a block) */
    q->res = t->p[P_RES2];
}
/* the ROUTE's weight at sample i of the block (Q8, 0 .. 256) */
static inline int32_t f2_wp(const f2q_t *q, uint32_t i)
{
    return (int32_t)(q->par0 * 32u) + (q->par1 > q->par0 ? (int32_t)(i + 1u) : q->par1 < q->par0 ? -(int32_t)(i + 1u) : 0);
}
/* BLEND at sample i of the block, in 1/32 (bq, -2048 .. 2016) and the weights it gives (Q12): bsw the unfiltered input's share of filter 2's input (SERIES, BLEND above 0), bnw filter 1's share of
 * the output (SERIES, BLEND below 0), wpw filter 2's share (PARALLEL) */
static inline int32_t f2_bq(const f2q_t *q, uint32_t i) { return (q->b0 << 5) + (q->b1 - q->b0) * (int32_t)(i + 1u); }

/* before filter 1 (voice_chain.c chain_stages): dry gets the signal that filter 2 needs unfiltered, and in PARALLEL (or while the ROUTE slides to or from it) DRIVE acts on the signal that enters
 * filter 1: tmp. dry holds (s->dk): 1 the engine's signal as it came (SERIES with BLEND above 0; the ROUTE sliding), 2 the driven signal (PARALLEL: both filters get it), 0 nothing */
__attribute__((noinline)) void chain_f2_pre(track_t *t, voice_t *v, int32_t *tmp, int32_t *dry, uint32_t n)
{
    f2s_t *s = f2_st(t, v);
    f2q_t q;
    uint32_t i;
    if (v->chn == VCH_NEW) {
        s->ty = TT_OFF;
        s->fade = 0;
    }
    f2_plan(t, s, &q);
    s->dk = 0;
    if (!q.ty && !s->ty && !s->fade)
        return;
    if (q.par0 == F2_FADE && q.par1 == F2_FADE) {
        if (q.d0 | q.d1) {
            int32_t c = f2_drv(tmp, n, q.d0, q.d1, q.dc0);
            if (q.d0 == q.d1)
                s->dcp = (uint16_t)c;                   /* (steady: the factor is kept for the next block; chain_f2 has no DRIVE of its own here) */
        }
        for (i = 0; i < n; i++)
            dry[i] = tmp[i];
        s->dk = 2;
    } else if (q.par0 | q.par1) {
        for (i = 0; i < n; i++)
            dry[i] = tmp[i];
        s->dk = 1;
        if (q.d0 | q.d1) {
            f2_drv(tmp, n, q.d0, q.d1, q.dc0);
            for (i = 0; i < n; i++)
                tmp[i] = dry[i] + (((tmp[i] - dry[i]) * f2_wp(&q, i)) >> 8);
        }
    } else if (q.b0 > 0 || q.b1 > 0) {
        for (i = 0; i < n; i++)
            dry[i] = tmp[i];
        s->dk = 1;
    }
}

/* ------------------------------------------------------------------------------------------ the cutoff --- */
/* filter 2's cutoff for voice v, per note and block (TWIN_FILTER_DESIGN_2 3.1), in Q8 dial steps like filter 1's (voice.c uf_run, one step 0.11 octave), clamped to the dial:
 *   LINK on   uc (filter 1's effective cutoff: its knob, its four depths, KEY TRK, the matrix's CUT; the accent's lift where filter 1 does not take it: every engine but ANALOG, TRIO and NOISE) + OFFSET (whole semitones, TW_OFF_Q8)
 *   LINK off  CUTOFF 2 as a dial
 *   + KEY TRK 2 (voice.c's UF_KT) + ENV 2, LFO 1, LFO 2 (the same forms as track_render's FLT depths) + the matrix's CUTOFF 2 (mod.c mod_f2_voice)
 * A LINK switch (the stored CUTOFF 2 changes its meaning with it) glides the cutoff from the last block's over F2_FADE blocks. q->tune: COMB's TUNE in 1/16 semitone (CUTOFF 2 is semitones around the
 * note there; LINK and KEY TRK 2 are not read; ENV 2 and LFO 1 / 2 and the matrix move it by their dial steps x 1.32 semitones) */
static __attribute__((noinline)) void f2_cut(track_t *t, voice_t *v, const vmod_t *m, int32_t uc, f2q_t *q, f2s_t *s)
{
    const int16_t *p = t->p;
    const mod_win_t *mod = &MODW(t);
    int32_t ms = ((v->env2 >> 9) * DEP_AMT(XV(t, X_ENV2))) >> 7, lf = mulq15(t->lfo_val, t->lfo_fade), c, ac, link = XV(t, X_LINK) != 0;
    if (mod->on && mod->ldep < 32767)
        lf = mulq15(lf, mod->ldep);
    ms += (lf * DEP_AMT(XV(t, X_LFO1))) >> 7;
    if (XV(t, X_LFO2)) {
        lf = mulq15(t->lfo2.val, t->lfo2.fade);
        if (mod->on && mod->ldep2 < 32767)
            lf = mulq15(lf, mod->ldep2);
        ms += (lf * DEP_AMT(XV(t, X_LFO2))) >> 7;
    }
    if (mod->on)
        ms += mod_f2_voice(t, v);
    q->tune = clamp((clamp(p[P_CUT2], -24, 24) << 4) + ((ms * 2711) >> 15), -768, 768);
    ac = v->vel > 110 ? (m->envq15 * 24) >> 7 : 0;      /* the accent's lift: on ANALOG, TRIO and NOISE (ACCENT_BOTH) filter 1's uc has it already and filter 2 takes it even with LINK off (cloud job J); on the other engines only filter 2 with LINK on */
    c = link ? uc + TW_OFF_Q8(clamp(p[P_CUT2], -TW_OFF_MAX, TW_OFF_MAX)) + (ACCENT_BOTH(ENGINES[t->engine]) ? 0 : ac) : clamp(p[P_CUT2], 0, UF_STEPS) * 256 + (ACCENT_BOTH(ENGINES[t->engine]) ? ac : 0);
    if (XV(t, X_KTRK2))
        c += UF_KT(XV(t, X_KTRK2), v->pitch_cur);
    c = clamp(c + ms, 0, UF_TOP);
    if (q->fresh) {
        s->lg = 0;
    } else if (s->lk != link && q->ty != TT_COMB) {
        s->lg = F2_FADE;
        s->cf = s->cl;
    }
    s->lk = (uint8_t)link;
    if (s->lg) {
        c = s->cf + (((c - s->cf) * (int32_t)(F2_FADE + 1u - s->lg)) >> 3);
        s->lg--;
    }
    s->cl = c;
    q->cab = c;
}

/* ------------------------------------------------------------------------------------------ the stage --- */
/* one type on the block: in to out (they may be the same buffer) */
static void f2_go(const track_t *t, const voice_t *v, const vmod_t *m, f2s_t *s, f2k_t *k, uint32_t ty, const f2q_t *q, const int32_t *in, int32_t *out, uint32_t n)
{
    if (ty == TT_COMB) {
        uint32_t len;
        int16_t *line = f2_line(t, v, &len);
        int32_t d1 = f2_dly(m, q->tune, len);
        f2_comb(k, line, len, in, out, n, s->dly ? s->dly : d1, d1, clamp(q->res, -64, 63) * 61);
        s->dly = d1;
    } else if (ty == TT_LADDER) {
        int32_t g = f2_g(40 + ((q->cab * 99) >> 6));
        f2_lad(k, in, out, n, (g << 16) / (4096 + g), clamp(q->res, 0, 127) * 142);
    } else if (ty) {
        s->drift += (((int32_t)(noise32(&s->nst) >> 22) - 512) - s->drift) >> 3;   /* the cutoff wanders a little, once per block (trio_render) */
        f2_svf(k, in, out, n, clamp(40 + ((q->cab * 99) >> 6) + s->drift, 0, 127 << 8), 8192 - clamp(q->res, 0, 127) * 7168 / 127, ty - TT_LP);
    } else if (in != out) {
        uint32_t i;
        for (i = 0; i < n; i++)
            out[i] = in[i];
    }
}
/* filter 2 on the block (tmp: filter 1's output, or the engine's while filter 1 is bypassed; dry: chain_f2_pre's), and the DRIVE, ROUTE and BLEND around it:
 *   SERIES    f2 in = tmp slid towards the unfiltered input by BLEND above 0, then DRIVE; out = f2, slid towards tmp (filter 1) by BLEND below 0
 *   PARALLEL  f2 in = the driven input (dry); out = tmp (filter 1) and f2 mixed, BLEND -64 filter 1 alone, 0 about equal, +63 filter 2 alone
 *   sliding   both, mixed by the ROUTE's weight (a ROUTE switch: F2_FADE blocks)
 * A TYPE switch while the voice sounds (a hand on TYPE, the MORPH fader's middle, a ROUTE / LINK / TYPE of a WILD end): the new type starts from rest charged with its input and the old one runs on
 * with its own state for F2_FADE blocks, mixed out in a line over the 256 samples (6 ms). TYPE OFF is the identity, so a switch to OFF fades filter 2 out and one from OFF is a fresh start */
__attribute__((noinline)) void chain_f2(track_t *t, voice_t *v, int32_t *tmp, int32_t *dry, uint32_t n, const vmod_t *m, int32_t uc)
{
    f2s_t *s = f2_st(t, v);
    f2q_t q;
    int32_t a[CTL], b[CTL];
    const int32_t *in;
    uint32_t i, sers, para, bl, plain;
    if (v->chn == VCH_NEW) {
        s->ty = TT_OFF;
        s->fade = 0;
    }
    f2_plan(t, s, &q);
    if (!q.ty && !s->ty && !s->fade)
        return;
    if (q.fresh) {
        s->fade = 0;                                    /* (a fade still running from a TYPE that went to OFF: dropped) */
        if (v->chn == VCH_RUN) {                        /* the voice is in the chain already (CHAIN_ALL: filter 1 runs): a start from OFF fades in from the identity over F2_FADE blocks, as a TYPE switch does */
            s->oty = TT_OFF;
            s->fade = F2_FADE;
        }
    }
    f2_cut(t, v, m, uc, &q, s);
    sers = !(q.par0 | q.par1);
    para = q.par0 == F2_FADE && q.par1 == F2_FADE;
    bl = (uint32_t)(q.b0 | q.b1);
    if (s->ty && q.ty != s->ty && !s->fade) {           /* a switch: the old type's state is kept for the fade */
        s->o = s->k;
        s->oty = s->ty;
        s->fade = F2_FADE;
    }
    plain = sers && !bl && !s->fade;
    if (plain) {                                        /* the usual case: DRIVE and filter 2 in place on filter 1's output */
        if (q.d0 | q.d1)
            q.dc1 = f2_drv(tmp, n, q.d0, q.d1, q.dc0);
        in = tmp;
    } else if (para) {
        in = dry;
    } else {
        for (i = 0; i < n; i++) {
            int32_t bq = f2_bq(&q, i);
            a[i] = s->dk && bq > 0 ? tmp[i] + (((dry[i] - tmp[i]) * ((bq * 2080) >> 10)) >> 12) : tmp[i];
        }
        if (q.d0 | q.d1)
            q.dc1 = f2_drv(a, n, q.d0, q.d1, q.dc0);
        if (!sers) {                                    /* sliding: the driven input slides in */
            for (i = 0; i < n; i++)
                b[i] = dry[i];
            if (q.d0 | q.d1)
                f2_drv(b, n, q.d0, q.d1, q.dc0);
            for (i = 0; i < n; i++)
                a[i] += ((b[i] - a[i]) * f2_wp(&q, i)) >> 8;
        }
        in = a;
    }
    if (s->ty != q.ty && q.ty)
        f2_start(t, v, s, &s->k, q.ty, in[0], q.res);
    f2_go(t, v, m, s, &s->k, q.ty, &q, in, plain ? tmp : b, n);
    if (s->fade) {                                      /* the outgoing type, on the same input, mixed out */
        uint32_t base = (F2_FADE - s->fade) * 32u;
        f2_go(t, v, m, s, &s->o, s->oty, &q, in, a, n);   /* (a fade is never the plain case: the input is a, which the new type has read, or dry) */
        for (i = 0; i < n; i++)
            b[i] = a[i] + (((b[i] - a[i]) * (int32_t)(base + i + 1u)) >> 8);
        s->fade--;
    }
    if (!plain) {
        if (para) {
            for (i = 0; i < n; i++)
                tmp[i] += ((b[i] - tmp[i]) * (((f2_bq(&q, i) + 2048) * 1032) >> 10)) >> 12;
        } else if (sers) {
            for (i = 0; i < n; i++) {
                int32_t bq = f2_bq(&q, i);
                tmp[i] = bq < 0 ? b[i] + (((tmp[i] - b[i]) * (-bq * 2)) >> 12) : b[i];
            }
        } else {
            for (i = 0; i < n; i++) {
                int32_t y1 = tmp[i], y2 = b[i], bq = f2_bq(&q, i), os = bq < 0 ? y2 + (((y1 - y2) * (-bq * 2)) >> 12) : y2, op = y1 + (((y2 - y1) * (((bq + 2048) * 1032) >> 10)) >> 12);
                tmp[i] = os + (((op - os) * f2_wp(&q, i)) >> 8);
            }
        }
    }
    s->ty = (uint8_t)q.ty;
    s->par = (uint8_t)q.par1;
    s->dv = (uint8_t)q.d1;
    s->dcp = (uint16_t)(q.d0 | q.d1 ? q.dc1 : 4096);
    s->bv = (int8_t)q.b1;
}

/* ------------------------------------------------------------------------------- for the pages (job H) --- */
/* TWIN is on: TYPE is not OFF (the pages' hide rule: only the TYPE card shows otherwise) */
int twin_on(const track_t *t) { return f2_ty(t) != TT_OFF; }
/* LINK on or off, keeping the sound: CUTOFF 2 changes meaning with it (OFFSET -36..+36 whole semitones from filter 1's knob, or a dial 0..82), so the stored value is converted using filter 1's knob
 * (a dial step is 1.324 semitones: the conversion rounds to the nearest whole value either way). On COMB (TUNE: LINK is not read) the switch alone. The converted value is clamped to the new range */
void twin_link_set(track_t *t, int on)
{
    int32_t knob = clamp(t->p[P_FREEA], 0, UF_STEPS), c = t->p[P_CUT2];
    on = on != 0;
    if (on != (XV(t, X_LINK) != 0) && t->p[P_TWTYPE] != TT_COMB)
        t->p[P_CUT2] = (int16_t)(on ? clamp(TW_STEPS_OFF(c - knob), -TW_OFF_MAX, TW_OFF_MAX) : clamp(knob + TW_OFF_STEPS(clamp(c, -TW_OFF_MAX, TW_OFF_MAX)), 0, UF_STEPS));
    XV(t, X_LINK) = (int16_t)on;
}
/* TYPE (TT_*): refuses COMB on WAVE and PHYS (nothing is changed); entering or leaving COMB gives RES 2 (RES / FEEDBACK) and CUTOFF 2 (OFFSET or CUTOFF / TUNE) the new kind's defaults, so a RES of 0
 * never becomes the strongest negative feedback and a TUNE never reads as an offset */
void twin_type_set(track_t *t, int type)
{
    if (type < TT_OFF || type > TT_COMB || (type == TT_COMB && (t->eng_req == ENGI_WAVE || t->eng_req == ENGI_PHYS)))
        return;
    if ((t->p[P_TWTYPE] == TT_COMB) != (type == TT_COMB)) {
        t->p[P_RES2] = 0;
        t->p[P_CUT2] = (int16_t)(type == TT_COMB || XV(t, X_LINK) ? 0 : UF_STEPS);
    }
    t->p[P_TWTYPE] = (int16_t)type;
}
