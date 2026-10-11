/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI: the panel (Y_GRAPH .. Y_GRAPH + H_GRAPH) between the cards and the footer: a SURF area
 * (rounded, on BG) holding the page graphs (ADSR, LFO, steps, piano roll, drum grid, scale, FX, SLICER,
 * MOD, lists: presets, user presets, patterns, project slots, song), the HOME oscilloscope, the
 * instrument diagrams; the MIXER page draws four SURF columns instead. Each graph is redrawn only when
 * graph_signature() changes. The look:
 * curves THEME (2 px), guides and empty marks RAISE, captions MID / DIM, the active thing ACCENT,
 * bars rounded; a list's selected row is a THEME bar with INK text. */
#define PANEL_X0 10                                  /* the graphs' inner area: x 10..230 */
#define PANEL_W 220
#define GOY 11                                       /* graphs drawn on a 100 px scale sit at y 11..111 */
/* the height ADSR and LFO are drawn on: 100 px, or (MENU > LARGE's strip: draw_graph) the strip's ~ 56 */
static int32_t graph_ht = 100;
#include "ui_fm6op.c"                                /* EDIT > OPERATOR, OP ENV, OPERATOR 2: FM6's operators (their charts below) */

/* Matches voice.c: attack is linear, decay and release are exponential
 * (env += (target - env) * k each tick, ~99 % after the set time). Time
 * axis is the parameter value (the times themselves are exponential). */
static void graph_adsr(const track_t *t, uint16_t c)
{
    const page_t *pg = cur_page();
    int32_t a = 4 + t->p[pg->id[0]] * 50 / 127, d = 6 + t->p[pg->id[1]] * 50 / 127, r = 6 + t->p[pg->id[3]] * 60 / 127;
    int32_t top = 6 * graph_ht / 100, bot = 88 * graph_ht / 100, sus = t->p[pg->id[2]] * 1000 / 127;   /* 0..1000 */
    int32_t x0 = 12, x1 = x0 + a, x3 = 226 - r, i, px, py;
    int32_t e = 32768;                                                  /* exp(-4.6 u), Q15 */
#define EGY(lvl) (bot - (lvl) * (bot - top) / 1000)
    cv_rect(PANEL_X0, bot + 2, PANEL_W, 1, T_RAISE);
    cv_line_t(x0, bot, x1, top, c, 2);                                  /* attack: linear */
    px = x1;
    py = top;
    for (i = 1; i <= d; i++) {                                          /* decay: exponential to SUS */
        int32_t lvl;
        e = (e * (32768 - 150733 / d)) >> 15;           /* k^d = exp(-4.6) */
        lvl = sus + ((1000 - sus) * e >> 15);
        cv_line_t(px, py, x1 + i, EGY(lvl), c, 2);
        px = x1 + i;
        py = EGY(lvl);
    }
    cv_line_t(px, py, x3, EGY(sus), c, 2);                               /* sustain */
    px = x3;
    py = EGY(sus);
    e = 32768;
    for (i = 1; i <= r; i++) {                                          /* release: exponential to 0 */
        e = (e * (32768 - 150733 / r)) >> 15;
        cv_line_t(px, py, x3 + i, EGY(sus * e >> 15), c, 2);
        px = x3 + i;
        py = EGY(sus * e >> 15);
    }
#undef EGY
}

static void graph_lfo(const track_t *t, uint16_t c)
{
    const page_t *pg = cur_page();
    int l2 = pg->id[0] == P_L2RATE || pg->id[0] == P_L2SYNC;   /* DUELL: LFO 2's pages draw LFO 2 */
    uint32_t r = l2 ? P_L2RATE : P_LRATE, s = l2 ? P_L2SYNC : P_LSYNC;
    int32_t x, cy = graph_ht / 2, a = 38 * graph_ht / 100, py = cy, uni = t->p[s + 2u] != 0;
    uint32_t ph = (uint32_t)t->p[r + 2u] << 25;
    int32_t z = uni ? cy + a : cy;                   /* the zero line: POL UNI draws 0..+1 over it */
    cv_rect(PANEL_X0, z, PANEL_W, 1, T_RAISE);
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
 * its column (an accent: TEXT, the row's full height; the cursor step's notes: ACCENT), chords stacked, lane hits as
 * their notes (a RATCH step's bars in its 2..4 parts, 1 px apart); a TIE carries the bars of the note before through
 * its column; a REST is empty; a slide is a TEXT diagonal from the end of its bar into the next note (not from a
 * RATCH step: it never slides out). Notes outside
 * the view: a 1 px mark on the edge. The cursor: a TEXT column frame; the playhead: a 1 px ACCENT line. The view
 * (proll.lo, the bottom row) fits the notes of the page, else centres on the cursor's note, and follows in steps (a
 * third of the way per frame); graph_signature() holds it, so a page that does not change is not drawn again. */
#define PR_ROWS 21                                  /* semitones shown (1.75 octaves) */
#define PR_RH 5                                     /* px per row */
#define PR_Y0 9                                     /* the top row (canvas y) */
#define PR_X0 35                                    /* the first step column */
#define PR_CW 12                                    /* px per step */
#define PR_KX 24                                    /* the keyboard strip, 9 px */
static struct {
    uint8_t lo, trk, init, moving;                    /* lo: the note of the bottom row; moving: on its way */
    uint32_t frame;
} proll;
static const uint8_t KEY_BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

static int32_t pr_note(const step_t *st, uint32_t j)   /* the step's note j (0..3), then its lane hits (-1: none) */
{
    if (j < 4u)
        return j < st->n ? st->note[j] : -1;
    return (st->hit >> (j - 4u)) & 1u ? DRUM_LANE_NOTE[j - 4u] : -1;
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
        for (j = 0; s < NSTEP && j < 4u + NLANE; j++) {
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
/* the bars of step s in column x (in its RATCH parts, hits; a TIE's carried bars: 1): a 1 px edge mark for a note
 * out of view; first: row y of the step's first note */
static void pr_bars(const step_t *st, int32_t x, int32_t w, uint16_t c, int acc_all, int32_t *first, uint32_t hits)
{
    uint32_t j;
    int32_t ybot = PR_Y0 + PR_ROWS * PR_RH;
    for (j = 0; j < 4u + NLANE; j++) {
        int32_t n = pr_note(st, j), y;
        int acc;
        if (n < 0)
            continue;
        acc = acc_all || (j >= 4u && ((st->acc >> (j - 4u)) & 1u));
        y = pr_row_y(n);
        if (*first < -1000)
            *first = y;
        if (y < PR_Y0)
            pr_split(x, PR_Y0, w, 1, c, hits);
        else if (y >= ybot)
            pr_split(x, ybot - 1, w, 1, c, hits);
        else if (acc)
            pr_split(x, y, w, PR_RH, c == T_ACCENT ? c : T_TEXT, hits);
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
        int in = mask == 0xFFFu ? !KEY_BLACK[pc] : (int)((mask >> ((uint32_t)(n - t->p[P_ROOT] + 120) % 12u)) & 1u);
        if (in)
            cv_rect(PR_X0, y, gw, PR_RH, T_LANE);
        if ((held >> r) & 1u)
            cv_rect(PR_KX, y, 9, PR_RH - 1, T_ACCENT);
        else
            cv_rect(PR_KX, y, KEY_BLACK[pc] ? 5 : 9, PR_RH - 1, KEY_BLACK[pc] ? T_DIM : T_RAISE);
        if (pc == 0u) {
            cv_rect(PR_X0, y + PR_RH - 1, gw, 1, T_RAISE);
            note_name(nb, (uint32_t)n);
            GFX_HOOK_ALIGN(0, y, 23, y + PR_RH, AL_V, "piano roll C label on its row");
            GFX_HOOK_ALIGN(0, y, 23, y + PR_RH, AL_R | AL_PASS, "piano roll C labels' right edge");
            {   /* its ink ends at x 23, a column before the keys (by the advance, C1's 1 ended 2 px short) */
                int32_t b[4];
                text_ink(&AF_S, nb, b);
                cv_text_on(23 - b[2], y - 5, &AF_S, nb, T_DIM, T_SURF);   /* (the widest, "C-1", inside the panel) */
            }
        }
    }
    for (i = 0; i <= ncol; i++)                       /* step lines, the beats brighter */
        cv_rect(PR_X0 + (int32_t)i * PR_CW, PR_Y0, 1, ybot - PR_Y0, i % 4u ? T_GRID : T_RAISE);
    for (i = 0; i < ncol; i++) {
        uint32_t si = base + i;
        int32_t x = PR_X0 + (int32_t)i * PR_CW;
        if (si == ui.cursor)
            cv_frame(x, PR_Y0 - 2, PR_CW + 1, ybot - PR_Y0 + 4, T_TEXT);
        if (song.playing && si == t->seq_idx)
            cv_rect(x + 6, PR_Y0, 1, ybot - PR_Y0, T_ACCENT);
        if ((locks >> si) & 1u)                       /* a parameter lock: a mark under the step */
            cv_rect(x + 3, ybot + 3, 7, 2, T_ACCENT);
    }
    for (i = 0; i < ncol; i++) {                      /* the notes */
        uint32_t si = base + i, s = pr_src(t, si, len), nx = (si + 1u) % len;
        const step_t *st, *ns = &seq_steps(t)[nx];
        int32_t x = PR_X0 + (int32_t)i * PR_CW, y = -10000;
        uint16_t col;
        if (s == NSTEP)
            continue;                                 /* a REST, empty, or a TIE after one */
        st = &seq_steps(t)[s];
        col = s == ui.cursor || si == ui.cursor ? T_ACCENT : c;
        if (s == si)
            pr_bars(st, x + 2, 9, col, (st->flags & SF_ACCENT) != 0u, &y, step_ratchet(st));
        else                                          /* a TIE: the bars on through the gap before */
            pr_bars(st, x - 1, 12, col, (st->flags & SF_ACCENT) != 0u, &y, 1u);
        if ((st->flags & SF_SLIDE) && step_ratchet(st) == 1u &&   /* (a ratcheted step never slides out) */
            ns->time == ST_NOTE && ns->n && y >= PR_Y0 && y < ybot) {
            int32_t ny = clamp(pr_row_y(ns->note[0]), PR_Y0, ybot - PR_RH);
            cv_line(x + 10, y + 2, x + 14, ny + 2, T_TEXT);   /* the slide into the next note */
        }
    }
}
/* SEQ > STEP on a DRUM track: the grid, 8 lanes x the 16 steps of the page shown. Lanes by their two-letter
 * names (BD SD CP CH OH TM RS CB; CG CL CY on the other kits). A hit is a rounded square (accented: the
 * accent; a RATCH step's: its 2..4 parts as bars), an empty step a dot (brighter on the beats and on the selected lane); the selected lane is
 * underlaid RAISE, the cursor framed in TEXT; a TEXT bar over the step playing. 1.4.1: a muted lane (the EDIT layer's
 * black keys, eng_drum.c drum_mute): its name DIM, the MUTE icon after it, its hits DIM */
static void graph_grid(const track_t *t, uint16_t c)
{
    uint32_t l, i, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u, mute = drum_mute[trk_index(t) % NPART];
    uint64_t locks = motion_lock_steps(trk_index(t));
    int32_t y0 = 6;
    for (i = 0; i < 16u && base + i < len; i++)     /* a parameter lock: a mark over the step */
        if ((locks >> (base + i)) & 1u)
            cv_rect(41 + (int32_t)i * 12, 1, 8, 2, T_ACCENT);
    for (l = 0; l < NLANE; l++) {
        int32_t y = y0 + 4 + (int32_t)l * 14;
        int sel = l == ui.lane, mu = (mute >> l) & 1u;
        uint16_t row = sel ? T_RAISE : T_SURF, hc = mu ? T_DIM : c, ac = mu ? T_DIM : T_ACCENT;
        if (sel) {
            GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_CELLS, "drum lane fill centred");
            cv_rrect(6, y, 228, 14, 3, T_RAISE, T_SURF);   /* (6 .. 234, as a list row; the hits 40 .. 230) */
        }
        GFX_HOOK_ALIGN(0, y + 2, 0, y + 12, AL_V, "drum lane label on its hits' line");
        cv_text_on(10, y - 1, &AF_S, drum_lane_abbr(t, l), mu ? T_DIM : sel ? T_ACCENT : T_MID, row);   /* ink rows 2 .. 11, as the hits */
        if (mu) {                                    /* (27 .. 39: between the name and the first hit) */
            GFX_HOOK_ALIGN(0, y, 0, y + 14, AL_V, "drum lane mute icon centred up/down");
            cv_icon_in(27, y, 0, 14, 12, ICON_MUTE, T_DIM, row);
        }
        for (i = 0; i < 16u && base + i < len; i++) {
            const step_t *st = &seq_steps(t)[base + i];
            uint32_t b = 1u << l;
            int32_t x = 40 + (int32_t)i * 12;
            if ((step_lanes(st) & b) && step_ratchet(st) > 1u)
                pr_split(x, y + 2, 10, 10, (step_accents(st) & b) ? ac : hc, step_ratchet(st));
            else if (step_lanes(st) & b)
                cv_rrect(x, y + 2, 10, 10, 2, (step_accents(st) & b) ? ac : hc, row);
            else
                cv_rect(x + 4, y + 6, 2, 2, i % 4u == 0u || sel ? T_MID : T_DIM);
            if (sel && base + i == ui.cursor)          /* the cursor: a 1 px frame */
                cv_frame(x - 1, y + 1, 12, 12, T_TEXT);
        }
    }
    if (song.playing && t->seq_idx < len && t->seq_idx / 16u == ui.bank)
        cv_rect(40 + (int32_t)(t->seq_idx % 16u) * 12, y0 - 1, 10, 3, T_TEXT);
}
/* SCL: the 12 keys as rounded bars (black keys high, white keys low): in the scale THEME, the root the
 * accent, out of the scale RAISE */
static void graph_scale(const track_t *t, uint16_t c)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    uint32_t i, mask = scale_mask(t);
    for (i = 0; i < 12u; i++) {
        uint32_t deg = (i + 12u - (uint32_t)t->p[P_ROOT]) % 12u;
        int32_t x = 13 + (int32_t)i * 18;
        uint16_t col = (mask >> deg) & 1u ? (deg == 0u ? T_ACCENT : c) : T_RAISE;
        cv_rrect(x + 3, BLACK[i] ? 6 : 38, 10, 52, 3, col, T_SURF);
    }
}
/* CHORD (SCL 2): the chord's name ("Cm7": the last one the track played; before any, the one on its ROOT from
 * C4) over the keys from the C below its lowest note (two octaves, three for a wide one): its notes THEME, its
 * root's ACCENT, the others RAISE. OFF or a kit: what to do instead */
static void panel_note(const char *a, const char *b, const char *c);
static void panel_alert(uint32_t id, const char *a);
static void graph_test(void);                        /* GLO > TEST (ui_menu.c: the CPU meter's code) */
static uint32_t test_sig(void);
static uint32_t tempo_tapping(void);                   /* TEMPO's TAP: the taps of the last 2 s (ui_input.c) */
static void graph_chord(const track_t *t, uint16_t c)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    uint8_t nn[CHORD_MAX];
    uint32_t n, i, j, k = trk_index(t), lo, cnt, w;
    int32_t r;
    uint16_t mask;
    char b[12];
    const char *cap = "LAST";
    if (!t->p[P_CHRD]) {
        panel_note("CHORD KEYS OFF", "[K1] DIA3: IN-KEY CHORDS", 0);   /* (the title says what they are) */
        return;
    }
    if (chord_kit(t)) {
        panel_note("NO CHORDS ON KITS", 0, 0);
        return;
    }
    if (chord_last[k].n) {
        r = chord_last[k].root;
        mask = chord_last[k].mask;
        n = chord_last[k].n;
        for (i = 0; i < n; i++)
            nn[i] = chord_last[k].note[i];
    } else {
        n = chord_make(t, (uint32_t)(60 + t->p[P_ROOT]), nn, &r, &mask);
        cap = "ON ROOT";
    }
    if (trk_vmode(t) != V_POLY)
        cap = "ROOT ONLY";                              /* MONO / LEGATO / UNISON */
    chord_name(b, (uint32_t)r, mask);
    cv_text_on(14, 8, &AF_M, b, T_TEXT, T_SURF);
    GFX_HOOK_ALIGN(0, 0, 0, 8 + AF_M.asc, AL_B, "chord caption on the name's baseline");
    cv_text_r(226, 8 + AF_M.asc - AF_S.asc, &AF_S, cap, T_MID, T_SURF);   /* on the name's baseline */
    lo = nn[0] - nn[0] % 12u;
    cnt = nn[n - 1u] - lo < 24u ? 24u : 36u;
    w = 216u / cnt;
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_CELLS | AL_N(cnt), "chord keys centred");
    for (i = 0; i < cnt; i++) {
        uint32_t note = lo + i, on = 0;
        int32_t x = 13 + (int32_t)(i * w);              /* (216 px of keys and their gaps: x 13 .. 227) */
        for (j = 0; j < n; j++)
            on |= nn[j] == note;
        cv_rrect(x, BLACK[note % 12u] ? 36 : 62, (int32_t)w - 2, 48, w > 6u ? 3 : 2,
                 !on ? T_RAISE : note % 12u == (uint32_t)r % 12u ? T_ACCENT : c, T_SURF);
    }
}
/* the four sends as faders under their cards: a RAISE slot, the THEME fill from the bottom, a cap */
static void graph_fx(const track_t *t, uint16_t c)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        int32_t h = t->p[P_DIST + i] * 76 / 127, x = CARD_X(i) + 26;
        cv_rrect(x, 6, 4, 82, 2, T_RAISE, T_SURF);
        if (h > 3)
            cv_rrect(x, 88 - h, 4, h, 2, c, T_RAISE);
        cv_rrect(x - 4, 85 - h, 12, 6, 3, c, T_SURF);
    }
}
/* INSERT pages (1.5, fx.c track_insert): the drives and CRUSH as their curve, input (-1.5 .. +1.5 of a voice's full
 * scale) across, output up, dry and wet blended by MIX (fx.c ins_shape: the same shape; the dry diagonal RAISE); the
 * swept ones as two cycles of their sweep, as deep as DEPTH (FLANG the squared triangle, CHOR a sine, PHASR a
 * triangle). OFF: the dry diagonal, dim */
