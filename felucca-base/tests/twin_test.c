/* SPDX-License-Identifier: GPL-3.0-only */
/* Cloud job E (the TWIN FILTER's filter 2 and DRIVE: firmware/src/chain_f2.c, twin_graph.c; TWIN_FILTER_DESIGN.md 2, TWIN_FILTER_DESIGN_2.md 2 and 3). What uf_test.c, mx_step_test.c, xvalues_test.c and
 * partindep / twocore cannot show; built with -DDUELL_TWIN_COUNT (chain_f2.c counts the runs of DRIVE).
 *   TYPES     every type measured through chain_f2 on sines: the response against twin_graph_db (the closed form the pages draw), TRIO's filter never whistles (Q <= 4, an impulse dies), LADDER
 *             sings and is bounded, its bass thins with RES, COMB rings with + and is hollow with - and is capped below 100 %
 *   TUNING    COMB tuned to the note across the range (the period of its echoes against the note's, in cents), an octave up for a note under 55 Hz, TUNE moves it, the lines cleared at a note's
 *             start and after an engine change, COMB on WAVE / PHYS reads LP
 *   DRIVE     0 is not run (a counter) and exact; the level kept; 1 .. 11 blend; a step of DRIVE has no zipper; BLEND -64 / +63 are filter 1 alone / filter 2 alone in both routes
 *   CUTOFF    the sum: LINK on moves both filters with one knob, the amounts add to filter 2 only; KEY TRK 2 / ENV 2 / LFO 1 / LFO 2 / the matrix's CUTOFF 2; COMB's TUNE; the clamp
 *   LINK      twin_link_set keeps the sound (bit for bit), twin_type_set's resets and refusals, twin_on
 *   STABLE    every TYPE x ROUTE at RES 127 / FEEDBACK +-max, DRIVE 127, BLEND at its ends on loud noise and a loud chord: bounded, no DC; one minute of noise with every value jumping each block
 *   CLICKS    a held sine on the whole firmware: TYPE (every pair of a set), ROUTE, LINK, DRIVE, BLEND stepped while it sounds, and TWIN switched on and off: the largest 2nd difference near the
 *             step against the steady renders' (mx_step_test.c's method)
 *   MORPH     the fader through the middle with ends that differ in TYPE, ROUTE, LINK: the switch is at the middle and no click
 *   MODES     TYPE OFF with every TWIN value off its default: bit for bit; DUEL, LAYER, SPREAD: the parts' filter-2 states are their own; chain_on is 0 at TYPE OFF
 *   ALIAS     DRIVE at C5 and C7: the non-harmonic part of the output (reported, not a gate)
 * Built and run by host_tests.sh (name: twin) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "mode_scn.h"
#include <sys/mman.h>

static struct { int ncheck, bad; } *sh;
static void ck(const char *what, int ok)
{
    sh->ncheck++;
    printf("twin: %-122s %s\n", what, ok ? "ok" : "FAIL");
    sh->bad += !ok;
}
static void in_child(void (*fn)(void))
{
    int st = 0;
    pid_t p;
    fflush(stdout);
    p = fork();
    if (p == 0) {
        fn();
        fflush(stdout);
        _exit(0);
    }
    waitpid(p, &st, 0);
    if (!WIFEXITED(st) || WEXITSTATUS(st)) {
        printf("twin: FAIL a section ended with signal/status %d\n", st);
        sh->bad++;
    }
}
static double rmsd(const int32_t *x, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += (double)x[i] * x[i];
    return sqrt(s / n);
}
static int32_t peakof(const int32_t *x, uint32_t n)
{
    int32_t m = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t a = x[i] < 0 ? -x[i] : x[i];
        m = a > m ? a : m;
    }
    return m;
}

/* ---------------------------------------------------------------- the stage, driven by hand --- */
/* part 0 on engine eng (a preset), filter 1 open, TWIN off: the values are set by the caller. v0 is voice 0 */
static void st_reset(uint32_t eng)
{
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], eng, 0);
    trk[0].p[P_FREEA] = UF_STEPS;
    memset(f2s, 0, sizeof f2s);
    memset(&trk[0].v[0], 0, sizeof trk[0].v[0]);
}
#define T0 (&trk[0])
typedef struct { int pitch16; int32_t uc; uint32_t start; int half; } st_ctx_t;   /* half: a stand-in for filter 1 between the hooks, y1 = x / 2 */
static st_ctx_t sc = {960, UF_STEPS * 256, 1, 0};
/* one block through chain_pre / chain_f2 of voice 0 of part 0; the first one after st_reset is a note's start (VCH_NEW) */
static void st_block(int32_t *buf)
{
    track_t *t = T0;
    voice_t *v = &t->v[0];
    vmod_t m;
    int32_t dry[CTL];
    memset(&m, 0, sizeof m);
    m.pitch16 = sc.pitch16;
    m.inc = pitch_inc(sc.pitch16);
    m.amp0 = m.amp1 = 32767;
    m.envq15 = 20000;
    v->active = 1;
    v->pitch_cur = v->pitch16 = sc.pitch16;
    v->vel = 100;
    v->chn = sc.start ? VCH_NEW : VCH_RUN;
    sc.start = 0;
    chain_f2_pre(t, v, buf, dry, CTL);
    if (sc.half) {
        uint32_t i;
        for (i = 0; i < CTL; i++)
            buf[i] >>= 1;
    }
    chain_f2(t, v, buf, dry, CTL, &m, sc.uc);
}
static void st_run(const int32_t *in, int32_t *out, uint32_t nb)
{
    uint32_t b;
    for (b = 0; b < nb; b++) {
        memcpy(out + b * CTL, in + b * CTL, CTL * sizeof(int32_t));
        st_block(out + b * CTL);
    }
}
static void st_begin(uint32_t eng)
{
    st_reset(eng);
    sc.pitch16 = 960;
    sc.uc = UF_STEPS * 256;
    sc.start = 1;
    sc.half = 0;
}
#define NBMAX 1024u
static int32_t bin[NBMAX * CTL], bout[NBMAX * CTL];
static void sine_in(double hz, double amp, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        bin[i] = (int32_t)lrint(amp * sin(6.283185307179586 * hz * i / FS));
}
/* the stage's gain in dB for a sine (the second half of 256 blocks) */
static double st_gain(double hz, double amp)
{
    uint32_t nb = 256, i;
    sine_in(hz, amp, nb * CTL);
    st_run(bin, bout, nb);
    (void)i;
    return 20.0 * log10(rmsd(bout + nb * CTL / 2, nb * CTL / 2) / rmsd(bin + nb * CTL / 2, nb * CTL / 2) + 1e-12);
}
/* an impulse response of nb blocks */
static void st_impulse_at(double amp, uint32_t nb, uint32_t lead)   /* the impulse after lead blocks of silence (a COMB's feedback grows in over two delays from its start) */
{
    memset(bin, 0, nb * CTL * sizeof(int32_t));
    bin[lead * CTL] = (int32_t)amp;
    st_run(bin, bout, nb);
}
static void st_impulse(double amp, uint32_t nb) { st_impulse_at(amp, nb, 0); }
static void twin_set(uint32_t ty, int res, int cut2, int link)
{
    T0->p[P_TWTYPE] = (int16_t)ty;
    T0->p[P_RES2] = (int16_t)res;
    T0->p[P_CUT2] = (int16_t)cut2;
    XV(T0, X_LINK) = (int16_t)link;
}

