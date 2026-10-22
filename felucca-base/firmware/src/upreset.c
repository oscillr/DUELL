/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* DUELL's PATCH store (SAVE > PATCHES, SPEC 3.6; editor protocol v2, cmds 16-21): ONE list of UP_SLOTS (128) patch slots (owner, 2026-10-15).
 * Slot k holds a SAVED record if one was written (SAVE overwrites the slot it is told to), else the FACTORY sound at browse position k
 * (sounds.c DS_LIST: category, engine, name = the #NN numbers): a virtual fallback, nothing is written at first boot, a fresh or erased
 * unit shows the factory sounds in all 128 slots. RESTORE removes the record of a slot, so the factory sound comes back; the factory sound
 * a slot falls back to is the one at that position in the INSTALLED firmware (the factory set changes between versions; a saved record
 * stays in its slot number: no compatibility is promised before the alpha). The slot layer is at the end of this file (sl_*).
 * Stored COMPACTLY in sixteen storage.c objects (chunks 0..15: OBJ_PATCH0..5, A/B sector pairs 0xC8000..0xCDFFF USR3's old place, 0xDC000..0xDFFFF
 * Felucca's retired preset banks, 0x9F000 / 0xFE000 the retired FM6 bank's pair; and, DUELL 0.2.15, OBJ_PATCHX0..9 = chunks 6..15 at 0xA6000..0xB9FFF),
 * one CHUNK each (at most PT_CAP bytes: the payload ends at sector offset 0xF00, where the update loader scans). NOT mirrored in RAM
 * (DUELL 0.2.15; the six mirrored chunks took 23 KB of the pool): RAM holds only a DIRECTORY (pt_dir: per slot its chunk, offset, length,
 * revision and what the lists need: name, engine, category, shadow marker), built at boot from the chunks; a record is read from flash
 * when it is loaded or edited (pt_raw: a ring of four), a write loads the one chunk it changes into a work buffer (pt_work).
 * The limit is HONEST and it is the slot count: PT_WORST is the largest record there can be (an FM6 sound with its patch, a SEQ-mode arp
 * with a full 16-step pattern, End B, a shadow marker, every parameter off its default), PT_K of them fit a chunk with its header,
 * PT_MAX = PT_NCH x PT_K = 128 = UP_SLOTS records of any kind always fit, so every one of the 128 slots can always be written, whatever
 * the other 127 hold. A chunk takes a NEW record only under PT_K records, which keeps that guarantee. There is no 'full': a slot is never
 * refused for room. PT_FULL (no chunk has room) cannot happen below that; it stays as a guard and reads as a plain SAVE ERROR.
 *
 * Chunk: u16 'PT' magic, u8 version PT_VER, u8 0, then records back to back. Record (little endian):
 *   0 u16 length of the whole record     2 u8 slot (0..127)       3 u8 revision (+1 at every rewrite; a record that moved to
 *   4 u8 flags PTF_*                      5 u8 engine              another chunk exists twice for a moment: the newer wins)
 *   6 u8 category (raw, 0 none)           7 12 bytes name, 0-padded                19 u8 npairs (nvalues with PTF_BM)
 *   20 [PTF_XB] the EXTENSION BLOCK (PT_VER 3; up_xb.h: [L][XV][section]...: the 16 extension slots' values off their defaults and the hand-set B edits;
 *      it replaces the retired 2-byte shadow marker, same place)     [PTF_MB] 7 bytes End B (morph block)
 *      npairs x (u8 parameter id, u8 value + 64)  SPARSE: only what differs from the engine's INIT (the defaults of
 *      params.c, as the factory sounds' format, sounds.c). PTF_BM (a sound that differs in more than 16 values, the encoder
 *      takes whichever is shorter): a 16-byte bitmap (bit id) instead, then nvalues x (u8 value + 64) in id order
 *      [PTF_PAT] 16 notes + 16 flags    [PTF_FM6] the FM6 sound's patch, UPF_PK bytes
 * A write rebuilds only the chunk that changed and saves it as any storage.c object (erase the other sector, payload, commit
 * record LAST, read back): a power cut leaves the old chunk. A record that no longer fits its chunk goes to another one
 * first, then is cut from the old (up_put). The decoded form (up_rec_t, below) is what the rest of the firmware and the editor
 * see; up_rec() decodes into a ring of four.
 *
 * Felucca's own banks (32 slots, 192-byte records, 0xDC000..) are not read: DUELL is not released on a device that has them.
 *
 * DUELL 0.2.14: only a record of today's form is read. A stored record carries PTF_CB (below); one without it was written by
 * firmware before 0.2.14 (its CRUSH BITS on the old curve) and is no record: pt_rec_ok refuses it, pt_scan_img leaves it out of the directory
 * (the flash chunk follows at its next write), the slot reads as unused, and a backup PUT of a chunk that holds one is refused (rc 2).
 * The decoded record (up_rec_t) is always UP_VER with P_COUNT values, a byte each: the records of Felucca's versions 1..5 (16-bit values,
 * its layout of ids 8, 61..80 and 83..90), the drum-grid versions and the mapping of a record by parameter count are gone, and so are
 * the conversions of PHYS MODEL 2 (DUST) and of the filter's CUT of before 0.2.9. A record of PHYS MODEL 4 or SAMPLE SET 4, or of the
 * retired engines (DIGITAL, SAMPLE, GRAIN, DRUM, SLICE: 1, 4, 8, 10, 13), stays as stored and loads onto the silent placeholder
 * (engines.c ENG_RETIRED).
 * A note pattern keeps each step's RATCH in its flags (core.h SF_RATCH, 8 | 16; 0 = x1). A pattern keeps no step's NUDGE (core.h
 * SF_NUDGE: its bit 4 would read as the tie here): the step on its start.
 *
 * With -DUP_HOST (host test) only the part above #ifndef UP_HOST is built;
 * it needs nothing but core.h. */
#define UP_PMAX 72u                              /* room for P_COUNT to grow */
#define UP_USED 0xA5u
#define UP_VER 6u                                /* the one form of the decoded record: DUELL's layout (core.h), a byte a value */
typedef struct {
    uint8_t used, ver, engine, np;               /* UP_USED, UP_VER, engine, P_COUNT when stored */
    char name[12];                               /* ASCII 32..126, 0-padded (no 0 when 12 long) */
    uint8_t packed[UP_PMAX * 2u];                /* the values, +64 a byte each; past np: the category, the morph block, the shadow marker */
    uint8_t note[16], flags[16];                 /* note 0 = rest; flags 1 accent, 2 slide, 4 tie, SF_RATCH */
    uint8_t xz[NX];                              /* the extension slots (ext.h XV), value + 64 XOR the slot's default (up_xb.h XB_DEFS): ZERO IS THE DEFAULT, so a record that was zeroed means defaults */
    uint8_t xb_n;                                /* the hand-set B edits (up_xb.h; EDITABLE_B_DESIGN section 3): their number (0: none) .. */
    xb_edit_t xb[XB_MAX];                        /* .. and the edits (absolute End B values). Nothing in this job reads or writes them but the store and the tests; job G does */
} up_rec_t;
_Static_assert(sizeof(up_rec_t) == 192u + NX + 2u + 4u * XB_MAX, "user preset record layout");
_Static_assert(P_COUNT <= UP_PMAX * 2u && P_COUNT < 128, "user preset record: P_COUNT");
/* 1.2 (Discussion #90): the sound's category (category.c CAT_*) in the record's last byte, packed[UP_CAT_AT]: the values take
 * np = P_COUNT (< 128) of its 144 bytes and nothing reads the others (up_valid neither); 0 there is CAT_NONE: listed by its engine,
 * cat_of_engine */
#define UP_CAT_AT (UP_PMAX * 2u - 1u)
/* DUELL 3.5 (SAVE held: the patch saved over itself): End B of the patch in the 7 bytes before the category, packed[136..142]: 'M', the
 * seed (16 bit), AMOUNT, the roll's bits (morph.c MC_*), 0, a check byte (morph.c morph_blk_ok). Same reasoning as the
 * category: a record's values take np = P_COUNT of the 144 bytes, nothing reads the others; 0 there is no marker (no B, a load
 * rolls a fresh one). UP_PUT from the editor writes none */
#define UP_MB_N 7u
#define UP_MB_AT (UP_CAT_AT - UP_MB_N)
_Static_assert(P_COUNT <= UP_MB_AT, "user preset record: P_COUNT leaves the morph block its room");
/* The two bytes before the morph block, packed[134..135], were the SHADOW marker of the 2026-10-11 save flow (a copy of a factory sound
 * named its engine + 1 and preset + 1). 0.2.17 has one list of slots and no shadows; since PT_VER 3 the record flag of the marker (16) is PTF_XB, the
 * EXTENSION BLOCK. The bytes stay reserved in up_rec_t */
#define UP_SH_AT (UP_MB_AT - 2u)
_Static_assert(P_COUNT <= UP_SH_AT, "user preset record: P_COUNT leaves the shadow marker its room");
#include "category.c"
static uint32_t up_cat_of(const up_rec_t *r)
{
    uint32_t c = r->np <= UP_CAT_AT ? r->packed[UP_CAT_AT] : 0u;
    return c && c < CAT_N ? c : cat_of_engine(r->engine);
}
static void up_set_cat(up_rec_t *r, uint32_t c)  /* c CAT_BASS..CAT_OTHER */
{
    if (c && c < CAT_N)
        r->packed[UP_CAT_AT] = (uint8_t)c;
}

static int up_valid(const up_rec_t *r)
{
    if (!(r->used == UP_USED && r->ver == UP_VER && r->engine < NENGINES && r->xb_n <= XB_MAX &&
          r->np >= 8u && r->np <= UP_PMAX * 2u && r->name[0])) return 0;
    for (uint32_t i = 0; i < r->np; i++) if (r->packed[i] > 191u) return 0;
    return 1;
}

static int16_t up_value(const up_rec_t *r, uint32_t k) { return (int16_t)r->packed[k] - 64; }
static int16_t up_xval(const up_rec_t *r, uint32_t k) { return (int16_t)(r->xz[k] ^ xb_def(k)) - 64; }              /* extension slot k of the record */
static void up_set_xval(up_rec_t *r, uint32_t k, int16_t v) { r->xz[k] = (uint8_t)(((v < -64 ? -64 : v > 127 ? 127 : v) + 64) ^ xb_def(k)); }
static void up_set_value(up_rec_t *r, uint32_t k, int16_t v) { r->packed[k] = (uint8_t)(v + 64); }
static void up_pat_norm(uint8_t *note, uint8_t *flags);

/* the sparse store's default of parameter id for engine eng: params.c's, except the two LFO TRIGs, the DUCK's CURVE and the multimode
 * filter's CUT and SLOPE, KEY TRK (0.2.22: params.c made 32 the default, the store keeps 0) and the old LEVEL (id 0, 0.2.19). The record format leaves out what equals this default, so it is part of the stored format and must not change
 * with params.c's: NOTE (0) for the TRIGs, LIN (0) for the CURVE (params.c made FREE and EXP the defaults), 0 for CUT and SLOPE (params.c: the
 * open end and 12 dB); a FREE one, an EXP one, an open CUT and a 12 dB SLOPE store their pair (2 bytes each; PT_WORST counts every
 * parameter off its default anyway) */
