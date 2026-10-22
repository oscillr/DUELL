/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* DUELL 0.2.5 PERFORM mode (SPEC "PERFORM mode"): the master effects of the 16 white keys, ported from Felucca's FX hold layer
 * (perform.c up to DUELL 0.2.1, removed in 0.2.2) and grown by DUELL. PERFORM is a mode with its own page (ui_perform.c): a long
 * press of FX (0.7 s, the SAVE hold's timing) or a key pressed while FX is held turns it on; a tap of FX, HOME or any other page
 * button turns it off. While it is on, the white keys F3 .. G5 belong to it (seq.c keyboard_block): they play no note, send no
 * MIDI and record nothing; each holds its effect while it is held (ROLL A / B act once on the press). MIDI-in notes, the arps and
 * the sequencer play on; the black keys stay notes. The key map, four keys a row as the keyboard's groups:
 *   1-4   REPEAT 1/8, REPEAT 1/16, REPEAT 1/32, REVERSE
 *   5-8   TAPE STOP, FREEZE, OCT UP, OCT DN
 *   9-12  FLANGER, PHASER, DRIVE, PUMP
 *   13-16 ROLL A, ROLL B, MUTE 1, MUTE 2
 * MUTE 1 / MUTE 2: part 1 / part 2 silent while held (its dry signal ramps out and back in 2.9 ms before its level and sends, so
 * its delay and reverb tails ring on; not P_MUTE, never saved, never recorded). ROLL A / B: the MORPH page's ROLL of that end on
 * the part(s) ALGORITHM picks in PERFORM (ui_perform.c; morph.c MQ_ROLL).
 * The master stages, after the master level and before master_out (the limiter and the soft clip), on both channels (stereo; in DUEL
 * the left is part 1 and the right part 2, and every stage keeps the two apart): [the buffer: REPEAT / REVERSE / TAPE STOP / FREEZE /
 * OCT UP / OCT DN] -> FILTER (K3) -> FLANGER -> PHASER -> PUMP; DRIVE before the master level (fx.c perf_master: MASTER scales what it
 * gives, as every sound). REPEAT and REVERSE start on the next 1/16 (at once while stopped); the others at once; all end when let go,
 * with a 2.9 ms ramp. Effects of different kinds stack; of the buffer ones the last pressed plays, and letting it go returns to the
 * one held before. K4 DEPTH: the buffer effects' level (OCT UP / DN: their shimmer), FLANGER's and PHASER's level, DRIVE's drive,
 * PUMP's depth (not ROLL, not MUTE).
 * Idle (no key held, every ramp over, the knobs at their neutral place): every stage is skipped and the output is bit for bit
 * the output without PERFORM (fx.c mix_block runs its own path). */
enum { PF_R8, PF_R16, PF_R32, PF_REV, PF_TAPE, PF_FRZ, PF_OUP, PF_ODN, PF_FLG, PF_PHS, PF_DRV, PF_PUMP, PF_ROLLA, PF_ROLLB,
       PF_M1, PF_M2, PF_N };                  /* white key p (0 = F3) holds effect p */
#define PF_KEYS 16u                           /* the white keys F3 .. G5 */
#define PF_BIT(e) (1u << (e))
#define PF_ROLL (PF_BIT(PF_ROLLA) | PF_BIT(PF_ROLLB))
#define PF_MUTES (PF_BIT(PF_M1) | PF_BIT(PF_M2))
#define PF_WHITE 0x5AD5AD5u                   /* the white keys of the 27 (seq.c key_black), a bit per key */
#define PF_REPEAT 0x7u                                         /* the three REPEAT rates */
#define PF_Q (PF_REPEAT | PF_BIT(PF_REV))                      /* start on the 1/16 */
#define PF_HARM (PF_BIT(PF_OUP) | PF_BIT(PF_ODN))
#define PF_BUF (PF_REPEAT | PF_BIT(PF_REV) | PF_BIT(PF_TAPE) | PF_BIT(PF_FRZ) | PF_HARM)   /* the capture buffer's effects */
#define PB_FRAMES 8192u                       /* the capture buffer (pb_buf): stereo frames, 16 bit, at 22.05 kHz: 371 ms */
#define PB_MAX (2u * PB_FRAMES)               /* the longest loop, 44.1 kHz samples */
#define PB_TAPE 32000u                        /* TAPE STOP takes 1 beat, at most this (it lags a quarter of it) */
#define PB_FREC 4096u                         /* FREEZE: frames it records (186 ms) */
#define PB_GRAIN 2048u                        /* .. its two overlapping grains, samples (46 ms) */
#define HB_MASK 4095u                         /* OCT UP / DN: the delay's frames (of the buffer's 8192) */
#define HB_BASE 128u                          /* .. the taps' shortest delay, samples; the sweep: 2048 */
#define HB_FB 164                             /* .. K4 DEPTH (0 .. 100) -> the shimmer, Q15: 0 .. 0.5 */
#define PF_KILL 96                            /* FILTER: beyond +-96 of the knob the output fades to silence at +-100 (the kill zone: fully closed) */
#define PF_FADE 32                            /* .. its share's ramp a sample, Q15: in and out over 23 ms at its open end (a high-pass at 21 Hz switched
                                               * in rings at its corner: 3 ms let -40 dB of it through) */
#define PF_KOPEN 128                          /* .. the closure (Q8 detents) within half a detent of the open end: the filter fades out */
#define PF_ULO 20410                          /* .. the cutoff, Q8 table steps (PF_G: 1/8 octave from 20 Hz): the low-pass's open end, 20.0 kHz .. */
#define PF_ULR 51232                          /* .. its fall a detent (Q16 of a Q8 detent): 30 Hz at 96 (PF_ULO - 1198 = 19212 over 96 << 8) */
#define PF_UHR 52160                          /* .. the high-pass's rise from 20 Hz (step 0): 15.0 kHz at 96 (19560 over 96 << 8) */
#ifndef PF_SH
#define PF_SH 10                              /* .. the states carry 10 more bits than the signal .. */
#endif
#define PF_XMAX (1 << 18)                     /* .. its input's clamp (8 x full scale: x 1024 within 2^28, the states within 2^31) */
#define PF_KA 1984016189                      /* .. the two stages' damping, Q30: a 4-pole Butterworth's 2 cos(pi / 8) = 1.848 .. */
#define PF_KB 821806413                       /* .. and 2 cos(3 pi / 8) = 0.765 (no resonance bump) */
#define FL_LEN 356u                           /* FLANGER: its line's frames (8.1 ms: FL_MAX + 2) */
#define FL_MIN 22u                            /* .. the delay's sweep, samples: 0.5 .. 8 ms */
#define FL_MAX 353u
#define FL_FB 16384                           /* .. the feedback, Q15: 0.5 */
#define PH_FB 13107                           /* PHASER: the feedback, Q15: 0.4 (PH_ST, PH_A: fx.c, the INSERT shares them) */
#define DV_CLIP0 24000                        /* DRIVE: the hard clip after the fold at DEPTH 0 .. */
#define DV_CLIP1 9000                         /* .. and at 100 (the fold reaches +-24576) */
/* DRIVE's model, chosen when the firmware is built (0.2.7: the owner picks by ear; only the chosen one is in the image):
 *   0 FOLD  the INSERT's fold into a hard clip (0.2.5, the default)
 *   1 TUBE  a biased tanh soft clip (even harmonics), a gentle high cut
 *   2 FUZZ  a high gain into a lopsided diode-style clip, a tone filter
 *   3 CRUSH a soft saturation, then the bits and the rate reduced (digital destruction) */
