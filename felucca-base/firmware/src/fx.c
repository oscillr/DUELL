/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Effects: per-track DIST and INSERT (1.5), then sends into three
 * shared buses (chorus, tempo delay, reverb). Mono buses, stereo dry mix.
 * DUELL step 5 (SPEC 5b.1): the buses come twice (fx_bus.c, a template), A and B. STEREO and LAYER: A is the shared
 * buses, as always (B idle: no cost). SPLIT: A takes part 1's sends and returns to the left only, B part 2's to the
 * right only (mix_block, split_buses): nothing of one part reaches the other side. The two delays share dly_buf (each half
 * of it); the chorus and reverb lines of B are its own (pool) */
#define DLY_LEN 65536u           /* 1.49 s: 1/4 at 40 BPM fits */
#define DLY_HALF (DLY_LEN / 2u)  /* SPLIT: each part's delay (0.74 s; a longer time halves until it fits: delay_split) */
#define CHO_LEN 2048u
static int16_t dly_buf[DLY_LEN] __attribute__((section(".pool")));
static int16_t cho_buf[CHO_LEN] __attribute__((section(".pool")));
static const uint16_t REV_COMB[4] = {1116, 1188, 1277, 1356};
static const uint16_t REV_AP[2] = {556, 441};
#define REV_COMB_N (1116 + 1188 + 1277 + 1356)
static int16_t rev_comb[REV_COMB_N] __attribute__((section(".pool")));
typedef union {                         /* ROOM's allpasses; SPRING's allpass chain (int32: no clamps) */
    int16_t ap[556 + 441];
    int32_t sp[(556 + 441) / 2];
} revu_t;
static revu_t rev_u __attribute__((section(".pool")));
#define rev_ap (rev_u.ap)
typedef struct {
    uint32_t dly_w, cho_w, cho_ph;
    int32_t dly_lp;
    uint16_t comb_i[4], ap_i[2];
    int32_t comb_lp[4];
    uint8_t rtype;                       /* the reverb model running (G_RTYPE: 0 ROOM, 1 SPRING, 2 HALL, 3 SHIM, 4 RESO) */
    uint32_t sh_w, sh_ph;                /* SHIM (ported from JIANT): its line's write count, the grains' phase */
    int32_t sh_hp, sh_he;                /* .. its output's low cut (~14 Hz) and that step's remainder */
    int32_t rs_lp[4];                    /* RESO (ported from JIANT): the combs' loop low-passes */
    uint16_t rs_w;                       /* .. their write index (each its own line, RS_LEN) */
    uint16_t sp_w;                       /* SPRING: the loop's write index (SP_MASK) */
    int32_t sp_lp, sp_hp, sp_he, sp_size;   /* .. its loop low-pass, low cut (and its remainder), the loop
                                             * length (Q8, glides) */
    uint32_t sp_ph;                      /* .. the output tap's wobble */
} fxs_t;
static fxs_t fx;
/* SHIM's own memory per bus (the one extra RAM of the JIANT models; RESO has none, it lives in ROOM's combs): its octave-up
 * line (4 KB) and the block of its output that returns into the next block's input (CTL int32) */
#define SH_LEN 2048u
typedef struct { int16_t buf[SH_LEN]; int32_t fb[CTL]; } shim_t;
static shim_t shim __attribute__((section(".pool")));
/* bus B (SPLIT, part 2): its own chorus line, reverb lines and states (pool: zero at boot, cleared at each switch to SPLIT) */
static int16_t cho_buf_b[CHO_LEN] __attribute__((section(".pool")));
static int16_t rev_comb_b[REV_COMB_N] __attribute__((section(".pool")));
static revu_t rev_u_b __attribute__((section(".pool")));
static fxs_t fx_b __attribute__((section(".pool")));
static shim_t shim_b __attribute__((section(".pool")));

/* SPRING (G_RTYPE 1): one spring of a spring tank, mono like the other buses, in the ROOM's own buffers (no
 * RAM of its own): the input and the loop's return -> a low cut (~110 Hz: a spring carries little bass) ->
 * SP_N stretched first-order allpasses, (a + z^-4) / (1 + a z^-4) (after Valimaki, Parker and Abel: below
 * fs / 8 = 5.5 kHz the group delay rises with frequency, the chirp; each pass round the loop adds more of
 * it: the "boing", the drips) -> the loop's delay line (rev_comb, SP_LEN) -> back through a one-pole
 * low-pass (DAMP) and the decay gain (SIZE). The output: the spring's far end, half way along the loop
 * (the first sound 15 .. 30 ms after the send: the tank's own pre-delay; a slow wobble of a sample or two
 * on it), plus a second, quieter pickup at three quarters (a shorter spring beside it: denser). SIZE sets
 * the loop's length (30 .. 60 ms) and its decay; DAMP the loop's low-pass. The allpasses' states: 4
 * samples each (int32), in rev_ap's memory. Changing the model fades the old one's block out and clears both buffers. */
#define SP_LEN 4096u                     /* the loop's line in rev_comb (4937 samples) */
#define SP_MASK (SP_LEN - 1u)
#define SP_N 10u                         /* allpass stages */
#define SP_A 2867                        /* their coefficient, Q12 (0.7: Q12 keeps (x - o) * a in 32 bits up to
                                          * |x - o| < 749000, far past any peak the chain reaches) */
_Static_assert(sizeof rev_comb / 2u >= SP_LEN && sizeof rev_u.sp / 4u >= 4u * (SP_N + 1u), "SPRING in ROOM's buffers");

/* SHIM (G_RTYPE 3) and RESO (G_RTYPE 4): ported from JIANT (https://github.com/juanjiant-bit/JIANT, v0.6.3, firmware/src/fx.c:
 * rev_shimmer, rev_reso; GPL-3.0, a fork of Felucca by juanjiant-bit), see LICENSING.md. Changed for DUELL: both buses (fx_bus.c),
 * no extra parameters (JIANT's MOD, RATE, WIDE do not exist here: SHIM's amount and RESO's voicing are fixed below, SIZE and
 * DAMP act as on the REVERB page), RESO reads the scale of the part its bus carries.
 *   SHIM  ROOM with its output an octave up fed back into its input (two crossfaded grains reading a 2048-sample line at twice
 *         the speed: a pitch shifter): each pass round the loop an octave higher, a halo of rising partials. SIZE DAMP as ROOM.
 *   RESO  four combs tuned to the part's scale (ROOT and SCALE: its I III V VII from C3), a sympathetic string each: whatever
 *         is sent rings in the key. SIZE their sustain (to 0.9992), DAMP their brightness. In ROOM's comb memory.
 * SHIM has a ~14 Hz low cut on its output and return: ROOM's loop holds a small constant offset after its tail (a few LSB, from
 * its floors; unchanged in ROOM), which SHIM would otherwise keep sounding for ever.
 * Both are mono like ROOM (the return is the same on both channels; in SPLIT it is folded to the part's own side).
 * (JIANT's CLOUD, a 32 KB granular wash, is not ported: see the report / LICENSING.md) */
#define SH_AMT 13100                     /* SHIM's feedback, Q15 (0.4: JIANT's MOD at 69 of 127; at most 0.74 there) */
#define SH_GATE 24                       /* the return's soft gate (-62 dBFS): a halo under it dies instead of ringing on for ever */
#define SH_OGATE 6                       /* the output's, under ROOM's rounding cycles that may go on at a few LSB (-70 dBFS) */
#define RS_LEN 1024u                     /* each comb's line, in rev_comb: down to ~43 Hz */
_Static_assert(sizeof rev_comb / 2u >= 4u * RS_LEN, "RESO in ROOM's buffers");
static uint32_t scale_mask(const track_t *t);           /* seq.c */

/* HALL (G_RTYPE 2, 1.2): a smooth, dense, stereo reverb, again in ROOM's buffers (its few states and one block
 * of work in the pool, no .bss). ROOM's 4 combs each ring on their own (the metallic tone); here every echo
 * goes round 8 lines that all feed each other, and they run at half the rate (22.05 kHz: the same memory holds
 * twice the time, so twice the resonances, at half the work; the tail stops near 10 kHz, as a hall's air
 * does). The send (every other sample of it, low-passed 1 2 1; ROOM's level) -> a low cut (~110 Hz: no boom
 * in a long tail) -> 2 allpass diffusers (rev_ap: a drum's click smeared into a burst) -> into the 8 lines
 * (rev_comb, mutually prime lengths, 21 .. 34 ms). Lines 1, 2, 4, 7 are read through slow sines (+-HL_MOD
 * samples, between samples; 0.37 or 0.74 Hz, each in its own phase): the resonances drift instead of ringing.
 * Lines 0, 3, 5, 6, read at their ends, go through a one-pole low-pass (DAMP: the mixing takes every echo
 * through them); every
 * line through its gain (from its length: all decay alike; RT60 below the damping 0.5 s at SIZE 0, 4 s at
 * 90, 9 s at 127; ~2.4 s heard at 90 with DAMP 60); then the 8 x 8 mixing: a conference matrix (Paley,
 * order 8: every line feeds the 7 others, +-1 each, none itself: no flutter at a line's own length), with
 * the send added. Stereo: left lines 0 3 4 -7, right 1 2 5 6 (sums spread over the matrix's rows); the wet
 * bus (mono) gets left + right, rev_side left - right, which mix_block adds to the left and takes from the
 * right (rev_side_mix). Back to 44.1 kHz between samples. Rounding: a loud line's gain rounds to the nearest
 * (the tail decays as it should), a quiet one's towards zero (it ends in exact silence: no limit cycle), and
 * the low-passes keep their steps' remainders (no offset). */
#define HL_NL 8u                         /* lines */
#define HL_NAP 2u                        /* diffusers */
#define HL_NC (CTL / 2u)                 /* a block at half the rate */
#define HL_MOD 8                         /* the taps' swing, samples (read 2 .. 18 samples short of the end) */
#define HL_LPM 0x69u                     /* the lines with a low-pass */
#define HL_KD(d) (32767 - (d) * 180)     /* its coefficient (Q15) for DAMP d: none at 0 .. ~1 kHz at 127 */
#define HL_LOUD 32                       /* a line this loud (two samples' sum) rounds its gain to the nearest */
#define HL_OUT 4                         /* the outputs' gain: the level of ROOM's tail */
static const uint16_t HL_LEN[HL_NL] = {467, 509, 547, 593, 631, 673, 709, 743};   /* (+1 each: the mirror) */
static const uint16_t HL_AP[HL_NAP] = {131, 307};
static const int16_t HL_APG[HL_NAP] = {11469, 10240};               /* Q14: 0.7, 0.625 */
_Static_assert(467 + 509 + 547 + 593 + 631 + 673 + 709 + 743 + HL_NL <= sizeof rev_comb / 2u &&
               131 + 307 <= sizeof rev_u.ap / 2u, "HALL in ROOM's buffers");
