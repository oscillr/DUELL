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
 * not in the settings record: rc 0, nothing for settings_save to write. TUNE is a number (until 0.2.21 EDM_INT: the cents -50..50, unit "ct"; DUELL 0.2.22 EDM_TENTHS: the frequency of A4
 * in 0.1 Hz, 4153..4662 = 415.3 .. 466.2 Hz, unit "Hz", a value out of range reads 440.0; v14 holds it as it is, no coarse / fine pair) */
enum { ED_MENU_DESC = 72, ED_MENU_SET };
/* DUELL 0.2.14: GLO > MODULES' four switches (core.h MODS_*, ui.c mods_put) are device settings but no MENU rows (they live on the GLO page):
 * here they are the rows ED_MODS + 0..3 (ARP, SEQ, SENDS, PART FX; no MI_ row has such a number), ids 28..31, values 0 OFF / 1 ON (the
 * knob's order: left OFF, right ON; ON the default), their tab index MTAB_COUNT (4) named "MODULES" (no MENU tab: the device shows them
 * on GLO > MODULES). A SET switches as the page's knob does and saves the settings record */
#define ED_MODS 0x60u
#define ED_MODS_TAB "MODULES"
/* DUELL 0.2.19 (owner: BASS MONO is a SOUND setting, not a MENU one): id 33, BASS MONO, is the page GLO > MASTER's K1 (core.h G_BM, song.bm: kept with the music, the
 * multis and the autosave). Like TUNE it is described here as a setting, but its row ED_BM is no MI_ row (the MENU has none) and its tab index MTAB_COUNT + 1 is named
 * "MASTER" (no MENU tab): the editor shows it on the GLOBAL tab. A SET changes the value as the knob does; rc 0 (the music holds it, no settings to save). The unit
 * shows no page in DUEL; the value is kept and a SET is still applied */
#define ED_BM 0x64u
#define ED_BM_TAB "MASTER"
/* DUELL 0.2.21 (owner: the master compressor is a DEVICE setting, page GLO > COMP): ids 34..37, THRESH, RATIO, ATTACK, RELEASE (core.h G_CT .. G_CL, song.cp[0..3]), rows ED_CP + 0..3 (no MI_ rows), tab
 * MTAB_COUNT + 1 named "MASTER" (as BASS MONO: the editor shows them on the GLOBAL tab). THRESH is a number (EDM_INT: -40 .. 0, unit "dB"; the stored index is the value + 40), the others are lists of names
 * (RATIO "1:1" is the stage off). A SET changes the value as the knob does, cmp_sync (ui.c) takes it into the settings word and the record is saved (rc as the MENU's: 0 saved, 3 RAM only, 4 queued). The unit
 * draws no card in DUEL; the values are kept and a SET is still applied */
#define ED_CP 0x68u
/* DUELL 0.2.22 (owner: the sidechain's MIDI setup is a DEVICE setting, page GLO > SIDECHAIN): ids 38..41, P1 CH, P1 NOTE, P2 CH, P2 NOTE (fx.c sc_midi, the settings bytes [15][14..17]; ui_input.c sc_get / sc_put), rows
 * ED_SC + 0..3 (no MI_ rows), tab MTAB_COUNT + 2 named "SIDECHAIN" (the editor shows them on the GLOBAL tab). All four are numbers (EDM_INT): a channel 1..16, a note 0..127 (0 = ANY; the editor names it as the unit does,
 * F_DKNOTE). A SET changes the value as the knob does and saves the settings record (rc as the MENU's); no patch, multi or project holds them. Editable whatever the parts' DUCK SRC is */
#define ED_SC 0x6Cu
#define ED_SC_TAB "SIDECHAIN"
/* DUELL 0.2.23 (owner: two more master stages, DEVICE settings like COMP): id 42 WIDTH (GLO > WIDTH, 0..200 %, 100 = as it is) (core.h G_WD,
 * song.ms[0]; ui.c ms_sync), row ED_MS (no MI_ row), tab MTAB_COUNT + 1 "MASTER" (as BASS MONO and COMP: the editor's GLOBAL tab). A number (EDM_INT; unit "%"). 0.2.25: id 43, DRIVE, is gone (the saturator is removed).
 * A SET changes the value as the knob does and saves the settings record (rc as the MENU's). The unit draws no card in DUEL; the values are kept and a SET is still applied */
