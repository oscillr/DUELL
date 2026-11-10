/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* KEY SENSE CAPTURE (DUELL_VELO_CAPTURE=1, a developer build ONLY: felucca.c stops a build without the USB serial console, build.py refuses --release). The raw logger of
 * docs/VELO_CSV.md: every converter reading of a key that is not at rest goes into a small ring from the one place all readings pass
 * (hal/fm1_input.h fm1__an_take, the scan tick), and velocap_drain (velocap.c, main loop only) sends the rows over the USB serial console.
 * This header is plain C, the tick side only: a copy of the reading into the ring, no wait, no call, no change to sense.c's behaviour.
 * Switch: vc.on, OFF at power-on, set by the console word `velocap on` / `velocap off` (so the CPU measurements run with it off).
 * Every serial line starts with `VC,` (tools/bench_run.py takes only OK / ERR / BENCH, / BT, lines and ignores them).
 * A full ring drops the row and counts it (vc.dropped); nothing ever waits.
 * Row in the ring: 4 + 2 bytes = ts_us (u32) and w = adc (bits 0..9) | drive << 10 | key << 11.
 * Which rows: a reading below SC_OPEN_MIN (900) or a driven one, the VC_PRE (2) readings before the first of a run, and the VC_POST (3)
 * readings after the last (so a landing and a release have their open readings, docs/VELO_CSV.md). Keys 0 .. 19 (no converter above). */
#pragma once
#include <stdint.h>
#define VC_N 512u                       /* rows in the ring (a power of 2): 512 x 6 B = 3,072 B */
#define VC_PRE 2u
#define VC_POST 3u
#define VC_NKEY 20u
#define VC_BARRIER() __asm__ volatile("" ::: "memory")
#ifndef VC_BSS                          /* (the host harness defines it empty) */
#define VC_BSS __attribute__((section(".bench_bss")))
#endif

static struct {
    uint32_t ts[VC_N];
    uint16_t w[VC_N];
    volatile uint32_t wr, rd;           /* rows put / taken (the tick writes wr, the main loop writes rd) */
    volatile uint8_t on;                /* the run-time switch: 0 at power-on (console word `velocap on`); OFF costs the tick one load and one branch per reading */
    uint32_t dropped, peak, sent;            /* rows dropped since the start (tick), the most rows waiting at once (tick), rows sent (main loop) */
    uint32_t pts[VC_NKEY][VC_PRE];      /* the last readings of a key at rest (to be sent when it starts to move) */
    uint16_t pw[VC_NKEY][VC_PRE];
    uint8_t pn[VC_NKEY], post[VC_NKEY];
} vc VC_BSS;

static void velocap_note(uint32_t key, uint32_t vel);
static void velocap_con(const char *p);                 /* velocap.c: the console word `velocap on | off | status` */   /* velocap.c: the event step calls it from seq.c key_on */

static inline void vc_put(uint32_t ts, uint32_t w)
{
    uint32_t n = vc.wr - vc.rd;
    if (n >= VC_N) {
        vc.dropped++;
        return;
    }
    vc.ts[vc.wr % VC_N] = ts;
    vc.w[vc.wr % VC_N] = (uint16_t)w;
    VC_BARRIER();                       /* the slot before the index */
    vc.wr++;
    if (n + 1u > vc.peak)
        vc.peak = n + 1u;
}

static inline void velocap_reading(uint32_t key, uint32_t adc, uint32_t drive, uint32_t ts)
{
    uint32_t w, i;
    if (!vc.on || key >= VC_NKEY)
        return;
    w = adc | drive << 10 | key << 11;
    if (adc < 900u || drive) {
        if (!vc.post[key]) {            /* a run starts: the readings before it first */
            for (i = VC_PRE - vc.pn[key]; i < VC_PRE; i++)
                vc_put(vc.pts[key][i], vc.pw[key][i]);
            vc.pn[key] = 0;
        }
        vc.post[key] = VC_POST;
        vc_put(ts, w);
    } else if (vc.post[key]) {
        vc.post[key]--;
        vc_put(ts, w);
    } else {                            /* at rest: remember it, the oldest out */
        for (i = 0; i + 1u < VC_PRE; i++) {
            vc.pts[key][i] = vc.pts[key][i + 1u];
            vc.pw[key][i] = vc.pw[key][i + 1u];
        }
        vc.pts[key][VC_PRE - 1u] = ts;
        vc.pw[key][VC_PRE - 1u] = (uint16_t)w;
        if (vc.pn[key] < VC_PRE)
            vc.pn[key]++;
    }
}