_Static_assert(CTL % 2u == 0u && HL_NC + 2u * HL_MOD + 2u < 467u && HL_NC <= 131u, "HALL: one wrap a block at most");
typedef struct {                         /* HALL's state (in the pool: no .bss) */
    uint16_t i[HL_NL], d[HL_NAP];        /* the lines' write indices, the diffusers' */
    int32_t lp[HL_NL], le[HL_NL];        /* the low-passes and their steps' remainders */
    int32_t x, m, s, hp, he;             /* the send's last sample, its low cut and that step's remainder; the
                                          * last mid and side out (half rate) */
    int16_t g[HL_NL], sz;                /* the lines' gains / sqrt(7) (Q14), for SIZE sz - 1 (0: none yet) */
    uint32_t ph;                         /* the taps' drift */
    uint8_t side;                        /* sd holds this block's stereo difference (HALL running) */
    int32_t sd[CTL];                     /* the block's left minus right (mix_block: rev_side_mix) */
    int32_t bx[HL_NC], l[HL_NC], r[HL_NC];   /* the block at half the rate: the diffused send, left, right */
    int16_t y[HL_NL][HL_NC];             /* the lines' ends, then what goes back into them */
} hall_t;
static hall_t hl __attribute__((section(".pool")));
static hall_t hl_b __attribute__((section(".pool")));   /* bus B's (SPLIT); its side is never mixed (folded: the mid only) */
#define rev_side (hl.sd)


/* DIST: low cut -> drive (1x..8x, exponential) -> asymmetric soft clip
 * (a little bias = even harmonics) -> tone low-pass that closes with drive ->
 * make-up gain (straight into tanh over the full band, it would sound like a
 * broken digital fuzz). State per part (track_t dist_*). */
static void track_dist(track_t *t, int32_t *b, uint32_t n)
{
    int32_t d = t->p[P_DIST], i, g, k, mk, bias = 2400, b0;
    if (!d)
        return;                                         /* states kept: switching on does not click */
    g = 4096 + d * d * 2;                                /* Q12: 1x .. ~9x, gentle at first */
    k = 32000 - d * 95;                                  /* tone: transparent at low drive .. ~3 kHz, Q15 */
    mk = 30000 - d * 120;                                /* make-up */
    b0 = softclip(bias);
    for (i = 0; i < (int32_t)n; i++) {
        int32_t x = b[i], y;
        t->dist_hp += (x - t->dist_hp + 64) >> 7;           /* ~55 Hz low cut: keep the bass out of the clipper */
        x = clamp(x - t->dist_hp, -230000, 230000);         /* (x >> 2) * g fits 32 bits; the clip is flat out there */
        y = softclip((((x >> 2) * g) >> 10) + bias) - b0;   /* >> 2 first: no overflow for loud poly */
        t->dist_lp1 += mulq15(y - t->dist_lp1, k);         /* two poles: tames the fizz */
        t->dist_lp2 += mulq15(t->dist_lp1 - t->dist_lp2, k);
        b[i] = mulq15(t->dist_lp2, mk);
    }
}

/* master: peak limiter in front of the soft clipper. Fast attack (~0.1 ms),
 * ~150 ms release, threshold where tanh is still nearly linear, so chords
 * get quieter instead of crushed. */
#define LIM_T 18000
static int32_t lim_env = LIM_T;
/* MENU > USB LEVEL FIXED (for #42: record over USB with the speaker turned down): the mix goes to master_out at the
 * full MASTER level, USB audio takes that (audio.c uac_tap), and only then does MASTER scale what the DAC gets
 * (usb_fixed_dac). MASTER (0, the default): MASTER before master_out, as always (USB follows the knob) */
static volatile uint8_t fx_usb_fixed;
#define MASTER_FULL 4096                /* main.c: the MASTER knob's top, Q12 */
/* the MASTER pot (ADC 0..1023, smoothed in main.c) to its level, square law: 0 .. 4088 (Q12) */
static inline uint32_t master_of_pot(uint32_t k10) { return (k10 * k10) >> 8; }
static __attribute__((noinline)) void usb_fixed_dac(int32_t *out, uint32_t n)   /* audio ISR, after uac_tap */
{
    uint32_t i;
    int32_t m = (int32_t)song.master_q12;
    for (i = 0; i < 2u * n; i++)
        out[i] = (out[i] * m) >> 12;                /* (|out| <= 32767 after the soft clip: fits) */
}
static volatile uint8_t fx_lowcut;     /* settings: 1 LOWCUT 12 dB/oct ~110 Hz, 2 BASS+ (the small speaker):
                                        * 12 dB/oct ~220 Hz plus the harmonics of the bass (spk_bass) */
static int32_t lc_l1, lc_l2, lc_r1, lc_r2, dc_l, dc_r, dce_l, dce_r;

/* DC blocker (~2 Hz), always on: a leaky integrator of the input (Q6 state) subtracted from it.
 * The >> 12 step keeps its remainder (error feedback, 0..4095) and adds it to the next one, so no
 * part of the step is lost: the state follows the input exactly, down to 0 after the sound stops.
 * (A rounded step of (x - dc) / 4096 would stop moving at |x - dc| < 2048 and leave an offset of up
 * to +-31 at the output after silence.) */
static inline int32_t dc_block(int32_t x, int32_t *dc, int32_t *err)
{
    int32_t e = (x << 6) - *dc + *err, d = e >> 12;
    *err = e - (d << 12);
    *dc += d;
    return x - ((*dc + 32) >> 6);
}

static int32_t lce[4];
static inline int32_t lowcut1(int32_t x, int32_t *lc, int32_t *err, uint32_t sh)   /* x minus its one-pole low-pass */
{
    int32_t e = x - *lc + *err, d = e >> sh;
    *err = e - (d << sh);
    *lc += d;
    return x - *lc;
}

/* BASS+: what the speaker cannot play, heard through its harmonics. The bass below ~150 Hz is clipped at its
 * own envelope (a level-following trapezoid: odd harmonics), then band-passed ~220 Hz..1 kHz and added.
 * The low-pass has 4 poles (#42: with 2, the trapezoid rebuilt the 300 .. 600 Hz of the mix itself, late,
 * and cancelled up to 6 dB of it; now under 0.5 dB) */
typedef struct { int32_t lp1, lp2, lp3, lp4, env, h1, h2, hl; } sbass_t;
static sbass_t sb, sb_r;              /* (sb_r: the right channel's own, SPLIT: master_out_split) */
static inline int32_t spk_bass(sbass_t *b, int32_t m)
{
    int32_t a, t, u;
    b->lp1 += ((m - b->lp1) * 692) >> 15;
    b->lp2 += ((b->lp1 - b->lp2) * 692) >> 15;
    b->lp3 += ((b->lp2 - b->lp3) * 692) >> 15;
    b->lp4 += ((b->lp3 - b->lp4) * 692) >> 15;
    a = b->lp4 < 0 ? -b->lp4 : b->lp4;
    if (a > b->env)
        b->env += (a - b->env) >> 2;
    else if (b->env > 0)
        b->env -= (b->env >> 11) + 1;
    t = clamp(b->lp4 * 8, -b->env, b->env);
    b->h1 += (t - b->h1) >> 5;
    u = t - b->h1;
    b->h2 += (u - b->h2) >> 5;
    u -= b->h2;
    b->hl += (u - b->hl) >> 3;
    return b->hl + (b->hl >> 1);      /* x1.5 (#180: x3 peaked at 1.5 .. 3.5 x a full-scale kick, the limiter */
}                                     /* pulled the mix down up to 16 dB on each hit and the buzz took over) */

static inline void master_out(int32_t *l, int32_t *r)
{
    int32_t al, ar, a;
    *l = dc_block(*l, &dc_l, &dce_l);
    *r = dc_block(*r, &dc_r, &dce_r);
    if (fx_lowcut) {                  /* two one-pole high-passes, error feedback as dc_block (the */
        uint32_t sh = fx_lowcut == 2u ? 5u : 6u;    /* rounded step stopped at |x - lc| < 32: an offset) */
        int32_t b = fx_lowcut == 2u ? spk_bass(&sb, (*l + *r) >> 1) : 0;
        *l = lowcut1(*l, &lc_l1, &lce[0], sh);
        *l = lowcut1(*l, &lc_l2, &lce[1], sh) + b;
        *r = lowcut1(*r, &lc_r1, &lce[2], sh);
        *r = lowcut1(*r, &lc_r2, &lce[3], sh) + b;
    }
    al = *l < 0 ? -*l : *l;
    ar = *r < 0 ? -*r : *r;
    a = al > ar ? al : ar;
    if (a > lim_env)
        lim_env += (a - lim_env) >> 2;
    else if (lim_env > LIM_T)
        lim_env -= ((lim_env - LIM_T) >> 12) + 1;
    if (lim_env > LIM_T) {
        int32_t g = (int32_t)(((uint32_t)LIM_T << 15) / (uint32_t)lim_env);   /* < 32768 */
        *l = ((*l >> 4) * g) >> 11;                      /* >> 4 first: |l| may be far above Q15 */
        *r = ((*r >> 4) * g) >> 11;
    }
    *l = softclip(*l);
    *r = softclip(*r);
}

/* SPLIT (DUELL step 5): master_out with nothing shared between the channels: BASS+ of each channel's own bass (not of
 * the mono sum: part 1's bass would sound right), and each channel its own limiter (a linked one would duck part 2 on
 * part 1's peaks: part 1's envelope on the right). The DC blocker, LOWCUT and the soft clip are per channel already */
static int32_t lim_env_r = LIM_T;
static inline int32_t lim_one(int32_t x, int32_t *env)
{
    int32_t a = x < 0 ? -x : x;
    if (a > *env)
        *env += (a - *env) >> 2;
    else if (*env > LIM_T)
        *env -= ((*env - LIM_T) >> 12) + 1;
    if (*env > LIM_T) {
        int32_t g = (int32_t)(((uint32_t)LIM_T << 15) / (uint32_t)*env);
        x = ((x >> 4) * g) >> 11;
    }
    return x;
}
static inline void master_out_split(int32_t *l, int32_t *r)
{
    *l = dc_block(*l, &dc_l, &dce_l);
    *r = dc_block(*r, &dc_r, &dce_r);
    if (fx_lowcut) {
        uint32_t sh = fx_lowcut == 2u ? 5u : 6u;
        int32_t bl = fx_lowcut == 2u ? spk_bass(&sb, *l) : 0, br = fx_lowcut == 2u ? spk_bass(&sb_r, *r) : 0;
        *l = lowcut1(*l, &lc_l1, &lce[0], sh);
        *l = lowcut1(*l, &lc_l2, &lce[1], sh) + bl;
        *r = lowcut1(*r, &lc_r1, &lce[2], sh);
        *r = lowcut1(*r, &lc_r2, &lce[3], sh) + br;
    }
    *l = softclip(lim_one(*l, &lim_env));
    *r = softclip(lim_one(*r, &lim_env_r));
}

