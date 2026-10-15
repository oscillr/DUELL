/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL MULTI (SAVE > MULTI, SPEC 5b.4): sixteen slots M01..M16, each the whole instrument state of both parts. Included by
 * project.c (it replaces Felucca's four PROJECT slots: the project format FUN11 stays for the autosave and the editor's
 * backup and restore of the music, project_capture / project_restore_runtime below are shared). The functions keep the
 * project_* names the UI and the editor call (project_save, project_load, ..): a slot number is a multi now, 0..15.
 *
 * What a multi holds: per part (1 and 2) the engine and preset, every P_* value (the sound with its matrix, LFO / ENV 2,
 * INSERT, EQ, DUCK, its TRIM (the PATCH TRIM); the arp; the MIXER LEVEL, PAN, MUTE and the sends; LEN DIV SWING GATE QUANTIZE), the 64 steps, the FM6 patch (an
 * FM6 part only), the morph's AMOUNT and End B (the 7-byte block of a user preset, morph.c morph_blk_make: seed, AMOUNT, roll
 * bits; End B is rolled again from it on load); the parts' automation and parameter locks; the globals of the music
 * (MU_GLOBAL: TEMPO, SWING, CHORUS, DLY 1 / 2 with PING-PONG and its SPREAD, REVERB with its TYPE; and BASS MONO, in byte 5), the output MODE
 * (duell_mode), MORPH SET's SMART / ARP / PITCH (TARGET: written, not loaded, 0.2.9), the selected part, the name (12 characters). Not held (the
 * device's, MENU's and the settings'): the clock source (G_CLOCK), MIDI IN's routing (G_ROUTE), TUNE (G_TUNE), the colour
 * theme, MIDI channels, display, LEDs, USB. Loading replaces everything held; each part's MORPH starts at fader A with the
 * multi's End B (a patch load's rule). Loading while playing keeps the transport running: every note off, an engine change
 * fades as ever (project_restore_runtime with proj_keep_play).
 *
 * Record (version MU_VER 5, little endian, variable length, at most MU_REC_MAX = 1916 bytes):
 *   0  version, np (P_COUNT when saved), ng (G_COUNT), the selected part, MODE, byte 5: bit 0 TARGET (1 BOTH; 0.2.9: ALGORITHM's BOTH when saved, never
 *      loaded: reserved, the state is not saved), bits 1..4 BASS MONO (0.2.19: 0 OFF, 1..11 the corner; the factory multis have 0 = OFF), MORPH SET bits (morph.c
 *      MC_SET), the parts' automation PLAY bits (motion.on)
 *   8  the name, 12 bytes (ASCII 32..126, 0-padded)
 *  20  ng globals, int16 each (all stored; only MU_GLOBAL ones are loaded)
 *      per part (2): engine, preset, AMOUNT, flags (bit 0: an FM6 patch follows); End B's block (MB_N = 7 bytes, 0s: none);
 *      np values (+64, a byte each, as FUN11); the steps sparse: a 64-bit map (8 bytes, bit i = step i stored), then 9 bytes
 *      per stored step (patslot.c ps_pack_step, the packing of FUN11 and the pattern slots; a step not stored is an empty
 *      step, step_clear); the FM6 patch, 128 bytes (eng_fm6.c fm6_pack), when flag bit 0
 *      the automation: a count, then (place, param, value) per event of parts 1..2 (core.h motion_event_t)
 *      then BALANCE (int8, -100..100), XFADE (0..100), SPLIT (0 OFF, else note + 1): core.h mix_bal, mix_xf, mix_split
 *   Sizes (P_COUNT 127): 74 bytes of header and globals, a part 146 + 9 per used step (+ 128 FM6), the automation 1 + 3 per event. Two FM6
 *   parts with all 64 steps used take 1775 bytes: the record always holds the sound and the steps; the automation gets the
 *   rest of the 1916 (46 events at worst after the 3 mix bytes; was 49 at P_COUNT 122; all 128 of a part with 16-step patterns); a save that has more keeps the first
 *   that fit and says AUTOMATION CUT.
 *   DUELL 0.2.14: only a record of exactly this version (MU_VER 5) with np = P_COUNT and ng = G_COUNT loads. A record of firmware
 *   before 0.2.14 (versions 1 and 2, an older P_COUNT) or of a newer one does not: the slot lists as empty ("--") and a LOAD says
 *   EMPTY SLOT / DAMAGED; the first SAVE there replaces it, and a factory slot (M01..M10) shows its factory multi instead.
 *
 * Factory multis (build/gen/duell_multis.h, tools/gen_duell_multis.py from tools/factory_multis/M01..M10.bin and M13..M16.bin), in the 0.2.13 order:
 * M01 REESE SWAMP (a new unit's first start loads it, ui_input.c first_boot_poll), M02 WHISPER TIDE, M03 EERIE SIGNAL, M04 WIRED SNAIL, M05 DEAD BUFFER,
 * M06 ACID ETHER, M07 COLD CHAPEL, M08 GARGLE ACID, M09 NIGHT ORGAN, M10 FINAL REEL (M01 M02 M07..M10: 0.2.11's six, tests/gen_factory_multis.c)
 * are in the firmware image (ROM), not in flash. While a slot's stored record is
 * empty (first boot, a flash erase, ERASE of a saved overwrite) it shows and loads the factory multi (mu_fact bit); SAVE
 * over it stores an ordinary record that wins from then on and survives firmware updates (the ROM copy is only the default);
 * ERASE of such a slot restores the factory multi ("ALREADY FACTORY" when it is not overwritten); a rename of an unchanged
 * factory slot stores its record under the new name (an overwrite). M11, M12 have no factory default (empty: "--"). M13..M16 (DUELL 0.1 beta) are the four CPU TEST multis (TEST 1 BASE, TEST 2 PHASE, TEST 3 HEAVY, TEST 4 MIX: tools/factory_multis/M13..M16.bin, docs/TESTING.md), the same mechanism (erase restores, overwritable).
 * The records name their sounds by engine + factory index (checked at build time) and hold End B as the 7-byte block (seed,
 * AMOUNT), so they need nothing else from the sound list.
 *
 * Store: two multis per storage.c object (OBJ_MULTI0..7: M01 M02 in object 0, ..): magic "MUL1", the two records' lengths
 * (u16; 0 = empty), record 1, record 2; at most 8 + 2 x 1916 = 3840 bytes, a whole object (ST_PAYLOAD_MAX). Each object is an
 * A/B sector pair (commit record last, the newest valid copy wins): a save reads the pair's current copy, replaces its
 * record and writes the whole object to the other sector, so a power cut at any point leaves both multis of the pair as
 * they were, and a write never disturbs the partner. Flash (fm1_flash.h FL_MULTI_*): objects 0..3 at 0x97000..0x9EFFF
 * (Felucca's project sectors), 4..7 at 0xCE000..0xD5FFF (the end of USR3's stride). A Felucca project left in those sectors
 * has another object type in its commit record (OBJ_PROJECT0..3, retired): it reads as no multi (the slot shows "--"), and
 * the first save there replaces it. Writes only while stopped (STOP TO SAVE), as the projects' were.
 * RAM: the names and which slots are used (multi_boot reads the eight objects once at power-on); a record is read from flash
 * when it is loaded or written, through proj_wire (the serialized scratch). Without FELUCCA_FLASH (host builds) the objects
 * live in RAM (mu_ram). */
#define MU_NOBJ (MU_SLOTS / 2u)
#define MU_MAGIC 0x314C554Du                      /* "MUL1" */
#define MU_VER 5u                                 /* 5 (0.2.19): P_LEVEL is the MIXER level (a multi holds it: the performance state) and P_TRIM, the sound's loudness, is on the id of the legacy DUCK HOLD (tools/factory_multis_levelsplit.py moved the factory records: trim = the old LEVEL, mixer 112 = unity); 4 (0.2.16): G_DMODE is 0 DIGITAL / 1 PING-PONG / 2 RANDOM / 3 OCT UP (the ping-pong switch is gone); 3 (0.2.14): CRUSH's BITS is the curve of ins_bits.h, BALANCE, XFADE, SPLIT after the automation. The only version that loads (1..3: refused) */
#define MU_HDR 8u
#define MU_OBJ_MAX 3840u
#define MU_REC_MAX ((MU_OBJ_MAX - MU_HDR) / 2u)
#define MU_NAME_AT 8u
#define MU_G_AT 20u
#define MU_PART_FIX (4u + MB_N + P_COUNT + 8u)    /* a part without its steps and patch */
_Static_assert(MU_G_AT + 2u * G_COUNT + NSHOWN * (MU_PART_FIX + NSTEP * PS_STEP + FM6_PACKED) + 1u + 3u <= MU_REC_MAX,
               "a multi's sound and steps always fit its record (only the automation can be cut)");
_Static_assert(sizeof proj_wire_u.raw >= MU_OBJ_MAX && PROJ_NAME_LEN == 12u && NSHOWN == 2u, "multi object staging");
#if FELUCCA_FLASH
_Static_assert(MU_OBJ_MAX <= ST_PAYLOAD_MAX && OBJ_PATCH0 == OBJ_MULTI0 + MU_NOBJ, "a multi object is one storage object");
#endif
/* the globals a multi loads (the music's; not the device's: clock source, TUNE, MIDI, the SAVE pages' own) */
#define MU_GLOBAL(i) ((i) == G_BPM || (i) == G_SWING || ((i) >= G_DTIME && (i) <= G_CDEPTH) || (i) == G_RTYPE || \
                      (i) == G_RLOCUT || (i) == G_PINGSPREAD || (i) == G_DMDEP || (i) == G_DMRATE || \
                      (i) == G_DMODE || (i) == G_SHPITCH || (i) == G_SHMOD)
#define mu_buf (proj_wire_u.raw)
static char mu_names[MU_SLOTS][PROJ_NAME_LEN + 1u] __attribute__((section(".pool")));
static uint16_t mu_used;                          /* bit k: M(k + 1) holds a multi that loads */
#if !FELUCCA_FLASH
static uint8_t mu_ram[MU_NOBJ][MU_OBJ_MAX];       /* (host builds: the store) */
static uint16_t mu_ram_len[MU_NOBJ];
#endif
static int project_restore_runtime(const project_t *input);
#include "duell_multis.h"                         /* DF_N factory multis: DF_SLOT (their slots) / DM_REC / DM_AT / DM_LEN (M01..M10, M13..M16) */
static uint32_t df_of(uint32_t k)                 /* the factory multi of slot k (index into DM_*); DF_N none */
{
    uint32_t i;
    for (i = 0; i < DF_N; i++)
        if (DF_SLOT[i] == k)
            return i;
    return DF_N;
}
_Static_assert(DF_TEST_MULTIS == DUELL_TEST_MODE, "duell_multis.h was generated with another DUELL_TEST_MODE: run tools/gen_duell_multis.py (tools/build.py) with the same switch");
_Static_assert(DF_N <= MU_SLOTS, "factory multis: slots below MU_SLOTS");
static uint16_t mu_fact;                          /* bit k: M(k + 1) is empty in flash and shows its factory multi */

static uint32_t mu_rd16(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8; }
static void mu_wr16(uint8_t *b, uint32_t v) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); }
static void mu_move(uint8_t *d, const uint8_t *s, uint32_t n)   /* overlapping copy */
{
    if (d < s)
        while (n--) *d++ = *s++;
    else
        while (n--) d[n] = s[n];
}
static void mu_label(char *b, uint32_t k)         /* "M01" .. "M16" (b: 4 bytes) */
{
    b[0] = 'M';
    b[1] = (char)('0' + (k + 1u) / 10u);
    b[2] = (char)('0' + (k + 1u) % 10u);
    b[3] = 0;
}

