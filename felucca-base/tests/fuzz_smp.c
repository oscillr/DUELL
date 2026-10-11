/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Fuzz harness: random user sample slot headers (half of them valid, zones at the edges of the data) ->
 * smp_user_scan -> SAMPLE / GRAIN / SLICE notes and render. run_tests.sh runs it briefly under ASan / UBSan with a
 * fixed seed.   fuzz_smp [ITERATIONS [SEED]] */
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"                                /* hostsim, the stubs, RAM flash, reset() */
static uint32_t rs = 0x6A09E667u;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
int main(int argc, char **argv)
{
    static int32_t o[CTL * 2 + 16];
    uint32_t iters = argc > 1 ? (uint32_t)atoi(argv[1]) : 20000u, i, j, k, valid = 0;
    if (argc > 2) rs = (uint32_t)atoi(argv[2]) | 1u;
    reset();
    for (i = 0; i < iters; i++) {
        k = rnd() % SMP_USER_SLOTS;
        smp_user_hdr_t *h = (smp_user_hdr_t *)host_samples[k];
        for (j = 0; j < sizeof host_samples[k]; j += 1u + rnd() % 7u) host_samples[k][j] = (uint8_t)rnd();
        if (rnd() % 8u) { h->magic = SMP_USER_MAGIC; h->version = 1; }
        h->nz = (uint8_t)(rnd() % 4u == 0 ? rnd() : 1u + rnd() % 16u);
        h->data_len = rnd() % 4u == 0 ? rnd() : rnd() % (SMP_USER_SIZE - SMP_USER_DATA + 1u);
        if (rnd() % 2u) {                                           /* a valid slot: data_len in range, zones inside it */
            h->nz = (uint8_t)(1u + rnd() % 16u);
            h->data_len = 1024u + rnd() % (SMP_USER_SIZE - SMP_USER_DATA - 1024u);
        }
        for (j = 0; j < 16u; j++) {
            smp_zone_t *z = &h->zone[j];
            if (rnd() % 4u) {                                       /* mostly valid zones */
                uint32_t maxb = h->data_len > SMP_USER_SIZE ? SMP_USER_SIZE : h->data_len;
                z->n = 2u + rnd() % (2u * maxb); z->off = rnd() % (maxb + 1u);
                if (z->off + (z->n + 1u) / 2u > maxb) z->off = maxb - (z->n + 1u) / 2u;
                z->ls = rnd() % z->n; z->le = z->ls + rnd() % (z->n - z->ls); if (z->le >= z->n) z->le = z->n - 1u;
                z->rate = rnd() % 16u == 0 ? rnd() : 1u + rnd() % (4u << 16);
                z->idx = (uint8_t)(rnd() % 16u == 0 ? rnd() : rnd() % 89u);
                z->lo = (uint8_t)(rnd() % 128u); z->hi = (uint8_t)(z->lo + rnd() % (128u - z->lo));
                z->looped = (uint8_t)(rnd() % 2u); z->root16 = (int16_t)(rnd() % 4096u); z->pred = (int16_t)rnd();
            }
        }
        /* half the time as the firmware does before a rescan (SMP_BEGIN / ERASE: the zones' n go to 0, sounding voices
         * end), half not: a slot that fails its check must leave no zone behind for a voice still on it (1.1) */
        if (rnd() % 2u) {
            uint32_t q;
            for (q = 0; q < 16u; q++) usr_zone[k][q].n = 0;
            usr_nz[k] = 0;
            for (q = 0; q < 4u; q++) mix_block(o, CTL);
        }
        smp_user_scan(k);
        if (usr_nz[k]) valid++;
        for (j = 0; j < NTRK; j++) {                                 /* SAMPLE, GRAIN, SLICE on the slot */
            uint32_t e = (j % 3u == 0) ? 4u : (j % 3u == 1) ? 8u : 13u;
            set_engine_of(&trk[j], e);
            trk[j].p[P_E0] = (int16_t)(SMP_NSETS + k);
            trk[j].p[P_E1] = (int16_t)(rnd() % 128u); trk[j].p[P_E2] = (int16_t)(rnd() % 128u);
            trk_note_on(&trk[j], 24u + rnd() % 84u, 100);
        }
        for (j = 0; j < 24u; j++) { mix_block(o, CTL); fm1_ms++; ed_service(); host_drain(); }
        for (j = 0; j < NTRK; j++) trk_note_on(&trk[j], 24u + rnd() % 84u, 100);
        for (j = 0; j < 24u; j++) mix_block(o, CTL);
    }
    printf("fuzz_smp ok: %u iterations, %u valid slots\n", iters, valid);
    return 0;
}
