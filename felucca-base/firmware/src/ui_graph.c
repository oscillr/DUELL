/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI: the panel (Y_GRAPH .. Y_GRAPH + H_GRAPH) between the cards and the footer: a SURF area
 * (rounded, on BG) holding the page graphs (ADSR, LFO, steps, piano roll, scale, FX, SLICER,
 * MOD, lists: presets, user presets, patterns, project slots, song), the HOME oscilloscope, the
 * instrument diagrams (GLO's MIXER page is drawn by ui_layer.c draw_layer, as the hold-GLO layer). Each graph is redrawn only when
 * graph_signature() changes. The look:
 * curves THEME (2 px), guides and empty marks RAISE, captions MID / DIM, the active thing ACCENT,
 * bars rounded; a list's selected row is a THEME bar with INK text. */
#define PANEL_X0 10                                  /* the graphs' inner area: x 10..230 */
#define PANEL_W 220
#define GOY (grid_on ? 1 : 11)                                       /* graphs drawn on a 100 px scale sit at y 11..111 */
/* the height ADSR and LFO are drawn on: 100 px */
#define graph_ht (grid_on ? 76 : 100)
/* the caption row of a part page's graph card (item 119): S, left aligned at x UT_GR_NOTE_X, its capitals' last row on UT_GR_NOTE_BASE (HOME's note row, canvas row 74 of the 76); on the old tall panel (grid_on 0) row 94 (was 98: a 15 px line was cut by the panel's end) */
static void graph_cap_at(int32_t x, const char *s, uint16_t col, int32_t up)   /* up: rows above the caption row (a caption of two lines: the first one 17 up) */
{
    int32_t o = cv_oy;
    cv_oy = 0;
    if (grid_on)
        cv_text_on(x, UT_TY(S, UT_GR_NOTE_BASE - UT_GR_Y) - up, &AF_S, s, col, T_BG);
    else
        cv_text_on(x, 94 + o - up, &AF_S, s, col, T_BG);   /* (the old tall panel: its capitals' last row 116 of the 122) */
    cv_oy = o;
}
static void graph_cap(const char *s, uint16_t col) { graph_cap_at(grid_on ? UT_GR_NOTE_X : PANEL_X0 + 2, s, col, 0); }
static int32_t guide_wrap(int32_t x, int32_t y, int32_t w, const aafont_t *f, const char *s, uint16_t fg, int32_t p);   /* (ui_guide.c) */
static uint32_t knob_slot(const page_t *pg, uint32_t k);   /* (ui_draw.c, T0227: the page slot a knob addresses once the cards moved left) */
#ifndef CARD_LABEL_HOOK
#define CARD_LABEL_HOOK(c, l, need, room) ((void)0)    /* host: a label wider than its room (ui_draw.c); here the PARAMETER LOCKS row's long name (item 120) */
#endif
#include "ui_fm6op.c"                                /* EDIT > OPERATOR, OP ENV, OPERATOR 2: FM6's operators (their charts below) */
#include "ui_kb.c"                                   /* the keyboard drawing of GLOBAL SCALE and CHORD (kb_draw; LAYOUT_IMPLEMENTATION_PLAN.md block F) */
static void area_rect(int32_t *y, int32_t *h);       /* (ui_draw.c, TEMPLATE 9: the big area of the area page, screen rows) */
/* TEMPLATE 9 (grid_on 16, the AREA page): every picture of the page is drawn into the big area (area_rect), one canvas the area's height, blitted at its first row */
#define AREA_PIC (grid_on == 16)

/* Matches voice.c: attack is linear, decay and release are exponential
 * (env += (target - env) * k each tick, ~99 % after the set time). Time
 * axis is the parameter value (the times themselves are exponential). */
/* the envelope a GR_ADSR page draws (0.2.25 layout round, owner: a DEST page draws its envelope's curve, as its first page): AMP DEST the AMP ENV's, ENV 2 DEST ENV 2's;
 * the envelope pages their own four. -> its first id (ATTACK; DECAY, SUSTAIN, RELEASE follow it) */
static uint32_t env_of(const page_t *pg)
{
    return pg->id[0] == P_ED_PIT ? (uint32_t)P_ATK : pg->id[0] == P_E2FLT ? (uint32_t)P_E2ATK : pg->id[0];
}
/* the guide line of a graph (ENV: the baseline, LFO: the zero line): on a template-1 page the picture's grey, else RAISE */
#define GR_GUIDE (grid_on == 1 ? ux_mix(T_BASE, T_TEXT, UT_GR_GUIDE_PCT) : T_RAISE)
/* one segment of the envelope, in two passes (template 1, item 114): pass 0 fills the area under the line down to the baseline (UT_GR_FILL_PCT of the part's colour over the card), pass 1 draws the 2 px line.
 * The line is the same on every page; only a template-1 page has the fill */
static void env_seg(int32_t pass, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t bot, uint16_t c, uint16_t fill)
{
    if (pass == 0) {
        int32_t x, n = x1 - x0 ? x1 - x0 : 1;
        for (x = x0; x <= x1; x++) {
            int32_t y = y0 + (y1 - y0) * (x - x0) / n;
            if (bot >= y)
                cv_rect(x, y, 1, bot - y + 1, fill);
        }
    } else
        cv_line_t(x0, y0, x1, y1, c, 2);
}
static void graph_adsr(const track_t *t, uint16_t c)
{
    const page_t *pg = cur_page();
    uint32_t e0 = env_of(pg);
    int32_t a = 4 + t->p[e0] * 50 / 127, d = 6 + t->p[e0 + 1u] * 50 / 127, r = 6 + t->p[e0 + 3u] * 60 / 127;
    int32_t top = 6 * graph_ht / 100, bot = 88 * graph_ht / 100, sus = t->p[e0 + 2u] * 1000 / 127;   /* 0..1000 */
    int32_t x0 = 12, x1 = x0 + a, x3 = 226 - r, i, px, py, pass;
    uint16_t fill = ux_mix(T_BASE, T_THEME, UT_GR_FILL_PCT);
    CV_HL(PANEL_X0, 0, PANEL_W, graph_ht);                      /* (data: the area under the envelope) */
#define EGY(lvl) (bot - (lvl) * (bot - top) / 1000)
    cv_rect(PANEL_X0, bot + (grid_on == 1 ? 1 : 2), PANEL_W, 1, GR_GUIDE);
    for (pass = grid_on == 1 ? 0 : 1; pass < 2; pass++) {
        int32_t e = 32768;                                              /* exp(-4.6 u), Q15 */
        env_seg(pass, x0, bot, x1, top, bot, c, fill);                  /* attack: linear */
        px = x1;
        py = top;
        for (i = 1; i <= d; i++) {                                      /* decay: exponential to SUS */
            int32_t lvl;
            e = (e * (32768 - 150733 / d)) >> 15;           /* k^d = exp(-4.6) */
            lvl = sus + ((1000 - sus) * e >> 15);
            env_seg(pass, px, py, x1 + i, EGY(lvl), bot, c, fill);
            px = x1 + i;
            py = EGY(lvl);
        }
        env_seg(pass, px, py, x3, EGY(sus), bot, c, fill);              /* sustain */
        px = x3;
        py = EGY(sus);
        e = 32768;
        for (i = 1; i <= r; i++) {                                      /* release: exponential to 0 */
            e = (e * (32768 - 150733 / r)) >> 15;
            env_seg(pass, px, py, x3 + i, EGY(sus * e >> 15), bot, c, fill);
            px = x3 + i;
            py = EGY(sus * e >> 15);
        }
    }
#undef EGY
}

static void graph_lfo(const track_t *t, uint16_t c)
{
    const page_t *pg = cur_page();
    int l2 = (pg->id[0] >= P_L2RATE && pg->id[0] <= P_L2FADE) || (pg->id[0] >= P_L2SYNC && pg->id[0] <= P_L2POL) || pg->id[0] == XW_BASE + X_LFO2;   /* DUELL: LFO 2's pages (any of its ids in slot 1) draw LFO 2; 0.2.25: LFO 2 DEST too (LFO 1 DEST: LFO 1's) */
    uint32_t r = l2 ? P_L2RATE : P_LRATE, s = l2 ? P_L2SYNC : P_LSYNC;
    int32_t x, cy = graph_ht / 2 - (grid_on == 1), a = 38 * graph_ht / 100, py = cy, uni = t->p[s + 2u] != 0;   /* (template 1: the zero line on screen row 65, the picture's) */
    uint32_t ph = (uint32_t)t->p[r + 2u] << 25;
    int32_t z = uni ? cy + a : cy;                   /* the zero line: POL UNI draws 0..+1 over it */
    cv_rect(PANEL_X0, z, PANEL_W, 1, GR_GUIDE);
    for (x = 0; x < PANEL_W; x++) {                  /* two cycles (the wave of the page's LFO) */
        int32_t w = lfo_wave_of(t->p[r + 1u], l2 ? t->lfo2.rnd : t->lfo_rnd, ph + (uint32_t)x * (0xFFFFFFFFu / (PANEL_W / 2u))), y;
        if (t->p[r + 1u] == 4)
            w = (int32_t)((x / 20 * 2654435761u) >> 16) - 32768;
        y = uni ? z - (w + 32768) * a / 32768 : cy - w * a / 32768;
        if (x)
            cv_line_t(PANEL_X0 + x - 1, py, PANEL_X0 + x, y, c, 2);
        py = y;
    }
}

/* step bar x of step i of a 16-step row: 4 groups, as the footer (9 px bars, 13 px apart, 4 px between groups),
 * the row centred: x 12 .. 228 */
static int32_t bar_x(uint32_t i) { return 12 + (int32_t)(i % 16u) * 13 + (int32_t)(i % 16u / 4u) * 4; }

/* STEP page (a melodic track): a piano roll of the cursor's 16-step page. PR_ROWS semitone rows of PR_RH px,
 * the highest on top; on the left the C rows named (S, DIM) and a keyboard strip (white keys RAISE, black keys a
 * shorter DIM; a key held on the track: its row ACCENT). Rows in the track's scale (SCL ROOT / SCALE; CHR: the white
 * keys) are tinted LANE, a C row closes with a RAISE line; step lines GRID, every 4th RAISE. A note is a 9 x 3 bar in
 * its column (an accent: TEXT, the row's full height; the cursor step's notes: ACCENT), chords stacked (a RATCH step's bars in its 2..4 parts, 1 px apart); a TIE carries the bars of the note before through
 * its column; a REST is empty; a slide is a TEXT diagonal from the end of its bar into the next note (not from a
 * RATCH step: it never slides out). Notes outside
 * the view: a 1 px mark on the edge. The cursor: a TEXT column frame; the playhead: a 1 px ACCENT line. The view
 * (proll.lo, the bottom row) fits the notes of the page, else centres on the cursor's note, and follows in steps (a
 * third of the way per frame); graph_signature() holds it, so a page that does not change is not drawn again. */
/* TEMPLATE 9 (the SEQUENCER page in the big area, NEW_LAYOUT_SHEET_V2.png): the roll is a WINDOW of rows UT_A_ROLL_Y .. + UT_A_ROLL_H - 1 (52 .. 143) on the same 8 px rows, the first row's top
 * one row above the window (row 51, so that C4 sits on rows 91 .. 98 as the picture): 12 rows, 10 whole and the two at the edges cut by the window (about 11 rows, as the picture: E4, C4, G3 named).
 * The cursor frame, the playhead and the step lines span the window; the parameter lock marks sit under it */
#define PR_AREA (grid_on == 16)
#define PR_ROWS (PR_AREA ? 12 : UT_T7_ROWS)          /* semitones shown (item 118, TEMPLATE 7: 17, 1.4 octaves; was 21 at 5 px) */
#define PR_RH UT_T7_RH                              /* px per row */
#define PR_Y0 (PR_AREA ? UT_A_ROLL_Y - 1 : UT_T7_Y0)   /* the top row (SCREEN row: the roll is drawn in two strips, graph_roll_t7; TEMPLATE 9: one canvas, the area) */
#define PR_WY0 (PR_AREA ? UT_A_ROLL_Y : PR_Y0)        /* the window the roll shows: its first screen row and the row after its last */
#define PR_WY1 (PR_AREA ? UT_A_ROLL_Y + UT_A_ROLL_H : PR_Y0 + PR_ROWS * PR_RH)
#define PR_LOCK_Y (PR_AREA ? UT_A_ROLL_Y + UT_A_ROLL_H + 1 : PR_Y0 + PR_ROWS * PR_RH + UT_T7_LOCK_DY)   /* the lock mark's top (TEMPLATE 9: not in the picture; one row under the window, QUESTION in the report) */
#define PR_X0 UT_T7_X0                              /* the first step column */
#define PR_CW UT_T7_CW                              /* px per step */
#define PR_KX UT_T7_KX                              /* the keyboard strip, 9 px (black keys 5) */
#define PR_KEYW ux_mix(T_BASE, T_TEXT, UT_T7_KEYW_PCT)   /* the roll's greys (item 118, measured on FINAL_V1_07): a white key, the C rows' and beats' lines */
#define PR_KEYB ux_mix(T_BASE, T_TEXT, UT_T7_KEYB_PCT)
#define PR_LANE ux_mix(T_BASE, T_THEME, UT_T7_LANE_PCT)
static struct {
    uint8_t lo, trk, init, moving;                    /* lo: the note of the bottom row; moving: on its way */
    uint32_t frame;
} proll;
static const uint8_t KEY_BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

static int32_t pr_note(const step_t *st, uint32_t j)   /* the step's note j (0..3; -1: none) */
{
    return j < st->n ? st->note[j] : -1;
}
static uint32_t pr_src(const track_t *t, uint32_t si, uint32_t len)   /* the step whose notes sound at si (NSTEP: none) */
{
    uint32_t k;
    for (k = 0; k < len && seq_steps(t)[si].time == ST_TIE; k++)
        si = (si + len - 1u) % len;
    return step_on(&seq_steps(t)[si]) ? si : NSTEP;
}
/* the rows lit by keys held on the selected track (bit r: row r from the top) */
static uint32_t pr_held(void)
{
    uint32_t k, i, m = 0;
    for (k = 0; k < 27u; k++)
        if (kb_chn[k] && kb_trk[k] == song.sel)
            for (i = 0; i < kb_chn[k]; i++) {
                int32_t r = (int32_t)proll.lo + PR_ROWS - 1 - kb_chord[k][i];
                if (r >= 0 && r < PR_ROWS)
                    m |= 1u << r;
            }
    return m;
}
/* once a frame: the view toward the page's notes (centred; once there it stays while they fit); at once on a redraw
 * forced, a gap in the frames, another track or MENU > ANIM OFF */
static void pr_follow(const track_t *t)
{
    uint32_t i, j, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    int32_t lo = 127, hi = -1, tgt, d, cur = proll.lo;
    int snap = ui.force || !proll.init || proll.trk != song.sel || ui.frame != proll.frame + 1u || (ui_prefs & PREF_ANIM_OFF);
    if (proll.init && proll.frame == ui.frame)
        return;
    for (i = 0; i < 16u && base + i < len; i++) {
        uint32_t s = pr_src(t, base + i, len);
        for (j = 0; s < NSTEP && j < 4u; j++) {
            int32_t n = pr_note(&seq_steps(t)[s], j);
            if (n < 0)
                continue;
            lo = n < lo ? n : lo;
            hi = n > hi ? n : hi;
        }
    }
    if (hi < 0)
        lo = hi = last_note;
    if (hi - lo <= PR_ROWS - 1)                     /* all fit: centred, then kept while they fit */
        tgt = !snap && !proll.moving && lo >= cur && hi <= cur + PR_ROWS - 1 ? cur : (lo + hi + 1) / 2 - PR_ROWS / 2;
    else {                                            /* too wide: the cursor's (lowest) note in the middle */
        const step_t *st = &seq_steps(t)[ui.cursor];
        tgt = (st->n ? st->note[0] : (lo + hi) / 2) - PR_ROWS / 2;
    }
    tgt = clamp(tgt, 0, 128 - PR_ROWS);
    d = tgt - cur;
    proll.lo = (uint8_t)(snap ? tgt : cur + (d / 3 ? d / 3 : d > 0 ? 1 : d < 0 ? -1 : 0));
    proll.moving = proll.lo != tgt;
    proll.trk = (uint8_t)song.sel;
    proll.frame = ui.frame;
    proll.init = 1;
}
static int32_t pr_row_y(int32_t n) { return PR_Y0 + ((int32_t)proll.lo + PR_ROWS - 1 - n) * PR_RH; }
/* a bar w px wide in `hits` equal parts, 1 px apart (a RATCH step; 1: the bar) */
static void pr_split(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, uint32_t hits)
{
    int32_t k, pw = (w - (int32_t)hits + 1) / (int32_t)hits;
    if (hits < 2u) {
        cv_rect(x, y, w, h, c);
        return;
    }
    for (k = 0; k < (int32_t)hits; k++)
        cv_rect(x + k * (pw + 1), y, pw, h, c);
}
/* the bars of step s in column x (in its RATCH parts; a TIE's carried bars: 1): a 1 px edge mark for a note
 * out of view; first: row y of the step's first note */
static void pr_bars(const step_t *st, int32_t x, int32_t w, uint16_t c, int acc_all, int32_t *first, uint32_t hits)
{
    uint32_t j;
    int32_t ybot = PR_Y0 + PR_ROWS * PR_RH;
    for (j = 0; j < 4u; j++) {
        int32_t n = pr_note(st, j), y;
        int acc;
        if (n < 0)
            continue;
        acc = acc_all;
        y = pr_row_y(n);
        if (*first < -1000)
            *first = y;
        if (y < PR_Y0)                                /* (the edge marks on the window's edges: TEMPLATE 9's window is not the rows' span) */
            pr_split(x, PR_WY0, w, 1, c, hits);
        else if (y >= ybot)
            pr_split(x, PR_WY1 - 1, w, 1, c, hits);
        else if (acc)
            pr_split(x, y, w, PR_RH, c == T_THEME ? c : T_TEXT, hits);
        else
            pr_split(x, y + 1, w, PR_RH - 2, c, hits);
    }
}
static void graph_roll(const track_t *t, uint16_t c)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u, ncol, mask = scale_mask(t), held = pr_held();
    uint64_t locks = motion_lock_steps(trk_index(t));
    int32_t r, ybot = PR_Y0 + PR_ROWS * PR_RH, gw;
    char nb[8];
    ncol = base < len ? (len - base < 16u ? len - base : 16u) : 0u;
    gw = (int32_t)ncol * PR_CW + 1;
    for (r = 0; r < PR_ROWS; r++) {                 /* rows: the lane, the key, the C names */
        int32_t n = (int32_t)proll.lo + PR_ROWS - 1 - r, y = PR_Y0 + r * PR_RH;
        uint32_t pc = (uint32_t)n % 12u;
        int in = mask == 0xFFFu ? !KEY_BLACK[pc] : (int)((mask >> ((uint32_t)(n - root_of(t) + 120) % 12u)) & 1u);
        if (in)
            cv_rect(PR_X0, y, gw, PR_RH, PR_LANE);
        if ((held >> r) & 1u)
            cv_rect(PR_KX, y, 9, PR_RH - 1, T_ACCENT);
        else
            cv_rect(PR_KX, y, KEY_BLACK[pc] ? 5 : 9, PR_RH - 1, KEY_BLACK[pc] ? PR_KEYB : PR_KEYW);
        if (pc == 0u)
            cv_rect(PR_X0, y + PR_RH - 1, gw, 1, PR_KEYW);
        if ((pc == 0u || pc == 4u || pc == 7u) &&      /* C, E and G named (S, DIM): TEMPLATE 7, the strip's left, the advance ending at UT_T7_NAME_R, capitals' last row = the row's top + RH */
            (!PR_AREA || (y + PR_RH - AF_S_CAP_H + 1 >= PR_WY0 && y + PR_RH < PR_WY1))) {   /* (TEMPLATE 9: only a name whose capitals are whole inside the window, never a cut one) */
            note_name(nb, (uint32_t)n);
            GFX_HOOK_ALIGN(0, y, UT_T7_NAME_R, y + PR_RH, AL_R | AL_PASS, "piano roll note names' right edge");
            cv_text_on(UT_T7_NAME_R - text_w(&AF_S, nb), UT_TY(S, y + PR_RH), &AF_S, nb, T_DIM, T_BG);
        }
    }
    for (i = 0; i <= ncol; i++)                       /* step lines, the beats brighter (over the window) */
        cv_rect(PR_X0 + (int32_t)i * PR_CW, PR_WY0, 1, PR_WY1 - PR_WY0, i % 4u ? T_GRID : PR_KEYW);
    for (i = 0; i < ncol; i++) {
        uint32_t si = base + i;
        int32_t x = PR_X0 + (int32_t)i * PR_CW;
        if (si == ui.cursor) {                        /* the cursor frame: 2 rows over the rows on each side (TEMPLATE 9: the window's height, as the picture) */
            if (PR_AREA)
                cv_frame(x, PR_WY0, PR_CW + 1, PR_WY1 - PR_WY0, T_TEXT);
            else
                cv_frame(x, PR_Y0 - 2, PR_CW + 1, ybot - PR_Y0 + 4, T_TEXT);
        }
        if (song.playing && si == t->seq_idx)
            cv_rect(x + 6, PR_WY0, 1, PR_WY1 - PR_WY0, T_ACCENT);
    }
    for (i = 0; i < ncol; i++) {                      /* the notes */
        uint32_t si = base + i, s = pr_src(t, si, len), nx = (si + 1u) % len;
        const step_t *st, *ns = &seq_steps(t)[nx];
        int32_t x = PR_X0 + (int32_t)i * PR_CW, y = -10000;
        uint16_t col;
        if (s == NSTEP)
            continue;                                 /* a REST, empty, or a TIE after one */
        st = &seq_steps(t)[s];
        col = s == ui.cursor || si == ui.cursor ? c : ux_mix(T_BASE, c, UT_T7_NOTE_PCT);   /* (item 118, the picture: the notes dim, the cursor step's in the part's colour) */
        if (s == si)
            pr_bars(st, x + UT_T7_NOTE_X, UT_T7_NOTE_W, col, (st->flags & SF_ACCENT) != 0u, &y, step_ratchet(st));
        else                                          /* a TIE: the bars on through the gap before */
            pr_bars(st, x - 1, 12, col, (st->flags & SF_ACCENT) != 0u, &y, 1u);
        if ((st->flags & SF_SLIDE) && step_ratchet(st) == 1u &&   /* (a ratcheted step never slides out) */
            ns->time == ST_NOTE && ns->n && y >= PR_Y0 && y < ybot) {
            int32_t ny = clamp(pr_row_y(ns->note[0]), PR_Y0, ybot - PR_RH);
            cv_line(x + UT_T7_NOTE_X + UT_T7_NOTE_W, y + PR_RH / 2, x + PR_CW + UT_T7_NOTE_X + 2, ny + PR_RH / 2, T_TEXT);   /* the slide into the next note (from the end of its bar to the start of the next) */
        }
    }
    if (PR_AREA) {                                    /* TEMPLATE 9: the rows cut at the window's edges (the area's rows above and under the window back to the background) */
        int32_t ay, ah;
        area_rect(&ay, &ah);
        cv_rect(0, ay, 240, PR_WY0 - ay, T_BG);
        cv_rect(0, PR_WY1, 240, ay + ah - PR_WY1, T_BG);
    }
    for (i = 0; i < ncol; i++)                        /* a parameter lock: a mark under the step */
        if ((locks >> (base + i)) & 1u)
            cv_rect(PR_X0 + (int32_t)i * PR_CW + 3, PR_LOCK_Y, 7, 2, T_ACCENT);
}
/* GLOBAL SCALE, TEMPLATE 9 (SCALE_CHORD_NEW_LAYOUT_V3.png): the keyboard (kb_draw, ui_kb.c) on rows UT_A_KB_Y .. + UT_A_KB_H - 1 (56 .. 130), x UT_A_KB_X .. + UT_A_KB_W, drawn on the area
 * canvas in SCREEN rows (draw_graph sets cv_oy); one octave from C, lit = the scale's notes, the root marked (kb_draw's light key). The old bars (black keys high, white keys low) are gone */
static uint32_t scale_lit(const track_t *t)          /* the scale's notes as semitones from C (bit 0 = C) */
{
    uint32_t i, lit = 0, mask = scale_mask(t);
    for (i = 0; i < 12u; i++)
        if ((mask >> ((i + 12u - (uint32_t)root_of(t) % 12u) % 12u)) & 1u)
            lit |= 1u << i;
    return lit;
}
static void graph_scale(const track_t *t, uint16_t c)
{
    /* CHROMATIC: the OFF-SCALE card is hidden on this page (ui_draw.c draw_column_hide), so the old note "NO SCALE: OFF-SCALE HAS NO EFFECT" (two lines; they do not fit under the keyboard)
     * is not drawn: QUESTION in the report */
    kb_draw(UT_A_KB_X, UT_A_KB_Y, UT_A_KB_W, UT_A_KB_H, scale_lit(t), (uint32_t)root_of(t) % 12u, 1u, c);
}
static void panel_note(const char *a, const char *b, const char *c);
static void panel_alert(uint32_t id, const char *a);
#if DUELL_TEST_MODE
static void graph_test(void);                        /* MENU > SYSTEM > TEST (ui_menu.c: the CPU meter's code) */
#endif
static uint32_t test_sig(void);
static uint32_t tempo_tapping(void);                   /* TEMPO's TAP: the taps of the last 2 s (ui_input.c) */
/* CHORD, TEMPLATE 9 (CHORD_NEW_LAYOUT_V5.png, change list B.4 and B.9 a): the keyboard, two octaves from the C below the chord's lowest note, on GLOBAL SCALE's rows. The chord's name is on
 * the knob-1 line (ui_draw.c k1_after_of); the old name, "LOWEST KEY" and the right caption (LAST, ON ROOT, ROOT ONLY, NEEDS SCALE) are gone. The lit mask is made apart from the drawing
 * (chord_keys, then chord_kb) so that a later job can pass the notes that sound (change list B.9 b): lit = bit mask of the semitones counted from the C at `lo` (24 bits), root = the root
 * key's semitone counted the same way, or 0xFF for none */
#define CHORD_KB_SPAN 24u                            /* two octaves */
static uint32_t chord_keys(const track_t *t, uint32_t *lo_out, uint32_t *root_out)   /* the chord shown (the last one played, else the one on the scale's root from C4): its lit mask */
{
    uint8_t nn[CHORD_MAX];
    uint32_t n, i, k = trk_index(t), lo, lit = 0;
    int32_t r;
    uint16_t mask;
    if (chord_last[k].n) {
        r = chord_last[k].root;
        n = chord_last[k].n;
        for (i = 0; i < n && i < CHORD_MAX; i++)
            nn[i] = chord_last[k].note[i];
    } else
        n = chord_make(t, (uint32_t)(60 + root_of(t)), nn, &r, &mask);
    *root_out = 0xFFu;
    *lo_out = 48u;
    if (!n)
        return 0;
    lo = nn[0] - nn[0] % 12u;                        /* (the C below the lowest note: the notes come lowest first, as the old picture took them) */
    for (i = 0; i < n && i < CHORD_MAX; i++) {
        uint32_t d = (uint32_t)nn[i] - lo;
        if (d < CHORD_KB_SPAN) {                     /* (a note beyond two octaves has no key: QUESTION in the report) */
            lit |= 1u << d;
            if (*root_out == 0xFFu && (uint32_t)nn[i] % 12u == (uint32_t)r % 12u)
                *root_out = d;                       /* the root: its lowest key */
        }
    }
    *lo_out = lo;
    return lit;
}
/* CHORD live lighting (owner decision, change list B.9 b): the notes the selected part's chord keys sound now, in chord mode only (the part's CHORD TYPE not OFF; else none):
 * bit n of set[n >> 5] = MIDI note n. The tones each held key has started (seq.c key_on, chord.c: kb_chord / kb_chn; with STRUM the tones start one after the other, so they light
 * as they start); LAYER and part 2 selected: part 2's own copy (kb_chord2 / kb_chn2). Read in the main loop from the audio interrupt's bytes, as pr_held does: no work is added to the
 * interrupt. MIDI chords are not counted here (their keys light through ui_input.c midi_leds). Used by the CHORD page's keyboard (graph_chord) and the key LEDs (ui_input.c chord_leds).
 * -> the number of different notes */
static uint32_t chord_sounding(uint32_t *set)
{
    uint32_t k, i, n = 0, p = song.sel, two = duell_mode == DM_LAYER && p == 1u;
    set[0] = set[1] = set[2] = set[3] = 0;
    if (!TSEL->p[P_CHRD])
        return 0;
    for (k = 0; k < 27u; k++) {
        const uint8_t *nt = two ? kb_chord2[k] : kb_chord[k];
        uint32_t c = two ? kb_chn2[k] : kb_trk[k] == p ? kb_chn[k] : 0u;
        for (i = 0; i < c && i < CHORD_MAX; i++) {
            uint32_t x = nt[i] & 127u;
            if (!((set[x >> 5] >> (x & 31u)) & 1u)) {
                set[x >> 5] |= 1u << (x & 31u);
                n++;
            }
        }
    }
    return n;
}
/* the CHORD page's lit mask from the sounding notes (chord_sounding): two octaves from the C below the lowest sounding note, a note beyond them has no key (as chord_keys);
 * the root marked as chord_keys marks it: the lowest key of the last chord played's root (chord_last), 0xFF when that root is not among the lit notes */
