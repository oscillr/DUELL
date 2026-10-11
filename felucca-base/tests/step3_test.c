/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL build step 3 (SPEC 2.4): the 8-slot matrix, ENV 2, LFO 2, on the ids Felucca's layout gave to things DUELL does not
 * have (core.h PL_*: P_ED_FX, DIGITAL's OP ENV / OP LEVEL, DRUM's lane levels), and the project format FUN11 (parts A and B).
 *   build/host/step3_test            (DUELL/host_tests.sh "step3")
 * 1. layout: P_COUNT 111 and P_E0 103 as before (no 7-bit id grew), the retired ids each a DUELL parameter or free.
 * 2. old stores: a user preset record, a project (FUN10 of four tracks, FUN9 / FUN8 with locks and motion), a backup
 *    (project, user preset bank, settings with a MIDI LEARN map) of Felucca's layout load with every retired id at its
 *    DUELL default (matrix slots 5..8 OFF, ENV 2 / LFO 2 at their defaults), the rest as stored, DIGITAL's operator
 *    values still converted; a FUN10's tracks 3..4 not kept. FUN11 round trips DUELL's values.
 * 3. slots 5..8: every source on every destination kind evaluates exactly as in slot 1 (per voice, per block, DEPTH).
 * 4. bit-identity: slots that do nothing, the new parameters at any value while unused: the same render as none.
 * 5. ENV 2 / LFO 2 (3b, 3c): see their sections.
 * 6. cost: instructions / sample (mod_test's method) of a PHYS part and of the two parts with everything active, against
 *    the same with the matrix off; at most the +25 % the CPU baseline allows.
 * 7. pages: MOD 2 (slots 5..8), ENV 2, LFO 2 and LFO 2 SYNC in their families, with help hints; the layout lint over every
 *    page is tests/ui_render.c's (host_tests.sh "ui"). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "old_pack.h"
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif
/* the editor's backup commit (editor_backup.c) on the same build: the stubs editor.c would give it */
static uint8_t s3_rep[256];
static uint32_t s3_rep_n;
static void ed_b(uint32_t v) { if (s3_rep_n < sizeof s3_rep) s3_rep[s3_rep_n++] = (uint8_t)(v & 127u); }
static int ed_flash_stop(void) { return 0; }
static uint32_t ed_unpack7(const uint8_t *a, uint32_t na, uint8_t *out, uint32_t max) { (void)a; (void)na; (void)out; (void)max; return 0; }
static uint8_t ed_smp_buf[512] __attribute__((aligned(4)));
enum { ED_BACKUP_LIST = 65, ED_BACKUP_GET, ED_BACKUP_PUT };
static uint8_t s3_nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, s3_nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(s3_nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { memcpy(s3_nor + off, src, n); return 0; }
#define FL_FAR(fn) (fn)
#include "../firmware/src/storage.c"
#include "../firmware/src/editor_backup.c"

static int bad3;
static void ck(const char *what, int ok)
{
    printf("step3: %-88s %s\n", what, ok ? "ok" : "FAIL");
    bad3 += !ok;
}

static int32_t out_buf[2 * CTL];
static uint64_t hash;
static void mixb(uint32_t n)
{
    uint32_t i, k;
    while (n--) {
        mix_block(out_buf, CTL);
        for (i = 0; i < 2u * CTL; i++)
            for (k = 0; k < 4u; k++) {
                hash ^= ((uint32_t)out_buf[i] >> (8u * k)) & 0xFFu;
                hash *= 0x100000001B3ull;
            }
    }
}
static uint32_t eng_by_name(const char *n)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (str_eq(ENGINES[e]->name, n))
            return e;
    return 0;
}
static uint32_t preset_by_name(uint32_t e, const char *n)
{
    uint32_t k;
    for (k = 0; k < ENGINES[e]->npresets; k++)
        if (str_eq(ENGINES[e]->presets[k].name, n))
            return k;
    return 0;
}
static void fresh(uint32_t e, uint32_t pi)        /* the boot state (no FX tails), track 1 = engine e preset pi */
{
    uint32_t k;
    memset(dly_buf, 0, sizeof dly_buf);
    memset(cho_buf, 0, sizeof cho_buf);
    memset(rev_comb, 0, sizeof rev_comb);
    memset(rev_ap, 0, sizeof rev_ap);
    memset(&fx, 0, sizeof fx);
    memset(&pf, 0, sizeof pf);
    memset(ins, 0, sizeof ins);
    for (k = 0; k < NTRK; k++)
        pf.mg[k] = 32768;
    perf_held = perf_latched = 0;
    lim_env = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    vage = 0;
    rng_state = 0x1234567u;
    mod_seed = 0x2545F491u;
    song.playing = 0;
    memset(trk, 0, sizeof trk);
    memset(&mod, 0, sizeof mod);
    memset(&motion, 0, sizeof motion);
    host_tracks_init();
    for (k = 0; k < NPART; k++)
        host_preset(&trk[k], k ? 0u : e, k ? 0u : pi);
    song.sel = 0;
    hash = 0xCBF29CE484222325ull;
}
static void mslot(track_t *t, uint32_t k, int32_t s, int32_t d, int32_t a)
{
    t->p[MSLOT_ID(k)] = (int16_t)s;
    t->p[MSLOT_ID(k) + 1u] = (int16_t)d;
    t->p[MSLOT_ID(k) + 2u] = (int16_t)a;
}
static void def_of(uint32_t e, int16_t *def)
{
    uint32_t i;
    for (i = 0; i < P_COUNT; i++)
        def[i] = param_desc_of(e, i)->def;
}

/* ------------------------------------------------------------------ 1 --- */
static void t_layout(void)
{
    uint32_t i, n = 0, dk = 0, ok = P_E0 + 8u == P_COUNT && P_M5SRC == P_M4AMT + 1u && P_M8AMT == 72u &&
                                     MSLOT_ID(7u) + 2u == P_M8AMT && NMSLOT == 8u;
    for (i = 0; i < P_COUNT; i++)
        if (param_retired(i)) {
            n++;
            if (TP[i].label[0] == '-')                   /* free: no label, no range, on no page */
                ok &= TP[i].min == 0 && TP[i].max == 0 && TP[i].def == 0 && !motion_param(i);
            else
                dk++;
        }
    for (i = 0; i < NPAGES; i++)
        for (uint32_t c = 0; c < 4u; c++)
            ok &= PAGES[i].scope == SC_GLOBAL || PAGES[i].scope == SC_STEP || PAGES[i].scope == SC_TRK ||
                  PAGES[i].id[c] == 0xFFu || PAGES[i].id[c] >= P_E0 || TP[PAGES[i].id[c]].label[0] != '-';
    printf("step3: retired ids %u: DUELL parameters on them %u, free %u (the EQ's five are DUELL's now)\n", n, dk, n - dk);
    ck("layout: P_E0 + 8 == P_COUNT (the ids DUELL added all go before P_E0; no 7-bit id overflows); matrix slots 1..8 in a row (49..72); 29 retired ids, the "
       "free ones inert and on no page", ok && n == PL_NRETIRED);
    ok = 1;
    for (i = 0; i < NELEM(MIDI_CC_MAP); i++)
        ok &= MIDI_CC_MAP[i][1] >= MCC_CUT || !param_retired(MIDI_CC_MAP[i][1]);
    ck("MIDI's CC map: no CC on a retired id (the matrix's MIDI handling as before)", ok);
}

/* ------------------------------------------------------------------ 2 --- */
/* a project of Felucca's layout (four tracks): the retired ids as Felucca held them */
static void felucca_values(int16_t *p, int32_t seed)
{
    uint32_t i;
    p[PL_ED_FX] = (int16_t)(10 + seed);
    for (i = 0; i < 20u; i++)
        p[PL_FM1_ATK + i] = (int16_t)((i % 5u == 2u || i % 5u == 4u) ? 127 - seed : 20 + seed + (int32_t)i);
    for (i = PL_LN0; i <= PL_LN7; i++)
        p[i] = (int16_t)(90 - seed);
}
static int retired_defaults(const int16_t *p, uint32_t e)
{
    uint32_t i;
    int ok = 1;
    for (i = 0; i < P_E0; i++)
        ok &= !param_retired(i) || p[i] == param_desc_of(e, i)->def;
    return ok;
}

static void t_old_presets(void)
{
    static up_rec_t r;
    int16_t v[P_COUNT], def[P_COUNT], p[P_COUNT];
    uint32_t i;
    int ok;
    /* a record of Felucca 1.5 (version 4, 111 values): its retired ids hold OP ENV / lanes / ED FX */
    memset(&r, 0, sizeof r);
    r.used = UP_USED; r.ver = UP_VER_FEL; r.engine = 0; r.np = P_COUNT;
    memcpy(r.name, "OLD", 3);
    def_of(0, p);
    felucca_values(p, 3);
    p[P_REV] = 33; p[P_M2SRC] = MS_MODW; p[P_M2DST] = MD_CUT; p[P_M2AMT] = -20; p[P_E0 + 2] = 7;
    for (i = 0; i < P_COUNT; i++)
        up_set_value(&r, i, p[i]);
    up_values(&r, v);
    ok = retired_defaults(v, 0) && v[P_REV] == 33 && v[P_M2SRC] == MS_MODW && v[P_M2AMT] == -20 && v[P_E0 + 2] == 7;
    for (i = P_M5SRC; i <= P_M8AMT; i++)
        ok &= v[i] == 0;
    ck("user preset of Felucca (version 4, 111): matrix slots 5..8 OFF, every retired id its default, the rest kept", ok);
    /* 1.2 .. 1.4 (104 values): mapped by count, then the same */
    r.np = 104;
    for (i = 0; i < 96u; i++) up_set_value(&r, i, p[i]);
    for (i = 0; i < 8u; i++) up_set_value(&r, 96u + i, p[P_E0 + i]);
    up_values(&r, v);
    ok = retired_defaults(v, 0) && v[P_REV] == 33 && v[P_M2AMT] == -20 && v[P_E0 + 2] == 7 && v[P_ITYPE] == 0;
    ck("user preset of Felucca 1.2 (104): mapped by count, retired ids at their defaults", ok);
    /* a version 6 record (DUELL's): its slot 5..8 values are kept */
    r.ver = UP_VER; r.np = P_COUNT;
    p[P_M5SRC] = MS_LFO; p[P_M5DST] = MD_PAN; p[P_M5AMT] = 30; p[P_M8SRC] = MS_AT; p[P_M8DST] = MD_E1 + 3; p[P_M8AMT] = -9;
    for (i = 0; i < P_COUNT; i++)
        up_set_value(&r, i, (int16_t)param_fit(&TP[i < P_E0 ? i : 0], p[i]));
    up_values(&r, v);
    ok = v[P_M5SRC] == MS_LFO && v[P_M5DST] == MD_PAN && v[P_M5AMT] == 30 && v[P_M8SRC] == MS_AT && v[P_M8AMT] == -9;
    ck("user preset of DUELL (version 6): matrix slots 5..8 as stored", ok);
    /* a grid record of Felucca (version 5) stays Felucca's on a rename (version 4 / 5 keep their layout) */
    r.ver = UP_VER_FEL_GRID;
    ck("a Felucca grid record is a grid, of Felucca's layout", up_grid(&r) && !up_duell(&r) && up_valid(&r));
    def_of(0, def);
    (void)def;
}

/* a DIGITAL sound of Felucca (its OP ENV values at 61..80): converted to FM6 with them, not without. (DUELL: the user preset
 * banks are gone: the PATCHES store holds DUELL's layout only, so an up_put / up_load of a Felucca record is no path any
 * more; the conversion itself (fm4_convert with fm4_ops) is what is left to test.) */
static void t_old_digital_preset(void)
{
    int16_t p[P_COUNT], q[P_COUNT], ops[FM4_NOPS];
    uint8_t v[FP_SIZE + 1u], w[FP_SIZE + 1u];
    uint32_t i;
    ui_power_on();
    for (i = 0; i < P_COUNT; i++)
        p[i] = param_desc_of(ENGI_DIGITAL, i)->def;
    fm4_preset_values(p, 5);                           /* DIGITAL PAD */
    for (i = 0; i < FM4_NOPS; i++)
        p[PL_FM1_ATK + i] = FM4_OPS_DEF[i];
    p[PL_FM1_ATK + 5u] = 30;                           /* op 2 ATK */
    p[PL_FM1_ATK + 9u] = 100;                          /* op 2 LEVEL */
    memcpy(q, p, sizeof q);
    fm4_convert(q, fm4_ops(p, ops), v);
    memcpy(q, p, sizeof q);
    fm4_convert(q, 0, w);
    ck("a DIGITAL sound of Felucca: its OP ENV values matter: the conversion without them differs", memcmp(v, w, FP_SIZE) != 0);
}

static void t_old_projects(void)
{
    static project_t a, c, d;
    static uint8_t b10[3840], b9[3648];
    static project_store_t st;
    int16_t ops[FM4_NOPS], q[P_COUNT];
    uint8_t v[FP_SIZE + 1u], pk[FM6_PACKED];
    uint32_t t, i;
    int ok;
    ui_power_on();
    project_capture(&a);
    for (t = 0; t < NTRK; t++)
        felucca_values(a.t[t].p, (int32_t)t);
    a.sel = 3;
    a.t[0].p[P_REV] = 40;
    a.t[0].p[P_M2SRC] = MS_LFO; a.t[0].p[P_M2DST] = MD_SHP; a.t[0].p[P_M2AMT] = 11;
    a.t[1].engine = ENGI_DIGITAL;                       /* part B: a DIGITAL sound, its op 2 edited */
    for (i = 0; i < P_E0; i++)
        if (!param_retired(i)) a.t[1].p[i] = param_desc_of(ENGI_DIGITAL, i)->def;
    fm4_preset_values(a.t[1].p, 5);
    for (i = 0; i < FM4_NOPS; i++)
        a.t[1].p[PL_FM1_ATK + i] = FM4_OPS_DEF[i];
    a.t[1].p[PL_FM1_ATK + 5u] = 30;
    a.t[1].p[PL_FM1_ATK + 9u] = 100;
    a.t[2].engine = 6; a.t[2].p[P_LEVEL] = 50;          /* tracks 3..4: not DUELL's parts */
    a.t[3].engine = 9; a.t[3].p[P_LEVEL] = 51;
    a.t[2].step[0] = (step_t){{60}, 1, ST_NOTE, 0, 96, 0, 0, 0};
    a.motion.count = 6; a.motion.on = 15;
    a.motion.event[0] = (motion_event_t){0u << 6 | 1u, P_REV, 90};                           /* kept */
    a.motion.event[1] = (motion_event_t){0u << 6 | 2u, (uint8_t)((PL_LN0 + 2u) | MOTION_LOCK), 40};   /* a lane: goes */
    a.motion.event[2] = (motion_event_t){1u << 6 | 3u, (uint8_t)(PL_FM1_ATK + 4u), 50};      /* OP1 LEVEL: goes */
    a.motion.event[3] = (motion_event_t){2u << 6 | 4u, P_REV, 10};                           /* track 3: goes */
    a.motion.event[4] = (motion_event_t){0u << 6 | 6u, (uint8_t)(P_E5 | MOTION_LOCK), -3};   /* kept */
    a.motion.event[5] = (motion_event_t){0u << 6 | 7u, PL_ED_FX, 5};                         /* ED FX: goes */
    memcpy(q, a.t[1].p, sizeof q);
    fm4_convert(q, fm4_ops(a.t[1].p, ops), v);
    fm6_pack(v, pk);
    ok = old_pack10(b10, &a, 111u) == sizeof b10 && proj_import(&c, b10, sizeof b10);
    ck("a FUN10 of Felucca (four tracks, 111): loads", ok);
    ok &= retired_defaults(c.t[0].p, c.t[0].engine) && retired_defaults(c.t[1].p, c.t[1].engine) && c.t[0].p[P_REV] == 40 &&
          c.t[0].p[P_M2SRC] == MS_LFO && c.t[0].p[P_M2AMT] == 11 && c.t[0].p[P_M5SRC] == 0 && c.t[0].p[P_M8AMT] == 0 &&
          c.sel == 0;
    ck("  parts A B: matrix slots 5..8 OFF, every retired id its default, the rest as stored, track 4 selected -> A", ok);
    ck("  part B's DIGITAL sound: FM6, converted with its OP ENV values (read before they were retired)",
       c.t[1].engine == ENGI_FM6 && !memcmp(c.fm6[1], pk, FM6_PACKED));
    ok = 1;
    for (t = NSHOWN; t < NTRK; t++)
        ok &= c.t[t].preset == PROJ_DEF_SOUND && c.t[t].engine == trk_def_engine(t) && c.t[t].step[0].time == ST_REST;
    ck("  tracks 3..4 not kept: their power-on sound, no steps", ok);
    ck("  its motion: REV and the E5 lock kept; a lane lock, OP LEVEL, ED FX and track 3's record dropped",
       c.motion.count == 2u && c.motion.event[0].param == P_REV && c.motion.event[1].param == (P_E5 | MOTION_LOCK) &&
       !(c.motion.on >> NSHOWN) && motion_valid(&c.motion));
    /* FUN11: written as DUELL's, read back the same; its parts A B only */
    c.t[0].p[P_M5SRC] = MS_VEL; c.t[0].p[P_M5DST] = MD_AMP; c.t[0].p[P_M5AMT] = 21;
    c.t[1].p[P_M8SRC] = MS_KEY; c.t[1].p[P_M8DST] = MD_DEPTH; c.t[1].p[P_M8AMT] = -40;
    c.sum = proj_sum(&c);
    ok = proj_pack(&st, &c) && ((uint32_t *)st.raw)[0] == 0x46554E42u && proj_import(&d, &st, sizeof st) &&
         !memcmp(d.t, c.t, NSHOWN * sizeof c.t[0]) && !memcmp(&d.motion, &c.motion, sizeof c.motion) &&
         d.t[0].p[P_M5AMT] == 21 && d.t[1].p[P_M8AMT] == -40;
    ck("FUN11 round trip: DUELL's values on the reused ids kept (slots 5..8), parts A B, motion", ok);
    /* FUN9 (1.1: 99 parameters) with a lock on a lane level and on OP LEVEL: loads (they go), it is not refused */
    a.motion.count = 3;
    a.motion.event[2] = (motion_event_t){0u << 6 | 3u, (uint8_t)(PL_FM1_ATK + 9u), 60};
    a.motion.event[3] = (motion_event_t){0};
    a.motion.event[4] = (motion_event_t){0};
    a.motion.event[5] = (motion_event_t){0};
    chain_defaults(&a.chain);
    ok = old_pack(b9, &a, OLD_FUN9, 99u) == sizeof b9 && proj_import(&c, b9, sizeof b9) && c.motion.count == 1u &&
         c.motion.event[0].param == P_REV && retired_defaults(c.t[0].p, c.t[0].engine);
    ck("FUN9 of Felucca with motion on a lane level and on OP LEVEL: loads, those records go", ok);
    /* restored onto the device: no crash, the parts have DUELL's defaults there */
    ok = proj_import(&c, b10, sizeof b10) && !project_restore_runtime(&c);
    for (t = 0; t < NSHOWN; t++)
        ok &= retired_defaults(trk[t].p, trk[t].eng_req) && trk[t].p[P_M5SRC] == 0;
    ck("  restored onto the device: parts A B with DUELL's defaults on the reused ids", ok && trk[0].p[P_REV] == 40);
}

/* the backup (editor_backup.c): an archive of Felucca's (its project, user preset bank and settings) restored */
static uint32_t bk_put(uint32_t id, const void *v, uint32_t len)
{
    ed_bk_id = (uint8_t)id;
    ed_bk_len = len;
    ed_bk_pos = len;
    memcpy(ED_BK_RAW, v, len);
    ed_bk_crc = st_crc32(ED_BK_RAW, len);
    return ed_bk_commit();
}
/* (DUELL: the project-object and user-bank halves of this test went with the PROJECT slots (proj_slot) and the user preset
 * banks (up_bank_t); tests/project_test.c holds the old project imports, patch_test / patslot_test the PATCHES store.
 * What stays: the settings object of Felucca with a MIDI LEARN map) */
static void t_old_backup(void)
{
    static persist_t ps;
    uint32_t e0, e1, e2;
    int ok;
    ui_power_on();
    /* settings of Felucca with a MIDI LEARN map: CC 20 on a lane level (84: a DUELL id now), CC 21 on REV; DUELL's mark 0 */
    settings_export(&ps);
    ps.favorites.factory[15][25] = 0;
    e0 = 20u | 0u << 7 | (PL_LN0 + 1u + 1u) << 9;
    e1 = 21u | 1u << 7 | (P_REV + 1u) << 9;
    e2 = 22u | 0u << 7 | (PL_FM1_ATK + 1u) << 9;
    ps.favorites.factory[14][0] = (uint8_t)e0; ps.favorites.factory[14][1] = (uint8_t)(e0 >> 8);
    ps.favorites.factory[14][2] = (uint8_t)e1; ps.favorites.factory[14][3] = (uint8_t)(e1 >> 8);
    ps.favorites.factory[14][4] = (uint8_t)e2; ps.favorites.factory[14][5] = (uint8_t)(e2 >> 8);
    ok = bk_put(1, &ps, sizeof ps) == 0u;
    ok &= ml_get(0) == 0u && ml_get(1) == e1 && ml_get(2) == 0u && favorites.factory[15][25] == 1u;
    ck("backup: settings of Felucca: MIDI LEARN entries on retired ids dropped (REV kept), DUELL's mark set", ok);
    settings_export(&ps);                               /* DUELL's own map: an entry on id 84 stays */
    ps.favorites.factory[14][0] = (uint8_t)e0; ps.favorites.factory[14][1] = (uint8_t)(e0 >> 8);
    ok = ps.favorites.factory[15][25] == 1u && bk_put(1, &ps, sizeof ps) == 0u && ml_get(0) == e0;
    ck("  settings of DUELL (marked): an entry on a reused id is DUELL's and stays", ok);
    memset(favorites.factory[14], 0, sizeof favorites.factory[14]);
}

/* ------------------------------------------------------------------ 3 --- */
typedef struct {
    uint8_t on, amp, nv, nk;
    int32_t pit, cut, shp, gain, ldep;
    uint8_t vsrc, vdst;
    int8_t vamt;
    int16_t p[P_COUNT];
    vmod_t m;
} snap_t;
static void snap(track_t *t, uint32_t k, uint32_t s, uint32_t d, snap_t *o)
{
    uint32_t j;
    vmod_t m;
    for (j = 0; j < NMSLOT; j++)
        mslot(t, j, 0, 0, 0);
    mslot(t, k, (int32_t)s, (int32_t)d, 37);
    memset(o, 0, sizeof *o);
    mod_begin(t);
    o->on = mod.on; o->amp = mod.amp; o->nv = mod.nv; o->nk = mod.nk;
    o->pit = mod.pit; o->cut = mod.cut; o->shp = mod.shp; o->gain = mod.gain; o->ldep = mod.ldep;
    o->vsrc = mod.nv ? mod.vsrc[0] : 0; o->vdst = mod.nv ? mod.vdst[0] : 0; o->vamt = mod.nv ? mod.vamt[0] : 0;
    for (j = 0; j < P_COUNT; j++)
        o->p[j] = t->p[j];
    for (j = 0; j < 3u; j++)
        o->p[MSLOT_ID(k) + j] = 0;                      /* (the slot itself: not the result) */
    memset(&m, 0, sizeof m);
    m.envq15 = 20000; m.shape = 64 << 8; m.pitch16 = 1000; m.inc = pitch_inc(1000); m.amp1 = 20000;
    if (mod.on)
        mod_voice(t, &t->v[0], &m, 0);
    o->m = m;
    if (mod.on)
        mod_end(t);
}
static void t_slots(void)
{
    track_t *t = &trk[0];
    static snap_t a, b;
    uint32_t s, d, k, n = 0, diff = 0, kinds = 0;
    fresh(eng_by_name("TRIO"), 0);
    t->p[P_VOICE] = V_POLY;
    trk_note_on(t, 64, 90);
    mixb(4);
    t->lfo_val = 21000; t->lfo_fade = 32767; t->lfo_rnd = 0x9ABC1234u; t->lfo_prev = -5000; t->lfo_ph = 0x40000000u;
    t->mw = 100; t->at = 40; t->ex_off = 20;
    for (s = 1; s < MS_N; s++)
        for (d = 1; d < MD_N; d++) {
            snap(t, 0, s, d, &a);
            for (k = 4; k < NMSLOT; k++) {
                snap(t, k, s, d, &b);
                n++;
                diff += memcmp(&a, &b, sizeof a) != 0;
            }
            kinds |= a.on ? 1u << (d == MD_DEPTH ? 2 : d <= MD_AMP ? 0 : 1) : 0u;
        }
    printf("step3: slots 5..8: %u routes (%u sources x %u destinations x 4 slots) compared with slot 1, %u differ\n", n,
           MS_N - 1u, MD_N - 1u, diff);
    ck("slots 5..8: every source on every destination (per voice, per block, DEPTH) as in slot 1", !diff && kinds == 7u);
    /* two slots on one destination add up: slot 1 and slot 5 on PAN, MODW +16 each (+31 + +31) */
    fresh(0, 0);
    t->mw = 127; t->p[P_PAN] = 0;
    mslot(t, 0, MS_MODW, MD_PAN, 16);
    mslot(t, 4, MS_MODW, MD_PAN, 16);
    mod_begin(t);
    s = (uint32_t)t->p[P_PAN];
    mod_end(t);
    ck("slot 1 and slot 5 on PAN add up (+62), and PAN is put back after the block", s == 62u && t->p[P_PAN] == 0);
    /* slot 8 as DEPTH of the LFO: it scales the LFO the other slots see */
    fresh(0, 0);
    t->lfo_val = 32767; t->lfo_fade = 32767; t->mw = 0;
    mslot(t, 7, MS_MODW, MD_DEPTH, 63);
    mslot(t, 1, MS_LFO, MD_CUT, 63);
    mod_begin(t);
    ck("slot 8 MODW -> DEPTH (wheel down): the LFO slot 2 reads is nearly closed", mod.ldep < 600 && mod.cut < 600 && mod.cut > 0);
    mod_end(t);
}

/* ------------------------------------------------------------------ 4 --- */
typedef void (*setup_fn)(track_t *t);
static setup_fn ph_setup;
static uint64_t phrase_child(setup_fn f)
{
    int fd[2];
    uint64_t h = 0;
    pid_t pid;
    if (pipe(fd))
        return 0;
    fflush(stdout);
    if (!(pid = fork())) {
        track_t *t = &trk[0];
        int16_t keep[P_COUNT];
        fresh(0, 0);
        t->p[P_VOICE] = V_POLY;
        if (f)
            f(t);
        memcpy(keep, t->p, sizeof keep);
        trk_note_on(t, 60, 100);
        trk_note_on(t, 64, 70);
        trk_note_on(t, 67, 120);
        mixb(FS / 2u / CTL);
        trk_note_on(t, 72, 90);
        mixb(FS / 4u / CTL);
        trk_note_off(t, 60); trk_note_off(t, 64); trk_note_off(t, 67); trk_note_off(t, 72);
        mixb(FS / CTL);
        h = hash;
        if (memcmp(keep, t->p, sizeof keep) || mod.on)
            h = 1;
        if (write(fd[1], &h, sizeof h) != sizeof h)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], &h, sizeof h) != sizeof h)
        h = 0;
    close(fd[0]);
    waitpid(pid, 0, 0);
    return h;
}
static void su_idle(track_t *t)                      /* slots 5..8 set but doing nothing (SRC, DST or AMT 0) */
{
    mslot(t, 4, MS_LFO, MD_CUT, 0);
    mslot(t, 5, MS_OFF, MD_PAN, 40);
    mslot(t, 6, MS_VEL, MD_OFF, -30);
    mslot(t, 7, MS_MODW, 0, 0);
}
static void su_slot1(track_t *t) { mslot(t, 0, MS_LFO, MD_CUT, 40); mslot(t, 1, MS_ENV, MD_REV, 30); }
static void su_slot6(track_t *t) { mslot(t, 5, MS_LFO, MD_CUT, 40); mslot(t, 6, MS_ENV, MD_REV, 30); }
static void t_bitident(void)
{
    uint64_t h0 = phrase_child(0), h1 = phrase_child(su_idle), h2 = phrase_child(su_slot1), h3 = phrase_child(su_slot6);
    ck("slots 5..8 that do nothing: bit-identical to no matrix", h0 > 1u && h0 == h1);
    ck("the same routes in slots 6..7 as in slots 1..2: the same render (and not the dry one)", h2 > 1u && h2 == h3 && h2 != h0);
}