/* length of one division (N_DIV order) in samples at the song tempo */
static const uint8_t DIV_DEN[6] = {1, 2, 4, 8, 3, 6};    /* original IDs stay fixed; slow rates append */
static const uint8_t DIV48[7] = {32, 64, 4, 144, 72, 36, 18};   /* 4T 2T 32T 2D 4D 8D 16D in 1/48 quarter notes (div >= 10) */
static uint32_t midi_beat_samples;                    /* zero until an external clock has a measured tempo */
static uint32_t beat_samples(void)
{
    return song.g[G_CLOCK] && midi_beat_samples ? midi_beat_samples : (uint32_t)FS * 60u / (uint32_t)song.g[G_BPM];
}
static uint32_t div_samples(uint32_t div)
{
    uint32_t quarter = beat_samples();
    return div < 6u ? quarter / DIV_DEN[div] : div < 10u ? quarter << (div - 5u) : quarter * DIV48[(div - 10u) % 7u] / 48u;
}

#include "perform.c"                                 /* the FX hold layer's effects (the master) */
#include "click.c"                                   /* the metronome's click (after the master: audio.c) */

static uint32_t delay_samples(void)
{
    uint32_t s = div_samples((uint32_t)song.g[G_DTIME]);
    return s < 16u ? 16u : s >= DLY_LEN ? DLY_LEN - 1u : s;
}

/* HALL's helpers: m / 2^sh rounded towards zero (a product that feeds back must not floor: a loop of floors
 * holds an offset and a little noise for ever) */
static inline int32_t sh_tz(int32_t m, uint32_t sh) { return (m + ((m >> 31) & ((1 << sh) - 1))) >> sh; }
/* 2^-x, x Q16 (0 .. 16): Q15 (32768 at x = 0). 2^-x = 2^u / 2^(i + 1), u = 1 - frac(x); 2^u by a cubic (1e-4) */
static uint32_t exp2_neg(uint32_t x)
{
    uint32_t i = x >> 16, u = 32768u - ((x & 0xFFFFu) >> 1), p = 2579u;
    p = ((p * u) >> 15) + 7409u;
    p = ((p * u) >> 15) + 22780u;
    p = ((p * u) >> 15) + 32768u;
    return i < 15u ? p >> (i + 1u) : 0u;
}
/* the lines' gains (hall_gains): fx_bus.c */

/* an allpass diffuser over x[0 .. n) from q (its next samples, no wrap): its state rounded towards zero */
static inline void hall_ap(int16_t *q, int32_t g, int32_t *x, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t y = q[i], v = x[i] + sh_tz(y * g, 14);
        q[i] = (int16_t)clamp(v, -32768, 32767);
        x[i] = y - ((v * g + 8192) >> 14);
    }
}
/* a line's end over y[0 .. n) from b (no wrap; b[n] is still the line's: its mirror at the end): between b[i]
 * and b[i + 1], f / 256 of the way (rounded to the nearest: it stays between the two), times its gain g (Q14).
 * The gain's product: rounded to the nearest while the line is loud (lo 0: the tail decays as it should; a
 * truncation would make it die early, the sooner the quieter), towards zero once it is quiet (lo 1: the last
 * steps of the tail, which then ends in exact silence) */
static inline void hall_end(const int16_t *b, int32_t f, int16_t *y, uint32_t n, int32_t g, int lo)
{
    uint32_t i;
    if (lo)
        for (i = 0; i < n; i++)
            y[i] = (int16_t)sh_tz((b[i] + (((b[i + 1u] - b[i]) * f + 128) >> 8)) * g, 14);
    else
        for (i = 0; i < n; i++)
            y[i] = (int16_t)(((b[i] + (((b[i + 1u] - b[i]) * f + 128) >> 8)) * g + 8192) >> 14);
}
/* a line's end over y[0 .. n) from b (no wrap), through the low-pass (kd, Q15; the state *lp and its step's
 * remainder *le), times its gain g (rounded as hall_end's) */
static inline void hall_end_lp(const int16_t *b, int16_t *y, uint32_t n, int32_t g, int lo, int32_t *lp, int32_t *le,
                               int32_t kd)
{
    uint32_t i;
    int32_t l = *lp, r = *le, e;
    if (lo)
        for (i = 0; i < n; i++) {
            e = (b[i] - l) * kd + r, r = e & 32767, l += e >> 15;
            y[i] = (int16_t)sh_tz(l * g, 14);
        }
    else
        for (i = 0; i < n; i++) {
            e = (b[i] - l) * kd + r, r = e & 32767, l += e >> 15;
            y[i] = (int16_t)((l * g + 8192) >> 14);
        }
    *lp = l;
    *le = r;
}
/* the block's HL_NC samples of a circular buffer of len from index d in at most two runs, no wrap inside them:
 * body with s (the run's start in the block) and k (its length); d moves on */
#define HL_RUNS(d, len, k, s, body)                                                         \
    for (s = 0; s < HL_NC; s += k) {                                                        \
        k = (len) - (d) < HL_NC - s ? (len) - (d) : HL_NC - s;                              \
        body;                                                                               \
        d = (d) + k >= (len) ? 0u : (d) + k;                                                \
    }

#define HL_LFO ((uint32_t)(0.37 * CTL / FS * 4294967296.0))   /* the drift: 0.37 Hz (even lines), 0.74 Hz (odd) */

static int32_t part_buf[CTL];                            /* a part's block (mix_part); the fade of a model change */

/* the buses (fx_bus.c): bus A on the globals they always had, bus B (SPLIT, part 2) on its own */
static uint8_t dm_split;                /* (below: the mix runs SPLIT; RESO reads the part its bus carries) */
#define BUS_FX fx
#define BUS_SHIM shim
#define BUS_SPLITB 0u                   /* bus A: part 1 (all the parts when not SPLIT); B: part 2 */
#define BUS_HL hl
#define BUS_COMB rev_comb
#define BUS_U rev_u
#define BUS_CHO cho_buf
#define BUS_DLY dly_buf
#define BUS_FN(f) f
#include "fx_bus.c"
#undef BUS_FX
#undef BUS_SHIM
#undef BUS_SPLITB
#undef BUS_HL
#undef BUS_COMB
#undef BUS_U
#undef BUS_CHO
#undef BUS_DLY
#undef BUS_FN
#define BUS_FX fx_b
#define BUS_SHIM shim_b
#define BUS_SPLITB 1u
#define BUS_HL hl_b
#define BUS_COMB rev_comb_b
#define BUS_U rev_u_b
#define BUS_CHO cho_buf_b
#define BUS_DLY (dly_buf + DLY_HALF)
#define BUS_FN(f) f##_b
#include "fx_bus.c"
#undef BUS_FX
#undef BUS_SHIM
#undef BUS_SPLITB
#undef BUS_HL
#undef BUS_COMB
#undef BUS_U
#undef BUS_CHO
#undef BUS_DLY
#undef BUS_FN

/* bus A over the whole delay line: STEREO and LAYER (and the tests' entry point), as the buses always ran */
static uint32_t delay_split(void);
static int32_t dly_sd[CTL];             /* PING-PONG: this block's left minus right of the delay's return, / 2 (fx_bus.c) */
static uint8_t dly_side;                /* dly_sd holds this block's (PING-PONG ON) */
static void fx_buses(const int32_t *cho_in, const int32_t *dly_in, const int32_t *rev_in, int32_t *wet, uint32_t n)
{
    uint32_t pp = song.g[G_PINGPONG] == 1;              /* (DUELL, SPEC 5c.2: the repeats alternate left and right) */
    dly_side = (uint8_t)pp;
    if (pp)                                             /* (the loop is 2 x the time: a time that does not fit halves, as in SPLIT) */
        fx_buses_m(DLY_LEN - 1u, delay_split(), cho_in, dly_in, rev_in, wet, n, 1u, dly_sd);
    else
        fx_buses_m(DLY_LEN - 1u, delay_samples(), cho_in, dly_in, rev_in, wet, n, 0u, dly_sd);
}

/* after the buses: HALL's stereo difference to the left and from the right of the dry mix (the wet bus adds the
 * same to both) */
static __attribute__((noinline)) void rev_side_mix(int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        l[i] += rev_side[i];
        r[i] -= rev_side[i];
    }
}

/* PING-PONG: the delay's stereo difference to the left and from the right of the dry mix (as HALL's, above) */
static __attribute__((noinline)) void dly_side_mix(int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t i;
    int32_t k = clamp(song.g[G_PINGSPREAD], 0, 100) * 32768 / 100;   /* SPREAD (Q15): 100 % the whole side, 0 % none (centred) */
    for (i = 0; i < n; i++) {
        int32_t sd = k >= 32768 ? dly_sd[i] : (((dly_sd[i] >> 4) * (k >> 3)) >> 8);
        l[i] += sd;
        r[i] -= sd;
    }
}

/* INSERT (1.5, Discussions #78 and #177): one effect per track, after DIST and before the EQ, with a dry / wet
 * MIX. TYPE (P_ITYPE, N_ITYPE; stored, append-only) and its three values A B C (P_IA .. P_IC, what they mean: params.c
 * ins_desc):
 *   SOFT HARD FOLD FUZZ  DRIVE (1x .. ~13x, squared), TONE (two one-pole low-passes at F_CUTOFF), LEVEL (F_DB, as the
 *                        track's LEVEL: 112 is 0 dB). A slow low cut in front (~27 Hz), a DC blocker after (FUZZ is
 *                        lopsided). SOFT tanh, HARD a flat clip at a voice's level, FOLD a triangle folder (the wave
 *                        reflected at +-16384: every fold more harmonics), FUZZ a hard top and a soft, halved bottom;
 *   CRUSH                BITS (1 .. 16, of a voice's full scale), RATE (a held sample, 689 Hz .. 44.1 kHz, INS_HOLD:
 *                        aliasing as the old samplers), LPF (two one-pole low-passes at F_CUTOFF after them, #177);
 *   PHASR                RATE (F_LFOHZ), DEPTH (the sweep around ~850 Hz: 0 still .. the whole 150 Hz .. 4.8 kHz),
 *                        FDBK (0 .. 0.7): the FX layer's four all-pass stages (perform.c PH_A), mono, per track;
 *   FLANG CHOR           RATE, DEPTH, FDBK: a 256-sample line per track (5.8 ms); FLANG swept 0.25 .. 5 ms (the FX
 *                        layer's squared triangle), feedback 0 .. 0.85; CHOR a sine around 3.2 ms (+-2.3 ms at DEPTH
 *                        127), feedback 0 .. 0.4. A DC blocker on the swept copy (a clipped loud input is lopsided).
 * The swept ones are the pedals': their wet is (dry + swept) / 2, the notches need both, so MIX 100 % is the whole
 * effect and less a lighter one. MIX (P_IMIX) blends dry and wet linearly; MIX 0 or TYPE OFF: the track bit for bit
 * as before (nothing runs). MIX glides (2.9 ms, SL_SLOPE); a TYPE change fades the old one out, clears the state,
 * fades the new one in; so does a start from MIX 0 (no stale line or filter). With SPREAD (mix_spread) the insert
 * takes the mono sum b, as DIST does; the side passes it by.
 * Order: DIST -> INSERT -> EQ: the insert colours the sound as DIST does (DUELL: the SLICER that came after is
 * removed). State in the pool, per track (ins[], 580 B each); noinline, its own loops: the audio
 * ISR's inlined path gains one test per part (track_t ins_run) */
