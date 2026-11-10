/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The REVERB LOW CUT (DUELL 0.2.14; G_RLOCUT, REVERB page K4 "LOCUT"; fx_bus.c rev_locut): a one-pole high-pass on the reverb bus's input in front of
 * all six models, per bus (A, B), glided, 0 = off. On the Mac through hostsim.c as plate_test.c.
 *   build/host/locut_test            (host_tests.sh locut)
 *   cc -DLOCUT_REF ... locut_test.c  prints the hashes of test 1 (these are the ones pasted below: made with the code BEFORE the low cut, at 0e8d8ed)
 * 1. LOCUT 0 is the reverb as before, bit for bit: all six models on bus A (wet and side) and bus B, 8 s of sends, against the hashes of the code before.
 * 2. the control: id, label, range, default (0.2.22: 75 = 299 Hz; 61 = 88 Hz before), the REVERB page's K4, the display (OFF / Hz / kHz from 1000 Hz), the table (exponential, 20 Hz .. 2 kHz, 0.2.22; 20 .. 450 Hz before).
 * 3. the response of the filter itself: the -3 dB corner against the printed Hz over the whole range, 6 dB an octave below it, unity far above it.
 * 4. every model: the 40 Hz and 1 kHz (and 150 Hz) level of the reverb's output with the default low cut against LOCUT 0 (bass down, treble not).
 * 5. no click: the value changed while a bass note rings (jumps across the range, off and on), the step of the filter's output no larger than the signal's own.
 * 6. silence: the filter's output and each model's wet end in exact zeros; OFF restores the input bit for bit after the fade; no DC (PLATE) with it.
 * 7. bus A and B: each its own state; B alone filters as A; SPACE (sv.on) does not bypass it; SENDS OFF's clear drops the state; a model change keeps the filter.
 * 8. cost of the low cut, host instructions per sample (steady, gliding, off). */
#define main hostsim_main
#include "hostsim.c"
#undef main
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif
#ifdef LOCUT_REF
#define LOCUT_ID 21           /* (the code before: the unused G_ENGGO's place; the value is not read there) */
#else
#define LOCUT_ID G_RLOCUT
#endif

