/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* DUELL 0.2.14's USER WAVETABLES (wtab.c, editor_wtab.c, the backup ids 24..26) against simulated NOR flash:
 * where the slots live (0xA0000..0xA5FFF, three A/B pairs, nothing else touched, inside the loader's protected store), the commit record and CRC, the
 * object at boot, a corrupt or torn copy, an upload that fails, refusal while the sequencer plays (nothing written), stale and malformed transfers,
 * erase, ERASE DATA, the backup round trip (GET / PUT, length 0), and the engine reading what was stored. */
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
#include "../firmware/src/editor_wtab.c"

static int check(const char *what, int ok)
{
    printf("wtab: %-110s %s\n", what, ok ? "ok" : "FAIL");
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
    ed_bk_valid = ed_bk_put = ed_wt_put = 0;
    wt_boot();
}
static void put32(uint8_t *a, uint32_t v) { for (uint32_t i = 0; i < 5u; i++) a[i] = (uint8_t)((v >> (7u * i)) & 127u); }
static void pack7(uint8_t *a, uint32_t *k, const uint8_t *p, uint32_t n)
{
    while (n) {
        uint32_t g = n > 7u ? 7u : n, m = 0, i;
        for (i = 0; i < g; i++) m |= (uint32_t)(p[i] >> 7) << i;
        a[(*k)++] = (uint8_t)m;
        for (i = 0; i < g; i++) a[(*k)++] = p[i] & 127u;
        p += g;
        n -= g;
    }
}
static uint32_t wcall(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    rep_n = 0;
    ed_wt_handle(cmd, a, n);
    return rep_n;
}
static uint32_t wput(const uint8_t *a, uint32_t n) { wcall(ED_WT_PUT, a, n); return rep[2]; }
static uint32_t w_begin(uint32_t slot, const char *name, uint32_t crc)
{
    uint8_t a[15] = {0, (uint8_t)slot};
    memcpy(a + 2, name, strlen(name) < 8u ? strlen(name) : 8u);
    put32(a + 10, crc);
    return wput(a, sizeof a);
}
static uint32_t w_data(uint32_t slot, uint32_t off, const uint8_t *p, uint32_t n)
{
    uint8_t a[4 + 160] = {1, (uint8_t)slot, (uint8_t)(off & 127u), (uint8_t)(off >> 7)};
    uint32_t k = 4;
    pack7(a, &k, p, n);
    return wput(a, k);
}
static uint32_t w_end(uint32_t slot, uint32_t op) { uint8_t a[2] = {(uint8_t)op, (uint8_t)slot}; return wput(a, 2); }
static uint32_t w_all(uint32_t slot, const char *name, const uint8_t *d)   /* begin, 112 byte pieces, commit: the first failing rc */
{
    uint32_t off, rc = w_begin(slot, name, st_crc32(d, 1216));
    for (off = 0; !rc && off < 1216u; off += 112u)
        rc = w_data(slot, off, d + off, 1216u - off > 112u ? 112u : 1216u - off);
    return rc ? rc : w_end(slot, 2);
}
static void mk(uint8_t *d, uint32_t seed)   /* 1216 bytes: a pattern with all values (also bit 7 set) */
{
    for (uint32_t i = 0; i < 1216u; i++)
        d[i] = (uint8_t)((i * 37u + seed * 101u + (i >> 5)) % 251u) ;
}
static uint32_t ovec(const wt_obj_t *o, const uint8_t *d, const char *name)   /* the object a slot should hold */
{
    return o->ver == 1 && o->nf == 19 && o->nh == 64 && !o->flags && !strncmp(o->name, name, 8) && !memcmp(o->amp, d, 1216);
}

static uint32_t call(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    rep_n = 0;
    ed_backup_handle(cmd, a, n);
    return rep_n;
}

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


static int bad;
static void parse_list(uint32_t u, int *used, char *name, uint32_t *crc)   /* WT_LIST's slot u */
{
    uint32_t p = 3, i;
    for (i = 0; i < u; i++) {
        p++;
        while (rep[p++]) ;
        p += 5;
    }
    *used = rep[p++];
    for (i = 0; rep[p]; )
        name[i++] = (char)rep[p++];
    name[i] = 0;
    p++;
    *crc = ed_bk_r32(rep + p);
}
int main(void)
{
    static uint8_t D1[1216], D2[1216], D3[1216], Z[1216], got[1228];
    static uint8_t snap[0x100000];
    uint32_t len = 0, crc = 0, i, ok, e0;
    int used;
    char nm[16];
    st_hdr_t h;

    mk(D1, 1); mk(D2, 2); mk(D3, 3);

    /* where the slots live: three A/B pairs, 0xA0000..0xA5FFF, no other object's sector, inside the loader's protected store [0x97000, 0xE0000) */
    {
        uint32_t o, c, o2, c2, seen = 1;
        for (o = 0; o < 3u; o++)
            for (c = 0; c < 2u; c++) {
                uint32_t at = st_sector(OBJ_WTAB0 + o, c);
                seen &= at == 0xA0000u + (o * 2u + c) * 0x1000u && at >= 0x97000u && at + 0x1000u <= 0xE0000u;
                for (o2 = 0; o2 < OBJ_COUNT; o2++)
                    for (c2 = 0; c2 < 2u; c2++)
                        if (o2 != OBJ_WTAB0 + o && !OBJ_RETIRED(o2) && !(o2 >= OBJ_WTAB0) && st_sector(o2, c2) == at)
                            seen = 0;
            }
        bad += check("the slots: USR1 0xA0000/0xA1000, USR2 0xA2000/0xA3000, USR3 0xA4000/0xA5000; no other object has these sectors; inside [0x97000, 0xE0000), the loader's keep-out store", seen);
        bad += check("a wavetable object is 1228 bytes (12 header + 19 x 64), one A/B payload (<= 3840), object numbers below the voice bank's commit records",
                     sizeof(wt_obj_t) == 1228u && sizeof(wt_obj_t) <= ST_PAYLOAD_MAX && OBJ_WTAB0 + 3u <= OBJ_FM6VB && OBJ_COUNT == OBJ_PATCHX0 + 10u && OBJ_PATCHX0 > OBJ_FM6VB);
    }

    /* a fresh unit: every slot EMPTY, the list says so, a GET refuses */
    reset();
    memset(snap, 0xFF, sizeof snap);
    wcall(ED_WT_LIST, 0, 0);
    ok = rep[0] == 3u && rep[1] == 19u && rep[2] == 64u;
    for (i = 0; i < 3u; i++) {
        parse_list(i, &used, nm, &crc);
        ok &= !used && !nm[0] && !crc;
    }
    bad += check("fresh unit: WT_LIST 3 slots x 19 frames x 64 harmonics, all unused; the shown names EMPTY", ok && !strcmp(N_WV_TABLE[26], "EMPTY") && !strcmp(N_WV_TABLE[28], "EMPTY"));
    {
        uint8_t g[4] = {0, 0, 0, 16};
        wcall(ED_WT_GET, g, 4);
        bad += check("WT_GET of an empty slot: rc 1", rep[1] == 1u);
    }

    /* an upload: begin, pieces, commit */
    e0 = erases;
    bad += check("upload to USR2 (slot 1): begin, 11 pieces of 112 bytes, commit: rc 0", w_all(1, "MYTABLE", D1) == 0u);
    bad += check("the flash write is one erase and the pages, the commit record last: copy A of the object at 0xA2000, valid, the payload the object, CRC-32 over it",
                 erases == e0 + 1u && st_head(OBJ_WTAB0 + 1u, 0, &h) == 0 && h.len == 1228u && h.seq == 1u && h.type == OBJ_WTAB0 + 1u &&
                 st_crc32(nor + 0xA2000 + 256, 1228) == h.crc && ovec((const wt_obj_t *)(nor + 0xA2000 + 256), D1, "MYTABLE") && st_head(OBJ_WTAB0 + 1u, 1, &h) != 0);
    {   /* nothing else in the flash changed */
        uint32_t k, same = 1;
        for (k = 0; k < sizeof nor; k++)
            if (!(k >= 0xA2000u && k < 0xA3000u))
                same &= nor[k] == 0xFF;
        bad += check("nothing else in the flash was written (every byte outside 0xA2000..0xA2FFF still erased)", same);
    }
    wcall(ED_WT_LIST, 0, 0);
    parse_list(1, &used, nm, &crc);
    bad += check("WT_LIST: USR2 used, the name, the CRC of the 1216 data bytes; USR1 and USR3 still unused",
                 used && !strcmp(nm, "MYTABLE") && crc == st_crc32(D1, 1216));
    parse_list(0, &used, nm, &crc);
    ok = !used;
    parse_list(2, &used, nm, &crc);
    bad += check("..the other two slots untouched", ok && !used);
    bad += check("the engine's side: the slot's RAM mirror is the object, TABLE 27 is named MYTABLE, wt_used bit 1", (wt_used & 7u) == 2u && ovec(&wt_ram[1], D1, "MYTABLE") &&
                 !strcmp(N_WV_TABLE[27], "MYTABLE") && wv_uver[1] == 1u);
    ok = 1;
    for (i = 0; i < 1216u; i += 112u) {
        uint8_t g[4] = {1, (uint8_t)(i & 127u), (uint8_t)(i >> 7), (uint8_t)(1216u - i > 112u ? 112u : 1216u - i)};
        uint32_t n = g[3];
        wcall(ED_WT_GET, g, 4);
        ok &= rep[0] == 1u && rep[1] == 0u && (rep[2] | rep[3] << 7) == (int)i && rep[4] == n;
        {
            uint8_t out[112];
            ok &= ed_unpack7(rep + 5, rep_n - 5u, out, sizeof out) == n && !memcmp(out, D1 + i, n);
        }
    }
    bad += check("WT_GET reads the table back in 112 byte pieces, bit for bit", ok);
    {
        uint8_t g0[4] = {1, 0, 0, 0}, g1[4] = {1, 0, 0, 128}, g2[4] = {1, 100, 9, 112}, g3[4] = {3, 0, 0, 8}, g4[4] = {1, 0, 0, 127};
        uint32_t r;
        wcall(ED_WT_GET, g0, 4); r = rep[1] == 1u;
        wcall(ED_WT_GET, g1, 4); r &= rep[1] == 1u;
        wcall(ED_WT_GET, g2, 4); r &= rep[1] == 1u;
        wcall(ED_WT_GET, g3, 4); r &= rep[1] == 1u;
        wcall(ED_WT_GET, g4, 4); r &= rep[1] == 0u;
        wcall(ED_WT_GET, g4, 3); r &= rep[1] == 1u;
        bad += check("WT_GET refuses count 0, count 128, a read past the end, slot 3, a short request (rc 1)", r);
    }

    /* power-on: the slots come back from the flash */
    memset(wt_ram, 0, sizeof wt_ram);
    wt_used = 0;
    wt_boot();
    bad += check("power-on (wt_boot): the table is back from the flash, named", wt_used == 2u && ovec(&wt_ram[1], D1, "MYTABLE") && !strcmp(N_WV_TABLE[27], "MYTABLE") && !strcmp(N_WV_TABLE[26], "EMPTY"));

    /* a second upload goes to the other copy; a failing one leaves the slot as it was */
    bad += check("a second upload (new name, no name at all for USR1): copy B of USR2, the sequence up", w_all(1, "TWO", D2) == 0u && st_head(OBJ_WTAB0 + 1u, 1, &h) == 0 && h.seq == 2u &&
                 ovec(&wt_ram[1], D2, "TWO") && w_all(0, "", D3) == 0u && !strcmp(N_WV_TABLE[26], "USR1"));
    erase_error = 1;
    e0 = wv_uver[1];
    bad += check("a failing flash erase: rc 4, the slot as it was (RAM, flash, the engine's version not counted)", w_all(1, "THREE", D3) == 4u && ovec(&wt_ram[1], D2, "TWO") && wv_uver[1] == e0);
    erase_error = 0;
    memset(wt_ram, 0, sizeof wt_ram); wt_used = 0; wt_boot();
    bad += check("..and after power-on the old table is still the one", ovec(&wt_ram[1], D2, "TWO") && ovec(&wt_ram[0], D3, ""));
    {   /* a torn write: payload page of the newest copy damaged -> the older copy is in charge; both damaged -> EMPTY */
        nor[0xA2000 + 256 + 5] ^= 0x40;               /* copy A (the older, seq 1: D1)... copy B (seq 2) is the current one */
        st_head(OBJ_WTAB0 + 1u, 1, &h);
        nor[0xA3000 + 256 + 100] ^= 0x01;             /* damage B */
        memset(wt_ram, 0, sizeof wt_ram); wt_used = 0; wt_boot();
        ok = !(wt_used & 2u) && !strcmp(N_WV_TABLE[27], "EMPTY");     /* A is damaged too (flipped above) */
        nor[0xA2000 + 256 + 5] ^= 0x40;               /* repair A: D1 comes back, with its own CRC */
        wt_boot();
        bad += check("a damaged copy fails its CRC: the older whole copy is used (USR2 back to MYTABLE), both damaged: EMPTY, never a half table", ok && ovec(&wt_ram[1], D1, "MYTABLE"));
    }

    /* refusal while the sequencer plays: nothing written */
    reset();
    memcpy(snap, nor, sizeof nor);
    transport_req = 1;                                 /* PLAY starting: transport_busy() */
    e0 = erases;
    ok = w_begin(0, "X", st_crc32(D1, 1216)) == 3u;
    {
        uint8_t er[2] = {4, 0};
        ok &= wput(er, 2) == 3u;
    }
    transport_req = 0;
    ok &= w_begin(0, "X", st_crc32(D1, 1216)) == 0u && w_data(0, 0, D1, 112) == 0u;
    transport_req = 1;
    ok &= w_data(0, 112, D1 + 112, 112) == 3u && w_end(0, 2) == 3u;
    transport_req = 0;
    bad += check("while the sequencer plays (or starts): begin, a piece, commit and erase are refused (rc 3), nothing is written, the slot stays EMPTY", ok && erases == e0 && !memcmp(snap, nor, sizeof nor) && !wt_used);
    bad += check("..and the transfer goes on after it stops: pieces in order, commit", w_data(0, 112, D1 + 112, 112) == 0u && w_end(0, 3) == 0u && w_end(0, 2) == 5u);

    /* malformed and stale transfers */
    reset();
    ok = w_begin(3, "A", 0) == 1u;
    {
        uint8_t b1[15] = {0, 0, 'A', 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, b2[15] = {0, 0, 'A'}, b3[14] = {0, 0};
        ok &= wput(b1, 15) == 1u;                      /* a control character in the name */
        b2[2] = 'A'; b2[10] = 0; b2[14] = 16;
        ok &= wput(b2, 15) == 1u;                      /* a CRC byte past 15 */
        ok &= wput(b3, 14) == 1u;                      /* short */
        ok &= w_data(0, 0, D1, 112) == 5u;             /* no begin: stale */
    }
    bad += check("bad arguments are refused (1): slot 3, a control character in the name, a CRC byte over 15, a short begin; data with no begin is stale (5)", ok);
    ok = w_begin(0, "ABC", st_crc32(D1, 1216)) == 0u;
    ok &= w_data(0, 112, D1 + 112, 112) == 1u;         /* out of order */
    ok &= w_data(0, 0, D1, 112) == 0u && w_data(0, 0, D1, 112) == 1u;   /* a piece twice */
    ok &= w_end(0, 2) == 2u;                           /* commit with a missing part: validation failed */
    bad += check("a piece out of order or twice: rc 1; a commit before all 1216 bytes: rc 2, nothing written", ok && !wt_used && !erases);
    ok = w_begin(0, "ABC", st_crc32(D2, 1216)) == 0u;
    for (i = 0; i < 1216u; i += 112u)
        ok &= w_data(0, i, D1 + i, 1216u - i > 112u ? 112u : 1216u - i) == 0u;
    ok &= w_end(0, 2) == 2u;
    bad += check("a wrong CRC at commit: rc 2, nothing written", ok && !wt_used && !erases);
    memset(Z, 0, sizeof Z);
    bad += check("an all-zero table: rc 2 (an object the loader would refuse)", w_all(0, "ZERO", Z) == 2u && !wt_used && !erases);
    ok = w_begin(0, "ABC", st_crc32(D1, 1216)) == 0u && w_data(0, 0, D1, 112) == 0u && w_end(0, 3) == 0u && w_data(0, 112, D1 + 112, 112) == 5u;
    bad += check("abort (3) ends the transfer: the next piece is stale (5)", ok);
    ok = w_begin(0, "ABC", st_crc32(D1, 1216)) == 0u;
    usb.resets++;
    ok &= w_data(0, 0, D1, 112) == 5u;
    ok &= w_begin(0, "ABC", st_crc32(D1, 1216)) == 0u;
    fm1_ms += 15001u;
    ok &= w_data(0, 0, D1, 112) == 5u;
    ok &= w_begin(0, "ABC", st_crc32(D1, 1216)) == 0u;
    project_save(0);
    ok &= w_data(0, 0, D1, 112) == 5u;
    bad += check("a USB bus reset, 15 s without a request, or a project save (it reuses the staging RAM) make the transfer stale (5)", ok && !wt_used);
    ok = w_begin(0, "ABC", st_crc32(D1, 1216)) == 0u && w_data(0, 0, D1, 112) == 0u;
    {
        uint8_t bb[12] = {0, 13};
        put32(bb + 2, 0); put32(bb + 7, 0);
        call(ED_BACKUP_PUT, bb, 12);                   /* a backup transfer starts in the same staging RAM */
    }
    ok &= w_data(0, 112, D1 + 112, 112) == 5u;
    bad += check("a backup PUT beginning ends a wavetable upload (5): they share the staging RAM", ok);

    /* erase */
    reset();
    w_all(2, "GONE", D3);
    e0 = erases;
    {
        uint8_t er[2] = {4, 2}, er2[2] = {4, 1}, er3[3] = {4, 2, 0};
        ok = (wt_used & 4u) && wput(er, 2) == 0u && !(wt_used & 7u) && !strcmp(N_WV_TABLE[28], "EMPTY");
        ok &= st_head(OBJ_WTAB0 + 2u, 0, &h) != 0 && st_head(OBJ_WTAB0 + 2u, 1, &h) != 0;
        ok &= erases == e0 + 1u;
        ok &= wput(er2, 2) == 0u && erases == e0 + 1u;     /* an empty slot: nothing erased again (no wear) */
        ok &= wput(er3, 3) == 1u;
    }
    memset(wt_ram, 0, sizeof wt_ram); wt_used = 0; wt_boot();
    bad += check("erase (op 4): both copies gone, EMPTY after power-on too; erasing an empty slot wears nothing; a long erase request: rc 1", ok && !wt_used);

    /* ERASE DATA takes the wavetables too */
    reset();
    w_all(0, "A", D1); w_all(1, "B", D2); w_all(2, "C", D3);
    bad += check("ERASE DATA (erase_data_run): all three slots' sectors blank again", erase_data_run() == 0 && ({ uint32_t k, b = 1; for (k = 0xA0000u; k < 0xA6000u; k++) b &= nor[k] == 0xFF; b; }));
    reset();

    /* the backup: ids 24..26 list, GET, restore, length 0 */
    w_all(1, "BACKME", D2);
    bad += check("backup LIST: 40 objects; 24 and 26 length 0, 25 the 1228 byte object with the CRC of the RAM mirror",
                 list(25, &len, &crc) == 0 && rep[2] == 40u && len == 1228u && crc == st_crc32(&wt_ram[1], 1228) && list(24, &len, &crc) == 0 && len == 0 &&
                 list(26, &len, &crc) == 0 && len == 0);
    ok = 1;
    for (i = 0; i < 1228u; i += 256u) {
        uint32_t n = 1228u - i > 256u ? 256u : 1228u - i;
        ok &= get(25, i, n) == 0u && ed_unpack7(rep + 9, rep_n - 9u, got + i, 256u) == n;
    }
    bad += check("backup GET of id 25: the object, bit for bit (version 1, 19, 64, flags 0, the name, the data)", ok && ovec((wt_obj_t *)got, D2, "BACKME"));
    {
        uint32_t c25;
        static uint8_t obj25[1228];
        memcpy(obj25, got, 1228);
        c25 = st_crc32(obj25, 1228);
        reset();
        bad += check("restore into a unit that lost it: PUT id 25 commits (0); slot 1 holds the table, named, the flash written; USR1 and USR3 stay EMPTY",
                     put_all(25, obj25, 1228, c25) == 0 && ovec(&wt_ram[1], D2, "BACKME") && wt_used == 2u && st_head(OBJ_WTAB0 + 1u, 0, &h) == 0 &&
                     !strcmp(N_WV_TABLE[27], "BACKME") && list(25, &len, &crc) == 0 && crc == c25);
        obj25[0] = 2;
        ok = put_all(24, obj25, 1228, st_crc32(obj25, 1228)) == 2u;
        obj25[0] = 1; obj25[1] = 18;
        ok &= put_all(24, obj25, 1228, st_crc32(obj25, 1228)) == 2u;
        obj25[1] = 19; obj25[6] = 3;
        ok &= put_all(24, obj25, 1228, st_crc32(obj25, 1228)) == 2u;
        obj25[6] = 'X'; memset(obj25 + 12, 0, 1216);
        ok &= put_all(24, obj25, 1228, st_crc32(obj25, 1228)) == 2u;
        ok &= put_begin(24, 1227, 0) == 1u && put_begin(24, 1229, 0) == 1u && put_begin(40, 0, 0) == 1u;
        bad += check("bad objects (version, frames, a control character in the name, all zero) are refused at commit (2), wrong sizes at begin (1); nothing written", ok && !(wt_used & 1u));
        e0 = erases;
        ok = put_all(25, "", 0, 0) == 0u && !(wt_used & 2u) && list(25, &len, &crc) == 0 && len == 0 && erases == e0 + 1u;
        bad += check("length 0 for id 25 empties the slot (both copies erased); the table is EMPTY again", ok && !strcmp(N_WV_TABLE[27], "EMPTY"));
    }
    reset();
    transport_req = 1;
    bad += check("a backup PUT of a wavetable while PLAY starts is refused (3)", put_begin(24, 1228, 0) == 3u);
    transport_req = 0;

    printf("wtab test %s\n", bad ? "FAILED" : "passed");
    return bad != 0;
}
