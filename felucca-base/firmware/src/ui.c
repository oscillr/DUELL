/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FELUCCA user interface.
 * Flat: SURF cards and panels on the palette's background, no rules, one type family (Inter Tight, three sizes), tracks
 * named by their numbers 1..4 on a cushion (icons.c trk_icon). Four columns <-> KNOB 1..4. Rendering is lazy:
 * every element remembers what it last drew and is redrawn only on change. */
static int project_save(uint32_t slot);
static void panel_setup(void);
static void project_load(uint32_t slot);
static int project_used(uint32_t slot);
static int up_used(uint32_t k);              /* a SAVED record in slot k: upreset.c (no record: the slot is its factory sound, sl_*) */
static int up_load(uint32_t k);
static uint32_t up_count(void);
static uint32_t up_engine(uint32_t k);
static void up_name(uint32_t k, char *b);
static void up_slot_label(char *b, uint32_t k);   /* "#7" */
static void up_ui(uint32_t op, uint32_t k);
static int ps_used(uint32_t k);                  /* DUELL: the pattern slots P1..P16 (patslot.c) */
static uint32_t ps_len(uint32_t k);
static int ps_save_trk(const track_t *t, uint32_t k, const char *name);
static void ps_name(uint32_t k, char *b);
static int ps_load_trk(track_t *t, uint32_t k);
static int ps_erase(uint32_t k);
static void ps_label(char *b, uint32_t k);
static void up_auto_name(char *b, uint32_t e, uint32_t k);   /* naming (ui_name.c) */
static void up_ui_named(uint32_t op, uint32_t k, const char *name, uint32_t cat);
static uint32_t up_cat(uint32_t k);
static int project_save_as(uint32_t slot, const char *name);
static int project_name(uint32_t slot, char *b);
static int project_info(uint32_t slot, char *b);
static int project_rename(uint32_t slot, const char *name);
static void project_cur_name(char *b);
static uint32_t project_cur_slot(void);           /* .. the multi the music came from (0..15; 0xFF none) */
static int project_factory(uint32_t slot);         /* .. M01..M10 not overwritten: shows its factory multi */
static int project_erase(uint32_t slot);           /* DUELL: SAVE > MULTI's ERASE (project.c multi.c) */
static int erase_data_run(void);                   /* DUELL 0.2.2: MENU > SYSTEM > ERASE DATA (erase_data.c) */
static void mu_label(char *b, uint32_t k);         /* .. "M01" .. "M16" */
/* The PATCH SLOTS (upreset.c, the slot layer): ONE list of UP_SLOTS (128), slot k = its saved record, else the factory sound at browse position k */
static uint32_t slot_of(const track_t *t);         /* the slot t's sound is in, UP_SLOTS = none */
static uint32_t slot_home(const track_t *t);       /* .. where SAVE offers to write it */
static uint32_t sl_engine(uint32_t k);             /* slot k's engine, name, category: its record's, else its factory sound's */
static void sl_name(uint32_t k, char *b);
static uint32_t sl_cat(uint32_t k);
static int sl_fac(uint32_t k, uint32_t *e, uint32_t *fk);   /* the factory sound slot k falls back to (1: it has one) */
static void sl_load(uint32_t k);                   /* slot k -> the selected part */
static uint32_t up_gen;                      /* bumped on every patch store change (redraws) */
static void patch_save_do(uint32_t both);    /* SAVE held, OCT+: the sound over its slot (upreset.c) */
static void patch_restore_ask(void);         /* PATCHES, K4 RESTORE (upreset.c) */
static void patch_restore_do(uint32_t k);
static void patch_restore_all_do(void);      /* SAVE > TOOLS: RESTORE ALL */
#include "favorites.c"
#include "category.c"                   /* sound categories (Discussion #90): PRESETS > LIST */
/* MENU's two-valued settings (ui_menu.c MENU_FLAGS), a bit each in a byte no engine uses (as ui_layer.c layer_seen):
 * saved with the settings; 0 in older ones = every setting's default (append-only: a new setting takes a new bit,
 * its default is 0) */
#define ui_prefs (favorites.factory[15][30])
/* DUELL 0.2.19 BASS MONO (fx.c bass_mono) is a sound setting of the music, the page MASTER's: song.bm (core.h G_BM), kept in the autosave, the project and the multis
 * (project.c byte 65, multi.c byte 5), not in the settings record. [15][29], the retired STYLE byte, is unused again (it held BASS MONO in an early 0.2.19 build; whatever it
 * holds is ignored). ui_input copies song.bm to fx.c bm_sel every frame */
                                               /* (1: retired, was MENU > FX LATCH ON; the FX hold layer is removed in DUELL 0.2.2: keep it clear, never reused) */
#define PREF_ANIM_OFF 2u                       /* MENU > ANIM OFF (#46): values snap (no rolling digits, no glide) */
#define PREF_ACCEL 4u                          /* MENU > KNOB ACCEL ON (#52): fast turns of wide values x2..x8 */
#define PREF_USB_FIXED 8u                      /* MENU > USB LEVEL FIXED: USB audio at the default MASTER level, MASTER after */
                                               /* (16: retired, was BPM LOCK; SELECT is no tempo any more: keep it clear) */
                                               /* (32: retired, was MENU > LARGE ON; DUELL 0.2.2 removed the view: the bit is ignored, never reused) */
#define PREF_SERIAL_OFF 64u                    /* MENU > USB SERIAL OFF (#67): no serial console, the device enumerates as
                                                * audio + MIDI only (usb.c usb_cdc_switch); clear in every older setting = ON */
#define PREF_RESTORE_OFF 128u                  /* MENU > RESTORE LAST OFF (1.2, Discussion #130): no autosave written, none
                                                * restored at power-on (project.c); clear in every older setting = ON */
/* MENU > AUDIO CLICK, CLICK LEVEL, COUNT-IN (1.1, Discussion #131; click.c, seq.c): two bits each in another byte no
 * engine uses, saved with the settings: bits 0-1 CLICK (OFF REC ON), 2-3 CLICK LEVEL (stored 0 MID, 1 LOW, 2 HIGH),
 * 4-5 COUNT-IN (OFF, 1 BAR, 2 BARS); 0 in older settings = OFF / MID / OFF, so nothing clicks or counts in that did
 * not; 3 (unknown) reads as 0 (settings_persist.c puts it back). Bit 6 (1.2): was MENU > SCALE LEDS ON (Discussion #127):
 * retired in 0.2.5, whatever it holds is ignored, never reused. Bit 7 (0.2.5): MENU > SCALE LEDS OFF (ui_input.c scale_leds); clear in every
 * older setting and a new unit = AUTO, so the scale shows by itself (an old ON or OFF alike) */
#define ui_rec_prefs (favorites.factory[15][28])
#define PREF_REC 0xFF00u                       /* (menu_items.c MENU_FLAGS: a bit of ui_rec_prefs, << 8) */
#define PREF_SCALE_LEDS_OFF (0x80u << 8)
/* 1.2: three more two-valued MENU settings, bits of another byte no engine uses, saved with the settings; 0 in older
 * settings = today's behaviour (append-only, as ui_prefs): bit 0 DISPLAY > SCOPE MIX (Discussion #165, fx.c scope_mix),
 * bit 1 CONTROL > STEP PREVIEW ON (Discussion #169, ui_input.c step_preview), bit 2 CONTROL > CHORD ENTRY ADD (#155,
 * ui_input.c seq_entry). 1.5: bit 3 SYSTEM > HELP ON (Discussion #156; removed in DUELL 0.2.2: the bit is ignored, never reused). Bit 4: CPU METER OFF (DUELL 0.2.9).
 * Bit 5: the first-start GUIDE seen (DUELL 0.2.11, ui_guide.c; no MENU row, no editor id). Bit 6: MODE RECALL ON (0.2.11, multi.c project_load;
 * 0.2.14: MENU > CONTROL > MULTI LOAD SETS MODE). Bit 7 (0.2.14): the 0.2.14 defaults applied (PREF_DEFAULTS, settings_persist.c settings_defaults).
 * DUELL 0.2.14: bit 0 (SCOPE MIX) is retired with its MENU row (the scope always draws OUT, after MASTER): ignored, kept as stored, never reused */
#define ui_prefs2 (favorites.factory[15][24])
#define PREF_X 0xFF0000u                       /* (menu_items.c MENU_FLAGS: a bit of ui_prefs2, << 16) */
#define PREF_PREVIEW (0x02u << 16)
#define PREF_CHORD_ADD (0x04u << 16)
#define PREF_MODE_RECALL (0x40u << 16)         /* DUELL 0.2.11: CONTROL > MODE RECALL ON (multi.c project_load): a multi load also sets the MODE saved in it; clear in every older setting = OFF (a load leaves the MODE as it is) */
#define PREF_CPU_OFF (0x10u << 16)             /* DUELL 0.2.9: DISPLAY > CPU METER OFF (ui_draw.c cpu bar under the battery); clear in every older setting = ON (bit 3 was HELP: retired) */
#define PREF_DEFAULTS (0x80u << 16)            /* DUELL 0.2.14: the one-time change to 0.2.14's defaults is done (settings_persist.c settings_defaults: KNOB ACCEL ON,
                                                * CLICK REC, USB SERIAL OFF where the old default was stored); clear in every older setting and on a new unit */
#define PREF_GUIDE_SEEN (0x20u << 16)          /* DUELL 0.2.11: the first-start GUIDE was left (ui_guide.c); clear on a new unit and in every older setting (never
                                                * written before 0.2.11): the guide shows once after the splash. Not a MENU_FLAGS row: MENU > SYSTEM > GUIDE only replays it */
#define GUIDE_N 6u                             /* .. its pages (ui_guide.c GUIDE_TXT) */
/* DUELL 0.1: MENU > CONTROL > HOME OPENS, the page the HOME button opens from another page, power-on and leaving the menu (go_main):
 * 0 MORPH (the default, every older setting), 1 MODE, 2 MIXER; [15][2], a byte no engine uses, saved with the settings;
 * an unknown code reads as MORPH. HOME tapped on HOME's own pages goes round them from MORPH as ever (home_tap).
 * DUELL 0.2.14 (owner): the OVERVIEW page is gone from HOME's round, and with it HOME OPENS' value 3 (OVERVIEW): a stored 3 reads as
 * MORPH (the unknown-code rule), kept as stored until HOME OPENS is changed. MENU > DISPLAY > OVERVIEW is gone too, and the OVERVIEW screen's
 * code (ui.home, its scope / TRACKS / LEVELS views, HOME's four cards); its settings byte [15][26] is kept as stored, read by nothing */
#define ui_home_pg (favorites.factory[15][2])
/* DUELL: the global SCALE, ROOT and OFF-SCALE KEYS (core.h): [15][3] bit 7 the format flag (0: an older blob or a new unit = CHROMATIC, SNAP: its
 * other bits, the former SCALE LINK and a global scale of 0.2.1 builds, are not read), bit 6 PLAY (0 SNAP), bits 0-3 the scale (0 = CHROMATIC);
 * [15][4] the global root 0..11 (0 = C) */
#define ui_scale_set (favorites.factory[15][3])
#define ui_root_set (favorites.factory[15][4])
enum { HP_MORPH, HP_MODE, HP_MIXER, HP_COUNT };   /* (0.2.14: 3 was OVERVIEW, retired: reads as MORPH) */
/* DUELL 0.1: the very first start. project.c persist_boot raises it when the flash holds no valid settings (a new unit, stock firmware's
 * data or garbage in the sector, a flash erase) and main.c drops it again when an autosave was restored; ui_input.c first_boot_poll then
 * makes the start state: MODE LAYER and factory multi M01 loaded (VIOLET is the theme of a settings record that is not valid:
 * panel.c settings_init). Valid settings (any DUELL, Felucca blob) never raise it, so the next start is an ordinary one */