/* object o -> mu_buf, always a well-formed one (none, torn or damaged: both records empty); its length */
static uint32_t mu_obj_read(uint32_t o)
{
    int n;
    uint32_t l0, l1, magic;
    proj_wire_gen++;                              /* (the editor's staging lives there too) */
#if FELUCCA_FLASH
    n = flash_ok ? st_load(OBJ_MULTI0 + o, mu_buf, MU_OBJ_MAX) : -1;
#else
    n = mu_ram_len[o];
    memcpy(mu_buf, mu_ram[o], (uint32_t)n);
#endif
    if (n >= (int)MU_HDR) {
        memcpy(&magic, mu_buf, 4);
        l0 = mu_rd16(mu_buf + 4);
        l1 = mu_rd16(mu_buf + 6);
        if (magic == MU_MAGIC && l0 <= MU_REC_MAX && l1 <= MU_REC_MAX && MU_HDR + l0 + l1 == (uint32_t)n)
            return (uint32_t)n;
    }
    magic = MU_MAGIC;
    memcpy(mu_buf, &magic, 4);
    mu_wr16(mu_buf + 4, 0);
    mu_wr16(mu_buf + 6, 0);
    return MU_HDR;
}
static const uint8_t *mu_rec(uint32_t h, uint32_t *len)   /* record h (0, 1) of the object in mu_buf */
{
    uint32_t l0 = mu_rd16(mu_buf + 4);
    *len = h ? mu_rd16(mu_buf + 6) : l0;
    return mu_buf + MU_HDR + (h ? l0 : 0u);
}
static int mu_obj_write(uint32_t o, uint32_t len)   /* mu_buf -> object o; 0 written */
{
#if FELUCCA_FLASH
    return flash_ok ? st_save(OBJ_MULTI0 + o, mu_buf, len) : -1;
#else
    memcpy(mu_ram[o], mu_buf, len);
    mu_ram_len[o] = (uint16_t)len;
    return 0;
#endif
}

