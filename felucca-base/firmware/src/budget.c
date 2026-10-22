/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL NOTES budget (docs/CPU_PLAN.md section 5, MIXSPLIT job 4; the API and the units: budget.h). The number of notes each part may play at
 * once, from what is switched on in the settings, so that every note the budget allows can sound together without the audio running out of
 * time: the idle mix + the sends and master stages + each part's own stages + the voices of both parts <= BUDGET_LIMIT of the time budget.
 *
 *   notes(part) = floor((limit - global - other part's stages - this part's stages - other part's notes x their cost) / this part's cost)
 *
 * Here the "other part's notes" are the same number n (or its own limit, if lower): a part's number is the most notes it can play while the other part
 * plays as many (budget_solve). The worst case is taken over every way the shared voice pool can be split (a + b over NVOICE cannot all sound at once). That
 * makes the numbers depend on the settings only, not on the order of the calls, on what plays or on a half-solved other number, and switching an item on
 * can only lower them (an earlier version that gave the cheaper part the leftover notes raised one part's number when the other got dearer). A part that
 * is muted costs nothing. Never below 1 a part.
 * This is the ONE-core definition: one core runs everything. Nothing here runs per sample: flash code, called when a setting changes
 * (budget_poll, once a UI frame) and by the tests; the note-on path (voice.c trk_nvoice) reads the numbers it left in bnotes[].
 *
 * The cost table is ONE table, in the units of tests/bench_baseline.txt (host instructions a sample, the bench's Mac figures); BD_U turns
 * them into 1/100 % and applies BUDGET_MARGIN. Where a line says "bench" it is a row of that file; the other lines are marked:
 *   (lin)  measured on Linux with valgrind (callgrind) in the bench's way, 4 ANALOG voices, a slot / an LFO / the arp against none: gcc's
 *          counts differ from the Mac's (the multimode filter and the EQ read 1.5 to 2 x there), so only the ratios were used;
 *   (old)  the VOICE CAP's figures of 0.2.11 (voice.c VC_MORPH, VC_FADE: 0.5 % and 2 % a voice).
 * They are for the model only: the unit is not measured yet (stage 2 of CPU_PLAN). The device runs 0.75 to 0.9 of these; the margin and
 * the 20 % kept back are the cover. */

#include "budget.h"

#define BD_U(i) ((((uint32_t)(i)) * 64u * BUDGET_MARGIN + 2099u) / 2100u)   /* host instructions a sample -> 1/100 % (3281.25 = 100 %), margin applied, rounded up */

/* ----------------------------------------------------------------- the table (host instructions a sample) --- */
/* one voice of an engine: bench voice/<ENGINE> is its dearest factory sound, and every factory sound runs the 12 dB multimode filter (KEY TRK 32 keeps it
 * out of the bypass), so the filter's BD_F12 is taken out of the row here and put back by the slope the part really has */
#define BD_F12 39u              /* the multimode filter a voice, 12 dB: 4.8 % for 4 voices (uf_test.c: under 70 a voice) */
#define BD_F24 71u              /* .. 24 dB: 8.6 % for 4 voices (uf_test.c: under 110 a voice) */
/* cloud job J (the engine filters left): ANALOG, TRIO and NOISE no longer run a filter of their own, so their rows drop by what it cost (est.: the mid values of TWIN_FILTER_DESIGN_2 4.1, 13 .. 20,
 * 20 .. 26 and 23 .. 30 host instructions a sample; no instruction counter here, the owner's Mac bench replaces them). Their filter is filter 1 now (bd_slope), which the bench rows already
 * had in (KEY TRK 32 kept it out of the bypass) */
#define BD_RM_ANALOG 17u        /* est. */
#define BD_RM_TRIO 23u          /* est. */
#define BD_RM_NOISE 27u         /* est. */
static const struct { const engine_t *e; uint16_t c; } BD_ENG[] = {
    {&ENG_ANALOG, 235 - BD_F12 - BD_RM_ANALOG}, {&ENG_FM6, 266 - BD_F12}, {&ENG_PHASE, 210 - BD_F12}, {&ENG_LOFI, 159 - BD_F12}, {&ENG_FORMANT, 270 - BD_F12},   /* (FORMANT is the engine VOICE) */
    {&ENG_TRIO, 238 - BD_F12 - BD_RM_TRIO}, {&ENG_WHEEL, 253 - BD_F12}, {&ENG_PHYS, 511 - BD_F12}, {&ENG_NOISE, 232 - BD_F12 - BD_RM_NOISE}, {&ENG_WAVE, 234 - BD_F12},
};
#define BD_ENG_DEF (270u - BD_F12)   /* an engine not in the table: as the dearest of the normal ones */
#define BD_MORPH 16u            /* the MORPH a voice (old: 0.5 %) */
#define BD_FADE 66u             /* .. an engine choice cross-faded, both shapes made (old: 2 %) */
/* the part's own stages */
static const uint8_t BD_INS[] = {0, 79, 72, 74, 80, 48, 96, 73, 76, 94, 95};   /* bench insert/<TYPE>, P_ITYPE order (N_ITYPE): OFF SOFT HARD FOLD FUZZ CRUSH PHASR FLANG CHOR TAPE VINYL */
#define BD_INS_MAX 96u          /* a type out of the table: the dearest */
#define BD_SAT 49u              /* bench part/SAT */
#define BD_EQ_BASE 6u           /* bench part/EQ is 87 for three bands: this + 3 x BD_EQ_BAND (the lin ratio: a small fixed part) */
#define BD_EQ_BAND 27u          /* .. a band whose gain is not 0 (a flat band is not run) */
#define BD_DUCK 33u             /* bench part/DUCK */
#define BD_KTRK 1u              /* KEY TRK with the filter on: uf_test.c "under 1 instruction a sample" */
#define BD_MX_SLOT 8u           /* a matrix slot with SRC, DST and AMT set (lin: 7 a slot, 4 voices; the voice share is in the margin) */
#define BD_LFO 5u               /* an LFO that a slot reads (lin: under 5; LFO 1 / LFO 2) */
#define BD_ARP 18u              /* the arp's own step work a part (bench scene fixed, STEREO / DUEL: 36 for two parts) */
/* the mix with nothing playing, by MODE [0 STEREO and LAYER, 1 DUEL]: BD_IDLE is the mix with SENDS OFF (bench idle 455 less bench bus/STEREO/nothing_fed 221;
 * DUEL: CPU_PLAN section 1, 21.7 % less the bench's 492), then the buses (bench bus/<MODE>/<row>: the whole bus section against SENDS OFF with only that effect fed;
 * the delay and the reverb run whenever SENDS is ON, the chorus only while some part sends to it) */
static const uint16_t BD_IDLE[2] = {234, 220};
static const uint16_t BD_BUS[2] = {221, 492};                                   /* nothing fed, HALL + DIGITAL (the power-on) */
static const uint16_t BD_CHO[2] = {301, 561};                                   /* + the chorus fed */
static const uint16_t BD_DLY[2][4] = {{229, 256, 353, 352}, {500, 499, 622, 620}};   /* DIGITAL PING-PONG RANDOM OCT UP, fed */
static const uint16_t BD_REV[2][6] = {{187, 198, 221, 259, 177, 222}, {430, 458, 481, 579, 413, 482}};   /* ROOM SPRING HALL SHIM(UP) RESO PLATE, fed */
static const uint16_t BD_SHD[2] = {296, 652};                                   /* SHIM PITCH DOWN */
static const uint16_t BD_SHM[2] = {299, 659};                                   /* SHIM PITCH UP + MOD 50 (DOWN + MOD: the two extras added, not measured) */
#define BD_COMP 54u             /* bench master/COMP, WIDTH, BASSMONO (none runs in DUEL) */
#define BD_WIDTH 37u
#define BD_BMONO 38u

/* ----------------------------------------------------------------- the parts --- */
typedef struct {                /* one part's items, host instructions a sample (the ablations of budget_reason zero one) */
    uint32_t eng, filt, ext;    /* a voice: the engine, the multimode filter's slope, the morph's extras */
    uint32_t ins, sat, eq, misc;/* fixed: INSERT, SAT, EQ, the rest (ducker, KEY TRK, matrix, LFOs, arp) */
    uint32_t cap;               /* the most notes it can have: NOTES_MAX, the pool, the engine's own limit */
    uint32_t on;                /* sounds at all (not muted) */
} bd_part_t;
typedef struct { uint32_t idle, fx; bd_part_t p[2]; } bd_t;   /* the global items too */
#define BD_N 2u                  /* the parts the budget covers (the two shown ones; a test rig may have more live tracks) */
_Static_assert(NLIVE >= BD_N, "budget.c: two parts");
#include "budget_feat.h"        /* the new features' items: TWIN FILTER, SUB, CYCLE (cloud job B's stub; jobs E, F fill budget_feat.c) */

static uint32_t bd_slope(const engine_t *e, const int16_t *p)  /* filter 1, the multimode filter, on every engine since cloud job J: 0 bypassed or none, 1 12 dB, 2 24 dB (voice.c vc_filter's rule) */
{
    if (!e->ufilt || (p[P_FREEA] >= (int16_t)UF_STEPS && p[P_FREEC] <= 0 && !p[P_KTRK]))
        return 0;
    return p[P_FREED] == 2 ? 2u : 1u;
}
static uint32_t bd_slope8(const engine_t *e, const int8_t *p)
{
    int16_t q[P_COUNT];
    uint32_t i;
    for (i = 0; i < P_COUNT; i++)
        q[i] = p[i];
    return bd_slope(e, q);
}

#include "budget_feat.c"
static void bd_part(const track_t *t, bd_part_t *r)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];   /* (the sound's engine: a switch fades first, eng_req leads) */
    const int16_t *p = t->p;
    uint32_t i, sl, bands, pi = (uint32_t)(t - trk), lf = 0;
    memset(r, 0, sizeof *r);
    r->cap = vc_eng_cap(e);
    r->cap = r->cap < NOTES_MAX ? r->cap : NOTES_MAX;
    r->cap = r->cap < NVOICE ? r->cap : NVOICE;
    if (p[P_MUTE])
        return;                                         /* a muted part costs nothing */
    r->on = 1;
    r->eng = BD_ENG_DEF;
    for (i = 0; i < NELEM(BD_ENG); i++)
        if (BD_ENG[i].e == e)
            r->eng = BD_ENG[i].c;
    sl = bd_slope(e, p);
    if (pi < NSHOWN && (mph[pi].nid || mph[pi].fnid) && !morph_stale(t, &mph[pi])) {   /* the MORPH on this part: as voice.c vc_voice */
        const morph_part_t *mp = &mph[pi];
        uint32_t fa = bd_slope8(e, morph_cur(mp, 0)), fb = bd_slope8(e, morph_cur(mp, 1));
        sl = fa > sl ? fa : sl;
        sl = fb > sl ? fb : sl;
        if (fa && fb && morph_cur(mp, 0)[P_FREED] != morph_cur(mp, 1)[P_FREED])
            sl = 2;                                     /* (the slopes cross-fade: both stages run) */
        for (i = 0; i < mp->nid; i++)
            if ((mp->ent[i].kind & 0x7Fu) == MK_FADE && mp->ent[i].id >= P_E0 && mp->ent[i].id < P_E0 + 8u &&
                morph_cur(mp, 0)[mp->ent[i].id] != morph_cur(mp, 1)[mp->ent[i].id]) {
                r->ext += BD_FADE;                      /* an engine choice cross-faded: both shapes */
                break;
            }
        r->ext += BD_MORPH;
    }
    r->filt = sl == 2u ? BD_F24 : sl ? BD_F12 : 0;
    i = (uint32_t)p[P_ITYPE];
    r->ins = i == 0 ? 0 : i < NELEM(BD_INS) ? BD_INS[i] : BD_INS_MAX;
    r->sat = p[P_DIST] ? BD_SAT : 0;
    bands = (p[P_EQLG] != 0) + (p[P_EQMG] != 0) + (p[P_EQHG] != 0);
    r->eq = bands ? BD_EQ_BASE + bands * BD_EQ_BAND : 0;
    r->misc = p[P_DKDEP] > 0 ? BD_DUCK : 0;
    r->misc += sl && p[P_KTRK] ? BD_KTRK : 0;
    for (i = 0; i < NMSLOT; i++) {
        const int16_t *q = &p[MSLOT_ID(i)];
        if (q[0] && q[1] && q[2]) {
            r->misc += BD_MX_SLOT;
            lf |= (q[0] == MS_LFO ? 1u : 0u) | (q[0] == MS_LFO2 ? 2u : 0u);
        }
    }
    r->misc += ((lf & 1u) + (lf >> 1)) * BD_LFO;
    r->misc += p[P_AMODE] ? BD_ARP : 0;
    bd_feat(t, r);                                      /* TWIN FILTER, SUB, CYCLE (budget_feat.c) */
}

static uint32_t part_cost(const track_t *t)
{
    bd_part_t r;
    bd_part(t, &r);
    return BD_U(r.eng + r.filt + r.ext);
}
static uint32_t part_fixed(const track_t *t)
{
    bd_part_t r;
    bd_part(t, &r);
    return BD_U(r.ins + r.sat + r.eq + r.misc);
}

/* ----------------------------------------------------------------- the globals --- */
static void bd_global(uint32_t *idle, uint32_t *fx)     /* host instructions a sample: the idle mix, and what the sends and the master stages add */
{
    uint32_t m = duell_mode_req == DM_SPLIT, dm = (uint32_t)song.g[G_DMODE], rt = (uint32_t)song.g[G_RTYPE], p, cho = 0, mod;
    int32_t t = 0;
    *idle = BD_IDLE[m];
    for (p = 0; p < BD_N; p++)
        cho |= !trk[p].p[P_MUTE] && trk[p].p[P_CHOR] != 0;
    if (!(mods_off & MODS_SENDS)) {                     /* (SENDS OFF: the buses do not run) */
        dm = dm < 4u ? dm : 0u;                         /* (a value past OCT UP plays as DIGITAL, fx.c) */
        rt = rt < 6u ? rt : 2u;
        mod = (uint32_t)clamp(song.g[G_SHMOD], 0, 100);
        t = (int32_t)BD_BUS[m] + ((int32_t)BD_DLY[m][dm] - (int32_t)BD_BUS[m]);
        if (rt == 3u && song.g[G_SHPITCH] == SHP_DOWN)
            t += (int32_t)BD_SHD[m] - (int32_t)BD_BUS[m] + (mod ? (int32_t)BD_SHM[m] - (int32_t)BD_REV[m][3] : 0);
        else if (rt == 3u && mod)
            t += (int32_t)BD_SHM[m] - (int32_t)BD_BUS[m];
        else
            t += (int32_t)BD_REV[m][rt] - (int32_t)BD_BUS[m];
        t += cho ? (int32_t)BD_CHO[m] - (int32_t)BD_BUS[m] : 0;
    }
    if (!m) {                                           /* the master stages (none runs in DUEL) */
        t += (cmp_cfg & 0xF00u) ? (int32_t)BD_COMP : 0;
        t += wd_sel != 100u ? (int32_t)BD_WIDTH : 0;
        t += bm_sel ? (int32_t)BD_BMONO : 0;
    }
#if DUELL_VELO
    t += sense_running() ? 57 : 0;                      /* KEY SENSE (after Velo's method; est.: the key scan is not on the CPU bar): a fixed core-0 load of 2 % after the margin, 57 host instructions a sample, not measured */
#endif
    *fx = (uint32_t)t;
}
static uint32_t global_cost(void)
{
    uint32_t i, f;
    bd_global(&i, &f);
    return BD_U(i + f);
}

/* ----------------------------------------------------------------- the pair --- */
static void bd_build(bd_t *b)
{
    uint32_t p;
    bd_global(&b->idle, &b->fx);
    for (p = 0; p < BD_N; p++)
        bd_part(&trk[p], &b->p[p]);
}
static uint32_t bd_worst(const uint32_t *c, const uint32_t *n)   /* the dearest the voice pool can be split: the dearer part first */
{
    uint32_t h = c[1] > c[0], nh = n[h] < NVOICE ? n[h] : NVOICE, nl = n[!h] < NVOICE - nh ? n[!h] : NVOICE - nh;
    return nh * c[h] + nl * c[!h];
}
/* The numbers of both parts. A part's number is the most notes it can play while the other part plays the same number (or its own limit, when that is
 * lower): the worst case of that pair, whatever way the shared pool is split, stays under the limit. So a pair (a, b) from here always fits: with a >= b,
 * the pair (a, min(a, cap of the other)) is the one a was checked with and b is not above it. Each number depends on the settings only (never on what
 * plays, on the order of the calls or on the other number), and switching an item on in a part can only lower the numbers: its stages and voices
 * make the pair dearer. Never below 1. */
static void budget_solve(const bd_t *b, uint32_t *out)
{
    uint32_t c[2], fx[2], g = BD_U(b->idle + b->fx), lim = BUDGET_LIMIT * 100u, p;
    for (p = 0; p < 2u; p++) {
        const bd_part_t *r = &b->p[p];
        c[p] = r->on ? BD_U(r->eng + r->filt + r->ext) : 0;
        fx[p] = r->on ? BD_U(r->ins + r->sat + r->eq + r->misc) : 0;
    }
    for (p = 0; p < 2u; p++) {
        uint32_t n[2], k;
        for (k = b->p[p].cap; k > 1u; k--) {            /* (the most that fits: a pair with more notes costs more) */
            n[p] = k;
            n[!p] = k < b->p[!p].cap ? k : b->p[!p].cap;
            if (g + fx[0] + fx[1] + bd_worst(c, n) <= lim)
                break;
        }
        out[p] = k;
    }
}

static uint32_t part_notes(int part)
{
    bd_t b;
    uint32_t n[2];
    if (part < 0 || part >= (int)BD_N)
        return 0;
    bd_build(&b);
    budget_solve(&b, n);
    return n[part];
}
static uint8_t part_notes_shown(int part)
{
    return (uint8_t)part_notes(part);
}

/* what limits part p: the item whose removal gives it the most notes (ties: the dearer item) */
static uint8_t part_notes_reason(int part)
{
    bd_t b, q;
    uint32_t n[2], m[2], k, best = 0, gain = 0, bcost = 0, o = part == 0;
    static const uint8_t WHY[7] = {NR_FILTER, NR_INSERT, NR_SAT, NR_EQ, NR_FX, NR_OTHER, NR_MISC};
    if (part < 0 || part >= (int)BD_N)
        return NR_NONE;
    bd_build(&b);
    budget_solve(&b, n);
    for (k = 0; k < 7u; k++) {
        uint32_t cost = 0;
        q = b;
        switch (k) {
        case 0: cost = q.p[part].filt > BD_F12 ? q.p[part].filt - BD_F12 : 0, q.p[part].filt = BD_F12 * (q.p[part].filt != 0); break;   /* (the 24 dB slope: down to 12 dB) */
        case 1: cost = q.p[part].ins, q.p[part].ins = 0; break;
        case 2: cost = q.p[part].sat, q.p[part].sat = 0; break;
        case 3: cost = q.p[part].eq, q.p[part].eq = 0; break;
        case 4: cost = q.fx, q.fx = 0; break;
        case 5: cost = q.p[o].eng + q.p[o].filt + q.p[o].ext, q.p[o].eng = q.p[o].filt = q.p[o].ext = 0; break;
        default: cost = q.p[part].misc + q.p[part].ext, q.p[part].misc = q.p[part].ext = 0; break;
        }
        if (!cost)
            continue;
        budget_solve(&q, m);
        if (m[part] > n[part] && (m[part] - n[part] > gain || (m[part] - n[part] == gain && cost > bcost))) {
            gain = m[part] - n[part];
            bcost = cost;
            best = WHY[k];
        }
    }
    if (best)
        return (uint8_t)best;
    if (n[part] >= b.p[part].cap)                       /* it has all it can have: the pool, NOTES_MAX, or the engine's own limit */
        return b.p[part].cap < NOTES_MAX && b.p[part].cap < NVOICE ? NR_ENGINE : NR_NONE;
    return NR_ENGINE;                                   /* the engine's own voices are what is left */
}

static void budget_poll(void)                           /* main loop, every frame (main.c, beside vcap_poll) */
{
    bd_t b;
    uint32_t n[2], p;
#if DUELL_BENCH
    if (bench_bopen) {                                 /* (bench builds: a cost row of src/bench.c holds the budget open: no numbers, every voice the
                                                        * VOICE CAP and the pool give, as the bench of 0.2.25 measured) */
        for (p = 0; p < BD_N; p++)
            bnotes[p] = 0;
        return;
    }
#endif
    bd_build(&b);
    budget_solve(&b, n);
    for (p = 0; p < BD_N; p++)
        bnotes[p] = (uint8_t)n[p];
}