enum { IT_OFF, IT_SOFT, IT_HARD, IT_FOLD, IT_FUZZ, IT_CRUSH, IT_PHASER, IT_FLANGER, IT_CHORUS, IT_N };
#define IL_LEN 256u                     /* FLANG / CHOR: the line, samples (a power of 2: masked) */
static const uint8_t INS_HOLD[16] = {64, 48, 40, 32, 24, 20, 16, 12, 10, 8, 6, 5, 4, 3, 2, 1};   /* CRUSH RATE */
typedef struct {
    int32_t w;                          /* the wet share now, Q15 */
    uint8_t type;                       /* the type running */
    uint8_t cnt;                        /* CRUSH: samples left of the held one */
    uint16_t wp;                        /* FLANG / CHOR: the line's next write */
    uint32_t ph;                        /* the sweep's phase */
    int32_t hp, lp1, lp2, dc, hold;     /* low cut (Q8), the two low-passes, DC blocker (Q8), CRUSH's held sample */
    int32_t px[PH_ST], py[PH_ST], fb;   /* PHASR: each stage's last input and output, the fed back */
    int16_t line[IL_LEN];               /* FLANG / CHOR, at a quarter (headroom) */
} ins_t;
static ins_t ins[NLIVE] __attribute__((section(".pool")));

static inline int32_t ins_mixq(int32_t m) { return m >= 127 ? 32768 : m <= 0 ? 0 : m * 258; }   /* MIX -> Q15 */
/* the one-pole low-pass (topology-preserving) coefficient of cutoff index c, Q16: G = g / (1 + g), g = tan(pi fc / FS) */
static int32_t ins_lpk(int32_t c)
{
    uint32_t g = SVF_G[clamp(c, 0, 127)];
    return (int32_t)((g << 16) / (4096u + g));
}
/* the drives' and CRUSH's shape of v (a voice's full scale 32768; v already through DRIVE): the curve the INSERT
 * graph draws too */
static inline int32_t ins_shape(uint32_t ty, int32_t v, uint32_t sh)
{
    switch (ty) {
    case IT_SOFT:
        return softclip(v);
    case IT_HARD:
        return clamp(v, -24000, 24000);
    case IT_FOLD: {
        int32_t u = (int32_t)((uint32_t)(v + 16384) & 0xFFFFu) - 32768;   /* a triangle of period 65536: +-16384 */
        u = 16384 - (u < 0 ? -u : u);
        return u + (u >> 1);                            /* x 1.5 (no divide) */
    }
    case IT_FUZZ:
        return v > 0 ? (v > 20000 ? 20000 : v) : softclip(v) >> 1;
    case IT_CRUSH:                                      /* sh: 16 - BITS; to the nearest step */
        return sh ? ((v + (1 << (sh - 1u))) >> sh) << sh : v;
    default:
        return v;
    }
}
static inline uint32_t ins_bits_sh(int32_t a) { return 15u - (uint32_t)(clamp(a, 0, 127) >> 3); }   /* BITS 1..16 */

/* SOFT .. FUZZ: low cut -> DRIVE -> shape -> TONE -> LEVEL -> DC blocker; CRUSH: held sample -> bits -> LPF */
static __attribute__((noinline)) void ins_drive(ins_t *s, const track_t *t, int32_t *b, uint32_t n, int32_t tw)
{
    uint32_t i, ty = s->type, crush = ty == IT_CRUSH, sh = ins_bits_sh(t->p[P_IA]);
    uint32_t hold = INS_HOLD[clamp(t->p[P_IB], 0, 127) >> 3];
    int32_t a = t->p[P_IA], g = 4096 + a * a * 3;       /* DRIVE, Q12: 1x .. 12.8x, gentle at first */
    int32_t k = ins_lpk(crush ? t->p[P_IC] : t->p[P_IB]), lvl = crush ? 4096 : LEVEL_Q12[t->p[P_IC] & 127];
    for (i = 0; i < n; i++) {
        int32_t x = b[i], y, v;
        s->w += clamp(tw - s->w, -SL_SLOPE, SL_SLOPE);
        if (crush) {
            if (!s->cnt) {
                s->hold = ins_shape(IT_CRUSH, clamp(x, -262144, 262143), sh);
                s->cnt = (uint8_t)hold;
            }
            s->cnt--;
            y = s->hold;
        } else {
            s->hp += ((x << 8) - s->hp) >> 8;            /* ~27 Hz low cut (Q8: no offset left by the rounding) */
            v = clamp(x - (s->hp >> 8), -230000, 230000);
            y = ins_shape(ty, ((v >> 3) * g) >> 9, 0);    /* (>> 3 first: 32 bits) */
        }
        y <<= 4;                                        /* (x16: the low cutoffs' rounding leaves no offset) */
        v = mulq16(y - s->lp1, (uint32_t)k);             /* TONE / LPF: two one-pole low-passes */
        y = v + s->lp1;
        s->lp1 = y + v;
        v = mulq16(y - s->lp2, (uint32_t)k);
        y = v + s->lp2;
        s->lp2 = y + v;
        y = (y + 8) >> 4;
        if (!crush) {
            y = (y * lvl) >> 12;                        /* LEVEL */
            s->dc += ((y << 8) - s->dc) >> 9;           /* DC blocker, ~14 Hz (FUZZ's lopsided halves) */
            y -= s->dc >> 8;
        }
        b[i] = pf_mix(x, y, s->w);
    }
}

/* PHASR, FLANG, CHOR: the sweep at RATE (one LFO per track), DEPTH, FDBK */
static __attribute__((noinline)) void ins_swept(ins_t *s, const track_t *t, int32_t *b, uint32_t n, int32_t tw)
{
    uint32_t i, inc = LFO_INC[clamp(t->p[P_IA], 0, 127)] / CTL, dep = (uint32_t)clamp(t->p[P_IB], 0, 127);
    int32_t c = clamp(t->p[P_IC], 0, 127);
    if (s->type == IT_PHASER) {                         /* the coefficients a block at a time (at its middle) */
        uint32_t tr = pf_tri(s->ph + (n >> 1) * inc), q = 32768u + (uint32_t)(((int32_t)tr - 32768) * (int32_t)dep / 127);
        int32_t a = PH_A[q >> 11] + (((PH_A[(q >> 11) + 1u] - PH_A[q >> 11]) * (int32_t)(q & 2047u)) >> 11);
        int32_t fbk = c * 180;                          /* 0 .. 0.7, Q15 */
        for (i = 0; i < n; i++) {
            int32_t x = b[i], xs = clamp((x + 4) >> 3, -16383, 16383), u = xs + ((s->fb * fbk + 16384) >> 15), y = u;
            uint32_t st;
            s->w += clamp(tw - s->w, -SL_SLOPE, SL_SLOPE);
            for (st = 0; st < PH_ST; st++) {            /* y = a (u - y1) + u1 */
                y = clamp(s->px[st] + ((a * (u - s->py[st]) + 8192) >> 14), -65535, 65535);
                s->px[st] = u;
                s->py[st] = y;
                u = y;
            }
            s->fb = clamp(y, -32767, 32767);
            y <<= 2;                                    /* (y: at an eighth) */
            s->dc += ((y << 8) - s->dc) >> 9;           /* DC blocker, ~14 Hz (a clipped loud input is lopsided) */
            b[i] = pf_mix(x, (x >> 1) + y - (s->dc >> 8), s->w);   /* (dry + all-passed) / 2 */
        }
        s->ph += n * inc;
        return;
    }
    {
        int32_t chorus = s->type == IT_CHORUS, fbk = c * (chorus ? 103 : 219);   /* 0 .. 0.4 / 0 .. 0.85, Q15 */
        uint32_t dq = dep * 516u;                       /* DEPTH, Q16 */
        for (i = 0; i < n; i++, s->ph += inc) {
            int32_t x = b[i], r0, r1, v, fr;
            uint32_t d;                                 /* the delay, Q16 samples */
            s->w += clamp(tw - s->w, -SL_SLOPE, SL_SLOPE);
            if (chorus) {
                d = (uint32_t)((140 << 16) + ((osc_sine(s->ph) * (int32_t)dep * 50) >> 5));   /* 3.2 +- 2.3 ms */
            } else {
                uint32_t tr = pf_tri(s->ph), sw = (tr * tr) >> 16;   /* longer near the short end (perform.c) */
                d = (11u << 16) + 209u * (((sw >> 1) * dq) >> 15);  /* 0.25 .. 5 ms */
            }
            r0 = (int32_t)(s->wp - (d >> 16)) & (int32_t)(IL_LEN - 1u);
            r1 = (r0 - 1) & (int32_t)(IL_LEN - 1u);
            fr = (int32_t)((d >> 1) & 0x7FFFu);
            v = s->line[r0] + (((s->line[r1] - s->line[r0]) * fr) >> 15);
            s->line[s->wp] = (int16_t)clamp((x >> 2) + softclip(mulq15(v, fbk)), -32767, 32767);
            s->wp = (uint16_t)((s->wp + 1u) & (IL_LEN - 1u));
            v <<= 1;                                    /* (v: at a quarter) */
            s->dc += ((v << 8) - s->dc) >> 9;           /* DC blocker (the line's clip on a loud lopsided wave) */
            b[i] = pf_mix(x, (x >> 1) + v - (s->dc >> 8), s->w);   /* (dry + delayed) / 2 */
        }
    }
}

/* the INSERT at rest at once (mix_part: TYPE turned OFF while the part was silent, its fade never ran) */
static __attribute__((noinline)) void ins_rest(track_t *t)
{
    if (!TRK_HIDDEN(t))                                 /* (DUELL: a hidden part has no INSERT state) */
        memset(&ins[(uint32_t)(t - trk)], 0, sizeof(ins_t));
    t->ins_run = 0;
}

/* the track's INSERT on its block b (mix_part, mix_spread): nothing while TYPE is OFF or MIX is 0 and faded */
static __attribute__((noinline)) void track_insert(track_t *t, int32_t *b, uint32_t n)
{
    ins_t *s = &ins[(uint32_t)(t - trk) % NLIVE];
    int32_t ty = clamp(t->p[P_ITYPE], 0, IT_N - 1), tw;
    if (TRK_HIDDEN(t)) {
        t->ins_run = 0;
        return;
    }
    if (s->type != ty && !s->w)                         /* faded out: the new type from rest */
        s->type = (uint8_t)ty;
    tw = s->type == ty && ty ? ins_mixq(t->p[P_IMIX]) : 0;
    if (!s->w) {
        if (!tw) {
            t->ins_run = 0;
            return;                                     /* dry: bit for bit */
        }
        memset(s, 0, sizeof *s);                        /* starting: no stale line or filter */
        s->type = (uint8_t)ty;
    }
    if (s->type >= IT_PHASER)
        ins_swept(s, t, b, n, tw);
    else
        ins_drive(s, t, b, n, tw);
    t->ins_run = s->w != 0;                             /* (mix_part calls again while it fades) */
}

