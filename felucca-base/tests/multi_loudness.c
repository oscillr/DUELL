/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.0: the loudness of the sixteen factory multis, as tests/sound_audit.c measures the patches (tests/loudness.h: ear-weighted, on the mix before
 * MASTER; the key C3 held 10 s, as played from the keys: both parts answer, LAYER, the multi's own BALANCE / XFADE, MODE RECALL ON, End A, 1.0 s tap).
 * Output: one line per multi: slot, name, sustains, MED (the held body), LOUD, PEAK, TAP (a 1 s note's loudest 0.5 s).
 *   build/host/multi_loudness            (by hand, release_drafts/VOLUME_CHECK_V1.md; not in the default list) */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define HOST_IRQ_HOOK 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
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
#include <math.h>
#include "loudness.h"

static void ml_setup(void *v)
{
    uint32_t slot = *(const uint32_t *)v, i;
    int32_t o[2 * CTL];
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    multi_boot();
    frame();
    song.master_q12 = 4096;
    ui_prefs2 |= PREF_MODE_RECALL >> 16;
    project_load(slot);
    for (i = 0; i < 40; i++) { memset(o, 0, sizeof o); mix_block(o, CTL); while (wave_service()) ; }
    morph_set_target(1);
    morph_set_fader(getenv("ML_FADER") ? (uint32_t)atoi(getenv("ML_FADER")) : 0u);
    if (getenv("ML_INFO"))
        fprintf(stderr, "M%02u LEVEL %d/%d TRIM %d/%d PAN %d/%d BAL %d XF %u CHRD %d/%d VOICE %d/%d\n", slot + 1u, trk[0].p[P_LEVEL], trk[1].p[P_LEVEL], trk[0].p[P_TRIM], trk[1].p[P_TRIM], trk[0].p[P_PAN], trk[1].p[P_PAN], (int)mix_bal, (unsigned)mix_xf, trk[0].p[P_CHRD], trk[1].p[P_CHRD], trk[0].p[P_VOICE], trk[1].p[P_VOICE]);
    if (getenv("ML_K")) {                          /* tools/morph_b_roll.py: End B rolled again with the ML_K-th seed of the slot's fixed sequence (as tests/morph_b_roll.c draws it) */
        uint32_t k, seed = 1, pi;
        morph.gen = 1717u + 7u * (slot + 1u);
        fm1_ms = 0;
        for (k = 0; k < (uint32_t)atoi(getenv("ML_K")); k++)
            seed = morph_new_seed();
        for (pi = 0; pi < NSHOWN; pi++)
            morph_roll_seed(pi, 1u, seed);
    }
    song.octave = -1;                              /* key 7 = C4 (60) - 12 = C3 (48), the audit's note */
    key_down(7);
}

int main(void)
{
    uint32_t s, ar[16];
    lz_job_t jb[16], jt[16];
    for (s = 0; s < 16u; s++)
        ar[s] = s;
    lz_hold = 10.0;
    for (s = 0; s < 16u; s++)
        jb[s] = lz_spawn(ml_setup, &ar[s], 0, 0);
    lz_t L[16], T[16];
    for (s = 0; s < 16u; s++)
        L[s] = lz_collect(jb[s]);
    lz_hold = 1.0;
    for (s = 0; s < 16u; s++)
        jt[s] = lz_spawn(ml_setup, &ar[s], 0, 0);
    for (s = 0; s < 16u; s++)
        T[s] = lz_collect(jt[s]);
    for (s = 0; s < 16u; s++)
        printf("M%02u sus=%d MED=%.1f LOUD=%.1f PEAK=%.1f TAP=%.1f L9=%.1f nan=%d\n", s + 1u, L[s].l9 > -60.0, L[s].med16, L[s].loud, L[s].peak, T[s].loud, L[s].l9, L[s].nan);
    return 0;
}