#ifndef PERF_DRIVE_MODEL
#define PERF_DRIVE_MODEL 2                     /* 2 = FUZZ (the owner chose it); 0 FOLD, 1 TUBE, 3 CRUSH are kept for comparison */
#endif
#define PU_TOP 622                            /* PUMP: DEPTH (0 .. 100) -> the duck, Q16: 0 .. 0.95 (-26 dB) */
#define PU_ATK 88u                            /* .. its attack, samples (2 ms) */
enum { BM_NONE, BM_LOOP, BM_TAPE, BM_FRZ, BM_HARM };

static volatile uint8_t perf_keys;    /* main: PERFORM is on and owns the white keys (no menu, dialog, NAME over it) */
static volatile uint32_t perf_fxb;    /* main: FX's button bit while a key pressed with FX held opens PERFORM, 0 = not */
static volatile uint8_t perf_fx_hit;  /* ISR: a white key was taken while FX was held (main: PERFORM opens) */
static volatile uint32_t kb_perf;     /* ISR: the keys held that are PERFORM's, a bit per key */
static volatile uint32_t perf_held;   /* ISR: the effects held (PF_*) */
static volatile uint8_t perf_roll_n[2];   /* ISR: ROLL A / ROLL B presses, counted (main: morph_act once each) */
static volatile uint8_t perf_kill;    /* main: every effect off until its keys are let go (the menu, a dialog over PERFORM) */
static volatile uint32_t perf_act;    /* ISR: the effects running (PF_*) */
static uint32_t perf_ord[PF_N], perf_seq;   /* ISR: press order (the last pressed buffer effect plays) */
static volatile uint8_t perf_beat;    /* ISR: the beats of the tempo, counted (seq.c events_block: the ARP LED's beat; PUMP's) */
/* the knobs (ui_perform.c perf_knob; transient, never saved): K2 SPACE and K3 FILTER -100 .. 100, 0 = neutral (back to 0 when
 * PERFORM is left); K4 DEPTH 0 .. 100 % (kept until power-off) */
static volatile int8_t perf_space, perf_filt;
static volatile int8_t perf_depth = 100;

static struct {
    uint32_t ph, P, split;             /* samples since play; the 1/16; this block: where a 1/16 starts */
    uint32_t act, act0;                /* running after / before split */
    uint8_t busy, sync, mute;          /* a master stage at work; a 1/16 starts with the next block; parts whose mute ramps */
    /* the buffer */
    uint8_t mode, src, next, rev;      /* BM_*, the effect it plays (PF_N none), one waiting for the fade, REVERSE */
    uint8_t state, odd, xrev;          /* LOOP: 0 recording, 1 playing; the sample of a pair; the old loop's */
    uint32_t len, wr, rp;              /* loop length (samples), frames written, read (samples into the loop) */
    uint32_t xrp, xlen, xn;            /* the loop it switched from, faded over xn samples */
    uint32_t tv, dv, tpos;             /* TAPE: speed Q16, its step, read position Q16 frames */
    uint32_t gp[2], gs[2];             /* FREEZE: grain positions (samples), starts (frames) */
    uint32_t hph, hinc;                /* OCT UP / DN: tap A's place in the sweep (Q32), its step a sample */
    int32_t hfb;                       /* .. the shimmer now, Q15 (glides to K4 DEPTH's a block at a time) */
    int32_t w, tw, hl, hr;             /* the buffer's share and its target, Q15; the first of a pair */
    int32_t mg[NPART];                 /* the mutes' gains, Q15 (32768 = open) */
    /* FILTER (K3); all 0 = at rest, the low-pass, open */
    int32_t fk, fa, fkc;               /* the closure (Q8 detents from the open end, 0 .. 96 << 8, glides), the filter's share (Q15), the closure
                                        * fc is of */
    uint32_t fm;                       /* 0 the low-pass, 1 the high-pass (changes only at rest) */
    int32_t ka;                        /* the kill zone's share (Q15: 0 none, 32768 silent) */
    int32_t fc[6];                     /* the coefficients at the end of the last block: stage A a1 a2 a3, stage B a1 a2 a3 (Q30) */
    int32_t fz[2][4];                  /* the states, L R: A ic1 ic2, B ic1 ic2 (the signal x 256) */
    /* FLANGER, PHASER */
    int32_t fw, pw;                    /* their shares, Q15 */
    uint32_t fwp, fn;                  /* FLANGER: the frame written next; frames written since it started */
    int32_t px[2][PH_ST], py[2][PH_ST], pfb[2];   /* PHASER: each stage's last input and output, L and R; the fed back */
    /* DRIVE */
    int32_t dw, dg;                    /* its share (Q15), its drive now (Q12, glides to K4's) */
    int32_t dhp[2], ddc[2];            /* the low cut in front and the DC blocker after, L R (Q8) */
    int32_t dlp[2];                    /* TUBE, FUZZ: the tone's low-pass; CRUSH: the sample held, L R */
    uint32_t dph;                      /* CRUSH: the reduced rate's phase, Q16 */
    /* PUMP */
    int32_t pdn;                       /* the duck now, Q16 (0: none) */
    uint8_t pph, pbeat;                /* 0 at rest, 1 the attack, 3 the release; the beats seen (perf_beat) */
} pf = {.src = PF_N, .next = PF_N, .mg = {32768, 32768, 32768, 32768}};

static int16_t fl_buf[FL_LEN][2];      /* FLANGER's line: L R, at half the level (headroom) */

/* K3 FILTER's cutoff table (DUELL 0.2.14): g = tan(pi f / FS), Q27, a point every 1/8 octave from 20 Hz (u = 0 .. 80: f = 20 Hz * 2^(u / 8),
 * up to 20.5 kHz), FS 44100 */
static const int32_t PF_G[81] = {
    191228, 208536, 227410, 247992, 270437, 294914, 321606, 350714, 382457, 417072, 454821, 495986, 540877, 589831, 643216, 701433, 764920, 834153,
    909652, 991985, 1081772, 1179685, 1286462, 1402905, 1529889, 1668370, 1819388, 1984079, 2163684, 2359552, 2573160, 2806116, 3060176, 3337255,
    3639444, 3969026, 4328492, 4720564, 5148213, 5614687, 6123535, 6678639, 7284244, 7945000, 8665997, 9452821, 10311597, 11249059, 12272616,
    13390432, 14611525, 15945874, 17404552, 18999885, 20745644, 22657273, 24752183, 27050104, 29573540, 32348341, 35404442, 38776831, 42506817,
    46643742, 51247289, 56390683, 62165184, 68686524, 76104364, 84616511, 94491006, 106101610, 119987295, 136957088, 158286533, 186115271, 224335710,
    280864772, 374692284, 565754023, 1195040938,
};

/* the capture buffer (DUELL 0.2.5: the old FX layer's sl_buf, 32 KB in the pool): one stereo loop of PB_FRAMES at 22.05 kHz, L R */
static int16_t pb_buf[PB_FRAMES][2] __attribute__((section(".pool")));

/* the loop length of a REPEAT / REVERSE (1/8 .. 1/32) at the tempo, 44.1 kHz samples; 0 = not one */
static const uint8_t PF_DEN[PF_REV + 1] = {2, 4, 8, 2};
static uint32_t perf_len(uint32_t e) { return e <= PF_REV ? beat_samples() / PF_DEN[e] : 0u; }
/* the effects that can run at the tempo: a REPEAT / REVERSE longer than the buffer cannot (1/8 and REVERSE below 81 BPM, 1/16 below
 * 41, 1/32 below 21) */
static uint32_t perf_avail(void)
{
    uint32_t e, m = ~0u, b = beat_samples();
    for (e = PF_R8; e <= PF_REV; e++)
        if (b > PB_MAX * PF_DEN[e])
            m &= ~PF_BIT(e);
    return m;
}

