/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* DUELL pattern slots P1..P16 (SAVE > PATTERNS): sixteen patterns of their own, independent of the sounds and shared by
 * both parts (any slot loads onto either part). A slot holds a part's step pattern only: all NSTEP (64) steps (notes,
 * time, accent, slide, RATCH, NUDGE, velocity, CHANCE, the hits) and the part's LEN, DIV, SWING, GATE and QUANTIZE, so
 * the pattern plays back with its own length and division. No sound, no automation.
 *
 * Format: a slot record (ps_rec_t) is 8 bytes (used, version, LEN, DIV, SWING, GATE, QUANTIZE, 0), the NAME (12 characters,
 * NUL-padded, no terminator needed; empty = none) and 64 steps of 9 bytes, packed as a FUN11 project packs them
 * (project.c proj_pack / proj_unpack): 8 + 12 + 576 = 596 bytes. Six records make one storage.c object (ps_obj_t,
 * 8 + 6 x 596 = 3584 bytes, within the 3840 B payload). Version 2 / magic "PTS2": data of the nameless format reads as empty: three objects, OBJ_PATSLOT0..2 (P1..P6, P7..P12, P13..P16).
 *
 * Where: each object has its A/B sector pair as every storage.c object (the commit record last, the newest valid copy
 * wins: a power cut during a write leaves the slot as it was). Flash 0xD6000..0xDBFFF (fm1_flash.h FL_PSLOT_*), the six
 * sectors at the end of the user sample region (storage.c's flash map; below them the MULTI store's second range, then USR3's
 * end; the sample slots are retired since DUELL 0.2.9),
 * just below the user preset banks. The stock firmware's "USR" region (0xEA000..0xFBFFF, tools/fm1pkg_make.py) is never
 * written. A payload ends at 0xEB8 < 0xF00: a sector's tail from 0xF00 stays erased (where the update loader and the SPL
 * look for update records, as fm6_vbank.c keeps it). Firmware without slots never reads these sectors; a stock firmware
 * that writes there leaves no valid commit record of ours (CRC): the slots read as empty.
 *
 * RAM: a mirror of the three objects (.pool), so the list never reads flash. SAVE and ERASE change the mirror at once
 * (jam: while playing too); the flash write waits until nothing plays (ps_poll, from settings_poll: no erase is heard).
 * Without FELUCCA_FLASH (host builds) the mirror is all there is. */
#define PS_PER_OBJ 6u
#define PS_NOBJ ((PS_SLOTS + PS_PER_OBJ - 1u) / PS_PER_OBJ)
#define PS_USED 0xA7u
#define PS_VER 2u
#define PS_MAGIC 0x32535450u                    /* "PTS2" */
#define PS_STEP 9u
#define PS_NAME 12u                             /* the name's characters (the NAME screen's NM_LEN) */
typedef struct {
    uint8_t used, ver;                          /* PS_USED, PS_VER */
    uint8_t len, div, swing, gate, qnt, rsv;    /* P_SLEN P_SDIV P_SSWING P_SGATE P_SQNT */
    char name[PS_NAME];                         /* the user's name, NUL-padded */
    uint8_t step[NSTEP][PS_STEP];
} ps_rec_t;
typedef struct {
    uint32_t magic;
    uint16_t rsize, nslot;
    ps_rec_t r[PS_PER_OBJ];
} ps_obj_t;
_Static_assert(sizeof(ps_rec_t) == 596u && sizeof(ps_obj_t) == 3584u, "pattern slot layout");
_Static_assert(256u + sizeof(ps_obj_t) <= 0xF00u, "a pattern slot object leaves its sector's tail (0xF00..) erased");
static ps_obj_t ps_bank[PS_NOBJ] __attribute__((section(".pool")));   /* (main loop only) */
static uint8_t ps_dirty;                        /* bit o: object o changed in RAM, not in flash yet */

static ps_rec_t *ps_rec(uint32_t k) { return &ps_bank[k / PS_PER_OBJ].r[k % PS_PER_OBJ]; }

/* step s -> 9 bytes (project.c proj_pack's packing: NUDGE in bit 7 of the notes, RATCH in bit 7 of VEL and CHANCE) */
static void ps_pack_step(const step_t *s, uint8_t *b)
{
    uint32_t r = step_ratchet(s) - 1u, nd = (uint32_t)step_nudge(s) & 15u, j;
    for (j = 0; j < 4u; j++)
        b[j] = (uint8_t)((s->note[j] & 127u) | (nd >> j & 1u) << 7);
    b[4] = (uint8_t)((s->n > 4u ? 4u : s->n) | (s->time > ST_REST ? ST_REST : s->time) << 3 | (s->flags & 3u) << 5);
    b[5] = (uint8_t)((s->vel & 127u) | (r & 1u) << 7);
    b[6] = s->hit;
    b[7] = s->acc & s->hit;
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
    s->hit = b[6];
    s->acc = b[7];
    s->probability = b[8] & 127u;
    if ((b[4] & 128u) || s->n > 4u || s->time > ST_REST || s->probability > 101u || (s->acc & ~s->hit))
        return 0;
    step_set_ratchet(s, r + 1u);
    step_set_nudge(s, (int32_t)(nd ^ 8u) - 8);
    return 1;
}

