/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MENU's rows and their settings: what ui_menu.c draws and steps, and what the editor reads and sets (editor_menu.c
 * MENU_DESC / MENU_SET). One apply path for both: menu_put. Grouped: the screen (COLOR, ANIM, LEDS; 1.1.5:
 * SCREEN OFF; 1.2: SCOPE; 1.3: HOME), the
 * controls (HOLD: the layer threshold, KNOB ACCEL; 1.2: SCALE LEDS, the keys, STEP PREVIEW, CHORD
 * ENTRY), the sound (1.x SPEAKER EQ: FLAT LOWCUT BASS+, removed in 0.2.19;
 * 1.x USB LEVEL, removed in 0.2.25; 1.1: CLICK, CLICK LEVEL, COUNT-IN; 1.2: TUNE), MIDI (1.2: MIDI IN; 1.5: LEARN CLEAR), USB SERIAL, RESTORE
 * LAST (1.2), then CALIBRATION (the setup screen: HARDWARE CALIBRATION), TEST (1.2: INFO; DUELL 0.2.2: the CPU test screen, the id is the old INFO's), GUIDE (DUELL 0.2.11:
 * the first-start guide's pages again, ui_guide.c; no editor id) and ABOUT, the rows with no value
 * (menu_valued; up to 0.2.13 the rows after MI_VALUES). 1.0.5: in four tabs (MI_TAB); 1.2: five.
 * DUELL 0.1 (owner's feedback after the first flash): MENU holds no MIDI setting any more. MIDI IN (OWN CH / SELECTED, the
 * project's G_ROUTE) and LEARN CLEAR moved to GLO's MIDI pages (MIDI, SIDECHAIN, MIDI LEARN); the MIDI tab is gone (four tabs). HOME
 * became two rows: OVERVIEW (what HOME's last page shows under its cards: SCOPE or PARTS) and HOME OPENS (the page the HOME
 * button, power-on and leaving the menu open: MORPH, MODE, MIXER or OVERVIEW; CONTROL tab).
 * 1.2: TUNE is not a setting: it edits the project's G_TUNE, the value the project holds and saves as before (no format change;
 * a project load brings its own); nothing of its is in the settings record (MI_PROJECT: menu_get / menu_put read and write song.g) */
/* DUELL 0.2.14 (the owner's MENU rework, 2026-10-14): new names (ANIM -> ANIMATION, HOLD -> BUTTON HOLD,
 * SPEAKER EQ -> OUTPUT EQ, MODE RECALL -> MULTI LOAD (KEEPS MODE / SETS MODE), CHORD ENTRY -> STEP KEYS (WRITE / ADD), CALIBRATION ->
 * BUTTON MAP), new groups (SCALE LEDS in DISPLAY), SCOPE and OVERVIEW removed (their editor ids 18 and 23 retired, never reused).
 * MI order = the order the device shows (a tab's rows follow each other); the editor's ids are editor_menu.c ED_MENU's, by row, so
 * the order is no id. MI_NAME is the editor's name (at most 12 characters, ed_str) and the device's label unless MI_LABEL has one */
enum { MI_COLOR, MI_LEDS, MI_SCLLED, MI_ANIM, MI_SCROFF, MI_CPU,                    /* DISPLAY */
       MI_MODERECALL, MI_HOMEPG, MI_ACCEL, MI_HOLD, MI_CHADD, MI_PREVIEW, MI_PERFLEAVE,  /* CONTROL (MULTI LOAD .. STEP KEYS .., PERF LEAVE) */
#if DUELL_VELO
       MI_KEYVEL,                                      /* (KEY VELOCITY OFF / ON: builds with KEY SENSE built in only, DUELL_VELO; no row, no editor id otherwise) */
       MI_AFTERT,                                      /* (DUELL 1.0: AFTERTOUCH OFF / ON, its own line, the same builds) */
#endif
       MI_CLICK, MI_CLKLVL, MI_COUNTIN, MI_TUNE,                 /* AUDIO (CLICK .. COUNT-IN, TUNE; 0.2.19: OUTPUT EQ removed; 0.2.25: USB LEVEL removed) */
       MI_RESTORE, MI_PANEL,                                                        /* SYSTEM (RESTORE LAST, BUTTON MAP) */
#if FELUCCA_CDC
       MI_SERIAL,                                      /* (USB SERIAL: developer builds only, FELUCCA_CDC: the product has no serial console, so no row that would do nothing) */
#endif
#if DUELL_TEST_MODE
       MI_TEST,                                        /* (developer builds only: DUELL_TEST_MODE; the release has no TEST row) */
#endif
       MI_GUIDE,                                       /* (DUELL 0.2.11: the first-start GUIDE again; no value, no editor id) */
       MI_ABOUT, MI_ERASE, MI_COUNT };
                                                       /* (1.1: the metronome's rows in AUDIO; 1.2: RESTORE LAST in
                                                        * SYSTEM, SCALE LEDS STEP PREVIEW CHORD ENTRY in CONTROL, SCOPE in
                                                        * DISPLAY, TUNE in AUDIO, MIDI IN in MIDI, INFO (now TEST) in SYSTEM;
                                                        * 1.3: HOME in DISPLAY). DUELL 0.2.2 removed STYLE (FLAT only), LARGE and HELP (their editor ids 1, 2 and 24 are retired) and FX LATCH (id 7: the FX hold layer is gone).
                                                        * DUELL 0.2.14 removed SCOPE (id 18: the scope always draws OUT, after MASTER) and OVERVIEW (id 23: the page is gone from HOME's round) */
/* the rows that open a screen or a dialog and hold no value (BUTTON MAP, TEST, GUIDE, ABOUT, ERASE DATA; 0.2.14: not the last rows
 * any more, BUTTON MAP sits between two valued rows) */
static int menu_valued(uint32_t row);
/* the rows that open a screen with no value (TEST, GUIDE, ABOUT): OCT+ or a knob turned right opens them, a knob turned left or OCT- closes
 * them again (ui_menu.c menu_input; the GUIDE: its own keys, ui_guide.c). BUTTON MAP (was CALIBRATION) is a setup, not read-only: OCT+ only */
#if DUELL_TEST_MODE
#define MI_READONLY(row) ((row) == MI_TEST || (row) == MI_GUIDE || (row) == MI_ABOUT)
#else
#define MI_READONLY(row) ((row) == MI_GUIDE || (row) == MI_ABOUT)
#endif
static int menu_valued(uint32_t row) { return row < MI_COUNT && row != MI_PANEL && row != MI_ERASE && !MI_READONLY(row); }   /* (a row with a value) */
#define MI_PROJECT(row) ((row) == MI_TUNE)   /* the project's value (song.g), not a setting */
static const char *const MI_NAME[MI_COUNT] = {"COLOR", "LEDS", "SCALE LEDS", "ANIMATION", "SCREEN OFF", "CPU METER",
                                              "MULTI LOAD", "HOME OPENS", "KNOB ACCEL", "BUTTON HOLD", "STEP KEYS", "STEP PREVIEW", "PERF LEAVE",
#if DUELL_VELO
                                              "KEY VELOCITY", "AFTERTOUCH",
#endif
                                              "CLICK", "CLICK LEVEL", "COUNT-IN", "TUNE",
                                              "RESTORE LAST", "BUTTON MAP",
#if FELUCCA_CDC
                                              "USB SERIAL",
#endif
#if DUELL_TEST_MODE
                                              "TEST",
#endif
#if DUELL_BENCH
                                              "BENCH",         /* (bench builds: the GUIDE row opens the BENCH screen, src/bench.c bench_input; the row and its
                                                                * code stay as they are, so the menu's code keeps its size: the first-start GUIDE still opens) */
#else
                                              "GUIDE",
#endif
                                              "ABOUT", "ERASE DATA"};
/* DUELL 0.2.14: the device's label where it is not MI_NAME (0: MI_NAME). One '\n' at most, a designed break (nothing wraps by itself):
 * the line after it is drawn small (S, MID: a hint, or the rest of a long name) and the row takes two units (ui_menu.c) */
static const char *const MI_LABEL[MI_COUNT] = {
#if FELUCCA_CDC
    [MI_SERIAL] = "USB SERIAL\nDEVELOPER CONSOLE"      /* (USB SERIAL: its hint) */
#else
    0
#endif
};
static const char *menu_label(uint32_t row)
{
    return row >= MI_COUNT ? "" : MI_LABEL[row] ? MI_LABEL[row] : MI_NAME[row];
}
/* 1.0.5: the MENU's tabs (ui_menu.c: ALGORITHM steps between them, PRESETS among one tab's rows; the editor gets a
 * row's tab after its MENU_DESC reply). A tab's rows follow each other in MI order (tests/ui_test.c checks it). DUELL 0.2.14:
 * a row is one unit (21 px and a 1 px rule) or two (a two-line label); a page holds MPAGE_UNITS units; a tab with more goes on
 * on a page 2 (menu_place), its pill then reads "CONTROL 1/2". Tabs are in the order the device shows them; a tab's index is what
 * the editor is told, not an id (1.2: MIDI came in before SYSTEM, SYSTEM's index 3 -> 4) */
enum { MTAB_DISPLAY, MTAB_CONTROL, MTAB_AUDIO, MTAB_SYSTEM, MTAB_COUNT };   /* (DUELL: MIDI is GLO's, no tab here) */
#define MPAGE_UNITS 7u
static const char *const MTAB_NAME[MTAB_COUNT] = {"DISPLAY", "CONTROL", "AUDIO", "SYSTEM"};
static const uint8_t MI_TAB[MI_COUNT] = {
    MTAB_DISPLAY, MTAB_DISPLAY, MTAB_DISPLAY, MTAB_DISPLAY, MTAB_DISPLAY, MTAB_DISPLAY,   /* COLOR LEDS SCALE LEDS ANIMATION SCREEN OFF CPU METER */
    MTAB_CONTROL, MTAB_CONTROL, MTAB_CONTROL, MTAB_CONTROL, MTAB_CONTROL, MTAB_CONTROL,   /* MULTI LOAD HOME OPENS KNOB ACCEL BUTTON HOLD STEP KEYS
                                                                                          * STEP PREVIEW */
    MTAB_CONTROL,                                                                         /* PERF LEAVE (FADE / KEEP; the 7th unit: one page is full) */
#if DUELL_VELO
    MTAB_CONTROL, MTAB_CONTROL,                                                           /* KEY VELOCITY, AFTERTOUCH (OFF / ON; CONTROL page 2) */
#endif
    MTAB_AUDIO, MTAB_AUDIO, MTAB_AUDIO, MTAB_AUDIO,                                       /* CLICK CLICK LEVEL COUNT-IN TUNE */
    MTAB_SYSTEM, MTAB_SYSTEM,                                                             /* RESTORE LAST BUTTON MAP */
#if FELUCCA_CDC
    MTAB_SYSTEM,                                                                          /* USB SERIAL */
#endif
#if DUELL_TEST_MODE
    MTAB_SYSTEM,                                                            /* TEST (1.2: INFO) */
#endif
    MTAB_SYSTEM,                                                            /* GUIDE (DUELL 0.2.11) */
    MTAB_SYSTEM,                                                            /* ABOUT */
    MTAB_SYSTEM,                                                            /* ERASE DATA (DUELL 0.2.2: OCT+ opens its dialog, ui_menu.c) */
};
/* TUNE (DUELL 0.2.22: MASTER TUNE like a Roland keyboard's): the project's G_TUNE, the frequency of A4 in 0.1 Hz, 415.3 .. 466.2 Hz (core.h TUNE_MIN .. TUNE_MAX); as a row
 * its values 0..TUNE_N - 1 (value - TUNE_MIN, 510 steps, 247 = 440.0 Hz). The editor gets it as a number in tenths (EDM_TENTHS: min..max, its unit "Hz") */
#define TUNE_N ((uint32_t)(TUNE_MAX - TUNE_MIN + 1))
typedef char mtab_fits_ui[sizeof ui.menu_row >= MTAB_COUNT ? 1 : -1];   /* (ui.c: the row last picked per tab) */
static uint32_t mtab_first(uint32_t t)                 /* a tab's first row */
{
    uint32_t i = 0;
    while (i < MI_COUNT && MI_TAB[i] != t)
        i++;
    return i;
}
static uint32_t mtab_rows(uint32_t t)                  /* .. and how many it has */
{
    uint32_t i, n = 0;
    for (i = 0; i < MI_COUNT; i++)
        n += MI_TAB[i] == t;
    return n;
}
/* DUELL 0.2.14: a row's units (1, or 2 for a two-line label) and its place: the page of its tab and the unit it starts at. Rows fill a
 * page in MI order until the next one does not fit; the rest go on the next page (a few loops over ~26 rows: no table to keep) */
static uint32_t menu_units(uint32_t row)
{
    const char *l = menu_label(row);
    while (*l && *l != '\n')
        l++;
    return *l ? 2u : 1u;
}
static uint32_t menu_place(uint32_t row, uint32_t *unit)   /* the page (0 ..) of row in its tab; *unit its first unit there */
{
    uint32_t i = mtab_first(MI_TAB[row % MI_COUNT]), pg = 0, u = 0, n;
    for (;; i++) {
        n = menu_units(i);
        if (u + n > MPAGE_UNITS) {
            pg++;
            u = 0;
        }
        if (i >= row % MI_COUNT)
            break;
        u += n;
    }
    if (unit)
        *unit = u;
    return pg;
}
static uint32_t mtab_pages(uint32_t t)                 /* a tab's pages (1 ..) */
{
    return menu_place(mtab_first(t) + mtab_rows(t) - 1u, 0) + 1u;
}
/* the two-valued rows: a bit of ui_prefs (PREF_REC: of ui_rec_prefs, ui.c PREF_SCALE_LEDS_OFF) and its names (bit clear =
 * the default, bit set). A step up (a knob right,
 * OCT+) = ON on the ON / OFF rows (the switch's knob to the right; ANIM, USB SERIAL: their bit clears), else the
 * second name; a step down the other (menu_step) */
typedef struct { uint8_t row; uint32_t bit; const char *name[2]; } menu_flag_t;
static const menu_flag_t MENU_FLAGS[] = {
    {MI_ACCEL, PREF_ACCEL, {"OFF", "ON"}},            /* #52: ui_input.c accel */
#if FELUCCA_CDC
    {MI_SERIAL, PREF_SERIAL_OFF, {"ON", "OFF"}},       /* #67: OFF, no serial console (usb_serial_apply) */
#endif
    {MI_RESTORE, PREF_RESTORE_OFF, {"ON", "OFF"}},     /* 1.2, #130: OFF, no autosave, power-on as new (project.c) */
    {MI_SCLLED, PREF_SCALE_LEDS_OFF, {"AUTO", "OFF"}}, /* 1.2, Discussion #127; 0.2.5: AUTO (default), the keys show the scale when it is not CHROMATIC; OFF, they do not (ui_input.c) */
                                                       /* (1.2's SCOPE OUT / MIX, ui_prefs2 bit 0: removed in DUELL 0.2.14, the bit ignored) */
    {MI_PREVIEW, PREF_PREVIEW, {"OFF", "ON"}},         /* 1.2, Discussion #169: ON, the cursor's step sounds (ui_input.c) */
    {MI_CPU, PREF_CPU_OFF, {"ON", "OFF"}},             /* DUELL 0.2.9: OFF, no CPU bar under the header's battery (ui_draw.c); bit clear = ON, as every older setting */
    {MI_MODERECALL, PREF_MODE_RECALL, {"KEEPS MODE", "SETS MODE"}},   /* DUELL 0.2.11 MODE RECALL, 0.2.14 MULTI LOAD: KEEPS MODE (default, bit clear) a multi load
                                                       * leaves the MODE page (MODE, BALANCE, XFADE, SPLIT) as it is; SETS MODE, it sets the ones saved in the multi (multi.c) */
    {MI_CHADD, PREF_CHORD_ADD, {"WRITE", "ADD"}},      /* 1.2, #155 CHORD ENTRY HOLD / ADD, 0.2.14 STEP KEYS WRITE / ADD: ADD, keys add to the cursor step (ui_input.c) */
};
/* (OUTPUT EQ, the low cut and BASS+, was removed in 0.2.19) */
static const char *const HOLD_N[4] = {"0.3 s", "0.4 s", "0.5 s", "0.6 s"};   /* (the editor's names; the menu draws its own) */
/* 1.1 (Discussion #131): the metronome (click.c) and the count-in (seq.c); ui.c ui_rec_prefs holds them */
static const char *const CLICK_N[3] = {"OFF", "REC", "ON"};          /* REC: while a track is armed and playing */
static const char *const CLKLVL_N[3] = {"LOW", "MID", "HIGH"};
static const char *const COUNTIN_N[3] = {"OFF", "1 BAR", "2 BARS"};
/* 1.1.5: SCREEN OFF (ui.c scr_*: the screen dark after this long without panel input; NEVER the default since 1.1.5.1) */
static const char *const SCROFF_N[5] = {"NEVER", "5 MIN", "15 MIN", "30 MIN", "60 MIN"};
/* ANIM (#46: OFF, values snap: ui_draw.c roll_note, ui_graph.c pr_follow; no power-on sweep, main.c boot_leds). 1.2
 * (Discussion #135): IDLE, as ON and the LEDs' idle animation after a minute without input (ui_input.c idle_leds; ui.c
 * ui_idle). Its values keep 1.1's: 0 ON, 1 OFF (the editor's), 2 IDLE; the menu steps them OFF, ON, IDLE (ANIM_ORD) */
static const char *const ANIM_N[3] = {"ON", "OFF", "IDLE"};
static const uint8_t ANIM_ORD[3] = {1, 0, 2};          /* (a value's place in the menu's order, and back: its own inverse) */
/* (OVERVIEW, was HOME in 1.3: what HOME's last page showed under its cards, SCOPE or PARTS; removed with the page in DUELL 0.2.14,
 * its byte [15][26] kept as stored) */
/* HOME OPENS (DUELL): the page the HOME button opens from another page, power-on and leaving the menu (ui.c go_main): MORPH (the
 * default, stored 0), MODE, MIXER; ui_home_pg (ui.c), an unknown stored code reads as MORPH (0.2.14: OVERVIEW's 3 too) */
static const char *const HOMEPG_N[HP_COUNT] = {"MORPH", "MODE"};   /* (0.2.25: MIXER left HOME for GLO; a stored MIXER reads MORPH) */
/* (SCALE LINK, a CONTROL row of 0.2.1 builds before the global scale, is RETIRED: the scale is the instrument's, core.h; the editor protocol never
 * listed it, so no id is lost) */

/* MENU > USB SERIAL (#67). The serial console is a developer tool (README: FELUCCA_CDC); since memory trim 1 only developer builds (DUELL_BENCH / DUELL_TEST_MODE) have it, and the row. The product never has it: usb_serial_apply is empty there, and the saved PREF_SERIAL_OFF bit is read by nobody. DUELL 0.2.14: OFF is the default (a new unit,
 * ERASE DATA, and once on a unit that had the old default ON: settings_persist.c settings_defaults). ON presents it,
 * the descriptors byte for byte as before; OFF re-enumerates as audio + MIDI only, device class 0 (the bytes of a
 * FELUCCA_CDC=0 build): macOS 13-15 then attach their USB audio driver (with the console Apple's CDC composite driver
 * takes the device and the audio input never appears). USB-MIDI stays, and with it the editor, the update installer
 * and the soft key (SysEx on EP1). Applied at boot before USB starts (main.c: enumerated with it from the start, never
 * on then off) and while the menu is closed (ui_input: one re-enumeration when it closes, not one per KNOB 1 step).
 * A change from the editor (MENU_SET) waits 200 ms (menu_serial_at) so its reply leaves before the device drops off
 * the bus: the whole device re-enumerates, USB-MIDI and audio too */
#if FELUCCA_CDC
static uint32_t menu_serial_at;                        /* fm1_ms | 1 of the editor's change; 0 none */
#endif
static void usb_serial_apply(void)
{
#if FELUCCA_CDC
    if (menu_serial_at && fm1_ms - (menu_serial_at & ~1u) < 200u)   /* (& ~1: at an even fm1_ms the | 1 was 1 ms ahead) */
        return;
    menu_serial_at = 0;
#if DUELL_BENCH
    usb_cdc_switch(FELUCCA_CDC_DEFAULT);                /* (bench builds: the console is always presented, whatever USB SERIAL says: the bench suite's runner needs it) */
#else
    usb_cdc_switch(FELUCCA_CDC_DEFAULT && !(ui_prefs & PREF_SERIAL_OFF));   /* (a FELUCCA_CDC_DEFAULT=0 build: off) */
#endif
#endif
}
static const menu_flag_t *menu_flag(uint32_t row)
{
    uint32_t i;
    for (i = 0; i < NELEM(MENU_FLAGS); i++)
        if (MENU_FLAGS[i].row == row)
            return &MENU_FLAGS[i];
    return 0;
}

/* a row's value as 0..menu_n(row) - 1 in the order the menu steps it (LEDS: OFF LOW MEDIUM MAX, LEDS_MENU) */
static uint32_t menu_n(uint32_t row)
{
    return row == MI_COLOR ? NPALETTES : (row >= MI_CLICK && row <= MI_COUNTIN) ? 3u :
           row == MI_HOLD ? 4u : row == MI_LEDS ? LEDS_COUNT : row == MI_SCROFF ? NELEM(SCROFF_N) :
           row == MI_TUNE ? TUNE_N : row == MI_ANIM ? 3u : row == MI_HOMEPG ? (uint32_t)HP_COUNT : 2u;
}
static uint32_t menu_get(uint32_t row)
{
    const menu_flag_t *f = menu_flag(row);
    uint32_t i = 0;
    if (f)
        return ((ui_prefs | (uint32_t)ui_rec_prefs << 8 | (uint32_t)ui_prefs2 << 16) & f->bit) != 0;
    switch (row) {
    case MI_COLOR: return settings.palette % NPALETTES;
    case MI_HOLD: return settings_hold % 4u;
    case MI_SCROFF: return scr_get();
    case MI_ANIM: return ui_prefs & PREF_ANIM_OFF ? 1u : (ui_idle & 1u) ? 2u : 0u;
    case MI_HOMEPG: return ui_home_pg < HP_COUNT ? ui_home_pg : HP_MORPH;
    case MI_PERFLEAVE: return perf_keep();
#if DUELL_VELO
    case MI_KEYVEL: return keyvel_on();                /* 1 = ON (the stored bit is ON: set = ON, clear = OFF, the default) */
    case MI_AFTERT: return aftert_on();                /* the same, bit 7 */
#endif
    case MI_CLICK: case MI_CLKLVL: case MI_COUNTIN: return rp_get(row - MI_CLICK);
    case MI_TUNE: return (uint32_t)((song.g[G_TUNE] >= TUNE_MIN && song.g[G_TUNE] <= TUNE_MAX ? song.g[G_TUNE] : TUNE_DEF) - TUNE_MIN);   /* (out of range: 440.0) */
    case MI_LEDS:
        while (i + 1u < LEDS_COUNT && LEDS_MENU[i] != settings_leds)
            i++;
        return i;
    }
    return 0;
}
static const char *menu_vname(uint32_t row, uint32_t v)
{
    const menu_flag_t *f = menu_flag(row);
    if (f)
        return f->name[v & 1u];
    switch (row) {
    case MI_COLOR: return UI_PALETTES[v % NPALETTES].name;
    case MI_HOLD: return HOLD_N[v & 3u];
    case MI_CLICK: return CLICK_N[v % 3u];
    case MI_CLKLVL: return CLKLVL_N[v % 3u];
    case MI_COUNTIN: return COUNTIN_N[v % 3u];
    case MI_SCROFF: return SCROFF_N[v % NELEM(SCROFF_N)];
    case MI_ANIM: return ANIM_N[v % 3u];
    case MI_HOMEPG: return HOMEPG_N[v % HP_COUNT];
#if DUELL_VELO
    case MI_KEYVEL: case MI_AFTERT: return v & 1u ? "ON" : "OFF";
#endif
    case MI_PERFLEAVE: return v & 1u ? "KEEP" : "FADE";   /* DUELL 0.2.14: leaving PERFORM / loading a sound: SPACE and KILL fade to neutral (default) / stay */
    case MI_LEDS: return LEDS_NAME[LEDS_MENU[v % LEDS_COUNT]];
    case MI_TUNE: {                                    /* A4 in Hz with one decimal ("440.0"; the menu adds "Hz") */
        static char b[8];
        fmt_fix(b, (int32_t)(v % TUNE_N) + TUNE_MIN, 1);
        return b;
    }
    }
    return "";
}
/* set a row (v below menu_n(row)) and do what the menu does on a change: the palette and the EQ at once;
 * ANIM, LEDS, HOLD, the flags are read where they are used, every frame;
 * USB SERIAL when the menu has closed (usb_serial_apply). Saved by the caller (menu_close, the editor). TUNE: the project's
 * value, as its page knob set it up to 1.1.5 (voice.c reads TUNE); kept with the project (the autosave), not by settings_save */
static void menu_put(uint32_t row, uint32_t v)
{
    const menu_flag_t *f = menu_flag(row);
    if (f) {
        if (f->bit & PREF_X)
            ui_prefs2 = (uint8_t)(v ? ui_prefs2 | f->bit >> 16 : ui_prefs2 & ~(f->bit >> 16));
        else if (f->bit & PREF_REC)
            ui_rec_prefs = (uint8_t)(v ? ui_rec_prefs | f->bit >> 8 : ui_rec_prefs & ~(f->bit >> 8));
        else
            ui_prefs = (uint8_t)(v ? ui_prefs | f->bit : ui_prefs & ~f->bit);
        return;
    }
    switch (row) {
    case MI_COLOR:
        settings.palette = v;
        palette_set(v);                                /* (the menu signature redraws) */
        ui.force = 1;
        break;
    case MI_HOLD: settings_hold = (uint8_t)v; break;
    case MI_SCROFF: scr_put(v); break;                 /* (read every frame: ui.c scr_frame) */
    case MI_ANIM:                                      /* (read where they are used, every frame) */
        ui_prefs = (uint8_t)(v == 1u ? ui_prefs | PREF_ANIM_OFF : ui_prefs & ~PREF_ANIM_OFF);
        ui_idle = (uint8_t)(v == 2u ? ui_idle | 1u : ui_idle & ~1u);
        break;                                         /* (1.5: up to 1.4.1 it fell through: ANIM set HOME too) */
    case MI_HOMEPG: ui_home_pg = (uint8_t)(v < HP_COUNT ? v : HP_MORPH); break;   /* (read where HOME opens: go_main) */
    case MI_LEDS: settings_leds = LEDS_MENU[v]; break;
#if DUELL_VELO
    case MI_KEYVEL:                                    /* (ON = the bit set; sense.c takes it at once, ui_input.c again every frame) */
        ui_idle = (uint8_t)(v ? ui_idle | KEYVEL_ON_BIT : ui_idle & ~KEYVEL_ON_BIT);
        sense_enable(v != 0u);
        break;
    case MI_AFTERT:                                    /* (its own bit, inverted: the bit set = OFF, so the default is ON; the keys' channel pressure) */
        ui_idle = (uint8_t)(v ? ui_idle & ~AFTERT_OFF_BIT : ui_idle | AFTERT_OFF_BIT);
        sense_at_enable(v != 0u);
        break;
#endif
    case MI_PERFLEAVE: ui_idle = (uint8_t)(v ? ui_idle | PERF_KEEP_BIT : ui_idle & ~PERF_KEEP_BIT); break;   /* (read where PERFORM is left: ui_perform.c) */
    case MI_CLICK: case MI_CLKLVL: case MI_COUNTIN: rp_put(row - MI_CLICK, v); break;   /* (at once: click.c, seq.c) */
    case MI_TUNE: song.g[G_TUNE] = (int16_t)((int32_t)v + TUNE_MIN); break;
    }
}
/* the menu's step, any of KNOB 1..4 or OCT+ (s > 0) / OCT- (s < 0): the next / previous value, stopping at the ends;
 * COLOR wraps round its palettes. On an ON / OFF row up is ON (MENU_FLAGS). TUNE: s steps of 0.1 Hz at a time (510 values; ui_menu.c
 * gives a fast turn KNOB ACCEL's multiple over that range, up to x8) */
static uint32_t menu_step(uint32_t row, int32_t s)
{
    const menu_flag_t *f = menu_flag(row);
    uint32_t v = menu_get(row), n = menu_n(row);
    if (row == MI_TUNE)
        return (uint32_t)clamp((int32_t)v + s, 0, (int32_t)n - 1);
    if (row == MI_ANIM)                                /* OFF ON IDLE, stopping at the ends */
        return ANIM_ORD[clamp((int32_t)ANIM_ORD[v % 3u] + (s > 0) - (s < 0), 0, 2)];
    if (f && str_eq(f->name[0], "ON"))                 /* (USB SERIAL, RESTORE LAST, CPU METER: ON is their value 0) */
        s = -s;
    if (s > 0)
        return v + 1u < n ? v + 1u : row == MI_COLOR ? 0u : v;
    if (s < 0)
        return v ? v - 1u : row == MI_COLOR ? n - 1u : 0u;
    return v;
}
