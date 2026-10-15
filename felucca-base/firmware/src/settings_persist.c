/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared PER4 layout: each feature updates only its own fields, preserving
 * the other feature's saved preferences when either is built independently.
 * DUELL 0.2.14: only the current record (PER4, exactly sizeof persist_t) is read. PER1 .. PER3 (upstream's, bold, favorites) and a PER4
 * without favorites.user_hi (before the PATCHES page) are no record: settings_import returns 0 and the unit starts as a new one (the
 * defaults, the first-start GUIDE, the panel table learned again).
 * palette: UI_PAL_TAG + index; an id below 20 (earlier firmware) is read through palette_from_stored's table.
 * bold: no longer used (one font weight); kept as it was saved, unless it holds the HOLD setting (panel.c).
 * zoom: no longer used (the large readout); kept as it was saved, unless it holds the LEDS setting (panel.c).
 * favorites.factory[15][28..31]: CLICK / CLICK LEVEL / COUNT-IN (1.1), STYLE, MENU's flags, the layers seen (bytes no
 * engine uses; 0 in older settings = every one's default); [15][27]: SCREEN OFF (1.1.5; 0 = 30 MIN, the default);
 * [15][26]: MENU > DISPLAY > HOME (1.3; 0 = SCOPE, 1 TRACKS, 2 LEVELS (1.5)); read by nothing since 0.2.14 (the OVERVIEW screen is gone), kept as saved;
 * [15][24]: MENU's 1.2 flags SCOPE MIX (retired in 0.2.14), STEP PREVIEW ON, CHORD ENTRY ADD (0.2.14: STEP KEYS ADD) (bits 0-2, ui.c ui_prefs2; 0 = the
 * defaults); bit 7 (0.2.14): the 0.2.14 defaults applied (settings_defaults);
 * [15][10]: MENU > ANIM IDLE (1.2, Discussion #135, ui.c ui_idle: bit 0; 0 = off, ANIM as its bit in MENU's flags says); bits 1..4 (DUELL 0.2.14):
 *   GLO > MODULES' ARP SEQ SENDS PART FX, a set bit = OFF (ui.c mods_*; every value valid, nothing to check); bit 5 (DUELL 0.2.14) MENU > CONTROL > PERF LEAVE, set = KEEP (clear = FADE, the default; ui.c perf_keep); bits 6..7 free;
 * [15][12..13] (DUELL 4b): the MORPH's TARGET (bit 7 of [12]; 0.2.9: reserved, kept as stored, never read) and each part's AMOUNT (bits 0-6: 1 + %, 0 = the default, 15 %: 25 % before 2026-10-11, the
 * default; ui.c ui_morph_set, ui_input.c morph_settings: read on any change, so an import needs nothing here);
 * [15][11]: PATCHES > TYPE (1.2, Discussion #90; DUELL: 1..CAT_N-1 a category, CAT_N + e only the engine e's patches; ui.c ui_lcat:
 * 0 none = ALL / FAV as favorites.filter says);
 * [15][14..23]: the FX layer's key map up to 0.2.1 (16 white keys x 5 bits); the layer is removed in DUELL 0.2.2: not read or written any
 * more (an older record's bytes are carried through unchanged), never reused.
 * [15][29]: the retired STYLE byte (nothing reads it; BASS MONO is not in the settings: DUELL 0.2.19 keeps it with the music, song.bm, project.c / multi.c).
 * [15][25] (DUELL step 3): 1 = the MIDI LEARN map in [14] names DUELL's ids; every export writes 1 (firmware up to 0.2.13 reads it;
 * 0.2.14 no longer converts a map of Felucca's ids: nothing reads the byte).
 * MENU's flags (ui.c PREF_*) are append-only bits whose 0 is the default: bit 16 (BPM LOCK, retired in DUELL: SELECT is no tempo) stays clear, the retired LARGE (32) is ignored, RESTORE LAST OFF (128, 1.2) = ON, read as saved (nothing to migrate, so importing twice changes nothing; DUELL 0.2.14: but for settings_defaults, once, below). */
/* DUELL 0.2.14: new defaults (owner): KNOB ACCEL ON, CLICK REC, USB SERIAL OFF. A bit's 0 is the old default in every one of them
 * ([15][30] bit 2 clear = ACCEL OFF, [15][28] bits 0-1 = 0 CLICK OFF, [15][30] bit 6 clear = USB SERIAL ON), and a value the user chose
 * cannot be told from the untouched default (same bits). So the change is made ONCE, while the flag [15][24] bit 7 (ui.c PREF_DEFAULTS)
 * is clear, on the record's bytes: KNOB ACCEL set ON; CLICK set REC where it holds OFF (REC or ON are kept); USB SERIAL set OFF;
 * [15][10] bits 1-7 cleared (left 0 by every writer so far; room for 0.2.14's next settings, which then start from 0: GLO > MODULES' four
 * switches, bits 1..4, all ON); the flag set.
 * After that the user's choice sticks (the flag stays set). Run by settings_import on every record it accepts (an older record or
 * an editor backup of one gets the new defaults; a 0.2.14 record is left as it is: importing twice changes nothing) and by
 * project.c persist_boot when there is no valid record (a new unit, ERASE DATA, no flash): there the bytes are 0, the old defaults,
 * so the same change gives the new defaults. Downgrade: 0.2.13 reads the three bits as they are (ACCEL ON, CLICK REC, SERIAL OFF)
 * and ignores the flag */
static void settings_defaults(uint8_t *s)              /* s: favorites.factory[15] (a record's or the RAM copy) */
{
    if (s[24] & (PREF_DEFAULTS >> 16))
        return;
    s[30] = (uint8_t)(s[30] | PREF_ACCEL | PREF_SERIAL_OFF);
    if ((s[28] & 3u) == 0u)                            /* (CLICK: 0 OFF, 1 REC, 2 ON; 3 was put back to 0 above) */
        s[28] = (uint8_t)(s[28] | 1u);
    s[10] &= 1u;                                       /* (bit 0: ANIM IDLE) */
    s[24] = (uint8_t)(s[24] | PREF_DEFAULTS >> 16);
}
typedef struct {
    uint32_t magic, palette, rsv, zoom;   /* rsv: was the OUTPUT EQ (0 FLAT, 1 LOWCUT, 2 BASS+), removed in 0.2.19: written 0, whatever an older record holds is ignored */
    panel_t panel;
    uint32_t bold;
    struct { uint8_t factory[16][32]; uint32_t user, filter, user_hi[3]; } favorites;   /* (= favorites.c favorites_t) */
} persist_t;
#define PERSIST_MAGIC 0x50455234u

/* Normalize in place; 1 = current, 0 = invalid (any other size or magic: the caller starts with the defaults). */
static int settings_import(persist_t *p, int n)
{
    if (n != (int)sizeof *p || p->magic != PERSIST_MAGIC) return 0;
    /* ([15][29]: the retired STYLE byte (FLAT / LINE, 0 or 1); read by nothing: nothing to normalise) */
    {   /* CLICK, CLICK LEVEL, COUNT-IN (ui.c ui_rec_prefs): two bits each, 3 is no value: the default (0) */
        uint32_t b = p->favorites.factory[15][28], f;
        for (f = 0; f < 3u; f++)
            if (((b >> (2u * f)) & 3u) == 3u)
                b &= ~(3u << (2u * f));
        p->favorites.factory[15][28] = (uint8_t)b;
    }
    settings_defaults(p->favorites.factory[15]);   /* DUELL 0.2.14: the new defaults, once (above) */
    if (p->favorites.factory[15][27] > 7u)         /* SCREEN OFF (1.1.5, ui.c SCR_CODE): unknown = 0 (NEVER since 1.1.5.1) */
        p->favorites.factory[15][27] = 0;
    if (p->favorites.factory[15][11] > CAT_N + 14u) /* PATCHES TYPE (ui.c ui_lcat): a category CAT_*, or CAT_N + an engine; unknown = none */
        p->favorites.factory[15][11] = 0;
    /* [15][26] HOME (1.3): read by nothing since 0.2.14, kept as saved */
    p->magic = PERSIST_MAGIC;
    p->palette = palette_to_stored(palette_from_stored(p->palette));
    settings.magic = SETTINGS_MAGIC;
    settings.palette = palette_from_stored(p->palette);
    settings.rsv = 0;
    settings.zoom = p->zoom;
    settings_hold = (uint8_t)hold_from_stored(p->bold);
    settings_leds = (uint8_t)leds_from_stored(p->zoom);
#ifdef FELUCCA_FAVORITES
    memcpy(&favorites, &p->favorites, sizeof favorites);
    favorites.filter = favorites.filter == 1u;
    memset(favorites.factory[FAV_WAVE_ROW], 0, FAV_WAVE_AT);   /* DIGITAL's starred presets (engine 1, gone in 0.2.14): cleared (bytes 16..31 hold WAVE's flags, favorites.c) */
#endif
    if (p->panel.magic == PANEL_MAGIC) panel = p->panel;
    return 1;
}

/* Start with the last imported/saved object, including fields owned by a
 * feature absent from this build. No save-on-boot or extra flash writes. */
static void settings_export(persist_t *p)
{
    p->magic = PERSIST_MAGIC;
    p->palette = palette_to_stored(settings.palette);
    p->rsv = 0;
    p->zoom = settings.zoom = leds_to_stored(settings.zoom, settings_leds);
    p->panel = panel;
    p->bold = hold_to_stored(p->bold, settings_hold);
#ifdef FELUCCA_FAVORITES
    favorites.factory[15][25] = 1u;               /* (DUELL step 3: the MIDI LEARN map names DUELL's ids; RAM and the
                                                   * saved copy alike, so an unchanged setting compares equal) */
    memcpy(&p->favorites, &favorites, sizeof favorites);
#endif
}
