/* SPDX-License-Identifier: GPL-3.0-only */
/* Cloud job B (the values): the five TWIN FILTER values on ids (TYPE 43, CUTOFF 2 106, RES 2 25, DRIVE 26, BLEND 101) and the 15 extension slots X0..X14 (ext.h, wide ids 128..142, XV(t, k)) are
 * values like any other, and the places the feature jobs plug into exist and do nothing yet. docs/design/EXTENSION_BLOCK_PLAN.md 2, 3.4, 5; TWIN_FILTER_DESIGN_2.md 3; SUB_AND_CYCLE_DESIGN.md A4, B2.
 *   TABLES    every slot has a descriptor (params.c XTP) and a morph tag (morph.c MTX); ranges, defaults, names as the plan has them; every label fits a card (50 px) and every name a value (49 px, the
 *             unit's own font table); the five ids' descriptors, the meanings of CUTOFF 2 (OFFSET / CUTOFF / TUNE) and RES 2 (RES / FDBK) by LINK and TYPE; F_DSEMI's text
 *   NOTHING   the values at any setting change nothing while they are not read: a two-part render with the five ids and all 15 slots off their defaults (TYPE OFF) is bit for bit the one without
 *   MORPH     the old ids' End B from a stored seed is the one cloud/base3 rolled (a hash made on cloud/base3: xvalues_ref.h), except the five; the ends and the history of the slots (XH deep: the
 *             report runs this test at 9 and at 3), the blend of the continuous ones, the switches that flip at the middle of the fader, UNDO; the rules of TWIN_FILTER_DESIGN 3.3 (TYPE never
 *             rolled in SMART, never switched on, never COMB on WAVE / PHYS; no dead cutoff 2; RES 2 above 100 never with more DRIVE; DRIVE never far above; a sub never switched on; the amounts and
 *             TWIN stay put while TYPE is OFF), the draws of the five ids' old tags kept
 *   MATRIX    mod_voice and mod_voice_add untouched: a busy two-part matrix render is the hash cloud/base3 made (the second list empty); the new destinations 51..55 and source 15, their names,
 *             "offered only while TYPE is not OFF", the per-block ones (RES 2, DRIVE, BLEND, SUB LEVEL) written for the block and put back, CUTOFF 2's second list (mod_f2_voice), CYCLE's
 *             (mod_cycle_voice, t->cy, t->m_cycle), routes into the second list changing no sample of the product
 *   CHAIN     the voice chain (voice_chain.c) with the stages empty: forced on (DUELL_CHAIN_FORCE) it is the plain path within rounding on every engine, with filter 1 bypassed, SPREAD, and across the
 *             one-block cross-fade when a sounding voice enters and leaves it; v->chn; the product leaves chain_on at 0
 * Built twice by host_tests.sh (xvalues: the default XH 3, and -DXH=1), run once each */
#define DUELL_CHAIN_FORCE 1
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "mode_scn.h"
#include "xvalues_ref.h"
#include <math.h>

#include <sys/mman.h>
#define CNB 60u
/* every section, and every render scene inside one, runs in a child process (fork): a scene starts from the process's own power-on state, as the hashes cloud/base3 made did, and what one
 * leaves in the engines' memory cannot reach the next. The counters and the render buffers are shared memory */
static struct {
    int ncheck, bad;
    uint64_t h[2];
    int32_t a[CNB * CTL], b[CNB * CTL];
} *sh;
static void ck(const char *what, int ok)
{
    sh->ncheck++;
    printf("xvalues: %-120s %s\n", what, ok ? "ok" : "FAIL");
    sh->bad += !ok;
}
static void in_child(void (*fn)(void))
{
    int st = 0;
    pid_t p;
    fflush(stdout);
    p = fork();
    if (p == 0) {
        fn();
        fflush(stdout);
        _exit(0);
    }
    waitpid(p, &st, 0);
    if (!WIFEXITED(st) || WEXITSTATUS(st)) {
        printf("xvalues: FAIL a section or scene ended with signal/status %d\n", st);
        sh->bad++;
    }
}
#define LBL_FIT (CARD_W - 7)
#define VAL_FIT (CARD_W - 8)

static void clean(void)
{
    memset(trk, 0, sizeof trk);
    memset(&trk[0].mdw, 0, sizeof trk[0].mdw);
    memset(mph, 0, sizeof mph);
    memset(mwork, 0, sizeof mwork);
    memset(&morph, 0, sizeof morph);
    memset(ins, 0, sizeof ins);
    memset(eqs, 0, sizeof eqs);
    memset(pan_x, 0, sizeof pan_x);
    memset(pan_gv, 0, sizeof pan_gv);
    memset(duck_st, 0, sizeof duck_st);
    memset(mx_sl, 0, sizeof mx_sl);
    memset(mx_ok, 0, sizeof mx_ok);
    memset(mx_was, 0, sizeof mx_was);
    memset(mod_mixmv, 0, sizeof mod_mixmv);
    mix_bal = 0;
    vage = 0;
    mod_seed = 0x2545F491u;
    chain_force = 0;
    duell_mode_force(DM_SPLIT);
    duell_mode_force(DM_STEREO);
    memset(dly_buf, 0, sizeof dly_buf);
    memset(&fx, 0, sizeof fx);
    memset(&fx_b, 0, sizeof fx_b);
    host_tracks_init();
    song.g[G_RTYPE] = 0;
}

