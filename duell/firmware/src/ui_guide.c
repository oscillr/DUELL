/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.11: the first-start GUIDE (SPEC "First-start guide"; docs/GUIDE_PLAN.md, the owner's pages). Included at the end of ui_layer.c.
 *   WHEN       once, after the power-on splash, while the settings bit "guide seen" (ui.c PREF_GUIDE_SEEN, bit 5 of [15][24]) is clear: a
 *              new unit, a flash erase, and any settings record of an older build (the bit was never written before), so an update
 *              shows it once too. The splash then stays GUIDE_SPLASH_MS longer (ui_draw.c splash_draw). Leaving it, or finishing it,
 *              sets the bit and writes the settings once (settings_save: the write waits for silence, as every settings write since 0.2.11: project.c settings_poll).
 *              Power off with the guide up: the bit stays clear, so the next start shows it again.
 *              MENU > SYSTEM > GUIDE opens the same pages at any time; that never touches the bit.
 *   PAGES      GUIDE_N full-screen pages over whatever page is under them: the header (item 118: its title SETUP or GUIDE, no counter in it), the text, the legend row
 *              (rows 197 .. 209: OCT + NEXT, OCT - BACK, HOME SKIP) and the SHARED footer (engine, patch, the counter 1/5, battery: ui_draw.c draw_foot). A page's text: "#" a heading (L, the theme colour), "~" a
 *              small gap, "*" a line whose words up to its first ':' are in the theme colour (what it is, then how), else a line
 *              (M, the text colour); each line at most GD_W px (the layout lint, tests/ui_render.c, checks
 *              every page in every palette).
 *   KEYS       SELECT right / OCT+ the next page, SELECT left / OCT- the previous one (SELECT stops at both ends, OCT+ on the last
 *              page finishes); HOME leaves at once (from the MENU it closes the menu, as HOME does there); EDIT FX ENV LFO ARP
 *              SCL GLO SEQ SAVE leave at once and go on to their own job, in the same press (as they leave the MENU); PLAY/STOP and
 *              REC (the HOLD switch) work as on HOME; the keys and MIDI play as ever. KNOB 1-4, PRESETS and ALGORITHM do nothing (the header
 *              says "[HOME] CLOSES GUIDE" for a moment, so no turn is silently ignored). */
#define GD_X UT_T6_X                                    /* the text's left edge (item 118: x 9, as the picture; was 12) */
#define GD_W (240 - 2 * UT_T6_X)                        /* the text's room: x 9 .. 230 */
#define GD_HINT_Y UT_T6_END                             /* the body's end (item 118): the legend row is drawn in the last strip, the shared footer (draw_foot) owns rows 217 .. 239 */
static uint8_t guide_pick[2];                           /* the option picked on SETUP 1/5 (0 LAYER, 1 STEREO, 2 DUEL) and on SETUP 5/5 (0 LATER, 1 NOW) */
#define GUIDE_KNOB_MSG "HOME ENDS THE GUIDE"            /* a knob turned while it is up (it does nothing there) */
static const khint_t *guide_kh;                       /* the key hints drawn last (the host test reads them) */
static uint32_t guide_sig;                              /* the page drawn last (+ 1; 0: draw it) */
static uint32_t guide_lo(void) { return ui.guide_menu ? 2u : 1u; }          /* the first and the last page shown (the menu's: the GUIDE pages only) */
static uint32_t guide_hi(void) { return ui.guide_menu ? 4u : GUIDE_N; }
static int guide_ask(uint32_t pg) { return pg == 1u || pg == 5u; }          /* SETUP pages ask, GUIDE pages tell */

#if DUELL_BENCH
/* (DUELL_BENCH developer builds, HANDOFF_V2 item 107: the first-start guide is not built in, so XIP holds the CPU suite and the velocity capture together:
 * it counts as seen, so no start shows it and the splash is not stretched; the pages' text and the H / G heading faces (about 19 KB) are left out.
 * The GUIDE row is the BENCH row there; nothing of the audio side is touched.) */
static int guide_seen(void) { return 1; }
#else
static int guide_seen(void) { return (ui_prefs2 & (PREF_GUIDE_SEEN >> 16)) != 0u; }
#endif
/* opened: by the splash's end (from_menu 0: the first start, all five pages) or MENU > SYSTEM > GUIDE (1: the three GUIDE pages) */
static void guide_open(uint32_t from_menu)
{
#if DUELL_BENCH
    if (from_menu) {                                    /* (bench builds: the GUIDE row is the BENCH row, menu_items.c: it opens the BENCH screen, src/bench.c;
                                                         * the first start's guide opens as ever) */
        ui.menu = 5;
        bench_open();
        ui.force = 1;
        return;
    }
#endif
    ui.guide_menu = (uint8_t)(from_menu != 0u);
    ui.guide = (uint8_t)guide_lo();
    guide_pick[0] = 0;
    guide_pick[1] = 0;
    guide_sig = 0;
    ui.force = 1;
    if (!from_menu) {                                   /* (a fresh unit's "LOADED" of M01, made under the splash: not over the first page) */
        ui.msg_t = 0;
        ui.msg2[0] = 0;
    }
}
/* left or finished: the first start's guide sets the bit and writes the settings once; the MENU's never touches it (and goes
 * back to the MENU, which is still open under it) */
static void guide_close(void)
{
    uint32_t first = !ui.guide_menu;
    ui.guide = 0;
    ui.guide_menu = 0;
    ui.force = 1;
    if (first && !guide_seen()) {
        ui_prefs2 |= (uint8_t)(PREF_GUIDE_SEEN >> 16);
        settings_save();
    }
}
/* GLO > MIDI (SETUP 5/5: NOW): the page itself, as if its button had been pressed */
static void guide_to_midi(void)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].graph == GR_MIDI && page_visible(i)) {
            ui.page = (uint8_t)i;
            page_entered();
            break;
        }
}
static void guide_page(int32_t d)                      /* the next / previous page, stopping at both ends */
{
    ui.guide = (uint8_t)clamp((int32_t)ui.guide + d, (int32_t)guide_lo(), (int32_t)guide_hi());
}
/* the choice of the page just left: SETUP 1/5 sets the mode (the audio takes it at its next block, seq.c mode_apply) */
static void guide_apply(uint32_t pg)
{
    if (pg == 1u && !ui.guide_menu)
        duell_mode_req = (uint8_t)(guide_pick[0] == 0u ? DM_LAYER : guide_pick[0] == 1u ? DM_STEREO : DM_SPLIT);
}

