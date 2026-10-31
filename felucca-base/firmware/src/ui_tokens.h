/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.1 UI TOKENS (item 111): the numbers of the parts every page shares, measured on the owner's FINAL_V1_*.png pictures (240 x 240, 1 px = 1 px) and checked by
 * tests/ui_render.c --target + tools/ui_diff.py. Rows and columns are SCREEN coordinates (the header canvas starts at row 0, the footer canvas at Y_FOOT: draw code subtracts it).
 * Included by gfx.c (right after ui_fonts.h, so the face names below hold in every file) and ui_draw.c; the later template work reuses these names instead of writing the numbers again.
 * Faces: the FACE ROLE MAP below (item 113). Colours are palette tokens (gfx.c T_*), never RGB. */
#pragma once

/* ---- FACE ROLE MAP (item 113): the CONSOLIDATED face set, tools/gen_aa_font.py preset barlow (all faces PLAIN: no kerning, as the design pictures set text).
 * Floor rule (owner): no text on any page is smaller than the engine name on ENGINE row 1 = Barlow Medium 15 (cap height 11). Roles of the FINAL_V1 design -> face:
 *   S  SemiBold 15, ASCII + u-umlaut + ellipsis   capsule letters and numbers, footer, cell / dot labels (design S 15 and S 16), hints, notes, legends, list rows, button words (was S 12 and F 15)
 *   M  Medium 15, ASCII + ellipsis                engine name row, units (design M 15 and M 16), patch numbers, grey means, menu row names, sentence-case lines (the design's M 18 rows are M 15 here)
 *   L  Bold 23, ASCII                             values and big headings (the design's S 20 and S 23 values; the guide's headings H / G; the splash word, the update countdown)
 *   T  Bold 15, capitals                          the header's page title
 *   D  SemiBold 12, digits only                   the number inside a numbered dot (1 .. 4): the one text under the floor, it is a glyph in an 11 px disc, not a word
 *   N  Bold 31, figures . - + %                   the design's big values (ENGINE page, FINAL_V1_02); not drawn by any page yet (the compiler drops it); text values use L
 *   X  Bold 38, A B and figures                   HOME's big MORPH value
 *   B  Bold 64, + - 0 .. 3                        the OCTAVE page's big figure
 *   P  Bold 20, capitals, figures, + - / space    role AF_NAME (owner, 2026-10-29): the TOOLS page's names and the big names of the PERFORM and QUICK SEQ boxes (capitals rows 6 .. 19 of its line, 14 rows;
 *                                                 a lower case letter shows as its capital; a character not in the face shows as a space)
 * Retired: S 12, I 18 (never drawn), H 21 and G 18 (the guide's headings: now L, they are 2 px bigger / 5 px bigger), F 15 (is S now). */
#define AF_H AF_L
#define AF_H_CAP_Y AF_L_CAP_Y
#define AF_H_CAP_H AF_L_CAP_H
#define AF_G AF_L
#define AF_G_CAP_Y AF_L_CAP_Y
#define AF_G_CAP_H AF_L_CAP_H
#define AF_DOT AF_D
#define AF_NAME AF_P            /* the names role: TOOLS names, the big PERFORM and QUICK SEQ box names (a developer bench build maps it to face L: the bench block at the end of this file) */
#define AF_NAME_CAP_Y AF_P_CAP_Y
#define AF_NAME_CAP_H AF_P_CAP_H

/* ---- NO BOXES (owner, 2026-10-26, items 122, 124): no background box, rectangle, card fill, border or divider behind text or graphics on any page; pages are plain background (T_BG).
 * There is no old-look switch and no code for one. The ONLY filled shapes: the selected row / option, the capsules, the keycap pills, bars and meters, the RECOMMENDED pill, and DATA
 * (the picture, roll, envelope fill, curves). The palette's card tone survives only as T_BASE / T_BASE_P1 (gfx.c): the base the derived greys, bar tracks, data fills and roll lanes
 * are mixed from (the colours of the final pictures), never drawn as a fill. tests/ui_render.c box_lint finds a flat fill of 12 x 12 px or more that is neither the background nor
 * allowed (GFX_HOOK_ALLOW) on every screen in every palette and fails on one. */

/* ---- header: the two capsules [part number | morph end letter | voices number] ---- */
#define UT_CAP_X 4              /* left capsule's left edge; the right capsule ends at 240 - UT_CAP_X (x 235) */
#define UT_CAP_Y 4              /* rows 4 .. 19 */
#define UT_CAP_W 37             /* x 4 .. 40 and 199 .. 235, outline included */
#define UT_CAP_H 16
#define UT_CAP_R 5              /* outer corner radius; the dim half's is one less (1 px outline) */
#define UT_CAP_CELL 12          /* the number cell (outline included) and the letter / voices text cells: 12 px each, the text centred in its cell by its advance */
#define UT_CAP_BASE 16          /* the capitals' last row of the capsule text; the page title shares it */
#define UT_CAP_DIM_PCT 27       /* a lit capsule's dim half: BG -> the part colour, % (VIOLET: 66,15,75) */
#define UT_CAP_OFF_PCT 10       /* an unlit capsule's dim half: BG -> TEXT, % (VIOLET: 32,28,32); its outline and number cell are T_DIM */
#define UT_CAP_LETTER_PCT 88    /* the morph end letter: BG -> TEXT, % (VIOLET: 222,218,230); the number and the voices number are T_TEXT (white; dark in the light palettes) */
#define UT_CAP_VOICES_OWN 1    /* item 117: STEREO and LAYER show the SELECTED part's number (the same one in both capsules, as the one shared number was). 1: each capsule shows its OWN part's number (as in DUEL, and as the pictures' 8 | 6) */

