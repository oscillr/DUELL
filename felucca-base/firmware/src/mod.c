/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Modulation matrix: per track eight slots (DUELL; Felucca had four: slots 5..8 are on ids that were DIGITAL's, core.h PL_*), each SRC -> DST by AMT (-64..63), added to the fixed
 * routings (ENV DEST, LFO DEST). Runs in the audio ISR (fx.c mix_part, voice.c track_render).
 *
 * Sources, Q15:
 *   LFO   the track LFO with its FADE (as LFO DEST), bipolar; with LFO 2 POL UNI 0..1 (1.2)
 *   ENV   the amp envelope of the voice (after the engine's own amp curve, as ENV DEST), 0..1
 *   VEL   the note-on velocity (before UNISON's level scaling), 0..1
 *   KEY   the note relative to C4, bipolar: +-64 semitones = +-1 (an octave = 0.19)
 *   RAND  a new random value per note-on (its own generator: the shared rng is not touched), bipolar
 *   MODW  MIDI CC1, AT channel aftertouch, EXPR CC11 of the track's channel (seq.c), 0..1; until a
 *         controller arrives (and after CC121 RESET ALL CONTROLLERS) MODW and AT are 0, EXPR 1
 *   S&H   (#132) a random value held for one cycle of the track LFO: the value LFO WAVE S&H steps through
 *         (voice.c lfo_rand, drawn as each cycle starts, whatever the WAVE), without the LFO's FADE, POL or
 *         DEPTH; bipolar, 0 until the LFO's first cycle ends. It follows RATE and SYNC (a step per note value)
 *   SLEW  (#132) S&H slewed: it glides linearly from the cycle before's value to this cycle's over the cycle
 *         (a smooth random line, one cycle behind S&H, continuous where S&H steps); bipolar
 *   Both are per track (one value for every voice) and cost no state but the one value kept per cycle.
 *   LFO2  (DUELL) the track's LFO 2 (voice.c lfo_tick: LFO 1's code, RATE2 WAVE2 PHS2 FADE2 SYNC2 TRIG2 POL2), as LFO;
 *         its own S&H draws (lfo2_rand), no S&H / SLEW source of its own. It runs only while an active slot reads it (mod.l2)
 *   MORPH (DUELL 4b) the part's MORPH fader (morph.c), 0..1 = fader 0..127, one value per part (in BOTH both faders
 *         move together); read from t->mf, which morph_begin sets before each block of the part. No feedback: the fader is no parameter (no destination reaches it), the morph never rolls a
 *         route from MORPH nor a route to it (morph.c morph_slot), and the morph's blend is written before the matrix
 *         runs (fx.c mix_block: morph_begin, then mix_part's mod_begin), which reads the fader only
 *   ENV2  (DUELL) the voice's ENV 2 (voice.c env2_tick: ATK2 DEC2 SUS2 REL2, ENV SYNC as ENV's; no engine curve), 0..1.
 *         It runs only while an active slot reads it (mod.e2), so it costs nothing and changes nothing unread
 * Destinations:
 *   per voice (vmod_t, like the fixed routings):
 *     PITCH  AMT 63 at a full source = +-11.8 semitones (as LFO DEST PIT)
 *     CUT    m->cutoff, SHP m->shape: AMT 63 = +-63 steps (as ENV DEST); the sums are clamped to what
 *            the fixed routings can reach, so the engines see no new range
 *     AMP    a gain <= 1: AMT > 0 the source opens it (AMT 64: silent at 0, full at 1), AMT < 0 closes it
 *            (bipolar sources as 0..1, as LFO DEST AMP); several AMP slots multiply
 *   per track, once per block: PAN, DIST CHO DLY REV (the sends), RATE (LFO rate), VIB (LFO DEST PIT, the
 *     vibrato depth), E1..E8 (the engine's parameters P_E0..P_E7, named by the engine): AMT 64 at a full
 *     source = the parameter's whole range (a bipolar source swings +-half of it around the base value, so its full swing
 *     is the whole range: a sine LFO at AMT 63 sweeps PAN from full left to full right; AMT < 0 inverts the phase); the sum is clamped to its range
 *   DEPTH (#132, per track): the depth of the track LFO, a gain <= 1 on it as AMP is on the voice (AMT > 0 the source
 *     opens it, AMT 64: none at 0, full at 1; AMT < 0 closes it; several DEPTH slots multiply): it scales the LFO
 *     wherever it goes, the four LFO DEST routings and the matrix's LFO source (not S&H / SLEW). With RATE (the LFO's
 *     rate) the matrix modulates the modulator: MODW -> DEPTH is a mod-wheel LFO, ENV -> DEPTH an LFO that
 *     follows the note's envelope. A DEPTH slot whose source is the LFO takes it unscaled
 *   RATE2, DEPTH2 (DUELL): LFO 2's rate (P_L2RATE, per block as RATE) and depth (as DEPTH, on the LFO2 source). DEPTH and
 *     DEPTH2 read both LFOs unscaled, RATE / RATE2 move the rate from the next tick: LFO to LFO in a fixed order, a block
 *     late, no loop
 * A per-voice source on a per-block destination takes the latest note-on's value (VEL, KEY, RAND), ENV the
 * envelope of the latest note-on's voice (one block late, 0 when it has ended).
 *
 * Per-block destinations are modulated in place: mod_begin writes the modulated values into t->p for the
 * part's block (its render and its mix in fx.c mix_part) and mod_end puts the stored values back. Both run
 * in the audio ISR, so nothing outside it ever sees a modulated value (the UI, the editor, projects and
 * presets read and write t->p from the main loop, which the ISR preempts; TIMER5 only fills the MIDI rings),
 * and the engines read t->p as before. An engine reads its parameters in note_on (events_block) unmodulated.
 * Nothing active (every slot with SRC, DST or AMT at 0): no work, and the sound is bit-identical. */
enum { MS_OFF, MS_LFO, MS_ENV, MS_VEL, MS_KEY, MS_RAND, MS_MODW, MS_AT, MS_EXPR, MS_SH, MS_SLEW,
       MS_ENV2,                                   /* DUELL: ENV 2 of the voice (voice.c env2_tick), 0..1 */
       MS_LFO2,                                   /* DUELL: LFO 2 of the track (voice.c lfo_tick), as MS_LFO */
       MS_MORPH,                                  /* DUELL 4b: the part's MORPH fader (t->mf), 0..1 */
       MS_DUCK,                                   /* DUELL (SPEC 5c.4): how far the part is ducked now, 0 (full level) .. DEPTH (duck_st) */
       MS_N };
enum { MD_OFF, MD_PITCH, MD_CUT, MD_SHP, MD_AMP, MD_PAN, MD_DIST, MD_CHO, MD_DLY, MD_REV, MD_RATE, MD_VIB, MD_E1,
       MD_DEPTH = MD_E1 + 8,
       MD_RATE2, MD_DEPTH2,                       /* DUELL: LFO 2's RATE (P_L2RATE, per block) and DEPTH (as DEPTH) */
       MD_N };                                    /* (append-only: the stored values keep their meaning) */
_Static_assert(NELEM(N_MSRC) == MS_N && NELEM(N_MDST) == MD_N, "N_MSRC / N_MDST == MS_* / MD_*");
#define NMSLOT 8u
#define MSLOT_ID(k) (P_M1SRC + 3u * (k))                         /* slot k's SRC; DST + 1, AMT + 2 */
_Static_assert(P_M2SRC == P_M1SRC + 3 && P_M4AMT == P_M1SRC + 11 && P_M5SRC == P_M4AMT + 1 && P_M8AMT == MSLOT_ID(NMSLOT) - 1,
               "matrix slots: 3 ids each, 1..8 in a row");
#define MS_VOICE(s) (((s) >= MS_ENV && (s) <= MS_RAND) || (s) == MS_ENV2)   /* a value per voice */
#define MS_BIPOLAR(s) ((s) == MS_LFO || (s) == MS_KEY || (s) == MS_RAND || (s) == MS_SH || (s) == MS_SLEW || \
                       (s) == MS_LFO2)                           /* (ENV2 is 0..1, as ENV) */
/* the kind source s gains as (mod_gain): a UNI LFO (POL), 0..1, as ENV does */
#define MS_GAIN(p, s) (((s) == MS_LFO && (p)[P_LPOL]) || ((s) == MS_LFO2 && (p)[P_L2POL]) ? MS_ENV : (s))
#define MD_ENGINE(d) ((d) >= MD_E1 && (d) < MD_E1 + 8)           /* E1..E8: P_E0..P_E7 */

/* the track parameter of a per-block destination */
static uint32_t mod_param(uint32_t d)
{
    static const uint8_t ID[MD_E1] = {[MD_PAN] = P_PAN, [MD_DIST] = P_DIST, [MD_CHO] = P_CHOR, [MD_DLY] = P_DLY,
                                      [MD_REV] = P_REV, [MD_RATE] = P_LRATE, [MD_VIB] = P_LD_PIT};
    return d == MD_RATE2 ? P_L2RATE : MD_ENGINE(d) ? P_E0 + d - MD_E1 : ID[d];
}

static struct {                  /* the part in mix_part (one at a time) */
    uint8_t on;                  /* a slot is active: track_render calls mod_voice */
    uint8_t e2;                  /* DUELL: an active slot reads ENV 2: track_render ticks it (voice.c env2_tick) */
    uint8_t l2;                  /* DUELL: an active slot reads LFO 2: track_render ticks it (voice.c lfo_tick) */
    int32_t lfo2;                /* DUELL: LFO 2 as the slots read it this block (FADE, DEPTH2 applied; Q15) */
    int32_t ldep2;               /* DUELL: DEPTH2, LFO 2's gain (Q15, 32767 = none) */
    uint8_t amp;                 /* an AMP slot: the voice gain applies */
    uint8_t nv, nk;              /* voice terms, per-block parameters held */
    int32_t pit, cut, shp, gain; /* per voice, from the per-track sources (gain Q15) */
    int32_t ldep;                /* DEPTH: the track LFO's gain (Q15; 32767 = none, track_render leaves it alone) */
    uint8_t vsrc[NMSLOT], vdst[NMSLOT];
    int8_t vamt[NMSLOT];
    uint8_t kid[NMSLOT];         /* per-block: the parameter, its stored value */
    int16_t keep[NMSLOT];
} mod;

/* DUELL DUCK (fx.c duck_*): the part's sidechain envelope; dn = how far the gain is down, Q16 (0 = full level, 65536 = silent), ph 0 idle (dn 0), 1 going down, 2 held,
 * 3 coming back, 4 following the PART key (SRC PART); cnt = the hold's samples left; SRC PART: key = this part's level
 * follower (Q: x of mix_part), tg = the reduction it asks of the part now (Q16), on = the other part's key is over THR;
 * SRC PUMP: pos = samples since the last pump, run = it has fired (0: fires at the next tick) */
typedef struct { int32_t dn, key, tg; uint32_t cnt, pos; uint8_t ph, on, run; } duck_t;
static duck_t duck_st[NTRK];
static uint32_t mod_seed = 0x2545F491u;
static int16_t mod_rand(void)    /* RAND of a note-on: xorshift32, bipolar */
{
    mod_seed ^= mod_seed << 13;
    mod_seed ^= mod_seed >> 17;
    mod_seed ^= mod_seed << 5;
    return (int16_t)(mod_seed >> 16);
}

static int32_t mod_key(uint32_t note) { return clamp(((int32_t)note - 60) * 512, -32767, 32767); }

/* S&H: the value of the LFO's cycle (voice.c lfo_rand; 0 before its first one ends), bipolar */
static int32_t mod_sh(const track_t *t) { return t->lfo_rnd ? (int32_t)(t->lfo_rnd >> 16) - 32768 : 0; }

/* source s of the track: the per-track ones, the per-voice ones of the latest note-on */
static int32_t mod_tsrc(const track_t *t, uint32_t s, int32_t lfo)
{
    switch (s) {
    case MS_SH:
        return mod_sh(t);
    case MS_SLEW:                                       /* the cycle before's value .. this one's, by the phase */
        return t->lfo_prev + (((mod_sh(t) - t->lfo_prev) * (int32_t)(t->lfo_ph >> 17)) >> 15);
    case MS_LFO:
        return lfo;
    case MS_LFO2:
        return mod.lfo2;
    case MS_ENV:
        return t->m_env;
    case MS_ENV2:
        return t->m_env2;
    case MS_VEL:
        return t->m_vel * 258;
    case MS_KEY:
        return mod_key(t->m_key);
    case MS_RAND:
        return t->m_rnd;
    case MS_MODW:
        return t->mw * 258;
    case MS_AT:
        return t->at * 258;
    case MS_DUCK:                                       /* (the ducked amount at the end of the last block) */
        return duck_st[(uint32_t)(t - trk) % NTRK].dn >> 1;
    default:                                            /* EXPR; (4b) MORPH: t->mf, the fader morph_begin set for this block */
        return (s == MS_MORPH ? t->mf : 127 - t->ex_off) * 258;
    }
}

/* AMP: the gain (Q15, 0..32767) of source x (bipolar: as 0..1) at amount a */
static int32_t mod_gain(uint32_t s, int32_t x, int32_t a)
{
    int32_t u = MS_BIPOLAR(s) ? (x + 32768) >> 1 : x;
    return clamp(32767 - (a > 0 ? ((32767 - u) * a) >> 6 : (u * -a) >> 6), 0, 32767);
}

/* a per-voice destination d by source value x, amount a */
static void mod_voice_add(uint32_t s, uint32_t d, int32_t x, int32_t a, int32_t *pit, int32_t *cut, int32_t *shp,
                          int32_t *gain)
{
    if (d == MD_PITCH)
        *pit += (x * a * 3) >> 15;
    else if (d == MD_CUT)
        *cut += (x * a) >> 7;
    else if (d == MD_SHP)
        *shp += (x * a) >> 7;
    else
        *gain = mulq15(*gain, mod_gain(s, x, a));
}

/* DEPTH: the track LFO's gain (Q15) from the DEPTH slots, 32767 = none (lfo: the LFO unscaled; a per-voice source
 * takes the latest note's value, as on the other per-block destinations). Its own function: mod_begin's loop stays
 * what it was */
static __attribute__((noinline)) int32_t mod_depth(const track_t *t, int32_t lfo, uint32_t dep)   /* dep: the DEPTH slots */
{
    const int16_t *p = t->p;
    int32_t g = 32767;
    for (; dep; dep &= dep - 1u) {                      /* (in slot order) */
        uint32_t k = (uint32_t)__builtin_ctz(dep), s = (uint32_t)p[MSLOT_ID(k)];
        if (s < MS_N)
            g = mulq15(g, mod_gain(MS_GAIN(p, s), mod_tsrc(t, s, lfo), p[MSLOT_ID(k) + 2u]));
    }
    return g;
}

/* before a part's block (fx.c mix_part): the per-track offsets, the per-block destinations into t->p.
 * 0 = no slot active (then nothing changed) */
static __attribute__((noinline)) int mod_begin(track_t *t)
{
    const int16_t *p = t->p;
    int32_t lfo, off[NMSLOT];
    uint32_t k, j, act = 0, dep = 0, dep2 = 0;
    mod.on = 0;
    mod.e2 = mod.l2 = 0;
    for (k = 0; k < NMSLOT; k++) {                      /* the active slots (SRC, DST and AMT set), once a block */
        const int16_t *q = &p[MSLOT_ID(k)];
        if (q[0] && q[1] && q[2]) {
            act |= 1u << k;
            dep |= (uint32_t)(q[1] == MD_DEPTH) << k;
            dep2 |= (uint32_t)(q[1] == MD_DEPTH2) << k;
            mod.e2 |= q[0] == MS_ENV2;
            mod.l2 |= q[0] == MS_LFO2;
        }
    }
    if (!act)
        return 0;
    lfo = mulq15(t->lfo_val, t->lfo_fade);              /* (the LFO of the block before: track_render ticks it) */
    mod.lfo2 = mod.l2 ? mulq15(t->lfo2.val, t->lfo2.fade) : 0;   /* (LFO 2's of the block before, the same way) */
    mod.on = 1;
    mod.amp = 0;
    mod.nv = mod.nk = 0;
    mod.pit = mod.cut = mod.shp = 0;
    mod.gain = 32767;
    mod.ldep = dep ? mod_depth(t, lfo, dep) : 32767;    /* DEPTH first: the other slots see the LFO it leaves */
    mod.ldep2 = dep2 ? mod_depth(t, lfo, dep2) : 32767; /* DUELL: DEPTH2 too; both read either LFO unscaled (no loop: the */
    if (mod.ldep < 32767)                               /* LFOs a block before, RATE / RATE2 heard from the next tick) */
        lfo = mulq15(lfo, mod.ldep);
    if (mod.ldep2 < 32767)
        mod.lfo2 = mulq15(mod.lfo2, mod.ldep2);
    for (act &= ~(dep | dep2); act; act &= act - 1u) {   /* the other active slots, in slot order */
        uint32_t s, d;
        int32_t a, x;
        k = (uint32_t)__builtin_ctz(act);
        s = (uint32_t)p[MSLOT_ID(k)];
        d = (uint32_t)p[MSLOT_ID(k) + 1u];
        a = p[MSLOT_ID(k) + 2u];
        if (s >= MS_N || d >= MD_N)
            continue;
        if (d <= MD_AMP) {                              /* per voice */
            mod.amp |= d == MD_AMP;
            if (MS_VOICE(s)) {
                mod.vsrc[mod.nv] = (uint8_t)s;
                mod.vdst[mod.nv] = (uint8_t)d;
                mod.vamt[mod.nv++] = (int8_t)a;
            } else {                                    /* (a UNI LFO, 0..1, takes AMP as ENV does) */
                mod_voice_add(MS_GAIN(p, s), d, mod_tsrc(t, s, lfo), a, &mod.pit, &mod.cut, &mod.shp, &mod.gain);
            }
            continue;
        }
        {                                               /* per block: AMT 64 = the whole range */
            uint32_t id = mod_param(d);
            const param_desc_t *pd = id >= P_E0 ? &ENGINES[t->engine]->edit[id - P_E0] : &TP[id];
            x = mod_tsrc(t, s, lfo);
            for (j = 0; j < mod.nk && mod.kid[j] != id; j++)
                ;
            if (j == mod.nk) {
                mod.kid[j] = (uint8_t)id;
                mod.keep[j] = p[id];
                off[j] = 0;
                mod.nk++;
            }
            /* a unipolar source (ENV, VEL, MODW, UNI LFO ..) at AMT 64 moves the parameter by its whole range one way; a
             * bipolar one (LFO, S&H, KEY, RAND ..) swings +-half the range, so its full swing spans the whole range
             * around the base value (PAN: full left to full right at AMT 63 / -64; a negative AMT is the inverted phase) */
            off[j] += (((x * a) >> (MS_BIPOLAR(MS_GAIN(p, s)) ? 7 : 6)) * (pd->max - pd->min)) >> 15;
        }
    }
    for (j = 0; j < mod.nk; j++) {
        uint32_t id = mod.kid[j];
        const param_desc_t *pd = id >= P_E0 ? &ENGINES[t->engine]->edit[id - P_E0] : &TP[id];
        t->p[id] = (int16_t)clamp(mod.keep[j] + off[j], pd->min, pd->max);
    }
    return 1;
}

/* after the part's block (when mod.on): the stored values back */
static __attribute__((noinline)) void mod_end(track_t *t)
{
    uint32_t j;
    for (j = 0; j < mod.nk; j++)
        t->p[mod.kid[j]] = mod.keep[j];
    mod.on = 0;
}

/* a voice's modulation (voice.c track_render, when mod.on), onto its vmod_t m as track_render made it:
 * pitch (the increment again, with the fine factor fine as there), cutoff and shape (clamped to what the
 * fixed routings can reach), the amplitude (and v->env_out, the next block's start). env: its envelope */
static __attribute__((noinline)) void mod_voice(track_t *t, voice_t *v, vmod_t *m, int32_t fine)
{
    int32_t pit = mod.pit, cut = mod.cut, shp = mod.shp, gain = mod.gain, env = m->envq15;
    uint32_t k;
    if (v == &t->v[t->m_vi]) {
        t->m_env = env;                                 /* ENV of the latest note, for per-block destinations */
        t->m_env2 = v->env2 >> 9;                       /* .. and its ENV 2 */
    }
    for (k = 0; k < mod.nv; k++) {
        uint32_t s = mod.vsrc[k];
        int32_t x = s == MS_ENV ? env : s == MS_ENV2 ? v->env2 >> 9 : s == MS_VEL ? v->mvel * 258 : s == MS_KEY ?
                    mod_key(v->note) : v->mrnd;
        mod_voice_add(s, mod.vdst[k], x, mod.vamt[k], &pit, &cut, &shp, &gain);
    }
    if (pit) {
        m->pitch16 = clamp(m->pitch16 + pit, 0, 2047);
        m->inc = pitch_inc(m->pitch16);
        if (fine)
            m->inc += (uint32_t)((int32_t)(m->inc >> 12) * fine);
    }
    m->cutoff = clamp(m->cutoff + cut, -150 * 256, 150 * 256);
    m->shape = clamp(m->shape + shp, -62 * 256, 190 * 256);
    if (mod.amp) {
        m->amp1 = mulq15(m->amp1, gain);
        v->env_out = m->amp1;
    }
}

/* a note-on of track t (voice.c trk_note_on): the per-voice sources of the latest note */
static void mod_note(track_t *t, uint32_t note, uint32_t vel)
{
    t->m_vel = (uint8_t)vel;
    t->m_key = (uint8_t)note;
    t->m_rnd = mod_rand();
}

/* a MIDI controller for track t (seq.c events_block): CC1 / CC11 / channel aftertouch; CC121 resets them */
static __attribute__((noinline)) void mod_midi(track_t *t, uint32_t st, uint32_t d1, uint32_t d2)
{
    if (st == 0xD0u) {
        t->at = (uint8_t)d1;
    } else if (st == 0xB0u) {
        if (d1 == 1u)
            t->mw = (uint8_t)d2;
        else if (d1 == 11u)
            t->ex_off = (uint8_t)(127u - d2);
        else if (d1 == 121u)
            t->mw = t->at = t->ex_off = 0;
    }
}

/* the name of destination value v for track t (UI, <= 5 characters): E1..E8 by the engine's labels (of the
 * engine the UI shows), "E5" for a parameter the engine does not use */
static const char *mod_dst_name(const track_t *t, int32_t v)
{
    v = clamp(v, 0, MD_N - 1);
    if (MD_ENGINE(v)) {
        const char *l = track_desc(t, P_E0 + (uint32_t)(v - MD_E1))->label;
        if (l && l[0] && l[0] != '-')
            return l;
    }
    return N_MDST[v];
}
