/* SPDX-License-Identifier: GPL-3.0-only
 * Host listening set (not firmware, not in a test list): the firmware's PLATE against HALL and ROOM, and against the host harness's
 * integer rendition it was ported from (revcompare_test.c, the "small" one), on the same dry sources (pluck, click, pad, padlong:
 * revcompare_test.c's capture()), level-matched to ROOM's wet energy so you hear character, not loudness. Writes WAVs, an index.html
 * and report.txt (RT60, levels, how close the firmware's PLATE is to the harness's: a null test on the wet).
 *
 *   cc -O2 -w -DDUELL_TEST_MODE=1 -Ibuild/gen -Ifirmware/src -Itests -o build/host/plate_render tests/plate_render.c -lm
 *   build/host/plate_render OUTDIR
 *
 * 0.2.14: the reverb low cut (LOCUT). Without PLATE_AB the set is rendered with LOCUT OFF (as before the low cut existed). With PLATE_AB=1 each
 * file is rendered twice, <job>_<MODEL>.wav with LOCUT OFF and <job>_<MODEL>_locut.wav with the default LOCUT (88 Hz), at the SAME gain (the one that
 * matches the OFF render to ROOM's wet energy), and index.html lists them side by side. The harness's file is not filtered (it is not the firmware).
 *
 * The harness follows the reverb of the CHOMPI firmware (MIT): see revcompare_test.c, LICENSES/MIT-CHOMPI.txt. */
#define REVCOMPARE_NO_MAIN
#include "revcompare_test.c"

enum { W_ROOM, W_HALL, W_PLATE, W_HARN, W_N };
static const char *const W_NAME[W_N] = {"ROOM", "HALL", "PLATE", "harness_small"};
static int32_t wl_[W_N][NFR], wr_[W_N][NFR];
static double harn_gain;
static const char *const J_TAG[4] = {"pluck", "click", "pad", "padlong"};
static const int32_t J_SIZE[4] = {90, 90, 90, 127};
#define JOBS_TAG(j) J_TAG[j]
#define JOBS_SIZE(j) J_SIZE[j]

static int g_locut;                    /* the LOCUT value of the render (set after model_reset, which loads the defaults) */
static void wet_start(int w, int32_t size)
{
    if (w == W_HARN) {
        model_reset(M_CHS, size, 60);
        ch_time[M_CHS] = harn_gain;
        chi_params(&chi[2], CH_KNOB, harn_gain);
        return;
    }
    model_reset(w == W_HALL ? M_HALL : M_ROOM, size, 60);
    fx.lo_lp = fx.lo_k = fx.lo_g = 0;     /* (the low cut from rest) */
    if (w == W_PLATE) {
        song.g[G_RTYPE] = 5;
        rev_clear();
        fx.rtype = 5;
    }
    song.g[G_RLOCUT] = (int16_t)g_locut;
}
/* the wet of source src through w (unscaled), into wl_[w], wr_[w] */
static void wet_render(int src, int w, int32_t size)
{
    static int32_t sl[CTL], sr[CTL];
    uint32_t t, i;
    if (cap_src != src) capture(src);
    wet_start(w, size);
    for (t = 0; t < NFR; t += CTL) {
        if (w == W_HARN) {
            model_wet(M_CHS, cap_s + t, sl, sr, CTL);
        } else {
            for (i = 0; i < CTL; i++) {
                send_c[i] = send_d[i] = 0;
                send_r[i] = cap_s[t + i];
                wet[i] = 0;
            }
            hl.side = 0;
            fx_buses(send_c, send_d, send_r, wet, CTL);
            for (i = 0; i < CTL; i++) {
                sl[i] = wet[i] + (hl.side ? rev_side[i] : 0);
                sr[i] = wet[i] - (hl.side ? rev_side[i] : 0);
            }
        }
        for (i = 0; i < CTL; i++) {
            wl_[w][t + i] = sl[i];
            wr_[w][t + i] = sr[i];
        }
    }
}
static double energy(int w)
{
    double e = 0;
    uint32_t i;
    for (i = 0; i < NFR; i++)
        e += (double)wl_[w][i] * wl_[w][i] + (double)wr_[w][i] * wr_[w][i];
    return e;
}
static void write_wav(const char *path, int src, int w, int32_t gq)      /* w < 0: dry */
{
    FILE *f = fopen(path, "wb");
    uint32_t t;
    int32_t mg = (int32_t)song.master_q12;
    wav_hdr(f, NFR);
    (void)src;
    for (t = 0; t < NFR; t++) {
        int32_t a = w < 0 ? 0 : (wl_[w][t] * gq) >> 10, b = w < 0 ? 0 : (wr_[w][t] * gq) >> 10, l, r;
        l = (((cap_l[t] + a) >> 2) * mg) >> 10;
        r = (((cap_r[t] + b) >> 2) * mg) >> 10;
        master_out(&l, &r);
        wav_put(f, l, r);
    }
    fclose(f);
}
/* the RT60 of the wet response of w (as revcompare_test.c model_rt60): a 50 ms noise burst on the send */
static double rt60_of(int w, int32_t size)
{
    static int32_t in[CTL], sl[CTL], sr[CTL];
    uint32_t t, i;
    double s = 0, t5 = -1, t35 = -1, dc = 0;
    wet_start(w, size);
    xs = 0x1234567u;
    for (t = 0; t < IRN; t += CTL) {
        for (i = 0; i < CTL; i++) in[i] = t + i < FS / 20u ? noise(IR_AMP) : 0;
        if (w == W_HARN) {
            model_wet(M_CHS, in, sl, sr, CTL);
        } else {
            for (i = 0; i < CTL; i++) { send_c[i] = send_d[i] = 0; send_r[i] = in[i]; wet[i] = 0; }
            hl.side = 0;
            fx_buses(send_c, send_d, send_r, wet, CTL);
            for (i = 0; i < CTL; i++) { sl[i] = wet[i] + (hl.side ? rev_side[i] : 0); sr[i] = wet[i] - (hl.side ? rev_side[i] : 0); }
        }
        for (i = 0; i < CTL; i++) ir_h[t + i] = (sl[i] + sr[i]) / 2;
    }
    for (i = IRN - FS / 2u; i < IRN; i++) dc += ir_h[i];
    dc /= FS / 2u;
    for (i = 0; i < IRN; i++) ir_h[i] -= (int32_t)(dc < 0 ? dc - 0.5 : dc + 0.5);
    for (i = IRN; i-- > 0;) { s += (double)ir_h[i] * ir_h[i]; ir_e[i] = s; }
    if (s <= 0) return 0;
    for (i = 0; i < IRN; i++) {
        double db = 10 * log10(ir_e[i] / ir_e[0] + 1e-30);
        if (t5 < 0 && db <= -5) t5 = i;
        if (t35 < 0 && db <= -35) { t35 = i; break; }
    }
    return t35 < 0 ? 99 : (t35 - t5) * 2.0 / FS;
}

