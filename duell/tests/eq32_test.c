/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.25: the part EQ's filter in 32-bit multiplies (fx.c eq_run). The chip (pi32v2) has a 32 x 32 -> 64 multiply-accumulate but no
 * 64 x 64 multiply: 0.2.24's loop (64-bit coefficients times 64-bit samples, a 64-bit clamp) cost ~85 device instructions a sample a band.
 * Now: Q28 coefficients in int32 (eq_design: the same values as before), the sample and the output in Q8 (32 bit), the two states 64 bit
 * (Q36, only added to), the output's rounding remainder carried into the next sample (first-order error feedback).
 * Checked here against a verbatim copy of 0.2.24's 64-bit loop (old_run) and an ideal double-precision filter:
 *   RANGE     every setting (25 gains x 49 / 37 frequencies x 25 Q): each coefficient fits int32, and the sum of their sizes < 16, so the 64-bit
 *             state and sum can not overflow at full-scale input with the output at its clamp (|x|, |y| <= 2^31 in Q8: 16 x 2^59 = 2^63)
 *   OVERFLOW  three bands at +12 dB at every frequency end (and mixed), full-scale square / white / sweep: bit for bit a copy of the loop
 *             with 128-bit sums (no 64-bit wrap anywhere), the output inside its 24-bit clamp
 *   LEVELS    a part's signal is on the mix's 16-bit scale (0 dBFS = 32768, an LSB = one step of the 16-bit output at unity); the EQ clamps
 *             at +-2^23 (48 dB above), so OVERFLOW drives it at the clamp, the others at 0, -20 and -60 dBFS of the mix scale
 *   OLD       tones, a log sweep 20 Hz .. 20 kHz, white noise and a DC step at 0, -20 and -60 dBFS through each band at the ends of its ranges:
 *             the largest difference to 0.2.24's loop in LSB and in dB below the signal
 *   IDEAL     the same against the ideal (double) filter of the same coefficients: the new loop's error, and the old's
 *   RESPONSE  the gain of steady sines (1/3 octave, 25 Hz .. 16 kHz) through each band's extremes against the exact RBJ response, in dB
 *   SILENCE   after a signal the output comes back to exactly 0 at every setting (0.2.24's could keep a DC of a few LSB on a low LOW band
 *             until the part's tail ended: its rounding error was not carried), a 3 LSB tone at the lowest and most resonant
 *             settings shows no limit cycle (the output 0 within 0.2 s of the end)
 * Built and run by DUELL/host_tests.sh (name: eq32). Prints the figures (docs/TESTING.md "CPU on the unit" quotes them) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <math.h>

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    printf("eq32: %s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        bad++;
    return ok;
}

/* 0.2.24's loop, verbatim (64-bit coefficients and states, Q28; the output Q8 clamped, then rounded) */
static void old_run(const eqc_t *c32, int64_t *s, int32_t *b, uint32_t n)
{
    const int64_t b0 = c32->b0, b1 = c32->b1, b2 = c32->b2, a1 = c32->a1, a2 = c32->a2;
    uint32_t i;
    int64_t s1 = s[0], s2 = s[1];
    for (i = 0; i < n; i++) {
        int64_t x = clamp(b[i], -8388608, 8388607), y = b0 * x + s1;
        y = y >> 20;
        y = y > 2147483647ll ? 2147483647ll : y < -2147483647ll ? -2147483647ll : y;
        s1 = b1 * x - ((a1 * y) >> 8) + s2;
        s2 = b2 * x - ((a2 * y) >> 8);
        b[i] = (int32_t)((y + 128) >> 8);
    }
    s[0] = s1, s[1] = s2;
}
/* the new loop with 128-bit sums: the same integers as eq_run while nothing wraps */
static void wide_run(const eqc_t *c, __int128 *s, int32_t *b, uint32_t n)
{
    uint32_t i;
    __int128 s1 = s[0], s2 = s[1];
    for (i = 0; i < n; i++) {
        int32_t x = clamp(b[i], -8388608, 8388607) * 256, y;
        __int128 a = s1 + (__int128)c->b0 * x, q = a >> 28;
        y = q > 2147483647 ? 2147483647 : q < -2147483648ll ? (int32_t)-2147483648ll : (int32_t)q;
        if (q > 2147483647 || q < -2147483648ll)        /* (eq_run clamps the high word: the low 4 bits of y are the sum's) */
            y = (int32_t)(((uint32_t)(q > 0 ? 2147483647 : 0x80000000u) & ~15u) | ((uint32_t)(uint64_t)a >> 28 & 15u));
        s1 = s2 + (int64_t)((uint64_t)a & 0x0FFFFFFFu) + (__int128)c->b1 * x - (__int128)c->a1 * y;
        s2 = (__int128)c->b2 * x - (__int128)c->a2 * y;
        b[i] = (y >> 8) + ((y >> 7) & 1);
    }
    s[0] = s1, s[1] = s2;
}
/* the ideal filter of the same coefficients (double, no clamp) */
static void ideal_run(const eqc_t *c, double *s, const int32_t *x, double *y, uint32_t n)
{
    const double b0 = c->b0 / 268435456.0, b1 = c->b1 / 268435456.0, b2 = c->b2 / 268435456.0, a1 = c->a1 / 268435456.0, a2 = c->a2 / 268435456.0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double v = b0 * x[i] + s[0];
        s[0] = b1 * x[i] - a1 * v + s[1];
        s[1] = b2 * x[i] - a2 * v;
        y[i] = v;
    }
}
/* the exact RBJ response of band bd at its settings (exact sin / cos, the frequency of eq_hz_q8), dB at f Hz */
static double rbj_db(uint32_t bd, int32_t g, int32_t fr, int32_t q, double f)
{
    double w = 2 * M_PI * (eq_hz_q8(bd, fr) / 256.0) / FS, A = pow(10, g / 40.0), cw = cos(w), sw = sin(w), c[6];
    if (bd == 1u) {
        double al = sw * pow(2, -q / 6.0) / 2;
        c[0] = 1 + al * A, c[1] = -2 * cw, c[2] = 1 - al * A, c[5] = 1 + al / A, c[3] = -2 * cw, c[4] = 1 - al / A;
    } else {
        double T = sqrt(2.0) * sqrt(A) * sw, s0 = bd == 0u ? -1 : 1;
        c[0] = A * ((A + 1) + s0 * (A - 1) * cw + T), c[1] = -s0 * 2 * A * ((A - 1) + s0 * (A + 1) * cw), c[2] = A * ((A + 1) + s0 * (A - 1) * cw - T);
        c[5] = (A + 1) - s0 * (A - 1) * cw + T, c[3] = s0 * 2 * ((A - 1) - s0 * (A + 1) * cw), c[4] = (A + 1) - s0 * (A - 1) * cw - T;
    }
    {
        double v = 2 * M_PI * f / FS, cr = cos(v), ci = -sin(v), c2r = cos(2 * v), c2i = -sin(2 * v);
        double nr = c[0] + c[1] * cr + c[2] * c2r, ni = c[1] * ci + c[2] * c2i, dr = c[5] + c[3] * cr + c[4] * c2r, di = c[3] * ci + c[4] * c2i;
        return 10 * log10((nr * nr + ni * ni) / (dr * dr + di * di));
    }
}