static void graph_insert(const track_t *t, uint16_t c)
{
    int32_t x, cy = graph_ht / 2, a = 38 * graph_ht / 100, py = cy, ty = clamp(t->p[P_ITYPE], 0, IT_N - 1);
    int32_t m = ins_mixq(t->p[P_IMIX]), g = 4096 + t->p[P_IA] * t->p[P_IA] * 3, lvl = LEVEL_Q12[t->p[P_IC] & 127];
    uint32_t sh = ins_bits_sh(t->p[P_IA]);
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
    cv_line(PANEL_X0 + PANEL_W / 2, cy - a, PANEL_X0 + PANEL_W / 2, cy + a, T_RAISE);
    if (ty == IT_OFF || ty >= IT_PHASER || !m)
        cv_line(PANEL_X0 + PANEL_W / 6, cy + a, PANEL_X0 + PANEL_W * 5 / 6, cy - a, ty ? T_RAISE : T_DIM);
    if (ty == IT_OFF)
        return;
    for (x = 0; x < PANEL_W; x++) {
        int32_t y, w;
        if (ty >= IT_PHASER) {                       /* the sweep, two cycles */
            uint32_t ph = (uint32_t)x * (0xFFFFFFFFu / (PANEL_W / 2u)), tr = pf_tri(ph);
            w = ty == IT_CHORUS ? osc_sine(ph) * 2 : ty == IT_FLANGER ? (int32_t)((tr * tr) >> 16) * 2 - 65536 : (int32_t)tr * 2 - 65536;
            y = cy - (w >> 1) * a / 32768 * t->p[P_IB] / 127;
        } else {                                     /* the curve: v in, dry v, wet shaped */
            int32_t v = (x - PANEL_W / 2) * 49152 / (PANEL_W / 2), o;
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
 * nothing (SRC, DST or AMT at 0) dim */
static void graph_mod(const track_t *t, uint16_t c)
{
    uint32_t k, base = mod_page_base(cur_page());      /* (MOD: slots 1..4, MOD 2: 5..8) */
    for (k = 0; k < 4u; k++) {
        const int16_t *p = &t->p[MSLOT_ID(base + k)];
        int32_t y = 14 + (int32_t)k * 24, on = p[0] && p[1] && p[2], sel = k == mod_ui_slot, x;
        uint16_t bg = sel ? T_THEME : T_SURF, col = sel ? T_INK : on ? c : T_DIM;
        char b[8];
        const char *unit;
        if (sel)
            cv_rrect(6, y, 228, 17, 4, T_THEME, T_SURF);
        b[0] = (char)('1' + base + k);
        b[1] = 0;
        GFX_HOOK_ALIGN(0, y, 0, y + 17, AL_V, "mod row text centred up/down");
        cv_text_on(14, y + CAP_IN(M, 17), &AF_M, b, sel ? T_INK : T_MID, bg);
        GFX_HOOK_ALIGN(0, y, 0, y + 17, AL_V, "mod row text centred up/down");
        cv_text_on(30, y + CAP_IN(M, 17), &AF_M, N_MSRC[clamp(p[0], 0, MS_N - 1)], col, bg);
        cv_line(84, y + 8, 98, y + 8, col);              /* an arrow */
        cv_line(94, y + 4, 98, y + 8, col);
        cv_line(94, y + 12, 98, y + 8, col);
        cv_text_on(106, y + CAP_IN(M, 17), &AF_M, mod_dst_name(t, p[1]), col, bg);
        param_format(&TP[P_M1AMT], p[2], b, &unit);
        GFX_HOOK_ALIGN(0, y, 0, y + 17, AL_V, "mod row text centred up/down");
        x = cv_text_on(172, y + CAP_IN(M, 17), &AF_M, b, col, bg) + 2;
        GFX_HOOK_ALIGN(0, 0, 0, y + CAP_IN(M, 17) + AF_M.asc, AL_B, "mod row unit on its amount's baseline");
        cv_text_on(x, y + CAP_IN(M, 17) + AF_M.asc - AF_S.asc, &AF_S, unit, sel ? T_INK : T_DIM, bg);   /* (a unit: S) */
    }
}

static uint32_t steps_hash(const track_t *t)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < NSTEP; i++) {
        const step_t *st = &seq_steps(t)[i];
        h = (h ^ (st->note[0] + st->n * 128u + st->time * 1024u + st->flags * 4096u + st->note[1] * 65536u)) *
            16777619u;
        h = (h ^ (st->hit | (uint32_t)st->acc << 8 | (uint32_t)st->note[2] << 16 | (uint32_t)st->note[3] << 24)) * 16777619u;
    }
    return h;
}

static uint32_t str_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

/* One shared, reduced sample display. Decode only in the UI/main loop with a
 * private IMA state, 512 samples per frame; no audio voice state is touched.
 * Cache follows the sample zone, selected note and user-slot generation. */
#define SAMPLE_WAVE_COLS 96u
static struct {
    int16_t lo[SAMPLE_WAVE_COLS], hi[SAMPLE_WAVE_COLS];
    const smp_zone_t *zone;
    uint32_t key, gen, pos;
    int32_t pred, index, peak;
    uint8_t ready;
} sample_wave;

static uint32_t sample_wave_zone(const track_t *t)
{
    uint32_t si = (uint32_t)t->p[P_E0] % SMP_NALL, i, zi = 0xFFFFu;
    if (si < SMP_NSETS) {
        const smp_set_t *set = &SMP_SETS[si];
        for (i = 0; i < set->nz; i++)
            if (last_note >= SMP_ZONES[set->z0 + i].lo && last_note <= SMP_ZONES[set->z0 + i].hi) zi = set->z0 + i;
        if (zi == 0xFFFFu && set->nz) zi = set->z0;
    } else {
        uint32_t k = si - SMP_NSETS;
        for (i = 0; i < usr_nz[k]; i++)
            if (last_note >= usr_zone[k][i].lo && last_note <= usr_zone[k][i].hi) zi = 0x8000u | k << 5 | i;
        if (zi == 0xFFFFu && usr_nz[k]) zi = 0x8000u | k << 5;
    }
    return zi;
}

static void sample_wave_tick(const track_t *t)
{
    uint32_t zi = sample_wave_zone(t), key = zi + ((uint32_t)last_note << 16), i;
    if (key != sample_wave.key || sample_wave.gen != smp_user_gen || !sample_wave.zone) {
        sample_wave.key = key; sample_wave.gen = smp_user_gen;
        sample_wave.zone = zi == 0xFFFFu ? 0 : smp_zone(zi);
        sample_wave.pos = 0; sample_wave.pred = sample_wave.index = 0;
        sample_wave.peak = 1; sample_wave.ready = 0;
        for (i = 0; i < SAMPLE_WAVE_COLS; i++) sample_wave.lo[i] = sample_wave.hi[i] = 0;
    }
    const smp_zone_t *z = sample_wave.zone;
    if (!z || !z->n || sample_wave.ready) return;
    for (i = 0; i < 512u && sample_wave.pos < z->n; i++) {
        uint32_t pos = sample_wave.pos, b = SMP_DATA[z->off + (pos >> 1)];
        uint32_t code = (pos & 1u) ? b >> 4 : b & 15u;
        int32_t step = IMA_STEP[sample_wave.index], d = step >> 3;
        if (code & 4u) d += step;
        if (code & 2u) d += step >> 1;
        if (code & 1u) d += step >> 2;
        sample_wave.pred = clamp(sample_wave.pred + ((code & 8u) ? -d : d), -32768, 32767);
        sample_wave.index = clamp(sample_wave.index + IMA_IDX[code & 7u], 0, 88);
        uint32_t col = pos * SAMPLE_WAVE_COLS / z->n;
        int32_t v = sample_wave.pred, a = v < 0 ? -v : v;
        if (v < sample_wave.lo[col]) sample_wave.lo[col] = (int16_t)v;
        if (v > sample_wave.hi[col]) sample_wave.hi[col] = (int16_t)v;
        if (a > sample_wave.peak) sample_wave.peak = a;
        sample_wave.pos++;
    }
    sample_wave.ready = sample_wave.pos == z->n;
}

static void graph_sample(uint16_t c)
{
    uint32_t i;
    const smp_zone_t *z = sample_wave.zone;
    cv_rect(PANEL_X0, 48, PANEL_W, 1, T_RAISE);
    for (i = 0; i < SAMPLE_WAVE_COLS; i++) {
        int32_t x = 12 + (int32_t)i * 216 / (SAMPLE_WAVE_COLS - 1u);
        cv_line(x, 48 - sample_wave.hi[i] * 38 / sample_wave.peak,
                x, 48 - sample_wave.lo[i] * 38 / sample_wave.peak, c);
    }
    if (z->looped && TSEL->p[P_E3]) {
        int32_t left = 12 + (int32_t)(z->ls * 216u / z->n), right = 12 + (int32_t)(z->le * 216u / z->n);
        cv_line(left, 4, left, 90, T_MID); cv_line(right, 4, right, 90, T_MID);
    }
}

#if FELUCCA_SLICE
/* SLICES (ui_slice.c): the waveform from the marker before the selected one to the one after it (the selected
 * slice tinted, its marker the accent, outside the slices DIM), under it the whole material with every marker and
 * the view; then the selected slice's length and the source */
static void graph_slices(void)
{
    uint32_t a, b, len, n = slice_count(), j = slice_sel(), i, src, div, ma, mb;
    char t[20], u[12];
    if (!slice_src(&src, &div) || !n) {             /* (it plays a sine: eng_slice.c) */
        panel_alert(ICON_X_NOFILE, "SAMPLE NOT FOUND");
        return;
    }
    slice_view(&a, &b, &len);
    slice_env(a, b);
    ma = slice_mark(0);
    mb = slice_mark(n);
#define SPX(p) (12 + (int32_t)((uint32_t)((p) - a) * SP_COLS / (b - a)))   /* p in [a, b] */
    if (j < n) {                                     /* the selected slice */
        uint32_t s0 = slice_mark(j), s1 = slice_mark(j + 1u);
        int32_t x0 = SPX(s0 > a ? s0 : a), x1 = SPX(s1 < b ? s1 : b);
        if (x1 > x0)
            cv_rect(x0, 6, x1 - x0, 80, T_TINT);
    }
    for (i = 0; i < SP_COLS; i++) {
        uint32_t p = a + (uint32_t)((uint32_t)i * (b - a) / SP_COLS);
        cv_line(12 + (int32_t)i, 46 - sp.hi[i] * 38 / 64, 12 + (int32_t)i, 46 - sp.lo[i] * 38 / 64,
                p >= ma && p < mb ? T_THEME : T_DIM);
    }
    for (i = 0; i <= n; i++) {                       /* the markers in the view (the selected one last: on top) */
        uint32_t p = slice_mark(i);
        if (i != j && p >= a && p <= b)
            cv_rect(SPX(p), 4, 1, 84, T_MID);
    }
    {
        uint32_t p = slice_mark(j);
        if (p >= a && p <= b)
            cv_rect(SPX(p) > 226 ? 226 : SPX(p), 2, 2, 88, T_ACCENT);
    }
#undef SPX
    cv_rrect(12, 96, (int32_t)SP_COLS, 3, 1, T_RAISE, T_SURF);   /* the whole: every marker, the view */
    for (i = 0; i <= n; i++)
        cv_rect(12 + (int32_t)((uint32_t)slice_mark(i) * (SP_COLS - 1u) / len), 94, 1, 7, i == j ? T_ACCENT : T_MID);
    {
        int32_t x0 = 12 + (int32_t)((uint32_t)a * SP_COLS / len), x1 = 12 + (int32_t)((uint32_t)b * SP_COLS / len);
        cv_rect(x0, 103, x1 - x0 < 2 ? 2 : x1 - x0, 2, T_THEME);
    }
    str_cpy(t, "LEN ", sizeof t);
    slice_time(u, j < n ? slice_mark(j + 1u) - slice_mark(j) : len - mb);
    str_cpy(t + 4, u, sizeof t - 4);
    str_cpy(t + str_len(t), " S", sizeof t - str_len(t));
    cv_text(12, 104, &AF_M, t, T_MID);                  /* the slice's detail (M): its capitals rows 108 .. 118 */
    cv_text_r(228, 104, &AF_M, N_SLC_SRC[src], src && src != SLC_SRC_PIANO ? vt(T_THEME, T_SURF) : T_DIM, T_SURF);
}
#endif

/* WHEEL: the nine drawbars as rounded bars over RAISE slots (the bars of the knob just turned: the accent),
 * their footages under them */
static void graph_wheel(const track_t *t, uint16_t c)
{
    uint32_t k;
    static const char *const names[9] = {"16", "5.3", "8", "4", "2.7", "2", "1.6", "1.3", "1"};
    for (k = 0; k < 9u; k++) {
        int32_t x = 11 + (int32_t)k * 25, level = drw_level(t->p, k);
        if (t->p[P_E4] && k == 8u) level = 0; /* DSP's percussion cancels the 1-foot bar. */
        int hot = ui.hot_t && cur_page()->id[0] == P_E0 &&
                  (ui.hot_col == 0u || ui.hot_col == (k < 2u ? 1u : k < 4u ? 2u : 3u));
        cv_rrect(x + 7, 2, 4, 73, 2, T_RAISE, T_SURF);
        if (level) cv_rrect(x + 3, 75 - level * 8, 12, level * 8, 3, hot ? T_ACCENT : c, T_SURF);
        else cv_rrect(x + 3, 73, 12, 2, 1, hot ? T_ACCENT : T_DIM, T_SURF);
        GFX_HOOK_ALIGN(x + 7, 0, x + 11, 0, AL_H, "wheel footage under its drawbar");
        cv_text_in(x + 3, 82, 12, &AF_S, names[k], T_MID, T_SURF);   /* under the bar, by its ink */
    }
}
/* The FM charts' parts: an operator box 21 x 17 (rows 23 px apart, columns 24), junction dots, Manhattan routes */
#define FM_BH 17
static void fm_dot(int32_t x, int32_t y, uint16_t c) { cv_rect(x - 1, y - 1, 3, 3, c); }
/* a Manhattan route, 1 px: down from (x0, y0) to the jog row jy, across to x1, down to y1 */
static void fm_route(int32_t x0, int32_t y0, int32_t x1, int32_t jy, int32_t y1, uint16_t c)
{
    cv_rect(x0, y0, 1, jy - y0, c);
    cv_rect(x0 < x1 ? x0 : x1, jy, (x0 < x1 ? x1 - x0 : x0 - x1) + 1, 1, c);
    cv_rect(x1, jy, 1, y1 - jy, c);
}
/* a feedback loop on the operator box at (x, y): out of its right side, up, back in on top with an arrow */
static void fm_loop(int32_t x, int32_t y, uint16_t c)
{
    cv_rect(x + 11, y + 5, 5, 1, c);
    cv_rect(x + 15, y - 4, 1, 9, c);
    cv_rect(x, y - 4, 15, 1, c);
    cv_rect(x, y - 4, 1, 4, c);
    cv_line(x - 2, y - 3, x - 1, y - 2, c); cv_line(x + 2, y - 3, x + 1, y - 2, c);
}
/* the output bus under the carriers (the leftmost at cl, the rightmost at cr) at y bus, an arrow out on the right */
static void fm_bus(int32_t cl, int32_t cr, int32_t bus)
{
    cv_rect(cl, bus, cr + 24 - cl, 1, T_MID);
    cv_line(cr + 20, bus - 3, cr + 23, bus, T_MID); cv_line(cr + 20, bus + 3, cr + 23, bus, T_MID);
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
/* FM6's EDIT pages: the algorithm, drawn as graph_fm draws DIGITAL's, "ALG 05" top left; SLOT on a bank voice (B1..B32,
 * 1.4.1): its name top right. Carriers THEME with INK
 * numerals, modulators RAISE with THEME ones; an operator at output level 0: a DIM numeral (a carrier on RAISE).
 * Routes THEME, the feedback loop MID (DIM at feedback 0), the output bus MID. The knob just turned in ACCENT:
 * ALG and PTCH the label, FB the loop, MLVL the routes, MRAT MEG VMOD the modulators, DTUN the carriers.
 * sel < 6 (the OPERATOR pages, ui_fm6op.c): that operator's box ACCENT with an INK numeral, its frequency top right */
static void graph_fm6(const track_t *t, uint16_t c, uint32_t sel)
{
    uint32_t alg = fm6_alg_of(t), k, j, car = fm6_car_ops(alg), top = 0, hi = 0, fbop = fm6_fb_op(alg), hotset = 0;
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
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
    bus = 50 + (27 + 23 * (int32_t)top) / 2;         /* the chart centred on the 100 px scale */
    for (k = 0; k < 6u; k++) {
        x[k] = 120 + (int32_t)(FM6_CELL[alg][k] & 7u) * 24 - (int32_t)hi * 12;
        y[k] = bus - 6 - FM_BH - 23 * (int32_t)(FM6_CELL[alg][k] >> 4);
        if (car >> k & 1u) { cl = x[k] < cl ? x[k] : cl; cr = x[k] > cr ? x[k] : cr; }
    }
    for (k = 0; k < 6u; k++) {                       /* into k: its sources down to its jog row, across, down */
        for (j = 0; j < 6u; j++)
            if (m[k] >> j & 1u) {
                FM6_CHART_HOOK(FMH_ROUTE, j, k);
                fm_route(x[j], y[j] + FM_BH, x[k], y[k] - 3, y[k], rc);
                if (fan[j] > 1u) fm_dot(x[j], y[k] - 3, rc);          /* one operator modulating several */
                if (m[k] & (m[k] - 1u)) fm_dot(x[k], y[k] - 3, rc);   /* several modulating one */
            }
        if (car >> k & 1u) {
            FM6_CHART_HOOK(FMH_CAR, k, 0);
            cv_rect(x[k], y[k] + FM_BH, 1, bus - y[k] - FM_BH, T_MID);
            if (x[k] != cl) fm_dot(x[k], bus, T_MID);
        }
    }
    FM6_CHART_HOOK(FMH_BUS, 0, 0);
    fm_bus(cl, cr, bus);
    FM6_CHART_HOOK(FMH_FB, fbop, 0);
    fm_loop(x[fbop], y[fbop], hot == P_E1 ? T_ACCENT : fb ? T_MID : T_DIM);
    for (k = 0; k < 6u; k++) {
        uint32_t on = pt[(5u - k) * FP_OP + FP_OL] != 0, fill_c = (car >> k & 1u) && on;
        uint16_t f = hotset >> k & 1u ? T_ACCENT : c, fill = k == sel ? T_ACCENT : fill_c ? f : T_RAISE;
        char n[2] = {(char)('1' + k), 0};
        FM6_CHART_HOOK(FMH_BOX, k, 0);
        cv_rrect(x[k] - 10, y[k], 21, FM_BH, 3, fill, T_SURF);
        GFX_HOOK_ALIGN(x[k] - 10, y[k], x[k] + 11, y[k] + FM_BH, AL_HV, "FM6 operator number in its box");
        cv_text_in(x[k] - 10, y[k] + CAP_IN(S, FM_BH), 21, &AF_S, n, fill_c || k == sel ? T_INK : on ? f : T_DIM, fill);
    }
    FM6_CHART_HOOK(FMH_LABEL, 0, 0);
    cv_text(10, 4 - GOY, &AF_S, b, hot == P_E0 || hot == P_E7 ? T_ACCENT : T_MID);
    if (sel < 6u) {                                  /* the operator picked: "OP3 x1.50" */
        char fq[16] = {'O', 'P', (char)('1' + sel), ' ', 0};
        fop_freq(pt + (5u - sel) * FP_OP, fq + 4);
        cv_text_r(228, 4 - GOY, &AF_S, fq, T_TEXT, T_SURF);
    } else if (fm6_bank_slot(fm6_slot[(t - trk) % NTRK]) >= 0) {   /* SLOT B1..B32: the bank voice's name */
        char nm[11];
        for (k = 0; k < 10u; k++)
            nm[k] = (char)pt[FP_NAME + k];
        for (nm[10] = 0; k && nm[k - 1] == ' '; k--)
            nm[k - 1] = 0;
        cv_text_r(228, 4 - GOY, &AF_S, nm, T_TEXT, T_SURF);
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
    int32_t top = 24, bot = 88, x[6], y[6], k;
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
    cv_text(10, 4 - GOY, &AF_S, b, T_MID);
    str_cpy(b, STG[sg], sizeof b);
    str_cpy(b + str_len(b), "  R ", 5);
    fmt_int(b + str_len(b), op[FP_R1 + sg]);
    str_cpy(b + str_len(b), "  L ", 5);
    fmt_int(b + str_len(b), op[FP_L1 + sg]);
    cv_text_r(228, 4 - GOY, &AF_S, b, T_TEXT, T_SURF);
}

#if FELUCCA_FM4
/* DIGITAL's eight algorithms as FM charts, read from src/eng_digital.c's switch (alg) (ui_test.c checks
 * these tables against it). FM_CELL: per operator 1..4 its grid cell, the column in 24 px steps (bits 0..2)
 * and the row above the output bus (bits 4..5; row 0 = the carriers). FM_MOD: per destination operator d a
 * nibble at bit 4 d of the operators modulating it (bit k = operator k + 1). Operator 4 feeds itself (FB). */
static const uint8_t FM_CELL[8][4] = {
    {0x00, 0x10, 0x20, 0x30}, {0x01, 0x11, 0x20, 0x22}, {0x01, 0x10, 0x20, 0x12}, {0x01, 0x12, 0x10, 0x20},
    {0x00, 0x10, 0x02, 0x12}, {0x00, 0x02, 0x04, 0x12}, {0x00, 0x02, 0x04, 0x14}, {0x00, 0x02, 0x04, 0x06}};
static const uint16_t FM_MOD[8] = {0x0842, 0x00C2, 0x004A, 0x0806, 0x0802, 0x0888, 0x0800, 0x0000};
/* Modulators above what they modulate, the carriers on the bottom row over the output bus (an arrow out).
 * Carriers THEME with INK numerals, modulators RAISE with THEME ones; an operator at LEVEL 0: DIM numeral.
 * Routes by IDX: DIM at 0, MID, THEME from 64; op 4's feedback loop MID (DIM at FB 0). The knob just turned
 * (an operator's ratio or level, IDX, FB): ACCENT. Junction dots where routes split or merge. */
static void graph_fm(const track_t *t, uint16_t c)
{
    uint32_t alg = (uint32_t)t->p[P_E0] & 7u, k, j, mods = FM_MOD[alg], car = 0, hop = 9;
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
    int32_t x[4], y[4], lo = 7, hi = 0, top = 0, bus, cl = 240, cr = 0, idx = t->p[P_E4];
    uint16_t ec = hot == P_E4 ? T_ACCENT : !idx ? T_DIM : idx < 64 ? T_MID : c;
    if (hot >= P_E1 && hot <= P_E3) hop = hot - P_E1 + 1u;
    else if (hot >= P_FM1_ATK && hot < P_E0) hop = (hot - P_FM1_ATK) / 5u;
    for (k = 0; k < 4u; k++) {
        int32_t col = FM_CELL[alg][k] & 7, row = FM_CELL[alg][k] >> 4;
        lo = col < lo ? col : lo; hi = col > hi ? col : hi; top = row > top ? row : top;
    }
    bus = 50 + (27 + 23 * top) / 2;                  /* the chart centred on the 100 px scale */
    for (k = 0; k < 4u; k++) {
        x[k] = 120 + (FM_CELL[alg][k] & 7) * 24 - (lo + hi) * 12;
        y[k] = bus - 6 - FM_BH - 23 * (FM_CELL[alg][k] >> 4);
        if (FM_CELL[alg][k] < 0x10) { car |= 1u << k; cl = x[k] < cl ? x[k] : cl; cr = x[k] > cr ? x[k] : cr; }
    }
    for (k = 0; k < 4u; k++) {                       /* into k: sources down to its jog row, across, down */
        uint32_t m = (mods >> (4u * k)) & 15u;
        for (j = 0; j < 4u; j++) if (m >> j & 1u) {
            uint32_t fan = mods & (0x1111u << j);
            fm_route(x[j], y[j] + FM_BH, x[k], y[k] - 3, y[k], ec);
            if (fan & (fan - 1u)) fm_dot(x[j], y[k] - 3, ec);   /* one operator modulating several */
        }
        if (m & (m - 1u)) fm_dot(x[k], y[k] - 3, ec);           /* several modulating one */
        if (car >> k & 1u) {
            cv_rect(x[k], y[k] + FM_BH, 1, bus - y[k] - FM_BH, T_MID);
            if (x[k] != cl) fm_dot(x[k], bus, T_MID);
        }
    }
    fm_bus(cl, cr, bus);                             /* the output bus and its arrow */
    fm_loop(x[3], y[3], hot == P_E6 ? T_ACCENT : t->p[P_E6] ? T_MID : T_DIM);   /* op 4's feedback */
    for (k = 0; k < 4u; k++) {
        char b[2] = {(char)('1' + k), 0};
        uint32_t on = t->p[P_FM1_LEVEL + k * 5u] != 0, fill_c = (car >> k & 1u) && on;
        uint16_t f = k == hop ? T_ACCENT : c, fill = fill_c ? f : T_RAISE;
        cv_rrect(x[k] - 10, y[k], 21, FM_BH, 3, fill, T_SURF);
        GFX_HOOK_ALIGN(x[k] - 10, y[k], x[k] + 11, y[k] + FM_BH, AL_HV, "DIGITAL operator number in its box");
        cv_text_in(x[k] - 10, y[k] + CAP_IN(S, FM_BH), 21, &AF_S, b, fill_c ? T_INK : on ? f : T_DIM, fill);
    }
}
#endif
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


/* MENU > LARGE's strip under the tall cards (ui.c LK_TALL): HOME's scope, ENV's ADSR, LFO's wave,
 * MIXER's tracks drawn small; on the other pages the page's title and number (their charts need the whole panel) */
enum { SK_NONE, SK_SCOPE, SK_ADSR, SK_LFO, SK_TRK, SK_TITLE };
static uint32_t strip_kind(void)
{
    uint32_t g;
    if (large_kind() != LK_TALL)
        return SK_NONE;
    if (ui.home)
        return SK_SCOPE;
    g = cur_page()->graph;
    return g == GR_ADSR ? SK_ADSR : g == GR_LFO ? SK_LFO : g == GR_TRK ? SK_TRK : SK_TITLE;
}
/* OVERVIEW (ui.home), the last of HOME's round: "OVERVIEW" and its number "5/5" (title_parts) */
static void home_parts(char *nm, char *ct)
{
    uint32_t i, n = 1;
    for (i = 0; i < NPAGES; i++)
        n += PAGES[i].fam == FAM_TRK && page_visible(i);
    str_cpy(nm, "OVERVIEW", 12);
    fmt_int(ct, (int32_t)n);
    str_cpy(ct + str_len(ct), "/", 4);
    fmt_int(ct + str_len(ct), (int32_t)n);
}
/* the page's name and its number in its family, apart (the footer draws them either side of its separator; nm holds
 * 12, ct 8; ct empty: the page is alone in its family): "ENV DEST" "2/2", EDIT: the engine's "OSC" "1/2"; the piano
 * roll: its scale ("C MIN") for the name. Returns 1 for a piano roll whose scale is the name (the strip's title
 * leaves the number out there) */
static int page_parts(char *nm, char *ct)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    uint32_t i, n = 0, k = 0;
    int roll = 0;
    const char *pt = pg->scope == SC_ENGINE ? e->page_title[pg->id[0] != P_E0] : 0;   /* EDIT: the engine's */
    ct[0] = 0;
    if (ui.home) {                                  /* OVERVIEW, the round's last (3.5) */
        home_parts(nm, ct);
        return 0;
    }
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == pg->fam && page_visible(i)) {
            n++;
            if (i == ui.page)
                k = n;
        }
    str_cpy(nm, pt ? pt : grid_on() ? "GRID" : pg->title, 12);
    if (pg->fam == FAM_TRK)                         /* HOME's pages (3.5): OVERVIEW is the round's last, "MORPH 1/5" */
        n++;
    if (n > 1) {
        fmt_int(ct, (int32_t)k);
        str_cpy(ct + str_len(ct), "/", 4);
        fmt_int(ct + str_len(ct), (int32_t)n);
    }
    if (pg->graph == GR_ROLL && !drum_track(t)) {  /* the piano roll: its tinted rows' scale ("C MIN") */
        str_cpy(nm, N_NOTE[(uint32_t)t->p[P_ROOT] % 12u], 4);
        str_cpy(nm + str_len(nm), " ", 4);
        str_cpy(nm + str_len(nm), N_SCALE[clamp(t->p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)], 8);
        roll = 1;
    }
    return roll;
}
/* the page's title and its number in its family ("ENV DEST 2/2", EDIT: the engine's "OSC 1/2"); the piano roll: its
 * scale ("C MIN"); ti holds 20 (the strip) */
static void page_title(char *ti)
{
    char nm[12], ct[8];
    int roll = page_parts(nm, ct);
    str_cpy(ti, nm, 12);
    if (ct[0] && !roll) {
        str_cpy(ti + str_len(ti), " ", 4);
        str_cpy(ti + str_len(ti), ct, 8);
    }
}
/* the strip's title: L centred (M when L lacks a glyph or is too wide) */
static void graph_title(void)
{
    char ti[20];
    const aafont_t *f = &AF_L;
    int32_t y;
    page_title(ti);
    if (!large_face_has(ti) || text_w(f, ti) > 220)
        f = &AF_M;
    y = f == &AF_L ? CAP_IN(L, LG_H_GRAPH) : CAP_IN(M, LG_H_GRAPH);
    GFX_HOOK_ALIGN(0, 0, 240, LG_H_GRAPH, AL_HV, "large strip title centred");
    cv_text_in(0, y, 240, f, ti, vt(T_THEME, T_SURF), T_SURF);
}

static uint32_t mv_tick(void);                       /* (the MORPH page's animation step, below) */
static uint32_t graph_signature(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint32_t h = 2166136261u, i;
    if (ui.home)
        return h ^ (ui.frame / 2u);                  /* scope: redraw every other frame */
    h ^= (uint32_t)pg->graph * 131u + TSEL->eng_req + song.sel * 7777u + ui.page * 1291u;
    if (strip_kind() == SK_TITLE)                    /* MENU > LARGE: the page's title (its engine, its number) */
        return h ^ 0x5A17u;
    if (pg->graph == GR_NONE || pg->graph == GR_ARP || pg->graph == GR_TEMPO) h ^= ui.frame / 2u;
    for (i = 0; i < P_COUNT; i++)
        h = (h ^ (uint32_t)t->p[i]) * 16777619u;
    h ^= (uint32_t)TSEL->preset * 7u + (uint32_t)song.g[G_SLOT] * 13u + TSEL->user * 257u + up_gen * 7919u + ui.pe * 104729u + list_mode() * 15485863u +
         ui.ppick * 1299709u;
    if (pg->graph == GR_CHORD) {                     /* the last chord played */
        h = (h ^ (chord_last[song.sel].root + 131u * chord_last[song.sel].mask)) * 16777619u;
        for (i = 0; i < CHORD_MAX; i++)
            h = (h ^ chord_last[song.sel].note[i]) * 16777619u;
        h ^= (uint32_t)t->engine * 389u;             /* (MONO and kits follow the sounding engine) */
    }
    if (pg->graph == GR_MOD)
        h ^= (mod_ui_slot + 1u) * 40503u;
    if (pg->scope == SC_MIDI)                        /* step 6: the channels, the route, the mode, MIDI LEARN's state */
        h = (h ^ ((uint32_t)midi_pch[0] | (uint32_t)midi_pch[1] << 4 | (uint32_t)(song.g[G_ROUTE] == 1) << 8 | (uint32_t)duell_mode_req << 9 |
                  (uint32_t)ui.ml.on << 11 | (uint32_t)ui.ml.pick << 12 | ml_count() << 13 | (uint32_t)ui.ml.id << 19 | (uint32_t)ui.ml.trk << 27) * 0x9E3779B1u) * 16777619u;
    if (pg->graph == GR_TEMPO)                       /* TEMPO's TAP card: the taps counted, over for good after 2 s */
        h = (h ^ (tempo_tapping() + 1u + (uint32_t)song.g[G_BPM] * 7u) * 2246822519u) * 16777619u;
    if (pg->graph == GR_TEST)                        /* the CPU test: multi, engines, patches, the meter (it polls the counters here) */
        h = (h ^ test_sig()) * 16777619u;
    if (pg->scope == SC_MODE)                        /* step 5: the MODE asked for */
        h = (h ^ (duell_mode_req + 1u) * 0x2545F491u ^ ((uint32_t)(uint8_t)mix_bal | (uint32_t)mix_xf << 8 | (uint32_t)mix_split << 16) * 0x9E3779B1u) * 16777619u;
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
    if (pg->graph == GR_EVENTS)                      /* AUTOMATION: the rows, the one selected, + ADD's place, PLAY,
                                                      * QUANTIZE */
        h = (h ^ ev_sig()) * 16777619u;
#if FELUCCA_SLICE
    if (pg->graph == GR_SLICES && slice_page_ok()) h ^= slice_sig();
#endif
    if (pg->scope == SC_ENGINE && (ENGINES[t->eng_req % NENGINES] == &ENG_WHEEL || t->eng_req % NENGINES == ENGI_FM6 ||
                                   (FELUCCA_FM4 && t->eng_req % NENGINES == ENGI_DIGITAL)))
        h ^= (ui.hot_t ? ui.hot_col + 1u : 0u) * 65537u;
    if ((pg->scope == SC_ENGINE || fop_page(pg->graph)) && t->eng_req % NENGINES == ENGI_FM6)   /* the patch (PAT's algorithm,
                                                     * levels, FB; the operator pages: the operator, the stage) */
        h ^= (fm6_pgen[(t - trk) % NTRK] + 1u) * 2246822519u + (fop_op + 1u) * 40503u + (fop_stg + 1u) * 7919u;
    if (pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_SAMPLE) h ^= sample_wave.pos * 13u + sample_wave.key;
    if (pg->graph == GR_ROLL) {
        uint32_t ph = song.playing ? t->seq_idx : 0xFFFFu;
        if (ph / 16u != ui.bank)
            ph = 0xFFFFu;                            /* the roll shows the cursor's bank only */
        h ^= steps_hash(t) + ph * 31u + ui.cursor * 7919u + ui.lane * 104723u + ui.bank * 613u +
             drum_mute[trk_index(t) % NPART] * 2654435761u;
        {                                            /* the parameter locks' marks (the roll, the grid) */
            uint64_t lk = motion_lock_steps(trk_index(t));
            h = (h ^ (uint32_t)lk ^ (uint32_t)(lk >> 32) * 2654435761u) * 16777619u;
        }
        if (!drum_track(t)) {                        /* the roll: its view and the keys held */
            pr_follow(t);
            h = (h ^ (proll.lo + 1u)) * 16777619u;
            h = (h ^ pr_held()) * 16777619u;
        }
    }
    return h;
}
/* 4-letter engine tags of the preset list */
static const char *eng_abbr(const char *name)
{
    static const char *const A[][2] = {{"ANALOG", "ANLG"},
#if FELUCCA_FM4
                                       {"DIGITAL", "DGTL"},
#endif
                                       {"PHASE", "PHAS"}, {"LOFI", "LOFI"},
                                       {"SAMPLE", "SMPL"}, {"VOICE", "VOCL"}, {"TRIO", "TRIO"}, {"WHEEL", "WHEL"},
                                       {"GRAIN", "GRAN"}, {"PHYS", "PHYS"}, {"DRUM", "DRUM"},
                                       {"NOISE", "NOIS"}, {"FM6", "FM6"}, {"SLICE", "SLCE"}};
    uint32_t i;
    for (i = 0; i < sizeof A / sizeof A[0]; i++)
        if (str_eq(name, A[i][0]))
            return A[i][1];
    return name;
}

/* a list row (17 px), in M as every list's (PRESETS, USER, PATTERNS, PROJECT, AUTOMATION, MOD, the MENU): the selected
 * one a THEME bar with INK text; tag at x 14 (the widest, DRUM, to x 57), free text from x LIST_NX to x1 */
#define LIST_Y(k) (3 + 17 * (int32_t)(k))
#define LIST_TY CAP_IN(M, 16)                           /* a row's text: its capitals rows 2 .. 12 of the row */
#define LIST_NX 64
static void list_row(int32_t y, int sel, const char *tag, uint16_t tc, const char *name, uint16_t nc, int32_t x1)
{
    uint16_t bg = sel ? T_THEME : T_SURF;
    if (sel)
        cv_rrect(6, y, 228, 16, 4, T_THEME, T_SURF);
    GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_text_on(14, y + LIST_TY, &AF_M, tag, sel ? T_INK : tc, bg);
    if (name[0])
        GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_free_text(LIST_NX, y + LIST_TY, &AF_M, name, sel ? T_INK : nc, bg, x1 - LIST_NX);
}
/* an empty list: a title and a hint, centred */
static void note_line(int32_t y, const char *s, uint16_t fg)   /* centred S; "[K2] ADD PATTERN": a key hint */
{
    uint32_t n;
    int32_t id = kc_tag(s, &n);
    if (id < 0) {
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "panel note centred");
        cv_text_in(0, y, 240, &AF_S, s, fg, T_SURF);
        return;
    }
    s += n;
    while (*s == ' ')
        s++;
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "panel note hint centred");
    cv_text_on(cv_keycap(HALF_UP(240 - kh_ink((uint32_t)id, s)), y + 1, (uint32_t)id, T_KEY, T_KINK, T_SURF) + KH_GAP, y,
               &AF_S, s, fg, T_SURF);
}
static void panel_note(const char *a, const char *b, const char *c)
{
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "panel note centred");
    cv_text_in(0, 30, 240, &AF_M, a, T_TEXT, T_SURF);
    if (b) note_line(60, b, T_MID);
    if (c) note_line(82, c, T_DIM);
}
/* the same title led by an icon (16 px, the accent's: the header message's, ui_draw.c draw_head) in the middle of
 * the panel: the title's capitals centred up and down in it, the icon's ink on them; across, the pair centred by
 * its ink */