static uint8_t first_boot_pending;
enum { RP_CLICK, RP_LEVEL, RP_COUNTIN };
static uint32_t rp_get(uint32_t f)                     /* a field as the MENU steps it: 0..2 */
{
    uint32_t v = (ui_rec_prefs >> (2u * f)) & 3u;
    v = v == 3u ? 0u : v;
    return f == RP_LEVEL ? (v == 0u ? 1u : v == 1u ? 0u : 2u) : v;   /* (LEVEL: LOW MID HIGH, MID stored as 0) */
}
static void rp_apply(void)                             /* the settings into click.c / seq.c (every frame: ui_input) */
{
    click_mode = (uint8_t)rp_get(RP_CLICK);
    click_lvl = (uint8_t)rp_get(RP_LEVEL);
    cin_bars = (uint8_t)rp_get(RP_COUNTIN);
}
static void rp_put(uint32_t f, uint32_t v)
{
    uint32_t st = f == RP_LEVEL ? (v == 1u ? 0u : v == 0u ? 1u : 2u) : v % 3u;
    ui_rec_prefs = (uint8_t)((ui_rec_prefs & ~(3u << (2u * f))) | st << (2u * f));
    rp_apply();
}
/* (favorites.factory[15][29]: the retired MENU > STYLE byte, FLAT or LINE; DUELL 0.2.2: FLAT only, the byte was ignored; unused) */
/* DUELL 4b: the MORPH's settings (SPEC 3.4 AMOUNT, 3.4a TARGET), kept with the settings (not the patch, not a project) in
 * two bytes no engine uses: [15][12] bit 7 TARGET BOTH (0 SELECTED; 0.2.9: reserved, kept as stored and never read:
 * ALGORITHM's BOTH is not saved), bits 0-6 part 1's AMOUNT, [15][13] bits 0-6 part 2's
 * AMOUNT (bit 7 free, 0); an AMOUNT as 1 + its percent, 0 = the default (MORPH_AMOUNT_DEF, 15 % since 2026-10-11, 25 % before: every older setting, and the default
 * state writes nothing). An unknown code (> 101) reads as the default. The ends, seeds and faders are not kept
 * (ui_input.c morph_settings) */
#define ui_morph_set (&favorites.factory[15][12])
/* DUELL 3.5: the MORPH SET page's SMART / ARP / PITCH (morph.c MC_WILD, MC_ARP, MC_PIT: bit 0 WILD = SMART off, bit 1 ARP ON,
 * bit 2 PITCH ON) in [15][8], a byte no engine uses: 0 = SMART, ARP OFF, PITCH OFF (the default: every older setting) */
#define ui_morph_cfg (favorites.factory[15][8])
/* DUELL step 5: the output MODE (core.h DM_*, SPEC 5b.1) in a byte no engine uses, kept with the settings: 0 SPLIT (the
 * default: every older setting), 1 STEREO, 2 LAYER; an unknown code reads as SPLIT and is kept as saved until the mode is
 * changed (ui_input.c mode_settings) */
#define ui_mode_set (favorites.factory[15][9])
/* the MODE page's KNOB 2..4 (core.h mix_bal, mix_xf, mix_split), three more bytes no engine uses, kept with the settings and in
 * each MULTI: [15][5] BALANCE (int8, -100..100), [15][6] XFADE (0..100 %), [15][7] SPLIT (0 = OFF, else the note + 1).
 * 0 in every older setting = BALANCE 0, XFADE 0, SPLIT OFF (nothing changes); an out-of-range byte reads as 0 / 0 / OFF
 * and is kept as saved until the value is changed (ui_input.c mode_settings) */
#define ui_mix_bal (favorites.factory[15][5])
#define ui_mix_xf (favorites.factory[15][6])
#define ui_mix_split (favorites.factory[15][7])
/* DUELL step 6: GLO > MIDI's parts' MIDI channels (seq.c midi_pch), kept with the settings in two more bytes no engine uses: [15][0] part 1,
 * [15][1] part 2; 0 (every older setting) = the default, channel 1 for part 1 and 2 for part 2, else the channel 1..16 (an unknown code
 * reads as the default and is kept as saved until changed: ui_input.c midi_ch_get). Not in the MULTIs (the device's, like the clock) */
#define ui_midi_p1 (favorites.factory[15][0])
#define ui_midi_p2 (favorites.factory[15][1])
static const char *const DM_NAME[DM_N] = {"DUEL", "STEREO", "LAYER"};
static void draw_gaps(uint32_t y, uint32_t h);        /* (ui_draw.c) */

static uint8_t sync_reload;                  /* engine / preset / project / user preset loaded: editor RELOAD push */

#define ACC T_THEME                /* values, curves */
#define VAL(c) ((c) == ui.hot_col && ui.hot_t ? T_ACCENT : T_THEME)   /* the knob just turned: accent */
#define RATIO(d, v) ((d)->max > (d)->min ? ((int32_t)(v) - (d)->min) * 1000 / ((d)->max - (d)->min) : -1)
/* layout: header 0..24, four cards 28..72 (57 px at x 3 + 59 c),
 * the panel 76..198 (a SURF area; the graphs live in it), footer 202..240; BG between them */
#define Y_HEAD 0
#define H_HEAD 24
#define Y_LABEL 28                    /* the cards */
#define Y_SEP_END 72
#define CARD_W 57
#define CARD_H 44
#define CARD_X(c) (3 + 59 * (int32_t)(c))
#define Y_GRAPH 76                    /* the panel */
#define H_GRAPH 122
#define Y_FOOT 202
#define H_FOOT 38

static struct {
    uint8_t page;                /* index into PAGES */
    uint8_t fam_last[FAM_COUNT]; /* last page used per family */
    uint8_t bank;                /* SEQ: 16-step bank (follows the cursor) */
    uint8_t cursor;              /* SEQ: step being edited (STEP page KNOB 1 moves it) */
    uint8_t entry_open;          /* SEQ: keys held since the first press of this entry */
    uint8_t plk_src;             /* parameter locks (ui_input.c lock_*): the sound page whose KNOB 1..4 a step held
                                  * locks, PAGES index + 1; 0 = the engine's four (ENGINES[].knob) */
    uint8_t hot_col, hot_t;      /* column whose knob was just turned (drawn white) */
    uint8_t menu;                /* 0 off, 1 list, 2 about + credits, 3 info (1.2) (HOME held) */
    uint8_t guide;               /* DUELL 0.2.11: the GUIDE's page shown (1..GUIDE_N), over everything but UPDATE MODE; 0 = none (ui_guide.c) */
    uint8_t guide_menu;          /* .. opened from MENU > SYSTEM > GUIDE (the bit untouched, back to the menu at its end) */
    uint8_t menu_sel;            /* MENU: the row (menu_items.c MI_*; its tab MI_TAB), kept while the device runs */
    uint8_t menu_row[5];         /* MENU: the row last picked in each tab, from its first (ALGORITHM comes back to it) */
    uint16_t menu_scroll;        /* continuous ABOUT + CREDITS position, pixels */
    uint32_t menu_sig, home_t0;  /* HOME press time (btn_hold) */
    uint8_t force;               /* full redraw pending */
    uint8_t msg_t;               /* transient message frames */
    uint8_t bpm_t;               /* frames after GLO + F4 TAP set the tempo (ui_input.c; the header shows no tempo any more) */
    uint8_t act;                 /* action pages: the column whose action OCT+ does, + 1; 0 = none (act_col) */
    uint32_t rec_t0;             /* REC press time: a tap arms (btn_hold, no hold: held, REC opens its layer) */
    uint32_t save_t0;           /* SAVE press time (btn_hold: held = UNDO) */
    uint32_t fx_t0;             /* FX press time (btn_hold: held 0.7 s = PERFORM on / off, ui_perform.c perf_input) */
    uint8_t perf;               /* DUELL 0.2.5: PERFORM is on (its page shown over ui.page: cur_page) */
    uint32_t perf_seen;         /* .. the PERFORM keys seen held last pass (a new press: its message) */
    uint8_t perf_rolls[2];      /* .. ROLL A / B presses done (perf_roll_n's count) */
    uint8_t confirm;             /* the OCT- / OCT+ dialog: CF_*, 0 = none */
    uint8_t confirm_trk;         /* the track it clears, the slot it overwrites */
    uint8_t svs;                 /* SAVE held (CF_SAVE_OVER): the slot the sound is written to (K1 picks it in the dialog) */
    uint8_t ppick;               /* SAVE > PATTERNS: the pattern picked (pat_count list index) */
    uint16_t ev_row;             /* SEQ > AUTOMATION (ui_events.c ev_*): the row selected (0 PLAY / CLEAR, 1 QUANTIZE, the
                                  * last + ADD; 1.2: up to EV_ROWS, past 255) */
    uint8_t ev_step, ev_id;      /* .. + ADD: the step and the parameter (or EV_CHANCE, EV_RATCH, EV_NUDGE, EV_VEL) it
                                  * adds on */
    uint16_t ev_keep;            /* .. the CHANCE / RATCH / NUDGE row being edited (its EVC code, 0 none): listed at its default */
    uint8_t ev_vel;              /* .. 1.5: the VEL rows shown (+ ADD VEL's SHOW / HIDE; off when the page is entered) */
    uint8_t uboot;               /* main.c: seconds left before UPDATE MODE (OCT- + OCT+ held), 0 = none */
    uint32_t ly_t0;              /* the layer button's press time | 1, LY_* bits (ui_layer.c layer_gesture) */
    uint8_t ly;                  /* the layer whose button is down (LAYER_*), 0 = none */
    uint8_t layer;               /* the layer whose map is shown (LAYER_*), 0 = none */
    uint8_t lock;                /* #83: the layer locked open by a double tap (LAYER_*), 0 = none (ui_layer.c) */
    uint16_t pg_down;            /* page buttons down (panel ids) that act when let go */
    uint32_t layer_sig;          /* drawn-state cache of the map */
    char msg[24];
    char msg2[24];               /* a second message, shown when the first is over */
    uint32_t enc_t[NE];
    /* drawn-state cache */
    char col[4][32];
    uint32_t graph_sig, head_sig, foot_sig, frame;
    /* rolling digits (ui_draw.c roll_*): the four card values and the header BPM */
    struct {
        char from[7];            /* the value rolling out; "" = idle */
        uint8_t t0;              /* (ui.frame) of the value's last change */
        int8_t dir;              /* +1: it went up (the old digit leaves upward), -1: down */
        uint8_t sig;             /* a card: what its value is of (label, unit, track, engine, palette) */
    } roll[5];
    struct {                     /* DUELL 4b, the twist gestures (ui_input.c morph_gesture): 0 MORPH ROLL, 1 UNDO, 2 GLO MUTE 1, 3 GLO MUTE 2 */
        int8_t acc[4];           /* detents turned in this gesture, signed (a reversal starts anew) */
        uint8_t fired[4];        /* the gesture fired: its further detents do nothing until the knob rests */
        uint32_t t[4];           /* fm1_ms of its last detent */
    } mg;
    uint8_t tap_n;               /* TEMPO's TAP (ui_input.c tempo_tap): the taps counted (at most 4), their fm1_ms times (a rest of 2 s starts anew) */
    uint32_t tap_t[4];
    uint8_t mset_seen[3], mset_ok, mset_dirty;   /* 4b: the MORPH settings bytes as last synced (morph_settings) */
    uint32_t mset_t;             /* .. fm1_ms of the last change not saved yet */
    uint8_t mix_seen[3], mix_ok; /* .. and the three of KNOB 2..4 (BALANCE, XFADE, SPLIT) */
    uint8_t play_go; uint32_t play_t0, play_last;  /* PLAY STOP: its press time | 1 while down and not fired; the last press (double press: ui_input.c play_hold) */
    uint8_t mode_seen, mode_ok;  /* DUELL step 5: the MODE's settings byte as last synced (mode_settings; its save: mset_*) */
    struct {                     /* MIDI LEARN (midi_learn.c; here, not loose globals: the audio ISR's data keeps its
                                  * layout and its cost, tests/target_budget.py) */
        uint8_t on;              /* MIDI LEARN is on */
        uint8_t pick, trk, id;   /* a parameter picked (1): its track and P_* id */
        volatile uint8_t io[2];  /* midi_control.c ml_arm, ml_heard */
    } ml;
} ui;

enum { CF_NONE, CF_CLEAR_SEQ, CF_CLEAR_TRK, CF_OVR_PROJ, CF_OVR_USER, CF_LOAD_PAT,
       CF_INIT_SOUND, CF_CLEAR_MOTION, CF_RESTORE, CF_RESTORE_ALL, CF_ERASE_PSLOT, CF_OVR_PSLOT,
       CF_SAVE_OVER, CF_ERASE_MULTI, CF_ERASE_ALL, CF_HOLD_PSLOT, CF_HOLD_MULTI };   /* ui.confirm: TOOLS' clears; SAVE over a used slot (CF_OVR_PROJ: a MULTI); a
                          * pattern over the user's steps; PATCHES' RESTORE (CF_RESTORE: a slot's saved sound goes, its factory sound comes back), SAVE > TOOLS' RESTORE ALL (CF_RESTORE_ALL); PATTERNS: a pattern slot's ERASE, SAVE over a used
                          * one; CF_ERASE_ALL: MENU > SYSTEM > ERASE DATA (opened in the menu, drawn over it; erase_data.c); SAVE held: the sound over a patch slot (CF_SAVE_OVER, confirm_trk the part, 2 both; ui.svs the slot, K1 picks it); MULTI's ERASE;
                          * SAVE held on PATTERNS / MULTIS: the slot picked over itself under its own name (CF_HOLD_PSLOT / CF_HOLD_MULTI) */