static void mu_rest(uint8_t *q)                   /* an empty step, packed (what a record leaves out) */
{
    step_t s;
    memset(&s, 0, sizeof s);
    step_clear(&s);
    ps_pack_step(&s, q);
}

/* p (project_capture: the music as it is) and the live MODE / MORPH -> record b; its length. *cut: automation left out */
static uint32_t mu_pack(uint8_t *b, const project_t *p, const uint8_t blk[NSHOWN][MB_N], uint32_t *cut)
{
    uint32_t pos, t, i, n = 0, at;
    uint8_t rest[PS_STEP];
    char nm[PROJ_NAME_LEN + 1u];
    mu_rest(rest);
    memset(b, 0, MU_G_AT);
    b[0] = MU_VER;
    b[1] = P_COUNT;
    b[2] = G_COUNT;
    b[3] = (uint8_t)(p->sel < NSHOWN ? p->sel : 0u);
    b[4] = (uint8_t)(duell_mode_req < DM_N ? duell_mode_req : DM_SPLIT);
    b[5] = (uint8_t)((morph.target == MT_BOTH) | (p->bm <= BM_N ? p->bm : 0u) << 1);   /* bit 0 TARGET (reserved), bits 1..4 BASS MONO (0.2.19) */
    b[6] = (uint8_t)(morph.cfg & MC_SET);
    b[7] = (uint8_t)(p->motion.on & ((1u << NSHOWN) - 1u));
    memcpy(b + MU_NAME_AT, nm, proj_name_get(nm, (const uint8_t *)p->name));
    pos = MU_G_AT;
    for (i = 0; i < G_COUNT; i++, pos += 2u)
        mu_wr16(b + pos, (uint16_t)p->g[i]);
    for (t = 0; t < NSHOWN; t++) {
        const proj_trk_t *s = &p->t[t];
        uint32_t fm = s->engine == ENGI_FM6;
        uint8_t *map;
        b[pos++] = s->engine;
        b[pos++] = s->preset;
        b[pos++] = (uint8_t)morph_amount_of(t);
        b[pos++] = (uint8_t)fm;
        memcpy(b + pos, blk[t], MB_N);
        pos += MB_N;
        for (i = 0; i < P_COUNT; i++)
            b[pos++] = (uint8_t)(clamp(s->p[i], -64, 127) + 64);
        map = b + pos;
        memset(map, 0, 8);
        pos += 8u;
        for (i = 0; i < NSTEP; i++) {
            ps_pack_step(&s->step[i], b + pos);
            if (memcmp(b + pos, rest, PS_STEP)) {
                map[i >> 3] |= (uint8_t)(1u << (i & 7u));
                pos += PS_STEP;
            }
        }
        if (fm) {
            for (i = 0; i < FM6_PACKED; i++)
                b[pos + i] = p->fm6[t][i] & 0x7Fu;
            pos += FM6_PACKED;
        }
    }
    at = pos++;
    *cut = 0;
    for (i = 0; i < p->motion.count && i < MOTION_MAX; i++) {
        const motion_event_t *e = &p->motion.event[i];
        if ((e->place >> 6) >= NSHOWN)
            continue;
        if (pos + 3u + 3u > MU_REC_MAX) {                 /* (room kept for the 3 bytes at the end) */
            *cut = 1;
            continue;
        }
        b[pos++] = e->place;
        b[pos++] = e->param;
        b[pos++] = (uint8_t)e->value;
        n++;
    }
    b[at] = (uint8_t)n;
    b[pos++] = (uint8_t)mix_bal;                    /* version 2: BALANCE (int8), XFADE, SPLIT (0 OFF, else note + 1) */
    b[pos++] = mix_xf;
    b[pos++] = mix_split;
    return pos;
}