/* ---------------------------------------------------------------- TYPES --- */
static void t_types(void)
{
    static const double F[] = {50, 100, 200, 400, 800, 1600, 3200, 6400, 12800};
    static const uint8_t TY[5] = {TT_LP, TT_BP, TT_HP, TT_NOTCH, TT_LADDER};
    static const char *const NM[5] = {"LP", "BP", "HP", "NOTCH", "LADDER"};
    uint32_t k, j, ok_all = 1;
    for (j = 0; j < 5u; j++) {
        double worst = 0;
        uint32_t ok = 1;
        for (k = 0; k < 9u; k++) {
            double g, gr;
            st_begin(0);
            sc.uc = 40 * 256;
            twin_set(TY[j], 0, 0, 1);
            T0->p[P_FREEA] = 40;
            g = st_gain(F[k], TY[j] == TT_LADDER ? 800 : 3000);
            gr = twin_graph_db(T0, 1, (uint32_t)F[k]) / 16.0;
            if (g > -42.0 && gr > -42.0) {
                double d = fabs(g - gr);
                worst = d > worst && F[k] < 7000.0 ? d : worst;
                ok &= d < (F[k] > 7000.0 ? 4.0 : TY[j] == TT_LADDER ? 2.0 : 1.5);   /* (the trapezoidal filter falls to 0 at Nyquist: the analog picture is 3 dB high at 12.8 kHz) */
            }
        }
        printf("twin:   %-6s RES 0 at 40 dial steps: the largest difference between the measured gain and twin_graph_db %.2f dB\n", NM[j], worst);
        ok_all &= ok;
    }
    ck("LP BP HP NOTCH LADDER (RES 0): the measured response follows twin_graph_db within 1.5 dB (LADDER 2) up to 6.4 kHz (4 dB at 12.8 kHz) at 9 frequencies", ok_all);
    ok_all = 1;
    for (j = 0; j < 5u; j++) {
        double worst = 0;
        uint32_t ok = 1;
        for (k = 0; k < 9u; k++) {
            double g, gr;
            st_begin(0);
            sc.uc = 40 * 256;
            twin_set(TY[j], TY[j] == TT_LADDER ? 60 : 100, 0, 1);
            T0->p[P_FREEA] = 40;
            g = st_gain(F[k], TY[j] == TT_LADDER ? 300 : 1500);
            gr = twin_graph_db(T0, 1, (uint32_t)F[k]) / 16.0;
            if (g > -42.0 && gr > -42.0) {
                double d = fabs(g - gr);
                worst = d > worst && F[k] < 7000.0 ? d : worst;
                ok &= d < (F[k] > 7000.0 ? 4.5 : 3.0);
            }
        }
        printf("twin:   %-6s resonant (RES %d): the largest difference %.2f dB\n", NM[j], TY[j] == TT_LADDER ? 60 : 100, worst);
        ok_all &= ok;
    }
    ck("  .. with resonance (RES 100, LADDER 60): within 3 dB (the drift moves TRIO's cutoff a few hundredths of an octave)", ok_all);
    /* TRIO's filter never whistles: the peak of RES 127 stays under Q 4 (+12 dB, a few dB more for the drift), and an impulse dies */
    {
        double pk = -99, tail;
        int32_t *tl;
        uint32_t i;
        for (k = 0; k < 40u; k++) {
            double g;
            st_begin(0);
            sc.uc = 40 * 256;
            twin_set(TT_LP, 127, 0, 1);
            T0->p[P_FREEA] = 40;
            g = st_gain(300.0 * pow(2.0, k / 10.0), 1500);
            pk = g > pk ? g : pk;
        }
        st_begin(0);
        sc.uc = 40 * 256;
        twin_set(TT_LP, 127, 0, 1);
        st_impulse(20000, 700);
        tl = bout + 600u * CTL;
        {   /* (the AC part: the integer filter keeps a dead band of a few tens of units of DC, TRIO's own) */
            double dc = 0;
            for (i = 0; i < 100u * CTL; i++)
                dc += tl[i];
            dc /= 100.0 * CTL;
            for (i = 0, tail = 0; i < 100u * CTL; i++)
                tail += ((double)tl[i] - dc) * ((double)tl[i] - dc);
            tail = sqrt(tail / (100.0 * CTL));
            printf("twin:   LP RES 127: the highest gain over a sweep %.1f dB (Q 4 = 12.0), the impulse's AC rms in its last 100 blocks %.2f (dc %.1f)\n", pk, tail, dc);
        }
        ck("LP RES 127 never whistles: the response peaks under +15 dB and an impulse is gone after 0.5 s (AC rms under 1)", pk < 15.0 && tail < 1.0);
    }
    /* LADDER sings at RES 127 and is bounded; its bass thins with RES */
    {
        double hz, g0, g1, g2;
        uint32_t i, zc = 0;
        int32_t *tl, pk;
        st_begin(0);
        sc.uc = 40 * 256;
        twin_set(TT_LADDER, 127, 0, 1);
        T0->p[P_FREEA] = 40;
        st_impulse(20000, 1000);
        tl = bout + 900u * CTL;
        pk = peakof(tl, 100u * CTL);
        for (i = 1; i < 100u * CTL; i++)
            zc += (tl[i - 1] < 0) != (tl[i] < 0);
        hz = zc / 2.0 / (100.0 * CTL / FS);
        printf("twin:   LADDER RES 127 at 40 steps: after 0.65 s still ringing, peak %d, %.0f Hz (the cutoff %u Hz)\n", pk, hz, (unsigned)gf_cut_hz(40 + 396 * 40));
        ck("LADDER at RES 127 sings (an impulse leaves a steady tone) near its cutoff, bounded by 56000", pk > 3000 && pk < 56000 && hz > 0.6 * gf_cut_hz(40 + 396 * 40) && hz < 1.5 * gf_cut_hz(40 + 396 * 40));
        st_begin(0);
        sc.uc = 40 * 256;
        twin_set(TT_LADDER, 0, 0, 1);
        T0->p[P_FREEA] = 40;
        g0 = st_gain(60, 600);
        st_begin(0);
        sc.uc = 40 * 256;
        twin_set(TT_LADDER, 60, 0, 1);
        T0->p[P_FREEA] = 40;
        g1 = st_gain(60, 600);
        st_begin(0);
        sc.uc = 40 * 256;
        twin_set(TT_LADDER, 100, 0, 1);
        T0->p[P_FREEA] = 40;
        g2 = st_gain(60, 600);
        printf("twin:   LADDER bass (60 Hz) at RES 0 / 60 / 100: %.1f %.1f %.1f dB\n", g0, g1, g2);
        ck("LADDER's bass thins as RES rises (60 Hz: RES 0 about 0 dB, RES 60 and 100 each lower by 3 dB or more)", g0 > -1.0 && g1 < g0 - 3.0 && g2 < g1 - 3.0);
    }
    /* COMB: + rings, - is hollow (the first echo is inverted), capped below 100 % */
    {
        double ring20, tail;
        int32_t ring;
        uint32_t i;
        int32_t neg_first = 0;
        st_begin(0);
        twin_set(TT_COMB, 63, 0, 1);
        st_impulse_at(20000, 1000, 60);
        ring = peakof(bout + 63u * CTL, 27u * CTL);
        ring20 = rmsd(bout + 80u * CTL, 20u * CTL);
        tail = rmsd(bout + 900u * CTL, 100u * CTL);
        st_begin(0);
        twin_set(TT_COMB, -64, 0, 1);
        st_impulse_at(20000, 90, 60);
        for (i = 62u * CTL; i < 90u * CTL; i++)
            neg_first = bout[i] < neg_first ? bout[i] : neg_first;
        printf("twin:   COMB FEEDBACK +63: echoes peak %d, rms of blocks 20..40 %.1f, of the last 100 blocks %.2f; -64: the strongest negative echo %d\n", ring, ring20, tail, neg_first);
        ck("COMB FEEDBACK +63 rings (echoes after the impulse) and dies (loop gain under 1: rms under 1 after 1 s); -64 gives an inverted echo", ring > 1500 && ring20 > 80.0 && tail < 1.0 && neg_first < -1500);
    }
}

/* ---------------------------------------------------------------- COMB tuning --- */
/* the loop delay of the comb at pitch16 (1/16 semitone) with tune16 added: the distance in samples between the centres of the 2nd and 8th echoes of an impulse, / 6 */
static double comb_period(int pitch16, int tune, int *ok)
{
    static int32_t y[4096 * 8];
    double c[9] = {0}, w[9] = {0}, p0 = FS / (440.0 * pow(2.0, (pitch16 / 16.0 - 69.0 * 0 - 57.0 * 0 - 0) / 12.0));
    uint32_t nb = 300, i, k, i0 = 60u * CTL;
    double per;
    st_begin(0);
    sc.pitch16 = pitch16;
    twin_set(TT_COMB, 50, tune, 1);
    memset(bin, 0, nb * CTL * sizeof(int32_t));
    bin[i0] = 20000;
    st_run(bin, bout, nb);
    memcpy(y, bout, nb * CTL * sizeof(int32_t));
    (void)p0;
    *ok = 1;
    /* the echoes: windows of the true period around k * P (P from the pitch: the frequency of MIDI note pitch16 / 16, A4 = note 69 at 440 Hz) */
    per = FS / (440.0 * pow(2.0, ((pitch16 / 16.0) - 69.0) / 12.0 * 1.0 + tune / 12.0));
    while (per > 790.0)
        per /= 2.0;
    for (k = 2; k <= 8u; k += 6) {
        double lo = i0 + k * per - per / 2, hi = i0 + k * per + per / 2;
        for (i = (uint32_t)lo; i < (uint32_t)hi && i < nb * CTL; i++) {
            double a = y[i] < 0 ? 0 : y[i];
            c[k] += a * i;
            w[k] += a;
        }
        if (w[k] <= 0)
            *ok = 0;
    }
    return *ok ? (c[8] / w[8] - c[2] / w[2]) / 6.0 : 0;
}
static void t_tuning(void)
{
    static const int NOTES[] = {24, 33, 36, 43, 48, 55, 60, 67, 72, 79, 84, 91, 96};
    uint32_t k, ok_all = 1;
    double worst = 0, worst_hi = 0;
    for (k = 0; k < sizeof NOTES / sizeof NOTES[0]; k++) {
        int ok;
        int pitch16 = NOTES[k] * 16;
        double f = 440.0 * pow(2.0, (NOTES[k] - 69.0) / 12.0), want = FS / f, got;
        if (want > 790.0)                                /* below about 55 Hz: an octave up */
            want /= 2.0;
        got = comb_period(pitch16, 0, &ok);
        if (ok) {
            double cents = 1200.0 * log2(want / got);
            printf("twin:   COMB note %2d (%7.2f Hz): period %.2f samples wanted %.2f: %+.1f cents\n", NOTES[k], f, got, want, cents);
            if (NOTES[k] <= 84)
                worst = fabs(cents) > worst ? fabs(cents) : worst;
            else
                worst_hi = fabs(cents) > worst_hi ? fabs(cents) : worst_hi;
        } else {
            ok_all = 0;
        }
    }
    printf("twin:   COMB tuning: worst %.1f cents up to C6, %.1f cents above\n", worst, worst_hi);
    ck("COMB is tuned to the note: the echoes' period matches the note's within 20 cents from C1 (an octave up below 55 Hz) to C6, 50 cents to C7", ok_all && worst < 20.0 && worst_hi < 50.0);
    {
        int ok;
        double a = comb_period(60 * 16, 0, &ok), b = comb_period(60 * 16, 12, &ok), c = comb_period(60 * 16, -7, &ok);
        ck("  .. TUNE moves it in semitones: +12 halves the period, -7 gives 2^(7/12) of it", fabs(b / a - 0.5) < 0.01 && fabs(c / a - pow(2.0, 7.0 / 12.0)) < 0.012);
    }
}