/* ---- header: the page title (face T, T_TEXT, centred on the screen's middle, no line, no colour) ---- */
#define UT_TITLE_CX 120
#define UT_TITLE_BASE 16        /* capitals' last row: the capsule text's */
#define UT_TITLE_GAP 6          /* free room between the title and a capsule */
#define UT_TITLE_L (UT_CAP_X + UT_CAP_W + UT_TITLE_GAP)           /* 47: the title room with capsules */
#define UT_TITLE_R (240 - UT_CAP_X - UT_CAP_W - UT_TITLE_GAP)     /* 193 */
#define UT_TITLE_GL 8           /* the title room of a page without capsules (global pages) */
#define UT_TITLE_GR 232

/* ---- footer (the same rows on every page): the engine name in the part colour, the patch name, the page counter, REC, the battery ---- */
#define UT_FOOT_BASE 231        /* capitals' last row of every footer word */
#define UT_FOOT_X 5             /* first word's pen */
#define UT_FOOT_WORD_GAP 8      /* engine name -> patch name; patch name -> the counter (or the REC dot) */
#define UT_FOOT_CT_R 217        /* the page counter ends here: 5 px left of the battery (x 222) */
#define UT_FOOT_REC_GAP 8       /* REC word -> the counter */
#define UT_REC_DOT 12           /* the REC dot's diameter; its left edge is the REC word's pen - 15, its top row UT_FOOT_BASE - 10 */
#define UT_REC_DOT_DX 15
#define UT_REC_DOT_DY 10

/* ---- battery: a 15 x 9 outline with its 2 x 4 nub; rows 222 .. 230, x 222 .. 236. Empty: the outline only; full: the 9 x 5 block filled; charging: the EMPTY outline with a 5 x 7 bolt ---- */
#define UT_BAT_X 222
#define UT_BAT_Y 222
#define UT_BAT_W 15
#define UT_BAT_H 9
#define UT_BAT_FX 2             /* the fill block: x 224 .. 232, rows 224 .. 228 (relative to the battery's corner) */
#define UT_BAT_FY 2
#define UT_BAT_FW 9
#define UT_BAT_FH 5

/* ---- TEMPLATE 1, the four-card part page (item 114; FINAL_V1_01_morph / 05_env / 06_lfo, 240 x 240, 1 px = 1 px; screen rows unless a name says "rel" = inside its card or panel) ----
 * The graph area: ONE card on rows 27 .. 102, x 3 .. 236 (hard corners, no border). The four cards under it: 116 x 54 at x 3 / 121, y 106 / 162 (the same card colour = T_BASE, exact; no border, no line).
 * Per card (rel): the number dot = the label's capital height (11 px), its top on the label's cap top, x 6; the label pen is the dot + 15; label baseline row 20 (the capitals' last row), label face S, colour T_MID;
 * the value's baseline row 42 for every face (L 23 / X 38 / M / S), pen x 6; its unit M, T_MID, 4 px after the value, on the SAME baseline; the bar rows 46 .. 48 (3 px, radius 1), x 6, width card - 12 = 104,
 * track = T_BASE -> T_THEME 24 % (VIOLET 59,16,73: the picture's 57,20,74), fill = the part's colour, min 3 px. A card with a two-line value has no bar: its lines' baselines are rows 35 and 50. */
#define UT_GR_X 3
#define UT_GR_Y 27              /* the graph area's card: rows 27 .. 102 */
#define UT_GR_W 234
#define UT_GR_H 76
#define UT_CARD_W 116
#define UT_CARD_H 54
#define UT_CARD_X0 3            /* column 0; column 1 is UT_CARD_DX further */
#define UT_CARD_DX 118
#define UT_CARD_Y0 106          /* row 0; row 1 is UT_CARD_DY further */
#define UT_CARD_DY 56
#define UT_CARD_PAD 6           /* left edge of the dot, the value and the bar; also the room kept free at the right */
#define UT_CARD_LBL_BASE 20     /* rel: the label's and the dot's baseline */
#define UT_CARD_LBL_DX 15       /* label pen = dot x + this (11 px dot + 4) */
#define UT_CARD_LBL_PAD_R 4     /* the room a label may use ends this far from the card's right edge (room 91 px: a label wider than that is cut by the card's edge, see ui_draw.c card_t1_paint) */
#define UT_CARD_VAL_BASE 42     /* rel: every value's baseline (and its unit's) */
#define UT_CARD_VAL2_BASE1 35   /* rel: the two lines of a two-line value (no bar then) */
#define UT_CARD_VAL2_BASE2 50
#define UT_CARD_UNIT_GAP 0      /* value -> unit */
#define UT_CARD_BAR_Y 46        /* rel */
#define UT_CARD_BAR_H 3
#define UT_CARD_BAR_R 1
#define UT_CARD_BAR_MIN 3       /* the shortest fill */
#define UT_CARD_BAR_REST_PCT 24 /* the bar's unfilled track: T_BASE -> T_THEME, % */
#define UT_CARD_BIG_BAR 1      /* 1: HOME's big A / B value gets the thin A-to-B bar under it (the ticket's text); 0: none, as the FINAL_V1_01 picture on disk shows (flag, item 114) */
#define UT_CARD_ENUM_BAR 0      /* 1: a card whose value is a NAME (an enumeration: WAVE SINE, SYNC OFF) also gets its bar; 0: no bar, as FINAL_V1_06_lfo shows (the grid cards of the other templates are not affected) */
#define UT_CARD_CORNER_A 2      /* the hard corner: row 0 loses this many px at each end, row 1 loses UT_CARD_CORNER_B (the card is NOT anti-aliased) */
#define UT_CARD_CORNER_B 1
/* a text of face F whose capitals' last row is `base`: the y to hand to cv_text_on (the line box's top) */
#define UT_TY(F, base) ((base) - AF_##F##_CAP_H + 1 - AF_##F##_CAP_Y)
/* HOME's graph area (screen rows): the knob-1 dot + MORPH on one baseline, the note at the bottom */
#define UT_GR_TITLE_X 9         /* the dot's left edge; its label's pen is + UT_CARD_LBL_DX */
#define UT_GR_TITLE_BASE 43
#define UT_GR_NOTE_X 9          /* "HOLD SAVE = SAVE PATCH", left aligned, T_DIM */
#define UT_GR_NOTE_BASE 101
/* the envelope / LFO graphs (grid_on 1): the area under the envelope's line, and the guide line (the baseline of the ENV, the zero line of the LFO) */
#define UT_GR_FILL_PCT 11       /* T_BASE -> T_THEME, % (VIOLET 32,8,41: the picture's 32,8,41) */
#define UT_GR_GUIDE_PCT 33      /* T_BASE -> T_TEXT, % (VIOLET 90,85,98 in the picture) */
#define UT_PIC_TINT_PCT 50      /* item 165 (owner): the PHASE picture's second cycle, a lighter tint of the part colour: part colour -> TEXT, % (lighter on the dark palettes, toward the text on the light ones: readable on all) */