static uint32_t chord_live_keys(const track_t *t, const uint32_t *set, uint32_t *root_out)
{
    uint32_t x, lo = 128u, lit = 0, rp = chord_last[trk_index(t)].root % 12u;
    *root_out = 0xFFu;
    for (x = 0; x < 128u; x++)
        if ((set[x >> 5] >> (x & 31u)) & 1u) {
            if (lo == 128u)
                lo = x - x % 12u;                    /* (the C below the lowest note) */
            if (x - lo < CHORD_KB_SPAN) {
                lit |= 1u << (x - lo);
                if (*root_out == 0xFFu && chord_last[trk_index(t)].n && x % 12u == rp)
                    *root_out = x - lo;
            }
        }
    return lit;
}
static void chord_kb(uint32_t lit, uint32_t root, uint16_t c)
{
    kb_draw(UT_A_KB_X, UT_A_KB_Y, UT_A_KB_W, UT_A_KB_H, lit, root, 2u, c);
}
static void graph_chord(const track_t *t, uint16_t c)
{
    uint32_t root, lo, lit;
    if (!t->p[P_CHRD]) {                             /* TYPE OFF: the keyboard with no lit note, the hint LEFT aligned under it (its pen on the keyboard's left edge) */
        int32_t ay, ah, base = UT_A_KB_Y + UT_A_KB_H + 14;   /* (the hint's capitals' last row: 4 rows of air under the keyboard, capitals on rows 135 .. 145) */
        chord_kb(0u, 0xFFu, c);
        area_rect(&ay, &ah);
        if (base < ay + ah)
            cv_text_on(UT_A_KB_X, UT_TY(S, base), &AF_S, "KEYS PLAY CHORDS IN THE SCALE", T_MID, T_BG);
        return;
    }
    {
        uint32_t set[4];
        if (chord_sounding(set))                     /* keys held: the notes their chords sound now (live lighting) */
            lit = chord_live_keys(t, set, &root);
        else                                         /* none held: the last chord played, as before */
            lit = chord_keys(t, &lo, &root);
    }
    chord_kb(lit, root, c);
}
/* the four sends as faders under their cards: a RAISE slot, the THEME fill from the bottom, a cap */
static void graph_fx(const track_t *t, uint16_t c)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        int32_t h = t->p[P_DIST + i] * 76 / 127, x = CARD_X(i) + 26;
        cv_rrect(x, 6, 4, 82, 2, T_RAISE, T_BG);
        if (h > 3)
            cv_rrect(x, 88 - h, 4, h, 2, c, T_RAISE);
        cv_rrect(x - 4, 85 - h, 12, 6, 3, c, T_BG);
    }
}
/* INSERT pages (1.5, fx.c track_insert): the drives and CRUSH as their curve, input (-1.5 .. +1.5 of a voice's full
 * scale) across, output up, dry and wet blended by MIX (fx.c ins_shape: the same shape; the dry diagonal RAISE); the
 * swept ones as two cycles of their sweep, as deep as DEPTH (FLANG the squared triangle, CHOR a sine, PHASR a
 * triangle); TAPE (0.2.14) as its SAT curve, VINYL as a record's trace: the warp's slow wave (WARP), the noise ripple (NOISE),
 * the clicks (DUST: more of them). OFF: the dry diagonal, dim */
static void graph_insert(const track_t *t, uint16_t c)
{
    int32_t x, cy = graph_ht / 2, a = 38 * graph_ht / 100, py = cy, ty = clamp(t->p[P_ITYPE], 0, IT_N - 1);
    int32_t m = ins_mixq(t->p[P_IMIX]), g = 4096 + t->p[P_IA] * t->p[P_IA] * 3, lvl = LEVEL_Q12[t->p[P_IC] & 127];
    uint32_t sh = ins_bits_step(t->p[P_IA]);
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
    cv_line(PANEL_X0 + PANEL_W / 2, cy - a, PANEL_X0 + PANEL_W / 2, cy + a, T_RAISE);
    if (ty == IT_OFF || (ty >= IT_PHASER && ty != IT_TAPE) || !m)
        cv_line(PANEL_X0 + PANEL_W / 6, cy + a, PANEL_X0 + PANEL_W * 5 / 6, cy - a, ty ? T_RAISE : T_DIM);
    if (ty == IT_OFF)
        return;
    for (x = 0; x < PANEL_W; x++) {
        int32_t y, w;
        if (ty == IT_VINYL) {                        /* the record: WARP's slow sine (one cycle), NOISE's ripple, DUST's clicks (fixed places, more of them) */
            uint32_t h = (uint32_t)x * 2654435761u >> 24, h2 = ((uint32_t)x * 40503u >> 7) & 255u;
            w = osc_sine((uint32_t)x * (0xFFFFFFFFu / PANEL_W)) / 3 * t->p[P_IC] / 127 + ((int32_t)h - 128) * 48 * t->p[P_IB] / 127;
            if (h2 < (uint32_t)t->p[P_IA] / 5u)
                w += (h & 1u ? 1 : -1) * (12000 + (int32_t)h * 40);
            y = cy - clamp(w, -32767, 32767) * a / 32768;
        } else if (ty >= IT_PHASER && ty != IT_TAPE) {   /* the sweep, two cycles */
            uint32_t ph = (uint32_t)x * (0xFFFFFFFFu / (PANEL_W / 2u)), tr = pf_tri(ph);
            w = ty == IT_CHORUS ? osc_sine(ph) * 2 : ty == IT_FLANGER ? (int32_t)((tr * tr) >> 16) * 2 - 65536 : (int32_t)tr * 2 - 65536;
            y = cy - (w >> 1) * a / 32768 * t->p[P_IB] / 127;
        } else {                                     /* the curve: v in, dry v, wet shaped */
            int32_t v = (x - PANEL_W / 2) * 49152 / (PANEL_W / 2), o;
            if (ty == IT_TAPE)                       /* SAT's curve (WOW, TONE: not in a static curve) */
                o = ins_tape_sat(v >> 3, 4096 + t->p[P_IB] * t->p[P_IB], ins_tape_comp(4096 + t->p[P_IB] * t->p[P_IB]));
            else
                o = ty == IT_CRUSH ? ins_shape(IT_CRUSH, v, sh) : (ins_shape((uint32_t)ty, ((v >> 3) * g) >> 9, 0) * lvl) >> 12;
            w = v + mulq16(o - v, (uint32_t)m << 1);
            y = cy - clamp(w, -49152, 49152) * a / 49152;
        }
        if (x)
            cv_line_t(PANEL_X0 + x - 1, py, PANEL_X0 + x, y, c, 2);
        py = y;
    }
}
/* MOD pages: the page's four slots as rows "1 LFO > CUT +50%" (M, a list's rows: the source to x 78, the arrow 84..98, the
 * destination of up to 5 characters 106..160, the amount from 172), the one KNOB 2..4 edit selected, slots that do
 * nothing (SRC, DEST or AMT at 0, or a destination the part cannot move now: mod.c mod_dst_why) dim. Under them (0.2.7, owner:
 * "you do not know what E7 or SYNC is") the selected slot's destination in full, "DEST: WAVE SYNC" (M), and on a line of its
 * own (S) why it does nothing, or which bus a bus value moves (mod.c mod_dst_line) */
/* a matrix source as the unit shows it: in full words (DUELL 1.1, owner), a two-word name with its break ("AMP\nENV") for the SOURCE card (ui_draw.c draw_column: two lines in S);
 * the stored N_MSRC names stay the keys (the factory sound tool, the editor). A name the card or the list row has no room for is cut by the drawing (ellipsis), never shortened */
static const char *msrc_shown(int32_t s)
{
    s = clamp(s, 0, MS_N - 1);
    switch (s) {
    case MS_ENV:  return "AMP\nENV";
    case MS_ENV2: return "FILTER\nENV";
    case MS_LFO:  return "LFO 1";
    case MS_LFO2: return "LFO 2";
    case MS_MODW: return "MOD\nWHEEL";
    case MS_AT:   return "AFTERTOUCH";
    case MS_EXPR: return "EXPRESSION";
    case MS_KEY:  return "KEY\nTRACK";
    case MS_RAND: return "RANDOM";
    case MS_VEL:  return "VELOCITY";
    case MS_SLEW: return "SMOOTH\nRANDOM";   /* (owner 2026-10-25: the held random value gliding to the next one; S&H stays) */
    default:      return N_MSRC[s];
    }
}
/* the same on one line (a list row): the break is a space */
static void one_line(char *d, uint32_t n, const char *s)
{
    uint32_t i;
    for (i = 0; s[i] && i + 1u < n; i++)
        d[i] = s[i] == '\n' ? ' ' : s[i];
    d[i] = 0;
}
static void graph_mod(const track_t *t, uint16_t c)
{
    uint32_t k, base = mod_page_base(cur_page());      /* (MOD: slots 1..4, MOD 2: 5..8) */
    char line[32];
    const char *more;
    int32_t dv = morph_view(song.sel, MSLOT_ID(base + mod_ui_slot) + 1u);   /* (the DEST card's value) */
    for (k = 0; k < 4u; k++) {
        const int16_t *p = &t->p[MSLOT_ID(base + k)];
        int32_t am = morph_view(song.sel, MSLOT_ID(base + k) + 2u);   /* item 158 (as item 152 E, graph_mod_new): the amount as the fader sounds it (at B the rolled End B's), not the stored Original */
        int32_t y = 4 + (int32_t)k * 20, on = p[0] && p[1] && am && !mod_dst_why(t, p[1]), sel = k == mod_ui_slot, x;
        uint16_t bg = sel ? T_THEME : T_BG, col = sel ? T_INK : on ? c : T_DIM;
        char b[PV_LEN];
        const char *unit;
        if (sel)
            { CV_HL(6, y, 228, 17); cv_rrect(6, y, 228, 17, 4, T_THEME, T_BG); }
        b[0] = (char)('1' + base + k);
        b[1] = 0;
        GFX_HOOK_ALIGN(0, y, 0, y + 17, AL_V, "mod row text centred up/down");
        cv_text_on(14, y + CAP_IN(M, 17), &AF_M, b, sel ? T_INK : T_MID, bg);
        GFX_HOOK_ALIGN(0, y, 0, y + 17, AL_V, "mod row text centred up/down");
        {                                                /* (DUELL 1.1: the source in full; the column has 54 px: M, else S, else cut with an ellipsis) */
            char sn[24];
            one_line(sn, sizeof sn, msrc_shown(p[0]));
            if (text_w(&AF_M, sn) <= 54)
                cv_text_on(30, y + CAP_IN(M, 17), &AF_M, sn, col, bg);
            else
                cv_free_text(30, y + CAP_IN(S, 17), &AF_S, sn, col, bg, 54);
        }
        cv_line(84, y + 8, 98, y + 8, col);              /* an arrow */
        cv_line(94, y + 4, 98, y + 8, col);
        cv_line(94, y + 12, 98, y + 8, col);
        {                                                /* (T0229: a two-line name, FILTER RES, on one line in S) */
            const char *dn = mod_dst_name(t, p[1]);
            char one[40];
            one_line(one, sizeof one, dn);
            /* (owner 2026-10-25: NO short row forms; AMP ATK, MULTI CUT, TWIN CUT are gone: the row shows the full name, and what the 64 px cannot hold the drawing cuts with an ellipsis until the redesign) */
            if (text_w(&AF_M, one) <= 64)
                cv_text_on(106, y + CAP_IN(M, 17), &AF_M, one, col, bg);
            else                                         /* (DUELL 1.1: a full name the row has no 64 px for is cut with an ellipsis by the drawing, not shortened) */
                cv_free_text(106, y + CAP_IN(S, 17), &AF_S, one, col, bg, 64);
        }
        param_format(&TP[P_M1AMT], (int16_t)am, b, &unit);
        GFX_HOOK_ALIGN(0, y, 0, y + 17, AL_V, "mod row text centred up/down");
        x = cv_text_on(172, y + CAP_IN(M, 17), &AF_M, b, col, bg) + 2;
        GFX_HOOK_ALIGN(0, 0, 0, y + CAP_IN(M, 17) + AF_M.asc, AL_B, "mod row unit on its amount's baseline");
        cv_text_on(x, y + CAP_IN(M, 17) + AF_M.asc - AF_S.asc, &AF_S, unit, sel ? T_INK : T_DIM, bg);   /* (a unit: S) */
    }
    mod_dst_line(t, dv, line, &more);
    cv_free_text(14, 86 + CAP_IN(M, 17), &AF_M, line, dv && !mod_dst_why(t, dv) ? T_TEXT : T_DIM, T_BG, 212);
    if (more)
        cv_free_text(14, 105, &AF_S, more, T_MID, T_BG, 212);
}

#define PIC_NOTE16 (60 * 16)                           /* item 165: the pictures' note, middle C (pitch16, 1/16 semitone): ANALOG's SYNC / SUB, TRIO's intervals, VOICE's formants */
/* DUELL 1.1 engine pictures (ENGINE_PICTURES_V1.png): ANALOG draws two cycles of the chosen wave (item 165: LOFI's picture removed, owner; PWM / SYNC / SUB: pic_analog_at below) */
static int32_t pic_wave(uint32_t kind, uint32_t ph, uint32_t pw)       /* -1000 .. 1000 */
{
    int32_t v = (int32_t)(ph >> 22);                                   /* 0 .. 1023 */
    switch (kind) {
    case 0: return -1000 + v * 2000 / 1023;                             /* SAW (rising, then the step down, as drawn in ENGINE_PICTURES_V1.png) */
    case 1: return v < (int32_t)pw ? 1000 : -1000;                      /* SQR */
    case 2: return v < 512 ? -1000 + v * 4000 / 1023 : 3000 - v * 4000 / 1023;   /* TRI */
    default: return sine_i(ph) * 1000 / 32767;                          /* SIN */
    }
}
static void pic_draw(uint32_t kind, uint32_t pw, uint16_t c)
{
    int32_t x, py = 0, cy = graph_ht / 2, a = graph_ht * 4 / 10, first = 1;
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
    for (x = 0; x < PANEL_W; x++) {
        uint32_t ph = (uint32_t)x * (0xFFFFFFFFu / (PANEL_W / 2u));
        int32_t w = pic_wave(kind, ph, pw), y;
        y = cy - w * a / 1000;
        if (!first)
            cv_line_t(PANEL_X0 + x - 1, py, PANEL_X0 + x, y, c, 2);
        py = y;
        first = 0;
    }
}
/* item 165 (owner: the ANALOG picture true): PWM, SYNC and SUB with the engine's own maths (eng_analog.c), at the pictures' note (middle C, PIC_NOTE16) and the modulation at rest (no ENV / LFO /
 * matrix on TIMBRE: m->shape = 64 << 8, voice.c). ph: OSC 1's phase, Q32 a cycle, not wrapped (the picture: two cycles). -> -1000 .. 1000 of the saw's full scale (+-32768).
 *   PWM   OSC 1's pulse, osc_pulse at the engine's width pw = 0x80000000 + ((shape - 64 << 8) << 15): 50 % at rest (low the first half, as the engine), drawn as tall as SQR (x 2)
 *   SYNC  the engine's output of the pair (analog_render_x): OSC 1's saw and OSC 2's saw restarted where OSC 1 starts a cycle, OSC 2 at pitch_inc(note + SYNC x 3) (3/16 semitone a step),
 *         mixed by MIX (m2 = MIX x 258, m1 = 32767 - m2); the steady cycles (from OSC 1's second cycle on: before its first restart OSC 2 runs from note-on's quarter turn)
 *   SUB   the engine's output of the pair: OSC 1's saw and the square an octave below it (osc_pulse 50 %, at (DETUNE ? cents_inc(note, DETUNE) : OSC 1's increment) / 2), mixed by SUB
 *         (MIX's slot); the square starts where note-on puts it, a quarter turn of its cycle ahead of OSC 1 (analog_note_on), OSC 1 from phase 0 (a voice's first note) */
static int32_t pic_analog_at(const track_t *t, uint64_t ph)
{
    uint32_t w = (uint32_t)clamp(t->p[P_E0], 0, 6), p0 = (uint32_t)ph, i1 = pitch_inc(PIC_NOTE16);
    int32_t m2 = clamp(t->p[P_E2], 0, 127) * 258, m1 = 32767 - m2, det = clamp(t->p[P_E1], 0, 127), shape = 64 << 8, s;
    if (w == 4u)                                                        /* PWM (analog_osc case 4) */
        return osc_pulse(p0, 0u, 0x80000000u + (uint32_t)((shape - (64 << 8)) << 15)) * 2 * 1000 / 32768;
    if (w == 5u) {                                                      /* SYNC (analog_render_x, sub = 0) */
        uint32_t i2 = pitch_inc((uint32_t)clamp(PIC_NOTE16 + det * 3 + (((shape - (64 << 8)) * 3) >> 8), 0, 2047));
        uint32_t p1 = (uint32_t)((uint64_t)p0 * i2 / i1);
        s = mulq15(osc_saw(p0, 0u), m1) + mulq15((int32_t)(p1 >> 16) - 32768, m2);
    } else {                                                            /* SUB (analog_render_x, sub = 1) */
        uint32_t i2 = (det ? cents_inc(PIC_NOTE16, det, 0) : i1) >> 1;
        uint32_t p1 = 0x40000000u + (uint32_t)(ph * i2 / i1);
        s = mulq15(osc_saw(p0, 0u), m1) + mulq15(osc_pulse(p1, 0u, 0x80000000u), m2);
    }
    return s * 1000 / 32768;
}
static void graph_pic_analog(const track_t *t, uint16_t c)
{
    uint32_t w = (uint32_t)clamp(t->p[P_E0], 0, 6);                    /* SAW SQR TRI SIN PWM SYNC SUB */
    if (w < 4u) {                                                       /* SAW SQR TRI SIN: one oscillator's ideal shape (as before) */
        pic_draw(w == 1u ? 1u : w == 2u ? 2u : w == 3u ? 3u : 0u, 512u, c);
        return;
    }
    {                                                                   /* PWM SYNC SUB (item 165): the engine's maths, pic_analog_at; the same frame and line as pic_draw */
        int32_t x, py = 0, cy = graph_ht / 2, a = graph_ht * 4 / 10;
        cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
        for (x = 0; x < PANEL_W; x++) {
            int32_t y = cy - pic_analog_at(t, (uint64_t)x * (0xFFFFFFFFu / (PANEL_W / 2u))) * a / 1000;
            if (x)
                cv_line_t(PANEL_X0 + x - 1, py, PANEL_X0 + x, y, c, 2);
            py = y;
        }
    }
}
/* DUELL 1.1 (PAGES_V2_glo-a.png): the calm picture on GLO's effect pages, a smooth curve of three slow sines in the gradient (a different one for each page), a thin zero line */
static void graph_calm(uint32_t seed)
{
    int32_t x, py = 0, cy = graph_ht / 2, a = graph_ht * 3 / 10;
    cv_oy = 0;
    cv_rect(PANEL_X0, cy + 1, PANEL_W, 1, T_RAISE);
    for (x = 0; x < PANEL_W; x++) {
        uint32_t ph = (uint32_t)x * (0xFFFFFFFFu / PANEL_W);
        int32_t w = sine_i(ph * (2u + seed % 3u) + seed * 0x13579BDFu) * 5 / 8 + sine_i(ph * (5u + seed % 4u) + seed * 0x2468ACEu) / 4 + sine_i(ph * 9u + seed * 0x7654321u) / 10;
        int32_t y = cy + 1 - w * a / 32767;
        if (x)
            cv_line_t(PANEL_X0 + x - 1, py, PANEL_X0 + x, y, grad_col(PANEL_X0 + x), 2);
        py = y;
    }
}
/* ---- the other engines' pictures (ENGINE_PICTURES_V1.png / V1b.png), drawn from the engine's own numbers; only on the engine page (grid_on 5), redrawn only when the page's signature changes ---- */
/* (PHASE's wave code lives in RAM range X: the flash drawing reaches it through pointers only, tools/ram_place.py) */
static void (*volatile const pic_pd_setup)(pdw_t *, uint32_t, uint32_t) = pd_setup;
static int32_t (*volatile const pic_pd_wave)(const pdw_t *, uint32_t) = pd_wave;
static void pic_phase(const track_t *t, uint16_t c)
{
    pdw_t a, b;
    uint32_t w1 = (uint32_t)clamp(t->p[P_E0], 0, 7), w2 = (uint32_t)clamp(t->p[P_E1], 0, 8), wb = w2 ? w2 - 1u : w1, pass;
    uint32_t dcw = (uint32_t)clamp(t->p[P_E2], 0, 127) * 65535u / 127u * 56000u >> 16;
    int32_t cy = graph_ht / 2, amp = graph_ht * 4 / 10;
    uint16_t c2 = ux_mix(c, T_TEXT, UT_PIC_TINT_PCT);   /* item 165 (owner): the second cycle in a lighter tint of the part colour (was T_MID, white) */
    for (pass = 0; pass < 2u; pass++) {          /* the waves at DCW 0 dim, then bent: WAVE 1 in the part colour, WAVE 2 a lighter tint of it */
        int32_t x, py = 0;
        pic_pd_setup(&a, w1, pass ? dcw : 0u);
        pic_pd_setup(&b, wb, pass ? dcw : 0u);
        for (x = 0; x < PANEL_W; x++) {
            uint32_t ph = ((uint32_t)x * 2u * 65536u / PANEL_W) & 0xFFFFu;
            int32_t y = cy - pic_pd_wave(x < PANEL_W / 2 ? &a : &b, ph) * amp / 32768;
            if (x)
                cv_line_t(PANEL_X0 + x - 1, py, PANEL_X0 + x, y, pass ? (x < PANEL_W / 2 ? c : c2) : T_RAISE, pass ? 2 : 1);
            py = y;
        }
    }
}
static void pic_table(const track_t *t, uint16_t c)
{   /* the table's 19 frames as slices, the front one first, the hidden parts left out; the slice at POS in the part colour on top */
    const wv_mem_t *w = (const wv_mem_t *)ENG_MEM(t - trk);
    uint8_t hor[240];
    int32_t f, i, fpos, f0, fr, px = 0, py = 0, first = 1;
    if (!w || !w->h.have)
        return;
    for (i = 0; i < 240; i++)
        hor[i] = 255;
    for (f = 0; f < (int32_t)WV_F; f++) {
        int32_t base = 62 - f * 2, x0 = 12 + f * 3;
        uint16_t col = ux_mix(T_BASE, T_TEXT, 20 + (18 - f) * 2);
        for (i = 0; i < 256; i += 2) {
            int32_t x = x0 + (i * 150 >> 8), y = base - w->tab[f * (int32_t)WV_FS + i] * 15 / 128;
            if (x >= 0 && x < 240 && y >= 0 && y < (int32_t)hor[x]) {
                cv_rect(x, y, 1, 1, col);
                hor[x] = (uint8_t)y;
            }
        }
    }
    {   /* STEPS (E6): POSITION snaps into 2 .. 19 places (eng_wave.c wave_steps): the slice sits on the nearest place, the places are marked */
        int32_t n1 = clamp(t->p[P_E6], 0, 18), pos = clamp(t->p[P_E1], 0, 127), k;
        if (n1) {
            for (k = 0; k <= n1; k++)
                cv_rect(5, 62 - ((k * 127 / n1) * 18 / 127) * 2, 3, 1, T_MID);
            k = (pos * n1 + 63) / 127;
            pos = k * 127 / n1;
        }
        fpos = pos * 18 * 256 / 127;
    }
    f0 = fpos >> 8;
    fr = fpos & 255;
    for (i = 0; i < 256; i += 2) {
        int32_t a0 = w->tab[f0 * (int32_t)WV_FS + i], a1 = w->tab[(f0 < 18 ? f0 + 1 : f0) * (int32_t)WV_FS + i], v = (a0 * (256 - fr) + a1 * fr) >> 8;
        int32_t x = 12 + (fpos * 3 >> 8) + (i * 150 >> 8), y = 62 - (fpos * 2 >> 8) - v * 15 / 128;
        if (!first)
            cv_line_t(px, py, x, y, c, 2);
        px = x;
        py = y;
        first = 0;
    }
}
/* the vowel filter's response, exactly as the voice computes it (eng_formant.c): the four Klatt resonators of fres_coef at 44.1 kHz (F1 to F3 of the vowel, F4 3300 Hz; item 165: all four moved by
 * FORMANT SHIFT, the bandwidths by RESONANCE: pic_formants), |A / (1 - B z^-1 - C z^-2)| of each, in dB, summed, on a log axis 100 Hz to 5 kHz, 50 dB high;
 * VOWL 1 in the part colour with a dark fill under it, VOWL 2 light grey. The two RAM-range functions are reached through pointers. */
static void (*volatile const pic_fres)(fres_t *, uint32_t, uint32_t) = fres_coef;
static void (*volatile const pic_vowel)(int32_t, int32_t *, int32_t *) = vowel_at;
static uint32_t (*volatile const pic_fmt_ratio)(int32_t) = fmt_ratio;
/* item 165 (owner): the four resonators' F and BW (Hz x 16) of vowel position pos (0..127 << 8) as formant_render computes them at the pictures' note (middle C): FORMANT SHIFT moves all four by
 * ratio = fmt_ratio(SHIFT x 16 + key tracking), the key tracking term 0 at middle C; RESONANCE scales the bandwidths by bws = fmt_ratio((64 - RESONANCE) x 4) (x 2.52 .. x 0.40); F1 below 17/16 of the
 * note's F0 is lifted to it (the engine's rule, sung high: at middle C it acts for SHIFT down on the low-F1 vowels); the engine's clamps 60 .. 7000 Hz, 20 .. 1500 Hz. Not drawn: TALK's glide,
 * RANDOM VOWEL, the key tracking of other notes */
static void pic_formants(const track_t *t, int32_t pos, int32_t f[4], int32_t b[4])
{
    int32_t fa[3], ba[3], j;
    uint32_t ratio = pic_fmt_ratio(clamp(t->p[P_E3], -12, 12) * 16), bws = pic_fmt_ratio((64 - clamp(t->p[P_E6], 0, 127)) * 4);
    uint32_t f0 = (uint32_t)(((uint64_t)pitch_inc(PIC_NOTE16) * 705600u) >> 32);   /* F0 in Hz * 16, as formant_render */
    pic_vowel(pos, fa, ba);
    for (j = 0; j < 3; j++) {
        int32_t fj = (int32_t)(((int64_t)fa[j] * ratio) >> 16), bj = (int32_t)(((((int64_t)ba[j] * ratio) >> 16) * bws) >> 16);
        if (j == 0 && (uint32_t)fj < f0 + (f0 >> 4) && fj > 0) {
            bj = (int32_t)(((int64_t)bj * (int32_t)(f0 + (f0 >> 4))) / fj);
            fj = (int32_t)(f0 + (f0 >> 4));
        }
        f[j] = clamp(fj, 60 * 16, 7000 * 16);
        b[j] = clamp(bj, 20 * 16, 1500 * 16);
    }
    f[3] = clamp((int32_t)((3300u * 16u * (uint64_t)ratio) >> 16), 60 * 16, 7000 * 16);
    b[3] = clamp((int32_t)((((250u * 16u * (uint64_t)ratio) >> 16) * bws) >> 16), 20 * 16, 1500 * 16);
}
static int32_t pic_log2q8(uint64_t v)     /* log2 of v (> 0), Q8: the leading bit, then the next 16 bits f with log2(1 + f) ~ f + f (1 - f) (0.4209 - 0.1564 f) (item 165: within 0.0011 of a bit; the plain f
                                           * was up to 0.086 of a bit off) */
{
    uint32_t n = 0, f;
    uint64_t t = v;
    while (t >>= 1)
        n++;
    f = (uint32_t)(n >= 16u ? (v >> (n - 16u)) & 0xFFFFu : (v << (16u - n)) & 0xFFFFu);
    f = (uint32_t)((int64_t)f + ((((int64_t)f * (65536 - (int64_t)f)) >> 16) * (27582 - ((10249 * (int64_t)f) >> 16)) >> 16));
    return (int32_t)(n << 8) + (int32_t)((f + 128u) >> 8);
}
/* item 165: cos and sin of x (radians Q30, 0 .. 0.8) in Q30 by their Taylor series. The Q15 sine table was too coarse here: near a sharp formant and at low frequencies 1 - B cos w - C cos 2w is a small
 * difference of large terms, and the table's 1/32768 left the drawn curve up to 1.2 dB (RESONANCE 0) .. 3.5 dB (127) off the engine's filter; now within 0.06 dB (tests/ui_test.c test_item165) */