/* MENU > DISPLAY > SCREEN OFF (1.1.5): after a while with no input on the panel the screen goes dark (the backlight
 * stays on: lcd.c lcd_power)
 * (a picture held for hours can stay faintly on the panel); the sound, the sequencer, MIDI and USB go on, the LEDs
 * stay as they are. NEVER (the default since 1.1.5.1), 5, 15, 30, 60 minutes, kept in a byte no engine uses, saved
 * with the settings (SCR_CODE). 1.1.5 stored value ^ 3, 0 (its default and every older setting) being 30 MIN; 1.1.5.1
 * reads that 0 as NEVER (the new default: the panel went dark for good on 1.1.5, see lcd.c lcd_power) and keeps the
 * other codes, 30 MIN now 4. Unknown codes read as NEVER.
 * Input is the panel's only: a button or key held or pressed, a knob turned (not MIDI, the editor or MASTER, an
 * analogue pot). The press, key or turn that wakes it is swallowed: no note (seq.c kb_asleep), no button, no
 * value, until everything is let go and the knobs rest for SCR_EAT_MS. A dialog, the count-in and the UPDATE MODE
 * countdown keep it on (and wake it); the crash screen, UPDATE and UBOOT wake it at once (lcd_wake_now).
 * Off: lcd_power(0) (DISPOFF, SLPIN) and nothing is drawn. Waking, a frame at a time (never a wait in
 * the loop): 120 ms after the SLPIN, SLPOUT (lcd_power(1)); 120 ms after that one frame redraws everything
 * (ui.force), then DISPON (lcd_power(2)) */
#define ui_scr (favorites.factory[15][27])
/* MENU > ANIM IDLE (1.2, Discussion #135; menu_items.c, ui_input.c idle_leds): bit 0, the LEDs' idle animation, in a
 * byte no engine uses, kept with the settings; 0 in every older setting = off. The other bits: 0 (room for a delay) */
#define ui_idle (favorites.factory[15][10])
/* DUELL 0.2.14 GLO > MODULES (core.h MODS_*): bits 1..4 of the same byte, ARP SEQ SENDS PART FX, a SET bit = that module OFF. Every older
 * record has them clear (settings_persist.c settings_defaults clears bits 1..7 of an older record once: nothing wrote them before), so a
 * new unit, ERASE DATA and any older setting read all four ON. A device setting: no patch, multi or project holds it. Bits 5..7 stay free.
 * mods_apply copies them to the audio side every frame (ui_input; an import or an editor restore takes effect there too); a change made
 * on the page is saved ~2 s later (morph_settings' timer, ui.mset_dirty) */
#define MODS_SHIFT 1u
/* DUELL 0.2.14 MENU > CONTROL > PERF LEAVE (FADE / KEEP): bit 5 of the same byte, set = KEEP. FADE (the default, a clear bit: a new unit, ERASE DATA and every
 * older record, settings_defaults clears bits 1..7 once): leaving PERFORM, and a sound / multi load, glide PERFORM's live SPACE and KILL back to neutral
 * (ui_perform.c perf_fade_*); KEEP: they stay as they are. A device setting, never in a patch or multi */
#define PERF_KEEP_BIT 0x20u
static uint32_t perf_keep(void) { return (ui_idle & PERF_KEEP_BIT) != 0u; }
static uint32_t mods_get(void) { return ((uint32_t)ui_idle >> MODS_SHIFT) & MODS_ALL; }
static void mods_apply(void) { mods_off = (uint8_t)mods_get(); }
static void mods_put(uint32_t bit, uint32_t off)       /* bit: MODS_*; off 1 / 0 */
{
    uint32_t m = off ? mods_get() | bit : mods_get() & ~bit;
    if (m == mods_get())
        return;
    ui_idle = (uint8_t)((ui_idle & ~(MODS_ALL << MODS_SHIFT)) | m << MODS_SHIFT);
    mods_apply();
    ui.mset_dirty = 1;
    ui.mset_t = fm1_ms;
    ui.force = 1;
}
/* the page's cards, K1..K4 = bits 0..3 (MODS_ARP SEQ SENDS PFX): names, icons. A module OFF hides its cards (ui_draw.c card_hidden_vp); MODS_FULL
 * is the header's line for the PLAY button, which still runs the clock */
static const char *const MODS_NAME[4] = {"ARP", "SEQ", "SENDS", "PART FX"};
static const char *const MODS_FULL[4] = {"MODULES: ARP OFF", "MODULES: SEQ OFF", "MODULES: SENDS OFF", "MODULES: PART FX OFF"};
static uint32_t mods_idx(uint32_t bit) { return bit == MODS_ARP ? 0u : bit == MODS_SEQ ? 1u : bit == MODS_SENDS ? 2u : 3u; }
/* the module a parameter belongs to (MODS_*; 0: none): glob a song.g id, else a part's. SENDS: the three sends and the CHORUS, DELAY 1/2,
 * 2/2 and REVERB pages' globals; PART FX: DIST, INSERT, EQ, DUCK (its SRC / NOTE on GLO > SIDECHAIN too); ARP: the arp's nine; SEQ: the
 * steps' DIV and SWING (they only time the playback; LEN and GATE also serve the editing and STEP PREVIEW) */
static uint32_t mods_of(uint32_t glob, uint32_t id)
{
    if (glob) {
        switch (id) {
        case G_CRATE: case G_CDEPTH: case G_DTIME: case G_DFDBK: case G_DCOLOR: case G_DMIX: case G_PINGSPREAD:
        case G_DMRATE: case G_DMDEP: case G_DMODE: case G_RTYPE: case G_RSIZE: case G_RDAMP: case G_RLOCUT: case G_SHPITCH: case G_SHMOD:
            return MODS_SENDS;
        default:
            return 0;
        }
    }
    switch (id) {
    case P_CHOR: case P_DLY: case P_REV:
        return MODS_SENDS;
    case P_DIST: case P_ITYPE: case P_IA: case P_IB: case P_IC: case P_IMIX:
    case P_EQLF: case P_EQLG: case P_EQMF: case P_EQMQ: case P_EQMG: case P_EQHF: case P_EQHG:
    case P_DKSRC: case P_DKRATE: case P_DKTHR: case P_DKNOTE: case P_DKDEP: case P_DKATK: case P_DKRLS: case P_DKCRV:
        return MODS_PFX;
    case P_AMODE: case P_ARATE: case P_AOCT: case P_AGATE: case P_ASWING: case P_APROB: case P_AHOLD: case P_AORDER: case P_ATGT:
        return MODS_ARP;
    case P_SDIV: case P_SSWING:
        return MODS_SEQ;
    default:
        return 0;
    }
}
/* the module that hides a card now (MODS_*; 0: a live card): t the part (0: id is a song.g id). The arp's cards: every one with ARP OFF, or
 * with SEQ OFF on a part whose MODE is SEQUENCER (its arp plays the steps); HOLD only while it holds ARP (no chord to keep: no latch; OFF
 * and ALL act as ever) */
static uint32_t mods_fade(const track_t *t, uint32_t id)
{
    uint32_t b = mods_of(!t, id);
    if (b == MODS_ARP) {
        if (id == P_AHOLD && t->p[P_AHOLD] != 1)
            return 0;
        if (mods_off & MODS_ARP)
            return MODS_ARP;
        return (mods_off & MODS_SEQ) && t->p[P_AMODE] == AM_SEQ_ID ? MODS_SEQ : 0u;
    }
    return mods_off & b;
}
/* .. of a card's value pointer (a part's p[] or song.g[]): the module that hides it, or 0 */
static uint32_t mods_fade_vp(const int16_t *vp)
{
    uint32_t k;
    if (!vp)
        return 0;
    if (vp >= song.g && vp < song.g + G_COUNT)
        return mods_fade(0, (uint32_t)(vp - song.g));
    for (k = 0; k < NLIVE; k++)
        if (vp >= trk[k].p && vp < trk[k].p + P_COUNT)
            return mods_fade(&trk[k], (uint32_t)(vp - trk[k].p));
    return 0;
}
#define SCR_DEF 0u                                     /* NEVER (1.1.5.1; 1.1.5: 30 MIN) */
static const uint8_t SCR_CODE[5] = {0, 2, 1, 4, 7};    /* the stored byte of NEVER 5 15 30 60 MIN (1.1.5: 3 2 1 0 7) */
static const uint8_t SCR_DEC[8] = {0, 2, 1, 0, 3, 0, 0, 4};   /* .. and back (3: 1.1.5's NEVER; 5, 6 unknown) */
#define SCR_EAT_MS 300u
static const uint8_t SCR_MIN[5] = {0, 5, 15, 30, 60};  /* NEVER, minutes */
enum { SCR_ON, SCR_OFF, SCR_WAKE, SCR_SLPOUT, SCR_SHOW };
static struct {
    uint8_t st;                  /* SCR_*: SCR_WAKE asked, SCR_SLPOUT sent, SCR_SHOW the frame that redraws */
    uint8_t eat;                 /* the wake gesture: panel input swallowed (ui_input) */
    uint32_t idle;               /* fm1_ms of the last panel input (or blocker) */
    uint32_t t;                  /* fm1_ms of the last panel command (SLPIN, SLPOUT) */
    uint32_t eat_t;              /* fm1_ms of the last swallowed knob detent */
} scrn;
static uint32_t scr_get(void)                          /* the MENU's value: 0 NEVER .. 4 60 MIN */
{
    return ui_scr < NELEM(SCR_DEC) ? SCR_DEC[ui_scr] : SCR_DEF;
}
static void scr_put(uint32_t v)                        /* (the time counts from the change: the editor's too) */
{
    ui_scr = SCR_CODE[v < NELEM(SCR_MIN) ? v : SCR_DEF];
    scrn.idle = fm1_ms;
}
static void scr_wake(void)
{
    if (scrn.st == SCR_OFF)
        scrn.st = SCR_WAKE;
    scrn.eat = 1;
}
static void scr_wake_now(void)                         /* main.c: the crash screen, UPDATE, UBOOT */
{
    if (scrn.st != SCR_ON)
        lcd_wake_now();
    scrn.st = SCR_ON;
    scrn.idle = fm1_ms;
}
/* ui_input, every pass: 1 = the pass is swallowed (the screen dark or waking, or the wake gesture going on) */
static int scr_input(uint32_t pressed, uint32_t notes)
{
    uint32_t held = fm1_in.buttons | fm1_in.notes, k;
    int32_t moved = 0;
    if (pressed | notes | held | panel_moved)
        scrn.idle = fm1_ms;
    panel_moved = 0;
    if (scrn.st == SCR_ON && !scrn.eat)
        return 0;
    for (k = 0; k < NE; k++)                           /* (every knob's turns go) */
        moved |= panel_enc(k);
    panel_moved = 0;
    if (moved)
        scrn.eat_t = scrn.idle = fm1_ms;
    if (pressed | notes | held | (uint32_t)moved)
        scr_wake();
    if (scrn.st != SCR_ON || held || (int32_t)(fm1_ms - scrn.eat_t) < (int32_t)SCR_EAT_MS)
        return 1;
    scrn.eat = 0;                                       /* all let go, the knobs at rest: input again */
    kb_asleep = 0;
    return 0;
}
/* ui_draw, every frame: 1 = draw it */
static int scr_frame(void)
{
    uint32_t lim = SCR_MIN[scr_get()] * 60000u;
    if (ui.confirm || ui.uboot || seq_counting()) {    /* (never dark under these) */
        scrn.idle = fm1_ms;
        if (scrn.st == SCR_OFF)
            scr_wake();
    }
    switch (scrn.st) {
    case SCR_ON:
        if (!lim || (int32_t)(fm1_ms - scrn.idle) < (int32_t)lim)   /* (signed: robust to a wrap) */
            return 1;
        lcd_power(0);
        scrn.st = SCR_OFF;
        scrn.t = fm1_ms;
        scrn.eat = 1;
        kb_asleep = 1;
        break;
    case SCR_WAKE:
        if (fm1_ms - scrn.t >= 120u) {
            lcd_power(1);
            scrn.st = SCR_SLPOUT;
            scrn.t = fm1_ms;
        }
        break;
    case SCR_SLPOUT:
        if (fm1_ms - scrn.t >= 120u) {
            scrn.st = SCR_SHOW;
            ui.force = 1;
            return 1;
        }
        break;
    case SCR_SHOW:
        return 1;
    }
    ui.msg_t = 0;                                      /* (dark: what would have shown is over; a clean page back) */
    ui.msg2[0] = 0;
    ui.hot_t = ui.bpm_t = 0;
    return 0;
}
static void scr_shown(void)                            /* after the frame: the redrawn page on */
{
    if (scrn.st != SCR_SHOW)
        return;
    lcd_power(2);
    scrn.st = SCR_ON;
    scrn.idle = fm1_ms;
}

