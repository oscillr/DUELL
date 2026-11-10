/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI drawing: the header, the four knob cards, the footer (steps + engine / preset / page)
 * and the frame; the panel between the cards and the footer is ui_graph.c. The layout:
 * flat SURF cards and panels on BG,
 * rounded corners, no rules (FLAT is the only look since DUELL 0.2.2), colours from the theme tokens only (gfx.c T_*).
 * Type, by role: M (15 px) what is read: values, titles, messages, every list's rows, the
 * selected item's detail; S (12 px) what describes it: labels, units, key hints (a keycap's word), the footer, map
 * and layer cells, graph annotations, document text; L (28 px) the one big readout of a screen. Where M cannot fit
 * (a card value with its unit wider than the card, a dialog question or a header message too long) it is set in S;
 * free text (names) is ellipsised at its size. */
#include "ui_tokens.h"                                /* DUELL 1.1 item 111: the shared header / footer / battery numbers, one set */
static void draw_menu(void);
static int name_on(void);                              /* NAME (ui_name.c) */
static int32_t morph_pending(uint32_t g);              /* the twists building up: MORPH's ROLL, UNDO, GLO's MUTE 1, MUTE 2 (ui_input.c, 4b) */
static void name_draw(void);
static int guide_seen(void);                          /* DUELL 0.2.11: the first-start GUIDE (ui_guide.c, included with ui_layer.c) */
static void guide_open(uint32_t from_menu);
static void guide_draw(void);
static int32_t guide_wrap(int32_t x, int32_t y, int32_t w, const aafont_t *f, const char *s, uint16_t fg, int32_t p);
#if DUELL_TEST_MODE
static uint32_t test_slot(void);                       /* MENU > SYSTEM > TEST: the multi K3 shows (ui_menu.c) */
#endif

/* --------------------------------------------------------- drawing --- */
#define COL_W CARD_W                                  /* a card: 57 x 44 at x 3 + 59 c, y 28 */
#define COL_H CARD_H

static int32_t batt_level(void)                         /* thresholds 531 / 561 / 591 on ADC ch3 */
{
    return song.batt_raw >= 591 ? 3 : song.batt_raw >= 561 ? 2 : song.batt_raw >= 531 ? 1 : 0;
}
/* A USB host powers us; there is no separate charger status line.
 * 4 means external power (a steady bolt inside the battery), otherwise its level. */
static int32_t batt_shown(void)
{
    if (usb.config && !usb.suspended)
        return 4;
    return batt_level();
}

/* The armed-for-recording mark (DUELL 0.2.10, owner: not in the header): a red dot and "REC" in the footer's row, just left of the page's number, on every page while any part is armed
 * (song.rec). DUELL 1.1 item 111 (FINAL_V1_01_morph.png): the word in face F at the counter's pen - UT_FOOT_REC_GAP - its width, the 12 px dot (UT_REC_DOT) 15 px left of the word's pen;
 * filled: the selected part is armed; a ring: another part only. Armed and disarmed only on the sequencer's pages (REC elsewhere is the HOLD switch).
 * foot_rec_dot returns the dot's left edge (0: not armed); right16: the pen 1/16 px the mark's word may end before (the counter's, or UT_FOOT_CT_R) */
#define FT_GAP UT_FOOT_WORD_GAP                        /* the patch name -> the number */
static int32_t foot_rec_dot(int32_t right16)
{
    if (!song.rec)
        return 0;
    return (right16 - UT_FOOT_REC_GAP * 16 - text_w16(&AF_S, "REC") - UT_REC_DOT_DX * 16) >> 4;
}
static void foot_rec_mark(int32_t right16)
{
    int32_t rx16, dx, dy = UT_FOOT_BASE - UT_REC_DOT_DY - Y_FOOT;
    if (!song.rec)
        return;
    rx16 = right16 - UT_FOOT_REC_GAP * 16 - text_w16(&AF_S, "REC");
    dx = foot_rec_dot(right16);
    GFX_HOOK_ALIGN(0, dy, 0, dy + UT_REC_DOT, AL_V, "footer REC mark on the names' line");
    cv_text_x16(rx16, UT_FOOT_BASE - Y_FOOT - AF_S_CAP_Y - AF_S_CAP_H + 1, &AF_S, "REC", T_REC, T_BG);
    cv_rrect(dx, dy, UT_REC_DOT, UT_REC_DOT, UT_REC_DOT / 2, T_REC, T_BG);
    if (!((song.rec >> song.sel) & 1u))                /* (another part only: a ring) */
        cv_rrect(dx + 2, dy + 2, UT_REC_DOT - 4, UT_REC_DOT - 4, UT_REC_DOT / 2 - 2, T_BG, T_REC);
}

/* the battery level: 0 empty .. 3 full, 4 = USB power (charging). The stock thresholds give 0..3 of 3 */
/* THE battery (DUELL 1.1 item 111; FINAL_V1_10_battery.png, bottom right of every page; ui_tokens.h UT_BAT_*): a 15 x 9 outline with its 2 x 4 nub at x 222 .. 236, rows 222 .. 230, in T_MID.
 * Empty: the outline only. Full: the 9 x 5 block filled (levels 1 and 2: 3 and 6 px of it; one bar left in the accent). Charging (USB power): the EMPTY outline with a 5 x 7 bolt, no fill.
 * The two bitmaps are the pictures' pixels as 4-bit coverage (15 x 9, rows back to back; the corners 10 / 15 are anti-aliased), drawn through the text ramp */
static const uint8_t BAT_OUT[68] = {175, 255, 255, 255, 255, 255, 160, 15, 0, 0, 0, 0, 0, 15, 0, 240, 0, 0, 0, 0, 0, 240, 15, 0, 0, 0, 0, 0, 15, 204, 240, 0, 0, 0, 0, 0, 255, 255, 0, 0, 0, 0, 0, 15, 255, 240, 0, 0, 0, 0, 0, 252, 207, 0, 0, 0, 0, 0, 15, 0, 175, 255, 255, 255, 255, 255, 160, 0};
static const uint8_t BAT_BOLT[68] = {175, 255, 255, 255, 255, 255, 160, 15, 0, 0, 0, 255, 0, 15, 0, 240, 0, 0, 255, 0, 0, 240, 15, 0, 0, 255, 0, 0, 15, 204, 240, 0, 255, 255, 240, 0, 255, 255, 0, 0, 15, 240, 0, 15, 255, 240, 0, 15, 240, 0, 0, 252, 207, 0, 15, 240, 0, 0, 15, 0, 175, 255, 255, 255, 255, 255, 160, 0};
static void draw_battery(void)
{
    int32_t lvl = batt_shown(), x = UT_BAT_X, y = UT_BAT_Y - Y_FOOT;
    uint16_t rv[16];
    ramp_lin(rv, T_MID, T_BG);
    if (lvl >= 4) {                                     /* charging: the empty outline and the bolt */
        cv_alpha(x, y, UT_BAT_W, UT_BAT_H, BAT_BOLT, rv);
        return;
    }
    cv_alpha(x, y, UT_BAT_W, UT_BAT_H, BAT_OUT, rv);
    if (lvl >= 1)                                       /* the block: 1 / 3, 2 / 3, all of it */
        cv_rect(x + UT_BAT_FX, y + UT_BAT_FY, lvl >= 3 ? UT_BAT_FW : lvl * (UT_BAT_FW / 3), UT_BAT_FH, lvl == 1 ? T_ACCENT : T_MID);
}

/* DUELL 0.2.9: the CPU bar, a 5 px gauge of the audio load under the battery (MENU > DISPLAY > CPU METER, ON by default). The
 * audio ISR already counts every I2S half's render time (core.h cpu_sum / cpu_n / cpu_miss, always running); cpu_bar_poll
 * reads them once a second from draw_head (no timer, no ISR code added), so the bar is the load of the last ~1 s. 8 steps
 * (any load above 0 shows at least one); the header redraws when the step or colour changes only. Colours as the TEST card's
 * (ui_menu.c cpu_col: the theme colour, amber, REC red; MONO: the text colour, the length tells), from 70 % amber, from 85 %
 * red. A half that missed its time in the last second: the whole bar in REC red for the next second. */
#define HEAD_CPU_X 215                                 /* the bar's rectangle: x 215..236, y 19..23 (5 px, under the battery's ink, to the header's last row) */
#define HEAD_CPU_Y 33                                  /* (the footer canvas: under the battery) */
#define HEAD_CPU_W 22
#define HEAD_CPU_H 5
#define CPU_BAR_WIN_MS 1000u
#define CPU_BAR_STALE_MS 4000u                         /* a longer gap (no header drawn meanwhile): start a fresh window, no flash */
#define CPU_BAR_WARN 70u
#define CPU_BAR_HOT 85u
#define CPU_BAR_STEPS 8u
static uint16_t cpu_col(uint32_t lvl, uint16_t bg);   /* (ui_menu.c) */
static struct { uint32_t t, n0, sum0, miss0; uint8_t ok, step, lvl; } cpub;   /* lvl 0 theme, 1 amber, 2 red */
static void cpu_bar_set(uint32_t pct, uint32_t flash) /* the load in % of the last window, and a missed half in it */
{
    cpub.step = (uint8_t)(flash || pct >= 100u ? CPU_BAR_STEPS : (pct * CPU_BAR_STEPS + 99u) / 100u);
    cpub.lvl = (uint8_t)(flash || pct >= CPU_BAR_HOT ? 2u : pct >= CPU_BAR_WARN ? 1u : 0u);
}
static void cpu_bar_poll(void)
{
    uint32_t dt = fm1_ms - cpub.t, n, sum, miss;
    if (cpub.ok && dt < CPU_BAR_WIN_MS)
        return;
    do {
        n = cpu_n;
        sum = cpu_sum;
        miss = cpu_miss;
    } while (n != cpu_n);
    if (cpub.ok && dt < CPU_BAR_STALE_MS && n != cpub.n0)
        cpu_bar_set((sum - cpub.sum0) * 100u / ((n - cpub.n0) * CPU_BUDGET_US), miss != cpub.miss0);
    cpub.t = fm1_ms;
    cpub.n0 = n;
    cpub.sum0 = sum;
    cpub.miss0 = miss;
    cpub.ok = 1;
}

/* ---------------------------------------------------- rolling digits --- */
/* A number that changes rolls its changed digits like a slot machine; only the drawing: the value, the gauge,
 * the hot colour and the sound change at once. Up: the old digit leaves upward and the new one comes from below;
 * down: the reverse. ROLL_FRAMES UI frames (ui.frame, ~135 ms) on an integer ease-out (ROLL_EASE: cubic, over
 * half-way in a quarter of the time, no overshoot); the ones digit first, each place one frame later, all ending
 * together on a frame equal to the static render. Clipped to the value strip, which alone is redrawn while it
 * rolls (a card: rows ROLL_Y.., the header: the BPM's columns). Characters other than digits never roll.
 * A change during a roll retargets it: it restarts from the value it was going to. Shown at once (no roll):
 * a change within ROLL_SNAP frames of the last one (a fast turn reads better as plain numbers), another
 * shape (length, sign, a non-digit, a name or an enum, a value set in S), another label or unit, a track /
 * engine / palette change (ui.roll[].sig), ui.force (a page change), a card's first draw; MENU > ANIM OFF (#46). */
#define ROLL_FRAMES 9u
#define ROLL_SNAP 3u                                    /* frames (~45 ms) */
#define ROLL_BPM 4u                                     /* ui.roll[]: the four cards, then the header BPM */
#define ROLL_Y 18                                       /* a card's value strip: rows 18..35, its figures (21..31) in the middle, */
#define ROLL_H 18                                       /* the label (MONO's chip: to row 17) above it, the gauge (38) below (item 120: 17 -> 18 rows, so that a unit in S 15 (rows 24..35) lies inside it) */
static const uint8_t ROLL_EASE[17] = {0, 45, 84, 118, 147, 172, 193, 210, 223, 234, 242, 247, 251, 253, 254, 255, 255};

static int roll_digit(char c) { return c >= '0' && c <= '9'; }
/* +1 / -1: b rolls in from a (same length, digits where a has digits, the rest equal and one of + - .),
 * the sign of b - a; 0: b is shown at once */
static int roll_dir(const char *a, const char *b)
{
    uint32_t i;
    int d = 0, neg = 0;
    for (i = 0; a[i] || b[i]; i++) {
        if (i + 1u >= sizeof ui.roll[0].from || roll_digit(a[i]) != roll_digit(b[i]))
            return 0;
        if (!roll_digit(a[i]) && (a[i] != b[i] || (a[i] != '-' && a[i] != '+' && a[i] != '.')))
            return 0;
        neg |= a[i] == '-';
        if (!d && a[i] != b[i])
            d = b[i] > a[i] ? 1 : -1;
    }
    return neg ? -d : d;
}
/* field k now shows b instead of a: roll (a retarget mid-roll), or snap */
static void roll_note(uint32_t k, const char *a, const char *b, int snap)
{
    uint32_t el = (uint8_t)(ui.frame - ui.roll[k].t0);
    int d = roll_dir(a, b);
    ui.roll[k].t0 = (uint8_t)ui.frame;
    ui.roll[k].from[0] = 0;
    if (snap || !d || el < ROLL_SNAP || (ui_prefs & PREF_ANIM_OFF))
        return;
    str_cpy(ui.roll[k].from, a, sizeof ui.roll[k].from);
    ui.roll[k].dir = (int8_t)d;
}
/* s (M) at x, y with field k's roll: each character at its pen in s (the digits are tabular and never kerned,
 * so the old value has the same pens); a rolling digit has moved o of the strip's h rows (ROLL_EASE, one frame
 * later per place from the right), clipped to the strip. The roll's last frame clears it: the static text. */
static int32_t roll_text(uint32_t k, int32_t x, int32_t y, const char *s, uint16_t fg)
{
    const aafont_t *f = &AF_M;
    uint32_t e = (uint8_t)(ui.frame - ui.roll[k].t0) + 1u, i, p = 0;
    const char *a = ui.roll[k].from;
    int32_t cy = k < ROLL_BPM ? ROLL_Y : 0, h = k < ROLL_BPM ? ROLL_H : H_HEAD;
    uint16_t bg = T_BG;
    char t[8], ch[2] = {0, 0};
    if (e >= ROLL_FRAMES)
        ui.roll[k].from[0] = 0;
    if (!a[0])
        return cv_text_on(x, y, f, s, fg, bg);
    str_cpy(t, s, sizeof t);
    for (i = 0; s[i]; i++)
        p += (uint32_t)roll_digit(s[i]);
    cv_cy0 = (int16_t)(cy + CV_OY);                     /* the strip (the static characters lie inside it) */
    cv_cy1 = (int16_t)(cy + CV_OY + h);
    cv_scroll = 1;                                      /* (the lint: rolling digits are cut on purpose) */
    for (i = 0; s[i]; i++) {
        int32_t px, ny = y;
        t[i] = 0;
        px = x + text_w(f, t);                          /* (its pen: no digit kerns) */
        t[i] = s[i];
        p -= (uint32_t)roll_digit(s[i]);
        if (s[i] != a[i]) {                             /* p: the places right of it */
            int32_t o = 0, dir = ui.roll[k].dir;
            if (e > p)                                  /* (e < ROLL_FRAMES: the LUT index stays in 1..16) */
                o = (int32_t)((ROLL_EASE[((e - p) * 32u / (ROLL_FRAMES - p) + 1u) >> 1] * (uint32_t)h + 128u) >> 8);
            ch[0] = a[i];
            cv_text_on(px, y - dir * o, f, ch, fg, bg);
            ny = y + dir * (h - o);
        }
        ch[0] = s[i];
        cv_text_on(px, ny, f, ch, fg, bg);
    }
    cv_cy0 = 0;
    cv_cy1 = (int16_t)cv_h;
    cv_scroll = 0;
    return x + text_w(f, s);
}

/* DUELL 1.1 OCTAVE page (PAGES_V1_octave.png, approved): after OCT - or OCT + moved the octave (ui_input.c oct_rel) a full page shows it for 1.5 s: the big figure and the seven positions */
static uint32_t oct_show_t0;                           /* fm1_ms of the last octave change (0: none yet) */
static uint32_t oct_press_t0;                          /* fm1_ms when OCT - or OCT + went down on a normal page (ui_input.c oct_rel) */
static uint8_t oct_pend, oct_mod;                      /* (ui_input.c: a button is down / OCT - was used as a modifier) */
static int oct_show_on(void)
{
    return oct_show_t0 && (uint32_t)(fm1_ms - oct_show_t0) < 1500u && !ui.menu && !ui.guide && !ui.uboot && !ui.confirm && !name_on() && !seq_counting();
}
/* the header (y 0..24), three calm slots (0.2.1):
 *   left    the part badges (the selected track's cushion, ink 4..20); nothing after them: no multi number, no REC mark (DUELL 0.2.10, owner)
 *   centre  the PAGE NAME (head_page_title; a layer's label, a message, a dialog's or NAME's title in its place), M, centred on x 120 within
 *           the room between the badges and the battery (S when M does not fit; then the octave gives way). No
 *           transport here: the tempo lives on the TEMPO page, the running state on PLAY's LED (ui_input.c: green while it runs)
 *   right   the octave (ending at 208) and the battery (ink 216..236; its bolt says USB power: no separate USB icon)
 * A message or a layer's label replaces the centre and the octave, centred on x 120, up to 150 px (0.2.9: a layer's label is "QUICK <PAGE>"
 * after a 12 px bolt, ICON_X_QUICK, in the accent: head_msg). Every element's ink centred on row 12
 * (H_HEAD / 2; ui_test.c test_head_centres): icons by cv_icon_mid, text by its capitals' band (CAP_IN; the odd row left over above it, as
 * everywhere): M (the page name, the octave, a message: capitals rows 6..16) from y 2, S (OCT, a key hint: rows 7..15) from y 4 */
#define HEAD_SY CAP_IN(S, H_HEAD)
#define HEAD_MY (CAP_IN(M, H_HEAD) - 2)             /* (PALETTES_V3_1.png: the title's capitals on rows 4 .. 14) */
#define HEAD_TRK_X 3                                   /* the part badge: 1 2 side by side, 14 px cushions 15 apart (cv_part) */
#define HEAD_LEFT_R 32                                 /* the part badges end here (ink 31): the page name starts 4 px on at the earliest */
#define HEAD_BAT_X 214                                 /* the battery's cell */
#define HEAD_GRP_R 208                                 /* the octave / song row ends here */
#define HEAD_MSG_W 150                                 /* a message centred on x 120: 45..195 (after the part badges) */
#define HEAD_ROOM_R 212                                 /* the page name ends here at the latest (the battery's ink starts at 216) */
/* the width cv_free_hint(x, .., s, .., maxw) draws (its word, keycap and the ellipsised rest) */
static int32_t head_hint_w(const char *s, int32_t maxw)
{
    uint32_t n, i;
    int32_t id = kc_tag(s, &n), w = 0;
    char b[48];
    for (i = 0; id < 0 && i < 7u && s[i] && s[i] != ' '; i++)
        ;
    if (id < 0 && i && s[i] == ' ' && (id = kc_tag(s + i + 1u, &n)) >= 0) {
        char wd[8] = {0};
        uint32_t j;
        for (j = 0; j < i; j++)
            wd[j] = s[j];
        w = text_w(&AF_S, wd) + KH_GAP;
        s += i + 1u;
    }
    if (id >= 0 && kc_w((uint32_t)id) + KH_GAP < maxw - w) {
        w += kc_w((uint32_t)id) + KH_GAP;
        s += n;
        while (*s == ' ')
            s++;
        if (!*s)
            return w - KH_GAP;
    }
    text_fit(b, sizeof b, s, &AF_S, maxw - w);
    return w + text_w(&AF_S, b);
}
/* a key hint: a keycap first, or after the first word ("[SAVE] HOLD TO UNDO", "HOLD [GLO] QUICK"; cv_free_hint) */
static int head_is_hint(const char *s)
{
    uint32_t n, i;
    if (kc_tag(s, &n) >= 0)
        return 1;
    for (i = 0; i < 7u && s[i] && s[i] != ' '; i++)
        ;
    return i && s[i] == ' ' && kc_tag(s + i + 1u, &n) >= 0;
}
/* a message / a layer's label at x, or centred on x 120 (x < 0), at most maxw px with its
 * icon (ui.c MSG_NOFILE: the accent's) and a locked layer's lock. A message in M (as the panel's titles), a key hint
 * (a layer's label, "[SAVE] HOLD TO UNDO") in S as every keycap's word; a message M cannot hold in maxw (right of the
 * BPM; a long name) in S, ellipsised there if it must be */
static void head_msg(int32_t x, int32_t maxw)
{
    const char *m = ui.msg_t ? ui.msg : layer_head();
    int lock = !ui.msg_t && layer_locked();   /* #83: locked open (a double tap): the lock after its name */
    int icon = m[0] == MSG_NOFILE[0], quick = !ui.msg_t && ui.layer;   /* (0.2.9: a layer's label is "QUICK <PAGE>": the bolt before it in */
    int32_t qb[4] = {0, 0, 0, 0};                       /* the accent, the words in the accent where it reads (vt, 3:1; else the text's) */
    uint16_t ink = T_TEXT;                              /* (item 119: a title is never coloured, a quick layer's label too) */
    int32_t lw = lock ? 15 : 0, iw = icon ? 16 + KH_GAP : 0;   /* the lock: its cell 6 px on, its ink to 15 */
    int big;
    (void)qb;                                           /* (DUELL 1.1: a title never has an icon: no bolt before QUICK ...) */
    big = !head_is_hint(m + icon) && text_w(&AF_M, m + icon) <= maxw - lw - iw;
    if (x < 0)
        x = 120 - HALF_UP(iw + (big ? text_w(&AF_M, m + icon) : head_hint_w(m + icon, maxw - lw - iw)) + lw);
    if (icon)
        (void)x;                                       /* (DUELL 1.1: no icon before a message) */

    if (!big && !icon && !lock && !head_is_hint(m) && text_w(&AF_S, m) > maxw) {   /* too long even for S: two lines of S, split at a space, the whole text (never "...") */
        char l1[40], l2[40];
        uint32_t n = str_len(m), cut = 0, i;
        for (i = 1; i < n && i + 1u < sizeof l1; i++) {
            char t[40];
            uint32_t j;
            for (j = 0; j < i; j++)
                t[j] = m[j];
            t[i] = 0;
            if (text_w(&AF_S, t) > maxw)
                break;
            if (m[i] == ' ')
                cut = i;
        }
        if (!cut)
            cut = i - 1u;
        for (i = 0; i < cut; i++)
            l1[i] = m[i];
        l1[cut] = 0;
        str_cpy(l2, m + cut + (m[cut] == ' ' ? 1u : 0u), sizeof l2);
        {
            int32_t cx = 120 - HALF_UP(text_w(&AF_S, l1)), c2 = 120 - HALF_UP(text_w(&AF_S, l2));
            GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V | AL_N(2), "header message in two lines");
            cv_text(cx, -4, &AF_S, l1, ink);               /* (item 119: S 15 has capitals of 11 rows: line 1 on rows 0 .. 10, line 2 on 12 .. 22 of the 24 px header) */
            cv_free_text(c2, 8, &AF_S, l2, ink, T_BG, maxw);
        }
        return;
    }
    if (big) {
        GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
        x = cv_text(x, HEAD_MY, &AF_M, m + icon, ink);
    } else if (quick) {                                 /* (too wide for M: S, still the accent) */
        x = cv_free_text(x, HEAD_SY, &AF_S, m, ink, T_BG, maxw - lw - iw);
    } else {
        x = cv_free_hint(x, HEAD_SY, m + icon, T_TEXT, T_BG, maxw - lw - iw);   /* (may start with a keycap) */
    }
    if (lock)
        cv_icon_mid(x + 6, H_HEAD / 2, 16, ICON_X_LOCK, T_THEME, T_BG);
}
static void nm_title(char *b);                         /* ui_name.c: NAME's title ("SAVE U07") */
/* the header's centre text (0.2.1): what the screen is called. Over a page, the PAGE NAME (params.c titles; EDIT's: the engine's; the piano roll
 * "STEP C MIN", the GRID "GRID"; OVERVIEW); the full-screen dialogs and NAME their own title; a layer shows its label as a message
 * (head_msg). tb holds 24; *pi gets the page's icon (MIXER, PATTERNS, PARAMETER LOCKS) or ICON_NONE */
static void head_page_title(char *tb, uint32_t *pi)
{
    char nm[28], ct[8];
    *pi = ICON_NONE;
    if (oct_show_on()) {
        str_cpy(tb, "OCTAVE", 24);
    } else if (ui.uboot) {
        str_cpy(tb, "UPDATE MODE", 24);
    } else if (ui.guide) {                              /* DUELL 1.1 (item 118): SETUP on the questions (1, 5), GUIDE on the info pages; the counter is the footer's (draw_foot: 1/5 .. 5/5, the MENU's copy 1/3 .. 3/3), no longer in the title (ui_guide.c) */
        uint32_t pg = ui.guide, ask = pg == 1u || pg == 5u;
        str_cpy(tb, ask && !ui.guide_menu ? "SETUP" : "GUIDE", 24);
    } else if (seq_counting() && !ui.confirm && !name_on()) {
        str_cpy(tb, "COUNT-IN", 24);
    } else if (ui.confirm) {
        str_cpy(tb, "CONFIRM", 24);
    } else if (name_on()) {
        nm_title(tb);
    } else {
        int roll = page_parts(nm, ct);
        str_cpy(tb, roll ? "SEQUENCER" : nm, 40);            /* (item 118, FINAL_V1_07: the piano roll's title is STEP, no scale name; the name is the SCALE page's) */
        *pi = page_icon(cur_page());
    }
}
/* EDITABLE B (cloud job G, docs/design/EDITABLE_B_DESIGN.md 2.2): the letter A or B in a filled cushion in the part's A or B colour, right of the 1 2 badges (x 33, 14 px like them; where the multi number was
 * before 0.2.10), on the sound pages while the selected part has a live morph (an End B that differs from A; shown always, so B is seen before the first turn). B when the part's fader is at 127: the end
 * a knob edits. Not on MORPH, MORPH SET, PERFORM, MODE, the global pages (GLO, SCALE) or the SAVE / MENU / dialog screens. The cushion is drawn by cv_trk_r as the badges are (its digit is the letter's
 * glyph, '1' + 16 = 'A'); it takes 15 px of the page title's room (head_page_title drops to the small font when the name no longer fits, as it does for the octave) */
#define HEAD_AB_X (HEAD_TRK_X + (int32_t)NSHOWN * 15)
#define HEAD_AB_W 15                                   /* its cell with the gap */
static int head_ab_on(void)
{
    const page_t *pg = cur_page();
#ifdef DUELL_TEST_NO_AB
    return 0;                                          /* (tests/pages_fp.c's check: the same renders with the letter off, to tell its change from any other in the header) */
#endif
    if (ui.menu || ui.confirm || ui.uboot || ui.guide || name_on() || seq_counting())
        return 0;
    if (ui.layer)
        return ui.layer == LAYER_EDIT || ui.layer == LAYER_LFO || ui.layer == LAYER_ENV || ui.layer == LAYER_ARP ? morph_view_on(song.sel) : 0;
    if (page_global(pg) || (pg->scope != SC_TRACK && pg->scope != SC_ENGINE) || pg->fam == FAM_SAVE || pg->fam == FAM_HOME)
        return 0;
    if (pg->graph == GR_BROWSE || pg->graph == GR_PATS || pg->graph == GR_TOOLS || pg->graph == GR_EVENTS || pg->graph == GR_SLOTS)
        return 0;
    return morph_view_on(song.sel);
}
static uint32_t head_ab_b(void) { return morph_fader_of(song.sel) >= 127u; }    /* 1: B (the knobs edit End B), 0: A */
static void head_ab(int32_t x)
{
    uint32_t b = head_ab_b();
    cv_trk_r(x, H_HEAD / 2 - 7, 14, b ? 'B' - '1' : 'A' - '1', T_PEND(song.sel, b), T_BG, ux.bw && song.sel == 1u);
}
/* the parts the badge lights: the selected one; with ALGORITHM at BOTH both, only where both are the target (0.2.9, owner 2026-10-14:
 * the MORPH page, PERFORM, the TEST page's MORPH; was every MORPH page with TARGET BOTH): every other page edits one part and shows
 * its badge alone; on a GLOBAL page (params.c page_global: the SCALE page, GLO's) both, equally lit: the page does not edit the selected part */
static uint32_t badge_lit(void)
{
    uint32_t lit = 1u << song.sel;
    const page_t *pg = cur_page();
    if (ui.layer)                                       /* a quick layer (0.2.11, owner): the selected part alone, as it edits one part (also over a BOTH page); */
        return ui.layer == LAYER_GLO ? (1u << NSHOWN) - 1u : lit;   /* GLO's cards are per part: both (QUICK SCALE, whose three were global, is removed: change list A.7) */
    if (page_global(pg) || pg->scope == SC_MODE)   /* (a GLOBAL page: the SCALE page's, GLO's; the MODE page counts as global as in screen_global and ui_input.c algo_input, item 138: one rule. A global screen draws no capsules, so nothing on the screen changes) */
        return (1u << NSHOWN) - 1u;
    if ((pg->graph == GR_MORPH || pg->scope == SC_TEST || pg->scope == SC_PERF))
        lit |= morph_mask(song.sel);                    /* (ALGORITHM's PART 1 / BOTH / PART 2 there: the morph's target) */
    return lit;
}

/* DUELL 1.1 NOTES LEFT (MENU > DISPLAY > NOTES LEFT, ON by default): the notes each part can still play, digits only, in the header's left corner right after the part
 * badges (and the A / B letter where it shows), AF_S. A part's number is the voices the audio engine will give it (voice.c trk_nvoice: the stricter of the engine's own limit,
 * the VOICE CAP and the budget's NOTES, budget.c part_notes via bnotes[], which budget_poll refreshes every frame, so any change of patch, engine, filter, twin filter, effects or
 * mode shows at once) minus its keys held now (voices that are active with the gate on; fading ones count as free), never below 0. DUEL (DM_SPLIT): two numbers, part 1 then
 * part 2, 6 px apart. STEREO and LAYER (item 117, owner): one number, the SELECTED part's own (not the smaller of the two: every key plays on both parts, but the number says what
 * the part you are editing can still play). A muted part gives 0. A number is drawn in the accent
 * colour when it is 0, or when the budget (CPU) is what holds it under the engine's own limit and part_notes_reason names a cause; else in the theme colour. Hidden while the header
 * shows a message, a layer, the menu or a dialog (head_msg's room starts at x 45). The page name gives way (head_page_title's room starts after the numbers) */
