/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* ANALOG: two band-limited oscillators (saw / square / tri / sine / PWM),
 * noise and drive. No filter of its own since the engine filters left (TWIN_FILTER_DESIGN_2, cloud job J): its VCF is filter 1, the MULTI FILTER of the voice chain (voice.c uf_run;
 * ANALOG's softer RES floor became filter 1's). DRV stays here: it drives the oscillator mix before any filter (the Minimoog's overdriven mixer).
 * WAVE SYNC and SUB (#104, appended: WAVE 0..4 play as they always did) are two more oscillator pairs (analog_osc_x):
 *   SYNC  OSC 2 a saw hard-synced to OSC 1 (a saw): it restarts with OSC 1's every cycle, band-limited at the restart
 *         (polyBLEP scaled by the step the restart makes). DTN (shown SYNC) sets OSC 2 above OSC 1, 3/16 semitone a
 *         step (127: +23.8 semitones), and the SHP routings sweep it (ENV DEST / LFO DEST SHP, the matrix's SHP: 3/16
 *         semitone a step of shape, as PWM takes them for its width): the sync sweep. MIX between the two as for
 *         every pair (OSC 2 alone at 127)
 *   SUB   OSC 1 a saw, OSC 2 a square an octave below it (detuned by DTN in cents as before): MIX (shown SUB) is the
 *         sub oscillator's level */
static const char *const N_ANALOG_WAVE[] = {"SAW", "SQR", "TRI", "SIN", "PWM", "SYNC", "SUB"};
enum { AW_SYNC = 5, AW_SUB = 6 };
static const param_desc_t ANALOG_SYNC = {"SYNC", F_INT, 0, 127, 10, 0, 0};   /* DTN's range and default */
static const param_desc_t ANALOG_SUB = {"SUB", F_PCT, 0, 127, 64, 0, 0};      /* MIX's */

static const param_desc_t *analog_desc(const track_t *t, uint32_t k)
{
    if (k == 1u && t->p[P_E0] == AW_SYNC)
        return &ANALOG_SYNC;
    if (k == 2u && t->p[P_E0] == AW_SUB)
        return &ANALOG_SUB;
    return 0;
}

static void analog_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->ph[1] = v->ph[0] + 0x40000000u;
    if (!v->s[2])
        v->s[2] = 0x1234567 + (int32_t)v->age;        /* noise state */
    v->s[3] = 0;                                      /* SYNC: no restart pending */
}

/* ARP TARGET (SPEC 2.3): OSC A (bit 0) and OSC B (bit 1) on the arp's pitch or the root's; the arp's gate on their mix
 * level. Not inlined: only with units masked */
static uint32_t atg_osc_b(const vmod_t *m)            /* OSC B at DTN 0: on OSC A's increment of its own pitch */
{
    return (m->amask & 2u) ? atg_inc(m) : m->inc;
}
static __attribute__((noinline)) void atg_analog(const vmod_t *m, int32_t det, uint32_t *inc1, uint32_t *inc2,
                                                 int32_t *m1, int32_t *m2)
{
    if (m->amask & 1u) {
        *inc1 = atg_inc(m);
        *m1 = mulq15(*m1, m->again);
    }
    *inc2 = det ? cents_inc(atg_pitch(m, 1), det, m->fine) : atg_osc_b(m);
    if (m->amask & 2u)
        *m2 = mulq15(*m2, m->again);
}

/* WAVE SYNC / SUB: OSC 1 a saw, OSC 2 the synced saw / the sub square; then noise and drive as
 * analog_render. Its own function: analog_render's loop stays what it was */
static __attribute__((noinline)) void analog_render_x(track_t *t, voice_t *v, int32_t *out, uint32_t n,
                                                      const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t sub = p[P_E0] == AW_SUB, i;
    int32_t det = p[P_E1], mix = p[P_E2], noise = p[P_E3];
    int32_t drive = 32768 + p[P_E6] * 512;
    int32_t m2 = mix * 258, m1 = 32767 - m2;
    int32_t nz = noise * 200, drv = p[P_E6];
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], inc1 = m->inc, inc2;
    int32_t nst = v->s[2];
    int32_t jl = v->s[3];                             /* SYNC: 1 + the step of a restart a sample ago (16 bits), 0 none */
    int32_t pb = m->pitch16;                          /* OSC B's pitch (ARP TARGET: the arp's, SPEC 2.3) */
    if (m->amask) {
        pb = atg_pitch(m, 1);
        atg_analog(m, 0, &inc1, &inc2, &m1, &m2);     /* (inc2: below) */
    }
    if (sub) {                                        /* an octave below OSC 1, DTN as OSC 2 always had it */
        inc2 = (det ? cents_inc(pb, det, m->fine) : atg_osc_b(m)) >> 1;
    } else {                                          /* DTN and the shape above OSC 1, 3/16 semitone a step */
        inc2 = pitch_inc(clamp(pb + det * 3 + (((m->shape - (64 << 8)) * 3) >> 8), 0, 2047));
        if (m->fine)
            inc2 += (uint32_t)((int32_t)(inc2 >> 12) * m->fine);
    }
    for (i = 0; i < n; i++) {
        int32_t a = osc_saw(ph0, inc1), b, s;
        if (sub) {
            b = osc_pulse(ph1, inc2, 0x80000000u);
        } else {
            int32_t bl = blep(ph1, inc2);
            if (jl) {                                 /* just restarted: the step was jl - 1, not a whole cycle */
                bl = (bl * ((jl - 1) >> 1)) >> 15;
                jl = 0;
            }
            b = (int32_t)(ph1 >> 16) - 32768 - bl;
            if (ph0 > 0xFFFFFFFFu - inc1)             /* OSC 1 restarts it before the next sample: round that step */
                b -= (blep(ph0, inc1) * (int32_t)(ph1 >> 17)) >> 15;
        }
        ph0 += inc1;
        ph1 += inc2;
        if (!sub && ph0 < inc1) {                     /* OSC 1 wrapped: OSC 2 restarts at the same point */
            uint32_t f = ph0 / ((inc1 >> 16) | 1u);   /* the part of a sample since, 0..65535 */
            uint32_t at = ph1 - f * (inc2 >> 16);     /* OSC 2 at the wrap */
            jl = (int32_t)(at >> 16) + 1;
            ph1 = f * (inc2 >> 16);
        }
        s = mulq15(a, m1) + mulq15(b, m2);
        if (nz)
            s += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, nz);
        if (drv)
            s = softclip(((s >> 2) * (drive >> 2)) >> 11);
        out[i] += voice_amp(s, m, i) << 1;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->s[2] = nst;
    v->s[3] = jl;
}

