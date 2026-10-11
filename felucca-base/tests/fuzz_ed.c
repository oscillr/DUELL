/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Fuzz harness: random editor SysEx frames (with and without the header, any command, 0..1100 bytes, sometimes no
 * terminator) and raw USB-MIDI packets through the real usb.c / editor.c paths of the host build. run_tests.sh runs
 * it briefly under ASan / UBSan with a fixed seed.   fuzz_ed [ITERATIONS [SEED]] */
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"                                /* hostsim, the stubs, RAM flash, reset() */
static uint32_t rs = 0x9E3779B9u;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static void fz_feed(const uint8_t *p, uint32_t n)
{
    uint32_t o, tries;
    for (o = 0; o < n; o += 64u) {
        for (tries = 0; tries < 50u && !ep1_take(p + o, n - o > 64u ? 64u : n - o); tries++) {
            fm1_ms += 2u; ed_service(); host_drain();
        }
    }
}
int main(int argc, char **argv)
{
    static uint8_t f[1300], u[2600];
    uint32_t iters = argc > 1 ? (uint32_t)atoi(argv[1]) : 200000u, i, n, L, j, cmd, raw = 0, frames = 0;
    if (argc > 2) rs = (uint32_t)atoi(argv[2]) | 1u;
    reset();
    usb.rx_pend = 0;
    for (i = 0; i < iters; i++) {
        uint32_t mode = rnd() % 16u;
        if (mode == 0) {                                    /* raw random USB-MIDI event packets */
            uint32_t k = 4u * (1u + rnd() % 16u);
            for (j = 0; j < k; j++) u[j] = (uint8_t)rnd();
            ep1_take(u, k); raw++;
        } else {
            n = 0; f[n++] = 0xF0;
            if (mode == 1) { f[n++] = (uint8_t)(rnd() & 0x7F); f[n++] = (uint8_t)(rnd() & 0x7F); f[n++] = (uint8_t)(rnd() & 0x7F); }
            else { f[n++] = ED_HDR0; f[n++] = ED_HDR1; f[n++] = ED_HDR2; }
            cmd = mode < 4u ? (rnd() & 0x7F) : 1u + rnd() % 70u;
            f[n++] = (uint8_t)cmd;
            L = rnd() % 4u == 0 ? rnd() % 1100u : rnd() % 40u;
            for (j = 0; j < L; j++) {
                uint32_t r = rnd();
                f[n++] = (uint8_t)(r % 8u == 0 ? 0x7F : r % 8u == 1 ? 0 : r % 8u == 2 ? 1 : (r >> 8) & 0x7F);
            }
            if (rnd() % 32u) f[n++] = 0xF7;                 /* sometimes no terminator */
            fz_feed(u, usb_frame(f, n, u)); frames++;
        }
        fm1_ms += 1u + rnd() % 40u;
        ed_service(); host_drain();
        if (rnd() % 64u == 0) { host_wire_n = 0; ed_sync(); host_drain(); }

    }
    printf("fuzz ok: %u frames, %u raw packets\n", frames, raw);
    return 0;
}