static int32_t up_def(uint32_t eng, uint32_t id)
{
    if (id == P_LEVEL)                           /* 0.2.19: the stored default of id 0 is the old LEVEL's 104 (params.c: the mixer level is 112 now): a record before PTF_TR left it out at 104 and
                                                  * means a trim of 104 by it; a record now never writes the mixer level (pt_encode), so it reads as this */
        return 104;
    return id == P_LTRIG || id == P_L2TRIG || id == P_DKCRV || id == P_FREEA || id == P_FREED || id == P_KTRK || id == P_DKSRC ? 0 : param_desc_of(eng, id)->def;   /* (0.2.24: DUCK SRC's stored default stays PUMP (0): a record that left it out still reads PUMP; a new sound's default is OFF, params.c) */
}

static int up_name_ok(const uint8_t *s, uint32_t n)   /* 1..12 printable ASCII */
{
    uint32_t i;
    if (!n || n > 12u)
        return 0;
    for (i = 0; i < n; i++)
        if (s[i] < 32u || s[i] > 126u)
            return 0;
    return 1;
}

static void up_pat_norm(uint8_t *note, uint8_t *flags)   /* tie: no note; rest: no flags */
{
    *note &= 127u;
    if (*flags & 4u) {
        *note = 0;
        *flags = 4;
    } else {
        *flags = *note ? (uint8_t)(*flags & (SF_ACCENT | SF_SLIDE | SF_RATCH)) : 0u;
    }
}

static void up_pat_from(up_rec_t *r, const step_t *st)   /* the first 16 steps -> the pattern */
{
    uint32_t i;
    for (i = 0; i < 16u; i++) {
        r->note[i] = st[i].time == ST_NOTE && st[i].n ? st[i].note[0] : 0u;
        r->flags[i] = st[i].time == ST_TIE ? 4u : (uint8_t)(st[i].flags & ~SF_NUDGE);   /* (no nudge: 4 is the tie here) */
        up_pat_norm(&r->note[i], &r->flags[i]);
    }
}

static int up_pat_empty(const up_rec_t *r)
{
    uint32_t i;
    for (i = 0; i < 16u; i++)
        if (r->note[i])
            return 0;
    return 1;
}

/* UP_PUT arguments: slot, engine, name, P_COUNT x v14, 16 x (note, flags) [, kind, 16 x hi [, category]] -> *r
 * (values not yet clamped); 0 ok, 1 bad arguments. *slot gets the slot byte when there is one. The kind is 0 (1 was a drum grid:
 * gone in 0.2.14, refused as bad arguments), then 16 bytes, ignored.
 * category (1.2, category.c CAT_*, 0 none): after the kind's 16 bytes */
static int up_parse(const uint8_t *a, uint32_t na, up_rec_t *r, uint32_t *slot)
{
    uint32_t i, n, k, end;
    if (na < 3u)
        return 1;
    *slot = a[0];
    for (n = 0; 2u + n < na && a[2 + n]; n++)
        ;
    k = 3u + n;                                  /* after the name's 0 */
    end = k + 2u * P_COUNT + 32u;
    if (a[0] >= UP_SLOTS || a[1] >= NENGINES || 2u + n >= na || !up_name_ok(a + 2, n) ||
        (na != end && ((na != end + 17u && na != end + 18u) || a[end] != 0u)) || (na == end + 18u && a[end + 17u] >= CAT_N))
        return 1;
    up_rec_t staged, *target = r;
    r = &staged;
    memset(r, 0, sizeof *r);
    r->used = UP_USED;
    r->ver = UP_VER;
    r->engine = a[1];
    r->np = P_COUNT;                             /* (the extension slots: xz 0 = their defaults; no hand-set B edits: UP_PUT carries none) */
    for (i = 0; i < n; i++)
        r->name[i] = (char)a[2 + i];
    for (i = 0; i < P_COUNT; i++, k += 2u)
        {
            int32_t value = (int32_t)((a[k] & 127u) | (a[k + 1] & 127u) << 7) - 8192;
            if (value < -64 || value > 127) return 1;
            up_set_value(r, i, (int16_t)value);
        }
    for (i = 0; i < 16u; i++, k += 2u) {
        r->note[i] = a[k];
        r->flags[i] = a[k + 1];
        up_pat_norm(&r->note[i], &r->flags[i]);
    }
    if (na == end + 18u)
        up_set_cat(r, a[end + 17u]);
    *target = *r;
    return 0;
}

#ifndef UP_HOST
static void up_values(const up_rec_t *r, int16_t *v)   /* the record's values clamped for its engine */
{
    uint32_t i;
    for (i = 0; i < P_COUNT; i++)
        v[i] = (int16_t)param_fit(param_desc_of(r->engine, i), up_value(r, i));
}


/* ---------------------------------------------------------------- the store --- */
#define PT_NCH 22u                               /* chunks = storage.c objects: 0..5 OBJ_PATCH0.., 6..21 OBJ_PATCHX0.. (pt_obj); 16..21 = objects 43..48 at 0xBA000..0xC5FFF (the cloud plan's job A) */
#define PT_CAP 3584u                             /* a chunk's most bytes: the payload (at sector offset 256) ends at 0xF00 (ST_PAYLOAD_END) */
#define PT_HDR 4u
#define PT_MAGIC 0x5450u                         /* "PT" */
#define PT_FIX 20u                               /* a record's fixed part */
#define PT_VER 3u                                /* the chunk image's version (byte 2 after "PT"): 3 = the extension block (PTF_XB: replaces the retired shadow marker), 6 records a chunk, 22 chunks; 2 = DUELL 0.2.22's DUCK SRC (2 is MIDI, the channel and the note are the device's: the sidechain setup); 2 and 1 are refused (no backward compatibility before the alpha) */
#define PT_NONE 0xFFFFu
#define PT_NOCH 0xFFu                            /* a directory entry with no record */
#define PT_FULL 4                                /* up_put: no chunk has room (a guard only: PT_MAX records always fit; reads as SAVE ERROR) */
#define PT_BMB 16u                               /* PTF_BM's bitmap: ids 0..127 */
#define PTF_PAT 1u
#define PTF_TR 2u                                /* 0.2.19: the TRIM form: the loudness is the pair of P_TRIM (the PATCH TRIM) and the record carries no MIXER LEVEL (P_LEVEL is never written). A record
                                                  * WITHOUT it (0.2.14 .. 0.2.18) stored the loudness as P_LEVEL: up_rec reads that as the trim. Always set in a record written now */
#define PTF_FM6 4u
#define PTF_MB 8u
#define PTF_XB 16u                               /* PT_VER 3: the EXTENSION BLOCK (up_xb.h) sits after the fixed part, where the 2-byte shadow marker was (retired 0.2.17, never written since) */
#define PTF_AK 32u                               /* DUELL: the ARP / KEYS / SEQUENCER block (ak_* below), the record's last part */
#define PTF_BM 64u                               /* the record's values as a bitmap + values, not (id, value) pairs (shorter above 16) */
#define PTF_CB 128u                              /* 0.2.14: always set in a record written now (its CRUSH BITS, INS A, is the curve of ins_bits.h); a record without it is
                                                  * of firmware before 0.2.14 and no record (pt_rec_ok). Bit 2 (2) was the drum grid, never set: 0.2.19 uses it as PTF_TR */
#define PTF_ALL 255u
#define UPF_PK 112u                              /* an FM6 patch, FM6_PACKED 7-bit bytes packed 8 to 7 (112 bytes) */
#define AK_MAX 190u                              /* the AK block's most bytes with its length (ak_build) */
#define AK_NS 16u
#define PS_STEP_N 9u                             /* patslot.c PS_STEP */
/* the worst record: AK_WORST is a block with all sixteen values and all sixteen steps (u16 length, g, bitmap, values, step map,
 * steps); PT_W_SEQ a record that carries it with an FM6 patch, End B, a shadow marker and every other value off its default
 * (and no pattern of its own: the block's steps are it); PT_W_PLAIN one with no block but the pattern and the patch. PT_K of the
 * worst fit a chunk (its 4-byte header included), PT_MAX = PT_NCH x PT_K always fit: the limit of the store (test: patch_test) */
#define AK_WORST (2u + 1u + 2u + 16u + 2u + PS_STEP_N * AK_NS)
#define PT_W_SEQ (PT_FIX + XB_BLOCK_MAX + UP_MB_N + PT_BMB + (P_COUNT - 17u) + UPF_PK + AK_WORST)       /* (0.2.19: one value fewer, the MIXER LEVEL is never written; PT_VER 3: the extension block at its biggest, every slot off its default and XB_MAX edits, up_xb.h, was the 2-byte shadow marker) */
#define PT_W_PLAIN (PT_FIX + XB_BLOCK_MAX + UP_MB_N + PT_BMB + (P_COUNT - 1u) + 32u + UPF_PK)
#define PT_WORST (PT_W_SEQ > PT_W_PLAIN ? PT_W_SEQ : PT_W_PLAIN)
#define PT_K ((PT_CAP - PT_HDR) / PT_WORST)
#define PT_MAX (PT_NCH * PT_K)                   /* 132 records of the worst size: at least the 128 slots (a chunk takes a NEW record only under PT_K) */
#if FELUCCA_FLASH
_Static_assert(PT_CAP + ST_PAYLOAD_OFF == ST_PAYLOAD_END && PT_CAP <= ST_PAYLOAD_MAX, "a patch chunk is one storage object whose payload ends at 0xF00");
#endif
_Static_assert(PT_K == 6u && PT_WORST * PT_K + PT_HDR <= PT_CAP && PT_MAX >= UP_SLOTS && AK_WORST <= AK_MAX, "the store's limit: 6 records of the worst size fit a chunk, and PT_MAX is at least the slot count (all 128 always fit)");
_Static_assert(PT_NCH == 22u, "pt_obj, the erase list (erase_data.c), the backup (editor_backup.c)");
_Static_assert(PT_FIX + XB_BLOCK_MAX + UP_MB_N + 2u * P_COUNT + 32u + UPF_PK + AK_MAX <= 768u, "pt_enc holds a record");
_Static_assert(P_COUNT <= PT_BMB * 8u, "PTF_BM's bitmap holds every parameter id");
/* the directory: slot -> its record and what the lists need without reading flash */
typedef struct {
    uint16_t off, len;                           /* the record's offset in its chunk's payload, its length */
    uint8_t ch, rev;                             /* its chunk (PT_NOCH: unused slot), its revision */
    uint8_t eng, cat;                            /* engine, category (raw, 0 none) */
    char name[12];                               /* as stored: 0-padded, no 0 when 12 long */
} pt_ent_t;
static pt_ent_t pt_dir[UP_SLOTS] __attribute__((section(".pool")));
static uint16_t pt_len[PT_NCH] __attribute__((section(".pool")));   /* each chunk's live bytes: header + its directory records (0: none stored) */
static uint8_t pt_n[PT_NCH] __attribute__((section(".pool")));      /* .. and its live records */
static int8_t pt_cur[PT_NCH] __attribute__((section(".pool")));     /* the copy (A 0 / B 1) that holds the chunk in flash, -1 none */
static uint8_t pt_work[PT_CAP] __attribute__((aligned(4), section(".pool")));   /* the chunk being rebuilt, a loaded one (main loop only) */
#if !FELUCCA_FLASH
static uint8_t pt_ram[PT_NCH][PT_CAP];           /* no flash (host tests, the browser build): the chunks as flash would hold them */
static uint16_t pt_ramlen[PT_NCH];
#endif
static uint8_t pt_enc[768] SCRATCH_BSS;   /* (memory trim 1: ordinary RAM, main loop only; cloud job A: 768 B for the extension block's worst record) */
static const uint8_t *up_ak_in;                  /* the AK block (u16 length, content) the record up_put writes carries (0: none) */
static const uint8_t *up_pk_in;                  /* the FM6 patch (FM6_PACKED bytes) the record up_put writes carries (0: none) */

