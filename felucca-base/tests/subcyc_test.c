/* SPDX-License-Identifier: GPL-3.0-only */
/* Cloud job F: the SUB LAYER (chain_sub.c), the CYCLE envelope (cycle.c) and WAVE STEPS (eng_wave.c wave_steps): what wave_test, mx_step_test and xvalues_test cannot show.
 * docs/design/SUB_AND_CYCLE_DESIGN.md. Every section runs in a child process (fork): a scene starts from the process's own power-on state, and the counters and render buffers are shared memory.
 *   REF     bit for bit the render of cloud/base3-wave2 (the hashes of subcyc_ref.h, made by building this file with -DSUBCYC_BASE against it): SUB LEVEL 0 (with the other sub values set),
 *           CYCLE set but read by no slot (a matrix on, another source), STEPS OFF: five scenes in STEREO, DUEL and LAYER on WAVE, FM6, ANALOG, PHASE, VOICE
 *   SUB     the wave, the octave and the level of the sub alone (the render with the sub less the render without it, the chain forced on in both: the difference is the sub): frequency per
 *           OCTAVE, the spectrum per WAVE and HARMONICS, the level curve, HARMONICS 0 bit for bit the plain wave, phase kept at a retrigger and 0 at a new note, no click (note start, steal,
 *           OCTAVE, WAVE, LEVEL), UNISON one undetuned sub a key at the poly level, glide and bend followed, FM6 follow (the sub against the loudest carrier), the aliasing of TRI and SQUARE
 *           at OCTAVE 0 on C6 (reported), the NOTES rows
 *   CYCLE   LOOP / ONCE, the curve, the times (the ATK table, SYNC's note values and CYCLE's own switch against ENV SYNC), a retrigger without a jump, legato keeps it, every voice from its
 *           own key-down, the read flag, unread = no tick
 *   STEPS   (wave_test.c has the engine's side: places, hysteresis, cross-fade, morph)
 * Built and run by host_tests.sh (name: subcyc) */
#define DUELL_CHAIN_FORCE 1
#define WV_TEST_HOOKS 1
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <sys/mman.h>
#include "subcyc_ref.h"

#define NB 800u
static struct { int ncheck, bad, sel; int32_t a[NB * CTL], b[NB * CTL], cyr[4][NB], cyr2[4][NB]; double rep[4]; } *sh;
static void ck(const char *what, int ok)
{
    sh->ncheck++;
    printf("subcyc: %-128s %s\n", what, ok ? "ok" : "FAIL");
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
        printf("subcyc: FAIL a section or scene ended with signal/status %d\n", st);
        sh->bad++;
    }
}
static void fresh(void)
{
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    memset(eng_mem_owner, 0, sizeof eng_mem_owner);
    memset(&wvj, 0, sizeof wvj);
    memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target);
    chain_force = 0;
}
static uint32_t scn_eng(const char *n)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (str_eq(ENGINES[e]->name, n))
            return e;
    return 0;
}
static uint32_t eng_idx_of(const engine_t *e)
{
    uint32_t i;
    for (i = 0; i < NENGINES; i++)
        if (ENGINES[i] == e)
            return i;
    return 0;
}
/* a WAVE part, dry: SAWSQ, POS 30, an instant attack, full sustain */
static void part_wave(uint32_t part, uint32_t table, int pos)
{
    track_t *t = &trk[part];
    host_preset(t, ENGI_WAVE, 0);
    t->p[P_E0] = (int16_t)table;
    t->p[P_E1] = (int16_t)pos;
    t->p[P_E2] = t->p[P_E3] = t->p[P_E4] = t->p[P_E5] = 0;
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_ATK] = 0;
    t->p[P_SUS] = 127;
    t->p[P_REL] = 20;
    t->p[P_VOICE] = V_POLY;
    t->p[P_TRIM] = 104;
    wave_build_now(t);
}
static int32_t *cur;
/* nb blocks of part 1's voices (the sum before pan, effects and the master: linear, the engine's oscillator at unity is VOICE_FS) into cur, from block b0 */
static void rec_blocks(uint32_t b0, uint32_t nb)
{
    int32_t o[CTL];
    uint32_t b, k;
    for (b = 0; b < nb; b++) {
        mod_begin(&trk[0]);
        track_render(&trk[0], o, CTL);
        if (MODW(&trk[0]).on)
            mod_end(&trk[0]);
        for (k = 0; k < CTL; k++)
            cur[(b0 + b) * CTL + k] = o[k];
    }
}

/* ------------------------------------------------------------------------------------------ scenes with events --- */
enum { EV_ON, EV_OFF, EV_P, EV_X, EV_BEND, EV_CB, EV_XA, EV_END };   /* EV_XA: an XV change in the first run of a pair only (the one with the sub) */
static int scn_a;
typedef struct { uint32_t blk; int kind, a, b; void (*cb)(int); } ev_t;
static ev_t EV[24];
static uint32_t nev, scn_nb;
static void ev_clear(void) { nev = 0; scn_nb = 200; }
static void ev_add(uint32_t blk, int kind, int a, int b) { EV[nev].blk = blk; EV[nev].kind = kind; EV[nev].a = a; EV[nev].b = b; EV[nev].cb = 0; nev++; }
static void ev_cb(uint32_t blk, void (*cb)(int), int a) { EV[nev].blk = blk; EV[nev].kind = EV_CB; EV[nev].a = a; EV[nev].cb = cb; nev++; }
static void ev_apply(uint32_t b)
{
    uint32_t i;
    for (i = 0; i < nev; i++)
        if (EV[i].blk == b)
            switch (EV[i].kind) {
            case EV_ON: trk_note_on(&trk[0], (uint32_t)EV[i].a, (uint32_t)EV[i].b); break;
            case EV_OFF: trk_note_off(&trk[0], (uint32_t)EV[i].a); break;
            case EV_P: trk[0].p[EV[i].a] = (int16_t)EV[i].b; break;
            case EV_X: XV(&trk[0], EV[i].a) = (int16_t)EV[i].b; break;
            case EV_XA: if (scn_a) XV(&trk[0], EV[i].a) = (int16_t)EV[i].b; break;
            case EV_BEND: midi_bend_target[0] = EV[i].a; break;
            case EV_CB: EV[i].cb(EV[i].a); break;
            default: break;
            }
}
/* the sub's settings of a scene (set at the start; EV_X changes them later) */
static struct { int wave, oct, harm, mode, eng, preset, uni_detune, glide, force; } SP;
static void sp_default(void)
{
    SP.wave = 0; SP.oct = 1; SP.harm = 0; SP.mode = V_POLY; SP.eng = -1; SP.preset = 0; SP.uni_detune = 0; SP.glide = 0; SP.force = 1;
}
static void scene_run(int level)                           /* the scene, the sub at LEVEL level, the chain forced on: into cur */
{
    uint32_t b;
    fresh();
    if (SP.eng < 0) {
        part_wave(0, 0, 30);
    } else {
        host_preset(&trk[0], (uint32_t)SP.eng, (uint32_t)SP.preset);
        trk[0].p[P_DIST] = trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
        trk[0].p[P_TRIM] = 104;
        trk[0].p[P_VOICE] = V_POLY;
    }
    trk[0].p[P_VOICE] = (int16_t)SP.mode;
    trk[0].p[P_DETUNE] = (int16_t)SP.uni_detune;
    trk[0].p[P_GLIDE] = (int16_t)SP.glide;
    XV(&trk[0], X_SUBLVL) = (int16_t)level;
    XV(&trk[0], X_SUBWAVE) = (int16_t)SP.wave;
    XV(&trk[0], X_SUBOCT) = (int16_t)SP.oct;
    XV(&trk[0], X_SUBHARM) = (int16_t)SP.harm;
    chain_force = (uint8_t)SP.force;
    for (b = 0; b < scn_nb; b++) {
        ev_apply(b);
        rec_blocks(b, 1);
    }
}
static void run_pair(int level, int32_t *d)                /* d = the render with the sub minus the render without it (both in the chain) */
{
    uint32_t i;
    int st = 0;
    pid_t p;
    int sel;
    for (sel = 0; sel < 2; sel++) {
        fflush(stdout);
        p = fork();
        if (p == 0) {
            cur = sel ? sh->b : sh->a;
            scn_a = !sel;
            scene_run(sel ? 0 : level);
            fflush(stdout);
            _exit(0);
        }
        waitpid(p, &st, 0);
    }
    for (i = 0; i < scn_nb * CTL; i++)
        d[i] = sh->a[i] - sh->b[i];
}
static int32_t D[NB * CTL];

