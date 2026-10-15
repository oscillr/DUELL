/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Projects: the music as one record. DUELL: the four PROJECT slots are gone; SAVE > MULTI's sixteen multis (multi.c, its own
 * compact record, two per storage object) took their flash. The project format below is kept for the autosave (OBJ_AUTOSAVE)
 * and the editor's backup / restore of the music (editor_backup.c id 0); project_capture and project_restore_runtime
 * are shared with the multis.
 *
 * Format 11 ("FUNB", FUN11): 3840 bytes (a whole storage object, storage.c ST_PAYLOAD_MAX), little endian, parts A and B only
 * (the NSHOWN shown parts, tracks 1..2). From byte 0: the magic, the size, the 27 globals (int16), the selected part (62), NPART (63),
 * PROJ_PHYS (64), BASS MONO (65: 0 OFF, 1..11 the corner; 0.2.19, was reserved), P_COUNT (66), the revision PROJ_REV (67); from byte 68 per part its P_COUNT values (+64, a byte
 * each), engine, preset and 64 steps of 9 bytes (notes with the NUDGE in bit 7, time / count / flags, velocity with RATCH bit 0, two
 * reserved bytes written 0 and ignored, chance with RATCH bit 1); then the song chain (84 bytes, an empty one: DUELL has no song
 * mode, it is checked and ignored), the motion (388 bytes: 128 records, parts A and B only), the parts' FM6 patches (PROJ_FM6_OFF,
 * NSHOWN x 128 just before the name), the 12-byte name and the FNV-1a hash. 68 + 2 x (127 + 2 + 576) + 84 + 388 = 1950 of the 3568
 * bytes before the patches (the _Static_assert at proj_pack keeps it inside). Tracks 3..4 (no screen, voice or channel in DUELL)
 * load with their power-on sound (proj_trk_default).
 *
 * DUELL 0.2.14: a project loads only if it is exactly this format: the magic, the hash, P_COUNT (127) at byte 66 and PROJ_REV 3 at
 * byte 67 (CRUSH's BITS in INS A on the curve of ins_bits.h; the delay MODE of 0.2.16; 0.2.19's MIXER LEVEL and PATCH TRIM) must all match, NPART and PROJ_PHYS at bytes 63 / 64. Anything else, in
 * particular a project of firmware before 0.2.14 (FUN1 .. FUN10 of Felucca, a FUN11 of DUELL up to 0.2.13: revision 0, fewer
 * parameters), is no project: proj_import returns 0, the autosave is ignored at power-on, a backup restore is refused (rc 2) and
 * nothing is changed. Their readers (the formats' conversions, the mapping by parameter count, the CRUSH BITS and multimode filter
 * migrations) are gone.
 *
 * Built on the Mac too (tests/project_test.c, -DPROJ_HOST): the part above the #ifndef
 * PROJ_HOST needs core.h, params.c (TP) and engines.c. */
#define PROJ_MAGIC 0x46554E42u                 /* "FUNB" (FUN11, DUELL): parts A and B only */
#define PROJ_PHYS 2u                           /* project_t.phys, byte 64: always written 2, a project that says otherwise is not one */
#define PROJ_REV 3u                            /* byte 67: 3 = DUELL 0.2.19's level split (P_LEVEL is the MIXER level, default 112 = unity; P_TRIM, the patch's loudness, is on the id of the legacy DUCK HOLD: a project of revision 2 would read its 0 as a silent trim); 2 = DUELL 0.2.16's delay MODE (G_DMODE 0 DIGITAL, 1 PING-PONG, 2 RANDOM, 3 OCT UP; no ping-pong switch); 1 = 0.2.14's layout and CRUSH curve, 0 = before 0.2.14: both refused */
#define PROJ_NAME_LEN 12u                      /* the name: the reserved tail's end */
typedef struct {                               /* one track */
    int16_t p[P_COUNT];
    uint8_t engine, preset;
    step_t step[NSTEP];
} proj_trk_t;
typedef struct {
    uint32_t magic, size;
    int16_t g[G_COUNT];
    uint8_t sel;                               /* the selected track */
    uint8_t parts;                             /* NPART (always; a stored project that says another number is refused) */
    uint8_t phys;                              /* PROJ_PHYS (always) */
    uint8_t bm;                                /* byte 65 (was reserved, written 0): BASS MONO, 0 OFF .. BM_N (0.2.19; core.h G_BM); a larger byte reads OFF */
    proj_trk_t t[NTRK];
    chain_config_t chain;
    motion_store_t motion;
    uint8_t fm6[NTRK][FM6_PACKED];             /* each track's FM6 patch, packed (eng_fm6.c) */
    char name[PROJ_NAME_LEN];                  /* the project's name: upper-case ASCII 32..126, 0-padded; "" = none */
    uint32_t sum;
} project_t;
/* The song chain (Felucca 1.2's SONG: sections of PROJECT slots) is gone from DUELL (a jam instrument: no song mode). Its
 * block stays in the format (its size does not move): a save writes an empty one (chain_defaults: no section, so
 * any firmware that still has SONG finds nothing to play), a load checks it and ignores it */
