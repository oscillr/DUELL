/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MENU's settings for the editor (EDITOR_PROTOCOL.md "MENU settings"): MENU_DESC describes one, MENU_SET sets one;
 * INFO 4E 01 count. The editor builds its settings from these; the values, names and the apply path are the menu's
 * own (menu_items.c), so the two cannot differ. BUTTON MAP (was CALIBRATION), GUIDE, ABOUT and ERASE DATA are not offered (no value). 1.0.5: each reply
 * ends with the row's MENU tab (MI_TAB: its index and name), so the editor can group the settings as the device does.
 * ED_MENU: {row, id}, the 1.0.4 settings in the menu's order, then the later ones as they came (1.1: CLICK, CLICK LEVEL,
 * COUNT-IN, rows of AUDIO; 1.2: RESTORE LAST, SYSTEM; SCALE LEDS, CONTROL; 1.1.5: SCREEN OFF, DISPLAY; 1.2: SCOPE,
 * DISPLAY; STEP PREVIEW, CHORD ENTRY, CONTROL; MIDI IN, MIDI; TUNE, AUDIO; 1.3: HOME, DISPLAY): an index keeps its
 * setting too, so an older editor lists what it knew where it was (the
 * editor groups them by their tab). An id keeps its meaning for good: a new setting takes the next free id
 * (append-only), wherever its row goes; ids 0..126 (127: none). DUELL: id 8 (BPM LOCK) is retired, never reused (SELECT is
 * no tempo any more), so the index of every setting after it is its id - 1.
 * 1.2: TUNE (22) is the project's G_TUNE (menu_items.c MI_PROJECT; DUELL 0.1: MIDI IN (21), the project's G_ROUTE, left MENU for
 * GLO > MIDI: its id is retired, MIDI IN is no MENU setting), the same value as a SET of global 3: MENU_SET applies them to the music in RAM, kept with the project (the autosave, PROJECT SAVE),
 * not in the settings record: rc 0, nothing for settings_save to write. TUNE is a number (EDM_INT: the cents -50..50,
 * unit "ct"), the only one so far */
enum { ED_MENU_DESC = 72, ED_MENU_SET };
/* DUELL 0.2.14: GLO > MODULES' four switches (core.h MODS_*, ui.c mods_put) are device settings but no MENU rows (they live on the GLO page):
 * here they are the rows ED_MODS + 0..3 (ARP, SEQ, SENDS, PART FX; no MI_ row has such a number), ids 28..31, values 0 OFF / 1 ON (the
 * knob's order: left OFF, right ON; ON the default), their tab index MTAB_COUNT (4) named "MODULES" (no MENU tab: the device shows them
 * on GLO > MODULES). A SET switches as the page's knob does and saves the settings record */
#define ED_MODS 0x60u
#define ED_MODS_TAB "MODULES"
/* DUELL 0.2.19 (owner: BASS MONO is a SOUND setting, not a MENU one): id 33, BASS MONO, is the page FX > MASTER's K1 (core.h G_BM, song.bm: kept with the music, the
 * multis and the autosave). Like TUNE it is described here as a setting, but its row ED_BM is no MI_ row (the MENU has none) and its tab index MTAB_COUNT + 1 is named
 * "MASTER" (no MENU tab): the editor shows it on the GLOBAL tab. A SET changes the value as the knob does; rc 0 (the music holds it, no settings to save). The unit
 * shows no page in DUEL; the value is kept and a SET is still applied */