/* ------------------------------------------------------------------------------------------------------ measures --- */
static int32_t peak_of(const int32_t *x, uint32_t a, uint32_t b)
{
    int32_t m = 0;
    uint32_t i;
    for (i = a; i < b; i++)
        m = abs(x[i]) > m ? abs(x[i]) : m;
    return m;
}
static int32_t maxstep_of(const int32_t *x, uint32_t a, uint32_t b)
{
    int32_t m = 0;
    uint32_t i;
    for (i = a + 1u; i < b; i++)
        m = abs(x[i] - x[i - 1u]) > m ? abs(x[i] - x[i - 1u]) : m;
    return m;
}
static double zc_freq(const int32_t *x, uint32_t a, uint32_t b)    /* Hz from the rising zero crossings */
{
    uint32_t i, n = 0;
    double first = 0, last = 0;
    for (i = a + 1u; i < b; i++)
        if (x[i - 1u] < 0 && x[i] >= 0) {
            double pos = (double)i - 1.0 + (double)(-x[i - 1u]) / (double)(x[i] - x[i - 1u]);
            if (!n)
                first = pos;
            last = pos;
            n++;
        }
    return n > 1u ? (double)(n - 1u) * FS / (last - first) : 0;
}
static double goertzel(const int32_t *x, uint32_t a, uint32_t b, double f)   /* amplitude of f (Hann window) */
{
    double re = 0, im = 0, w = 0, n = (double)(b - a);
    uint32_t i;
    for (i = a; i < b; i++) {
        double h = 0.5 - 0.5 * cos(2 * M_PI * (double)(i - a) / (n - 1)), ph = 2 * M_PI * f * (double)i / FS;
        re += h * x[i] * cos(ph);
        im += h * x[i] * sin(ph);
        w += h;
    }
    return 2 * sqrt(re * re + im * im) / w;
}
static double note_hz(int n) { return 440.0 * pow(2.0, (n - 69) / 12.0); }
static void fft(double *re, double *im, uint32_t n)
{
    uint32_t i, j = 0, k, m;
    for (i = 1; i < n; i++) {
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
        double ang = -2 * M_PI / (double)m;
        for (i = 0; i < n; i += m)
            for (k = 0; k < m / 2u; k++) {
                double wr = cos(ang * k), wi = sin(ang * k), xr = re[i + k + m / 2u] * wr - im[i + k + m / 2u] * wi, xi = re[i + k + m / 2u] * wi + im[i + k + m / 2u] * wr;
                re[i + k + m / 2u] = re[i + k] - xr;
                im[i + k + m / 2u] = im[i + k] - xi;
                re[i + k] += xr;
                im[i + k] += xi;
            }
    }
}

/* ============================================================================================================ REF === */
/* five scenes whose hash is the one cloud/base3-wave2 gives: the sub's values set with LEVEL 0, CYCLE's values set and read by no slot, STEPS at OFF */
static uint64_t ref_h;
static void ref_feed(uint32_t v) { ref_h ^= v; ref_h *= 0x100000001B3ull; }
static uint64_t ref_scene(uint32_t id)
{
    static const uint8_t C1[4] = {48, 55, 60, 64}, C2[3] = {67, 72, 76};
    static int32_t o[2 * CTL];
    uint32_t k, i, b, nb = 500;
    track_t *t0 = &trk[0], *t1 = &trk[1];
    ref_h = 0xCBF29CE484222325ull;
    fresh();
    host_tracks_init();
    host_preset(t0, id == 0 || id == 3 ? ENGI_WAVE : scn_eng(id == 1 ? "FM6" : id == 2 ? "ANALOG" : "VOICE"), 1);
    host_preset(t1, id == 1 ? scn_eng("PHASE") : id == 4 ? scn_eng("WAVE") : scn_eng("ANALOG"), 2);
    if (id == 0 || id == 3)
        wave_build_now(t0);
    if (id == 4)
        wave_build_now(t1);
    song.g[G_RTYPE] = 2;
    song.g[G_DMIX] = 100;
    for (k = 0; k < 2u; k++) {
        track_t *t = &trk[k];
        t->p[P_TRIM] = 110;
        t->p[P_REV] = 80;
        t->p[P_SUS] = 100;
        t->p[P_REL] = 60;
        t->p[P_VOICE] = V_POLY;
        /* the sub's values set, LEVEL 0 (the default): it is not run; CYCLE's values set, no slot reads it; the matrix on, with another source */
        XV(t, X_SUBWAVE) = 2;
        XV(t, X_SUBOCT) = 2;
        XV(t, X_SUBHARM) = 90;
        XV(t, X_CYMODE) = 1;
        XV(t, X_CYRISE) = 20;
        XV(t, X_CYFALL) = 30;
        XV(t, X_CYCURVE) = -20;
        XV(t, X_CYSYNC) = 1;
        t->p[P_M1SRC] = MS_LFO;
        t->p[P_M1DST] = id == 3 ? MD_SHP : MD_PAN;
        t->p[P_M1AMT] = id == 3 ? 40 : 50;
        t->p[P_M2SRC] = MS_ENV2;
        t->p[P_M2DST] = MD_CUT;
        t->p[P_M2AMT] = 30;
        t->p[P_E2ATK] = 20;
    }
    t0->p[P_PAN] = -30;
    t1->p[P_PAN] = 30;
    if (id == 2)
        t0->p[P_SPRD] = 80;
    if (id == 3) {                                      /* UNISON, a legato second part */
        t0->p[P_VOICE] = V_UNISON;
        t0->p[P_DETUNE] = 40;
        t1->p[P_VOICE] = V_LEGATO;
        t1->p[P_GLIDE] = 40;
    }
    if (id == 1)
        duell_mode_force(DM_SPLIT);
    if (id == 4)
        duell_mode_force(DM_LAYER);
    for (b = 0; b < nb; b++) {
        if (b == 10u) {
            for (i = 0; i < 4u; i++)
                trk_note_on(t0, C1[i], 110);
            for (i = 0; i < 3u; i++)
                trk_note_on(t1, C2[i], 100);
        }
        if (b == 200u)
            for (i = 0; i < 3u; i++)
                trk_note_on(t1, C2[i] + 2u, 90);      /* a new note while others sound: a steal or a legato step */
        if (b == 300u) {
            for (i = 0; i < 4u; i++)
                trk_note_off(t0, C1[i]);
            for (i = 0; i < 6u; i++)
                trk_note_off(t1, C2[i % 3u] + (i / 3u) * 2u);
        }
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++)
            ref_feed((uint32_t)o[i]);
    }
    return ref_h;
}
static uint64_t ref_got[5];
static void t_ref_one(void)
{
    uint32_t id;
    for (id = 0; id < 5u; id++) {
        int fd[2], st = 0;
        pid_t p;
        uint64_t h = 0;
        if (pipe(fd))
            continue;
        fflush(stdout);
        p = fork();
        if (p == 0) {
            uint64_t v = ref_scene(id);
            if (write(fd[1], &v, sizeof v) != (ssize_t)sizeof v)
                _exit(1);
            _exit(0);
        }
        close(fd[1]);
        if (read(fd[0], &h, sizeof h) != (ssize_t)sizeof h)
            h = 0;
        close(fd[0]);
        waitpid(p, &st, 0);
        ref_got[id] = h;
    }
}
static void t_ref(void)
{
    static const char *const NAME[5] = {"WAVE + ANALOG, STEREO, matrix LFO -> PAN", "FM6 + PHASE, DUEL", "ANALOG (SPREAD) + ANALOG, STEREO", "WAVE UNISON + ANALOG LEGATO, matrix LFO -> SHP (STEPS OFF)", "VOICE + WAVE, LAYER"};
    uint32_t id;
    t_ref_one();
#ifdef SUBCYC_REFGEN
    for (id = 0; id < 5u; id++)
        printf("SUBCYC_REF %u 0x%016llxull\n", id, (unsigned long long)ref_got[id]);
#else
    for (id = 0; id < 5u; id++) {
        char m[200];
        snprintf(m, sizeof m, "REF %u (%s): bit for bit cloud/base3-wave2 (SUB LEVEL 0, CYCLE unread, STEPS OFF)", id, NAME[id]);
        ck(m, ref_got[id] && ref_got[id] == SUBCYC_REF[id]);
    }
#endif
}

