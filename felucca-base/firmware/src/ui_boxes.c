/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.0 layout: the BOX GRID (DUELL/release_drafts/LAYOUT_IMPLEMENTATION_PLAN.md block D; the approved pictures PERFORM_NEW_LAYOUT_V4.png and
 * QUICK_SEQ_NEW_LAYOUT.png). One function for the PERFORM page's 16 effects (4 x 4) and the QUICK SEQ layer's 8 tools (4 x 2), drawn into the big
 * area of the area page (template 9, grid_on 16: ui_draw.c area_rect). Included by ui_layer.c after its cell states (LS_OFF, LS_HELD, LS_WAIT,
 * LS_DIM) and after ui_draw.c (area_rect, the faces, the theme colours).
 *   A box: a rounded rectangle (cv_rrect, radius BOX_R) in a dark fill (the picture's 36,10,46 on VIOLET: the part tone mixed toward the theme
 *   colour by BOX_FILL_PCT). Held: the accent fill with dark text. Waiting (an effect that waits for the next 1/16): the theme fill, as today.
 *   Cannot run now: the dark fill with dimmed text, as today.
 *   The name: CENTRED across the box (the owner's rule for these two pages: FIRMWARE_CHANGE_LIST_2026-10-28.md B.5). Two text sizes only: the
 *   larger face (BOX_BIG) where the picture shows the larger size AND the name's ink fits the box with BOX_FIT_PAD free at each side, else the
 *   11-row face (BOX_SMALL). Without a key letter the name is centred up and down too; with one (QUICK SEQ) the name's capitals end BOX_NAME_FOOT
 *   rows above the box's last row, under the key letter.
 *   The key letter (QUICK SEQ: the white key's note name, F G A B over C D E F) in the box's top left corner, in the accent colour, face BOX_KEYF.
 * The faces are one constant each: BOX_BIG is the names role (the 20 px face P, owner 2026-10-29).
 * The geometry comes from ui_tokens.h (template 9 block): UT_BOX_X0, UT_BOX_DX, UT_BOX_W across; the caller gives the rows (UT_BOX_PF_* for
 * PERFORM, UT_BOX_QS_* for QUICK SEQ). */
#define BOX_BIG NAME               /* the larger name size: the names role AF_NAME (ui_tokens.h: face P, Bold 20, capitals 14 rows; a developer bench build: face L) */
#define BOX_SMALL M                /* the 11-row face for the longer names (M: REVERSE's ink is 54 px, exactly the box; in S it is 55 and would be cut) */
#define BOX_KEYF S                 /* the key letter's face (11 rows, the smallest allowed) */
#define BOX_AF(F) BOX_AF_(F)
#define BOX_AF_(F) (&AF_##F)
#define BOX_CAP_Y(F) BOX_CAP_Y_(F)
#define BOX_CAP_Y_(F) AF_##F##_CAP_Y
#define BOX_CAP_H(F) BOX_CAP_H_(F)
#define BOX_CAP_H_(F) AF_##F##_CAP_H
#define BOX_R 4                    /* the corner radius (as the old cells' lc_box) */
#define BOX_FILL_PCT 13            /* the dark fill: T_BASE -> T_THEME, % (VIOLET 36,11,47; the pictures 36,10,46) */
#define BOX_FIT_PAD 2              /* the larger size only when the name's ink leaves this many px free at each side */
#define BOX_KEY_X 4                /* the key letter's pen from the box's left edge (the picture: x 13 in a box from x 9) */
#define BOX_KEY_TOP 3              /* .. its capitals' first row below the box's top */
#define BOX_NAME_FOOT 6            /* with a key letter: the name's capitals end this many rows above the box's last row */

typedef struct {
    const char *name;              /* the name, as printed */
    char key;                      /* the key letter in the corner, 0: none */
    uint8_t big;                   /* the picture shows this name in the larger size */
    uint8_t state;                 /* LS_OFF (idle), LS_HELD, LS_WAIT, LS_DIM (ui_layer.c) */
} box_t;

/* the fill, the name's colour and the key letter's colour of a box in state st */
static uint16_t box_colours(uint32_t st, uint16_t *ink, uint16_t *keyc)
{
    if (st == LS_HELD) {
        *ink = *keyc = T_BG;
        return T_ACCENT;
    }
    if (st == LS_WAIT) {
        *ink = *keyc = T_INK;
        return T_THEME;
    }
    *ink = st == LS_DIM ? T_DIM : T_TEXT;
    *keyc = st == LS_DIM ? T_DIM : T_ACCENT;
    return ux_mix(T_BASE, T_THEME, BOX_FILL_PCT);
}

/* the boxes b[0 .. rows * cols - 1], row by row, left to right: the first row's top at screen row y0, the rows pitch apart, each h high; the
 * canvas being drawn covers the big area (its row 0 is the area's first row, area_rect) */
static void box_grid(int32_t y0, uint32_t rows, uint32_t cols, int32_t pitch, int32_t h, const box_t *b)
{
    int32_t ay, ah;
    uint32_t r, c;
    area_rect(&ay, &ah);
    (void)ah;
    for (r = 0; r < rows; r++)
        for (c = 0; c < cols; c++) {
            const box_t *e = &b[r * cols + c];
            int32_t x = UT_BOX_X0 + UT_BOX_DX * (int32_t)c, y = y0 - ay + pitch * (int32_t)r, w = UT_BOX_W, ty;
            uint16_t ink, keyc, fill = box_colours(e->state, &ink, &keyc);
            int32_t ib[4];
            int big;
            { CV_HL(x, y, w, h); cv_rrect(x, y, w, h, BOX_R, fill, T_BG); }
            if (e->key) {
                char k[2] = {e->key, 0};
                cv_text_on(x + BOX_KEY_X, y + BOX_KEY_TOP - BOX_CAP_Y(BOX_KEYF), BOX_AF(BOX_KEYF), k, keyc, fill);
            }
            if (!e->name || !*e->name)
                continue;
            big = 0;
            if (e->big) {
                text_ink(BOX_AF(BOX_BIG), e->name, ib);
                big = ib[2] - ib[0] <= w - 2 * BOX_FIT_PAD;
            }
            if (e->key) {                                   /* under the key letter: the capitals end BOX_NAME_FOOT rows above the box's last row */
                ty = big ? y + h - 1 - BOX_NAME_FOOT - BOX_CAP_H(BOX_BIG) + 1 - BOX_CAP_Y(BOX_BIG)
                         : y + h - 1 - BOX_NAME_FOOT - BOX_CAP_H(BOX_SMALL) + 1 - BOX_CAP_Y(BOX_SMALL);
                GFX_HOOK_ALIGN(x, 0, x + w, 0, AL_H, "box name centred across");
            } else {                                        /* the capitals' band on the box's middle */
                ty = big ? y + HALF_UP(h - BOX_CAP_H(BOX_BIG)) - BOX_CAP_Y(BOX_BIG)
                         : y + HALF_UP(h - BOX_CAP_H(BOX_SMALL)) - BOX_CAP_Y(BOX_SMALL);
                GFX_HOOK_ALIGN(x, y, x + w, y + h, AL_HV, "box name centred");
            }
            cv_text_in(x, ty, w, big ? BOX_AF(BOX_BIG) : BOX_AF(BOX_SMALL), e->name, ink, fill);
        }
}
