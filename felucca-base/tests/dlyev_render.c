/* SPDX-License-Identifier: GPL-3.0-only
 * Host tool (not firmware, not in any test list): demo WAVs of the delay's modes through the REAL firmware path (events_block, mix_block: the parts, the
 * delay bus with DELAY MODE, the master). Four sources (pluck, pad, chord, beat: ANALOG / WAVE / PHYS / NOISE engines, no drum engine) x DELAY (PING-PONG
 * on, the delay the owner has) / RAND / OCT+ x three settings, 10 s each, plus index.html. Reuses graindelay_test.c's sources and WAV writer.
 *   build (from felucca-base, after host_tests.sh has made build/gen):
 *     cc -O2 -w -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests -o build/host/dlyev_render tests/dlyev_render.c -lm
 *   build/host/dlyev_render OUTDIR
 * 0.2.22: build/host/dlyev_render OUTDIR duel renders the delay's modes in DUEL instead (part 1 hard left: the ANALOG PLUCK chords, part 2 hard right: the ANALOG SOFT PAD
 * note, one delay on each part's own bus, mono): DIGI / PING / RAND / OCT+ x two settings, stereo WAVs + index.html. */
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

static const char *const DM_NAME[4] = {"DIGI", "PING", "RAND", "OCT+"};
static const char *const DM_FILE[4] = {"digi", "ping", "rand", "octup"};
static void duel_render(int md, const rs_t *st, const char *dir, char *fname, int32_t *pkl, int32_t *pkr)
{
    static int32_t ob2[2 * CTL];
    uint32_t t, i;
    FILE *f;
    char path[512];
    const uint8_t N0 = 55;
    snprintf(fname, 128, "duel_%s_%s.wav", DM_FILE[md], st->name);
    snprintf(path, sizeof path, "%s/%s", dir, fname);
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    vage = 0;                                               /* (the voice clock and the note RAND from power-on: a file always sounds the same) */
    mod_seed = 0x2545F491u;
    duell_mode_req = DM_SPLIT;
    duell_mode_force(DM_STEREO);
    duell_mode_force(DM_SPLIT);                             /* (from rest: every line and state cleared) */
    memset(dly_buf, 0, sizeof dly_buf);
    memset(&fx, 0, sizeof fx);
    memset(&fx_b, 0, sizeof fx_b);
    memset(&hl, 0, sizeof hl);
    memset(&hl_b, 0, sizeof hl_b);
    lim_env = lim_env_r = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    dlyev_clear();
    song.g[G_BPM] = 100;
    song.g[G_CLOCK] = 0;
    song.g[G_RTYPE] = 0;
    song.g[G_DTIME] = 1;
    song.g[G_DFDBK] = (int16_t)st->fdbk;
    song.g[G_DCOLOR] = 70;
    song.g[G_DMIX] = 90;
    song.g[G_PINGSPREAD] = 100;
    song.g[G_DMRATE] = 44;
    song.g[G_DMDEP] = md >= 2 ? (int16_t)st->chance : 0;
    song.g[G_DMODE] = (int16_t)md;
    {
        static const uint8_t C1[3] = {60, 64, 67}, A1[3] = {57, 60, 64}, F1[3] = {53, 57, 60};
        gs_track(0, GE_ANALOG, 8);                          /* part 1: PLUCK, three chords */
        for (i = 0; i < 16u; i++) {
            const uint8_t *cd = i == 0 ? C1 : i == 6 ? A1 : F1;
            put_step(&trk[0], i, i == 0 || i == 6 || i == 10 ? 3u : 0u, cd, i == 0 || i == 6 || i == 10 ? ST_NOTE : ST_REST, 0);
        }
        gs_track(1, GE_ANALOG, 1);                          /* part 2: SOFT PAD, one held note */
        for (i = 0; i < 16u; i++) put_step(&trk[1], i, i == 0 ? 1u : 0u, &N0, i == 0 ? ST_NOTE : i < 14u ? ST_TIE : ST_REST, 0);
    }
    *pkl = *pkr = 0;
    f = fopen(path, "wb");
    wav_hdr(f, GN);
    transport_req = 1;
    for (t = 0; t < GN; t += CTL) {
        if (t <= (uint32_t)(4.7 * FS) && t + CTL > (uint32_t)(4.7 * FS)) transport_req = 2;
        events_block(CTL);
        mix_block(ob2, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t l = ob2[2u * i], r = ob2[2u * i + 1u];
            *pkl = (l < 0 ? -l : l) > *pkl ? (l < 0 ? -l : l) : *pkl;
            *pkr = (r < 0 ? -r : r) > *pkr ? (r < 0 ? -r : r) : *pkr;
            wav_put(f, l, r);
        }
    }
    fclose(f);
}
static int duel_main(const char *dir)
{
    static const rs_t DS[2] = {{"medium", 60, 50}, {"heavy", 85, 100}};
    int md, s;
    char fn[128], path[512];
    FILE *ix;
    mkdir(dir, 0755);
    gd_tables();
    snprintf(path, sizeof path, "%s/index.html", dir);
    ix = fopen(path, "w");
    fprintf(ix, "<!doctype html><meta charset=utf-8><title>DUELL 0.2.22: the delay modes in DUEL</title><style>body{font:15px system-ui;margin:2em;max-width:62em}"
                "td{padding:2px 10px}audio{width:260px;height:30px}</style>\n<h1>DUELL 0.2.22: the delay's modes in DUEL (host renders)</h1>\n"
                "<p>DUEL mode, stereo WAVs: <b>part 1 hard left</b> (ANALOG PLUCK, three chords, 100 BPM) and <b>part 2 hard right</b> (ANALOG SOFT PAD, one held note). "
                "Each part has its own delay (TIME 1/8, COLR 70, MIX 90, delay send 100, reverb off), mono. The real firmware path: the parts, the delay MODE, the master.</p>"
                "<ul><li><b>DIGI</b>: the plain delay (as before).</li><li><b>PING</b>: sounds the same as DIGI (a ping-pong needs two sides; in DUEL a part has one).</li>"
                "<li><b>RAND</b>: on a share of the beats the repeat is retriggered, reversed, an octave up or down; <b>OCT+</b>: octave-up repeats. The events stay in the middle of each part's own side: "
                "the pluck's repeats never reach the right, the pad's never the left (headphones: check the pad's side is silent for the pluck's echoes).</li></ul>"
                "<p>Settings: medium FDBK 60 / CHANCE 50, heavy FDBK 85 / CHANCE 100. The random draws use a fixed seed: a file always sounds the same. Not listened to by the author of the code.</p>\n"
                "<table><tr><th></th><th>DIGI</th><th>PING</th><th>RAND</th><th>OCT+</th></tr>\n");
    for (s = 0; s < 2; s++) {
        fprintf(ix, "<tr><td>%s</td>", DS[s].name);
        for (md = 0; md < 4; md++) {
            int32_t pl, pr;
            duel_render(md, &DS[s], dir, fn, &pl, &pr);
            fprintf(ix, "<td><audio controls preload=none src=\"%s\"></audio></td>", fn);
            printf("dlyev_render: %s peaks left %d right %d\n", fn, (int)pl, (int)pr);
        }
        fprintf(ix, "</tr>\n");
    }
    fprintf(ix, "</table>\n");
    fclose(ix);
    return 0;
}
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    if (argc > 2 && !strcmp(argv[2], "duel"))
        return duel_main(dir);
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