/* ---- TEMPLATE 2, the ENGINE page (item 115; FINAL_V1_02_engine, 240 x 240, 1 px = 1 px; screen rows unless "rel" = inside its card; geometry of ui_redesign/_scripts/final_pages.py, last page_engine) ----
 * Row 1: ONE card, rows 27 .. 50 (x 3 .. 236): the numbered dot 1 (11 px, = the name's cap height) at x 9 and the engine's name (M, left aligned, NO highlight pill) at x 28, both on baseline row 43.
 * Under it the engine's picture card (the graph area): rows 54 .. 129, x 3 .. 236. Under that the three cells K2 .. K4: 76 x 83 at x 3 / 82 / 161, rows 133 .. 215: dot + label (S) on baseline rel 19,
 * the value on baseline rel 58 (figures in N 31, text in L), its unit (M) 5 px after it on the same baseline, the thin bar rel 75 .. 77 (x 6, width 64) under an ADJUSTABLE value only (a NAME like SAW has none).
 * The engine list (K1 turning) takes the picture card's place: the template-3 rows (UT_T3_*), three of them, UT_T2_LIST_Y from the card's top. */
#define UT_T2_ROW_Y 27
#define UT_T2_ROW_H 24          /* rows 27 .. 50 */
#define UT_T2_ROW_DOT_X 9
#define UT_T2_ROW_NAME_X 28
#define UT_T2_ROW_BASE 43       /* screen row: the dot's and the name's capitals' last row */
#define UT_T2_GR_Y 54           /* the picture card: rows 54 .. 129 (x UT_GR_X .. , width UT_GR_W as template 1) */
#define UT_T2_GR_H 76
#define UT_T2_CELL_X0 3
#define UT_T2_CELL_DX 79        /* cells at x 3, 82, 161 */
#define UT_T2_CELL_W 76
#define UT_T2_CELL_Y 133
#define UT_T2_CELL_H 83         /* rows 133 .. 215 */
#define UT_T2_PAD 6             /* the dot's, the value's and the bar's left edge; the room kept free at the right */
#define UT_T2_LBL_BASE 19       /* rel */
#define UT_T2_LBL_DX 15         /* label pen = dot x + this */
#define UT_T2_LBL_PAD_R 4       /* a label may use up to this far from the card's right edge; a wider one is cut by the card's edge (listed, never shortened) */
#define UT_T2_VAL_BASE 58       /* rel */
#define UT_T2_VAL2_BASE1 43     /* rel: a two-line value, no bar */
#define UT_T2_VAL2_BASE2 58
#define UT_T2_UNIT_GAP 0
#define UT_T2_NUM_DX -1         /* the N face's first ink sits 1 px right of the picture's: its figures are drawn this much left (the unit keeps its place) */
#define UT_T2_BAR_Y 75          /* rel; height, radius, track and minimum as template 1 */
#define UT_T2_ENUM_BAR 0        /* 0: a NAME (WAVE SAW) has no bar, as the picture; 1: it gets one */
#define UT_T2_LIST_Y 7          /* the engine list's first row, from the picture card's top (3 rows of UT_T3_ROW_H + 2 gaps = 61 px in 76) */

/* ---- TEMPLATE 3, the list pages PATCHES / PATTERNS / MULTIS (item 115; FINAL_V1_03_patches) ----
 * Heads: four cards in ONE row, rows 27 .. 63 (37 px), 2 px apart from x 3; each: the dot (11 px) at x +3 and the label (S, floor face) at x +16 on baseline row 43, the value (L) at x +6 on baseline row 62;
 * a card is as wide as its label needs (dot + label + 3), the last one takes the rest up to x 236 (PATCH 60, TYPE 51, FAV 43, RESTORE 75 in the picture). No bar. An action head (LOAD, ERASE, SAVE, RESTORE) shows its label only.
 * Rows: seven, the first at screen row 67, one every 21 px, each 19 px high over x 3 .. 236 (radius 4); the selected row filled with the part colour, its text white; the row number at x 14, the name at x 66, both on baseline rel 14
 * (list face M); a favourite's star (12 px) at x 214, rel 3, in the part colour (white on the selected row). The list sits on the background; there is no card behind it. */