static void chain_defaults(chain_config_t *c)
{
    uint32_t i;
    memset(c, 0, sizeof *c);
    for (i = 0; i < CHAIN_ROWS; i++)
        c->row[i].repeat = 1;
}
static int chain_valid(const chain_config_t *c)
{
    uint32_t i, k;
    if (c->count > CHAIN_ROWS)
        return 0;
    for (i = 0; i < c->count; i++) {
        for (k = 0; k < NTRK; k++)
            if (c->row[i].slot[k] > CHAIN_SILENT)
                return 0;
        if (!c->row[i].repeat || c->row[i].repeat > 16u)
            return 0;
    }
    return 1;
}
/* Serialized: params are biased bytes, steps pack n/time/flags. The reserved tail is zero and covered by the hash. The tail's last
 * 12 bytes (PROJ_NAME_OFF, just before the hash) are the project's name: ASCII 32..126 (upper case), 0-padded, all 0 = no name. */
#define PROJ_STORE_SIZE 3840u                  /* a whole storage object (storage.c ST_PAYLOAD_MAX) */
#define PROJ_NAME_OFF (PROJ_STORE_SIZE - 4u - PROJ_NAME_LEN)
#define PROJ_NTRK NSHOWN                       /* the tracks it stores (parts A and B) */
#define PROJ_FM6_OFF (PROJ_NAME_OFF - PROJ_NTRK * FM6_PACKED)   /* the patches */
#define PROJ_DEF_SOUND 0xFFu                   /* preset byte: the track's power-on sound, no steps (tracks 3..4) */
typedef union { uint32_t align; uint8_t raw[PROJ_STORE_SIZE]; } project_store_t;
_Static_assert(G_COUNT == 27u, "FUN11 globals: 27");
_Static_assert(sizeof(project_store_t) == 3840u, "FUN11 size");

static uint32_t proj_hash(const void *p, uint32_t n)   /* FNV-1a over n bytes */
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t i, s = 0x811C9DC5u;
    for (i = 0; i < n; i++)
        s = (s ^ b[i]) * 16777619u;
    return s;
}
static uint32_t proj_sum(const project_t *p) { return proj_hash(p, sizeof *p - 4u); }
static int proj_ok(const project_t *q) { return q->magic == PROJ_MAGIC && q->size == sizeof *q &&
    q->sum == proj_sum(q) && chain_valid(&q->chain) && motion_valid(&q->motion); }

/* track i of q: its power-on sound (loaded as one: PROJ_DEF_SOUND), no steps, the init FM6 patch */
static void proj_trk_default(project_t *q, uint32_t i)
{
    uint32_t k;
    memset(&q->t[i], 0, sizeof q->t[i]);
    for (k = 0; k < P_COUNT; k++)
        q->t[i].p[k] = param_desc_of(trk_def_engine(i), k)->def;
    q->t[i].engine = (uint8_t)trk_def_engine(i);
    q->t[i].preset = PROJ_DEF_SOUND;
    for (k = 0; k < NSTEP; k++)
        q->t[i].step[k].time = ST_REST;
    memcpy(q->fm6[i], FM6_INIT, FM6_PACKED);
}

/* every track's engine a number this firmware has (a retired one is: it plays silent): a project of another number
 * is not one (it never was: no engine number past the last was ever stored) */
static int proj_engines_ok(const project_t *q)
{
    uint32_t t;
    for (t = 0; t < NTRK; t++)
        if (q->t[t].engine >= NENGINES)
            return 0;
    return 1;
}
static int proj_unpack(project_t *q, const uint8_t *b, uint32_t st);
/* n bytes of a stored project -> q; 0 = not a project (any other format, size, hash or revision: nothing is changed) */
static int proj_import(project_t *q, const void *b, int n)
{
    return n == (int)PROJ_STORE_SIZE && proj_unpack(q, b, PROJ_STORE_SIZE) && proj_engines_ok(q);
}

/* 12 stored name bytes -> d (PROJ_NAME_LEN + 1): up to the first 0, upper case; a byte outside 32..126 makes it
 * no name (a damaged tail never refuses the project) */