/* seq.c keyboard_block: a PERFORM key down / up (e: PF_*) */
static void perf_press(uint32_t e, int down)
{
    if (e >= PF_N)
        return;
    if ((PF_ROLL >> e) & 1u) {                          /* ROLL A / B: once, on the press (the main loop rolls) */
        if (down)
            perf_roll_n[e - PF_ROLLA]++;
        return;
    }
    if (down) {
        perf_held |= PF_BIT(e);
        perf_ord[e] = ++perf_seq;
        if (e == PF_PUMP)
            pf.pbeat = perf_beat;                       /* (PUMP: from the next beat, not one counted before) */
    } else {
        perf_held &= ~PF_BIT(e);
    }
}

static void perf_start(void)                            /* seq_start: a 1/16 starts with the transport, and a beat (PUMP) */
{
    pf.ph = 0;
    pf.sync = 1;
    perf_beat++;
}

/* the buffer effect to play: the last pressed of those running */
static uint32_t perf_pick(uint32_t a)
{
    uint32_t e, best = PF_N;
    a &= PF_BUF;
    for (e = 0; e < PF_N; e++)
        if (((a >> e) & 1u) && (best == PF_N || perf_ord[e] - perf_ord[best] < 0x80000000u))
            best = e;
    return best;
}

static void perf_buf_start(uint32_t e)
{
    pf.src = (uint8_t)e;
    pf.next = PF_N;
    pf.wr = pf.rp = 0;
    pf.odd = 0;
    pf.xn = 0;
    if (e == PF_TAPE) {
        uint32_t t = beat_samples();
        pf.mode = BM_TAPE;
        if (t > PB_TAPE)
            t = PB_TAPE;
        pf.tv = 65536;
        pf.dv = 65536u / t + 1u;
        pf.tpos = 0;
    } else if (e == PF_OUP || e == PF_ODN) {    /* (wr: the frame written next, len: frames written) */
        pf.mode = BM_HARM;
        pf.len = 0;
        pf.hph = 0;
        pf.hinc = e == PF_OUP ? 0u - (1u << 21) : 1u << 20;   /* the delay: -1 / +0.5 sample a sample */
        pf.hfb = 0;
    } else if (e == PF_FRZ) {
        pf.mode = BM_FRZ;
        pf.gp[0] = 0;
        pf.gp[1] = PB_GRAIN / 2u;
        pf.gs[0] = pf.gs[1] = 0;
    } else {
        pf.mode = BM_LOOP;
        pf.state = 0;
        pf.len = perf_len(e);
        pf.rev = e == PF_REV;
    }
}

/* the buffer effect to play becomes e (at a 1/16 for REPEAT / REVERSE): a shorter REPEAT of what is
 * recorded switches with a cross-fade, anything else fades the old one out first */
static void perf_buf_select(uint32_t e)
{
    if (e == pf.src && pf.next == PF_N)
        return;
    if (e != PF_N && pf.mode == BM_LOOP && pf.state && e <= PF_REV && pf.src != PF_N &&
        perf_len(e) <= 2u * pf.wr) {
        pf.xrp = pf.rp;
        pf.xlen = pf.len;
        pf.xrev = pf.rev;
        pf.xn = SL_RAMP;
        pf.src = (uint8_t)e;
        pf.next = PF_N;
        pf.len = perf_len(e);
        pf.rev = e == PF_REV;
        pf.rp = 0;
        return;
    }
    if (pf.mode == BM_NONE || !pf.w) {
        if (e == PF_N) {
            pf.src = PF_N;
            pf.next = PF_N;
        } else {
            perf_buf_start(e);
        }
        return;
    }
    pf.src = PF_N;                                  /* fade out, then the next one (perf_buf_done) */
    pf.next = (uint8_t)e;
}

/* each block, after the keys (seq.c) and before the parts: what runs, the 1/16, the mutes. Returns 1 when a master stage has
 * something to do (fx.c perf_master) */
static __attribute__((noinline)) int perf_begin(uint32_t n)
{
    uint32_t held = (perf_kill ? 0u : perf_held) & perf_avail();
    uint32_t q, k, ph0, bnd;
    if (!held && !pf.busy && !perf_filt) {          /* nothing at the master: the clock only (and a mute's ramp) */
        pf.ph += n;
        pf.sync = 0;
        perf_act = pf.act = pf.act0 = 0;
        pf.split = n;
        pf.mute = 0;
        for (k = 0; k < NSHOWN; k++)
            if (pf.mg[k] != 32768)
                pf.mute |= (uint8_t)(1u << k);
        return 0;
    }
    pf.P = beat_samples() >> 2;
    if (pf.sync) {
        pf.ph = 0;
        pf.sync = 0;
    }
    ph0 = pf.ph % pf.P;                             /* the 1/16 in this block (n: none) */
    bnd = ph0 ? (pf.P - ph0 < n ? pf.P - ph0 : n) : 0u;
    pf.ph += n;
    pf.act &= held;                                 /* let go: at once */
    pf.act |= held & ~PF_Q;
    pf.act0 = pf.act;
    q = held & PF_Q & ~pf.act;
    pf.split = n;
    if (q && (!song.playing || bnd < n)) {
        pf.split = song.playing ? bnd : 0u;
        pf.act |= q;
    }
    perf_act = pf.act;
    pf.mute = 0;
    for (k = 0; k < NSHOWN; k++)
        if (((held >> (PF_M1 + k)) & 1u) || pf.mg[k] != 32768)
            pf.mute |= (uint8_t)(1u << k);
    pf.busy = 1;
    return 1;
}
/* the test of perf_begin's early way out, inlined in mix_block: nothing held, nothing ramping */
static inline int perf_idle(void) { return !perf_held && !pf.busy && !pf.mute && !perf_filt; }
/* idle: the 1/16's clock only (samples since PLAY, as perf_begin counts them) */
static inline void perf_tick(uint32_t n)
{
    pf.ph += n;
    pf.sync = 0;
}

/* a part muted by MUTE 1 / MUTE 2: its signal ramps to 0 and back (fx.c mix_part, after the EQ, before the DUCK and the level). t: the gain it goes to
 * (the block's snapshot, blockstate_t mute_tg: 0 while its key is held, else 32768) */
static __attribute__((noinline)) void perf_mute(uint32_t k, int32_t *b, uint32_t n, int32_t t)
{
    uint32_t i;
    int32_t g = pf.mg[k];
    for (i = 0; i < n; i++) {
        g += clamp(t - g, -SL_SLOPE, SL_SLOPE);
        b[i] = mulq16(b[i], (uint32_t)g << 1);
    }
    pf.mg[k] = g;
}
/* perf_mute with SPREAD's side signal sd (fx.c mix_spread): the same ramp on both */
static __attribute__((noinline)) void perf_mute_sd(uint32_t k, int32_t *b, int32_t *sd, uint32_t n, int32_t t)
{
    uint32_t i;
    int32_t g = pf.mg[k];
    for (i = 0; i < n; i++) {
        g += clamp(t - g, -SL_SLOPE, SL_SLOPE);
        b[i] = mulq16(b[i], (uint32_t)g << 1);
        sd[i] = mulq16(sd[i], (uint32_t)g << 1);
    }
    pf.mg[k] = g;
}
/* a part silent (fx.c mix_part's early way out): its mute's gain at once where it goes (nothing to ramp) */
static __attribute__((noinline)) void perf_mute_rest(uint32_t k, int32_t t)
{
    pf.mg[k] = t;
}

