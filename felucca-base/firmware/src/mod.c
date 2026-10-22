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
 *     late, no loop. (No feedback anywhere: every source a per-block destination reads is the value the block before left
 *     (LFO 1 / 2, ENV, ENV 2, DUCK), so a source on its own LFO's RATE / DEPTH or ENV 2 on its own times is heard a block
 *     later, never within the block that computes it)
 *   DUELL 0.2.7, per block as PAN: LEVEL (the part's), the INSERT's A B C and MIX, the EQ's LOW MID HIGH gains, DUCK DEPTH,
 *     ENV 2's ATK DEC SUS REL and its FLT / SHP depths, RES (the multimode filter's: the engines with no filter of their
 *     own). The bus values CHORUS RATE / DEPTH, DELAY FDBK, the delay modulation's DEPTH / RATE, REVERB SIZE / DAMP: an
 *     offset per part on a global value (mod_bus: in DUEL each part moves its own bus, in STEREO / LAYER the parts add).
 *     An engine slot the matrix cannot move (mod_e_why: unused, WAVE's TABLE, FM6's SLOT, read only at a note-on) does
 *     nothing (TABLE is skipped, the rest is never read in the block); the MATRIX page shows it faded with the reason
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
       MS_CYCLE,                                  /* cloud job B: the voice's CYCLE envelope (job F: cycle_value), 0..1; read through the second list (mod_cycle_voice, mod_f2_voice): mod_voice does not know it */
       MS_N };
enum { MD_OFF, MD_PITCH, MD_CUT, MD_SHP, MD_AMP, MD_PAN, MD_DIST, MD_CHO, MD_DLY, MD_REV, MD_RATE, MD_VIB, MD_E1,
       MD_DEPTH = MD_E1 + 8,
       MD_RATE2, MD_DEPTH2,                       /* DUELL: LFO 2's RATE (P_L2RATE, per block) and DEPTH (as DEPTH) */
       /* DUELL 0.2.7 (owner: "more destinations"), per block as PAN: the part's LEVEL, the INSERT's A B C (named by its TYPE)
        * and MIX, the EQ's three gains, the DUCK's DEPTH, ENV 2's ATK DEC SUS REL and its FLT / SHP depths, the multimode
        * filter's RES (the engines without a filter of their own) */
       MD_LEVEL, MD_INSA, MD_INSB, MD_INSC, MD_IMIX, MD_EQLO, MD_EQMID, MD_EQHI, MD_DUCK,
       MD_E2ATK, MD_E2DEC, MD_E2SUS, MD_E2REL, MD_E2FLT, MD_E2SHP, MD_RES,
       /* .. and the effect buses' values (global parameters: mod_bus below): CHORUS RATE DEPTH, DELAY FDBK, the delay
        * modulation's DEPTH RATE, REVERB SIZE DAMP */
       MD_BUS, MD_CRATE = MD_BUS, MD_CDEP, MD_DFB, MD_DMDEP, MD_DMRATE, MD_RSIZE, MD_RDAMP,
       /* DUELL 0.2.25 (owner, matrix round): the multimode filter's MODE (P_FREEC: LP 0 .. BP 64 .. HP 127), per block as RES (uf_run ramps its
        * output weights over each block: clean). Not a bus value, though it comes after them (append-only) */
       MD_FMODE,
       /* T0229 (owner, MATRIX_LIST_PREVIEW.md): the AMP ENV's ATTACK DECAY SUSTAIN RELEASE (P_ATK .. P_REL), per block as ENV 2's times: voice.c esync_coef reads
        * the block's values, the envelope itself stays continuous (only its slope or its sustain target moves) */
       MD_AATK, MD_ADEC, MD_ASUS, MD_AREL,
       /* cloud job B (ext.h EXT_MD_*): the TWIN FILTER's CUTOFF 2 (per note: mod_f2_voice), RES 2, DRIVE, BLEND (per block, as RES; the filter glides them), the SUB LAYER's LEVEL (per block, an
        * extension slot: the id table is a byte, wide ids fit) */
       MD_CUT2, MD_RES2, MD_DRIVE, MD_BLEND, MD_SUBLVL,
       MD_N };                                    /* (append-only: the stored values keep their meaning) */
#define MD_N_026 (MD_DEPTH2 + 1)                  /* 0.2.6's destinations: the morph's WILD roll picks among these only (morph.c) */
#define MG_N (MD_RDAMP + 1 - MD_BUS)              /* the bus values (MD_BUS .. MD_RDAMP) */
#define MD_IS_BUS(d) ((d) >= MD_BUS && (d) < MD_BUS + MG_N)
_Static_assert(NELEM(N_MSRC) == MS_N && NELEM(N_MDST) == MD_N, "N_MSRC / N_MDST == MS_* / MD_*");
_Static_assert(MD_LEVEL == 23 && MD_BUS == 39 && MG_N == 7 && MD_FMODE == 46 && MD_AATK == 47 && MD_N == 56 && MD_N < 128, "matrix DST: 0.2.6's 0..22 unchanged, appended after");
_Static_assert(MD_CUT2 == EXT_MD_CUT2 && MD_RES2 == EXT_MD_RES2 && MD_DRIVE == EXT_MD_DRIVE && MD_BLEND == EXT_MD_BLEND && MD_SUBLVL == EXT_MD_SUBLVL && MS_CYCLE == EXT_MS_CYCLE && MS_N == 16,
               "ext.h: the matrix numbers 51..55 and source 15");
_Static_assert(P_DEC == P_ATK + 1 && P_SUS == P_ATK + 2 && P_REL == P_ATK + 3, "the AMP ENV times in a row");
#define MSLOT_ID(k) (P_M1SRC + 3u * (k))                         /* slot k's SRC; DST + 1, AMT + 2 */
_Static_assert(P_M2SRC == P_M1SRC + 3 && P_M4AMT == P_M1SRC + 11 && P_M5SRC == P_M4AMT + 1 && P_M8AMT == MSLOT_ID(NMSLOT) - 1,
               "matrix slots: 3 ids each, 1..8 in a row");
#define MS_VOICE(s) (((s) >= MS_ENV && (s) <= MS_RAND) || (s) == MS_ENV2 || (s) == MS_CYCLE)   /* a value per voice */
#define MS_BIPOLAR(s) ((s) == MS_LFO || (s) == MS_KEY || (s) == MS_RAND || (s) == MS_SH || (s) == MS_SLEW || \
                       (s) == MS_LFO2)                           /* (ENV2 is 0..1, as ENV) */
