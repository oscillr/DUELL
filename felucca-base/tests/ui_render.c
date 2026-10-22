/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Renders of the firmware UI (firmware/src/ui*.c on tests/ui_test.c's stubs), the layout lint, the GREY and MONO
 * checks and the host draw cost.
 *   ui_render OUTDIR [SLOTDIR]   (run by tests/run_tests.sh; tests/ui_render.py makes the PNGs;
 *                                 SLOTDIR: filmstrips of the rolling digits, GREY MONO and GREEN)
 * Layout lint, over every screen below in every palette, every page of every engine and every value of every
 * column: the ink box of every text and icon drawn (gfx.c GFX_HOOK_TEXT) as it lands on the screen. A finding:
 * a box off the screen, cut by its canvas, partly covered by a later strip or fill, overlapping another box, or
 * ellipsised when it is not free text (labels and values must fit; names and messages may be ellipsised), or
 * spilling out of its cell: touching a rounded rectangle of its canvas (gfx.c cv_rrect: a cell, card, row, button,
 * GFX_HOOK_CELL) without lying inside it, though still on the screen (the FX map's REVERSE), or drawn in the
 * colour it is drawn on (gfx.c GFX_HOOK_INK: a text, icon or keycap label that cannot be seen; MONO's tokens share
 * values, DIM and RAISE its one grey).
 * GREY: every pixel of every screen is RGB565 gray (R = B, G = 2 R). MONO (black and white, blended per channel):
 * every pixel is neutral (R = B, G within 2 of 2 R).
 * MENU > STYLE LINE: every screen again in every palette, the same lint; a text or icon on a divider
 * (ui_draw.c lcd_rule, GFX_HOOK_RULE: the rules between the strips; cv_rule's are cells) is a finding too.
 * Rolling digits: every frame of a header BPM roll and a card roll, up and down, in every palette (lint, GREY, MONO).
 * FM6 charts (ui_graph.c graph_fm6, its parts through FM6_CHART_HOOK), every algorithm and every chart drawn: no two
 * operator boxes overlapping or touching, no route, loop, bus or label inside a box or touching one it does not
 * connect, no two nets (one gap's routes that share an operator, the output, the loop, the label) sharing or
 * touching a pixel, everything inside the panel.
 * Output: OUTDIR/ppm/<PALETTE>_<screen>.ppm for GREY MONO GREEN PAPER NIGHT, OUTDIR/report.txt (findings, ellipsised
 * free text, the draw cost), OUTDIR/text_audit.tsv (GREY, per screen: texts, icons and keycaps with their ink-box px,
 * the ellipsised ones, the texts closer than 2 px to their cell's edge, the words).
 * Alignment (gfx.c GFX_HOOK_ALIGN): every place where a text, icon or keycap is meant to sit centred (in a cell, a
 * chip, a button, a row, a column, a knob, a strip) or on a line shared with its neighbours (an icon on its label's
 * line, a word on its keycap's, a unit on its value's baseline) declares its box; the ink drawn there (a text: its
 * glyphs' boxes across, its capitals' band up and down; an icon: its cell's nibbles; a keycap: its pill; also the
 * keycaps' labels in their pills, from the data, and rows of keys or bars against their panel) is measured against
 * it, on every screen above in FLAT and LINE and every palette, the page / value sweep, and align_sweeps (every value
 * each place can show). 0.5 px is the odd pixel left over (it goes left / up); 1 px or more is a finding.
 * OUTDIR/align.txt: per place, how often measured, the worst offsets and where.
 * Exit 1 on a finding, a GREY pixel off gray, a MONO one off neutral, or an alignment 1 px or more off. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

typedef struct { int16_t x0, y0, x1, y1; uint8_t flags; char s[28]; char fnt; } rbox_t;   /* fnt: S M L (its face) */
static rbox_t pend[256], scr[1200];
static int16_t cells[64][4];                    /* GFX_HOOK_CELL: the cells and cards of the canvas being drawn */
static uint32_t ncells, nspill;
static FILE *aud;                               /* OUTDIR/text_audit.tsv: GREY, the text of each screen */
static char tight[400];                         /* .. the texts of this screen closer than 2 px to their cell's edge */
static uint32_t ntight;
static uint32_t npend, nscr, nfind, nfree;
static uint64_t px_visited, n_text, blit_px;
static FILE *rep;
static const char *cur_name = "";
static char free_seen[64][40];
static uint32_t nfree_seen;
/* the alignment check (gfx.c GFX_HOOK_ALIGN): a place declares the box its next item(s) are meant to be centred in,
 * across and / or up and down; the items' ink as drawn (a text: its glyphs' boxes across, its capitals' band up and
 * down, a text of no letter or figure: across only; an icon: the ink of its cell; a keycap: its pill; a cushion: the
 * cushion) against the box: the offset of the ink's centre from the box's, in half pixels. 0.5 is the odd pixel
 * left over (it goes left / up: a +0.5 is counted apart); 1 px or more is a finding. Per place (its tag): how
 * often it was measured, the worst offsets and where, in OUTDIR/align.txt */
typedef struct { int32_t x0, y0, x1, y1, ix0, iy0, ix1, iy1; uint32_t mode, left, nv; const char *tag; char s[24]; } al_t;
typedef struct { const char *tag; uint32_t n, nfail, half_lo, half_hi; int32_t wh2, wv2; char ex_h[112], ex_v[112]; } alstat_t;
static al_t al_stk[8];
static uint32_t al_n, al_fail, al_lost, al_count, al_off;
static int32_t al_cy0, al_cy1;                  /* GFX_HOOK_PEN: the capitals' band of the text being hooked */
static alstat_t al_st[160];
static uint32_t al_nst;
static FILE *al_rep;
static alstat_t *al_stat(const char *tag)
{
    uint32_t i;
    for (i = 0; i < al_nst && strcmp(al_st[i].tag, tag); i++) ;
    if (i == al_nst && al_nst < 160u) { memset(&al_st[i], 0, sizeof al_st[i]); al_st[i].tag = tag; al_nst++; }
    return &al_st[i < 160u ? i : 159u];
}
/* one measurement: offsets in half pixels (h2, v2; 99: not measured) */
static void al_record(const char *tag, int32_t h2, int32_t v2, const char *s, int32_t bx0, int32_t by0, int32_t bx1,
                      int32_t by1, int32_t ix0, int32_t iy0, int32_t ix1, int32_t iy1)
{
    alstat_t *a = al_stat(tag);
    int32_t ah = h2 == 99 ? 0 : h2 < 0 ? -h2 : h2, av = v2 == 99 ? 0 : v2 < 0 ? -v2 : v2;
    a->n++;
    al_count++;
    if (h2 == 1 || v2 == 1) a->half_hi++;
    if (h2 == -1 || v2 == -1) a->half_lo++;
    if (ah > a->wh2 || (!a->ex_h[0] && h2 != 99)) {
        if (ah > a->wh2) a->wh2 = ah;
        snprintf(a->ex_h, sizeof a->ex_h, "%+.1f '%s' %s ink x %d..%d box %d..%d", h2 / 2.0, s, cur_name, ix0, ix1, bx0, bx1);
    }
    if (av > a->wv2 || (!a->ex_v[0] && v2 != 99)) {
        if (av > a->wv2) a->wv2 = av;
        snprintf(a->ex_v, sizeof a->ex_v, "%+.1f '%s' %s ink y %d..%d box %d..%d", v2 / 2.0, s, cur_name, iy0, iy1, by0, by1);
    }
    if (ah >= 2 || av >= 2) {
        a->nfail++;
        al_fail++;
        if (al_rep && a->nfail <= 3u)
            fprintf(al_rep, "OFF  %-28s %-26s '%s' across %+.1f, up/down %+.1f  (ink %d,%d-%d,%d box %d,%d-%d,%d)\n", tag, cur_name,
                    s, h2 == 99 ? 0.0 : h2 / 2.0, v2 == 99 ? 0.0 : v2 / 2.0, ix0, iy0, ix1, iy1, bx0, by0, bx1, by1);
    }
}
static void hk_align(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t mode, const char *tag)
{
    al_t *a;
    if (al_off) return;                         /* (the draw cost: timed without the check)  */
    if (al_n >= 8u) { al_lost++; return; }
    a = &al_stk[al_n++];
    memset(a, 0, sizeof *a);
    a->x0 = x0; a->y0 = y0; a->x1 = x1; a->y1 = y1; a->mode = mode; a->tag = tag;
    a->left = mode >> 8 ? mode >> 8 : 1u;
    a->ix0 = a->iy0 = 1 << 20; a->ix1 = a->iy1 = -(1 << 20);
}
/* an item drawn (cell: a cv_rrect, else ink): its extent across (x0..x1), up and down (y0..y1; vy 0: none), to the
 * place declared last (AL_PASS, its item done: to the one before it too) */
static void al_item_k(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int vy, const char *s, int cell)
{
    al_t *a;
    int32_t h2 = 99, v2 = 99;
    uint32_t pass;
    if (!al_n) return;
    a = &al_stk[al_n - 1u];
    if (!(a->mode & 32u) != !cell) return;          /* (AL_CELLS: its cells only; else ink only) */
    if (x0 < a->ix0) a->ix0 = x0;
    if (x1 > a->ix1) a->ix1 = x1;
    if (vy) {
        if (y0 < a->iy0) a->iy0 = y0;
        if (y1 > a->iy1) a->iy1 = y1;
        a->nv++;
    }
    if (!a->s[0] || strlen(a->s) + strlen(s) + 2u < sizeof a->s)
        snprintf(a->s + strlen(a->s), sizeof a->s - strlen(a->s), "%s%s", a->s[0] ? "+" : "", s);
    if (--a->left) return;
    al_n--;
    pass = a->mode & 16u;
    if (a->mode & 1u) h2 = (a->ix0 + a->ix1) - (a->x0 + a->x1);       /* AL_H: the centres */
    if (a->mode & 8u) h2 = 2 * (a->ix1 - a->x1);                      /* AL_R: the right edges */
    if (a->nv && (a->mode & 2u)) v2 = (a->iy0 + a->iy1) - (a->y0 + a->y1);
    if (a->nv && (a->mode & 4u)) v2 = 2 * (a->iy1 - a->y1);           /* AL_B: the baselines (capitals' bottoms) */
    al_record(a->tag, h2, v2, a->s, a->x0, a->y0, a->x1, a->y1, a->ix0, a->iy0, a->ix1, a->iy1);
    if (pass) al_item_k(x0, y0, x1, y1, vy, s, cell);
}
static void al_item(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int vy, const char *s) { al_item_k(x0, y0, x1, y1, vy, s, 0); }
static int al_alnum(const char *s)
{
    for (; *s; s++)
        if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9')) return 1;
    return 0;
}
static void al_text(int32_t x0, int32_t y0, int32_t x1, int32_t y1, const char *s, uint32_t flags)
{
    if (!al_n || ((flags & 4u) && !strcmp(s, "icon"))) return;      /* (an icon: GFX_HOOK_ICON) */
    if (flags & 4u) al_item(x0, y0, x1, y1, 1, s);                    /* a keycap: its pill */
    else al_item(x0, al_cy0, x1, al_cy1, al_alnum(s), s);
}
static void al_flush(void)                      /* a canvas blitted or begun: declared places left without their items */
{
    uint32_t i;
    for (i = 0; i < al_n; i++)
        if (al_rep && al_lost + i < 12u) fprintf(al_rep, "LOST %-28s %s (declared, nothing drawn after it)\n", al_stk[i].tag, cur_name);
    al_lost += al_n;
    al_n = 0;
}
static int32_t icon_cell(uint32_t *size, uint32_t id);
static void hk_icon(int32_t x, int32_t y, uint32_t size, uint32_t id);   /* (after the icons: their cells' nibbles) */
#define GFX_HOOK_ALIGN(x0, y0, x1, y1, mode, tag) hk_align(x0, (y0) + cv_oy, x1, (y1) + cv_oy, mode, tag)
static uint8_t pen_h;                           /* GFX_HOOK_PEN: the line height of the text being hooked (its face) */
#define GFX_HOOK_PEN(y, f) (pen_h = (f)->h, al_n ? (al_cy0 = (y) + cv_oy + (f)->g[glyph((f), 'H')].by, al_cy1 = al_cy0 + (f)->g[glyph((f), 'H')].bh) : 0)
#define GFX_HOOK_ITEM(x0, y0, x1, y1) al_item(x0, (y0) + cv_oy, x1, (y1) + cv_oy, 1, "cushion")
#define GFX_HOOK_ICON(x, y, size, id) hk_icon(x, (y) + cv_oy, size, id)
static void hk_text(int32_t x0, int32_t y0, int32_t x1, int32_t y1, const char *s, uint32_t flags)
{
    al_text(x0, y0, x1, y1, s, flags);
    n_text++;
    if (npend >= 256u) return;
    pend[npend] = (rbox_t){(int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1, (uint8_t)flags, {0}};
    strncpy(pend[npend].s, s, 27);
    pend[npend].fnt = (flags & 4u) ? 'K' : pen_h >= 30u ? 'L' : pen_h >= 18u ? 'M' : 'S';
    pen_h = 0;
    npend++;
}
static void hk_cell(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    al_item_k(x0, y0, x1, y1, 1, "cell", 1);
    if (ncells < 64u) {
        cells[ncells][0] = (int16_t)x0; cells[ncells][1] = (int16_t)y0;
        cells[ncells][2] = (int16_t)x1; cells[ncells][3] = (int16_t)y1;
        ncells++;
    }
}
static void hk_blit(uint32_t x, uint32_t y, uint32_t r0, uint32_t w, uint32_t h);
static void hk_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
/* STYLE LINE: the dividers drawn on the screen (ui_draw.c lcd_rule) while no later blit or fill covers them */
static int16_t rules[32][4];
static uint32_t nrules;
static void hk_rule(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    if (nrules < 32u) {
        rules[nrules][0] = (int16_t)x; rules[nrules][1] = (int16_t)y;
        rules[nrules][2] = (int16_t)(x + w); rules[nrules][3] = (int16_t)(y + h);
        nrules++;
    }
}
static void rules_cover(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    uint32_t i, k = 0;
    for (i = 0; i < nrules; i++)
        if (!(rules[i][0] >= x0 && rules[i][2] <= x1 && rules[i][1] >= y0 && rules[i][3] <= y1))
            memcpy(rules[k++], rules[i], sizeof rules[i]);
    nrules = k;
}
#define GFX_HOOK_TEXT(x0, y0, x1, y1, s, flags) hk_text(x0, y0, x1, y1, s, flags)
#define GFX_HOOK_BLIT(x, y, r0) hk_blit(x, y, r0, cv_w, cv_h)
#define GFX_HOOK_BEGIN() (npend = ncells = 0, al_flush())
#define GFX_HOOK_CELL(x0, y0, x1, y1) hk_cell(x0, y0, x1, y1)
#define GFX_HOOK_RULE(x, y, w, h) hk_rule(x, y, w, h)
static uint32_t nink;                           /* GFX_HOOK_INK: inks drawn on their own colour */
static void hk_ink(uint16_t ink, uint16_t bg);
#define GFX_HOOK_INK(ink, bg) hk_ink(ink, bg)
#define GFX_HOOK_PIXELS(n) (px_visited += (n))
#define UI_FILL_HOOK(x, y, w, h) hk_fill(x, y, w, h)
static void fm6_hook(uint32_t kind, uint32_t a, uint32_t b);
#define FM6_CHART_HOOK(kind, a, b) fm6_hook(kind, a, b)
static void cs_hook(const char *v, const char *u);
#define CARD_HOOK(v, u) cs_hook(v, u)
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

/* the card values set in S (ui_draw.c CARD_HOOK: M too wide for the card with its unit), distinct, for the report
 */
static char cs_seen[64][16];
static uint32_t cs_n, cs_more;
static uint64_t cs_count;
static void cs_hook(const char *v, const char *u)
{
    char b[16];
    uint32_t i;
    cs_count++;
    snprintf(b, sizeof b, "%s%s%s", v, u[0] ? " " : "", u);
    for (i = 0; i < cs_n && strcmp(cs_seen[i], b); i++) ;
    if (i == cs_n) { if (cs_n < 64u) snprintf(cs_seen[cs_n++], 16, "%s", b); else cs_more++; }
}
/* an icon drawn: its ink, read from its cell's nibbles here (not icons.c icon_ink's table: measured, not trusted) */
/* n alpha values of a cell from its bit stream (the canonical Huffman code AI_HC, as gen_aa_font.py huff_unpack:
 * decoded here bit by bit, not by gfx.c's table) */
static void icon_unpack(const uint8_t *d, uint8_t *out, uint32_t n)
{
    uint32_t k = 0, pos = 0;
    while (k < n) {
        uint32_t code = 0, first = 0, idx = 0, len, s, r;
        for (len = 0; len < 15u; len++) {
            code |= (d[pos >> 3] >> (7u - (pos & 7u))) & 1u;
            pos++;
            if (code - first < AI_HC[len]) break;
            idx += AI_HC[len];
            first = (first + AI_HC[len]) << 1;
            code <<= 1;
        }
        s = AI_HC[15u + idx + code - first];
        for (r = 0; r <= s >> 4 && k < n; r++) out[k++] = (uint8_t)(s & 15u);
    }
}
static void hk_icon(int32_t x, int32_t y, uint32_t size, uint32_t id)
{
    int32_t k, x0 = 99, y0 = 99, x1 = -1, y1 = -1;
    uint32_t i;
    const uint8_t *c;
    uint8_t cell[24 * 24];
    if (!al_n || (k = icon_cell(&size, id)) < 0) return;
    c = size == 24u ? AI24_DATA + AI24_OFF[k] : size == 16u ? AI16_DATA + AI16_OFF[k] : AI12_DATA + AI12_OFF[k];
    icon_unpack(c, cell, size * size);
    for (i = 0; i < size * size; i++)
        if (cell[i]) {
            int32_t px = (int32_t)(i % size), py = (int32_t)(i / size);
            if (px < x0) x0 = px;
            if (px + 1 > x1) x1 = px + 1;
            if (py < y0) y0 = py;
            if (py + 1 > y1) y1 = py + 1;
        }
    if (cv_flip) { int32_t t = x0; x0 = (int32_t)size - x1; x1 = (int32_t)size - t; }   /* (mirrored: the metronome) */
    al_item(x + x0, y + y0, x + x1, y + y1, 1, "icon");
}

/* the FM6 chart lint: each part of a chart (FMH_*) drawn alone onto a canvas of SENT, its pixels kept in fmp_mask
 * (bit: the part), then the canvas as drawn restored with the part on it; at FMH_END the parts are checked */
#define FMP_MAX 48u
#define FMP_SENT 0x0861u
static uint64_t fmp_mask[CV_MAX];
static uint16_t fmp_save[CV_MAX];
static uint8_t fmp_kind[FMP_MAX], fmp_a[FMP_MAX], fmp_b[FMP_MAX];
static uint32_t fmp_n, fmp_charts;
static void finding(const char *what, const rbox_t *b, const rbox_t *o);
static void fmp_find(uint32_t alg, const char *what, uint32_t i, int32_t x, int32_t y, uint32_t j)
{
    static const char *const KIND[] = {"box", "route", "carrier", "bus", "loop", "label"};
    rbox_t b = {(int16_t)x, (int16_t)y, (int16_t)x, (int16_t)y, 0, {0}}, o = b;
    snprintf(b.s, sizeof b.s, "ALG %u %s %u>%u", alg + 1u, KIND[fmp_kind[i]], fmp_a[i] + 1u, fmp_b[i] + 1u);
    snprintf(o.s, sizeof o.s, "%s %u>%u", j < FMP_MAX ? KIND[fmp_kind[j]] : "panel", j < FMP_MAX ? fmp_a[j] + 1u : 0u,
             j < FMP_MAX ? fmp_b[j] + 1u : 0u);
    finding(what, &b, &o);
}
static void fmp_check(uint32_t alg)
{
    int32_t bx0[FMP_MAX], by0[FMP_MAX], bx1[FMP_MAX], by1[FMP_MAX], x, y, dx, dy;
    uint32_t net[FMP_MAX], par[12], i, j, w = cv_w, h = cv_h;
    for (i = 0; i < 12u; i++) par[i] = i;               /* the routes' nets: an operator as source (0..5), as destination (6..11) */
    #define FMP_ROOT(v) ({ uint32_t r_ = (v); while (par[r_] != r_) r_ = par[r_]; r_; })
    for (i = 0; i < fmp_n; i++)
        if (fmp_kind[i] == FMH_ROUTE) par[FMP_ROOT(fmp_a[i])] = FMP_ROOT(6u + fmp_b[i]);
    for (i = 0; i < fmp_n; i++) {
        net[i] = fmp_kind[i] == FMH_ROUTE ? FMP_ROOT(6u + fmp_b[i]) : fmp_kind[i] == FMH_CAR || fmp_kind[i] == FMH_BUS ? 20u
                 : 21u + fmp_kind[i];
        bx0[i] = by0[i] = 9999; bx1[i] = by1[i] = -1;
    }
    #undef FMP_ROOT
    for (y = 0; y < (int32_t)h; y++)                     /* each part's extent */
        for (x = 0; x < (int32_t)w; x++)
            for (i = 0; i < fmp_n; i++)
                if (fmp_mask[y * w + x] >> i & 1u) {
                    if (x < bx0[i]) bx0[i] = x;
                    if (y < by0[i]) by0[i] = y;
                    if (x > bx1[i]) bx1[i] = x;
                    if (y > by1[i]) by1[i] = y;
                }
    for (i = 0; i < fmp_n; i++)                          /* boxes: apart, by a pixel at least */
        for (j = 0; j < i; j++)
            if (fmp_kind[i] == FMH_BOX && fmp_kind[j] == FMH_BOX && bx0[i] <= bx1[j] + 1 && bx0[j] <= bx1[i] + 1 &&
                by0[i] <= by1[j] + 1 && by0[j] <= by1[i] + 1)
                fmp_find(alg, "FM6 boxes overlap", i, bx0[i], by0[i], j);
    for (y = 0; y < (int32_t)h; y++)
        for (x = 0; x < (int32_t)w; x++) {
            uint64_t mk = fmp_mask[y * w + x];
            if (!mk) continue;
            for (i = 0; i < fmp_n; i++) {
                if (!(mk >> i & 1u) || fmp_kind[i] == FMH_BOX) continue;
                if (x < 6 || x > 233 || y < 2 || y > 119)
                    fmp_find(alg, "FM6 part off the panel", i, x, y, FMP_MAX);
                for (j = 0; j < fmp_n; j++) {
                    if (fmp_kind[j] != FMH_BOX) continue;
                    if (x >= bx0[j] && x <= bx1[j] && y >= by0[j] && y <= by1[j])
                        fmp_find(alg, "FM6 part through a box", i, x, y, j);
                    else if (x >= bx0[j] - 1 && x <= bx1[j] + 1 && y >= by0[j] - 1 && y <= by1[j] + 1 &&
                             !((fmp_kind[i] == FMH_ROUTE && (fmp_a[i] == fmp_a[j] || fmp_b[i] == fmp_a[j])) ||
                               ((fmp_kind[i] == FMH_CAR || fmp_kind[i] == FMH_FB) && fmp_a[i] == fmp_a[j])))
                        fmp_find(alg, "FM6 part touches a box", i, x, y, j);
                }
                for (dy = -1; dy <= 1; dy++)              /* another net on this pixel or next to it */
                    for (dx = -1; dx <= 1; dx++) {
                        int32_t nx = x + dx, ny = y + dy;
                        uint64_t mn;
                        if (nx < 0 || ny < 0 || nx >= (int32_t)w || ny >= (int32_t)h) continue;
                        mn = fmp_mask[ny * w + nx];
                        for (j = 0; j < fmp_n; j++)
                            if (mn >> j & 1u && fmp_kind[j] != FMH_BOX && net[j] != net[i])
                                fmp_find(alg, "FM6 nets cross or touch", i, x, y, j);
                    }
            }
        }
    fmp_charts++;
}
static void fm6_hook(uint32_t kind, uint32_t a, uint32_t b)
{
    uint32_t i, n = cv_w * cv_h;
    if (fmp_n) {                                         /* the part just drawn: its pixels */
        for (i = 0; i < n; i++)
            if (cv_px[i] != FMP_SENT) fmp_mask[i] |= (uint64_t)1u << (fmp_n - 1u);
            else cv_px[i] = fmp_save[i];
    } else {
        memset(fmp_mask, 0, n * sizeof fmp_mask[0]);
    }
    if (kind == FMH_END) {
        fmp_check(a);
        fmp_n = 0;
        return;
    }
    if (fmp_n >= FMP_MAX) { fprintf(stderr, "ui_render: an FM6 chart of more than %u parts\n", FMP_MAX); exit(1); }
    memcpy(fmp_save, cv_px, n * sizeof cv_px[0]);
    for (i = 0; i < n; i++) cv_px[i] = FMP_SENT;
    fmp_kind[fmp_n] = (uint8_t)kind; fmp_a[fmp_n] = (uint8_t)a; fmp_b[fmp_n] = (uint8_t)b;
    fmp_n++;
}

static void finding(const char *what, const rbox_t *b, const rbox_t *o)
{
    static char seen[600][64];
    static uint32_t nseen;
    char key[64];
    uint32_t i;
    nfind++;
    snprintf(key, sizeof key, "%.20s|%.14s|%.24s", cur_name, what, b->s);
    for (i = 0; i < nseen && strcmp(seen[i], key); i++) ;
    if (i < nseen) return;                       /* each finding once per screen */
    if (nseen < 600u) strcpy(seen[nseen++], key);
    else return;
    fprintf(rep, "LINT %-22s %-32s '%s' (%d,%d)-(%d,%d)", cur_name, what, b->s, b->x0, b->y0, b->x1, b->y1);
    if (o) fprintf(rep, " / '%s' (%d,%d)-(%d,%d)", o->s, o->x0, o->y0, o->x1, o->y1);
    fputc('\n', rep);
}
/* a rectangle of the screen is drawn again: boxes inside it are gone, boxes it covers in part are hidden */
static void cover(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    uint32_t i, k = 0;
    for (i = 0; i < nscr; i++) {
        rbox_t *b = &scr[i];
        int in = b->x0 >= x0 && b->x1 <= x1 && b->y0 >= y0 && b->y1 <= y1;
        int touch = b->x0 < x1 && x0 < b->x1 && b->y0 < y1 && y0 < b->y1;
        if (in) continue;
        if (touch && !(b->flags & 48u)) finding("hidden in part by a later draw", b, 0);
        if (touch && (b->flags & 48u)) scr[k++] = *b;   /* split across bands / scrolled: drawn on purpose */
        else scr[k++] = *b;
    }
    nscr = k;
}
static void hk_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    cover((int32_t)x, (int32_t)y, (int32_t)(x + w), (int32_t)(y + h));
    rules_cover((int32_t)x, (int32_t)y, (int32_t)(x + w), (int32_t)(y + h));
}
/* a text across two bands (the menu and the document draw 124 + 85 rows): each band draws its part, the same
 * screen box twice, cut at the band edge; together they are whole (flag 16) */
static int split_pair(rbox_t *b)
{
    uint32_t j;
    if (!(b->flags & 2u)) return 0;
    for (j = 0; j < nscr; j++) {
        rbox_t *o = &scr[j];
        if ((o->flags & 2u) && o->x0 == b->x0 && o->x1 == b->x1 && o->y0 == b->y0 && o->y1 == b->y1 && !strcmp(o->s, b->s)) {
            o->flags = (uint8_t)((o->flags & ~2u) | 16u);
            return 1;
        }
    }
    return 0;
}
/* a text or icon box that touches a cell or card of its canvas (GFX_HOOK_CELL) but spills past its edge; for the
 * audit, a text inside its smallest cell closer than 2 px to an edge */
static void contain(int32_t x, int32_t y)
{
    uint32_t i, c;
    for (i = 0; i < npend; i++) {
        const rbox_t *b = &pend[i];
        int32_t best = 1 << 30, gap = 99;
        for (c = 0; c < ncells && !(b->flags & 4u); c++) {   /* (texts only: an icon's box is its whole cell) */
            const int16_t *r = cells[c];
            int32_t a = (r[2] - r[0]) * (r[3] - r[1]), g;
            if (b->x0 < r[0] || b->x1 > r[2] || b->y0 < r[1] || b->y1 > r[3] || a >= best) continue;
            best = a;
            g = b->x0 - r[0];
            if (r[2] - b->x1 < g) g = r[2] - b->x1;
            if (b->y0 - r[1] < g) g = b->y0 - r[1];
            if (r[3] - b->y1 < g) g = r[3] - b->y1;
            gap = g;
        }
        if (gap < 2 && aud) {
            ntight++;
            if (strlen(tight) + strlen(b->s) + 8u < sizeof tight)
                snprintf(tight + strlen(tight), sizeof tight - strlen(tight), "%s'%s' %dpx", tight[0] ? ", " : "", b->s, (int)gap);
        }
        for (c = 0; c < ncells; c++) {
            const int16_t *r = cells[c];
            int touch = b->x0 < r[2] && r[0] < b->x1 && b->y0 < r[3] && r[1] < b->y1;
            int in = b->x0 >= r[0] && b->x1 <= r[2] && b->y0 >= r[1] && b->y1 <= r[3];
            int around = r[0] >= b->x0 && r[2] <= b->x1 && r[1] >= b->y0 && r[3] <= b->y1;   /* (a mark inside the box) */
            if (touch && !in && !around) {
                rbox_t t = *b, o = {(int16_t)(r[0] + x), (int16_t)(r[1] + y), (int16_t)(r[2] + x), (int16_t)(r[3] + y), 0, "cell"};
                t.x0 += (int16_t)x; t.x1 += (int16_t)x; t.y0 += (int16_t)y; t.y1 += (int16_t)y;
                nspill++;
                finding("spills out of its cell", &t, &o);
                break;
            }
        }
    }
}
static void hk_blit(uint32_t x, uint32_t y, uint32_t r0, uint32_t w, uint32_t h)
{
    uint32_t i, k = 0;
    al_flush();
    contain((int32_t)x, (int32_t)y);
    ncells = 0;
    blit_px += (uint64_t)w * (h > r0 ? h - r0 : 0u);
    for (i = 0; i < npend; i++) {
        rbox_t b = pend[i];
        if (b.y1 <= (int32_t)r0) continue;           /* rows not blitted */
        if (b.y0 < (int32_t)r0) b.flags |= 2u;
        b.x0 += (int16_t)x; b.x1 += (int16_t)x; b.y0 += (int16_t)y; b.y1 += (int16_t)y;
        if (!split_pair(&b)) pend[k++] = b;
    }
    cover((int32_t)x, (int32_t)(y + r0), (int32_t)(x + w), (int32_t)(y + h));
    rules_cover((int32_t)x, (int32_t)(y + r0), (int32_t)(x + w), (int32_t)(y + h));
    for (i = 0; i < k && nscr < 1200u; i++)
        scr[nscr++] = pend[i];
    npend = 0;
}

static void lint(void)
{
    uint32_t i, j;
    for (i = 0; i < nscr; i++) {
        const rbox_t *b = &scr[i];
        if (b->x0 < 0 || b->y0 < 0 || b->x1 > 240 || b->y1 > 240) finding("off the screen", b, 0);
        if ((b->flags & 2u) && !(b->flags & 32u)) finding("cut by its canvas", b, 0);
        if ((b->flags & 1u) && !(b->flags & 8u)) finding("ellipsised (not free text)", b, 0);
        if ((b->flags & 9u) == 9u) {
            for (j = 0; j < nfree_seen && strcmp(free_seen[j], b->s); j++) ;
            if (j == nfree_seen && nfree_seen < 64u) {
                snprintf(free_seen[nfree_seen++], 40, "%s", b->s);
                fprintf(rep, "free   %-22s ellipsised '%s'\n", cur_name, b->s);
            }
            nfree++;
        }
        for (j = i + 1u; j < nscr; j++) {
            const rbox_t *o = &scr[j];
            if (b->x0 < o->x1 && o->x0 < b->x1 && b->y0 < o->y1 && o->y0 < b->y1) finding("overlaps", b, o);
        }
        for (j = 0; j < nrules; j++) {               /* a divider is a cell edge: nothing on it */
            const int16_t *r = rules[j];
            rbox_t o = {r[0], r[1], r[2], r[3], 0, "divider"};
            if (b->x0 < r[2] && r[0] < b->x1 && b->y0 < r[3] && r[1] < b->y1) finding("touches a divider", b, &o);
        }
    }
}

/* the audit of one screen (GREY): its texts (count, ink-box px), icons, keycaps, the ellipsised and tight texts,
 * and the words themselves */
static FILE *audf;
static void audit_scene(const char *name)
{
    uint32_t i, nt = 0, ni = 0, nk = 0, ne = 0, at = 0, ai = 0, ak = 0;
    char words[1200] = "";
    if (!audf) return;
    for (i = 0; i < nscr; i++) {
        const rbox_t *b = &scr[i];
        uint32_t a = (uint32_t)((b->x1 - b->x0) * (b->y1 - b->y0));
        if ((b->flags & 4u) && !strcmp(b->s, "icon")) { ni++; ai += a; continue; }
        if (b->flags & 4u) { nk++; ak += a; continue; }
        nt++; at += a;
        if (b->flags & 1u) ne++;
        if (strlen(words) + strlen(b->s) + 4u < sizeof words)
            snprintf(words + strlen(words), sizeof words - strlen(words), "%s%s", words[0] ? " | " : "", b->s);
    }
    fprintf(audf, "%s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%s\t%s\n", name, nt, at, ni, ai, nk, ak, ne, ntight, words, tight);
}
/* OUTDIR/font_audit.tsv (GREY, FLAT and LARGE): every text of every screen with its face (S M L), its ink box and
 * whether it was ellipsised */
static FILE *fontf;
static void font_scene(const char *mode, const char *name)
{
    uint32_t i;
    if (!fontf) return;
    for (i = 0; i < nscr; i++) {
        const rbox_t *b = &scr[i];
        if (b->flags & 4u) continue;                 /* (icons, keycaps) */
        fprintf(fontf, "%s\t%s\t%c\t%d\t%d\t%d\t%d\t%s%s\n", mode, name, b->fnt ? b->fnt : '?', b->x0, b->y0, b->x1, b->y1,
                b->s, (b->flags & 1u) ? "\t(ellipsised)" : "");
    }
}
/* GREY: every pixel RGB565 gray (R = B, G = 2 R); MONO (black and white, its edges blended per channel): neutral
 * (R = B, G within 2 of 2 R, the 5-6-5 rounding). Other palettes: nothing */
static uint32_t mono_bad;
static void mono_check(void)
{
    uint32_t i, bw = settings.palette == UI_BW_INDEX;
    if (settings.palette != UI_GREY_INDEX && !bw) return;
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);
        int32_t d = (int32_t)((c >> 5) & 63u) - (int32_t)(c >> 11) * 2;
        if ((c >> 11) != (c & 31u) || (bw ? d < -2 || d > 2 : d != 0)) {
            if (!mono_bad) fprintf(rep, "%s %s: pixel %u,%u = %04x off %s\n", bw ? "MONO" : "GREY", cur_name, i % 240u, i / 240u, c,
                                   bw ? "neutral" : "gray");
            mono_bad++;
        }
    }
}
static void hk_ink(uint16_t ink, uint16_t bg)
{
    if (ink != bg) return;
    if (nink < 20u) fprintf(rep, "%s: ink %04x drawn on its own colour (%s)\n", cur_name, ink, UI_PALETTES[settings.palette % NPALETTES].name);
    nink++;
}
static void write_ppm(const char *dir, const char *pal, const char *name)
{
    char path[512];
    uint32_t i;
    FILE *f;
    snprintf(path, sizeof path, "%s/ppm/%s_%s.ppm", dir, pal, name);
    f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return; }
    fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);
        uint8_t b[3] = {(uint8_t)((c >> 11) * 255u / 31u), (uint8_t)(((c >> 5) & 63u) * 255u / 63u), (uint8_t)((c & 31u) * 255u / 31u)};
        fwrite(b, 1, 3, f);
    }
    fclose(f);
}

/* ------------------------------------------------------------------ the screens --- */
static void pal(uint32_t p) { settings.palette = p; palette_set(p); }
static void state(void)                          /* a playing song with steps on every track */
{
    uint32_t i;
    ui_power_on();
    memset(&cpub, 0, sizeof cpub);                   /* (DUELL 0.2.9: the CPU bar under the battery in every scene: 5 of 8 steps, amber) */
    cpub.ok = 1; cpub.t = fm1_ms; cpub.step = 5; cpub.lvl = 1;
    memset(&gm, 0, sizeof gm);                       /* (GLO's level meters from silence) */
    midi_pch[0] = 0; midi_pch[1] = 1; meter_bus[0] = meter_bus[1] = 0;
    mv_reset();                                      /* (MORPH v4: the picture's motion and levels from silence) */
    song.playing = 1;
    song.g[G_BPM] = 124;
    clk_pos = 0; clk_step = 8; cin_total = cin_left = 0;   /* (the bar's 3rd beat) */
    for (i = 0; i < NTRK; i++) trk[i].seq_idx = 5;
    my_steps(&trk[1]);
    trk[0].step[2].flags |= SF_ACCENT;
    trk[0].step[6].flags |= SF_SLIDE;
    for (i = 0; i < SCOPE_N; i++)                 /* a stand-in signal for the scope */
        scope_buf[i] = (int16_t)((int32_t)((i * 37u) % 128u) * 200 - 12800 + (int32_t)((i % 32u) < 16u ? 3000 : -3000));
    scope_w = 0;
}
static void eng(uint32_t e) { set_engine_of(TSEL, e); }
/* a part on a retired engine (SAMPLE 4, GRAIN 8, SLICE 13: DUELL 0.2.9; DIGITAL 1, DRUM 10: 0.2.14), as a stored sound puts it there (a pick never can) */
static void eng_retired(uint32_t e) { set_engine_of(TSEL, 0); TSEL->eng_req = (uint8_t)e; TSEL->preset = 0; }
static void mslot_ui(uint32_t k, int32_t s, int32_t d, int32_t a)   /* matrix slot k of the selected part (0.2.7 scenes) */
{
    TSEL->p[P_M1SRC + 3u * k] = (int16_t)s;
    TSEL->p[P_M1DST + 3u * k] = (int16_t)d;
    TSEL->p[P_M1AMT + 3u * k] = (int16_t)a;
}
/* DUELL: SAVE > PATTERNS, the one list of 32 slots (patslot.c; the scenes that showed Felucca's SONG): P01..P03 saved from part 1
 * (LEN 16, 32, 7), the row picked: v 0 a slot that holds its default (P05 TRANCE ROLL), 1 a saved one (P02), 2 SAVE picked on it, 3
 * OVERWRITE?, 4 ERASE?, 5 playing (LOAD picked), 6 the list's end (P32 SEVEN EIGHT) */
static void pslot_scene(uint32_t v)
{
    static const int16_t L[3] = {16, 32, 7};
    uint32_t k, i;
    song.playing = 0;
    for (k = 0; k < 3u; k++) {
        for (i = 0; i < NSTEP; i++) {
            step_t *st = &trk[0].step[i];
            memset(st, 0, sizeof *st);
            st->time = (i + k) % 3u ? ST_NOTE : ST_REST;
            st->n = st->time == ST_NOTE;
            st->note[0] = (uint8_t)(48u + (i * 5u + k) % 12u);
            st->vel = 100;
        }
        trk[0].p[P_SLEN] = L[k];
        ps_save_trk(&trk[0], k, k == 4u ? "MY ACID LINE" : "SAVED");
    }
    go_page(GR_PATS);
    ui.ppick = (uint8_t)(v == 0u ? 4u : v == 6u ? PS_SLOTS - 1u : 1u);
    ui.act = v == 2u || v == 3u ? 4u : v == 4u ? 3u : v == 5u ? 2u : 0u;
    if (v == 3u) confirm_open(CF_OVR_PSLOT, 1);
    if (v == 4u) confirm_open(CF_ERASE_PSLOT, 1);
    if (v == 5u) song.playing = 1;
    ui.msg_t = 0;
}
/* the FM engine of the FM screens: FM6 (DIGITAL is gone in 0.2.14) */
#define E_FM ENGI_FM6
#define go_home() go_main_to(HP_MORPH)           /* (HOME is MORPH, the main page, since 0.2.14: the OVERVIEW screen is gone) */

enum { S_HOME, S_HOME_IDLE, S_MESSAGE, S_MESSAGE_KEY, S_MESSAGE_NOFILE, S_PRESETS, S_PRESETS_NOFAV, S_USER, S_PHRASES, S_PROJECT,
       S_TOOLS, S_SONG_EMPTY, S_SONG, S_STEP, S_AUTO_STEPS, S_AUTO_ADD_CHANCE, S_MOTION, S_MIXER, S_MIXER_HOME, S_MIXER_PAN, S_ENV,
       S_ENVDEST, S_LFO, S_MOD, S_FX, S_DLY, S_SCL, S_CHORD, S_CHORD_WIDE, S_CHORD_OFF, S_ARP, S_VOICE, S_CLOCK, S_CLOCK_EXT,
       S_MENU_CONTROL, S_EDIT_ANALOG, S_EDIT_DIGITAL, S_EDIT_WHEEL, S_EDIT_RETIRED, S_EDIT_PHYS, S_FM6_ALG1, S_FM6_ALG5, S_FM6_ALG22,
       S_FM6_ALG32, S_FM6_OP, S_FM6_OP_FIXED, S_FM6_OP2, S_FM6_OP_ENV, S_FM6_OP_ENV_REL, S_CONFIRM_SEQ, S_CONFIRM_PROJ,
       S_CONFIRM_USER, S_CONFIRM_PAT, S_CONFIRM_MOTION, S_CONFIRM_ERASE, S_MENU, S_MENU_SPEAKER, S_ABOUT, S_ABOUT_REC,
       S_ABOUT_CREDITS, S_ABOUT_END, S_UBOOT, S_CALIBRATION, S_SPLASH, S_BATT0, S_BATT1, S_BATT2, S_BATT3, S_BATT_USB, S_MOTION_REC,
       S_MOTION_OFF, S_MOTION_CARD, S_SONG_HOME, S_MENU_HOLD, S_MENU_LEDS, S_MENU_END, S_MENU_SYSTEM, S_MENU_SLIDE, S_REVERB,
       S_GLO_PEEK, S_GLO_ACTIVE, S_GLO_EXT, S_SCL_PEEK, S_SCL_ACTIVE, S_EDIT_PEEK, S_EDIT_ACTIVE, S_EDIT_USER, S_LAYER_HINT,
       S_LAYER_LOCK, S_SEQ_TOOLS, S_SEQ_TOOLS_SONG, S_NAME_USER, S_NAME_TYPING, S_NAME_123, S_NAME_EMPTY, S_NAME_FULL, S_NAME_PLAYING,
       S_PROJECT_NAMED, S_SONG_NAMED, S_PRESETS_CAT, S_PRESETS_CAT_EMPTY, S_NAME_CAT, S_NAME_RANDOM, S_MENU_ANIM_IDLE, S_QUICK_LFO,
       S_QUICK_ENV, S_QUICK_ARP, S_SONG_LONG, S_SONG_PLAYING, S_SONG_LONG_PLAYING, S_SONG_UNSAVED, S_SONG_ADD, S_USER_FOOT,
       S_ROLL_EMPTY, S_ROLL_ACID, S_ROLL_CHORDS, S_ROLL_TIES, S_ROLL_LEN32, S_ROLL_HIGH, S_ROLL_LOW, S_ROLL_WIDE, S_ROLL_PLAYING,
       S_ROLL_LOCKS, S_ROLL_LOCK_HELD, S_MOTION_LOCKS, S_AUTO_LIST, S_AUTO_LIST_ADD, S_AUTO_NUDGE, S_AUTO_QNT, S_AUTO_ADD_NUDGE,
       S_ROLL_LEN, S_AUTO_VEL, S_AUTO_ADD_VEL, S_LFO2, S_REC_LAYER, S_REC_LAYER_SET, S_REC_LAYER_SONG, S_COUNTIN, S_COUNTIN2,
       S_MENU_AUDIO, S_HEAD_PLAY_REC, S_HEAD_REC_OTHER, S_HEAD_BPM_TURN, S_HEAD_GLO_TURN, S_HEAD_DOWNBEAT, S_HEAD_BEAT_2,
       S_SONG_LANES, S_SONG_LANES_PLAYING, S_SONG_LANES_SILENT, S_SONG_PICTURES, S_HEAD_SLOT, S_HEAD_SLOT_EDITED, S_HEAD_SLOT_SONG,
       S_MENU_TUNE, S_MENU_INFO, S_MENU_HOME, S_INSERT_CRUSH, S_INSERT_FOLD, S_INSERT_PHASER, S_INSERT_OFF, S_INSERT_MIX, S_ENV_SYNC,
       S_ENV_DEST_SYNC, S_FILTER, S_FILTER_UF, S_FILTER_UF_OFF, S_FILTER_UF2, S_LEARN_WAIT, S_LEARN_PICK, S_LEARN_SET, S_LEARN_LONG, S_GLO_LEARN,
       S_MENU_INFO_ROW, S_MENU_ABOUT_ROW, S_ENGINE, S_ENGINE_FM6, S_ENGINE_FAV, S_ENGINE_USER, S_ENGINE_INIT, S_MORPH, S_MORPH_ROLLED,
       S_MORPH_BOTH, S_MORPH_TWIST, S_MORPH_SET, S_MORPH_SET_BOTH, S_MORPH_SET_WILD, S_MORPH_SAVE, S_MORPH_MID, S_MORPH_B,
       S_MORPH_BOTH_MID, S_MODE, S_MODE_STEREO, S_MODE_LAYER, S_MIXER_SPLIT, S_MIXER_SPLIT_TURNED, S_PATCH_FACTORY, S_PATCH_ENGINE,
       S_PATCH_RESTORE, S_PATCH_SAVE, S_GLO_METERS, S_GLO_CLIP, S_MIDI, S_MIDI_DUCK, S_MIDI_LAYER, S_MIDI_SEL, S_MIDI_LEARN_OFF,
       S_MIDI_LEARN_ON, S_MIDI_LEARN_PICK, S_MIDI_LEARN_CC, S_ABOUT_SCROLL, S_TEST, S_TEST_ALARM, S_SCL_CHROM, S_SCL_CHROM_PLAY,
       S_CHORD_NOSCALE, S_CHORD_NOSCALE7, S_PERFORM, S_PERFORM_HELD, S_PERFORM_SLOW, S_PERFORM_NOCLK, S_PERFORM_LAYER, S_MOD_WAVE,
       S_MOD_WAVE_SYNC, S_MOD_WAVE_E7, S_MOD_BUS, S_MOD_BUS_STEREO, S_MOD_INS_OFF, S_MOD_INS_FUZZ, S_MOD_PAN_DUEL, S_MOD_RETIRED,
       S_GUIDE1, S_GUIDE2, S_GUIDE3, S_GUIDE4, S_GUIDE5, S_GUIDE6, S_GUIDE_MSG, S_GUIDE_KNOB, S_GUIDE_OCT, S_GUIDE_ROW,
       S_QUICK_FILTER, S_QUICK_GLO, S_MULTIS, S_MULTIS2, S_MODULES, S_MODULES_OFF, S_MODS_FX, S_MODS_ARP, S_MODS_ARP_SEQ,
       S_MODS_MIXER, S_MODS_PERFORM, S_MODS_SCHAIN, S_MODS_AUTO, S_MODS_QUICK_ARP, S_MODS_MSG, S_TOOLS_RESTORE, S_PATCH_SAVE_BOTH, S_CONFIRM_RESTORE_ALL, S_PATCH_SAVE_NAMED, S_NAME_SAVE_PATCH, S_FXSUM, S_FXSUM_DUEL,
       S_EQ_CURVE, S_EQ_CURVE2, S_COMP_CURVE, S_COMP_OFF, S_SMASTER, S_SMASTER_WIDE, S_FILT_MULTI, S_FILT_MULTI24, S_FILT_VCF, S_ARP_PATTERN, S_ARP_CHORD, S_SIDECHAIN_PARTS, S_VOICE_PATCH, S_COUNT };
static const char *const S_NAME[S_COUNT] = {"home", "home_idle", "message", "message_key", "message_nofile", "presets",
    "presets_nofav", "user", "phrases", "project", "tools", "pslot_empty", "pslot", "step", "auto_steps", "auto_add_chance",
    "automation", "mixer", "mixer_from_home", "mixer_pan", "env", "env_dest", "lfo", "mod", "fx", "dly", "scl", "chord", "chord_wide",
    "chord_off", "arp", "voice", "clock", "clock_ext", "menu_control", "edit_analog", "edit_fm6", "edit_wheel", "edit_retired",
    "edit_phys", "fm6_alg_01", "fm6_alg_05", "fm6_alg_22", "fm6_alg_32", "fm6_op", "fm6_op_fixed", "fm6_op2", "fm6_op_env",
    "fm6_op_env_rel", "confirm_seq", "confirm_project", "confirm_user", "confirm_pattern", "confirm_motion", "confirm_erase", "menu",
    "menu_speaker", "about", "about_rec", "about_credits", "about_end", "uboot", "calibration", "splash", "batt_0", "batt_1",
    "batt_2", "batt_3", "batt_usb", "motion_rec", "motion_off", "motion_card", "pslot_home", "menu_hold", "menu_leds", "menu_end",
    "menu_system", "menu_slide", "reverb_spring", "glo_page", "glo_page_active", "glo_page_ext", "layer_scl_peek", "layer_scl_active",
    "layer_edit_peek", "layer_edit_active", "layer_edit_user", "layer_hint", "layer_lock", "layer_seq_tools", "layer_seq_tools_pslot",
    "name_user", "name_typing", "name_123", "name_empty", "name_full", "name_playing", "project_named", "pslot_named", "presets_bass",
    "presets_other_empty", "name_category", "name_random", "menu_anim_idle", "layer_quick_lfo", "layer_quick_env", "layer_quick_arp",
    "pslot_long", "pslot_playing", "pslot_long_playing", "pslot_unsaved", "pslot_add", "user_foot", "roll_empty", "roll_acid",
    "roll_chords", "roll_ties", "roll_len32_p2", "roll_high", "roll_low", "roll_wide", "roll_playing", "roll_locks", "roll_lock_held",
    "motion_locks", "auto_list", "auto_list_add", "auto_nudge", "auto_quantize", "auto_add_nudge", "roll_len", "auto_vel",
    "auto_add_vel", "lfo2", "layer_rec", "layer_rec_set", "layer_rec_pslot", "countin", "countin_2bars", "menu_audio",
    "head_play_rec", "head_rec_other", "head_long_oct_rec", "head_glo", "head_slot_long", "head_matrix_oct", "pslot_lanes",
    "pslot_lanes_playing", "pslot_lanes_silent", "pslot_pictures", "head_slot", "head_slot_edited", "head_slot_pslot", "menu_tune",
    "menu_info", "menu_home", "insert_crush", "insert_fold", "insert_phaser", "insert_off", "insert_mix", "env_sync", "env_dest_sync",
    "filter", "filter_uf", "filter_uf_off", "filter_uf2", "learn_wait", "learn_pick", "learn_set", "learn_long", "glo_learn_on", "menu_info_row",
    "menu_about_row", "engine", "engine_fm6", "engine_fav", "engine_user", "engine_init", "morph_from_home", "morph_rolled",
    "morph_both", "morph_twist", "morph_set", "morph_set_both", "morph_set_wild", "morph_save", "morph_mid", "morph_b",
    "morph_both_mid", "mode_from_home", "mode_stereo", "mode_layer", "mixer_split", "mixer_split_turned", "patches_factory",
    "patches_engine", "patches_restore", "patches_save", "glo_meters", "glo_clip", "midi", "midi_duck", "midi_layer", "midi_sel",
    "midi_learn_off", "midi_learn_on", "midi_learn_pick", "midi_learn_cc", "about_scroll", "test", "test_alarm", "scl_chromatic",
    "scl_chromatic_play", "chord_noscale", "chord_noscale7", "perform", "perform_held", "perform_slow", "perform_noclock",
    "perform_layer", "mod_wave", "mod_wave_sync", "mod_wave_e7", "mod_bus", "mod_bus_stereo", "mod_ins_off", "mod_ins_fuzz",
    "mod_pan_duel", "mod_retired", "guide_1", "guide_2", "guide_3", "guide_4", "guide_5", "guide_6", "guide_message", "guide_knob",
    "guide_octave", "menu_guide_row", "layer_quick_filter", "quick_glo", "multis", "multis_2", "modules", "modules_off", "mods_fx",
    "mods_arp", "mods_arp_seq", "mods_mixer", "mods_perform", "mods_schain", "mods_auto", "mods_quick_arp", "mods_msg", "tools_restore", "patches_save_both", "confirm_restore_all", "patches_save_named", "name_save_patch", "fxsum", "fxsum_duel",
    "eq_curve", "eq_curve2", "comp_curve", "comp_off", "stereo_master", "stereo_master_wide", "filt_multi", "filt_multi24", "filt_vcf", "arp_pattern", "arp_chord", "sidechain_parts", "voice_patch"};

/* the STEP page's piano roll (ui_graph.c graph_roll) on track 1, stopped unless said: an empty pattern; ACID in
 * A minor; POLY chords (Am7 F C G, tied); a line with ties, slides and accents in C major; LEN 32 on its second
 * page, playing; high (A6..) and low (C-1..) notes; a page wider than the view (edge marks); ACID playing with a
 * key held (A3, its row lit) */
static void step_put(step_t *st, uint32_t time, uint32_t flags, uint32_t n, const uint8_t *notes)
{
    uint32_t j;
    memset(st, 0, sizeof *st);
    st->time = (uint8_t)time; st->flags = (uint8_t)flags; st->n = (uint8_t)n; st->vel = 100;
    for (j = 0; j < n; j++) st->note[j] = notes[j];
}
static void roll_scene(int s)
{
    static const uint8_t AM7[] = {57, 60, 64, 67}, FM7[] = {53, 57, 60, 64}, CMA[] = {60, 64, 67}, GMA[] = {55, 59, 62, 67};
    static const uint8_t LINE[16] = {48, 0, 55, 60, 0, 59, 0, 0, 62, 64, 0, 67, 0, 65, 64, 0};
    static const uint8_t LINE_T[16] = {0, 1, 0, 0, 1, 0, 2, 2, 0, 0, 1, 0, 1, 0, 0, 2};   /* 0 note, 1 tie, 2 rest */
    track_t *t;
    uint32_t i;
    song.sel = 0; t = TSEL;
    song.playing = 0;
    track_defaults_steps(t);
    t->p[P_SLEN] = 16;
    go_page(GR_ROLL);
    ui.cursor = 0;
    switch (s) {
    case S_ROLL_EMPTY: break;
    case S_ROLL_ACID:
    case S_ROLL_PLAYING:
        load_pat16(t, PATTERNS[0].note, PATTERNS[0].flags);
        root_g = 9; scale_g = 2; ui.cursor = 6;
        if (s == S_ROLL_PLAYING) {
            song.playing = 1; t->seq_idx = 9;
            kb_trk[3] = 0; kb_chn[3] = 1; kb_chord[3][0] = 57;            /* A3 held */
        }
        break;
    case S_ROLL_CHORDS:
        t->p[P_VOICE] = V_POLY; root_g = 9; scale_g = 2;
        for (i = 0; i < 16u; i++) {
            const uint8_t *c = i / 4u == 0u ? AM7 : i / 4u == 1u ? FM7 : i / 4u == 2u ? CMA : GMA;
            uint32_t n = i / 4u == 2u ? 3u : 4u;
            if (i % 4u == 0u) step_put(&t->step[i], ST_NOTE, i == 8u ? SF_ACCENT : 0u, n, c);
            else if (i % 4u == 1u) step_put(&t->step[i], ST_TIE, 0, 0, c);
            else if (i % 4u == 2u) step_put(&t->step[i], ST_NOTE, 0, n, c);
            else step_put(&t->step[i], ST_REST, 0, 0, c);
        }
        ui.cursor = 4;
        break;
    case S_ROLL_TIES:
        root_g = 0; scale_g = 1;
        for (i = 0; i < 16u; i++) {
            uint8_t n = LINE[i];
            uint32_t f = (i == 3u || i == 9u ? SF_SLIDE : 0u) | (i == 0u || i == 8u || i == 13u ? SF_ACCENT : 0u);
            if (LINE_T[i] == 0u) step_put(&t->step[i], ST_NOTE, f, 1, &n);
            else step_put(&t->step[i], LINE_T[i] == 1u ? ST_TIE : ST_REST, 0, 0, &n);
        }
        t->step[2].flags |= SF_SLIDE;                  /* (slides into a note, from before a tie) */
        ui.cursor = 9;
        break;
    case S_ROLL_LEN32:
        load_pat16(t, PATTERNS[0].note, PATTERNS[0].flags);
        t->p[P_SLEN] = 32;
        for (i = 16; i < 32u; i++) {
            uint8_t n = (uint8_t)(60 + (i * 5u) % 12u);
            if (i % 3u != 2u) step_put(&t->step[i], ST_NOTE, i % 8u == 0u ? SF_ACCENT : 0u, 1, &n);
        }
        ui.cursor = 20; song.playing = 1; t->seq_idx = 22;
        break;
    case S_ROLL_LEN:                                 /* 1.5 (#178): step 5's note made 4 steps long (KNOB 3 LEN, hot) */
        root_g = 0; scale_g = 1;
        for (i = 0; i < 16u; i += 4u) {
            uint8_t n = LINE[i] ? LINE[i] : 60;
            step_put(&t->step[i], ST_NOTE, i == 8u ? SF_ACCENT : 0u, 1, &n);
        }
        (void)note_set_len(t, 0, 2);
        (void)note_set_len(t, 4, 4);
        ui.cursor = 4; ui.hot_col = 2; ui.hot_t = 30;
        break;
    case S_ROLL_HIGH:
    case S_ROLL_LOW:
        for (i = 0; i < 16u; i += 2u) {
            uint8_t n = (uint8_t)(s == S_ROLL_HIGH ? 112 + (i * 7u) % 15u : (i * 7u) % 15u);
            step_put(&t->step[i], ST_NOTE, 0, 1, &n);
        }
        ui.cursor = 2;
        break;
    case S_ROLL_WIDE:
        t->p[P_VOICE] = V_POLY;
        for (i = 0; i < 16u; i += 2u) {
            uint8_t n[2] = {(uint8_t)(36 + i), (uint8_t)(72 + i)};
            step_put(&t->step[i], ST_NOTE, 0, 2, n);
        }
        ui.cursor = 4;
        break;
    default: break;
    }
    ui.bank = (uint8_t)(ui.cursor / 16u);
}

/* the INSERT of the selected track (1.5): TYPE, A B C, MIX */
static void set_ins_ui(int32_t ty, int32_t a, int32_t b, int32_t c, int32_t mix)
{
    TSEL->p[P_ITYPE] = (int16_t)ty;
    TSEL->p[P_IA] = (int16_t)a;
    TSEL->p[P_IB] = (int16_t)b;
    TSEL->p[P_IC] = (int16_t)c;
    TSEL->p[P_IMIX] = (int16_t)mix;
}

static void setup(int s)
{
    memset(kb_chn, 0, sizeof kb_chn);               /* no key held (roll_playing holds one) */
    state();
    scale_g = 0; root_g = 0; offscale_g = OS_SNAP;   /* the global scale: CHROMATIC, C, SNAP (a scene sets its own) */
    switch (s) {
    case S_HOME: song.octave = 2; song.rec = 1; usb.config = 1; break;
    case S_HOME_IDLE: song.playing = 0; song.batt_raw = 570; ui.hot_col = 1; ui.hot_t = 30; break;
    case S_MESSAGE: ui_say("LOADED ", "07 A VERY LONG PATTERN NAME"); break;
    case S_MESSAGE_KEY: ui_message("[SAVE] HOLD TO UNDO"); break;            /* a message with a keycap */
    case S_MESSAGE_NOFILE:                          /* a part on a retired engine (ui_input.c gone_notice): NO ENGINE */
        eng_retired(4); memset(snd_said, 0, sizeof snd_said); gone_notice(); break;
    case S_PRESETS: favorite_set(NENGINES, 4, 1); favorite_set(NENGINES, 5, 1); go_page(GR_BROWSE); break;
    case S_PRESETS_NOFAV: favorites.filter = 1; go_page(GR_BROWSE); break;
    case S_USER: song.playing = 0; up_store(3, "MY LONG BASS NAME"); up_store(4, "PAD"); up_load(3); go_page(GR_BROWSE); break;   /* (PATCHES: two slots saved over, one loaded) */
    case S_PATCH_FACTORY: song.playing = 0; up_store(3, "MY LONG BASS NAME"); sl_load(7); go_page(GR_BROWSE); break;   /* a slot with its factory sound: RESTORE not drawn */
    case S_PATCH_ENGINE: eng(0); apply_preset(1); list_set(1u + CAT_N + 1u); go_page(GR_BROWSE); break;            /* TYPE: one engine (FM6) */
    case S_PATCH_RESTORE: song.playing = 0; up_store(3, "MY LONG BASS NAME"); up_load(3); go_page(GR_BROWSE); confirm_open(CF_RESTORE, 3); break;
    case S_PATCH_SAVE: song.playing = 0; up_store(5, "A VERY LONG SOUND NAME"); sl_load(2); go_page(GR_BROWSE); ui.svs = 5; confirm_open(CF_SAVE_OVER, 0); break;   /* SAVE held: OVERWRITE #6 over a saved slot, the K1 hint */
    case S_PATCH_SAVE_NAMED: song.playing = 0; up_store(5, "A VERY LONG SOUND NAME"); sl_load(2); go_page(GR_BROWSE); ui.svs = 5; str_cpy(save_nm, "A VERY LONG NEW NAME", sizeof save_nm); confirm_open(CF_SAVE_OVER, 0); break;   /* 0.2.21: after RENAME */
    case S_NAME_SAVE_PATCH: song.playing = 0; up_store(5, "A VERY LONG SOUND NAME"); sl_load(2); go_page(GR_BROWSE); ui.svs = 5; name_open(NK_SAVE_NAME, 0); break;   /* 0.2.21: the dialog's RENAME */
    case S_PATCH_SAVE_BOTH: song.playing = 0; sl_load(2); go_page(GR_MORPH); morph.target = MT_BOTH; confirm_open(CF_SAVE_OVER, 2); break;   /* MORPH TARGET BOTH: each part over its own slot */
    case S_TOOLS_RESTORE: song.playing = 0; up_store(3, "MY LONG BASS NAME"); go_page(GR_TOOLS); ui.act = 3; break;   /* RESTORE ALL picked (a saved slot exists) */
    case S_CONFIRM_RESTORE_ALL: song.playing = 0; up_store(6, "A VERY LONG SOUND NAME"); ui.confirm = CF_RESTORE_ALL; ui.confirm_trk = 0; break;
    case S_PHRASES: go_page(GR_PATS); break;
    case S_PROJECT: song.playing = 0; project_save(1); song.g[G_SLOT] = 2; go_page(GR_SLOTS); ui.act = 4; break;
    case S_TOOLS: go_page(GR_TOOLS); ui.act = 1; break;
    case S_SONG_EMPTY: pslot_scene(0); break;
    case S_SONG: pslot_scene(1); break;
    case S_STEP: song.rec = 1; go_page(GR_ROLL); ui.cursor = 6; break;
    /* 1.2 SEQ > AUTOMATION: a step's CHANCE and RATCH as rows among the locks and events (step 5's CHANCE selected, its
     * value just turned); + ADD CHANCE on step 4; the first row (PLAY ON, the counts, OCT+ CLEAR) */
    case S_AUTO_STEPS:
        step_set_chance(&TSEL->step[0], 65); step_set_chance(&TSEL->step[4], 25); step_set_ratchet(&TSEL->step[4], 3);
        step_set_ratchet(&TSEL->step[8], 2); motion_set_event(TSEL, 2, P_LRATE, 70); motion_set_lock(TSEL, 4, P_REV, 100);
        go_page(GR_EVENTS); (void)auto_pick(EVC(EVK_CHANCE, 4)); ui.hot_col = 3; ui.hot_t = 30;
        break;
    case S_AUTO_ADD_CHANCE: go_auto_add(EV_CHANCE, 3); break;
    case S_MOTION:
        motion_set_event(TSEL, 1, P_REV, 40); motion_set_lock(TSEL, 2, P_REV, 100); step_set_chance(&TSEL->step[3], 50);
        go_auto_top();
        break;
    /* 1.1.5, the REC layer (REC held): over HOME, the settings as they come (CLICK OFF, COUNT-IN OFF, LEVEL MID);
     * over STEP armed, CLICK REC, COUNT-IN 2 BARS, LEVEL HIGH (KNOB 2 just turned); a song playing on an empty
     * track (CLEAR dimmed: STOP TO EDIT) */
    case S_REC_LAYER: go_home(); ui.layer = LAYER_REC; break;
    case S_REC_LAYER_SET: song.rec = 1; go_page(GR_ROLL); ui.cursor = 6; rp_put(RP_CLICK, 1u); rp_put(RP_COUNTIN, 2u);
        rp_put(RP_LEVEL, 2u); ui.layer = LAYER_REC; ui.hot_col = 1; ui.hot_t = 30; break;
    case S_REC_LAYER_SONG: pslot_scene(1); ui.layer = LAYER_REC; break;
    /* 1.1 (Discussion #131): the count-in over the page: 1 BAR on its second beat (3 to go); 2 BARS, the first bar's
     * first beat (STEP under it); the MENU's AUDIO tab with CLICK REC selected, COUNT-IN 1 BAR */
    case S_COUNTIN: song.playing = 0; song.rec = 1; cin_total = 4; cin_left = 3; go_home(); break;
    case S_COUNTIN2: song.playing = 0; song.rec = 1; cin_total = 8; cin_left = 8; go_page(GR_ROLL); break;
    case S_MENU_AUDIO: ui.menu = 1; ui.menu_sel = MI_CLICK; rp_put(RP_CLICK, 1u); rp_put(RP_COUNTIN, 1u); break;
    /* 0.2.1: the header (ui_draw.c draw_head): the page name in the top middle; the selected track armed; another track armed; the longest
     * name with REC, the octave at -8 and a slot number "16*" (everything gives way in turn); GLO's page; a two-unit page (MATRIX) with the octave */
    case S_HEAD_PLAY_REC: song.rec = 1u << song.sel; go_home(); break;
    case S_HEAD_REC_OTHER: song.rec = 1u << ((song.sel + 1u) % NTRK); go_home(); break;
    case S_HEAD_BPM_TURN: song.rec = 1u << song.sel; song.octave = -8; go_title("AUTOMATION"); break;
    case S_HEAD_GLO_TURN: go_title("TEMPO"); break;   /* (0.2.21: GLO has no page of its own; GLO's first page is TEMPO) */
    case S_HEAD_DOWNBEAT: song.rec = 1u << song.sel; song.octave = -8; pat_from[song.sel] = 16; pat_from_h[song.sel] ^= 1u; go_title("MIDI LEARN"); break;
    case S_HEAD_BEAT_2: song.octave = 2; go_title("MATRIX 5-8"); break;
    case S_SONG_LANES: pslot_scene(2); break;
    case S_SONG_LANES_PLAYING: pslot_scene(5); break;
    case S_SONG_LANES_SILENT: pslot_scene(3); break;
    case S_SONG_PICTURES: pslot_scene(4); break;
    /* 1.2: the header's slot letter (ui_draw.c head_slot): "1 A" (PROJECT A loaded; another track armed: the REC mark
     * outlined after it); "1 A*" edited since, armed (the REC mark after it) */
    case S_HEAD_SLOT: song.playing = 0; project_save(0); project_load(0); song.rec = 1u << ((song.sel + 1u) % NTRK); go_home(); ui.msg_t = 0; break;
    case S_HEAD_SLOT_EDITED:
        song.playing = 0; project_save(0); project_load(0); TSEL->step[3].note[0] = 70; TSEL->step[3].n = 1;
        song.rec = 1u << song.sel; go_home(); ui.msg_t = 0; break;
    case S_HEAD_SLOT_SONG: pslot_scene(6); break;
    case S_MIXER:                               /* (STEREO: PAN live; SPLIT's fixed PAN: S_MIXER_SPLIT) */
        ui_mode_set = DM_STEREO; duell_mode_req = DM_STEREO;
        go_page(GR_GLO);
        trk[0].p[P_LEVEL] = 100; trk[1].p[P_LEVEL] = 84;
        trk[0].p[P_PAN] = -20; trk[1].p[P_PAN] = 24; trk[1].p[P_REV] = 80;
        trk[1].p[P_MUTE] = 1; song.rec = 2u; trk[0].peak = 9000;
        break;
    case S_MIXER_HOME: go_home(); open_family(FAM_GLO); break;   /* 0.2.25 layout round: the MIXER is GLO's first page (a tap of GLO from HOME); HOME's round is MORPH, MORPH SET, MODE */
    case S_MIXER_PAN:                           /* PAN just turned, the ends of each knob, OFF, armed while stopped */
        ui_mode_set = DM_STEREO; duell_mode_req = DM_STEREO;
        go_page(GR_GLO); song.playing = 0; song.sel = 1;
        trk[0].p[P_LEVEL] = 127; trk[1].p[P_LEVEL] = 0;
        trk[0].p[P_PAN] = -64; trk[1].p[P_PAN] = 63;
        trk[0].p[P_REV] = 127; trk[1].p[P_MUTE] = 1;
        song.rec = 1u; ui.hot_col = 3; ui.hot_t = 30;
        break;
    case S_ENV: go_title("AMP ENV"); ui.hot_col = 2; ui.hot_t = 30; break;
    case S_ENVDEST: go_title("AMP DEST"); break;
    case S_ENV_SYNC:                                  /* 1.5: ENV SYNC on, the note values (the widest: 1/16D 1/32T) */
        TSEL->p[P_ESYNC] = 1; TSEL->p[P_ATK] = 52; TSEL->p[P_DEC] = 16; TSEL->p[P_REL] = 127;
        go_title("AMP ENV"); ui.hot_col = 0; ui.hot_t = 30; break;
    case S_ENV_DEST_SYNC: TSEL->p[P_ESYNC] = 1; go_title("AMP DEST"); break;
    case S_FILTER: set_engine_of(TSEL, 0); TSEL->p[P_FREEA] = 64; TSEL->p[P_FREEB] = 30; TSEL->p[P_KTRK] = 13; go_title("MULTI FILTER 1/2"); break;   /* cloud job J: ANALOG's VCF is filter 1 now (the VCF page is gone) */
    case S_FILTER_UF: set_engine_of(TSEL, 2); TSEL->p[P_FREEA] = 52; TSEL->p[P_FREEB] = 70; TSEL->p[P_FREEC] = 32; go_title("MULTI FILTER 1/2"); break;   /* 0.2.9: PHASE's multimode filter, 1.4 kHz, LP>BP */
    case S_FILTER_UF2: set_engine_of(TSEL, 2); TSEL->p[P_FREEA] = 52; TSEL->p[P_KTRK] = 32; TSEL->p[P_E2FLT] = 20; TSEL->p[P_LD_FLT] = -12; TSEL->p[P_ED_FLT] = 17; go_title("MULTI FILTER 2/2"); break;   /* 0.2.21: KTRK, ENV 2, LFO 1 (PHASE: all three live) */
    case S_FILTER_UF_OFF: set_engine_of(TSEL, 2); TSEL->p[P_FREEB] = 70; go_title("MULTI FILTER 1/2"); break;   /* CUT OFF: RES and the FLT depths are live (0.2.5: a zero is not faded) */
    case S_LFO: go_title("LFO 1"); break;
    case S_MOD:                                   /* 0.2.10 deck: WAVE "PWM PAD", four slots, four sources, four destinations (none MIX), slot 1 selected */
        eng(ENGI_WAVE); apply_preset_to(TSEL, 0);
        mslot_ui(0, MS_LFO, MD_CUT, 40); mslot_ui(1, MS_ENV2, MD_SHP, 27); mslot_ui(2, MS_VEL, MD_RES, -41); mslot_ui(3, MS_MODW, MD_DLY, 17);   /* (raw 64 = 100 %) */
        mod_ui_slot = 0; go_title("MATRIX 1-4"); break;
    /* 0.2.7: the MATRIX page's DEST in words: WAVE's TABLE (faded: REBUILDS), its SYNC, an unused slot (E7, faded); a bus value in DUEL
     * (bus A) and in STEREO (shared); the INSERT's A while OFF (faded) and as FUZZ's DRIVE; PAN in DUEL (faded); a retired engine's slots (UNUSED) */
    case S_MOD_WAVE: case S_MOD_WAVE_SYNC: case S_MOD_WAVE_E7:
        eng(ENGI_WAVE); mslot_ui(0, MS_LFO, MD_E1, 30); mslot_ui(1, MS_ENV2, MD_E1 + 3, 40); mslot_ui(2, MS_MODW, MD_E1 + 6, 20);
        mslot_ui(3, MS_AT, MD_SHP, -24); mod_ui_slot = (uint8_t)(s - S_MOD_WAVE); go_title("MATRIX 1-4"); break;
    case S_MOD_BUS: case S_MOD_BUS_STEREO:
        if (s == S_MOD_BUS_STEREO) duell_mode_req = DM_STEREO;
        mslot_ui(0, MS_LFO2, MD_RSIZE, 44); mslot_ui(1, MS_MODW, MD_DMDEP, 63); mslot_ui(2, MS_ENV, MD_CDEP, -30); mod_ui_slot = 0;
        go_title("MATRIX 1-4"); break;
    case S_MOD_INS_OFF: case S_MOD_INS_FUZZ:
        if (s == S_MOD_INS_FUZZ) TSEL->p[P_ITYPE] = 4;
        mslot_ui(0, MS_VEL, MD_INSA, 40); mslot_ui(1, MS_LFO, MD_IMIX, -20); mslot_ui(2, MS_ENV2, MD_EQMID, 12); mod_ui_slot = 0;
        go_title("MATRIX 1-4"); break;
    case S_MOD_PAN_DUEL: mslot_ui(0, MS_LFO, MD_PAN, 63); mslot_ui(1, MS_LFO, MD_LEVEL, 20); mod_ui_slot = 0; go_title("MATRIX 1-4"); break;
    /* DUELL 0.2.11: the first-start GUIDE's pages (ui_guide.c) over HOME's page, a message in its header (REC: HOLD ON), the octave beside its
     * title with BOTH lit (over the MORPH page), the MENU's GUIDE row */
    case S_GUIDE1: case S_GUIDE2: case S_GUIDE3: case S_GUIDE4: case S_GUIDE5: case S_GUIDE6:
        go_home(); guide_open(0); ui.guide = (uint8_t)(1 + s - S_GUIDE1); break;
    case S_GUIDE_MSG: go_home(); guide_open(0); ui.guide = 4; ui_message("HOLD ON"); break;
    case S_GUIDE_KNOB: go_home(); guide_open(0); ui.guide = 2; ui_message(GUIDE_KNOB_MSG); break;   /* (a knob turned: why nothing happens) */
    case S_GUIDE_OCT: go_title("MORPH"); morph.target = MT_BOTH; song.octave = -3; guide_open(1); ui.guide = GUIDE_N; break;
    case S_GUIDE_ROW: ui.menu = 1; ui.menu_sel = MI_GUIDE; break;
    case S_MOD_RETIRED: eng_retired(13); mslot_ui(4, MS_RAND, MD_E1, 30); mslot_ui(5, MS_LFO, MD_E1 + 7, 30); mod_ui_slot = 0;
        go_title("MATRIX 5-8"); break;
    case S_FX: go_title("FX"); break;
    case S_DLY: go_title("DELAY 1/2"); break;
    case S_FXSUM: go_title("SEND FX"); duell_mode_req = DM_STEREO; song.g[G_DMODE] = DMD_RAND; song.g[G_DTIME] = 1; song.g[G_RTYPE] = 5; song.g[G_RSIZE] = 90; TSEL->p[P_CHOR] = 41; TSEL->p[P_DLY] = 71; TSEL->p[P_REV] = 101; break;   /* 0.2.22: FX's last page, the selected part's sends: RAND 1/8, PLATE 71 % */
    /* 0.2.25 layout round: the pictures (EQ curve, compressor, STEREO MASTER, the filters, the arp pattern), the two-line labels */
    case S_EQ_CURVE: TSEL->p[P_EQLG] = 6; TSEL->p[P_EQLF] = -6; TSEL->p[P_EQMG] = -5; TSEL->p[P_EQMF] = 4; TSEL->p[P_EQHG] = 3; go_title("EQ 1/2"); ui.hot_col = 1; ui.hot_t = 30; break;
    case S_EQ_CURVE2: TSEL->p[P_EQLG] = -12; TSEL->p[P_EQMG] = 12; TSEL->p[P_EQMQ] = 12; TSEL->p[P_EQHG] = 12; TSEL->p[P_EQHF] = 12; TSEL->p[P_EQLF] = -24; go_title("EQ 2/2"); break;
    case S_COMP_CURVE: ui_mode_set = DM_STEREO; duell_mode_req = DM_STEREO; duell_mode = DM_STEREO; song.cp[1] = 4; song.cp[0] = 20; go_title("COMPRESSOR"); break;
    case S_COMP_OFF: ui_mode_set = DM_STEREO; duell_mode_req = DM_STEREO; duell_mode = DM_STEREO; song.cp[1] = 0; go_title("COMPRESSOR"); break;
    case S_SMASTER: ui_mode_set = DM_STEREO; duell_mode_req = DM_STEREO; duell_mode = DM_STEREO; song.bm = 8; song.ms[0] = 100; go_title("STEREO MASTER"); break;
    case S_SMASTER_WIDE: ui_mode_set = DM_STEREO; duell_mode_req = DM_STEREO; duell_mode = DM_STEREO; song.bm = 0; song.ms[0] = 200; go_title("STEREO MASTER"); ui.hot_col = 0; ui.hot_t = 30; break;
    case S_FILT_MULTI: eng(E_FM); TSEL->p[P_FREEA] = 50; TSEL->p[P_FREEB] = 100; TSEL->p[P_FREEC] = 20; TSEL->p[P_FREED] = 1; go_title("MULTI FILTER 1/2"); break;
    case S_FILT_MULTI24: eng(E_FM); TSEL->p[P_FREEA] = 40; TSEL->p[P_FREEB] = 60; TSEL->p[P_FREEC] = 127; TSEL->p[P_FREED] = 2; go_title("MULTI FILTER 2/2"); break;
    case S_FILT_VCF: eng(0); TSEL->p[P_FREEA] = 45; TSEL->p[P_FREEB] = 110; go_title("MULTI FILTER 1/2"); break;   /* (was ANALOG's own VCF page; cloud job J: filter 1 on ANALOG) */
    case S_ARP_PATTERN: TSEL->p[P_AMODE] = 3; TSEL->p[P_AOCT] = 2; TSEL->p[P_AGATE] = 100; TSEL->p[P_ASWING] = 50; go_title("ARP 2/3"); break;
    case S_ARP_CHORD: TSEL->p[P_AMODE] = 14; TSEL->p[P_AOCT] = 3; TSEL->p[P_APROB] = 90; go_title("ARP 1/3"); break;
    case S_SIDECHAIN_PARTS: go_title("SIDECHAIN"); ui.hot_col = 2; ui.hot_t = 30; break;
    case S_VOICE_PATCH: TSEL->p[P_VOICE] = 3; go_title("VOICE 2/2"); ui.hot_col = 3; ui.hot_t = 30; break;
    case S_FXSUM_DUEL: go_title("SEND FX"); TSEL->p[P_CHOR] = 127; TSEL->p[P_DLY] = 127; TSEL->p[P_REV] = 127; duell_mode_req = DM_SPLIT; song.g[G_DMODE] = DMD_RAND; song.g[G_DTIME] = 2; song.g[G_RTYPE] = 1; song.g[G_RSIZE] = 127; break;   /* DUEL: no delay modes (the time alone), the widest reverb card */
    case S_SCL: scale_g = 2; go_title("SCALE"); break;                 /* GLOBAL: SCALE ROOT OFF-SCALE */
    case S_SCL_CHROM: go_title("SCALE"); break;                      /* CHROMATIC: OFF-SCALE not drawn */
    case S_SCL_CHROM_PLAY: offscale_g = OS_PLAY; go_title("SCALE"); ui.hot_col = 2; ui.hot_t = 30; break;
    case S_CHORD_NOSCALE: TSEL->p[P_CHRD] = CH_DIA3; go_title("CHORD"); break;      /* DIA3 with no scale: the card stays (plays MAJ) */
    case S_CHORD_NOSCALE7: TSEL->p[P_CHRD] = CH_DIA7; TSEL->p[P_TRANS] = -5; go_title("CHORD"); break;
    case S_CHORD:                                    /* A minor DIA7, the last chord on B: Bm7b5 */
    case S_CHORD_WIDE: {                             /* A harmonic minor DIA7 +OCT on G#: G#dim7 over three octaves */
        uint8_t out[CHORD_MAX];
        TSEL->p[P_VOICE] = V_POLY; root_g = 9; scale_g = s == S_CHORD ? 2 : 7;
        TSEL->p[P_CHRD] = CH_DIA7; TSEL->p[P_VOIC] = s == S_CHORD ? VC_CLOSE : VC_BASS;
        chord_build(TSEL, s == S_CHORD ? 71u : 68u, out);
        go_title("CHORD"); ui.hot_col = 0; ui.hot_t = 30;
        break;
    }
    case S_CHORD_OFF: go_title("CHORD"); break;
    case S_ARP: go_title("ARP 1/3"); break;
    case S_VOICE: go_title("VOICE 1/2"); break;
    case S_CLOCK: go_title("TEMPO"); break;
    case S_CLOCK_EXT: go_title("TEMPO"); song.g[G_CLOCK] = 1; break;   /* DUELL: external clock, TEMPO shows EXT */          /* (1.2: HOME > MIXER > CLOCK; up to 1.1.5 GLO > GLOBAL) */
    /* 1.2: the old SYSTEM page's items in the MENU: MIDI > MIDI IN (ROUT) on CH5-8; AUDIO > TUNE (438.8 Hz) selected;
     * SYSTEM > INFO (the USB state, the CPU load) */
    case S_MENU_CONTROL: ui.menu = 1; ui.menu_sel = MI_HOMEPG; ui_home_pg = HP_MODE; break;   /* (DUELL 0.1: HOME OPENS, CONTROL's last row) */
    case S_MENU_TUNE: ui.menu = 1; ui.menu_sel = MI_TUNE; song.g[G_TUNE] = 4388; break;
    /* 1.3: HOME > TRACKS playing (T2 selected), stopped (T1 selected, its cursor's page), K2 just turned; the MENU's DISPLAY
     * tab with HOME TRACKS selected (8 rows) */
    case S_MENU_HOME: ui.menu = 1; ui.menu_sel = MI_CPU; break;   /* (0.2.14: OVERVIEW is gone: DISPLAY's last row, CPU METER) */
    /* 1.5 (#134): HOME LEVELS: the rows of TRACKS, the cards T1..T4 LEVEL (T3 muted: DIM), K2 just turned */
    case S_INSERT_CRUSH: set_ins_ui(IT_CRUSH, 64, 96, 80, 127); go_title("INSERT 1/2"); break;
    case S_INSERT_FOLD: set_ins_ui(IT_FOLD, 100, 96, 100, 76); go_title("INSERT 1/2"); break;
    case S_INSERT_PHASER: set_ins_ui(IT_PHASER, 70, 110, 90, 127); go_title("INSERT 1/2"); break;
    case S_INSERT_OFF: go_title("INSERT 1/2"); break;
    case S_INSERT_MIX: set_ins_ui(IT_FLANGER, 40, 127, 110, 90); go_title("INSERT 2/2"); break;
    /* 1.5 MIDI LEARN (midi_learn.c): on, nothing picked (MIXER); T1 CUT picked, no CC yet (EDIT 2); T1 CUT on CC74 (its
     * message in the header); the widest line, a 5-letter label waiting; GLO's map with LEARN on (D4) */
    case S_LEARN_WAIT: go_page(GR_GLO); ml_toggle(); ui.msg_t = 0; break;
    case S_LEARN_PICK: go_title("EDIT 2"); ml_toggle(); ml_knob(TSEL, P_E4); ui.msg_t = 0; ui.hot_col = 0; ui.hot_t = 30; break;
    case S_LEARN_SET: go_title("EDIT 2"); ml_toggle(); ml_learn_f(74, 0, P_E4, 1, 0); ml_knob(TSEL, P_E4); break;   /* (74 is the standard map's: a taken number needs force now) */
    case S_LEARN_LONG: {                             /* "T4 WAVE2: SEND A CC" (PHASE's WAVE2: the widest label) */
        uint32_t k;
        song.sel = 1; eng(2); go_title("EDIT 2"); ml_toggle();
        for (k = 0; k < 7u && !str_eq(ENGINES[2]->edit[k].label, "WAVE2"); k++)
            ;
        ml_knob(TSEL, P_E0 + k); ui.msg_t = 0;
        break;
    }
    case S_GLO_LEARN: go_title("EDIT 2"); ml_toggle(); ui.msg_t = 0; break;   /* (LEARN on, shown on another page: the footer line) */
    /* DUELL 0.1: the menu with INFO / ABOUT picked (KNOB 1 opens them: the hint row says so) */
    case S_MENU_INFO_ROW: ui.menu = 1; ui.menu_sel = MI_TEST; break;
    case S_MENU_ABOUT_ROW: ui.menu = 1; ui.menu_sel = MI_ABOUT; break;
    case S_MENU_INFO:
        ui.menu = 3; ui.menu_sel = MI_TEST; song.cpu_q8 = 87u;   /* (USB: the host has none, OFF) */
        cpum.t = fm1_ms; cpum.avg = 84; cpum.hold = 97; cpum.miss = 3; cpum.seen = 1; cpum.miss_t = fm1_ms;   /* near the limit, a miss just now */
        break;
    case S_EDIT_ANALOG: go_title("EDIT 1"); break;
    case S_EDIT_DIGITAL: eng(E_FM); go_title("EDIT 1"); break;
    case S_EDIT_WHEEL: eng(7); go_title("EDIT 1"); ui.hot_col = 1; ui.hot_t = 30; break;
    case S_EDIT_RETIRED: eng_retired(8); go_title("EDIT 1"); ui.msg_t = 0; break;   /* (a stored GRAIN sound: "-", blank cards) */
    case S_EDIT_PHYS: eng(9); go_title("EDIT 1"); break;
    case S_FM6_ALG1: eng(ENGI_FM6); TSEL->p[P_E0] = 1; go_title("EDIT 1"); break;
    case S_FM6_ALG5: eng(ENGI_FM6); TSEL->p[P_E0] = 5; TSEL->p[P_E1] = 3; go_title("EDIT 1"); ui.hot_col = 1; ui.hot_t = 30; break;
    case S_FM6_ALG22: eng(ENGI_FM6); TSEL->p[P_E0] = 22; TSEL->p[P_E6] = 40; go_title("EDIT 2"); ui.hot_col = 2; ui.hot_t = 30; break;
    case S_FM6_ALG32:
        eng(ENGI_FM6); TSEL->p[P_E0] = 32; fm6_patch[song.sel][FP_OL] = 0; fm6_pgen[song.sel]++;   /* (op 6: the patch's first) */
        go_title("EDIT 1"); ui.hot_col = 2; ui.hot_t = 30;
        break;
    /* FM6's operator pages (ui_fm6op.c): OPERATOR on TINE EP's OP 3, LEVEL just turned; OP 5 FIXED (277 HZ), COARSE just
     * turned; OPERATOR 2 on OP 2 with DTUN -3 just turned; OP ENV: OP 1's DEC stage, its RATE just turned; OP 6's REL */
    case S_FM6_OP: eng(ENGI_FM6); fop_op = 2; go_title("OP TUNE"); ui.hot_col = 3; ui.hot_t = 30; break;
    case S_FM6_OP_FIXED:
        eng(ENGI_FM6); fop_op = 4;
        fm6_patch[song.sel][FP_OP + FP_MODE] = 1; fm6_patch[song.sel][FP_OP + FP_FC] = 2; fm6_patch[song.sel][FP_OP + FP_FF] = 44;
        fm6_pgen[song.sel]++;
        go_title("OP TUNE"); ui.hot_col = 1; ui.hot_t = 30;
        break;
    case S_FM6_OP2:
        eng(ENGI_FM6); fop_op = 1; fm6_patch[song.sel][4u * FP_OP + FP_DET] = 4; fm6_pgen[song.sel]++;
        go_title("OP MODE"); ui.hot_col = 2; ui.hot_t = 30;
        break;
    case S_FM6_OP_ENV: eng(ENGI_FM6); fop_op = 0; fop_stg = 1; go_title("OP ENV"); ui.hot_col = 2; ui.hot_t = 30; break;
    case S_FM6_OP_ENV_REL: eng(ENGI_FM6); fop_op = 5; fop_stg = 3; go_title("OP ENV"); break;
    case S_CONFIRM_SEQ: ui.confirm = CF_CLEAR_SEQ; ui.confirm_trk = 2; break;
    case S_CONFIRM_PROJ: ui.confirm = CF_OVR_PROJ; ui.confirm_trk = 0; break;
    case S_CONFIRM_USER: song.playing = 0; up_store(6, "A VERY LONG SOUND NAME"); ui.confirm = CF_RESTORE; ui.confirm_trk = 6; break;
    case S_CONFIRM_PAT: ui.confirm = CF_LOAD_PAT; ui.confirm_trk = 0; ui.ppick = 4; break;
    case S_CONFIRM_MOTION: ui.confirm = CF_CLEAR_MOTION; ui.confirm_trk = 3; break;
    case S_CONFIRM_ERASE: song.playing = 0; up_store(6, "A VERY LONG SOUND NAME"); ui.confirm = CF_SAVE_OVER; ui.svs = 6; ui.confirm_trk = 1; break;   /* (SAVE held on part 2) */
    case S_MENU: ui.menu = 1; ui.menu_sel = 0; song.rec = 1; break;
    case S_MENU_SPEAKER: ui.menu = 1; ui.menu_sel = MI_CLICK; break;
    case S_ABOUT: ui.menu = 2; ui.menu_scroll = 0; about_t0 = fm1_ms - 330u; break;   /* the splash drawing (its first view), step 5 */
    case S_ABOUT_SCROLL: ui.menu = 2; ui.menu_scroll = 18; cpum.t = fm1_ms; cpum.avg = 37; cpum.hold = 61; cpum.miss = 0; cpum.seen = 0; break;
    case S_ABOUT_REC: ui.menu = 2; ui.menu_scroll = 18; song.rec = 1;
        cpum.t = fm1_ms; cpum.avg = 88; cpum.hold = 104; cpum.miss = 12; cpum.seen = 1; cpum.miss_t = fm1_ms; break;   /* (and the meter in alarm) */          /* the REC mark beside OCT- BACK */
    case S_ABOUT_CREDITS: ui.menu = 2; ui.menu_scroll = 360; break;
    case S_ABOUT_END: ui.menu = 2; ui.menu_scroll = (uint16_t)menu_scroll_max(); break;
    case S_TEST: project_load(15); felucca_dbg.erase_max_us = 41234u; ui.menu = 3; ui.menu_sel = MI_TEST; test_t = fm1_ms; cpum.avg = 58; cpum.hold = 73; cpum.miss = cpum.base = 0; cpum.seen = 0; cpum.t = fm1_ms; break;
    case S_TEST_ALARM: project_load(14); ui.menu = 3; ui.menu_sel = MI_TEST; song.sel = 1; test_t = fm1_ms; cpum.avg = 97; cpum.hold = 124; cpum.miss = 27; cpum.base = 0; cpum.seen = 1;
        cpum.miss_t = fm1_ms; cpum.t = fm1_ms; break;
    case S_UBOOT: ui.uboot = 3; break;
    /* the header's battery at each stock level (raw ADC: under 531, 531.., 561.., 591..) and on USB power */
    case S_BATT0: usb.config = 0; song.batt_raw = 500; go_home(); break;
    case S_BATT1: usb.config = 0; song.batt_raw = 540; go_home(); break;
    case S_BATT2: usb.config = 0; song.batt_raw = 570; go_home(); break;
    case S_BATT3: usb.config = 0; song.batt_raw = 600; go_home(); break;
    case S_BATT_USB: usb.config = 1; usb.suspended = 0; song.batt_raw = 500; go_home(); break;
    case S_MOTION_REC: song.rec = 1u << song.sel; go_auto_top(); break;          /* recording into the motion */
    case S_MOTION_OFF:                                                            /* stopped, PLAY OFF (OCT+ CLEAR) */
        song.playing = 0; motion_set_event(TSEL, 1, P_REV, 40); motion_set_enabled(TSEL, 0); go_auto_top(); break;
    case S_MOTION_CARD: {                                          /* #63: HOME, MOTION drives KNOB 1 and 3 (3 just turned) */
        const engine_t *en = ENGINES[TSEL->eng_req];
        motion_set_event(TSEL, 2, en->knob[0], TSEL->p[en->knob[0]]);
        motion_set_event(TSEL, 6, en->knob[2], TSEL->p[en->knob[2]]);
        go_home(); ui.hot_col = 2; ui.hot_t = 30;
        break;
    }
    case S_SONG_HOME: song.playing = 1; go_home(); ui.msg_t = 0; break;   /* (playing: the PLAY mark, no song) */
    case S_REVERB: go_title("REVERB"); song.g[G_RTYPE] = 1; ui.hot_col = 0; ui.hot_t = 30; break;   /* TYPE: SPRING */
    case S_MENU_HOLD: ui.menu = 1; ui.menu_sel = MI_HOLD; settings_hold = 2; break;
    case S_MENU_LEDS: ui.menu = 1; ui.menu_sel = MI_LEDS; settings_leds = LEDS_INV; break;
    case S_MENU_ANIM_IDLE: ui.menu = 1; ui.menu_sel = MI_ANIM; menu_put(MI_ANIM, 2); break;   /* (1.2: ANIM IDLE, #135) */
    /* DUELL 0.2.9: the QUICK EDIT layers (LFO, ENV, ARP held): QUICK LFO with LFO 2 synced (its K3 SYNC2), KNOB 2 just turned */
    case S_QUICK_LFO: go_title("AMP ENV"); TSEL->p[P_L2SYNC] = 5; ui.layer = LAYER_LFO; ui.hot_col = 1; ui.hot_t = 40; break;
    case S_QUICK_FILTER: go_title("AMP ENV"); ui.layer = LAYER_EDIT; ui.hot_col = 0; ui.hot_t = 30; break;   /* EDIT held: QUICK FILTER (0.2.9) */
    case S_QUICK_GLO: go_title("AMP ENV"); ui.layer = LAYER_GLO; trk[0].peak = 11000; trk[1].peak = 4000; break;   /* 0.2.13 deck: GLO held, QUICK GLO (0.2.11) */
    case S_MULTIS: case S_MULTIS2:                /* 0.2.13 deck: SAVE > MULTIS, the factory multis in their 0.2.13 order (M01 REESE SWAMP loaded) */
        song.playing = 0; project_load(0); song.g[G_SLOT] = s == S_MULTIS ? 1 : 7; go_page(GR_SLOTS); ui.act = 2; ui.msg_t = 0; break;
    case S_QUICK_ENV: go_title("LFO 1"); ui.layer = LAYER_ENV; break;
    /* DUELL 0.2.14 GLO > MODULES: the page (all ON; all OFF, its K2 just turned) and the cards a module OFF fades, with the footer's line */
    case S_MODULES: go_title("MODULES"); break;
    case S_MODULES_OFF: mods_put(MODS_ALL, 1u); go_title("MODULES"); ui.hot_col = 1; ui.hot_t = 30; break;
    case S_MODS_FX: mods_put(MODS_SENDS | MODS_PFX, 1u); TSEL->p[P_DIST] = 30; TSEL->p[P_REV] = 70; go_title("FX"); break;
    case S_MODS_ARP: mods_put(MODS_ARP, 1u); TSEL->p[P_AMODE] = 1; TSEL->p[P_AHOLD] = 1; go_title("ARP 1/3"); break;
    case S_MODS_ARP_SEQ: mods_put(MODS_SEQ, 1u); TSEL->p[P_AMODE] = AM_SEQ; go_title("ARP 1/3"); break;
    case S_MODS_MIXER: mods_put(MODS_SENDS, 1u); perf_space = 40; go_page(GR_GLO); break;
    case S_MODS_PERFORM: mods_put(MODS_SENDS, 1u); go_title("AMP ENV"); perf_enter(); perf_space = -30; break;
    case S_MODS_SCHAIN: mods_put(MODS_PFX, 1u); go_title("SIDECHAIN"); TSEL->p[P_DKSRC] = 2; break;
    case S_MODS_AUTO: mods_put(MODS_SEQ, 1u); go_page(GR_EVENTS); break;
    case S_MODS_QUICK_ARP: mods_put(MODS_ARP, 1u); go_home(); TSEL->p[P_AMODE] = 1; ui.layer = LAYER_ARP; break;
    case S_MODS_MSG: mods_put(MODS_PFX, 1u); go_title("EQ 1/2"); ui_message(MODS_FULL[3]); break;
    case S_QUICK_ARP: go_home(); TSEL->p[P_AMODE] = 1; ui.layer = LAYER_ARP; break;
    case S_MENU_END: ui.menu = 1; ui.menu_sel = MI_COUNT - 1u; ui_prefs = 0xFF; ui_rec_prefs = 0x2A; break;   /* the last row,
                                                     * every flag set (CLICK ON, LEVEL HIGH, COUNT-IN 2 BARS) */
    /* the MENU's tabs (1.0.5): menu DISPLAY (COLOR), menu_hold CONTROL, menu_speaker AUDIO, menu_system SYSTEM (USB
     * SERIAL); menu_slide: from DISPLAY to CONTROL with ANIM ON, the frame half way (draw) */
    case S_MENU_SYSTEM: ui.menu = 1;
#if FELUCCA_CDC
        ui.menu_sel = MI_SERIAL; break;
#else
        ui.menu_sel = MI_PANEL; break;
#endif
    case S_MENU_SLIDE: ui.menu = 1; ui.menu_sel = MI_ACCEL; ui_prefs |= PREF_ACCEL; break;
    /* the GLO SCL EDIT layers (ui_layer.c): just opened (a peek), and in use: GLO with part 2 muted and KNOB 1 turned; with CLK EXT (TAP dimmed); SCL at D# minor, KNOB 2 turned; EDIT on DIGITAL preset 3,
     * a favourite; a user preset; the hint after a tap */
    case S_GLO_PEEK: go_title("TEMPO"); ui.layer = LAYER_GLO; pat_from[0] = 4; break;   /* (a multi loaded: its number stays out of this page's header) */
    case S_GLO_ACTIVE:
        go_title("TEMPO"); ui.layer = LAYER_GLO; trk[1].p[P_MUTE] = 1; trk[0].p[P_LEVEL] = 90;
        ui.hot_col = 2; ui.hot_t = 30;
        break;
    case S_GLO_EXT: go_title("TEMPO"); ui.layer = LAYER_GLO; song.g[G_CLOCK] = 1; trk[0].p[P_MUTE] = 1; break;
    /* DUELL step 6: the level meters (a part near its top, another past -1 dBFS: the CLIP lamp and the red zone; a peak of each frame, so the
     * first frame shows the level and the hold tick), the MIDI pages (the defaults; the sidechain on channel 10, note C2 and part 2 selected;
     * LAYER: part 2's channel not heard; MIDI IN = SEL), MIDI LEARN off, on, a parameter picked */
    case S_GLO_METERS: go_title("TEMPO"); ui.layer = LAYER_GLO; trk[0].peak = 11000; trk[1].peak = 4000; break;
    case S_GLO_CLIP: go_title("TEMPO"); ui.layer = LAYER_GLO; trk[0].peak = 21000; trk[1].peak = 33000; break;
    case S_MIDI: go_title("MIDI"); break;
    case S_MIDI_DUCK: go_title("SIDECHAIN"); song.sel = 1; midi_pch[0] = 0; midi_pch[1] = 4; ui_midi_p2 = 5; TSEL->p[P_DKSRC] = 2; sc_put(0, 15); sc_put(1, 36); sc_put(2, 4); sc_put(3, 0); break;
    case S_MIDI_LAYER: go_title("MIDI"); duell_mode_req = DM_LAYER; break;
    case S_MIDI_SEL: go_title("MIDI"); song.g[G_ROUTE] = 1; break;
    case S_MIDI_LEARN_OFF: go_title("MIDI LEARN"); break;
    case S_MIDI_LEARN_ON: go_title("MIDI LEARN"); ml_toggle(); ui.msg_t = 0; break;
    case S_MIDI_LEARN_PICK: go_title("MIDI LEARN"); ml_toggle(); ml_knob(TSEL, P_E5); ui.msg_t = 0; break;
    case S_MIDI_LEARN_CC: {                            /* DUELL 0.1: LEARN off, three CCs learned: KNOB 2 CLEAR says how many, the panel names it */
        uint32_t k;
        go_title("MIDI LEARN");
        for (k = 0; k < 3u; k++) ml_learn(40u + k, k & 3u, P_ATK + k);
        break;
    }
    case S_SCL_PEEK: go_title("AMP ENV"); ui.layer = LAYER_SCL; break;
    case S_SCL_ACTIVE: go_home(); ui.layer = LAYER_SCL; root_g = 3; scale_g = 2; ui.hot_col = 1; ui.hot_t = 30; break;
    /* DUELL 3.6: EDIT's first page, ENGINE (KNOB 1 ENGINE, 2 SOUND, 3 FAV, 4 INIT): ANALOG; FM6; a favourite; a user sound;
     * the INIT dialog (every scene is also drawn LARGE) */
    case S_ENGINE: go_title("ENGINE"); break;
    case S_ENGINE_FM6: eng(ENGI_FM6); go_title("ENGINE"); break;
    case S_ENGINE_FAV: eng(ENGI_FM6); apply_preset_to(TSEL, 2); favorite_set(ENGI_FM6, 2, 1); go_title("ENGINE"); break;
    case S_ENGINE_USER: song.playing = 0; eng(6); up_store(6, "MY LONG TRIO NAME"); up_load(6); favorite_set(NENGINES, 6, 1);
        go_title("ENGINE"); break;
    case S_ENGINE_INIT: go_title("ENGINE"); confirm_open(CF_INIT_SOUND, song.sel); break;
    /* DUELL 4b: HOME's MORPH (HOME tapped twice: nothing rolled, fader A, AMOUNT's default); both ends rolled (seeds,
     * UNDO counts), the fader between them, KNOB 1 hot; TARGET BOTH on part 2, the fader at B (far end A); the twists of
     * KNOB 3 / 4 building up; MORPH SET (SELECTED, BOTH) */
    case S_MORPH: go_home(); home_tap(); break;   /* (DUELL 3.5: HOME tapped from OVERVIEW: the main page) */
    /* DUELL 0.2.5 PERFORM (FX held): the map at rest over AMP ENV; REPEAT 1/16, FLANGER and MUTE 2 held, the knobs turned (SPACE -40,
     * FILTER 60 HP, DEPTH 75), BOTH parts, BALANCE +20; at 40 BPM (REPEAT 1/8, 1/16 and REVERSE greyed: TEMPO); CLK USB with no clock
     * (PUMP greyed), FILTER all the way left; in LAYER with part 2, the fader at B and BALANCE -100 */
    case S_PERFORM: go_title("AMP ENV"); perf_enter(); break;
    case S_PERFORM_HELD:
        go_title("AMP ENV"); perf_enter(); morph_set_target(1); mix_bal = 20;
        kb_perf = 1u << key_at(0, PF_R16) | 1u << key_at(0, PF_FLG) | 1u << key_at(0, PF_M2);
        perf_held = perf_act = PF_BIT(PF_R16) | PF_BIT(PF_FLG) | PF_BIT(PF_M2);
        perf_space = -40; perf_filt = 60; perf_depth = 75; ui.hot_col = 2; ui.hot_t = 30;
        break;
    case S_PERFORM_SLOW: song.g[G_BPM] = 40; go_title("FX"); perf_enter(); break;
    case S_PERFORM_NOCLK: song.g[G_CLOCK] = 1; song.playing = 0; go_home(); perf_enter(); perf_filt = -100; break;
    case S_PERFORM_LAYER: duell_mode_req = DM_LAYER; song.sel = 1; mix_bal = -100; go_title("MORPH"); perf_enter(); morph_do(MQ_FADER, 2u, 127u); break;
    case S_MORPH_ROLLED:
        morph_do(MQ_AMOUNT, 1u, 100u); morph_roll_seed(0, 0, 65535u); morph_roll_seed(0, 1, 12345u); morph_roll_seed(0, 1, 4242u);
        morph_do(MQ_FADER, 1u, 40u); go_title("MORPH"); ui.hot_col = 0; ui.hot_t = 30; break;
    case S_MORPH_BOTH:
        song.sel = 1; morph_set_target(1); morph_do(MQ_AMOUNT, 3u, 7u); morph_roll_seed(0, 0, 101u); morph_roll_seed(1, 0, 101u);
        morph_do(MQ_FADER, 3u, 127u); go_title("MORPH"); break;
    case S_MORPH_TWIST:
        go_title("MORPH"); ui.mg.acc[0] = -2; ui.mg.acc[1] = 2; ui.mg.t[0] = ui.mg.t[1] = fm1_ms; ui.hot_col = 3; ui.hot_t = 30;
        morph_do(MQ_FADER, 1u, 64u); break;
    case S_MORPH_SET: go_title("MORPH SET"); break;
    case S_MORPH_SET_BOTH: morph_set_target(1); go_title("MORPH SET"); ui.hot_col = 0; ui.hot_t = 30; break;
    case S_MORPH_SET_WILD: morph_set_cfg(MC_WILD | MC_ARP | MC_PIT, 1); morph_set_target(1); go_title("MORPH SET"); ui.hot_col = 1; ui.hot_t = 30; break;
    case S_MORPH_SAVE: go_title("MORPH"); confirm_open(CF_SAVE_OVER, 0u); break;   /* SAVE held: the SAVE OVER dialog */
    /* MORPH v3 (the ribbons): the fader at 64 and 127 with part 1 playing (its peak), BOTH at 64 (both parts playing) */
    case S_MORPH_MID: morph_do(MQ_FADER, 1u, 64u); go_title("MORPH"); trk[0].peak = 5000; break;
    case S_MORPH_B: morph_do(MQ_FADER, 1u, 127u); go_title("MORPH"); trk[0].peak = 14000; break;
    case S_MORPH_BOTH_MID:
        morph_set_target(1); morph_do(MQ_FADER, 3u, 64u); go_title("MORPH"); trk[0].peak = 9000; trk[1].peak = 3000; break;
    /* DUELL step 5: HOME's MODE (HOME tapped three times from OVERVIEW: SPLIT, the power-on default), STEREO, LAYER (KNOB 1 just turned); the
     * MIXER in SPLIT (each part's PAN fixed: LEFT / RIGHT, DIM), part 2 selected and KNOB 2 turned (the message) */
    case S_MODE: go_home(); home_tap(); home_tap(); home_tap(); break;
    case S_MODE_STEREO: ui_mode_set = DM_STEREO; duell_mode_req = DM_STEREO; go_title("MODE"); break;
    case S_MODE_LAYER: ui_mode_set = DM_LAYER; duell_mode_req = DM_LAYER; go_title("MODE"); ui.hot_col = 0; ui.hot_t = 30; break;
    case S_MIXER_SPLIT:
        go_page(GR_GLO); trk[0].p[P_PAN] = 20; trk[1].p[P_PAN] = -30; trk[1].p[P_REV] = 60; trk[0].peak = 9000; break;
    case S_MIXER_SPLIT_TURNED: go_page(GR_GLO); song.sel = 1; turn(EN_K2, 1); break;
    case S_EDIT_PEEK: go_title("EDIT 1"); ui.layer = LAYER_EDIT; break;
    case S_EDIT_ACTIVE: eng(E_FM); apply_preset_to(TSEL, 2); favorite_set(E_FM, 2, 1); go_home(); ui.layer = LAYER_EDIT;
        ui.hot_col = 0; ui.hot_t = 30; break;
    case S_EDIT_USER: song.playing = 0; eng(6); up_store(6, "MY LONG TRIO NAME"); up_load(6); favorite_set(NENGINES, 6, 1);
        go_home(); ui.layer = LAYER_EDIT; break;
    case S_LAYER_HINT: go_page(GR_GLO); ui.msg_t = 0; layer_tap(LAYER_SCL); break;
    /* #83: locked open by a double tap (the lock after the header's name): EDIT */
    case S_LAYER_LOCK: go_title("AMP ENV"); ui.layer = ui.lock = LAYER_EDIT; break;
    /* 1.2 SEQ TOOLS (SEQ held on STEP): the sequence's tools, a song playing (every tool dimmed: STOP TO EDIT) */
    case S_SEQ_TOOLS: go_page(GR_ROLL); ui.layer = LAYER_SEQ; break;
    case S_SEQ_TOOLS_SONG: go_page(GR_ROLL); song.playing = 1; ui.layer = LAYER_SEQ; break;
    case S_NAME_USER: song.playing = 0; go_page(GR_BROWSE); name_open(NK_USER_SAVE, 1); break;
    case S_NAME_TYPING:
        song.playing = 0; go_page(GR_BROWSE); name_open(NK_USER_SAVE, 6);
        str_cpy(nm.s, "SUB BAS", sizeof nm.s); nm.len = 7; nm.cur = 6; nm.key = 9; nm.tap = 1; nm.t = fm1_ms;
        break;
    case S_NAME_123:
        song.playing = 0; go_page(GR_SLOTS); name_open(NK_PROJ_SAVE, 1);
        str_cpy(nm.s, "LIVE 2026", sizeof nm.s); nm.len = 9; nm.cur = 5; nm.num = 1;
        break;
    case S_NAME_EMPTY: song.playing = 0; go_page(GR_SLOTS); proj_name[0] = 0; name_open(NK_PROJ_SAVE, 2); nm.s[0] = 0; nm.len = nm.cur = 0; break;
    case S_NAME_FULL:
        song.playing = 0; up_store(4, "PAD"); go_page(GR_BROWSE); name_open(NK_USER_RENAME, 4);
        str_cpy(nm.s, "MWMWMWMWMWMW", sizeof nm.s); nm.len = nm.cur = 12;
        break;
    case S_NAME_PLAYING: go_page(GR_BROWSE); name_open(NK_USER_SAVE, 6); ui_message("STOP TO SAVE"); break;
    case S_PROJECT_NAMED:
        song.playing = 0; project_save_as(0, "LOFI JAM"); project_save_as(1, "MWMWMWMWMWMW"); project_save_as(2, "");
        song.g[G_SLOT] = 2; go_page(GR_SLOTS); ui.act = 4; ui.msg_t = 0;
        break;
    /* 1.2 (Discussion #90): PRESETS > LIST BASS (factory basses and a user one); OTHER with none; NAME's category
     * (the widest, PLUCK) on a rename of a long name; PROJECT's random name */
    case S_PRESETS_CAT:
        song.playing = 0; up_store_cat(3, "MY DEEP BASS", CAT_BASS);
        eng(0); apply_preset(2); list_set(1u + CAT_BASS); go_page(GR_BROWSE);
        break;
    case S_PRESETS_CAT_EMPTY: list_set(1u + CAT_OTHER); go_page(GR_BROWSE); break;
    case S_NAME_CAT:
        song.playing = 0; up_store_cat(4, "MWMWMWMWMWMW", CAT_PLUCK); go_page(GR_BROWSE); name_open(NK_USER_RENAME, 4);
        break;
    case S_NAME_RANDOM: song.playing = 0; go_page(GR_SLOTS); name_open(NK_PROJ_SAVE, 1); nm.rnd = 12345u; nm_random(); break;
    case S_SONG_NAMED: pslot_scene(1); ui.ppick = 0; break;   /* (the list's top: the factory patterns) */
    case S_SONG_LONG: pslot_scene(6); break;
    case S_SONG_LONG_PLAYING: pslot_scene(5); ui.ppick = 3; break;
    case S_SONG_PLAYING: pslot_scene(5); break;
    case S_SONG_UNSAVED: pslot_scene(0); break;
    case S_SONG_ADD: pslot_scene(2); break;
    case S_ROLL_EMPTY: case S_ROLL_ACID: case S_ROLL_CHORDS: case S_ROLL_TIES: case S_ROLL_LEN32: case S_ROLL_HIGH:
    case S_ROLL_LOW: case S_ROLL_WIDE: case S_ROLL_PLAYING: case S_ROLL_LEN: roll_scene(s); break;
    /* 1.1 parameter locks (ui_input.c lock_*): the roll's steps with a lock marked under them; a step held (the note
     * key of its entry) shows what KNOB 1..4 lock (HOME's: ANALOG E5 E6 ATK REL), the locked ones bright; the DRUM grid
     * with its marks over the steps; a step key held after FX (DIST CHO DLY REV, REV and DLY locked); AUTOMATION's
     * LOCK count */
    case S_ROLL_LOCKS: case S_ROLL_LOCK_HELD:
        roll_scene(S_ROLL_ACID);
        motion_set_lock(TSEL, 2, P_REV, 100); motion_set_lock(TSEL, 6, P_E4, 20); motion_set_lock(TSEL, 6, P_ATK, 90);
        motion_set_lock(TSEL, 9, P_E5, 110); motion_set_lock(TSEL, 13, P_REV, 80);
        if (s == S_ROLL_LOCK_HELD) {
            ui.plk_src = 0;
            ui.entry_open = 1;
            fm1_in.notes = 1u << 10;
            ui.hot_col = 0; ui.hot_t = 30;
        }
        break;
    case S_MOTION_LOCKS:
        motion_set_event(TSEL, 1, P_REV, 40); motion_set_lock(TSEL, 2, P_REV, 100); motion_set_lock(TSEL, 6, P_ATK, 90);
        go_auto_top();
        break;
    /* 1.1.5 AUTO LIST, SEQ > AUTOMATION since 1.2 (ui_events.c): locks and automation events of the track, the 4th selected, its
     * VALUE just turned; empty (+ ADD LOCK: step 5, the first knob of HOME); a DRUM track's long list, scrolled
     * (its lane levels, E names, the last row), playing */
    case S_AUTO_LIST:
        motion_set_event(TSEL, 0, P_LRATE, 70); motion_set_lock(TSEL, 2, P_REV, 100); motion_set_lock(TSEL, 2, P_ED_FLT, -30);
        motion_set_lock(TSEL, 6, P_ATK, 90); motion_set_lock(TSEL, 6, P_E4, 20); motion_set_event(TSEL, 8, P_PAN, -40);
        motion_set_lock(TSEL, 11, P_LD_FLT, 50); motion_set_lock(TSEL, 13, P_DLY, 80); motion_set_lock(TSEL, 15, P_GLIDE, 60);
        go_page(GR_EVENTS); ui.ev_row = 5; ui.hot_col = 3; ui.hot_t = 30;   /* (row 0: PLAY, 1: QUANTIZE) */
        break;
    case S_AUTO_LIST_ADD: ui.cursor = 4; go_page(GR_EVENTS); break;
    /* 1.2 (#162): AUTOMATION's NUDGE rows beside CHANCE and RATCH, QUANTIZE OFF, a NUDGE selected (KNOB 4 just
     * turned; step 9's on a REST: dimmed); QUANTIZE ON, its row selected (every NUDGE dimmed); + ADD NUDGE; LFO 2
     * (SYNC 1/8, TRIG FREE, POL UNI) */
    case S_AUTO_NUDGE:
    case S_AUTO_QNT:
        step_set_chance(&TSEL->step[0], 65); step_set_ratchet(&TSEL->step[4], 3); step_set_nudge(&TSEL->step[2], 3);
        step_set_nudge(&TSEL->step[6], -5); step_set_nudge(&TSEL->step[9], 2); TSEL->step[9].time = ST_REST;
        motion_set_lock(TSEL, 6, P_REV, 100);
        TSEL->p[P_SQNT] = s == S_AUTO_QNT;
        go_page(GR_EVENTS);
        if (s == S_AUTO_QNT) {
            ui.ev_row = 1;
        } else {
            (void)auto_pick(EVC(EVK_NUDGE, 6)); ui.hot_col = 3; ui.hot_t = 30;
        }
        break;
    case S_AUTO_ADD_NUDGE: TSEL->p[P_SQNT] = 0; go_auto_add(EV_NUDGE, 3); break;
    /* 1.5 (#173): AUTOMATION's VEL rows shown (+ ADD VEL's SHOW): every note step's velocity, step 5's just turned, the
     * ACC step 9's dimmed (it plays 127), among a CHANCE and a lock; + ADD VEL (SHOW) */
    case S_AUTO_VEL: {
        static const uint8_t VEL[8] = {100, 0, 64, 112, 100, 40, 127, 90};
        uint32_t i;
        track_defaults_steps(TSEL);
        TSEL->p[P_SLEN] = 16;
        for (i = 0; i < 16u; i += 2u) {
            uint8_t n = (uint8_t)(57 + (i * 5u) % 12u);
            step_put(&TSEL->step[i], ST_NOTE, i == 8u ? SF_ACCENT : 0u, 1, &n);
            TSEL->step[i].vel = VEL[i / 2u];
        }
        step_set_chance(&TSEL->step[2], 70); motion_set_lock(TSEL, 4, P_REV, 100);
        go_page(GR_EVENTS); ui.ev_vel = 1; (void)auto_pick(EVC(EVK_VEL, 4)); ui.hot_col = 3; ui.hot_t = 30;
        break;
    }
    case S_AUTO_ADD_VEL: go_auto_add(EV_VEL, 3); break;
    case S_LFO2: TSEL->p[P_LSYNC] = 6; TSEL->p[P_LTRIG] = 1; TSEL->p[P_LPOL] = 1; go_title("LFO 1 TRIG"); break;   /* (DUELL: the second LFO page: TRIG PHS POL) */
    case S_USER_FOOT: song.playing = 0; up_store(3, "MY BASS"); up_load(3); go_page(GR_BROWSE); break;   /* EDIT NAME lit */
    default: break;
    }
}
static void draw(int s)
{
    nscr = npend = nrules = 0;
    ntight = 0;
    tight[0] = 0;
    memset(host_screen, 0, sizeof host_screen);
    if (s == S_CALIBRATION) {                     /* the blocking setup screen: its two drawing steps */
        setup_title();
        setup_show("TURN RIGHT", "ALGORITHM");
        return;
    }
    if (s == S_SPLASH) {                          /* the power-on splash (ui_draw.c, the ribbon at step 5): the lockup (a graphic: its mask in the
                                                   * text colour, no text drawn) over lines coloured part 1 -> part 2 of the theme (HI-CON: two flat colours) */
        const ui_pal_t *pl = &UI_PALETTES[ux.pal % NPALETTES];
        uint32_t x, y, c0 = 0, c1 = 0, other = 0, ink = 0, soft = 0;
        uint16_t top = pl->hc ? pl->c[0] : ux_mix(ux_mix(pl->c[0], pl->c[2], 0), T_BG, 18), bot = pl->hc ? pl->c[2] : ux_mix(ux_mix(pl->c[0], pl->c[2], 100), T_BG, 18);
        n_text = 0;
        splash_frame(5, 0);
        for (y = 0; y < 240u; y++)                /* the ribbon's pixels: the two end colours present; HI-CON: nothing but them and the page */
            for (x = 0; x < 240u; x++) {
                uint16_t p = swap16(host_screen[y * 240u + x]);
                c0 += p == top; c1 += p == bot;
                if (x >= SPL_LOGO_X0 && x < SPL_LOGO_X0 + SPL_LOGO_W && y >= SPL_LOGO_Y0 && y < SPL_LOGO_Y0 + SPL_LOGO_H) {
                    ink += p == T_TEXT;                   /* (the mask's full coverage: exactly the text colour; the edges blend) */
                    soft += p != T_TEXT && p != top && p != bot && p != T_BG;
                }
                if (pl->hc && (y < 50u || y > 190u) && p != top && p != bot && p != T_BG)   /* (outside the text block) */
                    other++;
            }
        if (!c0 || !c1 || other) {
            if (rep) fprintf(rep, "FIND %-26s the splash lines: part 1 colour %u px, part 2 colour %u px, %u px of a blend in HI-CON\n", cur_name, c0, c1, other);
            nfind++;
        }
        if (ink + soft < 800u || (!pl->hc && pl->name[0] != 'M' && !ink) || n_text) {   /* (MONO's two-level screen may blend every edge) */
            if (rep) fprintf(rep, "FIND %-26s the splash lockup: %u px of the text colour, %u soft edge px, %u font texts (want >= 800 together, text colour, none)\n", cur_name, ink, soft, n_text);
            nfind++;
        } else {
            static uint8_t said;
            if (!said++) printf("ui_render: the splash draws the lockup mask in the text colour over the part 1 -> part 2 lines (%u px)\n", ink);
        }
        return;
    }
    ui.force = 1;
    mt.frame = ui.frame + 7u;                         /* (the MENU's tabs settled, as when it opens) */
    if (s == S_MENU_SLIDE) mt.pos = 0, mt.frame = ui.frame;   /* (one step of the slide from DISPLAY) */
    ui_draw();
}

/* every value of every column on every page of every engine: labels and values fit their column */
static uint32_t nsweep, nmotion;
/* #63: the page drawn again with MOTION driving each of its cards' track parameters (the motion icon beside
 * every label, the long ones too): the lint over it; the motion cleared again */
static void sweep_motion(void)
{
    uint32_t c, any = 0;
    for (c = 0; c < 4u; c++) {
        int16_t *vp = 0;
        const param_desc_t *d = page_desc(cur_page(), c, &vp);
        uintptr_t id = vp ? ((uintptr_t)vp - (uintptr_t)TSEL->p) / sizeof *vp : P_COUNT;
        if (d && d->label && d->label[0] != '-' && id < P_COUNT && !motion_set_event(TSEL, c, (uint32_t)id, *vp))
            any = 1;
    }
    if (!any) return;
    draw(-1);
    for (c = 0; c < 4u; c++) nmotion += (card_mot >> c) & 1u;
    lint();
    memset(&motion, 0, sizeof motion);
}
static void sweep_columns(void)
{
    uint32_t e, i, c, esync_pass = 0, twin_pass = 0;   /* (cloud job H: twin_pass 1 / 2: filter 2 on as LP / as COMB, so TWIN MOD and TWIN CHAIN are in the page round and the TWIN pages are drawn in both of their shapes) */
    char name[64];
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e))
            continue;                                    /* (DIGITAL without FELUCCA_FM4: no track has it) */
        for (i = 0; i < NPAGES; i++) {
            state();
            pal(UI_GREY_INDEX);
            eng(e);
            TSEL->p[P_ESYNC] = (int16_t)esync_pass;
            TSEL->p[P_TWTYPE] = (int16_t)(twin_pass == 1u ? TT_LP : twin_pass == 2u ? (e == ENGI_WAVE || e == ENGI_PHYS ? TT_LADDER : TT_COMB) : TT_OFF);
            ui.page = (uint8_t)i; page_entered();
            if (!page_visible(i)) {
                if (!twin_pass && !strncmp(PAGES[i].title, "TWIN ", 5)) { twin_pass = 1; i--; } else twin_pass = 0;   /* (TWIN MOD, TWIN CHAIN: not in the round with filter 2 OFF; seen again with it on) */
                continue;
            }
            snprintf(name, sizeof name, "%s%s/%s%s%s", "", ENGINES[e]->name, PAGES[i].title,
                     esync_pass ? " ESYNC" : "", twin_pass == 1u ? " TWIN LP" : twin_pass == 2u ? " TWIN COMB" : "");
            cur_name = name;
            for (c = 0; c < 4u; c++) {
                int16_t *vp;
                const param_desc_t *d = PAGES[i].scope == SC_GLOBAL || PAGES[i].scope == SC_TRACK || PAGES[i].scope == SC_ENGINE
                                        ? page_desc(cur_page(), c, &vp) : 0;
                int32_t v, v0;
                if (!d || !d->label || d->label[0] == '-' || PAGES[i].graph == GR_MOD) continue;
                v0 = *vp;
                for (v = d->min; v <= d->max; v++) {
                    *vp = (int16_t)v;
                    nscr = npend = 0;
                    ui.force = 1;
                    draw_columns();
                    lint();
                    nsweep++;
                    if (v - d->min > 300) v = d->max - 1;      /* wide ranges: the ends */
                }
                *vp = (int16_t)v0;
            }
            draw(-1);                                    /* the whole page */
            lint();
            sweep_motion();
            if (str_eq(PAGES[i].title, "AMP ENV") && !esync_pass) {   /* 1.5: again with ENV SYNC's note values */
                esync_pass = 1;
                i--;
            } else if (twin_pass < 2u && (!strncmp(PAGES[i].title, "TWIN ", 5) || !strncmp(PAGES[i].title, "MULTI FILTER", 12))) {   /* cloud job H: and again with filter 2 on (LP, then COMB) */
                twin_pass++;
                i--;
            } else {
                esync_pass = 0;
                twin_pass = 0;
            }
        }
        state(); pal(UI_GREY_INDEX); eng(e); go_home();   /* HOME's four knobs of this engine */
        snprintf(name, sizeof name, "%s%s/HOME", "", ENGINES[e]->name);
        cur_name = name;
        draw(-1);
        lint();
        sweep_motion();
    }
    {   /* FM6's operator pages (ui_fm6op.c): every operator, stage and value of each byte KNOB 2..4 edit (COARSE in both
         * modes), then the page with its chart / envelope */
        static const char *const PG[3] = {"OP TUNE", "OP MODE", "OP ENV"};
        uint32_t g, k, o, st, m;
        for (g = 0; g < 3u; g++) {
            state(); pal(UI_GREY_INDEX); eng(ENGI_FM6); go_title(PG[g]);
            cur_name = "FM6 operator sweep";
            for (o = 0; o < 6u; o++)
                for (st = 0; st < (g == 2u ? 4u : 1u); st++)
                    for (m = 0; m < 2u; m++)
                        for (k = 1; k < 4u; k++) {
                            uint32_t f, b, v, v0, mb;
                            fop_op = (uint8_t)o; fop_stg = (uint8_t)st;
                            f = fop_field(k);
                            if (f == FO_STG) continue;
                            b = fop_byte(f); mb = (5u - o) * FP_OP + FP_MODE;
                            fm6_patch[song.sel][mb] = (uint8_t)m;
                            v0 = fm6_patch[song.sel][b];
                            for (v = 0; v <= fm6_max(b); v++) {
                                fm6_patch[song.sel][b] = (uint8_t)v;
                                nscr = npend = 0;
                                ui.force = 1;
                                draw_columns();
                                lint();
                                nsweep++;
                            }
                            fm6_patch[song.sel][b] = (uint8_t)v0;
                            fm6_pgen[song.sel]++;
                            draw(-1);
                            lint();
                        }
        }
    }
    for (e = 0; e < 4u; e++) {                           /* the MOD page: every source and destination */
        int32_t v;
        state(); pal(UI_GREY_INDEX); go_title("MATRIX 1-4");
        cur_name = "MOD sweep";
        for (v = 0; v < MD_N; v++) {
            TSEL->p[P_M1SRC] = (int16_t)(v % MS_N); TSEL->p[P_M1DST] = (int16_t)v; TSEL->p[P_M1AMT] = (int16_t)(e * 40 - 64);
            mod_ui_slot = (uint8_t)(v & 3u);
            draw(-1);
            lint();
        }
    }
    {   /* 0.2.7: every destination's card, rows and DEST lines on the engines whose slots are named or faded differently, in DUEL and
         * STEREO, with the INSERT off and on (FUZZ): the names, the reasons and the bus lines all fit */
        static const uint8_t EN[6] = {0, ENGI_WAVE, ENGI_FM6, 3 /* LOFI */, 4 /* SAMPLE, retired: every slot UNUSED */, ENGI_PHYS};
        int32_t v;
        uint32_t m;
        for (e = 0; e < 6u; e++)
            for (m = 0; m < 2u; m++) {
                state(); pal(UI_GREY_INDEX); if (ENGI_RETIRED(EN[e])) eng_retired(EN[e]); else eng(EN[e]); duell_mode_req = m ? DM_STEREO : DM_SPLIT; TSEL->p[P_ITYPE] = (int16_t)(m * 4u);
                go_title(m ? "MATRIX 5-8" : "MATRIX 1-4");
                cur_name = "MOD DEST sweep";
                for (v = 0; v < MD_N; v++) {
                    uint32_t k = (m ? 4u : 0u) + (uint32_t)(v & 3);
                    TSEL->p[P_M1SRC + 3u * k] = (int16_t)(1 + v % (MS_N - 1)); TSEL->p[P_M1DST + 3u * k] = (int16_t)v;
                    TSEL->p[P_M1AMT + 3u * k] = (int16_t)(v * 3 - 64);
                    mod_ui_slot = (uint8_t)(v & 3u);
                    draw(-1);
                    lint();
                }
            }
        duell_mode_req = DM_SPLIT;
    }
}