static const page_t *page_over;   /* a quick layer's own four knobs (ui_layer.c), while it edits or draws them */
static const page_t *cur_page(void) { return page_over ? page_over : ui.perf ? &PERF_PAGE : &PAGES[ui.page]; }
/* PERFORM (ui_perform.c) ends: every way off its page (another page, HOME, the menu's HOME) comes here; FILTER back to its
 * neutral place (SPACE and DEPTH stay) */
/* DUELL 0.2.14 PERF LEAVE (MENU > CONTROL; ui.c perf_keep): FADE (default) glides PERFORM's two live values, SPACE (perf_space) and KILL (perf_filt),
 * to 0 when PERFORM is left and when a sound or multi is loaded: one step (1 of 200) every 4 ms, so at most 0.4 s from the ends, in a smooth
 * line (SPACE's own glide in fx.c and KILL's ramps in perform.c follow it: no click). SPACE is also the MIXER's third card: it is faded only
 * if PERFORM moved it (perf_sp_live: turned while PERFORM was on; a multi load always), so a SPACE set on the MIXER alone stays when PERFORM
 * is merely opened and left. At 0 both stages are idle again: the output is bit for bit the patch's own. KEEP: nothing happens. DEPTH
 * (it sounds only with a key held) and MORPH (the sound's own fader) are no live values of this kind and are left as they are */
static uint8_t perf_fading, perf_sp_live;
static uint32_t perf_ft;
static void perf_fade_begin(uint32_t all)
{
    if (perf_keep())
        return;
    if (all)
        perf_sp_live = 1;
    if (perf_filt || (perf_sp_live && perf_space)) {
        if (!perf_fading)
            perf_ft = fm1_ms;
        perf_fading = 1;
    } else {
        perf_sp_live = 0;
    }
}
static int8_t perf_toward0(int8_t v, int32_t n) { return (int8_t)(v > 0 ? (v > n ? v - n : 0) : (v < -n ? v + n : 0)); }
static void perf_fade_step(void)
{
    int32_t n;
    if (!perf_fading)
        return;
    if (perf_keep()) {                                  /* (switched to KEEP meanwhile: stays where it is) */
        perf_fading = 0;
        return;
    }
    n = (int32_t)((fm1_ms - perf_ft) / 4u);
    if (!n)
        return;
    perf_ft += (uint32_t)n * 4u;
    if (n > 200)
        n = 200;
    perf_filt = perf_toward0(perf_filt, n);
    if (perf_sp_live)
        perf_space = perf_toward0(perf_space, n);
    if (!perf_filt && (!perf_sp_live || !perf_space)) {
        perf_fading = 0;
        perf_sp_live = 0;
    }
}

static void perf_off(void)
{
    if (!ui.perf)
        return;
    ui.perf = 0;
    perf_fade_begin(0);                                /* (FADE: SPACE, if PERFORM moved it, and KILL glide to neutral; KEEP: they stay) */
    ui.force = 1;
}
static void perf_cards(void);                         /* (ui_perform.c: the PERFORM page's parts, drawn by ui_draw.c and ui_graph.c) */
static void graph_perf(void);
static void space_turn(uint32_t role, int32_t s);      /* (ui_perform.c: SPACE, PERFORM's K2 and the MIXER's K3) */
static void space_card(uint32_t col);
static uint32_t perf_sig(void);
static int perf_foot(char *l, char *r);
static uint32_t perf_key_leds(uint32_t *mid);
static void perf_input(uint32_t pressed, uint32_t now, uint32_t lay);   /* (ui_perform.c: ui_input's) */
static void perf_knob(uint32_t slot, int32_t s);
static void perf_presets(int32_t s);
static void presets_turn(int32_t s);


/* the quick layers (ui_layer.c): a button held, the keys and KNOB 1..4 are its shortcuts, its map over the page */
enum { LAYER_NONE, LAYER_RES, LAYER_GLO, LAYER_SCL, LAYER_EDIT, LAYER_SEQ, LAYER_REC,
       LAYER_LFO, LAYER_ENV, LAYER_ARP,               /* DUELL 0.2.9: the QUICK EDIT layers of LFO, ENV, ARP (the keys stay notes) */
       LAYER_N };
static void draw_layer(void);
static int glo_pg(void);                              /* (ui_layer.c: GLO's main page shown) */
static uint32_t midi_ch_get(uint32_t k);              /* (ui_input.c: part k's MIDI channel, 0..15) */
static const char *layer_head(void);
static int layer_locked(void);
static void layer_foot_title(char *nm, char *ct);   /* the footer's page name and number for a layer (ui_layer.c) */
static uint32_t layer_leds(uint32_t *br);
static uint32_t layer_btn(void);

/* FILTER the multimode filter's (every engine with ufilt), VCF ANALOG's own, TRIO FILTER, NOISE FILT their own; OPERATOR, OP ENV, OPERATOR 2 an FM6 track's (its patch's
 * operators, ui_fm6op.c). (DUELL: DIGITAL's OP ENV pages and DRUM's LANES are gone, their ids hold DUELL's parameters; 0.2.9: SLICE's
 * SLICES page is gone with the engine) */
static int page_visible(uint32_t i)
{
    if (PAGES[i].scope == SC_GLOBAL && PAGES[i].id[0] == G_SHPITCH)   /* SHIMMER (0.2.19): SHIM's PITCH and MOD, shown only while the reverb TYPE is SHIM (the page cycle skips it) */
        return song.g[G_RTYPE] == 3;
    if (PAGES[i].id[0] == P_FTYPE)                    /* VCF (1.5, #104): ANALOG's own filter */
        return ENGINES[TSEL->eng_req % NENGINES] == &ENG_ANALOG;
    if (PAGES[i].id[0] == P_FREEA && PAGES[i].id[3] == P_FREED)   /* MULTI FILTER (DUELL 0.2.2): the multimode filter's, on every engine that has it (ufilt) */
        return ENGINES[TSEL->eng_req % NENGINES]->ufilt;
    if (PAGES[i].scope == SC_TRACK && PAGES[i].id[0] == P_E4)   /* TRIO FILTER: TRIO's own */
        return ENGINES[TSEL->eng_req % NENGINES] == &ENG_TRIO;
    if (PAGES[i].scope == SC_TRACK && PAGES[i].id[0] == P_E2)   /* NOISE FILT: NOISE's own */
        return ENGINES[TSEL->eng_req % NENGINES] == &ENG_NOISE;
    if (PAGES[i].graph >= GR_FMOP && PAGES[i].graph <= GR_FMENV)
        return TSEL->eng_req % NENGINES == ENGI_FM6;
    return 1;
}

static uint32_t page_first(uint32_t fam)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == fam)
            return i;
    return 0;
}

/* transient message in the top bar, right of the transport and the BPM: a + b */
static void ui_say(const char *a, const char *b)
{
    uint32_t n;
    str_cpy(ui.msg, a, sizeof ui.msg);
    n = str_len(ui.msg);
    str_cpy(ui.msg + n, b, sizeof ui.msg - n);
    ui.msg_t = 40;
    ui.msg2[0] = 0;
}

static void ui_message(const char *s) { ui_say(s, ""); }
/* a message led by an icon: its first byte (ui_draw.c draw_head draws the icon, then the words) */
#define MSG_NOFILE "\x01"                      /* the no-file icon (ICON_X_NOFILE) */
/* a part on a retired engine (DUELL 0.2.9: a stored SAMPLE / GRAIN / SLICE sound, silent; ui_input.c gone_notice): NO ENGINE (fits the
 * header's 130 px after the icon, as the NO SAMPLE it replaces) */
#define MSG_NO_ENGINE MSG_NOFILE "NO ENGINE"

/* Main loop only, with interrupts enabled. PLAY may be consumed between reads;
 * take one coherent snapshot before a flash operation. */
static int transport_busy(void)
{
    int busy;
    fm1_irq_off();
    busy = song.playing || transport_req == 1u || seq_counting();   /* (a count-in: as playing) */
    fm1_irq_on();
    return busy;
}


/* parameter locks: a page whose KNOB 1..4 can lock (a track's sound: one of its parameters can be motion; not MOD's
 * slots), the parameter KNOB k locks (ui.plk_src's page, HOME's four), 0xFF = none */
static int lock_page_ok(uint32_t i)
{
    uint32_t k;
    if (i >= NPAGES || (PAGES[i].scope != SC_TRACK && PAGES[i].scope != SC_ENGINE) || PAGES[i].graph == GR_MOD)
        return 0;
    for (k = 0; k < 4u; k++)
        if (PAGES[i].id[k] != 0xFFu && motion_param(pg_id(&PAGES[i], k)))
            return 1;
    return 0;
}
static uint32_t lock_id(uint32_t k)
{
    uint32_t id = ui.plk_src && lock_page_ok(ui.plk_src - 1u) ? pg_id(&PAGES[ui.plk_src - 1u], k)
                                                               : ENGINES[TSEL->eng_req % NENGINES]->knob[k & 3u];
    return id < P_COUNT && motion_param(id) ? id : 0xFFu;
}

static uint8_t pl_pend_on;                       /* PATCHES: a cursor place waits for its load (K1 accelerated, pl_step below) */
static void ev_enter(void);
static void page_entered(void)
{
    const page_t *pg;
    perf_off();                                  /* (a page entered: PERFORM is over) */
    pg = cur_page();
    if (pg->graph == GR_EVENTS)      /* AUTOMATION: + ADD on the SEQ cursor's step */
        ev_enter();
    if (lock_page_ok(ui.page))
        ui.plk_src = (uint8_t)(ui.page + 1u);
    song.seq_mode = pg->fam == FAM_SEQ;
    ui.entry_open = 0;
    ui.hot_t = 0;                                /* clear the previous page's emphasis */
    ui.act = 0;
    pl_pend_on = 0;
    ui.force = 1;
}

static int step_on(const step_t *st) { return st->time == ST_NOTE && st->n; }

static void step_clear(step_t *st)
{
    st->n = 0;
    st->time = ST_REST;
    st->flags = 0;
    st->vel = 0;
    st->probability = 0;
}

/* LEN (1.5, Discussions #178, #173): a NOTE step's length on the piano roll is the step and the TIE steps after it
 * (inside LEN, no wrap); SEQ > STEP's KNOB 3 on a note sets it (ui_input.c step_edit). Longer: the steps after it
 * become clean TIEs (a REST or an empty step; it stops at the next step holding notes, a REST's kept ones too,
 * which stays, and at LEN);
 * shorter: its last TIEs become RESTs (step_clear). The steps' locks stay. Returns the length it got */
static uint32_t note_len(const track_t *t, uint32_t i)
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), n = 1;
    while (i + n < len && t->step[i + n].time == ST_TIE)
        n++;
    return n;
}
static uint32_t note_set_len(track_t *t, uint32_t i, uint32_t want)
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), n = note_len(t, i);
    for (; n > want && n > 1u; n--)
        step_clear(&t->step[i + n - 1u]);
    for (; n < want && i + n < len && !t->step[i + n].n; n++) {
        step_clear(&t->step[i + n]);
        t->step[i + n].time = ST_TIE;
    }
    return n;
}

/* SEQ cursor: wraps inside the pattern length, the bank follows, a step entry ends */
static void cursor_set(int32_t c)
{
    int32_t len = TSEL->p[P_SLEN] > 0 ? TSEL->p[P_SLEN] : 1;
    ui.cursor = (uint8_t)((c % len + len) % len);
    ui.bank = (uint8_t)(ui.cursor / 16u);
    ui.entry_open = 0;
}

static void cursor_fix(void)                           /* LEN got shorter: onto the last step */
{
    if (ui.cursor >= (uint32_t)TSEL->p[P_SLEN])
        cursor_set(TSEL->p[P_SLEN] - 1);
}

static void note_name(char *b, uint32_t n)
{
    str_cpy(b, N_NOTE[n % 12u], 4);
    fmt_int(b + str_len(b), (int32_t)(n / 12u) - 1);
}

/* a page button tapped (1.2, Discussion #153: SEQ STEP <-> AUTOMATION, GLO: CLOCK (DUELL: no SONG page, no song mode),
 * SAVE USER -> PRESETS -> PATTERNS -> PROJECT -> TOOLS (1.5, #173; DUELL: PATCHES PATTERNS MULTIS TOOLS, SPEC 3.6)
 * to 1.4.1 the last); HOME: home_tap). From another
 * family the family's page shown last (SAVE: USER); one that is not the family's any more (a page that moved, as GLOBAL
 * and SYSTEM in 1.2), or not shown for this track, gives the family's first */