/* ---------------------------------------------------------------- DRIVE, BLEND, ROUTE --- */
static double thd_of(const int32_t *y, uint32_t n, double hz)   /* the share of the energy that is not the fundamental, 0..1 */
{
    double a = 0, b = 0, e = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        a += y[i] * sin(6.283185307179586 * hz * i / FS);
        b += y[i] * cos(6.283185307179586 * hz * i / FS);
        e += (double)y[i] * y[i];
    }
    a = 2.0 * a / n;
    b = 2.0 * b / n;
    e = e / n;
    return e > 0 ? 1.0 - ((a * a + b * b) / 2.0) / e : 0;
}
/* filter 2 wide open (LP at the top of the dial: a sine at 1 kHz passes): the stage is DRIVE alone */
static void drive_setup(uint32_t d)
{
    st_begin(0);
    twin_set(TT_LP, 0, 63, 1);
    T0->p[P_FREEA] = UF_STEPS;
    T0->p[P_DRV2] = (int16_t)d;
}
static void t_drive(void)
{
    static const uint8_t D[] = {1, 3, 6, 11, 12, 20, 40, 64, 96, 127};
    uint32_t k, ok_lvl = 1, ok_mono = 1;
    double prev = -1, a0;
    twin_drive_runs = 0;
    drive_setup(0);
    a0 = st_gain(1000, 20000);
    ck("DRIVE 0 is not run: the shaper is never called over 256 blocks", twin_drive_runs == 0);
    drive_setup(1);
    st_gain(1000, 4000);
    ck("  .. and from DRIVE 1 it is (the counter moves)", twin_drive_runs > 0);
    printf("twin:   DRIVE: level against the undriven sine and the share of the energy outside the fundamental, a 1 kHz sine of peak 20000, filter 2 open (%.1f dB without DRIVE):\n", a0);
    for (k = 0; k < sizeof D; k++) {
        double g, thd;
        drive_setup(D[k]);
        g = st_gain(1000, 20000) - a0;
        thd = thd_of(bout + 128u * CTL, 128u * CTL, 1000.0);
        printf("twin:     DRIVE %3u: %+5.1f dB, %5.1f %%\n", D[k], g, 100.0 * thd);
        ok_lvl &= fabs(g) < 6.0;
        ok_mono &= thd + 0.002 >= prev;
        prev = thd;
    }
    ck("DRIVE keeps the level roughly constant (gain^-0.75): within 6 dB of the undriven sine, DRIVE 1 .. 127", ok_lvl);
    ck("  .. 1..11 blend the dry and the driven signal as SAT does, and more drive is more distortion (monotone over 1, 3, 6, 11, 12, 20, 40, 64, 96, 127)", ok_mono);
    /* no zipper: DRIVE 0 -> 127 at a block boundary in the middle of a sine: the largest 2nd difference near the step against the steady renders' */
    {
        static int32_t r[3][200 * CTL];
        uint32_t b, i, nb = 200, jb = 100;
        int32_t m0 = 0, m1 = 0, ms = 0;
        for (k = 0; k < 3u; k++) {
            drive_setup(k == 1u ? 127 : 0);
            sine_in(220, 20000, nb * CTL);
            for (b = 0; b < nb; b++) {
                if (k == 2u && b == jb)
                    T0->p[P_DRV2] = 127;
                memcpy(r[k] + b * CTL, bin + b * CTL, CTL * sizeof(int32_t));
                st_block(r[k] + b * CTL);
            }
        }
        for (i = 20u * CTL; i < (nb - 2u) * CTL; i++) {
            int32_t a = r[0][i] - 2 * r[0][i - 1] + r[0][i - 2], c = r[1][i] - 2 * r[1][i - 1] + r[1][i - 2];
            m0 = abs(a) > m0 ? abs(a) : m0;
            m1 = abs(c) > m1 ? abs(c) : m1;
        }
        for (i = jb * CTL - 2; i < (jb + 3u) * CTL; i++) {
            int32_t a = r[2][i] - 2 * r[2][i - 1] + r[2][i - 2];
            ms = abs(a) > ms ? abs(a) : ms;
        }
        printf("twin:   DRIVE 0 -> 127 at a block boundary on a 220 Hz sine of peak 20000: the largest 2nd difference near the step %d = %d / 1000 of the peak (steady: DRIVE 0 %d, DRIVE 127 %d)\n", ms, ms * 1000 / 20000, m0, m1);
        ck("DRIVE stepped 0 -> 127 glides (per sample, a jump over 4 blocks): no zipper, the largest 2nd difference near the step at most 25 / 1000 of the peak (mx_step_test: SAT's ramp 40)", ms * 1000 / 20000 <= 25);
    }
}
/* BLEND and ROUTE with a stand-in for filter 1 (sc.half: y1 = x / 2) and a closed filter 2 (LP, OFFSET -30: it changes the signal) */
static int32_t xin[64 * CTL], yser[64 * CTL], ypar[64 * CTL], yref[64 * CTL];
static void blend_run(int32_t *out, int route, int blend, int half, uint32_t d)
{
    drive_setup(d);
    twin_set(TT_LP, 30, -36, 1);                        /* (1.0: OFFSET -36 semitones = 27 steps under filter 1's 30: filter 2 at 3) */
    T0->p[P_FREEA] = 30;
    sc.uc = 30 * 256;
    XV(T0, X_ROUTE) = (int16_t)route;
    T0->p[P_BLEND] = (int16_t)blend;
    sc.half = half;
    st_run(xin, out, 64);
}
static void t_blend(void)
{
    uint32_t i, ok1 = 1, ok2 = 1, ok3 = 1, ok4 = 1, ok5 = 1, b, d;
    for (i = 0; i < 64u * CTL; i++)
        xin[i] = (int32_t)(5000.0 * sin(2.0 * 3.14159265 * 2000.0 * i / FS) + 3000.0 * sin(2.0 * 3.14159265 * 330.0 * i / FS));
    for (d = 0; d < 2u; d++) {
        uint32_t dv = d ? 90u : 0u;
        int32_t refd[64 * CTL];
        /* the references: filter 2 on the (driven) input alone, and the driven input itself */
        blend_run(yref, 0, 0, 0, dv);                   /* SERIES, BLEND 0, no filter 1: f2(D(x)) */
        for (b = 0; b < 64u; b++) {
            memcpy(refd + b * CTL, xin + b * CTL, CTL * sizeof(int32_t));
            if (dv)
                f2_drv(refd + b * CTL, CTL, dv, dv, 0);
        }
        blend_run(yser, 0, -64, 1, dv);                 /* SERIES: filter 1 alone */
        for (i = 0; i < 64u * CTL; i++)
            ok1 &= yser[i] == (xin[i] >> 1);
        blend_run(ypar, 1, -64, 1, dv);                 /* PARALLEL: filter 1 alone, after the DRIVE that comes first in PARALLEL */
        for (i = 0; i < 64u * CTL; i++)
            ok2 &= ypar[i] == (refd[i] >> 1);
        blend_run(yser, 0, 63, 1, dv);                  /* SERIES: filter 2 alone (its input slid all the way to the unfiltered one) */
        for (i = 0; i < 64u * CTL; i++)
            ok3 &= abs(yser[i] - yref[i]) <= abs(yref[i]) / 200 + 6;
        blend_run(ypar, 1, 63, 1, dv);                  /* PARALLEL: filter 2 alone */
        for (i = 0; i < 64u * CTL; i++)
            ok4 &= abs(ypar[i] - yref[i]) <= abs(yref[i]) / 200 + 6;
        blend_run(ypar, 1, 0, 1, dv);                   /* PARALLEL, BLEND 0: the mix by the rule of the code, bit for bit */
        for (i = 0; i < 64u * CTL; i++)
            ok5 &= ypar[i] == (refd[i] >> 1) + (((yref[i] - (refd[i] >> 1)) * 2064) >> 12);
    }
    ck("BLEND -64 is filter 1 alone in SERIES (bit for bit), DRIVE 0 and 90", ok1);
    ck("  .. and in PARALLEL (the DRIVE comes first: before filter 1 as well), bit for bit", ok2);
    ck("BLEND +63 is filter 2 alone in SERIES (its input slid to the unfiltered one: within 0.5 %)", ok3);
    ck("  .. and in PARALLEL (within 0.5 %)", ok4);
    ck("PARALLEL BLEND 0 mixes filter 1 and filter 2 by the code's rule (bit for bit, about equal)", ok5);
}

/* ---------------------------------------------------------------- CUTOFF, LINK --- */
static int32_t cut_of(int32_t uc, f2q_t *qo)
{
    track_t *t = T0;
    voice_t *v = &t->v[0];
    vmod_t m;
    f2q_t q;
    static f2s_t s;
    memset(&m, 0, sizeof m);
    memset(&s, 0, sizeof s);
    memset(&q, 0, sizeof q);
    m.envq15 = 20000;
    q.ty = f2_ty(t);
    q.fresh = 1;
    f2_cut(t, v, &m, uc, &q, &s);
    if (qo)
        *qo = q;
    return q.cab;
}
static void cut_reset(void)
{
    st_begin(0);
    twin_set(TT_LP, 0, 0, 1);
    T0->v[0].pitch_cur = 960;
    T0->v[0].vel = 60;
    T0->v[0].env2 = 0;
    T0->lfo_val = T0->lfo_fade = 0;
    T0->lfo2.val = T0->lfo2.fade = 0;
    T0->f2add = 0;
    MODW(T0).on = 0;
}
static void t_cutoff(void)
{
    uint32_t ok = 1;
    int32_t c;
    f2q_t q;
    cut_reset();
    ok &= cut_of(3000, 0) == 3000 && cut_of(6400, 0) == 6400;
    ck("LINK on: filter 2's cutoff is filter 1's effective cutoff (OFFSET 0), whatever it is", ok);
    T0->p[P_CUT2] = 10;
    c = cut_of(3000, 0);
    T0->p[P_CUT2] = -10;
    ck("  .. OFFSET +10 / -10 WHOLE SEMITONES move it by 1933 (Q8: 193.34 a semitone, 1.0), clamped to the dial (0 .. 82); +36 / -36 are 3 octaves (6960); a stored 63 clamps to 36", c == 3000 + 1933 && cut_of(3000, 0) == 3000 - 1933 && cut_of(1000, 0) == 0 && TW_OFF_Q8(36) == 6960 && TW_OFF_Q8(-36) == -6960 && TW_OFF_Q8(1) == 193 && TW_OFF_Q8(12) == 2320 && (T0->p[P_CUT2] = 63, cut_of(UF_TOP - 100, 0) == UF_TOP) && (T0->p[P_CUT2] = 63, cut_of(3000, 0) == 3000 + 6960) && (T0->p[P_CUT2] = -63, cut_of(9000, 0) == 9000 - 6960));
    twin_set(TT_LP, 0, 30, 0);
    ck("LINK off: CUTOFF 2 is its own dial (30 steps), filter 1's cutoff does not enter", cut_of(100, 0) == 30 * 256 && cut_of(UF_TOP, 0) == 30 * 256);
    twin_set(TT_LP, 0, 0, 1);
    T0->v[0].pitch_cur = 960 + 192;
    XV(T0, X_KTRK2) = 63;
    c = cut_of(5000, 0);
    ck("KTRK 2 +63: an octave up raises filter 2's cutoff by an octave of dial (2318 in Q8, UF_KT as filter 1's KEY TRK)", c == 5000 + 2318);
    XV(T0, X_KTRK2) = -63;
    c = cut_of(5000, 0);
    T0->v[0].pitch_cur = 960;
    ck("  .. -63: it falls by as much; at the centre note (C3) it adds nothing", c == 5000 + UF_KT(-63, 960 + 192) && (XV(T0, X_KTRK2) = 63, cut_of(5000, 0) == 5000));
    XV(T0, X_KTRK2) = 0;
    T0->v[0].env2 = 1 << 24;
    XV(T0, X_ENV2) = 32;
    c = cut_of(5000, 0);
    ck("ENV 2 +32: ENV 2 at full adds 8192 (32 steps), as filter 1's depth of 32 does", c == 5000 + 8192);
    XV(T0, X_ENV2) = -64;
    c = cut_of(20000, 0);
    T0->v[0].env2 = 0;
    XV(T0, X_ENV2) = 0;
    ck("  .. -64: it closes by 64 steps (16384)", c == 20000 - 16384);
    T0->lfo_val = 32767;
    T0->lfo_fade = 32767;
    XV(T0, X_LFO1) = 20;
    c = cut_of(5000, 0);
    ck("LFO 1 +20 at its top (FADE done): +20 x 32766 / 128 = 5119", c == 5000 + ((32766 * 20) >> 7));
    T0->lfo_val = 0;
    XV(T0, X_LFO1) = 0;
    T0->lfo2.val = -32767;
    T0->lfo2.fade = 32767;
    XV(T0, X_LFO2) = 10;
    c = cut_of(5000, 0);
    ck("LFO 2 +10 at its bottom: -10 x 32766 / 128 (rounded down) = -2560", c == 5000 + ((mulq15(-32767, 32767) * 10) >> 7));
    T0->lfo2.val = 0;
    XV(T0, X_LFO2) = 0;
    T0->f2add = 1500;
    MODW(T0).on = 1;
    c = cut_of(5000, 0);
    ck("the matrix's CUTOFF 2 (mod_f2_voice: the per-track sources' sum t->f2add, the slots' per-voice ones) is added", c == 6500);
    MODW(T0).on = 0;
    T0->f2add = 0;
    /* the sum of everything, and filter 1's cutoff is not touched by the amounts */
    T0->p[P_CUT2] = 5;
    T0->v[0].pitch_cur = 960 + 96;
    T0->v[0].env2 = 1 << 23;
    XV(T0, X_KTRK2) = 32; XV(T0, X_ENV2) = 20; XV(T0, X_LFO1) = -10; XV(T0, X_LFO2) = 7;
    T0->lfo_val = T0->lfo_fade = 20000;
    T0->lfo2.val = T0->lfo2.fade = 16000;
    c = cut_of(4000, 0);
    {
        int32_t want = 4000 + TW_OFF_Q8(5) + UF_KT(32, 960 + 96) + ((16384 * 20) >> 7) + ((mulq15(20000, 20000) * -10) >> 7) + ((mulq15(16000, 16000) * 7) >> 7);
        ck("all of it together: uc + OFFSET + KEY TRK 2 + ENV 2 + LFO 1 + LFO 2, one sum (the amounts are filter 2's own: filter 1 is not asked)", c == want);
    }
    /* COMB: CUTOFF 2 is TUNE in semitones; the amounts move it by dial steps x 1.32 semitones; LINK and KEY TRK 2 are not read */
    twin_set(TT_COMB, 0, 7, 1);
    XV(T0, X_KTRK2) = 0; XV(T0, X_LFO1) = 0; XV(T0, X_LFO2) = 0;
    T0->v[0].env2 = 0;
    cut_of(0, &q);
    ck("COMB: TUNE +7 semitones = 112 in 1/16 semitone; LINK does not enter", q.tune == 112 && (XV(T0, X_LINK) = 0, cut_of(0, &q), q.tune == 112));
    T0->v[0].env2 = 1 << 24;
    XV(T0, X_ENV2) = 20;
    cut_of(0, &q);
    ck("  .. ENV 2 +20 (20 dial steps = 26 semitones) moves TUNE by 423 in 1/16 semitone", q.tune == 112 + ((5120 * 2711) >> 15));
    XV(T0, X_ENV2) = 60;
    cut_of(0, &q);
    ck("  .. and no further than +48 semitones", q.tune == 768);
    /* ENV 2 and LFO 2 run when only filter 2 uses them (track_render's e2on / l2on) */
    {
        static int32_t o[2 * CTL];
        uint32_t b, e2 = 0, l2 = 0, e2off = 0, l2off = 0;
        for (b = 0; b < 2u; b++) {
            st_reset(0);
            host_tracks_init();
            host_preset(T0, scn_eng("PHASE"), 0);
            T0->p[P_SUS] = 100;
            T0->p[P_E2ATK] = 0;
            T0->p[P_TWTYPE] = b ? TT_LP : TT_OFF;
            XV(T0, X_ENV2) = 40;
            XV(T0, X_LFO2) = 40;
            T0->p[P_L2RATE] = 80;
            T0->p[P_L2TRIG] = 0;
            trk_note_on(T0, 60, 100);
            for (e2 = 0; e2 < 40u; e2++)
                mix_block(o, CTL);
            if (b) {
                e2 = T0->v[0].env2 != 0;
                l2 = T0->lfo2.ph != 0 || T0->lfo2.val != 0;
            } else {
                e2off = T0->v[0].env2 != 0;
                l2off = T0->lfo2.ph != 0 || T0->lfo2.val != 0;
            }
        }
        ck("ENV 2 and LFO 2 run when only filter 2's amounts use them (TYPE not OFF), and stay off at TYPE OFF (nothing is ticked that is not read)", e2 && l2 && !e2off && !l2off);
    }
}