#define NS (44096u)                                      /* (a whole number of blocks) */
static int32_t xin[NS], yo[NS], yw[NS], yv[NS];
static double yd[NS];
static uint32_t eq_rs = 0x2545F491u;
static uint32_t xr(void) { eq_rs ^= eq_rs << 13; eq_rs ^= eq_rs >> 17; eq_rs ^= eq_rs << 5; return eq_rs; }
static double xu(void) { return (xr() >> 8) * (2.0 / 16777216.0) - 1.0; }
static void make_sig(uint32_t sig, double amp)
{
    uint32_t i;
    double ph = 0;
    for (i = 0; i < NS; i++) {
        double v;
        if (sig == 0) { ph += 2 * M_PI * 20.0 * pow(1000.0, (double)i / NS) / FS; v = sin(ph); }   /* log sweep 20 Hz .. 20 kHz */
        else if (sig == 1) v = xu();                                                             /* white */
        else if (sig == 2) v = (i / 50u) & 1u ? 1 : -1;                                          /* square 441 Hz */
        else if (sig == 3) v = i < NS / 2u ? 1 : 0;                                              /* a DC step, then silence */
        else v = sin(2 * M_PI * 997.0 * i / FS);                                                 /* a 997 Hz tone */
        v *= amp;
        xin[i] = (int32_t)lrint(v > 8388607 ? 8388607 : v < -8388608 ? -8388608 : v);
    }
}
static void run_new(const eqc_t *c, int32_t *y) { int64_t s[2] = {0, 0}; uint32_t i; memcpy(y, xin, sizeof xin); for (i = 0; i < NS; i += CTL) eq_run(c, s, y + i, CTL); }
static void run_old(const eqc_t *c, int32_t *y) { int64_t s[2] = {0, 0}; uint32_t i; memcpy(y, xin, sizeof xin); for (i = 0; i < NS; i += CTL) old_run(c, s, y + i, CTL); }
static int32_t fmax_of(uint32_t bd) { return bd == 2u ? 12 : 24; }

