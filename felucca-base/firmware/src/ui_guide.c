/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.11: the first-start GUIDE (SPEC "First-start guide"; docs/GUIDE_PLAN.md, the owner's pages). Included at the end of ui_layer.c.
 *   WHEN       once, after the power-on splash, while the settings bit "guide seen" (ui.c PREF_GUIDE_SEEN, bit 5 of [15][24]) is clear: a
 *              new unit, a flash erase, and any settings record of an older build (the bit was never written before), so an update
 *              shows it once too. The splash then stays GUIDE_SPLASH_MS longer (ui_draw.c splash_draw). Leaving it, or finishing it,
 *              sets the bit and writes the settings once (settings_save: the write waits for silence, as every settings write since 0.2.11: project.c settings_poll).
 *              Power off with the guide up: the bit stays clear, so the next start shows it again.
 *              MENU > SYSTEM > GUIDE opens the same pages at any time; that never touches the bit.
 *   PAGES      GUIDE_N full-screen pages over whatever page is under them: the header (its title "GUIDE 2/6", the part badges and
 *              the battery as on every page), the text, the key hints. A page's text: "#" a heading (L, the theme colour), "~" a
 *              small gap, "*" a line whose words up to its first ':' are in the theme colour (what it is, then how), else a line
 *              (M, the text colour); each line at most GD_W px (the layout lint, tests/ui_render.c, checks
 *              every page in every palette).
 *   KEYS       SELECT right / OCT+ the next page, SELECT left / OCT- the previous one (SELECT stops at both ends, OCT+ on the last
 *              page finishes); HOME leaves at once (from the MENU it closes the menu, as HOME does there); EDIT FX ENV LFO ARP
 *              SCL GLO SEQ SAVE leave at once and go on to their own job, in the same press (as they leave the MENU); PLAY/STOP and
 *              REC (the HOLD switch) work as on HOME; the keys and MIDI play as ever. KNOB 1-4, PRESETS and ALGORITHM do nothing (the header
 *              says "[HOME] CLOSES GUIDE" for a moment, so no turn is silently ignored). */
#define GD_X 10                                         /* the text's left edge */
#define GD_W 220                                        /* .. its widest line (to x 230) */
#define GD_TOP 28                                       /* the first row under the header */
#define GD_HINT_Y 214                                   /* the key hints' strip (rows 214..239), its keycaps on row MK_Y */
#define GD_H_HEAD 34                                    /* a heading's strip (L: capitals on rows 4..24 of it) */
#define GD_H_LINE 18                                    /* a line's strip (M: capitals on rows 4..14) */
#define GD_H_GAP 4
static const char *const GUIDE_TXT[GUIDE_N] = {
    "#MODES\n*LAYER: BOTH PARTS\nCOMBINED: BALANCE,\nVELOCITY CROSSFADE,\nKEY SPLIT\n~\n*DUEL: DUAL MONO, FOR\nEXTERNAL EFFECTS\n~\n"
        "*STEREO: BOTH PARTS IN\nONE STEREO FIELD",
    "#MORPH A<>B\nGLIDE BETWEEN TWO\nVERSIONS OF THE SAME PATCH\n~\nA SMART RANDOMIZER\nCREATES NEW VARIANTS\nFROM YOUR STARTING POINT",
    "#LONG PRESS\nHOLD FX: PERFORM\n~\nHOLD FOR QUICK EDIT:\nEDIT, LFO, ENV, ARP, GLO",
    "#REC = HOLD\nKEYS KEEP SOUNDING\nAFTER YOU LET GO.\nPRESS REC AGAIN\nTO RELEASE.\n~\nNOT ON THE SEQUENCER\nPAGES",
    "#RESET\nHOLD OCT- AND TURN\nA KNOB TO RESET IT\nTO ITS DEFAULT",
    "#STOP + SAVE\n*STOP ALL: PLAY/STOP 2X\n~\n*SAVE OVER PATCH: HOLD SAVE",
};
#define GUIDE_KNOB_MSG "[HOME] CLOSES GUIDE"           /* a knob turned while it is up (it does nothing there) */
static uint32_t guide_sig;                              /* the page drawn last (+ 1; 0: draw it) */

static int guide_seen(void) { return (ui_prefs2 & (PREF_GUIDE_SEEN >> 16)) != 0u; }
/* opened: by the splash's end (from_menu 0: the first start) or MENU > SYSTEM > GUIDE (1) */
static void guide_open(uint32_t from_menu)
{
    ui.guide = 1;
    ui.guide_menu = (uint8_t)(from_menu != 0u);
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
static void guide_page(int32_t d)                      /* the next / previous page, stopping at both ends */
{
    ui.guide = (uint8_t)clamp((int32_t)ui.guide + d, 1, (int32_t)GUIDE_N);
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
        if (ui.guide >= GUIDE_N) {                      /* OCT+ on the last page: done */
            guide_close();
            enc_drop();
            return 1;
        }
        guide_page(1);
    }
    if ((pressed >> panel.btn[B_OCTDN]) & 1u)
        guide_page(-1);
    if ((s = panel_enc(EN_SELECT)) != 0)
        guide_page(clamp(s, -(int32_t)GUIDE_N, (int32_t)GUIDE_N));
    {   /* KNOB 1-4, PRESETS, ALGORITHM: nothing changes, and the header says why (no silently ignored control) */
        uint32_t k, any = 0;
        for (k = 0; k < NE; k++)
            any |= k != EN_SELECT && panel_enc(k) != 0;
        if (any)
            ui_message(GUIDE_KNOB_MSG);
    }
    return 1;
}

/* the page (ui_draw_page): the text strips when the page changes (or ui.force), the header every frame (its own signature: a message) */
static void guide_draw(void)
{
    static const khint_t HINT[2][2] = {{{KC_SELECT, "PAGE"}, {KC_HOME, "CLOSE"}}, {{KC_SELECT, "PAGE"}, {KC_HOME, "DONE"}}};
    uint32_t pg = (ui.guide - 1u) % GUIDE_N;
    if (ui.force || guide_sig != pg + 1u) {
        const char *s = GUIDE_TXT[pg];
        int32_t y = GD_TOP;
        guide_sig = pg + 1u;
        lcd_fill(0, H_HEAD, 240, GD_TOP - H_HEAD, T_BG);
        while (*s) {
            char ln[32];
            uint32_t n = 0;
            int head = *s == '#', gap = *s == '~', key = *s == '*';
            s += head || gap || key;
            while (*s && *s != '\n' && n + 1u < sizeof ln)
                ln[n++] = *s++;
            ln[n] = 0;
            if (*s == '\n')
                s++;
            if (gap) {
                lcd_fill(0, (uint32_t)y, 240, GD_H_GAP, T_BG);
                y += GD_H_GAP;
                continue;
            }
            cv_begin(240, head ? GD_H_HEAD : GD_H_LINE, T_BG);
            if (head) {
                cv_text_fit(GD_X, -3, &AF_L, ln, vt(T_THEME, T_BG), T_BG, GD_W);
            } else if (key) {                           /* "STOP ALL:" in the theme colour, " PLAY/STOP 2X" after it */
                uint32_t c = 0;
                int32_t x;
                while (ln[c] && ln[c] != ':')
                    c++;
                c += ln[c] == ':';
                {
                    char hd[32];
                    str_cpy(hd, ln, c + 1u < sizeof hd ? c + 1u : sizeof hd);
                    x = cv_text_fit(GD_X, 0, &AF_M, hd, vt(T_THEME, T_BG), T_BG, GD_W);
                }
                if (ln[c])
                    cv_text_fit(x, 0, &AF_M, ln + c, T_TEXT, T_BG, GD_X + GD_W - x);
            } else
                cv_text_fit(GD_X, 0, &AF_M, ln, T_TEXT, T_BG, GD_W);
            cv_blit(0, (uint32_t)y);
            y += head ? GD_H_HEAD : GD_H_LINE;
        }
        if (y < GD_HINT_Y)
            lcd_fill(0, (uint32_t)y, 240, (uint32_t)(GD_HINT_Y - y), T_BG);
        cv_begin(240, 240 - GD_HINT_Y, T_BG);
        cv_key_row(8, 232, MK_Y - GD_HINT_Y, HINT[pg + 1u == GUIDE_N], 2u, 3u, T_BG);
        cv_blit(0, GD_HINT_Y);
    }
    draw_head();                                        /* GUIDE n/N (ui_draw.c head_page_title), or a message (HOLD ON, STOP ALL) */
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
        ui.msg2[0] = 0;
        ui.msg_t = 60;
    }
    ui.force = 0;
}
