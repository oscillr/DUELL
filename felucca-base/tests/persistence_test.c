/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Firmware save flows against NOR flash: deferred settings, retries, and failed-save rollback. */
#ifndef FELUCCA_CDC
#define FELUCCA_CDC 1                                    /* (the developer builds' MENU: USB SERIAL is a row there; the product has none, tests/release_test.c) */
#endif
#define main hostsim_main
#include "hostsim.c"
#undef main

static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static uint32_t irq_start_race, irq_races;
static void fm1_irq_off(void)
{
    if (irq_start_race) {                         /* audio serviced PLAY immediately before main masks it */
        irq_start_race = 0;
        events_block(CTL);
        irq_races++;
    }
}
static void fm1_irq_on(void) {}
static void lcd_sync(void) {}
static void lcd_power(uint32_t s) { (void)s; }   /* (MENU > SCREEN OFF: lcd.c) */
static void lcd_wake_now(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#define FELUCCA_FLASH 1
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/menu_items.c"
static void panel_setup(void) {}

static uint8_t nor[0x100000], flash_ok = 1;
static int erase_error, fail_after = -1;
static uint32_t erases, as_erases;               /* (as_erases: the autosave's two sectors, 0xE5000 / 0xE6000) */
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    erases++;
    as_erases += off >= 0xE5000u && off < 0xE7000u;
    if (erase_error) return -8;
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (fail_after == 0) return -9;
    if (fail_after > 0) fail_after--;
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint8_t jedec_ok;                          /* (0.2.14 defaults test: persist_boot finds the flash part) */
static uint32_t fl_jedec_ram(void) { return jedec_ok ? 0x856014u : 0u; }
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"

enum { ED_UI_STATE = 34, ED_UI_SET, ED_UI_PALETTES, ED_FAV_GET, ED_FAV_SET };
static uint8_t ed_rep[64];                        /* the reply's bytes (MENU_SET) */
static uint32_t ed_repn;
static void ed_b(uint32_t n) { if (ed_repn < sizeof ed_rep) ed_rep[ed_repn++] = (uint8_t)(n & 127u); }
static void ed_v(int32_t n) { ed_b((uint32_t)(n + 8192)); ed_b((uint32_t)(n + 8192) >> 7); }
static int32_t ed_rv(const uint8_t *a) { return (int32_t)(a[0] | (uint32_t)a[1] << 7) - 8192; }
static void ed_str(const char *s, uint32_t n) { (void)s; (void)n; }
#include "../firmware/src/editor_preferences.c"
#include "../firmware/src/editor_menu.c"

static int check(const char *what, int ok)
{
    printf("persist: %-66s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* T0227: the unit's save timer (ui_input.c morph_settings: ui.mset_dirty saved ~2 s after the last change), as the main loop runs it */
static void rest(void)
{
    fm1_ms += 2100u;
    if (ui.mset_dirty && fm1_ms - ui.mset_t >= 2000u) {
        ui.mset_dirty = 0;
        settings_save();
    }
    settings_poll();
}

static void reset(void)
{
    memset(nor, 0xFF, sizeof nor);
    memset(&song, 0, sizeof song);
    memset(trk, 0, sizeof trk);
    memset(&persist_saved, 0, sizeof persist_saved);
    memset(&settings, 0, sizeof settings);
    memset(&ui, 0, sizeof ui);
    memset(&favorites, 0, sizeof favorites);
    panel = PANEL_DEFAULT;
    host_tracks_init();
    fm1_ms = 0;
    transport_req = 0;
    persist_pending = 0;
    persist_retry_ms = 0;
    flash_ok = 1;
    erase_error = 0;
    fail_after = -1;
    up_gen = erases = 0;
    irq_start_race = irq_races = 0;
}

/* ---- 1.2, Discussion #130: the autosave (project.c): written only when the music changed, stopped and idle 10 s, at
 * most one a minute; restored at power-on (RESTORE LAST ON, a clean boot); a damaged one ignored ---- */
static void as_run(uint32_t ms)                          /* the main loop for ms (autosave_poll checks every 250 ms) */
{
    uint32_t end = fm1_ms + ms;
    while ((int32_t)(end - fm1_ms) > 0) {
        fm1_ms += 50u;
        autosave_poll();
    }
}
static void as_power_on(void)                            /* RAM lost: the defaults, then main.c's restore */
{
    uint32_t k;
    memset(trk, 0, sizeof trk);
    memset(&motion, 0, sizeof motion);
    memset(motion_active, 0, sizeof motion_active);
    memset(&as, 0, sizeof as);
    memset(&ui, 0, sizeof ui);
    proj_name[0] = 0;
    host_tracks_init();
    for (k = 0; k < NTRK; k++)
        trk[k].eng_req = trk[k].engine = (uint8_t)TRK_DEF[k][0];
}
static void as_edit(uint32_t i)                          /* a change a project holds: step i's note (always another) */
{
    static uint32_t n;
    track_t *t = &trk[i % NTRK];
    step_t *s = &t->step[i % 16u];
    s->time = ST_NOTE;
    s->n = 1;
    s->note[0] = (uint8_t)(20u + (s->note[0] + 1u + n++ % 50u) % 100u);
    s->vel = 100;
}
static int autosave_test(void)
{
    int bad = 0, ok;
    uint32_t w, k, b0, minute;
    step_t st[NSTEP];
    project_store_t raw;
    reset();
    as_power_on();
    as_erases = 0;
    ok = !autosave_boot(1) && !as.writes;
    as_run(120000u);
    bad += check("autosave: power-on with none: nothing restored; 2 min unchanged: no write", ok && !as.writes && !as_erases);
    as_edit(0);
    as_run(9000u);
    ok = !as.writes;
    as_run(1500u);
    bad += check("autosave: a change: no write within 10 s, one once idle 10 s (one erase)",
                 ok && as.writes == 1u && as_erases == 1u && st_load(OBJ_AUTOSAVE, &raw, sizeof raw) == (int)sizeof raw);
    as_run(600000u);
    bad += check("autosave: 10 min unchanged after it: no more writes", as.writes == 1u && as_erases == 1u);
    song.g[G_SLOT] = 3;                                  /* (the PROJECT page's pick, the selected track: not the music) */
    song.sel = 2;
    as_run(120000u);
    bad += check("autosave: the selected track / PROJECT's slot pick alone: no write", as.writes == 1u);
    song.g[G_BPM] = 133;
    song.playing = 1;
    as_run(300000u);
    ok = as.writes == 1u;
    song.playing = 0;
    as_run(9000u);
    ok &= as.writes == 1u;
    as_run(1500u);
    bad += check("autosave: changed while playing: no write until stopped and idle 10 s, then one", ok && as.writes == 2u);
    as_edit(1);
    fm1_in.notes = 1u << 7;                              /* a key held (playing the sound, stopped) */
    as_run(60000u);
    ok = as.writes == 2u;
    fm1_in.notes = 0;
    trk[1].v[0].active = 1;                              /* .. its release still sounding */
    as_run(20000u);
    ok &= as.writes == 2u;
    trk[1].v[0].active = 0;
    as_run(9000u);
    ok &= as.writes == 2u;
    as_run(1500u);
    bad += check("autosave: a key held, a voice sounding: no write (an erase stops the audio); after, idle 10 s: one",
                 ok && as.writes == 3u);
    as_run(60000u);
    as_edit(2);                                          /* written 10 s on; then edits 15 s apart: a minute's gap */
    as_run(11000u);
    w = as.writes;
    as_edit(3);
    as_run(15000u);
    ok = as.writes == w;
    as_edit(4);
    as_run(15000u);
    ok &= as.writes == w;
    as_run(40000u);
    bad += check("autosave: two writes at least 60 s apart", ok && as.writes == w + 1u);
    as_run(60000u);
    w = as.writes;
    as_edit(5);
    for (k = 0; k < 120u; k++) {                         /* an editor backup going on: holds it */
        as_run(500u);
        autosave_hold();
    }
    ok = as.writes == w;
    as_run(10500u);
    bad += check("autosave: none during an editor transfer (autosave_hold), one after", ok && as.writes == w + 1u);
    /* power-on: restored */
    as_edit(6);
    song.g[G_BPM] = 97;
    song.bm = 5;                                           /* BASS MONO: the autosave carries it, a multi does not (0.2.21) */
    song.g[G_TUNE] = 4321;                                 /* MASTER TUNE (0.2.22): 432.1 Hz, the project's: the autosave carries it */
    str_cpy(proj_name, "MY TUNE", sizeof proj_name);
    as_run(70000u);
    memcpy(st, trk[0].step, sizeof st);
    b0 = as_erases;
    as_power_on();
    ok = memcmp(st, trk[0].step, sizeof st) != 0 && song.g[G_BPM] != 97;
    ok &= autosave_boot(1) == 1 && !memcmp(st, trk[0].step, sizeof st) && song.g[G_BPM] == 97 && str_eq(proj_name, "MY TUNE") &&
          ui.msg_t && str_eq(ui.msg, "RESTORED");
    as_run(300000u);
    bad += check("autosave: power-on restores it (steps, tempo, name; RESTORED); no write after (unchanged)",
                 ok && !as.writes && as_erases == b0);
    bad += check("autosave: MASTER TUNE (A4 in 0.1 Hz) comes back with it (432.1 Hz)", song.g[G_TUNE] == 4321);
    bad += check("autosave: BASS MONO (a master effect, project byte 65) comes back with it (5); a multi load (M01) leaves it as it is", song.bm == 5 && (project_load(0), song.bm == 5));
    as_power_on();
    ok = autosave_boot(0) == 0 && song.g[G_BPM] != 97;
    as_run(60000u);
    bad += check("autosave: after a crash / hang / failed boot: not restored, nothing written", ok && as_erases == b0);
    as_power_on();
    ui_prefs |= PREF_RESTORE_OFF;
    ok = autosave_boot(1) == 0 && song.g[G_BPM] != 97;
    as_edit(7);
    as_run(300000u);
    bad += check("autosave: RESTORE LAST OFF: none restored, none written", ok && !as.writes && as_erases == b0);
    ui_prefs &= (uint8_t)~PREF_RESTORE_OFF;
    {   /* 0.2.7: a matrix routed to the new destinations (a bus value, the part's LEVEL, an engine slot) is restored as it was */
        as_power_on();
        autosave_boot(1);
        trk[0].p[P_M3SRC] = MS_LFO2, trk[0].p[P_M3DST] = MD_RDAMP, trk[0].p[P_M3AMT] = -33;
        trk[1].p[P_M7SRC] = MS_VEL, trk[1].p[P_M7DST] = MD_LEVEL, trk[1].p[P_M7AMT] = 21;
        trk[1].p[P_M1SRC] = MS_AT, trk[1].p[P_M1DST] = MD_E1 + 4, trk[1].p[P_M1AMT] = 40;
        as_run(11000u);
        as_power_on();
        ok = autosave_boot(1) == 1 && trk[0].p[P_M3DST] == MD_RDAMP && trk[0].p[P_M3AMT] == -33 && trk[1].p[P_M7DST] == MD_LEVEL &&
             trk[1].p[P_M7SRC] == MS_VEL && trk[1].p[P_M1DST] == MD_E1 + 4 && trk[1].p[P_M1AMT] == 40;
        bad += check("autosave: matrix slots routed to REVERB DAMP, LEVEL, an engine slot (0.2.7): restored as they were", ok);
    }
    {   /* mute is cleared by a restore: the autosave holds a muted part 1 (the owner's report), power-on is audible */
        as_power_on();
        autosave_boot(1);
        trk[0].p[P_MUTE] = 1;
        as_run(11000u);
        as_power_on();
        ok = autosave_boot(1) == 1 && trk[0].p[P_MUTE] == 0 && trk[1].p[P_MUTE] == 0;
        bad += check("autosave: a muted part 1 in the autosave: restored UNMUTED at power-on", ok);
    }
    {   /* a damaged copy: the other one (A/B); both damaged: none */
        st_hdr_t h;
        int cur, other;
        as_power_on();
        autosave_boot(1);
        song.g[G_BPM] = 111;                             /* a newer copy into the other sector */
        as_run(11000u);
        cur = st_current(OBJ_AUTOSAVE, &h);
        nor[st_sector(OBJ_AUTOSAVE, (uint32_t)cur) + ST_PAYLOAD_OFF + 100u] ^= 0x10u;   /* its payload: the CRC fails */
        other = st_current(OBJ_AUTOSAVE, &h);
        as_power_on();
        ok = cur >= 0 && other == !cur && autosave_boot(1) == 1 && song.g[G_BPM] == 97;
        bad += check("autosave: the newest copy damaged: the copy before it is restored", ok);
        nor[st_sector(OBJ_AUTOSAVE, (uint32_t)other) + 12u] ^= 0x01u;   /* the other one's header too */
        as_power_on();
        ok = autosave_boot(1) == 0 && song.g[G_BPM] != 97 && song.g[G_BPM] != 111;
        bad += check("autosave: both copies damaged: ignored, power-on as new", ok);
        memset(nor + 0xE5000u, 0xFF, 0x2000u);
        st_save(OBJ_AUTOSAVE, "NOT A PROJECT", 13u);     /* a valid record that is no project */
        as_power_on();
        bad += check("autosave: a record that is no project: ignored", autosave_boot(1) == 0 && song.g[G_BPM] != 97);
    }
    {   /* DUELL 0.2.14: an autosave of before (0.2.13's FUN11: revision 0, the hash right; Felucca's FUN10) is ignored at power-on: nothing restored, nothing crashes,
         * and the power-on state is what the unit writes from then on */
        static uint8_t buf[3840];
        uint32_t sum, magic = 0x46554E41u, n;
        memset(nor + 0xE5000u, 0xFF, 0x2000u);
        as_power_on();
        autosave_boot(1);
        as_edit(9);
        song.g[G_BPM] = 97;
        as_run(11000u);                                  /* (a valid autosave, tempo 97) */
        n = (uint32_t)st_load(OBJ_AUTOSAVE, buf, sizeof buf);
        buf[67] = 0;
        sum = proj_hash(buf, sizeof buf - 4u); memcpy(buf + sizeof buf - 4u, &sum, 4);
        ok = n == sizeof buf && st_save(OBJ_AUTOSAVE, buf, sizeof buf) == 0;
        as_power_on();
        ok &= autosave_boot(1) == 0 && song.g[G_BPM] != 97;
        memcpy(buf, &magic, 4);
        buf[67] = 1;
        sum = proj_hash(buf, sizeof buf - 4u); memcpy(buf + sizeof buf - 4u, &sum, 4);
        ok &= st_save(OBJ_AUTOSAVE, buf, sizeof buf) == 0;
        as_power_on();
        ok &= autosave_boot(1) == 0 && song.g[G_BPM] != 97;
        bad += check("autosave: a FUN11 of before 0.2.14 (revision 0) or a FUN10 is ignored at power-on, nothing restored", ok);
    }
    /* a power cut in the middle of a write: the copy before stays in charge */
    memset(nor + 0xE5000u, 0xFF, 0x2000u);
    as_power_on();
    autosave_boot(1);
    as_edit(8);
    as_run(11000u);                                      /* (written: copy A) */
    song.g[G_BPM] = 150;
    fail_after = 3;                                      /* the next write stops after 3 programs */
    as_run(70000u);
    fail_after = -1;
    ok = as.err;
    as_power_on();
    ok &= autosave_boot(1) == 1 && song.g[G_BPM] != 150 && trk[0].step[8].n == 1u;
    bad += check("autosave: a write cut short (power off): the copy before is restored", ok);
    /* wear: a 4-hour session: 3 h of editing (a change every 3 s), pauses and playing it back, then an hour away */
    as_power_on();
    {   /* the cloud plan's job A: the 16 extension slots of both parts are part of the music: a change of one alone is autosaved, and they come back at power-on */
        uint32_t w0 = as.writes, k2;
        for (k2 = 0; k2 < NX; k2++) {
            XV(&trk[0], k2) = (int16_t)(k2 * 5u) - 30;
            XV(&trk[1], k2) = (int16_t)(100 - 6 * (int)k2);
        }
        as_run(70000u);
        ok = as.writes == w0 + 1u;
        as_power_on();
        ok &= autosave_boot(1) == 1;
        for (k2 = 0; k2 < NX; k2++)
            ok &= XV(&trk[0], k2) == (int)(k2 * 5u) - 30 && XV(&trk[1], k2) == 100 - 6 * (int)k2;
        as_run(300000u);
        bad += check("autosave: the extension slots (XV) of both parts are written when they change (alone) and come back at power-on", ok && as.writes == 0u);
        w0 = as.writes;
        XV(&trk[1], X_SUBLVL) = 5;
        as_run(70000u);
        bad += check("  and a change of one slot alone is a new autosave (the signature covers them)", as.writes == w0 + 1u);
    }
    autosave_boot(1);
    as_erases = 0;
    for (minute = 0; minute < 240u; minute++) {
        uint32_t s;
        if (minute >= 180u) {
            as_run(60000u);
            continue;
        }
        song.playing = minute % 10u >= 7u;               /* 3 minutes in 10 playing */
        for (s = 0; s < 60u; s += 3u) {
            if (minute % 5u != 4u)                       /* (every fifth minute a pause, no edits) */
                as_edit(minute * 20u + s);
            as_run(3000u);
        }
    }
    song.playing = 0;
    as_run(20000u);
    printf("persist:   a 4-hour session (3 h editing and playing, 1 h idle): %u autosave writes\n", (unsigned)as_erases);
    bad += check("autosave wear: a session's writes stay far below one a minute", as_erases > 0u && as_erases <= 60u);
    {   /* worst case: a change, then just enough idle, over and over, for 8 hours */
        uint32_t i;
        as_erases = 0;
        for (i = 0; i < 8u * 3600u / 11u; i++) {
            as_edit(i);
            as_run(11000u);
        }
        printf("persist:   worst case, 8 h of a change every 11 s: %u writes (%u per sector)\n", (unsigned)as_erases,
               (unsigned)(as_erases / 2u));
        bad += check("autosave wear, worst case: at most one write a minute (480 in 8 h)", as_erases <= 8u * 60u + 1u);
    }
    return bad;
}

/* ---- DUELL 0.2.14: the new defaults (KNOB ACCEL ON, CLICK REC, USB SERIAL OFF) and their one-time flag [15][24] bit 7
 * (settings_persist.c settings_defaults): a new unit (and ERASE DATA, erase_test.c) gets them, a unit with an older record gets them
 * once where the old default is stored (an explicit OFF / ON cannot be told apart from it), then the user's choice sticks ---- */
static void d14_boot(void)                               /* a power cycle: RAM gone, persist_boot from the NOR, settings_init */
{
    memset(&favorites, 0, sizeof favorites);
    memset(&settings, 0, sizeof settings);
    memset(&persist_saved, 0, sizeof persist_saved);
    first_boot_pending = 0;
    persist_pending = 0;
    jedec_ok = 1;
    persist_boot();
    ps_dirty = 0;                                        /* (the pattern slots' defaults of a blank flash are written at the first quiet pass: not what these counts of erases are about) */
    jedec_ok = 0;
    flash_ok = 1;
    settings_init();
}
static int d14_new(void)                                 /* the three new defaults and the flag in RAM */
{
    return (ui_prefs & PREF_ACCEL) && (ui_prefs & PREF_SERIAL_OFF) && rp_get(RP_CLICK) == 1u && (ui_prefs2 & (PREF_DEFAULTS >> 16));
}
static int defaults_0214_test(void)
{
    int bad = 0, ok;
    persist_t p, q;
    uint32_t e0;
    /* a new unit: no settings in the flash */
    reset();
    d14_boot();
    ok = first_boot_pending == 1u && d14_new() && menu_get(MI_ACCEL) == 1u && menu_get(MI_CLICK) == 1u &&
         str_eq(menu_vname(MI_SERIAL, menu_get(MI_SERIAL)), "OFF") && str_eq(menu_vname(MI_CLICK, menu_get(MI_CLICK)), "REC") &&
         rp_get(RP_LEVEL) == 1u && rp_get(RP_COUNTIN) == 0u && !(ui_prefs & ~(uint32_t)(PREF_ACCEL | PREF_SERIAL_OFF)) &&
         ui_prefs2 == (PREF_DEFAULTS >> 16);
    settings_save(); settings_poll();
    ok &= !persist_pending && st_load(OBJ_SETTINGS, &q, sizeof q) == (int)sizeof q && (q.favorites.factory[15][24] & 0x80u) &&
          (q.favorites.factory[15][30] & (PREF_ACCEL | PREF_SERIAL_OFF)) == (PREF_ACCEL | PREF_SERIAL_OFF) && (q.favorites.factory[15][28] & 3u) == 1u;
    bad += check("0.2.14 a new unit: KNOB ACCEL ON, CLICK REC, USB SERIAL OFF, the flag set; saved so", ok);
    /* the user's choices on a 0.2.14 unit stick: ACCEL OFF, CLICK OFF, USB SERIAL ON, saved, a power cycle */
    menu_put(MI_ACCEL, 0); menu_put(MI_CLICK, 0); menu_put(MI_SERIAL, 0);
    settings_save(); settings_poll();
    d14_boot();
    ok = first_boot_pending == 0u && !(ui_prefs & PREF_ACCEL) && !(ui_prefs & PREF_SERIAL_OFF) && rp_get(RP_CLICK) == 0u &&
         (ui_prefs2 & 0x80u);
    e0 = erases;
    settings_save(); settings_poll();
    ok &= erases == e0;                                  /* (nothing changed: no write) */
    d14_boot();
    ok &= !(ui_prefs & PREF_ACCEL) && !(ui_prefs & PREF_SERIAL_OFF) && rp_get(RP_CLICK) == 0u;
    bad += check("0.2.14 the user's ACCEL OFF / CLICK OFF / USB SERIAL ON (flag set) stay so, power cycle after power cycle", ok);
    /* an older record (0.2.13): ACCEL OFF, CLICK OFF, USB SERIAL ON (the old defaults), MODE RECALL ON, ANIM IDLE, junk in [15][10] bits 1-7 */
    reset();
    d14_boot();
    settings_export(&p);
    p.favorites.factory[15][30] = 8u;        /* (ACCEL clear, SERIAL clear = ON, the retired bit 8 (was USB LEVEL FIXED): kept as it is) */
    p.favorites.factory[15][28] = 2u << 4;               /* CLICK OFF, LEVEL MID, COUNT-IN 2 BARS */
    p.favorites.factory[15][24] = 0x40u | 0x02u;         /* MODE RECALL ON, STEP PREVIEW ON; the flag clear */
    p.favorites.factory[15][10] = 0xFFu;                 /* ANIM IDLE (bit 0) and bits 1-7 set */
    ok = st_save(OBJ_SETTINGS, &p, sizeof p) == 0;
    d14_boot();
    ok &= first_boot_pending == 0u && d14_new() && (ui_prefs & 8u) && rp_get(RP_COUNTIN) == 2u &&
          (ui_prefs2 & 0x42u) == 0x42u && ui_idle == 1u && ui_prefs == (8u | PREF_ACCEL | PREF_SERIAL_OFF);
    e0 = erases;
    settings_save(); settings_poll();
    ok &= erases == e0 && !persist_pending;              /* (no save at boot: the record as read, changed in RAM, is the saved copy) */
    persist_base(&q);
    memcpy(q.map, midi_map, sizeof q.map);
    ok &= settings_import(&q, (int)sizeof q) == 1 && !memcmp(&q, &persist_saved, sizeof persist_saved) && persist_is_saved(&q);   /* (importing twice changes nothing) */
    bad += check("0.2.14 an older record: ACCEL ON, CLICK OFF -> REC, USB SERIAL ON -> OFF once; the rest kept, [15][10] bits 1-7 cleared", ok);
    /* .. with CLICK ON / REC and USB SERIAL OFF stored: kept (only the old default changes) */
    p.favorites.factory[15][28] = 2u;                    /* CLICK ON */
    p.favorites.factory[15][30] = PREF_SERIAL_OFF | PREF_ACCEL;
    st_save(OBJ_SETTINGS, &p, sizeof p);
    d14_boot();
    ok = rp_get(RP_CLICK) == 2u && (ui_prefs & PREF_SERIAL_OFF) && (ui_prefs & PREF_ACCEL) && (ui_prefs2 & 0x80u);
    p.favorites.factory[15][28] = 1u;                    /* CLICK REC */
    st_save(OBJ_SETTINGS, &p, sizeof p);
    d14_boot();
    ok &= rp_get(RP_CLICK) == 1u;
    /* .. then the user sets them back to the old values: they stick (the flag is saved with them) */
    menu_put(MI_ACCEL, 0); menu_put(MI_CLICK, 0); menu_put(MI_SERIAL, 0);
    settings_save(); settings_poll();
    d14_boot();
    ok &= !(ui_prefs & PREF_ACCEL) && rp_get(RP_CLICK) == 0u && !(ui_prefs & PREF_SERIAL_OFF);
    bad += check("0.2.14 an older record's CLICK ON / REC and SERIAL OFF kept; set back to OFF / ON after the change: they stick", ok);
    /* a record without any favourites bytes (the flag clear, as 0.2.13 left it): the new defaults; no flash (RAM only): the new defaults too.
     * Older forms (PER1..PER3, a PER4 without user_hi) are no record at all (0.2.14): refused, the unit starts as a new one */
    reset();
    memset(&p, 0, sizeof p);
    p.magic = 0x50455233u;
    ok = settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 0;
    p.magic = PERSIST_MAGIC;
    ok &= settings_import(&p, (int)(sizeof p - sizeof p.favorites.user_hi)) == 0;
    ok &= settings_import(&p, (int)sizeof p) == 1 && d14_new();
    reset();
    jedec_ok = 0;
    persist_boot();
    flash_ok = 1;
    ok &= d14_new();
    bad += check("0.2.14 a PER3 record (no favourites) and a unit with no flash part: the new defaults", ok);
    reset();
    return bad;
}

/* cloud job G (EDITABLE B): End B, as saved, survives a power cycle through the autosave (PROJ_REV 7's block), and so do the values set by hand */
static int editb_power_test(void)
{
    int bad = 0, ok;
    uint32_t seed0, w;
    int8_t b0[P_COUNT], bx0[NX];
    xb_edit_t ed[XB_MAX], ed0[XB_MAX];
    uint32_t ne0;
    reset();
    as_power_on();
    memset(mph, 0, sizeof mph);
    memset(&morph, 0, sizeof morph);
    song.sel = 0;
    morph_set_target(0);
    trk[0].user = 3;                                        /* a loaded user patch: slot 3, preset 0 (up_load) */
    trk[0].preset = 0;
    morph_rebase_all();
    morph_roll_seed(0, 1, 4321);                            /* End B as the patch saved it */
    morph_set_fader(127);
    morph_b_set(0, P_E6, 44);                               /* two values by hand */
    morph_b_set(0, P_LD_FLT, 3);
    morph_set_fader(0);
    seed0 = morph_seed_of(0, 1);
    memcpy(b0, morph_cur(&mph[0], 1), P_COUNT);
    memcpy(bx0, morph_curx(&mph[0], 1), NX);
    ne0 = morph_edits(0, ed0);
    as_edit(0);
    as_run(30000u);
    ok = as.writes == 1u && ne0 == 2u;
    memset(mph, 0, sizeof mph);                             /* the power cycle: RAM gone, the morph too */
    memset(&morph, 0, sizeof morph);
    as_power_on();
    ok &= autosave_boot(1) == 1;
    morph_ready(0);                                         /* (the first use of the morph) */
    bad += check("EDITABLE B: a power cycle (autosave): End B comes back as it was (the seed, the whole end)",
                 ok && morph_seed_of(0, 1) == seed0 && !memcmp(b0, morph_cur(&mph[0], 1), P_COUNT) && !memcmp(bx0, morph_curx(&mph[0], 1), NX) && mph[0].fader == 0u);
    ok = morph_edits(0, ed) == 2u && ed[0].wid == ed0[0].wid && ed[0].val == ed0[0].val && ed[1].wid == ed0[1].wid && ed[1].val == ed0[1].val;
    bad += check("  .. and the two values set by hand with it", ok);
    as_run(300000u);
    bad += check("  .. nothing is written after the restore (the music is unchanged)", as.writes == 0u);
    as_edit(1);
    as_run(30000u);
    ok = as.writes == 1u;
    morph_set_fader(127);
    morph_b_set(0, P_E1, 12);
    as_run(70000u);                                         /* (a minute's gap after the write before) */
    bad += check("  .. a B edit alone (nothing else changed) is a change the autosave writes", ok && as.writes == 2u);
    for (w = 0; w < 4u; w++)
        ed[w].val = 0;
    return bad;
}

int main(void)
{
    int bad = 0, ok;
    uint32_t before;
    persist_t p;
    reset();
    settings_init();
    {
        const uint8_t amber[] = {0, 2};
        song.playing = 1;
        bad += check("editor display change while playing is applied and reports deferred saving",
                      ed_ui_set(amber, sizeof amber) == 4 && settings.palette == 2u && persist_pending == 1 && !erases);
        song.playing = 0;
        settings_poll();
        bad += check("STOP commits the deferred palette setting",
                      !persist_pending && erases == 1 && persist_saved.palette == palette_to_stored(2));
        before = erases;
        bad += check("repeating an unchanged editor preference succeeds without erasing",
                      ed_ui_set(amber, sizeof amber) == 0 && erases == before);
        erase_error = 1;
        const uint8_t mono[] = {0, 0};
        bad += check("editor reports failed saving while preserving the previous flash setting",
                      ed_ui_set(mono, sizeof mono) == 3 && settings.palette == 0u && persist_pending == 2 &&
                      persist_saved.palette == palette_to_stored(2));
        erase_error = 0;
        fm1_ms += 1000;
        settings_poll();
        bad += check("failed editor preference retries through the existing persistence path",
                      !persist_pending && persist_saved.palette == palette_to_stored(0));
        const uint8_t bold[] = {1, 1};
        bad += check("the retired font weight is refused without a write", ed_ui_set(bold, sizeof bold) == 2);
    }
    {   /* MENU_SET (editor_menu.c): the same settings record, the same deferral, retry and idempotence. T0227 (flicker fix): as the unit's own knob, a two-value
         * switch is saved at once, a setting of more values once it has rested ~2 s (ui.mset_dirty; ui_input.c morph_settings' timer, emulated here by rest()) */
        const uint8_t leds_off[] = {4, 0, 64}, accel_on[] = {6, 1, 64}, hold_06[] = {5, 3, 64};
        persist_t q;
        uint32_t k;
        reset();
        settings_init();
        song.playing = 1;
        ed_repn = 0; ed_menu_handle(ED_MENU_SET, leds_off, 3);
        bad += check("MENU_SET while playing: applied, rc 4 (queued), no erase",
                     ed_repn == 4 && ed_rep[0] == 4 && ed_rep[1] == 4 && settings_leds == LEDS_OFF && ui.mset_dirty && !erases);
        rest();
        bad += check("  .. rested 2 s, still playing: the write waits (no erase)", persist_pending == 1 && !erases);
        song.playing = 0;
        settings_poll();
        bad += check("STOP commits the MENU setting (LEDS OFF in the settings record)",
                     !persist_pending && erases == 1 && st_load(OBJ_SETTINGS, &q, sizeof q) == (int)sizeof q &&
                     leds_from_stored(q.zoom) == LEDS_OFF);
        ed_repn = 0; ed_menu_handle(ED_MENU_SET, accel_on, 3);
        ok = ed_rep[0] == 0 && (ui_prefs & PREF_ACCEL) && erases == 2;
        bad += check("MENU_SET stopped, a two-value switch (KNOB ACCEL): rc 0, saved at once (as the menu's knob)", ok);
        ed_repn = 0; ed_menu_handle(ED_MENU_SET, hold_06, 3);
        ok = ed_rep[0] == 4 && settings_hold == 3u && erases == 2;
        rest();
        ok &= erases == 3 && st_load(OBJ_SETTINGS, &q, sizeof q) == (int)sizeof q &&
              hold_from_stored(q.bold) == 3u && (q.favorites.factory[15][30] & PREF_ACCEL) && leds_from_stored(q.zoom) == LEDS_OFF;
        bad += check("MENU_SET stopped, HOLD (4 values): rc 4, saved once it rested, the others kept", ok);
        before = erases;
        ed_repn = 0; ed_menu_handle(ED_MENU_SET, hold_06, 3);
        rest();
        bad += check("repeating an unchanged MENU_SET: rc 0, no erase", ed_rep[0] == 0 && erases == before);
        /* T0227: the editor's BASS MONO slider dragged over every value with nothing sounding: no erase while it moves, ONE when it rests */
        before = erases;
        ok = 1;
        ui.force = 0;
        for (k = 0; k <= BM_N; k++) {
            const uint8_t bm[] = {33, (uint8_t)((k + 8192u) & 127u), (uint8_t)((k + 8192u) >> 7)};
            ed_repn = 0; ed_menu_handle(ED_MENU_SET, bm, 3);
            ok &= song.bm == (int16_t)k && ed_rep[0] != 3 && !ui.force;
            fm1_ms += 30u;
            settings_poll();
        }
        ok &= erases == before;
        rest();
        ok &= erases == before + 1u;
        bad += check("BASS MONO dragged OFF..400 Hz (stopped, quiet): no erase per step, no forced redraw, one erase 2 s after the last", ok);
        before = erases;
        for (k = 0; k < 20u; k++) {                      /* WIDTH, COMP THRESH, SIDECHAIN P1 NOTE: the same */
            const uint8_t wd[] = {42, (uint8_t)((100u + k + 8192u) & 127u), (uint8_t)((100u + k + 8192u) >> 7)};
            const uint8_t th[] = {34, (uint8_t)((8192u - 30u + k) & 127u), (uint8_t)((8192u - 30u + k) >> 7)};
            const uint8_t sn[] = {39, (uint8_t)((30u + k + 8192u) & 127u), (uint8_t)((30u + k + 8192u) >> 7)};
            ed_repn = 0; ed_menu_handle(ED_MENU_SET, wd, 3);
            ed_repn = 0; ed_menu_handle(ED_MENU_SET, th, 3);
            ed_repn = 0; ed_menu_handle(ED_MENU_SET, sn, 3);
            fm1_ms += 30u;
            settings_poll();
        }
        ok = erases == before && !ui.force;
        rest();
        ok &= erases == before + 1u;
        bad += check("WIDTH, COMP THRESH, SIDECHAIN NOTE dragged: one erase once they rest", ok);
        erase_error = 1;
        ed_repn = 0; ed_menu_handle(ED_MENU_SET, leds_off, 3);   /* unchanged: still rc 0 (no write) */
        ok = ed_rep[0] == 0;
        {
            const uint8_t leds_inv[] = {4, 3, 64};
            ed_repn = 0; ed_menu_handle(ED_MENU_SET, leds_inv, 3);
            rest();
        }
        ok &= settings_leds == LEDS_INV && persist_pending == 2 && leds_from_stored(persist_saved.zoom) == LEDS_OFF;
        ed_repn = 0;
        {
            const uint8_t leds_inv[] = {4, 3, 64};
            ed_menu_handle(ED_MENU_SET, leds_inv, 3);
        }
        ok &= ed_rep[0] == 3;
        bad += check("MENU_SET with a failing flash: applied, rc 3, the saved setting kept", ok);
        erase_error = 0;
        fm1_ms += 1000;
        settings_poll();
        bad += check("... and retried by the settings path", !persist_pending && leds_from_stored(persist_saved.zoom) == LEDS_INV);
        settings_hold = HOLD_DEF; settings_leds = LEDS_DIM;
    }
    reset();
    settings.palette = 3;
    settings.zoom = 1;
    persist_saved.bold = 1;                       /* (an earlier firmware's font weight: kept as saved) */
    favorites.factory[8][0] = 1;
    favorites.user = 1u << 31;
    song.playing = 1;
    settings_save();
    bad += check("playing: settings queued, no erase, runtime kept", persist_pending && !erases &&
                  settings.palette == 3u && settings.zoom == 1u);
    song.playing = 0;
    transport_req = 1;
    settings_poll();
    bad += check("pending PLAY: settings still queued, no erase", persist_pending && !erases);
    transport_req = 0;
    settings.palette = 2;
    settings_poll();
    bad += check("STOP: latest settings saved and request cleared", !persist_pending && erases == 1u &&
                  st_load(OBJ_SETTINGS, &p, sizeof p) == (int)sizeof p && p.palette == palette_to_stored(2) &&
                  p.zoom == 1u && p.bold == 1u &&
                  p.favorites.factory[8][0] == 1 && p.favorites.user == (1u << 31) &&
                  !memcmp(&p.panel, &panel, sizeof panel));
    settings_save();
    bad += check("unchanged settings do not erase", !persist_pending && erases == 1u);
    settings.palette = 1;
    erase_error = 1;
    fm1_ms = 0xFFFFFF00u;
    settings_save();
    before = erases;
    settings_poll();
    fm1_ms += 999u;
    settings_poll();
    bad += check("failed settings retry waits through clock wrap", persist_pending && erases == before &&
                  persist_saved.palette == palette_to_stored(2) && settings.palette == 1u);
    erase_error = 0;
    fm1_ms++;
    settings_poll();
    bad += check("settings retry succeeds after 1000 ms", !persist_pending && erases == before + 1u &&
                  st_load(OBJ_SETTINGS, &p, sizeof p) == (int)sizeof p && p.palette == palette_to_stored(1));

    {   /* DUELL 0.2.11: a settings write (an erase stops the audio ~40 ms) waits for silence, not only for the transport: the
         * one-time "layer seen" bit (ui_layer.c layer_opened, favorites.factory[15][31]) on a layer's first open while played */
        uint8_t seen0 = favorites.factory[15][31];
        before = erases;
        song.playing = 0; transport_req = 0; fm1_in.notes = 0; fm1_in.buttons = 0;
        favorites.factory[15][31] = (uint8_t)(seen0 | 0x10u);
        trk[1].v[1].active = 1;                               /* a voice sounding (stopped transport) */
        settings_save();
        bad += check("a voice sounding: seen bit queued, no erase", persist_pending && erases == before);
        trk[1].v[1].active = 0; fm1_in.notes = 1u << 5;       /* a key held */
        settings_poll();
        bad += check("a key held: still no erase", persist_pending && erases == before);
        fm1_in.notes = 0; fm1_in.buttons = 1u << 3;           /* a button held (the long press that opened the layer) */
        settings_poll();
        bad += check("a button held: still no erase", persist_pending && erases == before);
        fm1_in.buttons = 0;
        settings_poll();
        bad += check("silent: written once, the seen bit saved", !persist_pending && erases == before + 1u &&
                      st_load(OBJ_SETTINGS, &p, sizeof p) == (int)sizeof p && (p.favorites.factory[15][31] & 0x10u));
        favorites.factory[15][31] = seen0; settings_save();   /* (put back, saved: later checks start clean) */
        before = erases;
    }
    /* (DUELL: the PROJECT slots and the user preset banks are gone; their failed-save flows are not tested here any more) */
    before = erases;
    song.playing = 0; transport_req = 1; irq_start_race = 1;
    settings.palette = 4;
    settings_save();
    bad += check("PLAY consumed before the settings snapshot defers the write",
                  irq_races == 1u && song.playing && !transport_req && persist_pending && erases == before);
    bad += autosave_test();
    bad += editb_power_test();
    bad += defaults_0214_test();
    printf("%s\n", bad ? "PERSISTENCE TEST FAILED" : "persistence test passed");
    return bad != 0;
}
