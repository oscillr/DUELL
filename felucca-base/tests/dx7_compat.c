/* SPDX-License-Identifier: GPL-3.0-only */
/* DX7 compatibility harness for the FM6 engine (host build, read-only: it changes no firmware file). NOT part of host_tests.sh.
 * OPTIONAL and local: the real banks are not in the repo. tests/dx7_compat.sh (host_tests.sh dx7compat) runs the whole recipe below on the folder DX7_BANKS=<dir of
 * 32-voice .syx banks> and reports 'skipped: no banks' (PASS) when it is not set.
 * Recipe (from felucca-base/, build/gen made by DUELL/gen_only.py; the banks: any 32-voice DX7 .syx files):
 *   cc -O2 -w -DDUELL_TEST_MODE=1 -Ibuild/gen -Ifirmware/src -Itests -o build/host/dx7_compat tests/dx7_compat.c -lm
 *   cc -O2 -o build/host/dx7_ref tests/dx7_ref.c -lm
 *   build/host/dx7_compat OUT/render BANKS/*.syx ; build/host/dx7_ref OUT/ref BANKS/*.syx
 *   node tests/dx7_js_parse.mjs web/editor.html OUT/js.tsv BANKS/*.syx          (the editor's own FM6.parseSysex)
 *   python3 -I tests/dx7_analyze.py BANKS OUT/render OUT/ref OUT/js.tsv OUT/results.json
 *   DX7_LFO=1 build/host/dx7_compat x y                                          (the engine's LFO speed / delay as numbers)
 *   build/host/dx7_compat OUTROOT FILE.syx [FILE.syx ...]
 * For every 32-voice DX7 bank (F0 43 0n 09 20 00, 4096 bytes, checksum, F7) it takes the 128-byte packed records exactly
 * as the editor sends them to the unit (FM6.pack(FM6.unpack(rec)) is the identity on in-range data; the device then runs
 * fm6_unpack, eng_fm6.c) and writes into OUTROOT/<bank>/:
 *   unpacked.tsv   index, 155 bytes as hex  (fm6_unpack's result: the python side compares it with its own parse)
 *   NN-NAME.wav    44.1 kHz mono 16 bit: the note held 1.5 s at velocity 100, released, 2.0 s in all, through the
 *                  engine (note_on -> track_render), macros neutral, no FX
 *   render.tsv     index, name, note, vel in, vel seen by the engine, max |sample| (int32 mix), samples beyond int16,
 *                  voice still active at the end
 * The note is MIDI 60 (DX7 C3); names with bass / sub / kick: 36, low / deep: 48. */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define SECS_TOTAL 2.0
#define SECS_HOLD 1.5

static void le32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

static void write_wav(const char *path, const int16_t *s, uint32_t n)
{
    uint8_t h[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0,
                     0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 16, 0, 'd', 'a', 't', 'a', 0, 0, 0, 0};
    FILE *f = fopen(path, "wb");
    uint32_t i;
    if (!f)
        return;
    le32(h + 4, 36 + n * 2u); le32(h + 24, FS); le32(h + 28, FS * 2u); le32(h + 40, n * 2u);
    fwrite(h, 1, 44, f);
    for (i = 0; i < n; i++) {
        uint8_t b[2] = {(uint8_t)s[i], (uint8_t)((uint16_t)s[i] >> 8)};
        fwrite(b, 1, 2, f);
    }
    fclose(f);
}

static void setup_patch(const uint8_t *v)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    memset(eng_mem, 0, sizeof eng_mem);
    memset(fm6_eff, 0, sizeof fm6_eff);
    memset(fm6_lfo, 0, sizeof fm6_lfo);
    host_tracks_init();
    host_preset(&trk[0], ENGI_FM6, 0);
    for (i = 0; i < 8u; i++)
        trk[0].p[P_E0 + i] = 0;
    trk[0].p[P_E7] = 0;
    fm6_set_patch(0, v);
    fm6_slot[0] = 0;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
}

/* the engine's LFO rate and delay as numbers (printed with DX7_LFO=1): speed 0..99 -> Hz of the LFO, delay 0..99 -> seconds
 * until the LFO starts (the ramp's first non-zero block) and until it is at full depth */
static void lfo_table(void)
{
    static const int sp[] = {0, 1, 5, 10, 20, 35, 50, 64, 80, 99}, dl[] = {0, 1, 10, 20, 35, 50, 70, 90, 99};
    uint8_t v[FP_SIZE + 1u];
    uint32_t i, j;
    printf("LFO speed -> Hz\n");
    for (i = 0; i < sizeof sp / sizeof *sp; i++) {
        fm6_lfo_t l;
        memset(v, 0, sizeof v);
        v[FP_LFS] = (uint8_t)sp[i];
        fm6_lfo_reset(&l, v);
        printf("  %2d  %.3f Hz\n", sp[i], (double)l.delta * ((double)FS / FM6_N) / 4294967296.0);
    }
    printf("LFO delay -> seconds to first output / to full depth\n");
    for (i = 0; i < sizeof dl / sizeof *dl; i++) {
        fm6_lfo_t l;
        double t0 = -1, t1 = -1;
        memset(v, 0, sizeof v);
        v[FP_LFD] = (uint8_t)dl[i];
        fm6_lfo_reset(&l, v);
        fm6_lfo_key(&l);
        for (j = 0; j < 4000000u && t1 < 0; j++) {
            int32_t d = fm6_lfo_delay(&l);
            double t = (double)(j + 1) * FM6_N / FS;
            if (d > 0 && t0 < 0) t0 = t;
            if (d >= (1 << 24) - 1) t1 = t;
        }
        printf("  %2d  %.2f s / %.2f s\n", dl[i], t0, t1);
    }
}