/* record b (len bytes) -> q (the music as it is: what a multi does not hold stays); set[]: MODE, TARGET, MORPH SET bits;
 * blk[], amt[]: each part's End B block and AMOUNT. 0 = not a multi this firmware loads */
static int mu_unpack(project_t *q, uint8_t blk[NSHOWN][MB_N], uint8_t amt[NSHOWN], uint8_t set[6], const uint8_t *b, uint32_t len)
{
    uint32_t pos, t, i, n;
    if (len < MU_G_AT || b[0] != MU_VER || b[1] != P_COUNT || b[2] != G_COUNT)
        return 0;
    if (b[3] >= NSHOWN || b[4] >= DM_N || (b[5] >> 5) || (b[6] & ~MC_SET) ||
        (b[7] >> NSHOWN) || MU_G_AT + 2u * G_COUNT > len)
        return 0;
    pos = MU_G_AT;
    for (i = 0; i < G_COUNT; i++, pos += 2u)
        if (MU_GLOBAL(i))
            q->g[i] = (int16_t)mu_rd16(b + pos);
    for (t = 0; t < NSHOWN; t++) {
        proj_trk_t *d = &q->t[t];
        uint32_t e, fl;
        const uint8_t *map;
        if (pos + MU_PART_FIX > len || (e = b[pos]) >= NENGINES || (b[pos + 3] & ~1u))
            return 0;
        d->engine = (uint8_t)e;
        d->preset = b[pos + 1];
        amt[t] = b[pos + 2];
        fl = b[pos + 3];
        pos += 4u;
        memcpy(blk[t], b + pos, MB_N);
        pos += MB_N;
        for (i = 0; i < P_COUNT; i++) {
            if (b[pos] > 191u)
                return 0;
            d->p[i] = (int16_t)(b[pos++] - 64);
        }
        map = b + pos;
        pos += 8u;
        for (i = 0; i < NSTEP; i++) {
            memset(&d->step[i], 0, sizeof d->step[i]);
            if (!((map[i >> 3] >> (i & 7u)) & 1u)) {
                step_clear(&d->step[i]);
                continue;
            }
            if (pos + PS_STEP > len || !ps_unpack_step(b + pos, &d->step[i]))
                return 0;
            pos += PS_STEP;
        }
        if (fl & 1u) {
            if (pos + FM6_PACKED > len)
                return 0;
            for (i = 0; i < FM6_PACKED; i++)
                if (b[pos + i] > 127u)
                    return 0;
            memcpy(q->fm6[t], b + pos, FM6_PACKED);
            pos += FM6_PACKED;
        } else {
            memcpy(q->fm6[t], FM6_INIT, FM6_PACKED);
        }
    }
    if (pos >= len || (n = b[pos++]) > MOTION_MAX || pos + 3u * n + 3u != len)
        return 0;
    memset(&q->motion, 0, sizeof q->motion);       /* (tracks 3..4: none in DUELL) */
    for (i = 0; i < n; i++, pos += 3u) {
        if ((b[pos] >> 6) >= NSHOWN)
            return 0;
        q->motion.event[i].place = b[pos];
        q->motion.event[i].param = b[pos + 1];
        q->motion.event[i].value = (int8_t)b[pos + 2];
    }
    q->motion.count = (uint8_t)n;
    q->motion.on = b[7];
    if (!motion_valid(&q->motion))
        return 0;
    q->sel = b[3];
    memset(q->name, 0, sizeof q->name);
    {
        char nm[PROJ_NAME_LEN + 1u];
        memcpy(q->name, nm, proj_name_get(nm, b + MU_NAME_AT));
    }
    {                                               /* BALANCE, XFADE, SPLIT (after the events): out of range = the defaults */
        int32_t sb = (int8_t)b[pos];
        set[3] = (uint8_t)(sb < -100 || sb > 100 ? 0 : sb);
        set[4] = b[pos + 1u] <= 100u ? b[pos + 1u] : 0u;
        set[5] = b[pos + 2u] <= 128u ? b[pos + 2u] : 0u;
    }
    set[0] = b[4];
    set[1] = b[5] & 1u;
    set[2] = b[6];
    q->bm = (b[5] >> 1) <= BM_N ? (uint8_t)(b[5] >> 1) : 0u;   /* BASS MONO (0.2.19): a code past BM_N reads OFF */
    q->sum = proj_sum(q);
    return 1;
}

