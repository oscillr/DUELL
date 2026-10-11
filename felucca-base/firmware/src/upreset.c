/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* DUELL's PATCH store (SAVE > PATCHES, SPEC 3.6; editor protocol v2, cmds 16-21): at most PT_MAX stored patches (the number that
 * always fits, below), a patch being a factory sound's overwritten copy (a SHADOW, UP_SH_AT below) or a user patch of its own.
 * Stored COMPACTLY in six storage.c objects (OBJ_PATCH0..5, A/B sector pairs: 0xC8000..0xCDFFF USR3's old place, 0xDC000..0xDFFFF
 * Felucca's retired preset banks, 0x9F000 / 0xFE000 the retired FM6 bank's pair), one CHUNK each (at most ST_PAYLOAD_MAX
 * bytes), mirrored in RAM (pt_img) so browsing never reads flash.
 * The limit is HONEST: PT_WORST is the largest record there can be (an FM6 sound with its patch, a SEQ-mode arp with a full
 * 16-step pattern, End B, a shadow marker, every parameter off its default), PT_K of them fit a chunk with its header, so
 * PT_MAX = PT_NCH x PT_K records of any kind always fit, and a new patch is refused ("NO FREE PLACE") at PT_MAX instead of
 * failing half way ("STORE FULL" stays only as a guard for a store written by other rules). A chunk takes a new record only
 * under PT_K records, which keeps that guarantee.
 *
 * Chunk: u16 'PT' magic, u8 version 1, u8 0, then records back to back. Record (little endian):
 *   0 u16 length of the whole record     2 u8 slot (0..127)       3 u8 revision (+1 at every rewrite; a record that moved to
 *   4 u8 flags PTF_*                      5 u8 engine              another chunk exists twice for a moment: the newer wins)
 *   6 u8 category (raw, 0 none)           7 12 bytes name, 0-padded                19 u8 npairs (nvalues with PTF_BM)
 *   20 [PTF_SH] 2 bytes shadow engine + 1, preset + 1     [PTF_MB] 7 bytes End B (morph block)
 *      npairs x (u8 parameter id, u8 value + 64)  SPARSE: only what differs from the engine's INIT (the defaults of
 *      params.c, as the factory sounds' format, sounds.c). PTF_BM (a sound that differs in more than 16 values, the encoder
 *      takes whichever is shorter): a 16-byte bitmap (bit id) instead, then nvalues x (u8 value + 64) in id order
 *      [PTF_PAT] 16 notes + 16 flags (a drum grid with PTF_GRID)    [PTF_FM6] the FM6 sound's patch, UPF_PK bytes
 * A write rebuilds only the chunk that changed and saves it as any storage.c object (erase the other sector, payload, commit
 * record LAST, read back): a power cut leaves the old chunk. A record that no longer fits its chunk goes to another one
 * first, then is cut from the old (up_put). No chunk with room: 'STORE FULL' (rc 4). The decoded form (up_rec_t, below) is
 * what the rest of the firmware and the editor see; up_rec() decodes into a ring of four.
 *
 * Felucca's own banks (32 slots, 192-byte records, 0xDC000..) are not read: DUELL is not released on a device that has them.
 * The decoded record's history, as before:
 *
 * Versions: a bank whose magic, record size or slot count differ reads as
 * empty; so does a record with another layout version. A record keeps np =
 * the P_COUNT it was stored with; when that differs from today's it is
 * mapped by count: its last 8 values are P_E0..P_E7, the first np - 8 are
 * P_LEVEL.. in order, and parameters it does not have take their defaults.
 * That holds as long as common parameters are only ever added just before
 * P_E0 (else bump UP_VER and translate). Version 1 (before 1.0) has the
 * same layout; only its PHYS MODEL 2 meant DUST, which up_values loads as
 * MODAL bowed (eng_phys.c phys_legacy). A record of PHYS MODEL 4 (DRUM, version 2
 * until the kit became the DRUM engine) is that engine: up_migrate rewrites it
 * in the RAM mirror when a bank is read and when UP_PUT sends one (core.h
 * drum_from_phys); flash keeps the old record until its bank is written again,
 * which stores the new one. No version bump: today's PHYS has no MODEL 4, so
 * such a record cannot be anything else, and older firmware keeps reading the bank.
 * A record of SAMPLE SET 4 (PERC, the GM kit, retired after 1.0.2) is migrated the same way to the DRUM engine
 * with its default kit (core.h drum_from_perc: its E values the kit's, the rest of the sound and its pattern as
 * stored; the GM notes play the same drums).
 * Version 3 (UP_VER_GRID, since the DRUM grid) is the same record with a drum grid as its pattern: note[i] is
 * step i's lane hits (bit l = lane l), flags[i] their accents. A DRUM track whose first 16 steps strike a
 * lane is stored so (its notes on their lanes); every other sound as version 2, which older firmware reads.
 * Older firmware shows a version 3 record as empty and keeps its bytes.
 * A note pattern keeps each step's RATCH in its flags (core.h SF_RATCH, 8 | 16; 0 = x1, every older record):
 * bits firmware before RATCH drops when it loads the pattern, so the record is the same version. A drum grid
 * has no room for it (its flags are the accents): it loads x1.
 * 1.2 (P_COUNT 103: LFO 2's SYNC TRIG POL, QUANTIZE): no version bump, records map by count as above (one of 99
 * gets the four at their defaults, which play as before); firmware before reads a record of 103 by count too (its
 * 91 parameters before P_E0 and the engine's 8: the four new ones are left out). A pattern keeps no step's NUDGE (core.h
 * SF_NUDGE: its bit 4 would read as the tie here): the step on its start.
 * SPREAD (P_SPRD, #148, before 1.2's release) made it P_COUNT 104 the same way: a record of 103 gets SPREAD 0.
 * 1.5: the INSERT (P_ITYPE .. P_IMIX), ANALOG's filter TYPE and ENV SYNC (P_FTYPE, P_ESYNC) made it 111 the same way:
 * a record of 104 gets the INSERT OFF, LP and OFF; 1.2 .. 1.4 read a record of 111 by count (the seven left out).
 * A record of engine 1 (DIGITAL, retired in 1.0) stays as it is (UP_PUT takes it too): its values are DIGITAL's,
 * and every load converts them to an FM6 sound with its own patch (ui.c fm4_apply, fm4_convert.c); lists count it
 * with FM6's (up_engine).
 *
 * DUELL (step 3): versions 6 / 7 (UP_VER, UP_VER_GRID) are 4 / 5 with DUELL's layout: ids 8, 61..80 and 83..90 hold DUELL's
 * parameters (matrix slots 5..8, ENV 2, LFO 2; core.h PL_*). A record of versions 1..5 is Felucca's layout: it loads by
 * count as before, then those ids take today's defaults (up_values, core.h params_retire); a DIGITAL one converts with
 * its operator values read from their old places first (up_fm4_ops). Firmware before reads 6 / 7 as empty.
 *
 * With -DUP_HOST (host test) only the part above #ifndef UP_HOST is built;
 * it needs nothing but core.h. */
#define UP_PMAX 72u                              /* room for P_COUNT to grow */
#define UP_USED 0xA5u
#define UP_VER_FEL 4u                            /* Felucca's layout, a byte a value (read: core.h PL_*, params_retire) */
#define UP_VER_FEL_GRID 5u                       /* .. with a drum grid as the pattern (see the top) */
#define UP_VER 6u                                /* DUELL's layout (step 3: ids 61..90 and 8 are DUELL's, core.h PL_*), a byte
                                                  * a value; 4 / 5 and 1..3 are read */
#define UP_VER_GRID 7u                           /* 6 with a drum grid as the pattern */
typedef struct {
    uint8_t used, ver, engine, np;               /* UP_USED, UP_VER, engine, P_COUNT when stored */
    char name[12];                               /* ASCII 32..126, 0-padded (no 0 when 12 long) */
    union { int16_t p[UP_PMAX]; uint8_t packed[UP_PMAX * 2u]; };
    uint8_t note[16], flags[16];                 /* note 0 = rest; flags 1 accent, 2 slide, 4 tie, SF_RATCH
                                                  * (UP_VER_GRID: lane hits, their accents) */
} up_rec_t;
_Static_assert(sizeof(up_rec_t) == 192, "user preset record layout");
_Static_assert(P_COUNT <= UP_PMAX * 2u && P_COUNT < 128, "user preset record: P_COUNT");
/* 1.2 (Discussion #90): the sound's category (category.c CAT_*) in the record's last byte, packed[UP_CAT_AT]: a
 * version 4 / 5 record's values take np = P_COUNT (< 128) of its 144 bytes and nothing reads the others (up_valid
 * neither), so every record of before holds 0 there (CAT_NONE: listed by its engine, cat_of_engine) and firmware
 * before keeps the byte (a bank is written whole) without reading it: no format change, no version bump. A record of
 * versions 1..3 (a 16-bit value in every pair) has none; giving it one (a rename) rewrites it as version 4 / 5 when its
 * values fit a byte (up_to_v4; not a version 1 PHYS sound, whose MODEL 2 means DUST) */
#define UP_CAT_AT (UP_PMAX * 2u - 1u)
/* DUELL 3.5 (SAVE held: the patch saved over itself): End B of the patch in the 7 bytes before the category, packed[136..142]: 'M', the
 * seed (16 bit), AMOUNT, the roll's bits (morph.c MC_*), 0, a check byte (morph.c morph_blk_ok). Same reasoning as the
 * category: a record's values take np = P_COUNT (111) of the 144 bytes, nothing reads the others, so every older record
 * holds 0 there (no marker: no B, a load rolls a fresh one) and older firmware keeps the bytes: no format change, no
 * version bump. A record of versions 1..3 (16-bit values) has none; UP_PUT from the editor writes none */
#define UP_MB_N 7u
#define UP_MB_AT (UP_CAT_AT - UP_MB_N)
_Static_assert(P_COUNT <= UP_MB_AT, "user preset record: P_COUNT leaves the morph block its room");
/* DUELL save flow (2026-10-11): no split between the built-in sounds and the stored ones. A built-in preset lives in the
 * firmware image, so saving over one (SAVE held, ui_input.c) stores a copy that SHADOWS it: the two bytes before the morph
 * block, packed[134] = its engine + 1, packed[135] = its preset index + 1 (both 0: no shadow). The browser, its stepping,
 * FAV and the categories list the copy in the preset's place and hide the preset; erasing the copy brings it back. Same
 * reasoning as the morph block: bytes past np that nothing else reads, 0 in every older record, kept by older firmware
 * (which lists the copy as a plain stored sound): no format change, no version bump */
#define UP_SH_AT (UP_MB_AT - 2u)
_Static_assert(P_COUNT <= UP_SH_AT, "user preset record: P_COUNT leaves the shadow marker its room");
#include "category.c"
static uint32_t up_cat_of(const up_rec_t *r)
{
    uint32_t c = r->ver >= 4u && r->np <= UP_CAT_AT ? r->packed[UP_CAT_AT] : 0u;
    return c && c < CAT_N ? c : cat_of_engine(r->engine);
}
static int up_to_v4(up_rec_t *r)                 /* 1 = r is version 4 / 5 (now) */
{
    int16_t v[UP_PMAX];
    uint32_t i;
    if (r->ver >= 4u)
        return 1;
    if (r->ver == 1u && r->engine == ENGI_PHYS)
        return 0;
    for (i = 0; i < r->np; i++)
        if ((v[i] = r->p[i]) < -64 || v[i] > 127)
            return 0;
    memset(r->packed, 0, sizeof r->packed);
    r->ver = r->ver == 3u ? UP_VER_FEL_GRID : UP_VER_FEL;   /* (still Felucca's layout: its values as they were) */
    for (i = 0; i < r->np; i++)
        r->packed[i] = (uint8_t)(v[i] + 64);
    return 1;
}
static void up_set_cat(up_rec_t *r, uint32_t c)  /* c CAT_BASS..CAT_OTHER; a record that cannot hold one keeps none */
{
    if (c && c < CAT_N && up_to_v4(r))
        r->packed[UP_CAT_AT] = (uint8_t)c;
}

static int up_valid(const up_rec_t *r)
{
    if (!(r->used == UP_USED && r->ver >= 1u && r->ver <= UP_VER_GRID && r->engine < NENGINES &&
          r->np >= 8u && r->np <= (r->ver >= 4u ? UP_PMAX * 2u : UP_PMAX) && r->name[0])) return 0;
    if (r->ver >= 4u) for (uint32_t i = 0; i < r->np; i++) if (r->packed[i] > 191u) return 0;
    return 1;
}

static int up_grid(const up_rec_t *r) { return r->ver == 3u || r->ver == UP_VER_FEL_GRID || r->ver == UP_VER_GRID; }
/* 1: a record of DUELL's layout; 0: Felucca's (versions 1..5), whose ids 8, 61..80 and 83..90 meant what core.h PL_* says */
static int up_duell(const up_rec_t *r) { return r->ver >= UP_VER; }
static int16_t up_value(const up_rec_t *r, uint32_t k) { return r->ver >= 4u ? (int16_t)r->packed[k] - 64 : r->p[k]; }
static void up_set_value(up_rec_t *r, uint32_t k, int16_t v)
{
    if (r->ver >= 4u) r->packed[k] = (uint8_t)(v + 64); else r->p[k] = v;
}
/* Legacy PHYS drums map through decoded parameters, not their disk representation. */
static void up_migrate(up_rec_t *r)
{
    int16_t e[8]; uint32_t k;
    if (!up_valid(r)) return;
    for (k = 0; k < 8u; k++) e[k] = up_value(r, r->np - 8u + k);
    if (drum_from_phys(r->engine, e) || drum_from_perc(r->engine, e)) {   /* (SAMPLE PERC: see the top) */
        r->engine = ENGI_DRUM;
        for (k = 0; k < 8u; k++) up_set_value(r, r->np - 8u + k, e[k]);
    }
}

static void up_pat_norm(uint8_t *note, uint8_t *flags);

/* the record's values in today's P_* order (mapped by count, see above); def = the defaults */
static void up_params(const up_rec_t *r, int16_t *out, const int16_t *def)
{
    int16_t values[UP_PMAX * 2u];
    for (uint32_t i = 0; i < r->np && i < NELEM(values); i++) values[i] = up_value(r, i);
    params_by_count(out, values, r->np, def);
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
 * (values not yet clamped); 0 ok, 1 bad arguments. *slot gets the slot byte when there is one. kind 1 (and its 16
 * bytes): a drum grid, the pairs the low 7 bits of each step's hits and accents, hi bit 0 / 1 their bit 7 (lane 8).
 * category (1.2, category.c CAT_*, 0 none): after the kind's 16 bytes (kind 0 sends them too, ignored) */
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
        (na != end && ((na != end + 17u && na != end + 18u) || a[end] > 1u)) || (na == end + 18u && a[end + 17u] >= CAT_N))
        return 1;
    up_rec_t staged, *target = r;
    r = &staged;
    memset(r, 0, sizeof *r);
    r->used = UP_USED;
    r->ver = UP_VER;
    r->engine = a[1];
    r->np = P_COUNT;
    for (i = 0; i < n; i++)
        r->name[i] = (char)a[2 + i];
    for (i = 0; i < P_COUNT; i++, k += 2u)
        {
            int32_t value = (int32_t)((a[k] & 127u) | (a[k + 1] & 127u) << 7) - 8192;
            if (value < -64 || value > 127) return 1;
            up_set_value(r, i, (int16_t)value);
        }
    if (na >= k + 33u + 16u && a[k + 32u] == 1u) {    /* a drum grid */
        r->ver = UP_VER_GRID;
        for (i = 0; i < 16u; i++) {
            uint32_t hi = a[k + 33u + i];
            r->note[i] = (uint8_t)((a[k + 2u * i] & 127u) | (hi & 1u) << 7);
            r->flags[i] = (uint8_t)(((a[k + 2u * i + 1u] & 127u) | (hi & 2u) << 6) & r->note[i]);
        }
        if (na == end + 18u)
            up_set_cat(r, a[end + 17u]);
        *target = *r;
        return 0;
    }
    for (i = 0; i < 16u; i++, k += 2u) {
        r->note[i] = a[k];
        r->flags[i] = a[k + 1];
        up_pat_norm(&r->note[i], &r->flags[i]);
    }
    if (na == end + 18u)
        up_set_cat(r, a[end + 17u]);
    up_migrate(r);                               /* (an editor of before the DRUM engine) */
    *target = *r;
    return 0;
}

#ifndef UP_HOST
static void up_values(const up_rec_t *r, int16_t *v)   /* mapped and clamped for its engine */
{
    int16_t def[P_COUNT];
    uint32_t i;
    for (i = 0; i < P_COUNT; i++)
        def[i] = param_desc_of(r->engine, i)->def;
    up_params(r, v, def);
    if (!up_duell(r))                                   /* Felucca's layout: no old value on a DUELL parameter */
        params_retire(v, def);
    if (r->ver == 1u && ENGINES[r->engine] == &ENG_PHYS)   /* (before 1.0: MODEL 2 was DUST) */
        phys_legacy(&v[P_E0]);
    for (i = 0; i < P_COUNT; i++)
        v[i] = (int16_t)param_fit(param_desc_of(r->engine, i), v[i]);   /* (a retired KIT: the kit it plays) */
}

/* a DIGITAL record's OP ENV / OP LEVEL values (fm4_convert's ops): a record of Felucca's layout has them at its ids
 * 61..80 (core.h PL_FM1_ATK; one of before them: the defaults); DUELL's has none (0) */
static const int16_t *up_fm4_ops(const up_rec_t *r, int16_t *ops)
{
    int16_t old[P_COUNT], def[P_COUNT];
    uint32_t i;
    if (up_duell(r))
        return 0;
    for (i = 0; i < P_COUNT; i++)
        def[i] = i >= PL_FM1_ATK && i <= PL_FM4_LEVEL ? FM4_OPS_DEF[i - PL_FM1_ATK] : 0;
    up_params(r, old, def);
    return fm4_ops(old, ops);
}


/* ---------------------------------------------------------------- the store --- */
#define PT_NCH 6u                                /* chunks = storage.c objects OBJ_PATCH0..5 */
#define PT_CAP 3840u                             /* a chunk's most bytes (storage.c ST_PAYLOAD_MAX) */
#define PT_HDR 4u
#define PT_MAGIC 0x5450u                         /* "PT" */
#define PT_FIX 20u                               /* a record's fixed part */
#define PT_NONE 0xFFFFu
#define PT_FULL 4                                /* up_put: no chunk has room (STORE FULL: a guard only, see PT_MAX) */
#define PT_NOFREE 5                              /* up_put: PT_MAX patches are stored, a new one has no place (NO FREE PLACE) */
#define PT_BMB 16u                               /* PTF_BM's bitmap: ids 0..127 */
#define PTF_PAT 1u
#define PTF_GRID 2u
#define PTF_FM6 4u
#define PTF_MB 8u
#define PTF_SH 16u
#define PTF_AK 32u                               /* DUELL: the ARP / KEYS / SEQUENCER block (ak_* below), the record's last part */
#define PTF_BM 64u                               /* the record's values as a bitmap + values, not (id, value) pairs (shorter above 16) */
#define PTF_ALL 127u
#define UPF_PK 112u                              /* an FM6 patch, FM6_PACKED 7-bit bytes packed 8 to 7 (112 bytes) */
#if FELUCCA_FLASH
_Static_assert(PT_CAP == ST_PAYLOAD_MAX, "a patch chunk is one storage object");
#endif
#define AK_MAX 190u                              /* the AK block's most bytes with its length (ak_build) */
_Static_assert(PT_FIX + 2u + UP_MB_N + 2u * P_COUNT + 32u + UPF_PK + AK_MAX <= 640u, "pt_enc holds a record");
_Static_assert(P_COUNT <= PT_BMB * 8u, "PTF_BM's bitmap holds every parameter id");
static uint8_t pt_img[PT_NCH][PT_CAP] __attribute__((aligned(4), section(".pool")));   /* the chunks as in flash (main loop only) */
static uint16_t pt_len[PT_NCH];                  /* each chunk's bytes (0: none stored yet) */
static uint16_t pt_idx[UP_SLOTS];                /* slot -> chunk << 12 | offset, PT_NONE: unused */
static uint8_t pt_enc[640] __attribute__((section(".pool")));
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
/* where a record's optional parts start: *sh, *mb, *pairs, *pat, *fm, *ak (offsets); the total length. The AK block (u16 length,
 * content) is last; one that does not fit the record's own length makes the total wrong (pt_rec_ok refuses it) */
static uint32_t pt_layout_x(const uint8_t *p, uint32_t *sh, uint32_t *mb, uint32_t *pr, uint32_t *pat, uint32_t *fm, uint32_t *ak)
{
    uint32_t f = p[4], o = PT_FIX;
    *sh = o;
    o += f & PTF_SH ? 2u : 0u;
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
static uint32_t pt_layout(const uint8_t *p, uint32_t *sh, uint32_t *mb, uint32_t *pr, uint32_t *pat, uint32_t *fm)
{
    uint32_t ak;
    return pt_layout_x(p, sh, mb, pr, pat, fm, &ak);
}
static int pt_rec_ok(const uint8_t *p, uint32_t rl)   /* a well-formed record of rl bytes */
{
    uint32_t a, b, c, d, e, i, n = 0;
    if (rl < PT_FIX || pt_rd16(p) != rl || p[2] >= UP_SLOTS || p[5] >= NENGINES || (p[4] & ~PTF_ALL) || !p[7])
        return 0;
    if (pt_layout(p, &a, &b, &c, &d, &e) != rl)
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
static void pt_cut(uint32_t c, uint32_t off)     /* the record at off out of chunk c */
{
    uint32_t rl = pt_rd16(pt_img[c] + off);
    uint32_t i;
    for (i = off; i + rl < pt_len[c]; i++)           /* (down, forwards: the firmware's libc has no memmove) */
        pt_img[c][i] = pt_img[c][i + rl];
    pt_len[c] = (uint16_t)(pt_len[c] - rl);
}
/* the index again from the chunks: a record twice (a move cut short) keeps the newer revision; the older one, and any
 * record that is no record, is dropped from the RAM image (flash follows at the chunk's next write) */
static void pt_scan(void)
{
    uint32_t c, off, k;
again:
    for (k = 0; k < UP_SLOTS; k++)
        pt_idx[k] = PT_NONE;
    for (c = 0; c < PT_NCH; c++) {
        if (pt_len[c] < PT_HDR) {
            pt_len[c] = 0;
            continue;
        }
        for (off = PT_HDR; off + PT_FIX <= pt_len[c];) {
            const uint8_t *p = pt_img[c] + off;
            uint32_t rl = pt_rd16(p);
            if (rl < PT_FIX || off + rl > pt_len[c]) {   /* damaged tail: cut */
                pt_len[c] = (uint16_t)off;
                break;
            }
            if (pt_rec_ok(p, rl)) {
                k = p[2];
                if (pt_idx[k] == PT_NONE ||
                    (uint8_t)(p[3] - pt_img[pt_idx[k] >> 12][(pt_idx[k] & 0xFFFu) + 3u]) - 1u < 127u)
                    pt_idx[k] = (uint16_t)(c << 12 | off);
            }
            off += rl;
        }
    }
    for (c = 0; c < PT_NCH; c++)                 /* a record the index does not point to: out */
        for (off = PT_HDR; off + PT_FIX <= pt_len[c]; off += pt_rd16(pt_img[c] + off))
            if (!pt_rec_ok(pt_img[c] + off, pt_rd16(pt_img[c] + off)) || pt_idx[pt_img[c][off + 2u]] != (uint16_t)(c << 12 | off)) {
                pt_cut(c, off);
                goto again;
            }
}
static const uint8_t *pt_raw(uint32_t k)         /* slot k's record in the chunks, 0: unused */
{
    return k < UP_SLOTS && pt_idx[k] != PT_NONE ? pt_img[pt_idx[k] >> 12] + (pt_idx[k] & 0xFFFu) : 0;
}
static int up_used(uint32_t k) { return pt_raw(k) != 0; }

/* ---------------------------------------------------------------- the AK block (DUELL: arp, keys, sequencer) ---
 * A patch (a user patch, or the shadow copy of a factory sound) carries the part's ARP, its chord / voicing / scale keys and its
 * SEQUENCER pattern, so loading it brings them back (up_load: the lend rule of the factory sounds, ui.c ds_arp). The record's last
 * part, flag PTF_AK: u16 content length, then
 *   u8 g       groups present: AK_ARP 1 (the arp's eight values P_AMODE .. P_AORDER: stored when the arp is ON, or for a shadow
 *              of a factory sound that sets an arp, so that an arp the player switched off stays off), AK_KEYS 2 (P_SCALE P_CHRD
 *              P_VOIC; stored when any is not its default, likewise for a shadow of a sound that sets keys; ROOT stays the
 *              player's), AK_SEQ 4 (the sequencer pattern; stored when the arp mode is SEQ, as the pattern is then the arp's)
 *   u8 np      then np x (u8 parameter id, u8 value + 64): the values of the present groups that differ from the defaults
 *              (the groups' other values are the defaults); AK_SEQ's ids are LEN DIV SWING GATE QUANTIZE (LEN at most 16).
 *              With g bit 8 (AK_BM, from 3 values on: shorter) there is no np: a u16 bitmap (bit = the value's ordinal: the arp's
 *              eight 0..7, the keys' three 8..10, the sequencer's five 11..15) then one u8 (value + 64) per set bit, in ordinal order
 *   AK_SEQ:    u16 map (bit i: step i of 16 is stored; a step not stored is empty), then 9 bytes per stored step in the
 *              multi's / pattern slots' packing (patslot.c ps_pack_step: notes, time, accent, slide, RATCH, NUDGE, velocity,
 *              CHANCE, the hits)
 * The ids of the present groups are not stored in the record's own pairs (up_rec puts them back). A record without PTF_AK
 * (every older one, and a patch with the arp off, no keys and no SEQ arp) leaves the part's arp, keys and steps as they are. ARP TARGET
 * (P_ATGT) is not in it: it is the sound's own, a pair of the record as ever. The pattern is 16 steps (the record's old pattern,
 * PTF_PAT, is these steps' notes and flags: not stored twice). */
#define AK_ARP 1u
#define AK_KEYS 2u
#define AK_SEQ 4u
#define AK_BM 8u                                 /* (g's bit 8: the values as a bitmap, see above) */
#define AK_NS 16u
#define PS_STEP_N 9u                             /* patslot.c PS_STEP */
static void ps_pack_step(const step_t *s, uint8_t *b);   /* (patslot.c) */
static int ps_unpack_step(const uint8_t *b, step_t *s);
static const uint8_t AK_SEQID[5] = {P_SLEN, P_SDIV, P_SSWING, P_SGATE, P_SQNT};
/* the worst record: AK_WORST is a block with all sixteen values and all sixteen steps (u16 length, g, bitmap, values, step map,
 * steps); PT_W_SEQ a record that carries it with an FM6 patch, End B, a shadow marker and every other value off its default
 * (and no pattern of its own: the block's steps are it); PT_W_PLAIN one with no block but the pattern and the patch. PT_K of the
 * worst fit a chunk (its 4-byte header included), PT_MAX = PT_NCH x PT_K always fit: the limit of the store (test: patch_test) */
#define AK_WORST (2u + 1u + 2u + 16u + 2u + PS_STEP_N * AK_NS)
#define PT_W_SEQ (PT_FIX + 2u + UP_MB_N + PT_BMB + (P_COUNT - 16u) + UPF_PK + AK_WORST)
#define PT_W_PLAIN (PT_FIX + 2u + UP_MB_N + PT_BMB + P_COUNT + 32u + UPF_PK)
#define PT_WORST (PT_W_SEQ > PT_W_PLAIN ? PT_W_SEQ : PT_W_PLAIN)
#define PT_K ((PT_CAP - PT_HDR) / PT_WORST)
#define PT_MAX (PT_NCH * PT_K)
_Static_assert(PT_K >= 1u && PT_MAX <= UP_SLOTS && AK_WORST <= AK_MAX, "the store's limit: PT_MAX records of the worst size fit, in slots 0..UP_SLOTS-1");
static uint32_t ak_id(uint32_t o)                /* the ordinal o (0..15) -> its parameter id */
{
    return o < 8u ? P_AMODE + o : o < 11u ? DS_KEYID[o - 8u] : AK_SEQID[o - 11u];
}
static uint32_t ak_grp(uint32_t id)              /* the group an id belongs to (0: none) */
{
    uint32_t i;
    if (id >= P_AMODE && id <= P_AORDER)
        return AK_ARP;
    for (i = 0; i < 3u; i++)
        if (id == DS_KEYID[i])
            return AK_KEYS;
    for (i = 0; i < 5u; i++)
        if (id == AK_SEQID[i])
            return AK_SEQ;
    return 0;
}
typedef struct {
    uint8_t g;
    int16_t arp[8], key[3], seq[5];              /* P_AMODE.., DS_KEYID, AK_SEQID order */
    step_t st[AK_NS];
} ak_t;
static int16_t *ak_slot(ak_t *a, uint32_t id)
{
    uint32_t i;
    if (id >= P_AMODE && id <= P_AORDER)
        return &a->arp[id - P_AMODE];
    for (i = 0; i < 3u; i++)
        if (id == DS_KEYID[i])
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
            a->key[i] = param_desc_of(eng, DS_KEYID[i])->def;
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
        a->key[i] = param_desc_of(eng, DS_KEYID[i])->def;
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
/* the part's AK block -> o (AK_MAX bytes): u16 length + content; the length of all (0: nothing to store). fs: the factory sound
 * the record shadows (0: none) */
static uint32_t ak_build(const track_t *t, uint8_t *o, const ds_sound_t *fs)
{
    uint32_t eng = t->eng_req, g = 0, i, at = 4u, np = 0, map = 0;
    int32_t v;
    if (motion_base_value(t, P_AMODE) || (fs && (fs->flags & DSF_ARP)))
        g |= AK_ARP;
    for (i = 0; i < 3u; i++)
        if (motion_base_value(t, DS_KEYID[i]) != param_desc_of(eng, DS_KEYID[i])->def)
            g |= AK_KEYS;
    if (fs && (fs->flags & DSF_KEYS))
        g |= AK_KEYS;
    if (motion_base_value(t, P_AMODE) == AM_SEQ)
        g |= AK_SEQ;
    if (!g)
        return 0;
    {   /* the values of the present groups that differ from the defaults, in ordinal order (ak_id) */
        uint8_t oid[16], ov[16];
        uint32_t bm = 0, j;
        for (j = 0; j < 16u; j++) {
            i = ak_id(j);
            if (!(ak_grp(i) & g))
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
    uint32_t sh, mb, pr, pat, fm, ak;
    if (!p || !(p[4] & PTF_AK))
        return 0;
    pt_layout_x(p, &sh, &mb, &pr, &pat, &fm, &ak);
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
    uint32_t sh, mb, pr, pat, fm, i;
    memset(r, 0, sizeof *r);
    if (!p)
        return r;
    pt_layout(p, &sh, &mb, &pr, &pat, &fm);
    r->used = UP_USED;
    r->ver = p[4] & PTF_GRID ? UP_VER_GRID : UP_VER;
    r->engine = p[5];
    r->np = P_COUNT;
    memcpy(r->name, p + 7, 12);
    for (i = 0; i < P_COUNT; i++)
        up_set_value(r, i, param_desc_of(r->engine, i)->def);
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
    r->packed[UP_CAT_AT] = p[6];
    if (p[4] & PTF_MB)
        memcpy(r->packed + UP_MB_AT, p + mb, UP_MB_N);
    if (p[4] & PTF_SH)
        memcpy(r->packed + UP_SH_AT, p + sh, 2);
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
            for (i = 0; i < 3u; i++)
                if (a.g & AK_KEYS)
                    up_set_value(r, DS_KEYID[i], a.key[i]);
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
    uint8_t *o = pt_enc, n = 0, f = 0, did[P_COUNT], dv[P_COUNT];
    uint32_t i, at, mb = 0, gm;
    if (r->ver >= 4u && r->np <= UP_MB_AT)
        for (i = 0; i < UP_MB_N; i++)
            mb |= r->packed[UP_MB_AT + i];
    if (up_ak_in && pt_rd16(up_ak_in) && up_ak_in[2])
        f |= PTF_AK;
    else
        up_ak_in = 0;
    gm = f & PTF_AK ? up_ak_in[2] & 7u : 0u;
    if (!up_pat_empty(r) && !(gm & AK_SEQ))          /* (the AK block's steps are the pattern) */
        f |= PTF_PAT;
    if (up_grid(r))
        f |= PTF_GRID;
    if (pk && r->engine == ENGI_FM6)
        f |= PTF_FM6;
    if (mb)
        f |= PTF_MB;
    if (r->ver >= 4u && r->np <= UP_SH_AT && (r->packed[UP_SH_AT] || r->packed[UP_SH_AT + 1u]))
        f |= PTF_SH;
    for (i = 0; i < P_COUNT; i++) {                  /* the values off their defaults (not the AK block's: it carries them) */
        int32_t v = i < r->np ? up_value(r, i) : param_desc_of(r->engine, i)->def;
        if (v != param_desc_of(r->engine, i)->def && !(ak_grp(i) & gm)) {
            did[n] = (uint8_t)i;
            dv[n++] = (uint8_t)(v + 64);
        }
    }
    if (n > PT_BMB)                                  /* a bitmap and a byte each is shorter than a pair each from 17 on */
        f |= PTF_BM;
    memset(o, 0, PT_FIX);
    o[2] = (uint8_t)k;
    o[3] = (uint8_t)rev;
    o[4] = f;
    o[5] = r->engine;
    o[6] = r->ver >= 4u && r->np <= UP_CAT_AT ? r->packed[UP_CAT_AT] : 0u;
    memcpy(o + 7, r->name, 12);
    at = PT_FIX;
    if (f & PTF_SH) {
        o[at++] = r->packed[UP_SH_AT];
        o[at++] = r->packed[UP_SH_AT + 1u];
    }
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

/* chunk c to flash (the standard commit-last write): 0 ok, 2 flash error, 3 no flash (kept in RAM) */
static int pt_commit(uint32_t c)
{
#if FELUCCA_FLASH
    if (flash_ok)
        return st_save(OBJ_PATCH0 + c, pt_img[c], pt_len[c]) ? 2 : 0;
#endif
    (void)c;
    return 3;
}
static int pt_idle(void)                         /* what a write that changes nothing answers */
{
#if FELUCCA_FLASH
    return flash_ok ? 0 : 3;
#else
    return 3;
#endif
}
static void pt_reload(uint32_t c)                /* after a failed write: the chunk as flash holds it */
{
#if FELUCCA_FLASH
    if (flash_ok) {
        int n = st_load(OBJ_PATCH0 + c, pt_img[c], PT_CAP);
        pt_len[c] = (uint16_t)(n >= (int)PT_HDR && pt_rd16(pt_img[c]) == PT_MAGIC && pt_img[c][2] == 1u ? n : 0);
    }
#else
    (void)c;
#endif
}
static int pt_room(uint32_t c, uint32_t n) { return (pt_len[c] ? pt_len[c] : PT_HDR) + n <= PT_CAP; }
static uint32_t pt_cnt(uint32_t c)               /* the records in chunk c */
{
    uint32_t off, n = 0;
    for (off = PT_HDR; off + PT_FIX <= pt_len[c]; off += pt_rd16(pt_img[c] + off))
        n++;
    return n;
}
static uint32_t pt_total(void)                   /* the stored patches */
{
    uint32_t k, n = 0;
    for (k = 0; k < UP_SLOTS; k++)
        n += pt_idx[k] != PT_NONE;
    return n;
}
/* chunk d takes a NEW record of n bytes: under PT_K records (so that PT_K worst-case records always fit it) and with the room */
static int pt_takes(uint32_t d, uint32_t n) { return pt_cnt(d) < PT_K && pt_room(d, n); }
static void pt_append(uint32_t c, uint32_t n)    /* pt_enc (n bytes) at the end of chunk c */
{
    if (!pt_len[c]) {
        pt_img[c][0] = (uint8_t)PT_MAGIC;
        pt_img[c][1] = (uint8_t)(PT_MAGIC >> 8);
        pt_img[c][2] = 1;
        pt_img[c][3] = 0;
        pt_len[c] = PT_HDR;
    }
    memcpy(pt_img[c] + pt_len[c], pt_enc, n);
    pt_len[c] = (uint16_t)(pt_len[c] + n);
}
/* slot k = r (+ pk), or erased (r 0), in the chunks and in flash: 0 ok, 2 flash error (nothing changed), 3 no flash
 * (kept in RAM), PT_FULL no room (a guard only), PT_NOFREE a new patch with PT_MAX stored */
static int pt_put(uint32_t k, const up_rec_t *r, const uint8_t *pk)
{
    uint32_t oc = PT_NCH, ooff = 0, orl = 0, rev = 0, n, d;
    int rc;
    if (pt_idx[k] != PT_NONE) {
        oc = pt_idx[k] >> 12;
        ooff = pt_idx[k] & 0xFFFu;
        orl = pt_rd16(pt_img[oc] + ooff);
        rev = (uint32_t)pt_img[oc][ooff + 3u] + 1u;
    }
    if (!r) {                                    /* erase */
        if (oc == PT_NCH)
            return pt_idle();
        pt_cut(oc, ooff);
        if ((rc = pt_commit(oc)) == 2)
            pt_reload(oc);
        pt_scan();
        return rc;
    }
    if (oc == PT_NCH && pt_total() >= PT_MAX)    /* a new patch, PT_MAX stored: no place (an overwrite is always in place) */
        return PT_NOFREE;
    n = pt_encode(k, rev, r, pk);
    if (oc != PT_NCH && pt_len[oc] - orl + n <= PT_CAP) {   /* in its own chunk */
        pt_cut(oc, ooff);
        pt_append(oc, n);
        if ((rc = pt_commit(oc)) == 2)
            pt_reload(oc);
        pt_scan();
        return rc;
    }
    for (d = 0; d < PT_NCH && (d == oc || !pt_takes(d, n)); d++)
        ;
    if (d == PT_NCH)
        return PT_FULL;
    pt_append(d, n);                             /* the new record first (the newer revision wins until the old is cut) */
    if ((rc = pt_commit(d)) == 2) {
        pt_reload(d);
        pt_scan();
        return 2;
    }
    if (oc != PT_NCH) {
        pt_cut(oc, ooff);
        if (pt_commit(oc) == 2)
            pt_reload(oc);                       /* (the old one stays; the newer revision still wins) */
    }
    pt_scan();
    return rc;
}

/* record k's fields without decoding it */
static uint32_t up_engine_raw(uint32_t k) { const uint8_t *p = pt_raw(k); return p ? p[5] : 0u; }
static void up_name(uint32_t k, char *b)       /* upper case, 0-terminated: b holds 13 */
{
    const uint8_t *p = pt_raw(k);
    uint32_t i;
    for (i = 0; p && i < 12u && p[7 + i]; i++)
        b[i] = p[7 + i] >= 'a' && p[7 + i] <= 'z' ? (char)(p[7 + i] - 32) : (char)p[7 + i];
    b[i] = 0;
}
static uint32_t up_cat(uint32_t k)             /* a used slot's category (an unused slot: OTHER) */
{
    const uint8_t *p = pt_raw(k);
    return p && p[6] && p[6] < CAT_N ? p[6] : cat_of_engine(p ? p[5] : 0u);
}

static int up_put(uint32_t k, const up_rec_t *r);
#include "up_fm6.c"                            /* an FM6 sound's patch in its record */
#include "fm6_vbank.c"                         /* FM6's voice bank (SLOT B1..B32, 1.4.1) */
#include "patslot.c"                           /* DUELL: the pattern slots P1..P16 (SAVE > PATTERNS) */

static void pt_boot(void)                      /* the chunks from flash (a power-on's RAM: all gone before) */
{
    uint32_t b;
    for (b = 0; b < PT_NCH; b++) {
        pt_len[b] = 0;
        pt_reload(b);
    }
    pt_scan();
    up_gen++;
}
static void up_boot(void)                      /* persist_boot: the chunks from flash */
{
    pt_boot();
    fvb_boot();                                  /* FM6's voice bank: a staged one finished, its voices */
    ps_boot();                                   /* DUELL: the pattern slots */
#ifdef FELUCCA_FAVORITES
    for (uint32_t k = 0; k < UP_SLOTS; k++)
        if (!up_used(k)) favorite_set(NENGINES, k, 0);
#endif
    up_gen++;                                    /* (the shadow table: read again) */
}

/* RAM only (a test's power-on): no patch stored */
static void up_ram_reset(void)
{
    memset(pt_len, 0, sizeof pt_len);
    pt_scan();
    up_gen++;
}

/* the shadows (UP_SH_AT), a table read again whenever the banks change (up_gen): slot k shadows engine up_sh_e[k] - 1's
 * preset up_sh_p[k] (0: none). Only a marker that names a preset the browser lists (an engine shown, not an alias) counts,
 * and only the first slot that names it: any other is listed as a plain stored sound, so no slot ever disappears */
static uint8_t up_sh_e[UP_SLOTS], up_sh_p[UP_SLOTS];
static uint32_t up_sh_seen = 0xFFFFFFFFu;
static void up_sh_scan(void)
{
    uint32_t k, j;
    if (up_sh_seen == up_gen)
        return;
    up_sh_seen = up_gen;
    for (k = 0; k < UP_SLOTS; k++) {
        const uint8_t *q = pt_raw(k);
        uint32_t e, p;
        up_sh_e[k] = up_sh_p[k] = 0;
        if (!q || !(q[4] & PTF_SH))
            continue;
        e = q[PT_FIX];
        p = q[PT_FIX + 1u];
        if (!e || !p || e > NENGINES || !eng_ok(e - 1u) || p > ds_count(e - 1u))   /* (DUELL: a factory sound, sounds.c) */
            continue;
        for (j = 0; j < k && !(up_sh_e[j] == e && up_sh_p[j] == p - 1u); j++)
            ;
        if (j == k) {
            up_sh_e[k] = (uint8_t)e;
            up_sh_p[k] = (uint8_t)(p - 1u);
        }
    }
}
/* slot k's shadowed engine (*p its preset), NENGINES: a plain stored sound (or none) */
static uint32_t up_shadow(uint32_t k, uint32_t *p)
{
    up_sh_scan();
    if (k >= UP_SLOTS || !up_sh_e[k])
        return NENGINES;
    *p = up_sh_p[k];
    return up_sh_e[k] - 1u;
}
/* the slot that shadows engine e's preset p, UP_SLOTS: none */
static uint32_t up_shadow_slot(uint32_t e, uint32_t p)
{
    uint32_t k;
    up_sh_scan();
    for (k = 0; k < UP_SLOTS && !(up_sh_e[k] == e + 1u && up_sh_p[k] == p); k++)
        ;
    return k;
}

/* record k = *r (0: erase) in the patch store and flash: 0 ok, 1 bad slot, 2 flash error or the transport runs (nothing
 * written), 3 no flash (kept in RAM), 4 STORE FULL (no chunk has room: a guard, PT_MAX records
 * always fit; nothing written), 5 NO FREE PLACE (a new patch with PT_MAX stored; nothing written). up_pk_in: the FM6 patch it carries */
static int up_put(uint32_t k, const up_rec_t *r)
{
    uint32_t shp;
    int shadow, rc;
    if (k >= UP_SLOTS)
        return 1;
    shadow = up_shadow(k, &shp) < NENGINES;            /* (an erase: what the slot was) */
    if (transport_busy()) {                            /* no flash erase while playing (project_save) */
        ui_message("STOP TO SAVE");
        return 2;
    }
    rc = pt_put(k, r, up_pk_in);
    if (rc == 2 || rc == PT_FULL || rc == PT_NOFREE)
        return rc;
    if (!r) {
#ifdef FELUCCA_FAVORITES
        if (favorite_set(NENGINES, k, 0)) settings_save();
#endif
        uint32_t i;
        for (i = 0; i < NTRK; i++)
            if (trk[i].user == k + 1u) {
                trk[i].user = 0;
                if (!shadow)                            /* a shadow's track stays its preset (up_load set t->preset): */
                    ui.noid |= (uint8_t)(1u << i);      /* SAVE held saves it again in its place; else no patch */
            }
    }
    up_gen++;
    return rc;
}

static void up_slot_label(char *b, uint32_t k)  /* "U007" (b holds 5) */
{
    b[0] = 'U';
    b[1] = (char)('0' + (k + 1u) / 100u % 10u);
    b[2] = (char)('0' + (k + 1u) / 10u % 10u);
    b[3] = (char)('0' + (k + 1u) % 10u);
    b[4] = 0;
}

/* the automatic name of engine e's sound in slot k: engine name + slot number ("ANALOG 007"); b holds 13 */
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
static uint8_t up_shmk[2];                       /* .. and the shadow marker (engine + 1, preset + 1; 0 0 none) */
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
    if (up_mblk)
        memcpy(r.packed + UP_MB_AT, up_mblk, UP_MB_N);
    r.packed[UP_SH_AT] = up_shmk[0];                   /* the shadow marker (0 0: none; only SAVE held sets one) */
    r.packed[UP_SH_AT + 1u] = up_shmk[1];
    if (drum_track(t)) {                                /* a DRUM track that strikes a lane: its grid */
        uint32_t any = 0;
        for (i = 0; i < 16u; i++)
            any |= step_lanes(&t->step[i]);
        if (any) {
            r.ver = UP_VER_GRID;
            for (i = 0; i < 16u; i++) {
                r.note[i] = (uint8_t)step_lanes(&t->step[i]);
                r.flags[i] = (uint8_t)step_accents(&t->step[i]);
            }
        }
    }
    {
        uint8_t pk[FM6_PACKED], akb[AK_MAX];
        int rc;
        /* the part's arp, keys and SEQ pattern (ak_*): for a shadow, the factory sound's flags say whether an arp that is off is
         * stored (so that a load does not bring the factory sound's arp back) */
        up_ak_in = ak_build(t, akb, up_shmk[0] ? ds_get((uint32_t)up_shmk[0] - 1u, (uint32_t)up_shmk[1] - 1u) : 0) ? akb : 0;
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

/* slot k -> the selected part's sound: engine and every parameter except the track's own (param_kept:
 * the mix, ARP, SCL, the pattern parameters). The steps stay, but for the patch's SEQ pattern (its AK block, lent: above); the
 * record's older pattern is loaded only from SAVE > PATTERNS (up_pat_load). 0 ok, 1 empty */
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
#if !FELUCCA_FM4
    if (r->engine == ENGI_DIGITAL) {                    /* a DIGITAL sound (kept as it was stored): FM6 */
        int16_t p[P_COUNT], ops[FM4_NOPS];
        for (i = 0; i < P_COUNT; i++)
            p[i] = param_kept(i) ? t->p[i] : v[i];
        fm4_apply(t, p, up_fm4_ops(r, ops));
    } else
#endif
    {
        fm1_irq_off();                                  /* the audio ISR must not see half a sound */
        t->eng_req = r->engine;
        for (i = 0; i < P_COUNT; i++)
            if (!param_kept(i))
                t->p[i] = v[i];
        t->preset = 0;
        fm1_irq_on();
        upf_track_load(t, k);                           /* FM6: the preset's own patch (up_fm6.c) */
    }
    {
        uint32_t p, e = up_shadow(k, &p);               /* a shadow: the track counts as its preset too (the browser's */
        const ds_sound_t *s = e == t->eng_req ? ds_get(e, p) : 0;
        if (e == t->eng_req)                            /* place, and erased later: that preset's place again) */
            t->preset = (uint8_t)p;
        if (s)                                          /* DUELL: a factory sound's copy plays at its level, with its arp */
            t->p[P_LEVEL] = (int16_t)ds_value(s, P_LEVEL, TP[P_LEVEL].def);
        {   /* the patch's own arp / keys / SEQ pattern (its AK block) wins over the factory sound's, and is lent as theirs is: the
             * part's own go aside and come back with the next load (ui.c ds_arp / ds_keys, ds_seq_back below). A patch
             * without the block (an older one, or arp off) leaves them as they are */
            ak_t a;
            int has = ak_get(k, &a);
            ds_arp(t, s, has && (a.g & AK_ARP) ? a.arp : 0);   /* (a lent arp back: ui.c) */
            ds_keys(t, s, has && (a.g & AK_KEYS) ? a.key : 0); /* (and the chord keys / scale: ui.c) */
            ds_seq_back(t);
            if (has && (a.g & AK_SEQ))
                ds_seq_lend(t, &a);
        }
    }
    ui.noid &= (uint8_t)~(1u << trk_index(t));          /* (a patch loaded: SAVE held saves over it) */
    t->user = (uint8_t)(k + 1u);
    morph_ld = r->ver >= 4u && r->np <= UP_MB_AT ? r->packed + UP_MB_AT : 0;   /* DUELL: End B saved with it (morph.c) */
    load_end(t);
    morph_ld = 0;
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
    if (up_grid(r))
        load_grid16(t, r->note, r->flags);
    else
        load_pat16(t, r->note, r->flags);
    for (i = P_SDIV; i <= P_SGATE; i++)
        t->p[i] = v[i];
    t->p[P_SLEN] = (int16_t)clamp(v[P_SLEN], 1, 16);   /* (the pattern has 16 steps) */
}

/* the engine a used slot's sound plays on (a DIGITAL record: FM6, without FELUCCA_FM4) */
static uint32_t up_engine(uint32_t k) { return eng_safe(up_engine_raw(k)); }

__attribute__((unused)) static uint32_t up_count(void)                 /* used slots */
{
    uint32_t k, n = 0;
    for (k = 0; k < UP_SLOTS; k++)
        n += (uint32_t)up_used(k);
    return n;
}

static uint32_t up_free_places(void)           /* the places a new patch may still take (PT_MAX less the stored ones) */
{
    uint32_t n = up_count();
    return n < PT_MAX ? PT_MAX - n : 0u;
}

__attribute__((unused)) static uint32_t up_nth(uint32_t n)             /* slot of the n-th used one (n < up_count()) */
{
    uint32_t k;
    for (k = 0; k < UP_SLOTS; k++)
        if (up_used(k) && !n--)
            return k;
    return 0;
}

__attribute__((unused)) static uint32_t up_rank(uint32_t slot)         /* used slots before it */
{
    uint32_t k, n = 0;
    for (k = 0; k < slot && k < UP_SLOTS; k++)
        n += (uint32_t)up_used(k);
    return n;
}

/* SAVE > USER page actions, with the message in the top bar. name: the save's or the rename's (0 or "": automatic);
 * cat: its category (0: the sound's, a rename keeps it) */
static void patch_say_nofree(void)              /* the header holds 130 px: two messages, one after the other */
{
    ui_message("NO FREE PLACE");
    str_cpy(ui.msg2, "(ERASE ONE)", sizeof ui.msg2);
}
static void up_ui_named(uint32_t op, uint32_t k, const char *name, uint32_t cat)   /* 0 load, 1 erase, 2 save, 3 rename */
{
    char l[6];
    int rc;
    up_slot_label(l, k);
    if ((op < 2u || op == 3u) && !up_used(k)) {
        ui_message("EMPTY SLOT");
        return;
    }
    if (op && (song.playing || transport_req == 1u)) {    /* (up_put refuses too) */
        ui_message("STOP TO SAVE");
        return;
    }
    if (op == 0u) {
        up_load(k);
        ui_say("LOADED ", l);
        return;
    }
    rc = op == 1u ? up_put(k, 0) : op == 3u ? up_rename_cat(k, name, cat) : up_store_cat(k, name, cat);
    if (rc == 3)
        ui_message(op == 1u ? "ERASED (RAM)" : op == 3u ? "RENAMED (RAM)" : "SAVED (RAM)");
    else if (rc == PT_NOFREE)
        patch_say_nofree();
    else if (rc)
        ui_message(op == 1u ? "ERASE ERROR" : "SAVE ERROR");
    else
        ui_say(op == 1u ? "ERASED " : op == 3u ? "RENAMED " : "SAVED ", l);
    ui.force = 1;
}
static void up_ui(uint32_t op, uint32_t k) { up_ui_named(op, k, 0, 0); }

/* PATCHES, K4 ERASE (a turn right): the patch under the cursor. A factory sound's copy: the dialog RESTORE FACTORY?; a patch of
 * its own: ERASE PATCH?; a factory sound without a copy, an empty place: a word. patch_erase_do: the dialog's OCT+ */
static void patch_erase_ask(void)
{
    uint32_t e, k;
    if (ui.pe) {
        ui_message("EMPTY PLACE");
        return;
    }
    if (!pl_row(&e, &k)) {
        ui_message("SELECT A PATCH");
        return;
    }
    if (e != NENGINES) {
        ui_message("ALREADY FACTORY");
        return;
    }
    if (transport_busy()) {
        ui_message("STOP TO SAVE");
        return;
    }
    confirm_open(CF_ERASE_USER, k);
}
static void patch_erase_do(uint32_t k)
{
    uint32_t sp = 0, se = up_shadow(k, &sp);
    int was = user_of(TSEL) == k;                       /* the selected part plays it */
    char nm[13];
    int rc;
    if (!up_used(k)) {
        ui_message("EMPTY PLACE");
        return;
    }
    up_name(k, nm);
    rc = up_put(k, 0);
    if (rc == 0 || rc == 3) {
        if (se < NENGINES) {                            /* a factory sound's copy: the factory sound is back in its place, and */
            if (was && TSEL->eng_req == se)             /* the part that played the copy plays it */
                apply_preset(sp);
            ui_message(rc == 3 ? "RESTORED (RAM)" : "FACTORY RESTORED");
        } else {                                        /* a patch of its own: its place is empty (the cursor goes to the first one) */
            if (!list_mode())
                ui.pe = 1;
            if (rc == 3)
                ui_message("ERASED (RAM)");
            else
                ui_say("ERASED ", nm);
        }
    } else if (rc != 2) {
        ui_message("ERASE ERROR");
    }
    ui.force = 1;
}

/* DUELL save flow (SPEC 3.5, 2026-10-11): SAVE held, OCT+ in its dialog (ui_input.c): part pi's patch over the one it plays.
 * The sound saved is the Original (End A, t->p: the morph never writes into it), with End B (morph_blk_make: seed, AMOUNT,
 * bits) in the record's morph block.
 * patch_target: what part pi's patch is. 0: a stored sound, *k its slot (a shadow keeps its marker); 1: a built-in preset
 * (*e, *p), saved as its shadow (UP_SH_AT): *k the copy it already has, UP_SLOTS a new one in the first free slot; 2: no
 * patch (INIT SOUND, its slot erased: ui.noid): *k UP_SLOTS, a new slot named after the engine */
static uint32_t patch_target(uint32_t pi, uint32_t *k, uint32_t *e, uint32_t *p)
{
    const track_t *t = &trk[pi % NTRK];
    uint32_t en = t->eng_req % NENGINES;
    uint32_t n = ds_count(en);                          /* (DUELL: the engine's factory sounds, sounds.c) */
    *e = NENGINES;
    *p = 0;
    if (up_new_only) {
        *k = UP_SLOTS;
        return 2;
    }
    if ((*k = user_of(t)) < UP_SLOTS) {
        *e = up_shadow(*k, p);
        return 0;
    }
    if (!((ui.noid >> (pi % NTRK)) & 1u) && eng_ok(en) && n) {
        *e = en;
        *p = t->preset % n;
        *k = up_shadow_slot(en, *p);                    /* (the preset loaded another way, the editor, an engine change: its copy) */
        return 1;
    }
    *k = UP_SLOTS;
    return 2;
}
/* the name of part pi's patch for the dialog (b holds 13): "" none */
static void patch_name(uint32_t pi, char *b)
{
    uint32_t k, e, p, w = patch_target(pi, &k, &e, &p);
    b[0] = 0;
    if (k < UP_SLOTS)
        up_name(k, b);
    else if (w == 1u)
        str_cpy(b, ds_name(e, p), 13);
}
/* the slot a new patch goes to: after the last plain one (the empty rows are the list's end), else the first free; UP_SLOTS none */
static uint32_t up_free_slot(void)
{
    uint32_t k, last = 0, any = 0;
    for (k = 0; k < UP_SLOTS; k++)
        if (up_plain(k)) {
            last = k;
            any = 1;
        }
    for (k = any ? last + 1u : 0u; k < UP_SLOTS; k++)
        if (!up_used(k))
            return k;
    for (k = 0; k < UP_SLOTS; k++)
        if (!up_used(k))
            return k;
    return UP_SLOTS;
}
/* 0 saved, 1 no free slot (impossible below PT_MAX), PT_NOFREE no free place, 4 the store is full (a guard), else up_put's error */
_Static_assert(UP_MB_N == MB_N, "morph.c MB_N");
static int up_patch_save_part(uint32_t pi)
{
    track_t *t = &trk[pi];
    uint32_t k, e, p, cat = 0, w = patch_target(pi, &k, &e, &p);
    uint8_t blk[UP_MB_N];
    char nm[16];
    const char *name = 0;
    int rc;
    up_shmk[0] = up_shmk[1] = 0;
    if (k < UP_SLOTS) {                                 /* over a stored sound: its name, category and marker */
        const up_rec_t *r = up_rec(k);
        up_name(k, nm);
        name = nm;
        cat = up_cat(k);
        if (r->ver >= 4u && r->np <= UP_SH_AT) {
            up_shmk[0] = r->packed[UP_SH_AT];
            up_shmk[1] = r->packed[UP_SH_AT + 1u];
        }
    } else {
        k = up_free_slot();
        if (up_count() >= PT_MAX)
            return PT_NOFREE;                           /* (a new patch, or a factory sound's first copy, takes one of the PT_MAX places) */
        if (k >= UP_SLOTS)
            return 1;
        if (w == 1u) {                                  /* a built-in preset: its copy, its name and category */
            name = ds_name(e, p);
            cat = preset_cat(e, p);
            up_shmk[0] = (uint8_t)(e + 1u);
            up_shmk[1] = (uint8_t)(p + 1u);
        } else {
            name = up_pname;                            /* (no patch: the NAME screen's, else the engine's name and the slot's number) */
        }
    }
    up_mblk = morph_blk_make(pi, blk) ? blk : 0;
    rc = up_store_trk(t, k, name, cat);
    up_mblk = 0;
    up_shmk[0] = up_shmk[1] = 0;
    if (rc == 0 || rc == 3) {
        t->user = (uint8_t)(k + 1u);                    /* the part plays that patch now (the next save: the same slot) */
        mph[pi].user = t->user;                         /* (the morph still belongs to this sound) */
        ui.noid &= (uint8_t)~(1u << pi);
    }
    return rc == 3 ? 0 : rc;
}
/* SAVE held, before its dialog: 1 when the part(s) to save need more new places (a patch of its own, a factory sound's first copy)
 * than are free; the dialog is not opened then (a save over a stored patch is always in place) */
static int patch_no_free(uint32_t both)
{
    uint32_t pi, first = both ? 0u : song.sel < NSHOWN ? song.sel : 0u, end = both ? NSHOWN : first + 1u, k, e, p, need = 0;
    for (pi = first; pi < end; pi++) {
        patch_target(pi, &k, &e, &p);
        need += k >= UP_SLOTS;
    }
    return need > up_free_places();
}
/* both: parts 1 and 2 (MORPH TARGET BOTH on the MORPH pages), else the selected one */
static void patch_save_do(uint32_t both)
{
    uint32_t pi, first = both ? 0u : song.sel < NSHOWN ? song.sel : 0u, end = both ? NSHOWN : first + 1u, n = 0;
    int rc = 0;
    if (song.playing || transport_req == 1u) {
        ui_message("STOP TO SAVE");
        return;
    }
    for (pi = first; pi < end && !rc; pi++, n++)
        rc = up_patch_save_part(pi);
    if (rc == PT_NOFREE)
        patch_say_nofree();
    else
        ui_message(rc == 1 || rc == PT_FULL ? "STORE FULL" : rc ? "SAVE ERROR" : n > 1u ? "SAVED 1+2" : "SAVED");
    ui.pe = 0;
    up_gen++;
    ui.force = 1;
}
#endif