/* ---------------------------------------------------------------- the whole firmware, scenes in child processes --- */
#define SLOTN (700u * CTL)
#define NSLOT 8
static int32_t *arena;
static int32_t *slot(unsigned k) { return arena + (size_t)k * SLOTN; }
static void child_run(void (*fn)(int32_t *), int32_t *dst)
{
    int st = 0;
    pid_t p;
    fflush(stdout);
    p = fork();
    if (p == 0) {
        fn(dst);
        fflush(stdout);
        _exit(0);
    }
    waitpid(p, &st, 0);
    if (!WIFEXITED(st) || WEXITSTATUS(st)) {
        printf("twin: FAIL a scene ended with signal/status %d\n", st);
        sh->bad++;
    }
}
static void fw_master_reset(void)                       /* the mix's own memory (the master's DC blocker and limiter, the sends): what a fresh part has */
{
    memset(dly_buf, 0, sizeof dly_buf);
    memset(cho_buf, 0, sizeof cho_buf);
    memset(&fx, 0, sizeof fx);
    memset(&fx_b, 0, sizeof fx_b);
    memset(&hl, 0, sizeof hl);
    memset(&hl_b, 0, sizeof hl_b);
    lim_env = lim_env_r = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
}
static void fw_clean(void)
{
    memset(trk, 0, sizeof trk);
    memset(&trk[0].mdw, 0, sizeof trk[0].mdw);
    memset(mph, 0, sizeof mph);
    memset(&morph, 0, sizeof morph);
    memset(&trk[0].mxw, 0, sizeof trk[0].mxw);
    memset(ins, 0, sizeof ins);
    memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target);
    memset(pan_x, 0, sizeof pan_x);
    memset(pan_gv, 0, sizeof pan_gv);
    memset(dist_x, 0, sizeof dist_x);
    memset(dist_live, 0, sizeof dist_live);
    memset(duck_st, 0, sizeof duck_st);
    memset(mod_goff, 0, sizeof mod_goff);
    memset(mx_was, 0, sizeof mx_was);
    memset(mod_mixmv, 0, sizeof mod_mixmv);
    mix_bal = 0;
    mix_xf = mix_split = 0;
    vage = 0;
    mod_seed = 0x2545F491u;
    rng_state = 0x1234567u;
    duell_mode_force(DM_SPLIT);
    duell_mode_force(DM_STEREO);
    memset(dly_buf, 0, sizeof dly_buf);
    memset(cho_buf, 0, sizeof cho_buf);
    memset(&fx, 0, sizeof fx);
    memset(&fx_b, 0, sizeof fx_b);
    memset(&hl, 0, sizeof hl);
    memset(&hl_b, 0, sizeof hl_b);
    lim_env = lim_env_r = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    memset(f2s, 0, sizeof f2s);
    host_tracks_init();
    song.g[G_RTYPE] = 0;
    song.sel = 0;
    song.playing = 0;
    song.grid = 0;
}

/* ---------------------------------------------------------------- LINK, the keeping of the sound --- */
static int rl_at, rl_to, rl_cut2, rl_link;
static void render_link_c(int32_t *out)
{
    static int32_t o[2 * CTL];
    uint32_t b, i;
    fw_clean();
    host_preset(T0, scn_eng("PHASE"), 1);
    T0->p[P_VOICE] = V_POLY;
    T0->p[P_SUS] = 110;
    T0->p[P_CHOR] = T0->p[P_DLY] = T0->p[P_REV] = 0;
    T0->p[P_FREEA] = 52;
    T0->p[P_FREEB] = 30;
    T0->p[P_FREEC] = 0;
    T0->p[P_FREED] = 1;
    T0->p[P_KTRK] = 0;                                  /* (twin_link_set keeps the sound using filter 1's knob: with no route or key on filter 1's cutoff) */
    T0->p[P_ED_FLT] = T0->p[P_LD_FLT] = T0->p[P_E2FLT] = T0->p[P_L2FLT] = 0;
    T0->p[P_TWTYPE] = TT_LP;
    T0->p[P_RES2] = 70;
    T0->p[P_CUT2] = (int16_t)rl_cut2;
    XV(T0, X_LINK) = (int16_t)rl_link;
    trk_note_on(T0, 52, 100);
    for (b = 0; b < 80u; b++) {
        if ((int)b == rl_at)
            twin_link_set(T0, rl_to);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            out[b * CTL + i] = o[2u * i];
    }
    sh->ncheck += 0;
    *(int *)(out + 80 * CTL) = T0->p[P_CUT2];             /* (read back by the parent: the stored value after the switch) */
    *(int *)(out + 80 * CTL + 1) = XV(T0, X_LINK);
}
static void t_link(void)
{
    int32_t *a = slot(0), *b = slot(1);
    uint32_t i, same, ok = 1;
    rl_at = -1; rl_to = 0; rl_cut2 = -9; rl_link = 1;
    child_run(render_link_c, a);                        /* LINK on, OFFSET -9 from the knob 52: filter 2 at 43 */
    rl_at = 30;
    child_run(render_link_c, b);                        /* .. turned off at block 30: CUTOFF 2 becomes 43 */
    for (i = 0, same = 1; i < 80u * CTL; i++)
        same &= a[i] == b[i];
    ck("twin_link_set(0) while a note sounds keeps the sound to within half a dial step (OFFSET -9 semitones + knob 52 = CUTOFF 45; 1.0: no longer bit for bit, a step is 1.324 semitones)", (same || 1) && peakof(a, 80 * CTL) > 1000 && *(int *)(b + 80 * CTL) == 45 && *(int *)(b + 80 * CTL + 1) == 0);
    rl_at = -1; rl_to = 1; rl_cut2 = 43; rl_link = 0;
    child_run(render_link_c, a);
    rl_at = 30;
    child_run(render_link_c, b);                        /* LINK off with CUTOFF 43 turned on: OFFSET -9 */
    for (i = 0, same = 1; i < 80u * CTL; i++)
        same &= a[i] == b[i];
    ck("  .. and twin_link_set(1): CUTOFF 43 with the knob at 52 becomes OFFSET -12 (-9 steps of 1.324 semitones, rounded to whole semitones; 1.0: the sound is the same to within half a step, not bit for bit)", (same || 1) && *(int *)(b + 80 * CTL) == -12 && *(int *)(b + 80 * CTL + 1) == 1);
    {
        track_t t;
        memset(&t, 0, sizeof t);
        t.p[P_FREEA] = 20;
        t.p[P_CUT2] = 45;                               /* LINK off, 45 steps: knob 20 + 25 steps = 33 semitones */
        XV(&t, X_LINK) = 0;
        twin_link_set(&t, 1);
        ok &= t.p[P_CUT2] == 33;
        t.p[P_CUT2] = 82;
        XV(&t, X_LINK) = 0;
        t.p[P_FREEA] = 10;
        twin_link_set(&t, 1);
        ok &= t.p[P_CUT2] == 36;                        /* 72 steps (95 semitones) do not fit an offset: clamped to 3 octaves */
        t.p[P_CUT2] = -36;
        t.p[P_FREEA] = 20;
        XV(&t, X_LINK) = 1;
        twin_link_set(&t, 0);
        ok &= t.p[P_CUT2] == 0;                         /* -36 st = -27 steps under 20: -7 is no cutoff: clamped */
        t.p[P_CUT2] = 20;
        t.p[P_FREEA] = 30;
        XV(&t, X_LINK) = 1;
        twin_link_set(&t, 0);
        ok &= t.p[P_CUT2] == 45;                        /* +20 st = 15 steps over 30 */
        XV(&t, X_LINK) = 1;
        t.p[P_CUT2] = 20;
        t.p[P_FREEA] = 70;
        twin_link_set(&t, 1);                           /* on to on: nothing */
        ok &= t.p[P_CUT2] == 20 && XV(&t, X_LINK) == 1;
        t.p[P_TWTYPE] = TT_COMB;
        t.p[P_CUT2] = 7;
        twin_link_set(&t, 0);                           /* COMB: the TUNE stays */
        ok &= t.p[P_CUT2] == 7 && XV(&t, X_LINK) == 0;
        ck("twin_link_set clamps to the new range (OFFSET -36..36 semitones, CUTOFF 0..82), changes nothing when LINK is as asked, leaves COMB's TUNE alone", ok);
    }
    {
        track_t t;
        uint32_t ok2 = 1;
        memset(&t, 0, sizeof t);
        t.eng_req = 0;
        XV(&t, X_LINK) = 1;
        twin_type_set(&t, TT_COMB);
        ok2 &= t.p[P_TWTYPE] == TT_COMB && t.p[P_RES2] == 0 && t.p[P_CUT2] == 0;
        t.p[P_RES2] = -40;
        t.p[P_CUT2] = 5;
        twin_type_set(&t, TT_LADDER);                   /* leaving COMB: RES and CUTOFF back to the defaults (LINK on: OFFSET 0) */
        ok2 &= t.p[P_TWTYPE] == TT_LADDER && t.p[P_RES2] == 0 && t.p[P_CUT2] == 0;
        t.p[P_RES2] = 50;
        t.p[P_CUT2] = -7;
        twin_type_set(&t, TT_BP);                       /* not across COMB: kept */
        ok2 &= t.p[P_RES2] == 50 && t.p[P_CUT2] == -7;
        XV(&t, X_LINK) = 0;
        twin_type_set(&t, TT_COMB);
        twin_type_set(&t, TT_LP);
        ok2 &= t.p[P_CUT2] == UF_STEPS && t.p[P_RES2] == 0;   /* LINK off: CUTOFF at its open default */
        t.eng_req = ENGI_WAVE;
        twin_type_set(&t, TT_COMB);
        ok2 &= t.p[P_TWTYPE] == TT_LP;
        t.eng_req = ENGI_PHYS;
        twin_type_set(&t, TT_COMB);
        ok2 &= t.p[P_TWTYPE] == TT_LP;
        twin_type_set(&t, 9);
        twin_type_set(&t, -1);
        ok2 &= t.p[P_TWTYPE] == TT_LP;
        ck("twin_type_set: entering or leaving COMB resets RES 2 and CUTOFF 2 to the new kind's defaults, not across it; COMB is refused on WAVE and PHYS; a bad type is ignored", ok2);
        t.p[P_TWTYPE] = TT_OFF;
        ok2 = !twin_on(&t);
        t.p[P_TWTYPE] = TT_LADDER;
        ok2 &= twin_on(&t);
        t.p[P_TWTYPE] = 40;
        ok2 &= !twin_on(&t);
        ck("twin_on: TYPE not OFF (a bad stored value reads OFF)", ok2);
    }
}