#define HEAD_NOTES_GAP 6
#define HEAD_NOTES_RISE_MS 500u                         /* (owner 2026-11-07: the number flashed while the arp played) a lower number shows at once, a higher one only after it has stayed higher this long */
static uint8_t hn_shown[2];
static uint32_t hn_low_t[2];
static uint32_t head_notes(uint8_t *n)                  /* the numbers to show: how many (0, 1 or 2), in n[] */
{
    uint32_t p, k, v[2];
    if (!notes_show_on())
        return 0;
    for (p = 0; p < 2u; p++) {
        uint32_t held = 0, lim = trk[p].p[P_MUTE] ? 0u : duell_mode_req == DM_LAYER ? trk_nkeys_both() : trk_nvoice(&trk[p]);   /* (2026-11-09, owner: the number must be what
                                                         * plays: in LAYER every key sounds on both parts, so a part's number is the keys the pool holds with both playing them, voice.c trk_nkeys_both) */
        for (k = 0; k < NVOICE; k++)
            held += trk[p].v[k].active && trk[p].v[k].gate && trk[p].v[k].stage != 4u;
        v[p] = lim > held ? lim - held : 0u;
        if (v[p] <= hn_shown[p]) {                      /* drops (and steady): at once; the rise waits from here */
            hn_shown[p] = (uint8_t)v[p];
            hn_low_t[p] = fm1_ms;
        } else if (fm1_ms - hn_low_t[p] >= HEAD_NOTES_RISE_MS)
            hn_shown[p] = (uint8_t)v[p];
        v[p] = hn_shown[p];
    }
    if (duell_mode_req == DM_SPLIT || UT_CAP_VOICES_OWN) {
        n[0] = (uint8_t)v[0];
        n[1] = (uint8_t)v[1];
        return 2;
    }
    n[0] = (uint8_t)v[song.sel & 1u];                   /* (item 117: the selected part's own number) */
    return 1;
}
/* item 153 (owner): the third cell's number is the free voices in POLY only. MONO shows M, LEGATO L, UNISON U in its place (those modes play one note at a time; the
 * count there said nothing useful). The letter follows the part whose number would have stood there (the selected part's own for the one shared number). 0: POLY, a number */
static char head_mode_ch(uint32_t p)
{
    switch (trk[p & 1u].p[P_VOICE]) {
    case V_MONO: return 'M';
    case V_LEGATO: return 'L';
    case V_UNISON: return 'U';
    default: return 0;
    }
}
static uint32_t head_notes_cpu(uint32_t p)              /* 1: the budget (CPU) is what holds part p under its engine's own limit, and part_notes_reason names a cause */
{
    return !trk[p].p[P_MUTE] && bnotes[p] && bnotes[p] < trk_nvoice_eng(&trk[p]) && part_notes_reason((int)p) != NR_NONE;
}
static uint32_t head_notes_sig(void)                    /* what the drawing depends on (the numbers and, cheap, whether the budget is the limit) */
{
    uint8_t n[2];
    uint32_t c = head_notes(n), s = c * 977u;
    if (c)
        s += n[0] * 31u + (c == 2u ? n[1] * 1009u : 0u) + (bnotes[0] < trk_nvoice_eng(&trk[0]) ? 5u : 0u) + (bnotes[1] < trk_nvoice_eng(&trk[1]) ? 7u : 0u);
    return s;
}
static int32_t head_notes_w(void)                       /* the room the numbers take from x HEAD_LEFT_R (+ the A / B cell), 0: none; the gap before the page name included */
{
    uint8_t n[2];
    uint32_t c = head_notes(n), i;
    int32_t w = 0;
    char b[8];
    for (i = 0; i < c; i++) {
        b[0] = 0;
        fmt_int(b, n[i]);
        w += text_w(&AF_S, b) + (i ? HEAD_NOTES_GAP : 0);
    }
    return w ? w + 1 : 0;
}
static void head_notes_draw(int32_t x)
{
    uint8_t n[2];
    uint32_t c = head_notes(n), i;
    for (i = 0; i < c; i++) {
        char b[8];
        uint32_t hot = n[i] == 0u || (c == 2u ? head_notes_cpu(i) : head_notes_cpu(song.sel & 1u));   /* (one number: the selected part's budget) */
        b[0] = 0;
        fmt_int(b, n[i]);
        GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
        x = cv_text(x, HEAD_SY, &AF_S, b, hot ? T_ACCENT : T_THEME) + HEAD_NOTES_GAP;
    }
}

/* DUELL 1.1 HEADER (owner, HANDOFF_V2 items 82, 83 and the one after 86; HEADER_MORPH_MARKER_V1.png, the CAPSULE): on a PART page part 1's capsule [number | letter | voices] stands top left, part 2's top right,
 * 4 px from the screen's side and top, full capsules (all four corners round); the number cell is filled in the part's colour (the other part's grey), the letter (A or B: the morph end the part's knobs
 * edit) and the AVAILABLE VOICES number are plain text on the dim part. The cells have fixed widths, so only the letter and the number ever change. The page title stands centred between them, on
 * their baseline; no line, no bar. DUEL shows both capsules, STEREO and LAYER one number in both capsules: the selected part's own (item 117; UT_CAP_VOICES_OWN 1: each its own). A GLOBAL page (screen_global: GLO, SCALE, MODE, SAVE, MENU, the
 * guide, UPDATE MODE) shows NO capsule: its title centred in the gradient of the two part colours with the gradient rule under it. The battery is in the footer (draw_foot). */
#define CAP_X UT_CAP_X                                 /* (the numbers are ui_tokens.h's: item 111, measured on FINAL_V1_01_morph.png: 37 x 16, radius 5, cells 12 / 12 / 13) */
#define CAP_Y UT_CAP_Y
#define CAP_H UT_CAP_H
#define CAP_NUM_W UT_CAP_NUM                           /* its cells: the part's number, the letter, the voices number (+ the divider and the right outline) */
#define CAP_LET_W UT_CAP_LET
#define CAP_VOI_W (UT_CAP_DIV + UT_CAP_VOI + 1)
#define CAP_W UT_CAP_W
#define HEAD_TITLE_L (CAP_X + CAP_W + 4)               /* a message's room: 45 .. 195 (the page title's is UT_TITLE_L .. UT_TITLE_R) */
#define HEAD_TITLE_R (240 - CAP_X - CAP_W - 4)
/* a GLOBAL screen: the gradient accent, no part capsules (the pages that edit the instrument, not a part) */
static int screen_global(void)
{
    const page_t *pg = cur_page();
    if (ui.menu || ui.guide || ui.uboot)
        return 1;
    if (name_on())                                      /* (owner V13: SAVE / NAME show both capsules, the target part lit) */
        return 0;
    if (ui.layer || ui.perf)
        return 0;
    return page_global(pg) || pg->scope == SC_MODE;
}
/* a text of the capsule centred by its advance in the 12 px cell at x (item 111: the pictures centre by the advance, on a fraction of a pixel), face F, capitals ending on UT_CAP_BASE */
static void head_cell_text(int32_t x16, int32_t cw, const char *s, uint16_t fg, uint16_t bg)   /* x16: the cell's left, 1/16 px; cw its width (2026-11-07: two digits sit closer, UT_CAP_TIGHT16, the pair centred by its ink) */
{
    int32_t y = UT_CAP_BASE - AF_S_CAP_Y - AF_S_CAP_H + 1 - Y_HEAD;
    GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
    if (s[0] >= '0' && s[0] <= '9' && s[1] >= '0' && s[1] <= '9' && !s[2]) {
        char d[2] = {0, 0};
        const aag_t *g0 = &AF_S.g[glyph(&AF_S, (uint32_t)(uint8_t)s[0])], *g1 = &AF_S.g[glyph(&AF_S, (uint32_t)(uint8_t)s[1])];
        int32_t a0, l16, r16, p16;
        d[0] = s[0];
        a0 = text_w16(&AF_S, d);
        l16 = g0->bx * 16;
        r16 = a0 - UT_CAP_TIGHT16 + (g1->bx + g1->bw) * 16;
        p16 = x16 + (cw * 16 - (r16 - l16)) / 2 - l16;
        cv_text_x16(p16, y, &AF_S, d, fg, bg);
        d[0] = s[1];
        cv_text_x16(p16 + a0 - UT_CAP_TIGHT16, y, &AF_S, d, fg, bg);
        return;
    }
    cv_text_x16(x16 + (cw * 16 - text_w16(&AF_S, s)) / 2, y, &AF_S, s, fg, bg);
}
static void head_capsule(int32_t x, uint32_t k, uint32_t lit, uint32_t b, int32_t voices, uint32_t hot)
{
    uint16_t pc = lit ? T_PTHEME(k) : T_DIM, cell = lit ? ux_mix(T_BG, pc, UT_CAP_DIM_PCT) : ux_mix(T_BG, T_TEXT, UT_CAP_OFF_PCT);
    char n[8];
    (void)hot;
    cv_rrect(x, UT_CAP_Y, UT_CAP_W, UT_CAP_H, UT_CAP_R, pc, T_BG);                          /* the outline and the number cell in the part colour (an unlit part: T_DIM) */
    cv_rrect(x + UT_CAP_NUM, UT_CAP_Y + 1, UT_CAP_W - UT_CAP_NUM - 1, UT_CAP_H - 2, UT_CAP_R - 1, cell, pc);   /* the dim part: the letter and the voices number */
    cv_rect(x + UT_CAP_NUM, UT_CAP_Y + 1, 4, UT_CAP_H - 2, cell);                          /* (its left side square) */
    if (UT_CAP_DIV)                                     /* (owner 2026-11-07, option 3) the line before the voices cell */
        cv_rect(x + UT_CAP_NUM + UT_CAP_LET, UT_CAP_Y + 1, 1, UT_CAP_H - 2, pc);
    n[0] = (char)('1' + k);
    n[1] = 0;
    head_cell_text(x * 16 + (k ? UT_CAP_P2_DX * 16 : 0), UT_CAP_NUM, n, ux_cr(T_TEXT, pc) >= 300u ? T_TEXT : ux_cr(T_BG, pc) >= ux_cr(T_TEXT, pc) ? T_BG : T_TEXT, pc);   /* the part number: T_TEXT (white; dark in the light palettes) where it reads 3:1 on the cell, else the better of BG / TEXT (ORANGE, GREY, MONO: dark) */
    n[0] = b ? 'B' : 'A';
    head_cell_text((x + UT_CAP_NUM) * 16, UT_CAP_LET, n, ux_mix(T_BG, T_TEXT, UT_CAP_LETTER_PCT), cell);
    if (voices < -1) {                                  /* item 153: a mode letter (the code is -voices: 'M', 'L', 'U') instead of the number */
        n[0] = (char)(-voices);
        n[1] = 0;
        head_cell_text((x + UT_CAP_NUM + UT_CAP_LET + UT_CAP_DIV) * 16, UT_CAP_VOI, n, T_TEXT, cell);   /* (2026-11-07: centred in the voices cell's inside, after the divider) */
    } else if (voices >= 0) {
        fmt_int(n, voices);
        head_cell_text((x + UT_CAP_NUM + UT_CAP_LET + UT_CAP_DIV + (voices < 10 ? 1 : 0)) * 16, UT_CAP_VOI, n, T_TEXT, cell);   /* (owner V13: the voices number is T_TEXT always; 2026-11-07: two digits sit closer, centred in the voices cell; one digit 1 px further right) */
    }
}
/* the page title, on the header canvas (item 111: face T Bold 15, T_TEXT white, centred on the screen's middle, no colour, no line; S when T does not fit; `glob`: no capsules, the whole width).
 * One function for every header: draw_head below and the MENU / ABOUT header (ui_menu.c menu_head, item 119) */
static int32_t head_title_ink(const char *tb, int glob, uint16_t ink, int32_t *pw);
static void head_title_draw(const char *tb, int glob)
{
    int32_t w;
    (void)head_title_ink(tb, glob, T_TEXT, &w);
}
/* item 158: the title's one placement and drawing, in colour ink (the page title: T_TEXT; a program change's '#NN NAME': the part's colour, head_title_msg). Returns the pen's
 * start in 1/16 px and *pw the advance in 1/16 px (the face is the title's: T, or S when T does not fit), so a message drawn here stands pixel for pixel where the same text as a title stands */
static int32_t head_title_ink(const char *tb, int glob, uint16_t ink, int32_t *pw)
{
    int32_t tw, x16, lb = UT_TITLE_L, rb = UT_TITLE_R;
    const aafont_t *fo = &AF_T;
    if (glob) {                                    /* global: the whole width, no capsule */
        lb = UT_TITLE_GL;
        rb = UT_TITLE_GR;
    }                                              /* (DUELL 1.1: a title never has an icon) */
    tw = text_w16(fo, tb);
    if ((tw + 8) >> 4 > rb - lb) {                 /* T does not fit: S */
        fo = &AF_S;
        tw = text_w16(fo, tb);
    }
    x16 = UT_TITLE_CX * 16 - tw / 2;               /* centred on the screen's middle (a fraction of a pixel too), kept inside the room */
    if (x16 > (rb << 4) - tw)
        x16 = (rb << 4) - tw;
    if (x16 < (lb << 4))
        x16 = lb << 4;
    GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
    if (fo == &AF_T)                               /* item 111: Bold 15, white, centred, no colour, no line (global pages too) */
        cv_text_x16(x16, UT_TITLE_BASE - AF_T_CAP_Y - AF_T_CAP_H + 1 - Y_HEAD, &AF_T, tb, ink, T_BG);
    else
        cv_free_text(x16 >> 4, HEAD_SY, &AF_S, tb, ink, T_BG, rb - (x16 >> 4));
    *pw = tw;
    return x16;
}
#define HEAD_ARROW_W 10                               /* item 158: the program change's arrow, 10 px wide: the head's 6 columns and the shaft's 4 */
#define HEAD_ARROW_HEAD 6
#define HEAD_ARROW_SP 3                                /* .. at least 3 px of background on both sides of it (to the text's ink and to the capsule) */
/* the ink of text b in T placed as the title places it (head_title_ink's T branch: centred on UT_TITLE_CX, kept in lb .. rb) -> bb (x0, -, x1 past the ink), its pen start */
static int32_t head_title_inkbox(const char *b, int32_t lb, int32_t rb, int32_t *bb)
{
    int32_t tw = text_w16(&AF_T, b), x16 = UT_TITLE_CX * 16 - tw / 2;
    if (x16 > (rb << 4) - tw)
        x16 = (rb << 4) - tw;
    if (x16 < (lb << 4))
        x16 = lb << 4;
    text_pen0 = x16 & 15;
    text_exact = 1;
    (void)text_run(x16 >> 4, 0, &AF_T, b, 0, bb);
    text_exact = 0;
    text_pen0 = 0;
    return x16;
}
/* item 157 (owner): a message in the title's place (ui.c MSG_PC1 / MSG_PC2 / MSG_TITLE, its first byte). Item 158 (owner): the TEXT is placed exactly as the title (head_title_ink:
 * the title's own placement, face T, size and baseline); only its colour differs. MIDI program change of a sound: "#NN NAME" in the part's colour (T_PTHEME) and an arrow in that
 * colour, which belongs to the message and is ALWAYS drawn: part 1 left of the text pointing left, part 2 right of it pointing right, the exact mirror. The arrow (owner: the left
 * arrow of capsule_shift_and_part_load.png, pixel for pixel): 10 px wide, 11 rows (T's capitals' rows), a head of 6 columns (1, 3, 5, 7, 9, 11 rows from the tip) and a shaft of
 * 4 columns x 3 rows. It stands HALFWAY in the free columns between the text's INK (as drawn) and the capsule's outline (a global screen: the canvas's edge), an odd pixel to the
 * text's side, with at least 3 px of background on both sides: the free room must be 16 px. When it is not, the NAME is shortened (text_fit in T, the cut shown by one '.': T has
 * no ellipsis) a pixel at a time until it is, and the shortened text is placed as a title. MSG_TITLE: the title, white */
static void head_title_msg(int glob)
{
    uint32_t k = ui.msg[0] == MSG_PC2[0] ? 1u : 0u;
    int arrow = ui.msg[0] != MSG_TITLE[0];
    uint16_t ink = arrow ? T_PTHEME(k) : T_TEXT;
    int32_t ah = AF_T_CAP_H, aw = HEAD_ARROW_W, need = HEAD_ARROW_W + 2 * HEAD_ARROW_SP;
    int32_t top = UT_TITLE_BASE - AF_T_CAP_H + 1 - Y_HEAD, tw, ax, i, f, w, bb[4];
    int32_t lb = glob ? UT_TITLE_GL : UT_TITLE_L, rb = glob ? UT_TITLE_GR : UT_TITLE_R;
    int32_t lo = glob ? 0 : CAP_X + CAP_W, hi = glob ? 239 : 240 - CAP_X - CAP_W - 1;   /* the arrow's room: between the capsules (their outlines excluded) */
    char b[48];
    if (!arrow) {
        (void)head_title_ink(ui.msg + 1, glob, ink, &tw);
        return;
    }
    str_cpy(b, ui.msg + 1, sizeof b);
    w = text_w(&AF_T, b);
    if (w > rb - lb)                                    /* (wider than the title room in T: cut to it first) */
        text_fit(b, sizeof b, ui.msg + 1, &AF_T, rb - lb);
    w = text_w(&AF_T, b);
    for (;;) {                                          /* the name shortened until the arrow has its 16 px */
        (void)head_title_inkbox(b, lb, rb, bb);
        f = bb[2] <= bb[0] ? need : k ? hi - bb[2] + 1 : bb[0] - lo;
        if (f >= need || w <= 0)
            break;
        text_fit(b, sizeof b, ui.msg + 1, &AF_T, --w);
    }
    (void)head_title_ink(b, glob, ink, &tw);
    ax = k ? bb[2] + (f - aw + 1) / 2                   /* part 2: halfway in bb[2] .. hi, an odd pixel to the text's side */
           : lo + (f - aw) / 2;                         /* part 1: halfway in lo .. bb[0] - 1 */
    for (i = 0; i < aw; i++) {                          /* column i from the tip: the head 2 i + 1 rows, then the shaft's 3, on the capitals' middle row */
        int32_t h = i < HEAD_ARROW_HEAD ? 2 * i + 1 : 3;
        if (h > ah)
            h = ah;
        cv_rect(k ? ax + aw - 1 - i : ax + i, top + (ah - h) / 2, 1, h, ink);
    }
}
static void draw_head(void)
{
    char tb[40];
    uint32_t pi;
    int msg = ui.msg_t || ui.layer, glob = screen_global();
    uint32_t sig, mut = 0;
    uint8_t vn[2];
    uint32_t nv = 0;
    head_page_title(tb, &pi);
    if (!glob)
        nv = head_notes(vn);
    sig = song.sel * 13131u + (uint32_t)glob * 7u + (ui.msg_t ? str_hash(7u, ui.msg) : ui.layer * 7919u + (uint32_t)layer_locked() * 3u) +
          head_notes_sig() * 6007u + (uint32_t)head_mode_ch(0) * 53u + (uint32_t)head_mode_ch(1) * 59u + (uint32_t)(song.sel & 1u) * 61u + badge_lit() * 4093u + (uint32_t)duell_mode_req * 331u +
          (msg ? 0u : str_hash(13u, tb) + pi * 31u) + (head_ab_b() ? 811u : 0u) + (morph_fader_of(1 - song.sel) >= 127u ? 1301u : 0u) + (uint32_t)(notes_show_on() ? 17u : 0u) + (uint32_t)nv * 41u;
    (void)mut;
    if (!ui.force && sig == ui.head_sig)
        return;
    ui.head_sig = sig;
    cv_begin(240, H_HEAD, T_BG);
    if (!glob) {                                       /* a part page: the capsules */
        uint32_t lit = badge_lit(), p;
        int32_t v0 = -1, v1 = -1;
        if (nv == 2u) {
            v0 = vn[0];
            v1 = vn[1];
        } else if (nv == 1u) {
            v0 = v1 = vn[0];
        }
        if (nv) {                                      /* item 153: MONO / LEGATO / UNISON show their letter in place of the number (the shared number: the selected part's mode) */
            char m0 = head_mode_ch(nv == 2u ? 0u : song.sel), m1 = head_mode_ch(nv == 2u ? 1u : song.sel);
            if (m0)
                v0 = -(int32_t)m0;
            if (m1)
                v1 = -(int32_t)m1;
        }
        /* both parts' capsules in every mode (owner, test3): the selected part lit, the other dim; STEREO and LAYER show the one shared voices number in both */
        head_capsule(CAP_X, 0, lit & 1u, morph_fader_of(0) >= 127u, v0, v0 == 0 || (nv == 2u && head_notes_cpu(0)) || (nv == 1u && (head_notes_cpu(0) || head_notes_cpu(1))));
        head_capsule(240 - CAP_X - CAP_W, 1, (lit >> 1) & 1u, morph_fader_of(1) >= 127u, v1, v1 == 0 || (nv == 2u && head_notes_cpu(1)) || (nv == 1u && (head_notes_cpu(0) || head_notes_cpu(1))));
    }
    if (ui.msg_t && (ui.msg[0] == MSG_PC1[0] || ui.msg[0] == MSG_PC2[0] || ui.msg[0] == MSG_TITLE[0])) {   /* item 157: a program change's load, as the title */
        head_title_msg(glob);
    } else if (msg) {                                  /* a message / a layer's label: centred, over the page name */
        head_msg(-1, HEAD_TITLE_R - HEAD_TITLE_L);
    } else {
        head_title_draw(tb, glob);
    }
    /* (owner V9: no rule under any title; the gradient stays on the title text, values and bars) */
    cv_blit(0, Y_HEAD);
}
/* Rows y0 .. y1 - 1 across the screen: the BG */
static void gap_rows(uint32_t y0, uint32_t y1)
{
    lcd_fill(0, y0, 240, y1 - y0, T_BG);
}
/* the BG beside the four columns (the cards; MIXER's strips) over rows y .. y + h - 1 */
static void draw_gaps(uint32_t y, uint32_t h)
{
    uint32_t i;
    lcd_fill(0, y, (uint32_t)CARD_X(0), h, T_BG);
    for (i = 0; i < 4u; i++) {                          /* right of each column */
        uint32_t x = (uint32_t)(CARD_X(i) + CARD_W), w = i < 3u ? (uint32_t)CARD_X(i + 1u) - x : 240u - x;
        lcd_fill(x, y, w, h, T_BG);
    }
}
/* full redraw: the strips (header, cards, panel, footer) cover the rest; only the BG between them is filled */
static void draw_frame(void)
{
    const uint32_t ch = CARD_H, gy = Y_GRAPH, gh = H_GRAPH;
    if (ui.force == 2)                                 /* item 152 A: the same page again (ui_refresh): the strips and cards cover their own rows, the BG between them is already there */
        return;
    if (grid_on) {                                     /* the grid: everything under the header is drawn over the background */
        gap_rows(H_HEAD, Y_FOOT + 15);
        return;
    }
    gap_rows(H_HEAD, Y_LABEL);
    gap_rows(Y_LABEL + ch, gy);
    gap_rows(gy + gh, Y_FOOT);
    draw_gaps(Y_LABEL, ch);
}

#ifndef CARD_HOOK
#define CARD_HOOK(v, u) ((void)0)                       /* host: a card value set in S (M too wide with its unit) */
#endif
/* MOTION on the cards (#63: values that move on their own): the selected track's motion-driven parameters
 * (motion.c motion_mask: PLAY ON and an event for the id), once per draw_columns; card_mot_next is set just
 * before a draw_column of a track parameter (card_mot_of) and taken by it; card_mot: the cards drawn with it. */
static uint32_t card_mot_mask[(P_COUNT + 31u) / 32u];
static uint8_t card_mot_next, card_mot;
/* An INAPPLICABLE card (owner rule, 2026-10-15, replacing the 2026-10-12 fade-with-reason rule): a control that does nothing for the current
 * selection is NOT DRAWN: its column stays empty (the panel's background, no card), the other cards keep their knobs K1..K4, and the hidden
 * card's knob does nothing and says nothing (ui_input.c: card_hidden_vp). ONE mechanism: draw_column_hide(c) (below draw_column); the
 * predicate for a parameter's card is card_hidden_vp (below). card_why_next is no reason any more: it is the CLK card's state line only
 * (the source an external or AUTO clock plays on, clock_why). */
static const char *card_why_next;
static uint8_t card_hide_next;
static const char *quick_label(const page_t *pg, uint32_t s);   /* (ui_layer.c: QUICK MULTI's CUT 1 RES 1 CUT 2 RES 2) */
static uint8_t card_ctr_next;                           /* the next draw_column's gauge fills from the CENTRE (a true bipolar value: OFFSET, TUNE, FDBK, BLEND; ratio 0..1000 with 500 = zero); cloud job H */
/* MORPH on the cards: the value a track parameter's card shows is what the fader sounds like (morph.c morph_view), and the card
 * the morph is moving (shown != stored Original) carries a mark. card_mk_next (ui_fm6op.c): set by card_vv, taken by the next draw_column */
static int32_t card_vv(const int16_t *vp)
{
    uintptr_t id = ((uintptr_t)vp - (uintptr_t)TSEL->p) / sizeof *vp;
    int32_t v = *vp;
    if ((id < P_COUNT || (id >= XW_BASE && id < EXT_P_LEN)) && morph_view_on(song.sel)) {   /* item 154: the extension slots too (TWIN ENV 2 / LFO 1 / LFO 2, SUB, CYCLE: the roll moves them, morph_view has them; the card showed the Original at B) */
        v = morph_view(song.sel, (uint32_t)id);
        card_mk_next = (uint8_t)(v != *vp);
    }
    return v;
}
static void card_mot_of(const int16_t *vp)
{
    uintptr_t id = ((uintptr_t)vp - (uintptr_t)TSEL->p) / sizeof *vp;   /* (not the track's: huge) */
    card_mot_next = (uint8_t)(id < P_COUNT && ((card_mot_mask[id / 32u] >> (id % 32u)) & 1u));
}

/* one card = one knob: [icon] LABEL / value unit / gauge on a SURF card, redrawn only when it changed.
 * The value is M, or S when M is too wide (engine names, long ENUMs); the unit S after it. The gauge is
 * a 3 px rounded bar: BG track, THEME fill. The knob just turned (hot): icon, label, value and gauge in
 * the accent. ratio: 0..1000 for the gauge, -1 = no gauge. icon: ICON_* (icons.c), ICON_AUTO = by label;
 * a label too long to share the card with its icon goes without it. A parameter MOTION drives (card_mot_next):
 * the motion icon in place of its own, in the theme colour; a long label moves it left, up to the card's edge;
 * a label with no room for it at all (or FELUCCA_ICONS=0): the label in the theme colour.
 * vc: the value's colour (T_THEME, T_DIM inactive, T_ACCENT the knob just turned) */
/* 0.2.25 layout round (owner): a label too long for the card's 50 px breaks into two lines at a space (never inside a word); a label may also carry
 * its break itself ("PATCH\nLEVEL": the cards of one page alike). Then the label takes the card's first two lines, the value its third, in S, and
 * there is no gauge (the TEMPO page's clock card's method: CLK / AUTO / INT). l1 holds 16; -> the second line, 0 for a label of one line */
#define LBL_W (COL_W - 19)                             /* a label's line after the numbered dot: x 17 .. COL_W - 2 */
static const char *label_split(const char *label, char *l1)
{
    uint32_t i, cut = 0;
    for (i = 0; label[i] && i < 15u; i++)
        if (label[i] == '\n') {
            str_cpy(l1, label, i + 1u);
            return label + i + 1;
        }
    if (!label[0] || text_w(&AF_S, label) <= LBL_W)
        return 0;
    for (i = 0; label[i] && i < 15u; i++)              /* the last space whose first line fits */
        if (label[i] == ' ') {
            str_cpy(l1, label, i + 1u);
            if (text_w(&AF_S, l1) <= LBL_W)
                cut = i;
        }
    if (!cut)
        return 0;
    str_cpy(l1, label, cut + 1u);
    return label + cut + 1;
}
#ifndef CARD_TRACK_HOOK
#define CARD_TRACK_HOOK(l, trk) ((void)0)              /* host: a label drawn tightened (its tracking, 1/16 px a glyph) */
#endif
/* item 158 (owner: no label is cut by even 1 px; fit by tightening the letter spacing, the minimum needed, same face, weight and size): the tracking (1/16 px a glyph, 0 or
 * negative) that ends the label's ink at least 1 px inside the canvas (the card) it is drawn on, starting at x lx; at most 1 px a letter, else 0 (the label as before, cut) */
#define LBL_TRK_MAX 16                                 /* item 158 (coordinator): the tightening is capped at 1.0 px a letter (16/16) */
static int32_t lbl_trk(int32_t lx, const aafont_t *f, const char *s)
{
    int32_t b[4], t = 0;
    for (;; t--) {
        text_trk = t;
        (void)text_run(lx, 0, f, s, 0, b);
        text_trk = 0;
        if (b[2] <= (int32_t)cv_w - 1 || b[2] <= b[0] || t < -LBL_TRK_MAX)
            break;
    }
    if (t < -LBL_TRK_MAX)                               /* (more than 1 px a letter reads crushed: drawn as before item 158, untightened; listed as OPEN, the owner decides) */
        t = 0;
    if (t)
        CARD_TRACK_HOOK(s, t);
    return t;
}
/* one line of a card's label at (lx, ly): MONO's hot chip, or the label colour */
static void card_label_line(uint32_t c, int32_t lx, int32_t ly, const char *txt, int hot, uint16_t lc)
{
    const aafont_t *lf = &AF_S;
    int32_t cy = ly + AF_S_CAP_Y;
    if (text_w(lf, txt) > COL_W - 2 - lx)               /* (item 120: cut by the card's edge, never ellipsised; listed) */
        CARD_LABEL_HOOK(c, txt, text_w(lf, txt), COL_W - 2 - lx);
    text_trk = lbl_trk(lx, lf, txt);                   /* (item 158: tightened only when it would reach the card's edge) */
    if (hot && T_ACCENT == T_MID) {                     /* MONO: no colour tells the hot knob: its label inverted, */
        int32_t b[4], w;                                /* the chip 2 px round its ink (M: 3 above, 2 below) */
        text_ink(lf, txt, b);
        w = b[2] - b[0] + 4;
        if (w > COL_W + 1 - lx - b[0]) w = COL_W + 1 - lx - b[0];
        cv_rrect(lx + b[0] - 2, cy - 3, w, 15, 3, T_ACCENT, T_BG);
        GFX_HOOK_ALIGN(lx + b[0] - 2, cy - 3, lx + b[0] - 2 + w, cy + 12, AL_HV,
                       "card label chip (MONO hot)");
        cv_text_on(lx, ly, lf, txt, T_INK, T_ACCENT);
    } else
        cv_text_on(lx, ly, lf, txt, lc, T_BG);
    text_trk = 0;
}
/* ---- the GRID layout (grid_on 1: the picture on top, four cards 115 x 52 under it; grid_on 2: no picture, four cards 115 x 84 with a line of text under the value, MODULES, MIDI, SIDECHAIN): K1 K2 over K3 K4, the knob's
 * number in a circle, the label, the value big (Bold 23, Medium or small when it is too wide), its unit small, the gauge ---- */