int main(int argc, char **argv)
{
    static int16_t pcm[FS * 3];
    int a;
    if (getenv("DX7_LFO")) {
        lfo_table();
        return 0;
    }
    if (argc < 3) {
        if (!getenv("DX7_BANKS")) { printf("dx7_compat: skipped: no banks (set DX7_BANKS=<dir of .syx banks>, or use tests/dx7_compat.sh)\n"); return 0; }
        fprintf(stderr, "usage: dx7_compat OUTROOT FILE.syx ...\n");
        return 2;
    }
    for (a = 2; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb");
        static uint8_t d[8192];
        size_t n;
        char bank[128], dir[512], path[640];
        const char *b = strrchr(argv[a], '/');
        uint32_t k, i, j;
        FILE *ut, *rt;
        if (!f) { fprintf(stderr, "cannot open %s\n", argv[a]); continue; }
        n = fread(d, 1, sizeof d, f);
        fclose(f);
        b = b ? b + 1 : argv[a];
        snprintf(bank, sizeof bank, "%s", b);
        if (strlen(bank) > 4 && !strcmp(bank + strlen(bank) - 4, ".syx"))
            bank[strlen(bank) - 4] = 0;
        for (i = 0; bank[i]; i++)
            if (bank[i] == ' ')
                bank[i] = '_';
        if (n < 4104 || d[0] != 0xF0 || d[1] != 0x43 || d[3] != 9) { fprintf(stderr, "%s: not a 32-voice bank\n", argv[a]); continue; }
        snprintf(dir, sizeof dir, "%s/%s", argv[1], bank);
        snprintf(path, sizeof path, "mkdir -p '%s'", dir);
        if (system(path)) return 1;
        snprintf(path, sizeof path, "%s/unpacked.tsv", dir); ut = fopen(path, "w");
        snprintf(path, sizeof path, "%s/render.tsv", dir); rt = fopen(path, "w");
        for (k = 0; k < 32u; k++) {
            uint8_t v[FP_SIZE + 1u], *pk = d + 6 + k * 128u;
            char nm[11], fn[700];
            uint32_t note, total = (uint32_t)(SECS_TOTAL * FS), hold = (uint32_t)(SECS_HOLD * FS), over = 0, active;
            int32_t peak = 0, vin = 100, vseen;
            static int32_t blk[CTL];
            fm6_unpack(pk, v);
            for (i = 0; i < 10u; i++)
                nm[i] = (char)v[FP_NAME + i];
            nm[10] = 0;
            for (j = 10; j && nm[j - 1] == ' '; j--)
                nm[j - 1] = 0;
            fprintf(ut, "%u\t", k + 1);
            for (i = 0; i < FP_SIZE; i++)
                fprintf(ut, "%02x", v[i]);
            fprintf(ut, "\n");
            {   /* the reference note by name (case-insensitive): bass / sub / kick C1 (36), low / deep C2 (48), else C3 (60) */
                char lo[11];
                for (i = 0; i < 11u; i++)
                    lo[i] = (char)(nm[i] >= 'A' && nm[i] <= 'Z' ? nm[i] + 32 : nm[i]);
                note = strstr(lo, "bass") || strstr(lo, "sub") || strstr(lo, "kick") ? 36u :
                       strstr(lo, "low") || strstr(lo, "deep") ? 48u : 60u;
            }
            setup_patch(v);
            trk_note_on(&trk[0], note, (uint32_t)vin);
            vseen = 0;
            for (i = 0; i < NVOICE; i++)
                if (trk[0].v[i].gate)
                    vseen = trk[0].v[i].mvel ? trk[0].v[i].mvel : trk[0].v[i].vel;
            for (i = 0; i < total; i += CTL) {
                if (i / CTL == hold / CTL)
                    trk_note_off(&trk[0], note);
                memset(blk, 0, sizeof blk);
                track_render(&trk[0], blk, CTL);
                for (j = 0; j < CTL && i + j < total; j++) {
                    int32_t x = blk[j], ax = x < 0 ? -x : x;
                    peak = ax > peak ? ax : peak;
                    over += ax > 32767;
                    pcm[i + j] = (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
                }
            }
            active = 0;
            for (i = 0; i < NVOICE; i++)
                active += trk[0].v[i].active;
            for (j = 0; nm[j]; j++)
                if (!((nm[j] >= '0' && nm[j] <= '9') || (nm[j] >= 'A' && nm[j] <= 'Z') || (nm[j] >= 'a' && nm[j] <= 'z')))
                    nm[j] = '_';
            if (!nm[0]) snprintf(nm, sizeof nm, "VOICE");
            snprintf(fn, sizeof fn, "%s/%02u-%s.wav", dir, k + 1, nm);
            write_wav(fn, pcm, total);
            fprintf(rt, "%u\t%s\t%u\t%d\t%d\t%d\t%u\t%u\n", k + 1, nm, note, vin, vseen, peak, over, active);
        }
        fclose(ut);
        fclose(rt);
        printf("%s: 32 voices -> %s\n", bank, dir);
    }
    return 0;
}
