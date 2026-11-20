/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Physical panel: which matrix button / encoder carries which printed label.
 * The matrix ids are known, the printed labels are not, so
 * the mapping is a table with a best guess, overridable by HARDWARE CALIBRATION
 * (hold OCT- and OCT+ while powering on): FELUCCA asks for each label in turn.
 * The learned table lives in .noinit and, with FELUCCA_FLASH, in flash with
 * the settings (project.c); read it back with `fm1t memr` to bake it in. */
enum { B_FX, B_SCL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC,
       B_OCTDN, B_OCTUP, NB };
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };
static const char *const B_NAME[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE",
                                        "ARP", "SEQ", "PLAY", "REC", "OCT -", "OCT +"};
static const char *const E_NAME[NE] = {"SELECT", "ALGORITHM", "PRESETS", "KNOB 1", "KNOB 2",
                                        "KNOB 3", "KNOB 4"};
#define PANEL_MAGIC 0x50414E35u          /* "PAN5": bump when PANEL_DEFAULT changes */

typedef struct {
    uint32_t magic;
    uint8_t btn[NB];             /* matrix button id (0..13) per label */
    uint8_t enc[NE];             /* matrix encoder (0..6) per role */
    int8_t dir[NE];              /* +1 / -1 so that clockwise is + */
} panel_t;
panel_t panel __attribute__((section(".noinit")));

#if BOARD_SMK37
/* The SMK-37 (owner's control plan, research/smk37-elite-port-study, section 5): the buttons do what is printed on them, DUELL's
 * page buttons without a print of their own on the free ones. Hardware ids (hal/smk_input.h SMK_BTNC): 0 KNOB BANK, 1 FADER BANK,
 * 2 <, 3 >, 4 OCTAVE-, 5 OCTAVE+, 6 ARP, 7 NOTE REPEAT, 8 SCALE, 9 CHORD, 10 GLOBE, 11 BT, 12 PATCH, 13 PARA, 14 FX, 15 SEQ,
 * 16 SEQ PLAY, 17 SEQ REC. Labels: FX = FX, SEL (the scale page) = SCALE, ENV = FADER BANK, LFO = NOTE REPEAT, EDIT = PARA,
 * GLO = GLOBE, HOME = PATCH, SAVE = BT, ARP, SEQ, PLAY = SEQ PLAY, REC = SEQ REC, OCT- / OCT+ = OCTAVE- / OCTAVE+.
 * KNOB BANK, < >, CHORD have no FM-1 label: src/smk_ui.c. The table is fixed (no HARDWARE CALIBRATION on the SMK-37). */
/* Owner 2026-11-18: KNOB BANK = LFO, FADER BANK = ENV, both together = MATRIX; NOTE REPEAT = PERFORM; BT = MENU; PATCH = HOME and the
 * SAVE pages (PATCHES, PATTERNS, MULTIS, TOOLS) in one round. The extras below: smk_ui.c smk_buttons */
enum { SMK_B_KNOB_BANK = 0, SMK_B_FADER_BANK = 1, SMK_B_LEFT = 2, SMK_B_RIGHT = 3, SMK_B_NOTE_REPEAT = 7, SMK_B_CHORD = 9, SMK_B_BT = 11 };
#define SMK_B_EXTRA ((1u << SMK_B_KNOB_BANK) | (1u << SMK_B_FADER_BANK) | (1u << SMK_B_LEFT) | (1u << SMK_B_RIGHT) | (1u << SMK_B_CHORD) | (1u << SMK_B_NOTE_REPEAT) | (1u << SMK_B_BT))
#define PANEL_NBTN FM1_NBTN
static const panel_t PANEL_DEFAULT = {
    PANEL_MAGIC,
    {14, 8, 29, 30, 13, 10, 12, 31, 6, 15, 16, 17, 4, 5},   /* (owner 2026-11-18: ENV = FADER BANK, LFO = KNOB BANK and MATRIX on both, in smk_ui.c smk_buttons, so their labels get ids that are never pressed: 29, 30; SAVE has no button, its pages are in PATCH's round: 31) */
    {0, 1, 2, 3, 4, 5, 6},                           /* (not used: panel_enc below reads the SMK's eight knobs itself) */
    {1, 1, 1, 1, 1, 1, 1},
};
static void panel_init(void) { panel = PANEL_DEFAULT; }
#else
#define PANEL_NBTN 14u
static const panel_t PANEL_DEFAULT = {
    PANEL_MAGIC,
    {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 1},   /* PLAY = 12, REC = 13 (measured on hardware) */
    {0, 1, 6, 2, 3, 4, 5},                           /* SELECT = enc 0, ALGORITHM = enc 1, PRESETS = enc 6 (printed labels) */
    {1, 1, 1, 1, 1, 1, 1},
};

static void panel_init(void)                     /* also after a flash load: ids are used as array indexes and shifts */
{
    uint32_t i, buttons = 0, encoders = 0, ok = panel.magic == PANEL_MAGIC;
    for (i = 0; ok && i < NB; i++) {
        ok = panel.btn[i] < NB && !(buttons & (1u << panel.btn[i]));
        if (ok) buttons |= 1u << panel.btn[i];
    }
    for (i = 0; ok && i < NE; i++) {
        ok = panel.enc[i] < NE && !(encoders & (1u << panel.enc[i])) &&
             (panel.dir[i] == 1 || panel.dir[i] == -1);
        if (ok) encoders |= 1u << panel.enc[i];
    }
    if (!ok)
        panel = PANEL_DEFAULT;
}
#endif

static uint32_t panel_btn_of(uint32_t matrix_id)        /* label of a matrix button, NB if none */
{
    uint32_t b;
    for (b = 0; b < NB; b++)
        if (panel.btn[b] == matrix_id)
            return b;
    return NB;
}