static uint8_t grid_big;                               /* the next grid card is HOME's big MORPH value: no label, no mark, no bar, the value in X (38 px) */
static const char *grid_d;                             /* the text under the value of the next card (grid_on 2) */
static uint8_t grid_wide;                              /* grid_on 2: card 3 is as wide as the screen (MIDI IN) */
static uint8_t grid_mot;                               /* the next grid card's parameter is driven by MOTION (item 120) */
/* item 156, TEMPLATE 8 (grid_on 14, ui_tokens.h UT_T8_*): the CELL pages. cell_n: how many cards show on the page now; cell_at[c]: the cell (0 .. cell_n - 1, in the order they show) of column c, 0xFF: none.
 * Set by cells_plan (ui_draw_page, before the frame is drawn); a change of either draws the page whole (its cells move). cell_seg_n / cell_seg_i: the CHOICE the next card shows (its option count and
 * the option's place in the knob's order), set by draw_columns for that card only; 0: not a choice */
static uint8_t cell_n, cell_at[4] = {0xFFu, 0xFFu, 0xFFu, 0xFFu}, cell_seg_n, cell_seg_i;
#define CELLS_ON (grid_on == 14 || grid_on == 15)       /* item 165: the cells of template 8, also on the ENGINE page of LOFI / NOISE (grid_on 15: K1 is the engine row, the cards K2 .. K4 are cells) */
#define ENG_LIST_ON (grid_on == 15 && ui.hot_t && ui.hot_col == 0u)   /* item 165: K1 turning on that page: the engine list in the top cell's place (ui_graph.c draw_graph) */
/* TEMPLATE 9, the AREA page (grid_on 16, ui_tokens.h UT_A_*; LAYOUT_IMPLEMENTATION_PLAN.md block A): knob 1 is a LINE (no card) on rows 27 .. 47, the page's picture is drawn into the big area (area_rect),
 * knobs 2 .. 4 are cards in ONE card row. area_cols: 3 (columns of 76) or 2 (columns of 118); area_two: a card label of this page needs two lines, so every card of the page starts one label line higher
 * (FIRMWARE_CHANGE_LIST section C). Both are set by area_plan (ui_draw_page, before the frame is drawn) and, for the QUICK SEQ layer, by ui_draw_page's layer choice; a change of either draws the page whole.
 * The ENGINE page (grid_on 5) keeps its K1 row and its picture but uses the same card row, always three columns. */
#define AREA_ON (grid_on == 16)
#define AREA_ROW_ON (grid_on == 16 || grid_on == 5)   /* the pages whose knobs 2 .. 4 are cards of the area card row */
static uint8_t area_cols = 3u, area_two;
#define AREA_TWO (area_two || grid_on == 5)            /* the card row starts one label line higher: a page with a two-line label; the ENGINE page always (its picture ends at row 129: the taller cards cost nothing there and an engine change never moves them) */
/* the text after the knob-1 line's value (CHORD: the chord's name; ARP: EXAMPLE: C E G at the right edge; PERFORM: SELECT = BALANCE), set by draw_columns for the next knob-1 line only; 0: none */
static const char *k1_after_next;
static uint16_t k1_after_col;
static uint8_t k1_after_r;
/* the big area of an area page now (screen rows): the picture code of the page draws into rows *y .. *y + *h - 1 (x 0 .. 239). A page with a two-line card label leaves its last UT_A_LBL2_PITCH rows to the cards */
static void area_rect(int32_t *y, int32_t *h)
{
    *y = UT_A_GR_Y;
    *h = UT_A_GR_H - (area_two ? UT_A_LBL2_PITCH : 0);
}
/* the card of knob c (1 .. 3) in the area card row: x, y, w, h (w = h = 0: no card in that column) */
static void area_geo(uint32_t c, int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
    uint32_t cols = grid_on == 5 ? 3u : area_cols;
    int32_t top = UT_A_CARD_Y - (AREA_TWO ? UT_A_LBL2_PITCH : 0);
    *x = *y = *w = *h = 0;
    if (c < 1u || c > cols)
        return;
    *x = UT_A_X0 + (cols == 2u ? UT_A_DX2 : UT_A_DX3) * (int32_t)(c - 1u);
    *w = cols == 2u ? UT_A_W2 : UT_A_W3;
    *y = top;
    *h = UT_A_CARD_Y + UT_A_CARD_H - top;
}
static void grid_geo(uint32_t c, int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
    *x = UT_CARD_X0 + UT_CARD_DX * (int32_t)(c & 1u);
    *w = UT_CARD_W;
    if (AREA_ON && c == 0u) {                          /* TEMPLATE 9: knob 1's line, the screen's width */
        *x = 0;
        *y = UT_A_K1_Y;
        *w = 240;
        *h = UT_A_K1_H;
        return;
    }
    if (AREA_ROW_ON) {                                 /* TEMPLATE 9 and the ENGINE page: the card row (knob 1 of the ENGINE page is its K1 row, engine_card: no rectangle here) */
        area_geo(c, x, y, w, h);
        return;
    }
    if (CELLS_ON) {                                  /* TEMPLATE 8 (item 156): the cell of column c by the number of cards that show; a column with no cell: no rectangle (w = h = 0) */
        uint32_t k = cell_at[c & 3u];
        int32_t y0 = grid_on == 15 ? UT_T8E_Y0 : UT_T8_Y0, dy = grid_on == 15 ? UT_T8E_DY : UT_T8_DY;   /* (item 165: the ENGINE page's cells under its K1 row) */
        *x = UT_T8_X0;
        *y = y0;
        *w = UT_T8_WIDE;
        *h = grid_on == 15 ? UT_T8E_H : UT_T8_H;
        if (k >= cell_n) {
            *w = *h = 0;
        } else if (cell_n >= 4u) {                     /* 2 x 2 */
            *x = UT_T8_X0 + UT_T8_DX * (int32_t)(k & 1u);
            *y = y0 + dy * (int32_t)(k >> 1);
            *w = UT_T8_W;
        } else if (cell_n == 3u && k) {                /* one wide cell on top, two under it */
            *x = UT_T8_X0 + UT_T8_DX * (int32_t)(k - 1u);
            *y = y0 + dy;
            *w = UT_T8_W;
        } else if (cell_n == 2u) {                     /* two wide rows */
            *y = y0 + dy * (int32_t)k;
        }
        return;
    }
    if (grid_on == 6) {                                /* MIDI LEARN: LEARN and CLEAR on top, the steps under them */
        *x = 4 + 117 * (int32_t)(c & 1u);
        *w = 115;
        *y = 28;
        *h = 52;
        return;
    }
    if (grid_on == 7) {                                /* TEMPLATE 4 (MIXER, item 117): K1 LEVEL 1, K2 LEVEL 2 over K3 PAN 1, K4 PAN 2 */
        *x = UT_T4_X0 + UT_T4_DX * (int32_t)(c & 1u);
        *w = UT_T4_W;
        *y = (c >> 1) ? UT_T4_PAN_Y : UT_T4_LEV_Y;
        *h = UT_T4_H;
        return;
    }
    if (grid_on == 10) {                               /* TEMPLATE 7 (item 118): the STEP page's four cards in one compact row under the roll */
        static const int16_t W7[3] = {UT_T7_CARD_W0, UT_T7_CARD_W1, UT_T7_CARD_W2};
        uint32_t i;
        *x = UT_T7_X;
        for (i = 0; i < c; i++)
            *x += W7[i] + UT_T7_CARD_GAP;
        *w = c == 3u ? UT_T7_CARD_R - *x : W7[c];
        *y = UT_T7_CARD_Y;
        *h = UT_T7_CARD_H;
        return;
    }
    if (grid_on == 3) {                                /* MODE: K2 K3 K4 in a row under the list (K1 is the list) */
        *x = 4 + 78 * (int32_t)(c - 1u);
        *w = 76;
        *y = 150;
        *h = UT_CARD_H;
        return;
    }
    if (grid_on == 2) {
        *y = 28 + 88 * (int32_t)(c >> 1);
        *h = 84;
        if (grid_wide && c == 2u) {
            *x = 4;
            *w = 232;
        }
    } else {
        *y = UT_CARD_Y0 + UT_CARD_DY * (int32_t)(c >> 1);   /* TEMPLATE 1 (ui_tokens.h): x 3 .. 118 and 121 .. 236, y 106 .. 159 and 162 .. 215 */
        *h = UT_CARD_H;
    }
}
#ifndef CARD_VALUE_HOOK
#define CARD_VALUE_HOOK(c, v, need, room) ((void)0)    /* host: a card value wider than its room (old four-card pages: cut by the card's edge, never ellipsised; item 120) */
#endif
#ifndef CARD_LABEL_HOOK
#define CARD_LABEL_HOOK(c, l, need, room) ((void)0)    /* host: a template-1 label wider than its room (it is cut by the card's edge, never shortened) */
#endif
/* ONE card's dot + label (templates 1, 2 and 3; item 115): the numbered dot (the label's cap height, on its cap rows) at dx, the label in S at lx, both on the capitals' last row `base` (card rows);
 * the label is NEVER shortened and NEVER ellipsised: wider than `room` it is cut by the card's edge and CARD_LABEL_HOOK lists it. lc: the colour of both */
/* the one dot + label (item 117: the dot and the label may have their own colours, on any ground `under`): the cards of templates 1 .. 4 (card_label) and the knob labels of template 5 */
static void dot_label(uint32_t c, int32_t dx, int32_t lx, int32_t base, const char *label, uint16_t dc, uint16_t lc, int32_t room, uint16_t under)
{
    char dg[2] = {(char)('1' + c), 0};
    int32_t dy = base - AF_S_CAP_H + 1;               /* the dot's top = the label's cap top; its diameter = the label's cap height */
    cv_rrect(dx, dy, AF_S_CAP_H, AF_S_CAP_H, AF_S_CAP_H / 2, dc, under);
    cv_text_in(dx, dy + HALF_UP(AF_S_CAP_H - AF_D_CAP_H) - AF_D_CAP_Y, AF_S_CAP_H, &AF_DOT, dg, under, dc);
    if (text_w(&AF_S, label) > room)
        CARD_LABEL_HOOK(c, label, text_w(&AF_S, label), room);
    text_trk = lbl_trk(lx, &AF_S, label);              /* (item 158: tightened only when it would reach the card's edge) */
    cv_text_on(lx, UT_TY(S, base), &AF_S, label, lc, under);
    text_trk = 0;
}
static void card_label(uint32_t c, int32_t dx, int32_t lx, int32_t base, const char *label, uint16_t lc, int32_t room)
{
    dot_label(c, dx, lx, base, label, lc, lc, room, T_BG);
}
/* the rows of a face's line box for a text whose capitals' last row is `base` (the faces a card value may use) */
static int32_t card_ty(const aafont_t *f, int32_t base)
{
    return f == &AF_N ? UT_TY(N, base) + 1 : f == &AF_L ? UT_TY(L, base) : f == &AF_M ? UT_TY(M, base) : f == &AF_T ? UT_TY(T, base) : UT_TY(S, base);   /* (T: item 158, a CELL's Bold fallback; N + 1: its figures end one row above its H, measured 2026-11-08: on the baseline with the unit and the names now) */
}
static int card_figure(const char *s)                  /* only the figures the N face has: 0 .. 9 . - + % */
{
    int32_t i;
    for (i = 0; s[i]; i++)
        if (!((s[i] >= '0' && s[i] <= '9') || s[i] == '.' || s[i] == '-' || s[i] == '+' || s[i] == '%'))
            return 0;
    return i > 0;
}
/* the numbers of a card template that differ between templates 1 (part pages), 2 (ENGINE cells), 8 (cells) and 9 (the area card row): rows rel to the card.
 * Flags: num (figures in N), cut (a value is cut by the card's edge, never fitted), seg (a CHOICE shows the segment line: cell_seg_n / cell_seg_i, the ONE place that decides it),
 * bold (a value too wide for L falls to the Bold T only, never to M / S: item 158), area (the area card row's rules: a label may take two lines, % and "/16" with no space and white, "/16" in the value's face,
 * an action's key pill on the row pill_base) */
typedef struct { int16_t pad, lbl_base, lbl_dx, lbl_pad_r, val_base, val2_b1, val2_b2, unit_gap, bar_y, pill_base; uint8_t num, cut, seg, bold, area; } card_geo_t;
static const card_geo_t CG1 = {UT_CARD_PAD, UT_CARD_LBL_BASE, UT_CARD_LBL_DX, UT_CARD_LBL_PAD_R, UT_CARD_VAL_BASE, UT_CARD_VAL2_BASE1, UT_CARD_VAL2_BASE2, UT_CARD_UNIT_GAP, UT_CARD_BAR_Y, 0, 0, 0, 1, 0, 0};
/* item 120: the MODE page's K2..K4 (76 x 54): template 1's rows and sizes, a value cut by the card's edge like template 2's, never ellipsised */
static const card_geo_t CG3 = {UT_CARD_PAD, UT_CARD_LBL_BASE, UT_CARD_LBL_DX, UT_CARD_LBL_PAD_R, UT_CARD_VAL_BASE, UT_CARD_VAL2_BASE1, UT_CARD_VAL2_BASE2, UT_CARD_UNIT_GAP, UT_CARD_BAR_Y, 0, 0, 1, 1, 0, 0};
/* item 156: TEMPLATE 8's cells (the CELL pages): template 2's rows in 116 / 234 x 92 cells */
static const card_geo_t CG8 = {UT_T8_PAD, UT_T8_LBL_BASE, UT_T8_LBL_DX, UT_T8_LBL_PAD_R, UT_T8_VAL_BASE, UT_T8_VAL2_BASE1, UT_T8_VAL2_BASE2, UT_T8_UNIT_GAP, UT_T8_BAR_Y, 0, 1, 1, 1, 1, 0};
/* TEMPLATE 9 (grid_on 16): the area page's card row, the faces of the cells (figures N, else L, else the Bold T) */
static const card_geo_t CGA = {UT_A_PAD, UT_A_LBL_BASE, UT_A_LBL_DX, UT_A_LBL_PAD_R, UT_A_VAL_BASE, UT_A_VAL2_BASE1, UT_A_VAL2_BASE2, UT_A_UNIT_GAP_W, UT_A_BAR_Y, UT_A_PILL_BASE, 1, 1, 1, 1, 1};
/* the ENGINE page (grid_on 5): the same card row; the value's faces as the ENGINE cells had them (figures N, else L, T, S: template 2) */
static const card_geo_t CGE = {UT_A_PAD, UT_E_LBL_BASE, UT_A_LBL_DX, UT_A_LBL_PAD_R, UT_A_VAL_BASE, UT_E_VAL2_BASE1, UT_E_VAL2_BASE2, UT_A_UNIT_GAP_W, UT_A_BAR_Y, UT_A_PILL_BASE, 1, 1, 1, 0, 1};
/* the grid card template of the page now (0: the page's cards are drawn by another painter) */
static const card_geo_t *grid_cg(void)
{
    return grid_on == 1 ? &CG1 : grid_on == 3 ? &CG3 : grid_on == 5 ? &CGE : CELLS_ON ? &CG8 : AREA_ON ? &CGA : 0;
}
#define CARD_SEG_ON (grid_cg() && grid_cg()->seg)     /* the page's cards show a CHOICE as the segment line (draw_columns sets cell_seg_n / cell_seg_i for it) */
/* an area card's label (TEMPLATE 9, section C of the change list): `one` (holds 24) gets the label on one line (a break the label carries, "OFF-\nSCALE", joined: no space after a hyphen);
 * when that does not fit `room`, the label takes two lines, broken where the label says or at the last space whose first line fits, never inside a word: l1 (holds 24) the first line, -> the second;
 * 0: one line (a single word too wide stays one line: tightened or cut by the card's edge, as every label) */
static const char *area_label_lines(const char *label, char *one, char *l1, int32_t room)
{
    uint32_t i, j = 0, brk = 0xFFu, cut = 0;
    for (i = 0; label[i] && j + 1u < 24u; i++) {
        if (label[i] == '\n') {
            brk = i;
            if (j && one[j - 1u] != '-')
                one[j++] = ' ';
            continue;
        }
        one[j++] = label[i];
    }
    one[j] = 0;
    if (!one[0] || text_w(&AF_S, one) <= room)
        return 0;
    if (brk != 0xFFu) {
        str_cpy(l1, label, brk + 1u < 24u ? brk + 1u : 24u);
        return label + brk + 1;
    }
    for (i = 0; one[i]; i++)                           /* the last space whose first line fits */
        if (one[i] == ' ') {
            str_cpy(l1, one, i + 1u);
            if (text_w(&AF_S, l1) <= room)
                cut = i;
        }
    if (!cut)
        return 0;
    str_cpy(l1, one, cut + 1u);
    return label + cut + 1;
}
/* a card unit's face, its gap and its colour: % and every other unit in M (T_MID; the area card row: white, % with no space), "/16" on the area card row in the value's face, white, no space */
static const aafont_t *card_unit(const card_geo_t *g, const aafont_t *vf, const char *unit, int32_t *gap, uint16_t *col)
{
    *gap = g->unit_gap;
    *col = T_MID;
    if (!g->area)
        return &AF_M;
    *col = T_TEXT;
    if (unit[0] == '%' || unit[0] == '/')
        *gap = UT_A_UNIT_GAP;
    return unit[0] == '/' ? vf : &AF_M;
}
static int32_t card_uw(const card_geo_t *g, const aafont_t *vf, const char *unit)
{
    int32_t gap;
    uint16_t col;
    const aafont_t *uf = card_unit(g, vf, unit, &gap, &col);
    return unit[0] ? text_w(uf, unit) + gap : 0;
}
/* the card body of templates 1, 2, 8 and 9 (items 114, 115, 156; the area page), drawn from the UT_CARD_* / UT_T2_* / UT_T8_* / UT_A_* tokens (ui_tokens.h), one function for every page that uses them.
 * Order of the rules: the card is T_BG with hard corners; the dot + the label in S on ONE baseline, never shortened and never ellipsised (a label wider than its room is cut by the card's edge: the pages
 * that do it are listed in HANDOFF_V2 items 114 / 115; the area card row: two lines where it does not fit, section C); the value on ONE baseline for every face: (N for figures,) L, else M, else S (a two-line value in S has its own two baselines and no bar;
 * templates 2, 8 and 9 never ellipsise a value either: too wide it is cut by the card's edge and listed by the lint); the unit on the value's baseline (card_unit); the bar at one row. big (HOME's K1): the value in X,
 * no label, no dot, the bar under it as the others' */
static void card_paint(const card_geo_t *g, uint32_t c, int32_t gx, int32_t gw, int32_t gh, const char *label, const char *vb, const char *val, const char *unit,
                       uint16_t vc, int32_t ratio, int hot, int ctr, int big)
{
    const aafont_t *vf = g->num && card_figure(vb) ? &AF_N : &AF_L;
    int32_t pad = g->pad, room = gw - 2 * pad, uw, x, i, two = 0, kn = -1;
    uint32_t kl;
    uint16_t col;
    if (ux.glob && !ux.bw && vc == T_THEME)           /* a GLOBAL page: the value in the gradient's colour at this card */
        vc = grad_col(gx + gw / 2);
    col = vt(vc, T_BG);
    if (big) {
        int ok = vb[0] != 0;
        for (i = 0; vb[i]; i++)
            ok &= (vb[i] >= '0' && vb[i] <= '9') || vb[i] == 'A' || vb[i] == 'B';
        if (ok) {
            cv_text_fit(pad, UT_TY(X, g->val_base), &AF_X, vb, col, T_BG, room);
            if (!UT_CARD_BIG_BAR)                       /* (the picture FINAL_V1_01 has no bar under the big A) */
                ratio = -1;
        } else
            big = 0;
    }
    if (!big) {
        uint16_t lc = hot ? T_ACCENT : grid_mot ? T_THEME : T_MID;
        int32_t lroom = gw - pad - g->lbl_dx - g->lbl_pad_r;
        if (label[0] && g->area) {                      /* TEMPLATE 9: one line, or two (the dot on the first, one line above the shared baseline; the second line under the label's pen) */
            char one[24], l1[24];
            const char *l2 = area_label_lines(label, one, l1, lroom);
            if (l2) {
                card_label(c, pad, pad + g->lbl_dx, g->lbl_base - UT_A_LBL2_PITCH, l1, lc, lroom);
                text_trk = lbl_trk(pad + g->lbl_dx, &AF_S, l2);
                cv_text_on(pad + g->lbl_dx, UT_TY(S, g->lbl_base), &AF_S, l2, lc, T_BG);
                text_trk = 0;
            } else
                card_label(c, pad, pad + g->lbl_dx, g->lbl_base, one, lc, lroom);
        } else if (label[0])
            card_label(c, pad, pad + g->lbl_dx, g->lbl_base, label, lc, lroom);
        if (g->pill_base)
            kn = kc_tag(vb, &kl);
        if (unit[0] == '/' && vf == &AF_N)              /* (the N face has no slash: "+3/16" in L) */
            vf = &AF_L;
        uw = card_uw(g, vf, unit);
        if (vf == &AF_N && text_w(vf, vb) + uw > room)
            vf = &AF_L;
        uw = card_uw(g, vf, unit);
        if (g->bold) {                                  /* item 158 (owner): a CELL keeps the large face's weight, Bold: L, else the next smaller Bold of the tables, T (15 px); never M / S */
            if (vf == &AF_L && text_w(vf, vb) + uw > room)
                vf = &AF_T;
        } else {
            if (vf == &AF_L && text_w(vf, vb) + uw > room)
                vf = &AF_T;                             /* item 164 (owner rule C2): the same weight, only smaller: Bold 15, never Medium */
            uw = card_uw(g, vf, unit);
            if (text_w(vf, vb) + uw > room)
                vf = &AF_S;
        }
        uw = card_uw(g, vf, unit);
        for (i = 0; val[i] && val[i] != '\n'; i++)
            ;
        if (kn >= 0) {                                  /* "[OCT+]": the action's key pill (accent: it would act; DIM: it would not), its last row on pill_base */
            cv_keycap(pad, g->pill_base - KC_H + 1, (uint32_t)kn, vc == T_ACCENT || vc == T_DIM ? vc : T_KEY, T_KINK, T_BG);
            ratio = -1;
        } else if (vf == &AF_S && text_w(vf, vb) + uw > room && val[i]) {   /* "DOUBLE / SINE": two lines, no bar */
            char v1[PV_LEN];
            str_cpy(v1, val, (uint32_t)i + 1u);
            if (g->cut) {
                cv_text_on(pad, UT_TY(S, g->val2_b1), &AF_S, v1, col, T_BG);
                cv_text_on(pad, UT_TY(S, g->val2_b2), &AF_S, val + i + 1, col, T_BG);
            } else {
                cv_text_fit(pad, UT_TY(S, g->val2_b1), &AF_S, v1, col, T_BG, room);
                cv_text_fit(pad, UT_TY(S, g->val2_b2), &AF_S, val + i + 1, col, T_BG, room);
            }
            two = 1;
        } else if (vb[0]) {
            int32_t ty = card_ty(vf, g->val_base), ug;
            int32_t dx = g->cut && vf == &AF_N ? UT_T2_NUM_DX : 0;
            uint16_t ucol;
            const aafont_t *uf = card_unit(g, vf, unit, &ug, &ucol);
            x = (g->cut ? cv_text_on(pad + dx, ty, vf, vb, col, T_BG) : cv_text_fit(pad, ty, vf, vb, col, T_BG, room - uw)) - dx;
            if (unit[0])
                cv_text_on(x + ug, card_ty(uf, g->val_base), uf, unit, ucol, T_BG);
        }
    }
    if (ratio >= 0 || (g->seg && cell_seg_n > 1u && !big) || (!big && (vb[0] || two))) {   /* (item 156: a CHOICE's segment bar, also under a value of two lines; owner 2026-11-08: EVERY card has its line, an action or a count with nothing to show: the empty track) */
        int32_t bw = gw - 2 * pad, fx = (ratio * bw + 500) / 1000;
        uint16_t gc = hot ? T_ACCENT : vc == T_DIM ? T_DIM : ux.glob && !ux.bw ? grad_col(gx + gw / 2) : T_THEME;
        uint16_t rest = ux_mix(T_BASE, T_THEME, UT_CARD_BAR_REST_PCT);
        cv_rrect(pad, g->bar_y, bw, UT_CARD_BAR_H, UT_CARD_BAR_R, rest, T_BG);
        if (g->seg && cell_seg_n > 1u) {                /* item 156: a CHOICE of N options: a segment 1/N of the track at the option's place (a switch: the left or the right half) */
            int32_t sx = pad + (int32_t)cell_seg_i * bw / (int32_t)cell_seg_n, ex = pad + ((int32_t)cell_seg_i + 1) * bw / (int32_t)cell_seg_n;
            cv_rrect(sx, g->bar_y, ex - sx, UT_CARD_BAR_H, UT_CARD_BAR_R, gc, rest);
        } else if (ratio < 0) {                         /* (owner 2026-11-08: the empty track only) */
        } else if (ctr) {                               /* a bipolar value: from the middle of the track to its side (zero: a 2 px tick) */
            int32_t half = bw / 2, ext = (ratio - 500) * half / 500, x0 = pad + half;
            cv_rrect(ext < 0 ? x0 + ext : x0 - (ext ? 0 : 1), g->bar_y, ext < 0 ? -ext : ext < 2 ? 2 : ext, UT_CARD_BAR_H, UT_CARD_BAR_R, gc, rest);
        } else
            cv_rrect(pad, g->bar_y, fx < UT_CARD_BAR_MIN ? UT_CARD_BAR_MIN : fx, UT_CARD_BAR_H, UT_CARD_BAR_R, gc, rest);
    }
    (void)gh;
    (void)two;
}
/* TEMPLATE 4 card (MIXER, item 117; ui_tokens.h UT_T4_*): the round card, the white numbered dot + the grey label (S) on baseline UT_T4_LBL_BASE, the value (L, else M, else S; cut by the card's edge, never
 * ellipsised) on UT_T4_VAL_BASE with its grey unit (M) on the same baseline, the thin bar at UT_T4_BAR_Y (width card - 2 pad). vc: the part's colour (T_DIM: the part is muted); ctr: a bipolar bar (PAN) fills from
 * its middle and carries a tick there. A label wider than its room is cut by the card's edge and listed (CARD_LABEL_HOOK), never shortened. */
static void card_t4_paint(uint32_t c, int32_t gw, int32_t gh, const char *label, const char *vb, const char *unit, uint16_t vc, int32_t ratio, int hot, int ctr)
{
    const aafont_t *vf = &AF_L;
    int32_t pad = UT_T4_PAD, room = gw - 2 * pad, uw = unit[0] ? text_w(&AF_M, unit) + UT_T4_UNIT_GAP : 0, x;
    uint16_t grey = ux_mix(T_BASE_P1, T_TEXT, UT_GREY_PCT), col = vt(vc, T_BG);
    if (label[0])
        dot_label(c, pad, pad + UT_T4_LBL_DX, UT_T4_LBL_BASE, label, hot ? T_ACCENT : T_TEXT, hot ? T_ACCENT : grey, gw - pad - UT_T4_LBL_DX - 4, T_BG);
    if (text_w(vf, vb) + uw > room)
        vf = &AF_M;
    if (text_w(vf, vb) + uw > room)
        vf = &AF_S;
    if (vb[0]) {
        x = cv_text_on(pad, card_ty(vf, UT_T4_VAL_BASE), vf, vb, col, T_BG);
        if (unit[0])
            cv_text_on(x + UT_T4_UNIT_GAP, UT_TY(M, UT_T4_VAL_BASE), &AF_M, unit, grey, T_BG);
    }
    if (ratio >= 0) {
        int32_t bw = gw - 2 * pad, fx = (ratio * bw + 500) / 1000;
        uint16_t gc = hot ? T_ACCENT : vc, rest = ux_mix(T_BASE_P1, T_TEXT, UT_T4_TRACK_PCT);
        cv_rrect(pad, UT_T4_BAR_Y, bw, UT_T4_BAR_H, UT_T4_BAR_R, rest, T_BG);
        if (ctr) {                                      /* the middle: a tick, the fill goes from it to the value */
            int32_t half = bw / 2, ext = (ratio - 500) * half / 500, x0 = pad + half;
            cv_rect(x0, UT_T4_BAR_Y - UT_T4_TICK_DY, 1, UT_T4_BAR_H + 2 * UT_T4_TICK_DY, grey);
            if (ext)
                cv_rrect(ext < 0 ? x0 + ext : x0, UT_T4_BAR_Y, ext < 0 ? -ext : ext, UT_T4_BAR_H, UT_T4_BAR_R, gc, rest);
        } else
            cv_rrect(pad, UT_T4_BAR_Y, fx < UT_T4_BAR_MIN ? UT_T4_BAR_MIN : fx, UT_T4_BAR_H, UT_T4_BAR_R, gc, rest);
    }
}
/* TEMPLATE 7 card (item 118; ui_tokens.h UT_T7_*, FINAL_V1_07_sequencer): the STEP page's STEP / NOTE / LENGTH / FLAG, compact: the numbered dot + the label (S) on baseline UT_T7_LBL_BASE, the value (L, else M, else S; cut by the card's
 * edge, never ellipsised) on UT_T7_VAL_BASE with its unit (M) on the same baseline; no bar. A label wider than its room is cut by the card's edge and listed (CARD_LABEL_HOOK), never shortened */
static void card_t7_paint(uint32_t c, int32_t gw, int32_t gh, const char *label, const char *vb, const char *unit, uint16_t vc, int hot)
{
    const aafont_t *vf = &AF_L;
    int32_t room = gw - UT_T7_VAL_X - UT_T7_VAL_PAD_R, uw = unit[0] ? text_w(&AF_M, unit) + UT_CARD_UNIT_GAP : 0, x;
    if (label[0])
        card_label(c, UT_T7_DOT_X, UT_T7_LBL_DX, UT_T7_LBL_BASE, label, hot ? T_ACCENT : T_MID, gw - UT_T7_LBL_DX - 3);
    if (text_w(vf, vb) + uw > room)
        vf = &AF_M;
    if (text_w(vf, vb) + uw > room)
        vf = &AF_S;
    if (vb[0]) {
        x = cv_text_on(UT_T7_VAL_X, card_ty(vf, UT_T7_VAL_BASE), vf, vb, vt(vc, T_BG), T_BG);
        if (unit[0])
            cv_text_on(x + UT_CARD_UNIT_GAP, UT_TY(M, UT_T7_VAL_BASE), &AF_M, unit, T_MID, T_BG);
    }
}
/* TEMPLATE 9 (grid_on 16): the knob-1 LINE of the area page, on the canvas of rows UT_A_K1_Y .. + UT_A_K1_H (240 wide; begun by the caller). No card, no bar, everything on ONE baseline UT_A_K1_BASE, left aligned:
 * the numbered dot 1 and the label (S, T_MID; hot: the accent), the value (S, its colour vc) UT_A_K1_VAL_GAP after the label, its unit (white: "/16" with no space, any other unit one space after it),
 * then the text `after` (S, colour ac): UT_A_K1_AFTER_GAP after the value, or (after_r) flush right ending at UT_A_K1_R; a right text that would touch the value is not drawn */