static uint32_t proj_name_get(char *d, const uint8_t *s)   /* its length */
{
    uint32_t i;
    for (i = 0; i < PROJ_NAME_LEN && s[i]; i++) {
        if (s[i] < 32u || s[i] > 126u) { i = 0; break; }
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32u : s[i]);
    }
    d[i] = 0;
    return i;
}

/* A stable serialized schema (see the top). Parts A and B only */
_Static_assert(68u + PROJ_NTRK * (P_COUNT + 2u + NSTEP * 9u) + sizeof(chain_config_t) + sizeof(motion_store_t) <= PROJ_FM6_OFF &&
               P_COUNT <= 127u && PROJ_FM6_OFF + PROJ_NTRK * FM6_PACKED + PROJ_NAME_LEN + 4u == PROJ_STORE_SIZE,
               "FUN11: the parts' values, steps, chain and motion fit before the FM6 patches (else a save fails)");
static int proj_pack(project_store_t *out, const project_t *q)
{
    uint8_t *b = out->raw; uint32_t pos = 68u, t, i; uint32_t magic = PROJ_MAGIC, size = PROJ_STORE_SIZE, sum;
    motion_store_t mo;
    for (t = 0; t < PROJ_NTRK; t++)
        for (i = 0; i < FM6_PACKED; i++)
            if (q->fm6[t][i] > 127u) return 0;
    if (!chain_valid(&q->chain) || !motion_valid(&q->motion) || P_COUNT > 127u) return 0;
    memset(&mo, 0, sizeof mo);                          /* the motion of parts A and B (tracks 3..4 are not stored) */
    mo.on = (uint8_t)(q->motion.on & ((1u << PROJ_NTRK) - 1u));
    for (i = 0; i < q->motion.count && i < MOTION_MAX; i++)
        if ((q->motion.event[i].place >> 6) < PROJ_NTRK)
            mo.event[mo.count++] = q->motion.event[i];
    memset(out, 0, sizeof *out); memcpy(b, &magic, 4); memcpy(b + 4, &size, 4);
    memcpy(b + 8, q->g, sizeof q->g); b[62] = (uint8_t)(q->sel < PROJ_NTRK ? q->sel : 0u); b[63] = q->parts; b[64] = q->phys; b[65] = q->bm <= BM_N ? q->bm : 0u;
    b[66] = P_COUNT;
    b[67] = PROJ_REV;
    for (t = 0; t < PROJ_NTRK; t++) {
        for (i = 0; i < P_COUNT; i++) {
            if (q->t[t].p[i] < -64 || q->t[t].p[i] > 127) return 0;
            b[pos++] = (uint8_t)(q->t[t].p[i] + 64);
        }
        b[pos++] = q->t[t].engine; b[pos++] = q->t[t].preset;
        for (i = 0; i < NSTEP; i++) {
            const step_t *s = &q->t[t].step[i];
            uint32_t r = step_ratchet(s) - 1u;          /* RATCH: bit 7 of the velocity and chance bytes (see the top) */
            uint32_t nd = (uint32_t)step_nudge(s) & 15u;   /* NUDGE (FUN10): bit j in bit 7 of note j */
            if (s->n > 4u || s->time > ST_REST || (s->flags & ~(3u | SF_RATCH | SF_NUDGE)) || s->probability > 101u)
                return 0;
            for (uint32_t j = 0; j < 4u; j++)           /* what proj_unpack checks, so a saved project always loads: */
                b[pos++] = (uint8_t)((s->note[j] > 127u ? 127u : s->note[j]) | (nd >> j & 1u) << 7);   /* notes and
                                                         * velocity 0..127, accents */
            b[pos++] = (uint8_t)(s->n | s->time << 3 | (s->flags & 3u) << 5);   /* only on hits */
            b[pos++] = (uint8_t)((s->vel > 127u ? 127u : s->vel) | (r & 1u) << 7);
            b[pos++] = 0; b[pos++] = 0;                 /* (the drum lane hits and accents: gone in 0.2.14, the two bytes stay) */
            b[pos++] = (uint8_t)(s->probability | (r >> 1) << 7);
        }
    }
    if (pos + sizeof q->chain + sizeof mo > PROJ_FM6_OFF) return 0;
    memcpy(b + pos, &q->chain, sizeof q->chain); pos += sizeof q->chain;
    memcpy(b + pos, &mo, sizeof mo);
    memcpy(b + PROJ_FM6_OFF, q->fm6, PROJ_NTRK * sizeof q->fm6[0]);
    {   /* the name (0-padded; stops at the first 0) */
        char n[PROJ_NAME_LEN + 1u];
        memcpy(b + PROJ_NAME_OFF, n, proj_name_get(n, (const uint8_t *)q->name));
    }
    sum = proj_hash(b, PROJ_STORE_SIZE - 4u); memcpy(b + PROJ_STORE_SIZE - 4u, &sum, 4);
    return 1;
}
/* a stored FUN11 (st = PROJ_STORE_SIZE; PROJ_NTRK parts) -> q. 0: not exactly this firmware's format (see the top) or damaged.
 * Tracks 3..4: their power-on sound (proj_trk_default) */