/* ui_input, while it is up (after the HOME and REC button timers). 1: it took this frame's input; 0: it closed for a page button or
 * SAVE, whose press goes on through ui_input to its job */
static void play_press(uint32_t now);                   /* (ui_input.c: PLAY/STOP pressed) */
static int guide_input(uint32_t pressed, uint32_t rec, uint32_t now)
{
    int32_t s;
    if ((pressed >> panel.btn[B_HOME]) & 1u) {          /* HOME: leave at once (used up: no tap, no menu) */
        guide_close();
        ui.home_t0 |= 2u;
        if (ui.menu)
            menu_close();                               /* (as HOME closes the MENU) */
        enc_drop();
        return 1;
    }
    if (leave_pressed(pressed) || ((pressed >> panel.btn[B_SAVE]) & 1u)) {
        guide_close();
        return 0;
    }
    if ((pressed >> panel.btn[B_PLAY]) & 1u)            /* PLAY/STOP as on HOME (twice: STOP ALL); its hold: play_hold */
        play_press(now);
    if (rec == BT_TAP && !ui.menu)                      /* REC: the HOLD switch (rec_tap: rec_is_hold on HOME) */
        rec_tap();
    song.grid = (uint8_t)keys_mode();                   /* (the keys play) */
    if ((pressed >> panel.btn[B_OCTUP]) & 1u) {
        guide_apply(ui.guide);
        if (ui.guide >= guide_hi()) {                   /* OCT+ on the last page: done (SETUP 5/5: NOW goes on to GLO > MIDI) */
            uint32_t now_midi = !ui.guide_menu && guide_pick[1] == 1u;
            guide_close();
            if (now_midi)
                guide_to_midi();
            enc_drop();
            return 1;
        }
        guide_page(1);
    }
    if ((pressed >> panel.btn[B_OCTDN]) & 1u)
        guide_page(-1);
    s = panel_enc(EN_SELECT);
    if (guide_ask(ui.guide) && !ui.guide_menu) {      /* SETUP 1/5 and 5/5: the four knobs turn through the options too (they have nothing else to do there) */
        uint32_t k;
        for (k = EN_K1; k <= EN_K4; k++) {
            int32_t d = panel_enc(k);
            if (d)
                s = d;
        }
    }
    if (s != 0) {
        if (guide_ask(ui.guide) && !ui.guide_menu) {    /* a question: SELECT picks the option */
            uint32_t pk = ui.guide == 1u ? 0u : 1u, n = ui.guide == 1u ? 3u : 2u;
            guide_pick[pk] = (uint8_t)clamp((int32_t)guide_pick[pk] + (s > 0 ? 1 : -1), 0, (int32_t)n - 1);
        } else
            guide_page(s > 0 ? 1 : -1);
    }
    {   /* KNOB 1-4, PRESETS, ALGORITHM: nothing changes, and the header says why (no silently ignored control) */
        uint32_t k, any = 0;
        for (k = 0; k < NE; k++)
            any |= k != EN_SELECT && !(guide_ask(ui.guide) && !ui.guide_menu) && panel_enc(k) != 0;   /* (a SETUP question: no message at all) */
        if (any)
            ui_message(GUIDE_KNOB_MSG);
    }
    return 1;
}

