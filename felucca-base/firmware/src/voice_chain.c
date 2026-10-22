/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The VOICE CHAIN's sequence (cloud job B; voice_chain.h says what it is). Included by voice.c before track_render, after uf_run / uf_voice / uf_edge, which it uses as they are.
 *
 *   chain_voice  one voice's block through the whole chain:
 *                  1. a cleared scratch block; the engine rendered into it with the amp ramp at UNITY (amp0 = amp1 = 32767, Q15 full scale): every engine scales its own signal by the ramp
 *                     (dsp.c amp_at / voice_amp, PHASE's stepped ramp, WAVE's gain pair, FM6's a / da), so the scratch holds the signal at full level. FM6 owns its envelope (ownenv): its
 *                     ramp in track_render already is the envelope's 32767 times the LAYER share, the LFO -> AMP and the engine-switch fade, which the chain applies in step 5 as for the others
 *                  2. chain_sub (the SUB LAYER, job F), on the scratch; chain_f2_pre (job E): in PARALLEL DRIVE acts here, before both filters, and the unfiltered signal is kept for filter 2
 *                  3. filter 1, the multimode filter, uf_run, as track_render has it: not run while bypassed (MODE LP and the effective cutoff at the open end), and the bypass edge cross-faded
 *                     over the block as uf_edge does it (v->uft)
 *                  4. chain_f2 (filter 2 and DRIVE, job E)
 *                  5. the real amp ramp, m->amp0 to m->amp1 (dsp.c amp_at), per sample
 *                  6. added into dst (the part's block, or the SPREAD buffer)
 *   chain_block  what track_render calls for a voice that is in the chain this block, or was last block: the chain alone, or - on the one block where the voice enters or leaves it - the
 *                plain path (the plain path and the chain made from one engine render), cross-faded
 * The stage hooks run on the voice's own state; nothing here is shared by the two parts (the scratch blocks are on the stack) */
#include "voice_chain.h"
#include "chain_f2.c"
#include "chain_sub.c"

/* filter 1 of the chain on the scratch block (see 3): the bypass rules and the edge cross-fades of track_render's `if (uf)` block, on tmp instead of the engine's render */
static __attribute__((noinline)) void chain_f1(const engine_t *e, track_t *t, voice_t *v, int32_t *tmp, uint32_t n, int32_t uc)
{
    int32_t dry[CTL];
    uint32_t i;
    if (!e->ufilt)
        return;
    if (uc < UF_TOP || t->p[P_FREEC] > 0) {             /* the filter runs */
        if (v->uft == UF_BYP) {                         /* out of the bypass while sounding: from rest, faded in over the block */
            for (i = 0; i < n; i++)
                dry[i] = tmp[i];
            v->uft = 0;
            uf_run(v, tmp, n, uc, t->p);
            for (i = 0; i < n; i++)
                tmp[i] = dry[i] + (tmp[i] - dry[i]) * (int32_t)(i + 1u) / (int32_t)n;
        } else {
            uf_run(v, tmp, n, uc, t->p);
        }
        return;
    }
    if (v->uft && v->uft != UF_BYP) {                   /* into the bypass while sounding: the filter's last block fades to dry */
        for (i = 0; i < n; i++)
            dry[i] = tmp[i];
        uf_run(v, tmp, n, UF_TOP, t->p);
        for (i = 0; i < n; i++)
            tmp[i] = dry[i] + (tmp[i] - dry[i]) * (int32_t)(n - 1u - i) / (int32_t)n;
    }
    v->uft = UF_BYP;
}

/* the real amp ramp on a unity-level sample: NOISE's make-up puts the raw block above 65536 (its shift 2^sh is put back after the engine's own ramp, eng_noise.c), which mulq15's int32 product cannot take; mulq16 can */
static inline int32_t chain_amp(int32_t x, int32_t a)
{
    return x > 65535 || x < -65535 ? mulq16(x, (uint32_t)a << 1) : mulq15(x, a);
}

/* the stages of the chain on a raw (unity-level) block: tmp is changed, then added to dst through the real amp ramp */
static __attribute__((noinline)) void chain_stages(const engine_t *e, track_t *t, voice_t *v, int32_t *tmp, int32_t *dst, uint32_t n, const vmod_t *m, int32_t uc)
{
    int32_t dry[CTL];                                   /* (filter 2's unfiltered input: chain_f2_pre fills it when SERIES with BLEND, PARALLEL or a ROUTE switch need it) */
    uint32_t i;
    chain_sub(t, v, tmp, n, m);
    chain_f2_pre(t, v, tmp, dry, n);
    chain_f1(e, t, v, tmp, n, uc);
    chain_f2(t, v, tmp, dry, n, m, uc);
    for (i = 0; i < n; i++)
        dst[i] += chain_amp(tmp[i], amp_at(m, i));       /* the real amp ramp */
}

/* chain_voice and chain_block are static (one translation unit with voice.c): an external function's n is unknown to the compiler, and then uf_run, which has one more caller here, would no longer
 * be built for the constant block length CTL (+28 B of the hot block on the device: uf_run 1,048 -> 1,074 B, uf_voice +4; found by the merge of A and B with the device build) */
/* the engine at full level into a cleared block */
static __attribute__((noinline)) void chain_raw(const engine_t *e, track_t *t, voice_t *v, int32_t *raw, uint32_t n, const vmod_t *m)
{
    vmod_t mu = *m;
    uint32_t i;
    for (i = 0; i < n; i++)
        raw[i] = 0;
    mu.amp0 = mu.amp1 = 32767;                          /* the engine at full level */
    e->render(t, v, raw, n, &mu);
}

static __attribute__((noinline)) void chain_voice(const engine_t *e, track_t *t, voice_t *v, int32_t *dst, uint32_t n, const vmod_t *m, int32_t uc)
{
    int32_t tmp[CTL];
    if (n > CTL)
        n = CTL;
    chain_raw(e, t, v, tmp, n, m);
    chain_stages(e, t, v, tmp, dst, n, m, uc);
}

/* filter 1's effective cutoff as track_render's `if (uf)` block makes it (the same lines), and what it does to the voice's modulation m on the way (uf 1: the cutoff routes are the filter's) */
static int32_t chain_uc(const engine_t *e, track_t *t, voice_t *v, vmod_t *m, int32_t ucut0, uint32_t msl)
{
    const int16_t *p = t->p;
    int32_t kt, uc;
    if (!e->ufilt)
        return UF_TOP;
    v->msl = (uint16_t)msl;
    kt = p[P_KTRK] ? UF_KT(p[P_KTRK], v->pitch_cur) : 0;
    uc = clamp(ucut0 + m->cutoff + kt, 0, UF_TOP);
    m->cutoff = v->vel > 110 ? (m->envq15 * 24) >> 7 : 0;
    m->ucut = uc;                                       /* (NOISE's loudness make-up reads it) */
    return uc;
}

/* CHAIN_ALL (cloud job J): 1 = every note whose filter 1 runs goes through the chain (engine at unity, SUB, filter 1, DRIVE, filter 2, then the amp) on every engine: the owner's decision of
 * 2026-10-19, TWIN_FILTER_DESIGN_2 question 3. 0 = only where TWIN / SUB is on (chain_on) or the engine lost its own filter (ACCENT_BOTH: ANALOG, TRIO, NOISE), so the helper can back out
 * in this one line if the renders show an audible change on the seven others */
#ifndef CHAIN_ALL                               /* (a test or the helper may build with -DCHAIN_ALL=0) */
#define CHAIN_ALL 1
#endif
/* voice v goes through the chain this block only because filter 1 runs for it: the engine has the multimode filter and its effective cutoff (the same sum chain_uc and track_render make:
 * the dial, the routes in m->cutoff, KEY TRK) is below the open end, or its MODE is not LP. m->cutoff is still the routes' sum here (chain_uc / track_render's `if (uf)` reset it after) */
static inline uint32_t chain_f1_on(const engine_t *e, const track_t *t, const voice_t *v, const vmod_t *m, int32_t ucut0)
{
    const int16_t *p = t->p;
    if (!e->ufilt || !(CHAIN_ALL || ACCENT_BOTH(e)))
        return 0;
    if (p[P_FREEC] > 0)
        return 1;
    return clamp(ucut0 + m->cutoff + (p[P_KTRK] ? UF_KT(p[P_KTRK], v->pitch_cur) : 0), 0, UF_TOP) < UF_TOP;
}

/* voice v this block: in the chain (it was last block, or it enters now). On the one block where it enters or leaves, the engine renders ONCE (at unity: the engine's own state moves once, whatever
 * it keeps outside the voice) and two paths are made from that block: the plain one (the amp, then filter 1 by the bypass rules) and the chain (its stages, then the amp), and the output goes
 * from one to the other over the block. A note that starts in the chain (v->chn VCH_FRESH) has no plain path to come from; the chain's stages start from rest (VCH_NEW). Leaves v->chn VCH_RUN,
 * or VCH_OFF after a block on the plain path. m is track_render's vmod_t for the voice (this function changes it as the plain path does) */
static __attribute__((noinline)) void chain_block(const engine_t *e, track_t *t, voice_t *v, int32_t *dst, uint32_t n, vmod_t *m, int32_t ucut0, uint32_t msl, uint32_t on)
{
    uint32_t was = v->chn, i;
    int32_t uc;
    if (n > CTL)
        n = CTL;
    uc = chain_uc(e, t, v, m, ucut0, msl);
    if (on && was != VCH_OFF) {                          /* in the chain already, or a note that starts in it */
        v->chn = was == VCH_RUN ? VCH_RUN : VCH_NEW;
        chain_voice(e, t, v, dst, n, m, uc);
        v->chn = VCH_RUN;
        return;
    }
    {   /* the edge */
        int32_t raw[CTL], pb[CTL], cb[CTL];
        chain_raw(e, t, v, raw, n, m);
        for (i = 0; i < n; i++) {
            pb[i] = chain_amp(raw[i], amp_at(m, i));
            cb[i] = 0;
        }
        if (on) {                                       /* in: the plain filter first (its state), then the chain from rest */
            chain_f1(e, t, v, pb, n, uc);
            v->uft = 0;
            v->chn = VCH_NEW;
            chain_stages(e, t, v, raw, cb, n, m, uc);
            v->chn = VCH_RUN;
            for (i = 0; i < n; i++)
                dst[i] += pb[i] + (cb[i] - pb[i]) * (int32_t)(i + 1u) / (int32_t)n;
        } else {                                        /* out: the chain's last block, then the plain filter from rest */
            chain_stages(e, t, v, raw, cb, n, m, uc);
            v->uft = 0;
            chain_f1(e, t, v, pb, n, uc);
            v->chn = VCH_OFF;
            for (i = 0; i < n; i++)
                dst[i] += cb[i] + (pb[i] - cb[i]) * (int32_t)(i + 1u) / (int32_t)n;
        }
    }
}