#define ED_MS 0x70u
#if FELUCCA_CDC
#define ED_ROW_SERIAL(row) ((row) == MI_SERIAL)       /* (the USB SERIAL row exists in developer builds only) */
#else
#define ED_ROW_SERIAL(row) 0
#endif
enum { EDM_ENUM, EDM_INT, EDM_TENTHS };                /* kind: names follow; or a unit (THRESH); 0.2.22: a number in tenths of its unit (TUNE: 4400 = 440.0 Hz, shown with one decimal) */
static const uint8_t ED_MENU[][2] = {
    {MI_COLOR, 0}, {MI_ANIM, 3}, {MI_LEDS, 4}, {MI_HOLD, 5}, {MI_ACCEL, 6},
                                                       /* (id 7, FX LATCH, retired in DUELL 0.2.2: the FX hold layer is removed; never reused) */
                                                       /* (id 9, OUTPUT EQ FLAT / LOWCUT / BASS+, retired in DUELL 0.2.19: the low cut and BASS+ are removed; never reused) */
                                                       /* (id 10, USB LEVEL MASTER / FIXED, retired in DUELL 0.2.25: USB audio is always fixed; never reused) */
#if FELUCCA_CDC
    {MI_SERIAL, 11},                                   /* (developer builds only: the product has no console and no USB SERIAL row; the id stays retired there, never reused) */
#endif
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
#if DUELL_VELO
    {MI_AFTERT, 45},                                    /* DUELL 1.0: AFTERTOUCH OFF / ON, CONTROL, its own line (the keys' channel pressure; KEY VELOCITY is id 44) */
    {MI_KEYVEL, 44},                                    /* DUELL (job D): KEY VELOCITY OFF / ON, CONTROL (developer builds with KEY SENSE only; the id is free in every other build) */
#endif
    {MI_PERFLEAVE, 32},                                 /* DUELL 0.2.14: PERF LEAVE FADE / KEEP, CONTROL (leaving PERFORM / a sound load: SPACE and KILL to neutral, or stay) */
    {ED_BM, 33},                                        /* DUELL 0.2.19: BASS MONO OFF / 40 .. 400 Hz, the MASTER page (the master mono below the corner; not in DUEL); no MENU row (ED_BM) */
    {ED_CP + 0u, 34}, {ED_CP + 1u, 35},                 /* DUELL 0.2.21: COMP THRESH, RATIO, ATTACK, RELEASE, the page GLO > COMP (a device setting; not in DUEL); no MENU rows (ED_CP) */
    {ED_CP + 2u, 36}, {ED_CP + 3u, 37},
    {ED_SC + 0u, 38}, {ED_SC + 1u, 39},                 /* DUELL 0.2.22: SIDECHAIN P1 CH, P1 NOTE, P2 CH, P2 NOTE, the page GLO > SIDECHAIN (a device setting); no MENU rows (ED_SC) */
    {ED_SC + 2u, 40}, {ED_SC + 3u, 41},
    {ED_MS + 0u, 42},                                   /* DUELL 0.2.23: WIDTH (GLO > WIDTH), a device setting; not in DUEL; no MENU row (ED_MS). 0.2.25: id 43 (DRIVE, the saturator) is removed and not reused */
};
_Static_assert(MI_COUNT < ED_MODS && ED_MODS + 4u <= ED_BM && ED_BM < ED_CP && ED_CP + 4u <= ED_SC && ED_SC + 4u <= ED_MS, "editor_menu.c: the MODULES rows, BASS MONO, COMP, SIDECHAIN and WIDTH are no MI_ row");
#define ED_MENU_N NELEM(ED_MENU)

