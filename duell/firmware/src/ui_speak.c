/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL) */
/* DUELL 1.4: what the screen shows, said to a listening screen reader (announce.c, included by ui.c; fm1-reader INTEGRATION.md
 * section 4). Once a UI frame (main.c, after ui_draw) it compares the screen with the last frame and says what changed, in the
 * screen's own words (firmware/words.tsv turns the short ones into spoken words on the reader's side):
 *   - a new screen (a page, a layer, MENU's section, a dialog, the other part): its title, the part and its sound, the four cards;
 *     once it has rested 200 ms, so flipping through pages does not stutter
 *   - a knob turning: its card, "LABEL value unit", 120 ms after the value rests
 *   - MENU: the row picked and its value
 *   - a dialog: the question, what it is about, and how to answer it
 *   - the sound changing (PRESETS): its slot number and name
 *   - messages: ui_say (ui.c) says them at once
 * Nothing here runs while no reader listens (ann_on). Included at the end of ui_layer.c: every build with the UI has it. */
#if BENCH_TRIM   /* (2026-11-17, owner: "Bench build, no reader") bench builds only: no screen reader (and no splash logo, ui_draw.c) (announce.c stubs too) */
static void sp_card_set(uint32_t c, const char *label, const char *val, const char *unit) { (void)c, (void)label, (void)val, (void)unit; }
static void ui_speak(void) {}
#else
#define SP_MAX 241u
#define SP_CARD 40u
static char sp_card[4][SP_CARD];      /* the four cards as draw_column last had them, "LABEL value unit" ("" = no card) */
static char sp_buf[SP_MAX];           /* the text being built (not on the stack: fm1-reader INTEGRATION.md) */
static struct {
    uint8_t started, due;             /* the whole screen said since the reader came; a new screen waits to be said */
    uint32_t ctx, ctx_t;              /* the screen (a hash of what makes it another screen), when it last changed */
    uint32_t hot, row, snd;           /* the turned card's text, MENU's row, the sound: hashes of what was said last */
} sp;

/* b += s (b holds max): newlines as spaces, brackets and bytes outside printable ASCII left out, never two spaces, none first */
static void sp_cat(char *b, const char *s, uint32_t max)
{
    uint32_t n = str_len(b);
    for (; s && *s && n + 1u < max; s++) {
        char ch = *s == '\n' ? ' ' : *s;
        if ((uint8_t)ch < 0x20u || (uint8_t)ch > 0x7Eu || ch == '[' || ch == ']')
            continue;
        if (ch == ' ' && (!n || b[n - 1] == ' '))
            continue;
        b[n++] = ch;
    }
    b[n] = 0;
}
/* one item more: ". " before it when b has text */
static void sp_item(char *b, const char *s)
{
    char t[SP_CARD + 8];
    uint32_t n;
    t[0] = 0;
    sp_cat(t, s, sizeof t);
    for (n = str_len(t); n && t[n - 1] == ' '; n--)     /* (a card with no unit ends in its space) */
        t[n - 1] = 0;
    if (!t[0])
        return;
    if (b[0])
        sp_cat(b, ". ", SP_MAX);
    sp_cat(b, t, SP_MAX);
}

/* draw_column (ui_draw.c): card c as it is drawn this frame */
static void sp_card_set(uint32_t c, const char *label, const char *val, const char *unit)
{
    char *b = sp_card[c & 3u];
    if (!ann_on())
        return;
    b[0] = 0;
    if (!label[0] && !val[0])
        return;
    sp_cat(b, label, SP_CARD);
    if (b[0])
        sp_cat(b, " ", SP_CARD);
    sp_cat(b, val, SP_CARD);
    if (unit && unit[0]) {
        sp_cat(b, " ", SP_CARD);
        sp_cat(b, unit, SP_CARD);
    }
}