/* the rolling digits (ui_draw.c roll_*): the old GLOBAL page's four (1.2: a page over HOME > CLOCK, whose KNOB 4 is
 * empty: ui_test.c ROLL_PG), SELECT and KNOB 4 turned together, BPM 129 -> 130 on a card
 * and TUNE 19 -> 20 on a card, then back. Every frame is linted and (GREY, MONO) checked for gray; with a directory,
 * filmstrips of both (each frame side by side, x4: the static frame before, then the roll's frames) as
 * DIR/<PALETTE>_bpm_roll.ppm and DIR/<PALETTE>_card_roll.ppm, the up roll above the down roll. */
#define FS_N (1u + ROLL_FRAMES)                   /* frames per filmstrip row */
#define FS_Z 4u                                   /* zoom */
#define FS_GAP 2u                                 /* px between frames, before the zoom */
static void roll_turns(int32_t s)                 /* one UI frame with the tempo moved and KNOB 4 turned by s */
{
    song.g[G_BPM] = (int16_t)(song.g[G_BPM] + s);   /* (SELECT is no tempo: GLO > CLOCK KNOB 1, in the real thing) */
    host_enc[panel.enc[EN_K4]] += s * panel.dir[EN_K4];
    host_ticks += 16000u; fm1_ms += 16u;
    ui_input();
    ui_draw();
    host_ticks += 200000u;
}
static void roll_film_put(uint8_t *img, uint32_t iw, uint32_t row, uint32_t k, int32_t x0, int32_t y0, uint32_t w, uint32_t h)
{
    uint32_t x, y;
    for (y = 0; y < h * FS_Z; y++)
        for (x = 0; x < w * FS_Z; x++) {
            uint16_t c = swap16(host_screen[(uint32_t)(y0 + (int32_t)(y / FS_Z)) * 240u + (uint32_t)x0 + x / FS_Z]);
            uint8_t *p = img + (((row * (h + FS_GAP) + FS_GAP) * FS_Z + y) * iw + (k * (w + FS_GAP) + FS_GAP) * FS_Z + x) * 3u;
            p[0] = (uint8_t)((c >> 11) * 255u / 31u);
            p[1] = (uint8_t)(((c >> 5) & 63u) * 255u / 63u);
            p[2] = (uint8_t)((c & 31u) * 255u / 31u);
        }
}
static void roll_film_save(const char *dir, const char *pal, const char *name, const uint8_t *img, uint32_t iw, uint32_t ih)
{
    char path[512];
    FILE *f;
    snprintf(path, sizeof path, "%s/%s_%s.ppm", dir, pal, name);
    f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return; }
    fprintf(f, "P6\n%u %u\n255\n", iw, ih);
    fwrite(img, 1, (size_t)iw * ih * 3u, f);
    fclose(f);
}
static void roll_frames(const char *dir)
{
    enum { HX = 0, HW = COL_W, HH = COL_H };      /* the BPM card (KNOB 1 of the TEMPO page) */
    static uint8_t bimg[(FS_N * (HW + FS_GAP) + FS_GAP) * FS_Z * (2u * (HH + FS_GAP) + FS_GAP) * FS_Z * 3u];
    static uint8_t cimg[(FS_N * (COL_W + FS_GAP) + FS_GAP) * FS_Z * (2u * (COL_H + FS_GAP) + FS_GAP) * FS_Z * 3u];
    const uint32_t biw = (FS_N * (HW + FS_GAP) + FS_GAP) * FS_Z, bih = (2u * (HH + FS_GAP) + FS_GAP) * FS_Z;
    const uint32_t ciw = (FS_N * (COL_W + FS_GAP) + FS_GAP) * FS_Z, cih = (2u * (COL_H + FS_GAP) + FS_GAP) * FS_Z;
    uint32_t p, row, k;
    for (p = 0; p < NPALETTES; p++) {
        char name[64];
        memset(bimg, 90, sizeof bimg);
        memset(cimg, 90, sizeof cimg);
        state(); song.playing = 0; pal(p);
        song.g[G_BPM] = 129; song.g[G_CDEPTH] = 19;
        go_title("TEMPO"); page_over = &ROLL_PG;
        snprintf(name, sizeof name, "%s/roll", UI_PALETTES[p].name);
        cur_name = name;
        draw(-1);
        for (k = 0; k < 8u; k++) { ui.hot_t = 0; ui.bpm_t = 0; ui_draw(); }   /* settled, nothing hot */
        lint();
        for (row = 0; row < 2u; row++) {
            roll_film_put(bimg, biw, row, 0, CARD_X(0), Y_LABEL, HW, HH);
            roll_film_put(cimg, ciw, row, 0, CARD_X(3), Y_LABEL, COL_W, COL_H);
            for (k = 1; k < FS_N; k++) {
                if (k == 1u) roll_turns(row ? -1 : 1);
                else ui_draw();
                lint();
                mono_check();
                roll_film_put(bimg, biw, row, k, CARD_X(0), Y_LABEL, HW, HH);
                roll_film_put(cimg, ciw, row, k, CARD_X(3), Y_LABEL, COL_W, COL_H);
            }
            if (ui.roll[3].from[0] || ui.roll[0].from[0] || song.g[G_BPM] != (row ? 129 : 130) ||
                song.g[G_CDEPTH] != (row ? 19 : 20)) {
                fprintf(rep, "ROLL %s: not over after %u frames (BPM %d CDP %d)\n", name, ROLL_FRAMES, song.g[G_BPM], song.g[G_CDEPTH]);
                nfind++;
            }
            for (k = 0; k < 8u; k++) ui_draw();
        }
        if (dir && (p == UI_GREY_INDEX || p == UI_BW_INDEX || !strcmp(UI_PALETTES[p].name, "GREEN"))) {
            roll_film_save(dir, UI_PALETTES[p].name, "bpm_roll", bimg, biw, bih);
            roll_film_save(dir, UI_PALETTES[p].name, "card_roll", cimg, ciw, cih);
        }
    }
    page_over = 0;
}
/* the host cost of a roll frame: the header and the cards (draw_head, draw_columns) while both strips roll,
 * against the same calls on an idle frame (nothing drawn) and a full redraw of the four cards and the header */