#define PA_GAP 6                                        /* icon ink -> title ink (setup_title's) */
static void panel_alert(uint32_t id, const char *a)
{
    int32_t bi[4], bt[4], x, y = CAP_IN(M, (int32_t)graph_h());
    icon_ink(16, id, bi);
    text_ink(&AF_M, a, bt);
    x = HALF_UP(240 - (bi[2] - bi[0] + PA_GAP + bt[2] - bt[0])) - bi[0];
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "panel alert centred");
    GFX_HOOK_ALIGN(0, y + AF_M_CAP_Y, 0, y + AF_M_CAP_Y + AF_M_CAP_H, AL_V | AL_PASS, "panel alert icon on its title's line");
    cv_icon_in(x, y + AF_M_CAP_Y, 0, AF_M_CAP_H, 16, id, T_ACCENT, T_SURF);
    GFX_HOOK_ALIGN(0, 0, 0, (int32_t)graph_h(), AL_V | AL_PASS, "panel alert title in the panel's middle");
    cv_text_on(x + bi[2] + PA_GAP - bt[0], y, &AF_M, a, T_TEXT, T_SURF);
}

/* PATCHES (SPEC 3.6): ONE list of places (the card says how many) around the cursor: every sound of the TYPE shown (the factory sounds in category order,
 * a stored copy in its sound's place, then the user patches), then (TYPE ALL) the empty places, dim. Tag: the engine; an empty
 * place its number; name TEXT; favourites starred (the accent) */
