/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Complete musical archive: no raw addresses are accepted. Small objects are staged
 * in main-loop RAM and fully validated before the existing A/B commit path writes.
 * Ids 32..34 were the user sample slots USR1..USR3: DUELL 0.2.9 retired the sample engines and their slots, so they are listed empty
 * (the archive keeps its 16 ids); an archive's sample objects are not restored (SMP_BEGIN answers rc 6, unsupported: editor.c).
 * DUELL: ids 6, 7, 9, 10, 11 and 12 are the first six chunks of the PATCH store (upreset.c: sparse records, FM6 patches inside them), ids
 * 6, 7, 9, 10, 11, 12 = OBJ_PATCH0 .. +5 (ed_bk_chunk). DUELL 0.2.15 (INFO 42 01 caps bit 4, value 16): chunks 6..15 are ids 27..31 and 35..39
 * (OBJ_PATCHX0..9; ids 32..34, the retired sample slots, stay between them: 40 ids in all). The archive carries the PHYSICAL chunks (the
 * smaller change, one code path with the first six; a logical "patch store" object would survive a later change of the geometry): a chunk
 * is sent as its live records (pt_build; empty: length 0), a restore writes the chunk and reads the directory again (pt_boot). A chunk with
 * more than PT_K records is refused (2): the guarantee that every one of the 128 slots fits rests on it. The file is "version 4" (duellbackup.js). an archive of Felucca's banks (another shape) is refused (2). Id 8 (the FM6 patch bank of
 * 1.0..1.0.2) is listed empty and a PUT of it is accepted and dropped.
 * Ids 2..5 (Felucca's project slots A..D) are listed empty since DUELL's MULTI store took their flash (project.c multi.c);
 * a PUT of one from an older archive is accepted (whatever its length) and dropped.
 * DUELL 0.2.14: what a PUT restores must be of this firmware's formats (project.c, multi.c, upreset.c): a project of another format
 * or revision (2), a multi object whose record is not MU_VER with P_COUNT values (2), a patch chunk with a record without PTF_CB (2) are
 * refused; nothing is written or changed then.
 * DUELL 0.2.14 (INFO 42 01 caps bit 2): ids 13..20 = OBJ_MULTI0..7 (M01-02 .. M15-16, two multis an object, magic MUL1, multi.c) and ids
 * 21..23 = OBJ_PATSLOT0..2 (P1-6, P7-12, P13-16, six slots an object, magic PTS2, patslot.c) are in the archive (27 ids). They are read
 * where they live (a multi object from flash through st_current, a pattern object from the RAM mirror ps_bank, which is the truth while a
 * pattern write is pending), never from the staging RAM (the runtime snapshot of id 0 sits there). An object with nothing in it (no multi
 * record, no used pattern slot) is listed with length 0, and a PUT of length 0 writes that empty object (the factory multis show again).
 * A pattern restore replaces the mirror and clears its dirty bit, so a pending write cannot overwrite what was restored.
 * DUELL 0.2.14 (INFO 42 01 caps bit 3): ids 24..26 = the user wavetables USR1..USR3 (wtab.c; the object is wt_obj_t, 1228 bytes, read from the RAM
 * mirror wt_ram; length 0 = EMPTY, a PUT of length 0 empties the slot). A restore is wt_store: the same checks and the same write as the
 * editor's WT_PUT (the playing sequencer is stopped first, ed_flash_stop). 30 ids.
 */
#define ED_BK_MUL0 13u
#define ED_BK_PAT0 21u
#define ED_BK_WT0 24u
static const uint8_t ED_BK_IDS[40] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
                                      27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39};