/* the loop: frame f of it, both channels (Q15) */
static inline void pb_put(uint32_t f, int32_t l, int32_t r)
{
    int16_t *p = &pb_buf[0][0] + 2u * (f & (PB_FRAMES - 1u));
    p[0] = (int16_t)clamp(l, -32768, 32767);
    p[1] = (int16_t)clamp(r, -32768, 32767);
}
/* sample s of the frames recorded from 0 (n of them): between two frames on an odd one */
static inline void pb_at(uint32_t s, uint32_t n, int32_t *l, int32_t *r)
{
    uint32_t f = s >> 1 < n ? s >> 1 : n - 1u;    /* (an odd loop's last sample: the frame before) */
    const int16_t *p = &pb_buf[0][0] + 2u * (f & (PB_FRAMES - 1u));
    const int16_t *q;                               /* a frame is the mean of samples 2f, 2f + 1: sample s */
    if (s & 1u)                                     /* lies a quarter frame from it, towards f + 1 or f - 1 */
        q = f + 1u < n ? p + 2 : p;
    else
        q = f ? p - 2 : p;
    *l = (3 * p[0] + q[0]) >> 1;
    *r = (3 * p[1] + q[1]) >> 1;
}
/* REPEAT / REVERSE at sample rp of a loop of len, windowed at both ends */
static inline void pb_loop(uint32_t rp, uint32_t len, int rev, uint32_t n, int32_t *l, int32_t *r)
{
    uint32_t e = len - 1u - rp;
    pb_at(rev ? e : rp, n, l, r);
    if (rp < e)
        e = rp;
    if (e < (uint32_t)SL_RAMP) {
        *l = (*l * (int32_t)e) >> SL_RAMP_LOG2;
        *r = (*r * (int32_t)e) >> SL_RAMP_LOG2;
    }
}

/* record the pair into frame f (the mean, >> 1: headroom) */
static inline uint32_t pb_rec(uint32_t f, int32_t l, int32_t r)
{
    if (!pf.odd) {
        pf.hl = l;
        pf.hr = r;
        pf.odd = 1;
        return f;
    }
    pf.odd = 0;
    pb_put(f, (pf.hl + l) >> 2, (pf.hr + r) >> 2);
    return f + 1u;
}

/* FREEZE: grain g's sample, by its triangle window, added to yl / yr; a new start at each pass */
static inline void perf_grain(uint32_t g, int32_t *yl, int32_t *yr)
{
    uint32_t p = pf.gp[g], tri = p < PB_GRAIN / 2u ? p : PB_GRAIN - p;
    int32_t a, b;
    if (!p)
        pf.gs[g] = ((rng() >> 16) * (pf.wr - PB_GRAIN / 2u + 1u)) >> 16;
    pb_at(2u * pf.gs[g] + p, pf.wr, &a, &b);
    *yl += a * (int32_t)tri;
    *yr += b * (int32_t)tri;
    pf.gp[g] = p + 1u < PB_GRAIN ? p + 1u : 0u;
}

/* OCT UP / DN: the tap at sweep place ph, both channels (as stored: half the level); silence before the
 * delay has that much in it */
static inline void hb_tap(uint32_t ph, int32_t *l, int32_t *r)
{
    uint32_t d = (HB_BASE << 16) + (ph >> 5), di = d >> 16;   /* the delay, Q16: 128 .. 2176 samples */
    int32_t f = (int32_t)((d >> 1) & 0x7FFFu);
    const int16_t *p, *q;
    if (di + 1u > pf.len) {
        *l = *r = 0;
        return;
    }
    p = &pb_buf[0][0] + 2u * ((pf.wr - di) & HB_MASK);
    q = &pb_buf[0][0] + 2u * ((pf.wr - di - 1u) & HB_MASK);
    *l = p[0] + (((q[0] - p[0]) * f) >> 15);
    *r = p[1] + (((q[1] - p[1]) * f) >> 15);
}
/* OCT UP / DN on one sample: yl / yr the harmonized mix: the mix an octave up / down beside itself (the classic rotating-delay
 * shifter: two read taps of a delay sweep 128 .. 2176 samples, half a sweep apart, at the speed that makes the pitch 2x / 0.5x; a
 * raised-cosine cross-fade hides each tap's jump back). Out: 0.56 the live mix + 0.7 the shifted one; K4 DEPTH its shimmer: the
 * shifted sound fed back into its own delay (0 .. 0.5, through the soft clip). noinline: perf_block's loop keeps its code layout */
static __attribute__((noinline)) void perf_harm(int32_t l, int32_t r, int32_t *yl, int32_t *yr)
{
    int32_t wa = (32768 - osc_sine(pf.hph + 0x40000000u)) >> 1, al, ar, bl, br, sl_, sr_;   /* A's share, Q15 */
    hb_tap(pf.hph, &al, &ar);
    hb_tap(pf.hph + 0x80000000u, &bl, &br);
    sl_ = (al * wa + bl * (32768 - wa)) >> 15;
    sr_ = (ar * wa + br * (32768 - wa)) >> 15;
    l >>= 1;                                        /* (stored at half: headroom, as the loop's frames) */
    r >>= 1;
    {
        int32_t xl = l, xr = r;
        if (pf.hfb) {                               /* the shimmer: back into the delay, soft-clipped */
            xl += softclip(mulq15(sl_, pf.hfb));
            xr += softclip(mulq15(sr_, pf.hfb));
        }
        if (pf.len < (uint32_t)SL_RAMP) {           /* the recording fades in (2.9 ms): no step where it */
            xl = (xl * (int32_t)pf.len) >> SL_RAMP_LOG2;   /* begins, for a tap to read across */
            xr = (xr * (int32_t)pf.len) >> SL_RAMP_LOG2;
        }
        pb_put(pf.wr & HB_MASK, xl, xr);
    }
    pf.wr++;
    if (pf.len <= HB_MASK)
        pf.len++;
    pf.hph += pf.hinc;
    *yl = l + (l >> 3) + ((sl_ * 45875) >> 15);     /* 0.56 live + 0.7 shifted (x2: stored at half) */
    *yr = r + (r >> 3) + ((sr_ * 45875) >> 15);
}

/* the buffer effect faded out: the next one, or the buffer back. Once per effect, out of the per-sample loop */
static __attribute__((noinline)) void perf_buf_done(void)
{
    if (pf.next != PF_N)
        perf_buf_start(pf.next);
    else
        pf.mode = BM_NONE;
}

/* K4 DEPTH as a share, Q15 (100 %: all of it) */
static inline int32_t perf_depth_q15(void) { return perf_depth >= 100 ? 32768 : perf_depth <= 0 ? 0 : perf_depth * 328; }