static int proj_unpack(project_t *q, const uint8_t *b, uint32_t st)
{
    uint32_t pos = 68u, t, i, magic, size, sum;
    memcpy(&magic, b, 4); memcpy(&size, b + 4, 4); memcpy(&sum, b + st - 4u, 4);
    if (st != PROJ_STORE_SIZE || magic != PROJ_MAGIC || size != st || sum != proj_hash(b, st - 4u) ||
        b[66] != P_COUNT || b[67] != PROJ_REV || b[63] != NPART || b[64] != PROJ_PHYS)
        return 0;
    memset(q, 0, sizeof *q); q->magic = PROJ_MAGIC; q->size = sizeof *q;
    memcpy(q->g, b + 8, sizeof q->g); q->sel = b[62]; q->parts = b[63]; q->phys = b[64]; q->bm = b[65] <= BM_N ? b[65] : 0u;
    if (q->sel >= PROJ_NTRK)
        q->sel = 0;
    for (t = PROJ_NTRK; t < NTRK; t++)
        proj_trk_default(q, t);
    for (t = 0; t < PROJ_NTRK; t++) {
        for (i = 0; i < P_COUNT; i++) { if (b[pos] > 191u) return 0; q->t[t].p[i] = (int16_t)b[pos++] - 64; }
        q->t[t].engine = b[pos++]; q->t[t].preset = b[pos++];
        if (q->t[t].engine >= NENGINES) return 0;
        for (i = 0; i < NSTEP; i++) {
            step_t *s = &q->t[t].step[i]; uint32_t meta, nd = 0;
            for (uint32_t j = 0; j < 4u; j++) {         /* (bit 7 of note j is bit j of the NUDGE) */
                nd |= (uint32_t)(b[pos] >> 7) << j;
                s->note[j] = b[pos++] & 127u;
            }
            meta = b[pos++];
            s->n = meta & 7u; s->time = (meta >> 3) & 3u; s->flags = (meta >> 5) & 3u;
            s->vel = b[pos] & 127u; meta |= (b[pos++] & 128u) << 1;      /* (RATCH bit 0 at bit 8 of meta) */
            pos += 2u;                                  /* (two reserved bytes, written 0: ignored) */
            s->probability = b[pos] & 127u; meta |= (b[pos++] & 128u) << 2;   /* (bit 1 at bit 9) */
            if ((meta & 255u) > 127u || s->n > 4u || s->time > ST_REST || s->probability > 101u) return 0;
            step_set_ratchet(s, (meta >> 8) + 1u);
            step_set_nudge(s, (int32_t)(nd ^ 8u) - 8);
        }
    }
    memcpy(&q->chain, b + pos, sizeof q->chain); pos += sizeof q->chain;
    memcpy(&q->motion, b + pos, sizeof q->motion);
    if (!chain_valid(&q->chain) || !motion_valid(&q->motion) || (q->motion.on >> PROJ_NTRK)) return 0;
    for (i = 0; i < q->motion.count; i++)
        if ((q->motion.event[i].place >> 6) >= PROJ_NTRK) return 0;
    for (t = 0; t < PROJ_NTRK; t++)
        for (i = 0; i < FM6_PACKED; i++)
            q->fm6[t][i] = b[PROJ_FM6_OFF + t * FM6_PACKED + i] & 0x7Fu;
    {
        char n[PROJ_NAME_LEN + 1u];
        memcpy(q->name, n, proj_name_get(n, b + PROJ_NAME_OFF));
    }
    q->sum = proj_sum(q);
    return 1;
}

#ifndef PROJ_HOST
static project_t proj_scratch               /* decoded main-loop work, never audio ISR */
    __attribute__((section(".pool")));
static char proj_name[PROJ_NAME_LEN + 1u]    /* the name of the music as it is now (loaded, saved, the editor's */
    __attribute__((section(".pool")));       /* runtime restore); "" = none. A save takes it unless one is given */
