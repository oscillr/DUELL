/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Voice allocation, envelopes, LFO and per-track rendering.
 * Runs in the audio ISR.
 *
 * Each synth part has its own NVOICE voice slots (so MONO / LEGATO / UNISON keep using
 * v[0..]). How many of them sound at once is the VOICE POOL's (owner rules G22 / G23; budget.c works it out,
 * bd_pool below hands it over): a POLY part may sound a new voice while the pool has room for one more of its
 * voices; without room the new note takes the oldest voice of the part that holds more of the pool (voice_pool_room;
 * it fades out over one block, stage 4: the envelope goes to 0, the block's amplitude ramp declicks it), else one of
 * the part's own is restarted in place. A MONO / LEGATO / UNISON part holds its fixed need of the pool. */
static uint32_t vage;                                   /* voice ages: one clock for every part */
/* an LFO's WAVE (N_LWAVE) at phase ph; rnd: its S&H value of the cycle */
static int32_t lfo_wave_of(int32_t wave, uint32_t rnd, uint32_t ph)
{
    switch (wave) {
    case 1:
        return osc_tri(ph);
    case 2:
        return (int32_t)(ph >> 16) - 32768;
    case 3:
        return ph < 0x80000000u ? 32767 : -32767;
    case 4:
        return (int32_t)(rnd >> 16) - 32768;
    default:
        return osc_sine(ph);
    }
}

/* the S&H value of a new LFO cycle (the previous value is a part's own state) */
static uint32_t lfo_rand(track_t *t)
{
    return part_rand(t, &t->lfo_rnd);
}

/* LFO 2 (1.2, #154; params.c N_LSYNC): with SYNC on, one cycle lasts the note value of the tempo (fx.c div_samples:
 * BPM, or the external clock's measured tempo while it runs), whatever RATE says (the matrix's RATE too): the phase
 * advances 2^32 x CTL / cycle a block (two 32-bit divides; the 1-bit truncation of 0xFFFFFFFF drifts by less than a
 * millionth of a cycle a cycle). OFF: RATE's table, as before */
static uint32_t div_samples(uint32_t div);         /* (fx.c) */
static __attribute__((noinline)) uint32_t lfo_sync_inc(uint32_t sync)   /* (not inlined: its divides stay out of
                                                                       * the audio ISR's loop over the tracks) */
{
    uint32_t cyc, q, r;
    cyc = div_samples(LSYNC_DIV[sync % NELEM(LSYNC_DIV)]);
    cyc = cyc ? cyc : 1u;
    q = 0xFFFFFFFFu / cyc;
    r = 0xFFFFFFFFu % cyc;
    return q * CTL + (r * CTL + CTL) / cyc;
}

/* LFO 2's S&H draws: its own xorshift on every part (the shared rng, and so the golden renders, never see LFO 2) */
static uint32_t lfo2_rand(uint32_t s)
{
    s = s ? s : 0x7F4A7C15u;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

/* one control tick of an LFO l of track t (DUELL: LFO 1, t->lfo1, and LFO 2, t->lfo2, share it): r its RATE (WAVE PHS FADE
 * follow), s its SYNC (TRIG POL follow); lfo1: LFO 1's S&H generator (lfo_rand: the shared rng on parts 1..3), else LFO 2's.
 * LFO 1's numbers are the ones track_lfo_tick made before (the same operations in the same order) */
static void lfo_tick(track_t *t, lfo_t *l, uint32_t r, uint32_t s, int lfo1)
{
    const int16_t *p = t->p;
    uint32_t old = l->ph;
    l->ph += p[s] ? lfo_sync_inc((uint32_t)p[s]) : LFO_INC[p[r] & 127];
    if (l->ph < old) {
        l->prev = (int16_t)(l->rnd ? (int32_t)(l->rnd >> 16) - 32768 : 0);   /* (mod.c mod_sh: SLEW glides on from there) */
        l->rnd = lfo1 ? lfo_rand(t) : lfo2_rand(l->rnd);
    }
    l->val = lfo_wave_of(p[r + 1u], l->rnd, l->ph);
    if (MXF(t).on && MXF(t).w[lfo1 ? MX_LWAVE : MX_L2WAVE]) {   /* DUELL MORPH: the other end's WAVE runs too, cross-faded */
        uint32_t x = lfo1 ? MX_LWAVE : MX_L2WAVE;
        l->val += ((lfo_wave_of(MXF(t).b[x], l->rnd, l->ph) - l->val) * (int32_t)MXF(t).w[x]) >> 15;
    }
    if (p[s + 2u])                                      /* UNI: 0..+1 (the depths reach as far, one way only) */
        l->val = (l->val + 32768) >> 1;
    {                                                   /* DUELL 1.1 LFO LEVEL (X_LFO1LVL / X_LFO2LVL, 0..127, 127 = full): the LFO's master depth, applied where the value is made so that every reader (the DEST depths, the matrix source, the TWIN depth, the meter) sees it; 127 skips this, the value as before */
        int32_t lv = XV(t, lfo1 ? X_LFO1LVL : X_LFO2LVL);
        if (lv < 127)
            l->val = mulq15(l->val, lv < 0 ? 0 : lv * 258);
    }
    if (BYP(t, lfo1 ? BY_LFO1 : BY_LFO2))               /* DUELL 1.1 BYPASS: the LFO keeps running (in step when it comes back) but sends 0 */
        l->val = 0;
    if (l->fade < 32767) {
        int32_t step = (int32_t)(ENV_LIN[p[r + 3u] & 127] >> 9);
        l->fade = p[r + 3u] ? clamp(l->fade + (step ? step : 1), 0, 32767) : 32767;
    }
}
static void track_lfo_tick(track_t *t) { lfo_tick(t, &t->lfo1, P_LRATE, P_LSYNC, 1); }

/* DUELL 0.2.11 VOICE CAP (owner, 2026-10-14; SPEC 6.3): a heavy combination gets fewer voices per part, so the predicted
 * worst case of both parts stays under VC_BUDGET of the half (the loadcost scale of docs/CPU_ANALYSIS.md: model %, the device
 * reads ~10-25 % lower). The cost of a voice (0.1 % of the half): the engine's (its heaviest factory sound, 8 voices held, minus the
 * idle mix over the voices it holds, CPU_ANALYSIS 2a, measured again: tests/cpu_scenarios_test.c P), + the multimode filter
 * 12 dB / 24 dB (2b: +14 / +28 % at 8 voices), and with the MORPH on that part:
 * + VC_MORPH (the hook), the filter as the heaviest of the sound and both ends (a bypassed filter that an end switches on counts,
 * SLOPE 12 / 24 dB that differ: 24, both stages run), + VC_FADE when an engine choice is cross-faded (both shapes made). The worst case: the
 * dearer part takes all the voices its cap allows, the other the rest of the 8-voice pool, + the idle mix (LAYER: every note on
 * both parts, 4 + 4: no cap of 4 or more changes it, so none is set). Over VC_BUDGET: the
 * dearer part's cap down by one (never below VC_MIN), again. Only the dearer part's cap can lower the worst case: the pool stays
 * 8 voices, so a cap moves voices to the cheaper part (a muted part costs 0, so a part played alone is capped to fit by itself).
 * When even VC_MIN cannot bring the pair under VC_BUDGET (two dear parts: 4 + 4 is over), the caps cannot help the pair, and
 * each part gets the cap that fits it alone (VC_BUDGET - VC_IDLE over its voice, VC_MIN at least) instead of 4. Computed
 * every UI frame (vcap_poll: main loop; a load, an engine, filter or SLOPE change, a roll are seen at once), deterministic.
 * It bites only for NEW notes: voice_alloc / UNISON take at most vcap voices (a note over the cap steals within the part, as
 * at an engine's own cap); notes that sound beyond the cap keep sounding until they end. Shown only on the TEST screen */
#define VC_IDLE 150u                                    /* the idle mix with power-on FX: 15 % */
#define VC_BUDGET 850u                                  /* 85 % */
#define VC_F12 18u                                      /* the multimode filter a voice: 12 dB */
#define VC_F24 36u                                      /* .. 24 dB (24 dB HP, the dearest) */
#define VC_MORPH 5u                                     /* the morph a voice (the hook, its blends: +0.3..0.5 %, measured) */
#define VC_FADE 20u                                     /* .. an engine choice cross-faded (M04 WIRED SNAIL's ANALOG WAVE, M14 PHASE WAVE: +2..2.6 %) */
#define VC_MIN 4u
static uint8_t vcap[NTRK];                              /* the cap of each part, 0: none yet (all of the engine's) */
static uint8_t bnotes[NTRK];                            /* DUELL NOTES budget (budget.c budget_poll): the notes of each part by the rule in force (two cores: the pool with the other part silent), 0: none yet (the budget is not asked) */
static uint8_t bnotes1[NTRK];                           /* DUELL 1.1 item 90: the one-core numbers, always (the pair rule: budget.c budget_solve2) */
static volatile uint8_t bd_one_now;                     /* DUELL 1.1 item 90: 1 = the audio plays both parts on core 0 right now (core 1 down, paused, or a flash park: twocore_run.c tcr_begin sets it
                                                         * every block): the note-on path and the counter use bnotes1. 0 in every one-core build */
/* THE VOICE POOL (owner rules G22 / G23, 2026-11-06; working docs/design studies/voice-pool-budget-plan_2026-11-06.md). Both parts draw their voices from ONE pool of audio time on two cores. budget.c
 * works the pool out from the settings (budget_pool) and hands it over the way the deadline's items are (budget.h bd_dl): the main loop writes the copy the audio side is not reading, then one byte
 * says which copy is new. The audio side counts the sounding voices itself (vp_held), so a part's limit moves with what the other part plays. All in 1/100 % of a half's time (budget.h) */
typedef struct {
    uint32_t pool;              /* the room for voices over both cores */
    uint32_t c[2];              /* one voice of part 1 / part 2 (0: the part is muted) */
    uint8_t need[2];            /* the part's FIXED NEED in its own voices, held for good: 0 = a POLY part (it holds what it sounds), 1 = MONO / LEGATO, UNISON = its stack */
    uint8_t comb[2];            /* a TWIN COMB sounds on the part (live or at a MORPH end): its slot ceiling is NVOICE_COMB */
    uint8_t two;                /* 1 = the pool is the rule in force (two cores); 0 = the one-core pair rule (bnotes1) */
} bd_pool_t;
static bd_pool_t bd_pool_buf[2];
static volatile uint8_t bd_pool_i, bd_pool_ok;          /* which copy is the new one; 1 = budget_poll has handed one over (0: bench builds holding the budget open, tests without the main loop's pass) */
/* VOICE SHARE stage B2 step 5 (working docs/voice-split-b2-design_2026-11-03.md section 10): the core 0 side of the single share job. VS_SHARE is the master switch, a COMPILE-TIME switch, ON since step 8 (-DVS_SHARE=0 turns it off: every test of it below
 * folds away and the firmware is as before the share). Nothing in the share code runs, and nothing is added to the audio path, while it is 0.
 * sh_busy: bit i = slot i of PART 1 is offloaded to core 1 and may be half written (a share job that core 1 claimed and did not finish: its set is FROZEN until twocore_run.c tcr_settle sees the job closed). EVERY writer of a
 * voice skips a frozen slot (VS_FROZEN), or, where it only wants a voice faded or let go, leaves its wish for the release: sh_pend_kill (voice_kill) / sh_pend_off (note-off, all-off), applied by tcr_sh_release */
#ifndef VS_SHARE
#define VS_SHARE 0                                      /* OFF since 2026-11-05: the half split (core 1 renders every second voice of part 1 while part 2 is silent) gave the heavy patch no voice; the runner's share code goes with the equal-parts runner (working docs/design studies/voice-split-equal-parts-size_2026-11-05.md) */
#endif
static volatile uint8_t bd_share_now;                   /* the old half split's flag (tcr_begin sets it every block; stays 0 while VS_SHARE is 0). Nothing reads it since the voice pool */
static volatile uint16_t sh_busy;                       /* the frozen slots of part 1 (see above) */
static uint16_t sh_pend_kill, sh_pend_off;              /* wishes that met a frozen voice: fade it / let it go, at the release */
#define VS_FROZEN(t, i) (VS_SHARE && (t) == &trk[0] && ((sh_busy >> (i)) & 1u))
#define VS_FROZEN_V(v) (VS_SHARE && (v) >= trk[0].v && (v) < trk[0].v + NVOICE && ((sh_busy >> ((v) - trk[0].v)) & 1u))
#if DUELL_TWOCORE
#if TC_MEAS
/* BENCH-HEAVY timing of part 1's share path (bench builds only; cleared with the tcm figures, twocore_run.c tcm_clear; read by bench.c): per block with a posted share job the ticks of
 * [0] the control pass (pass 1), [1] core 0's own set render, [2] the wait for core 1 (tcr_share_wait; the merge is not in it): max and sum over n blocks (the ones counted in sh_posts). */
static struct { uint32_t n, max[3], sum[3]; } vsm TC_BSS;
#define VSM_ADD(k, d) do { uint32_t d_ = (d); if (d_ > vsm.max[k]) vsm.max[k] = d_; vsm.sum[k] += d_; } while (0)
#endif
#endif
static const struct { const engine_t *e; uint8_t c; } VCOST[] = {   /* (tests/cpu_scenarios_test.c CPU_SCEN=P: the heaviest sound, its voices) */
    {&ENG_ANALOG, 51}, {&ENG_FM6, 61}, {&ENG_PHASE, 40}, {&ENG_LOFI, 24}, {&ENG_FORMANT, 60}, {&ENG_TRIO, 44}, {&ENG_WHEEL, 59},
    {&ENG_PHYS, 144}, {&ENG_NOISE, 47}, {&ENG_WAVE, 52},   /* (FM6: 6 voices, PHYS 3, VOICE 4; WAVE: #60 DIGITAL PAD less its 12 dB filter) */
};
/* THE SLOT CEILING of a part (static: from the settings only): the voice slots it may use, 0 .. ceiling - 1. Every slot (NVOICE), or what the engine's memory holds (engine_t.poly: PHYS, FM6), and
 * NVOICE_COMB with a TWIN COMB that sounds (a slot above it has no line and would play LP: chain_f2.c f2_line). Never a count of how many may sound: that is trk_nvoice */
static uint32_t trk_nvoice_eng(const track_t *t)
{
    uint32_t p = (uint32_t)(t - trk) % NTRK, c = ENGINES[t->engine]->poly;
    uint32_t comb = t->p[P_TWTYPE] == (int16_t)TT_COMB && t->engine != ENGI_WAVE && t->engine != ENGI_PHYS;
    c = c && c < NVOICE ? c : NVOICE;
    comb |= bd_pool_ok && p < 2u && bd_pool_buf[bd_pool_i].comb[p];   /* (.. or at a MORPH end: the budget saw it) */
    if (t->engine == ENGI_FM6 && p < NLIVE && (comb || fm6_comb[p]) && c > FM6_POLY_COMB)
        return FM6_POLY_COMB;                           /* FM6 with a COMB (or while the COMB's lines still ring: f2_fm6_share): its first 6 records; the others lie under the lines */
    return comb && c > NVOICE_COMB ? NVOICE_COMB : c;
}
static const bd_pool_t *vp_now(void)                    /* the pool, while it is the rule in force; 0: the one-core numbers hold (bnotes / bnotes1) */
{
    const bd_pool_t *d = &bd_pool_buf[bd_pool_i];
    return bd_pool_ok && !bd_one_now && d->two ? d : 0;
}
static uint32_t vp_sounding(const track_t *t)           /* the part's sounding voices (not the ones fading out) */
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active && t->v[i].stage != 4u;
    return n;
}
static uint32_t vp_held(const bd_pool_t *d, uint32_t p)  /* what part p (0 / 1) holds of the pool right now: its sounding voices, its fixed need at least */
{
    uint32_t n = vp_sounding(&trk[p]);
    return (n > d->need[p] ? n : d->need[p]) * d->c[p];
}
/* UNISON's STACK: the slot ceiling, NVOICE_UNISON at most (its spread and level are made for 8), and what the budget gave it as its fixed need. From the settings only: it never moves with what plays */
static uint32_t trk_unison(const track_t *t)
{
    uint32_t p = (uint32_t)(t - trk) % NTRK, c = trk_nvoice_eng(t), b;
    const bd_pool_t *d = vp_now();
    c = c < NVOICE_UNISON ? c : NVOICE_UNISON;
    b = d && p < 2u ? d->need[p] : bd_one_now ? bnotes1[p] : bnotes[p];
    return b && b < c ? b : c;
}
/* THE LIVE COUNT LIMIT of a part: how many voices it may SOUND right now. Two cores (the pool, rule G22): the room the other part leaves in the pool, in this part's own voices, so it falls while
 * the other part plays; a MONO / LEGATO part its one voice, a UNISON part its stack. One core (core 1 down or paused, a one-core build): the pair rule's number. Never above the slot ceiling.
 * The header's AVAILABLE VOICES is this less the keys held (ui_draw.c head_notes) */
static uint32_t trk_nvoice_m(const track_t *t);
static uint32_t trk_nvoice(const track_t *t) { return trk_nvoice_m(t); }   /* (2026-11-09: the measured cap of 2026-11-08 is gone: it cut held notes while the screen showed room, owner rule) */
static uint32_t trk_nvoice_m(const track_t *t)          /* the number by the model (the budget, the pool, the engine's ceiling) */
{
    uint32_t p = (uint32_t)(t - trk) % NTRK, c = trk_nvoice_eng(t), b;
    const bd_pool_t *d = vp_now();
    if (t->p[P_VOICE] == V_UNISON)
        return trk_unison(t);
    if (d && p < 2u) {
        uint32_t held, k;
        if (d->need[p])
            return d->need[p] < c ? d->need[p] : c;
        held = vp_held(d, p ^ 1u);
        k = d->c[p] ? (d->pool > held ? d->pool - held : 0u) / d->c[p] : c;
        return k < c ? k : c;
    }
    b = bd_one_now ? bnotes1[p] : bnotes[p];
    return b && b < c ? b : c;                          /* (0: the budget is not asked: every slot) */
}
/* LAYER (every key plays on BOTH parts; owner 2026-11-08: "if we promise that it plays seven voices parallel, it should better play seven voices parallel"): the KEYS the pool
 * holds with both parts sounding every one of them. A POLY part holds one voice a key, a MONO / LEGATO / UNISON part its fixed need whatever the keys; a muted part (cost 0) nothing. The allocation
 * shares the pool between two POLY parts that hold the same keys (voice_pool_room: the part that holds more gives way), so each part reaches this number. The one-core rule: the pair rule's numbers,
 * which already count both parts, the smaller. The screen's numbers in LAYER (ui_draw.c head_notes) are this less the keys held */
static uint32_t trk_nkeys_both(void)
{
    const bd_pool_t *d = vp_now();
    uint32_t p, cs = 0, fx = 0, m = NVOICE, k;
    if (!d) {
        uint32_t a = trk[0].p[P_MUTE] ? NVOICE : trk_nvoice(&trk[0]), b = trk[1].p[P_MUTE] ? NVOICE : trk_nvoice(&trk[1]);
        return a < b ? a : b;
    }
    for (p = 0; p < 2u; p++) {
        uint32_t ce;
        if (!d->c[p] || trk[p].p[P_MUTE])
            continue;
        if (d->need[p]) {
            fx += (uint32_t)d->need[p] * d->c[p];
            continue;
        }
        cs += d->c[p];
        ce = trk_nvoice_eng(&trk[p]);
        m = ce < m ? ce : m;
    }
    if (!cs)
        return trk_nvoice(&trk[song.sel & 1u]);         /* (no POLY part: the mode letters show, not this) */
    k = d->pool > fx ? (d->pool - fx) / cs : 0u;
    k = k < m ? k : m;
    return k ? k : 1u;
}
#define VC_POOL 8u                                      /* the 0.2.11 VOICE CAP table's own model: 8 voices over both parts. Only the TEST screen still shows its caps (vcap_shown); no note-on reads them since the voice pool */
static uint32_t vc_eng_cap(const engine_t *e) { return e->poly && e->poly < VC_POOL ? e->poly : VC_POOL; }
static uint32_t vcap_shown(uint32_t p)                  /* the TEST screen: the voices part p may use (its sound's engine, the cap) */
{
    uint32_t c = vc_eng_cap(ENGINES[eng_idx(trk[p].eng_req)]);
    return vcap[p] && vcap[p] < c ? vcap[p] : c;
}
static uint32_t vc_filter(const engine_t *e, const int8_t *p)   /* the multimode filter's cost a voice in these values */
{
    if (!e->ufilt || (p[P_FREEA] >= UF_STEPS && p[P_FREEC] <= 0 && !p[P_KTRK]))
        return 0;                                       /* (the bypass: MODE LP, CUT open, no KEY TRK) */
    return p[P_FREED] == 2 ? VC_F24 : VC_F12;
}
static uint32_t vc_voice(const track_t *t)              /* a voice of part t, 0.1 % of the half (0: the part is muted) */
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];   /* (the sound's engine: a switch fades first, eng_req leads) */
    uint32_t i, c = 60u, f, pi = (uint32_t)(t - trk);   /* (an engine not in the table: as the dearest) */
    int8_t v[P_COUNT];
    if (t->p[P_MUTE])
        return 0;
    for (i = 0; i < NELEM(VCOST); i++)
        if (VCOST[i].e == e)
            c = VCOST[i].c;
    for (i = 0; i < P_COUNT; i++)
        v[i] = (int8_t)t->p[i];
    f = vc_filter(e, v);
    if (pi < NSHOWN && (mph[pi].nid || mph[pi].fnid) && !morph_stale(t, &mph[pi])) {
        const morph_part_t *mp = &mph[pi];
        uint32_t fa = vc_filter(e, morph_cur(mp, 0)), fb = vc_filter(e, morph_cur(mp, 1));
        f = fa > f ? fa : f;
        f = fb > f ? fb : f;
        if (fa && fb && morph_cur(mp, 0)[P_FREED] != morph_cur(mp, 1)[P_FREED])
            f = VC_F24;                                 /* (the slopes cross-fade: both stages run) */
        for (i = 0; i < mp->nid; i++)
            if ((mp->ent[i].kind & 0x7Fu) == MK_FADE && mp->ent[i].id >= P_E0 && mp->ent[i].id < P_E0 + 8u &&
                morph_cur(mp, 0)[mp->ent[i].id] != morph_cur(mp, 1)[mp->ent[i].id]) {
                c += VC_FADE;                           /* an engine choice cross-faded: both shapes */
                break;
            }
        c += VC_MORPH;
    }
    return c + f;
}
/* the predicted worst case (0.1 %) of the per-voice costs k[0..1] with the caps c[0..1] */
static uint32_t vc_worst(const uint32_t *k, const uint32_t *c)
{
    uint32_t h = k[1] > k[0], nh = c[h] < VC_POOL ? c[h] : VC_POOL, nl = c[!h] < VC_POOL - nh ? c[!h] : VC_POOL - nh;
    return VC_IDLE + nh * k[h] + nl * k[!h];
}
/* the caps for the per-voice costs k[0..1] and the engines' own caps e[0..1] into cap[0..1]; returns the predicted worst case with them */
static uint32_t vc_caps(const uint32_t *k, const uint32_t *e, uint32_t *cap, uint32_t layer)
{
    uint32_t w, h;
    cap[0] = e[0];
    cap[1] = e[1];
    if (layer)                                          /* LAYER: every note on both parts, the pool split evenly (4 + 4): a cap of 4 or */
        return VC_IDLE + (e[0] < VC_POOL / 2u ? e[0] : VC_POOL / 2u) * k[0] + (e[1] < VC_POOL / 2u ? e[1] : VC_POOL / 2u) * k[1];   /* more changes nothing */
    h = k[1] > k[0];
    while ((w = vc_worst(k, cap)) > VC_BUDGET && k[h] > k[!h] && cap[h] > VC_MIN)
        cap[h]--;                                       /* (one voice of the dearer part goes to the cheaper one: the pool stays 8) */
    if (w > VC_BUDGET) {                                /* the pair cannot get under it with caps of 4 or more: then each part as if it */
        for (h = 0; h < 2u; h++) {                      /* played alone (a part on its own fits), never below VC_MIN */
            uint32_t c = k[h] ? (VC_BUDGET - VC_IDLE) / k[h] : VC_POOL;
            c = c < VC_MIN ? VC_MIN : c;
            cap[h] = c < e[h] ? c : e[h];
        }
        w = vc_worst(k, cap);
    }
    return w;
}
static uint32_t vcap_worst;                             /* the predicted worst case with the caps (0.1 %; the TEST screen) */
static void vcap_poll(void)                             /* main loop, every frame */
{
    uint32_t k[2], e[2], cap[2], p;
    for (p = 0; p < 2u; p++) {
        k[p] = vc_voice(&trk[p]);
        e[p] = vc_eng_cap(ENGINES[eng_idx(trk[p].eng_req)]);
    }
    vcap_worst = vc_caps(k, e, cap, duell_mode_req == DM_LAYER);
    for (p = 0; p < 2u; p++)
        vcap[p] = (uint8_t)cap[p];
}