static void graph_browse(void)
{
    uint32_t total, cur = pl_cursor(&total), occ, e, k, c0;
    int32_t row;
    (void)preset_pos(&occ);
    if (!total) {
        char b[24];
        if (list_mode() == 1u) {
            panel_note("NO FAVORITES", "TYPE ALL TO ADD SOUNDS", 0);
            return;
        }
        str_cpy(b, "NO ", sizeof b);                    /* a type with no sound (OTHER: tag your own) */
        str_cpy(b + 3, list_name(list_mode()), sizeof b - 3);
        str_cpy(b + str_len(b), " SOUNDS", sizeof b - str_len(b));
        panel_note(b, "[K2] ANOTHER TYPE", "TAG YOURS WHEN SAVING");
        return;
    }
    c0 = cur < total ? cur : 0u;
    for (row = -3; row <= 3; row++) {
        int32_t y = LIST_Y(row + 3);
        char tag[6], nm[13];
        uint32_t index;
        int sel;
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
        if (index >= occ) {                              /* an empty place */
            fmt_int(tag, (int32_t)index + 1);
            list_row(y, sel, tag, T_DIM, "EMPTY", sel ? T_TEXT : T_DIM, 232);
            continue;
        }
        e = preset_at(index, &k);
        if (e == NENGINES) {                             /* a stored patch: a user patch's engine tag, or a factory sound's copy: */
            uint32_t p, se = up_shadow(k, &p);           /* the sound's engine tag, the copy's name */
            str_cpy(tag, eng_abbr(ENGINES[se == NENGINES ? up_engine(k) : se]->name), sizeof tag);
            up_name(k, nm);
        } else {
            str_cpy(tag, eng_abbr(ENGINES[e]->name), sizeof tag);
            str_cpy(nm, ds_name(e, k), sizeof nm);
        }
        list_row(y, sel, tag, T_DIM, nm, T_TEXT, list_fav(e, k) ? 212 : 232);
        if (list_fav(e, k)) {
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row star centred up/down");
            cv_icon_in(214, y, 0, 16, 12, ICON_X_STAR, sel ? T_INK : T_ACCENT, sel ? T_THEME : T_SURF);
        }
    }
}
/* the EDIT layer (ui_layer.c): the sound loaded, as the browser's selected row: "03" (its place in KNOB 2's list,
 * the engine's sounds) or "U07", the name, the star of a favourite */
