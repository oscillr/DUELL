/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The chain's SUB LAYER (cloud job F, docs/design/SUB_AND_CYCLE_DESIGN.md A; the CYCLE source is cycle.c). Included by voice_chain.c, so voice.c's one translation unit.
 * chain_sub_on: 1 when the sub runs for voice v this block (SUB LEVEL above 0; in UNISON only voice 0 plays it); chain_sub: the stage, called by voice_chain.c right after the engine, on the voice's
 * scratch block tmp (n samples, the engine's signal at full level, before filter 1 and the amp), m the voice's modulation. The sub is added into tmp, so filter 1, filter 2 and DRIVE shape it, and the
 * amp ramp (the AMP ENV, velocity, LFO AMP, the matrix's AMP, LAYER XFADE, the steal's fade-out) is its envelope.
 *
 *   OCTAVE  -2 -1 0 (stored 0..2): one phase accumulator at m->inc >> 2 (the pitch the engine gets once a block: glide, bend, vibrato, master tune, every PITCH route); the octave's phase is the
 *           accumulator << 0 / 1 / 2, so every octave is locked to the others (a switch of OCTAVE keeps the phase)
 *   WAVE    SINE sine_i, TRI osc_tri (a quarter cycle on: its zero crossing where the sine's is), SQUARE osc_pulse (polyBLEP at the sub's own increment), PULSE the same at width 1/4 (x 4/3: its high level is a full-scale pulse)
 *   HARM    0..127: the same wave an octave above the sub mixed in (0 to 64: up to half the sub's level), then two octaves above (64 to 127: up to half); an octave above Nyquist is left out. 0 = not run
 *   LEVEL   0..127, 0 = not run. A curve (SUB_CURVE: the owner sets it by ear) to a gain in the engine's own units, 127 = a full-scale engine oscillator (VOICE_FS); the gain glides per sample
 *           from the block before's (no zipper); a sub that is silent and stays silent is not run
 *   UNISON  only voice 0 plays it, undetuned (its step is made from the pitch without the voice's detune v->fine), at the velocity of the key, not of the unison share: one centred sub a key
 *   FM6     owns its amplitude (ownenv: the AMP ENV is forced to full, the operators' envelopes are inside the engine's signal): the sub follows the loudest carrier's gain (fm6_sub_follow, eng_fm6.c)
 * A voice that enters the chain (v->chn VCH_NEW: a new note, or a voice on the plain path whose LEVEL went above 0) starts the phase at 0 and the gain at once (the amp ramp or chain_block's one-block cross-fade is its attack); a retrigger in place keeps both, as voice_start keeps the engine's oscillator phases. A change of
 * WAVE or OCTAVE while it sounds runs the old and the new together for SUB_XFB blocks, one fading out and the other in. State per voice, arrays [part][voice] (not in voice_t, whose layout the
 * hot functions address): the phase 4 B, the gliding gain 2 B, the wave and octave now, before, and the blocks of the cross-fade left 2 B */
#define SUB_XFB 4u                                      /* blocks of the cross-fade of a WAVE / OCTAVE switch (an engine choice's XF_BLOCKS) */
static uint32_t sub_ph[NLIVE][NVOICE];                  /* the accumulator (step m->inc >> 2) */
static uint16_t sub_g[NLIVE][NVOICE];                   /* the gain it had at the end of the block before (VOICE_FS units, 0..65535) */
static uint16_t sub_cfg[NLIVE][NVOICE];                 /* bits 0..3 the wave | octave << 2 in use, 4..7 the one fading out, 8..10 the blocks of the cross-fade left */
/* LEVEL -> gain, Q15: 17 points at LEVEL 0, 8 .. 128 (a LEVEL is mapped x + x / 64, so 127 is the last), straight lines between; one more at the end for the read. (x / 128) ^ 1.6: an estimate */
static const uint16_t SUB_CURVE[18] = {0, 388, 1176, 2250, 3566, 5096, 6822, 8730, 10809, 13051, 15447, 17992, 20680, 23505, 26464, 29553, 32768, 32768};

__attribute__((noinline)) int chain_sub_on(track_t *t, voice_t *v)
{
    return XV(t, X_SUBLVL) > 0 && (t->p[P_VOICE] != V_UNISON || v == t->v);
}
/* the gain of the sub for voice v in VOICE_FS units (0 = not run), without the FM6 follow */
static int32_t sub_gain(track_t *t, voice_t *v)
{
    int32_t lvl = XV(t, X_SUBLVL), x = lvl + (lvl >> 6), i = x >> 3, y, g;
    if (!chain_sub_on(t, v))
        return 0;
    y = SUB_CURVE[i] + (((SUB_CURVE[i + 1] - SUB_CURVE[i]) * (x & 7)) >> 3);
    g = (y * VOICE_FS) >> 15;
    if (t->p[P_VOICE] == V_UNISON && v->vel)            /* the key's velocity, not the unison share's (voice_start: vel * 36 / 100, or / 2) */
        g = g * v->mvel / v->vel;
    return g < 1 ? 1 : g > 65535 ? 65535 : g;
}
/* the sub's phase step: m->inc >> 2 (the accumulator; the octave shifts it back up); UNISON: from the pitch without this voice's detune */
static uint32_t sub_step(const track_t *t, const voice_t *v, const vmod_t *m)
{
    uint32_t inc = m->inc;
    if (t->p[P_VOICE] == V_UNISON) {
        int32_t f = m->fine - v->fine;
        inc = pitch_inc((uint32_t)clamp(m->pitch16, 0, 2047));
        if (f)
            inc += (uint32_t)((int32_t)(inc >> 12) * f);
    }
    return inc >> 2;
}
/* the wave cfg (WAVE | OCTAVE << 2) at the accumulator ph (step st) added into tmp with the gain gliding from g0 to g1 (VOICE_FS units; Q8 steps, the sample's share is (s * gain / 2) >> 14: s
 * reaches 2 x 32768 with both harmonics, gain / 2 at most 32767: 2^31 not passed). h1 / h2 (Q15) the octaves above */
#define SUB_LOOP(WAVE)                                                                      \
    for (i = 0; i < n; i++) {                                                               \
        int32_t s = WAVE(ph, inc);                                                          \
        if (h1)                                                                             \
            s += mulq15(WAVE(ph << 1, inc << 1), h1);                                       \
        if (h2)                                                                             \
            s += mulq15(WAVE(ph << 2, inc << 2), h2);                                       \
        gq += dg;                                                                           \
        tmp[i] += (s * (gq >> 9)) >> 14;                                                    \
        ph += inc;                                                                          \
    }
#define SUB_SIN(p, inc) sine_i(p)
#define SUB_TRI(p, inc) osc_tri((p) + 0x40000000u)     /* (a quarter cycle on: it starts at its zero crossing, as the sine does) */
#define SUB_PLS(p, inc) ((osc_pulse(p, inc, pw) * pk) >> 14)
static __attribute__((noinline)) void sub_pass(int32_t *tmp, uint32_t cfg, uint32_t ph, uint32_t st, int32_t g0, int32_t g1, int32_t h1, int32_t h2, uint32_t n)
{
    uint32_t o = cfg >> 2 & 3u, w = cfg & 3u, inc = st << o, i, pw = w == 3u ? 0x40000000u : 0x80000000u;
    int32_t gq = g0 << 8, dg = n ? ((g1 - g0) << 8) / (int32_t)n : 0, pk = w == 3u ? 21845 : 32768;
    ph <<= o;
    if (inc >= 0x40000000u)                             /* no octave above the Nyquist rate (the 2x step would wrap) */
        h1 = 0;
    if (inc >= 0x20000000u)
        h2 = 0;
    if (!w)
        SUB_LOOP(SUB_SIN)
    else if (w == 1u)
        SUB_LOOP(SUB_TRI)
    else
        SUB_LOOP(SUB_PLS)
}

__attribute__((noinline)) void chain_sub(track_t *t, voice_t *v, int32_t *tmp, uint32_t n, const vmod_t *m)
{
    uint32_t ti = (uint32_t)(t - trk), vi = (uint32_t)(v - t->v), cur, last, old, c, st;
    int32_t g0, g1, h1, h2;
    if (ti >= NLIVE || vi >= NVOICE)
        return;
    cur = ((uint32_t)XV(t, X_SUBWAVE) & 3u) | ((uint32_t)XV(t, X_SUBOCT) % 3u) << 2;
    g1 = sub_gain(t, v);
    if (g1 && ENGINES[t->engine] == &ENG_FM6)           /* FM6: the loudest carrier's level, 0..1 (Q15) */
        g1 = (g1 * fm6_sub_follow(t, v)) >> 15;                 /* (65535 x 32767 stays under 2^31) */
    if (v->chn == VCH_NEW) {                            /* the voice enters the chain: the phase from 0, the gain at once: the attack is the amp ramp of a new note, or the cross-fade of chain_block's edge for a voice that was on the plain path */
        sub_ph[ti][vi] = 0;
        sub_g[ti][vi] = (uint16_t)g1;
        sub_cfg[ti][vi] = (uint16_t)(cur | cur << 4);
    }
    g0 = sub_g[ti][vi];
    last = sub_cfg[ti][vi] & 15u;
    old = sub_cfg[ti][vi] >> 4 & 15u;
    c = sub_cfg[ti][vi] >> 8 & 7u;
    if (!c && cur != last) {                            /* WAVE or OCTAVE changed: the old runs on for SUB_XFB blocks, fading out (a silent sub just takes the new one) */
        old = last;
        last = cur;
        c = g0 ? SUB_XFB : 0;
    }
    if (!g0 && !g1 && !c) {                             /* silent and staying so: not run */
        sub_cfg[ti][vi] = (uint16_t)(last | old << 4);
        return;
    }
    st = sub_step(t, v, m);
    h1 = h2 = 0;                                        /* SUB HARMONICS is disabled (owner, 2026-10-21): the stored X_SUBHARM (id reserved) is never read, so no octave copies and no glitch */
    if (c) {                                            /* the old one's share falls c / SUB_XFB -> (c - 1) / SUB_XFB, the new one's rises to match */
        sub_pass(tmp, old, sub_ph[ti][vi], st, g0 * (int32_t)c / (int32_t)SUB_XFB, g1 * ((int32_t)c - 1) / (int32_t)SUB_XFB, h1, h2, n);
        sub_pass(tmp, last, sub_ph[ti][vi], st, g0 * ((int32_t)SUB_XFB - (int32_t)c) / (int32_t)SUB_XFB, g1 * ((int32_t)SUB_XFB + 1 - (int32_t)c) / (int32_t)SUB_XFB, h1, h2, n);
        c--;
    } else {
        sub_pass(tmp, last, sub_ph[ti][vi], st, g0, g1, h1, h2, n);
    }
    sub_ph[ti][vi] += st * n;
    sub_g[ti][vi] = (uint16_t)g1;
    sub_cfg[ti][vi] = (uint16_t)(last | old << 4 | c << 8);
}