/* the part's voice mode: P_VOICE */
static uint32_t trk_vmode(const track_t *t)
{
    return (uint32_t)t->p[P_VOICE];
}

/* ------------------------------------------------- the shared voice budget --- */
static uint32_t voices_busy(void)                       /* sounding voices of all parts (not the fading ones) */
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].active && trk[p].v[i].stage != 4u;
    return n;
}

static uint32_t lowest_held(const track_t *t)           /* index of the lowest held note (the bass), NVOICE = none */
{
    uint32_t i, low = NVOICE;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && (low == NVOICE || t->v[i].note < t->v[low].note))
            low = i;
    return low;
}

static uint32_t voice_kills;                            /* voices given up (budget, overload): console, hostsim */
static void voice_kill(voice_t *v)                      /* fade out over the next block (env_tick stage 4) */
{
    if (VS_FROZEN_V(v)) {                               /* a frozen voice (core 1 may be writing it): the fade waits for the release */
        sh_pend_kill |= (uint16_t)(1u << (v - trk[0].v));
        return;
    }
    v->gate = 0;
    v->stage = 4;
    voice_kills++;
}

/* the voice of part t to give up first among its slots from .. NVOICE - 1: the oldest released one, else the oldest held one. 0 = none */
static voice_t *voice_oldest(track_t *t, uint32_t from)
{
    voice_t *best = 0;
    uint32_t i;
    for (i = from; i < NVOICE; i++) {
        voice_t *v = &t->v[i];
        if (v->active && v->stage != 4u && !VS_FROZEN(t, i) && (!best || (!v->gate && best->gate) || (!v->gate == !best->gate && v->age < best->age)))
            best = v;
    }
    return best;
}
/* DUELL NOTES budget (budget.c): a part that sounds more than it may (an item was switched on, the pool shrank, core 1 stopped) loses the voices above its number, faded (voice_kill: the one-block fade,
 * never a cut), BY COUNT: the released ones first (the oldest), then the oldest held. Never by slot index: a part within its number keeps every voice, whatever slot it sits in. Voices in slots above
 * the SLOT CEILING go too (the ceiling dropped: another engine, a TWIN COMB switched on; UNISON: above its stack). Only while the budget is on (bnotes set by budget_poll). Returns how many it faded.
 * Called at the part's next note-on (voice_trim), and by the two-core runner at once when the jobs pause or core 1 stops (twocore_run.c) */
