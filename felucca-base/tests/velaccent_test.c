/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.1: the velocity ACCENT LIFT is smooth and loudness never falls as velocity rises (release_drafts/VELOCITY_AFTERTOUCH_V1.md). Before, a velocity
 * above 110 added the envelope x 24 >> 7 to the filter / vowel at once (a step at 111): 24 of 127 sounds got QUIETER there, by up to 5.2 dB. Causes and fixes:
 * core.h (accent_lift, accent_dir_*): the lift now ramps from 0 at 110 to 24 at 127, and a filter opens the way its MODE / TYPE opens (LP up, HP down, BP none);
 * eng_noise.c: the make-up gain no longer follows the lifted cutoff; eng_formant.c: the lift opens the presence instead of moving the vowel; eng_phys.c: the lift
 * is the model's accent instead of its exciter brightness.
 * Scan: every non-retired engine but LOFI (someone else's), its first 8 presets (host_preset) and first 8 factory sounds (host_ds_load), one note (C3, 48),
 * velocity 1..127, sends off, STEREO, the RMS of the part's mix 0.5 s after a 4-block skip. Per sound the running maximum of the level over the lower velocities is
 * kept; the sound FAILS if at some velocity its level is more than its allowance below that maximum (levels under FLOOR_DB are not compared).
 * ALLOWANCE (measured with the lift OFF, -DACCENT_MAX=0, 127 sounds): ANALOG PHASE WAVE WHEEL VOICE TRIO NOISE PHYS are exactly monotone (0.00 dB), so their
 * allowance is NOISE_DB 0.02 dB (rounding). KNOWN exceptions, each printed (KNOWN), and what is left of them:
 *  - FM6, 1.5 dB (largest 1.49, sound 7 near velocity 124): FM6 is an exact DX7 copy; its velocity table raises the modulation index with velocity and the Bessel ripple of FM
 *    moves the level by itself. Not a bug, not changed.
 *  - ANALOG factory 4, 0.95 dB (0.89 at velocity 115): RES 97 low-pass at CUT 26; the accent opens the cutoff and the resonance peak passes a harmonic of the note. Measured
 *    with the lift OFF and the velocity fixed, the level against CUT alone falls the same way (about 4 dB between CUT 21 and 27): the filter's own behaviour, not the method.
 *  - PHYS sweep (phys_sweep, 4 models x 12 settings x velocity 1..127, any setting, not only factory sounds): 0.02 dB, except the CURVED BRIDGE (STRNG with STRC 30 or less, SYMP
 *    with BUZZ 6 or more) and the BOWED string (STRNG with BOW > 0), 2.5 dB (measured up to 2.4): a nonlinear string driven by its own amplitude, or by a few random hits in
 *    the window, jumps between regimes; no velocity mapping can make that monotone. The PHYS factory sounds themselves are all at 0.02 dB.
 *   cc -O1 -w -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests '-DFELUCCA_VERSION="x"' -o build/host/velaccent_test tests/velaccent_test.c -lm
 *   build/host/velaccent_test      (DUELL/host_tests.sh "velaccent"; VA_TABLE=1 prints every sound's worst fall) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <sys/mman.h>
#include <sys/wait.h>

#ifndef NOISE_DB
#define NOISE_DB 0.02
#endif
#define FLOOR_DB (-90.0)
#define NOTE 48
#define NSKIP 4u
#define NMEAS (FS / 2u / CTL)

static int32_t ob[2 * CTL];
static int32_t PP[8], use_pp, note_x = NOTE;               /* PHYS sweep: the engine parameters P_E0..P_E7 and the note */
/* the allowance of a sound, dB (see the header) */
static double allow(const char *eng, uint32_t pi)
{
    if (!strcmp(eng, "FM6"))
        return 1.5;
    if (!strcmp(eng, "ANALOG") && pi == 8u + 4u)
        return 0.95;
    return NOISE_DB;
}
static double *lvl;                                      /* shared with the render's child (mmap): the level of the last render, dB */

static void dsp_clean(void)
{
    memset(trk, 0, sizeof trk);
    memset(mph, 0, sizeof mph);
    memset(&morph, 0, sizeof morph);
    memset(ins, 0, sizeof ins);
    memset(midi_sel_on, 0, sizeof midi_sel_on);
    memset(midi_ch, 0, sizeof midi_ch);
    memset(midi_owners, 0, sizeof midi_owners);
    memset(mchord, 0, sizeof mchord);
    memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target);
    memset(kb_chn, 0, sizeof kb_chn);
    mix_xf = mix_split = 0;
    vage = 0;
    mod_seed = 0x2545F491u;
    rng_state = 0x1234567u;
    fm1_in.notes = 0;
    kb_prev = 0;
    kb_layer = 0;
    mi_r = mi_w = 0;
    panic_req = 0;
    duell_mode_force(DM_SPLIT);
    duell_mode_force(DM_STEREO);
    memset(dly_buf, 0, sizeof dly_buf);
    memset(&fx, 0, sizeof fx);
    memset(&fx_b, 0, sizeof fx_b);
    memset(&hl, 0, sizeof hl);
    memset(&hl_b, 0, sizeof hl_b);
    lim_env = lim_env_r = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    host_tracks_init();
    song.sel = 0;
    song.playing = 0;
    song.grid = 0;
}

static void render_child(uint32_t e, uint32_t pi, uint32_t vel)   /* pi < 8: preset pi; pi >= 8: factory sound pi - 8 */
{
    track_t *t = &trk[0];
    double s = 0;
    uint32_t i, n;
    dsp_clean();
    if (pi < 8u)
        host_preset(t, e, pi);
    else
        host_ds_load(t, e, pi - 8u);
    if (use_pp) {
        uint32_t k;
        for (k = 0; k < 8u; k++)
            t->p[P_E0 + k] = PP[k];
    }
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_TRIM] = 60;                                   /* (well under the master limiter, whose gain would hide a fall) */
    trk_note_on(t, note_x, vel);
    for (n = 0; n < NSKIP; n++)
        mix_block(ob, CTL);
    for (n = 0; n < NMEAS; n++) {
        mix_block(ob, CTL);
        for (i = 0; i < CTL; i++)
            s += (double)ob[2u * i] * ob[2u * i] + (double)ob[2u * i + 1u] * ob[2u * i + 1u];
    }
    s /= 2.0 * NMEAS * CTL;
    *lvl = s > 0 ? 10 * log10(s / (32767.0 * 32767.0)) : -200;
}
static double render(uint32_t e, uint32_t pi, uint32_t vel)
{
    pid_t pid;
    fflush(stdout);
    if (!(pid = fork())) {
        render_child(e, pi, vel);
        _exit(0);
    }
    waitpid(pid, 0, 0);
    return *lvl;
}