/* the kind source s gains as (mod_gain): a UNI LFO (POL), 0..1, as ENV does */
#define MS_GAIN(p, s) (((s) == MS_LFO && (p)[P_LPOL]) || ((s) == MS_LFO2 && (p)[P_L2POL]) ? MS_ENV : (s))
#define MD_ENGINE(d) ((d) >= MD_E1 && (d) < MD_E1 + 8)           /* E1..E8: P_E0..P_E7 */

/* the track parameter of a per-block destination (not a bus value) */
static uint32_t mod_param(uint32_t d)
{
    static const uint8_t ID[MD_N] = {[MD_PAN] = P_PAN, [MD_DIST] = P_DIST, [MD_CHO] = P_CHOR, [MD_DLY] = P_DLY,
                                       [MD_REV] = P_REV, [MD_RATE] = P_LRATE, [MD_VIB] = P_LD_PIT, [MD_RATE2] = P_L2RATE,
                                       [MD_LEVEL] = P_TRIM,   /* 0.2.19: the matrix is the sound's: DST TRIM moves the PATCH TRIM, not the player's mixer level */
                                       [MD_INSA] = P_IA, [MD_INSB] = P_IB, [MD_INSC] = P_IC,
                                       [MD_IMIX] = P_IMIX, [MD_EQLO] = P_EQLG, [MD_EQMID] = P_EQMG, [MD_EQHI] = P_EQHG,
                                       [MD_DUCK] = P_DKDEP, [MD_E2ATK] = P_E2ATK, [MD_E2DEC] = P_E2DEC, [MD_E2SUS] = P_E2SUS,
                                       [MD_E2REL] = P_E2REL, [MD_E2FLT] = P_E2FLT, [MD_E2SHP] = P_E2SHP, [MD_RES] = P_FREEB,
                                       [MD_FMODE] = P_FREEC, [MD_AATK] = P_ATK, [MD_ADEC] = P_DEC, [MD_ASUS] = P_SUS, [MD_AREL] = P_REL,   /* (T0229: one table, no branches for the newest) */
                                       [MD_CUT2] = P_CUT2, [MD_RES2] = P_RES2, [MD_DRIVE] = P_DRV2, [MD_BLEND] = P_BLEND, [MD_SUBLVL] = XW_BASE + X_SUBLVL};   /* (cloud job B: CUTOFF 2 is per note, mod_begin leaves it to mod_f2_voice; the rest per block; wide ids fit the byte) */
    return MD_ENGINE(d) ? P_E0 + d - MD_E1 : ID[d];
}

/* Engine slot k (P_E0 + k) of engine e: why the matrix does not move it, 0 = it does. A slot the engine does not use ("-");
 * WAVE's TABLE (a change builds a new table, the notes silent meanwhile: not a modulation); FM6's SLOT (the main loop loads
 * the patch, the audio never reads it); a value the engine reads only as a note starts (AT NOTE: SAMPLE SET, SLICE SRC DIV START
 * REV until DUELL 0.2.9 retired both engines; none today). The MATRIX page shows such a route faded with the reason (<= 8
 * characters, a card's); it does nothing: mod_begin skips TABLE (the one an engine would act on in the block: wave_block),
 * the others have no reader between mod_begin and mod_end (the value written there is put back unread) */
static __attribute__((noinline)) const char *mod_e_why(uint32_t e, uint32_t k)
{
    const char *l = ENGINES[e % NENGINES]->edit[k & 7u].label;
    if (!l || !l[0] || l[0] == '-')
        return "UNUSED";
    if (e == ENGI_WAVE && !k)
        return "REBUILD";
    if (e == ENGI_FM6 && k == 7u)
        return "LOADS";
    return 0;
}

/* DUELL 0.2.25 (owner: the hide rule applies to modulation, its depths and its destinations; docs/MOD_INERT_TABLE.md): what the current WAVE of t's engine
 * leaves without any effect (the engine's own render code reads it, or not, for that wave; a value is never a reason).
 *   SHAPE, the shape input (m->shape: the SHP depth cards of AMP DEST, ENV 2 DEST, LFO 1 DEST, LFO 2 DEST, the matrix's SHP and E2SHP): ANALOG reads it only
 *     with WAVE PWM (the pulse width) and SYNC (the sync sweep), LOFI only with WAVE PLS (the width around DUTY), TRIO only with a set that has a pulse
 *     (PLS3 PPT P&S P+N SYNCP R+SP: the PW); PHASE (DCW), VOICE (BUZZ), WHEEL (brightness), PHYS (STRC, SYMP's BUZZ), NOISE (COLR), FM6 (feedback) and
 *     WAVE (POS) always do. 1 = no effect */
static int trio_has(const int16_t *p, uint32_t w)        /* TRIO's set p[P_E0] has an oscillator of wave w */
{
    const trio_set_t *s = &TRIO_SETS[(uint32_t)p[P_E0] & 15u];
    return s->w[0] == w || s->w[1] == w || s->w[2] == w;
}
static __attribute__((noinline)) int mod_shp_inert(const track_t *t)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    const int16_t *p = t->p;
    if (e == &ENG_ANALOG)
        return p[P_E0] != 4 && p[P_E0] != AW_SYNC;      /* (4: PWM) */
    if (e == &ENG_LOFI)
        return p[P_E1] != RW_PLS;
    if (e == &ENG_TRIO)
        return !trio_has(p, TW_PLS) && !trio_has(p, TW_PS);
    return 0;
}
/* engine slot k (P_E0 + k) with no effect for the current wave: TRIO's INT2 with no oscillator 2 (P+N NOIS RING RING3), INT3 with no oscillator 3 that
 * sounds or rings (NOIS SYNC SYNCP), DTN with neither (NOIS), PW with no pulse; LOFI's DUTY with WAVE TRI, SAW or NOIS (PLS takes its width, WRAM its table) */