static uint32_t voice_trim_now(track_t *t)
{
    uint32_t i, ns, np, n, k = 0, mode = trk_vmode(t);
    if (!bnotes[(t - trk) % NTRK])
        return 0;
    ns = mode == V_UNISON ? trk_unison(t) : trk_nvoice_eng(t);
    for (i = ns; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].stage != 4u && !VS_FROZEN(t, i)) {
            voice_kill(&t->v[i]);
            k++;
        }
    if (mode != V_POLY)
        return k;                                       /* (MONO / LEGATO / UNISON: the fixed need, nothing to count) */
    np = trk_nvoice(t);
    for (n = vp_sounding(t); n > np; n--, k++) {
        voice_t *v = voice_oldest(t, 0);
        if (!v)
            break;
        voice_kill(v);
    }
    return k;
}
static void voice_trim(track_t *t) { (void)voice_trim_now(t); }

/* ROOM IN THE POOL for part t to sound `want` voices (rule G22; two cores only). While they do not fit, the OTHER part gives up its oldest voice (released first; one-block fade):
 *   a POLY part's new note: only while the other part holds more of the pool than this part does now (then the steal stays inside the part: voice_alloc);
 *   a MONO / LEGATO / UNISON part: its fixed need is its own, so until it fits.
 * The other part never goes below its own fixed need (its stack sits in slots 0 .. need - 1) */
static void voice_pool_room(track_t *t, uint32_t want)
{
    const bd_pool_t *d = vp_now();
    uint32_t p = (uint32_t)(t - trk), k;
    if (!d || p >= 2u || !want)
        return;
    for (k = 0; k < NVOICE; k++) {
        uint32_t ho = vp_held(d, p ^ 1u);
        voice_t *v;
        if (want * d->c[p] + ho <= d->pool)
            return;                                     /* it fits */
        if (!d->need[p] && ho <= (want - 1u) * d->c[p])
            return;                                     /* the other part holds no more than this one */
        if (vp_sounding(&trk[p ^ 1u]) <= d->need[p ^ 1u] || !(v = voice_oldest(&trk[p ^ 1u], d->need[p ^ 1u])))
            return;                                     /* it holds only its fixed need */
        voice_kill(v);
    }
}

/* POLY allocation: the same note again restarts its voice in place. Else, while the part sounds fewer voices than it may (trk_nvoice, after rule G22's room), a free slot under the
 * slot ceiling: ROTATE takes the next one round-robin (release tails ring out), any GLIDE takes the free voice whose last pitch is closest, so poly portamento moves each
 * voice the shortest way. Else steal inside the part, restarted in place: the oldest released voice, else the oldest held one, never the lowest held note (the bass) */
static voice_t *voice_alloc(track_t *t, uint32_t note)
{
    uint32_t i, ns = trk_nvoice_eng(t), np, n, best = ns, low = ns, same = ns;
    int32_t bd = 0x7FFFFFFF;
    voice_trim(t);
    for (i = 0; i < ns; i++)
        if (t->v[i].active && t->v[i].note == note && !VS_FROZEN(t, i)) {
            if (t->v[i].stage != 4u)
                return &t->v[i];                        /* it sounds: restarted in place, the count stays */
            same = i;                                   /* fading out: its slot again, if the count allows one more */
        }
    n = vp_sounding(t);
    voice_pool_room(t, n + 1u);
    np = trk_nvoice(t);
    np = np ? np : 1u;                                  /* (a part that sounds nothing always gets its one voice) */
    if (n < np) {
        if (same < ns)
            return &t->v[same];
        if (t->p[P_GLIDE]) {                            /* (T0227, owner: the stored VOICE ALLOCATION is ignored: ROTATE always; with GLIDE the free voice nearest in pitch, as ever) */
            for (i = 0; i < ns; i++) {
                int32_t d = t->v[i].pitch_cur - (int32_t)note * 16;
                if (t->v[i].active)
                    continue;
                d = d < 0 ? -d : d;
                if (!t->v[i].pitch_cur)
                    d = 0x7FFFFFF0;                      /* never played: last choice */
                if (d < bd) {
                    bd = d;
                    best = i;
                }
            }
        } else {
            for (i = 0; i < ns && best == ns; i++) {
                uint32_t k = (t->rr + i) % ns;
                if (!t->v[k].active)
                    best = k;
            }
            t->rr = (uint8_t)((best == ns ? t->rr : best) + 1u) % ns;
        }
        if (best == ns)                                 /* every slot under the ceiling is taken, some by voices fading out: the oldest of those starts again */
            for (i = 0; i < ns; i++)
                if (t->v[i].active && t->v[i].stage == 4u && !VS_FROZEN(t, i) && (best == ns || t->v[i].age < t->v[best].age))
                    best = i;
        if (best < ns)
            return &t->v[best];
    }
    for (i = 0; i < ns; i++)                             /* the lowest held note is protected */
        if (t->v[i].active && t->v[i].gate && (low == ns || t->v[i].note < t->v[low].note))
            low = i;
    for (i = 0; i < ns; i++)                             /* oldest released (not one fading out) */
        if (t->v[i].active && !t->v[i].gate && t->v[i].stage != 4u && !VS_FROZEN(t, i) && (best == ns || t->v[i].age < t->v[best].age))
            best = i;
    if (best == ns)
        for (i = 0; i < ns; i++)                         /* oldest held, not the bass */
            if (i != low && t->v[i].active && t->v[i].gate && !VS_FROZEN(t, i) && (best == ns || t->v[i].age < t->v[best].age))
                best = i;
    if (best == ns)                                     /* nothing else to steal: the bass itself, else the note's own fading slot, else slot 0 */
        best = low < ns ? low : same < ns ? same : 0u;
    return &t->v[best];
}

/* glide: RATE = a fixed speed, TIME = the same time for any interval */
static void glide_set(track_t *t, voice_t *v, int glide)
{
    if (!glide || !v->pitch_cur) {
        v->pitch_cur = v->pitch16;
        v->gstep = 0;
        return;
    }
    if (t->p[P_GLMODE]) {
        int32_t d = v->pitch16 - v->pitch_cur, ticks = 1 + t->p[P_GLIDE] * t->p[P_GLIDE] / 8;
        d = d < 0 ? -d : d;
        v->gstep = d / ticks > 0 ? d / ticks : 1;
    } else {
        v->gstep = 0;
    }
}

/* hunt3: STEP CANCELLATION at a re-strike. A key struck again over a voice that still sounds (or a voice restarted in place) can make the voice's output jump:
 * an engine that resets a state at its note-on (a wrong-state reset) puts its new first sample somewhere else than where the old note was going. That step is an
 * artifact; voice_step() measures it (the new first sample against where the voice's last two output samples (v->lastc, v->lastd) were heading) and takes it off
 * again, decaying linearly over VOICE_STEP_LEN samples. Nothing else is touched: a voice that continues without a step is bit for bit as before, and a note that
 * starts from silence keeps the waveform's own first value (a saw's edge, a kick's first sample, a DX7's one-block attack): the engine's normal behaviour, not an artifact.
 * VOICE_OWN_START is the per-engine switch (bit = engine index, engines.c ENGINES[]): a 1 cancels the waveform's own first value of a fresh note too (the same decay).
 * All 0 by decision, engine by engine:
 *   ANALOG 0   the oscillators' start phase and the saw's edge are the model
 *   PHASE 2    phase distortion's cosine starts at its peak by design (the CZ's), the strike is the sound
 *   LOFI 3     the chip waves start at their edge, as the chips did
 *   VOICE 5    formant buzz: starts at the pulse
 *   TRIO 6     the three oscillators start at phase 0, the model
 *   WHEEL 7    tonewheel / bar starts at its excitation
 *   PHYS 9     a struck string / membrane starts with its excitation: the hit is the sound
 *   NOISE 11   noise has no start phase
 *   FM6 12     msfa / Dexed (the DX7 reference): carrier phase 0, the operators' gains ramp across the first block (rate 99 = one block), so a loud low note starts with an edge
 *   WAVE 14    the table read starts at phase 0 of its frame
 * VOICE_STEP_LEN: 64 samples = 1.45 ms. A step counts when it is bigger than VOICE_STEP_K x the old note's last change per sample + VOICE_STEP_MIN (32768 = full scale). */
#ifndef VOICE_STEP_LEN
#define VOICE_STEP_LEN 64u
#define VOICE_STEP_K 2
#define VOICE_STEP_MIN 256
#endif
static const uint16_t VOICE_OWN_START = 0x0000u;
static __attribute__((noinline)) void voice_step(voice_t *v, int32_t *dst, int32_t b0, uint32_t n)
{
    uint32_t i;
    if (v->offp) {                                      /* the first block of a re-strike (or of a fresh note, VOICE_OWN_START) */
        int32_t err = dst[0] - b0 - (v->lastc + v->lastd), ad = v->lastd < 0 ? -v->lastd : v->lastd, ae = err < 0 ? -err : err;
        v->offp = 0;
        if (ae > VOICE_STEP_K * ad + VOICE_STEP_MIN) {  /* (not the wave's own slope: only a jump the old note's last two samples did not promise) */
            v->offc = err;
            v->offn = (uint8_t)VOICE_STEP_LEN;
        }
    }
    for (i = 0; i < n && v->offn; i++, v->offn--)
        dst[i] -= (v->offc * (int32_t)v->offn) / (int32_t)VOICE_STEP_LEN;
}

static void cycle_start(track_t *t, voice_t *v, int sounding);   /* (cycle.c) */
static void voice_start(track_t *t, voice_t *v, uint32_t note, uint32_t vel, int glide)
{
    const engine_t *e = ENGINES[t->engine];
    int sounding = v->active && v->stage != 0;
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    int32_t s0 = v->s[0], s1 = v->s[1], s4 = v->s[4], s5 = v->s[5], s6 = v->s[6], s7 = v->s[7];
    uint32_t keep = e->keep;
    v->note = (uint8_t)note;
    v->vel = (uint8_t)vel;
    v->xg = t->lay_xg;                                  /* LAYER XFADE: this note's share (0: none) */
    v->gate = 1;
    v->active = 1;
    v->stage = 1;
    v->age = ++vage;
    v->pitch16 = (int32_t)note * 16;
    v->mvel = t->m_vel;                                 /* mod.c: the note's VEL and RAND */
    v->mrnd = t->m_rnd;
    t->m_vi = (uint8_t)(v - t->v);
    glide_set(t, v, glide);
    v->apitch = t->atg_note ? (int32_t)t->atg_note * 16 : v->pitch16;   /* ARP TARGET: at the arp's note now */
    v->again = t->atg_vel * 258;
    v->stage2 = 1;                                      /* ENV 2 (DUELL): an attack, from where it is (as ENV) */
    v->offn = 0;                                        /* (hunt3: a step still being cancelled ends with the old note) */
    v->offp = (uint8_t)(sounding || ((VOICE_OWN_START >> t->engine) & 1u));
    if (!sounding)
        v->lastc = v->lastd = 0;
    cycle_start(t, v, sounding);                        /* CYCLE: a rise, from where the generator stands (cycle.c) */
    if (!sounding) {
        v->env = 0;
        v->env_out = 0;
        v->env2 = 0;
        v->uft = 0;                                     /* the multimode filter from rest */
        v->chn = VCH_FRESH;                              /* the voice chain: a note that has not rendered yet (voice_chain.h) */
        v->side = t->sp_alt;                            /* SPREAD: left, right, left .. (UNISON: mono_play); a */
        t->sp_alt ^= 1u;                                /* sounding voice keeps its side (no jump) */
    }                                                   /* sounding: the attack starts from the current level */
    if (TRK_HIDDEN(t)) {                                /* DUELL: tracks 3..4 never sound */
        v->active = v->gate = 0;
        v->stage = 0;
        return;
    }
    e->note_on(t, v);
    if (sounding && !e->sampled) {                     /* retrigger / steal: keep phases and filter */
        v->ph[0] = ph0;                                 /* states (resetting them clicks) */
        v->ph[1] = ph1;
        v->ph[2] = ph2;
        if (keep & 0x01u)                               /* engine_t.keep: the slots the engines use */
            v->s[0] = s0;
        if (keep & 0x02u)
            v->s[1] = s1;
        if (keep & 0x10u)
            v->s[4] = s4;
        if (keep & 0x20u)
            v->s[5] = s5;
        if (keep & 0x40u)
            v->s[6] = s6;
        if (keep & 0x80u)
            v->s[7] = s7;
    }
}

/* MONO / LEGATO / UNISON: one note on one voice (eight for UNISON, or the
 * engine's voice cap, spread by DETUNE over the same width) */
