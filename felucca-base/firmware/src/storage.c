/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Persistent storage on the SPI NOR.
 *
 * Every object has an A/B sector pair. A save goes to the copy that is not
 * the current one: erase the sector, program the payload pages (from offset
 * 256), then the 32-byte header at offset 0 LAST. The header is the commit
 * record (magic, seq, length, payload CRC, header CRC); on load the valid
 * copy with the newest seq wins (including wrap), so a write torn at any point leaves the
 * previous copy in charge.
 *
 * Flash access goes through three hooks (also used by the host test):
 *   st_read(off, dst, n)   st_erase(off)   st_prog(off, src, n)
 */
#define ST_MAGIC 0x554C4546u                   /* "FELU" */
#define ST_SECTOR 4096u
#define ST_PAYLOAD_OFF 256u
#define ST_PAYLOAD_MAX (ST_SECTOR - ST_PAYLOAD_OFF)

/* flash map (FL_DATA 0x97000..0xDFFFF, FL_GLOB 0xFC000.., FL_AUTO 0xE5000..0xE6FFF, FL_VBANK 0xE7000): settings 0xFC000, DUELL's
 * MULTI store M01..M08 0x97000..0x9EFFF (Felucca's four project slots were there: OBJ_PROJECT0..3 are retired, below), the
 * user sample slots 0xA0000..0xC7FFF (USR1 0xA0000, USR2 0xB4000, 80 KiB each; DUELL retired USR3, which lay at 0xC8000..0xCDFFF;
 * DUELL 0.2.9 retired USR1 and USR2 with the SAMPLE engine: 0xA0000..0xC7FFF (160 KiB) is UNUSED, nothing reads or writes it (old
 * sample data may still lie there), FREE for future WAVE user wavetable slots), DUELL's PATCH store (upreset.c, OBJ_PATCH0..5: six A/B pairs, below) 0xC8000..0xCDFFF (chunks 0..2),
 * the MULTI store M09..M16 0xCE000..0xD5FFF (taken from USR3's end), DUELL's pattern slots 0xD6000..0xDBFFF (below); the user
 * preset banks of Felucca (OBJ_UPRESET0/1, 0xDC000..0xDFFFF: since DUELL step 3.6b the PATCH store's chunks 3 and 4) and the user
 * presets' FM6 patches (OBJ_UPFM6, copy A 0x9F000, copy B 0xFE000, shared with the retired FM6 bank: the PATCH store's chunk 5)
 * are retired as objects: DUELL keeps patches in the patch store only; the autosave (project.c,
 * 1.2): A 0xE5000, B 0xE6000, above the OTA staging (0xE0000..0xE4FFF), in the margin the stock firmware's own update
 * stages into (only ever while the stock firmware runs: a later Felucca finds no valid copy and starts as without one).
 * Those two sectors (0x9F000, 0xFE000) held the FM6 patch bank of 1.0..1.0.2 and then the user presets' FM6 patches; DUELL
 * stores an FM6 patch inside the patch's own record, so only the patch store (chunk 5) uses them now. Nothing else touches
 * 0xDC000..0xDFFFF (below the OTA staging 0xE0000, inside FL_DATA) nor those two (FL_DATA / FL_GLOB, below 0xFF000).
 * The FM6 voice bank (1.4.1, fm6_vbank.c): 0xE7000, the sector above the autosave, with a commit record of the same
 * kind (type OBJ_FM6VB, not an st_load object); a new bank is first staged in the autosave's older copy (fm6_vbank.c).
 * DUELL's pattern slots (patslot.c, OBJ_PATSLOT0..2): A/B pairs at 0xD6000..0xDBFFF (FL_PSLOT_*), the last 24 KiB
 * of the user sample region (taken from USR3, which ends at 0xD5FFF), just below the user preset banks; nothing is written to the
 * stock firmware's USR region (0xEA000..0xFBFFF). Their payloads keep each sector's tail (0xF00..) erased.
 * DUELL's PATCH store (upreset.c, SAVE > PATCHES; OBJ_PATCH0..2, ids after the multis', so no commit record of another object
 * changes): six A/B pairs (0xC8000..0xCDFFF, 0xDC000..0xDFFFF, 0x9F000 / 0xFE000), a chunk of sparse records each (a record = a stored patch, upreset.c). A save
 * is the standard commit-last write of the chunk that changed; a record that moves to another chunk is written there first
 * (newer revision byte wins at the next boot), then cut from the old one.
 * DUELL's MULTI store (project.c, SAVE > MULTI; OBJ_MULTI0..7, two multis per object): eight A/B pairs in two ranges
 * (fm1_flash.h FL_MULTI_*): objects 0..3 at 0x97000..0x9EFFF (the sectors of Felucca's four project slots, OBJ_PROJECT0..3:
 * retired, their numbers kept so no other object's commit records change; st_head / st_save refuse them, so nothing
 * writes there under their name and an old project's record reads as no multi), objects 4..7 at 0xCE000..0xD5FFF.
 * The factory multis M01..M10 (multi.c, build/gen/duell_multis.h) are ROM, not flash: an empty record shows them, a stored one wins. */
