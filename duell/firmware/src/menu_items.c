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
/* DUELL 1.3 (owner 2026-11-13, menu-sections-proposal_2026-11-13 option A): seven SECTIONS (MTAB_*), the MENU opens on their list; PERFORM's
 * rows left CONTROL, MIDI got GLO > MIDI's three settings, SIDECHAIN (its four values, a sub-section) and MIDI LEARN (its screen), MODULES got GLO > MODULES' four.
 * The values and their storage are the same as before: only where they are shown and set changed */
/* DUELL 1.3 (owner 2026-11-15, after the Fable review, menu-consistency_2026-11-15.md sections 15 and 16): seven categories in this order, PLAYING RECORDING MORPH PERFORM MODULES
 * MIDI SYSTEM; MODE (with XFADE and SPLIT, LAYER only) and MORPH SET's SMART / ARP / PITCH came in from HOME's pages; DISPLAY, CONTROL and AUDIO are gone (their rows in PLAYING,
 * RECORDING and SYSTEM). The order of the rows inside each category follows the review. The values and their storage are unchanged */
enum { MI_MODE, MI_XFADE, MI_SPLIT, MI_MODERECALL,    /* PLAYING: MODE (was HOME's MODE page: duell_mode_req), XFADE and SPLIT (mix_xf, mix_split: LAYER only), MULTI LOAD, */
#if DUELL_VELO
       MI_KEYVEL,                                      /* (KEY VELOCITY OFF / ON: builds with KEY SENSE built in only, DUELL_VELO; every product build since 0.2.30) */
       MI_AFTERT,                                      /* (DUELL 1.0: AFTERTOUCH OFF / ON, its own line, the same builds) */
#endif
       MI_TUNE,                                        /* TUNE (the project's G_TUNE) */
       MI_CLICK, MI_CLKLVL, MI_COUNTIN, MI_CHADD, MI_PREVIEW,   /* RECORDING: CLICK, CLICK LEVEL, COUNT-IN (contiguous: rp_get), STEP KEYS, STEP PREVIEW */
       MI_RSMART, MI_RARP, MI_RPITCH,                  /* MORPH: MORPH SET's SMART, ARP, PITCH (morph.cfg MC_WILD inverted, MC_ARP, MC_PIT) */
       MI_PERFK1, MI_PERFK2, MI_PERFK3, MI_PERFK4,     /* PERFORM: PERFORM's four knobs, each any of the eight macros (or DEPTH; K1 has no row: it is DEPTH), */
       MI_QMAC1, MI_QMAC2, MI_QMAC3, MI_QMAC4,         /* QUICK MACRO's four (SEL held: ui_layer.c), */
       MI_PERFLEAVE,                                   /* LEAVING PERFORM */
       MI_ARP, MI_SEQ, MI_SENDS, MI_PFX,               /* MODULES (core.h MODS_*; ui.c mods_*) */
       MI_P1CH, MI_P2CH, MI_MIDIIN,                    /* MIDI: the parts' channels (ui_input.c midi_ch_*), MIDI IN (the project's G_ROUTE), */
       MI_MIDIPRE,                                     /* the MIDI PRESET (STANDARD, CIRCUIT TRACKS, MPC/FORCE: midi_control.c), */
       MI_MLEARN, MI_SCHAIN,                           /* MIDI LEARN (opens its screen, ui.menu 4) and SIDECHAIN (opens its four values, MTAB_SCHAIN; last: it hides with PART FX OFF) */
       MI_COLOR, MI_LEDS, MI_SCLLED, MI_ANIM, MI_SCROFF, MI_NOTESLEFT, MI_CPU,   /* SYSTEM, page 1: the look (was DISPLAY), */
       MI_ACCEL, MI_HOLD,                              /* the feel (was CONTROL), */
       MI_HOMEPG, MI_RESTORE, MI_PANEL,                /* page 2: where you land (HOME OPENS, RESTORE LAST), BUTTON MAP (no row), */
#if FELUCCA_CDC
       MI_SERIAL,                                      /* (USB SERIAL: developer builds only, FELUCCA_CDC: the product has no serial console, so no row that would do nothing) */
#endif
#if DUELL_TEST_MODE
       MI_TEST,                                        /* (developer builds only: DUELL_TEST_MODE; the release has no TEST row) */
#endif
       MI_GUIDE,                                       /* (DUELL 0.2.11: the first-start GUIDE again; no value, no editor id) */
       MI_ABOUT, MI_ERASE,                             /* ERASE DATA last */
       MI_SC1CH, MI_SC1NT, MI_SC2CH, MI_SC2NT,         /* MIDI > SIDECHAIN: the ducker's MIDI setup (ui.c sc_get / sc_put; was GLO > SIDECHAIN) */
       MI_COUNT };
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
#define MI_OPENS(row) ((row) == MI_SCHAIN || (row) == MI_MLEARN)   /* (DUELL 1.3: the rows that open a sub-section or a screen: OCT+ or a knob right, as the three above) */
static int menu_valued(uint32_t row) { return row < MI_COUNT && row != MI_PANEL && row != MI_ERASE && !MI_READONLY(row) && !MI_OPENS(row); }   /* (a row with a value) */
#define MI_PROJECT(row) ((row) == MI_TUNE || (row) == MI_MIDIIN || ((row) >= MI_MODE && (row) <= MI_SPLIT) || ((row) >= MI_RSMART && (row) <= MI_RPITCH))   /* the project's value (song.g), not a setting; (owner 2026-11-15) MODE, XFADE, SPLIT and MORPH's three: saved by their own paths (ui_input.c mode_settings, morph_settings) */
static const char *const MI_NAME[MI_COUNT] = {"MODE", "XFADE", "SPLIT", "MULTI LOAD",
#if DUELL_VELO
                                              "KEY VELOCITY", "AFTERTOUCH",
#endif
                                              "TUNE",
                                              "CLICK", "CLICK LEVEL", "COUNT-IN", "STEP KEYS", "STEP PREVIEW",
                                              "MORPH SMART", "MORPH ARP", "MORPH PITCH",   /* (the editor's keys; the unit says SMART, ARP, PITCH under MORPH: MI_LABEL) */
                                              "PERFORM K1", "PERFORM K2", "PERFORM K3", "PERFORM K4", "QUICK K1", "QUICK K2", "QUICK K3", "QUICK K4",   /* (DUELL 1.3: the editor's keys; the unit and the editor say them in full: MI_LABEL, labels.js MENU_LONG) */
                                              "PERF LEAVE",
                                              "ARP", "SEQ", "SENDS", "PART FX",
                                              "PART 1 CHANNEL", "PART 2 CHANNEL", "MIDI IN", "MIDI PRESET", "MIDI LEARN", "SIDECHAIN",
                                              "COLOR", "LEDS", "SCALE LEDS", "ANIMATION", "SCREEN OFF", "NOTES LEFT", "CPU METER",
                                              "KNOB ACCEL", "BUTTON HOLD",
                                              "HOME OPENS", "RESTORE LAST", "BUTTON MAP",
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
                                              "ABOUT", "ERASE DATA",
                                              "PART 1 CHANNEL", "PART 1 NOTE", "PART 2 CHANNEL", "PART 2 NOTE"};
