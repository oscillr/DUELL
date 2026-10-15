/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The editor's full backup (editor_backup.c: LIST / GET / PUT) against simulated NOR flash:
 * CRC before any write, stale runtime copies, USB resets and timeouts, malformed objects, older
 * project formats, settings values and user preset banks kept byte for byte. */
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

int main(void)
{
    int bad = 0;
    uint32_t len = 0, crc = 0, before;
    static project_store_t st;
    persist_t ps;

    reset();
    trk[0].step[0] = (step_t){{60}, 1, ST_NOTE, 0, 96, 0, 0};
    bad += check("LIST captures the runtime: all 40 objects (ED_BK_N: id 8 empty, 6 7 9..12 27..31 35..39 the PATCH store's 16 chunks, 13..20 the multis, 21..23 the pattern slots, 24..26 the user wavetables), runtime 3840 B (FUN11)",
                 list(0, &len, &crc) == 0 && rep[2] == ED_BK_N && ED_BK_N == 40u && len == sizeof(project_store_t) && len == 3840u &&
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
        bad += check("a Felucca user preset bank (ids 6, 7: another shape) is refused (2)",
                     put_all(6, old_bank, 3000u, st_crc32(old_bank, 3000u)) == 2u &&
                     put_all(7, old_bank, 3000u, st_crc32(old_bank, 3000u)) == 2u && put_begin(6, 3585u, 0) == 1u && put_begin(35, 3840u, 0) == 1u);
        bad += check("an id 40 is refused (1); ids 24..26 (the user wavetables) list empty on a fresh unit",
                     put_begin(40, 0, 0) == 1u && list(24, &len, &crc) == 0 && len == 0 && list(26, &len, &crc) == 0 && len == 0);
    }


    /* DUELL 0.2.14: the multis (ids 13..20 = OBJ_MULTI0..7) and the pattern slots (ids 21..23 = OBJ_PATSLOT0..2) are in the archive */
    {
        static uint8_t m_a[3840], p_a[3584], m_b[3840], got[3840];
        uint32_t ml = 0, mc = 0, pl = 0, pc = 0, ml2 = 0, mc2 = 0, off, e0, ok2;
        ps_obj_t *po;
        reset();
        multi_boot(); ps_boot();
        bad += check("empty multi and pattern objects list as length 0 (13..20, 21..23)",
                     list(13, &len, &crc) == 0 && len == 0 && list(20, &len, &crc) == 0 && len == 0 &&
                     list(21, &len, &crc) == 0 && len == 0 && list(23, &len, &crc) == 0 && len == 0);
        trk[0].step[0] = (step_t){{62}, 1, ST_NOTE, 0, 96, 0, 0};
        trk[0].p[P_SLEN] = 12;
        project_save_as(2, "TESTONE");                      /* M03: object 1, record 0 */
        project_save_as(3, "TESTTWO");                      /* M04: object 1, record 1 */
        project_save_as(9, "OTHERNINE");                    /* M10: object 4 */
        ps_save_trk(&trk[0], 2, "PAT THREE");               /* P3: object 0, still a pending write (ps_dirty) */
        bad += check("a saved multi lists in its object (id 14), a pattern in id 21 (the RAM mirror: still dirty)",
                     list(14, &ml, &mc) == 0 && ml > 8u && ml < 3840u && list(21, &pl, &pc) == 0 && pl == 3584u && (ps_dirty & 1u) &&
                     list(22, &len, &crc) == 0 && len == 0);
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
        memcpy(p_a, &ps_bank[0], 3584);
        bad += check("a pattern object GET is the RAM mirror (CRC as listed)", st_crc32(p_a, 3584) == pc);
        bad += check("only the objects with a stored multi list a length (M10 = object 4, id 17; object 3, id 16: none)",
                     list(17, &ml2, &mc2) == 0 && ml2 > 8u && list(16, &len, &crc) == 0 && len == 0);
        bad += check("an id past the user wavetables, and a bad size at begin, are refused (1)",
                     put_begin(41, 0, 0) == 1u && put_begin(24, 5, 0) == 1u && put_begin(24, 1229, 0) == 1u && put_begin(14, 5, 0) == 1u && put_begin(14, 3841, 0) == 1u &&
                     put_begin(21, 3583, 0) == 1u && put_begin(22, 3585, 0) == 1u);
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
        bad += check("restore: the pattern object (id 21) commits over a dirty mirror", (ps_dirty & 1u) && put_all(21, p_a, 3584, pc) == 0 && !(ps_dirty & 1u));
        ps_poll();
        {
            ps_obj_t *fl = (ps_obj_t *)got;
            int n = st_load(OBJ_PATSLOT0, got, sizeof got);
            bad += check("restore: ps_poll writes nothing over it; flash and mirror are the restored object, P3 is back by name, P6 gone",
                         n == 3584 && st_crc32(got, 3584) == pc && !memcmp(&ps_bank[0], p_a, 3584) && ps_used(2) && !ps_used(5) &&
                         fl->r[2].used == PS_USED);
        }
        {
            char nm[13];
            ps_name(2, nm);
            bad += check("restore: P3 loads onto a part (LEN 12, note 62), name PAT THREE",
                         !strcmp(nm, "PAT THREE") && (trk[1].p[P_SLEN] = 0, ps_load_trk(&trk[1], 2) == 0) && trk[1].p[P_SLEN] == 12 && trk[1].step[0].note[0] == 62u);
        }
        bad += check("a verify LIST after the restore: both objects equal the file's size and CRC",
                     list(14, &len, &crc) == 0 && len == ml && crc == mc && list(21, &len, &crc) == 0 && len == 3584u && crc == pc);
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
        po = (ps_obj_t *)got;
        memcpy(got, p_a, 3584);
        po->magic ^= 1u;
        ok2 = put_all(21, got, 3584, st_crc32(got, 3584)) == 2u;
        memcpy(got, p_a, 3584);
        po->r[2].len = 0;                                   /* LEN out of range */
        ok2 &= put_all(21, got, 3584, st_crc32(got, 3584)) == 2u;
        memcpy(got, p_a, 3584);
        po->r[2].name[0] = 3;                               /* an unprintable name character */
        ok2 &= put_all(21, got, 3584, st_crc32(got, 3584)) == 2u;
        memcpy(got, p_a, 3584);
        po->r[2].used = 1;                                  /* neither unused nor a slot */
        ok2 &= put_all(21, got, 3584, st_crc32(got, 3584)) == 2u;
        memset(got, 0, 3584);
        po->magic = PS_MAGIC; po->rsize = sizeof(ps_rec_t); po->nslot = PS_PER_OBJ;
        memcpy(&po->r[4], &((ps_obj_t *)p_a)->r[2], sizeof(ps_rec_t));   /* a valid slot in the place of P17 (object 2 has four) */
        ok2 &= put_all(23, got, 3584, st_crc32(got, 3584)) == 2u;
        bad += check("bad pattern objects (magic, LEN, name, used marker, a slot past P16) are refused at commit (2), nothing written",
                     ok2 && erases == e0 && ps_used(2));
        transport_req = 1;
        bad += check("PUT of a multi or pattern while PLAY starts is refused (3), as for the others", put_begin(14, ml, mc) == 3u && put_begin(21, 3584, pc) == 3u);
        transport_req = 0;
        /* an empty object restores as empty: the factory multi shows again, the pattern slots are unused */
        reset();
        multi_boot(); ps_boot();
        project_save_as(0, "MINE");
        bad += check("before: M01 holds a saved multi, not the factory one", project_used(0) && !project_factory(0));
        bad += check("restore: length 0 for id 13 empties M01 / M02 (the factory multi shows again)",
                     put_all(13, "", 0, 0) == 0 && project_factory(0) && list(13, &len, &crc) == 0 && len == 0);
        ps_save_trk(&trk[0], 0, "X");
        bad += check("restore: length 0 for id 21 empties the pattern slots (and clears the dirty bit)",
                     put_all(21, "", 0, 0) == 0 && !ps_used(0) && !(ps_dirty & 1u) && list(21, &len, &crc) == 0 && len == 0);
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

    /* DUELL 0.2.15: the PATCH store's 16 chunks are backup ids 6 7 9..12 27..31 35..39: 128 patches saved, every chunk read (LIST / GET), the
     * store erased, every chunk restored (PUT): the same 128 patches, the same names, the same bytes; a chunk with more than PT_K records is refused */
    {
        static uint8_t obj[16][3840];
        static const uint8_t IDS[16] = {6, 7, 9, 10, 11, 12, 27, 28, 29, 30, 31, 35, 36, 37, 38, 39};
        uint32_t c, k, off, ok = 1, olen[16], used = 0, bytes = 0, same = 0;
        char nm[13], want[13];
        reset();
        set_engine_of(&trk[0], 0u);
        apply_preset_to(&trk[0], 2u);
        for (k = 0; k < UP_SLOTS; k++) {
            char nmx[8] = {'B', (char)('0' + k / 10u % 10u), (char)('0' + k % 10u), 0};
            ok &= up_store_trk(&trk[0], k, nmx, 0) == 0;
        }
        bad += check("128 patches saved (one per slot)", ok && up_count() == UP_SLOTS);
        for (c = 0; c < 16u; c++) {
            olen[c] = 0;
            ok &= list(IDS[c], &len, &crc) == 0 && len > 0 && len <= PT_CAP;
            for (off = 0; ok && off < len; off += 256u) {
                uint32_t n = len - off > 256u ? 256u : len - off;
                ok &= get(IDS[c], off, n) == 0 && ed_unpack7(rep + 9, rep_n - 9u, obj[c] + off, 256u) == n;
            }
            ok &= st_crc32(obj[c], len) == crc;
            olen[c] = len;
            bytes += len;
            used += pt_cnt(c) > 0;
        }
        bad += check("LIST / GET: all 16 chunks (ids 6 7 9..12 27..31 35..39) are read, each with the CRC LIST gave, each at most 3584 bytes", ok && used == 16u);
        printf("backup: the 16 chunks hold %u bytes (128 plain patches)\n", bytes);
        for (k = 0; k < UP_SLOTS; k++)
            up_put(k, 0);
        bad += check("  the store erased: no saved patch", up_count() == 0u);
        {   /* 0.2.17: a backup carries the saved records only; a slot with no record shows its factory sound */
            uint32_t fe, fk;
            sl_fac(5u, &fe, &fk);
            sl_name(5u, nm);
            bad += check("  and every slot shows its factory sound again (a backup carries the saved records only)", !up_used(5u) && !strcmp(nm, ds_name(fe, fk)) && sl_engine(5u) == fe);
        }
        for (c = 0; c < 16u; c++)
            ok &= put_all(IDS[c], obj[c], olen[c], st_crc32(obj[c], olen[c])) == 0;
        for (k = 0; k < UP_SLOTS; k++) {
            want[0] = 'B'; want[1] = (char)('0' + k / 10u % 10u); want[2] = (char)('0' + k % 10u); want[3] = 0;
            up_name(k, nm);
            same += up_used(k) && !strcmp(nm, want);
        }
        bad += check("  every chunk restored by PUT: the same 128 patches, names and slots", ok && same == UP_SLOTS && up_count() == UP_SLOTS);
        for (c = 0, ok = 1; c < 16u; c++)
            ok &= list(IDS[c], &len, &crc) == 0 && len == olen[c] && crc == st_crc32(obj[c], olen[c]);
        bad += check("  and LIST again: size and CRC of each chunk equal the file's", ok);
        {   /* more records than a chunk may hold (PT_K + 1 small ones): refused at commit */
            static uint8_t big[3840];
            uint32_t w = PT_HDR, i;
            up_rec_t r = *up_rec(0);
            big[0] = (uint8_t)PT_MAGIC; big[1] = (uint8_t)(PT_MAGIC >> 8); big[2] = 1; big[3] = 0;
            for (i = 0; i < PT_K + 1u; i++) {
                uint32_t n = pt_encode(i, 1u, &r, 0);
                memcpy(big + w, pt_enc, n);
                w += n;
            }
            bad += check("a chunk with more than PT_K records is refused (2): the guarantee that all 128 slots fit rests on it",
                         put_all(27, big, w, st_crc32(big, w)) == 2u && up_count() == UP_SLOTS);
        }
        for (k = 0; k < UP_SLOTS; k++)
            up_put(k, 0);
        reset();
    }

    printf("backup test %s\n", bad ? "FAILED" : "passed");
    return bad != 0;
}