static __attribute__((noinline)) int mod_e_inert(const track_t *t, uint32_t k)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    const int16_t *p = t->p;
    if (e == &ENG_TRIO) {
        const trio_set_t *s = &TRIO_SETS[(uint32_t)p[P_E0] & 15u];
        int o2 = s->w[1] != TW_OFF, o3 = s->w[2] != TW_OFF || (s->flags & TRIO_RING);
        return k == 1u ? !o2 : k == 2u ? !o3 : k == 3u ? !o2 && !o3 : k == 7u ? mod_shp_inert(t) : 0;
    }
    if (e == &ENG_LOFI)
        return k == 2u && p[P_E1] != RW_PLS && p[P_E1] != RW_WRAM;
    return 0;
}
/* REMOVED from the destination list (owner, 2026-10-18; the study: release_drafts/MATRIX_DESTINATIONS.md): the EQ's three gains (EQ HIGH clicks badly) and
 * the engine slots that are a list of choices, every change a hard switch: ANALOG WAVE, PHASE WAVE 1 / WAVE 2 / LINE, LOFI BITS / WAVE / ARP, TRIO WAVE /
 * MODE, PHYS MODEL, NOISE MODE, FM6 ALG (WHEEL's REG and ROTR stay: the engine ramps them). The ids stay (append-only) and a stored route to one plays as
 * before: the unit and the editor only no longer offer it (mod_dst_offered).
 * 1.0 (owner, 2026-10-21): the LFO master depths DEPTH (DEP1) and DEPTH2 (DEP2) are removed the same way: invisible in the list, the engine keeps them
 * and the ids stay (LFO 1 / LFO 2 LEVEL knobs come on the LFO pages after the release). The factory sounds no longer route to them */
static __attribute__((noinline)) int mod_dst_removed(uint32_t eng, int32_t d)
{
    const engine_t *e = ENGINES[eng % NENGINES];
    uint32_t k = (uint32_t)(d - MD_E1), m;
    if (d == MD_EQLO || d == MD_EQMID || d == MD_EQHI || d == MD_RSIZE)   /* (T0229, owner: REVERB SIZE out for now: SPRING bends its pitch while SIZE moves) */
        return 1;
    if (d == MD_DEPTH || d == MD_DEPTH2)                                  /* (1.0, owner: the LFO depths are invisible; the code stays in the mod_depth readers) */
        return 1;
    if (!MD_ENGINE(d))
        return 0;
    m = e == &ENG_ANALOG ? 0x01u : e == &ENG_PHASE ? 0x23u : e == &ENG_LOFI ? 0x43u : e == &ENG_TRIO ? 0x11u
      : e == &ENG_PHYS || e == &ENG_NOISE || e == &ENG_FM6 ? 0x01u : 0u;
    return (int)((m >> k) & 1u);
}

/* The bus values (DUELL 0.2.7): CHORUS RATE / DEPTH, the delay's FDBK and its modulation's DEPTH / RATE, REVERB SIZE / DAMP
 * are global parameters (song.g, one per instrument) while the matrix is a part's. Each part's routes make an OFFSET in the
 * value's own units (AMT 64 at a full source = the whole range, as on the per-block destinations), kept per part
 * (mod_goff, mod_begin). mod_bus writes stored value + the offsets of the parts a bus carries into song.g just before the
 * buses run (fx.c mix_block), clamped to the range, and mod_bus_end puts the stored values back after them: DUEL (SPLIT) has
 * a bus per part, so part 1's routes move bus A's value and part 2's bus B's, never the other's; STEREO and LAYER share one
 * set of buses, so both parts' offsets ADD. As with t->p, all of it runs inside the audio ISR: the UI, the editor and the
 * stores never see a modulated value. Not a bus value: the delay's TIME (a note value: a step moves the read point, a
 * click; the delay modulation's DEPTH is the smooth wobble) */
static const uint8_t MG_ID[MG_N] = {G_CRATE, G_CDEPTH, G_DFDBK, G_DMDEP, G_DMRATE, G_RSIZE, G_RDAMP};
static int16_t mod_goff[NLIVE][MG_N];    /* per part, its offsets (mod_begin) */
/* per part, in track_t: mgon (0 / 1) an offset is not 0, mdk (0 / 1) an active slot moves DUCK DEPTH (fx.c duck_pump / duck_note let it start) */
static int16_t mod_gkeep[MG_N];          /* the stored values mod_bus wrote over */
static uint8_t mod_gw;                   /* bit per bus value: written (mod_bus_end puts it back) */
static __attribute__((noinline)) void mod_bus(uint32_t mask)       /* the offsets of the parts in mask (bit per part) onto song.g */
{
    uint32_t g, p;
    for (g = 0; g < MG_N; g++) {
        int32_t o = 0;
        const param_desc_t *gd = &GP[MG_ID[g]];
        for (p = 0; p < NLIVE; p++)
            o += (int32_t)((mask >> p) & 1u) * mod_goff[p][g];
        if (!((mod_gw >> g) & 1u)) {
            if (!o)
                continue;
            mod_gkeep[g] = song.g[MG_ID[g]];
            mod_gw |= (uint8_t)(1u << g);
        }
        song.g[MG_ID[g]] = (int16_t)clamp(mod_gkeep[g] + o, gd->min, gd->max);
    }
}
static __attribute__((noinline)) void mod_bus_end(void)
{
    uint32_t g;
    for (g = 0; g < MG_N; g++)
        if ((mod_gw >> g) & 1u)
            song.g[MG_ID[g]] = mod_gkeep[g];
    mod_gw = 0;
}
/* any part's bus offsets are not 0 (the send stage: it reads both parts', after the parts) */
static uint32_t mod_gon_any(void)
{
    uint32_t p, a = 0;
    for (p = 0; p < NLIVE; p++)
        a |= trk[p].mgon;
    return a;
}
/* part t's bus offsets (off: MG_N of them, 0 = none) and its DUCK flag, from mod_begin */
static __attribute__((noinline)) void mod_part_put(track_t *t, const int32_t *off, uint32_t dk)
{
    uint32_t p = (uint32_t)(t - trk), g;
    if (p >= NLIVE)
        return;
    t->mdk = dk != 0;
    if (!off && !t->mgon)
        return;
    for (g = 0; g < MG_N; g++)
        mod_goff[p][g] = (int16_t)(off ? clamp(off[g], -255, 255) : 0);
    t->mgon = off != 0;
}


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
        return MODW(t).lfo2;
    case MS_ENV:
        return t->m_env;
    case MS_ENV2:
        return t->m_env2;
    case MS_CYCLE:                                      /* the latest note's CYCLE, last block (mod_cycle_voice) */
        return t->m_cycle;
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

/* MX_AMT (the +63 = 64 rule of the matrix amount) is in core.h: the direct depth cards use it too (voice.c, chain_f2.c) */

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

