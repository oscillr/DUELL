/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI drawing: the header, the four knob cards, the footer (steps + engine / preset / page)
 * and the frame; the panel between the cards and the footer is ui_graph.c. The layout:
 * flat SURF cards and panels on BG,
 * rounded corners, no rules (MENU > STYLE LINE: no SURF, 1 px rules between the areas instead: draw_gaps), colours from the theme tokens only (gfx.c T_*).
 * Type, by role: M (15 px) what is read: values, titles, messages, every list's rows, the
 * selected item's detail; S (12 px) what describes it: labels, units, key hints (a keycap's word), the footer, map
 * and layer cells, graph annotations, document text; L (28 px) the one big readout of a screen. Where M cannot fit
 * (a card value with its unit wider than the card, a dialog question or a header message too long) it is set in S;
 * free text (names) is ellipsised at its size. */
static void draw_menu(void);
static int name_on(void);                              /* NAME (ui_name.c) */
static uint64_t lock_held(void);                       /* parameter locks: the steps held on STEP (ui_input.c) */
static int32_t morph_pending(uint32_t g);              /* the twists building up: MORPH's ROLL, UNDO, GLO's MUTE, SOLO (ui_input.c, 4b) */
static void name_draw(void);
static uint32_t test_slot(void);                       /* GLO > TEST: the multi K3 shows (ui_menu.c) */
#include "ui_help.c"                                   /* 1.5: HELP's hints (draw_foot, ui_layer.c draw_layer) */

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

/* REC: filled, the selected track armed; outlined, another track armed */
static void draw_rec_mark(int32_t x, uint16_t bg)
{
    if (song.rec)
        cv_icon_mid(x, H_HEAD / 2, 16, (song.rec >> song.sel) & 1u ? ICON_X_REC : ICON_X_REC_O, T_REC, bg);
}

/* the battery: a 16 px Fukiai icon. The stock thresholds give 0..3 bars of 3; the font has 0..4 bars of 4:
 * each level takes the nearest share, 0 -> battery_0 (empty), 1 (1/3) -> battery_1 (1/4), 2 (2/3) ->
 * battery_3 (3/4), 3 (full) -> battery_4 (battery_2 is not used); USB power: battery_charging.
 * Colours as the drawn battery had them: one bar left the accent, empty MID (its outline), else THEME */
static uint32_t batt_icon(int32_t lvl)
{
    static const uint8_t I[5] = {ICON_X_BAT0, ICON_X_BAT1, ICON_X_BAT3, ICON_X_BAT4, ICON_X_BAT_CHG};
    return I[clamp(lvl, 0, 4)];
}
static void draw_battery(int32_t bx)
{
    int32_t lvl = batt_shown();
    cv_icon_mid(bx, H_HEAD / 2, 24, batt_icon(lvl), lvl == 1 ? T_ACCENT : lvl == 0 ? T_MID : T_THEME, T_BG);   /* 24 px: the glyph is wide and short */
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
#define ROLL_Y 18                                       /* a card's value strip: rows 18..34, its figures (21..31) in the middle, */
#define ROLL_H 17                                       /* the label (MONO's chip: to row 17) above it, the gauge (38) below */
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
    uint16_t bg = k < ROLL_BPM ? T_SURF : T_BG;
    char t[8], ch[2] = {0, 0};
    if (e >= ROLL_FRAMES)
        ui.roll[k].from[0] = 0;
    if (!a[0])
        return cv_text_on(x, y, f, s, fg, bg);
    str_cpy(t, s, sizeof t);
    for (i = 0; s[i]; i++)
        p += (uint32_t)roll_digit(s[i]);
    cv_cy0 = (int16_t)(cy + cv_oy);                     /* the strip (the static characters lie inside it) */
    cv_cy1 = (int16_t)(cy + cv_oy + h);
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

/* the header (y 0..24), three calm slots (1.2):
 *   left    the selected track's cushion (ink 4..20), the PROJECT slot letter of its pattern ("1 A", head_slot), the REC
 *           mark attached (filled: it is armed; outlined: another). A message centred hides the letter (its room)
 *   centre  the play state (stop, play, the song's disc), the metronome and the BPM, its ink centred on x 120. The
 *           metronome swings on the click's beats (seq.c click_beat_now): mirrored
 *           on the bar's 2nd and 4th, the accent's on its 1st; stopped, still and MID
 *   right   the octave, or the song row while a song plays (ending at 208), and the battery (ink 216..236; its bolt
 *           says USB power: no separate USB icon)
 * A message or a layer's label replaces the centre and the octave, centred on x 120, up to
 * 166 px; while the BPM is being changed (GLO + F4 TAP: bpm_t) or in the GLO layer the centre stays and the message
 * goes right of the BPM (over the right slot). Every element's ink centred on row 12 (H_HEAD / 2; ui_test.c
 * test_head_centres): icons by cv_icon_mid, text by its capitals' band (CAP_IN; the odd row left over above it, as
 * everywhere): M (BPM, the song row, the octave, a message: capitals rows 6..16) from y 2, S (OCT, a key hint: rows 7..15)
 * from y 4 */
#define HEAD_SY CAP_IN(S, H_HEAD)
#define HEAD_MY CAP_IN(M, H_HEAD)
#define HEAD_TRK_X 3                                   /* the part badge: 1 2 side by side, 14 px cushions 15 apart (cv_part) */
#define HEAD_REC_X 32                                  /* the REC mark's cell (ink 35..45), after the slot letter if any */
#define HEAD_SLOT_X 35                                 /* the slot letter's pen (ui_draw.c head_slot) */
#define HEAD_BAT_X 214                                 /* the battery's cell */
#define HEAD_GRP_R 208                                 /* the octave / song row ends here */
#define HEAD_MSG_W 150                                 /* a message centred on x 120: 45..195 (after the REC mark) */
#define HEAD_GAP 4                                     /* ink to ink: the metronome -> the BPM -> the lock */
#define HEAD_PLAY_GAP 6                                /* the play state -> the metronome */
/* the header's centre: the pen of the BPM, its ink, the metronome's ink (x0 .. x1), the BPM's rolling strip (sx0 ..
 * sx1: between the metronome's cell, either way round, and the lock's). Placed by as many 0s as the BPM has digits
 * (tabular figures: the same advance), so nothing moves while the tempo changes, only with a digit more or less; the
 * BPM's ink then sits within a pixel or so of x 120 */
typedef struct { int32_t bx, b[4], mx0, mx1, sx0, sx1; } head_geo_t;
static void head_geo(head_geo_t *g, const char *bpm)
{
    int32_t ib[4], lb[4], c = 16;
    uint32_t sz = 16;
    char z[8] = "0000000";
    z[str_len(bpm) < 7u ? str_len(bpm) : 7u] = 0;
    g->bx = ink_in(&AF_M, z, 240);
    text_ink(&AF_M, z, g->b);
    icon_ink(16, ICON_TEMPO, ib);
    if (icon_cell(&sz, ICON_TEMPO) >= 0)
        c = (int32_t)sz;
    g->mx1 = g->bx + g->b[0] - HEAD_GAP;
    g->mx0 = g->mx1 - (ib[2] - ib[0]);
    g->sx0 = g->mx1 + (ib[0] > c - ib[2] ? ib[0] : c - ib[2]);
    icon_ink(16, ICON_X_LOCK, lb);
    g->sx1 = g->bx + g->b[2] + HEAD_GAP - lb[0];
}
/* the metronome with its ink at x0 (beat: click_beat_now; the 2nd and 4th mirrored, the same ink box; the 1st the
 * accent's). Mirrored by cv_flip: the cell's alpha drawn right to left, no second glyph */
static void head_metro(int32_t x0, int32_t beat)
{
    int32_t ib[4], c = 16;
    uint32_t sz = 16;
    icon_ink(16, ICON_TEMPO, ib);
    if (icon_cell(&sz, ICON_TEMPO) >= 0)
        c = (int32_t)sz;
    cv_flip = (uint8_t)(beat > 0 && (beat & 1));
    cv_icon_mid(x0 - (cv_flip ? c - ib[2] : ib[0]), H_HEAD / 2, 16, ICON_TEMPO, beat == 0 ? T_ACCENT : T_MID, T_BG);
    cv_flip = 0;
}
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
    int icon = m[0] == MSG_NOFILE[0];
    int32_t lw = lock ? 15 : 0, iw = icon ? 16 + KH_GAP : 0;   /* the lock: its cell 6 px on, its ink to 15 */
    int big = !head_is_hint(m + icon) && text_w(&AF_M, m + icon) <= maxw - lw - iw;
    if (x < 0)
        x = 120 - HALF_UP(iw + (big ? text_w(&AF_M, m + icon) : head_hint_w(m + icon, maxw - lw - iw)) + lw);
    if (icon)
        x += cv_icon_mid(x, H_HEAD / 2, 16, ICON_X_NOFILE, T_ACCENT, T_BG) + KH_GAP;
    if (big) {
        GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
        x = cv_text(x, HEAD_MY, &AF_M, m + icon, T_TEXT);
    } else {
        x = cv_free_hint(x, HEAD_SY, m + icon, T_TEXT, T_BG, maxw - lw - iw);   /* (may start with a keycap) */
    }
    if (lock)
        cv_icon_mid(x + 6, H_HEAD / 2, 16, ICON_X_LOCK, T_THEME, T_BG);
}
/* the slot mark after the parts' badge (1.2): the MULTI (DUELL; Felucca: the PROJECT slot) of the pattern the selected part
 * plays or edits: the multi the working pattern came from (ui.c pat_from: the last MULTI load or save) as its number 1..16,
 * with a mark ("5*") once it was edited since (its steps, LEN DIV SWING GATE, QUANTIZE or automation: ui.c pat_from_sig);
 * none at power-on, after a PATTERNS pattern or the editor's restore. b: the number (then '*' or nothing); 0 = none */
static int head_slot(char *b)
{
    uint32_t k = song.sel % NTRK, n;
    b[0] = b[1] = b[2] = b[3] = 0;
    if (!pat_from[k] || pat_from[k] > 16u)
        return 0;
    fmt_int(b, pat_from[k]);
    n = str_len(b);
    if (pat_from_sig(TSEL) != pat_from_h[k])
        b[n] = '*';
    return 1;
}
/* the SAVE family's pages (owner, 2026-10-11): the header's centre is the page's TITLE, what it edits, in place of the
 * transport and the tempo (the footer names the page the same word). 0 = another page */
