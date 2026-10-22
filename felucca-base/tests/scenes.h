/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.23: the TEST SCENES of the host tests. Until 0.2.22 the CPU tests and several other tests played factory multis (the CPU
 * TEST multis M13..M16 of developer builds, and M01..M10 of that day); since 0.2.23 all sixteen slots are musical factory multis (and
 * M16 the STRESS TEST, tools/factory_multis.py), so the scenes the tests were written for are kept here, byte for byte, as records:
 *   tests/scenes/M13_TEST_1_BASE.bin .. M16_TEST_4_MIX.bin   the four CPU TEST multis (tests/gen_test_multis.c made them)
 *   tests/scenes/M01_REESE_SWAMP.bin .. M10_FINAL_REEL.bin   the factory multis of 0.2.13 .. 0.2.22 in their slots of then (0.2.24: M06 ACID ETHER,
 *                                                            M07 COLD CHAPEL and M08 GARGLE ACID are factory multis again, M09 / M06 / M14, the same bytes)
 *   tests/scenes/spare/M06_TINE_STRINGS.bin (VELVET TINES), M07_BOWL_AND_WIND.bin, M14_SEVENOAKS.bin, M13_BASILDON.bin   SPARE DATA (0.2.24; BASILDON 0.2.25): multis made
 *                                                            for 0.2.23 that left the sixteen so the owner's could come back (tools/factory_multis.py SPARE); multi_render_test renders them
 *                                                            (0.2.25: M07_COLD_CHAPEL.bin is also the factory multi M13, byte for byte)
 *   tests/scenes/spare/M08_MONO_DUEL.bin                     SPARE DATA (0.2.25): MONO DUEL, the factory M08 of 0.2.23 .. 0.2.25 and the only factory multi in DUEL, left the
 *                                                            sixteen for the owner's HAZY TENSOR (the owner: "Mono-dual is out"); multi_render_test renders it as X08_MONO_DUEL
 * scene_put(k, NAME) stores the record in slot k (0..15) as a SAVED multi (multi.c's own store: flash or the host RAM store);
 * scene_load(k, NAME) does that and loads it as the MULTI page does, with the MODE, BALANCE, XFADE and SPLIT it holds applied (as the
 * CPU TEST multis always did, and as MODE RECALL ON does). A test loads a scene into the slot it used to load, so what it measures and
 * hashes is what it was. The files are read relative to felucca-base (host_tests.sh runs there). */
#include <stdio.h>
static uint32_t scene_rec(const char *name, uint8_t *rec, uint32_t max)
{
    char path[160];
    FILE *f;
    uint32_t n;
    snprintf(path, sizeof path, "tests/scenes/%s.bin", name);
    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "scenes.h: %s missing (run from felucca-base)\n", path);
        exit(2);
    }
    n = (uint32_t)fread(rec, 1, max, f);
    fclose(f);
    return n;
}
static void scene_put(uint32_t k, const char *name)
{
    static uint8_t rec[2048];
    uint32_t n = scene_rec(name, rec, sizeof rec), cut, len;
    k %= MU_SLOTS;
    mu_obj_read(k / 2u);
    len = mu_put_rec(k & 1u, 1, 0, &cut, rec, n);
    if (mu_obj_write(k / 2u, len)) {
        fprintf(stderr, "scenes.h: %s not stored in M%02u\n", name, k + 1u);
        exit(2);
    }
    mu_note(k / 2u);
}
static void scene_load(uint32_t k, const char *name)
{
    uint8_t keep = ui_prefs2;
    scene_put(k, name);
    ui_prefs2 |= PREF_MODE_RECALL >> 16;
    project_load(k);
    ui_prefs2 = keep;
}