#define PROJ_NO_SLOT 0xFFu
static uint8_t proj_cur = PROJ_NO_SLOT;      /* DUELL: the MULTI (0..15) the music was loaded from or last saved to (a rename
                                              * of it renames the music too); PROJ_NO_SLOT none (the editor's restore) */
static union {                               /* serialized main-loop work; no retained expansion */
    project_store_t s;
    uint8_t raw[3840];                         /* (the staging of a backup object, up to a storage object: editor_backup.c) */
} proj_wire_u __attribute__((section(".pool")));
#define proj_wire (proj_wire_u.s)
static uint8_t proj_wire_gen;                /* +1 whenever proj_wire is rewritten (a backup's runtime copy lives there) */
static uint8_t proj_keep_play;               /* project_restore_runtime: 1 = the transport keeps running (a MULTI load) */

static void proj_steps(step_t *s)            /* a loaded sequence stays inside its fixed fields */
{
    uint32_t i, j;
    for (i = 0; i < NSTEP; i++) {
        if (s[i].n > 4u) s[i].n = 4;
        if (s[i].time > ST_REST) s[i].time = ST_REST;
        for (j = 0; j < 4u; j++) s[i].note[j] &= 127u;
    }
}

/* an import may hold values the runtime cannot take: keep them inside the fields and ranges */
static void proj_bound(project_t *q)
{
    uint32_t t, i;
    for (t = 0; t < NTRK; t++) {
        proj_steps(q->t[t].step);
        for (i = 0; i < NSTEP; i++) {
            step_t *s = &q->t[t].step[i];
            s->flags &= 3u | SF_RATCH | SF_NUDGE;
            s->vel &= 127u;
            if (s->probability > 101u) s->probability = 0;
        }
        for (i = 0; i < P_COUNT; i++) {
            const param_desc_t *d = param_desc_of(q->t[t].engine % NENGINES, i);
            q->t[t].p[i] = (int16_t)param_fit(d, q->t[t].p[i]);   /* (a retired KIT: the kit it plays) */
        }
    }
    q->sum = proj_sum(q);
}

static void project_capture(project_t *p)
{
    uint32_t i;
    uint32_t f = motion_guard();
    memset(p, 0, sizeof *p);
    p->magic = PROJ_MAGIC;
    p->size = sizeof *p;
    for (i = 0; i < G_COUNT; i++)
        p->g[i] = song.g[i];
    p->sel = song.sel;
    p->parts = NPART;
    p->phys = PROJ_PHYS;
    p->bm = (uint8_t)(song.bm > 0 && song.bm <= (int16_t)BM_N ? song.bm : 0);
    chain_defaults(&p->chain);                          /* (no song mode: an empty chain, see chain_defaults) */
    for (i = 0; i < NTRK; i++) {
        for (uint32_t j = 0; j < P_COUNT; j++) p->t[i].p[j] = motion_base_value(&trk[i], j);
        p->t[i].engine = trk[i].eng_req;
        p->t[i].preset = trk[i].preset;
        memcpy(p->t[i].step, trk[i].step, sizeof trk[i].step);
        fm6_pack(fm6_patch[i], p->fm6[i]);
    }
    p->motion = motion;
    motion_unguard(f);
    memcpy(p->name, proj_name, str_len(proj_name));
    p->sum = proj_sum(p);
}

#include "multi.c"                              /* DUELL: SAVE > MULTI, the sixteen multis M01..M16 */