/* WAVE 0..4 (the loop as it always was, less its filter) */
static inline __attribute__((always_inline)) void analog_osc(track_t *t, voice_t *v, int32_t *out, uint32_t n,
                                                             const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t wave = (uint32_t)p[P_E0], i;
    int32_t det = p[P_E1], mix = p[P_E2], noise = p[P_E3];
    int32_t drive = 32768 + p[P_E6] * 512;                       /* 1x .. 3x */
    uint32_t inc1 = m->inc;
    /* DTN in cents. OSC B carries the voice's fine factor (unison detune, TUNE, bend) as OSC A does (SPEC 2.3: it
     * dropped it before DUELL) */
    uint32_t inc2 = det ? cents_inc(m->pitch16, det, m->fine) : inc1;
    uint32_t pw = 0x80000000u + (uint32_t)((m->shape - (64 << 8)) << 15);
    int32_t m2 = mix * 258, m1 = 32767 - m2;                    /* osc mix Q15 */
    if (m->amask)                                               /* ARP TARGET: the units on the arp (SPEC 2.3) */
        atg_analog(m, det, &inc1, &inc2, &m1, &m2);
    int32_t nz = noise * 200, drv = p[P_E6];
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1];                  /* state in locals: out[] may alias v->s[] */
    int32_t nst = v->s[2];
    for (i = 0; i < n; i++) {
        int32_t a, b, s;
        switch (wave) {
        case 1:
            a = osc_pulse(ph0, inc1, 0x80000000u);
            b = osc_pulse(ph1, inc2, 0x80000000u);
            break;
        case 2:
            a = osc_tri(ph0);
            b = osc_tri(ph1);
            break;
        case 3:
            a = osc_sine(ph0);
            b = osc_sine(ph1);
            break;
        case 4:
            a = osc_pulse(ph0, inc1, pw);
            b = osc_pulse(ph1, inc2, pw);
            break;
        default:
            a = osc_saw(ph0, inc1);
            b = osc_saw(ph1, inc2);
            break;
        }
        ph0 += inc1;
        ph1 += inc2;
        s = mulq15(a, m1) + mulq15(b, m2);
        if (nz)
            s += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, nz);
        if (drv)
            s = softclip(((s >> 2) * (drive >> 2)) >> 11);   /* pre-shifts: drive is up to 3x, no overflow */
        out[i] += voice_amp(s, m, i) << 1;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->s[2] = nst;
}
/* DUELL MORPH (morph.c, core.h mxf): WAVE 0..4 between the two ends (the filter TYPE fade went with the TYPE switch: MX_FTYPE stays a reserved index, nothing sets it). Both waves of each oscillator are
 * made and their sums cross-faded before the drive. Only while a fade is under way (MXF(t).w): its own copy of the loop, so
 * analog_osc stays what it was. (SYNC / SUB never fade: the morph never rolls to or from them) */