static int32_t guide_last_x;                           /* the right end of the last line guide_wrap drew */
/* a text wrapped at spaces into lines of at most w px at pitch p from (x, y); the y after it */
static int32_t guide_wrap(int32_t x, int32_t y, int32_t w, const aafont_t *f, const char *s, uint16_t fg, int32_t p)
{
    char ln[64];
    uint32_t n, last;
    const char *q = s;
    while (*q) {
        last = 0;
        for (n = 0; q[n] && n + 1u < sizeof ln; n++) {
            ln[n] = q[n];
            ln[n + 1u] = 0;
            if (q[n] == ' ')
                last = n;
            if (text_w(f, ln) > w && last) {
                n = last;
                break;
            }
        }
        ln[n] = 0;
        guide_last_x = cv_text(x, y, f, ln, fg);
        y += p;
        q += n;
        while (*q == ' ')
            q++;
    }
    return y;
}
#if !DUELL_BENCH                                       /* (the guide's page drawing: not in a DUELL_BENCH developer build, see guide_seen) */
/* DUELL 1.1 (FIRST_RUN_SETUP_V2_A.png): a sentence (M) wrapped at spaces to w px, a word "[SAVE]" drawn as its keycap (kc_tag) in the line, centred on the line's capitals; the first line's capitals end on row `base`,
 * the next ones UT_T6_G_LINE under it; -> the last line's baseline */
static int32_t guide_rich(int32_t x, int32_t base, int32_t w, const char *s)
{
    int32_t cx = x;
    while (*s) {
        char wd[24];
        uint32_t n = 0, tl = 0;
        int32_t id, wpx;
        while (*s == ' ')
            s++;
        while (s[n] && s[n] != ' ' && n + 1u < sizeof wd) {
            wd[n] = s[n];
            n++;
        }
        wd[n] = 0;
        id = kc_tag(wd, &tl);
        wpx = id >= 0 ? kc_w((uint32_t)id) : text_w(&AF_M, wd);
        if (id >= 0 && wd[tl])                          /* (a mark after the keycap: "[OCT+]," ) */
            wpx += text_w(&AF_M, wd + tl);
        if (cx > x && cx + wpx > x + w) {
            cx = x;
            base += UT_T6_G_LINE;
        }
        if (id >= 0) {
            cx = cv_keycap(cx, base - AF_M_CAP_H + 1 - (KC_H - AF_M_CAP_H) / 2, (uint32_t)id, T_KEY, T_KINK, T_BG);
            if (wd[tl])
                cx = cv_text_on(cx, UT_TY(M, base), &AF_M, wd + tl, T_TEXT, T_BG);
        } else
            cx = cv_text_on(cx, UT_TY(M, base), &AF_M, wd, T_TEXT, T_BG);
        cx += 5;
        s += n;
    }
    return base;
}
/* TEMPLATE 6 (item 118; ui_tokens.h): the rows of one option block */
typedef struct { int16_t top, h, nbase, dbase; } t6_opt_t;
/* a question's option: the picked one on a box of the selected part's colour (hard corners), its name (L, white) on baseline nbase, up to three explanation lines (S; grey, lilac on the box) from dbase, 15 px apart, a pill (RECOMMENDED,
 * DEFAULT) after the name in the part's colour, as tall as the name's capitals */