/* steps of a role, + = clockwise */
static uint8_t panel_moved;                     /* a knob turned since ui.c scr_input last looked (SCREEN OFF) */
#if BOARD_SMK37
/* The SMK-37's eight knobs (owner): K1..K4 always part 1's four cards, K5..K8 part 2's, on every page; turning one shows that part
 * (track_select). On a page that edits no part (GLO, MODE, the MENU) K5..K8 do what K1..K4 do. No ALGORITHM. SELECT is < and >
 * (smk_ui.c counts their presses into smk_sel_steps). PATCH held: the knobs change the sound (PRESETS) of their part, nothing
 * else. KNOB BANK on: K1..K4 the PERFORM page's knobs, K5..K8 the user's four (smk_ui.c). The bodies: src/smk_ui.c */
static int32_t smk_sel_steps;
static int32_t smk_knob(uint32_t k);            /* (smk_ui.c) */
static int32_t smk_preset_take(void);           /* (smk_ui.c) */
static int32_t panel_enc(uint32_t role)
{
    int32_t s = 0;
    if (role == EN_SELECT) {
        s = smk_sel_steps;
        smk_sel_steps = 0;
    } else if (role == EN_PRESET) {
        s = smk_preset_take();
    } else if (role >= EN_K1 && role <= EN_K4) {
        s = smk_knob(role - EN_K1);
    }
    if (s)
        panel_moved = 1;
    return s;
}
#else
static int32_t panel_enc(uint32_t role)
{
    int32_t s = fm1_enc_take(panel.enc[role]) * panel.dir[role];
    if (s)
        panel_moved = 1;
    return s;
}
#endif

/* Keep the retained settings layout fixed. palette: the index into UI_PALETTES (SET3 held an index
 * into the 20 palettes of earlier firmware) */
#define SETTINGS_MAGIC 0x53455434u              /* "SET4" */
#define SETTINGS_MAGIC_OLD 0x53455433u          /* "SET3" */
struct { uint32_t magic, palette, rsv, zoom; } settings __attribute__((section(".noinit")));

static void settings_save(void);              /* project.c: flash copy (FELUCCA_FLASH) */

/* HOLD (menu): how long a button is held before its layer opens (ui_input.c). Saved in the settings record's
 * retired bold field as HOLD_TAG | index; any other value there (0 or 1 from older firmware) is the default */
#define HOLD_TAG 0x484C4400u
#define HOLD_DEF 1u
static const uint16_t HOLD_MS[4] = {300, 400, 500, 600};
static uint8_t settings_hold = HOLD_DEF;
static uint32_t hold_from_stored(uint32_t v) { return (v & ~3u) == HOLD_TAG ? v & 3u : HOLD_DEF; }
static uint32_t hold_to_stored(uint32_t old, uint32_t i)
{
    return i % 4u != HOLD_DEF ? HOLD_TAG | (i & 3u) : old > 1u ? 0u : old;
}
static int hold_stored_ok(uint32_t v) { return v <= 1u || (v & ~3u) == HOLD_TAG; }

/* LEDS (menu): OFF, LOW, MEDIUM, MAX. MEDIUM (the default), the idle buttons and keys are lit 1 frame in 4 (25 %) and the
 * active ones fully (#35); LOW a faint glow (~1/60, hal/fm1_input.h THE LED LEVELS); OFF no glow,
 * the active ones lit (as 1.0); MAX the idle ones lit and the active ones dark, as the stock firmware (ui_input.c
 * ui_leds). Saved in the settings record's retired zoom field as LEDS_TAG | mode, the modes append-only: DIM 0
 * (now MEDIUM) and MAX 1 keep their values, OFF 2 and LOW 3 are new; any other value there (0 or 1 from older
 * firmware) is MEDIUM. LEDS_MENU: the menu's order, darkest first */
#define LEDS_TAG 0x4C454400u                    /* "LED" */
enum { LEDS_DIM, LEDS_INV, LEDS_OFF, LEDS_DIM_LO, LEDS_COUNT };
static const uint8_t LEDS_MENU[LEDS_COUNT] = {LEDS_OFF, LEDS_DIM_LO, LEDS_DIM, LEDS_INV};
static const char *const LEDS_NAME[LEDS_COUNT] = {"MEDIUM", "MAX", "OFF", "LOW"};
static uint8_t settings_leds = LEDS_DIM;
static uint32_t leds_from_stored(uint32_t v) { return (v & ~3u) == LEDS_TAG && (v & 3u) < LEDS_COUNT ? v & 3u : LEDS_DIM; }
static uint32_t leds_to_stored(uint32_t old, uint32_t m)
{
    return m != LEDS_DIM && m < LEDS_COUNT ? LEDS_TAG | m : old > 1u ? 0u : old;
}
static int leds_stored_ok(uint32_t v) { return v <= 1u || ((v & ~3u) == LEDS_TAG && (v & 3u) < LEDS_COUNT); }

static void settings_init(void)
{
    if (settings.magic == SETTINGS_MAGIC_OLD && settings.palette < 20u) {
        settings.magic = SETTINGS_MAGIC;
        settings.palette = palette_from_stored(settings.palette);
    }
    if (settings.magic != SETTINGS_MAGIC || settings.palette >= NPALETTES) {
        settings.magic = SETTINGS_MAGIC;
        settings.palette = UI_DEFAULT_INDEX;   /* the first theme */
        settings.rsv = 0;
        settings.zoom = 0;                     /* (retired: the LEDS setting, settings_persist.c) */
    }
    palette_set(settings.palette);
}