static void k1_line(const char *label, const char *value, const char *unit, const char *after, uint16_t vc, uint16_t ac, int after_r, int hot)
{
    int32_t base = UT_A_K1_BASE - UT_A_K1_Y, ty = UT_TY(S, base), x = UT_A_K1_LBL_X;
    uint16_t lc = hot ? T_ACCENT : grid_mot ? T_THEME : T_MID;
    if (label[0]) {
        dot_label(0, UT_A_K1_DOT_X, UT_A_K1_LBL_X, base, label, lc, lc, 240 - UT_A_K1_LBL_X, T_BG);
        x += text_w(&AF_S, label) + UT_A_K1_VAL_GAP;
    }
    if (ux.glob && !ux.bw && vc == T_THEME)           /* a GLOBAL page: the value in the gradient's colour at its place */
        vc = grad_col(x);
    if (value[0])
        x = cv_text_on(x, ty, &AF_S, value, vt(vc, T_BG), T_BG);
    if (unit[0])
        x = cv_text_on(x + (unit[0] == '/' ? 0 : text_w(&AF_S, " ")), ty, &AF_S, unit, T_TEXT, T_BG);
    if (after && after[0]) {
        if (!after_r)
            cv_text_on(x + UT_A_K1_AFTER_GAP, ty, &AF_S, after, ac, T_BG);
        else if (x + UT_A_K1_AFTER_GAP <= UT_A_K1_R - text_w(&AF_S, after))
            cv_text_r(UT_A_K1_R, ty, &AF_S, after, ac, T_BG);
    }
}
/* the text after the knob-1 line's value of the page now (TEMPLATE 9): b holds 24; -> b, or 0 for none. ac: its colour, *right: 1 = flush right.
 * PERFORM: SELECT = BALANCE (what SELECT does here; it was the footer's first row). CHORD: the chord's name (Bm7b5: the last chord played, else the chord on the scale's root, as the old picture's name).
 * ARP 1/3 and 2/3: EXAMPLE: C E G at the right edge while the pattern is drawn from the example notes (no key held), as the old picture's text */
static const char *k1_after_of(char *b, uint16_t *ac, int *right)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    *ac = T_TEXT;
    *right = 0;
    if (k1_after_next) {                                /* (set by draw_columns for the next line) */
        *ac = k1_after_col;
        *right = k1_after_r;
        str_cpy(b, k1_after_next, 24);
        return b;
    }
    if (pg->graph == GR_CHORD && t->p[P_CHRD]) {
        uint8_t nn[CHORD_MAX];
        int32_t r;
        uint16_t mask;
        uint32_t k = trk_index(t);
        if (chord_last[k].n) {
            r = chord_last[k].root;
            mask = chord_last[k].mask;
        } else
            (void)chord_make(t, (uint32_t)(60 + root_of(t)), nn, &r, &mask);
        chord_name(b, (uint32_t)r, mask);
        return b;
    }
    if (pg->graph == GR_ARP && arp_drawn(t) && !t->nheld) {
        *right = 1;
        str_cpy(b, "EXAMPLE: C E G", 24);
        return b;
    }
    return 0;
}
#define room_scale_card (COL_W - 8)
static void draw_card_grid(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc, int32_t ratio, int hot, int ctr)
{
    char lb[24], vb[PV_LEN], dg[2], key[16];
    uint32_t i, h;
    const aafont_t *vf = &AF_L;
    int32_t x, uw = unit[0] ? text_w(&AF_S, unit) : 0, vy, gx, gy, gw, gh, vcy = grid_on == 2 ? 36 : grid_on == 5 ? 38 : 30;
    const char *desc = grid_d;
    int big1 = grid_on == 1 && grid_big;                /* (template 1: taken at once, so that a card drawn from the cache never leaves it set for the next) */
    grid_d = 0;
    if (grid_on == 1)
        grid_big = 0;
    if (grid_on == 2 && grid_wide && c == 3u)
        return;
    char ab[24];
    const char *after = 0;
    uint16_t acol = T_TEXT;
    int aright = 0;
    if (AREA_ON && c == 0u)                             /* (TEMPLATE 9: the text after knob 1's value, taken for this line only) */
        after = k1_after_of(ab, &acol, &aright);
    k1_after_next = 0;
    grid_geo(c, &gx, &gy, &gw, &gh);
    if (!gw)                                            /* (item 156: a column with no cell on a CELL page draws nothing; the page was drawn whole when its cells last moved; TEMPLATE 9: a column with no card) */
        return;
    if (ENG_LIST_ON && cell_at[c & 3u] == 0u)           /* (item 165: the engine list covers the top cell while K1 turns; the cell is drawn again when it ends) */
        return;
    for (i = 0; label[i] && i + 1u < sizeof lb; i++)
        lb[i] = label[i] == '\n' ? ' ' : label[i];
    lb[i] = 0;
    for (i = 0; val[i] && i + 1u < sizeof vb; i++)
        vb[i] = val[i] == '\n' ? ' ' : val[i];
    vb[i] = 0;
    h = str_hash(str_hash(str_hash(str_hash(0x51u + c, lb), vb), unit), desc ? desc : "") + (uint32_t)vc * 7u + (uint32_t)(ratio + 2) * 131u + (uint32_t)hot * 17u + (uint32_t)ctr * 5u + (uint32_t)grid_mot * 29u + ux.gen * 3u + (uint32_t)gw;
    if (CELLS_ON || AREA_ROW_ON)                        /* (item 156: the cell's place and the choice's segment are in the key too; TEMPLATE 9: the card's place, the two-line row, the text after knob 1's value) */
        h += (uint32_t)gx * 101u + (uint32_t)gy * 7919u + (uint32_t)cell_seg_n * 977u + (uint32_t)cell_seg_i * 31u + (uint32_t)AREA_TWO * 3331u;
    if (after)
        h = str_hash(h + (uint32_t)acol * 13u + (uint32_t)aright * 7u, after);
    for (i = 0; i < 8u; i++)
        key[i] = (char)('A' + ((h >> (4u * i)) & 15u));
    key[8] = 0;
    if (!ui.force && str_eq(ui.col[c], key))
        return;
    str_cpy(ui.col[c], key, sizeof ui.col[c]);
    cv_begin((uint32_t)gw, (uint32_t)gh, T_BG);
    if (!label[0] && !vb[0]) {                          /* no card here: the background */
        cv_blit((uint32_t)gx, (uint32_t)gy);
        return;
    }
    if (AREA_ON && c == 0u) {                           /* TEMPLATE 9: knob 1's line */
        k1_line(lb, vb, unit, after, vc, acol, aright, hot);
        cv_blit((uint32_t)gx, (uint32_t)gy);
        return;
    }
    if (AREA_ROW_ON) {                                  /* TEMPLATE 9 and the ENGINE page: a card of the area card row (its rows one label line lower inside the card when the page has a two-line label: the card starts that much higher) */
        card_geo_t ga = grid_on == 5 ? CGE : CGA;
        if (AREA_TWO) {
            ga.lbl_base = (int16_t)(ga.lbl_base + UT_A_LBL2_PITCH);
            ga.val_base = (int16_t)(ga.val_base + UT_A_LBL2_PITCH);
            ga.val2_b1 = (int16_t)(ga.val2_b1 + UT_A_LBL2_PITCH);
            ga.val2_b2 = (int16_t)(ga.val2_b2 + UT_A_LBL2_PITCH);
            ga.bar_y = (int16_t)(ga.bar_y + UT_A_LBL2_PITCH);
            ga.pill_base = (int16_t)(ga.pill_base + UT_A_LBL2_PITCH);
        }
        card_paint(&ga, c, gx, gw, gh, label[0] ? label : "", vb, val, unit, vc, ratio, hot, ctr, 0);
        cv_blit((uint32_t)gx, (uint32_t)gy);
        return;
    }
    if (grid_on == 1 || grid_on == 3) {                 /* TEMPLATE 1: the design's card (item 114); item 120: the MODE page's three cards with template 1's rows (CG3) */
        card_paint(grid_on == 1 ? &CG1 : &CG3, c, gx, gw, gh, label[0] ? lb : "", vb, val, unit, vc, ratio, hot, ctr, big1);
        cv_blit((uint32_t)gx, (uint32_t)gy);
        return;
    }
    if (CELLS_ON) {                                     /* TEMPLATE 8: a cell of the CELL pages (item 156; item 165: the ENGINE page of LOFI / NOISE) */
        card_paint(&CG8, c, gx, gw, gh, label[0] ? lb : "", vb, val, unit, vc, ratio, hot, ctr, 0);
        cv_blit((uint32_t)gx, (uint32_t)gy);
        return;
    }
    if (grid_on == 7) {                                 /* TEMPLATE 4: the MIXER's LEVEL / PAN card (item 117) */
        card_t4_paint(c, gw, gh, lb, vb, unit, vc, ratio, hot, ctr);
        cv_blit((uint32_t)gx, (uint32_t)gy);
        return;
    }
    if (grid_on == 10) {                                /* TEMPLATE 7: a card of the STEP page (item 118) */
        card_t7_paint(c, gw, gh, lb, vb, unit, vc, hot);
        cv_blit((uint32_t)gx, (uint32_t)gy);
        return;
    }
    if (grid_big) {
        int ok = 1;
        grid_big = 0;
        for (i = 0; vb[i]; i++)
            ok &= (vb[i] >= '0' && vb[i] <= '9') || vb[i] == 'A' || vb[i] == 'B';
        if (ok && vb[0]) {
            cv_text_fit(4, gh / 2 + 13 - AF_X.asc, &AF_X, vb, vt(vc, T_BG), T_BG, gw - 14);
            cv_blit((uint32_t)gx, (uint32_t)gy);
            return;
        }
    }
    if (label[0]) {
        uint16_t mk = hot ? T_ACCENT : T_TEXT;
        dg[0] = (char)('1' + c);
        dg[1] = 0;
        cv_rrect(4, 6, 11, 11, 5, mk, T_BG);
        cv_text_in(4, 6 + HALF_UP(11 - AF_D_CAP_H) - AF_D_CAP_Y, 11, &AF_DOT, dg, T_BG, mk);
        {   /* a label wider than its room breaks at its last space into two lines (PULSE / WIDTH) */
            uint16_t lcol = hot ? T_ACCENT : ux_mix(T_BASE, T_TEXT, 44);   /* (the approved card label: the text at 44 % over the card, PALETTES_V3: (112,105,112) in VIOLET) */
            int32_t cut = -1, ly = 11 - AF_S_CAP_Y - AF_S_CAP_H / 2;
            for (i = 0; lb[i]; i++)
                if (lb[i] == ' ')
                    cut = (int32_t)i;
            if (text_w(&AF_S, lb) > gw - 26 && cut > 0) {
                lb[cut] = 0;
                cv_text_fit(19, ly - 6, &AF_S, lb, lcol, T_BG, gw - 26);
                cv_text_fit(19, ly + 7, &AF_S, lb + cut + 1, lcol, T_BG, gw - 26);
            } else if (text_w(&AF_S, lb) > gw - 26)           /* one long word: on a line of its own under the knob mark */
                cv_text_fit(4, ly + 13, &AF_S, lb, lcol, T_BG, gw - 12);
            else
                cv_text_fit(19, ly, &AF_S, lb, lcol, T_BG, gw - 26);
        }
    }
    if (text_w(vf, vb) + uw > gw - 14)
        vf = &AF_M;
    if (text_w(vf, vb) + uw > gw - 14)
        vf = &AF_S;
    for (i = 0; val[i] && val[i] != '\n'; i++)
        ;
    if (vf == &AF_S && text_w(vf, vb) + uw > gw - 14 && val[i]) {   /* a two-line value ("DOUBLE / SINE") in two small lines */
        char v1[PV_LEN];
        const char *v2 = val + i + 1;
        str_cpy(v1, val, i + 1u);
        cv_text_fit(4, vcy - AF_S_CAP_Y - AF_S_CAP_H / 2 - 7, &AF_S, v1, vt(vc, T_BG), T_BG, gw - 14);
        cv_text_fit(4, vcy - AF_S_CAP_Y - AF_S_CAP_H / 2 + 6, &AF_S, v2, vt(vc, T_BG), T_BG, gw - 14);
        vb[0] = 0;
    }
    vy = vf == &AF_L ? vcy - AF_L_CAP_Y - AF_L_CAP_H / 2 : vf == &AF_M ? vcy - AF_M_CAP_Y - AF_M_CAP_H / 2 : vcy - AF_S_CAP_Y - AF_S_CAP_H / 2;
    if (ux.glob && !ux.bw && vc == T_THEME)           /* a GLOBAL page: the value in the gradient's colour at this card */
        vc = grad_col(gx + gw / 2);
    x = cv_text_fit(4, vy, vf, vb, vt(vc, T_BG), T_BG, vf == &AF_S && text_w(vf, vb) + uw <= gw - 8 ? gw - 8 - uw : gw - 14 - uw);   /* (the small face may use 6 px more of the margin before a word is cut) */
    if (unit[0])
        cv_text_on(x, vy + vf->asc - AF_S.asc, &AF_S, unit, T_MID, T_BG);
    if (desc && desc[0])
        guide_wrap(4, vcy + 10, gw - 14, &AF_M, desc, T_TEXT, 14);
    if (ratio >= 0) {
        int32_t bw = gw - 8, fx = ratio * bw / 1000, by = grid_on == 2 ? gh - 4 : grid_on == 5 ? gh - 12 : gh - 10;
        uint16_t gc = hot ? T_ACCENT : vc == T_DIM ? T_DIM : ux.glob && !ux.bw ? grad_col(gx + gw / 2) : T_THEME;
        if (ctr) {
            int32_t half = bw / 2, ext = (ratio - 500) * half / 500, x0 = 4 + half;
            cv_rrect(ext < 0 ? x0 + ext : x0 - (ext ? 0 : 1), by, ext < 0 ? -ext : ext < 2 ? 2 : ext, 3, 1, gc, T_BG);
        } else
            cv_rrect(4, by, fx < 3 ? 3 : fx, 3, 1, gc, T_BG);
    }
    cv_blit((uint32_t)gx, (uint32_t)gy);
}
/* owner 2026-11-08: a switch's or a turn's segment line for the next card (n options, the lit one i; 0: none) on the card pages that draw it */
static void seg_next(uint32_t n, int i)
{
    cell_seg_n = (uint8_t)(CARD_SEG_ON ? n : 0u);
    cell_seg_i = (uint8_t)(n ? i : 0);
}
static void draw_column(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc,
                        int32_t ratio, uint32_t icon)
{
    if (grid_on == 4 || (grid_on == 3 && c == 0u) || (grid_on == 6 && c >= 2u)) {    /* (the MODE list is K1's; the MATRIX rows draw all four knobs themselves) */
        card_hide_next = 0;
        card_mk_next = card_mot_next = card_ctr_next = 0;
        return;
    }
    char key[64], l1[16];
    const char *l2 = label_split(label, l1);            /* (0.2.25: a label of two lines) */
    char v1[PV_LEN];
    const char *v2 = 0;                                 /* (T0229: a value of two lines, "FILTER\nRES": the label's line, then the value's two lines in S) */
    int hot = c == ui.hot_col && ui.hot_t, named = fmt_named, mot = card_mot_next && label[0], strip, snap;
    uint8_t sig;
    uint16_t lc = hot ? T_ACCENT : T_MID;
    int32_t x, lx = 5, uw, room = COL_W - 8;
    const aafont_t *vf = &AF_M;
    uint32_t n, kn;
    int32_t kid = kc_tag(val, &kn);
    int mk = card_mk_next;                              /* MORPH moves this card's value (card_vv) */
    int ctr = card_ctr_next && ratio >= 0;              /* the gauge fills from the centre */
    uint16_t gcol = T_THEME;                            /* the gauge's fill; on a GLOBAL page the gradient's colour at this card (a colour per column, owner item 78) */
    if (ux.glob && !ux.bw) {
        gcol = grad_col(CARD_X(c) + COL_W / 2);
        if (vc == T_THEME)
            vc = gcol;
    }
    card_ctr_next = 0;
    const char *why = card_why_next;                    /* the CLK card's state line (clock_why) */
    card_why_next = 0;
    if (card_hide_next) {                               /* draw_column_hide: the column is left empty (the background only) */
        card_hide_next = 0;
        card_mk_next = 0;
        card_mot_next = 0;
        fmt_named = 0;
        card_mot = (uint8_t)(card_mot & ~(1u << c));
        if (!ui.force && str_eq(ui.col[c], "\x7f"))
            return;
        str_cpy(ui.col[c], "\x7f", sizeof ui.col[c]);
        ui.roll[c].from[0] = 0;
        if (grid_on) {
            int32_t hx, hy, hw, hh;
            if (grid_on == 2 && grid_wide && c == 3u)   /* (the wide card 3 covers it) */
                return;
            grid_geo(c, &hx, &hy, &hw, &hh);
            if (!hw)                                    /* (item 156: a hidden card has no cell on a CELL page; another card's cell may be where it was; TEMPLATE 9: a column with no card) */
                return;
            cv_begin((uint32_t)hw, (uint32_t)hh, T_BG);
            cv_blit((uint32_t)hx, (uint32_t)hy);
            return;
        }
        cv_begin(COL_W, COL_H, T_BG);
        cv_blit((uint32_t)CARD_X(c), Y_LABEL);
        return;
    }
    if (grid_on) {                                      /* DUELL 1.1: the grid card */
        int hot_g = c == ui.hot_col && ui.hot_t, ctr_g = ctr;   /* (item 118: `ctr` was taken from card_ctr_next BEFORE it was cleared; the flag is set only by the true bipolar cards: OFFSET, TUNE, FDBK, BLEND, PAN) */
        if (named && ((grid_on == 1 && !UT_CARD_ENUM_BAR) || (grid_on == 5 && !UT_T2_ENUM_BAR)))                 /* template 1 (FINAL_V1_06_lfo): a name (WAVE SINE, SYNC OFF) has no bar under it, only a number does */
            ratio = -1;
        if (named && CARD_SEG_ON)                       /* item 156, TEMPLATE 8 (and 9, the ENGINE page): a name has the CHOICE's segment bar (cell_seg_n), no value bar (a list of one option: none) */
            ratio = -1;
        fmt_named = 0;
        card_mk_next = 0;
        card_mot_next = 0;
        card_ctr_next = 0;
        card_why_next = 0;
        card_mot = (uint8_t)((card_mot & ~(1u << c)) | (uint32_t)mot << c);   /* (item 120: MOTION drives this card: its dot and label take the theme colour, the old cards' fallback; was dropped by the grid pages) */
        grid_mot = (uint8_t)mot;
        if (str_eq(unit, label))
            unit = "";
        draw_card_grid(c, label, val, unit, vc, ratio, hot_g, ctr_g);
        grid_mot = 0;
        return;
    }
    fmt_named = 0;                                      /* (params.c: a name, for this card only) */
    card_mk_next = 0;
    card_mot_next = 0;                                  /* (the same: MOTION, this card only) */
    card_mot = (uint8_t)((card_mot & ~(1u << c)) | (uint32_t)mot << c);
    if (str_eq(unit, label))
        unit = "";                                      /* "BPM 124 BPM", "USB OFF USB": the label says it */
    icon = ICON_NONE;                                   /* DUELL 1.1 (owner): no icon before a label; the knob's number in its white dot instead (drawn below) */
    if (l2) {                                           /* two lines: no icon, the value on the third line in S, no gauge */
        icon = ICON_NONE;
        ratio = -1;
        why = 0;
    }
    if (!l2) {
        uint32_t i;
        for (i = 0; val[i] && i < PV_LEN - 1u; i++)
            if (val[i] == '\n') {
                str_cpy(v1, val, i + 1u);
                v2 = val + i + 1;
                ratio = -1;
                why = 0;
                break;
            }
    }
    str_cpy(key, label, 12);                            /* cache key: texts + colour + gauge */
    str_cpy(key + str_len(key), "|", 2);
    str_cpy(key + str_len(key), val, PV_LEN);
    str_cpy(key + str_len(key), "|", 2);
    str_cpy(key + str_len(key), unit, 8);
    n = str_len(key);
    /* every RGB565 bit: status colours can change with identical text */
    key[n] = (char)('A' + (vc & 15u));
    key[n + 1] = (char)('A' + ((vc >> 4) & 15u));
    key[n + 2] = (char)('A' + ((vc >> 8) & 15u));
    key[n + 3] = (char)('A' + (vc >> 12));
    key[n + 4] = (char)(' ' + (ratio < 0 ? 0 : 1 + ratio * (COL_W - 10) / 1000));   /* the gauge's own pixel (as
                                                        * drawn: gw; ratio / 20 missed a pixel inside a step, #63 soak) */
    key[n + 5] = (char)(icon == ICON_NONE ? '~' : '!' + icon % 90u);
    key[n + 6] = (char)('0' + hot + 2 * mot + 16 * mk + 32 * ctr);
    key[n + 7] = (char)(why ? '#' + (int)(str_hash(5u, why) & 31u) : '-');   /* (the reason, as its own mark) */
    key[n + 8] = (char)(why ? '#' + (int)((str_hash(5u, why) >> 5) & 31u) : '-');   /* (0.2.21: ten bits of it: SEND FX's second lines, "71 %", "72 %", are many) */
    key[n + 9] = 0;
    uw = unit[0] ? text_w(&AF_S, unit) : 0;
    if (!v2 && !l2 && kid < 0 && !why && text_w(&AF_S, val) + uw > room) {   /* item 120: a value too wide even in S breaks at the first space that leaves both lines whole (UP AND / DOWN), never ellipsised */
        uint32_t i;
        for (i = 0; val[i] && i + 1u < PV_LEN; i++)
            if (val[i] == ' ') {
                str_cpy(v1, val, i + 1u);
                if (text_w(&AF_S, v1) <= room && text_w(&AF_S, val + i + 1) + uw <= room) {
                    v2 = val + i + 1;
                    ratio = -1;
                    break;
                }
            }
    }
    if (why)
        vf = &AF_S;                                     /* (faded value small, the reason under it) */
    if (v2 || text_w(vf, val) + uw > room || l2) {
        vf = &AF_S;
        if (!l2)
            CARD_HOOK(val, unit);                       /* (host: the values set in S) */
    }
    sig = (uint8_t)str_hash(str_hash(song.sel + TSEL->eng_req * 4u + ux.gen * 64u, label), unit);
    snap = ui.force || sig != ui.roll[c].sig;           /* what the value is of: label, unit, track, engine, palette */
    strip = !snap && str_eq(key, ui.col[c]);
    if (strip && !ui.roll[c].from[0])
        return;
    if (strip) {                                        /* rolling: the value strip only */
        cv_begin(COL_W, ROLL_H, T_BG);
        cv_oy = -ROLL_Y;
    } else {
        char ov[PV_LEN];                                  /* the value drawn before (in the cache key) */
        const char *k = ui.col[c];
        uint32_t i = 0;
        while (*k && *k != '|')
            k++;
        while (*k && k[1] && k[1] != '|' && i + 1u < sizeof ov)
            ov[i++] = *++k;
        ov[i] = 0;
        ui.roll[c].sig = sig;
        if (!str_eq(ov, val))
            roll_note(c, ov, val, snap || named || vf != &AF_M || kid >= 0);
        else if (snap)
            ui.roll[c].from[0] = 0;
        str_cpy(ui.col[c], key, sizeof ui.col[c]);
        cv_begin(COL_W, COL_H, T_BG);
    }
    if (label[0] || val[0]) {
        if (!strip && label[0]) {                       /* the numbered white dot, the label after it; a single word too long for the room beside the dot takes the whole line (full word, never an ellipsis) */
            char dg[2] = {(char)('1' + c), 0};
            int wide = text_w(&AF_S, l2 ? (text_w(&AF_S, l1) > text_w(&AF_S, l2) ? l1 : l2) : label) > COL_W - 19;
            if (!wide) {
                cv_rrect(4, 4, 11, 11, 5, hot ? T_ACCENT : T_TEXT, T_BG);
                cv_text_in(4, 4 + HALF_UP(11 - AF_D_CAP_H) - AF_D_CAP_Y, 11, &AF_DOT, dg, T_BG, hot ? T_ACCENT : T_TEXT);
                lx = 17;
            } else
                lx = 5;
        }
        if (!strip && l2) {                             /* 0.2.25: the label's two lines (rows 3 and 16), the value under them */
            if (mot)
                lc = hot ? T_ACCENT : T_THEME;          /* (no room for MOTION's icon: the label says it) */
            card_label_line(c, lx, 2, l1, hot, lc);
            card_label_line(c, lx, 15, l2, hot, lc);
        } else if (!strip && label[0]) {
            const aafont_t *lf = &AF_S;                 /* the label in S */
            int32_t lw = text_w(lf, label), ix = lx, ly = v2 ? 2 : 3;   /* (item 120: a two-line value takes the card's rows 15 and 28, the label one row up) */
            if (lw > COL_W - 2 - lx)                    /* (cut by the card's edge, never ellipsised; listed) */
                CARD_LABEL_HOOK(c, label, lw, COL_W - 2 - lx);
            int32_t cy = ly + AF_S_CAP_Y, chh = AF_S_CAP_H;
            uint32_t ic = ICON_NONE;                       /* (DUELL 1.1: no icon before a label, not even MOTION's: the label shows in the theme colour) */
            if (mot && ix + ICON_CELL + ICON_GAP + lw > COL_W - 2)
                ix = COL_W - 2 - lw - ICON_GAP - ICON_CELL;   /* a long label: MOTION's icon moves left */
            if (FELUCCA_ICONS && ic != ICON_NONE && ix >= 1 && ix + ICON_CELL + ICON_GAP + lw <= COL_W - 2) {
                GFX_HOOK_ALIGN(ix, cy, ix, cy + chh, AL_V, "card icon on its label's line");
                lx = ix + cv_icon_in(ix, cy, 0, chh, ICON_CELL, ic,   /* its ink on the label's line */
                                     mot ? (hot ? T_ACCENT : T_THEME) : lc, T_BG) + ICON_GAP;
            }
            else if (mot)                               /* the label from x 19 (5 + 12 + 2) */
                lc = hot ? T_ACCENT : T_THEME;          /* no room for MOTION's icon: the label says it */
            text_trk = lbl_trk(lx, lf, label);          /* (item 158: tightened only when it would reach the card's edge) */
            if (hot && T_ACCENT == T_MID) {             /* MONO: no colour tells the hot knob: its label inverted, */
                int32_t b[4], w;                        /* the chip 2 px round its ink (M: 3 above, 2 below) */
                text_ink(lf, label, b);
                w = b[2] - b[0] + 4;
                if (w > COL_W + 1 - lx - b[0]) w = COL_W + 1 - lx - b[0];
                cv_rrect(lx + b[0] - 2, cy - 3, w, 15, 3, T_ACCENT, T_BG);
                GFX_HOOK_ALIGN(lx + b[0] - 2, cy - 3, lx + b[0] - 2 + w, 18, AL_HV,
                               "card label chip (MONO hot)");
                cv_text_on(lx, ly, lf, label, T_INK, T_ACCENT);
            } else
                cv_text_on(lx, ly, lf, label, lc, T_BG);
            text_trk = 0;
        }
        if (v2) {                                       /* T0229: the value's two lines (rows 15 and 28; item 120: one row up, so that the 11 px capitals end on 42 of the 44 rows) */
            if (text_w(vf, v1) > room)
                CARD_VALUE_HOOK(c, v1, text_w(vf, v1), room);
            if (text_w(vf, v2) + uw > room)
                CARD_VALUE_HOOK(c, v2, text_w(vf, v2) + uw, room);
            cv_text_on(5, 15, vf, v1, vt(vc, T_BG), T_BG);
            x = cv_text_on(5, 28, vf, v2, vt(vc, T_BG), T_BG);
        } else if (kid >= 0)                            /* "[OCT+]": the keycap (accent: it would act; DIM: it would not) */
            x = cv_keycap(5, 20, (uint32_t)kid, vc == T_ACCENT || vc == T_DIM ? vc : T_KEY, T_KINK, T_BG);
        else if (vf == &AF_M) {
            if (!ui.roll[c].from[0] && val[0])
                GFX_HOOK_ALIGN(0, ROLL_Y, COL_W, ROLL_Y + ROLL_H, AL_V, "card value in its roll window");
            x = roll_text(c, 5, 17, val, vt(vc, T_BG));
        } else {
            if (text_w(vf, val) + uw > room)            /* (item 120: cut by the card's edge, never ellipsised; listed) */
                CARD_VALUE_HOOK(c, val, text_w(vf, val) + uw, room);
            x = cv_text_on(5, l2 ? 28 : why ? 17 : 20, vf, val, vt(vc, T_BG), T_BG);   /* (a faded card's value a little higher, its reason under it; two label lines: the third line, row 28) */
        }
        if (unit[0]) {
            GFX_HOOK_ALIGN(0, 0, 0, vf == &AF_M ? 17 + AF_M.asc : (l2 || v2 ? 28 : 20) + AF_S.asc, AL_B, "card unit on its value's baseline");
            cv_text_on(x, l2 || v2 ? 28 : 20, &AF_S, unit, T_MID, T_BG);     /* on the value's baseline (S) */
        }
        if (!strip && why) {                            /* the reason where the gauge would be (its capitals rows 31 .. 39) */
            GFX_HOOK_ALIGN(0, 28, 0, 28 + AF_S.asc, AL_B, "card reason under its value");
            cv_text_fit(5, 28, &AF_S, why, T_MID, T_BG, room + 1);   /* (0.2.17: 50 px, so that FACTORY, 50 wide, fits whole) */
        }
        if (!strip && ratio >= 0 && !why) {
            int32_t gw = COL_W - 10, fx = ratio * gw / 1000;
            if (ctr) {                                  /* a bipolar value: from the middle of the track to its side (zero: a 2 px tick) */
                int32_t half = gw / 2, ext = (ratio - 500) * half / 500, x0 = 5 + half;
                cv_rrect(ext < 0 ? x0 + ext : x0 - (ext ? 0 : 1), 38, ext < 0 ? -ext : ext < 2 ? 2 : ext, 3, 1, hot ? T_ACCENT : vc == T_DIM ? T_DIM : gcol, T_BG);
            } else
            cv_rrect(5, 38, fx < 3 ? 3 : fx, 3, 1, hot ? T_ACCENT : vc == T_DIM ? T_DIM : gcol, T_BG);
        }
        if (!strip && mk)                               /* MORPH moves it: a 7 x 2 tick in the accent under the gauge */
            cv_rrect(5, 42, 7, 2, 1, T_ACCENT, T_BG);
    }
    cv_oy = 0;
    cv_blit((uint32_t)CARD_X(c), Y_LABEL + (strip ? ROLL_Y : 0));
}