static void pic_cs30(int64_t x, int64_t *c, int64_t *s)
{   /* Horner: cos = 1 - x^2/(1.2) (1 - x^2/(3.4) (1 - ..)), sin = x (1 - x^2/(2.3) (1 - x^2/(4.5) (1 - ..))), 8 terms each; multiplies only (2^30 / n as constants) */
    static const int32_t RC[8] = {536870912, 89478485, 35791394, 19173961, 11930465, 8134408, 5899680, 4473924};   /* 2^30 / ((2k - 1) 2k) */
    static const int32_t RS[8] = {178956971, 53687091, 25565282, 14913081, 9761289, 6882960, 5113056, 3947580};    /* 2^30 / (2k (2k + 1)) */
    int64_t x2 = (x * x) >> 30, tc = 1LL << 30, ts = 1LL << 30;
    int32_t k;
    for (k = 7; k >= 0; k--) {
        tc = (1LL << 30) - ((((x2 * tc) >> 30) * RC[k]) >> 30);
        ts = (1LL << 30) - ((((x2 * ts) >> 30) * RS[k]) >> 30);
    }
    *c = tc;
    *s = (x * ts) >> 30;
}
static int32_t pic_klatt_db(const fres_t *r, uint32_t hz)    /* dB x 100 of one resonator at hz: |A / (1 - B e^-jw - C e^-2jw)| */
{
    int64_t c1, s1, c2, s2, re, im;
    uint64_t d2, a2;
    pic_cs30((int64_t)(((uint64_t)hz * 10025847154ull) >> 16), &c1, &s1);   /* w = 2 pi hz / 44100, Q30 (2 pi / 44100 x 2^46) */
    c2 = ((2 * c1 * c1) >> 30) - (1LL << 30);                     /* cos 2w, sin 2w */
    s2 = (2 * s1 * c1) >> 30;
    re = (1LL << 30) - (((int64_t)r->b * c1) >> 30) - (((int64_t)r->c * c2) >> 30);
    im = (((int64_t)r->b * s1) >> 30) + (((int64_t)r->c * s2) >> 30);
    re >>= 2;
    im >>= 2;
    d2 = (uint64_t)(re * re) + (uint64_t)(im * im);
    a2 = (uint64_t)(((int64_t)r->a >> 2) * ((int64_t)r->a >> 2));
    if (d2 < 1u)
        d2 = 1u;
    if (a2 < 1u)
        a2 = 1u;
    return (int32_t)(((int64_t)(pic_log2q8(a2) - pic_log2q8(d2)) * 301) >> 8);
}
static void pic_voice(const track_t *t, uint16_t c)
{
    static const uint16_t FK[17] = {100, 128, 163, 208, 266, 340, 434, 554, 707, 903, 1153, 1472, 1880, 2400, 3065, 3914, 5000};   /* 100 * 50^(k/16) */
    uint32_t vw;
    int32_t top = 4, hgt = graph_ht - 10;
    cv_rect(PANEL_X0, top + hgt + 2, PANEL_W, 1, T_RAISE);
    for (vw = 0; vw < 2u; vw++) {
        int32_t fa[4], ba[4], x, py = 0;
        fres_t r[4];
        uint32_t k;
        pic_formants(t, clamp(t->p[vw ? P_E1 : P_E0], 0, 127) << 8, fa, ba);   /* (item 165: SHIFT and RESONANCE as the engine) */
        for (k = 0; k < 4u; k++)
            pic_fres(&r[k], (uint32_t)fa[k], (uint32_t)ba[k]);
        CV_HL(PANEL_X0, top, PANEL_W, hgt + 1);   /* (data: the formant curve's area) */
        for (x = 0; x < PANEL_W; x++) {
            int32_t kk = x * 16 / PANEL_W, fq = FK[kk] + (FK[kk + 1] - FK[kk]) * (x * 16 - kk * PANEL_W) / PANEL_W, db = 0, y;
            for (k = 0; k < 4u; k++)
                db += pic_klatt_db(&r[k], (uint32_t)fq);
            db = db < -1000 ? -1000 : db > 4000 ? 4000 : db;       /* 50 dB: from -10 to +40 */
            y = top + (4000 - db) * hgt / 5000;
            if (!vw)
                cv_rect(PANEL_X0 + x, y, 1, top + hgt - y + 1, ux_mix(T_BASE, c, 22));   /* the dark fill under VOWL 1 */
            if (x)
                cv_line_t(PANEL_X0 + x - 1, py, PANEL_X0 + x, y, vw ? T_TEXT : c, 2);
            py = y;
        }
    }
}
static void pic_lines(int32_t x, int32_t up, int32_t down, uint16_t cu, uint16_t cd)
{
    int32_t cy = graph_ht / 2;
    if (up > 0)
        cv_rect(x, cy - up, 2, up, cu);
    if (down > 0)
        cv_rect(x, cy, 2, down, cd);
}
static void pic_phys(const track_t *t, uint16_t c)
{   /* one line a mode at its frequency ratio (up: how hard the strike excites it, down: how long it rings); STRING and SYMP: the harmonics of the string, SYMP with the sympathetic strings below */
    static const uint16_t MEMB[10] = {100, 159, 214, 230, 265, 292, 316, 342, 360, 400};   /* the membrane's ratios x 100 (the unloaded head) */
    uint32_t model = (uint32_t)clamp(t->p[P_E0], 0, 3);
    int32_t strc = clamp(t->p[P_E1], 0, 127), brit = clamp(t->p[P_E2], 0, 127), damp = clamp(t->p[P_E3], 0, 127), pos = clamp(t->p[P_E4], 0, 127), n, cy = graph_ht / 2;
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
    for (n = 1; n <= 13; n++) {
        int32_t ratio100 = model == 0u ? n * 100 + (n * n * strc) / 6 : model == 2u ? (n <= 10 ? MEMB[n - 1] : 400 + 30 * (n - 10)) : n * 100;
        int32_t x = PANEL_X0 + ratio100 * (PANEL_W - 14) / (model == 2u ? 600 : 1500 + strc * 4), amp, ring;
        int32_t ph = (pos * n * 2) & 255, s = ph < 128 ? ph : 255 - ph;     /* |sin| of the strike place, in 0..128 */
        amp = (model == 1u || model == 3u) ? (s * 30 / 128) * (150 - n * 8) / 150 : (s * 30 / 128) * (130 - n * 6) / 130;
        ring = (30 * (64 + brit) / 200) * (200 - damp) / 200 - n * 2;
        if (x < PANEL_X0 + PANEL_W - 2)
            pic_lines(x, amp > 1 ? amp : 1, model == 3u ? 0 : ring > 1 ? ring : 1, c, T_DIM);
    }
    if (model == 3u) {                                                       /* SYMP: the three sympathetic strings (CHRD) as grey lines below */
        static const uint8_t CH[3] = {100, 150, 200};                         /* octave, fifth, two octaves: ratio x 100 / 1 */
        int32_t k;
        for (k = 0; k < 3; k++)
            cv_rect(PANEL_X0 + CH[k] * (PANEL_W - 14) / (1500 + strc * 4), cy + 2, 2, 22, T_MID);
    }
}
static int32_t pic_trio_osc(uint32_t shape, uint32_t ph, int32_t pw)       /* -1000 .. 1000: 0 SAW 1 PULSE 2 TRI 3 NOISE */
{
    int32_t v = (int32_t)(ph >> 22);
    switch (shape) {
    case 1: return v < pw ? 1000 : -1000;
    case 2: return v < 512 ? -1000 + v * 4000 / 1023 : 3000 - v * 4000 / 1023;
    case 3: return (int32_t)((ph >> 24) * 2654435761u >> 22 & 1023u) * 2000 / 1023 - 1000;
    default: return -1000 + v * 2000 / 1023;
    }
}
/* item 165 (owner): osc k's phase step a pixel: OSC 1 makes PIC_TRIO_CYC cycles over the width, OSC 2 / 3 their real interval above or below it, the ratio of the engine's own increments
 * (eng_trio.c trio_inc = cents_inc of the pitch + INT x 16, DETUNE not drawn) at the picture's note, middle C: +7 st 1.4983, -12 st 0.5000, not rounded to whole cycles */
#define PIC_TRIO_CYC 3u
static uint32_t pic_trio_dph(int32_t semi)
{
    uint64_t i1 = pitch_inc(PIC_NOTE16), ik = cents_inc(PIC_NOTE16 + semi * 16, 0, 0);
    return (uint32_t)((((uint64_t)PIC_TRIO_CYC << 32) / PANEL_W) * ik / i1);
}
static void pic_trio(const track_t *t, uint16_t c)
{   /* three lanes, one a oscillator, the same stretch of time: INT2 and INT3 make the intervals visible as the real ratio of the cycles */
    uint32_t wv = (uint32_t)clamp(t->p[P_E0], 0, 15), sh[3] = {0, 0, 0}, k;
    const char *nm = N_TRIO_WAVE[wv];
    int32_t semi[3] = {0, clamp(t->p[P_E1], -24, 24), clamp(t->p[P_E2], -24, 24)}, pw = 128 + clamp(t->p[P_E7], 0, 127) * 3, lane = graph_ht / 3;
    {   /* the shapes from the set's name: its words in order, an XxWORD set takes the word for all three */
        uint32_t found = 0, i;
        uint32_t seq[3] = {0, 0, 0};
        for (i = 0; nm[i] && found < 3u; i++) {
            if (!memcmp(nm + i, "SAW", 3)) { seq[found++] = 0; i += 2; }
            else if (!memcmp(nm + i, "PULSE", 5)) { seq[found++] = 1; i += 4; }
            else if (!memcmp(nm + i, "TRI", 3)) { seq[found++] = 2; i += 2; }
            else if (!memcmp(nm + i, "NOISE", 5)) { seq[found++] = 3; i += 4; }
        }
        for (k = 0; k < 3u; k++)
            sh[k] = found ? seq[nm[0] == '3' || found == 1u ? 0 : k < found ? k : found - 1u] : 0u;
        if (nm[0] == '2' && found >= 2u)
            sh[2] = seq[1];
    }
    for (k = 0; k < 3u; k++) {
        int32_t x, py = 0, cy = (int32_t)k * lane + lane / 2;
        uint32_t dph = pic_trio_dph(semi[k]);                                  /* (item 165: the real ratio, 3 cycles at the note) */
        for (x = 0; x < PANEL_W; x++) {
            uint32_t ph = (uint32_t)x * dph;
            int32_t y = cy - pic_trio_osc(sh[k], ph, pw) * (lane * 4 / 10) / 1000;
            if (x)
                cv_line_t(PANEL_X0 + x - 1, py, PANEL_X0 + x, y, c, 1);
            py = y;
        }
    }
}
/* DUELL 1.1 MATRIX (PAGES_V1_matrix.png, approved): a legend of the four knobs, then four rows of 37 px: the slot's number, the source and the destination's group in small capitals, the destination's name large, the amount at the right;
 * the picked slot on the theme fill. A destination the engine does not use says why where its group would be (nothing is silently ignored). Two strips of 86 rows (the canvas holds 124) */
/* change list 2026-10-28 section D (owner, all decided): the destinations' categories, each once along the DESTINATION knob (params.c MDST_ORDER keeps them together). Index: the stored destination id
 * (mod.c MD_*, append-only, never moved); entry: the category in MDST_GRPNAME. SHARED FX and SENDS are gone: each send and each effect value is under its own effect (CHORUS, DELAY, REVERB); the INSERT and the
 * DUCK are their own categories; PART FX keeps SATURATION (and the retired EQ gains 28 .. 30, never offered); SPACE is its own category with the item SPACE; LFO 1 PITCH DEPTH (11) is LFO 1 DEPTH under PITCH */
static const uint8_t MDST_GRP[57] = {
    0, 1, 2, 4, 3, 3, 10, 11, 12, 13,   /*  0 OFF, PITCH, MULTI CUTOFF, TIMBRE, AMP LEVEL, PAN, SATURATION, CHORUS SEND, DELAY SEND, REVERB SEND */
    7, 1,                               /* 10 LFO 1 RATE, 11 LFO 1 DEPTH (the pitch's) */
    4, 4, 4, 4, 4, 4, 4, 4,             /* 12 .. 19 the engine's own eight slots */
    7, 7, 7, 3,                         /* 20 LFO 1 DEPTH (retired master depth), LFO 2 RATE, 22 LFO 2 DEPTH (retired), 23 PATCH LEVEL */
    8, 8, 8, 8,                         /* 24 .. 26 the INSERT's three values (its type's labels), 27 MIX */
    10, 10, 10, 9,                      /* 28 .. 30 the EQ gains (removed, never offered), 31 DUCK DEPTH */
    6, 6, 6, 6, 6, 6,                   /* 32 .. 37 FILTER ENV ATTACK, DECAY, SUSTAIN, RELEASE, MULTI CUTOFF, TIMBRE (retired) */
    2,                                  /* 38 MULTI RES */
    11, 11, 12, 12, 12, 13, 13,         /* 39 CHORUS RATE, 40 CHORUS DEPTH, 41 DELAY FEEDBACK, 42 DELAY MOD DEPTH (CHANCE), 43 DELAY MOD RATE, 44 REVERB SIZE (removed), 45 REVERB DAMPING */
    2,                                  /* 46 MULTI MODE */
    5, 5, 5, 5,                         /* 47 .. 50 AMP ENV ATTACK, DECAY, SUSTAIN, RELEASE */
    2, 2, 2, 2,                         /* 51 TWIN CUTOFF, 52 TWIN RES, 53 FILTER DRIVE, 54 FILTER BLEND */
    4, 14};                             /* 55 SUB LEVEL, 56 SPACE */
static const char *const MDST_GRPNAME[15] = {"", "PITCH", "FILTER", "AMP", "ENGINE", "AMP ENV", "FILTER ENV", "LFO", "INSERT", "DUCK", "PART FX", "CHORUS", "DELAY", "REVERB", "SPACE"};
/* rule B1 (owner): the categories whose word is NOT repeated in the item under it: the row says AMP ENV over ATTACK, CHORUS over SEND, DELAY over MOD DEPTH, INSERT over MIX, DUCK over DEPTH.
 * The other categories keep their items' own names as the owner decided them (PITCH, AMP LEVEL, FILTER DRIVE, FILTER BLEND, LFO 1 RATE, SPACE: change list section H 2 .. 4) */
static const uint8_t MDST_GRP_STRIP[15] = {0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0};
/* item 119: the rows at the floor (S, M 15, L 23): the legend of the four knobs in two rows of two (one row of four needs 275 px of the 232), then four rows 38 px high, 40 px apart: the slot's number at the left, over the
 * big line the source and the destination's group in small capitals (S, a gap between them, no rule), the destination's name (L, else M, else S: whole, never cut) and the amount (L) on ONE baseline under them.
 * Two strips (the canvas holds 124 rows): screen rows 28 .. 133 and 134 .. 217 */
#define MX_LEG_Y 28                                    /* the legend's first row (the dots' top); the second 14 px under it */
#define MX_Y0 56                                       /* the first row's top */
#define MX_PITCH 40
#define MX_H 38
#define MX_SPLIT 134
static void graph_mod_new(const track_t *t)
{
    uint32_t base = mod_page_base(cur_page()), k, band;
    for (band = 0; band < 2u; band++) {
        const int32_t y0 = band ? MX_SPLIT : MX_LEG_Y;
        cv_begin(240, band ? 218u - MX_SPLIT : MX_SPLIT - MX_LEG_Y, T_BG);
        cv_oy = -y0;
        if (!band) {   /* the legend: the four knobs, K1 K2 over K3 K4 (the grid of the cards' own knobs) */
            static const char *const LG[4] = {"SLOT", "SOURCE", "DESTINATION", "AMOUNT"};
            for (k = 0; k < 4u; k++) {
                char d[2] = {(char)('1' + k), 0};
                int32_t lx = k & 1u ? 124 : 9, ly = MX_LEG_Y + (int32_t)(k >> 1) * 14;
                cv_rrect(lx, ly, 11, 11, 5, T_TEXT, T_BG);
                cv_text_in(lx, ly + HALF_UP(11 - AF_D_CAP_H) - AF_D_CAP_Y, 11, &AF_DOT, d, T_BG, T_TEXT);
                cv_text(lx + 15, UT_TY(S, ly + 10), &AF_S, LG[k], T_TEXT);
            }
        }
        for (k = 0; k < 4u; k++) {
            const int32_t y = MX_Y0 + (int32_t)k * MX_PITCH;
            const int16_t *tp = &t->p[MSLOT_ID(base + k)];
            const int16_t p[3] = {tp[0], tp[1], (int16_t)morph_view(song.sel, MSLOT_ID(base + k) + 2u)};   /* item 152 E: the amount as the fader sounds it (the AMT card shows that too): at B the rolled End B's, not the Original's */
            int32_t sel = k == mod_ui_slot, on = p[0] && p[1] && p[2] && !mod_dst_why(t, p[1]);
            uint16_t bg = sel ? T_THEME : T_BG, big = sel ? T_INK : on ? T_TEXT : T_DIM, sm = sel ? T_INK : T_MID, amc = sel ? T_INK : on ? T_THEME : T_DIM;
            char b[PV_LEN], sn[24], dn[40], line[32], num[2] = {(char)('1' + base + k), 0}, am[16];
            const char *unit, *more = 0, *grp;
            int32_t x, aw, room;
            if (y + MX_H <= y0 || y >= y0 + (int32_t)cv_h)
                continue;
            if (sel) CV_HL(3, y, 234, MX_H);
            cv_rrect(3, y, 234, MX_H, 4, bg, T_BG);
            one_line(sn, sizeof sn, msrc_shown(p[0]));
            one_line(dn, sizeof dn, mod_dst_name(t, p[1]));
            mod_dst_line(t, p[1], line, &more);
            grp = mod_dst_why(t, p[1]) ? mod_dst_why(t, p[1]) : MDST_GRPNAME[p[1] >= 0 && p[1] < 57 ? MDST_GRP[p[1]] : 0];
            {   /* rule B1 (HANDOFF item 95, change list section D): where the row's small line says the category (AMP ENV, FILTER ENV, INSERT, DUCK, CHORUS, DELAY, REVERB), the name is the item alone
                 * (ATTACK, MIX, DEPTH, SEND, MOD DEPTH). Where the small line says a reason instead (INS OFF, OWN ENV ...), the category is not on the row, so the name keeps it (INSERT MIX) */
                uint32_t gi = p[1] >= 0 && p[1] < 57 ? MDST_GRP[p[1]] : 0u, gl = str_len(MDST_GRPNAME[gi]);
                if (MDST_GRP_STRIP[gi] && grp == MDST_GRPNAME[gi] && !memcmp(dn, MDST_GRPNAME[gi], gl) && dn[gl] == ' ')
                    str_cpy(dn, dn + gl + 1u, sizeof dn);
            }
            param_format(&TP[P_M1AMT], p[2], b, &unit);
            str_cpy(am, b, sizeof am);
            /* change list A.2 and D (owner): the amount's number in L (Bold 23, the name's big face), the % right after it with no space, in M (Medium 15: thinner and smaller), white, on the number's
             * baseline (the capitals' last row y + 34, UT_TY). 12 px for the % instead of 18 + a 5 px gap: the name has 11 px more room (130 px at +100 %, 132 at -100 %, 143 at two digits) */
            aw = text_w(&AF_L, am) + (unit[0] ? text_w(&AF_M, unit) + UT_A_UNIT_GAP : 0);
            cv_text_on(12, y + HALF_UP(MX_H - AF_L_CAP_H) - AF_L_CAP_Y, &AF_L, num, sel ? T_INK : T_THEME, bg);
            cv_text_on(34, y, &AF_S, sn, sm, bg);   /* (the source: whole, S 15 over the big line; the group follows after a gap, no rule) */
            if (grp[0])
                cv_text_on(34 + text_w(&AF_S, sn) + 12, y, &AF_S, grp, sm, bg);
            x = 232 - aw;
            cv_text_on(x, y + 12, &AF_L, am, amc, bg);
            if (unit[0])
                cv_text_on(x + text_w(&AF_L, am) + UT_A_UNIT_GAP, UT_TY(M, y + 12 + AF_L_CAP_Y + AF_L_CAP_H - 1), &AF_M, unit, sel ? T_INK : on ? T_TEXT : T_DIM, bg);   /* (white; the picked row's ink on its fill, a row that does nothing dim, as its number) */
            room = x - 6 - 34;
            if (text_w(&AF_L, dn) <= room)
                cv_text_on(34, y + 12, &AF_L, dn, big, bg);
            else if (text_w(&AF_T, dn) <= room)               /* item 152 D: the smaller steps are the same Bold face as L (T: Bold 15, capitals; M and S are Medium / SemiBold: they looked thin next to the big names) */
                cv_text_on(34, y + 20, &AF_T, dn, big, bg);
            else
                cv_free_text(34, y + 20, &AF_T, dn, big, bg, room);   /* (the smallest step, still the whole word) */
            (void)more;
        }
        cv_oy = 0;
        cv_blit(0, (uint32_t)y0);
    }
}

static uint32_t steps_hash(const track_t *t)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < NSTEP; i++) {
        const step_t *st = &seq_steps(t)[i];
        h = (h ^ (st->note[0] + st->n * 128u + st->time * 1024u + st->flags * 4096u + st->note[1] * 65536u)) *
            16777619u;
        h = (h ^ ((uint32_t)st->note[2] << 16 | (uint32_t)st->note[3] << 24)) * 16777619u;
    }
    return h;
}

static uint32_t str_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

/* WHEEL: the nine drawbars as rounded bars over RAISE slots (the bars of the knob just turned: the accent),
 * their footages under them */
static void graph_wheel(const track_t *t, uint16_t c)
{
    uint32_t k;
    static const char *const names[9] = {"16", "5.3", "8", "4", "2.7", "2", "1.6", "1.3", "1"};
    for (k = 0; k < 9u; k++) {
        int32_t x = 11 + (int32_t)k * 25, level = drw_level(t->p, k);
        if (t->p[P_E4] && k == 8u) level = 0; /* DSP's percussion cancels the 1-foot bar. */
        uint32_t he = ui.hot_t && knob_slot(cur_page(), ui.hot_col) < 4u ? pg_id(cur_page(), knob_slot(cur_page(), ui.hot_col)) : 0u;   /* (T0227: the card turned, whichever page) */
        int hot = he >= P_E0 && he <= P_E3 && (he == P_E0 || (k >= 2u && he - P_E0 == (k < 4u ? 2u : 3u)));   /* (the sub bars: REG's alone) */
        cv_rrect(x + 7, 2, 4, 54, 2, T_RAISE, T_BG);   /* (item 119, as the ENGINE page's bars: the 76 px card, the bars end on row 56, the footages' capitals on rows 63 .. 73) */
        if (level) { CV_HL(x + 3, 56 - level * 6, 12, level * 6); cv_rrect(x + 3, 56 - level * 6, 12, level * 6, 3, hot ? T_ACCENT : c, T_BG); }
        else cv_rrect(x + 3, 54, 12, 2, 1, hot ? T_ACCENT : T_DIM, T_BG);
        GFX_HOOK_ALIGN(x + 7, 0, x + 11, 0, AL_H, "wheel footage under its drawbar");
        cv_text_in(x - 3, 59, 24, &AF_S, names[k], T_MID, T_BG);   /* under the bar, by its ink */
    }
}
/* The FM charts' parts: an operator box 21 x 17 (rows 23 px apart, columns 24), junction dots, Manhattan routes */
#define FM_BH 17
#define FM_BH2 13                                    /* item 119, the 76 px card: box height (the digit's capitals are 11), row pitch, column pitch (boxes 3 px apart), box width, the chart's centre; its 74 rows 1 .. 74: the loop's 4, the boxes, a 3 px stem, the bus and the arrow's 2 */
#define FM_RP2 17
#define FM_CP2 22
#define FM_BW2 19
#define FM_XC 162
static void fm_dot(int32_t x, int32_t y, uint16_t c) { if (grid_on) cv_rect(x - 1, y, 3, 2, c); else cv_rect(x - 1, y - 1, 3, 3, c); }   /* (the card's rows are 17 apart: a junction dot 3 x 2 on and under its jog row keeps a clear row to the boxes) */
/* a Manhattan route, 1 px: down from (x0, y0) to the jog row jy, across to x1, down to y1 */
static void fm_route(int32_t x0, int32_t y0, int32_t x1, int32_t jy, int32_t y1, uint16_t c)
{
    cv_rect(x0, y0, 1, jy - y0, c);
    cv_rect(x0 < x1 ? x0 : x1, jy, (x0 < x1 ? x1 - x0 : x0 - x1) + 1, 1, c);
    cv_rect(x1, jy, 1, y1 - jy, c);
}
/* a feedback loop on the operator box at (x, y): out of its right side, up, back in on top with an arrow (r: the box's half width + 1: 11 on the 21 px box, 10 on the 19 px one; n: the length of the
 * way out, 5 on the old panel, 2 on the card, where the boxes are 3 px apart) */
static void fm_loop(int32_t x, int32_t y, uint16_t c, int32_t r, int32_t n)
{
    cv_rect(x + r, y + 5, n, 1, c);
    cv_rect(x + r + n - 1, y - 4, 1, 9, c);
    cv_rect(x, y - 4, r + n - 1, 1, c);
    cv_rect(x, y - 4, 1, 4, c);
    cv_line(x - 2, y - 3, x - 1, y - 2, c); cv_line(x + 2, y - 3, x + 1, y - 2, c);
}
/* the output bus under the carriers (the leftmost at cl, the rightmost at cr) at y bus, an arrow out on the right (e: how far past the rightmost: 24 on the old panel, 14 on the card; ah: the arrow head's half height, 3 / 2) */
static void fm_bus(int32_t cl, int32_t cr, int32_t bus, int32_t e, int32_t ah)
{
    cv_rect(cl, bus, cr + e - cl, 1, T_MID);
    cv_line(cr + e - ah - 1, bus - ah, cr + e - 1, bus, T_MID); cv_line(cr + e - ah - 1, bus + ah, cr + e - 1, bus, T_MID);
}

#ifndef FM6_CHART_HOOK
#define FM6_CHART_HOOK(kind, a, b) ((void)0)   /* ui_render.c (host tests): the chart's parts one by one (its lint) */
#endif
enum { FMH_BOX, FMH_ROUTE, FMH_CAR, FMH_BUS, FMH_FB, FMH_LABEL, FMH_END };
/* FM6's 32 algorithms as charts. FM6_CELL: per operator 1..6 its cell, the column in 24 px steps (bits 0..2) and
 * the row above the output bus (bits 4..5; row 0 = the carriers). The routes, the carriers and the feedback
 * operator are fm6_core.c's FM6_ALG itself (fm6_routes, fm6_car_ops, fm6_fb_op). ui_test.c checks the cells
 * against them (a modulator one row above what it modulates, the carriers on the bottom row and only they, one
 * operator per cell) and the routes against the 32 algorithms written out; ui_render.c lints every chart as drawn
 * (no route through a box or touching another route, no two boxes overlapping, all inside the panel). The cells:
 * a search for the fewest and shortest bends, the operators numbered left to right. */
static const uint8_t FM6_CELL[32][6] = {
    {0x00, 0x10, 0x01, 0x11, 0x21, 0x31}, {0x00, 0x10, 0x02, 0x12, 0x22, 0x32}, {0x00, 0x10, 0x20, 0x01, 0x11, 0x21},
    {0x00, 0x10, 0x20, 0x01, 0x11, 0x21}, {0x00, 0x10, 0x01, 0x11, 0x02, 0x12}, {0x00, 0x10, 0x01, 0x11, 0x02, 0x12},
    {0x00, 0x10, 0x01, 0x11, 0x12, 0x22}, {0x00, 0x10, 0x01, 0x12, 0x11, 0x21}, {0x00, 0x10, 0x02, 0x12, 0x13, 0x23},
    {0x00, 0x10, 0x20, 0x01, 0x11, 0x12}, {0x00, 0x10, 0x20, 0x01, 0x11, 0x12}, {0x00, 0x10, 0x03, 0x12, 0x13, 0x14},
    {0x00, 0x10, 0x02, 0x11, 0x12, 0x13}, {0x00, 0x10, 0x01, 0x11, 0x20, 0x21}, {0x00, 0x10, 0x02, 0x12, 0x21, 0x22},
    {0x01, 0x10, 0x11, 0x21, 0x12, 0x22}, {0x01, 0x12, 0x10, 0x20, 0x11, 0x21}, {0x01, 0x10, 0x12, 0x11, 0x21, 0x31},
    {0x00, 0x10, 0x20, 0x01, 0x02, 0x11}, {0x00, 0x01, 0x10, 0x02, 0x12, 0x13}, {0x00, 0x01, 0x10, 0x02, 0x03, 0x12},
    {0x00, 0x10, 0x01, 0x02, 0x03, 0x12}, {0x00, 0x01, 0x11, 0x02, 0x03, 0x12}, {0x00, 0x01, 0x02, 0x03, 0x04, 0x13},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x13}, {0x00, 0x01, 0x11, 0x02, 0x12, 0x13}, {0x00, 0x01, 0x11, 0x03, 0x13, 0x14},
    {0x00, 0x10, 0x01, 0x11, 0x21, 0x02}, {0x00, 0x01, 0x02, 0x12, 0x03, 0x13}, {0x00, 0x01, 0x02, 0x12, 0x22, 0x03},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x14}, {0x00, 0x01, 0x02, 0x03, 0x04, 0x05}};
/* algorithm a's routes: m[i] bit j: operator j + 1 modulates operator i + 1. FM6_ALG runs the operators sixth
 * first, each reading a bus (or none) and writing one (or adding to it): who wrote a bus modulates its reader */
static void fm6_routes(uint32_t a, uint8_t *m)
{
    const uint8_t *f = FM6_ALG[a & 31u];
    uint8_t bus[4] = {0, 0, 0, 0};
    uint32_t k;
    for (k = 0; k < 6u; k++) {
        uint32_t op = 5u - k, in = (f[k] >> 4) & 3u, o = f[k] & 3u;
        m[op] = in ? bus[in] : 0u;
        bus[o] = (uint8_t)(((f[k] & FM6_OADD) ? bus[o] : 0u) | 1u << op);
    }
}
/* the operator with feedback in algorithm a (0..5: operator 1..6) */
static uint32_t fm6_fb_op(uint32_t a)
{
    uint32_t k;
    for (k = 0; k < 5u && (FM6_ALG[a & 31u][5u - k] & 0xC0u) != 0xC0u; k++)
        ;
    return k;
}
/* the carriers of algorithm a: bit k operator k + 1 (fm6_carriers counts the sixth first) */
static uint32_t fm6_car_ops(uint32_t a)
{
    uint32_t c = fm6_carriers(a), r = 0, k;
    for (k = 0; k < 6u; k++)
        r |= (c >> (5u - k) & 1u) << k;
    return r;
}
/* the algorithm FM6 plays on track t (0..31): ALG, or the patch's at PAT */
static uint32_t fm6_alg_of(const track_t *t)
{
    return t->p[P_E0] >= 1 && t->p[P_E0] <= 32 ? (uint32_t)t->p[P_E0] - 1u : fm6_patch[(t - trk) % NTRK][FP_ALG] & 31u;
}
/* FM6's EDIT pages: the algorithm, "ALG 05" top left; SLOT on a bank voice (B1..B32,
 * 1.4.1): its name top right. Carriers THEME with INK
 * numerals, modulators RAISE with THEME ones; an operator at output level 0: a DIM numeral (a carrier on RAISE).
 * Routes THEME, the feedback loop MID (DIM at feedback 0), the output bus MID. The knob just turned in ACCENT:
 * ALG and PTCH the label, FB the loop, MLVL the routes, MRAT MEG VMOD the modulators, DTUN the carriers.
 * sel < 6 (the OPERATOR pages, ui_fm6op.c): that operator's box ACCENT with an INK numeral, its frequency top right */