/* ------------------------------------------------------------------------------------ TABLES --- */
static void t_tables(void)
{
    static const struct { int16_t mn, mx, df; } X[15] = {
        [X_ROUTE] = {0, 1, 0}, [X_LINK] = {0, 1, 1}, [X_KTRK2] = {-63, 63, 0}, [X_ENV2] = {-64, 63, 0}, [X_LFO1] = {-64, 63, 0}, [X_LFO2] = {-64, 63, 0},
        [X_SUBLVL] = {0, 127, 0}, [X_SUBOCT] = {0, 2, 1}, [X_SUBWAVE] = {0, 3, 0}, [X_SUBHARM] = {0, 127, 0},
        [X_CYMODE] = {0, 1, 0}, [X_CYRISE] = {0, 127, 10}, [X_CYFALL] = {0, 127, 70}, [X_CYCURVE] = {-64, 63, 0}, [X_CYSYNC] = {0, 1, 0},
    };
    uint32_t k, ok = 1, okl = 1, okv = 1, okt = 1, oki = 1, okx = 1, j;
    int8_t o[EXT_P_LEN] = {0};
    char v[8];
    const char *u;
    o[P_TWTYPE] = TT_LP;                                  /* (TWIN on, a sub: the tags are the rolling ones, not the frozen ones) */
    o[XW_BASE + X_SUBLVL] = 50;
    o[XW_BASE + X_KTRK2] = 10;
    for (k = 0; k < 15u; k++) {
        const param_desc_t *d = &XTP[k];
        mtag_t g = morph_tag(&ENG_ANALOG, o, XW_BASE + k, 1);
        ok &= d->label && d->label[0] && d->label[0] != '-' && d->min == X[k].mn && d->max == X[k].mx && d->def == X[k].df && d->def >= d->min && d->def <= d->max;
        okl &= text_w(&AF_S, d->label) <= LBL_FIT;
        for (j = 0; d->fmt == F_ENUM && j <= (uint32_t)d->max; j++)
            okv &= text_w(&AF_S, d->names[j]) <= VAL_FIT;
        okt &= g.kind == MK_CONT || g.kind == MK_HARD;
        oki &= d->min >= -128 && d->max <= 127;
        okx &= param_desc_of(0, XW_BASE + k) == d && track_desc(&trk[0], XW_BASE + k) == d;
    }
    ck("15 extension slots: a descriptor each (label, ranges and defaults of EXTENSION_BLOCK_PLAN 2A: CYCLE MODE LOOP, RISE 10, FALL 70, SUB OCTAVE -1, LINK ON)", ok);
    ck("  .. every label fits a 50 px card (small font), every value name 49 px", okl && okv);
    { uint32_t okd = 1; for (k = 0; k < 15u; k++) okd &= (uint32_t)(XTP[k].def + 64) == xb_def(k);
      ck("  .. the store's defaults (up_xb.h XB_DEFS, the value + 64 an absent slot means) equal the descriptors' defaults", okd); }
    ck("  .. every slot has a morph tag (continuous or a switch), every range fits the morph's int8 ends; param_desc_of / track_desc give XTP", okt && oki && okx);
    ck("  .. X15 is spare: a free descriptor, no tag", XTP[15].label[0] == '-' && morph_tag(&ENG_ANALOG, o, XW_BASE + 15u, 1).kind == MK_EX && !strcmp(XTP[X_ROUTE].names[1], "PARAL") &&
       !strcmp(XTP[X_SUBOCT].names[1], "-1") && !strcmp(XTP[X_SUBWAVE].names[3], "PULSE") && !strcmp(XTP[X_CYMODE].names[0], "LOOP"));
    ck("  .. the CYCLE times are note values with SYNC on (xdesc)", (clean(), XV(&trk[0], X_CYSYNC) = 1, xdesc(&trk[0], X_CYRISE)->names == ESYNC_NAMES && xdesc(&trk[0], X_CYFALL)->names == ESYNC_NAMES && xdesc(&trk[0], X_CYCURVE) == &XTP[X_CYCURVE]));
    /* the five ids */
    ck("TYPE (id 43): OFF LP BP HP NOTCH LADDER COMB = 0..6, default OFF", P_TWTYPE == 43 && TP[P_TWTYPE].fmt == F_ENUM && TP[P_TWTYPE].max == 6 && TP[P_TWTYPE].def == 0 && !strcmp(TP[P_TWTYPE].names[6], "COMB") &&
       !strcmp(TP[P_TWTYPE].names[5], "LADDER") && !strcmp(TP[P_TWTYPE].label, "TYPE"));
    ck("CUTOFF 2 (106), RES 2 (25), DRIVE (26), BLEND (101): stored ranges that hold every meaning, defaults 0", P_CUT2 == 106 && P_RES2 == 25 && P_DRV2 == 26 && P_BLEND == 101 &&
       TP[P_CUT2].min == -63 && TP[P_CUT2].max == 82 && TP[P_CUT2].def == 0 && TP[P_RES2].min == -64 && TP[P_RES2].max == 127 && TP[P_RES2].def == 0 &&
       TP[P_DRV2].min == 0 && TP[P_DRV2].max == 127 && TP[P_DRV2].def == 0 && TP[P_BLEND].min == -64 && TP[P_BLEND].max == 63 && TP[P_BLEND].def == 0);
    clean();
    {
        track_t *t = &trk[0];
        uint32_t a, b, c, d, e, f;
        const param_desc_t *d1, *d2, *d3, *d4, *d5, *d6;
        t->p[P_TWTYPE] = TT_LP;
        XV(t, X_LINK) = 1;
        d1 = track_desc(t, P_CUT2);
        XV(t, X_LINK) = 0;
        d2 = track_desc(t, P_CUT2);
        d3 = track_desc(t, P_RES2);
        t->p[P_TWTYPE] = TT_COMB;
        d4 = track_desc(t, P_CUT2);
        d5 = track_desc(t, P_RES2);
        d6 = track_desc(t, P_BLEND);
        a = !strcmp(d1->label, "OFFSET") && d1->min == -36 && d1->max == 36 && d1->fmt == F_DSEMI;
        b = !strcmp(d2->label, "CUTOFF") && d2->min == 0 && d2->max == UF_STEPS && d2->fmt == F_FILT;
        c = !strcmp(d3->label, "RES") && d3->min == 0 && d3->max == 127;
        d = !strcmp(d4->label, "TUNE") && d4->min == -24 && d4->max == 24;
        e = !strcmp(d5->label, "FDBK") && d5->min == -64 && d5->max == 63;
        f = d6 == &TP[P_BLEND];
        ck("CUTOFF 2 is OFFSET -63..+63 with LINK on, CUTOFF 0..82 with LINK off, TUNE on COMB; RES 2 is RES 0..127, FDBK -64..+63 on COMB (track_desc)", a && b && c && d && e && f);
        for (k = 0, okl = 1; k < 7u; k++)
            okl &= text_w(&AF_S, N_TWTYPE[k]) <= VAL_FIT;
        okl &= text_w(&AF_S, TWD_OFFSET.label) <= LBL_FIT && text_w(&AF_S, TWD_CUTOFF.label) <= LBL_FIT && text_w(&AF_S, TWD_TUNE.label) <= LBL_FIT && text_w(&AF_S, TWD_FDBK.label) <= LBL_FIT &&
               text_w(&AF_S, TP[P_TWTYPE].label) <= LBL_FIT && text_w(&AF_S, TWD_RES.label) <= LBL_FIT && text_w(&AF_S, TP[P_DRV2].label) <= LBL_FIT && text_w(&AF_S, TP[P_BLEND].label) <= LBL_FIT;
        ck("  .. their labels (TYPE CUTOFF OFFSET TUNE RES FDBK DRIVE BLEND) and the seven type names fit a card", okl);
    }
    param_format(&TWD_OFFSET, -36, v, &u);
    ck("F_DSEMI: -36 whole semitones \"-36\" st (1.0: 3 octaves, one step a semitone)", !strcmp(v, "-36") && !strcmp(u, "st"));
    {
        char v0[8], v1[8], v63[8];
        param_format(&TWD_OFFSET, 0, v0, &u);
        param_format(&TWD_OFFSET, 1, v1, &u);
        param_format(&TWD_OFFSET, 63, v63, &u);
        ck("  .. 0 \"0\", 1 \"+1\", a stored 63 past the end shows what the filter does, \"+36\"", !strcmp(v0, "0") && !strcmp(v1, "+1") && !strcmp(v63, "+36"));
        if (strcmp(v0, "0") || strcmp(v1, "+1") || strcmp(v63, "+36"))
            printf("xvalues:     got \"%s\" \"%s\" \"%s\"\n", v0, v1, v63);
    }
    /* ids 25 / 26 are no longer the scale's: ROOT and SCALE have descriptors of their own */
    ck("ROOT and SCALE: the instrument's own descriptors (SCALE_D 16 names, ROOT_D C..B), not ids; id 127 and the retired SCALE ordinal read as free", SCALE_D.max == 15 && ROOT_D.max == 11 &&
       param_desc_of(0, P_COUNT)->max == 0 && param_desc_of(0, P_COUNT)->label[0] == '-' && track_desc(&trk[0], P_COUNT)->max == 0 && P_DKNOTE == P_COUNT);
}