/* SLEW (DUELL 0.2.25, owner's matrix round; release_drafts/MATRIX_DESTINATIONS.md): each slot's source value as mod_begin reads it (a per-track source, or a
 * per-voice one's latest note for a per-block destination) moves at most MX_SLEW a block: a jump (an S&H step, a square or saw LFO's edge, an LFO restarted by a
 * note, a MIDI CC step, a new note's VEL / KEY / RAND) becomes a glide of at most 5 blocks (3.6 ms) for a unipolar full swing, 10 (7.3 ms) for a bipolar one,
 * instead of a step at one block boundary (a click on every destination, the filter's cutoff the worst). An LFO or an envelope that moves less than that a
 * block passes unchanged: a full-depth sine up to RATE's top (40 Hz), every synced rate but 32T above 200 BPM; an attack of 4 ms or longer. (The study's 6-block
 * setting rounded LFOs above 36 Hz.) A slot that was not active the block before takes its value at once
 * (nothing to glide from), and so does a slot whose SRC changed. Per part and slot, 80 bytes; flash code (not hot) */
#ifndef MX_SLEW
#define MX_SLEW 6554                              /* 32767 / 5, Q15 of a unipolar full swing a block: LFO RATE's top (40 Hz, a full sine moves 5976 a block) passes */
#endif
typedef struct {
    int32_t v[NMSLOT + 2];                        /* the slewed value of the block before; [NMSLOT], [NMSLOT + 1]: LFO 1's and LFO 2's value for their own depths (voice.c track_render) */
    uint8_t src[NMSLOT];                          /* .. of this SRC (another SRC in the slot: its value at once) */
    uint16_t ok;                                  /* bit per slot (and 8, 9 the two LFOs): v holds the block before's value */
} mx_sl_t;
/* (the matrix round's state in a section of its own, .bss.mxr: the compiler's global merge keeps the layout of the variables it had, so the hot RAM
 * code (tools/hot_fns.py) compiles as before) */
#define MXR_BSS __attribute__((section(".bss.mxr")))
static mx_sl_t mx_sl[NLIVE] MXR_BSS;
/* (join0233: the part's state is found from t, not through a global pointer: on two cores part 2's mod_begin runs on core 1 at the same time as part 1's on core 0) */
static __attribute__((noinline)) int32_t mx_slew(const track_t *t, uint32_t k, int32_t x)
{
    uint32_t pi = (uint32_t)(t - trk);
    mx_sl_t *c = pi < NLIVE ? &mx_sl[pi] : 0;
    int32_t v, d;
    if (!c)
        return x;
    if (!((c->ok >> k) & 1u)) {
        c->ok |= (uint16_t)(1u << k);
        c->v[k] = x;
        return x;
    }
    v = c->v[k];
    d = x - v;
    v += d > MX_SLEW ? MX_SLEW : d < -MX_SLEW ? -MX_SLEW : d;
    c->v[k] = v;
    return v;
}
/* per part (0 / 1): an active slot moves the part's PAN, PATCH LEVEL or a send this block, or the part's MORPH window is open (hunt1) (fx.c part_mix ramps them inside the block, mix_ramp).
 * A byte for each part, not a bit of one shared byte (join0233): the two parts' mod_begin may run on two cores, and a bit written by a read-modify-write
 * of a shared byte would lose the other core's write */
static uint8_t mod_mixmv[NLIVE] MXR_BSS;
#define MD_MIXED(d) ((d) == MD_PAN || (d) == MD_LEVEL || (d) == MD_CHO || (d) == MD_DLY || (d) == MD_REV)

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
            g = mulq15(g, mod_gain(MS_GAIN(p, s), mx_slew(t, k, mod_tsrc(t, s, lfo)), MX_AMT(p[MSLOT_ID(k) + 2u])));
    }
    return g;
}

/* DUELL 0.2.24 (owner): the matrix stays PACKED: a slot is used while its SRC is not OFF; when a SRC is set to OFF (the knob, the editor) or a sound loads with gaps, the used
 * slots move up in their order (a stable shift: mod_begin reads them in slot order, so the sum, the per-voice list and the AMP / DEPTH products come out the same), the slots
 * after them are cleared (SRC DST AMT 0); there is never an empty slot before a used one, and MATRIX 5-8 is in the page round only while slot 4 is used (ui.c page_visible).
 * A slot with a SRC but no DST or AMT yet is being set up: it stays where it is. 1 = something moved. Not hot code */
static int mod_compact(track_t *t)
{
    int16_t *p = t->p;
    uint32_t k, w = 0;
    int moved = 0;
    for (k = 0; k < NMSLOT; k++) {
        if (!p[MSLOT_ID(k)])
            continue;
        if (w != k) {
            p[MSLOT_ID(w)] = p[MSLOT_ID(k)];
            p[MSLOT_ID(w) + 1u] = p[MSLOT_ID(k) + 1u];
            p[MSLOT_ID(w) + 2u] = p[MSLOT_ID(k) + 2u];
            moved = 1;
        }
        w++;
    }
    for (; w < NMSLOT; w++) {
        if (p[MSLOT_ID(w) + 1u] | p[MSLOT_ID(w) + 2u])
            moved |= !p[MSLOT_ID(w)] ? 0 : 1;
        if (p[MSLOT_ID(w)] | p[MSLOT_ID(w) + 1u] | p[MSLOT_ID(w) + 2u])
            moved = 1;
        p[MSLOT_ID(w)] = p[MSLOT_ID(w) + 1u] = p[MSLOT_ID(w) + 2u] = 0;
    }
    return moved;
}
/* is id a slot's SRC (the knob / the editor just wrote it: a 0 compacts) */
static int mod_is_src(uint32_t id) { return id >= P_M1SRC && id <= P_M4AMT ? (id - P_M1SRC) % 3u == 0u : id >= P_M5SRC && id <= P_M8AMT ? (id - P_M5SRC) % 3u == 0u : 0; }

/* the descriptor whose range a per-block destination's id moves over: the engine's slot, a part's value, an extension slot or the TWIN FILTER's RES 2 by its TYPE (RES / FDBK) */
static __attribute__((noinline)) const param_desc_t *mod_pd(const track_t *t, uint32_t id)
{
    return id >= XW_BASE || id == P_RES2 ? track_desc(t, id) : id >= P_E0 ? &ENGINES[t->engine]->edit[id - P_E0] : &TP[id];
}