/* ------------------------------------------------------------------ 5 --- */
/* ENV 2 (3b): ENV 1's math tick for tick with the same times (ENV SYNC OFF and ON); through the matrix it shapes the
 * amplitude with the expected timing; unread it changes nothing */
static int env2_follows_env1(int esync)
{
    track_t *t = &trk[0];
    uint32_t b, same = 1, moved = 0;
    fresh(0, 0);
    t->p[P_VOICE] = V_POLY;
    t->p[P_ESYNC] = (int16_t)esync;
    t->p[P_ATK] = t->p[P_E2ATK] = 40; t->p[P_DEC] = t->p[P_E2DEC] = 50;
    t->p[P_SUS] = t->p[P_E2SUS] = 70; t->p[P_REL] = t->p[P_E2REL] = 45;
    mslot(t, 6, MS_ENV2, MD_SHP, 1);                   /* (read by a slot: ENV 2 runs) */
    trk_note_on(t, 60, 100);
    for (b = 0; b < 400u; b++) {
        if (b == 250u)
            trk_note_off(t, 60);
        mixb(1);
        if (!t->v[0].active)
            break;
        same &= t->v[0].env2 == t->v[0].env && t->v[0].stage2 == (t->v[0].stage == 4u ? 3u : t->v[0].stage);
        moved |= t->v[0].env2 != 0;
    }
    return same && moved && b > 250u;
}
static double rms_block(void)
{
    double s = 0;
    uint32_t i;
    for (i = 0; i < 2u * CTL; i++)
        s += (double)out_buf[i] * out_buf[i];
    return sqrt(s / (2.0 * CTL));
}
static uint64_t su_hash_env2(void);
static void su_env2_params(track_t *t) { t->p[P_E2ATK] = 3; t->p[P_E2DEC] = 120; t->p[P_E2SUS] = 0; t->p[P_E2REL] = 1; }
static void su_env2_idle(track_t *t) { su_env2_params(t); mslot(t, 2, MS_ENV2, MD_AMP, 0); mslot(t, 3, MS_ENV2, 0, 40); }
static void t_env2(void)
{
    track_t *t = &trk[0];
    double r[400], g[400], top = 0, gtop = 0, early = 0, late = 0;
    uint32_t b, nb, half = 0, full = 0, rel = 0, ta;
    ck("ENV 2 with ENV 1's times: the same envelope tick for tick (attack, decay to SUS, release; voices end with ENV 1)",
       env2_follows_env1(0));
    ck("  .. with ENV SYNC on (note values of the tempo): the same", env2_follows_env1(1));
    /* ENV 2 -> AMP +63: ENV 1 flat (ATK 0, SUS 100 %, REL long); ENV 2 ATK 60 (TIME_MS_X10), SUS 100 %, REL 20 */
    fresh(0, 0);
    t->p[P_VOICE] = V_MONO;
    t->p[P_ATK] = 0; t->p[P_SUS] = 127; t->p[P_REL] = 100; t->p[P_ED_FLT] = 0; t->p[P_LD_AMP] = 0;
    t->p[P_LEVEL] = 60; t->p[P_DIST] = 0;              /* (well below the limiter: the level is the amplitude) */
    t->p[P_E2ATK] = 60; t->p[P_E2DEC] = 0; t->p[P_E2SUS] = 127; t->p[P_E2REL] = 20;
    t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    mslot(t, 4, MS_ENV2, MD_AMP, 63);
    ta = TIME_MS_X10[60] * FS / 10000u / CTL;           /* ENV 2's attack in blocks */
    nb = ta * 3u + 40u < 400u ? ta * 3u + 40u : 400u;
    trk_note_on(t, 57, 100);
    for (b = 0; b < nb; b++) {
        if (b == ta * 2u)
            trk_note_off(t, 57);
        mixb(1);
        r[b] = rms_block();                             /* (what is heard) */
        g[b] = t->v[0].env_out;                         /* (the amplitude the voice got: ENV 1 x VEL x the matrix's AMP) */
        if (b > ta + 2u && b < ta * 2u) {
            top = r[b] > top ? r[b] : top;
            gtop = g[b] > gtop ? g[b] : gtop;
        }
    }
    for (b = 0; b < ta * 2u; b++) {
        if (!half && g[b] >= gtop * 0.5)
            half = b;
        if (!full && g[b] >= gtop * 0.95)
            full = b;
        if (b < ta / 4u)
            early += r[b] / (ta / 4u);
        else if (b > ta + 4u)
            late += r[b] / (ta - 4u);
    }
    for (b = ta * 2u; b < nb && !rel; b++)
        if (g[b] < gtop * 0.1)
            rel = b - ta * 2u;
    printf("step3: ENV2 -> AMP: attack %u blocks (ATK2 60: %u blocks), half way at block %u, 95 %% at %u; release to 10 %% in %u "
           "blocks (REL2 20)\n", ta, ta, half, full, rel);
    ck("ENV 2 -> AMP: the amplitude's attack is ATK2's time (half way at the middle, 95 % near its end, +-2 blocks), linear",
       gtop > 1000.0 && g[0] < gtop * 0.05 && half + 2u >= ta / 2u && half <= ta / 2u + 2u && full + 2u >= ta * 95u / 100u &&
       full <= ta + 2u);
    printf("step3: ENV2 -> AMP heard: RMS %.0f over the first quarter of the attack, %.0f once it is up\n", early, late);
    ck("  heard: the first quarter of the attack under 1/4 of the level once it is up", top > 50.0 && early < late * 0.25);
    ck("  the key let go: ENV 2 releases (REL2 20) long before ENV 1 (REL 100) would", rel > 0u &&
       rel * CTL * 1000u / FS < TIME_MS_X10[20] / 10u + 10u && t->v[0].active);
    /* unread (no active slot names it) ENV 2 changes nothing, whatever its values */
    {
        uint64_t h0 = phrase_child(0), h1 = phrase_child(su_env2_params), h2 = phrase_child(su_env2_idle);
        ck("ENV 2 unread (its values set, slots naming it inactive): bit-identical to none", h0 > 1u && h0 == h1 && h0 == h2);
    }
    (void)su_hash_env2;
}
static uint64_t su_hash_env2(void) { return 0; }