/* the buffer effect on one sample: the share pf.w of what it plays, the rest live */
static inline void perf_buf(int32_t *pl, int32_t *pr)
{
    int32_t l = *pl, r = *pr, yl = 0, yr = 0, on = 0;
    if (pf.mode == BM_LOOP) {
        if (!pf.state) {
            pf.wr = pb_rec(pf.wr, l, r);
            if (++pf.rp >= pf.len) {                /* (rp: samples recorded) */
                pf.state = 1;
                pf.rp = 0;
            }
        } else {
            pb_loop(pf.rp, pf.len, pf.rev, pf.wr, &yl, &yr);
            if (pf.xn) {                            /* the loop it switched from, fading out */
                int32_t ol, orr, g = (int32_t)pf.xn;
                pb_loop(pf.xrp, pf.xlen, pf.xrev, pf.wr, &ol, &orr);
                yl += (ol * g) >> SL_RAMP_LOG2;
                yr += (orr * g) >> SL_RAMP_LOG2;
                pf.xn--;
                if (++pf.xrp >= pf.xlen)
                    pf.xrp = 0;
            }
            if (++pf.rp >= pf.len)
                pf.rp = 0;
            on = 1;
        }
    } else if (pf.mode == BM_TAPE) {
        pf.wr = pb_rec(pf.wr, l, r);
        if (pf.wr >= 2u) {                          /* read behind the writing, slower and slower */
            uint32_t f = pf.tpos >> 16;
            int32_t fr = (int32_t)((pf.tpos >> 1) & 0x7FFFu), a, b, env;
            const int16_t *p = &pb_buf[0][0] + 2u * (f & (PB_FRAMES - 1u));
            const int16_t *s = &pb_buf[0][0] + 2u * ((f + 1u) & (PB_FRAMES - 1u));
            a = p[0];
            b = s[0];
            yl = (a + (((b - a) * fr) >> 15)) << 1;
            a = p[1];
            b = s[1];
            yr = (a + (((b - a) * fr) >> 15)) << 1;
            env = pf.tv >= 8192u ? 32768 : (int32_t)(pf.tv << 2);   /* the last eighth fades to silence */
            yl = mulq16(yl, (uint32_t)env << 1);
            yr = mulq16(yr, (uint32_t)env << 1);
            pf.tpos += pf.tv >> 1;
            pf.tv = pf.tv > pf.dv ? pf.tv - pf.dv : 0u;
            on = 1;
        }
    } else if (pf.mode == BM_FRZ) {
        if (pf.wr < PB_FREC)
            pf.wr = pb_rec(pf.wr, l, r);
        if (pf.wr >= PB_GRAIN / 2u) {               /* two grains of what it caught, triangle windows */
            perf_grain(0, &yl, &yr);
            perf_grain(1, &yl, &yr);
            yl >>= 10;                              /* (the windows sum to PB_GRAIN / 2) */
            yr >>= 10;
            on = 1;
        }
    } else if (pf.mode == BM_HARM) {
        perf_harm(l, r, &yl, &yr);
        on = 2;                                     /* (K4 DEPTH is the shimmer: the level stays full) */
    }
    pf.tw = on && pf.src != PF_N ? on == 2 ? 32768 : perf_depth_q15() : 0;
    pf.w += clamp(pf.tw - pf.w, -SL_SLOPE, SL_SLOPE);
    if (pf.w) {
        *pl = pf_mix(l, yl, pf.w);
        *pr = pf_mix(r, yr, pf.w);
    } else if (pf.src == PF_N) {                    /* faded out: the next one, or the buffer back */
        perf_buf_done();
    }
}

static __attribute__((noinline)) void perf_switch(uint32_t a) { perf_buf_select(perf_pick(a)); }

/* K3 FILTER (DUELL 0.2.14; SPEC 5e): a 24 dB/oct stereo filter, low-pass left of the middle, high-pass right of it, the middle exactly off.
 * Each channel two 12 dB stages of the trapezoidal state-variable filter (voice.c uf_run's, dsp.c tsvf: stable at any cutoff), a 4-pole
 * Butterworth (dampings 1.848 and 0.765: flat, no resonance bump). The numbers: the coefficients Q30, the states the signal x 256, the products
 * 32 x 32 -> 64 bits (0.2.9's Q14 table and states at a quarter of the signal's resolution stalled below ~200 Hz: the output stuck at a
 * DC offset that moved in steps, the crackle; the high-pass near its open end left that offset in). The cutoff glides (a one-pole in the
 * log domain, ~12 ms: knob detents become a smooth sweep) a block at a time and the coefficients move in straight lines between blocks,
 * a step every sample (no zipper). Turned from the middle the filter fades in over 2.9 ms at its open end (the low-pass 18.7 kHz at the
 * first detent, the high-pass 21 Hz); back in the middle (or across it) it sweeps open first, then fades out, then rests (states cleared;
 * the low-pass and the high-pass trade places only at rest). Beyond +-96 the kill zone fades to silence (2.9 ms a step at most). */
/* the stages over a channel's block b, in place, a stage at a time (its 3 coefficients and 2 states in registers: both stages in one
 * loop spilled on the device): state z (ic1 ic2), coefficients c (a1 a2 a3, Q30), moving by d a sample (0: steady). pf_a: stage A, in
 * the signal (clamped) x 256, out x 256; pf_b: stage B, in that, out rounded back to the signal. The low-pass v2, the high-pass
 * x - k v1 - v2; the products rounded (truncated, their bias left a DC offset at the lowest cutoffs). The four loops each: low-pass or
 * high-pass, steady or moving (the host build drops the attributes, so no loop relies on a flag folded by inlining) */
#define PF_STEP v3 = x - z1;                                                                    \
                v1 = (int32_t)(((int64_t)a1 * z0 + (int64_t)a2 * v3 + (1 << 29)) >> 30);        \
                v2 = z1 + (int32_t)(((int64_t)a2 * z0 + (int64_t)a3 * v3 + (1 << 29)) >> 30);   \
                z0 = 2 * v1 - z0;                                                               \
                z1 = 2 * v2 - z1
#define PF_MOVE a1 += d1, a2 += d2, a3 += d3
#define PF_HP(k) (x - v2 - (int32_t)(((int64_t)(k) * v1) >> 30))
#define PF_IN(v) (clamp(v, -PF_XMAX, PF_XMAX) << PF_SH)
#define PF_OUT(v) (((v) + (1 << (PF_SH - 1))) >> PF_SH)
#define PF_LOOPS(IN, LP, HP)                                                                        \
    uint32_t i;                                                                                   \
    int32_t a1 = c[0], a2 = c[1], a3 = c[2], z0 = z[0], z1 = z[1], d1 = d[0], d2 = d[1], d3 = d[2], x, v1, v2, v3;   \
    if (hp && (d1 | d2 | d3))                                                                     \
        for (i = 0; i < n; i++) { x = IN(b[i]); PF_MOVE; PF_STEP; b[i] = HP; }                    \
    else if (hp)                                                                                  \
        for (i = 0; i < n; i++) { x = IN(b[i]); PF_STEP; b[i] = HP; }                             \
    else if (d1 | d2 | d3)                                                                        \
        for (i = 0; i < n; i++) { x = IN(b[i]); PF_MOVE; PF_STEP; b[i] = LP; }                    \
    else                                                                                          \
        for (i = 0; i < n; i++) { x = IN(b[i]); PF_STEP; b[i] = LP; }                             \
    z[0] = z0;                                                                                    \
    z[1] = z1
#define PF_ID(v) (v)
static void pf_a(int32_t *b, uint32_t n, const int32_t *c, const int32_t *d, int32_t *z, uint32_t hp) { PF_LOOPS(PF_IN, v2, PF_HP(PF_KA)); }
static void pf_b(int32_t *b, uint32_t n, const int32_t *c, const int32_t *d, int32_t *z, uint32_t hp) { PF_LOOPS(PF_ID, PF_OUT(v2), PF_OUT(PF_HP(PF_KB))); }
#undef PF_LOOPS
#undef PF_STEP
#undef PF_MOVE
#undef PF_HP
#undef PF_IN
#undef PF_OUT
#undef PF_ID
/* one stage's a1 a2 a3 (Q30) of g (Q27) and the damping k (Q27): a1 = 1 / (1 + g (g + k)), a2 = g a1, a3 = g a2. a1 a 32-bit divide's
 * estimate (16 bits at least) and a Newton step (to ~1e-9): the three always of one g, so a block's coefficients are a stable filter's
 * and so is every straight line between two blocks' (det and trace of the state matrix stay inside the bounds) */
