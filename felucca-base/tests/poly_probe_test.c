/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.1 probe (owner's test3 bug): POLY voice mode plays several notes on WAVE and ANALOG, in every mode, and the AVAILABLE VOICES number counts the free voices.
 * Same set-up as budget_test.c. */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define _FORTIFY_SOURCE 0
#define HOST_IRQ_HOOK 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
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
static uint32_t held(const track_t *t) { uint32_t i, n = 0; for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].gate; return n; }
static void multis(void)
{
    uint32_t m, i;
    for (m = 0; m < 16u; m++) {
        power();
        project_load(m);
        for (i = 0; i < 8; i++) {
            frame();
            budget_poll();
        }
        printf("multi %2u: mode %u  part 1 %-8s VCE %d notes %u (reason %d)  part 2 %-8s VCE %d notes %u\n", m + 1u, (unsigned)duell_mode_req, ENGINES[eng_idx(trk[0].eng_req)]->name, trk[0].p[P_VOICE], trk_nvoice(&trk[0]),
               (int)part_notes_reason(0), ENGINES[eng_idx(trk[1].eng_req)]->name, trk[1].p[P_VOICE], trk_nvoice(&trk[1]));
    }
}
int main(void)
{
    static const char *const EN[] = {"WAVE", "ANALOG"};
    static const char *const MN[] = {"DUEL", "STEREO", "LAYER"};
    uint32_t e, m, k;
    multis();
    for (e = 0; e < 2; e++)
        for (m = 0; m < 3; m++) {
            uint32_t ei = 0, i;
            int32_t o[2 * CTL];
            power();
            for (i = 0; i < NENGINES; i++)
                if (str_eq(ENGINES[i]->name, EN[e]))
                    ei = i;
            duell_mode_req = (uint8_t)(m == 0 ? DM_SPLIT : m == 1 ? DM_STEREO : DM_LAYER);
            for (k = 0; k < 2; k++) {
                trk[k].eng_req = (uint8_t)ei;
                apply_preset_to(&trk[k], 0);
                trk[k].p[P_VOICE] = 0;                       /* POLY */
            }
            for (i = 0; i < 6; i++) {
                frame();
                budget_poll();
            }
            {
                uint32_t nv = trk_nvoice(&trk[0]), ne = trk_nvoice_eng(&trk[0]);
                for (i = 0; i < 4; i++) {
                    trk_note_on(&trk[0], 48u + 4u * i, 100);
                    mix_block(o, CTL);
                }
                printf("poly: %-6s %-6s P_VOICE %d  voices allowed %u (engine %u, cap %u, budget %u)  held after 4 note-ons %u\n", EN[e], MN[m], trk[0].p[P_VOICE], nv, ne, vcap[0], bnotes[0], held(&trk[0]));
                fails += held(&trk[0]) < 2u;
            }
        }
    printf("poly: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
