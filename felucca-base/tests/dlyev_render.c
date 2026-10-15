/* SPDX-License-Identifier: GPL-3.0-only
 * Host tool (not firmware, not in any test list): demo WAVs of the delay's modes through the REAL firmware path (events_block, mix_block: the parts, the
 * delay bus with DELAY MODE, the master). Four sources (pluck, pad, chord, beat: ANALOG / WAVE / PHYS / NOISE engines, no drum engine) x DELAY (PING-PONG
 * on, the delay the owner has) / RAND / OCT+ x three settings, 10 s each, plus index.html. Reuses graindelay_test.c's sources and WAV writer.
 *   build (from felucca-base, after host_tests.sh has made build/gen):
 *     cc -O2 -w -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests -o build/host/dlyev_render tests/dlyev_render.c -lm
 *   build/host/dlyev_render OUTDIR */
#define GD_NO_MAIN
#include "graindelay_test.c"
#include <sys/stat.h>

typedef struct { const char *name; int fdbk, chance; } rs_t;
static const rs_t RS[3] = {{"light", 40, 25}, {"medium", 60, 50}, {"heavy", 90, 80}};
static const char *const RM[3] = {"DELAY", "RAND", "OCT+"};
static const char *const RF[3] = {"delay", "rand", "octup"};

static void dm_render(int src, int md, const rs_t *st, const char *dir, char *fname)
{
    static int32_t ob2[2 * CTL];
    uint32_t t, i;
    FILE *f;
    char path[512];
    snprintf(fname, 128, "%s_%s_%s.wav", GS[src].tag, RF[md], st->name);
    snprintf(path, sizeof path, "%s/%s", dir, fname);
    gs_setup(src);
    memset(dly_buf, 0, sizeof dly_buf);
    memset(cho_buf, 0, sizeof cho_buf);
    memset(&fx, 0, sizeof fx);
    dlyev_clear();
    rev_clear();
    song.g[G_RTYPE] = 0;
    song.g[G_DTIME] = GS_DTIME[src];
    song.g[G_DFDBK] = (int16_t)st->fdbk;
    song.g[G_DCOLOR] = 70;
    song.g[G_DMIX] = 90;
    song.g[G_PINGSPREAD] = 100;
    dly_plain_pp = 1;
    song.g[G_DMRATE] = 44;
    song.g[G_DMDEP] = md == 0 ? 0 : (int16_t)st->chance;
    song.g[G_DMODE] = (int16_t)(md == 0 ? DMD_PING : md + 1);   /* (md 0: the plain delay with the ping-pong, 1 RANDOM, 2 OCT UP) */
    f = fopen(path, "wb");
    wav_hdr(f, GN);
    transport_req = 1;
    for (t = 0; t < GN; t += CTL) {
        if (t <= (uint32_t)(GS_STOP[src] * FS) && t + CTL > (uint32_t)(GS_STOP[src] * FS)) transport_req = 2;
        events_block(CTL);
        mix_block(ob2, CTL);
        for (i = 0; i < CTL; i++) wav_put(f, ob2[2u * i], ob2[2u * i + 1u]);
    }
    fclose(f);
}
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    int src, md, s;
    char fn[128];
    FILE *ix;
    char path[512];
    mkdir(dir, 0755);
    gd_tables();
    snprintf(path, sizeof path, "%s/index.html", dir);
    ix = fopen(path, "w");
    fprintf(ix, "<!doctype html><meta charset=utf-8><title>DUELL delay modes</title><style>body{font:15px system-ui;margin:2em;max-width:62em}"
                "td{padding:2px 10px}audio{width:240px;height:30px}h2{margin-top:1.6em}</style>\n<h1>DUELL 0.2.14: the delay's modes (host renders)</h1>\n"
                "<p>The real firmware path: the parts, DELAY MODE, the master. PING-PONG = the delay as it is with the ping-pong; RANDOM and OCT UP = the event modes, the ping-pong not used. "
                "Delay only (reverb off), MIX 90, COLR 70. Settings: light FDBK 40 / CHANCE 25, medium 60 / 50, heavy 90 / 80. The random draws use a fixed seed: a file always sounds the same. "
                "Not listened to by the author of the code.</p>\n");
    for (src = 0; src < 4; src++) {
        fprintf(ix, "<h2>%s</h2><p>%s</p><table><tr><th></th><th>DELAY</th><th>RAND</th><th>OCT+</th></tr>\n", GS[src].tag, GS[src].what);
        for (s = 0; s < 3; s++) {
            fprintf(ix, "<tr><td>%s</td>", RS[s].name);
            for (md = 0; md < 3; md++) {
                dm_render(src, md, &RS[s], dir, fn);
                fprintf(ix, "<td><audio controls preload=none src=\"%s\"></audio></td>", fn);
                printf("dlyev_render: %s\n", fn);
            }
            fprintf(ix, "</tr>\n");
        }
        fprintf(ix, "</table>\n");
    }
    fclose(ix);
    return 0;
}
