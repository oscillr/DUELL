/* SPDX-License-Identifier: GPL-3.0-only */
/* MASTER volume hunt (release_drafts/HUNT4.md). Not part of the test list. Three tools in one program:
 *  - the real audio block (audio.c audio_block: mix, USB tap, master_dac, click) with a simulated MASTER pot (master.c master_poll) and a held chord, the DAC feed written to a WAV
 *    (and the Q12 gain per block to OUT.wav.csv) for tools/bench_analyze.py:   master_hunt OUT.wav PATTERN POLL_BLOCKS
 *  - SINE=Hz: the gain stage alone on a sine, the 0.2.24 stage (one Q15 gain a block) and master_dac side by side as raw int32 (OUT.old.i32 / OUT.new.i32)
 *  - COST=1: host instructions per sample of the old stage and of master_dac
 *    PATTERN: slow fast step1 step8 low lowstep still top (the pot at its end). POLL_BLOCKS: master_poll runs every this many blocks (1 block = 0.73 ms; the main loop's real rate is not known)
 * Build (audio_test.c has its own main, so a copy with that one renamed is included):
 *   sed 's/^int main(void)$/int at_main(void)/' tests/audio_test.c > tests/.at_copy.c
 *   cc -O1 -w -DDUELL_TEST_MODE=1 -DHOST_MIX_FOLLOWS_MASTER=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests '-DFELUCCA_VERSION="x"' -o build/host/master_hunt tests/master_hunt.c -lm
 * (-DHUNT_HAS_FIXED and the old firmware: the 0.2.24 tree with MENU > USB LEVEL, for the before / after runs of the report) */
#include ".at_copy.c"
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#include <unistd.h>
static uint64_t instr_now(void)
{
    struct rusage_info_v4 ri;
    return proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri) ? 0 : ri.ri_instructions;
}
#else
static uint64_t instr_now(void) { return 0; }
#endif

