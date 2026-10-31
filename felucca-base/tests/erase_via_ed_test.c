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
/* DUELL 1.1: ERASE_ALL through the editor protocol's real entry (ed_handle: every handler before it sees the frame first), on the flash image of an OLD firmware: the reply, the flash, the next start.
 *   erase_via_ed_test FILE */
int main(int argc, char **argv)
{
    uint8_t fr[16];
    FILE *f;
    uint32_t i, filled0 = 0, filled1 = 0, bad = 0;
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
    printf("erase: before: palette %s, M01 stored %d\n", UI_PALETTES[settings.palette % NPALETTES].name, project_used(0));
    fr[0] = ED_HDR0; fr[1] = ED_HDR1; fr[2] = ED_HDR2; fr[3] = 80;
    memcpy(fr + 4, "ERASE ALL", 9);
    ed_n = 0;
    ed_handle(fr, 13u);
    printf("erase: reply %u bytes, rc byte %d (0 = erased)  restart due %u\n", ed_n, ed_n > 5 ? ed_out[5] : -1, erase_at != 0);
    bad += !(ed_n > 5 && ed_out[5] == 0);
    for (i = 0; i < 40; i++) { fm1_ms += 100; frame(); }
    printf("erase: restart due now: %d\n", erase_restart_due());
    /* the next start: the same flash, power-on again */
    ui_power_on();
    flash_ok = 1;
    erase_at = 0;
    persist_boot();
    multi_boot();
    settings_init();
    frame();
    printf("erase: after the restart: palette %s (VIOLET = a new unit), first start pending %d\n", UI_PALETTES[settings.palette % NPALETTES].name, (int)first_boot_pending);
    bad += settings.palette != 0u || !first_boot_pending;
    printf("erase: %s\n", bad ? "FAIL" : "ok");
    (void)filled0; (void)filled1;
    return bad != 0;
}