static int bad;
static void check(const char *what, int ok)
{
    printf("locut: %-110s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}
static uint32_t xs = 0x1234567u;
static int32_t noise(int32_t amp)
{
    xs ^= xs << 13;
    xs ^= xs >> 17;
    xs ^= xs << 5;
    return (int32_t)(((int64_t)(int32_t)xs * amp) / 2147483648LL);
}

static void start(int m)
{
    host_tracks_init();
    memset(&fx, 0, sizeof fx);
    memset(&hl, 0, sizeof hl);
    rev_clear();
    memset(dly_buf, 0, sizeof dly_buf);
    memset(cho_buf, 0, sizeof cho_buf);
    song.g[G_RTYPE] = (int16_t)m;
    song.g[G_RSIZE] = 90;
    song.g[G_RDAMP] = 60;
    song.g[G_DMDEP] = 0;
    song.g[G_DMODE] = DMD_DIGITAL;
    fx.rtype = (uint8_t)m;
    memset(&sv, 0, sizeof sv);
    sv.sg = sv.sg0 = 32768;
}
static void start_b(int m)
{
    memset(&fx_b, 0, sizeof fx_b);
    memset(&hl_b, 0, sizeof hl_b);
    rev_clear_b();
    memset(cho_buf_b, 0, sizeof cho_buf_b);
    fx_b.rtype = (uint8_t)m;
}

/* the filter on a block, written back over it */
static void rev_locut_inplace(int32_t *x, uint32_t n)
{
    const int32_t *o = rev_locut(x, n);
    uint32_t i;
    if (o != x)
        for (i = 0; i < n; i++)
            x[i] = o[i];
}

/* ------------------------------------------------ 1. LOCUT 0 as before --- */
#ifndef LOCUT_REF
/* made by -DLOCUT_REF with the code before the low cut (0e8d8ed); HALL's (the third) re-made in 0.2.29 (its quiet lines round to the nearest:
 * fx.c hall_end; with LOCUT 0 it is the 0.2.29 HALL bit for bit; before: A 0x2aba341d, B 0xe38e0e45) */
static const uint32_t REF_A[6] = { 0x54f891f4u, 0xb9dbb185u, 0x7964e0edu, 0x145eae7cu, 0x23eaefbau, 0x5213a10bu, };
static const uint32_t REF_B[6] = { 0x54f891f4u, 0xb9dbb185u, 0x15be3bf1u, 0x145eae7cu, 0x23eaefbau, 0xb0f3473cu, };
#endif
static uint32_t run_hash(int model, int bus_b, uint32_t secs)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t b, i, h = 2166136261u, nb = secs * FS / CTL;
    xs = 0x2468ACEu;
    start(model);
    start_b(model);
    song.g[LOCUT_ID] = 0;
    for (b = 0; b < nb; b++) {
        for (i = 0; i < CTL; i++) {
            r[i] = (b / 300u) % 3u != 2u ? noise(b % 2000u < 1000u ? 90000 : 4500) : 0;
            c[i] = d[i] = 0;
        }
        if (bus_b)
            fx_buses_m_b(DLY_HALF - 1u, delay_split(), c, d, r, w, CTL, 0u, 0);
        else
            fx_buses(c, d, r, w, CTL);
        for (i = 0; i < CTL; i++) {
            h = (h ^ (uint32_t)w[i]) * 16777619u;
            h = (h ^ (uint32_t)(bus_b ? 0 : hl.side ? rev_side[i] : 0)) * 16777619u;
        }
    }
    return h;
}
#ifdef LOCUT_REF
int main(void)
{
    int m;
    printf("static const uint32_t REF_A[6] = {");
    for (m = 0; m < 6; m++) printf(" 0x%08xu,", run_hash(m, 0, 8));
    printf(" };\nstatic const uint32_t REF_B[6] = {");
    for (m = 0; m < 6; m++) printf(" 0x%08xu,", run_hash(m, 1, 8));
    printf(" };\n");
    return 0;
}
#else
static const char *const MN[6] = {"ROOM", "SPRING", "HALL", "SHIM", "RESO", "PLATE"};
static void test_zero(void)
{
    int m, ok_a = 1, ok_b = 1;
    char w[160];
    for (m = 0; m < 6; m++) {
        uint32_t a = run_hash(m, 0, 8), b = run_hash(m, 1, 8);
        ok_a &= a == REF_A[m];
        ok_b &= b == REF_B[m];
        snprintf(w, sizeof w, "LOCUT 0, %s: bus A %08x (before: %08x), bus B %08x (before: %08x), bit for bit", MN[m], a, REF_A[m], b, REF_B[m]);
        check(w, a == REF_A[m] && b == REF_B[m]);
    }
    check("LOCUT 0: all six models, both buses, 8 s of sends: the reverb as before the low cut", ok_a && ok_b);
}

/* ------------------------------------------------------- 2. the control --- */
static void test_control(void)
{
    char val[8], w[200];
    const char *unit = "";
    uint32_t i, ok = 1;
    const param_desc_t *d = &GP[G_RLOCUT];
    uint32_t k4 = 0, pg, found = 0;
    for (pg = 0; pg < NELEM(PAGES); pg++)
        if (!strcmp(PAGES[pg].title, "REVERB"))
            k4 = PAGES[pg].id[3], found = PAGES[pg].id[0] == G_RTYPE && PAGES[pg].id[1] == G_RSIZE && PAGES[pg].id[2] == G_RDAMP;
    snprintf(w, sizeof w, "the control: id %d (the old G_ENGGO's, G_COUNT %d), label %s, 0..127, default %d, fmt F_LOCUT", (int)G_RLOCUT, (int)G_COUNT, d->label, d->def);
    check(w, G_RLOCUT == 21 && G_COUNT == 27 && !strcmp(d->label, "LOCUT") && d->min == 0 && d->max == 127 && d->def == 75 && d->fmt == F_LOCUT);
    check("the REVERB page: TYPE SIZE DAMP LOCUT (K4)", k4 == G_RLOCUT && found);
    param_format(d, 0, val, &unit);
    ok &= !strcmp(val, "OFF");
    param_format(d, 1, val, &unit);
    ok &= !strcmp(val, "20") && !strcmp(unit, "Hz");
    param_format(d, 75, val, &unit);
    snprintf(w, sizeof w, "the display: 0 OFF, 1 20 Hz, 75 %s %s, ", val, unit);
    ok &= !strcmp(val, "299") && !strcmp(unit, "Hz");
    param_format(d, 127, val, &unit);
    ok &= !strcmp(val, "2.0") && !strcmp(unit, "kHz");
    param_format(d, 61, val, &unit);
    ok &= !strcmp(val, "179") && !strcmp(unit, "Hz");               /* (the old default's value: 179 Hz now) */
    for (i = 1; i < 128; i++) {                                     /* Hz below 1000, kHz with one decimal from 1000 (the filter CUT's way) */
        param_format(d, (int32_t)i, val, &unit);
        ok &= LOCUT_HZ[i] < 1000u ? !strcmp(unit, "Hz") && (uint32_t)atoi(val) == LOCUT_HZ[i] : !strcmp(unit, "kHz") && strlen(val) == 3 && val[1] == '.';
    }
    param_format(d, 102, val, &unit);
    strcat(w, val); strcat(w, " "); strcat(w, unit);
    strcat(w, " at 102, 127 2.0 kHz");
    check(w, ok);
    ok = LOCUT_HZ[0] == 0 && LOCUT_K[0] == 0;
    for (i = 2; i < 128; i++)
        ok &= LOCUT_HZ[i] >= LOCUT_HZ[i - 1] && (double)LOCUT_HZ[i] / LOCUT_HZ[i - 1] < 1.07 && LOCUT_K[i] > LOCUT_K[i - 1];
    check("the table: 20 Hz .. 2 kHz, rising, a step of 1.07 at most (exponential, 1.037 before the whole-Hz rounding)", ok && LOCUT_HZ[1] == 20 && LOCUT_HZ[127] == 2000);
    {   /* the default is the value whose frequency is closest to 300 Hz */
        uint32_t best = 1;
        for (i = 1; i < 128; i++)
            if (abs((int)LOCUT_HZ[i] - 300) < abs((int)LOCUT_HZ[best] - 300))
                best = i;
        snprintf(w, sizeof w, "the default is the value closest to 300 Hz: %u = %u Hz (the factory multis hold it too)", best, LOCUT_HZ[best]);
        check(w, best == GP[G_RLOCUT].def && best == 75u && LOCUT_HZ[best] == 299u);
    }
}

/* ------------------------------------------- 3. the filter's response --- */
/* the gain (linear) of rev_locut on a sine of f Hz, measured over the last second of 3 */
static double gain_at(int v, double f)
{
    static int32_t x[CTL];
    uint32_t b, i, nb = 3u * FS / CTL;
    double s_in = 0, s_out = 0;
    start(0);
    song.g[LOCUT_ID] = (int16_t)v;
    for (b = 0; b < nb; b++) {
        for (i = 0; i < CTL; i++)
            x[i] = (int32_t)lround(50000.0 * sin(2 * M_PI * f * (double)(b * CTL + i) / FS));
        if (b >= 2u * FS / CTL)
            for (i = 0; i < CTL; i++)
                s_in += (double)x[i] * x[i];
        rev_locut_inplace(x, CTL);
        if (b >= 2u * FS / CTL)
            for (i = 0; i < CTL; i++)
                s_out += (double)x[i] * x[i];
    }
    return sqrt(s_out / s_in);
}
/* |H| of the filter as it is built (y = x - lp, lp += k (x - lp), a = 1 - k): a (1 - 1/z) / (1 - a / z) at f Hz */
static double one_pole_gain(int v, double f)
{
    double a = 1.0 - LOCUT_K[v] / 32768.0, w = 2 * M_PI * f / FS;
    double nr = a * (1 - cos(w)), ni = a * sin(w), dr = 1 - a * cos(w), di = a * sin(w);
    return sqrt((nr * nr + ni * ni) / (dr * dr + di * di));
}
static void test_response(void)
{
    static const int V[] = {1, 21, 41, 61, 81, 101, 127};
    uint32_t vi;
    int ok_c = 1, ok_s = 1, ok_u = 1;
    double worst_c = 0, worst_s = 0, worst_u = 0;
    char w[220];
    for (vi = 0; vi < NELEM(V); vi++) {
        double fc = LOCUT_HZ[V[vi]], lo = fc * 0.5, hi = fc * 2.0, f, g = 0;
        /* the -3 dB point by bisection on the measured gain */
        lo = fc * 0.6;
        hi = fc * 1.6;
        for (f = 0; f < 12; f++) {
            double mid = 0.5 * (lo + hi);
            g = gain_at(V[vi], mid);
            if (g < 0.70710678)
                lo = mid;
            else
                hi = mid;
        }
        double ff = fc * 20 < 18000 ? fc * 20 : 18000, c = 0.5 * (lo + hi), oct = gain_at(V[vi], fc / 2), far = gain_at(V[vi], ff) / one_pole_gain(V[vi], ff);   /* (far: against the one-pole's own response: the discrete design is 1.3 dB down at Nyquist at the 2 kHz corner) */
        double dev = fabs(c / fc - 1.0);
        double sl = 20 * log10(gain_at(V[vi], fc / 4) / gain_at(V[vi], fc / 8));
        worst_c = dev > worst_c ? dev : worst_c;
        worst_s = fabs(sl - 6.0) > worst_s ? fabs(sl - 6.0) : worst_s;
        worst_u = fabs(20 * log10(far)) > worst_u ? fabs(20 * log10(far)) : worst_u;
        ok_c &= dev < 0.06;
        printf("locut:   value %3d: printed %3.0f Hz, measured -3 dB at %6.1f Hz (%+.1f %%), %.2f x at half of it (%.1f dB), %.2f dB per octave below, far above %+.2f dB\n",
               V[vi], fc, c, 100 * (c / fc - 1), oct, 20 * log10(oct), sl, 20 * log10(far));
    }
    ok_s = worst_s < 0.8;
    ok_u = worst_u < 0.4;
    snprintf(w, sizeof w, "corner: the -3 dB point within 6 %% of the printed Hz over the range (worst %.1f %%)", 100 * worst_c);
    check(w, ok_c);
    snprintf(w, sizeof w, "slope: 6 dB an octave an octave and more below the corner (worst %.2f dB off); far above: the one-pole's own response within %.2f dB", worst_s, worst_u);
    check(w, ok_s && ok_u);
}

/* ------------------------------------------ 4. each model's bass --- */
/* rms of the reverb's wet output (left) for a sine f Hz at send 20000 held 3 s (over the last second), LOCUT v on bus A */
static double wet_rms(int m, int v, double f)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t b, i, nb = 4u * FS / CTL;
    double s = 0, n = 0;
    start(m);
    song.g[LOCUT_ID] = (int16_t)v;
    for (b = 0; b < nb; b++) {
        for (i = 0; i < CTL; i++) {
            r[i] = (int32_t)lround(20000.0 * sin(2 * M_PI * f * (double)(b * CTL + i) / FS));
            c[i] = d[i] = 0;
        }
        fx_buses(c, d, r, w, CTL);
        if (b >= 3u * FS / CTL)
            for (i = 0; i < CTL; i++)
                s += (double)w[i] * w[i], n += 1;
    }
    return sqrt(s / n);
}
static void test_models(void)
{
    int m, ok = 1, ok_t = 1;
    char w[300];
    double want40 = 20 * log10(gain_at(75, 40)), want150 = 20 * log10(gain_at(75, 150)), want1k = 20 * log10(gain_at(75, 1000));
    printf("locut:   the filter alone at the default (299 Hz): 40 Hz %.1f dB, 150 Hz %.1f dB\n", want40, want150);
    for (m = 0; m < 6; m++) {
        double a40 = 20 * log10(wet_rms(m, 75, 40) / wet_rms(m, 0, 40)), a150 = 20 * log10(wet_rms(m, 75, 150) / wet_rms(m, 0, 150));
        double a1k = 20 * log10(wet_rms(m, 75, 1000) / wet_rms(m, 0, 1000)), a40m = 20 * log10(wet_rms(m, 127, 40) / wet_rms(m, 0, 40));
        snprintf(w, sizeof w, "%-6s wet level with the default low cut against LOCUT 0: 40 Hz %+.1f dB (the filter: %.1f), 150 Hz %+.1f dB (%.1f), 1 kHz %+.1f dB; LOCUT 127 at 40 Hz %+.1f dB",
                 MN[m], a40, want40, a150, want150, a1k, a40m);
        /* SHIM puts the octave above a bass, RESO rings tuned combs: allow them more; the others follow the filter */
        double tol = m == 3 || m == 4 ? 4.0 : 1.5;
        ok = (a40 < -4.0 && fabs(a40 - want40) < tol && fabs(a1k - want1k) < 0.8 && a40m < a40 - 5.0);   /* (the treble: only what the filter itself takes at 1 kHz, -0.4 dB at 299 Hz) */
        ok_t &= ok;
        check(w, ok);
    }
    check("every model: the bass is down by the filter's amount, the treble untouched", ok_t);
}

