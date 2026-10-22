/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.29 diagnosis (owner, T0226): the tail of M07 FINAL REEL (#58 BRASS PAD + #54 STRING MACH, LAYER, a hall) "cut like a noise gate".
 * The factory multi loaded as the LOAD page does it (MODE RECALL), MASTER at the default (2048), both parts a 3-note chord 3 s, released, 10 s.
 * Per 100 ms: the output (dBFS of 16 bit), the dry mid ((mix_l + mix_r) / 2: the HALL side cancels) and the wet bus (wet + the HALL side, left),
 * in the units before MASTER (32767 = 0 dBFS), and the voices sounding. build/host/m07_tail_probe [slot0] */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define HOST_IRQ_HOOK 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
static void fm1_irq_off(void);
static void fm1_irq_on(void);
#include "ui_test.c"
#include <math.h>
static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { const uint8_t *s = src; uint32_t i; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}
static double dbv(double ms) { return ms > 0 ? 10 * log10(ms / (32767.0 * 32767.0)) : -999; }
static const uint8_t CH1[3] = {48, 55, 64}, CH2[3] = {60, 67, 72};
int main(int argc, char **argv)
{
    int32_t o[2 * CTL];
    uint32_t i, b, k, slot = argc > 1 ? (uint32_t)atoi(argv[1]) : 6u, nh = (uint32_t)(3.0 * FS) / CTL, nt = (uint32_t)(10.0 * FS) / CTL, per = (uint32_t)(0.1 * FS / CTL);
    double eo = 0, ed = 0, ew = 0;
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    multi_boot();
    frame();
    ui_prefs2 |= PREF_MODE_RECALL >> 16;
    project_load(slot);
    song.master_q12 = 2048;
    for (i = 0; i < 40; i++) { mix_block(o, CTL); while (wave_service()) ; }
    printf("slot %u mode %d RTYPE %d RSIZE %d RDAMP %d LOCUT %d | p1 C%d D%d R%d p2 C%d D%d R%d | BM %d comp 0x%x\n", slot, duell_mode, song.g[G_RTYPE], song.g[G_RSIZE], song.g[G_RDAMP],
           song.g[G_RLOCUT], trk[0].p[P_CHOR], trk[0].p[P_DLY], trk[0].p[P_REV], trk[1].p[P_CHOR], trk[1].p[P_DLY], trk[1].p[P_REV], bm_sel, cmp_cfg);
    for (i = 0; i < 3; i++) { trk_note_on(&trk[0], CH1[i], 100); trk_note_on(&trk[1], CH2[i], 100); }
    printf("  t      out    dry    wet  voices\n");
    for (b = 0; b < nh + nt; b++) {
        uint32_t nv = 0;
        if (b == nh)
            for (i = 0; i < 3; i++) { trk_note_off(&trk[0], CH1[i]); trk_note_off(&trk[1], CH2[i]); }
        mix_block(o, CTL);
        while (wave_service()) ;
        for (i = 0; i < CTL; i++) {
            double x = o[2 * i], d = (mix_l[i] + mix_r[i]) * 0.5, w = wet[i] + (hl.side ? rev_side[i] : 0);
            eo += x * x; ed += d * d; ew += w * w;
        }
        if ((b + 1) % per == 0) {
            for (k = 0; k < 2; k++) for (i = 0; i < NVOICE; i++) nv += trk[k].v[i].active != 0;
            if (b + 1 >= nh)
                printf("%5.1f %6.1f %6.1f %6.1f  %u\n", (double)(b + 1 - nh) * CTL / FS, dbv(eo / (per * CTL)), dbv(ed / (per * CTL)), dbv(ew / (per * CTL)), nv);
            eo = ed = ew = 0;
        }
    }
    return 0;
}