#define UT_T3_HEAD_Y 27
#define UT_T3_HEAD_H 37
#define UT_T3_HEAD_GAP 2
#define UT_T3_HEAD_R 237        /* the last head ends before this x */
#define UT_T3_DOT_X 3
#define UT_T3_LBL_DX 16
#define UT_T3_LBL_PAD_R 3       /* label ink -> the card's right edge */
#define UT_T3_LBL_BASE 43       /* rel to the screen: capitals' last row (head canvas row = this - UT_T3_HEAD_Y) */
#define UT_T3_VAL_PAD 6
#define UT_T3_VAL_PAD_R 2       /* a value may use up to this far from the card's right edge (WHEEL, M 15, is 43 px in a 51 px card: 2 px of air) */
#define UT_T3_VAL_BASE 62       /* screen row */
#define UT_T3_LIST_Y 67
#define UT_T3_ROW_PITCH 21
#define UT_T3_ROW_H 19
#define UT_T3_ROWS 7
#define UT_T3_ROW_X 3
#define UT_T3_ROW_W 234
#define UT_T3_ROW_R 4
#define UT_T3_TAG_X 14
#define UT_T3_NAME_X 66
#define UT_T3_TEXT_BASE 14      /* rel to the row's top */
#define UT_T3_STAR_X 214
#define UT_T3_STAR_DY 3
#define UT_T3_HINT 0            /* 1: the footer's "HOLD SAVE = SAVE PATCH / PATTERN / MULTI" line stays on these pages (the picture has none: it would cover the footer's page counter) */

/* ---- TEMPLATE 4, the MIXER page (GLO's first page and QUICK GLO over any page, the same screen; item 117; FINAL_V1_09_mixer, 240 x 240, 1 px = 1 px; screen rows unless "rel" = inside its card) ----
 * Geometry measured on the picture (its body is the owner's old PAGES_V2_glo-a cells, so the cards are ROUND, radius 4, not template 1's hard corners): two MUTE cards 116 x 60 at x 3 / 121, rows 30 .. 89;
 * under them LEVEL 1 | LEVEL 2 (K1, K2) rows 93 .. 151 and PAN 1 | PAN 2 (K3, K4) rows 155 .. 213, cards 116 x 59 at x 3 / 121: part 1 on the left, part 2 on the right, on both rows.
 * MUTE card (rel): the word MUTE 1 / MUTE 2 (S, grey) baseline 15, "Hold" + the GLO keycap baseline 32, "and press" + the OCT- / OCT+ keycap baseline 48 (the keycaps stand on those baselines), text and keycaps from x 3;
 * the small level meter at the right: 6 x 44 at x rel 106, rows rel 8 .. 51, radius 3, track = card -> TEXT 13 % (VIOLET 40,34,48), the level in the part's colour from the bottom.
 * LEVEL / PAN card (rel): the numbered dot (11 px = the label's cap height, white) at x 3, the label (S, grey) pen = dot + 14, baseline 15; the value (L) baseline 37 from x 3, its unit (M, grey) 4 px after it on the same baseline
 * (PAN has none: its own signed number, no % sign); the thin bar rel 51 .. 53, x 3, width 110 (the card - 6), radius 1, track = card -> TEXT 13 %, fill in the part's colour (DIM when the part is muted); PAN's bar fills from
 * the middle and its middle carries a 1 px tick (5 rows, rel 50 .. 54, grey). The value's colour is the part's colour (the picture's two violet / pink tones are the old page's gradient; part 1 violet, part 2 pink here). */
#define UT_T4_X0 3              /* left column; the right one is UT_T4_DX further */
#define UT_T4_DX 118
#define UT_T4_W 116
#define UT_T4_R 4               /* the cards' corner radius (round, as the picture) */
#define UT_T4_MUTE_Y 30
#define UT_T4_MUTE_H 60
#define UT_T4_LEV_Y 93          /* LEVEL cards (K1, K2) */
#define UT_T4_PAN_Y 155         /* PAN cards (K3, K4) */
#define UT_T4_H 59
#define UT_T4_PAD 3             /* the dot's, the text's and the bar's left edge; the bar keeps as much at the right */
#define UT_T4_LBL_BASE 15       /* rel: the label's and the dot's baseline (also MUTE n) */
#define UT_T4_LBL_DX 14         /* label pen = dot x + this */
#define UT_T4_VAL_BASE 37       /* rel */
#define UT_T4_UNIT_GAP 0
#define UT_T4_BAR_Y 51          /* rel; height 3 */
#define UT_T4_BAR_H 3
#define UT_T4_BAR_R 1
#define UT_T4_BAR_MIN 3
#define UT_T4_TICK_DY 1         /* PAN's middle tick starts this many rows above the bar and ends this many under it */
#define UT_T4_TRACK_PCT 13      /* a bar's and the meter's empty track: card -> TEXT, % */
#define UT_T4_HOLD_BASE 32      /* rel (MUTE card): "Hold" and its GLO keycap */
#define UT_T4_PRESS_BASE 48     /* rel (MUTE card): "and press" and its OCT- / OCT+ keycap */
#define UT_T4_KEY_GAP 3         /* the word -> its keycap */
#define UT_T4_METER_X 106       /* rel (MUTE card) */
#define UT_T4_METER_Y 8
#define UT_T4_METER_W 6
#define UT_T4_METER_H 44
#define UT_T4_METER_R 3