/* the screen's title into t (48); its kind: 0 a page, 1 a dialog, 2 MENU's rows, 3 MENU's other screens, 4 a layer, 5 the splash, 6 the GUIDE */
static uint32_t sp_title(char *t)
{
    uint32_t pi, n;
    static const char *const MENU_SCREEN[7] = {"", "", "ABOUT", "TEST", "MIDI LEARN", "BENCH", "SETTINGS"};
    t[0] = 0;
    if (splash_on) {                                   /* the start screen (ui_draw.c splash_frame): the name, and on the first start its hint */
        str_cpy(t, "DUELL. MULTIMORPHING BITIMBRAL SYNTHESIZER", 48);
        return 5;
    }
    if (ui.confirm) {
        str_cpy(t, "CONFIRM", 48);
        return 1;
    }
    if (ui.guide) {                                    /* SETUP or GUIDE and its place: "SETUP (1/5)" (the footer's counter) */
        uint32_t pg = (ui.guide - 1u) % GUIDE_N + 1u, lo = guide_lo();
        char c[12];
        str_cpy(t, guide_ask(pg) && !ui.guide_menu ? "SETUP (" : "GUIDE (", 48);
        fmt_int(c, (int32_t)(pg - lo + 1u));
        sp_cat(t, c, 48);
        sp_cat(t, "/", 48);
        fmt_int(c, (int32_t)(guide_hi() - lo + 1u));
        sp_cat(t, c, 48);
        sp_cat(t, ")", 48);
        return 6;
    }
    if (ui.menu == 1u) {
        str_cpy(t, MTAB_NAME[menu_sec() % MTAB_COUNT], 48);
        return 2;
    }
    if (ui.menu) {
        str_cpy(t, MENU_SCREEN[ui.menu < 7u ? ui.menu : 0u], 48);
        if (ui.menu == 6u) {                           /* the list of sections: the one picked */
            sp_cat(t, ". ", 48);
            sp_cat(t, MTAB_NAME[menu_sec() % MTAB_COUNT], 48);
        }
        return 3;
    }
    if (ui.layer && !ui.guide) {
        sp_cat(t, layer_head(), 48);
        return 4;
    }
    head_page_title(t, &pi);
    n = str_len(t);                                    /* a page's number in its title ("MACROS 1/2") as the reader's page mark "(1/2)": "page 1 of 2", not a fraction */
    if (n > 4u && n + 3u <= 48u && t[n - 4u] == ' ' && t[n - 2u] == '/' && t[n - 3u] >= '0' && t[n - 3u] <= '9') {
        t[n + 2u] = 0;                                 /* "MACROS 1/2" -> "MACROS (1/2)" */
        t[n + 1u] = ')';
        t[n] = t[n - 1u];
        t[n - 1u] = t[n - 2u];
        t[n - 2u] = t[n - 3u];
        t[n - 3u] = '(';
    }
    return 0;
}

/* MENU's row picked: its name and its value */
static void sp_menu_row(char *b)
{
    uint32_t row = ui.menu_sel;
    b[0] = 0;
    sp_cat(b, menu_label(row), SP_MAX);
    if (menu_valued(row)) {
        sp_cat(b, " ", SP_MAX);
        sp_cat(b, menu_vname(row, menu_get(row)), SP_MAX);
        if (row == MI_TUNE)
            sp_cat(b, " Hz", SP_MAX);
    }
}

/* the selected part and its sound: "PART 1, #12 PAD SPLIT" */
static void sp_sound(char *b)
{
    char s[24];
    char p[3] = {(char)('1' + (song.sel & 1u)), ',', 0};
    b[0] = 0;
    sound_label(TSEL, s);
    sp_cat(b, "PART ", 48);
    sp_cat(b, p, 48);
    sp_cat(b, " ", 48);
    sp_cat(b, s, 48);
}

/* the whole screen into sp_buf */
static void sp_screen(const char *title, uint32_t kind)
{
    char a[48], d[32];
    uint32_t c;
    sp_buf[0] = 0;
    sp_item(sp_buf, title);
    if (kind == 1u) {                                  /* a dialog: the question, the detail, the answers */
        a[0] = d[0] = 0;
        confirm_text(a, d);
        sp_item(sp_buf, a);
        sp_item(sp_buf, d);
        sp_item(sp_buf, "OCT+ YES, OCT- NO");
        return;
    }
    if (kind == 2u) {                                  /* MENU's section: the row picked */
        sp_menu_row(a);
        sp_item(sp_buf, a);
        return;
    }
    if (kind == 6u) {                                  /* the GUIDE: its words as drawn (ui_guide.c sp_guide); the option chosen and the keys follow in a second SAY */
        sp_cat(sp_buf, ". ", SP_MAX);
        sp_cat(sp_buf, sp_guide, SP_MAX);
        return;
    }
    if (kind == 5u) {                                  /* the splash: on the first start, its hint and keys */
        if (splash_guide) {
            sp_item(sp_buf, "Guide for your first start");
            sp_item(sp_buf, "OCT+ NEXT, HOME SKIP");
        }
        return;
    }
    if (kind == 3u)
        return;
    sp_sound(a);
    sp_item(sp_buf, a);
    for (c = 0; c < 4u; c++)
        sp_item(sp_buf, sp_card[c]);
}