/* ------------------------------------------------------ 5. no click --- */
static void test_click(void)
{
    static int32_t x[CTL];
    static const int32_t SEQ[][2] = {{75, 127}, {127, 1}, {1, 75}, {75, 0}, {0, 75}, {75, 31}, {0, 127}, {127, 0}};
    uint32_t s, b, i, nb = 2u * FS / CTL, ok = 1;
    int32_t worst_step = 0, own = 0, prev = 0, raw = 0;
    char w[220];
    for (b = 1; b < FS / 2u; b++) {   /* the signal's own largest step */
        double t0 = (double)(b - 1u) / FS, t1 = (double)b / FS;
        int32_t a0 = (int32_t)lround(30000.0 * sin(2 * M_PI * 62.0 * t0) + 20000.0 * sin(2 * M_PI * 180.0 * t0) + 8000.0 * sin(2 * M_PI * 2000.0 * t0));
        int32_t a1 = (int32_t)lround(30000.0 * sin(2 * M_PI * 62.0 * t1) + 20000.0 * sin(2 * M_PI * 180.0 * t1) + 8000.0 * sin(2 * M_PI * 2000.0 * t1));
        raw = a1 - a0 > raw ? a1 - a0 : a0 - a1 > raw ? a0 - a1 : raw;
    }
    for (s = 0; s < NELEM(SEQ); s++) {
        start(0);
        song.g[LOCUT_ID] = (int16_t)SEQ[s][0];
        prev = 0;
        int32_t st_own = 0, st_chg = 0;
        for (b = 0; b < nb; b++) {
            if (b == nb / 2u)
                song.g[LOCUT_ID] = (int16_t)SEQ[s][1];
            for (i = 0; i < CTL; i++) {
                double t = (double)(b * CTL + i) / FS;
                x[i] = (int32_t)lround(30000.0 * sin(2 * M_PI * 62.0 * t) + 20000.0 * sin(2 * M_PI * 180.0 * t) + 8000.0 * sin(2 * M_PI * 2000.0 * t));
            }
            rev_locut_inplace(x, CTL);
            for (i = 0; i < CTL; i++) {
                int32_t st = x[i] - prev;
                st = st < 0 ? -st : st;
                if (b > nb / 4u && b < nb / 2u)
                    st_own = st > st_own ? st : st_own;
                if (b >= nb / 2u && b < nb / 2u + 2u * FS / CTL / 2u)
                    st_chg = st > st_chg ? st : st_chg;
                prev = x[i];
            }
        }
        /* the step of the output after the change against the signal's own largest step before it */
        own = st_own;
        worst_step = st_chg > worst_step ? st_chg : worst_step;
        ok &= st_chg <= raw + raw / 10;
        printf("locut:   value %3d -> %3d: largest sample step of the output: before %d, after the change %d (the unfiltered signal's own %d)\n", SEQ[s][0], SEQ[s][1], own, st_chg, raw);
    }
    snprintf(w, sizeof w, "no click: LOCUT jumped across its range (and off / on) under a bass + mids + hi signal: no sample step beyond 1.1 x the unfiltered signal's own");
    check(w, ok);
}