static void roll_cost(void)
{
    enum { N = 400 };
    uint32_t i, k;
    double t_roll = 0, t_full = 0;
    uint64_t px_roll = 0, bl_roll = 0, nroll = 0, px_full = 0, bl_full = 0;
    for (i = 0; i < N; i++) {
        clock_t c0;
        state(); song.playing = 0; pal(1);
        song.g[G_BPM] = 129; song.g[G_CDEPTH] = 19;
        go_title("TEMPO"); page_over = &ROLL_PG;
        draw(-1);
        for (k = 0; k < 8u; k++) { ui.hot_t = 0; ui.bpm_t = 0; ui_draw(); }
        px_visited = 0; blit_px = 0;
        c0 = clock();
        ui.force = 1; ui.frame++; draw_head(); draw_columns(); ui.force = 0;
        t_full += (double)(clock() - c0); px_full += px_visited; bl_full += blit_px;
        nscr = npend = 0;
        roll_turns(i & 1u ? -1 : 1);                /* the change: the card and the header redrawn whole */
        for (k = 2; k < ROLL_FRAMES; k++) {         /* the strip-only frames */
            px_visited = 0; blit_px = 0;
            c0 = clock();
            ui.frame++; draw_head(); draw_columns();
            t_roll += (double)(clock() - c0); px_roll += px_visited; bl_roll += blit_px; nroll++;
            nscr = npend = 0;
        }
    }
    fprintf(rep, "  roll frame (cards BPM 129->130 and CDP 19->20, strips only): %.2f us, %llu glyph px, %llu px blitted;"
            " header + 4 cards redrawn whole: %.2f us, %llu glyph px, %llu px blitted\n",
            t_roll / CLOCKS_PER_SEC / (double)nroll * 1e6, (unsigned long long)(px_roll / nroll), (unsigned long long)(bl_roll / nroll),
            t_full / CLOCKS_PER_SEC / N * 1e6, (unsigned long long)(px_full / N), (unsigned long long)(bl_full / N));
    page_over = 0;
}

