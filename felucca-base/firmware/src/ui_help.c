/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* HELP (1.5, Discussion #156; MENU > SYSTEM > HELP, OFF by default): a short, visual hint per page and per layer, key
 * caps and two or three words, never a screen of text.
 *   a page   entered (a page button, HOME): its hint in the footer's first row for HELP_FRAMES (~2 s; in place of the
 *            steps, which come back after), ui_draw.c draw_foot
 *   a layer  while its map shows: its footer's first row, in place of the steps (ui_draw.c draw_foot)
 * A hint is "[KEYCAP]WORDS" up to three times (gfx.c kc_tag names the keycaps); help_parse splits it for cv_key_row.
 * Pages by their title (a page added later has none until it is written here); HOME, HOME LEVELS and the DRUM grid by
 * what they are. Every hint fits its row with every word (tests/ui_test.c test_help: no word dropped). The setting and
 * the timer: ui.c help_on, help_start (PREF_HELP, ui.help_t) */

static const char H_EDIT[] = "[K1-4]SOUND";
static const char H_FMOP[] = "[K1]OPERATOR";
static const struct { const char *title, *hint; } HELP_PAGE[] = {
    {"AMP ENV", "[K1-4]AMP ENVELOPE: A D S R"},            /* (classic arrangement, SPEC 2.4: AMP ENV (ENV 1), ENV 2 the filter) */
    {"AMP DEST", "[K1-4]AMP ENV TO FLT PIT SHP"},
    {"ENV 2", "[K1-4]FILTER ENVELOPE: A D S R"},
    {"ENV 2 DEST", "[K1]CUTOFF[K2]SHAPE"},
    {"LFO 1", "[K1-4]SHAPE[LFO]HOLD: MOMENTARY"},
    {"LFO 1 SYNC", "[K1]SYNC TO BPM"},
    {"LFO 1 DEST", "[K1-4]LFO AMOUNTS"},
    {"LFO 2", "[K1-4]SHAPE: LFO 2 DEST, MOD"},
    {"LFO 2 SYNC", "[K1]SYNC TO BPM"},
    {"LFO 2 DEST", "[K1]VIBRATO[K2]CUTOFF[K3]SHAPE"},
    {"MOD 1", "[K1]SLOT[K2]SOURCE[K3]DEST"},
    {"MOD 2", "[K1]SLOT 5-8[K2]SOURCE[K3]DEST"},
    {"FX", "[K1-4]SENDS[FX]HOLD: PERFORM"},
    {"INSERT 1", "[K1]TYPE"},
    {"INSERT 2", "[K1]DRY / WET"},
    {"EQ 1", "[K1-4]LOW HIGH SHELVES"},
    {"EQ 2", "[K1-3]MID GAIN FREQ Q"},
    {"DUCK 1", "[K1]SRC[K2]RATE THR NOTE"},
    {"DUCK 2", "[K1]HOLD[K2]RELEASE[K3]CURVE"},
    {"DLY 1", "[K1-4]ALL TRACKS"},
    {"DLY 2", "[K1]PING-PONG[K2]SPREAD"},
    {"REVERB", "[K1]ROOM SPRING HALL SHIM RESO"},
    {"CHORUS", "[K1-4]ALL TRACKS"},
    {"SCL", "[K2]SCALE[K3]QUANTIZE"},
    {"CHORD", "[K1]CHORD[KEYS]ONE FINGER"},
    {"ENGINE", "[K1]ENGINE[K2]SOUND[K3]FAV"},
    {"EDIT 1", H_EDIT},
    {"EDIT 2", H_EDIT},
    {"FILTER", "[K1]LP BP HP[K2]CUTOFF"},
    {"SLICES", "[K1]SLICE[K2]MOVE"},
    {"OPERATOR 1", H_FMOP},
    {"OP ENV", H_FMOP},
    {"OPERATOR 2", H_FMOP},
    {"VOICE 1", "[K1]POLY, MONO[K2]GLIDE"},
    {"VOICE 2", "[K2]DETUNE[K3]PAN[K4]MUTE"},
    {"VOICE 3", "[K1]STEREO SPREAD"},
    {"PATCHES", "[K1]PATCH[K2]TYPE[K3]FAV"},          /* (K4 ERASE: its card says it; SAVE held saves over the patch, or into an empty place) */
    {"PATTERNS", "[K1]SLOT P1-16[K4]SAVE"},
    {"MULTIS", "[K1]SLOT M1-16[K4]SAVE"},
    {"TOOLS", "[K1]CLEAR PATTERN[K2]INIT SOUND"},
    {"ARP 1", "[K1]MODE"},
    {"ARP 2", "[K3]LATCH[K4]ORDER"},
    {"ARP 3", "[K1]TARGET"},                          /* (found by tests/step3_test.c: the ARP TARGET page had no hint) */
    {"STEP", "[KEYS]WRITE[K1]STEP"},
    {"AUTOMATION", "[K1]ROW"},
    {"MORPH", "[K1]A-B[K2]ROLL[K3]UNDO A|B"},           /* (4b; v2: no KEEP; SAVE held saves the patch; v3: ROLL K2, UNDO K3, AMOUNT K4 (3 hints at most: its card says it)) */
    {"MORPH SET", "[K2]SMART[K3]ARP[K4]PITCH"},
    {"MODE", "[K2]BAL[K3]XFADE[K4]SPLIT"},             /* (step 5) */
    {"MIXER", "[K4]MUTE[ALGO]PART"},                /* (3.5: OVERVIEW after it; CLOCK is GLO's since step 2) */
    {"GLO", "[KEYS]SOLO MUTE TAP[K1]P1 LVL[K2]P2 LVL"},          /* (step 6: GLO's main page; the keys F G SOLO, F# G# MUTE, C ALL, F TAP) */
    {"CHANNELS", "[K1]P1 CH[K2]P2 CH[K3]MIDI IN"},
    {"SIDECHAIN", "[K1]SC SRC[K2]SC NOTE"},
    {"MIDI LEARN", "[K1]LEARN ON / OFF[K2]CLEAR ALL"},
    {"CLOCK", "[K1]TEMPO[K2]SWING[K3]CLOCK"},
    {"TEST", "[K1]MORPH[K3]LOAD MULTI[K4]RESET"},    /* (the CPU test; K2 ROLL: its card says it) */
};
#define H_HOME "[PRESETS]SOUND[HOME]HOLD: MENU[PLAY]x2: STOP ALL"   /* (PLAY STOP pressed twice or held: STOP ALL, SPEC 1.2) */
#define H_LEVELS "[K1-4]T1-T4 LEVEL"
#define H_GRID "[KEYS]STEPS[K2]LANE"
/* a layer's (LAYER_*): its footer's first row while HELP is ON (draw_foot) */
static const char *const HELP_LAYER[LAYER_N] = {
    "", "[KEYS]BLACK: MUTES[FX]2 TAPS: LOCK", "", "[KEYS]ROOT[K1-4]ROOT SCL CHORD",   /* (GLO: a page now, HELP_PAGE) */
    "[K1]ENGINE[K2]SOUND[K3]FAV", "[K1]LENGTH[K3]SWING[K4]GATE", "[K1]CLICK[K2]COUNT-IN[K3]LEVEL",
};