/* ------------------------------------------------------------------------------------ NOTHING --- */
static uint64_t scene_hash_run(void (*prep)(void))
{
    static const uint8_t C1[4] = {48, 55, 60, 64}, C2[3] = {67, 72, 76};
    static int32_t o[2 * CTL];
    uint64_t h = 0xCBF29CE484222325ull;
    uint32_t k, i, b, nb = 90u;
    clean();
    host_preset(&trk[0], scn_eng("ANALOG"), 1);
    host_preset(&trk[1], scn_eng("TRIO"), 1);
    for (k = 0; k < 2u; k++) {
        trk[k].p[P_VOICE] = V_POLY;
        trk[k].p[P_TRIM] = 110;
        trk[k].p[P_REV] = 50;
        trk[k].p[P_SUS] = 110;
        trk[k].p[P_REL] = 50;
    }
    trk[0].p[P_M1SRC] = MS_LFO;
    trk[0].p[P_M1DST] = MD_CUT;
    trk[0].p[P_M1AMT] = 20;
    if (prep)
        prep();
    for (b = 0; b < nb; b++) {
        if (b == 5u) {
            for (i = 0; i < 4u; i++)
                trk_note_on(&trk[0], C1[i], 100);
            for (i = 0; i < 3u; i++)
                trk_note_on(&trk[1], C2[i], 100);
        }
        if (b == 60u)
            for (i = 0; i < 4u; i++)
                trk_note_off(&trk[0], C1[i]);
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++)
            h = xv_fnv(h, (uint32_t)o[i]);
    }
    return h;
}
static void (*g_prep)(void);
static void scene_child(void) { sh->h[0] = scene_hash_run(g_prep); }
static uint64_t scene_hash(void (*prep)(void))
{
    g_prep = prep;
    in_child(scene_child);
    return sh->h[0];
}
static void roll_child(void) { sh->h[0] = xv_roll_hash(); }
static void matrix_child(void) { sh->h[0] = xv_matrix_hash(); }
static uint64_t hash_c(void (*fn)(void))
{
    in_child(fn);
    return sh->h[0];
}
static void prep_wild(void)
{
    uint32_t k, x, i;
    for (x = 0; x < 2u; x++) {
        track_t *t = &trk[x];
        t->p[P_RES2] = 77; t->p[P_DRV2] = 90; t->p[P_BLEND] = -20; t->p[P_CUT2] = 14;      /* TYPE stays OFF: nothing of it is read */
        for (k = 0; k < 15u; k++)
            XV(t, k) = (int16_t)(XTP[k].min + (int32_t)(k * 7u + 3u + x) % (XTP[k].max - XTP[k].min + 1));
        for (i = 0; i < 1u; i++)
            XV(t, X_LINK) = 0;
        XV(t, X_SUBLVL) = 0;                                /* (job F: a sub with LEVEL above 0 plays; its other values, and CYCLE's, are read by nothing at 0 / unread) */
    }
}
static void t_nothing(void)
{
    uint64_t a = scene_hash(0), b = scene_hash(prep_wild);
    ck("the five ids and the 15 slots at any value with TYPE OFF (SUB LEVEL 0, CYCLE read by no slot): a two-part render is bit for bit the one at their defaults", a == b);
    ck("  .. and the chain stays off: chain_on is 0 for every voice (the product runs the plain path)", (clean(), chain_on(&trk[0], &trk[0].v[0]) == 0 && !chain_force));
}

/* ------------------------------------------------------------------------------------ MORPH --- */
static int8_t *h_of(uint32_t pi, uint32_t x) { return mph[pi].h[x][mph[pi].top[x]]; }
static int8_t *hx_of(uint32_t pi, uint32_t x) { return mph[pi].hx[x][mph[pi].top[x] % XH]; }
/* a part with the TWIN FILTER on (TYPE ty) and the slots set: ANALOG, rebased for the morph */
static void twin_part(uint32_t pi, const char *eng, uint32_t type)
{
    track_t *t;
    clean();
    host_preset(&trk[pi], scn_eng(eng), 1);
    t = &trk[pi];
    t->p[P_TWTYPE] = (int16_t)type;
    t->p[P_RES2] = 50; t->p[P_DRV2] = 40; t->p[P_BLEND] = 10; t->p[P_CUT2] = -8;
    XV(t, X_ROUTE) = 0; XV(t, X_LINK) = 1; XV(t, X_KTRK2) = 20; XV(t, X_ENV2) = 30; XV(t, X_LFO1) = 15; XV(t, X_LFO2) = -10;
    XV(t, X_SUBLVL) = 60; XV(t, X_SUBHARM) = 40; XV(t, X_CYRISE) = 50; XV(t, X_CYFALL) = 60; XV(t, X_CYCURVE) = 10;
    mph[pi].n[0] = 1;
    morph_rebase_part(pi);
}
static void roll_end(uint32_t pi, uint32_t x, uint32_t seed, uint32_t amount, uint32_t cfg) { morph_roll_cfg(pi, x, seed, amount, cfg); }

static void t_morph_old(void)
{
    ck("the End B of every id but the five from a stored seed: cloud/base3's hash (24 seeds x 3 AMOUNTs x SMART / WILD x PITCH / ARP x both streams x 13 engines)", hash_c(roll_child) == 0x03e34858cc8b1a21ull);   /* (re-based on purpose 2026-10-21: ANALOG and TRIO rolls keep filter 1's cutoff steering; was cloud/base3's 0x8ac190ea492b678f) */
    {   /* the draws of the five ids' old tags are kept: ROOT / SCALE a pitch item, ALLOC a hard switch, the TYPE a fade on ANALOG */
        uint32_t seed, ok = 1, moved = 0;
        for (seed = 1; seed <= 300u; seed++) {
            int8_t o[P_COUNT] = {0}, a[P_COUNT], b[P_COUNT];
            uint32_t id;
            for (id = 0; id < P_COUNT; id++)
                o[id] = (int8_t)(id >= P_E0 ? ENG_ANALOG.edit[id - P_E0].def : TP[id].def);
            o[P_FREEA] = 60;
            o[P_ED_FLT] = 20;
            o[P_KTRK] = 0;
            uint8_t sv = mrc;
            mrc = MC_PIT;
            morph_roll_pure(o, a, &ENG_ANALOG, 100, seed, 0, 1);
            o[P_RES2] = 33; o[P_DRV2] = 21; o[P_TWTYPE] = 3; o[P_CUT2] = 9; o[P_BLEND] = 5;       /* the five at other values: every other id rolls alike */
            morph_roll_pure(o, b, &ENG_ANALOG, 100, seed, 0, 1);
            mrc = sv;
            for (id = 0; id < P_COUNT; id++)
                if (!xv_five(id) && a[id] != b[id])
                    ok = 0;
            moved += a[P_E0] != o[P_E0];
        }
        ck("  .. the five ids' values do not move any other id's End B (the old draws are kept, the new ones come from a stream of their own)", ok && moved > 0);
    }
}