static uint8_t ed_wt_put;                       /* a user wavetable upload is staging (editor_wtab.c): they share ED_BK_RAW */
static int ed_bk_wt(uint32_t id) { return id >= ED_BK_WT0 && id < ED_BK_WT0 + WV_NU ? (int)(id - ED_BK_WT0) : -1; }
static int ed_bk_multi(uint32_t id) { return id >= ED_BK_MUL0 && id < ED_BK_MUL0 + MU_NOBJ ? (int)(id - ED_BK_MUL0) : -1; }
static int ed_bk_pat(uint32_t id) { return id >= ED_BK_PAT0 && id < ED_BK_PAT0 + PS_NOBJ ? (int)(id - ED_BK_PAT0) : -1; }
static int ed_bk_chunk(uint32_t id)             /* the PATCH store chunk an id is, -1 none */
{
    return id == 6u || id == 7u ? (int)id - 6 : id >= 9u && id <= 12u ? (int)id - 7 : id >= 27u && id <= 31u ? (int)id - 21 :
           id >= 35u && id <= 39u ? (int)id - 24 : -1;
}
#define ED_BK_N ((uint32_t)sizeof ED_BK_IDS)
#define ED_BK_MAX ((uint32_t)sizeof proj_wire_u.raw)
#define ED_BK_RAW (proj_wire_u.raw)  /* reuse the existing serialized main-loop scratch */
_Static_assert(sizeof proj_wire_u.raw >= PT_CAP, "backup staging");
static persist_t ed_bk_settings __attribute__((section(".pool")));
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
/* a PTS2 object: magic, record size and count, every record unused (used 0) or a valid slot (ps_valid), a used one below PS_SLOTS, names printable */
static int ed_bk_pat_ok(uint32_t o, const uint8_t *b, uint32_t len)
{
    const ps_obj_t *x = (const ps_obj_t *)b;
    uint32_t i, j;
    if (len != sizeof *x || x->magic != PS_MAGIC || x->rsize != sizeof(ps_rec_t) || x->nslot != PS_PER_OBJ)
        return 0;
    for (i = 0; i < PS_PER_OBJ; i++) {
        if (x->r[i].used == 0u)
            continue;
        if (o * PS_PER_OBJ + i >= PS_SLOTS || !ps_valid(&x->r[i]))
            return 0;
        for (j = 0; j < PS_NAME; j++)
            if (x->r[i].name[j] && ((uint8_t)x->r[i].name[j] < 32u || (uint8_t)x->r[i].name[j] > 126u))
                return 0;
    }
    return 1;
}
static int ed_bk_pat_empty(const ps_obj_t *x)
{
    for (uint32_t i = 0; i < PS_PER_OBJ; i++)
        if (x->r[i].used)
            return 0;
    return 1;
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
        const ps_obj_t *x = &ps_bank[ed_bk_pat(id)];
        if (!ed_bk_pat_ok((uint32_t)ed_bk_pat(id), (const uint8_t *)x, sizeof *x) || ed_bk_pat_empty(x))
            return ED_BK_RAW;
        *len = sizeof *x;
        return (const uint8_t *)x;
    }
    if (id == 0u) { *len = sizeof(project_store_t); return ED_BK_RAW; }
    if (id == 1u) { *len = sizeof ed_bk_settings; return (const uint8_t *)&ed_bk_settings; }
    if (id >= 2u && id <= 5u)                       /* the retired project slots: always empty */
        return ED_BK_RAW;
    if (ed_bk_chunk(id) >= 0) {                     /* the PATCH store's chunks 0..15: the live records, built in pt_work (length 0: none) */
        uint32_t c = (uint32_t)ed_bk_chunk(id);
        int w = pt_n[c] ? pt_build(c, UP_SLOTS, 0) : 0;
        if (w < 0)
            return ED_BK_RAW;
        *len = (uint32_t)w;
        return pt_work;
    }
    if (id == 8u)                                   /* the retired FM6 patch bank: always empty */
        return ED_BK_RAW;
    if (id >= 32u && id <= 34u)                     /* the retired user sample slots (DUELL 0.2.9): always empty */
        return ED_BK_RAW;
    return 0;
}
static uint32_t ed_bk_capture(void)
{
    if (ed_flash_stop()) return 3;
    ed_wt_put = 0;
    project_capture(&proj_scratch);
    if (!proj_pack((project_store_t *)ED_BK_RAW, &proj_scratch)) return 2;
    ed_bk_gen = ++proj_wire_gen;
#if FELUCCA_FLASH
    ed_bk_settings = persist_saved;                 /* fields absent from this build survive */
#else
    memset(&ed_bk_settings, 0, sizeof ed_bk_settings);
#endif
    settings_export(&ed_bk_settings);
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
    } else if (ed_bk_pat(ed_bk_id) >= 0) {     /* DUELL 0.2.14: a PATTERN object (six slots); length 0 = all unused */
        uint32_t o = (uint32_t)ed_bk_pat(ed_bk_id);
        if (!ed_bk_len) {
            memset(raw, 0, sizeof(ps_obj_t));
            ((ps_obj_t *)raw)->magic = PS_MAGIC;
            ((ps_obj_t *)raw)->rsize = sizeof(ps_rec_t);
            ((ps_obj_t *)raw)->nslot = PS_PER_OBJ;
            ed_bk_len = sizeof(ps_obj_t);
        } else if (!ed_bk_pat_ok(o, raw, ed_bk_len)) return 2;
#if FELUCCA_FLASH
        if (!flash_ok || st_save(OBJ_PATSLOT0 + o, raw, ed_bk_len)) return 4;
#endif
        memcpy(&ps_bank[o], raw, sizeof(ps_obj_t));  /* the mirror is the restored object; no write of the old one is pending */
        ps_obj_check(o, (int)sizeof(ps_obj_t));
        ps_dirty &= (uint8_t)~(1u << o);
        sync_reload = 1; ui.force = 1;
        return 0;
    } else if (ed_bk_chunk(ed_bk_id) >= 0) {   /* a chunk of the PATCH store: empty, or well formed */
        uint32_t off, nrec = 0;
        if (ed_bk_len && (ed_bk_len < PT_HDR || ed_bk_len > PT_CAP || pt_rd16(raw) != PT_MAGIC || raw[2] != 1u)) return 2;
        for (off = PT_HDR; ed_bk_len && off < ed_bk_len;) {
            uint32_t rl = pt_rd16(raw + off);
            if (off + PT_FIX > ed_bk_len || off + rl > ed_bk_len || !pt_rec_ok(raw + off, rl)) return 2;
            off += rl;
            nrec++;
        }
        if (nrec > PT_K) return 2;                 /* (more than PT_K records would break "all 128 slots always fit") */
        obj = pt_obj((uint32_t)ed_bk_chunk(ed_bk_id));
    } else return 1;