static void graph_fm6(const track_t *t, uint16_t c, uint32_t sel)
{
    uint32_t alg = fm6_alg_of(t), k, j, car = fm6_car_ops(alg), top = 0, hi = 0, fbop = fm6_fb_op(alg), hotset = 0;
    uint32_t hot = ui.hot_t && knob_slot(cur_page(), ui.hot_col) < 4u ? pg_id(cur_page(), knob_slot(cur_page(), ui.hot_col)) : 0u;
    const uint8_t *pt = fm6_patch[(t - trk) % NTRK];
    int32_t x[6], y[6], bus, cl = 240, cr = 0, fb = clamp(pt[FP_FB] + t->p[P_E1], 0, 7);
    uint8_t m[6], fan[6] = {0, 0, 0, 0, 0, 0};
    uint16_t rc = hot == P_E2 ? T_ACCENT : c;
    char b[7] = {'A', 'L', 'G', ' ', (char)('0' + (alg + 1u) / 10u), (char)('0' + (alg + 1u) % 10u), 0};
    if (hot == P_E3 || hot == P_E4 || hot == P_E5)
        hotset = 63u & ~car;
    else if (hot == P_E6)
        hotset = car;
    fm6_routes(alg, m);
    for (k = 0; k < 6u; k++) {
        uint32_t col = FM6_CELL[alg][k] & 7u, row = FM6_CELL[alg][k] >> 4;
        hi = col > hi ? col : hi;
        top = row > top ? row : top;
        for (j = 0; j < 6u; j++)
            fan[j] += (uint8_t)(m[k] >> j & 1u);
    }
    if (grid_on) {   /* item 119: the 76 px card. Boxes 19 x 13 (the digit's capitals are 11), 22 px apart across, 17 down; the chart in the right part of the card (its centre on x FM_XC), the algorithm and the voice name / the operator's frequency on two lines at the left */
        const int32_t tot = 4 + FM_BH2 + FM_RP2 * (int32_t)top + 3 + 3, yt = 4 + (74 - tot) / 2;   /* (the canvas row of the loop = yt - 4 + GOY; 4 + box + rows + stem + bus and the arrow's 2 under it) */
        bus = yt + FM_BH2 + FM_RP2 * (int32_t)top + 3;
        for (k = 0; k < 6u; k++) {
            x[k] = FM_XC + (int32_t)(FM6_CELL[alg][k] & 7u) * FM_CP2 - (int32_t)hi * FM_CP2 / 2;
            y[k] = yt + FM_RP2 * ((int32_t)top - (int32_t)(FM6_CELL[alg][k] >> 4));
            if (car >> k & 1u) { cl = x[k] < cl ? x[k] : cl; cr = x[k] > cr ? x[k] : cr; }
        }
    } else {
    bus = 50 + (27 + 23 * (int32_t)top) / 2;         /* the chart centred on the 100 px scale */
    for (k = 0; k < 6u; k++) {
        x[k] = 120 + (int32_t)(FM6_CELL[alg][k] & 7u) * 24 - (int32_t)hi * 12;
        y[k] = bus - 6 - FM_BH - 23 * (int32_t)(FM6_CELL[alg][k] >> 4);
        if (car >> k & 1u) { cl = x[k] < cl ? x[k] : cl; cr = x[k] > cr ? x[k] : cr; }
    }
    }
    {
    const int32_t bh = grid_on ? FM_BH2 : FM_BH, bw = grid_on ? FM_BW2 : 21, hw = bw / 2;
    for (k = 0; k < 6u; k++) {                       /* into k: its sources down to its jog row, across, down */
        for (j = 0; j < 6u; j++)
            if (m[k] >> j & 1u) {
                FM6_CHART_HOOK(FMH_ROUTE, j, k);
                fm_route(x[j], y[j] + bh, x[k], y[k] - 3, y[k], rc);
                if (fan[j] > 1u) fm_dot(x[j], y[k] - 3, rc);          /* one operator modulating several */
                if (m[k] & (m[k] - 1u)) fm_dot(x[k], y[k] - 3, rc);   /* several modulating one */
            }
        if (car >> k & 1u) {
            FM6_CHART_HOOK(FMH_CAR, k, 0);
            cv_rect(x[k], y[k] + bh, 1, bus - y[k] - bh, T_MID);
            if (x[k] != cl) fm_dot(x[k], bus, T_MID);
        }
    }
    FM6_CHART_HOOK(FMH_BUS, 0, 0);
    fm_bus(cl, cr, bus, grid_on ? 14 : 24, grid_on ? 2 : 3);
    FM6_CHART_HOOK(FMH_FB, fbop, 0);
    fm_loop(x[fbop], y[fbop], hot == P_E1 ? T_ACCENT : fb ? T_MID : T_DIM, hw + 1, grid_on ? 2 : 5);
    for (k = 0; k < 6u; k++) {
        uint32_t on = pt[(5u - k) * FP_OP + FP_OL] != 0, fill_c = (car >> k & 1u) && on;
        uint16_t f = hotset >> k & 1u ? T_ACCENT : c, fill = k == sel ? T_ACCENT : fill_c ? f : T_RAISE;
        char n[2] = {(char)('1' + k), 0};
        FM6_CHART_HOOK(FMH_BOX, k, 0);
        cv_rrect(x[k] - hw, y[k], bw, bh, 3, fill, T_BG);
        GFX_HOOK_ALIGN(x[k] - hw, y[k], x[k] + hw + 1, y[k] + bh, AL_HV, "FM6 operator number in its box");
        cv_text_in(x[k] - hw, y[k] + CAP_IN(S, bh), bw, &AF_S, n, fill_c || k == sel ? T_INK : on ? f : T_DIM, fill);
    }
    }
    FM6_CHART_HOOK(FMH_LABEL, 0, 0);
    {   /* the labels: on the 76 px card at the left, the algorithm and under it the voice name / the operator's frequency (the chart has the right part of the card) */
        const int32_t ly = grid_on ? 2 - 1 : 4 - GOY, ly2 = grid_on ? 22 - 1 : 4 - GOY;
        cv_text(grid_on ? 9 : 10, ly, &AF_S, b, hot == P_E0 || hot == P_E7 ? T_ACCENT : T_MID);
        if (sel < 6u) {                                  /* the operator picked: "OP3 x1.50" */
            char fq[16] = {'O', 'P', (char)('1' + sel), ' ', 0};
            fop_freq(pt + (5u - sel) * FP_OP, fq + 4);
            if (grid_on) cv_text(9, ly2, &AF_S, fq, T_TEXT); else cv_text_r(228, ly2, &AF_S, fq, T_TEXT, T_BG);
        } else if (fm6_bank_slot(fm6_slot[(t - trk) % NTRK]) >= 0) {   /* SLOT B1..B32: the bank voice's name */
            char nm[11];
            for (k = 0; k < 10u; k++)
                nm[k] = (char)pt[FP_NAME + k];
            for (nm[10] = 0; k && nm[k - 1] == ' '; k--)
                nm[k - 1] = 0;
            if (grid_on) cv_text(9, ly2, &AF_S, nm, T_TEXT); else cv_text_r(228, ly2, &AF_S, nm, T_TEXT, T_BG);
        }
    }
    FM6_CHART_HOOK(FMH_END, alg, 0);
}

/* OP ENV (ui_fm6op.c): the picked operator's envelope as the patch has it: from L4 to L1 at R1, L2 at R2, L3 at R3, held
 * there while the key is down (a RAISE mark where it is let go), back to L4 at R4. A stage's width grows as its rate
 * falls (4..48 px: 99 is a step, 0 the longest), the levels 0..99 linearly (dB). The curve THEME, the stage KNOB 3 / 4
 * edit (STAGE) ACCENT with a dot at its target; "OP3" top left, the stage's rate and level top right */
static void graph_fm6env(const track_t *t, uint16_t c)
{
    static const char *const STG[4] = {"ATK", "DEC", "SUS", "REL"};
    const uint8_t *op = fop_bytes((uint32_t)(t - trk));
    int32_t top = grid_on ? 23 - GOY : 24, bot = grid_on ? 66 - GOY : 88, x[6], y[6], k;   /* (the 76 px card: the curve on rows 23 .. 66, the axis on 68; the 122 px panel: 35 .. 99) */
    uint32_t sg = fop_stg & 3u;
    char b[24] = {'O', 'P', (char)('1' + fop_op % 6u), 0};
#define FEGY(l) (bot - (int32_t)(l) * (bot - top) / 99)
#define FEGW(r) (4 + (99 - (int32_t)(r)) * (99 - (int32_t)(r)) * 44 / 9801)
    x[0] = 12;
    y[0] = FEGY(op[FP_L1 + 3]);
    for (k = 0; k < 3; k++) {
        x[k + 1] = x[k] + FEGW(op[FP_R1 + k]);
        y[k + 1] = FEGY(op[FP_L1 + k]);
    }
    x[5] = 228;
    x[4] = x[5] - FEGW(op[FP_R1 + 3]);
    y[4] = y[3];
    y[5] = y[0];
    cv_rect(PANEL_X0, bot + 2, PANEL_W, 1, T_RAISE);
    cv_rect(x[4], top, 1, bot - top + 1, T_RAISE);   /* the key let go */
    for (k = 0; k < 5; k++)                          /* (3: the hold, no stage of its own) */
        cv_line_t(x[k], y[k], x[k + 1], y[k + 1], k != 3 && (uint32_t)(k < 3 ? k : 3) == sg ? T_ACCENT : c, 2);
    k = sg < 3u ? (int32_t)sg + 1 : 5;
    fm_dot(x[k], y[k], T_ACCENT);
#undef FEGY
#undef FEGW
    cv_text(grid_on ? 9 : 10, grid_on ? 2 - GOY : 4 - GOY, &AF_S, b, T_MID);   /* (the 76 px card: capitals' rows 6 .. 16 as the chart's label) */
    str_cpy(b, STG[sg], sizeof b);
    str_cpy(b + str_len(b), "  R ", 5);
    fmt_int(b + str_len(b), op[FP_R1 + sg]);
    str_cpy(b + str_len(b), "  L ", 5);
    fmt_int(b + str_len(b), op[FP_L1 + sg]);
    cv_text_r(grid_on ? 231 : 228, grid_on ? 2 - GOY : 4 - GOY, &AF_S, b, T_TEXT, T_BG);
}

/* SAVE > MULTI's slots: used and their names (project.c multi.c keeps them in RAM: cheap every frame) */
static char graph_pname[MU_SLOTS][13];
static uint32_t graph_pname_sig;
static int graph_project_used(uint32_t slot)
{
    uint32_t i;
    for (i = 0; i < MU_SLOTS; i++)
        project_info(i, graph_pname[i]);
    graph_pname_sig = fnv(2166136261u, graph_pname, sizeof graph_pname);
    return project_used(slot % MU_SLOTS);
}
static const char *graph_project_name(uint32_t slot)  /* (after graph_project_used) */
{
    return graph_pname[slot % MU_SLOTS];
}


/* the page's name and its number in its family, apart (the header shows the name, the footer the number; nm holds
 * 28, ct 8; ct empty: the page is alone in its family): "ENV DEST" "2/2", EDIT: the engine's "OSC" "1/2"; the piano
 * roll: its scale ("C MIN") for the name. Returns 1 for a piano roll whose scale is the name (the strip's title
 * leaves the number out there) */
static int page_parts(char *nm, char *ct)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    uint32_t i, n = 0, k = 0;
    int roll = 0;
    const char *pt = pg->scope == SC_ENGINE && pg->graph != GR_ENGINE ? eng_shown(e->name) : 0;   /* EDIT 1 / 2: the engine's name (T0227: the pages no longer follow its two groups;
                                                     * ENGINE: its own title) */
    ct[0] = 0;
    for (i = 0; i < NPAGES && pg->scope != SC_TEST && pg->scope != SC_PERF; i++)   /* (TEST, PERFORM: on no family's round, no number) */
        if (PAGES[i].fam == pg->fam && page_visible(i)) {
            n++;
            if (i == ui.page)
                k = n;
        }
    str_cpy(nm, title_shown(pt ? pt : pg->title), 28);   /* (T0227: the shown title, params.c title_shown) */               /* (0.2.21: 20, "MULTI FILTER 1/2" is 16) */
    if (!pt && pg->scope == SC_TRACK) {             /* item 158: a numbered page ("DUCK 1/2") whose sibling pages are all out of the page order (page_visible: no card to show) is alone: its title has no number, as a page alone in its family has none */
        uint32_t L = str_len(pg->title), m = 0, j;
        if (L > 4u && pg->title[L - 4u] == ' ' && pg->title[L - 2u] == '/') {
            for (i = 0; i < NPAGES; i++)
                if (PAGES[i].fam == pg->fam && page_visible(i) && str_len(PAGES[i].title) == L) {
                    for (j = 0; j < L - 4u && PAGES[i].title[j] == pg->title[j]; j++)
                        ;
                    m += j == L - 4u;
                }
            L = str_len(nm);
            if (m == 1u && L > 4u && nm[L - 4u] == ' ' && nm[L - 2u] == '/')
                nm[L - 4u] = 0;
        }
    }
    if (pg->scope == SC_SCALE)                      /* the SCALE page is GLOBAL (core.h): its title says so, whichever part is selected */
        str_cpy(nm, "GLOBAL SCALE", 16);
                                                    /* (HOME's pages: "MORPH 1/4"; 0.2.14: OVERVIEW is no longer the round's last, 1/5) */
    if (n > 1) {
        fmt_int(ct, (int32_t)k);
        str_cpy(ct + str_len(ct), "/", 4);
        fmt_int(ct + str_len(ct), (int32_t)n);
    }
    if (pg->graph == GR_ROLL) {  /* the piano roll: its tinted rows' scale ("C MIN") */
        str_cpy(nm, N_NOTE[(uint32_t)root_of(t) % 12u], 4);
        str_cpy(nm + str_len(nm), " ", 4);
        one_line(nm + str_len(nm), 25u, val_shown(&SCALE_D, N_SCALE[clamp(scale_of(t), 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)]));   /* (owner 2026-10-25: the whole shown name, "C DIMINISHED HALF-WHOLE"; the old copy kept 7 letters) */
        roll = 1;
    }
    return roll;
}
/* the page's title and its number in its family ("ENV DEST 2/2", EDIT: the engine's "OSC 1/2"); the piano roll: its
 * scale ("C MIN"); ti holds 28 (the strip) */
static void page_title(char *ti)
{
    char nm[28], ct[8];
    int roll = page_parts(nm, ct);
    str_cpy(ti, nm, 28);
    if (ct[0] && !roll) {
        str_cpy(ti + str_len(ti), " ", 4);
        str_cpy(ti + str_len(ti), ct, 8);
    }
}
/* ---------------------------------------------- 0.2.25 layout round: the pictures of the EQ, the filters, the compressor and the arp ---
 * (owner: a natural picture wherever there is one, in place of the scope). All static (drawn when a value changes; graph_signature), in the UI loop only,
 * from the audio's own formulas (fx.c eq_design, cmp_gr, cmp_mk; voice.c uf_run's weights and dampings; seq.c's note order), at a low resolution:
 * the frequency pictures are 56 points 4 px apart over 20 Hz .. 20 kHz (a log axis, PANEL_W wide). Integer only (the device has no FPU) */
#define GF_N 56u                                     /* the points of a frequency curve, 4 px apart */
#define GF_STEP 74307u                               /* their ratio, 1000^(1/55) in Q16 (20 Hz .. 20 kHz) */
#define GF_F0 (20u << 8)                             /* the first point, 20 Hz in Q8 */
/* 256 x log2 of a power (a 64-bit value >= 1) */
static int32_t gr_lg64(uint64_t a)
{
    int32_t sh = 0;
    while (a >> 32) {
        a >>= 1;
        sh += 256;
    }
    return cmp_lg((uint32_t)a | 1u) + sh;
}
/* a power ratio num / den (as 256 log2 of each) in tenths of a dB */
static int32_t gr_db10(int32_t lgn, int32_t lgd) { return ((lgn - lgd) * 7706) >> 16; }   /* (10 log10 2 / 256 x 10, Q16) */
/* x of a frequency (Hz Q8) on the 20 Hz .. 20 kHz axis (0 .. PANEL_W - 4) */
static int32_t gf_x(uint32_t f_q8)
{
    int32_t x = (cmp_lg(f_q8 | 1u) - cmp_lg(GF_F0)) * (int32_t)(4u * (GF_N - 1u)) / 2551;   /* (256 log2 1000 = 2551) */
    return clamp(x, 0, (int32_t)(4u * (GF_N - 1u)));
}

/* EQ 1/2, 2/2: the curve of the three bands together (fx.c eq_design: the same coefficients the audio runs, a band at 0 dB is flat and left out), +-12 dB over
 * the panel's height, the 0 dB line RAISE; a dot at each band's frequency on the curve (the band of the knob just turned: the accent; a band at 0 dB: DIM).
 * |H|^2 of a biquad at w (phi = sin^2(w / 2), the form that keeps its precision at low frequencies):
 *   ((b0 + b1 + b2)^2 - 4 (b0 b1 + 4 b0 b2 + b1 b2) phi + 16 b0 b2 phi^2) / ((1 + a1 + a2)^2 - 4 (a1 + 4 a2 + a1 a2) phi + 16 a2 phi^2) */
typedef struct { int64_t s, p, q; } gq_t;            /* a quadratic in phi: s (Q28, squared Q56), p, q (Q28) */
static void eq_quad(gq_t *n, gq_t *d, const eqc_t *c)
{
    int64_t b0 = c->b0, b1 = c->b1, b2 = c->b2, a1 = c->a1, a2 = c->a2, one = EQ_ONE;
    n->s = b0 + b1 + b2;
    n->p = (b0 * b1 + 4 * b0 * b2 + b1 * b2) >> 28;
    n->q = (b0 * b2) >> 28;
    d->s = one + a1 + a2;
    d->p = (a1 + 4 * a2 + ((a1 * a2) >> 28));
    d->q = a2;
}
static uint64_t gq_at(const gq_t *g, int64_t ph30)   /* the quadratic at phi (Q30): Q52, >= 1 (phi^2 kept to its bits: a low shelf's terms are tiny there) */
{
    int64_t v = ((g->s * g->s) >> 4) - 4 * g->p * (ph30 >> 6) + 16 * ((((g->q * ph30) >> 30) * ph30) >> 6);
    return v > 0 ? (uint64_t)v : 1u;
}
/* phi = sin^2(pi f / FS) in Q30 for f in Hz Q8 (the sine as fx.c eq_design makes it: Taylor to h^9) */
static int64_t gr_phi30(uint32_t f_q8)
{
    int64_t h = ((int64_t)f_q8 * EQ_HK) >> 10, h2, t;
    if (h > 413000000ll)                             /* (0.49 FS at most: the top of the axis, 20 kHz, is under it) */
        h = 413000000ll;
    h2 = (h * h) >> 28;
    t = EQ_ONE - h2 / 72;
    t = EQ_ONE - (h2 * t >> 28) / 42;
    t = EQ_ONE - (h2 * t >> 28) / 20;
    t = EQ_ONE - (h2 * t >> 28) / 6;
    t = h * t >> 28;                                 /* sin, Q28 */
    return (t * t) >> 26;
}
static int32_t eq_curve_db10(const gq_t *n, const gq_t *d, uint32_t on, uint32_t f_q8)
{
    int64_t ph = gr_phi30(f_q8);
    int32_t db = 0;
    uint32_t k;
    for (k = 0; k < 3u; k++)
        if ((on >> k) & 1u)
            db += gr_db10(gr_lg64(gq_at(&n[k], ph)), gr_lg64(gq_at(&d[k], ph)));
    return db;
}
static void graph_eq(const track_t *t, uint16_t c)
{
    static const uint8_t GK[3] = {P_EQLG, P_EQMG, P_EQHG}, FK[3] = {P_EQLF, P_EQMF, P_EQHF};
    static const int8_t FMAX[3] = {24, 24, 12};
    gq_t n[3], d[3];
    eqc_t cf;
    uint32_t k, i, on = 0, f = GF_F0, hot = 3u;
    int32_t cy = graph_ht / 2, a = 40, px = 0, py = cy;
    const page_t *pg = cur_page();
    if (ui.hot_t)                                    /* the band of the knob just turned: EQ 1/2 K1 K2 LOW, K3 K4 MID; EQ 2/2 K1 MID (Q), K2 K3 HIGH */
        hot = knob_slot(pg, ui.hot_col) > 3u ? 3u : pg->id[0] == P_EQLF ? knob_slot(pg, ui.hot_col) / 2u : knob_slot(pg, ui.hot_col) ? 2u : 1u;   /* (T0227: the card's slot) */
    for (k = 0; k < 3u; k++) {
        int32_t g = clamp(t->p[GK[k]], -12, 12);
        if (!g)
            continue;
        eq_design(&cf, k, g, clamp(t->p[FK[k]], -24, FMAX[k]), k == 1u ? clamp(t->p[P_EQMQ], -12, 12) : 0);
        eq_quad(&n[k], &d[k], &cf);
        on |= 1u << k;
    }
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);      /* 0 dB */
    for (i = 0; i < GF_N; i++, f = (uint32_t)(((uint64_t)f * GF_STEP) >> 16)) {
        int32_t x = PANEL_X0 + 4 * (int32_t)i, y = cy - clamp(eq_curve_db10(n, d, on, f), -120, 120) * a / 120;
        if (i)
            cv_line_t(px, py, x, y, c, 2);
        px = x;
        py = y;
    }
    for (k = 0; k < 3u; k++) {                       /* the bands' dots, on the curve at their frequency */
        uint32_t fk = eq_hz_q8(k, clamp(t->p[FK[k]], -24, FMAX[k]));
        int32_t x = PANEL_X0 + gf_x(fk), y = cy - clamp(eq_curve_db10(n, d, on, fk), -120, 120) * a / 120;
        cv_rect(x - 2, y - 2, 5, 5, k == hot ? T_ACCENT : ((on >> k) & 1u) ? c : T_DIM);
    }
}

/* MULTI FILTER 1/2, 2/2 and ANALOG's VCF: the response of the state-variable filter (voice.c uf_run, eng_analog.c analog_tap) at its CUT and RES, from the
 * analog prototype the trapezoidal SVF follows: H(s) = (wl + wb kb s + wh s^2) / (s^2 + k s + 1), s = j f / fc; the MULTI FILTER's MODE as uf_run's weights (LP .. BP .. HP),
 * its band-pass normalised (kb = k), SLOPE 24 dB a second stage of the same MODE with the Butterworth pair's dampings; ANALOG's BP is the SVF's plain band output (kb = 1:
 * its peak rises with RES). +18 .. -42 dB over the panel (0 dB RAISE), a tick at the cutoff. The MULTI FILTER at its open end with MODE LP is not run: a flat line */
static int32_t svf_db10(uint32_t f_q8, uint32_t fc, int32_t wl, int32_t wb, int32_t wh, int32_t k, int32_t kb)
{
    uint64_t r = ((uint64_t)f_q8 << 8) / (fc ? fc : 1u), u;   /* f / fc, Q16 */
    int64_t u2, A, B, Dr, Di, W_l = (int64_t)wl << 6, W_b = (int64_t)wb << 6, W_h = (int64_t)wh << 6, K = (int64_t)k << 4, KB = (int64_t)kb << 4;
    u = r <= 65536u ? r : ((uint64_t)1 << 32) / (r ? r : 1u);   /* (above the cutoff: the same in 1 / r, every term divided by r^4) */
    u2 = (int64_t)((u * u) >> 16);
    A = r <= 65536u ? W_l - ((W_h * u2) >> 16) : ((W_l * u2) >> 16) - W_h;
    B = (((W_b * KB) >> 16) * (int64_t)u) >> 16;
    Dr = r <= 65536u ? 65536 - u2 : u2 - 65536;
    Di = (K * (int64_t)u) >> 16;
    return gr_db10(gr_lg64((uint64_t)(A * A + B * B) + 1u), gr_lg64((uint64_t)(Dr * Dr + Di * Di) + 1u));
}
static uint32_t gf_cut_hz(int32_t idx_q8)            /* CUTOFF_HZ between its entries (the filters' index, Q8) */
{
    int32_t i = clamp(idx_q8, 0, 127 << 8) >> 8, fr = idx_q8 & 255;
    return CUTOFF_HZ[i] + (i < 127 ? (uint32_t)(((int32_t)(CUTOFF_HZ[i + 1] - CUTOFF_HZ[i]) * fr) >> 8) : 0u);
}
static void graph_twin(const track_t *t, uint16_t c, uint32_t kind);
static void graph_filt(const track_t *t, uint16_t c)
{
    const page_t *pg = cur_page();
    int s24 = 0, flat = 0;
    {   /* cloud job H: the TWIN FILTER's three pages, and MULTI FILTER 1/2, 2/2 while filter 2 runs, draw both filters (graph_twin); with filter 2 OFF the MULTI FILTER pages are drawn as ever */
        uint32_t id0 = pg->id[0], kind = id0 == P_TWTYPE ? 1u : id0 == XW_BASE + X_KTRK2 ? 2u : id0 == XW_BASE + X_ROUTE ? 3u : (id0 == P_FREEA || id0 == P_KTRK) && twin_on(t) ? 4u : 0u;
        if (kind) {
            graph_twin(t, c, kind);
            return;
        }
    }
    int32_t wl, wb, wh, k1, k2 = 0, kb, z = 30, px = 0, py = 0;
    uint32_t fc, i, f = GF_F0;
    {                                                /* the MULTI FILTER (voice.c uf_run, UF_K / UF_K24A / UF_K24B) */
        int32_t md = clamp(t->p[P_FREEC], 0, UF_HP), res = clamp(t->p[P_FREEB], 0, 127), cut = clamp(t->p[P_FREEA], 0, UF_STEPS);
        wb = md <= 64 ? md << 4 : ((UF_HP - md) * 65) >> 2, wl = md < 64 ? 1024 - wb : 0, wh = md > 64 ? 1024 - wb : 0;
        s24 = t->p[P_FREED] == 2;
        k1 = s24 ? UF_K24A : UF_K(res);
        k2 = UF_K24B(res);
        kb = -1;
        fc = gf_cut_hz(40 + 396 * cut);
        flat = cut >= UF_STEPS && md <= 0;           /* (the bypass: the filter is not run) */
    }
    cv_rect(PANEL_X0, z, PANEL_W, 1, T_RAISE);       /* 0 dB */
    if (!flat)
        cv_rect(PANEL_X0 + gf_x(fc << 8), 4, 1, 92, T_RAISE);   /* the cutoff */
    for (i = 0; i < GF_N; i++, f = (uint32_t)(((uint64_t)f * GF_STEP) >> 16)) {
        int32_t db = 0, x = PANEL_X0 + 4 * (int32_t)i, y;
        if (!flat) {
            db = svf_db10(f, fc, wl, wb, wh, k1, kb < 0 ? k1 : kb);
            if (s24)
                db += svf_db10(f, fc, wl, wb, wh, k2, k2);
        }
        y = z - clamp(db, -420, 180) * 3 / 20;       /* 1.5 px a dB */
        if (i)
            cv_line_t(px, py, x, y, c, 2);
        px = x;
        py = y;
    }
}

/* The TWIN FILTER's pictures (cloud job H, FILTER_PAGES_PROPOSAL 2.3): ONE graph for both filters, the same axes as graph_filt. kind 1 TWIN FILTER, 2 TWIN MOD (filter 2's curve bright, filter 1's dim; filter 2 OFF: filter 1 bright
 * alone), 4 MULTI FILTER 1/2, 2/2 with filter 2 on (filter 1 bright, filter 2 dim), 3 TWIN CHAIN (the whole chain bright, both single curves dim, no ticks, the flow strip under it). A tick for each cutoff (the numeral 1 or 2
 * beside it while both run), the caption row under the curves. The curves are twin_graph_db's (twin_graph.c, 1/16 dB: DRIVE is in none of them). COMB has no cutoff: no tick 2 */
static void graph_twin(const track_t *t, uint16_t c, uint32_t kind)
{
    const int on = twin_on(t), comb = t->p[P_TWTYPE] == TT_COMB, link = XV(t, X_LINK) != 0;
    const int bright = kind == 3u ? 2 : (kind == 1u || kind == 2u) && on ? 1 : 0;
    int32_t cut1 = clamp(t->p[P_FREEA], 0, UF_STEPS), cut2 = link ? clamp(cut1 * 256 + TW_OFF_Q8(clamp(t->p[P_CUT2], -TW_OFF_MAX, TW_OFF_MAX)), 0, UF_STEPS * 256) : clamp(t->p[P_CUT2], 0, UF_STEPS) * 256;   /* (cut2 in Q8 dial steps) */
    int32_t flat1 = cut1 >= UF_STEPS && clamp(t->p[P_FREEC], 0, UF_HP) <= 0, x1 = PANEL_X0 + gf_x(gf_cut_hz(40 + 396 * cut1) << 8), x2 = PANEL_X0 + gf_x(gf_cut_hz(40 + ((396 * cut2) >> 8)) << 8);
    char cap[40];
    uint32_t pass;
    /* item 119: the card is 76 px high (rows 0 .. 75), the caption row 64 .. 74 (graph_cap). The curves: the 0 dB line on row 20 and 1 px a dB (-36 dB on row 56) on the single pages; on the chain page
     * (curves, then the flow strip under them) the 0 dB line on row 10 and 1/2 px a dB (the curves fill rows 1 .. 31); the single pages' curves end at -36 dB, row 56 */
    const int32_t zc = kind == 3u ? 10 : 20, zs = kind == 3u ? 1 : 2;
    cv_rect(PANEL_X0, zc, PANEL_W, 1, T_RAISE);      /* 0 dB */
    if (kind != 3u) {                                /* the ticks: the filter being edited stronger */
        if (!flat1)
            cv_rect(x1, 4, 1, 58, bright == 0 ? T_MID : T_RAISE);
        if (on && !comb)
            cv_rect(x2, 4, 1, 58, bright == 1 ? T_MID : T_RAISE);
        if (on && !flat1 && !comb) {                 /* .. and which is which: the name outside, away from the other tick (MULTI left of the left one, TWIN right of the right one; equal: MULTI left, TWIN right; 1.0, owner: the filters' names, not 1 and 2).
                                                      * Where a word would leave the panel it goes to the other side of its tick */
            const int32_t w1 = text_w(&AF_S, "MULTI"), w2 = text_w(&AF_S, "TWIN");
            int32_t a1 = x2 < x1 ? x1 + 3 : x1 - 2 - w1, a2 = x2 < x1 ? x2 - 2 - w2 : x2 + 3;
            if (a1 < PANEL_X0)
                a1 = x1 + 3;
            else if (a1 + w1 > PANEL_X0 + PANEL_W)
                a1 = x1 - 2 - w1;
            if (a2 < PANEL_X0)
                a2 = x2 + 3;
            else if (a2 + w2 > PANEL_X0 + PANEL_W)
                a2 = x2 - 2 - w2;
            cv_text_on(a1, 2, &AF_S, "MULTI", bright == 0 ? T_TEXT : T_MID, T_BG);
            cv_text_on(a2, a2 < a1 + w1 + 2 && a2 + w2 + 2 > a1 ? 14 : 2, &AF_S, "TWIN", bright == 1 ? T_TEXT : T_MID, T_BG);   /* (the two words would touch: TWIN goes one line down) */
        }
    }
    for (pass = 0; pass < 2u; pass++) {              /* the dim curves first, the bright one over them */
        uint32_t w;
        for (w = 0; w < 3u; w++) {
            int dim = (int)w != bright;
            uint32_t i, f = GF_F0;
            int32_t px = 0, py = 0;
            if ((pass == 0u) != dim)
                continue;
            if (kind != 3u && (w == 2u || (w != (uint32_t)bright && !on)))   /* (the chain page: all three; the others: filters 1 and 2, the one not edited only while filter 2 runs) */
                continue;
            for (i = 0; i < GF_N; i++, f = (uint32_t)(((uint64_t)f * GF_STEP) >> 16)) {
                int32_t db = twin_graph_db(t, (int)w, f >> 8) * 10 / 16, x = PANEL_X0 + 4 * (int32_t)i, y = zc - clamp(db, grid_on && kind != 3u ? -360 : -420, 180) * zs / 20;   /* (item 119: on the card the floor is -36 dB, row 56, clear of the caption row) */
                if (i)
                    cv_line_t(px, py, x, y, dim ? T_DIM : c, dim ? 1 : 2);
                px = x;
                py = y;
            }
        }
    }
    cap[0] = 0;
    if (kind == 3u) {                                /* the flow strip (1.0, owner): the filters by name. SERIES one line, as chain_f2 runs it: the engine, MULTI, DRIVE (before TWIN's input), TWIN, AMP.
                                                      * PARALLEL two branches: DRIVE acts on the signal that enters both (chain_f2_pre), then MULTI on one line and TWIN on the other, merging into AMP, caption MULTI + TWIN */
        const uint16_t wc = T_MID, ac = T_DIM, lcol = T_MID, dc = t->p[P_DRV2] > 0 ? T_TEXT : T_DIM;   /* (DRIVE dim while it is OFF: not run) */
        int32_t x = grid_on ? UT_GR_NOTE_X : PANEL_X0 + 2;   /* (item 119: from the caption's own left edge) */
        if (!XV(t, X_ROUTE)) {
            x = cv_text_on(x, 55, &AF_S, "ENGINE", wc, T_BG) + 2;
            x = cv_text_on(x, 55, &AF_S, ">", ac, T_BG) + 2;
            x = cv_text_on(x, 55, &AF_S, "MULTI", T_TEXT, T_BG) + 2;
            x = cv_text_on(x, 55, &AF_S, ">", ac, T_BG) + 2;
            x = cv_text_on(x, 55, &AF_S, "DRIVE", dc, T_BG) + 2;
            x = cv_text_on(x, 55, &AF_S, ">", ac, T_BG) + 2;
            x = cv_text_on(x, 55, &AF_S, "TWIN", T_TEXT, T_BG) + 2;
            x = cv_text_on(x, 55, &AF_S, ">", ac, T_BG) + 2;
            cv_text_on(x, 55, &AF_S, "AMP", wc, T_BG);
        } else {
            const int32_t y1 = 34, y2 = 52, ym = 43, h = 9;   /* the text tops of the two branches and of the trunk (item 119: in the 76 px card, under the curves); h: from a text top to the middle of its capitals */
            int32_t xf, xb, xm;
            x = cv_text_on(x, ym, &AF_S, "ENGINE", wc, T_BG) + 3;
            x = cv_text_on(x, ym, &AF_S, ">", ac, T_BG) + 3;
            x = cv_text_on(x, ym, &AF_S, "DRIVE", dc, T_BG) + 4;
            xf = x + 2;                                   /* the fork */
            cv_rect(x, ym + h, xf - x, 1, lcol);
            cv_rect(xf, y1 + h, 1, y2 - y1 + 1, lcol);
            xb = xf + 8;
            cv_rect(xf, y1 + h, xb - xf, 1, lcol);
            cv_rect(xf, y2 + h, xb - xf, 1, lcol);
            xm = cv_text_on(xb + 2, y1, &AF_S, "MULTI", T_TEXT, T_BG);
            x = cv_text_on(xb + 2, y2, &AF_S, "TWIN", T_TEXT, T_BG);
            xm = (xm > x ? xm : x) + 4;                   /* the merge, right of the longer word */
            cv_rect(xb + 2 + text_w(&AF_S, "MULTI") + 1, y1 + h, xm - (xb + 2 + text_w(&AF_S, "MULTI") + 1), 1, lcol);
            cv_rect(xb + 2 + text_w(&AF_S, "TWIN") + 1, y2 + h, xm - (xb + 2 + text_w(&AF_S, "TWIN") + 1), 1, lcol);
            cv_rect(xm, y1 + h, 1, y2 - y1 + 1, lcol);
            cv_rect(xm, ym + h, 5, 1, lcol);
            x = cv_text_on(xm + 7, ym, &AF_S, ">", ac, T_BG) + 3;
            cv_text_on(x, ym, &AF_S, "AMP", wc, T_BG);
            str_cpy(cap, "MULTI + TWIN", sizeof cap);
        }
    } else if (kind == 4u) {
        if (link && !comb)
            str_cpy(cap, "TWIN FOLLOWS MULTI", sizeof cap);
    } else if (!on) {
        str_cpy(cap, "TWIN FILTER OFF", sizeof cap);
    } else if (kind == 1u) {
        str_cpy(cap, comb ? "SHOWN AT C3" : link ? "TWIN = MULTI + OFFSET" : "TWIN FILTER ON ITS OWN", sizeof cap);
    } else {
        str_cpy(cap, link && !comb ? "ADDED TO WHAT MULTI SENDS" : "TWIN FILTER ONLY", sizeof cap);
    }
    if (cap[0])
        graph_cap(cap, T_MID);
}