/* before a part's block (fx.c mix_part): the per-track offsets, the per-block destinations into t->p.
 * 0 = no slot active (then nothing changed) */
static __attribute__((noinline)) int mod_begin(track_t *t)
{
    const int16_t *p = t->p;
    mod_win_t *mod = &MODW(t);
    int32_t lfo, off[NMSLOT], goff[MG_N];
    uint32_t k, j, act = 0, dep = 0, dep2 = 0, dk = 0, gb = 0, mx = 0, pi = (uint32_t)(t - trk);
    mx_sl_t *mc = pi < NLIVE ? &mx_sl[pi] : 0;      /* (the slew's state of this part: 0 for a track that does not render) */
    uint32_t f2m = 0, cym = 0, cy = 0;                  /* the second list (cloud job B): slots to CUTOFF 2 from a per-voice source, CYCLE slots to a per-note destination, CYCLE read */
    mod->on = 0;
    mod->e2 = mod->l2 = 0;
    t->f2add = 0;
    for (k = 0; k < NMSLOT; k++) {                      /* the active slots (SRC, DST and AMT set), once a block */
        const int16_t *q = &p[MSLOT_ID(k)];
        if (q[0] && q[1] && q[2]) {
            if (mc && mc->src[k] != (uint8_t)q[0]) {   /* (the slew: another source in the slot starts from its own value) */
                mc->src[k] = (uint8_t)q[0];
                mc->ok &= (uint8_t)~(1u << k);
            }
            act |= 1u << k;
            dep |= (uint32_t)(q[1] == MD_DEPTH) << k;
            dep2 |= (uint32_t)(q[1] == MD_DEPTH2) << k;
            dk |= q[1] == MD_DUCK;
            mx |= MD_MIXED(q[1]);
            mod->e2 |= q[0] == MS_ENV2;
            mod->l2 |= q[0] == MS_LFO2;
            cy |= q[0] == MS_CYCLE;
        }
    }
    t->f2m = t->cym = 0;                                /* (the second list is made below, with the other slots; nothing active: it stays empty) */
    t->cy = (uint8_t)(cy != 0);
    if (mc) {
        mc->ok &= (uint16_t)(act | 0x300u);                  /* (the slew: a slot not active now starts from its value when it comes back; bits 8, 9: the LFOs, always fed) */
        mod_mixmv[pi] = (uint8_t)(mx != 0 || t->mwon);   /* (hunt1: an open MORPH window counts too: the fader moves PAN, the sends and PATCH LEVEL by whole steps, as a route does) */
    }
    if (!act) {
        if (t->mgon | t->mdk)
            mod_part_put(t, 0, 0);                      /* (no bus offset, no DUCK flag left from a block before) */
        return 0;
    }
    lfo = mulq15(t->lfo_val, t->lfo_fade);              /* (the LFO of the block before: track_render ticks it) */
    mod->lfo2 = mod->l2 ? mulq15(t->lfo2.val, t->lfo2.fade) : 0;   /* (LFO 2's of the block before, the same way) */
    mod->on = 1;
    mod->amp = 0;
    mod->nv = mod->nk = 0;
    mod->pit = mod->cut = mod->shp = 0;
    mod->gain = 32767;
    mod->ldep = dep ? mod_depth(t, lfo, dep) : 32767;    /* DEPTH first: the other slots see the LFO it leaves */
    mod->ldep2 = dep2 ? mod_depth(t, lfo, dep2) : 32767; /* DUELL: DEPTH2 too; both read either LFO unscaled (no loop: the */
    if (mod->ldep < 32767)                               /* LFOs a block before, RATE / RATE2 heard from the next tick) */
        lfo = mulq15(lfo, mod->ldep);
    if (mod->ldep2 < 32767)
        mod->lfo2 = mulq15(mod->lfo2, mod->ldep2);
    for (act &= ~(dep | dep2); act; act &= act - 1u) {   /* the other active slots, in slot order */
        uint32_t s, d;
        int32_t a, x;
        k = (uint32_t)__builtin_ctz(act);
        s = (uint32_t)p[MSLOT_ID(k)];
        d = (uint32_t)p[MSLOT_ID(k) + 1u];
        a = MX_AMT(p[MSLOT_ID(k) + 2u]);
        if (s >= MS_N || d >= MD_N)
            continue;
        if (d == MD_CUT2) {                             /* filter 2's cutoff, per note (cloud job B): a per-voice source is read by mod_f2_voice (the slot's bit), a per-track one is summed here, as mod_voice_add sums CUT */
            if (MS_VOICE(s))
                f2m |= 1u << k;
            else
                t->f2add += (mx_slew(t, k, mod_tsrc(t, s, lfo)) * a) >> 7;
            continue;
        }
        if (d <= MD_AMP && s == MS_CYCLE) {             /* CYCLE to a per-note destination: mod_cycle_voice, after mod_voice (which does not know the source) */
            cym |= 1u << k;
            continue;
        }
        if (d <= MD_AMP) {                              /* per voice */
            mod->amp |= d == MD_AMP;
            if (MS_VOICE(s)) {
                mod->vsrc[mod->nv] = (uint8_t)s;
                mod->vdst[mod->nv] = (uint8_t)d;
                mod->vamt[mod->nv++] = (int8_t)a;
            } else {                                    /* (a UNI LFO, 0..1, takes AMP as ENV does) */
                mod_voice_add(MS_GAIN(p, s), d, mx_slew(t, k, mod_tsrc(t, s, lfo)), a, &mod->pit, &mod->cut, &mod->shp, &mod->gain);
            }
            continue;
        }
        x = mx_slew(t, k, mod_tsrc(t, s, lfo));
        if (MD_IS_BUS(d)) {                              /* a bus value (0.2.7): this part's offset, mod_bus applies it */
            const param_desc_t *gd = &GP[MG_ID[d - MD_BUS]];
            if (!gb)
                for (j = 0; j < MG_N; j++)
                    goff[j] = 0;
            gb = 1;
            goff[d - MD_BUS] += (((x * a) >> (MS_BIPOLAR(MS_GAIN(p, s)) ? 7 : 6)) * (gd->max - gd->min)) >> 15;
            continue;
        }
        {                                               /* per block: AMT 64 = the whole range */
            uint32_t id = mod_param(d);
            const param_desc_t *pd;
            if (id == P_E0 && t->engine == ENGI_WAVE)
                continue;                               /* (WAVE's TABLE: a change would build a new table. The other slots mod_e_why
                                                         * fades, unused, FM6's SLOT, read at a note-on, have no reader in the block) */
            pd = mod_pd(t, id);
            for (j = 0; j < mod->nk && mod->kid[j] != id; j++)
                ;
            if (j == mod->nk) {
                mod->kid[j] = (uint8_t)id;
                mod->keep[j] = p[id];
                off[j] = 0;
                mod->nk++;
            }
            /* a unipolar source (ENV, VEL, MODW, UNI LFO ..) at AMT 64 moves the parameter by its whole range one way; a
             * bipolar one (LFO, S&H, KEY, RAND ..) swings +-half the range, so its full swing spans the whole range
             * around the base value (PAN: full left to full right at AMT 63 / -64; a negative AMT is the inverted phase) */
            off[j] += (((x * a) >> (MS_BIPOLAR(MS_GAIN(p, s)) ? 7 : 6)) * (pd->max - pd->min)) >> 15;
        }
    }
    for (j = 0; j < mod->nk; j++) {
        uint32_t id = mod->kid[j];
        const param_desc_t *pd = mod_pd(t, id);
        t->p[id] = (int16_t)clamp(mod->keep[j] + off[j], pd->min, pd->max);
    }
    t->f2m = (uint8_t)f2m;
    t->cym = (uint8_t)cym;
    if (gb | dk | t->mgon | t->mdk)
        mod_part_put(t, gb ? goff : 0, dk);
    return 1;
}