static void pf_stage_coefs(int32_t *c, int32_t g, int32_t k)
{
    int64_t den = (1ll << 30) + (((int64_t)g * (g + k)) >> 24), s = (int64_t)(0x80000000u / (uint32_t)(den >> 15)) << 14;
    c[0] = (int32_t)((s * ((2ll << 30) - ((den * s) >> 30))) >> 30);
    c[1] = (int32_t)(((int64_t)g * c[0]) >> 27);
    c[2] = (int32_t)(((int64_t)g * c[1]) >> 27);
}
/* the coefficients (stage A a1 a2 a3, stage B a1 a2 a3) at the closure k (Q8 detents) of mode fm */
static void pf_coefs(int32_t *c, int32_t k, uint32_t fm)
{
    int32_t u = fm ? (k * PF_UHR) >> 16 : PF_ULO - ((k * PF_ULR) >> 16), i = u >> 8;
    int32_t g = PF_G[i] + (int32_t)(((int64_t)(PF_G[i + 1] - PF_G[i]) * (u & 255)) >> 8);
    pf_stage_coefs(c, g, PF_KA >> 3);
    pf_stage_coefs(c + 3, g, PF_KB >> 3);
}
/* one channel b through the two stages, in place: the coefficients from c0 (the last block's) on a straight line to c1 (dc: moving) */
static void pf_chan(int32_t *b, uint32_t n, const int32_t *c0, const int32_t *c1, int32_t *z, uint32_t hp, uint32_t dc)
{
    uint32_t j;
    int32_t d[6] = {0, 0, 0, 0, 0, 0};
    if (dc)
        for (j = 0; j < 6u; j++)
            d[j] = n == CTL ? (c1[j] - c0[j]) >> CTL_LOG2 : (c1[j] - c0[j]) / (int32_t)n;
    pf_a(b, n, c0, d, z, hp);
    pf_b(b, n, c0 + 3, d + 3, z + 2, hp);
}
/* the share of the filtered f against the dry x from *fa to ta, the kill zone's from *ka to kt (2.9 ms ramps), into x; the ramps' ends
 * back in *fa, *ka */
static void pf_fade(int32_t *x, const int32_t *f, uint32_t n, int32_t *fa, int32_t ta, int32_t *ka, int32_t kt)
{
    uint32_t i;
    int32_t a = *fa, k = *ka;
    for (i = 0; i < n; i++) {
        int32_t y;
        a += clamp(ta - a, -PF_FADE, PF_FADE);
        y = pf_mix(x[i], f[i], a);
        k += clamp(kt - k, -SL_SLOPE, SL_SLOPE);
        x[i] = k ? pf_mix(y, 0, k) : y;
    }
    *fa = a;
    *ka = k;
}
/* K3 FILTER on the block (noinline: its own loop) */
static __attribute__((noinline)) void perf_filter(int32_t *bl, int32_t *br, uint32_t n)
{
    uint32_t i, wm = perf_filt > 0 ? 1u : perf_filt < 0 ? 0u : pf.fm;
    int32_t m = perf_filt < 0 ? -perf_filt : perf_filt, kt = m > PF_KILL ? (m - PF_KILL) * (32768 / (100 - PF_KILL)) : 0;
    int32_t same, tk, ta, d, c[6], dc;
    if (!pf.fa && wm != pf.fm) {                        /* at rest: the other mode, from its open end */
        pf.fm = wm;
        pf.fk = 0;
    }
    same = perf_filt && wm == pf.fm;
    tk = same ? (m < PF_KILL ? m : PF_KILL) << 8 : 0;   /* (across the middle: open first) */
    d = tk - pf.fk;
    pf.fk += d > -16 && d < 16 ? d : d >> 4;            /* the glide: 1/16 of the way a block (11.6 ms) */
    ta = same || pf.fk > PF_KOPEN ? 32768 : 0;
    if (!pf.fa && !ta) {                                /* at rest: the kill zone's fade alone */
        for (i = 0; i < n && pf.ka; i++) {
            pf.ka += clamp(kt - pf.ka, -SL_SLOPE, SL_SLOPE);
            bl[i] = pf_mix(bl[i], 0, pf.ka);
            br[i] = pf_mix(br[i], 0, pf.ka);
        }
        return;
    }
    dc = !pf.fa || pf.fk != pf.fkc;                     /* (the glide moved: this block's coefficients) */
    if (dc) {
        pf_coefs(c, pf.fk, pf.fm);
        pf.fkc = pf.fk;
        if (!pf.fa)                                     /* (from rest: the states clear, no line to draw from the last block) */
            memcpy(pf.fc, c, sizeof c);
    } else {
        memcpy(c, pf.fc, sizeof c);
    }
    if (pf.fa == 32768 && ta == 32768) {               /* the share full: the filter alone, in place (then the kill zone's fade, if) */
        pf_chan(bl, n, pf.fc, c, pf.fz[0], pf.fm, dc);
        pf_chan(br, n, pf.fc, c, pf.fz[1], pf.fm, dc);
        for (i = 0; i < n && (pf.ka || kt); i++) {
            pf.ka += clamp(kt - pf.ka, -SL_SLOPE, SL_SLOPE);
            bl[i] = pf_mix(bl[i], 0, pf.ka);
            br[i] = pf_mix(br[i], 0, pf.ka);
        }
    } else {                                            /* fading: filtered apart, then the shares (each channel from the same start) */
        int32_t f[CTL], fa = pf.fa, ka = pf.ka;          /* (n <= CTL: the mix's blocks, fx.c mix_l) */
        memcpy(f, bl, n * 4u);
        pf_chan(f, n, pf.fc, c, pf.fz[0], pf.fm, dc);
        pf_fade(bl, f, n, &fa, ta, &ka, kt);
        memcpy(f, br, n * 4u);
        pf_chan(f, n, pf.fc, c, pf.fz[1], pf.fm, dc);
        pf_fade(br, f, n, &pf.fa, ta, &pf.ka, kt);
    }
    memcpy(pf.fc, c, sizeof c);
    if (!pf.fa) {                                       /* faded out: at rest, open */
        memset(pf.fz, 0, sizeof pf.fz);
        pf.fk = 0;
    }
}

/* FLANGER / PHASER: the sweep's place at this block's first sample, a cycle every `s` 1/16s of the transport's clock
 * (pf.ph: in time with the bars while playing), and its step a sample (*inc) */
static uint32_t perf_lfo(uint32_t s, uint32_t n, uint32_t *inc)
{
    uint32_t per = s * pf.P;
    *inc = 0xFFFFFFFFu / per;
    return ((pf.ph - n) % per) * *inc;
}
/* the share a FLANGER / PHASER fades to: K4 DEPTH (while OCT UP / DN plays, DEPTH is its shimmer: full) */
static int32_t perf_mod_depth(void) { return (PF_HARM >> perf_pick(perf_act)) & 1u ? 32768 : perf_depth_q15(); }

/* FLANGER on one side c: x in, the line read at the sweep's place q: a short delay swept 0.5 .. 8 ms (a squared triangle: longer near
 * the short end, where the comb sweeps fastest), feedback 0.5 (through the soft clip); out (x + delayed) / 2 * 0.85 */
static inline int32_t fl_side(uint32_t c, int32_t x, uint32_t q)
{
    uint32_t t = pf_tri(q), d = (FL_MIN << 16) + (FL_MAX - FL_MIN) * ((t * t) >> 16);   /* the delay, Q16 samples */
    int32_t r0 = (int32_t)pf.fwp - (int32_t)(d >> 16), r1, a, v, fr = (int32_t)((d >> 1) & 0x7FFFu);
    if (r0 < 0)
        r0 += (int32_t)FL_LEN;
    r1 = r0 ? r0 - 1 : (int32_t)FL_LEN - 1;
    a = fl_buf[r0][c];
    v = a + (((fl_buf[r1][c] - a) * fr) >> 15);
    fl_buf[pf.fwp][c] = (int16_t)clamp((x >> 1) + softclip(mulq15(v, FL_FB)), -32767, 32767);
    return pf_mix(x, mulq16((x >> 1) + v, 55706u), pf.fw);   /* (x + delayed) / 2 * 0.85 (v: stored at half) */
}
/* FLANGER while held or fading: a bar a sweep in time with the transport, the right a quarter of it behind the left (noinline) */
static __attribute__((noinline)) void perf_flanger(int32_t *bl, int32_t *br, uint32_t n)
{
    uint32_t i, c, inc, q = perf_lfo(16u, n, &inc);
    int32_t tw = pf.act & PF_BIT(PF_FLG) ? perf_mod_depth() : 0, *const b[2] = {bl, br};
    if (!pf.fw && !(pf.act & PF_BIT(PF_FLG))) {         /* let go and faded: off, the line empty next time */
        pf.fn = 0;
        return;
    }
    if (!pf.fn)                                         /* (starting: the line empty) */
        memset(fl_buf, 0, sizeof fl_buf);
    for (i = 0; i < n; i++, q += inc) {
        pf.fw += clamp((pf.fn > FL_MAX + 1u ? tw : 0) - pf.fw, -SL_SLOPE, SL_SLOPE);   /* (heard once it holds 8 ms) */
        for (c = 0; c < 2u; c++)                        /* (the right a quarter of the sweep behind) */
            b[c][i] = fl_side(c, b[c][i], q - (c << 30));
        if (++pf.fwp >= FL_LEN)
            pf.fwp = 0;
        if (pf.fn <= FL_MAX + 1u)
            pf.fn++;
    }
}

