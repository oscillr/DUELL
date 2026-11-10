/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Fuzz harness: mutated / random project stores (with and without a valid hash, FUN1..FUN8 magics, any length)
 * through proj_import -> project_restore_runtime -> render. run_tests.sh runs it briefly under ASan / UBSan with a
 * fixed seed.   fuzz_proj [ITERATIONS [SEED]]
 * The cloud plan's job A: every valid store carries random extension slots and a random hand-set B edit list per part (PROJ_REV 6), the mutations aim at their
 * bytes too (nx, ne and the pairs), and each iteration also packs the same music as a MULTI record (MU_VER 7), mutates / cuts it and unpacks it. */
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"                                /* hostsim, the stubs, RAM flash, reset() */
static uint32_t rs = 0x2545F491u;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
int main(int argc, char **argv)
{
    static project_t a, c;
    static project_store_t st;
    static int32_t o[CTL * 2 + 16];
    static uint8_t rec[2048];
    uint32_t iters = argc > 1 ? (uint32_t)atoi(argv[1]) : 20000u, i, j, k, n, imported = 0, mu_in = 0, mu_tried = 0;
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
            for (j = 0; j < NSHOWN; j++) {                          /* the extension slots and a hand-set B list, random */
                for (k = 0; k < NX; k++)
                    a.t[j].x[k] = (int16_t)(rnd() % 192u) - 64;
                a.xe_n[j] = (uint8_t)(rnd() % 3u == 0 ? 0u : rnd() % (XB_MAX + 1u));
                for (k = 0; k < a.xe_n[j]; k++)
                    a.xe[j][k] = (xb_edit_t){(uint8_t)((k * 3u + j) % 126u + (rnd() % 5u == 0 ? 128u - 0u : 0u) % 127u), (int16_t)(rnd() % 192u) - 64};
            }
            a.sum = proj_sum(&a);
            if (!proj_pack(&st, &a)) continue;
            k = 1u + rnd() % 48u;
            for (j = 0; j < k; j++) {
                uint32_t pa = 68u + P_COUNT, off;               /* (part A's nx byte; part B's follows A's steps) */
                off = rnd() % 4u == 0 ? rnd() % 200u : rnd() % 3u == 0 ? pa - 1u + rnd() % (2u + NX + 2u * XB_MAX) :
                      rnd() % 3u == 0 ? pa + 1u + NX + 1u + 2u * a.xe_n[0] + 2u + NSTEP * 9u + P_COUNT - 1u + rnd() % (2u + NX + 2u * XB_MAX) : rnd() % sizeof st.raw;
                if (off >= sizeof st.raw) off = rnd() % sizeof st.raw;
                st.raw[off] = (uint8_t)(rnd() % 4u == 0 ? 0xFF : rnd() % 4u == 0 ? 0x7F : rnd() % 4u == 0 ? XB_MAX + 1u : rnd());
            }
            if (rnd() % 2u) {                                   /* the same music as a multi record, mutated or cut */
                uint8_t blk[NSHOWN][MB_N], amt[NSHOWN], set[6];
                uint32_t cut, len, m;
                memset(blk, 0, sizeof blk);
                len = mu_pack(rec, &a, blk, &cut);
                m = 1u + rnd() % 24u;
                for (j = 0; j < m; j++) {
                    uint32_t off = rnd() % 2u ? MU_G_AT + 2u * G_COUNT + 4u + MB_N + P_COUNT - 1u + rnd() % (4u + NX + 2u * XB_MAX) : rnd() % len;
                    rec[off % len] = (uint8_t)(rnd() % 4u == 0 ? 0xFF : rnd() % 4u == 0 ? XB_MAX + 1u : rnd());
                }
                if (rnd() % 4u == 0) len = rnd() % (len + 1u);
                mu_tried++;
                if (mu_unpack(&c, blk, amt, set, rec, len)) {
                    mu_in++;
                    proj_bound(&c);
                    project_restore_runtime(&c);
                    for (j = 0; j < NTRK; j++) trk_note_on(&trk[j], 48u + rnd() % 36u, 100);
                    for (j = 0; j < 4u; j++) mix_block(o, CTL);
                }
            }
        }
        if (rnd() % 2u) {                                           /* a valid checksum: attacker-controlled flash */
            uint32_t sum = proj_hash(st.raw, PROJ_STORE_SIZE - 4u);
            memcpy(st.raw + PROJ_STORE_SIZE - 4u, &sum, 4);
        }
        n = rnd() % 8u == 0 ? rnd() % (PROJ_STORE_SIZE + 64u) : rnd() % 8u == 0 ? 3388u : PROJ_STORE_SIZE;
        if (proj_import(&c, &st, (int)n)) {
            imported++;
            project_restore_runtime(&c);
            for (j = 0; j < NTRK; j++) trk_note_on(&trk[j], 48u + rnd() % 36u, 100);
            for (j = 0; j < 12u; j++) mix_block(o, CTL);
        }
        if (rnd() % 64u == 0) reset();
    }
    printf("fuzz_proj ok: %u iterations, %u imported; multi records: %u tried, %u unpacked\n", iters, imported, mu_tried, mu_in);
    return 0;
}
