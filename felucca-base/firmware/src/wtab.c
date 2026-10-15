/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.14: the USER WAVETABLES USR1..USR3 (TABLE 26..28 of the WAVE engine, eng_wave.c: the data, the build, the names).
 *
 * A slot is a storage.c object (OBJ_WTAB0..2, an A/B pair of sectors each, flash 0xA0000..0xA5FFF: the first 24 KiB of the old user
 * sample region, which stays outside the app image and outside the update loader's window [0x97000, 0xE0000), so a firmware update keeps
 * the tables), the payload one wt_obj_t of 1228 bytes: version 1, 19 frames, 64 harmonics, flags 0, the 8 character name, 19 x 64
 * harmonic bytes. The commit record's CRC-32 (storage.c) covers the payload; wt_obj_ok is the second check, at load and before a write.
 * The slots are read once at boot (wt_boot) into the RAM mirror wt_ram, which the build reads (wv_user); an object that fails either check is
 * an EMPTY slot (the sine, named EMPTY).
 *
 * Written ONLY by the editor (editor_wtab.c: WT_PUT, and backup ids 24..26): the commands stop the sequencer first (ed_flash_stop; rc 3 when
 * it does not stop), exactly as the backup writes do, and the write is st_save's: the flash duck over the erase, the payload and the commit
 * record, the commit record last (a power cut leaves the old table). Nothing on the unit itself writes them. A patch, multi or
 * the morph ends name a user table by its number (TABLE 26..28) only; the table stays in the slot, never inside the patch: a patch
 * restored on a unit whose slot is EMPTY plays the sine, a slot with another table plays that one. */
static void wt_boot(void)                       /* persist_boot (up_boot): the slots from flash */
{
    uint32_t u;
    wt_used = 0;
    for (u = 0; u < WV_NU; u++) {
        memset(&wt_ram[u], 0, sizeof wt_ram[u]);
#if FELUCCA_FLASH
        if (flash_ok && st_load(OBJ_WTAB0 + u, &wt_ram[u], sizeof wt_ram[u]) == (int)sizeof wt_ram[u] &&
            wt_obj_ok((const uint8_t *)&wt_ram[u], sizeof wt_ram[u]))
            wt_used |= (uint8_t)(1u << u);
        else
            memset(&wt_ram[u], 0, sizeof wt_ram[u]);
#endif
    }
    wt_names();
}

/* slot u := the object (len bytes, checked here). 0 ok, 2 not a user table, 4 the flash write failed (the slot is as it was) */
static int wt_store(uint32_t u, const uint8_t *obj, uint32_t len)
{
    if (u >= WV_NU || !wt_obj_ok(obj, len))
        return 2;
#if FELUCCA_FLASH
    if (!flash_ok || st_save(OBJ_WTAB0 + u, obj, len))
        return 4;
#endif
    memcpy(&wt_ram[u], obj, len);
    wt_used |= (uint8_t)(1u << u);
    wt_changed(u);
    return 0;
}

/* slot u emptied (both copies erased, the copy that is not current first: st_wipe). 0 ok, 4 the flash failed */
static int wt_erase(uint32_t u)
{
    if (u >= WV_NU)
        return 2;
#if FELUCCA_FLASH
    if (!flash_ok || st_wipe(OBJ_WTAB0 + u))
        return 4;
#endif
    memset(&wt_ram[u], 0, sizeof wt_ram[u]);
    wt_used &= (uint8_t)~(1u << u);
    wt_changed(u);
    return 0;
}