/* ---- TEMPLATE 5, the SENDS page (the 'fxsum' page, FX's last; item 117; FINAL_V1_04_fx-send): no card, no box, no bar, everything on the background, left aligned; screen rows (the capitals' last row = the baseline) ----
 * The state text (M, TEXT-MID colour) at x 9, baseline 48; two info rows, labels (S, grey) at x 9 and 148, values (M, the part's colour; dim when that effect is not fed) at x 66 and 186, baselines 92 (DELAY: mode, TIME)
 * and 124 (REVERB: type, SIZE); a note (S, grey) in two lines, baselines 160 and 177, x 9; at the bottom the three knob labels (S) on baseline 206, each with its white numbered dot (11 px) 5 px before it: dots at x 9 / 88 / 167,
 * label pen = dot + 16. No chorus info line (the chorus has no setting on this page: its send is the FX page's). */
/* MODULES (item 124): four lines, K1 .. K4, each the numbered dot, the name (S, grey) and the state (M, the part's colour) on ONE baseline; lines 40 px apart (rows 27 .. 216 hold them with air) */
#define UT_MODS_BASE0 62        /* the first line's capitals' last row */
#define UT_MODS_PITCH 40
#define UT_MODS_DOT_X 9
#define UT_MODS_LBL_X 25        /* the name, after the dot */
#define UT_MODS_VAL_X 108       /* the state: the longest, KEEPS RUNNING, ends before x 236 */
/* A page of a function that GLO > MODULES has switched OFF (item 125): the body is only this notice, top left on the background, no card, no dot, no box; the title, capsules and footer are the page's own.
 * The message (M, TEXT-MID as the SENDS page's state line) at x 9, its capitals' last row on UT_OFF_MSG_BASE; under it the note (S, grey as the SENDS note) from UT_OFF_NOTE_GAP rows lower, UT_OFF_NOTE_PITCH apart;
 * every line wider than UT_OFF_W wraps at a space, left aligned. The canvas starts at row UT_OFF_Y0 and is UT_OFF_H high (the 124-row limit of the strips). A twist of any knob of such a page jumps to MODULES. */
#define UT_OFF_X 9
#define UT_OFF_Y0 27
#define UT_OFF_H 100
#define UT_OFF_MSG_BASE 48      /* the SENDS page's state line baseline */
#define UT_OFF_MSG_PITCH 20     /* a message that wraps */
#define UT_OFF_NOTE_GAP 26      /* the message's last baseline -> the note's first */
#define UT_OFF_NOTE_PITCH 17    /* the SENDS note's pitch (160, 177) */
#define UT_OFF_W 222            /* the longest line: x 9 .. 231 */
#define UT_OFF_TXT_ARP "THE ARPEGGIATOR IS SWITCHED OFF"
#define UT_OFF_TXT_SEQ "THE SEQUENCER IS SWITCHED OFF"
#define UT_OFF_TXT_SENDS "THE EFFECT SENDS ARE SWITCHED OFF"
#define UT_OFF_TXT_PFX "THE PART EFFECTS ARE SWITCHED OFF"
#define UT_OFF_NOTE_IT "TURN A KNOB TO SWITCH IT ON IN MODULES"      /* ARP, SEQ */
#define UT_OFF_NOTE_THEM "TURN A KNOB TO SWITCH THEM ON IN MODULES"  /* SENDS, PART FX */
#define UT_T5_X 9               /* state text, row labels and note */
#define UT_T5_STATE_BASE 48
#define UT_T5_ROW1_BASE 92
#define UT_T5_ROW2_BASE 124
#define UT_T5_VAL_X 66          /* the first value of a row */
#define UT_T5_LBL2_X 148        /* the second label of a row */
#define UT_T5_VAL2_X 186
#define UT_T5_NOTE_BASE1 160
#define UT_T5_NOTE_BASE2 177
#define UT_T5_KNOB_BASE 206
#define UT_T5_KNOB_X0 9         /* the dots of K1 .. K3 */
#define UT_T5_KNOB_DX 79
#define UT_T5_KNOB_LBL_DX 16
/* the grey of the small labels, units and notes of templates 4 and 5: BG (or the card) -> TEXT, % (VIOLET on the card 139,134,143 / on BG 139,132,139, against the pictures' 138,135,145 / 139,133,148) */
#define UT_GREY_PCT 53

/* ---- TEMPLATE 6, the option pages: the first-start SETUP questions (FINAL_V1_08_setup, item 118) and the MODE list in the same selection style ----
 * Measured on FINAL_V1_08_setup.png (240 x 240) and final_pages.py page_setup. Title SETUP (no counter: the footer carries 1/5), the question (M, white) at x 9 on baseline 39, three big option blocks, a legend row, the shared footer.
 * Every block has fixed rows whichever option is picked (only the box moves): the box = a hard-cornered card x 3 .. 236 filled with the SELECTED PART'S colour over the background at UT_T6_SEL_PCT % (VIOLET 66,15,75);
 * the name (L) on its baseline, its explanation lines (S, grey; lilac on the picked box) from baseline DESC1, 15 px apart; a pill (RECOMMENDED, DEFAULT) after the name: the part's colour, the name's capital rows high.
 * Rows: box top, box height, name baseline, first explanation baseline (SETUP 1/5: LAYER, STEREO, DUEL as the picture; SETUP 5/5 the same rules for LATER, NOW). */