/* the hint of the page shown, 0 = none */
static const char *help_page(void)
{
    uint32_t i;
    const char *t;
    if (ui.home)
        return home_levels() ? H_LEVELS : H_HOME;
    if (grid_on())
        return H_GRID;
    t = cur_page()->title;
    for (i = 0; i < NELEM(HELP_PAGE); i++)
        if (str_eq(HELP_PAGE[i].title, t))
            return HELP_PAGE[i].hint;
    return 0;
}
/* "[K1]SLOT[OCT+]SAVE" -> kh[] (at most 3), their words in b (bn bytes); the number of hints */
static uint32_t help_parse(const char *s, khint_t *kh, char *b, uint32_t bn)
{
    uint32_t n = 0, i = 0, len;
    int32_t id;
    while (s && n < 3u && (id = kc_tag(s, &len)) >= 0) {
        s += len;
        kh[n].key = (uint8_t)id;
        kh[n++].act = b + i;
        while (*s && *s != '[' && i + 2u < bn)
            b[i++] = *s++;
        b[i++] = 0;
    }
    return n;
}
/* a hint as a row of key hints from x 8 to 232 at y (the footer's rows: 2, 21); 0 = nothing to draw */
static int help_row(const char *s, int32_t y)
{
    khint_t kh[3];
    char b[48];
    uint32_t n = help_parse(s, kh, b, sizeof b);
    if (n)
        cv_key_row(8, 232, y, kh, n, 7u, T_BG);
    return n != 0u;
}