/* PHASER: the stages' coefficient at the sweep's place q (Q14; fx.c PH_A: a break of 150 Hz .. 4.8 kHz) */
static int32_t ph_coef(uint32_t q)
{
    uint32_t t = pf_tri(q), i = t >> 11;
    return PH_A[i] + (((PH_A[i + 1u] - PH_A[i]) * (int32_t)(t & 2047u)) >> 11);
}
/* PHASER on one side c: four first-order all-pass stages (the classic pedal), the last one's output fed back (0.4); out (x + all-passed)
 * / 2 * 0.9 (rounded: no DC creeping in through the low breaks) */
static inline int32_t ph_side(uint32_t c, int32_t x, int32_t a)
{
    int32_t xs = clamp((x + 2) >> 2, -16383, 16383), u = xs + ((pf.pfb[c] * PH_FB + 16384) >> 15), y = u;
    uint32_t s;
    for (s = 0; s < PH_ST; s++) {                       /* y = a (u - y1) + u1 */
        y = clamp(pf.px[c][s] + ((a * (u - pf.py[c][s]) + 8192) >> 14), -65535, 65535);
        pf.px[c][s] = u;
        pf.py[c][s] = y;
        u = y;
    }
    pf.pfb[c] = clamp(y, -32767, 32767);
    u = xs + y;                                         /* (x + y) / 2 * 0.9, x 4 (>> 2 above): (x + y) * 1.8 */
    return pf_mix(x, u + mulq16(u, 52429u), pf.pw);
}
/* PHASER while held or fading: the break swept 150 Hz .. 4.8 kHz (log; a triangle of half a bar, the right a quarter of it behind);
 * the coefficients a block at a time (at its middle: 2.9 ms at most, the sweep moves 1 % in frequency meanwhile) */
static __attribute__((noinline)) void perf_phaser(int32_t *bl, int32_t *br, uint32_t n)
{
    uint32_t i, c, inc, q = perf_lfo(8u, n, &inc) + (n >> 1) * inc;
    int32_t tw = pf.act & PF_BIT(PF_PHS) ? perf_mod_depth() : 0, *const b[2] = {bl, br};
    int32_t a[2] = {ph_coef(q), ph_coef(q - 0x40000000u)};
    if (!pf.pw) {                                       /* starting: the stages from rest */
        memset(pf.px, 0, sizeof pf.px);
        memset(pf.py, 0, sizeof pf.py);
        pf.pfb[0] = pf.pfb[1] = 0;
    }
    for (i = 0; i < n; i++) {
        pf.pw += clamp(tw - pf.pw, -SL_SLOPE, SL_SLOPE);
        for (c = 0; c < 2u; c++)
            b[c][i] = ph_side(c, b[c][i], a[c]);
    }
}

/* PUMP while held (and while its last duck releases): on every beat of the tempo (internal or external clock: seq.c events_block counts
 * them in perf_beat, PLAY starts one) the master ducks: down to K4 DEPTH (100 %: 0.95, -26 dB) in 2 ms, back with the part DUCK's EXP
 * curve (fx.c duck_run CURVE EXP: a share of what is left a sample, three time constants in the release), the release a beat long, so
 * the next beat finds it all but back. It is its own: no part's DUCK settings change. noinline: its own loop */
static __attribute__((noinline)) void perf_pump(int32_t *bl, int32_t *br, uint32_t n)
{
    uint32_t i, bs = beat_samples();
    int32_t top = perf_depth * PU_TOP, as = top / (int32_t)PU_ATK, ek = (int32_t)((3u << 20) / (bs ? bs : 1u));
    if (pf.pbeat != perf_beat) {                        /* a beat (this block's events, or PLAY): a duck from the block's start */
        pf.pbeat = perf_beat;
        if ((pf.act & PF_BIT(PF_PUMP)) && top > 0)
            pf.pph = 1;
    }
    if (as < 1)
        as = 1;
    for (i = 0; i < n; i++) {
        if (pf.pph == 1) {
            pf.pdn += as;
            if (pf.pdn >= top) {
                pf.pdn = top;
                pf.pph = 3;
            }
        } else if (pf.pph == 3) {
            pf.pdn -= ((pf.pdn * ek) >> 20) | 1;
            if (pf.pdn <= 0) {
                pf.pdn = 0;
                pf.pph = 0;
            }
        }
        if (pf.pdn) {
            int32_t g = (65536 - pf.pdn) >> 4;
            bl[i] = ((bl[i] >> 4) * g) >> 8;
            br[i] = ((br[i] >> 4) * g) >> 8;
        }
    }
}

/* DRIVE while held or fading, before the master level (fx.c perf_master), the master's stereo mix (each channel its own): a ~27 Hz
 * low cut in front, K4 DEPTH the drive (squared: gentle at first), the model's shape (PERF_DRIVE_MODEL), its level, a DC blocker
 * after (~14 Hz: the lopsided models' offset); its share fades in and out over 2.9 ms.
 *   FOLD  the drive 1x .. 15.6x, the fold (fx.c ins_shape IT_FOLD: the wave reflected at +-16384, x 1.5), a hard clip that comes
 *         down with DEPTH (24000 .. 9000: the folds' tops flattened), x 2/3 (DEPTH 0: unity below the fold) .. x 1/2 (DEPTH 100:
 *         about as loud as the dry mix, the squared waves louder for their peak)
 *   TUBE  the drive 1x .. 11x, tanh (dsp.c softclip) of the signal plus a bias of 0.3 (the positive half flattens first: even
 *         harmonics), less the bias's own value (silence stays 0), a one-pole high cut (9 kHz .. 4.5 kHz)
 *   FUZZ  the drive 1x .. 50x, a diode-style clip: the positive half hard at DV_FP, the negative a soft one 3/4 of full (a
 *         lopsided square at full drive), a one-pole tone filter (5.5 kHz .. 2.8 kHz)
 *   CRUSH the drive 1x .. 2x into tanh, then the bits (16 .. 4: the step 1 .. 4096) and the rate (44.1 kHz .. 1.8 kHz: a sample
 *         held, the same instants on both channels) reduced
 * The level (DV_OG, Q15, at DEPTH 0 25 50 75 100, straight lines between) keeps the DEPTH 0 sound at the dry level and the loudness
 * close to the dry mix's (measured on EERIE SIGNAL, M03 since 0.2.13, LADDER BASS, PWM PAD at MASTER's default position). */