static inline int32_t analog_wave(uint32_t wave, uint32_t ph, uint32_t inc, uint32_t pw)
{
    switch (wave) {
    case 1:
        return osc_pulse(ph, inc, 0x80000000u);
    case 2:
        return osc_tri(ph);
    case 3:
        return osc_sine(ph);
    case 4:
        return osc_pulse(ph, inc, pw);
    default:
        return osc_saw(ph, inc);
    }
}
static __attribute__((noinline)) void analog_render_m(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t wa = (uint32_t)p[P_E0], wb = MXF(t).w[MX_E0] ? MXF(t).b[MX_E0] : wa, i;
    int32_t ww = MXF(t).w[MX_E0];
    int32_t det = p[P_E1], mix = p[P_E2], noise = p[P_E3];
    int32_t drive = 32768 + p[P_E6] * 512;
    uint32_t inc1 = m->inc;
    uint32_t inc2 = det ? cents_inc(m->pitch16, det, m->fine) : inc1;
    uint32_t pw = 0x80000000u + (uint32_t)((m->shape - (64 << 8)) << 15);
    int32_t m2 = mix * 258, m1 = 32767 - m2;
    if (m->amask)
        atg_analog(m, det, &inc1, &inc2, &m1, &m2);
    int32_t nz = noise * 200, drv = p[P_E6];
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1];
    int32_t nst = v->s[2];
    for (i = 0; i < n; i++) {
        int32_t s;
        s = mulq15(analog_wave(wa, ph0, inc1, pw), m1) + mulq15(analog_wave(wa, ph1, inc2, pw), m2);
        if (ww) {
            int32_t sb = mulq15(analog_wave(wb, ph0, inc1, pw), m1) + mulq15(analog_wave(wb, ph1, inc2, pw), m2);
            s += ((sb - s) * ww) >> 15;
        }
        ph0 += inc1;
        ph1 += inc2;
        if (nz)
            s += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, nz);
        if (drv)
            s = softclip(((s >> 2) * (drive >> 2)) >> 11);
        out[i] += voice_amp(s, m, i) << 1;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->s[2] = nst;
}

static void analog_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    if (MXF(t).on && MXF(t).w[MX_E0] && t->p[P_E0] < AW_SYNC)
        analog_render_m(t, v, out, n, m);               /* DUELL MORPH: a WAVE cross-fade */
    else if (t->p[P_E0] >= AW_SYNC)
        analog_render_x(t, v, out, n, m);
    else
        analog_osc(t, v, out, n, m);
}