/* rc as UI_SET: 0 applied and saved, 3 applied in RAM only (no flash, a failed write), 4 queued until STOP */
static uint32_t ed_menu_save(void)
{
    settings_save();
#if FELUCCA_FLASH
    if (!flash_ok || persist_pending == 2u) return 3;
    if (persist_pending == 1u) return 4;
    {
        persist_t p;
        persist_base(&p);
        settings_export(&p);
        return persist_is_saved(&p) ? 0u : 3u;
    }
#else
    return 3;
#endif
}

/* T0227 (flicker fix): a MENU_SET of a setting with many values (BASS MONO, WIDTH, COMP, SIDECHAIN, the MENU rows that are not two-value switches) is saved as
 * the unit's own knob saves it: ui.mset_dirty, written once the setting has rested ~2 s (ui_input.c morph_settings' timer; while something sounds the write waits:
 * project.c settings_poll). A slider dragged in the editor so erases the flash once, not once per step (st_erase darkens the LED rows for 17..40 ms each time).
 * MODULES: mods_put queues it the same way. -> rc 0 nothing to save (the record holds it already), 4 queued, 3 RAM only (no flash, or a failed write waiting for its retry) */
static uint32_t ed_menu_defer(void)
{
#if FELUCCA_FLASH
    persist_t p;
    if (!flash_ok || persist_pending == 2u)
        return 3;
    persist_base(&p);
    settings_export(&p);
    if (persist_is_saved(&p))
        return persist_pending ? 4u : 0u;
    ui.mset_dirty = 1;
    ui.mset_t = fm1_ms;
    return 4;
#else
    ui.mset_dirty = 1;
    ui.mset_t = fm1_ms;
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
        if (row >= ED_MS) {                            /* (0.2.23: WIDTH 0..200 % as a number; tab MASTER) */
            ed_b(ED_MENU[a[0]][1]);
            ed_b(EDM_INT);
            ed_v(song.ms[0]);
            ed_v(0);
            ed_v(GP[G_WD].max);
            ed_str(GP[G_WD].label, 12);
            ed_str("%", 12);
            ed_b(MTAB_COUNT + 1u);
            ed_str(ED_BM_TAB, 12);
            return 1;
        }
        if (row >= ED_SC) {                            /* (0.2.22: SIDECHAIN: P1 CH, P1 NOTE, P2 CH, P2 NOTE as numbers; tab SIDECHAIN) */
            k = row - ED_SC;
            ed_b(ED_MENU[a[0]][1]);
            ed_b(EDM_INT);
            ed_v((int32_t)sc_get(k) + ((k & 1u) ? 0 : 1));
            ed_v((k & 1u) ? 0 : 1);
            ed_v((k & 1u) ? 127 : 16);
            ed_str(k == 0u ? "P1 CH" : k == 1u ? "P1 NOTE" : k == 2u ? "P2 CH" : "P2 NOTE", 12);
            ed_str("", 12);
            ed_b(MTAB_COUNT + 2u);
            ed_str(ED_SC_TAB, 12);
            return 1;
        }
        if (row >= ED_CP) {                            /* (0.2.21: COMP: THRESH as a number, the others as lists; tab MASTER) */
            char nm[16], u[8];
            const char *unit;
            k = row - ED_CP;
            ed_b(ED_MENU[a[0]][1]);
            ed_b(k == 1u ? EDM_ENUM : EDM_INT);         /* (0.2.24: ATTACK and RELEASE are fine, 121 / 81 steps: numbers (the index; the editor formats them from layout.json's COMP descriptors), not lists) */
            ed_v(song.cp[k] + (k == 0u ? -40 : 0));
            ed_v(k == 0u ? -40 : 0);
            ed_v(GP[G_CT + k].max - (k == 0u ? 40 : 0));
            ed_str(GP[G_CT + k].label, 12);
            if (k == 0u) {
                ed_str("dB", 12);
            } else if (k != 1u) {
                ed_str("", 12);
            } else {
                for (i = 0; i <= (uint32_t)GP[G_CT + k].max; i++) {
                    unit = "";
                    param_format(&GP[G_CT + k], (int32_t)i, nm, &unit);
                    str_cpy(u, unit, 8);
                    if (u[0] && u[0] != ':')
                        str_cpy(nm + str_len(nm), " ", 2), str_cpy(nm + str_len(nm), u, 6);
                    ed_str(nm, 12);
                }
            }
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
        ed_b(row == MI_TUNE ? EDM_TENTHS : EDM_ENUM);
        ed_v((int32_t)menu_get(row) + lo);
        ed_v(lo);
        ed_v((int32_t)k - 1 + lo);
        ed_str(MI_NAME[row], 12);
        if (row == MI_TUNE)
            ed_str("Hz", 12);                          /* (EDM_TENTHS: its unit) */
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
    if (row == ED_BM) {                                /* (0.2.19: BASS MONO: OFF .. 400 Hz; a device setting since 0.2.24: ms_sync takes it into the settings bytes) */
        ms_sync();                                     /* (its first look reads the bytes: before the write, so the write is not read back over) */
        song.bm = (int16_t)clamp(v, 0, (int32_t)BM_N);
        ms_sync();                                     /* (the settings bytes now: the save below writes them) */
        ed_b(ed_menu_defer());                         /* (T0227: saved once it rests; no forced redraw: the card and the picture redraw by their own keys) */
        ed_b(a[0]);
        ed_v(song.bm);
        return 1;
    }
    if (row >= ED_MS) {                                /* (0.2.23: WIDTH: clamped; ms_sync takes it into the settings bytes, the record saved) */
        ms_sync();                                     /* (its first look reads the bytes: before the write) */
        song.ms[0] = (int16_t)clamp(v, 0, GP[G_WD].max);
        ms_sync();
        ed_b(ed_menu_defer());
        ed_b(a[0]);
        ed_v(song.ms[0]);
        return 1;
    }
    if (row >= ED_SC) {                                /* (0.2.22: SIDECHAIN: a channel 1..16 or a note 0..127, clamped; the settings record saved) */
        k = row - ED_SC;
        sc_put(k, (uint32_t)clamp(v - ((k & 1u) ? 0 : 1), 0, (k & 1u) ? 127 : 15));
        ed_b(ed_menu_defer());
        ed_b(a[0]);
        ed_v((int32_t)sc_get(k) + ((k & 1u) ? 0 : 1));
        return 1;
    }
    if (row >= ED_CP) {                                /* (0.2.21: COMP: a number or a list index, clamped; the settings record saved) */
        k = row - ED_CP;
        song.cp[k] = (int16_t)clamp(v + (k == 0u ? 40 : 0), 0, GP[G_CT + k].max);
        cmp_sync();                                    /* (the settings word now: the save below writes it) */
        ed_b(ed_menu_defer());
        ed_b(a[0]);
        ed_v(song.cp[k] + (k == 0u ? -40 : 0));
        return 1;
    }
    if (row >= ED_MODS) {                              /* (0.2.14: a MODULES switch, as its knob: 0 OFF, 1 ON; saved with the settings) */
        k = row - ED_MODS;
        mods_put(1u << k, v <= 0);                     /* (as the MODULES knob: it redraws and queues the save itself when the switch changes) */
        ed_b(ed_menu_defer());
        ed_b(a[0]);
        ed_v((int32_t)!((mods_get() >> k) & 1u));
        return 1;
    }
    lo = row == MI_TUNE ? TUNE_MIN : 0;                /* (a number: from its min) */
    menu_put(row, (uint32_t)(clamp(v, lo, lo + (int32_t)menu_n(row) - 1) - lo));
#if FELUCCA_CDC
    if (row == MI_SERIAL)
        menu_serial_at = fm1_ms | 1u;                  /* (applied 200 ms after this reply: usb_serial_apply) */
#endif
    ui.force = 1;                                      /* (the MENU page, if shown, redraws) */
    ed_b(MI_PROJECT(row) ? 0u : menu_n(row) == 2u || ED_ROW_SERIAL(row) ? ed_menu_save() : ed_menu_defer());   /* (the project's: in the music, no settings to save;
                                                         * T0227: as the menu's knob, a two-value switch saved at once, the others once they rest) */
    ed_b(a[0]);
    ed_v((int32_t)menu_get(row) + lo);
    return 1;
}