/* SUB OSC (cloud job H, SUB_AND_CYCLE_DESIGN A4): the sub's wave small across four cycles of the note, its octave copies when HARM is up. OCTAVE -2, -1, 0 (stored 0..2) makes the sub one cycle, two or four
 * cycles over those four of the note; HARM adds the same wave an octave above the sub (HARM 0 .. 64: up to half the sub's level) and then two octaves above it (64 .. 127, up to half). LEVEL sets the height; at 0 the sub
 * is not run and the picture says so. The sum bright, the copies dim */
static int32_t sub_wave(uint32_t wave, uint32_t ph)    /* one cycle at phase ph (32 bit): -32767 .. +32767; PULSE is a width of 1/4 */
{
    int32_t x = (int32_t)(ph >> 17);                   /* 0 .. 32767 a cycle */
    switch (wave & 3u) {
    case 0:
        return sine_i(ph);
    case 1:
        return clamp(x < 8192 ? 4 * x : x < 24576 ? 65536 - 4 * x : 4 * x - 131072, -32767, 32767);   /* triangle: up through zero to +1 at 1/4, down to -1 at 3/4 (the engine's osc_tri starts where the sine does) */
    case 2:
        return x < 16384 ? 32767 : -32767;
    default:
        return x < 8192 ? 32767 : -32767;
    }
}
static void graph_sub(const track_t *t, uint16_t c)
{
    int32_t cy = graph_ht / 2, lv = clamp(XV(t, X_SUBLVL), 0, 127), h = 0 /* SUB HARMONICS disabled (owner, 2026-10-21) */, oct = clamp(XV(t, X_SUBOCT), 0, 2), amp = 4 + lv * 36 / 127;
    uint32_t wave = (uint32_t)XV(t, X_SUBWAVE), cyc = 1u << (uint32_t)oct, i, k;      /* cycles of the sub over four of the note: 1, 2, 4 */
    int32_t a1 = lv ? amp * (h < 64 ? h : 64) / 128 : 0, a2 = lv && h > 64 ? amp * (h - 64) / 126 : 0, px[3] = {0, 0, 0}, py[3] = {0, 0, 0};
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
    for (i = 0; i <= 110u; i++) {
        int32_t x = PANEL_X0 + 2 * (int32_t)i, y[3], sum;
        uint32_t ph = (uint32_t)(((uint64_t)i * cyc << 32) / 110u);
        y[0] = lv ? sub_wave(wave, ph) * amp / 32767 : 0;
        y[1] = a1 ? sub_wave(wave, ph * 2u) * a1 / 32767 : 0;
        y[2] = a2 ? sub_wave(wave, ph * 4u) * a2 / 32767 : 0;
        sum = y[0] + y[1] + y[2];
        if (i) {
            if (a1)
                cv_line_t(px[1], py[1], x, cy - y[1], T_DIM, 1);
            if (a2)
                cv_line_t(px[2], py[2], x, cy - y[2], T_DIM, 1);
            cv_line_t(px[0], py[0], x, cy - clamp(sum, -46, 46), lv ? c : T_DIM, 2);
        }
        for (k = 0; k < 3u; k++) {
            px[k] = x;
            py[k] = cy - (k == 0u ? clamp(sum, -46, 46) : y[k]);
        }
    }
    if (!lv)
        graph_cap("NOT RUN AT LEVEL 0", T_MID);
}

/* CYCLE ENV 1/2 and 2/2 (cloud job H, SUB_AND_CYCLE_DESIGN B4): the cycle's shape as cycle.c makes it (y = x + c (x^2 - x), c = CURVE / 64, on the rise; the fall is 1 - that of the progress), RISE and FALL in the
 * proportion of their times (ENV_LIN's steps; with SYNC the note values' share of a bar), three rise-fall shapes for LOOP, one then rest for ONCE. 2/2 draws the one cycle large, rise and fall the same width, with the straight line behind it
 * (CURVE 0) so the bend shows. The times themselves are on the cards: the picture is the shape, not a time scale */
static void graph_cycle(const track_t *t, uint16_t c)
{
    const int big = cur_page()->id[0] == XW_BASE + X_CYCURVE;
    int32_t top = 8, base = 92, cv = clamp(XV(t, X_CYCURVE), -64, 63) * 512, w = PANEL_W - 4, n = big || XV(t, X_CYMODE) ? 1 : 3, x0 = PANEL_X0 + 2, px = 0, py = 0, cw, rw, fw, i;
    uint32_t pass;
    uint64_t dr, df;
    uint32_t r = (uint32_t)XV(t, X_CYRISE) & 127u, f = (uint32_t)XV(t, X_CYFALL) & 127u;
    if (XV(t, X_CYSYNC)) {                            /* note values: ESYNC_UNITS (1/384 of a bar), 0 the shortest step */
        dr = (uint64_t)ESYNC_UNITS[esync_idx((int32_t)r)] + 2u;
        df = (uint64_t)ESYNC_UNITS[esync_idx((int32_t)f)] + 2u;
    } else {                                          /* the table's step a block (Q24): the time is 1 / step */
        dr = (uint64_t)((1ull << 40) / (ENV_LIN[r] ? ENV_LIN[r] : 1u));
        df = (uint64_t)((1ull << 40) / (ENV_LIN[f] ? ENV_LIN[f] : 1u));
    }
    cw = XV(t, X_CYMODE) && !big ? w * 2 / 3 : w / n;  /* ONCE: the one shape in two thirds, then rest */
    rw = big ? cw / 2 : (int32_t)((uint64_t)cw * dr / (dr + df));   /* (2/2: both segments the same width, so the bend of each shows; 1/2: the proportion of the times) */
    rw = clamp(rw, 3, cw - 3);
    fw = cw - rw;
    cv_rect(PANEL_X0, base, PANEL_W, 1, T_RAISE);
    for (pass = 0; pass < (big && cv ? 2u : 1u); pass++) {   /* 2/2: the straight line first, dim */
        int32_t cc = pass == 0u && big && cv ? 0 : cv, k;
        for (k = 0; k < (n); k++) {
            int32_t xs = x0 + k * cw;
            for (i = 0; i <= cw; i++) {
                int32_t y01, x, y;
                if (i <= rw) {
                    int32_t u = i * 32768 / rw;
                    y01 = u + ((cc * ((u * u >> 15) - u)) >> 15);
                } else {
                    int32_t u = (i - rw) * 32768 / fw;
                    y01 = 32768 - (u + ((cc * ((u * u >> 15) - u)) >> 15));
                }
                x = xs + i;
                y = base - clamp(y01, 0, 32768) * (base - top) / 32768;
                if (i || k)
                    cv_line_t(px, py, x, y, pass == 0u && big && cv ? T_DIM : c, pass == 0u && big && cv ? 1 : 2);
                px = x;
                py = y;
            }
        }
        if (XV(t, X_CYMODE) && !big)                  /* ONCE: it rests at 0 */
            cv_line_t(px, py, PANEL_X0 + PANEL_W - 2, base, c, 2);
    }
}

/* STEREO MASTER (WIDTH and BASS MONO, fx.c width_stage then bass_mono): frequency across (20 Hz .. 20 kHz), the width of the stereo picture up and down from the middle:
 * below the BASS MONO corner one line (mono), above it a band as wide as WIDTH (100 %: the normal width, 0 %: a line, 200 %: twice; wider only above the width stage's
 * 125 Hz crossover, narrower everywhere). Captions MONO and STEREO only (the numbers are on the cards). In DUEL the stages do not run: the note says why */
/* "NOT IN DUEL MODE" (DUELL 1.1, PAGES_V2_glo-a.png): the heading Bold 23, two lines under it; the first gap is the widest, the text sits as low as the panel allows */
static void duel_note(void)
{
    int32_t o = cv_oy;
    cv_oy = 0;
    cv_text_on(12, 12, &AF_L, "NOT IN DUEL MODE", T_TEXT, T_BG);
    cv_text_on(12, 42, &AF_M, "Part 1 left, part 2 right", T_MID, T_BG);
    cv_text_on(12, 58, &AF_M, "No stereo mix to process", T_MID, T_BG);
    cv_oy = o;
}
static void graph_mstg(uint16_t c)
{
    int32_t cy = 30, base = 12, wd = clamp(song.ms[0], 0, 200), xb = 0, x, i, w, pw = -1, px = 0;   /* (item 119: the band over rows 5 .. 55 of the 76 px card, MONO and STEREO on the caption row) */
    uint32_t bm = (uint32_t)song.bm, f = GF_F0;
    if (duell_mode == DM_SPLIT) {
        duel_note();
        return;
    }
    if (bm >= 1u && bm <= BM_N)
        xb = PANEL_X0 + gf_x((uint32_t)BM_HZ[bm - 1u] << 8);
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
    for (i = 0; i < (int32_t)GF_N; i++, f = (uint32_t)(((uint64_t)f * GF_STEP) >> 16)) {
        x = PANEL_X0 + 4 * i;
        w = x < xb ? 0 : wd <= 100 || f >= (125u << 8) ? base * wd / 100 : base;   /* the side's width there */
        if (i && pw >= 0) {
            cv_line_t(px, cy - pw, x, cy - w, c, 2);
            cv_line_t(px, cy + pw, x, cy + w, c, 2);
        }
        px = x;
        pw = w;
    }
    if (xb) {                                        /* the mono region: its corner, the caption under the line */
        cv_rect(xb, cy - 27, 1, 54, T_RAISE);
        if (xb - PANEL_X0 >= text_w(&AF_S, "MONO") + 4)
            graph_cap_at(PANEL_X0 + 2, "MONO", T_MID, 0);
    }
    if (PANEL_X0 + PANEL_W - (xb > PANEL_X0 ? xb : PANEL_X0) >= text_w(&AF_S, "STEREO") + 8)
        graph_cap_at(PANEL_X0 + PANEL_W - 2 - text_w(&AF_S, "STEREO"), "STEREO", T_MID, 0);
}

/* COMPRESSOR (fx.c comp_stage): the transfer curve, input -48 .. 0 dB across, output up on the same scale, with the soft knee and the automatic makeup (cmp_gr, cmp_mk: the
 * stage's own gain computer); the 1:1 line RAISE. RATIO OFF: the 1:1 line alone, DIM. At the right a bar of the gain reduction now (0 .. 12 dB down, live: comp_meter,
 * its own strip, redrawn only when it moved). In DUEL the stage does not run: the note says why */
#define CMP_GX1 196                                  /* the curve's box: x PANEL_X0 .. CMP_GX1, y 6 .. 106 */
#define CMP_GY0 6
#define CMP_GH 100
#define CMPM_X 210                                   /* the bar: x 210 .. 222 */
#define CMPM_W 12
static uint32_t cmpm_sig;
static int32_t comp_y(int32_t q8)                    /* a level (Q8 octaves, 4096 = 0 dB) -> y on the -48 .. 0 dB scale */
{
    return CMP_GY0 + clamp((4096 - q8) * CMP_GH / (48 * 10886 / 256), 0, CMP_GH);
}
static void graph_comp(uint16_t c)
{
    uint32_t r = (uint32_t)clamp(song.cp[1], 0, 8), i;
    int32_t thr = cmp_thr_q8((uint32_t)clamp(song.cp[0], 0, 40)), sl = (int32_t)CMP_SLOPE[r], mk = cmp_mk(thr, sl), w = CMP_GX1 - PANEL_X0, x, px = 0, py = 0;
    if (duell_mode == DM_SPLIT) {
        duel_note();
        return;
    }
    cv_line(PANEL_X0, CMP_GY0 + CMP_GH, CMP_GX1, CMP_GY0, r ? T_RAISE : T_DIM);   /* 1:1 */
    if (!r)
        return;
    for (i = 0; i <= 46u; i++) {
        int32_t in = 4096 - (46 - (int32_t)i) * (48 * 10886 / 256) / 46, y;
        x = PANEL_X0 + (int32_t)i * w / 46;
        y = comp_y(in - cmp_gr(in - thr, sl) + mk);
        if (i)
            cv_line_t(px, py, x, y, c, 2);
        px = x;
        py = y;
    }
    cv_rect(PANEL_X0 + (int32_t)((thr - (4096 - 48 * 10886 / 256)) * w / (48 * 10886 / 256)), comp_y(thr - cmp_gr(0, sl) + mk) - 2, 1, 5, T_MID);   /* THRESH's place on the curve */
}
static void comp_meter(void)
{
    int32_t gr = 0, px;
    uint32_t sig;
    if (duell_mode == DM_SPLIT || !song.cp[1])
        return;
    if (cmp.a > 0) {                                 /* (the stage runs: what its gain computer takes now, the detector's level as comp_stage reads it, PERFORM idle) */
        int32_t pk = cmp.pk;
        gr = cmp_gr(cmp_lg((uint32_t)(pk >> 12) + 1u) - cmp.thr, cmp.slope) * (cmp.a >> 7) >> 8;
    }
    px = clamp(gr * 64 / 511, 0, 64);                /* 0 .. 12 dB (511 Q8 octaves) over 64 px */
    sig = (uint32_t)px * 31u + ux.gen * 4099u + (uint32_t)T_BG * 131u + 1u;
    if (!ui.force && sig == cmpm_sig)
        return;
    cmpm_sig = sig;
    cv_begin(CMPM_W, 66, T_BG);
    CV_HL(0, 0, CMPM_W, 66);                         /* (a meter: its track and the reduction) */
    cv_rect(0, 0, CMPM_W, 66, T_RAISE);
    if (px > 0)
        cv_rect(0, 1, CMPM_W, px, T_ACCENT);         /* from the top down: the reduction */
    cv_blit(CMPM_X, Y_GRAPH + CMP_GY0);
}

/* ARP 1/3, 2/3: the MODE's pattern, 16 steps as dots in a small piano roll: the keys held (else a C major chord, C E G from C4; "EXAMPLE: C E G" is said on the knob-1 line, ui_draw.c
 * k1_after_of) over OCT octaves in the MODE's order (seq.c arp_list, arp_pick: the same list and order; RANDOM and WALK as an example, a fixed seed), CHORD every step all the notes an octave
 * up a step; GATE the dots' length, SWING the off-steps later, CHANCE under 100 % the dots DIM. OFF and SEQ: the calm flat baseline (draw_graph's area; no pattern of their own).
 * TEMPLATE 9 (NEW_LAYOUT_SHEET_V2.png, ARP 2/3): drawn in SCREEN rows into the big area, the dots between rows ARP_TOP and ARP_BOT (the picture's dots, rows 68 .. 127) */
#define ARP_TOP (UT_A_GR_Y + 10)                     /* rows 58 .. 136: the old panel's rows 8 .. 86, 50 rows down (measured on the picture) */
#define ARP_BOT (UT_A_GR_Y + 88)
static int arp_drawn(const track_t *t) { uint32_t m = (uint32_t)t->p[P_AMODE]; return m != AM_OFF && m != AM_SEQ && m < 16u; }
static uint32_t arp_view_list(const track_t *t, uint32_t *list, uint32_t *cnt)
{
    uint32_t i, n, o, len = 0;
    if (t->nheld)
        return arp_list(t, list, cnt);
    n = 3u;
    for (o = 0; o < (uint32_t)clamp(t->p[P_AOCT], 1, 4); o++)
        for (i = 0; i < n; i++)
            list[len++] = 60u + (uint32_t)(i == 0u ? 0 : i == 1u ? 4 : 7) + 12u * o;
    *cnt = n;
    if (t->p[P_AMODE] == AM_REPEAT)
        list[0] = 67u, len = 1u;
    return len;
}
static uint32_t arp_view_sig(const track_t *t)
{
    uint32_t h = t->nheld * 977u, i;
    for (i = 0; i < t->nheld && i < 16u; i++)
        h = (h ^ t->held[i]) * 16777619u;
    return h;
}
static uint32_t arp_view_pick(uint32_t m, uint32_t i, uint32_t len, uint32_t *walk)   /* seq.c arp_pick, step i (from 1), RANDOM / WALK from a fixed seed */
{
    uint32_t cyc = len > 1u ? 2u * len - 2u : 1u, k = i % cyc, h = (i * 2654435761u) >> 16, w = *walk;
    switch (m) {
    case AM_DN: return len - 1u - i % len;
    case AM_UPDN: return k < len ? k : cyc - k;
    case AM_DNUP: return k < len ? len - 1u - k : k - (len - 1u);
    case AM_RND: return h % len;
    case AM_CONV:
    case AM_DIVG:
        k = i % len;
        if (m == AM_DIVG)
            k = len - 1u - k;
        return (k & 1u) ? len - 1u - k / 2u : k / 2u;
    case AM_PINKY: return len < 2u ? 0u : (k & 1u) ? len - 1u : k / 2u;
    case AM_THUMB: return len < 2u ? 0u : (k & 1u) ? 1u + k / 2u : 0u;
    case AM_WALK:
        if (i == 1u)
            w = 0;
        else if (w >= len)
            w = len - 1u;
        else if (len > 1u)
            w = !w ? 1u : w == len - 1u ? w - 1u : (h >> 7) & 1u ? w + 1u : w - 1u;
        *walk = w;
        return w;
    default: return i % len;
    }
}
static void graph_arp(const track_t *t, uint16_t c)
{
    uint32_t list[66], cnt = 0, len = arp_view_list(t, list, &cnt), m = (uint32_t)t->p[P_AMODE], i, j, walk = 0, lo = 127, hi = 0;
    int32_t gw = 2 + clamp(t->p[P_AGATE], 1, 127) * 8 / 127, sw = clamp(t->p[P_ASWING] - 50, 0, 25) * 6 / 25, rows, rh;   /* (SWING 50 .. 75 %, 50 = straight: no shift; 75: the old full 6 px) */
    uint16_t dc = t->p[P_APROB] < 127 ? T_MID : c;
    uint8_t nt[16][4], nn[16];
    if (!len)
        return;
    if (m == AM_UP8 && len < 64u)                    /* UP+8: the first note again over the top (arp_pick) */
        list[len++] = (uint32_t)clamp((int32_t)list[0] + 12 * t->p[P_AOCT], 0, 127);
    for (i = 0; i < 16u; i++) {
        nn[i] = 0;
        if (m == AM_CHORD) {                         /* CHORD: the first 4 notes, an octave up each step through OCT */
            uint32_t o = (i + 1u) % (uint32_t)clamp(t->p[P_AOCT], 1, 4);
            for (j = 0; j < cnt && j < 4u; j++)
                nt[i][nn[i]++] = (uint8_t)clamp((int32_t)list[j] + 12 * (int32_t)o, 0, 127);
        } else {
            nt[i][nn[i]++] = (uint8_t)list[arp_view_pick(m, i + 1u, len, &walk) % len];
        }
        for (j = 0; j < nn[i]; j++) {
            lo = nt[i][j] < lo ? nt[i][j] : lo;
            hi = nt[i][j] > hi ? nt[i][j] : hi;
        }
    }
    rows = (int32_t)(hi - lo) + 1;
    rh = rows <= 1 ? 4 : clamp(72 / rows, 2, 8);
    for (i = 0; i < 16u; i++) {                      /* (the step's place, a RAISE mark under each step, is not in the picture: removed) */
        int32_t x = bar_x(i) + ((i & 1u) ? sw : 0);
        for (j = 0; j < nn[i]; j++) {
            int32_t y = ARP_BOT - ((int32_t)nt[i][j] - (int32_t)lo) * rh - (rows <= 1 ? 36 : (ARP_BOT - ARP_TOP - rows * rh) / 2);
            cv_rrect(x, y - 2, gw, rh < 4 ? 3 : rh - 1, 1, dc, T_BG);
        }
    }
}

static uint32_t mv_tick(void);                       /* (the MORPH page's animation step, below) */
/* DUELL 1.1 GENERAL RULE (owner, test3: "the knob does not show the new setting until I switch to another page"): a panel is drawn again whenever ANY value it could show changed, so its signature
 * holds all of them, not the few its author thought of: every stored value of the part (P_*), every extension slot (X*) and every global (G_*). Cards are cached by the text they show
 * (ui_draw.c draw_column's key), the menu by every row's value (ui_menu.c), so a shown value never lags behind the stored one */
static uint32_t vals_hash(const track_t *t)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < P_COUNT; i++)
        h = (h ^ (uint32_t)(uint16_t)t->p[i]) * 16777619u;
    for (i = 0; i < NX; i++)
        h = (h ^ (uint32_t)(uint16_t)XV(t, i)) * 16777619u;
    for (i = 0; i < G_COUNT; i++)
        h = (h ^ (uint32_t)(uint16_t)song.g[i]) * 16777619u;
    return h;
}
static uint32_t graph_signature(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint32_t h = 2166136261u, i;
    if (off_module(pg))                              /* (item 125: the notice: nothing else on the page is drawn) */
        return (h ^ off_module(pg)) * 16777619u + grid_on;
    h ^= (uint32_t)pg->graph * 131u + TSEL->eng_req + song.sel * 7777u + ui.page * 1291u;
    if (pg->graph == GR_NONE || pg->graph == GR_TEMPO || (pg->graph == GR_ARP && !arp_drawn(t) && !AREA_PIC)) h ^= ui.frame / 2u;   /* (the scope; 0.2.25: ARP's pattern and STEREO MASTER's picture are static; TEMPLATE 9: ARP without a pattern shows the calm flat baseline, static too) */
    if (pg->graph == GR_MSTG || pg->graph == GR_COMP)   /* (0.2.23: DUEL's note; 0.2.25 layout round: STEREO MASTER's picture, COMPRESSOR's curve: the settings, the mode) */
        h = (h ^ ((duell_mode == DM_SPLIT) * 0x2545F491u + (uint32_t)song.bm * 977u + (uint32_t)song.ms[0] * 7919u + (uint32_t)song.cp[0] * 31u + (uint32_t)song.cp[1] * 104729u)) * 16777619u;
    if (pg->graph == GR_EQ)                          /* (0.2.25: the band of the knob just turned is drawn in the accent) */
        h = (h ^ (ui.hot_t ? ui.hot_col + 1u : 0u) * 2654435761u) * 16777619u;
    if (pg->graph == GR_ARP)                         /* (0.2.25: the pattern of the keys held) */
        h = (h ^ arp_view_sig(t)) * 16777619u;
    for (i = 0; i < P_COUNT; i++)
        h = (h ^ (uint32_t)t->p[i]) * 16777619u;
    if (pg->graph == GR_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_WAVE) {   /* DUELL 1.1: the WAVE picture reads the table the main loop builds a frame at a time: it is drawn again when the build is done (have), */
        const wv_mem_t *wm = (const wv_mem_t *)ENG_MEM(t - trk);                    /* not only when a knob moved: without this a TABLE change left the picture blank (test3) */
        if (wm)
            h = (h ^ ((uint32_t)wm->h.have | (uint32_t)wm->h.want << 8 | (uint32_t)wm->h.fade << 16 | (uint32_t)wv_uver[0] << 20 ^ (uint32_t)wv_uver[1] << 24 ^ (uint32_t)wv_uver[2] << 28)) * 16777619u;
    }
    if (pg->graph == GR_ENGINE)                      /* (the list while K1 turns replaces the picture) */
        h = (h ^ (ui.hot_t && ui.hot_col == 0u ? 0x51ED27u : 0u)) * 16777619u;
    if (pg->graph == GR_CHORD)                       /* (DUELL 1.1: the readout of the lowest key followed the OCT buttons; TEMPLATE 9 has no such readout, the term is harmless and kept) */
        h = (h ^ (uint32_t)song.octave * 2246822519u) * 16777619u;
    if (pg->graph == GR_FILT || pg->graph == GR_SUB || pg->graph == GR_CYCLE || pg->graph == GR_ADSR || pg->graph == GR_LFO)   /* (DUELL 1.1: the DEST pages' and LFO LEVEL's extension values too) (cloud job H: the extension slots too: TWIN LINK and ROUTE, the SUB's and CYCLE's values; the engine decides COMB on WAVE / PHYS) */
        for (i = 0; i < NX; i++)
            h = (h ^ (uint32_t)XV(t, i)) * 16777619u;
    h = (h ^ ((uint32_t)scale_g | (uint32_t)root_g << 4 | (uint32_t)offscale_g << 8)) * 16777619u;   /* (the global scale, core.h) */
    h ^= (uint32_t)TSEL->preset * 7u + (uint32_t)song.g[G_SLOT] * 13u + TSEL->user * 257u + up_gen * 7919u + list_mode() * 15485863u +
         ui.ppick * 1299709u;
    if (pg->graph == GR_CHORD) {                     /* the last chord played */
        h = (h ^ (chord_last[song.sel].root + 131u * chord_last[song.sel].mask)) * 16777619u;
        for (i = 0; i < CHORD_MAX; i++)
            h = (h ^ chord_last[song.sel].note[i]) * 16777619u;
        h ^= (uint32_t)t->engine * 389u;             /* (MONO and kits follow the sounding engine) */
        {                                            /* the live lighting: the notes the held keys' chords sound now (chord_sounding) */
            uint32_t set[4];
            (void)chord_sounding(set);
            for (i = 0; i < 4u; i++)
                h = (h ^ set[i]) * 16777619u;
        }
    }
    if (pg->graph == GR_MOD) {                       /* (the slot; DUEL or not: PAN's reason, the bus a bus value moves) */
        uint32_t mb = mod_page_base(pg), mk;
        h ^= (mod_ui_slot + 1u) * 40503u + (duell_mode_req == DM_SPLIT) * 0x01000193u;
        for (mk = 0; mk < 4u; mk++)                  /* item 152 E: the rows show the amounts as the fader sounds them (graph_mod_new) */
            h = (h ^ (uint32_t)(morph_view(song.sel, MSLOT_ID(mb + mk) + 2u) + 128)) * 16777619u;
    }
    if (pg->scope == SC_MIDI)                        /* step 6: the channels, the route, the mode, MIDI LEARN's state */
        h = (h ^ ((uint32_t)sc_midi[0] * 31u + sc_midi[1] * 977u + sc_midi[2] * 15485863u + sc_midi[3] * 7919u) * 0x85EBCA6Bu ^ ((uint32_t)midi_pch[0] | (uint32_t)midi_pch[1] << 4 | (uint32_t)(song.g[G_ROUTE] == 1) << 8 | (uint32_t)duell_mode_req << 9 |
                  (uint32_t)ui.ml.on << 11 | (uint32_t)ui.ml.pick << 12 | ml_count() << 13 | (uint32_t)ui.ml.id << 19 | (uint32_t)ui.ml.trk << 27) * 0x9E3779B1u) * 16777619u;
    if (pg->graph == GR_SCHAIN)                      /* (0.2.22: both parts' DUCK SRC are in the panel's words) */
        h = (h ^ ((uint32_t)trk[0].p[P_DKSRC] + 1u + 7u * ((uint32_t)trk[1].p[P_DKSRC] + 1u)) * 2654435761u) * 16777619u;
    if (pg->graph == GR_FXSUM)                       /* (0.2.24: which sends of the selected part are 0; item 117: the page shows the delay's mode and time and the reverb's type and size live, and the SENDS switch) */
        h = (h ^ ((uint32_t)(TSEL->p[P_CHOR] <= 0) | (uint32_t)(TSEL->p[P_DLY] <= 0) << 1 | (uint32_t)(TSEL->p[P_REV] <= 0) << 2 | 8u | (uint32_t)(mods_off & MODS_SENDS) << 4 |
                  dly_mode_ui() << 8) * 2654435761u) * 16777619u;
    if (pg->graph == GR_MODS)                        /* (item 124: the knob just turned has the accent dot; the states are in mods_get / mods_keep_get below) */
        h = (h ^ (ui.hot_t ? ui.hot_col + 1u : 0u) * 2654435761u) * 16777619u;
    if (pg->graph == GR_FXSUM)
        h = (h ^ ((uint32_t)(uint8_t)song.g[G_DTIME] | (uint32_t)(uint8_t)song.g[G_RTYPE] << 8 | (uint32_t)(uint8_t)song.g[G_RSIZE] << 16 | 0x01000000u) * 40503u) * 16777619u;
    h = (h ^ (clock_state() + 1u + clock_src() * 4u) * 3266489917u) * 16777619u;   /* (CLK: EXT / NO CLOCK, AUTO's source, on the TEMPO and MIDI pages) */
    if (pg->graph == GR_TEMPO)                       /* TEMPO's TAP card: the taps counted, over for good after 2 s */
        h = (h ^ (tempo_tapping() + 1u + (uint32_t)song.g[G_BPM] * 7u) * 2246822519u) * 16777619u;
#if DUELL_TEST_MODE
    if (pg->graph == GR_TEST)                        /* the CPU test: multi, engines, patches, the meter (it polls the counters here) */
        h = (h ^ test_sig()) * 16777619u;
#endif
    h = (h ^ (mods_get() + 1u + mods_keep_get() * 64u) * 0x85EBCA6Bu) * 16777619u;   /* (0.2.14 GLO > MODULES: the switches; the cards they fade elsewhere) */
    if (pg->scope == SC_PERF)                        /* PERFORM: the keys held and running, what can run, the knobs, the part(s) */
        h = (h ^ perf_sig()) * 16777619u;
    if (pg->scope == SC_MODE)                        /* step 5: the MODE asked for */
        h = (h ^ (duell_mode_req + 1u) * 0x2545F491u ^ ((uint32_t)mix_xf << 8 | (uint32_t)mix_split << 16) * 0x9E3779B1u) * 16777619u;
    if (pg->scope == SC_MORPH) {                     /* 4b: the faders, AMOUNT, TARGET; MORPH: the animation step */
        uint32_t pi = song.sel;
        h = (h ^ (morph_fader_of(pi) | morph_amount_of(pi) << 8 | (uint32_t)morph.target << 16 | (uint32_t)morph.cfg << 20)) * 16777619u;
        h = (h ^ morph_fader_of(1u - pi)) * 16777619u;
        if (pg->graph == GR_MORPH)                   /* v4: the picture's motion, a step every 4th frame (~16 /s) */
            h = (h ^ (mv_tick() + 1u) * 2654435761u) * 16777619u;
    }
    if (pg->graph == GR_SLOTS) {                     /* (a checksum over each slot) */
        for (i = 0; i < MU_SLOTS; i++)
            h ^= (uint32_t)project_used(i) << (8u + i);
        graph_project_used(0);
        h += graph_pname_sig;
    }
    if (pg->graph == GR_TOOLS)                       /* TOOLS: the third row (RESTORE ALL PATCHES) is dimmed while no slot holds a saved sound */
        h = (h ^ (up_count() != 0u) * 0x27D4EB2Fu) * 16777619u;
    if (pg->graph == GR_EVENTS)                      /* PARAMETER LOCKS: the rows, the one selected, + ADD's place, PLAY,
                                                      * QUANTIZE */
        h = (h ^ ev_sig()) * 16777619u;
    if (pg->scope == SC_ENGINE && (ENGINES[t->eng_req % NENGINES] == &ENG_WHEEL || t->eng_req % NENGINES == ENGI_FM6))
        h ^= (ui.hot_t ? ui.hot_col + 1u : 0u) * 65537u;
    if ((pg->scope == SC_ENGINE || fop_page(pg->graph)) && t->eng_req % NENGINES == ENGI_FM6)   /* the patch (PAT's algorithm,
                                                     * levels, FB; the operator pages: the operator, the stage) */
        h ^= (fm6_pgen[(t - trk) % NTRK] + 1u) * 2246822519u + (fop_op + 1u) * 40503u + (fop_stg + 1u) * 7919u;
    if (pg->graph == GR_ROLL) {
        uint32_t ph = song.playing ? t->seq_idx : 0xFFFFu;
        if (ph / 16u != ui.bank)
            ph = 0xFFFFu;                            /* the roll shows the cursor's bank only */
        h ^= steps_hash(t) + ph * 31u + ui.cursor * 7919u + ui.bank * 613u;
        {                                            /* the parameter locks' marks (the roll) */
            uint64_t lk = motion_lock_steps(trk_index(t));
            h = (h ^ (uint32_t)lk ^ (uint32_t)(lk >> 32) * 2654435761u) * 16777619u;
        }
        {                                            /* the roll: its view and the keys held */
            pr_follow(t);
            h = (h ^ (proll.lo + 1u)) * 16777619u;
            h = (h ^ pr_held()) * 16777619u;
        }
    }
    return h;
}
/* a list row (17 px), in M as every list's (PRESETS, USER, PATTERNS, PROJECT, PARAMETER LOCKS, MOD, the MENU): the selected
 * one a THEME bar with INK text; tag at x 14 (the widest, DRUM, to x 57), free text from x LIST_NX to x1 */