enum { OBJ_SETTINGS, OBJ_PROJECT0, OBJ_UPRESET0 = OBJ_PROJECT0 + 4, OBJ_FM6BANK = OBJ_UPRESET0 + 2, OBJ_UPFM6,
       OBJ_AUTOSAVE, OBJ_PATSLOT0, OBJ_MULTI0 = OBJ_PATSLOT0 + 3,   /* DUELL: the pattern slots (patslot.c), 3 objects */
       OBJ_PATCH0 = OBJ_MULTI0 + 8,                                 /* DUELL: the MULTI store (project.c), 8 objects; the PATCH store (upreset.c), */
       OBJ_COUNT = OBJ_PATCH0 + 6 };                                /* .. 6 objects */
/* the project slots' numbers, Felucca's user preset banks and the FM6 patch objects (DUELL: all retired, never read or written) */
#define OBJ_RETIRED(obj) (((obj) >= OBJ_PROJECT0 && (obj) < OBJ_PROJECT0 + 4u) || ((obj) >= OBJ_UPRESET0 && (obj) <= OBJ_UPFM6))
#define OBJ_FM6VB 32u                          /* the voice bank's commit records (fm6_vbank.c; >= OBJ_COUNT: st_load
                                                * never takes one, no object here takes its sectors) */

typedef struct {
    uint32_t magic;
    uint16_t type, slot;
    uint32_t seq, len, crc, rsv[2];
    uint32_t hcrc;
} st_hdr_t;
_Static_assert(sizeof(st_hdr_t) == 32u, "storage commit record layout");

static int st_read(uint32_t off, void *dst, uint32_t n);
static int st_erase(uint32_t off);
static int st_prog(uint32_t off, const void *src, uint32_t n);