/* a 128-byte FM6 record's 7-bit bytes, 8 to 7 bytes (UPF_PK) and back (the voice bank too, fm6_vbank.c) */
static void upf_pk_get(const uint8_t *d, uint8_t *pk)
{
    uint32_t i, acc = 0, n = 0, o = 0;
    for (i = 0; i < FM6_PACKED; i++) {
        while (n < 7u) {
            acc |= (uint32_t)d[o++] << n;
            n += 8u;
        }
        pk[i] = (uint8_t)(acc & 127u);
        acc >>= 7;
        n -= 7u;
    }
}
static void upf_pk_put(const uint8_t *pk, uint8_t *d)
{
    uint32_t i, acc = 0, n = 0, o = 0;
    memset(d, 0, UPF_PK);
    for (i = 0; i < FM6_PACKED; i++) {
        acc |= (uint32_t)(pk[i] & 127u) << n;
        n += 7u;
        while (n >= 8u) {
            d[o++] = (uint8_t)acc;
            acc >>= 8;
            n -= 8u;
        }
    }
}

static uint32_t pt_rd16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
/* where a record's optional parts start: *xb (the extension block), *mb, *pairs, *pat, *fm, *ak (offsets); the total length. The AK block (u16 length,
 * content) is last; one that does not fit the record's own length makes the total wrong (pt_rec_ok refuses it) */
static uint32_t pt_layout_x(const uint8_t *p, uint32_t *xbo, uint32_t *mb, uint32_t *pr, uint32_t *pat, uint32_t *fm, uint32_t *ak)
{
    uint32_t f = p[4], o = PT_FIX;
    *xbo = o;
    if (f & PTF_XB) {
        if (o + 1u > pt_rd16(p))                 /* no byte for the block's length */
            return o + (1u << 16);
        o += 1u + p[o];
    }
    *mb = o;
    o += f & PTF_MB ? UP_MB_N : 0u;
    *pr = o;
    o += f & PTF_BM ? PT_BMB + p[19] : 2u * p[19];
    *pat = o;
    o += f & PTF_PAT ? 32u : 0u;
    *fm = o;
    o += f & PTF_FM6 ? UPF_PK : 0u;
    *ak = o;
    if (f & PTF_AK) {
        if (o + 2u > pt_rd16(p))
            return o + (1u << 16);
        o += 2u + pt_rd16(p + o);
    }
    return o;
}
static uint32_t pt_layout(const uint8_t *p, uint32_t *xbo, uint32_t *mb, uint32_t *pr, uint32_t *pat, uint32_t *fm)
{
    uint32_t ak;
    return pt_layout_x(p, xbo, mb, pr, pat, fm, &ak);
}
static int pt_rec_ok(const uint8_t *p, uint32_t rl)   /* a well-formed record of rl bytes */
{
    uint32_t a, b, c, d, e, i, n = 0;
    if (rl < PT_FIX || rl > PT_WORST || pt_rd16(p) != rl || p[2] >= UP_SLOTS || p[5] >= NENGINES || (p[4] & ~PTF_ALL) || !(p[4] & PTF_CB) || !p[7])
        return 0;
    if (pt_layout(p, &a, &b, &c, &d, &e) != rl)
        return 0;
    if ((p[4] & PTF_XB) && xb_block_ok(p + a, rl - a) != 1u + p[a])   /* the block: lengths that add up, the right version, values and ids that make sense */
        return 0;
    if (p[4] & PTF_BM) {                         /* the bitmap: as many bits as values, none past P_COUNT */
        for (i = 0; i < PT_BMB * 8u; i++)
            if (p[c + i / 8u] >> (i % 8u) & 1u) {
                if (i >= P_COUNT)
                    return 0;
                n++;
            }
        return n == p[19];
    }
    return 1;
}
/* ---- the flash (or, with no flash, RAM) side of a chunk ---- */
#if FELUCCA_FLASH
static uint32_t pt_obj(uint32_t c) { return c < 6u ? OBJ_PATCH0 + c : OBJ_PATCHX0 + (c - 6u); }
#else
static uint32_t pt_obj(uint32_t c) { return c; }   /* (no flash: no storage.c objects) */
#endif
/* chunk c's payload -> dst (PT_CAP bytes at most); its length, -1 when there is none (or it is damaged). Notes which copy holds it. */
static int pt_ld(uint32_t c, uint8_t *dst)
{
#if FELUCCA_FLASH
    st_hdr_t h;
    int cur;
    pt_cur[c] = -1;
    if (!flash_ok || (cur = st_current(pt_obj(c), &h)) < 0 || h.len > PT_CAP)
        return -1;
    pt_cur[c] = (int8_t)cur;
    memcpy(dst, st_buf, h.len);
    return (int)h.len;
#else
    if (!pt_ramlen[c])
        return -1;
    memcpy(dst, pt_ram[c], pt_ramlen[c]);
    return (int)pt_ramlen[c];
#endif
}
/* n bytes at offset off of chunk c's payload -> dst: 0 ok */
static int pt_rd(uint32_t c, uint32_t off, uint32_t n, uint8_t *dst)
{
#if FELUCCA_FLASH
    if (!flash_ok || pt_cur[c] < 0 || off + n > PT_CAP)
        return -1;
    return st_read(st_sector(pt_obj(c), (uint32_t)pt_cur[c]) + ST_PAYLOAD_OFF + off, dst, n);
#else
    if (off + n > pt_ramlen[c])
        return -1;
    memcpy(dst, pt_ram[c] + off, n);
    return 0;
#endif
}
/* chunk c := img (len bytes, at most PT_CAP): the standard commit-last write into the copy that is not the current one.
 * 0 ok, 2 flash error, 3 no flash (kept in RAM) */
static int pt_sv(uint32_t c, const uint8_t *img, uint32_t len)
{
#if FELUCCA_FLASH
    int to = pt_cur[c] == 0 ? 1 : 0;
    if (!flash_ok || len > PT_CAP || st_save_to(pt_obj(c), img, len, to))
        return 2;
    pt_cur[c] = (int8_t)to;
    return 0;
#else
    memcpy(pt_ram[c], img, len);
    pt_ramlen[c] = (uint16_t)len;
    return 3;
#endif
}

/* ---- the directory ---- */
static uint32_t pt_rgen __attribute__((section(".pool")));            /* +1 whenever the chunks change: the read records (pt_raw) are stale */
static void pt_dir_chunk_out(uint32_t c)         /* the records of chunk c out of the directory */
{
    uint32_t k;
    for (k = 0; k < UP_SLOTS; k++)
        if (pt_dir[k].ch == c)
            pt_dir[k].ch = PT_NOCH;
}
static void pt_tally(void)                       /* each chunk's live records and bytes, from the directory */
{
    uint32_t k, c;
    for (c = 0; c < PT_NCH; c++)
        pt_len[c] = pt_n[c] = 0;
    for (k = 0; k < UP_SLOTS; k++)
        if (pt_dir[k].ch != PT_NOCH) {
            pt_n[pt_dir[k].ch]++;
            pt_len[pt_dir[k].ch] = (uint16_t)(pt_len[pt_dir[k].ch] + pt_dir[k].len);
        }
    for (c = 0; c < PT_NCH; c++)
        if (pt_n[c])
            pt_len[c] = (uint16_t)(pt_len[c] + PT_HDR);
    pt_rgen++;
}
/* chunk c's image (len bytes) into the directory: a record becomes its slot's entry when the slot has none or it has the newer revision
 * (a record twice, a move cut short, keeps the newer; an equal one the first); a record that is no record is passed over (flash follows
 * at the chunk's next write: pt_build) */
static void pt_scan_img(uint32_t c, const uint8_t *img, uint32_t len)
{
    uint32_t off;
    if (len < PT_HDR || pt_rd16(img) != PT_MAGIC || img[2] != PT_VER)
        return;
    for (off = PT_HDR; off + PT_FIX <= len;) {
        const uint8_t *p = img + off;
        uint32_t rl = pt_rd16(p);
        pt_ent_t *e;
        if (rl < PT_FIX || off + rl > len)       /* damaged tail: the rest is gone */
            break;
        if (pt_rec_ok(p, rl)) {
            e = &pt_dir[p[2]];
            if (e->ch == PT_NOCH || (uint8_t)(p[3] - e->rev) - 1u < 127u) {
                e->off = (uint16_t)off;
                e->len = (uint16_t)rl;
                e->ch = (uint8_t)c;
                e->rev = p[3];
                e->eng = p[5];
                e->cat = p[6];
                memcpy(e->name, p + 7, 12);
            }
        }
        off += rl;
    }
}
static void pt_scan_all(void)                    /* the directory again from all chunks (boot, a restore, after a failed write) */
{
    uint32_t c, k;
    for (k = 0; k < UP_SLOTS; k++)
        pt_dir[k].ch = PT_NOCH;
    for (c = 0; c < PT_NCH; c++) {
        int n = pt_ld(c, pt_work);
        if (n > 0)
            pt_scan_img(c, pt_work, (uint32_t)n);
    }
    pt_tally();
}
static int up_used(uint32_t k) { return k < UP_SLOTS && pt_dir[k].ch != PT_NOCH; }
/* slot k's record read from its chunk (a ring of four: the pointer stays good through three more slots; a write ends it); 0: unused */
static uint8_t pt_rbuf[4][(PT_WORST + 3u) & ~3u] __attribute__((aligned(4), section(".bss.scr")));   /* (wave 5: ordinary RAM, main loop only: the pool tail) */
static uint8_t pt_rslot[4] SCRATCH_BSS;
static uint32_t pt_rver[4] SCRATCH_BSS, pt_rn SCRATCH_BSS;
static const uint8_t *pt_raw(uint32_t k)
{
    const pt_ent_t *e;
    uint32_t i;
    if (!up_used(k))
        return 0;
    for (i = 0; i < 4u; i++)
        if (pt_rslot[i] == k && pt_rver[i] == pt_rgen)
            return pt_rbuf[i];
    e = &pt_dir[k];
    i = pt_rn++ & 3u;
    pt_rver[i] = 0;
    if (pt_rd(e->ch, e->off, e->len, pt_rbuf[i]) || pt_rd16(pt_rbuf[i]) != e->len || pt_rbuf[i][2] != k)
        return 0;
    pt_rslot[i] = (uint8_t)k;
    pt_rver[i] = pt_rgen;
    return pt_rbuf[i];
}