static void guide_option(const t6_opt_t *g, const char *name, const char *d1, const char *d2, const char *d3, const char *tag, uint32_t picked)
{
    const char *d[3];
    uint32_t i;
    uint16_t pc = T_PTHEME(song.sel), sel = ux_mix(T_BG, pc, UT_T6_SEL_PCT), fill = picked ? sel : T_BG;
    uint16_t grey = ux_mix(T_BG, T_TEXT, UT_GREY_PCT), lilac = ux_mix(sel, T_TEXT, UT_T6_DESC_SEL_PCT);
    int32_t nx;
    d[0] = d1;
    d[1] = d2;
    d[2] = d3;
    if (picked)
        { CV_HL(UT_T6_BOX_X, g->top, UT_T6_BOX_W, g->h); cv_card(UT_T6_BOX_X, g->top, UT_T6_BOX_W, g->h, sel, T_BG); }
    nx = cv_text_on(GD_X, UT_TY(L, g->nbase), &AF_L, name, T_TEXT, fill);
    if (tag[0]) {
        int32_t pw = text_w(&AF_S, tag) + 2 * UT_T6_PILL_PAD, ph = AF_L_CAP_H, px = nx + UT_T6_PILL_GAP, py = g->nbase - ph + 1;
        uint16_t ink = ux_cr(T_TEXT, pc) >= 300u ? T_TEXT : ux_cr(T_BG, pc) >= ux_cr(T_TEXT, pc) ? T_BG : T_TEXT;
        cv_rrect(px, py, pw, ph, UT_T6_PILL_R, pc, fill);
        cv_text_in(px, py + HALF_UP(ph - AF_S_CAP_H) - AF_S_CAP_Y, pw, &AF_S, tag, ink, pc);
    }
    for (i = 0; i < 3u; i++)
        if (d[i][0])
            cv_text_on(GD_X, UT_TY(S, g->dbase + (int32_t)i * UT_T6_LINE), &AF_S, d[i], picked ? lilac : grey, fill);
}
/* a thin line from (x, y0) down to y and right to x1 (the AVAILABLE VOICES notes' leaders) */
static void guide_leader(int32_t x, int32_t y0, int32_t y, int32_t x1)
{
    cv_rect(x, y0, 1, y - y0 + 1, T_MID);
    if (x1 < x)
        cv_rect(x1, y, x - x1 + 1, 1, T_MID);
    else
        cv_rect(x, y, x1 - x, 1, T_MID);
}
/* from (x, y) right to xc, then up to y1 (the leader of a note to its item in the capsule) */
static void guide_leader2(int32_t x, int32_t y, int32_t xc, int32_t y1)
{
    cv_rect(x, y, xc - x + 1, 1, T_MID);
    cv_rect(xc, y1, 1, y - y1 + 1, T_MID);
}
/* one page's content into the current canvas (the caller sets cv_oy for the strip; everything is drawn at its SCREEN row) */
static void guide_content(uint32_t pg)
{
    static const t6_opt_t O1[3] = {{50, 55, 69, 84}, {111, 39, 130, 144}, {152, 39, 171, 185}};   /* SETUP 1/5: LAYER, STEREO, DUEL (FINAL_V1_08_setup) */
    static const t6_opt_t O5[2] = {{50, 39, 69, 84}, {91, 69, 110, 125}};                          /* SETUP 5/5: LATER, NOW (the same rules) */
    int32_t y;
    if (pg == 1u) {
        cv_text_on(GD_X, UT_TY(M, UT_T6_Q_BASE), &AF_M, "How do you want to start?", T_TEXT, T_BG);
        guide_option(&O1[0], "LAYER", "BOTH PARTS COMBINED:", "VELOCITY CROSSFADE, KEY SPLIT", "", "RECOMMENDED", guide_pick[0] == 0u);
        guide_option(&O1[1], "STEREO", "BOTH PARTS IN ONE STEREO FIELD", "", "", "", guide_pick[0] == 1u);
        guide_option(&O1[2], "DUEL", "DUAL MONO, FOR EXTERNAL EFFECTS", "", "", "", guide_pick[0] == 2u);
    } else if (pg == 2u) {                              /* FIRST_RUN_SETUP_V2_A.png: PART MARKER, the capsule at the right, three short notes (part number, morph end, voices), each joined to its cell by a thin line */
        const int32_t cx = 240 - 12 - CAP_W, tw = 172, my = AF_M_CAP_Y + AF_M_CAP_H / 2;
        static const char *const NOTE[3] = {"Part 1 or 2", "The morph end that you edit", "POLY: remaining voices with the current settings, or MONO, LEGATO, UNISON"};   /* (owner 2026-11-06: short notes, no full sentences) */
        static const int32_t NY[3] = {72, 108, 142};
        uint32_t i;
        /* (owner 2026-11-06: no title on this page, the header GUIDE is enough) */
        cv_oy += 50;                                    /* (head_capsule draws at the header's rows) */
        head_capsule(cx, 0, 1u, 0u, 8, 0u);
        cv_oy -= 50;
        for (i = 0; i < 3u; i++) {                      /* each leader starts 3 px after the end of its own text's last line, runs right, then up to its item in the capsule */
            int32_t xc = cx + (int32_t)i * CAP_NUM_W + (i == 0u ? CAP_NUM_W : i == 1u ? CAP_LET_W : CAP_VOI_W) / 2;
            y = guide_wrap(GD_X, NY[i], i == 2u ? tw + 24 : tw, &AF_M, NOTE[i], T_TEXT, UT_T6_G_LINE + 1);
            guide_leader2(guide_last_x + 3, y - (UT_T6_G_LINE + 1) + my, xc, 65);
        }
    } else if (pg == 3u) {                              /* outcome as a heading (L), the how-sentence under it with its keycaps in the sentence */
        y = UT_T6_G_HEAD0;
        cv_text_on(GD_X, UT_TY(L, y), &AF_L, "PERFORM MODE", T_TEXT, T_BG);
        y = guide_rich(GD_X, y + UT_T6_G_HT, GD_W, "Hold [FX] and the white keys turn into effects.") + UT_T6_G_TH;
        cv_text_on(GD_X, UT_TY(L, y), &AF_L, "QUICK EDIT", T_TEXT, T_BG);
        y = guide_rich(GD_X, y + UT_T6_G_HT, GD_W, "Keep [SEL] [ENV] [LFO] [EDIT] [GLO] [ARP] [SEQ] or [REC] pressed to show its main knobs over the page.") + UT_T6_G_TH;
        cv_text_on(GD_X, UT_TY(L, y), &AF_L, "STOP ALL", T_TEXT, T_BG);
        guide_rich(GD_X, y + UT_T6_G_HT, GD_W, "Tap [PLAY] twice.");
    } else if (pg == 4u) {
        y = UT_T6_G_HEAD0;
        cv_text_on(GD_X, UT_TY(L, y), &AF_L, "RESET A KNOB", T_TEXT, T_BG);
        y = guide_rich(GD_X, y + UT_T6_G_HT, GD_W, "Hold [OCT-] and turn the knob.") + UT_T6_G_TH;
        cv_text_on(GD_X, UT_TY(L, y), &AF_L, "SAVE OVER A PATCH", T_TEXT, T_BG);
        y = guide_rich(GD_X, y + UT_T6_G_HT, GD_W, "Keep [SAVE] pressed.") + UT_T6_G_TH;
        cv_text_on(GD_X, UT_TY(L, y), &AF_L, "PARAMETER LOCK", T_TEXT, T_BG);
        guide_rich(GD_X, y + UT_T6_G_HT, GD_W, "One step gets its own value for the knob you turn. With a note key down, press [OCT+] and turn a knob.");
    } else {                                            /* 5: SETUP 5/5 */
        cv_text_on(GD_X, UT_TY(M, UT_T6_Q_BASE), &AF_M, "Set up MIDI now?", T_TEXT, T_BG);
        guide_option(&O5[0], "LATER", "GLO > MIDI HAS IT AT ANY TIME", "", "", "DEFAULT", guide_pick[1] == 0u);
        guide_option(&O5[1], "NOW", "OPENS GLO > MIDI.", "WHEN YOU LEAVE IT,", "HOME ENDS THE FIRST START", "", guide_pick[1] == 1u);
    }
}
/* the legend row (item 118, FINAL_V1_08_setup): each key's cap (the firmware's own) and its word (S, grey) on the capitals' last row UT_T6_LEG_BASE; two items: at x UT_T6_LEG_X1 and UT_T6_LEG_X2 as the picture,
 * three: the first at X1, the last ending at UT_T6_LEG_R, the middle one between them. Three items whose words do not leave UT_T6_LEG_MIN px between them (the MENU copy's OCT+ NEXT / OCT- BACK / HOME CLOSE and
 * OCT+ FINISH: 219 / 229 px of 222) lose the LAST item's word, as cv_key_row always did; no other word is touched (HANDOFF_V2 item 118 lists it) */