#define UT_T6_X 9               /* the question's, the names' and the explanations' left edge */
#define UT_T6_Q_BASE 39
#define UT_T6_BOX_X 3
#define UT_T6_BOX_W 234
#define UT_T6_SEL_PCT 27        /* BG -> the selected part's colour, % (the capsule's dim half: VIOLET 66,15,75) */
#define UT_T6_DESC_SEL_PCT 65   /* the picked box's explanation: its fill -> TEXT, % (VIOLET 190,165,200 in the picture) */
#define UT_T6_LINE 15           /* explanation line pitch */
#define UT_T6_PILL_GAP 10       /* name -> its pill */
#define UT_T6_PILL_PAD 7        /* pill text -> pill edge */
#define UT_T6_PILL_R 8
#define UT_T6_LEG_Y 197         /* the legend row's keycap top: the keycap (13 rows) is centred on the picture's rows 196 .. 210 */
#define UT_T6_LEG_BASE 208      /* the legend words' capitals' last row */
#define UT_T6_LEG_X1 9          /* the first legend item's left edge */
#define UT_T6_LEG_X2 118        /* the second item's, when there are two (as the picture); three: shared evenly, the last flush right at x 231 */
#define UT_T6_LEG_GAP 5         /* keycap -> its word */
#define UT_T6_LEG_R 231
#define UT_T6_LEG_MIN 6         /* the least room between two legend items of a three-item row */
#define UT_T6_END 217           /* the body (guide pages) ends here; the shared footer's rows start (draw_foot short form) */
/* the GUIDE's information pages (2 .. 4 of 5; MENU > GUIDE 1 .. 3 of 3): headings L (the guide's heading face) on baselines, the sentences M under them with keycaps in the line, everything left aligned at UT_T6_X; the rows of the
 * tallest page (3 headings, 6 lines) are packed so that the last line ends above the legend row */
#define UT_T6_G_HEAD0 42        /* the first heading's baseline (capitals' last row) */
#define UT_T6_G_HT 16           /* heading baseline -> its first sentence line's baseline */
#define UT_T6_G_LINE 16         /* sentence line pitch */
#define UT_T6_G_TH 26           /* a sentence's last baseline -> the next heading's baseline */
/* MODE (live page): its list rows keep their place (ui_graph.c graph_mode); the picked row takes the same fill and corners */

/* ---- TEMPLATE 7, the STEP sequencer page (FINAL_V1_07_sequencer, item 118; final_pages.py page_step / roll_draw2) and its help page (FINAL_V1_07b_sequencer_help) ----
 * The roll card: x 3 .. 236, rows 27 .. 173 (hard corners). Inside: 17 semitone rows of UT_T7_RH px from row UT_T7_Y0 (the highest on top), the step columns from x UT_T7_X0, 12 px each, the keyboard strip at UT_T7_KX
 * (white keys 8 px, black keys 4 px, one row less than the row's height); the C, E and G rows named at the strip's left (S, T_DIM, ink ending at x UT_T7_NAME_R, capitals' last row = the row's top + RH);
 * a note is 10 px wide (x + 1) and RH - 2 high, an accent the row's whole height in TEXT; the cursor frame 1 px, 2 rows over the first row and 1 under the last; a lock mark 7 x 2 under the step.
 * Four cards under it, rows 177 .. 214: widths 51 / 53 / 69 and the rest to x 235, 2 px apart; each: the numbered dot at x +3, the label (S) at +17 on baseline rel 15, the value (L, else M, else S) at +5 on baseline rel 35 with its
 * unit (M) on the same baseline. No legend on the roll page; the footer is the shared one (no step strip). */
#define UT_T7_X 3
#define UT_T7_Y 27
#define UT_T7_W 234
#define UT_T7_H 147
#define UT_T7_ROWS 17
#define UT_T7_RH 8
#define UT_T7_Y0 31
#define UT_T7_X0 39
#define UT_T7_CW 12
#define UT_T7_KX 28
#define UT_T7_KW_WHITE 8
#define UT_T7_KW_BLACK 4
#define UT_T7_NAME_R 27
#define UT_T7_NOTE_X 1          /* a note starts this far into its column */
#define UT_T7_NOTE_W 10
#define UT_T7_NOTE_PCT 47       /* the notes (not the cursor step's): BG -> the part colour, % (VIOLET 106,40,131 in the picture) */
#define UT_T7_LOCK_DY 3         /* the lock mark's top, under the last row */
#define UT_T7_KEYW_PCT 23       /* the roll's greys, SURF -> TEXT, %: a white key and the C rows' / beats' line (VIOLET 66,58,76 as the picture) */
#define UT_T7_KEYB_PCT 13       /* a black key (VIOLET 41,36,49) */
#define UT_T7_LANE_PCT 13       /* an in-scale row, SURF -> the part's colour, % (VIOLET 36,11,47; the picture 36,20,52) */
#define UT_T7_CARD_Y 177
#define UT_T7_CARD_H 38
#define UT_T7_CARD_GAP 2
#define UT_T7_CARD_W0 51        /* widths of cards 1 .. 3; the fourth takes the rest up to UT_T7_CARD_R */
#define UT_T7_CARD_W1 53
#define UT_T7_CARD_W2 69
#define UT_T7_CARD_R 236
#define UT_T7_DOT_X 3
#define UT_T7_LBL_DX 17
#define UT_T7_LBL_BASE 15       /* rel */
#define UT_T7_VAL_X 5
#define UT_T7_VAL_BASE 35       /* rel: as the list heads (UT_T3_VAL_BASE - UT_T3_HEAD_Y) */
#define UT_T7_VAL_PAD_R 2
/* the help page: eight rows 23 px apart from row 33 (the legend symbol at x 9 centred on the row's cap rows, the word S at x 30, its meaning M grey at x 86) */
#define UT_T7_HELP_Y0 33
#define UT_T7_HELP_PITCH 23
#define UT_T7_HELP_SYM_X 9
#define UT_T7_HELP_WORD_X 30
#define UT_T7_HELP_MEAN_X 86

