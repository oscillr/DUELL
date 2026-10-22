/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL release build (DUELL_TEST_MODE off, the default): no MENU > SYSTEM > TEST row and no TEST page; the sixteen factory multis M01..M16
 * (0.2.23: the same as in the test mode, M16 the STRESS TEST; the CPU test multis of before are test scenes, tests/scenes.h) are intact. Built WITHOUT -DDUELL_TEST_MODE=1 and with build/gen/duell_multis.h
 * made without the switch (host_tests.sh "release"). The test-mode side is multi_test / select_test / erase_test / ui_test (built with the switch on).
 *   build/host/release_test            (DUELL/host_tests.sh "release") */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define FELUCCA_CDC 0                                  /* (the product: no serial console, no MENU > USB SERIAL; src/felucca.c's default for a build without DUELL_TEST_MODE / DUELL_BENCH) */
#include "ui_test.c"

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static int bad;
static void ck(const char *what, int ok)
{
    printf("release: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

int main(void)
{
    static const char *const FN[16] = {"REESE SWAMP", "NIGHT ORGAN", "PAD SPLIT", "DUB CHORD", "WHISPER TIDE", "TINE STRINGS", "FINAL REEL",
                                       "HAZY TENSOR", "ACID ETHER", "WIRED SNAIL", "EERIE SIGNAL", "DEAD BUFFER", "COLD CHAPEL", "GARGLE ACID",
                                       "WOOD & SKIN", "STRESS TEST"};   /* (the 0.2.24 order, tools/factory_multis.py) */
    uint32_t k, ok, test_rows = 0, sys_rows = 0;
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    multi_boot();
    frame();
    ck("the switch is off by default", DUELL_TEST_MODE == 0 && DF_TEST_MULTIS == 0 && DF_N == 16u);
    for (k = 0; k < MI_COUNT; k++) {
        test_rows += str_eq(MI_NAME[k], "TEST");
        sys_rows += MI_TAB[k] == MTAB_SYSTEM;
    }
    ck("MENU has no TEST row and no USB SERIAL row (SYSTEM: RESTORE LAST, BUTTON MAP, GUIDE, ABOUT, ERASE DATA)", test_rows == 0u && sys_rows == 5u && MI_TAB[MI_ABOUT] == MTAB_SYSTEM &&
       MI_TAB[MI_GUIDE] == MTAB_SYSTEM && MI_READONLY(MI_GUIDE) &&
       MI_READONLY(MI_ABOUT) && !MI_READONLY(MI_PANEL) && !menu_valued(MI_PANEL) && str_eq(MI_NAME[MI_PANEL], "BUTTON MAP") &&
       MI_PANEL == MI_RESTORE + 1u && MI_GUIDE == MI_PANEL + 1u && MI_ERASE == MI_COUNT - 1u);
    for (k = 0, ok = 1; k < MI_COUNT; k++)
        ok &= !str_eq(MI_NAME[k], "USB SERIAL");
    ck("memory trim 1: the product has no serial console: FELUCCA_CDC 0, no USB SERIAL row (the editor's list: tests/menu_product_test.c)", ok && FELUCCA_CDC == 0);
    for (k = 0, ok = 1; k < MTAB_COUNT; k++)            /* (the release's tabs fit one page each: SYSTEM 5 units) */
        ok &= mtab_pages(k) == 1u;
    {
        uint32_t u;
        ok &= menu_place(MI_ERASE, &u) == 0u && u == 4u;
    }
    ck("MENU: every tab one page in the release (SYSTEM: 5 units, ERASE DATA the last)", ok);
    for (k = 0, ok = 1; k < NPAGES; k++)
        ok &= !str_eq(PAGES[k].title, "TEST");
    ck("no page is called TEST (the GLO round is untouched)", ok);
    for (k = 0, ok = 1; k < MU_SLOTS; k++)
        ok &= project_used(k) && project_factory(k) && str_eq(mu_names[k], FN[k]);
    ck("M01..M16 are the factory multis (0.2.24 order: REESE SWAMP, NIGHT ORGAN .. WOOD & SKIN, M16 STRESS TEST), as in the test mode", ok);
    project_load(15);
    ck("LOAD of M16 loads the STRESS TEST (PHYS + WAVE)", proj_cur == 15u && trk[0].eng_req == 9u && trk[1].eng_req == ENGI_WAVE);
    printf(bad ? "release: FAILED (%d)\n" : "release: all ok\n", bad);
    return bad ? 1 : 0;
}