/* EQ (DUELL, SPEC 5c.3): one per part, after its INSERT (and DIST) and before the DUCK, the level, the pan and the sends: mono
 * per part like them (SPLIT's isolation holds; SPREAD's side signal gets the same filter, its own state). Three RBJ biquads in
 * a row: LOW shelf, MID peaking (a bell), HIGH shelf; GAIN -12..+12 dB in whole dB, FREQ a semitone step around the band's centre
 * (core.h eq_hz_q8), MID's Q 0.25 .. 4. A band at 0 dB is not run; all three at 0 (the default): the call is not made, the
 * block is not touched (bit for bit, no cost). The coefficients (Q28, 64-bit; a division each) are made when one of the part's
 * seven values changed; the filter is a transposed direct form II with 64-bit states and the output carried with 8 more bits into
 * the feedback (no limit cycle on a quiet tail). A band switched on starts from rest; all switched off, or the part silent and its
 * tail run out (eq_rest), the states are cleared. Changing a value moves the coefficients at once (no glide: a click is possible
 * on a loud, low band). */
typedef struct {
    int64_t b0, b1, b2, a1, a2;          /* Q28, divided by a0 */
} eqc_t;
typedef struct {
    int8_t key[7];                       /* LOW gain, freq, MID gain, freq, Q, HIGH gain, freq: the values the coefficients are of */
    uint8_t on;                          /* bit k: band k runs (gain not 0) */
    eqc_t c[3];
    int64_t s[2][3][2];                  /* the states: [0] the part's signal, [1] SPREAD's side; band; s1 s2 */
} eq_t;
static eq_t eqs[NLIVE];
static uint8_t eq_live;                  /* bit p: part p's states may hold a signal (eq_rest clears them) */
#define EQ_ONE (1ll << 28)
static const uint32_t EQ_A[25] = {      /* 10^(dB / 40), dB -12 .. 12, Q28: the shelf / bell amplitude A */
    134536424, 142508208, 150952350, 159896839, 169371322, 179407204, 190037749, 201298194, 213225862, 225860289,
    239243352, 253419412, 268435456, 284341257, 301189535, 319036137, 337940217, 357964434, 379175160, 401642701,
    425441527, 450650522, 477353244, 505638202, 535599149};
static const uint32_t EQ_SA[25] = {     /* 10^(dB / 80): sqrt(A) */
    190037749, 195586952, 201298194, 207176207, 213225862, 219452169, 225860289, 232455528, 239243352, 246229384,
    253419412, 260819392, 268435456, 276273913, 284341257, 292644171, 301189535, 309984429, 319036137, 328352161,
    337940217, 347808249, 357964434, 368417184, 379175160};
static const uint32_t EQ_K[25] = {      /* 2^(-r / 6), r -12 .. 12: 1 / Q */
    1073741824, 956595215, 852229450, 759250125, 676414963, 602617224, 536870912, 478297607, 426114725, 379625062,
    338207482, 301308612, 268435456, 239148804, 213057363, 189812531, 169103741, 150654306, 134217728, 119574402,
    106528681, 94906266, 84551870, 75327153, 67108864};
#define EQ_HK ((int64_t)(3.14159265358979 * 1073741824.0 / FS))   /* pi / FS, Q30: the half angle of a frequency in Hz */

/* band bd (0 LOW shelf, 1 MID bell, 2 HIGH shelf) at gain g dB, semitone step fr, MID's Q step q -> c */
static __attribute__((noinline)) void eq_design(eqc_t *c, uint32_t bd, int32_t g, int32_t fr, int32_t q)
{
    int64_t h = ((int64_t)eq_hz_q8(bd, fr) * EQ_HK) >> 10;              /* pi f / FS, Q28 (at most 1.15) */
    int64_t h2 = (h * h) >> 28, t, sn, cs, sw, cw, a0, b0, b1, b2, a1, a2;
    int64_t A = EQ_A[g + 12], SA = EQ_SA[g + 12], AM = A - EQ_ONE, AP = A + EQ_ONE, T, P1, P2;
    t = EQ_ONE - h2 / 72;                                               /* sin h, cos h (Taylor to h^9: 1e-7 at 1.15) */
    t = EQ_ONE - (h2 * t >> 28) / 42;
    t = EQ_ONE - (h2 * t >> 28) / 20;
    t = EQ_ONE - (h2 * t >> 28) / 6;
    sn = h * t >> 28;
    t = EQ_ONE - h2 / 56;
    t = EQ_ONE - (h2 * t >> 28) / 30;
    t = EQ_ONE - (h2 * t >> 28) / 12;
    cs = EQ_ONE - (h2 * t >> 28) / 2;
    sw = 2 * sn * cs >> 28;                                             /* sin w, w = 2 pi f / FS */
    cw = EQ_ONE - (2 * sn * sn >> 28);                                  /* cos w (1 - cos w kept to its own bits) */
    if (bd == 1u) {                                                     /* peaking: alpha = sin w / (2 Q) */
        int64_t al = sw * EQ_K[q + 12] >> 29, aA = al * A >> 28, aI = al * EQ_A[12 - g] >> 28;
        b0 = EQ_ONE + aA, b1 = -2 * cw, b2 = EQ_ONE - aA;
        a0 = EQ_ONE + aI, a1 = -2 * cw, a2 = EQ_ONE - aI;
    } else {                                                            /* shelf, slope 1: 2 sqrt(A) alpha = sqrt(2) sqrt(A) sin w */
        T = (SA * sw >> 28) * 379625062 >> 28;
        P1 = AM * cw >> 28;
        P2 = AP * cw >> 28;
        if (bd == 0u) {
            b0 = A * (AP - P1 + T) >> 28, b1 = 2 * A * (AM - P2) >> 28, b2 = A * (AP - P1 - T) >> 28;
            a0 = AP + P1 + T, a1 = -2 * (AM + P2), a2 = AP + P1 - T;
        } else {
            b0 = A * (AP + P1 + T) >> 28, b1 = -2 * A * (AM + P2) >> 28, b2 = A * (AP + P1 - T) >> 28;
            a0 = AP - P1 + T, a1 = 2 * (AM - P2), a2 = AP - P1 - T;
        }
    }
    c->b0 = (b0 << 28) / a0;
    c->b1 = (b1 << 28) / a0;
    c->b2 = (b2 << 28) / a0;
    c->a1 = (a1 << 28) / a0;
    c->a2 = (a2 << 28) / a0;
}

/* one band over a block, state s (s1 s2) */
static __attribute__((noinline)) void eq_run(const eqc_t *c, int64_t *s, int32_t *b, uint32_t n)
{
    uint32_t i;
    int64_t s1 = s[0], s2 = s[1];
    for (i = 0; i < n; i++) {
        int64_t x = clamp(b[i], -8388608, 8388607), y = c->b0 * x + s1;   /* Q28 */
        y = y >> 20;                                                    /* Q8 */
        y = y > 2147483647ll ? 2147483647ll : y < -2147483647ll ? -2147483647ll : y;
        s1 = c->b1 * x - ((c->a1 * y) >> 8) + s2;
        s2 = c->b2 * x - ((c->a2 * y) >> 8);
        b[i] = (int32_t)((y + 128) >> 8);
    }
    s[0] = s1, s[1] = s2;
}

/* part t's EQ on its block b (side 0: the part's signal; 1: SPREAD's side signal); not called while all three gains are 0
 * and no state is live */
static __attribute__((noinline)) void track_eq(track_t *t, int32_t *b, uint32_t n, uint32_t side)
{
    uint32_t p = (uint32_t)(t - trk) % NLIVE, k, on = 0;
    eq_t *e = &eqs[p];
    int8_t key[7];
    if (TRK_HIDDEN(t))
        return;
    key[0] = (int8_t)clamp(t->p[P_EQLG], -12, 12), key[1] = (int8_t)clamp(t->p[P_EQLF], -24, 24);
    key[2] = (int8_t)clamp(t->p[P_EQMG], -12, 12), key[3] = (int8_t)clamp(t->p[P_EQMF], -24, 24);
    key[4] = (int8_t)clamp(t->p[P_EQMQ], -12, 12);
    key[5] = (int8_t)clamp(t->p[P_EQHG], -12, 12), key[6] = (int8_t)clamp(t->p[P_EQHF], -24, 12);
    for (k = 0; k < 3u; k++)
        on |= (uint32_t)(key[2u * k] != 0) << k;
    if (!on) {                                          /* all off: rest */
        memset(e->s, 0, sizeof e->s);
        e->on = 0;
        eq_live &= (uint8_t)~(1u << p);
        return;
    }
    if (memcmp(key, e->key, sizeof key) || on != e->on) {   /* a value moved: the coefficients; a band newly on starts from rest */
        for (k = 0; k < 3u; k++)
            if (((on ^ e->on) >> k) & 1u)               /* (a band switched on or off) */
                memset(e->s[0][k], 0, sizeof e->s[0][k]), memset(e->s[1][k], 0, sizeof e->s[1][k]);
        eq_design(&e->c[0], 0u, key[0], key[1], 0);
        eq_design(&e->c[1], 1u, key[2], key[3], key[4]);
        eq_design(&e->c[2], 2u, key[5], key[6], 0);
        memcpy(e->key, key, sizeof key);
        e->on = (uint8_t)on;
    }
    eq_live |= (uint8_t)(1u << p);
    for (k = 0; k < 3u; k++)
        if ((on >> k) & 1u)
            eq_run(&e->c[k], e->s[side][k], b, n);
}

/* the part is silent and its tail has run out: the states to rest (nothing sounds; a note after starts clean) */
static __attribute__((noinline)) void eq_rest(track_t *t)
{
    uint32_t p = (uint32_t)(t - trk) % NLIVE;
    memset(eqs[p].s, 0, sizeof eqs[p].s);
    eq_live &= (uint8_t)~(1u << p);
}
/* does part t's EQ do anything now (a gain not 0) or hold a signal still */
#define EQ_ON(t) ((t)->p[P_EQLG] | (t)->p[P_EQMG] | (t)->p[P_EQHG] | ((eq_live >> ((uint32_t)((t) - trk) % NLIVE)) & 1u))

/* one block of the whole mix (shared with hostsim.c): events -> each part (with its modulation matrix)
 * -> dist -> INSERT -> EQ -> mute -> DUCK -> level / pan / sends -> buses -> master; out: stereo Q15 */
static void events_block(uint32_t n);                    /* seq.c */
static int32_t send_c[CTL], send_d[CTL], send_r[CTL], wet[CTL], mix_l[CTL], mix_r[CTL];

