/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.3 MACROS (owner 2026-11-12; DUELL/working docs/design studies/macros-and-device-presets_2026-11-12.md sections 6 to 9). Included at the end of mod.c.
 *
 * EIGHT macros: MORPH (the MORPH fader, morph.c), TIMBRE, ENVELOPE, FILTER, CHARACTER, MOTION (per part, here) and SPACE, KILL (global: PERFORM's perf_space and perf_filt,
 * perform.c, as they were). The five per-part ones work exactly like SPACE and KILL: a transient value -100 .. +100 per part, 0 = the patch exactly as saved, NOTHING stored
 * in a patch, a multi or the project. Turning one moves its targets away from their saved values: the LEAD moves its whole range over a half-turn (0 to +100 or to -100), each
 * partner by its share of that (a negative share: the other way). At a control's end it stops (mod_begin's clamp), the others keep moving. The offsets go into the part's
 * block only, through mod.c mod_begin's per-block window (kid / keep / off), and mod_end puts the stored values back: the stored patch never changes.
 *
 * What each macro moves is ONE set for the whole device, the const tables below (owner: "every patch is going to be adjusted by the same macros"). NOT BUILT in this round
 * (owner's design, section 6: "set in the MENU"): editing these targets and shares in the MENU. The tables are the defaults the owner approved (sections 7.2 and 8, "the
 * shares are tuned by ear on the unit before release"); a MENU editor would replace MAC_TAB / mac_timbre with stored copies.
 *
 * The TIMBRE input (MAC_SHP below) is the engine's shape input, m->shape, the matrix's TIMBRE destination (MD_SHP): it has no stored value, so its offset goes into the
 * per-voice shape sum (mod->shp), whose whole range is taken as 128 steps (the stored 0..127 it moves around). List controls (WAVE, MODEL, ALGO, BARS, MODE) are never moved. */
enum { MAC_MORPH, MAC_TIMBRE, MAC_ENVELOPE, MAC_FILTER, MAC_CHARACTER, MAC_MOTION, MAC_SPACE, MAC_KILL, MAC_N,
       MAC_DEPTH = MAC_N };                       /* (PERFORM's own K4 DEPTH: a pick of PERFORM's knobs only, not a macro) */
#define MAC_NPART 5u                              /* the per-part transient values: TIMBRE .. MOTION (mac_v[part][m - MAC_TIMBRE]) */
static const char *const MAC_NAME[MAC_N + 1u] = {"MORPH", "TIMBRE", "ENV MACRO", "FILTER MACRO", "CHARACTER", "MOTION", "SPACE", "KILL", "DEPTH"};   /* (owner 2026-11-13: ENV MACRO and FILTER MACRO, was ENVELOPE and FILTER) */
static volatile int8_t mac_v[NLIVE][MAC_NPART];   /* -100 .. +100, 0 = as saved (the main loop and MIDI's queue write them: ui_input.c morph_poll) */

#define MAC_SHP 0xFFu                             /* a target: the TIMBRE input (no stored value) */
enum { MC_ALWAYS, MC_LINK_OFF, MC_SYNC1_OFF, MC_SYNC2_OFF, MC_DENSITY, MC_AN_PLAIN };   /* when a target moves (MC_AN_PLAIN: ANALOG's WAVE is not PWM or SYNC) */
typedef struct { uint8_t id; int8_t share; uint8_t when; } mac_tgt_t;   /* share: % of the lead's move (the lead 100; negative: the opposite way) */
/* ENVELOPE, FILTER, CHARACTER, MOTION (section 8, with the owner's changes written there) */
static const mac_tgt_t MAC_ENV_T[] = {             /* right = longer and softer */
    {P_REL, 100, MC_ALWAYS}, {P_ATK, 60, MC_ALWAYS}, {P_DEC, 40, MC_ALWAYS}, {P_E2ATK, 60, MC_ALWAYS}, {P_E2REL, 100, MC_ALWAYS}, {0, 0, 0}};
static const mac_tgt_t MAC_FILT_T[] = {            /* right = more open. TWIN CUTOFF: only with LINK off (LINK on: it is an OFFSET from MULTI CUTOFF and follows it already, so the gap stays as saved: section 8's "Why") */
    {P_FREEA, 100, MC_ALWAYS}, {P_CUT2, 100, MC_LINK_OFF}, {P_E2FLT, -30, MC_ALWAYS}, {P_KTRK, 30, MC_ALWAYS}, {XW_BASE + X_KTRK2, 30, MC_ALWAYS},
    {P_FREEB, -15, MC_ALWAYS}, {0, 0, 0}};
static const mac_tgt_t MAC_CHAR_T[] = {            /* right = more aggressive (the owner's Virus idea) */
    {P_DIST, 100, MC_ALWAYS}, {P_DRV2, 60, MC_ALWAYS}, {P_FREEB, 20, MC_ALWAYS}, {P_RES2, 20, MC_ALWAYS}, {P_FREEA, -10, MC_ALWAYS}, {0, 0, 0}};
static const mac_tgt_t MAC_MOT_T[] = {             /* right = more movement; a RATE only on an LFO whose SYNC is off (a synced LFO stays on the beat) */
    {XW_BASE + X_LFO1LVL, 100, MC_ALWAYS}, {XW_BASE + X_LFO2LVL, 60, MC_ALWAYS}, {P_LRATE, 20, MC_SYNC1_OFF}, {P_L2RATE, 20, MC_SYNC2_OFF}, {0, 0, 0}};
/* TIMBRE per engine (section 7.2, approved): right = brighter and richer */
#define ME(k) (P_E0 + (k))
/* ANALOG (owner 2026-11-15: the TIMBRE input alone was barely audible on SAW, SQR, TRI, SIN and SUB): PWM and SYNC as before (the TIMBRE input; DRIVE, DETUNE);
 * the other waves bring OSC 2 in with MIX (on SUB: the sub's level), DETUNE with it, and DRIVE */
static const mac_tgt_t MAC_TIM_ANALOG[] = {{MAC_SHP, 100, MC_ALWAYS}, {ME(2), 100, MC_AN_PLAIN}, {ME(6), 40, MC_ALWAYS}, {ME(1), 20, MC_ALWAYS}, {0, 0, 0}};
static const mac_tgt_t MAC_TIM_PHASE[] = {{ME(2), 100, MC_ALWAYS}, {ME(3), -50, MC_ALWAYS}, {0, 0, 0}};                                /* WAVE SHAPING (DCW); SHAPING ENV opposite */
static const mac_tgt_t MAC_TIM_LOFI[] = {{ME(7), 100, MC_ALWAYS}, {MAC_SHP, 50, MC_ALWAYS}, {ME(3), 20, MC_ALWAYS}, {0, 0, 0}};         /* TONE; PULSE WIDTH (the TIMBRE input), CRUSH */
static const mac_tgt_t MAC_TIM_VOICE[] = {{MAC_SHP, 100, MC_ALWAYS}, {ME(5), -50, MC_ALWAYS}, {ME(6), 30, MC_ALWAYS}, {0, 0, 0}};       /* BUZZ (the TIMBRE input); BREATH opposite, RESONANCE (Q) */
static const mac_tgt_t MAC_TIM_TRIO[] = {{MAC_SHP, 100, MC_ALWAYS}, {ME(3), 30, MC_ALWAYS}, {0, 0, 0}};                                 /* PULSE WIDTH (the TIMBRE input); DETUNE */
static const mac_tgt_t MAC_TIM_WHEEL[] = {{ME(3), 100, MC_ALWAYS}, {ME(2), -50, MC_ALWAYS}, {ME(5), 25, MC_ALWAYS}, {0, 0, 0}};         /* TOP; BODY opposite, KEY CLICK */
static const mac_tgt_t MAC_TIM_PHYS[] = {{MAC_SHP, 100, MC_ALWAYS}, {ME(2), 60, MC_ALWAYS}, {0, 0, 0}};                                 /* TEXTURE (the TIMBRE input); BRIGHT */
static const mac_tgt_t MAC_TIM_NOISE[] = {{MAC_SHP, 100, MC_ALWAYS}, {ME(5), 40, MC_DENSITY}, {0, 0, 0}};                               /* COLOR (the TIMBRE input); DENSITY (modes LFSR and META only) */
static const mac_tgt_t MAC_TIM_FM6[] = {{ME(2), 100, MC_ALWAYS}, {ME(1), 25, MC_ALWAYS}, {ME(4), -30, MC_ALWAYS}, {0, 0, 0}};           /* MOD LEVEL; FEEDBACK, MOD ENV opposite */
static const mac_tgt_t MAC_TIM_WAVE[] = {{MAC_SHP, 100, MC_ALWAYS}, {ME(2), 40, MC_ALWAYS}, {0, 0, 0}};                                 /* POSITION (the TIMBRE input); BEND */
#undef ME
static const mac_tgt_t *mac_timbre(uint32_t e)
{
    const engine_t *g = ENGINES[eng_idx(e)];
    return g == &ENG_ANALOG ? MAC_TIM_ANALOG : g == &ENG_PHASE ? MAC_TIM_PHASE : g == &ENG_LOFI ? MAC_TIM_LOFI : g == &ENG_FORMANT ? MAC_TIM_VOICE
         : g == &ENG_TRIO ? MAC_TIM_TRIO : g == &ENG_WHEEL ? MAC_TIM_WHEEL : g == &ENG_PHYS ? MAC_TIM_PHYS : g == &ENG_NOISE ? MAC_TIM_NOISE
         : g == &ENG_FM6 ? MAC_TIM_FM6 : g == &ENG_WAVE ? MAC_TIM_WAVE : 0;
}
static const mac_tgt_t *mac_tab(uint32_t m, const track_t *t)   /* macro m's targets on part t (TIMBRE by its engine) */
{
    switch (m) {
    case MAC_TIMBRE: return mac_timbre(t->eng_req);
    case MAC_ENVELOPE: return MAC_ENV_T;
    case MAC_FILTER: return MAC_FILT_T;
    case MAC_CHARACTER: return MAC_CHAR_T;
    case MAC_MOTION: return MAC_MOT_T;
    }
    return 0;
}
static int mac_when(uint32_t w, const track_t *t)
{
    switch (w) {
    case MC_LINK_OFF: return !XV(t, X_LINK);
    case MC_SYNC1_OFF: return !t->p[P_LSYNC];
    case MC_SYNC2_OFF: return !t->p[P_L2SYNC];
    case MC_DENSITY: return t->p[P_E0] == NZ_LFSR || t->p[P_E0] == NZ_META;
    case MC_AN_PLAIN: return t->p[P_E0] != 4 && t->p[P_E0] != 5;   /* ANALOG WAVE PWM (4), SYNC (5): eng_analog.c N_ANALOG_WAVE */
    }
    return 1;
}
static int mac_any(void)                          /* a per-part macro off its middle */
{
    uint32_t pi, m;
    for (pi = 0; pi < NLIVE; pi++)
        for (m = 0; m < MAC_NPART; m++)
            if (mac_v[pi][m])
                return 1;
    return 0;
}
/* part pi's set from its five values -> the set the ISR does not read, then published (main loop) */
static void mac_build(uint32_t pi)
{
    const track_t *t = &trk[pi];
    mac_set_t *ms = &mac_set[pi][(mac_cur[pi] & 1u) ^ 1u];
    uint32_t m, k, j;
    ms->n = 0;
    ms->shp = 0;
    ms->eng = (uint8_t)eng_idx(t->eng_req);
    for (m = 0; m < MAC_NPART; m++) {
        int32_t v = mac_v[pi][m];
        const mac_tgt_t *g = v ? mac_tab(MAC_TIMBRE + m, t) : 0;
        for (k = 0; g && g[k].share; k++) {
            int32_t a = v * g[k].share, o;
            uint32_t id = g[k].id;
            if (!mac_when(g[k].when, t))
                continue;
            if (id == MAC_SHP) {                       /* the TIMBRE input: its whole range, 128 steps of 256, over a half-turn */
                ms->shp += a * (128 * 256) / 10000;
                continue;
            }
            {
                const param_desc_t *d = mod_pd(t, id);
                if (d->max <= d->min || d->fmt == F_ENUM)   /* (a list control never: rule 7.1.5; nothing to move) */
                    continue;
                o = a * (d->max - d->min) / 10000;
            }
            for (j = 0; j < ms->n && ms->id[j] != id; j++)
                ;
            if (j == ms->n) {
                if (j >= MAC_NT)
                    continue;
                ms->id[j] = (uint8_t)id;
                ms->off[j] = 0;
                ms->n++;
            }
            ms->off[j] = (int16_t)clamp(ms->off[j] + o, -32000, 32000);   /* (two macros on one control: their offsets add) */
        }
    }
    mac_cur[pi] = (uint8_t)((mac_cur[pi] & 1u) ^ 1u);
    mac_live[pi] = (uint8_t)(ms->n || ms->shp);
}
/* every pass of the main loop (ui_input.c): the sets follow the values, the engine, LINK, the LFOs' SYNC and NOISE's MODE (at most once a millisecond; nothing while every
 * macro is in its middle and nothing is published) */
static void mac_poll(void)
{
    static uint32_t at;
    uint32_t pi;
    if (!mac_any() && !(mac_live[0] | mac_live[1]))
        return;
    if (fm1_ms == at)
        return;
    at = fm1_ms;
    for (pi = 0; pi < NLIVE; pi++)
        mac_build(pi);
}
/* a MIDI CC value 0..127 as a macro value: 64 the middle (0), 0 = -100, 127 = +100 (section 9) */
static int32_t mac_from_cc(uint32_t v) { return v >= 64u ? ((int32_t)v - 64) * 100 / 63 : ((int32_t)v - 64) * 100 / 64; }
