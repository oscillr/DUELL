/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.2: ERASE DATA, start from scratch. MENU > SYSTEM > ERASE DATA (after its dialog, ui_input.c) and the editor's
 * ERASE_ALL (cmd 80, editor_erase.c, with its token) run the same erase_data_run(); included by project.c.
 *
 * What goes (flash objects, storage.c; a stored object is the A/B pair of sectors, both erased by st_wipe):
 *   the autosave (OBJ_AUTOSAVE), the pattern slots (OBJ_PATSLOT0..2), the multis (OBJ_MULTI0..7: M01..M04 and M13..M16 are
 *   the factory ones again, ROM), the patch store (OBJ_PATCH0..5 and, DUELL 0.2.15, OBJ_PATCHX0..9: every stored patch and every factory sound's shadow copy),
 *   the user wavetables (OBJ_WTAB0..2, DUELL 0.2.14: USR1..USR3 are EMPTY again),
 *   and the settings (OBJ_SETTINGS: theme, scale, MIDI channels, the favourites, the preferences, the panel table).
 * What stays: the FM6 voice bank and the firmware itself (DUELL 0.2.9: the user sample slots are gone with the SAMPLE engine; the rest of
 * their flash, 0xBA000..0xC7FFF (0xA6000..0xB9FFF is the patch store's chunks 6..15, above), is not touched; 0xA0000..0xA5FFF is the user wavetables, above).
 *
 * Order and power cuts: the objects in that order, the SETTINGS LAST. Every object is erased copy by copy (st_wipe: the
 * copy that is not the current one first), so a cut at any point leaves each object either whole as it was or empty; nothing
 * older comes back. The settings being last matters at boot: the unit starts as a NEW unit (VIOLET, LAYER, M01: persist_boot
 * finds no valid settings, ui_input.c first_boot_poll) only once everything else is gone. A cut before that boots as the
 * unit was left, with less of its data (the autosave, patterns, multis or patches already erased, the factory ones in
 * their place), and ERASE DATA can simply be run again; a cut after the settings' first sector leaves the other, whole copy
 * of them and is the same case. Nothing is half-valid: a commit record is valid or the object is empty.
 *
 * After it the unit restarts (erase_restart_due, main.c: the chip reset; the browser build reloads the page) so every
 * RAM copy (patch store, multis, pattern slots, settings) is read from the now empty flash: the first-start path. Until
 * then nothing may write the old state back: erase_busy blocks the settings / pattern slot / autosave writers. */
#if FELUCCA_FLASH
#ifndef ERASE_FEED
#define ERASE_FEED() ((void)0)                    /* (felucca.c: fm1_wdt_feed(); the host tests have none) */
#endif
#define ERASE_RESTART_MS 700u                    /* the message is seen / the editor's reply leaves before the restart */
static uint32_t erase_at;                        /* fm1_ms | 1 when the restart is due (0: none) */

static int erase_busy(void) { return erase_at != 0u; }
static int erase_restart_due(void) { return erase_at && (int32_t)(fm1_ms - (erase_at & ~1u)) >= 0; }

/* 0 done (the restart follows), 3 stop playback first, 4 the flash failed (some objects may be gone already: run it again;
 * the settings were not touched, so the unit is not a new unit) */
static int erase_data_run(void)
{
    static const uint8_t ORDER[] = {OBJ_AUTOSAVE, OBJ_PATSLOT0, OBJ_PATSLOT0 + 1, OBJ_PATSLOT0 + 2,
                                    OBJ_MULTI0, OBJ_MULTI0 + 1, OBJ_MULTI0 + 2, OBJ_MULTI0 + 3,
                                    OBJ_MULTI0 + 4, OBJ_MULTI0 + 5, OBJ_MULTI0 + 6, OBJ_MULTI0 + 7,
                                    OBJ_PATCH0, OBJ_PATCH0 + 1, OBJ_PATCH0 + 2, OBJ_PATCH0 + 3, OBJ_PATCH0 + 4, OBJ_PATCH0 + 5,
                                    OBJ_PATCHX0, OBJ_PATCHX0 + 1, OBJ_PATCHX0 + 2, OBJ_PATCHX0 + 3, OBJ_PATCHX0 + 4,
                                    OBJ_PATCHX0 + 5, OBJ_PATCHX0 + 6, OBJ_PATCHX0 + 7, OBJ_PATCHX0 + 8, OBJ_PATCHX0 + 9,
                                    OBJ_WTAB0, OBJ_WTAB0 + 1, OBJ_WTAB0 + 2,
                                    OBJ_SETTINGS};
    uint32_t i;
    if (transport_busy())
        return 3;
    if (!flash_ok || erase_at)
        return 4;
    persist_pending = 0;                         /* nothing queued is written over the empty flash */
    ps_dirty = 0;
    as.wrote = 1;                                /* and no autosave for the next 60 s (autosave_poll's gap) */
    as.last = fm1_ms;
    as.err = 0;
    for (i = 0; i < sizeof ORDER; i++) {
        if (st_wipe(ORDER[i]))
            return 4;
        ERASE_FEED();
    }
    erase_at = (fm1_ms + ERASE_RESTART_MS) | 1u;
    return 0;
}
#else
static int erase_busy(void) { return 0; }
static int erase_restart_due(void) { return 0; }
static int erase_data_run(void) { return 4; }
#endif