static void engine_sound_row(int32_t y)
{
    uint32_t total, cur = eng_list_pos(&total), u = user_of(TSEL);
    char tag[6], nm[13];
    int fav = preset_favorite();
    if (u < UP_SLOTS && up_shadow(u, &total) == NENGINES) {
        up_slot_label(tag, u);
        up_name(u, nm);
    } else if (u < UP_SLOTS) {                         /* a preset's stored copy: its place's number, the copy's name */
        tag[0] = (char)('0' + (cur + 1u) / 10u % 10u);
        tag[1] = (char)('0' + (cur + 1u) % 10u);
        tag[2] = 0;
        up_name(u, nm);
    } else {
        tag[0] = (char)('0' + (cur + 1u) / 10u % 10u);
        tag[1] = (char)('0' + (cur + 1u) % 10u);
        tag[2] = 0;
        str_cpy(nm, ds_name(TSEL->eng_req % NENGINES, TSEL->preset), sizeof nm);   /* (no factory sound: INIT) */
    }
    list_row(y, 1, tag, T_DIM, nm, T_TEXT, fav ? 212 : 232);
    if (fav) {
        GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row star centred up/down");
        cv_icon_in(214, y, 0, 16, 12, ICON_X_STAR, T_INK, T_THEME);
    }
    (void)total;
}
/* EDIT's first page, ENGINE (SPEC 1.2): the engines round the selected one (the previous and the next of PRESETS'
 * order, as KNOB 1 steps; DRUM is never in it) and under them the sound loaded, as the browser's selected row */
static void graph_engine(void)
{
    uint32_t e = TSEL->eng_req % NENGINES;
    int32_t d;
    for (d = -1; d <= 1; d++) {
        const engine_t *en = ENGINES[d < 0 ? eng_step(e, -1) : d > 0 ? eng_step(e, 1) : e];
        list_row(LIST_Y((uint32_t)(d + 1)), d == 0, d == 0 ? "ENG" : eng_abbr(en->name), T_DIM, en->name, T_MID, 232);
    }
    engine_sound_row(LIST_Y(4));
}
/* SAVE > PATTERNS: the sixteen slots around the one picked ("P03  MELODY" factory default, "P03  SAVED", an empty
 * slot "P14  --" dim) */
static void graph_pats(void)
{
    uint32_t n = pat_count(), cur = pat_pick();
    int32_t row, first = clamp((int32_t)cur - 3, 0, (int32_t)n > 7 ? (int32_t)n - 7 : 0);
    for (row = 0; row < 7 && (uint32_t)(first + row) < n; row++) {
        uint32_t k = (uint32_t)(first + row);
        char tag[4], nm[13];
        pat_label(k, tag, nm);
        list_row(LIST_Y(row), k == cur, tag, T_MID, nm, pat_has(k) ? T_TEXT : T_DIM, 232);
    }
}
/* SAVE > TOOLS: the panel names both actions in full, with the knob that does each and a line on what it does, for
 * the selected part (the cards say CLEAR and INIT: five letters at most). LARGE (a 62 px panel): the part and the two
 * key hints only */
static void tools_block(int32_t y, const char *knob, const char *name, const char *l1, const char *l2)
{
    uint32_t n;
    int32_t id = kc_tag(knob, &n), x = 10;
    if (id >= 0)
        x = cv_keycap(x, y + 2, (uint32_t)id, T_KEY, T_KINK, T_SURF) + KH_GAP;
    cv_text_on(x, y + LIST_TY, &AF_M, name, T_TEXT, T_SURF);
    cv_text_on(10, y + 20, &AF_S, l1, T_MID, T_SURF);
    cv_text_on(10, y + 31, &AF_S, l2, T_MID, T_SURF);
}
static void graph_tools(void)
{
    char part[8] = "PART 1";
    part[5] = (char)('1' + song.sel % NTRK);
    if (graph_h() < 100u) {
        panel_note(part, "[K1] CLEAR PATTERN", "[K2] INIT SOUND");
        return;
    }
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "panel note centred");
    cv_text_in(0, 4 + LIST_TY, 240, &AF_M, part, T_ACCENT, T_SURF);
    tools_block(22, "[K1]", "CLEAR PATTERN", "EMPTIES THE SELECTED", "PART'S PATTERN");
    tools_block(74, "[K2]", "INIT SOUND", "RESETS THE SELECTED PART'S", "SOUND TO ITS DEFAULT");
}
/* the AUTOMATION rows' columns (M): the kind at 14 (LOCK AUTO STEP PLAY, to x 54), the step / PLAY's ON at EV_SX, the
 * name / QUANTIZE's ON at EV_NX (QUANTIZE ends at x 86), the value right at 226 */
#define EV_SX 60
#define EV_NX 92
/* a row's value (M) and its unit (S, on the value's baseline, as a card's) ending at x 226; the value's left x */
static int32_t ev_value(int32_t y, const char *v, const char *unit, uint16_t fg, uint16_t bg)
{
    int32_t x = 226;
    GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    if (unit && unit[0]) {
        GFX_HOOK_ALIGN(0, 0, 0, y + LIST_TY + AF_M.asc, AL_B, "list row unit on its value's baseline");
        x = cv_text_r(x, y + LIST_TY + AF_M.asc - AF_S.asc, &AF_S, unit, fg, bg) - 2;
    }
    return cv_text_r(x, y + LIST_TY, &AF_M, v, fg, bg);
}
/* SEQ > AUTOMATION (ui_events.c): the track's rows around the one selected: PLAY first (ON / OFF and the counts),
 * QUANTIZE (ON / OFF and the steps nudged), then a row each: the kind (LOCK in the accent, AUTO; STEP: a step's CHANCE,
 * RATCH, NUDGE or VEL, a NUDGE or VEL that does not play DIM), the step, the name, the value at the right; the last row
 * + ADD (LOCK, CHANCE, RATCH, NUDGE or VEL: what OCT+ adds) */