static void mono_play(track_t *t, uint32_t note, uint32_t vel, int retrig, int glide)
{
    uint32_t mode = (uint32_t)t->p[P_VOICE], nv = mode == V_UNISON ? trk_unison(t) : 1u, i;
    voice_trim(t);
    voice_pool_room(t, nv);                             /* (rule G22: this part's fixed need is its own: the other part makes room if it holds it) */
    for (i = 0; i < nv; i++) {
        voice_t *v = &t->v[i];
        int32_t k = 2 * (int32_t)i - (int32_t)(nv - 1u);   /* -7 .. 7 */
        if (VS_FROZEN(t, i))
            continue;                                   /* (core 1's, half written: this note does not reach it) */
        if (nv > 1u && nv < NVOICE_UNISON)
            k = k * (NVOICE_UNISON - 1) / (int32_t)(nv - 1u);      /* fewer voices: the outer ones as wide */
        v->fine = nv > 1u ? k * t->p[P_DETUNE] * 56 / 889 : 0;   /* up to ~±40 cents */
        if (retrig || !v->active || !v->gate) {
            /* level: about the same sum for 8 or 4 voices at random phases */
            voice_start(t, v, note, nv >= NVOICE_UNISON ? vel * 36u / 100u : nv > 1u ? vel / 2u : vel, glide);
            v->side = (uint8_t)(i & 1u);                    /* SPREAD: the detuned voices alternate */
            if (nv > 1u && i && !ENGINES[t->engine]->sampled) {   /* random start phases: */
#if DUELL_BENCH
#define MONO_SEED bench_mono_seed                           /* (bench builds: at file level, so the per-test reset can put it back: bench.c) */
#else
                static uint32_t seed = 0x1234567u;          /* in phase they stack, evenly spread they cancel */
#define MONO_SEED seed
#endif
                MONO_SEED = MONO_SEED * 1664525u + 1013904223u;
                v->ph[0] += MONO_SEED;
            }
        } else {                                            /* legato: new pitch, same envelope */
            v->note = (uint8_t)note;
            v->pitch16 = (int32_t)note * 16;
            v->mvel = t->m_vel;
            v->mrnd = t->m_rnd;
            v->xg = t->lay_xg;
            if (vel > v->vel && nv == 1u)
                v->vel = (uint8_t)vel;
            glide_set(t, v, glide);
        }
    }
    t->mono_note = (uint8_t)note;
}

/* the note the mono stack asks for (PRIO: LAST / LOW / HIGH), 0 = none */
static uint32_t mono_pick(const track_t *t)
{
    uint32_t i, n;
    if (!t->nmono)
        return 0;
    n = t->mono_stack[t->nmono - 1u];
    for (i = 0; i < t->nmono; i++) {
        if (t->p[P_PRIO] == 1 && t->mono_stack[i] < n)
            n = t->mono_stack[i];
        if (t->p[P_PRIO] == 2 && t->mono_stack[i] > n)
            n = t->mono_stack[i];
    }
    return n;
}

static void mono_remove(track_t *t, uint32_t note)
{
    uint32_t j, k = 0;
    for (j = 0; j < t->nmono; j++)
        if (t->mono_stack[j] != note)
            t->mono_stack[k++] = t->mono_stack[j];
    t->nmono = (uint8_t)k;
}

static void trk_note_on_x(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t any = 0, i, mode = trk_vmode(t);
    if (t->p[P_MUTE])
        return;
    if (t->xf_on || t->eng_req != t->engine) {          /* engine switch under way: after the fade */
        for (i = 0; i < t->xp_n && t->xp_note[i] != note; i++)
            ;
        if (i == t->xp_n && t->xp_n < 4u)
            t->xp_n++;
        i = i < t->xp_n ? i : t->xp_n - 1u;             /* (full: the last one is replaced) */
        t->xp_note[i] = (uint8_t)note;
        t->xp_vel[i] = (uint8_t)vel;
        return;
    }
    mod_note(t, note, vel);                             /* the matrix's VEL / KEY / RAND */
    for (i = 0; i < NVOICE; i++)
        any |= t->v[i].gate;
    if (!any && !t->arp_ph) {                       /* fresh phrase: the LFO restarts at PHS only with TRIG NOTE (FREE, the default since 0.2.2, never restarts the phase), and its FADE starts over with either (an arp is one phrase: its steps after the first are not fresh) */
        if (!t->p[P_LTRIG])
            t->lfo_ph = (uint32_t)t->p[P_LPHASE] << 25;
        t->lfo_fade = 0;
        if (!t->p[P_L2TRIG])                            /* DUELL: LFO 2 the same */
            t->lfo2.ph = (uint32_t)t->p[P_L2PHASE] << 25;
        t->lfo2.fade = 0;
    }
    if (mode == V_POLY) {
        voice_t *v = voice_alloc(t, note);
        t->nmono = 0;                                   /* no stale mono stack after a mode change */
        t->mono_note = 0;
        v->fine = 0;
        voice_start(t, v, note, vel, t->p[P_GLIDE] != 0);
        return;
    }
    {
        uint32_t sounding = t->mono_note && t->v[0].active && t->v[0].gate, want;
        mono_remove(t, note);
        if (t->nmono >= 8u)
            mono_remove(t, t->mono_stack[0]);
        t->mono_stack[t->nmono++] = (uint8_t)note;
        want = mono_pick(t);
        if (sounding && want == t->mono_note)
            return;                                     /* LOW / HIGH priority: this key does not win */
        if (mode == V_MONO)                             /* MONO: always retrigger, glide when GLIDE is set */
            mono_play(t, want, vel, 1, t->p[P_GLIDE] != 0 || t->slide_glide);
        else                                            /* LEGATO / UNISON: glide only between held notes */
            mono_play(t, want, vel, !sounding, sounding && (t->p[P_GLIDE] != 0 || t->slide_glide));
    }
}

/* a note-on of part t: what the voice and the engine read as it starts (DTUNE, LFO PHASE, GLIDE, an engine's note-on
 * values) with the MORPH fader's values (morph.c), as its block will have them */
#if defined(DUELL_TWOCORE) && DUELL_TWOCORE && TC_MEAS
static uint32_t ev_t[5];                                       /* (2026-11-09) the event step: 0 the parts' loop, 1 the held-back starts + strum + keys, 2 the MIDI drain, 3 its CCs, 4 how many CCs (x ticks a us) */
static uint32_t ns_ticks, ns_n, ns_mo;                         /* BENCHVE: the time this block spent in note starts (the MORPH window, the voice's allocation and start, the engine's note-on), and how many: twocore_run.c tsg */
#endif
static void trk_note_on_now(track_t *t, uint32_t note, uint32_t vel)
{
#if defined(DUELL_TWOCORE) && DUELL_TWOCORE && TC_MEAS
    const uint32_t ns_t0 = TC_TICKS();
#endif
    int mo = morph_begin(t, 0);
#if defined(DUELL_TWOCORE) && DUELL_TWOCORE && TC_MEAS
    ns_mo += TC_TICKS() - ns_t0;
#endif
    trk_note_on_x(t, note, vel);
    if (mo)
        morph_end(t);
#if defined(DUELL_TWOCORE) && DUELL_TWOCORE && TC_MEAS
    ns_ticks += TC_TICKS() - ns_t0;
    ns_n++;
#endif
}

/* NOTE-START STAGGER (DUELL 1.1 item 144; owner: internal, not a setting). A burst of new voices in ONE block (a chord, several keys or MIDI notes at once)
 * is what made core 1's job late on the unit (HANDOFF item 129 / 139: every POLY-MAX failure at 7..8 voices a part came 7..15 ms into the note-on burst; the
 * steady load at the same count was fine). So a POLY part starts at most VS_MAX voices in one block; the next note-ons of that block wait in the part's queue
 * (vs_q, in order) and start in the following blocks, VS_MAX a block (vs_block, at the head of events_block): 8 voices at once start one a block, the last
 * 7 blocks (5.1 ms) late; a single key starts at once, the second tone of a pair 1 block (0.73 ms) late, a triad's top 2 blocks (1.46 ms) late; a single key is never held back unless a burst is still
 * waiting. VS_MAX = 1 (owner-accepted 2026-11-03; was 2, and 3 in test8 / BENCHV6, item 144; 3 was not enough for WHEEL / FM6 on the unit, item 145 E): a measured voice start on the host (tests/cost_cal.txt's scale; scratch probe of item 144) costs ~940..2000 host instructions for the
 * note-on itself (0.15..0.35 of one voice's block) and its first blocks render within +-2 % of the steady ones, so the unit's burst is not the start code
 * but the fresh voices' first blocks (cache, inferred from v4: +60..+360 us over steady at 8 voices = ~7..45 us a fresh voice); a few fresh voices a block keep a
 * part's burst well below what POLY-MAX showed clean at 6 voices starting together (with VS_MAX 1 a triad's third tone is two blocks late). A test build may set another.
 * Only POLY parts are held back (MONO / LEGATO / UNISON: one note-on is one gesture; UNISON's voices start together as before, they are counted), and only a
 * note that starts a voice from rest: a re-strike of a note the part sounds already (an arp or sequencer step on a held chord) restarts its voice in place
 * with its states kept, as cheap as any note-on, so it plays at once and is not counted.
 * The queue lives in events_block's context only: every note-on and note-off of a part comes from there (keys, MIDI, arp, sequencer, STRUM, an engine
 * switch's waiting notes), on core 0, before core 1's job for the block is posted, so it is the same whichever core renders part 2 (two cores, the take-over,
 * one core). A note-off of a waiting note cancels it (trk_note_off), a panic / STOP ALL / MODE change empties it (trk_all_off). Starting later through
 * trk_note_on_x, a waiting note meets the voice cap, stealing and the NOTES budget exactly as one played then would. */
#ifndef VS_MAX
#define VS_MAX 1u                                       /* new voices a part starts in one block */
#endif
#define VS_QN 16u                                       /* note-ons a part may hold back (full: the next one starts at once; nothing is ever dropped) */
typedef struct { uint8_t note, vel; uint16_t xg; } vs_ent_t;
static vs_ent_t vs_q[NTRK][VS_QN];
static uint8_t vs_qn[NTRK], vs_n[NTRK];                 /* waiting; started this block */
static uint32_t vs_held;                                /* note-ons held back since power-on (tests, console) */
/* note sounds on part t already (a re-strike: voice_alloc restarts that voice in place, its states kept: no start from rest, nothing to stagger) */
static int vs_sounding(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].stage != 4u && t->v[i].note == note)
            return 1;
    return 0;
}
static void trk_note_on_1(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t p = (uint32_t)(t - trk);
    if (p < NTRK && !t->p[P_MUTE] && !(trk_vmode(t) == V_POLY && vs_sounding(t, note))) {
        if (trk_vmode(t) == V_POLY && (vs_qn[p] || vs_n[p] >= VS_MAX) && vs_qn[p] < VS_QN) {
            vs_ent_t *e = &vs_q[p][vs_qn[p]++];         /* (in order behind the ones already waiting) */
            e->note = (uint8_t)note;
            e->vel = (uint8_t)vel;
            e->xg = t->lay_xg;                          /* (LAYER XFADE: the share it was played with) */
            vs_held++;
            return;
        }
        vs_n[p] = (uint8_t)(vs_n[p] + (trk_vmode(t) == V_UNISON ? trk_unison(t) : 1u));
    }
    trk_note_on_now(t, note, vel);
}
/* a block starts (seq.c events_block, before its keys, MIDI, arp and steps): the counts start again, the waiting note-ons start, VS_MAX a part, oldest first */
static __attribute__((noinline)) void vs_block(void)
{
    uint32_t p, i, k;
    for (p = 0; p < NTRK; p++) {
        track_t *t = &trk[p];
        vs_n[p] = 0;
        for (k = 0; k < vs_qn[p] && vs_n[p] < VS_MAX; k++) {
            uint16_t xg = t->lay_xg;
            t->lay_xg = vs_q[p][k].xg;
            vs_n[p]++;
            trk_note_on_now(t, vs_q[p][k].note, vs_q[p][k].vel);
            t->lay_xg = xg;
        }
        for (i = 0; k + i < vs_qn[p]; i++)
            vs_q[p][i] = vs_q[p][k + i];
        vs_qn[p] = (uint8_t)i;
    }
}
/* a note-off: the same note waiting in part t's queue never starts */
static void vs_cancel(track_t *t, uint32_t note)
{
    uint32_t p = (uint32_t)(t - trk), i, k = 0;
    if (p >= NTRK)
        return;
    for (i = 0; i < vs_qn[p]; i++)
        if (vs_q[p][i].note != note)
            vs_q[p][k++] = vs_q[p][i];
    vs_qn[p] = (uint8_t)k;
}

/* DUELL step 5, SPEC 5b.1 LAYER: nothing here. The notes reach both parts as INPUT (seq.c key_on / key_off, midi_control.c
 * midi_play / midi_release: each part's own input_on / input_off, chord and arp), so every part sounds what its own arp
 * (or its direct play) asks, one voice per note from the shared budget (NVOICE) */
#define trk_note_on trk_note_on_1
static void trk_note_off_1(track_t *t, uint32_t note);
#define trk_note_off trk_note_off_1

