/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL) */
/* The repo's golden renders (tests/regress.c, tests/golden.txt) as WebAssembly: the same job code, the same flags as host_tests.sh
 * (-DNPART=4 -DHOST_MIX_FOLLOWS_MASTER=1 -DDUELL_TEST_MODE=1, build/gen_test), run by golden_compare.mjs in Node. Test only: not part of the page.
 * regress.c forks one child per render so every render starts from the boot state; here each render is a fresh instance of the module. */
#include <stdint.h>
#define main regress_main
#include "../../tests/regress.c"
#undef main
#define EXPORT __attribute__((used, visibility("default")))

/* the golden renders in regress.c's order: presets, DUELL sounds, voice modes, sends, the song */
static job_t G[MAXJ];
static uint32_t ng;
static void gadd(uint32_t kind, const char *name, uint32_t e, uint32_t pi, uint32_t arg)
{
    job_t *j = &G[ng++];
    memset(j, 0, sizeof *j);
    j->kind = (uint8_t)kind; j->e = (uint8_t)e; j->pi = (uint8_t)pi; j->arg = (uint8_t)arg;
    snprintf(j->name, sizeof j->name, "%s", name);
}
EXPORT uint32_t g_count(void)
{
    static const char *const MN[4] = {"POLY", "MONO", "LEGATO", "UNISON"};
    static const char *const SN[6] = {"dry", "chorus", "delay", "reverb", "all", "dist"};
    static const uint8_t MODE_E[3][2] = {{0, 0}, {1, 1}, {5, 0}};
    static const uint8_t SEND_E[2][2] = {{0, 3}, {1, 0}};
    char name[64], s[64];
    uint32_t e, pi, i, k0;
    if (ng)
        return ng;
    for (e = 0; e < NENGINES; e++)
        for (pi = 0; pi < ENGINES[e]->npresets; pi++) {
            slug(s, ENGINES[e]->presets[pi].name, sizeof s);
            snprintf(name, sizeof name, "preset/%s/%02u_%s", ENGINES[e]->name, pi, s);
            gadd(J_PRESET, name, e, pi, 0);
        }
    for (e = 0; e < NENGINES; e++)
        for (pi = 0; pi < ds_count(e); pi++) {
            slug(s, ds_name(e, pi), sizeof s);
            snprintf(name, sizeof name, "sound/%s/%02u_%s", ENGINES[e]->name, pi, s);
            gadd(J_SOUND, name, e, pi, 0);
        }
    for (i = 0; i < 3u; i++)
        for (k0 = 0; k0 < 4u && eng_ok(MODE_E[i][0]); k0++) {
            snprintf(name, sizeof name, "mode/%s/%s", ENGINES[MODE_E[i][0]]->name, MN[k0]);
            gadd(J_MODE, name, MODE_E[i][0], MODE_E[i][1], k0);
        }
    for (i = 0; i < 2u; i++)
        for (k0 = 0; k0 < 6u && eng_ok(SEND_E[i][0]); k0++) {
            snprintf(name, sizeof name, "sends/%s/%s", ENGINES[SEND_E[i][0]]->name, SN[k0]);
            gadd(J_SENDS, name, SEND_E[i][0], SEND_E[i][1], k0);
        }
    gadd(J_SONG, "song/4track_mix", 0, 0, 0);
    return ng;
}
EXPORT const char *g_name(uint32_t i) { return G[i].name; }
/* render job i (once per fresh instance); the FNV-1a hash as two halves, the sample count */
static uint64_t g_hash;
static int32_t *g_cap;
EXPORT uint32_t g_run(uint32_t i) { memset(&R, 0, sizeof R); run_job_body(&G[i]); g_hash = R.hash; return R.frames; }
EXPORT uint32_t g_hash_lo(void) { return (uint32_t)g_hash; }
EXPORT uint32_t g_hash_hi(void) { return (uint32_t)(g_hash >> 32); }
EXPORT uint32_t g_peak(void) { return (uint32_t)R.peak; }
