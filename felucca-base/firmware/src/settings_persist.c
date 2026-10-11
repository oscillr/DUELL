/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared PER4 layout: each feature updates only its own fields, preserving
 * the other feature's saved preferences when either is built independently.
 * PER1/PER2 are upstream; PER3 added bold; PER4 added favorites.
 * palette: UI_PAL_TAG + index; an old id (below 20, earlier firmware) is migrated on import.
 * bold: no longer used (one font weight); kept as it was saved, unless it holds the HOLD setting (panel.c).
 * zoom: no longer used (the large readout); kept as it was saved, unless it holds the LEDS setting (panel.c).
 * favorites.factory[15][28..31]: CLICK / CLICK LEVEL / COUNT-IN (1.1), STYLE, MENU's flags, the layers seen (bytes no
 * engine uses; 0 in older settings = every one's default); [15][27]: SCREEN OFF (1.1.5; 0 = 30 MIN, the default);
 * [15][26]: MENU > DISPLAY > HOME (1.3; 0 = SCOPE, 1 TRACKS, 2 LEVELS (1.5), ui.c ui_home_view);
 * [15][24]: MENU's 1.2 flags SCOPE MIX, STEP PREVIEW ON, CHORD ENTRY ADD (bits 0-2, ui.c ui_prefs2; 0 = the defaults);
 * [15][10]: MENU > ANIM IDLE (1.2, Discussion #135, ui.c ui_idle: bit 0; 0 = off, ANIM as its bit in MENU's flags says);
 * [15][12..13] (DUELL 4b): the MORPH's TARGET (bit 7 of [12]) and each part's AMOUNT (bits 0-6: 1 + %, 0 = the default, 15 %: 25 % before 2026-10-11, the
 * default; ui.c ui_morph_set, ui_input.c morph_settings: read on any change, so an import needs nothing here);
 * [15][11]: PATCHES > TYPE (1.2, Discussion #90; DUELL: 1..CAT_N-1 a category, CAT_N + e only the engine e's patches; ui.c ui_lcat:
 * 0 none = ALL / FAV as favorites.filter says);
 * [15][14..23]: the FX layer's key map (1.2, ui_layer.c fx_keys: 16 white keys x 5 bits, perform.c perf_map_of; 0 = the
 * key's default, an unknown code too: kept as saved, a later firmware's effect survives a round trip here).
 * [15][25] (DUELL step 3): 1 = the MIDI LEARN map in [14] names DUELL's ids. 0 (every older setting) = Felucca's: its
 * entries on an id DUELL took (core.h PL_*: DIGITAL's OP ENV, DRUM's lane levels, ED FX) are dropped once on import, so
 * an old learned CC never moves a DUELL parameter; every export writes 1.
 * MENU's flags (ui.c PREF_*) are append-only bits whose 0 is the default: bit 16 (BPM LOCK, retired in DUELL: SELECT is no tempo) stays clear, LARGE (32) = OFF, RESTORE LAST OFF (128, 1.2) = ON, read as saved (nothing to migrate, so importing twice changes nothing). */
typedef struct {
    uint32_t magic, palette, lowcut, zoom;
    panel_t panel;
    uint32_t bold;
    struct { uint8_t factory[16][32]; uint32_t user, filter, user_hi[3]; } favorites;   /* (= favorites.c favorites_t) */
} persist_t;
#define PERSIST_MAGIC 0x50455234u

