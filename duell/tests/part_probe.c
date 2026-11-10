/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.24: PART PROBE, a measuring tool (not a test): ONE part of a factory multi played alone on one key and its spectrum measured, so a
 * claim like "the pad is dark" or "MORPH opens it" is a number, not an ear (owner: measure, do not claim). Used for M03 PAD SPLIT in 0.2.24
 * (SOUND_RECIPES.md 7.8): the pad's spectral centroid and band energies at E4 / A4 / C5, with KEY TRK, the reverb LOW CUT, the note (the TRN
 * of 0.2.22), the sends, the MORPH fader and the engine's CUT changed one at a time.
 *   cc -O1 -w -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests '-DFELUCCA_VERSION="x"' -o build/host/part_probe tests/part_probe.c -lm
 *   build/host/part_probe SLOT PART KEY KTRK LOCUT NOTEOFF DRY [FADER [E5 [E2FLT]]]
 *     SLOT 1..16, PART 1 / 2 (the other part muted), KEY the MIDI note (69 = A4), KTRK -99 = as stored else -63..63, LOCUT -1 = as stored else 0..127,
 *     NOTEOFF semitones added to the key (-12: the pad as 0.2.22 played it, with part 1's TRN), DRY 1 = the part's sends 0, FADER the MORPH fader 0..127
 *     (0), E5 the engine's fifth EDIT value (ANALOG: CUT; -1 as stored), E2FLT the ENV 2 -> filter depth (-999 as stored)
 *   The multi is loaded as MULTI LOAD SETS MODE does (its MODE applied), the key held 5 s; seconds 1.5 .. 4.5 of the master output (both channels summed)
 *   go through a Hann window and a 131072-point FFT: the RMS and peak (dBFS), the spectral centroid (Hz) and the energy of seven bands relative to
 *   the total (dB): <200, 200-500, 500-1k, 1k-2k, 2k-4k, 4k-8k, >8k Hz. */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define HOST_IRQ_HOOK 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
static void fm1_irq_off(void);
static void fm1_irq_on(void);
#include "ui_test.c"
static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { const uint8_t *s = src; uint32_t i; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

#define NFFT 131072
static double re[NFFT], im[NFFT];
static void fft(double *x, double *y, int n)
{
    int i, j, k, le, le2;
    for (i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double t = x[i]; x[i] = x[j]; x[j] = t; t = y[i]; y[i] = y[j]; y[j] = t; }
    }
    for (le = 2; le <= n; le <<= 1) {
        double ang = -2 * M_PI / le, wr = cos(ang), wi = sin(ang);
        le2 = le >> 1;
        for (i = 0; i < n; i += le) {
            double cr = 1, ci = 0;
            for (k = 0; k < le2; k++) {
                int a = i + k, b = a + le2;
                double tr = x[b] * cr - y[b] * ci, ti = x[b] * ci + y[b] * cr, t;
                x[b] = x[a] - tr; y[b] = y[a] - ti; x[a] += tr; y[a] += ti;
                t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t;
            }
        }
    }
}

int main(int argc, char **argv)
{
    uint32_t slot, part, key, i, b, nb = (uint32_t)(5.0 * FS) / CTL, skip = (uint32_t)(1.5 * FS), n = 0;
    int ktrk, locut, noteoff, dry, fader, cut, e2;
    int32_t o[2 * CTL];
    double rms = 0, pk = 0, band[7] = {0}, tot = 0, cen_n = 0;
    static const double EDGE[8] = {0, 200, 500, 1000, 2000, 4000, 8000, 1e9};
    static const char *const BN[7] = {"<200", "200-500", "500-1k", "1k-2k", "2k-4k", "4k-8k", ">8k"};
    if (argc < 8)
        return fprintf(stderr, "usage: part_probe SLOT PART KEY KTRK LOCUT NOTEOFF DRY [FADER [E5 [E2FLT]]]\n"), 2;
    slot = (uint32_t)atoi(argv[1]) - 1u; part = (uint32_t)atoi(argv[2]) - 1u; key = (uint32_t)atoi(argv[3]);
    ktrk = atoi(argv[4]); locut = atoi(argv[5]); noteoff = atoi(argv[6]); dry = atoi(argv[7]);
    fader = argc > 8 ? atoi(argv[8]) : 0; cut = argc > 9 ? atoi(argv[9]) : -1; e2 = argc > 10 ? atoi(argv[10]) : -999;
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    multi_boot();
    frame();
    song.master_q12 = 4096;
    ui_prefs2 |= PREF_MODE_RECALL >> 16;
    project_load(slot);
    for (i = 0; i < 40; i++) { memset(o, 0, sizeof o); mix_block(o, CTL); while (wave_service()) ; }
    trk[1u - part].p[P_MUTE] = 1;
    if (ktrk != -99) trk[part].p[P_KTRK] = (int16_t)ktrk;
    if (locut >= 0) song.g[G_RLOCUT] = (int16_t)locut;
    if (cut >= 0) trk[part].p[P_E0 + 4] = (int16_t)cut;
    if (e2 != -999) trk[part].p[P_E2FLT] = (int16_t)e2;
    if (dry) { trk[part].p[P_CHOR] = 0; trk[part].p[P_DLY] = 0; trk[part].p[P_REV] = 0; }
    morph_set_target(1);
    morph_set_fader((uint32_t)fader);
    for (i = 0; i < 4; i++) { memset(o, 0, sizeof o); mix_block(o, CTL); while (wave_service()) ; }
    printf("M%02u part %u %-12s key %u%+d KTRK %d UFCUT %d LOCUT %d dry %d fader %d E5 %d E2FLT %d: ", slot + 1u, part + 1u, ds_name(trk[part].eng_req, trk[part].preset),
           key, noteoff, trk[part].p[P_KTRK], trk[part].p[P_FREEA], song.g[G_RLOCUT], dry, fader, trk[part].p[P_E0 + 4], trk[part].p[P_E2FLT]);
    trk_note_on(&trk[part], (uint32_t)((int)key + noteoff), 100);
    for (b = 0; b < nb; b++) {
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        while (wave_service()) ;
        for (i = 0; i < CTL; i++) {
            uint32_t s = b * CTL + i;
            double x = (o[2 * i] + o[2 * i + 1]) / 65534.0;
            if (s < skip || n >= NFFT) continue;
            re[n] = x * (0.5 - 0.5 * cos(2 * M_PI * n / NFFT));
            im[n++] = 0;
            rms += x * x;
            if (fabs(x) > pk) pk = fabs(x);
        }
    }
    fft(re, im, NFFT);
    for (i = 1; i < NFFT / 2; i++) {
        double f = (double)i * FS / NFFT, p = re[i] * re[i] + im[i] * im[i];
        int k;
        for (k = 0; k < 7; k++) if (f >= EDGE[k] && f < EDGE[k + 1]) band[k] += p;
        tot += p; cen_n += f * p;
    }
    printf("rms %6.1f dBFS peak %6.1f | centroid %6.0f Hz |", 10 * log10(rms / n + 1e-30), 20 * log10(pk + 1e-30), cen_n / (tot + 1e-30));
    for (i = 0; i < 7; i++)
        printf(" %s %5.1f", BN[i], 10 * log10(band[i] / (tot + 1e-30) + 1e-30));
    printf("  (dB re total)\n");
    return 0;
}