/* the RAM view of object o (in mu_buf): which of its two multis load, their names */
static void mu_note(uint32_t o)
{
    uint32_t h, len, k;
    for (h = 0; h < 2u; h++) {
        const uint8_t *r = mu_rec(h, &len);
        uint8_t blk[NSHOWN][MB_N], amt[NSHOWN], set[6];
        k = 2u * o + h;
        mu_names[k][0] = 0;
        mu_used &= (uint16_t)~(1u << k);
        mu_fact &= (uint16_t)~(1u << k);
        int ok = len && mu_unpack(&proj_scratch, blk, amt, set, r, len);
        if (!ok && df_of(k) < DF_N) {             /* nothing stored, or a record this firmware does not load (one of before 0.2.14): the factory multi (ROM) */
            r = DM_REC + DM_AT[df_of(k)];
            len = DM_LEN[df_of(k)];
            mu_fact |= (uint16_t)(1u << k);
            ok = mu_unpack(&proj_scratch, blk, amt, set, r, len);
        }
        if (ok) {
            mu_used |= (uint16_t)(1u << k);
            proj_name_get(mu_names[k], r + MU_NAME_AT);
        }
    }
}
static void multi_boot(void)                      /* persist_boot: the names from flash */
{
    uint32_t o;
    mu_used = 0;
    mu_fact = 0;
    for (o = 0; o < MU_NOBJ; o++) {
        mu_obj_read(o);
        mu_note(o);
    }
}
static void multi_ram_reset(void)                 /* (host tests: a power-on; without flash every slot empty but the factory ones) */
{
    uint32_t o;
#if !FELUCCA_FLASH
    memset(mu_ram_len, 0, sizeof mu_ram_len);
#endif
    memset(mu_names, 0, sizeof mu_names);
    mu_used = 0;
    mu_fact = 0;
    for (o = 0; o < MU_NOBJ; o++) {                 /* (nothing stored: M01..M10 and M13..M16 are the factory multis) */
        mu_obj_read(o);
        mu_note(o);
    }
}

