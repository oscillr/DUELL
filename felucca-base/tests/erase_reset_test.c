/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.1 (item 91, owner: ERASE must erase EVERYTHING the user stored): the installer's "erase data" end to end, store by store.
 *   1. a brand-new unit (blank flash) boots and saves its first settings: the reference;
 *   2. every store is filled with non-default values through the real paths (theme, preferences, favourites, MIDI MAP, button map,
 *      autosave, 2 pattern slots, all 128 patch slots, 2 user multis, a user wavetable, an FM6 voice bank, the velocity calibration, which must SURVIVE);
 *   3. the exact frame the install page sends (tests/erase_page_bytes.mjs runs web/fm1backup.js and prints it) goes to the real
 *      ERASE_ALL handler (editor_erase.c, with editor.c's framing);
 *   4. the restart as the unit does it: a CHIP RESET (fm1_reboot) that keeps the .noinit RAM (`settings` = the theme, `panel` = the
 *      button map); the first start runs (guide pending, M01 loaded, settings saved); then a power cycle (.noinit garbage);
 *   5. one line per store: wiped (reads as on the new unit); owner, item 103: the velocity calibration is the unit's and is KEPT.
 *   erase_reset_test "f0 7d 46 4c 50 45 52 41 53 45 20 41 4c 4c f7"
 * (erase_test.c covers the menu path, the dialog, power cuts and the token; its boot() clears `settings`, a power-on, so it
 *  could not see the chip reset keeping the theme.) */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define _FORTIFY_SOURCE 0
#define HOST_IRQ_HOOK 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
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
static uint8_t ed_out[16];                   /* editor.c's reply buffer and writer, for the real ERASE_ALL handler (editor_erase.c) */
static uint32_t ed_n;
static void ed_b(uint32_t v) { if (ed_n < sizeof ed_out) ed_out[ed_n++] = (uint8_t)(v & 0x7Fu); }
#include "../firmware/src/editor_erase.c"
static void ed_frame(const uint8_t *f, uint32_t n)   /* editor.c ed_handle's framing: F0 | 7D 46 4C cmd args | F7 */
{
    ed_n = 5;
    if (n >= 4u && f[0] == 0x7D && f[1] == 0x46 && f[2] == 0x4C)
        ed_erase_handle(f[3], f + 4, n - 4u);
}

static uint32_t new_unit_seen;              /* first_boot_pending as persist_boot left it (the first frame consumes it) */
static void boot(int power_off)              /* main.c fm1_main's order. power_off: .noinit is garbage; else (a chip reset) .noinit as it was left */
{
    static struct { uint32_t magic, palette, rsv, zoom; } keep_settings;
    static panel_t keep_panel;
    memcpy(&keep_settings, &settings, sizeof settings);
    keep_panel = panel;
    ui_power_on();                           /* (the host's power-on also sets `settings` and `panel`: the .noinit RAM is put back below) */
    if (power_off) {
        memset(&settings, 0x5A, sizeof settings);
        memset(&panel, 0x5A, sizeof panel);
    } else {
        memcpy(&settings, &keep_settings, sizeof settings);
        panel = keep_panel;
    }
    cmp_set = 0;                             /* (.bss / .data on the unit: as at any reset; the host's power-on leaves these two) */
    cmp_last = 0xFFFFFFFFu;
    flash_ok = 1;
    erase_at = 0;
    memset(&persist_saved, 0, sizeof persist_saved);
    persist_pending = 0;
    first_boot_pending = 0;
    memset(&as, 0, sizeof as);
    persist_boot();
    new_unit_seen = first_boot_pending;
    settings_init();
    panel_init();
    frame();
}
static void settle(void)                     /* 6 s of main loop: first_boot_poll, settings_poll, the pattern slots' writes */
{
    uint32_t i;
    for (i = 0; i < 40; i++) { ps_retry_ms = 0; frames(150); settings_poll(); }
}
static int stored(uint32_t obj) { st_hdr_t h; return st_current(obj, &h) >= 0; }
static int blank(uint32_t off) { uint32_t i; for (i = 0; i < 4096u; i++) if (nor[off + i] != 0xFFu) return 0; return 1; }
static int velo_is(uint8_t tag) { uint8_t v[160]; return velo_cal_load(v, sizeof v) == 128 && v[0] == tag; }
static persist_t rec_ref, rec_now;
static int rec(persist_t *p) { return st_load(OBJ_SETTINGS, p, sizeof *p) == (int)sizeof *p; }
static int same_rec(const persist_t *a, const persist_t *b) { return !memcmp(a, b, sizeof *a); }   /* the whole settings record, the button map in it too */
static int bad;
static void row(const char *store, const char *now, int ok)
{
    printf("  %-44s %-34s %s\n", store, now, ok ? "ok" : "FAIL");
    bad += !ok;
}

int main(int argc, char **argv)
{
    static uint8_t junk[600];
    uint8_t fr[64];
    uint32_t n = 0, i, damaged = 0;
    panel_t cal;
    char *p;
    if (argc < 2) return 2;
    for (p = argv[1]; *p && n < sizeof fr;) { char *e; unsigned long v = strtoul(p, &e, 16); if (e == p) break; fr[n++] = (uint8_t)v; p = e; }
    if (n < 6 || fr[0] != 0xF0 || fr[n - 1] != 0xF7) { printf("bad frame\n"); return 2; }
    for (i = 0; i < sizeof junk; i++) junk[i] = (uint8_t)(i * 7u + 1u);

    /* 1. a brand-new unit: the reference */
    memset(nor, 0xFF, sizeof nor);
    boot(1);
    settle();
    if (!rec(&rec_ref)) { printf("no reference settings record\n"); return 1; }
    printf("new unit: theme %s, settings record saved\n", UI_PALETTES[settings.palette].name);

    /* 2. fill every store */
    settings.palette = 5u;                                   /* theme OCEAN */
    ui_prefs ^= PREF_ACCEL;                                  /* a preference (KNOB ACCEL) */
    favorites.user = 0x15u;                                  /* favourite patches */
    favorites.factory[15][28] = (uint8_t)((favorites.factory[15][28] & ~3u) | 2u);   /* CLICK ON */
    midi_map[20] = (uint16_t)(midi_map[20] ^ 0x11u);         /* a MIDI MAP line (learned CC) */
    cal = panel; i = cal.btn[B_FX]; cal.btn[B_FX] = cal.btn[B_SCL]; cal.btn[B_SCL] = (uint8_t)i;   /* a HARDWARE CALIBRATION of the buttons */
    panel = cal;
    settings_save(); settle();
    st_save(OBJ_AUTOSAVE, junk, 200);                        /* the autosave (the last session) */
    set_engine_of(&trk[0], 0u);
    apply_preset_to(&trk[0], 3u);
    for (i = 0; i < 128u; i++)                               /* every patch slot (all 22 chunks) */
        up_store_trk(&trk[0], i, i == 7u ? "MY BASS" : "FILL", CAT_PAD);
    ps_save_trk(&trk[0], 2u, "ACID DUB");                    /* pattern slots */
    ps_save_trk(&trk[0], 12u, "DEEP");
    song.playing = 0; transport_req = 0;
    project_save(4u);                                        /* user multis (M05 over a factory one, M12) */
    project_save(11u);
    st_save(OBJ_WTAB0 + 1u, junk, sizeof junk);              /* a user wavetable (USR2) */
    for (i = 0; i < 4096u; i++) nor[0xE7000u + i] = (uint8_t)(i * 3u);   /* an FM6 voice bank (its sector) */
    { uint8_t v[128]; for (i = 0; i < 128u; i++) v[i] = (uint8_t)(i * 5u + 1u); v[0] = 0x2Au; velo_cal_save(v, 128u); }   /* the velocity calibration */
    settle();
    printf("filled: theme %s, settings %d, autosave %d, patches %u, patterns %d %d, multis %d %d, wavetable %d, FM6 bank %d, velocity %d\n",
           UI_PALETTES[settings.palette].name, stored(OBJ_SETTINGS), stored(OBJ_AUTOSAVE), up_count(), ps_changed(2), ps_changed(12),
           stored(OBJ_MULTI0 + 2u), stored(OBJ_MULTI0 + 5u), stored(OBJ_WTAB0 + 1u), !blank(0xE7000u), velo_is(0x2Au));

    /* 3. the page's frame, the real handler */
    ed_frame(fr + 1, n - 2u);
    printf("erase: page frame %u bytes, unit reply rc %d (0 = erased)\n", n, ed_n > 5 ? ed_out[5] : -1);
    bad += !(ed_n > 5 && ed_out[5] == 0);

    /* 4. chip reset (RAM kept), the first start, then a power cycle */
    boot(0);
    printf("after the restart: theme %s, first-start guide pending %d\n", UI_PALETTES[settings.palette].name, (int)new_unit_seen);
    bad += !new_unit_seen;
    settle();
    printf("first start: settings record saved %d\n", rec(&rec_now));
    boot(1);
    settle();

    /* 5. the table */
    printf("store                                         after erase + restart + power cycle\n");
    row("colour theme (flash + RAM copy)", UI_PALETTES[settings.palette].name, settings.palette == UI_DEFAULT_INDEX);
    row("settings record: preferences, favourites,", "", 1);
    row("  MIDI channels, MIDI MAP / LEARN, scale ...", rec(&rec_now) && same_rec(&rec_now, &rec_ref) ? "same as a new unit" : "DIFFERS from a new unit",
        rec(&rec_now) && same_rec(&rec_now, &rec_ref));
    row("favourite patches (RAM)", favorites.user ? "left" : "none", !favorites.user);
    row("autosave (last session)", st_load(OBJ_AUTOSAVE, junk, sizeof junk) == 200 && junk[1] == 8u ? "OLD ONE LEFT" : "gone (new unit's own)",
        !(st_load(OBJ_AUTOSAVE, junk, sizeof junk) == 200 && junk[1] == 8u));
    for (i = 0; i < sizeof junk; i++) junk[i] = (uint8_t)(i * 7u + 1u);
    row("user patches (128 slots, 22 chunks)", up_count() ? "LEFT" : "none: factory sounds", up_count() == 0u);
    row("pattern slots", ps_changed(2) || ps_changed(12) ? "LEFT" : "the 32 default patterns", !ps_changed(2) && !ps_changed(12));
    row("user multis (M05, M12)", stored(OBJ_MULTI0 + 2u) || stored(OBJ_MULTI0 + 5u) ? "LEFT" : "factory M05, M12 back",
        !stored(OBJ_MULTI0 + 2u) && !stored(OBJ_MULTI0 + 5u) && project_factory(4u) && project_factory(11u));
    for (i = 0; i < DF_N; i++) {
        uint8_t blk[NSHOWN][MB_N], amt[NSHOWN], set[6];
        if (!mu_unpack(&proj_scratch, blk, amt, set, DM_REC + DM_AT[i], DM_LEN[i])) damaged++;
    }
    {
        uint32_t fact = 0;
        for (i = 0; i < 16u; i++) fact += project_factory(i);
        row("factory multis M01..M16 (load, not DAMAGED)", damaged ? "DAMAGED" : "all 16 load", !damaged && fact == 16u && DF_N == 16u);
    }
    row("user wavetables (USR1..USR3)", stored(OBJ_WTAB0) || stored(OBJ_WTAB0 + 1u) || stored(OBJ_WTAB0 + 2u) ? "LEFT" : "EMPTY",
        !stored(OBJ_WTAB0) && !stored(OBJ_WTAB0 + 1u) && !stored(OBJ_WTAB0 + 2u));
    row("FM6 voice bank (B1..B32)", blank(0xE7000u) && !fm6_bank_used ? "none" : "LEFT", blank(0xE7000u) && !fm6_bank_used);
    row("button / knob map (BUTTON MAP)", !memcmp(&panel, &PANEL_DEFAULT, sizeof panel) ? "the built-in map" : "OLD MAP LEFT", !memcmp(&panel, &PANEL_DEFAULT, sizeof panel));
    { uint8_t v[160]; int n = velo_cal_load(v, sizeof v), ok = n == 128 && v[0] == 0x2Au && v[1] == 6u && v[127] == (uint8_t)(127u * 5u + 1u); row("key velocity calibration (KEPT, item 103)", ok ? "kept" : "LOST OR CHANGED", ok);  }
    printf("erase_reset: %s\n", bad ? "FAIL" : "ok");
    return bad != 0;
}