static const char *head_title(void)
{
    if (ui.home)
        return 0;
    switch (cur_page()->graph) {
    case GR_BROWSE: return "PATCHES";
    case GR_PATS: return "PATTERNS";
    case GR_SLOTS: return "MULTIS";
    case GR_TOOLS: return "TOOLS";
    default: return 0;
    }
}
/* the octave, its right end at HEAD_GRP_R */
static void head_group(void)
{
    char b[8];
    int32_t x;
    {
        str_cpy(b, song.octave > 0 ? "+" : "", sizeof b);
        fmt_int(b + str_len(b), song.octave);
        x = HEAD_GRP_R - (text_w(&AF_S, "OCT") + 4 + text_w(&AF_M, b));
        GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
        x = cv_text(x, HEAD_SY, &AF_S, "OCT", T_MID);
    }
    GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
    cv_text(x + 4, HEAD_MY, &AF_M, b, vt(T_THEME, T_BG));
}
/* the parts the badge lights: the selected one; on the MORPH pages with TARGET BOTH, both */
static uint32_t badge_lit(void)
{
    uint32_t lit = 1u << song.sel;
    if (cur_page()->scope == SC_MORPH || cur_page()->scope == SC_TEST)
        lit |= morph_mask(song.sel);
    return lit;
}

static void draw_head(void)
{
    static int16_t metro_x = -1;                       /* the metronome's ink as drawn (-1: hidden), its beat */
    static int8_t metro_beat;
    char b[16];
    head_geo_t g;
    int32_t beat = -1;                                 /* the metronome stands still (its swing removed in 1.1.5) */
    uint32_t rec = (song.rec >> song.sel) & 1u ? 2u : song.rec != 0u;   /* 2 the selected track armed, 1 another */
    char sl[4];
    int slot = !ui.home && cur_page()->graph == GR_GLO ? 0 : head_slot(sl);   /* (GLO's page: no multi number beside the parts, owner 2026-10-12) */
    const char *ttl = head_title();                    /* SAVE pages: the title replaces the centre group */
    int32_t tx = ttl ? 120 - HALF_UP(text_w(&AF_M, ttl)) : 0;
    uint32_t sig = (uint32_t)song.playing * 3u + rec * 5u + (uint32_t)(song.octave + 8) * 11u + song.sel * 13131u +
                   (ui.msg_t ? str_hash(7u, ui.msg) : ui.layer * 7919u + (uint32_t)layer_locked() * 3u) + (uint32_t)song.g[G_BPM] * 101u + (ui.bpm_t != 0) * 31u +
                   (uint32_t)seq_counting() * 7u +
                   (uint32_t)batt_shown() * 7777u + badge_lit() * 4093u +
                   (slot ? str_hash(11u, sl) : 0u) + (ttl ? str_hash(13u, ttl) : 0u);
    int msg = ui.msg_t || ui.layer;
    int centre = !ttl && (!msg || ui.bpm_t || ui.layer == LAYER_GLO);   /* the BPM being changed: it stays, the message right */
    int32_t rx = HEAD_REC_X;
    if (song.g[G_BPM] != ui.roll_bpm) {
        char a[8];
        fmt_int(a, ui.roll_bpm);
        fmt_int(b, song.g[G_BPM]);
        roll_note(ROLL_BPM, a, b, ui.force);
        ui.roll_bpm = song.g[G_BPM];
    } else if (ui.force) {
        ui.roll[ROLL_BPM].from[0] = 0;
    }
    if (!centre)                                       /* (a message over the BPM: no roll there to finish; CLOCK's */
        ui.roll[ROLL_BPM].from[0] = 0;                 /* BPM knob turned while one shows) */
    fmt_int(b, song.g[G_BPM]);
    head_geo(&g, b);
    if (!ui.force && sig == ui.head_sig) {
        if (ui.roll[ROLL_BPM].from[0] && centre) {     /* rolling: the BPM's strip only */
            cv_begin((uint32_t)(g.sx1 - g.sx0), H_HEAD, T_BG);
            roll_text(ROLL_BPM, g.bx - g.sx0, HEAD_MY, b, ui.bpm_t ? T_ACCENT : T_THEME);
            cv_blit((uint32_t)g.sx0, Y_HEAD);
        }
        if (metro_x >= 0 && beat != metro_beat) {      /* a beat: the metronome's cell only (either way round) */
            int32_t cx = g.mx0 - (g.sx0 - g.mx1);
            metro_beat = (int8_t)beat;
            cv_begin((uint32_t)(g.sx0 - cx), H_HEAD, T_BG);
            head_metro(g.mx0 - cx, beat);
            cv_blit((uint32_t)cx, Y_HEAD);
        }
        return;
    }
    ui.head_sig = sig;
    metro_x = -1;
    cv_begin(240, H_HEAD, T_BG);
    {   /* the part badge: 1 2, only the part the settings affect (ALGORITHM's) lit in its colour, the other faded, in every mode incl. LAYER;
         * both lit only on the MORPH pages with MORPH TARGET BOTH (owner, 2026-10-11) */
        uint32_t k, lit = badge_lit();
        for (k = 0; k < NSHOWN; k++)
            cv_part(HEAD_TRK_X + (int32_t)k * 15, H_HEAD / 2 - 7, 14, k, (lit >> k) & 1u, T_BG);
    }
    if (slot && ttl) {                                 /* the title needs the room: the number only if it and the REC mark fit left of it */
        char l[3] = {sl[0], sl[1] == '*' ? 0 : sl[1], 0};
        if (HEAD_SLOT_X + text_w(&AF_M, l) + text_w(&AF_M, "*") + 1 + 14 + 4 > tx)
            slot = 0;
    }
    if (slot && (centre || !msg)) {                    /* the multi's number (not under a message centred: its room) */
        char l[3] = {sl[0], sl[1] == '*' ? 0 : sl[1], 0};
        uint32_t m = str_len(l);
        GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
        rx = cv_text(HEAD_SLOT_X, HEAD_MY, &AF_M, l, sl[0] == '-' ? T_MID : vt(T_THEME, T_BG));
        if (sl[m] == '*')                              /* edited since: the mark, up by the number's top */
            rx = cv_text(rx, HEAD_MY, &AF_M, "*", T_MID);
        rx += 1;                                       /* (the REC mark's cell after it, its ink 3 px on) */
    }
    draw_rec_mark(rx, T_BG);
    if (centre) {
        int32_t ib[4], lx = g.bx + g.b[2];             /* lx: the right end of the BPM (and its lock) */
        uint32_t id = song.playing ? ICON_X_PLAY : seq_counting() ? ICON_X_PLAY : ICON_X_STOP;
        icon_ink(16, id, ib);                          /* the play state: the song's disc, play (counting in: the
                                                        * accent's, about to play), stop */
        cv_icon_mid(g.mx0 - HEAD_PLAY_GAP - ib[2], H_HEAD / 2, 16, id,
                    song.playing ? T_THEME : seq_counting() ? T_ACCENT : T_MID, T_BG);
        head_metro(g.mx0, beat);
        metro_x = (int16_t)g.mx0;
        metro_beat = (int8_t)beat;
        if (!ui.roll[ROLL_BPM].from[0])
            GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
        roll_text(ROLL_BPM, g.bx, HEAD_MY, b, ui.bpm_t ? T_ACCENT : T_THEME);
        if (msg) {                                     /* right of the BPM, over the right slot */
            int32_t x = lx + 9 > 142 ? lx + 9 : 142;
            head_msg(x, 236 - x);
        }
    } else if (ttl && !msg) {
        GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
        cv_text(tx, HEAD_MY, &AF_M, ttl, T_TEXT);
    } else {
        head_msg(-1, HEAD_MSG_W);
    }
    if (!msg && song.octave && !(ttl && tx + text_w(&AF_M, ttl) + 8 > HEAD_GRP_R - 60))   /* (the title leaves the octave room: OCT -8 is ~56 px) */
        head_group();
    if (!(msg && centre))
        draw_battery(HEAD_BAT_X);
    cv_blit(0, Y_HEAD);
}
/* LINE: 1 px dividers in the BG gaps between the strips, not around them: under the header, above and
 * below the panel, and between the four columns over rows y .. y + h (the cards; the mixer's strips too) */
#define RULE_X0 CARD_X(0)
#define RULE_W (CARD_X(3) + CARD_W - CARD_X(0))
static void lcd_rule(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    lcd_fill(x, y, w, h, T_RULE);
    GFX_HOOK_RULE(x, y, w, h);
}
/* The gaps are filled around the rules, never over them (#181: a rule filled BG, then drawn again, flashed on every
 * redraw). Rows y0 .. y1 - 1 across the screen; LINE: the rule on its middle row */
