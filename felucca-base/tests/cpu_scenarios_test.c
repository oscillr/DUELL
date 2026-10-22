/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.11: STEADY-STATE CPU scenarios (docs/CPU_ANALYSIS.md; loadcost_test.c counts the cost of a CHANGE). Notes held for 160 halves
 * after a settle (attacks, ramps, WAVE tables built: wave_service), host instructions per half (proc_pid_rusage) in the loadcost scale:
 * 420 k instructions = 100 % of the device's half (33 a sample = 1 %; the device reads ~10-25 % lower, one calibration point: M15).
 * Build: -O1 -fno-builtin, the device's memset / memcpy (as loadcost_test.c). Needs macOS; elsewhere it reports and passes.
 * Default run (host_tests.sh "cpuscen"): Q, the factory multis M01 (0.2.13: REESE SWAMP, the first start's), M03..M06 (the four emulator-made
 * ones, M01..M04 before 0.2.13), M14, M15 at fader 0 / 63 / 127 (4 + 4 notes and 8 on part 1), with
 * guards on the fader in the middle (SMART's wake rule, morph.c morph_wake_half: the morph must not creep back up); C, the VOICE CAP
 * (voice.c vcap: the caps of heavy pairs and the factory multis, measured with and without, never below 4, new notes only).
 * DUELL 0.2.23: the multis of Q, N, K, O are the TEST SCENES of tests/scenes.h (0.2.22's factory multis and CPU TEST multis, byte for byte, loaded as then);
 * G (default) today's sixteen factory multis (MODE RECALL ON), 4 + 4 notes at fader 63, guarded at 100 % (M16 the STRESS TEST unguarded); S the STRESS TEST
 * played from the keys in LAYER / STEREO / DUEL, 1..8 keys (docs/TESTING.md).
 * CPU_SCEN=letters for more: I the INSERT types incl. TAPE, VINYL (0.2.14), N the other factory multis (M02, M07..M10) as Q (no guard), P the cap's per-voice table, E engine pairs x SMART / WILD rolls, M what a factory End B moves, X the
 * stages a small move wakes, W WAVE incl. the kitchen sink, K the faded choices of M04 (WIRED SNAIL) / M14 / M15, F bursts (retrigger, 64 MIDI
 * events, 63 CC74), L (0.2.14) PERFORM's K3 FILTER steady and turned, on one scene. Y (0.2.14, default): a flat part EQ (gains 0) is not run:
 * the output bit for bit and the cost as with none, and what an EQ on costs. V (0.2.25, default): M16 STRESS TEST (as S, fader at A, COMP off) in LAYER and
 * DUEL, 1..3 keys, with the OVERLOAD GUARD engaged (fx.c ovl_lite: RANDOM as the plain delay) against not: the points it saves */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define _FORTIFY_SOURCE 0
#define HOST_IRQ_HOOK 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif
#include "memfast.h"
void *memset(void *d, int c, size_t n) { return mf_memset(d, c, (unsigned)n); }
void *memcpy(void *d, const void *s, size_t n) { return mf_memcpy(d, s, (unsigned)n); }
static void fm1_irq_off(void);
static void fm1_irq_on(void);
#include "ui_test.c"
#include "scenes.h"
static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { const uint8_t *s = src; uint32_t i; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}
#define HB 420000.0
static uint64_t ovh;
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}
static int32_t ob[2 * CTL];
static uint64_t g_hash;                          /* (Y: the output, hashed outside the count) */
static uint64_t blk(void)
{
    uint64_t i0 = instr_now(), c;
    uint32_t i;
    mix_block(ob, CTL);
    c = instr_now() - i0 - ovh;
    for (i = 0; i < 2u * CTL; i++)
        g_hash = g_hash * 1000003u + (uint32_t)ob[i];
    return c;
}
static uint64_t half(uint64_t *mb) { uint64_t s = 0, c, m = 0; uint32_t b; for (b = 0; b < HALF_FRAMES / CTL; b++) { c = blk(); s += c; if (c > m) m = c; } if (mb) *mb = m; return s; }
static uint32_t g_mods;                           /* DUELL 0.2.14 GLO > MODULES switched OFF for a run (core.h MODS_*; section O) */
static void power(void)
{
    uint32_t k;
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    for (k = 0; k < NTRK; k++) memset(fm6_patch[k], 0, sizeof fm6_patch[k]);
    flash_ok = 1;
    multi_boot();
    frame();
    if (g_mods)
        mods_put(g_mods, 1u);                     /* (their fades run out in each run's settle) */
}
static uint32_t eng_named(const char *n) { uint32_t e; for (e = 0; e < NENGINES; e++) if (str_eq(ENGINES[e]->name, n)) return e; return 99; }
static void load_patch(track_t *t, uint32_t e, uint32_t k)
{
    uint32_t i;
    t->eng_req = (uint8_t)e;
    for (i = 0; i < 8u; i++) t->p[P_E0 + i] = ENGINES[e]->edit[i].def;
    apply_preset_to(t, k);
}
static const uint8_t N8[8] = {48, 52, 55, 59, 62, 65, 69, 72};
/* scene description */
typedef struct {
    int e1, k1, n1, e2, k2, n2;   /* engines (-1 none), sound, notes */
    int mode;                     /* DM_* */
    int uf;                       /* 0 as the sound, 1: 24 dB HP RES 80, 2: 12 dB LP RES 80, 3: 24 dB LP RES 80, 4: 24 dB BP-ish */
    int rtype;                    /* -1 as is */
    int sends;                    /* 1: CHO 60 DLY 60 REV 80 on both parts */
    int pingmod;                  /* 1: PING-PONG on, DEPTH 60 */
    int ins;                      /* insert type both parts (0 as is) */
    int eq, duck;
    int perf;                     /* 0 none, 1 R16+FLG+PHS+DRV+PUMP, 2 OCT UP, 3 FREEZE, 4 FLG+PHS+DRV+PUMP (no buffer) */
    int space;                    /* PERFORM SPACE value */
    int morph;                    /* 0 none, 1 both ends rolled, fader mid (SMART, AMOUNT 100), 2 WILD */
    int vmode;                    /* P_VOICE override (-1 none) */
    int filt;                     /* PERFORM K3 FILTER value (0.2.14: section L) */
} scn_t;
static const int *g_ins_abc;                  /* the INSERT's A B C for the scenario (0: as loaded) */
static void setpart(track_t *t, int e, int k, const scn_t *s)
{
    if (e < 0) return;
    load_patch(t, (uint32_t)e, (uint32_t)k);
    t->p[P_SUS] = 127; t->p[P_AMODE] = 0; t->p[P_CHRD] = 0;
    if (s->vmode >= 0) t->p[P_VOICE] = (int16_t)s->vmode; else t->p[P_VOICE] = V_POLY;

    if (s->uf && ENGINES[e]->ufilt) {
        t->p[P_FREEA] = 50; t->p[P_FREEB] = 80;
        t->p[P_FREEC] = s->uf == 1 ? UF_HP : s->uf == 4 ? 96 : 0;
        t->p[P_FREED] = s->uf == 2 ? 1 : 2;
    }
    if (s->sends) { t->p[P_CHOR] = 60; t->p[P_DLY] = 60; t->p[P_REV] = 80; }
    if (s->ins) { t->p[P_ITYPE] = (int16_t)s->ins; t->p[P_IMIX] = 127; if (g_ins_abc) { t->p[P_IA] = (int16_t)g_ins_abc[0]; t->p[P_IB] = (int16_t)g_ins_abc[1]; t->p[P_IC] = (int16_t)g_ins_abc[2]; } }
    if (s->eq) { t->p[P_EQLG] = 5; t->p[P_EQLF] = -4; t->p[P_EQMG] = -4; t->p[P_EQMF] = 5; t->p[P_EQMQ] = 3; t->p[P_EQHG] = 6; t->p[P_EQHF] = 2; }
    if (s->eq == 2) t->p[P_EQLG] = t->p[P_EQMG] = t->p[P_EQHG] = 0;   /* (Y: flat, its other four values moved) */
    if (s->duck) { t->p[P_DKDEP] = 90; t->p[P_DKATK] = 20; t->p[P_DKRLS] = 70; t->p[P_DKSRC] = 0; }
}
static double last_peak, last_blk;
static int g_amt = 100, g_fader = 64, g_x = 0, g_sweep = 0, g_master = 0;   /* (g_master: MASTER, Q12, 0 = as the test leaves it: silent) */
static double run(const scn_t *s)
{
    uint32_t i, n;
    uint64_t sum = 0, pk = 0, mb, h, bm = 0;
    power();
    if (g_master)
        song.master_q12 = (uint16_t)g_master;
    duell_mode_force((uint32_t)s->mode);
    setpart(&trk[0], s->e1, s->k1, s);
    setpart(&trk[1], s->e2, s->k2, s);
    for (i = 0; i < 2; i++) {
        track_t *t = &trk[i];
        if (g_x & 1) { t->p[P_FREEA] = 78; t->p[P_FREEB] = 6; t->p[P_FREEC] = 5; }
        if (g_x & 2) t->p[P_DIST] = 7;
        if (g_x & 4) { if (!t->p[P_CHOR]) t->p[P_CHOR] = 2; if (!t->p[P_DLY]) t->p[P_DLY] = 1; if (!t->p[P_REV]) t->p[P_REV] = 8; }
        if (g_x & 8) t->p[P_SPRD] = 6;
        if (g_x & 16) { t->p[P_LD_AMP] = 8; t->p[P_LD_SHP] = 2; }
    }
    if (s->rtype >= 0) song.g[G_RTYPE] = (int16_t)s->rtype;
    if (s->pingmod) { song.g[G_DMODE] = DMD_PING; song.g[G_DMDEP] = 60; song.g[G_DMRATE] = 40; }
    for (i = 0; i < 40; i++) { half(0); while (wave_service()) ; }   /* engine switch fades, the mode, WAVE tables built */

    perf_space = (int8_t)s->space;
    perf_filt = (int8_t)s->filt;
    if (s->perf == 1) { perf_press(PF_R16, 1); perf_press(PF_FLG, 1); perf_press(PF_PHS, 1); perf_press(PF_DRV, 1); perf_press(PF_PUMP, 1); }
    if (s->perf == 2) perf_press(PF_OUP, 1);
    if (s->perf == 3) perf_press(PF_FRZ, 1);
    if (s->perf == 4) { perf_press(PF_FLG, 1); perf_press(PF_PHS, 1); perf_press(PF_DRV, 1); perf_press(PF_PUMP, 1); }
    if (s->morph == 3) { morph_rebase_all(); morph_set_target(1); morph_set_fader((uint32_t)g_fader); }
    else if (s->morph) {
        if (s->morph == 2) morph.cfg |= MC_WILD;
        morph_set_target(1); morph_set_amount((uint32_t)g_amt);
        morph_roll(0); morph_roll(1);
        morph_set_fader((uint32_t)g_fader);
    }
    transport_req = 1;
    for (i = 0; s->e1 >= 0 && i < (uint32_t)s->n1; i++) trk_note_on(&trk[0], N8[i], 110);
    for (i = 0; s->e2 >= 0 && i < (uint32_t)s->n2; i++) trk_note_on(&trk[1], (uint8_t)(N8[i] + 7), 110);
    for (i = 0; i < 120; i++) { half(0); while (wave_service()) ; }          /* settle: attacks, ramps, the morph's ramp */
    for (n = 0; n < 160; n++) {
        if (g_sweep)                              /* (L: K3 turned all the time, a detent a half, between g_sweep and the middle's side) */
            perf_filt = (int8_t)(g_sweep + (g_sweep < 0 ? 1 : -1) * (int)(n % 40u < 20u ? n % 40u : 40u - n % 40u));
        h = half(&mb); sum += h; if (h > pk) pk = h; if (mb > bm) bm = mb;
    }
    perf_filt = 0;
    last_peak = pk * 100.0 / HB;
    last_blk = bm * 4 * 100.0 / HB;
    return sum / 160.0 * 100.0 / HB;
}
static scn_t base(void) { scn_t s = {-1, 0, 8, -1, 0, 0, DM_LAYER, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, -1}; return s; }
static const char *EN[] = {"ANALOG", "FM6", "PHASE", "LOFI", "VOICE", "TRIO", "WHEEL", "PHYS", "NOISE", "WAVE", "DRUM"};
static const char *RN[5] = {"ROOM", "SPRING", "HALL", "SHIM", "RESO"};
static void pr(const char *what, const scn_t *s) { double a = run(s); printf("%-78s avg %6.1f %%  peak half %6.1f %%\n", what, a, last_peak); fflush(stdout); }

/* a burst in ONE block on top of a steady scene: kind 0 retrigger 8 notes (off+on), 1 64 MIDI note events via the ring, 2 64 CC74 via the ring,
 * 3 a chord-key press (P_CHRD) etc. Returns the worst half around it (%), *st the steady half */
static double burst(const scn_t *s, int kind, double *st)
{
    uint32_t i, n;
    uint64_t pk = 0, h, sum = 0;
    run(s);                                       /* (leaves the scene running, notes held) */
    for (n = 0; n < 16; n++) sum += half(0);
    *st = sum / 16.0 * 100.0 / HB;
    if (kind == 0) for (i = 0; i < 8; i++) { trk_note_off(&trk[0], N8[i]); trk_note_on(&trk[0], (uint8_t)(N8[i] + 1), 120); }
    if (kind == 1) for (i = 0; i < 32; i++) { midi_enqueue(0x09u | 0x90u << 8 | (uint32_t)(40 + i) << 16 | 100u << 24, 1); midi_enqueue(0x08u | 0x80u << 8 | (uint32_t)(40 + i) << 16, 1); }
    if (kind == 2) for (i = 0; i < 63; i++) midi_enqueue(0x0Bu | 0xB0u << 8 | 74u << 16 | (uint32_t)(i * 2) << 24, 1);
    for (n = 0; n < 8; n++) { h = half(0); if (h > pk) pk = h; }
    return pk * 100.0 / HB;
}
static int bad;
static void guard(const char *what, double v, double lim)
{
    printf("cpuscen: %-66s %6.1f %% (limit %5.1f) %s\n", what, v, lim, v <= lim ? "ok" : "FAIL");
    bad += v > lim;
}
/* DUELL 0.2.23: the multis these scenarios were written for (0.2.22's factory multis and the CPU TEST multis M13..M16) are test scenes now
 * (tests/scenes.h): load_scene_of(slot) loads the one that slot held, as it loaded then (the TEST ones with their MODE) */
static const char *const OLD_SCENE[16] = {"M01_REESE_SWAMP", "M02_WHISPER_TIDE", "M03_EERIE_SIGNAL", "M04_WIRED_SNAIL", "M05_DEAD_BUFFER", "M06_ACID_ETHER",
                                          "M07_COLD_CHAPEL", "M08_GARGLE_ACID", "M09_NIGHT_ORGAN", "M10_FINAL_REEL", 0, 0,
                                          "M13_TEST_1_BASE", "M14_TEST_2_PHASE", "M15_TEST_3_HEAVY", "M16_TEST_4_MIX"};
static void load_scene_of(uint32_t slot)
{
    if (slot >= 12u)
        scene_load(slot, OLD_SCENE[slot]);
    else {
        scene_put(slot, OLD_SCENE[slot]);
        project_load(slot);
    }
}
/* the multis as stored (the scenes, above), nn notes (4: 4 + 4 on both parts, 8: on part 1), the fader at f; avg (returned) and *pk the worst half */
static int g_factory;                             /* (0.2.23, F: multi_run on the factory multi of the slot, loaded with its MODE) */
static double multi_run(uint32_t slot, uint32_t nn, uint32_t f, double *pk)
{
    uint64_t sum = 0, p = 0, h;
    uint32_t i, n;
    power();
    if (g_factory) {
        ui_prefs2 |= PREF_MODE_RECALL >> 16;
        project_load(slot);
    } else
        load_scene_of(slot);
    for (i = 0; i < 40; i++) { half(0); while (wave_service()) ; }
    morph_set_target(1);
    morph_set_fader(f);
    for (i = 0; i < nn; i++) {
        trk_note_on(&trk[0], N8[i], 110);
        if (nn == 4)
            trk_note_on(&trk[1], (uint8_t)(N8[i] + 7), 110);
    }
    for (i = 0; i < 120; i++) { half(0); while (wave_service()) ; }
    for (n = 0; n < 160; n++) { h = half(0); sum += h; if (h > p) p = h; }
    *pk = p * 100.0 / HB;
    return sum / 160.0 * 100.0 / HB;
}
/* ---- C: the VOICE CAP (DUELL 0.2.11, voice.c vcap) ---- */
static uint32_t sound_named(uint32_t e, const char *n)
{
    uint32_t k, m = ds_count(e);
    for (k = 0; k < m; k++)
        if (str_eq(ds_name(e, k), n))
            return k;
    return 0;
}
/* the scene s (its sounds, filters), loaded as the UI does (End B rolled: morph_rebase_all), the fader at 64, the caps (vcap_poll);
 * then every split of the 8-voice pool the caps allow (n on the dearer part, the rest on the other) held, the worst average kept;
 * capped 0: as without a cap (the engines' own). Returns that worst %, *c the caps, *pred the predicted worst (%) */
static double cap_alloc(const scn_t *s, uint32_t nh, uint32_t nl, uint32_t h)
{
    uint32_t i, n;
    uint64_t sum = 0;
    for (i = 0; i < nh; i++) trk_note_on(&trk[h], N8[i], 110);
    for (i = 0; i < nl; i++) trk_note_on(&trk[!h], (uint8_t)(N8[i] + 7), 110);
    for (i = 0; i < 120; i++) { half(0); while (wave_service()) ; }
    for (n = 0; n < 160; n++) sum += half(0);
    (void)s;
    return sum / 160.0 * 100.0 / HB;
}
static void cap_scene(const scn_t *s)
{
    uint32_t i;
    power();
    if (g_master)
        song.master_q12 = (uint16_t)g_master;
    duell_mode_force((uint32_t)s->mode);
    setpart(&trk[0], s->e1, s->k1, s);
    setpart(&trk[1], s->e2, s->k2, s);
    for (i = 0; i < 40; i++) { half(0); while (wave_service()) ; }
    morph_rebase_all();
    morph_set_target(1);
    morph_set_fader(64);
    vcap_poll();
}
static double cap_run(const scn_t *s, int capped, uint32_t *c, double *pred)
{
    uint32_t k[2], h, nh, ch, cl;
    double w = 0, a;
    cap_scene(s);
    k[0] = vc_voice(&trk[0]);
    k[1] = vc_voice(&trk[1]);
    c[0] = vcap_shown(0);
    c[1] = vcap_shown(1);
    *pred = vcap_worst / 10.0;
    h = k[1] > k[0];
    ch = capped ? c[h] : vc_eng_cap(ENGINES[eng_idx(trk[h].eng_req)]);
    cl = capped ? c[!h] : vc_eng_cap(ENGINES[eng_idx(trk[!h].eng_req)]);
    for (nh = ch; ; nh--) {                            /* the dearer part from its cap down, the other part the rest */
        uint32_t nl = NVOICE - nh < cl ? NVOICE - nh : cl;
        cap_scene(s);
        if (!capped)
            vcap[0] = vcap[1] = 0;
        a = cap_alloc(s, nh, nl, h);
        w = a > w ? a : w;
        if (!nh || nl == cl)
            break;
    }
    return w;
}
static void cap_tests(void)
{
    static const struct { const char *what, *e1, *s1, *e2, *s2; int uf, mode; } CS[] = {
        {"#60 DIGITAL PAD (WAVE) 24 dB HP + #5 SQUELCH (ANALOG), STEREO", "WAVE", "DIGITAL PAD", "ANALOG", "SQUELCH", 1, DM_STEREO},
        {"#60 DIGITAL PAD (WAVE) as stored (12 dB) + #5 SQUELCH, STEREO", "WAVE", "DIGITAL PAD", "ANALOG", "SQUELCH", 0, DM_STEREO},
        {"#60 DIGITAL PAD (WAVE) 24 dB HP + #5 SQUELCH, LAYER (4 + 4)", "WAVE", "DIGITAL PAD", "ANALOG", "SQUELCH", 1, DM_LAYER},
        {"#47 GLASS HALL (FM6) + #60 DIGITAL PAD (WAVE), 24 dB HP both", "FM6", "GLASS HALL", "WAVE", "DIGITAL PAD", 1, DM_SPLIT},
        {"#47 GLASS HALL (FM6) + #60 DIGITAL PAD (WAVE), 24 dB LP both", "FM6", "GLASS HALL", "WAVE", "DIGITAL PAD", 3, DM_SPLIT},
        {"#47 GLASS HALL (FM6) + #60 DIGITAL PAD (WAVE) as stored", "FM6", "GLASS HALL", "WAVE", "DIGITAL PAD", 0, DM_SPLIT},
        {"#109 LESLIE STORM (WHEEL) 24 dB HP + #37 PAN WHISTLE (PHYS)", "WHEEL", "LESLIE STORM", "PHYS", "PAN WHISTLE", 1, DM_SPLIT},
        {"#117 GLASS SHARDS (ANALOG) 24 dB HP + #53 CHIP CHAPEL (TRIO)", "ANALOG", "GLASS SHARDS", "TRIO", "CHIP CHAPEL", 1, DM_SPLIT},
        {"#13 CARTRIDGE (LOFI) 24 dB HP + #70 WIRE HARP (PHASE)", "LOFI", "CARTRIDGE", "PHASE", "WIRE HARP", 1, DM_SPLIT},
    };
    uint32_t i, c[2], k[2], e[2], cap[2], ok = 1;
    double pred, a, a0, p0;
    char b[200];
    for (i = 0; i < NELEM(CS); i++) {
        scn_t s = base();
        uint32_t c0[2];
        s.e1 = (int)eng_named(CS[i].e1); s.k1 = (int)sound_named((uint32_t)s.e1, CS[i].s1);
        s.e2 = (int)eng_named(CS[i].e2); s.k2 = (int)sound_named((uint32_t)s.e2, CS[i].s2);
        s.uf = CS[i].uf; s.mode = CS[i].mode;
        a = cap_run(&s, 1, c, &pred);
        a0 = cap_run(&s, 0, c0, &p0);
        printf("C %-62s caps %u / %u (predicted worst %5.1f %%): measured worst split with the caps %5.1f %%, without %5.1f %%",
               CS[i].what, c[0], c[1], pred, a, a0);
        {   /* the dearer part played alone (the other loaded, silent): its cap vs its engine's voices */
            uint32_t kk[2], h, nc, ne;
            double al, al0;
            cap_scene(&s);
            kk[0] = vc_voice(&trk[0]); kk[1] = vc_voice(&trk[1]); h = kk[1] > kk[0];
            nc = trk_nvoice(&trk[h]); ne = vc_eng_cap(ENGINES[eng_idx(trk[h].eng_req)]);
            al = cap_alloc(&s, nc, 0, h);
            cap_scene(&s); vcap[0] = vcap[1] = 0;
            al0 = cap_alloc(&s, ne, 0, h);
            printf("; part %u alone: %u voices %5.1f %%, %u voices %5.1f %%\n", h + 1u, nc, al, ne, al0);
        }
        ok &= c[0] >= VC_MIN || c[0] == vc_eng_cap(ENGINES[eng_idx(trk[0].eng_req)]);
        ok &= c[1] >= VC_MIN || c[1] == vc_eng_cap(ENGINES[eng_idx(trk[1].eng_req)]);
    }
    {   /* the factory multis as loaded (0.2.23: all sixteen, with their MODE: MODE RECALL ON) */
        for (i = 0; i < MU_SLOTS; i++) {
            power();
            ui_prefs2 |= PREF_MODE_RECALL >> 16;
            project_load(i);
            frame();
            printf("C M%02u %-12s (%s / %s) as loaded: caps %u / %u, predicted worst %5.1f %%\n", i + 1, mu_names[i], ENGINES[eng_idx(trk[0].eng_req)]->name,
                   ENGINES[eng_idx(trk[1].eng_req)]->name, vcap_shown(0), vcap_shown(1), vcap_worst / 10.0);
            {   uint32_t q;                              /* (an engine's own cap under 4 stays: M05's PHYS plays 3) */
                for (q = 0; q < 2u; q++)
                    ok &= vcap_shown(q) >= VC_MIN || vcap_shown(q) == vc_eng_cap(ENGINES[eng_idx(trk[q].eng_req)]);
            }
        }
    }
    guard("0.2.11 VOICE CAP: never below 4 voices a part (an engine's own cap under 4 stays as is)", ok ? 0.0 : 1.0, 0.0);
    {   /* the rule alone: never below VC_MIN, the dearer part first, nothing over the budget when it can be helped */
        int okc = 1;
        uint32_t x, y;
        for (x = 0; x <= 200u; x += 5u)
            for (y = 0; y <= 200u; y += 5u) {
                uint32_t w;
                k[0] = x; k[1] = y; e[0] = e[1] = NVOICE;
                w = vc_caps(k, e, cap, 0);
                okc &= cap[0] >= VC_MIN && cap[1] >= VC_MIN && cap[0] <= NVOICE && cap[1] <= NVOICE;
                if (w <= VC_BUDGET && vc_worst(k, e) > VC_BUDGET)       /* (reachable: only the dearer part is capped, as little as needed) */
                    okc &= cap[!(y > x)] == NVOICE && (cap[y > x] == NVOICE || (cap[y > x]++, vc_worst(k, cap) > VC_BUDGET));
                else if (w > VC_BUDGET) {                                /* (not: each part as if alone, at least 4) */
                    uint32_t c0 = x ? (VC_BUDGET - VC_IDLE) / x : 8u, c1 = y ? (VC_BUDGET - VC_IDLE) / y : 8u;
                    c0 = c0 < VC_MIN ? VC_MIN : c0 > 8u ? 8u : c0;
                    c1 = c1 < VC_MIN ? VC_MIN : c1 > 8u ? 8u : c1;
                    okc &= cap[0] == c0 && cap[1] == c1;
                } else
                    okc &= cap[0] == NVOICE && cap[1] == NVOICE;   /* (under it already: no cap) */
            }
        k[0] = k[1] = 60u; e[0] = 8u; e[1] = 6u;
        vc_caps(k, e, cap, 0);
        okc &= cap[0] == 8u && cap[1] == 6u;           /* (60 a voice: 15 + 8 x 6 = 63 %: no cap) */
        k[0] = 200u; k[1] = 0u;                         /* (part 2 muted: part 1 alone: 15 + 4 x 20 = 95 %, at 4 at least) */
        vc_caps(k, e, cap, 0);
        okc &= cap[0] == VC_MIN && cap[1] == 6u;
        k[0] = k[1] = 100u;                             /* (two dear parts, 4 + 4 = 95 %: each as if alone, 7 x 10 = 70 %: 7 / 6) */
        vc_caps(k, e, cap, 0);
        okc &= cap[0] == 7u && cap[1] == 6u;
        k[0] = 90u; k[1] = 0u;                          /* (15 + 7 x 9 = 78 %: 7) */
        vc_caps(k, e, cap, 0);
        okc &= cap[0] == 7u && cap[1] == 6u;
        vc_caps(k, e, cap, 1);
        okc &= cap[0] == 8u && cap[1] == 6u;          /* (LAYER: 4 + 4 whatever the caps: none set) */
        guard("0.2.11 VOICE CAP rule: never below 4; reachable: the dearer part, as little as needed; else each part as if alone", okc ? 0.0 : 1.0, 0.0);
    }
    {   /* it bites on NEW notes only: 8 held voices stay when the cap drops; new notes never raise the part above what sounds, and
         * after letting go the part takes at most its cap */
        scn_t s = base();
        uint32_t n0, n1, n2, j;
        s.e1 = (int)eng_named("WAVE"); s.k1 = (int)sound_named((uint32_t)s.e1, "DIGITAL PAD");
        s.e2 = (int)eng_named("ANALOG"); s.k2 = 0;
        power();
        duell_mode_force(DM_STEREO);
        setpart(&trk[0], s.e1, s.k1, &s);
        setpart(&trk[1], s.e2, s.k2, &s);
        for (i = 0; i < 40; i++) { half(0); while (wave_service()) ; }
        morph_rebase_all();
        vcap_poll();
        for (i = 0; i < 8u; i++) trk_note_on(&trk[0], N8[i], 110);
        for (i = 0; i < 20; i++) half(0);
        trk[0].p[P_FREEA] = 50; trk[0].p[P_FREEB] = 80; trk[0].p[P_FREEC] = UF_HP; trk[0].p[P_FREED] = 2;   /* 24 dB HP now: heavier */
        morph_rebase_all();
        vcap_poll();
        for (n0 = 0, j = 0; j < NVOICE; j++) n0 += trk[0].v[j].active && trk[0].v[j].gate && trk[0].v[j].stage != 4u;
        for (i = 0; i < 6u; i++) { trk_note_on(&trk[0], (uint8_t)(80u + i), 110); half(0); }
        for (n1 = 0, j = 0; j < NVOICE; j++) n1 += trk[0].v[j].active && trk[0].v[j].stage != 4u;
        trk_all_off(&trk[0]);
        for (i = 0; i < 400; i++) half(0);
        for (i = 0; i < 8u; i++) trk_note_on(&trk[0], N8[i], 110);
        for (i = 0; i < 4; i++) half(0);
        for (n2 = 0, j = 0; j < NVOICE; j++) n2 += trk[0].v[j].active && trk[0].v[j].gate;
        printf("C new notes only: cap %u; held when it dropped %u, after 6 more notes %u sounding, 8 pressed later %u held\n", vcap_shown(0), n0, n1, n2);
        guard("0.2.11 VOICE CAP bites on new notes only (no held note cut; then at most the cap)",
              (n0 == 8u && n1 <= 8u && n2 == vcap_shown(0) && vcap_shown(0) < 8u) ? 0.0 : 1.0, 0.0);
    }
}

/* ---- S (0.2.23): M16 STRESS TEST played as on the unit: the keys (the real input path, the arps on the internal clock, the transport stopped) in
 * LAYER, STEREO and DUEL; in STEREO and DUEL the keys play the selected part only (part 1), so there the same notes also come on MIDI channel 2
 * (part 2), the dearest case. n keys held (1..8: C4 E4 G4 B4 D5 F5 D4 A4), the MORPH fader at A (as loaded) or at 63, the master COMP (a device
 * setting, not in a multi) off or on (4:1, -20 dB); the load once the voices settle (avg and the worst half of 160 halves after 120). The voice cap
 * (voice.c vcap) is on, as on the unit. 100 % = 420 k host instructions a half (33 a sample = 1 %; the device reads about 0.75 .. 0.9 of it) */
static uint32_t key_of_note(uint32_t note) { uint32_t k; for (k = 0; k < 27u; k++) if (kb_map(&trk[0], k) == note) return k; return 27u; }
static uint32_t g_ovl;                            /* (0.2.25, section V: the overload guard forced on for the run: fx.c ovl_lite) */
static double stress_run(uint32_t mode, uint32_t nk, uint32_t fader, uint32_t comp, double *pk, uint32_t *voices)
{
    static const uint8_t KN[8] = {60, 64, 67, 71, 74, 77, 62, 69};
    uint64_t sum = 0, p = 0, h;
    uint32_t i, n, keys = 0, vb = 0;
    power();
    ui_prefs2 |= PREF_MODE_RECALL >> 16;
    project_load(15);
    duell_mode_req = (uint8_t)mode;
    if (comp) {
        song.cp[0] = 20; song.cp[1] = 4;
    }
    ovl_lite = (uint8_t)g_ovl;
    frame();
    for (i = 0; i < 40; i++) { half(0); while (wave_service()) ; frame(); }
    morph_set_target(1);
    morph_set_fader(fader);
    for (i = 0; i < nk; i++) {
        keys |= 1u << key_of_note(KN[i]);
        if (mode != DM_LAYER)
            midi_in_event(0x09u | 0x91u << 8 | (uint32_t)KN[i] << 16 | 110u << 24);
    }
    fm1_in.notes = keys;
    for (i = 0; i < 120; i++) { half(0); while (wave_service()) ; }
    for (n = 0; n < 160; n++) {
        uint32_t v = voices_busy();
        h = half(0); sum += h; if (h > p) p = h;
        vb = v > vb ? v : vb;
    }
    fm1_in.notes = 0;
    *pk = p * 100.0 / HB;
    *voices = vb;
    return sum / 160.0 * 100.0 / HB;
}
static void stress_table(void)
{
    static const uint32_t MD[3] = {DM_LAYER, DM_STEREO, DM_SPLIT};
    static const char *const MN[3] = {"LAYER (keys: both parts)", "STEREO (keys part 1 + MIDI ch 2 part 2)", "DUEL (keys part 1 + MIDI ch 2 part 2)"};
    static const uint32_t CF[3][2] = {{0, 0}, {0, 1}, {63, 1}};          /* (fader, COMP) */
    uint32_t m, nk, c, vb;
    double pk, av;
    for (m = 0; m < 3u; m++)
        for (c = 0; c < 3u; c++) {
            uint32_t ok_avg = 0, ok_pk = 0;
            for (nk = 1; nk <= 8u; nk++) {
                av = stress_run(MD[m], nk, CF[c][0], CF[c][1], &pk, &vb);
                printf("S M16 %-40s fader %2u COMP %-3s %u keys: avg %6.1f %%  peak half %6.1f %%  voices %u\n", MN[m], CF[c][0], CF[c][1] ? "ON" : "off",
                       nk, av, pk, vb);
                if (av <= 100.0 && ok_avg == nk - 1u)
                    ok_avg = nk;
                if (pk <= 100.0 && ok_pk == nk - 1u)
                    ok_pk = nk;
            }
            printf("S   = %s, fader %u, COMP %s: at most %u keys with the average under 100 %%, %u with every half under it\n", MN[m], CF[c][0],
                   CF[c][1] ? "ON" : "off", ok_avg, ok_pk);
        }
}

int main(void)
{
    uint32_t i, k, ei, n;
    scn_t s;
    char b[200];
    const char *sec = getenv("CPU_SCEN") ? getenv("CPU_SCEN") : "QGCYOV";
    uint64_t a0 = instr_now(), a1 = instr_now();
    setvbuf(stdout, NULL, _IONBF, 0);
    if (!a0) {
        printf("cpuscen: no instruction counter on this host, skipped\n");
        return 0;
    }
    ovh = a1 - a0;
    for (i = 0; i < 5; i++) { a0 = instr_now(); a1 = instr_now(); if (a1 - a0 < ovh) ovh = a1 - a0; }
    if (strchr(sec, 'Q')) {                       /* the factory multis as stored, fader 0 / 63 / 127 */
        static const uint32_t MS[7] = {0, 2, 3, 4, 5, 13, 14};   /* (0.2.13: M01 REESE SWAMP guarded too; M03..M06 were M01..M04, the same limits) */
        static const double LIM[7] = {85.0, 80.0, 85.0, 85.0, 45.0, 95.0, 115.0};   /* 4 + 4, fader 63 (0.2.11: 69.6 74.5 75.7 35.9 86.5 107.0 for the old six) */
        uint32_t m, f, nn;
        for (m = 0; m < 7; m++) for (nn = 4; nn <= 8; nn += 4) for (f = 0; f < 3; f++) {
            double pk, av = multi_run(MS[m], nn, f * 63u + (f == 2), &pk);
            printf("Q M%02u %s fader %3u: avg %5.1f %%  peak half %5.1f %%  (parts: %s / %s, mode %u)\n", MS[m] + 1, nn == 4 ? "4+4 notes" : "8 notes p1",
                   f * 63u + (f == 2), av, pk, ENGINES[trk[0].engine]->name, ENGINES[trk[1].engine]->name, (unsigned)dm_cur);
            if (nn == 4 && f == 1) {
                snprintf(b, sizeof b, "M%02u 4 + 4 notes, the MORPH fader in the middle (63), average", MS[m] + 1);
                guard(b, av, LIM[m]);
            }
        }
    }
    if (strchr(sec, 'N')) {                       /* (0.2.11 integration) the other factory multis (0.2.13: M02, M07..M10), as Q, no guard */
        static const uint32_t NS[5] = {1, 6, 7, 8, 9};
        uint32_t j, m, f, nn;
        for (j = 0; j < 5; j++) for (nn = 4; nn <= 8; nn += 4) for (f = 0; f < 3; f++) {
            m = NS[j];
            double pk, av = multi_run(m, nn, f * 63u + (f == 2), &pk);
            printf("N M%02u %s fader %3u: avg %5.1f %%  peak half %5.1f %%  (parts: %s / %s, mode %u)\n", m + 1, nn == 4 ? "4+4 notes" : "8 notes p1",
                   f * 63u + (f == 2), av, pk, ENGINES[trk[0].engine]->name, ENGINES[trk[1].engine]->name, (unsigned)dm_cur);
        }
    }
    if (strchr(sec, 'G')) {                       /* 0.2.23: the factory multis of today (MODE RECALL ON), 4 + 4 notes, fader 63; M16 (the STRESS TEST) unguarded */
        uint32_t m;
        g_factory = 1;
        for (m = 0; m < MU_SLOTS; m++) {
            double pk, av = multi_run(m, 4, 63, &pk);
            printf("G M%02u %-12s 4+4 notes fader  63: avg %5.1f %%  peak half %5.1f %%  (parts: %s / %s, mode %u)\n", m + 1, mu_names[m], av, pk,
                   ENGINES[trk[0].engine]->name, ENGINES[trk[1].engine]->name, (unsigned)dm_cur);
            if (m < 15u) {
                snprintf(b, sizeof b, "M%02u %s 4 + 4 notes, fader 63, average (a factory multi)", m + 1, mu_names[m]);
                guard(b, av, 100.0);
            }
        }
        g_factory = 0;
    }
    if (strchr(sec, 'S'))
        stress_table();
    if (strchr(sec, 'V')) {                       /* 0.2.25 (default): M16 STRESS TEST with the OVERLOAD GUARD engaged (RANDOM as the plain delay; 0.2.25: its PITCH BOTH reads UP) against not */
        static const uint32_t MD[2] = {DM_LAYER, DM_SPLIT};
        static const char *const MN[2] = {"LAYER", "DUEL"};
        uint32_t m, nk, vb;
        double pk, av[2];
        for (m = 0; m < 2u; m++)
            for (nk = 1; nk <= 3u; nk++) {
                for (g_ovl = 0; g_ovl < 2u; g_ovl++)
                    av[g_ovl] = stress_run(MD[m], nk, 0, 0, &pk, &vb);
                g_ovl = 0;
                printf("V M16 %-5s %u keys: avg %5.1f %%, with the guard engaged %5.1f %% (%.1f points lighter)\n", MN[m], nk, av[0], av[1], av[0] - av[1]);
                if (nk == 1u) {
                    snprintf(b, sizeof b, "M16 %s 1 key: the guard engaged saves at least %s points", MN[m], m ? "6" : "3");
                    guard(b, -(av[0] - av[1]), m ? -6.0 : -3.0);
                }
            }
    }
    if (strchr(sec, 'C'))
        cap_tests();
    if (strchr(sec, 'Y')) {                       /* (0.2.14) the part EQ flat (its three gains 0) is not run: bit for bit, no cost */
        double c[3];
        uint64_t h[3];
        static const char *const YN[3] = {"none", "flat (gains 0, FREQ / Q moved)", "on (LOW +5, MID -4, HIGH +6)"};
        s = base(); s.e1 = (int)eng_named("ANALOG"); s.n1 = 4; s.e2 = (int)eng_named("FM6"); s.k2 = 2; s.n2 = 4; s.sends = 1;
        for (k = 0; k < 3u; k++) {                /* (each from the same state: a child process; runs in a row carry state over) */
            int fd[2];
            pid_t pid;
            if (pipe(fd))
                return 1;
            pid = fork();
            if (!pid) {
                double v[2];
                s.eq = k == 1u ? 2 : k == 2u ? 1 : 0;
                g_master = 2048;                  /* (MASTER at its default: the output not silent, the hash sees it) */
                g_hash = 0;
                v[0] = run(&s);
                v[1] = (double)(g_hash & 0xFFFFFFFFFFFFull);
                if (write(fd[1], v, sizeof v) != (ssize_t)sizeof v)
                    _exit(1);
                _exit(0);
            }
            {
                double v[2] = {0, 0};
                if (read(fd[0], v, sizeof v) != (ssize_t)sizeof v)
                    v[0] = -1;
                waitpid(pid, 0, 0);
                close(fd[0]);
                close(fd[1]);
                c[k] = v[0];
                h[k] = (uint64_t)v[1];
            }
        }
        printf("Y EQ on a 4 + 4 voice scene, %% of the half: %s %.2f, %s %.2f, %s %.2f (+%.2f)\n", YN[0], c[0], YN[1], c[1], YN[2], c[2], c[2] - c[0]);
        printf("cpuscen: %-66s %s\n", "a flat EQ: the output bit for bit as with none, the same cost", h[0] == h[1] && h[2] != h[0] && fabs(c[1] - c[0]) < 0.3 ? "ok" : "FAIL");
        bad += h[0] != h[1] || h[2] == h[0] || fabs(c[1] - c[0]) >= 0.3;   /* (h[2]: the hash sees the EQ when it is on) */
    }

    if (strchr(sec, 'P')) {                       /* the voice cap's table (voice.c VCOST): a voice of each engine's heaviest sound (2a) */
        static const char *const PE[] = {"ANALOG", "FM6", "PHASE", "LOFI", "VOICE", "TRIO", "WHEEL", "PHYS", "NOISE", "WAVE"};
        static const char *const PS[] = {"GLASS SHARDS", "GLASS HALL", "WIRE HARP", "CARTRIDGE", "GHOST VOICES", "CHIP CHAPEL", "LESLIE STORM", "PAN WHISTLE", "MOOR WIND", "DIGITAL PAD"};
        double idle, v[4];
        uint32_t q, m, nv;
        s = base(); s.n1 = 0; s.e1 = (int)eng_named("ANALOG"); idle = run(&s);
        printf("P idle (part 1 ANALOG, no note) %5.1f %%; per voice (%% of the half), 8 voices (VOICE: 4), part 2 silent:\n", idle);
        for (q = 0; q < NELEM(PE); q++) {
            s = base(); s.e1 = (int)eng_named(PE[q]); s.k1 = (int)sound_named((uint32_t)s.e1, PS[q]);
            nv = str_eq(PE[q], "VOICE") ? 4u : str_eq(PE[q], "PHYS") ? 3u : str_eq(PE[q], "FM6") ? 6u : 8u;
            s.n1 = (int)nv;
            for (m = 0; m < 4u; m++) {
                s.uf = m & 1u ? 1 : 0; s.morph = m & 2u ? 3 : 0; g_fader = 64;
                v[m] = (run(&s) - idle) / nv;
            }
            printf("P %-7s %-13s %u v: stored %4.2f | 24 dB HP %4.2f | stored, morph 64 %4.2f | 24 HP, morph 64 %4.2f\n", PE[q], PS[q], nv, v[0], v[1], v[2], v[3]);
        }
    }
    if (strchr(sec, 'L')) {                       /* (0.2.14) PERFORM's K3 FILTER and the part EQ on a steady scene: what each costs */
        double a0, a;
        s = base(); s.e1 = (int)eng_named("ANALOG"); s.n1 = 4; s.e2 = (int)eng_named("FM6"); s.k2 = 2; s.n2 = 4; s.sends = 1;
        a0 = run(&s);
        printf("L ANALOG#0 4n + FM6#2 4n LAYER, sends: K3 in the middle %5.1f %% (peak half %5.1f)\n", a0, last_peak);
        static const int KF[] = {-1, -50, -95, -98, 1, 50, 95};
        for (k = 0; k < NELEM(KF); k++) {
            s.filt = KF[k]; a = run(&s);
            printf("L   K3 %+4d (steady): %5.1f %% (+%4.2f), peak half %5.1f\n", KF[k], a, a - a0, last_peak);
        }
        s.filt = -60; g_sweep = -60; a = run(&s); printf("L   K3 turned -60 .. -40, a detent a half: %5.1f %% (+%4.2f), peak half %5.1f\n", a, a - a0, last_peak);
        s.filt = 60; g_sweep = 60; a = run(&s); printf("L   K3 turned +60 .. +40, a detent a half: %5.1f %% (+%4.2f), peak half %5.1f\n", a, a - a0, last_peak);
        g_sweep = 0; s.filt = 0;
    }
    if (strchr(sec, 'I')) {                       /* the INSERT types (0.2.14: TAPE, VINYL), ANALOG#0 4 + 4 notes, LAYER, the insert on both parts at MIX 127 */
        static const int DEF[11][3] = {{0}, {64, 96, 96}, {64, 96, 96}, {64, 96, 96}, {64, 96, 96}, {99, 96, 96}, {64, 96, 96}, {64, 96, 96}, {64, 96, 96}, {47, 71, 110}, {47, 47, 17}};
        double base0 = 0, v;
        uint32_t ty;
        for (ty = 0; ty < IT_N; ty++) {
            s = base(); s.e1 = (int)eng_named("ANALOG"); s.n1 = 4; s.e2 = (int)eng_named("ANALOG"); s.n2 = 4; s.ins = (int)ty;
            g_ins_abc = ty ? DEF[ty] : 0;
            v = run(&s);
            if (!ty) base0 = v;
            printf("I %-6s both parts: avg %5.2f %% (%+5.2f %% of the device's half for two parts, %+5.2f per part), peak half %5.1f %%\n", N_ITYPE[ty], v, v - base0, (v - base0) / 2, last_peak);
        }
        g_ins_abc = 0;
    }
    if (strchr(sec, 'W')) {                       /* WAVE again, tables built */
        double w = -1, v; uint32_t wk = 0, e = eng_named("WAVE");
        n = ds_count(e);
        for (k = 0; k < n; k++) { s = base(); s.e1 = (int)e; s.k1 = (int)k; v = run(&s); printf("W WAVE #%u %-14s %5.1f %%\n", k, ds_name(e, k), v); if (v > w) w = v, wk = k; }
        s = base(); s.e1 = (int)e; s.k1 = (int)wk; printf("W worst #%u: as stored %5.1f", wk, run(&s));
        s.uf = 2; printf(" | 12 LP %5.1f", run(&s)); s.uf = 3; printf(" | 24 LP %5.1f", run(&s)); s.uf = 1; printf(" | 24 HP %5.1f %%\n", run(&s));
        s = base(); s.mode = DM_SPLIT; s.e1 = (int)eng_named("FM6"); s.k1 = 2; s.n1 = 4; s.e2 = (int)e; s.k2 = (int)wk; s.n2 = 4; s.sends = 1; s.rtype = 3; s.pingmod = 1;
        pr("W DUEL FM6#2 4n + WAVE worst 4n, sends, SHIM, PING mod", &s);
        s.uf = 1; pr("W .. + 24 dB HP both", &s);
        s.ins = 7; s.eq = 1; s.duck = 1; pr("W .. + FLANGER ins, EQ, DUCK both", &s);
        s.perf = 1; s.space = 100; pr("W .. + PERFORM R16+FLG+PHS+DRV+PUMP, SPACE +100", &s);
    }
    if (strchr(sec, 'E')) {                       /* morph, realistic */
        static const char *PA[][2] = {{"ANALOG", "PHASE"}, {"FM6", "WAVE"}, {"WHEEL", "TRIO"}, {"PHYS", "NOISE"}};
        static const int FD[3] = {0, 64, 127};
        uint32_t q, f;
        for (q = 0; q < 4; q++) {
            uint32_t e1 = eng_named(PA[q][0]), e2 = eng_named(PA[q][1]);
            for (k = 0; k < 2; k++) {             /* sound #0 and #1 */
                s = base(); s.e1 = (int)e1; s.k1 = (int)k; s.n1 = 4; s.e2 = (int)e2; s.k2 = (int)k; s.n2 = 4;
                snprintf(b, sizeof b, "E %s#%u + %s#%u 4+4 LAYER: no morph", PA[q][0], k, PA[q][1], k); pr(b, &s);
                s.morph = 3;
                for (f = 0; f < 3; f++) { g_fader = FD[f]; snprintf(b, sizeof b, "E   factory End B, fader %d", FD[f]); pr(b, &s); }
                s.morph = 1; g_fader = 64;
                g_amt = 15; pr("E   SMART AMOUNT 15 both ends rolled, fader 64", &s);
                g_amt = 40; pr("E   SMART AMOUNT 40, fader 64", &s);
                g_amt = 100; pr("E   SMART AMOUNT 100, fader 64", &s);
                s.morph = 2; g_amt = 40; pr("E   WILD AMOUNT 40, fader 64", &s);
                g_amt = 100; g_fader = 64;
            }
        }
    }
    if (strchr(sec, 'M')) {                       /* which values a factory End B moves (fader 127), and the cost of each alone */
        static const char *PA[][2] = {{"ANALOG", "PHASE"}, {"FM6", "WAVE"}, {"WHEEL", "TRIO"}, {"PHYS", "NOISE"}};
        uint32_t q, pi, id;
        for (q = 0; q < 4; q++) {
            s = base(); s.e1 = (int)eng_named(PA[q][0]); s.k1 = 0; s.n1 = 4; s.e2 = (int)eng_named(PA[q][1]); s.k2 = 0; s.n2 = 4; s.morph = 3; g_fader = 127;
            run(&s);
            for (pi = 0; pi < 2; pi++) {
                printf("M %s#0 End B moves:", PA[q][pi]);
                for (id = 0; id < P_COUNT; id++)
                    if (morph_view(pi, id) != trk[pi].p[id]) printf(" %s(%d>%d)", param_desc_of(trk[pi].engine, id)->label, trk[pi].p[id], (int)morph_view(pi, id));
                printf("\n");
            }
        }
        g_fader = 64;
    }
    if (strchr(sec, 'X')) {                       /* the stages a small move wakes */
        static const char *PA[][2] = {{"ANALOG", "PHASE"}, {"FM6", "WAVE"}, {"WHEEL", "TRIO"}};
        static const int XS[] = {0, 1, 2, 4, 8, 16, 31};
        static const char *XN[] = {"Original", "CUT 82>78 (filter wakes)", "DIST send 0>7", "CHO/DLY/REV sends 0>small", "SPREAD 0>6", "LFO AMP/SHP depth 0>small", "all of these"};
        uint32_t q, x;
        for (q = 0; q < 3; q++)
            for (x = 0; x < 7; x++) {
                s = base(); s.e1 = (int)eng_named(PA[q][0]); s.k1 = 0; s.n1 = 4; s.e2 = (int)eng_named(PA[q][1]); s.k2 = 0; s.n2 = 4;
                g_x = XS[x]; snprintf(b, sizeof b, "X %s#0 + %s#0 4+4: %s", PA[q][0], PA[q][1], XN[x]); pr(b, &s);
            }
        g_x = 0;
    }
    if (strchr(sec, 'K')) {                       /* M04 (0.2.13; M02 before) / M14 / M15: the faded choices (MK_FADE entries whose ends differ) */
        static const uint32_t MS[3] = {3, 13, 14};
        uint32_t m, pi, j;
        for (m = 0; m < 3; m++) {
            power(); load_scene_of(MS[m]);
            for (i = 0; i < 4; i++) half(0);
            for (pi = 0; pi < 2; pi++) {
                const morph_part_t *mp = &mph[pi];
                printf("K M%02u part %u (%s) fades:", MS[m] + 1, pi + 1, ENGINES[trk[pi].engine]->name);
                for (j = 0; j < mp->nid; j++) if ((mp->ent[j].kind & 0x7Fu) == MK_FADE) printf(" %s", param_desc_of(trk[pi].engine, mp->ent[j].id)->label);
                printf("  (entries %u)\n", mp->nid);
            }
        }
    }
    if (strchr(sec, 'O')) {                       /* DUELL 0.2.14 GLO > MODULES: the CPU SENDS OFF and PART FX OFF save (steady state, 4 + 4 notes, fader 63) */
        static const uint32_t MO[6] = {0, 1, 2, 3, 13, 14};   /* M01..M04, M14, M15 */
        static const uint32_t SW[4] = {0, MODS_SENDS, MODS_PFX, MODS_SENDS | MODS_PFX};
        static const char *const SWN[4] = {"all ON", "SENDS OFF", "PART FX OFF", "both OFF"};
        double v[4], pk;
        uint32_t m, q;
        for (m = 0; m < 6; m++) {
            for (q = 0; q < 4; q++) {
                g_mods = SW[q];
                v[q] = multi_run(MO[m], 4, 63, &pk);
            }
            g_mods = 0;
            printf("O M%02u 4+4 notes, fader 63 (%s / %s): %s %5.1f %% | %s %5.1f %% (-%4.1f) | %s %5.1f %% (-%4.1f) | %s %5.1f %% (-%4.1f)\n", MO[m] + 1,
                   ENGINES[trk[0].engine]->name, ENGINES[trk[1].engine]->name, SWN[0], v[0], SWN[1], v[1], v[0] - v[1], SWN[2], v[2], v[0] - v[2],
                   SWN[3], v[3], v[0] - v[3]);
            for (q = 1; q < 4; q++) {
                snprintf(b, sizeof b, "M%02u %s costs no more than all ON (+0.5 %% host noise)", MO[m] + 1, SWN[q]);
                guard(b, v[q] - v[0], 0.5);
            }
        }
        {   /* the bench scene: DUEL, FM6 #2 4 notes + WAVE #0 4 notes, sends on both (SHIM, PING-PONG with modulation), FLANGER insert, EQ, DUCK
             * (W's heaviest without PERFORM); and the idle unit (power-on FX, no note) */
            scn_t bs = base();
            bs.mode = DM_SPLIT; bs.e1 = (int)eng_named("FM6"); bs.k1 = 2; bs.n1 = 4; bs.e2 = (int)eng_named("WAVE"); bs.k2 = 0; bs.n2 = 4;
            bs.sends = 1; bs.rtype = 3; bs.pingmod = 1; bs.ins = 7; bs.eq = 1; bs.duck = 1;
            for (q = 0; q < 4; q++) {
                g_mods = SW[q];
                v[q] = run(&bs);
            }
            g_mods = 0;
            printf("O bench (DUEL FM6#2 4n + WAVE#0 4n, sends SHIM PING mod, FLANGER ins, EQ, DUCK): %s %5.1f %% | %s %5.1f %% (-%4.1f) | %s %5.1f %% (-%4.1f) | %s %5.1f %% (-%4.1f)\n",
                   SWN[0], v[0], SWN[1], v[1], v[0] - v[1], SWN[2], v[2], v[0] - v[2], SWN[3], v[3], v[0] - v[3]);
            for (q = 1; q < 4; q++) {
                snprintf(b, sizeof b, "bench %s costs no more than all ON (+0.5 %% host noise)", SWN[q]);
                guard(b, v[q] - v[0], 0.5);
            }
            bs = base(); bs.e1 = (int)eng_named("ANALOG"); bs.n1 = 0;
            for (q = 0; q < 4; q++) {
                g_mods = SW[q];
                v[q] = run(&bs);
            }
            g_mods = 0;
            printf("O idle (no note, power-on FX, LAYER): %s %5.1f %% | %s %5.1f %% (-%4.1f) | %s %5.1f %% (-%4.1f) | %s %5.1f %% (-%4.1f)\n",
                   SWN[0], v[0], SWN[1], v[1], v[0] - v[1], SWN[2], v[2], v[0] - v[2], SWN[3], v[3], v[0] - v[3]);
        }
    }
    if (strchr(sec, 'F')) {                       /* bursts */
        static const char *EN2[] = {"ANALOG", "FM6", "PHASE", "WHEEL", "PHYS", "NOISE", "WAVE", "TRIO"};
        double st, pk;
        for (i = 0; i < 8; i++) {
            ei = eng_named(EN2[i]);
            s = base(); s.e1 = (int)ei; s.k1 = 0; s.uf = 1;
            pk = burst(&s, 0, &st); printf("F %-7s#0 24HP 8n: retrigger 8 notes in one block: steady %5.1f %% -> worst half %5.1f %%\n", EN2[i], st, pk);
            s.n1 = 2; pk = burst(&s, 1, &st); printf("F %-7s#0 24HP 2n: 64 MIDI note events in one block:   steady %5.1f %% -> worst half %5.1f %%\n", EN2[i], st, pk);
            pk = burst(&s, 2, &st); printf("F %-7s#0 24HP 2n: 63 CC74 in one block:                steady %5.1f %% -> worst half %5.1f %%\n", EN2[i], st, pk);
            fflush(stdout);
        }
    }
    printf(bad ? "cpuscen: %d FAILED\n" : "cpuscen: all passed\n", bad);
    return bad != 0;
}