static int project_restore_runtime(const project_t *input)
{
    project_t *p = &proj_scratch;
    uint32_t i, k;
    int16_t fit[NTRK][P_COUNT];                         /* (the main loop's stack: ~1.6 KB) */
    uint8_t fv[NTRK][FP_SIZE + 1u];
    int32_t fslot[NTRK];
    if (!proj_ok(input) || !proj_engines_ok(input)) return 1;
    if (p != input) memcpy(p, input, sizeof *p);
    if (!proj_keep_play)
        transport_req = 2;
    panic_req = (1u << NTRK) - 1u;                      /* (every note off) */
    /* the slow part first, interrupts on (a window of ~74 k instructions here once, a quarter of the audio interrupt's time: a load
     * with the audio running missed blocks): every value fitted to its range, each FM6 patch unpacked and the SLOT it shows found
     * (fm6_adopt_slot compares with up to 32 factory patches). The window then only copies */
    for (k = 0; k < NTRK; k++) {
        const proj_trk_t *s = &p->t[k];
        uint32_t e = s->engine;                         /* (< NENGINES: proj_engines_ok) */
        for (i = 0; i < P_COUNT; i++) {                 /* every value back inside its range (param_fit) */
            const param_desc_t *d = param_desc_of(e, i);
            fit[k][i] = (int16_t)param_fit(d, s->p[i]);
        }
        proj_steps(p->t[k].step);                       /* (the steps inside their fields: p is the scratch copy) */
        fm6_unpack(p->fm6[k], fv[k]);
        fslot[k] = e == ENGI_FM6 ? fm6_adopt_slot(fv[k], fit[k][P_E7]) : fit[k][P_E7];
    }
    fm1_irq_off();                                      /* the audio ISR must not see half a project */
    if (!proj_keep_play) {                              /* (a MULTI load keeps the transport: multi_load) */
        seq_stop();
        transport_req = 0;
    }
    motion = p->motion;
    memset(motion_active, 0, sizeof motion_active);
    motion_base_valid = 0;
    for (i = 0; i < G_COUNT; i++)
        if (i != G_SLOT && i != G_LOAD && i != G_SAVE)
            song.g[i] = (int16_t)clamp(p->g[i], GP[i].min, GP[i].max);
    song.bm = p->bm <= BM_N ? p->bm : 0;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        const proj_trk_t *s = &p->t[k];
        uint32_t e = s->engine;
        t->eng_req = (uint8_t)e;
        t->user = 0;                                    /* (no user preset slot is saved) */
        memcpy(t->p, fit[k], sizeof t->p);
        t->p[P_SCALE] = TP[P_SCALE].def;                /* a stored per-part SCALE / QNT is ignored: the scale is the instrument's (core.h) */
        t->p[P_QUANT] = TP[P_QUANT].def;
        t->preset = (uint8_t)(ds_count(e) ? (s->preset >= PROJ_DEF_SOUND ? 0u : s->preset) % ds_count(e) : 0u);   /* (sounds.c) */
        memcpy(t->step, s->step, sizeof t->step);
        fm6_set_patch(k, fv[k]);                        /* the project's own FM6 patch, never reloaded from SLOT: F n if it is that */
        fm6_own_ok &= (uint8_t)~(1u << k);              /* factory patch, else OWN (fm6_adopt, done above) */
        if (e == ENGI_FM6)
            t->p[P_E7] = (int16_t)fslot[k];
        fm6_slot[k] = (uint8_t)fslot[k];
    }
    song.sel = (uint8_t)(p->sel < NTRK ? p->sel : 0u);
    fm1_irq_on();
    proj_name_get(proj_name, (const uint8_t *)p->name);
    proj_cur = PROJ_NO_SLOT;                            /* (project_load: its slot) */
    pat_from_set(0);                                    /* (project_load: its slot; the header's letter) */
    undo.trk = 0;                                       /* (ui.c) the undo copy belongs to the old project */
    undo_depth++;                                       /* and these loads take none */
    for (k = 0; k < NTRK; k++) {                        /* the power-on sounds: format 1 (tracks 2..4) */
        track_t *t = &trk[k];
        if (p->t[k].preset == PROJ_DEF_SOUND) {
            apply_preset_to(t, TRK_DEF[k][1]);
            track_defaults_steps(t);
        }
        pat_sig[k] = ~steps_sig(t);                     /* a project's steps are the user's */
    }
    undo_depth--;
    sync_reload = 1;
    ui.force = 1;
    ui_message("LOADED");
    memset(snd_said, 0, sizeof snd_said);               /* a part on a retired engine is said again (NO ENGINE), after LOADED */
    return 0;
}
/* settings + learned panel table: one flash object. The flash copy wins at
 * boot (the .noinit copies are garbage after a power-off). */
#include "settings_persist.c"
#if FELUCCA_FLASH
static persist_t persist_saved __attribute__((section(".pool")));
static uint8_t persist_pending;                 /* 1 requested, 2 waiting after a flash error */
static uint32_t persist_retry_ms;
#endif

static void persist_boot(void)                    /* before settings_init / panel_init */
{
#if FELUCCA_FLASH
    persist_t p;
    uint32_t f = irq_save();
    flash_ok = FL_FAR(fl_jedec_ram)() == 0x856014u;       /* the expected 1 MiB part, else stay RAM-only */
    irq_restore(f);
    if (!flash_ok) {
        settings_defaults(favorites.factory[15]);  /* (DUELL 0.2.14: RAM only, as a new unit: the new defaults) */
        return;
    }
    fl_plain_window_init();                        /* flash above 0x93000 reads as plaintext through XIP (the stores' read-back) */
    {
        int n = st_load(OBJ_SETTINGS, &p, sizeof p);
        if (settings_import(&p, n))
            persist_saved = p;
        else {
            first_boot_pending = 1;                    /* no valid settings: a new unit (ui_input.c first_boot_poll) */
            settings_defaults(favorites.factory[15]);  /* DUELL 0.2.14: KNOB ACCEL ON, CLICK REC, USB SERIAL OFF (before usb_serial_apply) */
        }
    }
    multi_boot();                                  /* DUELL: the multis' names, so SAVE > MULTI's list is right */
    up_boot();                                     /* user presets */
#else
    settings_defaults(favorites.factory[15]);      /* (DUELL 0.2.14: no flash, as a new unit: the new defaults) */
#endif
}

