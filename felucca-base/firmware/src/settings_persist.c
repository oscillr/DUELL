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
 *   GLO > MODULES' ARP SEQ SENDS PART FX, a set bit = OFF (ui.c mods_*; every value valid, nothing to check); bit 5 (DUELL 0.2.14) MENU > CONTROL > PERF LEAVE, set = KEEP (clear = FADE, the default; ui.c perf_keep); bit 6 (job D, builds with KEY SENSE) MENU > CONTROL > KEY VELOCITY, set = OFF (clear = ON, the default; ui.c keyvel_on); bit 7 free;
 * [15][12..13] (DUELL 4b): the MORPH's TARGET (bit 7 of [12]; 0.2.9: reserved, kept as stored, never read) and each part's AMOUNT (bits 0-6: 1 + %, 0 = the default, 15 %: 25 % before 2026-10-11, the
 * default; ui.c ui_morph_set, ui_input.c morph_settings: read on any change, so an import needs nothing here);
 * [15][11]: PATCHES > TYPE (1.2, Discussion #90; DUELL: 1..CAT_N-1 a category, CAT_N + e only the engine e's patches; ui.c ui_lcat:
 * 0 none = ALL / FAV as favorites.filter says);
 * [15][14..23]: the FX layer's key map up to 0.2.1 (16 white keys x 5 bits); the layer is removed in DUELL 0.2.2: not read or written any
 * more (an older record's bytes are carried through unchanged), never reused.
 * [15][29]: the retired STYLE byte (nothing reads it; BASS MONO is not in the settings: DUELL 0.2.19 keeps it with the music, song.bm, project.c / multi.c).
 * DUELL 0.2.21: the master compressor (GLO > COMP) is a DEVICE setting and lives in the record's `rsv` word (persist_t below: no byte of favorites.factory, the record is the same size); not in the multis, the project or the patches.
 * DUELL 0.2.23: [15][18..21]: the master WIDTH (GLO > WIDTH; ui.c ms_sync), a DEVICE setting: [18] the tag 0xB7, [19] WIDTH 0..200, [20] (the saturator's DRIVE of 0.2.23 / 0.2.24, removed in 0.2.25: never read, written 0), [21] 0
 *   (bytes of the FX layer's retired key map, see [15][14..23] below: without the tag they read as the defaults, WIDTH 100 % = off). [15][22..23] stay unused.
 * [15][25] (DUELL step 3): 1 = the MIDI LEARN map in [14] names DUELL's ids; every export writes 1 (firmware up to 0.2.13 reads it;
 * 0.2.14 no longer converts a map of Felucca's ids: nothing reads the byte).
 * DUELL MIDI MAP (docs/design/MIDI_MAP_DESIGN_2.md; the record changed ONCE, here, and only this job owns it: no other job adds to it): favorites.factory[14], the
 *   32 bytes that held the 16 MIDI LEARN slots (removed: learned CCs are lines of the map now), is the map's header: [0] the VERSION (MAP_REC_VER), [1] the flags (bit 0: the map differs
 *   from the shipped list, derived at every export), [2] the revision of the shipped list the stored map was made with (midi_map_defaults.h MIDI_MAP_DEFAULTS_REV: a different one
 *   fills the numbers of a later firmware's new shipped lines that are empty and were never cleared), [3..31] zero. The record ends with `map`, the 128 lines (midi_map.h) and `map_tail`,
 *   64 reserved bytes (zero; for the velocity settings and a second table, EXTENSION_BLOCK_PLAN 3.3). A record of another size is no record (settings_import), as ever.
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
/* The settings record (PER4). rsv (was the OUTPUT EQ (0 FLAT, 1 LOWCUT, 2 BASS+), removed in 0.2.19: written 0, whatever an older record held was ignored; DUELL 0.2.21: the master compressor's
 * settings word (core.h cmp_set, ui.c cmp_sync): 0xC8 << 24 (0.2.24; 0xC7 before: reads as the defaults) | THRESH (0..40, bits 0..5) | RATIO (0..8, bits 6..9) | ATTACK (0..120, bits 10..16) | RELEASE (0..80, bits 17..23).
 * A word without the tag 0xC8 (0 as every older record and a unit that never set it has, or the old OUTPUT EQ's 0..2) reads as the defaults (RATIO 1:1 = OFF); a field out of range
 * reads as its default. The record's size and magic are the same: no version change). favorites = favorites.c favorites_t (row 14: the map's header, above).
 * PERSIST_HEAD_FIELDS: the fields before the MIDI MAP. project.c keeps the last SAVED record as persist_head_t (this head) plus a CRC-32 of the map, not as a whole persist_t:
 * the 256 bytes of the map and the 64 of its tail would be a second copy in the pool for nothing (cloud job C, repair R1). */
#define PERSIST_HEAD_FIELDS \
    uint32_t magic, palette, rsv, zoom; \
    panel_t panel; \
    uint32_t bold; \
    struct { uint8_t factory[16][32]; uint32_t user, filter, user_hi[3]; } favorites;
typedef struct { PERSIST_HEAD_FIELDS } persist_head_t;
typedef struct {
    PERSIST_HEAD_FIELDS
    uint16_t map[MAP_N];                  /* the MIDI MAP: one line per CC number (midi_map.h) */
    uint8_t map_tail[64];                 /* reserved, zero */
} persist_t;
_Static_assert(sizeof(persist_t) == sizeof(persist_head_t) + 2u * MAP_N + 64u, "persist_head_t is the record before the map");
#define PERSIST_MAGIC 0x50455234u
#define MAP_REC_VER 1u                    /* favorites.factory[14][0] */
/* before the map: 4 words, the panel, bold, the favourites (16 x 32 bytes and 5 words); after it: + the 128 lines and the 64-byte tail, the 16 learn slots' 32 bytes now the header */
_Static_assert(sizeof(persist_t) == 4u * 4u + sizeof(panel_t) + 4u + 16u * 32u + 4u * 5u + 2u * MAP_N + 64u, "persist_t: the settings record's size is fixed (tests/settings_test.c, tests/midimap_test.c)");

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
    cmp_set = p->rsv;                              /* (0.2.21: the compressor's word as stored; ui.c cmp_sync normalises it: no tag or a bad field = the defaults) */
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
    if (p->favorites.factory[14][0] == MAP_REC_VER) {  /* the MIDI MAP: as stored, made valid; a record of another map version starts with the shipped list */
        memcpy(midi_map, p->map, sizeof midi_map);
        map_sanitize();
        if (p->favorites.factory[14][2] != MIDI_MAP_DEFAULTS_REV)
            map_merge_new();                         /* (a later firmware's shipped lines: empty and never cleared numbers only) */
    } else {
        map_defaults();
    }
    return 1;
}

/* Start with the last imported/saved object, including fields owned by a
 * feature absent from this build. No save-on-boot or extra flash writes. */
static void settings_export(persist_t *p)
{
    p->magic = PERSIST_MAGIC;
    p->palette = palette_to_stored(settings.palette);
    p->rsv = cmp_set;                              /* (0 until the compressor is touched here: a unit that never used it writes what it always did) */
    p->zoom = settings.zoom = leds_to_stored(settings.zoom, settings_leds);
    p->panel = panel;
    p->bold = hold_to_stored(p->bold, settings_hold);
#ifdef FELUCCA_FAVORITES
    favorites.factory[15][25] = 1u;               /* (DUELL step 3: the MIDI LEARN map names DUELL's ids; RAM and the
                                                   * saved copy alike, so an unchanged setting compares equal) */
    (void)map_line(0);                             /* (the shipped list, if the map was never loaded) */
    favorites.factory[14][0] = MAP_REC_VER;        /* the MIDI MAP's header (RAM and the saved copy alike, as [15][25]) */
    favorites.factory[14][1] = memcmp(midi_map, MIDI_MAP_DEFAULTS, sizeof midi_map) != 0;
    favorites.factory[14][2] = MIDI_MAP_DEFAULTS_REV;
    memcpy(&p->favorites, &favorites, sizeof favorites);
#endif
    memcpy(p->map, midi_map, sizeof p->map);
    memset(p->map_tail, 0, sizeof p->map_tail);
}