static void gap_rows(uint32_t y0, uint32_t y1)
{
    uint32_t r = (y0 + y1) / 2u;
    if (!ux.style) {
        lcd_fill(0, y0, 240, y1 - y0, T_BG);
        return;
    }
    lcd_fill(0, y0, 240, r - y0, T_BG);
    lcd_fill(0, r, RULE_X0, 1, T_BG);
    lcd_rule(RULE_X0, r, RULE_W, 1);
    lcd_fill(RULE_X0 + RULE_W, r, 240u - RULE_X0 - RULE_W, 1, T_BG);
    lcd_fill(0, r + 1u, 240, y1 - r - 1u, T_BG);
}
/* the BG beside the four columns (the cards; MIXER's strips) over rows y .. y + h - 1; LINE: the dividers */
static void draw_gaps(uint32_t y, uint32_t h)
{
    uint32_t i;
    lcd_fill(0, y, (uint32_t)CARD_X(0), h, T_BG);
    for (i = 0; i < 4u; i++) {                          /* right of each column */
        uint32_t x = (uint32_t)(CARD_X(i) + CARD_W), w = i < 3u ? (uint32_t)CARD_X(i + 1u) - x : 240u - x;
        if (ux.style && i < 3u) {
            lcd_rule(x, y, 1, h);
            lcd_fill(x + 1u, y, w - 1u, h, T_BG);
        } else {
            lcd_fill(x, y, w, h, T_BG);
        }
    }
}
/* full redraw: the strips (header, cards, panel, footer) cover the rest; only the BG between them is filled */
static void draw_frame(void)
{
    uint32_t ch = card_h(), gy = graph_y(), gh = graph_h();   /* (MENU > LARGE: tall cards, a strip) */
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
/* MORPH on the cards: the value a track parameter's card shows is what the fader sounds like (morph.c morph_view), and the card
 * the morph is moving (shown != stored Original) carries a mark. card_mk_next (ui_fm6op.c): set by card_vv, taken by the next draw_column */
static int32_t card_vv(const int16_t *vp)
{
    uintptr_t id = ((uintptr_t)vp - (uintptr_t)TSEL->p) / sizeof *vp;
    int32_t v = *vp;
    if (id < P_COUNT && morph_view_on(song.sel)) {
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

/* MENU > LARGE, a tall card (ui.c LK_TALL; 57 x LG_CARD_H): which knob it is (a "K1" pill in M, the keycap's colours,
 * the accent while it turns, DIM when the knob does nothing here) and the icon at the right of it; the label in M
 * centred (S when M is too wide); the value centred in L, its figures twice the height of M (M, then S, when L is too
 * wide or the sparse L face lacks a glyph: gen_aa_font.py L_CHARS); the unit (S) centred under it; the gauge. The
 * value snaps (no rolling digits). MONO, the knob just turned: the label inverted as on the small cards. */
#ifndef LARGE_HOOK
#define LARGE_HOOK(s, why) ((void)0)                    /* host: a tall card's value and its face (0 L, 1 M: a glyph not
                                                         * in L, 2 M: L too wide, 3 S) */
#endif
#define LG_MY 4                                         /* the K pill: rows 4..21 */
#define LG_MH 18
#define LG_LY 24                                        /* the label's line: M capitals rows 28..38 */
#define LG_VB 49                                        /* the value's band: L capitals rows 49..69 */
#define LG_UY 73                                        /* the unit's line (S capitals rows 76..84) */
#define LG_GY 92                                        /* the gauge, 3 px */
static void draw_column_tall(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc, int32_t ratio,
                             uint32_t icon, int hot, int mot, int32_t kid)
{
    uint16_t lc = hot ? T_ACCENT : mot ? T_THEME : T_MID;
    const aafont_t *lf = &AF_M, *vf = &AF_L;
    int32_t ly = LG_LY, vy, lw = 0, mw;
    char k[3] = {'K', (char)('1' + c), 0};
    uint32_t ic = mot ? ICON_X_MOTION : icon, isz = 16u;
    cv_begin(COL_W, LG_CARD_H, T_BG);
    cv_rrect(0, 0, COL_W, LG_CARD_H, 4, T_SURF, T_BG);
    {   /* the knob: "K1" .. "K4" on a pill */
        uint16_t fill = hot ? T_ACCENT : label[0] || val[0] ? T_KEY : T_DIM;
        mw = ink_w(&AF_M, k) + 10;
        cv_rrect(5, LG_MY, mw, LG_MH, 5, fill, T_SURF);
        GFX_HOOK_ALIGN(5, LG_MY, 5 + mw, LG_MY + LG_MH, AL_HV, "large card knob pill");
        cv_text_in(5, LG_MY + CAP_IN(M, LG_MH), mw, &AF_M, k, T_INK, fill);
    }
    if (!label[0] && !val[0]) {                         /* a knob that does nothing on this page */
        cv_blit((uint32_t)CARD_X(c), Y_LABEL);
        return;
    }
    if (FELUCCA_ICONS && ic != ICON_NONE && label[0] && (ic - ICON_TRK < NTRK || icon_cell(&isz, ic) >= 0)) {
        GFX_HOOK_ALIGN(0, LG_MY, 0, LG_MY + LG_MH, AL_V, "large card icon on its pill's line");
        cv_icon_in(COL_W - 5 - 16, LG_MY, 16, LG_MH, 16, ic, mot ? (hot ? T_ACCENT : T_THEME) : lc, T_SURF);
    }
    if (label[0]) {
        if ((lw = ink_w(lf, label)) > COL_W - 6) {      /* M too wide: S, on the same capitals' middle */
            lf = &AF_S;
            ly = LG_LY + AF_M_CAP_Y + HALF_UP(AF_M_CAP_H - AF_S_CAP_H) - AF_S_CAP_Y;
            lw = ink_w(lf, label);
        }
        if (hot && T_ACCENT == T_MID) {                 /* MONO: the label inverted, the chip 2 px round its ink */
            int32_t cy = ly + (lf == &AF_M ? AF_M_CAP_Y : AF_S_CAP_Y), chh = lf == &AF_M ? AF_M_CAP_H : AF_S_CAP_H;
            int32_t x0 = HALF_UP(COL_W - lw) - 2;
            cv_rrect(x0, cy - 3, lw + 4, chh + 6, 3, T_ACCENT, T_SURF);
            GFX_HOOK_ALIGN(x0, cy - 3, x0 + lw + 4, cy + chh + 3, AL_HV, "large card label chip (MONO hot)");
            cv_text_in(0, ly, COL_W, lf, label, T_INK, T_ACCENT);
        } else {
            GFX_HOOK_ALIGN(0, 0, COL_W, 0, AL_H, "large card label centred");
            cv_text_in(0, ly, COL_W, lf, label, lc, T_SURF);
        }
    }
    if (kid >= 0) {                                     /* "[OCT+]": the keycap, centred */
        GFX_HOOK_ALIGN(0, LG_VB, COL_W, LG_VB + AF_L_CAP_H, AL_HV, "large card keycap centred");
        cv_keycap(HALF_UP(COL_W - kc_w((uint32_t)kid)), LG_VB + HALF_UP(AF_L_CAP_H - KC_H), (uint32_t)kid,
                  vc == T_ACCENT || vc == T_DIM ? vc : T_KEY, T_KINK, T_SURF);
    } else if (val[0]) {
        int why = !large_face_has(val) ? 1 : ink_w(vf, val) > COL_W - 6 ? 2 : 0;
        if (why)
            vf = &AF_M;
        if (vf == &AF_M && ink_w(vf, val) > COL_W - 6)
            vf = &AF_S, why = 3;
        LARGE_HOOK(val, why);
        vy = vf == &AF_L ? LG_VB - AF_L_CAP_Y : vf == &AF_M ? LG_VB + HALF_UP(AF_L_CAP_H - AF_M_CAP_H) - AF_M_CAP_Y
                                                          : LG_VB + HALF_UP(AF_L_CAP_H - AF_S_CAP_H) - AF_S_CAP_Y;
        if (vf == &AF_S && ink_w(vf, val) > COL_W - 4) {   /* (a value always fits: the lint reports one cut) */
            cv_text_fit(2, vy, vf, val, vt(vc, T_SURF), T_SURF, COL_W - 4);
        } else {
            GFX_HOOK_ALIGN(0, LG_VB, COL_W, LG_VB + AF_L_CAP_H, AL_HV, "large card value centred");
            cv_text_in(0, vy, COL_W, vf, val, vt(vc, T_SURF), T_SURF);
        }
    }
    if (unit[0]) {
        GFX_HOOK_ALIGN(0, 0, COL_W, 0, AL_H, "large card unit centred");
        cv_text_in(0, LG_UY, COL_W, &AF_S, unit, T_MID, T_SURF);
    }
    if (ratio >= 0) {
        int32_t gw = COL_W - 10, fx = ratio * gw / 1000;
        cv_rrect(5, LG_GY, gw, 3, 1, ux.style ? T_LINE : T_BG, T_SURF);
        cv_rrect(5, LG_GY, fx < 3 ? 3 : fx, 3, 1, hot ? T_ACCENT : vc == T_DIM ? T_DIM : T_THEME, T_BG);
    }
    cv_blit((uint32_t)CARD_X(c), Y_LABEL);
}

/* one card = one knob: [icon] LABEL / value unit / gauge on a SURF card, redrawn only when it changed.
 * The value is M, or S when M is too wide (engine names, long ENUMs); the unit S after it. The gauge is
 * a 3 px rounded bar: BG track, THEME fill. The knob just turned (hot): icon, label, value and gauge in
 * the accent. ratio: 0..1000 for the gauge, -1 = no gauge. icon: ICON_* (icons.c), ICON_AUTO = by label;
 * a label too long to share the card with its icon goes without it. A parameter MOTION drives (card_mot_next):
 * the motion icon in place of its own, in the theme colour; a long label moves it left, up to the card's edge;
 * a label with no room for it at all (or FELUCCA_ICONS=0): the label in the theme colour.
 * vc: the value's colour (T_THEME, T_DIM inactive, T_ACCENT the knob just turned) */
static void draw_column(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc,
                        int32_t ratio, uint32_t icon)
{
    char key[48];
    int hot = c == ui.hot_col && ui.hot_t, named = fmt_named, mot = card_mot_next && label[0], strip, snap;
    uint32_t kind = large_kind();                       /* MENU > LARGE: tall (draw_column_tall), or the label in M */
    uint8_t sig;
    uint16_t lc = hot ? T_ACCENT : T_MID;
    int32_t x, lx = 5, uw, room = COL_W - 8;
    const aafont_t *vf = &AF_M;
    uint32_t n, kn;
    int32_t kid = kc_tag(val, &kn);
    int mk = card_mk_next;                              /* MORPH moves this card's value (card_vv) */
    fmt_named = 0;                                      /* (params.c: a name, for this card only) */
    card_mk_next = 0;
    card_mot_next = 0;                                  /* (the same: MOTION, this card only) */
    card_mot = (uint8_t)((card_mot & ~(1u << c)) | (uint32_t)mot << c);
    if (str_eq(unit, label))
        unit = "";                                      /* "BPM 124 BPM", "USB OFF USB": the label says it */
    if (icon == ICON_AUTO)
        icon = icon_for_label(label);
    str_cpy(key, label, 12);                            /* cache key: texts + colour + gauge */
    str_cpy(key + str_len(key), "|", 2);
    str_cpy(key + str_len(key), val, 14);
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
    key[n + 6] = (char)('0' + hot + 2 * mot + 4 * (int)kind + 16 * mk);
    key[n + 7] = 0;
    uw = unit[0] ? text_w(&AF_S, unit) + 3 : 0;
    if (text_w(vf, val) + uw > room) {
        vf = &AF_S;
        CARD_HOOK(val, unit);                           /* (host: the values set in S) */
    }
    sig = (uint8_t)str_hash(str_hash(song.sel + TSEL->eng_req * 4u + ux.gen * 64u, label), unit);
    snap = ui.force || sig != ui.roll[c].sig;           /* what the value is of: label, unit, track, engine, palette */
    if (kind == LK_TALL)
        ui.roll[c].from[0] = 0;                         /* (a tall card: no rolling digits) */
    strip = !snap && str_eq(key, ui.col[c]);
    if (strip && !ui.roll[c].from[0])
        return;
    if (strip) {                                        /* rolling: the value strip only */
        cv_begin(COL_W, ROLL_H, T_SURF);
        cv_oy = -ROLL_Y;
    } else {
        char ov[16];                                    /* the value drawn before (in the cache key) */
        const char *k = ui.col[c];
        uint32_t i = 0;
        while (*k && *k != '|')
            k++;
        while (*k && k[1] && k[1] != '|' && i + 1u < sizeof ov)
            ov[i++] = *++k;
        ov[i] = 0;
        ui.roll[c].sig = sig;
        if (!str_eq(ov, val))
            roll_note(c, ov, val, snap || named || vf != &AF_M || kid >= 0 || kind == LK_TALL);
        else if (snap)
            ui.roll[c].from[0] = 0;
        str_cpy(ui.col[c], key, sizeof ui.col[c]);
        if (kind == LK_TALL) {
            draw_column_tall(c, label, val, unit, vc, ratio, icon, hot, mot, kid);
            return;
        }
        cv_begin(COL_W, COL_H, T_BG);
        cv_rrect(0, 0, COL_W, COL_H, 4, T_SURF, T_BG);
    }
    if (label[0] || val[0]) {
        if (!strip && label[0]) {
            /* MENU > LARGE on a list / graph page or a layer (LK_LABEL): the label in M where it fits the card (its
             * capitals rows 5..15, the icon on them when it fits too), else in S as without LARGE */
            const aafont_t *lf = kind == LK_LABEL && 5 + text_w(&AF_M, label) <= COL_W - 2 ? &AF_M : &AF_S;
            int32_t lw = text_w(lf, label), ix = 5, ly = lf == &AF_M ? 1 : 3;
            int32_t cy = ly + (lf == &AF_M ? AF_M_CAP_Y : AF_S_CAP_Y), chh = lf == &AF_M ? AF_M_CAP_H : AF_S_CAP_H;
            uint32_t ic = mot ? ICON_X_MOTION : icon;
            if (mot && ix + ICON_CELL + ICON_GAP + lw > COL_W - 2)
                ix = COL_W - 2 - lw - ICON_GAP - ICON_CELL;   /* a long label: MOTION's icon moves left */
            if (FELUCCA_ICONS && ic != ICON_NONE && ix >= 1 && ix + ICON_CELL + ICON_GAP + lw <= COL_W - 2) {
                GFX_HOOK_ALIGN(ix, cy, ix, cy + chh, AL_V, "card icon on its label's line");
                lx = ix + cv_icon_in(ix, cy, 0, chh, ICON_CELL, ic,   /* its ink on the label's line */
                                     mot ? (hot ? T_ACCENT : T_THEME) : lc, T_SURF) + ICON_GAP;
            }
            else if (mot)                               /* the label from x 19 (5 + 12 + 2) */
                lc = hot ? T_ACCENT : T_THEME;          /* no room for MOTION's icon: the label says it */
            if (hot && T_ACCENT == T_MID) {             /* MONO: no colour tells the hot knob: its label inverted, */
                int32_t b[4], w;                        /* the chip 2 px round its ink (M: 3 above, 2 below) */
                text_ink(lf, label, b);
                w = b[2] - b[0] + 4;
                if (w > COL_W + 1 - lx - b[0]) w = COL_W + 1 - lx - b[0];
                cv_rrect(lx + b[0] - 2, cy - 3, w, lf == &AF_M ? chh + 5 : 15, 3, T_ACCENT, T_SURF);
                GFX_HOOK_ALIGN(lx + b[0] - 2, cy - 3, lx + b[0] - 2 + w, lf == &AF_M ? cy + chh + 2 : 18, AL_HV,
                               "card label chip (MONO hot)");
                cv_text_fit(lx, ly, lf, label, T_INK, T_ACCENT, COL_W - 2 - lx);
            } else
                cv_text_fit(lx, ly, lf, label, lc, T_SURF, COL_W - 2 - lx);
        }
        if (kid >= 0)                                   /* "[OCT+]": the keycap (accent: it would act; DIM: it would not) */
            x = cv_keycap(5, 20, (uint32_t)kid, vc == T_ACCENT || vc == T_DIM ? vc : T_KEY, T_KINK, T_SURF);
        else if (vf == &AF_M) {
            if (!ui.roll[c].from[0] && val[0])
                GFX_HOOK_ALIGN(0, ROLL_Y, COL_W, ROLL_Y + ROLL_H, AL_V, "card value in its roll window");
            x = roll_text(c, 5, 17, val, vt(vc, T_SURF));
        } else
            x = cv_text_fit(5, 20, vf, val, vt(vc, T_SURF), T_SURF, room - uw);
        if (unit[0]) {
            GFX_HOOK_ALIGN(0, 0, 0, vf == &AF_M ? 17 + AF_M.asc : 20 + AF_S.asc, AL_B, "card unit on its value's baseline");
            cv_text_on(x + 3, 20, &AF_S, unit, T_MID, T_SURF);     /* on the value's baseline (S) */
        }
        if (!strip && ratio >= 0) {
            int32_t gw = COL_W - 10, fx = ratio * gw / 1000;
            cv_rrect(5, 38, gw, 3, 1, ux.style ? T_LINE : T_BG, T_SURF);   /* (LINE: no card to cut it from) */
            cv_rrect(5, 38, fx < 3 ? 3 : fx, 3, 1, hot ? T_ACCENT : vc == T_DIM ? T_DIM : T_THEME, T_BG);
        }
        if (!strip && mk)                               /* MORPH moves it: a 7 x 2 tick in the accent under the gauge */
            cv_rrect(5, 42, 7, 2, 1, T_ACCENT, T_SURF);
    }
    cv_oy = 0;
    cv_blit((uint32_t)CARD_X(c), Y_LABEL + (strip ? ROLL_Y : 0));
}

/* an action's column (act_cols): picked, the OCT+ keycap (accent while it would do something); else "--" */
static void draw_act_column(uint32_t c, const char *label, uint16_t vc, uint32_t icon)   /* icon: ICON_AUTO = by label */
{
    int picked = act_col() == c + 1u;
    draw_column(c, label, picked ? "[OCT+]" : "--", "", picked ? (act_ready() ? T_ACCENT : T_DIM) : vc, -1, icon);
}

/* the sound's name on the track: a user preset or the engine's preset (b holds 16) */
static void sound_name(const track_t *t, char *b)
{
    b[0] = 0;
    if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), b);
    else
        str_cpy(b, ds_name(t->eng_req % NENGINES, t->preset), 16);   /* (DUELL: sounds.c; INIT for none) */
}