static void t_morph_slots(void)
{
    uint32_t pi = 0, ok, k, ok2;
    track_t *t;
    int8_t *a, *b;
    /* the ends and the blend of the continuous ones, the switches */
    twin_part(pi, "ANALOG", TT_LP);
    t = &trk[pi];
    a = hx_of(pi, 0);
    b = hx_of(pi, 1);
    ck("rebase: End A's slots are the Original (the base holds the wide ids), End B is the load's roll (two entries); the rings XH deep", !memcmp(a, mph[pi].base + XW_BASE, NX) && mph[pi].xv[0] == 1 &&
       mph[pi].xv[1] == 2 && mph[pi].n[1] == 2 && XH <= MORPH_HIST && !memcmp(mph[pi].base, t->p, P_COUNT * 0 + 1) && mph[pi].base[XW_BASE + X_SUBLVL] == 60 && mph[pi].base[P_CUT2] == -8);
    memcpy(h_of(pi, 1), mph[pi].base, P_COUNT);                /* End B: the Original again, then the differences set by hand */
    memcpy(b, mph[pi].base + XW_BASE, NX);
    a[X_SUBLVL] = 20; b[X_SUBLVL] = 100;                    /* continuous: blend */
    a[X_CYRISE] = 10; b[X_CYRISE] = 90;
    a[X_ROUTE] = 0; b[X_ROUTE] = 1;                         /* switches: the middle */
    a[X_LINK] = 1; b[X_LINK] = 0;
    a[X_SUBOCT] = 0; b[X_SUBOCT] = 2;
    a[X_SUBWAVE] = 1; b[X_SUBWAVE] = 3;
    a[X_CYMODE] = 0; b[X_CYMODE] = 1;
    a[X_CYSYNC] = 0; b[X_CYSYNC] = 1;
    h_of(pi, 0)[P_TWTYPE] = 2; h_of(pi, 1)[P_TWTYPE] = 5;   /* TYPE BP -> LADDER */
    h_of(pi, 0)[P_DRV2] = 20; h_of(pi, 1)[P_DRV2] = 80;
    h_of(pi, 0)[P_BLEND] = -30; h_of(pi, 1)[P_BLEND] = 30;
    h_of(pi, 0)[P_CUT2] = -20; h_of(pi, 1)[P_CUT2] = 20;     /* ends differ in LINK: CUTOFF 2 steps with it */
    h_of(pi, 0)[P_RES2] = 10; h_of(pi, 1)[P_RES2] = 90;
    morph_build(&mph[pi]);
    ok = 1;
    {
        static const uint8_t F[4] = {0, 40, 63, 127};
        uint32_t fi;
        for (fi = 0; fi < 4u; fi++) {
            int32_t w = morph_wb(F[fi]);
            int32_t want_sub = (20 * 256 + (((100 - 20) * 256 * w) >> 15) + 128) >> 8;
            mph[pi].fader = F[fi];
            if (!morph_begin(t, 1))
                ok = 0;
            ok &= XV(t, X_SUBLVL) == 60 + want_sub - 60;     /* live + (blend - base) with live = base */
            ok &= XV(t, X_ROUTE) == (w < 16384 ? 0 : 1) && XV(t, X_LINK) == (w < 16384 ? 1 : 0) && XV(t, X_SUBOCT) == (w < 16384 ? 0 : 2) &&
                  XV(t, X_SUBWAVE) == (w < 16384 ? 1 : 3) && XV(t, X_CYMODE) == (w < 16384 ? 0 : 1) && XV(t, X_CYSYNC) == (w < 16384 ? 0 : 1);
            ok &= t->p[P_TWTYPE] == (w < 16384 ? 2 : 5) && t->p[P_CUT2] == (w < 16384 ? -20 : 20) &&    /* TYPE, CUTOFF 2 (LINK differs) */
                  t->p[P_RES2] == 50 + (((10 * 256 + (((90 - 10) * 256 * w) >> 15) + 128) >> 8) - 50);       /* RES 2 (BP vs LADDER: not COMB: blends) */
            ok &= t->p[P_DRV2] == 40 + (((20 * 256 + (((80 - 20) * 256 * w) >> 15) + 128) >> 8) - 40);
            morph_end(t);
            ok &= XV(t, X_SUBLVL) == 60 && XV(t, X_ROUTE) == 0 && XV(t, X_LINK) == 1 && t->p[P_TWTYPE] == TT_LP && t->p[P_DRV2] == 40 && t->p[P_CUT2] == -8 && t->p[P_RES2] == 50;
        }
    }
    ck("the slots' blend: continuous ones live + (blend - Original), the 7 switches (TYPE ROUTE LINK SUB OCTAVE / WAVE CYCLE MODE / SYNC) flip at the middle of the fader, CUTOFF 2 with LINK; put back by morph_end", ok);
    /* RES 2 steps when exactly one end is COMB */
    h_of(pi, 1)[P_TWTYPE] = TT_COMB;
    h_of(pi, 0)[P_TWTYPE] = TT_LP;
    h_of(pi, 1)[P_RES2] = -40;
    morph_build(&mph[pi]);
    mph[pi].fader = 30;
    morph_begin(t, 1);
    ok = t->p[P_RES2] == 10;
    morph_end(t);
    mph[pi].fader = 100;
    morph_begin(t, 1);
    ok &= t->p[P_RES2] == -40;
    morph_end(t);
    ck("  .. RES 2 (RES vs FEEDBACK) and CUTOFF 2 do not blend across a COMB / LINK change: they step with the switch", ok);
    /* morph_view and the user's edits */
    h_of(pi, 1)[P_TWTYPE] = 5; h_of(pi, 1)[P_RES2] = 90; h_of(pi, 1)[P_CUT2] = -20; b[X_LINK] = 1;
    morph_build(&mph[pi]);
    mph[pi].fader = 127;
    ck("morph_view: a slot as the fader sounds it (SUB LEVEL 100 at B), the stored value for one the morph does not move; morph_view_moved", morph_view_on(pi) && morph_view(pi, XW_BASE + X_SUBLVL) == 100 &&
       morph_view(pi, XW_BASE + X_KTRK2) == 20 && morph_view_moved(pi, XW_BASE + X_SUBLVL) && !morph_view_moved(pi, XW_BASE + X_KTRK2) && morph_view(pi, P_COUNT) == 0);
    XV(t, X_SUBWAVE) = 2;                                    /* the user's own choice stays */
    morph_begin(t, 1);
    ok = XV(t, X_SUBWAVE) == 2;
    morph_end(t);
    ck("  .. a switch the user changed since the roll stays the user's (live != Original)", ok);
    (void)k; (void)ok2;
}

/* the History: XH deep */
static void t_morph_hist(void)
{
    uint32_t pi = 0, n, ok = 1, i;
    int8_t hist[12], hid[12];
    twin_part(pi, "ANALOG", TT_LP);
    hist[0] = hx_of(pi, 1)[X_ENV2];
    hid[0] = h_of(pi, 1)[P_DRV2];
    for (n = 1; n <= MORPH_HIST - 1u; n++) {
        roll_end(pi, 1, 100u + 37u * n, 60, 0);
        hist[n] = hx_of(pi, 1)[X_ENV2];
        hid[n] = h_of(pi, 1)[P_DRV2];
    }
    ok = mph[pi].n[1] == MORPH_HIST && mph[pi].xv[1] == XH;
    {   /* the ends differ from the Original (SMART rolls the continuous slots) */
        uint32_t moved = 0;
        for (n = 1; n <= MORPH_HIST - 1u; n++)
            moved += hist[n] != 30;
        ok &= moved > 0u;
    }
    ck("rolls (SMART, AMOUNT 60) move the slots' ends (ENV 2 and the rest), kept XH deep: xv", ok);
    for (n = MORPH_HIST - 1u; n >= 1u; n--) {
        ok = morph_undo_part(pi, 1) == 1;
        {
            uint32_t depth = MORPH_HIST - n;                        /* undos so far */
            int8_t want = depth <= XH - 1u ? hist[n - 1u] : 30;     /* beyond XH rolls: the slots at the Original (XH 3) */
            if (hx_of(pi, 1)[X_ENV2] != want || h_of(pi, 1)[P_DRV2] != hid[n - 1u])
                ok = 0;
        }
        if (!ok)
            break;
    }
    ck("UNDO walks the ends back: the ids always, the slots as far as XH reaches (then the Original's)", ok && mph[pi].n[1] == 1u);
    ck("  .. nothing more to undo", morph_undo_part(pi, 1) == 0);
    for (i = 0; i < 10u; i++)
        roll_end(pi, 0, 5u + i, 40, 0);
    ck("  .. rolls past MORPH_HIST wrap the rings together (slot index = id index mod XH)", mph[pi].n[0] == MORPH_HIST && mph[pi].top[0] == 1u && mph[pi].xv[0] == XH);
    printf("xvalues: morph_part_t %u bytes (XH %u), mwork_t %u, track_t %u, voice_t %u\n", (unsigned)sizeof(morph_part_t), (unsigned)XH, (unsigned)sizeof(mwork_t), (unsigned)sizeof(track_t), (unsigned)sizeof(voice_t));
}