/* ---------------------------------------------------------------- the AK block (DUELL: arp, keys, sequencer) ---
 * A patch carries the part's ARP, its chord / voicing keys and its SEQUENCER pattern, so loading it brings them back (up_load, ui.c ds_arp).
 * DUELL 1.0 (owner, 2026-10-21): EVERY patch restores its own arp state, ON and OFF; there are no factory and user kinds. The block is therefore ALWAYS written:
 * the arp group and the keys group are always present (an arp that is off is the group with all eight values at their defaults, 4 bytes), the SEQ group when the
 * arp mode is SEQ. A record without the block (written by firmware before 1.0, or by the editor's UP_PUT) reads as arp OFF, keys at their defaults, no pattern
 * of its own: the same as a block of defaults. PT_VER stays 3: such a record still reads, with the new rule. The record's last
 * part, flag PTF_AK: u16 content length, then
 *   u8 g       groups present: AK_ARP 1 (the arp's eight values P_AMODE .. P_AORDER), AK_KEYS 2 (P_CHRD P_VOIC; the SCALE slot, ordinal 8,
 *              is retired: read and discarded, never written), AK_SEQ 4 (the sequencer pattern; written when the arp mode is SEQ, as the pattern is then the arp's)
 *   u8 np      then np x (u8 parameter id, u8 value + 64): the values of the present groups that differ from the defaults
 *              (the groups' other values are the defaults); AK_SEQ's ids are LEN DIV SWING GATE QUANTIZE (LEN at most 16).
 *              With g bit 8 (AK_BM, from 3 values on: shorter) there is no np: a u16 bitmap (bit = the value's ordinal: the arp's
 *              eight 0..7, the keys' three 8..10, the sequencer's five 11..15) then one u8 (value + 64) per set bit, in ordinal order
 *   AK_SEQ:    u16 map (bit i: step i of 16 is stored; a step not stored is empty), then 9 bytes per stored step in the
 *              multi's / pattern slots' packing (patslot.c ps_pack_step: notes, time, accent, slide, RATCH, NUDGE, velocity,
 *              CHANCE, the hits)
 * The ids of the present groups are not stored in the record's own pairs (up_rec puts them back). ARP TARGET
 * (P_ATGT) is not in it: it is the sound's own, a pair of the record as ever. The pattern is 16 steps (the record's old pattern,
 * PTF_PAT, is these steps' notes and flags: not stored twice). A MULTI holds all of it already: every part's arp values and its 64 steps (multi.c). */
#define AK_ARP 1u
#define AK_KEYS 2u
#define AK_SEQ 4u
#define AK_BM 8u                                 /* (g's bit 8: the values as a bitmap, see above) */
static void ps_pack_step(const step_t *s, uint8_t *b);   /* (patslot.c) */
static int ps_unpack_step(const uint8_t *b, step_t *s);
/* the keys' three ordinals 8..10. The SCALE (ordinal 8, once the id P_SCALE) is RETIRED (owner 2026-10-12: the scale is the instrument's, core.h): a record that holds one
 * still reads (the parse stays as it was) and the value is DISCARDED; ak_build never writes it */
#define AK_NOSCALE P_COUNT                       /* the retired ordinal's id: not an id (P_COUNT is the reserved 127; param_desc_of gives it the free descriptor). The ids 25 and 26 are the TWIN FILTER's RES 2 and DRIVE now (cloud job B) */
static const uint8_t AK_KEYID[3] = {AK_NOSCALE, P_CHRD, P_VOIC};
static const uint8_t AK_SEQID[5] = {P_SLEN, P_SDIV, P_SSWING, P_SGATE, P_SQNT};
static uint32_t ak_id(uint32_t o)                /* the ordinal o (0..15) -> its parameter id */
{
    return o < 8u ? P_AMODE + o : o < 11u ? AK_KEYID[o - 8u] : AK_SEQID[o - 11u];
}
static uint32_t ak_grp(uint32_t id)              /* the group an id belongs to (0: none) */
{
    uint32_t i;
    if (id >= P_AMODE && id <= P_AORDER)
        return AK_ARP;
    for (i = 0; i < 3u; i++)
        if (id == AK_KEYID[i])
            return AK_KEYS;
    for (i = 0; i < 5u; i++)
        if (id == AK_SEQID[i])
            return AK_SEQ;
    return 0;
}
typedef struct {
    uint8_t g;
    int16_t arp[8], key[3], seq[5];              /* P_AMODE.., AK_KEYID, AK_SEQID order */
    step_t st[AK_NS];
} ak_t;
static int16_t *ak_slot(ak_t *a, uint32_t id)
{
    uint32_t i;
    if (id >= P_AMODE && id <= P_AORDER)
        return &a->arp[id - P_AMODE];
    for (i = 0; i < 3u; i++)
        if (id == AK_KEYID[i])
            return &a->key[i];
    for (i = 0; i < 5u; i++)
        if (id == AK_SEQID[i])
            return &a->seq[i];
    return 0;
}
static int ak_step_empty(const step_t *s)
{
    uint8_t a[PS_STEP_N], b[PS_STEP_N];
    step_t z;
    memset(&z, 0, sizeof z);
    step_clear(&z);
    ps_pack_step(s, a);
    ps_pack_step(&z, b);
    return !memcmp(a, b, sizeof a);
}
/* content c (n bytes) of a record of engine eng -> a; 0: it is no block (ignored) */
static int ak_dec(const uint8_t *c, uint32_t n, uint32_t eng, ak_t *a)
{
    uint32_t np, at = 2u, i, map;
    int16_t *d;
    if (n < 2u || (c[0] & ~15u) || !(c[0] & 7u))
        return 0;
    if (c[0] & AK_BM) {                          /* the bitmap form: a u16 (bit = ordinal), then a value per bit */
        uint32_t bm = (uint32_t)c[1] | (uint32_t)c[2] << 8;
        if (n < 3u)
            return 0;
        for (np = 0, i = 0; i < 16u; i++)
            np += bm >> i & 1u;
        if (3u + np > n)
            return 0;
        memset(a, 0, sizeof *a);
        a->g = c[0] & 7u;
        for (i = 0; i < 8u; i++)
            a->arp[i] = param_desc_of(eng, P_AMODE + i)->def;
        for (i = 0; i < 3u; i++)
            a->key[i] = param_desc_of(eng, AK_KEYID[i])->def;
        for (i = 0; i < 5u; i++)
            a->seq[i] = param_desc_of(eng, AK_SEQID[i])->def;
        for (at = 3u, i = 0; i < 16u; i++)
            if (bm >> i & 1u) {
                uint32_t id = ak_id(i);
                if (!(ak_grp(id) & a->g) || c[at] > 191u)
                    return 0;
                *ak_slot(a, id) = (int16_t)param_fit(param_desc_of(eng, id), (int16_t)c[at] - 64);
                at++;
            }
        np = 0;
        goto seq;
    }
    np = c[1];
    if (at + 2u * np > n)
        return 0;
    memset(a, 0, sizeof *a);
    a->g = c[0];
    for (i = 0; i < 8u; i++)
        a->arp[i] = param_desc_of(eng, P_AMODE + i)->def;
    for (i = 0; i < 3u; i++)
        a->key[i] = param_desc_of(eng, AK_KEYID[i])->def;
    for (i = 0; i < 5u; i++)
        a->seq[i] = param_desc_of(eng, AK_SEQID[i])->def;
    for (i = 0; i < np; i++, at += 2u) {
        if (c[at] >= P_COUNT || !(ak_grp(c[at]) & a->g) || c[at + 1u] > 191u)
            return 0;
        d = ak_slot(a, c[at]);
        *d = (int16_t)param_fit(param_desc_of(eng, c[at]), (int16_t)c[at + 1u] - 64);
    }
seq:
    if (a->g & AK_SEQ) {
        if (at + 2u > n)
            return 0;
        map = (uint32_t)c[at] | (uint32_t)c[at + 1u] << 8;
        at += 2u;
        for (i = 0; i < AK_NS; i++) {
            if (map >> i & 1u) {
                if (at + PS_STEP_N > n || !ps_unpack_step(c + at, &a->st[i]))
                    return 0;
                at += PS_STEP_N;
            } else
                step_clear(&a->st[i]);
        }
        if (a->seq[0] > 16)
            a->seq[0] = 16;
    }
    return at == n;
}
/* the part's AK block -> o (AK_MAX bytes): u16 length + content; the length of all. Always a block (DUELL 1.0): the arp and keys groups, SEQ with the SEQ arp */
static uint32_t ak_build(const track_t *t, uint8_t *o)
{
    uint32_t eng = t->eng_req, g = AK_ARP | AK_KEYS, i, at = 4u, np = 0, map = 0;
    int32_t v;
    if (motion_base_value(t, P_AMODE) == AM_SEQ)
        g |= AK_SEQ;
    {   /* the values of the present groups that differ from the defaults, in ordinal order (ak_id) */
        uint8_t oid[16], ov[16];
        uint32_t bm = 0, j;
        for (j = 0; j < 16u; j++) {
            i = ak_id(j);
            if (!(ak_grp(i) & g) || i == AK_NOSCALE)
                continue;
            v = motion_base_value(t, i);
            if (i == P_SLEN && v > 16)
                v = 16;
            if (v != param_desc_of(eng, i)->def) {
                oid[np] = (uint8_t)i;
                ov[np++] = (uint8_t)(v + 64);
                bm |= 1u << j;
            }
        }
        if (np > 2u) {                           /* the bitmap form is shorter from three values on */
            g |= AK_BM;
            o[3] = (uint8_t)bm;
            o[4] = (uint8_t)(bm >> 8);
            at = 5u;
            for (j = 0; j < np; j++)
                o[at++] = ov[j];
        } else {
            o[3] = (uint8_t)np;
            for (j = 0; j < np; j++) {
                o[at++] = oid[j];
                o[at++] = ov[j];
            }
        }
        o[2] = (uint8_t)g;
    }
    if (g & AK_SEQ) {
        uint32_t m = at;
        at += 2u;
        for (i = 0; i < AK_NS; i++)
            if (!ak_step_empty(&t->step[i])) {
                map |= 1u << i;
                ps_pack_step(&t->step[i], o + at);
                at += PS_STEP_N;
            }
        o[m] = (uint8_t)map;
        o[m + 1u] = (uint8_t)(map >> 8);
    }
    o[0] = (uint8_t)(at - 2u);
    o[1] = (uint8_t)((at - 2u) >> 8);
    return at;
}
/* slot k's AK block: the raw one (u16 length first; 0: none), and decoded */
static const uint8_t *pt_ak_raw(uint32_t k)
{
    const uint8_t *p = pt_raw(k);
    uint32_t xbo, mb, pr, pat, fm, ak;
    if (!p || !(p[4] & PTF_AK))
        return 0;
    pt_layout_x(p, &xbo, &mb, &pr, &pat, &fm, &ak);
    return p + ak;
}
static int ak_get(uint32_t k, ak_t *a)
{
    const uint8_t *b = pt_ak_raw(k);
    return b && ak_dec(b + 2, pt_rd16(b), pt_raw(k)[5], a);
}