int main(void)
{
    uint32_t bd, sig, i, k;
    int32_t g, fr, q;
    setvbuf(stdout, NULL, _IONBF, 0);

    {                                                   /* RANGE */
        double big = 0, sum = 0;
        uint32_t fit = 1;
        for (bd = 0; bd < 3u; bd++)
            for (g = -12; g <= 12; g++)
                for (fr = -24; fr <= fmax_of(bd); fr++)
                    for (q = bd == 1u ? -12 : 0; q <= (bd == 1u ? 12 : 0); q++) {
                        eqc_t c;
                        const int32_t *v = &c.b0;
                        double s = 0;
                        eq_design(&c, bd, g, fr, q);
                        for (k = 0; k < 5u; k++) {
                            double d = fabs(v[k] / 268435456.0);
                            fit &= v[k] != INT32_MAX && v[k] != INT32_MIN;   /* (eq_design's saturation never reached) */
                            big = d > big ? d : big;
                            s += d;
                        }
                        sum = s > sum ? s : sum;
                    }
        printf("eq32: RANGE largest coefficient %.3f (Q28: %.3g of 2^31), largest sum of the five %.3f (< 16: no 64-bit overflow)\n", big, big * 268435456.0 / 2147483648.0, sum);
        expect("RANGE: every coefficient of every setting fits int32 in Q28 (eq_design's clamp never acts)", fit && big < 8.0);
        expect("RANGE: the five coefficients' sizes sum to < 16 at every setting (state and sum within int64 at full scale)", sum < 16.0);
    }

    {                                                   /* OVERFLOW: full scale, all gains at +12, every frequency end; the 128-bit copy */
        static const int32_t FE[3][2] = {{-24, 24}, {-24, 24}, {-24, 12}};
        uint32_t same = 1, inside = 1, flips = 0, flips_o = 0, e;
        for (e = 0; e < 8u; e++)
            for (q = -12; q <= 12; q += 12)
                for (sig = 0; sig < 3u; sig++) {
                    eqc_t c[3];
                    int64_t s[3][2] = {{0}}, so[3][2] = {{0}};
                    __int128 w[3][2] = {{0}};
                    for (bd = 0; bd < 3u; bd++)
                        eq_design(&c[bd], bd, 12, FE[bd][(e >> bd) & 1u], bd == 1u ? q : 0);
                    make_sig(sig, 8388607.0);
                    memcpy(yv, xin, sizeof xin), memcpy(yw, xin, sizeof xin), memcpy(yo, xin, sizeof xin);
                    for (i = 0; i < NS; i += CTL)
                        for (bd = 0; bd < 3u; bd++)
                            eq_run(&c[bd], s[bd], yv + i, CTL), wide_run(&c[bd], w[bd], yw + i, CTL), old_run(&c[bd], so[bd], yo + i, CTL);
                    for (i = 0; i < NS; i++) {
                        same &= yv[i] == yw[i];
                        inside &= yv[i] >= -8388608 && yv[i] <= 8388608;
                        if (i && (xin[i] > 0) == (xin[i - 1] > 0)) {   /* (rail to rail while the input keeps its sign) */
                            flips += (yv[i] > 8000000 && yv[i - 1] < -8000000) || (yv[i] < -8000000 && yv[i - 1] > 8000000);
                            flips_o += (yo[i] > 8000000 && yo[i - 1] < -8000000) || (yo[i] < -8000000 && yo[i - 1] > 8000000);
                        }
                    }
                }
        printf("eq32: OVERFLOW full scale, three bands at +12: rail-to-rail steps while the input keeps its sign (the clamp's saturating feedback, as before): new %u, 0.2.24 %u, in %u samples\n",
               flips, flips_o, 8u * 3u * 3u * NS);
        expect("OVERFLOW: three bands at +12 dB, every frequency end and Q end, full-scale sweep / white / square: bit for bit the 128-bit copy (nothing wraps)", same);
        expect("  .. the output stays within 24 bits (+-8388608); rail-to-rail steps no more often than 0.2.24's (+10 %)", inside && flips <= flips_o + flips_o / 10u);
    }

    {                                                   /* OLD and IDEAL: each band at the ends of its ranges, 0, -20 and -60 dBFS */
        static const double AMP[3] = {32767.0, 3276.8, 32.768};
        static const char *const AN[3] = {"0", "-20", "-60"};
        static const int32_t GS[4] = {-12, -1, 1, 12};
        for (k = 0; k < 3u; k++) {
            double dmax = 0, rel = -999, emax_n = 0, emax_o = 0, reln = -999, relo = -999, db_[3] = {0, 0, 0};
            for (bd = 0; bd < 3u; bd++)
                for (g = 0; g < 4; g++)
                    for (fr = -24; fr <= fmax_of(bd); fr += 12)
                        for (q = bd == 1u ? -12 : 0; q <= (bd == 1u ? 12 : 0); q += 6)
                            for (sig = 0; sig < 5u; sig++) {
                                eqc_t c;
                                double sd[2] = {0, 0}, e2 = 0, en = 0, eo = 0, p2 = 0;
                                eq_design(&c, bd, GS[g], fr, q);
                                make_sig(sig, AMP[k]);
                                run_new(&c, yv);
                                run_old(&c, yo);
                                ideal_run(&c, sd, xin, yd, NS);
                                for (i = 0; i < NS; i++) {
                                    double d = (double)yv[i] - yo[i], dn = yv[i] - yd[i], dd = yo[i] - yd[i];
                                    dmax = fabs(d) > dmax ? fabs(d) : dmax;
                                    db_[bd] = fabs(d) > db_[bd] ? fabs(d) : db_[bd];
                                    emax_n = fabs(dn) > emax_n ? fabs(dn) : emax_n;
                                    emax_o = fabs(dd) > emax_o ? fabs(dd) : emax_o;
                                    e2 += d * d, en += dn * dn, eo += dd * dd, p2 += yd[i] * yd[i];
                                }
                                if (p2 > 0) {
                                    double a = 10 * log10(e2 / p2 + 1e-30), b = 10 * log10(en / p2 + 1e-30), c2 = 10 * log10(eo / p2 + 1e-30);
                                    rel = a > rel ? a : rel, reln = b > reln ? b : reln, relo = c2 > relo ? c2 : relo;
                                }
                            }
            printf("eq32: %s dBFS: new against 0.2.24 at most %.0f LSB, the difference at most %.1f dB below the signal; against the ideal: new %.1f LSB (%.1f dB below), 0.2.24 %.1f LSB (%.1f dB below)\n",
                   AN[k], dmax, -rel, emax_n, -reln, emax_o, -relo);
            printf("eq32:   .. new against 0.2.24 by band: LOW %.0f, MID %.0f, HIGH %.0f LSB\n", db_[0], db_[1], db_[2]);
            expect(k == 0u ? "OLD at 0 dBFS: MID and HIGH within 5 LSB of 0.2.24; the largest difference (LOW) no larger than 0.2.24's own error against the ideal (+2 LSB)" :
                   k == 1u ? "OLD at -20 dBFS: the same" : "OLD at -60 dBFS: the same", db_[1] <= 5.0 && db_[2] <= 5.0 && dmax <= emax_o + 2.0);
            if (!k)
                expect("IDEAL at 0 dBFS: the new loop within 2 LSB of the ideal filter and at least 80 dB below the signal", emax_n <= 2.0 && reln <= -80.0);
            else
                expect(k == 1u ? "IDEAL at -20 dBFS: the new loop within 2 LSB of the ideal filter" : "IDEAL at -60 dBFS: the new loop within 2 LSB of the ideal filter", emax_n <= 2.0);
        }
    }

    {                                                   /* RESPONSE: steady sines through each band's extremes against the exact RBJ curve */
        static const int32_t GS[2] = {-12, 12};
        double worst = 0, worst_o = 0, wf = 0;
        uint32_t wb = 0;
        for (bd = 0; bd < 3u; bd++)
            for (g = 0; g < 2; g++)
                for (fr = -24; fr <= fmax_of(bd); fr += fmax_of(bd) + 24)
                    for (q = bd == 1u ? -12 : 0; q <= (bd == 1u ? 12 : 0); q += 24) {
                        eqc_t c;
                        double f;
                        eq_design(&c, bd, GS[g], fr, q);
                        for (f = 25.0; f < 16500.0; f *= 1.2599210498948732) {
                            double pi = 0, pn = 0, po = 0, mdb, odb, ref;
                            for (i = 0; i < NS; i++)
                                xin[i] = (int32_t)lrint(16384.0 * sin(2 * M_PI * f * i / FS));
                            run_new(&c, yv);
                            run_old(&c, yo);
                            for (i = NS / 2u; i < NS; i++)
                                pi += (double)xin[i] * xin[i], pn += (double)yv[i] * yv[i], po += (double)yo[i] * yo[i];
                            ref = rbj_db(bd, GS[g], fr, q, f);
                            mdb = 10 * log10(pn / pi) - ref, odb = 10 * log10(po / pi) - ref;
                            if (fabs(mdb) > worst) worst = fabs(mdb), wf = f, wb = bd;
                            worst_o = fabs(odb) > worst_o ? fabs(odb) : worst_o;
                        }
                    }
        printf("eq32: RESPONSE against the exact RBJ curve (-6 dBFS, 1/3 octave 25 Hz..16 kHz, gains +-12 at the frequency and Q ends): new at most %.3f dB off (band %u, %.0f Hz), 0.2.24 %.3f dB\n",
               worst, wb, wf, worst_o);
        expect("RESPONSE: within 0.1 dB of the exact response everywhere (the design's Taylor sine and the Q28 rounding included)", worst < 0.1);
    }

    {                                                   /* SILENCE and limit cycles */
        uint32_t worst = 0, stuck_o = 0, lc = 0;
        for (bd = 0; bd < 3u; bd++)
            for (g = -12; g <= 12; g += 2)
                for (fr = -24; fr <= fmax_of(bd); fr += 4)
                    for (q = bd == 1u ? -12 : 0; q <= (bd == 1u ? 12 : 0); q += 4) {
                        eqc_t c;
                        uint32_t last = 0, so = 0;
                        if (!g)
                            continue;
                        eq_design(&c, bd, g, fr, q);
                        for (i = 0; i < NS; i++)
                            xin[i] = i < NS / 4u ? (int32_t)(xu() * 16384.0) : 0;
                        run_new(&c, yv);
                        run_old(&c, yo);
                        for (i = NS / 4u; i < NS; i++) {
                            if (yv[i])
                                last = i - NS / 4u;
                            if (i >= NS - 4410u && yo[i])
                                so = (uint32_t)abs(yo[i]) > so ? (uint32_t)abs(yo[i]) : so;
                        }
                        worst = last > worst ? last : worst;
                        stuck_o = so > stuck_o ? so : stuck_o;
                    }
        for (k = 0; k < 4u; k++) {                      /* a 3 LSB tone at the lowest LOW (both gains), the lowest MID at Q 4 (both gains) */
            eqc_t c;
            uint32_t last = 0;
            eq_design(&c, k < 2u ? 0u : 1u, k & 1u ? 12 : -12, -24, k < 2u ? 0 : 12);
            for (i = 0; i < NS; i++)
                xin[i] = i < NS / 2u ? (int32_t)lrint(3.0 * sin(2 * M_PI * (k < 2u ? 31.0 : 251.0) * i / FS)) : 0;
            run_new(&c, yv);
            for (i = NS / 2u; i < NS; i++)
                if (yv[i])
                    last = i - NS / 2u;
            lc = last > lc ? last : lc;
        }
        printf("eq32: SILENCE the output is exactly 0 at most %.1f ms after the input stops (every setting, -6 dBFS noise before); 0.2.24 kept up to %u LSB 1.9 s later; a 3 LSB tone: 0 after %.1f ms\n",
               worst * 1000.0 / FS, stuck_o, lc * 1000.0 / FS);
        expect("SILENCE: at every setting the output is exactly 0 within 200 ms of the input stopping (no stuck DC, no limit cycle)", worst < FS / 5u);
        expect("  .. a 3 LSB tone at the lowest LOW and the lowest, sharpest MID: the output 0 within 200 ms of its end", lc < FS / 5u);
    }

    printf("eq32: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
