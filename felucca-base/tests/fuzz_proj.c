/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Fuzz harness: mutated / random project stores (with and without a valid hash, FUN1..FUN8 magics, any length)
 * through proj_import -> project_restore_runtime -> render. run_tests.sh runs it briefly under ASan / UBSan with a
 * fixed seed.   fuzz_proj [ITERATIONS [SEED]] */
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"                                /* hostsim, the stubs, RAM flash, reset() */
static uint32_t rs = 0x2545F491u;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
int main(int argc, char **argv)
{
    static project_t a, c;
    static project_store_t st;
    static int32_t o[CTL * 2 + 16];
    uint32_t iters = argc > 1 ? (uint32_t)atoi(argv[1]) : 20000u, i, j, k, n, imported = 0;
    if (argc > 2) rs = (uint32_t)atoi(argv[2]) | 1u;
    reset();
    for (i = 0; i < iters; i++) {
        uint32_t mode = rnd() % 8u;
        if (mode == 0) {                                            /* fully random bytes */
            for (j = 0; j < sizeof st.raw; j++) st.raw[j] = (uint8_t)rnd();
            if (rnd() & 1u) { uint32_t m = 0x46554E30u + 1u + rnd() % 8u; memcpy(st.raw, &m, 4); }   /* "FUN1".."FUN8" */
        } else {                                                    /* a valid store, mutated */
            for (j = 0; j < NTRK; j++) if (rnd() & 1u) trk_note_on(&trk[j], 48u + rnd() % 36u, 1u + rnd() % 127u);
            project_capture(&a);
            if (!proj_pack(&st, &a)) continue;
            k = 1u + rnd() % 48u;
            for (j = 0; j < k; j++) {
                uint32_t off = rnd() % 4u == 0 ? rnd() % 200u : rnd() % sizeof st.raw;
                st.raw[off] = (uint8_t)(rnd() % 4u == 0 ? 0xFF : rnd() % 4u == 0 ? 0x7F : rnd());
            }
        }
        if (rnd() % 2u) {                                           /* a valid checksum: attacker-controlled flash */
            uint32_t sum = proj_hash(st.raw, PROJ_STORE_SIZE - 4u);
            memcpy(st.raw + PROJ_STORE_SIZE - 4u, &sum, 4);
        }
        n = rnd() % 8u == 0 ? rnd() % (PROJ_STORE_SIZE + 64u) : rnd() % 8u == 0 ? PROJ_STORE_V7 : PROJ_STORE_SIZE;
        if (proj_import(&c, &st, (int)n)) {
            imported++;
            project_restore_runtime(&c);
            for (j = 0; j < NTRK; j++) trk_note_on(&trk[j], 48u + rnd() % 36u, 100);
            for (j = 0; j < 12u; j++) mix_block(o, CTL);
        }
        if (rnd() % 64u == 0) reset();
    }
    printf("fuzz_proj ok: %u iterations, %u imported\n", iters, imported);
    return 0;
}