/* record k decoded (the ring of four: a pointer stays good through three more calls); an unused slot: all zero */
static up_rec_t up_ring[4] __attribute__((section(".pool")));
static uint32_t up_ring_n;
static up_rec_t *up_rec(uint32_t k)
{
    up_rec_t *r = &up_ring[up_ring_n++ & 3u];
    const uint8_t *p = pt_raw(k);
    uint32_t xbo, mb, pr, pat, fm, i;
    memset(r, 0, sizeof *r);
    if (!p)
        return r;
    pt_layout(p, &xbo, &mb, &pr, &pat, &fm);
    r->used = UP_USED;
    r->ver = UP_VER;
    r->engine = p[5];
    r->np = P_COUNT;
    memcpy(r->name, p + 7, 12);
    for (i = 0; i < P_COUNT; i++)
        up_set_value(r, i, (int16_t)up_def(r->engine, i));
    if (p[4] & PTF_BM) {                             /* the bitmap form: a value per set bit, in id order */
        const uint8_t *bv = p + pr + PT_BMB;
        uint32_t j = 0;
        for (i = 0; i < P_COUNT; i++)
            if (p[pr + i / 8u] >> (i % 8u) & 1u)
                up_set_value(r, i, (int16_t)bv[j++] - 64);
    } else {
        for (i = 0; i < p[19]; i++)
            if (p[pr + 2u * i] < P_COUNT)
                up_set_value(r, p[pr + 2u * i], (int16_t)p[pr + 2u * i + 1u] - 64);
    }
    if (!(p[4] & PTF_TR)) {                          /* 0.2.19: a record of before the split: its LEVEL was the sound's loudness, the trim (and the pair of its TRIM's id was the
                                                      * legacy DUCK HOLD): the trim it is; the mixer level is no record's */
        up_set_value(r, P_TRIM, up_value(r, P_LEVEL));
        up_set_value(r, P_LEVEL, (int16_t)up_def(r->engine, P_LEVEL));
    }
    r->packed[UP_CAT_AT] = p[6];
    if (p[4] & PTF_XB) {                             /* the extension block: the slots it names (the others stay at their default: xz 0), the hand-set B edits */
        uint8_t xv[NX];
        xb_defaults(xv);
        r->xb_n = (uint8_t)xb_block_read(p + xbo, xv, r->xb);
        for (i = 0; i < NX; i++)
            r->xz[i] = (uint8_t)(xv[i] ^ xb_def(i));
    }
    if (p[4] & PTF_MB)
        memcpy(r->packed + UP_MB_AT, p + mb, UP_MB_N);
    if (p[4] & PTF_PAT) {
        memcpy(r->note, p + pat, 16);
        memcpy(r->flags, p + pat + 16u, 16);
    }
    if (p[4] & PTF_AK) {                             /* the arp / keys / sequencer: their values back among the record's */
        ak_t a;
        if (ak_get(k, &a)) {
            for (i = 0; i < 8u; i++)
                if (a.g & AK_ARP)
                    up_set_value(r, P_AMODE + i, a.arp[i]);
            for (i = 1; i < 3u; i++)                 /* (key[0], the retired SCALE: discarded) */
                if (a.g & AK_KEYS)
                    up_set_value(r, AK_KEYID[i], a.key[i]);
            if (a.g & AK_SEQ) {
                for (i = 0; i < 5u; i++)
                    up_set_value(r, AK_SEQID[i], a.seq[i]);
                up_pat_from(r, a.st);
            }
        }
    }
    return r;
}
/* r (+ the FM6 patch pk, FM6_PACKED bytes, 0 none) -> pt_enc as slot k's record, revision rev; its length */
static uint32_t pt_encode(uint32_t k, uint32_t rev, const up_rec_t *r, const uint8_t *pk)
{
    uint8_t *o = pt_enc, n = 0, f = 0, did[P_COUNT], dv[P_COUNT], xv[NX];
    uint32_t i, at, mb = 0, gm, xn;
    for (i = 0; i < NX; i++)                         /* the extension block first, built in place after the fixed part (the memset below clears only the fixed part) */
        xv[i] = (uint8_t)(r->xz[i] ^ xb_def(i));
    xn = xb_block_build(xv, r->xb, r->xb_n <= XB_MAX ? r->xb_n : XB_MAX, o + PT_FIX);
    if (xn)
        f |= PTF_XB;
    if (r->np <= UP_MB_AT)
        for (i = 0; i < UP_MB_N; i++)
            mb |= r->packed[UP_MB_AT + i];
    if (up_ak_in && pt_rd16(up_ak_in) && up_ak_in[2])
        f |= PTF_AK;
    else
        up_ak_in = 0;
    gm = f & PTF_AK ? up_ak_in[2] & 7u : 0u;
    if (!up_pat_empty(r) && !(gm & AK_SEQ))          /* (the AK block's steps are the pattern) */
        f |= PTF_PAT;
    if (pk && r->engine == ENGI_FM6)
        f |= PTF_FM6;
    if (mb)
        f |= PTF_MB;
    for (i = 0; i < P_COUNT; i++) {                  /* the values off their defaults (not the AK block's: it carries them) */
        int32_t v = i < r->np && i != P_LEVEL ? up_value(r, i) : up_def(r->engine, i);   /* (0.2.19: the MIXER LEVEL is not a patch's: never written) */
        if (v != up_def(r->engine, i) && !(ak_grp(i) & gm)) {
            did[n] = (uint8_t)i;
            dv[n++] = (uint8_t)(v + 64);
        }
    }
    if (n > PT_BMB)                                  /* a bitmap and a byte each is shorter than a pair each from 17 on */
        f |= PTF_BM;
    memset(o, 0, PT_FIX);
    o[2] = (uint8_t)k;
    o[3] = (uint8_t)rev;
    o[4] = f | PTF_CB | PTF_TR;
    o[5] = r->engine;
    o[6] = r->np <= UP_CAT_AT ? r->packed[UP_CAT_AT] : 0u;
    memcpy(o + 7, r->name, 12);
    at = PT_FIX + xn;
    if (f & PTF_MB) {
        memcpy(o + at, r->packed + UP_MB_AT, UP_MB_N);
        at += UP_MB_N;
    }
    if (f & PTF_BM) {
        memset(o + at, 0, PT_BMB);
        for (i = 0; i < n; i++)
            o[at + did[i] / 8u] |= (uint8_t)(1u << (did[i] % 8u));
        at += PT_BMB;
        for (i = 0; i < n; i++)
            o[at++] = dv[i];
    } else {
        for (i = 0; i < n; i++) {
            o[at++] = did[i];
            o[at++] = dv[i];
        }
    }
    o[19] = n;
    if (f & PTF_PAT) {
        memcpy(o + at, r->note, 16);
        memcpy(o + at + 16u, r->flags, 16);
        at += 32u;
    }
    if (f & PTF_FM6) {
        upf_pk_put(pk, o + at);
        at += UPF_PK;
    }
    if (f & PTF_AK) {
        uint32_t n2 = 2u + pt_rd16(up_ak_in);
        memcpy(o + at, up_ak_in, n2);
        at += n2;
    }
    o[0] = (uint8_t)at;
    o[1] = (uint8_t)(at >> 8);
    return at;
}

static int pt_idle(void)                         /* what a write that changes nothing answers */
{
#if FELUCCA_FLASH
    return flash_ok ? 0 : 3;
#else
    return 3;
#endif
}
static int pt_room(uint32_t c, uint32_t n) { return (pt_len[c] ? pt_len[c] : PT_HDR) + n <= PT_CAP; }
static uint32_t pt_cnt(uint32_t c) { return pt_n[c]; }   /* the records in chunk c */
/* chunk d takes a NEW record of n bytes: under PT_K records (so that PT_K worst-case records always fit it) and with the room */
static int pt_takes(uint32_t d, uint32_t n) { return pt_cnt(d) < PT_K && pt_room(d, n); }
/* chunk c rebuilt in pt_work: its live records (the directory's, in file order) but slot `skip`'s (UP_SLOTS: none), then pt_enc (n bytes,
 * 0: none) as the newest. A record the directory does not point to (an older revision, a damaged one) is not carried over. The new length,
 * -1 when the chunk cannot be read */
static int pt_build(uint32_t c, uint32_t skip, uint32_t n)
{
    uint32_t off, w = PT_HDR;
    int len = pt_n[c] ? pt_ld(c, pt_work) : 0;
    if (len < 0 || (len > 0 && len < (int)PT_HDR))
        return -1;
    for (off = PT_HDR; (int)(off + PT_FIX) <= len;) {
        const uint8_t *p = pt_work + off;
        uint32_t rl = pt_rd16(p), i;
        if (rl < PT_FIX || off + rl > (uint32_t)len)
            break;
        if (p[2] < UP_SLOTS && p[2] != skip && pt_dir[p[2]].ch == c && pt_dir[p[2]].off == off) {
            for (i = 0; i < rl; i++)                 /* (down, forwards: the firmware's libc has no memmove) */
                pt_work[w + i] = pt_work[off + i];
            w += rl;
        }
        off += rl;
    }
    pt_work[0] = (uint8_t)PT_MAGIC;
    pt_work[1] = (uint8_t)(PT_MAGIC >> 8);
    pt_work[2] = PT_VER;
    pt_work[3] = 0;
    if (w + n > PT_CAP)
        return -1;
    memcpy(pt_work + w, pt_enc, n);
    return (int)(w + n);
}
/* chunk c := its live records but slot `skip`'s, plus pt_enc (n bytes) -> flash, and the directory follows. 0 ok, 2 flash error (nothing
 * changed), 3 no flash (kept in RAM) */
static int pt_write(uint32_t c, uint32_t skip, uint32_t n)
{
    int w = pt_build(c, skip, n), rc;
    if (w < 0)
        return 2;
    if ((rc = pt_sv(c, pt_work, (uint32_t)w)) == 2) {
        pt_scan_all();                           /* the chunks as flash holds them */
        return 2;
    }
    pt_dir_chunk_out(c);
    pt_scan_img(c, pt_work, (uint32_t)w);
    pt_tally();
    return rc;
}
/* slot k = r (+ pk), or its record removed (r 0: RESTORE, the factory sound is back), in the chunks and in flash: 0 ok, 2 flash error
 * (nothing changed), 3 no flash (kept in RAM), PT_FULL no room (a guard only) */