static uint32_t page_next(int32_t dir)              /* the family's next (dir > 0) / previous visible page, wrapping */
{
    uint32_t n, i = ui.page, fam = cur_page()->fam;
    for (n = 0; n < NPAGES; n++) {
        i = dir > 0 ? (i + 1u) % NPAGES : (i + NPAGES - 1u) % NPAGES;
        if (PAGES[i].fam == fam && page_visible(i)) break;
    }
    return i;
}
static void open_family(uint32_t fam)
{
    if (cur_page()->fam == fam) {          /* same button again: next page */
        ui.page = (uint8_t)page_next(1);
    } else if (fam == FAM_SAVE) {
        /* DUELL 0.2.2: SAVE opens the page that fits: PATTERNS from the sequencer (SEQ's pages), else by the output MODE:
         * LAYER the MULTIS, SPLIT and STEREO the PATCHES; TOOLS never. SELECT moves between the four */
        uint32_t i, want = cur_page()->fam == FAM_SEQ ? GR_PATS : duell_mode_req == DM_LAYER ? GR_SLOTS : GR_BROWSE;
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].graph == want) break;
        ui.page = (uint8_t)i;
    } else {
        uint32_t i = ui.fam_last[fam];
        ui.page = (uint8_t)(i && i < NPAGES && PAGES[i].fam == fam && page_visible(i) ? i : page_first(fam));
    }
    ui.fam_last[fam] = ui.page;
    page_entered();
}

/* DUELL 3.5: the main page. HOME's round is MORPH, MORPH SET, MODE, MIXER (FAM_TRK's pages, in PAGES order; 0.2.14: no OVERVIEW); power-on, a tap of HOME from another page and leaving the menu open the page
 * MENU > CONTROL > HOME OPENS says (ui_home_pg; MORPH unless changed) */
static void go_main_to(uint32_t want)
{
    uint32_t i;
    ui.page = (uint8_t)page_first(FAM_TRK);
    for (i = 0; (want == HP_MODE || want == HP_MIXER) && i < NPAGES; i++)
        if (PAGES[i].fam == FAM_TRK && page_visible(i) && (want == HP_MODE ? PAGES[i].scope == SC_MODE : PAGES[i].graph == GR_TRK)) {
            ui.page = (uint8_t)i;
            break;
        }
    ui.fam_last[FAM_TRK] = ui.page;
    ui.plk_src = 0;
    ui.entry_open = 0;
    ui.hot_t = 0;
    song.seq_mode = 0;
    page_entered();
}
static void go_main(void) { go_main_to(ui_home_pg < HP_COUNT ? ui_home_pg : HP_MORPH); }

/* The SELECT knob (SPEC 1.2): the previous / next sub-page of the screen shown, wrapping: the family's pages, the order
 * its button's taps go round (open_family). HOME's round: its pages, round (SELECT right from MIXER goes to MORPH, left from MORPH to MIXER) */
static void page_select(int32_t dir)
{
    uint32_t i;
    if (ui.perf)                                      /* PERFORM: one page, no other to turn to (a stray turn never ends it) */
        return;
    if (cur_page()->fam == FAM_TRK) {
        uint8_t pg[8];
        uint32_t m = 0, at = 0;
        for (i = 0; i < NPAGES && m < sizeof pg; i++)
            if (PAGES[i].fam == FAM_TRK && page_visible(i))
                pg[m++] = (uint8_t)i;
        if (!m)
            return;
        for (i = 0, at = 0u; i < m; i++)
            if (pg[i] == ui.page)
                at = i;
        at = (at + (dir > 0 ? 1u : m - 1u)) % m;
        ui.page = pg[at];
        ui.fam_last[FAM_TRK] = ui.page;
        page_entered();
        return;
    }
    i = page_next(dir);
    if (i != ui.page) {
        ui.page = (uint8_t)i;
        ui.fam_last[PAGES[i].fam] = ui.page;
        page_entered();
    }
}

/* HOME tapped: from another page MORPH; then MORPH SET, MODE, MIXER, and MORPH again (0.2.14: no OVERVIEW after MIXER). HOME's
 * pages are FAM_TRK's, in PAGES order, always from the first (none remembered). Held: the menu (ui_input) */
static void home_tap(void)
{
    uint32_t i = ui.page + 1u;
    if (cur_page()->fam != FAM_TRK) {
        go_main();
    } else if (i < NPAGES && PAGES[i].fam == FAM_TRK) {
        open_family(FAM_TRK);                         /* (on HOME's page again: the next one) */
    } else {
        go_main_to(HP_MORPH);                         /* (the last page, MIXER: round to MORPH) */
    }
}

/* ------------------------------------------------------- track setup --- */
static int seq_is_empty(const track_t *t)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++)
        if (t->step[i].n)
            return 0;
    return 1;
}

/* One-step UNDO of a load. A sound load (a factory or user preset, an engine jump, TOOLS INIT, the
 * editor's PRESET / G_ENGSEL / UP_LOAD) changes the sound only; a pattern load (SAVE > PATTERNS) changes
 * the steps and the pattern parameters only. Each first copies the track as it was; SAVE held 0.7 s
 * swaps back what the loads changed (held again: the loads again), so steps recorded after a sound
 * load, or a sound edited after a pattern load, stay as they are. One copy for all tracks: the last
 * load wins. Loads in a row on one track with nothing changed in between (the PRESETS knob through the
 * list, one pattern after the other, an editor audition) keep the copy from before the first, so the
 * undo goes back past the whole browse. Not snapshotted: power-on, projects.
 * 1.1.5: a lock or automation edit by hand (a step held and a knob turned, a step's locks cleared, SEQ > AUTOMATION)
 * takes the copy too (UNDO_MOT: SAVE held puts the track's motion back, the sound and the steps stay; motion_undo_take).
 * A sound load keeps the track's motion on the parameters every engine has (load_end, motion.c motion_sound_loaded);
 * a pattern load drops it all (the steps it was on are gone) */
enum { UNDO_SOUND = 1, UNDO_PAT = 2, UNDO_MOT = 4 };
static struct {
    uint8_t trk;                 /* track + 1, 0 = nothing to undo */
    uint8_t keep;                /* the track is as the last load left it (undo.after): a next load keeps the copy */
    uint8_t what;                /* UNDO_SOUND | UNDO_PAT: what the loads since the copy changed (undo_swap) */
    uint8_t eng, preset, user, patn;
    uint8_t fm6_slot;            /* the track's FM6 patch and its SLOT (eng_fm6.c): an edited or a project's */
    uint8_t fm6[FP_SIZE + 1u];   /* patch is the track's own, not a factory one */
    int16_t p[P_COUNT];
    step_t step[NSTEP];
    motion_store_t motion_backup; /* one track only, swaps with the shared event pool on undo */
    uint32_t after;              /* track_sig right after the last load */
    uint32_t pat;                /* pat_sig[] of the copy */
    uint32_t from_h;             /* pat_from_h[], pat_from[] of the copy (the header's slot letter) */
    uint8_t from;
    uint32_t t_ms;               /* time of the last load (the editor's SETs after it belong to it) */
} undo __attribute__((section(".pool")));   /* (main loop only: off the audio code's .bss) */
static uint8_t undo_depth;       /* loads nest (an engine jump loads its first preset): the outer one counts;
                                  * felucca_init / project_load raise it to take no copy at all */
static uint32_t pat_sig[NTRK];   /* steps_sig of the pattern the last pattern load put into each track: such
                                  * steps, untouched, are replaced by the next pattern without asking */
static uint8_t pat_last[NTRK];   /* that pattern's list index + 1, 0 = none */
/* the header's slot mark (ui_draw.c head_slot): the MULTI each track's working pattern came from (the last MULTI load or
 * save: 1..16 = M01..M16, shown as its number; 0 none: power-on, a PATTERNS pattern, the editor's restore) and pat_from_sig() of
 * it then: an edit since shows the letter with a mark */
static uint8_t pat_from[NTRK];
static uint32_t pat_from_h[NTRK];
static uint8_t load_from;        /* a sound load (the outer one): the engine before it (load_end) */
static uint8_t load_mo;          /* .. the track's motion paused while the sound changes: bit 0 set, bit 1 it was ON */

static uint32_t fnv(uint32_t h, const void *p, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h;
}
static uint32_t steps_sig(const track_t *t) { return fnv(2166136261u, t->step, sizeof t->step); }
static uint32_t track_sig(const track_t *t)      /* the sound (an FM6 track's patch too), the steps */
{
    uint8_t id[3] = {t->eng_req, t->preset, t->user};
    uint32_t h = fnv(fnv(steps_sig(t), t->p, sizeof t->p), id, 3), k = trk_index(t);
    h = fnv(h, fm6_patch[k], sizeof fm6_patch[k]); /* (a patch the editor sent between two loads) */
    for (uint32_t j = 0; j < motion.count; j++)
        if ((motion.event[j].place >> 6) == k) h = fnv(h, &motion.event[j], sizeof motion.event[j]);
    return h ^ ((motion.on >> k) & 1u);
}
static uint32_t motion_sig(const track_t *t)     /* the track's motion only (the sound moves while it plays) */
{
    uint32_t h = 2166136261u, k = trk_index(t);
    for (uint32_t j = 0; j < motion.count; j++)
        if ((motion.event[j].place >> 6) == k) h = fnv(h, &motion.event[j], sizeof motion.event[j]);
    return h ^ ((motion.on >> k) & 1u);
}
/* the track's pattern as a PROJECT slot holds it: the steps, LEN DIV SWING GATE, QUANTIZE, the automation */
static uint32_t pat_from_sig(const track_t *t)
{
    int16_t q = t->p[P_SQNT];
    uint32_t h = fnv(fnv(steps_sig(t), &t->p[P_SLEN], 4u * sizeof t->p[0]), &q, sizeof q);
    return h ^ motion_sig(t);
}
static void pat_from_set(uint32_t slot)             /* every track's pattern is slot's now (1..16), 0 none */
{
    for (uint32_t k = 0; k < NTRK; k++) {
        pat_from[k] = (uint8_t)slot;
        pat_from_h[k] = pat_from_sig(&trk[k]);
    }
}

/* load_begin, the copy taken: a pattern load drops the track's motion (its steps go); a sound load pauses it (PLAY
 * OFF while the sound changes under it) and load_end keeps what still means the same (motion_sound_loaded) */
static void load_motion_hold(track_t *t, uint32_t what)
{
    if (what & UNDO_PAT) {
        motion_reset(t);
        load_mo = 0;
        return;
    }
    load_from = t->eng_req;
    load_mo = (uint8_t)(1u | (motion_enabled(t) ? 2u : 0u));
    motion_set_enabled(t, 0);
}

static void load_begin(track_t *t, uint32_t what)
{
    uint32_t i = trk_index(t);
    if (what & UNDO_SOUND) {
        pl_pend_on = 0;                           /* (and no cursor place waits for a load) */
    }
    if (what & UNDO_SOUND)
        perf_fade_begin(0);                       /* (0.2.14 PERF LEAVE FADE: a sound load takes PERFORM's SPACE and KILL back to neutral) */
    if (undo_depth++)
        return;
    motion_restore(t);
    if (undo.keep && undo.trk == i + 1u && !(undo.what & UNDO_MOT) && track_sig(t) == undo.after) {
        undo.what |= (uint8_t)what;               /* browsing on: the copy from before the first load stays */
        load_motion_hold(t, what);
        return;
    }
    undo.trk = (uint8_t)(i + 1u);
    undo.what = (uint8_t)what;
    undo.eng = t->eng_req;
    undo.preset = t->preset;
    undo.user = t->user;
    memcpy(undo.p, t->p, sizeof undo.p);
    memcpy(undo.step, t->step, sizeof undo.step);
    memcpy(undo.fm6, fm6_patch[i], FP_SIZE);
    undo.fm6_slot = fm6_slot[i];
    undo.pat = pat_sig[i];
    undo.from = pat_from[i];
    undo.from_h = pat_from_h[i];
    undo.patn = pat_last[i];
    motion_snapshot_track(t, &undo.motion_backup);
    load_motion_hold(t, what);
}

static void load_end(track_t *t)
{
    if (--undo_depth)
        return;
    if (load_mo & 1u) {                           /* a sound load: the motion on the common parameters stays */
        morph_rebase(t);                          /* DUELL: the MORPH's ends belong to the sound before (morph.c) */
        (void)motion_sound_loaded(t, load_from, t->eng_req);
        if ((load_mo & 2u) && motion_count(t))
            motion_set_enabled(t, 1);
        load_mo = 0;
    }
    motion_rebase(t);
    undo.after = track_sig(t);
    undo.keep = 1;
    undo.t_ms = fm1_ms;
}

/* the editor's SET right after a load on the selected track (an audition: G_ENGSEL, then the patch's
 * values): part of that load, the copy from before it stays */
static void load_extend(track_t *t)
{
    if (undo.keep && undo.trk == trk_index(t) + 1u && !(undo.what & UNDO_MOT) && fm1_ms - undo.t_ms < 1500u) {
        undo.after = track_sig(t);
        undo.t_ms = fm1_ms;
    }
}