/* the CLK / TEMPO cards' state line with an external clock (midi_clock.c clock_state): the source while pulses arrive, else NO CLK
 * (the arp and the sequencer wait for it); 0 with the internal clock (no line). CLK AUTO (0.2.14): the CLK card (clk_card; its value says
 * AUTO) shows the source it plays on, INT USB or TRS; the TEMPO card USB / TRS while external (never NO CLK: AUTO falls back to INT;
 * "AUTO USB" is 57 px, the line has 49) */
static const char *clock_why(int clk_card)
{
    uint32_t st = clock_state(), s = clock_src();
    if (song.g[G_CLOCK] == CLK_AUTO && clk_card)
        return val_shown(&GP[G_CLOCK], GP[G_CLOCK].names[s]);   /* (owner 2026-10-25: TRS MIDI) */
    return !st ? 0 : st == 2u ? "NO CLK" : s == 1u ? "USB" : "TRS MIDI";
}

/* an inapplicable card: not drawn (card_hide_next above) */
static void draw_column_hide(uint32_t c)
{
    card_hide_next = 1;
    draw_column(c, "", "", "", T_THEME, -1, ICON_NONE);
}
/* The TWIN FILTER's hide-inert rules (cloud job H, FILTER_PAGES_PROPOSAL 5.2): TYPE OFF: filter 2 does nothing, so its TYPE card is the only one that shows (LINK, CUTOFF 2, RES 2, DRIVE, BLEND, ROUTE and the
 * four amounts have nothing to act on); COMB: LINK (the comb follows the key by itself) and KEY TRACK hide. A value of 0 never hides; filter 1's cards never hide */
static int twin_card_hidden(const track_t *t, const int16_t *vp)
{
    if (vp == &t->p[P_CUT2] || vp == &t->p[P_RES2] || vp == &t->p[P_DRV2] || vp == &t->p[P_BLEND])
        return !twin_on(t);
    if (vp == &XV(t, X_E2BLD) || vp == &XV(t, X_E2DRV) || vp == &XV(t, X_CYCUT2) || vp == &XV(t, X_L2RES2) || vp == &XV(t, X_L2BLD))   /* DUELL 1.1: the DEST pages' TWIN cards act only with the TWIN FILTER on */
        return !twin_on(t);
    if (vp >= &XV(t, X_ROUTE) && vp <= &XV(t, X_LFO2))
        return !twin_on(t) || (t->p[P_TWTYPE] == TT_COMB && (vp == &XV(t, X_LINK) || vp == &XV(t, X_KTRK2)));
    return 0;
}
/* The TWIN FILTER's cards where the generic one is not enough (cloud job H): DRIVE says OFF at 0 (really not run), BLEND names its ends and its middle (MULTI at -64, BOTH at 0, TWIN at +63, the name and a percent
 * between), TYPE is a list (no gauge that pretends to be an amount); OFFSET, TUNE, FDBK and BLEND are true bipolar: their gauge fills from the centre (card_ctr_next). val / unit as param_format left them;
 * -> the gauge's ratio (-1: none), or -2 when the generic one applies */
static int32_t twin_card(const track_t *t, const int16_t *vp, const param_desc_t *d, int32_t v, char *val, const char **unit)
{
    int comb = t->p[P_TWTYPE] == TT_COMB;
    if (d->fmt == F_ENUM && d->names && (vp == &t->p[P_TWTYPE] || (vp >= &XV(t, 0) && vp <= &XV(t, NX - 1)))) {   /* the names in full: param_format cuts an enum's name at 5 characters (SERIES, LADDER, SQUARE are 6) */
        str_cpy(val, d->names[clamp(v, d->min, d->max)], PV_LEN);
        if (d->unit)
            *unit = d->unit;
        if (vp == &t->p[P_TWTYPE])
            return -1;
        return d->max < 2 ? -1 : RATIO(d, enum_rank(d, v));              /* (the lists' gauge as every enum card's; a switch has none) */
    }
    if (vp == &t->p[P_DRV2]) {
        if (v <= 0)
            str_cpy(val, "OFF", 12);
        else
            fmt_int(val, v);
        *unit = "";
        return v * 1000 / 127;
    }
    if (vp == &t->p[P_BLEND]) {
        if (v <= -64) {                                  /* (1.0, owner: the filters' real names; a number alone does not say which: MULTI at the left end, BOTH in the middle, TWIN at the right end,
                                                          * between them the name and how far towards it from BOTH, in the card's two small lines: MULTI / 40% (the gauge is left out there, as on every two-line card)) */
            str_cpy(val, "MULTI", 12);
        } else if (v >= 63) {
            str_cpy(val, "TWIN", 12);
        } else if (!v) {
            str_cpy(val, "BOTH", 12);
        } else {
            char b[8];
            fmt_int(b, v < 0 ? -v * 100 / 64 : v * 100 / 63);
            str_cpy(val, v < 0 ? "MULTI\n" : "TWIN\n", 12);
            str_cpy(val + str_len(val), b, 5);
            str_cpy(val + str_len(val), "%", 2);
        }
        *unit = "";
        card_ctr_next = 1;
        return 500 + v * 500 / (v < 0 ? 64 : 63);
    }
    if (vp == &t->p[P_CUT2] && (comb || XV(t, X_LINK))) {      /* OFFSET (LINK on) or TUNE (COMB): around zero */
        card_ctr_next = 1;
        return 500 + clamp(v, d->min, d->max) * 500 / (comb ? 24 : TW_OFF_MAX);
    }
    if (vp == &t->p[P_RES2] && comb) {                        /* FDBK */
        card_ctr_next = 1;
        return 500 + clamp(v, -64, 63) * 500 / (v < 0 ? 64 : 63);
    }
    return -2;
}
/* the card of the parameter at vp does not apply for the current selection (ui_draw.c draws nothing, ui_input.c: the knob does nothing). Only what a
 * selection makes impossible; a value of 0 is not a reason. MODULES OFF (the module's cards), DUEL (PAN, SPREAD of the voice and the delay's
 * SPREAD), the delay mode (params.c dly_mode_why: DIGI hides SPREAD, RAND and OCT+ hide MRATE; PING-PONG hides nothing), an external MIDI clock
 * (TEMPO), a synced LFO's RATE, INSERT A B C MIX with the INSERT OFF, the ADSR of an engine with its own envelopes (FM6) */
static int card_hidden_vp(const int16_t *vp)
{
    const track_t *t = TSEL;
    if (!vp)
        return 0;
    if (mods_fade_vp(vp))
        return 1;
    if (twin_card_hidden(t, vp))
        return 1;
    if (vp == &song.g[G_BPM] && clock_src())
        return 1;
    if (vp == &song.bm && duell_mode == DM_SPLIT)          /* BASS MONO (0.2.19): the stage does not run in DUEL */
        return 1;
    if (vp >= song.cp && vp < song.cp + 4 && (duell_mode == DM_SPLIT || (vp != &song.cp[1] && !song.cp[1])))   /* COMP (0.2.21): the stage does not run in DUEL; at RATIO 1:1 (off) THRESH, ATTACK and RELEASE have nothing to act on */
        return 1;
    if (vp == &song.ms[0] && duell_mode == DM_SPLIT)   /* WIDTH (0.2.23): the stage does not run in DUEL (the panel says why: ui_graph.c graph_mstg) */
        return 1;
    if (duell_mode_req == DM_SPLIT && (vp == &t->p[P_PAN] || vp == &t->p[P_SPRD] || vp == &XV(t, X_CYPAN)))   /* (DUELL 1.1: CYCLE's PAN card too) */
        return 1;
    if (vp >= song.g && vp < song.g + G_COUNT && dly_mode_why((uint32_t)(vp - song.g)))
        return 1;
    if (vp >= &t->p[P_IA] && vp <= &t->p[P_IMIX] && vp != &t->p[P_ITYPE] && !t->p[P_ITYPE])   /* INSERT OFF: A, B, C and MIX have nothing to act on */
        return 1;
    if (duck_forced(t) && vp == &t->p[P_DKSRC])         /* the other part has DUCK SRC PART: this part has no ducker at all (params.c), its SRC card is not drawn either */
        return 1;
    if (duck_off(t) && (vp == &t->p[P_DKRATE] || vp == &t->p[P_DKTHR] || vp == &t->p[P_DKDEP] || vp == &t->p[P_DKATK] || vp == &t->p[P_DKRLS] || vp == &t->p[P_DKCRV]))   /* DUCK SRC OFF (0.2.24): nothing ducks, the other DUCK cards have nothing to act on */
        return 1;
    if (vp == &XV(t, X_STRUM) && !strum_acts(t))        /* DUELL 1.1 item 144: STRUM acts only on a chord the keys play: CHRD on, POLY, the arp not playing (chord.c strum_acts) */
        return 1;
    if (vp == &t->p[P_PRIO] && t->p[P_VOICE] == V_POLY)   /* T0227 (owner: the controls that apply): NOTE PRIORITY picks the one note of MONO, LEGATO, UNISON (voice.c mono_pick); POLY sounds every key */
        return 1;
    if (vp == &t->p[P_DETUNE] && t->p[P_VOICE] != V_UNISON)   /* 0.2.25 layout round (owner): UNISON DETUNE acts only in UNISON (voice.c mono_play: the unison voices' offsets) */
        return 1;
    if (vp == &t->p[P_SPRD] && (t->p[P_VOICE] == V_MONO || t->p[P_VOICE] == V_LEGATO))   /* .. SPREAD only with more than one voice: POLY and UNISON (fx.c spread_on; docs/VOICE_CONTROLS.md) */
        return 1;
    /* (DUELL 1.1, owner 2026-10-25: FM6's AMP ENV knobs show; they carry the carrier operators' envelopes, eng_fm6.c fm6_env_derive) */
    if ((vp == &t->p[P_ED_SHP] || vp == &t->p[P_LD_SHP] || vp == &t->p[P_E2SHP] || vp == &t->p[P_L2SHP]) && mod_shp_inert(t))   /* 0.2.25 (owner: the hide rule for modulation): the SHAPE depths
                                                         * of AMP DEST, LFO 1 DEST, ENV 2 DEST, LFO 2 DEST with a wave that has no shape input (mod.c mod_shp_inert: ANALOG not PWM / SYNC, LOFI not PLS, TRIO with no pulse) */
        return 1;
    if (ENGINES[t->eng_req % NENGINES] == &ENG_LOFI && vp == &t->p[P_E2] && t->p[P_E1] != RW_PLS)   /* DUELL 1.1: LOFI's PULSE WIDTH (K4) is the pulse's only; every other WAVE hides it */
        return 1;
    return (vp == &t->p[P_LRATE] && t->p[P_LSYNC]) || (vp == &t->p[P_L2RATE] && t->p[P_L2SYNC]);
}

/* DUELL 1.1 BYPASS (web/EDITOR_PROTOCOL.md "BYPASS"): a card of a bypassed module shows the word BYPASS in place of its value, in the dim of a hidden control (the gauge empty); the card still turns and its
 * value is not changed. By the page's title (the module the page belongs to), and for the FX page by the card (SATURATOR, the three sends). TWIN CHAIN (ROUTE, DRIVE, BLEND) keeps acting while the filter is
 * bypassed, so it shows its values. The AMP ENV pages have no bypass */
static int byp_card(const page_t *pg, const int16_t *vp)
{
    static const struct { const char *title; uint16_t part; uint8_t sh; } B[] = {
        {"MULTI FILTER 1/2", BY_MULTI, 0}, {"MULTI FILTER 2/2", BY_MULTI, 0}, {"TWIN FILTER", BY_TWIN, 0}, {"TWIN MOD", BY_TWIN, 0},
        {"LFO 1", BY_LFO1, 0}, {"LFO 1 TRIG", BY_LFO1, 0}, {"LFO 1 DEST", BY_LFO1, 0},
        {"LFO 2", BY_LFO2, 0}, {"LFO 2 TRIG", BY_LFO2, 0}, {"LFO 2 DEST", BY_LFO2, 0},
        {"ENV 2", BY_ENV2, 0}, {"ENV 2 DEST", BY_ENV2, 0}, {"CYCLE ENV 1/2", BY_CYC, 0}, {"CYCLE ENV 2/2", BY_CYC, 0},
        {"INSERT 1/2", BY_INS, 0}, {"INSERT 2/2", BY_INS, 0}, {"EQ 1/2", BY_EQ, 0}, {"EQ 2/2", BY_EQ, 0},
        {"DUCK 1/2", BY_DUCK, 0}, {"DUCK 2/2", BY_DUCK, 0}, {"SEND FX", BY_SEND, 0},
        {"CHORUS", 0, BYS_CHO}, {"DELAY 1/2", 0, BYS_DLY}, {"DELAY 2/2", 0, BYS_DLY}, {"REVERB", 0, BYS_REV}, {"SHIMMER", 0, BYS_REV},
        {MASTER_PAGE_TITLE, 0, BYS_MST}, {"COMPRESSOR", 0, BYS_CMP}};
    uint32_t i;
    const track_t *t = TSEL;
    if (!(byp_m[0] | byp_m[1] | byp_sh))
        return 0;
    if (str_eq(pg->title, "FX"))
        return vp == &t->p[P_DIST] ? (byp_m[song.sel & 1u] & BY_SAT) != 0u : (byp_m[song.sel & 1u] & BY_SEND) != 0u;
    for (i = 0; i < sizeof B / sizeof B[0]; i++)
        if (str_eq(pg->title, B[i].title))
            return (byp_m[song.sel & 1u] & B[i].part) != 0u || (byp_sh & B[i].sh) != 0u;
    return 0;
}

/* an action's column (act_cols): picked, the OCT+ keycap (accent while it would do something); else "--" */
static void draw_act_column(uint32_t c, const char *label, uint16_t vc, uint32_t icon)   /* icon: ICON_AUTO = by label */
{
    int picked = act_col() == c + 1u;
    draw_column(c, label, picked || AREA_ON ? "[OCT+]" : "--", "", picked ? (act_ready() ? T_ACCENT : T_DIM) : vc, -1, icon);   /* (TEMPLATE 9, as the pictures: the OCT+ pill always, plain until the knob picks it; DIM: it cannot act) */
}

/* the sound's name on the track: its slot's (a saved record's or a factory sound's), else the factory sound it plays (b holds 16) */
static void sound_name(const track_t *t, char *b)
{
    b[0] = 0;
    if (t->user)
        sl_name((uint32_t)t->user - 1u, b);
    else
        str_cpy(b, ds_name(t->eng_req % NENGINES, t->preset), 16);   /* (DUELL: sounds.c; INIT for none) */
}

/* the same for the footer: "#12 FOLD BASS" (b holds 20): the number of the slot the sound is in, then its name; a sound in no slot: the name alone */
static void sound_label(const track_t *t, char *b)
{
    uint32_t sl = slot_of(t);
    b[0] = 0;
    if (sl < UP_SLOTS) {
        up_slot_label(b, sl);
        str_cpy(b + str_len(b), " ", 2);
    }
    sound_name(t, b + str_len(b));
}

/* THE footer (every page, every layer, 202..240 = H_FOOT; the lint's rows: 2 and 19):
 *   row 1  y 2    the sequence's 16 step bars (the cursor's bank); the GRID's page and ACCENT; MIDI LEARN's line;
 *                 PATTERNS / MULTIS: what a held SAVE does there. A page never puts a keycap strip here (SPEC 1.1a: SELECT selects,
 *                 the knobs adjust; the footer says which page it is, not which button goes where)
 *   row 2  y 19   the engine's icon and name at x 8, the PATCH NAME after it, in full (the page's name is in the header since 0.2.1, the
 *                 room is the patch name's); at the right, ending at x 232: the page's number "6/7". A page with no number (alone in
  *                 its family) shows none. PERFORM: as on every page (PRESETS changes the sound there)
  * A layer shows the same footer, its kind ("SET", "HOLD") where the page's number is. While any part is armed for recording, a red dot and "REC"
  * stand in row 2 just left of that number (foot_rec_mark), on every page; the patch name gives way to it */
#define FT_R 232                                       /* the right margin (the left one is 8) */
/* the footer's step strip (row 1: the 16 step bars, the playhead, the cursor) shows on the sequencer's pages only (0.2.9, the owner: "hide the
 * step strip except on sequencer pages"): SEQ's STEP and PARAMETER LOCKS pages and its TOOLS layer, and the ARP pages while the selected part's arp
 * MODE is SEQ (the arp plays those steps); elsewhere row 1 is empty unless it has its own
 * line (MIDI LEARN, PERFORM's SELECT / ALGO, PATTERNS / MULTIS' SAVE hint, the GRID's page) */
static int foot_arp_page(void)
{
    return !ui.layer && !ui.perf && !page_over && cur_page()->fam == FAM_ARP;
}
/* item 125: the screen shows the OFF notice (a page or QUICK ARP / QUICK SEQ whose module is OFF): the module's MODS_* bit, else 0 */
static uint32_t off_screen(void)
{
    if (ui.menu || ui.guide || ui.uboot || ui.confirm || name_on() || (ui.perf && !ui.layer))
        return 0;
    return ui.layer ? off_layer(ui.layer) : off_module(cur_page());
}
static int foot_steps(void)
{
    if (off_screen())                                  /* (the notice page has no step strip) */
        return 0;
    if (ui.layer)
        return ui.layer == LAYER_SEQ;
    if (ui.perf || page_over || ui.guide)
        return 0;
    if (cur_page()->graph == GR_ROLL || cur_page()->graph == GR_SEQHELP)   /* item 118 (FINAL_V1_07): the roll shows its steps itself, the footer is the shared one (no step strip) */
        return 0;
    return cur_page()->fam == FAM_SEQ || (cur_page()->fam == FAM_ARP && arp_mode(TSEL) == AM_SEQ);   /* (0.2.14: as it plays now: MODULES) */
}
/* the step the strip marks as sounding (0xFF: none in the shown bank). SEQ's pages: the sequencer's step while it plays. ARP pages (arp in
 * SEQ mode): the arp's own step, arp_idx % LEN (seq.c arp_seq_notes), while keys are held or latched; the arp runs without PLAY */
static uint32_t foot_playhead(const track_t *t)
{
    uint32_t k;
    if (foot_arp_page()) {
        uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u;
        if (!t->nheld || t->arp_idx == 0xFFFFFFFFu)
            return 0xFFu;
        k = t->arp_idx % len;
    } else {
        if (!song.playing)
            return 0xFFu;
        k = t->seq_idx;
    }
    return k / 16u == ui.bank ? k : 0xFFu;
}
/* the page number's right edge in the footer: left of the battery (HEAD_BAT_X) */
#define FOOT_CT_R UT_FOOT_CT_R
/* which names the footer shows (item 111, FINAL_V1_02 / 03 pictures): the ENGINE page shows the patch only (the page itself names the engine), the PATCHES page the engine only (the list
 * names the patches), every other page both. Bit 0: the engine's name, bit 1: the patch's name */
static uint32_t foot_names(void)
{
    const page_t *pg = cur_page();
    if (ui.guide)
        return 0u;                                      /* (owner 2026-11-06: the GUIDE / SETUP pages show no engine or patch name) */
    if (ui.layer || ui.menu || ui.uboot || name_on())
        return 3u;
    return pg->graph == GR_ENGINE ? 2u : pg->graph == GR_BROWSE ? 1u : 3u;
}
/* how far a patch name wider than its room w is scrolled now (px; 0 when it fits): it stands still 1.2 s, runs at 25 px a second to its end, stands 1.2 s, and starts over (DUELL 1.1, owner item 83) */
static int32_t foot_scroll(const char *s, int32_t w)
{
    int32_t over = text_w(&AF_S, s) - w, cyc, t;
    if (over <= 0 || w <= 0 || (ui_prefs & PREF_ANIM_OFF))
        return 0;
    cyc = 1200 + over * 40 + 1200;
    t = (int32_t)(fm1_ms % (uint32_t)cyc);
    return t < 1200 ? 0 : t < 1200 + over * 40 ? (t - 1200) / 40 : over;
}
static void draw_foot(void)
{
    char s[48], pn[20], ti[28], ct[8], ln[56], pfl[24], pfr[24];
    const track_t *t = TSEL;
    uint32_t sig;
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    const char *ename = e->name, *save_hint;
    int32_t ctp, rdot;
    uint32_t fnm;
    int foot_short;
    pfl[0] = pfr[0] = 0;
    if (!ui.ml.on && !ui.layer && !ui.guide && grid_on != 16)
        (void)perf_foot(pfl, pfr);                     /* PERFORM: row 1 says what SELECT and ALGORITHM do there; row 2 as on every page (TEMPLATE 9: no first row, as PERFORM_NEW_LAYOUT_V4: SELECT = BALANCE is on knob 1's line) */
    sound_label(t, pn);                                /* "#12 FOLD BASS": the slot's number, when the sound is in a slot */
    if (ui.layer) {                                    /* a layer's map: its button and kind in the page's place */
        layer_foot_title(ti, ct);
    } else if (ui.guide) {                             /* item 118: the GUIDE / SETUP page's counter, "1/5" (the MENU's copy "1/3"), where every other page has its number */
        ti[0] = 0;
        fmt_int(ct, ui.guide_menu ? (int32_t)ui.guide - 1 : (int32_t)ui.guide);
        str_cpy(ct + str_len(ct), "/", (uint32_t)sizeof ct - str_len(ct));
        fmt_int(ct + str_len(ct), ui.guide_menu ? 3 : (int32_t)GUIDE_N);
    } else {
        page_parts(ti, ct);                            /* the name (header) and "6/7" (OVERVIEW 5/5 on HOME) */
    }
    str_cpy(s, eng_shown(ename), sizeof s);
    s[str_len(s) + 1u] = 0;
    s[str_len(s)] = (char)('1' + song.sel);
    str_cpy(s + str_len(s), pn, 20);
    str_cpy(s + str_len(s), ct, sizeof ct);
    sig = str_hash(0x9E3779B9u, s) + (uint32_t)ui.layer * 4177u + (song.rec ? ((song.rec >> song.sel) & 1u ? 0x2B1Fu : 0x3C27u) : 0u);
    if (foot_steps()) {                                /* step markers: the playhead only when it is in the shown bank, the cursor only in SEQ */
        uint32_t ph = foot_playhead(t);
        sig += 0x57E9u + ph * 97u + (song.seq_mode && !foot_arp_page() ? ui.cursor : 0xFFu) * 3001u + steps_hash(t) + ui.bank * 7u + (uint32_t)t->p[P_SLEN] * 13u;
    }
    save_hint = !ui.ml.on && !ui.layer && !off_screen() && !ui.guide && (grid_on != 16 || UT_T3_HINT) && (cur_page()->graph == GR_PATS || cur_page()->graph == GR_SLOTS || cur_page()->graph == GR_BROWSE)
                    ? (cur_page()->graph == GR_PATS ? "HOLD SAVE = SAVE PATTERN" : cur_page()->graph == GR_BROWSE ? "HOLD SAVE = SAVE PATCH" : "HOLD SAVE = SAVE MULTI") : 0;
    if (save_hint)
        sig = str_hash(sig + 0x5A7Eu, save_hint);
    if (ui.ml.on) {                                    /* MIDI LEARN (midi_learn.c): what is picked, its CC */
        ml_line(ln);
        sig += str_hash(0x1EA2u, ln);
    }
    if (pfl[0])
        sig = str_hash(str_hash(sig + 0x9E2Fu, pfl), pfr);
    sig += (uint32_t)batt_shown() * 7777u;   /* (the battery and the CPU bar are in the footer) */
    fnm = foot_names();
    sig += fnm * 3571u;
    ctp = FOOT_CT_R * 16 - (ct[0] ? text_w16(&AF_S, ct) : 0);   /* the counter's pen, 1/16 px */
    rdot = foot_rec_dot(ctp);                          /* (the REC mark's left edge: the patch name ends before it) */
    {   /* a patch name wider than its room scrolls: the offset it has now is part of the picture */
        int32_t pen = UT_FOOT_X * 16 + ((fnm & 1u) ? text_w16(&AF_S, eng_shown(ename)) + UT_FOOT_WORD_GAP * 16 : 0);
        int32_t nw = (rdot ? rdot : (ctp >> 4)) - UT_FOOT_WORD_GAP - (pen >> 4);
        sig += (uint32_t)foot_scroll(pn, nw) * 131u;
    }
    if (!ui.force && sig == ui.foot_sig)
        return;
    ui.foot_sig = sig;
    foot_short = (grid_on == 1 || grid_on == 3 || grid_on == 4 || grid_on == 5 || grid_on == 7 || grid_on == 9 || grid_on == 10 || grid_on == 11 || grid_on == 12 || grid_on == 13 || grid_on == 14 || grid_on == 15 || grid_on == 16 || grid_on == 17 || ui.guide) && !ui.ml.on && !pfl[0] && !save_hint && !foot_steps();   /* (DUELL 1.1 grid pages: row 1 is empty, the cards use its rows) */
    cv_begin(240, foot_short ? H_FOOT - 15 : pfl[0] ? H_FOOT + 15 : H_FOOT, T_BG);   /* (item 120: PERFORM's footer is 15 rows taller, its two hint lines on top) */
    cv_oy = foot_short ? -15 : pfl[0] ? 15 : 0;
    if (ui.ml.on) {                               /* row 1: MIDI LEARN's line, OCT+ DONE at the right (the one way out) */
        khint_t kh = {KC_OCTUP, "DONE"};
        int32_t kw = kh_w(KC_OCTUP, "DONE");
        cv_free_text(8, 2, &AF_S, ln, T_ACCENT, T_BG, 232 - kw - 8 - 8);
        cv_key_row(232 - kw, 232, 2, &kh, 1, 1u, T_BG);
    } else if (pfl[0]) {                               /* PERFORM: SELECT = BALANCE and */
        cv_oy = 0;                                    /* (item 120: one line each, left aligned: both on one row are 265 px of 224) */
        cv_text(8, 0, &AF_S, pfl, T_MID);             /* ALGORITHM = the part(s) MORPH and ROLL act on (the steps are not the page's) */
        cv_text(8, 13, &AF_S, pfr, T_MID);
        cv_oy = 15;
    } else if (save_hint) {                            /* PATTERNS / MULTIS: what a held SAVE does here (the steps are not the page's) */
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "footer save hint centred");
        cv_text_in(0, 2, 240, &AF_S, save_hint, T_DIM, T_BG);
    } else if (foot_steps()) {
        uint32_t i, ph = foot_playhead(t);
        if ((ui.bank + 1u) * 16u <= (uint32_t)t->p[P_SLEN])
            GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_CELLS | AL_N(16), "footer step bars centred");
        for (i = 0; i < 16u; i++) {                   /* row 1: the cursor's bank, 16 bars in 4 groups */
            uint32_t si = ui.bank * 16u + i;
            int32_t sx = bar_x(i);                    /* (centred: x 12 .. 228) */
            const step_t *st = &seq_steps(t)[si];
            if (si >= (uint32_t)t->p[P_SLEN])
                continue;
            if (step_on(st))                          /* a note: a bar (accented: the accent) */
                cv_rrect(sx, 1, 9, 11, 2, (st->flags & SF_ACCENT) ? T_ACCENT : T_THEME, T_BG);
            else                                      /* empty: a stub (a tie: brighter) */
                cv_rrect(sx, 9, 9, 3, 1, st->time == ST_TIE ? T_MID : T_DIM, T_BG);
            if (song.seq_mode && !foot_arp_page() && si == ui.cursor)
                cv_rect(sx, 14, 9, 2, T_ACCENT);        /* the step edited */
            else if (si == ph)
                cv_rect(sx, 14, 9, 2, T_TEXT);          /* the step sounding */
        }
    }
    {   /* row 2 (item 111, FINAL_V1 pictures; ui_tokens.h UT_FOOT_*): face F, the capitals' last row UT_FOOT_BASE on every page. The engine's name in the part colour at x UT_FOOT_X, the patch's name
         * UT_FOOT_WORD_GAP after it (alone at x UT_FOOT_X where the engine's is left out: foot_names), the page's number ending UT_FOOT_CT_R, REC before it, the battery (draw_battery) at the
         * right; a patch name that does not fit SCROLLS (the REC mark, the number and the battery stay still on top of it) */
        int32_t fy = UT_FOOT_BASE - Y_FOOT - AF_S_CAP_Y - AF_S_CAP_H + 1, pen = UT_FOOT_X * 16, nr = rdot ? rdot : (ctp >> 4), nx, nw;
        nr -= UT_FOOT_WORD_GAP;                        /* the patch name ends here (before the REC mark or the number) */
        if (fnm & 1u)
            pen += text_w16(&AF_S, eng_shown(ename)) + UT_FOOT_WORD_GAP * 16;   /* the patch name's pen: after the engine's name */
        nx = pen >> 4;
        nw = nr - nx;
        if (fnm & 2u) {
            cv_scroll = 1;                              /* (a scrolled view: the lint knows the name runs on under the marks) */
            cv_text_x16(pen - foot_scroll(pn, nw) * 16, fy, &AF_S, pn, T_TEXT, T_BG);   /* (the name first: the margins, the engine and the marks are drawn over it) */
            cv_scroll = 0;
            cv_rect(0, fy, nx, 20, T_BG);
            cv_rect(nr, fy, 240 - nr, 20, T_BG);
        }
        if (fnm & 1u)
            cv_text_x16(UT_FOOT_X * 16, fy, &AF_S, eng_shown(ename), vt(T_PTHEME(song.sel), T_BG), T_BG);   /* (the selected part's colour on a global page too, not the global gradient's) */
        foot_rec_mark(ctp);
        if (ct[0])
            cv_text_x16(ctp, fy, &AF_S, ct, T_MID, T_BG);
        draw_battery();
    }
    cv_oy = 0;
    cv_blit(0, foot_short ? Y_FOOT + 15 : pfl[0] ? Y_FOOT - 15 : Y_FOOT);
}
/* the ENGINE page's K1 card (T0227: K2..K4 are the engine's first three controls, drawn as every parameter card; no SOUND, FAV, INIT here) */
static void engine_card(uint32_t c)
{
    uint32_t e = TSEL->eng_req % NENGINES;
    if (grid_on == 5 || grid_on == 15) {   /* TEMPLATE 2 row 1 (item 115, FINAL_V1_02_engine): the card, the knob mark 1 and the engine's name (M, left aligned, one baseline, no highlight pill); item 165: the same row over LOFI's / NOISE's cells */
        char key[16];
        uint32_t h, i, hot = c == ui.hot_col && ui.hot_t;
        const char *nm = eng_shown(ENGINES[e]->name);
        uint16_t lc = hot ? T_ACCENT : T_MID;
        int32_t base = UT_T2_ROW_BASE - UT_T2_ROW_Y, dy = base - AF_M_CAP_H + 1;   /* (card rows) the dot's top = the name's cap top */
        h = str_hash(0x7Eu, nm) + hot * 31u + ux.gen * 3u;
        for (i = 0; i < 8u; i++)
            key[i] = (char)('A' + ((h >> (4u * i)) & 15u));
        key[8] = 0;
        if (!ui.force && str_eq(ui.col[0], key))
            return;
        str_cpy(ui.col[0], key, sizeof ui.col[0]);
        cv_begin(240, UT_T2_ROW_H, T_BG);
        cv_rrect(UT_T2_ROW_DOT_X, dy, AF_M_CAP_H, AF_M_CAP_H, AF_M_CAP_H / 2, lc, T_BG);
        cv_text_in(UT_T2_ROW_DOT_X, dy + HALF_UP(AF_M_CAP_H - AF_D_CAP_H) - AF_D_CAP_Y, AF_M_CAP_H, &AF_DOT, "1", T_BG, lc);
        cv_text_on(UT_T2_ROW_NAME_X, UT_TY(M, base), &AF_M, nm, lc, T_BG);
        cv_blit(0, UT_T2_ROW_Y);
        return;
    }
    draw_column(c, "ENGINE", eng_shown(ENGINES[e]->name), "", VAL(c), (int32_t)eng_rank(e) * 1000 / (NENG_SHOWN > 1 ? NENG_SHOWN - 1 : 1), engine_icon(ENGINES[e]->name));
}
/* T0227 (owner): CONTROLS MOVE LEFT. On a parameter page, the cards a selection hides (card_hidden_vp: UNISON's DETUNE, SPREAD, the delay's MOD RATE in
 * RANDOM / OCT UP, DUCK's cards with SOURCE OFF, an external clock's TEMPO and TAP, ...) leave no hole: the cards that show take K1, K2, .. in the page's order
 * and the columns after them stay empty; K1..K4 address the n-th card that shows (ui_input.c edit_param, knob_reset), the stored ids and their order do not move.
 * A page with nothing hidden is drawn as ever (its blank slots where they are). Not on the pages with cards of their own (PARAMETER LOCKS, the action pages, the
 * matrix, FM6's operators, the step page) nor in the QUICK layers, whose group cells sit over fixed knobs (ui_layer.c layer_quick).
 * -> n, the page slots K1..Kn show in vis[] */