#define LIST_T3 (grid_on == 5 || grid_on == 15)   /* TEMPLATE 3's rows (item 115; template 3 itself, grid_on 8, is removed: the list pages are area pages; the ENGINE page's list while K1 turns uses them; item 165: also over LOFI's / NOISE's cells) */
#define LIST_Y(k) ((grid_on == 5 || grid_on == 15 ? UT_T2_LIST_Y : 3) + (LIST_T3 ? UT_T3_ROW_PITCH : 17) * (int32_t)(k))
#define LIST_TY CAP_IN(M, 16)                           /* a row's text: its capitals rows 2 .. 12 of the row */
#define LIST_NX 64
static void list_row(int32_t y, int sel, const char *tag, uint16_t tc, const char *name, uint16_t nc, int32_t x1)
{
    if (grid_on == 1 && y + 16 > (int32_t)H_GRAPH)        /* (DUELL 1.1: the grid's panel is 78 px: a row that does not fit whole is not drawn) */
        return;
    uint16_t bg = sel ? T_THEME : T_BG;
    if (LIST_T3) {                                         /* TEMPLATE 3 (item 115, FINAL_V1_03_patches): the row 19 px high over the card's width, the selected one in the part colour with white text, the number and the name on ONE baseline */
        int32_t ty;
        uint16_t on_part = ux_cr(T_TEXT, T_THEME) >= 300u ? T_TEXT : ux_cr(T_BG, T_THEME) >= ux_cr(T_TEXT, T_THEME) ? T_BG : T_TEXT;   /* white on the part colour where it reads 3:1, else the better of BG / TEXT (item 111's number cell) */
        if (y < 0 || y + UT_T3_ROW_H > (int32_t)cv_h)  /* (a row outside the canvas) */
            return;
        ty = UT_TY(M, y + UT_T3_TEXT_BASE);
        if (sel)
            { CV_HL(UT_T3_ROW_X, y, UT_T3_ROW_W, UT_T3_ROW_H); cv_rrect(UT_T3_ROW_X, y, UT_T3_ROW_W, UT_T3_ROW_H, UT_T3_ROW_R, T_THEME, T_BG); }
        GFX_HOOK_ALIGN(0, y, 0, y + UT_T3_ROW_H - 1, AL_V, "list row text centred up/down");
        cv_text_on(UT_T3_TAG_X, ty, &AF_M, tag, sel ? on_part : tc, bg);
        if (name[0])
            GFX_HOOK_ALIGN(0, y, 0, y + UT_T3_ROW_H - 1, AL_V, "list row text centred up/down");
        cv_free_text(UT_T3_NAME_X, ty, &AF_M, name, sel ? on_part : nc, bg, x1 - UT_T3_NAME_X);
        return;
    }
    if (sel)
        { CV_HL(6, y, 228, 16); cv_rrect(6, y, 228, 16, 4, T_THEME, T_BG); }
    GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_text_on(14, y + LIST_TY, &AF_M, tag, sel ? T_INK : tc, bg);
    if (name[0])
        GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_free_text(LIST_NX, y + LIST_TY, &AF_M, name, sel ? T_INK : nc, bg, x1 - LIST_NX);
}
/* a favourite's star on a row at y (item 115: template 3 puts it 12 px at x 214, rel 3; the others' 16 px strip) */
static void list_star(int32_t y, int sel)
{
    if (LIST_T3) {
        if (y < 0 || y + UT_T3_ROW_H > (int32_t)cv_h)
            return;
        GFX_HOOK_ALIGN(0, y, 0, y + UT_T3_ROW_H - 1, AL_V, "list row star centred up/down");
        cv_icon_in(UT_T3_STAR_X, y + UT_T3_STAR_DY, 0, 12, 12, ICON_X_STAR, sel ? (ux_cr(T_TEXT, T_THEME) >= 300u ? T_TEXT : ux_cr(T_BG, T_THEME) >= ux_cr(T_TEXT, T_THEME) ? T_BG : T_TEXT) : T_THEME, sel ? T_THEME : T_BG);
        return;
    }
    GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row star centred up/down");
    cv_icon_in(214, y, 0, 16, 12, ICON_X_STAR, sel ? T_INK : T_ACCENT, sel ? T_THEME : T_BG);
}
/* an empty list: a title and a hint, centred */
static void note_line(int32_t y, const char *s, uint16_t fg)   /* centred S; "[K2] ADD PATTERN": a key hint */
{
    uint32_t n;
    int32_t id = kc_tag(s, &n);
    if (id < 0) {
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "panel note centred");
        cv_text_in(0, y, 240, &AF_S, s, fg, T_BG);
        return;
    }
    s += n;
    while (*s == ' ')
        s++;
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "panel note hint centred");
    cv_text_on(cv_keycap(HALF_UP(240 - kh_ink((uint32_t)id, s)), y + 3, (uint32_t)id, T_KEY, T_KINK, T_BG) + KH_GAP, y,
               &AF_S, s, fg, T_BG);
}
static void panel_note(const char *a, const char *b, const char *c)
{
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "panel note centred");
    cv_text_in(0, 30, 240, &AF_M, a, T_TEXT, T_BG);
    if (b) note_line(60, b, T_MID);
    if (c) note_line(82, c, T_DIM);
}
/* the same title led by an icon (16 px, the accent's: the header message's, ui_draw.c draw_head) in the middle of
 * the panel: the title's capitals centred up and down in it, the icon's ink on them; across, the pair centred by
 * its ink */
#define PA_GAP 6                                        /* icon ink -> title ink (setup_title's) */
static void panel_alert(uint32_t id, const char *a)
{
    int32_t bi[4], bt[4], x, y = CAP_IN(M, (int32_t)H_GRAPH);
    icon_ink(16, id, bi);
    text_ink(&AF_M, a, bt);
    x = HALF_UP(240 - (bi[2] - bi[0] + PA_GAP + bt[2] - bt[0])) - bi[0];
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "panel alert centred");
    GFX_HOOK_ALIGN(0, y + AF_M_CAP_Y, 0, y + AF_M_CAP_Y + AF_M_CAP_H, AL_V | AL_PASS, "panel alert icon on its title's line");
    cv_icon_in(x, y + AF_M_CAP_Y, 0, AF_M_CAP_H, 16, id, T_ACCENT, T_BG);
    GFX_HOOK_ALIGN(0, 0, 0, (int32_t)H_GRAPH, AL_V | AL_PASS, "panel alert title in the panel's middle");
    cv_text_on(x + bi[2] + PA_GAP - bt[0], y, &AF_M, a, T_TEXT, T_BG);
}

/* TEMPLATE 9 (PATCHES_MULTIS_NEW_LAYOUT_V3.png, PATTERNS_NEW_LAYOUT.png): the list pages' five rows in the big area, SCREEN rows (draw_graph sets cv_oy): row k's top UT_L5_Y0 + k UT_L5_PITCH,
 * UT_L5_H high over x UT_L5_X .. + UT_L5_W; the number (M, dim) at UT_L5_TAG_X, the name (M, white) at UT_L5_NAME_X, both on the capitals' last row top + UT_L5_TEXT_BASE; the selected row filled
 * in the part colour with dark text (T_INK) and a white star; a favourite's star at UT_L5_STAR_X (the part colour) on PATCHES */
static void list_row5(uint32_t k, int sel, const char *tag, const char *name, uint16_t nc, int star)
{
    int32_t top = UT_L5_Y0 + UT_L5_PITCH * (int32_t)k, ty = UT_TY(M, top + UT_L5_TEXT_BASE), x1 = star ? UT_L5_STAR_X - 2 : UT_L5_X + UT_L5_W - 2;
    uint16_t bg = sel ? T_THEME : T_BG;
    if (sel)
        { CV_HL(UT_L5_X, top, UT_L5_W, UT_L5_H); cv_rrect(UT_L5_X, top, UT_L5_W, UT_L5_H, UT_T3_ROW_R, T_THEME, T_BG); }
    cv_text_on(UT_L5_TAG_X, ty, &AF_M, tag, sel ? T_INK : T_DIM, bg);
    if (name[0])
        cv_free_text(UT_L5_NAME_X, ty, &AF_M, name, sel ? T_INK : nc, bg, x1 - UT_L5_NAME_X);
    if (star)
        cv_icon_in(UT_L5_STAR_X, top + UT_T3_STAR_DY, 0, 12, 12, ICON_X_STAR, sel ? T_TEXT : T_THEME, bg);
}
/* an empty list in the area (PATCHES: no favourites, no sound of a type): the title (M, white) on the first row's text line and the hint (S, a key pill allowed) on the second's, LEFT aligned
 * at the numbers' column (change list B.5: no centred body text) */
static void list5_note(const char *a, const char *b)
{
    cv_text_on(UT_L5_TAG_X, UT_TY(M, UT_L5_Y0 + UT_L5_TEXT_BASE), &AF_M, a, T_TEXT, T_BG);
    if (b)
        cv_free_hint(UT_L5_TAG_X, UT_TY(S, UT_L5_Y0 + UT_L5_PITCH + UT_L5_TEXT_BASE), b, T_MID, T_BG, UT_L5_X + UT_L5_W - UT_L5_TAG_X);
}
/* the first row of a bounded window of UT_L5_ROWS rows over n rows with cur on the middle row where it can be */
static uint32_t list5_first(uint32_t cur, uint32_t n)
{
    uint32_t first = cur > UT_L5_ROWS / 2u ? cur - UT_L5_ROWS / 2u : 0u, last = n > UT_L5_ROWS ? n - UT_L5_ROWS : 0u;
    return first > last ? last : first;
}
/* PATCHES (SPEC 3.6): ONE list of the 128 slots around the cursor (the TYPE shown: ALL, FAV, a category or an engine). Row: the slot's number
 * (#NN), its name, a favourite starred (the accent). No row is "empty" and none is a "user" one */
static void graph_browse(void)
{
    uint32_t total, cur = pl_cursor(&total), c0;
    int32_t row;
    if (AREA_PIC) {                                     /* TEMPLATE 9: five rows, the cursor on the middle one (ALL goes round; a type is a bounded window) */
        uint32_t r;
        if (!total) {
            char b[24];
            if (list_mode() == 1u) {
                list5_note("NO FAVORITES", "TYPE ALL TO ADD SOUNDS");
                return;
            }
            str_cpy(b, "NO ", sizeof b);                /* a type with no sound (an engine whose slots were all saved over) */
            str_cpy(b + 3, list_name(list_mode()), sizeof b - 3);
            str_cpy(b + str_len(b), " SOUNDS", sizeof b - str_len(b));
            list5_note(b, "[K2] ANOTHER TYPE");
            return;
        }
        c0 = cur < total ? cur : 0u;
        for (r = 0; r < UT_L5_ROWS; r++) {
            char tag[6], nm[13];
            uint32_t index, k;
            if (!list_mode())
                index = (c0 + total * 4u + r - UT_L5_ROWS / 2u) % total;
            else {
                index = list5_first(cur < total ? cur : 0u, total) + r;
                if (index >= total)
                    continue;
            }
            k = preset_at(index);
            up_slot_label(tag, k);
            sl_name(k, nm);
            list_row5(r, index == cur, tag, nm, T_TEXT, list_fav(k));
        }
        return;
    }
    if (!total) {
        char b[24];
        if (list_mode() == 1u) {
            panel_note("NO FAVORITES", "TYPE ALL TO ADD SOUNDS", 0);
            return;
        }
        str_cpy(b, "NO ", sizeof b);                    /* a type with no sound (an engine whose slots were all saved over) */
        str_cpy(b + 3, list_name(list_mode()), sizeof b - 3);
        str_cpy(b + str_len(b), " SOUNDS", sizeof b - str_len(b));
        panel_note(b, "[K2] ANOTHER TYPE", 0);
        return;
    }
    c0 = cur < total ? cur : 0u;
    for (row = -3; row <= 3; row++) {
        int32_t y = LIST_Y(row + 3);
        char tag[6], nm[13];
        uint32_t index, k;
        int sel, fav;
        if (!list_mode()) {                              /* ALL: the list goes round */
            index = (c0 + total * 4u + (uint32_t)(row + 3) - 3u) % total;
        } else {                                         /* a type: a bounded window */
            uint32_t first = cur < total && cur > 3u ? cur - 3u : 0u, last = total > 7u ? total - 7u : 0u;
            if (first > last)
                first = last;
            index = first + (uint32_t)(row + 3);
            if (index >= total)
                continue;
        }
        sel = index == cur;
        k = preset_at(index);
        up_slot_label(tag, k);
        sl_name(k, nm);
        fav = list_fav(k);
        list_row(y, sel, tag, T_DIM, nm, T_TEXT, fav ? 212 : 232);
        if (fav)
            list_star(y, sel);
    }
}
/* the EDIT layer (ui_layer.c): the sound loaded, as the browser's selected row: its slot "#7", the name, the star of a favourite */
static void engine_sound_row(int32_t y)
{
    char tag[6], nm[13];
    uint32_t s = slot_of(TSEL);
    int fav = preset_favorite();
    if (s < UP_SLOTS) {
        up_slot_label(tag, s);
        sl_name(s, nm);
    } else {
        str_cpy(tag, "--", sizeof tag);                  /* a sound from no slot: its factory name (no factory sound: INIT) */
        str_cpy(nm, ds_name(TSEL->eng_req % NENGINES, TSEL->preset), sizeof nm);
    }
    list_row(y, 1, tag, T_DIM, nm, T_TEXT, fav ? 212 : 232);
    if (fav)
        list_star(y, 1);
}
/* EDIT's first page, ENGINE (SPEC 1.2): the engines round the selected one (the previous and the next of PRESETS'
 * order, as KNOB 1 steps; DRUM is never in it) and under them the sound loaded, as the browser's selected row */
static void graph_engine(void)
{
    uint32_t e = TSEL->eng_req % NENGINES;
    int32_t d;
    for (d = -1; d <= 1; d++) {
        const engine_t *en = ENGINES[d < 0 ? eng_step(e, -1) : d > 0 ? eng_step(e, 1) : e];
        list_row(LIST_Y((uint32_t)(d + 1)), d == 0, eng_shown(en->name), T_MID, "", T_MID, 232);   /* (DUELL 1.1, owner 2026-10-25: no cryptic tag ANLG PHAS VOCL WHEL NOIS, no ENG: the row is the full engine name, in the tag's column) */
    }
    engine_sound_row(LIST_Y(4));
}
/* SAVE > PATTERNS: the 32 slots around the one picked ("P03  OFFBEAT"; one list, every slot holds a pattern) */
static void graph_pats(void)
{
    uint32_t n = pat_count(), cur = pat_pick();
    int32_t row, first = clamp((int32_t)cur - 3, 0, (int32_t)n > 7 ? (int32_t)n - 7 : 0);
    if (AREA_PIC) {                                     /* TEMPLATE 9: five rows, the slot picked on the middle one where it can be */
        uint32_t r, f = list5_first(cur, n);
        for (r = 0; r < UT_L5_ROWS && f + r < n; r++) {
            char tag[4], nm[13];
            pat_label(f + r, tag, nm);
            list_row5(r, f + r == cur, tag, nm, T_TEXT, 0);
        }
        return;
    }
    for (row = 0; row < 7 && (uint32_t)(first + row) < n; row++) {
        uint32_t k = (uint32_t)(first + row);
        char tag[4], nm[13];
        pat_label(k, tag, nm);
        list_row(LIST_Y(row), k == cur, tag, T_MID, nm, T_TEXT, 232);
    }
}
/* SAVE > TOOLS (DUELL/release_drafts/TOOLS_NEW_LAYOUT_V2.png, owner 2026-10-29): three rows on the background, no card, no box, no knob-1 line (this is NOT an area page,
 * template 9). Each row, screen rows, the rows TOOLS_PITCH apart: the numbered knob dot (11 px, white, its digit dark) at x TOOLS_DOT_X, its top on the name's capitals' first row; the action's
 * NAME in the names face (AF_NAME: Bold 20, capitals 14 rows), left aligned at x TOOLS_NAME_X; under it the sub line (M, the 11-row face, the selected part's colour) at the same x, and the OCT+ key
 * pill (the accent: OCT+ does it) in one common column at x TOOLS_PILL_X, centred on the sub line's capitals (the pill is 13 rows, the capitals 11). Rows: CLEAR PATTERN / PART n, INITIALIZE SOUND /
 * PART n (both act on the selected part), RESTORE ALL PATCHES / EVERY SLOT REPLACED; with no saved sound in any slot the third row is dimmed whole (dot, name, sub line, pill) and its sub line
 * reads NOTHING TO RESTORE. Two strips (the canvas holds 124 rows, the body 190): rows TOOLS_Y0 .. TOOLS_Y1 - 1 and TOOLS_Y1 .. TOOLS_END - 1; every part of a row lies inside one strip.
 * The page draws its strips itself (as graph_mods): it must be called before draw_graph's shared canvas, with no cards on the page (interface request: ui_draw.c, ui.c, draw_graph) */
#define TOOLS_Y0 27             /* strip 1: rows 27 .. 119 (row 1 whole, row 2's dot and name) */
#define TOOLS_Y1 120            /* strip 2: rows 120 .. 216 (row 2's sub line and pill, row 3 whole) */
#define TOOLS_END 217           /* the body ends here; the shared footer's rows start (UT_T6_END) */
#define TOOLS_TOP0 43           /* row 1: the dot's top and the name's capitals' first row (the picture: rows 43 .. 53 the dot) */
#define TOOLS_PITCH 58          /* row to row (the picture: 43, 101, 159) */
#define TOOLS_DOT_X 9
#define TOOLS_DOT 11            /* the dot's diameter (the knob dots of every page) */
#define TOOLS_NAME_X 24
#define TOOLS_SUB_BASE 34       /* the name's capitals' first row -> the sub line's capitals' last row (the picture: 43 -> 77, 101 -> 135, 159 -> 193) */
#define TOOLS_PILL_X 164        /* the OCT+ pill's left edge, the same on every row (the picture: x 164 .. 196, rows 66 .. 78 under the sub line's capitals 67 .. 77) */
/* row k (0, 1, 2) into the strip whose first screen row is y0 and which ends before y1 (cv_oy = -y0: drawn in screen rows); only the parts that lie inside the strip */
static void tools_block(uint32_t k, int32_t y0, int32_t y1, const char *name, const char *sub, int on)
{
    int32_t top = TOOLS_TOP0 + TOOLS_PITCH * (int32_t)k, sb = top + TOOLS_SUB_BASE, pt = sb - AF_M_CAP_H - HALF_UP(KC_H - AF_M_CAP_H) + 1;
    uint16_t dc = on ? T_TEXT : T_DIM, nc = on ? T_TEXT : T_DIM, sc = on ? vt(T_THEME, T_BG) : T_DIM;
    if (top >= y0 && top + AF_NAME_CAP_H <= y1) {       /* the dot and the name */
        char d[2] = {(char)('1' + k), 0};
        cv_rrect(TOOLS_DOT_X, top, TOOLS_DOT, TOOLS_DOT, TOOLS_DOT / 2, dc, T_BG);
        cv_text_in(TOOLS_DOT_X, top + HALF_UP(TOOLS_DOT - AF_D_CAP_H) - AF_D_CAP_Y, TOOLS_DOT, &AF_DOT, d, T_BG, dc);
        cv_text_on(TOOLS_NAME_X, top - AF_NAME_CAP_Y, &AF_NAME, name, nc, T_BG);
    }
    if (pt >= y0 && pt + KC_H <= y1) {                  /* the sub line and the OCT+ pill */
        cv_text_on(TOOLS_NAME_X, UT_TY(M, sb), &AF_M, sub, sc, T_BG);
        cv_keycap(TOOLS_PILL_X, pt, KC_OCTUP, on ? T_ACCENT : T_DIM, T_KINK, T_BG);
    }
}
static void graph_tools(void)
{
    char part[16] = "PART 1";
    uint32_t s;
    int restore = up_count() != 0u;                     /* a slot holds a saved sound: RESTORE ALL PATCHES would act */
    part[5] = (char)('1' + song.sel % NTRK);
    for (s = 0; s < 2u; s++) {
        int32_t y0 = s ? TOOLS_Y1 : TOOLS_Y0, y1 = s ? TOOLS_END : TOOLS_Y1;
        cv_begin(240, (uint32_t)(y1 - y0), T_BG);
        cv_bg = T_BG;
        cv_oy = -y0;
        tools_block(0u, y0, y1, "CLEAR PATTERN", part, 1);
        tools_block(1u, y0, y1, "INITIALIZE SOUND", part, 1);
        tools_block(2u, y0, y1, "RESTORE ALL PATCHES", restore ? "EVERY SLOT REPLACED" : "NOTHING TO RESTORE", restore);
        cv_oy = 0;
        cv_blit(0, (uint32_t)y0);
    }
}
/* PARAMETER LOCKS, TEMPLATE 9 (NEW_LAYOUT_SHEET_V2.png): text rows on the background of the big area, drawn in SCREEN rows (draw_graph sets cv_oy). Columns as the picture (measured): the kind
 * (AUTO, LOCK in the accent, STEP) at LK_X, the step number at LK_SX, the full name at LK_NX, the value right-aligned ending at LK_R with its unit after it on the same baseline (S: the small face;
 * % with no space, owner rule A.2; "/16" with no space; any other unit one space after the value). QUANTIZE's row: QUANTIZE, its ON / OFF one value gap after it, NUDGED n at the right.
 * Rows LK_PITCH apart, the first row's capitals' last row LK_BASE0 (rows 58 .. 68, as the picture), four rows (the picture's four lines). The PLAY row of the list (ui_events.c EVK_TOP: ROW ALL,
 * its counts and PLAY are the cards EVENT, LOCK, PLAY) is not a text row: with it selected no row is filled (the picture). The selected row: the part colour's fill with dark text, as the list pages.
 * The old hint "HOLD A NOTE + [OCT+], TURN A KNOB" (one line and two lines) is gone (change list A.5) */
