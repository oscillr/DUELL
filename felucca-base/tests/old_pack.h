/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* A project as firmware before 1.2 wrote it (the host tests' old images; included after src/project.c). DUELL 0.2.14 reads none of
 * these any more: the tests use the images to show that they are refused (project_test.c, step3_test.c, fun10_test.c, ratchet_test.c):
 *   FUN9 (1.1: np 99, 3648 bytes), FUN8 (1.0.x: np 91, 3584), FUN7 (np as given, 3388, no patches)
 * byte for byte as their proj_pack: np parameters mapped by count (the first np - 8 are today's ids 0.., the last 8
 * P_E0..P_E7), 9-byte steps without a nudge (bit 7 of the note bytes 0), the motion block of 260 bytes (count <= 64,
 * 64 x (place, id, i16 value); the ids from P_E0 on at that store's own: id - (P_COUNT - np), a lock's bit kept), the
 * patches at name - 512 (not FUN7), the name, the FNV-1a hash. -> its size, 0 when q does not fit the old format.
 * The song's chain as rows {slot, repeat} (chain_v9_t, 36 bytes): a section with its four tracks on one slot each (a
 * section of several slots or a "-" does not fit) */
#define CHAIN_ROWS 16u   /* (the song chain: gone from the firmware; the old images still carry an empty one) */
typedef struct { uint8_t slot, repeat; } chain_row_v9_t;   /* the rows of FUN6..FUN9 (core.h had them until 0.2.14) */
typedef struct { uint8_t count, rsv[3]; chain_row_v9_t row[CHAIN_ROWS]; } chain_v9_t;
#define OLD_FUN9 0x46554E39u
#define OLD_FUN8 0x46554E38u
#define OLD_FUN7 0x46554E37u
static uint32_t old_pack(uint8_t *b, const project_t *q, uint32_t magic, uint32_t np)
{
    uint32_t st = magic == OLD_FUN9 ? 3648u : magic == OLD_FUN8 ? 3584u : 3388u, pos = 68u, t, i, sum;
    uint32_t name = st - 16u;
    if (np < 8u || np > P_COUNT || q->motion.count > 64u)
        return 0;
    memset(b, 0, st);
    memcpy(b, &magic, 4);
    memcpy(b + 4, &st, 4);
    memcpy(b + 8, q->g, sizeof q->g);
    b[62] = q->sel; b[63] = q->parts; b[64] = q->phys; b[66] = (uint8_t)np;
    for (t = 0; t < NTRK; t++) {
        for (i = 0; i < np; i++) {
            uint32_t id = i < np - 8u ? i : P_E0 + i - (np - 8u);
            b[pos++] = (uint8_t)(q->t[t].p[id] + 64);
        }
        b[pos++] = q->t[t].engine;
        b[pos++] = q->t[t].preset;
        for (i = 0; i < NSTEP; i++) {
            const step_t *s = &q->t[t].step[i];
            uint32_t r = step_ratchet(s) - 1u, j;
            for (j = 0; j < 4u; j++)
                b[pos++] = s->note[j] & 127u;
            b[pos++] = (uint8_t)(s->n | s->time << 3 | (s->flags & 3u) << 5);
            b[pos++] = (uint8_t)((s->vel & 127u) | (r & 1u) << 7);
            b[pos++] = 0;                                  /* (the drum lane hits and accents: gone in 0.2.14, the bytes stay) */
            b[pos++] = 0;
            b[pos++] = (uint8_t)(s->probability | (r >> 1) << 7);
        }
    }
    {
        chain_v9_t c9;
        memset(&c9, 0, sizeof c9);
        for (i = 0; i < CHAIN_ROWS; i++)                   /* (an empty chain: no section, every repeat 1) */
            c9.row[i].repeat = 1;
        memcpy(b + pos, &c9, sizeof c9);
        pos += sizeof c9;
    }
    b[pos] = q->motion.count;
    b[pos + 1u] = q->motion.on;
    for (i = 0; i < q->motion.count; i++) {
        const motion_event_t *e = &q->motion.event[i];
        uint32_t id = MOTION_ID(e);
        int16_t v = e->value;
        if (id >= P_E0)
            id -= P_COUNT - np;
        b[pos + 4u + 4u * i] = e->place;
        b[pos + 5u + 4u * i] = (uint8_t)(id | (e->param & MOTION_LOCK));
        memcpy(b + pos + 6u + 4u * i, &v, 2);
    }
    pos += 260u;
    if (magic != OLD_FUN7) {
        if (pos > name - NTRK * FM6_PACKED)
            return 0;
        memcpy(b + name - NTRK * FM6_PACKED, q->fm6, NTRK * FM6_PACKED);
    } else if (pos > name) {
        return 0;
    }
    for (i = 0; i < PROJ_NAME_LEN && q->name[i]; i++)
        b[name + i] = (uint8_t)q->name[i];
    sum = proj_hash(b, st - 4u);
    memcpy(b + st - 4u, &sum, 4);
    return st;
}