static void trk_note_off_1(track_t *t, uint32_t note)
{
    uint32_t i, k = 0, mode = trk_vmode(t);
#if DUELL_BENCH
    bench_noff++, bench_noff_ms = fm1_ms;               /* (BENCHV8 latch, bench builds only) */
#endif
    vs_cancel(t, note);                                 /* (item 144: held back by the start limit: it never starts) */
    for (i = 0; i < t->xp_n; i++)                       /* not sounding yet (engine switch): forget it */
        if (t->xp_note[i] != note) {
            t->xp_note[k] = t->xp_note[i];
            t->xp_vel[k++] = t->xp_vel[i];
        }
    t->xp_n = (uint8_t)k;
    mono_remove(t, note);                              /* a mode change must not retain a released key */
    if (mode != V_POLY) {
        uint32_t nv = mode == V_UNISON ? trk_unison(t) : 1u;
        if (t->mono_note == note) {
            uint32_t next = mono_pick(t);
            if (next) {                                 /* fall back to a held note, legato */
                mono_play(t, next, t->v[0].vel, 0, t->p[P_GLIDE] != 0);
            } else {
                for (i = 0; i < nv; i++)
                    if (t->v[i].gate) {
                        if (VS_FROZEN(t, i)) {          /* (core 1's, half written: let it go at the release) */
                            sh_pend_off |= (uint16_t)(1u << i);
                            continue;
                        }
                        t->v[i].gate = 0;
                        t->v[i].stage = 3;
                    }
                t->mono_note = 0;
            }
        }
    }
    for (i = 0; i < NVOICE; i++)                        /* a fallback above already has its new pitch */
        if (t->v[i].note == note && t->v[i].gate) {
            if (VS_FROZEN(t, i)) {                      /* (core 1's, half written: let it go at the release) */
                sh_pend_off |= (uint16_t)(1u << i);
                continue;
            }
            t->v[i].gate = 0;
            t->v[i].stage = 3;
        }
}

static void trk_all_off_1(track_t *t)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++) {
        if (VS_FROZEN(t, i)) {                          /* (core 1's, half written: let it go at the release) */
            sh_pend_off |= (uint16_t)(1u << i);
            continue;
        }
        t->v[i].gate = 0;
        t->v[i].stage = !t->v[i].active ? 0 : t->v[i].stage == 4u ? 4 : 3;   /* (item 135: a voice already fading out (voice_kill: STOP ALL, CC120, a steal, the NOTES trim) keeps
                                                                             * its one-block fade; made a release it sounded on for its whole RELEASE, e.g. STOP ALL and a load in one block) */
    }
    t->nmono = 0;
    t->mono_note = 0;
    t->xp_n = 0;
    if ((uint32_t)(t - trk) < NTRK)
        vs_qn[t - trk] = 0;                             /* (item 144: nothing held back by the start limit starts after it) */
}
#define trk_all_off trk_all_off_1

/* engine switch, at each block start (events_block), before any note of the block. The UI writes
 * eng_req and the new engine's P_E0..P_E7 together (IRQ off). The part's sounding voices (released by
 * the preset change) then fade out over XF_BLOCKS blocks on the old engine, rendered with its own
 * parameters (pe_old: an engine never reads another engine's values, which index its tables); only
 * then the engine switches and the notes that came during the fade start on it. A part with nothing
 * sounding switches at once. */
#define XF_BLOCKS 4u                                    /* 4 x 32 samples: 2.9 ms */
static void engine_block(track_t *t)
{
    uint32_t i, any = 0;
    if (VS_SHARE && t == &trk[0] && sh_busy)            /* a frozen set (core 1 may still render with this engine): no switch and no engine memory change until the release */
        return;
    if (t->xf_on || t->eng_req != t->engine) {
        if (!t->xf_on) {
            for (i = 0; i < NVOICE; i++)
                any |= t->v[i].active;
            if (any) {
                t->xf_on = 1;
                t->xf = XF_BLOCKS;
                return;
            }
        } else if (t->xf) {
            return;                                     /* still fading (track_render counts down) */
        }
        for (i = 0; i < NVOICE; i++) {                  /* faded to 0: gone */
            voice_t *v = &t->v[i];
            v->active = v->gate = 0;
            v->stage = 0;
            v->env = v->env_out = 0;
        }
        t->engine = eng_idx(t->eng_req);
        eng_mem_claim(t);                               /* its state region, before the waiting notes */
        t->xf_on = 0;
        t->nmono = 0;
        t->mono_note = 0;
        {
            uint32_t n = t->xp_n;
            t->xp_n = 0;
            for (i = 0; i < n; i++)
                trk_note_on_1(t, t->xp_note[i], t->xp_vel[i]);   /*  */
        }
    }
    eng_mem_claim(t);                                   /* (an engine set directly: power-on, a test) */
    for (i = 0; i < 8u; i++)                            /* the engine's own values, for a later fade */
        t->pe_old[i] = t->p[P_E0 + i];
}

/* ENV SYNC (1.5, #175; P_ESYNC, params.c ESYNC_NAMES): ATK DEC REL as note values of the tempo (BPM, or the external
 * clock's measured tempo: fx.c beat_samples, as LFO 2 SYNC). The part's three steps, made once a block before its
 * voices (track_render; the parts render one at a time), as gen_tables.py makes ENV_LIN / ENV_EXP for a time in ms:
 * the attack's linear step 2^24 CTL / T (rounded), the decay's and release's 1 - e^-x (x = 4.6 CTL / T: ~99 % of the way
 * after T), here as x - x^2/2 + x^3/6 - x^4/24 (x <= 0.33 at the shortest, 1/64T at 240 BPM: under 0.04 % off). The
 * coefficient is Q16 as ENV_EXP's: past ~2 s it is a few units, the time within half of one (4BAR at 90 BPM: 2.5 %).
 * Value 0 (and a time below a block) takes the tables' 0, as with SYNC OFF. OFF: the tables' values, as before (looked
 * up once a block here instead of once a voice in env_tick: the same numbers) */
static uint32_t beat_samples(void);                /* (fx.c) */
_Static_assert(CTL == 32, "esync_one: 4.6 CTL in Q16 is 9646899");
/* t->esk[3]: ATK's step, DEC's and REL's coefficient (Q16): the part's, this block (env_tick); t->esk2[3] .. ENV 2's (DUELL, env2_tick; made only while the matrix reads it).
 * Fields of track_t (job 3: they were two globals the parts' stages both wrote) */
static uint32_t esync_one(uint32_t quarter, int32_t v, uint32_t i)
{
    uint32_t u = ESYNC_UNITS[esync_idx(v)], T = quarter * u / 96u, x, x2, x3, x4;   /* T: samples */
    if (T < CTL)
        return i ? ENV_EXP[0] : ENV_LIN[0];
    if (!i)
        return ((1u << 24) * CTL + T / 2u) / T;
    x = (9646899u + T / 2u) / T;                        /* 4.6 CTL (CTL 32), Q16, rounded */
    if (x > 49152u)                                     /* (below ~200 samples: 0.75, the series 0.3 % low) */
        x = 49152u;
    x2 = x * x >> 16;
    x3 = x2 * x >> 16;
    x4 = x3 * x >> 16;
    x = x - x2 / 2u + x3 / 6u - x4 / 24u;
    return x < 1u ? 1u : x > 65535u ? 65535u : x;
}
/* the steps of the envelope whose ATK DEC REL are ids a, d, r of t into k (ENV 1: P_ATK..; DUELL's ENV 2: P_E2ATK..) */
static void esync_coefs(const track_t *t, uint32_t a, uint32_t d, uint32_t r, uint32_t *k)
{
    const int16_t *p = t->p;
    if (p[P_ESYNC]) {
        uint32_t q = beat_samples();
        k[0] = esync_one(q, p[a], 0);
        k[1] = esync_one(q, p[d], 1);
        k[2] = esync_one(q, p[r], 2);
    } else {                                            /* OFF: the tables, as always */
        k[0] = ENV_LIN[p[a] & 127];
        k[1] = ENV_EXP[p[d] & 127];
        k[2] = ENV_EXP[p[r] & 127];
    }
}
static __attribute__((noinline)) void esync_coef(const track_t *t)   /* (not inlined: its divides stay out of the
                                                                    * audio ISR's loop over the voices) */
{
    esync_coefs(t, P_ATK, P_DEC, P_REL, ((track_t *)t)->esk);
}
static __attribute__((noinline)) void esync2_coef(const track_t *t)
{
    esync_coefs(t, P_E2ATK, P_E2DEC, P_E2REL, ((track_t *)t)->esk2);
}

#include "cycle.c"                                      /* the CYCLE envelope (cloud job F): cycle_start, cycle_tick, cycle_value */
/* ENV 2 (DUELL, SPEC 2.4): a second ADSR per voice, ENV 1's stages and times (ENV SYNC too: esync2_coef): the filter
 * envelope of the classic arrangement (ENV 2 DEST: FLT, SHP, track_render) and a matrix source (mod.c MS_ENV2). It follows the gate: an attack as the voice starts (voice_start; a legato note keeps it), the
 * release from its level when the key is let go; at 0 it stays there. It never ends the voice (ENV 1 does). Ticked once
 * per control tick of a voice while an active matrix slot reads it (mod.e2) or an ENV 2 DEST depth is set: otherwise no
 * cost, no change to the sound */
static void env2_tick(const track_t *t, voice_t *v)
{
    int32_t sus = (int32_t)t->p[P_E2SUS] << 17;         /* Q24 */
    if (!v->gate && v->stage2 && v->stage2 < 3u)
        v->stage2 = 3;
    switch (v->stage2) {
    case 1:
        v->env2 += (int32_t)t->esk2[0];
        if (v->env2 >= (1 << 24)) {
            v->env2 = 1 << 24;
            v->stage2 = 2;
        }
        break;
    case 2:
        v->env2 += mulq16(sus - v->env2, t->esk2[1]);
        break;
    case 3:
        v->env2 -= mulq16(v->env2, t->esk2[2]);
        if (v->env2 < (1 << 12)) {
            v->env2 = 0;
            v->stage2 = 0;
        }
        break;
    default:
        v->env2 = 0;
        break;
    }
}

/* one control tick (CTL samples) of the amplitude envelope; returns Q15 */
static int32_t env_tick(track_t *t, voice_t *v)
{
    const int16_t *p = t->p;
    int32_t sus = (int32_t)p[P_SUS] << 17;              /* Q24 */
    switch (v->stage) {
    case 1:
        v->env += (int32_t)t->esk[0];                  /* ATK's step (esync_coef: the table's, or ENV SYNC's) */
        if (v->env >= (1 << 24)) {
            v->env = 1 << 24;
            v->stage = 2;
        }
        break;
    case 2:
        v->env += mulq16(sus - v->env, t->esk[1]);
        break;
    case 3:
        v->env -= mulq16(v->env, t->esk[2]);
        if (v->env < (1 << 12)) {
            v->env = 0;
            v->stage = 0;
            v->active = 0;
        }
        break;
    case 4:                                             /* given up for another part: this block fades it */
        v->env = 0;
        v->stage = 0;
        v->active = 0;
        break;
    default:
        v->env = 0;
        break;
    }
    return v->env >> 9;
}

/* SPREAD (#148, fx.c mix_spread): while the part's spread_on (fx.c part_voice sets it for the part it renders), track_render renders
 * the voices of side 0 into the part's sbuf and the others into out, then adds sbuf to out: out is every voice, as always
 * (the engines add into either the same way), sbuf the left ones. A cost per block, none per voice. The flag and the buffer are the
 * part's own (core.h pscr[]): two parts' blocks share nothing here */
static __attribute__((noinline)) void spread_clear(int32_t *sbuf)
{
    uint32_t i;
    for (i = 0; i < CTL; i++)
        sbuf[i] = 0;
}
static __attribute__((noinline)) void spread_sum(int32_t *out, const int32_t *sbuf)
{
    uint32_t i;
    for (i = 0; i < CTL; i++)
        out[i] += sbuf[i];
}

/* ARP TARGET (SPEC 2.3, seq.c atg_fire): the voice's arp pitch towards the arp's note now (the root's while there is none),
 * with the part's GLIDE as a note's (RATE: GLD's speed; 0: a jump), as the offset adelta over the root's pitch16 (so
 * TUNE, bend, LFO, ENV and the matrix move both alike); its level towards the step's gate (velocity, 0 off) in ~3 ms */
static __attribute__((noinline)) void atg_voice(const track_t *t, voice_t *v, vmod_t *m)
{
    int32_t want = t->atg_note ? (int32_t)t->atg_note * 16 : v->pitch_cur, d = want - v->apitch, g;
    if (d && !t->p[P_GLIDE]) {
        v->apitch = want;
    } else if (d) {
        int32_t st = (int32_t)(ENV_LIN[t->p[P_GLIDE] & 127] >> 14);
        if (st < 1)
            st = 1;
        v->apitch += d > 0 ? (d < st ? d : st) : (-d < st ? d : -st);
    }
    m->adelta = v->apitch - v->pitch_cur;
    g = t->atg_vel * 258 - v->again;
    v->again += g > -4 && g < 4 ? g : g / 4;
    m->again = v->again;
}