/* {WAVE, DTN, MIX, NOIS, -, -, DRV, -}: the filter's three slots are unused (CUT RES KTR were filter 1's since cloud job J) */
static const preset_t ANALOG_PRESETS[] = {
    {"SAW LEAD", {0, 12, 64, 0, 0, 0, 10, 0}, {4, 70, 100, 50}, 20, 1, FX(0, 10, 45, 30), PAT(4)},
    {"SOFT PAD", {0, 20, 64, 4, 0, 0, 0, 0}, {80, 90, 110, 95}, 10, 0, FX(0, 60, 20, 70), PAT(5)},
    {"SQR BASS", {1, 0, 0, 0, 0, 0, 40, 0}, {0, 60, 40, 30}, 30, 1, FX(5, 0, 10, 10), PAT(2)},
    {"PWM STR", {4, 8, 40, 0, 0, 0, 0, 0}, {60, 80, 110, 85}, 8, 0, FX(0, 50, 20, 60), PAT(5)},
    {"ACID", {0, 0, 0, 0, 0, 0, 25, 0}, {0, 55, 20, 30}, 48, 1, FX(20, 0, 45, 15), PAT(1)},
    {"SINE KEY", {3, 6, 50, 0, 0, 0, 0, 0}, {2, 80, 30, 70}, 0, 0, FX(0, 30, 25, 40), PAT(6)},
    {"RAVE", {4, 30, 64, 0, 0, 0, 30, 0}, {20, 80, 110, 60}, 10, 1, FX(30, 40, 30, 30), PAT(13)},
    {"SUB BASS", {3, 0, 0, 0, 0, 0, 20, 0}, {0, 60, 100, 20}, 0, 1, FX(0, 0, 0, 10), PAT(8)},
    {"PLUCK", {0, 8, 50, 0, 0, 0, 0, 0}, {0, 88, 0, 60}, 55, 0, FX(0, 20, 50, 30), PAT(3)},
    {"BRASS", {0, 10, 64, 0, 0, 0, 10, 0}, {35, 70, 90, 45}, 40, 0, FX(0, 20, 20, 40), PAT(6)},
    {"WIND", {0, 0, 0, 90, 0, 0, 0, 0}, {60, 90, 60, 80}, 50, 0, FX(0, 30, 30, 70), PAT(5)},
    {"STRINGS", {0, 25, 64, 0, 0, 0, 0, 0}, {70, 90, 115, 90}, 5, 0, FX(0, 60, 20, 70), PAT(5)},
};

static const uint8_t ANALOG_UIORD[8] = {0, 1, 2, 3, 6, 4, 5, 7};   /* card position -> E index: ENGINE WAVE DTN MIX; EDIT 1 NOIS DRV (the three unused slots after them, hidden); the stored ids never move */

static const engine_t ENG_ANALOG = {
    .name = "ANALOG",
    .page_title = {"OSC", "FLT"},
    .edit = {
        {"WAVE", F_ENUM, 0, 6, 0, N_ANALOG_WAVE, 0},
        {"DTN", F_INT, 0, 127, 10, 0, "ct"},
        {"MIX", F_PCT, 0, 127, 64, 0, 0},
        {"NOIS", F_PCT, 0, 127, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},                 /* was CUT: filter 1 (voice.c uf_run) */
        {"-", F_INT, 0, 0, 0, 0, 0},                 /* was RES */
        {"DRV", F_PCT, 0, 127, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},                 /* was KTR (filter 1's KEY TRK) */
    },
    .presets = ANALOG_PRESETS,
    .npresets = NELEM(ANALOG_PRESETS),
    .note_on = analog_note_on,
    .render = analog_render,
    .desc = analog_desc,
    .knob = {P_FREEA, P_FREEB, P_ATK, P_REL},   /* filter 1's CUT and RES (they were the engine's own) */
    .uiord = ANALOG_UIORD,
    .atgt = ATG_ANALOG,          /* ARP TARGET: OSC A, OSC B (SPEC 2.3) */
    .ufilt = 1,                  /* no filter of its own since cloud job J: the multimode filter (voice.c uf_run: CUT RES MODE SLOPE, P_FREEA..D) */
};
