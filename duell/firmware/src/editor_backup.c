/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Complete musical archive: no raw addresses are accepted. Small objects are staged
 * in main-loop RAM and fully validated before the existing A/B commit path writes.
 * Ids 32..34 were the user sample slots USR1..USR3: DUELL 0.2.9 retired the sample engines and their slots, so they are listed empty
 * (the archive keeps its 16 ids); an archive's sample objects are not restored (SMP_BEGIN answers rc 6, unsupported: editor.c).
 * The PATCH store (upreset.c) is carried ONE PATCH AT A TIME as LOGICAL records (the cloud plan's job A, INFO 42 01 caps bit 5, value 32; the archive is
 * "version 5", duellbackup.js): object id ED_BK_PATCH (40) plus a SLOT byte, the stored record as PATCH_RAW moves it (up_raw_get: the slot and revision bytes
 * zero, so the same sound is the same bytes in any slot; up_put_raw writes it back through the store's own placement), so an archive survives any later change of
 * the store's geometry (chunks, records a chunk). Before it (DUELL 0.2.15 .. job A) the archive carried the PHYSICAL chunks, ids 6, 7, 9..12, 27..31 and 35..39
 * (OBJ_PATCH0 .. OBJ_PATCHX0..9): those ids are gone (a LIST does not list them, GET and PUT answer rc 1); an old archive of chunks is refused by the editor.
 *   LIST (no argument)        stops the transport, takes the snapshot, lists the other objects (ids 0..5, 8, 13..26, 32..34: 24 in all)
 *   LIST page                 (page 1 .. UP_SLOTS / ED_BK_PPAGE = 4; needs the LIST before) the saved patches of slots 32 (page - 1) .. 32 page - 1: count, then per saved
 *                             patch slot (the id byte), size u32, crc u32 (a slot that holds its factory sound is not listed: it has no record)
 *   GET 40 off count slot     (9 bytes: id, offset u32, count lo, count hi, slot) a piece of the record; the reply is the usual one with id 40
 *   PUT begin 40 size crc slot (13 bytes) / data / commit / abort as every object; the record goes in through up_put_raw (refused at commit, rc 2, when the store would not
 *                             read it: pt_rec_ok, the extension block's lengths included); size 0 = the slot's record removed (its factory sound is back, as RESTORE)
 * Id 8 (the FM6 patch bank of 1.0..1.0.2) is listed empty and a PUT of it is accepted and dropped.
 * Ids 2..5 (Felucca's project slots A..D) are listed empty since DUELL's MULTI store took their flash (project.c multi.c);
 * a PUT of one from an older archive is accepted (whatever its length) and dropped.
 * DUELL 0.2.14: what a PUT restores must be of this firmware's formats (project.c, multi.c, upreset.c): a project of another format
 * or revision (2), a multi object whose record is not MU_VER with P_COUNT values (2), a patch chunk with a record without PTF_CB (2) are
 * refused; nothing is written or changed then.
 * DUELL 0.2.14 (INFO 42 01 caps bit 2): ids 13..20 = OBJ_MULTI0..7 (M01-02 .. M15-16, two multis an object, magic MUL1, multi.c) and ids
 * 21..23 = OBJ_PATSLOT0..2 (P01-11, P12-22, P23-32, 11 / 11 / 10 slots an object, magic PTS3, a compact variable-length object, patslot.c) are in the
 * archive (27 ids). They are read
 * where they live (a multi object from flash through st_current, a pattern object from the RAM mirror ps_bank, which is the truth while a
 * pattern write is pending), never from the staging RAM (the runtime snapshot of id 0 sits there). An object with no multi record is listed
 * with length 0, and a PUT of length 0 writes that empty object (the factory multis show again). A pattern object is never empty (every
 * slot holds a pattern): a PUT of length 0 gives its slots their defaults again.
 * A pattern restore replaces the mirror and clears its dirty bit, so a pending write cannot overwrite what was restored.
 * DUELL 0.2.14 (INFO 42 01 caps bit 3): ids 24..26 = the user wavetables USR1..USR3 (wtab.c; the object is wt_obj_t, 1228 bytes, read from the RAM
 * mirror wt_ram; length 0 = EMPTY, a PUT of length 0 empties the slot). A restore is wt_store: the same checks and the same write as the
 * editor's WT_PUT (the playing sequencer is stopped first, ed_flash_stop). 30 ids.
 */
#define ED_BK_MUL0 13u
#define ED_BK_PAT0 21u
#define ED_BK_WT0 24u
#define ED_BK_PATCH 40u                         /* a patch record, with its slot (see the top) */
#define ED_BK_PPAGE 32u                         /* patch records a LIST page lists (32 x 11 bytes fit the reply) */
static const uint8_t ED_BK_IDS[24] = {0, 1, 2, 3, 4, 5, 8, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 32, 33, 34};
static uint8_t ed_bk_slot;                      /* the slot of the patch record a PUT is staging */
static uint8_t ed_wt_put;                       /* a user wavetable upload is staging (editor_wtab.c): they share ED_BK_RAW */
static int ed_bk_wt(uint32_t id) { return id >= ED_BK_WT0 && id < ED_BK_WT0 + WV_NU ? (int)(id - ED_BK_WT0) : -1; }
static int ed_bk_multi(uint32_t id) { return id >= ED_BK_MUL0 && id < ED_BK_MUL0 + MU_NOBJ ? (int)(id - ED_BK_MUL0) : -1; }
static int ed_bk_pat(uint32_t id) { return id >= ED_BK_PAT0 && id < ED_BK_PAT0 + PS_NOBJ ? (int)(id - ED_BK_PAT0) : -1; }
#define ED_BK_N ((uint32_t)sizeof ED_BK_IDS)
#define ED_BK_MAX ((uint32_t)sizeof proj_wire_u.raw)
#define ED_BK_RAW (proj_wire_u.raw)  /* reuse the existing serialized main-loop scratch */
_Static_assert(sizeof proj_wire_u.raw >= PT_CAP, "backup staging");
static uint8_t ed_bk_valid, ed_bk_put, ed_bk_id, ed_bk_gen;
static uint32_t ed_bk_len, ed_bk_crc, ed_bk_pos, ed_bk_ms, ed_bk_usb;
static void ed_bk_u32(uint32_t n) { for (uint32_t i = 0; i < 5u; i++) ed_b((n >> (i * 7u)) & 127u); }
static uint32_t ed_bk_r32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 7 | (uint32_t)p[2] << 14 | (uint32_t)p[3] << 21 | (uint32_t)p[4] << 28;
}
static void ed_bk_pack(const uint8_t *p, uint32_t n)
{
    while (n) {
        uint32_t k = n > 7u ? 7u : n, mask = 0;
        for (uint32_t i = 0; i < k; i++) mask |= (uint32_t)(p[i] >> 7) << i;
        ed_b(mask);
        for (uint32_t i = 0; i < k; i++) ed_b(p[i]);
        p += k; n -= k;
    }
}
/* a MUL1 object: the shape multi.c mu_obj_read demands, and each stored record at least a header the loader takes (version, parts, name) */
static int ed_bk_mul_ok(const uint8_t *b, uint32_t len)
{
    uint32_t magic, l[2], h, at = MU_HDR, i;
    if (len < MU_HDR || len > MU_OBJ_MAX)
        return 0;
    memcpy(&magic, b, 4);
    l[0] = mu_rd16(b + 4);
    l[1] = mu_rd16(b + 6);
    if (magic != MU_MAGIC || l[0] > MU_REC_MAX || l[1] > MU_REC_MAX || MU_HDR + l[0] + l[1] != len)
        return 0;
    for (h = 0; h < 2u; at += l[h++]) {
        if (!l[h])
            continue;
        if (l[h] < MU_G_AT || b[at] != MU_VER || b[at + 1] != P_COUNT)
            return 0;
        for (i = 0; i < PROJ_NAME_LEN; i++)
            if (b[at + MU_NAME_AT + i] && (b[at + MU_NAME_AT + i] < 32u || b[at + MU_NAME_AT + i] > 126u))
                return 0;
    }
    return 1;
}
static int ed_bk_mul_empty(const uint8_t *b) { return !mu_rd16(b + 4) && !mu_rd16(b + 6); }
/* a PTS3 object (patslot.c): ps_obj_ok, the shape and every record (ranges, printable names, steps) */
static int ed_bk_pat_ok(uint32_t o, const uint8_t *b, uint32_t len) { return ps_obj_ok(o, b, len); }
/* the settings record as a backup holds it (id 1): built NOW, in proj_scratch (main loop; the caller uses it within the same command and keeps no pointer): the saved record's head, so
 * fields absent from this build survive, with the settings as they stand over it. No copy of its own in the pool (cloud job C, repair R1: the record grew by 320 bytes with the MIDI MAP);
 * a setting changed between two chunk reads shows in the CRC of the whole object, which the host checks */
_Static_assert(sizeof(project_t) >= sizeof(persist_t) && __alignof__(project_t) >= __alignof__(persist_t), "the settings record is built in proj_scratch");
static const uint8_t *ed_bk_settings_rec(void)
{
    persist_t *p = (persist_t *)(void *)&proj_scratch;
#if FELUCCA_FLASH
    persist_base(p);                                /* fields absent from this build survive */
#else
    memset(p, 0, sizeof *p);
#endif
    settings_export(p);
    return (const uint8_t *)p;
}
static const uint8_t *ed_bk_object(uint32_t id, uint32_t *len)
{
    *len = 0;
    if (ed_bk_multi(id) >= 0) {                     /* DUELL 0.2.14: a MULTI object, where it lives (not in the staging RAM) */
        uint32_t o = (uint32_t)ed_bk_multi(id), n;
        const uint8_t *p;
#if FELUCCA_FLASH
        st_hdr_t h;
        if (!flash_ok || st_current(OBJ_MULTI0 + o, &h) < 0)
            return ED_BK_RAW;
        n = h.len;
        p = st_buf;                                 /* (st_current left the payload there) */
#else
        n = mu_ram_len[o];
        p = mu_ram[o];
#endif
        if (!ed_bk_mul_ok(p, n) || ed_bk_mul_empty(p))
            return ED_BK_RAW;                       /* (none, torn, damaged or both multis empty: length 0, as mu_obj_read reads it) */
        *len = n;
        return p;
    }
    if (ed_bk_wt(id) >= 0) {                        /* DUELL 0.2.14: a user wavetable, from the RAM mirror (EMPTY: length 0) */
        uint32_t u = (uint32_t)ed_bk_wt(id);
        if (!(wt_used >> u & 1u) || !wt_obj_ok((const uint8_t *)&wt_ram[u], sizeof wt_ram[u]))
            return ED_BK_RAW;
        *len = sizeof wt_ram[u];
        return (const uint8_t *)&wt_ram[u];
    }
    if (ed_bk_pat(id) >= 0) {                       /* the pattern slots: the RAM mirror is the truth (a write may be pending) */
        uint32_t o = (uint32_t)ed_bk_pat(id);
        *len = ps_used_len(o);                      /* (no slot is empty: the object is always there, defaults included) */
        return ps_bank[o];
    }
    if (id == 0u) { *len = sizeof(project_store_t); return ED_BK_RAW; }
    if (id == 1u) { *len = sizeof(persist_t); return ed_bk_settings_rec(); }
    if (id >= 2u && id <= 5u)                       /* the retired project slots: always empty */
        return ED_BK_RAW;
    if (id == 8u)                                   /* the retired FM6 patch bank: always empty */
        return ED_BK_RAW;
    if (id >= 32u && id <= 34u)                     /* the retired user sample slots (DUELL 0.2.9): always empty */
        return ED_BK_RAW;
    return 0;
}
/* slot k's stored record (up_raw_get, in pt_enc: the store's write scratch, free between writes, main loop only); *len 0: the slot holds its factory sound */
static const uint8_t *ed_bk_patch(uint32_t k, uint32_t *len)
{
    *len = k < UP_SLOTS ? up_raw_get(k, pt_enc) : 0u;
    return pt_enc;
}
static uint32_t ed_bk_capture(void)
{
    if (ed_flash_stop()) return 3;
    ed_wt_put = 0;
    project_capture(&proj_scratch);
    if (!proj_pack((project_store_t *)ED_BK_RAW, &proj_scratch)) return 2;
    ed_bk_gen = ++proj_wire_gen;
    ed_bk_put = 0; ed_bk_valid = 1; ed_bk_usb = usb.resets;
    return 0;
}
static int ed_bk_panel_valid(const panel_t *p)
{
    uint32_t b = 0, e = 0;
    if (p->magic != PANEL_MAGIC) return 0;
    for (uint32_t i = 0; i < NB; i++) {
        if (p->btn[i] >= NB || (b & (1u << p->btn[i]))) return 0;
        b |= 1u << p->btn[i];
    }
    for (uint32_t i = 0; i < NE; i++) {
        if (p->enc[i] >= NE || (e & (1u << p->enc[i])) || (p->dir[i] != 1 && p->dir[i] != -1)) return 0;
        e |= 1u << p->enc[i];
    }
    return 1;
}
static uint32_t ed_bk_commit(void)
{
    uint8_t *raw = ED_BK_RAW;
    uint32_t obj;
    if (ed_bk_pos != ed_bk_len || st_crc32(raw, ed_bk_len) != ed_bk_crc) return 2;
    if ((ed_bk_id >= 2u && ed_bk_id <= 5u) || ed_bk_id == 8u)   /* an older archive's project slot, FM6 bank: nowhere to go (dropped) */
        return 0;
    if (ed_bk_id == 0u) {
        if (ed_bk_len && !proj_import(&proj_scratch, raw, (int)ed_bk_len)) return 2;
        if (ed_bk_len) {                            /* the values inside their ranges, written back as FUN11 */
            proj_bound(&proj_scratch);
            if (!proj_pack((project_store_t *)raw, &proj_scratch)) return 2;
            ed_bk_len = sizeof(project_store_t);
        }
        if (!ed_bk_len) return 2;
        return project_restore_runtime(&proj_scratch) ? 2u : 0u;
    } else if (ed_bk_id == 1u) {
        const persist_t *p = (const persist_t *)raw;
        if (ed_bk_len != sizeof *p || p->magic != PERSIST_MAGIC || !palette_stored_ok(p->palette) ||
            !leds_stored_ok(p->zoom) || !hold_stored_ok(p->bold) || p->favorites.filter > 1u || !ed_bk_panel_valid(&p->panel)) return 2;
        obj = OBJ_SETTINGS;
    } else if (ed_bk_multi(ed_bk_id) >= 0) {   /* DUELL 0.2.14: a MULTI object (two multis); length 0 = both empty */
        uint32_t o = (uint32_t)ed_bk_multi(ed_bk_id);
        if (!ed_bk_len) {
            uint32_t magic = MU_MAGIC;
            memcpy(raw, &magic, 4);
            memset(raw + 4, 0, 4);
            ed_bk_len = MU_HDR;
        } else if (!ed_bk_mul_ok(raw, ed_bk_len)) return 2;
        if (mu_obj_write(o, ed_bk_len)) return 4;   /* (mu_buf is the staging RAM; host builds: mu_ram) */
        mu_obj_read(o);                             /* what the unit holds now (flash, checked); its names and used mask */
        mu_note(o);
        sync_reload = 1; ui.force = 1;
        return 0;
    } else if (ed_bk_wt(ed_bk_id) >= 0) {      /* DUELL 0.2.14: a user wavetable; length 0 = EMPTY */
        uint32_t u = (uint32_t)ed_bk_wt(ed_bk_id);
        if (!ed_bk_len)
            return wt_erase(u) ? 4u : 0u;
        return (uint32_t)wt_store(u, raw, ed_bk_len);   /* 0 ok, 2 not a user table, 4 flash */
    } else if (ed_bk_pat(ed_bk_id) >= 0) {     /* DUELL: a PATTERN object (11 or 10 slots, PTS3); length 0 = the defaults of its slots again */
        uint32_t o = (uint32_t)ed_bk_pat(ed_bk_id);
        if (!ed_bk_len) {
            ps_reset_obj(o);                        /* (marks the object dirty: the flash follows; a failed write is retried) */
            sync_reload = 1; ui.force = 1;
            return 0;
        }
        if (!ed_bk_pat_ok(o, raw, ed_bk_len)) return 2;
#if FELUCCA_FLASH
        if (!flash_ok || st_save(OBJ_PATSLOT0 + o, raw, ed_bk_len)) return 4;
#endif
        memcpy(ps_bank[o], raw, ed_bk_len);         /* the mirror is the restored object; no write of the old one is pending */
        ps_obj_check(o, (int)ed_bk_len);
        ps_dirty &= (uint8_t)~(1u << o);
        sync_reload = 1; ui.force = 1;
        return 0;
    } else if (ed_bk_id == ED_BK_PATCH) {     /* one patch record into its slot (the store places it); length 0: the slot's record removed */
        int r = ed_bk_len ? up_put_raw(ed_bk_slot, raw, ed_bk_len) : sl_restore(ed_bk_slot);
        sync_reload = 1; ui.force = 1;
        return r == 0 || r == 3 ? 0u : r == 1 ? 2u : song.playing ? 3u : 4u;   /* (3: no flash, kept in RAM; 1: no record the store would read; 2: the flash failed) */
    } else return 1;
#if FELUCCA_FLASH
    if (!flash_ok || st_save(obj, raw, ed_bk_len)) return 4;
#else
    (void)obj;
#endif
    {
        persist_t *q = (persist_t *)(void *)&proj_scratch;   /* (only id 1 is left here; the record is checked and applied in proj_scratch, no copy of its own) */
        memcpy(q, raw, sizeof *q);
        settings_import(q, sizeof *q);
        panel_init(); settings_init(); palette_set(settings.palette);
#if FELUCCA_FLASH
        persist_note(q); persist_pending = 0;
#endif
    }
    sync_reload = 1; ui.force = 1;
    return 0;
}
/* a project object's length: FUN11's (the older lengths, FUN7 .. FUN9, are no project since 0.2.14) */
static int proj_store_len(uint32_t len) { return len == sizeof(project_store_t); }
static uint32_t ed_bk_write(const uint8_t *a, uint32_t n)
{
    /* the arguments first: a malformed request never stops the transport */
    if (n < 2u || a[0] > 3u || (a[1] > 5u && a[1] != 8u && a[1] != ED_BK_PATCH && ed_bk_multi(a[1]) < 0 && ed_bk_pat(a[1]) < 0 && ed_bk_wt(a[1]) < 0)) return 1;   /* (the ids 6, 7, 9..12, 27..31, 35..39 were the patch store's chunks: gone) */
    if (a[0] == 0u) {
        if (n != (a[1] == ED_BK_PATCH ? 13u : 12u) || a[6] > 15u || a[11] > 15u) return 1;
        uint32_t len = ed_bk_r32(a + 2);
        if (len > ED_BK_MAX || (a[1] == 0u && !proj_store_len(len)) ||
            (a[1] == 1u && len != sizeof(persist_t)) ||
            (a[1] == ED_BK_PATCH && (a[12] >= UP_SLOTS || (len && (len < PT_FIX || len > PT_WORST)))) ||
            (ed_bk_multi(a[1]) >= 0 && len && (len < MU_HDR || len > MU_OBJ_MAX)) ||
            (ed_bk_pat(a[1]) >= 0 && len && (len < PS_HDR || len > PS_OBJ_MAX)) ||
            (ed_bk_wt(a[1]) >= 0 && len && len != sizeof(wt_obj_t))) return 1;
        if (ed_flash_stop()) return 3;
        ed_wt_put = 0;
        ed_bk_valid = 0; ed_bk_put = 1; ed_bk_id = a[1]; ed_bk_len = len; ed_bk_gen = ++proj_wire_gen;
        ed_bk_slot = a[1] == ED_BK_PATCH ? a[12] : 0u;
        ed_bk_crc = ed_bk_r32(a + 7); ed_bk_pos = 0;
        ed_bk_usb = usb.resets; ed_bk_ms = fm1_ms;
        return 0;
    }
    if (!ed_bk_put || ed_bk_id != a[1] || ed_bk_usb != usb.resets || fm1_ms - ed_bk_ms > 15000u ||
        ed_bk_gen != proj_wire_gen) {                 /* (a project save / load reused the staging RAM) */
        ed_bk_put = 0; return 5;
    }
    ed_bk_ms = fm1_ms;
    if (a[0] == 3u) { ed_bk_put = 0; return n == 2u ? 0u : 1u; }
    if (a[0] == 2u) {
        if (n != 2u) return 1;
        if (ed_flash_stop()) return 3;
        uint32_t rc = ed_bk_commit(); ed_bk_put = 0; return rc;
    }
    if (n < 9u || a[6] > 15u || ed_bk_r32(a + 2) != ed_bk_pos) return 1;
    uint32_t count = ed_unpack7(a + 7, n - 7u, ed_smp_buf, 256u);
    if (!count || count > ed_bk_len - ed_bk_pos) return 1;
    if (ed_flash_stop()) return 3;
    memcpy(ED_BK_RAW + ed_bk_pos, ed_smp_buf, count); ed_bk_pos += count;
    return 0;
}
static int ed_backup_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    if (cmd == ED_BACKUP_LIST && n == 1u) {         /* a page of the saved patch records (see the top) */
        uint32_t pg = a[0], rc = pg >= 1u && pg <= UP_SLOTS / ED_BK_PPAGE ? 0u : 1u, k, cnt = 0;
        if (!rc && (!ed_bk_valid || ed_bk_usb != usb.resets))
            rc = 5u;
        else if (!rc && transport_busy())
            rc = 3u;
        if (!rc)
            for (k = (pg - 1u) * ED_BK_PPAGE; k < pg * ED_BK_PPAGE; k++)
                cnt += (uint32_t)up_used(k);
        ed_b(1); ed_b(rc); ed_b(rc ? 0u : cnt);
        if (!rc)
            for (k = (pg - 1u) * ED_BK_PPAGE; k < pg * ED_BK_PPAGE; k++)
                if (up_used(k)) {
                    uint32_t len;
                    const uint8_t *p = ed_bk_patch(k, &len);
                    ed_b(k); ed_bk_u32(len); ed_bk_u32(st_crc32(p, len));
                }
        return 1;
    }
    if (cmd == ED_BACKUP_LIST) {
        uint32_t rc = n ? 1u : ed_bk_capture();
        ed_b(1); ed_b(rc); ed_b(rc ? 0u : ED_BK_N);
        if (!rc) for (uint32_t i = 0; i < ED_BK_N; i++) {
            uint32_t len;
            const uint8_t *p = ed_bk_object(ED_BK_IDS[i], &len);
            ed_b(ED_BK_IDS[i]); ed_bk_u32(len); ed_bk_u32(st_crc32(p, len));
            fm1_wdt_feed();
        }
        return 1;
    }
    if (cmd == ED_BACKUP_GET) {
        uint32_t len = 0, off = n >= 6u ? ed_bk_r32(a + 1) : 0u;
        int isp = n && a[0] == ED_BK_PATCH;
        uint32_t count = n == (isp ? 9u : 8u) ? (uint32_t)a[6] | (uint32_t)a[7] << 7 : 0u;
        const uint8_t *p = !n ? 0 : isp ? (n == 9u && a[8] < UP_SLOTS ? ed_bk_patch(a[8], &len) : 0) : ed_bk_object(a[0], &len);
        uint32_t rc = (!ed_bk_valid || ed_bk_usb != usb.resets || (n && !a[0] && ed_bk_gen != proj_wire_gen)) ? 5u : transport_busy() ? 3u :
            n != (isp ? 9u : 8u) || a[5] > 15u || !p || !count || count > 256u || off > len || count > len - off ? 1u : 0u;
        ed_b(n ? a[0] : 127u); ed_b(rc); ed_bk_u32(off); ed_b(rc ? 0u : count & 127u); ed_b(rc ? 0u : count >> 7);
        if (!rc) ed_bk_pack(p + off, count);
        return 1;
    }
    if (cmd == ED_BACKUP_PUT) {
        uint32_t rc = ed_bk_write(a, n);
        ed_b(n ? a[0] : 127u); ed_b(n >= 2u ? a[1] : 127u); ed_b(rc);
        return 1;
    }
    return 0;
}
