/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* DUELL pattern slots P01..P32 (SAVE > PATTERNS): ONE list of 32 patterns for the whole device (job F3, owner rule A8): independent
 * of the sounds and shared by both parts (any slot loads onto either part; the arp's SEQ mode plays the part's own steps, which came
 * from a slot like any other). NO slot is empty and none is protected: the 32 default patterns (engines.c PATTERNS[], PATTERNS_PROPOSAL.md)
 * are installed as ordinary slots on a first start, after ERASE DATA and when a stored object is not of this format; SAVE overwrites any
 * slot; ERASE of a slot gives its default back. A slot holds a part's step pattern only: all NSTEP (64) steps (notes, time, accent,
 * slide, RATCH, NUDGE, velocity, CHANCE) and the part's LEN, DIV, SWING, GATE and QUANTIZE, so the pattern plays back with its own
 * length and division. No sound, no automation.
 *
 * Format PTS3 (the format change of job F3; older objects, magic PTS2, are not read: the slots get their defaults): three storage.c
 * objects, OBJ_PATSLOT0..2, hold 11, 11 and 10 slots. There is no flash for more (the flash map is full: storage.c), so a slot is stored
 * COMPACTLY, in as many bytes as it needs (a default takes about 20 to 90, a 64-step mono pattern about 210), and the slots of an object
 * share its 3576 bytes. SAVE says PATTERN LIST FULL, and changes nothing, when a pattern does not fit its object (about eleven patterns
 * of 64 steps with chords in one object; never with ordinary patterns).
 *   object  magic "PTS3" (u32), used (u16: the object's length, header included), nslot (u8: the slots of this object, 11 or 10),
 *           per (u8: 11, the slots an object holds: slot = object x per + index), then nslot records one after the other
 *   record  rlen (u16: the bytes that follow), flags (u8: bit 0 CHANGED, the slot was saved by the user, ERASE gives the default back;
 *           bit 1 QUANTIZE), LEN, DIV, SWING, GATE, nlen (u8, 0..12), the name (printable ASCII 32..126, no padding),
 *           nst (u8: 0..64 steps stored, the steps after them are empty), nst compact steps
 *   step    c0 = time (bits 0..1) | accent << 2 | slide << 3 | RATCH << 4 (2 bits) | code << 6, then by code
 *           0: nothing (no note, velocity 0, chance 0, nudge 0)   1: note, velocity (one note, nothing else)
 *           2: n | c << 3 (notes in use, note bytes stored), velocity, chance byte, nudge byte (4 bits), c notes   (3: never written)
 *           The empty step the track starts with (time REST, no flags: c0 = 2) after the last real one is not stored.
 *   The expanded form of a step is the 9-byte one of FUN11 and the multis (ps_pack_step / ps_unpack_step; the editor: stored.js).
 *
 * Where: each object has its A/B sector pair as every storage.c object (the commit record last, the newest valid copy wins: a power
 * cut during a write leaves the slots as they were). Flash 0xD6000..0xDBFFF (fm1_flash.h FL_PSLOT_*), the six sectors at the end of
 * the user sample region (storage.c's flash map), just below the user preset banks. The object ids and sectors are those of the
 * 16-slot format. A payload ends at 3584 + 256 = 0xF00: a sector's tail from 0xF00 stays erased (where the update loader and the
 * SPL look for update records, as fm6_vbank.c keeps it). The stock firmware's "USR" region (0xEA000..0xFBFFF) is never written.
 *
 * RAM: a mirror of the three objects (.pool, the same 3 x 3584 bytes as the 16-slot format), so the list never reads flash. SAVE and
 * ERASE change the mirror at once (jam: while playing too); the flash write waits until nothing plays (ps_poll, from settings_poll: no
 * erase is heard). The defaults of an object that was missing or unreadable are written the same way, once nothing plays.
 * Without FELUCCA_FLASH (host builds) the mirror is all there is. */
#define PS_PER_OBJ 11u
#define PS_NOBJ ((PS_SLOTS + PS_PER_OBJ - 1u) / PS_PER_OBJ)
#define PS_OBJ_MAX 3584u                        /* ST_PAYLOAD_END - ST_PAYLOAD_OFF */
#define PS_HDR 8u
#define PS_MAGIC 0x33535450u                    /* "PTS3" */
#define PS_NAME 12u                             /* the name's characters (the NAME screen's NM_LEN) */
#define PS_STEP 9u                              /* the expanded step (FUN11) */
#define PS_REC_MAX (2u + 1u + 4u + 1u + PS_NAME + 1u + NSTEP * PS_STEP)   /* a record at its largest: 597 bytes */
#define PS_F_CHANGED 1u
#define PS_F_QNT 2u
#if FELUCCA_FLASH
_Static_assert(PS_OBJ_MAX == ST_PAYLOAD_END - ST_PAYLOAD_OFF, "a pattern object leaves its sector's tail (0xF00..) erased");
#endif
_Static_assert(PS_NOBJ == 3u && (PS_NOBJ - 1u) * PS_PER_OBJ < PS_SLOTS, "32 slots in 3 objects: 11, 11 and 10");
_Static_assert(NPATTERNS == PS_SLOTS, "the default patterns are the content of the pattern slots, one each");
static uint8_t ps_bank[PS_NOBJ][PS_OBJ_MAX] __attribute__((aligned(4), section(".pool")));   /* (main loop only) */
static uint16_t ps_at[PS_SLOTS];                /* slot k's record: its offset in its object (kept by ps_index) */
static uint8_t ps_dirty;                        /* bit o: object o changed in RAM, not in flash yet */
static uint8_t ps_booted;                       /* ps_boot has run (a host test that never boots the stores: the first access does) */
static void ps_boot(void);

static uint32_t ps_nslot(uint32_t o) { return PS_SLOTS - o * PS_PER_OBJ > PS_PER_OBJ ? PS_PER_OBJ : PS_SLOTS - o * PS_PER_OBJ; }
static uint32_t ps_used_len(uint32_t o) { if (!ps_booted) ps_boot(); return (uint32_t)ps_bank[o][4] | (uint32_t)ps_bank[o][5] << 8; }
static void ps_set_used(uint32_t o, uint32_t n) { ps_bank[o][4] = (uint8_t)n; ps_bank[o][5] = (uint8_t)(n >> 8); }
static uint8_t *ps_rec(uint32_t k) { if (!ps_booted) ps_boot(); return &ps_bank[k / PS_PER_OBJ][ps_at[k]]; }   /* (k < PS_SLOTS) */
static uint32_t ps_rlen(const uint8_t *r) { return 2u + ((uint32_t)r[0] | (uint32_t)r[1] << 8); }   /* the record's bytes, rlen included */

/* step s -> 9 bytes (project.c proj_pack's packing: NUDGE in bit 7 of the notes, RATCH in bit 7 of VEL and CHANCE) */
static void ps_pack_step(const step_t *s, uint8_t *b)
{
    uint32_t r = step_ratchet(s) - 1u, nd = (uint32_t)step_nudge(s) & 15u, j;
    for (j = 0; j < 4u; j++)
        b[j] = (uint8_t)((s->note[j] & 127u) | (nd >> j & 1u) << 7);
    b[4] = (uint8_t)((s->n > 4u ? 4u : s->n) | (s->time > ST_REST ? ST_REST : s->time) << 3 | (s->flags & 3u) << 5);
    b[5] = (uint8_t)((s->vel & 127u) | (r & 1u) << 7);
    b[6] = 0;                                      /* (the drum lane hits and accents: gone in 0.2.14, the two bytes stay) */
    b[7] = 0;
    b[8] = (uint8_t)((s->probability > 101u ? 0u : s->probability) | (r >> 1) << 7);
}
/* 9 bytes -> step s; 0: not a step ps_pack_step writes (flash may hold anything) */
static int ps_unpack_step(const uint8_t *b, step_t *s)
{
    uint32_t nd = 0, j, r = (uint32_t)(b[5] >> 7) | (uint32_t)(b[8] >> 7) << 1;
    for (j = 0; j < 4u; j++) {
        nd |= (uint32_t)(b[j] >> 7) << j;
        s->note[j] = b[j] & 127u;
    }
    s->n = b[4] & 7u;
    s->time = (b[4] >> 3) & 3u;
    s->flags = (b[4] >> 5) & 3u;
    s->vel = b[5] & 127u;
    s->probability = b[8] & 127u;
    if ((b[4] & 128u) || s->n > 4u || s->time > ST_REST || s->probability > 101u)
        return 0;
    step_set_ratchet(s, r + 1u);
    step_set_nudge(s, (int32_t)(nd ^ 8u) - 8);
    return 1;
}

/* the compact step: b (9 bytes, as ps_pack_step writes them) -> d, the bytes written (1..9) */
static uint32_t ps_cstep(uint8_t *d, const uint8_t *b)
{
    uint32_t n = b[4] & 7u, vel = b[5] & 127u, prob = b[8] & 127u, nd = 0, c = 0, j;
    uint32_t c0 = ((b[4] >> 3) & 3u) | (uint32_t)((b[4] >> 5) & 3u) << 2 | (uint32_t)((b[5] >> 7) | (b[8] >> 7) << 1) << 4;
    for (j = 0; j < 4u; j++) {
        nd |= (uint32_t)(b[j] >> 7) << j;
        if (b[j] & 127u)
            c = j + 1u;
    }
    if (!n && !c && !vel && !prob && !nd) {                          /* code 0 */
        d[0] = (uint8_t)c0;
        return 1u;
    }
    if (n == 1u && c == 1u && !nd && !prob) {                        /* code 1 */
        d[0] = (uint8_t)(c0 | 1u << 6);
        d[1] = b[0] & 127u;
        d[2] = (uint8_t)vel;
        return 3u;
    }
    d[0] = (uint8_t)(c0 | 2u << 6);                                  /* code 2 */
    d[1] = (uint8_t)(n | c << 3);
    d[2] = (uint8_t)vel;
    d[3] = (uint8_t)prob;
    d[4] = (uint8_t)nd;
    for (j = 0; j < c; j++)
        d[5u + j] = b[j] & 127u;
    return 5u + c;
}
/* the compact step at s (before e) -> b (9 bytes); the bytes read, 0 for a step ps_cstep does not write */
static uint32_t ps_xstep(const uint8_t *s, const uint8_t *e, uint8_t *b)
{
    uint32_t c0, code, c, nd, j, len;
    if (s >= e)
        return 0;
    c0 = s[0];
    code = c0 >> 6;
    memset(b, 0, PS_STEP);
    b[4] = (uint8_t)((c0 & 3u) << 3 | (c0 >> 2 & 3u) << 5);
    b[5] = (uint8_t)((c0 >> 4 & 1u) << 7);
    b[8] = (uint8_t)((c0 >> 5 & 1u) << 7);
    if (code == 0u)
        return 1u;
    if (code == 1u) {
        if (e - s < 3 || s[1] > 127u || s[2] > 127u)
            return 0;
        b[0] = s[1];
        b[4] |= 1u;
        b[5] |= s[2];
        return 3u;
    }
    if (code != 2u || e - s < 5)
        return 0;
    c = s[1] >> 3;
    if ((s[1] & 7u) > 4u || c > 4u || (s[1] & 0xC0u) || s[2] > 127u || s[3] > 101u || s[4] > 15u)
        return 0;
    len = 5u + c;
    if ((uint32_t)(e - s) < len)
        return 0;
    nd = s[4];
    b[4] |= (uint8_t)(s[1] & 7u);
    b[5] |= s[2];
    b[8] |= s[3];
    for (j = 0; j < c; j++) {
        if (s[5u + j] > 127u)
            return 0;
        b[j] = s[5u + j];
    }
    for (j = 0; j < 4u; j++)
        b[j] |= (uint8_t)((nd >> j & 1u) << 7);
    return len;
}

typedef void (*ps_get_f)(const void *ctx, uint32_t i, uint8_t *b9);   /* step i of the source, as 9 bytes */
/* the record of a pattern source into d (PS_REC_MAX bytes at most): the bytes written */
static uint32_t ps_enc(uint8_t *d, uint32_t fl, uint32_t len, uint32_t div, uint32_t swing, uint32_t gate, const char *name,
                       ps_get_f get, const void *ctx)
{
    uint32_t i, nl = 0, at, last;
    uint8_t b[PS_STEP], cs[PS_STEP];
    while (name && nl < PS_NAME && name[nl])
        nl++;
    d[2] = (uint8_t)fl;
    d[3] = (uint8_t)len;
    d[4] = (uint8_t)div;
    d[5] = (uint8_t)swing;
    d[6] = (uint8_t)gate;
    d[7] = (uint8_t)nl;
    for (i = 0; i < nl; i++)
        d[8u + i] = (uint8_t)name[i];
    at = 8u + nl;
    for (i = 0, last = 0; i < NSTEP; i++) {                          /* the steps up to the last one that is not empty */
        get(ctx, i, b);
        if (ps_cstep(cs, b) != 1u || cs[0] != (uint8_t)ST_REST)    /* (c0 = ST_REST: time REST, no flags: the empty step) */
            last = i + 1u;
    }
    d[at++] = (uint8_t)last;
    for (i = 0; i < last; i++) {
        get(ctx, i, b);
        at += ps_cstep(d + at, b);
    }
    d[0] = (uint8_t)(at - 2u);
    d[1] = (uint8_t)((at - 2u) >> 8);
    return at;
}

/* a record at r (before e): its bytes, 0 when it is not one ps_enc writes (ranges, names, steps) */
static uint32_t ps_rec_ok(const uint8_t *r, const uint8_t *e)
{
    uint32_t n, nl, nst, i, k;
    const uint8_t *p, *end;
    uint8_t b[PS_STEP];
    step_t s;
    if (e - r < 9)
        return 0;
    n = ps_rlen(r);
    if ((uint32_t)(e - r) < n || n < 9u)
        return 0;
    end = r + n;
    nl = r[7];
    if (r[2] > 3u || r[3] < TP[P_SLEN].min || r[3] > TP[P_SLEN].max || r[4] > TP[P_SDIV].max || r[5] > TP[P_SSWING].max ||
        r[6] < TP[P_SGATE].min || r[6] > TP[P_SGATE].max || nl > PS_NAME || n < 9u + nl)
        return 0;
    for (i = 0; i < nl; i++)
        if (r[8u + i] < 32u || r[8u + i] > 126u)
            return 0;
    nst = r[8u + nl];
    if (nst > NSTEP)
        return 0;
    p = r + 9u + nl;
    for (i = 0; i < nst; i++) {
        if (!(k = ps_xstep(p, end, b)) || !ps_unpack_step(b, &s))
            return 0;
        p += k;
    }
    return p == end ? n : 0;
}

/* the offsets of the records of object o (the object is valid) */
static void ps_index(uint32_t o)
{
    uint32_t i, at = PS_HDR;
    for (i = 0; i < ps_nslot(o); i++) {
        ps_at[o * PS_PER_OBJ + i] = (uint16_t)at;
        at += ps_rlen(&ps_bank[o][at]);
    }
}
/* object o as b (len bytes): the shape ps_obj_check wants (a PUT of the editor's backup restore is checked with it too) */
static int ps_obj_ok(uint32_t o, const uint8_t *b, uint32_t len)
{
    uint32_t i, at = PS_HDR, n;
    if (len < PS_HDR || len > PS_OBJ_MAX || b[0] != 'P' || b[1] != 'T' || b[2] != 'S' || b[3] != '3' ||
        ((uint32_t)b[4] | (uint32_t)b[5] << 8) != len || b[6] != ps_nslot(o) || b[7] != PS_PER_OBJ)
        return 0;
    for (i = 0; i < b[6]; i++) {
        if (!(n = ps_rec_ok(b + at, b + len)))
            return 0;
        at += n;
    }
    return at == len;
}

/* ---- the defaults: engines.c PATTERNS[k] as a source of steps */
static void ps_def_get(const void *ctx, uint32_t i, uint8_t *b9)
{
    uint32_t k = *(const uint32_t *)ctx;
    step_t s;
    if (i < NPAT_STEPS)
        pat_make_step(&s, PATTERNS[k].note[i], PATTERNS[k].flags[i]);
    else
        pat_make_step(&s, 0, 0);
    ps_pack_step(&s, b9);
}
static uint32_t ps_def_rec(uint8_t *d, uint32_t k)
{
    return ps_enc(d, TP[P_SQNT].def ? PS_F_QNT : 0u, PATTERNS[k].len, PATTERNS[k].div, (uint32_t)TP[P_SSWING].def,
                  (uint32_t)TP[P_SGATE].def, PATTERNS[k].name, ps_def_get, &k);
}

static void ps_touch(uint32_t o) { ps_dirty |= (uint8_t)(1u << o); }

static void ps_reset_obj(uint32_t o)            /* all the defaults of object o (to be written when nothing plays) */
{
    uint8_t r[PS_REC_MAX];
    uint32_t i, at = PS_HDR;
    memset(ps_bank[o], 0, PS_OBJ_MAX);
    for (i = 0; i < ps_nslot(o); i++) {
        uint32_t n = ps_def_rec(r, o * PS_PER_OBJ + i);
        memcpy(&ps_bank[o][at], r, n);
        at += n;
    }
    ps_bank[o][0] = 'P'; ps_bank[o][1] = 'T'; ps_bank[o][2] = 'S'; ps_bank[o][3] = '3';
    ps_bank[o][6] = (uint8_t)ps_nslot(o);
    ps_bank[o][7] = PS_PER_OBJ;
    ps_set_used(o, at);
    ps_index(o);
    ps_touch(o);
}

static void ps_obj_check(uint32_t o, int len)   /* after a load: a wrong shape is the defaults, each record checked */
{
    if (len <= 0 || !ps_obj_ok(o, ps_bank[o], (uint32_t)len))
        ps_reset_obj(o);
    else
        ps_index(o);
}

static void ps_boot(void)                       /* persist_boot (up_boot): the slots from flash, else the defaults */
{
    uint32_t o;
    ps_dirty = 0;
    ps_booted = 1;
    for (o = 0; o < PS_NOBJ; o++) {
#if FELUCCA_FLASH
        ps_obj_check(o, flash_ok ? st_load(OBJ_PATSLOT0 + o, ps_bank[o], PS_OBJ_MAX) : -1);
#else
        ps_obj_check(o, -1);
#endif
    }
}

static int ps_changed(uint32_t k) { return k < PS_SLOTS && (ps_rec(k)[2] & PS_F_CHANGED); }   /* saved by the user: ERASE gives the default back */
static uint32_t ps_len(uint32_t k) { return k < PS_SLOTS ? ps_rec(k)[3] : 0u; }
static void ps_name(uint32_t k, char *b)        /* slot k's stored name (b: 13 bytes; "" none) */
{
    uint32_t i, n = k < PS_SLOTS ? ps_rec(k)[7] : 0u;
    for (i = 0; i < n; i++)
        b[i] = (char)ps_rec(k)[8u + i];
    b[i] = 0;
}

/* record r (n bytes) into slot k: its place in its object. 0 ok, 2 does not fit (nothing changed) */
static int ps_splice(uint32_t k, const uint8_t *r, uint32_t n)
{
    uint32_t o = k / PS_PER_OBJ, old = ps_rlen(ps_rec(k)), used = ps_used_len(o), at = ps_at[k];
    if (used - old + n > PS_OBJ_MAX)
        return 2;
    {                                           /* the records after slot k move by n - old bytes (the device's libc has no memmove) */
        uint8_t *b = ps_bank[o];
        uint32_t i, tail = used - at - old;
        if (n > old)
            for (i = tail; i-- > 0;)
                b[at + n + i] = b[at + old + i];
        else if (n < old)
            for (i = 0; i < tail; i++)
                b[at + n + i] = b[at + old + i];
    }
    memcpy(&ps_bank[o][at], r, n);
    ps_set_used(o, used - old + n);
    ps_index(o);
    ps_touch(o);
    return 0;
}

static void ps_trk_get(const void *ctx, uint32_t i, uint8_t *b9) { ps_pack_step(&((const track_t *)ctx)->step[i], b9); }

/* track t's pattern -> slot k under name (at most 12 characters, 0 none) (RAM now, flash at ps_poll). 0 ok, 1 bad slot, 2 no room */
static int ps_save_trk(const track_t *t, uint32_t k, const char *name)
{
    uint8_t r[PS_REC_MAX];
    uint32_t n, i;
    char nm[PS_NAME + 1];
    if (k >= PS_SLOTS)
        return 1;
    for (i = 0; name && i < PS_NAME && name[i] >= 32 && name[i] < 127; i++)
        nm[i] = name[i];
    nm[i] = 0;
    n = ps_enc(r, PS_F_CHANGED | (t->p[P_SQNT] != 0 ? PS_F_QNT : 0u), (uint32_t)clamp(t->p[P_SLEN], TP[P_SLEN].min, TP[P_SLEN].max),
               (uint32_t)clamp(t->p[P_SDIV], TP[P_SDIV].min, TP[P_SDIV].max), (uint32_t)clamp(t->p[P_SSWING], TP[P_SSWING].min, TP[P_SSWING].max),
               (uint32_t)clamp(t->p[P_SGATE], TP[P_SGATE].min, TP[P_SGATE].max), nm, ps_trk_get, t);
    return ps_splice(k, r, n);
}

/* ERASE: slot k gets its default back. 0 ok, 1 bad slot or it holds its default already, 2 no room (the default is longer than what
 * is there and the object is full of the neighbours' patterns: nothing changed) */
static int ps_erase(uint32_t k)
{
    uint8_t r[PS_REC_MAX];
    if (k >= PS_SLOTS || !ps_changed(k))
        return 1;
    return ps_splice(k, r, ps_def_rec(r, k));
}

/* slot k -> track t's steps (all 64) and LEN DIV SWING GATE QUANTIZE; the sound stays (ui.c pat_load: the undo copy). 0 ok, 1 bad slot */
static int ps_load_trk(track_t *t, uint32_t k)
{
    const uint8_t *r, *p, *end;
    uint32_t i, nl, nst;
    uint8_t b[PS_STEP];
    if (k >= PS_SLOTS)
        return 1;
    r = ps_rec(k);
    nl = r[7];
    nst = r[8u + nl];
    p = r + 9u + nl;
    end = r + ps_rlen(r);
    for (i = 0; i < NSTEP; i++) {
        if (i < nst)
            p += ps_xstep(p, end, b);
        else {
            memset(b, 0, sizeof b);
            b[4] = (uint8_t)(ST_REST << 3);
        }
        ps_unpack_step(b, &t->step[i]);
    }
    t->p[P_SLEN] = r[3];
    t->p[P_SDIV] = r[4];
    t->p[P_SSWING] = r[5];
    t->p[P_SGATE] = r[6];
    t->p[P_SQNT] = (r[2] & PS_F_QNT) != 0;
    return 0;
}

static void ps_label(char *b, uint32_t k)       /* "P01" .. "P32" (b: 4 bytes) */
{
    b[0] = 'P';
    b[1] = (char)('0' + (k + 1u) / 10u);
    b[2] = (char)('0' + (k + 1u) % 10u);
    b[3] = 0;
}

/* main loop (settings_poll): a changed object to flash once nothing plays (quiet: an erase stops the audio ~40 ms).
 * One object a pass; a failed write is tried again at the next quiet pass */
#if FELUCCA_FLASH
static int autosave_quiet(void);
static uint32_t ps_retry_ms;
static void ps_poll(void)
{
    uint32_t o;
    if (!ps_dirty || !flash_ok || (ps_retry_ms && (uint32_t)(fm1_ms - ps_retry_ms) < 1000u) || !autosave_quiet())
        return;
    for (o = 0; o < PS_NOBJ && !((ps_dirty >> o) & 1u); o++)
        ;
    if (o >= PS_NOBJ)
        return;
    if (st_save(OBJ_PATSLOT0 + o, ps_bank[o], ps_used_len(o)) == 0) {
        ps_dirty &= (uint8_t)~(1u << o);
        ps_retry_ms = 0;
    } else {
        ps_retry_ms = fm1_ms | 1u;
    }
}
#endif