static int project_used(uint32_t slot) { return (mu_used >> (slot % MU_SLOTS)) & 1u; }
static int project_factory(uint32_t slot) { return (mu_fact >> (slot % MU_SLOTS)) & 1u; }   /* shows its factory multi */
/* slot's name -> b (PROJ_NAME_LEN + 1 bytes); 0 = an empty slot (b "") */
static int project_info(uint32_t slot, char *b)
{
    str_cpy(b, project_used(slot) ? mu_names[slot % MU_SLOTS] : "", PROJ_NAME_LEN + 1u);
    return project_used(slot);
}
static int project_name(uint32_t slot, char *b) { return project_info(slot, b); }
static uint32_t project_cur_slot(void) { return proj_cur; }   /* (0..15, PROJ_NO_SLOT none) */
static void project_cur_name(char *b) { str_cpy(b, proj_name, PROJ_NAME_LEN + 1u); }   /* b: 13 bytes */

static void mu_say(const char *what, uint32_t k)  /* "SAVED M05" */
{
    char b[4];
    mu_label(b, k);
    ui_say(what, b);
}

/* object o in mu_buf with record h replaced by rec (n bytes, 0: emptied) built in place by mu_pack, or its bytes moved;
 * the partner kept as it is. fill: 1 = pack the music into it (*cut), 0 = empty it. Returns the object's length */