/* after the part's block (when mod.on): the stored values back */
static __attribute__((noinline)) void mod_end(track_t *t)
{
    mod_win_t *mod = &MODW(t);
    uint32_t j;
    for (j = 0; j < mod->nk; j++)
        t->p[mod->kid[j]] = mod->keep[j];
    mod->on = 0;
}

/* a voice's modulation (voice.c track_render, when the part's window mod is on), onto its vmod_t m as track_render made it:
 * pitch (the increment again, with the fine factor fine as there), cutoff and shape (clamped to what the
 * fixed routings can reach), the amplitude (and v->env_out, the next block's start). env: its envelope */
static __attribute__((noinline)) void mod_voice(track_t *t, voice_t *v, vmod_t *m, int32_t fine)
{
    const mod_win_t *mod = &MODW(t);
    int32_t pit = mod->pit, cut = mod->cut, shp = mod->shp, gain = mod->gain, env = m->envq15;
    uint32_t k;
    if (v == &t->v[t->m_vi]) {
        t->m_env = env;                                 /* ENV of the latest note, for per-block destinations */
        t->m_env2 = v->env2 >> 9;                       /* .. and its ENV 2 */
    }
    for (k = 0; k < mod->nv; k++) {
        uint32_t s = mod->vsrc[k];
        int32_t x = s == MS_ENV ? env : s == MS_ENV2 ? v->env2 >> 9 : s == MS_VEL ? v->mvel * 258 : s == MS_KEY ?
                    mod_key(v->note) : v->mrnd;
        mod_voice_add(s, mod->vdst[k], x, mod->vamt[k], &pit, &cut, &shp, &gain);
    }
    if (pit) {
        m->pitch16 = clamp(m->pitch16 + pit, 0, 2047);
        m->inc = pitch_inc(m->pitch16);
        if (fine)
            m->inc += (uint32_t)((int32_t)(m->inc >> 12) * fine);
    }
    m->cutoff = clamp(m->cutoff + cut, -150 * 256, 150 * 256);
    m->shape = clamp(m->shape + shp, -62 * 256, 190 * 256);
    if (mod->amp) {
        m->amp1 = mulq15(m->amp1, gain);
        v->env_out = m->amp1;
    }
}

/* THE SECOND LIST (cloud job B, EXTENSION_BLOCK_PLAN 3.4): what mod_voice and mod_voice_add (hot RAM, closed) do not know, in two cold functions that voice.c track_render and the chain call.
 * mod_begin took the slots whose destination is CUTOFF 2, or whose source is CYCLE with a per-note destination, out of the per-voice list (t->f2m, t->cym, t->f2add, t->cy); with the
 * second list empty they do nothing and mod_voice runs as it always did.
 *
 * mod_f2_voice: the matrix's sum for filter 2's cutoff for voice v, in 1/256 dial steps as mod_voice's cut is (job E adds it to filter 2's own cutoff sum): the per-track sources' sum
 * made in mod_begin, and each CUTOFF 2 slot of a per-voice source (ENV, ENV 2, VEL, KEY, RAND, CYCLE) by its amount, x * AMT >> 7 as mod_voice_add's CUT. ENV is the voice's envelope as
 * track_render has it (t->venv, set before the call: the engine's amp curve applied; 0 for an engine with its own envelopes, as mod_voice). Call it after mod_voice, in track_render's
 * loop of voice v; cycle_value must give the same value for v on every call in a block (it is read here and in mod_cycle_voice) */
static __attribute__((noinline)) int32_t mod_f2_voice(track_t *t, voice_t *v)
{
    const int16_t *p = t->p;
    int32_t sum = t->f2add;
    uint32_t m;
    for (m = t->f2m; m; m &= m - 1u) {
        uint32_t k = (uint32_t)__builtin_ctz(m), s = (uint32_t)p[MSLOT_ID(k)];
        int32_t x = s == MS_ENV ? t->venv : s == MS_ENV2 ? v->env2 >> 9 : s == MS_VEL ? v->mvel * 258 : s == MS_KEY ? mod_key(v->note) : s == MS_CYCLE ? cycle_value(t, v) : v->mrnd;
        sum += (x * MX_AMT(p[MSLOT_ID(k) + 2u])) >> 7;
    }
    return sum;
}
/* mod_cycle_voice: the CYCLE-sourced slots with a per-note destination (PITCH, CUT, SHP, AMP) applied to voice v's vmod_t m after mod_voice has made its own, with the same sums and the same
 * re-computations (the pitch increment again with the fine factor, cutoff and shape clamped to what the fixed routings reach, the amplitude with v->env_out), and the latest note's CYCLE kept
 * for the per-block destinations (t->m_cycle, as ENV 2's m_env2). Called when an active slot reads CYCLE (t->cy), whatever its destination */