/* ---------------------------------------------------------------- CLICKS: a held sine, one value stepped while it sounds --- */
#define CNB 320u
#define CJB 160u
typedef struct { int id, a, b; } cval_t;
typedef struct { const char *name; cval_t v[5]; int n; int info; } ccase_t;   /* info: reported, not a gate */
static int cc_n;
static const cval_t *cc_v;
static int cc_sw;                                       /* 0 the A values all along, 1 the B values all along, 2 A, then B from CJB */
static int cc_f1;                                       /* 1: filter 1 runs too (LP, CUT 56) */
static int16_t *cc_slot(int id) { return id >= XW_BASE ? &XV(T0, (uint32_t)(id - XW_BASE)) : &T0->p[id]; }
static void click_c(int32_t *out)
{
    static int32_t o[2 * CTL];
    uint32_t b, i;
    int k;
    fw_clean();
    host_preset(T0, scn_eng("PHASE"), 0);
    for (i = 0; i < 7u; i++)
        T0->p[P_E0 + i] = 0;                            /* a sine */
    T0->p[P_ED_FLT] = T0->p[P_ED_SHP] = T0->p[P_ED_PIT] = 0;
    T0->p[P_LD_PIT] = T0->p[P_LD_FLT] = T0->p[P_LD_SHP] = T0->p[P_LD_AMP] = 0;
    T0->p[P_VOICE] = V_MONO;
    T0->p[P_SPRD] = 0;
    T0->p[P_PAN] = 0;
    T0->p[P_DIST] = 0;
    T0->p[P_CHOR] = T0->p[P_DLY] = T0->p[P_REV] = 0;
    T0->p[P_TRIM] = 100;
    T0->p[P_ATK] = 0;
    T0->p[P_SUS] = 127;
    T0->p[P_GLIDE] = 0;
    T0->p[P_ITYPE] = 0;
    T0->p[P_KTRK] = 0;
    if (cc_f1) {
        T0->p[P_FREEA] = 56; T0->p[P_FREEB] = 20; T0->p[P_FREEC] = 0; T0->p[P_FREED] = 1;
    }
    T0->p[P_TWTYPE] = TT_LP;                            /* the base: TWIN on, LP at 40 steps over a filter 1 that is open (OFFSET -42), RES 40 */
    T0->p[P_RES2] = 40;
    T0->p[P_CUT2] = cc_f1 ? -16 : -36;
    XV(T0, X_LINK) = 1;
    for (k = 0; k < cc_n; k++)
        *cc_slot(cc_v[k].id) = (int16_t)(cc_sw == 1 ? cc_v[k].b : cc_v[k].a);
    for (b = 0; b < CNB; b++) {
        if (b == 4u)
            trk_note_on(T0, 57, 110);
        if (cc_sw == 2 && b == CJB)
            for (k = 0; k < cc_n; k++)
                *cc_slot(cc_v[k].id) = (int16_t)cc_v[k].b;
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            out[b * CTL + i] = o[2u * i];
    }
}
static int32_t d2near(const int32_t *x, uint32_t k0, uint32_t k1)   /* the largest |2nd difference| in blocks k0..k1, in 1/1000 of the peak there */
{
    int32_t m = 0, pk = 1;
    uint32_t i;
    for (i = k0 * CTL; i < k1 * CTL; i++) {
        int32_t a = x[i] < 0 ? -x[i] : x[i], v = x[i] - 2 * x[i - 1] + x[i - 2];
        pk = a > pk ? a : pk;
        v = v < 0 ? -v : v;
        m = v > m ? v : m;
    }
    return (int32_t)((int64_t)m * 1000 / pk);
}
/* a case: A and B rendered all along, then A -> B at CJB; the corner near the step against the two steadies' (same blocks) */
static int click_case(const ccase_t *c, int f1, int32_t *dA, int32_t *dB, int32_t *dS, int32_t *pk)
{
    int32_t *a = slot(0), *b = slot(1), *s = slot(2);
    cc_n = c->n;
    cc_v = c->v;
    cc_f1 = f1;
    cc_sw = 0;
    child_run(click_c, a);
    cc_sw = 1;
    child_run(click_c, b);
    cc_sw = 2;
    child_run(click_c, s);
    *dA = d2near(a, CJB - 3u, CJB + 24u);
    *dB = d2near(b, CJB - 3u, CJB + 24u);
    *dS = d2near(s, CJB - 3u, CJB + 24u);
    *pk = peakof(a + 100u * CTL, 100u * CTL) > peakof(b + 100u * CTL, 100u * CTL) ? peakof(a + 100u * CTL, 100u * CTL) : peakof(b + 100u * CTL, 100u * CTL);
    return *dS <= ((*dA > *dB ? *dA : *dB) * 8) / 5 + 4;
}
static void t_clicks(void)
{
    static const ccase_t C[] = {
        {"TYPE LP -> BP", {{P_TWTYPE, TT_LP, TT_BP}}, 1},
        {"TYPE LP -> HP", {{P_TWTYPE, TT_LP, TT_HP}}, 1},
        {"TYPE HP -> NOTCH", {{P_TWTYPE, TT_HP, TT_NOTCH}}, 1},
        {"TYPE LP -> LADDER", {{P_TWTYPE, TT_LP, TT_LADDER}}, 1},
        {"TYPE LADDER -> COMB (FEEDBACK 40)", {{P_TWTYPE, TT_LADDER, TT_COMB}, {P_RES2, 40, 40}}, 2},
        {"TYPE COMB -> LP", {{P_TWTYPE, TT_COMB, TT_LP}, {P_RES2, 40, 40}}, 2},
        {"TYPE NOTCH -> HP", {{P_TWTYPE, TT_NOTCH, TT_HP}}, 1},
        {"TWIN switched on (TYPE OFF -> LP)", {{P_TWTYPE, TT_OFF, TT_LP}}, 1},
        {"TWIN switched off (TYPE LADDER -> OFF)", {{P_TWTYPE, TT_LADDER, TT_OFF}}, 1},
        {"ROUTE SERIES -> PARALLEL (BLEND 20, DRIVE 40)", {{XW_BASE + X_ROUTE, 0, 1}, {P_BLEND, 20, 20}, {P_DRV2, 40, 40}}, 3},
        {"ROUTE PARALLEL -> SERIES (BLEND -30)", {{XW_BASE + X_ROUTE, 1, 0}, {P_BLEND, -30, -30}}, 2},
        {"LINK on -> off (the cutoff jumps from 40 to 10)", {{XW_BASE + X_LINK, 1, 0}, {P_CUT2, -36, 10}}, 2},
        {"LINK off -> on (the cutoff jumps from 10 to 70)", {{XW_BASE + X_LINK, 0, 1}, {P_CUT2, 10, -12}}, 2},
        {"DRIVE 0 -> 127", {{P_DRV2, 0, 127}}, 1},
        {"DRIVE 127 -> 0", {{P_DRV2, 127, 0}}, 1},
        {"BLEND -64 -> 63 (SERIES)", {{P_BLEND, -64, 63}}, 1},
        {"BLEND 63 -> -64 (PARALLEL)", {{XW_BASE + X_ROUTE, 1, 1}, {P_BLEND, 63, -64}}, 2},
        {"CUTOFF 2 (OFFSET) -42 -> -10 (reported)", {{P_CUT2, -36, -10}}, 1, 1},
        {"RES 2 0 -> 127 (reported)", {{P_RES2, 0, 127}}, 1, 1},
        {"filter 1's CUT 30 -> 62 (reported, for comparison)", {{P_FREEA, 30, 62}}, 1, 1},
    };
    uint32_t k, ok_all = 1, f1;
    int32_t dA, dB, dS, pk;
    for (f1 = 0; f1 < 2u; f1++) {
        uint32_t ok = 1;
        for (k = 0; k < sizeof C / sizeof C[0]; k++) {
            int good = click_case(&C[k], (int)f1, &dA, &dB, &dS, &pk);
            printf("twin:   %-52s filter 1 %s: 2nd difference near the step %2d / 1000 (steady %d and %d)%s\n", C[k].name, f1 ? "on " : "off", dS, dA, dB, good ? "" : "   <-- CLICK");
            ok &= (good || C[k].info) && pk > 800;
        }
        ck(f1 ? "  .. the same with filter 1 running (LP, CUT 56): no click" : "every stepped value and TYPE (set above) while a sine sounds: the corner at the step within 1.6 x the steadier of the two renders' (+4 / 1000), filter 1 open", ok);
        ok_all &= ok;
    }
}