static uint32_t mu_put_rec(uint32_t h, int fill, const uint8_t blk[NSHOWN][MB_N], uint32_t *cut, const uint8_t *raw, uint32_t rawlen);
static uint32_t mu_put(uint32_t h, int fill, const uint8_t blk[NSHOWN][MB_N], uint32_t *cut)
{
    return mu_put_rec(h, fill, blk, cut, 0, 0);
}
static uint32_t mu_put_rec(uint32_t h, int fill, const uint8_t blk[NSHOWN][MB_N], uint32_t *cut, const uint8_t *raw, uint32_t rawlen)
{
    uint32_t l0 = mu_rd16(mu_buf + 4), l1 = mu_rd16(mu_buf + 6), n;
    *cut = 0;
    if (h == 0u) {                                /* the partner (record 2) to the end of the buffer, record 1 at 8, the */
        mu_move(mu_buf + MU_OBJ_MAX - l1, mu_buf + MU_HDR + l0, l1);   /* partner back after it */
        n = raw ? rawlen : fill ? mu_pack(mu_buf + MU_HDR, &proj_scratch, blk, cut) : 0u;
        if (raw)
            memcpy(mu_buf + MU_HDR, raw, rawlen);
        mu_move(mu_buf + MU_HDR + n, mu_buf + MU_OBJ_MAX - l1, l1);
        l0 = n;
    } else {
        l1 = raw ? rawlen : fill ? mu_pack(mu_buf + MU_HDR + l0, &proj_scratch, blk, cut) : 0u;
        if (raw)
            memcpy(mu_buf + MU_HDR + l0, raw, rawlen);
    }
    mu_wr16(mu_buf + 4, l0);
    mu_wr16(mu_buf + 6, l1);
    return MU_HDR + l0 + l1;
}

/* the music as it is now -> multi slot, named `name` (0: the current name; "" none); 0 saved, nonzero refused or failed.
 * The current name becomes the saved one */
static int project_save_as(uint32_t slot, const char *name)
{
    uint32_t k = slot % MU_SLOTS, o = k / 2u, len, cut, pi;
    uint8_t blk[NSHOWN][MB_N];
    project_t *p = &proj_scratch;
    if (transport_busy()) {                            /* a flash erase silences the audio and stalls the */
        ui_message("STOP TO SAVE");                     /* sequencer (storage_hw.c): only while stopped */
        return 1;
    }
    for (pi = 0; pi < NSHOWN; pi++)                    /* End B as its seed (it is rolled again if edits moved it) */
        if (!morph_blk_make(pi, blk[pi]))
            memset(blk[pi], 0, MB_N);
    project_capture(p);
    if (name) {
        memset(p->name, 0, sizeof p->name);
        memcpy(p->name, name, str_len(name) < PROJ_NAME_LEN ? str_len(name) : PROJ_NAME_LEN);
    }
    mu_obj_read(o);
    len = mu_put(k & 1u, 1, blk, &cut);
    if (mu_obj_write(o, len)) {
        mu_obj_read(o);                                /* (the slots as they are) */
        mu_note(o);
        ui_message("SAVE ERROR");
        return 2;
    }
    proj_name_get(proj_name, (const uint8_t *)p->name);   /* (before mu_note: it decodes into proj_scratch) */
    mu_note(o);
    proj_cur = (uint8_t)k;
    pat_from_set(k + 1u);
    if (cut)
        ui_message("AUTOMATION CUT");
    else
        mu_say("SAVED ", k);
    return 0;
}
static int project_save(uint32_t slot) { return project_save_as(slot, 0); }

/* ERASE: the multi gone, its partner kept; 0 done, 1 refused (playing, empty), 2 failed */
static int project_erase(uint32_t slot)
{
    uint32_t k = slot % MU_SLOTS, o = k / 2u, len, cut;
    if (transport_busy()) {
        ui_message("STOP TO SAVE");
        return 1;
    }
    if (project_factory(k)) {                          /* never overwritten: nothing stored to erase */
        ui_message("ALREADY FACTORY");
        return 1;
    }
    if (!project_used(k)) {
        ui_message("EMPTY");
        return 1;
    }
    mu_obj_read(o);
    len = mu_put(k & 1u, 0, 0, &cut);
    if (mu_obj_write(o, len)) {
        mu_obj_read(o);
        mu_note(o);
        ui_message("SAVE ERROR");
        return 2;
    }
    mu_note(o);
    if (proj_cur == k)
        proj_cur = PROJ_NO_SLOT;
    mu_say("ERASED ", k);                              /* (M01..M10, M13..M16: the factory multi is back, mu_note) */
    return 0;
}

/* a stored multi renamed in place (nothing else of it changes; the music playing is not touched, but the slot it was loaded
 * from / saved to: its name is the new one, the next SAVE's prefill): 0 done, 1 refused (playing, an empty slot), 2 failed */
