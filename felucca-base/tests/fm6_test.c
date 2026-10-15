/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FM6 engine test (src/eng_fm6.c, src/fm6_core.c) on the Mac, through hostsim.c as regress.c.
 *   build/host/fm6_test [DEMODIR]          (run_tests.sh: build/fm6_demo)
 * 1. algorithms: every one of the 32 has its classic carrier count; with every operator at full level and
 *    a ratio of 1, each sounds, and its level grows with its carriers.
 * 2. envelopes: a carrier goes through its stages (attack to L1, decay to L2 then L3, held at L3 while the key
 *    is down, released to L4), at the rates' speed (rate 99 within a block, faster rates faster); the voice
 *    ends (voice.c engine_t.done) once its carriers have gone silent, not before.
 * 3. retrigger: a note from silence renders the same samples twice (bit-stable); the same key again while it
 *    sounds keeps going without a jump.
 * 4. no DC, no clipping: every factory patch, notes C1..C7 at velocity 127 and 30: no voice beyond 2 x
 *    VOICE_FS, |mean| < 1 % of VOICE_FS.
 * 5. macros: MLVL, MRAT, FB raise the spectral centroid; MEG + keeps it up longer; VMOD + makes velocity count
 *    more; DTUN spreads the carriers (beats); ALG puts another algorithm in; PTCH loads its patch (main loop).
 * 6. formats: pack(unpack(x)) == x for the factory patches (the Python generator and the C packer agree), any
 *    128 bytes unpack into range, a 32-voice SysEx bank and a single-voice SysEx made here parse back
 *    (checksums) into the same patches; the patch survives a project (FUN8) round trip with the same sound.
 * 7. cost: host instructions per sample and voice with six voices held (the heaviest: six carriers), and the
 *    device estimate (1.7 % per 100: the PHYS measurements' ratio, as drum_test); fails above FM6_COST_MAX.
 * 8. demos into DEMODIR: every factory preset playing its suggested pattern. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif

#define FM6_COST_MAX 265.0        /* host instructions per sample and voice (4.5 % on the device at 1.7 % per 100) */

static int bad;
static void check(const char *what, int ok)
{
    printf("fm6: %-90s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* ------------------------------------------------------------ helpers --- */
static uint8_t base[FP_SIZE + 1u];   /* the patch under test (155 bytes) */

static void op_set(uint8_t *v, uint32_t opn, const int *r, const int *l, int ol, int fc)   /* opn 1..6 */
{
    uint8_t *o = v + (6u - opn) * FP_OP;
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        o[FP_R1 + i] = (uint8_t)r[i];
        o[FP_L1 + i] = (uint8_t)l[i];
    }
    o[FP_OL] = (uint8_t)ol;
    o[FP_FC] = (uint8_t)fc;
    o[FP_DET] = 7;
    o[FP_BP] = 39;
}

static void init_patch(uint8_t *v)
{
    fm6_unpack(FM6_INIT, v);
}

/* track 0 = FM6 with patch v and macros e (0: all 0), POLY */
static void setup(const uint8_t *v, const int16_t *e)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    memset(eng_mem, 0, sizeof eng_mem);
    memset(fm6_eff, 0, sizeof fm6_eff);
    memset(fm6_lfo, 0, sizeof fm6_lfo);
    host_tracks_init();
    host_preset(&trk[0], ENGI_FM6, 0);
    for (i = 0; i < 8u; i++)
        trk[0].p[P_E0 + i] = e ? e[i] : 0;
    trk[0].p[P_E7] = (int16_t)(e ? e[7] : 0);
    fm6_set_patch(0, v);
    fm6_slot[0] = (uint8_t)trk[0].p[P_E7];
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
}

/* the voices' sum (track_render) of note at vel for n samples (from the note-on); release after rel samples */
static void voice_render(uint32_t note, uint32_t vel, double *y, uint32_t n, uint32_t rel)
{
    static int32_t b[CTL];
    uint32_t i, k;
    trk_note_on(&trk[0], note, vel);
    for (i = 0; i < n; i += CTL) {
        if (i == (rel / CTL) * CTL && rel)
            trk_note_off(&trk[0], note);
        track_render(&trk[0], b, CTL);
        for (k = 0; k < CTL && i + k < n; k++)
            y[i + k] = b[k];
    }
}

static double rms(const double *y, uint32_t a, uint32_t b)
{
    double s = 0;
    uint32_t i;
    for (i = a; i < b; i++)
        s += y[i] * y[i];
    return sqrt(s / (b - a));
}

/* the spectral centroid (Hz) of y[a .. a + 4096) (Hann) */
#define NFFT 4096
static double centroid(const double *y, uint32_t a)
{
    static double re[NFFT], im[NFFT];
    uint32_t i, j, len;
    double num = 0, den = 0;
    for (i = 0; i < NFFT; i++) {
        re[i] = y[a + i] * (0.5 - 0.5 * cos(2 * M_PI * i / NFFT));
        im[i] = 0;
    }
    for (i = 1, j = 0; i < NFFT; i++) {
        uint32_t bit = NFFT >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (len = 2; len <= NFFT; len <<= 1) {
        double ang = -2 * M_PI / len;
        for (i = 0; i < NFFT; i += len)
            for (j = 0; j < len / 2; j++) {
                double wr = cos(ang * j), wi = sin(ang * j);
                double xr = re[i + j + len / 2] * wr - im[i + j + len / 2] * wi;
                double xi = re[i + j + len / 2] * wi + im[i + j + len / 2] * wr;
                re[i + j + len / 2] = re[i + j] - xr, im[i + j + len / 2] = im[i + j] - xi;
                re[i + j] += xr, im[i + j] += xi;
            }
    }
    for (i = 1; i < NFFT / 2; i++) {
        double p = re[i] * re[i] + im[i] * im[i];
        num += p * i * FS / NFFT;
        den += p;
    }
    return den > 0 ? num / den : 0;
}

/* --------------------------------------------------------- algorithms --- */
static void algorithms(void)
{
    static const uint8_t CARRIERS[32] = {2, 2, 2, 2, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 3, 3,
                                         4, 4, 4, 5, 5, 3, 3, 3, 4, 4, 5, 6};
    static double y[FS / 2];
    uint32_t a, k, counts = 1, sound = 1, grows = 1;
    double lvl[7] = {0};
    int n7[7] = {0};
    for (a = 0; a < 32u; a++) {
        uint32_t c = fm6_carriers(a), n = 0;
        for (k = 0; k < 6u; k++)
            n += (c >> k) & 1u;
        counts &= n == CARRIERS[a];
        init_patch(base);
        for (k = 1; k <= 6u; k++) {
            static const int R[4] = {99, 99, 99, 99}, L[4] = {99, 99, 99, 0};
            op_set(base, k, R, L, 99, 1);
        }
        base[FP_ALG] = (uint8_t)a;
        setup(base, 0);
        voice_render(60, 100, y, FS / 2, 0);
        {
            double r = rms(y, FS / 10, FS / 2);
            sound &= r > 1000;
            lvl[n] += r;
            n7[n]++;
        }
    }
    for (k = 1; k < 6u; k++)          /* (FM is not additive: the mean over the algorithms with that many) */
        if (n7[k] && n7[k + 1])
            grows &= lvl[k + 1] / n7[k + 1] > lvl[k] / n7[k];
    check("algorithms: the classic carrier counts of all 32 (operators on the output bus)", counts);
    check("algorithms: each of the 32 sounds with every operator at full level", sound);
    check("algorithms: more carriers, more level (1 .. 6)", grows);
}

/* ---------------------------------------------------------- envelopes --- */
static void envelopes(void)
{
    static double y[FS * 2];
    static const int R[4] = {60, 50, 40, 55}, L[4] = {99, 70, 50, 0}, Z[4] = {99, 99, 99, 99}, LZ[4] = {0, 0, 0, 0};
    fm6_note_t *n;
    const fm6_env_t *e;
    uint32_t i, stages = 0, ix_prev = 0, ok = 1, t_l1 = 0, t_rel = 0;
    init_patch(base);
    op_set(base, 1, R, L, 99, 1);      /* algorithm 1: OP1 a carrier; OP2..6 silent */
    for (i = 2; i <= 6u; i++)
        op_set(base, i, Z, LZ, 0, 1);
    setup(base, 0);
    trk_note_on(&trk[0], 60, 100);
    n = fm6_note_of(&trk[0], &trk[0].v[0]);
    e = &n->env[5];
    for (i = 0; i < FS * 2u / CTL; i++) {
        static int32_t b[CTL];
        if (i == FS / CTL)
            trk_note_off(&trk[0], 60);
        track_render(&trk[0], b, CTL);
        if (e->ix != ix_prev) {
            ok &= e->ix == ix_prev + 1u;
            stages |= 1u << e->ix;
            if (e->ix == 1u)
                t_l1 = i;
            ix_prev = e->ix;
        }
        if (i < FS / CTL && e->ix == 3u)
            ok &= e->level == e->target || e->down;   /* held at L3 */
        if (!trk[0].v[0].active && !t_rel)
            t_rel = i;
    }
    check("envelope: attack -> L1, decay -> L2 -> L3, held while the key is down, then the release (in order)",
          ok && (stages & 0xEu) == 0xEu);
    check("envelope: the voice ends after the release (engine done), not while the key is held",
          t_rel > FS / CTL && t_rel < FS * 2u / CTL);
    {   /* faster rates are faster: the attack to L1 at rate 60 / 80 / 99 */
        uint32_t r, tl[3];
        static const int RA[3] = {60, 80, 99};
        for (r = 0; r < 3u; r++) {
            int RR[4] = {RA[r], 50, 40, 55};
            init_patch(base);
            op_set(base, 1, RR, L, 99, 1);
            setup(base, 0);
            trk_note_on(&trk[0], 60, 100);
            for (tl[r] = 0; tl[r] < 2000u && fm6_note_of(&trk[0], &trk[0].v[0])->env[5].ix == 0u; tl[r]++) {
                static int32_t b[CTL];
                track_render(&trk[0], b, CTL);
            }
        }
        check("envelope: attack rate 60 slower than 80 slower than 99; 99 within a block or two",
              tl[0] > tl[1] && tl[1] > tl[2] && tl[2] <= 2u);
        printf("fm6: envelope: attack to L1 in %u / %u / %u blocks (rates 60 / 80 / 99), L1 reached at block %u\n",
               tl[0], tl[1], tl[2], t_l1);
    }
    {   /* a percussive patch held: the voice ends once its carriers are silent, the key still down */
        static const int RP[4] = {99, 70, 99, 60}, LP[4] = {99, 0, 0, 0};
        init_patch(base);
        op_set(base, 1, RP, LP, 99, 1);
        setup(base, 0);
        trk_note_on(&trk[0], 60, 100);
        for (i = 0; i < FS * 2u / CTL && trk[0].v[0].active; i++) {
            static int32_t b[CTL];
            track_render(&trk[0], b, CTL);
        }
        check("envelope: a decayed note ends while its key is held (L3 0), its voice free", !trk[0].v[0].active &&
              i < FS * 2u / CTL && trk[0].v[0].gate == 0);
    }
    (void)y;
}

/* --------------------------------------------------------- retrigger --- */
static void retrigger(void)
{
    static double a[FS], b[FS];
    uint32_t i, same = 1;
    double jump = 0;
    fm6_unpack(FM6_FACTORY[0], base);
    setup(base, 0);
    voice_render(60, 100, a, FS, FS / 2);
    setup(base, 0);
    voice_render(60, 100, b, FS, FS / 2);
    for (i = 0; i < FS; i++)
        same &= a[i] == b[i];
    check("retrigger: a note from silence renders the same samples twice", same);
    /* the same key again at 0.3 s while it sounds: no jump beyond the signal's own step */
    {
        static int32_t blk[CTL];
        double prev = 0, maxstep = 0, at = 0;
        uint32_t k;
        fm6_unpack(FM6_FACTORY[6], base);              /* ORGAN: sustained */
        setup(base, 0);
        trk_note_on(&trk[0], 60, 100);
        for (i = 0; i < FS / 2u / CTL; i++) {
            if (i == (FS * 3u / 10u) / CTL)
                trk_note_on(&trk[0], 60, 100);
            track_render(&trk[0], blk, CTL);
            for (k = 0; k < CTL; k++) {
                double d = fabs(blk[k] - prev);
                if (i > 10u && i != (FS * 3u / 10u) / CTL)
                    maxstep = d > maxstep ? d : maxstep;
                else if (i == (FS * 3u / 10u) / CTL)
                    at = d > at ? d : at;
                prev = blk[k];
            }
        }
        jump = at;
        check("retrigger: the same key again while it sounds: no step larger than the tone's own", jump <= maxstep * 1.1);
    }
}

/* ------------------------------------------------------ DC, clipping --- */
static void dc_clip(void)
{
    static double y[FS];
    uint32_t pi, note, v, ok = 1, okdc = 1;
    double worst = 0, wdc = 0;
    for (pi = 0; pi < FM6_NFACTORY; pi++)
        for (note = 24; note <= 96; note += 12)
            for (v = 0; v < 2u; v++) {
                double s = 0, pk = 0;
                uint32_t i;
                fm6_unpack(FM6_FACTORY[pi], base);
                setup(base, 0);
                voice_render(note, v ? 127 : 30, y, FS, FS * 6u / 10u);
                for (i = 0; i < FS; i++) {
                    s += y[i];
                    pk = fabs(y[i]) > pk ? fabs(y[i]) : pk;
                }
                ok &= pk < 2.0 * VOICE_FS;
                okdc &= fabs(s / FS) < 0.01 * VOICE_FS;
                worst = pk > worst ? pk : worst;
                wdc = fabs(s / FS) > wdc ? fabs(s / FS) : wdc;
            }
    printf("fm6: factory patches C1..C7, velocity 30 / 127: peak %.0f (VOICE_FS %d), |mean| at most %.1f\n", worst,
           VOICE_FS, wdc);
    check("no clipping: every factory patch, C1..C7, velocity 30 and 127: a voice below 2 x VOICE_FS", ok);
    check("no DC: |mean| below 1 % of VOICE_FS", okdc);
}

/* ------------------------------------------- amplitude modulation (AMS) --- */
/* DX7: an operator's AMS scales how far the LFO's AMD pulls its level down; AMD 0 (or no LFO yet) = no change at all.
 * Before 0.2.14 the AMS path shifted the wrong way (16384 x too big): every AMS > 0 operator was garbage, AMD 0 included. */
static void ams_patch(uint8_t *v, int ams, int amd, int lfw)
{
    static const int r[4] = {99, 99, 99, 99}, l[4] = {99, 99, 99, 0};
    uint32_t k;
    init_patch(v);
    for (k = 1; k <= 6u; k++)
        op_set(v, k, r, l, k == 1u ? 99 : 0, 1);
    v[FP_ALG] = 31;                                /* algorithm 32: six carriers; only operator 1 sounds */
    v[FP_FB] = 0;
    v[(6u - 1u) * FP_OP + FP_AMS] = (uint8_t)ams;
    v[FP_LFS] = 35;                                /* about 5.8 Hz */
    v[FP_LFD] = 0;
    v[FP_LPMD] = 0;
    v[FP_LPMS] = 0;
    v[FP_LAMD] = (uint8_t)amd;
    v[FP_LFW] = (uint8_t)lfw;
}

static void ams_wave(const uint8_t *v, double *y, uint32_t n)
{
    setup(v, 0);
    voice_render(69, 100, y, n, 0);
}

/* the engine's attenuation of the LFO's low-amplitude half against its other half, in octaves of the Q24 level (6.02 dB each), from
 * the peaks of 400 samples: the operator level L(1 - pt / 2^24) with pt the AMS curve, so the dB difference is 6.02 * L * pt / 2^24 */
static double ams_db(int ams, int amd)
{
    static double y[FS], ref[FS];
    uint8_t v[FP_SIZE + 1u];
    double lo = 1e30, hi = 0;
    uint32_t i, k, w = 400;                        /* about four periods of the note: the peak is independent of the window's phase */
    ams_patch(v, 0, amd, 3);
    ams_wave(v, ref, FS);
    ams_patch(v, ams, amd, 3);                     /* LFO: square, so two steady levels */
    ams_wave(v, y, FS);
    (void)ref;
    for (i = FS / 4u; i + w < FS; i += w) {
        double r = 0;
        for (k = i; k < i + w; k++)
            r = fabs(y[k]) > r ? fabs(y[k]) : r;
        lo = r < lo ? r : lo;
        hi = r > hi ? r : hi;
    }
    return hi > 0 && lo > 0 ? 20.0 * log10(hi / lo) : 1e9;
}

static void ams_tests(void)
{
    static double y0[FS], y1[FS], y2[FS];
    static const char *const flute[3] = {
        "384e2d2b6328160027000000300b2604003c4b2d2d632d1900270000002a132d0c0040482d2f63321c0027000000490f34060044452d3163371f0027000000400b3b021e48422d33633c2200270000003a13420a005f3f2d3563554b0027000000310f5c020063636363323232321f0d37000c633b184b49434b203036202020",
        "384e2d2b6328160027000000480a1e06003c4b2d2d632d190027000000421224020040482d2f63321c0027000000390e2b0a0044452d3163371f0027000000300a32043248422d33633c2200270000002a123902005f3f2d3563554b0027000000490e5c020063636363323232320d0b2d000046071842454c4c203034202020",
        "384e2d2b6328160027000000400b2002003c4b2d2d632d1900270000003a1327020040482d2f63321c0027000000310f2e040044452d3163371f0027000000280b350c3248422d33633c2200270000004a133c06005f3f2d3563554b0027000000410f5c02006363636332323232160f1e000c3233184f5247414e2030382020"};
    static const char *const fname[3] = {"synthetic KICK 06 (AMS 3, AMD 99)", "synthetic BELL 04 (AMS 2, AMD 70)", "synthetic ORGAN 08 (AMS 3, AMD 50)"};
    uint8_t v[FP_SIZE + 1u], pk[128];
    uint32_t i, a, same, f, ok;
    double pk_max = 0;
    /* 1. AMD 0: any AMS, any LFO wave and speed, gives exactly the AMS 0 output (the LFO runs, the depth is 0) */
    for (a = 1, same = 1; a <= 3u; a++) {
        uint32_t w;
        for (w = 0; w < 6u; w++) {
            ams_patch(v, 0, 0, (int)w);
            ams_wave(v, y0, FS / 2u);
            ams_patch(v, (int)a, 0, (int)w);
            ams_wave(v, y1, FS / 2u);
            for (i = 0; i < FS / 2u; i++)
                same &= y0[i] == y1[i];
        }
    }
    check("AMS 1..3 with AMD 0 (every LFO wave): the output is bit for bit the AMS 0 output", same);
    /* the same on a modulator of a stacked algorithm, with an LFO PMD on (the pitch LFO must not leak into the level) */
    ams_patch(v, 0, 0, 4);
    v[FP_ALG] = 0;                                 /* algorithm 1 */
    {
        static const int r[4] = {99, 99, 99, 99}, l[4] = {99, 99, 99, 0};
        op_set(v, 2, r, l, 80, 1);
        op_set(v, 3, r, l, 70, 1);
    }
    ams_wave(v, y0, FS / 2u);
    v[(6u - 2u) * FP_OP + FP_AMS] = 3;
    v[(6u - 3u) * FP_OP + FP_AMS] = 2;
    ams_wave(v, y1, FS / 2u);
    for (i = 0, same = 1; i < FS / 2u; i++)
        same &= y0[i] == y1[i];
    check("AMS on two modulators with AMD 0 (algorithm 1): the output is the AMS 0 output bit for bit", same);
    /* 2. AMD > 0: the depth. With a square LFO the output has two steady levels; the low one is the operator's level L (log2, Q24)
     * pulled down by L * pt / 2^24, so the level difference is 6.02 dB * L * pt / 2^24 with pt = e^(sensamp / 262144 * 0.07 + 12.2)
     * - e^12.2 (the DX7 curve, as msfa's exponential; the constant e^12.2 off so that depth 0 is no attenuation), sensamp = the
     * AMS factor (0, 0.2588, 0.4274, 1) * the LFO amplitude at AMD (AMD * 165 >> 6, of 256). dB / pt must then be the same number
     * for every AMS and AMD (it is 6.02 * L, L about 15 octaves at OL 99) */
    {
        static const double ams_f[4] = {0, 4342338.0 / 16777216.0, 7171437.0 / 16777216.0, 1.0};
        static const int cs[6][2] = {{1, 20}, {1, 60}, {2, 30}, {2, 60}, {3, 20}, {3, 50}};
        double q[6], mean = 0, dev = 0;
        for (i = 0; i < 6u; i++) {
            double sens = ((cs[i][1] * 165) >> 6) / 256.0 * ams_f[cs[i][0]] * 16777216.0;
            double pt = (exp(sens / 262144.0 * 0.07 + 12.2) - exp(12.2)) / 16777216.0;
            q[i] = ams_db(cs[i][0], cs[i][1]) / pt;
            mean += q[i] / 6.0;
        }
        for (i = 0; i < 6u; i++)
            dev = fabs(q[i] / mean - 1.0) > dev ? fabs(q[i] / mean - 1.0) : dev;
        printf("fm6: AMS depth (square LFO) dB per unit of the curve, AMS/AMD 1/20 1/60 2/30 2/60 3/20 3/50: %.1f %.1f %.1f %.1f %.1f %.1f (L = %.2f octaves)\n",
               q[0], q[1], q[2], q[3], q[4], q[5], mean / 6.0206);
        check("AMS depth: dB follows the curve over AMS 1..3 and AMD 20..60 (one factor, within 1 %)", dev < 0.01);
        check("AMS depth: that factor is 6.02 dB x the operator level (14 .. 16 octaves)", mean / 6.0206 > 14.0 && mean / 6.0206 < 16.0);
    }
    /* 3. three synthetic loud voices of our own (web/test_dx7_synth.mjs, algorithms with several carriers, AMS on every operator, an amplitude LFO): no sample beyond int16 */
    for (f = 0, ok = 1; f < 3u; f++) {
        double pkv = 0;
        for (i = 0; i < 128u; i++) {
            unsigned x;
            sscanf(flute[f] + 2u * i, "%2x", &x);
            pk[i] = (uint8_t)x;
        }
        fm6_unpack(pk, v);
        setup(v, 0);
        voice_render(60, 100, y2, FS * 3u / 2u, FS * 3u / 2u);
        for (i = 0; i < FS * 3u / 2u; i++)
            pkv = fabs(y2[i]) > pkv ? fabs(y2[i]) : pkv;
        printf("fm6: voice %s: peak %.0f (int16 limit 32767)\n", fname[f], pkv);
        ok &= pkv <= 32767.0;
        pk_max = pkv > pk_max ? pkv : pk_max;
    }
    check("three synthetic DX7 style voices (AMS + amplitude LFO): no sample beyond int16", ok);
}

/* ---------------------------------------------------------------- macros --- */
static double centroid_of(const uint8_t *v, const int16_t *e, uint32_t vel, uint32_t at)
{
    static double y[FS];
    setup(v, e);
    voice_render(48, vel, y, at + NFFT, 0);
    return centroid(y, at);
}

static void macros(void)
{
    static const int16_t E0[8] = {0};
    int16_t e[8];
    double c0, c1, c2;
    {   /* a plain two-operator voice (algorithm 1: OP2 -> OP1), the others off: the macros' effect is clear */
        static const int RC[4] = {99, 30, 30, 50}, LC[4] = {99, 95, 90, 0}, RM[4] = {99, 45, 30, 50},
                         LM[4] = {99, 80, 60, 0}, Z[4] = {99, 99, 99, 99}, LZ[4] = {0, 0, 0, 0};
        uint32_t k;
        init_patch(base);
        op_set(base, 1, RC, LC, 99, 1);
        op_set(base, 2, RM, LM, 70, 1);
        base[(6u - 2u) * FP_OP + FP_KVS] = 2;
        for (k = 3; k <= 6u; k++)
            op_set(base, k, Z, LZ, 0, 1);
        base[FP_ALG] = 0;
    }
    c0 = centroid_of(base, E0, 100, FS / 10);
    memcpy(e, E0, sizeof e); e[2] = 40;
    c1 = centroid_of(base, e, 100, FS / 10);
    e[2] = -40;
    c2 = centroid_of(base, e, 100, FS / 10);
    printf("fm6: MLVL -40 / 0 / +40: centroid %.0f / %.0f / %.0f Hz\n", c2, c0, c1);
    check("MLVL: + brighter, - darker", c1 > c0 * 1.1 && c2 < c0 * 0.95);
    memcpy(e, E0, sizeof e); e[3] = 3;
    c1 = centroid_of(base, e, 100, FS / 10);
    printf("fm6: MRAT 0 / +3: centroid %.0f / %.0f Hz\n", c0, c1);
    check("MRAT: + raises the modulators' ratios (brighter)", c1 > c0 * 1.1);
    {   /* MEG: slower modulator envelopes keep the brightness later */
        double l0, l1;
        l0 = centroid_of(base, E0, 100, FS / 2);
        memcpy(e, E0, sizeof e); e[4] = 50;
        l1 = centroid_of(base, e, 100, FS / 2);
        printf("fm6: MEG 0 / +50: centroid at 0.5 s %.0f / %.0f Hz\n", l0, l1);
        check("MEG: + the modulators decay slower (brighter later)", l1 > l0 * 1.1);
    }
    {   /* VMOD: velocity changes the brightness more */
        double s0 = centroid_of(base, E0, 127, FS / 10) / centroid_of(base, E0, 40, FS / 10);
        memcpy(e, E0, sizeof e); e[5] = 5;
        c1 = centroid_of(base, e, 127, FS / 10) / centroid_of(base, e, 40, FS / 10);
        printf("fm6: VMOD 0 / +5: centroid ratio vel 127 / 40 %.2f / %.2f\n", s0, c1);
        check("VMOD: + velocity moves the brightness more", c1 > s0 * 1.05);
    }
    {   /* FB on the organ's feedback operator (alg 32: OP6) */
        static const int R[4] = {99, 99, 99, 99}, L[4] = {99, 99, 99, 0}, LZ[4] = {0, 0, 0, 0};
        uint8_t v[FP_SIZE + 1u];
        uint32_t k;
        init_patch(v);                                  /* algorithm 32: OP6 alone, the one with the feedback */
        for (k = 1; k <= 6u; k++)
            op_set(v, k, R, k == 6u ? L : LZ, k == 6u ? 99 : 0, 1);
        v[FP_ALG] = 31;
        c0 = centroid_of(v, E0, 100, FS / 10);
        memcpy(e, E0, sizeof e); e[1] = 6;
        c1 = centroid_of(v, e, 100, FS / 10);
        printf("fm6: FB 0 / +6 (OP6 alone): centroid %.0f / %.0f Hz\n", c0, c1);
        check("FB: + more feedback (brighter)", c1 > c0 * 1.05);
    }
    {   /* ALG: 32 (six carriers) instead of the patch's */
        memcpy(e, E0, sizeof e); e[0] = 32;
        setup(base, e);
        fm6_sync(&trk[0]);
        check("ALG: 1..32 replaces the patch's algorithm, PAT (0) keeps it",
              fm6_eff[0].alg == 31u && (setup(base, E0), fm6_sync(&trk[0]), fm6_eff[0].alg == base[FP_ALG]));
    }
    {   /* DTUN: the carriers apart -> beating: the envelope of the sum varies more */
        static double y[FS];
        double v0, v1;
        uint32_t i, k;
        static const int R[4] = {99, 99, 99, 99}, L[4] = {99, 99, 99, 0};
        uint8_t v[FP_SIZE + 1u];
        double m[2];
        init_patch(v);                                  /* algorithm 32: OP1..OP3 carriers at the same ratio */
        for (k = 1; k <= 6u; k++)
            op_set(v, k, R, L, k <= 3u ? 90 : 0, 1);
        v[FP_ALG] = 31;
        for (k = 0; k < 2u; k++) {
            double lo = 1e18, hi = 0;
            memcpy(e, E0, sizeof e); e[6] = k ? 127 : 0;
            setup(v, e);
            voice_render(48, 100, y, FS, 0);
            for (i = FS / 4; i + 2048 <= FS; i += 2048) {
                double r = rms(y, i, i + 2048);
                lo = r < lo ? r : lo;
                hi = r > hi ? r : hi;
            }
            m[k] = hi / lo;
        }
        v0 = m[0];
        v1 = m[1];
        printf("fm6: DTUN 0 / 127 (3 carriers): level swing over 50 ms windows %.3f / %.3f\n", v0, v1);
        check("DTUN: the carriers apart: they beat", v1 > v0 * 1.05);
    }
    {   /* SLOT: the main loop loads a factory patch; OWN keeps (and brings back) the track's own */
        uint8_t own[FP_SIZE + 1u];
        setup(base, 0);
        memcpy(own, fm6_patch[0], sizeof own);
        trk[0].p[P_E7] = FM6_OWN;
        fm6_poll();
        check("SLOT OWN: the track's patch stays (nothing reloads)", fm6_slot[0] == FM6_OWN &&
              !memcmp(fm6_patch[0], own, FP_SIZE));
        trk[0].p[P_E7] = 4;
        fm6_poll();
        check("SLOT F5 loads the fifth factory patch (main loop)", fm6_slot[0] == 4u &&
              !memcmp(fm6_patch[0] + FP_NAME, "SOFT PAD  ", 10));
        trk[0].p[P_E7] = FM6_OWN;
        fm6_poll();
        check("SLOT back to OWN: the own patch again", fm6_slot[0] == FM6_OWN && !memcmp(fm6_patch[0], own, FP_SIZE));
        check("SLOT: F1..F8, OWN, B1..B32 (1.4.1, the voice bank), then F9..F11 behind the bank (0.2.17), nothing past it",
              ENG_FM6.edit[7].max == FM6_OWN + FM6_NBANK + FM6_NXF && FM6_OWN == FM6_NFACTORY && FM6_NBANK == 32u && FM6_NXF == 3u &&
              !strcmp(ENG_FM6.edit[7].names[FM6_OWN], "OWN") && !strcmp(ENG_FM6.edit[7].names[FM6_OWN + 1], "B1") &&
              !strcmp(ENG_FM6.edit[7].names[FM6_OWN + FM6_NBANK], "B32") && !strcmp(ENG_FM6.edit[7].names[FM6_XBASE], "F9") &&
              !strcmp(ENG_FM6.edit[7].names[FM6_XBASE + 2u], "F11") && !ENG_FM6.edit[7].names[FM6_XBASE + FM6_NXF]);
        {   /* 0.2.17: the factory patches behind the bank load like F1..F8; the stored SLOT values of OWN and the bank are untouched */
            uint8_t v[FP_SIZE + 1u];
            uint32_t x;
            for (x = 0; x < FM6_NXF; x++) {
                trk[0].p[P_E7] = (int16_t)(FM6_XBASE + x);
                fm6_poll();
                fm6_unpack(FM6_XFACTORY[x], v);
                check("SLOT F9.. (behind the bank) loads its own factory patch", fm6_slot[0] == FM6_XBASE + x && !memcmp(fm6_patch[0], v, FP_SIZE));
                check("  and a patch equal to it shows as that slot (project load: fm6_adopt_slot)", fm6_adopt_slot(v, 0) == (int32_t)(FM6_XBASE + x));
            }
            check("  F9 FM BASS 2, F11 FRETLESS (the sounds' voices): the names", !memcmp(FM6_XFACTORY[0] + 118, "FM BASS 2 ", 10));
            check("  a stored SLOT above OWN is OWN (params.c param_fit)", param_fit(&ENG_FM6.edit[7], (int32_t)FM6_XBASE) == (int32_t)FM6_OWN &&
                  param_fit(&ENG_FM6.edit[7], 3) == 3);
            check("  the SLOT knob (no bank here): OWN up goes on to F9, F11 is the end, F9 down is OWN again",
                  param_turn(&ENG_FM6.edit[7], FM6_OWN, 1) == (int32_t)FM6_XBASE && param_turn(&ENG_FM6.edit[7], (int32_t)FM6_XBASE + 2, 1) == (int32_t)FM6_XBASE + 2 &&
                  param_turn(&ENG_FM6.edit[7], (int32_t)FM6_XBASE, -1) == (int32_t)FM6_OWN);
            trk[0].p[P_E7] = FM6_OWN;
            fm6_poll();
        }
        trk[0].p[P_E7] = FM6_OWN + 3;                /* B3 without a bank (no store here): refused, SLOT back */
        fm6_poll();
        check("SLOT B3 with no bank: refused (SLOT back to OWN), the patch unchanged", fm6_slot[0] == FM6_OWN &&
              trk[0].p[P_E7] == FM6_OWN && !memcmp(fm6_patch[0], own, FP_SIZE));
        trk[0].p[P_E7] = FM6_OWN;
        fm6_set_patch(0, own);
        trk[0].p[P_E7] = 2;                          /* a load that says F3 with F3 unchanged: F3; edited: OWN */
        {
            uint8_t v[FP_SIZE + 1u];
            fm6_unpack(FM6_FACTORY[2], v);
            fm6_set_patch(0, v);
            fm6_adopt(0);
            check("fm6_adopt: a factory patch unchanged shows its slot", trk[0].p[P_E7] == 2 && fm6_slot[0] == 2u);
            v[FP_ALG] ^= 1;
            fm6_set_patch(0, v);
            fm6_adopt(0);
            check("fm6_adopt: edited, it is the track's own (OWN)", trk[0].p[P_E7] == FM6_OWN && fm6_slot[0] == FM6_OWN);
            trk[0].p[P_E7] = FM6_NFACTORY + 3;       /* (a stored B4 of 1.0.2) */
            fm6_unpack(FM6_FACTORY[6], v);
            fm6_set_patch(0, v);
            fm6_adopt(0);
            check("fm6_adopt: an old B slot holding factory F7 shows F7", trk[0].p[P_E7] == 6);
        }
    }
}

/* -------------------------------------------------------------- formats --- */
static uint32_t chk(const uint8_t *p, uint32_t n)
{
    uint32_t s = 0, i;
    for (i = 0; i < n; i++)
        s += p[i];
    return (0x80u - (s & 0x7Fu)) & 0x7Fu;
}

static void formats(void)
{
    uint8_t v[FP_SIZE + 1u], pk[FM6_PACKED];
    static uint8_t bank[4104], single[163];
    uint32_t i, k, ok = 1, inrange = 1, seed = 12345;
    for (k = 0; k < FM6_NFACTORY; k++) {
        fm6_unpack(FM6_FACTORY[k], v);
        fm6_pack(v, pk);
        ok &= !memcmp(pk, FM6_FACTORY[k], FM6_PACKED);
    }
    fm6_unpack(FM6_INIT, v);
    fm6_pack(v, pk);
    ok &= !memcmp(pk, FM6_INIT, FM6_PACKED);
    check("pack(unpack(x)) == x: the factory patches and the init voice (generator and C agree)", ok);
    for (k = 0; k < 2000u; k++) {
        for (i = 0; i < FM6_PACKED; i++) {
            seed = seed * 1103515245u + 12345u;
            pk[i] = (uint8_t)(seed >> 16);
        }
        fm6_unpack(pk, v);
        for (i = 0; i < FP_SIZE; i++)
            inrange &= i >= FP_NAME ? (v[i] >= 32u && v[i] <= 126u) : v[i] <= fm6_max(i);
    }
    check("any 128 bytes unpack into range (2000 random records)", inrange);
    /* a 32-voice bank SysEx (the generic 6-operator layout): F0 43 00 09 20 00, 4096 bytes, checksum, F7 */
    bank[0] = 0xF0; bank[1] = 0x43; bank[2] = 0x00; bank[3] = 0x09; bank[4] = 0x20; bank[5] = 0x00;
    for (k = 0; k < 32u; k++)
        memcpy(bank + 6 + k * 128u, k < FM6_NFACTORY ? FM6_FACTORY[k] : FM6_INIT, 128);
    bank[4102] = (uint8_t)chk(bank + 6, 4096);
    bank[4103] = 0xF7;
    ok = (uint32_t)chk(bank + 6, 4096) == bank[4102];
    for (k = 0; k < 32u; k++) {
        fm6_unpack(bank + 6 + k * 128u, v);
        fm6_pack(v, pk);
        ok &= !memcmp(pk, k < FM6_NFACTORY ? FM6_FACTORY[k] : FM6_INIT, 128);
    }
    check("a 32-voice bank SysEx: checksum, its 32 records back as they were", ok);
    /* a single voice: F0 43 00 00 01 1B, the 155 bytes, checksum, F7 */
    fm6_unpack(FM6_FACTORY[3], v);
    single[0] = 0xF0; single[1] = 0x43; single[2] = 0x00; single[3] = 0x00; single[4] = 0x01; single[5] = 0x1B;
    memcpy(single + 6, v, FP_SIZE);
    single[161] = (uint8_t)chk(single + 6, FP_SIZE);
    single[162] = 0xF7;
    memcpy(v, single + 6, FP_SIZE);
    fm6_sanitize(v);
    fm6_pack(v, pk);
    check("a single-voice SysEx (155 bytes): back to the same packed record", !memcmp(pk, FM6_FACTORY[3], 128) &&
          single[161] == chk(single + 6, FP_SIZE));
}

/* --------------------------------------------------------------- voices --- */
static void voices(void)
{
    uint32_t i, n = 0;
    static int32_t b[CTL];
    fm6_unpack(FM6_FACTORY[4], base);
    setup(base, 0);
    for (i = 0; i < 8u; i++)
        trk_note_on(&trk[0], 48 + 2 * i, 100);
    track_render(&trk[0], b, CTL);
    for (i = 0; i < NVOICE; i++)
        n += trk[0].v[i].active;
    check("voices: 8 keys in POLY: 6 voices (the engine's cap)", n == FM6_POLY && ENG_FM6.poly == FM6_POLY);
    for (i = 0; i < 8u; i++)
        trk_note_off(&trk[0], 48 + 2 * i);
    for (i = 0; i < FS * 8u / CTL; i++)
        track_render(&trk[0], b, CTL);
    for (i = 0, n = 0; i < NVOICE; i++)
        n += trk[0].v[i].active;
    check("voices: all free some seconds after the release (the operator envelopes end them)", n == 0);
}

/* ------------------------------------------------------------------ cost --- */
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}

static double mix_cost(const uint8_t *v, uint32_t notes)
{
    static int32_t o[2 * CTL];
    uint32_t i, nb = FS * 2u / CTL;
    uint64_t i0;
    setup(v, 0);
    for (i = 0; i < notes; i++)
        trk_note_on(&trk[0], 48 + 3 * i, 100);
    for (i = 0; i < 64u; i++)
        mix_block(o, CTL);
    i0 = instr_now();
    for (i = 0; i < nb; i++)
        mix_block(o, CTL);
    return (double)(instr_now() - i0) / (nb * CTL);
}

static void cost(void)
{
    uint8_t heavy[FP_SIZE + 1u];
    double idle, c, worst = 0;
    uint32_t pi, k;
    char what[200];
    if (!instr_now()) {
        printf("fm6: cost: no instruction counter on this host (proc_pid_rusage); not measured\n");
        return;
    }
    init_patch(heavy);                                 /* alg 32, six carriers at full, sustained, feedback 7 */
    for (k = 1; k <= 6u; k++) {
        static const int R[4] = {99, 99, 99, 99}, L[4] = {99, 99, 99, 0};
        op_set(heavy, k, R, L, 99, (int)k);
    }
    heavy[FP_ALG] = 31;
    heavy[FP_FB] = 7;
    idle = mix_cost(heavy, 0);
    printf("fm6: cost, host instructions per sample and voice (6 voices held; device estimate 1.7 %% per 100):\n");
    for (pi = 0; pi <= FM6_NFACTORY; pi++) {
        uint8_t v[FP_SIZE + 1u];
        if (pi < FM6_NFACTORY)
            fm6_unpack(FM6_FACTORY[pi], v);
        c = (mix_cost(pi < FM6_NFACTORY ? v : heavy, 6) - idle) / 6;
        printf("fm6:   %-12s %5.0f  ~%.2f %%\n", pi < FM6_NFACTORY ? ENG_FM6.presets[pi].name : "(6 carriers)", c, c * 0.017);
        worst = c > worst ? c : worst;
    }
    snprintf(what, sizeof what, "cost: at most %.0f instructions a sample and voice (~%.1f %% on the device, 6 voices ~%.0f %%; limit %.0f)",
             worst, worst * 0.017, worst * 0.017 * 6, FM6_COST_MAX);
    check(what, worst <= FM6_COST_MAX);
}

/* ----------------------------------------------------------------- demos --- */
static void demo(const char *dir, uint32_t pi)
{
    static int32_t o[2 * CTL];
    const preset_t *pr = &ENG_FM6.presets[pi];
    uint32_t pat = pr->pat ? pr->pat - 1u : 4u, step = FS / 8u, s, i, held = 0, frames = 0;
    char path[512], nm[32];
    FILE *w;
    for (i = 0; pr->name[i] && i < 31u; i++)
        nm[i] = pr->name[i] == ' ' ? '_' : pr->name[i];
    nm[i] = 0;
    snprintf(path, sizeof path, "%s/%02u_%s.wav", dir, pi, nm);
    if (!(w = fopen(path, "wb")))
        return;
    wav_hdr(w, 0);
    memset(trk, 0, sizeof trk);
    memset(eng_mem, 0, sizeof eng_mem);
    host_tracks_init();
    host_preset(&trk[0], ENGI_FM6, pi);
    for (s = 0; s < 48u; s++) {                       /* three times through the pattern's 16 steps, 120 BPM 1/16 */
        uint32_t k = s % 16u, note = PATTERNS[pat].note[k], fl = PATTERNS[pat].flags[k];
        if (!(fl & 4u) && held) {
            trk_note_off(&trk[0], held);
            held = 0;
        }
        if (note && !(fl & 4u)) {
            trk_note_on(&trk[0], note, fl & 1u ? 120 : 90);
            held = note;
        }
        for (i = 0; i < step; i += CTL) {
            uint32_t j;
            mix_block(o, CTL);
            for (j = 0; j < CTL; j++)
                wav_put(w, o[2 * j], o[2 * j + 1]);
            frames += CTL;
        }
    }
    if (held)
        trk_note_off(&trk[0], held);
    for (i = 0; i < FS * 2u; i += CTL) {
        uint32_t j;
        mix_block(o, CTL);
        for (j = 0; j < CTL; j++)
            wav_put(w, o[2 * j], o[2 * j + 1]);
        frames += CTL;
    }
    fseek(w, 0, SEEK_SET);
    wav_hdr(w, frames);
    fclose(w);
}

int main(int argc, char **argv)
{
    uint32_t pi;
    algorithms();
    envelopes();
    retrigger();
    dc_clip();
    ams_tests();
    macros();
    formats();
    voices();
    if (!getenv("NOCOST"))
        cost();
    if (argc > 1) {
        for (pi = 0; pi < FM6_NFACTORY; pi++)
            demo(argv[1], pi);
        printf("fm6: demos in %s: the %u presets, each playing its suggested pattern\n", argv[1], (uint32_t)FM6_NFACTORY);
    }
    printf("fm6_test: %s\n", bad ? "FAILED" : "all checks ok");
    return bad != 0;
}