static int page_compacts(const page_t *pg)
{
    uint32_t g = pg->graph;
    if (ui.layer || (pg->scope != SC_TRACK && pg->scope != SC_GLOBAL && pg->scope != SC_ENGINE))
        return 0;
    if (g == GR_EVENTS || g == GR_TOOLS || g == GR_ENGINE || g == GR_BROWSE || g == GR_PATS || g == GR_SLOTS || g == GR_MOD || g == GR_ROLL || g == GR_GLO || fop_page(g))
        return 0;
    return !act_cols();
}
static int slot_hidden(const page_t *pg, uint32_t s)
{
    int16_t *vp;
    if (pg->scope == SC_MODS)
        return 0;
    if (pg->graph == GR_TEMPO && s == TEMPO_TAP_SLOT)
        return clock_src() != 0;
    if (pg->id[s] == P_DKRATE && duck_ctx_id(TSEL) == 0xFFu)
        return 1;
    if (s == 3u && duck_midi_page(pg))
        return 0;
    (void)page_desc(pg, s, &vp);
    return vp && card_hidden_vp(vp);
}
static int slot_blank(const page_t *pg, uint32_t s)
{
    int16_t *vp;
    const param_desc_t *d;
    if (pg->scope == SC_MODS)                           /* (GLO's MODULES: all four slots are cells, though no parameter id) */
        return 0;
    if ((pg->graph == GR_TEMPO && s == TEMPO_TAP_SLOT) || (s == 3u && duck_midi_page(pg)))
        return 0;
    d = page_desc(pg, s, &vp);
    return !d || !d->label || d->label[0] == '-';
}
static uint32_t page_cards(const page_t *pg, uint8_t vis[4])
{
    uint32_t s, n = 0, hid = 0;
    for (s = 0; s < 4u; s++)
        vis[s] = (uint8_t)s;
    if (!page_compacts(pg))
        return 4u;
    for (s = 0; s < 4u; s++)
        hid |= (uint32_t)slot_hidden(pg, s) << s;
    if (!hid)
        return 4u;
    for (s = 0; s < 4u; s++)
        if (!((hid >> s) & 1u) && !slot_blank(pg, s))
            vis[n++] = (uint8_t)s;
    for (s = n; s < 4u; s++)
        vis[s] = 0xFFu;
    return n;
}
/* the page slot knob k (0..3) addresses: 0xFF = none (no card there) */
static uint32_t knob_slot(const page_t *pg, uint32_t k)
{
    uint8_t vis[4];
    uint32_t n = page_cards(pg, vis);
    return k < n ? vis[k] : 0xFFu;
}
/* item 156 (owner, UI_REVIEW_TEST9 B): the CELL pages (TEMPLATE 8, grid_on 14): the pages whose picture was only the calm curve, the graph GR_NONE: CHORUS, DELAY 1/2, DELAY 2/2, REVERB, SHIMMER, DUCK 1/2,
 * DUCK 2/2, VOICE 1/2, VOICE 2/2, EDIT 1, EDIT 2. Not EDIT 1 / EDIT 2 of WHEEL and FM6: they keep their picture (ui_graph.c draw_graph, the drawbars and the operator chart).
 * ARP 3/3 is a CELL page now (LAYOUT_IMPLEMENTATION_PLAN.md block E, ARP_3of3_new_layout.png: one wide cell, TARGET; the live output scope is gone). Item 162 (owner, test10: "it doesn't show anything"): TEMPO too (its picture was the
 * same calm curve, reacting to nothing): K1 CLOCK a choice cell, TEMPO and SWING value cells, TAP its own card (no bar) */
static int cells_page(const page_t *pg)
{
    uint32_t e = TSEL->eng_req % NENGINES;
    if (pg->scope == SC_MODS)                           /* GLO's MODULES: four choice cells (draw_columns) */
        return 1;
    if ((pg->graph != GR_NONE && pg->graph != GR_TEMPO) || (pg->scope != SC_TRACK && pg->scope != SC_ENGINE && pg->scope != SC_GLOBAL))
        return 0;
    return !(pg->scope == SC_ENGINE && (ENGINES[e] == &ENG_WHEEL || e == ENGI_FM6));
}
/* item 158 (owner, "no empty pages" / "nothing to adjust = no page"): a page of parameter cards whose cards all hide or are blank has nothing to show or adjust: it is not in the page order
 * (ui.c page_visible), also when it keeps a picture (owner: INSERT 2/2 with INSERT OFF; STEREO MASTER and COMPRESSOR in DUEL, where every card hides). A page with its module OFF shows the
 * notice instead (template 13): not this rule. Pages that draw their own controls (the lists, the MATRIX, the roll, the ENGINE list, the MIXER, SEQUENCER HELP's legend ..) are never empty by it */
static int page_empty(const page_t *pg)
{
    uint32_t s, g = pg->graph;
    if (off_module(pg) || (pg->scope != SC_TRACK && pg->scope != SC_GLOBAL && pg->scope != SC_ENGINE))
        return 0;
    if (g == GR_EVENTS || g == GR_TOOLS || g == GR_ENGINE || g == GR_BROWSE || g == GR_PATS || g == GR_SLOTS || g == GR_MOD || g == GR_ROLL || g == GR_GLO || g == GR_SEQHELP || fop_page(g))
        return 0;
    for (s = 0; s < 4u; s++)
        if (!slot_blank(pg, s) && !slot_hidden(pg, s))
            return 0;
    return 1;
}
/* the cells of a CELL page now: the columns whose card shows (page_cards: the cards that show, moved left; a blank slot or a hidden card has none) get the cells 0, 1, .. in their order.
 * -> 1 when the cells moved (another count, another column): the page is drawn whole */
static int cells_plan(const page_t *pg)
{
    uint8_t vis[4], at[4];
    uint32_t c, n = page_cards(pg, vis), k = 0;
    int moved;
    for (c = 0; c < 4u; c++) {
        uint32_t s = c < n ? vis[c] : 0xFFu;
        at[c] = 0xFFu;
        if (s < 4u && !slot_blank(pg, s) && !slot_hidden(pg, s))
            at[c] = (uint8_t)k++;
    }
    moved = k != cell_n || at[0] != cell_at[0] || at[1] != cell_at[1] || at[2] != cell_at[2] || at[3] != cell_at[3];
    cell_n = (uint8_t)k;
    for (c = 0; c < 4u; c++)
        cell_at[c] = at[c];
    return moved;
}
/* TEMPLATE 9 (grid_on 16, LAYOUT_IMPLEMENTATION_PLAN.md block C): the AREA pages, knob 1 a line, the big area for the page's picture, knobs 2 .. 4 in the card row: ARP 1/3, ARP 2/3, PARAMETER LOCKS,
 * SEQUENCER (its roll in the area), GLOBAL SCALE, CHORD, PATCHES, MULTIS, PATTERNS (PERFORM and the QUICK SEQ layer are chosen by ui_draw_page: PERFORM through perf_pg, the layer through `want`).
 * Not SEQUENCER HELP (its legend page, grid_on 11) */
static int area_page(const page_t *pg)
{
    uint32_t g = pg->graph;
    return g == GR_ARP || g == GR_EVENTS || g == GR_ROLL || pg->scope == SC_SCALE || g == GR_CHORD || g == GR_BROWSE || g == GR_PATS || g == GR_SLOTS;
}
/* the card row of an area page (or of the ENGINE page, grid 5) now: area_cols (3, or 2 for the pages with two cards: GLOBAL SCALE, CHORD, MULTIS, PATTERNS) and area_two (a card label of the page
 * needs two lines at its column's width: every card starts one label line higher). The pages that draw their own cards (PARAMETER LOCKS, SEQUENCER, the lists, PERFORM) have fixed one-line labels.
 * -> 1 when either changed: the page is drawn whole */
static int area_plan(const page_t *pg, uint32_t g)
{
    uint8_t vis[4];
    uint32_t cols = 3u, s, c, n, slots = 0;
    int two = 0, moved;
    if (g == 16u && (pg->scope == SC_SCALE || pg->graph == GR_PATS || pg->graph == GR_SLOTS))
        cols = 2u;
    else if (g == 16u && (pg->graph == GR_EVENTS || pg->graph == GR_ROLL || pg->graph == GR_BROWSE || pg->scope == SC_PERF))
        cols = 3u;
    else {                                              /* a page of parameter cards (ARP 1/3, ARP 2/3, CHORD; the ENGINE page): its slots 2 .. 4 that hold a card at all; their labels */
        for (s = 1; s < 4u; s++)
            slots += !slot_blank(pg, s);
        cols = g == 5u || slots >= 3u ? 3u : 2u;
        n = page_cards(pg, vis);
        for (c = 1; c < 4u && c < n; c++) {
            int16_t *vp;
            const param_desc_t *d;
            const char *lb;
            char one[24], l1[24];
            s = vis[c];
            if (s >= 4u || slot_blank(pg, s) || slot_hidden(pg, s))
                continue;
            d = page_desc(pg, s, &vp);
            if (!d)
                continue;
            lb = quick_label(pg, s) ? quick_label(pg, s) : desc_shown(d);
            two |= area_label_lines(lb, one, l1, (cols == 2u ? UT_A_W2 : UT_A_W3) - UT_A_PAD - UT_A_LBL_DX - UT_A_LBL_PAD_R) != 0;
        }
    }
    if (g == 5u)                                        /* (the ENGINE page's cards are always the taller ones: AREA_TWO) */
        two = 0;
    moved = cols != area_cols || (uint8_t)two != area_two;
    area_cols = (uint8_t)cols;
    area_two = (uint8_t)two;
    return moved;
}
/* item 165 (owner): the ENGINE pages without a picture: LOFI and NOISE (grid_on 15; the ENGINE page's K1 slot has no card, so cells_plan gives K2 .. K4 the cells) */
static int eng_cells(const page_t *pg)
{
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    return pg->graph == GR_ENGINE && (e == &ENG_LOFI || e == &ENG_NOISE);
}
/* item 165: K1 turning from an ENGINE page with a picture (grid_on 5) onto LOFI / NOISE (15) or back: the page is drawn again WITHOUT the body's BG fill (item 152: no flicker on a knob
 * turn); only the rows the NEW layout leaves between its strips and cells are filled here, the pixels that end as BG anyway (none of them is drawn over afterwards) */
static void eng_gaps(void)
{
    lcd_fill(0, UT_T2_ROW_Y + UT_T2_ROW_H, 240, UT_T2_GR_Y - UT_T2_ROW_Y - UT_T2_ROW_H, T_BG);   /* (under the K1 row: the same in both) */
    if (grid_on == 15) {
        uint32_t y1 = UT_T8E_Y0 + UT_T8E_H, y2 = UT_T8E_Y0 + UT_T8E_DY;
        lcd_fill(0, UT_T8E_Y0, UT_T8_X0, UT_T2_CELL_Y + UT_T2_CELL_H - UT_T8E_Y0, T_BG);
        lcd_fill(UT_T8_X0 + UT_T8_WIDE, UT_T8E_Y0, 240 - UT_T8_X0 - UT_T8_WIDE, UT_T2_CELL_Y + UT_T2_CELL_H - UT_T8E_Y0, T_BG);
        if (cell_n < 2u)
            lcd_fill(UT_T8_X0, y1, UT_T8_WIDE, UT_T2_CELL_Y + UT_T2_CELL_H - y1, T_BG);
        else
            lcd_fill(UT_T8_X0, y1, UT_T8_WIDE, y2 - y1, T_BG);
        if (cell_n == 3u)
            lcd_fill(UT_T8_X0 + UT_T8_W, y2, UT_T8_DX - UT_T8_W, UT_T8E_H, T_BG);
    } else {                                            /* the ENGINE page with its picture: the card row of TEMPLATE 9 (area_geo) under the picture, down to the old cells' last row */
        uint32_t i, top = (uint32_t)(UT_A_CARD_Y - UT_A_LBL2_PITCH), bot = UT_A_CARD_Y + UT_A_CARD_H, end = UT_T2_CELL_Y + UT_T2_CELL_H;
        lcd_fill(0, UT_T2_GR_Y + UT_T2_GR_H, 240, top - UT_T2_GR_Y - UT_T2_GR_H, T_BG);
        lcd_fill(0, top, UT_A_X0, bot - top, T_BG);
        for (i = 0; i < 3u; i++) {                      /* right of each card */
            uint32_t x = UT_A_X0 + UT_A_DX3 * i + UT_A_W3;
            lcd_fill(x, top, i < 2u ? UT_A_DX3 - UT_A_W3 : 240u - x, bot - top, T_BG);
        }
        if (end > bot)                                  /* (under the card row: the cells of grid 15 reached row end - 1) */
            lcd_fill(0, bot, 240, end - bot, T_BG);
    }
}
/* GLO > MIDI (owner 2026-10-25, item 86): the three cards are always there and always adjustable, K1 PART 1 CHANNEL, K2 PART 2 CHANNEL, K3 MIDI IN (PART CHANNEL / ANY CHANNEL); never faded, never
 * hidden, no card jumps to another knob. -> their count (3), the cards (0, 1 the parts' channels, 2 MIDI IN) in vis[] */