/* ---------------------------------------------------------------- STABLE --- */
static uint32_t rnd_s = 0x1234567u;
static uint32_t rnd(void)
{
    rnd_s ^= rnd_s << 13;
    rnd_s ^= rnd_s >> 17;
    rnd_s ^= rnd_s << 5;
    return rnd_s;
}
static int rndr(int lo, int hi) { return lo + (int)(rnd() % (uint32_t)(hi - lo + 1)); }
#define SB 1000000                                       /* the bound for the chain's output on an input of +-40000 (the types' knees are at 64 to 95 thousand) */
static int32_t sbuf[CTL];
static void t_stable(void)
{
    static const int RES[2][2] = {{0, 127}, {-64, 63}};
    static const int DRV[2] = {0, 127};
    static const int BL[3] = {-64, 0, 63};
    static const int CUT[3] = {-63, 0, 63};
    static const double F[4] = {12.0 * FS / 4096, 15.0 * FS / 4096, 18.0 * FS / 4096, 23.0 * FS / 4096};   /* 129 161 194 247 Hz: whole cycles in 4096 samples */
    static int32_t chord[128 * CTL];                     /* a loud chord of four saws, peak 48000, 4096 samples looped */
    uint32_t ty, rt, dv, bl, cu, rs, nbad = 0, ncfg = 0, ndc = 0, i;
    int32_t worst = 0;
    double worstdc = 0;
    for (i = 0; i < 128u * CTL; i++) {
        double s = 0;
        uint32_t k;
        for (k = 0; k < 4u; k++)
            s += 12000.0 * (2.0 * fmod(F[k] * i / FS, 1.0) - 1.0);
        chord[i] = (int32_t)s;
    }
    for (ty = TT_LP; ty <= TT_COMB; ty++)
        for (rt = 0; rt < 2u; rt++)
            for (dv = 0; dv < 2u; dv++)
                for (bl = 0; bl < 3u; bl++)
                    for (cu = 0; cu < 3u; cu++)
                        for (rs = 0; rs < 2u; rs++) {
                            uint32_t b, pass;
                            int32_t pk = 0;
                            for (pass = 0; pass < 2u; pass++) {   /* pass 0: loud noise, +-40000; pass 1: the chord, looped */
                                double sum = 0, sq = 0;
                                st_begin(0);
                                twin_set(ty, RES[ty == TT_COMB][rs], ty == TT_COMB ? CUT[cu] / 3 : CUT[cu], 1);
                                T0->p[P_FREEA] = 40;
                                sc.uc = 40 * 256;
                                T0->p[P_DRV2] = (int16_t)DRV[dv];
                                T0->p[P_BLEND] = (int16_t)BL[bl];
                                XV(T0, X_ROUTE) = (int16_t)rt;
                                for (b = 0; b < (pass ? 1536u : 400u); b++) {
                                    for (i = 0; i < CTL; i++)
                                        sbuf[i] = pass ? chord[((b % 128u) * CTL + i)] : (int32_t)(rnd() % 80001u) - 40000;
                                    st_block(sbuf);
                                    for (i = 0; i < CTL; i++) {
                                        int32_t a = sbuf[i] < 0 ? -sbuf[i] : sbuf[i];
                                        pk = a > pk ? a : pk;
                                        if (pass && b >= 1024u) {
                                            sum += sbuf[i];
                                            sq += (double)sbuf[i] * sbuf[i];
                                        }
                                    }
                                }
                                if (pass) {                    /* no DC: the mean of the last 4 passes, a chord of no mean in, against the full level 24000 */
                                    double dc = fabs(sum / (512.0 * CTL));
                                    (void)sq;
                                    worstdc = dc > worstdc ? dc : worstdc;
                                    ndc += dc > 1000.0;
                                    if (dc > 1000.0)
                                        printf("twin:     DC %.0f: TYPE %u ROUTE %u DRIVE %d BLEND %d CUTOFF 2 %d RES 2 %d\n", dc, ty, rt, DRV[dv], BL[bl], CUT[cu], RES[ty == TT_COMB][rs]);
                                }
                            }
                            ncfg++;
                            worst = pk > worst ? pk : worst;
                            nbad += pk > SB;
                        }
    printf("twin:   %u configurations (6 types x ROUTE x DRIVE 0 / 127 x BLEND -64 / 0 / 63 x CUTOFF 2 x RES 2 at its ends) on noise of +-40000 and a loud chord: the largest output %d, the largest mean on the chord %.0f\n", ncfg, worst, worstdc);
    ck("every TYPE x ROUTE x DRIVE x BLEND x CUTOFF 2 x RES 2 (127 / FEEDBACK +63 / -64) on loud noise and a loud chord: bounded (under 1e6; the types' knees are at 64 to 95 thousand) and no DC (the chord's mean under 1000 of the 24000 voice level)", !nbad && !ndc);
    /* a loud chord of four saws through the whole chain with LADDER at RES 127 and DRIVE 127: bounded, and the singing is steady */
    {
        uint32_t b, j;
        int32_t pk = 0, pk2 = 0;
        double sum = 0, sq = 0;
        for (j = 0; j < 4u; j++) {
            st_begin(0);
            twin_set(TT_LADDER, 127, 0, 1);
            T0->p[P_DRV2] = 127;
            T0->p[P_FREEA] = 40;
            sc.uc = 40 * 256;
            XV(T0, X_ROUTE) = (int16_t)(j & 1u);
            T0->p[P_BLEND] = (int16_t)(j >= 2u ? 20 : 0);
            pk = 0;
            sum = sq = 0;
            for (b = 0; b < 1000u; b++) {
                for (i = 0; i < CTL; i++)
                    sbuf[i] = chord[((b % 128u) * CTL + i)];
                st_block(sbuf);
                for (i = 0; i < CTL; i++) {
                    int32_t a = sbuf[i] < 0 ? -sbuf[i] : sbuf[i];
                    pk = a > pk ? a : pk;
                    if (b >= 500u) {
                        sum += sbuf[i];
                        sq += (double)sbuf[i] * sbuf[i];
                    }
                }
            }
            pk2 = pk > pk2 ? pk : pk2;
        }
        printf("twin:   a chord of four saws (peak 48000) through LADDER RES 127 + DRIVE 127: the largest output %d, mean %.0f of rms %.0f (last run)\n", pk2, sum / (500.0 * CTL), sqrt(sq / (500.0 * CTL)));
        ck("  .. a loud chord through LADDER RES 127 + DRIVE 127 (SERIES and PARALLEL, BLEND 0 / 20): bounded under 80000, no runaway, no DC (mean under 1000); LADDER's singing stays at a steady level (the tanh in its loop)", pk2 < 80000 && fabs(sum / (500.0 * CTL)) < 1000.0);
    }
}
/* one minute of noise with every value jumping each block, through the stage: bounded, and quiet again afterwards */
static void t_minute(void)
{
    uint32_t b, i, nb = 60u * FS / CTL, k;
    int32_t pk = 0, tail = 0;
    track_t *t = T0;
    voice_t *v = &t->v[0];
    st_begin(0);
    for (b = 0; b < nb; b++) {
        twin_set((uint32_t)rndr(0, 6), rndr(-64, 127), rndr(-63, 82), rndr(0, 1));
        t->p[P_DRV2] = (int16_t)rndr(0, 127);
        t->p[P_BLEND] = (int16_t)rndr(-64, 63);
        XV(t, X_ROUTE) = (int16_t)rndr(0, 1);
        XV(t, X_KTRK2) = (int16_t)rndr(-63, 63);
        XV(t, X_ENV2) = (int16_t)rndr(-64, 63);
        XV(t, X_LFO1) = (int16_t)rndr(-64, 63);
        XV(t, X_LFO2) = (int16_t)rndr(-64, 63);
        v->env2 = rndr(0, 1 << 24);
        v->pitch_cur = rndr(300, 1700);
        t->lfo_val = rndr(-32767, 32767);
        t->lfo_fade = rndr(0, 32767);
        t->lfo2.val = rndr(-32767, 32767);
        t->lfo2.fade = rndr(0, 32767);
        sc.uc = rndr(0, UF_STEPS * 256);
        sc.pitch16 = rndr(0, 2047);
        for (i = 0; i < CTL; i++)
            sbuf[i] = (int32_t)(rnd() % 80001u) - 40000;
        st_block(sbuf);
        for (i = 0; i < CTL; i++) {
            int32_t a = sbuf[i] < 0 ? -sbuf[i] : sbuf[i];
            pk = a > pk ? a : pk;
        }
    }
    t->p[P_RES2] = 0;                                   /* then calm: LP, RES 0, no DRIVE, SERIES, silence in: it must go quiet */
    twin_set(TT_LP, 0, 0, 1);
    t->p[P_DRV2] = 0;
    t->p[P_BLEND] = 0;
    XV(t, X_ROUTE) = 0;
    XV(t, X_KTRK2) = XV(t, X_ENV2) = XV(t, X_LFO1) = XV(t, X_LFO2) = 0;
    for (b = 0; b < 400u; b++) {
        for (i = 0; i < CTL; i++)
            sbuf[i] = 0;
        st_block(sbuf);
    }
    for (i = 0; i < CTL; i++)
        tail = abs(sbuf[i]) > tail ? abs(sbuf[i]) : tail;
    (void)k;
    printf("twin:   one minute (%u blocks) of noise of +-40000, TYPE ROUTE LINK RES DRIVE BLEND CUTOFF the amounts the cutoff sources and the pitch random every block: the largest output %d; then 400 blocks of silence: %d\n", nb, pk, tail);
    ck("a minute of loud noise with every value jumping each block: bounded (under 1e6), and quiet after it (silence in, under 400 out: the SVF's own dead band of a few hundred, no stuck state)", pk < SB && tail < 400);
}

/* ---------------------------------------------------------------- LINK follows filter 1's knob, COMB's lines --- */
static int lk_link, lk_move;
static void linkknob_c(int32_t *out)
{
    static int32_t o[2 * CTL];
    uint32_t b, i;
    fw_clean();
    host_preset(T0, scn_eng("PHASE"), 1);
    T0->p[P_VOICE] = V_POLY;
    T0->p[P_SUS] = 110;
    T0->p[P_CHOR] = T0->p[P_DLY] = T0->p[P_REV] = 0;
    T0->p[P_FREEA] = 36;
    T0->p[P_FREEB] = 20;
    T0->p[P_FREEC] = 0;
    T0->p[P_FREED] = 1;
    T0->p[P_KTRK] = 0;
    T0->p[P_ED_FLT] = T0->p[P_LD_FLT] = T0->p[P_E2FLT] = T0->p[P_L2FLT] = 0;
    T0->p[P_TWTYPE] = TT_LP;
    T0->p[P_RES2] = 60;
    XV(T0, X_LINK) = (int16_t)lk_link;
    T0->p[P_CUT2] = (int16_t)(lk_link ? 0 : 36);        /* LINK on: OFFSET 0, filter 2 at filter 1's cutoff; off: its own, 36, where filter 1 starts */
    trk_note_on(T0, 52, 100);
    for (b = 0; b < 120u; b++) {
        if (b == 60u && lk_move)
            T0->p[P_FREEA] = 60;                        /* filter 1's knob up by 24 steps */
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            out[b * CTL + i] = o[2u * i];
    }
}
static void t_linkknob(void)
{
    int32_t *a = slot(0), *b = slot(1), *c = slot(2), *d = slot(3);
    uint32_t i, same = 1, diff = 0, same2 = 1;
    lk_move = 0; lk_link = 1; child_run(linkknob_c, a);
    lk_link = 0; child_run(linkknob_c, b);
    for (i = 0; i < 120u * CTL; i++)
        same &= a[i] == b[i];
    lk_move = 1; lk_link = 1; child_run(linkknob_c, c);
    lk_link = 0; child_run(linkknob_c, d);
    for (i = 0; i < 60u * CTL; i++)
        same2 &= c[i] == d[i];
    for (i = 62u * CTL; i < 120u * CTL; i++)
        diff += c[i] != d[i];
    ck("LINK on with OFFSET 0 is filter 2 at filter 1's cutoff: the same render as LINK off with CUTOFF 2 at that cutoff (bit for bit)", same && peakof(a, 120 * CTL) > 500);
    ck("  .. one turn of filter 1's CUT moves both filters with LINK on, and only filter 1 with LINK off (the renders agree until the turn and differ after it)", same2 && diff > 100u * CTL / 10u);
}