/* LFO 2 (3c): LFO 1's code (voice.c lfo_tick) with its own ids and state: with LFO 1's settings it moves as LFO 1 does
 * (every wave but S&H, which draws from its own generator), FREE / NOTE, SYNC to the tempo, POL, FADE; unread it changes
 * nothing; RATE2 / DEPTH2 move it from the matrix */
static int lfo2_follows_lfo1(int wave, int sync, int trig, int pol, int fade)
{
    track_t *t = &trk[0];
    uint32_t b, same = 1;
    fresh(0, 0);
    t->p[P_LRATE] = t->p[P_L2RATE] = 90; t->p[P_LWAVE] = t->p[P_L2WAVE] = (int16_t)wave;
    t->p[P_LPHASE] = t->p[P_L2PHASE] = 33; t->p[P_LFADE] = t->p[P_L2FADE] = (int16_t)fade;
    t->p[P_LSYNC] = t->p[P_L2SYNC] = (int16_t)sync; t->p[P_LTRIG] = t->p[P_L2TRIG] = (int16_t)trig;
    t->p[P_LPOL] = t->p[P_L2POL] = (int16_t)pol;
    mslot(t, 5, MS_LFO2, MD_SHP, 1);                   /* (read by a slot: LFO 2 runs) */
    for (b = 0; b < 600u; b++) {
        if (b == 50u || b == 400u)
            trk_note_on(t, 60, 100);
        if (b == 300u)
            trk_note_off(t, 60);
        mixb(1);
        same &= t->lfo2.ph == t->lfo_ph && t->lfo2.val == t->lfo_val && t->lfo2.fade == t->lfo_fade;
    }
    return same;
}
static int lfo2_wraps;
static void su_lfo2_params(track_t *t)
{
    t->p[P_L2RATE] = 120; t->p[P_L2WAVE] = 4; t->p[P_L2PHASE] = 90; t->p[P_L2FADE] = 30;
    t->p[P_L2SYNC] = 3; t->p[P_L2TRIG] = 1; t->p[P_L2POL] = 1;
}
static void su_lfo2_idle(track_t *t) { su_lfo2_params(t); mslot(t, 2, MS_LFO2, MD_CUT, 0); mslot(t, 3, MS_MODW, MD_RATE2, 40);
                                       mslot(t, 4, MS_LFO2, MD_DEPTH2, 0); }