static void ui_speak(void)
{
    char t[48];
    uint32_t kind, h, now = fm1_ms;
    int all;
    if (!ann_on()) {
        sp.started = 0;
        return;
    }
    all = !sp.started || ann_where();
    kind = sp_title(t);
    h = str_hash(kind * 977u + (uint32_t)ui.layer * 31u + (uint32_t)ui.menu * 7u + (uint32_t)song.sel * 131u + ui.confirm * 3u
                 + (kind == 6u ? guide_pick[0] * 5u + guide_pick[1] * 17u : 0u), t);
    if (all || h != sp.ctx) {                          /* another screen: said once it rests */
        sp.ctx = h;
        sp.ctx_t = now;
        sp.due = 1;
    }
    if (sp.due) {
        if (!all && now - sp.ctx_t < 200u)
            return;
        if (kind == 6u && !sp_guide[0]) {              /* the GUIDE's words are taken as it draws: drawn before the reader listened, draw it again */
            guide_sig = 0;
            return;
        }
        sp.due = 0;
        sp.started = 1;
        sp_screen(t, kind);
        ann_say(sp_buf, 0, ANN_NOW);
        if (kind == 6u) {                              /* the GUIDE's second SAY, after the first: the option chosen (SETUP), the keys */
            static const char *const PICK[2][3] = {{"KEYS OR OTHER MIDI", "CIRCUIT TRACKS", "MPC/FORCE"}, {"LAYER", "STEREO", "DUEL"}};
            uint32_t pg = (ui.guide - 1u) % GUIDE_N + 1u;
            sp_buf[0] = 0;
            if (guide_ask(pg) && !ui.guide_menu) {
                sp_cat(sp_buf, "CHOSEN: ", SP_MAX);
                sp_cat(sp_buf, PICK[pg - 1u][guide_pick[pg - 1u] % 3u], SP_MAX);
                sp_cat(sp_buf, ". ", SP_MAX);
            }
            sp_cat(sp_buf, sp_guide_keys, SP_MAX);
            ann_say(sp_buf, 0, 0);
        }
        sp.hot = ui.hot_t && ui.hot_col < 4u ? str_hash(ui.hot_col, sp_card[ui.hot_col]) : 0u;
        sp_menu_row(sp_buf);
        sp.row = str_hash(0u, sp_buf);
        sp_sound(sp_buf);
        sp.snd = str_hash(0u, sp_buf);
        return;
    }
    if (kind == 2u) {                                  /* MENU: the row moved to, or its value changed */
        sp_menu_row(sp_buf);
        h = str_hash(0u, sp_buf);
        if (h != sp.row) {
            sp.row = h;
            ann_later(sp_buf, ANN_NOW, 120u, now);
        }
        return;
    }
    if (kind)
        return;
    if (ui.hot_t && ui.hot_col < 4u) {                 /* a knob turning: its card */
        h = str_hash(ui.hot_col, sp_card[ui.hot_col]);
        if (h != sp.hot && sp_card[ui.hot_col][0]) {
            sp.hot = h;
            ann_later(sp_card[ui.hot_col], ANN_NOW, 120u, now);
        }
    }
    sp_sound(sp_buf);                                  /* the sound changed (PRESETS, a load) */
    h = str_hash(0u, sp_buf);
    if (h != sp.snd) {
        sp.snd = h;
        ann_later(sp_buf, ANN_NOW, 200u, now);
    }
}
#endif /* BENCH_TRIM */