#define ED_BM 0x64u
#define ED_BM_TAB "MASTER"
enum { EDM_ENUM, EDM_INT };                            /* kind: names follow; or a unit (none yet) */
static const uint8_t ED_MENU[][2] = {
    {MI_COLOR, 0}, {MI_ANIM, 3}, {MI_LEDS, 4}, {MI_HOLD, 5}, {MI_ACCEL, 6},
                                                       /* (id 7, FX LATCH, retired in DUELL 0.2.2: the FX hold layer is removed; never reused) */
                                                       /* (id 9, OUTPUT EQ FLAT / LOWCUT / BASS+, retired in DUELL 0.2.19: the low cut and BASS+ are removed; never reused) */
    {MI_USB, 10}, {MI_SERIAL, 11},
    {MI_CLICK, 12}, {MI_CLKLVL, 13}, {MI_COUNTIN, 14},
    {MI_RESTORE, 15},                                  /* 1.2 (Discussion #130), a row of SYSTEM */
    {MI_SCLLED, 16},                                   /* 1.2 (Discussion #127), a row of CONTROL */
    {MI_SCROFF, 17},                                   /* 1.1.5, a row of DISPLAY */
                                                       /* (id 18, SCOPE OUT / MIX, retired in DUELL 0.2.14: the scope always draws OUT; never reused) */
    {MI_PREVIEW, 19},                                  /* 1.2 (Discussion #169), a row of CONTROL */
    {MI_CHADD, 20},                                    /* 1.2 (#155), a row of CONTROL */
                                                       /* (ids 1 STYLE and 2 LARGE, retired in DUELL 0.2.2: the look is FLAT only and LARGE is gone; never reused) */
                                                       /* (id 21, MIDI IN, retired in DUELL 0.1: it is GLO > MIDI' MIDI IN now; never reused) */
    {MI_TUNE, 22},                                     /* 1.2: the project's G_TUNE (was GLO > GLOBAL), AUDIO */
                                                       /* (id 23, 1.3 HOME / DUELL OVERVIEW SCOPE / PARTS, retired in DUELL 0.2.14 with the OVERVIEW page; never reused) */
                                                       /* (id 24, HELP, retired in DUELL 0.2.2: removed for now; never reused) */
    {MI_HOMEPG, 25},                                   /* DUELL 0.1: HOME OPENS MORPH / MODE / MIXER (0.2.14: no OVERVIEW, max 2), CONTROL */
    {MI_CPU, 26},                                      /* DUELL 0.2.9: CPU METER ON / OFF, DISPLAY (the CPU bar under the header's battery) */
    {MI_MODERECALL, 27},                                /* DUELL 0.2.11: MODE RECALL OFF / ON, CONTROL (does a multi load set its saved MODE); 0.2.14: MULTI LOAD KEEPS MODE / SETS MODE */
    {ED_MODS + 0u, 28}, {ED_MODS + 1u, 29},             /* DUELL 0.2.14: GLO > MODULES ARP, SEQ, SENDS, PART FX (OFF / ON; no MENU row: ED_MODS) */
    {ED_MODS + 2u, 30}, {ED_MODS + 3u, 31},
    {MI_PERFLEAVE, 32},                                 /* DUELL 0.2.14: PERF LEAVE FADE / KEEP, CONTROL (leaving PERFORM / a sound load: SPACE and KILL to neutral, or stay) */
    {ED_BM, 33},                                        /* DUELL 0.2.19: BASS MONO OFF / 40 .. 400 Hz, the MASTER page (the master mono below the corner; not in DUEL); no MENU row (ED_BM) */
};
_Static_assert(MI_COUNT < ED_MODS && ED_MODS + 4u <= ED_BM, "editor_menu.c: the MODULES rows and BASS MONO are no MI_ row");
#define ED_MENU_N NELEM(ED_MENU)

/* rc as UI_SET: 0 applied and saved, 3 applied in RAM only (no flash, a failed write), 4 queued until STOP */
static uint32_t ed_menu_save(void)
{
    settings_save();
#if FELUCCA_FLASH
    if (!flash_ok || persist_pending == 2u) return 3;
    if (persist_pending == 1u) return 4;
    {
        persist_t p = persist_saved;
        settings_export(&p);
        return memcmp(&p, &persist_saved, sizeof p) ? 3u : 0u;
    }
#else
    return 3;
#endif
}