/* 1.1.5: a lock / automation edit by hand on track t is about to happen: the undo copy of the track as it is (as
 * ui_tools.c tl_undo_take, UNDO_MOT: the swap puts back its motion only). A knob turned on (the same tag, not 0:
 * the same knob of the same gesture), on the same track, nothing else changed and less than 1.5 s apart, shares the
 * copy from before its first detent; tag 0 (an add, a delete, a clear, a kind) always takes its own.
 * motion_undo_done after each */
static uint32_t undo_mot_tag;
/* what an edit's copy compares (undo.after): the motion; with the steps (UNDO_PAT: step_undo_take) the steps too */
static uint32_t edit_sig(const track_t *t) { return undo.what & UNDO_PAT ? motion_sig(t) ^ steps_sig(t) : motion_sig(t); }
static void edit_undo_take(track_t *t, uint32_t tag, uint32_t what)
{
    uint32_t i = trk_index(t);
    if (tag && tag == undo_mot_tag && undo.keep && undo.trk == i + 1u && undo.what == what &&
        edit_sig(t) == undo.after && fm1_ms - undo.t_ms < 1500u)
        return;
    undo_mot_tag = tag;
    undo.trk = (uint8_t)(i + 1u);
    undo.what = (uint8_t)what;
    undo.eng = t->eng_req;
    undo.preset = t->preset;
    undo.user = t->user;
    memcpy(undo.p, t->p, sizeof undo.p);
    memcpy(undo.step, t->step, sizeof undo.step);
    memcpy(undo.fm6, fm6_patch[i], FP_SIZE);
    undo.fm6_slot = fm6_slot[i];
    undo.pat = pat_sig[i];
    undo.from = pat_from[i];
    undo.from_h = pat_from_h[i];
    undo.patn = pat_last[i];
    motion_snapshot_track(t, &undo.motion_backup);
    undo.keep = 0;
}
static void motion_undo_take(track_t *t, uint32_t tag) { edit_undo_take(t, tag, UNDO_MOT); }
/* 1.2: a step's CHANCE / RATCH edited on SEQ > AUTOMATION (ui_events.c): the swap puts back the steps (and LEN DIV
 * SWING GATE, as copied) and the motion, the sound stays. motion_undo_done after each, as for a lock */
static void step_undo_take(track_t *t, uint32_t tag) { edit_undo_take(t, tag, UNDO_MOT | UNDO_PAT); }
static void motion_undo_done(track_t *t)
{
    undo.after = edit_sig(t);
    undo.keep = 1;
    undo.t_ms = fm1_ms;
}

static int param_kept(uint32_t i);
/* The global SCALE / ROOT / OFF-SCALE KEYS (core.h) <-> their settings bytes (ui_scale_set, ui_root_set). Every frame (ui_input.c), and at once
 * after a menu change: a RAM value changed since the last sync (the SCL pages, the quick layer) is written to the bytes and saved ~2 s later
 * (morph_settings' timer); a byte changed from outside (power-on, a settings import) is read. An unknown value reads as the default */
static uint32_t scale_pack(void)
{
    return 0x80u | ((uint32_t)scale_g & 15u) | (offscale_g == OS_PLAY ? 0x40u : 0u) | (uint32_t)(root_g % 12) << 8;
}
static uint32_t scale_last = 0xFFFFFFFFu;
static void scale_sync(void)
{
    uint32_t ram = scale_pack(), by = (ui_scale_set & 0x80u) ? (ui_scale_set & 0xCFu) | (uint32_t)ui_root_set << 8 : 0x80u;   /* (unflagged: the defaults) */
    if (ram != scale_last && scale_last != 0xFFFFFFFFu) {           /* changed here */
        scale_gen++;
        ui_scale_set = (uint8_t)(ram & 0xFFu);
        ui_root_set = (uint8_t)(ram >> 8);
        ui.mset_dirty = 1;
        ui.mset_t = fm1_ms;
    } else if (by != ram || scale_last == 0xFFFFFFFFu) {            /* changed outside, or the first look */
        scale_g = (int16_t)(by & 15u);
        offscale_g = (uint8_t)((by & 0x40u) ? OS_PLAY : OS_SNAP);
        root_g = (int16_t)((by >> 8) % 12u);
        scale_gen++;
    }
    scale_last = scale_pack();
}
/* param_kept for the sound loaded into track t: a load never touches the global scale (core.h), and no patch holds one */
static int param_kept_on(const track_t *t, uint32_t i) { (void)t; return param_kept(i); }
static void ds_seq_back(track_t *t);              /* (upreset.c: a patch's lent sequencer pattern back) */

/* What the loads changed (undo.what) and the copy change places, so again = the loads again. DUELL save flow
 * (2026-10-11): SAVE held is SAVE OVER now (ui_input.c patch_save_ask); this stays the EDIT layer's OCT- (ui_layer.c). The sound: engine, preset and its parameters (not param_kept); the pattern: the steps and LEN
 * DIV SWING GATE. The mix (LEVEL PAN MUTE) stays: no load changes it */
static void undo_swap(void)
{
    track_t *t;
    uint32_t i;
    char b[4] = {'T', 0, 0, 0};
    if (!undo.trk) {
        ui_message("NOTHING TO UNDO");
        return;
    }
    t = &trk[(undo.trk - 1u) % NTRK];
    motion_restore(t);
    motion_store_t current_motion;
    motion_snapshot_track(t, &current_motion);
    if (motion_replace_track(t, &undo.motion_backup) != 0) {
        ui_message("AUTOMATION FULL");
        return;
    }
    undo.motion_backup = current_motion;
    fm1_irq_off();                                /* the audio ISR must not see half a sound */
    if (undo.what & UNDO_SOUND) {
        uint8_t e = t->eng_req, pr = t->preset, u = t->user;
        panic_req |= (uint8_t)(1u << trk_index(t));
        t->eng_req = undo.eng;
        t->preset = undo.preset;
        t->user = undo.user;
        undo.eng = e;
        undo.preset = pr;
        undo.user = u;
        for (i = 0; i < P_COUNT; i++)
            if (!param_kept_on(t, i)) {
                int16_t v = t->p[i];
                t->p[i] = undo.p[i];
                undo.p[i] = v;
            }
    }
    if (undo.what & UNDO_PAT) {
        uint32_t ps = pat_sig[trk_index(t)];
        uint8_t pn = pat_last[trk_index(t)];
        pat_sig[trk_index(t)] = undo.pat;
        undo.pat = ps;
        pat_last[trk_index(t)] = undo.patn;
        undo.patn = pn;
        pn = pat_from[trk_index(t)];
        ps = pat_from_h[trk_index(t)];
        pat_from[trk_index(t)] = undo.from;
        pat_from_h[trk_index(t)] = undo.from_h;
        undo.from = pn;
        undo.from_h = ps;
        for (i = P_SLEN; i <= P_SGATE; i++) {
            int16_t v = t->p[i];
            t->p[i] = undo.p[i];
            undo.p[i] = v;
        }
        for (i = 0; i < NSTEP; i++) {
            step_t s = t->step[i];
            t->step[i] = undo.step[i];
            undo.step[i] = s;
        }
    }
    fm1_irq_on();
    if (undo.what & UNDO_SOUND) {                 /* FM6: the track's patch as it was (edited, a project's, a
                                                   * converted DIGITAL sound), not a factory one */
        uint32_t tr = trk_index(t);
        uint8_t v[FP_SIZE + 1u], sl = fm6_slot[tr];
        memcpy(v, fm6_patch[tr], FP_SIZE);
        fm6_set_patch(tr, undo.fm6);
        fm6_slot[tr] = undo.fm6_slot;             /* (fm6_poll: the patch stays) */
        fm6_own_ok &= (uint8_t)~(1u << tr);
        memcpy(undo.fm6, v, FP_SIZE);
        undo.fm6_slot = sl;
    }
    undo.keep = 0;                                /* the next load copies the track as it is now */
    if (undo.what & UNDO_SOUND)
        morph_rebase(t);                          /* DUELL: another sound, the MORPH starts from it (morph.c) */
    if (t == TSEL)
        sync_reload = 1;
    b[1] = (char)('1' + trk_index(t));
    ui_say("UNDO/REDO ", b);
    ui.force = 1;
}

/* a 16-step pattern (PATTERNS[] format, user presets too) into steps 1..16, the rest empty, LEN 16 */
static void load_pat16(track_t *t, const uint8_t *note, const uint8_t *flags)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++) {
        step_t *s = &t->step[i];
        uint8_t n = i < 16u ? note[i] : 0, fl = i < 16u ? flags[i] : 0;
        s->note[0] = n;
        s->n = n ? 1 : 0;
        s->time = (fl & 4u) ? ST_TIE : n ? ST_NOTE : ST_REST;
        s->flags = n ? (fl & (SF_ACCENT | SF_SLIDE | SF_RATCH)) : 0;
        s->vel = n ? 96 : 0;
        s->probability = 0;
    }
    t->p[P_SLEN] = 16;
}

static void track_defaults_steps(track_t *t)
{
    uint32_t i;
    motion_reset(t);
    for (i = 0; i < NSTEP; i++)
        step_clear(&t->step[i]);
}

/* SAVE > PATTERNS: ONE list of DUELL's sixteen pattern slots ("P01".."P16"), all overwritable (patslot.c). The factory
 * patterns (PATTERNS[], 16: engines.c, SPEC 3.7) are the DEFAULT content of all 16 slots: a slot never saved (or erased) shows
 * and loads its factory pattern ("P03 OFFBEAT"); no slot is empty by default. SAVE writes the selected part's pattern
 * into the slot, ERASE deletes what was saved there (back to the factory default / empty). The patterns inside user
 * sounds are not listed (a sound still carries one). A factory pattern replaces the track's steps 1..16 (the rest
 * cleared) and LEN; a slot's own: all 64 steps with its LEN, DIV, SWING, GATE and QUANTIZE. The notes are
 * loaded as they are: the factory ones are C4 (60) + an offset, so they also serve the SEQUENCER arp as it is;
 * SCL TRANS / OCT transpose what plays */
_Static_assert(NPATTERNS <= PS_SLOTS, "the factory patterns are the default content of the pattern slots");
static uint32_t pat_count(void) { return PS_SLOTS; }
static int pat_has(uint32_t k) { return k < NPATTERNS || ps_used(k); }   /* the slot shows / loads something */

static void pat_label(uint32_t n, char *tag, char *name)   /* tag: 4 bytes ("P03"), name: 13 ("MELODY", "SAVED", "--") */
{
    ps_label(tag, n % PS_SLOTS);
    if (ps_used(n)) {
        ps_name(n % PS_SLOTS, name);                /* the stored name ("SAVED" if it has none) */
        if (!name[0])
            str_cpy(name, "SAVED", 13);
    } else if (n < NPATTERNS)
        str_cpy(name, PATTERNS[n].name, 13);
    else
        str_cpy(name, "--", 13);
}

static void pat_load(track_t *t, uint32_t n)
{
    load_begin(t, UNDO_PAT);
    if (ps_used(n))
        ps_load_trk(t, n);
    else if (n < NPATTERNS)
        load_pat16(t, PATTERNS[n].note, PATTERNS[n].flags);
    pat_sig[trk_index(t)] = steps_sig(t);
    pat_last[trk_index(t)] = (uint8_t)(n + 1u);
    pat_from[trk_index(t)] = 0;                     /* (a phrase: no PROJECT slot) */
    load_end(t);
    ui.force = 1;
}

/* a pattern load would throw away steps of the user's (recorded, edited, from a project): ask first */
static int pat_needs_confirm(const track_t *t)
{
    return !seq_is_empty(t) && steps_sig(t) != pat_sig[trk_index(t)];
}

static uint32_t pat_pick(void)                   /* ui.ppick inside the list (user presets may be gone) */
{
    uint32_t n = pat_count();
    return ui.ppick < n ? ui.ppick : n - 1u;
}

/* SAVE > PATTERNS LOAD: slot n into track t, "LOADED P03 MELODY" (factory default), "LOADED P03 <name>" (saved) */
static void pat_load_ui(track_t *t, uint32_t n)
{
    char tag[4], nm[13], b[20];
    pat_label(n, tag, nm);
    if (!pat_has(n)) {
        ui_message("EMPTY SLOT");
        return;
    }
    pat_load(t, n);
    str_cpy(b, tag, sizeof b);
    str_cpy(b + str_len(b), " ", 2);
    str_cpy(b + str_len(b), nm, sizeof b - str_len(b));
    ui_say("LOADED ", b);
}

/* SAVE > PATTERNS SAVE: the selected part's pattern into the slot picked under the name the NAME screen gave it
 * (ui_name.c NK_PATTERN; "SAVED P03"); ERASE: what was saved there
 * deleted (the factory default / empty again; "ALREADY FACTORY" / "EMPTY" when nothing was saved). Both
 * at once, while playing too: the flash write follows when nothing plays (patslot.c ps_poll) */