static int erase_busy(void);                      /* erase_data.c (included below): ERASE DATA done, the restart pending */
static int autosave_quiet(void);                  /* below: transport stopped, no key / button down, no voice sounding */
static void settings_poll(void)
{
#if FELUCCA_FLASH
    persist_t p;
    if (erase_busy())
        return;                                    /* DUELL 0.2.2: nothing is written over the erased flash */
    ps_poll();                                     /* DUELL: pattern slots saved / erased (patslot.c) */
    /* DUELL 0.2.11: a flash erase stops the audio ~40 ms, so the write waits until nothing sounds (autosave_quiet: transport
     * stopped, no key or button down, no voice, release tails included), not only for the transport: the one-time "layer seen"
     * bit, set on the first open of a layer while a multi is played by hand, clicked and spiked the CPU bar (0.2.10) */
    if (!persist_pending || !flash_ok || !autosave_quiet() ||
        (persist_pending == 2u && (uint32_t)(fm1_ms - persist_retry_ms) < 1000u))
        return;
    p = persist_saved;
    settings_export(&p);
    if (!memcmp(&p, &persist_saved, sizeof p)) {
        persist_pending = 0;
        return;                                    /* unchanged: no erase cycle */
    }
    if (st_save(OBJ_SETTINGS, &p, sizeof p) == 0) {
        persist_saved = p;
        persist_pending = 0;
    } else {
        persist_pending = 2;
        persist_retry_ms = fm1_ms;
    }
#endif
}

static void settings_save(void)
{
#if FELUCCA_FLASH
    persist_pending = 1;
#endif
    settings_poll();
}

/* The autosave (1.2, Discussion #130): the music as it is (what a project holds: every track's sound, steps, LEN DIV
 * SWING GATE, mix, the song, the automation and locks, the FM6 patches, the name) kept in its own storage object
 * (OBJ_AUTOSAVE, A/B at 0xE5000 / 0xE6000, never one of the four project slots), and put back at power-on
 * (autosave_boot) with MENU > SYSTEM > RESTORE LAST ON (the default). Flash wear: a 4 KiB erase per write, the two
 * sectors taking turns, so a write only when
 *   - something a project holds changed since the last one (autosave_sig: a hash of those values, no copy; not the
 *     selected track, nor the PROJECT page's own controls),
 *   - and it has stayed the same for AS_IDLE_MS (10 s) with the transport stopped, no key or button down, no voice
 *     sounding and no editor transfer (a backup or a sample: autosave_hold); an erase stops the audio for its
 *     ~40 ms, as every save does, so it never happens while anything plays,
 *   - and at least AS_GAP_MS (60 s) after the one before (AS_RETRY_MS after a failed write).
 * So at most one write a minute while editing, none while playing or idle: about 100,000 erase cycles per sector
 * give over 200,000 writes, a write a minute for 8 hours a day for over a year, far more with real pauses.
 * RESTORE LAST OFF writes none (and restores none). Never after a crash, a hang or a failed boot (main.c): the music
 * that was playing then could be what crashed. Checked every AS_POLL_MS; the main loop only (never the audio ISR) */
#define AS_POLL_MS 250u
#define AS_IDLE_MS 10000u
#define AS_GAP_MS 60000u
#define AS_RETRY_MS 30000u
static struct {
    uint32_t saved, seen;                        /* the signature written last (or loaded at power-on), seen last */
    uint32_t t, last, poll;                      /* fm1_ms: seen changed / busy, the last write, the last check */
    uint32_t writes;                             /* (written since power-on: the host tests count them) */
    uint8_t wrote, err;                          /* a write since power-on (last is valid); the last one failed */
} as __attribute__((section(".pool")));

static uint32_t as_mix(uint32_t h, const void *p, uint32_t n)   /* FNV-1a, continued */
{
    const uint8_t *b = p;
    while (n--) {
        h ^= *b++;
        h *= 16777619u;
    }
    return h;
}

