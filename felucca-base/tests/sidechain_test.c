/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.22: the sidechain's MIDI setup is a DEVICE setting (GLO > SIDECHAIN; fx.c sc_midi, ui.c sc_get / sc_put, the settings record's bytes favorites.factory[15][14..17]), on the
 * firmware's sources (the UI of ui_test.c) against simulated NOR flash.
 *   build/host/sidechain_test         (host_tests.sh sidechain)
 * 1. the defaults (channel 10, note 36 = C2 for both parts); a change is saved ~2 s later in the settings record's bytes 14..17 (a channel 1 + 0..15, a note 1 + 0..127), the same record and
 *    size; a power-on reads them back; an older record (zeros) = the defaults; every byte value in each of the four bytes (1024 power-ons) reads safely (out of range = the default); ERASE DATA
 *    (no settings) = the defaults.
 * 2. a device setting: a patch load (factory, saved), a multi load (the factory multis, a saved one), the project capture / restore and a morph leave them alone, and the stored forms (a multi
 *    record, a project) are bit for bit the same whatever the setup is.
 * 3. DUCK SRC is PUMP, PART, MIDI or OFF (stored 0..3; 0.2.24: OFF, the new-sound default): a stored value past that is clamped on load (a record of the old CH1..16 form reads as OFF), the knob order, the setup is editable with every SRC. */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if ((off & 0xFFu) + n > 256u)
        return -1;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    printf("sidechain: %-124s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
    return ok;
}
static void tails_over(void)
{
    uint32_t k, i;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NVOICE; i++)
            trk[k].v[i].active = 0;
}
static void boot(void)                                  /* main.c's start: the RAM gone, the flash read, the settings */
{
    ui_power_on();
    tails_over();
    memset(&settings, 0, sizeof settings);
    first_boot_pending = 0;
    memset(&persist_saved, 0, sizeof persist_saved);
    persist_pending = 0;
    cmp_set = 0;
    cmp_last = 0xFFFFFFFFu;
    memset(song.cp, 0, sizeof song.cp);
    memset(ui_sc_set, 0, 4);                            /* (the RAM is gone) */
    sc_midi[0] = sc_midi[2] = 77; sc_midi[1] = sc_midi[3] = 99;
    flash_ok = 1;
    persist_boot();
    settings_init();
    multi_boot();
    frame();
}
static void flush(void)                                 /* the settings written (settings_poll, nothing sounding) */
{
    uint32_t n;
    tails_over();
    for (n = 0; n < 8u; n++) {
        fm1_ms += 2000u;
        settings_poll();
    }
}
static int is(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { return sc_midi[0] == a && sc_midi[1] == b && sc_midi[2] == c && sc_midi[3] == d; }

int main(void)
{
    static persist_t p;
    uint32_t v, r, ok, k;
    memset(nor, 0xFF, sizeof nor);
    boot();
    /* 1. the setting */
    expect("defaults of a new unit: P1 CH 10, P1 NOTE 36, P2 CH 10, P2 NOTE 36 (the bytes 0 = the default)", is(9, 36, 9, 36) && !ui_sc_set[0] && !ui_sc_set[1] && !ui_sc_set[2] && !ui_sc_set[3]);
    sc_put(0, 12); sc_put(1, 0); sc_put(2, 2); sc_put(3, 127);
    frame();
    expect("sc_put: the audio side's copy and the bytes (channel + 1, note + 1) at once, a save queued", is(12, 0, 2, 127) && ui_sc_set[0] == 13u && ui_sc_set[1] == 1u && ui_sc_set[2] == 3u && ui_sc_set[3] == 128u && ui.mset_dirty);
    fm1_ms += 2100u;
    frame();
    flush();
    ok = st_load(OBJ_SETTINGS, &p, sizeof p) > 0 && p.magic == PERSIST_MAGIC && p.favorites.factory[15][14] == 13u && p.favorites.factory[15][15] == 1u && p.favorites.factory[15][16] == 3u &&
         p.favorites.factory[15][17] == 128u;
    expect("saved ~2 s later in the settings record's bytes 14..17 (the same record: magic PER4, the MIDI MAP's size)", ok && sizeof(persist_t) == 4u * 4u + sizeof(panel_t) + 4u + 16u * 32u + 4u * 5u + 2u * MAP_N + 64u);
    boot();
    expect("a power-on reads them back", is(12, 0, 2, 127));
    ok = 1;
    for (v = 0; v < 4u; v++) {                          /* an older record: zeros there */
        memset(&p, 0, sizeof p);
        st_load(OBJ_SETTINGS, &p, sizeof p);
        p.favorites.factory[15][14] = p.favorites.factory[15][15] = p.favorites.factory[15][16] = p.favorites.factory[15][17] = 0;
        st_save(OBJ_SETTINGS, &p, sizeof p);
        boot();
        ok &= is(9, 36, 9, 36);
    }
    expect("a record with zeros there (every older one) reads as the defaults", ok);
    st_load(OBJ_SETTINGS, &p, sizeof p);
    ok = 1;
    for (r = 0; r < 4u; r++)
        for (v = 0; v < 256u; v++) {
            st_load(OBJ_SETTINGS, &p, sizeof p);
            p.favorites.factory[15][14] = p.favorites.factory[15][15] = p.favorites.factory[15][16] = p.favorites.factory[15][17] = 0;
            p.favorites.factory[15][14 + r] = (uint8_t)v;
            st_save(OBJ_SETTINGS, &p, sizeof p);
            boot();
            ok &= sc_midi[0] <= 15u && sc_midi[2] <= 15u && sc_midi[1] <= 127u && sc_midi[3] <= 127u;
            if (r == 0u) ok &= sc_midi[0] == (v >= 1u && v <= 16u ? v - 1u : 9u);
            if (r == 1u) ok &= sc_midi[1] == (v >= 1u && v <= 128u ? v - 1u : 36u);
            if (r == 2u) ok &= sc_midi[2] == (v >= 1u && v <= 16u ? v - 1u : 9u);
            if (r == 3u) ok &= sc_midi[3] == (v >= 1u && v <= 128u ? v - 1u : 36u);
        }
    expect("every byte value 0..255 in each of the four bytes (1024 power-ons): in range, out of range = the default, the rest as it was", ok);
    sc_put(0, 3); sc_put(3, 50);
    frame();
    fm1_ms += 2100u;
    frame();
    flush();
    memset(nor, 0xFF, sizeof nor);                      /* ERASE DATA: no valid settings; the RAM is gone at the restart */
    boot();
    expect("after ERASE DATA (no settings, the restart): the defaults", is(9, 36, 9, 36));

    /* 2. a device setting: loads leave it alone */
    boot();
    duell_mode_force(DM_STEREO);
    sc_put(0, 4); sc_put(1, 60); sc_put(2, 5); sc_put(3, 61);
    trk[0].p[P_DKSRC] = 2; trk[1].p[P_DKSRC] = 1;
    frame();
    ok = 1;
    for (k = 0; k < 40u; k++) {                         /* a factory sound into each part, then a saved one */
        sl_load(k);
        ok &= is(4, 60, 5, 61);
    }
    expect("a patch load (40 sounds) leaves the setup as it is", ok);
    {
        static uint8_t rec[MU_REC_MAX + 8u], rec2[MU_REC_MAX + 8u], blk[NSHOWN][MB_N], amt[NSHOWN], set[6];
        uint32_t cut = 0, len, len2;
        project_t q;
        memset(blk, 0, sizeof blk);
        project_capture(&q);
        len = mu_pack(rec, &q, blk, &cut);
        sc_put(0, 15); sc_put(1, 7); sc_put(2, 9); sc_put(3, 99);
        project_capture(&q);
        len2 = mu_pack(rec2, &q, blk, &cut);
        expect("a multi record is bit for bit the same whatever the setup is (nothing of it is stored in it)", len == len2 && !memcmp(rec, rec2, len));
        ok = 1;
        for (k = 0; k < 16u; k++) {
            if (df_of(k) >= DF_N)
                continue;
            project_load(k);
            frame();
            ok &= is(15, 7, 9, 99);
        }
        expect("a multi load (every factory multi) leaves the setup as it is", ok);
        project_save_as(10, "SC TEST");
        sc_put(0, 1);
        project_load(10);
        frame();
        expect("  .. nor does a saved multi", is(1, 7, 9, 99));
        (void)amt; (void)set;
        project_capture(&q);
        {
            static project_t q2;
            static project_store_t st1, st2;
            q.sum = proj_sum(&q);
            sc_put(2, 0);
            project_capture(&q2);
            q2.sum = proj_sum(&q2);
            ok = proj_pack(&st1, &q) && proj_pack(&st2, &q2) && !memcmp(&st1, &st2, sizeof st1);
            project_restore_runtime(&q);
            frame();
            expect("the project (pack, restore) holds nothing of it: bit for bit the same, a restore leaves the setup", ok && is(1, 7, 0, 99));
        }
    }

    /* 3. DUCK SRC */
    boot();
    expect("DUCK SRC: PUMP, PART, MIDI, OFF (stored 0..3; 0.2.24: OFF, the default of a new sound; the sparse store's own default stays PUMP); the note is retired", TP[P_DKSRC].min == 0 && TP[P_DKSRC].max == 3 && TP[P_DKSRC].def == 3 && str_eq(N_DKSRC[2], "MIDI") &&
           str_eq(N_DKSRC[3], "OFF") && up_def(0, P_DKSRC) == 0 && str_eq(TP[P_CUT2].label, "CUTOFF"));   /* (0.2.24: the retired NOTE id was the EQ switch; the TWIN FILTER's CUTOFF 2 now) */
    ok = param_fit(&TP[P_DKSRC], 7) == 3 && param_fit(&TP[P_DKSRC], 17) == 3 && param_fit(&TP[P_DKSRC], 2) == 2 && param_fit(&TP[P_DKSRC], 0) == 0;
    expect("a stored SRC past OFF (a record of the old CH1..CH16 form) is clamped on load: OFF (0.2.24; it read MIDI up to 0.2.23)", ok);
    for (k = 0; k < 3u; k++) {
        trk[0].p[P_DKSRC] = (int16_t)k;
        go_title("SIDECHAIN");
        frame();
        sc_put(0, 8);
        turn(EN_K1, 2);
        ok = sc_midi[0] == 10u;
        expect("  .. the setup is editable whatever the SRC is (K1 with SRC PUMP / PART / MIDI)", ok);
        sc_put(0, 9);
    }
    printf("sidechain: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
