/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL factory sounds (owner, 2026-10-11): the sounds the browser lists, in place of Felucca's presets (engine_t.presets,
 * which stay compiled for the host tests' renders and nothing else of the product reads).
 *
 * Format (build/gen/duell_sounds.h, written by tools/gen_duell_sounds.py from tools/duell_sounds.py): a sound is a name
 * (<= 12 characters), its engine, category (category.c CAT_*), flags, and a SPARSE list of (P_ id, value) pairs, two bytes
 * each, on top of INIT: every sound parameter at its default (params.c TP; the engine's EDIT defaults for P_E0..P_E7).
 * Arp settings and the level are pairs like any other (ui.c apply_preset_to: a factory sound sets its LEVEL, its loudness
 * trim, and an arp sound lends the arp: ds_arp).
 *
 * Identity: sound k of engine e (k = its factory index, append-only per engine in duell_sounds.py) is DS_SOUNDS[DS_ENG_AT[e]
 * + k]; t->preset holds k (as it held the preset number), favourites (favorites.c), saved copies (upreset.c up_shadow) and
 * the End B seed (ds_seed) use (e, k). An engine with no sound: k is 0 and the sound is INIT (ui.c apply_preset_to).
 * The browse order is DS_LIST (category-major: BASS LEAD PAD PLUCK KEYS FX OTHER, then ENGINE_ORDER, then name), engines
 * this build hides (eng_ok) left out.
 *
 * End B of a factory sound (morph.c morph_rebase_part): a sound loaded with no End B of its own (not a stored sound) rolls
 * B from seed ds_seed(e, k) at AMOUNT ds_amount(e, k), SMART (never WILD), PITCH and ARP off, part 1's stream: the same B on
 * every load, on either part. Both are the sound's own (duell_sounds.py 'bseed', 'bamt': the AMOUNT by category, the seed
 * picked by tests/sound_audit.c SOUND_BSEED=1 so B sits within a few dB of A's loudness and its brightness in both SPLIT and
 * STEREO); a sound without a 'bseed' uses a hash of (e, k). A stored sound keeps its saved B (or a fresh roll, as before). */
typedef struct {
    uint8_t id;                  /* P_* */
    int8_t v;
} ds_pair_t;
typedef struct {
    const char *name;
    uint8_t eng, cat, flags, np; /* engine index, CAT_*, DSF_*, pairs */
    uint8_t bamt;                /* End B: the AMOUNT (%) */
    uint16_t at;                 /* its first pair in DS_PAIR */
    uint16_t bseed;              /* End B: the seed (0: the hash of (engine, index)) */
} ds_sound_t;
#define DSF_MONO 1u              /* (informative: the generator wrote VOICE LEG) */
#define DSF_ARP 2u               /* sets the arp (a pair in P_AMODE .. P_AORDER) */
#define DSF_KEYS 4u              /* sets the chord keys / scale (a pair in P_SCALE, P_CHRD, P_VOIC): lent like the arp (ui.c ds_keys) */
#include "duell_sounds.h"
_Static_assert(DS_NENG >= NENGINES && DS_N <= 255u, "duell_sounds.h: DS_ENG_AT covers every engine, DS_LIST bytes");

static uint32_t ds_count(uint32_t e) { return e < NENGINES ? (uint32_t)(DS_ENG_AT[e + 1u] - DS_ENG_AT[e]) : 0u; }
static const ds_sound_t *ds_get(uint32_t e, uint32_t k) { return k < ds_count(e) ? &DS_SOUNDS[DS_ENG_AT[e] + k] : 0; }
static const char *ds_name(uint32_t e, uint32_t k)
{
    const ds_sound_t *s = ds_get(e, k);
    return s ? s->name : "INIT";
}
static uint32_t ds_cat_of(uint32_t e, uint32_t k)
{
    const ds_sound_t *s = ds_get(e, k);
    return s ? s->cat : 0u;      /* (none: the caller's default) */
}
static int ds_arp_id(uint32_t id) { return id >= P_AMODE && id <= P_AORDER; }
static int ds_key_id(uint32_t id) { return id == P_SCALE || id == P_CHRD || id == P_VOIC; }
/* sound s's pairs into p (kind: 1 the arp's pairs only, 2 the scale / chord keys' only, 0 every other one) */
static void ds_put(const ds_sound_t *s, int16_t *p, int kind)
{
    uint32_t i;
    for (i = 0; i < s->np; i++) {
        const ds_pair_t *q = &DS_PAIR[s->at + i];
        if (q->id < P_COUNT && (ds_arp_id(q->id) ? 1 : ds_key_id(q->id) ? 2 : 0) == kind)
            p[q->id] = q->v;
    }
}
static int32_t ds_value(const ds_sound_t *s, uint32_t id, int32_t def)   /* sound s's value of id, def if it has none */
{
    uint32_t i;
    for (i = 0; s && i < s->np; i++)
        if (DS_PAIR[s->at + i].id == id)
            return DS_PAIR[s->at + i].v;
    return def;
}

/* the browse list: DS_LIST without the engines this build hides */
static uint32_t ds_list_n(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < DS_N; i++)
        n += (uint32_t)eng_ok(DS_SOUNDS[DS_LIST[i]].eng);
    return n;
}
static uint32_t ds_list_at(uint32_t n, uint32_t *k)    /* entry n -> its engine, *k its factory index */
{
    uint32_t i;
    for (i = 0; i < DS_N; i++) {
        const ds_sound_t *s = &DS_SOUNDS[DS_LIST[i]];
        if (eng_ok(s->eng) && !n--) {
            *k = DS_LIST[i] - DS_ENG_AT[s->eng];
            return s->eng;
        }
    }
    *k = 0;
    return 0;
}
static uint32_t ds_list_rank(uint32_t e, uint32_t k)   /* (e, k)'s entry, 0xFFFFFFFF none */
{
    uint32_t i, n = 0;
    if (k >= ds_count(e) || !eng_ok(e))
        return 0xFFFFFFFFu;
    for (i = 0; i < DS_N; i++) {
        if (DS_LIST[i] == DS_ENG_AT[e] + k)
            return n;
        n += (uint32_t)eng_ok(DS_SOUNDS[DS_LIST[i]].eng);
    }
    return 0xFFFFFFFFu;
}

/* the hash seed of (e, k): 1..65535 (so neighbours roll unalike) */
static uint32_t ds_hash(uint32_t e, uint32_t k)
{
    uint32_t x = (e + 1u) * 0x9E3779B1u ^ (k + 1u) * 0x85EBCA77u;
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    return 1u + x % 65535u;
}
/* the End B seed of factory sound (e, k): 1..65535, fixed: its 'bseed', else the hash */
static uint32_t ds_seed(uint32_t e, uint32_t k)
{
    const ds_sound_t *s = ds_get(e, k);
    return s && s->bseed ? s->bseed : ds_hash(e, k);
}
/* the End B AMOUNT of factory sound (e, k) (%) */
static uint32_t ds_amount(uint32_t e, uint32_t k)
{
    const ds_sound_t *s = ds_get(e, k);
    return s && s->bamt ? s->bamt : 12u;
}