static void pslot_save_ui(uint32_t k, const char *name)
{
    char l[4];
    ps_label(l, k);
    if (ps_save_trk(TSEL, k, name)) {
        ui_message("SAVE ERROR");
        return;
    }
    pat_last[song.sel] = (uint8_t)(pat_pick() + 1u);   /* (the part now plays what the slot holds) */
    pat_sig[song.sel] = steps_sig(TSEL);
    ui_say("SAVED ", l);
    ui.force = 1;
}
static void pslot_erase_ui(uint32_t k)
{
    char l[4];
    uint32_t i;
    ps_label(l, k);
    if (ps_erase(k)) {
        ui_message(k < NPATTERNS ? "ALREADY FACTORY" : "EMPTY");
        return;
    }
    for (i = 0; i < NTRK; i++)                          /* (no part plays "the slot's pattern" any more) */
        if (pat_last[i] == pat_pick() + 1u)
            pat_last[i] = 0;
    ui_say("ERASED ", l);
    ui.force = 1;
}

/* the track's settings, not the sound's: what a sound load (factory or user preset, an engine jump,
 * TOOLS INIT) leaves alone. The mix (LEVEL, PAN, MUTE: the MIXER; 0.2.19: LEVEL is the player's mixer level, the sound's loudness is P_TRIM, which a load sets), the arpeggiator (ARP, ARP 2),
 * the scale and key map (SCL), the pattern parameters (LEN, DIV, SWING, GATE; 1.2: QUANTIZE). (DUELL: the SLICER
 * insert's ids were kept too; it is removed, its ids P_FREEA..D are not) */
static int param_kept(uint32_t i)
{
    return i == P_LEVEL || i == P_PAN || i == P_MUTE || (i >= P_AMODE && i <= P_SGATE) ||
           i == P_CHRD || i == P_VOIC || i == P_SQNT;
}

/* DUELL factory sounds (sounds.c) and the arp: a sound that sets the arp (DSF_ARP) lends it to the part. Its load keeps the
 * part's arp settings (P_AMODE .. P_AORDER) aside; the next sound loaded (factory or stored) gives them back, unless the arp
 * was changed since (then the change stays: the player's). A sound without arp pairs leaves the arp as it is (param_kept) */
_Static_assert(P_AORDER == P_AMODE + 7, "the arp's eight ids in a row");
static uint8_t ds_lent[NTRK];
static int16_t ds_arp_was[NTRK][8], ds_arp_set[NTRK][8];
/* ov: a stored patch's own arp (upreset.c: its AK block, the eight values), which wins over the factory sound s (a shadow copy
 * whose arp the player edited); 0: the factory sound's, if it has one */
static void ds_arp(track_t *t, const ds_sound_t *s, const int16_t *ov)
{
    uint32_t k = trk_index(t) % NTRK, i;
    int16_t *a = &t->p[P_AMODE];
    if (ds_lent[k]) {
        if (!memcmp(a, ds_arp_set[k], sizeof ds_arp_set[k]))
            memcpy(a, ds_arp_was[k], sizeof ds_arp_was[k]);
        ds_lent[k] = 0;
    }
    if (ov || (s && (s->flags & DSF_ARP))) {
        memcpy(ds_arp_was[k], a, sizeof ds_arp_was[k]);
        for (i = 0; i < 8u; i++)
            a[i] = TP[P_AMODE + i].def;
        if (ov)
            memcpy(a, ov, sizeof ds_arp_set[k]);
        else
            ds_put(s, t->p, 1);
        memcpy(ds_arp_set[k], a, sizeof ds_arp_set[k]);
        ds_lent[k] = 1;
    }
}

/* the same for the chord keys (P_CHRD, P_VOIC; DSF_KEYS): a pad may set chord keys. No scale: it is the instrument's (core.h) */
static const uint8_t DS_KEYID[2] = {P_CHRD, P_VOIC};
static uint8_t ds_klent[NTRK];
static int16_t ds_key_was[NTRK][2], ds_key_set[NTRK][2];
static void ds_keys(track_t *t, const ds_sound_t *s, const int16_t *ov)   /* ov: a patch's own CHRD VOIC (as ds_arp) */
{
    uint32_t k = trk_index(t) % NTRK, i;
    if (ds_klent[k]) {
        for (i = 0; i < 2u; i++)
            if (t->p[DS_KEYID[i]] == ds_key_set[k][i])
                t->p[DS_KEYID[i]] = ds_key_was[k][i];
        ds_klent[k] = 0;
    }
    if (ov || (s && (s->flags & DSF_KEYS))) {
        for (i = 0; i < 2u; i++) {
            ds_key_was[k][i] = t->p[DS_KEYID[i]];
            t->p[DS_KEYID[i]] = TP[DS_KEYID[i]].def;
        }
        if (ov)
            for (i = 0; i < 2u; i++)
                t->p[DS_KEYID[i]] = ov[i];
        else
            ds_put(s, t->p, 2);
        for (i = 0; i < 2u; i++)
            ds_key_set[k][i] = t->p[DS_KEYID[i]];
        ds_klent[k] = 1;
    }
}

/* the engine's factory sound pi (sounds.c; pi modulo its count), or INIT for an engine that has none: the sound only
 * (not the steps, not param_kept: the mixer LEVEL stays; but the factory sound's TRIM, its loudness, and its arp: ds_arp) */
static void apply_preset_to(track_t *t, uint32_t pi)
{
    uint32_t en = t->eng_req % NENGINES, n = ds_count(en), i;
    const engine_t *e = ENGINES[en];
    const ds_sound_t *s;
    load_begin(t, UNDO_SOUND);
    panic_req |= (uint8_t)(1u << trk_index(t));       /* MONO/POLY may change: release what sounds */
    t->user = 0;
    if (t == TSEL)
        sync_reload = 1;
    t->preset = (uint8_t)(n ? pi % n : 0u);
    s = ds_get(en, t->preset);
    for (i = 0; i < P_E0; i++)                        /* INIT: the sound's parameters to their defaults (a preset */
        if (!param_kept_on(t, i))                     /* sounds the same after any edit) */
            t->p[i] = TP[i].def;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = e->edit[i].def;
    if (s)
        ds_put(s, t->p, 0);
    ds_arp(t, s, 0);
    ds_keys(t, s, 0);
    ds_seq_back(t);                                   /* (a lent sequencer pattern back: upreset.c) */
    fm6_track_loaded(t);                              /* FM6: the sound's patch (its SLOT) */
    load_end(t);
}

/* the engine's defaults and its first preset. With the audio IRQ off: the ISR sees the old engine with
 * its values or the new one with its own (voice.c engine_block), never one with the other's */
static void set_engine_of(track_t *t, uint32_t ei)
{
    const engine_t *e = ENGINES[ei % NENGINES];
    uint32_t i;
    /* a retired engine (core.h ENGI_RETIRED: INIT on such a part, the editor's ENG): ANALOG, its first preset */
    if (!eng_ok(ei % NENGINES))
        ei = 0u;
    e = ENGINES[ei % NENGINES];
    load_begin(t, UNDO_SOUND);
    fm1_irq_off();
    t->eng_req = (uint8_t)(ei % NENGINES);
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = e->edit[i].def;
    apply_preset_to(t, 0);
    fm1_irq_on();
    load_end(t);
}

static void apply_preset(uint32_t pi) { apply_preset_to(TSEL, pi); }
static void set_engine(uint32_t ei) { set_engine_of(TSEL, ei); }

static void track_defaults(track_t *t)
{
    uint32_t i;
    for (i = 0; i < P_E0; i++)
        t->p[i] = TP[i].def;
    track_defaults_steps(t);
}

/* switch engine (its defaults + first preset) and say so */
static void select_engine(uint32_t e)
{
    set_engine(e);
    ui_say("ENGINE ", ENGINES[TSEL->eng_req]->name);
    ui.force = 1;
}

/* PATCHES (SPEC 3.6, 0.2.17): ONE list of 128 SLOTS, in slot order #1..#128. A slot is its saved record, else the factory sound at that
 * browse position (upreset.c, the slot layer sl_*): there is no other kind of entry, no empty place and no tail. The PRESETS knob, the
 * PATCHES page, the EDIT layer's sound list and MIDI program change all step this list. A part whose sound is in no slot (an engine
 * change, a multi, the editor; or its slot was overwritten) is at no place of it: total is returned (the browser shows "--", a step goes
 * to the first / last entry) */

/* PATCHES > TYPE (KNOB 2): ALL, FAV (favorites.filter, as up to 1.1.5), one category (1.2, Discussion #90: BASS ..
 * OTHER, category.c) or one engine (DUELL: a filter only; the part's engine is not changed): the list mode 0 ALL, 1 FAV,
 * 1 + CAT_* a category, 1 + CAT_N + n the engine at place n of ENGINE_ORDER. The category is kept with the settings in a
 * byte no engine uses (ui_lcat: 0 = none; firmware before reads ALL there, and favorites.filter stays 0 / 1 for it, the
 * editor and backups); the PRESETS knob and page browse that list. The filters look at what the slots hold now */
#define ui_lcat (favorites.factory[15][11])
#define LIST_N (1u + CAT_N + NENG_SHOWN)                 /* ALL FAV BASS LEAD PAD PLUCK KEYS PERC FX OTHER, then one per engine */
static uint32_t list_mode(void) { return favorites.filter ? 1u : ui_lcat && ui_lcat < LIST_N - 1u ? 1u + ui_lcat : 0u; }
static void list_set(uint32_t m)
{
    pl_pend_on = 0;
    favorites.filter = m == 1u;
    ui_lcat = (uint8_t)(m >= 2u && m < LIST_N ? m - 1u : 0u);
}
static const char *list_name(uint32_t m)
{
    return m == 0u ? "ALL" : m == 1u ? "FAV" : m <= CAT_N ? CAT_NAME[m - 1u] : ENGINES[eng_vis(m - CAT_N - 1u)]->name;
}
/* FAV is the slot's flag (favorites.c: the bits of the 128 slots): it stays through a save and a restore */
static int list_fav(uint32_t k) { return favorite_has(NENGINES, k); }
static int list_keep(uint32_t k)
{
    uint32_t m = list_mode();
    if (m > CAT_N)                                       /* one engine: only its sounds (a filter, nothing is loaded) */
        return sl_engine(k) == eng_vis(m - CAT_N - 1u);
    return m == 1u ? list_fav(k) : m ? sl_cat(k) == m - 1u : 1;
}
/* the category of the sound track t plays: its slot's, else its factory sound's (the editor's default) */
static uint32_t track_cat(const track_t *t)
{
    uint32_t s = slot_of(t);
    return s < UP_SLOTS ? sl_cat(s) : preset_cat(t->eng_req % NENGINES, t->preset);
}

/* place of the selected part's slot in the list shown (*total its length); total when its sound is in no slot or not in this TYPE */
static uint32_t preset_pos(uint32_t *total)
{
    uint32_t s = slot_of(TSEL), k, n = 0, pos = 0xFFFFFFFFu;
    for (k = 0; k < UP_SLOTS; k++) {
        if (!list_keep(k)) continue;
        if (k == s) pos = n;
        n++;
    }
    *total = n;
    return pos == 0xFFFFFFFFu ? n : pos;
}
static uint32_t preset_at(uint32_t n)                    /* place n of the list shown -> its slot; UP_SLOTS past the end */
{
    uint32_t k;
    for (k = 0; k < UP_SLOTS; k++)
        if (list_keep(k) && !n--) return k;
    return UP_SLOTS;
}
static void preset_go(uint32_t n);
/* PATCHES (SPEC 3.6): the cursor is the place of the slot the selected part plays; K1 steps on and loads it. */
/* DUELL 0.2.16 (KNOB ACCEL ON): K1 on PATCHES steps the cursor by the turn speed (accel, ui_input.c: x2..x8 over this list of 128 places) and loads
 * ONLY THE SOUND IT ENDS ON: an accelerated step moves the cursor (pl_pend) and loads nothing; the load follows PL_SETTLE_MS after the last step
 * (pl_poll, every frame), so a fast turn never loads the sounds it passes. A single step (a slow turn, KNOB ACCEL OFF) loads at once as before. */