static uint32_t midi_cards(uint8_t vis[4])
{
    vis[0] = 0;
    vis[1] = 1;
    vis[2] = 2;
    vis[3] = 0xFFu;
    return 3;
}
/* (the name a card shows: params.c desc_shown, T0227's unit label pass) */
static void draw_columns(void)
{
    uint32_t c, nvis;
    uint8_t vis[4];
    char val[PV_LEN];
    const char *unit;
    int32_t twin_rt;
    grid_wide = 0;
    (void)motion_mask(song.sel, card_mot_mask);       /* (PLAY OFF: no scan) */
    if (off_module(cur_page()))                        /* item 125: its module is OFF in GLO > MODULES: no cards, only the notice (ui_graph.c graph_off) */
        return;
    if (cur_page()->graph == GR_SEQHELP)               /* SEQUENCER HELP (item 118): no cards, the legend is the page (ui_graph.c graph_seqhelp) */
        return;
    if (cur_page()->graph == GR_EVENTS) {              /* PARAMETER LOCKS (ui_events.c): ROW STEP PARAM VALUE */
        const track_t *t = TSEL;
        uint16_t rw[EV_ROWS];
        uint32_t n, c = ev_cur(rw, &n), r = ev_row(n), k = c >> 8, step, id;
        int16_t v;
        char nm[32];
        const param_desc_t *d;
        if (k == EVK_TOP) {                            /* PLAY: the counts, PLAY ON / OFF (OCT+: CLEAR) */
            draw_column(0, "ROW", "ALL", "", VAL(0u), -1, ICON_AUTO);
            fmt_int(val, (int32_t)(motion_count(t) - motion_lock_count(t)));
            draw_column(1, "EVENT", val, "", T_MID, -1, ICON_NONE);
            fmt_int(val, (int32_t)motion_lock_count(t));   /* parameter locks (SEQ > STEP: a step held, a knob) */
            draw_column(2, "LOCK", val, "", T_MID, -1, ICON_NONE);
            if (mods_off & MODS_SEQ)                   /* (GLO > MODULES SEQ OFF: no automation plays; the switch is kept, the card is not shown) */
                draw_column_hide(3);
            else {
                if (CARD_SEG_ON) {                     /* (TEMPLATE 9: a switch, the segment line: OFF the left half, ON the right) */
                    cell_seg_n = 2;
                    cell_seg_i = (uint8_t)(motion_enabled(t) != 0);
                }
                draw_column(3, "PLAY", motion_enabled(t) ? "ON" : "OFF", "", VAL(3u), -1, motion_icon());
                cell_seg_n = cell_seg_i = 0;
            }
            return;
        }
        if (k == EVK_QNT) {                            /* QUANTIZE (1.2): the steps with a NUDGE, QNTZ ON / OFF */
            draw_column(0, "ROW", "ALL", "", VAL(0u), -1, ICON_AUTO);
            fmt_int(val, (int32_t)ev_nudged(t));
            draw_column(1, "NUDGED", val, "", T_MID, -1, ICON_NONE);
            draw_column(2, "", "", "", T_THEME, -1, ICON_NONE);
            seg_next(2u, t->p[P_SQNT] != 0);
            draw_column(3, "QUANTIZE", t->p[P_SQNT] ? "ON" : "OFF", "", VAL(3u), -1, ICON_QUANTIZE);
            seg_next(0u, 0u);
            return;
        }
        if (k == EVK_ADD) {
            ev_fix();
            str_cpy(val, "ADD", sizeof val);
        } else {                                       /* "3/7": the rows between QUANTIZE and + ADD */
            fmt_int(val, (int32_t)r - 1);
            str_cpy(val + str_len(val), "/", 4);
            fmt_int(val + str_len(val), (int32_t)n - 3);
        }
        step = ev_step_of(c);
        id = k == EVK_REC ? MOTION_TID(&motion.event[c & 0xFFu]) : k == EVK_ADD ? ui.ev_id :
             k == EVK_CHANCE ? EV_CHANCE : k == EVK_RATCH ? EV_RATCH : k == EVK_VEL ? EV_VEL : EV_NUDGE;
        draw_column(0, "ROW", val, "", VAL(0u), -1, ICON_AUTO);
        fmt_int(val, (int32_t)step + 1);
        draw_column(1, "STEP", val, "", k != EVK_ADD || step < (uint32_t)t->p[P_SLEN] ? VAL(1u) : T_DIM, -1, ICON_AUTO);
        if (!ev_id_ok(t, id)) {
            draw_column(2, "PARAM", "--", "", T_DIM, -1, ICON_AUTO);
            draw_column(3, "VALUE", "--", "", T_DIM, -1, ICON_AUTO);
            return;
        }
        ev_dest_name(t, id, nm, 1);                    /* (1.1: the matrix's DEST card name, the same list and the same names as the MATRIX page; the card cuts what does not fit, nothing is shortened) */
        draw_column(2, "PARAM", nm, "", k == EVK_REC || k == EVK_ADD ? VAL(2u) : T_MID, -1, ICON_AUTO);
        if (id == EV_NUDGE) {                          /* a step's NUDGE (1.2): "+3 /16", DIM where it does not play */
            const step_t *cs = &t->step[step % NSTEP];
            int32_t sv = k == EVK_ADD ? ev_sadd(EVK_NUDGE) : step_nudge(cs);
            ev_nudge_fmt(val, sv);
            draw_column(3, "VALUE", val, "/16", k != EVK_ADD && ev_nudge_plays(t, cs) ? VAL(3u) : T_DIM, (sv + 8) * 66,
                        ICON_AUTO);
            return;
        }
        if (id == EV_VEL) {                            /* a step's VEL (1.5): 1..127, DIM where it does not play; */
            const step_t *cs = &t->step[step % NSTEP];  /* + ADD: the rows shown or not */
            int32_t sv = ev_sval(cs, EVK_VEL);
            if (k == EVK_ADD) {
                seg_next(2u, ui.ev_vel != 0);
                draw_column(3, "ROWS", ui.ev_vel ? "SHOWN" : "HIDDEN", "", T_DIM, -1, ICON_AUTO);
                seg_next(0u, 0u);
                return;
            }
            fmt_int(val, sv);
            draw_column(3, "VALUE", val, "", ev_vel_plays(cs) ? VAL(3u) : T_DIM, sv * 1000 / 127, ICON_LEVEL);
            return;
        }
        if (id == EV_CHANCE || id == EV_RATCH) {       /* a step's CHANCE (the die) / RATCH; + ADD: what it adds */
            const step_t *cs = &t->step[step % NSTEP];
            uint32_t sk = ev_skind(id), sv = (uint32_t)(k == EVK_ADD ? ev_sadd(sk) : ev_sval(cs, sk));
            int plays = sk == EVK_CHANCE || (cs->time == ST_NOTE && cs->n);   /* (RATCH does nothing on a
                                                                                            * REST, a TIE, empty: DIM) */
            if (sk == EVK_CHANCE) {
                fmt_int(val, (int32_t)sv);
            } else {
                val[0] = 'x';                          /* x1 .. x4 */
                val[1] = (char)('0' + sv);
                val[2] = 0;
            }
            draw_column(3, "VALUE", val, sk == EVK_CHANCE ? "%" : "", k != EVK_ADD && plays ? VAL(3u) : T_DIM,
                        sk == EVK_CHANCE ? (int32_t)sv * 10 : ((int32_t)sv - 1) * 333, sk == EVK_CHANCE ? ICON_PROB : ICON_X_REPEAT);
            return;
        }
        v = k == EVK_REC ? motion.event[c & 0xFFu].value : motion_base_value(t, id);
        d = lock_desc(t, id);
        param_format(d, v, val, &unit);
        draw_column(3, k == EVK_ADD ? "VALUE" : (motion.event[c & 0xFFu].param & MOTION_LOCK) ? "LOCK" : "AUTO", val, unit,
                    k != EVK_ADD ? VAL(3u) : T_DIM, RATIO(d, enum_rank(d, v)), param_icon(d, v));
        return;
    }
    if (cur_page()->graph == GR_TOOLS)                 /* TOOLS (grid_on 17): no cards, the three rows are the page (ui_graph.c graph_tools); the knob actions stay in ui.c */
        return;
    if (cur_page()->scope == SC_PERF) {                /* PERFORM (ui_perform.c): MORPH SPACE FILTER DEPTH */
        if (AREA_ON) {                                 /* TEMPLATE 9 (PERFORM_NEW_LAYOUT_V4): what SELECT does here, after MORPH's value on knob 1's line (the footer's first row is gone) */
            k1_after_next = "SELECT = BALANCE";
            k1_after_col = ux_mix(T_BASE_P1, T_TEXT, UT_GREY_PCT);
            k1_after_r = 0;
        }
        perf_cards();
        k1_after_next = 0;
        return;
    }
    if (cur_page()->scope == SC_MORPH) {               /* MORPH (fader, ROLL, UNDO, AMOUNT: v3), MORPH SET (SMART ARP PITCH; 0.2.9: TARGET is ALGORITHM's BOTH) */
        uint32_t pi = song.sel, f = morph_fader_of(pi);
        int32_t g;
        if (cur_page()->graph == GR_MORPHSET) {
            seg_next(2u, (morph.cfg & MC_WILD) != 0u);   /* (owner 2026-11-08: every switch its line: the left / the right half) */
            draw_column(0, "SMART", morph.cfg & MC_WILD ? "WILD" : "ON", "", VAL(0u), -1, ICON_NONE);
            seg_next(2u, (morph.cfg & MC_ARP) != 0u);
            draw_column(1, "ARP", morph.cfg & MC_ARP ? "ON" : "OFF", "", VAL(1u), -1, ICON_NONE);
            seg_next(2u, (morph.cfg & MC_PIT) != 0u);
            draw_column(2, "PITCH", morph.cfg & MC_PIT ? "ON" : "OFF", "", VAL(2u), -1, ICON_NONE);
            seg_next(0u, 0u);
            draw_column(3, "", "", "", T_THEME, -1, ICON_NONE);   /* (an empty column: KNOB 4 does nothing here) */
            return;
        }
        if (f == 0u || f == 127u)                      /* the fader: its ends by their letters, between them 1..126 */
            str_cpy(val, f ? "B" : "A", sizeof val);
        else
            fmt_int(val, (int32_t)f);
        grid_big = (uint8_t)(grid_on != 0);                /* (HOME's grid: the big value, PAGES_V1_home.png) */
        draw_column(0, "MORPH", val, "", VAL(0u), (int32_t)f * 1000 / 127, ICON_MIX);
        grid_big = 0;
        g = morph_pending(0);                          /* ROLL: A left, B right; a twist building up: its way */
        seg_next(g ? 2u : 0u, g > 0);                  /* (owner 2026-11-08: the line lights its half while ROLL is turned: the left half toward A, the right toward B; at rest the empty track) */
        draw_column(1, "ROLL", g < 0 ? "<A" : g > 0 ? "B>" : "A<>B", "", g ? T_ACCENT : VAL(1u), -1, ICON_NONE);
        g = morph_pending(1);                          /* UNDO: A left, B right, like ROLL; a twist building up: its way */
        seg_next(g ? 2u : 0u, g > 0);
        draw_column(2, "UNDO", g < 0 ? "<A" : g > 0 ? "B>" : "A<>B", "", g ? T_ACCENT : VAL(2u), -1, ICON_NONE);
        seg_next(0u, 0u);
        fmt_int(val, (int32_t)morph_amount_of(pi));
        draw_column(3, "AMOUNT", val, "%", VAL(3u), (int32_t)morph_amount_of(pi) * 10, ICON_PROB);
        return;
    }
#if DUELL_TEST_MODE
    if (cur_page()->scope == SC_TEST) {                /* MENU > SYSTEM > TEST: MORPH and ROLL as the MORPH page's, LOAD MULTI, RESET */
        uint32_t pi = song.sel, f = morph_fader_of(pi), mk = test_slot();
        int32_t g;
        if (f == 0u || f == 127u)
            str_cpy(val, f ? "B" : "A", sizeof val);
        else
            fmt_int(val, (int32_t)f);
        draw_column(0, "MORPH", val, "", VAL(0u), (int32_t)f * 1000 / 127, ICON_MIX);
        g = morph_pending(0);
        draw_column(1, "ROLL", g < 0 ? "<A" : g > 0 ? "B>" : "A<>B", "", g ? T_ACCENT : VAL(1u), -1, ICON_NONE);
        if (mk < MU_SLOTS)
            mu_label(val, mk);
        else
            str_cpy(val, "--", sizeof val);
        draw_column(2, "LOAD", val, "", mk < MU_SLOTS ? VAL(2u) : T_DIM, -1, ICON_AUTO);
        draw_column(3, "RESET", "--", "", VAL(3u), -1, ICON_AUTO);
        return;
    }
#endif
    if (cur_page()->scope == SC_MODS) {                /* GLO's MODULES (owner 2026-10-29, PROPOSAL_MODULES_PAGE.png): TEMPLATE 8, a 2 x 2 of choice cells ARP, SEQ, SENDS, PART FX with the segment line; ARP and SEQ OFF / ON / KEEPS RUNNING (mods_run_get), SENDS and PART FX OFF / ON */
        static const char *const L[4] = {"ARP", "SEQ", "SENDS", "PART FX"};
        for (c = 0; c < 4u; c++) {
            uint32_t rv = c < 2u ? mods_run_get(c) : ((mods_off >> c) & 1u) ? 0u : 1u;
            cell_seg_n = c < 2u ? 3u : 2u;
            cell_seg_i = (uint8_t)rv;
            draw_column(c, L[c], rv == 0u ? "OFF" : rv == 1u ? "ON" : "KEEPS RUNNING", "", VAL(c), -1, ICON_AUTO);
            cell_seg_n = cell_seg_i = 0;
        }
        return;
    }
    if (cur_page()->scope == SC_FXSUM)                 /* FX's SEND FX (0.2.22; item 117, TEMPLATE 5): no cards, the page is text on the background drawn by ui_graph.c graph_fxsum (state, delay and reverb info, note,
                                                        * the three knob labels); a twist of K1 / K2 / K3 still jumps to GLO's page of that effect (ui_input.c fxsum_knob) */
        return;
    if (cur_page()->scope == SC_MIDI) {                /* GLO's MIDI pages: MIDI (P1 CH, P2 CH, MIDI IN; CLK is on TEMPO), SIDECHAIN (SC SRC, SC NOTE), MIDI LEARN (LEARN, CLEAR) */
        if (cur_page()->graph == GR_MLEARN) {
            char n[10] = "NONE";
            seg_next(2u, ui.ml.on != 0);
            draw_column(0, "LEARN", ui.ml.on ? "ON" : "OFF", "", ui.ml.on ? T_ACCENT : VAL(0u), -1, ICON_MIDI);
            seg_next(0u, 0u);
            if (ml_count()) {                          /* CLEAR: right clears every learned CC; the card says how many there are */
                fmt_int(n, (int32_t)ml_count());
                str_cpy(n + str_len(n), " CC", 4);
            }
            draw_column(1, "CLEAR", n, "", ml_count() ? VAL(1u) : T_DIM, -1, ICON_MIDI);
            for (c = 2; c < 4u; c++)
                draw_column(c, "", "", "", T_THEME, -1, ICON_NONE);
            return;
        }
        if (cur_page()->graph == GR_SCHAIN) {          /* 0.2.22: the sidechain's MIDI setup, a device setting: K1 P1 CH, K2 P1 NOTE, K3 P2 CH, K4 P2 NOTE (shown whatever the DUCK SRC is) */
            static const char *const SL[4] = {"PART 1\nCHANNEL", "PART 1\nNOTE", "PART 2\nCHANNEL", "PART 2\nNOTE"};   /* (0.2.25 layout round, owner: PART 1 / PART 2, not P1 CH; two lines: CHANNEL does not fit) */
            for (c = 0; c < 4u; c++) {
                uint32_t n = sc_midi[c];
                if (mods_off & MODS_PFX) {             /* (0.2.14 GLO > MODULES PART FX OFF: the DUCK is skipped; the footer says it in full) */
                    draw_column_hide(c);
                    continue;
                }
                if (c & 1u) {
                    param_format(&SC_NOTE_D, (int16_t)n, val, &unit);
                } else {
                    fmt_int(val, (int32_t)n + 1);
                    unit = "";
                }
                if (!(c & 1u)) {                       /* (DUELL 1.1: the duck source under the part's channel) */
                    static char ds[24];
                    str_cpy(ds, "Duck source: ", sizeof ds);
                    str_cpy(ds + str_len(ds), val_shown(&TP[P_DKSRC], N_DKSRC[(uint32_t)clamp(trk[c >> 1].p[P_DKSRC], 0, 3)]), (uint32_t)sizeof ds - str_len(ds));
                    for (n = 13; ds[n]; n++)
                        ds[n] = (char)(ds[n] >= 'A' && ds[n] <= 'Z' ? ds[n] + 32 : ds[n] == '\n' ? ' ' : ds[n]);
                    grid_d = ds;
                    n = sc_midi[c];
                }
                draw_column(c, SL[c], val, unit, VAL(c), (int32_t)n * 1000 / ((c & 1u) ? 127 : 15), ICON_MIDI);
            }
            return;
        }
        {
            uint32_t sel = song.g[G_ROUTE] == 1, k, n = midi_cards(vis), mm = duell_mode_req < DM_N ? duell_mode_req : DM_SPLIT;
            static const char *const WH[3][2] = {{"Duel: left output", "Duel: right output"}, {"Stereo output", "Stereo output"}, {"Layer: plays both parts", "Layer: not heard, use P1 CH"}};
            grid_wide = 1;
            for (c = 0; c < 4u; c++) {                 /* the parts' channels (not shown when SELECTED listens on one channel; part 2 in LAYER), MIDI IN; T0227: moved left */
                k = c < n ? vis[c] : 0xFFu;
                if (k < 2u) {
                    fmt_int(val, (int32_t)midi_ch_get(k) + 1);
                    grid_d = WH[mm][k];
                    draw_column(c, k ? "PART 2\nCHANNEL" : "PART 1\nCHANNEL", val, "", VAL(c), (int32_t)midi_ch_get(k) * 1000 / 15, ICON_MIDI);   /* (item 86: never faded) */
                } else if (k == 2u) {
                    grid_d = sel ? "Any channel plays the selected part" : mm == DM_LAYER ? "Layer mode: part 1 channel counts" : "Each part on its own channel";
                    draw_column(c, "MIDI IN", sel ? "ANY\nCHANNEL" : "PART\nCHANNEL", "", VAL(c), -1, ICON_MIDI);
                } else if (n < 3u) {
                    draw_column_hide(c);
                } else {
                    draw_column(c, "", "", "", T_THEME, -1, ICON_NONE);   /* (CLK moved to TEMPO) */
                }
            }
        }
        return;
    }
    if (cur_page()->scope == SC_SCALE) {               /* GLOBAL (core.h): K1 SCALE, K2 ROOT, K3 OFF-SCALE SNAP / PLAY, K4 none */
        const param_desc_t *ds = &SCALE_D, *dr = &ROOT_D;
        int16_t sv = (int16_t)clamp(scale_g, 0, ds->max), rv = (int16_t)clamp(root_g, 0, dr->max);
        param_format(ds, sv, val, &unit);
        if ((page_over || ui.layer) && text_w(&AF_S, val) > room_scale_card) {   /* QUICK SCALE: a name too wide for the card is whole in the list under it; the card says which one, "3/16" */
            fmt_int(val, sv + 1);
            str_cpy(val + str_len(val), "/", 8u);
            fmt_int(val + str_len(val), (int32_t)ds->max + 1);
        }
        draw_column(0, "SCALE", val, unit, VAL(0u), RATIO(ds, enum_rank(ds, sv)), param_icon(ds, sv));
        param_format(dr, rv, val, &unit);
        if (CARD_SEG_ON && dr->fmt == F_ENUM && dr->max > dr->min) {   /* (TEMPLATE 9: ROOT is a CHOICE of the twelve notes: the segment line) */
            cell_seg_n = (uint8_t)(dr->max - dr->min + 1);
            cell_seg_i = (uint8_t)clamp(enum_rank(dr, rv) - dr->min, 0, dr->max - dr->min);
        }
        draw_column(1, "ROOT", val, unit, VAL(1u), RATIO(dr, enum_rank(dr, rv)), param_icon(dr, rv));
        cell_seg_n = cell_seg_i = 0;
        if (!sv)                                       /* CHROMATIC: every note is in the scale, nothing to snap: no card */
            draw_column_hide(2);
        else {
            if (CARD_SEG_ON) {                         /* (TEMPLATE 9: OFF-SCALE is a CHOICE: the segment line) */
                cell_seg_n = OS_N;
                cell_seg_i = (uint8_t)(offscale_g % OS_N);
            }
            draw_column(2, "OFF-\nSCALE", N_OFFSCALE[offscale_g % OS_N], "", VAL(2u), -1, ICON_AUTO);   /* (T0227: OFF-SCALE in two lines, at its hyphen; TEMPLATE 9: one line where it fits, as the picture) */
            cell_seg_n = cell_seg_i = 0;
        }
        draw_column(3, "", "", "", T_THEME, -1, ICON_NONE);
        return;
    }
    if (cur_page()->scope == SC_MODE) {                /* step 5: MODE (SPLIT / STEREO / LAYER); the rest nothing */
        uint32_t m = duell_mode_req < DM_N ? duell_mode_req : DM_SPLIT;
        draw_column(0, "MODE", DM_NAME[m], "", VAL(0u), (int32_t)m * 500, ICON_MIX);
        fmt_int(val, mix_xf);                          /* item 158 (owner: no gap in the knobs, the cards line up to the left): BALANCE gone (item 157), XFADE on KNOB 2, SPLIT on KNOB 3, KNOB 4 empty */
        if (duell_mode_req == DM_LAYER)                /* XFADE and SPLIT are LAYER's only: no card in DUEL and STEREO */
            draw_column(1, "XFADE", val, "%", VAL(1u), (int32_t)mix_xf * 10, ICON_MIX);
        else
            draw_column_hide(1);
        if (mix_split)
            note_name(val, (uint32_t)mix_split - 1u);
        else
            str_cpy(val, "OFF", sizeof val);
        if (duell_mode_req == DM_LAYER)
            draw_column(2, "SPLIT", val, "", VAL(2u), mix_split ? ((int32_t)mix_split - 1) * 1000 / 127 : -1, ICON_MIX);
        else
            draw_column_hide(2);
        draw_column_hide(3);
        return;
    }
    if (cur_page()->graph == GR_BROWSE) {              /* PATCHES (change list A.9, PATCHES_MULTIS_NEW_LAYOUT_V3): K1 PATCH (the slot's number), K2 TYPE, K3 SAVE (the save window), K4 FAV; RESTORE is gone */
        uint32_t total, cur = pl_cursor(&total), k = pl_cursor_slot();
        char sl[6];
        up_slot_label(sl, k < UP_SLOTS ? k : 0u);
        draw_column(0, "PATCH", k < UP_SLOTS ? sl : "--", "", VAL(0u), cur < total && total > 1u ? (int32_t)(cur * 1000u / (total - 1u)) : -1, ICON_NONE);
        if (CARD_SEG_ON) {                             /* (TEMPLATE 9: TYPE is a CHOICE of the list's kinds: the segment line) */
            cell_seg_n = (uint8_t)LIST_N;
            cell_seg_i = (uint8_t)list_mode();
        }
        draw_column(1, "TYPE", list_name(list_mode()), "", VAL(1u), -1, ICON_X_FOLDER);
        cell_seg_n = cell_seg_i = 0;
        draw_act_column(2, "SAVE", T_THEME, ICON_AUTO);
        if (k < UP_SLOTS) {
            if (CARD_SEG_ON) {                         /* (TEMPLATE 9: a switch: OFF the left half, ON the right) */
                cell_seg_n = 2;
                cell_seg_i = (uint8_t)(list_fav(k) != 0);
            }
            draw_column(3, "FAV", list_fav(k) ? "ON" : "OFF", "", VAL(3u), -1, ICON_X_STAR);   /* (item 115: the picture's head label) */
            cell_seg_n = cell_seg_i = 0;
        } else
            draw_column_hide(3);
        return;
    }
    if (cur_page()->graph == GR_PATS) {                  /* SLOT, then LOAD SAVE (GO buttons) */
        uint32_t n = pat_count(), k = pat_pick();
        char tag[4], nm[13];
        pat_label(k, tag, nm);
        draw_column(0, "SLOT", tag, "", VAL(0u), (int32_t)k * 1000 / (int32_t)(n > 1u ? n - 1u : 1u), ICON_X_PATTERN);
        draw_act_column(1, "LOAD", T_THEME, ICON_AUTO);
        draw_act_column(2, "SAVE", T_THEME, ICON_AUTO);   /* (change list A.9 / H.8: ERASE is gone, SAVE is knob 3) */
        draw_column_hide(3);
        return;
    }
    if (cur_page()->graph == GR_SLOTS) {                 /* MULTI: SLOT M01..M16, then LOAD SAVE (GO buttons) */
        uint32_t k = (uint32_t)(song.g[G_SLOT] - 1) % MU_SLOTS;
        int used = project_used(k);
        mu_label(val, k);
        draw_column(0, "SLOT", val, "", VAL(0u), (int32_t)k * 1000 / (int32_t)(MU_SLOTS - 1u), ICON_AUTO);
        draw_act_column(1, "LOAD", used ? T_THEME : T_DIM, ICON_AUTO);
        draw_act_column(2, "SAVE", T_THEME, ICON_AUTO);   /* (change list A.9: ERASE is gone, SAVE is knob 3) */
        draw_column_hide(3);
        return;
    }
    if (fop_page(cur_page()->graph)) {                   /* FM6: OP, then three of its bytes (ui_fm6op.c) */
        for (c = 0; c < 4u; c++) {
            const char *lb, *u;
            int32_t r;
            uint32_t ic;
            int on;
            if (!fop_ok()) {                             /* (the engine changed before ui_draw left the page) */
                draw_column(c, "", "", "", T_THEME, -1, ICON_NONE);
                continue;
            }
            on = fop_card(c, &lb, val, &u, &r, &ic);
            draw_column(c, label_shown(lb), val, u, on ? VAL(c) : T_DIM, r, ic);   /* (T0227: COARSE, STAGE, DETUNE) */
        }
        return;
    }
    if (cur_page()->graph == GR_MOD) {                   /* SLOT, then that slot's SRC DST AMT */
        const track_t *t = TSEL;
        uint32_t base = mod_page_base(cur_page()), id = MSLOT_ID(base + mod_ui_slot);   /* (MOD: 1..4, MOD 2: 5..8) */
        int32_t s = morph_view(song.sel, id), d = morph_view(song.sel, id + 1u), a = morph_view(song.sel, id + 2u);
        uint8_t mks = s != t->p[id], mkd = d != t->p[id + 1u], mka = a != t->p[id + 2u];   /* (MORPH: the route the fader has,
                                                                                            * the dominant end's; marks) */
        fmt_int(val, (int32_t)(base + mod_ui_slot) + 1);
        draw_column(0, "SLOT", val, "/8", VAL(0u), (int32_t)(base + mod_ui_slot) * 1000 / (NMSLOT - 1u),
                    mod_src_icon(MS_OFF));   /* (the mod icon) */
        card_mk_next = mks;
        draw_column(1, "SOURCE", msrc_shown(s), "", s ? VAL(1u) : T_DIM, -1, mod_src_icon(s));
        card_mk_next = mkd;
        /* (a destination this part cannot move now: the DEST card is the selector that changes it, so it stays; the panel's DEST line says it in
         * words. The one card that is not hidden for an inapplicable state: hiding it would lock the slot) */
        draw_column(2, "DEST", mod_dst_name(t, d), "", d && !mod_dst_why(t, d) ? VAL(2u) : T_DIM, -1, mod_dst_icon(t, d));
        param_format(&TP[id + 2u], a, val, &unit);
        card_mk_next = mka;
        draw_column(3, "AMOUNT", val, unit, a ? VAL(3u) : T_DIM, RATIO(&TP[id + 2u], a), mod_src_icon(MS_OFF));
        return;
    }
    if (cur_page()->scope == SC_STEP) {
        static const char *const TIME_N[3] = {"NOTE", "TIE", "REST"};
        const step_t *st = &seq_steps(TSEL)[ui.cursor];
        char u[8];
        uint32_t cnt = st->n, first = st->note[0];
        if (cnt) {
            note_name(val, first);
            u[0] = 0;
            if (cnt > 1) {
                str_cpy(u, "+", 8);
                fmt_int(u + 1, (int32_t)cnt - 1);
            }
        } else {
            str_cpy(val, "--", 12);
            u[0] = 0;
        }
        {
            static const char *const FLAG_N[4] = {"-", "ACCENT", "SLIDE", "ACCENT\nSLIDE"};
            char sn[8], sl[8];
            fmt_int(sn, (int32_t)ui.cursor + 1);
            str_cpy(sl, "/", 8);
            fmt_int(sl + 1, TSEL->p[P_SLEN]);
            draw_column(0, "STEP", sn, sl, VAL(0u), -1, ICON_AUTO);
            draw_column(1, "NOTE", val, u, step_on(st) ? VAL(1u) : T_DIM, -1, ICON_AUTO);
            if (step_on(st)) {                            /* a note: its LEN (1.5, KNOB 3: the TIEs after it) */
                fmt_int(sn, (int32_t)note_len(TSEL, ui.cursor));
                draw_column(2, "LENGTH", sn, "", VAL(2u), -1, icon_for_label("LEN"));   /* (T0227: LENGTH; the icon of LEN) */
            } else {
                if (CARD_SEG_ON) {                        /* (TEMPLATE 9: TIME is a CHOICE of three: the segment line) */
                    cell_seg_n = 3;
                    cell_seg_i = (uint8_t)(st->time % 3u);
                }
                draw_column(2, "TIME", TIME_N[st->time % 3u], "", VAL(2u), -1, ICON_AUTO);
                cell_seg_n = cell_seg_i = 0;
            }
            if (CARD_SEG_ON) {                            /* (TEMPLATE 9: FLAG is a CHOICE of four, -, ACCENT, SLIDE, ACCENT SLIDE: the segment line, as the picture) */
                cell_seg_n = 4;
                cell_seg_i = (uint8_t)((st->flags & SF_ACCENT ? 1u : 0u) | (st->flags & SF_SLIDE ? 2u : 0u));
            }
            draw_column(3, "FLAG", FLAG_N[(st->flags & SF_ACCENT ? 1u : 0u) | (st->flags & SF_SLIDE ? 2u : 0u)], "",
                        VAL(3u), -1, ICON_AUTO);
            cell_seg_n = cell_seg_i = 0;
        }
        return;
    }
    nvis = page_cards(cur_page(), vis);                 /* T0227 (owner): the cards that show, moved left (K1..Kn); the columns after them stay empty */
    for (c = 0; c < 4u; c++) {
        int16_t *vp;
        int32_t vv;
        uint32_t s = c < nvis ? vis[c] : 0xFFu;         /* (the page's slot this column shows) */
        const param_desc_t *d;
        if (s == 0xFFu) {
            draw_column_hide(c);
            continue;
        }
        d = page_desc(cur_page(), s, &vp);
        if (cur_page()->graph == GR_ENGINE && s == 0u) {   /* ENGINE (T0227): K1 the engine, then its own controls */
            engine_card(c);
            continue;
        }
        if (cur_page()->graph == GR_TEMPO && s == TEMPO_TAP_SLOT) { /* TAP: the tempo of the taps for 2 s after the last, else TAP (EXT clock: no card) */
            uint32_t n = tempo_tapping();
            if (clock_src()) {
                draw_column_hide(c);
                continue;
            }
            if (n >= 2u)
                fmt_int(val, song.g[G_BPM]);
            else
                str_cpy(val, n ? "1" : "TAP", sizeof val);
            draw_column(c, "TAP", val, n >= 2u ? "BPM" : "", n ? T_ACCENT : VAL(c), -1, ICON_TEMPO);
            continue;
        }
        if (cur_page()->id[s] == P_DKRATE && duck_ctx_id(TSEL) == 0xFFu) {   /* DUCK 1/2 K2 with SRC MIDI / OFF (0.2.24): no card (hidden, as the hide rule) */
            draw_column_hide(c);
            continue;
        }
        if (s == 3u && duck_midi_page(cur_page())) {    /* DUCK 1/2, SRC MIDI: the notes come from GLO > SIDECHAIN; K4 jumps there */
            draw_column(c, "SETUP", "GLO", "", VAL(c), -1, ICON_MIDI);   /* (T0227: the link card says SETUP, its value GLO: the page it jumps to) */
            continue;
        }
        if (vp && card_hidden_vp(vp)) {                 /* a card the selection makes inapplicable: not drawn (the knob does nothing); before the blank check: DUCK's retired K2 (SRC MIDI / OFF) hides with the rest */
            draw_column_hide(c);
            continue;
        }
        if (!d || !d->label || d->label[0] == '-') {
            draw_column(c, "", "", "", T_THEME, -1, ICON_AUTO);
            continue;
        }
        if (cur_page()->graph == GR_TEMPO && s == 0u)  /* CLK: an external clock's state under the source (EXT / NO CLOCK; AUTO: the source) */
            card_why_next = clock_why(1);
        if ((act_cols() >> s) & 1u) {
            draw_act_column(c, d->label, T_THEME, ICON_AUTO);
            continue;
        }
        if (vp && byp_card(cur_page(), vp)) {           /* DUELL 1.1 BYPASS: the word in place of the value, dim */
            draw_column(c, quick_label(cur_page(), s) ? quick_label(cur_page(), s) : desc_shown(d), "BYPASS", "", T_DIM, -1, param_icon(d, *vp));
            continue;
        }
        vv = card_vv(vp);                               /* (MORPH: the blended value; the mark) */
        param_format(d, (int16_t)vv, val, &unit);
        if (vp && vp == &XV(TSEL, X_STRUM) && vv <= 0) {  /* item 144: STRUM 0 is OFF (as TWIN DRIVE's 0) */
            str_cpy(val, "OFF", PV_LEN);
            unit = "";
        }
        card_mot_of(vp);                                /* (a global: never) */
        if (CARD_SEG_ON && d->fmt == F_ENUM && d->max > d->min) {   /* (owner 2026-11-08: the TWIN pages' choices too) */
            cell_seg_n = (uint8_t)(d->max - d->min + 1);
            cell_seg_i = (uint8_t)clamp(enum_rank(d, vv) - d->min, 0, d->max - d->min);
        }
        if (cur_page()->scope == SC_TRACK && cur_page()->graph != GR_MOD && (twin_rt = twin_card(TSEL, vp, d, vv, val, &unit)) != -2) {   /* cloud job H: the TWIN FILTER's own texts and gauges */
            draw_column(c, quick_label(cur_page(), s) ? quick_label(cur_page(), s) : desc_shown(d), val, unit, VAL(c), twin_rt, param_icon(d, (int16_t)vv));
            cell_seg_n = cell_seg_i = 0;
            continue;
        }
        /* (owner, 2026-10-13: faded ONLY what a selection makes impossible. A value of 0 is not a reason: CUT's RES and the FLT depths of the
         * universal filter, the delay modulation's RATE at DEPTH 0 and the like are live cards, a zero can be turned up) */
        if (CARD_SEG_ON && d->fmt == F_ENUM && d->max > d->min) {   /* (owner 2026-11-08: every choice, also one without names) */   /* item 156, TEMPLATE 8 (and 9, the ENGINE page: the card template's seg flag): a CHOICE (a list, not a value): its N options and the shown one's place in the knob's order (enum_rank) */
            cell_seg_n = (uint8_t)(d->max - d->min + 1);
            cell_seg_i = (uint8_t)clamp(enum_rank(d, vv) - d->min, 0, d->max - d->min);
        }
        draw_column(c, quick_label(cur_page(), s) ? quick_label(cur_page(), s) : desc_shown(d), val, unit, VAL(c), d->fmt == F_ENUM && d->max < 2 ? -1 : RATIO(d, enum_rank(d, (int16_t)vv)),
                    param_icon(d, (int16_t)vv));   /* (0.2.25: the card's display name; the icon by the stored label) */
        cell_seg_n = cell_seg_i = 0;
    }
}


/* The power-on splash (DUELL 0.1 beta; main.c fm1_main, web/emu felucca_web.c): the title block, left-aligned and vertically
 * centred, no panel or frame, in ONE colour (T_TEXT, the theme's text colour; POLICE its own). It is a graphic, the art
 * director's lockup (assets/splash_logo.svg, rasterised at build time to a coverage mask, splash_logo.h), not drawn from a font:
 *     DUELL / MultiMorphing / Bitimbral Synthesizer / by OSCILL/R
 * over the thin morphing wave lines of the MORPH page (ui_graph.c mv_*: thin lines, no centre line, calm): SPL_N lines in a
 * slow ribbon (its middle drifting, its width breathing), smooth on the left, with overtones and ripples further right as the
 * morph sweeps across, coloured from the SELECTED THEME's part 1 colour (the top line) to its part 2 colour (the bottom one):
 * violet to pink in VIOLET. POLICE: flat red lines above, flat blue below, no blend (the palette's c[0], c[2]). The lines run on unbroken behind the text, which is drawn on top: no box, no gap, no halo. Drawn while the
 * synth already runs (audio_init is before it; notes play through it), not by a delay: the first frame at lcd_init, then
 * splash_begin (main.c) and the main loop's ui_draw draw a step every SPL_STEP_MS until SPLASH_MS are over (then the normal
 * screen at once) or any button, key or knob (splash_input, ui_input.c) ends it; MIDI notes never do. MENU > ANIM OFF: the
 * first frame stays still. MENU > SYSTEM > ABOUT shows the same drawing (splash_frame) as its first view, continuously, with no end (ui_menu.c). In the saved palette (settings_init before it). Integer only; three strips of the screen
 * (the mask is clipped to each). DUELL 0.2.11: while the GUIDE has not been seen (ui.c PREF_GUIDE_SEEN clear: a new unit, an older build's
 * settings) the splash stays GUIDE_SPLASH_MS longer and the GUIDE opens as it ends, by its time or by a key (ui_guide.c) */
#define SPLASH_MS 1200u                             /* how long (any key ends it) */
#define GUIDE_SPLASH_MS 500u                        /* .. longer before the first-start GUIDE */
#define SPL_STEP_MS 66u                             /* one animation step (~15 /s, as the MORPH page's) */
#define SPL_N 15                                    /* the lines */
static uint8_t splash_on;
static uint8_t splash_guide;                        /* the GUIDE follows this splash (its bit was clear at splash_begin) */
static uint32_t splash_t0, splash_fr;
/* The lockup (the art director's SVG, rasterised by tools/gen_splash_logo.py into build/gen/splash_logo.h: 4-bit coverage,
 * byte = value << 4 | (run - 1)), blended in T_TEXT over what the strip holds; the strip starts at screen row y0, h rows.
 * No RAM: the run list is walked from its start, only the pixels inside the strip are touched. */
static void splash_logo_draw(int32_t y0, int32_t h)
{
    uint16_t tx = T_TEXT;
    uint32_t i, p = 0;
    uint8_t gray = (tx >> 11) == (tx & 31u);            /* (GREY / MONO: one channel blended, so the pixel stays neutral) */
    for (i = 0; i < (uint32_t)sizeof SPL_LOGO; i++) {
        uint32_t v = SPL_LOGO[i] >> 4, n = (SPL_LOGO[i] & 15u) + 1u, e = p + n;
        for (; v && p < e; p++) {
            int32_t py = SPL_LOGO_Y0 + (int32_t)(p / SPL_LOGO_W) - y0, px = SPL_LOGO_X0 + (int32_t)(p % SPL_LOGO_W), a = (int32_t)(v * 100u / 15u);
            uint16_t c;
            if (py < 0 || py >= h)
                continue;
            c = swap16(cv_px[py * 240 + px]);
            if (gray && (c >> 11) == (c & 31u)) {
                int32_t r = c >> 11, t = tx >> 11;
                cv_px[py * 240 + px] = swap16(ux_gray((uint32_t)(r + ((t - r) * a + 50) / 100)));
            } else
                cv_px[py * 240 + px] = swap16(ux_mix(c, tx, a));
        }
        p = e;
    }
}
/* MENU > ABOUT's key hints (0.2.9, the owner: KNOB 1 scrolls, SELECT goes left and right between its sections; OCT- back, lit) */
static const khint_t ABOUT_KEYS[2] = {{KC_K1, "SCROLL"}, {KC_SELECT, "HEADINGS"}};   /* (DUELL 1.1: SELECT jumps to the next heading: the logo, ABOUT, CREDITS, LICENSES) */
#define ABOUT_KY 206                                   /* ABOUT's legend: the first row's keycap top; the second row 17 px lower (item 119: the three hints need 237 px of the 224 a row has, so BACK has its own row, as PAGES_V2_menu.png) */
#define ABOUT_KDY 17
static void about_keys(int32_t y)                      /* SCROLL and HEADINGS on one row, OCT- BACK under it: every word whole */
{
    cv_key_row(8, 232, y, ABOUT_KEYS, 2u, 3u, T_BG);
    cv_key_hint(8, y + ABOUT_KDY, KC_OCTDN, "BACK", 1, T_BG);
}
/* hint_y: 0 the splash; else the screen row of MENU > ABOUT's key hints (about_keys), drawn on top of the last strip */
static void splash_frame(uint32_t st, int32_t hint_y)
{
    static const int32_t EDGE[4] = {0, 96, 192, 240};    /* the strips: no text row lies across an edge (2026-11-10: 108 -> 104, then 96 -> 192 for the button scan in RAM: the canvas holds 96 rows of 240, gfx.c CV_ROWS) */
    const ui_pal_t *pl = &UI_PALETTES[ux.pal % NPALETTES];
    uint16_t col[SPL_N];
    uint32_t band, x, j, m = st * 0x00800000u;          /* the motion: 2^32 a turn */
    int32_t j2;
    for (j = 0; j < (uint32_t)SPL_N; j++)               /* the part 1 colour to the part 2 colour; HC: two flat halves */
        col[j] = pl->hc ? pl->c[j >= (uint32_t)SPL_N / 2u ? 2 : 0]
                        : ux_mix(ux_mix(pl->c[0], pl->c[2], (int32_t)j * 100 / (SPL_N - 1)), T_BG, 18);
    for (band = 0; band < 3u; band++) {
        int32_t y0 = EDGE[band], h = EDGE[band + 1] - y0;
        cv_begin(240, (uint32_t)h, T_BG);
        cv_oy = -y0;
        for (j = 0; j < (uint32_t)SPL_N; j++) {
            int32_t px = 0, py = 0, pen = 0;
            j2 = (int32_t)j * 2 - (SPL_N - 1);          /* -14 .. +14: the line's place across the ribbon */
            for (x = 0; x < 240u; x += 2u) {
                uint32_t xt = x * 17895697u / 100u;     /* x in hundredths of a turn per 1.00 cycle across the width */
                int32_t mc = 120 + (sine_i(xt * 90u + m) * 22 >> 15);                      /* the ribbon's middle, 0.9 cycles */
                int32_t w = 170 + (sine_i(xt * 130u + m * 2u + 0x40000000u) * 40 >> 15);   /* its width, 130 .. 210 */
                int32_t mo = clamp(((int32_t)x - 120) * 256 / 240 + 128 + (sine_i(m) >> 8), 0, 256);   /* the morph: 0 left .. 256 right */
                int32_t calm = sine_i(xt * 100u + j * 0x05000000u + m);                    /* the smooth wave every line has */
                int32_t rip = (sine_i(xt * 370u + j * 0x0C000000u + m * 3u) * 3 + sine_i(xt * 710u - j * 0x1B000000u + m * 5u) * 2) / 5;   /* overtones and ripples */
                int32_t y = mc + j2 * w / (2 * (SPL_N - 1)) + (int32_t)(((int64_t)calm * 5 + (int64_t)rip * mo / 256 * 4) >> 15);
                if (pen)
                    cv_line(px, py, (int32_t)x, y, col[j]);
                px = (int32_t)x;
                py = y;
                pen = 1;
            }
        }
        cv_oy = -y0;
        splash_logo_draw(y0, h);                        /* (the lockup, on top of the unbroken lines) */
        if (hint_y)                                     /* (in every strip: the canvas clips it, so it is whole whichever edge it crosses) */
            about_keys(hint_y);
        if (!hint_y && splash_guide && band == 2u) {      /* (the last strip, rows 192..239: its text from row 199) */     /* DUELL 1.1: the first start only: a small hint under the DUELL text (LOGO_SCREENS_V1.png), OCT + goes on, HOME skips */
            static const khint_t FIRST[2] = {{KC_OCTUP, "NEXT"}, {KC_HOME, "SKIP"}};
            cv_text(8, 199, &AF_S, "GUIDE FOR YOUR FIRST START", T_MID);
            cv_key_row(8, 232, 217, FIRST, 2u, 3u, T_BG);
        }
        cv_oy = 0;
        cv_blit(0, (uint32_t)y0);
    }
    lcd_sync();
}
static void draw_splash(void) { splash_frame(0, 0); }  /* the first frame, at lcd_init (main.c, felucca_web.c) */
static void splash_begin(void)                      /* the synth runs: the splash proper starts (main.c, felucca_web.c) */
{
    splash_on = 1;
    splash_guide = (uint8_t)!guide_seen();
    splash_t0 = fm1_ms;
    splash_fr = 0;
}
static void splash_end(void)                        /* the UI takes the screen: all of it drawn again */
{   /* item 162 (owner, test10: "a very brief flicker when the device started"): no whole-screen fill here any more. It blanked the panel (~77 ms of
     * SPI at 12 MHz), the page came a frame later and draw_frame filled the body AGAIN: splash -> blank screen -> page. Now the page is drawn over
     * the splash in the same frame (ui_draw below): the header and footer strips land straight on it, the body is filled once by draw_frame (ui.force 1) */
    splash_on = 0;
    kb_boot_hold = 0;
    ui.force = 1;
    if (splash_guide) {                             /* DUELL 0.2.11: the first-start GUIDE */
        splash_guide = 0;
        guide_open(0);
    }
}
static void splash_draw(void)                       /* ui_draw while it is up: a step when due, the end when over */
{
    uint32_t st;
    if (kb_boot_hold && fm1_ms - splash_t0 >= 100u)  /* (#137: the keys are silent for the first 0.1 s only) */
        kb_boot_hold = 0;
    if (fm1_ms - splash_t0 >= (splash_guide ? 60000u : SPLASH_MS)) {   /* (the first start waits for OCT + or HOME, 60 s at most) */
        splash_end();
        return;
    }
    st = (ui_prefs & PREF_ANIM_OFF) ? 0u : (fm1_ms - splash_t0) / SPL_STEP_MS;
    if (st == splash_fr)
        return;
    splash_fr = st;
    splash_frame(st, 0);
}