static uint32_t pt_next_rev(uint32_t k) { return pt_dir[k].ch != PT_NOCH ? (uint32_t)pt_dir[k].rev + 1u : 0u; }
/* slot k := the record already in pt_enc (n bytes, its slot and revision bytes set): the part of pt_put that places and writes it */
static int pt_put_enc(uint32_t k, uint32_t n)
{
    uint32_t oc = PT_NCH, orl = 0, d;
    int rc;
    if (pt_dir[k].ch != PT_NOCH) {
        oc = pt_dir[k].ch;
        orl = pt_dir[k].len;
    }
    if (oc != PT_NCH && pt_len[oc] - orl + n <= PT_CAP)   /* in its own chunk */
        return pt_write(oc, k, n);
    for (d = 0; d < PT_NCH && (d == oc || !pt_takes(d, n)); d++)
        ;
    if (d == PT_NCH)
        return PT_FULL;
    rc = pt_write(d, UP_SLOTS, n);               /* the new record first (the newer revision wins until the old is cut) */
    if (rc == 2 || oc == PT_NCH)
        return rc;
    pt_write(oc, k, 0);                          /* (an error: the old one stays; the newer revision still wins) */
    return rc;
}
static int pt_put(uint32_t k, const up_rec_t *r, const uint8_t *pk)
{
    if (!r) {                                    /* erase */
        if (pt_dir[k].ch == PT_NOCH)
            return pt_idle();
        return pt_write(pt_dir[k].ch, k, 0);
    }
    return pt_put_enc(k, pt_encode(k, pt_next_rev(k), r, pk));
}
/* 0.2.19 (the editor's PATCH_RAW, editor_patchraw.c): slot k := the stored record rec (n bytes, exactly the form pt_encode writes; a record read
 * by up_raw_get). Refused (1) when it is no well-formed record of today's form (pt_rec_ok, a printable name, an engine that exists); 2 the transport
 * runs or the flash fails (nothing changed), 3 no flash (kept in RAM). The slot and revision bytes are the unit's: the record's own are ignored. */
static int up_put_raw(uint32_t k, const uint8_t *rec, uint32_t n)
{
    uint8_t *o = pt_enc;
    uint32_t i, xbo, mb, pr, pat, fm;
    int rc;
    if (k >= UP_SLOTS || n < PT_FIX || n > PT_WORST || n > sizeof pt_enc)
        return 1;
    memcpy(o, rec, n);
    o[2] = (uint8_t)k;
    o[3] = (uint8_t)pt_next_rev(k);
    if (!pt_rec_ok(o, n))
        return 1;
    for (i = 0; i < 12u && o[7u + i]; i++)
        ;
    if (!up_name_ok(o + 7, i))
        return 1;
    for (; i < 12u; i++)                          /* (the name is 0-padded: nothing but 0 after its end) */
        if (o[7u + i])
            return 1;
    pt_layout(o, &xbo, &mb, &pr, &pat, &fm);      /* every stored value inside what up_rec reads (a byte, +64) */
    if (o[4] & PTF_BM) {
        for (i = 0; i < o[19]; i++)
            if (o[pr + PT_BMB + i] > 191u)
                return 1;
    } else {
        for (i = 0; i < o[19]; i++)
            if (o[pr + 2u * i] >= P_COUNT || o[pr + 2u * i + 1u] > 191u)
                return 1;
    }
    if (transport_busy()) {
        ui_message("STOP TO SAVE");
        return 2;
    }
    rc = pt_put_enc(k, n);
    if (rc == 2 || rc == PT_FULL)
        return rc == PT_FULL ? 2 : rc;
    up_gen++;
    return rc;
}
/* slot k's stored record without its position: the bytes of the saved record with the slot and revision bytes (2, 3) zero, so the same sound is
 * the same bytes in any slot. Returns its length (0: the slot holds its factory sound, nothing saved); out holds at least PT_WORST bytes */
static uint32_t up_raw_get(uint32_t k, uint8_t *out)
{
    const uint8_t *p = pt_raw(k);
    uint32_t n;
    if (!p)
        return 0;
    n = pt_rd16(p);
    if (n < PT_FIX || n > PT_WORST)
        return 0;
    memcpy(out, p, n);
    out[2] = 0;
    out[3] = 0;
    return n;
}

/* RESTORE ALL, one chunk: it holds no record any more (a header-only image, the standard commit-last write). 0 ok, 2 flash error, 3 no flash */
static int pt_wipe(uint32_t c)
{
    int rc;
    if (!pt_n[c])
        return pt_idle();
    pt_work[0] = (uint8_t)PT_MAGIC;
    pt_work[1] = (uint8_t)(PT_MAGIC >> 8);
    pt_work[2] = PT_VER;
    pt_work[3] = 0;
    if ((rc = pt_sv(c, pt_work, PT_HDR)) == 2) {
        pt_scan_all();
        return 2;
    }
    pt_dir_chunk_out(c);
    pt_tally();
    return rc;
}

/* record k's fields without decoding it (the directory: no flash read) */
static uint32_t up_engine_raw(uint32_t k) { return up_used(k) ? pt_dir[k].eng : 0u; }
static void up_name(uint32_t k, char *b)       /* upper case, 0-terminated: b holds 13 */
{
    uint32_t i;
    for (i = 0; up_used(k) && i < 12u && pt_dir[k].name[i]; i++)
        b[i] = pt_dir[k].name[i] >= 'a' && pt_dir[k].name[i] <= 'z' ? (char)(pt_dir[k].name[i] - 32) : pt_dir[k].name[i];
    b[i] = 0;
}
static uint32_t up_cat(uint32_t k)             /* a used slot's category (an unused slot: OTHER) */
{
    uint32_t c = up_used(k) ? pt_dir[k].cat : 0u;
    return c && c < CAT_N ? c : cat_of_engine(up_used(k) ? pt_dir[k].eng : 0u);
}

static int up_put(uint32_t k, const up_rec_t *r);
#include "up_fm6.c"                            /* an FM6 sound's patch in its record */
#include "fm6_vbank.c"                         /* FM6's voice bank (SLOT B1..B32, 1.4.1) */
#include "patslot.c"                           /* DUELL: the pattern slots P1..P16 (SAVE > PATTERNS) */
#include "wtab.c"                              /* DUELL 0.2.14: the user wavetables USR1..USR3 (eng_wave.c reads them) */

static void pt_boot(void)                      /* the directory from the chunks in flash (a power-on's RAM: all gone before) */
{
    uint32_t c;
    for (c = 0; c < PT_NCH; c++)
        pt_cur[c] = -1;
    for (c = 0; c < 4u; c++)                     /* (.pool is not cleared at start: no read record yet) */
        pt_rver[c] = pt_rslot[c] = 0;
    pt_rn = 0;
    pt_rgen = 0;
    pt_scan_all();                               /* (pt_tally: pt_rgen 1) */
    up_gen++;
}
static void up_boot(void)                      /* persist_boot: the chunks from flash */
{
    pt_boot();
    fvb_boot();                                  /* FM6's voice bank: a staged one finished, its voices */
    ps_boot();                                   /* DUELL: the pattern slots */
    wt_boot();                                   /* DUELL 0.2.14: the user wavetables */
    up_gen++;                                    /* (the favourites are the slots': a slot with no record is a factory sound, FAV stays) */
}

/* RAM only (a test's power-on): no patch stored */
static void up_ram_reset(void)
{
    uint32_t k;
    for (k = 0; k < UP_SLOTS; k++)
        pt_dir[k].ch = PT_NOCH;
#if !FELUCCA_FLASH
    memset(pt_ramlen, 0, sizeof pt_ramlen);
#endif
    pt_tally();
    up_gen++;
}

/* record k = *r (0: its record removed, RESTORE) in the patch store and flash: 0 ok, 1 bad slot, 2 flash error or the transport runs (nothing
 * written), 3 no flash (kept in RAM), 4 no chunk has room (a guard: PT_MAX = 128 records always fit, only a flash fault can make a
 * write fail; nothing written). up_pk_in: the FM6 patch it carries. What the parts that play the slot do about it: sl_restore */
static int up_put(uint32_t k, const up_rec_t *r)
{
    int rc;
    if (k >= UP_SLOTS)
        return 1;
    if (transport_busy()) {                            /* no flash erase while playing (project_save) */
        ui_message("STOP TO SAVE");
        return 2;
    }
    rc = pt_put(k, r, up_pk_in);
    if (rc == 2 || rc == PT_FULL)
        return rc;
    up_gen++;
    return rc;
}

static void up_slot_label(char *b, uint32_t k)  /* "#7", "#128" (b holds 5): the number of the slot, 1..128 */
{
    uint32_t n = k + 1u, i = 0;
    b[i++] = '#';
    if (n >= 100u)
        b[i++] = (char)('0' + n / 100u % 10u);
    if (n >= 10u)
        b[i++] = (char)('0' + n / 10u % 10u);
    b[i++] = (char)('0' + n % 10u);
    b[i] = 0;
}

/* the automatic name of engine e's sound in slot k: engine name + slot number ("ANALOG 7"); b holds 13 */
static void up_auto_name(char *b, uint32_t e, uint32_t k)
{
    char l[6];
    e %= NENGINES;
    str_cpy(b, ENGINES[eng_safe(e)]->name, 9);   /* (a DIGITAL record plays as FM6) */
    up_slot_label(l, k);
    str_cpy(b + str_len(b), " ", 2);
    str_cpy(b + str_len(b), l + 1, 4);
}

/* the record's name = name (at most 12), 0 or "": the automatic one */
static void up_set_name(up_rec_t *r, uint32_t k, const char *name)
{
    char b[16];
    uint32_t i;
    if (!name || !name[0]) {
        up_auto_name(b, r->engine, k);
        name = b;
    }
    memset(r->name, 0, sizeof r->name);
    for (i = 0; i < 12u && name[i]; i++)
        r->name[i] = name[i];
}

static uint32_t track_cat(const track_t *t);     /* (ui.c: the category of the sound a track plays) */

/* the selected part's sound -> slot k; name 0 or "": the automatic name (up_auto_name); cat its category (CAT_*), 0:
 * the one of the sound it came from (track_cat); up_put's result */
static const uint8_t *up_mblk;                   /* DUELL: the morph block up_store_trk writes into the record (0: none) */
static const xb_edit_t *up_xb_in;                /* the hand-set B edit list (up_xb.h) up_store_trk writes into the record, up_xb_nin of them (0: none); set by job G, the tests */
static uint32_t up_xb_nin;
static int up_store_trk(track_t *t, uint32_t k, const char *name, uint32_t cat)
{
    up_rec_t r;
    uint32_t i;
    memset(&r, 0, sizeof r);
    r.used = UP_USED;
    r.ver = UP_VER;
    r.engine = t->eng_req;
    r.np = P_COUNT;
    up_set_name(&r, k, name);
    for (i = 0; i < P_COUNT; i++)
        up_set_value(&r, i, motion_base_value(t, i));
    up_pat_from(&r, t->step);
    up_set_cat(&r, cat ? cat : track_cat(t));
    for (i = 0; i < NX; i++)                            /* the extension slots (ext.h XV), through the accessor only */
        up_set_xval(&r, i, XV(t, i));
    if (up_xb_in && up_xb_nin) {                        /* the hand-set B edits of the part (job G: it sets up_xb_in / up_xb_nin before the save, as up_mblk) */
        r.xb_n = (uint8_t)(up_xb_nin < XB_MAX ? up_xb_nin : XB_MAX);
        memcpy(r.xb, up_xb_in, r.xb_n * sizeof r.xb[0]);
    }
    if (up_mblk)
        memcpy(r.packed + UP_MB_AT, up_mblk, UP_MB_N);
    {
        uint8_t pk[FM6_PACKED], akb[AK_MAX];
        int rc;
        /* the part's arp, keys and SEQ pattern (ak_*): written for every patch, the arp OFF as well, so a load brings each patch's own back */
        up_ak_in = ak_build(t, akb) ? akb : 0;      /* (DUELL 1.0: always a block, the arp OFF too) */
        if (r.engine == ENGI_FM6) {                     /* an FM6 sound carries its track's patch in the record */
            fm6_pack(fm6_patch[(uint32_t)(t - trk) % NTRK], pk);
            up_pk_in = pk;
        }
        rc = up_put(k, &r);
        up_pk_in = 0;
        up_ak_in = 0;
        return rc;
    }
}
static int up_store_cat(uint32_t k, const char *name, uint32_t cat) { return up_store_trk(TSEL, k, name, cat); }