/* the rolls' rules */
static void t_morph_rules(void)
{
    uint32_t seed, ok_type = 1, ok_off = 1, ok_res = 1, ok_dead = 1, ok_rise = 1, ok_sub = 1, ok_subw = 1, ok_comb = 1, ok_wild_off = 1, moved_type = 0, moved_amt = 0, ok_range = 1, ok_draw = 1;
    for (seed = 1; seed <= 400u; seed++) {
        uint32_t cfg;
        for (cfg = 0; cfg < 2u; cfg++) {
            int8_t o[EXT_P_LEN], end[EXT_P_LEN];
            const engine_t *en = seed % 5u == 0 ? &ENG_WAVE : seed % 7u == 0 ? &ENG_PHYS : &ENG_ANALOG;
            uint32_t id, amount = 20u + seed % 80u, ty = 1u + seed % 5u, k;
            for (id = 0; id < EXT_P_LEN; id++)
                o[id] = 0;
            for (id = 0; id < P_COUNT; id++)
                o[id] = (int8_t)(id >= P_E0 ? en->edit[id - P_E0].def : TP[id].def);
            for (k = 0; k < NX; k++)
                o[XW_BASE + k] = (int8_t)XTP[k].def;
            o[P_TWTYPE] = (int8_t)ty;
            o[P_RES2] = (int8_t)(seed % 120u); o[P_DRV2] = (int8_t)(seed * 3u % 100u); o[P_BLEND] = (int8_t)(seed % 100u - 40); o[P_CUT2] = (int8_t)(seed % 60u - 30);
            o[P_FREEA] = (int8_t)(seed % 83u);
            o[XW_BASE + X_KTRK2] = 20; o[XW_BASE + X_ENV2] = 30; o[XW_BASE + X_LFO1] = 15; o[XW_BASE + X_LFO2] = -12;
            o[XW_BASE + X_SUBLVL] = 50; o[XW_BASE + X_SUBHARM] = 30; o[XW_BASE + X_CYRISE] = 40; o[XW_BASE + X_CYFALL] = 60; o[XW_BASE + X_CYCURVE] = 8;
            {
                uint8_t sv = mrc;
                mrc = (uint8_t)(cfg ? MC_WILD : 0u);
                morph_roll_full(o, end, en, amount, seed, seed & 1u, 1);
                mrc = sv;
            }
            if (!cfg) {
                ok_type &= end[P_TWTYPE] == o[P_TWTYPE] && end[XW_BASE + X_ROUTE] == o[XW_BASE + X_ROUTE] && end[XW_BASE + X_LINK] == o[XW_BASE + X_LINK] && end[XW_BASE + X_SUBOCT] == o[XW_BASE + X_SUBOCT] &&
                           end[XW_BASE + X_SUBWAVE] == o[XW_BASE + X_SUBWAVE] && end[XW_BASE + X_CYMODE] == o[XW_BASE + X_CYMODE] && end[XW_BASE + X_CYSYNC] == o[XW_BASE + X_CYSYNC];
                moved_amt += end[XW_BASE + X_ENV2] != o[XW_BASE + X_ENV2];
                /* RES 2 above 100 never with more DRIVE; DRIVE at most 32 above the Original's */
                if (ty != TT_COMB && end[P_RES2] > (o[P_RES2] > 100 ? o[P_RES2] : 100) && end[P_DRV2] > o[P_DRV2])
                    ok_res = 0;
                ok_rise &= end[P_DRV2] <= o[P_DRV2] + 32 && end[XW_BASE + X_SUBLVL] <= o[XW_BASE + X_SUBLVL] + 48 && end[P_RES2] <= (o[P_RES2] > 108 ? o[P_RES2] : 108);
                {   /* no dead cutoff 2 */
                    int32_t eo = (o[XW_BASE + X_LINK] ? o[P_FREEA] + TW_OFF_STEPS(clamp(o[P_CUT2], -TW_OFF_MAX, TW_OFF_MAX)) : o[P_CUT2]), ee = (end[XW_BASE + X_LINK] ? end[P_FREEA] + TW_OFF_STEPS(clamp(end[P_CUT2], -TW_OFF_MAX, TW_OFF_MAX)) : end[P_CUT2]);   /* (1.0: OFFSET is whole semitones, the window is in dial steps) */
                    if (ty == TT_LP || ty == TT_LADDER)
                        ok_dead &= ee >= (eo < MORPH_UF_LO ? eo : MORPH_UF_LO) || end[P_CUT2] == (end[XW_BASE + X_LINK] ? TW_OFF_MAX : UF_STEPS);
                    else if (ty == TT_HP)
                        ok_dead &= ee <= (eo > MORPH_UF_HI ? eo : MORPH_UF_HI) || end[P_CUT2] == (end[XW_BASE + X_LINK] ? -TW_OFF_MAX : 0);
                    else if (ty == TT_BP)
                        ok_dead &= (ee >= (eo < MORPH_UF_BP0 ? eo : MORPH_UF_BP0) || end[P_CUT2] == (end[XW_BASE + X_LINK] ? TW_OFF_MAX : UF_STEPS)) &&
                                   (ee <= (eo > MORPH_UF_BP1 ? eo : MORPH_UF_BP1) || end[P_CUT2] == (end[XW_BASE + X_LINK] ? -TW_OFF_MAX : 0));
                }
            } else {
                moved_type += end[P_TWTYPE] != o[P_TWTYPE];
                ok_type &= end[P_TWTYPE] >= TT_LP && end[P_TWTYPE] <= TT_COMB;
                ok_comb &= (en != &ENG_WAVE && en != &ENG_PHYS) || end[P_TWTYPE] != TT_COMB || o[P_TWTYPE] == TT_COMB;
            }
            ok_range &= end[XW_BASE + X_SUBOCT] >= 0 && end[XW_BASE + X_SUBOCT] <= 2 && end[XW_BASE + X_SUBWAVE] >= 0 && end[XW_BASE + X_SUBWAVE] <= 3 && end[XW_BASE + X_ROUTE] >= 0 && end[XW_BASE + X_ROUTE] <= 1 &&
                        end[XW_BASE + X_LINK] >= 0 && end[XW_BASE + X_LINK] <= 1 && end[XW_BASE + X_CYMODE] >= 0 && end[XW_BASE + X_CYMODE] <= 1 && end[XW_BASE + X_CYSYNC] >= 0 && end[XW_BASE + X_CYSYNC] <= 1 &&
                        end[XW_BASE + X_CYRISE] >= 0 && end[XW_BASE + X_CYFALL] >= 0 && end[P_CUT2] >= -63 && end[P_CUT2] <= 82 && end[P_BLEND] >= -64 && end[P_BLEND] <= 63 && end[P_DRV2] >= 0 && end[P_RES2] >= -64 &&
                        (end[XW_BASE + X_LINK] || end[P_CUT2] >= 0 || o[P_CUT2] < 0 || end[P_TWTYPE] == TT_COMB);
            {   /* TYPE OFF: never switched on; SMART leaves TWIN's values alone, WILD rolls them but TYPE stays OFF */
                int8_t o2[EXT_P_LEN], e2[EXT_P_LEN];
                uint8_t sv = mrc;
                memcpy(o2, o, sizeof o2);
                o2[P_TWTYPE] = 0;
                mrc = (uint8_t)(cfg ? MC_WILD : 0u);
                morph_roll_full(o2, e2, en, amount, seed, 0, 1);
                mrc = sv;
                ok_off &= e2[P_TWTYPE] == 0;
                if (!cfg)
                    ok_wild_off &= e2[P_CUT2] == o2[P_CUT2] && e2[P_RES2] == o2[P_RES2] && e2[P_DRV2] == o2[P_DRV2] && e2[P_BLEND] == o2[P_BLEND] && e2[XW_BASE + X_ENV2] == o2[XW_BASE + X_ENV2] &&
                                   e2[XW_BASE + X_KTRK2] == o2[XW_BASE + X_KTRK2] && e2[XW_BASE + X_LFO1] == o2[XW_BASE + X_LFO1];
            }
            {   /* a sub is never switched on: LEVEL 0 keeps the sub's values */
                int8_t o2[EXT_P_LEN], e2[EXT_P_LEN];
                uint8_t sv = mrc;
                memcpy(o2, o, sizeof o2);
                o2[XW_BASE + X_SUBLVL] = 0;
                mrc = (uint8_t)(cfg ? MC_WILD : 0u);
                morph_roll_full(o2, e2, en, amount, seed, 0, 1);
                mrc = sv;
                ok_sub &= e2[XW_BASE + X_SUBLVL] == 0 && e2[XW_BASE + X_SUBHARM] == o2[XW_BASE + X_SUBHARM] && e2[XW_BASE + X_SUBOCT] == o2[XW_BASE + X_SUBOCT] && e2[XW_BASE + X_SUBWAVE] == o2[XW_BASE + X_SUBWAVE];
            }
            {   /* the stream does not depend on the Original (the draws of the five ids and the slots are made whether or not a value rolls) */
                int8_t e3[EXT_P_LEN];
                uint8_t sv = mrc;
                mrc = (uint8_t)(cfg ? MC_WILD : 0u);
                morph_roll_full(o, e3, en, amount, seed, seed & 1u, 1);
                mrc = sv;
                ok_draw &= !memcmp(e3, end, sizeof e3);
            }
        }
    }
    ck("SMART never rolls TYPE, ROUTE, LINK, SUB OCTAVE / WAVE, CYCLE MODE / SYNC (400 seeds, AMOUNT 20..99, ANALOG / WAVE / PHYS)", ok_type);
    ck("  .. a roll never switches TWIN on: TYPE OFF stays OFF in SMART and in WILD; WILD flips TYPE only among the types (never OFF, never COMB on WAVE and PHYS)", ok_off && moved_type > 100u && ok_comb);
    ck("  .. SMART leaves TWIN's values (CUTOFF 2, RES 2, DRIVE, BLEND, the amounts) alone while TYPE is OFF; with it on the amounts roll like filter 1's depths", ok_wild_off && moved_amt > 100u);
    ck("  .. RES 2 above 100 never with more DRIVE; DRIVE never 32 above the Original's, SUB LEVEL never 48 above, RES 2 never above max(Original, 108)", ok_res && ok_rise);
    ck("  .. no dead cutoff 2: LP / LADDER at or above min(Original's, ~1.3 kHz), HP at or below max(Original's, ~210 Hz), BP in its window (CUTOFF 2 at its limit when it cannot)", ok_dead);
    ck("  .. a sub is never switched on: SUB LEVEL 0 stays 0 and its WAVE / OCTAVE / HARMONICS stay, in WILD too", ok_sub);
    ck("  .. every end inside the ranges of its meaning (switches 0..n, CUTOFF 2 -63..82, LINK off 0..82); the draws are a pure function of the seed and the Original", ok_range && ok_draw);
}