/* THE footer (every page, every layer, 202..240 = H_FOOT; the lint's rows: 2 and 19):
 *   row 1  y 2    the sequence's 16 step bars (the cursor's bank); the GRID's page and ACCENT; MIDI LEARN's line; a
 *                 HELP hint for a moment (HELP ON). A page never puts a keycap strip here (SPEC 1.1a: SELECT selects,
 *                 the knobs adjust; the footer says which page it is, not which button goes where)
 *   row 2  y 19   the engine's icon and name at x 8, the sound after it; at the right, ending at x 232: the page's
 *                 icon and name, a thin vertical LINE rule (FT_GAP either side) and the page's number "6/7". A page
 *                 with no number (alone in its family, a layer's "SET") ends its name at 232
 * A layer shows the same footer, its button's name and kind ("GLO | SET") where the page's are */
#define FT_R 232                                       /* the right margin (the left one is 8) */
#define FT_GAP 5                                       /* the page name -> the separator -> the number */
static void draw_foot(void)
{
    char s[48], pn[16], ti[12], ct[8], ln[24];
    const track_t *t = TSEL;
    uint32_t sig;
    const page_t *pg = cur_page();
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    const char *ename = e->name, *help;
    int32_t x;
    sound_name(t, pn);
    if (ui.layer)                                      /* a layer's map: its button and kind in the page's place */
        layer_foot_title(ti, ct);
    else
        page_parts(ti, ct);                            /* "VOICE 2" and "6/7" (OVERVIEW 5/5 on HOME) */
    str_cpy(s, ename, sizeof s);
    s[str_len(s) + 1u] = 0;
    s[str_len(s)] = (char)('1' + song.sel);
    str_cpy(s + str_len(s), pn, 16);
    str_cpy(s + str_len(s), ti, sizeof ti);
    str_cpy(s + str_len(s), ct, sizeof ct);
    {   /* step markers: the playhead only when it is in the shown bank, the cursor only in SEQ */
        uint32_t ph = song.playing && t->seq_idx / 16u == ui.bank ? t->seq_idx : 0xFFu;
        sig = str_hash(0x9E3779B9u, s) + ph * 97u + (song.seq_mode ? ui.cursor : 0xFFu) * 3001u + steps_hash(t) +
              (ui.home || ui.layer ? 0u : page_icon(pg)) * 7121u + (uint32_t)ui.layer * 4177u +
              ui.bank * 7u + (uint32_t)t->p[P_SLEN] * 13u;
    }
    if (grid_on())
        sig += 0x51EDu + (uint32_t)black_held(GK_ACC) * 977u;
    help = ui.ml.on ? 0 : ui.layer ? (help_on() ? HELP_LAYER[ui.layer % LAYER_N] : 0)   /* 1.5, HELP ON: a layer's hint stays while it
                                                       * shows */
                    : ui.help_t ? help_page() : 0;      /* a page's for ~2 s after it was entered (neither while MIDI
                                                       * LEARN shows its line) */
    if (help)
        sig = str_hash(sig + 0x4E1Bu, help);
    if (ui.ml.on) {                                    /* MIDI LEARN (midi_learn.c): what is picked, its CC */
        ml_line(ln);
        sig += str_hash(0x1EA2u, ln);
    }
    if (!ui.force && sig == ui.foot_sig)
        return;
    ui.foot_sig = sig;
    cv_begin(240, H_FOOT, T_BG);
    if (help) {                                       /* row 1: the hint in place of the steps */
        help_row(help, 2);
    } else if (ui.ml.on) {                               /* row 1: MIDI LEARN's line, OCT+ DONE at the right (the one way out) */
        khint_t kh = {KC_OCTUP, "DONE"};
        int32_t kw = kh_w(KC_OCTUP, "DONE");
        cv_free_text(8, 2, &AF_S, ln, T_ACCENT, T_BG, 232 - kw - 8 - 8);
        cv_key_row(232 - kw, 232, 2, &kh, 1, 1u, T_BG);
    } else if (grid_on() && !ui.layer) {              /* row 1: the page (and ACCENT while the black key is held) */
        char b[16];
        uint32_t len = (uint32_t)t->p[P_SLEN];
        str_cpy(b, "PAGE ", sizeof b);
        fmt_int(b + 5, (int32_t)ui.bank + 1);
        str_cpy(b + str_len(b), "/", 4);
        fmt_int(b + str_len(b), (int32_t)((len + 15u) / 16u));
        cv_text(8, 2, &AF_S, b, vt(T_THEME, T_BG));
        if (black_held(GK_ACC))
            cv_text_r(232, 2, &AF_S, "ACCENT", T_ACCENT, T_BG);
    } else {
        uint32_t i;
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
                cv_rrect(sx, 9, 9, 3, 1, st->time == ST_TIE ? T_MID : T_RAISE, T_BG);
            if (song.seq_mode && si == ui.cursor)
                cv_rect(sx, 14, 9, 2, T_ACCENT);        /* the step edited */
            else if (song.playing && si == t->seq_idx)
                cv_rect(sx, 14, 9, 2, T_TEXT);          /* the step sounding */
        }
    }
    {   /* row 2: engine icon + name, sound, then at the right the page's icon and name | its number (MIXER, PATTERNS,
         * SONG, AUTOMATION); icons: their ink on the names' capitals */
        uint32_t pi = ui.home || ui.layer ? ICON_NONE : page_icon(pg), ei = FELUCCA_ICONS ? engine_icon(ename) : ICON_NONE;
        int32_t r = FT_R, tx, nr, ex, need = text_w(&AF_S, pn), ew = text_w(&AF_S, ename), cw = ct[0] ? text_w(&AF_S, ct) : 0;
        ex = 8 + (FELUCCA_ICONS ? 12 + 5 : 0) + (ew < 80 ? ew : 80);   /* (where the engine's name ends) */
        nr = ct[0] ? r - cw - FT_GAP - 1 - FT_GAP : r;  /* the name ends here: number, gap, 1 px rule, gap */
        tx = nr - text_w(&AF_S, ti) - (pi != ICON_NONE ? 16 : 0);
        if (!ui.home && !ui.layer && pg->graph == GR_EVENTS && tx - 8 - (ex + 10) < need && str_eq(ti, "AUTOMATION")) {
            str_cpy(ti, "AUTO", sizeof ti);             /* #93: "AUTOMATION" -> "AUTO" where the sound's name */
            tx = nr - text_w(&AF_S, ti) - (pi != ICON_NONE ? 16 : 0);   /* would be cut (MENU > LARGE) */
        }
        x = 8;
        if (FELUCCA_ICONS) {
            if (ei != ICON_NONE)
                GFX_HOOK_ALIGN(0, 19 + AF_S_CAP_Y, 0, 19 + AF_S_CAP_Y + AF_S_CAP_H, AL_V,
                               "footer icons on the names' line");
            x += cv_icon_in(x, 19 + AF_S_CAP_Y, 0, AF_S_CAP_H, 12, ei, T_MID, T_BG) + 5;
        }
        x = cv_text_fit(x, 19, &AF_S, ename, vt(T_THEME, T_BG), T_BG, 80);
        cv_free_text(x + 10, 19, &AF_S, pn, T_TEXT, T_BG, tx - 8 - (x + 10));
        if (pi != ICON_NONE) {
            GFX_HOOK_ALIGN(0, 19 + AF_S_CAP_Y, 0, 19 + AF_S_CAP_Y + AF_S_CAP_H, AL_V,
                           "footer icons on the names' line");
            cv_icon_in(tx, 19 + AF_S_CAP_Y, 0, AF_S_CAP_H, 12, pi, T_MID, T_BG);
        }
        cv_text_r(nr, 19, &AF_S, ti, T_MID, T_BG);
        if (ct[0]) {                                  /* | and the number: the rule on the capitals' band, 1 px over */
            cv_rect(r - cw - FT_GAP - 1, 19 + AF_S_CAP_Y - 1, 1, AF_S_CAP_H + 2, T_LINE);
            cv_text_r(r, 19, &AF_S, ct, T_MID, T_BG);
        }
    }
    cv_blit(0, Y_FOOT);
}
/* the EDIT layer's cards (ui_layer.c): ENG (the engine), No. (its sounds: KNOB 2's list), FAV, an empty card */
static void engine_columns(int init)
{
    uint32_t total, cur = eng_list_pos(&total), e = TSEL->eng_req % NENGINES;
    char val[8], u[8];
    fmt_int(val, (int32_t)cur + 1);
    str_cpy(u, "/", 8);
    fmt_int(u + 1, (int32_t)total);
    draw_column(0, init ? "ENGINE" : "ENG", ENGINES[e]->name, "", VAL(0u), (int32_t)eng_rank(e) * 1000 / (NENG_SHOWN > 1 ? NENG_SHOWN - 1 : 1),
                engine_icon(ENGINES[e]->name));
    draw_column(1, init ? "SOUND" : "No.", val, u, VAL(1u), total > 1u ? (int32_t)(cur * 1000u / (total - 1u)) : 0, ICON_NONE);
    draw_column(2, "FAV", preset_favorite() ? "ON" : "OFF", "", VAL(2u), -1, ICON_X_STAR);
    if (init)                                           /* the ENGINE page (SPEC 1.2): KNOB 4 INIT, a turn right asks */
        draw_column(3, "INIT", "--", "", VAL(3u), -1, ICON_AUTO);
    else
        draw_column(3, "", "", "", T_THEME, -1, ICON_NONE);
}
static void draw_columns(void)
{
    uint32_t c;
    char val[12];
    const char *unit;
    (void)motion_mask(song.sel, card_mot_mask);       /* (PLAY OFF: no scan) */
    if (ui.home) {
        for (c = 0; c < 4u; c++) {
            int16_t *vp;
            const param_desc_t *d = home_param(c, &vp);
            param_format(d, *vp, val, &unit);
            card_mot_of(vp);
            if (home_levels())                         /* 1.5: T1..T4 LEVEL, the track's cushion, muted: DIM */
                draw_column(c, "LEVEL", val, unit, trk[c].p[P_MUTE] ? T_DIM : VAL(c), RATIO(d, *vp), trk_icon(c, 1));
            else
                draw_column(c, d->label, val, unit, VAL(c), RATIO(d, enum_rank(d, *vp)), param_icon(d, *vp));
        }
        return;
    }
    if (cur_page()->graph == GR_EVENTS) {              /* AUTOMATION (ui_events.c): ROW STEP PARAM VALUE */
        const track_t *t = TSEL;
        uint16_t rw[EV_ROWS];
        uint32_t n, c = ev_cur(rw, &n), r = ev_row(n), k = c >> 8, step, id;
        int16_t v;
        char nm[12];
        const param_desc_t *d;
        if (k == EVK_TOP) {                            /* PLAY: the counts, PLAY ON / OFF (OCT+: CLEAR) */
            draw_column(0, "ROW", "ALL", "", VAL(0u), -1, ICON_AUTO);
            fmt_int(val, (int32_t)(motion_count(t) - motion_lock_count(t)));
            draw_column(1, "EVENT", val, "", T_MID, -1, ICON_NONE);
            fmt_int(val, (int32_t)motion_lock_count(t));   /* parameter locks (SEQ > STEP: a step held, a knob) */
            draw_column(2, "LOCK", val, "", T_MID, -1, ICON_NONE);
            draw_column(3, "PLAY", motion_enabled(t) ? "ON" : "OFF", "", VAL(3u), -1, motion_icon());
            return;
        }
        if (k == EVK_QNT) {                            /* QUANTIZE (1.2): the steps with a NUDGE, QNTZ ON / OFF */
            draw_column(0, "ROW", "ALL", "", VAL(0u), -1, ICON_AUTO);
            fmt_int(val, (int32_t)ev_nudged(t));
            draw_column(1, "NUDGED", val, "", T_MID, -1, ICON_NONE);
            draw_column(2, "", "", "", T_THEME, -1, ICON_NONE);
            draw_column(3, "QNTZ", t->p[P_SQNT] ? "ON" : "OFF", "", VAL(3u), -1, ICON_QUANTIZE);
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
        id = k == EVK_REC ? MOTION_ID(&motion.event[c & 0xFFu]) : k == EVK_ADD ? ui.ev_id :
             k == EVK_CHANCE ? EV_CHANCE : k == EVK_RATCH ? EV_RATCH : k == EVK_VEL ? EV_VEL : EV_NUDGE;
        draw_column(0, "ROW", val, "", VAL(0u), -1, ICON_AUTO);
        fmt_int(val, (int32_t)step + 1);
        draw_column(1, "STEP", val, "", k != EVK_ADD || step < (uint32_t)t->p[P_SLEN] ? VAL(1u) : T_DIM, -1, ICON_AUTO);
        if (!ev_id_ok(t, id)) {
            draw_column(2, "PARAM", "--", "", T_DIM, -1, ICON_AUTO);
            draw_column(3, "VALUE", "--", "", T_DIM, -1, ICON_AUTO);
            return;
        }
        ev_name(t, id, nm);
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
                draw_column(3, "ROWS", ui.ev_vel ? "SHOWN" : "HIDDEN", "", T_DIM, -1, ICON_AUTO);
                return;
            }
            fmt_int(val, sv);
            draw_column(3, "VALUE", val, "", ev_vel_plays(cs) ? VAL(3u) : T_DIM, sv * 1000 / 127, ICON_LEVEL);
            return;
        }
        if (id == EV_CHANCE || id == EV_RATCH) {       /* a step's CHANCE (the die) / RATCH; + ADD: what it adds */
            const step_t *cs = &t->step[step % NSTEP];
            uint32_t sk = ev_skind(id), sv = (uint32_t)(k == EVK_ADD ? ev_sadd(sk) : ev_sval(cs, sk));
            int plays = sk == EVK_CHANCE || (cs->time == ST_NOTE && (cs->n || cs->hit));   /* (RATCH does nothing on a
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
        d = track_desc(t, id);
        param_format(d, v, val, &unit);
        draw_column(3, k == EVK_ADD ? "VALUE" : (motion.event[c & 0xFFu].param & MOTION_LOCK) ? "LOCK" : "AUTO", val, unit,
                    k != EVK_ADD ? VAL(3u) : T_DIM, RATIO(d, enum_rank(d, v)), param_icon(d, v));
        return;
    }
    if (cur_page()->graph == GR_TOOLS) {
        draw_act_column(0, "CLEAR", VAL(0u), ICON_AUTO);   /* CLEAR PATTERN, INIT SOUND (the panel has the full names; */
        draw_act_column(1, "INIT", VAL(1u), ICON_AUTO);    /* no song mode: no SECTION, SONG) */
        draw_column(2, "", "", "", T_THEME, -1, ICON_NONE);
        draw_column(3, "", "", "", T_THEME, -1, ICON_NONE);
        return;
    }
    if (cur_page()->scope == SC_MORPH) {               /* MORPH (fader, ROLL, UNDO, AMOUNT: v3), MORPH SET (TARGET SMART ARP PITCH) */
        uint32_t pi = song.sel, f = morph_fader_of(pi);
        int32_t g;
        if (cur_page()->graph == GR_MORPHSET) {
            draw_column(0, "TARGET", morph.target == MT_BOTH ? "BOTH" : pi ? "PART 2" : "PART 1", "", VAL(0u), -1, ICON_AUTO);
            draw_column(1, "SMART", morph.cfg & MC_WILD ? "OFF" : "ON", "", VAL(1u), -1, ICON_NONE);
            draw_column(2, "ARP", morph.cfg & MC_ARP ? "ON" : "OFF", "", VAL(2u), -1, ICON_NONE);
            draw_column(3, "PITCH", morph.cfg & MC_PIT ? "ON" : "OFF", "", VAL(3u), -1, ICON_NONE);
            return;
        }
        if (f == 0u || f == 127u)                      /* the fader: its ends by their letters, between them 1..126 */
            str_cpy(val, f ? "B" : "A", sizeof val);
        else
            fmt_int(val, (int32_t)f);
        draw_column(0, "MORPH", val, "", VAL(0u), (int32_t)f * 1000 / 127, ICON_MIX);
        g = morph_pending(0);                          /* ROLL: A left, B right; a twist building up: its way */
        draw_column(1, "ROLL", g < 0 ? "<A" : g > 0 ? "B>" : "A|B", "", g ? T_ACCENT : VAL(1u), -1, ICON_NONE);
        g = morph_pending(1);                          /* UNDO: A left, B right, like ROLL; a twist building up: its way */
        draw_column(2, "UNDO", g < 0 ? "<A" : g > 0 ? "B>" : "A|B", "", g ? T_ACCENT : VAL(2u), -1, ICON_NONE);
        fmt_int(val, (int32_t)morph_amount_of(pi));
        draw_column(3, "AMOUNT", val, "%", VAL(3u), (int32_t)morph_amount_of(pi) * 10, ICON_PROB);
        return;
    }
    if (cur_page()->scope == SC_TEST) {                /* GLO > TEST: MORPH and ROLL as the MORPH page's, LOAD MULTI, RESET */
        uint32_t pi = song.sel, f = morph_fader_of(pi), mk = test_slot();
        int32_t g;
        if (f == 0u || f == 127u)
            str_cpy(val, f ? "B" : "A", sizeof val);
        else
            fmt_int(val, (int32_t)f);
        draw_column(0, "MORPH", val, "", VAL(0u), (int32_t)f * 1000 / 127, ICON_MIX);
        g = morph_pending(0);
        draw_column(1, "ROLL", g < 0 ? "<A" : g > 0 ? "B>" : "A|B", "", g ? T_ACCENT : VAL(1u), -1, ICON_NONE);
        if (mk < MU_SLOTS)
            mu_label(val, mk);
        else
            str_cpy(val, "--", sizeof val);
        draw_column(2, "LOAD", val, "", mk < MU_SLOTS ? VAL(2u) : T_DIM, -1, ICON_AUTO);
        draw_column(3, "RESET", "--", "", VAL(3u), -1, ICON_AUTO);
        return;
    }
    if (cur_page()->scope == SC_MIDI) {                /* GLO's MIDI pages: MIDI (P1 CH, P2 CH, MIDI IN, CLK), SIDECHAIN (SC SRC, SC NOTE), MIDI LEARN (LEARN, CLEAR) */
        const track_t *t = TSEL;
        if (cur_page()->graph == GR_MLEARN) {
            char n[10] = "NONE";
            draw_column(0, "LEARN", ui.ml.on ? "ON" : "OFF", "", ui.ml.on ? T_ACCENT : VAL(0u), -1, ICON_MIDI);
            if (ml_count()) {                          /* CLEAR: right clears every learned CC; the card says how many there are */
                fmt_int(n, (int32_t)ml_count());
                str_cpy(n + str_len(n), " CC", 4);
            }
            draw_column(1, "CLEAR", n, "", ml_count() ? VAL(1u) : T_DIM, -1, ICON_MIDI);
            for (c = 2; c < 4u; c++)
                draw_column(c, "", "", "", T_THEME, -1, ICON_NONE);
            return;
        }
        if (cur_page()->graph == GR_SCHAIN) {          /* the selected part's DUCK SRC and NOTE: the same values as its DUCK 1 page */
            uint32_t src = (uint32_t)t->p[P_DKSRC];
            param_format(&TP[P_DKSRC], t->p[P_DKSRC], val, &unit);
            draw_column(0, "SC SRC", val, unit, VAL(0u), -1, ICON_MIDI);
            param_format(&TP[P_DKNOTE], t->p[P_DKNOTE], val, &unit);
            draw_column(1, "SC NOTE", val, unit, src >= 2u ? VAL(1u) : T_DIM, -1, ICON_MIDI);
            for (c = 2; c < 4u; c++)
                draw_column(c, "", "", "", T_THEME, -1, ICON_NONE);
            return;
        }
        {
            uint32_t sel = song.g[G_ROUTE] == 1, lay = duell_mode_req == DM_LAYER, k;
            for (k = 0; k < 2u; k++) {                 /* the parts' channels (dim: not heard when SELECTED; part 2 in LAYER) */
                fmt_int(val, (int32_t)midi_ch_get(k) + 1);
                draw_column(k, k ? "P2 CH" : "P1 CH", val, "", sel || (lay && k) ? T_DIM : VAL(k), (int32_t)midi_ch_get(k) * 1000 / 15, ICON_MIDI);
            }
            draw_column(2, "MIDI IN", sel ? "SELECT" : "OWN CH", "", VAL(2u), -1, ICON_MIDI);
            draw_column(3, "CLK", GP[G_CLOCK].names[song.g[G_CLOCK] % 3], "", VAL(3u), -1, ICON_TEMPO);   /* the clock source INT / USB / TRS */
        }
        return;
    }
    if (cur_page()->scope == SC_MODE) {                /* step 5: MODE (SPLIT / STEREO / LAYER); the rest nothing */
        uint32_t m = duell_mode_req < DM_N ? duell_mode_req : DM_SPLIT;
        draw_column(0, "MODE", DM_NAME[m], "", VAL(0u), (int32_t)m * 500, ICON_MIX);
        fmt_int(val, mix_bal < 0 ? -mix_bal : mix_bal);   /* BALANCE: L100 .. 0 .. R100 (left = part 2 fades), XFADE %, SPLIT a note */
        if (mix_bal) {
            char t2[8];
            str_cpy(t2, val, sizeof t2);
            str_cpy(val, mix_bal < 0 ? "-" : "+", 4);
            str_cpy(val + 1, t2, 6);
        }
        draw_column(1, "BAL", val, "", VAL(1u), ((int32_t)mix_bal + 100) * 5, ICON_MIX);
        fmt_int(val, mix_xf);
        draw_column(2, "XFADE", val, "%", duell_mode_req == DM_LAYER ? VAL(2u) : T_DIM, (int32_t)mix_xf * 10, ICON_MIX);
        if (mix_split)
            note_name(val, (uint32_t)mix_split - 1u);
        else
            str_cpy(val, "OFF", sizeof val);
        draw_column(3, "SPLIT", val, "", mix_split && duell_mode_req != DM_LAYER ? T_DIM : VAL(3u), mix_split ? ((int32_t)mix_split - 1) * 1000 / 127 : -1, ICON_MIX);
        return;
    }
    if (cur_page()->scope == SC_TRK) {                 /* LEVEL PAN REV MUTE of the selected track */
        const track_t *t = TSEL;
        uint32_t lvl = trk_level(song.sel);
        param_format(&TP[P_LEVEL], (int32_t)lvl, val, &unit);   /* (0: OFF) */
        card_mot_of(&TSEL->p[P_LEVEL]);
        draw_column(0, "LEVEL", val, unit, lvl && !t->p[P_MUTE] ? VAL(0u) : T_DIM, (int32_t)lvl * 1000 / 127, ICON_AUTO);
        if (duell_mode_req == DM_SPLIT) {              /* step 5: SPLIT, part 1 left, part 2 right: fixed (DIM) */
            int32_t sp = song.sel ? 63 : -64;
            card_mot_of(&TSEL->p[P_PAN]);              /* (its automation is kept for STEREO / LAYER: still marked) */
            draw_column(1, "PAN", song.sel ? "RIGHT" : "LEFT", "", T_DIM, RATIO(&TP[P_PAN], sp), param_icon(&TP[P_PAN], sp));
        } else {
            param_format(&TP[P_PAN], t->p[P_PAN], val, &unit);
            card_mot_of(&TSEL->p[P_PAN]);
            draw_column(1, "PAN", val, unit, VAL(1u), RATIO(&TP[P_PAN], t->p[P_PAN]), param_icon(&TP[P_PAN], t->p[P_PAN]));
        }
        param_format(&TP[P_REV], t->p[P_REV], val, &unit);
        card_mot_of(&TSEL->p[P_REV]);
        draw_column(2, "REV", val, unit, VAL(2u), RATIO(&TP[P_REV], t->p[P_REV]), ICON_AUTO);
        draw_column(3, "MUTE", t->p[P_MUTE] ? "ON" : "OFF", "", t->p[P_MUTE] ? T_ACCENT : VAL(3u), -1, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_ENGINE) {              /* ENGINE SOUND FAV INIT (the EDIT layer's three, and INIT) */
        engine_columns(1);
        return;
    }
    if (cur_page()->graph == GR_BROWSE) {              /* PATCHES: PATCH (the place / the places), TYPE, FAV, ERASE (a turn right asks) */
        uint32_t total, cur = pl_cursor(&total);
        char u[8], fr[12];
        if (cur < total) fmt_int(val, (int32_t)cur + 1);
        else str_cpy(val, "--", 8);
        str_cpy(u, "/", 8);
        fmt_int(u + 1, (int32_t)total);
        str_cpy(fr, "FREE ", 12);                       /* the places a new patch may still take (upreset.c PT_MAX), under ERASE: it gives one back */
        fmt_int(fr + 5, (int32_t)up_free_places());
        draw_column(0, "PATCH", val, u, VAL(0u), -1, ICON_NONE);
        draw_column(1, "TYPE", list_name(list_mode()), "", VAL(1u), -1, ICON_X_FOLDER);
        draw_column(2, "FAV", ui.pe ? "--" : preset_favorite() ? "ON" : "OFF", "", VAL(2u), -1, ICON_X_STAR);
        draw_column(3, "ERASE", fr, "", VAL(3u), -1, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_PATS) {                  /* SLOT, then LOAD ERASE SAVE (GO buttons) */
        uint32_t n = pat_count(), k = pat_pick();
        char tag[4], nm[13];
        pat_label(k, tag, nm);
        draw_column(0, "SLOT", tag, "", VAL(0u), (int32_t)k * 1000 / (int32_t)(n > 1u ? n - 1u : 1u), ICON_X_PATTERN);
        draw_act_column(1, "LOAD", pat_has(k) ? T_THEME : T_DIM, ICON_AUTO);
        draw_act_column(2, "ERASE", ps_used(k) ? T_THEME : T_DIM, ICON_AUTO);   /* (a saved pattern only) */
        draw_act_column(3, "SAVE", T_THEME, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_SLOTS) {                 /* MULTI: SLOT M01..M16, then LOAD ERASE SAVE (GO buttons) */
        uint32_t k = (uint32_t)(song.g[G_SLOT] - 1) % MU_SLOTS;
        int used = project_used(k);
        mu_label(val, k);
        draw_column(0, "SLOT", val, "", VAL(0u), (int32_t)k * 1000 / (int32_t)(MU_SLOTS - 1u), ICON_AUTO);
        draw_act_column(1, "LOAD", used ? T_THEME : T_DIM, ICON_AUTO);
        draw_act_column(2, "ERASE", used ? T_THEME : T_DIM, ICON_AUTO);
        draw_act_column(3, "SAVE", T_THEME, ICON_AUTO);
        return;
    }
#if FELUCCA_SLICE
    if (cur_page()->graph == GR_SLICES) {                /* SLICE POS, then SPLIT JOIN (ui_slice.c) */
        uint32_t n = slice_count(), j = slice_sel(), src, div, ok = slice_src(&src, &div) && src;
        char u[8];
        if (j < n) {
            fmt_int(val, (int32_t)j + 1);
            str_cpy(u, "/", 8);
            fmt_int(u + 1, (int32_t)n);
        } else {
            str_cpy(val, n ? "END" : "--", sizeof val);
            u[0] = 0;
        }
        draw_column(0, "SLICE", val, u, VAL(0u), -1, ICON_SLICE);
        slice_time(val, slice_mark(j));
        draw_column(1, "POS", val, "S", ok ? VAL(1u) : T_DIM, -1, ICON_AUTO);
        draw_act_column(2, "SPLIT", ok ? T_THEME : T_DIM, ICON_AUTO);
        draw_act_column(3, "JOIN", ok ? T_THEME : T_DIM, ICON_AUTO);
        return;
    }
#endif
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
            draw_column(c, lb, val, u, on ? VAL(c) : T_DIM, r, ic);
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
        draw_column(1, "SRC", N_MSRC[clamp(s, 0, MS_N - 1)], "", s ? VAL(1u) : T_DIM, -1, mod_src_icon(s));
        card_mk_next = mkd;
        draw_column(2, "DST", mod_dst_name(t, d), "", d ? VAL(2u) : T_DIM, -1, mod_dst_icon(t, d));
        param_format(&TP[id + 2u], a, val, &unit);
        card_mk_next = mka;
        draw_column(3, "AMT", val, unit, a ? VAL(3u) : T_DIM, RATIO(&TP[id + 2u], a), mod_src_icon(MS_OFF));
        return;
    }
    if (cur_page()->graph == GR_ROLL && lock_held()) {   /* a step held: what KNOB 1..4 lock (ui_input.c lock_turn) */
        uint64_t held = lock_held();
        uint32_t st = 0;
        while (!((held >> st) & 1u))
            st++;                                       /* (the first step held: its values) */
        for (c = 0; c < 4u; c++) {
            uint32_t id = lock_id(c);
            const param_desc_t *d;
            int16_t v;
            int on;
            if (id == 0xFFu) {
                draw_column(c, "", "", "", T_THEME, -1, ICON_AUTO);
                continue;
            }
            d = track_desc(TSEL, id);
            on = motion_lock_get(TSEL, st, id, &v);
            if (!on)
                v = motion_base_value(TSEL, id);      /* not locked: the sound's own, DIM */
            param_format(d, v, val, &unit);
            draw_column(c, d->label, val, unit, on ? VAL(c) : T_DIM, RATIO(d, enum_rank(d, v)), param_icon(d, v));
        }
        return;
    }
    if (cur_page()->scope == SC_STEP && drum_track(TSEL)) {   /* the grid: STEP LANE HIT ACC */
        const step_t *st = &seq_steps(TSEL)[ui.cursor];
        uint32_t b = 1u << ui.lane, on = (step_lanes(st) & b) != 0u, ac = (step_accents(st) & b) != 0u;
        char sn[8], sl[8];
        fmt_int(sn, (int32_t)ui.cursor + 1);
        str_cpy(sl, "/", 8);
        fmt_int(sl + 1, TSEL->p[P_SLEN]);
        draw_column(0, "STEP", sn, sl, VAL(0u), -1, ICON_AUTO);
        draw_column(1, "LANE", drum_lane_name(TSEL, ui.lane), "", VAL(1u), -1, ICON_AUTO);
        draw_column(2, "HIT", on ? "ON" : "--", "", on ? VAL(2u) : T_DIM, -1, ICON_AUTO);
        draw_column(3, "ACC", ac ? "ON" : "--", "", ac ? VAL(3u) : T_DIM, -1, ICON_AUTO);
        return;
    }
    if (cur_page()->scope == SC_STEP) {
        static const char *const TIME_N[3] = {"NOTE", "TIE", "REST"};
        const step_t *st = &seq_steps(TSEL)[ui.cursor];
        char u[8];
        uint32_t cnt = st->n, first = st->note[0], l;
        for (l = NLANE; l-- > 0;)                         /* (lane hits count as their notes) */
            if ((st->hit >> l) & 1u) {
                cnt++;
                if (!st->n)
                    first = DRUM_LANE_NOTE[l];
            }
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
            static const char *const FLAG_N[4] = {"-", "ACC", "SLD", "A+S"};
            char sn[8], sl[8];
            fmt_int(sn, (int32_t)ui.cursor + 1);
            str_cpy(sl, "/", 8);
            fmt_int(sl + 1, TSEL->p[P_SLEN]);
            draw_column(0, "STEP", sn, sl, VAL(0u), -1, ICON_AUTO);
            draw_column(1, "NOTE", val, u, step_on(st) ? VAL(1u) : T_DIM, -1, ICON_AUTO);
            if (step_on(st)) {                            /* a note: its LEN (1.5, KNOB 3: the TIEs after it) */
                fmt_int(sn, (int32_t)note_len(TSEL, ui.cursor));
                draw_column(2, "LEN", sn, "", VAL(2u), -1, ICON_AUTO);
            } else {
                draw_column(2, "TIME", TIME_N[st->time % 3u], "", VAL(2u), -1, ICON_AUTO);
            }
            draw_column(3, "FLAG", FLAG_N[(st->flags & SF_ACCENT ? 1u : 0u) | (st->flags & SF_SLIDE ? 2u : 0u)], "",
                        VAL(3u), -1, ICON_AUTO);
        }
        return;
    }
    for (c = 0; c < 4u; c++) {
        int16_t *vp;
        int32_t vv;
        const param_desc_t *d = page_desc(cur_page(), c, &vp);
        if (cur_page()->graph == GR_TEMPO && c == 2u) { /* TAP: the tempo of the taps for 2 s after the last, else TAP (EXT clock: DIM) */
            uint32_t n = tempo_tapping();
            if (n >= 2u)
                fmt_int(val, song.g[G_BPM]);
            else
                str_cpy(val, song.g[G_CLOCK] ? "EXT" : n ? "1" : "TAP", sizeof val);
            draw_column(2, "TAP", val, n >= 2u ? "BPM" : "", song.g[G_CLOCK] ? T_DIM : n ? T_ACCENT : VAL(2u), -1, ICON_TEMPO);
            continue;
        }
        if (!d || !d->label || d->label[0] == '-') {
            draw_column(c, "", "", "", T_THEME, -1, ICON_AUTO);
            continue;
        }
        if ((act_cols() >> c) & 1u) {
            draw_act_column(c, d->label, T_THEME, ICON_AUTO);
            continue;
        }
        vv = card_vv(vp);                               /* (MORPH: the blended value; the mark) */
        param_format(d, (int16_t)vv, val, &unit);
        card_mot_of(vp);                                /* (a global: never) */
        if (vp == &TSEL->p[P_LRATE] && TSEL->p[P_LSYNC]) {   /* LFO 2 SYNC on: RATE does nothing, DIM (1.2) */
            draw_column(c, d->label, val, unit, T_DIM, RATIO(d, vv), param_icon(d, (int16_t)vv));
            continue;
        }
        draw_column(c, d->label, val, unit, VAL(c), d->fmt == F_ENUM && d->max < 2 ? -1 : RATIO(d, enum_rank(d, (int16_t)vv)),
                    param_icon(d, (int16_t)vv));
    }
}


/* The power-on splash (DUELL 0.1 beta; main.c fm1_main, web/emu felucca_web.c): the title block, left-aligned and vertically
 * centred, no panel or frame, in ONE colour (T_TEXT, the theme's text colour; HI-CON its own). It is a graphic, the art
 * director's lockup (assets/splash_logo.svg, rasterised at build time to a coverage mask, splash_logo.h), not drawn from a font:
 *     DUELL / MultiMorphing / Bitimbral Synthesizer / by OSCILL/R
 * over the thin morphing wave lines of the MORPH page (ui_graph.c mv_*: thin lines, no centre line, calm): SPL_N lines in a
 * slow ribbon (its middle drifting, its width breathing), smooth on the left, with overtones and ripples further right as the
 * morph sweeps across, coloured from the SELECTED THEME's part 1 colour (the top line) to its part 2 colour (the bottom one):
 * violet to pink in VIOLET. High Contrast: flat red lines above, flat blue below, no blend (the palette's c[0], c[2]). The lines run on unbroken behind the text, which is drawn on top: no box, no gap, no halo. Drawn while the
 * synth already runs (audio_init is before it; notes play through it), not by a delay: the first frame at lcd_init, then
 * splash_begin (main.c) and the main loop's ui_draw draw a step every SPL_STEP_MS until SPLASH_MS are over (then the normal
 * screen at once) or any button, key or knob (splash_input, ui_input.c) ends it; MIDI notes never do. MENU > ANIM OFF: the
 * first frame stays still. MENU > SYSTEM > ABOUT shows the same drawing (splash_frame) as its first view, continuously, with no end (ui_menu.c). In the saved palette (settings_init before it). Integer only; three strips of the screen
 * (the mask is clipped to each) */
#define SPLASH_MS 1200u                             /* how long (any key ends it) */
#define SPL_STEP_MS 66u                             /* one animation step (~15 /s, as the MORPH page's) */
#define SPL_N 15                                    /* the lines */
static uint8_t splash_on;
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
/* hint_y: 0 the splash; else the screen row of MENU > ABOUT's key hints (SELECT SCROLL, K1 BACK), drawn on top of the last strip */
static void splash_frame(uint32_t st, int32_t hint_y)
{
    static const int32_t EDGE[4] = {0, 108, 200, 240};   /* the strips: no text row lies across an edge */
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
        if (hint_y && hint_y >= y0 && hint_y < y0 + h)
            cv_key_row(8, 232, hint_y, (const khint_t[2]){{KC_SELECT, "SCROLL"}, {KC_K1, "BACK"}}, 2u, 3u, T_BG);
        cv_oy = 0;
        cv_blit(0, (uint32_t)y0);
    }
    lcd_sync();
}
static void draw_splash(void) { splash_frame(0, 0); }  /* the first frame, at lcd_init (main.c, felucca_web.c) */
static void splash_begin(void)                      /* the synth runs: the splash proper starts (main.c, felucca_web.c) */
{
    splash_on = 1;
    splash_t0 = fm1_ms;
    splash_fr = 0;
}
static void splash_end(void)                        /* the UI takes the screen: all of it drawn again */
{
    splash_on = 0;
    kb_boot_hold = 0;
    lcd_fill(0, 0, 240, 240, T_BG);
    ui.force = 1;
}
static void splash_draw(void)                       /* ui_draw while it is up: a step when due, the end when over */
{
    uint32_t st;
    if (kb_boot_hold && fm1_ms - splash_t0 >= 100u)  /* (#137: the keys are silent for the first 0.1 s only) */
        kb_boot_hold = 0;
    if (fm1_ms - splash_t0 >= SPLASH_MS) {
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
    draw_text_box(0, 76, 240, &AF_M, "UPDATE MODE IN", T_TEXT, 1);
    draw_text_box(0, 102, 240, &AF_L, d, T_THEME, 1);
    {   /* the two keycaps held, and what letting go does */
        int32_t w = kc_w(KC_OCTDN) + 3 + kh_ink(KC_OCTUP, "LET GO TO CANCEL"), x = HALF_UP(240 - w);
        cv_begin(240, KC_H + 2, T_BG);
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(3), "update-mode keys centred");
        x = cv_keycap(x, 1, KC_OCTDN, T_KEY, T_KINK, T_BG) + 3;
        cv_key_hint(x, 1, KC_OCTUP, "LET GO TO CANCEL", 1, T_BG);
        cv_blit(0, 149);
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
    draw_text_box(0, 44, 240, &AF_M, "COUNT-IN", T_TEXT, 1);
    if (total > 4u) {
        str_cpy(sub, "BAR 1 OF 2", sizeof sub);
        sub[4] = (char)('1' + e / 4u % 9u);
        sub[9] = (char)('0' + total / 4u % 10u);
    } else
        str_cpy(sub, "1 BAR", sizeof sub);
    draw_text_box(0, 66, 240, &AF_S, sub, T_MID, 1);
    cv_begin(216, CI_H, T_BG);
    for (j = 0; j < 4u; j++) {
        int32_t x = (int32_t)j * 56;
        char d[2] = {(char)('4' - j), 0};
        int now = j == e % 4u, gone = j < e % 4u;
        uint16_t fill = now ? T_THEME : T_SURF;
        if (ux.style && !now) {                       /* LINE: no SURF card, a 1 px rule round it */
            cv_rrect(x, 0, 48, CI_H, 8, T_RULE, T_BG);
            cv_rrect(x + 1, 1, 46, CI_H - 2, 7, T_BG, T_RULE);
        } else
            cv_rrect(x, 0, 48, CI_H, 8, fill, T_BG);
        GFX_HOOK_ALIGN(x, 0, x + 48, CI_H, AL_HV, "count-in beat centred in its cell");
        cv_text_in(x, CAP_IN(L, CI_H), 48, &AF_L, d, now ? T_INK : gone ? T_DIM : T_TEXT, fill);
    }
    cv_blit(12, CI_Y);
    draw_text_box(0, CI_Y + CI_H + 14, 240, &AF_S, "RECORDING STARTS AFTER 1", T_MID, 1);
    {   /* PLAY stops it */
        int32_t x = HALF_UP(240 - kh_ink(KC_PLAY, "STOP"));
        cv_begin(240, KC_H + 2, T_BG);
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "count-in key centred");
        cv_key_hint(x, 1, KC_PLAY, "STOP", 1, T_BG);
        cv_blit(0, 206);
        lcd_sync();
    }
    ui.force = 0;
}