static int up_store(uint32_t k, const char *name) { return up_store_cat(k, name, 0); }

/* slot k renamed (name 0 or "": the automatic one) and given category cat (0: kept), the sound and its pattern as
 * they are; up_put's result, 1 for an empty slot */
static int up_rename_cat(uint32_t k, const char *name, uint32_t cat)
{
    up_rec_t r;
    uint8_t pk[FM6_PACKED];
    int rc;
    if (!up_used(k))
        return 1;
    r = *up_rec(k);
    up_set_name(&r, k, name);
    up_set_cat(&r, cat);
    if (!upf_get(k, pk))                                /* (an FM6 sound keeps its patch) */
        up_pk_in = pk;
    up_ak_in = pt_ak_raw(k);                            /* (and its arp / keys / pattern block) */
    rc = up_put(k, &r);
    up_pk_in = 0;
    up_ak_in = 0;
    return rc;
}
static int up_rename(uint32_t k, const char *name) { return up_rename_cat(k, name, 0); }

/* A patch's SEQ pattern (AK_SEQ) is lent like the arp (ui.c ds_arp): the part's steps 1..16 and LEN DIV SWING GATE QUANTIZE go
 * aside, the patch's take their place; the next load of any sound gives them back unless they were edited since (then the
 * edit stays: the player's), and a patch with a pattern of its own replaces the lent one (the player's still aside) */
static uint8_t ds_slent[NSHOWN];
static step_t ds_st_was[NSHOWN][AK_NS];
static int16_t ds_sq_was[NSHOWN][5];
static uint32_t ds_sq_sig[NSHOWN];
static uint32_t ds_sq_sig_of(const track_t *t)
{
    int16_t q[5];
    uint32_t i;
    for (i = 0; i < 5u; i++)
        q[i] = t->p[AK_SEQID[i]];
    return fnv(fnv(2166136261u, t->step, AK_NS * (uint32_t)sizeof(step_t)), q, sizeof q);
}
static void ds_seq_back(track_t *t)
{
    uint32_t k = trk_index(t) % NTRK, i;
    if (k >= NSHOWN || !ds_slent[k])
        return;
    if (ds_sq_sig_of(t) == ds_sq_sig[k]) {
        memcpy(t->step, ds_st_was[k], sizeof ds_st_was[k]);
        for (i = 0; i < 5u; i++)
            t->p[AK_SEQID[i]] = ds_sq_was[k][i];
    }
    ds_slent[k] = 0;
}
static void ds_seq_lend(track_t *t, const ak_t *a)
{
    uint32_t k = trk_index(t) % NTRK, i;
    if (k >= NSHOWN)
        return;
    memcpy(ds_st_was[k], t->step, sizeof ds_st_was[k]);
    for (i = 0; i < 5u; i++) {
        ds_sq_was[k][i] = t->p[AK_SEQID[i]];
        t->p[AK_SEQID[i]] = a->seq[i];
    }
    memcpy(t->step, a->st, sizeof a->st);
    ds_sq_sig[k] = ds_sq_sig_of(t);
    ds_slent[k] = 1;
}

/* record k -> the selected part's sound: engine and every parameter except the track's own (param_kept:
 * the mix, ARP, SCL, the pattern parameters). The steps stay, but for the patch's SEQ pattern (its AK block, lent: above); the
 * record's older pattern is loaded only from SAVE > PATTERNS (up_pat_load). 0 ok, 1 no record (sl_load loads a slot: its record, else
 * its factory sound) */
static int up_load(uint32_t k)
{
    const up_rec_t *r;
    int16_t v[P_COUNT];
    uint32_t i;
    track_t *t = TSEL;
    if (!up_used(k))
        return 1;
    r = up_rec(k);
    up_values(r, v);
    load_begin(t, UNDO_SOUND);                          /* (ui.c: the copy for SAVE held = undo) */
    panic_req |= (uint8_t)(1u << song.sel);
    {
        fm1_irq_off();                                  /* the audio ISR must not see half a sound */
        t->eng_req = r->engine;
        for (i = 0; i < P_COUNT; i++)
            if (!param_kept_on(t, i))                   /* (0.2.19: P_TRIM comes with the patch, as a factory sound's does; the MIXER LEVEL stays, with the rest of the mix) */
                t->p[i] = v[i];
        for (i = 0; i < NX; i++)                        /* the extension slots: the record's, the defaults for what it does not name (up_rec) */
            XV(t, i) = up_xval(r, i);
        duck_set_part(trk_index(t));                    /* (DUCK SRC PART on one part only: the other part's ducker goes OFF, params.c) */
        t->preset = 0;
        fm1_irq_on();
        upf_track_load(t, k);                           /* FM6: the preset's own patch (up_fm6.c) */
    }
    {   /* the patch's own arp / keys / SEQ pattern (its AK block) is lent as the factory sounds' are: the part's own go aside and
         * come back with the next load (ui.c ds_arp / ds_keys, ds_seq_back below). DUELL 1.0: every patch sets them, a patch without the block
         * (an older one, or the editor's UP_PUT copy) as the defaults: arp OFF, no chord keys */
        ak_t a;
        int has = ak_get(k, &a);
        int16_t darp[8], dkey[2];
        for (i = 0; i < 8u; i++)
            darp[i] = TP[P_AMODE + i].def;
        dkey[0] = TP[P_CHRD].def;
        dkey[1] = TP[P_VOIC].def;
        ds_arp(t, 0, has && (a.g & AK_ARP) ? a.arp : darp);        /* (a lent arp back: ui.c) */
        ds_keys(t, 0, has && (a.g & AK_KEYS) ? a.key + 1 : dkey);  /* (and the chord keys: ui.c; an old record's SCALE, key[0], is dropped) */
        ds_seq_back(t);
        if (has && (a.g & AK_SEQ))
            ds_seq_lend(t, &a);
    }
    t->user = (uint8_t)(k + 1u);
    morph_ld = r->np <= UP_MB_AT ? r->packed + UP_MB_AT : 0;   /* DUELL: End B saved with it (morph.c) */
    morph_ld_e = r->xb_n ? r->xb : 0;                   /* .. and the values set by hand on it */
    morph_ld_n = r->xb_n;
    load_end(t);
    morph_ld = 0;
    morph_ld_e = 0;
    morph_ld_n = 0;
    sync_reload = 1;
    ui.force = 1;
    return 0;
}

/* the patterns of the user presets (no longer listed by SAVE > PATTERNS: its list is the sixteen slots; kept for tests) */
static int up_has_pat(uint32_t k) { return up_used(k) && !up_pat_empty(up_rec(k)); }

__attribute__((unused)) static uint32_t up_pat_count(void)
{
    uint32_t k, n = 0;
    for (k = 0; k < UP_SLOTS; k++)
        n += (uint32_t)up_has_pat(k);
    return n;
}

__attribute__((unused)) static uint32_t up_pat_nth(uint32_t n)         /* slot of the n-th one that holds a pattern (n < up_pat_count()) */
{
    uint32_t k;
    for (k = 0; k < UP_SLOTS; k++)
        if (up_has_pat(k) && !n--)
            return k;
    return 0;
}

__attribute__((unused)) static uint32_t up_pat_rank(uint32_t slot)     /* ones that hold a pattern before it */
{
    uint32_t k, n = 0;
    for (k = 0; k < slot && k < UP_SLOTS; k++)
        n += (uint32_t)up_has_pat(k);
    return n;
}

/* slot k's pattern -> track t's steps 1..16 (the rest cleared), with the record's LEN (at most 16), DIV,
 * SWING and GATE; the sound stays (ui.c pat_load: the undo copy) */
__attribute__((unused)) static void up_pat_load(track_t *t, uint32_t k)
{
    const up_rec_t *r;
    int16_t v[P_COUNT];
    uint32_t i;
    if (!up_has_pat(k))
        return;
    r = up_rec(k);
    up_values(r, v);
    load_pat16(t, r->note, r->flags);
    for (i = P_SDIV; i <= P_SGATE; i++)
        t->p[i] = v[i];
    t->p[P_SLEN] = (int16_t)clamp(v[P_SLEN], 1, 16);   /* (the pattern has 16 steps) */
}

/* the engine a record's sound plays on (a retired engine's record: itself, the silent placeholder) */
static uint32_t up_engine(uint32_t k) { return eng_safe(up_engine_raw(k)); }

__attribute__((unused)) static uint32_t up_count(void)                 /* slots that hold a SAVED record */
{
    uint32_t k, n = 0;
    for (k = 0; k < UP_SLOTS; k++)
        n += (uint32_t)up_used(k);
    return n;
}

/* ================================================================ the slot layer ===
 * ONE list of UP_SLOTS (128) patch slots. Slot k is its saved record if there is one (up_used), else the factory sound at browse position k
 * (sounds.c ds_list_at: category, engine, name = the #NN numbers, #NN = k + 1). Nothing is written for the factory ones (a virtual
 * fallback). With fewer than 128 factory sounds the slots past them are INIT sounds (never in the product: a test checks the 128). */
static uint8_t sl_te[UP_SLOTS], sl_tk[UP_SLOTS], sl_tn = 0xFFu;   /* the browse list as a table (engine, factory index per position), built once: the factory
                                                                  * table is const, and the lists ask for a slot's sound in loops (plain .bss: the pool is not cleared) */
static void sl_tab(void)
{
    uint32_t i, n = ds_list_n(), k;
    if (sl_tn != 0xFFu)
        return;
    for (i = 0; i < n && i < UP_SLOTS; i++) {
        sl_te[i] = (uint8_t)ds_list_at(i, &k);
        sl_tk[i] = (uint8_t)k;
    }
    sl_tn = (uint8_t)(n < UP_SLOTS ? n : UP_SLOTS);
}
static int sl_fac(uint32_t k, uint32_t *e, uint32_t *fk)   /* 1: slot k has a factory sound (engine *e, its factory index *fk) */
{
    sl_tab();
    if (k >= sl_tn)
        return 0;
    *e = sl_te[k];
    *fk = sl_tk[k];
    return 1;
}
static uint32_t sl_engine(uint32_t k)            /* the engine slot k's sound plays on */
{
    uint32_t e, fk;
    if (up_used(k))
        return up_engine(k);
    return sl_fac(k, &e, &fk) ? eng_safe(e) : 0u;
}
static void sl_name(uint32_t k, char *b)         /* upper case, 0-terminated: b holds 13 */
{
    uint32_t e, fk;
    if (up_used(k) || !sl_fac(k, &e, &fk)) {
        up_name(k, b);
        if (!b[0])
            str_cpy(b, "INIT", 13);
        return;
    }
    str_cpy(b, ds_name(e, fk), 13);
}
static uint32_t sl_cat(uint32_t k)               /* its category (category.c CAT_*) */
{
    uint32_t e, fk;
    if (up_used(k))
        return up_cat(k);
    return sl_fac(k, &e, &fk) ? preset_cat(e, fk) : CAT_OTHER;
}
/* slot k -> the selected part: its record, else its factory sound (the ENGINE changes as the PRESETS knob did, with its message) */
static void sl_load(uint32_t k)
{
    uint32_t e, fk;
    if (k >= UP_SLOTS)
        return;
    if (up_used(k) && !up_load(k))
        return;
    if (!sl_fac(k, &e, &fk)) {
        e = 0;
        fk = 0;
    }
    if (e != TSEL->eng_req)
        select_engine(e);
    apply_preset(fk);
}
/* slot k's values as a part would load them (the editor's UP_GET): the record's, or the factory sound's on INIT: the engine's EDIT defaults,
 * the sound's pairs, its level and its arp / chord keys. 0 ok */