/* FUN10 as Felucca 1.2 .. 1.5 wrote it ("FUNA", 3840 bytes; DUELL writes FUN11): four tracks of np parameters (Felucca's
 * layout: ids 8, 61..80, 83..90 as core.h PL_* says, q holds them so), 9-byte steps with the nudge in bit 7 of the note
 * bytes, the 84-byte chain, the 388-byte motion block (ids from P_E0 on at that store's own), the four patches at
 * name - 512, the name, the hash. -> 3840, 0 when q does not fit */
#define OLD_FUN10 0x46554E41u
static uint32_t old_pack10(uint8_t *b, const project_t *q, uint32_t np)
{
    uint32_t st = 3840u, pos = 68u, t, i, sum, magic = OLD_FUN10, name = st - 16u, fm6 = name - NTRK * FM6_PACKED;
    motion_store_t m = q->motion;
    if (np < 8u || np > P_COUNT)
        return 0;
    memset(b, 0, st);
    memcpy(b, &magic, 4);
    memcpy(b + 4, &st, 4);
    memcpy(b + 8, q->g, sizeof q->g);
    b[62] = q->sel; b[63] = q->parts; b[64] = q->phys; b[66] = (uint8_t)np;
    for (t = 0; t < NTRK; t++) {
        for (i = 0; i < np; i++) {
            uint32_t id = i < np - 8u ? i : P_E0 + i - (np - 8u);
            b[pos++] = (uint8_t)(q->t[t].p[id] + 64);
        }
        b[pos++] = q->t[t].engine;
        b[pos++] = q->t[t].preset;
        for (i = 0; i < NSTEP; i++) {
            const step_t *s = &q->t[t].step[i];
            uint32_t r = step_ratchet(s) - 1u, nd = (uint32_t)step_nudge(s) & 15u, j;
            for (j = 0; j < 4u; j++)
                b[pos++] = (uint8_t)((s->note[j] & 127u) | (nd >> j & 1u) << 7);
            b[pos++] = (uint8_t)(s->n | s->time << 3 | (s->flags & 3u) << 5);
            b[pos++] = (uint8_t)((s->vel & 127u) | (r & 1u) << 7);
            b[pos++] = 0;                                  /* (the drum lane hits and accents: gone in 0.2.14, the bytes stay) */
            b[pos++] = 0;
            b[pos++] = (uint8_t)(s->probability | (r >> 1) << 7);
        }
    }
    for (i = 0; i < m.count && i < MOTION_MAX; i++)
        if (MOTION_ID(&m.event[i]) >= P_E0)
            m.event[i].param = (uint8_t)((m.event[i].param & MOTION_LOCK) | (MOTION_ID(&m.event[i]) - (P_COUNT - np)));
    for (i = 0; i < CHAIN_ROWS; i++)                       /* the 84-byte chain: empty (count 0, rsv 3, 16 rows of 4 slots and a repeat 1) */
        b[pos + 4u + i * 5u + 4u] = 1;
    pos += 84u;
    memcpy(b + pos, &m, sizeof m);
    pos += sizeof m;
    if (pos > fm6)
        return 0;
    memcpy(b + fm6, q->fm6, NTRK * FM6_PACKED);
    for (i = 0; i < PROJ_NAME_LEN && q->name[i]; i++)
        b[name + i] = (uint8_t)q->name[i];
    sum = proj_hash(b, st - 4u);
    memcpy(b + st - 4u, &sum, 4);
    return st;
}