/* PHYS over the parameter space, not only the factory sounds (owner's rule 2026-10-24: fix the generic logic): every model (MODAL STRNG MEMB SYMP), PHYS_NSET settings of
 * STRC BRIT DAMP POS ACC BOW EXC from a fixed generator (ACC at least 30: with ACC 0 the velocity does nothing but the amplitude), notes C2..C6, velocity 1..127.
 * Allowance 0.02 dB, except the two nonlinear regimes that no velocity mapping can make monotone (KNOWN, PHYS_NL_DB): the CURVED BRIDGE (STRNG with STRC 30 or less,
 * SYMP with BUZZ 6 or more: the string is driven by its own amplitude, and at a long DAMP the level jumps between regimes), and the BOWED string (STRNG with BOW > 0:
 * random hits, a few in the measuring window). */
#define PHYS_NSET 12
#define PHYS_NL_DB 2.5
static uint32_t phys_sweep(uint32_t e, uint32_t *nrender, uint32_t *nnan, double *worst_all)
{
    static const int notes[5] = {36, 48, 60, 72, 84};
    uint32_t nfail = 0, m, s, v;
    uint32_t rs = 7u * 2654435761u + 1u;
    use_pp = 1;
    for (m = 0; m < 4u; m++)
        for (s = 0; s < PHYS_NSET; s++) {
            uint32_t k;
            double mx = -300, worst = 0, allowed = NOISE_DB;
            uint32_t wv = 0;
            int nl;
            for (k = 0; k < 8u; k++) {
                rs = rs * 1664525u + 1013904223u;
                PP[k] = (int32_t)((rs >> 16) % 128u);
            }
            PP[0] = (int32_t)m;
            if (PP[5] < 30)
                PP[5] += 60;
            if (m == 2u)                                 /* (MEMB: P_E6 is BEND, not BOW) */
                PP[6] = PP[6] * (int32_t)(s & 1u);
            rs = rs * 1664525u + 1013904223u;
            note_x = notes[(rs >> 16) % 5u];
            nl = (m == 1u && PP[1] <= 30) || (m == 3u && PP[6] >= 6) || (m == 1u && PP[6] > 0);
            if (nl)
                allowed = PHYS_NL_DB;
            for (v = 1; v <= 127u; v += v < 100u ? 4u : 1u) {
                double l = render(e, 0, v);
                (*nrender)++;
                if (!(l == l))
                    (*nnan)++;
                if (mx > FLOOR_DB && l - mx < worst) {
                    worst = l - mx;
                    wv = v;
                }
                if (l > mx)
                    mx = l;
            }
            if (!nl && worst < -*worst_all)
                *worst_all = -worst;
            if (worst < -allowed) {
                nfail++;
                printf("velaccent: FALL PHYS sweep model %u setting %u (note %d, STRC/HARM/CHRD %d BRIT %d DAMP %d POS %d ACC %d BOW/BEND/BUZZ %d EXC %d): %.2f dB at velocity %u\n", m, s, note_x,
                       PP[1], PP[2], PP[3], PP[4], PP[5], PP[6], PP[7], worst, wv);
            } else if (worst < -NOISE_DB)
                printf("velaccent: KNOWN PHYS sweep model %u setting %u (%s): %.2f dB at velocity %u (within %.2f)\n", m, s, "curved bridge or bowed string, nonlinear", worst, wv, allowed);
        }
    use_pp = 0;
    note_x = NOTE;
    return nfail;
}

