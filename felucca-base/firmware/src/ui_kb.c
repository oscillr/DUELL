/* SPDX-License-Identifier: GPL-3.0-only */
/* The keyboard drawing of GLOBAL SCALE (one octave) and CHORD (two octaves): block F of LAYOUT_IMPLEMENTATION_PLAN.md.
 * Not included anywhere yet; the wiring job includes it in ui_graph.c, after gfx.c (cv_rrect, ux_mix and the T_* tokens) and before graph_scale.
 *
 * Look (SCALE_CHORD_NEW_LAYOUT_V3.png, CHORD_NEW_LAYOUT_V5.png): white keys fill the width evenly with a gap of 2 px and rounded corners; black keys on top,
 * 60 percent of the height, each with a 1 px ring of the page background around it so that it reads as a separate key. A lit white key and a lit black key
 * have the SAME colour (the `colour` argument: the page's accent); an unlit white key is lighter than an unlit black key; the root key is a light tint.
 * No other mark is drawn. */

#define KB_GAP 2              /* the gap between two white keys, in pixels */
#define KB_BLACK_PCT 60       /* the height of a black key, percent of the keyboard's height */
#define KB_RADIUS 3           /* the corner radius of every key, in pixels */
#define KB_BLACK_W_NUM 2      /* a black key is this many thirds of a white key's pitch wide (QUESTION for the owner: the width, see the report) */
#define KB_BLACK_W_DEN 3
#define KB_WHITE_OFF T_RAISE  /* an unlit white key: the lighter of the two unlit tones */
#define KB_BLACK_OFF T_BASE   /* an unlit black key: the darker one */
#define KB_ROOT T_TEXT        /* the root key: the light text tone */

/* lit: bit mask of the semitones counted from C (bit 0 = C, up to 24 bits); root: a semitone index counted the same way, or 0xFF for none;
 * octaves: 1 or 2; colour: the lit keys' colour; x, y, w, h: the whole keyboard (x = 9, w = 222 on a 240 px page, h = 74). */
static void kb_draw(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t lit, uint32_t root, uint32_t octaves, uint16_t colour)
{
    /* white keys in an octave: C D E F G A B -> their semitone; a black key sits after the white key whose index is listed (C#, D#, F#, G#, A#) */
    static const uint8_t WHITE_SEMI[7] = {0, 2, 4, 5, 7, 9, 11};
    static const uint8_t BLACK_AFTER[5] = {0, 1, 3, 4, 5};   /* the white key index (0 = C) the black key follows */
    static const uint8_t BLACK_SEMI[5] = {1, 3, 6, 8, 10};
    int32_t n, i, bh, bw, pitch_n;
    if (octaves < 1u) octaves = 1u;
    if (octaves > 2u) octaves = 2u;
    CV_HL(x, y - 1, w, h + 1);   /* (data: the keys are the picture; the box lint skips these fills) */
    n = (int32_t)octaves * 7;
    pitch_n = w + KB_GAP;                                    /* the edge of white key i: x + i * (w + gap) / n, so the keys use the full width */
    bh = h * KB_BLACK_PCT / 100;
    bw = (pitch_n / n - KB_GAP) * KB_BLACK_W_NUM / KB_BLACK_W_DEN;
    for (i = 0; i < n; i++) {                                /* the white keys first */
        uint32_t semi = (uint32_t)WHITE_SEMI[i % 7] + 12u * (uint32_t)(i / 7);
        int32_t x0 = x + i * pitch_n / n, x1 = x + (i + 1) * pitch_n / n - KB_GAP;
        uint16_t col = semi == root ? (uint16_t)KB_ROOT : ((lit >> semi) & 1u) ? colour : (uint16_t)KB_WHITE_OFF;
        cv_rrect(x0, y, x1 - x0, h, KB_RADIUS, col, T_BG);
    }
    for (i = 0; i < (int32_t)octaves * 5; i++) {             /* the black keys on top, each in a ring of the page background */
        uint32_t semi = (uint32_t)BLACK_SEMI[i % 5] + 12u * (uint32_t)(i / 5);
        int32_t wi = (int32_t)BLACK_AFTER[i % 5] + 7 * (i / 5);
        int32_t cx = x + (wi + 1) * pitch_n / n - KB_GAP / 2;   /* the middle of the gap between the two white keys */
        int32_t bx = cx - bw / 2;
        uint16_t col = semi == root ? (uint16_t)KB_ROOT : ((lit >> semi) & 1u) ? colour : (uint16_t)KB_BLACK_OFF;
        cv_rrect(bx - 1, y - 1, bw + 2, bh + 2, KB_RADIUS + 1, T_BG, T_BG);   /* the ring: 1 px of page background around the key (it also reaches 1 px over the top edge) */
        cv_rrect(bx, y, bw, bh, KB_RADIUS, col, T_BG);
    }
}