static void guide_legend(const khint_t *h, uint32_t n)
{
    uint32_t i;
    int32_t x[3], w[3];
    int word2 = 1;
    uint16_t grey = ux_mix(T_BG, T_TEXT, UT_GREY_PCT);
    for (;;) {
        for (i = 0; i < n; i++)
            w[i] = kc_w(h[i].key) + (h[i].act[0] && (i + 1u < n || word2) ? UT_T6_LEG_GAP + text_w(&AF_S, h[i].act) : 0);
        x[0] = UT_T6_LEG_X1;
        if (n == 2u) {
            x[1] = UT_T6_LEG_X2;
            break;
        }
        x[2] = UT_T6_LEG_R - w[2];
        x[1] = x[0] + w[0] + (x[2] - x[0] - w[0] - w[1]) / 2;
        if (!word2 || (x[1] - x[0] - w[0] >= UT_T6_LEG_MIN && x[2] - x[1] - w[1] >= UT_T6_LEG_MIN))
            break;
        word2 = 0;
    }
    for (i = 0; i < n; i++) {
        int32_t e = cv_keycap(x[i], UT_T6_LEG_Y, h[i].key, T_KEY, T_KINK, T_BG);
        if (h[i].act[0] && (i + 1u < n || word2))
            cv_text_on(e + UT_T6_LEG_GAP, UT_TY(S, UT_T6_LEG_BASE), &AF_S, h[i].act, grey, T_BG);
    }
}
#else
static void guide_content(uint32_t pg) { (void)pg; }
static void guide_legend(const khint_t *h, uint32_t n) { (void)h; (void)n; }
#endif
/* the page (ui_draw_page): the content when the page or a pick changes (or ui.force), the header every frame (its own signature: a message) */
static void guide_draw(void)
{
    static const khint_t H1[2] = {{KC_OCTUP, "NEXT"}, {KC_HOME, "SKIP"}};
    static const khint_t HL[2] = {{KC_OCTUP, "FINISH"}, {KC_HOME, "SKIP"}};
    static const khint_t HN[3] = {{KC_OCTUP, "NEXT"}, {KC_OCTDN, "BACK"}, {KC_HOME, "SKIP"}};
    static const khint_t HM[3] = {{KC_OCTUP, "NEXT"}, {KC_OCTDN, "BACK"}, {KC_HOME, "CLOSE"}};
    static const khint_t HMF[3] = {{KC_OCTUP, "FINISH"}, {KC_OCTDN, "BACK"}, {KC_HOME, "CLOSE"}};
    uint32_t pg = (ui.guide - 1u) % GUIDE_N + 1u, sig = pg * 64u + ui.guide_menu * 32u + guide_pick[0] * 4u + guide_pick[1];
    if (ui.force || guide_sig != sig + 1u) {
        static const int32_t EDGE[3] = {H_HEAD, H_HEAD + 124, GD_HINT_Y};
        uint32_t band;
        const khint_t *kh = ui.guide_menu ? (pg == 4u ? HMF : HM) : pg == 1u ? H1 : pg == 5u ? HL : HN;   /* (SETUP questions: only OCT + NEXT and HOME SKIP) */
        guide_sig = sig + 1u;
        guide_kh = kh;
        for (band = 0; band < 2u; band++) {             /* two strips: the canvas holds 124 rows (the legend row is in the last one) */
            int32_t y0 = EDGE[band];
            CV_BANDS(240, (uint32_t)(EDGE[band + 1] - y0), T_BG) {                 /* (in bands: gfx.c CV_BANDS) */
                cv_oy = -y0;
                guide_content(pg);
                if (band == 1u)
                    guide_legend(kh, guide_ask(pg) && !ui.guide_menu ? 2u : 3u);
                cv_oy = 0;
                cv_blit(0, (uint32_t)y0 + (uint32_t)cv_by);
            }
        }
    }
    draw_head();                                        /* SETUP or GUIDE (ui_draw.c head_page_title), or a message (STOP ALL) */
    draw_foot();                                        /* item 118: the shared footer, the counter 1/5 (MENU's copy 1/3) in it */
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
        ui.msg2[0] = 0;
        ui.msg_t = 60;
    }
    ui.force = 0;
}