int main(int argc, char **argv)
{
    char path[600], dir[500];
    FILE *rp, *ix;
    static const int ORDER[4] = {W_PLATE, W_HARN, W_HALL, W_ROOM};
    int j, w, k, ab = getenv("PLATE_AB") != 0;
    double rt[2][W_N], raw[4][W_N];
    snprintf(dir, sizeof dir, "%s", argc > 1 ? argv[1] : "build/plate_listen");
    mkdir(dir, 0755);
    snprintf(path, sizeof path, "%s/report.txt", dir);
    rp = fopen(path, "w");
    if (!rp) return 1;
#define OUT(...) do { printf(__VA_ARGS__); fprintf(rp, __VA_ARGS__); } while (0)
    chi_setup(&chi[0], 0, 1.0, chi_mem_full);
    chi_setup(&chi[1], 1, 1.0, chi_mem_half);
    chi_setup(&chi[2], 1, 0.546, chi_mem_small);
    for (j = 0; j < 2; j++) {
        int32_t size = j ? 127 : 90;
        harn_gain = 0.0;
        for (w = 0; w < W_N; w++) {
            if (w == W_HARN) {      /* the harness at PLATE's own loop gain (so the null test compares rounding, not a different decay) */
                song.g[G_RSIZE] = (int16_t)size;
                harn_gain = plate_gain(size) / 32768.0;
            }
            rt[j][w] = rt60_of(w, size);
        }
        OUT("plate_render: SIZE %d DAMP 60: RT60", (int)size);
        for (w = 0; w < W_N; w++) OUT(" %s %.2f s%s", W_NAME[w], rt[j][w], w == W_N - 1 ? "" : ",");
        OUT("; PLATE's loop gain %.3f\n", harn_gain);
    }
    for (j = 0; j < 4; j++) {
        int32_t size = JOBS_SIZE(j), gq[W_N];
        int s = j == 3 ? 2 : j;       /* (padlong = the pad source) */
        double e[W_N], num = 0, dl = 0, dh = 0;
        uint32_t i;
        song.g[G_RSIZE] = (int16_t)size;
        harn_gain = plate_gain(size) / 32768.0;
        for (w = 0; w < W_N; w++) wet_render(s, w, size);
        for (w = 0; w < W_N; w++) {
            double g;
            e[w] = energy(w);
            g = sqrt(e[W_ROOM] / (e[w] + 1e-9));
            g = g < 0.05 ? 0.05 : g > 30 ? 30 : g;
            gq[w] = (int32_t)(g * 1024.0 + 0.5);
            raw[j][w] = 10 * log10((e[w] + 1e-9) / (e[W_ROOM] + 1e-9));
        }
        for (i = 0; i < NFR; i++) {          /* the null test: PLATE against the harness at the same loop gain */
            double a = wl_[W_PLATE][i], b = wl_[W_HARN][i];
            num += (a - b) * (a - b);
            dl += a * a;
            dh += b * b;
        }
        OUT("plate_render: %s: wet level against ROOM before matching:", JOBS_TAG(j));
        for (w = 0; w < W_N; w++) OUT(" %s %+.1f dB%s", W_NAME[w], raw[j][w], w == W_N - 1 ? "\n" : ",");
        OUT("plate_render: %s: firmware PLATE against the harness (left wet, same loop gain): difference %.1f dB below the signal (rms %.0f / %.0f)\n",
            JOBS_TAG(j), 10 * log10(dl / (num + 1e-9)), sqrt(dl / NFR), sqrt(dh / NFR));
        snprintf(path, sizeof path, "%s/%s_dry.wav", dir, JOBS_TAG(j));
        write_wav(path, s, -1, 1024);
        for (w = 0; w < W_N; w++) {
            snprintf(path, sizeof path, "%s/%s_%s.wav", dir, JOBS_TAG(j), W_NAME[w]);
            write_wav(path, s, w, gq[w]);
        }
        if (ab) {                            /* the same again with the default LOCUT, at the same gains */
            g_locut = GP[G_RLOCUT].def;
            for (w = 0; w < W_N; w++) {
                wet_render(s, w, size);
                snprintf(path, sizeof path, "%s/%s_%s_locut.wav", dir, JOBS_TAG(j), W_NAME[w]);
                write_wav(path, s, w, gq[w]);
            }
            g_locut = 0;
        }
    }
    fclose(rp);
    snprintf(path, sizeof path, "%s/index.html", dir);
    ix = fopen(path, "w");
    fprintf(ix, "<!doctype html><meta charset=utf-8><title>DUELL PLATE reverb</title>"
                "<style>body{font:15px sans-serif;max-width:62em;margin:1em auto}td,th{padding:3px 8px;text-align:left;vertical-align:middle}"
                "h2{margin-top:1.4em}audio{width:300px;height:30px}</style><h1>DUELL PLATE (the sixth reverb)</h1>"
                "<p>The firmware's integer PLATE (the code in fx.c, run on the Mac) against HALL and ROOM, and against the host harness's integer "
                "version it was ported from. Same dry source and send for each, SIZE 90 DAMP 60 (padlong: SIZE 127). <b>The wet level of every file is "
                "matched to ROOM's</b>, so you hear character, not loudness. 44.1 kHz stereo 16-bit.</p>"
                "<p><b>Listen first:</b> pad_PLATE against pad_harness_small (they should sound the same), then against pad_HALL and pad_ROOM; "
                "then pluck, click, padlong. What the firmware plays on the device is the PLATE file, not the harness one.</p>");
    if (ab) {
        fprintf(ix, "<h2>LOCUT: off against the default (88 Hz)</h2><p>Each model twice at the same gain: <b>LOCUT OFF</b> (how the reverb sounded before) and <b>LOCUT 88 Hz</b> "
                    "(the new default: a one-pole high-pass on what goes into the reverb). The harness is not part of the firmware and is not filtered.</p>");
        for (j = 0; j < 4; j++) {
            fprintf(ix, "<h3>%s</h3><table><tr><th></th><th>LOCUT OFF</th><th>LOCUT 88 Hz</th></tr><tr><td>dry</td><td><audio controls preload=none src=\"%s_dry.wav\"></audio></td><td></td></tr>", JOBS_TAG(j), JOBS_TAG(j));
            for (k = 0; k < 4; k++) {
                w = ORDER[k];
                if (w == W_HARN) continue;
                fprintf(ix, "<tr><td><b>%s</b></td><td><audio controls preload=none src=\"%s_%s.wav\"></audio></td><td><audio controls preload=none src=\"%s_%s_locut.wav\"></audio></td></tr>",
                        W_NAME[w], JOBS_TAG(j), W_NAME[w], JOBS_TAG(j), W_NAME[w]);
            }
            fprintf(ix, "</table>");
        }
        fprintf(ix, "<hr><h2>The original set (LOCUT OFF)</h2>");
    }
    for (j = 0; j < 4; j++) {
        fprintf(ix, "<h2>%s</h2><table><tr><td>dry</td><td><audio controls preload=none src=\"%s_dry.wav\"></audio></td><td></td></tr>", JOBS_TAG(j), JOBS_TAG(j));
        for (k = 0; k < 4; k++) {
            w = ORDER[k];
            fprintf(ix, "<tr><td><b>%s</b></td><td><audio controls preload=none src=\"%s_%s.wav\"></audio></td><td>RT60 %.1f s, wet before matching %+.1f dB vs ROOM</td></tr>",
                    W_NAME[w], JOBS_TAG(j), W_NAME[w], rt[j == 3 ? 1 : 0][w], raw[j][w]);
        }
        fprintf(ix, "</table>");
    }
    fprintf(ix, "<p><a href=report.txt>report.txt</a></p>");
    fclose(ix);
    printf("plate_render: wrote %s\n", dir);
    return 0;
}
