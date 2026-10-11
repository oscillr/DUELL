/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Complete musical archive: no raw addresses are accepted. Small objects are staged
 * in main-loop RAM and fully validated before the existing A/B commit path writes.
 * Sample restore uses SMP_BEGIN/WRITE/END and its existing CRC/header-last commit.
 * A disconnected sample restore can lose that sample; the exported file is retained.
 * DUELL: ids 6, 7, 9, 10, 11 and 12 are the six chunks of the PATCH store (upreset.c: sparse records, FM6 patches inside them), ids
 * 6, 7, 9, 10, 11, 12 = OBJ_PATCH0 .. +5 (ed_bk_chunk); an archive of Felucca's banks (another shape) is refused (2). Id 8 (the FM6 patch bank of
 * 1.0..1.0.2) is listed empty and a PUT of it is accepted and dropped.
 * Ids 2..5 (Felucca's project slots A..D) are listed empty since DUELL's MULTI store took their flash (project.c multi.c);
 * a PUT of one from an older archive is accepted and dropped. The multis are not in the archive (yet).
 */
static const uint8_t ED_BK_IDS[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 32, 33, 34};
static int ed_bk_chunk(uint32_t id)             /* the PATCH store chunk an id is, -1 none */
{
    return id == 6u || id == 7u ? (int)id - 6 : id >= 9u && id <= 12u ? (int)id - 7 : -1;
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
static const uint8_t *ed_bk_object(uint32_t id, uint32_t *len)
{
    *len = 0;
    if (id == 0u) { *len = sizeof(project_store_t); return ED_BK_RAW; }
    if (id == 1u) { *len = sizeof ed_bk_settings; return (const uint8_t *)&ed_bk_settings; }
    if (id >= 2u && id <= 5u)                       /* the retired project slots: always empty */
        return ED_BK_RAW;
    if (ed_bk_chunk(id) >= 0) {                     /* the PATCH store's chunks 0..5 */
        uint32_t c = (uint32_t)ed_bk_chunk(id);
        *len = pt_len[c];
        return pt_img[c];
    }
    if (id == 8u)                                   /* the retired FM6 patch bank: always empty */
        return ED_BK_RAW;
    if (id >= 32u && id < 32u + SMP_USER_SLOTS) {
        uint32_t k = id - 32u;
        const smp_user_hdr_t *h = (const smp_user_hdr_t *)smp_user_xip(k);
        if (usr_nz[k] && h->magic == SMP_USER_MAGIC && h->data_len <= SMP_USER_CAP(k) - SMP_USER_DATA)
            *len = SMP_USER_DATA + h->data_len;
        return smp_user_xip(k);
    }
    return 0;
}
static uint32_t ed_bk_capture(void)
{
    if (ed_flash_stop()) return 3;
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
        if (ed_bk_len) {                            /* an older format becomes FUN11 inside its ranges */
            proj_bound(&proj_scratch);
            if (!proj_pack((project_store_t *)raw, &proj_scratch)) return 2;
            ed_bk_len = sizeof(project_store_t);
        }
        if (!ed_bk_len) return 2;
        return project_restore_runtime(&proj_scratch) ? 2u : 0u;
    } else if (ed_bk_id == 1u) {
        const persist_t *p = (const persist_t *)raw;
        if (ed_bk_len != sizeof *p || p->magic != PERSIST_MAGIC || !palette_stored_ok(p->palette) ||
            p->lowcut > 2u || !leds_stored_ok(p->zoom) || !hold_stored_ok(p->bold) || p->favorites.filter > 1u || !ed_bk_panel_valid(&p->panel)) return 2;
        obj = OBJ_SETTINGS;
    } else if (ed_bk_chunk(ed_bk_id) >= 0) {   /* a chunk of the PATCH store: empty, or well formed */
        uint32_t off;
        if (ed_bk_len && (ed_bk_len < PT_HDR || ed_bk_len > PT_CAP || pt_rd16(raw) != PT_MAGIC || raw[2] != 1u)) return 2;
        for (off = PT_HDR; ed_bk_len && off < ed_bk_len;) {
            uint32_t rl = pt_rd16(raw + off);
            if (off + PT_FIX > ed_bk_len || off + rl > ed_bk_len || !pt_rec_ok(raw + off, rl)) return 2;
            off += rl;
        }
        obj = OBJ_PATCH0 + (uint32_t)ed_bk_chunk(ed_bk_id);
    } else return 1;
#if FELUCCA_FLASH
    if (!flash_ok || st_save(obj, raw, ed_bk_len)) return 4;
#else
    (void)obj;
#endif
    if (ed_bk_id == 1u) {
        memcpy(&ed_bk_settings, raw, sizeof ed_bk_settings);
        settings_import(&ed_bk_settings, sizeof ed_bk_settings);
        panel_init(); settings_init(); palette_set(settings.palette);
#if FELUCCA_FLASH
        persist_saved = ed_bk_settings; persist_pending = 0;
#endif
    } else {
        uint32_t c = (uint32_t)ed_bk_chunk(ed_bk_id);
        if (ed_bk_len) memcpy(pt_img[c], raw, ed_bk_len);
        pt_len[c] = (uint16_t)ed_bk_len;
        pt_scan(); up_gen++;
    }
    sync_reload = 1; ui.force = 1;
    return 0;
}
/* a project object's length: FUN10 (today's, 1.2), FUN9 (1.1), FUN8 (before the DRUM lane levels, 1.0.x), FUN7 */
static int proj_store_len(uint32_t len)
{
    return len == sizeof(project_store_t) || len == PROJ_STORE_V9 || len == PROJ_STORE_V8 || len == PROJ_STORE_V7;
}
static uint32_t ed_bk_write(const uint8_t *a, uint32_t n)
{
    /* the arguments first: a malformed request never stops the transport */
    if (n < 2u || a[0] > 3u || a[1] > 12u) return 1;
    if (a[0] == 0u) {
        if (n != 12u || a[6] > 15u || a[11] > 15u) return 1;
        uint32_t len = ed_bk_r32(a + 2);
        if (len > ED_BK_MAX || (a[1] == 0u && !proj_store_len(len)) ||
            (a[1] == 1u && len != sizeof(persist_t)) ||
            (a[1] >= 2u && a[1] <= 5u && len && !proj_store_len(len)) ||
            (ed_bk_chunk(a[1]) >= 0 && len > PT_CAP)) return 1;
        if (ed_flash_stop()) return 3;
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