/* ---------------------------------------------------------- 6. silence --- */
static void test_silence(void)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t b, i, nb = 40u * FS / CTL;
    int m, ok_all = 1;
    char t[220];
    for (m = 0; m < 6; m++) {
        uint32_t last = 0, sil_in = 0;
        double dc = 0, rms = 0, cnt = 0;
        start(m);
        song.g[LOCUT_ID] = 75;
        xs = 0x777u;
        for (b = 0; b < nb; b++) {
            for (i = 0; i < CTL; i++) {
                r[i] = b < 2u * FS / CTL ? noise(60000) + (int32_t)lround(15000.0 * sin(2 * M_PI * 50.0 * (double)(b * CTL + i) / FS)) + 9000 : 0;
                c[i] = d[i] = 0;
            }
            fx_buses(c, d, r, w, CTL);
            for (i = 0; i < CTL; i++) {
                if (w[i] || (hl.side && rev_side[i]))
                    last = b;
                if (r[i])
                    sil_in = b;
                if (b > 4u * FS / CTL && b < 6u * FS / CTL)
                    dc += w[i], rms += (double)w[i] * w[i], cnt += 1;
            }
        }
        /* ROOM and RESO keep a constant rounding offset in their int16 combs at the end of a tail (-41 and -60 here, the same with LOCUT 0 and no DC in
         * the burst: a quirk of the models from before the low cut, -68 dB), the others end in exact zeros */
        int fixed = m == 0 || m == 4;
        int ok = fixed ? (fabs(dc / cnt) < 70 && fabs(sqrt(rms / cnt) - fabs(dc / cnt)) < 1.0) : (last < nb - 4u * FS / CTL && fabs(dc / cnt) < 0.05 * sqrt(rms / cnt) + 1);
        snprintf(t, sizeof t, "%-6s a burst with a DC offset and a 50 Hz tone, then silence: %s (last non-zero at %.1f s), the tail's DC %.2f of its rms %.0f",
                 MN[m], fixed ? "a constant offset of the model's own at the end (also with LOCUT 0)" : "the wet ends in exact zeros", (double)last * CTL / FS, dc / cnt, sqrt(rms / cnt));
        ok_all &= ok;
        check(t, ok);
        (void)sil_in;
    }
    check("silence: every model ends as it did (exact silence, ROOM / RESO their constant offset) with the low cut on", ok_all);
    {   /* the filter alone: zeros out once the input is zero, from any state; OFF gives the input back bit for bit */
        static int32_t x[CTL], y[CTL];
        uint32_t zeros = 0, same = 1;
        start(0);
        song.g[LOCUT_ID] = 75;
        for (b = 0; b < 200; b++) {
            for (i = 0; i < CTL; i++)
                x[i] = 40000 + noise(40000);
            rev_locut_inplace(x, CTL);
        }
        for (b = 0; b < 40000u / CTL; b++) {
            for (i = 0; i < CTL; i++)
                x[i] = 0;
            rev_locut_inplace(x, CTL);
        }
        for (i = 0; i < CTL; i++)
            zeros += x[i] == 0;
        check("the filter alone: input stops after a loud DC + noise: 0.9 s later the output is exactly 0", zeros == CTL);
        song.g[LOCUT_ID] = 0;
        for (b = 0; b < 4000u / CTL * 2u; b++) {
            for (i = 0; i < CTL; i++)
                x[i] = y[i] = noise(50000);
            rev_locut_inplace(x, CTL);
        }
        for (b = 0; b < 8; b++) {
            for (i = 0; i < CTL; i++)
                x[i] = y[i] = noise(50000);
            rev_locut_inplace(x, CTL);
            for (i = 0; i < CTL; i++)
                same &= x[i] == y[i];
        }
        check("OFF again: after its fade (1024 samples) the block is the input bit for bit, the state dropped", same && !fx.lo_g && !fx.lo_lp && !fx.lo_k);
    }
}