int main(void)
{
    uint32_t n, e, pi, v, nsound = 0, nfall = 0, nrender = 0, nnan = 0;
    double worst_all = 0;
    int show = getenv("VA_TABLE") != NULL;
    setvbuf(stdout, NULL, _IONBF, 0);
    lvl = mmap(0, sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    for (n = 0; n < NENG_SHOWN; n++) {
        e = eng_vis(n);
        if (str_eq(ENGINES[e]->name, "LOFI"))            /* (eng_lofi.c is being changed by someone else) */
            continue;
        for (pi = 0; pi < 16u; pi++) {
            if (pi < 8u ? pi >= ENGINES[e]->npresets : pi - 8u >= ds_count(e))
                continue;
            double mx = -300, worst = 0, l127 = 0, l110 = 0;
            uint32_t wv = 0;
            for (v = 1; v <= 127u; v++) {
                double l = render(e, pi, v);
                nrender++;
                if (!(l == l))
                    nnan++;
                if (v == 110u)
                    l110 = l;
                if (v == 127u)
                    l127 = l;
                if (mx > FLOOR_DB && l - mx < worst) {
                    worst = l - mx;
                    wv = v;
                }
                if (l > mx)
                    mx = l;
            }
            nsound++;
            if (worst < -worst_all)
                worst_all = -worst;
            if (worst < -allow(ENGINES[e]->name, pi)) {
                nfall++;
                printf("velaccent: FALL %-8s %s %u: %.2f dB at velocity %u below the level of a lower velocity (110: %.1f, 127: %.1f dB)\n",
                       ENGINES[e]->name, pi < 8u ? "preset" : "factory", pi & 7u, worst, wv, l110, l127);
            } else if (worst < -NOISE_DB)
                printf("velaccent: KNOWN %-8s %s %u: %.2f dB at velocity %u (within its allowance %.2f)\n", ENGINES[e]->name, pi < 8u ? "preset" : "factory", pi & 7u, worst, wv, allow(ENGINES[e]->name, pi));
            else if (show)
                printf("velaccent: ok   %-8s %s %u: worst %.2f dB, 110: %.1f, 127: %.1f dB\n", ENGINES[e]->name, pi < 8u ? "preset" : "factory", pi & 7u, worst, l110, l127);
        }
    }
    for (n = 0; n < NENG_SHOWN; n++)
        if (str_eq(ENGINES[eng_vis(n)]->name, "PHYS"))
            nfall += phys_sweep(eng_vis(n), &nrender, &nnan, &worst_all);
    printf("velaccent: %u sounds, %u renders, largest fall %.2f dB (allowance 0.02, FM6 1.5, ANALOG factory 4 0.95, PHYS sweep curved bridge / bowed 2.5: %.2f), %u sounds fall more, %u NaN levels\n", nsound, nrender, worst_all,
           NOISE_DB, nfall, nnan);
    printf("velaccent: no sound is quieter at a harder velocity (velocity 1..127, every engine but LOFI, 8 presets + 8 factory sounds each)  %s\n", nfall || nnan ? "FAIL" : "ok");
    return nfall || nnan ? 1 : 0;
}