#if PERF_DRIVE_MODEL == 1
#define DV_K 66                               /* TUBE: the drive's curve, 4096 + DEPTH^2 x DV_K / 16 (Q12): 1x .. 11x */
#define DV_BIAS 9830                          /* .. the bias, 0.3 (Q15) */
#define DV_A0 23700                           /* .. the high cut's coefficient at DEPTH 0 (9 kHz) .. */
#define DV_A1 15500                           /* .. and 100 (4.5 kHz), Q15 */
#define DV_OG {35800, 24260, 13970, 10430, 9110}   /* .. the level (Q15) at DEPTH 0 25 50 75 100 (0: 1 / tanh' at the bias) */
#elif PERF_DRIVE_MODEL == 2
#define DV_K 320                              /* FUZZ: 1x .. 50x */
#define DV_FP 14000                           /* .. the positive half's clip */
#define DV_A0 17500                           /* .. the tone (5.5 kHz) .. */
#define DV_A1 10000                           /* .. (2.8 kHz) */
#define DV_OG {32768, 17000, 14000, 13270, 13130}
#elif PERF_DRIVE_MODEL == 3
#define DV_K 7                                /* CRUSH: 1x .. 2x */
#define DV_OG {32768, 32950, 28320, 23610, 18960}
#endif
static __attribute__((noinline)) void perf_drive(int32_t *bl, int32_t *br, uint32_t n)
{
    uint32_t i, c;
#if PERF_DRIVE_MODEL == 0
    int32_t tw = pf.act & PF_BIT(PF_DRV) ? 32768 : 0, gt = 4096 + perf_depth * perf_depth * 6;
    int32_t cl = DV_CLIP0 - (DV_CLIP0 - DV_CLIP1) * perf_depth / 100, og = 21845 - 5461 * perf_depth / 100, *const b[2] = {bl, br};
#else
    int32_t tw = pf.act & PF_BIT(PF_DRV) ? 32768 : 0, gt = 4096 + ((perf_depth * perf_depth * DV_K) >> 4), lim;
    static const uint16_t OG[5] = DV_OG;
    int32_t q = perf_depth / 25, og = OG[q] + (q < 4 ? (OG[q + 1] - OG[q]) * (perf_depth - 25 * q) / 25 : 0), *const b[2] = {bl, br};
#if PERF_DRIVE_MODEL == 3
    uint32_t sh = (uint32_t)perf_depth * 12u / 100u, inc, hit;   /* the bits' step 2^sh; the rate's step (Q16) */
#else
    int32_t a = DV_A0 - (DV_A0 - DV_A1) * perf_depth / 100;
#endif
#endif
    if (!pf.dw) {                                       /* starting: from rest, the drive at K4's at once */
        pf.dhp[0] = pf.dhp[1] = pf.ddc[0] = pf.ddc[1] = pf.dlp[0] = pf.dlp[1] = 0;
        pf.dph = 1u << 16;
        pf.dg = gt;
    }
    pf.dg += clamp(gt - pf.dg, -2048, 2048);            /* (K4 turned: the drive glides, ~25 ms over the range) */
#if PERF_DRIVE_MODEL
    lim = 0x7FFFFFFF / pf.dg;                           /* (the drive's product in 32 bits; past it every model is flat) */
#endif
#if PERF_DRIVE_MODEL == 3
    inc = 65536u / (1u + (uint32_t)(perf_depth * perf_depth) * 23u / 10000u);   /* 1x .. 1/24 the rate (squared) */
#endif
    for (i = 0; i < n; i++) {
        pf.dw += clamp(tw - pf.dw, -SL_SLOPE, SL_SLOPE);
#if PERF_DRIVE_MODEL == 3
        pf.dph += inc;
        hit = pf.dph >= (1u << 16);                       /* a new sample at the reduced rate (both channels) */
        pf.dph &= 0xFFFFu;
#endif
        for (c = 0; c < 2u; c++) {
            int32_t x = b[c][i], v, y;
            pf.dhp[c] += ((x << 8) - pf.dhp[c]) >> 8;   /* ~27 Hz low cut (Q8) */
            v = clamp(x - (pf.dhp[c] >> 8), -230000, 230000);
#if PERF_DRIVE_MODEL == 0
            y = ins_shape(IT_FOLD, ((v >> 3) * pf.dg) >> 9, 0);   /* (>> 3 first: 32 bits) */
            y = clamp(y, -cl, cl);
            y = (y * og) >> 15;                         /* x 2/3 .. x 1/2 (DEPTH 0 .. 100) */
#else
            v = (clamp(v, -lim, lim) * pf.dg) >> 12;
#if PERF_DRIVE_MODEL == 1
            y = softclip(v + DV_BIAS) - softclip(DV_BIAS);
#elif PERF_DRIVE_MODEL == 2
            y = v > DV_FP ? DV_FP : v > 0 ? v : (softclip(v + (v >> 2) + (v >> 4)) * 3) >> 2;   /* (x 1.31 in: slope 1 at 0) */
#else
            if (hit)
                pf.dlp[c] = ((softclip(v) + ((1 << sh) >> 1)) >> sh) << sh;
            y = pf.dlp[c];
#endif
#if PERF_DRIVE_MODEL != 3
            pf.dlp[c] += (((y - pf.dlp[c]) >> 1) * a) >> 14;   /* the tone: a one-pole low-pass */
            y = pf.dlp[c];
#endif
            y = (y * og) >> 15;
#endif
            pf.ddc[c] += ((y << 8) - pf.ddc[c]) >> 9;   /* DC blocker, ~14 Hz */
            y -= pf.ddc[c] >> 8;
            b[c][i] = pf_mix(x, y, pf.dw);
        }
    }
}

/* after the master level, before master_out: the buffer effect (stereo: L and R apart, in DUEL part 1 and part 2 each their own) */
static __attribute__((noinline)) void perf_block(int32_t *bl, int32_t *br, uint32_t n)
{
    uint32_t i, a = pf.act0;
    if (pf.split)
        perf_switch(a);
    pf.hfb += clamp(perf_depth * HB_FB - pf.hfb, -512, 512);   /* OCT UP / DN's shimmer glides (~40 ms) */
    for (i = 0; i < n; i++) {
        int32_t l = bl[i], r = br[i];
        if (i == pf.split) {
            a = pf.act;
            perf_switch(a);
        }
        if (pf.mode != BM_NONE)
            perf_buf(&l, &r);
        bl[i] = l;
        br[i] = r;
    }
    if (perf_filt || pf.fa || pf.ka)
        perf_filter(bl, br, n);
    if ((pf.act & PF_BIT(PF_FLG)) || pf.fw || pf.fn)
        perf_flanger(bl, br, n);
    if ((pf.act & PF_BIT(PF_PHS)) || pf.pw)
        perf_phaser(bl, br, n);
    if ((pf.act & PF_BIT(PF_PUMP)) || pf.pph)
        perf_pump(bl, br, n);
    pf.busy = pf.act || pf.mode != BM_NONE || pf.w || pf.fa || pf.ka || pf.fw || pf.pw || pf.fn || pf.dw || pf.pph || perf_filt;
}

/* the master's lines from silence (fx.c dm_lines_clear: a change between DUEL and STEREO / LAYER): a buffer effect held records anew */
static __attribute__((noinline)) void perf_lines_clear(void)
{
    pf.fn = 0;                                          /* FLANGER's line (cleared when it starts again), PHASER's stages, the filters, DRIVE's */
    memset(pf.px, 0, sizeof pf.px);
    memset(pf.py, 0, sizeof pf.py);
    pf.pfb[0] = pf.pfb[1] = 0;
    memset(pf.fz, 0, sizeof pf.fz);
    pf.dhp[0] = pf.dhp[1] = pf.ddc[0] = pf.ddc[1] = pf.dlp[0] = pf.dlp[1] = 0;
    if (pf.mode != BM_NONE) {
        if (pf.src != PF_N) {
            perf_buf_start(pf.src);
        } else {                                    /* (fading out: gone at once, the buffer back) */
            pf.w = 0;
            perf_buf_done();
        }
    }
}
