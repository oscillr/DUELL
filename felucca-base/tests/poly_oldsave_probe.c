/* SPDX-License-Identifier: GPL-3.0-only */
/* HANDOFF_V2 item 89 (owner: test3 plays one note). Build the SAME file against the 1.0 sources (add -DOLD) and against these: "w FILE" writes the old save (M02 NIGHT ORGAN as the
 * session, settings + autosave), "r FILE" boots from it and plays 6 keys with part 1 on WAVE / LOFI / ANALOG / FM6 (POLY, LAYER). Prints the NOTES budget, the AVAILABLE VOICES number and the voices
 * that sound; "with key sensing on" = the device builds with KEY VELOCITY or AFTERTOUCH ON (+57, budget.c bd_global). Same cc line as poly_probe_test.c. */
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
static uint32_t eng_named(const char *n) { uint32_t e; for (e = 0; e < NENGINES; e++) if (str_eq(ENGINES[e]->name, n)) return e; return 0; }
static void fr(void) { frame(); budget_poll(); }
/* 1) "w FILE": as the old firmware, M02 NIGHT ORGAN loaded, settings + autosave written.  2) "r FILE": boot from that flash (settings, autosave), then part 1 -> engine X, POLY,
   6 keys through the key path: notes allowed, the counter, voices sounding; the same with the key sensing's 2 % added (device builds, KEY VELOCITY or AFTERTOUCH ON) */
static uint32_t held(const track_t *t) { uint32_t i, n = 0; for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].gate && t->v[i].stage != 4u; return n; }
static void play(const char *what)
{
    int32_t o[2 * CTL]; uint32_t i, j, mx0 = 0, mx1 = 0, keys[6] = {7, 11, 14, 18, 21, 24}, c0 = 0, ns[2]; bd_t b;
    for (i = 0; i < 12; i++) fr();
#ifndef OLD
    { uint8_t n[2]; head_notes(n); c0 = n[0]; }
#endif
    for (i = 0; i < 6; i++) {
        fm1_in.notes |= 1u << keys[i]; host_notes |= 1u << keys[i];
        for (j = 0; j < 3; j++) { mix_block(o, CTL); fr(); }
        if (held(&trk[0]) > mx0) mx0 = held(&trk[0]);
        if (held(&trk[1]) > mx1) mx1 = held(&trk[1]);
    }
    bd_build(&b); b.fx += 57; budget_solve(&b, ns);
    printf("  %-30s %s/%s VCE %d/%d: notes %u/%u, counter %u, 6 keys -> %u + %u sounding | with key sensing on: %u/%u\n", what, ENGINES[eng_idx(trk[0].eng_req)]->name, ENGINES[eng_idx(trk[1].eng_req)]->name,
           trk[0].p[P_VOICE], trk[1].p[P_VOICE], trk_nvoice(&trk[0]), trk_nvoice(&trk[1]), c0, mx0, mx1, ns[0], ns[1]);
    fm1_in.notes = 0; host_notes = 0;
    for (j = 0; j < 60; j++) { mix_block(o, CTL); fr(); }
}
static void boot(void) { ui_power_on(); flash_ok = 1; persist_boot(); settings_init(); autosave_boot(1); fr(); }
int main(int argc, char **argv)
{
    FILE *f; uint32_t e, k; static const char *const E[] = {"WAVE", "LOFI", "ANALOG", "FM6"};
    if (argc > 2 && argv[1][0] == 'w') {
        memset(nor, 0xFF, sizeof nor); boot(); ui_prefs2 |= PREF_MODE_RECALL >> 16; project_load(1); fr();
        settings_save(); project_capture(&proj_scratch); proj_wire_gen++; proj_pack(&proj_wire, &proj_scratch); st_save(OBJ_AUTOSAVE, &proj_wire, sizeof proj_wire);
        f = fopen(argv[2], "wb"); fwrite(nor, 1, sizeof nor, f); fclose(f); return 0;
    }
    memset(nor, 0xFF, sizeof nor); boot(); duell_mode_req = DM_LAYER;
    for (k = 0; k < 2; k++) { trk[k].eng_req = (uint8_t)eng_named("WAVE"); apply_preset_to(&trk[k], 0); trk[k].p[P_VOICE] = V_POLY; }
    play("erased unit, WAVE both parts");
    f = fopen(argv[2], "rb"); fread(nor, 1, sizeof nor, f); fclose(f);
    boot(); play("old save as restored");
    for (e = 0; e < 4; e++) { char w[48]; boot(); trk[0].eng_req = (uint8_t)eng_named(E[e]); apply_preset_to(&trk[0], 0); trk[0].p[P_VOICE] = V_POLY; sprintf(w, "old save, part 1 -> %s", E[e]); play(w); }
    return 0;
}