#if FELUCCA_FLASH
    if (!flash_ok || st_save(obj, raw, ed_bk_len)) return 4;
#else
    (void)obj;
    if (ed_bk_chunk(ed_bk_id) >= 0) {
        memcpy(pt_ram[ed_bk_chunk(ed_bk_id)], raw, ed_bk_len);
        pt_ramlen[ed_bk_chunk(ed_bk_id)] = (uint16_t)ed_bk_len;
    }
#endif
    if (ed_bk_id == 1u) {
        memcpy(&ed_bk_settings, raw, sizeof ed_bk_settings);
        settings_import(&ed_bk_settings, sizeof ed_bk_settings);
        panel_init(); settings_init(); palette_set(settings.palette);
#if FELUCCA_FLASH
        persist_saved = ed_bk_settings; persist_pending = 0;
#endif
    } else {
        pt_boot();                                  /* the directory again from all chunks (the restored one included) */
    }
    sync_reload = 1; ui.force = 1;
    return 0;
}
/* a project object's length: FUN11's (the older lengths, FUN7 .. FUN9, are no project since 0.2.14) */
static int proj_store_len(uint32_t len) { return len == sizeof(project_store_t); }
static uint32_t ed_bk_write(const uint8_t *a, uint32_t n)
{
    /* the arguments first: a malformed request never stops the transport */
    if (n < 2u || a[0] > 3u || (a[1] > 12u && ed_bk_multi(a[1]) < 0 && ed_bk_pat(a[1]) < 0 && ed_bk_wt(a[1]) < 0 && ed_bk_chunk(a[1]) < 0)) return 1;
    if (a[0] == 0u) {
        if (n != 12u || a[6] > 15u || a[11] > 15u) return 1;
        uint32_t len = ed_bk_r32(a + 2);
        if (len > ED_BK_MAX || (a[1] == 0u && !proj_store_len(len)) ||
            (a[1] == 1u && len != sizeof(persist_t)) ||
            (ed_bk_chunk(a[1]) >= 0 && len > PT_CAP) ||
            (ed_bk_multi(a[1]) >= 0 && len && (len < MU_HDR || len > MU_OBJ_MAX)) ||
            (ed_bk_pat(a[1]) >= 0 && len && len != sizeof(ps_obj_t)) ||
            (ed_bk_wt(a[1]) >= 0 && len && len != sizeof(wt_obj_t))) return 1;
        if (ed_flash_stop()) return 3;
        ed_wt_put = 0;
        ed_bk_valid = 0; ed_bk_put = 1; ed_bk_id = a[1]; ed_bk_len = len; ed_bk_gen = ++proj_wire_gen;
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
        uint32_t count = n == 8u ? (uint32_t)a[6] | (uint32_t)a[7] << 7 : 0u;
        const uint8_t *p = n ? ed_bk_object(a[0], &len) : 0;
        uint32_t rc = (!ed_bk_valid || ed_bk_usb != usb.resets || (n && !a[0] && ed_bk_gen != proj_wire_gen)) ? 5u : transport_busy() ? 3u :
            n != 8u || a[5] > 15u || !p || !count || count > 256u || off > len || count > len - off ? 1u : 0u;
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