/* ------------------------------------------------- 7. buses, SPACE, ... --- */
static void test_buses(void)
{
    static int32_t c[CTL], d[CTL], r[CTL], ra[CTL], rb[CTL], w[CTL], wa[CTL];
    uint32_t b, i, same = 1, wb_ok = 1;
    char t[200];
    int m;
    for (m = 0; m < 6; m += 5) {   /* ROOM and PLATE */
        double sa = 0, sb = 0;
        start(m);
        start_b(m);
        song.g[LOCUT_ID] = 75;
        xs = 0xBEEFu;
        for (b = 0; b < 3u * FS / CTL; b++) {
            for (i = 0; i < CTL; i++)
                r[i] = ra[i] = rb[i] = (int32_t)lround(30000.0 * sin(2 * M_PI * 45.0 * (double)(b * CTL + i) / FS)) + noise(8000);
            for (i = 0; i < CTL; i++)
                c[i] = d[i] = 0;
            fx_buses(c, d, ra, wa, CTL);
            for (i = 0; i < CTL; i++)
                c[i] = d[i] = 0;
            fx_buses_m_b(DLY_HALF - 1u, delay_split(), c, d, rb, w, CTL, 0u, 0);
            for (i = 0; i < CTL; i++) {
                sa += (double)wa[i] * wa[i];
                sb += (double)w[i] * w[i];
            }
        }
        wb_ok &= fx.lo_g == 4096 && fx_b.lo_g == 4096 && fx.lo_lp == fx_b.lo_lp;
        snprintf(t, sizeof t, "SPLIT, %s: bus A and bus B each filter their own send (same state, same wet energy A %.0f B %.0f)", MN[m], sqrt(sa / CTL), sqrt(sb / CTL));
        check(t, wb_ok && fabs(sa - sb) < 1e-9 * sa + 1e6);
    }
    /* B alone, A silent: only B's state moves */
    start(0);
    start_b(0);
    song.g[LOCUT_ID] = 75;
    for (i = 0; i < CTL; i++)
        c[i] = d[i] = 0, r[i] = 30000;
    fx_buses_m_b(DLY_HALF - 1u, delay_split(), c, d, r, w, CTL, 0u, 0);
    check("bus B alone: A's filter state stays at rest (they do not share one)", fx.lo_g == 0 && fx.lo_lp == 0 && fx_b.lo_g > 0);
    /* SPACE: sv.on with its own values: the low cut still on (it reads song.g, no override) */
    {
        double on = 0, off = 0;
        int v;
        for (v = 0; v < 2; v++) {
            double s = 0;
            start(0);
            memset(&sv, 0, sizeof sv);
            sv.on = 1;
            sv.rsize = 120;
            sv.sg = sv.sg0 = 32768;
            song.g[LOCUT_ID] = v ? 75 : 0;
            for (b = 0; b < 3u * FS / CTL; b++) {
                for (i = 0; i < CTL; i++)
                    r[i] = (int32_t)lround(20000.0 * sin(2 * M_PI * 40.0 * (double)(b * CTL + i) / FS)), c[i] = d[i] = 0;
                fx_buses(c, d, r, w, CTL);
                if (b > 2u * FS / CTL)
                    for (i = 0; i < CTL; i++)
                        s += (double)w[i] * w[i];
            }
            if (v) on = s; else off = s;
        }
        snprintf(t, sizeof t, "SPACE on (sv.on, its own SIZE): the low cut still works, 40 Hz %.1f dB", 10 * log10(on / off));
        check(t, on < off * 0.4);
    }
    /* SENDS OFF's line clear drops the state; a model change keeps it (no step) */
    start(0);
    song.g[LOCUT_ID] = 75;
    for (b = 0; b < 40; b++) {
        for (i = 0; i < CTL; i++)
            r[i] = 30000, c[i] = d[i] = 0;
        fx_buses(c, d, r, w, CTL);
    }
    {
        int32_t lp0 = fx.lo_lp;
        song.g[G_RTYPE] = 2;
        for (i = 0; i < CTL; i++)
            r[i] = 30000, c[i] = d[i] = 0;
        fx_buses(c, d, r, w, CTL);
        check("a model change keeps the low cut running (its state is not cleared with the lines)", fx.lo_g == 4096 && fx.lo_lp != 0 && lp0 != 0);
        snd_lines_clear();
        check("MODULES SENDS OFF's line clear drops the filter state (an ON starts from rest)", !fx.lo_lp && !fx.lo_k && !fx.lo_g && !fx_b.lo_g);
    }
    (void)same;
}