/* ------------------------------------------------------------------------------------ MATRIX --- */
static void slot(track_t *t, uint32_t k, int32_t s, int32_t d, int32_t a)
{
    t->p[MSLOT_ID(k)] = (int16_t)s;
    t->p[MSLOT_ID(k) + 1u] = (int16_t)d;
    t->p[MSLOT_ID(k) + 2u] = (int16_t)a;
}
static uint64_t second_hash(void (*prep)(void))
{
    return scene_hash(prep);
}
static void prep_f2routes(void)
{
    track_t *t = &trk[0];
    slot(t, 1, MS_ENV, MD_CUT2, 20);
    slot(t, 2, MS_VEL, MD_CUT2, -30);
    slot(t, 3, MS_KEY, MD_CUT2, 10);
    slot(t, 4, MS_RAND, MD_CUT2, 5);
    slot(t, 5, MS_ENV2, MD_CUT2, 7);
    slot(t, 6, MS_CYCLE, MD_CUT2, 50);
    slot(t, 7, MS_LFO, MD_CUT2, 33);
}
static void prep_cycleroutes(void)
{
    track_t *t = &trk[0];
    slot(t, 1, MS_CYCLE, MD_CUT, 40);
    slot(t, 2, MS_CYCLE, MD_SHP, -30);
    slot(t, 3, MS_CYCLE, MD_PITCH, 12);
    slot(t, 4, MS_CYCLE, MD_PAN, 20);                       /* per block: the latest note's CYCLE (0) times nothing */
}
static void t_matrix(void)
{
    uint64_t base = scene_hash(0);
    track_t *t;
    uint32_t ok;
    ck("a busy two-part matrix render (every per-note destination from ENV, ENV 2, VEL, KEY, RAND, the LFOs; PAN, sends, E5, RES, FMODE) is cloud/base3's hash: mod_voice runs as it did", hash_c(matrix_child) == 0x8c526ee5e3c74750ull);   /* (re-based on purpose 2026-10-21: the click fixes of hunt1..hunt4 (LFO / matrix slew, SAT, PHASE seam); was cloud/base3's 0xec458fefd57d01b4; and for 1.0: the matrix amount +63 acts as a true 100 %, mod.c MX_AMT; was 0x4396c17e8041202e) */
    ck("CUTOFF 2 routes (ENV VEL KEY RAND ENV 2 CYCLE LFO) change no sample of the product: the second list is read by the chain only", second_hash(prep_f2routes) == base);
    ck("CYCLE to CUT, SHP, PITCH (per note) and PAN (per block): a working source since job F (cycle.c): it moves the sound (job B's stub at rest changed no sample)", second_hash(prep_cycleroutes) != base);
    /* the lists */
    clean();
    host_preset(&trk[0], scn_eng("ANALOG"), 1);
    t = &trk[0];
    prep_f2routes();
    slot(t, 0, MS_CYCLE, MD_PITCH, 9);
    t->p[P_E0] = t->p[P_E0];
    trk_note_on(t, 60, 100);
    {
        static int32_t o[2 * CTL];
        uint32_t b, m;
        for (b = 0; b < 40u; b++)
            mix_block(o, CTL);
        m = t->f2m;
        ok = m == 0x7Eu && t->cym == 0x01u && t->cy == 1u && MODW(t).nv == 0u && t->f2add != 0;
        ck("mod_begin: the slots to CUTOFF 2 from per-voice sources (1..6) are the second list (f2m), the LFO's is summed (f2add); CYCLE to PITCH is cym; CYCLE read (cy); the hot list (nv) empty", ok);
        if (!ok)
            printf("xvalues:     f2m %02x cym %02x cy %u nv %u on %u f2add %d\n", (unsigned)m, (unsigned)t->cym, (unsigned)t->cy, (unsigned)MODW(t).nv, (unsigned)MODW(t).on, (int)t->f2add);
        {
            voice_t *v = &t->v[t->m_vi];
            int32_t sum = t->f2add, x;
            t->venv = 12345;
            x = (12345 * 20 >> 7) + ((v->mvel * 258) * -30 >> 7) + (mod_key(v->note) * 10 >> 7) + (v->mrnd * 5 >> 7) + ((v->env2 >> 9) * 7 >> 7) + (cycle_value(t, v) * 51 >> 7);   /* (50 acts as 51: MX_AMT, 1.0: +63 is a true 100 %) */
            ck("mod_f2_voice: f2add + each slot's x * AMT >> 7 as mod_voice_add's CUT (ENV, VEL, KEY, RAND, ENV 2, CYCLE: cycle_value)", mod_f2_voice(t, v) == sum + x);
        }
        trk_note_off(t, 60);
    }
    /* mod_cycle_voice: the same sums as mod_voice_add and the same re-computations; a CYCLE source at 0 */
    {
        vmod_t m0, m1;
        voice_t v;
        int32_t fine = 5;
        track_t tt;
        memset(&tt, 0, sizeof tt);
        memset(&v, 0, sizeof v);
        tt.v[0].note = 60;
        tt.m_vi = 0;
        slot(&tt, 0, MS_CYCLE, MD_CUT, 40);
        slot(&tt, 1, MS_CYCLE, MD_SHP, -30);
        slot(&tt, 2, MS_CYCLE, MD_PITCH, 12);
        slot(&tt, 3, MS_CYCLE, MD_AMP, 20);
        tt.cym = 0x0Fu;
        tt.cy = 1;
        memset(&m0, 0, sizeof m0);
        m0.pitch16 = 960; m0.inc = pitch_inc(960); m0.cutoff = 1000; m0.shape = 64 << 8; m0.amp0 = 20000; m0.amp1 = 30000;
        m1 = m0;
        mod_cycle_voice(&tt, &tt.v[0], &m1, fine);
        ok = m1.cutoff == m0.cutoff && m1.shape == m0.shape && m1.pitch16 == m0.pitch16 && m1.amp1 == mulq15(m0.amp1, mulq15(32767, mod_gain(MS_CYCLE, 0, 20))) && tt.v[0].env_out == m1.amp1 && tt.m_cycle == 0;
        ck("mod_cycle_voice: a CYCLE at rest adds 0 to cutoff, shape, pitch and closes an AMP route by mod_gain; env_out follows; t->m_cycle kept", ok);
        if (!ok)
            printf("xvalues:     cut %d/%d shp %d/%d pit %d/%d amp1 %d want %d env_out %d\n", (int)m1.cutoff, (int)m0.cutoff, (int)m1.shape, (int)m0.shape, (int)m1.pitch16, (int)m0.pitch16, (int)m1.amp1, (int)mulq15(m0.amp1, mulq15(32767, mod_gain(MS_CYCLE, 0, 20))), (int)tt.v[0].env_out);
        tt.cym = 0;
        m1 = m0;
        v = tt.v[0];
        mod_cycle_voice(&tt, &tt.v[0], &m1, fine);
        ck("  .. with the list empty it changes nothing", !memcmp(&m1, &m0, sizeof m0));
        (void)v;
    }
    /* per-block destinations */
    clean();
    host_preset(&trk[0], scn_eng("ANALOG"), 1);
    t = &trk[0];
    t->p[P_TWTYPE] = TT_LP;
    slot(t, 0, MS_LFO, MD_DRIVE, 32);
    slot(t, 1, MS_LFO, MD_RES2, -16);
    slot(t, 2, MS_LFO, MD_BLEND, 20);
    slot(t, 3, MS_LFO, MD_SUBLVL, 30);
    t->lfo_val = 20000; t->lfo_fade = 32767;
    XV(t, X_SUBLVL) = 50;
    t->p[P_RES2] = 60; t->p[P_DRV2] = 10; t->p[P_BLEND] = -5;
    ck("mod_begin: DRIVE, RES 2, BLEND and SUB LEVEL (destinations 53, 52, 54, 55) are written into t->p for the block ..", mod_begin(t) == 1 &&
       t->p[P_DRV2] == 10 + (((20000 * 32 >> 7) * 127) >> 15) && t->p[P_RES2] == 60 + (((20000 * -16 >> 7) * 127) >> 15) && t->p[P_BLEND] == -5 + (((20000 * 20 >> 7) * 127) >> 15) &&
       XV(t, X_SUBLVL) == 50 + (((20000 * 30 >> 7) * 127) >> 15) && MODW(t).nk == 4u);
    mod_end(t);
    ck("  .. and put back by mod_end (the wide id sits in the byte table)", t->p[P_DRV2] == 10 && t->p[P_RES2] == 60 && t->p[P_BLEND] == -5 && XV(t, X_SUBLVL) == 50);
    t->p[P_TWTYPE] = TT_COMB;
    t->p[P_RES2] = 0;
    t->lfo_val = 20000;
    mx_ok[0] = 0;
    mod_begin(t);
    ok = t->p[P_RES2] == (((20000 * -16 >> 7) * 127) >> 15);
    mod_end(t);
    ck("  .. RES 2 on COMB moves over FEEDBACK's range (-64..63: 127 wide) and is clamped to it", ok);
    /* offered, named */
    clean();
    t = &trk[0];
    host_preset(t, scn_eng("ANALOG"), 1);
    ok = 1;
    ok &= !mod_dst_offered(t, MD_CUT2) && !mod_dst_offered(t, MD_RES2) && !mod_dst_offered(t, MD_DRIVE) && !mod_dst_offered(t, MD_BLEND) && mod_dst_offered(t, MD_SUBLVL);
    ok &= !strcmp(mod_dst_why(t, MD_DRIVE), "TWIN OFF");
    t->p[P_TWTYPE] = TT_LADDER;
    ok &= mod_dst_offered(t, MD_CUT2) && mod_dst_offered(t, MD_RES2) && mod_dst_offered(t, MD_DRIVE) && mod_dst_offered(t, MD_BLEND) && mod_dst_offered(t, MD_SUBLVL) && !mod_dst_why(t, MD_BLEND);
    ck("the TWIN destinations are offered only while TYPE is not OFF (SUB LEVEL always); a stored route keeps playing (the matrix still runs it)", ok);
    {
        char b[40];
        const char *more;
        int ok2 = 1;
        uint32_t d;
        for (d = MD_CUT2; d <= MD_SUBLVL; d++) {
            mod_dst_line(t, (int32_t)d, b, &more);
            ok2 &= strlen(b) <= 31u && text_w(&AF_S, mod_dst_name(t, (int32_t)d)) < 200;
        }
        ok2 &= !strcmp(mod_dst_name(t, MD_CUT2), "TWIN\nCUTOFF") && !strcmp(mod_dst_name(t, MD_BLEND), "FILTER\nBLEND") && !strcmp(N_MDST[MD_DRIVE], "FDRV") && !strcmp(N_MSRC[MS_CYCLE], "CYCLE");
        t->p[P_TWTYPE] = TT_COMB;
        ok2 &= !strcmp(mod_dst_name(t, MD_CUT2), "TWIN\nTUNE") && !strcmp(mod_dst_name(t, MD_RES2), "TWIN\nFDBK");
        mod_dst_line(t, MD_CUT2, b, &more);
        ok2 &= !strcmp(b, "DEST: TWIN FILTER TUNE");
        t->p[P_TWTYPE] = TT_OFF;
        mod_dst_line(t, MD_RES2, b, &more);
        ok2 &= !strcmp(b, "DEST: TWIN FILTER RES") && more && !strcmp(more, "THE TWIN FILTER IS OFF");
        ck("  .. their names (TWIN / CUTOFF, TWIN / RES, FILTER / DRIVE, FILTER / BLEND, SUB / LEVEL; TUNE and FDBK on COMB), the DEST lines (32 characters), N_MDST's factory names", ok2);
    }
    {   /* the lists */
        uint32_t i, seen[MD_N] = {0}, ok3 = 1, ok4 = 1;
        for (i = 0; i < MD_N; i++) {
            ok3 &= MDST_ORDER[i] < MD_N && !seen[MDST_ORDER[i]]++;
        }
        for (i = 0; i < MS_N; i++)
            ok4 &= MSRC_ORDER[i] < MS_N;
        ck("the knob orders hold every destination (56) and source (16) once; CYCLE sits after ENV2; MS_VOICE counts it", ok3 && ok4 && MSRC_ORDER[3] == MS_CYCLE && MSRC_ORDER[2] == MS_ENV2 && MS_VOICE(MS_CYCLE) &&
           MD_N == 56 && MS_N == 16 && MD_CUT2 == 51 && MD_SUBLVL == 55 && MS_CYCLE == 15);
    }
}

