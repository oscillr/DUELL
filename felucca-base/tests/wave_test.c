/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL WAVE (firmware/src/eng_wave.c): the wavetable engine.
 *   1  the build: deterministic bytes, every frame's shape (sine phase: f(0) = 0, the guard, odd symmetry), level and peak
 *   2  the band-limiting: one voice per table, POS 0 / 64 / 127, notes C2 .. C8: the energy away from the note's harmonics
 *      (aliasing and the 8-bit tables' own error), dB under the signal; the same with every note on level 0 (no band-limiting)
 *   3  no table yet: silent; a TABLE change fades out and in (no click); POS jumps ramp (no click)
 *   4  robustness: TABLE changed every block (no crash, the last one built right); an engine switch in the middle of a build
 *      (the other engine's memory untouched); two parts building and playing at once
 *   5  MORPH: TABLE never rolls (SMART, WILD), SMART keeps SYNC 0 at 0, POS rolls
 *   6  cost: host instructions per voice and sample (8 voices), a table build (main loop), the interrupts-off copy
 *   7  the tables of 0.2.7 (TABLE 14..25): the list (26 names of at most 5 letters, TABLE's range), the 14 older tables byte for
 *      byte as built before (a stored TABLE keeps its sound), a TABLE change into each new one fades (no click), their aliasing
 *      worst case, every table's build cost (the dearest named)
 *   8  DUELL 0.2.14's user tables (TABLE 26..28 = USR1..USR3, eng_wave.c wv_user): the byte -> level map against the exact 2^(-(255 - b) / 16), a table
 *      built from a slot against a reference computed here (double precision sums, the engine's 10 bit levels, RMS 63.5 / peak 127: within 2 steps of the
 *      8-bit samples, three levels), an EMPTY slot = a sine named EMPTY, the shown names, a slot stored again under a playing part (fade out, built anew,
 *      no click), a slot changed in the middle of its build (never half of each), determinism, the factory tables untouched (t_list's fingerprint)
 *   build/host/wave_test        (DUELL/host_tests.sh wave); WAVE_VERBOSE=1 prints the whole aliasing table */
#define WV_TEST_HOOKS 1
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "wtab_encode.h"
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif

static int bad, verbose;
static void expect(const char *what, int ok)
{
    printf("wave: %-112s %s\n", what, ok ? "ok" : "FAIL");
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
static uint64_t fnv(const void *p, size_t n)
{
    const uint8_t *b = p;
    uint64_t h = 0xcbf29ce484222325ull;
    while (n--)
        h = (h ^ *b++) * 0x100000001B3ull;
    return h;
}
static wv_mem_t *wmem(uint32_t part) { return (wv_mem_t *)ENG_MEM(part); }

/* part 'part' on WAVE with its first host preset, then these values; dry (no sends, no insert), its table built */
static void setup_part(uint32_t part, uint32_t table, int32_t pos, int32_t sub, int32_t dtn)
{
    track_t *t = &trk[part];
    host_preset(t, ENGI_WAVE, 0);
    t->p[P_E0] = (int16_t)table;
    t->p[P_E1] = (int16_t)pos;
    t->p[P_E2] = t->p[P_E3] = 0;
    t->p[P_E4] = (int16_t)sub;
    t->p[P_E5] = (int16_t)dtn;
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_ATK] = 0;
    t->p[P_SUS] = 127;
    t->p[P_REL] = 20;
    t->p[P_VOICE] = V_POLY;
    t->p[P_TRIM] = (int16_t)(getenv("WLVL") ? atoi(getenv("WLVL")) : 104);
    wave_build_now(t);
}
static void fresh(void)                         /* (within one child: the parts, the engine memory and the build job anew) */
{
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    memset(eng_mem_owner, 0, sizeof eng_mem_owner);
    memset(&wvj, 0, sizeof wvj);
}

/* ---------------------------------------------------------------------------------------------- 1 the build --- */
static void t_build(void)
{
    static int8_t a[WV_F * WV_FS], b[WV_F * WV_FS];
    uint32_t tb, f, l, n, okshape = 1, oklvl = 1, okdet = 1;
    int32_t rmin = 1 << 30, rmax = 0;
    for (tb = 0; tb < WV_NT; tb++) {
        for (f = 0; f < WV_F; f++)
            wv_frame(tb, f, a + f * WV_FS);
        fresh();
        setup_part(0, tb, 0, 0, 0);
        memcpy(b, wmem(0)->tab, sizeof b);
        okdet &= !memcmp(a, b, sizeof a) && wmem(0)->h.have == tb + 1u;
        for (f = 0; f < WV_F; f++)
            for (l = 0; l < WV_NL; l++) {
                const int8_t *s = a + f * WV_FS + WV_LO[l];
                uint32_t N = WV_N[l];
                int64_t ss = 0;
                okshape &= s[0] == 0 && s[N] == 0 && s[N / 2u] == 0;
                for (n = 1; n < N; n++)
                    okshape &= s[N - n] == -s[n];
                for (n = 0; n < N; n++)
                    ss += s[n] * s[n];
                if (!l) {
                    int32_t r = (int32_t)(sqrt((double)ss / N) * 100);
                    rmin = r < rmin ? r : rmin;
                    rmax = r > rmax ? r : rmax;
                    oklvl &= r >= 2000 && r <= 6400;
                }
            }
    }
    {
        char m[160];
        snprintf(m, sizeof m, "the build: %u tables x %u frames x %u levels, the main loop's bytes = a direct build of each frame (deterministic)",
                 (unsigned)WV_NT, (unsigned)WV_F, (unsigned)WV_NL);
        expect(m, okdet);
        expect("every frame and level: sine phase (f(0) = f(N/2) = 0, the guard = f(0), f(N - n) = -f(n))", okshape);
        snprintf(m, sizeof m, "every frame's level-0 RMS %.1f .. %.1f (8-bit units; 63.5 the target, within 10 dB: a spiky frame lower, its peak <= 127)",
                 rmin / 100.0, rmax / 100.0);
        expect(m, oklvl);
    }
}

/* ------------------------------------------------------------------------------------ 7 the tables of 0.2.7 --- */
#define WV_NT_OLD 14u                            /* the tables of 0.2.5 / 0.2.6; 14..25 are 0.2.7's */
#define WV_OLD_FP 0x6b07ca785cb43a2full          /* their bytes, built by eng_wave.c of 0.2.6 (c1ac549) */
static void t_list(void)
{
    static int8_t a[WV_F * WV_FS];
    uint64_t h = 0xcbf29ce484222325ull;
    uint32_t tb, f, okn = 1;
    char m[200];
    for (tb = 0; tb < WV_NTF; tb++)
        okn &= strlen(N_WV_TABLE[tb]) >= 3u && strlen(N_WV_TABLE[tb]) <= 5u;
    wt_names();
    for (tb = WV_NTF; tb < WV_NT; tb++)                 /* (0.2.14: the user slots: EMPTY here, 8 characters at most) */
        okn &= !strcmp(N_WV_TABLE[tb], "EMPTY") && N_WV_TABLE[tb] == wv_uname[tb - WV_NTF];
    okn &= ENG_WAVE.edit[0].max == (int)WV_NT - 1 && ENG_WAVE.edit[0].names == N_WV_TABLE && WV_NT == 29u && WV_NTF == 26u && WV_NU == 3u;
    okn &= !strcmp(N_WV_TABLE[13], "ROUGH") && !strcmp(N_WV_TABLE[14], "BAND") && !strcmp(N_WV_TABLE[25], "AIR");
    okn &= wv_table_of(-3) == 0u && wv_table_of(13) == 13u && wv_table_of(25) == 25u && wv_table_of(28) == 28u && wv_table_of(200) == WV_NT - 1u;
    snprintf(m, sizeof m, "the list: %u tables (26 factory: 3..5 letter names; 3 user slots, EMPTY), TABLE 0..%d, a value past the end the last",
             (unsigned)WV_NT, ENG_WAVE.edit[0].max);
    expect(m, okn);
    for (tb = 0; tb < WV_NT_OLD; tb++) {               /* (FNV-1a over every byte of the 14 older tables) */
        for (f = 0; f < WV_F; f++)
            wv_frame(tb, f, a + f * WV_FS);
        for (f = 0; f < sizeof a; f++)
            h = (h ^ (uint8_t)a[f]) * 0x100000001B3ull;
    }
    snprintf(m, sizeof m, "the 14 older tables byte for byte as in 0.2.5 / 0.2.6 (fingerprint %016llx): a stored TABLE 0..13 sounds as it did",
             (unsigned long long)h);
    expect(m, h == WV_OLD_FP);
}

/* --------------------------------------------------------------------------------------- 2 the band-limiting --- */
#define FFT_N 16384u
static void fft(double *re, double *im, uint32_t n)
{
    uint32_t i, j, k, m;
    for (i = 1, j = 0; i < n; i++) {
        uint32_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (m = 2; m <= n; m <<= 1) {
        double a = -2 * M_PI / m;
        for (k = 0; k < n; k += m)
            for (j = 0; j < m / 2u; j++) {
                double wr = cos(a * j), wi = sin(a * j);
                double xr = re[k + j + m / 2u] * wr - im[k + j + m / 2u] * wi, xi = re[k + j + m / 2u] * wi + im[k + j + m / 2u] * wr;
                re[k + j + m / 2u] = re[k + j] - xr;
                im[k + j + m / 2u] = im[k + j] - xi;
                re[k + j] += xr;
                im[k + j] += xi;
            }
    }
}
/* one held note on part 1 (dry): dB of the energy away from its harmonics (+-6 bins of k f0, Blackman-Harris) under the total */
static double alias_db_1(uint32_t table, int32_t pos, uint32_t note, int32_t e2, int32_t e3, double *f0o);
static double alias_db(uint32_t table, int32_t pos, uint32_t note, int32_t e2, int32_t e3, double *f0o)
{
    double r[2] = {0, 0};
    int fd[2];
    pid_t pid;
    if (pipe(fd))
        return 0;
    fflush(stdout);
    pid = fork();
    if (!pid) {
        close(fd[0]);
        r[0] = alias_db_1(table, pos, note, e2, e3, &r[1]);
        if (write(fd[1], r, sizeof r) != sizeof r)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], r, sizeof r) != sizeof r)
        r[0] = 99;
    close(fd[0]);
    waitpid(pid, 0, 0);
    *f0o = r[1];
    return r[0];
}
static double alias_db_1(uint32_t table, int32_t pos, uint32_t note, int32_t e2, int32_t e3, double *f0o)
{
    static double re[FFT_N], im[FFT_N];
    int32_t o[2 * CTL];
    uint32_t b, i, k, nb = (FFT_N + 22050u) / CTL;
    double f0, harm = 0, tot = 0;
    fresh();
    setup_part(0, table, pos, 0, 0);
    trk[0].p[P_E2] = (int16_t)e2;
    trk[0].p[P_E3] = (int16_t)e3;
    trk_note_on(&trk[0], note, 100);
    for (b = 0, i = 0; b < nb; b++) {
        mix_block(o, CTL);
        if (b * CTL >= 22050u)
            for (k = 0; k < CTL && i < FFT_N; k++, i++)
                re[i] = o[2u * k], im[i] = 0;
    }
    for (i = 0; i < FFT_N; i++) {
        double x = 2 * M_PI * i / FFT_N;
        re[i] *= 0.35875 - 0.48829 * cos(x) + 0.14128 * cos(2 * x) - 0.01168 * cos(3 * x);
    }
    fft(re, im, FFT_N);
    f0 = (double)pitch_inc(note * 16u) * FS / 4294967296.0;
    *f0o = f0;
    for (i = 8; i < FFT_N / 2u; i++) {                       /* (above ~20 Hz: the master's DC blocker) */
        double e = re[i] * re[i] + im[i] * im[i], fq = (double)i * FS / FFT_N, h = fq / f0;
        double kk = floor(h + 0.5);
        if (kk >= 1 && fabs(fq - kk * f0) <= 6.0 * FS / FFT_N)
            harm += e;
        else if (fq < 16000.0)                               /* (what folds back above 16 kHz is not counted: WV_FOLD's design) */
            tot += e;
    }
    return harm > 0 ? 10 * log10((tot > 1e-9 ? tot : 1e-9) / harm) : 0;
}
static int dcmp(const void *x, const void *y) { double a = *(const double *)x, b = *(const double *)y; return a < b ? -1 : a > b; }
static void t_alias(void)
{
    static const uint8_t NOTES[] = {36, 48, 60, 67, 72, 79, 84, 91, 96, 103, 108};
    static const uint8_t POS[] = {0, 64, 127};
    static double all[WV_NT * 3u * 11u];
    uint32_t tb, pi, ni, na = 0;
    double worst = -999, worst0 = -999, hi_bl = -999, hi_l0 = -999, f0, wnew = -999;
    char wn[64] = "", m[220], wnn[64] = "";
    for (tb = 0; tb < WV_NT; tb++) {
        double tw = -999, tw0 = -999;
        char twn[32] = "";
        for (pi = 0; pi < NELEM(POS); pi++)
            for (ni = 0; ni < NELEM(NOTES); ni++) {
                double a, a0;
                wv_test_l0 = 0;
                a = alias_db(tb, POS[pi], NOTES[ni], 0, 0, &f0);
                wv_test_l0 = 1;
                a0 = alias_db(tb, POS[pi], NOTES[ni], 0, 0, &f0);
                wv_test_l0 = 0;
                all[na++] = a;
                if (verbose)
                    printf("wave:   %-6s POS %3u note %3u (%6.0f Hz): off the harmonics %6.1f dB, level 0 only %6.1f dB\n", N_WV_TABLE[tb],
                           POS[pi], NOTES[ni], f0, a, a0);
                if (a > tw) {
                    tw = a;
                    snprintf(twn, sizeof twn, "POS %u note %u", POS[pi], NOTES[ni]);
                }
                tw0 = a0 > tw0 ? a0 : tw0;
                if (a > worst) {
                    worst = a;
                    snprintf(wn, sizeof wn, "%s POS %u note %u", N_WV_TABLE[tb], POS[pi], NOTES[ni]);
                }
                if (tb >= WV_NT_OLD && a > wnew) {
                    wnew = a;
                    snprintf(wnn, sizeof wnn, "%s POS %u note %u", N_WV_TABLE[tb], POS[pi], NOTES[ni]);
                }
                worst0 = a0 > worst0 ? a0 : worst0;
                if (NOTES[ni] >= 84) {
                    hi_bl = a > hi_bl ? a : hi_bl;
                    hi_l0 = a0 > hi_l0 ? a0 : hi_l0;
                }
            }
        printf("wave:   %-6s worst %6.1f dB (%s); level 0 only: worst %6.1f dB\n", N_WV_TABLE[tb], tw, twn, tw0);
    }
    qsort(all, na, sizeof all[0], dcmp);
    snprintf(m, sizeof m, "aliasing (energy off the harmonics under 16 kHz, C2..C8, POS 0/64/127): median %.1f dB, worst %.1f dB (%s); level 0 only %.1f",
             all[na / 2u], worst, wn, worst0);
    expect(m, worst <= -30.0 && all[na / 2u] <= -42.0);
    snprintf(m, sizeof m, "notes C6 .. C8: band-limited %.1f dB, level 0 only %.1f dB: the levels take away at least 20 dB", hi_bl, hi_l0);
    expect(m, hi_bl <= hi_l0 - 20.0);
    snprintf(m, sizeof m, "the tables of 0.2.7 (BAND .. AIR), C2..C8, POS 0/64/127: aliasing worst %.1f dB (%s), within the older tables' -30 dB",
             wnew, wnn);
    expect(m, wnew <= -30.0);
}

/* SYNC and BEND: the aliasing with and without the reset correction (SAWSQ, POS 0, a sweep of SYNC / BEND, C3 .. C6), and DC */
static void t_syncbend(void)
{
    static const uint8_t NOTES[] = {48, 60, 72, 84};
    static const uint8_t SY[] = {20, 50, 80, 110, 127};
    static const uint8_t BD[] = {30, 70, 110, 127};
    uint32_t i, k;
    double ws = -999, ws0 = -999, wb = -999, f0, ms = 0, ms0 = 0;
    char m[200];
    for (i = 0; i < NELEM(SY); i++)
        for (k = 0; k < NELEM(NOTES); k++) {
            double a, a0;
            wv_test_noblep = 0;
            a = alias_db(0, 0, NOTES[k], 0, SY[i], &f0);
            wv_test_noblep = 1;
            a0 = alias_db(0, 0, NOTES[k], 0, SY[i], &f0);
            wv_test_noblep = 0;
            ms += a;
            ms0 += a0;
            if (verbose)
                printf("wave:   SYNC %3u note %3u: %6.1f dB, without the correction %6.1f dB\n", SY[i], NOTES[k], a, a0);
            ws = a > ws ? a : ws;
            ws0 = a0 > ws0 ? a0 : ws0;
        }
    for (i = 0; i < NELEM(BD); i++)
        for (k = 0; k < NELEM(NOTES); k++) {
            double a = alias_db(0, 64, NOTES[k], BD[i], 0, &f0);
            if (verbose)
                printf("wave:   BEND %3u note %3u: %6.1f dB\n", BD[i], NOTES[k], a);
            wb = a > wb ? a : wb;
        }
    ms /= NELEM(SY) * NELEM(NOTES);
    ms0 /= NELEM(SY) * NELEM(NOTES);
    snprintf(m, sizeof m, "SYNC (20..127, C3..C6): aliasing mean %.1f dB, worst %.1f dB; without the reset correction mean %.1f, worst %.1f", ms, ws,
             ms0, ws0);
    expect(m, ms <= ms0 - 3.0 && ws <= -20.0);
    snprintf(m, sizeof m, "BEND (30..127, C3..C6, the level chosen for the steepest part): aliasing worst %.1f dB", wb);
    expect(m, wb <= -30.0);
    {   /* DC: a held note with BEND and SYNC far up: its mean against its level */
        int32_t o[2 * CTL];
        int64_t sum = 0, ss = 0;
        uint32_t b, j;
        fresh();
        setup_part(0, 1, 100, 0, 0);
        trk[0].p[P_E2] = 127;
        trk[0].p[P_E3] = 90;
        trk_note_on(&trk[0], 48, 100);
        for (b = 0; b < 400u; b++) {
            mix_block(o, CTL);
            if (b >= 200u)
                for (j = 0; j < CTL; j++) {
                    sum += o[2u * j];
                    ss += (int64_t)o[2u * j] * o[2u * j];
                }
        }
        snprintf(m, sizeof m, "DC with BEND 127, SYNC 90 (PWM, a narrow pulse): mean %.1f, rms %.0f (the voice's own DC tracker; the regress limit is 400)",
                 (double)sum / (200.0 * CTL), sqrt((double)ss / (200.0 * CTL)));
        expect(m, fabs((double)sum / (200.0 * CTL)) < 100.0);
    }
}

/* ------------------------------------------------------------------------------- 3 silence, fades, no clicks --- */
static int32_t max_step(const int32_t *x, uint32_t n)
{
    int32_t m = 0;
    uint32_t i;
    for (i = 1; i < n; i++) {
        int32_t d = x[i] - x[i - 1];
        d = d < 0 ? -d : d;
        m = d > m ? d : m;
    }
    return m;
}
#define RB 400u
static int32_t rec[RB * CTL];
static void render(uint32_t nb, int service)    /* nb blocks of the left channel into rec */
{
    int32_t o[2 * CTL];
    uint32_t b, k;
    for (b = 0; b < nb && b < RB; b++) {
        mix_block(o, CTL);
        for (k = 0; k < CTL; k++)
            rec[b * CTL + k] = o[2u * k];
        if (service)
            wave_service();
    }
}
static void t_fades(void)
{
    static const uint8_t CH[3] = {48, 52, 55};
    int32_t steady, s_out, s_in, m;
    uint32_t i, sil = 1, back;
    char msg[200];
    fresh();
    setup_part(0, 0, 30, 0, 0);
    for (i = 0; i < 3u; i++)
        trk_note_on(&trk[0], CH[i], 100);
    render(40, 0);
    steady = max_step(rec + 20 * CTL, 20 * CTL);
    trk[0].p[P_E0] = 2;                                /* TABLE SAWSQ -> HARM, nobody builds yet */
    render(60, 0);
    s_out = max_step(rec, 8 * CTL);
    {                                                   /* (silent: no AC; the master's DC blocker settles slowly, as after any note) */
        double d2 = 0;
        for (i = 8 * CTL + 1u; i < 60u * CTL; i++)
            d2 += (double)(rec[i] - rec[i - 1u]) * (rec[i] - rec[i - 1u]);
        m = (int32_t)(sqrt(d2 / (52.0 * CTL)) * 100);
    }
    sil = m <= 100;
    snprintf(msg, sizeof msg, "a TABLE change: the old table fades out (largest step %d vs %d held), then silence while the new one is built (rms step %d / 100)",
             s_out, steady, m);
    expect(msg, s_out <= steady * 3 / 2 && sil && !wmem(0)->h.have);
    for (back = 0; back < 400u && !(wmem(0)->h.have && wmem(0)->h.have == wmem(0)->h.want); back++)
        render(1, 1);
    render(30, 0);
    s_in = max_step(rec, 10 * CTL);
    for (i = 20 * CTL, m = 0; i < 30u * CTL; i++)
        m = abs(rec[i]) > m ? abs(rec[i]) : m;
    snprintf(msg, sizeof msg, "the main loop builds it in %u blocks of audio (one frame a block here); the voices fade in (largest step %d), sound (peak %d)",
             back, s_in, m);
    expect(msg, back <= WV_F + 6u && s_in <= steady * 3 / 2 && m > 1000);
    /* POS jumps 0 -> 127 -> 0 on a held chord: ramped over a block */
    fresh();
    setup_part(0, 0, 0, 0, 0);
    for (i = 0; i < 3u; i++)
        trk_note_on(&trk[0], CH[i], 100);
    render(40, 0);
    steady = max_step(rec + 10 * CTL, 30 * CTL);
    trk[0].p[P_E1] = 127;
    render(40, 0);
    steady = max_step(rec + 10 * CTL, 30 * CTL) > steady ? max_step(rec + 10 * CTL, 30 * CTL) : steady;
    trk[0].p[P_E1] = 0;
    render(3, 0);
    s_out = max_step(rec, 3 * CTL);
    snprintf(msg, sizeof msg, "POS 127 -> 0 at once on a held chord: ramped over a block, largest step %d (held at either end: %d)", s_out, steady);
    expect(msg, s_out <= steady * 3 / 2);
}

/* a TABLE change into each table of 0.2.7 on a held chord (from SAWSQ, the main loop building one frame a block): the fade out
 * no larger a step than the chord's own on SAWSQ, the fade in than its own on the new table (x 1.5), then the new table sounds */
static void t_newfades(void)
{
    static const uint8_t CH[3] = {48, 52, 55};
    uint32_t tb, i, b, ok = 1;
    int32_t worst = 0, wsteady = 0;
    char m[200], bad_n[16] = "";
    for (tb = WV_NT_OLD; tb < WV_NT; tb++) {
        int32_t st0, st1, s_out, s_in, pk = 0;
        fresh();
        setup_part(0, 0, 64, 0, 0);
        for (i = 0; i < 3u; i++)
            trk_note_on(&trk[0], CH[i], 100);
        render(40, 0);
        st0 = max_step(rec + 20 * CTL, 20 * CTL);
        trk[0].p[P_E0] = (int16_t)tb;
        render(8, 0);
        s_out = max_step(rec, 8 * CTL);
        for (b = 0; b < 400u && !(wmem(0)->h.have && wmem(0)->h.have == wmem(0)->h.want); b++)
            render(1, 1);
        render(40, 0);
        s_in = max_step(rec, 10 * CTL);
        st1 = max_step(rec + 20 * CTL, 20 * CTL);
        for (i = 20 * CTL; i < 40u * CTL; i++)
            pk = abs(rec[i]) > pk ? abs(rec[i]) : pk;
        if (!(s_out <= st0 * 3 / 2 && s_in <= st1 * 3 / 2 && pk > 1000 && wmem(0)->h.have == tb + 1u)) {
            ok = 0;
            snprintf(bad_n, sizeof bad_n, "%s", N_WV_TABLE[tb]);
        }
        if (verbose)
            printf("wave:   SAWSQ -> %-6s fade out step %5d, in %5d (held %5d / %5d), peak %5d, built in %u blocks\n", N_WV_TABLE[tb],
                   s_out, s_in, st0, st1, pk, b);
        if (s_in > worst) {
            worst = s_in;
            wsteady = st1;
        }
    }
    snprintf(m, sizeof m, "a TABLE change into each of the 12 new tables on a held chord: fade out, silence, fade in, no click (largest fade-in step %d, held %d)%s%s",
             worst, wsteady, *bad_n ? ": FAILS on " : "", bad_n);
    expect(m, ok);
}

/* ------------------------------------------------------------------------------------------- 4 robustness --- */
static void t_robust(void)
{
    static int8_t ref[WV_F * WV_FS];
    static uint8_t keep[ENG_MEM_PART];
    uint32_t b, f, ok = 1, seed = 12345, rec_ok = 1;
    int32_t o[2 * CTL];
    char msg[160];
    fresh();
    setup_part(0, 0, 50, 40, 9);
    trk_note_on(&trk[0], 48, 100);
    trk_note_on(&trk[0], 55, 100);
    for (b = 0; b < 300u; b++) {                       /* TABLE every block, the main loop now and then */
        uint32_t k;
        seed = seed * 1664525u + 1013904223u;
        trk[0].p[P_E0] = (int16_t)((seed >> 16) % WV_NT);
        mix_block(o, CTL);
        for (k = 0; k < 2u * CTL; k++)
            ok &= o[k] > -40000 && o[k] < 40000;
        if (seed & 0x100u)
            wave_service();
    }
    trk[0].p[P_E0] = 3;
    for (b = 0; b < 200u && !(wmem(0)->h.have == 4u && wmem(0)->h.want == 4u); b++) {
        mix_block(o, CTL);
        wave_service();
    }
    for (f = 0; f < WV_F; f++)
        wv_frame(3, f, ref + f * WV_FS);
    expect("TABLE changed every block for 300 blocks (the main loop between): no crash, the output bounded; then ODD is built byte for byte",
           ok && wmem(0)->h.have == 4u && !memcmp(ref, wmem(0)->tab, sizeof ref));
    /* an engine switch while a table is being built: the job stops, the new engine's memory is never written */
    fresh();
    setup_part(0, 0, 50, 0, 0);
    trk[0].p[P_E0] = 1;
    for (b = 0; b < 6u; b++)                           /* the fade out, then a few frames of PWM */
        mix_block(o, CTL);
    for (b = 0; b < 5u; b++)
        wave_service();
    host_preset_req(&trk[0], ENGI_PHYS, 0);            /* to PHYS, as the UI asks (the audio side switches) */
    for (b = 0; b < 8u; b++)
        mix_block(o, CTL);
    ok = trk[0].engine == ENGI_PHYS && eng_mem_owner[0] == ENGI_PHYS + 1u;
    memcpy(keep, ENG_MEM(0), ENG_MEM_PART);
    for (b = 0; b < 40u; b++)
        rec_ok &= !wave_service() || 1;
    expect("an engine switch (WAVE -> PHYS) during a build: the build stops, PHYS's memory is not written by it afterwards",
           ok && !memcmp(keep, ENG_MEM(0), ENG_MEM_PART) && !wvj.on);
    /* two parts at once: each builds and plays its own table */
    fresh();
    setup_part(0, 1, 40, 0, 0);
    setup_part(1, 2, 90, 0, 0);
    trk[0].p[P_E0] = 3;
    trk[1].p[P_E0] = 0;
    for (b = 0; b < 300u && !(wmem(0)->h.have == 4u && wmem(1)->h.have == 1u); b++) {
        mix_block(o, CTL);
        wave_service();
    }
    ok = wmem(0)->h.have == 4u && wmem(1)->h.have == 1u;
    for (f = 0; f < WV_F; f++)
        wv_frame(3, f, ref + f * WV_FS);
    ok &= !memcmp(ref, wmem(0)->tab, sizeof ref);
    for (f = 0; f < WV_F; f++)
        wv_frame(0, f, ref + f * WV_FS);
    ok &= !memcmp(ref, wmem(1)->tab, sizeof ref);
    {
        int64_t e0 = 0, e1 = 0;
        uint32_t k;
        trk_note_on(&trk[0], 60, 100);
        trk_note_on(&trk[1], 64, 100);
        duell_mode_force(DM_SPLIT);
        for (b = 0; b < 200u; b++) {
            mix_block(o, CTL);
            for (k = 0; k < CTL; k++) {
                e0 += (int64_t)o[2u * k] * o[2u * k];
                e1 += (int64_t)o[2u * k + 1u] * o[2u * k + 1u];
            }
        }
        duell_mode_force(DM_STEREO);
        snprintf(msg, sizeof msg, "two parts on WAVE at once (ODD, SAWSQ; %u blocks of building): each table right, both sound (SPLIT: L %.0f, R %.0f rms)",
                 b, sqrt((double)e0 / (200.0 * CTL)), sqrt((double)e1 / (200.0 * CTL)));
        expect(msg, ok && e0 > 200ll * CTL * 1000000ll && e1 > 200ll * CTL * 1000000ll);
    }
}

/* ------------------------------------------------------------------------------------------------ 5 morph --- */
static void t_morph(void)
{
    int8_t o[P_COUNT], end[P_COUNT];
    uint32_t s, i, tab_ok = 1, sync_ok = 1, pos_moved = 0, sync_moved = 0;
    fresh();
    setup_part(0, 2, 60, 0, 0);
    for (i = 0; i < P_COUNT; i++)
        o[i] = (int8_t)trk[0].p[i];
    for (s = 1; s <= 200u; s++) {
        mrc = 0;
        morph_roll_pure(o, end, &ENG_WAVE, 60, s, 0, 1);
        tab_ok &= end[P_E0] == o[P_E0];
        sync_ok &= end[P_E3] == 0;
        pos_moved += end[P_E1] != o[P_E1];
        mrc = MC_WILD;
        morph_roll_pure(o, end, &ENG_WAVE, 60, s, 0, 1);
        tab_ok &= end[P_E0] == o[P_E0];
        sync_moved += end[P_E3] != 0;
        mrc = 0;
    }
    expect("MORPH: TABLE never rolls (200 SMART + 200 WILD rolls at AMOUNT 60); SMART keeps SYNC 0 at 0, WILD may move it; POS rolls",
           tab_ok && sync_ok && pos_moved > 100u && sync_moved > 0u);
    for (tab_ok = 1, i = WV_NT_OLD; i < WV_NT; i++) {   /* the new tables: a TABLE value 14..25 stays too (SMART and WILD, AMOUNT 100) */
        uint32_t k;
        fresh();
        setup_part(0, i, 60, 0, 0);
        for (k = 0; k < P_COUNT; k++)
            o[k] = (int8_t)trk[0].p[k];
        for (s = 1; s <= 50u; s++) {
            mrc = 0;
            morph_roll_pure(o, end, &ENG_WAVE, 100, s, 0, 1);
            tab_ok &= end[P_E0] == (int8_t)i;
            mrc = MC_WILD;
            morph_roll_pure(o, end, &ENG_WAVE, 100, s, 0, 1);
            tab_ok &= end[P_E0] == (int8_t)i;
            mrc = 0;
        }
    }
    expect("MORPH: TABLE 14..25 never rolls either (50 SMART + 50 WILD rolls each at AMOUNT 100)", tab_ok);
}

/* ------------------------------------------------------------------------------------------------- 6 cost --- */
static double cost_of_1(uint32_t table, int32_t sub, int32_t dtn, int32_t e2, int32_t e3, uint32_t notes);
static double cost_of(uint32_t table, int32_t sub, int32_t dtn, int32_t e2, int32_t e3, uint32_t notes)
{
    double r = 0;
    int fd[2];
    pid_t pid;
    if (pipe(fd))
        return 0;
    fflush(stdout);
    pid = fork();
    if (!pid) {
        close(fd[0]);
        r = cost_of_1(table, sub, dtn, e2, e3, notes);
        if (write(fd[1], &r, sizeof r) != sizeof r)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], &r, sizeof r) != sizeof r)
        r = 0;
    close(fd[0]);
    waitpid(pid, 0, 0);
    return r;
}
static double cost_of_1(uint32_t table, int32_t sub, int32_t dtn, int32_t e2, int32_t e3, uint32_t notes)
{
    static const uint8_t N[8] = {36, 43, 48, 55, 60, 64, 67, 72};
    int32_t o[2 * CTL];
    uint32_t b, i, nb = FS / CTL;
    uint64_t i0;
    fresh();
    setup_part(0, table, 64, sub, dtn);
    trk[0].p[P_E2] = (int16_t)e2;
    trk[0].p[P_E3] = (int16_t)e3;
    for (i = 0; i < notes; i++)
        trk_note_on(&trk[0], N[i], 100);
    for (b = 0; b < 100u; b++)
        mix_block(o, CTL);
    i0 = instr_now();
    for (b = 0; b < nb; b++)
        mix_block(o, CTL);
    return i0 ? (double)(instr_now() - i0) / (nb * CTL) : 0;
}
static void t_cost(void)
{
    double idle = cost_of(0, 0, 0, 0, 0, 0), plain = cost_of(0, 0, 0, 0, 0, 8), sub = cost_of(0, 80, 0, 0, 0, 8);
    double dtn = cost_of(0, 0, 9, 0, 0, 8), all = cost_of(0, 80, 9, 0, 0, 8), sy = cost_of(0, 0, 0, 0, 70, 8), bd = cost_of(0, 0, 0, 60, 0, 8);
    double heavy = cost_of(0, 80, 9, 60, 70, 8);
    uint64_t i0, i1;
    uint32_t f;
    char m[220];
    if (!instr_now()) {
        printf("wave: cost: no instruction counter on this host (proc_pid_rusage): not measured\n");
        return;
    }
    printf("wave:   per voice and sample (8 voices, over the idle mix %.0f): plain %.1f, SUB %.1f, DTN %.1f, SUB + DTN %.1f, SYNC %.1f, BEND %.1f,"
           " all four %.1f\n", idle, (plain - idle) / 8, (sub - idle) / 8, (dtn - idle) / 8, (all - idle) / 8, (sy - idle) / 8, (bd - idle) / 8,
           (heavy - idle) / 8);
    i0 = instr_now();
    for (f = 0; f < WV_F; f++)
        wv_frame(0, f, wvj.out);
    i1 = instr_now();
    printf("wave:   a table build (main loop, the host's instructions): %llu a table, %llu a frame (SAWSQ, 64 harmonics)\n",
           (unsigned long long)(i1 - i0), (unsigned long long)((i1 - i0) / WV_F));
    {   /* every table's build, the dearest named (420 k host instructions ~ one 2.9 ms half buffer of the device's CPU) */
        uint64_t most = 0, mostn = 0, least = ~0ull;
        uint32_t tb, wt = 0, wn_ = 0;
        for (tb = 0; tb < WV_NT; tb++) {
            uint64_t c;
            i0 = instr_now();
            for (f = 0; f < WV_F; f++)
                wv_frame(tb, f, wvj.out);
            c = instr_now() - i0;
            if (verbose)
                printf("wave:   build %-6s %5.2f M host instructions\n", N_WV_TABLE[tb], c / 1e6);
            if (c > most)
                most = c, wt = tb;
            if (tb >= WV_NT_OLD && c > mostn)
                mostn = c, wn_ = tb;
            least = c < least ? c : least;
        }
        snprintf(m, sizeof m, "every table's build %.2f .. %.2f M host instructions (the dearest %s; of 0.2.7's %s %.2f M, ~%.0f ms of the device): each under 2.5 M",
                 least / 1e6, most / 1e6, N_WV_TABLE[wt], N_WV_TABLE[wn_], mostn / 1e6, mostn / 1e6 / 0.42 * 2.9);
        expect(m, most < 2500000u);
    }
    snprintf(m, sizeof m, "cost: a plain read under 80 host instructions per voice and sample, every feature at once under 200 (PHASE: ~57 .. 120)");
    expect(m, (plain - idle) / 8 < 80 && (heavy - idle) / 8 < 200);
}


/* ------------------------------------------------------------------------------------------ 8 the user tables --- */
static void user_set(uint32_t u, const uint8_t *amp, const char *name)   /* a slot as wt_store leaves it (RAM) */
{
    memset(&wt_ram[u], 0, sizeof wt_ram[u]);
    wt_ram[u].ver = WT_VER;
    wt_ram[u].nf = WV_F;
    wt_ram[u].nh = WV_H;
    memcpy(wt_ram[u].name, name, strlen(name) < 8u ? strlen(name) : 8u);
    memcpy(wt_ram[u].amp, amp, WV_F * WV_H);
    wt_used |= (uint8_t)(1u << u);
    wt_changed(u);
}
static void user_pattern(uint8_t *amp, uint32_t seed)   /* 19 frames x 64 harmonics: a saw's 1/k slope, frame by frame a different tilt, cutoff and holes */
{
    uint32_t f, k;
    for (f = 0; f < WV_F; f++)
        for (k = 1; k <= WV_H; k++) {
            double v = 255.0 - 16.0 * log2((double)k) * (1.0 + (double)((f + seed) % 3u) * 0.5) - (double)((k * 7u + f * 13u + seed) % 20u);
            amp[f * WV_H + k - 1u] = k > 4u + 3u * f || v < 1.0 || (k * 5u + f + seed) % 11u == 0u ? 0 : (uint8_t)v;
        }
}
/* the reference: level l of frame f from the bytes, double precision sums; the levels as the engine quantises them (10 bit, the largest 1023),
 * the gain RMS 63.5 (the level-0 sum of squares) unless the peak of any level passes 127. Fills out[N] */
static void user_ref(const uint8_t *amp, uint32_t f, uint32_t l, double *out)
{
    double a[WV_H + 1u], big = 0, ss = 0, pk = 0, g;
    uint32_t k, n, ll;
    for (k = 1; k <= WV_H; k++) {
        uint8_t b = amp[f * WV_H + k - 1u];
        a[k] = b ? pow(2.0, -(255.0 - b) / 16.0) : 0;
        big = a[k] > big ? a[k] : big;
    }
    for (k = 1; k <= WV_H; k++) {
        a[k] = big > 0 ? floor(a[k] * 1023.0 / big + 0.5) : 0;
        ss += a[k] * a[k];
    }
    for (ll = 0; ll < WV_NL; ll++)
        for (n = 0; n < WV_N[ll]; n++) {
            double v = 0;
            for (k = 1; k <= (WV_H >> ll); k++)
                v += a[k] * sin(2 * M_PI * (double)k * (double)n / WV_N[ll]);
            pk = fabs(v) > pk ? fabs(v) : pk;
        }
    g = ss > 0 ? 63.5 / sqrt(ss / 2) : 0;
    if (pk * g > 127)
        g = 127 / pk;
    for (n = 0; n < WV_N[l]; n++) {
        double v = 0;
        for (k = 1; k <= (WV_H >> l); k++)
            v += a[k] * sin(2 * M_PI * (double)k * (double)n / WV_N[l]);
        out[n] = v * g;
    }
}
static uint32_t user_cmp(const int8_t *tab, const uint8_t *amp, uint32_t lv, int32_t *worst)   /* frames x levels lv (bit mask) against the reference */
{
    uint32_t f, l, n, bad = 0;
    double ref[256];
    *worst = 0;
    for (f = 0; f < WV_F; f++)
        for (l = 0; l < WV_NL; l++) {
            if (!(lv >> l & 1u))
                continue;
            user_ref(amp, f, l, ref);
            for (n = 0; n < WV_N[l]; n++) {
                int32_t d = abs(tab[f * WV_FS + WV_LO[l] + n] - (int32_t)floor(ref[n] + 0.5));
                *worst = d > *worst ? d : *worst;
                bad += d > 2;
            }
        }
    return bad;
}
static void t_user(void)
{
    static uint8_t A[WV_F * WV_H], B[WV_F * WV_H];
    static int8_t ta[WV_F * WV_FS], tb_[WV_F * WV_FS];
    uint32_t b, f, k, n, ok, same;
    int32_t worst;
    double ref[256], dq = 0;
    char m[220];
    /* the byte -> level map: 2^(-(255 - b) / 16) of 65536 within 1 (the shift truncates), 0 off, 255 = 65536, steps 0.376 dB, non-decreasing */
    for (ok = 1, b = 1; b < 256u; b++) {
        double ex = 65536.0 * pow(2.0, -(255.0 - b) / 16.0);
        ok &= fabs(wt_q16(b) - ex) <= 1.0 && wt_q16(b) >= wt_q16(b - 1u);
        if (b > 100u)
            dq = fmax(dq, fabs(20 * log10(wt_q16(b) / ex)));
    }
    expect("user bytes -> levels: 0 off, 255 = 1.0 (65536), 2^(-(255 - b) / 16) within 1 Q16 for every byte, monotonic", ok && !wt_q16(0) && wt_q16(255) == 65536);
    expect("user bytes: 16 steps an octave: 239 = -6.02 dB, 223 = -12.04 dB (exact shifts of the 16 entry table)", wt_q16(239) == 32768 && wt_q16(223) == 16384);
    user_pattern(A, 0);
    user_pattern(B, 1);
    /* the build against the reference: levels 0 (256 samples, harmonics 1..64), 2 (128, 1..16) and 4 (64, 1..4) */
    fresh();
    user_set(0, A, "SAWTILT");
    for (f = 0; f < WV_F; f++)
        wv_frame(WV_NTF, f, ta + f * WV_FS);
    k = user_cmp(ta, A, 1u | 4u | 16u, &worst);
    snprintf(m, sizeof m, "a user table built from a slot = the reference (double sums, 10 bit levels, RMS 63.5 / peak 127): %u of 19 x 448 samples off by more than 2, worst %d",
             (unsigned)k, (int)worst);
    expect(m, k == 0u);
    user_set(2, B, "TILT 2");
    for (f = 0; f < WV_F; f++)
        wv_frame(WV_NTF + 2u, f, tb_ + f * WV_FS);
    k = user_cmp(tb_, B, 1u | 2u | 8u, &worst);
    snprintf(m, sizeof m, "the same for USR3 and levels 1 3 (128, 64 samples): %u off by more than 2, worst %d; the slots do not leak into each other", (unsigned)k, (int)worst);
    expect(m, k == 0u && memcmp(ta, tb_, sizeof ta));
    for (same = 1, f = 0; f < WV_F; f++) {                          /* all frames sine phase (the factory build's own check), the guard */
        for (b = 0; b < WV_NL; b++) {
            const int8_t *s = tb_ + f * WV_FS + WV_LO[b];
            same &= s[0] == 0 && s[WV_N[b]] == 0 && s[WV_N[b] / 2u] == 0;
            for (n = 1; n < WV_N[b]; n++)
                same &= s[WV_N[b] - n] == -s[n];
        }
    }
    expect("a user table's frames: sine phase (f(0) = f(N/2) = 0, the guard = f(0), f(N - n) = -f(n)), as a factory table's", same);
    /* through the main loop: built at once for a part, deterministic, the same bytes as the direct build */
    fresh();
    user_set(0, A, "SAWTILT");
    setup_part(0, WV_NTF, 40, 0, 0);
    expect("TABLE 26 (USR1) built by the main loop = the direct build, have = want", wmem(0)->h.have == WV_NTF + 1u && !memcmp(wmem(0)->tab, ta, sizeof ta));
    /* a silent frame stays silent (all bytes 0 in a frame), a slot of one harmonic: a sine */
    {
        static uint8_t Z[WV_F * WV_H];
        memcpy(Z, A, sizeof Z);
        memset(Z + 3u * WV_H, 0, WV_H);
        user_set(1, Z, "HOLE");
        wv_frame(WV_NTF + 1u, 3, wvj.out);
        for (n = 0, ok = 1; n < WV_FS; n++)
            ok &= wvj.out[n] == 0;
        wv_frame(WV_NTF + 1u, 4, wvj.out);
        for (n = 0, same = 0; n < WV_FS; n++)
            same |= wvj.out[n] != 0;
        expect("a frame of all-zero bytes is silent (every byte 0), its neighbours are not", ok && same);
    }
    /* EMPTY: a sine in every frame, named EMPTY; the shown names */
    fresh();
    wt_used = 0;
    memset(wt_ram, 0, sizeof wt_ram);
    wt_names();
    setup_part(0, WV_NTF + 1u, 0, 0, 0);
    {
        double e = 0;
        for (f = 0, ok = 1; f < WV_F; f++) {
            const int8_t *s = wmem(0)->tab + f * WV_FS;
            ok &= !memcmp(s, wmem(0)->tab, 257u);
            for (n = 0; n < 256u; n++) {
                double r = 89.8 * sin(2 * M_PI * n / 256.0);
                e = fmax(e, fabs(s[n] - r));
            }
        }
        snprintf(m, sizeof m, "an EMPTY slot plays a sine in every frame (level 0 within %.1f steps of 63.5 RMS), the table is built, its name is EMPTY", e);
        expect(m, ok && e <= 1.5 && wmem(0)->h.have == WV_NTF + 2u && !strcmp(N_WV_TABLE[WV_NTF + 1u], "EMPTY"));
    }
    user_set(1, A, "MY SAW");
    user_set(0, A, "");
    user_set(2, A, "EIGHTCHR");
    expect("shown names: the user's (8 characters whole), USRn when the name is empty, EMPTY after an erase",
           !strcmp(N_WV_TABLE[WV_NTF + 1u], "MY SAW") && !strcmp(N_WV_TABLE[WV_NTF], "USR1") && !strcmp(N_WV_TABLE[WV_NTF + 2u], "EIGHTCHR"));
    wt_used &= (uint8_t)~2u;
    wt_names();
    expect("shown names: an emptied slot is EMPTY again; the others keep theirs", !strcmp(N_WV_TABLE[WV_NTF + 1u], "EMPTY") && !strcmp(N_WV_TABLE[WV_NTF + 2u], "EIGHTCHR"));
    /* a slot stored again under a playing part: out, built anew, in, with no click (largest step vs the held chord's) */
    {
        static const uint8_t CH[3] = {48, 52, 55};
        int32_t steady, s_out = 0, s_in, i, sawfade = 0;
        uint32_t back;
        fresh();
        user_set(0, A, "SAWTILT");
        setup_part(0, WV_NTF, 40, 0, 0);
        for (b = 0; b < 3u; b++)
            trk_note_on(&trk[0], CH[b], 100);
        render(40, 1);
        steady = max_step(rec + 20 * CTL, 20 * CTL);
        user_set(0, B, "NEWER");
        render(8, 0);                                               /* the audio side sees the new version: fade out (4 blocks), drop, silence while building */
        s_out = max_step(rec, 8 * CTL);
        sawfade = !wmem(0)->h.have;
        for (back = 0; back < 400u && !(wmem(0)->h.have && wmem(0)->h.have == wmem(0)->h.want); back++)
            render(1, 1);
        s_in = 0;
        render(30, 1);
        for (i = 0; i < 30 * (int32_t)CTL; i++)
            s_in = abs(rec[i]) > s_in ? abs(rec[i]) : s_in;
        for (f = 0; f < WV_F; f++)
            wv_frame(WV_NTF, f, ta + f * WV_FS);
        snprintf(m, sizeof m, "a slot stored again under a playing part: fades out and drops it (%s, largest step %d vs %d held), builds anew in %u blocks, plays the NEW table (peak %d)",
                 sawfade ? "yes" : "NO", s_out, steady, back, s_in);
        expect(m, sawfade && s_out <= steady * 3 / 2 && wmem(0)->h.have == WV_NTF + 1u && !memcmp(wmem(0)->tab, ta, sizeof ta) && s_in > 1000);
    }
    /* a slot changed in the middle of its own build: the job drops, the table is the new one whole (never frames of both) */
    {
        track_t *t;
        fresh();
        user_set(0, A, "SAWTILT");
        host_preset(&trk[0], ENGI_WAVE, 0);
        t = &trk[0];
        t->p[P_E0] = WV_NTF;
        wave_block(t);
        for (b = 0; b < 7u; b++)
            wave_service();
        user_set(0, B, "NEWER");                                    /* the editor's commit, in the main loop between two frames */
        expect("a slot stored in the middle of its build drops the job (wvj.on = 0) at once", !wvj.on);
        wave_build_now(t);
        for (f = 0; f < WV_F; f++)
            wv_frame(WV_NTF, f, ta + f * WV_FS);
        expect("..the table that is built is the new one whole (all 19 frames)", wmem(0)->h.have == WV_NTF + 1u && !memcmp(wmem(0)->tab, ta, sizeof ta));
    }
    /* a factory table's own recipe, encoded as user bytes (the converter's encoding, wtab_encode.h), sounds as the factory table: level 0 within a few steps.
     * (Only tables whose recipe has no negative harmonic: a sign is a phase flip, and user tables keep no phase. PWM, BAND etc. would differ in shape.) */
    {
        static const uint8_t TBL[] = {0, 2, 3, 4, 5, 6, 8, 12, 14};
        static uint8_t E[WV_F * WV_H];
        static int8_t fa[WV_FS];
        uint32_t t, fr, nn, tot = 0;
        int32_t w = 0;
        double sq = 0;
        for (t = 0; t < NELEM(TBL); t++) {
            wtab_encode(TBL[t], E);
            user_set(0, E, "ENC");
            for (fr = 0; fr < WV_F; fr++) {
                wv_frame(WV_NTF, fr, wvj.out);
                memcpy(fa, wvj.out, WV_FS);
                wv_frame(TBL[t], fr, wvj.out);
                for (nn = 0; nn < 256u; nn++) {
                    int32_t d = abs(fa[nn] - wvj.out[nn]);
                    w = d > w ? d : w;
                    sq += (double)d * d;
                    tot++;
                }
            }
        }
        snprintf(m, sizeof m, "factory recipes (SAWSQ HARM ODD ORGAN VOWEL BRASS RESO CHIME BAND) encoded as user bytes play as themselves: level 0 worst %d steps, rms %.2f (of 63.5)",
                 (int)w, sqrt(sq / tot));
        expect(m, w <= 4 && sqrt(sq / tot) < 1.0);
    }
    /* determinism: two builds of one slot give the same bytes; the factory tables are not touched by any of it (t_list's fingerprint) */
    for (f = 0; f < WV_F; f++) {
        wv_frame(WV_NTF, f, ta + f * WV_FS);
        wv_frame(WV_NTF, f, tb_ + f * WV_FS);
    }
    expect("a user table is deterministic (two builds, the same bytes)", !memcmp(ta, tb_, sizeof ta));
    {   /* the cost of a user table's build is a factory table's order (no recipe maths: the dearest factory tables are 2 M) */
        uint64_t i0 = instr_now(), c;
        for (f = 0; f < WV_F; f++)
            wv_frame(WV_NTF, f, wvj.out);
        c = instr_now() - i0;
        snprintf(m, sizeof m, "a user table's build %.2f M host instructions (under the 2.5 M of every factory table)", c / 1e6);
        expect(m, c < 2500000u);
    }
    (void)ref;
}

/* each part in a child of a process that never rendered (every state as at boot: voices, FX tails, seeds), as regress.c */
static int sub_run(void (*fn)(void))
{
    pid_t pid;
    int st = 0;
    fflush(stdout);
    pid = fork();
    if (!pid) {
        fn();
        fflush(stdout);
        _exit(bad ? 1 : 0);
    }
    if (pid < 0 || waitpid(pid, &st, 0) < 0)
        return 1;
    if (!WIFEXITED(st)) {
        printf("wave: CRASH\n");
        return 1;
    }
    return WEXITSTATUS(st) != 0;
}
static void t_alias_cost(void) { t_alias(); }
int main(void)
{
    int nbad = 0;
    verbose = getenv("WAVE_VERBOSE") != 0;
    nbad += sub_run(t_build);
    nbad += sub_run(t_list);
    nbad += sub_run(t_user);
    nbad += sub_run(t_fades);
    nbad += sub_run(t_newfades);
    nbad += sub_run(t_robust);
    nbad += sub_run(t_morph);
    nbad += sub_run(t_alias_cost);
    nbad += sub_run(t_syncbend);
    nbad += sub_run(t_cost);
    printf("wave: %s\n", nbad ? "FAILED" : "all ok");
    return nbad != 0;
}
