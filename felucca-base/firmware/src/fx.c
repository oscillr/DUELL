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
    uint32_t dm_pl, dm_pr;               /* the delay's modulation (fx_bus.c dly_mod): the two LFOs' phases (left, right tap) */
    int32_t dm_xl, dm_xr, dm_dev;        /* .. the two taps' offsets now (Q16 samples) and the depth reached (Q8 samples; glides, 0 = at rest) */
    int32_t cho_dq;                      /* the chorus's depth reached (Q8; fx_bus.c: it glides over a block, no jump of the read point) */
    uint8_t cho_on;                      /* .. cho_dq holds one (0: the first block takes its depth at once) */
    uint16_t comb_i[4], ap_i[2];
    int32_t comb_lp[4];
    int32_t lo_lp, lo_k, lo_g;           /* the reverb's LOW CUT (fx_bus.c rev_locut): the low-pass (Q12), its coefficient now (Q20) and the filter's share (Q12; 0 = off) */
    uint8_t rtype;                       /* the reverb model running (G_RTYPE: 0 ROOM, 1 SPRING, 2 HALL, 3 SHIM, 4 RESO, 5 PLATE) */
    uint16_t pl_w[10];                   /* PLATE: its ten lines' write indices (rev_plate), */
    int32_t pl_lp[2], pl_le[2];          /* .. the two loops' low-passes and their steps' remainders, */
    uint32_t pl_ph1, pl_ph2;             /* .. the two read points' sines (0.5 Hz, 0.3 Hz) */
    uint32_t sh_w, sh_ph;                /* SHIM (ported from JIANT): its line's write count, the grains' phase */
    int32_t sh_hp, sh_he;                /* .. its output's low cut (~14 Hz) and that step's remainder */
    uint32_t sh_pu, sh_pd;               /* .. 0.2.19 PITCH / MOD (fx_bus.c rev_shimx): the up and the down grains' phase (Q16 of SH_LEN), */
    uint32_t sh_l1, sh_l2;               /* .. the MOD's two sines, */
    int32_t sh_g;                        /* .. the return's gain (Q15: it fades out and in over a change of PITCH), */
    uint8_t sh_x, sh_cfg;                /* .. sh_x 1: rev_shimx runs (0: the plain loop, the state above is from rest), sh_cfg the PITCH it plays (SHP_*) */
    int32_t rs_lp[4];                    /* RESO (ported from JIANT): the combs' loop low-passes */
    uint16_t rs_w;                       /* .. their write index (each its own line, RS_LEN) */
    uint16_t sp_w;                       /* SPRING: the loop's write index (SP_MASK) */
    int32_t sp_lp, sp_hp, sp_he, sp_size;   /* .. its loop low-pass, low cut (and its remainder), the loop
                                             * length (Q8, glides) */
    uint32_t sp_ph;                      /* .. the output tap's wobble */
    uint16_t cho_z;                      /* 0.2.21 (fx_bus.c cho_zrun): the chorus line's run of zero samples written (saturates at CHO_LEN: the whole line is 0) */
    uint8_t cho_zs;                      /* .. 1: this block's chorus input is all 0 and the whole line was 0 before it: fx_buses_m leaves the chorus out (its output is 0) */
} fxs_t;
static fxs_t fx;
static int32_t lo_buf[CTL];             /* the reverb low cut's output block (fx_bus.c rev_locut; bus A and B run one after the other) */
/* PERFORM's K2 SPACE (DUELL 0.2.5; perform.c, the knob ui_perform.c): a macro on the shared buses, applied to the values they use (song.g,
 * the patch's and the multi's, is never written). While SPACE is off its middle (or gliding back to it: sv.on) the buses read these
 * instead of song.g's (fx_bus.c SV_*); in the middle (sv.on 0) every bus reads song.g as ever: bit for bit. space_step (below, after
 * perform.c) makes them a block at a time; s = SPACE / 100 (-1 .. 1), each value from the stored one:
 *   the sends into the chorus, the delay and the reverb (every part's, both buses in DUEL)   x (1 - 0.85 |s|) left .. x (1 + 0.6 s) right
 *   the reverb's SIZE (its decay; ROOM, SPRING, HALL, SHIM, RESO each read it as ever)       r - 0.75 r |s| left .. r + (150 - r) s right
 *   the delay's FDBK                                                                        x (1 - 0.75 |s|) left .. up to 0.92 right
 *   the chorus's DEPTH                                                                      x (1 - 0.8 |s|) left .. up to the knob's largest right
 * Not the delay's TIME (0.2.9, the owner: a moving delay time bends the echoes' pitch, it sounded weird and glitchy). RESO to the right: its
 * SIZE no higher than SV_RESO_TOP (or the stored SIZE if that is higher) and its send not above x 1 (space_sends): with SIZE 150 (0.9992) and
 * the send x 1.6 its four tuned combs rang into their int16 limit, a hard clip inside the loop, on a held chord (5 % of the comb cells at the
 * limit at any moment; now 0.3 %: tests/perform_test.c). */
static struct {
    uint8_t on;                          /* SPACE off its middle, or gliding back: the values below are the buses' */
    int32_t spc;                         /* SPACE now, Q8 (-25600 .. 25600: -100 .. 100), gliding to K2's (0.29 s over the range) */
    int32_t spc0;                        /* .. the last block's (the stereo width's ramp: space_width) */
    int32_t rsize;                       /* the reverb's SIZE as the models read it (past 127 to the right: the models cap their gains) */
    int32_t dfb;                         /* the delay's feedback (Q15: FDBK x 230 as stored) */
    int32_t cdep;                        /* the chorus's depth (CDEPTH x 6 as stored) */
    int32_t sg, sg0;                     /* the sends' gain (Q15), this block's and the last one's (a ramp between them) */
    int32_t ac, ad, ar;                  /* the send floors to the right (0.2.9): every part's chorus, delay and reverb send at least this
                                          * (Q15 of a send's range, 32766 = send 127; space_tap) */
} sv;
#define SV_ADD_C 7864                            /* SPACE +100's send floor (every send at least this): chorus 24 % of a send's range (send 30 of 127) */
#define SV_ADD_D 11796                           /* .. delay 36 % (send 46) */
#define SV_ADD_R 19661                           /* .. reverb 60 % (send 76) */
#define SV_RESO_ADD 43                           /* .. RESO's reverb floor: 43 / 256 of it (10 %, send 13): its tuned combs rang into their int16 limit */
#define SV_SHIM_ADD 128                          /* .. SHIM's: 128 / 256 (30 %, send 38): with the part's own send on top its ROOM combs did */
static int32_t sp_fc[CTL], sp_fd[CTL], sp_fr[CTL];   /* SPACE to the right: what the parts' send floors add to the chorus, delay, reverb */
static int32_t sp_fc_a[CTL], sp_fd_a[CTL], sp_fr_a[CTL];   /* .. DUEL's part 1's (bus A) */
static uint8_t sp_tap;                           /* this block the parts add their floors (space_tap: SPACE right of the middle) */
#define SV_KNEE 1638                             /* the floor's soft knee: 5 % of a send's range either side (Q15 of the range) */
#define SV_RSIZE (sv.on ? sv.rsize : song.g[G_RSIZE])
#define SV_DFB (sv.on ? sv.dfb : song.g[G_DFDBK] * 230)
#define SV_CDEP (sv.on ? sv.cdep : song.g[G_CDEPTH] * 6)
#define SV_CDEP_TOP (127 * 6)                    /* SPACE: the chorus's largest DEPTH to the right, the knob's own (0.2.9; 2x it, 0.2.5 .. 0.2.8, made
                                                  * a +-0.8 semitone wobble on ACID ETHER's chorus, M06 since 0.2.13); a stored depth above it is kept */
#define SV_RESO_TOP 100                          /* SPACE: RESO's largest SIZE to the right (its combs' loop 0.952: fx_bus.c rev_reso) */
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
#define SH_FADE 74                       /* PITCH changed (0.2.19): the return fades out and in, 32767 / 74 = 443 samples, 10 ms each way */
#define SH_L1 28243u                     /* MOD's two sines, the phase a sample: 0.29 Hz and 0.47 Hz at 44.1 kHz (they never line up: no steady vibrato) */
#define SH_L2 45774u
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
 * right (rev_side_mix). Back to 44.1 kHz between samples. Rounding: the lines' gains round to the nearest (0.2.29: the
 * quiet lines too, hall_end: the tail decays as it should down to the lines' last LSB and then ends in exact silence),
 * and the low-passes keep their steps' remainders (no offset). */
#define HL_NL 8u                         /* lines */
#define HL_NAP 2u                        /* diffusers */
#define HL_NC (CTL / 2u)                 /* a block at half the rate */
#define HL_MOD 8                         /* the taps' swing, samples (read 2 .. 18 samples short of the end) */
#define HL_LPM 0x69u                     /* the lines with a low-pass */
#define HL_KD(d) (32767 - (d) * 180)     /* its coefficient (Q15) for DAMP d: none at 0 .. ~1 kHz at 127 */
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
 * broken digital fuzz). State per part (track_t dist_*).
 * 0.2.14: DIST 1 .. 12 blends the dry signal and this wet one, out = dry + (wet - dry) * DIST / 12 (the wet alone cut the
 * bass and the level at the first detent: a jump from the bypass); from 13 up it is the wet alone, as before. DIST 0 is
 * the bypass (the block untouched). Switching on from 0 starts the filters from this signal (the high-pass at the first sample,
 * the low-passes at the clip's rest, 0), not from what the last use left: no click */
#define DIST_OFF (-0x7FFFFFFF)                           /* dist_hp while DIST is 0 */
static void track_dist(track_t *t, int32_t *b, uint32_t n)
{
    int32_t d = t->p[P_DIST], i, g, k, mk, bias = 2400, b0;
    if (!d) {
        t->dist_hp = DIST_OFF;                           /* (states are restarted when it comes on) */
        return;
    }
    if (t->dist_hp == DIST_OFF && n) {
        t->dist_hp = b[0];
        t->dist_lp1 = t->dist_lp2 = 0;
    }
    g = 4096 + d * d * 2;                                /* Q12: 1x .. ~9x, gentle at first */
    k = 32000 - d * 95;                                  /* tone: transparent at low drive .. ~3 kHz, Q15 */
    mk = 30000 - d * 120;                                /* make-up */
    b0 = softclip(bias);
    for (i = 0; i < (int32_t)n; i++) {
        int32_t x = b[i], dry = x, y;
        t->dist_hp += (x - t->dist_hp + 64) >> 7;           /* ~55 Hz low cut: keep the bass out of the clipper */
        x = clamp(x - t->dist_hp, -230000, 230000);         /* (x >> 2) * g fits 32 bits; the clip is flat out there */
        y = softclip((((x >> 2) * g) >> 10) + bias) - b0;   /* >> 2 first: no overflow for loud poly */
        t->dist_lp1 += mulq15(y - t->dist_lp1, k);         /* two poles: tames the fizz */
        t->dist_lp2 += mulq15(t->dist_lp1 - t->dist_lp2, k);
        y = mulq15(t->dist_lp2, mk);
        b[i] = d < 12 ? dry + (y - dry) * d / 12 : y;       /* (DIST 12 up: the wet alone) */
    }
}

/* master: peak limiter in front of the soft clipper. Fast attack (~0.1 ms),
 * ~150 ms release, threshold where tanh is still nearly linear, so chords
 * get quieter instead of crushed. */
#define LIM_T 18000
static int32_t lim_env = LIM_T;
/* The mix is always made at the reference level MASTER_USB_REF (the default knob position, song.master_q12 = 2048, gain 0.5), whatever the MASTER
 * knob says: the limiter and the soft clip see the same signal, and USB audio takes it (audio.c uac_tap) at that fixed level. MASTER is a volume on what
 * the DAC gets, after the USB tap: audio.c master_dac (a per-sample glide, 24-bit result). (Up to DUELL 0.2.24 MENU > USB LEVEL chose between this
 * "FIXED" and MASTER before master_out, where MASTER's level stepped once per block; the row is removed, FIXED is the only way.) */
#define MASTER_FULL 4096                /* main.c: the MASTER knob's top, Q12 */
#define MASTER_USB_REF 2048             /* the level master_out gets (Q12), the default master_q12 */
/* The level mix_block makes the mix at (Q12). The device: always MASTER_USB_REF. HOST TESTS ONLY (-DHOST_MIX_FOLLOWS_MASTER=1, host_tests.sh): the level is song.master_q12 unless
 * a test clears host_mix_follows_master, as every host test rendered before 0.2.25 (hostsim.c sets master_q12 = 4096, the renders and their pinned fingerprints are at that level;
 * a few tests set another value on purpose). tests/audio_test.c, click_test.c and master_hunt.c clear it: they run the device's own path (audio_block). The device build
 * never defines the macro: no variable, no branch, the constant. */
#ifdef HOST_MIX_FOLLOWS_MASTER
int host_mix_follows_master = 1;
#define MIX_MG() (host_mix_follows_master ? (int32_t)song.master_q12 : (int32_t)MASTER_USB_REF)
#else
#define MIX_MG() ((int32_t)MASTER_USB_REF)
#endif
/* the MASTER pot (ADC 0..1023, smoothed in main.c) to its level, square law: 0 .. 2048 (Q12). DUELL 0.2.30 (owner decision 2026-10-20): the TOP of the knob is unity (0 dB, 2048 = the
 * level the knob's default position gave until 0.2.29, MASTER_USB_REF); there is no gain above it (no boost, so no push into the limiter and no clip after it). The old law (k^2 >> 8, 0 .. 4088,
 * 2048 at 71 % of the knob) is halved and lifted by 1/64 so the top reaches 2048 a hair below the pot's physical end (pot 1016 of 1023: a pot that does not quite reach 1023 still gets 0 dB),
 * and held there. The quiet end keeps the square law's fine steps (master_g16 below has 4 more bits) and the 11.6 ms glide of audio.c master_dac */
#define MASTER_TOP 2048u
static inline uint32_t master_of_pot(uint32_t k10)
{
    uint32_t g = (k10 * k10) >> 8;
    g = (g + (g >> 6)) >> 1;
    return g > MASTER_TOP ? MASTER_TOP : g;
}
/* DUELL 0.2.25: the same law at 4 more bits, from the smoothed knob (main loop: master.c, the pot x16): the level in Q16 (0 .. 32,768 since 0.2.30; was 65,412). song.master_q12 is its upper 12 bits (the
 * whole-step value everything else reads); master_g16 is the whole value, which only audio.c master_dac uses: at the quiet end of the knob one whole Q12 step is 33 % (3 -> 4), a
 * step of 10 dB in the volume that no glide can make fine; with the fraction the volume moves in 1/16 steps of that. The main loop writes master_g16 first and song.master_q12
 * after it, one 32-bit store each; the ISR uses master_g16 only while its upper 12 bits equal song.master_q12 (otherwise the whole-step value alone: a torn pair, or a test
 * or another writer that set song.master_q12 only, is read as the whole step, never as a mix of the two) */
static inline uint32_t master_of_knob16(uint32_t k16)
{
    uint32_t g = (k16 * k16) >> 12;
    g = (g + (g >> 6)) >> 1;                            /* (0.2.30: the top of the knob is unity, see master_of_pot) */
    return g > MASTER_TOP * 16u ? MASTER_TOP * 16u : g;
}
static volatile uint32_t master_g16;
static int32_t dc_l, dc_r, dce_l, dce_r;

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

static inline void master_out(int32_t *l, int32_t *r)
{
    int32_t al, ar, a;
    *l = dc_block(*l, &dc_l, &dce_l);
    *r = dc_block(*r, &dc_r, &dce_r);
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

/* SPLIT (DUELL step 5): master_out with each channel its own limiter (a linked one would duck part 2 on part 1's peaks: part 1's
 * envelope on the right). The DC blocker and the soft clip are per channel already */
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
    *l = softclip(lim_one(*l, &lim_env));
    *r = softclip(lim_one(*r, &lim_env_r));
}

/* length of one division (N_DIV order) in samples at the song tempo */
static const uint8_t DIV_DEN[6] = {1, 2, 4, 8, 3, 6};    /* original IDs stay fixed; slow rates append */
static const uint8_t DIV48[7] = {32, 64, 4, 144, 72, 36, 18};   /* 4T 2T 32T 2D 4D 8D 16D in 1/48 quarter notes (div >= 10) */
static uint32_t midi_beat_samples;                    /* zero until an external clock has a measured tempo */
static uint32_t beat_samples(void)
{
    return clock_src() && midi_beat_samples ? midi_beat_samples : (uint32_t)FS * 60u / (uint32_t)song.g[G_BPM];
}
static uint32_t div_samples(uint32_t div)
{
    uint32_t quarter = beat_samples();
    return div < 6u ? quarter / DIV_DEN[div] : div < 10u ? quarter << (div - 5u) : quarter * DIV48[(div - 10u) % 7u] / 48u;
}

/* what the per-part INSERT (FLANGER, PHASER below) shares: the PHASER's stage count and its coefficient table, the sweep's
 * triangle and the wet mix (they came from the FX hold layer, removed in 0.2.2) */
#define PH_ST 4u                                     /* PHASER: all-pass stages (two notches, the classic pedal's) */
/* the stages' coefficient a (Q14) for a break of 150 Hz * 2^(5 i / 32): a = (g - 1) / (g + 1), g = tan(pi f / fs) */
static const int16_t PH_A[33] = {
    -16038, -15998, -15955, -15906, -15853, -15793, -15727, -15653, -15572, -15481, -15381, -15270, -15147, -15012,
    -14862, -14696, -14514, -14313, -14092, -13849, -13582, -13290, -12970, -12621, -12240, -11825, -11375, -10886,
    -10356, -9784, -9166, -8500, -7783,
};
/* a triangle of the sweep's place q, 0 .. 65534 (Q16) */
static inline uint32_t pf_tri(uint32_t q)
{
    uint32_t t = q >> 16;
    return t < 32768u ? t << 1 : (65535u - t) << 1;
}
/* x towards y by w (Q15, 32768 = y) */
static inline int32_t pf_mix(int32_t x, int32_t y, int32_t w)
{
    return w == 32768 ? y : x + mulq16(y - x, (uint32_t)w << 1);
}
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
 * and b[i + 1], f / 256 of the way (rounded to the nearest: it stays between the two), times its gain g (Q14), rounded to the nearest.
 * DUELL 0.2.29 (owner, T0226: M07 FINAL REEL's hall tail "cut like a noise gate"): until 0.2.28 a quiet line (two samples' sum under 32,
 * HL_LOUD) rounded its gain's product towards zero instead, so that the tail would end in exact silence; at a line of a few LSB that is a
 * loss of up to 1 / 6 a pass (0.374 x 15 = 5.6 -> 5), and the tail fell from its natural ~15..22 dB/s to ~200 dB/s once it was ~50 dB down
 * (the wet bus at -53 dBFS, the output at -60 with MASTER at the default; tests/hall_tail_test.c). Rounded to the nearest everywhere, the tail
 * decays at its own rate to ~-75 dBFS and still ends in exact silence: the products of a line of 1 LSB round to 0 (0.374 < 0.5), the
 * diffusers' states and the low-passes' remainders go towards zero, and no limit cycle was found (288 renders: SIZE 0..150, DAMP 0..127,
 * noise / 100 Hz / 5 kHz / an impulse at 0 .. -60 dBFS: all silent within 12 s) */
static inline void hall_end(const int16_t *b, int32_t f, int16_t *y, uint32_t n, int32_t g)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        y[i] = (int16_t)(((b[i] + (((b[i + 1u] - b[i]) * f + 128) >> 8)) * g + 8192) >> 14);
}
/* a line's end over y[0 .. n) from b (no wrap), through the low-pass (kd, Q15; the state *lp and its step's
 * remainder *le), times its gain g (rounded as hall_end's) */