static uint32_t st_crc32_add(uint32_t c, const void *p, uint32_t n)   /* zlib CRC-32, 4 bits per step; running:
                                                                         * from 0xFFFFFFFF, the CRC is ~c */
{
    static const uint32_t T[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
    const uint8_t *b = p;
    while (n--) {
        c ^= *b++;
        c = (c >> 4) ^ T[c & 15u];
        c = (c >> 4) ^ T[c & 15u];
    }
    return c;
}
static uint32_t st_crc32(const void *p, uint32_t n) { return ~st_crc32_add(0xFFFFFFFFu, p, n); }

static uint32_t st_sector(uint32_t obj, uint32_t copy)  /* flash offset of copy A (0) / B (1) */
{
    if (obj == OBJ_SETTINGS)
        return 0xFC000u + copy * ST_SECTOR;
    if (obj >= OBJ_PATCH0) {                             /* (the PATCH store, chunks 0..5: see the flash map) */
        uint32_t c = obj - OBJ_PATCH0;
        if (c < 3u)                                      /* USR3's old place 0xC8000..0xCDFFF */
            return 0xC8000u + c * 2u * ST_SECTOR + copy * ST_SECTOR;
        if (c < 5u)                                      /* Felucca's retired user preset banks 0xDC000..0xDFFFF */
            return 0xDC000u + (c - 3u) * 2u * ST_SECTOR + copy * ST_SECTOR;
        return copy ? 0xFE000u : 0x9F000u;               /* the retired FM6 patch bank's pair */
    }
    if (obj >= OBJ_MULTI0)                               /* (before the PATSLOT and UPRESET cases, which take every obj above) */
        return (obj - OBJ_MULTI0 < 4u ? 0x97000u + (obj - OBJ_MULTI0) * 2u * ST_SECTOR
                                      : 0xCE000u + (obj - OBJ_MULTI0 - 4u) * 2u * ST_SECTOR) + copy * ST_SECTOR;
    if (obj >= OBJ_PATSLOT0)                             /* (before the UPRESET case, which takes every obj above) */
        return 0xD6000u + (obj - OBJ_PATSLOT0) * 2u * ST_SECTOR + copy * ST_SECTOR;
    if (obj == OBJ_FM6BANK || obj == OBJ_UPFM6)          /* (the same pair: see the flash map) */
        return copy ? 0xFE000u : 0x9F000u;
    if (obj == OBJ_AUTOSAVE)
        return 0xE5000u + copy * ST_SECTOR;
    if (obj >= OBJ_UPRESET0)
        return 0xDC000u + (obj - OBJ_UPRESET0) * 2u * ST_SECTOR + copy * ST_SECTOR;
    return 0x97000u + (obj - OBJ_PROJECT0) * 2u * ST_SECTOR + copy * ST_SECTOR;
}

static uint8_t st_buf[ST_PAYLOAD_MAX] __attribute__((aligned(4), section(".pool")));   /* (main loop only) */

static int st_head(uint32_t obj, uint32_t copy, st_hdr_t *h)   /* commit record valid: 0 */
{
    if (obj >= OBJ_COUNT || OBJ_RETIRED(obj) || copy > 1u)
        return -1;
    if (st_read(st_sector(obj, copy), h, sizeof *h))
        return -1;
    if (h->magic != ST_MAGIC || h->type != obj || h->slot != copy || h->len > ST_PAYLOAD_MAX ||
        h->hcrc != st_crc32(h, sizeof *h - 4u))
        return -1;
    return 0;
}

static int st_body(uint32_t obj, uint32_t copy, const st_hdr_t *h)   /* payload -> st_buf, CRC ok: 0 */
{
    if (st_read(st_sector(obj, copy) + ST_PAYLOAD_OFF, st_buf, h->len) || st_crc32(st_buf, h->len) != h->crc)
        return -1;
    return 0;
}

/* the current copy: the newest valid sequence (A on a tie), -1 when
 * neither is valid. Headers first, so only the winner's payload is read (it
 * is left in st_buf); *h gets its header. */
static int st_current(uint32_t obj, st_hdr_t *h)
{
    st_hdr_t a, b;
    int va = st_head(obj, 0, &a) == 0, vb = st_head(obj, 1, &b) == 0;
    if (vb && (!va || (b.seq != a.seq && b.seq - a.seq < 0x80000000u))) {
        if (st_body(obj, 1, &b) == 0) {
            *h = b;
            return 1;
        }
        vb = 0;
    }
    if (va && st_body(obj, 0, &a) == 0) {
        *h = a;
        return 0;
    }
    if (vb && st_body(obj, 1, &b) == 0) {
        *h = b;
        return 1;
    }
    return -1;
}

/* load the whole object into dst; returns its length, or -1 if it does not fit */
static int st_load(uint32_t obj, void *dst, uint32_t max)
{
    uint32_t i;
    st_hdr_t h;
    if (obj >= OBJ_COUNT || st_current(obj, &h) < 0 || h.len > max)
        return -1;
    for (i = 0; i < h.len; i++)
        ((uint8_t *)dst)[i] = st_buf[i];
    return (int)h.len;
}

#ifndef ST_DUCK_BEGIN                                   /* (storage_hw.c: the flash duck around a write; host tests: none) */
#define ST_DUCK_BEGIN() ((void)0)
#define ST_DUCK_END() ((void)0)
#endif
/* save into copy `to` (0 A, 1 B; -1: the one that is not the current copy, as st_save) */
static int st_save_to_x(uint32_t obj, const void *src, uint32_t len, int to)
{
    uint32_t seq, base, off;
    int cur, rc;
    st_hdr_t h;
    if (obj >= OBJ_COUNT || OBJ_RETIRED(obj) || len > ST_PAYLOAD_MAX || to > 1)
        return -1;
    cur = st_current(obj, &h);
    seq = cur < 0 ? 0u : h.seq;
    if (to < 0)
        to = cur == 0 ? 1 : 0;                        /* write the other copy */
    base = st_sector(obj, (uint32_t)to);
    for (off = 0; off < len; off++)
        st_buf[off] = ((const uint8_t *)src)[off];    /* the driver wants RAM sources */
    if ((rc = st_erase(base)) != 0)
        return rc;
    for (off = 0; off < len; off += 256u) {
        uint32_t n = len - off > 256u ? 256u : len - off;
        if ((rc = st_prog(base + ST_PAYLOAD_OFF + off, st_buf + off, n)) != 0)
            return rc;
    }
    h.magic = ST_MAGIC;
    h.type = (uint16_t)obj;
    h.slot = (uint16_t)to;
    h.seq = seq + 1u;
    h.len = len;
    h.crc = st_crc32(st_buf, len);
    h.rsv[0] = h.rsv[1] = 0xFFFFFFFFu;
    h.hcrc = st_crc32(&h, sizeof h - 4u);
    if ((rc = st_prog(base, &h, sizeof h)) != 0)       /* the commit record, last */
        return rc;
    {   /* read back: a write-protected or failing part must not report SAVED */
        st_hdr_t chk;
        uint32_t c = (uint32_t)to;
        if (st_head(obj, c, &chk) || memcmp(&chk, &h, sizeof h) || st_body(obj, c, &chk))
            return -7;
    }
    return 0;
}
static int st_save_to(uint32_t obj, const void *src, uint32_t len, int to)   /* DUELL 0.2.11: one duck over erase, payload and record */
{
    int rc;
    ST_DUCK_BEGIN();
    rc = st_save_to_x(obj, src, len, to);
    ST_DUCK_END();
    return rc;
}

static int st_save(uint32_t obj, const void *src, uint32_t len) { return st_save_to(obj, src, len, -1); }

/* DUELL 0.2.2, MENU > SYSTEM > ERASE DATA (erase_data.c): an object gone, both copies. The copy that is NOT the current one
 * first, then the current one, so a power cut between the two leaves the old object whole (never the older copy
 * coming back in its place); a cut inside an erase leaves a sector with no valid commit record, which reads as empty (or as
 * the other, still whole, copy). A sector that is blank already is not erased again (no wear, no 40 ms of silence).
 * 0 done, else the failing driver code; an unknown or retired object: -1, nothing touched */
static int st_blank(uint32_t off)
{
    uint32_t i, j;
    for (i = 0; i < ST_SECTOR; i += 256u) {
        if (st_read(off + i, st_buf, 256u))
            return 0;
        for (j = 0; j < 256u; j++)
            if (st_buf[j] != 0xFFu)
                return 0;
    }
    return 1;
}
static int st_wipe(uint32_t obj)
{
    st_hdr_t h;
    uint32_t k;
    int cur, rc;
    if (obj >= OBJ_COUNT || OBJ_RETIRED(obj))
        return -1;
    cur = st_current(obj, &h);                         /* (-1: neither copy is valid: A then B) */
    for (k = 0; k < 2u; k++) {
        uint32_t off = st_sector(obj, cur == 0 ? 1u - k : k);
        if (!st_blank(off) && (rc = st_erase(off)) != 0)
            return rc;
    }
    return 0;
}