/* DUELL 0.2.9 MULTIMODE FILTER (SPEC 2.5; 0.2.2's universal filter): a per-voice state-variable filter on the engines with no filter of
 * their own (engine_t.ufilt 1: all of them since cloud job J; ANALOG's VCF and NOISE's filter ARE this filter, TRIO's lives on as filter 2). FILTER page: K1 CUT (P_FREEA, 0..UF_STEPS: 30 Hz .. 16 kHz, core.h),
 * K2 RES (P_FREEB: the damping from 2.0, Q 0.5, to 0.145, Q 6.9; the 24 dB path is unchanged), K3 MODE (P_FREEC: 0 LP, 64 BP, 127 HP and every blend between:
 * the SVF's low-pass, band-pass and high-pass outputs summed with weights), K4 SLOPE (P_FREED: 12 dB, or 24 dB: a second stage of
 * the same MODE, the two dampings a 4th-order Butterworth's, 1.848 and 0.765, RES narrowing the second down to 0.10).
 * The EFFECTIVE cut = CUT + every cutoff route (ENV 1 / ENV 2 / LFO 1 / LFO 2 FLT, the matrix's CUT; 1/256 dial steps, 63 steps = the
 * whole of an old dial: their depths as in 0.2.2), per voice and block, clamped to 0..UF_STEPS; the accent's opening does not apply.
 * KEY TRK (0.2.21, P_KTRK): the effective cut also follows the played note (core.h UF_KT: -100..+100 %, around C3, ANALOG's KTR centre), added per voice
 * and block before the clamp, so the coefficient of uf_run just takes another cut (it is worked out every block anyway: nothing new in the hot loop, no click
 * on a turn); 0 adds nothing. With CUT at the open end a positive KEY TRK closes the notes below C3, so the bypass test below is on the effective cut.
 * BYPASS: MODE LP with the effective cut at the open end (RES and SLOPE do not matter there): the voice renders straight into the
 * part's buffer (no work, bit for bit as without a filter): the defaults, so every sound of before plays as it did. (A store of before 0.2.9, with 0.2.2's
 * bipolar CUT, was converted at its load into this one until DUELL 0.2.14; it is no longer read.)
 * KILL ZONES: within the last 4 dial steps at an end the output's share that the end closes fades to 0 (the low-pass and band-pass
 * shares to 30 Hz, the high-pass and band-pass ones to 16 kHz), so a filter fully closed is silent. The weights are worked out per
 * block (Q10) and ramp over it from the last block's (a MODE turn, a route through a kill zone: no click).
 * The filter is the trapezoidal SVF of dsp.c (ANALOG's, TRIO's: unconditionally stable at any cutoff and resonance; the integrators
 * clamp at +-150000, the second stage's input at +-65536, the output goes through a soft knee at the voice's full level, 48000), run on
 * the voice AFTER its amplitude, in a scratch block (the engine renders into a cleared buffer, the filtered block is added to the
 * part's). It starts from rest (the integrators charged with the block's first sample: no step) when a voice starts or after a
 * bypass; the second stage starts charged with the first's low-pass state, and a SLOPE turned while it sounds fades the second stage
 * in (out) over one block */
#define UF_TOP (UF_STEPS << 8)                          /* the open end, Q8 dial steps */
/* the three engines that lost their own filter (cloud job J, TWIN_FILTER_DESIGN_2 1.2): the accent (velocity over 110 opens the filter with the envelope) lifts BOTH filters on them, filter 1
 * (in its effective cutoff) and filter 2 even with LINK off (chain_f2.c f2_cut); on the other engines the lift stays the engine's own tone control and filter 2's linked cutoff */
#define ACCENT_BOTH(e) ((e) == &ENG_ANALOG || (e) == &ENG_TRIO || (e) == &ENG_NOISE)
#define UF_KZ (4 << 8)                                  /* the kill zones: 4 dial steps (= 1.0 in Q10, the weights' unit) */
#define UF_K(res) (8192 - (res) * 7600 / 127)           /* damping, Q12: 12 dB. 2.0 down to 0.145 (the law of dsp.c tsvf_coef, ANALOG's VCF's: TWIN_FILTER_DESIGN_2 question 2a; until the engine filters left it was 1.414 down to 0.145: 5793 - res * 5200 / 127) */
#define UF_K24A 7568                                    /* 24 dB: the first stage's (1.848), the second's from 0.765 down with RES */
#define UF_K24B(res) (3135 - (((res) * 2731) >> 7))   /* (0.765 down to 0.104) */
#define UF_KNEE 48000
static __attribute__((noinline)) void uf_coef(tsvf_t *f, int32_t idx, int32_t k) { tsvf_coef_k(f, idx, k); }   /* (one copy: a divide) */
/* one voice's block through the filter, in place: uc the effective cut (Q8 dial steps), p the part's values (RES MODE SLOPE) */
static __attribute__((noinline)) void uf_run(voice_t *v, int32_t *buf, uint32_t n, int32_t uc, const int16_t *p)
{
    tsvf_t f, g;
    uint32_t i, s24 = p[P_FREED] == 2, run2, lp;
    int32_t md = clamp(p[P_FREEC], 0, UF_HP), res = clamp(p[P_FREEB], 0, 127), k1 = s24 ? UF_K24A : UF_K(res), k2 = UF_K24B(res);
    int32_t bl = 0, blend = 0;                           /* MORPH (0.2.9): the two ends' SLOPEs cross-faded, 0 (12 dB) .. 32 (24 dB) */
    int32_t wb = md <= 64 ? md << 4 : ((UF_HP - md) * 65) >> 2, wl = md < 64 ? 1024 - wb : 0, wh = md > 64 ? 1024 - wb : 0;   /* MODE: LP .. BP
                                                         * .. HP (Q10; 1024 / 63 a step above BP, no divide) */
    int32_t gl = uc < UF_KZ ? uc : UF_KZ, gt = UF_TOP - uc < UF_KZ ? UF_TOP - uc : UF_KZ, idx = 40 + ((uc * 99) >> 6);
    int32_t cx = (wh * gt) >> 10, cb = ((((wb * gl) >> 10) * gt) >> 10) - cx, cl = ((wl * gl) >> 10) - cx;   /* the output: cx x + cb band-pass
                                                         * + cl low-pass (Q10; the kill zones in them), this block's */
    int32_t a1 = v->uf[0], a2 = v->uf[1], b1 = v->uf[2], b2 = v->uf[3], ax, ab, al, dx, db, dl, fd = 0, xg;
    if (v->msl) {                                        /* (v->msl: the part's MXF slope weight, bit 15 set when the other end is 24 dB; track_render sets it) */
        int32_t b24 = v->msl & 0x8000u ? 32 : 0;
        bl = s24 ? 32 : 0;
        bl += ((b24 - bl) * (int32_t)(v->msl & 0x7FFFu)) >> 15;
        s24 = bl >= 32;
        blend = bl > 0 && bl < 32;
        if (blend)
            k1 = UF_K(res) + (((UF_K24A - UF_K(res)) * bl) >> 5);   /* (the first stage's damping between the two) */
    }
    if (!v->uft) {                                      /* from rest: no ramp */
        a1 = a2 = 0;
        v->ufw[0] = (int16_t)cx; v->ufw[1] = (int16_t)cb; v->ufw[2] = (int16_t)cl;
    } else if (!blend && v->uft != 1u + s24) {
        fd = s24 ? 1 : -1;                              /* SLOPE turned while sounding: the second stage fades in (out) over the block */
    }
    if (!cx && !a1 && !a2)
        a2 = buf[0] >> 1;                               /* (no high-pass share: the low-pass charged with the signal, no step at the start; 0.2.2's rule) */
    if ((s24 || blend) && v->uft != 2u) {               /* the second stage starts: charged with the first's low-pass */
        b1 = 0;
        b2 = a2;
    }
    v->uft = (uint8_t)(1u + (s24 || blend));
    run2 = s24 || blend || fd;
    xg = fd > 0 ? 0 : 32;
    uf_coef(&f, idx, k1);
    if (run2)
        uf_coef(&g, idx, k2);
    ax = v->ufw[0] << 5; ab = v->ufw[1] << 5; al = v->ufw[2] << 5;   /* (CTL 32: the weights ramp, Q15, exactly onto this block's) */
    dx = cx - v->ufw[0]; db = cb - v->ufw[1]; dl = cl - v->ufw[2];
    lp = !(dx | db | dl | cx | cb) && cl == 1024;      /* a steady low-pass: its output is the low-pass itself (0.2.2's loop's cost) */
    for (i = 0; i < n; i++) {
        int32_t x = buf[i] >> 1, v3 = x - a2, v1 = (f.a1 * a1 + f.a2 * v3) >> 13, v2 = a2 + ((f.a2 * a1 + f.a3 * v3) >> 13), y = v2;
        a1 = clamp(2 * v1 - a1, -150000, 150000);
        a2 = clamp(2 * v2 - a2, -150000, 150000);
        if (!lp) {
            ax += dx; ab += db; al += dl;
            y = ((ax >> 5) * x + (ab >> 5) * ((k1 * v1) >> 12) + (al >> 5) * v2) >> 10;
        }
        if (run2) {                                     /* 24 dB: the same MODE again */
            int32_t z = clamp(y, -65536, 65536), u3 = z - b2, u1 = (g.a1 * b1 + g.a2 * u3) >> 13, u2 = b2 + ((g.a2 * b1 + g.a3 * u3) >> 13);
            b1 = clamp(2 * u1 - b1, -150000, 150000);
            b2 = clamp(2 * u2 - b2, -150000, 150000);
            u2 = lp ? u2 : ((ax >> 5) * z + (ab >> 5) * ((k2 * u1) >> 12) + (al >> 5) * u2) >> 10;
            if (fd) {
                xg += fd;
                u2 = y + (((u2 - y) * xg) >> 5);
            } else if (blend) {
                u2 = y + (((u2 - y) * bl) >> 5);
            }
            y = u2;
        }
        buf[i] = soft_knee(y << 1, UF_KNEE);
    }
    v->ufw[0] = (int16_t)cx; v->ufw[1] = (int16_t)cb; v->ufw[2] = (int16_t)cl;
    v->uf[0] = a1; v->uf[1] = a2; v->uf[2] = b1; v->uf[3] = b2;
}
/* one voice of a ufilt engine with the filter on: the engine into a cleared scratch block, filtered, added to dst */
static __attribute__((noinline)) void uf_voice(const engine_t *e, track_t *t, voice_t *v, int32_t *dst, uint32_t n,
                                               const vmod_t *m, int32_t uc)
{
    int32_t tmp[CTL];
    uint32_t i;
    if (n > CTL)
        n = CTL;
    for (i = 0; i < n; i++)
        tmp[i] = 0;
    e->render(t, v, tmp, n, m);
    uf_run(v, tmp, n, uc, t->p);
    for (i = 0; i < n; i++)
        dst[i] += tmp[i];
}
/* DUELL 0.2.25: the bypass edge without a click (owner: CUTOFF turned between 81 and the open end 82 crackled, #83 TINE ROOM sits at 82). The
 * bypass (MODE LP, the effective cut at the open end) is dry; the filter one step below is a low-pass near 16 kHz with its own phase and
 * soft knee, and it started from rest: the step between the two was ~4 x the music's sharpest. A voice that crosses the edge while it
 * sounds now has one block cross-faded: in = the filter starts (from rest, as before) and the block goes from the dry signal to the filter's;
 * out = the filter runs one more block at the open end and the block goes from it to the dry signal. Out of line, in the flash (uf_run and
 * uf_voice, hot, are untouched); a voice that never crosses, and a note's start, play bit for bit as before. v->uft 3: bypassed while sounding
 * (the next filtered block fades in); 0 as ever: from rest (a new note) */
#define UF_BYP 3u
static __attribute__((noinline)) void uf_edge(const engine_t *e, track_t *t, voice_t *v, int32_t *dst, uint32_t n, const vmod_t *m, int32_t uc, int in)
{
    int32_t tmp[CTL], dry[CTL];
    uint32_t i;
    if (n > CTL)
        n = CTL;
    for (i = 0; i < n; i++)
        tmp[i] = 0;
    e->render(t, v, tmp, n, m);
    for (i = 0; i < n; i++)
        dry[i] = tmp[i];
    uf_run(v, tmp, n, uc, t->p);
    for (i = 0; i < n; i++) {
        int32_t w = (int32_t)(in ? i + 1u : n - 1u - i);  /* (the filter's share, of n) */
        dst[i] += dry[i] + (tmp[i] - dry[i]) * w / (int32_t)n;   /* (32-bit: |a difference| < 2^18, x 32) */
    }
}
#include "voice_chain.c"                                /* cloud job B: the voice chain (voice_chain.h), its stage hooks chain_f2.c / chain_sub.c (jobs E, F) and its sequence; uf_run, uf_voice, uf_edge as they are */
/* MASTER TUNE (DUELL 0.2.22): G_TUNE is the frequency of A4 in 0.1 Hz (TUNE_MIN 4153 .. TUNE_MAX 4662, TUNE_DEF 4400 = 440.0 Hz); a value outside the range (an older
 * project's cents, a stray write) reads as 440.0. The offset 1200 log2(f / 440) is worked out ONCE, when the value changes (tune_get: the last value is kept), in the form
 * the voice already adds: whole 1/16 semitones (*u) and a rest in 1/4096 of the increment (*f; 1 ct = 2.367), both towards zero as the cents were. log2 by repeated squaring
 * (Q30, 24 bits), integers only. The rest is on the 1/4096 grid, so the pitch is within 0.21 ct of the exact one (440.0 is exactly 0 / 0: the sound of TUNE 0). */