/* DUELL step 5 (SPEC 5b.1): the mix's MODE. dm_cur follows duell_mode (core.h; seq.c mode_apply) and dm_split is this
 * block's: SPLIT mixes part 1 (trk[0]) to the left only and every other part to the right only, with
 *   - its PAN and SPREAD (and the matrix's and the morph's PAN) not heard: the dry part at full on its side, as a centred
 *     part has it on each side in STEREO (mix_part; mix_spread never runs: spread_on 0, the voices all into b);
 *   - its sends into its own bus (A: part 1, B: part 2; split_buses), each bus's return to its own side only, folded to
 *     mono (HALL's stereo difference left out: the return's mid, as a side of STEREO gets it); the FX layer's THROW of the
 *     left channel into A, of the right into B (perform.c perf_pre_split);
 *   - the master without anything shared between the channels (master_out_split: BASS+ and the limiter per channel).
 * The DIST, the INSERT, the EQ, the FX layer's mutes and master effects are mono per part / per channel already.
 * A change between SPLIT and STEREO / LAYER: the output fades out (DM_STEP a sample, ~22 ms), the delay line is cleared
 * in DM_CHUNK pieces over the next blocks (silent: ~6 ms), then the chorus and reverb lines, the limiters and BASS+, the
 * FX layer's per-channel lines (dm_lines_clear), the old notes' tails stop (silent: seq.c mode_apply let every note go at the request), the mix switches and
 * fades in (~22 ms): ~50 ms in all, no click, no stale echo or tail of the old mode on a new side. STEREO <-> LAYER mix
 * alike: no fade, the tails ring on. Steady state: STEREO and LAYER run exactly the code they ran before (bit for bit:
 * regress, the goldens), SPLIT its own; the switch costs one compare a block */
#define DM_ONE 32768                    /* the dip's gain, Q15 */
#define DM_STEP 33                      /* .. its step a sample: 993 samples (22.5 ms) each way */
#define DM_CHUNK 8192u                  /* the delay line's samples cleared a silent block (8 blocks: 5.8 ms) */
static uint8_t dm_cur = DM_STEREO;      /* the mode the mix runs (DM_*) */
static uint8_t dm_split;                /* dm_cur == DM_SPLIT (declared above, before fx_bus.c) */
static uint8_t dm_hold;                 /* this block is silent (clearing): the buses do not run */
static int8_t dm_dir;                   /* this block's ramp: -1 down, +1 up, 0 none */
static uint8_t dm_clr;                  /* the delay line's chunks cleared so far (0: not clearing yet) */
static int32_t dm_g = DM_ONE;
static int32_t sa_c[CTL], sa_d[CTL], sa_r[CTL];   /* SPLIT: part 1's sends (bus A), the others' stay in send_* (bus B) */

/* the buses' lines and states from silence (both instances), the master's per-channel states as one: a change between
 * SPLIT and STEREO / LAYER (dm_switch, after the delay line's chunks; duell_mode_force, all of it) */
static __attribute__((noinline)) void dm_lines_clear(void)
{
    uint32_t i;
    for (i = 0; i < CHO_LEN; i++)
        cho_buf[i] = cho_buf_b[i] = 0;
    rev_clear();
    rev_clear_b();
    fx.dly_lp = fx_b.dly_lp = 0;
    fx_b.rtype = fx.rtype;                              /* (B's lines are clear: no model fade to run) */
    fx_b.sp_size = fx.sp_size;
    hl_b.sz = 0;
    lim_env = lim_env_r = LIM_T;                        /* the limiters and BASS+ from rest (they held the old mix's */
    memset(&sb, 0, sizeof sb);                          /* levels; at the silent point nothing to glide from) */
    memset(&sb_r, 0, sizeof sb_r);
    /* the FX layer's per-channel lines and states (they hold the old mix: a part's sound on the other side): FLANGER's
     * line, PHASER's stages, the filters, CRUSH's held samples; a buffer effect held records anew (perform.c) */
    memset(fl_buf, 0, sizeof fl_buf);
    pf.fn = 0;
    memset(pf.px, 0, sizeof pf.px);
    memset(pf.py, 0, sizeof pf.py);
    pf.pfb[0] = pf.pfb[1] = 0;
    memset(pf.lz, 0, sizeof pf.lz);
    memset(pf.hz, 0, sizeof pf.hz);
    pf.chl = pf.chr = 0;
    if (pf.mode != BM_NONE) {
        if (pf.src != PF_N)
            perf_buf_start(pf.src);
        else {                                          /* (fading out: gone at once, the buffer back) */
            pf.w = 0;
            perf_buf_done();
        }
    }
}

/* the mode now, at once (no fade, no note let go): power-on, the tests. A change between SPLIT and the others clears the
 * buses (no line holds what the other mix put there) */
static void duell_mode_force(uint32_t m)
{
    m = m < DM_N ? m : DM_SPLIT;
    if ((m == DM_SPLIT) != (dm_cur == DM_SPLIT) || dm_clr) {
        memset(dly_buf, 0, sizeof dly_buf);
        dm_lines_clear();
    }
    duell_mode_req = duell_mode = dm_cur = (uint8_t)m;
    dm_split = m == DM_SPLIT;
    dm_g = DM_ONE;
    dm_dir = 0;
    dm_clr = 0;
    dm_hold = 0;
}

/* at the silent point of a SPLIT <-> STEREO switch (the delay line cleared already): the other lines and states, the
 * old notes' tails stopped (every voice let go: seq.c mode_apply let them all go at the request; a voice held now is a
 * note played since, in the new mode: it fades in with the mix); then the new mode */
static __attribute__((noinline)) void dm_switch(void)
{
    uint32_t i, k;
    dm_lines_clear();
    for (k = 0; k < NPART; k++)
        for (i = 0; i < NVOICE; i++)
            if (trk[k].v[i].active && !trk[k].v[i].gate)
                voice_kill(&trk[k].v[i]);
    dm_cur = duell_mode;
    dm_split = dm_cur == DM_SPLIT;
}

/* once a block, before the parts, while the mix and duell_mode differ or the dip is under way */
static __attribute__((noinline)) void dm_step(void)
{
    uint32_t want = duell_mode;
    dm_hold = 0;
    dm_dir = 0;
    if (!dm_clr && (want == DM_SPLIT) == dm_split) {    /* STEREO <-> LAYER (the same mix), or back before the silence */
        dm_cur = (uint8_t)want;
        dm_dir = dm_g < DM_ONE ? 1 : 0;
        return;
    }
    if (dm_g > 0 && !dm_clr) {                          /* fading out */
        dm_dir = -1;
        return;
    }
    dm_hold = 1;                                        /* silent: the delay line cleared a chunk a block */
    memset(dly_buf + (uint32_t)dm_clr * DM_CHUNK, 0, DM_CHUNK * sizeof dly_buf[0]);
    if (++dm_clr < DLY_LEN / DM_CHUNK)
        return;
    dm_clr = 0;
    dm_switch();
}

/* the dip's ramp over the block (mix_l, mix_r and the returns, before the master; with the FX layer at work after it:
 * perf_master) */
static __attribute__((noinline)) void dm_ramp(uint32_t n)
{
    uint32_t i;
    int32_t g = dm_g;
    for (i = 0; i < n; i++) {
        g = clamp(g + dm_dir * DM_STEP, 0, DM_ONE);
        mix_l[i] = mulq16(mix_l[i], (uint32_t)g << 1);
        mix_r[i] = mulq16(mix_r[i], (uint32_t)g << 1);
        wet[i] = mulq16(wet[i], (uint32_t)g << 1);
    }
    dm_g = g;
    if (g == DM_ONE)
        dm_dir = 0;                                     /* (faded in: the next blocks skip all of this) */
}

/* SPLIT: the delay time halved until it fits a half of the line (a note value of the tempo stays one: 1/4 -> 1/8 ..) */
static uint32_t delay_split(void)
{
    uint32_t s = delay_samples();
    while (s >= DLY_HALF)
        s >>= 1;
    return s;
}

/* SPLIT: part 1's sends (mix_part put them into send_*) to bus A's, send_* cleared for the other parts (bus B) */
static __attribute__((noinline)) void split_stash(uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        sa_c[i] = send_c[i];
        sa_d[i] = send_d[i];
        sa_r[i] = send_r[i];
        send_c[i] = send_d[i] = send_r[i] = 0;
    }
}

/* SPLIT: bus A on part 1's sends, its return (the mid) to the left; bus B on part 2's, to the right; wet stays 0 */
static __attribute__((noinline)) void split_buses(uint32_t n)
{
    uint32_t i, dl = delay_split();
    fx_buses_m(DLY_HALF - 1u, dl, sa_c, sa_d, sa_r, wet, n, 0u, 0);
    for (i = 0; i < n; i++)
        mix_l[i] += wet[i];
    fx_buses_m_b(DLY_HALF - 1u, dl, send_c, send_d, send_r, wet, n, 0u, 0);
    for (i = 0; i < n; i++) {
        mix_r[i] += wet[i];
        wet[i] = 0;
    }
}

/* the end of a SPLIT block: the master level, then master_out_split (the FX layer's, perf_master, alike) */
static __attribute__((noinline)) void split_master(int32_t *out, uint32_t n, int32_t mg)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t l = ((mix_l[i] >> 2) * mg) >> 10;
        int32_t r = ((mix_r[i] >> 2) * mg) >> 10;
        master_out_split(&l, &r);
        out[2u * i] = l;
        out[2u * i + 1u] = r;
    }
}

/* SPREAD (#148): a POLY or UNISON part whose SPRD is above 0. Its voices start left and right of PAN in turn (voice.c
 * voice_start; UNISON's detuned voices alternate), at PAN -/+ SPRD / 2 (SPRD 127: hard left and right), with the pan
 * law PAN has (the far side down, the near one as it is). track_render gives every voice (b, as always) and the left
 * ones (spread_buf); here d = left - right, the side signal, so for a pair of pans L = b (gla + glb) / 2 + d (gla - glb)
 * / 2 (R alike): the mono sum is b's, the level of a centre pan less by SPRD / 256 at most. b goes through DIST and the
 * FX layer's mute key as always; d gets the mute's ramp (perf_mute_sd) and passes DIST by (the spread of a distorted chord is its voices' clean difference). The sends
 * take b, mono, as before. SPRD 0, MONO or LEGATO: mix_part's own loop, bit for bit as before. The cost: a block's
 * clear and sum of spread_buf, the side and a wider mix loop, per part; per voice none (it picks its buffer) */
static void duck_run(track_t *t, int32_t *b, int32_t *d, uint32_t n);
/* BALANCE (core.h mix_bal, the MODE page): part 1's level 1 for BALANCE <= 0, else 1 - bal / 100; part 2's 1 for >= 0, else
 * 1 + bal / 100 (linear; turning right fades part 1 out). 0: lvl untouched. In every mode, at the part's level stage */