/* the alignment check over every value its places can show, beyond the screens above (GREY): the
 * track cushions 1..4 at 12 16 24 px; every icon in a card's icon slot; the dialogs of every kind; every menu row with
 * each of its values; NAME's every key, letter and cycle in ABC and 123, the field holding every character; the
 * piano roll's C labels C-1 .. C9; the drum lanes of every kit; the mixer's LEVEL, PAN and REV over their ranges */
static void align_sweeps(void)
{
    uint32_t st, i, k, nf = nfind;
    FILE *r0 = rep, *nul = fopen("/dev/null", "w");
    int32_t v;
    if (nul) rep = nul;                              /* (not lint scenes: their draws pile up) */
    for (st = 0; st < 1u; st++) {
        state(); pal(UI_GREY_INDEX);
        cur_name = st ? "sweep LINE: cushions" : "sweep FLAT: cushions";
        for (k = 0; k < NTRK; k++)
            for (i = 0; i < 3u; i++) {
                cv_begin(40, 40, T_SURF);
                cv_icon_on(4, 4, i == 0u ? 12u : i == 1u ? 16u : 24u, trk_icon(k, 1), T_ACCENT, T_SURF);
            }
        cur_name = st ? "sweep LINE: card icons" : "sweep FLAT: card icons";
        for (i = 0; i < ICON_COUNT; i++) {
            ui.force = 1;
            draw_column(i & 3u, "LVL", "64", "", T_THEME, 500, i);
            ui.col[i & 3u][0] = 0;
        }
        ui.layer = LAYER_SCL;                           /* (the labels in M: a layer's cards) */
        for (i = 0; i < ICON_COUNT; i++) {
            ui.force = 1; ui.hot_col = (uint8_t)(i & 3u); ui.hot_t = (uint8_t)(i & 4u);
            draw_column(i & 3u, "LVL", "64", "", T_THEME, 500, i);
            ui.col[i & 3u][0] = 0;
        }
        ui.layer = 0; ui.hot_t = 0; ui_prefs = 0;
        cur_name = st ? "sweep LINE: dialogs" : "sweep FLAT: dialogs";
        for (i = CF_CLEAR_SEQ; i <= CF_SAVE_OVER; i++)
            for (k = 0; k < 4u; k++) {
                ui.confirm = (uint8_t)i; ui.confirm_trk = (uint8_t)k;
                draw_confirm();
            }
        ui.confirm = 0;
        cur_name = st ? "sweep LINE: menu rows" : "sweep FLAT: menu rows";
        ui.menu = 1;
        for (i = 0; i < MI_COUNT; i++)
            for (k = 0; k < 4u; k++) {
                ui.menu_sel = (uint8_t)i; ui_prefs = k & 1u ? 0xFFu : 0u; settings_hold = (uint8_t)k;
                ui_rec_prefs = (uint8_t)((k % 3u) | (k % 3u) << 2 | ((k + 1u) % 3u) << 4);   /* (CLICK, LEVEL, COUNT-IN: each value) */
                settings_leds = (uint8_t)(k % LEDS_COUNT); ui.force = 1;   /* (every LEDS name) */
                draw_menu();
            }
        ui_prefs = 0; ui_rec_prefs = 0;
        cur_name = st ? "sweep LINE: menu TUNE / HOME OPENS" : "sweep FLAT: menu TUNE / HOME OPENS";   /* (1.2: the project's) */
        for (k = 0; k < 2u * TUNE_N; k++) {             /* every TUNE (the cents, signed), selected or not, S and M */
            ui.menu_sel = (uint8_t)(k & 2u ? MI_TUNE : MI_CLICK); ui_prefs = 0;
            song.g[G_TUNE] = (int16_t)((int32_t)(k / 2u) + TUNE_MIN); ui.force = 1;
            draw_menu();
        }
        for (k = 0; k < 4u * HP_COUNT; k++) {          /* every HOME OPENS (DUELL 0.1), selected or not, S and M */
            ui.menu_sel = (uint8_t)(k & 2u ? MI_HOMEPG : MI_MODERECALL); ui_prefs = 0;
            ui_home_pg = (uint8_t)(k / 4u); ui.force = 1;
            draw_menu();
        }
        song.g[G_TUNE] = TUNE_DEF; ui_home_pg = 0;
        cur_name = st ? "sweep LINE: ABOUT USB+CPU" : "sweep FLAT: ABOUT USB+CPU";   /* (every USB state, the CPU 0..99 %, S and M: ABOUT's document, DUELL 0.2.2) */
        {
            uint8_t up0 = usb.up, cf0 = usb.config;
            uint32_t su0 = usb.setups, sof0 = usb.sof_seen;
            ui.menu = 2; ui.menu_scroll = 1;
            for (k = 0; k < 5u * 2u * 10u; k++) {
                uint32_t u = k % 5u;
                usb.up = (uint8_t)(u != 0u); usb.config = (uint8_t)(u == 4u); usb.setups = u >= 3u; usb.sof_seen = u >= 2u;
                song.cpu_q8 = (k / 10u) * 28u;
                cpum.t = fm1_ms; cpum.avg = (uint16_t)((k / 10u) * 28u * 100u / 256u); cpum.hold = (uint16_t)(cpum.avg + 15u * (k % 3u));
                cpum.miss = (k / 5u) % 4u * 77u; cpum.seen = (uint8_t)(cpum.miss != 0u); cpum.miss_t = fm1_ms;   /* (all three levels, the words) */
                ui_prefs = 0; ui.force = 1;
                draw_menu();
            }
            usb.up = up0; usb.config = cf0; usb.setups = su0; usb.sof_seen = sof0; song.cpu_q8 = 0;
            ui.menu = 1; ui.menu_scroll = 0;
        }
        cur_name = st ? "sweep LINE: menu tabs sliding" : "sweep FLAT: menu tabs sliding";
        for (i = 0; i < MI_COUNT; i++)                  /* every tab, from every place of the bar, S and M */
            for (k = 0; k <= (MTAB_COUNT - 1u) * 16u; k++) {
                ui.menu_sel = (uint8_t)i; ui_prefs = 0;
                mt.pos = (int16_t)(k * 4u); mt.frame = ui.frame - 1u; ui.force = 1;
                draw_menu();
            }
        ui.menu = 0; ui_prefs = 0; ui_rec_prefs = 0; settings_hold = 0; settings_leds = 0;
        cur_name = st ? "sweep LINE: REC layer values" : "sweep FLAT: REC layer values";
        go_home(); ui.layer = LAYER_REC;              /* (the map and the cards: every value of the three, each track) */
        for (k = 0; k < 27u * 2u; k++) {
            rp_put(RP_CLICK, k % 3u); rp_put(RP_COUNTIN, k / 3u % 3u); rp_put(RP_LEVEL, k / 9u % 3u);
            song.sel = (uint8_t)(k % NTRK); ui_prefs = 0;
            ui.force = 1; draw_layer(); ui.force = 0;
        }
        ui.layer = 0; song.sel = 0; ui_prefs = 0; ui_rec_prefs = 0;
        cur_name = st ? "sweep LINE: count-in" : "sweep FLAT: count-in";
        for (i = 1; i <= 2u; i++)                       /* 1 and 2 bars, every beat */
            for (k = 4u * i; k >= 1u; k--) {
                cin_total = (uint8_t)(4u * i); cin_left = (uint8_t)k; ui.force = 1;
                draw_countin();
            }
        cin_total = cin_left = 0;
        cur_name = st ? "sweep LINE: CPU bar" : "sweep FLAT: CPU bar";   /* (DUELL 0.2.9: the header in every palette, every step of the bar in every colour, and off) */
        for (i = 0; i < NPALETTES; i++)
            for (k = 0; k < 9u * 3u + 1u; k++) {
                state(); pal(i);
                cpub.step = (uint8_t)(k % 9u); cpub.lvl = (uint8_t)(k / 9u % 3u);
                ui_prefs2 = k == 9u * 3u ? PREF_CPU_OFF >> 16 : 0u;
                song.rec = k & 1u ? 1u << song.sel : 0u; song.octave = k % 5u == 0u ? -8 : 0; usb.config = (k % 4u) == 1u; song.batt_raw = k % 3u == 0u ? 500 : 600;
                ui.force = 1; draw_head(); ui.force = 0;
                lint();
            }
        ui_prefs2 = 0; song.rec = 0; song.octave = 0; usb.config = 0; song.batt_raw = 600;
        state(); pal(UI_GREY_INDEX);
        cur_name = st ? "sweep LINE: NAME" : "sweep FLAT: NAME";
        song.playing = 0; go_page(GR_BROWSE); name_open(NK_USER_SAVE, 6);
        for (k = 0; k < 2u; k++) {                       /* ABC, 123: every key cycling, every letter typed next */
            nm.num = (uint8_t)k;
            for (i = 0; i <= 16u; i++) {
                uint32_t n = i ? str_len(nm_group(i - 1u)) : 1u, j;
                for (j = 0; j < n; j++) {
                    nm.key = (uint8_t)i; nm.tap = (uint8_t)j;
                    nm_draw_panel();
                }
            }
        }
        nm.key = 0;
        for (i = 0; i < sizeof NM_SET - 1u; i += NM_LEN) {   /* the field: every character */
            for (k = 0; k < NM_LEN; k++) nm.s[k] = NM_SET[(i + k) % (sizeof NM_SET - 1u)];
            nm.s[NM_LEN] = 0; nm.len = NM_LEN; nm.cur = 0;
            nm_draw_field();
        }
        name_close();
        cur_name = st ? "sweep LINE: piano roll" : "sweep FLAT: piano roll";
        roll_scene(S_ROLL_ACID);
        for (v = 0; v + PR_ROWS <= 128; v++) {
            proll.lo = (uint8_t)v;
            cv_begin(240, H_GRAPH, T_SURF);
            graph_roll(TSEL, T_THEME);
        }
        state(); pal(UI_GREY_INDEX);
        cur_name = st ? "sweep LINE: mixer" : "sweep FLAT: mixer";
        go_page(GR_GLO);                             /* (0.2.14: both parts side by side, ui_layer.c mixer_panel; every value of LEVEL PAN and the
                                                      * three sends on both, MUTE, DUEL's fixed PAN, the value just turned, either part selected) */
        for (v = 0; v < 256; v++) {
            uint32_t k = v & 1u, w = (v >> 1) & 127u;
            trk[k].p[P_LEVEL] = (int16_t)w; trk[k].p[P_PAN] = (int16_t)((int32_t)w - 64);
            trk[k].p[P_REV] = (int16_t)w; trk[k].p[P_DLY] = (int16_t)(127u - w); trk[k].p[P_CHOR] = (int16_t)((w * 37u) & 127u);
            trk[k ^ 1u].p[P_MUTE] = (int16_t)((v >> 2) & 1u); trk[k].p[P_MUTE] = (int16_t)((v >> 4) & 1u);
            song.sel = (uint8_t)((v >> 3) & 1u); duell_mode_req = (uint8_t)((v >> 5) & 1u ? DM_SPLIT : DM_STEREO);
            ui.hot_t = (uint8_t)(v & 4); ui.hot_col = (uint8_t)((v >> 6) & 3u);
            ui.force = 1;
            draw_layer();                            /* (0.2.25: GLO's MIXER page, drawn as the hold-GLO layer) */
        }
        ui_prefs = 0; ui.hot_t = 0; song.sel = 0; duell_mode_req = DM_STEREO;
        ui.force = 0;
    }
    state();
    nscr = npend = nrules = 0;
    nfind = nf;
    rep = r0;
    if (nul) fclose(nul);
}

