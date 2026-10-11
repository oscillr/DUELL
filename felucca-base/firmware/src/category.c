/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Sound categories (1.2, Discussion #90): every sound in the PATCHES list belongs to one, so KNOB 2 (TYPE) can show
 * only the basses, the pads, .. (ui.c list_mode). Factory presets: a static table here (a letter per preset, in the
 * engine's preset order); user presets: a byte of their record (upreset.c up_cat, set on the NAME screen when saving
 * or renaming, ui_name.c). Included by ui.c and upreset.c (the host build of the user presets needs the numbers only).
 * The numbers are stored (a user preset's record, the editor's UP_LIST / UP_GET / UP_PUT): append-only. CAT_NONE is a
 * record of before (or the editor's without one): its category follows from its engine (cat_of_engine) */
#ifndef FELUCCA_CATEGORY
#define FELUCCA_CATEGORY 1
enum { CAT_NONE, CAT_BASS, CAT_LEAD, CAT_PAD, CAT_PLUCK, CAT_KEYS, CAT_PERC, CAT_FX, CAT_OTHER, CAT_N };
#define CAT_DRUM CAT_PERC                         /* (its name before DUELL: the same stored number 6) */
static const char *const CAT_NAME[CAT_N] = {"-", "BASS", "LEAD", "PAD", "PLUCK", "KEYS", "PERC", "FX", "OTHER"};
/* the category a sound without one is listed under: a kit (DRUM, SLICE: engines 10, 13) PERC, any other OTHER */
static uint32_t cat_of_engine(uint32_t e) { return e == 10u || e == 13u ? CAT_PERC : CAT_OTHER; }

#ifndef UP_HOST
/* a factory sound's category (DUELL: sounds.c, the generator's from tools/duell_sounds.py); one without (an engine's INIT):
 * its engine's (cat_of_engine). A hint for browsing: nothing else reads it */
static uint32_t preset_cat(uint32_t e, uint32_t k)    /* factory sound k of engine e */
{
    uint32_t c = ds_cat_of(e, k);
    return c && c < CAT_N ? c : cat_of_engine(e);
}
#endif
#endif