/* ---- TEMPLATE 8, the CELL pages (item 156; owner-approved design_grid_2x2_REVERB.png, design_fill_2_and_3.png, design_choice_bars.png in DUELL/release_drafts) ----
 * The pages whose picture was only the calm curve (graph GR_NONE) have no picture: the cards that show fill the body, rows 27 .. 215, as big cells with template 2's rows (dot + label S on rel 19,
 * the value on rel 58: figures N, else L, else M, else S, cut by the cell's edge, never ellipsised; unit M 5 px after it; the bar rel 75 .. 77, x 6 .. width - 6). By the number of cards that show:
 * 4: a 2 x 2 grid of 116 x 92 (x 3 / 121, y 27 / 124); 3: one wide cell (234 x 92) on top, two cells under it; 2: two wide rows; 1: one wide cell on top; 0: nothing.
 * A CHOICE (a list of N options, not a value: TYPE, MODE, SOURCE, a note division, CURVE, PRIORITY, a switch) has a SEGMENT bar: the value bar's track, a segment 1/N of it at the option's place in the
 * knob's order (a switch: half the track, left or right); a list of one option has none */
#define UT_T8_X0 3
#define UT_T8_DX 118
#define UT_T8_W 116
#define UT_T8_WIDE 234
#define UT_T8_Y0 27
#define UT_T8_DY 97
#define UT_T8_H 92              /* rows 27 .. 118 and 124 .. 215 */
#define UT_T8_PAD UT_T2_PAD
#define UT_T8_LBL_BASE UT_T2_LBL_BASE
#define UT_T8_LBL_DX UT_T2_LBL_DX
#define UT_T8_LBL_PAD_R UT_T2_LBL_PAD_R
#define UT_T8_VAL_BASE UT_T2_VAL_BASE
#define UT_T8_VAL2_BASE1 UT_T2_VAL2_BASE1
#define UT_T8_VAL2_BASE2 UT_T2_VAL2_BASE2
#define UT_T8_UNIT_GAP UT_T2_UNIT_GAP
#define UT_T8_BAR_Y UT_T2_BAR_Y
/* item 165 (owner): the ENGINE page of LOFI and NOISE has no picture (grid_on 15): the K1 row as on every ENGINE page (template 2, rows 27 .. 50), the cards that show as template 8's cells
 * (the same x, widths, rows inside a cell, faces and segment bars) in rows 54 .. 215: two rows of 79, 4 rows apart (54 .. 132, 137 .. 215). By the cards that show: 3 = one wide on top and two
 * under it, 2 = two wide rows, 1 = one wide on top. While K1 turns, the engine list (template 2's rows) takes the top cell's place */
#define UT_T8E_Y0 UT_T2_GR_Y
#define UT_T8E_H 79
#define UT_T8E_DY 83

/* ---- TEMPLATE 9, the AREA page (grid_on 16; LAYOUT_IMPLEMENTATION_PLAN.md 2026-10-29 block A; the approved pictures NEW_LAYOUT_SHEET_V2, SCALE_CHORD_NEW_LAYOUT_V3, CHORD_NEW_LAYOUT_V5,
 * PERFORM_NEW_LAYOUT_V4, QUICK_SEQ_NEW_LAYOUT, PATCHES_MULTIS_NEW_LAYOUT_V3, PATTERNS_NEW_LAYOUT; 240 x 240, 1 px = 1 px, rows measured by counting lit pixel rows, about 1 px error; screen rows unless "rel" = inside its card) ----
 * Three parts under the header: the LINE for knob 1 (no card: the numbered dot, the label, the value, its unit and an optional text after it, all on one baseline, left aligned), the big AREA (the page's picture code
 * draws into it: ui_draw.c area_rect), and ONE card row (owner default: the LOWER row of the pictures, label 171, value 197, line 205 .. 207, on every area page and on the ENGINE page) of 1 to 3 cards in
 * 3 columns of 76 (x 3, 82, 161) or 2 columns (x 3, 118: the dots of the pictures at x 9 and 124, the lines 8 .. 113 and 123 .. 228). The card: the dot + the label (S) on one baseline, the value (figures N,
 * else L, else the Bold T; cut by the card's edge, never ellipsised), the unit: % (and every unit but "/16") in M, white, on the value's baseline with NO space before it; "/16" in the value's face, white,
 * no space. A CHOICE card shows the segment line (1/N of the line at the option's place); an action card (LOAD, SAVE) shows the OCT+ key pill, its last row UT_A_PILL_BASE.
 * Two-line labels (FIRMWARE_CHANGE_LIST_2026-10-28.md section C): on a page where a card label needs two lines, every card of the page starts UT_A_LBL2_PITCH rows higher, the two-line label's first line
 * (with the dot) sits one line above the shared baseline, one-line labels stay on the baseline; the values and lines do not move; the area ends that much higher */