/* the OCT- / OCT+ dialog: what it does (ui.confirm, ui.confirm_trk) on two lines, on a surface */
#define DLG_X 16
#define DLG_Y 62
#define DLG_W 208
#define DLG_H 116
static void confirm_text(char *a, char *b)
{
    uint32_t k = ui.confirm_trk;
    b[0] = 0;
    switch (ui.confirm) {
    case CF_CLEAR_TRK:
        str_cpy(a, "CLEAR TRACK 1?", 24);
        a[12] = (char)('1' + k % NTRK);
        break;
    case CF_OVR_PROJ:                                   /* MULTI: SAVE over a used multi, ERASE */
    case CF_ERASE_MULTI:
        str_cpy(a, ui.confirm == CF_OVR_PROJ ? "OVERWRITE " : "ERASE ", 24);
        mu_label(a + str_len(a), k % MU_SLOTS);
        str_cpy(a + str_len(a), "?", 2);
        project_name(k % MU_SLOTS, b);                  /* the multi's name ("" none) */
        break;
    case CF_OVR_PSLOT:                                  /* PATTERNS: SAVE over a saved pattern or the factory default */
    case CF_ERASE_PSLOT:
        str_cpy(a, ui.confirm == CF_OVR_PSLOT ? "OVERWRITE " : "ERASE ", 24);
        ps_label(a + str_len(a), k % PS_SLOTS);
        str_cpy(a + str_len(a), "?", 2);
        {                                               /* its name (the stored one, or the factory default's) */
            char tag[4];
            pat_label(k % PS_SLOTS, tag, b);
        }
        break;
    case CF_INIT_SOUND:
        str_cpy(a, "INITIALIZE SOUND?", 24);
        break;
    case CF_SAVE_OVER: {                                /* SAVE held: the patch over itself (ui_input.c patch_save_ask) */
        char n[2][13];
        uint32_t i;
        for (i = 0; i < 2u; i++) {                      /* (no patch: a new slot, named after the engine) */
            uint32_t pi = k < 2u ? k : i;
            patch_name(pi, n[i]);
            if (!n[i][0] && k >= 2u)
                str_cpy(n[i], ENGINES[trk[pi].eng_req % NENGINES]->name, 13);
        }
        if (k >= 2u) {
            str_cpy(a, "SAVE OVER BOTH PATCHES?", 24);
            str_cpy(b, n[0], 24);
            str_cpy(b + str_len(b), " + ", 4);
            str_cpy(b + str_len(b), n[1], 24 - str_len(b));
        } else if (n[0][0]) {
            str_cpy(a, "SAVE OVER ", 24);
            str_cpy(a + str_len(a), n[0], 14);
            str_cpy(a + str_len(a), "?", 2);
        } else {
            str_cpy(a, "SAVE AS NEW SOUND?", 24);
            str_cpy(b, ENGINES[trk[k].eng_req % NENGINES]->name, 24);
        }
        break;
    }
    case CF_CLEAR_MOTION:
        str_cpy(a, "CLEAR T1 AUTOMATION?", 24); a[7] = (char)('1' + k % NTRK);
        break;
    case CF_ERASE_USER: {                           /* PATCHES, K4 ERASE: a patch of its own is emptied, a factory sound's copy gives it back */
        uint32_t sp;
        str_cpy(a, up_shadow(k, &sp) < NENGINES ? "RESTORE FACTORY?" : "ERASE PATCH?", 24);
        up_name(k, b);
        break;
    }
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
static void draw_confirm(void)
{
    char a[24], b[24];
    const aafont_t *tf = &AF_M;
    confirm_text(a, b);
    lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    ui.head_sig = ~0u;
    cv_begin(DLG_W, DLG_H, T_BG);                     /* a SURF card: warning, the question, the detail, two buttons */
    if (ux.style)                                     /* LINE: no card, a 1 px rule round it */
        cv_rrect(0, 0, DLG_W, DLG_H, 8, T_RULE, T_BG);
    cv_rrect(!!ux.style, !!ux.style, DLG_W - 2 * !!ux.style, DLG_H - 2 * !!ux.style, 8 - !!ux.style, T_SURF,
             ux.style ? T_RULE : T_BG);
    GFX_HOOK_ALIGN(0, 0, DLG_W, 0, AL_H, "dialog icon centred");
    cv_icon_in(0, 12, DLG_W, 0, 16, ui.confirm == CF_CLEAR_MOTION ? ICON_X_MOTION_DEL : ICON_X_WARN, T_ACCENT, T_SURF);
    if (text_w(tf, a) > DLG_W - 16)
        tf = &AF_S;
    GFX_HOOK_ALIGN(0, 0, DLG_W, 0, AL_H, "dialog question centred");
    cv_text_in(0, 34, DLG_W, tf, a, T_TEXT, T_SURF);
    if (b[0]) {
        char f[32];
        text_fit(f, sizeof f, b, &AF_M, DLG_W - 16);    /* a name: free text (M: what is selected, as the question) */
        GFX_HOOK_ALIGN(0, 0, DLG_W, 0, AL_H, "dialog detail centred");
        cv_text_flags(ink_in(&AF_M, f, DLG_W), 55, &AF_M, f, T_MID, T_SURF, 8u | (f[str_len(f) - 1u] == ELLIPSIS));
    }
    cv_rrect(10, 80, 90, 26, 6, T_RAISE, T_SURF);     /* OCT-: NO */
    GFX_HOOK_ALIGN(10, 80, 100, 106, AL_HV | AL_N(2), "dialog button NO");
    cv_key_hint(10 + HALF_UP(90 - kh_ink(KC_OCTDN, "NO")), 80 + HALF_UP(26 - KC_H), KC_OCTDN, "NO", 1, T_RAISE);   /* (ink centred) */
    cv_rrect(108, 80, 90, 26, 6, T_THEME, T_SURF);    /* OCT+: YES (on the THEME fill: an INK keycap, THEME label) */
    {
        int32_t x = 108 + HALF_UP(90 - kh_ink(KC_OCTUP, "YES"));
        GFX_HOOK_ALIGN(108, 80, 198, 106, AL_HV | AL_N(2), "dialog button YES");
        x = cv_keycap(x, 80 + HALF_UP(26 - KC_H), KC_OCTUP, T_INK, T_THEME, T_THEME);
        cv_text_on(x + KH_GAP, 80 + HALF_UP(26 - KC_H) - 1, &AF_S, "YES", T_INK, T_THEME);
    }
    cv_blit(DLG_X, DLG_Y);
}

/* the stored STYLE (ui_style: the menu, a settings import, a restore) into gfx.c's copy: the tokens again, all redrawn */
static void style_apply(void)
{
    if (ui_style > ST_LINE)                             /* 2, the retired PIXEL: LINE; unknown: FLAT */
        ui_style = ui_style == 2u ? ST_LINE : ST_FLAT;
    if (ux.style != ui_style) {
        ux.style = ui_style;
        palette_set(settings.palette);
        ui.force = 1;
    }
}

static void ui_draw_page(uint32_t counting);
static void ui_draw(void)
{
    static uint8_t counting;
    style_apply();
    /* the theme follows the selected part (SPEC 5a), its MORPH fader (colour A -> B) and LAYER mode (cheap: only on a change) */
    if (theme_follow(song.sel, morph_fader_of(0), morph_fader_of(1), duell_mode_req == DM_LAYER))
        ui.force = 1;
    ui.frame++;
    if (counting != (uint8_t)seq_counting()) {          /* a count-in began or ended: the page drawn again after it */
        counting = (uint8_t)seq_counting();
        ui.force = 1;
    }
    if (!scr_frame())                                   /* MENU > SCREEN OFF: dark (or waking), nothing drawn */
        return;
    if (splash_on) {                                    /* the power-on splash (above): the page waits under it */
        splash_draw();
        return;
    }
    ui_draw_page(counting);
    scr_shown();
}
static void ui_draw_page(uint32_t counting)
{
    if (ui.uboot) {
        draw_uboot();
        draw_head();
        return;
    }
    if (ui.menu) {
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
    if (!ui.home && !page_visible(ui.page)) {          /* an OP page of a track that is not DIGITAL (without
                                                         * FELUCCA_FM4: any track): EDIT 1 */
        ui.page = (uint8_t)page_first(FAM_EDIT);
        page_entered();
    }
    cursor_fix();
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
    if (ui.help_t)
        ui.help_t--;
    felucca_dbg.stage = 6;
    draw_foot();
    ui.force = 0;
}