/* ------------------------------------------------------------------------------------ CHAIN --- */
static int f1_mode;                                      /* 1: filter 1 on (LP -> BP blend, CUT 40) for the whole note; 2: the cutoff opens to the bypass and closes again (block 20 .. 30) */
static void chain_scene_run(int32_t *out, const char *eng, uint32_t preset, uint32_t mode, uint32_t spread, void (*prep)(void))
{
    static const uint8_t C1[3] = {48, 55, 64};
    static int32_t o[2 * CTL];
    uint32_t b, i;
    clean();
    host_preset(&trk[0], scn_eng(eng), preset);
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk[0].p[P_SUS] = 100;
    trk[0].p[P_REL] = 40;
    trk[0].p[P_SPRD] = (int16_t)(spread ? 100 : 0);
    if (prep)
        prep();
    if (f1_mode) {
        trk[0].p[P_FREEA] = 40; trk[0].p[P_FREEB] = 50; trk[0].p[P_FREEC] = 20; trk[0].p[P_FREED] = 1;
    }
    for (b = 0; b < CNB; b++) {
        if (f1_mode == 2)
            trk[0].p[P_FREEA] = (int16_t)(b >= 20u && b < 30u ? UF_STEPS : 40);
        if (b == 2u)
            for (i = 0; i < 3u; i++)
                trk_note_on(&trk[0], C1[i], 70u + 20u * i);
        if (b == 40u)
            for (i = 0; i < 3u; i++)
                trk_note_off(&trk[0], C1[i]);
        if (mode == 1u)
            chain_force = 1;
        if (mode == 2u)
            chain_force = (uint8_t)(b >= 10u && b < 24u);   /* a sounding voice enters the chain and leaves it again */
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            out[b * CTL + i] = o[2u * i];
    }
}
static int32_t *c_out;
static const char *c_eng;
static uint32_t c_preset, c_mode, c_spread;
static void (*c_prep)(void);
static void chain_child(void) { chain_scene_run(c_out, c_eng, c_preset, c_mode, c_spread, c_prep); }
static void chain_scene(int32_t *out, const char *eng, uint32_t preset, uint32_t mode, uint32_t spread, void (*prep)(void))
{
    c_out = out; c_eng = eng; c_preset = preset; c_mode = mode; c_spread = spread; c_prep = prep;
    in_child(chain_child);
}
static double rms(const int32_t *x, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += (double)x[i] * x[i];
    return sqrt(s / n);
}
static void t_chain(void)
{
    int32_t *cl = sh->a, *cr = sh->b;
    uint32_t e, ok = 1, ok_e = 1, n = CNB * CTL, ok_s = 1, ok_edge = 1;
    double worst = 0, worst_edge = 0, peakmax = 0;
    for (e = 0; e < NENGINES; e++) {
        uint32_t k;
        if (ENGI_RETIRED(e))
            continue;
        for (k = 0; k < 2u; k++) {
            uint32_t i;
            double pk = 0, d1 = 0, d2 = 0;
            chain_scene(cl, ENGINES[e]->name, k + 1u, 0, 0, 0);
            chain_scene(cr, ENGINES[e]->name, k + 1u, 1, 0, 0);
            for (i = 0; i < n; i++) {
                pk = fabs((double)cl[i]) > pk ? fabs((double)cl[i]) : pk;
                d1 = fabs((double)cl[i] - cr[i]) > d1 ? fabs((double)cl[i] - cr[i]) : d1;
            }
            peakmax = pk > peakmax ? pk : peakmax;
            if (pk > 50.0)
                worst = d1 / pk > worst ? d1 / pk : worst;
            { double tol = !strcmp(ENGINES[e]->name, "FM6") ? 0.03 : 0.01;   /* FM6's own envelope ramps inside the render (a / da): the chain's per-sample ramp after it differs by up to ~2 % */
                ok &= pk > 50.0 && fabs(rms(cl, n) / rms(cr, n) - 1.0) < tol && d1 <= pk * tol + 8.0; }
            chain_scene(cr, ENGINES[e]->name, k + 1u, 2, 0, 0);
            for (i = 0; i < n; i++)
                d2 = fabs((double)cl[i] - cr[i]) > d2 ? fabs((double)cl[i] - cr[i]) : d2;
            worst_edge = d2 / pk > worst_edge ? d2 / pk : worst_edge;
            printf("xvalues:     %-7s preset %u: peak %6.0f plain-vs-chain %.4f enter/leave %.4f spread-diff below\n", ENGINES[e]->name, (unsigned)k + 1u, pk, d1 / pk, d2 / pk);
            ok_edge &= d2 <= pk * 0.02 + 8.0;
            chain_scene(cl, ENGINES[e]->name, k + 1u, 0, 1, 0);
            chain_scene(cr, ENGINES[e]->name, k + 1u, 1, 1, 0);
            for (i = 0, d1 = 0; i < n; i++)
                d1 = fabs((double)cl[i] - cr[i]) > d1 ? fabs((double)cl[i] - cr[i]) : d1;
            ok_s &= d1 <= pk * (!strcmp(ENGINES[e]->name, "FM6") ? 0.03 : 0.01) + 8.0 && fabs(rms(cl, n) / rms(cr, n) - 1.0) < (!strcmp(ENGINES[e]->name, "FM6") ? 0.03 : 0.01);
        }
    }
    printf("xvalues: chain forced on, stages empty, filter 1 bypassed: the largest difference to the plain path %.4f of the peak (peak %.0f); across an enter / leave %.4f\n", worst, peakmax, worst_edge);
    ck("the voice chain forced on (engine at unity, filter 1 bypassed, the real amp ramp after, added to the part's block) sounds as the plain path within rounding on every engine (rms 1 %, peak diff 1 %, FM6 3 %)", ok);
    ck("  .. with SPREAD 100 (the voices of one side in the part's second buffer): the same", ok_s);
    ck("  .. a sounding voice that enters the chain (block 10) and leaves it (block 24) is cross-faded over one block both ways: the output stays within 2 % of the plain path's peak", ok_edge);
    (void)ok_e;
    /* v->chn */
    {
        static int32_t o[2 * CTL];
        uint32_t b, st[8] = {0};
        clean();
        host_preset(&trk[0], scn_eng("ANALOG"), 1);
        trk[0].p[P_SUS] = 100;
        trk_note_on(&trk[0], 60, 100);
        st[0] = trk[0].v[0].chn;
        chain_force = 1;
        mix_block(o, CTL);
        st[1] = trk[0].v[0].chn;
        mix_block(o, CTL);
        st[2] = trk[0].v[0].chn;
        chain_force = 0;
        mix_block(o, CTL);
        st[3] = trk[0].v[0].chn;
        mix_block(o, CTL);
        st[4] = trk[0].v[0].chn;
        (void)b;
        ck("v->chn: a note starts FRESH, a block in the chain makes it RUN, a block on the plain path OFF (voice_chain.h); a note started on the plain path is OFF after its first block", st[0] == VCH_FRESH && st[1] == VCH_RUN && st[2] == VCH_RUN && st[3] == VCH_OFF && st[4] == VCH_OFF);
    }
    /* filter 1 inside the chain (uf_run on the scratch, unity level): it sounds, and follows the plain path (the filter is linear below its soft knee); its bypass edge is cross-faded as uf_edge does it */
    {
        uint32_t m, i, k, ok_f = 1;
        double worst1 = 0;
        static const char *const ENGN[3] = {"PHASE", "LOFI", "WAVE"};
        for (m = 1; m <= 2u; m++)
            for (k = 0; k < 3u; k++) {
                double pk = 0, d = 0;
                f1_mode = (int)m;
                chain_scene(cl, ENGN[k], 1, 0, 0, 0);
                chain_scene(cr, ENGN[k], 1, 1, 0, 0);
                f1_mode = 0;
                for (i = 0; i < n; i++) {
                    pk = fabs((double)cl[i]) > pk ? fabs((double)cl[i]) : pk;
                    d = fabs((double)cl[i] - cr[i]) > d ? fabs((double)cl[i] - cr[i]) : d;
                }
                worst1 = pk > 50.0 && d / pk > worst1 ? d / pk : worst1;
                ok_f &= pk > 50.0 && d <= pk * 0.5 + 8.0 && fabs(rms(cl, n) / rms(cr, n) - 1.0) < 0.5;   /* the chain filters the unity-level block (the soft knee meets 2x the level), then the amp */
            }
        printf("xvalues: chain with filter 1 running (and its bypass edge): the largest difference to the plain path %.4f of the peak (PHASE, LOFI, WAVE)\n", worst1);
        ck("  .. filter 1 (LP -> BP, CUT 40) run by the chain on the unity-level scratch follows the plain path within 50 % of the peak (the soft knee sees the unity level), rms within 50 %, also across its bypass edge (CUT opened and closed while a note sounds)", ok_f);
    }
    chain_force = 0;
}

