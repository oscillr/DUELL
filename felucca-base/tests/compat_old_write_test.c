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
/* DUELL 1.1 compatibility, step 1 (run it against the 1.0 sources, git archive of the release): save multis and the settings the way the OLD firmware does and keep the flash image.
 *   compat_old_test write FILE */
int main(int argc, char **argv)
{
    uint32_t slot;
    FILE *f;
    if (argc < 2) return 2;
    power();
    for (slot = 0; slot < 6u; slot++) {
        uint32_t k, e0 = slot % 3u == 0u ? 14u : slot % 3u == 1u ? 7u : 0u;       /* WAVE, WHEEL, ANALOG */
        for (k = 0; k < 2u; k++) {
            trk[k].eng_req = (uint8_t)e0;
            apply_preset_to(&trk[k], (slot + k) % 3u);
            trk[k].p[P_VOICE] = 0;                                              /* POLY */
        }
        if (slot & 1u) {                                                        /* old automation events on parameters the matrix list has not (GLIDE, DETUNE, LFO PHASE): 1.0 recorded them */
            motion_set_event(&trk[0], 2, P_GLIDE, 20);
            motion_set_event(&trk[0], 5, P_DETUNE, 30);
        }
        motion_set_event(&trk[1], 3, P_REV, 40);
        duell_mode_req = (uint8_t)(slot % 3u == 0u ? DM_LAYER : slot % 3u == 1u ? DM_STEREO : DM_SPLIT);
        project_save(slot);
        trk[0].p[P_VOICE] = 0;
        motion.count = 0;
    }
    {   /* two user patches as the old firmware stores them (slots 40 and 41): a WAVE and an ANALOG sound in POLY */
        uint32_t n, i;
        for (n = 0; n < 2u; n++) {
            up_rec_t r;
            memset(&r, 0, sizeof r);
            trk[0].eng_req = (uint8_t)(n ? 0u : 14u);
            apply_preset_to(&trk[0], 1);
            trk[0].p[P_VOICE] = 0;
            r.used = UP_USED; r.ver = UP_VER; r.engine = trk[0].eng_req; r.np = P_COUNT;
            memcpy(r.name, n ? "OLD ANALOG" : "OLD WAVE", n ? 10 : 8);
            for (i = 0; i < P_COUNT; i++)
                up_set_value(&r, i, trk[0].p[i]);
            up_set_cat(&r, CAT_LEAD);
            printf("compat: old patch %u written: up_put -> %d\n", 40u + n, up_put(40u + n, &r));
        }
    }
    settings.palette = 4;                                                        /* PALE: index 4 of the 1.0 list */
    settings_save();
    f = fopen(argv[1], "wb");
    fwrite(nor, 1, sizeof nor, f);
    fclose(f);
    printf("compat: wrote %s (multis M01..M06 as the old firmware saves them, palette index 4)\n", argv[1]);
    return 0;
}
