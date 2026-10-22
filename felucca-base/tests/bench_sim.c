/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* DUELL BENCH on the Mac (firmware/src/bench.c; docs/BENCH_GUIDE.md): the bench runner itself, its row list, its table and its log, against a
 * simulated audio interrupt; and the host model's figure of every row (firmware/src/bench_model.h).
 *   build/host/bench_sim                       the checks (host_tests.sh benchsim)
 *   BENCH_MODEL_UPDATE=1 build/host/bench_sim  writes firmware/src/bench_model.h (the dearest sound of each engine, measured as tests/cpu_bench.c
 *                                              does, and every row's average in the host model)
 * The simulated interrupt: a half (HALF_FRAMES) every 2902 us of simulated time, four mix_block calls of CTL frames; its time is the host's
 * instruction count in the loadcost scale (tests/cpu_bench.c: 420 k instructions = one half = 100 %, -O1 -fno-builtin, the device's memset),
 * so a row's average here is the host model's prediction for that row; cpu_sum / cpu_n / cpu_pk / cpu_miss as audio.c keeps them. The
 * main loop: a frame (ui_test.c frame: ui_input, then ui_draw) every 16 ms, so the bench is driven through the real BENCH screen.
 * Checks:
 *   1 the list: every name unique and within the table's name column (AF_S, 88 px); every ref and every bench_baseline.txt line exists
 *   2 a whole run started from the menu (the BENCH row, a knob right, then OCT+): every row measured, in order, the progress, the pages drawn,
 *     the voices of the rows (1, 4, the pool's 8), the delta rows against their refs
 *   3 the log: the INFO and header lines, one CSV line per row with the header's field count, the END line
 *   4 nothing written to the flash, and the session put back after the run (both parts' values, the mode, the master words)
 *   5 OCT- stops a run: the rows after it NOT RUN, the session put back
 *   6 the model header is current (its row count and the names' signature); with the instruction counter (macOS) every row within 15 %
 *     (and 2 points) of it
 *   7 (bench suite stage 0) the NOTES budget: held open (no numbers) while a cost row measures, given while an M16 row does; the log's
 *     budget column OPEN / ON, the ramcode column (- here: no RAMPLACE on the Mac), the boots column; K3 starts the STRESS row (10 minutes)
 * (BENCH-B's core 1 checks are not in this line: two-core stage 1 brings core 1.) */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define _FORTIFY_SOURCE 0
#define HOST_IRQ_HOOK 1
#ifndef DUELL_BENCH
#define DUELL_BENCH 1
#endif
#define BENCH_TC_SIM 1                                 /* core 1's view for the log (firmware/src/bench.c bench_tc_get): faked below, see sim_tc_* */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif
#include "memfast.h"
void *memset(void *d, int c, size_t n) { return mf_memset(d, c, (unsigned)n); }
void *memcpy(void *d, const void *s, size_t n) { return mf_memcpy(d, s, (unsigned)n); }
static void fm1_irq_off(void);
static void fm1_irq_on(void);
#include "ui_test.c"
static uint8_t nor[0x100000];
static uint32_t nor_writes;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static uint32_t host_erase_off[64], host_erase_row[64], host_erases;
static uint8_t host_erase_row_of(void);
static int st_erase(uint32_t off)
{
    if (host_erases < 64u)
        host_erase_off[host_erases] = off, host_erase_row[host_erases] = host_erase_row_of();
    host_erases++;
    memset(nor + off, 0xFF, 4096);
    nor_writes++;
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n) { const uint8_t *s = src; uint32_t i; for (i = 0; i < n; i++) nor[off + i] &= s[i]; nor_writes++; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

/* ---- what bench.c takes from the device's audio.c, main.c and HAL */
static uint32_t shed_count;
static volatile uint32_t audio_halves;
static char host_log[1u << 17];
static uint32_t host_log_n;
static void host_out(const char *s)
{
    while (*s && host_log_n + 1u < sizeof host_log)
        host_log[host_log_n++] = *s++;
    host_log[host_log_n] = 0;
}
#define BENCH_OUT(s) host_out(s)
#define BENCH_REPLY(s) host_out(s)                    /* (a console word's reply: the same log) */
#define BENCH_HOST 1
#define BENCH_TAG "HOST"
static uint32_t host_boots = 3u;                       /* (the device's restart counter: a fixed figure here; --serve --boots N) */
#define BENCH_BOOTS host_boots

/* core 0's stack marks (main.c c0_stack_paint / c0_stack_bounds on the unit): two small stand-in stacks */
static uint32_t host_c0u[256], host_c0s[64];
static void c0_stack_bounds(uint32_t **b) { b[0] = host_c0u; b[1] = host_c0u + 256; b[2] = host_c0s; b[3] = host_c0s + 64; }
#include "../firmware/src/bench.c"
/* the twin's core 1 (BENCH_SIM_TC=<scenario> in --serve; the default is "ok"): per half, 4 blocks counted as the runner (twocore_run.c) would:
 *   ok         core 1 up, every block in time
 *   late       one block of four late (core 0 played part 2)       vlate   one block of four very late, done in the half, nothing paused (BENCHT3)
 *   vlaterun   as vlate, but the very late block pauses the jobs (a run of very late blocks)               vlatepause  the same through the old rule (vrun 1)
 *   silent     one very late block that was silent (a pause: fault) own     core 1 up but every block played by core 0 (paused)
 *   floor      as ok, and 7 of the own blocks are 'the floor' (core 1's measured run time does not fit before the latest deadline)
 *   noanswer   one core, core 1 did not answer at the start (NOANSWER, a strike)
 *   notarmed   one core, nothing armed                              prev    one core, the last session ended in a HANG (PREV-HANG)
 *   failhang   two cores at the start, core 1 stopped by a HANG after 50 halves */
static struct { char mode[16]; bench_tcv_t v; uint32_t halves; } sim_tc = { "ok", { { 0 }, 0, 1, 0, TCW_AUTO, 0 }, 0 };
static void bench_tc_get(bench_tcv_t *v) { *v = sim_tc.v; }
/* BENCHT2: the twin's timing figures (bench.c bench_tm_get / _clear / _set). Per half they are faked from the scenario's counters: an in-time job has delay 3 us, run 180 us, margin 400 us;
 * a very late one run 700 us, margin -50 us (the last block of the half); the settings are kept and reported as on the unit. */
static struct { bench_tmv_t v; uint64_t sd_sum, rt_sum; int64_t mg_sum; uint32_t slack, calm_ms, lrun, vrun, vwin_k, vwin_ms, floor, clears; } sim_tm = { .slack = 3481, .calm_ms = 10000, .lrun = 4, .vrun = 0, .vwin_k = 0, .vwin_ms = 2000, .floor = 0 };   /* (BENCHT4 defaults: the run and window rules and the floor are off) */
static void bench_tm_get(bench_tmv_t *v)
{
    uint32_t n = sim_tm.v.n;
    *v = sim_tm.v;
    v->sd_mean = n ? (int32_t)(sim_tm.sd_sum / n) : 0, v->rt_mean = n ? (int32_t)(sim_tm.rt_sum / n) : 0, v->mg_mean = n ? (int32_t)(sim_tm.mg_sum / (int64_t)n) : 0;
    v->slack = sim_tm.slack, v->calm_ms = sim_tm.calm_ms, v->lrun = sim_tm.lrun, v->vrun = sim_tm.vrun;
    v->vwin_k = sim_tm.vwin_k, v->vwin_ms = sim_tm.vwin_ms, v->floor = sim_tm.floor;
    v->c1x10 = n ? 253u : 0u, v->c1pkx10 = n ? 310u : 0u;
    if (n) {
        uint32_t i;
        v->st_max = 2300u;
        for (i = 0; i < 4u; i++)
            v->st_mean[i] = 100u + 600u * i, v->dl_mean[i] = 1100u + 400u * i;
    }
}
static void bench_tm_clear(void) { memset(&sim_tm.v, 0, sizeof sim_tm.v); sim_tm.sd_sum = sim_tm.rt_sum = 0, sim_tm.mg_sum = 0, sim_tm.clears++; }
static void bench_tm_set(uint32_t what, uint32_t val)
{
    uint32_t d = val == 0xFFFFFFFFu;
    if (what == 0)
        sim_tm.slack = d ? 3481u : val;
    else if (what == 1)
        sim_tm.calm_ms = d ? 10000u : val;
    else if (what == 2)
        sim_tm.lrun = d ? 4u : val;
    else if (what == 3)
        sim_tm.vrun = d ? 0u : val;
    else if (what == 4)
        sim_tm.vwin_k = d ? 0u : val;
    else if (what == 5)
        sim_tm.vwin_ms = d ? 2000u : val;
    else
        sim_tm.floor = d ? 0u : val;
}
static void sim_tm_job(int vlate)
{
    bench_tmv_t *v = &sim_tm.v;
    int32_t sd = 3, rt = vlate ? 700 : 180, mg = vlate ? -50 : 400;
    v->posted++, v->n++;
    v->hsd[1]++, v->hrt[vlate ? 5 : 2]++, v->hmg[vlate ? 2 : 5]++;
    sim_tm.sd_sum += (uint32_t)sd, sim_tm.rt_sum += (uint32_t)rt, sim_tm.mg_sum += mg;
    if (v->n == 1u || mg < v->mg_min)
        v->mg_min = mg;
    if (sd > v->sd_max)
        v->sd_max = sd;
    if (rt > v->rt_max)
        v->rt_max = rt;
    if (vlate) {
        v->nv++, v->vblk[3]++, v->vrt_max = (uint32_t)rt, v->vrt_mean = (uint32_t)rt, v->vsd_max = (uint32_t)sd;
        v->ev[v->ev_k % BTM_NEV].blk = 3u, v->ev[v->ev_k % BTM_NEV].at_ms = v->n, v->ev[v->ev_k % BTM_NEV].a1 = 3u, v->ev[v->ev_k % BTM_NEV].a2 = 3u;
        v->ev[v->ev_k % BTM_NEV].sd = sd, v->ev[v->ev_k % BTM_NEV].rt = rt, v->ev[v->ev_k % BTM_NEV].mg = mg;
        v->ev_n++, v->ev_k = v->ev_n < BTM_NEV ? v->ev_n : BTM_NEV;
    }
}

static void sim_tc_set(const char *m)
{
    memset(&sim_tc.v, 0, sizeof sim_tc.v);
    strncpy(sim_tc.mode, m, sizeof sim_tc.mode - 1u);
    sim_tc.v.armed = TCW_AUTO;
    sim_tc.v.up = 1;
    if (!strcmp(m, "noanswer"))
        sim_tc.v.up = 0, sim_tc.v.failed = TCR_NOANSWER;
    else if (!strcmp(m, "notarmed"))
        sim_tc.v.up = 0, sim_tc.v.armed = TCW_NOT;
    else if (!strcmp(m, "prev"))
        sim_tc.v.up = 0, sim_tc.v.boot_last = TCR_HANG;
    sim_tc.v.stack = 1208;
}
static void sim_tc_half(void)
{
    uint32_t *c = sim_tc.v.c;
    const char *m = sim_tc.mode;
    uint32_t i, jobs = 0, vl = 0;
    sim_tc.halves++;
    c[BTC_BLOCKS] += 4;
    if (!sim_tc.v.up || !strcmp(m, "own")) {
        c[BTC_OWN] += 4;
        sim_tm.v.own[sim_tc.v.up ? 2 : 0] += 4;
        return;
    }
    if (!strcmp(m, "failhang") && sim_tc.halves == 50u) {
        sim_tc.v.up = 0, sim_tc.v.failed = TCR_HANG, c[BTC_OWN] += 4;
        sim_tm.v.own[0] += 4;
        return;
    }
    if (!strcmp(m, "late"))
        c[BTC_INTIME] += 3, c[BTC_LATE] += 1, jobs = 3;
    else if (!strcmp(m, "vlate"))
        c[BTC_INTIME] += 3, c[BTC_VLATE] += 1, c[BTC_VLOK] += 1, jobs = 3, vl = 1;
    else if (!strcmp(m, "vlaterun") || !strcmp(m, "vlatepause"))
        c[BTC_INTIME] += 3, c[BTC_VLATE] += 1, c[BTC_PAUSES] += 1, c[BTC_PRUN] += 1, c[BTC_RESUMES] += 1, jobs = 3, vl = 1;
    else if (!strcmp(m, "silent"))
        c[BTC_INTIME] += 3, c[BTC_VLATE] += 1, c[BTC_SILENT] += 1, c[BTC_PAUSES] += 1, c[BTC_PFAULT] += 1, jobs = 3;
    else if (!strcmp(m, "floor"))
        c[BTC_INTIME] += 3, c[BTC_OWN] += 1, sim_tm.v.own[4] += 1, jobs = 3;
    else
        c[BTC_INTIME] += 4, jobs = 4;
    for (i = 0; i < jobs; i++)
        sim_tm_job(0);
    if (vl)
        sim_tm_job(1);
}

#define BENCH_NA 88u
static uint8_t host_erase_row_of(void) { return bench.on ? (uint8_t)(bench.row + 1u) : 0u; }   /* (1-based; 0: not running) */                                   /* (BENCH_NA: the automatic rows; the STRESS row after them) */


/* ---- the simulated audio interrupt and main loop */
#define FULL_HALF 420000.0                             /* host instructions = one half = 100 % (tests/cpu_bench.c) */
static uint64_t ovh;
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}
static int have_counter;
static int32_t ob[2 * CTL];
/* the rendered audio, hashed (FNV-1a over the 32-bit samples) and measured: a test's output is compared with its second run (the reset list) */
static struct { uint32_t h, n, nz; int32_t pk; int on; int32_t *buf; uint32_t cap; } sink;   /* buf: the samples are kept too (cap of them) */
static void sink_add(const int32_t *o, uint32_t n)
{
    uint32_t i;
    if (!sink.on)
        return;
    for (i = 0; i < 2u * n; i++) {
        sink.h = (sink.h ^ (uint32_t)o[i]) * 16777619u;
        if (sink.buf && sink.n + i < sink.cap)
            sink.buf[sink.n + i] = o[i];
        sink.nz += o[i] != 0;
        if (o[i] > sink.pk)
            sink.pk = o[i];
        if (-o[i] > sink.pk)
            sink.pk = -o[i];
    }
    sink.n += 2u * n;
}
/* ---- --serve: the twin's own USB audio recording (bench suite stage 2): "@rec PATH" starts a 16-bit stereo 44.1 kHz WAV of every block the twin renders
 * (what usb.c's uac_tap would send: clipped to 16 bits, no dither), "@recstop" closes it. The runner's recorder for the twin (tools/bench_record.py) */
static FILE *rec_f;
static uint32_t rec_frames;
static void rec_head(uint32_t frames)
{
    uint32_t d = frames * 4u, v;
    fseek(rec_f, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, rec_f);
    v = 36u + d; fwrite(&v, 4, 1, rec_f);
    fwrite("WAVEfmt ", 1, 8, rec_f);
    v = 16u; fwrite(&v, 4, 1, rec_f);
    { uint16_t w = 1; fwrite(&w, 2, 1, rec_f); w = 2; fwrite(&w, 2, 1, rec_f); }
    v = 44100u; fwrite(&v, 4, 1, rec_f);
    v = 44100u * 4u; fwrite(&v, 4, 1, rec_f);
    { uint16_t w = 4; fwrite(&w, 2, 1, rec_f); w = 16; fwrite(&w, 2, 1, rec_f); }
    fwrite("data", 1, 4, rec_f);
    fwrite(&d, 4, 1, rec_f);
}
static void rec_open(const char *path)
{
    if (rec_f)
        fclose(rec_f);
    rec_f = fopen(path, "wb");
    rec_frames = 0;
    if (rec_f)
        rec_head(0);
}
static void rec_close(void)
{
    if (!rec_f)
        return;
    rec_head(rec_frames);
    fclose(rec_f);
    rec_f = 0;
}
static void rec_add(const int32_t *o, uint32_t n)
{
    uint32_t i;
    int16_t b[2 * 64];
    if (!rec_f || n > 64u)
        return;
    for (i = 0; i < 2u * n; i++)
        b[i] = (int16_t)(o[i] > 32767 ? 32767 : o[i] < -32768 ? -32768 : o[i]);
    fwrite(b, 2, 2u * n, rec_f);
    rec_frames += n;
}
static uint32_t sim_us;                                /* simulated time, us (fm1_ms follows it) */
static uint32_t next_half_us;
static void sim_half(void)
{
    uint64_t a = have_counter ? instr_now() : 0, n;
    uint32_t b, us;
    int rec = bench_tl_on;
    if (bench_tl_on)
        bench_half();                                  /* (audio.c's call, bench builds: the timeline player, before the half) */
    for (b = 0; b < HALF_FRAMES; b += CTL) {
        mix_block(ob, CTL);
        if (rec)
            sink_add(ob, CTL);                         /* (the halves of a timeline test only) */
        rec_add(ob, CTL);
    }
    if (have_counter) {
        n = instr_now() - a - ovh;
        us = (uint32_t)((double)n * CPU_BUDGET_US / FULL_HALF + 0.5);
    } else
        us = CPU_BUDGET_US * (16u + 10u * bench_voices()) / 100u;   /* (no counter: audio.c's emulator estimate) */
    cpu_sum += us;
    cpu_n++;
    if (us > cpu_pk)
        cpu_pk = us;
    cpu_miss += us > CPU_BUDGET_US;
    audio_halves++;
    sim_tc_half();
}
static void sim_frame(void)                            /* 16 ms: the halves due, then a frame */
{
    uint32_t end = sim_us + 16000u;
    while (next_half_us < end) {
        sim_half();
        next_half_us += CPU_BUDGET_US;
    }
    sim_us = end;
    frame();                                           /* (ui_test.c: fm1_ms += 16, ui_input, the WAVE builds, vcap, ui_draw) */
    budget_poll();                                     /* (main.c's loop: the NOTES budget after the VOICE CAP) */
    settings_poll();                                   /* (main.c's loop: the settings' and the autosave's writes, as on the unit) */
    autosave_poll();
}
static void sim_power_on(void)
{
    uint32_t p;
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    for (p = 0; p < NTRK; p++)
        memset(fm6_patch[p], 0, sizeof fm6_patch[p]);
    flash_ok = 1;
    multi_boot();
    frame();
    song.master_q12 = 2048;
    sim_us = next_half_us = 0;
}
static void sim_knob(uint32_t role, int32_t s) { host_enc[panel.enc[role]] += s * panel.dir[role]; }
static uint32_t host_text_w(const char *s) { return (uint32_t)text_w(&AF_S, s); }
static bench_res_t main_res[BENCH_NSCN];              /* (the whole run's rows: the model header is written from them) */

/* ---- the runs */
static void open_bench(void)
{
    ui.menu = 1;
    ui.menu_sel = MI_GUIDE;
    sim_knob(EN_K1, 1);                                    /* (a knob right on the BENCH row opens it) */
    sim_frame();
}
static void press_oct(uint32_t up);
static int open_row(int oct)                           /* from the menu's tabs, the BENCH row (the GUIDE's place) opened as the owner does: K1 right or OCT+ */
{
    uint32_t i;
    ui.menu = 1;
    ui.guide = 0;
    ui.force = 1;
    sim_frame();
    ui.menu_sel = MI_GUIDE;
    sim_frame();
    if (oct)
        press_oct(1);
    else {
        sim_knob(EN_K1, 1);
        sim_frame();
    }
    for (i = 0; i < 3u; i++)
        sim_frame();
    return ui.menu == 5u && !ui.guide;
}
static void press_oct(uint32_t up)
{
    fm1_in.buttons |= 1u << panel.btn[up ? B_OCTUP : B_OCTDN];
    host_pressed |= 1u << panel.btn[up ? B_OCTUP : B_OCTDN];
    sim_frame();
    fm1_in.buttons &= ~(1u << panel.btn[up ? B_OCTUP : B_OCTDN]);
    sim_frame();
}
static uint32_t run_until_done(uint32_t max_s)
{
    uint32_t t0 = fm1_ms;
    while (bench.on && fm1_ms - t0 < max_s * 1000u)
        sim_frame();
    return (fm1_ms - t0) / 1000u;
}
static uint32_t log_lines(const char *prefix, uint32_t *fields_bad, uint32_t want_fields)
{
    uint32_t n = 0, i = 0;
    while (i < host_log_n) {
        uint32_t j = i, commas = 0;
        while (j < host_log_n && host_log[j] != '\n')
            commas += host_log[j++] == ',';
        if (!strncmp(host_log + i, prefix, strlen(prefix)) && (strcmp(prefix, "BENCH,") || (host_log[i + 6u] >= '0' && host_log[i + 6u] <= '9'))) {
            n++;
            if (want_fields && commas + 1u != want_fields) {
                (*fields_bad)++;
                printf("bench_sim: %u fields: %.*s", commas + 1u, (int)(j - i + 1u), host_log + i);
            }
        }
        i = j + 1u;
    }
    return n;
}

/* the dearest factory sound of each engine (as tests/cpu_bench.c: one held note of each sound against the same sound silent, STEREO, no stage,
 * no send), host model */
static double row_avg(uint32_t i) { return bench_res[i].avg / 10.0; }
static uint64_t host_cost(const bench_scn_t *s)        /* instructions of 200 blocks of the scene, settled */
{
    uint64_t sum = 0, a;
    uint32_t h;
    transport_req = 4;
    for (h = 0; h < 40u; h++)
        mix_block(ob, CTL);
    bench_set(s);
    for (h = 0; h < 30u; h++)
        sim_frame();
    bench_notes_on(s);
    for (h = 0; h < 25u; h++)
        sim_frame();
    for (h = 0; h < 200u; h++) {
        a = instr_now();
        mix_block(ob, CTL);
        sum += instr_now() - a;
    }
    bench_notes_off();
    return sum;
}
static void find_dearest(void)
{
    static const uint8_t ENGS[10] = {0, 12, 2, 3, 5, 6, 7, 9, 11, 14};
    uint32_t t, k, best;
    double mx, v;
    bench_scn_t s;
    for (t = 0; t < 10u; t++) {
        uint32_t e = ENGS[t];
        mx = -1e9, best = 0;
        for (k = 0; k < ds_count(e); k++) {
            memset(&s, 0, sizeof s);
            s.mode = DM_STEREO, s.e[0] = (uint8_t)(e + 1u);
            bench_dear[e] = (uint8_t)k;
            v = -(double)host_cost(&s);
            s.n[0] = 1;
            v += (double)host_cost(&s);
            if (v > mx + (mx > 0 ? mx * 0.02 : 0))        /* (2 % ahead to win: near-ties keep the first, the choice stays put) */
                mx = v, best = k;
        }
        bench_dear[e] = (uint8_t)best;
        printf("bench_sim: dearest %s: #%u %s (%.1f %% a voice)\n", ENGINES[e]->name, best, ds_name(e, best), mx / (200.0 * CTL) * 100.0 / 3281.25);
    }
}
static int write_model(const char *path)
{
    FILE *f = fopen(path, "w");
    uint32_t i;
    if (!f)
        return 1;
    fprintf(f, "/* SPDX-License-Identifier: GPL-3.0-only */\n"
               "/* GENERATED by tests/bench_sim.c (BENCH_MODEL_UPDATE=1 build/host/bench_sim; host_tests.sh benchsim checks it is current): the host\n"
               " * model's figure of every BENCH row (src/bench.c BENCH_SCN), the row's average load in %% x10 as the bench measures it on the Mac\n"
               " * (instruction counts, -O1 -fno-builtin, 420 k instructions = one half = 100 %%: tests/cpu_bench.c's scale), and the dearest factory\n"
               " * sound of each engine (one held note, measured the same way). The automatic rows; STRESS 10M has its scene's figure. */\n");
    fprintf(f, "#define BENCH_MODEL_N %uu\n#define BENCH_MODEL_SIG 0x%08Xu\n#define BENCH_MODEL_NA %uu\n#define BENCH_MODEL_SIGA 0x%08Xu\n",
            (unsigned)BENCH_NSCN, bench_sig_n(BENCH_NSCN), (unsigned)BENCH_NA, bench_sig_n(BENCH_NA));
    fprintf(f, "static const uint8_t BENCH_DEAR[NENGINES] BENCH_RO = {");
    for (i = 0; i < NENGINES; i++)
        fprintf(f, "%s%u", i ? ", " : "", bench_dear[i]);
    fprintf(f, "};\nstatic const uint16_t BENCH_MODEL_AVG[%u] BENCH_RO = {\n", (unsigned)BENCH_NSCN);
    for (i = 0; i < BENCH_NSCN; i++) {
        uint32_t v = (bench_res[i].flags & RF_DONE) ? bench_res[i].avg : bench_refi[i] != BR_NONE ? bench_res[bench_refi[i]].avg : 0u;   /* (STRESS: its scene's) */
        fprintf(f, "    %u,%s/* %s */\n", v, v < 100u ? "  " : v < 1000u ? " " : "", BENCH_SCN[i].name);
    }
    fprintf(f, "};\n");
    fclose(f);
    return 0;
}


/* ---------------------------------------------------------------------------------------- bench suite stage 1: the console words and the timeline */
static const char *con(const char *line)             /* one console word ("info", "ev 0 on 1 60 90" ..): the reply; every line must fit the console's 64 characters */
{
    host_log_n = 0;
    host_log[0] = 0;
    if (strlen(line) + 6u >= 64u)
        printf("bench_sim: the word 'bench %s' is %u characters (the console takes 63)\n", line, (unsigned)strlen(line) + 6u);
    bench_con(line);
    return host_log;
}
static int is_ok(const char *line) { return !strncmp(con(line), "OK", 2); }
static int is_err(const char *line) { return !strncmp(con(line), "ERR", 3); }
static void adv(uint32_t ms)
{
    uint32_t t = fm1_ms;
    while (fm1_ms - t < ms)
        sim_frame();
}
static uint32_t csv_fields(const char *p)
{
    uint32_t n = 1;
    for (; *p && *p != '\r' && *p != '\n'; p++)
        n += *p == ',';
    return n;
}
/* a timeline test run to its end: the output hashed (sink), the result line left in host_log; returns the hash */
static uint32_t tl_run(const char *name, uint32_t ms, int32_t *keep, uint32_t keep_n)
{
    char b[64];
    uint32_t t0 = fm1_ms;
    sink.on = 1, sink.h = 2166136261u, sink.n = sink.nz = 0, sink.pk = 0, sink.buf = keep, sink.cap = keep_n;
    snprintf(b, sizeof b, "run %s %u", name, ms);
    if (!is_ok(b))
        return 0;
    host_log_n = 0;
    while (btl.state != TS_DONE && btl.state != TS_STOP && fm1_ms - t0 < ms + 5000u)
        sim_frame();
    adv(50);                                           /* (the result line is printed by the main loop) */
    sink.on = 0;
    sink.buf = 0;
    return sink.h;
}
static void tl_setup(const char *mode, const char *load)   /* a fresh scene: reset, the mode, a load, the settle */
{
    char b[64];
    (void)con("reset");
    adv(300);
    snprintf(b, sizeof b, "mode %s", mode);
    (void)con(b);
    if (load && *load)
        (void)con(load);
    adv(1500);
}
/* the repeat check: tests played in one order and again in the reverse order must give the same samples. scene(i) sets test i up (and its events) */
static uint32_t repeat_hash[300];
static int repeat_sweep(const char *what, uint32_t n, void (*scene)(uint32_t i), uint32_t ms, int quiet)
{
    uint32_t i, diff = 0, first[300];
    for (i = 0; i < n; i++) {
        scene(i);
        first[i] = tl_run("R", ms, 0, 0);
    }
    for (i = n; i-- > 0;) {
        scene(i);
        repeat_hash[i] = tl_run("R", ms, 0, 0);
        if (repeat_hash[i] != first[i]) {
            diff++;
            if (!quiet && diff <= 5u)
                printf("bench_sim: %s %u does not repeat\n", what, i);
        }
    }
    printf("bench_sim: %s: %u of %u tests repeat sample for sample (played forward, then backward)%s\n", what, n - diff, n, quiet ? " (the reset switched off: expected to fail)" : "");
    return (int)diff;
}
static void scn_patch(uint32_t i)                      /* patch #i+1 on part 1, STEREO: a chord, aftertouch, release (MORPH is worked off in the main loop: not sample exact) */
{
    char b[64];
    snprintf(b, sizeof b, "load patch 1 %u", i + 1u);
    tl_setup("STEREO", b);
    (void)con("ev 0 on 1 48 90"), (void)con("ev 0 on 1 52 100"), (void)con("ev 0 on 1 55 100");
    (void)con("ev 800 ramp 1 AT 0 127 800");
    (void)con("ev 1800 off 1 48"), (void)con("ev 1800 off 1 52"), (void)con("ev 1800 off 1 55");
}
static void scn_multi(uint32_t i)                      /* multi M(i+1), LAYER, a chord with the mod wheel and aftertouch moving */
{
    char b[64];
    snprintf(b, sizeof b, "load multi %u", i + 1u);
    tl_setup("LAYER", b);
    (void)con("ev 0 on 1 48 90"), (void)con("ev 0 on 1 55 100"), (void)con("ev 0 on 2 60 100");
    (void)con("ev 600 ramp 1 AT 0 127 800");
    (void)con("ev 1500 off 1 48"), (void)con("ev 1500 off 1 55"), (void)con("ev 1500 off 2 60");
}
static void scn_arp(uint32_t i)                        /* the arpeggiator on part 1 (MODE UP .. SEQ), a chord held: its step clock runs from the test's start */
{
    char b[64];
    tl_setup("STEREO", "load patch 1 3");
    snprintf(b, sizeof b, "set 1 MODE %u", i + 1u);
    (void)con(b);
    (void)con("set 1 RATE 1"), (void)con("set 1 AHOLD 0");
    adv(300);
    (void)con("ev 0 on 1 48 90"), (void)con("ev 0 on 1 52 100"), (void)con("ev 0 on 1 55 100"), (void)con("ev 0 on 1 59 100");
    (void)con("ev 1800 off 1 48"), (void)con("ev 1800 off 1 52"), (void)con("ev 1800 off 1 55"), (void)con("ev 1800 off 1 59");
}
static void scn_row(uint32_t i)                        /* a row of the bench list as it plays: its scene and notes */
{
    char b[96];
    const bench_scn_t *s = &BENCH_SCN[i];
    uint32_t p, k;
    (void)con("reset");
    adv(300);
    snprintf(b, sizeof b, "scene %s", s->name);
    (void)con(b);
    adv(1500);
    for (p = 0; p < 2u; p++) {
        if (!s->e[p] || (s->mode == DM_LAYER && p == 1u))
            continue;
        for (k = 0; k < s->n[p] && k < 8u; k++) {
            snprintf(b, sizeof b, "ev 0 on %u %u 64", p + 1u, BENCH_N8[k] + 7u * p);
            (void)con(b);
            snprintf(b, sizeof b, "ev 1500 off %u %u", p + 1u, BENCH_N8[k] + 7u * p);
            (void)con(b);
        }
    }
    if (s->m16) {
        static const uint8_t K[3] = {60, 64, 67};
        for (k = 0; k < s->n[0] && k < 3u; k++) {
            snprintf(b, sizeof b, "ev 0 on 1 %u 64", K[k]);
            (void)con(b);
            snprintf(b, sizeof b, "ev 1500 off 1 %u", K[k]);
            (void)con(b);
        }
    }
}
static int stage1_checks(void)
{
    int bad = 0;
    uint32_t i;
    char b[96];
    static int32_t cap[2 * 128 * 700];
    /* 8a the words */
    {
        const char *r;
        project_t before, after;
        trk[0].p[P_LEVEL] = 77;
        trk[1].p[P_PAN] = 30;
        duell_mode_req = DM_STEREO;
        adv(500);
        project_capture(&before);
        { uint32_t i; for (i = 0; i < 256u; i++) host_c0u[i] = C0_STK_FILL; for (i = 0; i < 64u; i++) host_c0s[i] = C0_STK_FILL; }
        host_c0u[256 - 10] = 1; host_c0u[256 - 3] = 2; host_c0s[64 - 5] = 3;   /* the user stack has been 10 words (40 B) deep, the exception stack 5 words (20 B) */
        r = con("info");
        bad += check("8 `bench info`: the INFO line (version, tag, RAM code, restarts) and the STATE line", strstr(r, "BENCH,INFO,") && strstr(r, "ramcode -,boots") && strstr(r, "BENCH,STATE,sess 0,run 0,evfree 256"));
        bad += check("8 `bench info`: core 0's stack marks c0stack user/exception in bytes (developer builds)", strstr(r, ",c0stack 40/20,") != 0);
        bad += check("8 the words need a session first (bench reset), the session starts with it",
                     is_err("load multi 7") && is_err("ev 0 on 1 60 90") && is_ok("reset") && btl.sess && bench.holds);
        bad += check("8 reset: STEREO, both parts from INIT, the NOTES budget as played", duell_mode_req == DM_STEREO && !bench_bopen && trk[0].p[P_LEVEL] != 77);
        r = con("load multi 7");
        bad += check("8 load multi 7: 'OK multi 7 FINAL REEL' (the runner checks the name)", !strcmp(r, "OK multi 7 FINAL REEL\r\n"));
        r = con("load patch 1 61");
        bad += check("8 load patch 1 61: 'OK patch 61 EVOLVING PAD'", !strcmp(r, "OK patch 61 EVOLVING PAD\r\n"));
        bad += check("8 load: no such multi / part / slot is ERR", is_err("load multi 17") && is_err("load multi 0") && is_err("load patch 3 1") && is_err("load patch 1 129") && is_err("load foo"));
        bad += check("8 mode: DUEL STEREO LAYER, others ERR", is_ok("mode LAYER") && duell_mode_req == DM_LAYER && is_ok("mode duel") && duell_mode_req == DM_SPLIT && is_ok("mode STEREO") && is_err("mode MONO") && is_err("mode"));
        bad += check("8 set: a part's value by label, by enum name, clamped; a global value; wrong names, parts and values are ERR",
                     is_ok("set 1 SRC8 LFO2") && trk[0].p[P_M8SRC] == 12 && is_ok("set 1 DST8 CUT") && trk[0].p[P_M8DST] == 2 && is_ok("set 1 AMT8 63") && trk[0].p[P_M8AMT] == 63 &&
                     is_ok("set 1 AMT8 99") && trk[0].p[P_M8AMT] == 63 && is_ok("set 1 AMT8 -64") && trk[0].p[P_M8AMT] == -64 && is_ok("set 2 WAVE2 S&H") && trk[1].p[P_L2WAVE] == 4 &&
                     is_ok("set g TEMPO 100") && song.g[G_BPM] == 100 && is_ok(strcat(strcpy(b, "set 1 "), "9 5")) && trk[0].p[9] == 5 &&
                     is_err("set 3 CUT 1") && is_err("set 1 NOSUCH 1") && is_err("set 1 SRC8 NOSUCH") && is_err("set g NOSUCH 1") && is_err("set 1 CUT"));
        bad += check("8 scene: a row of the bench list (the budget held open); budget ON / OPEN; a wrong row is ERR",
                     is_ok("scene HVY DUEL 8+8") && bench_bopen && duell_mode_req == DM_SPLIT && is_ok("budget ON") && !bench_bopen && is_ok("budget OPEN") && bench_bopen && is_err("scene NO SUCH ROW") && is_err("budget MAYBE"));
        bad += check("8 ev: wrong channel, velocity, note, kind, order are ERR; clear empties", is_ok("ev 1000 on 1 60 90") && is_err("ev 500 on 1 60 90") && is_err("ev 2000 on 17 60 90") &&
                     is_err("ev 2000 on 1 60 0") && is_err("ev 2000 on 1 128 90") && is_err("ev 2000 foo 1") && is_err("ev 2000 ramp 1 20 0 127 0") && is_err("ev x") && is_ok("ev clear") && btl.w == btl.r);
        bad += check("8 measure, run: a window must be forward, a test at least 100 ms; no second run while one runs",
                     is_err("measure 500 500") && is_ok("measure 500 1500") && is_err("run T 50") && is_ok("run T 3000") && is_err("run U 3000") && is_ok("stop"));
        adv(100);
        bad += check("8 stop: the result line says STOPPED; the header and the row have the same number of fields (114: 31, the 16 core 1 columns, BENCHT2's 58 and BENCHT3's 9)",
                     strstr(host_log, "BT,T,STOPPED,") && csv_fields(strstr(host_log, "BTH,")) == 114u && csv_fields(strstr(host_log, "BT,T,")) == 114u);
        bad += check("8 the result line carries core 1: mode 2, why OK, then the counters",
                     strstr(host_log, "BT,T,STOPPED,") && strstr(strstr(host_log, "BT,T,"), ",2,OK,"));
        {   /* BENCHT2: the runtime switches and the timing words */
            const char *inf;
            bad += check("8 BENCHT2 slack, calm, lrun, vrun: a number sets it and `info` says so; default puts it back",
                         !strcmp(con("slack 10443"), "OK slack 10443\r\n") && !strcmp(con("calm 2500"), "OK calm 2500\r\n") && !strcmp(con("lrun 7"), "OK lrun 7\r\n") && !strcmp(con("vrun 3"), "OK vrun 3\r\n") &&
                         (inf = con("info"), strstr(inf, ",slack 10443,calm 2500,lrun 7,vrun 3,vwin 0/2000,floor 0\r\n")) &&
                         is_ok("slack default") && is_ok("calm def") && is_ok("lrun default") && is_ok("vrun default") && (inf = con("info"), strstr(inf, ",slack 3481,calm 10000,lrun 4,vrun 0,vwin 0/2000,floor 0\r\n")));
            bad += check("8 BENCHT2 the limits: a negative, too large or missing number, a word that is not a number, lrun 0 are ERR and change nothing; vrun 0 is allowed (BENCHT4: off)",
                         is_err("slack -1") && is_err("slack 400001") && is_err("slack") && is_err("slack x") && is_err("calm 80001") && is_err("calm") && is_err("lrun 0") && is_err("vrun 1001") && is_err("vrun") && is_err("vrun -1") && is_ok("vrun 0") &&
                         is_ok("slack 0") && is_ok("slack 400000") && is_ok("calm 0") && is_ok("calm 80000") && is_ok("lrun 1000") && (inf = con("info"), strstr(inf, ",slack 400000,calm 80000,lrun 1000,vrun 0,vwin 0/2000,floor 0\r\n")));
            bad += check("8 BENCHT2 pause off: the late run huge, the very late run and window off (nothing pauses the jobs but a real fault); pause on: the defaults; a wrong word is ERR",
                         is_ok("vrun 4") && is_ok("vwin 8") && is_ok("pause off") && (inf = con("info"), strstr(inf, ",lrun 1000,vrun 0,vwin 0/2000,floor 0\r\n")) && is_ok("pause on") && (inf = con("info"), strstr(inf, ",lrun 4,vrun 0,vwin 0/2000,floor 0\r\n")) &&
                         is_err("pause maybe") && is_err("pause"));
            bad += check("8 BENCHT2 reset puts the settings back; `tcm` answers with its header line",
                         is_ok("slack 99") && is_ok("pause off") && is_ok("reset") && (inf = con("info"), strstr(inf, ",slack 3481,calm 10000,lrun 4,vrun 0,vwin 0/2000,floor 0\r\n")) &&
                         strstr(con("tcm"), "BENCH,TCM,very late jobs 0,shown 0"));
        }
        {   /* BENCHT3: the very late window and the floor */
            const char *inf;
            bad += check("8 BENCHT3 vwin K [MS] and floor: they set, `info` says so (BENCHT4: vwin 0/2000 floor 0 at the start), default puts them back, K 0 is off, pause off turns the window off, reset puts them back",
                         (inf = con("info"), strstr(inf, ",vwin 0/2000,floor 0\r\n")) && !strcmp(con("vwin 5 1500"), "OK vwin 5\r\n") && (inf = con("info"), strstr(inf, ",vwin 5/1500,floor 0\r\n")) &&
                         is_ok("vwin 3") && (inf = con("info"), strstr(inf, ",vwin 3/1500,floor 0\r\n")) && is_ok("vwin 0") && (inf = con("info"), strstr(inf, ",vwin 0/1500,")) &&
                         !strcmp(con("floor 1"), "OK floor 1\r\n") && (inf = con("info"), strstr(inf, ",floor 1\r\n")) && is_ok("floor 0") && is_ok("vwin 8 2000") && (inf = con("info"), strstr(inf, ",vwin 8/2000,floor 0\r\n")) &&
                         is_ok("vwin default") && is_ok("floor 1") && is_ok("floor default") && (inf = con("info"), strstr(inf, ",vwin 0/2000,floor 0\r\n")) && is_ok("vwin 8") && is_ok("pause off") && (inf = con("info"), strstr(inf, ",vwin 0/2000,floor 0\r\n")) && is_ok("reset") &&
                         (inf = con("info"), strstr(inf, ",vrun 0,vwin 0/2000,floor 0\r\n")));
            bad += check("8 BENCHT3 the limits: vwin K above 16 or below 0, MS 0 or above 60000, a missing or non-number, floor 2, floor with no value are ERR and change nothing",
                         is_err("vwin 17") && is_err("vwin -1") && is_err("vwin 5 0") && is_err("vwin 5 60001") && is_err("vwin") && is_err("vwin x") && is_err("vwin 5 y") && is_err("floor 2") && is_err("floor") && is_err("floor x") &&
                         (inf = con("info"), strstr(inf, ",vwin 0/2000,floor 0\r\n")) && is_ok("vwin 16 60000") && (inf = con("info"), strstr(inf, ",vwin 16/60000,floor 0\r\n")) && is_ok("reset"));
        }
        bad += check("8 end: the owner's session put back (both parts' values, the mode); the keys sound again",
                     is_ok("end") && !btl.sess && !bench.holds && !kb_boot_hold && (project_capture(&after), after.t[0].p[P_LEVEL] == 77 && after.t[1].p[P_PAN] == 30 && duell_mode_req == DM_STEREO));
        bad += check("8 a normal word (status) is not touched; `bench` alone and an unknown word say so", is_err("") && is_err("frobnicate"));
    }
    /* 8b the timeline: events land at their half; the window; the ramps */
    {
        uint32_t h1000 = (1000u * 441u * 2u + 1280u) / 2560u, first_nz = 0, k;
        (void)con("reset"), (void)con("mode STEREO");
        adv(2000);
        is_ok("ev 1000 on 1 60 100");
        is_ok("ev 2000 off 1 60");
        is_ok("measure 500 1500");
        (void)tl_run("EDGE", 3000, cap, sizeof cap / sizeof cap[0]);
        for (k = 0; k < sink.n; k++)
            if (cap[k]) {
                first_nz = k / 2u;
                break;
            }
        printf("bench_sim: a note at 1000 ms: half %u (frame %u), the first sample that is not 0: frame %u\n", h1000, h1000 * 128u, first_nz);
        bad += check("8 an event plays at the half it falls in: silence before it, sound within two blocks of the half's start", first_nz >= h1000 * 128u && first_nz < h1000 * 128u + 64u);
        bad += check("8 the window: 'halves' is the window's length (500..1500 ms = 345 halves)", strstr(host_log, "BT,EDGE,DONE,") && strstr(host_log, ",345,") != 0);
        (void)con("reset"), (void)con("mode STEREO");
        adv(1500);
        is_ok("ev 100 ramp 1 AT 0 127 1000");
        is_ok("ev 1100 ramp 1 20 100 0 400");
        is_ok("ev 1500 flash");
        {
            uint32_t t0, mono = 1, prev = 0, mid = 0, wr;
            uint8_t rec[1024], rec2[1024];
            int n1, n2;
            bad += check("8 save-test: a settings record is written (the settings as they stand when the flash holds none)", is_ok("save-test"));
            n1 = st_load(OBJ_SETTINGS, rec, sizeof rec);
            wr = nor_writes;
            bad += check("8 ..again: the record in the flash written back (the other copy), nothing else written", is_ok("save-test") && nor_writes > wr && nor_writes - wr < 8u);
            n2 = st_load(OBJ_SETTINGS, rec2, sizeof rec2);
            bad += check("8 ..and it reads back unchanged", n1 > 0 && n1 == n2 && !memcmp(rec, rec2, (size_t)n1));
            wr = nor_writes;
            is_ok("run RAMP 2500");
            t0 = fm1_ms;
            while (btl.state != TS_DONE && fm1_ms - t0 < 5000u) {
                sim_frame();
                if (btl.state == TS_RUN) {
                    uint32_t at = trk[0].at;
                    if (at < prev)
                        mono = 0;
                    prev = at;
                    if (btl.h >= 207u && !mid)
                        mid = at;                       /* (600 ms in: half way up) */
                }
            }
            adv(60);
            printf("bench_sim: aftertouch ramp: half way (600 ms) %u, at the end %u, steps up only %u\n", mid, prev, mono);
            bad += check("8 a ramp: aftertouch 0 to 127 over 1 s, steps up only, half way at 600 ms, 127 at the end", mono && prev == 127u && mid > 50u && mid < 76u);
            bad += check("8 ..the flash event of the timeline wrote once (flashes 1 in the result line)", strstr(host_log, "BT,RAMP,DONE,") && strstr(host_log, ",0,1,44100,") && nor_writes > wr);
        }
    }
    /* 8b2 MORPH is CC20: heard (the runner's `morph` word), worked off in the main loop (so not sample exact: the design's `expect close`) */
    {
        uint32_t hn, hm, hm2, pass;
        for (pass = 0; pass < 3u; pass++) {
            tl_setup("STEREO", "load patch 1 61");
            (void)con("ev 0 on 1 48 90"), (void)con("ev 0 on 1 55 100");
            if (pass)
                (void)con("ev 300 ramp 1 20 0 127 1500");
            (void)con("ev 1800 off 1 48"), (void)con("ev 1800 off 1 55");
            if (pass == 2u)
                adv(137);                               /* (another place on the 16 ms frame grid) */
            {
                uint32_t h = tl_run("MORPH", 2500, 0, 0);
                if (pass == 0u)
                    hn = h;
                else if (pass == 1u)
                    hm = h;
                else
                    hm2 = h;
            }
        }
        printf("bench_sim: #61 EVOLVING PAD: without MORPH %08x, with %08x, again on another frame phase %08x\n", hn, hm, hm2);
        bad += check("8 a CC20 ramp moves the MORPH fader (the sound differs from the same test without it)", hn != hm);
        (void)con("end");
    }
    /* 8c the streaming: more events than the ring holds, sent while the test runs */
    {
        uint32_t sent = 0, t0, full_seen = 0;
        (void)con("reset");
        adv(300);
        for (i = 0; i < 400u; i++) {
            snprintf(b, sizeof b, "ev %u cc 1 11 %u", i * 50u, i % 100u);
            if (!is_ok(b)) {
                full_seen = !strncmp(host_log, "ERR full", 8);
                break;
            }
            sent++;
        }
        bad += check("8 the ring holds 256 events; the 257th is 'ERR full'", sent == 256u && full_seen);
        is_ok("run STREAM 22000");
        t0 = fm1_ms;
        while (btl.state != TS_DONE && fm1_ms - t0 < 30000u) {
            sim_frame();
            while (sent < 400u) {
                snprintf(b, sizeof b, "ev %u cc 1 11 %u", sent * 50u, sent % 100u);
                if (!is_ok(b))
                    break;
                sent++;
            }
        }
        adv(60);
        bad += check("8 ..the rest sent while it runs: all 400 played, none lost, none refused by the MIDI-in ring", sent == 400u && btl.lost == 0 && btl.w == btl.r && strstr(host_log, "BT,STREAM,DONE,"));
        (void)con("end");
    }
    /* 8d the reset list: every test repeats sample for sample, whatever ran before (forward then backward) */
    {
        int d_patch, d_multi, d_row, d_off;
        d_patch = repeat_sweep("the 128 factory patches", 128u, scn_patch, 2500u, 0);
        bad += check("8 the reset list: every factory patch repeats (a chord, aftertouch, release; played forward, then backward)", d_patch == 0);
        d_multi = repeat_sweep("the 16 factory multis", MU_SLOTS, scn_multi, 2300u, 0);
        bad += check("8 the reset list: every factory multi repeats", d_multi == 0);
        d_row = repeat_sweep("the bench rows (88)", BENCH_NAUTO, scn_row, 2300u, 0);
        bad += check("8 the reset list: every row of the bench list repeats", d_row == 0);
        {
            int d_arp = repeat_sweep("the arpeggiator modes (15)", 15u, scn_arp, 2300u, 0);
            bad += check("8 the reset list: every arpeggiator mode repeats (the step clock, the held notes)", d_arp == 0);
        }
        bench_noreset = 1;
        d_off = repeat_sweep("the 128 patches", 128u, scn_patch, 2500u, 1);
        bench_noreset = 0;
        bad += check("8 ..and the check can fail: with the reset switched off most patches do not repeat", d_off > 64);
        (void)con("end");
    }
    return bad;
}

/* ---- --serve: the unit for tools/bench_run.py (host_tests.sh benchrun): the console words of the bench over stdin / stdout, the simulated time moving only
 * when the runner says so. A line is a console line ("bench ...", "status", "crash"); "@adv MS" moves the time, "@crash" ends the process (a unit that
 * restarted: the runner starts it again with --boots N+1), "@quit". Every command is answered, then "@done" */
static void serve_flush(void)
{
    fwrite(host_log, 1, host_log_n, stdout);
    host_log_n = 0;
    host_log[0] = 0;
    fputs("@done\n", stdout);
    fflush(stdout);
}
static int serve(void)
{
    char ln[256];
    if (getenv("BENCH_SIM_TC"))
        sim_tc_set(getenv("BENCH_SIM_TC"));
    sim_power_on();
    bench_resolve();
    while (fgets(ln, sizeof ln, stdin)) {
        size_t n = strlen(ln);
        while (n && (ln[n - 1] == '\n' || ln[n - 1] == '\r'))
            ln[--n] = 0;
        if (!strncmp(ln, "@adv ", 5)) {
            uint32_t t = fm1_ms, ms = (uint32_t)atoi(ln + 5);
            while (fm1_ms - t < ms)
                sim_frame();
        } else if (!strncmp(ln, "@rec ", 5)) {
            rec_open(ln + 5);
            host_out(rec_f ? "OK rec\r\n" : "ERR rec\r\n");
        } else if (!strcmp(ln, "@recstop")) {
            char b[48];
            snprintf(b, sizeof b, "OK frames %u\r\n", rec_frames);
            rec_close();
            host_out(b);
        } else if (!strcmp(ln, "@crash")) {
            return 3;
        } else if (!strcmp(ln, "@quit")) {
            break;
        } else if (!strncmp(ln, "bench", 5) && (ln[5] == ' ' || !ln[5])) {
            if (n >= 64u)
                host_out("ERR line too long for the console (64)\r\n");
            else
                bench_con(ln + (ln[5] ? 6 : 5));
        } else if (!strcmp(ln, "status")) {
            char b[64];
            snprintf(b, sizeof b, "boots %u\r\nuptime_ms %u\r\nuac_hz 44100\r\n", host_boots, fm1_ms);
            host_out(b);
        } else if (!strcmp(ln, "crash")) {
            host_out(host_boots > 3u ? "count 1\r\nvec 0x00000007\r\npc 0x02012345\r\n" : "no crash record\r\n");
        } else
            host_out("? (help)\r\n");
        serve_flush();
    }
    return 0;
}

int main(int argc, char **argv)
{
    int bad = 0, update = getenv("BENCH_MODEL_UPDATE") != 0;
    uint32_t i, j, n, done, fb = 0, secs;
    char b[220];
    setvbuf(stdout, NULL, _IONBF, 0);
    have_counter = instr_now() != 0;
    if (argc > 1 && !strcmp(argv[1], "--serve")) {
        if (argc > 3 && !strcmp(argv[2], "--boots"))
            host_boots = (uint32_t)atoi(argv[3]);
        have_counter = 0;                               /* (the figures are an estimate here: deterministic) */
        return serve();
    }
    if (have_counter) {
        uint64_t a0, a1;
        ovh = ~0ull;
        for (i = 0; i < 20u; i++) { a0 = instr_now(); a1 = instr_now(); if (a1 - a0 < ovh) ovh = a1 - a0; }
    }
    sim_power_on();
    bench_resolve();

    /* 1 the list */
    {
        uint32_t dup = 0, wide = 0, refs = 0, bases = 0;
        FILE *f = fopen("tests/bench_baseline.txt", "r");
        static char bl[8192];
        size_t bn = f ? fread(bl, 1, sizeof bl - 1u, f) : 0;
        if (f)
            fclose(f);
        bl[bn] = 0;
        for (i = 0; i < BENCH_NSCN; i++) {
            for (j = i + 1u; j < BENCH_NSCN; j++)
                dup += str_eq(BENCH_SCN[i].name, BENCH_SCN[j].name);
            if (host_text_w(BENCH_SCN[i].name) > 88u) {
                printf("bench_sim: %s is %u px wide\n", BENCH_SCN[i].name, host_text_w(BENCH_SCN[i].name));
                wide++;
            }
            refs += BENCH_SCN[i].ref && bench_refi[i] == BR_NONE;
            if (BENCH_SCN[i].base) {
                snprintf(b, sizeof b, "\n%s ", BENCH_SCN[i].base);
                bases += !strstr(bl, b);
            }
        }
        printf("bench_sim: %u rows (%u in the automatic run), %u pages of the table\n", (unsigned)BENCH_NSCN, (unsigned)BENCH_NAUTO, bench_pages() - 1u);
        bad += check("the list: names unique, each within the name column, every ref and bench_baseline.txt line found", !dup && !wide && !refs && !bases && bn);
    }

    /* 6a the model header (and BENCH_MODEL_UPDATE: the dearest sounds first) */
    if (update) {
        if (!have_counter) {
            printf("bench_sim: BENCH_MODEL_UPDATE needs the instruction counter (macOS)\n");
            return 1;
        }
        bench_resolve();                                /* (the header's sounds first, then the measured ones over them) */
        find_dearest();
    }

    /* 2 a whole run, from the menu */
    {
        project_t before;
        uint32_t cmp0 = cmp_cfg, mode0 = duell_mode_req, w0 = nor_writes, vok, order = 1, v1, v4, v8, vl, bud_cost = 0, bud_m16 = 0;
        trk[0].p[P_LEVEL] = 77;                         /* (a session of the owner's: put back after the run) */
        trk[1].p[P_PAN] = 30;
        project_capture(&proj_scratch);                 /* (one round trip first: a restore settles FM6's SLOT, P_E7, from its packed patch) */
        (void)project_restore_runtime(&proj_scratch);
        for (i = 0; i < 20u; i++)
            sim_frame();
        project_capture(&before);
        bad += check("the BENCH row (the GUIDE's place) opened with K1 right: the BENCH screen, not the GUIDE", open_row(0));
        press_oct(0);
        bad += check("..and with OCT+: the BENCH screen, not the GUIDE", open_row(1) && !bench.on);
        ui.menu = 0;
        guide_open(0);
        bad += check("the first start's GUIDE still opens (guide_open(0))", ui.guide && ui.menu == 0u);
        ui.guide = 0;
        ui.force = 1;
        sim_frame();
        open_bench();
        bad += check("the BENCH row opens the BENCH screen", ui.menu == 5u);
        press_oct(1);
        bad += check("OCT+ starts the run", bench.on && bench.first == 0u && bench.last == BENCH_NAUTO - 1u);
        {
            uint32_t last_row = 0, t0 = fm1_ms;
            while (bench.on && fm1_ms - t0 < 1200u * 1000u) {
                if (bench.row < last_row)
                    order = 0;
                last_row = bench.row;
                if (bench.phase == BS_MEASURE) {       /* (7: the budget while a row measures: off in the cost rows, on in M16's) */
                    int given = bnotes[0] || bnotes[1];
                    if (BENCH_SCN[bench.row].grp == BG_M16)
                        bud_m16 += given;
                    else
                        bud_cost += given;
                }
                if ((fm1_ms / 16u) % 397u == 0u) {     /* (the pages drawn now and then while it runs) */
                    bench.page = (uint8_t)((bench.page + 1u) % bench_pages());
                    ui.force = 1;
                }
                sim_frame();
            }
            secs = (fm1_ms - t0) / 1000u;
        }
        for (done = 0, i = 0; i < BENCH_NAUTO; i++)
            done += (bench_res[i].flags & RF_DONE) != 0u;
        printf("bench_sim: the run took %u simulated s (%u rows measured)\n", secs, done);
        memcpy(main_res, bench_res, sizeof main_res);
        bad += check("the run: every row measured, in order, about 7 minutes", !bench.on && order && done == BENCH_NAUTO && secs > 300u && secs < 600u);
        v1 = bench_res[bench_find("ANALOG 1")].vox, v4 = bench_res[bench_find("ANALOG 4")].vox;
        v8 = bench_res[bench_find("TYP ST 8+8")].vox, vl = bench_res[bench_find("TYP LAY 4+4")].vox;
        vok = v1 >= 9u && v1 <= 11u && v4 >= 38u && v4 <= 42u && v8 <= 80u && v8 >= 60u && vl >= 70u && vl <= 80u;
        printf("bench_sim: voices ANALOG 1 %.1f, ANALOG 4 %.1f, TYP ST 8+8 %.1f (the pool: 8), TYP LAY 4+4 %.1f\n", v1 / 10.0, v4 / 10.0, v8 / 10.0, vl / 10.0);
        bad += check("the rows' voices: 1, 4, at most the pool's 8 with 8 + 8 asked for, LAYER doubles", vok);
        {
            uint32_t ia = bench_find("INS PHASR"), ra = bench_find("ANALOG 1"), ic = bench_find("IDLE ST"), dl = bench_find("DL RAND ST"), bo = bench_find("BUS OFF ST");
            int ok = bench_res[ia].avg > bench_res[ra].avg && bench_res[ra].avg > bench_res[ic].avg && bench_res[dl].avg > bench_res[bo].avg &&
                     bench_res[bench_find("HVY DUEL 8+8")].avg > bench_res[bench_find("TYP DUEL 8+8")].avg;
            printf("bench_sim: IDLE ST %.1f %%, ANALOG 1 %.1f %%, INS PHASR %.1f %%, BUS OFF ST %.1f %%, DL RAND ST %.1f %%\n", row_avg(ic), row_avg(ra), row_avg(ia), row_avg(bo), row_avg(dl));
            bad += check("the stages cost something: a voice over idle, an insert over its voice, RANDOM over SENDS OFF, heavy over typical", ok);
        }
        {                                               /* (the log as the console had it: host_tests.sh feeds it to tools/bench_log.py) */
            FILE *lf = fopen(getenv("BENCH_SIM_LOG") ? getenv("BENCH_SIM_LOG") : "build/host/bench_sim.log", "w");
            if (lf) {
                fwrite(host_log, 1, host_log_n, lf);
                fclose(lf);
            }
        }
        /* 3 the log */
        n = log_lines("BENCH,", &fb, 0);
        {
            uint32_t f2 = 0, nh = log_lines("BENCH,#", &f2, 33u), nr = log_lines("BENCH,", &f2, 33u);
            printf("bench_sim: log: %u row lines, header %u, field errors %u\n", n, nh, f2);
            (void)nr;
        }
        bad += check("the log: INFO, the header, a line per row with the header's 33 fields, END",
                     log_lines("BENCH,INFO", &fb, 0) == 1u && log_lines("BENCH,#", &fb, 33u) == 1u && log_lines("BENCH,END", &fb, 0) == 1u &&
                     n == BENCH_NAUTO && (log_lines("BENCH,", &fb, 33u), fb == 0u));
        {
            uint32_t k = 0;
            for (i = 0; i < host_log_n && k < 5u; i++)
                if (host_log[i] == '\n') {
                    k++;
                    if (k == 2u || k == 3u)
                        printf("bench_sim: log %.*s", (int)(strchr(host_log + i + 1u, '\n') - (host_log + i)), host_log + i + 1u);
                }
        }
        /* 4 nothing written, the session back */
        {
            project_t after;
            for (i = 0; i < 30u; i++)
                sim_frame();
            project_capture(&after);
            for (i = 0; i < host_erases && i < 64u; i++)
                if (host_erase_row[i])
                    printf("bench_sim: an erase at 0x%05X during row %u (%s)\n", host_erase_off[i], host_erase_row[i], BENCH_SCN[host_erase_row[i] - 1u].name);
            bad += check("nothing written to the flash during the run", nor_writes == w0);
            for (j = 0; j < 2u; j++)
                for (i = 0; i < P_COUNT; i++)
                    if (after.t[j].p[i] != before.t[j].p[i])
                        printf("bench_sim: part %u value %u: %d, was %d\n", j + 1u, i, after.t[j].p[i], before.t[j].p[i]);
            if (duell_mode_req != mode0 || cmp_cfg != cmp0 || after.t[0].engine != before.t[0].engine)
                printf("bench_sim: mode %u (was %u), cmp %08x (was %08x), engine %u (was %u)\n", duell_mode_req, mode0, cmp_cfg, cmp0, after.t[0].engine, before.t[0].engine);
            bad += check("the session put back: both parts' values, the mode, the master words",
                         !memcmp(after.t[0].p, before.t[0].p, sizeof after.t[0].p) && !memcmp(after.t[1].p, before.t[1].p, sizeof after.t[1].p) &&
                         after.t[0].engine == before.t[0].engine && duell_mode_req == mode0 && cmp_cfg == cmp0 && trk[0].p[P_LEVEL] == 77);
        }
        /* the pages, drawn once each after the run */
        for (i = 0; i < bench_pages(); i++) {
            bench.page = (uint8_t)i;
            ui.force = 1;
            sim_frame();
        }
        bad += check("every page of the table drawn (no fault)", 1);
        {
            uint32_t m3 = bench_find("M16 3 KEYS"), a4 = bench_find("ANALOG 4");
            printf("bench_sim: NOTES while measuring: cost rows given %u frames, M16 rows given %u frames; M16 3 KEYS NOTES %u + %u (%s, %s), voices %.1f; ANALOG 4 NOTES %u (%s, budget %s)\n",
                   bud_cost, bud_m16, bench_res[m3].bn[0], bench_res[m3].bn[1], BENCH_NR[bench_res[m3].bw[0] % NR_N], BENCH_NR[bench_res[m3].bw[1] % NR_N],
                   bench_res[m3].vox / 10.0, bench_res[a4].bn[0], BENCH_NR[bench_res[a4].bw[0] % NR_N], bench_res[a4].bopen ? "open" : "on");
            bad += check("7 the NOTES budget: held open while the cost rows measure, given while the M16 rows do; the rows say so (OPEN / ON)",
                         !bud_cost && bud_m16 && bench_res[a4].bopen && !bench_res[m3].bopen && bench_res[m3].bn[0] >= 1u && !bench_bopen);
            bad += check("7 the log: budget OPEN and ON rows, ramcode '-' (no RAMPLACE here), boots 3, INFO with ramcode and boots",
                         strstr(host_log, ",OPEN,-,3\r\n") && strstr(host_log, ",ON,-,3\r\n") && strstr(host_log, ",ramcode -,boots 3,c0stack 1024/256,cores 2,why OK,slack 3481,calm 10000,lrun 4,vrun 0,vwin 0/2000,floor 0\r\n"));
        }
        if (update) {
            bad += write_model("firmware/src/bench_model.h");
            printf("bench_sim: firmware/src/bench_model.h written (%u rows)\n", (unsigned)BENCH_NSCN);
        }
    }

    /* 6b the model (BENCH_MODEL_UPDATE: written after the B1 rows, below) */
    if (!update) {
        uint32_t off = 0;
        bad += check("the model header is current (rows, names)", bench.model_ok);
        if (bench.model_ok && have_counter)
            for (i = 0; i < BENCH_NAUTO; i++) {
                double m = BENCH_MODEL_AVG[i] / 10.0, v = row_avg(i);
                if (!(bench_res[i].flags & RF_DONE))
                    continue;
                if (v > m * 1.15 + 2.0 || v < m * 0.85 - 2.0) {
                    printf("bench_sim: %s: %.1f %%, the header %.1f %%\n", BENCH_SCN[i].name, v, m);
                    off++;
                }
            }
        if (have_counter)
            bad += check("every row within 15 % (and 2 points) of the model header (else BENCH_MODEL_UPDATE=1)", !off);
        else
            printf("bench_sim: no instruction counter: the figures are an estimate, not compared\n");
    }

    /* 5 OCT- stops a run */
    {
        uint32_t cmp0 = cmp_cfg, mode0 = duell_mode_req, t0 = fm1_ms;
        host_log_n = 0;
        press_oct(1);
        while (bench.on && bench.row < 2u && fm1_ms - t0 < 60000u)
            sim_frame();
        press_oct(0);
        for (i = 0; i < 20u; i++)
            sim_frame();
        bad += check("OCT- stops a run: the rest NOT RUN, STOPPED in the log, the session back",
                     !bench.on && bench.aborted && !(bench_res[5].flags & RF_DONE) && (bench_res[0].flags & RF_DONE) &&
                     log_lines("BENCH,END", &fb, 0) == 1u && strstr(host_log, "STOPPED") && duell_mode_req == mode0 && cmp_cfg == cmp0);
        press_oct(0);
        bad += check("OCT- again: back to the menu's tabs", ui.menu == 1u);
    }

    /* 7 K3: the STRESS row (10 minutes, not in the automatic list), stopped after a while with OCT- */
    {
        uint32_t st = bench_find("STRESS 10M"), t0;
        open_bench();
        sim_knob(EN_K3, 1);
        sim_frame();
        bad += check("7 K3 starts STRESS 10M alone (10 minutes, the last row, not in the automatic run)",
                     bench.on && bench.first == st && bench.last == st && st == BENCH_NSCN - 1u && BENCH_NAUTO == BENCH_NA && bench_secs(&BENCH_SCN[st]) == 600u);
        t0 = fm1_ms;
        while (bench.on && !(bench.phase == BS_MEASURE && fm1_ms - bench.t > 5000u) && fm1_ms - t0 < 30000u)
            sim_frame();
        {
            int open_now = bench_bopen && !bnotes[0] && !bnotes[1];
            press_oct(0);
            bad += check("7 ..the budget open while it measures; OCT- stops it", open_now && !bench.on && bench.aborted && !bench_bopen);
        }
    }


    /* 8 (bench suite stage 1) the console words, the timeline player, the reset list */
    bad += stage1_checks();

    printf("bench_sim: %s\n", bad ? "FAIL" : "all checks passed");
    return bad != 0;
}