static void su_lfo2_cut(track_t *t) { mslot(t, 2, MS_LFO2, MD_CUT, 50); }
static void t_lfo2(void)
{
    track_t *t = &trk[0];
    static const char *const W[4] = {"SIN", "TRI", "SAW", "SQR"};
    uint32_t w, b, ok = 1, steps = 0, rs;
    int32_t last;
    char what[120];
    for (w = 0; w < 4u; w++) {
        ok = lfo2_follows_lfo1((int)w, 0, 0, 0, 0) && lfo2_follows_lfo1((int)w, 0, 1, 1, 40) && lfo2_follows_lfo1((int)w, 5, 1, 0, 0) &&
             lfo2_follows_lfo1((int)w, 8, 0, 1, 70);
        snprintf(what, sizeof what, "LFO 2 = LFO 1 with its settings, %s: RATE / SYNC, NOTE / FREE, BI / UNI, FADE, block for block", W[w]);
        ck(what, ok);
    }
    /* SYNC2 1/4 at 120 BPM: a cycle a quarter (22050 samples): 8 cycles in 4 s */
    fresh(0, 0);
    song.g[G_BPM] = 120;
    t->p[P_L2SYNC] = 5; t->p[P_L2WAVE] = 2;
    mslot(t, 5, MS_LFO2, MD_SHP, 1);
    lfo2_wraps = 0;
    for (b = 0, last = 0; b < 4u * FS / CTL; b++) {
        uint32_t ph = t->lfo2.ph;
        mixb(1);
        lfo2_wraps += t->lfo2.ph < ph;
        (void)last;
    }
    ck("LFO 2 SYNC 1/4 at 120 BPM: 8 cycles in 4 s (+-1)", lfo2_wraps >= 7 && lfo2_wraps <= 9);
    /* NOTE: a fresh phrase restarts it at PHS2; FREE: it runs on */
    fresh(0, 0);
    t->p[P_L2PHASE] = 64; t->p[P_L2RATE] = 40;
    mslot(t, 5, MS_LFO2, MD_SHP, 1);
    mixb(77);
    trk_note_on(t, 60, 100);
    ok = t->lfo2.ph == 64u << 25 && t->lfo2.fade == 0;
    trk_note_off(t, 60);
    mixb(20);
    t->p[P_L2TRIG] = 1;
    rs = t->lfo2.ph;
    trk_note_on(t, 62, 100);
    ck("LFO 2 TRIG NOTE: a fresh phrase starts it at PHS2 (FADE from 0); FREE: it runs on", ok && t->lfo2.ph == rs);
    /* S&H: a value a cycle from its own generator: the shared rng (the engines' noise, the golden renders) untouched */
    fresh(0, 0);                                     /* (LFO 1 draws from the shared rng each cycle: the same run without LFO 2) */
    t->p[P_L2WAVE] = 4; t->p[P_L2RATE] = 110;
    mixb(2000);
    rs = rng_state;
    fresh(0, 0);
    t->p[P_L2WAVE] = 4; t->p[P_L2RATE] = 110;
    mslot(t, 5, MS_LFO2, MD_SHP, 1);
    last = t->lfo2.val;
    for (b = 0; b < 2000u; b++) {
        mixb(1);
        steps += t->lfo2.val != last;
        last = t->lfo2.val;
    }
    printf("step3: LFO 2 S&H: %u steps in 2000 blocks\n", steps);
    ck("LFO 2 S&H: steps once a cycle, its own generator (the shared rng untouched)", steps > 5u && rng_state == rs);
    /* RATE2 and DEPTH2 from the matrix */
    fresh(0, 0);
    t->p[P_L2RATE] = 50;
    t->mw = 127;
    mslot(t, 0, MS_MODW, MD_RATE2, 32);
    mod_begin(t);
    ok = t->p[P_L2RATE] == 50 + ((((127 * 258) * 32) >> 6) * 127 >> 15);
    mod_end(t);
    ck("MODW -> RATE2 +32: LFO 2's rate up half its range this block, put back after", ok && t->p[P_L2RATE] == 50);
    t->lfo2.val = 32767; t->lfo2.fade = 32767; t->mw = 0;
    mslot(t, 0, MS_MODW, MD_DEPTH2, 63);
    mslot(t, 1, MS_LFO2, MD_CUT, 63);
    mod_begin(t);
    ok = mod.ldep2 < 600 && mod.ldep == 32767 && mod.cut > 0 && mod.cut < 600;
    mod_end(t);
    t->mw = 127;
    mod_begin(t);
    ok &= mod.ldep2 >= 32700 && mod.cut > 16000;
    mod_end(t);
    ck("MODW -> DEPTH2: the wheel down closes LFO 2 (and not LFO 1), up opens it", ok);
    mslot(t, 0, MS_LFO2, MD_DEPTH, 63);              /* LFO 2 -> DEPTH of LFO 1: LFO 2 unscaled (no loop) */
    mslot(t, 1, MS_LFO, MD_DEPTH2, 63);
    t->lfo_val = -32767; t->lfo_fade = 32767;
    mod_begin(t);
    ok = mod.ldep >= 32700 && mod.ldep2 < 600;
    mod_end(t);
    ck("LFO 2 -> DEPTH and LFO -> DEPTH2 together: each reads the other unscaled (fixed order, no loop)", ok);
    {
        uint64_t h0 = phrase_child(0), h1 = phrase_child(su_lfo2_params), h2 = phrase_child(su_lfo2_idle),
                 h3 = phrase_child(su_lfo2_cut);
        ck("LFO 2 unread (its values set, slots naming it inactive): bit-identical to none", h0 > 1u && h0 == h1 && h0 == h2);
        ck("LFO 2 -> CUT: heard (not the dry render)", h3 > 1u && h3 != h0);
    }
}