#ifndef SUBCYC_BASE
/* ============================================================================================================ SUB === */
static void sub_freq(void)
{
    static const struct { int oct; double div; } O[3] = {{0, 4}, {1, 2}, {2, 1}};
    static const char *const WN[4] = {"SINE", "TRI", "SQUARE", "PULSE"};
    uint32_t i, w;
    char m[200];
    int ok = 1;
    double fr[12];
    for (w = 0; w < 4u; w++)
        for (i = 0; i < 3u; i++) {
            sp_default();
            SP.wave = (int)w;
            SP.oct = O[i].oct;
            ev_clear();
            scn_nb = 160;
            ev_add(0, EV_ON, 48, 100);
            run_pair(100, D);
            fr[w * 3u + i] = zc_freq(D, 60u * CTL, 160u * CTL);
            ok &= fabs(fr[w * 3u + i] / (note_hz(48) / O[i].div) - 1.0) < 0.004;
        }
    snprintf(m, sizeof m, "OCTAVE -2 / -1 / 0 on C3: %.2f / %.2f / %.2f Hz (SINE; the note %.2f Hz: / 4, / 2, x 1); TRI, SQUARE, PULSE the same within 0.4 %%", fr[0], fr[1], fr[2], note_hz(48));
    ck(m, ok);
    (void)WN;
}
static void sub_curve(void)
{
    uint32_t i;
    int lv[5] = {10, 40, 70, 100, 127};
    double amp[5], want[5];
    char m[200];
    int ok = 1;
    sh->rep[0] = 0;
    for (i = 0; i < 5u; i++) {
        sp_default();
        SP.oct = 2;
        ev_clear();
        scn_nb = 120;
        ev_add(0, EV_ON, 60, 127);
        run_pair(lv[i], D);
        amp[i] = (double)peak_of(D, 60u * CTL, 120u * CTL);
        fresh();
        XV(&trk[0], X_SUBLVL) = (int16_t)lv[i];
        want[i] = (double)sub_gain(&trk[0], &trk[0].v[0]);
        if (i)
            ok &= fabs((amp[i] / amp[0]) / (want[i] / want[0]) - 1.0) < 0.03;
    }
    snprintf(m, sizeof m, "LEVEL 10 .. 127 (SINE, 0 oct): peak %.0f %.0f %.0f %.0f %.0f follows the curve's gain %.0f %.0f %.0f %.0f %.0f within 3 %%", amp[0], amp[1], amp[2], amp[3], amp[4], want[0], want[1], want[2], want[3], want[4]);
    ck(m, ok && amp[4] > amp[3] && amp[1] > amp[0]);
    {   /* 127 against WAVE's own sine (the EMPTY user slot, POS 0): reported */
        int32_t pe;
        fresh();
        part_wave(0, 26, 0);
        trk_note_on(&trk[0], 60, 127);
        cur = sh->a;
        rec_blocks(0, 120);
        pe = peak_of(sh->a, 60u * CTL, 120u * CTL);
        snprintf(m, sizeof m, "LEVEL 127, SINE, 0 oct: peak %.0f against %d of the engine's own sine (WAVE, EMPTY slot): %.2f x (about a full-scale engine oscillator; the owner sets the curve by ear)", amp[4], pe, amp[4] / pe);
        ck(m, amp[4] / pe > 0.5 && amp[4] / pe < 2.5);
    }
}
static void sub_spectrum(void)
{
    char m[220];
    double f = note_hz(48) / 2, a1, a2, a3, a4;
    int ok;
    sp_default();
    ev_clear();
    scn_nb = 520;
    ev_add(0, EV_ON, 48, 100);
    SP.harm = 0;
    run_pair(110, D);
    a1 = goertzel(D, 100u * CTL, 520u * CTL, f);
    a2 = goertzel(D, 100u * CTL, 520u * CTL, 2 * f);
    a4 = goertzel(D, 100u * CTL, 520u * CTL, 4 * f);
    ok = a2 / a1 < 0.01 && a4 / a1 < 0.01;
    snprintf(m, sizeof m, "SINE -1, HARMONICS 0: the sub alone (octave above %.4f, two octaves %.4f of the sub): nothing runs", a2 / a1, a4 / a1);
    ck(m, ok);
    SP.harm = 64;
    run_pair(110, D);
    a1 = goertzel(D, 100u * CTL, 520u * CTL, f);
    a2 = goertzel(D, 100u * CTL, 520u * CTL, 2 * f);
    a4 = goertzel(D, 100u * CTL, 520u * CTL, 4 * f);
    snprintf(m, sizeof m, "HARMONICS 64 (disabled since 0.2.31, forced to 0): the octave above at %.3f of the sub (0), two octaves above %.3f (0)", a2 / a1, a4 / a1);
    ck(m, a2 / a1 < 0.01 && a4 / a1 < 0.01);
    SP.harm = 127;
    run_pair(110, D);
    a1 = goertzel(D, 100u * CTL, 520u * CTL, f);
    a2 = goertzel(D, 100u * CTL, 520u * CTL, 2 * f);
    a4 = goertzel(D, 100u * CTL, 520u * CTL, 4 * f);
    snprintf(m, sizeof m, "HARMONICS 127 (disabled since 0.2.31, forced to 0): octave above %.3f, two octaves above %.3f of the sub (0, 0)", a2 / a1, a4 / a1);
    ck(m, a2 / a1 < 0.01 && a4 / a1 < 0.01);
    SP.harm = 0;
    SP.wave = 1;
    run_pair(110, D);
    a1 = goertzel(D, 100u * CTL, 520u * CTL, f);
    a3 = goertzel(D, 100u * CTL, 520u * CTL, 3 * f);
    snprintf(m, sizeof m, "TRI: the third harmonic %.3f of the fundamental (1/9 = 0.111)", a3 / a1);
    ck(m, a3 / a1 > 0.09 && a3 / a1 < 0.135);
    SP.wave = 2;
    run_pair(110, D);
    a1 = goertzel(D, 100u * CTL, 520u * CTL, f);
    a2 = goertzel(D, 100u * CTL, 520u * CTL, 2 * f);
    a3 = goertzel(D, 100u * CTL, 520u * CTL, 3 * f);
    snprintf(m, sizeof m, "SQUARE: odd harmonics (third %.3f of the fundamental, 1/3 = 0.333), no even ones (second %.4f)", a3 / a1, a2 / a1);
    ck(m, a3 / a1 > 0.28 && a3 / a1 < 0.38 && a2 / a1 < 0.03);
    {
        double a1s = a1;
        SP.wave = 3;
        run_pair(110, D);
        a1 = goertzel(D, 100u * CTL, 520u * CTL, f);
        a2 = goertzel(D, 100u * CTL, 520u * CTL, 2 * f);
        snprintf(m, sizeof m, "PULSE (width 1/4): even harmonics present (second %.3f of the fundamental, 0.707 for a 25 %% pulse); the fundamental %.2f x the SQUARE's (0.47: the same peak, 1/4 of the time)", a2 / a1, a1 / a1s);
        ck(m, a2 / a1 > 0.55 && a2 / a1 < 0.85 && a1 / a1s > 0.42 && a1 / a1s < 0.53);
    }
}
/* the formula, exactly: sub_pass with no harmonics is the wave times the gain, sample for sample */
static void sub_exact(void)
{
    int32_t tmp[CTL], ref[CTL];
    uint32_t i, w, ph = 0x12345678u, st = 0x00ABCDEFu, ok = 1, cfg;
    int32_t g0 = 5000, g1 = 17000, gq;
    for (w = 0; w < 3u; w++) {
        cfg = w | 1u << 2;
        for (i = 0; i < CTL; i++)
            tmp[i] = (int32_t)i * 7;
        sub_pass(tmp, cfg, ph, st, g0, g1, 0, 0, CTL);
        gq = g0 << 8;
        for (i = 0; i < CTL; i++) {
            uint32_t p = (ph << 1) + (st << 1) * i;
            int32_t s = w == 0u ? sine_i(p) : w == 1u ? osc_tri(p + 0x40000000u) : (osc_pulse(p, st << 1, 0x80000000u) * 32768) >> 14;
            gq += ((g1 - g0) << 8) / (int32_t)CTL;
            ref[i] = (int32_t)i * 7 + ((s * (gq >> 9)) >> 14);
        }
        ok &= !memcmp(tmp, ref, sizeof tmp);
    }
    ck("HARMONICS 0: the wave times the gain's ramp, sample for sample (SINE, TRI, SQUARE at -1; sub_pass with no harmonics runs none)", ok);
}
static void sub_levels(void)
{
    int ok = 1;
    uint32_t i;
    char m[200];
    fresh();
    part_wave(0, 0, 30);
    XV(&trk[0], X_SUBWAVE) = 2;
    XV(&trk[0], X_SUBHARM) = 100;
    ok &= !chain_sub_on(&trk[0], &trk[0].v[0]) && !chain_on(&trk[0], &trk[0].v[0]);
    for (i = 0; i < 8u; i++)
        ok &= !chain_sub_on(&trk[0], &trk[0].v[i]);
    XV(&trk[0], X_SUBLVL) = 1;
    ok &= chain_sub_on(&trk[0], &trk[0].v[3]) && sub_gain(&trk[0], &trk[0].v[3]) > 0;
    XV(&trk[0], X_SUBLVL) = 127;
    ok &= sub_gain(&trk[0], &trk[0].v[0]) >= 23000 && sub_gain(&trk[0], &trk[0].v[0]) <= 24000;
    trk[0].p[P_VOICE] = V_UNISON;
    ok &= chain_sub_on(&trk[0], &trk[0].v[0]) && !chain_sub_on(&trk[0], &trk[0].v[1]) && !chain_sub_on(&trk[0], &trk[0].v[7]);
    snprintf(m, sizeof m, "LEVEL 0: chain_sub_on and chain_on are 0 for every voice; LEVEL 1 .. 127 on (gain %d at 127 of VOICE_FS %d); UNISON: voice 0 only", (int)sub_gain(&trk[0], &trk[0].v[0]), VOICE_FS);
    ck(m, ok);
}
/* a retrigger keeps the phase, a new note starts it at 0 */
static void sub_phase(void)
{
    uint32_t pa, pb, pc, pd, pn, d1;
    char m[200];
    fresh();
    part_wave(0, 0, 30);
    trk[0].p[P_VOICE] = V_MONO;
    XV(&trk[0], X_SUBLVL) = 90;
    chain_force = 1;
    trk_note_on(&trk[0], 52, 100);
    cur = sh->a;
    rec_blocks(0, 20);
    pa = sub_ph[0][0];
    rec_blocks(20, 1);
    pb = sub_ph[0][0];
    d1 = pb - pa;
    trk_note_on(&trk[0], 52, 100);                      /* MONO: the same voice again, the envelope retriggered */
    rec_blocks(21, 1);
    pc = sub_ph[0][0];
    rec_blocks(22, 1);
    pd = sub_ph[0][0];
    snprintf(m, sizeof m, "a retrigger in place keeps the sub's phase: a block adds %u before, %u across it, %u after (a restart would reset it)", d1, pc - pb, pd - pc);
    ck(m, d1 > 1000u && pc - pb == d1 && pd - pc == d1);
    fresh();
    part_wave(0, 0, 30);
    XV(&trk[0], X_SUBLVL) = 90;
    chain_force = 1;
    trk_note_on(&trk[0], 52, 100);
    rec_blocks(0, 1);
    pn = sub_ph[0][0];
    snprintf(m, sizeof m, "a new note starts the sub's phase at 0: after its first block it is %u (a block's step %u)", pn, d1);
    ck(m, pn == d1);
}
/* the clicks: the largest step of the sub alone against a steady stretch of the same sub */
static void sub_clicks(void)
{
    char m[220];
    int32_t st, big;
    uint32_t i;
    /* note start: the amp ramp comes from 0 */
    sp_default();
    SP.wave = 1;
    SP.oct = 2;
    ev_clear();
    scn_nb = 120;
    ev_add(20, EV_ON, 48, 127);
    run_pair(127, D);
    st = maxstep_of(D, 80u * CTL, 120u * CTL);
    big = maxstep_of(D, 0, 80u * CTL);
    snprintf(m, sizeof m, "a note start (TRI, 0 oct, LEVEL 127): the largest step %d against the steady %d (<= 2 x: the amp ramp on a rising wave); the first sample %d", big, st, D[20u * CTL]);
    ck(m, big <= st * 2 && st > 100 && abs(D[20u * CTL]) < st / 4);
    /* steal: eight notes held, a ninth */
    sp_default();
    SP.oct = 1;
    ev_clear();
    scn_nb = 160;
    for (i = 0; i < 8u; i++)
        ev_add(2, EV_ON, 40 + (int)i * 3, 100);
    ev_add(80, EV_ON, 71, 100);
    run_pair(110, D);
    st = maxstep_of(D, 40u * CTL, 78u * CTL);
    big = maxstep_of(D, 78u * CTL, 160u * CTL);
    {
        int32_t st2 = maxstep_of(D, 120u * CTL, 160u * CTL);
        snprintf(m, sizeof m, "a steal (eight SINE subs, a ninth note, a higher pitch): the largest step %d against the steady %d before and %d after (<= 1.5 x the larger)", big, st, st2);
        ck(m, big <= (st2 > st ? st2 : st) * 3 / 2);
    }
    /* OCTAVE -2 -> 0 and 0 -> -1, SINE */
    sp_default();
    SP.oct = 0;
    ev_clear();
    scn_nb = 200;
    ev_add(0, EV_ON, 48, 110);
    ev_add(60, EV_X, X_SUBOCT, 2);
    ev_add(120, EV_X, X_SUBOCT, 1);
    run_pair(110, D);
    st = maxstep_of(D, 20u * CTL, 58u * CTL);
    {
        int32_t st0 = maxstep_of(D, 100u * CTL, 118u * CTL);
        big = maxstep_of(D, 58u * CTL, 200u * CTL);
        snprintf(m, sizeof m, "OCTAVE -2 -> 0 -> -1 on a held note: the largest step %d against the steady %d (-2) and %d (0): <= 1.5 x the larger", big, st, st0);
        ck(m, big <= (st0 > st ? st0 : st) * 3 / 2 && st0 > 100);
    }
    /* WAVE SINE -> TRI -> SINE */
    sp_default();
    SP.oct = 2;
    ev_clear();
    scn_nb = 200;
    ev_add(0, EV_ON, 48, 110);
    ev_add(60, EV_X, X_SUBWAVE, 1);
    ev_add(120, EV_X, X_SUBWAVE, 0);
    run_pair(110, D);
    st = maxstep_of(D, 20u * CTL, 58u * CTL);
    big = maxstep_of(D, 58u * CTL, 200u * CTL);
    snprintf(m, sizeof m, "WAVE SINE -> TRI -> SINE on a held note: the largest step %d against the steady %d (<= 1.5 x)", big, st);
    ck(m, big <= st * 3 / 2 && st > 100);
    {   /* the cross-fade is four blocks: the state, the old one on the blocks 60..63 */
        uint32_t c = 0, b;
        fresh();
        part_wave(0, 0, 30);
        XV(&trk[0], X_SUBLVL) = 100;
        XV(&trk[0], X_SUBWAVE) = 0;
        chain_force = 1;
        trk_note_on(&trk[0], 48, 100);
        cur = sh->a;
        rec_blocks(0, 30);
        XV(&trk[0], X_SUBWAVE) = 1;
        for (b = 0; b < 8u; b++) {
            rec_blocks(30 + b, 1);
            c += (sub_cfg[0][0] >> 8 & 7u) != 0u;
        }
        snprintf(m, sizeof m, "a WAVE switch: the old and the new run together for SUB_XFB = 4 blocks (the counter reads non-zero after %u of them, 0 after the 4th)", c);
        ck(m, c == 3u && (sub_cfg[0][0] & 15u) == (1u | 1u << 2));
    }
    /* LEVEL turned: 0 -> 127 -> 0 -> 60 on a held note */
    sp_default();
    SP.oct = 2;
    SP.wave = 2;
    ev_clear();
    scn_nb = 220;
    ev_add(0, EV_ON, 48, 110);
    ev_add(50, EV_X, X_SUBLVL, 127);
    ev_add(100, EV_X, X_SUBLVL, 0);
    ev_add(150, EV_X, X_SUBLVL, 60);
    run_pair(1, D);                                     /* (the pair's first run starts at LEVEL 1; the events move it) */
    st = maxstep_of(D, 70u * CTL, 98u * CTL);
    big = maxstep_of(D, 48u * CTL, 220u * CTL);
    snprintf(m, sizeof m, "LEVEL 1 -> 127 -> 0 -> 60 on a held SQUARE: the largest step %d against the steady %d (<= 1.5 x; the sub's gain glides per sample)", big, st);
    ck(m, big <= st * 3 / 2 && st > 100);
}
/* the product's own path (the chain not forced: a voice enters it when LEVEL goes above 0 and leaves it at 0, cross-faded over a block): the sub's turn on and off on a held note */
static void sub_enter_leave(void)
{
    char m[220];
    int32_t st, big;
    sp_default();
    SP.force = 0;
    SP.oct = 2;
    SP.wave = 0;
    ev_clear();
    scn_nb = 220;
    ev_add(0, EV_ON, 48, 110);
    ev_add(60, EV_XA, X_SUBLVL, 110);
    ev_add(140, EV_XA, X_SUBLVL, 0);
    run_pair(0, D);                                     /* (the pair's second run is the same scene at LEVEL 0 all along; D = the sub plus the plain-path / chain rounding, a few units) */
    st = maxstep_of(D, 90u * CTL, 135u * CTL);
    big = maxstep_of(D, 40u * CTL, 220u * CTL);
    snprintf(m, sizeof m, "the product's path: LEVEL 0 -> 110 -> 0 on a held note (the voice enters and leaves the chain, cross-faded a block): the largest step %d against the steady %d (<= 2 x: a one-block fade on a sine, as a note start)", big, st);
    ck(m, big <= st * 2 + 40 && st > 100 && peak_of(D, 200u * CTL, 220u * CTL) < 40);
}
static void sub_unison(void)
{
    char m[220];
    double fp, fu, ap, au;
    sp_default();
    SP.oct = 1;
    ev_clear();
    scn_nb = 200;
    ev_add(0, EV_ON, 48, 100);
    run_pair(100, D);
    fp = zc_freq(D, 80u * CTL, 200u * CTL);
    ap = (double)peak_of(D, 80u * CTL, 200u * CTL);
    SP.mode = V_UNISON;
    SP.uni_detune = 127;
    run_pair(100, D);
    fu = zc_freq(D, 80u * CTL, 200u * CTL);
    au = (double)peak_of(D, 80u * CTL, 200u * CTL);
    snprintf(m, sizeof m, "UNISON (DETUNE 127, eight voices): one sub, undetuned: %.2f Hz against POLY's %.2f (a detuned voice would be 1 to 2 %% off); the level %.0f against POLY's %.0f", fu, fp, au, ap);
    ck(m, fabs(fu / fp - 1) < 0.002 && fabs(au / ap - 1) < 0.06);
    {   /* the state: only voice 0 ever ran a sub, the other seven never left rest */
        uint32_t i, others = 0;
        for (i = 1; i < NVOICE; i++)
            others |= sub_g[0][i] | sub_ph[0][i];
        snprintf(m, sizeof m, "  .. the state of the eight voices after the render: voice 0 gain %u, phase running; voices 1..7: nothing (%u)", sub_g[0][0], others);
        ck(m, sub_g[0][0] > 0u && !others);
    }
}
static void sub_pitch(void)
{
    char m[220];
    double f0, f1, fm;
    uint32_t i;
    int mono = 1;
    /* glide: LEGATO, TIME glide between held notes */
    sp_default();
    SP.mode = V_LEGATO;
    SP.glide = 60;
    SP.oct = 2;
    ev_clear();
    scn_nb = 400;
    ev_add(0, EV_ON, 48, 100);
    ev_add(100, EV_ON, 60, 100);
    run_pair(100, D);
    f0 = zc_freq(D, 60u * CTL, 98u * CTL);
    f1 = zc_freq(D, 350u * CTL, 400u * CTL);
    fm = 0;
    for (i = 100; i < 330u; i += 10u) {                    /* the frequency in windows of 10 blocks: rises, never falls back */
        double f = zc_freq(D, i * CTL, (i + 10u) * CTL);
        if (f > 0 && f < fm * 0.97)
            mono = 0;
        fm = f > fm ? f : fm;
    }
    snprintf(m, sizeof m, "GLIDE: the sub goes %.2f Hz -> %.2f Hz with the notes C3 -> C4 (%.2f -> %.2f Hz), rising through the slide (no step)", f0, f1, note_hz(48), note_hz(60));
    ck(m, fabs(f0 / note_hz(48) - 1) < 0.004 && fabs(f1 / note_hz(60) - 1) < 0.004 && mono);
    /* bend: +2 semitones */
    sp_default();
    SP.oct = 2;
    ev_clear();
    scn_nb = 300;
    ev_add(0, EV_ON, 48, 100);
    ev_add(100, EV_BEND, 2 * 256, 0);
    run_pair(100, D);
    f0 = zc_freq(D, 60u * CTL, 98u * CTL);
    f1 = zc_freq(D, 200u * CTL, 300u * CTL);
    snprintf(m, sizeof m, "BEND +2 semitones: the sub's frequency x %.4f (2^(2/12) = 1.1225)", f1 / f0);
    ck(m, fabs(f1 / f0 / 1.12246 - 1) < 0.004);
}
/* FM6: the sub follows the loudest carrier. The sub alone (SINE, 0 oct, LEVEL 100) in windows of 6 blocks (a period of C4 is 5.3), against the follow value (fm6_sub_follow: the loudest carrier's gain_out
 * over a full carrier's) at the middle of the window: the sub's peak is the curve's gain x the follow, whatever the preset's envelope does (a decay of 40 dB, the release); the engine's own peak is
 * reported next to it (carriers add, modulators change the colour: not a gate); the sub ends with the voice, without a step */