/* ------------------------------------------------------------- 8. cost --- */
static double cost_of(int v, int glide)
{
    static int32_t x[CTL], y[CTL];
    uint32_t b, i, nb = 6000u;
    uint64_t i0, i1;
    start(0);
    song.g[LOCUT_ID] = (int16_t)v;
    for (i = 0; i < CTL; i++)
        y[i] = (int32_t)(i * 997u % 20011u) - 10000;
    i0 = instr_now();
    for (b = 0; b < nb; b++) {
        if (glide && (b % 8u) == 0u)
            song.g[LOCUT_ID] = (int16_t)(b / 8u % 2u ? 30 : 120);   /* (the knob moved every 8 blocks: mostly gliding) */
        memcpy(x, y, sizeof x);
        rev_locut_inplace(x, CTL);
    }
    i1 = instr_now();
    return (double)(i1 - i0) / ((double)nb * CTL);
}
static void test_cost(void)
{
    double st = cost_of(75, 0), gl = cost_of(75, 1), off = cost_of(0, 0);   /* (off: the loop's own memcpy and call) */
    char w[220];
    if (!st) {
        printf("locut: cost not measured (no instruction counter)\n");
        return;
    }
    snprintf(w, sizeof w, "cost, host instructions per sample a bus: steady %.1f, gliding %.1f, OFF %.2f; the low cut alone (less the block copy) steady %.1f, gliding %.1f (device CPU 1.7 %% per 100: %.2f %%)",
             st, gl, off, st - off, gl - off, (st - off) * 0.017);
    check(w, st - off < 14 && off < 3.0);
}

int main(void)
{
    test_zero();
    test_control();
    test_response();
    test_models();
    test_click();
    test_silence();
    test_buses();
    test_cost();
    if (bad) {
        printf("LOCUT TEST FAILED\n");
        return 1;
    }
    printf("LOCUT TEST PASSED\n");
    return 0;
}
#endif