static void graph_events(void)
{
    const track_t *t = TSEL;
    uint16_t rw[EV_ROWS];
    uint32_t n = ev_list(rw), cur = ev_row(n), k;
    int32_t row, first = clamp((int32_t)cur - 3, 0, (int32_t)n > 7 ? (int32_t)n - 7 : 0);
    for (row = 0; row < 7 && (uint32_t)(first + row) < n; row++) {
        int32_t y = LIST_Y(row);
        int sel;
        uint16_t bg, c;
        k = (uint32_t)(first + row);
        c = rw[k];
        sel = k == cur;
        bg = sel ? T_THEME : T_SURF;
        if (sel)
            cv_rrect(6, y, 228, 16, 4, T_THEME, T_SURF);
        if (c >> 8 == EVK_ADD) {
            ev_fix();
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(14, y + LIST_TY, &AF_M, ui.ev_id == EV_CHANCE ? "+ ADD CHANCE" : ui.ev_id == EV_RATCH ?
                       "+ ADD RATCH" : ui.ev_id == EV_NUDGE ? "+ ADD NUDGE" : ui.ev_id == EV_VEL ?
                       (ui.ev_vel ? "+ HIDE VEL" : "+ SHOW VEL") : "+ ADD LOCK", sel ? T_INK : T_DIM, bg);
            break;
        }
        if (c >> 8 == EVK_TOP) {                     /* PLAY ON / OFF, the counts: "EVENT 3  LOCK 2" */
            char b[24];
            str_cpy(b, "EVENT ", sizeof b);
            fmt_int(b + str_len(b), (int32_t)(motion_count(t) - motion_lock_count(t)));
            str_cpy(b + str_len(b), "  LOCK ", sizeof b - str_len(b));
            fmt_int(b + str_len(b), (int32_t)motion_lock_count(t));
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(14, y + LIST_TY, &AF_M, "PLAY", sel ? T_INK : T_MID, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(EV_SX, y + LIST_TY, &AF_M, motion_enabled(t) ? "ON" : "OFF",
                       sel ? T_INK : motion_enabled(t) ? T_TEXT : T_DIM, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_r(226, y + LIST_TY, &AF_M, b, sel ? T_INK : T_MID, bg);
            continue;
        }
        if (c >> 8 == EVK_QNT) {                     /* QUANTIZE ON / OFF (1.2), the steps nudged: "NUDGED 3" */
            char b[16];
            str_cpy(b, "NUDGED ", sizeof b);
            fmt_int(b + str_len(b), (int32_t)ev_nudged(t));
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(14, y + LIST_TY, &AF_M, "QUANTIZE", sel ? T_INK : T_MID, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(EV_NX, y + LIST_TY, &AF_M, t->p[P_SQNT] ? "ON" : "OFF", sel ? T_INK : T_TEXT, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_r(226, y + LIST_TY, &AF_M, b, sel ? T_INK : T_MID, bg);
            continue;
        }
        if (c >> 8 != EVK_REC) {                     /* a step's CHANCE / RATCH / NUDGE: "STEP 05 CHANCE  50%" */
            uint32_t s = c & 0xFFu, kd = c >> 8;
            int32_t v = ev_sval(&t->step[s], kd);
            uint16_t vc = (kd == EVK_NUDGE && !ev_nudge_plays(t, &t->step[s])) ||   /* (QUANTIZE ON ..) */
                          (kd == EVK_VEL && !ev_vel_plays(&t->step[s])) ? T_DIM : T_TEXT;
            char b[8];
            b[0] = (char)('0' + (s + 1u) / 10u);
            b[1] = (char)('0' + (s + 1u) % 10u);
            b[2] = 0;
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(14, y + LIST_TY, &AF_M, "STEP", sel ? T_INK : T_MID, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(EV_SX, y + LIST_TY, &AF_M, b, sel ? T_INK : T_MID, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(EV_NX, y + LIST_TY, &AF_M, kd == EVK_CHANCE ? "CHANCE" : kd == EVK_RATCH ? "RATCH" :
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
            ev_value(y, b, kd == EVK_CHANCE ? "%" : kd == EVK_NUDGE ? "/16" : "", sel ? T_INK : vc, bg);
            continue;
        }
        {
            const motion_event_t *e = &motion.event[c & 0xFFu];
            uint32_t id = MOTION_ID(e), lock = (e->param & MOTION_LOCK) != 0u;
            char b[16], nm[12];
            const char *unit;
            const param_desc_t *d = track_desc(t, id);
            int32_t x;
            b[0] = (char)('0' + ((e->place & 63u) + 1u) / 10u);
            b[1] = (char)('0' + ((e->place & 63u) + 1u) % 10u);
            b[2] = 0;
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(14, y + LIST_TY, &AF_M, lock ? "LOCK" : "AUTO", sel ? T_INK : lock ? T_ACCENT : T_MID, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(EV_SX, y + LIST_TY, &AF_M, b, sel ? T_INK : T_MID, bg);
            param_format(d, e->value, b, &unit);
            ev_name(t, id, nm);
            x = ev_value(y, b, unit, sel ? T_INK : T_TEXT, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_free_text(EV_NX, y + LIST_TY, &AF_M, nm, sel ? T_INK : T_TEXT, bg, x - 8 - EV_NX);
        }
    }
}
/* SAVE > MULTI: the sixteen multis around the one picked ("M05  LOFI JAM"; unnamed "USED"; empty "M14  --" dim) */
static void graph_slots(void)
{
    uint32_t cur = (uint32_t)(song.g[G_SLOT] - 1) % MU_SLOTS;
    int32_t row, first = clamp((int32_t)cur - 3, 0, (int32_t)MU_SLOTS - 7);
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
    knob_arc(x, y, r, cov, ang, a0, a1, T_RAISE, vc, T_SURF);
}

static uint32_t trk_level(uint32_t c) { return (uint32_t)trk[c].p[P_LEVEL] & 127u; }   /* LEVEL 0..127 */

static void trk_short_name(uint32_t c, char *b)      /* the track's sound, b holds 13 */
{
    const track_t *t = &trk[c];
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), b);
    else if (ds_count(t->eng_req % NENGINES))
        str_cpy(b, ds_name(t->eng_req % NENGINES, t->preset), 13);
    else
        str_cpy(b, e->name, 13);
}

/* MENU > LARGE: the four tracks in the strip under the tall cards (57 x LG_H_GRAPH each): the cushion (the selected
 * one the accent), REC / ARM / MUTE as on the full mixer, the sound's name, LEVEL as a gauge (THEME: the cards'),
 * the output meter under it (MID); a muted track DIM */
#define TSS_GY 40                                    /* the LEVEL gauge, 3 px */
#define TSS_MY 48                                    /* the meter, 3 px */
static void track_strip(uint32_t c, uint32_t sel, uint32_t st, uint32_t mute, uint32_t arm, uint32_t hot, uint32_t lvl,
                        const char *b)
{
    int32_t gw = CARD_W - 10, m = ts.meter[c] * gw / (TS_MH - 2), fx = (int32_t)lvl * gw / 127;
    uint16_t vc = mute ? T_DIM : T_PTHEME(c);
    cv_begin(CARD_W, LG_H_GRAPH, T_BG);
    cv_rrect(0, 0, CARD_W, LG_H_GRAPH, 5, T_SURF, T_BG);
    cv_part(4, 4, 16, c, sel, T_SURF);
    if (st)
        GFX_HOOK_ALIGN(0, 4, 0, 20, AL_V, "mixer badge on the cushion's line");
    if (st == 1u || st == 2u)
        cv_keycap(53 - kc_w(st == 1u ? KC_REC : KC_ARM), 5, st == 1u ? KC_REC : KC_ARM, st == 1u ? T_REC : T_ACCENT,
                  T_KINK, T_SURF);
    else if (st)
        cv_keycap(53 - kc_w(KC_MUTE), 5, KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_KINK, T_SURF);
    if (mute && arm)                                 /* armed and muted: MUTE in place of the name */
        GFX_HOOK_ALIGN(0, 22 + AF_S_CAP_Y, 0, 22 + AF_S_CAP_Y + AF_S_CAP_H, AL_V, "mixer MUTE badge on the name's line");
    if (mute && arm)
        cv_keycap(5, 22 + AF_S_CAP_Y + HALF_UP(AF_S_CAP_H - KC_H), KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_KINK, T_SURF);
    else
        cv_free_text(5, 22, &AF_S, b, mute ? T_DIM : sel ? T_TEXT : T_MID, T_SURF, CARD_W - 10);
    cv_rrect(5, TSS_GY, gw, 3, 1, ux.style ? T_LINE : T_BG, T_SURF);
    cv_rrect(5, TSS_GY, fx < 3 ? 3 : fx, 3, 1, hot == 1u ? T_ACCENT : vc, T_BG);
    cv_rrect(5, TSS_MY, gw, 3, 1, T_RAISE, T_SURF);
    if (m)
        cv_rrect(5, TSS_MY, m < 3 ? 3 : m, 3, 1, T_MID, T_RAISE);
    cv_blit((uint32_t)CARD_X(c), LG_Y_GRAPH);
}
/* HOME > MIXER (DUELL 5b.3): two wide strips (234 x 58, 4 apart) in place of four columns. A strip: the part's badge
 * (24 px cell: its number on a cushion in the part's colour, MONO: part 2 an outline), the sound's name in M, the engine
 * right of it (S, MID) and REC / ARM / MUTE as a keycap at the end; under it three items, each its caption (S), its
 * value (M) and a 3 px gauge: LEVEL (wide, its output meter under the gauge), PAN (from the centre; SPLIT: L / R, DIM,
 * fixed) and REV. The selected part's strip has a 1 px outline in its colour. A muted part's values are DIM */
#define MX_W 234
#define MX_H 58
#define MX_PITCH 62
#define MX_LX 8                                      /* LEVEL's item: x 8 .. 110 */
#define MX_LW 102
#define MX_PX 120                                    /* PAN's: 120 .. 170 */
#define MX_PW 50
#define MX_RX 180                                    /* REV's: 180 .. 228 */
#define MX_RW 48
#define MX_CAP 27                                    /* the captions' capitals from row 27 (S, 9 rows), the values' from 38 (M, 11) */
#define MX_VAL 38
#define MX_GY 51                                     /* the gauges, 3 px; LEVEL's meter under it on row 55 */
#define MX_MY 55
static void mixer_gauge(int32_t x, int32_t w, int32_t fx0, int32_t fx1, uint16_t c)
{
    cv_rrect(x, MX_GY, w, 3, 1, T_RAISE, T_SURF);
    if (fx1 > fx0)
        cv_rrect(x + fx0, MX_GY, fx1 - fx0 < 3 ? 3 : fx1 - fx0, 3, 1, c, T_RAISE);
}
static void mixer_item(int32_t x, int32_t w, const char *cap, const char *v, uint16_t vc)
{
    cv_text(x, MX_CAP - AF_S_CAP_Y, &AF_S, cap, T_DIM);
    cv_free_text(x, MX_VAL - AF_M_CAP_Y, &AF_M, v, vt(vc, T_SURF), T_SURF, w);
}
static void draw_mixer_strip(uint32_t c)
{
    track_t *t = &trk[c];
    uint32_t sel = c == song.sel, lvl = trk_level(c), mute = t->p[P_MUTE] != 0;
    uint32_t arm = (song.rec >> c) & 1u, st = arm ? (song.playing ? 1u : 2u) : mute ? 3u : 0u, sig;
    uint32_t hot = sel && ui.hot_t ? ui.hot_col + 1u : 0u;   /* the knob just turned: 1 LEVEL, 2 PAN, 3 REV, 4 MUTE (its badge) */
    int32_t pk = t->peak, m, pan = clamp(t->p[P_PAN], -64, 63), rv = clamp(t->p[P_REV], 0, 127), xr = MX_W - 8, xe, ew;
    uint16_t th = T_PTHEME(c), vc = mute ? T_DIM : th, bg = T_BG;
    uint32_t split = duell_mode_req == DM_SPLIT;     /* (SPLIT: the PAN fixed, 1 left, 2 right, DIM) */
    const char *en = ENGINES[t->eng_req % NENGINES]->name;
    char b[16], v[12];
    if (split)
        pan = c ? 63 : -64;
    t->peak = 0;
    trk_short_name(c, b);
    m = mute ? 0 : meter_px(pk);
    if (m < ts.meter[c] - 1)
        m = ts.meter[c] - 1;                         /* falls ~2 dB a frame */
    ts.meter[c] = (uint8_t)(m < 0 ? 0 : m);
    sig = str_hash(1u + sel + st * 2u + hot * 32u + t->eng_req * 64u + ux.bw * 5u, b) + lvl * 7919u + ts.meter[c] * 131u +
          (uint32_t)(pan + 128) * 104729u + (uint32_t)rv * 1299709u + mute * 3u + split * 0x51ED27u + (uint32_t)th * 17u;
    if (!ui.force && sig == ts.col[c])
        return;
    ts.col[c] = sig;
    cv_begin(MX_W, MX_H, bg);
    if (sel) {                                       /* the selection: a 1 px outline in the part's colour */
        cv_rrect(0, 0, MX_W, MX_H, 6, th, bg);
        cv_rrect(1, 1, MX_W - 2, MX_H - 2, 5, T_SURF, th);
    } else {
        cv_rrect(0, 0, MX_W, MX_H, 6, T_SURF, bg);
    }
    cv_part(3, 2, 24, c, 1u, T_SURF);
    if (st) {                                        /* REC / ARM / MUTE at the end of the name's line */
        uint32_t id = st == 1u ? KC_REC : st == 2u ? KC_ARM : KC_MUTE;
        xr -= kc_w(id);
        GFX_HOOK_ALIGN(0, 2, 0, 26, AL_V, "mixer strip badge on the badge's line");
        cv_keycap(xr, 2 + (24 - KC_H) / 2, id, st == 1u ? T_REC : st == 2u ? T_ACCENT : hot == 4u ? T_ACCENT : T_KEY, T_KINK, T_SURF);
        xr -= 6;
    }
    ew = text_w(&AF_S, en);                          /* the engine, right-aligned (S, MID) */
    if (ew > 76)
        ew = 76;
    xe = xr - ew;
    cv_free_text(xe, 2 + CAP_IN(S, 24), &AF_S, en, mute ? T_DIM : T_MID, T_SURF, ew);
    cv_free_text(32, 2 + CAP_IN(M, 24), &AF_M, b, mute ? T_DIM : sel ? T_TEXT : T_MID, T_SURF, xe - 8 - 32);
    {   /* LEVEL: dB (OFF at 0), the gauge, the output meter under it */
        int32_t d = LEVEL_DB_X10[lvl], mw = ts.meter[c] * MX_LW / (TS_MH - 2);
        if (lvl) {
            fmt_int(v, (d + (d < 0 ? -5 : 5)) / 10);
            str_cpy(v + str_len(v), " dB", 4);
        } else {
            str_cpy(v, "OFF", sizeof v);
        }
        mixer_item(MX_LX, MX_LW, "LEVEL", v, hot == 1u ? T_ACCENT : lvl ? vc : T_DIM);
        mixer_gauge(MX_LX, MX_LW, 0, (int32_t)lvl * MX_LW / 127, hot == 1u ? T_ACCENT : vc);
        cv_rrect(MX_LX, MX_MY, MX_LW, 3, 1, T_RAISE, T_SURF);
        if (mw)
            cv_rrect(MX_LX, MX_MY, mw < 3 ? 3 : mw, 3, 1, T_MID, T_RAISE);
    }
    {   /* PAN from the centre (SPLIT: L / R, fixed) and the REV send; numbers without the % */
        uint16_t pc = split ? T_DIM : hot == 2u ? T_ACCENT : vc, rc = hot == 3u ? T_ACCENT : vc;
        int32_t cx = MX_PW / 2, px = (pan + 64) * MX_PW / 127;
        v[0] = '+';
        fmt_int(pan > 0 ? v + 1 : v, pan * 100 / 64);
        if (split)
            str_cpy(v, c ? "R" : "L", sizeof v);
        mixer_item(MX_PX, MX_PW, "PAN", v, pc);
        mixer_gauge(MX_PX, MX_PW, px < cx ? px : cx, px < cx ? cx : px, pc);
        cv_rect(MX_PX + cx, MX_GY - 1, 1, 5, T_DIM);                      /* the centre */
        fmt_int(v, (rv * 100 + 63) / 127);
        mixer_item(MX_RX, MX_RW, "REV", v, rc);
        mixer_gauge(MX_RX, MX_RW, 0, rv * MX_RW / 127, rc);
    }
    cv_blit(3, (uint32_t)(Y_GRAPH + (int32_t)c * MX_PITCH));
}
static void draw_tracks(void)
{
    uint32_t c, sk = strip_kind() == SK_TRK;
    if (ui.force) {                                  /* the BG beside the columns only, they cover the rest (#181: */
        for (c = 0; c < NTRK; c++)
            ts.meter[c] = 0;
        if (sk) {
            draw_gaps(graph_y(), graph_h());         /* the whole panel cleared first flashed on every sound load and
                                                      * track change); LINE: divided as the cards above */
            for (c = NSHOWN; c < NTRK; c++)          /* (DUELL: the hidden tracks' columns stay empty) */
                lcd_fill((uint32_t)CARD_X(c), graph_y(), CARD_W, graph_h(), T_BG);
        } else {                                     /* the BG round the two wide strips; LINE: a rule between them */
            lcd_fill(0, Y_GRAPH, 3, H_GRAPH, T_BG);
            lcd_fill(3 + MX_W, Y_GRAPH, 240 - 3 - MX_W, H_GRAPH, T_BG);
            lcd_fill(3, Y_GRAPH + MX_H, MX_W, MX_PITCH - MX_H, T_BG);
            lcd_fill(3, Y_GRAPH + NSHOWN * MX_PITCH - (MX_PITCH - MX_H), MX_W, H_GRAPH - NSHOWN * MX_PITCH + (MX_PITCH - MX_H), T_BG);
            if (ux.style) {
                lcd_fill(3, (uint32_t)(Y_GRAPH + MX_H + 1), MX_W, 1, T_RULE);
                GFX_HOOK_RULE(3, Y_GRAPH + MX_H + 1, MX_W, 1);
            }
        }
    }
    if (!sk) {
        for (c = 0; c < NSHOWN; c++)
            draw_mixer_strip(c);
        return;
    }
    for (c = 0; c < NSHOWN; c++) {
        track_t *t = &trk[c];
        uint32_t sel = c == song.sel, lvl = trk_level(c), mute = t->p[P_MUTE] != 0;
        uint32_t arm = (song.rec >> c) & 1u, st = arm ? (song.playing ? 1u : 2u) : mute ? 3u : 0u, sig;
        uint32_t hot = sel && ui.hot_t ? ui.hot_col + 1u : 0u;   /* the knob just turned (hot_col + 1): 1 LEVEL, 2 PAN,
                                                                * 3 REV, 4 MUTE (its badge) */
        int32_t pk = t->peak, m, pan = clamp(t->p[P_PAN], -64, 63), rv = clamp(t->p[P_REV], 0, 127);
        uint32_t split = duell_mode_req == DM_SPLIT;   /* (step 5: SPLIT, the PAN fixed: 1 left, 2 right, DIM) */
        if (split)
            pan = c ? 63 : -64;
        char b[16];
        t->peak = 0;
        trk_short_name(c, b);
        m = mute ? 0 : meter_px(pk);
        if (m < ts.meter[c] - 1)
            m = ts.meter[c] - 1;                     /* falls ~2 dB a frame */
        ts.meter[c] = (uint8_t)(m < 0 ? 0 : m);
        sig = str_hash(1u + sel + st * 2u + (mute && arm) * 16u + hot * 32u, b) + lvl * 7919u +
              ts.meter[c] * 131u + (uint32_t)(pan + 128) * 104729u + (uint32_t)rv * 1299709u + mute * 3u + sk * 0x9E37u +
              split * 0x51ED27u;
        if (!ui.force && sig == ts.col[c])
            continue;
        ts.col[c] = sig;
        if (sk) {
            track_strip(c, sel, st, mute, arm, hot, lvl, b);
            continue;
        }
    }
}
/* HOME > TRACKS (1.3, Discussions #112 / #134: MENU > DISPLAY > HOME TRACKS): the four tracks as rows in the panel, one
 * under the other (29 px, 2 apart), in place of the scope; the cards above stay HOME's four (the selected track's). A row:
 * the track's cushion at the left; on its first line the engine's icon and the sound's name (MENU > LARGE: the name in M,
 * no icon), a MUTE badge at its right when muted; under it the 16 steps of the page of the track's pattern that plays
 * (stopped: the selected track's cursor page, the others' first): a note a bar (accented: taller, the accent), a TIE after
 * a note the bar carried on from the one before, DRUM hits a pad (two hits or more: two pads), an empty step or a REST a
 * stub (a TIE with nothing to carry: brighter); the step playing on a raised cell. LEN > 16: a pip per page right of the
 * steps (the page shown THEME). At the right the track's output meter. The selected track's row is the selection (THEME,
 * its ink INK, as a list's selected row); a muted track's steps DIM. Each row is its own canvas with its own signature
 * (ts.col, the MIXER's: the two never show together); a meter that moved alone redraws its slot only. LINE: the rows on
 * BG, 1 px rules between them */
#define TR_H 29                                      /* a row; rows TR_PITCH apart from Y_GRAPH, 3 .. 237 across */
#define TR_PITCH 31
#define TR_W 234
#define TR_NY(lg) ((lg) ? 2 : 4)                     /* the name line's capitals from row 4 (LARGE, M: 2) */
#define TR_SB 26                                     /* the steps: bars standing on row 25 */
#define TR_BW 9                                      /* a step's bar, 11 px apart, 4 more between groups of 4 */
#define TR_PX 216                                    /* the page pips */
#define TR_MX 226                                    /* the meter: 4 x 22 from row 4 */
#define TR_MY 4
#define TR_MH 22
static int32_t tr_bx(uint32_t i) { return 24 + (int32_t)i * 11 + (int32_t)(i / 4u) * 4; }
static void tr_meter(int32_t x, int32_t y, uint32_t c, uint16_t slot, uint16_t fill, uint16_t under)
{
    int32_t m = ts.meter[c] * TR_MH / (TS_MH - 2);
    cv_rrect(x, y, 4, TR_MH, 2, slot, under);
    if (m)
        cv_rrect(x, y + TR_MH - m, 4, m, m >= 4 ? 2 : 0, fill, slot);
}
static void home_track_row(uint32_t c)
{
    track_t *t = &trk[c];
    const step_t *s = seq_steps(t);
    uint32_t sel = c == song.sel, mute = t->p[P_MUTE] != 0, large = (ui_prefs & PREF_LARGE) != 0u;
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), np = (len + 15u) / 16u, pg, p0, ph, i, sig, carry = 0;
    int32_t y = Y_GRAPH + (int32_t)c * TR_PITCH, m, x, xr = 222, ch = large ? AF_M_CAP_H : AF_S_CAP_H;
    uint16_t bg = sel ? T_PTHEME(c) : T_SURF, half = ux_mix(T_PTHEME(c), T_INK, 35);
    uint16_t cn = sel ? (mute ? ux_mix(T_PTHEME(c), T_INK, 55) : T_INK) : mute ? T_DIM : T_PTHEME(c);   /* a note */
    uint16_t ca = sel || mute ? cn : T_ACCENT, cs = sel ? half : T_RAISE;   /* accented; a stub */
    uint16_t cp = sel ? ux_mix(T_PTHEME(c), T_INK, 20) : T_RAISE, cpl = sel ? T_INK : mute ? T_MID : T_TEXT;   /* playing */
    char b[16];
    pg = song.playing ? t->seq_idx / 16u : sel ? ui.cursor / 16u : 0u;
    if (pg >= np)
        pg = np - 1u;
    p0 = pg * 16u;
    ph = song.playing && t->seq_idx / 16u == pg ? t->seq_idx % 16u : 0xFFu;
    trk_short_name(c, b);
    m = mute ? 0 : meter_px(t->peak);
    t->peak = 0;
    if (m < ts.meter[c] - 1)
        m = ts.meter[c] - 1;                         /* (falls as the MIXER's) */
    sig = str_hash(1u + sel + mute * 2u + large * 4u + t->eng_req * 8u, b) + steps_hash(t) * 31u + pg * 7919u +
          ph * 104729u + len * 1299709u;
    if (!ui.force && sig == ts.col[c]) {
        if (m != ts.meter[c]) {                      /* the meter alone: its slot */
            ts.meter[c] = (uint8_t)m;
            cv_begin(4, TR_MH, bg);
            tr_meter(0, 0, c, cs, sel ? T_INK : T_MID, bg);
            cv_blit(3 + TR_MX, (uint32_t)(y + TR_MY));
        }
        return;
    }
    ts.col[c] = sig;
    ts.meter[c] = (uint8_t)m;
    cv_begin(TR_W, TR_H, T_BG);
    cv_rrect(0, 0, TR_W, TR_H, 5, bg, T_BG);
    cv_icon_mid(4, TR_H / 2, 16, trk_icon(c, 1), sel ? T_INK : mute ? T_DIM : T_MID, bg);
    if (mute) {                                      /* MUTE: its badge at the name line's end */
        xr -= kc_w(KC_MUTE);
        GFX_HOOK_ALIGN(0, TR_NY(large), 0, TR_NY(large) + ch, AL_V, "tracks row badge on the name's line");
        cv_keycap(xr, TR_NY(large) + (ch - KC_H) / 2, KC_MUTE, sel ? T_INK : T_KEY, sel ? T_PTHEME(c) : T_INK, bg);
        xr -= 6;
    }
    x = 24;
    if (FELUCCA_ICONS && !large) {                   /* the engine's icon on the name's capitals */
        uint32_t ei = engine_icon(ENGINES[t->eng_req % NENGINES]->name);
        if (ei != ICON_NONE)
            GFX_HOOK_ALIGN(0, TR_NY(large), 0, TR_NY(large) + ch, AL_V, "tracks row icon on the name's line");
        x += cv_icon_in(x, TR_NY(large), 0, ch, 12, ei, sel ? T_INK : T_MID, bg) + 5;
    }
    cv_free_text(x, TR_NY(large) - (large ? AF_M_CAP_Y : AF_S_CAP_Y), large ? &AF_M : &AF_S, b, sel ? T_INK : mute ? T_DIM : T_TEXT,
                 bg, xr - x);
    for (i = p0; i-- > 0u;)                          /* a TIE at the page's start: carried from a note before it? */
        if (s[i].time != ST_TIE) {
            carry = s[i].time == ST_NOTE && s[i].n;
            break;
        }
    for (i = 0; i < 16u && p0 + i < len; i++) {
        const step_t *st = &s[p0 + i];
        int32_t bx = tr_bx(i);
        uint16_t under = i == ph ? cp : bg;
        if (i == ph)                                 /* the step playing: a raised cell */
            cv_rrect(bx - 1, 15, TR_BW + 2, 13, 2, cp, bg);
        if (st->time == ST_TIE && carry) {           /* carried on: joined to the bar before */
            int32_t x0 = i ? tr_bx(i - 1u) + TR_BW : bx - 2;
            cv_rect(x0, TR_SB - 7, bx + 2 - x0, 7, i == ph ? cpl : cn);
            cv_rrect(bx, TR_SB - 7, TR_BW, 7, 2, i == ph ? cpl : cn, under);
        } else if (st->time == ST_NOTE && st->n) {   /* notes: a bar, accented taller */
            int acc = (st->flags & SF_ACCENT) != 0u;
            cv_rrect(bx, TR_SB - (acc ? 10 : 7), TR_BW, acc ? 10 : 7, 2, i == ph ? cpl : acc ? ca : cn, under);
            carry = 1;
            continue;
        } else if (st->time == ST_NOTE && st->hit) { /* DRUM hits: a pad, two for two hits or more */
            uint16_t pc = i == ph ? cpl : (st->acc & st->hit) ? ca : cn;
            cv_rrect(bx, TR_SB - 4, TR_BW, 4, 1, pc, under);
            if (st->hit & (st->hit - 1u))
                cv_rrect(bx, TR_SB - 9, TR_BW, 4, 1, pc, under);
        } else {                                     /* empty, a REST, a TIE carrying nothing */
            cv_rect(bx, TR_SB - 2, TR_BW, 2, i == ph ? (sel ? T_INK : T_MID) : st->time == ST_TIE ? (sel ? T_INK : T_MID) : cs);
        }
        carry = st->time == ST_TIE && carry;
    }
    if (np > 1u)                                     /* LEN > 16: the pages, top to bottom */
        for (i = 0; i < np; i++)
            cv_rect(TR_PX, TR_SB - 3 * (int32_t)(np - i) + 1, 3, 2, i == pg ? (sel ? T_INK : T_PTHEME(c)) : cs);
    tr_meter(TR_MX, TR_MY, c, cs, sel ? T_INK : T_MID, bg);
    cv_blit(3, (uint32_t)y);
}
static void draw_home_tracks(void)
{
    uint32_t c;
    if (ui.force) {                                  /* the BG round the rows only, they cover the rest (#181) */
        lcd_fill(0, Y_GRAPH, 3, H_GRAPH, T_BG);
        lcd_fill(3 + TR_W, Y_GRAPH, 240 - 3 - TR_W, H_GRAPH, T_BG);
        for (c = 0; c < NTRK; c++)
            ts.meter[c] = 0;
        lcd_fill(3, Y_GRAPH + NSHOWN * TR_PITCH - 2, TR_W, H_GRAPH - NSHOWN * TR_PITCH + 2, T_BG);   /* (the hidden tracks' rows) */
        for (c = 1; c < NSHOWN; c++) {               /* the 2 rows between two rows; LINE: a rule on the first */
            uint32_t y = (uint32_t)(Y_GRAPH + (int32_t)c * TR_PITCH - 2);
            if (ux.style) {
                lcd_fill(3, y, TR_W, 1, T_RULE);
                GFX_HOOK_RULE(3, y, TR_W, 1);
            }
            lcd_fill(3, ux.style ? y + 1u : y, TR_W, ux.style ? 1u : 2u, T_BG);
        }
    }
    for (c = 0; c < NSHOWN; c++)
        home_track_row(c);
}
/* oscilloscope of the output, triggered on a rising zero crossing: a RAISE centre line, the trace 2 px (in silence
 * a flat line on it; MENU > SCREEN OFF keeps it from staying on the panel for hours) */
static void graph_scope(uint16_t c)
{
    static int16_t snap[SCOPE_N] __attribute__((section(".pool")));
    uint32_t w = scope_w, i, trig = 0;
    int32_t cy = (int32_t)cv_h / 2, py = cy, x, peak = 1500, a = cv_h == H_GRAPH ? 46 : cy - 6;   /* (LARGE: the strip) */
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
 * move the most, the outer ones less and fainter. High Contrast: each line solid A or B (its half). TARGET BOTH: part 1's
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
/* part k's colour at pct (0 A .. 100 B), faded into the panel by fade %; High Contrast: solid A or B */
static uint16_t mv_col(uint32_t k, int32_t pct, int32_t fade)
{
    const ui_pal_t *pl = &UI_PALETTES[ux.pal % NPALETTES];
    pct = clamp(pct, 0, 100);
    if (pl->hc)
        return pl->c[2u * k + (pct >= 50)];
    return ux_mix(ux_mix(pl->c[2u * k], pl->c[2u * k + 1u], pct), T_SURF, clamp(fade, 0, 92));
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
    if (both) {                                      /* part 1 above, part 2 below */
        mv_field(0, MV_BY0, (MV_BY0 + MV_BY1) / 2, 3u);
        mv_field(1, (MV_BY0 + MV_BY1) / 2, MV_BY1, 3u);
    } else {
        mv_field(pi, MV_BY0, MV_BY1, 5u);
    }
    for (i = MV_TX0; i <= MV_TX1; i++) {             /* the A..B track (High Contrast: two solid halves) */
        int32_t p = (i - MV_TX0) * 100 / (MV_TX1 - MV_TX0);
        uint16_t col = pl->hc ? pl->c[2u * pi + (p >= 50)] : ux_mix(ux_mix(pl->c[2u * pi], pl->c[2u * pi + 1u], p), T_SURF, 45);
        cv_rect(i, MV_TY - 1, 1, 2, col);
    }
    if (both && morph_fader_of(1u - pi) != f) {      /* BOTH with the faders apart: the other part's, a tick */
        int32_t ox = MV_TX0 + (int32_t)(morph_fader_of(1u - pi) * (uint32_t)(MV_TX1 - MV_TX0) / 127u);
        cv_rect(ox - 1, MV_TY - 5, 3, 10, T_PFILL(1u - pi));
    }
    GFX_HOOK_ALIGN(0, MV_TY - 7, 0, MV_TY + 8, AL_V, "morph end letter on the track");
    cv_text_on(14, MV_TY - 7 + CAP_IN(S, 15), &AF_S, "A", T_MID, T_SURF);
    GFX_HOOK_ALIGN(0, MV_TY - 7, 0, MV_TY + 8, AL_V, "morph end letter on the track");
    cv_text_r(226, MV_TY - 7 + CAP_IN(S, 15), &AF_S, "B", T_MID, T_SURF);
    fmt_int(b, (int32_t)f);
    cv_rrect(cx - 14, MV_TY - 7, 29, 15, 7, T_PFILL(pi), T_SURF);   /* the thumb: the value in the fader's colour */
    GFX_HOOK_ALIGN(cx - 14, MV_TY - 7, cx + 15, MV_TY + 8, AL_HV, "morph value in its thumb");
    cv_text_in(cx - 14, MV_TY - 7 + CAP_IN(S, 15), 29, &AF_S, b, T_INK, T_PFILL(pi));
    kh[0] = (khint_t){KC_K2, "ROLL A|B"};
    kh[1] = (khint_t){KC_K3, "UNDO A|B"};
    kh[2] = (khint_t){KC_K4, "AMT"};             /* (AMOUNT: 5 px too wide for the row) */
    cv_key_row(12, 228, MV_KY, kh, 3, 7u, T_SURF);
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "morph save line centred");
    cv_text_in(0, MV_SY, 240, &AF_S, "HOLD SAVE = SAVE PATCH", T_DIM, T_SURF);
}
/* HOME's MODE (step 5, SPEC 5b.1): the mode large, then BALANCE, XFADE and SPLIT each in a plain line with its value (XFADE and
 * SPLIT work in LAYER only: DIM in the other modes); the voices: LAYER takes a voice of each part a note, so half as many notes */
static void mode_line(int32_t y, const char *name, const char *val, const char *what, uint32_t on)
{
    cv_text_on(12, y, &AF_S, name, on ? T_TEXT : T_DIM, T_SURF);
    cv_text_r(228, y, &AF_S, val, on ? T_TEXT : T_DIM, T_SURF);
    cv_text_on(12, y + 13, &AF_S, what, on ? T_MID : T_DIM, T_SURF);
}
static void graph_mode(void)
{
    uint32_t m = duell_mode_req < DM_N ? duell_mode_req : DM_SPLIT, lay = m == DM_LAYER;
    char v[8];
    cv_text_on(12, 6, &AF_S, DM_NAME[m], vt(T_THEME, T_SURF), T_SURF);
    cv_text_r(228, 6, &AF_S, lay ? "4 NOTES" : "8 VOICES", T_MID, T_SURF);
    if (mix_bal) {
        v[0] = mix_bal < 0 ? '-' : '+';
        fmt_int(v + 1, mix_bal < 0 ? -mix_bal : mix_bal);
    } else {
        str_cpy(v, "0", sizeof v);
    }
    mode_line(26, "BALANCE", v, "PART 1 <-> PART 2", 1);
    fmt_int(v, mix_xf);
    str_cpy(v + str_len(v), "%", 3);
    mode_line(56, "XFADE (LAYER)", v, "SOFT = PART 1, HARD = 2", lay);
    if (mix_split)
        note_name(v, (uint32_t)mix_split - 1u);
    else
        str_cpy(v, "OFF", sizeof v);
    mode_line(86, "SPLIT (LAYER)", v, "BELOW = PART 1, ABOVE = 2", lay);
}
/* GLO > MIDI: the parts' channels with where each is heard, and MIDI IN (OWN CH / SELECTED) with a line in words under it */
static void graph_midi(void)
{
    uint32_t m = duell_mode_req < DM_N ? duell_mode_req : DM_SPLIT, sel = song.g[G_ROUTE] == 1, same = midi_pch[0] == midi_pch[1], k;
    char v[12], w[40];
    static const char *const WHERE[3][2] = {{"SPLIT: LEFT OUTPUT", "SPLIT: RIGHT OUTPUT"}, {"STEREO OUTPUT", "STEREO OUTPUT"},
                                             {"LAYER: PLAYS BOTH PARTS", "LAYER: NOT HEARD, USE P1 CH"}};
    for (k = 0; k < 2u; k++) {
        str_cpy(v, "CH ", sizeof v);
        fmt_int(v + 3, (int32_t)midi_ch_get(k) + 1);
        str_cpy(w, k ? "PART 2" : "PART 1", sizeof w);
        mode_line(4 + 26 * (int32_t)k, w, v, WHERE[m][k], !sel && !(m == DM_LAYER && k));
    }
    mode_line(56, "MIDI IN", sel ? "SELECTED" : "OWN CH", sel ? "EVERY CHANNEL PLAYS ONE PART" : "EACH PART ON ITS OWN CHANNEL", 1);
    if (sel)
        cv_text_on(12, 86, &AF_S, "THE ONE CHOSEN WITH ALGORITHM", T_ACCENT, T_SURF);
    else if (same)
        cv_text_on(12, 86, &AF_S, "SAME CHANNEL: ONLY PART 1 PLAYS", T_ACCENT, T_SURF);
    else
        cv_text_on(12, 86, &AF_S, "SET THE CHANNELS WITH P1 CH, P2 CH", T_MID, T_SURF);
    cv_text_on(12, 104, &AF_S, "CLOCK", T_TEXT, T_SURF);   /* CLK: the clock source, its line (INT: the tempo is this unit's) */
    cv_text_r(228, 104, &AF_S, song.g[G_CLOCK] == 1 ? "USB (MIDI CLOCK)" : song.g[G_CLOCK] == 2 ? "TRS (MIDI CLOCK)" : "INT (OWN TEMPO)", T_MID, T_SURF);
}
/* GLO > SIDECHAIN: the selected part's DUCK SRC and NOTE (the same two values as its DUCK 1 page) in words */
static void graph_schain(void)
{
    const track_t *t = TSEL;
    char v[12], w[40];
    const char *un;
    str_cpy(w, "SIDECHAIN PART ", sizeof w);
    str_cpy(w + str_len(w), song.sel ? "2" : "1", 2);
    param_format(&TP[P_DKSRC], t->p[P_DKSRC], v, &un);
    mode_line(4, w, v, t->p[P_DKSRC] >= 2 ? "DUCKS WHEN THE NOTE ARRIVES" : t->p[P_DKSRC] == 1 ? "KEYED BY THE OTHER PART" : "PUMP: NO MIDI KEY", 1);
    param_format(&TP[P_DKNOTE], t->p[P_DKNOTE], v, &un);
    mode_line(34, "SC NOTE", v, "THE KEY THAT TRIGGERS THE DUCK", t->p[P_DKSRC] >= 2);
    cv_text_on(12, 66, &AF_S, "CHANGE THE PART WITH ALGORITHM", T_MID, T_SURF);
}
/* MIDI LEARN (step 6, midi_learn.c): its state in words: OFF, ON (turn a knob), a parameter picked (move a CC) */
static void graph_mlearn(void)
{
    char a[32], b[32];
    if (!ui.ml.on) {
        fmt_int(b, (int32_t)ml_count());
        str_cpy(b + str_len(b), ml_count() == 1u ? " CC LEARNED" : " CCS LEARNED", 14);
        panel_note("MIDI LEARN: OFF", "[K1] TURN RIGHT: START", ml_count() ? "[K2] TURN RIGHT: CLEAR ALL" : b);
        (void)b;
    } else if (!ui.ml.pick) {
        panel_note("MIDI LEARN: ON", "1. TURN ANY KNOB TO PICK IT", "2. SEND A CC FROM YOUR CONTROLLER");
    } else {
        ml_name(a);
        str_cpy(a + str_len(a), ": SEND A CC", 14);
        panel_note(a, "[OCT+] DONE", "OCT- CLEARS ITS CC");
    }
}
/* MORPH SET: what TARGET means now (in words), and SMART, ARP and PITCH */
static void graph_morphset(void)
{
    uint32_t pi = song.sel;
    char l[40], c[40];
    const char *t = morph.target == MT_BOTH ? "BOTH PARTS TOGETHER" : pi ? "PART 2 ONLY" : "PART 1 ONLY";
    str_cpy(l, "THE MORPH MOVES ", sizeof l);
    str_cpy(l + str_len(l), t, 24);
    if (text_w(&AF_M, l) <= 232) {                      /* the whole sentence in one line, else two */
        str_cpy(c, "ARP ", sizeof c);
        str_cpy(c + str_len(c), morph.cfg & MC_ARP ? "SHUFFLES TOO  " : "STAYS  ", 16);
        str_cpy(c + str_len(c), "PITCH ", 8);
        str_cpy(c + str_len(c), morph.cfg & MC_PIT ? "MOVES" : "STAYS", 8);
        panel_note(l, morph.cfg & MC_WILD ? "WILD: ANYTHING MAY CHANGE" : "SMART: NOTHING STUPID", c);
    } else {
        str_cpy(c, morph.cfg & MC_WILD ? "WILD   " : "SMART   ", sizeof c);
        str_cpy(c + str_len(c), morph.cfg & MC_ARP ? "ARP ON   " : "ARP OFF   ", 12);
        str_cpy(c + str_len(c), morph.cfg & MC_PIT ? "PITCH ON" : "PITCH OFF", 12);
        panel_note("THE MORPH MOVES", t, c);
    }
}

static void draw_graph(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint16_t c = ACC;
    uint32_t sig;
    if (!ui.home && pg->graph == GR_TRK) {
        draw_tracks();
        return;
    }
    if (ui.home && home_tracks()) {                  /* MENU > HOME TRACKS (1.3) */
        draw_home_tracks();
        return;
    }
    if (!ui.home && pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_SAMPLE) sample_wave_tick(t);
    sig = graph_signature();
    if (!ui.force && sig == ui.graph_sig)
        return;
    ui.graph_sig = sig;
    cv_begin(240, graph_h(), T_BG);
    cv_rrect(3, 0, 234, graph_h(), 5, T_SURF, T_BG);   /* the panel (MENU > LARGE: the strip) */
    cv_bg = T_SURF;                                  /* (text drawn with cv_text lands on it) */
    cv_oy = GOY;                                     /* graphs on a 100 px scale */
    if (strip_kind() != SK_NONE) {                   /* MENU > LARGE: the strip */
        uint32_t k = strip_kind();
        cv_oy = k == SK_ADSR || k == SK_LFO ? 3 : 0;
        graph_ht = LG_H_GRAPH - 6;
        if (k == SK_SCOPE) graph_scope(c);
        else if (k == SK_ADSR) graph_adsr(t, c);
        else if (k == SK_LFO) graph_lfo(t, c);
        else graph_title();
        graph_ht = 100;
    } else if (ui.home) {
        cv_oy = 0;
        graph_scope(c);
    } else {
        switch (pg->graph) {
        case GR_ADSR:
            graph_adsr(t, c);
            break;
        case GR_LFO:
            graph_lfo(t, c);
            break;
        case GR_ROLL:
            cv_oy = 0;
            if (drum_track(t))
                graph_grid(t, c);
            else
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
        case GR_ENGINE:
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
        case GR_TOOLS:
            cv_oy = 0;
            graph_tools();
            break;
        case GR_MORPH:
            cv_oy = 0;
            graph_morph();
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
        case GR_TEST:
            cv_oy = 0;
            graph_test();
            break;
#if FELUCCA_SLICE
        case GR_SLICES:
            cv_oy = 0;
            graph_slices();
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
            else if (pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_SAMPLE && sample_wave.ready) graph_sample(c);
            else if (pg->scope == SC_ENGINE && t->eng_req % NENGINES == ENGI_FM6) graph_fm6(t, c, 6u);   /* EDIT 1 and 2 */
#if FELUCCA_FM4
            else if ((pg->scope == SC_ENGINE || pg->id[0] == P_FM1_LEVEL) && t->eng_req % NENGINES == ENGI_DIGITAL)
                graph_fm(t, c);                      /* (OP LEVEL too: the levels on the chart) */
#endif
            else { cv_oy = 0; graph_scope(c); }
            break;
        }
    }
    cv_oy = 0;
    cv_blit(0, graph_y());
}