static __attribute__((noinline)) void mod_cycle_voice(track_t *t, voice_t *v, vmod_t *m, int32_t fine)
{
    const int16_t *p = t->p;
    int32_t pit = 0, cut = 0, shp = 0, gain = 32767, x = cycle_value(t, v);
    uint32_t cm, amp = 0;
    if (v == &t->v[t->m_vi])
        t->m_cycle = x;
    for (cm = t->cym; cm; cm &= cm - 1u) {
        uint32_t k = (uint32_t)__builtin_ctz(cm), d = (uint32_t)p[MSLOT_ID(k) + 1u];
        amp |= d == MD_AMP;
        mod_voice_add(MS_CYCLE, d, x, MX_AMT(p[MSLOT_ID(k) + 2u]), &pit, &cut, &shp, &gain);
    }
    if (!t->cym)
        return;
    if (pit) {
        m->pitch16 = clamp(m->pitch16 + pit, 0, 2047);
        m->inc = pitch_inc(m->pitch16);
        if (fine)
            m->inc += (uint32_t)((int32_t)(m->inc >> 12) * fine);
    }
    m->cutoff = clamp(m->cutoff + cut, -150 * 256, 150 * 256);
    m->shape = clamp(m->shape + shp, -62 * 256, 190 * 256);
    if (amp) {
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

/* the name of destination value v for track t (UI, <= 5 characters: the MATRIX page's DEST card and rows): E1..E8 by the
 * engine's labels (of the engine the UI shows), "E5" for a parameter the engine does not use; the INSERT's A B C by its
 * TYPE's labels (DRIVE TONE LEVEL, BITS RATE LPF, RATE DEPTH FDBK; "INS A" while it is OFF) */
static __attribute__((noinline)) const char *mod_dst_name(const track_t *t, int32_t v)
{
    v = clamp(v, 0, MD_N - 1);
    if (MD_ENGINE(v) || (v >= MD_INSA && v <= MD_INSC)) {
        const char *l = track_desc(t, MD_ENGINE(v) ? P_E0 + (uint32_t)(v - MD_E1) : P_IA + (uint32_t)(v - MD_INSA))->label;
        if (l && l[0] && l[0] != '-')
            return l;
    }
    if (v == MD_BUS + 3 && dly_ev_ui())  /* 0.2.16 DELAY MODE: the delay's DEPTH is CHANCE in RANDOM / OCT UP */
        return "CHNC";
    if (v >= MD_CUT2 && v <= MD_SUBLVL) {  /* cloud job B (FILTER_PAGES_PROPOSAL 5.1): filter 2's, two lines as FILTER RES where the DEST card has the room; on COMB the names of the type (TUNE, FEEDBACK) */
        int comb = t->p[P_TWTYPE] == TT_COMB;
        return v == MD_CUT2 ? (comb ? "TWIN\nTUNE" : "TWIN\nCUTOFF") : v == MD_RES2 ? (comb ? "TWIN\nFDBK" : "TWIN\nRES") : v == MD_DRIVE ? "FILTER\nDRIVE" : v == MD_BLEND ? "FILTER\nBLEND" : "SUB\nLEVEL";
    }
    if (v == MD_LEVEL)                    /* 0.2.25 layout round (owner): TRIM is shown PATCH LEVEL everywhere; the code in 5 characters (N_MDST keeps TRIM: the factory sound tool's key) */
        return "PATCH";
    if (v == MD_RES)                      /* T0229 (owner): FRES is FILTER RES, in two lines on the DEST card (ui_draw.c draw_column), one line in the rows (N_MDST keeps FRES) */
        return "MULTI\nRES";   /* (1.0, owner 2026-10-21: the filters are MULTI and TWIN, not 1 and 2; the DEST card has MULTI over RES in its two small lines, the matrix row MULTI RES) */
    if (v >= MD_AATK) {                   /* T0229: the AMP ENV times, two lines as FILTER RES (N_MDST keeps AATK .. AREL) */
        static const char *const AT[4] = {"AMP\nATTACK", "AMP\nDECAY", "AMP\nSUSTAIN", "AMP\nRELEASE"};
        return AT[v - MD_AATK];
    }
    if (v == MD_CUT)
        return "MULTI\nCUTOFF";              /* (1.0, owner 2026-10-21: MULTI over CUTOFF on the DEST card; the matrix row cannot hold MULTI CUTOFF in 64 px and says MULTI CUT; N_MDST keeps CUT: the factory sound tool's key) */
    return N_MDST[v];
}

/* why destination v does nothing for track t now, 0 = it works (a card's reason, <= 8 characters). Only what a selection
 * makes impossible (owner rule): the engine slot (mod_e_why: UNUSED, REBUILD, LOADS, AT NOTE), PAN in DUEL (each part on
 * its own side), the INSERT's values while its TYPE is OFF, FRES on an engine with no multimode filter at all (DRUM; on
 * ANALOG, TRIO, NOISE, whose own RES is an E slot, it is the multimode filter's RES since 0.2.9: ufilt 2). Never a value at 0 */
static __attribute__((noinline)) const char *mod_dst_why(const track_t *t, int32_t v)
{
    v = clamp(v, 0, MD_N - 1);
    if (MD_ENGINE(v)) {
        const char *l = track_desc(t, P_E0 + (uint32_t)(v - MD_E1))->label;
        if (!l || !l[0] || l[0] == '-')
            return "UNUSED";
        return mod_e_why(eng_idx(t->eng_req), (uint32_t)(v - MD_E1)) ? mod_e_why(eng_idx(t->eng_req), (uint32_t)(v - MD_E1))
             : mod_e_inert(t, (uint32_t)(v - MD_E1)) ? "WAVE" : 0;   /* (0.2.25: a slot the current wave does not use) */
    }
    if ((v == MD_SHP || v == MD_E2SHP) && mod_shp_inert(t))   /* 0.2.25: the shape input, unused with this wave */
        return "WAVE";
    if (v == MD_PAN && duell_mode_req == DM_SPLIT)
        return "DUEL";
    if (v == MD_BUS + 4 && dly_mode_why(G_DMRATE))      /* 0.2.14 DELAY MODE: the delay modulation's RATE does nothing in RANDOM / OCT UP (the mode's name) */
        return dly_mode_why(G_DMRATE);
    if (v >= MD_INSA && v <= MD_IMIX && !t->p[P_ITYPE])
        return "INS OFF";
    if (v == MD_RES && !ENGINES[eng_idx(t->eng_req)]->ufilt)
        return "ENGINE";
    if (v >= MD_AATK && v < MD_CUT2 && ENGINES[eng_idx(t->eng_req)]->ownenv)   /* (T0229: FM6's operators have their own envelopes: the AMP ENV times do nothing) */
        return "OWN ENV";
    if (v >= MD_CUT2 && v <= MD_BLEND && !t->p[P_TWTYPE])   /* cloud job B: the TWIN FILTER's values are offered while its TYPE is not OFF (a stored route keeps playing, like the INSERT's A B C) */
        return "TWIN OFF";
    return 0;
}

/* the matrix's DEST knob and the editor offer destination v for t (0.2.25): not a removed one (mod_dst_removed), and none the current selection makes
 * do nothing (mod_dst_why: an unused or inert engine slot, SHAPE with a wave that ignores it, PAN in DUEL, the INSERT's values while it is OFF, the delay
 * modulation's RATE in RANDOM / OCT UP, RES with no multimode filter). OFF is always offered. T0229 (owner): FILTER RES is never dimmed or hidden by
 * the cutoff (the CUT OPEN reason is gone) */
static __attribute__((noinline)) int mod_dst_offered(const track_t *t, int32_t v)
{
    const char *w;
    if (v <= 0)
        return 1;
    if (v >= MD_N || mod_dst_removed(eng_idx(t->eng_req), v))
        return 0;
    w = mod_dst_why(t, v);
    return !w;
}

/* the MATRIX page's DEST line for destination v of track t: "DEST: WAVE POS", "DEST: INSERT DRIVE", "DEST: REVERB SIZE"
 * (b: 32 bytes), and in more (0: nothing to add) what the card's reason or a bus value means in words */
static __attribute__((noinline)) void mod_dst_line(const track_t *t, int32_t v, char *b, const char **more)
{
    /* every destination's name in full, one after the other (none for E1..E8 and the INSERT's A B C: named by their owner) */
    static const char FULL[] =
        "NONE\0""PITCH\0""MULTI FILTER CUTOFF\0""SHAPE (TIMBRE)\0""VOICE AMP (TREMOLO)\0""PAN\0""SATURATION\0""CHORUS SEND\0"
        "DELAY SEND\0""REVERB SEND\0""LFO 1 RATE\0""LFO 1 PITCH DEPTH\0""\0""\0""\0""\0""\0""\0""\0""\0""LFO 1 DEPTH\0"
        "LFO 2 RATE\0""LFO 2 DEPTH\0""PATCH LEVEL\0""\0""\0""\0""INSERT MIX\0""EQ LOW GAIN\0""EQ MID GAIN\0""EQ HIGH GAIN\0"
        "DUCK DEPTH\0""FILT ENV ATTACK\0""FILT ENV DECAY\0""FILT ENV SUSTAIN\0""FILT ENV RELEASE\0""FILT ENV CUTOFF\0"
        "FILT ENV SHAPE\0""MULTI FILTER RES\0""CHORUS RATE\0""CHORUS DEPTH\0""DELAY FEEDBACK\0""DELAY MOD DEPTH\0"
        "DELAY MOD RATE\0""REVERB SIZE\0""REVERB DAMP\0""MULTI FILTER MODE\0"
        "AMP ENV ATTACK\0""AMP ENV DECAY\0""AMP ENV SUSTAIN\0""AMP ENV RELEASE\0"
        "TWIN FILTER CUTOFF\0""TWIN FILTER RES\0""FILTER DRIVE\0""FILTER BLEND\0""SUB LEVEL\0";
    const char *f = FULL;
    const char *why;
    uint32_t n;
    int32_t k;
    v = clamp(v, 0, MD_N - 1);
    for (k = v; k; k--)
        f += str_len(f) + 1u;
    why = mod_dst_why(t, v);
    str_cpy(b, "DEST: ", 32);
    n = str_len(b);
    if (MD_ENGINE(v) || (v >= MD_INSA && v <= MD_INSC)) {   /* whose: the engine's (its name), the INSERT's */
        str_cpy(b + n, MD_ENGINE(v) ? ENGINES[eng_idx(t->eng_req)]->name : "INSERT", 32 - n);
        n = str_len(b);
        str_cpy(b + n, " ", 32 - n);
        if (MD_ENGINE(v) && why && why[0] == 'U') {     /* an unused slot: where it is ("WAVE EDIT 2 KNOB 3"), not "E7" */
            char where[14];
            str_cpy(where, "EDIT 1 KNOB 1", sizeof where);
            where[5] = (char)('1' + (v - MD_E1) / 4);
            where[12] = (char)('1' + (v - MD_E1) % 4);
            str_cpy(b + n + 1u, where, 32 - n - 1u);
        } else {                                        /* (the INSERT OFF: "INSERT A") */
            const char *nm = mod_dst_name(t, v);
            str_cpy(b + n + 1u, nm == N_MDST[v] && !MD_ENGINE(v) ? nm + 4 : nm, 32 - n - 1u);
        }
    } else {
        str_cpy(b + n, v == MD_BUS + 3 && dly_ev_ui() ? "DELAY EVENT CHANCE" : t->p[P_TWTYPE] == TT_COMB && v == MD_CUT2 ? "TWIN FILTER TUNE" : t->p[P_TWTYPE] == TT_COMB && v == MD_RES2 ? "TWIN FILTER FEEDBACK" : f, 32 - n);
    }
    *more = !why ? 0 : v == MD_BUS + 4 ? "NOT USED IN THIS DELAY MODE" : why[0] == 'U' ? "NOT USED BY THIS ENGINE" : why[0] == 'R' ? "A TABLE CHANGE IS A REBUILD"
          : why[0] == 'L' ? "SLOT LOADS A WHOLE PATCH" : why[0] == 'A' ? "READ ONLY AS A NOTE STARTS"
          : why[0] == 'D' ? "DUEL: EACH PART ON ITS SIDE" : why[0] == 'I' ? "THE INSERT TYPE IS OFF"
          : why[0] == 'W' ? "NOT USED WITH THIS WAVE" : why[0] == 'O' ? "THE OPERATORS HAVE THEIR OWN"
          : why[0] == 'T' ? "THE TWIN FILTER IS OFF"
          : "THIS ENGINE HAS ITS OWN FILTER";
    if (!why && MD_IS_BUS(v))                            /* a bus value: whose bus it moves */
        *more = duell_mode_req == DM_SPLIT ? (t == trk ? "MOVES BUS A ONLY (PART 1)" : "MOVES BUS B ONLY (PART 2)")
                                           : "SHARED BUS: BOTH PARTS ADD";
}