static void sl_values(uint32_t k, int16_t *v)
{
    uint32_t e, fk, i;
    if (up_used(k)) {
        up_values(up_rec(k), v);
        return;
    }
    if (!sl_fac(k, &e, &fk)) {
        e = 0;
        fk = 0;
    }
    for (i = 0; i < P_COUNT; i++)
        v[i] = (int16_t)(i < P_E0 ? TP[i].def : ENGINES[e]->edit[i - P_E0].def);
    if (ds_get(e, fk)) {
        ds_put(ds_get(e, fk), v, 0);
        ds_put(ds_get(e, fk), v, 1);
        ds_put(ds_get(e, fk), v, 2);
    }
}

/* the parts that play slot k (t->user) load its factory sound again once the record is gone (the selected part's PATCHES, the editor's) */
static void sl_reload_parts(uint32_t mask)       /* bit i: part i played a slot whose record is gone */
{
    uint32_t i, was = song.sel;
    for (i = 0; i < NSHOWN; i++) {
        if (!((mask >> i) & 1u) || !trk[i].user || up_used((uint32_t)trk[i].user - 1u))
            continue;
        song.sel = (uint8_t)i;
        sl_load((uint32_t)trk[i].user - 1u);
    }
    song.sel = (uint8_t)was;
    sync_reload = 1;
    ui.force = 1;
}
/* RESTORE: slot k's record removed, its factory sound is back (the parts that played the record play the factory sound). 0 ok (also: it was
 * the factory sound already), 1 bad slot, 2 flash error / transport, 3 no flash (kept in RAM) */
static int sl_restore(uint32_t k)
{
    uint32_t i, m = 0;
    int rc;
    if (k >= UP_SLOTS)
        return 1;
    if (!up_used(k))
        return 0;                                    /* (already the factory sound) */
    for (i = 0; i < NSHOWN; i++)
        m |= (uint32_t)(trk[i].user == k + 1u) << i;
    rc = up_put(k, 0);
    if (rc == 0 || rc == 3)
        sl_reload_parts(m);
    return rc;
}
/* RESTORE ALL: every record removed, chunk by chunk (each chunk one atomic commit-last write: a power cut leaves some slots restored and the
 * others as they were, never a damaged one; do it again). Same results as sl_restore */
static int sl_restore_all(void)
{
    uint32_t c, i, m = 0;
    int rc = 0, r;
    if (transport_busy()) {
        ui_message("STOP TO SAVE");
        return 2;
    }
    for (i = 0; i < NSHOWN; i++)
        m |= (uint32_t)(trk[i].user && up_used((uint32_t)trk[i].user - 1u)) << i;
    for (c = 0; c < PT_NCH; c++) {
        r = pt_wipe(c);
        if (r == 2) {
            rc = 2;
            break;
        }
        if (r == 3)
            rc = 3;
    }
    up_gen++;
    sl_reload_parts(m);
    return rc;
}

/* The slot a part's sound is in: its t->user (a saved record loaded; or a restored slot) else the slot whose factory sound it plays and that
 * still holds that factory sound. UP_SLOTS: none (the sound came from no slot: an engine change, the editor, a multi; or its slot was overwritten) */
static uint32_t slot_of(const track_t *t)
{
    uint32_t e = t->eng_req % NENGINES, r;
    if (t->user)
        return t->user - 1u < UP_SLOTS ? (uint32_t)t->user - 1u : UP_SLOTS;
    r = ds_list_rank(e, t->preset);
    return r < UP_SLOTS && !up_used(r) ? r : UP_SLOTS;
}
/* where SAVE offers to write part t's sound: its slot; else the slot its factory sound belongs to (even if that holds a record now); else #1 */
static uint32_t slot_home(const track_t *t)
{
    uint32_t r;
    if (slot_of(t) < UP_SLOTS)
        return slot_of(t);
    r = ds_list_rank(t->eng_req % NENGINES, t->preset);
    return r < UP_SLOTS ? r : 0u;
}

/* ---- SAVE (hold SAVE, ui_input.c patch_save_ask) ---- */
_Static_assert(UP_MB_N == MB_N, "morph.c MB_N");
static void up_adopt(uint32_t pi, uint32_t k)    /* part pi's sound was written to slot k: it is that slot's now (the next save offers the same slot) */
{
    trk[pi].user = (uint8_t)(k + 1u);
    mph[pi].user = trk[pi].user;                        /* (the morph still belongs to this sound) */
}
/* part pi's sound -> slot k, overwriting what is there. Its name: the sound's own (a factory sound's, the slot it came from's); a sound that
 * has none (INIT) gets the engine's automatic name. Its category: the sound's. The record carries End B (morph block), the arp / keys /
 * SEQ pattern (AK block) and an FM6 sound's patch. 0 saved, else up_put's result */
static uint8_t both_names;                       /* 1 while both parts are saved at once: the dialog's RENAME does not apply */
static int up_patch_save_part(uint32_t pi, uint32_t k)
{
    track_t *t = &trk[pi];
    uint8_t blk[UP_MB_N];
    char nm[16];
    int rc;
    if (k >= UP_SLOTS)
        return 1;
    if (save_nm[0] && !both_names)                      /* 0.2.21: RENAME in the SAVE dialog (one part only) */
        str_cpy(nm, save_nm, sizeof nm);
    else if (t->user)                                   /* its slot's name; else the factory sound it plays */
        sl_name((uint32_t)t->user - 1u, nm);
    else
        str_cpy(nm, ds_name(t->eng_req % NENGINES, t->preset), 13);
    if (!nm[0] || str_eq(nm, "INIT"))
        up_auto_name(nm, t->eng_req, k);
    up_mblk = morph_blk_make(pi, blk) ? blk : 0;
    {   /* EDITABLE B: the values set by hand go into the record's extension block (after the re-roll above, which keeps them) */
        xb_edit_t ed[XB_MAX];
        up_xb_nin = morph_edits(pi, ed);
        up_xb_in = up_xb_nin ? ed : 0;
        rc = up_store_trk(t, k, nm, 0);
        up_xb_in = 0;
        up_xb_nin = 0;
    }
    up_mblk = 0;
    if (rc == 0 || rc == 3)
        up_adopt(pi, k);
    return rc == 3 ? 0 : rc;
}
/* SAVE held, OCT+: the selected part's sound over slot ui.svs, or with both (MORPH TARGET BOTH) each part over its own slot. A message says it */
static void patch_save_do(uint32_t both)
{
    uint32_t pi, first = both ? 0u : song.sel < NSHOWN ? song.sel : 0u, end = both ? NSHOWN : first + 1u, n = 0;
    char l[6];
    int rc = 0;
    if (song.playing || transport_req == 1u) {
        ui_message("STOP TO SAVE");
        return;
    }
    both_names = (uint8_t)both;
    for (pi = first; pi < end && !rc; pi++, n++)
        rc = up_patch_save_part(pi, both ? slot_home(&trk[pi]) : ui.svs % UP_SLOTS);
    both_names = 0;
    save_nm[0] = 0;
    up_slot_label(l, ui.svs % UP_SLOTS);
    if (rc)
        ui_message("SAVE ERROR");
    else if (n > 1u)
        ui_message("SAVED 1+2");
    else
        ui_say("SAVED ", l);
    up_gen++;
    ui.force = 1;
}

/* ---- RESTORE (PATCHES K4: a turn right asks; SAVE > TOOLS: RESTORE ALL) ---- */
static void patch_restore_ask(void)
{
    uint32_t k;
    pl_commit();                                        /* (it acts on a settled cursor) */
    k = pl_slot();
    if (k >= UP_SLOTS) {
        ui_message(list_mode() == 1u ? "NO FAVORITES" : "SELECT A PATCH");
        return;
    }
    if (!up_used(k)) {
        ui_message("ALREADY FACTORY");
        return;
    }
    if (transport_busy()) {
        ui_message("STOP TO SAVE");
        return;
    }
    confirm_open(CF_RESTORE, k);
}
static void patch_restore_do(uint32_t k)
{
    char l[6];
    int rc;
    up_slot_label(l, k);
    rc = sl_restore(k);
    if (rc == 0)
        ui_say("RESTORED ", l);
    else if (rc == 3)
        ui_message("RESTORED (RAM)");
    else if (rc != 2)
        ui_message("RESTORE ERROR");
    ui.force = 1;
}
static void patch_restore_all_do(void)
{
    int rc = sl_restore_all();
    if (rc == 0)
        ui_message("ALL RESTORED");
    else if (rc == 3)
        ui_message("RESTORED (RAM)");
    else if (rc != 2)
        ui_message("RESTORE ERROR");
    else if (!transport_busy())
        ui_message("RESTORE ERROR");
    ui.force = 1;
}

/* SAVE > USER-style actions with the message in the top bar (the NAME screen's ops, ui_name.c): 0 load, 1 restore, 2 save, 3 rename */
static void up_ui_named(uint32_t op, uint32_t k, const char *name, uint32_t cat)
{
    char l[6];
    int rc;
    up_slot_label(l, k);
    if (op == 3u && !up_used(k)) {
        ui_message("FACTORY SOUND");
        return;
    }
    if (op && (song.playing || transport_req == 1u)) {    /* (up_put refuses too) */
        ui_message("STOP TO SAVE");
        return;
    }
    if (op == 0u) {
        sl_load(k);
        ui_say("LOADED ", l);
        return;
    }
    rc = op == 1u ? sl_restore(k) : op == 3u ? up_rename_cat(k, name, cat) : up_store_cat(k, name, cat);
    if (rc == 3)
        ui_message(op == 1u ? "RESTORED (RAM)" : op == 3u ? "RENAMED (RAM)" : "SAVED (RAM)");
    else if (rc)
        ui_message(op == 1u ? "RESTORE ERROR" : "SAVE ERROR");
    else
        ui_say(op == 1u ? "RESTORED " : op == 3u ? "RENAMED " : "SAVED ", l);
    ui.force = 1;
}
static void up_ui(uint32_t op, uint32_t k) { up_ui_named(op, k, 0, 0); }
#endif