static void sub_fm6(void)
{
    uint32_t fm = eng_idx_of(&ENG_FM6), pi, b, np = ENG_FM6.npresets, ok_all = 0, tested = 0;
    char m[300];
    double worst = 0, eng_lo = 1e9, eng_hi = 0;
    int32_t tail_step = 0, tail_steady = 0, ok_tail = 1;
    for (pi = 0; pi < np && pi < 8u; pi++) {
        static double gk[NB];
        uint32_t n = 0, nb = 520;
        double err = 0, want;
        sp_default();
        SP.eng = (int)fm;
        SP.preset = (int)pi;
        SP.oct = 2;
        ev_clear();
        scn_nb = nb;
        ev_add(0, EV_ON, 60, 110);
        ev_add(300, EV_OFF, 60, 0);
        run_pair(100, D);                                /* (sh->b: the engine alone) */
        {   /* the follow values: the same scene again, reading the engine's state after each block */
            int st = 0, fd[2];
            pid_t p;
            if (pipe(fd))
                continue;
            fflush(stdout);
            p = fork();
            if (p == 0) {
                static double g[NB];
                uint32_t k;
                fresh();
                host_preset(&trk[0], fm, pi);
                trk[0].p[P_DIST] = trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
                trk[0].p[P_TRIM] = 104;
                XV(&trk[0], X_SUBLVL) = 100;
                chain_force = 1;
                cur = sh->b;
                for (k = 0; k < nb; k++) {
                    ev_apply(k);
                    rec_blocks(k, 1);
                    g[k] = (double)fm6_sub_follow(&trk[0], &trk[0].v[0]);
                }
                if (write(fd[1], g, sizeof g) < 0)
                    _exit(1);
                _exit(0);
            }
            close(fd[1]);
            if (read(fd[0], gk, sizeof gk) < 0)
                memset(gk, 0, sizeof gk);
            close(fd[0]);
            waitpid(p, &st, 0);
        }
        fresh();
        XV(&trk[0], X_SUBLVL) = 100;
        want = (double)sub_gain(&trk[0], &trk[0].v[0]) / 32768.0;     /* (the engine's own scale: the FM6 follow at 1.0 is a carrier at full) */
        for (b = 20; b + 6u < nb; b += 12u) {
            double sk = (double)peak_of(D, b * CTL, (b + 6u) * CTL), g = gk[b + 3u] / 32768.0 * 32768.0, eng = (double)peak_of(sh->b, b * CTL, (b + 6u) * CTL);
            if (g > 300 && gk[b] > 300 && gk[b + 6u] > 300 && sk > 20 && fabs(gk[b] - gk[b + 6u]) < 0.02 * g) {   /* (steady enough over the window) */
                double r = sk / g / want - 1;
                if (getenv("SUBCYC_VERBOSE"))
                    printf("  fm6 p%u b%u gk %.0f sub %.0f ratio-err %.3f eng %.0f\n", pi, b, g, sk, r, eng);
                err = fmax(err, fabs(r));
                n++;
                if (g < 32000)
                    eng_lo = fmin(eng_lo, eng / g), eng_hi = fmax(eng_hi, eng / g);
            }
        }
        if (getenv("SUBCYC_VERBOSE"))
            printf("  fm6 preset %u: %u windows, worst error %.3f\n", pi, n, err);
        if (n > 8u) {
            tested++;
            ok_all += err < 0.05;
            worst = fmax(worst, err);
        }
        /* the end of the voice: the sub's step in the last blocks against the steady */
        tail_steady = maxstep_of(D, 10u * CTL, 300u * CTL);   /* (the note: its own steps, the attack on) */
        tail_step = maxstep_of(D, 300u * CTL, nb * CTL);
        if (getenv("SUBCYC_VERBOSE"))
            printf("  fm6 preset %u: tail step %d, steady %d, peak %d\n", pi, tail_step, tail_steady, peak_of(D, 0, nb * CTL));
        ok_tail &= tail_step <= tail_steady * 3 / 2 + 8;
    }
    snprintf(m, sizeof m, "FM6 follow: on %u of %u presets the sub's level is the curve's gain x the loudest carrier's level within 5 %% over the decay and the release (worst %.1f %%)", ok_all, tested, worst * 100);
    ck(m, tested >= 4u && ok_all == tested);
    snprintf(m, sizeof m, "  .. REPORT: the engine's own peak is %.2f to %.2f x the loudest carrier's gain_out (Q15 scale, below a full carrier), so the sub rides the voice's real level; the sub ends with the voice, without a step (%d)", eng_lo, eng_hi, tail_step);
    ck(m, ok_tail);
}
/* the aliasing of TRI and SQUARE at OCTAVE 0 on C6, reported: the energy outside the note's harmonics */
static void sub_alias(void)
{
    static double re[16384], im[16384];
    uint32_t w, i, k;
    char m[260];
    double res[2];
    for (w = 1; w <= 2u; w++) {
        double tot = 0, off = 0, f0 = note_hz(84);
        sp_default();
        SP.wave = (int)w;
        SP.oct = 2;
        ev_clear();
        scn_nb = 600;
        ev_add(0, EV_ON, 84, 127);
        run_pair(127, D);
        for (i = 0; i < 16384u; i++) {
            double h = 0.5 - 0.5 * cos(2 * M_PI * i / 16383.0);
            re[i] = h * D[100u * CTL + i];
            im[i] = 0;
        }
        fft(re, im, 16384);
        for (k = 1; k < 8192u; k++) {
            double f = (double)k * FS / 16384.0, e = re[k] * re[k] + im[k] * im[k], q = f / f0, near = fabs(q - floor(q + 0.5)) * f0 < 6.0;
            tot += e;
            if (!near && q > 0.5)
                off += e;
        }
        res[w - 1u] = 10 * log10(off / tot);
    }
    snprintf(m, sizeof m, "REPORT: aliasing at OCTAVE 0 on C6 (1046.5 Hz): the energy off the note's harmonics is %.1f dB (TRI, plain osc_tri) and %.1f dB (SQUARE, polyBLEP) of the sub", res[0], res[1]);
    ck(m, res[0] < 0 && res[1] < 0);
}
/* the NOTES rows */
static void sub_budget(void)
{
    uint32_t c0, c1, c2, c3, f0, f1;
    char m[220];
    fresh();
    part_wave(0, 0, 30);
    c0 = part_cost(&trk[0]);
    f0 = part_fixed(&trk[0]);
    XV(&trk[0], X_SUBLVL) = 80;
    c1 = part_cost(&trk[0]);
    XV(&trk[0], X_SUBWAVE) = 2;
    XV(&trk[0], X_SUBHARM) = 100;
    c2 = part_cost(&trk[0]);
    trk[0].p[P_VOICE] = V_UNISON;
    c3 = part_cost(&trk[0]);
    f1 = part_fixed(&trk[0]);
    snprintf(m, sizeof m, "NOTES: a voice costs %u (no sub), %u (SINE sub, est. +23), %u (SQUARE, HARMONICS 100, est. +69); UNISON: the voice %u, the part's fixed %u -> %u", c0, c1, c2, c3, f0, f1);
    ck(m, c1 > c0 && c2 > c1 && c3 == c0 && f1 > f0 && part_notes(0) >= 1u);
    XV(&trk[0], X_SUBLVL) = 0;
    trk[0].p[P_VOICE] = V_POLY;
    ck("  .. LEVEL 0 costs nothing (the rows are not added)", part_cost(&trk[0]) == c0 && part_fixed(&trk[0]) == f0);
}
static void t_sub(void)
{
    sub_exact();
    sub_levels();
    sub_freq();
    sub_curve();
    sub_spectrum();
    sub_phase();
    sub_clicks();
    sub_enter_leave();
    sub_unison();
    sub_pitch();
    sub_alias();
    sub_budget();
}
static void t_sub_fm6(void) { sub_fm6(); }

