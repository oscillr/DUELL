/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.2: ERASE DATA (src/erase_data.c, MENU > SYSTEM > ERASE DATA; the editor's ERASE_ALL, cmd 80) against simulated NOR flash:
 *   1. the dialog: OCT+ on the row opens it, OCT- / a page button / HOME cancel and write NOTHING (the flash image is identical);
 *      YES while playing is refused (nothing written); the dialog's lines fit the card;
 *   2. YES erases the settings, favourites, autosave, patches (+ shadow copies), multis, pattern slots; the user sample
 *      slots and the FM6 voice bank sector are untouched; the restart is due ~0.7 s later (the main loop reboots); nothing is
 *      written meanwhile (settings_poll / autosave);
 *   3. the next start is a fresh unit: VIOLET, LAYER, factory multi M01 (0.2.13: REESE SWAMP), no patches, the 32 default patterns and the factory multis back;
 *   4. a power cut after any number of erases: every object whole or empty, the settings still valid until the very last
 *      object has gone, the unit boots; running it again finishes;
 *   5. the editor op: only the exact token "ERASE ALL" (wrong token, wrong length: rc 1, nothing written), not while playing (rc 3).
 *   build/host/erase_test            (DUELL/host_tests.sh "erase") */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"

static uint8_t nor[0x100000];
static int dead, erase_cut = -1;                    /* the power goes at the (erase_cut+1)th erase */
static uint32_t n_erase, n_prog;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    if (dead || (erase_cut >= 0 && erase_cut-- == 0)) {
        dead = 1;
        return -8;
    }
    n_erase++;
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (dead)
        return -9;
    n_prog++;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static uint32_t ed_rc = 99, ed_calls;               /* the editor op's reply */
static void ed_b(uint32_t v) { ed_rc = v; ed_calls++; }
#include "../firmware/src/editor_erase.c"

static int bad;
static void ck(const char *what, int ok)
{
    printf("erase: %-110s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static void boot(void)                                /* power-on: the RAM gone, flash read again (persist_boot) */
{
    dead = 0;
    erase_cut = -1;
    ui_power_on();
    flash_ok = 1;
    erase_at = 0;
    memset(&settings, 0, sizeof settings);
    settings.palette = 7777u;
    memset(&persist_saved, 0, sizeof persist_saved);
    persist_pending = 0;
    first_boot_pending = 0;
    memset(&as, 0, sizeof as);
    persist_boot();
    settings_init();
}
static void flush(void)
{
    uint32_t n;
    for (n = 0; n < 8u; n++) {
        ps_retry_ms = 0;
        fm1_ms += 2000u;
        settings_poll();
    }
}
static void fill(void)                                /* every kind of user data, through the real paths */
{
    static uint8_t junk[200];
    uint32_t i;
    for (i = 0; i < sizeof junk; i++)
        junk[i] = (uint8_t)(i * 7u + 1u);
    boot();
    settings.palette = 4u;                            /* COBALT-ish: not VIOLET */
    ui_prefs = 0; ui_rec_prefs = 2u;                  /* (0.2.14: the user's KNOB ACCEL OFF, USB SERIAL ON, CLICK ON, the flag set) */
    ui_prefs2 = (uint8_t)(ui_prefs2 | PREF_DEFAULTS >> 16);
    favorites.user = 0x1234u;
    favorites.factory[0][0] = 0x55u;
    persist_pending = 1;
    flush();
    st_save(OBJ_AUTOSAVE, junk, sizeof junk);
    set_engine_of(&trk[0], 0u);
    apply_preset_to(&trk[0], 3u);
    up_store_trk(&trk[0], 7u, "TEST A", CAT_BASS);
    up_store_trk(&trk[0], 20u, "TEST B", CAT_LEAD);
    for (i = 0; i < 128u; i++)                        /* the cloud plan's job A: all 128 slots, the PATCH store's 22 chunks (the six new ones, 0xBA000..0xC5FFF, too) */
        if (i != 7u && i != 20u)
            up_store_trk(&trk[0], i, "FILL", CAT_PAD);
    {   /* the velocity calibration (OBJ_VELO, 0xC6000 / 0xC7000): 128 bytes, saved twice so both copies hold one */
        uint8_t v[128];
        uint32_t j;
        for (j = 0; j < 128u; j++)
            v[j] = (uint8_t)(j * 5u + 1u);
        velo_cal_save(v, 128u);
        v[0] ^= 1u;
        velo_cal_save(v, 128u);
    }
    ps_save_trk(&trk[0], 2u, "ACID DUB");
    ps_save_trk(&trk[0], 12u, "DEEP");
    flush();
    song.playing = 0;
    transport_req = 0;
    song.g[G_TUNE] = 4321;                            /* (0.2.22: MASTER TUNE 432.1 Hz, kept with the projects) */
    project_save(4u);
    project_save(11u);
    for (i = 0; i < 4096u; i++) {                     /* what must stay: the old sample region beyond the wavetables, the FM6 voice bank sector */
        nor[0xEA000u + i] = (uint8_t)(i ^ 0x5Au);      /* (0xEA000.. and 0xF0000..: the stock firmware's user region, outside every storage.c object AND outside FL_STORE_OK (0x97000..0xDFFFF, 0xE5000..0xE7FFF,
                                                        * 0xFC000..0xFEFFF): the erase cannot reach it even by a wrong address. 0xBA000 and 0xC0000 were the free range until the patch store's chunks 16..21 took 0xBA000..0xC5FFF) */
        nor[0xF0000u + i] = (uint8_t)(i ^ 0xA5u);
        nor[0xE7000u + i] = (uint8_t)(i * 3u);
    }
}
static int all_gone(void)                             /* after a boot: no stored object reads valid */
{
    static uint8_t buf[ST_PAYLOAD_MAX];
    uint32_t o;
    for (o = 0; o < OBJ_COUNT; o++)
        if (!OBJ_RETIRED(o) && st_load(o, buf, sizeof buf) >= 0)
            return 0;
    return 1;
}
static int kept_ok(void)
{
    uint32_t i;
    for (i = 0; i < 4096u; i++)
        if (nor[0xEA000u + i] != (uint8_t)(i ^ 0x5Au) || nor[0xF0000u + i] != (uint8_t)(i ^ 0xA5u) || nor[0xE7000u + i] != (uint8_t)(i * 3u))
            return 0;
    return 1;
}
static void open_dialog(void)
{
    hold(B_HOME);                                     /* HOME held: the menu */
    ui.menu_sel = MI_ERASE;
    ui.menu_row[MI_TAB[MI_ERASE]] = (uint8_t)(MI_ERASE - mtab_first(MI_TAB[MI_ERASE]));
    ui.force = 1;
    frame();
    press(B_OCTUP);
}

int main(void)
{
    static uint8_t snap[0x100000], tok[9] = {'E', 'R', 'A', 'S', 'E', ' ', 'A', 'L', 'L'};
    uint32_t i, total, e0, w0, cuts_ok = 0;
    memset(nor, 0xFF, sizeof nor);
    fill();
    memcpy(snap, nor, sizeof nor);
    ck("filled: settings, autosave, 128 patches (all 22 chunks), 2 pattern slots, 2 multis stored", ({ persist_t tmp_rec; st_load(OBJ_SETTINGS, &tmp_rec, sizeof tmp_rec) > 0; }) &&
       up_count() == 128u && pt_n[12] > 0u && pt_n[PT_NCH - 1u] > 0u && ({ uint8_t v[160]; velo_cal_load(v, sizeof v) == 128 && v[0] == 0u; }) && ps_changed(2) && ps_changed(12) && project_used(4) && project_used(11));

    {   /* velo_cal_load / velo_cal_save (job D's store): the length is 1..128, a smaller buffer is refused, the record that was saved is the record loaded
         * (the two sectors are put back as they were afterwards: the flash image of the checks below is the filled one) */
        static uint8_t keep[8192];
        uint8_t v[200], w[200];
        uint32_t j;
        memcpy(keep, nor + st_sector(OBJ_VELO, 0), 4096u);
        memcpy(keep + 4096u, nor + st_sector(OBJ_VELO, 1), 4096u);
        for (j = 0; j < sizeof v; j++)
            v[j] = (uint8_t)(j * 3u + 7u);
        ck("velocity store: 129 bytes and 0 bytes are refused (-1), 1 byte and 128 bytes are saved; a buffer of 64 does not take the 128; the 128 loads back as saved",
           velo_cal_save(v, 129u) == -1 && velo_cal_save(v, 0u) == -1 && velo_cal_save(v, 1u) == 0 && velo_cal_load(w, sizeof w) == 1 && w[0] == v[0] &&
           velo_cal_save(v, 128u) == 0 && velo_cal_load(w, 64u) == -1 && velo_cal_load(w, sizeof w) == 128 && !memcmp(v, w, 128) && velo_cal_load(w, 0u) == -1);
        memset(nor + st_sector(OBJ_VELO, 0), 0xFF, 4096u);
        memset(nor + st_sector(OBJ_VELO, 1), 0xFF, 4096u);
        ck("  nothing stored (both sectors blank): -1", velo_cal_load(w, sizeof w) == -1);
        memcpy(nor + st_sector(OBJ_VELO, 0), keep, 4096u);
        memcpy(nor + st_sector(OBJ_VELO, 1), keep + 4096u, 4096u);
    }

    /* 1: the dialog */
    ck("ERASE DATA is the last row of the SYSTEM tab, label of at most 12 characters", MI_TAB[MI_ERASE] == MTAB_SYSTEM && MI_ERASE == MI_COUNT - 1u &&
       str_len(MI_NAME[MI_ERASE]) <= 12u && menu_place(MI_ERASE, 0) + 1u == mtab_pages(MTAB_SYSTEM));   /* (0.2.14: on SYSTEM's last page) */
    for (i = 0; i < 5u; i++)
        if (text_w(&AF_S, ERASE_LINES[i]) > 200)
            break;
    ck("the dialog's five lines fit the card (200 px)", i == 5u);
    e0 = n_erase;
    w0 = n_prog;
    open_dialog();
    ck("OCT+ on ERASE DATA opens the dialog, over the menu", ui.menu == 1u && ui.confirm == CF_ERASE_ALL && !erase_busy());
    frame();
    press(B_OCTDN);
    ck("  OCT- (NO): closed, back in the menu, nothing written, flash image identical", !ui.confirm && ui.menu == 1u && n_erase == e0 && n_prog == w0 &&
       !memcmp(nor, snap, sizeof nor) && msg_is("NOTHING ERASED"));
    press(B_OCTUP);
    ck("  and the dialog opens again", ui.confirm == CF_ERASE_ALL);
    press(B_EDIT);
    frames(32);
    ck("  a page button (EDIT): cancels, the menu closes and EDIT opens, nothing erased (DUELL 0.2.2; leaving the menu saves a changed setting as always)",
       !ui.confirm && !ui.menu && cur_page()->fam == FAM_EDIT && !erase_busy() && !memcmp(nor, snap, 0xFC000u));
    open_dialog();
    press(B_HOME);
    ck("  HOME: cancels and leaves the menu; nothing erased (leaving the menu saves a changed setting as always)",
       !ui.confirm && !ui.menu && !erase_busy() && !memcmp(nor, snap, 0xFC000u) && ({ persist_t tmp_rec; st_load(OBJ_SETTINGS, &tmp_rec, sizeof tmp_rec) > 0; }));
    /* 1b: DUELL 0.2.2: a page button pressed while the MENU is open closes it and goes straight to its page, from every tab, ABOUT, TEST and
     * ERASE DATA's dialog; nothing is written to the flash (a dialog is cancelled) */
    {
        static const struct { uint8_t btn, fam; const char *name; } PB[] = {
            {B_EDIT, FAM_EDIT, "EDIT"}, {B_FX, FAM_FX, "FX"}, {B_ENV, FAM_ENV, "ENV"}, {B_LFO, FAM_LFO, "LFO"}, {B_ARP, FAM_ARP, "ARP"},
            {B_SEQ, FAM_SEQ, "SEQ"}, {B_SCL, FAM_SCL, "SCL"}, {B_GLO, FAM_GLO, "GLO"}, {B_SAVE, FAM_SAVE, "SAVE"}};
        static const char *const CTX[] = {"DISPLAY tab", "CONTROL tab", "AUDIO tab", "SYSTEM tab", "ABOUT", "TEST screen", "ERASE DATA dialog"};
        uint32_t c, b, good = 0, n = 0;
        char what[96];
        memcpy(snap, nor, sizeof nor);
        e0 = n_erase;
        w0 = n_prog;
        for (c = 0; c < 7u; c++) {
            uint32_t ok = 1;
            for (b = 0; b < sizeof PB / sizeof PB[0]; b++) {
                go_main();
                ui.confirm = 0;
                ui.menu = 1;
                ui.menu_sel = (uint8_t)(c < 4u ? mtab_first(c) : c == 6u ? MI_ERASE : MI_TEST);
                if (c == 4u) { ui.menu = 2; ui.menu_scroll = 0; about_t0 = fm1_ms; }
                if (c == 5u) { ui.menu = 3; cpu_meter_open(); }
                if (c == 6u) { ui.confirm = CF_ERASE_ALL; ui.confirm_trk = 0; }
                ui.force = 1;
                frames(48);
                press(PB[b].btn);
                frames(32);
                n++;
                good += ok &= !ui.menu && !ui.confirm && !at_home() && cur_page()->fam == PB[b].fam && n_erase == e0 && n_prog == w0 &&
                              !memcmp(nor, snap, sizeof nor) && !erase_busy();
                if (!ok) {
                    str_cpy(what, CTX[c], sizeof what);
                    str_cpy(what + str_len(what), " + ", 4);
                    str_cpy(what + str_len(what), PB[b].name, 8);
                    ck(what, 0);
                    ok = 1;
                }
            }
        }
        ck("a page button (EDIT FX ENV LFO ARP SEQ SCL GLO, SAVE) in the MENU (4 tabs, ABOUT, TEST, ERASE DATA's dialog): menu closed, its page open, flash untouched",
           good == n && n == 63u);
        go_main();
        ui.menu = 1; ui.menu_sel = MI_COLOR; ui.force = 1; frames(48);
        press(B_HOME);
        ck("HOME from the menu: closes it, the main page (as before)", !ui.menu && cur_page()->fam == FAM_TRK && n_prog == w0 && n_erase == e0);
        ui.menu = 0;
    }
    memcpy(snap, nor, sizeof nor);
    open_dialog();
    ck("  (a knob turned: nothing)", ui.confirm == CF_ERASE_ALL);
    turn(EN_K1, 1);
    turn(EN_SELECT, 1);
    ck("  knobs do not answer the dialog: still asking, nothing written", ui.confirm == CF_ERASE_ALL && !memcmp(nor, snap, sizeof nor));
    song.playing = 1;
    press(B_OCTUP);
    ck("  YES while the transport runs: refused (STOP TO ERASE), nothing written", !ui.confirm && msg_is("STOP TO ERASE") && !memcmp(nor, snap, sizeof nor) &&
       !erase_busy());
    song.playing = 0;
    transport_req = 0;

    /* 5 (first half): the editor op refuses */
    {
        static const uint8_t wrong[9] = {'E', 'R', 'A', 'S', 'E', ' ', 'A', 'L', 'M'}, longer[10] = {'E', 'R', 'A', 'S', 'E', ' ', 'A', 'L', 'L', '!'};
        ck("editor: not the op's id -> not taken", ed_erase_handle(79, tok, 9) == 0);
        ed_rc = 99;
        ck("editor: a wrong token: rc 1", ed_erase_handle(ED_ERASE_ALL, wrong, 9) == 1 && ed_rc == 1u);
        ed_rc = 99;
        ck("  a short payload: rc 1", ed_erase_handle(ED_ERASE_ALL, tok, 8) == 1 && ed_rc == 1u);
        ed_rc = 99;
        ck("  a long payload: rc 1", ed_erase_handle(ED_ERASE_ALL, longer, 10) == 1 && ed_rc == 1u);
        ed_rc = 99;
        ck("  no payload: rc 1", ed_erase_handle(ED_ERASE_ALL, tok, 0) == 1 && ed_rc == 1u);
        song.playing = 1;
        ed_rc = 99;
        ck("  the right token while the transport runs: rc 3", ed_erase_handle(ED_ERASE_ALL, tok, 9) == 1 && ed_rc == 3u);
        song.playing = 0;
        transport_req = 0;
        ck("  nothing written by any of them, no restart due", !memcmp(nor, snap, sizeof nor) && !erase_busy());
    }

    /* 2 + 3: YES, through the dialog */
    memcpy(nor, snap, sizeof nor);
    boot();
    song.playing = 0;
    open_dialog();
    ck("YES: everything stored is gone (settings, autosave, patches, multis, pattern slots), the restart pending", ui.confirm == CF_ERASE_ALL);
    press(B_OCTUP);
    ck("  the dialog closes with ERASED, RESTARTING; every storage object reads empty", !ui.confirm && msg_is("ERASED, RESTARTING") && erase_busy() && all_gone());
    ck("  the old sample region (0xA8000, 0xB4000: free; 0xA0000..0xA5FFF are the user wavetables, erased with the rest) and the FM6 voice bank are untouched", kept_ok());
    ck("  the restart is not due at once (the message is seen), nothing is written meanwhile", !erase_restart_due());
    {
        uint32_t p0 = n_prog, e1 = n_erase;
        persist_pending = 1;
        settings.palette = 3u;
        ps_dirty = 1;
        for (i = 0; i < 40u; i++) {
            fm1_ms += 16u;
            settings_poll();
            autosave_poll();
        }
        ck("  settings_poll / autosave_poll write nothing over the erased flash", n_prog == p0 && n_erase == e1 && all_gone());
    }
    fm1_ms += 800u;
    ck("  ~0.7 s later the restart is due (main.c reboots; the browser build reloads)", erase_restart_due());
    boot();
    ck("the next start is a new unit: no valid settings (first start), VIOLET", first_boot_pending == 1u && settings.palette == UI_DEFAULT_INDEX &&
       str_eq(UI_PALETTES[settings.palette].name, "VIOLET"));
    if (autosave_boot(1))
        first_boot_pending = 0;
    ck("  no autosave to restore", first_boot_pending == 1u);
    ck("  MASTER TUNE is 440.0 Hz again (0.2.22: G_TUNE 4400; the projects that held 432.1 are gone)", song.g[G_TUNE] == 4400 && menu_get(MI_TUNE) == 247u &&
       str_eq(menu_vname(MI_TUNE, menu_get(MI_TUNE)), "440.0"));
    ck("  0.2.14's defaults: KNOB ACCEL ON, CLICK REC, USB SERIAL OFF (the user's OFF / ON / ON before the erase gone)",
       (ui_prefs & PREF_ACCEL) && (ui_prefs & PREF_SERIAL_OFF) && rp_get(RP_CLICK) == 1u && (ui_prefs2 & (PREF_DEFAULTS >> 16)));
    frames(160);
    ck("  MODE is LAYER and factory multi M01 (REESE SWAMP since 0.2.13) is loaded", duell_mode_req == DM_LAYER && project_cur_slot() == 0u && first_boot_pending == 0u &&
       str_eq(proj_name, "REESE SWAMP"));
    ck("  no patches, no user favourites; the pattern slots hold their 32 defaults again (none saved, none empty)", up_count() == 0u && !ps_changed(2) && !ps_changed(12) && favorites.user == 0u &&
       ({ char b0[13], b1[13]; ps_name(2, b0); ps_name(31, b1); str_eq(b0, PATTERNS[2].name) && str_eq(b1, PATTERNS[31].name); }));
    {   /* the sixteen new patch chunks (OBJ_PATCHX0..15, 0xA6000..0xC5FFF) are blank sectors again, the planted sectors outside every object untouched */
        uint32_t o, i, blank = 1;
        for (o = 0; o < PT_NCH; o++)
            for (i = 0; i < 2u; i++) {
                uint32_t b = st_sector(pt_obj(o), i), j;
                for (j = 0; j < 4096u; j++)
                    blank &= nor[b + j] == 0xFFu;
            }
        for (i = 0; i < 2u; i++)
            for (o = 0; o < 4096u; o++)
                blank &= nor[st_sector(OBJ_VELO, i) + o] == 0xFFu;
        ck("  all 22 patch chunks (44 sectors, 0xA6000..0xC5FFF among them) and both sectors of the velocity store are blank (the six new chunks and OBJ_VELO are in the erase order)",
           blank && kept_ok() && ({ uint8_t v[160]; velo_cal_load(v, sizeof v) < 0; }));
    }
    ck("  the factory multis M01..M16 show again (nothing stored in the slots; M05 and M12 were saved over)",
       project_factory(0) && project_factory(3) && project_factory(4) && project_factory(9) && project_factory(11) && project_factory(15));
    frames(2400);
    ck("  the settings are written again (the next start is an ordinary one)", ({ persist_t tmp_rec; st_load(OBJ_SETTINGS, &tmp_rec, sizeof tmp_rec) > 0; }));

    /* 4: a power cut after k erases (k = 0 .. all): each object whole as it was or empty, never an older copy; the settings
     * valid, as they were, until the last erase; the unit boots; running ERASE DATA again finishes */
    {
        static uint8_t ref[OBJ_COUNT][ST_PAYLOAD_MAX], got[ST_PAYLOAD_MAX];
        int reflen[OBJ_COUNT];
        uint32_t k, o, good = 0;
        memcpy(nor, snap, sizeof nor);
        for (o = 0; o < OBJ_COUNT; o++)
            reflen[o] = OBJ_RETIRED(o) ? -1 : st_load(o, ref[o], sizeof ref[o]);
        boot();
        n_erase = 0;
        total = erase_data_run() == 0 ? n_erase : 0;
        ck("  (reference run: rc 0; it erased only the sectors that held data: fewer than all 64)", total > 0 && total < 64u);
        for (k = 0; k <= total + 1u; k++) {
            int rc, objs = 1;
            memcpy(nor, snap, sizeof nor);
            boot();
            erase_cut = (int)k;
            rc = erase_data_run();
            boot();                                   /* the power came back */
            for (o = 0; o < OBJ_COUNT; o++) {
                int n;
                if (OBJ_RETIRED(o))
                    continue;
                n = st_load(o, got, sizeof got);
                if (n >= 0 && (n != reflen[o] || memcmp(got, ref[o], (size_t)n)))
                    objs = 0;                         /* neither as it was nor empty */
            }
            good += objs && kept_ok() && (k < total ? (rc == 4 && first_boot_pending == 0u && st_load(OBJ_SETTINGS, got, sizeof got) == reflen[OBJ_SETTINGS])
                                                    : (rc == 0 && first_boot_pending == 1u && all_gone()));
            if (k < total) {                          /* and a second run finishes it */
                erase_cut = -1;
                good += erase_data_run() == 0 && all_gone();
            } else
                good++;
        }
        ck("  a cut after every number of erases (0..N+1): objects whole or empty, settings valid until the end, a second run finishes",
           good == 2u * (total + 2u));
    }

    /* 6 (DUELL 0.2.2): MENU settings are saved when they change, not only when the menu is left. A knob row (COLOR): once the knob has
     * rested ~2 s (the AMOUNT knob's debounce); a two-value switch (KNOB ACCEL): at once; the menu stays open; a power cycle then
     * keeps both. Leaving the menu after changes shows no message. Several rows turned: all kept. */
    {
        uint32_t pal0, pal1;
        memset(nor, 0xFF, sizeof nor);
        boot();
        persist_pending = 1;
        flush();
        pal0 = settings.palette;
        hold(B_HOME);
        ui.menu_sel = MI_COLOR;
        ui.force = 1;
        frame();
        turn(EN_K1, 1);
        pal1 = settings.palette;
        ck("MENU > COLOR: turned, the menu stays open, the palette changed", ui.menu == 1u && pal1 != pal0);
        frames(500);
        boot();
        ck("  a power cut 0.5 s after the turn: not saved yet (the knob may still be moving)", settings.palette == pal0);
        hold(B_HOME);
        ui.menu_sel = MI_COLOR;
        ui.force = 1;
        frame();
        turn(EN_K1, 1);
        pal1 = settings.palette;
        frames(2200);
        ck("  after the knob rests 2 s: written, with the menu still open", ui.menu == 1u && !persist_pending && persist_saved.palette == palette_to_stored(pal1));
        boot();
        ck("  a power cycle with the menu never left: the colour is kept", settings.palette == pal1);
        hold(B_HOME);
        ui.menu_sel = MI_ACCEL;
        ui.force = 1;
        frame();
        e0 = n_erase;
        press(B_OCTDN);                                 /* (0.2.14: ON on a new unit; OFF is the change) */
        ck("MENU > KNOB ACCEL (a two-value switch): saved at once", ui.menu == 1u && !(ui_prefs & PREF_ACCEL) && n_erase > e0 && !persist_pending);
        boot();
        ck("  a power cycle right after: OFF is kept (and the colour)", !(ui_prefs & PREF_ACCEL) && settings.palette == pal1);
        hold(B_HOME);
        ui.menu_sel = MI_PREVIEW;
        ui.force = 1;
        frame();
        press(B_OCTUP);
        ui.menu_sel = MI_COLOR;
        turn(EN_K1, 1);
        pal0 = settings.palette;
        frames(2200);
        boot();
        ck("  several settings changed in one visit (STEP PREVIEW, COLOR): all kept", (ui_prefs2 & (PREF_PREVIEW >> 16)) && !(ui_prefs & PREF_ACCEL) && settings.palette == pal0);
        hold(B_HOME);
        ui.menu_sel = MI_ACCEL;
        ui.force = 1;
        frame();
        press(B_OCTUP);
        press(B_HOME);
        ck("leaving the menu after a change shows no message", !ui.menu && ui.msg_t == 0u);
    }
    return bad;
}