/* the loader (task 5): built only against the header tools/gen_duell_sounds.py makes from a list with one more sound that names extension slots (host_tests.sh: -DXV_LOADER, -Ibuild/gen_xv) */
#ifdef XV_LOADER
static void t_loader(void)
{
    uint32_t i, e = 99, k = 0;
    static track_t T;
    for (i = 0; i < DS_N; i++)
        if (!strcmp(DS_SOUNDS[i].name, "XLOADTEST")) {
            e = DS_SOUNDS[i].eng;
            k = i - DS_ENG_AT[e];
        }
    ck("loader: the generated header carries the test sound (wide ids 128.. in its pairs, as XW_BASE + X_...)", e != 99u);
    if (e == 99u)
        return;
    host_tracks_init();
    memset(&T, 0, sizeof T);
    host_ds_load(&T, e, k);
    ck("loader: ds_put routes the wide ids into XV (SUB LEVEL 80, CYCLE RISE 33, TWIN LINK OFF, CYCLE MODE ONCE)",
       XV(&T, X_SUBLVL) == 80 && XV(&T, X_CYRISE) == 33 && XV(&T, X_LINK) == 0 && XV(&T, X_CYMODE) == 1);
    ck("loader: the other slots stay at their defaults, id 127 stays 0, ids below keep their values",
       XV(&T, X_CYFALL) == 70 && T.p[P_COUNT] == 0 && T.p[P_TWTYPE] == TT_OFF);
}
#endif
int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    sh = mmap(0, sizeof *sh, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sh == MAP_FAILED)
        return 2;
    in_child(t_tables);
    in_child(t_nothing);
    in_child(t_morph_old);
    in_child(t_morph_slots);
    in_child(t_morph_hist);
    in_child(t_morph_rules);
    in_child(t_matrix);
    in_child(t_chain);
#ifdef XV_LOADER
    in_child(t_loader);
#endif
    printf("xvalues: %d checks, %d failed\n", sh->ncheck, sh->bad);
    return sh->bad ? 1 : 0;
}