#define UT_A_K1_Y 27            /* the knob-1 line's canvas: rows 27 .. 47 (its text rows 33 .. 43) */
#define UT_A_K1_H 21
#define UT_A_K1_BASE 43         /* the capitals' last row of the dot, the label, the value, the unit and the text after it */
#define UT_A_K1_DOT_X 9
#define UT_A_K1_LBL_X 24        /* the label's pen (dot + 15) */
#define UT_A_K1_VAL_GAP 9       /* label pen end -> the value's pen (the pictures: about 10 px of ink between them) */
#define UT_A_K1_AFTER_GAP 15    /* value (or unit) pen end -> the text after it (chord name; SELECT = BALANCE: 16 to 19 px of ink) */
#define UT_A_K1_R 231           /* a right-aligned text after the value (EXAMPLE: C E G) ends here (its ink at x 230) */
#define UT_A_GR_Y 48            /* the big area: rows 48 .. 149 (holds the measured pictures: ARP 2/3 pattern 68 .. 127, roll 52 .. 143, keyboard 56 .. 130, PERFORM boxes 48 .. 149, QUICK SEQ boxes 49 .. 130, lists 50 .. 145) */
#define UT_A_GR_H 102
#define UT_A_CARD_Y 150         /* the card row: rows 150 .. 208 (a page with a two-line label: from row 150 - UT_A_LBL2_PITCH) */
#define UT_A_CARD_H 59
#define UT_A_X0 3
#define UT_A_DX3 79             /* three columns at x 3, 82, 161, 76 wide (the ENGINE page's cells) */
#define UT_A_W3 76
#define UT_A_DX2 115            /* two columns at x 3 and 118 (measured: dots at 9 / 124, lines 8 .. 113 / 123 .. 228) */
#define UT_A_W2 118             /* (the measured line length 106 = 118 - 2 x 6; the two cards share 3 columns of padding, x 118 .. 120) */
#define UT_A_PAD 6              /* the dot's, the value's, the line's and the pill's left edge; the room kept free at the right */
#define UT_A_LBL_BASE 21        /* rel: row 171 */
#define UT_A_LBL_DX 15          /* label pen = dot x + this (the pictures: dot 9, label 24) */
#define UT_A_LBL_PAD_R 4
#define UT_A_LBL2_PITCH 13      /* a two-line label: its first line's baseline this many rows above the second's (two_line_labels.png: 152 / 165) */
#define UT_A_VAL_BASE 47        /* rel: row 197 */
#define UT_A_VAL2_BASE1 32      /* rel: a value of two lines (S): rows 182 and 197 (template 8's 15 rows apart), no line */
#define UT_A_VAL2_BASE2 47
#define UT_A_UNIT_GAP 0         /* value -> % (owner rule A.2: no space) and value -> "/16" */
#define UT_A_UNIT_GAP_W UT_T2_UNIT_GAP   /* value -> any other unit (ms, st, LP, HP, +2): as the ENGINE cells today */
#define UT_A_BAR_Y 55           /* rel: rows 205 .. 207 */
#define UT_A_PILL_BASE 44       /* rel: the key pill's last row, row 194 (MULTIS LOAD / SAVE: rows 182 .. 194) */
/* the measured parts of the area's pictures (for the picture code; screen rows): */
#define UT_A_ROLL_Y 52          /* SEQUENCER roll rows 52 .. 143 */
#define UT_A_ROLL_H 92
#define UT_A_KB_Y 56            /* GLOBAL SCALE / CHORD keyboard rows 56 .. 130, x 10 .. 230 */
#define UT_A_KB_H 75
#define UT_A_KB_X 10
#define UT_A_KB_W 221
#define UT_BOX_X0 9             /* PERFORM and QUICK SEQ boxes: x 9, 65, 121, 177, 54 wide */
#define UT_BOX_DX 56
#define UT_BOX_W 54
#define UT_BOX_PF_Y0 48         /* PERFORM: 4 rows 24 high, 26 apart (48 .. 71, 74 .. 97, 100 .. 123, 126 .. 149) */
#define UT_BOX_PF_H 24
#define UT_BOX_PF_DY 26
#define UT_BOX_QS_Y0 49         /* QUICK SEQ: 2 rows 38 high, 44 apart (49 .. 86, 93 .. 130) */
#define UT_BOX_QS_H 38
#define UT_BOX_QS_DY 44
#define UT_L5_Y0 50             /* PATCHES / MULTIS / PATTERNS: five rows from row 50, 20 apart, 19 high (MULTIS M01 filled 50 .. 68), x 9 .. 230 */
#define UT_L5_PITCH 20
#define UT_L5_H 19
#define UT_L5_ROWS 5
#define UT_L5_X 9
#define UT_L5_W 222
#define UT_L5_TEXT_BASE 15      /* rel to the row's top: text rows 55 .. 65 */
#define UT_L5_TAG_X 14          /* the number (#5, M01, P01) */
#define UT_L5_NAME_X 52
#define UT_L5_STAR_X 214

/* ---- developer bench build only (DUELL_BENCH; HANDOFF_V2 item 112): the title face T (9,555 B) is mapped to the existing AF_M (15 px wide cells, same cap rows 4 .. 14); its table is then an
 * unused static and the compiler drops it. (Item 113: the face F is gone, the footer and capsule text are S in every build.) A normal build and a release keep T exactly (this block is not
 * compiled there). The bench screen's look is not a deliverable; its timing is the same (the draw runs in the main loop, never in the audio interrupt). ---- */
#if defined(DUELL_BENCH) && DUELL_BENCH
#define AF_T AF_M
#undef AF_T_CAP_Y
#undef AF_T_CAP_H
#define AF_T_CAP_Y AF_M_CAP_Y
#define AF_T_CAP_H AF_M_CAP_H
/* the names role (face P, 5,403 B) is mapped to the existing AF_L (Bold 23, capitals rows 6 .. 22: the box names' larger face before face P existed); face P's table is then an unused
 * static and the compiler drops it. A name that does not fit its box in L falls back to the box's smaller face as in every build; the TOOLS names are wider than in P */
#undef AF_NAME
#undef AF_NAME_CAP_Y
#undef AF_NAME_CAP_H
#define AF_NAME AF_L
#define AF_NAME_CAP_Y AF_L_CAP_Y
#define AF_NAME_CAP_H AF_L_CAP_H
#endif
