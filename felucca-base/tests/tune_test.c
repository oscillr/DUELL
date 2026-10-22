/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.22: MASTER TUNE in Hz (core.h TUNE_MIN .. TUNE_MAX, G_TUNE = the frequency of A4 in 0.1 Hz; voice.c tune_get; the menu row and
 * the editor's id 22: ui_test.c, editor_test.c; the project, autosave and ERASE DATA: ui_test.c, persistence_test.c, erase_test.c):
 *   1. 440.0 Hz is TUNE 0 of before: bit for bit the same render as a value that is no tuning at all (0, an older project's cents); the goldens (regress, mrender) say the same
 *      against the renders of 0.2.21;
 *   2. the offset in cents (tune_get) for all 510 values against 1200 log2(f / 440): within half a step of the 1/4096 grid (0.211 ct); the ends 415.3 (-100.0 ct) and 466.2 (+100.1);
 *   3. the pitch of a rendered ANALOG sine (zero crossings over 2 s) at 432.0, 442.0, 415.3, 466.2 Hz: the cents against 440.0 are the exact ones within 0.25 ct;
 *   4. the texts: '440.0' Hz in the descriptor, the menu row; a value out of range reads 440.0.
 *   build/host/tune_test            (DUELL/host_tests.sh "tune") */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("tune: %-120s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static int32_t out_buf[2 * CTL];
static uint64_t hash;
static float ren_l[2 * FS];
static void blocks(uint32_t n, int keep, uint32_t at)
{
    uint32_t i, k;
    while (n--) {
        mix_block(out_buf, CTL);
        for (i = 0; i < 2u * CTL; i++)
            for (k = 0; k < 4u; k++) {
                hash ^= ((uint32_t)out_buf[i] >> (8u * k)) & 0xFFu;
                hash *= 0x100000001B3ull;
            }
        if (keep)
            for (i = 0; i < CTL; i++)
                ren_l[at + i] = (float)out_buf[2u * i];
        at += CTL;
    }
}
static void fresh(void)
{
    uint32_t k;
    memset(dly_buf, 0, sizeof dly_buf);
    memset(cho_buf, 0, sizeof cho_buf);
    memset(rev_comb, 0, sizeof rev_comb);
    memset(rev_ap, 0, sizeof rev_ap);
    memset(&fx, 0, sizeof fx);
    lim_env = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    vage = 0;
    rng_state = 0x1234567u;
    mod_seed = 0x2545F491u;
    memset(trk, 0, sizeof trk);
    memset(&trk[0].mdw, 0, sizeof trk[0].mdw);
    host_tracks_init();
    for (k = 0; k < NPART; k++)
        host_preset(&trk[k], k ? 0u : 0u, 0u);
    song.sel = 0;
    hash = 0xCBF29CE484222325ull;
}

/* ANALOG, a sine (WAVE 3), no sends, note n held 2.5 s; returns the hash of the whole render; ren_l holds the left channel from 0.4 s */
static uint64_t sine_note(uint32_t note, int32_t tune)
{
    track_t *t = &trk[0];
    fresh();
    song.g[G_TUNE] = (int16_t)tune;
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_VOICE] = V_POLY;
    t->p[P_E0] = 3;                                       /* SIN */
    t->p[P_E1] = 0;
    t->p[P_E2] = 0;
    t->p[P_E4] = 127;
    t->p[P_E5] = 0;
    t->p[P_E3] = 0;
    t->p[P_E6] = 0;
    t->p[P_ED_FLT] = 0;
    t->p[P_ATK] = 0;
    t->p[P_SUS] = 127;
    trk_note_on(t, note, 100);
    blocks(FS * 4u / 10u / CTL, 0, 0);
    blocks(2u * FS / CTL, 1, 0);
    trk_note_off(t, note);
    blocks(FS / 4u / CTL, 0, 0);
    return hash;
}
/* the frequency of ren_l over its first 2 s: rising zero crossings, linear interpolation, first to last */
static double measure(void)
{
    uint32_t i, n = 2u * FS / CTL * CTL, first = 0, last = 0, cnt = 0;
    double t0 = 0, t1 = 0;
    for (i = 1; i < n; i++)
        if (ren_l[i - 1] < 0 && ren_l[i] >= 0) {
            double t = (double)(i - 1) + (double)(-ren_l[i - 1]) / (double)(ren_l[i] - ren_l[i - 1]);
            if (!cnt) { t0 = t; first = i; }
            t1 = t; last = i; cnt++;
        }
    (void)first; (void)last;
    return cnt > 1 ? (double)(cnt - 1) * FS / (t1 - t0) : 0.0;
}
static double cents(double a, double b) { return 1200.0 * log2(a / b); }

int main(void)
{
    uint32_t g;
    double worst = 0, f0, fx, want;
    uint64_t h0, h1, h2, h3;
    char msg[200], v[16];
    const char *unit;
    static const int32_t T[4] = {4320, 4420, 4153, 4662};
    uint32_t k;

    /* 1. 440.0 Hz = TUNE 0 */
    h0 = sine_note(69, TUNE_DEF);
    h1 = sine_note(69, 0);                                /* (the stored 0 of an older project / a stray write: no tuning) */
    h2 = sine_note(69, -5);
    h3 = sine_note(69, 4401);
    check("440.0 Hz renders bit for bit what a value outside the range (0, -5: the old TUNE 0) renders, and 440.1 differs", h0 == h1 && h0 == h2 && h0 != h3);
    f0 = (sine_note(69, TUNE_DEF), measure());
    snprintf(msg, sizeof msg, "  the sine at 440.0 Hz plays A4 at %.3f Hz (the engine's own, the reference of the others)", f0);
    check(msg, f0 > 438.0 && f0 < 442.0);

    /* 2. tune_get for every value */
    for (g = TUNE_MIN; g <= TUNE_MAX; g++) {
        int32_t u, f;
        double got, exact = 1200.0 * log2((double)g / 4400.0);
        double e;
        song.g[G_TUNE] = (int16_t)g;
        tune_get(&u, &f);
        got = u * 6.25 + f / 2.367;
        e = fabs(got - exact);
        if (e > worst) worst = e;
    }
    snprintf(msg, sizeof msg, "tune_get: all 510 values within %.3f ct of 1200 log2(f / 440) (the 1/4096 grid's half step is 0.211 ct)", worst);
    check(msg, worst < 0.225);
    {
        int32_t u, f;
        song.g[G_TUNE] = 4400; tune_get(&u, &f);
        check("  440.0: exactly 0 and 0 (the sound of TUNE 0)", u == 0 && f == 0);
        song.g[G_TUNE] = 4153; tune_get(&u, &f);
        snprintf(msg, sizeof msg, "  415.3 Hz: %.2f ct (G#4 in tune: -100.0)", u * 6.25 + f / 2.367);
        check(msg, fabs(u * 6.25 + f / 2.367 + 100.0) < 0.25);
        song.g[G_TUNE] = 4662; tune_get(&u, &f);
        snprintf(msg, sizeof msg, "  466.2 Hz: %.2f ct (A#4 in tune: +100.0)", u * 6.25 + f / 2.367);
        check(msg, fabs(u * 6.25 + f / 2.367 - 100.0) < 0.35);
        song.g[G_TUNE] = 3; tune_get(&u, &f);
        check("  a value out of range (3, 9999): 0 and 0", u == 0 && f == 0 && (song.g[G_TUNE] = 9999, tune_get(&u, &f), u == 0 && f == 0));
        song.g[G_TUNE] = 4400;
    }

    /* 3. the pitch of a rendered sine */
    for (k = 0; k < 4u; k++) {
        sine_note(69, T[k]);
        fx = measure();
        want = 1200.0 * log2((double)T[k] / 4400.0);
        snprintf(msg, sizeof msg, "A4 at %.1f Hz: the sine is %+.3f ct from the 440.0 one (exact %+.3f ct; %.3f Hz)", T[k] / 10.0, cents(fx, f0), want, fx);
        check(msg, fabs(cents(fx, f0) - want) < 0.25);
    }
    sine_note(57, 4320);                                  /* the offset is the same at every note (A3) */
    fx = measure();
    sine_note(57, TUNE_DEF);
    f0 = measure();
    snprintf(msg, sizeof msg, "A3 at 432.0 Hz: %+.3f ct (exact %+.3f)", cents(fx, f0), 1200.0 * log2(432.0 / 440.0));
    check(msg, fabs(cents(fx, f0) - 1200.0 * log2(432.0 / 440.0)) < 0.25);

    /* 4. the texts */
    param_format(&GP[G_TUNE], 4400, v, &unit);
    check("param_format: 4400 is '440.0' Hz", str_eq(v, "440.0") && str_eq(unit, "Hz"));
    param_format(&GP[G_TUNE], 4153, v, &unit);
    check("  4153 is '415.3'", str_eq(v, "415.3"));
    param_format(&GP[G_TUNE], 4662, v, &unit);
    check("  4662 is '466.2'", str_eq(v, "466.2"));
    param_format(&GP[G_TUNE], 12, v, &unit);
    check("  a value out of range (an older project's 12 cents) reads '440.0'", str_eq(v, "440.0"));
    check("the descriptor: TUNE, 4153 .. 4662 (510 steps), default 4400", str_eq(GP[G_TUNE].label, "TUNE") && GP[G_TUNE].min == 4153 && GP[G_TUNE].max == 4662 &&
          GP[G_TUNE].def == 4400 && GP[G_TUNE].fmt == F_TUNEHZ);
    printf("tune: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