static int16_t tune_key = TUNE_DEF;
static int16_t tune_u, tune_f;
static void tune_get(int32_t *u, int32_t *f)
{
    int32_t g = song.g[G_TUNE];
    if (g != tune_key) {
        int32_t su = 0, sf = 0;
        if (g >= TUNE_MIN && g <= TUNE_MAX && g != TUNE_DEF) {
            uint64_t x = ((uint64_t)(g > TUNE_DEF ? g : TUNE_DEF) << 30) / (uint64_t)(g > TUNE_DEF ? TUNE_DEF : g);   /* the ratio >= 1, Q30 */
            uint32_t l = 0, i, w, r;
            for (i = 0; i < 24u; i++) {
                x = (x * x) >> 30;
                l <<= 1;
                if (x >= (2ull << 30)) {
                    x >>= 1;
                    l |= 1u;
                }
            }
            w = l * 192u;                              /* 1/16 semitones, Q24 (a 1/16 semitone = 6.25 ct) */
            r = (uint32_t)((((uint64_t)(w & 0xFFFFFFu)) * 14794u / 1000u + (1u << 23)) >> 24);   /* the rest: 6.25 ct x 2.367 = 14.794 per 1/16 semitone */
            su = (int32_t)(w >> 24);
            sf = (int32_t)r;
            if (g < TUNE_DEF)
                su = -su, sf = -sf;
        }
        tune_key = (int16_t)g;
        tune_u = (int16_t)su;
        tune_f = (int16_t)sf;
    }
    *u = tune_u;
    *f = tune_f;
}
/* VOICE SHARE stage B1 (2026-11-03): the RENDER STEP of one voice, split from track_render's voice loop. The loop body above it is the CONTROL STEP and stays on core 0 (envelopes, glide, pitch, the matrix, accent,
 * STEPS, the engines' pre-hooks formant_pre / phys_pre; the `continue` exits that end or skip a voice; everything that writes the part: t->, the LFOs, the matrix's m_*, cycle ticks). It hands this function the
 * voice's vmod_t. The render step reads t / t->p / the engine memory and writes ONLY: voice v (phases, filter and chain state, uft, chn, msl, lastc / lastd, the re-strike step), the engine's per-voice slots
 * (f2s, sub_*, FM6 / PHYS note slots, drw_v, all indexed by the voice) and the buffers of rc (c->out, and c->sbuf for the SPREAD left side): a core that renders a subset of a part's voices passes its own
 * pair of buffers. Called today for every voice on the one core with the part's own buffers, in voice order, right after the voice's control step: the result is the old loop's, bit for bit.
 * ONE copy (owner 2026-11-04, size fix): a single noinline function, placed in range Z (tools/ram_fns_z.py, group CORE 1), called by part 2's one-pass branch, core 0's pass 2, the take-over and core 1's set loop. The block length is
 * the constant CTL INSIDE the function (not c->n, which is unused now), so uf_run / uf_voice still fold it. Three inlined copies (about 550-600 B each) were what the pool tail could not hold */
typedef struct {
    track_t *t;
    const engine_t *e;
    int32_t *out, *sbuf;                                /* the part's block and its SPREAD left side, or core 1's own pair (core.h pscr_c1) */
    uint32_t n, spread_on, msl, uf;                     /* (n: not read any more, voice_render uses the constant CTL; kept so the initialisers stay as they are) */
    int32_t ucut0;
} vrctx_t;
static __attribute__((noinline)) void voice_render(const vrctx_t *c, voice_t *v, vmod_t *m)
{
    track_t *t = c->t;
    const engine_t *e = c->e;
    const int16_t *p = t->p;
    const uint32_t n = CTL, uf = c->uf, msl = c->msl;   /* (the block length is the constant here: the hot filter code folds it) */
    int32_t ucut0 = c->ucut0;
    m->ucut = m->ucn = UF_TOP;                          /* filter 1's effective cutoff (core.h vmod_t): set below / by chain_uc where the engine has the multimode filter */
    int32_t *dst = c->spread_on > v->side ? c->sbuf : c->out, b0 = dst[0], bn = dst[n - 1], bm = dst[n - 2];   /* (hunt3: the voice's own contribution is what it adds to dst) */
#define VSTEP() do { if (v->offp | v->offn) voice_step(v, dst, b0, n); v->lastc = dst[n - 1] - bn; v->lastd = v->lastc - (dst[n - 2] - bm); } while (0)
    {
        uint32_t on = chain_on(t, v) | chain_f1_on(e, t, v, m, ucut0);   /* the voice chain: TWIN / SUB on, or (CHAIN_ALL, voice_chain.h) filter 1 runs */
        if (on | (v->chn == VCH_RUN)) {      /* the voice chain (cloud job B, voice_chain.c): engine, SUB, filter 1, filter 2, then the amp; a voice entering or leaving it is cross-faded over a block */
            chain_block(e, t, v, dst, n, m, ucut0, msl, on);
            VSTEP();                                    /* (hunt3 + cloud: the re-strike step cancellation sees the chain's output too) */
            return;
        }
    }
    v->chn = VCH_OFF;
    if (uf) {                                           /* the multimode filter: the cutoff routes are its own */
        v->msl = (uint16_t)msl;
        int32_t kt = p[P_KTRK] ? UF_KT(p[P_KTRK], v->pitch_cur) : 0;   /* KEY TRK (0.2.21): the note's pitch around C3, an octave of cutoff per octave at +100 %; 0: nothing added, nothing computed */
        int32_t uc = clamp(ucut0 + m->cutoff + kt, 0, UF_TOP);   /* the effective cut: the dial plus every route (and the key) */
        int32_t ucn = clamp(ucut0 + m->cutoff - m->acc1 + kt, 0, UF_TOP);   /* the same without the accent (filter 2 LINK, NOISE's make-up) */
        m->cutoff = accent_lift(v->vel, m->envq15);     /* the routes are the filter's, whatever CUT is; the accent's lift stays the engine's own (its tone control, as ever) */
        m->ucut = uc;
        m->ucn = ucn;
        if ((uc < UF_TOP || p[P_FREEC] > 0) && !BYP(t, BY_MULTI)) {   /* (the bypass: MODE LP, the effective cut at the open end; DUELL 1.1: or the BYPASS switch) */
            if (v->uft == UF_BYP) {                     /* (0.2.25: out of the bypass while sounding: from rest, faded in over the block) */
                v->uft = 0;
                uf_edge(e, t, v, dst, n, m, uc, 1);
            } else {
                uf_voice(e, t, v, dst, n, m, uc);
            }
            VSTEP();
            return;
        }
        if (v->uft && v->uft != UF_BYP) {               /* (0.2.25: into the bypass while sounding: the filter's last block fades to dry) */
            uf_edge(e, t, v, dst, n, m, UF_TOP, 0);
            v->uft = UF_BYP;
            VSTEP();
            return;
        }
        v->uft = UF_BYP;                                /* bypassed while it sounds (the filter from rest, faded in, if it comes back) */
    }
    e->render(t, v, dst, n, m);                         /* (SPREAD: the left ones) */
    VSTEP();
#undef VSTEP
}
/* VOICE SHARE stage B2 step 2: the render of a SET of the render list. rl[0..nl-1] are the slots the control pass listed (voice order), vm the control results by RANK (the part's stretch of vr_m); bit r of sel names the voice of RANK r in the
 * list (not the slot: the allocator leaves holes), so any subset of the list renders in voice order into the buffers of c. Core 0's pass 2 calls it with the complement of the share mask (every voice while the mask is
 * empty: the old loop's result bit for bit), the take-over of a late core 1 calls it with the mask itself into pscr_c1 (then vs_merge). Only the loop: the render step is the one noinline voice_render (range Z), so inlining
 * this loop into core 0's pass 2 adds no second copy of the render step */
static inline void vs_render_set(const vrctx_t *c, const uint8_t *rl, uint32_t nl, uint32_t sel, vmod_t *vm)
{
    uint32_t r;
    for (r = 0; r < nl; r++)
        if ((sel >> r) & 1u)
            voice_render(c, &c->t->v[rl[r]], &vm[r]);
}
/* the merge (after core 1's done word, or after the take-over): core 1's partial block and SPREAD left side added into the part's own, before spread_sum. Integer adds: exact in any order. The caller clears pscr_c1
 * before the set renders into it. Nothing calls it while the mask is empty (the compiler drops it) */
static __attribute__((noinline, unused)) void vs_merge(int32_t *out, int32_t *sbuf, uint32_t spread_on, const part_scr_c1_t *c1)
{   /* (c1: pscr_c1, or core 1's second set for part 2, twocore_run.c pscr_c1b, 2026-11-09) */
    uint32_t k;
    for (k = 0; k < CTL; k++)
        out[k] += c1->buf[k];
    if (spread_on)
        for (k = 0; k < CTL; k++)
            sbuf[k] += c1->sbuf[k];
}
/* VOICE SHARE stage B2 step 4: core 1's entry to the set render (twocore_run.c tc_share_run) and the take-over's. Only the loop over voice_render (both in range Z: .c1_text in range X cannot reach flash). The block length
 * is the constant CTL inside voice_render, so the context is passed as it is */
static __attribute__((noinline, unused)) void vs_render_set_c1(const vrctx_t *c, const uint8_t *rl, uint32_t nl, uint32_t sel, vmod_t *vm)
{
    vs_render_set(c, rl, nl, sel, vm);
}
/* TEST SEAM (host tests only, tests/voicesplit_test.c): with DUELL_VS_TEST defined, vs_mask_fn(block, nr) chooses, per block of part 1, the set of ranks core 1 would render (NULL or 0: none); track_render then renders
 * that set in-thread into pscr_c1 and merges it. In the firmware the macro is 0u and everything behind it is dropped */
#ifdef DUELL_VS_TEST
uint32_t (*vs_mask_fn)(uint32_t block, uint32_t nr);
static uint32_t vs_blk;
#define VS_MASK(nl) (vs_mask_fn ? vs_mask_fn(vs_blk++, (nl)) & (uint32_t)((1u << (nl)) - 1u) : 0u)
#else
#define VS_MASK(nl) 0u
#endif
/* render one block of a part into out (cleared here); returns the voices rendered */
/* Channel bend is live performance state, outside projects/presets. Q8 semitones. */
static int32_t midi_bend_q8[NTRK], midi_bend_target[NTRK];
/* A part's block between its control pass and its finish (the two-pass order: every part's control first, then the renders on either core, then the part's own stages): the render step's
 * block constants and core 0's buffers, the render list (the slots in voice order; the control result of rank r is vr_m[r0 + r]) and what the finish needs */
typedef struct {
    vrctx_t rc;
    uint8_t rl[NVOICE];
    uint8_t nl, r0, fade;
    uint16_t nr;                                        /* the voices that render this block */
    int16_t pe_new[8];                                  /* (the engine switch's fade: the new engine's values, put back by the finish) */
} vpass_t;
/* vp 0: ONE pass, each voice rendered right after its control step and the block finished here (track_render). vp set: the control pass only (track_ctl): the results go to vr_m from *nrec on (one set of
 * NVOICE records for all parts; a voice that finds none left renders right away into the part's own buffers), the renders follow (track_voices, or a set on the other core), then track_finish */
/* FM6 AND THE COMB SHARE MEMORY (owner 2026-11-06: FM6 12 voices, 6 with a COMB, the COMB's lines where they were). FM6's records 7 .. 12 and the COMB's lines are the same bytes of the part's engine
 * memory; eng_fm6.c fm6_comb says whose they are, and this is the only place that changes it: once a block in the part's control step (core 0), before any voice of the block renders on either core.
 *   to the COMB: when TYPE is COMB and no FM6 voice is left in slots 6 and up. Any that still sound there fade now (voice_kill, the one-block fade), and the COMB plays LP until they are gone (a block or two);
 *   back to FM6: when TYPE is no longer COMB and no sounding voice still runs a COMB (the running type or the one fading out, F2_FADE blocks). Until then FM6 keeps to its first 6 (trk_nvoice_eng).
 * Flash code (noinline): the control step is core 0's */