/* Normalize in place; 1 = current, 2 = migrated, 0 = invalid. */
static int settings_import(persist_t *p, int n)
{
    /* DUELL: favorites.user_hi (patches 32..127) was appended; settings saved before it (12 bytes shorter) read as current, those bits clear */
    int cur1 = n == (int)(sizeof *p - sizeof p->favorites.user_hi) && p->magic == PERSIST_MAGIC;
    int current = n == (int)sizeof *p && p->magic == PERSIST_MAGIC;
    int old3 = n == (int)(sizeof *p - sizeof p->favorites) && p->magic == 0x50455233u;
    int old2 = n == (int)(sizeof *p - sizeof p->favorites - sizeof p->bold) && p->magic == 0x50455232u;
    int old1 = n == (int)(8u + sizeof(panel_t)) && p->magic == 0x50455231u;
    if (!(current || cur1 || old3 || old2 || old1)) return 0;
    if (cur1) {
        memset(p->favorites.user_hi, 0, sizeof p->favorites.user_hi);
        current = 1;
    }
    if (old1) {
        panel_t old;
        memcpy(&old, (uint8_t *)p + 8, sizeof old);
        p->panel = old;
        p->lowcut = p->zoom = 0;
    }
    if (old1 || old2) p->bold = 0;
    if (!current) memset(&p->favorites, 0, sizeof p->favorites);
    if (p->favorites.factory[15][29] > 1u)        /* STYLE (ui.c ui_style): 2, the retired PIXEL = LINE; unknown = FLAT */
        p->favorites.factory[15][29] = p->favorites.factory[15][29] == 2u ? 1u : 0u;
    {   /* CLICK, CLICK LEVEL, COUNT-IN (ui.c ui_rec_prefs): two bits each, 3 is no value: the default (0) */
        uint32_t b = p->favorites.factory[15][28], f;
        for (f = 0; f < 3u; f++)
            if (((b >> (2u * f)) & 3u) == 3u)
                b &= ~(3u << (2u * f));
        p->favorites.factory[15][28] = (uint8_t)b;
    }
    if (p->favorites.factory[15][27] > 7u)         /* SCREEN OFF (1.1.5, ui.c SCR_CODE): unknown = 0 (NEVER since 1.1.5.1) */
        p->favorites.factory[15][27] = 0;
    if (p->favorites.factory[15][11] > CAT_N + 14u) /* PATCHES TYPE (ui.c ui_lcat): a category CAT_*, or CAT_N + an engine; unknown = none */
        p->favorites.factory[15][11] = 0;
    /* [15][26] HOME (1.3, ui.c ui_home_view): kept as saved; a value this firmware does not know draws SCOPE */
#ifdef PL_FM1_ATK
    if (p->favorites.factory[15][25] != 1u) {      /* (DUELL step 3) a MIDI LEARN map of Felucca's ids: see the top */
        uint8_t *ml = p->favorites.factory[14];
        uint32_t i;
        for (i = 0; i < 16u; i++) {                /* (midi_control.c ML_N entries: bits 9..15 the code, common ids id + 1) */
            uint32_t e = ml[2u * i] | (uint32_t)ml[2u * i + 1u] << 8, code = e >> 9;
            if (code && code - 1u < P_E0 && param_retired(code - 1u))
                ml[2u * i] = ml[2u * i + 1u] = 0;
        }
        p->favorites.factory[15][25] = 1u;
    }
#endif
    p->magic = PERSIST_MAGIC;
    p->palette = palette_to_stored(palette_from_stored(p->palette));
    settings.magic = SETTINGS_MAGIC;
    settings.palette = palette_from_stored(p->palette);
    settings.lowcut = p->lowcut;
    settings.zoom = p->zoom;
    settings_hold = (uint8_t)hold_from_stored(p->bold);
    settings_leds = (uint8_t)leds_from_stored(p->zoom);
#ifdef FELUCCA_FAVORITES
    memcpy(&favorites, &p->favorites, sizeof favorites);
    favorites.filter = favorites.filter == 1u;
#if defined(FM4_NPRESETS) && !FELUCCA_FM4
    {   /* DIGITAL's starred presets (engine 1, retired) -> the FM6 presets that cover them (fm4_convert.c) */
        uint32_t k;
        for (k = 0; k < FM4_NPRESETS; k++)
            if ((favorites.factory[ENGI_DIGITAL][0] >> k) & 1u)
                favorites.factory[ENGI_FM6][FM4_TO_FM6[k] / 8u] |= (uint8_t)(1u << (FM4_TO_FM6[k] % 8u));
        memset(favorites.factory[ENGI_DIGITAL], 0, sizeof favorites.factory[ENGI_DIGITAL]);
    }
#endif
#ifdef SMP_SET_PERC
    if ((favorites.factory[ENGI_SAMPLE][0] >> SMP_SET_PERC) & 1u) {   /* SAMPLE's PERC (preset 4, retired): DRUM's */
        favorites.factory[ENGI_DRUM][0] |= 1u;                         /* kit (core.h drum_from_perc) */
        favorites.factory[ENGI_SAMPLE][0] &= (uint8_t)~(1u << SMP_SET_PERC);
    }
#endif
#endif
    if (p->panel.magic == PANEL_MAGIC) panel = p->panel;
    return current ? 1 : 2;
}

/* Start with the last imported/saved object, including fields owned by a
 * feature absent from this build. No save-on-boot or extra flash writes. */
static void settings_export(persist_t *p)
{
    p->magic = PERSIST_MAGIC;
    p->palette = palette_to_stored(settings.palette);
    p->lowcut = settings.lowcut;
    p->zoom = settings.zoom = leds_to_stored(settings.zoom, settings_leds);
    p->panel = panel;
    p->bold = hold_to_stored(p->bold, settings_hold);
#ifdef FELUCCA_FAVORITES
#ifdef PL_FM1_ATK
    favorites.factory[15][25] = 1u;               /* (DUELL step 3: the MIDI LEARN map names DUELL's ids; RAM and the
                                                   * saved copy alike, so an unchanged setting compares equal) */
#endif
    memcpy(&p->favorites, &favorites, sizeof favorites);
#endif
}