/* what an autosave would hold, hashed (no copy, no RAM): project_capture's values */
static uint32_t autosave_sig(void)
{
    uint32_t h = 2166136261u, i, j;
    for (i = 0; i < NTRK; i++) {
        const track_t *t = &trk[i];
        for (j = 0; j < P_COUNT; j++) {
            int16_t v = motion_base_value(t, j);
            h = as_mix(h, &v, sizeof v);
        }
        h = as_mix(h, &t->eng_req, 1);
        h = as_mix(h, &t->preset, 1);
        h = as_mix(h, t->step, sizeof t->step);
        h = as_mix(h, fm6_patch[i], sizeof fm6_patch[i]);
    }
    for (i = 0; i < G_COUNT; i++)
        if (i != G_SLOT && i != G_LOAD && i != G_SAVE)       /* (0.2.19: G_NAME is SHIM's MOD now) */
            h = as_mix(h, &song.g[i], sizeof song.g[i]);
    h = as_mix(h, &song.bm, sizeof song.bm);
    h = as_mix(h, &motion, sizeof motion);
    return as_mix(h, proj_name, sizeof proj_name);
}

/* nothing going on that an erase (the audio stopped ~40 ms) would be heard in, or would race */
static int autosave_quiet(void)
{
    uint32_t k, i;
    if (transport_busy() || fm1_in.notes || fm1_in.buttons)
        return 0;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NVOICE; i++)
            if (trk[k].v[i].active)
                return 0;
    return 1;
}

static void autosave_hold(void) { as.t = fm1_ms; }   /* the editor's transfers: their staging RAM is proj_wire */

/* main loop, every pass (after settings_poll) */
static void autosave_poll(void)
{
#if FELUCCA_FLASH
    uint32_t sig;
    if (!flash_ok || fm1_ms - as.poll < AS_POLL_MS)
        return;
    as.poll = fm1_ms;
    sig = autosave_sig();
    if (sig != as.seen || !autosave_quiet()) {
        as.seen = sig;
        as.t = fm1_ms;
        return;
    }
    if (sig == as.saved || (ui_prefs & PREF_RESTORE_OFF) || fm1_ms - as.t < AS_IDLE_MS ||
        (as.wrote && fm1_ms - as.last < (as.err ? AS_RETRY_MS : AS_GAP_MS)))
        return;
    as.last = fm1_ms;
    as.wrote = 1;
    project_capture(&proj_scratch);
    proj_wire_gen++;
    as.err = !proj_pack(&proj_wire, &proj_scratch) || st_save(OBJ_AUTOSAVE, &proj_wire, sizeof proj_wire) != 0;
    if (!as.err) {
        as.saved = sig;
        as.writes++;
    }
#endif
}

/* power-on, after the defaults (main.c): the autosave becomes the music when `allowed` (no crash, hang or failed boot
 * before) and RESTORE LAST is ON; an autosave that does not load (none, torn, damaged, another format) is ignored.
 * DUELL 0.2.2: a part never starts muted (MUTE is a part's setting, kept in multis and the autosave while editing, but
 * power-on is audible, see boot_unmute); a restored autosave is cleared of it, a later multi LOAD keeps its stored mute.
 * The music now (restored or not) is what is saved: nothing is written until it changes. 1 = restored */
static void boot_unmute(void)                     /* power-on is audible: no part starts muted (DUELL 0.2.2) */
{
    uint32_t k;
    for (k = 0; k < NTRK; k++)
        trk[k].p[P_MUTE] = 0;
}

static int autosave_boot(int allowed)
{
    int rc = 0;
#if FELUCCA_FLASH
    if (flash_ok && allowed && !(ui_prefs & PREF_RESTORE_OFF)) {
        int n;
        proj_wire_gen++;
        n = st_load(OBJ_AUTOSAVE, &proj_wire, sizeof proj_wire);
        if (n > 0 && proj_import(&proj_scratch, &proj_wire, n)) {
            proj_bound(&proj_scratch);
            if (!project_restore_runtime(&proj_scratch)) {
                ui_message("RESTORED");
                rc = 1;
                boot_unmute();
            }
        }
    }
#else
    (void)allowed;
#endif
    as.saved = as.seen = autosave_sig();
    as.t = fm1_ms;
    return rc;
}

#include "erase_data.c"                          /* DUELL 0.2.2: ERASE DATA (MENU > SYSTEM, editor cmd 80) */
#if FELUCCA_FLASH
_Static_assert(sizeof(project_store_t) <= ST_PAYLOAD_MAX, "project does not fit one flash sector");
_Static_assert(sizeof(persist_t) <= ST_PAYLOAD_MAX, "settings do not fit one flash sector");
#endif
#endif /* PROJ_HOST */