static __attribute__((noinline)) void f2_fm6_share(track_t *t)
{
    uint32_t pi = (uint32_t)(t - trk), i, want;
    if (pi >= NLIVE)
        return;
    want = f2_ty(t) == TT_COMB;
    if (want && !fm6_comb[pi]) {
        uint32_t busy = 0;
        if (t->engine == ENGI_FM6)
            for (i = FM6_POLY_COMB; i < NVOICE; i++)
                if (t->v[i].active) {
                    busy = 1;
                    if (t->v[i].stage != 4u)
                        voice_kill(&t->v[i]);
                }
        if (!busy)
            fm6_comb[pi] = 1;
    } else if (!want && fm6_comb[pi]) {
        for (i = 0; i < NVOICE_COMB; i++) {
            const f2s_t *s = &f2s[pi][i];
            if (t->v[i].active && t->v[i].chn == VCH_RUN && (s->ty == TT_COMB || (s->fade && s->oty == TT_COMB)))
                return;
        }
        fm6_comb[pi] = 0;
    }
}
static inline __attribute__((always_inline)) uint32_t track_pass(track_t *t, int32_t *out, uint32_t n, vpass_t *vp, uint32_t *nrec)
{
    const engine_t *e = ENGINES[t->engine];
    const int16_t *p = t->p;
    uint32_t i;
    const mod_win_t *mod = &MODW(t);
    uint32_t msl = MXF(t).on && MXF(t).w[MX_SLOPE] ? MXF(t).w[MX_SLOPE] | (MXF(t).b[MX_SLOPE] == 2u ? 0x8000u : 0u) : 0u;   /* the filter's SLOPE cross-fade (uf_run) */
    const part_scr_t *scr = &pscr[PART_OF(t)];          /* (SPREAD: this part's scratch and flag, set by fx.c part_voice) */
    int32_t *spread_buf = (int32_t *)scr->sbuf;
    uint32_t spread_on = scr->spread_on;
    int32_t lfo = mulq15(t->lfo_val, t->lfo_fade);
    /* DUELL (SPEC 2.4, the classic arrangement): ENV 2 DEST / LFO 2 DEST, their own depths: either set, that one runs
     * (as when the matrix reads it); all 0, neither costs anything (every sound of before) */
    uint32_t e2on = (mod->on && mod->e2) || p[P_E2FLT] || (p[P_TWTYPE] && XV(t, X_ENV2));   /* (TWIN FILTER: filter 2's ENV 2 amount) */
    uint32_t l2on = (mod->on && mod->l2) || (p[P_TWTYPE] && XV(t, X_LFO2));   /* (DUELL 1.1: LFO 2 DEST's old PITCH / MULTI CUTOFF / TIMBRE cards no longer act: they reach the engine through the matrix) */   /* (.. and its LFO 2 amount) */
    int32_t tune, tune_fine;                           /* MASTER TUNE (tune_get above): whole 1/16 semitones in the pitch, the rest as a fine factor (no dead zone) */
    int32_t bend, bend16, bend_fine;
    uint32_t ti = (uint32_t)(t - trk);
    tune_get(&tune, &tune_fine);
    if (ti >= NLIVE)                                   /* DUELL: a hidden part never renders (no engine memory) */
        return 0;
    {
        int32_t d = midi_bend_target[ti] - midi_bend_q8[ti];
        /* ~6 ms smoothing, with an exact landing (no permanent small offset). */
        midi_bend_q8[ti] += d > 0 ? (d < 4 ? d : (d + 3) / 4) : (d > -4 ? d : (d - 3) / 4);
    }
    bend = midi_bend_q8[ti];
    bend16 = bend >= 0 ? bend / 16 : -((-bend + 15) / 16);
    bend_fine = (bend - bend16 * 16) * 2367 / 2560;
    uint32_t nr = 0, fade = t->xf_on && t->xf, abits = atgt_bits(t), uf = e->ufilt;
    int32_t ucut0 = uf ? clamp(p[P_FREEA], 0, UF_STEPS) << 8 : 0;   /* the multimode filter's dial (Q8) */
    int16_t pe_new[8];
    const vrctx_t rc = {t, e, out, spread_buf, CTL, spread_on, msl, uf, ucut0};   /* (the render step's block constants and buffers; core 0 passes the part's own; n is always CTL: the constant lets the hot code fold the block length) */
    const uint32_t two = vp != 0;                       /* the control pass only: the results kept in vr_m, the renders after every part's control */
    if (two)
        vp->nl = 0, vp->r0 = (uint8_t)*nrec;
    for (i = 0; i < n; i++)
        out[i] = 0;
    if (mod->on && mod->ldep < 32767)                     /* the matrix's DEPTH (mod.c) */
        lfo = mulq15(lfo, mod->ldep);
    lfo = mx_slew(t, NMSLOT, lfo);                      /* (hunt1: LFO 1's value for its own depths (VIB, CUT, SHP, AMP) moves at most MX_SLEW a block like a matrix source's: a saw's or a square's edge, an S&H step or
                                                         * a restart of the LFO glides over 5 to 10 blocks (3.6 to 7.3 ms) instead of stepping the pitch / cutoff / shape at one block boundary; an LFO that moves less passes unchanged) */
    if (spread_on)
        spread_clear(spread_buf);
    if (fade)                                           /* engine switch: the old engine, its own values */
        for (i = 0; i < 8u; i++) {
            pe_new[i] = t->p[P_E0 + i];
            t->p[P_E0 + i] = t->pe_old[i];
        }
    track_lfo_tick(t);
    if (l2on)                                           /* DUELL: LFO 2, read by the matrix or LFO 2 DEST */
        lfo_tick(t, &t->lfo2, P_L2RATE, P_L2SYNC, 0);
    esync_coef(t);                                      /* this block's envelope steps for env_tick (ENV SYNC) */
    if (e2on)                                           /* DUELL: ENV 2, read by the matrix or ENV 2 DEST */
        esync2_coef(t);
    if (fm6_comb[ti] || p[P_TWTYPE] == (int16_t)TT_COMB)   /* FM6's upper records and the COMB's lines are the same memory: whose it is this block */
        f2_fm6_share(t);
    if (e->block)                                       /* the engine's per-part work (WHEEL: bars, rotor) */
        e->block(t);
    for (i = 0; i < NVOICE; i++) {
        voice_t *v = &t->v[i];
        vmod_t m;
        int32_t env, pitch;
        if (!v->active || VS_FROZEN(t, i))              /* (a frozen voice is core 1's, half written: no control, no render, no sound this block) */
            continue;
        {
            env = env_tick(t, v);
            if (e->ownenv && v->active) {               /* the engine's envelopes (FM6): they end the voice */
                if (e->done(t, v)) {
                    v->active = v->gate = 0;
                    v->stage = 0;
                    v->env = v->env_out = 0;
                    continue;
                }
                v->env = 1 << 24;                       /* (the ADSR's release never ends it) */
                env = 32767;
            }
            if (e2on)                                   /* DUELL: ENV 2 (read by the matrix or ENV 2 DEST) */
                env2_tick(t, v);
            if (t->cy)                                  /* CYCLE (cycle.c), read by an active matrix slot: ticked here, not in mod_voice (hot RAM) */
                cycle_tick(t, v);
            if (e->amp)                                 /* the engine's own amplitude curve */
                env = e->amp(t, v, env);
            m.envq15 = e->ownenv ? 0 : env;
            m.amp1 = e->ownenv ? env : mulq15(env, v->vel * 258);
            if (v->xg)                                  /* LAYER XFADE (core.h mix_xf): the note's share between the parts */
                m.amp1 = mulq15(m.amp1, v->xg);
            if (p[P_LD_AMP] && !BYP(t, BY_LFO1))
                m.amp1 = mulq15(m.amp1, 32767 - mulq15(p[P_LPOL] ? lfo : (lfo + 32768) >> 1, p[P_LD_AMP] * 258));
            if (fade)                                   /* linear to 0 over the fade */
                m.amp1 = m.amp1 * (int32_t)(t->xf - 1u) / (int32_t)XF_BLOCKS;
            m.amp0 = v->env_out;
            v->env_out = m.amp1;
        }
        if (v->pitch_cur != v->pitch16) {               /* glide */
            int32_t d = v->pitch16 - v->pitch_cur;
            int32_t st = v->gstep ? v->gstep : (int32_t)(ENV_LIN[(p[P_GLIDE] ? p[P_GLIDE] : 40) & 127] >> 14);   /* slide: ~45 ms */
            if (st < 1)
                st = 1;
            v->pitch_cur += d > 0 ? (d < st ? d : st) : (-d < st ? d : -st);
        }
        if (!env && !m.amp0 && v->stage == 2 && !e->sampled) {
            v->lastc = v->lastd = 0;
            continue;
        }                                   /* held at a silent sustain (SUS 0): nothing to render */
        pitch = v->pitch_cur + tune + bend16 + ((lfo * DEP_AMT(p[P_LD_PIT]) * 3) >> 15) + ((m.envq15 * DEP_AMT(p[P_ED_PIT]) * 3) >> 15);
        m.pitch16 = clamp(pitch, 0, 2047);
        m.inc = pitch_inc(m.pitch16);
        m.fine = v->fine + tune_fine + bend_fine;
        if (v->fine + tune_fine + bend_fine)             /* residual below 1/16 semitone */
            m.inc += (uint32_t)((int32_t)(m.inc >> 12) * (v->fine + tune_fine + bend_fine));
        m.cutoff = ((lfo * DEP_AMT(p[P_LD_FLT])) >> 7) +
                   (((E2Q(t, v)) * DEP_AMT(p[P_E2FLT])) >> 7); /* (DUELL 1.1: AMP ENV's and LFO 2's cutoff depths no longer act; FILTER ENV's MULTI CUTOFF and LFO 1's stay) */
        if (e == &ENG_NOISE)                            /* NOISE's DRFT wander of the cutoff, worked out by the block before (eng_noise.c): filter 1's, one block late */
            m.cutoff += v->s[0];
        m.acc1 = 0;
        if (v->vel > 110 && (uf != 1u || ACCENT_BOTH(e)))   /* accent opens the filter with the env: filter 1 on the three engines whose own filter went (ACCENT_BOTH), the engine's own tone control on the others (below) */
        {
            int32_t al = accent_lift(v->vel, m.envq15);
            if (ACCENT_BOTH(e))
                al = m.acc1 = accent_dir(al, accent_dir_mode(p[P_FREEC]));   /* filter 1 opens the way its MODE opens (LP up, HP down, BP none) */
            m.cutoff += al;
        }
        m.shape = (64 << 8) + ((lfo * DEP_AMT(p[P_LD_SHP])) >> 7) + ((m.envq15 * DEP_AMT(p[P_ED_SHP])) >> 7);   /* (DUELL 1.1: FILTER ENV's and LFO 2's TIMBRE depths no longer act) */
        m.amask = abits;                                /* ARP TARGET (SPEC 2.3): 0, every unit on the root */
        if (abits)
            atg_voice(t, v, &m);
        if (mod->on) {                                   /* the modulation matrix (mod.c) */
            mod_voice(t, v, &m, v->fine + tune_fine + bend_fine);
            if (t->cy)                                   /* its second list (cloud job B, mod.c): CYCLE's per-note routes, after mod_voice .. */
                mod_cycle_voice(t, v, &m, v->fine + tune_fine + bend_fine);
            /* (B1 of the voice share: the voice's ENV for filter 2's sum is its own m.envq15, read by mod_f2_voice through the chain's vmod_t: no part scalar t->venv written here any more) */
        }
        if (e == &ENG_WAVE && p[P_E6])                  /* WAVE STEPS (eng_wave.c): POS lands on one of STEPS places; the block ramp is the cross-fade between two steps */
            wave_steps(t, v, &m);
        if (e == &ENG_FORMANT)                          /* VOICE SHARE B1: the engines' state that voices of one part used to hand on inside their render, done here in voice order (control step) */
            formant_pre(t, v, n);
        else if (e == &ENG_PHYS)
            phys_pre(t, v);
        nr++;
        if (two && *nrec < VR_RECS) {                      /* the control pass ends here for this voice: its result kept (rank order), the render comes after every part's control */
            vr_m[(*nrec)++] = m;
            vp->rl[vp->nl++] = (uint8_t)i;
        } else {
            voice_render(&rc, v, &m);                   /* one pass (or no record left): the render step right away: touches this voice, the output and SPREAD buffers of rc, nothing else shared */
        }
    }
    if (two) {                                          /* (track_finish does the rest, after the renders) */
        vp->rc = rc;
        vp->fade = (uint8_t)fade;
        vp->nr = (uint16_t)nr;
        if (fade)
            for (i = 0; i < 8u; i++)
                vp->pe_new[i] = pe_new[i];
        return nr;
    }
    if (spread_on)
        spread_sum(out, spread_buf);                                /* out: every voice; spread_buf: the left ones */
    if (fade) {
        for (i = 0; i < 8u; i++)
            t->p[P_E0 + i] = pe_new[i];
        t->xf--;
    }
    return nr;
}
/* render one block of a part into out (cleared here), in one pass; returns the voices rendered */
static uint32_t track_render(track_t *t, int32_t *out, uint32_t n)
{
    return track_pass(t, out, n, 0, 0);
}
/* THE TWO-PASS ORDER, step 1: a part's control pass (out cleared; nothing rendered but a voice past the last record). Returns the voices that render */
static uint32_t track_ctl(track_t *t, int32_t *out, vpass_t *vp, uint32_t *nrec)
{
    return track_pass(t, out, CTL, vp, nrec);
}
/* step 2: the listed voices of a part on this core, in voice order, into the part's own buffers. The host tests' seam (VS_MASK) renders its set of ranks into pscr_c1 and merges it, as another core would */
static void track_voices(const vpass_t *vp)
{
    const uint32_t all = (1u << vp->nl) - 1u, cm = VS_MASK(vp->nl);
    vs_render_set(&vp->rc, vp->rl, vp->nl, ~cm & all, vr_m + vp->r0);
#ifdef DUELL_VS_TEST
    if (cm) {
        vrctx_t rc1 = vp->rc;
        uint32_t k;
        rc1.out = pscr_c1.buf, rc1.sbuf = pscr_c1.sbuf;
        for (k = 0; k < CTL; k++)
            pscr_c1.buf[k] = pscr_c1.sbuf[k] = 0;
        vs_render_set_c1(&rc1, vp->rl, vp->nl, cm, vr_m + vp->r0);
        vs_merge(vp->rc.out, vp->rc.sbuf, vp->rc.spread_on, &pscr_c1);
    }
#endif
}
/* step 3: after every voice of the part rendered (and another core's were merged): SPREAD's sum and the engine switch's values back. Returns the voices rendered */
static uint32_t track_finish(track_t *t, const vpass_t *vp)
{
    uint32_t i;
    if (vp->rc.spread_on)
        spread_sum(vp->rc.out, vp->rc.sbuf);
    if (vp->fade) {
        for (i = 0; i < 8u; i++)
            t->p[P_E0 + i] = vp->pe_new[i];
        t->xf--;
    }
    return vp->nr;
}