#define LK_X 9
#define LK_SX 42
#define LK_NX 66
#define LK_R 230
#define LK_BASE0 68
#define LK_PITCH 20                                  /* (measured 20 on the picture: capitals 58, 78, 98, 118; the job brief said 19: QUESTION in the report) */
#define LK_ROWS 4
#define LK_FILL_DY 15                                /* the selected row's fill: from LK_FILL_DY rows over the capitals' last row, UT_L5_H high (the list pages' row: capitals 5 rows under its top) */
static int32_t lk_value(int32_t base, const char *v, const char *unit, uint16_t fg, uint16_t bg)   /* a row's value and unit ending at LK_R; -> the value's left x */
{
    int32_t x = LK_R;
    if (unit && unit[0]) {
        x = cv_text_r(x, UT_TY(S, base), &AF_S, unit, fg, bg);
    }
    return cv_text_r(x, UT_TY(M, base), &AF_M, v, fg, bg);
}
static int32_t lk_value_w(const char *v, const char *unit)   /* the width lk_value takes */
{
    int32_t w = text_w(&AF_M, v);
    if (unit && unit[0])
        w += text_w(&AF_S, unit);
    return w;
}
static void graph_events(void)
{
    const track_t *t = TSEL;
    uint16_t rw[EV_ROWS], vis[EV_ROWS];
    uint32_t n = ev_list(rw), cur = ev_row(n), k, nv = 0;
    int32_t row, first, cv = -1;
    for (k = 0; k < n; k++)                          /* the rows shown: all but PLAY's */
        if (rw[k] >> 8 != EVK_TOP) {
            if (k == cur)
                cv = (int32_t)nv;
            vis[nv++] = rw[k];
        }
    first = clamp(cv - 1, 0, (int32_t)nv > LK_ROWS ? (int32_t)nv - LK_ROWS : 0);   /* (the selected row second from the top, as before) */
    for (row = 0; row < LK_ROWS && first + row < (int32_t)nv; row++) {
        int32_t base = LK_BASE0 + LK_PITCH * row, top = base - LK_FILL_DY;
        uint32_t c = vis[first + row];
        int sel = first + row == cv;
        uint16_t bg = sel ? T_THEME : T_BG;
        if (sel)
            { CV_HL(UT_T3_ROW_X, top, UT_T3_ROW_W, UT_L5_H); cv_rrect(UT_T3_ROW_X, top, UT_T3_ROW_W, UT_L5_H, UT_T3_ROW_R, T_THEME, T_BG); }
        if (c >> 8 == EVK_ADD) {                     /* + ADD (not in the picture: as before, QUESTION in the report) */
            ev_fix();
            cv_text_on(LK_X, UT_TY(M, base), &AF_M, ui.ev_id == EV_CHANCE ? "+ ADD CHANCE" : ui.ev_id == EV_RATCH ?
                       "+ ADD RATCHET" : ui.ev_id == EV_NUDGE ? "+ ADD NUDGE" : ui.ev_id == EV_VEL ?
                       (ui.ev_vel ? "+ HIDE VEL" : "+ SHOW VEL") : "+ ADD LOCK", sel ? T_INK : T_DIM, bg);
            /* DUELL 1.1 (HANDOFF_V2 item 35, HIDDEN_BUTTONS_V1.md): the button that does it, said on the row: turning the knobs only chooses the step and the parameter, OCTAVE + adds the lock */
            cv_free_hint(142, UT_TY(S, base), ui.ev_id == EV_VEL ? "[OCT+] TOGGLE" : "[OCT+] ADD", sel ? T_INK : T_MID, bg, 88);
            break;
        }
        if (c >> 8 == EVK_QNT) {                     /* QUANTIZE ON / OFF (1.2), the steps nudged: "NUDGED 3" */
            char b[16];
            int32_t x;
            str_cpy(b, "NUDGED ", sizeof b);
            fmt_int(b + str_len(b), (int32_t)ev_nudged(t));
            x = cv_text_on(LK_X, UT_TY(M, base), &AF_M, "QUANTIZE", sel ? T_INK : T_TEXT, bg);
            cv_text_on(x + UT_A_K1_VAL_GAP, UT_TY(M, base), &AF_M, t->p[P_SQNT] ? "ON" : "OFF", sel ? T_INK : T_THEME, bg);
            cv_text_r(LK_R, UT_TY(M, base), &AF_M, b, sel ? T_INK : T_TEXT, bg);
            continue;
        }
        if (c >> 8 != EVK_REC) {                     /* a step's CHANCE / RATCH / NUDGE / VEL: "STEP 05 CHANCE  50%" (not in the picture: the colours as before) */
            uint32_t s = c & 0xFFu, kd = c >> 8;
            int32_t v = ev_sval(&t->step[s], kd);
            uint16_t vc = (kd == EVK_NUDGE && !ev_nudge_plays(t, &t->step[s])) ||   /* (QUANTIZE ON ..) */
                          (kd == EVK_VEL && !ev_vel_plays(&t->step[s])) ? T_DIM : T_TEXT;
            char b[8];
            b[0] = (char)('0' + (s + 1u) / 10u);
            b[1] = (char)('0' + (s + 1u) % 10u);
            b[2] = 0;
            cv_text_on(LK_X, UT_TY(M, base), &AF_M, "STEP", sel ? T_INK : T_MID, bg);
            cv_text_on(LK_SX, UT_TY(M, base), &AF_M, b, sel ? T_INK : T_MID, bg);
            cv_text_on(LK_NX, UT_TY(M, base), &AF_M, kd == EVK_CHANCE ? "CHANCE" : kd == EVK_RATCH ? "RATCHET" :
                       kd == EVK_VEL ? "VEL" : "NUDGE", sel ? T_INK : vc, bg);
            if (kd == EVK_CHANCE || kd == EVK_VEL) {
                fmt_int(b, v);
            } else if (kd == EVK_RATCH) {
                b[0] = 'x';
                b[1] = (char)('0' + v);
                b[2] = 0;
            } else {                                 /* "+3" "/16" */
                ev_nudge_fmt(b, v);
            }
            lk_value(base, b, kd == EVK_CHANCE ? "%" : kd == EVK_NUDGE ? "/16" : "", sel ? T_INK : vc, bg);
            continue;
        }
        {                                            /* a recorded row: AUTO (white) or LOCK (the accent), its step in the same colour, the full name, the value */
            const motion_event_t *e = &motion.event[c & 0xFFu];
            uint32_t id = MOTION_TID(e), lock = (e->param & MOTION_LOCK) != 0u;
            char b[PV_LEN], nm[32];
            const char *unit;
            const param_desc_t *d = lock_desc(t, id);
            uint16_t kc = sel ? T_INK : lock ? T_ACCENT : T_TEXT;
            int32_t x;
            b[0] = (char)('0' + ((e->place & 63u) + 1u) / 10u);
            b[1] = (char)('0' + ((e->place & 63u) + 1u) % 10u);
            b[2] = 0;
            cv_text_on(LK_X, UT_TY(M, base), &AF_M, lock ? "LOCK" : "AUTO", kc, bg);
            cv_text_on(LK_SX, UT_TY(M, base), &AF_M, b, kc, bg);
            param_format(d, e->value, b, &unit);
            ev_dest_name(t, id, nm, 0);                  /* (1.1: the matrix's DEST line wording) */
            x = LK_R - lk_value_w(b, unit);              /* (where lk_value will put the value's left edge) */
            if (text_w(&AF_M, nm) <= x - 4 - LK_NX) {
                cv_text_on(LK_NX, UT_TY(M, base), &AF_M, nm, sel ? T_INK : T_TEXT, bg);
            } else {                                 /* (a long name: the small face; item 120: where even that is wider than the room the value leaves, it is cut 4 px before the value, never ellipsised, and listed) */
                if (text_w(&AF_S, nm) > x - 4 - LK_NX)
                    CARD_LABEL_HOOK(0, nm, text_w(&AF_S, nm), x - 4 - LK_NX);
                cv_scroll = 1;
                cv_text_on(LK_NX, UT_TY(S, base), &AF_S, nm, sel ? T_INK : T_TEXT, bg);
                cv_scroll = 0;
                cv_rect(x - 4, top, 4, UT_L5_H, bg);
            }
            lk_value(base, b, unit, sel ? T_INK : T_TEXT, bg);
        }
    }
}
/* SAVE > MULTI: the sixteen multis around the one picked ("M05  LOFI JAM"; unnamed "USED"; empty "M14  --" dim) */
static void graph_slots(void)
{
    uint32_t cur = (uint32_t)(song.g[G_SLOT] - 1) % MU_SLOTS;
    int32_t row, first = clamp((int32_t)cur - 3, 0, (int32_t)MU_SLOTS - 7);
    if (AREA_PIC) {                                     /* TEMPLATE 9: five rows, the multi picked on the middle one where it can be */
        uint32_t r, f = list5_first(cur, MU_SLOTS);
        for (r = 0; r < UT_L5_ROWS && f + r < MU_SLOTS; r++) {
            char b[4];
            int used = graph_project_used(f + r);
            const char *nmp = graph_project_name(f + r);
            mu_label(b, f + r);
            list_row5(r, f + r == cur, b, !used ? "--" : nmp[0] ? nmp : "USED", used ? T_TEXT : T_DIM, 0);
        }
        return;
    }
    for (row = 0; row < 7; row++) {
        uint32_t k = (uint32_t)(first + row);
        char b[4];
        int used = graph_project_used(k);
        const char *n = graph_project_name(k);
        mu_label(b, k);
        list_row(LIST_Y(row), k == cur, b, T_MID, !used ? "--" : n[0] ? n : "USED", used ? T_TEXT : T_DIM, 232);
    }
}
/* MIXER page: four SURF columns, one under each card: the track number on its cushion (in the accent:
 * the selected track) with a REC / ARM / MUTE badge (P_MUTE, KNOB 1), the sound's short name (a MUTE badge
 * when armed and muted), the LEVEL knob (dB inside) with the output meter beside it, then the PAN and REV
 * knobs with their values. Knobs: a 270 degree ring, the track RAISE, the value arc THEME (the knob just
 * turned: ACCENT; muted: DIM); PAN from 12 o'clock. Each column is its own canvas with its own signature:
 * while the transport runs only the meters move. */
#define TS_MY 39                                     /* meter slot: column rows 39 .. 70 */
#define TS_MH 32
#define KB_X 4                                       /* LEVEL knob: box 4..37 x 37..70, centre (21, 54) */
#define KB_Y 37
#define KS_Y 86                                      /* PAN / REV knobs: boxes 86..105, centres x 15 and 42 */
static struct {
    uint32_t col[NTRK];
    uint8_t meter[NTRK];
} ts;

static int32_t meter_px(int32_t a)                   /* |sample| (Q15) -> px: 60 dB over the slot */
{
    int32_t lg = 0, v;
    if (a < 64)
        return 0;
    while ((a >> lg) > 1)
        lg++;
    v = lg * 8 + (((a << 3) >> lg) & 7);             /* 8 log2(a): 48 (-54 dB) .. 128 (+6 dB) */
    return clamp((v - 48) * (TS_MH - 2) / 80, 0, TS_MH - 2);
}

/* a knob's ring: 270 degrees (7:30 .. 4:30 o'clock), 2 px, anti-aliased, from the quadrant mask of its size
 * (tools/gen_aa_keycaps.py: coverage and angle; the other three quadrants mirror it, no trigonometry here).
 * Box top-left (x, y), outer radius r. Angles: 1/1024 turn from 12 o'clock, clockwise, -384 .. 384; the value
 * arc is lo .. hi (in vc), the rest of the ring the track (tr); bg lies under the ring */
#define KA_END 384
static void knob_arc(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t lo, int32_t hi,
                     uint16_t tr, uint16_t vc, uint16_t bg)
{
    const uint16_t *rt = ramp(tr, bg), *rv = ramp(vc, bg);
    int32_t i, j, m;
    for (j = 0; j < r; j++)
        for (i = 0; i < r; i++) {
            uint32_t k = (uint32_t)(j * r + i), a = (cov[k >> 1] >> ((k & 1u) ? 0 : 4)) & 15u;
            int32_t q = ang[k];
            if (!a)
                continue;
            for (m = 0; m < 4; m++) {                /* top right, bottom right, bottom left, top left */
                int32_t s = m == 0 ? q : m == 1 ? 512 - q : m == 2 ? q - 512 : -q;
                int32_t px = m < 2 ? x + r + i : x + r - 1 - i, py = (m == 0 || m == 3 ? y + r - 1 - j : y + r + j) + cv_oy;
                if (s < -KA_END || s > KA_END || (uint32_t)px >= cv_w || (uint32_t)py >= cv_h)
                    continue;
                cv_px[(uint32_t)py * cv_w + (uint32_t)px] = (s >= lo && s <= hi ? rv : rt)[a];
            }
        }
}
/* value v of lo..hi as an arc: from the start, or (bipolar, lo < 0) from 12 o'clock with a 1 px nub at 0 */
static void knob(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t v, int32_t lo,
                 int32_t hi, uint16_t vc)
{
    int32_t a0, a1;
    if (lo < 0) {
        int32_t s = v * KA_END / (v < 0 ? -lo : hi);
        a0 = (s < 0 ? s : 0) - 8;
        a1 = (s > 0 ? s : 0) + 8;
    } else {
        a0 = -KA_END;
        a1 = v > lo ? -KA_END + (v - lo) * 2 * KA_END / (hi - lo) : -KA_END - 1;
    }
    knob_arc(x, y, r, cov, ang, a0, a1, T_RAISE, vc, T_BG);
}

static uint32_t trk_level(uint32_t c) { return (uint32_t)trk[c].p[P_LEVEL] & 127u; }   /* LEVEL 0..127 */

static void trk_short_name(uint32_t c, char *b)      /* the track's sound, b holds 13 */
{
    const track_t *t = &trk[c];
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    if (t->user)
        sl_name((uint32_t)t->user - 1u, b);
    else if (ds_count(t->eng_req % NENGINES))
        str_cpy(b, ds_name(t->eng_req % NENGINES, t->preset), 13);
    else
        str_cpy(b, eng_shown(e->name), 13);
}

/* oscilloscope of the output, triggered on a rising zero crossing: a RAISE centre line, the trace 2 px (in silence
 * a flat line on it; MENU > SCREEN OFF keeps it from staying on the panel for hours) */
static void graph_scope(uint16_t c)
{
    static int16_t snap[SCOPE_N] __attribute__((section(".pool")));
    uint32_t w = scope_w, i, trig = 0;
    int32_t cy = (int32_t)cv_h / 2, py = cy, x, peak = 1500, a = 46;
    for (i = 0; i < SCOPE_N; i++) {
        snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (snap[i] > peak)
            peak = snap[i];
        else if (-snap[i] > peak)
            peak = -snap[i];
    }
    for (i = 1; i < SCOPE_N - 240u; i++)
        if (snap[i - 1] < 0 && snap[i] >= 0) {
            trig = i;
            break;
        }
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
    for (x = 0; x < PANEL_W; x++) {
        int32_t y = cy - snap[trig + (uint32_t)x] * a / peak;    /* auto-scaled */
        if (x)
            cv_line_t(PANEL_X0 - 1 + x, py, PANEL_X0 + x, y, c, 2);
        py = y;
    }
}

/* HOME's MORPH (4b; v5 2026-10-11, SPEC 3.5): the morph as a picture: a field of thin wave lines across the whole width of
 * the panel's box (x 6..233, y 2..75), A at its left edge, B at its right, blending from one to the other (colour and
 * shape) across a soft front the fader sweeps: at A the whole field is A-like, at B all of it B-like; between them the
 * left is A, the right B, the front (a faint vertical glow) where they meet, moving right to left as the fader goes A -> B.
 *  - A-like: few lines, smooth single waves, the lines moving together, the A colour.
 *  - B-like: twice as many lines (the in-between ones fade in), overtones and fine ripples, each line drifting out of step
 *    with its neighbours (they cross and interleave), folded back at the box's edge, the B colour.
 * THE SOUND moves it (the part's output level, t->peak, a log level with a ~1.5 s fall; its voices' envelope and highest
 * pitch; LFO 1 and 2): the waves travel to the right as fast as it is loud and grow as tall, the pitch sets how many waves
 * span the width (eased from note to note), LFO 1 sways the field, LFO 2 shifts the lines against each other, the envelope
 * brings out the ripples. Silence: the lines lie on top of each other (one wavy line, mid-box, A..B); the level (mv_lvl, rise at once, fall ~1.6 s) spreads them apart (only their spacing; shapes and amplitudes as ever). The middle lines
 * move the most, the outer ones less and fainter. POLICE: each line solid A or B (its half). TARGET BOTH: part 1's
 * field in the upper half, part 2's in the lower, each with its own front and colours.
 * Under it the A..B track with the value in its thumb (BOTH: the other part's fader a tick), the key hints, "HOLD SAVE =
 * SAVE PATCH". Integer only; the motion is integrated a step every 4th UI frame (~16 /s, ui.frame; MENU > ANIM OFF: still) */
#define MV_BX0 6                                     /* the picture's box: x 6..233, y 2..75 */
#define MV_BX1 234
#define MV_BY0 2
#define MV_BY1 76
#define MV_NC 114                                    /* its columns, 2 px apart */
#define MV_TY 84                                     /* the track's centre line, the thumb on it */
#define MV_TX0 38
#define MV_TX1 202
#define MV_KY 95                                     /* the key hints' row */
#define MV_SY 108                                    /* "HOLD SAVE = SAVE PATCH" (its 15 px cell) */
#define MV_SOFT 30                                   /* the front's half width (px) */
static uint8_t mv_lvl[2];                            /* each part's level 0..255 (attack at once, fall 10 a step) */
static uint32_t mv_step = 0xFFFFFFFFu;               /* (the step the levels last fell at) */
typedef struct {
    uint32_t ph;                                     /* the waves' travel (2^32 a wave) */
    int32_t pit;                                     /* the pitch shown (1/16 semitone), eased toward the voices' */
    uint8_t act, env;                                /* the sound's activity, its envelope (0..255) */
    int16_t l1, l2;                                  /* LFO 1, 2 as shown (-255..255: x activity) */
} mv_part_t;
static mv_part_t mv_p[2];
static void mv_reset(void)
{
    memset(mv_p, 0, sizeof mv_p);
    mv_p[0].pit = mv_p[1].pit = 60 * 16;
    memset(mv_lvl, 0, sizeof mv_lvl);
    mv_step = 0xFFFFFFFFu;
}
static uint32_t mv_tick(void) { return (ui_prefs & PREF_ANIM_OFF) ? 0u : ui.frame >> 2; }
static void mv_levels(uint32_t tk)                  /* the parts' peaks since the last step (read and cleared, as the meters) */
{
    uint32_t k;
    for (k = 0; k < 2u; k++) {
        int32_t a = trk[k].peak, lg = 0, v = 0;
        trk[k].peak = 0;
        if (a >= 64) {
            while ((a >> lg) > 1)
                lg++;
            v = clamp((lg * 8 + (((a << 3) >> lg) & 7) - 48) * 255 / 72, 0, 255);   /* -54 .. 0 dB */
        }
        if (v >= mv_lvl[k])
            mv_lvl[k] = (uint8_t)v;
        else if (tk != mv_step)
            mv_lvl[k] = (uint8_t)(mv_lvl[k] > 10u ? mv_lvl[k] - 10u : 0u);
    }
}
/* the sound of part k this step: its voices (the highest pitch, the loudest envelope), level and LFOs; then the motion */
static void mv_listen(uint32_t k, uint32_t n)
{
    const track_t *t = &trk[k];
    mv_part_t *m = &mv_p[k];
    int32_t pit = -1, env = 0, act;
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active) {
            if (t->v[i].pitch_cur > pit)
                pit = t->v[i].pitch_cur;
            if (t->v[i].env_out > env)
                env = t->v[i].env_out;
        }
    env = clamp(env >> 7, 0, 255);
    act = env > mv_lvl[k] ? env : mv_lvl[k];
    m->act = (uint8_t)act;
    m->env = (uint8_t)env;
    m->l1 = (int16_t)((mulq15(t->lfo_val, t->lfo_fade) >> 7) * act >> 8);
    m->l2 = (int16_t)((mulq15(t->lfo2.val, t->lfo2.fade) >> 7) * act >> 8);
    if (!n)
        return;
    if (pit >= 0)                                    /* (silence: the last pitch stays) */
        m->pit += (pit - m->pit) * (int32_t)(n > 2u ? 2u : n) / 4;
    m->ph += n * ((1u << 18) + (uint32_t)act * 327680u);   /* silence: a crawl; loud: ~1/3 wave a second */
}
static int32_t mv_fold(int32_t v, int32_t lo, int32_t hi)   /* v folded back into lo .. hi (past an edge it turns round) */
{
    int32_t l = hi - lo, t;
    if (v >= lo && v <= hi)
        return v;
    t = (v - lo) % (2 * l);
    if (t < 0)
        t += 2 * l;
    return lo + (t <= l ? t : 2 * l - t);
}
static void mv_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)   /* 1 px (the box's rows) */
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy, guard = 200;
    while (guard--) {
        int32_t e2 = 2 * err;
        if (y0 >= MV_BY0 && y0 < MV_BY1)
            cv_pset(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}
/* part k's colour at pct (0 A .. 100 B), faded into the panel by fade %; POLICE: solid A or B */
static uint16_t mv_col(uint32_t k, int32_t pct, int32_t fade)
{
    const ui_pal_t *pl = &UI_PALETTES[ux.pal % NPALETTES];
    pct = clamp(pct, 0, 100);
    if (pl->hc)
        return pl->c[2u * k + (pct >= 50)];
    return ux_mix(ux_mix(pl->c[2u * k], pl->c[2u * k + 1u], pct), T_BG, clamp(fade, 0, 92));
}
/* the front's position for fader c: past the right edge at A, past the left at B (px) */
static int32_t mv_front(uint32_t c)
{
    return MV_BX1 - 1 + MV_SOFT - (int32_t)c * (MV_BX1 - 1 - MV_BX0 + 2 * MV_SOFT) / 127;
}
/* part k's field of lines between rows y0 and y1: nl lines A-like, 2 nl - 1 B-like */
static void mv_field(uint32_t k, int32_t y0, int32_t y1, uint32_t nl)
{
    const ui_pal_t *pl = &UI_PALETTES[ux.pal % NPALETTES];
    const mv_part_t *m = &mv_p[k];
    uint32_t c = morph_fader_of(k), nj = 2u * nl - 1u, j, i;
    int32_t fx = mv_front(c), note = m->pit / 16, h = y1 - y0;
    int32_t cyc = clamp((note - 24) * 256 / 16, 192, 4 * 256);   /* waves across the width: ~1 a low note .. 4 high */
    uint32_t inc = (uint32_t)(((uint64_t)(uint32_t)cyc << 24) / (uint32_t)(MV_BX1 - 1 - MV_BX0));
    int32_t ampA = (int32_t)m->act * 5 * 256 / 255, ampB = 384 + (int32_t)m->act * 11 * 256 / 255;   /* px, Q8 (B: ripples at rest) */
    int32_t rip = 64 + (int32_t)m->env * 160 / 255, sway = m->l1 * h / 2048;
    int32_t lf = clamp((int32_t)mv_lvl[k] * 256 / 200, 0, 256), ym = (y0 + y1 - 1) / 2;   /* the spread 0 (flatline) .. 256 (full) */
    uint32_t j0 = lf ? 0u : nj / 2u, j1 = lf ? nj : nj / 2u + 1u;   /* (silent: only the middle line) */
    uint8_t bq[MV_NC];
    for (i = 0; i < MV_NC; i++) {                    /* each column's B-ness 0..256: smooth across the front */
        int32_t t = clamp(((int32_t)(MV_BX0 + 2 * i) - fx + MV_SOFT) * 256 / (2 * MV_SOFT), 0, 256);
        bq[i] = (uint8_t)(t * t * (768 - 2 * t) / 65536 * 255 / 256);
    }
    if (c && c < 127u && fx > MV_BX0 && fx < MV_BX1 - 1 && !pl->hc)   /* the front: a faint glow */
        for (i = (uint32_t)y0; i < (uint32_t)y1; i += 2u)
            cv_pset(fx, (int32_t)i, mv_col(k, 50, 78));
    for (j = j0; j < j1; j++) {
        uint32_t bo = j & 1u, q;                     /* bo: a B-only line (fades in toward B) */
        int32_t yl = y0 + (int32_t)(j + 1u) * h / (int32_t)(nj + 1u), px = 0, py = 0, pen = 0;
        int32_t tap = sine_i((j + 1u) * (0x80000000u / (nj + 1u))), fade = 34 + (32767 - tap) * 36 / 32767 + (int32_t)bo * 12 + (j == nj / 2u ? 0 : (256 - lf) * 56 >> 8);   /* (stacked: the others faint) */
        uint32_t lp = j * 0x15000000u + (uint32_t)(m->l2 * (int32_t)j * (1 << 18));
        uint16_t col[9];
        yl = ym + (yl - ym) * lf / 256;
        for (q = 0; q < 9u; q++)
            col[q] = mv_col(k, (int32_t)q * 100 / 8, fade + (bo && q < 6u ? (6 - (int32_t)q) * 11 : 0));
        for (i = 0; i < MV_NC; i++) {
            int32_t b = bq[i], x = MV_BX0 + 2 * (int32_t)i, wa, wb, w, amp, y;
            uint32_t u;
            if (bo && (b < 48 || (pl->hc && b < 128))) {
                pen = 0;
                continue;
            }
            u = (uint32_t)x * inc + m->ph + lp;
            wa = sine_i(u);
            if (b) {
                uint32_t ub = u + j * 0x16000000u;   /* (the B lines out of step: a twelfth of a wave each) */
                wb = (sine_i(ub) * 3 + sine_i(2u * ub + m->ph) * 2 + sine_i(3u * ub - 2u * m->ph + j * 0x2A000000u)) / 5 +
                     ((sine_i(9u * ub + 3u * m->ph + j * 0x51000000u) >> 8) * rip >> 8);
                w = wa + ((wb - wa) * b >> 8);
            } else {
                w = wa;
            }
            amp = (ampA + ((ampB - ampA) * b >> 8)) * (tap >> 7) >> 8;   /* px Q8, the middle lines the most */
            y = yl + (int32_t)(((int64_t)w * amp) >> 23) + (sway * tap >> 15);
            y = mv_fold(y, y0, y1 - 1);
            if (pen)
                mv_line(px, py, x, y, col[(b + 16) >> 5]);
            px = x;
            py = y;
            pen = 1;
        }
    }
}
static void graph_morph(void)
{
    const ui_pal_t *pl = &UI_PALETTES[ux.pal % NPALETTES];
    uint32_t pi = song.sel, f = morph_fader_of(pi), tk = mv_tick(), both = morph.target == MT_BOTH, n;
    int32_t cx = MV_TX0 + (int32_t)(f * (uint32_t)(MV_TX1 - MV_TX0) / 127u), i;
    char b[8];
    khint_t kh[3];
    mv_levels(tk);
    n = mv_step == 0xFFFFFFFFu ? 0u : tk - mv_step;   /* (steps since the last: the motion moves on by as many) */
    mv_listen(0, n > 4u ? 4u : n);
    mv_listen(1, n > 4u ? 4u : n);
    mv_step = tk;
    if (grid_on) {   /* DUELL 1.1 HOME (PAGES_V1_home.png / PALETTES_V3_1.png): the knob mark and MORPH top left, the motion field under it, HOLD SAVE = SAVE PATCH at the bottom of the same area; no track, no thumb, no key row (the cards under are the controls) */
        const int32_t y0 = 17, y1 = 62;
        if (both) {
            mv_field(0, y0, (y0 + y1) / 2, 3u);
            mv_field(1, (y0 + y1) / 2, y1, 3u);
        } else {
            mv_field(pi, y0, y1, 5u);
        }
        {   /* the knob-1 dot and MORPH on one baseline, the note left aligned at the bottom (item 114, ui_tokens.h UT_GR_*; the dot is the label's cap height, as on every card) */
            const int32_t by = UT_GR_TITLE_BASE - UT_GR_Y, dy = by - AF_S_CAP_H + 1;
            cv_rrect(UT_GR_TITLE_X, dy, AF_S_CAP_H, AF_S_CAP_H, AF_S_CAP_H / 2, T_MID, T_BG);
            cv_text_in(UT_GR_TITLE_X, dy + HALF_UP(AF_S_CAP_H - AF_D_CAP_H) - AF_D_CAP_Y, AF_S_CAP_H, &AF_DOT, "1", T_BG, T_MID);
            cv_text_on(UT_GR_TITLE_X + UT_CARD_LBL_DX, UT_TY(S, by), &AF_S, "MORPH", T_MID, T_BG);
            cv_text_on(UT_GR_NOTE_X, UT_TY(S, UT_GR_NOTE_BASE - UT_GR_Y), &AF_S, "HOLD SAVE = SAVE PATCH", T_DIM, T_BG);
        }
        return;
    }
    if (both) {                                      /* part 1 above, part 2 below */
        mv_field(0, MV_BY0, (MV_BY0 + MV_BY1) / 2, 3u);
        mv_field(1, (MV_BY0 + MV_BY1) / 2, MV_BY1, 3u);
    } else {
        mv_field(pi, MV_BY0, MV_BY1, 5u);
    }
    for (i = MV_TX0; i <= MV_TX1; i++) {             /* the A..B track (POLICE: two solid halves) */
        int32_t p = (i - MV_TX0) * 100 / (MV_TX1 - MV_TX0);
        uint16_t col = pl->hc ? pl->c[2u * pi + (p >= 50)] : ux_mix(ux_mix(pl->c[2u * pi], pl->c[2u * pi + 1u], p), T_BG, 45);
        cv_rect(i, MV_TY - 1, 1, 2, col);
    }
    if (both && morph_fader_of(1u - pi) != f) {      /* BOTH with the faders apart: the other part's, a tick */
        int32_t ox = MV_TX0 + (int32_t)(morph_fader_of(1u - pi) * (uint32_t)(MV_TX1 - MV_TX0) / 127u);
        cv_rect(ox - 1, MV_TY - 5, 3, 10, T_PFILL(1u - pi));
    }
    GFX_HOOK_ALIGN(0, MV_TY - 7, 0, MV_TY + 8, AL_V, "morph end letter on the track");
    cv_text_on(14, MV_TY - 7 + CAP_IN(S, 15), &AF_S, "A", T_MID, T_BG);
    GFX_HOOK_ALIGN(0, MV_TY - 7, 0, MV_TY + 8, AL_V, "morph end letter on the track");
    cv_text_r(226, MV_TY - 7 + CAP_IN(S, 15), &AF_S, "B", T_MID, T_BG);
    fmt_int(b, (int32_t)f);
    cv_rrect(cx - 14, MV_TY - 7, 29, 15, 7, T_PFILL(pi), T_BG);   /* the thumb: the value in the fader's colour */
    GFX_HOOK_ALIGN(cx - 14, MV_TY - 7, cx + 15, MV_TY + 8, AL_HV, "morph value in its thumb");
    cv_text_in(cx - 14, MV_TY - 7 + CAP_IN(S, 15), 29, &AF_S, b, T_INK, T_PFILL(pi));
    kh[0] = (khint_t){KC_K2, "ROLL A|B"};
    kh[1] = (khint_t){KC_K3, "UNDO A|B"};
    kh[2] = (khint_t){KC_K4, "AMT"};             /* (AMOUNT: 5 px too wide for the row) */
    cv_key_row(12, 228, MV_KY, kh, 3, 7u, T_BG);
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "morph save line centred");
    cv_text_in(0, MV_SY, 240, &AF_S, "HOLD SAVE = SAVE PATCH", T_DIM, T_BG);
}
/* HOME's MODE (step 5, SPEC 5b.1): the mode large, then XFADE and SPLIT each in a plain line with its value (XFADE and
 * SPLIT work in LAYER only: DIM in the other modes); the voices: LAYER takes a voice of each part a note, so half as many notes */
static void mode_line(int32_t y, const char *name, const char *val, const char *what, uint32_t on)
{
    cv_text_on(12, y, &AF_S, name, on ? T_TEXT : T_DIM, T_BG);
    cv_text_r(228, y, &AF_S, val, on ? T_TEXT : T_DIM, T_BG);
    cv_text_on(12, y + 13, &AF_S, what, on ? T_MID : T_DIM, T_BG);
}
static void graph_mode(void)
{   /* DUELL 1.1 (PAGES_V2_home-modes.png), item 118: the three modes as a list in the order LAYER, STEREO, DUEL (the SETUP page's), the picked one on a box of the selected part's colour (ui_tokens.h TEMPLATE 6: the selection style of
     * FINAL_V1_08_setup) with the same room above and below its text, never overlapping the next row; what it means under the list; the knob mark in front of everything */
    static const char *const NM[3] = {"LAYER", "STEREO", "DUEL"}, *const WH[3] = {"Both parts combined: velocity crossfade, key split", "Both parts in one stereo field", "Dual mono, for external effects"};
    uint32_t m = duell_mode_req < DM_N ? duell_mode_req : DM_SPLIT, i, sel = (uint32_t)(DM_LAYER - (int32_t)m);   /* (row 0 is LAYER = mode 2, row 2 is DUEL = mode 0) */
    int32_t o = cv_oy;
    const int32_t RH = 25, RP = 26, RY = 4;            /* a row: its box 25 px high, 26 px apart (a gap of 1), the first at y 4 */
    uint16_t tint = ux_mix(T_BG, T_PTHEME(song.sel), UT_T6_SEL_PCT);
    cv_oy = 0;
    for (i = 0; i < 3u; i++) {
        int32_t y = RY + RP * (int32_t)i;
        if (i == sel)
            { CV_HL(26, y, 206, RH); cv_card(26, y, 206, RH, tint, T_BG); }
        cv_text_on(34, y + HALF_UP(RH - AF_L_CAP_H) - AF_L_CAP_Y, &AF_L, NM[i], T_TEXT, i == sel ? tint : T_BG);   /* (the capitals centred in the box: equal room above and below) */
    }
    guide_wrap(12, sel == 0u ? 83 : 86, 216, &AF_M, WH[sel], T_TEXT, 15);   /* (LAYER's two lines end 4 px above the card's edge, the room the picked box has above it) */
    cv_rrect(8, RY + HALF_UP(RH - 11), 11, 11, 5, T_TEXT, T_BG);                           /* the knob mark, last: in front */
    cv_text_in(8, RY + HALF_UP(RH - 11) + HALF_UP(11 - AF_D_CAP_H) - AF_D_CAP_Y, 11, &AF_DOT, "1", T_BG, T_TEXT);
    cv_oy = o;
}
/* a GLO page's line in M (0.2.24, owner: less tiny text, bigger labels): the name left, the value right, in the primary text colour; dim: both dim */
static void big_line(int32_t y, const char *name, const char *val, uint32_t on)
{
    cv_text_on(12, y, &AF_M, name, on ? T_TEXT : T_DIM, T_BG);
    if (val)
        cv_text_r(228, y, &AF_M, val, on ? vt(T_THEME, T_BG) : T_DIM, T_BG);
}
/* GLO > MIDI (0.2.24, owner: the repeated lines at the bottom are gone, the labels bigger): each part's channel (M) with where it is heard (S) under it, MIDI IN (PART CHANNEL / ANY CHANNEL)
 * with one line of what that means; a warning when both parts share a channel */
