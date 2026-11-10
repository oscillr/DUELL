/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The editor's full backup (editor_backup.c: LIST / GET / PUT) against simulated NOR flash:
 * CRC before any write, stale runtime copies, USB resets and timeouts, malformed objects, older
 * project formats, settings values and user preset banks kept byte for byte. The cloud plan's job A: the PATCH store is carried one patch at a time as LOGICAL
 * records (object id 40 plus a slot byte, LIST pages), the same bytes PATCH_RAW moves: 128 patches (a part of them with the extension slots and the hand-set B
 * edit list) read, the store erased, every record written back through PUT: the same patches, the same bytes; refusals leave the store as it was. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static void lcd_sync(void) {}
static void lcd_power(uint32_t s) { (void)s; }   /* (MENU > SCREEN OFF: lcd.c) */
static void lcd_wake_now(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#define FELUCCA_FLASH 1
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
static void panel_setup(void) {}

static uint8_t nor[0x100000], flash_ok = 1;
static int erase_error;
static uint32_t erases;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { if (erase_error) return -8; memset(nor + off, 0xFF, 4096); erases++; return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { memcpy(nor + off, src, n); return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0; }
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"
#include "old_pack.h"

enum { ED_BACKUP_LIST = 65, ED_BACKUP_GET, ED_BACKUP_PUT };
static uint8_t rep[4096];
static uint32_t rep_n;
static void ed_b(uint32_t v) { if (rep_n < sizeof rep) rep[rep_n++] = (uint8_t)(v & 127u); }
static uint32_t ed_unpack7(const uint8_t *a, uint32_t na, uint8_t *out, uint32_t max)
{
    uint32_t n = 0;
    while (na && n < max) {
        uint32_t m = *a++, j;
        na--;
        uint32_t k = na > 7u ? 7u : na;
        if (!k || n + k > max || (m >> k))
            return 0;
        for (j = 0; j < k; j++, na--)
            out[n++] = (uint8_t)(*a++ | ((m >> j) & 1u) << 7);
    }
    return na ? 0u : n;
}
static uint8_t ed_smp_buf[512] __attribute__((aligned(4)));
static void fm1_wdt_feed(void) {}
static int ed_flash_stop(void) { return transport_busy(); }
#include "../firmware/src/editor_backup.c"

static int check(const char *what, int ok)
{
    printf("backup: %-72s %s\n", what, ok ? "ok" : "FAIL");
    return !ok;
}

static void reset(void)
{
    memset(nor, 0xFF, sizeof nor);
    memset(&song, 0, sizeof song);
    memset(trk, 0, sizeof trk);
    memset(&persist_saved, 0, sizeof persist_saved);
    memset(&settings, 0, sizeof settings);
    memset(&ui, 0, sizeof ui);
    panel = PANEL_DEFAULT;
    settings_init();
    host_tracks_init();
    fm1_ms = 0;
    transport_req = 0;
    usb.resets = 0;
    erase_error = 0;
    erases = 0;
    ed_bk_valid = ed_bk_put = 0;
}

static uint32_t call(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    rep_n = 0;
    ed_backup_handle(cmd, a, n);
    return rep_n;
}
static void put32(uint8_t *a, uint32_t v) { for (uint32_t i = 0; i < 5u; i++) a[i] = (uint8_t)((v >> (7u * i)) & 127u); }

/* PUT begin: rc */
static uint32_t put_begin(uint32_t id, uint32_t len, uint32_t crc)
{
    uint8_t a[12] = {0, (uint8_t)id};
    put32(a + 2, len);
    put32(a + 7, crc);
    call(ED_BACKUP_PUT, a, sizeof a);
    return rep[2];
}
/* PUT one chunk of data at off: rc */
static uint32_t put_chunk(uint32_t id, uint32_t off, const uint8_t *p, uint32_t n)
{
    uint8_t a[16 + 300];
    uint32_t k = 7;
    a[0] = 1;
    a[1] = (uint8_t)id;
    put32(a + 2, off);
    while (n) {
        uint32_t g = n > 7u ? 7u : n, m = 0, i;
        for (i = 0; i < g; i++) m |= (uint32_t)(p[i] >> 7) << i;
        a[k++] = (uint8_t)m;
        for (i = 0; i < g; i++) a[k++] = p[i] & 127u;
        p += g;
        n -= g;
    }
    call(ED_BACKUP_PUT, a, k);
    return rep[2];
}
static uint32_t put_end(uint32_t id, uint32_t op)   /* op 2 commit, 3 abort */
{
    uint8_t a[2] = {(uint8_t)op, (uint8_t)id};
    call(ED_BACKUP_PUT, a, 2);
    return rep[2];
}
/* a whole object: begin, 256-byte chunks, commit; the first failing rc or the commit's */
static uint32_t put_all(uint32_t id, const void *v, uint32_t len, uint32_t crc)
{
    const uint8_t *p = v;
    uint32_t off, rc = put_begin(id, len, crc);
    for (off = 0; !rc && off < len; off += 256u)
        rc = put_chunk(id, off, p + off, len - off > 256u ? 256u : len - off);
    return rc ? rc : put_end(id, 2);
}
/* LIST: rc; the length and CRC of object id */
static uint32_t list(uint32_t id, uint32_t *len, uint32_t *crc)
{
    uint32_t i;
    call(ED_BACKUP_LIST, 0, 0);
    if (rep[1])
        return rep[1];
    for (i = 0; i < rep[2]; i++) {
        const uint8_t *o = rep + 3 + i * 11u;
        if (o[0] == id) {
            *len = ed_bk_r32(o + 1);
            *crc = ed_bk_r32(o + 6);
        }
    }
    return 0;
}
static uint32_t get(uint32_t id, uint32_t off, uint32_t count)
{
    uint8_t a[8] = {(uint8_t)id};
    put32(a + 1, off);
    a[6] = (uint8_t)(count & 127u);
    a[7] = (uint8_t)(count >> 7);
    call(ED_BACKUP_GET, a, sizeof a);
    return rep[1];
}

/* the patch records (object 40 + a slot byte, editor_backup.c) */
static uint32_t list_page(uint32_t pg)               /* rc; the count and the entries (slot, length, CRC) are in rep */
{
    uint8_t a[1] = {(uint8_t)pg};
    call(ED_BACKUP_LIST, a, 1);
    return rep[1];
}
static uint32_t get_patch(uint32_t slot, uint32_t off, uint32_t count)
{
    uint8_t a[9] = {ED_BK_PATCH};
    put32(a + 1, off);
    a[6] = (uint8_t)(count & 127u);
    a[7] = (uint8_t)(count >> 7);
    a[8] = (uint8_t)slot;
    call(ED_BACKUP_GET, a, sizeof a);
    return rep[1];
}
static uint32_t put_begin_patch(uint32_t slot, uint32_t len, uint32_t crc)
{
    uint8_t a[13] = {0, ED_BK_PATCH};
    put32(a + 2, len);
    put32(a + 7, crc);
    a[12] = (uint8_t)slot;
    call(ED_BACKUP_PUT, a, sizeof a);
    return rep[2];
}
static uint32_t put_patch(uint32_t slot, const uint8_t *r, uint32_t len, uint32_t crc)   /* begin, pieces, commit: the first failing rc or the commit's */
{
    uint32_t off, rc = put_begin_patch(slot, len, crc);
    for (off = 0; !rc && off < len; off += 256u)
        rc = put_chunk(ED_BK_PATCH, off, r + off, len - off > 256u ? 256u : len - off);
    return rc ? rc : put_end(ED_BK_PATCH, 2);
}
static uint32_t pt_bytes_total(void)
{
    uint32_t c, n = 0;
    for (c = 0; c < PT_NCH; c++)
        n += pt_len[c];
    return n;
}

int main(void)
{
    int bad = 0;
    uint32_t len = 0, crc = 0, before;
    static project_store_t st;
    persist_t ps;

    reset();
    trk[0].step[0] = (step_t){{60}, 1, ST_NOTE, 0, 96, 0, 0};
    bad += check("LIST captures the runtime: the 24 other objects (ED_BK_N: ids 0..5 8 empty ones, 13..20 the multis, 21..23 the pattern slots, 24..26 the user wavetables, 32..34; the patches are paged), runtime 3840 B (FUN11)",
                 list(0, &len, &crc) == 0 && rep[2] == ED_BK_N && ED_BK_N == 24u && len == sizeof(project_store_t) && len == 3840u &&
                 crc == st_crc32(ED_BK_RAW, len));
    bad += check("an empty project slot lists as length 0", list(2, &len, &crc) == 0 && len == 0);
    bad += check("GET of the runtime copy", get(0, 0, 64) == 0);
    project_save(0);
    bad += check("a project save after LIST reused the staging RAM: GET of the runtime is stale (5)",
                 get(0, 0, 64) == 5u);
    bad += check("a new LIST makes it readable again", list(0, &len, &crc) == 0 && get(0, 0, 64) == 0);
    usb.resets++;
    bad += check("a USB reset after LIST: GET is stale (5)", get(2, 0, 16) == 5u);

    /* settings */
    reset();
    memset(&ps, 0, sizeof ps);
    settings_export(&ps);
    ps.rsv = 2;                                          /* (the retired OUTPUT EQ field: whatever it holds is accepted and ignored since 0.2.19) */
    ps.palette = palette_to_stored(6);
    bad += check("settings with a retired OUTPUT EQ value restore", put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 0 &&
                 settings.rsv == 0u && settings.palette == 6u);
    ps.palette = 13;                                     /* an older backup: its PAPER (old id 13, Felucca's palettes; no longer read) */
    bad += check("settings of an older backup (an untagged palette id) are refused (rc 2, 0.2.14), the theme stays",
                 put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 2u && settings.palette == 6u);
    ps.palette = palette_to_stored(6);
    before = erases;
    ps.rsv = 0;
    ps.bold = hold_to_stored(0, 3);                      /* HOLD 0.6 s */
    bad += check("settings with HOLD 0.6 s restore",
                 put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 0 && HOLD_MS[settings_hold] == 600u);
    ps.bold = 1;                                         /* an older backup (its font weight): HOLD 0.4 s */
    bad += check("settings of an older backup restore with HOLD 0.4 s",
                 put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 0 && HOLD_MS[settings_hold] == 400u);
    before = erases;
    ps.bold = HOLD_TAG + 7u;
    bad += check("settings with an unknown HOLD value are refused, nothing written",
                 put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 2u && erases == before);
    ps.bold = 0;
    ps.zoom = LEDS_TAG | LEDS_INV;                       /* LEDS MAX */
    bad += check("settings with LEDS MAX restore",
                 put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 0 && settings_leds == LEDS_INV);
    ps.zoom = 1;                                         /* an older backup (its large readout): LEDS DIM */
    bad += check("settings of an older backup restore with LEDS DIM",
                 put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 0 && settings_leds == LEDS_DIM);
    before = erases;
    ps.zoom = LEDS_TAG | LEDS_DIM_LO;                    /* LEDS LOW, OFF (appended) */
    bad += check("settings with LEDS LOW restore",
                 put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 0 && settings_leds == LEDS_DIM_LO);
    ps.zoom = LEDS_TAG | LEDS_OFF;
    bad += check("settings with LEDS OFF restore",
                 put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 0 && settings_leds == LEDS_OFF);
    before = erases;
    ps.zoom = LEDS_TAG + 4u;
    bad += check("settings with an unknown LEDS value are refused, nothing written",
                 put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 2u && erases == before);
    ps.zoom = 0;
    bad += check("a wrong CRC is refused before any write",
                 put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps) ^ 1u) == 2u && erases == before);

    /* malformed requests */
    bad += check("an unknown object id is refused", put_begin(24, sizeof ps, 0) == 1u && put_begin(41, 0, 0) == 1u && put_begin(40, 0, 0) == 1u);
    bad += check("a project of the wrong length is refused", put_begin(0, sizeof(project_store_t) - 4u, 0) == 1u && put_begin(0, 3648u, 0) == 1u);
    bad += check("a project slot of an older archive (ids 2..5) is accepted whatever its length, and dropped at commit",
                 put_begin(3, 3648u, 0) == 0u && (ed_bk_put = 0, 1));
    bad += check("settings of the wrong length are refused", put_begin(1, sizeof ps + 4u, 0) == 1u);
    bad += check("a chunk without a begin is refused (5)", (ed_bk_put = 0, put_chunk(1, 0, (const uint8_t *)&ps, 16)) == 5u);
    put_begin(1, sizeof ps, st_crc32(&ps, sizeof ps));
    bad += check("a chunk at the wrong offset is refused", put_chunk(1, 16, (const uint8_t *)&ps, 16) == 1u);
    bad += check("a chunk for another object is refused (5)", put_chunk(2, 0, (const uint8_t *)&ps, 16) == 5u);
    {
        uint32_t off = 0, step = sizeof ps - 8u;
        put_begin(1, sizeof ps, st_crc32(&ps, sizeof ps));
        for (; off + 256u <= step; off += 256u)
            put_chunk(1, off, (const uint8_t *)&ps + off, 256);
        if (off < step) put_chunk(1, off, (const uint8_t *)&ps + off, step - off);
        bad += check("a chunk past the length is refused", put_chunk(1, step, (const uint8_t *)&ps, 16) == 1u);
    }
    bad += check("commit before all bytes arrived is refused", put_end(1, 2) == 2u && erases == before);
    put_begin(1, sizeof ps, st_crc32(&ps, sizeof ps));
    put_chunk(1, 0, (const uint8_t *)&ps, 64);
    usb.resets++;
    bad += check("a USB reset during PUT ends it (5)", put_chunk(1, 64, (const uint8_t *)&ps + 64, 64) == 5u);
    put_begin(1, sizeof ps, st_crc32(&ps, sizeof ps));
    fm1_ms += 16000u;
    bad += check("16 s without a chunk ends the PUT (5)", put_chunk(1, 0, (const uint8_t *)&ps, 64) == 5u);
    put_begin(2, sizeof st, 0);
    put_chunk(2, 0, (const uint8_t *)&ps, 64);
    project_save(1);
    bad += check("a project save during PUT (the staging RAM) ends it (5)",
                 put_chunk(2, 64, (const uint8_t *)&ps, 64) == 5u);
    transport_req = 1;
    bad += check("PUT while starting PLAY is refused (3)", put_begin(1, sizeof ps, 0) == 3u);
    transport_req = 0;

    /* DUELL: ids 2..5 (Felucca's project slots) and 8 (the retired FM6 patch bank) are listed empty; a PUT of one from an older
     * archive is accepted (0) and dropped; ids 6, 7, 9 are the PATCH store's chunks, an archive of Felucca's banks (another
     * shape) is refused (2) (editor_backup.c) */
    reset();
    trk[1].step[3] = (step_t){{64}, 1, ST_NOTE, 0, 96, 0, 0};
    project_capture(&proj_scratch);
    proj_pack(&st, &proj_scratch);
    {
        uint32_t i, ok = 1;
        for (i = 2; i <= 5u; i++)
            ok &= put_all(i, &st, sizeof st, st_crc32(&st, sizeof st)) == 0 && list(i, &len, &crc) == 0 && len == 0;
        bad += check("a project slot of an older archive (ids 2..5): accepted (0) and dropped, listed empty", ok && !erases);
    }
    {
        static uint8_t old_bank[3840];
        memset(old_bank, 0x5A, sizeof old_bank);
        bad += check("an old archive's FM6 patch bank (id 8): accepted and dropped, listed empty",
                     put_all(8, old_bank, 1024, st_crc32(old_bank, 1024)) == 0 && list(8, &len, &crc) == 0 && len == 0);
        bad += check("the retired user sample slots (ids 32..34, DUELL 0.2.9): listed empty",
                     list(32, &len, &crc) == 0 && len == 0 && list(33, &len, &crc) == 0 && len == 0 && list(34, &len, &crc) == 0 &&
                     len == 0);
        bad += check("the physical chunk ids of before (6 7 9..12 27..31 35..39: Felucca's preset banks, then the patch store's chunks) are refused (1): no such object any more",
                     put_all(6, old_bank, 3000u, st_crc32(old_bank, 3000u)) == 1u && put_all(7, old_bank, 3000u, st_crc32(old_bank, 3000u)) == 1u &&
                     put_begin(9, 100u, 0) == 1u && put_begin(12, 100u, 0) == 1u && put_begin(27, 100u, 0) == 1u && put_begin(31, 100u, 0) == 1u && put_begin(35, 3840u, 0) == 1u && put_begin(39, 100u, 0) == 1u);
        bad += check("a bare id 40 (no slot byte) is refused (1); ids 24..26 (the user wavetables) list empty on a fresh unit",
                     put_begin(40, 0, 0) == 1u && list(24, &len, &crc) == 0 && len == 0 && list(26, &len, &crc) == 0 && len == 0);
    }


    /* DUELL 0.2.14: the multis (ids 13..20 = OBJ_MULTI0..7) and the pattern slots (ids 21..23 = OBJ_PATSLOT0..2) are in the archive */
    {
        static uint8_t m_a[3840], p_a[3584], m_b[3840], got[3840];
        uint32_t ml = 0, mc = 0, pl = 0, pc = 0, ml2 = 0, mc2 = 0, off, e0, ok2;
        uint8_t *po = got;
        uint32_t at2;
        reset();
        multi_boot(); ps_boot();
        bad += check("empty multi objects list as length 0 (13..20); the pattern objects are never empty: the 32 defaults (21..23: 11 + 11 + 10 slots, PTS3)",
                     list(13, &len, &crc) == 0 && len == 0 && list(20, &len, &crc) == 0 && len == 0 &&
                     list(21, &len, &crc) == 0 && len == ps_used_len(0) && len > 400u && list(23, &len, &crc) == 0 && len == ps_used_len(2) && len > 400u);
        trk[0].step[0] = (step_t){{62}, 1, ST_NOTE, 0, 96, 0, 0};
        trk[0].p[P_SLEN] = 12;
        project_save_as(2, "TESTONE");                      /* M03: object 1, record 0 */
        project_save_as(3, "TESTTWO");                      /* M04: object 1, record 1 */
        project_save_as(9, "OTHERNINE");                    /* M10: object 4 */
        ps_save_trk(&trk[0], 2, "PAT THREE");               /* P3: object 0, still a pending write (ps_dirty) */
        bad += check("a saved multi lists in its object (id 14), a pattern in id 21 (the RAM mirror: still dirty)",
                     list(14, &ml, &mc) == 0 && ml > 8u && ml < 3840u && list(21, &pl, &pc) == 0 && pl == ps_used_len(0) && pl > 400u && pl < 3584u && (ps_dirty & 1u) &&
                     list(22, &len, &crc) == 0 && len == ps_used_len(1) && len > 400u);
        for (off = 0; off < ml; off += 256u) {
            uint32_t n = ml - off > 256u ? 256u : ml - off;
            uint8_t a[8] = {14};
            put32(a + 1, off);
            a[6] = (uint8_t)(n & 127u); a[7] = (uint8_t)(n >> 7);
            call(ED_BACKUP_GET, a, sizeof a);
            ed_unpack7(rep + 9, rep_n - 9u, m_a + off, n);
        }
        bad += check("GET of a multi object in pieces: the bytes are the flash object (MUL1, CRC as listed)",
                     st_crc32(m_a, ml) == mc && !memcmp(m_a, "MUL1", 4));
        bad += check("GET of the runtime id 0 still works after multis were read (no staging clash)", list(0, &len, &crc) == 0 && get(0, 0, 64) == 0 &&
                     get(14, 0, 64) == 0 && get(0, 0, 64) == 0);
        memcpy(p_a, &ps_bank[0], pl);
        at2 = ps_at[2];
        bad += check("a pattern object GET is the RAM mirror (CRC as listed)", st_crc32(p_a, pl) == pc && !memcmp(p_a, "PTS3", 4));
        bad += check("only the objects with a stored multi list a length (M10 = object 4, id 17; object 3, id 16: none)",
                     list(17, &ml2, &mc2) == 0 && ml2 > 8u && list(16, &len, &crc) == 0 && len == 0);
        bad += check("an id past the user wavetables, and a bad size at begin, are refused (1)",
                     put_begin(41, 0, 0) == 1u && put_begin(24, 5, 0) == 1u && put_begin(24, 1229, 0) == 1u && put_begin(14, 5, 0) == 1u && put_begin(14, 3841, 0) == 1u &&
                     put_begin(21, 7, 0) == 1u && put_begin(21, 3585, 0) == 1u && put_begin(22, 3585, 0) == 1u);
        /* a restore into a unit that lost them: reset, then PUT the two objects */
        memcpy(m_b, m_a, ml);
        reset();
        multi_boot(); ps_boot();
        bad += check("restore: the multi object (id 14) commits", put_all(14, m_b, ml, mc) == 0);
        bad += check("restore: M03 and M04 are back by name, used (names, mask updated at once)",
                     project_used(2) && project_used(3) && !strcmp(mu_names[2], "TESTONE") && !strcmp(mu_names[3], "TESTTWO") &&
                     project_factory(9) && list(14, &len, &crc) == 0 && len == ml && crc == mc);
        bad += check("restore: the multi LOADs (its part A step 0)", (host_tracks_init(), project_load(2), trk[0].step[0].note[0] == 62u));
        ps_save_trk(&trk[1], 5, "NEWER");                   /* P6 now in the mirror, dirty: the restore must win over it */
        bad += check("restore: the pattern object (id 21) commits over a dirty mirror", (ps_dirty & 1u) && put_all(21, p_a, pl, pc) == 0 && !(ps_dirty & 1u));
        ps_poll();
        {
            int n = st_load(OBJ_PATSLOT0, got, sizeof got);
            bad += check("restore: ps_poll writes nothing over it; flash and mirror are the restored object, P3 is back by name, P6 is its default again",
                         n == (int)pl && st_crc32(got, pl) == pc && !memcmp(&ps_bank[0], p_a, pl) && ps_changed(2) && !ps_changed(5));
        }
        {
            char nm[13];
            ps_name(2, nm);
            bad += check("restore: P3 loads onto a part (LEN 12, note 62), name PAT THREE",
                         !strcmp(nm, "PAT THREE") && (trk[1].p[P_SLEN] = 0, ps_load_trk(&trk[1], 2) == 0) && trk[1].p[P_SLEN] == 12 && trk[1].step[0].note[0] == 62u);
        }
        bad += check("a verify LIST after the restore: both objects equal the file's size and CRC",
                     list(14, &len, &crc) == 0 && len == ml && crc == mc && list(21, &len, &crc) == 0 && len == pl && crc == pc);
        /* refusals leave everything as it was */
        e0 = erases; ok2 = 1;
        memcpy(got, m_b, ml);
        got[0] ^= 1;                                        /* magic */
        ok2 &= put_all(14, got, ml, st_crc32(got, ml)) == 2u;
        memcpy(got, m_b, ml);
        got[4] ^= 1;                                        /* a record length that no longer adds up */
        ok2 &= put_all(14, got, ml, st_crc32(got, ml)) == 2u;
        memcpy(got, m_b, ml);
        got[8] = 9;                                         /* a record version the loader does not know */
        ok2 &= put_all(14, got, ml, st_crc32(got, ml)) == 2u;
        memcpy(got, m_b, ml);
        got[8 + 8] = 7;                                     /* a name character below a space */
        ok2 &= put_all(14, got, ml, st_crc32(got, ml)) == 2u;
        ok2 &= put_all(14, m_b, ml, mc ^ 1u) == 2u;         /* a wrong CRC */
        bad += check("bad multi objects (magic, lengths, version, name, CRC) are refused at commit (2), nothing written",
                     ok2 && erases == e0 && project_used(2));
        memcpy(got, p_a, pl);
        got[0] ^= 1u;                                       /* magic */
        ok2 = put_all(21, got, pl, st_crc32(got, pl)) == 2u;
        memcpy(got, p_a, pl);
        got[at2 + 3] = 0;                                   /* LEN out of range */
        ok2 &= put_all(21, got, pl, st_crc32(got, pl)) == 2u;
        memcpy(got, p_a, pl);
        got[at2 + 8] = 3;                                   /* an unprintable name character */
        ok2 &= put_all(21, got, pl, st_crc32(got, pl)) == 2u;
        memcpy(got, p_a, pl);
        got[at2 + 2] = 4;                                   /* a flag bit that is nobody's */
        ok2 &= put_all(21, got, pl, st_crc32(got, pl)) == 2u;
        memcpy(got, p_a, pl);
        got[6] = 10;                                        /* the wrong number of slots for the object */
        ok2 &= put_all(21, got, pl, st_crc32(got, pl)) == 2u;
        memcpy(got, p_a, pl);
        ok2 &= put_all(23, got, pl, st_crc32(got, pl)) == 2u;   /* object 0's bytes in object 2's place (11 slots, not 10) */
        memcpy(got, p_a, pl);
        ok2 &= put_all(21, got, pl - 1u, st_crc32(got, pl - 1u)) == 2u;   /* cut short */
        bad += check("bad pattern objects (magic, LEN, name, flag bits, slot count, another object's, cut) are refused at commit (2), nothing written",
                     ok2 && erases == e0 && ps_changed(2));
        (void)po;
        transport_req = 1;
        bad += check("PUT of a multi or pattern while PLAY starts is refused (3), as for the others", put_begin(14, ml, mc) == 3u && put_begin(21, pl, pc) == 3u);
        transport_req = 0;
        /* an empty object restores as empty: the factory multi shows again, the pattern slots are unused */
        reset();
        multi_boot(); ps_boot();
        project_save_as(0, "MINE");
        bad += check("before: M01 holds a saved multi, not the factory one", project_used(0) && !project_factory(0));
        bad += check("restore: length 0 for id 13 empties M01 / M02 (the factory multi shows again)",
                     put_all(13, "", 0, 0) == 0 && project_factory(0) && list(13, &len, &crc) == 0 && len == 0);
        ps_save_trk(&trk[0], 0, "X");
        bad += check("restore: length 0 for id 21 gives its slots their defaults again (object listed, written at the next quiet pass)",
                     put_all(21, "", 0, 0) == 0 && !ps_changed(0) && (ps_dirty & 1u) && list(21, &len, &crc) == 0 && len == ps_used_len(0) && len > 400u);
    }

    /* the runtime */
    reset();
    trk[1].step[5] = (step_t){{67}, 1, ST_NOTE, 0, 96, 0, 0};   /* (DUELL: part B; FUN11 keeps parts A and B) */
    trk[1].p[P_LEVEL] = 77;
    project_capture(&proj_scratch);
    proj_pack(&st, &proj_scratch);
    host_tracks_init();
    bad += check("the runtime object restores the parts",
                 put_all(0, &st, sizeof st, st_crc32(&st, sizeof st)) == 0 && trk[1].step[5].note[0] == 67u &&
                 trk[1].p[P_LEVEL] == 77);

    /* the PATCH store, one patch at a time: object 40 + a slot byte. 128 patches (a part with the extension slots and the hand-set B edits) are listed in pages,
     * read in pieces, the store is erased and every record written back through PUT: the same patches, the same bytes */
    {
        static uint8_t rec[UP_SLOTS][PT_WORST + 8], got[PT_WORST + 8];
        static uint32_t rlen[UP_SLOTS], rcrc[UP_SLOTS];
        static up_rec_t was[UP_SLOTS];
        static xb_edit_t ed[XB_MAX];
        uint32_t k, i, ok = 1, pg, listed = 0, bytes = 0, same = 0, xbn = 0, e0;
        char nm[13], want[13];
        reset();
        set_engine_of(&trk[0], 0u);
        apply_preset_to(&trk[0], 2u);
        for (k = 0; k < UP_SLOTS; k++) {
            char nmx[8] = {'B', (char)('0' + k / 10u % 10u), (char)('0' + k % 10u), 0};
            for (i = 0; i < NX; i++)
                XV(&trk[0], i) = (int16_t)(k % 5u == 0u ? (int)(k + 3u * i) % 120 - 50 : (int)xb_def(i) - 64);   /* every 5th slot: slots off their defaults */
            for (xbn = 0; k % 7u == 0u && xbn < (k % 14u == 0u ? XB_MAX : 4u); xbn++)
                ed[xbn] = (xb_edit_t){(uint8_t)(xbn < 31u ? xbn * 3u : 140u), (int16_t)(xbn * 2 - 40 + (int)(k % 9u))};
            up_xb_in = xbn ? ed : 0;
            up_xb_nin = xbn;
            ok &= up_store_trk(&trk[0], k, nmx, 0) == 0;
            up_xb_in = 0;
            up_xb_nin = 0;
            was[k] = *up_rec(k);
            rlen[k] = up_raw_get(k, rec[k]);
            rcrc[k] = st_crc32(rec[k], rlen[k]);
            bytes += rlen[k];
        }
        bad += check("128 patches saved (one per slot, every 5th with the 16 slots off default, every 7th with a hand-set B list, every 14th with 32 edits)", ok && up_count() == UP_SLOTS);
        bad += check("LIST pages: before the capture LIST a page is stale (5); page 0 and 5 are invalid (1)",
                     (ed_bk_valid = 0, list_page(1) == 5u) && (call(ED_BACKUP_LIST, 0, 0), list_page(0) == 1u && list_page(5) == 1u));
        for (pg = 1; pg <= 4u; pg++) {
            ok = list_page(pg) == 0 && rep[2] == ED_BK_PPAGE;
            for (i = 0; ok && i < rep[2]; i++) {
                const uint8_t *o = rep + 3 + i * 11u;
                k = o[0];
                ok &= k >= (pg - 1u) * ED_BK_PPAGE && k < pg * ED_BK_PPAGE && ed_bk_r32(o + 1) == rlen[k] && ed_bk_r32(o + 6) == rcrc[k];
                listed++;
            }
            bad += check("  a LIST page: its 32 slots with the length and CRC of each record", ok);
        }
        bad += check("  4 pages = 128 records listed", listed == UP_SLOTS);
        for (k = 0, ok = 1; k < UP_SLOTS; k++) {
            uint32_t off;
            for (off = 0; ok && off < rlen[k]; off += 256u) {
                uint32_t n = rlen[k] - off > 256u ? 256u : rlen[k] - off;
                ok &= get_patch(k, off, n) == 0 && ed_unpack7(rep + 9, rep_n - 9u, got + off, 256u) == n;
            }
            ok &= !memcmp(got, rec[k], rlen[k]) && st_crc32(got, rlen[k]) == rcrc[k];
        }
        bad += check("GET: every record read in pieces equals the stored record (slot and revision bytes zero), CRC as listed", ok);
        printf("backup: 128 patches: %u bytes of logical records (the physical chunks held %u)\n", bytes, pt_bytes_total());
        bad += check("GET of a slot with no record, a slot past 127, a bad count, a missing slot byte: refused (1)",
                     (up_put(9, 0), get_patch(9, 0, 16)) == 1u && get_patch(128, 0, 16) == 1u && get_patch(3, 0, 0) == 1u && get_patch(3, rlen[3] - 1u, 2) == 1u && get_patch(3, 0, 16) == 0u);
        up_store_trk(&trk[0], 9, "B09", 0);
        {   /* the patches not in a page: a slot that holds its factory sound is not listed */
            up_put(9, 0);
            call(ED_BACKUP_LIST, 0, 0);
            ok = list_page(1) == 0 && rep[2] == ED_BK_PPAGE - 1u;
            bad += check("a slot with no record (its factory sound) is not listed", ok);
        }
        for (k = 0; k < UP_SLOTS; k++)
            up_put(k, 0);
        bad += check("  the store erased: no saved patch, every slot its factory sound again", up_count() == 0u && !up_used(5u));
        for (k = 0, ok = 1; k < UP_SLOTS; k++)
            ok &= k == 9u || put_patch(k, rec[k], rlen[k], rcrc[k]) == 0;
        same = 0;
        for (k = 0; k < UP_SLOTS; k++) {
            want[0] = 'B'; want[1] = (char)('0' + k / 10u % 10u); want[2] = (char)('0' + k % 10u); want[3] = 0;
            up_name(k, nm);
            same += up_used(k) && !strcmp(nm, want) && up_raw_get(k, got) == rlen[k] && !memcmp(got, rec[k], rlen[k]);
        }
        bad += check("  every record restored by PUT (slot 9 left out): the same patches, names, slots and bytes", ok && same == UP_SLOTS - 1u && up_count() == UP_SLOTS - 1u && !up_used(9));
        for (k = 0, ok = 1; k < UP_SLOTS; k++)
            if (k != 9u) {
                const up_rec_t *r = up_rec(k);
                ok &= !memcmp(r->xz, was[k].xz, sizeof r->xz) && r->xb_n == was[k].xb_n && !memcmp(r->xb, was[k].xb, was[k].xb_n * sizeof r->xb[0]);
            }
        bad += check("  the extension slots and the hand-set B lists of every record are back (up_rec)", ok);
        for (k = 0, ok = 1; k < UP_SLOTS; k++)
            if (k != 9u) {
                uint8_t ex[PT_WORST + 8];
                ok &= put_patch(k, rec[k], rlen[k], rcrc[k]) == 0 && up_raw_get(k, ex) == rlen[k] && !memcmp(ex, rec[k], rlen[k]);
            }
        bad += check("  and a second restore over the first (a newer revision each) leaves the same bytes", ok && up_count() == UP_SLOTS - 1u);
        /* the records go where the store puts them: into slots other than their own too (the bytes carry no slot) */
        bad += check("  a record into another slot than it came from: the same bytes there", put_patch(9, rec[40], rlen[40], rcrc[40]) == 0 && up_raw_get(9, got) == rlen[40] && !memcmp(got, rec[40], rlen[40]));
        up_put(9, 0);
        /* refusals */
        e0 = erases;
        {
            uint8_t bad_rec[PT_WORST + 8];
            uint32_t xbo, mb, pr, pat, fm, kx = 70u;                 /* slot 70: 70 % 14 == 0, 32 edits */
            pt_layout(rec[kx], &xbo, &mb, &pr, &pat, &fm);
            ok = (rec[kx][4] & PTF_XB) != 0;
            memcpy(bad_rec, rec[kx], rlen[kx]);
            bad_rec[xbo] += 1u;                                        /* the block's length no longer adds up */
            ok &= put_patch(kx, bad_rec, rlen[kx], st_crc32(bad_rec, rlen[kx])) == 2u;
            memcpy(bad_rec, rec[kx], rlen[kx]);
            bad_rec[xbo + 1u] = 2u;                                    /* an XV version this firmware does not read */
            ok &= put_patch(kx, bad_rec, rlen[kx], st_crc32(bad_rec, rlen[kx])) == 2u;
            memcpy(bad_rec, rec[kx], rlen[kx]);
            bad_rec[4] &= (uint8_t)~PTF_CB;                            /* a record of before 0.2.14 */
            ok &= put_patch(kx, bad_rec, rlen[kx], st_crc32(bad_rec, rlen[kx])) == 2u;
            ok &= put_patch(kx, rec[kx], rlen[kx], rcrc[kx] ^ 1u) == 2u;                    /* a wrong CRC */
            ok &= put_patch(kx, rec[kx], rlen[kx] - 1u, st_crc32(rec[kx], rlen[kx] - 1u)) == 2u;   /* a record cut short */
            ok &= put_begin_patch(kx, PT_WORST + 1u, 0) == 1u && put_begin_patch(kx, PT_FIX - 1u, 0) == 1u && put_begin_patch(128, 100u, 0) == 1u;
            ok &= (transport_req = 1, put_begin_patch(kx, rlen[kx], rcrc[kx]) == 3u);
            transport_req = 0;
            bad += check("bad patch records (block length, XV version, a record of before 0.2.14, CRC, cut short, too long or short, a slot past 127, PLAY starting) are refused; the store is as it was",
                         ok && up_count() == UP_SLOTS - 1u && erases == e0 && up_raw_get(kx, got) == rlen[kx] && !memcmp(got, rec[kx], rlen[kx]));
        }
        /* length 0: the slot's record removed, its factory sound is back */
        bad += check("PUT of length 0 for a slot: its record is removed, the factory sound is back (as RESTORE)",
                     put_patch(5, rec[5], 0, 0) == 0 && !up_used(5u) && up_count() == UP_SLOTS - 2u);
        put_patch(5, rec[5], rlen[5], rcrc[5]);
        /* the worst record into all 128 slots through PUT */
        {
            for (k = 0, ok = 1; k < UP_SLOTS; k++) {
                for (i = 0; i < NX; i++)
                    XV(&trk[0], i) = (int16_t)((i & 1u) ? -64 + (int)k % 40 : 127 - (int)k % 40);
                for (xbn = 0; xbn < XB_MAX; xbn++)
                    ed[xbn] = (xb_edit_t){(uint8_t)(xbn < 30u ? xbn * 4u : 128u + xbn), (int16_t)(127 - (int)xbn * 3)};
                up_xb_in = ed;
                up_xb_nin = XB_MAX;
                trk[0].p[P_AMODE] = AM_SEQ;
                ok &= up_store_trk(&trk[0], k, "W", 0) == 0;
                up_xb_in = 0;
                up_xb_nin = 0;
                rlen[k] = up_raw_get(k, rec[k]);
                rcrc[k] = st_crc32(rec[k], rlen[k]);
            }
            for (k = 0; k < UP_SLOTS; k++)
                up_put(k, 0);
            for (k = 0; k < UP_SLOTS; k++)
                ok &= put_patch(k, rec[k], rlen[k], rcrc[k]) == 0;
            for (k = 0, same = 0; k < UP_SLOTS; k++)
                same += up_raw_get(k, got) == rlen[k] && !memcmp(got, rec[k], rlen[k]);
            bad += check("128 records with the 16 slots off default and 32 edits, written back through PUT into an empty store: all 128 take, the same bytes", ok && same == UP_SLOTS);
        }
        for (k = 0; k < UP_SLOTS; k++)
            up_put(k, 0);
        reset();
    }

    printf("backup test %s\n", bad ? "FAILED" : "passed");
    return bad != 0;
}