/* ========================================================================================================= CYCLE === */
static struct { int mode, rise, fall, curve, sync, bpm, esync, read, vmode, uni; } CY;
static void cy_default(void)
{
    CY.mode = 0; CY.rise = 40; CY.fall = 50; CY.curve = 0; CY.sync = 0; CY.bpm = 120; CY.esync = 0; CY.read = 1; CY.vmode = V_POLY; CY.uni = 0;
}
static void cy_setup(void)
{
    track_t *t;
    fresh();
    part_wave(0, 0, 30);
    t = &trk[0];
    t->p[P_VOICE] = (int16_t)CY.vmode;
    t->p[P_ESYNC] = (int16_t)CY.esync;
    t->p[P_REL] = 40;
    song.g[G_BPM] = (int16_t)CY.bpm;
    XV(t, X_CYMODE) = (int16_t)CY.mode;
    XV(t, X_CYRISE) = (int16_t)CY.rise;
    XV(t, X_CYFALL) = (int16_t)CY.fall;
    XV(t, X_CYCURVE) = (int16_t)CY.curve;
    XV(t, X_CYSYNC) = (int16_t)CY.sync;
    t->p[P_M1SRC] = (int16_t)(CY.read ? MS_CYCLE : MS_LFO);        /* (the matrix on either way: another source reads nothing of CYCLE) */
    t->p[P_M1DST] = MD_PAN;
    t->p[P_M1AMT] = CY.read == 2 ? 0 : 20;
}
/* the scene's events, then the value of voices 0..3 after each block, into sh->cyr (the end state of the cycle in sh->rep) */
static void cy_scene_body(int32_t (*out)[NB], uint32_t nb)
{
    uint32_t b, i;
    cy_setup();
    cur = sh->a;
    for (b = 0; b < nb; b++) {
        ev_apply(b);
        rec_blocks(b, 1);
        for (i = 0; i < 4u; i++)
            out[i][b] = cycle_value(&trk[0], &trk[0].v[i]);
    }
}
static void cy_run(uint32_t nb, int32_t (*out)[NB])        /* in a child; the result copied out of the child by shared memory */
{
    pid_t p;
    int st = 0;
    fflush(stdout);
    p = fork();
    if (p == 0) {
        cy_scene_body(out, nb);
        sh->rep[0] = (double)cy_lin[0][0];
        sh->rep[1] = (double)cy_st[0][0];
        sh->rep[2] = (double)trk[0].cy;
        sh->rep[3] = (double)trk[0].m_cycle;
        fflush(stdout);
        _exit(0);
    }
    waitpid(p, &st, 0);
}
/* the reference model: the generator as the design has it (doubles) */
static double cy_f(double x, double c) { return x + c * (x * x - x); }
static void cy_model(double *y, uint32_t nb, int rise, int fall, int curve, int once)
{
    double lin = 0, kr = (double)ENV_LIN[rise] / 16777216.0, kf = (double)ENV_LIN[fall] / 16777216.0, c = curve / 64.0;
    int st = 1;
    uint32_t b;
    for (b = 0; b < nb; b++) {
        if (!st) {
            y[b] = 0;
            continue;
        }
        if (st == 1) {
            lin += kr;
            if (lin >= 1) { lin = 1; st = 2; }
        } else {
            lin -= kf;
            if (lin <= 0) { lin = 0; st = once ? 0 : 1; }
        }
        y[b] = !st ? 0 : st == 2 ? 1 - cy_f(1 - lin, c) : cy_f(lin, c);
        y[b] *= 32768.0;
    }
}
static int cy_cmp(const int32_t *got, const double *want, uint32_t nb, double tol, double *worst)
{
    uint32_t b;
    int ok = 1;
    *worst = 0;
    for (b = 0; b < nb; b++) {
        double w = want[b] > 32767 ? 32767 : want[b], d = fabs((double)got[b] - w);
        *worst = d > *worst ? d : *worst;
        ok &= d <= tol;
    }
    return ok;
}
static double MOD_Y[NB];
static uint32_t cy_first_at(const int32_t *y, uint32_t nb, int32_t v)   /* the first block where y reaches v */
{
    uint32_t b;
    for (b = 0; b < nb; b++)
        if (y[b] >= v)
            return b;
    return nb;
}
static void cy_shapes(void)
{
    static const int CU[5] = {0, -40, 32, 63, -64};
    uint32_t i, hump = 0, b, per = 0, last = 0, n = 0;
    double worst;
    char m[240];
    int ok = 1, ok1 = 1;
    for (i = 0; i < 5u; i++) {
        cy_default();
        CY.curve = CU[i];
        ev_clear();
        ev_add(0, EV_ON, 48, 100);
        cy_run(500, sh->cyr);
        cy_model(MOD_Y, 500, CY.rise, CY.fall, CY.curve, 0);
        ok &= cy_cmp(sh->cyr[0], MOD_Y, 500, 4.0, &worst);
        if (getenv("SUBCYC_VERBOSE"))
            printf("  cycle curve %d: worst %.1f\n", CU[i], worst);
    }
    snprintf(m, sizeof m, "LOOP, RISE 40 FALL 50, CURVE 0 / -40 / +32 / +63 / -64: the value block by block is the generator's model (y = x + c (x^2 - x); the fall the rise's shape flipped in value) within 4 of 32768 (Q15 rounding)");
    ck(m, ok);
    {   /* the period and the ends: LOOP keeps cycling, a peak of 32767 each time */
        cy_default();
        ev_clear();
        ev_add(0, EV_ON, 48, 100);
        cy_run(500, sh->cyr);
        for (b = 1; b < 500u; b++)
            if (sh->cyr[0][b] >= 32767 && sh->cyr[0][b - 1u] < 32767) {
                if (n)
                    per += b - last;
                last = b;
                n++;
            }
        {
            uint32_t rb = (uint32_t)ceil(16777216.0 / ENV_LIN[40]), fb = (uint32_t)ceil(16777216.0 / ENV_LIN[50]);
            snprintf(m, sizeof m, "LOOP: %u peaks in 500 blocks, a cycle every %.1f blocks (rise %u + fall %u: RISE 40 / FALL 50 of the ATK table)", n, n > 1u ? (double)per / (n - 1u) : 0.0, rb, fb);
            ck(m, n >= 5u && fabs((double)per / (n - 1u) - (double)(rb + fb)) < 1.5);
        }
    }
    {   /* ONCE: one hump, then rest at 0 */
        cy_default();
        CY.mode = 1;
        ev_clear();
        ev_add(0, EV_ON, 48, 100);
        cy_run(500, sh->cyr);
        cy_model(MOD_Y, 500, CY.rise, CY.fall, CY.curve, 1);
        for (b = 1, hump = 0; b < 500u; b++)
            hump += sh->cyr[0][b] >= 32767 && sh->cyr[0][b - 1u] < 32767;
        {
            uint32_t end = 0;
            for (b = 0; b < 500u; b++)
                if (sh->cyr[0][b] > 0)
                    end = b;
            snprintf(m, sizeof m, "ONCE: one hump (%u peak), at rest at 0 from block %u on (the rise %u and fall %u blocks)", hump, end + 1u, (unsigned)ceil(16777216.0 / ENV_LIN[40]), (unsigned)ceil(16777216.0 / ENV_LIN[50]));
            ck(m, hump == 1u && cy_cmp(sh->cyr[0], MOD_Y, 500, 4.0, &worst) && sh->cyr[0][499] == 0 && end < 100u);
        }
    }
    {   /* the sign of CURVE: + slow start of the rise, the fall starts slow too (stays high); - fast first */
        double lo[2], hi[2];
        for (i = 0; i < 2u; i++) {
            cy_default();
            CY.curve = i ? -40 : 40;
            ev_clear();
            ev_add(0, EV_ON, 48, 100);
            cy_run(200, sh->cyr);
            b = cy_first_at(sh->cyr[0], 200, 32767) / 2u;           /* half way up */
            lo[i] = sh->cyr[0][b];
            b = cy_first_at(sh->cyr[0], 200, 32767) + (unsigned)ceil(16777216.0 / ENV_LIN[50]) / 2u;   /* half way down */
            hi[i] = sh->cyr[0][b];
        }
        snprintf(m, sizeof m, "CURVE +40: half way up the value is %.0f (below the straight 16384: a slow start), half way down %.0f (above it: the fall stays high first); CURVE -40: %.0f and %.0f (fast first)", lo[0], hi[0], lo[1], hi[1]);
        ck(m, lo[0] < 14000 && hi[0] > 18500 && lo[1] > 18500 && hi[1] < 14000);
    }
    (void)ok1;
}
static void cy_times(void)
{
    uint32_t b, rb, ra = 0, vr;
    char m[240];
    int ok = 1;
    for (vr = 0; vr < 4u; vr++) {
        static const int RV[4] = {10, 30, 50, 70};
        int r = RV[vr];
        cy_default();
        CY.rise = r;
        ev_clear();
        ev_add(0, EV_ON, 48, 100);
        ev_add(0, EV_P, P_ATK, r);
        {   /* the AMP ENV with ATK = the same value: the block where its attack ends */
            pid_t p;
            int st = 0;
            fflush(stdout);
            p = fork();
            if (p == 0) {
                uint32_t e = 0;
                cy_setup();
                trk[0].p[P_ATK] = (int16_t)r;
                cur = sh->a;
                for (b = 0; b < 400u; b++) {
                    ev_apply(b);
                    rec_blocks(b, 1);
                    if (!e && trk[0].v[0].stage >= 2u)
                        e = b;
                    sh->cyr[0][b] = cycle_value(&trk[0], &trk[0].v[0]);
                }
                sh->rep[0] = (double)e;
                _exit(0);
            }
            waitpid(p, &st, 0);
        }
        ra = (uint32_t)sh->rep[0];
        rb = cy_first_at(sh->cyr[0], 400, 32767);
        ok &= ra == rb;
    }
    snprintf(m, sizeof m, "RISE 10 / 30 / 50 / 70 takes as many blocks as ATK of the same value (the ATK table: ENV_LIN): the cycle's peak and the AMP ENV's end of attack at the same block (last: %u / %u)", rb, ra);
    ck(m, ok);
}
/* SYNC: CYCLE's own switch, note values of the tempo; ENV SYNC stays the envelopes'. RISE 42 = 1/16, FALL 42 */
static void cy_sync(void)
{
    uint32_t pk[6];
    char m[260];
    int ci;
    static const struct { int sync, esync, bpm; } C[6] = {{0, 0, 120}, {0, 0, 60}, {0, 1, 120}, {1, 0, 120}, {1, 0, 60}, {1, 1, 240}};
    for (ci = 0; ci < 6; ci++) {
        cy_default();
        CY.rise = 42;
        CY.fall = 42;
        CY.sync = C[ci].sync;
        CY.esync = C[ci].esync;
        CY.bpm = C[ci].bpm;
        ev_clear();
        ev_add(0, EV_ON, 48, 100);
        cy_run(NB, sh->cyr);
        pk[ci] = cy_first_at(sh->cyr[0], NB, 32767);
    }
    snprintf(m, sizeof m, "SYNC: the rise (blocks to the peak) at BPM 120 / 60 with CYCLE SYNC off: %u / %u (the table's, tempo-free); ENV SYNC alone changes nothing: %u", pk[0], pk[1], pk[2]);
    ck(m, pk[0] == pk[1] && pk[2] == pk[0]);
    snprintf(m, sizeof m, "  .. CYCLE SYNC on (1/16 at 120 BPM = 5512 samples: %u blocks, at 60 BPM %u, at 240 BPM with ENV SYNC on %u): follows the tempo", pk[3], pk[4], pk[5]);
    ck(m, fabs((double)pk[3] - 5512.5 / CTL) < 2.0 && fabs((double)pk[4] / pk[3] - 2.0) < 0.03 && fabs((double)pk[5] / pk[3] - 0.5) < 0.04 && pk[3] != pk[0]);
}
/* a retrigger: the rise starts where the generator stands */
static void cy_retrigger(void)
{
    static const int CU[3] = {0, 40, -40};
    uint32_t i, at, ok = 1;
    char m[260];
    int32_t worst = 0;
    for (i = 0; i < 3u; i++)
        for (at = 0; at < 4u; at++) {
            static const uint32_t B[4] = {12, 28, 40, 90};          /* rising, near the peak, falling, falling (RISE 40: 25 blocks, FALL 60: 15) */
            int32_t before, after, lim;
            cy_default();
            CY.curve = CU[i];
            CY.vmode = V_MONO;
            ev_clear();
            ev_add(0, EV_ON, 48, 100);
            ev_add(B[at], EV_ON, 52, 100);                          /* MONO: the same voice, retriggered */
            cy_run(130, sh->cyr);
            before = sh->cyr[0][B[at] - 1u];
            after = sh->cyr[0][B[at]];
            lim = (int32_t)(1.15 * (1 + abs(CU[i]) / 64.0) * ENV_LIN[40] / 16777216.0 * 32768.0) + 8;   /* one rise step of the steepest point */
            worst = abs(after - before) > worst ? abs(after - before) : worst;
            ok &= abs(after - before) <= lim;
        }
    snprintf(m, sizeof m, "a MONO retrigger on a voice that cycles (rising, at the top, falling; CURVE 0 / +40 / -40): the value changes by at most a step (%d of the limit %d): no jump", worst, (int)(1.15 * 2 * ENV_LIN[40] / 16777216.0 * 32768.0) + 8);
    ck(m, ok);
    {   /* LEGATO: no restart: the series is the one of the single note; a MONO second note differs */
        static int32_t single[NB];
        cy_default();
        CY.vmode = V_LEGATO;
        ev_clear();
        ev_add(0, EV_ON, 48, 100);
        cy_run(300, sh->cyr);
        memcpy(single, sh->cyr[0], sizeof single);
        ev_add(33, EV_ON, 55, 100);
        ev_add(70, EV_ON, 50, 100);
        cy_run(300, sh->cyr);
        ck("a LEGATO note keeps the cycle running: the value of every block is the single note's (a legato step is no voice_start)", !memcmp(single, sh->cyr[0], 300u * sizeof(int32_t)));
        CY.vmode = V_MONO;
        cy_run(300, sh->cyr);
        ck("  .. a MONO second note restarts the rise from where it stands: the series differs after it", memcmp(single, sh->cyr[0], 300u * sizeof(int32_t)) != 0 && !memcmp(single, sh->cyr[0], 33u * sizeof(int32_t)));
    }
}
/* every voice from its own key-down */
static void cy_voices(void)
{
    uint32_t b, ok = 1, diff = 0;
    char m[240];
    cy_default();
    ev_clear();
    ev_add(0, EV_ON, 48, 100);
    ev_add(20, EV_ON, 52, 100);
    ev_add(45, EV_ON, 55, 100);
    cy_run(300, sh->cyr);
    for (b = 0; b < 250u; b++) {
        ok &= b + 20u >= 300u || sh->cyr[1][b + 20u] == sh->cyr[0][b];
        ok &= b + 45u >= 300u || sh->cyr[2][b + 45u] == sh->cyr[0][b];
    }
    for (b = 100; b < 300u; b++)
        diff += sh->cyr[0][b] != sh->cyr[1][b] && sh->cyr[1][b] != sh->cyr[2][b] && sh->cyr[0][b] != sh->cyr[2][b];
    snprintf(m, sizeof m, "a chord struck in three blocks (0, 20, 45): each voice's cycle is the first one's, shifted by its own key-down (exact); the three differ at %u of 200 blocks (they pulse unevenly)", diff);
    ck(m, ok && diff > 150u);
    ck("  .. nothing is ticked for the voices that do not sound (voice 3: at rest, 0)", sh->cyr[3][299] == 0 || sh->cyr[3][299] == (int32_t)cy_y[0][3]);
}
/* the read flag, unread = no tick, the release, the matrix value */
static void cy_read(void)
{
    char m[260];
    cy_default();
    ev_clear();
    ev_add(0, EV_ON, 48, 100);
    cy_run(200, sh->cyr);
    ck("a slot with SRC CYCLE (AMT set) sets the read flag (t->cy), the cycle runs (the state moved); t->m_cycle is the latest note's value", sh->rep[2] == 1.0 && sh->rep[0] != 0.0 && (int32_t)sh->rep[3] == sh->cyr[0][199]);
    CY.read = 0;
    cy_run(200, sh->cyr);
    snprintf(m, sizeof m, "the matrix on with another source (LFO): t->cy is 0 and nothing is ticked: the progress stays %.0f, the stage %.0f (a note start), the value reads %d", sh->rep[0], sh->rep[1], sh->cyr[0][199]);
    ck(m, sh->rep[2] == 0.0 && sh->rep[0] == 0.0 && sh->rep[1] == 1.0 && sh->cyr[0][199] == 0);
    CY.read = 2;
    cy_run(200, sh->cyr);
    ck("  .. a CYCLE slot with AMT 0 is no active slot: not read either", sh->rep[2] == 0.0 && sh->rep[0] == 0.0);
    {   /* the release: the cycle goes on until the AMP ENV ends the note, ONCE never repeats */
        uint32_t b, moved = 0, last = 0;
        cy_default();
        ev_clear();
        ev_add(0, EV_ON, 48, 100);
        ev_add(60, EV_OFF, 48, 0);
        ev_add(0, EV_P, P_REL, 60);
        cy_run(400, sh->cyr);
        for (b = 61; b < 400u; b++) {
            moved += sh->cyr[0][b] != sh->cyr[0][b - 1u];
            if (sh->cyr[0][b] != sh->cyr[0][b - 1u])
                last = b;
        }
        snprintf(m, sizeof m, "the release: the cycle keeps going after the note-off (block 60) until the voice ends (REL 60): %u blocks of movement, the last at block %u", moved, last);
        ck(m, moved > 40u && last > 100u && last < 399u);
    }
}
static void t_cycle(void)
{
    cy_shapes();
    cy_times();
    cy_sync();
    cy_retrigger();
    cy_voices();
    cy_read();
}
#endif /* SUBCYC_BASE */

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    sh = mmap(0, sizeof *sh, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    in_child(t_ref);
#ifndef SUBCYC_BASE
    in_child(t_sub);
    in_child(t_sub_fm6);
    in_child(t_cycle);
#endif
    printf("subcyc: %d checks, %d %s\n", sh->ncheck, sh->bad, sh->bad ? "FAILED" : "failed (all ok)");
    return sh->bad != 0;
}
