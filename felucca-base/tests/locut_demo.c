/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.22: listening set for the reverb LOW CUT's new range and default (not a test; not in host_tests.sh).
 *   cc -O2 -w -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests -o build/host/locut_demo tests/locut_demo.c -lm
 *   build/host/locut_demo OUTDIR     a bass line and a pad through PLATE and HALL (the real mixer, the real engines, the real bus and low cut of this build) at
 *                                    LOCUT 88 Hz (the old default; the step nearest to it is 42 = 90 Hz), 299 Hz (the new default, 75) and 1 kHz (108 = 999 Hz): 12 WAV + index.html
 * ANALOG #0 LADDER BASS on part 1, WAVE #1 VOWEL PAD on part 2, STEREO, SIZE 100, DAMP 55, sends 110 / 100, one gain for the whole set. No drum engine. */
#define MODE_TEST_NO_MAIN 1
#include "mode_test.c"

/* the real mixer, real engines, the real bus and low cut (the tables of this build) */
static void wav(const char *path, const int32_t *l, const int32_t *r, uint32_t n, double gain)
{
    FILE *f = fopen(path, "wb");
    uint32_t i, data = n * 4u, v;
    uint16_t s;
    if (!f) return;
    fwrite("RIFF", 1, 4, f); v = 36u + data; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    s = 1; fwrite(&s, 2, 1, f); s = 2; fwrite(&s, 2, 1, f);
    v = FS; fwrite(&v, 4, 1, f); v = FS * 4u; fwrite(&v, 4, 1, f);
    s = 4; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    for (i = 0; i < n; i++) {
        int32_t a = (int32_t)(l[i] * gain), c = (int32_t)(r[i] * gain);
        a = a > 32767 ? 32767 : a < -32768 ? -32768 : a;
        c = c > 32767 ? 32767 : c < -32768 ? -32768 : c;
        s = (uint16_t)a; fwrite(&s, 2, 1, f);
        s = (uint16_t)c; fwrite(&s, 2, 1, f);
    }
    fclose(f);
}
static const uint8_t BASSN[6] = {33, 33, 40, 36, 31, 33}, PADC[4] = {57, 60, 64, 67};
/* what: 0 bass + pad, 1 bass alone, 2 pad alone. model 5 PLATE, 2 HALL. lo = G_RLOCUT value */
static void scene(int model, int lo, int what, int send_bass, int send_pad)
{
    uint32_t b, nb = 12u * FS / CTL, j, ms = FS / 1000u;
    dsp_clean(DM_STEREO);
    song.g[G_BPM] = 100;
    song.g[G_RTYPE] = (int16_t)model;
    song.g[G_RSIZE] = 100;
    song.g[G_RDAMP] = 55;
    song.g[G_RLOCUT] = (int16_t)lo;
    song.g[G_DMDEP] = 0;
    song.g[G_DMODE] = DMD_DIGITAL;
    song.g[G_SHPITCH] = 0;
    song.g[G_SHMOD] = 0;
    fx.rtype = fx_b.rtype = (uint8_t)model;
    host_ds_load(&trk[0], scn_eng("ANALOG"), 0);          /* 0 LADDER BASS */
    host_ds_load(&trk[1], scn_eng("WAVE"), 1);            /* 1 VOWEL PAD */
    trk[0].p[P_REV] = (int16_t)send_bass;
    trk[1].p[P_REV] = (int16_t)send_pad;
    trk[0].p[P_DLY] = trk[0].p[P_CHOR] = trk[1].p[P_DLY] = trk[1].p[P_CHOR] = 0;
    trk[0].p[P_PAN] = trk[1].p[P_PAN] = 0;
    if (what == 2) trk[0].p[P_MUTE] = 1;
    if (what == 1) trk[1].p[P_MUTE] = 1;
    rec_n = 0;
    for (b = 0; b < nb; b++) {
        uint32_t t = b * CTL, step = t / (1500u * ms), w = t % (1500u * ms);
        if (what != 2 && step < 6u) {
            if (w < CTL) trk_note_on(&trk[0], BASSN[step], 110);
            if (w >= 1100u * ms && w < 1100u * ms + CTL) trk_note_off(&trk[0], BASSN[step]);
        }
        if (what != 1) {
            if (t < CTL) for (j = 0; j < 4u; j++) trk_note_on(&trk[1], PADC[j], 90);
            if (t >= 7000u * ms && t < 7000u * ms + CTL) for (j = 0; j < 4u; j++) trk_note_off(&trk[1], PADC[j]);
        }
        mblk(1);
    }
}
static void demo(const char *dir)
{
    static const struct { const char *tag; int model; } RV[2] = {{"plate", 5}, {"hall", 2}};
    static const char *const RVN[2] = {"PLATE", "HALL"};
    static const struct { const char *tag, *label; int what; } SRC[2] = {{"bass_pad", "bass + pad", 0}, {"bass", "bass alone", 1}};
    static const int VAL[3] = {42, 75, 108};
    static const char *const TAG[3] = {"88", "299", "1000"};
    static const char *const HEAD[3] = {"LOCUT 88 Hz (the old default; step 42 = 90 Hz)", "LOCUT 299 Hz (the new default, step 75)", "LOCUT 1 kHz (step 108 = 999 Hz)"};
    uint32_t r, s, v;
    char path[400], idx[400];
    FILE *h;
    int32_t pk = 0;
    double gain;
    snprintf(idx, sizeof idx, "%s/index.html", dir);
    h = fopen(idx, "w");
    if (!h) return;
    for (r = 0; r < 2u; r++) for (s = 0; s < 2u; s++) for (v = 0; v < 3u; v++) {
        int32_t p;
        scene(RV[r].model, VAL[v], SRC[s].what, 110, 100);
        p = peak_of(rec_l, rec_n) > peak_of(rec_r, rec_n) ? peak_of(rec_l, rec_n) : peak_of(rec_r, rec_n);
        pk = p > pk ? p : pk;
    }
    gain = pk > 0 ? 29000.0 / pk : 1.0;
    fprintf(h, "<!doctype html><meta charset=utf-8><title>DUELL 0.2.22 reverb LOW CUT: 88 / 299 / 1000 Hz</title>"
               "<style>body{font:15px/1.45 system-ui;margin:24px;max-width:1000px}table{border-collapse:collapse}td,th{padding:5px 12px;border-bottom:1px solid #ccc;text-align:left}audio{width:250px;height:30px}h2{margin-top:30px}</style>"
               "<h1>DUELL reverb LOW CUT: 88 Hz (old default) vs 299 Hz (new default) vs 1 kHz</h1>"
               "<p>A bass line (ANALOG #0 LADDER BASS, part 1) and a pad (WAVE #1 VOWEL PAD, part 2) through the reverb, STEREO mode, SIZE 100, DAMP 55, reverb sends 110 (bass) and 100 (pad). "
               "Only the reverb's LOW CUT differs between the three files of a row (a one-pole, 6 dB an octave, -3 dB at the printed frequency; the range is 20 Hz to 2 kHz since 0.2.22). Each file is 12 s: the notes, then the tail. "
               "All files have the same gain, so louder or quieter is real. Listen to the bass in the tail: at 88 Hz it carries the low notes into the room, at 299 Hz it is much less, at 1 kHz the reverb is only the top and the dry bass stands alone.</p>");
    for (r = 0; r < 2u; r++)
        for (s = 0; s < 2u; s++) {
            fprintf(h, "<h2>%s, %s</h2><table><tr>", RVN[r], SRC[s].label);
            for (v = 0; v < 3u; v++) fprintf(h, "<th>%s</th>", HEAD[v]);
            fprintf(h, "</tr><tr>");
            for (v = 0; v < 3u; v++) {
                scene(RV[r].model, VAL[v], SRC[s].what, 110, 100);
                snprintf(path, sizeof path, "%s/%s_%s_%s.wav", dir, RV[r].tag, SRC[s].tag, TAG[v]);
                wav(path, rec_l, rec_r, rec_n, gain);
                fprintf(h, "<td><audio controls preload=none src=\"%s_%s_%s.wav\"></audio></td>", RV[r].tag, SRC[s].tag, TAG[v]);
                printf("demo %s\n", path);
            }
            fprintf(h, "</tr></table>");
        }
    fprintf(h, "</body>");
    fclose(h);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: locut_demo OUTDIR\n"); return 1; }
    demo(argv[1]);
    return 0;
}