/* ------------------------------------------------------------------ 6 --- */
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}
static void all_active(track_t *t)                   /* every slot active, every kind (step 3: the new sources too) */
{
    mslot(t, 0, MS_LFO, MD_CUT, 30);
    mslot(t, 1, MS_VEL, MD_AMP, 40);
    mslot(t, 2, MS_MODW, MD_VIB, 8);
    mslot(t, 3, MS_LFO, MD_E1 + 2, 20);
    mslot(t, 4, MS_ENV2, MD_SHP, 25);                  /* (3b: ENV 2) */
    mslot(t, 5, MS_LFO2, MD_PITCH, 3);                 /* (3c: LFO 2, RATE2, DEPTH2) */
    mslot(t, 6, MS_SLEW, MD_RATE2, 20);
    mslot(t, 7, MS_AT, MD_DEPTH2, 30);
}
/* instructions per sample: parts[k] = {engine, preset, notes} (DUELL: parts A and B); on: all_active */
static double cost(const uint8_t parts[NSHOWN][3], int on)
{
    static const uint8_t NOTES[8] = {48, 52, 55, 59, 60, 64, 67, 71};
    uint32_t p, i, rep;
    double best = 1e30;
    fresh(0, 0);
    for (p = 0; p < NSHOWN; p++) {
        track_t *t = &trk[p];
        host_preset(t, parts[p][0], parts[p][1]);
        t->p[P_VOICE] = V_POLY;
        t->p[P_SUS] = 127;
        t->p[P_AMODE] = 0;
        t->mw = 90;
        t->at = 60;
        if (on && parts[p][2])
            all_active(t);
        for (i = 0; i < parts[p][2]; i++)
            trk_note_on(t, NOTES[i] + 12u * p, 100);
    }
    mixb(FS / 2u / CTL);
    for (rep = 0; rep < 3u; rep++) {
        uint64_t i0 = instr_now();
        double ipc;
        mixb(FS / CTL);
        ipc = (double)(instr_now() - i0) / FS;
        best = ipc < best ? ipc : best;
    }
    return best;
}
#define STEP3_COST_MAX 25.0
static void t_cost(void)
{
    uint32_t e_phys = eng_by_name("PHYS"), pp = preset_by_name(e_phys, "BOWED METAL");
    uint8_t phys[NSHOWN][3] = {{(uint8_t)e_phys, (uint8_t)pp, 4}}, two[NSHOWN][3] = {{2, 0, 4}, {(uint8_t)e_phys, (uint8_t)pp, 4}};
    double c0, c1, m0, m1;
    if (!instr_now()) {
        printf("step3: cost: no instruction counter on this host, skipped\n");
        return;
    }
    c0 = cost(phys, 0);
    c1 = cost(phys, 1);
    m0 = cost(two, 0);
    m1 = cost(two, 1);
    printf("step3: cost: PHYS BOWED METAL (4 voices): %.0f -> %.0f instructions / sample with everything active (+%.1f %%)\n",
           c0, c1, (c1 - c0) * 100 / c0);
    printf("step3: cost: parts A (PHASE, 4 voices) + B (PHYS, 4): %.0f -> %.0f (+%.1f %%)\n", m0, m1, (m1 - m0) * 100 / m0);
    ck("cost: everything active stays within the +25 % the CPU baseline allows",
       (c1 - c0) * 100 / c0 <= STEP3_COST_MAX && (m1 - m0) * 100 / m0 <= STEP3_COST_MAX);
}

