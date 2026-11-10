/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.17: FRETLESS and SLAP DOCTOR (the two FM6 basses) must not sound alike (owner, 2026-10-15: "almost identical"; both were the
 * ROUND BASS patch F3 with near-equal settings). Each is rendered as the firmware loads it (host_ds_load, C2 velocity 100, 1.5 s held,
 * the mix before MASTER) and measured:
 *   H200   the share of the energy above 250 Hz (dB) in the window 150..250 ms after the note-on (how bright the note is as the pluck settles)
 *   H600   the same at 550..650 ms
 *   TR     the peak of the first 80 ms over the RMS of 500..900 ms (dB): how sharp the transient is
 * They must differ clearly: H200 by at least 6 dB and TR by at least 4 dB. FM_BASS_DIFF_VERBOSE=1 prints the numbers.
 *   build/host/fm_bass_diff_test */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <sys/wait.h>

static double *buf; static uint32_t nfr;
static void render(const char *name, uint32_t note)
{
    uint32_t n, N = ds_list_n(), e = 99, k = 0, b, i, nb = (uint32_t)(1.5 * FS) / CTL; int32_t o[2 * CTL];
    for (n = 0; n < N; n++) { e = ds_list_at(n, &k); if (!strcmp(ds_get(e, k)->name, name)) break; }
    if (n == N) { fprintf(stderr, "no sound %s\n", name); exit(2); }
    host_tracks_init(); song.g[G_RTYPE] = GP[G_RTYPE].def; duell_mode_force(DM_STEREO);
    host_ds_load(&trk[0], e, k);
    input_on(&trk[0], note, 100);
    nfr = nb * CTL; buf = calloc(nfr, sizeof *buf);
    for (b = 0; b < nb; b++) {
        memset(o, 0, sizeof o); mix_block(o, CTL);
        for (i = 0; i < CTL; i++) buf[b * CTL + i] = 0.5 * (mix_l[i] + mix_r[i]) + wet[i];
    }
}
static double hfshare(double t0, double t1)   /* the energy above 250 Hz over the whole (dB) */
{
    enum { N = 8192 };
    static double re[N], im[N]; uint32_t i, j, m, a = (uint32_t)(t0 * FS), n = (uint32_t)((t1 - t0) * FS); double num = 0, den = 0;
    for (i = 0; i < N; i++) { double w = i < n ? 0.5 - 0.5 * cos(2 * M_PI * i / (n - 1)) : 0; re[i] = i < n ? buf[a + i] * w : 0; im[i] = 0; }
    for (i = 1, j = 0; i < N; i++) { uint32_t bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; } }
    for (m = 2; m <= N; m <<= 1) { double ang = -2 * M_PI / m; for (i = 0; i < N; i += m) for (j = 0; j < m / 2; j++) { double wr = cos(ang * j), wi = sin(ang * j), ur = re[i + j], ui = im[i + j], vr = re[i + j + m / 2] * wr - im[i + j + m / 2] * wi, vi = re[i + j + m / 2] * wi + im[i + j + m / 2] * wr; re[i + j] = ur + vr; im[i + j] = ui + vi; re[i + j + m / 2] = ur - vr; im[i + j + m / 2] = ui - vi; } }
    for (i = 1; i < N / 2; i++) { double p = re[i] * re[i] + im[i] * im[i]; if (i * (double)FS / N > 250.0) num += p; den += p; }
    return 10 * log10((num + 1e-9) / (den + 1e-9));
}
static double tr_db(void)
{
    uint32_t i; double pk = 0, s = 0; uint32_t a = FS / 2, b = FS * 9 / 10;
    for (i = 0; i < FS * 8 / 100; i++) if (fabs(buf[i]) > pk) pk = fabs(buf[i]);
    for (i = a; i < b; i++) s += buf[i] * buf[i];
    return 20 * log10(pk / sqrt(s / (b - a)));
}
int main(void)
{
    static const char *const NM[2] = {"FRETLESS", "SLAP DOCTOR"};
    double m[2][3]; int p, k, ok = 1, verbose = getenv("FM_BASS_DIFF_VERBOSE") != 0;
    for (k = 0; k < 2; k++) {
        int fd[2]; if (pipe(fd)) return 1;
        if (!fork()) { render(NM[k], 36); m[k][0] = hfshare(0.15, 0.25); m[k][1] = hfshare(0.55, 0.65); m[k][2] = tr_db(); if (write(fd[1], m[k], sizeof m[k]) < 0) _exit(1); _exit(0); }
        if (read(fd[0], m[k], sizeof m[k]) != (ssize_t)sizeof m[k]) return 1;
        wait(0); close(fd[0]); close(fd[1]);
        if (verbose) printf("fm_bass_diff: %-12s H200 %6.1f dB  H600 %6.1f dB  TR %5.1f dB\n", NM[k], m[k][0], m[k][1], m[k][2]);
    }
    p = 0;
    { double r = fabs(m[0][0] - m[1][0]); int g = r >= 6.0; printf("fm_bass_diff: brightness (energy above 250 Hz) at 200 ms differs %.1f dB (need >= 6) %s\n", r, g ? "ok" : "FAIL"); ok &= g; }
    { double d = fabs(m[0][2] - m[1][2]); int g = d >= 4.0; printf("fm_bass_diff: transient peak / body differs %.1f dB (need >= 4) %s\n", d, g ? "ok" : "FAIL"); ok &= g; }
    (void)p;
    return !ok;
}
