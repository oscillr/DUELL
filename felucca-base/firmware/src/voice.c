/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Voice allocation, envelopes, LFO and per-track rendering.
 * Runs in the audio ISR.
 *
 * Each synth part has its own NVOICE voices (so MONO / LEGATO / UNISON keep using
 * v[0..]), but only NVOICE of all the parts' voices sound at once: a part that starts
 * a voice while the budget is full takes one from any part (voice_victim): the oldest
 * released voice, else the oldest extra UNISON voice, else the oldest held voice of a
 * POLY part that is not its lowest note. A voice taken from another part fades out
 * over one block (stage 4: the envelope goes to 0, the block's amplitude ramp
 * declicks it); one of the part's own is restarted in place, as before. Extra UNISON
 * voices only start when there is room. */
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

/* the S&H value of a new LFO cycle. Parts 1..3 draw it from the shared generator (rng); part 4 has its
 * own xorshift, so the shared sequence, which also seeds the engines' noise (PHYS, VOICE, ..), does not
 * depend on part 4's LFO (the golden renders rely on it) */
#define LFO_SHARED_RNG 3u
static uint32_t lfo_rand(track_t *t)
{
    uint32_t s = t->lfo_rnd ? t->lfo_rnd : 0x9E3779B9u;
    if ((uint32_t)(t - trk) < LFO_SHARED_RNG)
        return rng();
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
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
    if (mxf.on && mxf.w[lfo1 ? MX_LWAVE : MX_L2WAVE]) {   /* DUELL MORPH: the other end's WAVE runs too, cross-faded */
        uint32_t x = lfo1 ? MX_LWAVE : MX_L2WAVE;
        l->val += ((lfo_wave_of(mxf.b[x], l->rnd, l->ph) - l->val) * (int32_t)mxf.w[x]) >> 15;
    }
    if (p[s + 2u])                                      /* UNI: 0..+1 (the depths reach as far, one way only) */
        l->val = (l->val + 32768) >> 1;
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
static const struct { const engine_t *e; uint8_t c; } VCOST[] = {   /* (tests/cpu_scenarios_test.c CPU_SCEN=P: the heaviest sound, its voices) */
    {&ENG_ANALOG, 51}, {&ENG_FM6, 61}, {&ENG_PHASE, 40}, {&ENG_LOFI, 24}, {&ENG_FORMANT, 60}, {&ENG_TRIO, 44}, {&ENG_WHEEL, 59},
    {&ENG_PHYS, 144}, {&ENG_NOISE, 47}, {&ENG_WAVE, 52},   /* (FM6: 6 voices, PHYS 3, VOICE 4; WAVE: #60 DIGITAL PAD less its 12 dB filter) */
};
/* voices the engine may use (POLY and UNISON): its cap, else all of them; DUELL 0.2.11: and the part's VOICE CAP */
static uint32_t trk_nvoice_eng(const track_t *t)
{
    uint32_t c = ENGINES[t->engine]->poly;
    return c && c < NVOICE ? c : NVOICE;
}
static uint32_t trk_nvoice(const track_t *t)
{
    uint32_t c = trk_nvoice_eng(t), v = vcap[(t - trk) % NTRK];
    return v && v < c ? v : c;
}
static uint32_t vc_eng_cap(const engine_t *e) { return e->poly && e->poly < NVOICE ? e->poly : NVOICE; }
static uint32_t vcap_shown(uint32_t p)                  /* the TEST screen: the voices part p may use (its sound's engine, the cap) */
{
    uint32_t c = vc_eng_cap(ENGINES[eng_idx(trk[p].eng_req)]);
    return vcap[p] && vcap[p] < c ? vcap[p] : c;
}
static uint32_t vc_filter(const engine_t *e, const int8_t *p)   /* the multimode filter's cost a voice in these values */
{
    if (!e->ufilt || (p[P_FREEA] >= UF_STEPS && p[P_FREEC] <= 0))
        return 0;                                       /* (the bypass: MODE LP, CUT open) */
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
    uint32_t h = k[1] > k[0], nh = c[h] < NVOICE ? c[h] : NVOICE, nl = c[!h] < NVOICE - nh ? c[!h] : NVOICE - nh;
    return VC_IDLE + nh * k[h] + nl * k[!h];
}
/* the caps for the per-voice costs k[0..1] and the engines' own caps e[0..1] into cap[0..1]; returns the predicted worst case with them */
static uint32_t vc_caps(const uint32_t *k, const uint32_t *e, uint32_t *cap, uint32_t layer)
{
    uint32_t w, h;
    cap[0] = e[0];
    cap[1] = e[1];
    if (layer)                                          /* LAYER: every note on both parts, the pool split evenly (4 + 4): a cap of 4 or */
        return VC_IDLE + (e[0] < NVOICE / 2u ? e[0] : NVOICE / 2u) * k[0] + (e[1] < NVOICE / 2u ? e[1] : NVOICE / 2u) * k[1];   /* more changes nothing */
    h = k[1] > k[0];
    while ((w = vc_worst(k, cap)) > VC_BUDGET && k[h] > k[!h] && cap[h] > VC_MIN)
        cap[h]--;                                       /* (one voice of the dearer part goes to the cheaper one: the pool stays 8) */
    if (w > VC_BUDGET) {                                /* the pair cannot get under it with caps of 4 or more: then each part as if it */
        for (h = 0; h < 2u; h++) {                      /* played alone (a part on its own fits), never below VC_MIN */
            uint32_t c = k[h] ? (VC_BUDGET - VC_IDLE) / k[h] : NVOICE;
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

/* the voice to give up for a new one (see the top); soft: only released voices and
 * extra voices of other non-POLY parts (also ones left after a mode change).
 * Returns its index, *pp its part; NVOICE = none */
static uint32_t voice_victim(const track_t *self, int soft, track_t **pp)
{
    uint32_t p, i, best = NVOICE, cat = 4;
    for (p = 0; p < NPART; p++) {
        track_t *t = &trk[p];
        uint32_t mode = trk_vmode(t);
        uint32_t low = mode == V_POLY ? lowest_held(t) : NVOICE;
        if (soft && t == self)
            continue;
        for (i = 0; i < NVOICE; i++) {
            const voice_t *v = &t->v[i];
            uint32_t c;
            if (!v->active || v->stage == 4u)
                continue;
            if (!v->gate)
                c = 1;                                  /* released */
            else if (mode != V_POLY && i > 0)
                c = 2;                                  /* extra UNISON, or a voice left by a mode / cap change */
            else if (!soft && mode == V_POLY && i != low)
                c = 3;                                  /* held, not the bass */
            else
                continue;
            if (c < cat || (c == cat && v->age < (*pp)->v[best].age)) {
                cat = c;
                best = i;
                *pp = t;
            }
        }
    }
    return best;
}

static uint32_t voice_kills;                            /* voices given up (budget, overload): console, hostsim */
static void voice_kill(voice_t *v)                      /* fade out over the next block (env_tick stage 4) */
{
    v->gate = 0;
    v->stage = 4;
    voice_kills++;
}

/* the budget is full: free a voice for part t. 0 = nothing to take (soft) */
static int voice_room(track_t *t, int soft)
{
    track_t *vp = 0;
    uint32_t k;
    if (voices_busy() < NVOICE)
        return 1;
    k = voice_victim(t, soft, &vp);
    if (k == NVOICE)
        return !soft;                                   /* hard: over the budget (cannot happen: each of the
                                                         * NPART < NVOICE parts protects one voice at most) */
    voice_kill(&vp->v[k]);
    return 1;
}

static voice_t *voice_reuse(track_t *t, voice_t *v)
{
    if (v->stage == 4u)
        voice_room(t, 0);                               /* its budget was taken before this block */
    return v;
}

/* POLY allocation (within the engine's voice cap): the same note again reuses its voice.
 * Else a free voice: ROTATE takes
 * the next one round-robin (release tails ring out), REUSE (or any GLIDE) takes
 * the free voice whose last pitch is closest, so poly portamento moves each
 * voice the shortest way. Steal: the oldest released voice, else the oldest
 * held one, never the lowest held note (the bass). A free voice needs room in the
 * shared budget: when the voice to give up is one of this part's, it is restarted
 * in place instead. */
static voice_t *voice_alloc(track_t *t, uint32_t note)
{
    uint32_t i, np = trk_nvoice(t), best = np, low = np, nfree = 0;
    int32_t bd = 0x7FFFFFFF;
    for (i = 0; i < np; i++) {
        if (t->v[i].active && t->v[i].note == note)
            return voice_reuse(t, &t->v[i]);
        nfree += !t->v[i].active;
    }
    if (nfree && voices_busy() >= NVOICE) {
        track_t *vp = 0;
        uint32_t k = voice_victim(t, 0, &vp);
        if (k < np && vp == t)
            return &t->v[k];                            /* our own: restart it in place (no click) */
        if (k < NVOICE)
            voice_kill(&vp->v[k]);
    }
    if (t->p[P_ALLOC] || t->p[P_GLIDE]) {
        for (i = 0; i < np; i++) {
            int32_t d = t->v[i].pitch_cur - (int32_t)note * 16;
            if (t->v[i].active)
                continue;
            d = d < 0 ? -d : d;
            if (!t->v[i].pitch_cur)
                d = 0x7FFFFFF0;                          /* never played: last choice */
            if (d < bd) {
                bd = d;
                best = i;
            }
        }
    } else {
        for (i = 0; i < np && best == np; i++) {
            uint32_t k = (t->rr + i) % np;
            if (!t->v[k].active)
                best = k;
        }
        t->rr = (uint8_t)((best == np ? t->rr : best) + 1u) % np;
    }
    if (best < np)
        return &t->v[best];
    for (i = 0; i < np; i++)                             /* the lowest held note is protected */
        if (t->v[i].gate && (low == np || t->v[i].note < t->v[low].note))
            low = i;
    for (i = 0; i < np; i++)                             /* oldest released (not one fading out for another part) */
        if (!t->v[i].gate && t->v[i].stage != 4u && (best == np || t->v[i].age < t->v[best].age))
            best = i;
    if (best == np)
        for (i = 0; i < np; i++)                         /* oldest held, not the bass */
            if (i != low && (best == np || t->v[i].age < t->v[best].age))
                best = i;
    return voice_reuse(t, &t->v[best == np ? 0 : best]);
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
    if (!sounding) {
        v->env = 0;
        v->env_out = 0;
        v->env2 = 0;
        v->uft = 0;                                     /* the multimode filter from rest */
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
    uint32_t mode = (uint32_t)t->p[P_VOICE], nv = mode == V_UNISON ? trk_nvoice(t) : 1u, i;
    for (i = 0; i < nv; i++) {
        voice_t *v = &t->v[i];
        int32_t k = 2 * (int32_t)i - (int32_t)(nv - 1u);   /* -7 .. 7 */
        if (nv > 1u && nv < NVOICE)
            k = k * (NVOICE - 1) / (int32_t)(nv - 1u);      /* fewer voices: the outer ones as wide */
        v->fine = nv > 1u ? k * t->p[P_DETUNE] * 56 / 889 : 0;   /* up to ~±40 cents */
        if (retrig || !v->active || !v->gate) {
            if ((!v->active || v->stage == 4u) && !voice_room(t, i > 0))
                continue;                                   /* no room for this extra UNISON voice */
            /* level: about the same sum for 8 or 4 voices at random phases */
            voice_start(t, v, note, nv >= NVOICE ? vel * 36u / 100u : nv > 1u ? vel / 2u : vel, glide);
            v->side = (uint8_t)(i & 1u);                    /* SPREAD: the detuned voices alternate */
            if (nv > 1u && i && !ENGINES[t->engine]->sampled) {   /* random start phases: */
                static uint32_t seed = 0x1234567u;          /* in phase they stack, evenly spread they cancel */
                seed = seed * 1664525u + 1013904223u;
                v->ph[0] += seed;
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
static void trk_note_on_1(track_t *t, uint32_t note, uint32_t vel)
{
    int mo = morph_begin(t, 0);
    trk_note_on_x(t, note, vel);
    if (mo)
        morph_end(t);
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
    for (i = 0; i < t->xp_n; i++)                       /* not sounding yet (engine switch): forget it */
        if (t->xp_note[i] != note) {
            t->xp_note[k] = t->xp_note[i];
            t->xp_vel[k++] = t->xp_vel[i];
        }
    t->xp_n = (uint8_t)k;
    mono_remove(t, note);                              /* a mode change must not retain a released key */
    if (mode != V_POLY) {
        uint32_t nv = mode == V_UNISON ? trk_nvoice(t) : 1u;
        if (t->mono_note == note) {
            uint32_t next = mono_pick(t);
            if (next) {                                 /* fall back to a held note, legato */
                mono_play(t, next, t->v[0].vel, 0, t->p[P_GLIDE] != 0);
            } else {
                for (i = 0; i < nv; i++)
                    if (t->v[i].gate) {
                        t->v[i].gate = 0;
                        t->v[i].stage = 3;
                    }
                t->mono_note = 0;
            }
        }
    }
    for (i = 0; i < NVOICE; i++)                        /* a fallback above already has its new pitch */
        if (t->v[i].note == note && t->v[i].gate) {
            t->v[i].gate = 0;
            t->v[i].stage = 3;
        }
}

static void trk_all_off_1(track_t *t)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++) {
        t->v[i].gate = 0;
        t->v[i].stage = t->v[i].active ? 3 : 0;
    }
    t->nmono = 0;
    t->mono_note = 0;
    t->xp_n = 0;
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
static uint32_t esync_k[3];      /* ATK's step, DEC's and REL's coefficient (Q16): the part's, this block (env_tick) */
static uint32_t esync2_k[3];     /* .. ENV 2's (DUELL, env2_tick; made only while the matrix reads it) */
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
    esync_coefs(t, P_ATK, P_DEC, P_REL, esync_k);
}
static __attribute__((noinline)) void esync2_coef(const track_t *t)
{
    esync_coefs(t, P_E2ATK, P_E2DEC, P_E2REL, esync2_k);
}

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
        v->env2 += (int32_t)esync2_k[0];
        if (v->env2 >= (1 << 24)) {
            v->env2 = 1 << 24;
            v->stage2 = 2;
        }
        break;
    case 2:
        v->env2 += mulq16(sus - v->env2, esync2_k[1]);
        break;
    case 3:
        v->env2 -= mulq16(v->env2, esync2_k[2]);
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
        v->env += (int32_t)esync_k[0];                  /* ATK's step (esync_coef: the table's, or ENV SYNC's) */
        if (v->env >= (1 << 24)) {
            v->env = 1 << 24;
            v->stage = 2;
        }
        break;
    case 2:
        v->env += mulq16(sus - v->env, esync_k[1]);
        break;
    case 3:
        v->env -= mulq16(v->env, esync_k[2]);
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

/* SPREAD (#148, fx.c mix_spread): while spread_on (mix_part sets it for the part it renders), track_render renders the
 * voices of side 0 into spread_buf and the others into out, then adds spread_buf to out: out is every voice, as always
 * (the engines add into either the same way), spread_buf the left ones. A cost per block, none per voice */
static uint8_t spread_on;
static int32_t spread_buf[CTL];
static __attribute__((noinline)) void spread_clear(void)
{
    uint32_t i;
    for (i = 0; i < CTL; i++)
        spread_buf[i] = 0;
}
static __attribute__((noinline)) void spread_sum(int32_t *out)
{
    uint32_t i;
    for (i = 0; i < CTL; i++)
        out[i] += spread_buf[i];
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
 * their own (engine_t.ufilt 1; the others keep theirs, unchanged: ANALOG, TRIO and NOISE, ufilt 2, run it IN SERIES after their own filter, on their
 * output, bypassed by default: their routes and CC74 / CC71 stay the engine's, the effective cut here is the CUT dial alone, the matrix's FRES moves RES). FILTER page: K1 CUT (P_FREEA, 0..UF_STEPS: 30 Hz .. 16 kHz, core.h),
 * K2 RES (P_FREEB: the damping from 1.41, Q 0.71, to 0.145, Q 6.9), K3 MODE (P_FREEC: 0 LP, 64 BP, 127 HP and every blend between:
 * the SVF's low-pass, band-pass and high-pass outputs summed with weights), K4 SLOPE (P_FREED: 12 dB, or 24 dB: a second stage of
 * the same MODE, the two dampings a 4th-order Butterworth's, 1.848 and 0.765, RES narrowing the second down to 0.10).
 * The EFFECTIVE cut = CUT + every cutoff route (ENV 1 / ENV 2 / LFO 1 / LFO 2 FLT, the matrix's CUT; 1/256 dial steps, 63 steps = the
 * whole of an old dial: their depths as in 0.2.2), per voice and block, clamped to 0..UF_STEPS; the accent's opening does not apply.
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
#define UF_KZ (4 << 8)                                  /* the kill zones: 4 dial steps (= 1.0 in Q10, the weights' unit) */
#define UF_K(res) (5793 - (res) * 5200 / 127)           /* damping, Q12 (dsp.c tsvf_coef_k): 12 dB */
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
    if (mxf.on && mxf.w[MX_SLOPE]) {
        int32_t b24 = mxf.b[MX_SLOPE] == 2u ? 32 : 0;
        bl = s24 ? 32 : 0;
        bl += ((b24 - bl) * (int32_t)mxf.w[MX_SLOPE]) >> 15;
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
/* render one block of a part into out (cleared here); returns the voices rendered */
/* Channel bend is live performance state, outside projects/presets. Q8 semitones. */
static int32_t midi_bend_q8[NTRK], midi_bend_target[NTRK];
static uint32_t track_render(track_t *t, int32_t *out, uint32_t n)
{
    const engine_t *e = ENGINES[t->engine];
    const int16_t *p = t->p;
    uint32_t i;
    int32_t lfo = mulq15(t->lfo_val, t->lfo_fade), lfo2 = 0;
    /* DUELL (SPEC 2.4, the classic arrangement): ENV 2 DEST / LFO 2 DEST, their own depths: either set, that one runs
     * (as when the matrix reads it); all 0, neither costs anything (every sound of before) */
    uint32_t e2on = (mod.on && mod.e2) || p[P_E2FLT] || p[P_E2SHP];
    uint32_t l2on = (mod.on && mod.l2) || p[P_L2PIT] || p[P_L2FLT] || p[P_L2SHP];
    /* TUNE in cents: whole 1/16 semitones in the pitch, the rest as a fine factor (no dead zone) */
    int32_t tune = song.g[G_TUNE] >= 0 ? song.g[G_TUNE] * 16 / 100 : -((-song.g[G_TUNE] * 16 + 99) / 100);
    int32_t tune_fine = (song.g[G_TUNE] * 16 - tune * 100) * 2367 / 16000;   /* rest, in 1/4096 (1 ct = 2.367) */
    int32_t bend, bend16, bend_fine;
    uint32_t ti = (uint32_t)(t - trk);
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
    for (i = 0; i < n; i++)
        out[i] = 0;
    if (mod.on && mod.ldep < 32767)                     /* the matrix's DEPTH (mod.c) */
        lfo = mulq15(lfo, mod.ldep);
    if (p[P_L2PIT] || p[P_L2FLT] || p[P_L2SHP]) {      /* LFO 2 as LFO 1: the block before's, FADE and DEPTH2 applied */
        lfo2 = mulq15(t->lfo2.val, t->lfo2.fade);
        if (mod.on && mod.ldep2 < 32767)
            lfo2 = mulq15(lfo2, mod.ldep2);
    }
    if (spread_on)
        spread_clear();
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
    if (e->block)                                       /* the engine's per-part work (WHEEL: bars, rotor) */
        e->block(t);
    for (i = 0; i < NVOICE; i++) {
        voice_t *v = &t->v[i];
        vmod_t m;
        int32_t env, pitch;
        if (!v->active)
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
            if (e->amp)                                 /* the engine's own amplitude curve */
                env = e->amp(t, v, env);
            m.envq15 = e->ownenv ? 0 : env;
            m.amp1 = e->ownenv ? env : mulq15(env, v->vel * 258);
            if (v->xg)                                  /* LAYER XFADE (core.h mix_xf): the note's share between the parts */
                m.amp1 = mulq15(m.amp1, v->xg);
            if (p[P_LD_AMP])
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
        if (!env && !m.amp0 && v->stage == 2 && !e->sampled)
            continue;                                   /* held at a silent sustain (SUS 0): nothing to render */
        pitch = v->pitch_cur + tune + bend16 + ((lfo * p[P_LD_PIT] * 3) >> 15) + ((m.envq15 * p[P_ED_PIT] * 3) >> 15) +
                ((lfo2 * p[P_L2PIT] * 3) >> 15);       /* (LFO 2 DEST PIT: as LFO 1's) */
        m.pitch16 = clamp(pitch, 0, 2047);
        m.inc = pitch_inc(m.pitch16);
        m.fine = v->fine + tune_fine + bend_fine;
        if (v->fine + tune_fine + bend_fine)             /* residual below 1/16 semitone */
            m.inc += (uint32_t)((int32_t)(m.inc >> 12) * (v->fine + tune_fine + bend_fine));
        m.cutoff = ((lfo * p[P_LD_FLT]) >> 7) + ((m.envq15 * p[P_ED_FLT]) >> 7) + ((lfo2 * p[P_L2FLT]) >> 7) +
                   (((v->env2 >> 9) * p[P_E2FLT]) >> 7); /* (ENV 2 / LFO 2 DEST: as ENV 1's and LFO 1's; 0 when unset) */
        if (v->vel > 110 && uf != 1u)                   /* accent opens the filter with the env (an engine whose filter the multimode one replaces: below) */
            m.cutoff += (m.envq15 * 24) >> 7;
        m.shape = (64 << 8) + ((lfo * p[P_LD_SHP]) >> 7) + ((m.envq15 * p[P_ED_SHP]) >> 7) + ((lfo2 * p[P_L2SHP]) >> 7) +
                  (((v->env2 >> 9) * p[P_E2SHP]) >> 7);
        m.amask = abits;                                /* ARP TARGET (SPEC 2.3): 0, every unit on the root */
        if (abits)
            atg_voice(t, v, &m);
        if (mod.on)                                     /* the modulation matrix (mod.c) */
            mod_voice(t, v, &m, v->fine + tune_fine + bend_fine);
        if (uf) {                                       /* no filter of its own: the cutoff routes are the multimode filter's */
            int32_t uc = clamp(ucut0 + m.cutoff, 0, UF_TOP);   /* the effective cut: the dial plus every route */
            if (uf == 1u)
                m.cutoff = v->vel > 110 ? (m.envq15 * 24) >> 7 : 0;   /* the routes are the filter's, whatever CUT is; the accent's lift stays the engine's own (its tone control, as ever) */
            else
                uc = ucut0;                             /* the multimode filter (ANALOG, TRIO, NOISE): the routes stay the engine's own filter's (m.cutoff untouched, the accent's lift too); this one, in series after it, is CUT alone */
            if (uc < UF_TOP || p[P_FREEC] > 0) {        /* (the bypass: MODE LP, the effective cut at the open end) */
                uf_voice(e, t, v, spread_on > v->side ? spread_buf : out, n, &m, uc);
                nr++;
                continue;
            }
            v->uft = 0;                                 /* bypassed: from rest next time */
        }
        e->render(t, v, spread_on > v->side ? spread_buf : out, n, &m);   /* (SPREAD: the left ones) */
        nr++;
    }
    if (spread_on)
        spread_sum(out);                                /* out: every voice; spread_buf: the left ones */
    if (fade) {
        for (i = 0; i < 8u; i++)
            t->p[P_E0 + i] = pe_new[i];
        t->xf--;
    }
    return nr;
}