/* ------------------------------------------------------------------ 7 --- */
static uint32_t page_of(const char *title)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (str_eq(PAGES[i].title, title))
            return i;
    return NPAGES;
}
static void t_pages(void)
{
    uint32_t m = page_of("MOD 1"), m2 = page_of("MOD 2"), i;
    int ok = m < NPAGES && m2 == m + 1u && PAGES[m2].fam == FAM_LFO && PAGES[m2].graph == GR_MOD &&
             PAGES[m2].id[1] == P_M5SRC && PAGES[m2].id[2] == P_M5DST && PAGES[m2].id[3] == P_M5AMT && PAGES[m2].id[0] == 0xFFu;
    ck("pages: MOD 2 after MOD (LFO's family): KNOB 1 the slot 5..8, KNOB 2..4 its SRC DST AMT", ok);
    ok = 1;
    for (i = 0; i < NPAGES; i++) {
        uint8_t h = ui.home, pg = ui.page;
        ui.home = 0; ui.page = (uint8_t)i;
        ok &= help_page() != 0;
        ui.home = h; ui.page = pg;
    }
    ck("pages: every page has its help hint", ok);
    ui_power_on();
    go_title("MOD 2");
    mod_ui_slot = 0;
    turn(EN_K1, 1); turn(EN_K1, 1);                    /* slot 7 */
    turn(EN_K2, 3);                                    /* SRC 7: LFO (the knob order is OFF AENV ENV2 LFO1 ..: params.c MSRC_ORDER) */
    turn(EN_K4, 5);
    ck("MOD 2: KNOB 1 to slot 7, KNOB 2 / 4 set SRC7 / AMT7 (not slot 3's)", str_eq(cur_page()->title, "MOD 2") &&
       mod_ui_slot == 2u && TSEL->p[P_M7SRC] == MS_LFO && TSEL->p[P_M7AMT] != 0 && !TSEL->p[P_M3SRC] && !TSEL->p[P_M3AMT]);
    turn(EN_K1, 5);
    ck("MOD 2: the slot stops at 8", mod_ui_slot == 3u);
    {   /* ENV 2 (3b): after ENV DEST in ENV's family, the ADSR graph, KNOB 1..4 = ATK2 DEC2 SUS2 REL2; ENV SYNC shows note values */
        uint32_t e2 = page_of("ENV 2"), ed = page_of("AMP DEST");
        ok = e2 == ed + 1u && PAGES[e2].fam == FAM_ENV && PAGES[e2].graph == GR_ADSR && PAGES[e2].id[0] == P_E2ATK &&
             PAGES[e2].id[1] == P_E2DEC && PAGES[e2].id[2] == P_E2SUS && PAGES[e2].id[3] == P_E2REL;
        ck("pages: ENV 2 after ENV DEST (ENV's family): ADSR graph, KNOB 1..4 ATK2 DEC2 SUS2 REL2", ok);
        ui_power_on();
        go_title("AMP DEST");
        press(B_ENV);
        ok = str_eq(cur_page()->title, "ENV 2");
        {
            int16_t atk = TSEL->p[P_ATK], atk2 = TSEL->p[P_E2ATK];
            turn(EN_K1, 3);
            ok &= TSEL->p[P_E2ATK] == atk2 + 3 && TSEL->p[P_ATK] == atk;
        }
        TSEL->p[P_ESYNC] = 1;
        ok &= str_eq(track_desc(TSEL, P_E2ATK)->label, "ATK2") && track_desc(TSEL, P_E2ATK)->names == ESYNC_NAMES &&
              track_desc(TSEL, P_E2SUS) == &TP[P_E2SUS];
        TSEL->p[P_ESYNC] = 0;
        ck("ENV on ENV DEST opens ENV 2; KNOB 1 turns ATK2 (not ATK); ENV SYNC: its times as note values", ok);
    }
    {   /* LFO 2 (3c): LFO, LFO SYNC (Felucca's "LFO 2"), LFO DEST, LFO 2, LFO 2 SYNC, MOD, MOD 2; no two pages one title */
        static const char *const L[8] = {"LFO 1", "LFO 1 SYNC", "LFO 1 DEST", "LFO 2", "LFO 2 SYNC", "LFO 2 DEST", "MOD 1", "MOD 2"};
        uint32_t j, k2, l2 = page_of("LFO 2"), l2s = page_of("LFO 2 SYNC");
        ok = 1;
        for (j = 0, k2 = page_of("LFO 1"); j < 8u; j++)
            ok &= k2 + j < NPAGES && str_eq(PAGES[k2 + j].title, L[j]) && PAGES[k2 + j].fam == FAM_LFO;
        for (j = 0; j < NPAGES; j++)
            for (k2 = j + 1u; k2 < NPAGES; k2++)
                ok &= !str_eq(PAGES[j].title, PAGES[k2].title) ||    /* (MOD 1 / MOD 2 are in ENV's family too: the same page twice) */
                      (PAGES[j].graph == GR_MOD && PAGES[k2].graph == GR_MOD && PAGES[j].fam == FAM_LFO && PAGES[k2].fam == FAM_ENV &&
                       !memcmp(PAGES[j].id, PAGES[k2].id, 4));
        ok &= PAGES[l2].graph == GR_LFO && PAGES[l2].id[0] == P_L2RATE && PAGES[l2].id[3] == P_L2FADE &&
              PAGES[l2s].id[0] == P_L2SYNC && PAGES[l2s].id[1] == P_L2TRIG && PAGES[l2s].id[2] == P_L2POL;
        ck("pages: LFO, LFO SYNC, LFO DEST, LFO 2, LFO 2 SYNC, MOD, MOD 2 (LFO's family); every title unique (but MOD 1 / MOD 2 again in ENV's)", ok);
        ui_power_on();
        go_title("LFO 1 DEST");
        press(B_LFO);
        ok = str_eq(cur_page()->title, "LFO 2");
        {
            int16_t r1 = TSEL->p[P_LRATE], r2 = TSEL->p[P_L2RATE];
            turn(EN_K1, 2);
            ok &= TSEL->p[P_L2RATE] == r2 + 2 && TSEL->p[P_LRATE] == r1;
        }
        press(B_LFO);
        ok &= str_eq(cur_page()->title, "LFO 2 SYNC");
        turn(EN_K3, 1);
        ok &= TSEL->p[P_L2POL] == 1 && TSEL->p[P_LPOL] == 0;
        ck("LFO on LFO DEST opens LFO 2 (KNOB 1 RATE2), again LFO 2 SYNC (KNOB 3 POL2); LFO 1's untouched", ok);
        ok = motion_param(P_L2RATE) && motion_param(P_L2FADE) && motion_param(P_E2ATK) && motion_param(P_E2REL) &&
             !motion_param(P_L2SYNC) && !motion_param(P_L2POL) && !motion_param(P_M5SRC);
        ck("automation as their twins: RATE2..FADE2, ATK2..REL2 yes; SYNC2 TRIG2 POL2 and the matrix no", ok);
    }
}

int main(void)
{
    t_layout();
    t_old_presets();
    t_old_digital_preset();
    t_old_projects();
    t_old_backup();
    t_slots();
    t_bitident();
    t_env2();
    t_lfo2();
    t_pages();
    t_cost();
    printf("%s\n", bad3 ? "STEP 3 TEST FAILED" : "step 3 test passed");
    return bad3 != 0;
}