static void graph_midi(void)
{
    uint32_t m = duell_mode_req < DM_N ? duell_mode_req : DM_SPLIT, sel = song.g[G_ROUTE] == 1, same = midi_pch[0] == midi_pch[1], k;
    char v[12];
    static const char *const WHERE[3][2] = {{"DUEL: LEFT OUTPUT", "DUEL: RIGHT OUTPUT"}, {"STEREO OUTPUT", "STEREO OUTPUT"},
                                             {"LAYER: PLAYS BOTH PARTS", "LAYER: NOT HEARD, USE P1 CH"}};
    for (k = 0; k < 2u; k++) {
        uint32_t on = 1;   /* (item 86: the two channels always work, never faded) */
        str_cpy(v, "CH ", sizeof v);
        fmt_int(v + 3, (int32_t)midi_ch_get(k) + 1);
        big_line(6 + 34 * (int32_t)k, k ? "PART 2" : "PART 1", v, on);
        cv_text_on(12, 24 + 34 * (int32_t)k, &AF_S, WHERE[m][k], on ? T_MID : T_DIM, T_BG);
    }
    big_line(74, "MIDI IN", sel ? "ANY CHANNEL" : "PART CHANNEL", 1);
    if (m == DM_LAYER && !sel)                    /* 1.1: both channels stay editable in LAYER; the note says which one counts */
        cv_text_on(12, 92, &AF_S, "LAYER MODE: PART 1 CHANNEL COUNTS", T_ACCENT, T_BG);
    else
        cv_text_on(12, 92, &AF_S, sel ? "ANY CHANNEL PLAYS THE SELECTED PART" : same ? "SAME CHANNEL: ONLY PART 1 PLAYS" : "EACH PART ON ITS OWN CHANNEL", same && !sel ? T_ACCENT : T_MID, T_BG);
}
/* GLO > MODULES (DUELL 0.2.14; item 124, owner: the page could not be found, its title said what ARP did): the title is MODULES (the header's), the page is four plain lines on the background, no cards and
 * no boxes: the numbered knob dot (white; the accent while that knob was just turned), the module's name (S, grey) and its state (M, the selected part's colour): ARP, SEQ: OFF, ON (follows the patch) or
 * KEEPS RUNNING (through patch changes: the third value those two knobs have), SENDS, PART FX: ON or OFF. Two strips (the canvas holds 124 rows, the page 189): rows 27 .. 121 and 122 .. 216 */
static void dot_label(uint32_t c, int32_t dx, int32_t lx, int32_t base, const char *label, uint16_t dc, uint16_t lc, int32_t room, uint16_t under);   /* (ui_draw.c) */
#define MOD_Y0 27
#define MOD_Y1 122
#define MOD_END 217
static void graph_mods(void)
{
    uint32_t m = mods_get(), k, s;
    uint16_t grey = ux_mix(T_BG, T_TEXT, UT_GREY_PCT), vc = vt(T_PTHEME(song.sel), T_BG);
    for (s = 0; s < 2u; s++) {
        int32_t y0 = s ? MOD_Y1 : MOD_Y0;
        cv_begin(240, (uint32_t)((s ? MOD_END : MOD_Y1) - y0), T_BG);
        cv_bg = T_BG;
        cv_oy = 0;
        for (k = 2u * s; k < 2u * s + 2u; k++) {
            int32_t base = UT_MODS_BASE0 + UT_MODS_PITCH * (int32_t)k - y0;
            uint32_t rv = k < 2u ? mods_run_get(k) : ((m >> k) & 1u) ? 0u : 1u;
            int hot = k == ui.hot_col && ui.hot_t;
            dot_label(k, UT_MODS_DOT_X, UT_MODS_LBL_X, base, MODS_NAME[k], hot ? T_ACCENT : T_TEXT, hot ? T_ACCENT : grey, UT_MODS_VAL_X - UT_MODS_LBL_X - 4, T_BG);
            cv_text_on(UT_MODS_VAL_X, UT_TY(M, base), &AF_M, rv == 0u ? "OFF" : rv == 1u ? "ON" : "KEEPS RUNNING", vc, T_BG);
        }
        cv_blit(0, (uint32_t)y0);
    }
}
/* FX > SEND FX, TEMPLATE 5 (item 117; FINAL_V1_04_fx-send, ui_tokens.h UT_T5_*): text on the background, no card, box or bar, left aligned, every row on its own baseline: the state (ALL THREE ACTIVE, or the effects this part
 * sends nothing to, or SENDS OFF) top left; DELAY: the mode and TIME, REVERB: the type and SIZE (labels small grey, values in the part's colour, dim for an effect this part does not feed: the real settings, live);
 * a grey note that a twist jumps to the effect's GLO page (ui_input.c fxsum_knob); at the bottom the three knob labels with their numbered dots. No chorus line: the chorus has no setting here. Two strips (the canvas
 * holds 124 rows, the page 189): rows 27 .. 143 and 144 .. 215; the signature (graph_signature) holds the sends, the modules switch and the four settings shown */
#define FXS_Y0 27
#define FXS_Y1 144
#define FXS_END 216
static void graph_fxsum(void)
{
    static const char *const NM[3] = {"CHORUS", "DELAY", "REVERB"};
    uint32_t c, n = 0, off[3], sends_off = (mods_off & MODS_SENDS) != 0u;
    char st[40], tm[PV_LEN], sz[PV_LEN];
    const char *unit2, *u1, *u2, *dm, *rt;
    uint16_t grey = ux_mix(T_BG, T_TEXT, UT_GREY_PCT);
    for (c = 0; c < 3u; c++) {
        off[c] = sends_off || morph_view(song.sel, FXSUM_ID[c]) <= 0;   /* the send as the fader sounds it (as the FX page's cards show it), not the stored Original */
        n += off[c];
    }
    if (sends_off) {
        str_cpy(st, "SENDS OFF (MODULES)", sizeof st);
    } else if (!n) {
        str_cpy(st, "ALL THREE ACTIVE", sizeof st);
    } else if (n == 3u) {
        str_cpy(st, "ALL THREE INACTIVE", sizeof st);
    } else {                                            /* "CHORUS INACTIVE", "CHORUS, REVERB INACTIVE": the effects this part's send of 0 leaves out */
        st[0] = 0;
        for (c = 0; c < 3u; c++)
            if (off[c]) {
                if (st[0])
                    str_cpy(st + str_len(st), ", ", 3);
                str_cpy(st + str_len(st), NM[c], 8);
            }
        str_cpy(st + str_len(st), " INACTIVE", 10);
    }
    dm = N_DMODE_FULL[dly_mode_ui() & 3u];             /* (DUELL 1.1: the full word, not DIGI PING RAND OCT+) */
    param_format(glob_desc(G_DTIME), song.g[G_DTIME], tm, &unit2);
    u1 = unit2 + (unit2[0] == ' ');                     /* (the unit is drawn small after the value, no gap, no space) */
    rt = val_shown(glob_desc(G_RTYPE), N_RTYPE[(uint32_t)clamp(song.g[G_RTYPE], 0, 5)]);   /* (SHIMMER, RESONATOR in full) */
    param_format(glob_desc(G_RSIZE), song.g[G_RSIZE], sz, &unit2);
    u2 = unit2 + (unit2[0] == ' ');
    cv_begin(240, FXS_Y1 - FXS_Y0, T_BG);
    cv_bg = T_BG;
    cv_oy = 0;
    cv_text_on(UT_T5_X, UT_TY(M, UT_T5_STATE_BASE - FXS_Y0), &AF_M, st, T_MID, T_BG);
    for (c = 1; c < 3u; c++) {                          /* DELAY (K2) and REVERB (K3): label, value, label, value */
        int32_t b = (c == 1u ? UT_T5_ROW1_BASE : UT_T5_ROW2_BASE) - FXS_Y0;
        uint16_t vc = off[c] ? T_DIM : vt(T_THEME, T_BG);
        int32_t x;
        const char *u = c == 1u ? u1 : u2;
        cv_text_on(UT_T5_X, UT_TY(S, b), &AF_S, NM[c], T_MID, T_BG);
        cv_text_on(UT_T5_VAL_X, UT_TY(T, b), &AF_T, c == 1u ? dm : rt, vc, T_BG);   /* mode / type: one size for every name (rule C2: the same Bold, smaller; RESONATOR 74 px of the 82) */
        cv_text_on(UT_T5_LBL2_X, UT_TY(S, b), &AF_S, c == 1u ? "TIME" : "SIZE", T_MID, T_BG);
        x = cv_text_on(UT_T5_VAL2_X, UT_TY(L, b), &AF_L, c == 1u ? tm : sz, vc, T_BG);
        if (u[0])
            cv_text_on(x, UT_TY(M, b), &AF_M, u, off[c] ? T_DIM : T_MID, T_BG);   /* the unit: the cells' unit face and colour, no gap */
    }
    cv_blit(0, FXS_Y0);
    cv_begin(240, FXS_END - FXS_Y1, T_BG);
    cv_bg = T_BG;
    cv_oy = 0;
    cv_text_on(UT_T5_X, UT_TY(S, UT_T5_NOTE_BASE1 - FXS_Y1), &AF_S, "TURN A KNOB: JUMP TO GLOBAL", grey, T_BG);
    cv_text_on(UT_T5_X, UT_TY(S, UT_T5_NOTE_BASE2 - FXS_Y1), &AF_S, "EFFECT SETTINGS", grey, T_BG);
    for (c = 0; c < 3u; c++) {
        int32_t dx = UT_T5_KNOB_X0 + UT_T5_KNOB_DX * (int32_t)c;
        dot_label(c, dx, dx + UT_T5_KNOB_LBL_DX, UT_T5_KNOB_BASE - FXS_Y1, NM[c], T_TEXT, T_MID, 240 - 2 - dx - UT_T5_KNOB_LBL_DX, T_BG);
    }
    cv_blit(0, FXS_Y1);
}
/* A page of a function GLO > MODULES has switched OFF (item 125; ui.c off_module): the message in M, under it the grey note in S, top left on the background, wrapped at spaces to UT_OFF_W, left aligned
 * (ui_tokens.h UT_OFF_*). bit: the MODS_* that is off. Used by draw_graph (the page) and draw_layer (QUICK ARP, QUICK SEQ) */
static uint32_t off_wrap(const aafont_t *f, const char *s, char out[4][48])
{
    uint32_t n = 0, i = 0, w0 = 0;
    char cur[48];
    cur[0] = 0;
    while (s[i] && n < 4u) {
        char word[32], t[48];
        uint32_t k = 0;
        while (s[i] && s[i] != ' ' && k + 1u < sizeof word)
            word[k++] = s[i++];
        word[k] = 0;
        while (s[i] == ' ')
            i++;
        str_cpy(t, cur, sizeof t);
        if (t[0])
            str_cpy(t + str_len(t), " ", 2);
        str_cpy(t + str_len(t), word, (uint32_t)sizeof t - str_len(t));
        if (cur[0] && text_w(f, t) > UT_OFF_W) {
            str_cpy(out[n++], cur, 48);
            str_cpy(cur, word, sizeof cur);
        } else {
            str_cpy(cur, t, sizeof cur);
        }
    }
    (void)w0;
    if (cur[0] && n < 4u)
        str_cpy(out[n++], cur, 48);
    return n;
}
static void graph_off(uint32_t bit)
{
    char ml[4][48], nl[4][48];
    uint32_t nm, nn, i;
    int32_t base = UT_OFF_MSG_BASE;
    uint16_t grey = ux_mix(T_BG, T_TEXT, UT_GREY_PCT);
    nm = off_wrap(&AF_M, bit == MODS_ARP ? UT_OFF_TXT_ARP : bit == MODS_SEQ ? UT_OFF_TXT_SEQ : bit == MODS_SENDS ? UT_OFF_TXT_SENDS : UT_OFF_TXT_PFX, ml);
    nn = off_wrap(&AF_S, bit == MODS_ARP || bit == MODS_SEQ ? UT_OFF_NOTE_IT : UT_OFF_NOTE_THEM, nl);
    cv_begin(240, UT_OFF_H, T_BG);
    cv_bg = T_BG;
    cv_oy = 0;
    for (i = 0; i < nm; i++, base += UT_OFF_MSG_PITCH)
        cv_text_on(UT_OFF_X, UT_TY(M, base - UT_OFF_Y0), &AF_M, ml[i], T_MID, T_BG);
    base += UT_OFF_NOTE_GAP - UT_OFF_MSG_PITCH;
    for (i = 0; i < nn; i++, base += UT_OFF_NOTE_PITCH)
        cv_text_on(UT_OFF_X, UT_TY(S, base - UT_OFF_Y0), &AF_S, nl[i], grey, T_BG);
    cv_blit(0, UT_OFF_Y0);
}
/* GLO > SIDECHAIN (0.2.22; 0.2.24, owner: far too much text, it repeated the cards): the note on top (when the setup counts), then each part's DUCK SRC as it is now
 * ("PART 1  PUMP", "PART 2  MIDI", "PART 2  OFF": the part the setup applies to is the one on MIDI); the cards above hold the channels and notes themselves */
static void graph_schain(void)
{
    uint32_t k, on = (mods_off & MODS_PFX) ? 0u : 1u;
    cv_text_on(12, 4, &AF_M, "MIDI SETTINGS", on ? T_TEXT : T_DIM, T_BG);
    cv_text_on(12, 22, &AF_S, "USED ONLY WHEN A PATCH USES MIDI", on ? T_MID : T_DIM, T_BG);
    for (k = 0; k < NSHOWN; k++) {
        char sn[16];
        one_line(sn, sizeof sn, val_shown(&TP[P_DKSRC], N_DKSRC[(uint32_t)clamp(trk[k].p[P_DKSRC], 0, 3)]));   /* (owner 2026-10-25: TEMPO PUMP, OTHER PART, MIDI NOTE) */
        big_line(46 + 26 * (int32_t)k, k ? "PART 2" : "PART 1", sn, on && trk[k].p[P_DKSRC] == 2);
    }
    if (!on)
        cv_text_on(12, 102, &AF_S, "PART FX OFF (MODULES): NO DUCK", T_DIM, T_BG);
}
/* MIDI LEARN (step 6, midi_learn.c): its state in words: OFF, ON (turn a knob), a parameter picked (move a CC) */
/* DUELL 1.1 MIDI LEARN (PAGES_V2_glo-b.png): the two cards above (grid_on 6), the steps under them as plain lines with the knob marks; panel rows from y 86 */
static void knob_mark(int32_t x, int32_t y, uint32_t n)
{
    char d[2] = {(char)('0' + n), 0};
    cv_rrect(x, y, 11, 11, 5, T_TEXT, T_BG);
    cv_text_in(x, y + HALF_UP(11 - AF_D_CAP_H) - AF_D_CAP_Y, 11, &AF_DOT, d, T_BG, T_TEXT);
}
static void graph_mlearn(void)
{
    char a[56], b[32];
    if (!ui.ml.on) {
        knob_mark(12, 12, 1);
        cv_text(30, 10, &AF_M, "Turn right to start", T_TEXT);
        knob_mark(12, 34, 2);
        cv_text(30, 32, &AF_M, "Turn right to clear all", T_TEXT);
        fmt_int(b, (int32_t)ml_count());
        str_cpy(b + str_len(b), " CC learned", 14);
        cv_text(12, 62, &AF_M, b, T_MID);
    } else if (!ui.ml.pick) {
        cv_text(12, 10, &AF_M, "1. Turn any knob to pick it", T_TEXT);
        cv_text(12, 32, &AF_M, "2. Send a CC from your controller", T_TEXT);
    } else {
        ml_name(a);
        cv_text_grad(12, 8, &AF_L, a, 12);
        cv_text(12, 38, &AF_M, "Send a CC", T_TEXT);
        cv_free_hint(12, 60, "[OCT+] Done", T_TEXT, T_BG, 200);
        cv_free_hint(12, 80, "[OCT-] Clears its CC", T_TEXT, T_BG, 200);
    }
}
/* MORPH SET: what TARGET means now (in words), and SMART, ARP and PITCH */
static void graph_morphset(void)
{   /* DUELL 1.1 (TEXT_STRUCTURE_V1_A.png, MORPH SET): the four facts as a table, the cards SMART, ARP, PITCH under it */
    uint32_t pi = song.sel;
    const char *t = morph.target == MT_BOTH ? "Moves both" : pi ? "Moves part 2" : "Moves part 1";
    static const char *const K[4] = {"MORPH", "SMART", "ARP", "PITCH"};
    const char *v[4];
    uint32_t i;
    v[0] = t;
    v[1] = morph.cfg & MC_WILD ? "Anything may change" : "Nothing stupid";
    v[2] = morph.cfg & MC_ARP ? "On" : "Off";
    v[3] = morph.cfg & MC_PIT ? "On" : "Off";
    for (i = 0; i < 4u; i++) {
        cv_text(12, 2 + 18 * (int32_t)i, &AF_S, K[i], T_MID);          /* (item 119: four rows of 18 px in the 76 px card, the label and its text on one baseline: S and M 15 have the same capitals) */
        cv_text(72, 2 + 18 * (int32_t)i, &AF_M, v[i], T_TEXT);
    }
}

/* TEMPLATE 7 (item 118, FINAL_V1_07_sequencer): the roll's card, rows UT_T7_Y .. +UT_T7_H (147: the canvas holds 124), drawn in two strips with the roll's own screen coordinates */
#define T7_STRIP 101                                 /* strip B's first screen row */
static void graph_roll_t7(const track_t *t, uint16_t c)
{
    uint32_t p;
    for (p = 0; p < 2u; p++) {
        int32_t y0 = p ? T7_STRIP : UT_T7_Y, y1 = p ? UT_T7_Y + UT_T7_H : T7_STRIP;
        cv_begin(240, (uint32_t)(y1 - y0), T_BG);
        cv_bg = T_BG;
        cv_oy = -y0;
        graph_roll(t, c);
        cv_oy = 0;
        cv_blit(0, (uint32_t)y0);
    }
}
/* SEQUENCER HELP (item 118, FINAL_V1_07b_sequencer_help): eight rows of the roll's symbols with their words and plain meanings, on the background; two strips (rows 24 .. 140, 140 .. 217).
 * The symbol is drawn with the roll's own colours (the notes dim, the accent white, the lock mark and the cursor as the roll draws them) */
static void graph_seqhelp(void)
{
    static const char *const NAME[8] = {"PLAYS", "ACCENT", "TIE", "SLIDE", "LOCKED", "CURSOR", "SCALE", "KEYS"};
    static const char *const MEAN[8] = {"SOUNDS A PITCH", "PLAYED LOUDER", "HOLDS ON LONGER", "GLIDES INTO NEXT", "HAS ITS OWN VALUES", "WHERE YOU EDIT", "ROWS IN THE KEY", "LIKE A PIANO"};
    uint32_t p, k;
    uint16_t grey = ux_mix(T_BG, T_TEXT, UT_GREY_PCT), note = ux_mix(T_BASE, T_THEME, UT_T7_NOTE_PCT);
    for (p = 0; p < 2u; p++) {
        int32_t y0 = p ? 140 : H_HEAD, y1 = p ? 217 : 140;
        cv_begin(240, (uint32_t)(y1 - y0), T_BG);
        cv_bg = T_BG;
        cv_oy = -y0;
        for (k = 0; k < 8u; k++) {
            int32_t top = UT_T7_HELP_Y0 + UT_T7_HELP_PITCH * (int32_t)k, cy = top + 5, x0 = UT_T7_HELP_SYM_X, base = top + 11;
            switch (k) {
            case 0: cv_rect(x0, cy - 2, 12, 5, note); break;                               /* PLAYS: a note's bar */
            case 1: cv_rect(x0, cy - 3, 12, 7, T_TEXT); break;                             /* ACCENT: the row's whole height, white */
            case 2: cv_rect(x0, cy - 2, 12, 5, note); cv_rect(x0 + 12, cy - 2, 3, 5, note); break;   /* TIE: the bar carried on */
            case 3: cv_line(x0, cy + 4, x0 + 12, cy - 4, T_TEXT); break;                   /* SLIDE: the diagonal */
            case 4: cv_rect(x0 + 1, cy - 1, 9, 3, T_THEME); break;                         /* LOCKED: the lock mark */
            case 5: cv_frame(x0, cy - 5, 12, 11, T_TEXT); break;                           /* CURSOR: the frame */
            case 6: cv_rect(x0, cy - 3, 14, 7, PR_LANE); break;                             /* SCALE: a lit row */
            default: cv_rect(x0, cy - 4, 12, 4, PR_KEYW); cv_rect(x0, cy + 1, 7, 4, PR_KEYB); break;   /* KEYS: a white and a black key */
            }
            cv_text_on(UT_T7_HELP_WORD_X, UT_TY(S, base), &AF_S, NAME[k], T_MID, T_BG);
            cv_text_on(UT_T7_HELP_MEAN_X, UT_TY(M, base), &AF_M, MEAN[k], grey, T_BG);
        }
        cv_oy = 0;
        cv_blit(0, (uint32_t)y0);
    }
}
/* the ENGINE page's list while K1 turns: the engine before, the engine, the engine after (template 3's rows); grid_on 5 in the picture's place, 15 (item 165) in the top cell's */
static void graph_eng_list(const track_t *t)
{
    uint32_t e = t->eng_req % NENGINES;
    int32_t d;
    cv_oy = 0;
    for (d = -1; d <= 1; d++)
        list_row(LIST_Y((uint32_t)(d + 1)), d == 0, eng_shown(ENGINES[d < 0 ? eng_step(e, -1) : d > 0 ? eng_step(e, 1) : e]->name), T_MID, "", T_MID, 232);
}
static void draw_graph(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint16_t c = ACC;
    uint32_t sig;
    sig = graph_signature();
    if (!ui.force && sig == ui.graph_sig) {
        if (pg->graph == GR_COMP)                    /* (0.2.25: the gain reduction bar, its own strip, each frame) */
            comp_meter();
        return;
    }
    ui.graph_sig = sig;
    if (grid_on == 2)                                /* DUELL 1.1: the cards carry the text, no panel */
        return;
    if (grid_on == 14)                               /* TEMPLATE 8 (item 156): the CELL pages have no picture (the calm curve is gone), their cells fill the body */
        return;
    if (grid_on == 15) {                             /* item 165 (owner): the ENGINE page of LOFI / NOISE has no picture; while K1 turns, the engine list in the top cell's place (ui_draw.c leaves that cell undrawn) */
        if (ui.hot_t && ui.hot_col == 0u) {
            cv_begin(240, H_GRAPH, T_BG);
            cv_bg = T_BG;
            graph_eng_list(t);
            cv_oy = 0;
            cv_blit(0, Y_GRAPH);
        }
        return;
    }
    if (grid_on == 13) {                             /* a page whose module is OFF (item 125): the notice */
        graph_off(off_module(pg));
        return;
    }
    if (grid_on == 12) {                             /* MODULES (item 124): four lines on the background, two strips */
        graph_mods();
        return;
    }
    if (grid_on == 17) {                             /* TOOLS: three rows on the background, in their own strips in screen rows (graph_tools) */
        graph_tools();
        return;
    }
    if (grid_on == 4) {                              /* the MATRIX rows (graph_mod_new) */
        graph_mod_new(t);
        return;
    }
    if (AREA_PIC && pg->graph != GR_PERF) {          /* TEMPLATE 9 (LAYOUT_IMPLEMENTATION_PLAN.md J7): the page's picture into the big area, one canvas of the area's rows blitted at its first row;
                                                      * every picture here draws in SCREEN rows (cv_oy = minus the area's first row). PERFORM keeps its own path below (ui_perform.c) */
        int32_t ay, ah;
        area_rect(&ay, &ah);
        cv_begin(240, (uint32_t)ah, T_BG);
        cv_bg = T_BG;
        cv_oy = -ay;
        if (pg->graph == GR_ARP) {
            if (arp_drawn(t))
                graph_arp(t, c);
            else                                     /* MODE OFF or SEQ: the calm flat baseline across the area's middle (the picture), not the live output scope */
                cv_rect(0, ay + ah / 2, 240, 1, T_RAISE);
        } else if (pg->graph == GR_EVENTS)
            graph_events();
        else if (pg->graph == GR_ROLL)
            graph_roll(t, c);
        else if (pg->scope == SC_SCALE || pg->graph == GR_SCALE)
            graph_scale(t, c);
        else if (pg->graph == GR_CHORD)
            graph_chord(t, c);
        else if (pg->graph == GR_BROWSE)
            graph_browse();
        else if (pg->graph == GR_PATS)
            graph_pats();
        else if (pg->graph == GR_SLOTS)
            graph_slots();
        cv_oy = 0;
        cv_blit(0, (uint32_t)ay);
        return;
    }
    if (pg->graph == GR_ROLL && grid_on == 10) {     /* TEMPLATE 7 (item 118): the STEP page's roll card in two strips */
        graph_roll_t7(t, c);
        return;
    }
    if (pg->graph == GR_SEQHELP) {                   /* SEQUENCER HELP (item 118): the legend, on the background */
        graph_seqhelp();
        return;
    }
    if (pg->graph == GR_FXSUM) {                     /* TEMPLATE 5 (item 117): the SENDS page, text on the background in two strips */
        graph_fxsum();
        return;
    }
    cv_begin(240, H_GRAPH, T_BG);
    cv_bg = T_BG;                                    /* (text drawn with cv_text lands on it) */
    cv_oy = GOY;                                     /* graphs on a 100 px scale */
    switch (pg->graph) {
    case GR_ADSR:
        graph_adsr(t, c);
        break;
    case GR_LFO:
        graph_lfo(t, c);
        break;
    case GR_ROLL:
        cv_oy = 0;
        graph_roll(t, c);
        break;
    case GR_SCALE:
        graph_scale(t, c);
        break;
    case GR_CHORD:
        cv_oy = 0;
        graph_chord(t, c);
        break;
    case GR_FX:
        graph_fx(t, c);
        break;
    case GR_INS:
        graph_insert(t, c);
        break;
    case GR_MOD:
        cv_oy = 0;
        graph_mod(t, c);
        break;
    case GR_BROWSE:
        cv_oy = 0;
        graph_browse();
        break;
    case GR_ENGINE:                                  /* T0227: WHEEL's drawbars and FM6's chart on the page that edits their first controls; the engines' list while K1 turns */
        if (!(ui.hot_t && ui.hot_col == 0u) && ENGINES[t->eng_req % NENGINES] == &ENG_WHEEL) { graph_wheel(t, c); break; }
        if (!(ui.hot_t && ui.hot_col == 0u) && t->eng_req % NENGINES == ENGI_FM6) { graph_fm6(t, c, 6u); break; }
        if (grid_on == 5 && ui.hot_t && ui.hot_col == 0u) {   /* K1 turning: the list of engines in the picture's place (the approved list look) */
            graph_eng_list(t);
            break;
        }
        if (grid_on == 5) {                          /* option B: the picture of the engine (LOFI and NOISE have none since item 165: grid_on 15, cells), the list is the slim row's */
            if (ENGINES[t->eng_req % NENGINES] == &ENG_ANALOG)
                graph_pic_analog(t, c);
            else if (ENGINES[t->eng_req % NENGINES] == &ENG_PHASE)
                pic_phase(t, c);
            else if (ENGINES[t->eng_req % NENGINES] == &ENG_WAVE)
                pic_table(t, c);
            else if (ENGINES[t->eng_req % NENGINES] == &ENG_FORMANT)
                pic_voice(t, c);
            else if (ENGINES[t->eng_req % NENGINES] == &ENG_PHYS)
                pic_phys(t, c);
            else if (ENGINES[t->eng_req % NENGINES] == &ENG_TRIO)
                pic_trio(t, c);
            break;
        }
        cv_oy = 0;
        graph_engine();
        break;
    case GR_SLOTS:
        cv_oy = 0;
        graph_slots();
        break;
    case GR_PATS:
        cv_oy = 0;
        graph_pats();
        break;
    case GR_EVENTS:
        cv_oy = 0;
        graph_events();
        break;
    case GR_MORPH:
        cv_oy = 0;
        graph_morph();
        break;
    case GR_PERF:
        cv_oy = 0;
        graph_perf();
        break;
    case GR_MORPHSET:
        cv_oy = 0;
        graph_morphset();
        break;
    case GR_MODE:
        cv_oy = 0;
        graph_mode();
        break;
    case GR_MIDI:
        cv_oy = 0;
        graph_midi();
        break;
    case GR_MLEARN:
        cv_oy = 0;
        graph_mlearn();
        break;
    case GR_SCHAIN:
        cv_oy = 0;
        graph_schain();
        break;
    case GR_MODS:
        cv_oy = 0;
        graph_mods();
        break;
    case GR_MSTG:
        cv_oy = 0;
        graph_mstg(c);
        break;
    case GR_EQ:
        graph_eq(t, c);
        break;
    case GR_COMP:
        cv_oy = 0;
        graph_comp(c);
        break;
    case GR_FILT:
        graph_filt(t, c);
        break;
    case GR_SUB:
        graph_sub(t, c);
        break;
    case GR_CYCLE:
        graph_cycle(t, c);
        break;
    case GR_ARP:
        if (arp_drawn(t)) {
            cv_oy = 0;
            graph_arp(t, c);
        } else {
            cv_oy = 0;
            graph_scope(c);
        }
        break;
#if DUELL_TEST_MODE
    case GR_TEST:
        cv_oy = 0;
        graph_test();
        break;
#endif
    case GR_FMOP:
    case GR_FMOP2:
        if (fop_ok()) graph_fm6(t, c, fop_op % 6u);   /* the chart, the operator picked */
        break;
    case GR_FMENV:
        if (fop_ok()) graph_fm6env(t, c);
        break;
    default:
        if (pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_WHEEL) graph_wheel(t, c);
        else if (pg->scope == SC_ENGINE && t->eng_req % NENGINES == ENGI_FM6) graph_fm6(t, c, 6u);   /* EDIT 1 and 2 */
        else if (grid_on == 1) graph_calm(ui.page);   /* (DUELL 1.1: every grid page without a picture of its own: the calm curve, not the live scope) */
        else { cv_oy = 0; graph_scope(c); }
        break;
    }
    cv_oy = 0;
    cv_blit(0, Y_GRAPH);
    if (pg->graph == GR_COMP) {                      /* (the bar over the panel just drawn) */
        cmpm_sig = 0;
        comp_meter();
    }
}