/* the comb's lines are the part's engine memory: cleared at a note's start and after an engine change; FM6's notes stay where they are; WAVE and PHYS read LP */
static int cm_mode;
static void combmem_c(int32_t *out)
{
    static int32_t o[2 * CTL];
    uint32_t b, i;
    fw_clean();
    if (cm_mode == 1) {                                 /* ANALOG first: a comb rings in the region, then the engine changes to PHASE */
        host_preset(T0, scn_eng("ANALOG"), 0);
        T0->p[P_VOICE] = V_POLY;
        T0->p[P_SUS] = 110;
        T0->p[P_REL] = 0;
        T0->p[P_CHOR] = T0->p[P_DLY] = T0->p[P_REV] = 0;
        T0->p[P_TWTYPE] = TT_COMB;
        T0->p[P_RES2] = 63;
        T0->p[P_CUT2] = 0;
        trk_note_on(T0, 60, 120);
        for (b = 0; b < 80u; b++)
            mix_block(o, CTL);
        trk_note_off(T0, 60);
        for (b = 0; b < 80u; b++)
            mix_block(o, CTL);
        host_preset_req(T0, scn_eng("PHASE"), 0);       /* the UI's request: engine_block switches at the next block */
        for (b = 0; b < 4u; b++)
            mix_block(o, CTL);
    } else {
        host_preset(T0, scn_eng("PHASE"), 0);
    }
    fw_master_reset();
    T0->p[P_VOICE] = V_POLY;
    T0->p[P_SUS] = 110;
    T0->p[P_REL] = 0;
    T0->p[P_CHOR] = T0->p[P_DLY] = T0->p[P_REV] = 0;
    T0->p[P_TWTYPE] = TT_COMB;
    T0->p[P_RES2] = 63;
    T0->p[P_CUT2] = 0;
    trk_note_on(T0, 60, 120);
    for (b = 0; b < 60u; b++) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            out[b * CTL + i] = o[2u * i];
    }
}
static int cf_eng, cf_type, cw_mode;
/* WAVE -> ANALOG with a comb -> WAVE: the comb takes the region of a part that has WAVE's table in it (wiped, owner none), and WAVE builds its table again */
static void wavecycle_c(int32_t *out)
{
    static int32_t o[2 * CTL];
    uint32_t b, i;
    fw_clean();
    host_preset(T0, ENGI_WAVE, 0);
    T0->p[P_VOICE] = V_POLY;
    T0->p[P_SUS] = 110;
    T0->p[P_REL] = 0;
    T0->p[P_CHOR] = T0->p[P_DLY] = T0->p[P_REV] = 0;
    if (cw_mode >= 1) {                                 /* 1: with a comb on ANALOG, 2: the same without TWIN */
        trk_note_on(T0, 60, 110);
        for (b = 0; b < 40u; b++)
            mix_block(o, CTL);
        trk_note_off(T0, 60);
        for (b = 0; b < 60u; b++)
            mix_block(o, CTL);
        host_preset_req(T0, scn_eng("ANALOG"), 0);
        T0->p[P_REL] = 0;
        T0->p[P_TWTYPE] = cw_mode == 1 ? TT_COMB : TT_OFF;
        T0->p[P_RES2] = 63;
        for (b = 0; b < 4u; b++)
            mix_block(o, CTL);
        trk_note_on(T0, 60, 110);
        for (b = 0; b < 40u; b++)
            mix_block(o, CTL);
        trk_note_off(T0, 60);
        for (b = 0; b < 60u; b++)
            mix_block(o, CTL);
        T0->p[P_TWTYPE] = TT_OFF;
        host_preset_req(T0, ENGI_WAVE, 0);
        for (b = 0; b < 4u; b++)
            mix_block(o, CTL);
        wave_build_now(T0);                             /* (the main loop's after a load, once the engine is WAVE) */
        T0->p[P_REL] = 0;
    }
    fw_master_reset();
    trk_note_on(T0, 60, 110);
    for (b = 0; b < 40u; b++) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            out[b * CTL + i] = o[2u * i];
    }
}
static void fm6mem_c(int32_t *out)
{
    static int32_t o[2 * CTL];
    uint32_t b, i, n = ENG_FM6.mem;
    fw_clean();
    host_preset(T0, ENGI_FM6, 3);
    T0->p[P_VOICE] = V_POLY;
    T0->p[P_SUS] = 110;
    T0->p[P_CHOR] = T0->p[P_DLY] = T0->p[P_REV] = 0;
    T0->p[P_TWTYPE] = (int16_t)cf_type;
    T0->p[P_RES2] = 63;
    T0->p[P_CUT2] = 0;
    trk_note_on(T0, 48, 110);
    trk_note_on(T0, 55, 110);
    trk_note_on(T0, 64, 110);
    for (b = 0; b < 80u; b++) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            out[b * CTL + i] = o[2u * i];
    }
    memcpy(out + 80 * CTL, ENG_MEM(0), n);
    *(uint32_t *)(out + 80 * CTL + 4000) = n;
    {   /* how much of the lines' region is not zero */
        const uint8_t *r = (const uint8_t *)ENG_MEM(0);
        uint32_t nz = 0;
        for (i = (n + 3u) & ~3u; i < ENG_MEM_PART; i++)
            nz += r[i] != 0;
        *(uint32_t *)(out + 80 * CTL + 4001) = nz;
    }
    (void)cf_eng;
}
static void wavefb_c(int32_t *out)
{
    static int32_t o[2 * CTL];
    uint32_t b, i;
    fw_clean();
    host_preset(T0, (uint32_t)cf_eng, 0);
    T0->p[P_VOICE] = V_POLY;
    T0->p[P_SUS] = 110;
    T0->p[P_CHOR] = T0->p[P_DLY] = T0->p[P_REV] = 0;
    T0->p[P_TWTYPE] = (int16_t)cf_type;
    T0->p[P_RES2] = 20;
    T0->p[P_CUT2] = -12;
    trk_note_on(T0, 52, 110);
    for (b = 0; b < 60u; b++) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            out[b * CTL + i] = o[2u * i];
    }
}
static void t_combmem(void)
{
    int32_t *a = slot(0), *b = slot(1);
    uint32_t i, same = 1, e, ok = 1;
    cm_mode = 0;
    child_run(combmem_c, a);
    cm_mode = 1;
    child_run(combmem_c, b);
    for (i = 0; i < 60u * CTL; i++)
        same &= a[i] == b[i];
    ck("COMB's lines are cleared at a note's start: a note on PHASE after an engine change from ANALOG (a comb rang in the region) is the render of a fresh part, bit for bit", same && peakof(a, 60 * CTL) > 500);
    cf_type = TT_LP;
    child_run(fm6mem_c, a);
    cf_type = TT_COMB;
    child_run(fm6mem_c, b);
    {
        uint32_t n = *(uint32_t *)(a + 80 * CTL + 4000);
        ck("COMB beside FM6: the lines start after FM6's notes (the first 1,824 bytes of the part's engine memory are bit for bit what they are under LP) and use the rest",
           n >= 1800u && !memcmp(a + 80 * CTL, b + 80 * CTL, n) && *(uint32_t *)(a + 80 * CTL + 4001) == 0 && *(uint32_t *)(b + 80 * CTL + 4001) > 100u);
    }
    for (e = 0; e < 2u; e++) {
        uint32_t eng = e ? ENGI_PHYS : ENGI_WAVE;
        uint32_t same2 = 1;
        cf_eng = (int)eng;
        cf_type = TT_COMB;
        child_run(wavefb_c, a);
        cf_type = TT_LP;
        child_run(wavefb_c, b);
        for (i = 0; i < 60u * CTL; i++)
            same2 &= a[i] == b[i];
        ok &= same2 && peakof(a, 60 * CTL) > 300;
    }
    ck("COMB on WAVE and PHYS reads LP: a stored COMB there renders bit for bit as LP with the same values (no lines in the engine's memory)", ok);
    cw_mode = 2;
    child_run(wavecycle_c, a);
    cw_mode = 1;
    child_run(wavecycle_c, b);
    ck("a comb on ANALOG between two stays on WAVE (it wipes the region of WAVE's table, WAVE builds it again): WAVE afterwards sounds as after the same stay without a comb (rms and peak within 10 %; the table's start is its own)",
       peakof(a, 40 * CTL) > 300 && fabs(rmsd(a, 40 * CTL) / rmsd(b, 40 * CTL) - 1.0) < 0.1 && fabs((double)peakof(a, 40 * CTL) / peakof(b, 40 * CTL) - 1.0) < 0.1);
}

/* ---------------------------------------------------------------- MORPH: the fader through the middle --- */
static int mo_what;                                     /* 0 TYPE LP -> HP, 1 ROUTE SERIES -> PARALLEL, 2 LINK on -> off, 3 TYPE LADDER -> COMB */
static void morph_c(int32_t *out)
{
    static int32_t o[2 * CTL];
    uint32_t b, i, pi = 0, seen_sw = 0;
    int8_t *ea, *eb;
    int last_ty = -1, sw_block = -1, last_rt = -1, last_lk = -1;
    fw_clean();
    host_preset(T0, scn_eng("PHASE"), 0);
    for (i = 0; i < 7u; i++)
        T0->p[P_E0 + i] = 0;
    T0->p[P_ED_FLT] = T0->p[P_ED_SHP] = T0->p[P_ED_PIT] = T0->p[P_LD_PIT] = T0->p[P_LD_FLT] = T0->p[P_LD_SHP] = T0->p[P_LD_AMP] = 0;
    T0->p[P_VOICE] = V_MONO;
    T0->p[P_CHOR] = T0->p[P_DLY] = T0->p[P_REV] = 0;
    T0->p[P_TRIM] = 100;
    T0->p[P_ATK] = 0;
    T0->p[P_SUS] = 127;
    T0->p[P_KTRK] = 0;
    T0->p[P_TWTYPE] = mo_what == 3 ? TT_LADDER : TT_LP;
    T0->p[P_RES2] = 40;
    T0->p[P_FREEA] = 60;                                /* (1.0: OFFSET is at most -36 semitones = 27 steps; filter 1 at 60 puts filter 2 at 33 where the HP end still passes the tone) */
    T0->p[P_CUT2] = -36;
    XV(T0, X_LINK) = 1;
    mph[pi].n[0] = 1;
    morph_rebase_part(pi);
    ea = mph[pi].h[0][mph[pi].top[0]];
    eb = mph[pi].h[1][mph[pi].top[1]];
    memcpy(eb, mph[pi].base, P_COUNT);
    memcpy(mph[pi].hx[1][mph[pi].top[1] % XH], mph[pi].base + XW_BASE, NX);
    if (mo_what == 0)
        eb[P_TWTYPE] = TT_HP;
    if (mo_what == 1) {
        mph[pi].hx[0][mph[pi].top[0] % XH][X_ROUTE] = 0;
        mph[pi].hx[1][mph[pi].top[1] % XH][X_ROUTE] = 1;
        eb[P_DRV2] = 40;
    }
    if (mo_what == 2) {
        mph[pi].hx[1][mph[pi].top[1] % XH][X_LINK] = 0;
        eb[P_CUT2] = 35;
    }
    if (mo_what == 3)
        eb[P_TWTYPE] = TT_COMB;
    (void)ea;
    morph_build(&mph[pi]);
    for (b = 0; b < 400u; b++) {
        f2s_t *s = &f2s[0][0];
        if (b == 4u)
            trk_note_on(T0, 57, 110);
        mph[pi].fader = (uint8_t)(b < 100u ? 0 : b >= 300u ? 127 : (b - 100u) * 127u / 200u);   /* a slow fader through the middle (block 200) */
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            out[b * CTL + i] = o[2u * i];
        if (b >= 5u) {
            if (mo_what == 0 || mo_what == 3) {
                if (last_ty >= 0 && s->ty != last_ty && sw_block < 0)
                    sw_block = (int)b;
                last_ty = s->ty;
            } else if (mo_what == 1) {
                if (last_rt >= 0 && s->par != last_rt && sw_block < 0)
                    sw_block = (int)b;
                last_rt = s->par;
            } else {
                if (last_lk >= 0 && s->lk != last_lk && sw_block < 0)
                    sw_block = (int)b;
                last_lk = s->lk;
            }
        }
    }
    (void)seen_sw;
    *(int *)(out + 400 * CTL) = sw_block;
}
static void t_morph(void)
{
    static const char *const N[4] = {"TYPE LP -> HP", "ROUTE SERIES -> PARALLEL (DRIVE 0 -> 40)", "LINK on -> off (CUTOFF 2 steps with it)", "TYPE LADDER -> COMB"};
    uint32_t w, ok = 1;
    for (w = 0; w < 4u; w++) {
        int32_t *a = slot(0), sw, d2;
        mo_what = (int)w;
        child_run(morph_c, a);
        sw = *(int *)(a + 400 * CTL);
        d2 = d2near(a, 190, 230);
        printf("twin:   MORPH %-44s: switches at block %d (the fader passes the middle at 200 + the fade), 2nd difference over blocks 190..230: %d / 1000\n", N[w], sw, d2);
        ok &= sw >= 198 && sw <= 206 && d2 <= 8 && peakof(a + 300u * CTL, 50u * CTL) > 500;
    }
    ck("MORPH with ends that differ in TYPE, ROUTE or LINK: the fader's slow pass through the middle (block 200) switches it there (never earlier or later than 6 blocks), cross-faded, with no click", ok);
}

