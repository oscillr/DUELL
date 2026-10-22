/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.25: the OVERLOAD GUARD (fx.c ovl_step, audio.c shed_check; SPEC 6.3). When the audio is near its limit (the 85 % rule of the voice
 * shedding: two halves in a row over 85 % of their time), the dearest effect fades to its lighter version through its own crossfade: the
 * delay's RANDOM / OCT UP plays as the plain delay (its ~11.6 ms crossfade); after the meter's load (the CPU bar's figure) has stayed under 70 %
 * for ~3 s it comes back the same way. Automatic, always on, nothing stored. (Built for SHIM's PITCH BOTH too, which the owner removed in 0.2.25.)
 * The real audio ISR with a simulated clock (as tests/audio_test.c), the measure forced by the render time it reports (host_us):
 *   MEASURE   one slow half does not engage it, two in a row do; a load between 70 and 85 % holds it (hysteresis); under 70 % it lets go after
 *             3 s (not before), a rise over 70 % on the way starts the 3 s again; single slow halves never engage it
 *   EFFECTS   engaged: RAND and OCT+ run the plain delay (no event voice, the delay's modulation not run in its place), SHIM untouched; released:
 *             the events again. STEREO, LAYER, DUEL (both buses)
 *   FADES     how long the crossfades take (ms) on the way down and back
 *   CLICK     engaged and released in the middle of plucks and of a held pad through RAND and SHIM (MOD 0 and 51, STEREO, LAYER, DUEL): around the
 *             switches no sample step above 1.3 x the largest step of the steady renders (RAND, DIGITAL) + 50, the criterion of the delay modes' own
 *             switch (dlyev_test.c); the difference to the never-engaged render no step above 1.3 x the two steady renders' difference + 50
 *   COST      host instructions a sample of the whole mix with the guard engaged against not, RANDOM (report and a floor)
 *   HASH      renders at a load under the threshold (single slow halves among them) of every scene: printed as HASH lines; DUELL/host_tests.sh builds
 *             this file a second time with -DOVL_NOGUARD (the guard compiled out) and the two lists must be equal: below the threshold the guard
 *             changes nothing, bit for bit
 * Built and run by DUELL/host_tests.sh (name: ovlguard) */
#define main hostsim_main
#include "hostsim.c"
#undef main
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif

#define FM1_AUDIO_HALF 0x80u
#define FM1_TICKS_PER_US 1u
static uint32_t host_clock, host_us, host_nest;
static volatile uint32_t t5_nested_ticks;
static uint32_t fm1_ticks(void) { t5_nested_ticks = host_nest; host_clock += host_us; return host_clock; }
static uint8_t fm1_audio_pending(void) { return FM1_AUDIO_HALF; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static uint32_t fm1_audio_free_half(void) { return 0u; }
static void fm1_audio_ack_half(void) {}
static void fm1_audio_init(int32_t *b, uint32_t n, void (*isr)(void), uint32_t p) { (void)b; (void)n; (void)isr; (void)p; }
void isr_alnk0(void) {}
#define FELUCCA_UAC 1
static void uac_tap(const int32_t *out, uint32_t n) { (void)out; (void)n; }
static void uac_render_start(void) {}
#include "../firmware/src/audio.c"
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_MASTER ? 1023 : 620; }
#include "../firmware/src/master.c"

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    printf("ovlguard: %-118s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        bad++;
    return ok;
}
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}
#define HALF_US (HALF_FRAMES * 1000000u / FS)            /* a half's time: 2902 us */
#define US_OF(pct) (HALF_US * (pct) / 100u)
#define HALVES_OF_MS(ms) ((uint32_t)((ms) * (double)FS / 1000.0 / HALF_FRAMES + 0.5))

/* ------------------------------------------------------------------ the scene --- */
static uint32_t eng_named(const char *n) { uint32_t e; for (e = 0; e < NENGINES; e++) if (str_eq(ENGINES[e]->name, n)) return e; return 0; }
#define MAXS (10u * FS)
static int32_t rec_l[MAXS], rec_r[MAXS];
static uint32_t rec_n;
static uint32_t s_mode, s_blk, s_pad;                  /* s_pad: the next scene is a held pad (PWM PAD under a 300 Hz low-pass: nearly sines, two low notes from the start) instead of the plucks */                          /* the scene's MODE, its block count (the notes' clock) */
/* the DSP from rest in mode, both parts a pluck (ANALOG #8) sent to the delay and the reverb; SHIM at PITCH pitch / MOD mod, delay MODE dm */
static void scene(uint32_t mode, int pitch, int smod, int dm)
{
    uint32_t k;
    memset(trk, 0, sizeof trk);
    memset(&trk[0].mdw, 0, sizeof trk[0].mdw);
    memset(mph, 0, sizeof mph);
    memset(&morph, 0, sizeof morph);
    memset(ins, 0, sizeof ins);
    memset(&song, 0, sizeof song);
    mix_bal = 0;
    vage = 0;
    mod_seed = 0x2545F491u;
    host_tracks_init();
    duell_mode_force(mode == DM_SPLIT ? DM_STEREO : DM_SPLIT);
    duell_mode_force(mode);
    memset(dly_buf, 0, sizeof dly_buf);
    memset(&fx, 0, sizeof fx);
    memset(&fx_b, 0, sizeof fx_b);
    memset(&hl, 0, sizeof hl);
    memset(&hl_b, 0, sizeof hl_b);
    dlyev_clear();
    dly_plain_pp = 0;
    lim_env = lim_env_r = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    shed_req = 0, shed_count = 0, shed_over = 0;
    audio_cpu_rem = 0;
    host_clock = host_nest = 0;
    host_us = US_OF(30u);
#ifndef OVL_NOGUARD
    ovl_lite = 0, ovl_calm = 0, ovl_count = 0;
#endif
    song.g[G_BPM] = 120;
    song.g[G_CLOCK] = 0;
    song.g[G_DTIME] = 1;                                /* 1/8: 250 ms */
    song.g[G_DFDBK] = 60;
    song.g[G_DMIX] = 127;
    song.g[G_DCOLOR] = 90;
    song.g[G_DMDEP] = 71;                               /* (CHANCE in the event modes: M16's) */
    song.g[G_DMODE] = (int16_t)dm;
    song.g[G_RTYPE] = 3;                                /* SHIM */
    fx.rtype = fx_b.rtype = 3;
    song.g[G_RSIZE] = 100;
    song.g[G_RDAMP] = 60;
    song.g[G_SHPITCH] = (int16_t)pitch;
    song.g[G_SHMOD] = (int16_t)smod;
    for (k = 0; k < 2u; k++) {
        if (s_pad) {
            uint32_t e, j;
            for (e = 0; e < NENGINES; e++)
                for (j = 0; j < ds_count(e); j++)
                    if (str_eq(ds_get(e, j)->name, "PWM PAD"))
                        host_ds_load(&trk[k], e, j);
            trk[k].p[P_FREEA] = 30, trk[k].p[P_FREEB] = 0, trk[k].p[P_FREEC] = 0, trk[k].p[P_FREED] = 2;   /* (24 dB low-pass at ~300 Hz: close to two sines, small steps) */
            trk[k].p[P_ED_FLT] = 0;
        } else
            host_preset(&trk[k], eng_named("ANALOG"), 8);
        trk[k].p[P_TRIM] = 100;
        trk[k].p[P_PAN] = 0;
        trk[k].p[P_CHOR] = 0;
        trk[k].p[P_DLY] = 127;
        trk[k].p[P_REV] = 100;
    }
    s_mode = mode, s_blk = 0;
    rec_n = 0;
}
/* the notes of block s_blk: a pluck pair every 0.5 s, 120 ms long; part 2 too in DUEL (its own side) */
static void notes(void)
{
    uint32_t t = s_blk * CTL, sidx = t / (FS / 2u), within = t % (FS / 2u), p, np = s_mode == DM_SPLIT ? 2u : 1u;
    if (s_pad) {
        if (!s_blk)
            for (p = 0; p < np; p++)
                trk_note_on(&trk[p], 48u + 3u * p, 90), trk_note_on(&trk[p], 55u + 3u * p, 90);
        s_blk++;
        return;
    }
    for (p = 0; p < np; p++) {
        uint32_t a = (sidx % 2u ? 55u : 52u) + 3u * p, b = (sidx % 2u ? 62u : 59u) + 3u * p;
        if (within < CTL)
            trk_note_on(&trk[p], a, 100), trk_note_on(&trk[p], b, 100);
        if (within >= 120u * FS / 1000u && within < 120u * FS / 1000u + CTL)
            trk_note_off(&trk[p], a), trk_note_off(&trk[p], b);
    }
    s_blk++;
}
static int32_t ob[2 * CTL];
static void blocks(uint32_t n)                          /* n blocks of mix_block (no ISR), recorded */
{
    uint32_t i;
    while (n--) {
        notes();
        mix_block(ob, CTL);
        for (i = 0; i < CTL && rec_n < MAXS; i++)
            rec_l[rec_n] = ob[2u * i], rec_r[rec_n++] = ob[2u * i + 1u];
    }
}
static uint64_t hh;
static void halves(uint32_t n, uint32_t pct)            /* n halves through the audio ISR, each reported at pct % of its time; the output hashed */
{
    uint32_t i;
    host_us = US_OF(pct);
    while (n--) {
        uint32_t b0 = s_blk;
        for (i = 0; i < HALF_FRAMES / CTL; i++)
            notes();
        s_blk = b0;                                     /* (the ISR renders the blocks; the notes were put in for each before it) */
        (void)b0;
        fm1_alnk0_irq();
        s_blk = b0 + HALF_FRAMES / CTL;
        for (i = 0; i < 2u * HALF_FRAMES; i++)
            hh = hh * 1000003u + (uint32_t)abuf[i];
    }
}
static int32_t step_of(const int32_t *x, uint32_t a, uint32_t b)
{
    int32_t m = 0;
    for (uint32_t i = a + 1u; i < b; i++) {
        int32_t d = x[i] - x[i - 1];
        d = d < 0 ? -d : d;
        m = d > m ? d : m;
    }
    return m;
}
static const uint32_t MODES[3] = {DM_STEREO, DM_LAYER, DM_SPLIT};
static const char *const MNAME[3] = {"STEREO", "LAYER", "DUEL"};

#ifndef OVL_NOGUARD
static void engage(void) { ovl_step(1u, 255u); }      /* the test hook: the load measure as two halves over 85 % report it */
static void release(void) { uint32_t i; for (i = 0; i < OVL_CALM_HALVES; i++) ovl_step(0u, 0u); }   /* .. and 3 s under 70 % */
#endif

int main(void)
{
    uint32_t m, i, k;
    setvbuf(stdout, NULL, _IONBF, 0);

#ifndef OVL_NOGUARD
    {                                                   /* MEASURE: through the ISR */
        uint32_t ok1, ok2, held, n, rel;
        scene(DM_STEREO, SHP_DOWN, 0, DMD_RAND);
        halves(HALVES_OF_MS(500), 30u);
        halves(1, 93u);
        ok1 = !ovl_lite;
        halves(1, 93u);
        ok2 = ovl_lite && ovl_count == 1u;
        expect("MEASURE: one half over 85 % does not engage the guard, the second in a row does (the voice shedding's rule)", ok1 && ok2);
        halves(HALVES_OF_MS(6000), 76u);
        held = ovl_lite;
        expect("  .. 6 s at 76 % (between 70 and 85 %): it stays engaged (hysteresis)", held);
        for (n = 0, rel = 0; n < HALVES_OF_MS(5000) && !rel; n++) {
            halves(1, 20u);
            rel = !ovl_lite;
        }
        printf("ovlguard: MEASURE released %u halves (%.2f s) after the load fell from 76 to 20 %%\n", n, n * (double)HALF_FRAMES / FS);
        expect("  .. at 20 %: released after 3 s under 70 % (the meter's ~93 ms average included: 3.0 .. 3.3 s), not before", rel && n >= OVL_CALM_HALVES && n <= OVL_CALM_HALVES + HALVES_OF_MS(300));
        halves(1, 93u), halves(1, 93u);
        halves(HALVES_OF_MS(2000), 20u);
        halves(HALVES_OF_MS(500), 76u);                 /* the meter's figure over 70 %: the 3 s start again */
        for (n = 0, rel = 0; n < HALVES_OF_MS(5000) && !rel; n++) {
            halves(1, 20u);
            rel = !ovl_lite;
        }
        expect("  .. 2 s calm, then 0.5 s at 76 %, then 20 %: released only 3 s after the 76 % ended (the count starts again)", rel && n >= OVL_CALM_HALVES - HALVES_OF_MS(100) && ovl_count == 2u);
        for (i = 0; i < 40u; i++) {
            halves(1, 97u);
            halves(HALVES_OF_MS(50), 30u);
        }
        expect("  .. single slow halves (97 %) every 50 ms never engage it", !ovl_lite && ovl_count == 2u);
    }

    {                                                   /* EFFECTS and FADES */
        static const int DMS[2] = {DMD_RAND, DMD_OCTUP};
        uint32_t ok_on = 1, ok_off = 1, ok_sh = 1, d, f_dl = 0, b_dl = 0;
        for (m = 0; m < 3u; m++)
            for (d = 0; d < 2u; d++) {
                uint32_t e0, fd_ = 0, bd_ = 0;
                scene(MODES[m], SHP_DOWN, 30, DMS[d]);
                blocks(FS / CTL);
                ok_on &= dlyev.x > 0;
                engage();
                e0 = rec_n;
                for (k = 0; k < 2000u && dlyev.x; k++) {
                    blocks(1);
                    if (!dlyev.x && !fd_)
                        fd_ = rec_n - e0;
                }
                blocks(4);
                ok_on &= !dlyev.x && (MODES[m] == DM_SPLIT || !dly_side);
                if (MODES[m] == DM_SPLIT)
                    ok_on &= !dlyev_b.x;
                ok_sh &= fx.sh_x == 1u && fx.sh_cfg == SHP_DOWN && fx.sh_g == 32767;   /* (SHIM is not the guard's: it plays on as set) */
                release();
                e0 = rec_n;
                for (k = 0; k < 2000u && dlyev.x < 32767; k++) {
                    blocks(1);
                    if (dlyev.x >= 32767 && !bd_)
                        bd_ = rec_n - e0;
                }
                ok_off &= dlyev.x > 0;
                if (MODES[m] == DM_SPLIT)
                    ok_off &= dlyev_b.x > 0;
                f_dl = fd_ > f_dl ? fd_ : f_dl, b_dl = bd_ > b_dl ? bd_ : b_dl;
            }
        expect("EFFECTS: engaged, RAND / OCT+ run as the plain delay (no event voice); STEREO, LAYER, DUEL (both buses)", ok_on);
        expect("  .. SHIM (PITCH DOWN, MOD 30 here) is not touched by the guard", ok_sh);
        expect("  .. released: the events again, faded in", ok_off);
        printf("ovlguard: FADES (the slowest of the scenes, blocks of 0.73 ms) engaged: the events out in %.1f ms; released: back in full after %.1f ms\n",
               f_dl * 1000.0 / FS, b_dl * 1000.0 / FS);
        expect("  .. the crossfade: the delay's own ~11.6 ms, either way", f_dl >= FS / 100u && f_dl <= FS * 13u / 1000u && b_dl >= FS / 100u && b_dl <= FS * 13u / 1000u);
    }

    {                                                   /* CLICK */
        static const int MODS[2] = {0, 51};
        static int32_t ra[2][MAXS], rb[2][MAXS];
        static const char *const KN[2] = {"plucks", "a held pad"};
        uint32_t mo, c, nr, kd, at[4];
        for (kd = 0; kd < 2u; kd++)
        for (m = 0; m < 3u; m++)
            for (mo = 0; mo < 2u; mo++) {
                int32_t base = 0, sw = 0, dab = 0, dsa = 0, v;
                s_pad = kd;
                for (k = 0; k < 2u; k++) {             /* steady: RAND (never engaged), then DIGITAL (what the guard plays); SHIM DOWN in both */
                    scene(MODES[m], SHP_DOWN, MODS[mo], k ? DMD_DIGITAL : DMD_RAND);
                    if (k)
                        song.g[G_DMDEP] = 0;
                    blocks(7u * FS / CTL);
                    for (c = 0; c < 2u; c++) {
                        memcpy(k ? rb[c] : ra[c], c ? rec_r : rec_l, rec_n * sizeof rec_l[0]);
                        v = step_of(c ? rec_r : rec_l, 0, rec_n);
                        base = v > base ? v : base;
                    }
                }
                nr = rec_n;
                scene(MODES[m], SHP_DOWN, MODS[mo], DMD_RAND);
                blocks(FS / CTL + 17u);                 /* (engaged in the middle of a pluck's ring) */
                at[0] = rec_n, engage();
                blocks(2u * FS / CTL);
                at[1] = rec_n, release();
                blocks(FS / CTL + 5u);
                at[2] = rec_n, engage();
                blocks(FS / CTL);
                at[3] = rec_n, release();
                blocks(2u * FS / CTL - 22u);
                for (c = 0; c < 2u; c++) {
                    const int32_t *x = c ? rec_r : rec_l;
                    uint32_t w;
                    for (w = 0; w < 4u; w++) {          /* around each switch: from 5 ms before to 40 ms after (the fades) */
                        v = step_of(x, at[w] - FS / 200u, at[w] + FS / 25u);
                        sw = v > sw ? v : sw;
                    }
                    for (i = 1; i < nr && i < rec_n; i++) {   /* the steps of the differences: switched - never, and the two steady renders' */
                        int32_t d1 = (x[i] - ra[c][i]) - (x[i - 1] - ra[c][i - 1]), d2 = (rb[c][i] - ra[c][i]) - (rb[c][i - 1] - ra[c][i - 1]);
                        d1 = d1 < 0 ? -d1 : d1, d2 = d2 < 0 ? -d2 : d2;
                        dsa = d1 > dsa ? d1 : dsa, dab = d2 > dab ? d2 : dab;
                    }
                }
                printf("ovlguard: CLICK %s, %s, MOD %d: engaged and released twice: largest step around the switches %d (steady renders, anywhere: %d); the difference to the never-engaged render steps at most %d (the two steady renders' difference: %d)\n",
                       KN[kd], MNAME[m], MODS[mo], sw, base, dsa, dab);
                expect("CLICK: no step around the switches over 1.3 x the steady renders' + 50, and the change itself no step over 1.3 x the steady difference's + 50",
                       sw < 13 * base / 10 + 50 && dsa < 13 * dab / 10 + 50);
            }
        s_pad = 0;
    }

    {                                                   /* COST: the whole mix, guard on against off (RANDOM; SHIM DOWN MOD 51 as a heavy bus beside it) */
        for (m = 0; m < 3u; m += 2u) {
            double c[2];
            for (k = 0; k < 2u; k++) {
                uint64_t i0;
                scene(MODES[m], SHP_DOWN, 51, DMD_RAND);
                blocks(FS / CTL);
                if (k)
                    engage();
                blocks(FS / 10u / CTL);
                i0 = instr_now();
                blocks(2u * FS / CTL);
                c[k] = (double)(instr_now() - i0) / (2.0 * FS);
            }
            printf("ovlguard: COST %s, RANDOM (SHIM DOWN MOD 51 beside it), a plucked pair: %.0f host instructions a sample, %.0f with the guard engaged (%.1f %% of the half saved, 33 = 1 %%)\n",
                   MNAME[m], c[0], c[1], (c[0] - c[1]) / 33.0);
            if (c[0] > 0)
                expect(m ? "COST DUEL: the guard saves at least 4 % of the half (host model)" : "COST STEREO: the guard saves at least 2 % of the half (host model)", c[0] - c[1] > (m ? 4.0 : 2.0) * 33.0);
        }
    }
#endif

    {                                                   /* HASH: under the threshold, bit for bit with the guard compiled out */
        static const int SC[6][3] = {{2, 0, DMD_RAND}, {2, 51, DMD_RAND}, {SHP_DOWN, 51, DMD_OCTUP}, {SHP_UP, 0, DMD_RAND},   /* (2: a stored BOTH, which plays UP) */
                                     {SHP_DOWN, 30, DMD_PING}, {SHP_UP, 51, DMD_DIGITAL}};
        for (m = 0; m < 3u; m++)
            for (k = 0; k < 6u; k++) {
                scene(MODES[m], SC[k][0], SC[k][1], SC[k][2]);
                hh = 1469598103934665603ull;
                for (i = 0; i < 25u; i++) {
                    halves(HALVES_OF_MS(100), 60u + i % 20u);   /* 60 .. 79 % */
                    halves(1, 97u);                            /* (a single slow half: never two in a row) */
                }
                printf("HASH %s %d/%d/%d %016llx\n", MNAME[m], SC[k][0], SC[k][1], SC[k][2], (unsigned long long)hh);
#ifndef OVL_NOGUARD
                if (ovl_count)
                    expect("HASH: the guard did not engage in the hash renders", 0);
#endif
            }
    }
    printf("ovlguard: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