static int32_t bal_lvl(const track_t *t, int32_t lvl)
{
    int32_t b = mix_bal;
    if (t == trk) {
        if (b > 0)
            lvl = lvl * (100 - b) / 100;
    } else if (b < 0) {
        lvl = lvl * (100 + b) / 100;
    }
    return lvl;
}
static __attribute__((noinline)) void mix_spread(track_t *t, int32_t *b, uint32_t n)
{
    int32_t *d = spread_buf;
    uint32_t i;
    int32_t lvl = bal_lvl(t, LEVEL_Q12[t->p[P_LEVEL] & 127]), pan = t->p[P_PAN], s = (t->p[P_SPRD] + 1) >> 1;
    int32_t pa = clamp(pan - s, -64, 63), pb = clamp(pan + s, -64, 63);
    int32_t gla = 4096 - (pa > 0 ? pa * 64 : 0), gra = 4096 + (pa < 0 ? pa * 64 : 0);
    int32_t glb = 4096 - (pb > 0 ? pb * 64 : 0), grb = 4096 + (pb < 0 ? pb * 64 : 0);
    int32_t glm = (gla + glb) >> 1, grm = (gra + grb) >> 1, gld = (gla - glb) >> 1, grd = (gra - grb) >> 1;
    int32_t c = t->p[P_CHOR] * 258, dl = t->p[P_DLY] * 258, r = t->p[P_REV] * 258, pk = t->peak;
    int32_t xmax = c > dl ? c : dl;
    xmax = 0x7FFFFFFF / ((xmax > r ? xmax : r) | 1);
    for (i = 0; i < n; i++)
        d[i] = (d[i] << 1) - b[i];                      /* left - right */
    track_dist(t, b, n);
    if (t->p[P_ITYPE] | t->ins_run)
        track_insert(t, b, n);                          /* (the mono sum, as DIST) */
    if (EQ_ON(t)) {                                     /* the EQ: the sum and the side alike */
        track_eq(t, b, n, 0u);
        track_eq(t, d, n, 1u);
    }
    if ((pf.mute >> (t - trk)) & 1u)
        perf_mute_sd((uint32_t)(t - trk), b, d, n);
    if (duck_st[(uint32_t)(t - trk) % NTRK].ph)
        duck_run(t, b, d, n);                           /* the DUCK (DUELL): the sum and the side alike */
    for (i = 0; i < n; i++) {
        int32_t x = ((b[i] >> 2) * lvl) >> 10, a = x < 0 ? -x : x, y = ((d[i] >> 2) * lvl) >> 10;
        int32_t xs = clamp(x, -xmax, xmax);
        if (a > pk)
            pk = a;
        if (c)
            send_c[i] += mulq15(xs, c);
        if (dl)
            send_d[i] += mulq15(xs, dl);
        if (r)
            send_r[i] += mulq15(xs, r);
        mix_l[i] += ((x * glm) >> 12) + ((y * gld) >> 12);
        mix_r[i] += ((x * grm) >> 12) + ((y * grd) >> 12);
    }
    t->peak = pk;
}

/* DUELL DUCK (SPEC 5c.4): the part's sidechain; the trigger is ONE setting, SRC (P_DKSRC: 0 PUMP, 1 PART, 2..17 CH1..16; two pages,
 * DUCK 1: SRC, the context knob RATE / THR / NOTE of the SRC (params.c duck_ctx_id), DEPTH, ATK; DUCK 2: HOLD, RLS, CURVE LIN / EXP).
 * With SRC a channel (CHn): a note-on on that MIDI channel whose note is DUCK
 * NOTE (P_DKNOTE: 0 ANY, n the note n: 1..127) starts the envelope (duck_st, mod.c): the gain goes down by DEPTH % in ATK ms, stays HOLD
 * ms, comes back to full in RLS ms (linear in the gain, or CURVE EXP: a share of what is left, 3 time constants in RLS); a new note-on goes down again from where the gain is. Note-offs are
 * ignored. DEPTH 0 or an envelope at rest: nothing, no sample touched. The latency is under a block: a note-on is
 * read at the block's start (events_block), the envelope runs from its first sample. The gain multiplies the part's block
 * where its level does (mix_part / mix_spread: after INSERT, EQ, DIST and mute, before the level, pan and the sends)
 * and runs on while the part is silent. Which notes play: a note matching a part's trigger is a trigger and no more when
 * NOTE is one note, or when the channel plays no part (midi_event: `plays`); with NOTE ANY on a channel that plays a part
 * (its own, or part 1's in LAYER, or any under SEL routing) the notes also play and trigger. Each part has its own
 * settings, so one channel can duck both parts, or two channels one part each. */
static uint32_t duck_note(uint32_t ch, uint32_t note, uint32_t on, uint32_t plays)
{
    uint32_t i, swallow = 0;
    for (i = 0; i < NSHOWN; i++) {
        int32_t c = trk[i].p[P_DKSRC], n = trk[i].p[P_DKNOTE];
        if (c != (int32_t)ch + 2 || (n && note != (uint32_t)n))   /* (SRC 2..17 = CH1..16) */
            continue;
        if (on && trk[i].p[P_DKDEP] > 0)
            duck_st[i].ph = 1;
        swallow |= (uint32_t)(n != 0) | (plays ^ 1u);
    }
    return swallow;
}
/* SRC, the trigger source. CHn: the notes above, NOTE. PUMP (stored 0, the default): no notes, a duck starts
 * on every RATE division (N_DIV, 1/4 = four on the floor) of the tempo, the same envelope (ATK, HOLD, RLS). It counts the
 * samples as the arp does (events_block: seq.c arp_step): CLK INT: n a block, free-running, the transport or not, on
 * div_samples (BPM); CLK USB / TRS: the samples the incoming MIDI clock advances (midi_clock_advance), so it follows
 * the external tempo and stands still while no clock runs (a stopped transport). PLAY / MIDI Start (seq_start, duck_start)
 * restarts the phase: a duck on the first block, then every division. A duck starts at the block's start (under a block
 * of latency). PART: a keyed compressor with the other part (1 by 2, 2 by 1) as the key: duck_part follows the other
 * part's level once a block (mix_part: its render times its LEVEL, before its own DIST, INSERT, EQ and duck, so the two
 * never key each other round; attack ~2 ms, release ~60 ms), the part just rendered's, or the last block's of the part
 * still to come (one block of latency). The reduction asked = DEPTH x clamp((key - THR) / (full - THR)), the full level
 * DUCK_KEY_FS (a part's x at 16384 of 32768); the gain goes down by ATK (the DEPTH-wide slope), up by RLS, and HOLD keeps the
 * reduction after the key falls under THR. The cost: nothing but a test of SRC in mix_part; a part used as a key, one
 * pass over its block's abs; a keyed part, a few multiplies a block and duck_run. MIDI notes do not trigger a part whose SRC is not a CHn. */
#define DUCK_KEY_FS 16384
static void duck_start(void)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++)
        duck_st[i].run = 0;
}
static __attribute__((noinline)) void duck_pump(uint32_t adv)
{
    uint32_t i;
    for (i = 0; i < NSHOWN; i++) {
        duck_t *k = &duck_st[i];
        uint32_t period;
        if (trk[i].p[P_DKSRC] != 0 || trk[i].p[P_DKDEP] <= 0) {
            k->run = 0;
            continue;
        }
        if (!adv)
            continue;                                   /* (an external clock not running) */
        period = div_samples((uint32_t)clamp(trk[i].p[P_DKRATE], 0, 16));
        period = period ? period : 1u;
        if (!k->run) {
            k->run = 1;
            k->pos = 0;
            k->ph = 1;
            continue;
        }
        k->pos += adv;
        if (k->pos >= period) {
            k->pos -= period;
            if (k->pos >= period)
                k->pos = 0;
            k->ph = 1;
        }
    }
}
/* t's block b (0: silent), n samples: the key follower for the other part, and this part's reduction asked by it */
static __attribute__((noinline)) void duck_part(track_t *t, const int32_t *b, uint32_t n)
{
    uint32_t id = (uint32_t)(t - trk) % NTRK, o = id ^ 1u, i;
    duck_t *k = &duck_st[id], *ok = &duck_st[o];
    if (trk[o].p[P_DKSRC] == 1) {                       /* the other part is keyed by this one */
        int32_t pk = 0, c;
        if (b)
            for (i = 0; i < n; i++) {
                int32_t a = b[i] < 0 ? -b[i] : b[i];
                if (a > pk)
                    pk = a;
            }
        pk = ((pk >> 2) * LEVEL_Q12[t->p[P_LEVEL] & 127]) >> 10;
        pk = pk > 32767 ? 32767 : pk;
        c = (int32_t)(n * 32768u / (n + (pk > k->key ? 88u : 2646u)));   /* 2 ms up, 60 ms down */
        k->key += ((pk - k->key) * c) >> 15;
    }
    if (t->p[P_DKSRC] == 1 && t->p[P_DKDEP] > 0) {
        int32_t thr = clamp(t->p[P_DKTHR], 0, 100) * (DUCK_KEY_FS / 100), kv = ok->key, raw = 0;
        int32_t top = (int32_t)((uint32_t)clamp(t->p[P_DKDEP], 0, 100) << 16) / 100;
        if (kv > thr && thr < DUCK_KEY_FS) {
            int32_t f = ((kv - thr) << 12) / (DUCK_KEY_FS - thr);
            raw = (top * (f > 4096 ? 4096 : f)) >> 12;
        }
        k->on = raw > 0;
        if (raw > 0) {
            k->tg = raw;
            k->cnt = (uint32_t)clamp(t->p[P_DKHLD], 0, 100) * 220u;
        } else if (!k->cnt)
            k->tg = 0;
        if (raw > 0 || k->tg || k->dn)
            k->ph = 4;
    } else if (k->ph == 4)
        k->ph = 3;
}
/* b (and the side signal d of mix_spread) times the gain, n samples; b 0: the envelope only */
static __attribute__((noinline)) void duck_run(track_t *t, int32_t *b, int32_t *d, uint32_t n)
{
    duck_t *k = &duck_st[(uint32_t)(t - trk) % NTRK];
    int32_t top = (int32_t)((uint32_t)clamp(t->p[P_DKDEP], 0, 100) << 16) / 100;
    int32_t as = top / (clamp(t->p[P_DKATK], 1, 100) * 441 / 10), rs = top / (int32_t)(duck_rls_ms(t->p[P_DKRLS]) * 441u / 10u);
    int32_t ek = t->p[P_DKCRV] ? (int32_t)(31457280u / (duck_rls_ms(t->p[P_DKRLS]) * 441u)) : 0;   /* CURVE EXP: dn x ek / 2^20 a sample */
    uint32_t i, hold = (uint32_t)clamp(t->p[P_DKHLD], 0, 100) * 220u;
    if (as < 1)
        as = 1;
    if (rs < 1)
        rs = 1;
    if (k->ph == 4 && t->p[P_DKSRC] != 1)
        k->ph = 3;
    for (i = 0; i < n; i++) {
        if (k->ph == 4) {                               /* SRC PART: towards the asked reduction */
            if (!k->on && k->cnt && !--k->cnt)
                k->tg = 0;
            if (k->dn < k->tg) {
                k->dn += as;
                if (k->dn > k->tg)
                    k->dn = k->tg;
            } else if (k->dn > k->tg) {
                k->dn -= ek ? (((k->dn * ek) >> 20) | 1) : rs;
                if (k->dn < k->tg)
                    k->dn = k->tg;
            }
            if (!k->dn && !k->tg && !k->on)
                k->ph = 0;
        } else if (k->ph == 1) {
            k->dn += as;
            if (k->dn >= top) {
                k->dn = top;
                k->ph = 2;
                k->cnt = hold;
            }
        } else if (k->ph == 2) {
            if (!k->cnt)
                k->ph = 3;
            else
                k->cnt--;
        } else if (k->ph == 3) {
            k->dn -= ek ? (((k->dn * ek) >> 20) | 1) : rs;   /* (CURVE EXP: a share of what is left, 3 time constants in RLS) */
            if (k->dn <= 0) {
                k->dn = 0;
                k->ph = 0;
            }
        }
        if (b && k->dn) {
            int32_t g = (65536 - k->dn) >> 4;
            b[i] = ((b[i] >> 4) * g) >> 8;
            if (d)
                d[i] = ((d[i] >> 4) * g) >> 8;
        }
    }
}