#define PL_SETTLE_MS 150u
static uint16_t pl_pend;                                 /* its place index */
static uint32_t pl_pend_t;                               /* the fm1_ms it loads at */
static int32_t accel(uint32_t role, int32_t s, int32_t range);   /* (ui_input.c) */
static uint32_t pl_cursor(uint32_t *total)               /* place index, 0xFFFFFFFF = the sound is not in this TYPE */
{
    uint32_t n, pos = preset_pos(&n);
    *total = n;
    if (pl_pend_on && pl_pend < n)
        return pl_pend;
    return pos < n ? pos : 0xFFFFFFFFu;
}
static void pl_go(uint32_t i)                            /* place i (< total): its slot loads */
{
    pl_pend_on = 0;
    preset_go(i);
}
static void pl_commit(void)                              /* the waiting place loads now */
{
    uint32_t total;
    if (!pl_pend_on)
        return;
    (void)pl_cursor(&total);
    if (pl_pend < total)
        pl_go(pl_pend);
    pl_pend_on = 0;
}
static void pl_poll(void)                                /* every frame */
{
    if (!pl_pend_on)
        return;
    if (cur_page()->graph != GR_BROWSE || ui.menu)       /* the page was left: the cursor place is dropped, nothing loads */
        pl_pend_on = 0;
    else if ((int32_t)(fm1_ms - pl_pend_t) >= 0)
        pl_commit();
}
/* K1 / PRESETS: role = the knob's accel role, or 0xFF for no acceleration. steps = the signed detents of this read. KNOB ACCEL OFF (or role 0xFF): one place
 * per read in the direction of the turn, loaded at once, round the ends, exactly as before. */
static void pl_step(uint32_t role, int32_t steps)
{
    uint32_t total, cur = pl_cursor(&total);
    int32_t s = steps > 0 ? 1 : -1, t;
    if (!total) {
        ui_message(list_mode() == 1u ? "NO FAVORITES" : "NO SOUNDS IN LIST");
        return;
    }
    if (role != 0xFFu && (ui_prefs & PREF_ACCEL))
        s = accel(role, steps, (int32_t)total);
    if (s == 1 || s == -1 || cur >= total) {
        pl_go(cur >= total ? (s > 0 ? 0u : total - 1u) : (cur + (s > 0 ? 1u : total - 1u)) % total);
        return;
    }
    t = (int32_t)cur + s;                                /* a faster step: no wrap, no load until the turn settles */
    t = t < 0 ? 0 : t >= (int32_t)total ? (int32_t)total - 1 : t;
    pl_pend_on = 1;
    pl_pend = (uint16_t)t;
    pl_pend_t = fm1_ms + PL_SETTLE_MS;
    ui.force = 1;
}
/* the slot the cursor is on (also a place a fast turn waits on, pl_pend); UP_SLOTS: none (the list shown is empty, or the part's sound is in no slot of it) */
static uint32_t pl_cursor_slot(void)
{
    uint32_t total, cur = pl_cursor(&total);
    return cur < total ? preset_at(cur) : UP_SLOTS;
}
static uint32_t pl_slot(void)                            /* .. after a waiting step has loaded (RESTORE acts on a settled cursor) */
{
    pl_commit();
    return pl_cursor_slot();
}

/* FAV of the selected part's slot (a sound in no slot has no flag: the card says NO SLOT) */
static int preset_favorite(void)
{
    uint32_t s = slot_of(TSEL);
    return s < UP_SLOTS && list_fav(s);
}
static void preset_mark(int on)
{
    uint32_t s = slot_of(TSEL);
    if (s >= UP_SLOTS) {
        ui_message("NO SLOT");
        return;
    }
    if (favorite_set(NENGINES, s, on)) {
        ui.force = 1;
        settings_save();
    }
}

/* the pattern the selected track's sound suggests: its slot in the SAVE > PATTERNS list (a factory preset's PAT(n) =
 * slot n), or -1 (a user sound's own pattern is not in the list) */
static int32_t preset_pat_hint(void)
{
    return -1;                                          /* (DUELL's factory sounds suggest no pattern: sounds.c) */
}

static void preset_hinted(void)                     /* after a sound load: SAVE > PATTERNS starts at the suggested pattern */
{
    int32_t h;
    if ((h = preset_pat_hint()) >= 0)
        ui.ppick = (uint8_t)h;
    ui.force = 1;
}

static void preset_go(uint32_t n)                    /* load place n of the list shown into the selected track (the sound only) */
{
    uint32_t k = preset_at(n);
    if (k < UP_SLOTS)
        sl_load(k);
    preset_hinted();
}

/* DUELL (SPEC 4.2a): a PROGRAM CHANGE midi_control.c midi_pc queued, run here in the main loop. A multi: the MULTI page's
 * LOAD (project_load: both parts, each MORPH at A with its End B, every note off, playing goes on), an empty slot nothing.
 * A sound for part pi: program n = patch slot n (#n + 1), bank 0 and bank 1 alike (0.2.17: one list of slots; the banks were two lists before);
 * whatever TYPE the PATCHES page shows (a program number must not move with a browsing filter); loaded as the PRESETS knob loads it
 * (load_begin / load_end: the part's notes off, an engine change fades, the MORPH at A from the new sound), the part's other settings kept,
 * the selection too (the load's functions work on the selected part: it is pointed at pi for the load only) */
static void pc_recall(uint32_t pi, uint32_t prog, uint32_t bank)
{
    uint32_t was = song.sel;
    if (pi == PCQ_MULTI) {
        if (prog < MU_SLOTS && project_used(prog))
            project_load(prog);
        return;
    }
    if (pi >= NSHOWN || bank > 1u || prog >= UP_SLOTS)
        return;
    song.sel = (uint8_t)pi;
    sl_load(prog);
    song.sel = (uint8_t)was;
    sync_reload = 1;
    ui.force = 1;
}
static void pc_poll(void)                            /* every main-loop pass: the recalls waiting, in order */
{
    while (pcq_r != pcq_w) {
        uint32_t q = pcq_buf[pcq_r % PCQ_N];
        pcq_r = (uint8_t)(pcq_r + 1u);
        pc_recall(q & 0xFFu, (q >> 8) & 0x7Fu, q >> 16);
    }
}

/* the EDIT layer's KNOB 2 (ui_layer.c): the slots that hold a sound of the selected track's engine, in slot order. Place of the current
 * sound in it (the length when its sound is in no slot of that engine); *total the length */
static uint32_t eng_list_pos(uint32_t *total)
{
    uint32_t e = TSEL->eng_req % NENGINES, s = slot_of(TSEL), k, n = 0, cur = 0xFFFFFFFFu;
    for (k = 0; k < UP_SLOTS; k++)
        if (sl_engine(k) == e) {
            if (k == s)
                cur = n;
            n++;
        }
    *total = n;
    return cur == 0xFFFFFFFFu ? n : cur;
}

static void eng_list_step(int32_t direction)         /* the next / previous sound of the engine (wraps) */
{
    uint32_t total, cur = eng_list_pos(&total), e = TSEL->eng_req % NENGINES, k, n;
    if (!total)
        return;
    n = cur >= total ? (direction > 0 ? 0u : total - 1u) : (cur + (direction > 0 ? 1u : total - 1u)) % total;
    for (k = 0; k < UP_SLOTS; k++)
        if (sl_engine(k) == e && !n--) {
            sl_load(k);
            break;
        }
    preset_hinted();
}

static void preset_step(int32_t direction)
{
    uint32_t total, cur = preset_pos(&total);
    if (!total) { ui_message(list_mode() == 1u ? "NO FAVORITES" : "NO SOUNDS IN LIST"); return; }
    preset_go(cur >= total ? (direction > 0 ? 0 : total - 1) :
              (cur + (direction > 0 ? 1u : total - 1u)) % total);
}

/* Seven display rows. FAV and a category use a bounded window, not a repeating carousel.
 * Return total for an empty row; a current sound not in the list shows the start. */
static uint32_t preset_visible(uint32_t cur, uint32_t total, uint32_t row)
{
    uint32_t first, last;
    if (!total || row >= 7u) return total;
    if (!list_mode())
        return (cur + total * 4u + row - 3u) % total;
    first = cur < total && cur > 3u ? cur - 3u : 0u;
    last = total > 7u ? total - 7u : 0u;
    if (first > last) first = last;
    return first + row < total ? first + row : total;
}

/* select track i (KNOB 1 on TRACKS, the editor): its sound, pages and pattern from now on */
static void track_select(uint32_t i)
{
    if (i >= NSHOWN || i == song.sel)             /* (tracks 3, 4: hidden) */
        return;
    song.sel = (uint8_t)i;
    ui.entry_open = 0;
    ui.hot_t = 0;
    ui.ev_keep = 0;                              /* (AUTOMATION: a row kept at its default was the other track's) */
    ui.cursor = 0;
    ui.bank = 0;
    sync_reload = 1;
    ui.force = 1;
}

#include "ui_events.c"                            /* SEQ > AUTOMATION: locks, events, CHANCE, RATCH as a list (an action page) */
#include "midi_learn.c"                           /* MIDI LEARN (1.5): GLO + D4, a knob, a CC */

/* ---------------------------------------------------- action pages --- */
/* Pages whose purpose is an action (SAVE > USER, PATTERNS, PROJECT, TOOLS, SEQ > AUTOMATION): the knobs pick,
 * OCT+ does it, OCT- cancels the picked action or goes HOME (ui_input.c). There OCT- / OCT+ do not
 * shift the octave */
static int go_id(uint32_t id) { return id == G_LOAD || id == G_SAVE || id == G_CLRSEQ || id == G_INITSND; }

static uint32_t act_cols(void)                   /* the columns that are actions, a bit each; 0 = not such a page */
{
    const page_t *pg = cur_page();
    uint32_t c, m = 0;
    if (pg->graph == GR_EVENTS) return 16u;     /* AUTOMATION: no knob is an action; OCT+ by the row (ev_oct) */
    if (pg->graph == GR_TOOLS) return up_count() ? 7u : 3u;   /* CLEAR (the pattern), INIT (the sound), RESTORE (all slots; none when none holds a saved sound) */
    if (pg->graph == GR_PATS)
        return 14u;                              /* LOAD ERASE SAVE (ERASE, SAVE: a pattern slot P1..P16) */
    if (pg->graph == GR_SLOTS)
        return 14u;                              /* LOAD ERASE SAVE (MULTI: a multi M01..M16) */
    if (pg->scope == SC_GLOBAL)
        for (c = 0; c < 4u; c++)
            if (go_id(pg->id[c]))
                m |= 1u << c;
    return m;
}

/* the action OCT+ does: its column + 1, 0 = none picked yet (PATTERNS: LOAD until another is picked) */
static uint32_t act_col(void)
{
    if (cur_page()->graph == GR_EVENTS) return 5u;
    return cur_page()->graph == GR_PATS && !ui.act ? 2u : ui.act;
}

static const char *act_name(uint32_t c)          /* column c's action (the footer hint) */
{
    static const char *const UP_GO[3] = {"ON", "ERASE", "SAVE"};
    uint32_t id = cur_page()->id[c & 3u];
    if (cur_page()->graph == GR_EVENTS) return ev_act_name();
    if (cur_page()->graph == GR_TOOLS) {
        static const char *const actions[] = {"CLEAR", "INIT", "RESTORE", ""};
        return actions[c & 3u];
    }
    if (cur_page()->graph == GR_PATS || cur_page()->graph == GR_SLOTS)
        return UP_GO[(c + 2u) % 3u];
    return id == G_CLRSEQ ? "CLEAR" : id == G_INITSND ? "INIT" : id == G_LOAD ? "ON" : "SAVE";
}

/* the picked action would do something now (OCT+ breathes): another pattern, a saved slot, stopped for
 * a flash write, steps to clear */
static int act_ready(void)
{
    uint32_t c = act_col(), s = song.sel, id;
    if (!c--)
        return 0;
    if (cur_page()->graph == GR_EVENTS) return ev_act_ready();
    if (cur_page()->graph == GR_TOOLS)                  /* one case per column: CLEAR PAT, INIT, RESTORE ALL */
        return c == 0u ? !seq_is_empty(TSEL) || motion_count(TSEL) : c == 1u ? 1 : up_count() > 0u;
    if (cur_page()->graph == GR_PATS) {                 /* LOAD: another pattern; ERASE: a saved slot; SAVE: any slot */
        if (c == 2u) return ps_used(pat_pick());
        if (c == 3u) return 1;
        return pat_has(pat_pick()) && (pat_last[s] != pat_pick() + 1u || steps_sig(TSEL) != pat_sig[s]);
    }
    if (cur_page()->graph == GR_SLOTS)                  /* MULTI: LOAD a used one (playing too), ERASE / SAVE stopped */
        return c == 3u ? !song.playing : project_used((uint32_t)song.g[G_SLOT] - 1u) && (c == 1u || (!song.playing && !(c == 2u && project_factory((uint32_t)song.g[G_SLOT] - 1u))));
    id = cur_page()->id[c & 3u];
    if (id == G_LOAD)
        return project_used((uint32_t)song.g[G_SLOT] - 1u);
    if (id == G_SAVE)
        return !song.playing;
    if (id == G_CLRSEQ)
        return !seq_is_empty(TSEL);
    return 1;
}
