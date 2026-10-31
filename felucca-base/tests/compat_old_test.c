/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define _FORTIFY_SOURCE 0
#define HOST_IRQ_HOOK 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include "memfast.h"
void *memset(void *d, int c, size_t n) { return mf_memset(d, c, (unsigned)n); }
void *memcpy(void *d, const void *s, size_t n) { return mf_memcpy(d, s, (unsigned)n); }
static void fm1_irq_off(void);
static void fm1_irq_on(void);
#include "ui_test.c"
static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { const uint8_t *s = src; uint32_t i; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}
static int fails;
static void power(void)
{
    uint32_t k;
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    for (k = 0; k < NTRK; k++)
        memset(fm6_patch[k], 0, sizeof fm6_patch[k]);
    flash_ok = 1;
    multi_boot();
    frame();
}
/* DUELL 1.1 compatibility, step 2 (the NEW sources): the flash image the OLD firmware wrote (compat_old_write_test.c, built from the 1.0 release) is read at power-on: the settings
 * (palette index), every multi M01..M06 must list under its name and LOAD without DAMAGED, with its voice mode (POLY) and its engines; what the new firmware does not record any more
 * (old automation events) is dropped, never a reason to refuse the multi.
 *   compat_old_test FILE */
int main(int argc, char **argv)
{
    uint32_t slot, bad = 0, i;
    FILE *f;
    if (argc < 2) return 2;
    f = fopen(argv[1], "rb");
    if (!f || fread(nor, 1, sizeof nor, f) != sizeof nor) return 3;
    fclose(f);
    ui_power_on();
    flash_ok = 1;
    persist_boot();
    multi_boot();
    settings_init();
    frame();
    printf("compat: palette %s (stored index 4 = %s)\n", UI_PALETTES[settings.palette % NPALETTES].name, UI_PALETTES[4].name);
    bad += settings.palette != 4u;
    for (slot = 0; slot < 6u; slot++) {
        char nm[PROJ_NAME_LEN + 1u];
        int listed;
        ui.msg[0] = 0;
        proj_name_get(nm, (const uint8_t *)"");
        listed = project_used(slot);
        {
            uint32_t len = 0;
            const uint8_t *r;
            mu_obj_read(slot / 2u);
            r = mu_rec(slot & 1u, &len);
            printf("compat: M%02u record: len %u ver %u np %u (now %u) ng %u (now %u) mode %u\n", slot + 1u, len, len ? r[0] : 0u, len ? r[1] : 0u, (unsigned)P_COUNT, len ? r[2] : 0u, (unsigned)G_COUNT, len ? r[4] : 0u);
        }
        trk[0].p[P_VOICE] = 3;
        project_load(slot);
        for (i = 0; i < 6; i++) {
            frame();
            budget_poll();
        }
        printf("compat: M%02u listed %d  message '%s'  part 1 %s VCE %d  part 2 %s VCE %d  events %u\n", slot + 1u, listed, ui.msg, ENGINES[eng_idx(trk[0].eng_req)]->name, trk[0].p[P_VOICE],
               ENGINES[eng_idx(trk[1].eng_req)]->name, trk[1].p[P_VOICE], (unsigned)motion.count);
        bad += !listed || str_eq(ui.msg, "DAMAGED") || trk[0].p[P_VOICE] != 0 || trk[1].p[P_VOICE] != 0;
    }
    for (slot = 40; slot < 42u; slot++) {            /* the two user patches the old firmware stored */
        track_t *t = TSEL;
        t->p[P_VOICE] = 3;
        t->eng_req = 3;
        ui.msg[0] = 0;
        printf("compat: patch %u used %d engine(raw) %u  ", slot, up_used(slot), up_engine_raw(slot));
        up_load(slot);
        frame();
        printf("-> engine %s VCE %d message '%s'\n", ENGINES[eng_idx(t->eng_req)]->name, t->p[P_VOICE], ui.msg);
        bad += !up_used(slot) || t->p[P_VOICE] != 0 || str_eq(ui.msg, "DAMAGED");
    }
    printf("compat: %s\n", bad ? "FAIL" : "ok");
    return bad != 0;
}