static double pot_at(const char *pat, double t)         /* the pot (ADC 0..1023) at t seconds */
{
    double v;
    if (!strcmp(pat, "slow"))    { v = t < 1 ? 300 : t < 3 ? 300 + (t - 1) * 200 : t < 3.5 ? 700 : t < 5.5 ? 700 - (t - 3.5) * 200 : 300; }
    else if (!strcmp(pat, "fast")) { v = t < 1 ? 100 : t < 1.25 ? 100 + (t - 1) * 3200 : t < 2.5 ? 900 : t < 2.75 ? 900 - (t - 2.5) * 3200 : 100; }
    else if (!strcmp(pat, "step1")) { v = t < 1 ? 500 : 500 + (int)((t - 1) / 0.05); if (v > 540) v = 540; }
    else if (!strcmp(pat, "step8")) { v = t < 1 ? 500 : 500 + 8 * (int)((t - 1) / 0.1); if (v > 580) v = 580; }
    else if (!strcmp(pat, "low"))   { v = t < 1 ? 30 : t < 3 ? 30 + (t - 1) * 30 : 90; }
    else if (!strcmp(pat, "lowstep")) { v = t < 1 ? 20 : 20 + (int)((t - 1) / 0.1); if (v > 50) v = 50; }
    else if (!strcmp(pat, "top")) v = 1023;
    else v = 500;
    return v;
}
int main(int argc, char **argv)
{
    int32_t out[64];
    uint32_t b, poll = argc > 3 ? (uint32_t)atoi(argv[3]) : 1, i, nb = 44100u * 6u / 32u;
    int fixed = argc > 4 ? atoi(argv[4]) : 0;
    FILE *w = fopen(argv[1], "wb"), *c;
    char cn[512];
    uint32_t dlen = nb * 32u * 4u, v32;
    uint16_t v16;
    snprintf(cn, sizeof cn, "%s.csv", argv[1]);
    c = fopen(cn, "w");
    if (getenv("COST")) {                              /* host instructions per sample (a channel pair) of the old stage and of master_dac: at rest and gliding */
        static int32_t x[64], y[64];
        uint64_t t0, t1;
        uint32_t k, N = 200000;
        double c_old, c_rest, c_glide;
        for (i = 0; i < 64; i++) x[i] = (int32_t)(8000 * sin(i * 0.37));
        t0 = instr_now();
        for (k = 0; k < N; k++) { int32_t m = 1500 + (int32_t)(k & 1); memcpy(y, x, sizeof y); for (i = 0; i < 64; i++) { int32_t v = (y[i] * m) >> 11; y[i] = v > 32767 ? 32767 : v < -32768 ? -32768 : v; } }
        t1 = instr_now(); c_old = (double)(t1 - t0) / N;
        t0 = instr_now();
        for (k = 0; k < N; k++) memcpy(y, x, sizeof y);
        t1 = instr_now(); c_old -= (double)(t1 - t0) / N;
        song.master_q12 = 1500; master_g16 = 1500u * 16u; mdac_g = -1;
        t0 = instr_now();
        for (k = 0; k < N; k++) { memcpy(y, x, sizeof y); master_dac(y, 32); }
        t1 = instr_now(); c_rest = (double)(t1 - t0) / N - (double)(t1 - t0 > 0 ? 0 : 0);
        song.master_q12 = 1500; mdac_g = -1;
        t0 = instr_now();
        for (k = 0; k < N; k++) { song.master_q12 = 1500u + (k & 1u) * 1000u; master_g16 = song.master_q12 << 4; memcpy(y, x, sizeof y); master_dac(y, 32); }
        t1 = instr_now(); c_glide = (double)(t1 - t0) / N;
        t0 = instr_now();
        for (k = 0; k < N; k++) memcpy(y, x, sizeof y);
        t1 = instr_now();
        c_rest -= (double)(t1 - t0) / N; c_glide -= (double)(t1 - t0) / N;
        printf("host instructions per sample (stereo pair): 0.2.24 usb_fixed_dac %.1f; master_dac at rest %.1f, gliding %.1f\n", c_old / 32, c_rest / 32, c_glide / 32);
        return 0;
    }
    if (getenv("SINE")) {                              /* the gain stage alone on a sine (Hz in SINE, peak in SINE_PK, default 8192 = -12 dBFS): the old stage (per-block Q15 gain,
                                                        * usb_fixed_dac of 0.2.24) and the new one (master_dac), the DAC feed as raw int32 (Q15 x 2^7) in OUT.old.i32 / OUT.new.i32 */
        double hz = atof(getenv("SINE")), pk = getenv("SINE_PK") ? atof(getenv("SINE_PK")) : 8192.0;
        char on[512], nn[512];
        FILE *fo, *fn;
        uint32_t nsamp = 0;
        snprintf(on, sizeof on, "%s.old.i32", argv[1]); snprintf(nn, sizeof nn, "%s.new.i32", argv[1]);
        fo = fopen(on, "wb"); fn = fopen(nn, "wb");
        host_pot = (int32_t)pot_at(argv[2], 0);
        master_knob = host_pot * 16;
        song.master_q12 = master_of_pot((uint32_t)host_pot);
        mdac_g = -1;
        for (b = 0; b < nb; b++) {
            int32_t x[64], y[64];
            host_pot = (int32_t)pot_at(argv[2], b * 32.0 / 44100.0);
            if (b % poll == 0) master_poll();
            for (i = 0; i < 32; i++, nsamp++) x[2 * i] = x[2 * i + 1] = (int32_t)floor(pk * sin(6.283185307179586 * hz * nsamp / 44100.0) + 0.5);
            memcpy(y, x, sizeof y);
            {   /* the 0.2.24 stage, as it was: one gain for the block, Q15 result, then the 24-bit scale */
                int32_t m = (int32_t)master_of_pot((uint32_t)(master_knob / 16));   /* (0.2.24: the whole-step level) */
                for (i = 0; i < 64; i++) { int32_t v = (x[i] * m) >> 11; v = v > 32767 ? 32767 : v < -32768 ? -32768 : v; v *= 1 << OUT_SHIFT; fwrite(&v, 4, 1, fo); }
            }
            master_dac(y, 32);
            fwrite(y, 4, 64, fn);
        }
        fclose(fo); fclose(fn);
        return 0;
    }
    fresh();
    lim_env = LIM_T; dc_l = dc_r = dce_l = dce_r = 0;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_DIST] = trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    memset(&pf, 0, sizeof pf);
    pf.src = pf.next = PF_N; pf.pbeat = perf_beat;
    pf.mg[0] = pf.mg[1] = pf.mg[2] = pf.mg[3] = 32768;
    host_pot = (int32_t)pot_at(argv[2], 0);
    master_knob = host_pot * 16;
    song.master_q12 = master_of_pot((uint32_t)host_pot);
#ifdef HUNT_HAS_FIXED
    fx_usb_fixed = (uint8_t)fixed;                     /* (the 0.2.24 firmware: MENU > USB LEVEL MASTER / FIXED) */
#else
    host_mix_follows_master = 0;                       /* (0.2.25: the device's own path, fx.c MIX_MG) */
    mdac_g = -1;
#endif
    (void)fixed;
    trk_note_on(&trk[0], 48, 110); trk_note_on(&trk[0], 55, 110); trk_note_on(&trk[0], 64, 110); trk_note_on(&trk[0], 72, 110);
    fwrite("RIFF", 1, 4, w); v32 = 36 + dlen; fwrite(&v32, 4, 1, w); fwrite("WAVEfmt ", 1, 8, w);
    v32 = 16; fwrite(&v32, 4, 1, w); v16 = 1; fwrite(&v16, 2, 1, w); v16 = 2; fwrite(&v16, 2, 1, w);
    v32 = 44100; fwrite(&v32, 4, 1, w); v32 = 44100 * 4; fwrite(&v32, 4, 1, w); v16 = 4; fwrite(&v16, 2, 1, w); v16 = 16; fwrite(&v16, 2, 1, w);
    fwrite("data", 1, 4, w); fwrite(&dlen, 4, 1, w);
    for (b = 0; b < nb; b++) {
        host_pot = (int32_t)pot_at(argv[2], b * 32.0 / 44100.0);
        if (b % poll == 0) master_poll();
        audio_block(out, 32);
        fprintf(c, "%u,%d,%d\n", b, (int)song.master_q12, (int)host_pot);
        for (i = 0; i < 64; i++) { int16_t s = (int16_t)(out[i] >> OUT_SHIFT); fwrite(&s, 2, 1, w); }
    }
    fclose(w); fclose(c);
    return 0;
}