static inline void hall_end_lp(const int16_t *b, int16_t *y, uint32_t n, int32_t g, int32_t *lp, int32_t *le, int32_t kd)
{
    uint32_t i;
    int32_t l = *lp, r = *le, e;
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

/* PLATE (G_RTYPE 5, 0.2.14): a plate reverb, again in ROOM's two buffers (no RAM of its own but 48 bytes of state a bus), stereo like HALL
 * (the mid on the wet bus, the difference in hl.sd). Based on the reverb of the CHOMPI firmware (MIT; CHOMPI Club,
 * <https://github.com/CHOMPI-Club/CHOMPI>, firmware/chompi-tape/code/src/reverb.h and fx_engine.h), which is Emilie Gillet's plate reverb of
 * Mutable Instruments Rings / Clouds (MIT, Copyright 2014 Emilie Gillet; the Griesinger plate of Dattorro's paper): see LICENSING.md,
 * LICENSES/MIT-CHOMPI.txt and NOTICE.md. Rewritten here as integer code for DUELL (Q15 coefficients, 16-bit lines, no float).
 *   the send (mono) -> 4 allpass diffusers in series -> a figure-eight tank of two halves that feed each other: half 1 reads line 9 at a
 *   drifting point (+-14 samples, a sine of 0.3 Hz), times the loop gain, a one-pole low-pass (DAMP), two allpasses, and writes line 6; half 2
 *   reads line 6 at a point drifting at 0.5 Hz, the same, and writes line 9. The output: the signal each half has after its two allpasses, times 2:
 *   left from half 1, right from half 2 (two different points of the tank: decorrelated, wide).
 * It runs at half the rate (22.05 kHz: the send 1 2 1 low-passed every other sample, the outputs between samples, as HALL) with CHOMPI's line
 * lengths x 0.273 (x 0.546 x the half rate): the ten lines (5,911 samples with their mirror sample) fill the 4,937 + 997 samples of ROOM's two buffers
 * (rev_comb: lines 0 4 5 6 8 9, 4,929; rev_u.ap: lines 1 2 3 7, 982). The tank's two halves are 138 and 114 ms long.
 *   SIZE   the loop gain g = 2^(-2.08 * 2^(-SIZE / 37.6)) (0.24 at 0, 0.76 at 90, 0.87 at 127, 0.90 at most: SPACE's 150), the tail's length:
 *          RT60 about 0.5 s at 0, 3 s at 90, 6 s at 127 (as the host comparison fitted it to HALL's)
 *   DAMP   the loops' low-pass: 0.81 (Q15 26668, ~2.8 kHz a pass) at 60, the comparison's setting; open (1.0) at 0, 0.30 (~1 kHz) at 127
 *   The diffusion (0.41) is fixed. Rounding: the loop gain's product towards zero and the low-passes keep their steps' remainders (the tail
 *   ends in exact silence, no offset); the allpasses (coefficient under 0.5) round to the nearest. Every line value is clamped to 16 bits. */
#define PL_N 10u
#define PL_LEN_SUM (42 + 59 + 88 + 145 + 597 + 735 + 1230 + 690 + 601 + 1724)
static const uint16_t PL_LEN[PL_N] = {41, 58, 87, 144, 596, 734, 1229, 689, 600, 1723};   /* the lines (+1 sample each: the mirror) */
static const uint16_t PL_OFF[PL_N] = {0, 0, 59, 147, 42, 639, 1374, 292, 2604, 3205};     /* where each starts in its buffer */
#define PL_ONB 0x8Eu                     /* the lines in rev_u.ap (the others in rev_comb) */
#define PL_KAP 13562                     /* the diffusion, Q15 (0.414: CHOMPI's knob 0.7) */
#define PL_IN 1548                       /* the send's gain into the lines, Q12 (ROOM's send level x CHOMPI's input gain 0.6 x 8: the lines use their bits) */
#define PL_INC1 97397u                   /* the read points' sines a tick (22.05 kHz): 0.5 Hz, */
#define PL_INC2 58438u                   /* .. 0.3 Hz (2^32 a turn) */
#define PL_DEP1 2795                     /* their depth, Q8 samples (10.9 and 13.7), */
#define PL_DEP2 3494
#define PL_BASE1 ((1229 * 256) - PL_DEP1 - 256)   /* .. the reads' centre, Q8 (the longest read reaches the line's last sample) */
#define PL_BASE2 ((1723 * 256) - PL_DEP2 - 256)
_Static_assert(PL_LEN_SUM == 5911 && 42 + 597 + 735 + 1230 + 601 + 1724 <= sizeof rev_comb / 2u && 59 + 88 + 145 + 690 <= sizeof rev_u.ap / 2u,
               "PLATE in ROOM's buffers");
_Static_assert(CTL % 2u == 0u, "PLATE: half the rate");
#define PL_MQ(a, k) ((((a) * (k)) + 16384) >> 15)       /* (Q15 product, to the nearest: |a| < 2^16, k < 2^15) */
static inline __attribute__((always_inline)) int32_t pl_c16(int32_t v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : v; }
static inline __attribute__((always_inline)) int32_t pl_rd(const int16_t *b, uint32_t w, uint32_t d, uint32_t len)
{
    return b[w >= d ? w - d : w + len + 1u - d];
}
static inline __attribute__((always_inline)) int32_t pl_ap(int16_t *b, uint32_t w, uint32_t len, int32_t acc, int32_t kr, int32_t kw)
{
    int32_t t = pl_rd(b, w, len - 1u, len);
    acc = pl_c16(acc + PL_MQ(t, kr));
    b[w] = (int16_t)acc;
    return PL_MQ(acc, kw) + t;
}
static inline __attribute__((always_inline)) int32_t pl_tap(const int16_t *b, uint32_t w, uint32_t q8, uint32_t len)
{
    uint32_t d = q8 >> 8;
    int32_t a = pl_rd(b, w, d, len), c = pl_rd(b, w, d + 1u, len);
    return a + sh_tz((c - a) * (int32_t)(q8 & 255u), 8);
}
/* the loop gain for SIZE s (Q15) */
static __attribute__((noinline)) int32_t plate_gain(int32_t s)
{
    uint32_t q = exp2_neg((uint32_t)clamp(s, 0, 160) * 1743u), g = exp2_neg((q * 8519u) >> 11);
    return (int32_t)(g > 29491u ? 29491u : g);
}
/* the low-pass coefficient for DAMP d (Q15) */
static int32_t plate_damp(int32_t d)
{
    d = clamp(d, 0, 127);
    return d <= 60 ? 32767 - d * 6099 / 60 : 26668 - (d - 60) * 16838 / 67;
}

/* the delay line b (index mask m) read at the position p (Q16: the sample index in the high half), linear between the two
 * neighbours; p = (k << 16) is b[k]. The delay's modulation reads with it (fx_bus.c dly_mod) */
static inline int32_t dly_tap(const int16_t *b, uint32_t m, uint32_t p)
{
    uint32_t i = p >> 16;
    int32_t c0 = b[i & m], c1 = b[(i + 1u) & m];
    return c0 + (((c1 - c0) * (int32_t)((p >> 8) & 255u)) >> 8);
}
/* The delay's EVENT modes (DUELL 0.2.14, core.h G_DMODE RAND / OCT+; fx_bus.c dly_ev): the state of bus A (STEREO and LAYER; SPLIT keeps the plain delay). Two
 * voices read the delay line (dly_buf: no buffer of its own) at a read head each; one plays the plain delay (NONE), the other starts an event on a random
 * share of the clock edges (RETRIG, REVERSE, UP an octave, DOWN an octave), the two crossfading. 76 bytes. "on" 0: not running (the next block starts it from
 * the plain delay: voices NONE, the generator on its seed, a crossfade from the plain delay's return); x: the event share of the output (Q15), the crossfade
 * between the plain delay and the events when the mode changes, ~11.6 ms, either way */
typedef struct {
    uint8_t act, fin, fout, ev, nev;     /* playing, fading in, fading out, the event it plays, the event it starts with */
    int16_t npan;                        /* the pan it starts with (Q15) */
    uint16_t cnt;                        /* the fade's place (0 .. DE_FADE) */
    int32_t gl, gr;                      /* the pan gains (Q15, 32768 = 1 at the centre) */
    uint32_t rh;                         /* the read head (Q16 samples) */
} dlyev_v_t;
typedef struct {
    uint32_t rng, dl16, ecnt;            /* the xorshift32, the delay time smoothed (Q16 samples), samples since the clock edge */
    int32_t lp, x;                       /* the events' feedback low-pass, the events' share (Q15) */
    uint8_t on, sync, cur, nxt, ev0, ev1, md, pend;   /* md: the event mode running (1 RAND, 2 OCT+; kept while the crossfade runs down); pend: a voice is yet to start for the last edge */
    dlyev_v_t v[2];
} dlyev_t;
static dlyev_t dlyev;
static dlyev_t dlyev_b __attribute__((section(".pool")));                  /* 0.2.22: DUEL's bus B (part 2) has its own copy: the event modes run per part, mono (fx_bus.c; 76 bytes) */
/* DUELL 0.2.25 OVERLOAD GUARD (SPEC 6.3; automatic, always on, nothing stored). When the audio is near its limit, by the voice shedding's own
 * measure (audio.c shed_check: two halves in a row over 85 % of their time), the delay's RANDOM / OCT UP runs as the plain delay it
 * cross-fades from (fx_buses, split_buses: the events' ~11.6 ms crossfade; the delay's modulation is not run in their place). (The other
 * dear effect it was built for, SHIM's PITCH BOTH, was removed in 0.2.25 by the owner.) Back after the meter's load (song.cpu_q8, the CPU bar's figure) has stayed under 70 % for ~3 s,
 * the same way. Nothing changes while it is not engaged: the stored values are never touched, ovl_lite only steers the delay's switches below
 * (tests/ovl_test.c renders bit for bit with it compiled out, -DOVL_NOGUARD). Shown on ABOUT (LIGHT FX); flash code only */
static uint8_t ovl_lite;                 /* 1: the guard is engaged */
static uint16_t ovl_calm;                /* halves in a row with the meter's load under 70 % while engaged */
static uint32_t ovl_count;               /* times it engaged since power-on (the console) */
#ifdef OVL_NOGUARD
#define OVL_LITE 0u                      /* (the test's reference build: the guard compiled out) */
#else
#define OVL_LITE ovl_lite
#endif
#define OVL_CALM_Q8 179u                 /* 70 % in the meter's Q8 (256 = the whole half): the bar's amber */
#define OVL_CALM_HALVES ((3u * FS + HALF_FRAMES / 2u) / HALF_FRAMES)   /* ~3 s of halves (1034) */
/* once a half (audio.c shed_check): over = the 85 % rule held (two halves in a row), q8 = the meter's load */
static __attribute__((noinline)) void ovl_step(uint32_t over, uint32_t q8)
{
    if (over) {
        ovl_count += !ovl_lite;
        ovl_lite = 1;
        ovl_calm = 0;
        return;
    }
    if (!ovl_lite)
        return;
    ovl_calm = q8 < OVL_CALM_Q8 ? (uint16_t)(ovl_calm + 1u) : 0u;
    if (ovl_calm >= OVL_CALM_HALVES)
        ovl_lite = 0, ovl_calm = 0;
}
static void dlyev_clear(void) { memset(&dlyev, 0, sizeof dlyev); memset(&dlyev_b, 0, sizeof dlyev_b); }   /* the lines are cleared (SENDS OFF, STOP ALL, a mode change of the mix): from rest, both buses */
static void dlyev_start(void) { dlyev.sync = 1; dlyev_b.sync = 1; }  /* PLAY: the clock edges count from the top (seq.c seq_start, as the DUCK's PUMP) */
static struct { int32_t r[CTL], l[CTL]; } mixlr HOTG_BSS;   /* the pair in one pinned object (core.h HOTG_BSS): the hot PERFORM / delay code reaches the left as the right + 128 */
#define mix_r (mixlr.r)
#define mix_l (mixlr.l)   /* (the dry mix: defined here for the delay's modulation, which adds its side to it) */

/* the buses (fx_bus.c): bus A on the globals they always had, bus B (SPLIT, part 2) on its own */
static uint8_t dm_split;                /* (below: the mix runs SPLIT; RESO reads the part its bus carries) */
#define BUS_FX fx
#define BUS_EV dlyev
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
#undef BUS_EV
#undef BUS_SHIM
#undef BUS_SPLITB
#undef BUS_HL
#undef BUS_COMB
#undef BUS_U
#undef BUS_CHO
#undef BUS_DLY
#undef BUS_FN
#define BUS_FX fx_b
#define BUS_EV dlyev_b
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
#undef BUS_EV
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
static uint8_t dly_side;                /* dly_sd holds this block's (PING-PONG, RANDOM, OCT UP) */
static uint8_t dly_plain_pp;            /* 1: the plain delay that ran last was the PING-PONG (the events' crossfade starts from it, and goes back to the mode's own) */
static void fx_buses(const int32_t *cho_in, const int32_t *dly_in, const int32_t *rev_in, int32_t *wet, uint32_t n)
{
    uint32_t dm = (uint32_t)song.g[G_DMODE];            /* (0.2.16: the delay's mode; DIGITAL, PING-PONG and a value past OCT UP: the paths below, bit for bit as 0.2.15's PING off / on) */
    uint32_t evm = dm == DMD_RAND || dm == DMD_OCTUP;   /* (the mode is an event mode) */
    uint32_t ev = evm && !OVL_LITE;                     /* (0.2.25: the overload guard runs it as the plain delay it cross-fades from) */
    uint32_t pp = dm == DMD_PING;                       /* (DUELL, SPEC 5c.2: the repeats alternate left and right) */
    cho_zrun(cho_in, n);                                /* (0.2.21: the chorus line's zero run, before any path writes it) */
    if (!evm)
        dly_plain_pp = (uint8_t)pp;                     /* (the plain delay the events cross-fade from: the last one that ran) */
    else
        pp = dly_plain_pp;
    if (ev || dlyev.x) {                                /* an event mode, or the crossfade back to the plain delay still running */
        dly_side = 1;                                   /* (the events' side goes the PING-PONG's way: dly_sd, SPREAD) */
        fx_buses_ev(pp ? delay_split() : delay_samples(), delay_split(), cho_in, dly_in, rev_in, wet, n,
                    ev ? dm - DMD_RAND + 1u : 0u, pp, dly_sd, DLY_LEN);   /* (dly_ev: 1 RANDOM, 2 OCT UP) */
        fx.cho_zs = 0;
        return;
    }
    dly_side = (uint8_t)pp;
    uint32_t md = !evm && (song.g[G_DMDEP] > 0 || fx.dm_dev);   /* (the delay's modulation: fx_bus.c dly_mod; an event mode's DEPTH is its CHANCE, not run as a modulation under the guard) */
    if (pp) {                                           /* (the loop is 2 x the time: a time that does not fit halves, as in SPLIT) */
        if (md)
            fx_buses_mod(DLY_LEN - 1u, delay_split(), cho_in, dly_in, rev_in, wet, n, 1u, dly_sd);
        else
            fx_buses_m(DLY_LEN - 1u, delay_split(), cho_in, dly_in, rev_in, wet, n, 1u, dly_sd);
    } else if (md)
        fx_buses_mod(DLY_LEN - 1u, delay_samples(), cho_in, dly_in, rev_in, wet, n, 0u, dly_sd);
    else
        fx_buses_m(DLY_LEN - 1u, delay_samples(), cho_in, dly_in, rev_in, wet, n, 0u, dly_sd);
    fx.cho_zs = 0;                                      /* (fx_bus.c cho_zrun: the flag is for this block only) */
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
 *   CRUSH                BITS (1 .. 16, of a voice's full scale; 0.2.14: a different, fractional depth at every detent, 16 only at the
 *                        last two, ins_bits.h), RATE (a held sample, 689 Hz .. 44.1 kHz, INS_HOLD:
 *                        aliasing as the old samplers), LPF (two one-pole low-passes at F_CUTOFF after them, #177);
 *   PHASR                RATE (F_LFOHZ), DEPTH (the sweep around ~850 Hz: 0 still .. the whole 150 Hz .. 4.8 kHz),
 *                        FDBK (0 .. 0.7): four all-pass stages (PH_A above), mono, per track;
 *   FLANG CHOR           RATE, DEPTH, FDBK: a 256-sample line per track (5.8 ms); FLANG swept 0.25 .. 5 ms (the FX
 *                        layer's squared triangle), feedback 0 .. 0.85; CHOR a sine around 3.2 ms (+-2.3 ms at DEPTH
 *                        127), feedback 0 .. 0.4. A DC blocker on the swept copy (a clipped loud input is lopsided).
 *   TAPE (0.2.14)       WOW (A: the depth of a slow random wandering of the delay, 0.5 .. 2 Hz, 0 still .. +-33 samples), SAT (B: a soft
 *                        saturation, 1x .. 5x, the level kept by 1 / gain^0.75), TONE (C: one one-pole high-cut at F_CUTOFF). Fixed: a flutter of 6 .. 9 Hz
 *                        (+-1.5 samples, ~3 cents) and a faint hiss (~ -61 dBFS), both before the high-cut. The line of FLANG / CHOR;
 *   VINYL (0.2.14)      DUST (A: crackle clicks a second, 0 = none, 1 .. 248, each of a random sign / size / 0.1 .. 0.7 ms decay),
 *                        NOISE (B: the quiet high-passed noise bed, 0 = none .. ~ -41 dBFS), WARP (C: the record's warp, a sine at 33 1/3 rpm,
 *                        0.556 Hz, 0 still .. +-35 samples). Fixed: the band narrowed (1-pole high-pass ~110 Hz, 1-pole low-pass 6 kHz) on the
 *                        signal only; clicks and noise are added after it. Both: a xorshift per track (seeded per part), nothing from the
 *                        shared rng; the hiss / dust / clicks exist while the part sounds (the INSERT does not run on a silent part).
 * The swept ones are the pedals': their wet is (dry + swept) / 2, the notches need both, so MIX 100 % is the whole
 * effect and less a lighter one. MIX (P_IMIX) blends dry and wet linearly; MIX 0 or TYPE OFF: the track bit for bit
 * as before (nothing runs). MIX glides (2.9 ms, SL_SLOPE); a TYPE change fades the old one out, clears the state,
 * fades the new one in; so does a start from MIX 0 (no stale line or filter). With SPREAD (mix_spread) the insert
 * takes the mono sum b, as DIST does; the side passes it by.
 * Order: DIST -> INSERT -> EQ: the insert colours the sound as DIST does (DUELL: the SLICER that came after is
 * removed). State in the pool, per track (ins[], 580 B each); noinline, its own loops: the audio
 * ISR's inlined path gains one test per part (track_t ins_run) */
enum { IT_OFF, IT_SOFT, IT_HARD, IT_FOLD, IT_FUZZ, IT_CRUSH, IT_PHASER, IT_FLANGER, IT_CHORUS, IT_TAPE, IT_VINYL, IT_N };
#define IL_LEN 256u                     /* FLANG / CHOR: the line, samples (a power of 2: masked) */
static const uint8_t INS_HOLD[16] = {64, 48, 40, 32, 24, 20, 16, 12, 10, 8, 6, 5, 4, 3, 2, 1};   /* CRUSH RATE */
typedef struct {
    int32_t w;                          /* the wet share now, Q15 */
    uint8_t type;                       /* the type running */
    uint8_t cnt;                        /* CRUSH: samples left of the held one */
    uint16_t wp;                        /* FLANG / CHOR: the line's next write */
    uint32_t ph;                        /* the sweep's phase */
    int32_t hp, lp1, lp2, dc, hold;     /* low cut (Q8), the two low-passes, DC blocker (Q8), CRUSH's held sample */
    union {                             /* (one type runs at a time: the same words, 36 bytes) */
        struct { int32_t px[PH_ST], py[PH_ST], fb; } ph;   /* PHASR: each stage's last input and output, the fed back */
        struct { int32_t rng, tgt, nxt, wa, wb, dep, env, ep; } lo;   /* TAPE, VINYL: the xorshift, the wow's target and samples to the next, its
                                                         * two smoothers (Q23), the smoothed depth (Q8), a click's level and its high-pass */
    } u;
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
    case IT_CRUSH: {                                    /* sh: the step, Q8 (ins_bits_step; 256 = 1: clean); to the nearest step */
        int32_t h, q;
        if (sh <= 256u)
            return v;
        h = v * 256 + (int32_t)(sh >> 1);               /* (|v| < 2^19: 32 bits; floor division, as the shift was) */
        q = h >= 0 ? h / (int32_t)sh : -((-h + (int32_t)sh - 1) / (int32_t)sh);
        h = q * (int32_t)sh;                            /* (a step off a whole number: odd rounding, -x gives minus what x gives) */
        return h >= 0 ? (h + 128) >> 8 : -((-h + 128) >> 8);
    }
    default:
        return v;
    }
}
/* CRUSH's BITS knob (0.2.14): INSERT A 0..127 -> the quantiser step, Q8, 2^(16 - bits) * 256 (ins_bits.h, tools/gen_ins_bits.py:
 * every detent a different, fractional depth; 1 bit at 0 .. 16 bits, clean, at 126 and 127). Whole bits sit on INS_BITS_KNOT */
static inline uint32_t ins_bits_step(int32_t a) { return INS_BITS_STEP[clamp(a, 0, 127)]; }

/* SOFT .. FUZZ: low cut -> DRIVE -> shape -> TONE -> LEVEL -> DC blocker; CRUSH: held sample -> bits -> LPF */
static __attribute__((noinline)) void ins_drive(ins_t *s, const track_t *t, int32_t *b, uint32_t n, int32_t tw)
{
    uint32_t i, ty = s->type, crush = ty == IT_CRUSH, sh = ins_bits_step(t->p[P_IA]);
    uint32_t hold = INS_HOLD[clamp(t->p[P_IB], 0, 127) >> 3];
    int32_t a = t->p[P_IA], g1 = 4096 + a * a * 3;      /* DRIVE, Q12: 1x .. 12.8x, gentle at first */
    int32_t k = ins_lpk(crush ? t->p[P_IC] : t->p[P_IB]), l1 = crush ? 4096 : LEVEL_Q12[t->p[P_IC] & 127];
    int32_t g0 = s->u.lo.wa ? s->u.lo.wa : g1, l0 = s->u.lo.wa ? s->u.lo.wb : l1;   /* hunt3: DRIVE and LEVEL go from the block before's value to this block's in a line (a morph or a route that moved */
    int32_t gq = g0 << 11, lq = l0 << 11, dg = (g1 - g0) << 6, dl = (l1 - l0) << 6;   /* A or C by a step was a click: #23 KICK BASS, #7 FM BASS 2); Q11, CTL = 32 samples; still: the old code bit for bit */
    s->u.lo.wa = g1;
    s->u.lo.wb = l1;
    for (i = 0; i < n; i++) {
        int32_t x = b[i], y, v, g, lvl;
        gq += dg;
        lq += dl;
        g = gq >> 11;
        lvl = lq >> 11;
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
            int32_t x = b[i], xs = clamp((x + 4) >> 3, -16383, 16383), u = xs + ((s->u.ph.fb * fbk + 16384) >> 15), y = u;
            uint32_t st;
            s->w += clamp(tw - s->w, -SL_SLOPE, SL_SLOPE);
            for (st = 0; st < PH_ST; st++) {            /* y = a (u - y1) + u1 */
                y = clamp(s->u.ph.px[st] + ((a * (u - s->u.ph.py[st]) + 8192) >> 14), -65535, 65535);
                s->u.ph.px[st] = u;
                s->u.ph.py[st] = y;
                u = y;
            }
            s->u.ph.fb = clamp(y, -32767, 32767);
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
                uint32_t tr = pf_tri(s->ph), sw = (tr * tr) >> 16;   /* longer near the short end */
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

/* TAPE, VINYL (0.2.14): the 256-sample line of FLANG / CHOR at an eighth (a voice-sum's full range), read at a delay that moves.
 * TAPE: wow (random targets every 0.25 .. 1 s through two one-poles of ~1.7 Hz: a wandering of ~0.5 .. 2 Hz) + flutter (a sine 6 .. 9 Hz, its
 * rate moved by the wow) -> soft saturation (tanh, 2x the voice scale, the level kept by 1 / gain^0.75) -> faint hiss -> one-pole high-cut -> DC blocker.
 * VINYL: warp (a sine at 0.556 Hz) -> band (high-pass 110 Hz, low-pass 6 kHz) -> + clicks (Poisson, random size / sign / decay, high-passed
 * so each has no area) + dust (white noise through a ~880 Hz high-pass) -> DC blocker. The wet is the effect alone (the delay is 2 .. 6 samples when still).
 * The noise generator is the track's own and does not depend on MIX or the input. */
static uint32_t ins_isqrt(uint32_t v)
{
    uint32_t r = 0, bit = 1u << 30;
    while (bit > v)
        bit >>= 2;
    while (bit) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}
/* TAPE's saturation (also the INSERT graph's curve): v8 an eighth of the voice-sum, g = SAT's gain (Q12), comp = 1 / sqrt(gain) (Q12) */
static inline int32_t ins_tape_comp(int32_t g)   /* 4096 x gain^-0.75 (g = 4096 x gain): sqrt, then its root, a division in 32 bits */
{
    uint32_t q = ins_isqrt((uint32_t)g << 16), q4 = ins_isqrt(q << 8);   /* 256 x sqrt(g), 256 x g^0.25 */
    return (int32_t)(0x80000000u / ((q * q4) >> 6));
}
static inline int32_t ins_tape_sat(int32_t v8, int32_t g, int32_t comp) { return (softclip((v8 * g) >> 10) * 2 * comp) >> 12; }
#define IT_WOW_K 130u                   /* TAPE's WOW 127: 65 samples peak to peak (measured: see tests/insert_test.c test_lofi) */
#define IT_WARP_K 141u                  /* VINYL's WARP 127: 70 samples */
static __attribute__((noinline)) void ins_lofi(ins_t *s, const track_t *t, int32_t *b, uint32_t n, int32_t tw)
{
    uint32_t i, tape = s->type == IT_TAPE;
    int32_t pa = clamp(t->p[P_IA], 0, 127), pb = clamp(t->p[P_IB], 0, 127), pc = clamp(t->p[P_IC], 0, 127);
    int32_t dep_t = (tape ? pa : pc) << 8, k = (int32_t)ins_lpk(tape ? pc : 107);
    int32_t g = 4096 + pb * pb, comp = ins_tape_comp(g), namp = (pb * pb) >> 5;
    int32_t dd, dcur, ni = (int32_t)n;
    uint32_t thr = 0, prod, dt;
    if (!tape && pa) {                                  /* DUST: (16 + f) << e / 16 clicks a second (1 .. 248), as a threshold of the 32-bit draw */
        uint32_t q = (16u + ((uint32_t)pa & 15u)) << ((uint32_t)pa >> 4);
        thr = q * 6087u;
    }
    /* the delay is worked out once a block (n samples) and moves in a straight line to it: wow and flutter are slow */
    s->u.lo.dep += ((dep_t - s->u.lo.dep) * ni) >> 10;  /* the depth glides (23 ms): no step in the delay while the knob moves */
    if (tape) {
        int32_t walk, flut;
        s->u.lo.nxt -= ni;
        if (s->u.lo.nxt <= 0) {                         /* a new target, in 0.25 .. 1 s */
            uint32_t r = (uint32_t)noise32(&s->u.lo.rng);
            s->u.lo.tgt = (int32_t)(r >> 16) - 32768;
            s->u.lo.nxt = 11025 + (int32_t)((r >> 1) & 0x7FFFu);
        }
        s->u.lo.wa += (((s->u.lo.tgt << 8) - s->u.lo.wa) * ni) >> 12;   /* two one-poles of ~1.7 Hz (1 / 4096 a sample) */
        s->u.lo.wb += ((s->u.lo.wa - s->u.lo.wb) * ni) >> 12;
        walk = clamp(s->u.lo.wb >> 7, -32767, 32767);   /* Q15, x2 (the smoothing leaves ~half the target's range) */
        s->ph += (uint32_t)ni * (730454u + (uint32_t)((walk >> 5) * 146));   /* flutter 7.5 Hz +-20 % */
        flut = osc_sine(s->ph) * 3;                     /* +-1.5 samples, Q16 */
        prod = ((uint32_t)(32768 + walk) * (uint32_t)s->u.lo.dep) >> 8;
        dt = (4u << 16) + (uint32_t)((int32_t)((prod * IT_WOW_K) >> 8) + flut);
    } else {
        s->ph += (uint32_t)ni * 54106u;                 /* 33 1/3 rpm */
        prod = ((uint32_t)(32768 + osc_sine(s->ph)) * (uint32_t)s->u.lo.dep) >> 8;
        dt = (4u << 16) + ((prod * IT_WARP_K) >> 8);
    }
    dcur = s->hold ? s->hold : (int32_t)dt;             /* (the first block starts on its delay) */
    dd = ((int32_t)dt - dcur) / ni;
    s->hold = (int32_t)dt;
    for (i = 0; i < n; i++, dcur += dd) {
        int32_t x = b[i], v, y, r0, r1, fr;
        s->w += clamp(tw - s->w, -SL_SLOPE, SL_SLOPE);
        r0 = (int32_t)(s->wp - ((uint32_t)dcur >> 16)) & (int32_t)(IL_LEN - 1u);
        r1 = (r0 - 1) & (int32_t)(IL_LEN - 1u);
        fr = (dcur >> 1) & 0x7FFF;
        v = s->line[r0] + (((s->line[r1] - s->line[r0]) * fr) >> 15);
        s->line[s->wp] = (int16_t)clamp((x + 4) >> 3, -32767, 32767);
        s->wp = (uint16_t)((s->wp + 1u) & (IL_LEN - 1u));
        if (tape) {
            y = ins_tape_sat(v, g, comp);               /* SAT: tanh of (voice-sum / 65536) x gain */
            y += (((int32_t)noise32(&s->u.lo.rng) >> 24) * 3) >> 3;   /* the hiss: +-48, flat */
            y <<= 4;
            v = mulq16(y - s->lp1, (uint32_t)k);        /* TONE: one one-pole high-cut */
            y = v + s->lp1;
            s->lp1 = y + v;
            y = (y + 8) >> 4;
        } else {
            uint32_t r = (uint32_t)noise32(&s->u.lo.rng);   /* (the top bits: a click; the low 16: the dust) */
            int32_t nz;
            y = v << 3;
            s->hp += ((y << 8) - s->hp) >> 6;           /* the band: ~110 Hz high-pass .. */
            y -= s->hp >> 8;
            y <<= 4;
            v = mulq16(y - s->lp1, (uint32_t)k);        /* .. ~6 kHz low-pass */
            y = v + s->lp1;
            s->lp1 = y + v;
            y = (y + 8) >> 4;
            if (r < thr) {                              /* a click: size 100 .. 3500 (mostly small), sign, decay 4 .. 32 samples */
                uint32_t r2 = (uint32_t)noise32(&s->u.lo.rng), u = r2 >> 24;
                int32_t amp = 100 + (int32_t)(u * u / 20u);
                s->u.lo.env = r2 & 0x100u ? -amp : amp;
                s->cnt = (uint8_t)(2u + ((r2 >> 9) & 3u));
            }
            if (s->u.lo.env | s->u.lo.ep) {
                s->u.lo.ep += ((s->u.lo.env << 4) - s->u.lo.ep) >> 3;
                y += s->u.lo.env - (s->u.lo.ep >> 4);   /* (the click less its own low-pass: no area) */
                if (s->u.lo.env) {                      /* its tail: the level falls by 1 / 2^cnt, always to 0 */
                    int32_t m = s->u.lo.env < 0 ? -s->u.lo.env : s->u.lo.env;
                    m -= (m + (1 << s->cnt) - 1) >> s->cnt;
                    s->u.lo.env = s->u.lo.env < 0 ? -m : m;
                }
            }
            nz = (((int32_t)(int16_t)r) * namp) >> 15;
            s->lp2 += ((nz << 4) - s->lp2) >> 3;
            y += nz - (s->lp2 >> 4);                    /* dust: the noise through a ~880 Hz high-pass */
        }
        s->dc += ((y << 8) - s->dc) >> 9;               /* DC blocker, ~14 Hz */
        y -= s->dc >> 8;
        b[i] = pf_mix(x, y, s->w);
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
        if (ty >= IT_TAPE)                              /* (TAPE, VINYL: the track's own noise, the same at every start; PHASR's words are the same words) */
            s->u.lo.rng = (int32_t)(0x2545F491u ^ ((uint32_t)(t - trk) * 0x9E3779B9u)) | 1;
    }
    if (s->type >= IT_TAPE)
        ins_lofi(s, t, b, n, tw);
    else if (s->type >= IT_PHASER)
        ins_swept(s, t, b, n, tw);
    else
        ins_drive(s, t, b, n, tw);
    t->ins_run = s->w != 0;                             /* (mix_part calls again while it fades) */
}

/* EQ (DUELL, SPEC 5c.3): one per part, after its INSERT (and DIST) and before the DUCK, the level, the pan and the sends: mono
 * per part like them (SPLIT's isolation holds; SPREAD's side signal gets the same filter, its own state). Three RBJ biquads in
 * a row: LOW shelf, MID peaking (a bell), HIGH shelf; GAIN -12..+12 dB in whole dB, FREQ a semitone step around the band's centre
 * (core.h eq_hz_q8), MID's Q 0.25 .. 4. A band at 0 dB is not run; all three at 0 (the default): the call is not made, the
 * block is not touched (bit for bit, no cost). The coefficients (Q28; made in 64 bit, a division each, then stored in 32 bit) are made
 * when one of the part's seven values changed; the filter is a transposed direct form II with 64-bit states and the output carried
 * with 8 more bits into the feedback. 0.2.25: the loop multiplies in 32 bit only (eq_run: the chip has a 32 x 32 -> 64
 * multiply-accumulate, no 64 x 64 multiply) and carries the output's rounding remainder into the next sample, so a quiet tail
 * comes back to exactly 0 (no stuck DC, no limit cycle). A band switched on starts from rest; all switched off, or the part silent and its
 * tail run out (eq_rest), the states are cleared. Changing a value moves the coefficients at once (no glide: a click is possible
 * on a loud, low band). */
typedef struct {
    int32_t b0, b1, b2, a1, a2;          /* Q28, divided by a0 (0.2.25: 32 bit; every setting fits, the largest is |b1| 5.94: tests/eq32_test.c) */
} eqc_t;
typedef struct {
    int8_t key[7];                       /* LOW gain, freq, MID gain, freq, Q, HIGH gain, freq: the values the coefficients are of */
    uint8_t on;                          /* bit k: band k runs (gain not 0) */
    uint8_t dr[3];                       /* 0.2.25, per band: blocks run flat since its gain read 0 (the switch OFF, or the gain turned to 0) while its states held a signal: they ring out through the band's own poles, then the band rests (0.2.24: all three at once, when the last gain read 0) */
    eqc_t c[3];
    int64_t s[2][3][2];                  /* the states (Q36): [0] the part's signal, [1] SPREAD's side; band; s1 s2 */
} eq_t;
static eq_t eqs[NLIVE];
#define EQ_DRAIN 16u                             /* blocks the bands ring out flat before they rest (23 ms) */
/* eq_live: t->eqlv (0 / 1), the part's EQ states may hold a signal (eq_rest clears them) */
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

/* a coefficient b / a0 in Q28, saturated to 32 bit (never reached: every setting's fits, tests/eq32_test.c) */
static __attribute__((noinline)) int32_t eq_q28(int64_t b, int64_t a0)
{
    int64_t v = (b << 28) / a0;
    return v > 2147483647ll ? 2147483647 : v < -2147483648ll ? (int32_t)-2147483648ll : (int32_t)v;
}
/* sin h / h and cos h by the Taylor sum to h^9, Horner's way: t = 1 - h2 t / d for the divisors d (sin: 72 42 20 6, cos: 56 30 12 2), the first step on t = 1
 * (h2 * 1 >> 28 is h2 itself). One loop, not eight inlined 64-bit constant divisions (range Z, 418 B; the same integer arithmetic as before, bit for bit) */
static const uint8_t EQ_TS[4] = {72, 42, 20, 6}, EQ_TC[4] = {56, 30, 12, 2};
static __attribute__((noinline)) int64_t eq_taylor(int64_t h2, const uint8_t *dv)
{
    int64_t t = EQ_ONE;
    uint32_t k;
    for (k = 0; k < 4u; k++)
        t = EQ_ONE - (h2 * t >> 28) / dv[k];
    return t;
}
/* band bd (0 LOW shelf, 1 MID bell, 2 HIGH shelf) at gain g dB, semitone step fr, MID's Q step q -> c */
static __attribute__((noinline)) void eq_design(eqc_t *c, uint32_t bd, int32_t g, int32_t fr, int32_t q)
{
    int64_t h = ((int64_t)eq_hz_q8(bd, fr) * EQ_HK) >> 10;              /* pi f / FS, Q28 (at most 1.15) */
    int64_t h2 = (h * h) >> 28, sn, cs, sw, cw, a0, b0, b1, b2, a1, a2;
    int64_t A = EQ_A[g + 12], SA = EQ_SA[g + 12], AM = A - EQ_ONE, AP = A + EQ_ONE, T, P1, P2;
    sn = h * eq_taylor(h2, EQ_TS) >> 28;
    cs = eq_taylor(h2, EQ_TC);
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
    c->b0 = eq_q28(b0, a0);
    c->b1 = eq_q28(b1, a0);
    c->b2 = eq_q28(b2, a0);
    c->a1 = eq_q28(a1, a0);
    c->a2 = eq_q28(a2, a0);
}

/* one band over a block, state s (s1 s2). 0.2.25: 32 x 32 -> 64 multiplies only (0.2.24 multiplied 64 x 64: ~85 device instructions a sample
 * a band, now ~32). The sample in Q8 (x 256: 32 bit), the coefficients Q28, the sums and states Q36 (the five coefficients' sizes sum to under 16
 * at every setting and |x|, |y| <= 2^31: the 64-bit sums cannot wrap at full scale). y, the output with 8 more bits (Q8), is the sum's bits 28..59:
 * its high word clamped (24-bit full scale, as before), and the 28 bits below it, the rounding remainder, go into the next sample's sum
 * (first-order error feedback: the rounding error is not left to circulate through the poles, so a quiet tail ends at exactly 0). The
 * output rounded to 24 bit. Within 1 LSB of the ideal filter at -20 dBFS (0.2.24's: up to ~400 LSB on a low LOW band); tests/eq32_test.c */
static __attribute__((noinline)) void eq_run(const eqc_t *c, int64_t *s, int32_t *b, uint32_t n)
{
    uint32_t i;
    int64_t s1 = s[0], s2 = s[1];
    const int32_t b0 = c->b0, b1 = c->b1, b2 = c->b2, a1 = c->a1, a2 = c->a2;
    for (i = 0; i < n; i++) {
        int32_t x = clamp(b[i], -8388608, 8388607) * 256, y, hi;   /* Q8 */
        int64_t a = s1 + (int64_t)b0 * x;                           /* Q36 */
        hi = clamp((int32_t)(a >> 32), -(1 << 27), (1 << 27) - 1);
        y = (int32_t)(((uint32_t)hi << 4) | ((uint32_t)a >> 28));  /* Q8: a >> 28, clamped */
        s1 = s2 + ((uint32_t)a & 0x0FFFFFFFu) + (int64_t)b1 * x - (int64_t)a1 * y;
        s2 = (int64_t)b2 * x - (int64_t)a2 * y;
        b[i] = (y >> 8) + ((y >> 7) & 1);                           /* (y + 128) >> 8 without the overflow */
    }
    s[0] = s1, s[1] = s2;
}

/* part t's EQ on its block b (side 0: the part's signal; 1: SPREAD's side signal); not called while all three gains are 0
 * and no state is live. 0.2.25: each band on its own: a band whose gain is 0 is not run (it costs nothing; one band set: that band only),
 * a band turned to 0 rings out flat (EQ_DRAIN blocks) and then rests, a band whose values moved is designed again (the others are not).
 * (0.2.24 tested band k's gain at key[2 k]: for HIGH that was MID's Q, so HIGH ran only with Q off its middle: fixed) */
static const uint8_t EQ_GK[3] = {0u, 2u, 5u};           /* band k's gain in key[]; its values: EQ_GK[k] .. EQ_GK[k] + EQ_KN[k] - 1 */
static const uint8_t EQ_KN[3] = {2u, 3u, 2u};
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
    for (k = 0; k < 3u; k++) {
        uint32_t gk = EQ_GK[k], was = (e->on >> k) & 1u;
        if (key[gk]) {                                  /* the band is in use */
            if (!was)                                   /* (newly on: from rest) */
                memset(e->s[0][k], 0, sizeof e->s[0][k]), memset(e->s[1][k], 0, sizeof e->s[1][k]);
            if (!was || e->dr[k] || memcmp(key + gk, e->key + gk, EQ_KN[k])) {
                eq_design(&e->c[k], k, key[gk], key[gk + 1u], k == 1u ? key[4] : 0);
                memcpy(e->key + gk, key + gk, EQ_KN[k]);
            }
            e->dr[k] = 0;
            on |= 1u << k;
        } else if (was && t->eqlv && e->dr[k] < EQ_DRAIN) {   /* turned to 0: ring out flat, then rest */
            if (!e->dr[k]) {
                eq_design(&e->c[k], k, 0, e->key[gk + 1u], k == 1u ? e->key[4] : 0);
                e->key[gk] = 0;
            }
            if (!side)
                e->dr[k]++;
            on |= 1u << k;
        } else if (was) {                               /* rung out (or the part's tail ended): the band rests */
            memset(e->s[0][k], 0, sizeof e->s[0][k]), memset(e->s[1][k], 0, sizeof e->s[1][k]);
            e->key[gk] = 0;
            e->dr[k] = 0;
        }
    }
    e->on = (uint8_t)on;
    if (!on) {                                          /* every band at rest: nothing runs, the part's EQ is not called again until a gain is set */
        t->eqlv = 0;
        return;
    }
    t->eqlv = 1;
    for (k = 0; k < 3u; k++)
        if ((on >> k) & 1u)
            eq_run(&e->c[k], e->s[side][k], b, n);
}

/* the part is silent and its tail has run out: the states to rest (nothing sounds; a note after starts clean) */
static __attribute__((noinline)) void eq_rest(track_t *t)
{
    uint32_t p = (uint32_t)(t - trk) % NLIVE;
    memset(eqs[p].s, 0, sizeof eqs[p].s);
    t->eqlv = 0;
}
/* does part t's EQ do anything now (a gain not 0) or hold a signal still */
#define EQ_ON(t) (((t)->p[P_DKNOTE] == 0 && ((t)->p[P_EQLG] | (t)->p[P_EQMG] | (t)->p[P_EQHG])) | (t)->eqlv)   /* (0.2.25 layout round: the EQ switch
                                                         * is gone, its id 106 is free and always reads 0 (params.c PFREE); the test stays only so the hot RAM code (mix_spread) is byte for byte as it was) */

/* one block of the whole mix (shared with hostsim.c): events -> each part (with its modulation matrix)
 * -> dist -> INSERT -> EQ -> mute -> DUCK -> level / pan / sends -> buses -> master; out: stereo Q15 */
/* MIXSPLIT job 2: the block's shared control state, read ONCE per block (blk_snap, in mix_block after the event stage and PERFORM's begin) into
 * this struct. The parts' stages (part_voice .. part_mix, mix_spread, pfx_*, perf_mute*) and mix_gather receive it by pointer and only read it:
 * the two parts then see the same values whatever order, or on whichever core, they run, and a value the UI or the key scan writes (mods_off,
 * perf_*, mix_bal: volatile) cannot change between the parts. Nothing in it is written by a part. mix_sends and mix_master (after the parts) still
 * read the live variables: nothing writes them between the parts and those stages.
 *   dm_*          the MODE (dm_split: the mix runs SPLIT)    snd_st  MODULES SENDS state    mods_off  MODULES switches    sp_tap  SPACE's tap
 *   perf_*, pf_*  PERFORM: the knobs, the effects running, the parts whose mute is on (pf_mute) and the mute's goal gain Q15 (mute_tg: 0 held, 32768 free)
 *   mix_bal       BALANCE
 *   (the voice budget, voice.c voices_busy / voice_room, is not in it: only the event stage uses it, live, a note-on changes it inside the block; counting it
 *   every block costs 10 host instructions a sample for a value no part stage reads) */
typedef struct {
    int32_t dm_g, mix_bal;
    uint32_t pf_act;
    int32_t mute_tg[NLIVE];
    int32_t duck_key[NLIVE];     /* each part's DUCK key follower at the snapshot (the block before's final): the ducked part's input (duck_part) */
    uint8_t dm_cur, dm_split, dm_hold, dm_clr;
    int8_t dm_dir, perf_space, perf_filt, perf_depth;
    uint8_t snd_st, mods_off, sp_tap, pf_mute;
} blockstate_t;
#include "perform.c"                                 /* DUELL 0.2.5 PERFORM: the white keys' master effects and mutes */

/* K2 SPACE a block (sv above): SPACE glides, its values from the stored ones; in the middle and at rest: off (the buses read song.g) */
static __attribute__((noinline)) void space_vals(void)   /* SPACE's values from song.g's (space_step; DUEL's bus B again, split_buses) */
{
    int32_t s = sv.spc, a = s < 0 ? -s : s, r0 = song.g[G_RSIZE], f0 = song.g[G_DFDBK] * 230, c0 = song.g[G_CDEPTH] * 6;
    if (s < 0) {                                        /* shrink */
        sv.rsize = r0 - (r0 * 3 * a) / (4 * 25600);
        sv.dfb = f0 - (int32_t)(((int64_t)f0 * 3 * a) / (4 * 25600));
        sv.cdep = c0 - (c0 * 4 * a) / (5 * 25600);
        sv.sg = 32768 - (int32_t)((27853LL * a) / 25600);   /* x 0.15 at the end */
        sv.ac = sv.ad = sv.ar = 0;                      /* (to the left a pure shrink: a send at 0 stays 0) */
    } else {                                            /* expand */
        int32_t top = song.g[G_RTYPE] == 4 ? (r0 > SV_RESO_TOP ? r0 : SV_RESO_TOP) : 150;   /* (RESO: SV_RESO_TOP, above) */
        sv.rsize = r0 + ((top - r0) * a) / 25600;
        sv.dfb = f0 + (int32_t)(((int64_t)(f0 > 30150 ? 0 : 30150 - f0) * a) / 25600);
        sv.cdep = c0 + (((c0 > SV_CDEP_TOP ? c0 : SV_CDEP_TOP) - c0) * a) / 25600;   /* (the knob's largest: SV_CDEP_TOP) */
        sv.sg = 32768 + (int32_t)((19661LL * a) / 25600);   /* x 1.6 at the end */
        sv.ac = SV_ADD_C * a / 25600;                   /* the send floor: 0 in the middle .. SV_ADD_* at +100 */
        sv.ad = SV_ADD_D * a / 25600;
        sv.ar = SV_ADD_R * a / 25600;
    }
    if (!sv.on)
        sv.sg = 32768, sv.ac = sv.ad = sv.ar = 0;
}
/* DUELL 0.2.14 GLO > MODULES, SENDS (core.h MODS_SENDS): the state of the shared buses (mix_block, snd_step below). SND_ON: as ever, bit for bit.
 * SND_FADE: the returns (the buses' whole contribution: wet, HALL's and PING-PONG's sides, the delay modulation's side, DUEL's two sides)
 * glide out over 993 samples (22.5 ms, the MODE dip's slope) against the dry mix, or back in when ON comes again before the end. SND_OFF:
 * the parts send nothing, the buses, the side mixes and SPACE are skipped, the output is the dry mix; its first blocks clear the lines (the
 * delay line a DM_CHUNK a block, then the chorus and reverb lines) so an ON starts the buses from silence, no stale tail. SPACE glides to
 * its middle while OFF (space_goal: its width ends exactly at 1 and 0, so the output is the dry mix bit for bit once it is there) */
enum { SND_ON, SND_FADE, SND_OFF };
static uint8_t snd_st;                   /* SND_* */
static uint8_t snd_clr;                  /* SND_OFF: the delay line's chunks cleared so far; DLY_LEN / DM_CHUNK + 1: all the lines clear */
static int32_t snd_x;                    /* SND_FADE: the returns' share taken away, Q15 (0 the whole return, 32768 none) */
static int32_t snd_dl[CTL], snd_dr[CTL]; /* SND_FADE: this block's dry mix (before the buses) */
static uint8_t snd_panic;                /* STOP ALL (seq.c stop_all): a one-shot SENDS OFF, FADE then the lines cleared, then back to the user's own state */
static uint32_t snd_off(void) { return ((mods_off & MODS_SENDS) != 0u) || snd_panic; }   /* the buses to be off: the user's switch or the panic */
static int32_t space_goal(void) { return snd_st == SND_OFF && (mods_off & MODS_SENDS) ? 0 : perf_space; }   /* (SENDS OFF: SPACE to its middle; STOP ALL's flush leaves it) */
static __attribute__((noinline)) void space_step(void)
{
    sv.spc += clamp(space_goal() * 256 - sv.spc, -64, 64);   /* Q8, -25600 .. 25600 */
    sv.on = sv.spc != 0;
    sv.sg0 = sv.sg;
    space_vals();
}
/* SPACE's send gain on a block of sends (a ramp from the last block's gain: no step), and to the right the parts' send floors (space_tap: fc,
 * fd, fr, what they add to each send, added after the gain). RESO's send is not raised above x 1 to the right (0.2.9): x 1.6 into its four
 * tuned combs drove them into their int16 limit (SV_RESO_TOP above); to the left it falls as the others */
static __attribute__((noinline)) void space_sends(int32_t *c, int32_t *d, int32_t *r, const int32_t *fc, const int32_t *fd, const int32_t *fr,
                                                  uint32_t n)
{
    uint32_t i;
    int32_t g = sv.sg0, st = (sv.sg - sv.sg0) / (int32_t)n, rg = g, rst = st;
    if (song.g[G_RTYPE] == 4 && (sv.sg > 32768 || sv.sg0 > 32768)) {
        rg = sv.sg0 > 32768 ? 32768 : sv.sg0;
        rst = ((sv.sg > 32768 ? 32768 : sv.sg) - rg) / (int32_t)n;
    }
    if (sp_tap) {                                       /* (the floors, 0.2.9) */
        for (i = 0; i < n; i++) {
            g += st;
            rg += rst;
            c[i] = (((c[i] >> 2) * g) >> 13) + fc[i];
            d[i] = (((d[i] >> 2) * g) >> 13) + fd[i];
            r[i] = (((r[i] >> 2) * rg) >> 13) + fr[i];
        }
        return;
    }
    for (i = 0; i < n; i++) {
        g += st;
        rg += rst;
        c[i] = ((c[i] >> 2) * g) >> 13;
        d[i] = ((d[i] >> 2) * g) >> 13;
        r[i] = ((r[i] >> 2) * rg) >> 13;
    }
}

static __attribute__((noinline)) void events_block(uint32_t n);   /* seq.c (a function of its own: mix_events) */
static int32_t send_c[CTL], send_d[CTL], send_r[CTL], wet[CTL];
/* MIXSPLIT job 2: each part adds its block into ITS OWN accumulators (pacc[pp]: the dry mix l / r, the three sends sc / sd / sr, SPACE's
 * floors fc / fd / fr). A part ASSIGNS its block into them (no clear, no read-modify-write; a send at 0 is left alone and flagged off in pacc_on[pp]);
 * a silent part writes nothing. mix_gather then sums the parts' that are on into the globals above (mix_l,
 * mix_r, send_*, sp_f*) in a fixed order, after both parts and before mix_sends: plain 32-bit integer adds, so the sum is the same in
 * whatever order the parts ran. In SPLIT (DUEL) part 1's sends and floors go to bus A's arrays (sa_*, sp_f*_a), the others' to bus B's */
typedef struct {
    int32_t l[CTL], r[CTL];
    int32_t sc[CTL], sd[CTL], sr[CTL];
    int32_t fc[CTL], fd[CTL], fr[CTL];
} part_acc_t;
_Static_assert(sizeof(part_acc_t) == 8u * CTL * sizeof(int32_t), "part_acc_t is eight arrays of CTL (mix_gather indexes them as one)");
static part_acc_t pacc[NLIVE];
static uint8_t pacc_on[NLIVE];                          /* the part wrote its accumulators this block: bit 0 l / r, bits 1..3 the sends sc / sd / sr that are not 0, bit 4 the floors fc / fd / fr (a silent part writes none: mix_gather skips it) */

/* SPACE's STEREO WIDTH (0.2.9, the owner: "SPACE should decrease the stereo space to completely mono, and widen it on the plus side"), STEREO and
 * LAYER only, on the mix before the master (mix_l / mix_r with the buses' return, wet folded in): mid M = (L + R) / 2, side S = (L - R) / 2,
 * out L = g (M + gs S), R = g (M - gs S). gs = 1 + s to the left (0 at -100: L = R, mono), 1 + SW_SIDE s to the right (x 1.9 at +100). The
 * guard g (0.0 .. 1, never a boost): the wider mix keeps the loudness it had, g = sqrt((Em + Es) / (Em + gs^2 Es)) from the mid's and the
 * side's energies (smoothed over ~23 ms), so a mix with much side (HALL's return) is not pushed into the limiter; a mono source has no side,
 * stays mono and as loud. a = g (1 + gs) / 2, b = g (1 - gs) / 2 (Q12): L' = a L + b R, R' = b L + a R, two multiplies a channel; they glide a
 * sample at a time from the last block's (no seam); at SPACE 0 they end at 1 and 0 exactly and the stage stops (mix_block: bit for bit). In
 * DUEL the two sides are part 1 and part 2 (dual mono): no width there (mix_block never calls it; the page says so, ui_perform.c) */
#define SW_SIDE 29491                           /* the side's gain to the right: 1 + 0.9 s (Q15 of s) */
static int32_t sw_a = 4096, sw_b;                /* the gains reached (Q12) */
static int32_t sw_em, sw_es;                     /* the mid's and the side's energy, smoothed */
static int32_t sw_g = 4096;                      /* the guard (Q12), worked out every 4th block (sw_n) */
static uint8_t sw_n;
static uint32_t sw_isqrt(uint32_t x)            /* floor(sqrt(x)) */
{
    uint32_t r = 0, b = 1u << 30;
    while (b > x)
        b >>= 2;
    while (b) {
        if (x >= r + b) {
            x -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return r;
}
static __attribute__((noinline)) void space_width(uint32_t n)
{
    uint32_t i;
    int32_t gs, g, a1, b1, da, db, a, b, em = 0, es = 0;
    int32_t sp = sv.spc;
    gs = sp <= 0 ? 4096 + sp * 4096 / 25600 : 4096 + (int32_t)((int64_t)sp * SW_SIDE / 25600 >> 3);   /* (Q12) */
    if (sp <= 0 || sw_es <= 0)
        sw_g = 4096;
    else if (!(sw_n++ & 3u)) {                          /* the guard (the energies of the blocks before: ~23 ms), every 4th block */
        int64_t num = (int64_t)sw_em + sw_es, den = (int64_t)sw_em + (((int64_t)sw_es * gs * gs) >> 24);
        uint32_t r = den > 0 ? (uint32_t)(num * 16777216 / den) : 16777216u;   /* (Q24, at most 1) */
        sw_g = (int32_t)sw_isqrt(r > 16777216u ? 16777216u : r);   /* (Q12) */
    }
    g = sw_g;
    a1 = sp ? (g * (4096 + gs)) >> 13 : 4096;
    b1 = sp ? (g * (4096 - gs)) >> 13 : 0;
    a = sw_a << 8, b = sw_b << 8;                       /* (Q20 while they glide) */
    da = ((a1 - sw_a) << 8) / (int32_t)n, db = ((b1 - sw_b) << 8) / (int32_t)n;
    for (i = 0; i < n; i += 8u) {                       /* the energies: every 8th sample (enough for a ~23 ms average) */
        int32_t l = clamp(mix_l[i] + wet[i], -524287, 524287), r = clamp(mix_r[i] + wet[i], -524287, 524287);
        int32_t m = (l + r) >> 8, d = (l - r) >> 8;     /* (4096 at most: 4 of them fit) */
        em += m * m >> 2, es += d * d >> 2;
    }
    for (i = 0; i < n; i++) {
        int32_t l = mix_l[i] + wet[i], r = mix_r[i] + wet[i], ai, bi;
        a += da, b += db;
        ai = a >> 8, bi = b >> 8;
        l = clamp(l, -1048575, 1048575) >> 3;           /* (l a + r b in 32 bits: |a| + |b| <= 7782) */
        r = clamp(r, -1048575, 1048575) >> 3;
        mix_l[i] = (l * ai + r * bi) >> 9;              /* (x 8: the >> 3 above) */
        mix_r[i] = (l * bi + r * ai) >> 9;
    }
    memset(wet, 0, n * sizeof wet[0]);
    sw_a = a1, sw_b = b1;                               /* (exactly the targets: what the glide did not reach, under a step of 1/n) */
    sw_em += (em - sw_em) >> 3;
    sw_es += (es - sw_es) >> 3;
}
static inline int sw_on(void) { return (sv.spc | sv.spc0) || sw_a != 4096 || sw_b; }

/* BASS MONO (DUELL 0.2.19, the page MASTER; the owner's request for clubs and vinyl): the master below a chosen frequency in mono, stereo above it,
 * so the wide effects (SPACE, chorus, SPREAD, PING-PONG, the reverbs) do not smear the low end. The side (L - R) goes through a 2-pole
 * Butterworth high-pass (12 dB/oct, -3.01 dB at the printed Hz); the part of the side it takes out is moved from the two channels, L - d / 2
 * and R + d / 2, so L + R is not touched (below the corner L = R = the mid). The wet bus (mono: the same on both) is no side, so it is not read.
 * Where: the end of mix_block's mix (after the sends, SPACE's width, PERFORM and the MODE's dip), before master_out (MASTER's gain is linear: the order with it does not
 * matter; then the DC blocker, the limiter, the soft clip). USB audio and the
 * DAC are the one output (uac_tap reads it): both get it. Not in DUEL (SPLIT: part 1 hard left, part 2 hard right for an external mixer: the
 * stage would mix them): it does not run there, and its state is cleared where the mix switches (dm_lines_clear).
 * OFF: not a bit of code run (bm_sel and bm.a both 0: mix_block's one test), the mix bit for bit as before. ON from OFF the share a (Q15)
 * fades in over 1024 samples (23 ms, 32 a sample) with the filter running from rest; OFF fades it out, then the state is dropped. A change of the
 * corner moves the coefficients 1/16 of the way a block (11.6 ms) and in straight lines inside it (as perform.c pf_chan: the same trapezoidal
 * state-variable filter, coefficients Q30, states x 1024, products 32 x 32 -> 64 bits). The table: g = tan(pi f / 44100), a1 = 1 / (1 + g (g + sqrt 2)),
 * a2 = g a1, a3 = g a2 (33 constants, kept here and checked by tests/bassmono_test.c) */
/* BM_N (core.h): the corners 40 .. 400 Hz in 1/3-octave steps (R10 series), value 1..BM_N; 0 = OFF */
#define BM_SH 10                                     /* the states carry 10 more bits than the signal */
#define BM_XMAX (1 << 18)                            /* the side's clamp (8 x full scale: x 1024 within 2^28) */
#define BM_K 1518500250                              /* the damping sqrt 2, Q30 */
#define BM_FADE 32                                   /* the share's ramp a sample, Q15 */
static const int32_t BM_C[BM_N][3] = {
    {1069423538, 3047349, 8683},      {1068346683, 3805356, 13554},     {1066948392, 4788485, 21491},
    {1065122619, 6070236, 34595},     {1062978650, 7572568, 53946},     {1060304755, 9441989, 84081},
    {1056572595, 12043408, 137278},   {1052323346, 14994082, 213644},   {1047035795, 18649137, 332167},
    {1040201639, 23345989, 523971},   {1031331827, 29395934, 837869},
};
static volatile uint8_t bm_sel;                      /* the setting now (ui_input copies it every frame): 0 OFF, 1..BM_N the corner */
static struct { int32_t c[3], z0, z1, a; } bm;       /* the coefficients now (Q30), the states, the share (Q15; 0 = at rest) */
#define BM_STEP v3 = x - z1;                                                                    \
                v1 = (int32_t)(((int64_t)a1 * z0 + (int64_t)a2 * v3 + (1 << 29)) >> 30);        \
                v2 = z1 + (int32_t)(((int64_t)a2 * z0 + (int64_t)a3 * v3 + (1 << 29)) >> 30);   \
                z0 = 2 * v1 - z0;                                                               \
                z1 = 2 * v2 - z1;                                                               \
                low = (int32_t)(((int64_t)BM_K * v1) >> 30) + v2;                               \
                low = (low + (1 << (BM_SH - 1))) >> BM_SH
static __attribute__((noinline)) void bass_mono(uint32_t n)
{
    uint32_t i, sel = bm_sel;
    int32_t a = bm.a, at = sel ? 32768 : 0, a1 = bm.c[0], a2 = bm.c[1], a3 = bm.c[2], z0 = bm.z0, z1 = bm.z1;
    int32_t d1 = 0, d2 = 0, d3 = 0, x, v1, v2, v3, low;
    if (sel) {                                           /* the corner: from rest at once, else 1/16 of the way a block and a line inside it */
        const int32_t *ct = BM_C[(sel - 1u) % BM_N];
        int32_t df[3], j;
        if (!a)
            a1 = ct[0], a2 = ct[1], a3 = ct[2], z0 = z1 = 0;
        df[0] = ct[0] - a1, df[1] = ct[1] - a2, df[2] = ct[2] - a3;
        for (j = 0; j < 3; j++)
            df[j] = df[j] > -16 && df[j] < 16 ? df[j] : df[j] >> 4;
        if (df[0] > -32 && df[0] < 32 && df[1] > -32 && df[1] < 32 && df[2] > -32 && df[2] < 32) {
            a1 += df[0], a2 += df[1], a3 += df[2];       /* (a step too small for a line: taken at once, 3e-8 of a coefficient) */
        } else {
            d1 = n == CTL ? df[0] >> CTL_LOG2 : df[0] / (int32_t)n;
            d2 = n == CTL ? df[1] >> CTL_LOG2 : df[1] / (int32_t)n;
            d3 = n == CTL ? df[2] >> CTL_LOG2 : df[2] / (int32_t)n;
        }
    }
    if (a == 32768 && at == 32768 && !(d1 | d2 | d3)) {  /* steady: the one loop */
        for (i = 0; i < n; i++) {
            x = mix_l[i] - mix_r[i];
            x = clamp(x, -BM_XMAX, BM_XMAX) << BM_SH;
            BM_STEP;
            low >>= 1;
            mix_l[i] -= low;
            mix_r[i] += low;
        }
    } else {                                             /* the corner or the share moving */
        for (i = 0; i < n; i++) {
            a1 += d1, a2 += d2, a3 += d3;
            a += clamp(at - a, -BM_FADE, BM_FADE);
            x = mix_l[i] - mix_r[i];
            x = clamp(x, -BM_XMAX, BM_XMAX) << BM_SH;
            BM_STEP;
            low = mulq16(low, (uint32_t)a);              /* (half of low x a / 32768) */
            mix_l[i] -= low;
            mix_r[i] += low;
        }
    }
    bm.c[0] = a1, bm.c[1] = a2, bm.c[2] = a3;
    bm.a = a;
    bm.z0 = a || sel ? z0 : 0;                           /* (faded out: at rest, the state dropped) */
    bm.z1 = a || sel ? z1 : 0;
}
/* MASTER STEREO WIDTH (DUELL 0.2.23, the page WIDTH; the owner's request). A mid / side width on the mix, STEREO and LAYER: WIDTH 0..200 %, 100 = as it is (the stage
 * is not run: bit for bit), 0 = mono, 200 = the side twice as loud. The mid (L + R) is never touched, so the mono sum stays exactly what it was at every setting; only the side
 * (L - R) moves, by k = (WIDTH - 100) / 100 of itself: L + d, R - d with d = k x side / 2.
 *   wider (k > 0): only the side above the crossover, 125 Hz (a 2-pole Butterworth high-pass of the side: BASS MONO's filter and its 125 Hz coefficients, BM_C[WD_XO]),
 *                  so the bass is never made wider;
 *   narrower (k < 0): the whole side (no filter): toward mono it narrows the bass too, which is always safe.
 * Where: after the MODE's dip and before BASS MONO, so BASS MONO, when it is on, still has the last word below its corner (a wide mix stays mono there), then COMP sees the
 * mix as it will be heard. The wet bus (mono: the same on both sides) has no side, so it is not read. Not in DUEL (its sides are part 1 and part 2, dual mono: mix_block never
 * calls it there and dm_lines_clear drops its state). k glides WD_GLIDE a sample (0 -> +-100 % in 23 ms): no click; back at 100 % k ends at 0 exactly, the crossover's
 * states are dropped and the gate shuts */
#define WD_XO 5u                                        /* the crossover: BM_HZ[5] = 125 Hz */
#define WD_GLIDE 32                                     /* k's step a sample, Q15 */
static volatile uint8_t wd_sel = 100;                   /* the setting (ui_input copies it every frame): 0..200 %, 100 = neutral */
static struct { int32_t k, z0, z1; } wdn;               /* k now (Q15: -32768 .. 32768), the crossover's states */
static __attribute__((noinline)) void width_stage(uint32_t n)
{
    uint32_t i, w = wd_sel <= 200u ? wd_sel : 100u;
    int32_t kt = ((int32_t)w - 100) * 32768 / 100, k = wdn.k, z0 = wdn.z0, z1 = wdn.z1, s, x, v1, v2, v3, low, d;
    const int32_t a1 = BM_C[WD_XO][0], a2 = BM_C[WD_XO][1], a3 = BM_C[WD_XO][2];
    if (k == kt && k < 0) {                              /* steady, narrower: the whole side */
        for (i = 0; i < n; i++) {
            d = (int32_t)(((int64_t)(mix_l[i] - mix_r[i]) * k) >> 16);
            mix_l[i] += d;
            mix_r[i] -= d;
        }
        z0 = z1 = 0;
    } else if (k == kt) {                                /* steady, wider: the side above the crossover */
        for (i = 0; i < n; i++) {
            s = mix_l[i] - mix_r[i];
            x = clamp(s, -BM_XMAX, BM_XMAX) << BM_SH;
            BM_STEP;
            d = (int32_t)(((int64_t)(s - low) * k) >> 16);
            mix_l[i] += d;
            mix_r[i] -= d;
        }
    } else {                                             /* k gliding (the filter runs on both sides of 100 %: no seam where k changes sign) */
        for (i = 0; i < n; i++) {
            k += clamp(kt - k, -WD_GLIDE, WD_GLIDE);
            s = mix_l[i] - mix_r[i];
            x = clamp(s, -BM_XMAX, BM_XMAX) << BM_SH;
            BM_STEP;
            d = (int32_t)(((int64_t)(k < 0 ? s : s - low) * k) >> 16);
            mix_l[i] += d;
            mix_r[i] -= d;
        }
    }
    wdn.k = k;
    wdn.z0 = k ? z0 : 0;                                 /* (back at 100 %: at rest, the states dropped) */
    wdn.z1 = k ? z1 : 0;
}
#undef BM_STEP

/* MASTER COMPRESSOR (DUELL 0.2.21, the page COMP; the owner's request). A stereo-linked feed-forward compressor at the end of the master chain: after PERFORM, SPACE's width and BASS MONO, before the MASTER gain,
 * the DC blocker, the limiter and the soft clip. It works on the mix as it is before MASTER, and its dB are those of the OUTPUT at the default MASTER (gain 0.5: 0 dB = a pre-MASTER amplitude of 65536); turning MASTER is a volume after it and never changes what is compressed; the wet bus's mono return is folded
 * into the mix when the stage runs.
 *   detector   every 8th sample (a hop, 0.18 ms): the peak of the hop's L and R (max(|L|, |R|): the link, both channels get one gain), held: the peak of the last 8 blocks of 64 samples and the one running
 *              (the hold, 11.6 .. 13 ms: a bass does not ripple the gain). pk (Q12 of the amplitude) moves toward the hold with the ATTACK's coefficient up and the RELEASE's down: pk += d x m >> sh, a product of the
 *              step and a coefficient of 8 bits shifted down (m / 2^sh, nearest to the printed time: core.h CMP_ATK_US / CMP_REL_MS; tools/comp_tables.py), a state with 12 fractional bits, so it does not
 *              stall short of its target (a Q16 coefficient on a small state does: about 2 dB). RELEASE is the time to cover 63 % of a fall, the hold included (so the 20 ms setting is a 12 ms hold and a pole of 8).
 *   gain       at each hop: the level in Q8 octaves of amplitude (256 = 6.02 dB; a leading-zero count and a 128-entry table); over = level - threshold;
 *              gain reduction gr = slope x over above the knee, slope x (over + knee / 2)^2 / (2 knee) inside it (the knee fixed at 6.02 dB), slope = 1 - 1 / ratio; the gain 2^((makeup - gr) / 256)
 *              (a table, a half step of interpolation), a straight line over the hop's 8 samples. An upward branch for an OTT mode would go in cmp_gr (a second, lower threshold): not built.
 *   makeup     fixed, from the threshold and the ratio: the gain reduction the same curve gives a level of -8 dB (CMP_REF), at most +12 dB. A level of -8 dB leaves the stage as loud as it came in,
 *              quieter signals are raised by the makeup, louder ones are brought down by what the curve takes less the makeup.
 *   knobs      threshold and slope glide to a new setting (8 and 1024 a hop: a 40 dB threshold sweep in 38 ms; the makeup is made from the gliding pair, so it is always one setting's curve); the effect's share `a`
 *              (Q15) fades in or out over 15 ms (CMP_A_STEP a hop): RATIO 1:1 is no code at all (cmp_cfg's ratio 0 with the share at 0 is mix_block's whole gate: the mix is bit for bit what it was)
 *              and 1:1 <-> a ratio is no click.
 *   not in DUEL (mix_block's gate; dm_lines_clear drops the state): its sides are the two parts, a linked gain would pump part 2 from part 1. */
#define CMP_RATIOS 9u                                   /* 1:1 (off), 1.5, 2, 3, 4, 6, 8, 12, INF */
#define CMP_KNEE 256                                    /* the knee, Q8 octaves (6.02 dB) */
#define CMP_AMAX 262144                                 /* the detector's ceiling (8 x full scale: pk <= 2^30) */
#define CMP_REF 3756                                    /* -8 dB in Q8 octaves (4096 - 8 x 42.52): the level the makeup keeps at unity */
#define CMP_MKMAX 510                                   /* the makeup's ceiling: +12 dB */
#define CMP_A_STEP 384                                  /* the share's step a hop: 85 hops, 15.5 ms */
static const uint32_t CMP_SLOPE[CMP_RATIOS] = {0, 21845, 32768, 43691, 49152, 54613, 57344, 60075, 65536};   /* 1 - 1 / ratio, Q16 */
static const uint8_t CMP_LG[128] = {   /* 256 log2(1 + (i + 1/2) / 128): the fraction of an octave of an amplitude, i = the top 7 bits after the leading 1 */
    1, 4, 7, 10, 13, 16, 18, 21, 24, 26, 29, 32, 34, 37, 40, 42,
    45, 47, 50, 52, 55, 57, 60, 62, 65, 67, 69, 72, 74, 77, 79, 81,
    84, 86, 88, 90, 93, 95, 97, 99, 102, 104, 106, 108, 110, 112, 114, 117,
    119, 121, 123, 125, 127, 129, 131, 133, 135, 137, 139, 141, 143, 145, 147, 149,
    151, 153, 155, 156, 158, 160, 162, 164, 166, 168, 169, 171, 173, 175, 177, 178,
    180, 182, 184, 185, 187, 189, 191, 192, 194, 196, 198, 199, 201, 203, 204, 206,
    208, 209, 211, 212, 214, 216, 217, 219, 220, 222, 224, 225, 227, 228, 230, 231,
    233, 234, 236, 238, 239, 241, 242, 244, 245, 247, 248, 249, 251, 252, 254, 255,
};
static const uint16_t CMP_EX[129] = {   /* 32768 2^(-i / 128): the gain of i / 128 octave of reduction, Q15 */
    32768, 32591, 32415, 32240, 32066, 31893, 31720, 31549, 31379, 31209, 31041, 30873,
    30706, 30541, 30376, 30212, 30048, 29886, 29725, 29564, 29405, 29246, 29088, 28931,
    28774, 28619, 28464, 28311, 28158, 28006, 27855, 27704, 27554, 27406, 27258, 27110,
    26964, 26818, 26674, 26530, 26386, 26244, 26102, 25961, 25821, 25681, 25543, 25405,
    25268, 25131, 24995, 24860, 24726, 24593, 24460, 24328, 24196, 24066, 23936, 23806,
    23678, 23550, 23423, 23296, 23170, 23045, 22921, 22797, 22674, 22552, 22430, 22309,
    22188, 22068, 21949, 21831, 21713, 21595, 21479, 21363, 21247, 21133, 21019, 20905,
    20792, 20680, 20568, 20457, 20347, 20237, 20127, 20019, 19911, 19803, 19696, 19590,
    19484, 19379, 19274, 19170, 19066, 18963, 18861, 18759, 18658, 18557, 18457, 18357,
    18258, 18160, 18061, 17964, 17867, 17770, 17674, 17579, 17484, 17390, 17296, 17202,
    17109, 17017, 16925, 16834, 16743, 16652, 16562, 16473, 16384,
};
/* the time of a setting as the detector's coefficient (0.2.24: ATTACK and RELEASE are fine, 40 steps a decade (core.h CMP_ATK_US / CMP_REL_MS), so the coefficient is computed
 * when the setting changes, not looked up): k = 1 - exp(-1 / tau), tau the time in hops of 8 samples (the release less the hold's 544 samples: the printed release is the time
 * to cover 63 % of a fall, the hold included), as Q24 (pk += d x k >> 24, the same product as the old m >> sh). tau >= 4 hops: the series 1/tau - 1/(2 tau^2) + 1/(6 tau^3)
 * (under 0.1 % off); shorter (attacks under 0.73 ms): 2^(-x log2 e) from CMP_EX with a straight line between its entries. tests/comp_test.c measures the times that come out */
static uint32_t cmp_coef(uint32_t samples)
{
    uint64_t tau32 = (uint64_t)(samples < 1u ? 1u : samples) * 4u;   /* tau in hops, Q5 (samples / 8 x 32) */
    uint64_t inv = ((uint64_t)1 << 29) / tau32;         /* 1 / tau, Q24 */
    if (tau32 >= 4u * 32u) {
        uint64_t i2 = (inv * inv) >> 24, i3 = (i2 * inv) >> 24;
        return (uint32_t)(inv - i2 / 2u + i3 / 6u);
    } else {                                            /* exp(-x) = 2^(-x log2 e): y Q24 */
        uint64_t y = (inv * 24204406u) >> 24;           /* log2 e in Q24 = 24204406 */
        uint32_t n = (uint32_t)(y >> 24), i = (uint32_t)((y >> 17) & 127u), r = (uint32_t)(y & 0x1FFFFu);
        uint32_t e = (uint32_t)(((uint64_t)CMP_EX[i] * (0x20000u - r) + (uint64_t)CMP_EX[i + 1u] * r) >> 17);   /* Q15 */
        e = n < 15u ? e >> n : 0u;
        return (32768u - e) << 9;
    }
}
#define CMP_HOLD_SAMPLES 544u                           /* the hold's mean delay (a window of 8 blocks of 64 and the one running) */
static uint32_t cmp_atk_coef(uint32_t i) { return cmp_coef((uint32_t)(((uint64_t)CMP_ATK_US(i % CMP_ATK_N) * FS + 500000u) / 1000000u)); }
static uint32_t cmp_rel_coef(uint32_t i)
{
    uint32_t smp = (uint32_t)(((uint64_t)CMP_REL_MS(i % CMP_REL_N) * FS + 500u) / 1000u);
    return cmp_coef(smp > CMP_HOLD_SAMPLES + 8u ? smp - CMP_HOLD_SAMPLES : 8u);
}
static volatile uint32_t cmp_cfg;                       /* the setting (ui_input copies it every frame): threshold index 0..40 | ratio << 8 | attack << 16 | release << 24 */
static struct {
    int32_t pk;                                         /* the smoothed level, Q12 of the amplitude */
    int32_t sm, hr, ring[8];                            /* the peak of this block of 64 samples so far, of the last 8 whole ones, and each of those */
    int32_t g, st;                                      /* the gain now and its step a sample, Q16 */
    int32_t a;                                          /* the effect's share, Q15 (0: at rest, the stage is not run) */
    int32_t thr, slope, mk;                             /* the threshold (Q8 octaves), the slope (Q16) and the makeup (Q8 octaves) now: they glide to the setting's */
    uint32_t cfg;                                       /* the setting the targets below were made from (0: none yet) */
    int32_t tthr, tslope;                               /* the setting's threshold and slope */
    uint32_t am, as, rm, rs;                            /* the attack's and the release's coefficients m, sh */
    uint32_t hc;                                        /* hops counted: a block of 64 samples ends every 8th */
} cmp;
/* 256 x log2 of an amplitude >= 1: the leading one by clz, the next 7 bits into the table (+-0.03 dB) */
static inline int32_t cmp_lg(uint32_t a)
{
    uint32_t n = 31u - (uint32_t)__builtin_clz(a);
    uint32_t f = n >= 7u ? (a >> (n - 7u)) & 127u : (a << (7u - n)) & 127u;
    return (int32_t)(n << 8) + CMP_LG[f];
}
/* 65536 x 2^(-e / 256), e in Q8 octaves of reduction (negative: a boost, down to -510); a half step of interpolation */
static inline int32_t cmp_exp2(int32_t e)
{
    int32_t n = e >> 8, f = e & 255, i = f >> 1, y = CMP_EX[i];
    y -= ((y - (int32_t)CMP_EX[i + 1]) * (f & 1)) >> 1;
    return n >= 0 ? (n > 16 ? 0 : (y << 1) >> n) : (y << 1) << -n;
}
/* the gain reduction (Q8 octaves) of a level `over` the threshold, the soft knee and the slope (Q16) */
static inline int32_t cmp_gr(int32_t over, int32_t slope)
{
    if (over <= -CMP_KNEE / 2)
        return 0;
    if (over >= CMP_KNEE / 2)
        return (over * slope) >> 16;
    over += CMP_KNEE / 2;
    return (((over * over) >> 9) * slope) >> 16;
}
/* the makeup of a threshold and a slope: the reduction at CMP_REF, at most CMP_MKMAX */
static inline int32_t cmp_mk(int32_t thr, int32_t slope)
{
    int32_t m = cmp_gr(CMP_REF - thr, slope);
    return m > CMP_MKMAX ? CMP_MKMAX : m;
}
/* the threshold index 0..40 = -40 .. 0 dB in Q8 octaves (10886 / 256 = 42.52 per dB; 4096 = 0 dB: a pre-MASTER amplitude of 65536, the output's full scale at the default MASTER) */
static inline int32_t cmp_thr_q8(uint32_t ti) { return 4096 - (int32_t)((40u - (ti > 40u ? 40u : ti)) * 10886u >> 8); }
static __attribute__((noinline)) void cmp_big(int32_t *l, int32_t *r)   /* (apart, so the loop keeps a branch it never takes) */
{
    *l = clamp(*l, -524287, 524287);
    *r = clamp(*r, -524287, 524287);
}
/* PERFORM's master stage (perf_master) has put MASTER's gain into the mix already (mg / 1024: x 2 at the default knob, x 4 at the top), so then the stage sees the level the idle path
 * would, times that: poff is the correction in Q8 octaves, log2(1024 / mg) x 256 (0 when PERFORM is idle: the mix is pre-MASTER). Without it THRESH would move with the MASTER knob and
 * with PERFORM. mg is the Q12 level mix_block uses (the reference MASTER_USB_REF) */
static inline int32_t cmp_poff(int32_t mg) { return 2560 - cmp_lg((uint32_t)(mg > 1 ? mg : 1)); }
static __attribute__((noinline)) void comp_stage(uint32_t n, int32_t poff)
{
    uint32_t cfg = cmp_cfg, rat = (cfg >> 8) & 15u, i, hop, j, hc;
    int32_t at = 0, a = cmp.a, pk = cmp.pk, g = cmp.g, st, sm = cmp.sm, hr = cmp.hr, thr, slope, mk, tthr, tslope;
    uint32_t am, as, rm, rs;
    if (rat - 1u < CMP_RATIOS - 1u) {                    /* ON: the setting's targets (OFF: the ones now, the share on its way out) */
        if (cfg != cmp.cfg) {
            cmp.cfg = cfg;
            cmp.tthr = cmp_thr_q8(cfg & 255u);
            cmp.tslope = (int32_t)CMP_SLOPE[rat];
            cmp.am = cmp_atk_coef((cfg >> 16) & 255u), cmp.as = 24u, cmp.rm = cmp_rel_coef((cfg >> 24) & 255u), cmp.rs = 24u;   /* (0.2.24: fine times, computed) */
        }
        at = 32768;
    }
    tthr = cmp.tthr, tslope = cmp.tslope;
    if (!a) {                                           /* from rest: the detector starts at this block's peak (no boost on the first sound), the gain at unity */
        int32_t m = 0;
        for (i = 0; i < n; i++) {
            int32_t x = mix_l[i] + wet[i], y = mix_r[i] + wet[i];
            x = x < 0 ? -x : x;
            y = y < 0 ? -y : y;
            m = x > m ? x : m;
            m = y > m ? y : m;
        }
        m = m > CMP_AMAX ? CMP_AMAX : m;
        cmp.thr = tthr, cmp.slope = tslope, cmp.mk = cmp_mk(tthr, tslope);
        pk = m << 12, sm = 0, hr = m, g = 65536;
        for (j = 0; j < 8u; j++)
            cmp.ring[j] = m;
    }
    thr = cmp.thr, slope = cmp.slope, mk = cmp.mk, hc = cmp.hc;
    am = cmp.am, as = cmp.as, rm = cmp.rm, rs = cmp.rs;
    for (hop = 0; hop < n; hop += 8u) {
        int32_t gt = cmp_exp2(cmp_gr(cmp_lg((uint32_t)(pk >> 12) + 1u) + poff - thr, slope) - mk), hm = 0, d;   /* the gain computer */
        a += clamp(at - a, -CMP_A_STEP, CMP_A_STEP);
        if (a != 32768)
            gt = 65536 + ((((gt - 65536) >> 3) * a) >> 12);
        st = (gt - g) >> 3;
        if (thr != tthr || slope != tslope) {           /* gliding to a new setting */
            thr += clamp(tthr - thr, -8, 8);
            slope += clamp(tslope - slope, -1024, 1024);
            mk = cmp_mk(thr, slope);
        }
        for (j = 0; j < 8u; j++) {
            int32_t w = wet[hop + j], l = mix_l[hop + j] + w, r = mix_r[hop + j] + w, x = l < 0 ? -l : l, y = r < 0 ? -r : r;   /* (the wet bus into the mix here: this stage is after the sends, MASTER after it) */
            x = x > y ? x : y;
            if (__builtin_expect(x > CMP_AMAX, 0)) {    /* (8 x full scale: the detector's ceiling; the audio's is space_width's, 16 x) */
                cmp_big(&l, &r);
                x = CMP_AMAX;
            }
            hm = x > hm ? x : hm;
            g += st;
            mix_l[hop + j] = ((l >> 2) * (g >> 4)) >> 10;
            mix_r[hop + j] = ((r >> 2) * (g >> 4)) >> 10;
            wet[hop + j] = 0;
        }
        sm = hm > sm ? hm : sm;                         /* the hold and the detector, once a hop */
        if (!(++hc & 7u)) {                             /* a block of 64 samples ends: its peak into the ring, the hold is the ring's */
            cmp.ring[(hc >> 3) & 7u] = sm;
            sm = 0;
            hr = cmp.ring[0];
            for (j = 1; j < 8u; j++)
                hr = cmp.ring[j] > hr ? cmp.ring[j] : hr;
        }
        d = ((sm > hr ? sm : hr) << 12) - pk;
        if (d > 0)
            pk += (int32_t)(((int64_t)d * am) >> as);
        else
            pk -= (int32_t)(((int64_t)(-d) * rm) >> rs);
    }
    if (!at && !a) {
        memset(&cmp, 0, sizeof cmp);                    /* faded out: at rest, the state dropped (the next ON starts from the peak) */
        return;
    }
    cmp.pk = pk, cmp.sm = sm, cmp.hr = hr, cmp.g = g, cmp.a = a, cmp.thr = thr, cmp.slope = slope, cmp.mk = mk, cmp.hc = hc;
}

/* DUELL step 5 (SPEC 5b.1): the mix's MODE. dm_cur follows duell_mode (core.h; seq.c mode_apply) and dm_split is this
 * block's: SPLIT mixes part 1 (trk[0]) to the left only and every other part to the right only, with
 *   - its PAN and SPREAD (and the matrix's and the morph's PAN) not heard: the dry part at full on its side, as a centred
 *     part has it on each side in STEREO (mix_part; mix_spread never runs: spread_on 0, the voices all into b);
 *   - its sends into its own bus (A: part 1, B: part 2; split_buses), each bus's return to its own side only, folded to
 *     mono (HALL's stereo difference left out: the return's mid, as a side of STEREO gets it);
 *   - the master without anything shared between the channels (master_out_split: the limiter per channel).
 * The DIST, the INSERT and the EQ are mono per part / per channel already.
 * A change between SPLIT and STEREO / LAYER: the output fades out (DM_STEP a sample, ~22 ms), the delay line is cleared
 * in DM_CHUNK pieces over the next blocks (silent: ~6 ms), then the chorus and reverb lines, the limiters, the
 * the master's lines (dm_lines_clear), the old notes' tails stop (silent: seq.c mode_apply let every note go at the request), the mix switches and
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
    memset(cho_buf, 0, sizeof cho_buf);
    memset(cho_buf_b, 0, sizeof cho_buf_b);
    rev_clear();
    rev_clear_b();
    fx.dly_lp = fx_b.dly_lp = 0;
    dlyev_clear();                                      /* (the delay's event modes from rest) */
    fx.lo_lp = fx.lo_k = fx.lo_g = fx_b.lo_lp = fx_b.lo_k = fx_b.lo_g = 0;   /* (the low cut from rest: no stale low-pass) */
    fx_b.rtype = fx.rtype;                              /* (B's lines are clear: no model fade to run) */
    fx_b.sp_size = fx.sp_size;
    hl_b.sz = 0;
    sw_a = 4096, sw_b = 0, sw_em = sw_es = 0, sw_g = 4096;   /* (SPACE's width from rest: DUEL has none) */
    lim_env = lim_env_r = LIM_T;                        /* the limiters from rest (they held the old mix's levels; at the silent point nothing to glide from) */
    memset(&bm, 0, sizeof bm);                          /* (BASS MONO from rest: it does not run in DUEL, the share fades in again) */
    memset(&cmp, 0, sizeof cmp);                        /* (the compressor too: from rest, its detector starts at the first block's peak) */
    memset(&wdn, 0, sizeof wdn);                        /* (0.2.23 WIDTH too: it does not run in DUEL, it starts from rest again) */
    perf_lines_clear();                                 /* PERFORM's lines and states (they hold the old mix: a part on the other side) */
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

/* the dip's ramp over the block (mix_l, mix_r and the returns, before the master) */
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

/* DUELL 0.2.14 MODULES (SENDS: snd_* above; PART FX: pfx_* below): the fades' step a sample, Q15: 993 samples, 22.5 ms (as DM_STEP) */
#define MF_STEP 33
/* STOP ALL's flush (0.2.14): the same machinery as SENDS OFF, asked once by seq.c stop_all: the returns fade out (22.5 ms), the delay line (dly_buf),
 * the chorus and both reverb buses are cleared (~6.5 ms), the sends fade back in over silent lines (22.5 ms); SPACE is not touched. The user's
 * SENDS OFF is not changed (snd_step ends the panic and goes on with the user's switch). A second press during it starts it again */
/* SENDS OFF: the lines to silence (snd_step, after the delay line's chunks). The buses' part of dm_lines_clear: the limiters, SPACE's
 * width and PERFORM's lines are not the sends' and keep their state */
static __attribute__((noinline)) void snd_lines_clear(void)
{
    memset(cho_buf, 0, sizeof cho_buf);
    memset(cho_buf_b, 0, sizeof cho_buf_b);
    rev_clear();
    rev_clear_b();
    fx.dly_lp = fx_b.dly_lp = 0;
    dlyev_clear();                                      /* (the delay's event modes from rest: voices, generator, clock, crossfade) */
    fx.lo_lp = fx.lo_k = fx.lo_g = fx_b.lo_lp = fx_b.lo_k = fx_b.lo_g = 0;   /* (SENDS ON again: the low cut from rest) */
    fx_b.rtype = fx.rtype;                              /* (B's lines are clear: no model fade to run) */
    fx_b.sp_size = fx.sp_size;
    hl_b.sz = 0;
}
/* once a block, before the parts, while SENDS is OFF or not at rest (mix_block): ON -> FADE when OFF is asked; FADE -> OFF at its end (or
 * back to ON, faded in again); OFF: the lines cleared, a DM_CHUNK of the delay line a block, then the rest (9 blocks, ~6.5 ms); ON asked
 * after that: FADE from silent lines (the chorus's and the delay's first return glides in, no step) */
static __attribute__((noinline)) void snd_step(void)
{
    uint32_t off = snd_off();
    if (snd_st == SND_ON) {
        if (off) {
            snd_st = SND_FADE;
            snd_x = 0;
        }
        return;
    }
    if (snd_st == SND_FADE) {
        if (!off && !snd_x)
            snd_st = SND_ON;                            /* (faded back in) */
        else if (off && snd_x >= 32768) {
            snd_st = SND_OFF;                           /* (faded out: from now on skipped) */
            snd_clr = 0;
        }
        return;
    }
    if (snd_clr <= DLY_LEN / DM_CHUNK) {                /* OFF: the lines to silence first (and stays OFF until they are) */
        if (snd_clr < DLY_LEN / DM_CHUNK)
            memset(dly_buf + (uint32_t)snd_clr * DM_CHUNK, 0, DM_CHUNK * sizeof dly_buf[0]);
        else
            snd_lines_clear();
        snd_clr++;
        return;
    }
    snd_panic = 0;                                      /* (STOP ALL's flush is done: the user's own state from here, OFF stays OFF) */
    off = (mods_off & MODS_SENDS) != 0u;
    if (!off) {
        snd_st = SND_FADE;                              /* ON again: the buses from silence, their return faded in */
        snd_x = 32768;
    }
}
/* SND_FADE, after the buses: the mix = the dry mix (snd_dl, snd_dr) + the buses' contribution (all they added to mix_l / mix_r and wet) x the
 * return's share, a sample at a time toward OFF (none) or ON (all); wet folded in (mix_block adds it to both channels) */
static __attribute__((noinline)) void snd_fade(uint32_t n)
{
    uint32_t i;
    int32_t x = snd_x, st = snd_off() ? MF_STEP : -MF_STEP;
    for (i = 0; i < n; i++) {
        uint32_t g;
        x = clamp(x + st, 0, 32768);
        g = (uint32_t)(32768 - x) << 1;                 /* (Q16: 65536 the whole return, exactly) */
        mix_l[i] = snd_dl[i] + mulq16(mix_l[i] + wet[i] - snd_dl[i], g);
        mix_r[i] = snd_dr[i] + mulq16(mix_r[i] + wet[i] - snd_dr[i], g);
        wet[i] = 0;
    }
    snd_x = x;
}

/* SPLIT: bus A on part 1's sends, its return (the mid) to the left; bus B on part 2's, to the right; wet stays 0 */
static __attribute__((noinline)) void split_buses(uint32_t n)
{
    uint32_t i, dl = delay_split();
    uint32_t dm = (uint32_t)song.g[G_DMODE];            /* (0.2.22: the delay's mode in DUEL, all mono per part: DIGI and PING the plain delay (a ping-pong needs two sides), RAND and OCT+ the events, centred) */
    uint32_t evm = dm == DMD_RAND || dm == DMD_OCTUP, ev = evm && !OVL_LITE;   /* (0.2.25: the overload guard, as in fx_buses) */
    uint32_t emode = ev ? dm - DMD_RAND + 1u : 0u;
    cho_zrun(sa_c, n);
    if (ev || dlyev.x) {                                /* an event mode, or the crossfade back to the plain delay still running */
        fx_buses_ev(dl, dl, sa_c, sa_d, sa_r, wet, n, emode, 0u, 0, DLY_HALF);
    } else if (!evm && (song.g[G_DMDEP] > 0 || fx.dm_dev))
        fx_buses_mod(DLY_HALF - 1u, dl, sa_c, sa_d, sa_r, wet, n, 0u, 0);
    else
        fx_buses_m(DLY_HALF - 1u, dl, sa_c, sa_d, sa_r, wet, n, 0u, 0);
    fx.cho_zs = 0;
    for (i = 0; i < n; i++)
        mix_l[i] += wet[i];
    if (mod_gon_any()) {                                /* (0.2.7: the matrix's bus values: bus B takes part 2's, mod.c mod_bus) */
        mod_bus(~1u);
        if (sv.on)
            space_vals();                               /* (SPACE's values from bus B's own) */
    }
    cho_zrun_b(send_c, n);
    if (ev || dlyev_b.x)
        fx_buses_ev_b(dl, dl, send_c, send_d, send_r, wet, n, emode, 0u, 0, DLY_HALF);
    else if (!evm && (song.g[G_DMDEP] > 0 || fx_b.dm_dev))
        fx_buses_mod_b(DLY_HALF - 1u, dl, send_c, send_d, send_r, wet, n, 0u, 0);
    else
        fx_buses_m_b(DLY_HALF - 1u, dl, send_c, send_d, send_r, wet, n, 0u, 0);
    fx_b.cho_zs = 0;
    for (i = 0; i < n; i++) {
        mix_r[i] += wet[i];
        wet[i] = 0;
    }
}

/* the end of a SPLIT block: the master level, then master_out_split */
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
 * ones (the part's pscr sbuf); here d = left - right, the side signal, so for a pair of pans L = b (gla + glb) / 2 + d (gla - glb)
 * / 2 (R alike): the mono sum is b's, the level of a centre pan less by SPRD / 256 at most. b goes through DIST as always; d passes DIST by (the spread of a distorted chord is its voices' clean difference). The sends
 * take b, mono, as before. SPRD 0, MONO or LEGATO: mix_part's own loop, bit for bit as before. The cost: a block's
 * clear and sum of sbuf, the side and a wider mix loop, per part; per voice none (it picks its buffer) */
static void duck_run(track_t *t, int32_t *b, int32_t *d, uint32_t n);
/* SPACE to the right (sp_tap; 0.2.9, the owner: SPACE adds space also when the sends are 0): each send of the part at least its floor. The
 * effective send is a soft max of the part's own (x SPACE's gain) and the floor (sv.ac / ad / ar, SV_ADD_*: SHIM's reverb SV_SHIM_ADD of it,
 * RESO's SV_RESO_ADD): the floor adds max(0, floor - send) with a quadratic knee of SV_KNEE either side (no kink); a send above floor + knee
 * gets nothing added (SPACE's x 1.6 as before), a send of 0 gets the floor. Its dry signal at its level x that, into the part's accumulators fc / fd / fr (pacc). The
 * values are the last block's (SPACE glides 0.25 % a block: a step under -50 dB), as a send's knob */
static int32_t sp_knee(int32_t dd)
{
    return dd <= -SV_KNEE ? 0 : dd >= SV_KNEE ? dd : (int32_t)(((int64_t)(dd + SV_KNEE) * (dd + SV_KNEE)) / (4 * SV_KNEE));
}
static __attribute__((noinline)) void space_tap(const int32_t *b, int32_t lvl, int32_t c, int32_t d, int32_t r, uint32_t n, part_acc_t *ac, uint8_t *on)
{
    uint32_t i;
    int32_t g = sv.sg, rg = song.g[G_RTYPE] == 4 && g > 32768 ? 32768 : g, fr = sv.ar, kc, kd, kr;
    if (song.g[G_RTYPE] == 4 || song.g[G_RTYPE] == 3)   /* (RESO, SHIM: a part of the floor, their combs off their limit) */
        fr = (fr * (song.g[G_RTYPE] == 4 ? SV_RESO_ADD : SV_SHIM_ADD)) >> 8;
    kc = sp_knee(sv.ac - (int32_t)(((int64_t)c * g) >> 15));
    kd = sp_knee(sv.ad - (int32_t)(((int64_t)d * g) >> 15));
    kr = sp_knee(fr - (int32_t)(((int64_t)r * rg) >> 15));
    if (!(kc | kd | kr))
        return;
    *on |= 16u;                                         /* (the floors are written: bit 4; with nothing to add they are not, and mix_gather reads none) */
    for (i = 0; i < n; i++) {
        int32_t x = clamp(((b[i] >> 2) * lvl) >> 10, -109000, 109000);   /* (as the sends take it; x k fits 32 bits) */
        ac->fc[i] = (x * kc) >> 15;
        ac->fd[i] = (x * kd) >> 15;
        ac->fr[i] = (x * kr) >> 15;
    }
}
/* The part's gain, Q12 (DUELL 0.2.19, owner 2026-10-15): the sound's PATCH TRIM times the MIXER LEVEL, as a real synth does: both are the same
 * 0.5 dB steps (LEVEL_Q12, 112 = 0 dB = 4096), so the product is their dB sum. A mixer level at unity (112) leaves the trim exactly (a * 4096 >> 12 = a): a part plays
 * at its trim as the one old LEVEL played. The product is held to the top of the table (+7.5 dB: LEVEL_Q12[127]), the headroom the stages after it were made for
 * (a loud chord x gain stays inside 32 bits); it binds only where the trim and the mixer together are above +7.5 dB. 0 in either = silent. One call a block, out of
 * line (the hot mix functions keep their size) */
static inline int32_t lvl_of(int32_t trim, int32_t level)
{
    int32_t g = ((int32_t)LEVEL_Q12[trim & 127] * (int32_t)LEVEL_Q12[level & 127]) >> 12;
    return g > (int32_t)LEVEL_Q12[127] ? (int32_t)LEVEL_Q12[127] : g;
}
static __attribute__((noinline)) int32_t part_lvl(const track_t *t) { return lvl_of(t->p[P_TRIM], t->p[P_LEVEL]); }
/* BALANCE (core.h mix_bal, the MODE page): part 1's level 1 for BALANCE <= 0, else 1 - bal / 100; part 2's 1 for >= 0, else
 * 1 + bal / 100 (linear; turning right fades part 1 out). 0: lvl untouched. In every mode, at the part's level stage */
static int32_t bal_lvl(const track_t *t, int32_t lvl, int32_t b)
{
    if (t == trk) {
        if (b > 0)
            lvl = lvl * (100 - b) / 100;
    } else if (b < 0) {
        lvl = lvl * (100 + b) / 100;
    }
    return lvl;
}
/* DUELL 0.2.14 GLO > MODULES, PART FX (core.h MODS_PFX): each part's DIST, INSERT, EQ and DUCK (the DUCK's key follower too) skipped while
 * OFF: the part's block passes as it came out of the voices (PERFORM's MUTE still applies: it is no part effect), bit for bit. A change
 * is a crossfade of 993 samples (22.5 ms) between the processed and the dry block (pfx_x: the dry share, Q15; 0 = all processed, as
 * ever; zero at power-on). Switching ON again starts the four from rest (pfx_restart: no stale filter or DIST state, no duck in the middle
 * of a dip; a SRC PUMP keeps its place in the beat). A silent part takes the new state at once (nothing to fade). The stored values of
 * the four are never touched (the morph, the matrix, CCs still change them: they act again when ON) */
static int32_t pfx_x[NLIVE];
enum { PFX_RUN, PFX_FADE, PFX_SKIP };
static __attribute__((noinline)) void pfx_restart(track_t *t)
{
    duck_t *k = &duck_st[(uint32_t)(t - trk) % NTRK];
    t->dist_hp = DIST_OFF;                              /* (DIST starts from its first sample when it runs: the blend's switch-on init, fxdiag) */
    t->dist_lp1 = t->dist_lp2 = 0;
    ins_rest(t);
    eq_rest(t);
    k->dn = k->key = k->tg = 0;                         /* (the DUCK at rest; a PUMP's run / pos stay: it ducks on the beat) */
    k->cnt = 0;
    k->ph = k->on = 0;
}
/* the part's PART FX this block: PFX_RUN (as ever), PFX_FADE (the four run and pfx_mix crossfades) or PFX_SKIP (none run) */
static uint32_t pfx_mode(track_t *t, const blockstate_t *bs)
{
    uint32_t p = (uint32_t)(t - trk) % NLIVE;
    int32_t want = (bs->mods_off & MODS_PFX) ? 32768 : 0;
    if (pfx_x[p] == want)
        return want ? PFX_SKIP : PFX_RUN;
    if (!want && pfx_x[p] == 32768)
        pfx_restart(t);                                 /* (ON again: from rest) */
    return PFX_FADE;
}
/* a silent part (mix_part's early way out): the state asked for at once; OFF: the DUCK at rest too (the matrix's DUCK source reads 0) */
static __attribute__((noinline)) void pfx_snap(track_t *t, const blockstate_t *bs)
{
    uint32_t p = (uint32_t)(t - trk) % NLIVE;
    int32_t want = (bs->mods_off & MODS_PFX) ? 32768 : 0;
    if (pfx_x[p] == want)
        return;
    if (want) {
        duck_t *k = &duck_st[(uint32_t)(t - trk) % NTRK];
        k->dn = k->tg = 0, k->ph = k->on = 0;
    } else if (pfx_x[p] == 32768)
        pfx_restart(t);
    pfx_x[p] = want;
}
/* PFX_FADE: b (and SPREAD's side d) = the processed block toward the dry one (the part's pscr dry, dryd), a sample at a time; at the end of a fade
 * out the DUCK at rest (as pfx_snap) */
static __attribute__((noinline)) void pfx_mix(track_t *t, int32_t *b, int32_t *d, uint32_t n, const blockstate_t *bs)
{
    uint32_t p = (uint32_t)(t - trk) % NLIVE, i;
    int32_t x = pfx_x[p], st = (bs->mods_off & MODS_PFX) ? MF_STEP : -MF_STEP;
    const int32_t *dry = pscr[p].dry, *dryd = pscr[p].dryd;
    for (i = 0; i < n; i++) {
        uint32_t g;
        x = clamp(x + st, 0, 32768);
        g = (uint32_t)x << 1;                           /* (Q16: the dry share) */
        b[i] += mulq16(dry[i] - b[i], g);
        if (d)
            d[i] += mulq16(dryd[i] - d[i], g);
    }
    pfx_x[p] = x;
    if (x == 32768) {
        duck_t *k = &duck_st[(uint32_t)(t - trk) % NTRK];
        k->dn = k->tg = 0, k->ph = k->on = 0;
    }
}
/* PERFORM's MUTE during a PART FX fade: the same ramp on the processed block and on the dry one (its gain run twice from the same start) */
static __attribute__((noinline)) void pfx_mute(uint32_t k, int32_t *b, int32_t *d, uint32_t n, int32_t tg)
{
    int32_t g0 = pf.mg[k];
    if (d) {
        perf_mute_sd(k, b, d, n, tg);
        pf.mg[k] = g0;
        perf_mute_sd(k, pscr[k].dry, pscr[k].dryd, n, tg);
    } else {
        perf_mute(k, b, n, tg);
        pf.mg[k] = g0;
        perf_mute(k, pscr[k].dry, n, tg);
    }
}
/* PAN without a zipper (DUELL 0.2.24, owner: a PAN turned from the editor or the knob crackled). The part's STORED pan is followed in Q8 (pan_x, pan x 256) a block at a
 * time: 5/16 of what is left each block, the rest snapped under 1/16 of a step (a step of the knob is smooth within ~13 ms, a jump across the whole range within ~30 ms);
 * a silent part takes the new pan at once (nothing to click). The matrix's offset on PAN (mod_begin writes the modulated value for the block) is added on top as it was:
 * a modulated pan steps per block exactly as before, so every render of before is bit for bit the same while the stored pan does not move. In the plain (no SPREAD) mix
 * the gains of a moving block go in a straight line from the block before's gains (pan_g) to this block's; mix_spread (hot) takes the Q8 pan and steps once a block
 * (its gains are the Q8 pan >> 2: the same numbers as pan x 64). pan_follow is not hot code */
static int32_t pan_x[NLIVE];                            /* the stored PAN as the mix has it, Q8 */
static int32_t pan_g[NLIVE][2];                         /* the plain mix's gains the last block ended at (gl, gr; Q12) */
static uint8_t pan_gv[NLIVE];                           /* .. valid (the block before was a plain sounding block) */
static __attribute__((noinline)) int32_t pan_follow(uint32_t p, int32_t stored, int silent, int fine)
{
    int32_t tg = stored << 8, d = tg - pan_x[p];
    if (silent || (d > -16 && d < 16)) {
        pan_x[p] = tg;
        return 0;
    }
    pan_x[p] += fine ? d >> 3 : (d * 5) >> 4;           /* (fine: the SPREAD mix, whose gains step once a block (hot code, no ramp): 1/8 of what is left, ~30 ms for a knob step) */
    return 1;
}
/* SAT (the part's DIST drive) without a zipper (DUELL 0.2.25, owner: a SAT changed on an FM part crackled: track_dist read the drive once a block and its gain, tone, make-up
 * and wet blend stepped at the block boundary). The part's SAT (the stored value plus the matrix's offset, clamped: what mod_begin leaves in t->p) is followed in Q8 (dist_x, SAT x 256) a block at a time, 1/4 of what is left but at most 1.5 steps (snapped under 1/16 of a
 * step: a knob step lands within ~10 ms, the whole range within ~140 ms), and track_dist (hot) gets the followed value as an integer in t->p[P_DIST] for the block (put back after
 * it). A jump of the modulated SAT (an LFO restarted by a note, a fast LFO, an envelope) ramps the same way; a SAT that does not move is bit for bit as before. A part that was
 * silent takes the new SAT at once (dist_live: nothing to click, the first sounding block is at the SAT). dist_follow is not hot code */
#define DIST_SLEW 384                                   /* at most 1.5 steps of SAT a block (1 step of the blend below 12 is 8 % of the wet / dry difference) */
static int32_t dist_x[NLIVE];                           /* the stored SAT as track_dist has it, Q8 */
static uint8_t dist_live[NLIVE];                        /* the block before was a sounding one */
static __attribute__((noinline)) int32_t dist_follow(uint32_t p, int32_t now, int silent)
{
    int32_t tg = now << 8, d = tg - dist_x[p];
    if (silent || !dist_live[p] || (d > -16 && d < 16))
        dist_x[p] = tg;
    else
        dist_x[p] += (d >> 2) > DIST_SLEW ? DIST_SLEW : ((d >> 2) < -DIST_SLEW ? -DIST_SLEW : d >> 2);
    dist_live[p] = (uint8_t)!silent;
    d = (dist_x[p] + 128) >> 8;
    return d < 0 ? 0 : (d > 127 ? 127 : d);
}
/* SAT's blend zone without a step (DUELL 0.2.25, the matrix round; release_drafts/MATRIX_DESTINATIONS.md "SAT"): below SAT 12 track_dist blends dry and wet by
 * SAT / 12, and the follower's 1.5 steps a block are 12 % of the wet / dry difference at a block boundary (a modulated or turned SAT ticked there). While the
 * followed SAT (dist_x, Q8) moves and either end of the block is in that zone, this copy of track_dist's loop takes the blend in a straight line from the
 * block before's dist_x (w0) to this block's (w1) inside the block; the drive, tone and make-up are the integer SAT's, as in track_dist. At rest, and above
 * the zone, track_dist (hot) runs as ever: bit for bit as before. Flash code */
static __attribute__((noinline)) void track_dist_ramp(track_t *t, int32_t *b, uint32_t n, int32_t w0, int32_t w1)
{
    int32_t d = t->p[P_DIST], i, g, k, mk, bias = 2400, b0, q0, q1, acc = 0, st;
    if (!d && (w0 > 0 || w1 > 0))
        d = 1;                                           /* (hunt1: the followed SAT is between 0 and the integer 1, which it reaches at 128 / 256: the wet runs at SAT 1's drive from the first block and the blend goes
                                                          * from / to 0 in the line below. With the integer 0 as the bypass, the wet came in at the first block with the integer 1 from a cold start at a blend of 4 %:
                                                          * a click out of SAT 0 (PHASE #31, FM6 #83, WAVE #61 in the bench), and a step into 0 on the way back. CLICK_HUNT1.md) */
    if (!d) {
        t->dist_hp = DIST_OFF;                           /* (as track_dist: the bypass) */
        return;
    }
    if (!n)
        return;
    if (t->dist_hp == DIST_OFF) {
        t->dist_hp = b[0];
        t->dist_lp1 = t->dist_lp2 = 0;
    }
    q0 = w0 >= 12 * 256 ? 32768 : (w0 < 0 ? 0 : w0 * 32 / 3);   /* the blend in Q15: SAT / 12 (w Q8: w * 32768 / 3072) */
    q1 = w1 >= 12 * 256 ? 32768 : (w1 < 0 ? 0 : w1 * 32 / 3);
    st = ((q1 - q0) << 10) / (int32_t)n;
    g = 4096 + d * d * 2;
    k = 32000 - d * 95;
    mk = 30000 - d * 120;
    b0 = softclip(bias);
    for (i = 0; i < (int32_t)n; i++) {
        int32_t x = b[i], dry = x, y, q;
        t->dist_hp += (x - t->dist_hp + 64) >> 7;
        x = clamp(x - t->dist_hp, -230000, 230000);
        y = softclip((((x >> 2) * g) >> 10) + bias) - b0;
        t->dist_lp1 += mulq15(y - t->dist_lp1, k);
        t->dist_lp2 += mulq15(t->dist_lp1 - t->dist_lp2, k);
        y = mulq15(t->dist_lp2, mk);
        acc += st;
        q = i == (int32_t)n - 1 ? q1 : q0 + (acc >> 10);      /* (the last sample on w1 exactly) */
        b[i] = q >= 32768 ? y : dry + (int32_t)(((int64_t)(y - dry) * q) >> 15);
    }
}

/* the one part's block context, built by part_voice and handed on by pointer. Nothing here is state across blocks (that lives in track_t
 * and in the per-part arrays indexed by pp: pan_x / pan_g / pan_gv, dist_x / dist_live, pfx_x, duck_st, ins[], the EQ states, pf.mg).
 *   t, n, pp      the part (trk[]), the block's samples, its index (0..NLIVE-1)
 *   b             the part's block: pscr[pp].buf (core.h), mono, Q15 x voices (written by part_voice, in place through part_fx, read by part_mix)
 *   acc           the part's own accumulators (pacc[pp]): part_mix and mix_spread add into these, mix_gather sums the parts' later
 *   scr           the part's scratch (pscr[pp])
 *   bs            the block's shared control state, read once (blockstate_t above): the stages only read it
 *   pan_st        the STORED pan before the matrix writes the block's (pan_follow);  dist_st  the stored SAT likewise (dist_follow)
 *   pan_mv        the pan is on its way (part_fx -> part_mix: the gains ramp)  pq  the pan this block mixes at, Q8
 *   fxm           PART FX mode (PFX_RUN / PFX_FADE / PFX_SKIP)  spread  SPREAD runs for this part (pscr[pp].spread_on) */
typedef struct {
    track_t *t;
    int32_t *b;
    part_acc_t *acc;
    part_scr_t *scr;
    const blockstate_t *bs;
    uint32_t n, pp, fxm;
    int32_t pan_st, dist_st, pan_mv, pq, dist_rw0;
    uint8_t spread;
} mixpart_t;

/* the part's accumulators, before its mix loop (part_fx for SPREAD, part_mix): marks them written (pacc_on bit 0) and which sends the loop will assign (bits 1..3: a
 * send at 0, or all of them with SENDS OFF, is not assigned and not read by mix_gather); space_tap sets bit 4 when it assigns the floors.
 * Out of line, once a block, so the loops (mix_spread is hot) do not carry it */
static __attribute__((noinline)) void acc_prep(const mixpart_t *m)
{
    const track_t *t = m->t;
    uint32_t sn = m->bs->snd_st != SND_OFF;
    pacc_on[m->pp] = (uint8_t)(1u | (sn && t->p[P_CHOR] ? 2u : 0u) | (sn && t->p[P_DLY] ? 4u : 0u) | (sn && t->p[P_REV] ? 8u : 0u));
}

/* PAN, PATCH LEVEL and the sends moved by the MATRIX without a step (DUELL 0.2.25, the matrix round; the study's PAN 613 -> 19, PATCH LEVEL 575 -> 17, DLY send
 * 385 -> 11 thousandths of the peak): mod_begin writes the modulated values once a block, so the plain mix's gains (left / right, the level, the three sends)
 * stepped at the block boundary, a click on a jump and a 1.4 kHz zipper under an LFO. While a matrix route moves one of them (mod.c mod_mixmv[pp]), and one block
 * after it stops (the step back to the stored value), the plain mix takes all six in a straight line from the values the block before ended at to this
 * block's. A part with no such route mixes as ever (bit for bit); the stored pan's own follower (pan_follow) is unchanged. Not the SPREAD mix (mix_spread is
 * hot code: its gains still step a block, as before). 1 .. 15 host instructions a sample while the values move; flash code */
static int32_t mx_lv[NLIVE] MXR_BSS, mx_sd[NLIVE][3] MXR_BSS;   /* the level and the sends the plain mix's last block ended at (mod.c MXR_BSS: a section of their own) */
static uint8_t mx_ok[NLIVE] MXR_BSS;                    /* .. valid (the block before was a plain or SPREAD sounding block) */
#define SPG_N 9
static int32_t sp_cur[NLIVE][SPG_N] MXR_BSS;            /* hunt1: SPREAD's mix of this block, as spread_run notes it for spread_post: P_TRIM, P_LEVEL, the CHORUS / DELAY / REVERB sends (0..127), P_SPRD, the Q8 pan, BALANCE, SENDS OFF (1) */
static int32_t sp_prv[NLIVE][SPG_N] MXR_BSS;            /* .. and the SPREAD block before's */
static uint8_t sp_go[NLIVE] MXR_BSS;                    /* .. 1: this block was a SPREAD one (bit 1: a route or the MORPH moves the gains); the plain mix and a silent part write 0 */
static uint8_t sp_ok[NLIVE] MXR_BSS;                    /* .. sp_prv holds the block before's (the block before was a SPREAD one) */
static uint8_t mx_was[NLIVE] MXR_BSS;                   /* per part (0 / 1): mod_mixmv[pp] of the block before (the ramp back after a route stops). A byte each, not bits of one byte: join0233,
                                                         * the two parts' stages may run on two cores */
/* the ramped plain mix (join0233: A's mix_ramp on the two-core line's per-part accumulators): the part's block m->b into m->acc (assigned, not added: part_mix's
 * way), the sends whose pacc_on bit acc_prep set (a send that ramps down to 0 from a value has its bit set by part_mix before the call) */
static __attribute__((noinline)) int32_t mix_ramp(const mixpart_t *m, int32_t pk, int32_t gl, int32_t gr, int32_t lvl, int32_t c, int32_t d, int32_t r)
{
    const int32_t *b = m->b;
    part_acc_t *ac = m->acc;
    uint32_t n = m->n, pp = m->pp;
    uint32_t on = pacc_on[pp];
    int32_t gl0 = pan_gv[pp] ? pan_g[pp][0] : gl, gr0 = pan_gv[pp] ? pan_g[pp][1] : gr, ni = (int32_t)n, i, xmax = 1;
    int32_t glq = gl0 << 16, grq = gr0 << 16, dgl = ((gl - gl0) << 16) / ni, dgr = ((gr - gr0) << 16) / ni;
    int32_t lq = mx_lv[pp] << 8, dl = ((lvl - mx_lv[pp]) << 8) / ni;
    int32_t cq = mx_sd[pp][0] << 8, dq = mx_sd[pp][1] << 8, rq = mx_sd[pp][2] << 8;
    int32_t dc = ((c - mx_sd[pp][0]) << 8) / ni, dd = ((d - mx_sd[pp][1]) << 8) / ni, dr = ((r - mx_sd[pp][2]) << 8) / ni;
    for (i = 0; i < 3; i++)                             /* (sends: the largest of either end, as the plain loop's guard) */
        xmax = mx_sd[pp][i] > xmax ? mx_sd[pp][i] : xmax;
    xmax = c > xmax ? c : xmax;
    xmax = d > xmax ? d : xmax;
    xmax = r > xmax ? r : xmax;
    xmax = 0x7FFFFFFF / (xmax | 1);
    for (i = 0; i < ni; i++) {
        int32_t x, a, xs, l, sc, sd, sr, gli, gri;
        lq += dl; cq += dc; dq += dd; rq += dr; glq += dgl; grq += dgr;
        if (i == ni - 1) {                              /* (the last sample on this block's values exactly) */
            l = lvl, sc = c, sd = d, sr = r, gli = gl, gri = gr;
        } else {
            l = lq >> 8, sc = cq >> 8, sd = dq >> 8, sr = rq >> 8, gli = glq >> 16, gri = grq >> 16;
        }
        x = ((b[i] >> 2) * l) >> 10;
        a = x < 0 ? -x : x;
        xs = clamp(x, -xmax, xmax);
        if (a > pk)
            pk = a;
        if (on & 2u)
            ac->sc[i] = mulq15(xs, sc);
        if (on & 4u)
            ac->sd[i] = mulq15(xs, sd);
        if (on & 8u)
            ac->sr[i] = mulq15(xs, sr);
        ac->l[i] = (x * gli) >> 12;
        ac->r[i] = (x * gri) >> 12;
    }
    return pk;
}

/* SPREAD's part (the mix too): the part t's block b (n samples), fxm the PART FX mode, pan the Q8 pan this block mixes at; m: the part's stage context
 * (its accumulators, the block's snapshot, its index) */
static __attribute__((noinline)) void mix_spread(track_t *t, int32_t *b, uint32_t n, uint32_t fxm, int32_t pan, const mixpart_t *m)
{
    part_acc_t *ac = m->acc;
    const blockstate_t *bs = m->bs;
    part_scr_t *sc = m->scr;
    int32_t *d = sc->sbuf;
    uint32_t i;
    int32_t lvl = bal_lvl(t, part_lvl(t), bs->mix_bal), s = (t->p[P_SPRD] + 1) >> 1 << 8;   /* (pan, s: Q8) */
    int32_t pa = clamp(pan - s, -64 * 256, 63 * 256), pb = clamp(pan + s, -64 * 256, 63 * 256);
    int32_t gla = 4096 - (pa > 0 ? pa >> 2 : 0), gra = 4096 + (pa < 0 ? pa >> 2 : 0);
    int32_t glb = 4096 - (pb > 0 ? pb >> 2 : 0), grb = 4096 + (pb < 0 ? pb >> 2 : 0);
    int32_t glm = (gla + glb) >> 1, grm = (gra + grb) >> 1, gld = (gla - glb) >> 1, grd = (gra - grb) >> 1;
    int32_t c = t->p[P_CHOR] * 258, dl = t->p[P_DLY] * 258, r = t->p[P_REV] * 258, pk = t->peak;
    int32_t xmax;
    if (bs->snd_st == SND_OFF)
        c = dl = r = 0;                                 /* (0.2.14 MODULES SENDS OFF: nothing sent) */
    xmax = c > dl ? c : dl;
    xmax = 0x7FFFFFFF / ((xmax > r ? xmax : r) | 1);
    for (i = 0; i < n; i++)
        d[i] = (d[i] << 1) - b[i];                      /* left - right */
    if (fxm == PFX_FADE) {                              /* (0.2.14 MODULES PART FX: the dry block kept for the crossfade) */
        memcpy(sc->dry, b, n * sizeof b[0]);
        memcpy(sc->dryd, d, n * sizeof d[0]);
    }
    if (fxm != PFX_SKIP) {
        track_dist(t, b, n);
        if (t->p[P_ITYPE] | t->ins_run)
            track_insert(t, b, n);                      /* (the mono sum, as DIST) */
        if (EQ_ON(t)) {                                 /* the EQ: the sum and the side alike */
            track_eq(t, b, n, 0u);
            track_eq(t, d, n, 1u);
        }
    }
    if ((bs->pf_mute >> m->pp) & 1u) {                  /* PERFORM: MUTE 1 / 2 held (perform.c) */
        if (fxm == PFX_FADE)
            pfx_mute(m->pp, b, d, n, bs->mute_tg[m->pp]);
        else
            perf_mute_sd(m->pp, b, d, n, bs->mute_tg[m->pp]);
    }
    if (fxm != PFX_SKIP && duck_st[(uint32_t)(t - trk) % NTRK].ph)
        duck_run(t, b, d, n);                           /* the DUCK (DUELL): the sum and the side alike */
    if (fxm == PFX_FADE)
        pfx_mix(t, b, d, n, bs);
    for (i = 0; i < n; i++) {
        int32_t x = ((b[i] >> 2) * lvl) >> 10, a = x < 0 ? -x : x, y = ((d[i] >> 2) * lvl) >> 10;
        int32_t xs = clamp(x, -xmax, xmax);
        if (a > pk)
            pk = a;
        if (c)
            ac->sc[i] = mulq15(xs, c);
        if (dl)
            ac->sd[i] = mulq15(xs, dl);
        if (r)
            ac->sr[i] = mulq15(xs, r);
        ac->l[i] = ((x * glm) >> 12) + ((y * gld) >> 12);
        ac->r[i] = ((x * grm) >> 12) + ((y * grd) >> 12);
    }
    t->peak = pk;
    if (bs->sp_tap)
        space_tap(b, lvl, c, dl, r, n, ac, &pacc_on[m->pp]);
}

/* SPREAD's mix without a step (hunt1, release_drafts/CLICK_HUNT1.md). mix_spread (hot code) runs SAT at its integer and mixes a block at one set of gains: PATCH LEVEL, the
 * pan gains (mid and side) and the three sends step at the block boundary, and so does SAT's blend zone (SAT 0..12): a click on every jump (the MORPH fader's steps, an S&H
 * or a square LFO on the matrix), a 1.4 kHz zipper under an LFO. T0229 ramped them for the plain mix (mix_ramp, track_dist_ramp) and only for the matrix. Two pieces, neither touches
 * mix_spread (hot RAM keeps its 232 B):
 *   spread_run   (part 2's chain: range X, with mix_ramp) around mix_spread: SAT moving in its blend zone (the plain mix's test): track_dist_ramp runs on the mid before
 *                mix_spread, whose own track_dist is switched off for the call (SAT 0; dist_hp, which that resets, put back), and the side d, which mix_spread forms as 2 d - b
 *                from the mid after SAT, gets half of SAT's change of the mid in advance so that it is still 2 d - b of the mid before SAT (at most 1 LSB of the side off, only in a
 *                block where SAT moves in its blend zone with SPREAD on; not in a PART FX fade, where the dry block is copied inside mix_spread). Then it notes the block's level,
 *                SPREAD, pan, BALANCE for the post-pass.
 *   spread_post  (core 0, flash, from mix_gather, both parts' chains done) when a matrix route or the MORPH window moves one of the gains (mod_mixmv: mix_ramp's test) and the
 *                block before was a SPREAD block too: the part's block is mixed again from b and d (the mid and the side after every stage) with the gains going in a straight line
 *                from the block before's to this block's, and that replaces mix_spread's accumulators. At rest it compares nine words with the block before's and copies them: nothing else runs (bit for bit) */
static void spread_post(uint32_t pp, uint32_t n, const blockstate_t *bs);
static __attribute__((noinline)) void spread_run(mixpart_t *m, uint32_t fxm, int32_t pan)
{
    track_t *t = m->t;
    uint32_t pp = m->pp, n = m->n, i;
    int32_t *b = m->b, hp = 0, di = t->p[P_DIST];
    volatile int sat = fxm == PFX_RUN && m->dist_rw0 != dist_x[pp] && (m->dist_rw0 < 12 * 256 || dist_x[pp] < 12 * 256);   /* (volatile: the test below is not made into a second copy of the call) */
    if (sat) {
        int32_t *d = m->scr->sbuf, *dry = m->scr->dry;  /* (dry: unused outside a PART FX fade) */
        for (i = 0; i < n; i++)
            dry[i] = b[i];
        track_dist_ramp(t, b, n, m->dist_rw0, dist_x[pp]);
        for (i = 0; i < n; i++)
            d[i] += (b[i] - dry[i]) >> 1;
        hp = t->dist_hp;
        t->p[P_DIST] = 0;                               /* (mix_spread's track_dist: the bypass, which resets dist_hp) */
    }
    mix_spread(t, b, n, fxm, pan, m);
    if (sat) {
        t->p[P_DIST] = (int16_t)di;
        t->dist_hp = hp;
    }
    {
        int32_t *q = sp_cur[pp];
        q[0] = t->p[P_TRIM], q[1] = t->p[P_LEVEL], q[2] = t->p[P_CHOR], q[3] = t->p[P_DLY], q[4] = t->p[P_REV], q[5] = t->p[P_SPRD];
        q[6] = pan, q[7] = m->bs->mix_bal, q[8] = m->bs->snd_st == SND_OFF;
    }
    sp_go[pp] = (uint8_t)(1u | ((mod_mixmv[pp] | mx_was[pp]) & 1u) << 1);
    pan_gv[pp] = 0;
    mx_ok[pp] = 0;                                      /* (the plain mix starts again from its own block's values) */
}
/* the mix's gains of a noted block (flash, core 0): g[0] the level, g[1..3] the three sends (Q15 gains), g[4] g[5] the mid pan gains (left, right), g[6] g[7] the side gains; as mix_spread's */
static void spread_gains(uint32_t pp, const int32_t *q, int32_t *g)
{
    int32_t s = (q[5] + 1) >> 1 << 8, pa = clamp(q[6] - s, -64 * 256, 63 * 256), pb = clamp(q[6] + s, -64 * 256, 63 * 256);
    int32_t gla = 4096 - (pa > 0 ? pa >> 2 : 0), gra = 4096 + (pa < 0 ? pa >> 2 : 0);
    int32_t glb = 4096 - (pb > 0 ? pb >> 2 : 0), grb = 4096 + (pb < 0 ? pb >> 2 : 0);
    g[0] = bal_lvl(&trk[pp], lvl_of(q[0], q[1]), q[7]);
    g[1] = q[8] ? 0 : q[2] * 258, g[2] = q[8] ? 0 : q[3] * 258, g[3] = q[8] ? 0 : q[4] * 258;
    g[4] = (gla + glb) >> 1, g[5] = (gra + grb) >> 1, g[6] = (gla - glb) >> 1, g[7] = (gra - grb) >> 1;
}
/* core 0, flash (mix_gather): see the comment above. At rest (the notes equal the block before's, or no route and no morph window) this is a comparison of nine words and a copy */
static __attribute__((noinline)) void spread_post(uint32_t pp, uint32_t n, const blockstate_t *bs)
{
    int32_t *q = sp_cur[pp], *pq = sp_prv[pp];
    uint32_t go = sp_go[pp], k, mv = 0;
    (void)bs;
    if (!go) {
        sp_ok[pp] = 0;
        return;
    }
    sp_go[pp] = 0;
    for (k = 0; k < SPG_N; k++)
        mv |= (uint32_t)(q[k] != pq[k]);
    if (mv && sp_ok[pp] && (go & 2u)) {
        int32_t cur[8], prv[8];
        spread_gains(pp, q, cur);
        spread_gains(pp, pq, prv);
        for (k = 0, mv = 0; k < 8u; k++)
            mv |= (uint32_t)(cur[k] != prv[k]);
        if (mv) {
            part_acc_t *ac = &pacc[pp];
            const int32_t *b = pscr[pp].buf, *d = pscr[pp].sbuf;
            int32_t ni = (int32_t)n, xmax = 1, i, pm;
            int32_t lq = prv[0] << 8, dlq = ((cur[0] - prv[0]) << 8) / ni;   /* the level, the three sends (Q8), the four pan gains (Q16): each a line from the block before's to this block's */
            int32_t cq = prv[1] << 8, dcq = ((cur[1] - prv[1]) << 8) / ni, eq = prv[2] << 8, deq = ((cur[2] - prv[2]) << 8) / ni, rq = prv[3] << 8, drq = ((cur[3] - prv[3]) << 8) / ni;
            int32_t aq = prv[4] << 16, daq = ((cur[4] - prv[4]) << 16) / ni, bq = prv[5] << 16, dbq = ((cur[5] - prv[5]) << 16) / ni;
            int32_t gq = prv[6] << 16, dgq = ((cur[6] - prv[6]) << 16) / ni, hq = prv[7] << 16, dhq = ((cur[7] - prv[7]) << 16) / ni;
            uint32_t on = pacc_on[pp] | (prv[1] ? 2u : 0u) | (prv[2] ? 4u : 0u) | (prv[3] ? 8u : 0u);   /* (a send ramping down to 0 still has samples to write) */
            for (k = 1; k < 4u; k++) {
                xmax = prv[k] > xmax ? prv[k] : xmax;
                xmax = cur[k] > xmax ? cur[k] : xmax;
            }
            pacc_on[pp] = (uint8_t)on;
            xmax = 0x7FFFFFFF / (xmax | 1);
            pm = cur[4] != prv[4] || cur[5] != prv[5] || cur[6] != prv[6] || cur[7] != prv[7];   /* (the pan gains stand still unless the stored PAN, the matrix or SPREAD moves them: then four more lines) */
            for (i = 0; i < ni; i++) {
                int32_t x, y, l = cur[0], ga = cur[4], gb = cur[5], gc = cur[6], gd = cur[7];
                lq += dlq, cq += dcq, eq += deq, rq += drq;
                if (i != ni - 1) {                      /* (the last sample on this block's values exactly, as mix_ramp) */
                    l = lq >> 8;
                    if (pm) {
                        aq += daq, bq += dbq, gq += dgq, hq += dhq;
                        ga = aq >> 16, gb = bq >> 16, gc = gq >> 16, gd = hq >> 16;
                    }
                } else {
                    cq = cur[1] << 8, eq = cur[2] << 8, rq = cur[3] << 8;
                }
                x = ((b[i] >> 2) * l) >> 10;
                y = ((d[i] >> 2) * l) >> 10;
                if (on & 14u) {
                    int32_t xs = clamp(x, -xmax, xmax);
                    if (on & 2u)
                        ac->sc[i] = mulq15(xs, cq >> 8);
                    if (on & 4u)
                        ac->sd[i] = mulq15(xs, eq >> 8);
                    if (on & 8u)
                        ac->sr[i] = mulq15(xs, rq >> 8);
                }
                ac->l[i] = ((x * ga) >> 12) + ((y * gc) >> 12);
                ac->r[i] = ((x * gb) >> 12) + ((y * gd) >> 12);
            }
        }
    }
    for (k = 0; k < SPG_N; k++)
        pq[k] = q[k];
    sp_ok[pp] = 1;
}

/* DUELL DUCK (SPEC 5c.4): the part's sidechain; the trigger is ONE setting, SRC (P_DKSRC: 0 PUMP, 1 PART, 2 MIDI; two pages,
 * DUCK 1: SRC, the context knob RATE / THR of the SRC (params.c duck_ctx_id; MIDI has none), DEPTH, ATK; DUCK 2: RLS, CURVE EXP / LIN).
 * With SRC MIDI: a note-on on the part's SIDECHAIN channel (sc_midi, GLO > SIDECHAIN: a device setting, not part of the sound) whose note is the
 * part's SIDECHAIN NOTE (0 ANY, n the note n: 1..127) starts the envelope (duck_st, mod.c): the gain goes down by DEPTH % in ATK ms (a
 * ramp), then comes back to full in RLS ms: CURVE EXP (the default, a pump's release: a share of what is left of the reduction
 * every sample, fast at first and settling smoothly, 3 time constants in RLS) or LIN (a straight line in the gain, the earlier
 * curve). There is no HOLD (its legacy id is the PATCH TRIM since 0.2.19, core.h P_TRIM); a new note-on goes down again from where the gain is. Note-offs are
 * ignored. DEPTH 0 or an envelope at rest: nothing, no sample touched. The latency is under a block: a note-on is
 * read at the block's start (events_block), the envelope runs from its first sample. The gain multiplies the part's block
 * where its level does (mix_part / mix_spread: after INSERT, EQ, DIST and mute, before the level, pan and the sends)
 * and runs on while the part is silent. Which notes play: a note matching a part's trigger is a trigger and no more when
 * NOTE is one note, or when the channel plays no part (midi_event: `plays`); with NOTE ANY on a channel that plays a part
 * (its own, or part 1's in LAYER, or any under SEL routing) the notes also play and trigger. Each part has its own
 * setup, so one channel can duck both parts, or two channels one part each. */
/* The sidechain's MIDI setup (core.h SC_CH_DEF): per part its channel (0..15) and its note (0 ANY, 1..127), the device's, read from the settings bytes by ui_input.c sc_sync */
static uint8_t sc_midi[4] = {SC_CH_DEF, SC_NOTE_DEF, SC_CH_DEF, SC_NOTE_DEF};   /* P1 channel, P1 note, P2 channel, P2 note */
static uint32_t duck_note(uint32_t ch, uint32_t note, uint32_t on, uint32_t plays)
{
    uint32_t i, swallow = 0;
    for (i = 0; i < NSHOWN; i++) {
        uint32_t n = sc_midi[2u * i + 1u];
        if (trk[i].p[P_DKSRC] != 2 || sc_midi[2u * i] != ch || (n && note != n))   /* (SRC 2 = MIDI) */
            continue;
        if (on && (trk[i].p[P_DKDEP] > 0 || trk[i].mdk))   /* (0.2.7: or the matrix moves DEPTH) */
            duck_st[i].ph = 1;
        swallow |= (uint32_t)(n != 0) | (plays ^ 1u);
    }
    return swallow;
}
/* SRC, the trigger source. MIDI: the notes above. PUMP (stored 0, the default): no notes, a duck starts
 * on every RATE division (N_DIV, 1/4 = four on the floor) of the tempo, the same envelope (ATK, HOLD, RLS). It counts the
 * samples as the arp does (events_block: seq.c arp_step): CLK INT: n a block, free-running, the transport or not, on
 * div_samples (BPM); CLK USB / TRS: the samples the incoming MIDI clock advances (midi_clock_advance), so it follows
 * the external tempo and stands still while no clock runs (a stopped transport). PLAY / MIDI Start (seq_start, duck_start)
 * restarts the phase: a duck on the first block, then every division. With SRC PUMP the division count runs on while DEPTH is 0
 * (nothing ducks), so a DEPTH raised later ducks on the beat of the transport, not at the instant of the turn. A duck starts at
 * the block's start (under a block of latency). PART: a keyed compressor with the other part (1 by 2, 2 by 1) as the key: duck_part follows the other
 * part's level once a block (mix_part: its render times its LEVEL, before its own DIST, INSERT, EQ and duck, so the two
 * never key each other round; attack ~2 ms, release ~60 ms), the part just rendered's, or the last block's of the part
 * still to come (one block of latency). The reduction asked = DEPTH x clamp((key - THR) / (full - THR)), the full level
 * DUCK_KEY_FS (a part's x at 16384 of 32768); the gain goes down by ATK (the DEPTH-wide slope) and up by RLS (CURVE). The cost: nothing but a test of SRC in mix_part; a part used as a key, one
 * pass over its block's abs; a keyed part, a few multiplies a block and duck_run. MIDI notes do not trigger a part whose SRC is not MIDI. */
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
        uint32_t period, on = trk[i].p[P_DKDEP] > 0 || trk[i].mdk;   /* (0.2.7: or the matrix moves DEPTH) */
        if (trk[i].p[P_DKSRC] != 0) {
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
            k->ph = on ? 1 : k->ph;                     /* (DEPTH 0: the beats are counted, nothing ducks) */
            continue;
        }
        k->pos += adv;
        if (k->pos >= period) {
            k->pos -= period;
            if (k->pos >= period)
                k->pos = 0;
            k->ph = on ? 1 : k->ph;
        }
    }
}
/* t's block b (0: silent), n samples: the key follower for the other part, and this part's reduction asked by it.
 * DUCK SRC PART is on ONE part only (job 3; params.c duck_set_part): the ducked part D has SRC = PART, the other part K has none (SRC OFF) and only keys: K's
 * stage updates its own follower k->key (above), D's stage reads K's follower of the BLOCK BEFORE from the block's snapshot (bs->duck_key, filled by blk_snap before
 * either part runs). One direction, no waiting, whichever stage runs first or on which core. Before job 3 a ducked part 2 read part 1's key of THIS block: 0.73 ms
 * (one block) earlier than now, the ducker's fastest attack is 1 ms; a ducked part 1 read part 2's of the block before, as now (bit for bit) */
static __attribute__((noinline)) void duck_part(track_t *t, const int32_t *b, uint32_t n, const blockstate_t *bs)
{
    uint32_t id = (uint32_t)(t - trk) % NTRK, o = id ^ 1u, i;
    duck_t *k = &duck_st[id];
    if (trk[o].p[P_DKSRC] == 1) {                       /* the other part is keyed by this one */
        int32_t pk = 0, c;
        if (b)
            for (i = 0; i < n; i++) {
                int32_t a = b[i] < 0 ? -b[i] : b[i];
                if (a > pk)
                    pk = a;
            }
        pk = ((pk >> 2) * part_lvl(t)) >> 10;
        pk = pk > 32767 ? 32767 : pk;
        c = (int32_t)(n * 32768u / (n + (pk > k->key ? 88u : 2646u)));   /* 2 ms up, 60 ms down */
        k->key += ((pk - k->key) * c) >> 15;
    }
    if (t->p[P_DKSRC] == 1 && t->p[P_DKDEP] > 0) {
        int32_t thr = clamp(t->p[P_DKTHR], 0, 100) * (DUCK_KEY_FS / 100), kv = bs->duck_key[o], raw = 0;   /* (the other part's key of the block before) */
        int32_t top = (int32_t)((uint32_t)clamp(t->p[P_DKDEP], 0, 100) << 16) / 100;
        if (kv > thr && thr < DUCK_KEY_FS) {
            int32_t f = ((kv - thr) << 12) / (DUCK_KEY_FS - thr);
            raw = (top * (f > 4096 ? 4096 : f)) >> 12;
        }
        k->on = raw > 0;
        k->tg = raw;                                    /* (no HOLD: the reduction follows the key) */
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
    uint32_t i;
    if (as < 1)
        as = 1;
    if (rs < 1)
        rs = 1;
    if (k->ph == 4 && t->p[P_DKSRC] != 1)
        k->ph = 3;
    for (i = 0; i < n; i++) {
        if (k->ph == 4) {                               /* SRC PART: towards the asked reduction */
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
                k->ph = 3;                              /* (no HOLD: the release follows the attack) */
            }
        } else if (k->ph == 3) {
            k->dn -= ek ? (((k->dn * ek) >> 20) | 1) : rs;   /* (CURVE EXP: a share of what is left, 3 time constants in RLS) */
            if (k->dn <= 0) {
                k->dn = 0;
                k->ph = 0;
            }
        }
        if (b && k->dn) {
            int32_t g = (65536 - k->dn) >> 4;           /* (pedal hunt: >> 2 first, not >> 4: the 4 low bits cut off a ducked sample were a hiss of up to 15 LSB that pulsed with every duck
                                                         * and was gone between ducks, about 2 LSB rms at a PUMP DEPTH 14, and a DC wobble; |b| may now reach 2^21 = 64 x full scale, the corpus peaks at 0.8 x) */
            b[i] = ((b[i] >> 2) * g) >> 10;
            if (d)
                d[i] = ((d[i] >> 2) * g) >> 10;
        }
    }
}

/* ------------------------------------------------------------------------------------------------------------------------------
 * MIXSPLIT (0.2.25 test line, job 1 of the second-core work; docs/MIXSPLIT_NOTES.md has the table of what is shared). mix_block is a
 * chain of stages with explicit inputs and outputs, each a function of its own (noinline), the arithmetic of each exactly what was inline
 * in mix_block (renders bit for bit; tests/mixsplit_test.c):
 *   mix_events    EVENTS  notes, sequencer, arp, MIDI, clock, the MODE and SENDS state machines. Control: writes the voices' note state, the
 *                         parts' parameters and the mode/sends flags; reads and writes no audio buffer. Runs before the parts, never between
 *   per part, in this order (mix_part below), the data in a mixpart_t:
 *   part_voice    VOICES  the part's voices into its pscr buf (mod_begin first: the matrix's values into t->p), or the silent way out
 *   part_rest     (silent part) the effects' state at rest, the pan and SAT taken at once; mod_end; nothing is mixed
 *   part_fx       EFFECTS PAN / SAT follow, DUCK key, SAT (track_dist), INSERT, EQ, PERFORM's MUTE, the DUCK, PART FX crossfade (or
 *                         mix_spread, which also mixes: SPREAD); in place on the part's buf
 *   part_mix      LEVEL, BALANCE, PAN into the part's own accumulators pacc[pp] (dry mix, sends, SPACE's floors) and the part's peak
 *   mix_gather    GATHER  the parts' accumulators summed, in a fixed order, into mix_l / mix_r / send_* / sp_f* (SPLIT: part 1's to bus A's sa_*)
 *   mix_sends     SENDS   the shared buses (chorus, delay, reverb; DUEL's A and B) take both parts' sends and add the return (wet)
 *   mix_master    MASTER  PERFORM's master, SPACE's width, the MODE's dip, WIDTH, BASS MONO, COMP, the level, the DC blocker, limiter and soft
 *                         clip (master_out), or DUEL's split_master. Reads mix_l / mix_r / wet, writes out
 * The parts' stages share nothing but the scratch and accumulators named in each stage's comment; two parts today run one after the other
 * in one core and the order matters only where the comments say so (DUCK SRC PART: the other part's key, one block old for part 1).
 * ------------------------------------------------------------------------------------------------------------------------------ */

/* VOICE stage. Reads: t (voices, parameters, envelope and LFO state), the matrix and MORPH values (morph_begin has already written the
 * block's values into t->p; mod_begin adds the matrix's), song.g's shared voice settings (tune, MIDI bend, vage), dm_split. Writes: the part's buf
 * (via b), t's voices / envelopes / LFOs / tail, mod (the part in its window: shared scratch, one part at a time), the SPREAD scratch
 * (pscr[pp].spread_on and sbuf, the part's own), the FM6 operator buses (on fm6_render's stack), the engine memory of the part (eng_mem[pp], its own).
 * Returns 1 when the part sounds (a voice, or the DIST / INSERT / EQ tail running out), 0 when silent (part_rest then follows) */
static __attribute__((noinline)) int part_voice(mixpart_t *m, track_t *t, uint32_t n, const blockstate_t *bs)
{
    int32_t *b = pscr[PART_OF(t)].buf;
    m->t = t;
    m->n = n;
    m->b = b;
    m->pp = (uint32_t)(t - trk) % NLIVE;
    m->acc = &pacc[m->pp];
    m->scr = &pscr[m->pp];
    m->bs = bs;
    pacc_on[m->pp] = 0;                                 /* (the part has not written its accumulators: part_mix / mix_spread do, a silent part not) */
    m->pan_st = t->p[P_PAN];                             /* (0.2.24: the STORED pan before the matrix writes the block's; pan_follow) */
    m->dist_st = t->p[P_DIST];                           /* (0.2.25: the STORED SAT likewise; dist_follow) */
    mod_begin(t);                                       /* the matrix's per-block values into t->p (mod.c) */
    m->spread = pscr[m->pp].spread_on = t->p[P_SPRD] > 0 && (trk_vmode(t) == V_POLY || trk_vmode(t) == V_UNISON) && !bs->dm_split;   /* (SPREAD,
                                                         * mix_spread; SPLIT: none, the part is mono on its side) */
    if (track_render(t, b, n))
        t->tail = t->p[P_ITYPE] ? 64 : 16;              /* blocks of DIST / INSERT state to run out after the last
                                                         * voice (the INSERT's: 46 ms, a flanger's feedback) */
    else if (!t->tail || !(t->p[P_DIST] | t->p[P_ITYPE] | t->p[P_EQLG] | t->p[P_EQMG] | t->p[P_EQHG]) ||
             pfx_x[(uint32_t)(t - trk) % NLIVE] == 32768 || !--t->tail)   /* (0.2.14 MODULES PART FX OFF: no DIST / INSERT tail to run out) */
        return 0;
    return 1;
}

/* SILENT part (part_voice returned 0): a part with no voice sounding costs the LFO tick and a cleared buffer only (after the DIST tail has run
 * out). Reads: t, the other part's DUCK source (P_DKSRC), the block's snapshot (mods_off, pf_mute, mute_tg). Writes: t's EQ / INSERT / PART FX / DUCK state at rest,
 * pan_x[pp], dist_x[pp], dist_live[pp], pan_gv[pp], pf.mg[pp] (PERFORM's MUTE gain), duck_st[pp] and, for DUCK SRC PART, duck_st[pp].key
 * (the key follower the other part reads), t->p[P_PAN / P_DIST] as stored again (mod_end). No audio buffer is touched */
static __attribute__((noinline)) void part_rest(mixpart_t *m)
{
    track_t *t = m->t;
    uint32_t n = m->n, pp = m->pp;
    int32_t pan_st = m->pan_st;
    if (t->eqlv)
        eq_rest(t);                                     /* (the EQ's states clear with the tail) */
    pfx_snap(t, m->bs);                                 /* (PART FX: what is asked, at once: nothing sounds) */
    if (!(m->bs->mods_off & MODS_PFX)) {
        if (t->p[P_DKSRC] == 1 || trk[(uint32_t)(t - trk) ^ 1u].p[P_DKSRC] == 1)
            duck_part(t, 0, n, m->bs);                  /* (SRC PART: a silent key, and the part keyed by the other) */
        if (duck_st[(uint32_t)(t - trk) % NTRK].ph)
            duck_run(t, 0, 0, n);                       /* (the DUCK's envelope runs on) */
    }
    if (t->ins_run && !t->p[P_ITYPE])
        ins_rest(t);                                    /* TYPE OFF while silent: no fade-out left for the next note */
    if ((m->bs->pf_mute >> pp) & 1u)
        perf_mute_rest(pp, m->bs->mute_tg[pp]);         /* (PERFORM's MUTE: silent, its gain where it goes) */
    pan_follow(pp, pan_st, 1, 0);                       /* (silent: the pan at once) */
    dist_follow(pp, t->p[P_DIST], 1);                   /* (.. and the SAT: what mod_begin left in t->p, as A's mix_part) */
    pan_gv[pp] = 0;
    mx_ok[pp] = 0;                                      /* (0.2.25: the matrix's mix ramp starts again from this block's values) */
    sp_go[pp] = 0;
    mx_was[pp] = mod_mixmv[pp];
    if (MODW(t).on)
        mod_end(t);
}

/* EFFECT stage (the part's insert chain), on the part's block b in place. Reads: t's parameters (SAT, INSERT, EQ, DUCK, PAN), the block's snapshot (mods_off for PART FX,
 * pf_mute, mute_tg), the OTHER part's DUCK source and its key follower (duck_st[pp ^ 1].key: DUCK SRC PART, the other part's level of this block or the
 * block before), the morph's / matrix's values already in t->p. Writes: b (the part's buf), t's INSERT / EQ / SAT states, pan_x[pp], dist_x[pp],
 * dist_live[pp], pfx_x[pp], pscr[pp].dry / dryd (the part's own), pscr[pp].sbuf (SPREAD), duck_st[pp] (its own key follower: read by
 * the other part), pf.mg[pp], t->p[P_DIST] (the SAT of the block; part_end puts the stored back), and with SPREAD the whole mix (mix_spread:
 * the part's pacc, t->peak: see part_mix). Fills m->fxm, m->pan_mv, m->pq for part_mix */
static __attribute__((noinline)) void part_fx(mixpart_t *m)
{
    track_t *t = m->t;
    int32_t *b = m->b;
    uint32_t n = m->n, pp = m->pp, fxm;
    int32_t pan_mv, pq;
    pan_mv = pan_follow(pp, m->pan_st, 0, m->spread);
    pq = pan_x[pp] + ((t->p[P_PAN] - m->pan_st) << 8);  /* the pan this block mixes at, Q8: the smoothed stored pan plus the matrix's offset as it was */
    {
        int32_t dw0 = dist_x[pp], dlv = dist_live[pp];  /* (0.2.25: the followed SAT before and after this block's step, for the blend ramp) */
        t->p[P_DIST] = (int16_t)dist_follow(pp, t->p[P_DIST], 0);   /* the SAT this block runs at (put back below; mod_end puts the stored back too) */
        m->dist_rw0 = dlv ? dw0 : dist_x[pp];           /* (in the part's own context: with two cores the parts' stages overlap) */
    }
    fxm = pfx_mode(t, m->bs);                           /* (0.2.14 MODULES PART FX: PFX_RUN as ever, PFX_FADE, PFX_SKIP) */
    if (fxm != PFX_SKIP && (t->p[P_DKSRC] == 1 || trk[(uint32_t)(t - trk) ^ 1u].p[P_DKSRC] == 1))
        duck_part(t, b, n, m->bs);                      /* DUCK SRC PART: this block's level for the other part, its key for this */
    if (m->spread) {
        acc_prep(m);
        spread_run(m, fxm, pq);
        mx_was[pp] = mod_mixmv[pp];
    } else {
        if (fxm == PFX_FADE)
            memcpy(pscr[pp].dry, b, n * sizeof b[0]);   /* (PART FX changing: the dry block for the crossfade) */
        if (fxm != PFX_SKIP) {
            if (m->dist_rw0 != dist_x[pp] && (m->dist_rw0 < 12 * 256 || dist_x[pp] < 12 * 256))
                track_dist_ramp(t, b, n, m->dist_rw0, dist_x[pp]);   /* (0.2.25: SAT moving in its blend zone: the blend ramps inside the block) */
            else
                track_dist(t, b, n);
            if (t->p[P_ITYPE] | t->ins_run)
                track_insert(t, b, n);                  /* the INSERT (1.5): off, nothing */
            if (EQ_ON(t))
                track_eq(t, b, n, 0u);                  /* the EQ (DUELL): all bands at 0 dB, nothing */
        }
        if ((m->bs->pf_mute >> pp) & 1u) {              /* PERFORM: MUTE 1 / 2 held (perform.c) */
            if (fxm == PFX_FADE)
                pfx_mute(pp, b, 0, n, m->bs->mute_tg[pp]);
            else
                perf_mute(pp, b, n, m->bs->mute_tg[pp]);
        }
        if (fxm != PFX_SKIP && duck_st[(uint32_t)(t - trk) % NTRK].ph)
            duck_run(t, b, 0, n);                       /* the DUCK (DUELL): before the level, pan and the sends */
        if (fxm == PFX_FADE)
            pfx_mix(t, b, 0, n, m->bs);
    }
    m->fxm = fxm;
    m->pan_mv = pan_mv;
    m->pq = pq;
}

/* MIX stage (the plain, no SPREAD, part: with SPREAD part_fx's mix_spread has done this). The part's block into the dry mix and the sends.
 * Reads: b (the part's buf), t's LEVEL / TRIM / CHOR / DLY / REV / PEAK, mix_bal (BALANCE), snd_st, dm_split, sp_tap, the pan state (pan_g[pp],
 * pan_gv[pp]). Writes (assigns, into the part's OWN accumulators pacc[pp]): l, r, sc, sd, sr, fc / fd / fr (SPACE's floors, via space_tap, which adds),
 * pan_g[pp], pan_gv[pp], t->peak. Nothing is shared with the other part: mix_gather sums the parts' accumulators */
static __attribute__((noinline)) void part_mix(const mixpart_t *m)
{
    track_t *t = m->t;
    int32_t *b = m->b;
    part_acc_t *ac = m->acc;
    const blockstate_t *bs = m->bs;
    uint32_t n = m->n, pp = m->pp, i;
    int32_t pq = m->pq;
    int32_t pan_mv = m->pan_mv;
    if (!m->spread) {                                   /* (the mix's values read after the calls: none is held over
                                                         * them, no spill in the loop: the ISR's cost as before 1.5) */
        int32_t lvl = bal_lvl(t, part_lvl(t), bs->mix_bal);
        int32_t gl = 4096 - (pq > 0 ? pq >> 2 : 0), gr = 4096 + (pq < 0 ? pq >> 2 : 0);   /* (pq Q8: pq >> 2 = pan x 64, the gains of before) */
        int32_t c = t->p[P_CHOR] * 258, d = t->p[P_DLY] * 258, r = t->p[P_REV] * 258, pk = t->peak;
        int ramp = 0;
        if (bs->snd_st == SND_OFF)
            c = d = r = 0;                              /* (0.2.14 MODULES SENDS OFF: nothing sent) */
        if (bs->dm_split) {                             /* SPLIT: part 1 left, the others right, PAN not heard */
            gl = t == trk ? 4096 : 0;
            gr = 4096 - gl;
        } else if (pan_mv) {                            /* (0.2.24: the stored pan on its way: the gains in a straight line from the block before's) */
            ramp = pan_gv[pp] && (pan_g[pp][0] != gl || pan_g[pp][1] != gr);
        }
        int32_t xmax = c > d ? c : d;
        xmax = 0x7FFFFFFF / ((xmax > r ? xmax : r) | 1);   /* sends: loud chords at a high LEVEL */
        acc_prep(m);
        if ((((uint32_t)mod_mixmv[pp] | mx_was[pp]) & 1u) && mx_ok[pp] &&   /* (0.2.25: the matrix moves PAN, PATCH LEVEL or a send: all six in a line inside the block) */
            (mx_lv[pp] != lvl || mx_sd[pp][0] != c || mx_sd[pp][1] != d || mx_sd[pp][2] != r || (pan_gv[pp] && (pan_g[pp][0] != gl || pan_g[pp][1] != gr)))) {
            pacc_on[pp] |= (uint8_t)((mx_sd[pp][0] ? 2u : 0u) | (mx_sd[pp][1] ? 4u : 0u) | (mx_sd[pp][2] ? 8u : 0u));   /* (a send ramping down to 0 still has samples to write) */
            pk = mix_ramp(m, pk, gl, gr, lvl, c, d, r);
        } else if (ramp) {
            int32_t glq = pan_g[pp][0] << 16, grq = pan_g[pp][1] << 16, dgl = ((gl - pan_g[pp][0]) << 16) / (int32_t)n, dgr = ((gr - pan_g[pp][1]) << 16) / (int32_t)n;
            for (i = 0; i < n; i++) {
                int32_t x = ((b[i] >> 2) * lvl) >> 10, a = x < 0 ? -x : x;
                int32_t xs = clamp(x, -xmax, xmax);
                if (a > pk)
                    pk = a;
                if (c)
                    ac->sc[i] = mulq15(xs, c);
                if (d)
                    ac->sd[i] = mulq15(xs, d);
                if (r)
                    ac->sr[i] = mulq15(xs, r);
                glq += dgl;
                grq += dgr;
                ac->l[i] = (x * (glq >> 16)) >> 12;
                ac->r[i] = (x * (grq >> 16)) >> 12;
            }
        } else
        for (i = 0; i < n; i++) {
            int32_t x = ((b[i] >> 2) * lvl) >> 10, a = x < 0 ? -x : x;   /* pre-shift: 8 loud voices */
            int32_t xs = clamp(x, -xmax, xmax);         /* sends: mulq15 would overflow */
            if (a > pk)
                pk = a;
            if (c)
                ac->sc[i] = mulq15(xs, c);
            if (d)
                ac->sd[i] = mulq15(xs, d);
            if (r)
                ac->sr[i] = mulq15(xs, r);
            ac->l[i] = (x * gl) >> 12;
            ac->r[i] = (x * gr) >> 12;
        }
        pan_g[pp][0] = gl;
        pan_g[pp][1] = gr;
        pan_gv[pp] = (uint8_t)!bs->dm_split;
        mx_lv[pp] = lvl, mx_sd[pp][0] = c, mx_sd[pp][1] = d, mx_sd[pp][2] = r;
        mx_ok[pp] = 1;
        sp_go[pp] = 0;
        mx_was[pp] = mod_mixmv[pp];
        t->peak = pk;
        if (bs->sp_tap)
            space_tap(b, lvl, c, d, r, n, ac, &pacc_on[pp]);  /* (SPACE right: the part's send floors) */
    }
}

/* the end of a sounding part's window: the stored SAT back (mod_end below also does), then the matrix's stored values */
static inline void part_end(const mixpart_t *m)
{
    m->t->p[P_DIST] = (int16_t)m->dist_st;              /* (the stored SAT back; mod_end below also does) */
    if (MODW(m->t).on)
        mod_end(m->t);                                  /* the stored values back */
}

/* one synth part into the dry mix and the sends: its stages in order (a part with no voice sounding costs the LFO tick and a cleared buffer only).
 * The callers' morph_begin / morph_end bracket it (mix_block); this function is the one place that fixes the order of a part's stages */
static void mix_part(track_t *t, uint32_t n, const blockstate_t *bs)
{
    mixpart_t m;
    if (!part_voice(&m, t, n, bs)) {
        part_rest(&m);
        return;
    }
    part_fx(&m);
    part_mix(&m);
    part_end(&m);
}

#if DUELL_TWOCORE
#include "twocore_run.c"                             /* DUELL TWO-CORE stage 1: the job core 1 plays (part 2) and the runner (docs/TWOCORE_STAGE1.md) */
#endif

/* GATHER stage, after the last part and before mix_sends: the parts' accumulators that were written this block (pacc_on; in the order pacc[0], pacc[1] ..) summed
 * into the globals the send stage and the master read: mix_l, mix_r, send_*, and with sp_tap SPACE's floors sp_f*. A silent part wrote nothing and costs nothing
 * here; with no part on the arrays are 0, as the clear at the head of a block used to leave them. SPLIT (DUEL, sends on): part 1's sends and floors go to bus
 * A's arrays (sa_*, sp_f*_a), the others' sum to bus B's (send_*, sp_f*). Integer adds: the sum does not depend on the order the parts ran in */
static void acc_sum(int32_t *d, part_acc_t *const *u, uint32_t nu, uint32_t k, uint32_t n)
{
    uint32_t i, j;
    if (!nu) {
        for (i = 0; i < n; i++)
            d[i] = 0;
    } else if (nu == 1u) {
        const int32_t *a = &u[0]->l[0] + k * CTL;
        for (i = 0; i < n; i++)
            d[i] = a[i];
    } else if (nu == 2u) {
        const int32_t *a = &u[0]->l[0] + k * CTL, *b = &u[1]->l[0] + k * CTL;
        for (i = 0; i < n; i++)
            d[i] = a[i] + b[i];
    } else {
        for (i = 0; i < n; i++) {
            int32_t x = 0;
            for (j = 0; j < nu; j++)
                x += (&u[j]->l[0] + k * CTL)[i];
            d[i] = x;
        }
    }
}
/* the general case: the lists of the parts that wrote each array, bus A's (SPLIT: part 1) and the others' */
static void gather_general(uint32_t n, uint32_t st, uint32_t sp, uint32_t sends)
{
    part_acc_t *all[NLIVE], *ba[3][NLIVE], *bb[3][NLIVE], *fa[NLIVE], *fb[NLIVE];
    uint32_t p, k, nl = 0, na[3] = {0, 0, 0}, nb[3] = {0, 0, 0}, nfa = 0, nfb = 0;
    for (p = 0; p < NLIVE; p++) {
        uint32_t on = pacc_on[p];
#if DUELL_TWOCORE
        if (p == 1u && tcr.skip2)
            continue;                                   /* (part 2 is silent this block and core 1 may still be writing it: not even read) */
#endif
        if (!on)
            continue;
        all[nl++] = &pacc[p];
        for (k = 0; k < 3u; k++)
            if ((on >> (k + 1u)) & 1u) {
                if (st && p == 0u)
                    ba[k][na[k]++] = &pacc[p];
                else
                    bb[k][nb[k]++] = &pacc[p];
            }
        if (on & 16u) {
            if (st && p == 0u)
                fa[nfa++] = &pacc[p];
            else
                fb[nfb++] = &pacc[p];
        }
    }
    if (sends) {
        acc_sum(mix_l, all, nl, 0u, n);
        acc_sum(mix_r, all, nl, 1u, n);
        if (st) {                                       /* (SPLIT: part 1 is bus A) */
            acc_sum(sa_c, ba[0], na[0], 2u, n);
            acc_sum(sa_d, ba[1], na[1], 3u, n);
            acc_sum(sa_r, ba[2], na[2], 4u, n);
        }
        acc_sum(send_c, bb[0], nb[0], 2u, n);
        acc_sum(send_d, bb[1], nb[1], 3u, n);
        acc_sum(send_r, bb[2], nb[2], 4u, n);
    }
    if (sp) {
        if (st) {
            acc_sum(sp_fc_a, fa, nfa, 5u, n);
            acc_sum(sp_fd_a, fa, nfa, 6u, n);
            acc_sum(sp_fr_a, fa, nfa, 7u, n);
        }
        acc_sum(sp_fc, fb, nfb, 5u, n);
        acc_sum(sp_fd, fb, nfb, 6u, n);
        acc_sum(sp_fr, fb, nfb, 7u, n);
    }
}
static __attribute__((noinline)) void mix_gather(uint32_t n, const blockstate_t *bs)
{
    const part_acc_t *a = 0, *b = 0, *fl0 = 0, *fl1 = 0;   /* the first two parts that wrote l / r, and the floors */
    uint32_t p, i, nl = 0, nf = 0, am = 15u, om = 0, st = bs->dm_split && bs->snd_st != SND_OFF, sp = bs->sp_tap;
    for (p = 0; p < NLIVE; p++) {
        uint32_t on = pacc_on[p];
#if DUELL_TWOCORE
        if (p == 1u && tcr.skip2)
            continue;                                   /* (part 2 is silent this block and core 1 may still be writing it: not even read) */
#endif
        spread_post(p, n, bs);                          /* (hunt1: SPREAD's gains in a line while a route or the MORPH moves them; changes pacc_on) */
        on = pacc_on[p];
        if (!on)
            continue;
        am &= on;
        om |= on;
        if (!nl)
            a = &pacc[p];
        else if (nl == 1u)
            b = &pacc[p];
        nl++;
        if (on & 16u) {
            if (!nf)
                fl0 = &pacc[p];
            else if (nf == 1u)
                fl1 = &pacc[p];
            nf++;
        }
    }
    uint32_t fast = !st && nl < 3u && ((am & 15u) == 15u || !(om & 14u)), flfast = sp && !st && nf < 3u;
    if (fast) {   /* the common blocks in one loop each: nothing sounding (the clear of before), one part, two; the sends all there or all 0 */
        uint32_t snd = nl && (am & 15u) == 15u;
        if (!nl) {
            for (i = 0; i < n; i++)
                send_c[i] = send_d[i] = send_r[i] = mix_l[i] = mix_r[i] = 0;
        } else if (nl == 1u && snd) {
            for (i = 0; i < n; i++) {
                mix_l[i] = a->l[i];
                mix_r[i] = a->r[i];
                send_c[i] = a->sc[i];
                send_d[i] = a->sd[i];
                send_r[i] = a->sr[i];
            }
        } else if (nl == 1u) {
            for (i = 0; i < n; i++) {
                mix_l[i] = a->l[i];
                mix_r[i] = a->r[i];
                send_c[i] = send_d[i] = send_r[i] = 0;
            }
        } else if (snd) {
            for (i = 0; i < n; i++) {
                mix_l[i] = a->l[i] + b->l[i];
                mix_r[i] = a->r[i] + b->r[i];
                send_c[i] = a->sc[i] + b->sc[i];
                send_d[i] = a->sd[i] + b->sd[i];
                send_r[i] = a->sr[i] + b->sr[i];
            }
        } else {
            for (i = 0; i < n; i++) {
                mix_l[i] = a->l[i] + b->l[i];
                mix_r[i] = a->r[i] + b->r[i];
                send_c[i] = send_d[i] = send_r[i] = 0;
            }
        }
    }
    if (!fast || (sp && !flfast))
        gather_general(n, st, sp && !flfast, !fast);   /* (everything else: SPLIT, three parts or more, mixed sends) */
    if (flfast) {                         /* SPACE's floors: only the parts whose space_tap ran wrote them; none: 0, as the clear left them */
        if (!nf) {
            for (i = 0; i < n; i++)
                sp_fc[i] = sp_fd[i] = sp_fr[i] = 0;
        } else if (nf == 1u) {
            for (i = 0; i < n; i++) {
                sp_fc[i] = fl0->fc[i];
                sp_fd[i] = fl0->fd[i];
                sp_fr[i] = fl0->fr[i];
            }
        } else {
            for (i = 0; i < n; i++) {
                sp_fc[i] = fl0->fc[i] + fl1->fc[i];
                sp_fd[i] = fl0->fd[i] + fl1->fd[i];
                sp_fr[i] = fl0->fr[i] + fl1->fr[i];
            }
        }
    }
}

/* the HOME oscilloscope (ui_graph.c graph_scope): every other sample of the left channel. MENU > DISPLAY > SCOPE (1.2,
 * Discussion #165): OUT (0, the default) what goes out, after MASTER (audio.c audio_block, before the click); MIX the
 * mix before MASTER (and before SPEAKER EQ and the limiter), at the level MASTER at its top gives, so its
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

/* PERFORM at work (perform.c): DRIVE, the master level, its master stages (stereo, both channels: in DUEL the left is part 1 and the right
 * part 2, and every stage keeps the two apart). mix_block goes on as ever after it (the MODE's dip, then master_out or master_out_split)
 * with its level at 1024 on mix_l / mix_r x 4: ((x << 2) >> 2) * 1024 >> 10 = x, exactly. So master_out has its one call site still
 * (no second copy of the limiter in the audio ISR, no change to its path when PERFORM rests) */
#define PERF_MG 1024
static __attribute__((noinline)) void perf_master(uint32_t n)
{
    uint32_t i;
    int32_t mg = MIX_MG();                              /* (the device: the default level, MASTER scales the DAC after: audio.c master_dac; host tests: see MIX_MG) */
    if ((pf.act & PF_BIT(PF_DRV)) || pf.dw) {           /* DRIVE: before the level (MASTER scales what it gives) */
        for (i = 0; i < n; i++) {
            mix_l[i] += wet[i];
            mix_r[i] += wet[i];
            wet[i] = 0;
        }
        perf_drive(mix_l, mix_r, n);
    }
    for (i = 0; i < n; i++) {
        mix_l[i] = (((mix_l[i] + wet[i]) >> 2) * mg) >> 10;
        mix_r[i] = (((mix_r[i] + wet[i]) >> 2) * mg) >> 10;
        wet[i] = 0;
    }
    perf_block(mix_l, mix_r, n);
    for (i = 0; i < n; i++) {                           /* (for mix_block's level at PERF_MG) */
        mix_l[i] <<= 2;
        mix_r[i] <<= 2;
    }
}

/* EVENT stage: everything that is not audio and runs at the head of a block. events_block (seq.c: MIDI in, notes, the sequencer, the arp, the
 * clocks, the PUMP DUCK's beat count), then the MODE's step (the dip, dm_step) and MODULES SENDS' (snd_step). Reads: the MIDI / USB queues,
 * the transport, song.*. Writes: the voices' note state (t->v, envelopes, midi_bend), the parts' parameters (t->p, via the sequencer, CCs
 * and the morph's queue), duck_st[].ph / run / pos, dm_*, snd_st, mod_dk. It touches NO audio buffer (mix_l, send_*, pscr[].buf, ...), so it
 * can run on the core that owns the events while the other core renders: its outputs are the parts' and the buses' controls the
 * render stages then read (see MIXSPLIT_NOTES.md). Takes and gives no audio data */
static __attribute__((noinline)) void mix_events(uint32_t n)
{
    duck_src_norm();                                    /* DUCK SRC PART on one part only (params.c): a pair that breaks the rule (a multi, a project) is mended here */
    events_block(n);
    if (dm_cur != duell_mode || dm_g != DM_ONE || dm_clr)
        dm_step();                                      /* DUELL step 5: the MODE changing (the dip) */
    if (snd_st != SND_ON || (mods_off & MODS_SENDS) || snd_panic)
        snd_step();                                     /* DUELL 0.2.14 MODULES SENDS: OFF, fading, clearing its lines */
}

/* SEND stage: the shared buses. Takes both parts' sends (send_c / send_d / send_r, summed from the parts' accumulators by mix_gather; in DUEL part 1's are
 * sa_*) and SPACE's floors (sp_f*), gives the effect return wet[] and the side
 * returns added to mix_l / mix_r (HALL, PING-PONG), and in DUEL each bus's return into its side. Reads: song.g (the bus settings), sv (SPACE),
 * snd_st, dm_*, hl / dly_side, the matrix's bus values (mod_gon / mod_goff: both parts' offsets). Writes: wet, mix_l, mix_r (the side
 * returns), snd_dl / snd_dr (a SENDS fade's dry copy), the buses' delay lines / reverb / chorus states (the pool: the one owner of each),
 * sv.*, mod_gkeep / mod_gw (the bus values written over for the block and put back at its end) */
static __attribute__((noinline)) void mix_sends(uint32_t n)
{
    uint32_t i;
    if (mod_gon_any())                                  /* 0.2.7: the parts' matrix offsets on the bus values (mod.c mod_bus): DUEL bus A part 1's, */
        mod_bus(dm_split ? 1u : ~0u);                   /* (split_buses: bus B part 2's), STEREO / LAYER the shared buses both parts' added */
    sv.spc0 = sv.spc;                                   /* (the width's ramp starts where the last block ended) */
    if (space_goal() || sv.spc) {                       /* PERFORM's K2 SPACE (above; in the middle and at rest: nothing; SENDS OFF: to the middle) */
        space_step();
        if (snd_st != SND_OFF && (sv.on || sv.sg0 != 32768)) {
            space_sends(send_c, send_d, send_r, sp_fc, sp_fd, sp_fr, n);
            if (dm_split)
                space_sends(sa_c, sa_d, sa_r, sp_fc_a, sp_fd_a, sp_fr_a, n);   /* (DUEL: bus A's, part 1's) */
        }
    } else if (sv.on || sv.sg != 32768) {
        sv.on = 0;                                      /* (back in the middle: the stored values again, no gain left) */
        sv.sg = sv.sg0 = 32768;
        sv.ac = sv.ad = sv.ar = 0;
    }
    if (snd_st == SND_FADE) {                           /* (MODULES SENDS changing: the dry mix kept, snd_fade below) */
        memcpy(snd_dl, mix_l, n * sizeof mix_l[0]);
        memcpy(snd_dr, mix_r, n * sizeof mix_r[0]);
    }
    if (dm_hold || snd_st == SND_OFF) {                 /* (the dip's silence: the lines being cleared; MODULES SENDS OFF: no bus) */
        for (i = 0; i < n; i++)
            wet[i] = 0;
    } else if (dm_split) {
        split_buses(n);
    } else {
        fx_buses(send_c, send_d, send_r, wet, n);
        if (hl.side)
            rev_side_mix(mix_l, mix_r, n);              /* HALL: stereo */
        if (dly_side)
            dly_side_mix(mix_l, mix_r, n);              /* PING-PONG: the repeats left and right */
    }
    if (snd_st == SND_FADE)
        snd_fade(n);                                    /* (MODULES SENDS: the buses' share of the mix toward OFF or ON) */
    if (mod_gw)
        mod_bus_end();                                  /* (the stored bus values back: the main loop never sees a modulated one) */
}

/* MASTER stages, after the buses: PERFORM's master (when held), SPACE's width, the MODE's dip, WIDTH, BASS MONO, COMP, then the level and
 * master_out's DC blocker, limiter and soft clip (DUEL: split_master, each side apart). Takes mix_l / mix_r (the dry mix with the side returns) and
 * wet (the return), gives out[2n] (interleaved L R). mg: the master level Q12 (the reference MASTER_USB_REF; mix_block);
 * perf: PERFORM at work this block. Reads: song.* (master, WIDTH, BASS MONO, COMP settings), the pf.* / sv.* / dm_* state. Writes: mix_l, mix_r,
 * wet (PERFORM's stage), the stages' own states (bm, wdn, cmp, the limiter's envelope and DC states: only these stages touch them) and out.
 * Nothing of the parts' states is touched, so this is the one stage that must see the sum of both parts */
static __attribute__((noinline)) void mix_master(int32_t *out, uint32_t n, int perf, int32_t mg)
{
    uint32_t i;
    int32_t mg0 = mg;
    if (perf) {
        perf_master(n);                                 /* (then the dip and master_out as ever, the level done) */
        mg = PERF_MG;
    }
    if (!dm_split && sw_on())
        space_width(n);                                 /* SPACE's stereo width (STEREO, LAYER; never DUEL: its sides are the two parts) */
    if (dm_g != DM_ONE || dm_dir)
        dm_ramp(n);
    if (!dm_split && (wd_sel != 100u || wdn.k))
        width_stage(n);                                 /* DUELL 0.2.23 WIDTH, the master stereo width (STEREO, LAYER; not in DUEL; 100 %: this test only) */
    if (!dm_split && (bm_sel || bm.a))
        bass_mono(n);                                   /* DUELL 0.2.19 BASS MONO (STEREO, LAYER; not in DUEL; OFF: this test only) */
    if (!dm_split && ((cmp_cfg & 0xF00u) || cmp.a))
        comp_stage(n, perf ? cmp_poff(mg0) : 0);        /* DUELL 0.2.21 COMP, the master compressor (STEREO, LAYER; not in DUEL; RATIO 1:1: this test only; PERFORM's stage has applied MASTER: poff) */
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

/* the block's snapshot (blockstate_t): called once, after EVENTS and PERFORM's begin, before the first part; plain reads of the variables the parts'
 * stages used to read themselves (the UI-written ones are volatile) */
static __attribute__((noinline)) void blk_snap(blockstate_t *bs, uint32_t sp)
{
    uint32_t k;
    bs->dm_g = dm_g;
    for (k = 0; k < NLIVE; k++)
        bs->duck_key[k] = duck_st[k].key;
    bs->mix_bal = mix_bal;
    bs->pf_act = perf_act;
    for (k = 0; k < NLIVE; k++)
        bs->mute_tg[k] = (perf_held >> (PF_M1 + k)) & 1u && !perf_kill ? 0 : 32768;
    bs->dm_cur = dm_cur;
    bs->dm_split = dm_split;
    bs->dm_hold = dm_hold;
    bs->dm_clr = dm_clr;
    bs->dm_dir = dm_dir;
    bs->perf_space = perf_space;
    bs->perf_filt = perf_filt;
    bs->perf_depth = perf_depth;
    bs->snd_st = snd_st;
    bs->mods_off = mods_off;
    bs->sp_tap = (uint8_t)sp;
    bs->pf_mute = pf.mute;
}

/* One block, in the stages above: EVENTS, the PARTS (voices, effects, mix), GATHER, SENDS, the meters, MASTER. The order is the data flow: each part
 * clears and adds into its own accumulators (pacc), mix_gather sums them into mix_l / mix_r / send_*, the sends read them, the master reads the sum. The block's frame setup (SPACE's tap, PERFORM's begin) stays here: a few tests and the PERFORM calls */
static void mix_block(int32_t *out, uint32_t n)
{
#if !DUELL_TWOCORE
    uint32_t i;
#endif
    int perf;
    blockstate_t bs;
    int32_t mg = MIX_MG();                              /* (the device: the default level, MASTER scales the DAC after: audio.c master_dac; host tests: see MIX_MG) */
    mix_events(n);
    sp_tap = snd_st != SND_OFF && (perf_space > 0 || sv.spc > 0);   /* (SPACE right of the middle, or gliding there: the parts' dry mix for its floor) */
    if (perf_idle()) {                                  /* PERFORM (perform.c) idle: nothing but its 1/16's clock */
        perf_tick(n);
        perf = 0;
    } else {
        perf = perf_begin(n);
    }
    blk_snap(&bs, sp_tap);                              /* the shared control state, once: the parts only read it */
#if DUELL_TWOCORE
    tcr_begin(&bs, n);                                  /* DUELL TWO-CORE stage 1: part 2 goes to core 1 (twocore_run.c), part 1 stays here */
    {
        int mo = morph_begin(&trk[0], 1);
        mix_part(&trk[0], n, &bs);
        if (mo)
            morph_end(&trk[0]);
    }
    tcr_part2(&bs, n);                                  /* (core 1's part 2 awaited, or core 0 plays it, or it is silent) */
#else
    for (i = 0; i < NLIVE; i++) {                      /* (DUELL: tracks 3..4 do not render) */
        int mo = morph_begin(&trk[i], 1);               /* DUELL MORPH: the fader's values into t->p (morph.c) */
        mix_part(&trk[i], n, &bs);
        if (mo)
            morph_end(&trk[i]);                         /* .. and the stored ones back */
    }
#endif
    mix_gather(n, &bs);                                 /* the parts' accumulators into mix_l / mix_r / send_* (SPLIT: part 1's sends to bus A) */
    mix_sends(n);
    if (meter_on) {
        meter_on--;
        if (dm_split)
            meter_scan(n);                              /* (GLO's page shows: SPLIT's two buses) */
    }
    if (scope_mix)
        scope_take_mix(n);
    mix_master(out, n, perf, mg);
}