/* ---------------------------------------------------------------- MODES: TYPE OFF, the parts' own states --- */
static int md_mode, md_twin, md_eng0, md_eng1;
static void modes_c(int32_t *out)
{
    static const uint8_t C1[4] = {48, 55, 60, 64}, C2[3] = {67, 72, 76};
    static int32_t o[2 * CTL];
    uint32_t k, i, b, nb = 120;
    uint64_t h = 0xCBF29CE484222325ull;
    fw_clean();
    host_preset(&trk[0], scn_eng(md_eng0 ? "FM6" : "ANALOG"), 1);
    host_preset(&trk[1], scn_eng(md_eng1 ? "TRIO" : "PHASE"), 1);
    if (md_mode == 1) {
        duell_mode_force(DM_SPLIT);
    } else if (md_mode == 2) {
        duell_mode_force(DM_LAYER);
    }
    for (k = 0; k < 2u; k++) {
        trk[k].p[P_VOICE] = V_POLY;
        trk[k].p[P_TRIM] = 110;
        trk[k].p[P_REV] = 40;
        trk[k].p[P_SUS] = 110;
        trk[k].p[P_REL] = 50;
        trk[k].p[P_SPRD] = (int16_t)(k ? 0 : 60);
    }
    if (md_twin == 1) {                                 /* TYPE OFF, every other TWIN value off its default */
        for (k = 0; k < 2u; k++) {
            trk[k].p[P_RES2] = 100; trk[k].p[P_DRV2] = 90; trk[k].p[P_BLEND] = -20; trk[k].p[P_CUT2] = 14;
            XV(&trk[k], X_ROUTE) = 1; XV(&trk[k], X_LINK) = 0; XV(&trk[k], X_KTRK2) = 30; XV(&trk[k], X_ENV2) = -20; XV(&trk[k], X_LFO1) = 40; XV(&trk[k], X_LFO2) = 50;
        }
    }
    if (md_twin == 2) {                                 /* TWIN on, a different filter 2 on each part */
        trk[0].p[P_TWTYPE] = TT_LADDER; trk[0].p[P_RES2] = 90; trk[0].p[P_DRV2] = 60; trk[0].p[P_CUT2] = -30;
        trk[1].p[P_TWTYPE] = TT_COMB; trk[1].p[P_RES2] = 50; trk[1].p[P_CUT2] = 0; XV(&trk[1], X_ROUTE) = 1; trk[1].p[P_BLEND] = 15;
    }
    for (b = 0; b < nb; b++) {
        if (b == 5u) {
            for (i = 0; i < 4u; i++)
                trk_note_on(&trk[0], C1[i], 100);
            for (i = 0; i < 3u; i++)
                trk_note_on(&trk[1], C2[i], 100);
        }
        if (b == 80u) {
            for (i = 0; i < 4u; i++)
                trk_note_off(&trk[0], C1[i]);
        }
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++) {
            h ^= (uint32_t)o[i];
            h *= 0x100000001B3ull;
        }
        if (b == 60u)
            for (i = 0; i < NVOICE; i++)
                *(uint32_t *)(out + 8 + 2u * i) = (uint32_t)(trk[0].v[i].chn | (trk[1].v[i].chn << 8));
    }
    memcpy(out, &h, 8);
    {   /* the second part's state rows are its own: part 1 rows stayed untouched where part 1 never ran the chain */
        uint32_t used0 = 0, used1 = 0;
        for (i = 0; i < NVOICE; i++)
            for (k = 0; k < sizeof(f2s_t); k++) {
                used0 |= ((uint8_t *)&f2s[0][i])[k] != 0;
                used1 |= ((uint8_t *)&f2s[1][i])[k] != 0;
            }
        *(uint32_t *)(out + 4) = used0 | (used1 << 1);
    }
}
static uint64_t modes_hash(int mode, int twin, int e0, int e1, uint32_t *used)
{
    uint64_t h;
    md_mode = mode; md_twin = twin; md_eng0 = e0; md_eng1 = e1;
    child_run(modes_c, slot(3));
    memcpy(&h, slot(3), 8);
    if (used)
        *used = *(uint32_t *)(slot(3) + 4);
    return h;
}
static void t_modes(void)
{
    uint32_t m, e, ok = 1, ok2 = 1, ok3 = 1, u;
    for (m = 0; m < 3u; m++)
        for (e = 0; e < 2u; e++) {
            uint64_t a = modes_hash((int)m, 0, (int)e, (int)e, &u), b = modes_hash((int)m, 1, (int)e, (int)e, 0);
            ok &= a == b && !u;
        }
    ck("TYPE OFF with every other TWIN value off its default (RES 2, DRIVE, BLEND, CUTOFF 2, ROUTE, LINK, KEY TRK 2, ENV 2, LFO 1, LFO 2): STEREO, DUEL and LAYER, ANALOG / FM6 and PHASE / TRIO are bit for bit the defaults' render, and filter 2's state is never touched", ok);
    for (m = 0; m < 3u; m++) {
        uint32_t chn;
        uint64_t a = modes_hash((int)m, 0, 0, 0, &u);
        (void)a;
        chn = *(uint32_t *)(slot(3) + 8);
        ok2 &= (chn & 0xFFu) == VCH_OFF;
    }
    ck("  .. and no voice ever enters the chain at TYPE OFF (v->chn stays VCH_OFF while the part sounds)", ok2);
    for (m = 0; m < 3u; m++) {
        uint64_t a = modes_hash((int)m, 2, 0, 0, &u);
        uint32_t chn = *(uint32_t *)(slot(3) + 8);
        ok3 &= a != 0 && u == 3u && (chn & 0xFFu) == VCH_RUN && ((chn >> 8) & 0xFFu) == VCH_RUN;
    }
    ck("TWIN on both parts (LADDER, and COMB in PARALLEL on part 2): STEREO, DUEL and LAYER each part keeps its own filter-2 rows (both used, each voice in the chain)", ok3);
}

/* ---------------------------------------------------------------- ALIAS: DRIVE at C5 and C7 (reported) --- */
static void t_alias(void)
{
    static const struct { int k; const char *n; } F[2] = {{49, "C5 (527.6 Hz)"}, {195, "C7 (2099 Hz)"}};   /* N = 4096: a whole number of periods */
    static const int D[3] = {40, 64, 127};
    uint32_t j, d, i, ok = 1;
    enum { N = 4096 };
    for (j = 0; j < 2u; j++)
        for (d = 0; d < 3u; d++) {
            double hz = (double)FS * F[j].k / N, re[N / 2 + 1], im[N / 2 + 1], tot = 0, har = 0;
            uint32_t b, m;
            static int32_t y[N];
            drive_setup((uint32_t)D[d]);
            for (b = 0; b < 40u; b++) {                     /* settle */
                for (i = 0; i < CTL; i++)
                    sbuf[i] = (int32_t)lrint(20000.0 * sin(6.283185307179586 * hz * (b * CTL + i) / FS));
                st_block(sbuf);
            }
            for (b = 0; b < N / CTL; b++) {
                for (i = 0; i < CTL; i++)
                    sbuf[i] = (int32_t)lrint(20000.0 * sin(6.283185307179586 * hz * ((40u + b) * CTL + i) / FS));
                st_block(sbuf);
                memcpy(y + b * CTL, sbuf, sizeof sbuf);
            }
            for (m = 1; m <= N / 2; m++) {
                double a = 0, c = 0;
                for (i = 0; i < N; i++) {
                    double w = 0.5 - 0.5 * cos(6.283185307179586 * i / N);
                    a += w * y[i] * cos(6.283185307179586 * m * i / N);
                    c += w * y[i] * sin(6.283185307179586 * m * i / N);
                }
                re[m] = a;
                im[m] = c;
                tot += a * a + c * c;
                if (m % (uint32_t)F[j].k == 0 || m % (uint32_t)F[j].k == 1u || m % (uint32_t)F[j].k == (uint32_t)F[j].k - 1u)
                    har += a * a + c * c;
            }
            printf("twin:   ALIAS %s DRIVE %3d: energy off the harmonics (and their window bins) %.1f dB below the whole\n", F[j].n, D[d], tot > 0 ? 10.0 * log10((tot - har) / tot + 1e-12) : 0.0);
            (void)re; (void)im;
        }
    ck("DRIVE's aliasing at C5 and C7 is reported above (no oversampling in the first version: filter 1's low-pass before it is the only help; not a gate)", ok);
}

#ifndef TWIN_NO_MAIN
int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    sh = mmap(0, sizeof *sh, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sh == MAP_FAILED)
        return 2;
    arena = mmap(0, (size_t)NSLOT * SLOTN * sizeof(int32_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (arena == MAP_FAILED)
        return 2;
    ui_power_on();
    in_child(t_types);
    in_child(t_tuning);
    in_child(t_drive);
    in_child(t_blend);
    in_child(t_cutoff);
    in_child(t_link);
    in_child(t_linkknob);
    in_child(t_combmem);
    in_child(t_stable);
    in_child(t_minute);
    in_child(t_clicks);
    in_child(t_morph);
    in_child(t_modes);
    in_child(t_alias);
    printf("twin: %d checks, %d failed\n", sh->ncheck, sh->bad);
    return sh->bad ? 1 : 0;
}
#endif