static int ed_menu_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint32_t i, row, k;
    int32_t v, lo;
    if (cmd == ED_MENU_DESC) {                         /* index -> index, id, kind, value, min, max (v14), name, names,
                                                          * (1.0.5) its tab: index, name */
        ed_b(a[0]);
        if (a[0] >= ED_MENU_N) {
            ed_b(127);                                 /* no such item: the list ends */
            return 1;
        }
        row = ED_MENU[a[0]][0];
        if (row == ED_BM) {                            /* (0.2.19: BASS MONO, the MASTER page's: song.bm) */
            ed_b(ED_MENU[a[0]][1]);
            ed_b(EDM_ENUM);
            ed_v(song.bm > 0 && song.bm <= (int16_t)BM_N ? song.bm : 0);
            ed_v(0);
            ed_v((int32_t)BM_N);
            ed_str("BASS MONO", 12);
            for (i = 0; i <= BM_N; i++)
                ed_str(N_BM[i], 12);
            ed_b(MTAB_COUNT + 1u);
            ed_str(ED_BM_TAB, 12);
            return 1;
        }
        if (row >= ED_MODS) {                          /* (0.2.14: a MODULES switch: OFF / ON, its tab MODULES) */
            k = row - ED_MODS;
            ed_b(ED_MENU[a[0]][1]);
            ed_b(EDM_ENUM);
            ed_v((int32_t)!((mods_get() >> k) & 1u));
            ed_v(0);
            ed_v(1);
            ed_str(MODS_NAME[k], 12);
            ed_str("OFF", 12);
            ed_str("ON", 12);
            ed_b(MTAB_COUNT);
            ed_str(ED_MODS_TAB, 12);
            return 1;
        }
        k = menu_n(row);
        lo = row == MI_TUNE ? TUNE_MIN : 0;            /* (a number: its values from min) */
        ed_b(ED_MENU[a[0]][1]);
        ed_b(row == MI_TUNE ? EDM_INT : EDM_ENUM);
        ed_v((int32_t)menu_get(row) + lo);
        ed_v(lo);
        ed_v((int32_t)k - 1 + lo);
        ed_str(MI_NAME[row], 12);
        if (row == MI_TUNE)
            ed_str("ct", 12);                          /* (EDM_INT: its unit) */
        else
            for (i = 0; i < k; i++)
                ed_str(menu_vname(row, i), 12);
        ed_b(MI_TAB[row]);                             /* (after the names: an older editor stops before it) */
        ed_str(MTAB_NAME[MI_TAB[row]], 12);
        return 1;
    }
    if (cmd != ED_MENU_SET)
        return 0;
    (void)n;                                           /* id, value v14 -> rc, id, value v14 (the device's, clamped) */
    v = ed_rv(a + 1);
    for (i = 0; i < ED_MENU_N && ED_MENU[i][1] != a[0]; i++)
        ;
    if (i == ED_MENU_N) {
        ed_b(1); ed_b(a[0]); ed_v(v);
        return 1;
    }
    row = ED_MENU[i][0];
    if (row == ED_BM) {                                /* (0.2.19: BASS MONO: OFF .. 400 Hz; the music's, nothing in the settings to save) */
        song.bm = (int16_t)clamp(v, 0, (int32_t)BM_N);
        ui.force = 1;
        ed_b(0);
        ed_b(a[0]);
        ed_v(song.bm);
        return 1;
    }
    if (row >= ED_MODS) {                              /* (0.2.14: a MODULES switch, as its knob: 0 OFF, 1 ON; saved with the settings) */
        k = row - ED_MODS;
        mods_put(1u << k, v <= 0);
        ui.force = 1;
        ed_b(ed_menu_save());
        ed_b(a[0]);
        ed_v((int32_t)!((mods_get() >> k) & 1u));
        return 1;
    }
    lo = row == MI_TUNE ? TUNE_MIN : 0;                /* (a number: from its min) */
    menu_put(row, (uint32_t)(clamp(v, lo, lo + (int32_t)menu_n(row) - 1) - lo));
    if (row == MI_SERIAL)
        menu_serial_at = fm1_ms | 1u;                  /* (applied 200 ms after this reply: usb_serial_apply) */
    ui.force = 1;                                      /* (the MENU page, if shown, redraws) */
    ed_b(MI_PROJECT(row) ? 0u : ed_menu_save());       /* (the project's: in the music, no settings to save) */
    ed_b(a[0]);
    ed_v((int32_t)menu_get(row) + lo);
    return 1;
}