static int project_rename(uint32_t slot, const char *name)
{
    uint32_t k = slot % MU_SLOTS, o = k / 2u, len, n;
    uint8_t *r;
    if (transport_busy()) {
        ui_message("STOP TO SAVE");
        return 1;
    }
    if (!project_used(k)) {
        ui_message("EMPTY SLOT");
        return 1;
    }
    len = mu_obj_read(o);
    if (project_factory(k)) {                          /* an unchanged factory multi: its record stored, renamed (an overwrite) */
        uint32_t cut;
        len = mu_put_rec(k & 1u, 1, 0, &cut, DM_REC + DM_AT[df_of(k)], DM_LEN[df_of(k)]);
    }
    r = (uint8_t *)mu_rec(k & 1u, &n);
    memset(r + MU_NAME_AT, 0, PROJ_NAME_LEN);
    {
        char nm[PROJ_NAME_LEN + 1u];
        uint32_t i;
        for (i = 0; i < PROJ_NAME_LEN && name[i]; i++)
            nm[i] = name[i];
        nm[i] = 0;
        memcpy(r + MU_NAME_AT, nm, proj_name_get(nm, (const uint8_t *)nm));
    }
    if (mu_obj_write(o, len)) {
        mu_obj_read(o);
        mu_note(o);
        ui_message("SAVE ERROR");
        return 2;
    }
    mu_note(o);
    if (proj_cur == k)
        str_cpy(proj_name, mu_names[k], sizeof proj_name);
    ui_message("RENAMED");
    return 0;
}

/* LOAD: everything the multi holds replaces the music (see the top); each part's MORPH at A with the multi's End B */
static void project_load(uint32_t slot)
{
    uint32_t k = slot % MU_SLOTS, len, pi;
    uint8_t blk[NSHOWN][MB_N], amt[NSHOWN], set[6];
    const uint8_t *r;
    int rc;
    mu_obj_read(k / 2u);
    r = mu_rec(k & 1u, &len);
    if (!len && df_of(k) < DF_N) {                     /* nothing stored: the factory multi */
        r = DM_REC + DM_AT[df_of(k)];
        len = DM_LEN[df_of(k)];
    }
    if (!len) {
        ui_message("EMPTY SLOT");
        return;
    }
    project_capture(&proj_scratch);                    /* (what a multi does not hold stays as it is) */
    if (!mu_unpack(&proj_scratch, blk, amt, set, r, len)) {
        ui_message("DAMAGED");
        return;
    }
    proj_bound(&proj_scratch);
    proj_keep_play = 1;                                /* (playing: it goes on; every note off) */
    rc = project_restore_runtime(&proj_scratch);
    proj_keep_play = 0;
    if (rc) {
        ui_message("DAMAGED");
        return;
    }
    /* 0.2.11: the MODE saved in the multi is applied only when MENU > CONTROL > MODE RECALL is ON (default OFF: the unit stays in its
     * mode, and BALANCE, XFADE and SPLIT stay as set; a PC recall too). The CPU TEST multis (M13..M16, developer builds) always set theirs: TEST 3 HEAVY is DUEL on purpose. */
    if ((ui_prefs2 & (PREF_MODE_RECALL >> 16))
#if DUELL_TEST_MODE
        || (k >= 12u && k <= 15u)
#endif
        )
    {
        mix_bal = (int8_t)set[3];                      /* (the settings bytes follow: ui_input.c mode_settings) */
        mix_xf = set[4];
        mix_split = set[5];
        duell_mode_req = set[0];                       /* (the audio takes it at its next block: seq.c mode_apply) */
    }
    /* set[1] TARGET: reserved (0.2.9: ALGORITHM's BOTH is the player's, a load keeps it) */
    morph.cfg = set[2];
    for (pi = 0; pi < NSHOWN; pi++)
        morph_load_part(pi, blk[pi], amt[pi]);
    proj_cur = (uint8_t)k;
    pat_from_set(k + 1u);
    perf_fade_begin(1);                                /* 0.2.14: PERF LEAVE FADE glides SPACE and KILL to neutral, KEEP leaves them (was: both set to 0 at once, 0.2.11: a new multi starts the performance macros neutral (not saved with it; SPACE glides: fx.c sv.spc; */
                                                       /* a patch / sound load now does the same (ui.c load_begin); DEPTH and MORPH are left as they are) */
    mu_say("LOADED ", k);
}