static int ps_valid(const ps_rec_t *r)
{
    uint32_t i;
    step_t s;
    if (r->used != PS_USED || r->ver != PS_VER || r->len < TP[P_SLEN].min || r->len > TP[P_SLEN].max ||
        r->div > TP[P_SDIV].max || r->swing > TP[P_SSWING].max || r->gate < TP[P_SGATE].min ||
        r->gate > TP[P_SGATE].max || r->qnt > 1u)
        return 0;
    for (i = 0; i < NSTEP; i++)
        if (!ps_unpack_step(r->step[i], &s))
            return 0;
    return 1;
}
static uint8_t ps_ok[PS_SLOTS];                 /* ps_valid, kept (the list asks every frame) */

static int ps_used(uint32_t k) { return k < PS_SLOTS && ps_ok[k]; }
static uint32_t ps_len(uint32_t k) { return ps_used(k) ? ps_rec(k)->len : 0u; }
static void ps_name(uint32_t k, char *b)        /* slot k's stored name (b: 13 bytes; "" none or an unused slot) */
{
    uint32_t i;
    for (i = 0; i < PS_NAME; i++)
        b[i] = ps_used(k) && ps_rec(k)->name[i] >= ' ' && ps_rec(k)->name[i] < 127 ? ps_rec(k)->name[i] : 0;
    for (i = 0; i < PS_NAME && b[i]; i++)
        ;
    b[i] = 0;
}

static void ps_obj_check(uint32_t o, int len)   /* after a load: a wrong shape is empty; each record checked */
{
    ps_obj_t *b = &ps_bank[o];
    uint32_t i, k;
    if (len != (int)sizeof *b || b->magic != PS_MAGIC || b->rsize != sizeof(ps_rec_t) || b->nslot != PS_PER_OBJ)
        memset(b, 0, sizeof *b);
    for (i = 0; i < PS_PER_OBJ; i++)
        if ((k = o * PS_PER_OBJ + i) < PS_SLOTS)
            ps_ok[k] = (uint8_t)ps_valid(&b->r[i]);
}

static void ps_boot(void)                       /* persist_boot (up_boot): the slots from flash */
{
    uint32_t o;
    for (o = 0; o < PS_NOBJ; o++) {
#if FELUCCA_FLASH
        ps_obj_check(o, flash_ok ? st_load(OBJ_PATSLOT0 + o, &ps_bank[o], sizeof ps_bank[o]) : -1);
#else
        ps_obj_check(o, -1);
#endif
    }
    ps_dirty = 0;
}

static void ps_touch(uint32_t k)
{
    ps_obj_t *b = &ps_bank[k / PS_PER_OBJ];
    b->magic = PS_MAGIC;
    b->rsize = sizeof(ps_rec_t);
    b->nslot = PS_PER_OBJ;
    ps_ok[k] = (uint8_t)ps_valid(ps_rec(k));
    ps_dirty |= (uint8_t)(1u << (k / PS_PER_OBJ));
}

/* track t's pattern -> slot k under name (at most 12 characters, 0 none) (RAM now, flash at ps_poll). 0 ok, 1 bad slot */
static int ps_save_trk(const track_t *t, uint32_t k, const char *name)
{
    ps_rec_t *r;
    uint32_t i;
    if (k >= PS_SLOTS)
        return 1;
    r = ps_rec(k);
    memset(r, 0, sizeof *r);
    r->used = PS_USED;
    r->ver = PS_VER;
    r->len = (uint8_t)clamp(t->p[P_SLEN], TP[P_SLEN].min, TP[P_SLEN].max);
    r->div = (uint8_t)clamp(t->p[P_SDIV], TP[P_SDIV].min, TP[P_SDIV].max);
    r->swing = (uint8_t)clamp(t->p[P_SSWING], TP[P_SSWING].min, TP[P_SSWING].max);
    r->gate = (uint8_t)clamp(t->p[P_SGATE], TP[P_SGATE].min, TP[P_SGATE].max);
    r->qnt = t->p[P_SQNT] != 0;
    for (i = 0; name && i < PS_NAME && name[i]; i++)
        r->name[i] = name[i];
    for (i = 0; i < NSTEP; i++)
        ps_pack_step(&t->step[i], r->step[i]);
    ps_touch(k);
    return 0;
}

static int ps_erase(uint32_t k)                 /* 0 ok, 1 bad or empty slot */
{
    if (!ps_used(k))
        return 1;
    memset(ps_rec(k), 0, sizeof(ps_rec_t));
    ps_touch(k);
    return 0;
}

/* slot k -> track t's steps (all 64) and LEN DIV SWING GATE QUANTIZE; the sound stays (ui.c pat_load: the undo copy).
 * 0 ok, 1 empty */
static int ps_load_trk(track_t *t, uint32_t k)
{
    const ps_rec_t *r;
    uint32_t i;
    if (!ps_used(k))
        return 1;
    r = ps_rec(k);
    for (i = 0; i < NSTEP; i++)
        ps_unpack_step(r->step[i], &t->step[i]);
    t->p[P_SLEN] = r->len;
    t->p[P_SDIV] = r->div;
    t->p[P_SSWING] = r->swing;
    t->p[P_SGATE] = r->gate;
    t->p[P_SQNT] = r->qnt;
    return 0;
}

static void ps_label(char *b, uint32_t k)       /* "P01" .. "P16" (b: 4 bytes) */
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
    if (st_save(OBJ_PATSLOT0 + o, &ps_bank[o], sizeof ps_bank[o]) == 0) {
        ps_dirty &= (uint8_t)~(1u << o);
        ps_retry_ms = 0;
    } else {
        ps_retry_ms = fm1_ms | 1u;
    }
}
#endif