/* one synth part into the dry mix and the sends; a part with no voice sounding costs
 * the LFO tick and a cleared buffer only (after the DIST tail has run out) */
static void mix_part(track_t *t, uint32_t n)
{
    int32_t *b = part_buf;
    uint32_t i;
    mod_begin(t);                                       /* the matrix's per-block values into t->p (mod.c) */
    spread_on = t->p[P_SPRD] > 0 && (trk_vmode(t) == V_POLY || trk_vmode(t) == V_UNISON) && !dm_split;   /* (SPREAD,
                                                         * mix_spread; SPLIT: none, the part is mono on its side) */
    if (track_render(t, b, n))
        t->tail = t->p[P_ITYPE] ? 64 : 16;              /* blocks of DIST / INSERT state to run out after the last
                                                         * voice (the INSERT's: 46 ms, a flanger's feedback) */
    else if (!t->tail || !(t->p[P_DIST] | t->p[P_ITYPE] | t->p[P_EQLG] | t->p[P_EQMG] | t->p[P_EQHG]) || !--t->tail) {
        if (eq_live)
            eq_rest(t);                                 /* (the EQ's states clear with the tail) */
        if (t->p[P_DKSRC] == 1 || trk[(uint32_t)(t - trk) ^ 1u].p[P_DKSRC] == 1)
            duck_part(t, 0, n);                         /* (SRC PART: a silent key, and the part keyed by the other) */
        if (duck_st[(uint32_t)(t - trk) % NTRK].ph)
            duck_run(t, 0, 0, n);                       /* (the DUCK's envelope runs on) */
        if (t->ins_run && !t->p[P_ITYPE])
            ins_rest(t);                                /* TYPE OFF while silent: no fade-out left for the next note */
        if (mod.on)
            mod_end(t);
        return;
    }
    if (t->p[P_DKSRC] == 1 || trk[(uint32_t)(t - trk) ^ 1u].p[P_DKSRC] == 1)
        duck_part(t, b, n);                             /* DUCK SRC PART: this block's level for the other part, its key for this */
    if (spread_on)
        mix_spread(t, b, n);
    else {
        track_dist(t, b, n);
        if (t->p[P_ITYPE] | t->ins_run)
            track_insert(t, b, n);                      /* the INSERT (1.5): off, nothing */
        if (EQ_ON(t))
            track_eq(t, b, n, 0u);                      /* the EQ (DUELL): all bands at 0 dB, nothing */
        if ((pf.mute >> (t - trk)) & 1u)
            perf_mute((uint32_t)(t - trk), b, n);       /* perform.c: a black key in the FX layer */
        if (duck_st[(uint32_t)(t - trk) % NTRK].ph)
            duck_run(t, b, 0, n);                       /* the DUCK (DUELL): before the level, pan and the sends */
    }
    if (!spread_on) {                                   /* (the mix's values read after the calls: none is held over
                                                         * them, no spill in the loop: the ISR's cost as before 1.5) */
        int32_t lvl = bal_lvl(t, LEVEL_Q12[t->p[P_LEVEL] & 127]), pan = t->p[P_PAN];
        int32_t gl = 4096 - (pan > 0 ? pan * 64 : 0), gr = 4096 + (pan < 0 ? pan * 64 : 0);
        int32_t c = t->p[P_CHOR] * 258, d = t->p[P_DLY] * 258, r = t->p[P_REV] * 258, pk = t->peak;
        if (dm_split) {                                 /* SPLIT: part 1 left, the others right, PAN not heard */
            gl = t == trk ? 4096 : 0;
            gr = 4096 - gl;
        }
        int32_t xmax = c > d ? c : d;
        xmax = 0x7FFFFFFF / ((xmax > r ? xmax : r) | 1);   /* sends: loud chords at a high LEVEL */
        for (i = 0; i < n; i++) {
            int32_t x = ((b[i] >> 2) * lvl) >> 10, a = x < 0 ? -x : x;   /* pre-shift: 8 loud voices */
            int32_t xs = clamp(x, -xmax, xmax);         /* sends: mulq15 would overflow */
            if (a > pk)
                pk = a;
            if (c)
                send_c[i] += mulq15(xs, c);
            if (d)
                send_d[i] += mulq15(xs, d);
            if (r)
                send_r[i] += mulq15(xs, r);
            mix_l[i] += (x * gl) >> 12;
            mix_r[i] += (x * gr) >> 12;
        }
        t->peak = pk;
    }
    if (mod.on)
        mod_end(t);                                     /* the stored values back */
}

/* the HOME oscilloscope (ui_graph.c graph_scope): every other sample of the left channel. MENU > DISPLAY > SCOPE (1.2,
 * Discussion #165): OUT (0, the default) what goes out, after MASTER (audio.c audio_block, before the click); MIX the
 * mix before MASTER (and before the FX layer, SPEAKER EQ and the limiter), at the level MASTER at its top gives, so its
 * size does not follow the volume. The click is in neither */
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N];
static uint32_t scope_w;
static volatile uint8_t scope_mix;
static __attribute__((noinline)) void scope_take_mix(uint32_t n)
{
    uint32_t i;
    for (i = 1; i < n; i += 2u)                         /* (((x >> 2) * MASTER_FULL) >> 10 is x) */
        scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)clamp(mix_l[i] + wet[i], -32767, 32767);
}

/* DUELL GLO's level meters (ui_layer.c glo_meters). STEREO and LAYER read each part's own peak, t->peak (mix_part, kept anyway).
 * SPLIT also scans the two buses after their effect returns (mix_l: part 1's side, mix_r: part 2's) while the page shows: the UI
 * sets meter_on, every block takes one off, so the scan stops by itself a moment after the page is left. meter_bus: the running
 * maximum |sample| of each side, cleared by the UI when it reads it (before MASTER, Q15 where 32768 is 0 dBFS) */
static volatile int32_t meter_bus[2];
static volatile uint8_t meter_on;
static __attribute__((noinline)) void meter_scan(uint32_t n)
{
    uint32_t i;
    int32_t a = meter_bus[0], b = meter_bus[1];
    for (i = 0; i < n; i++) {
        int32_t l = mix_l[i], r = mix_r[i];
        l = l < 0 ? -l : l;
        r = r < 0 ? -r : r;
        a = l > a ? l : a;
        b = r > b ? r : b;
    }
    meter_bus[0] = a;
    meter_bus[1] = b;
}

/* the master with the FX layer's effects between its level and master_out (perform.c) */
static __attribute__((noinline)) void perf_master(int32_t *out, uint32_t n)
{
    uint32_t i;
    int32_t mg = fx_usb_fixed ? MASTER_FULL : (int32_t)song.master_q12;   /* (USB LEVEL FIXED: MASTER after) */
    for (i = 0; i < n; i++) {
        mix_l[i] = (((mix_l[i] + wet[i]) >> 2) * mg) >> 10;
        mix_r[i] = (((mix_r[i] + wet[i]) >> 2) * mg) >> 10;
    }
    perf_block(mix_l, mix_r, n);
    if (dm_g != DM_ONE || dm_dir)
        dm_ramp(n);                                     /* (the MODE's dip after the FX layer: its buffer too) */
    if (dm_split) {                                     /* (SPLIT: nothing shared between the channels) */
        for (i = 0; i < n; i++) {
            int32_t l = mix_l[i], r = mix_r[i];
            master_out_split(&l, &r);
            out[2u * i] = l;
            out[2u * i + 1u] = r;
        }
        return;
    }
    for (i = 0; i < n; i++) {
        int32_t l = mix_l[i], r = mix_r[i];
        master_out(&l, &r);
        out[2u * i] = l;
        out[2u * i + 1u] = r;
    }
}

static void mix_block(int32_t *out, uint32_t n)
{
    uint32_t i;
    int perf;
    int32_t mg = fx_usb_fixed ? MASTER_FULL : (int32_t)song.master_q12;   /* (USB LEVEL FIXED: MASTER after) */
    for (i = 0; i < n; i++)
        send_c[i] = send_d[i] = send_r[i] = mix_l[i] = mix_r[i] = 0;
    events_block(n);
    if (dm_cur != duell_mode || dm_g != DM_ONE || dm_clr)
        dm_step();                                      /* DUELL step 5: the MODE changing (the dip) */
    perf = perf_begin(n);                               /* the FX hold layer at work (perform.c) */
    for (i = 0; i < NLIVE; i++) {                      /* (DUELL: tracks 3..4 do not render) */
        int mo = morph_begin(&trk[i], 1);               /* DUELL MORPH: the fader's values into t->p (morph.c) */
        mix_part(&trk[i], n);
        if (mo)
            morph_end(&trk[i]);                         /* .. and the stored ones back */
        if (!i && dm_split)
            split_stash(n);                             /* (SPLIT: part 1's sends to bus A) */
    }
    if (dm_hold) {                                      /* (the dip's silence: the lines being cleared) */
        for (i = 0; i < n; i++)
            wet[i] = 0;
    } else if (dm_split) {
        if (perf)
            perf_pre_split(mix_l, mix_r, sa_d, sa_r, send_d, send_r, n);
        split_buses(n);
    } else {
        if (perf)
            perf_pre(mix_l, mix_r, send_d, send_r, n);
        fx_buses(send_c, send_d, send_r, wet, n);
        if (hl.side)
            rev_side_mix(mix_l, mix_r, n);              /* HALL: stereo */
        if (dly_side)
            dly_side_mix(mix_l, mix_r, n);              /* PING-PONG: the repeats left and right */
    }
    if (meter_on) {
        meter_on--;
        if (dm_split)
            meter_scan(n);                              /* (GLO's page shows: SPLIT's two buses) */
    }
    if (scope_mix)
        scope_take_mix(n);
    if (perf) {
        perf_master(out, n);                            /* (the dip: there, after the FX layer) */
        return;
    }
    if (dm_g != DM_ONE || dm_dir)
        dm_ramp(n);
    if (dm_split) {
        split_master(out, n, mg);
        return;
    }
    for (i = 0; i < n; i++) {
        int32_t l = (((mix_l[i] + wet[i]) >> 2) * mg) >> 10;
        int32_t r = (((mix_r[i] + wet[i]) >> 2) * mg) >> 10;
        master_out(&l, &r);
        out[2u * i] = l;
        out[2u * i + 1u] = r;
    }
}