/* DUELL 0.2.14: the device's label where it is not MI_NAME (0: MI_NAME). One '\n' at most, a designed break (nothing wraps by itself):
 * the line after it is drawn small (S, MID: a hint, or the rest of a long name) and the row takes two units (ui_menu.c) */
static const char *const MI_LABEL[MI_COUNT] = {
    [MI_LEDS] = "LIGHTS", [MI_SCLLED] = "SCALE LIGHTS", [MI_PERFLEAVE] = "LEAVING PERFORM",   /* (owner 2026-10-25; MI_NAME stays the editor's key, the editor says it in MENU_LONG) */
    [MI_ACCEL] = "KNOB ACCELERATION",                 /* (DUELL 1.1 label audit, owner: no cryptic ACCEL; MI_NAME stays the editor's key, the editor says it in MENU_LONG) */
    [MI_MODERECALL] = "MULTI LOAD\n", [MI_RESTORE] = "RESTORE LAST\n",   /* DUELL 1.3 (owner 2026-11-13): a second line that says what the chosen value does (ui_menu.c menu_help) */
    [MI_PERFK2] = "PERFORM KNOB 2", [MI_PERFK3] = "PERFORM KNOB 3", [MI_PERFK4] = "PERFORM KNOB 4",   /* DUELL 1.3 MACROS (one line: the approved drawing) */
    [MI_RSMART] = "SMART", [MI_RARP] = "ARP", [MI_RPITCH] = "PITCH",   /* (owner 2026-11-15: MORPH SET's three, under MORPH) */
    [MI_QMAC1] = "QUICK MACRO\nKNOB 1", [MI_QMAC2] = "QUICK MACRO\nKNOB 2", [MI_QMAC3] = "QUICK MACRO\nKNOB 3", [MI_QMAC4] = "QUICK MACRO\nKNOB 4",
#if FELUCCA_CDC
    [MI_SERIAL] = "USB SERIAL\nDEVELOPER CONSOLE"      /* (USB SERIAL: its hint) */
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
/* DUELL 1.3 (owner 2026-11-13): the tabs are the SECTIONS of the list the MENU opens on, in its order; MTAB_SCHAIN is MIDI's SIDECHAIN, a sub-section
 * (opened from its row, not in the list: MTAB_LIST). MPAGE_UNITS 8: the rows start under the title (no tab bar) */
enum { MTAB_PLAYING, MTAB_RECORDING, MTAB_MORPH, MTAB_PERFORM, MTAB_MODULES, MTAB_MIDI, MTAB_SYSTEM, MTAB_SCHAIN, MTAB_COUNT };   /* (owner 2026-11-15: this order) */
#define MTAB_LIST MTAB_SCHAIN                          /* (the sections of the list: 0 .. MTAB_LIST - 1) */
#define MPAGE_UNITS 8u
static const char *const MTAB_NAME[MTAB_COUNT] = {"PLAYING", "RECORDING", "MORPH", "PERFORM", "MODULES", "MIDI", "SYSTEM", "SIDECHAIN"};
static const uint8_t MI_TAB[MI_COUNT] = {
    MTAB_PLAYING, MTAB_PLAYING, MTAB_PLAYING, MTAB_PLAYING,                               /* MODE XFADE SPLIT MULTI LOAD */
#if DUELL_VELO
    MTAB_PLAYING, MTAB_PLAYING,                                                           /* KEY VELOCITY, AFTERTOUCH */
#endif
    MTAB_PLAYING,                                                                         /* TUNE */
    MTAB_RECORDING, MTAB_RECORDING, MTAB_RECORDING, MTAB_RECORDING, MTAB_RECORDING,       /* CLICK CLICK LEVEL COUNT-IN STEP KEYS STEP PREVIEW */
    MTAB_MORPH, MTAB_MORPH, MTAB_MORPH,                                                   /* SMART ARP PITCH */
    0xFFu, MTAB_PERFORM, MTAB_PERFORM, MTAB_PERFORM,                                      /* PERFORM KNOB 1 (no row: K1 is DEPTH, owner 2026-11-13), 2 .. 4 */
    MTAB_PERFORM, MTAB_PERFORM, MTAB_PERFORM, MTAB_PERFORM,                               /* QUICK MACRO KNOB 1 .. 4 */
    MTAB_PERFORM,                                                                         /* LEAVING PERFORM */
    MTAB_MODULES, MTAB_MODULES, MTAB_MODULES, MTAB_MODULES,                               /* ARP SEQ SENDS PART FX */
    MTAB_MIDI, MTAB_MIDI, MTAB_MIDI, MTAB_MIDI, MTAB_MIDI, MTAB_MIDI,                     /* PART 1 CHANNEL PART 2 CHANNEL MIDI IN MIDI PRESET MIDI LEARN SIDECHAIN */
    MTAB_SYSTEM, MTAB_SYSTEM, MTAB_SYSTEM, MTAB_SYSTEM, MTAB_SYSTEM, MTAB_SYSTEM, 0xFFu,   /* COLOR LEDS SCALE LEDS ANIMATION SCREEN OFF NOTES LEFT (CPU METER: no row, its bit and editor id 26 reserved) */
    MTAB_SYSTEM, MTAB_SYSTEM,                                                             /* KNOB ACCEL BUTTON HOLD */
    MTAB_SYSTEM, MTAB_SYSTEM, 0xFFu,                                                      /* HOME OPENS, RESTORE LAST, BUTTON MAP (no row since 1.3: OCT- and OCT+ held at power-on still open it, main.c) */
#if FELUCCA_CDC
    MTAB_SYSTEM,                                                                          /* USB SERIAL */
#endif
#if DUELL_TEST_MODE
    MTAB_SYSTEM,                                                            /* TEST (1.2: INFO) */
#endif
    MTAB_SYSTEM,                                                            /* GUIDE (DUELL 0.2.11) */
    MTAB_SYSTEM,                                                            /* ABOUT */
    MTAB_SYSTEM,                                                            /* ERASE DATA (DUELL 0.2.2: OCT+ opens its dialog, ui_menu.c) */
    MTAB_SCHAIN, MTAB_SCHAIN, MTAB_SCHAIN, MTAB_SCHAIN,                     /* SIDECHAIN's PART 1 CHANNEL, PART 1 NOTE, PART 2 CHANNEL, PART 2 NOTE */
};
/* a row that shows: one with a section, and SIDECHAIN's (its row and its values) only while PART FX is ON (MODULES: the ducker is skipped otherwise; the hide rule) */
static int menu_shown(uint32_t row)
{
    return row < MI_COUNT && MI_TAB[row] < MTAB_COUNT && !((row == MI_SCHAIN || MI_TAB[row] == MTAB_SCHAIN) && (mods_off & MODS_PFX)) &&
           !((row == MI_XFADE || row == MI_SPLIT) && duell_mode_req != DM_LAYER);   /* (XFADE and SPLIT are LAYER's only, as on the MODE page: hidden in DUEL and STEREO) */
}
/* TUNE (DUELL 0.2.22: MASTER TUNE like a Roland keyboard's): the project's G_TUNE, the frequency of A4 in 0.1 Hz, 415.3 .. 466.2 Hz (core.h TUNE_MIN .. TUNE_MAX); as a row
 * its values 0..TUNE_N - 1 (value - TUNE_MIN, 510 steps, 247 = 440.0 Hz). The editor gets it as a number in tenths (EDM_TENTHS: min..max, its unit "Hz") */
#define TUNE_N ((uint32_t)(TUNE_MAX - TUNE_MIN + 1))
typedef char mtab_fits_ui[sizeof ui.menu_row >= MTAB_COUNT ? 1 : -1];   /* (ui.c: the row last picked per section) */
static uint32_t mtab_first(uint32_t t)                 /* a tab's first row */
{
    uint32_t i = 0;
    while (i < MI_COUNT && MI_TAB[i] != t)
        i++;
    return i;
}
static uint32_t mtab_end(uint32_t t)                   /* .. and the row after its last (rows without a place may sit between: menu_shown) */
{
    uint32_t i = MI_COUNT;
    while (i && MI_TAB[i - 1u] != t)
        i--;
    return i;
}
/* DUELL 0.2.14: a row's units (1, or 2 for a two-line label) and its place: the page of its tab and the unit it starts at. Rows fill a
 * page in MI order until the next one does not fit; the rest go on the next page (a few loops over ~26 rows: no table to keep) */
static uint32_t menu_units(uint32_t row)
{
    const char *l = menu_label(row);
    if (!menu_shown(row))
        return 0;                                      /* (a row without a place: PERFORM KNOB 1, BUTTON MAP, SIDECHAIN with PART FX OFF) */
    while (*l && *l != '\n')
        l++;
    return *l ? 2u : 1u;
}
static uint32_t menu_place(uint32_t row, uint32_t *unit)   /* the page (0 ..) of row in its tab; *unit its first unit there */
{
    uint32_t i = mtab_first(MI_TAB[row % MI_COUNT]), pg = 0, u = 0, n;
    for (;; i++) {
        n = menu_units(i);
        if (n && u + n > MPAGE_UNITS) {
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
    return menu_place(mtab_end(t) - 1u, 0) + 1u;
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
                                                       * leaves the MODE page (MODE, XFADE, SPLIT) as it is; SETS MODE, it sets the ones saved in the multi (multi.c) */
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
static const char *const HOMEPG_N[HP_COUNT] = {"MORPH", "MACROS 1/2", "MACROS 2/2", "PERFORM"};   /* (owner 2026-11-15: HOME's round in its order, then PERFORM) */   /* (owner 2026-11-15: MODE left HOME for SETTINGS > PLAYING; a stored MODE reads MORPH) */   /* (0.2.25: MIXER left HOME for GLO; a stored MIXER reads MORPH. DUELL 1.3 (owner 2026-11-13): PERFORM, stored as HP_PERF_CODE: ui.c home_pg) */
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
#if DUELL_VELO
    if (row == MI_KEYVEL || row == MI_AFTERT)
        return SC_TYPE_N + 1u;                         /* OFF + 5 types */
#endif
    if (row == MI_MODE)
        return DM_N;
    if (row == MI_XFADE)
        return 101u;                                   /* 0 .. 100 % */
    if (row == MI_SPLIT)
        return 129u;                                   /* 0 OFF, else the note + 1 */
    return row == MI_COLOR ? NPALETTES : (row >= MI_CLICK && row <= MI_COUNTIN) ? 3u :
           row == MI_HOLD ? 4u : row == MI_LEDS ? LEDS_COUNT : row == MI_SCROFF ? NELEM(SCROFF_N) :
           row == MI_TUNE ? TUNE_N : row == MI_ANIM ? 3u : row == MI_HOMEPG ? (uint32_t)HP_COUNT :
           row >= MI_PERFK1 && row <= MI_QMAC4 ? (uint32_t)MAC_N : row == MI_MIDIPRE ? (uint32_t)MPRE_N :   /* (DUELL 1.3: PERFORM's: the eight and DEPTH) */
           row == MI_P1CH || row == MI_P2CH || row == MI_SC1CH || row == MI_SC2CH ? 16u : row == MI_SC1NT || row == MI_SC2NT ? 128u :   /* (a channel 1..16; a note: 0 ANY, then the notes) */
           row == MI_ARP || row == MI_SEQ ? 3u : 2u;      /* (ARP, SEQ: OFF, ON (FOLLOWS PATCH), KEEPS RUNNING: ui.c mods_run_get) */
}
static uint32_t menu_get(uint32_t row)
{
    const menu_flag_t *f = menu_flag(row);
    uint32_t i = 0;
    if (f)
        return ((ui_prefs | (uint32_t)ui_rec_prefs << 8 | (uint32_t)ui_prefs2 << 16) & f->bit) != 0;
    switch (row) {
    case MI_COLOR: return settings.palette % NPALETTES;
    case MI_MODE: return (uint32_t)DM_LAYER - (duell_mode_req < DM_N ? duell_mode_req : DM_SPLIT);   /* (the list LAYER, STEREO, DUEL, as the MODE page read it) */
    case MI_XFADE: return mix_xf <= 100u ? mix_xf : 100u;
    case MI_SPLIT: return mix_split <= 128u ? mix_split : 0u;
    case MI_RSMART: return !(morph.cfg & MC_WILD);
    case MI_RARP: return (morph.cfg & MC_ARP) != 0u;
    case MI_RPITCH: return (morph.cfg & MC_PIT) != 0u;
    case MI_HOLD: return settings_hold % 4u;
    case MI_SCROFF: return scr_get();
    case MI_ANIM: return ui_prefs & PREF_ANIM_OFF ? 1u : (ui_idle & 1u) ? 2u : 0u;
    case MI_HOMEPG: return home_pg();
    case MI_P1CH: case MI_P2CH: return midi_ch_get(row - MI_P1CH);
    case MI_MIDIIN: return song.g[G_ROUTE] == 1;       /* (0 PART CHANNEL, 1 ANY CHANNEL: the project's G_ROUTE, as stored before) */
    case MI_ARP: case MI_SEQ: return mods_run_get(row - MI_ARP);
    case MI_SENDS: case MI_PFX: return !((mods_get() >> (row - MI_ARP)) & 1u);   /* (1 ON: the MODULES bit clear) */
    case MI_SC1CH: case MI_SC1NT: case MI_SC2CH: case MI_SC2NT: return sc_get(row - MI_SC1CH);
    case MI_PERFLEAVE: return perf_keep();
    case MI_PERFK1: case MI_PERFK2: case MI_PERFK3: case MI_PERFK4: case MI_QMAC1: case MI_QMAC2: case MI_QMAC3: case MI_QMAC4:
        return mac_pick(row - MI_PERFK1);              /* DUELL 1.3 (ui.c) */
    case MI_MIDIPRE: return mac_preset_get();
    case MI_NOTESLEFT: return notes_show_on();         /* 1 = ON (the stored bit is OFF: set = OFF, clear = ON, the default) */
#if DUELL_VELO
    case MI_KEYVEL: return keyvel_on() ? 1u + keyvel_type() : 0u;   /* 0 OFF, 1..5 SOFTER .. HARDER (the ON bit: set = ON, clear = OFF, the default; the type in ui_sense_type) */
    case MI_AFTERT: return aftert_on() ? 1u + aftert_type() : 0u;   /* the same (ON = bit 7 clear) */
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
    case MI_MODE: return DM_NAME[DM_LAYER - (v < DM_N ? v : 0u)];
    case MI_XFADE: {
        static char b[8];
        fmt_int(b, (int32_t)v);
        str_cpy(b + str_len(b), " %", 3);
        return b;
    }
    case MI_SPLIT: {
        static char b[8];
        if (!v)
            return "OFF";
        note_name(b, v - 1u);
        return b;
    }
    case MI_RSMART: case MI_RARP: case MI_RPITCH: return v & 1u ? "ON" : "OFF";
    case MI_HOLD: return HOLD_N[v & 3u];
    case MI_CLICK: return CLICK_N[v % 3u];
    case MI_CLKLVL: return CLKLVL_N[v % 3u];
    case MI_COUNTIN: return COUNTIN_N[v % 3u];
    case MI_SCROFF: return SCROFF_N[v % NELEM(SCROFF_N)];
    case MI_ANIM: return ANIM_N[v % 3u];
    case MI_HOMEPG: return HOMEPG_N[v % HP_COUNT];
    case MI_MIDIIN: return v & 1u ? "ANY CHANNEL" : "PART CHANNEL";
    case MI_ARP: case MI_SEQ: return v == 0u ? "OFF" : v == 1u ? "ON" : "KEEPS RUNNING";   /* (the words GLO > MODULES showed) */
    case MI_SENDS: case MI_PFX: return v & 1u ? "ON" : "OFF";
    case MI_P1CH: case MI_P2CH: case MI_SC1CH: case MI_SC2CH: case MI_SC1NT: case MI_SC2NT: {   /* a channel 1..16; a note as the DUCK's (F_DKNOTE: ANY, C-1 ..) */
        static char b[PV_LEN];
        const char *u;
        if (row == MI_SC1NT || row == MI_SC2NT)
            param_format(&SC_NOTE_D, (int16_t)v, b, &u);
        else
            fmt_int(b, (int32_t)v + 1);
        return b;
    }
#if DUELL_VELO
    case MI_KEYVEL: case MI_AFTERT: {                  /* DUELL 1.1: OFF + 5 types, the same words on the unit and in the editor (list names are PV_LEN 32: spelled out) */
        static const char *const TYPE_N[6] = {"OFF", "SOFTER", "SOFT", "NORMAL", "HARD", "HARDER"};
        return TYPE_N[v % 6u];
    }
#endif
    case MI_NOTESLEFT: return v & 1u ? "ON" : "OFF";
    case MI_PERFLEAVE: return v & 1u ? "KEEP" : "FADE";
    case MI_PERFK1: case MI_PERFK2: case MI_PERFK3: case MI_PERFK4: case MI_QMAC1: case MI_QMAC2: case MI_QMAC3: case MI_QMAC4:
        return MAC_NAME[v <= MAC_DEPTH ? v : 0u];
    case MI_MIDIPRE: return MPRE_NAME[v < MPRE_N ? v : 0u];   /* DUELL 0.2.14: leaving PERFORM / loading a sound: SPACE and KILL fade to neutral (default) / stay */
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
    case MI_MODE:                                      /* (owner 2026-11-15: was HOME's MODE page, ui_input.c mode_knob; the audio takes it at its next block, seq.c mode_apply) */
        if ((uint32_t)DM_LAYER - (v < DM_N ? v : 0u) != duell_mode_req) {
            duell_mode_req = (uint8_t)(DM_LAYER - (v < DM_N ? v : 0u));
            ui.force = 1;                              /* (XFADE and SPLIT show or hide) */
        }
        break;
    case MI_XFADE: mix_xf = (uint8_t)(v <= 100u ? v : 100u); break;
    case MI_SPLIT: mix_split = (uint8_t)(v <= 128u ? v : 0u); break;
    case MI_RSMART: case MI_RARP: case MI_RPITCH:      /* (as MORPH SET's knobs did: ui_input.c morph_knob) */
        (void)morph_do(row == MI_RSMART ? MQ_SMART : row == MI_RARP ? MQ_ARP : MQ_PITCH, morph_mask(song.sel), v & 1u);   /* (morph.c: as MORPH SET's knobs did through ui_input.c morph_act; the row shows the value, no message) */
        break;
    case MI_SCROFF: scr_put(v); break;                 /* (read every frame: ui.c scr_frame) */
    case MI_ANIM:                                      /* (read where they are used, every frame) */
        ui_prefs = (uint8_t)(v == 1u ? ui_prefs | PREF_ANIM_OFF : ui_prefs & ~PREF_ANIM_OFF);
        ui_idle = (uint8_t)(v == 2u ? ui_idle | 1u : ui_idle & ~1u);
        break;                                         /* (1.5: up to 1.4.1 it fell through: ANIM set HOME too) */
    case MI_HOMEPG: ui_home_pg = (uint8_t)(v == HP_PERF ? HP_PERF_CODE : v == HP_MAC1 ? HP_MAC1_CODE : v == HP_MAC2 ? HP_MAC2_CODE : HP_MORPH); break;   /* (read where HOME opens: go_main) */
    case MI_P1CH: case MI_P2CH: midi_ch_put(row - MI_P1CH, v); break;   /* (seq.c midi_pch at once, the settings bytes saved ~2 s later) */
    case MI_MIDIIN: song.g[G_ROUTE] = (int16_t)(v & 1u); break;
    case MI_ARP: case MI_SEQ: mods_run_put(row - MI_ARP, v); break;   /* (as GLO > MODULES' knob did: ui.c) */
    case MI_SENDS: case MI_PFX: mods_put(1u << (row - MI_ARP), !v); break;
    case MI_SC1CH: case MI_SC1NT: case MI_SC2CH: case MI_SC2NT: sc_put(row - MI_SC1CH, v); break;   /* (fx.c sc_midi at once, saved ~2 s later) */
    case MI_LEDS: settings_leds = LEDS_MENU[v]; break;
#if DUELL_VELO
    case MI_KEYVEL:                                    /* (v 0 OFF, 1..5 a type; ON = the bit set; sense.c takes it at once, ui_input.c again every frame) */
        ui_idle = (uint8_t)(v ? ui_idle | KEYVEL_ON_BIT : ui_idle & ~KEYVEL_ON_BIT);
        if (v)
            ui_sense_type = (uint8_t)((ui_sense_type & ~7u) | (v <= SC_TYPE_N ? v : SC_TYPE_NORMAL + 1u));   /* (stored type + 1 = the menu value) */
        sense_enable(v != 0u);
        sense_vel_type_set(keyvel_type());
        break;
    case MI_AFTERT:                                    /* (its own bit, inverted: the bit set = OFF, so the default is ON; the keys' channel pressure) */
        ui_idle = (uint8_t)(v ? ui_idle & ~AFTERT_OFF_BIT : ui_idle | AFTERT_OFF_BIT);
        if (v)
            ui_sense_type = (uint8_t)((ui_sense_type & ~(7u << 3)) | ((v <= SC_TYPE_N ? v : SC_TYPE_NORMAL + 1u) << 3));
        sense_at_enable(v != 0u);
        sense_at_type_set(aftert_type());
        break;
#endif
    case MI_NOTESLEFT: ui_notes_off = (uint8_t)(v ? ui_notes_off & ~NOTES_OFF_BIT : ui_notes_off | NOTES_OFF_BIT); break;   /* (read where the header is drawn: ui_draw.c) */
    case MI_PERFLEAVE: ui_idle = (uint8_t)(v ? ui_idle | PERF_KEEP_BIT : ui_idle & ~PERF_KEEP_BIT); break;   /* (read where PERFORM is left: ui_perform.c) */
    case MI_PERFK1: case MI_PERFK2: case MI_PERFK3: case MI_PERFK4: case MI_QMAC1: case MI_QMAC2: case MI_QMAC3: case MI_QMAC4:
        mac_pick_put(row - MI_PERFK1, v);              /* DUELL 1.3 (read where the knobs act and the cards are drawn: ui_perform.c) */
        break;
    case MI_MIDIPRE: mac_preset_put(v); break;         /* (midi_control.c reads it: ui.c mac_settings) */
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
    if (row == MI_TUNE || row == MI_XFADE)
        return (uint32_t)clamp((int32_t)v + s, 0, (int32_t)n - 1);
    if (row == MI_SPLIT)                               /* (as the MODE page's SPLIT: from OFF the first turn starts at the middle key, F#4, stored 67) */
        return (uint32_t)clamp(v == 0u && s ? 67 + (s > 0 ? s - 1 : s + 1) : (int32_t)v + s, 0, (int32_t)n - 1);
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