/* the keycaps' labels in their pills (tools/gen_aa_keycaps.py), from the data: the label's ink (nibbles 6..15) in the
 * pill (KC_H rows, its width) */
static void al_keycaps(void)
{
    uint32_t id;
    cur_name = "keycap data";
    for (id = 0; id < KC_COUNT; id++) {
        const kc_t *k = &KC[id];
        int32_t x, y, x0 = 999, y0 = 999, x1 = -1, y1 = -1, w = k->w;
        uint32_t i = 0;
        for (y = 0; y < KC_H; y++)
            for (x = 0; x < w; x++, i++)
                if (((KC_DATA[k->off + (i >> 1)] >> ((i & 1u) ? 0 : 4)) & 15u) >= 6u) {
                    if (x < x0) x0 = x;
                    if (y < y0) y0 = y;
                    if (x + 1 > x1) x1 = x + 1;
                    if (y + 1 > y1) y1 = y + 1;
                }
        al_record("keycap label in its pill", (x0 + x1) - w, (y0 + y1) - KC_H, k->label, 0, 0, w, KC_H, x0, y0, x1, y1);
        if (al_rep) fprintf(al_rep, "keycap %-8s w %2d  ink x %2d..%2d y %d..%d  across %+.1f up/down %+.1f\n", k->label, w, x0, x1, y0, y1,
                            ((x0 + x1) - w) / 2.0, ((y0 + y1) - KC_H) / 2.0);
    }
}
static void al_table(void)
{
    uint32_t i;
    if (!al_rep) return;
    fprintf(al_rep, "\n%-30s %8s %5s %5s %5s %6s %6s\n", "place", "measured", "1px+", "-0.5", "+0.5", "across", "up/dn");
    for (i = 0; i < al_nst; i++) {
        const alstat_t *a = &al_st[i];
        if (!strcmp(a->tag, "self-test")) continue;
        fprintf(al_rep, "%-30s %8u %5u %5u %5u %6.1f %6.1f\n", a->tag, a->n, a->nfail, a->half_lo, a->half_hi, a->wh2 / 2.0,
                a->wv2 / 2.0);
        if (a->ex_h[0]) fprintf(al_rep, "    across  worst %s\n", a->ex_h);
        if (a->ex_v[0]) fprintf(al_rep, "    up/down worst %s\n", a->ex_v);
    }
    fprintf(al_rep, "\n%u measurements, %u places, %u off by 1 px or more; %u declared places drawn without their items\n",
            al_count, al_nst, al_fail, al_lost);
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "build/ui_new";
    char path[600];
    uint32_t p, s;
    static const char *const SHOW[] = {"GREY", "MONO", "GREEN", "PAPER", "NIGHT"};   /* (NIGHT: #50, to review) */
    snprintf(path, sizeof path, "%s/report.txt", out);
    rep = fopen(path, "w");
    if (!rep) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    {
        char ap[600];
        snprintf(ap, sizeof ap, "%s/align.txt", out);
        al_rep = fopen(ap, "w");
        if (al_rep) fprintf(al_rep, "alignment: the ink of each centred text / icon / keycap against its box (gfx.c GFX_HOOK_ALIGN);"
                            " offsets in px, + right / down\n\n");
    }
    {
        char ap[600];
        snprintf(ap, sizeof ap, "%s/text_audit.tsv", out);
        audf = fopen(ap, "w");
    }
    {
        char ap[600];
        snprintf(ap, sizeof ap, "%s/font_audit.tsv", out);
        fontf = fopen(ap, "w");
        if (fontf) fprintf(fontf, "mode\tscreen\tface\tx0\ty0\tx1\ty1\ttext\n");
    }
    if (audf) fprintf(audf, "screen\ttexts\ttext_px\ticons\ticon_px\tkeycaps\tkeycap_px\tellipsised\ttight\twords\ttight_list\n");
    {   /* the lint itself: an overlap, a value too wide for its column, a text past its canvas, one past its cell */
        state(); pal(0); cur_name = "self-test";
        nscr = npend = 0;
        cv_begin(COL_W, COL_H, T_BG);
        cv_text(0, 0, &AF_S, "AAAA", T_TEXT);
        cv_text(10, 4, &AF_S, "BBBB", T_TEXT);
        cv_text_fit(0, 15, &AF_M, "WWWWWWWW", T_THEME, T_BG, COL_W);
        cv_text(40, 30, &AF_M, "CUT", T_THEME);
        cv_rrect(2, 31, 20, 13, 3, T_SURF, T_BG);          /* a cell narrower than its word */
        cv_text(4, 31, &AF_S, "SPILL", T_TEXT);
        cv_blit(4, Y_LABEL);
        lint();
        {   /* the alignment check: a word 2 px right of its box's centre, one centred */
            uint32_t f0 = al_fail;
            cv_begin(60, 20, T_BG);
            GFX_HOOK_ALIGN(0, 0, 60, 20, AL_HV, "self-test");
            cv_text_in(4, 0 + CAP_IN(S, 20), 60, &AF_S, "OFF", T_TEXT, T_BG);
            GFX_HOOK_ALIGN(0, 0, 60, 20, AL_HV, "self-test");
            cv_text_in(0, 0 + CAP_IN(S, 20), 60, &AF_S, "MID", T_TEXT, T_BG);
            if (al_fail - f0 != 1u) { fprintf(stderr, "ui_render: the alignment check missed its self-test (%u)\n", al_fail - f0); return 1; }
            al_fail = f0;
            al_count -= 2u;
            npend = 0;
        }
        if (nfind < 4u || !nspill) { fprintf(stderr, "ui_render: the lint missed its self-test (%u)\n", nfind); return 1; }
        fprintf(rep, "(self-test: %u findings above are expected)\n\n", nfind);
        nfind = 0;
    }
    for (p = 0; p < NPALETTES; p++)
        for (s = 0; s < S_COUNT; s++) {
            char name[64];
            uint32_t k;
            snprintf(name, sizeof name, "%s/%s", UI_PALETTES[p].name, S_NAME[s]);
            cur_name = name;
            setup((int)s);
            pal(p);
            if (p == UI_GREY_INDEX) aud = audf;
            draw((int)s);
            lint();
            if (p == UI_GREY_INDEX) { audit_scene(S_NAME[s]); font_scene("FLAT", S_NAME[s]); aud = 0; }
            mono_check();
            for (k = 0; k < sizeof SHOW / sizeof SHOW[0]; k++)
                if (!strcmp(UI_PALETTES[p].name, SHOW[k])) write_ppm(out, SHOW[k], S_NAME[s]);
            if (!strcmp(UI_PALETTES[p].name, "VIOLET")) {   /* DUELL 0.1: the changed MENU rows and GLO's MIDI pages, for the owner (VIOLET, the default theme) */
                static const char *const V01[] = {"menu", "menu_home", "menu_control", "menu_audio", "menu_system", "menu_info_row", "menu_about_row",
                                                  "midi", "midi_sel", "midi_layer", "midi_duck", "midi_learn_off", "midi_learn_cc", "midi_learn_on", "clock", "info", "about", "about_scroll", "test", "test_alarm",
    "scl_chromatic", "scl_chromatic_play", "chord_noscale", "chord_noscale7"};
                for (k = 0; k < sizeof V01 / sizeof V01[0]; k++)
                    if (!strcmp(S_NAME[s], V01[k])) write_ppm(out, "VIOLET", S_NAME[s]);
            }
            if (!strcmp(UI_PALETTES[p].name, "VIOLET")) {   /* 0.2.10 deck: the presentation's screens (docs/presentation/screens_0210; 0.2.13: screens_0213), VIOLET */
                static const char *const DECK[] = {"home", "fx", "dly", "env", "lfo2", "mod", "glo_page", "midi", "midi_duck", "midi_layer", "mixer_split", "mode_layer",
                                                   "morph_rolled", "patches_save", "layer_edit_active", "layer_quick_filter", "layer_quick_lfo", "perform", "splash",
                                                   "quick_glo", "multis", "multis_2", "guide_1", "guide_2", "guide_3", "guide_4", "guide_5", "guide_6",   /* (0.2.13 deck: + these) */
                                                   "user", "patches_factory", "patches_restore", "patches_save", "patches_save_both", "tools", "tools_restore", "confirm_restore_all"};   /* (0.2.17: the one list of patch slots) */
                for (k = 0; k < sizeof DECK / sizeof DECK[0]; k++)
                    if (!strcmp(S_NAME[s], DECK[k])) write_ppm(out, "VIOLET", S_NAME[s]);
            }
            if ((s == S_SPLASH || s == S_ABOUT || s == S_ABOUT_REC || s == S_MENU_INFO) &&   /* DUELL: the splash, ABOUT, the CPU meter in */
                (!strcmp(UI_PALETTES[p].name, "VIOLET") || !strcmp(UI_PALETTES[p].name, "HI-CON") || !strcmp(UI_PALETTES[p].name, "PALE")))   /* VIOLET (the default) and HI-CON */
                write_ppm(out, UI_PALETTES[p].name, S_NAME[s]);
        }
    {   /* MORPH v4 stills: the fader at A, 64, B and BOTH (64, B), each silent and playing (40 steps of a part sounding:
         * notes every 8 steps, its level, envelope, LFO 1 and 2 moving), as OUTDIR/ppm/<PAL>_mv_<fader>_<silent|play>.ppm */
        static const int MS[] = {S_MORPH, S_MORPH_MID, S_MORPH_B, S_MORPH_BOTH_MID, S_MORPH_BOTH};
        static const char *const MN[] = {"A", "64", "B", "both64", "bothB"};
        static const char *const MP[] = {"VIOLET", "COBALT", "ORANGE", "MINT", "GREY", "HI-CON"};
        uint32_t k, j, pl, st, q;
        for (p = 0; p < NPALETTES; p++)
            for (j = 0; j < sizeof MP / sizeof MP[0]; j++)
                if (!strcmp(UI_PALETTES[p].name, MP[j]))
                    for (k = 0; k < sizeof MS / sizeof MS[0]; k++)
                        for (pl = 0; pl < 2u; pl++) {
                            char nm[48];
                            cur_name = "morph_v4";
                            setup(MS[k]);
                            pal(p);
                            for (st = 0; st < 40u; st++) {
                                for (q = 0; q < 2u; q++) {
                                    uint32_t ph = st % 8u, d = q ? 3u : 0u;
                                    trk[q].peak = pl ? (int32_t)(q ? 6000 : 12000) * (int32_t)(8u - (ph + d) % 8u) / 8 : 0;
                                    trk[q].v[0].active = (uint8_t)pl;
                                    trk[q].v[0].pitch_cur = (int32_t)(q ? 72 : 55 + (st / 8u) % 2u * 5u) * 16;
                                    trk[q].v[0].env_out = pl ? 30000 * (int32_t)(8u - (ph + d) % 8u) / 8 : 0;
                                    trk[q].lfo_val = pl ? sine_i(st * 0x0C000000u + q * 0x40000000u) : 0;
                                    trk[q].lfo_fade = 32767;
                                    trk[q].lfo2.val = pl ? sine_i(st * 0x05000000u) : 0;
                                    trk[q].lfo2.fade = 32767;
                                }
                                ui.frame += 4u;
                                draw(MS[k]);
                            }
                            snprintf(nm, sizeof nm, "mv_%s_%s", MN[k], pl ? "play" : "silent");
                            write_ppm(out, MP[j], nm);
                            for (q = 0; q < 2u; q++) {
                                trk[q].v[0].active = 0;
                                trk[q].peak = 0;
                                trk[q].lfo_val = trk[q].lfo2.val = 0;
                            }
                        }
    }
    if (getenv("UI_PREVIEW")) {   /* DUELL design preview (SPEC 5a): the MORPH, MORPH BOTH, MIXER, MODE, EDIT and GLO pages of every palette with
                                   * part 1 and part 2 selected, as <PAL>.<part>_<screen>.ppm under $UI_PREVIEW/ppm (the sheet: DUELL/design_preview.py) */
        static const int PV[] = {S_MORPH, S_MORPH_BOTH, S_MIXER, S_MODE, S_EDIT_ANALOG, S_GLO_ACTIVE, S_SONG_LANES, S_GLO_METERS, S_GLO_CLIP, S_MIDI_DUCK, S_MIDI_LEARN_PICK};
        const char *pd = getenv("UI_PREVIEW");
        char tag[24], pp[600];
        uint32_t part, k;
        snprintf(pp, sizeof pp, "%s/ppm", pd);
        { char cmd[640]; snprintf(cmd, sizeof cmd, "mkdir -p '%s'", pp); if (system(cmd)) return 1; }
        for (p = 0; p < NPALETTES; p++)
            for (part = 0; part < 2u; part++)
                for (k = 0; k < sizeof PV / sizeof PV[0]; k++) {
                    cur_name = "preview";
                    setup(PV[k]);
                    song.sel = (uint8_t)part;
                    pal(p);
                    ui.force = 1;
                    draw(PV[k]);
                    snprintf(tag, sizeof tag, "%s.%u", UI_PALETTES[p].name, part + 1u);
                    write_ppm(pd, tag, S_NAME[PV[k]]);
                }
        nfind = 0;
    }
    {   /* every FM6 chart (the lint above, fmp_check, runs on each), in GREY: gray */
        uint32_t a, c0 = fmp_charts;
        for (a = 1; a <= 32u; a++) {
            char name[32];
            snprintf(name, sizeof name, "FM6 ALG %u", a);
            cur_name = name;
            state(); pal(UI_GREY_INDEX); eng(ENGI_FM6); TSEL->p[P_E0] = (int16_t)a; go_title("EDIT 1");
            draw(-1);
            lint();
            mono_check();
        }
        if (fmp_charts - c0 != 32u) { fprintf(stderr, "ui_render: %u FM6 charts drawn of 32\n", fmp_charts - c0); return 1; }
    }
    {   /* every FM6 chart again on OPERATOR, each operator picked in turn, its frequency at its widest ("x61.69", every
         * operator FIXED: "9.77 KHZ") top right: the same chart lint (the text is part of the label's net), in GREY */
        uint32_t a, o, w, k, c0 = fmp_charts;
        for (w = 0; w < 2u; w++)
            for (a = 1; a <= 32u; a++)
                for (o = 0; o < 6u; o++) {
                    char name[40];
                    snprintf(name, sizeof name, "FM6 ALG %u OP %u %s", a, o + 1u, w ? "FIXED" : "RATIO");
                    cur_name = name;
                    state(); pal(UI_GREY_INDEX); eng(ENGI_FM6); TSEL->p[P_E0] = (int16_t)a;
                    for (k = 0; k < 6u; k++) {
                        fm6_patch[song.sel][k * FP_OP + FP_MODE] = (uint8_t)w;
                        fm6_patch[song.sel][k * FP_OP + FP_FC] = w ? 3u : 31u;
                        fm6_patch[song.sel][k * FP_OP + FP_FF] = 99u;
                    }
                    fm6_pgen[song.sel]++;
                    fop_op = (uint8_t)o;
                    go_title("OP TUNE");
                    draw(-1);
                    lint();
                    mono_check();
                }
        if (fmp_charts - c0 != 384u) { fprintf(stderr, "ui_render: %u FM6 operator charts drawn of 384\n", fmp_charts - c0); return 1; }
    }
    sweep_columns();
    roll_frames(argc > 2 ? argv[2] : 0);
    al_off = 1;                                     /* (the alignment check is not timed) */
    /* draw cost: a full redraw of a screen (all strips), and HOME frame by frame (the scope, every other frame) */
    {
        static const int COST[] = {S_HOME, S_PRESETS, S_STEP, S_ROLL_CHORDS, S_MIXER, S_MENU, S_ABOUT_CREDITS, S_CONFIRM_PROJ,
                                   S_LFO, S_ENV, S_MORPH, S_MORPH_B, S_MORPH_BOTH};
        enum { N = 200 };
        uint32_t i, k;
        fprintf(rep, "\ndraw cost on the host (cc -O1; only the ratios mean anything on the device):\n");
        fprintf(rep, "  screen            full redraw us   texts+icons   glyph px read\n");
        for (k = 0; k < sizeof COST / sizeof COST[0]; k++) {
            clock_t c0;
            uint64_t px, nt;
            setup(COST[k]); pal(1);
            draw(COST[k]);
            px_visited = n_text = 0;
            c0 = clock();
            for (i = 0; i < N; i++) draw(COST[k]);
            px = px_visited / N; nt = n_text / N;
            fprintf(rep, "  %-16s %15.1f %13llu %15llu\n", S_NAME[COST[k]], (double)(clock() - c0) / CLOCKS_PER_SEC / N * 1e6,
                    (unsigned long long)nt, (unsigned long long)px);
        }
        {
            clock_t c0;
            setup(S_HOME); pal(1); draw(S_HOME);
            px_visited = n_text = 0;
            c0 = clock();
            for (i = 0; i < N * 4; i++) { ui_draw(); nscr = npend = 0; }
            fprintf(rep, "  HOME, a frame without force (lazy strips): %.1f us, %llu texts\n",
                    (double)(clock() - c0) / CLOCKS_PER_SEC / (N * 4) * 1e6, (unsigned long long)(n_text / (N * 4)));
        }
        {   /* the STEP page's piano roll: its panel alone redrawn, and a frame that changes nothing (the signature only) */
            static const int G[] = {S_ROLL_CHORDS, S_ROLL_PLAYING, S_LFO, S_ENV, S_HOME, S_MORPH, S_MORPH_B, S_MORPH_BOTH};
            for (k = 0; k < sizeof G / sizeof G[0]; k++) {
                clock_t c0;
                double tg, ti;
                setup(G[k]); pal(1); draw(G[k]);
                c0 = clock();
                for (i = 0; i < N * 4; i++) { ui.force = 1; draw_graph(); ui.force = 0; nscr = npend = 0; }
                tg = (double)(clock() - c0) / CLOCKS_PER_SEC / (N * 4) * 1e6;
                song.playing = 0;                        /* (no playhead moving) */
                ui_draw(); ui_draw(); nscr = npend = 0;
                c0 = clock();
                for (i = 0; i < N * 4; i++) { ui.frame++; draw_graph(); nscr = npend = 0; }
                ti = (double)(clock() - c0) / CLOCKS_PER_SEC / (N * 4) * 1e6;
                fprintf(rep, "  %s: the panel redrawn %.1f us; an unchanged frame (signature only) %.2f us\n", S_NAME[G[k]], tg, ti);
            }
        }
        roll_cost();
    }
    al_off = 0;
    al_keycaps();
    align_sweeps();
    al_table();
    if (al_rep) fclose(al_rep);
    fprintf(rep, "\nalignment: %u measurements of %u places, %u off by 1 px or more (OUTDIR/align.txt)\n", al_count, al_nst - 1u, al_fail);
    if (audf) fclose(audf);
    if (fontf) fclose(fontf);
    {
        uint32_t i;
        fprintf(rep, "\ncard values set in S (M too wide for the card with its unit; FLAT and LINE, the sweep too): %llu draws, "
                "%u distinct%s:", (unsigned long long)cs_count, cs_n + cs_more, cs_more ? " (the first 64)" : "");
        for (i = 0; i < cs_n; i++) fprintf(rep, " '%s'", cs_seen[i]);
        fprintf(rep, "\n");
    }
    fprintf(rep, "\n%u lint findings, %u inks on their own colour, %u ellipsised free texts, %u GREY / MONO pixels off gray / "
            "neutral; %u column values swept; %u FM6 charts linted; %u cards linted with MOTION's icon\n", nfind, nink, nfree, mono_bad,
            nsweep, fmp_charts, nmotion);
    if (!nmotion) { fprintf(stderr, "ui_render: the MOTION sweep marked no card\n"); nfind++; }
    {   /* 0.2.19: the header names the page the way its PAGES entry does: MIXER says MIXER (never the engine: "ENGINE ANALOG") */
        static const struct { int scene; const char *title; } HT[] = {{S_MIXER, "MIXER"}, {S_MIXER_SPLIT, "MIXER"}, {S_MODE_STEREO, "MODE"}};
        uint32_t k;
        for (k = 0; k < sizeof HT / sizeof HT[0]; k++) {
            char tb[24];
            uint32_t pi;
            setup(HT[k].scene);
            head_page_title(tb, &pi);
            if (strcmp(tb, HT[k].title)) { fprintf(stderr, "ui_render: scene %s: header '%s', want '%s'\n", S_NAME[HT[k].scene], tb, HT[k].title); nfind++; }
        }
    }
    fclose(rep);
    printf("ui_render: %u screens x %u palettes + the page/value sweep; %u lint findings, %u ellipsised free texts (%u distinct), "
           "%u inks on their own colour, %u GREY / MONO pixels off gray / neutral; report %s\n",
           (unsigned)S_COUNT, (unsigned)NPALETTES, nfind, nfree, nfree_seen, nink,
           mono_bad, path);
    printf("ui_render: alignment: %u measurements of %u places, %u off by 1 px or more%s\n", al_count, al_nst - 1u, al_fail,
           al_fail ? " (see align.txt)" : "");
    return nfind || nink || mono_bad || al_fail ? 1 : 0;
}