/* UPDATE MODE countdown (main.c: OCT- + OCT+ held): over everything, the menu and the dialogs too */
static void draw_uboot(void)
{
    static uint8_t shown;
    char d[4] = {(char)('0' + ui.uboot % 10u), 0, 0, 0};
    if (!ui.force && shown == ui.uboot)
        return;
    shown = ui.uboot;
    lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    ui.head_sig = ~0u;
    draw_text_box(0, 76, 240, &AF_M, "STARTS IN", T_TEXT, 1);   /* (the title, UPDATE MODE, is the header's) */
    draw_text_box(0, 102, 240, &AF_L, d, T_THEME, 1);
    {   /* the two keycaps held, and what letting go does */
        int32_t w = kc_w(KC_OCTDN) + 3 + kh_ink(KC_OCTUP, "LET GO TO CANCEL"), x = HALF_UP(240 - w);
        cv_begin(240, KC_H + 6, T_BG);                  /* (item 119: S 15's word is 19 rows high: the strip holds it whole, the keycaps stay on row 150) */
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(3), "update-mode keys centred");
        x = cv_keycap(x, 3, KC_OCTDN, T_KEY, T_KINK, T_BG) + 3;
        cv_key_hint(x, 3, KC_OCTUP, "LET GO TO CANCEL", 1, T_BG);
        cv_blit(0, 147);
        lcd_sync();
    }
    ui.force = 0;
}

/* the count-in (seq.c, MENU > COUNT-IN; 1.1, Discussion #131): over the page until the sequencer starts. Its bar's four
 * beats counted down, 4 3 2 1, in four cells (the beat now filled THEME, those gone dim), with 2 BARS which bar it is;
 * PLAY stops it. Drawn again on each beat (seq.c's state; the header stays live) */
#define CI_Y 88                                       /* the cells: 4 x 48 px, 8 px apart, 72 tall */
#define CI_H 72
static void draw_countin(void)
{
    static uint32_t shown;
    uint32_t total = cin_total, e = total - cin_left, j, sig = total * 64u + e + 1u;
    char sub[16];
    if (!ui.force && shown == sig)
        return;
    shown = sig;
    lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    ui.head_sig = ~0u;
    if (total > 4u) {
        str_cpy(sub, "BAR 1 OF 2", sizeof sub);
        sub[4] = (char)('1' + e / 4u % 9u);
        sub[9] = (char)('0' + total / 4u % 10u);
    } else
        str_cpy(sub, "1 BAR", sizeof sub);
    draw_text_box(0, 56, 240, &AF_S, sub, T_MID, 1);   /* (the title, COUNT-IN, is the header's) */
    cv_begin(216, CI_H, T_BG);
    for (j = 0; j < 4u; j++) {
        int32_t x = (int32_t)j * 56;
        char d[2] = {(char)('4' - j), 0};
        int now = j == e % 4u, gone = j < e % 4u;
        uint16_t fill = now ? T_THEME : T_BG;
        { CV_HL(x, 0, 48, CI_H); cv_rrect(x, 0, 48, CI_H, 8, fill, T_BG); }
        GFX_HOOK_ALIGN(x, 0, x + 48, CI_H, AL_HV, "count-in beat centred in its cell");
        cv_text_in(x, CAP_IN(L, CI_H), 48, &AF_L, d, now ? T_INK : gone ? T_DIM : T_TEXT, fill);
    }
    cv_blit(12, CI_Y);
    draw_text_box(0, CI_Y + CI_H + 14, 240, &AF_S, "RECORDING STARTS AFTER 1", T_MID, 1);
    {   /* PLAY stops it */
        int32_t x = HALF_UP(240 - kh_ink(KC_PLAY, "STOP"));
        cv_begin(240, KC_H + 6, T_BG);                  /* (item 119: as UPDATE MODE's strip, the keycap stays on row 207) */
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "count-in key centred");
        cv_key_hint(x, 3, KC_PLAY, "STOP", 1, T_BG);
        cv_blit(0, 204);
        lcd_sync();
    }
    ui.force = 0;
}

/* the OCT- / OCT+ dialog: what it does (ui.confirm, ui.confirm_trk) on two lines, on a surface */
#define DLG_X 16
#define DLG_Y 62
#define DLG_W 208
#define DLG_H 116
static void confirm_text(char *a, char *b)          /* a, b hold 32 */
{
    uint32_t k = ui.confirm_trk;
    b[0] = 0;
    switch (ui.confirm) {
    case CF_CLEAR_TRK:
        str_cpy(a, "CLEAR TRACK 1?", 24);
        a[12] = (char)('1' + k % NTRK);
        break;
    case CF_OVR_PROJ:                                   /* MULTI: SAVE over a used multi */
        str_cpy(a, "OVERWRITE ", 24);
        mu_label(a + str_len(a), k % MU_SLOTS);
        str_cpy(a + str_len(a), "?", 2);
        project_name(k % MU_SLOTS, b);                  /* the multi's name ("" none) */
        break;
    case CF_OVR_PSLOT:                                  /* PATTERNS: SAVE over what the slot holds */
        str_cpy(a, "OVERWRITE ", 24);
        ps_label(a + str_len(a), k % PS_SLOTS);
        str_cpy(a + str_len(a), "?", 2);
        {                                               /* its name */
            char tag[4];
            pat_label(k % PS_SLOTS, tag, b);
        }
        break;
    case CF_HOLD_PSLOT:                                 /* SAVE held on PATTERNS: "SAVE OVER MELODY?", the slot under it */
    case CF_HOLD_MULTI: {                               /* SAVE held on MULTIS */
        char tag[4];
        str_cpy(a, "SAVE OVER ", 24);
        if (ui.confirm == CF_HOLD_PSLOT) {
            pat_label(k % PS_SLOTS, tag, b);
        } else {
            mu_label(tag, k % MU_SLOTS);
            project_name(k % MU_SLOTS, b);
        }
        str_cpy(a + str_len(a), b[0] ? b : tag, 13);
        str_cpy(a + str_len(a), "?", 2);
        str_cpy(b, tag, 24);
        break;
    }
    case CF_INIT_SOUND:
        str_cpy(a, "INITIALIZE SOUND?", 24);
        break;
    case CF_SAVE_OVER: {                                /* SAVE held: the sound over a slot (ui_input.c patch_save_ask; K1 picks the slot) */
        char n[2][13], l[6], src[13];
        if (k >= 2u) {                                  /* both parts, each over its own slot */
            uint32_t i;
            str_cpy(a, "OVERWRITE BOTH PATCHES?", 32);
            b[0] = 0;
            for (i = 0; i < 2u; i++) {
                up_slot_label(l, slot_home(&trk[i]));
                str_cpy(b + str_len(b), i ? " + " : "", 4);
                str_cpy(b + str_len(b), l, 6);
            }
            break;
        }
        sl_name(ui.svs % UP_SLOTS, n[0]);
        up_slot_label(l, ui.svs % UP_SLOTS);
        if (save_nm[0])
            str_cpy(src, save_nm, sizeof src);           /* 0.2.21: RENAME (K2) */
        else
            sound_name(&trk[k], src);
        str_cpy(a, "OVERWRITE ", 32);
        str_cpy(a + str_len(a), l, 6);
        str_cpy(a + str_len(a), " ", 2);
        str_cpy(a + str_len(a), n[0], 14);
        str_cpy(a + str_len(a), "?", 2);
        str_cpy(b, "WITH ", 32);
        str_cpy(b + str_len(b), src, 14);
        break;
    }
    case CF_CLEAR_MOTION:
        str_cpy(a, "CLEAR T1 PARAMETER LOCKS?", 32); a[7] = (char)('1' + k % NTRK);
        break;
    case CF_RESTORE_ALL:                            /* SAVE > TOOLS: every slot back to its factory sound */
        str_cpy(a, "RESTORE ALL PATCHES?", 32);
        str_cpy(b, "SAVED SOUNDS ARE LOST", 32);
        break;
    case CF_LOAD_PAT: {
        char tag[4];
        str_cpy(a, "REPLACE T1 SEQUENCE?", 24);
        a[9] = (char)('1' + k % NTRK);
        pat_label(pat_pick(), tag, b);               /* with the slot picked ("MELODY", "SAVED") */
        {
            char nm[13];
            str_cpy(nm, b, sizeof nm);
            str_cpy(b, tag, 24);
            str_cpy(b + str_len(b), " ", 2);
            str_cpy(b + str_len(b), nm, 24 - str_len(b));
        }
        break;
    }
    default:
        str_cpy(a, "CLEAR T1 SEQUENCE?", 24);     /* the header (T1..T4) is hidden */
        a[7] = (char)('1' + k % NTRK);
        break;
    }
}
/* MENU > SYSTEM > ERASE DATA's dialog (DUELL 0.2.2): exactly what goes and what stays, then NO / YES as every dialog. Item 119: at the floor (S 15) the same words take five lines of at most 200 px on a card 224 px wide
 * (x 8 .. 231); the card is 188 rows high, the canvas holds 124: two strips (it had been cut at row 143, the NO / YES buttons were not drawn) */
static const char *const ERASE_LINES[5] = {"SETTINGS, FAVOURITES,", "AUTOSAVE, SAVED PATCHES,", "MULTIS, PATTERNS.",
                                           "FACTORY SOUNDS COME BACK.", "THIS CANNOT BE UNDONE."};   /* (DUELL 1.1, item 91: the FM6 voice bank is erased too, its line is gone) */
#define ERASE_X 8
#define ERASE_W 224
#define ERASE_H 188
#define ERASE_BY 150
#define ERASE_STRIP 100
static void draw_confirm_erase(void)
{
    const char *const *L = ERASE_LINES;
    const int32_t H = ERASE_H, BY = ERASE_BY, W = ERASE_W;
    uint32_t i, strip;
    lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    ui.head_sig = ~0u;
    for (strip = 0; strip < 2u; strip++) {
        const int32_t y0 = strip ? ERASE_STRIP : 0;
        cv_begin((uint32_t)W, strip ? (uint32_t)(H - ERASE_STRIP) : (uint32_t)ERASE_STRIP, T_BG);
        cv_oy = -y0;
        GFX_HOOK_ALIGN(0, 0, W, 0, AL_H, "dialog icon centred");
        /* (DUELL 1.1: no warning triangle: the words say it) */
        GFX_HOOK_ALIGN(0, 0, W, 0, AL_H, "dialog question centred");
        cv_text_in(0, 20, W, &AF_M, "ERASE ALL MY DATA?", T_TEXT, T_BG);
        for (i = 0; i < 5u; i++) {
            GFX_HOOK_ALIGN(0, 0, W, 0, AL_H, "dialog detail centred");
            cv_text_in(0, 46 + (int32_t)i * 17, W, &AF_S, L[i], i == 4u ? T_ACCENT : T_MID, T_BG);
        }
        GFX_HOOK_ALIGN(16, BY, 106, BY + 26, AL_HV | AL_N(2), "dialog button NO");
        cv_key_hint(16 + HALF_UP(90 - kh_ink(KC_OCTDN, "NO")), BY + HALF_UP(26 - KC_H), KC_OCTDN, "NO", 1, T_BG);
        { CV_HL(118, BY, 90, 26); cv_rrect(118, BY, 90, 26, 6, T_THEME, T_BG);    /* OCT+: YES */ }
        {
            int32_t x = 118 + HALF_UP(90 - kh_ink(KC_OCTUP, "YES"));
            GFX_HOOK_ALIGN(118, BY, 208, BY + 26, AL_HV | AL_N(2), "dialog button YES");
            x = cv_keycap(x, BY + HALF_UP(26 - KC_H), KC_OCTUP, T_INK, T_THEME, T_THEME);
            cv_text_on(x + KH_GAP, BY + HALF_UP(26 - KC_H) - 3, &AF_S, "YES", T_INK, T_THEME);   /* (item 122: the capitals centred on the pill) */
        }
        cv_oy = 0;
        cv_blit(ERASE_X, (uint32_t)(H_HEAD + 6 + y0));
    }
}
static void draw_confirm(void)
{
    char a[32], b[32];
    const aafont_t *tf = &AF_M;
    int hint = ui.confirm == CF_SAVE_OVER && ui.confirm_trk < 2u;   /* the slot picker's hint line (K1) */
    int32_t dh = hint ? 22 : 0, by = 80 + dh;
    b[0] = 0;
    if (ui.confirm == CF_ERASE_ALL) {
        draw_confirm_erase();
        return;
    }
    confirm_text(a, b);
    lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    ui.head_sig = ~0u;
    CV_BANDS(DLG_W, DLG_H + dh, T_BG) {                /* a SURF card: warning, the question, the detail, two buttons (in bands: gfx.c CV_BANDS) */
        GFX_HOOK_ALIGN(0, 0, DLG_W, 0, AL_H, "dialog icon centred");
        /* (DUELL 1.1: no icon above the question) */
        if (text_w(tf, a) > DLG_W - 16)
            tf = &AF_S;
        GFX_HOOK_ALIGN(0, 0, DLG_W, 0, AL_H, "dialog question centred");
        cv_text_in(0, 34, DLG_W, tf, a, T_TEXT, T_BG);
        if (b[0]) {
            char f[32];
            text_fit(f, sizeof f, b, &AF_M, DLG_W - 16);    /* a name: free text (M: what is selected, as the question) */
            GFX_HOOK_ALIGN(0, 0, DLG_W, 0, AL_H, "dialog detail centred");
            cv_text_flags(ink_in(&AF_M, f, DLG_W), 55, &AF_M, f, T_MID, T_BG, 8u | (f[str_len(f) - 1u] == ELLIPSIS));
        }
        if (hint) {                                       /* SAVE held: K1 picks the slot the sound goes to */
            GFX_HOOK_ALIGN(0, 78, DLG_W, 78 + KC_H, AL_H | AL_N(4), "dialog hint centred");
            int32_t w1 = kh_ink(KC_K1, "OTHER SLOT"), w2 = kh_ink(KC_K2, "RENAME"), x0 = HALF_UP(DLG_W - (w1 + 14 + w2));   /* K1 OTHER SLOT, K2 RENAME (0.2.21) */
            cv_key_hint(x0, 78, KC_K1, "OTHER SLOT", 1, T_BG);
            cv_key_hint(x0 + w1 + 14, 78, KC_K2, "RENAME", 1, T_BG);
        }
        GFX_HOOK_ALIGN(10, by, 100, by + 26, AL_HV | AL_N(2), "dialog button NO");
        cv_key_hint(10 + HALF_UP(90 - kh_ink(KC_OCTDN, "NO")), by + HALF_UP(26 - KC_H), KC_OCTDN, "NO", 1, T_BG);   /* (ink centred) */
        { CV_HL(108, by, 90, 26); cv_rrect(108, by, 90, 26, 6, T_THEME, T_BG);    /* OCT+: YES (on the THEME fill: an INK keycap, THEME label) */ }
        {
            int32_t x = 108 + HALF_UP(90 - kh_ink(KC_OCTUP, "YES"));
            GFX_HOOK_ALIGN(108, by, 198, by + 26, AL_HV | AL_N(2), "dialog button YES");
            x = cv_keycap(x, by + HALF_UP(26 - KC_H), KC_OCTUP, T_INK, T_THEME, T_THEME);
            cv_text_on(x + KH_GAP, by + HALF_UP(26 - KC_H) - 3, &AF_S, "YES", T_INK, T_THEME);   /* (item 122: the capitals centred on the pill) */
        }
        cv_blit(DLG_X, DLG_Y + (uint32_t)cv_by);
    }
}

static void ui_draw_page(uint32_t counting);
static void draw_foot(void);
static void draw_oct_page(void)
{
    static int32_t shown = 99;
    int32_t o = song.octave, i;
    if (ui.force || o != shown) {
        char b[4];
        shown = o;
        b[0] = o > 0 ? '+' : o < 0 ? '-' : '0';
        b[1] = o ? (char)('0' + (o < 0 ? -o : o)) : 0;
        b[2] = 0;
        CV_BANDS(240, 124, T_BG) {                      /* (in bands: gfx.c CV_BANDS) */
            cv_text_in(0, 16, 240, &AF_B, b, T_TEXT, T_BG);
            cv_blit(0, H_HEAD + (uint32_t)cv_by);
        }
        cv_begin(240, (uint32_t)(Y_FOOT - H_HEAD - 124), T_BG);
        for (i = -3; i <= 3; i++) {                     /* the seven octaves, the one now larger and white */
            int32_t cx = 120 + i * 18, cy = 28;
            if (i == o)
                cv_rrect(cx - 5, cy - 5, 10, 10, 5, T_TEXT, T_BG);
            else
                cv_rrect(cx - 4, cy - 4, 8, 8, 4, T_DIM, T_BG);
        }
        cv_blit(0, H_HEAD + 124);
    }
    draw_head();
    draw_foot();
    ui.force = 0;
}
static void ui_draw(void)
{
    static uint8_t counting;
    /* the theme follows the selected part (SPEC 5a), its MORPH fader (colour A -> B), LAYER mode and (0.2.24) a GLOBAL page: the third colour (cheap: only on a change). A quick layer over
     * a global page and PERFORM are per part: the part's colours */
    {   /* item 152 A: a fader moved (MORPH, a modulated fader) shifts the part colour A -> B only: the page is drawn again WITHOUT the BG fill (ui_refresh); the part, the layer or a GLOBAL page changing the background fills as before */
        uint8_t keep = (uint8_t)(ux.part == (song.sel & 1u) && ux.layer == (uint8_t)(duell_mode_req == DM_LAYER) && ux.glob == (uint8_t)(screen_global() != 0));
        uint16_t bg0 = ux.bg;
        if (theme_follow(song.sel, morph_fader_of(0), morph_fader_of(1), duell_mode_req == DM_LAYER, screen_global())) {
            if (keep && bg0 == ux.bg)
                ui_refresh();
            else
                ui.force = 1;
        }
    }
    ui.frame++;
    {   /* the OCTAVE page began or ended: the page under it is drawn whole again */
        static uint8_t oct_was;
        uint8_t on = (uint8_t)oct_show_on();
        if (on != oct_was) {
            oct_was = on;
            ui.force = 1;
        }
    }
    if (counting != (uint8_t)seq_counting()) {          /* a count-in began or ended: the page drawn again after it */
        counting = (uint8_t)seq_counting();
        ui.force = 1;
    }
    if (!scr_frame())                                   /* MENU > SCREEN OFF: dark (or waking), nothing drawn */
        return;
    if (splash_on) {                                    /* the power-on splash (above): the page waits under it */
        splash_draw();
        if (splash_on)                                  /* (item 162: the frame the splash ends draws the page at once, no blank frame between) */
            return;
    }
    ui_draw_page(counting);
    scr_shown();
}
static void ui_draw_page(uint32_t counting)
{
    {   /* item 120: the PERFORM page (not over a layer, menu or dialog) has its own map rows and a two-line footer hint (Y_GRAPH, H_GRAPH, draw_foot) */
        uint8_t pp = (uint8_t)(ui.perf && cur_page()->scope == SC_PERF && !(ui.layer || ui.menu || ui.guide || ui.uboot || ui.confirm || name_on() || counting || oct_show_on() || glo_pg()));
        if (pp != perf_pg) {
            perf_pg = pp;
            ui.force = 1;
        }
    }
    {   /* the MIXER (GLO's page, and QUICK GLO over any page) is a grid screen (7): PAGES_V2_glo-a.png; every other layer and screen uses the old layout */
        uint8_t quiet = !(ui.menu || ui.guide || ui.uboot || ui.confirm || name_on() || counting || oct_show_on());
        uint8_t want = perf_pg ? 16u                    /* TEMPLATE 9: the PERFORM page (kept here too, so that the line below does not drop it every frame) */
                     : (ui.layer == LAYER_GLO || (!ui.layer && glo_pg())) && quiet ? 7u
                     : off_layer(ui.layer) && quiet ? 13u   /* item 125: QUICK ARP / QUICK SEQ with the module OFF: the notice */
                     : ui.layer == LAYER_SEQ && quiet ? 16u   /* TEMPLATE 9 (QUICK_SEQ_NEW_LAYOUT): knob 1's line, the tools' boxes in the area, three cards */
                     : ui.layer == LAYER_REC && quiet ? 14u   /* TEMPLATE 8 (QUICK_REC_NEW_LAYOUT, block E): four cells CLICK, COUNT, LEVEL, CLEAR, 2 x 2 */
                     : (ui.layer == LAYER_EDIT || ui.layer == LAYER_LFO || ui.layer == LAYER_ENV || ui.layer == LAYER_ARP) && quiet ? 1u : 0u;   /* item 120: the four QUICK EDIT layers are template 1 (a group card over four cards) */
        if (want == 16u && (area_cols != 3u || area_two)) {   /* (QUICK SEQ: three one-line cards, DIVISION, SWING, GATE) */
            area_cols = 3u;
            area_two = 0;
            ui.force = 1;
        }
        if (want == 14u && ui.layer == LAYER_REC && cell_n != 4u) {   /* (QUICK REC: the four cells always show, knob c in cell c) */
            uint32_t c;
            cell_n = 4u;
            for (c = 0; c < 4u; c++)
                cell_at[c] = (uint8_t)c;
            ui.force = 1;
        }
        if (grid_on && grid_on != want && (ui.layer || ui.menu || ui.guide || ui.uboot || ui.confirm || name_on() || counting || ui.perf || oct_show_on() || glo_pg())) {
            grid_on = 0;                                /* (the other screens use the old layout) */
            ui.force = 1;
        }
        if (want && grid_on != want) {
            grid_on = want;
            ui.force = 1;
        }
    }
    if (ui.uboot) {
        draw_uboot();
        draw_head();
        return;
    }
    if (ui.guide) {                                     /* DUELL 0.2.11: the GUIDE, over the page or the MENU it was opened from (ui_guide.c) */
        guide_draw();
        return;
    }
    if (oct_show_on()) {                                /* DUELL 1.1: the OCTAVE page, over the page and a layer */
        draw_oct_page();
        return;
    }
    if (ui.menu && ui.confirm != CF_ERASE_ALL) {        /* (ERASE DATA's dialog is drawn over the menu's place, below) */
        draw_menu();
        ui.force = 0;
        return;
    }
    if (counting && !ui.confirm && !name_on()) {        /* the count-in: over the page (and a layer) */
        draw_countin();
        draw_head();
        return;
    }
    if (ui.confirm) {                                   /* the OCT- / OCT+ dialog */
        if (ui.force) {
            draw_confirm();
            ui.force = 0;
        }
        draw_head();                                    /* recording remains visible over confirmations */
        return;
    }
    if (name_on()) {                                    /* NAME: a user preset's or a project's (ui_name.c) */
        name_draw();
        return;
    }
    if (ui.layer || glo_pg()) {                         /* a layer's map over the page (ui_layer.c); GLO's page is one all the time */
        if (ui.force)
            draw_frame();
        draw_head();
        draw_layer();
        draw_foot();                                    /* (the page's footer, its button and kind in the page's place) */
        if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {
            str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
            ui.msg2[0] = 0;
            ui.msg_t = 60;
        }
        if (ui.hot_t)
            ui.hot_t--;
        if (ui.bpm_t)
            ui.bpm_t--;
        ui.force = 0;
        return;
    }
    if (!page_visible(ui.page)) {          /* a page the track has not (page_visible): EDIT 1; SHIMMER (the TYPE left SHIM, by the editor, a multi, MIDI): the page before it, REVERB */
        ui.page = (uint8_t)(PAGES[ui.page].fam == FAM_EDIT && !page_twin_sub(ui.page) ? page_first(FAM_EDIT) : page_next(-1));   /* (cloud job H: TWIN MOD / TWIN CHAIN, TYPE gone to OFF: the page before, TWIN FILTER) */
        if (!ui.perf)                                   /* (PERFORM over it stays: its tap of FX comes back to EDIT 1) */
            page_entered();
    }
    cursor_fix();
    {   /* DUELL 1.1: the pages redrawn as a grid (the picture on top, big values under it) */
        const page_t *gp = cur_page();
        uint8_t g = 0;
        if (!ui.perf && !page_over) {
            uint32_t gk = gp->graph;
            /* DUELL 1.1: every page of four plain cards, with or without a picture, is a grid page; the pages with a layout of their own stay as they are */
            if ((gp->scope == SC_TRACK || gp->scope == SC_ENGINE || gp->scope == SC_STEP) && gk != GR_MOD && gk != GR_ENGINE && gk != GR_USER &&
                gk != GR_TRK && gk != GR_SLCR)
                g = 1;
            if (gp->scope == SC_SCALE)
                g = 1;
            if (gp->scope == SC_MODS)                   /* MODULES (item 124): no cards, four lines on the background (graph_mods) */
                g = 12;
            if (gp->scope == SC_FXSUM)                  /* TEMPLATE 5 (item 117): the SENDS page, no cards, all text on the background (graph_fxsum) */
                g = 9;
            /* owner V13: the working pages keep their own arrangement, only the look is new: PATCHES, PATTERNS, MULTIS, TOOLS (the SAVE family), STEP (the piano roll), PARAMETER LOCKS and ARP are not grid pages */
            if (gp->fam == FAM_SAVE || gp->fam == FAM_ARP || gk == GR_ROLL || gk == GR_EVENTS || gk == GR_BROWSE || gk == GR_PATS || gk == GR_SLOTS || gk == GR_TOOLS)
                g = 0;
            if (gk == GR_TOOLS)                         /* TOOLS: its own grid value; the three rows are drawn whole by ui_graph.c graph_tools, no cards */
                g = 17;
            if (gp->scope == SC_GLOBAL && gp->fam == FAM_GLO && gk != GR_GLO)
                g = 1;
        }
        if (!ui.perf && !page_over && gp->scope == SC_MORPH && gp->graph == GR_MORPH)
            g = 1;
        if (!ui.perf && !page_over && gp->scope == SC_MORPH && gp->graph == GR_MORPHSET)
            g = 1;
        if (!ui.perf && !page_over && gp->graph == GR_ROLL)   /* TEMPLATE 7 (item 118): the STEP page, the big piano roll and four compact cards */
            g = 10;
        if (!ui.perf && !page_over && gp->graph == GR_SEQHELP)   /* SEQUENCER HELP (item 118): the legend page, drawn whole by ui_graph.c */
            g = 11;
        if (!ui.perf && !page_over && gp->scope == SC_MODE)
            g = 3;
        if (!ui.perf && !page_over && gp->scope == SC_TRACK && gp->graph == GR_MOD)
            g = 4;
        if (!ui.perf && !page_over && gp->graph == GR_ENGINE)
            g = eng_cells(gp) ? 15 : 5;                 /* (item 165: LOFI and NOISE have no picture: cells under the K1 row) */
        if (!ui.perf && !page_over && area_page(gp))   /* TEMPLATE 9 (block C): the AREA pages (over the ARP / list / STEP exceptions above: ARP 1/3, ARP 2/3, PARAMETER LOCKS, SEQUENCER, GLOBAL SCALE, CHORD, PATCHES, MULTIS, PATTERNS) */
            g = 16;
        if (perf_pg)                                    /* TEMPLATE 9: the PERFORM page (PERFORM_NEW_LAYOUT_V4: knob 1's line with SELECT = BALANCE, the 4 x 4 boxes in the area, three cards) */
            g = 16;
        if (!ui.perf && !page_over && gp->scope == SC_MIDI && gp->graph == GR_MLEARN)
            g = 6;
        if (!ui.perf && !page_over && (gp->scope == SC_MIDI && (gp->graph == GR_MIDI || gp->graph == GR_SCHAIN)))
            g = 2;
        if (!ui.perf && !page_over && cells_page(gp))   /* TEMPLATE 8 (item 156): the CELL pages, no picture, the cards that show as big cells */
            g = 14;
        if (!ui.perf && !page_over && off_module(gp))   /* item 125 (last, over every template): the notice of a switched-off module */
            g = 13;
        {
            static uint8_t list_was;
            int sw = (g == 15 && grid_on == 5) || (g == 5 && grid_on == 15);   /* (item 165: K1 turning onto LOFI / NOISE or off them) */
            int moved = (g == 14 || g == 15) && cells_plan(gp);
            if ((g == 16 || g == 5) && area_plan(gp, g))   /* (TEMPLATE 9: the card row's columns or its two-line row changed) */
                moved = 1;
            int lst = g == 15 && ui.hot_t && ui.hot_col == 0u;
            if (g != grid_on) {
                grid_on = g;
                if (sw) {                               /* (the same page in its other layout: the new layout's gaps, then everything drawn over without the body's fill) */
                    eng_gaps();
                    ui_refresh();
                } else
                    ui.force = 1;
            } else if (moved)                           /* (item 156: a card shows or hides, the cells move: the page drawn whole) */
                ui.force = 1;
            if (g == 15 && !lst && list_was)            /* (item 165: the engine list over the top cell ended: the cells drawn again, no fill) */
                ui_refresh();
            list_was = (uint8_t)lst;
        }
    }
    if (ui.force)
        draw_frame();
    felucca_dbg.stage = 3;
    draw_head();
    felucca_dbg.stage = 4;
    draw_columns();
    felucca_dbg.stage = 5;
    draw_graph();
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {     /* the second message (ui_notices) */
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
        ui.msg2[0] = 0;
        ui.msg_t = 60;
    }
    if (ui.bpm_t)
        ui.bpm_t--;
    if (ui.hot_t)
        ui.hot_t--;
    felucca_dbg.stage = 6;
    draw_foot();
    ui.force = 0;
}
