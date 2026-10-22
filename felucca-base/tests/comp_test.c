/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.21 COMP, the master compressor (fx.c comp_stage; params.c page COMP; core.h G_CT .. G_CL / song.cp / cmp_set; stored in the settings record's retired `rsv` word):
 * on the firmware's sources (the DSP of hostsim.c and the UI of ui_test.c), against simulated NOR flash.
 *   build/host/comp_test             (host_tests.sh comp)
 * 1. RATIO 1:1 is bit for bit what it was: a busy two-part render (mode_scn.h, the process's first render) hashes to MODE_STEREO_HASH, the hash the sources before COMP gave; the other
 *    settings changed at 1:1 change nothing; a render that had a ratio on and then 1:1 is, after the 15 ms fade, sample for sample the render that never had it. Zero cost: the stage is not called.
 * 2. the static curve: steady 1 kHz tones at many levels, 8 ratios x 5 thresholds, against the ideal curve (6 dB soft knee, the automatic makeup) within 0.1 dB; the makeup rule (unity at
 *    -11 dB, at most +12 dB); a sweep of frequencies.
 * 3. the times: ATTACK (19 settings, 0.1 ms .. 100 ms) and RELEASE (13, 20 ms .. 2 s) measured on the detector's step response (a DC step; the release also on a tone) against the printed times, within 15 %.
 * 4. the stereo link (an L-only loud tone lowers R by the same gain), silence (exact zeros, the detector falls to nothing, the gain returns to the makeup), no overshoot (loud bursts through the stage and the
 *    master stays inside the limiter; at a level of -11 dB and above the stage never makes a steady tone louder), a clamp for off-scale input.
 * 5. no click: RATIO 1:1 <-> a ratio while a tone sounds, the ratio / threshold / times walked, never a step faster than the tone's own slope.
 * 6. DUEL (SPLIT): the stage does not run (the render with COMP set is bit for bit the one without), STEREO and LAYER run it; a change of the mode clears its state.
 * 7. the control: the page (GLO family, after BASS MONO, title COMP), the cards, their names and values, the hide rules (DUEL; 1:1), the knobs, the stored word (rsv; the record's size and magic as ever),
 *    power-on, ERASE DATA, every byte value of the word read safely, an older record (rsv 0, 1, 2) = the defaults, not in the multis / project, the stage's place in mix_block (after PERFORM and BASS MONO, before MASTER).
 * 8. cost, host instructions per sample. */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"
#include "mode_scn.h"
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif

#define MODE_STEREO_HASH 0xC04494F53D46F0F0ull   /* 0.2.30: merged line (cloud wave 6 + click fixes), re-based on purpose, was 0x1CC6C8241C84C0DA; before:  0.2.29: re-based on purpose, the scenario runs the HALL and its quiet lines now round to the nearest (fx.c hall_end; 0.2.21..0.2.28: 0xC970174114DFD9BA) */   /* the same constant as mode_test.c and bassmono_test.c (0.2.21: re-based with the STEREO CHORUS; the sources before COMP gave 0xA77A7B40656EB88D; mode_test.c holds the hash of the sources without COMP but with the stereo chorus, the same) */

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if ((off & 0xFFu) + n > 256u)
        return -1;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    printf("comp: %-124s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
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
static void tails_over(void)
{
    uint32_t k, i;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NVOICE; i++)
            trk[k].v[i].active = 0;
}
static void boot(void)                                  /* main.c's start: the RAM gone, the flash read, the settings */
{
    ui_power_on();
    tails_over();
    memset(&settings, 0, sizeof settings);
    first_boot_pending = 0;
    memset(&persist_saved, 0, sizeof persist_saved);
    persist_pending = 0;
    cmp_set = 0;                                        /* (the RAM is gone) */
    cmp_last = 0xFFFFFFFFu;
    memset(song.cp, 0, sizeof song.cp);
    flash_ok = 1;
    persist_boot();
    settings_init();
    multi_boot();
    frame();
}
static void flush(void)                                 /* the settings written (settings_poll, nothing sounding) */
{
    uint32_t n;
    tails_over();
    for (n = 0; n < 8u; n++) {
        fm1_ms += 2000u;
        settings_poll();
    }
}

#define SEC FS
#define CFG(t, r, a, l) ((uint32_t)(t) | (uint32_t)(r) << 8 | (uint32_t)(a) << 16 | (uint32_t)(l) << 24)
static uint32_t xs = 0x2545F491u;
static int32_t noise(int32_t amp)
{
    xs ^= xs << 13;
    xs ^= xs >> 17;
    xs ^= xs << 5;
    return (int32_t)(((int64_t)(int32_t)xs * amp) / 2147483648LL);
}
static void stage_rest(uint32_t cfg)                    /* the stage at rest, setting cfg */
{
    memset(&cmp, 0, sizeof cmp);
    memset(wet, 0, sizeof wet);
    cmp_cfg = cfg;
}
/* one block through the stage as mix_block gates it (not in DUEL: the caller's case); g_poff: PERFORM's MASTER correction (0 when PERFORM is idle) */
static int32_t g_poff;
static void stage_block(uint32_t n)
{
    if ((cmp_cfg & 0xF00u) || cmp.a)
        comp_stage(n, g_poff);
}
static double db(double a, double b) { return 20.0 * log10(a / b); }

/* ------------------------------------------------------------------ the ideal curve --- */
#define KNEE_DB 6.0205999
#define REF_DB -8.0
#define FSC 65536.0                                    /* 0 dB: the output's full scale at the default MASTER, pre-MASTER 65536 */
static double ideal_gr(double over, double slope)       /* dB of gain reduction at `over` dB above the threshold */
{
    if (2.0 * over <= -KNEE_DB)
        return 0;
    if (2.0 * over >= KNEE_DB)
        return slope * over;
    return slope * (over + KNEE_DB / 2.0) * (over + KNEE_DB / 2.0) / (2.0 * KNEE_DB);
}
static const double RATIO_X[9] = {1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 1e9};
static double ideal_makeup(double thr_db, double slope)
{
    double m = ideal_gr(REF_DB - thr_db, slope);
    return m > 12.0 ? 12.0 : m;                         /* (the cap is 510/256 octaves = 11.995 dB) */
}
static double ideal_gain_db(double in_db, double thr_db, uint32_t rat)
{
    double slope = 1.0 - 1.0 / RATIO_X[rat];
    return ideal_makeup(thr_db, slope) - ideal_gr(in_db - thr_db, slope);
}

/* a steady tone of f Hz, amplitude amp (left; right ra), through the stage for `settle` + `meas` samples; returns the gain (dB) of the last `meas` samples (output peak / input peak) */
typedef struct { double gl, gr, pin_l, pout_l, pin_r, pout_r; } tone_t;
static tone_t tone_run(uint32_t cfg, double f, double amp, double ramp, uint32_t settle, uint32_t meas)
{
    uint32_t i, k, tot = settle + meas;
    double il = 0, ol = 0, ir = 0, or_ = 0;
    tone_t o;
    stage_rest(cfg);
    for (i = 0; i < tot; i += CTL) {
        for (k = 0; k < CTL; k++) {
            double s = sin(6.283185307179586 * f * (double)(i + k) / (double)FS);
            mix_l[k] = (int32_t)lrint(amp * s);
            mix_r[k] = (int32_t)lrint(ramp * s);
        }
        if (i >= settle) {
            for (k = 0; k < CTL; k++) {
                double a = fabs((double)mix_l[k]), b = fabs((double)mix_r[k]);
                il = a > il ? a : il;
                ir = b > ir ? b : ir;
            }
        }
        stage_block(CTL);
        if (i >= settle)
            for (k = 0; k < CTL; k++) {
                double a = fabs((double)mix_l[k]), b = fabs((double)mix_r[k]);
                ol = a > ol ? a : ol;
                or_ = b > or_ ? b : or_;
            }
    }
    o.pin_l = il, o.pout_l = ol, o.pin_r = ir, o.pout_r = or_;
    o.gl = il > 0 && ol > 0 ? db(ol, il) : -999.0;
    o.gr = ir > 0 && or_ > 0 ? db(or_, ir) : -999.0;
    return o;
}

static void test_curve(void)
{
    static const int THR_IDX[5] = {0, 10, 23, 33, 40};
    static const double LV[] = {-45, -40, -34, -28, -22, -17, -14, -11, -8, -5, -2, -0.2};
    uint32_t ti, r, li, nruns = 0;
    double worst = 0, wthr = 0, wlv = 0;
    uint32_t wr = 0;
    printf("comp: the static curve (1 kHz tones, attack 5 ms, release 200 ms, against the ideal curve with a 6 dB soft knee and the automatic makeup)\n");
    for (ti = 0; ti < 5u; ti++)
        for (r = 1; r < 9u; r++)
            for (li = 0; li < sizeof LV / sizeof LV[0]; li++) {
                double thr_db = THR_IDX[ti] - 40.0, amp = FSC * pow(10.0, LV[li] / 20.0), in_db, want, err, tol;
                tone_t t = tone_run(CFG(THR_IDX[ti], r, 10, 6), 1000.0, amp, 0, 6u * SEC / 10u, SEC / 10u);
                in_db = db(t.pin_l, FSC);
                want = ideal_gain_db(in_db, thr_db, r);
                err = fabs(t.gl - want);
                tol = 0.1 + 8.7 / t.pout_l;             /* (the output's own rounding: one count of its peak) */
                nruns++;
                if (err - tol > worst)
                    worst = err - tol, wr = r, wthr = thr_db, wlv = LV[li];
                if (err > tol) {
                    printf("comp:   miss: thr %.0f ratio %.1f in %.2f dB: gain %.3f want %.3f (err %.3f)\n", thr_db, RATIO_X[r], in_db, t.gl, want, err);
                    bad++;
                }
            }
    if (worst > 0)
        printf("comp:   worst miss beyond the tolerance: ratio %.1f thr %.0f level %.1f by %.3f dB\n", RATIO_X[wr], wthr, wlv, worst);
    ncheck++;
    printf("comp: %-124s %s\n", "the gain of 480 steady tones (8 ratios x 5 thresholds x 12 levels, -45 .. -0.2 dB) is within 0.1 dB of the ideal curve (soft knee, makeup)", worst <= 0 ? "ok" : "FAIL");
    printf("comp:   (%u runs)\n", nruns);
    {   /* the makeup rule: unity at -8 dB for thresholds under it (4:1, thr -17: +6.75 dB below the knee), capped at +12 dB */
        tone_t t = tone_run(CFG(23, 4, 10, 6), 1000.0, FSC * pow(10.0, -8.0 / 20.0), 0, SEC / 2u, SEC / 10u);
        double g8 = t.gl;
        int ok = fabs(t.gl) < 0.1;
        t = tone_run(CFG(23, 4, 10, 6), 1000.0, FSC * pow(10.0, -50.0 / 20.0), 0, SEC / 2u, SEC / 10u);
        ok &= fabs(t.gl - ideal_makeup(-17.0, 0.75)) < 0.1 && t.gl > 6.5 && t.gl < 7.0;
        t = tone_run(CFG(0, 8, 10, 6), 1000.0, FSC * pow(10.0, -50.0 / 20.0), 0, SEC / 2u, SEC / 10u);
        ok &= fabs(t.gl - 11.995) < 0.1;
        printf("comp:   (4:1 thr -17: -8 dB passes at %.2f dB, -50 dB is raised by %.2f dB; INF thr -40: the makeup caps at %.2f dB)\n", g8, ideal_makeup(-17.0, 0.75), t.gl);
        expect("the makeup rule: the curve's own gain reduction at -8 dB (unity there), quiet signals raised by it, at most +12 dB", ok);
    }
    {   /* a loud steady tone is never made louder than it came in (the makeup is the reduction at -11 dB, the curve takes more above it) */
        int ok = 1;
        uint32_t k, rr;
        for (rr = 1; rr < 9u; rr++)
            for (k = 0; k < 5u; k++) {
                tone_t t = tone_run(CFG(THR_IDX[k], rr, 10, 6), 1000.0, 36000.0, 0, SEC / 2u, SEC / 10u);   /* -5.2 dB: the limiter's threshold (18000 after MASTER at its default 0.5) */
                ok &= t.gl <= 0.1;
            }
        expect("at the limiter's threshold (-5.2 dB: 36000 before MASTER, 18000 after it at the default) a steady tone is never louder out than in, any ratio or threshold", ok);
    }
    {   /* PERFORM: its master stage has put MASTER's gain (mg / 1024) into the mix before the stage; the correction poff makes THRESH the same whatever the knob */
        static const int32_t MG[5] = {256, 1024, 2048, 3100, 4088};
        double lo = 99, hi = -99;
        uint32_t k;
        for (k = 0; k < 5u; k++) {
            tone_t t;
            g_poff = k ? cmp_poff(MG[k]) : 0;
            t = tone_run(CFG(23, 4, 10, 6), 1000.0, k ? 30000.0 * MG[k] / 1024.0 : 30000.0, 0, SEC / 2u, SEC / 10u);
            lo = t.gl < lo ? t.gl : lo;
            hi = t.gl > hi ? t.gl : hi;
        }
        g_poff = 0;
        printf("comp:   (PERFORM: 4:1 thr -17 on 30000 pre-MASTER, the mix scaled by mg / 1024 for mg 256 .. 4088: gains %.3f .. %.3f dB)\n", lo, hi);
        expect("PERFORM's stage has applied MASTER (mg / 1024): with the correction poff the gain is the same at MASTER 256, 1024, 2048, 3100 and 4088 as with PERFORM idle (within 0.1 dB)", hi - lo < 0.1);
    }
    {   /* frequencies: the same level, 100 Hz .. 10 kHz (the hold is a block of 512 samples: a tone from 87 Hz up has no ripple) */
        static const double F[] = {100, 250, 1000, 4000, 10000};
        double lo = 99, hi = -99;
        uint32_t k;
        for (k = 0; k < 5u; k++) {
            tone_t t = tone_run(CFG(23, 4, 10, 6), F[k], 20000.0, 0, SEC / 2u, SEC / 4u);
            lo = t.gl < lo ? t.gl : lo;
            hi = t.gl > hi ? t.gl : hi;
        }
        printf("comp:   (4:1 thr -17, 20000 peak, 100 Hz .. 10 kHz: gains %.3f .. %.3f dB)\n", lo, hi);
        expect("the same tone at 100 Hz, 250 Hz, 1 kHz, 4 kHz, 10 kHz: gains within 0.15 dB of each other", hi - lo < 0.15);
    }
    {
        tone_t t = tone_run(CFG(23, 4, 10, 6), 50.0, 20000.0, 0, SEC, SEC / 2u);
        printf("comp:   (info: 50 Hz at 20000: gain %.2f dB, a bass below the hold's 87 Hz ripples the gain a little)\n", t.gl);
    }
}

/* ------------------------------------------------------------------ the times --- */
/* the detector's response (cmp.pk, Q12) to a DC step, sampled every hop of 8: n = 8 calls. Returns the time (samples) of the 63 % point from the step. */
static double dc_block_run(int32_t v, uint32_t nblk)
{
    uint32_t b, k;
    for (b = 0; b < nblk; b++) {
        for (k = 0; k < 8u; k++)
            mix_l[k] = mix_r[k] = v;
        comp_stage(8, 0);
    }
    return cmp.pk / 4096.0;
}
static double attack_tau(uint32_t ai)
{
    double p0, p, tgt = 30000.0;
    uint32_t b, k;
    stage_rest(CFG(0, 2, ai, 6));
    p0 = dc_block_run(3000, 700u);
    for (b = 1; b < 200000u; b++) {
        for (k = 0; k < 8u; k++)
            mix_l[k] = mix_r[k] = 30000;
        comp_stage(8, 0);
        p = cmp.pk / 4096.0;
        if ((p - p0) / (tgt - p0) >= 0.3)               /* the first hop past 30 %: tau = -t / ln(1 - fraction) (exact for the one-pole, whatever the hop) */
            return -(8.0 * b) / log(1.0 - (p - p0) / (tgt - p0));
    }
    return -1;
}
static double release_t63(uint32_t ri, uint32_t phase, int tone)
{
    double p0, p, prev, tgt = 3000.0, thr;
    uint32_t b, k, t = 0;
    stage_rest(CFG(0, 2, 0, ri));
    for (b = 0; b < 4000u + (ri > 8u ? 12000u : 0u); b++) {   /* settle at 30000 */
        for (k = 0; k < 8u; k++, t++)
            mix_l[k] = mix_r[k] = tone ? (int32_t)lrint(30000.0 * sin(6.283185307179586 * 1000.0 * t / FS)) : 30000;
        comp_stage(8, 0);
    }
    for (b = 0; b < phase; b += 1u) {                   /* the grid of 64: the drop at every 8th hop of a block */
        for (k = 0; k < 8u; k++, t++)
            mix_l[k] = mix_r[k] = tone ? (int32_t)lrint(30000.0 * sin(6.283185307179586 * 1000.0 * t / FS)) : 30000;
        comp_stage(8, 0);
    }
    p0 = prev = cmp.pk / 4096.0;
    thr = p0 + 0.632 * (tgt - p0);
    for (b = 1; b < 3000000u; b++) {
        for (k = 0; k < 8u; k++, t++)
            mix_l[k] = mix_r[k] = tone ? (int32_t)lrint(3000.0 * sin(6.283185307179586 * 1000.0 * t / FS)) : 3000;
        comp_stage(8, 0);
        p = cmp.pk / 4096.0;
        if (p <= thr)
            return 8.0 * (b - 1) + 8.0 * (prev - thr) / (prev - p);
        prev = p;
    }
    return -1;
}
static void test_times(void)
{
    uint32_t i, ph, nbad = 0;
    double worst_a = 0, worst_r = 0, worst_t = 0;
    printf("comp: the times (the detector's step response; ATTACK up, RELEASE down, 63 %% of the step)\n");
    for (i = 0; i < CMP_ATK_N; i++) {
        double tau = attack_tau(i), want = CMP_ATK_US(i) * 1e-6 * FS, e = tau / want - 1.0;
        printf("comp:   ATTACK %7.2f ms: measured %8.3f ms (%+5.1f %%)\n", CMP_ATK_US(i) / 1000.0, tau / FS * 1000.0, e * 100.0);
        if (fabs(e) > fabs(worst_a))
            worst_a = e;
        nbad += fabs(e) > 0.15;
    }
    expect("ATTACK, all 121 settings (0.1 ms .. 100 ms, 40 a decade): the time to 63 % of an upward step is within 15 % of the printed time", nbad == 0);
    printf("comp:   (worst %+.1f %%)\n", worst_a * 100.0);
    nbad = 0;
    for (i = 0; i < CMP_REL_N; i++) {
        double sum = 0, want = CMP_REL_MS(i) * 1e-3 * FS, e, mn = 1e12, mx = 0;
        for (ph = 0; ph < 8u; ph++) {
            double t = release_t63(i, ph, 0);
            sum += t, mn = t < mn ? t : mn, mx = t > mx ? t : mx;
        }
        e = sum / 8.0 / want - 1.0;
        printf("comp:   RELEASE %5u ms: measured %8.2f ms (%+5.1f %%; the drop's place in the hold grid moves it %.2f .. %.2f ms)\n", CMP_REL_MS(i), sum / 8.0 / FS * 1000.0, e * 100.0, mn / FS * 1000.0, mx / FS * 1000.0);
        if (fabs(e) > fabs(worst_r))
            worst_r = e;
        nbad += fabs(e) > 0.15 || mx / want - 1.0 > 0.2 || mn / want - 1.0 < -0.2;
    }
    expect("RELEASE, all 81 settings (20 ms .. 2 s, 40 a decade): the time to 63 % of a downward step is within 15 % of the printed time (and within 20 % at any place in the hold's grid)", nbad == 0);
    printf("comp:   (worst %+.1f %%)\n", worst_r * 100.0);
    nbad = 0;
    for (i = 22; i < CMP_REL_N; i += 3) {                /* the same on a 1 kHz tone (its peaks are the hold's input) from 70 ms up */
        double t = release_t63(i, 3, 1), want = CMP_REL_MS(i) * 1e-3 * FS, e = t / want - 1.0;
        if (fabs(e) > fabs(worst_t))
            worst_t = e;
        nbad += fabs(e) > 0.15;
    }
    printf("comp:   (RELEASE 70 ms .. 2 s on a 1 kHz tone: worst %+.1f %%)\n", worst_t * 100.0);
    expect("RELEASE on a 1 kHz tone, 70 ms .. 2 s: within 15 % too", nbad == 0);
    {   /* 0.2.24: the defaults sound as before: the coefficients of ATTACK 10 ms and RELEASE 200 ms against the old table's {147, 13} and {253, 18} (m / 2^sh of a hop), within 1 % */
        double a_old = 147.0 / 8192.0, r_old = 253.0 / 262144.0, a_new = cmp_atk_coef(CMP_ATK_DEF) / 16777216.0, r_new = cmp_rel_coef(CMP_REL_DEF) / 16777216.0;
        printf("comp:   defaults: ATTACK k %.6f (was %.6f), RELEASE k %.7f (was %.7f)\n", a_new, a_old, r_new, r_old);
        expect("the default ATTACK (10 ms) and RELEASE (200 ms) coefficients are the old table's within 1 %", fabs(a_new / a_old - 1.0) < 0.01 && fabs(r_new / r_old - 1.0) < 0.01);
    }
    {   /* the gain follows: after a loud step the gain reduction settles (no drift), and comes back after the release */
        tone_t t;
        stage_rest(CFG(23, 4, 12, 4));
        t = tone_run(CFG(23, 4, 12, 4), 1000.0, 3000.0, 0, SEC / 2u, SEC / 20u);
        expect("a tone below the threshold keeps the makeup gain (no reduction)", fabs(t.gl - ideal_makeup(-17.0, 0.75)) < 0.1);
    }
}

/* ------------------------------------------------------------------ link, silence, overshoot --- */
static void test_link(void)
{
    tone_t a, b, c;
    uint32_t i, k, z = 0, m;
    int ok;
    printf("comp: the stereo link, silence, overshoot\n");
    a = tone_run(CFG(23, 4, 10, 6), 1000.0, 40000.0, 0.0, SEC / 2u, SEC / 10u);
    b = tone_run(CFG(23, 4, 10, 6), 1000.0, 40000.0, 2000.0, SEC / 2u, SEC / 10u);   /* R: a quiet tone (-30 dB) beside a loud L (-4.3 dB) */
    c = tone_run(CFG(23, 4, 10, 6), 1000.0, 0.0, 2000.0, SEC / 2u, SEC / 10u);      /* R alone: it is below the threshold (the makeup) */
    printf("comp:   L only 40000: L gain %.3f dB; L 40000 + R 2000: L %.3f, R %.3f dB; R 2000 alone: %.3f dB\n", a.gl, b.gl, b.gr, c.gr);
    expect("stereo link: an L-only loud tone and a quiet R: both channels get the same gain (within 0.02 dB), and it is the loud channel's", fabs(b.gl - b.gr) < 0.02 && fabs(a.gl - b.gl) < 0.02);
    expect("  .. the same quiet R alone is raised by the makeup instead (the loud L is what reduces it)", c.gr > 6.0 && c.gr - b.gr > 6.0);
    {   /* an R-only loud signal lowers L alike */
        tone_t d = tone_run(CFG(23, 4, 10, 6), 1000.0, 2000.0, 40000.0, SEC / 2u, SEC / 10u);
        expect("  .. and an R-only loud tone lowers a quiet L equally", fabs(d.gl - d.gr) < 0.02 && d.gl < 0.0);
    }
    /* silence */
    stage_rest(CFG(0, 4, 10, 8));                       /* thr -40, 4:1, release 500 ms */
    for (i = 0; i < 2u * SEC / CTL; i++) {
        for (k = 0; k < CTL; k++)
            mix_l[k] = noise(20000), mix_r[k] = noise(20000);
        stage_block(CTL);
    }
    for (i = 0; i < 12u * SEC / CTL; i++) {
        memset(mix_l, 0, sizeof mix_l);
        memset(mix_r, 0, sizeof mix_r);
        stage_block(CTL);
    }
    for (k = 0; k < CTL; k++)
        z |= (uint32_t)(mix_l[k] | mix_r[k]);
    expect("after the sound stops (12 s of zeros) the output is exactly 0, the detector has fallen to under 4 (of 32768): it does not stall", z == 0u && (cmp.pk >> 12) < 4);
    {   /* thr -40: a tail at -50 dB afterwards is raised by the makeup, not held down by a stuck gain reduction */
        double lo = 1e9;
        stage_rest(CFG(0, 4, 10, 8));
        for (i = 0; i < 2u * SEC / CTL; i++) {
            for (k = 0; k < CTL; k++)
                mix_l[k] = mix_r[k] = noise(20000);
            stage_block(CTL);
        }
        for (i = 0; i < 20u * SEC / CTL; i++) {
            for (k = 0; k < CTL; k++)
                mix_l[k] = mix_r[k] = (int32_t)lrint(100.0 * sin(6.283185307179586 * 1000.0 * (double)(i * CTL + k) / FS));
            stage_block(CTL);
        }
        {
            double mx = 0;
            for (k = 0; k < CTL; k++)
                mx = fabs((double)mix_l[k]) > mx ? fabs((double)mix_l[k]) : mx;
            lo = mx;
        }
        printf("comp:   (thr -40, 4:1: a 100-peak tone 20 s after a loud noise: gain tracks to %.2f of its input; the makeup alone would be %.2f)\n", lo / 100.0, pow(10.0, ideal_makeup(-40.0, 0.75) / 20.0));
        expect("  .. a quiet tone 20 s after a loud noise comes out at the makeup gain (the release brings the reduction back to nothing)", fabs(db(lo, 100.0) - ideal_makeup(-40.0, 0.75)) < 0.5);
    }
    /* overshoot: loud bursts through the stage and the master */
    ok = 1;
    m = 0;
    {
        uint32_t r, cfgs[4];
        cfgs[0] = CFG(0, 8, 18, 12);                    /* INF, thr -40, attack 100 ms: the worst for a burst (the makeup +12 dB at the first sound) */
        cfgs[1] = CFG(40, 2, 0, 0);
        cfgs[2] = CFG(23, 4, 9, 6);
        cfgs[3] = CFG(10, 6, 12, 8);
        for (r = 0; r < 4u; r++) {
            int32_t big = 0;
            stage_rest(cfgs[r]);
            lim_env = LIM_T;
            dc_l = dc_r = dce_l = dce_r = 0;
            for (i = 0; i < 3u * SEC / CTL; i++) {
                int quiet = (i / (SEC / 4u / CTL)) & 1u;   /* quiet / loud every 250 ms */
                for (k = 0; k < CTL; k++) {
                    double s = sin(6.283185307179586 * 220.0 * (double)(i * CTL + k) / FS);
                    int32_t a = (int32_t)lrint((quiet ? 300.0 : 30000.0) * s) + noise(quiet ? 20 : 2000);
                    int32_t l = a, rr = a / 2;
                    mix_l[k] = l, mix_r[k] = rr;
                }
                stage_block(CTL);
                for (k = 0; k < CTL; k++) {
                    int32_t l = mix_l[k], rr = mix_r[k];
                    big = l > big ? l : big;
                    if (cmp_cfg & 0xF00u)
                        m = (uint32_t)(l > 0 ? l : -l) > m ? (uint32_t)(l > 0 ? l : -l) : m;
                    master_out(&l, &rr);
                    ok &= l >= -32767 && l <= 32767 && rr >= -32767 && rr <= 32767;
                }
            }
            printf("comp:   (setting %u: the stage's largest output sample %d for an input of 30000 + noise; through the master all within +-32767)\n", r, (int)big);
            ok &= big <= 4 * 33000;                     /* (the makeup is at most +12 dB: x3.98) */
        }
    }
    expect("loud bursts after quiet (slow attack, the largest makeup): the stage never gives more than 4 x the input, and the limiter and soft clip keep the master inside +-32767", ok);
    {   /* an off-scale input (far beyond 8 x full scale): no overflow, a bounded output */
        int32_t mx = 0;
        stage_rest(CFG(10, 4, 0, 0));
        for (i = 0; i < 200u; i++) {
            for (k = 0; k < CTL; k++)
                mix_l[k] = noise(1500000), mix_r[k] = noise(1500000);
            stage_block(CTL);
            for (k = 0; k < CTL; k++) {
                int32_t a = mix_l[k] < 0 ? -mix_l[k] : mix_l[k];
                mx = a > mx ? a : mx;
            }
        }
        expect("off-scale input (+-1.5 million, 45 x full scale): no overflow, the output stays under 4 x the clamp (524287)", mx <= 4 * 524287);
    }
}

/* ------------------------------------------------------------------ no click --- */
static int32_t ol_[3 * SEC], or_[3 * SEC];
static void test_click(void)
{
    uint32_t n = 0, a, b, k, i;
    double slope = 2.0 * 3.141592653589793 * 20000.0 * 1000.0 / FS, worst = 0, sl2;
    static const uint32_t WALK[] = {CFG(23, 4, 12, 6), CFG(0, 8, 0, 0), CFG(40, 1, 18, 12), CFG(10, 5, 5, 3), CFG(33, 2, 12, 6), CFG(0, 4, 9, 6), CFG(23, 7, 12, 6), CFG(23, 0, 12, 6)};
    printf("comp: no click\n");
    /* RATIO 1:1 <-> a ratio, a loud 1 kHz tone (20000) */
    stage_rest(CFG(23, 0, 12, 6));
    for (a = 0; a < 8u; a++) {
        cmp_cfg = (a & 1u) ? CFG(23, 0, 12, 6) : CFG(0, 4, 12, 6);   /* thr -40 at 4:1: the tone is 15 dB over, 11 dB of reduction */
        for (b = 0; b < SEC / 4u / CTL; b++) {
            for (k = 0; k < CTL; k++, n++) {
                mix_l[k] = (int32_t)lrint(20000.0 * sin(6.283185307179586 * 1000.0 * n / FS));
                mix_r[k] = mix_l[k];
            }
            stage_block(CTL);
            for (k = 0; k < CTL; k++)
                ol_[n - CTL + k] = mix_l[k];
        }
    }
    for (i = 1; i < n; i++) {
        double d = fabs((double)(ol_[i] - ol_[i - 1]));
        worst = d > worst ? d : worst;
    }
    printf("comp:   1:1 <-> 4:1 x8 on 1 kHz at 20000 (11 dB of reduction at 4:1, thr -40): largest output step %.0f a sample (the input tone's steepest slope %.0f)\n", worst, slope);
    expect("RATIO 1:1 <-> 4:1 eight times while a tone sounds: the output never steps faster than 1.05 x the tone's own slope (the gain moves over 15 ms)", worst <= slope * 1.05);
    /* the settings walked while it sounds: ratio, threshold, times; a bass + a tone */
    n = 0;
    worst = 0;
    sl2 = 2.0 * 3.141592653589793 * (12000.0 * 1000.0 + 6000.0 * 55.0) / FS;
    stage_rest(WALK[0]);
    for (a = 0; a < 8u; a++) {
        cmp_cfg = WALK[a];
        for (b = 0; b < SEC / 3u / CTL; b++) {
            for (k = 0; k < CTL; k++, n++) {
                mix_l[k] = (int32_t)lrint(12000.0 * sin(6.283185307179586 * 1000.0 * n / FS) + 6000.0 * sin(6.283185307179586 * 55.0 * n / FS));
                mix_r[k] = mix_l[k];
            }
            stage_block(CTL);
            for (k = 0; k < CTL; k++)
                or_[n - CTL + k] = mix_l[k];
        }
    }
    for (i = 1; i < n; i++) {
        double d = fabs((double)(or_[i] - or_[i - 1]));
        worst = d > worst ? d : worst;
    }
    printf("comp:   the settings walked (8 jumps of ratio / threshold / times; thr 0 .. -40, 1.5:1 .. INF, 0.1 ms .. 100 ms): largest output step %.0f (the signal's steepest slope %.0f)\n", worst, sl2);
    expect("the settings walked over their range with a tone and a bass sounding: no step faster than 1.6 x the signal's own slope (the gain at these settings is up to +2.4 dB net; they glide)", worst <= sl2 * 1.6);
}

/* ------------------------------------------------------------------ the whole mix: OFF, DUEL --- */
static void dsp_clean(uint32_t mode)                    /* the DSP as at power-on (silence everywhere), in mode (as mode_test.c) */
{
    memset(trk, 0, sizeof trk);
    memset(&trk[0].mdw, 0, sizeof trk[0].mdw);
    memset(mph, 0, sizeof mph);
    memset(&morph, 0, sizeof morph);
    memset(&trk[0].mxw, 0, sizeof trk[0].mxw);
    memset(ins, 0, sizeof ins);
    memset(midi_sel_on, 0, sizeof midi_sel_on);
    memset(midi_ch, 0, sizeof midi_ch);
    memset(midi_owners, 0, sizeof midi_owners);
    memset(mchord, 0, sizeof mchord);
    memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target);
    memset(kb_chn, 0, sizeof kb_chn);
    mix_bal = 0;
    mix_xf = mix_split = 0;
    vage = 0;
    mod_seed = 0x2545F491u;
    fm1_in.notes = 0;
    kb_prev = 0;
    mi_r = mi_w = 0;
    duell_mode_force(mode == DM_SPLIT ? DM_STEREO : DM_SPLIT);
    duell_mode_force(mode);
    memset(dly_buf, 0, sizeof dly_buf);
    memset(&fx, 0, sizeof fx);
    memset(&fx_b, 0, sizeof fx_b);
    memset(&hl, 0, sizeof hl);
    memset(&hl_b, 0, sizeof hl_b);
    lim_env = lim_env_r = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    memset(&bm, 0, sizeof bm);
    memset(&cmp, 0, sizeof cmp);
    cmp_cfg = 0;
    host_tracks_init();
    song.sel = 0;
    song.playing = 0;
    song.grid = 0;
}
#define SCN_N (3u * FS / CTL * CTL)
static int32_t rl[SCN_N], rr[SCN_N], ml_[SCN_N], mr_[SCN_N];   /* the outputs; mix_l / mix_r after the stage (before the master) */
/* mode_scn.h's render (two parts, SPREAD, every send, HALL) in mode, recorded; the compressor's setting cfg0 from the start, cfg1 from block `at` on */
static void scene(uint32_t mode, uint32_t cfg0, uint32_t cfg1, uint32_t at)
{
    static const uint8_t C1[4] = {48, 55, 60, 64}, C2[3] = {67, 72, 76};
    static int32_t o[2 * CTL];
    uint32_t k, i, b, nb = 3u * FS / CTL;
    dsp_clean(mode);
    host_preset(&trk[0], scn_eng("ANALOG"), 1);
    host_preset(&trk[1], scn_eng("PHASE"), 2);
    song.g[G_RTYPE] = 2;
    song.g[G_RLOCUT] = 0;
    song.g[G_DFDBK] = 90;
    song.g[G_DMIX] = 110;
    song.g[G_CDEPTH] = 100;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_SPRD] = 100;
    trk[0].p[P_PAN] = -30;
    trk[0].p[P_DIST] = 40;
    trk[0].p[P_LTRIG] = 0;
    trk[0].p[P_M1SRC] = MS_LFO;
    trk[0].p[P_M1DST] = MD_PAN;
    trk[0].p[P_M1AMT] = 50;
    trk[1].p[P_VOICE] = V_POLY;
    trk[1].p[P_PAN] = 40;
    trk[1].p[P_ITYPE] = IT_CHORUS;
    trk[1].p[P_IA] = 60;
    trk[1].p[P_IB] = 90;
    trk[1].p[P_IMIX] = 127;
    for (k = 0; k < 2u; k++) {
        trk[k].p[P_TRIM] = 120;
        trk[k].p[P_CHOR] = 100;
        trk[k].p[P_DLY] = 90;
        trk[k].p[P_REV] = 110;
        trk[k].p[P_SUS] = 110;
        trk[k].p[P_REL] = 70;
    }
    cmp_cfg = cfg0;
    for (b = 0; b < nb; b++) {
        if (b == at)
            cmp_cfg = cfg1;
        if (b == 10u) {
            for (i = 0; i < 4u; i++)
                trk_note_on(&trk[0], C1[i], 120);
            for (i = 0; i < 3u; i++)
                trk_note_on(&trk[1], C2[i], 110);
        }
        if (b == nb / 2u) {
            for (i = 0; i < 4u; i++)
                trk_note_off(&trk[0], C1[i]);
            for (i = 0; i < 3u; i++)
                trk_note_off(&trk[1], C2[i]);
        }
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            rl[b * CTL + i] = o[2u * i];
            rr[b * CTL + i] = o[2u * i + 1u];
            ml_[b * CTL + i] = mix_l[i];
            mr_[b * CTL + i] = mix_r[i];
        }
    }
    cmp_cfg = 0;
}
static uint64_t hash_of(void)
{
    uint64_t h = 0xCBF29CE484222325ull;
    uint32_t i;
    for (i = 0; i < SCN_N; i++) {
        h ^= (uint32_t)rl[i];
        h *= 0x100000001B3ull;
        h ^= (uint32_t)rr[i];
        h *= 0x100000001B3ull;
    }
    return h;
}
static int32_t keep_l[SCN_N], keep_r[SCN_N], keep_ml[SCN_N], keep_mr[SCN_N];
static double peak_of(const int32_t *x, uint32_t a, uint32_t b)
{
    double p = 0;
    uint32_t i;
    for (i = a; i < b; i++)
        p = fabs((double)x[i]) > p ? fabs((double)x[i]) : p;
    return p;
}
static void test_mix(void)
{
    uint64_t h0, h1;
    uint32_t i, same = 1;
    double p_off, p_on;
    printf("comp: the whole mix\n");
    scene(DM_STEREO, 0, 0, ~0u);
    h0 = hash_of();
    expect("RATIO 1:1 is bit for bit what it was: the busy two-part render (STEREO) hashes to the sources-before-COMP hash", h0 == MODE_STEREO_HASH);
    memcpy(keep_l, rl, sizeof keep_l), memcpy(keep_r, rr, sizeof keep_r), memcpy(keep_ml, ml_, sizeof keep_ml), memcpy(keep_mr, mr_, sizeof keep_mr);
    scene(DM_STEREO, CFG(0, 0, 0, 0), CFG(40, 0, 18, 12), 800u);
    expect("  .. with THRESH, ATTACK and RELEASE set (and changed during the render) at 1:1: the same hash (they have nothing to act on)", hash_of() == h0 && cmp.a == 0);
    scene(DM_STEREO, CFG(23, 4, 12, 6), CFG(23, 4, 12, 6), ~0u);
    h1 = hash_of();
    p_off = peak_of(keep_ml, SCN_N / 4u, SCN_N);
    p_on = peak_of(ml_, SCN_N / 4u, SCN_N);
    expect("4:1, thr -17: the render differs from 1:1 (the stage runs in STEREO)", h1 != h0);
    printf("comp:   mix peak 1:1 %.0f, 4:1 thr -17 %.0f (before MASTER)\n", p_off, p_on);
    scene(DM_STEREO, CFG(23, 4, 12, 6), CFG(23, 0, 12, 6), 1500u);   /* ON, then 1:1 at block 1500 (1.09 s): after the fade the mix is the never-ON mix, bit for bit */
    for (i = 1500u * CTL + 3000u; i < SCN_N; i++)
        same &= ml_[i] == keep_ml[i] && mr_[i] == keep_mr[i];
    expect("4:1, then 1:1 at 1.09 s: after the 15 ms fade (+ 3000 samples) the mix before the master is the never-ON mix's, bit for bit (the stage at rest)", same && cmp.a == 0);
    scene(DM_STEREO, CFG(23, 0, 12, 6), CFG(23, 4, 12, 6), 1200u);
    same = 1;
    for (i = 0; i < 1200u * CTL; i++)
        same &= rl[i] == keep_l[i] && rr[i] == keep_r[i];
    expect("1:1, then 4:1 at 0.87 s: until the switch every output sample is the 1:1 render's, bit for bit", same);
    scene(DM_LAYER, 0, 0, ~0u);
    memcpy(keep_l, rl, sizeof keep_l), memcpy(keep_r, rr, sizeof keep_r);
    scene(DM_LAYER, CFG(23, 4, 12, 6), CFG(23, 4, 12, 6), ~0u);
    expect("LAYER runs it too (the render with 4:1 differs from LAYER's 1:1)", memcmp(keep_l, rl, sizeof rl) != 0);

    scene(DM_SPLIT, 0, 0, ~0u);
    memcpy(keep_l, rl, sizeof keep_l), memcpy(keep_r, rr, sizeof keep_r);
    scene(DM_SPLIT, CFG(23, 4, 12, 6), CFG(23, 4, 12, 6), ~0u);
    expect("DUEL: COMP set (4:1, thr -17) does nothing: the render is bit for bit the one without it (part 1 left, part 2 right)",
           !memcmp(keep_l, rl, sizeof rl) && !memcmp(keep_r, rr, sizeof rr) && cmp.a == 0);
    scene(DM_SPLIT, CFG(0, 8, 0, 0), CFG(0, 8, 0, 0), ~0u);
    expect("  .. at INF, thr -40 too; and the stage holds no state in DUEL (share 0)", !memcmp(keep_l, rl, sizeof rl) && !memcmp(keep_r, rr, sizeof rr) && cmp.a == 0);
    {   /* the mode changed under it: STEREO (running) -> DUEL: cleared at the switch, back to STEREO: faded in from rest */
        static int32_t o[2 * CTL];
        uint32_t b;
        dsp_clean(DM_STEREO);
        host_preset(&trk[0], scn_eng("ANALOG"), 1);
        trk[0].p[P_SPRD] = 100;
        trk[0].p[P_VOICE] = V_POLY;
        trk[0].p[P_TRIM] = 120;
        cmp_cfg = CFG(23, 4, 12, 6);
        trk_note_on(&trk[0], 40, 120);
        for (b = 0; b < 400u; b++)
            mix_block(o, CTL);
        expect("STEREO, 4:1, a spread note sounding: the stage is running (share Q15 full)", cmp.a == 32768);
        duell_mode_force(DM_SPLIT);
        expect("a change to DUEL clears the stage (share 0, state 0)", cmp.a == 0 && cmp.pk == 0 && cmp.g == 0);
        for (b = 0; b < 100u; b++)
            mix_block(o, CTL);
        expect("  .. and in DUEL it stays at rest", cmp.a == 0);
        duell_mode_force(DM_STEREO);
        for (b = 0; b < 100u; b++)
            mix_block(o, CTL);
        expect("back in STEREO it runs again from rest (the share fades in: full after 85 hops)", cmp.a == 32768);
        cmp_cfg = 0;
        for (b = 0; b < 40u; b++)
            mix_block(o, CTL);
        expect("1:1 again: the share fades out (15 ms), then the stage is at rest and the gate stays shut", cmp.a == 0 && cmp.pk == 0 && !((cmp_cfg & 0xF00u) || cmp.a));
    }
    cmp_cfg = 0;
    memset(&cmp, 0, sizeof cmp);
}

/* ------------------------------------------------------------------ the control --- */
static uint32_t comp_page(void)
{
    uint32_t i;
    for (i = 0; i < NPAGES && !str_eq(PAGES[i].title, "COMPRESSOR"); i++)
        ;
    return i;
}
static int in_text(const char *hay, const char *a, const char *b)   /* a occurs before b in hay */
{
    const char *p = strstr(hay, a), *q = strstr(hay, b);
    return p && q && p < q;
}
static void test_control(void)
{
    uint32_t v, r, ok, cp;
    static char src[4000000];
    printf("comp: the control (the COMP page, song.cp, the settings word; no MENU row)\n");
    memset(nor, 0xFF, sizeof nor);
    boot();
    duell_mode_force(DM_STEREO);
    cp = comp_page();
    expect("a fresh unit: THRESH -17 dB, RATIO 1:1, ATTACK 10 ms, RELEASE 200 ms; the audio side's gate is shut", song.cp[0] == 23 && song.cp[1] == 0 && song.cp[2] == 80 && song.cp[3] == 40 && cmp_cfg == CFG(23, 0, 80, 40) && !(cmp_cfg & 0xF00u));
    expect("a fresh unit writes nothing for it: the settings word is 0 until a card is touched", cmp_set == 0u);
    expect("the page COMPRESSOR exists (0.2.25 layout round): GLO family, right after STEREO MASTER and before MODULES, SC_GLOBAL, K1 RATIO K2 THRESH K3 ATTACK K4 RELEASE, the curve picture (GR_COMP)", cp < NPAGES && PAGES[cp].fam == FAM_GLO && PAGES[cp].scope == SC_GLOBAL &&
           PAGES[cp].graph == GR_COMP && PAGES[cp].id[0] == G_CR && PAGES[cp].id[1] == G_CT && PAGES[cp].id[2] == G_CA && PAGES[cp].id[3] == G_CL &&
           str_eq(PAGES[cp - 1u].title, MASTER_PAGE_TITLE) && str_eq(PAGES[cp + 1u].title, "MODULES") && page_global(&PAGES[cp]) && G_CL == G_CT + 3);
    {
        static const char *const LAB[4] = {"THRESH", "RATIO", "ATTACK", "RELEASE"};
        static const uint8_t FMT[4] = {F_CTHR, F_CRAT, F_CATK, F_CREL};
        static const int16_t MAXV[4] = {40, 8, 120, 80}, DEF[4] = {23, 0, 80, 40};
        int16_t *vp;
        ok = 1;
        for (v = 0; v < 4u; v++) {
            const param_desc_t *d = glob_desc(G_CT + v);
            ok &= d == &GP[G_CT + v] && str_eq(d->label, LAB[v]) && d->fmt == FMT[v] && d->min == 0 && d->max == MAXV[v] && d->def == DEF[v] && page_desc(&PAGES[cp], v == 0u ? 1u : v == 1u ? 0u : v, &vp) == d && vp == &song.cp[v];
        }
        expect("the cards: THRESH 0..40, RATIO 0..8, ATTACK 0..120, RELEASE 0..80 on song.cp[0..3] (cards K2, K1, K3, K4), defaults 23 / 0 / 80 / 40", ok);
    }
    {   /* the values as shown */
        static const char *const RT[9] = {"OFF", "1.5:1", "2:1", "3:1", "4:1", "6:1", "8:1", "12:1", "INF"};
        static const char *const AT[19] = {"0.1", "0.15", "0.2", "0.3", "0.5", "0.7", "1", "1.5", "2", "3", "5", "7", "10", "15", "20", "30", "50", "70", "100"};
        static const char *const RL[13] = {"20", "30", "50", "70", "100", "150", "200", "300", "500", "700", "1", "1.5", "2"};
        char val[16], want[16];
        const char *unit;
        ok = 1;
        for (v = 0; v < 41u; v++) {
            snprintf(want, sizeof want, "%d", (int)v - 40);
            unit = "";
            param_format(&GP[G_CT], (int32_t)v, val, &unit);
            ok &= !strcmp(val, want) && !strcmp(unit, "dB");
        }
        for (v = 0; v < 9u; v++) {
            unit = "";
            param_format(&GP[G_CR], (int32_t)v, val, &unit);
            ok &= !strcmp(val, RT[v]) && !unit[0];
        }
        {   /* 0.2.24: 121 attack and 81 release settings, 40 a decade: the decades and a few points as shown, and every value on the log grid within a hundredth of a ms */
            static const struct { uint8_t atk; const char *v; } AP[] = {{0, "0.1"}, {13, "0.21"}, {20, "0.31"}, {40, "1"}, {60, "3.16"}, {80, "10"}, {100, "31.62"}, {120, "100"}};
            static const struct { uint8_t rel; const char *v, *u; } RP[] = {{0, "20", "ms"}, {20, "63", "ms"}, {40, "200", "ms"}, {60, "632", "ms"}, {70, "1.12", "s"}, {80, "2", "s"}};
            for (v = 0; v < sizeof AP / sizeof AP[0]; v++) {
                unit = "";
                param_format(&GP[G_CA], AP[v].atk, val, &unit);
                ok &= !strcmp(val, AP[v].v) && !strcmp(unit, "ms");
            }
            for (v = 0; v < sizeof RP / sizeof RP[0]; v++) {
                unit = "";
                param_format(&GP[G_CL], RP[v].rel, val, &unit);
                ok &= !strcmp(val, RP[v].v) && !strcmp(unit, RP[v].u);
            }
            for (v = 0; v < CMP_ATK_N; v++)
                ok &= labs((long)CMP_ATK_US(v) - lrint(100.0 * pow(10.0, v / 40.0))) <= 1 + (long)(CMP_ATK_US(v) / 1000u);   /* (0.1 %) */
            for (v = 0; v < CMP_REL_N; v++)
                ok &= labs((long)CMP_REL_MS(v) - lrint(20.0 * pow(10.0, v / 40.0))) <= 1 + (long)(CMP_REL_MS(v) / 1000u);
        }
        expect("the values as shown: THRESH -40 .. 0 dB; RATIO OFF (the stored 1:1) 1.5:1 2:1 3:1 4:1 6:1 8:1 12:1 INF; ATTACK 0.1 .. 100 ms and RELEASE 20 ms .. 2 s, 40 a decade (0.2.24)", ok);
    }
    ok = 1;
    for (r = 0; r < MI_COUNT; r++)
        ok &= strcmp(MI_NAME[r], "COMP") != 0 && strcmp(MI_NAME[r], "COMPRESSOR") != 0;
    expect("no MENU row is called COMP: the tab AUDIO is as it was (4 rows, one page; 0.2.25: USB LEVEL removed)", ok && mtab_rows(MTAB_AUDIO) == 4u && mtab_pages(MTAB_AUDIO) == 1u);

    /* the knobs, the hide rule */
    go_title("COMPRESSOR");
    expect("STEREO, RATIO 1:1: the page shows; only RATIO is drawn (THRESH, ATTACK, RELEASE have nothing to act on at 1:1)", page_visible(cp) && ui.page == cp && card_hidden_vp(&song.cp[0]) && !card_hidden_vp(&song.cp[1]) &&
           card_hidden_vp(&song.cp[2]) && card_hidden_vp(&song.cp[3]));
    turn(EN_K2, 1);
    expect("  .. K2 (THRESH) changes nothing while it is hidden", song.cp[0] == 23);
    turn(EN_K1, 1);
    frame();
    expect("K1 right: RATIO 1.5:1 (OFF its lowest value: turning it up switches the compressor on); the gate opens on the audio side after the next frame; the header says COMPRESSOR 1.5:1", song.cp[1] == 1 && (cmp_cfg & 0xF00u) == 0x100u && str_eq(ui.msg, "COMPRESSOR 1.5:1"));
    expect("  .. now all four cards are drawn", !card_hidden_vp(&song.cp[0]) && !card_hidden_vp(&song.cp[1]) && !card_hidden_vp(&song.cp[2]) && !card_hidden_vp(&song.cp[3]));
    turn(EN_K2, -1);
    turn(EN_K3, 1);
    turn(EN_K4, 1);
    frame();
    expect("K2 left THRESH -18 dB, K3 right ATTACK 10.6 ms, K4 right RELEASE 212 ms (one fine setting a detent), the audio side follows", song.cp[0] == 22 && song.cp[2] == 81 && song.cp[3] == 41 && cmp_cfg == CFG(22, 1, 81, 41));
    ok = 1;
    for (v = 0; v < 60u; v++)
        turn(EN_K1, 1);
    ok &= song.cp[1] == 8;
    for (v = 0; v < 130u; v++)
        turn(EN_K2, -1), turn(EN_K3, -1), turn(EN_K4, -1);
    ok &= song.cp[0] == 0 && song.cp[2] == 0 && song.cp[3] == 0;
    for (v = 0; v < 130u; v++)
        turn(EN_K2, 1), turn(EN_K3, 1), turn(EN_K4, 1);
    ok &= song.cp[0] == 40 && song.cp[2] == 120 && song.cp[3] == 80;
    for (v = 0; v < 60u; v++)
        turn(EN_K1, -1);
    ok &= song.cp[1] == 0 && str_eq(ui.msg, "COMPRESSOR OFF");
    expect("the knobs stop at both ends; RATIO back to its lowest says COMPRESSOR OFF", ok);

    /* DUEL: the cards are not there (the page stays, as BASS MONO's) */
    song.cp[1] = 4;
    frame();
    duell_mode_force(DM_SPLIT);
    ok = card_hidden_vp(&song.cp[0]) && card_hidden_vp(&song.cp[1]) && card_hidden_vp(&song.cp[2]) && card_hidden_vp(&song.cp[3]);
    expect("DUEL: all four cards are hidden (not drawn, the knobs do nothing)", ok);
    go_title("COMPRESSOR");
    turn(EN_K1, 1);
    turn(EN_K2, 1);
    turn(EN_K3, 1);
    turn(EN_K4, 1);
    expect("  .. the knobs change nothing in DUEL; the values are kept for STEREO and the stage does not run (test 6)", song.cp[0] == 40 && song.cp[1] == 4 && song.cp[2] == 120 && song.cp[3] == 80);
    duell_mode_force(DM_LAYER);
    expect("LAYER: the cards are there again", !card_hidden_vp(&song.cp[0]) && !card_hidden_vp(&song.cp[1]));
    duell_mode_force(DM_STEREO);
    {   /* SELECT in the GLO round: REVERB (SHIMMER only with TYPE SHIM), BASS MONO, COMP, MODULES */
        go_title("REVERB");
        turn(EN_SELECT, 1);
        ok = str_eq(cur_page()->title, MASTER_PAGE_TITLE);
        turn(EN_SELECT, 1);
        ok &= ui.page == cp;
        turn(EN_SELECT, 1);
        ok &= str_eq(cur_page()->title, "MODULES");
        expect("the GLO round (SELECT) goes REVERB, STEREO MASTER, COMPRESSOR, MODULES (0.2.25 layout round)", ok);
    }
    {   /* OCT- held + a knob: the card's default */
        song.cp[3] = 2;
        song.cp[1] = 5;
        frame();
        go_title("COMPRESSOR");
        ok = reset_desc(&GP[G_CL], &song.cp[3], 0, 0) == 1 && song.cp[3] == 40 && reset_desc(&GP[G_CR], &song.cp[1], 0, 0) == 1 && song.cp[1] == 0;
        expect("the quick reset (OCT- + a knob) puts a card back to its default: RELEASE 200 ms, RATIO OFF (1:1)", ok);
    }

    /* the stored word */
    song.cp[0] = 30, song.cp[1] = 5, song.cp[2] = 7, song.cp[3] = 9;
    frame();
    fm1_ms += 2100u;
    frame();                                            /* (morph_settings' timer: the save ~2 s after the last change) */
    flush();
    {
        static persist_t p;
        uint32_t want = 0xC8000000u | 30u | 5u << 6 | 7u << 10 | 9u << 17;
        ok = st_load(OBJ_SETTINGS, &p, sizeof p) > 0 && p.rsv == want && p.magic == PERSIST_MAGIC;
        expect("a change is saved ~2 s later in the settings record's rsv word: 0xC8 | thr | ratio << 6 | atk << 10 | rel << 17 (same record, same magic PER4)", ok);
        expect("  .. the record's size is the MIDI MAP's (persist_t, 4 + 4 + 4 + 4 words, panel, favorites, the map's 128 lines and 64 reserved bytes)", sizeof(persist_t) == 4u * 4u + sizeof(panel_t) + 4u + 16u * 32u + 4u * 5u + 2u * MAP_N + 64u && ST_PAYLOAD_MAX >= sizeof(persist_t));
        boot();
        frame();
        expect("a power-on reads it back (THRESH -10, RATIO 6:1, ATTACK 1.5 ms... as set): the same four values, cmp_cfg to the audio side", song.cp[0] == 30 && song.cp[1] == 5 && song.cp[2] == 7 && song.cp[3] == 9 &&
               cmp_cfg == CFG(30, 5, 7, 9));
        /* an older record: rsv 0, 1, 2 (the OUTPUT EQ), or anything without the tag: the defaults */
        ok = 1;
        for (v = 0; v < 4u; v++) {
            p.rsv = v;
            st_save(OBJ_SETTINGS, &p, sizeof p);
            boot();
            frame();
            ok &= song.cp[0] == 23 && song.cp[1] == 0 && song.cp[2] == 80 && song.cp[3] == 40;
        }
        expect("an older record (rsv 0, 1, 2, 3: the retired OUTPUT EQ) reads as the defaults: 1:1 = off, nothing changes", ok);
        /* every byte value in every byte of the word: safe */
        ok = 1;
        for (r = 0; r < 4u; r++)
            for (v = 0; v < 256u; v++) {
                p.rsv = 0xC80000C8u & ~(0xFFu << (8u * r));
                p.rsv |= v << (8u * r);
                st_save(OBJ_SETTINGS, &p, sizeof p);
                boot();
                frame();
                ok &= song.cp[0] >= 0 && song.cp[0] <= 40 && song.cp[1] >= 0 && song.cp[1] <= 8 && song.cp[2] >= 0 && song.cp[2] <= 120 && song.cp[3] >= 0 && song.cp[3] <= 80 &&
                      ((cmp_cfg >> 8) & 255u) <= 8u && (cmp_cfg >> 16 & 255u) <= 120u && (cmp_cfg >> 24) <= 80u && (cmp_cfg & 255u) <= 40u;
            }
        expect("every byte value 0..255 in each of the word's four bytes (1024 power-ons): every setting in its range, nothing out of bounds on the audio side", ok);
        ok = 1;
        for (v = 0; v < 2000u; v++) {
            p.rsv = (uint32_t)noise(2147483647) * 2654435761u ^ (v & 1u ? 0xC8000000u : 0u);
            st_save(OBJ_SETTINGS, &p, sizeof p);
            boot();
            frame();
            ok &= song.cp[0] <= 40 && song.cp[1] <= 8 && song.cp[2] <= 120 && song.cp[3] <= 80;
        }
        expect("2000 random words (half of them with the tag): all in range", ok);
        /* unchanged -> not rewritten */
        p.rsv = 0;
        st_save(OBJ_SETTINGS, &p, sizeof p);
        boot();
        frame();
        flush();
        ok = st_load(OBJ_SETTINGS, &p, sizeof p) > 0 && p.rsv == 0u;
        expect("a unit that never touches COMP keeps rsv 0 in its record (nothing written for it)", ok);
    }
    /* ERASE DATA: the unit starts as a new one (the restart): the defaults */
    song.cp[1] = 4;
    frame();
    fm1_ms += 2100u;
    frame();
    flush();
    memset(nor, 0xFF, sizeof nor);                      /* (what ERASE DATA leaves: no valid settings; the RAM is gone at the restart) */
    boot();
    frame();
    expect("after ERASE DATA (no settings, the restart): the defaults, RATIO 1:1", song.cp[0] == 23 && song.cp[1] == 0 && song.cp[2] == 80 && song.cp[3] == 40 && !(cmp_cfg & 0xF00u) && cmp_set == 0u);

    /* not in the multis, the project, the patches */
    boot();
    duell_mode_force(DM_STEREO);
    song.cp[0] = 12, song.cp[1] = 3, song.cp[2] = 4, song.cp[3] = 5;
    frame();
    project_save_as(10, "COMP TEST");                   /* M11 */
    song.cp[1] = 7;
    frame();
    project_load(10);
    frame();
    expect("a multi load leaves COMP alone (a device setting: nothing of it in the multi)", song.cp[0] == 12 && song.cp[1] == 7 && song.cp[2] == 4 && song.cp[3] == 5);
    {
        uint32_t k;
        ok = 1;
        for (k = 0; k < 16u; k++) {
            if (df_of(k) >= DF_N)
                continue;
            project_load(k);
            frame();
            ok &= song.cp[0] == 12 && song.cp[1] == 7 && song.cp[2] == 4 && song.cp[3] == 5;
        }
        expect("  .. nor does any factory multi", ok);
    }
    {
        project_t q;
        project_capture(&q);
        song.cp[1] = 2;
        project_restore_runtime(&q);
        frame();
        expect("  .. nor the project (project_restore_runtime)", song.cp[1] == 2);
    }
    {   /* the stage's place in mix_block: after PERFORM, SPACE's width and BASS MONO, before the MASTER gain and master_out */
        FILE *f = fopen("firmware/src/fx.c", "r");
        if (!f)
            f = fopen("felucca-base/firmware/src/fx.c", "r");
        ok = 0;
        if (f) {
            size_t n = fread(src, 1, sizeof src - 1u, f);
            const char *mb;
            src[n] = 0;
            fclose(f);
            mb = strstr(src, "mix_master(int32_t *out, uint32_t n, int perf, int32_t mg)");   /* (MIXSPLIT: the master stages of mix_block are mix_master) */
            ok = mb && in_text(mb, "perf_master(n);", "space_width(n);") && in_text(mb, "space_width(n);", "bass_mono(n);") && in_text(mb, "bass_mono(n);", "comp_stage(n, ") &&
                 in_text(mb, "comp_stage(n, ", "split_master(out, n, mg);") && in_text(mb, "comp_stage(n, ", "master_out(&l, &r);") && strstr(mb, "!dm_split && ((cmp_cfg & 0xF00u) || cmp.a)") && strstr(mb, "comp_stage(n, perf ? cmp_poff(mg0) : 0)") && in_text(mb, "mg0 = mg", "perf_master(n);");
        }
        expect("mix_master (mix_block's master stages): perf_master, space_width, bass_mono, then comp_stage (gated: not in DUEL, only for a ratio or while fading), then the MASTER gain and master_out", ok);
    }
    {   /* not learnable, not morphed: the cards are song.cp (no part's p[]), the page is SC_GLOBAL; the matrix and MORPH reach part parameters only */
        ok = 1;
        for (v = 0; v < NTRK; v++)
            ok &= (int16_t *)&song.cp[0] < trk[v].p || (int16_t *)&song.cp[0] >= trk[v].p + P_COUNT;
        expect("the cards are no part parameter (MIDI LEARN, automation, the matrix and MORPH reach p[] only): not learnable, not morphed, as BASS MONO", ok && G_CT > G_COUNT);
    }
    boot();
}

/* ------------------------------------------------------------------ cost --- */
static void test_cost(void)
{
    uint32_t i, k, reps = 20000u;
    uint64_t t0, t1, base, st, gl;
    printf("comp: cost (host instructions per sample of the stage, one channel pair)\n");
    stage_rest(CFG(23, 4, 12, 6));
    for (i = 0; i < 100u; i++) {
        for (k = 0; k < CTL; k++)
            mix_l[k] = noise(20000), mix_r[k] = noise(20000);
        stage_block(CTL);
    }
    t0 = instr_now();
    for (i = 0; i < reps; i++)
        for (k = 0; k < CTL; k++)
            mix_l[k] = noise(20000), mix_r[k] = noise(20000);
    t1 = instr_now();
    base = t1 - t0;
    t0 = instr_now();
    for (i = 0; i < reps; i++) {
        for (k = 0; k < CTL; k++)
            mix_l[k] = noise(20000), mix_r[k] = noise(20000);
        comp_stage(CTL, 0);
    }
    t1 = instr_now();
    st = t1 - t0 - base;
    t0 = instr_now();
    for (i = 0; i < reps; i++) {
        for (k = 0; k < CTL; k++)
            mix_l[k] = noise(20000), mix_r[k] = noise(20000);
        cmp_cfg = CFG(i & 31u, 1u + (i & 7u), i % 19u, i % 13u);   /* the setting moving every block: the glides and the new coefficients */
        comp_stage(CTL, 0);
    }
    t1 = instr_now();
    gl = t1 - t0 - base;
    if (st && gl) {
        printf("comp:   steady %.1f, settings moving %.1f host instructions per sample (a block of %u: %.0f / %.0f)\n", (double)st / reps / CTL, (double)gl / reps / CTL, CTL, (double)st / reps, (double)gl / reps);
        expect("the stage costs under 60 host instructions per sample steady, under 65 while the settings move (the study estimated 30..48 for a simpler one); the gate when 1:1 is one load and a test a block", (double)st / reps / CTL < 60.0 && (double)gl / reps / CTL < 65.0);
    } else {
        printf("comp:   (no instruction counter on this host: cost not measured)\n");
    }
    cmp_cfg = 0;
    memset(&cmp, 0, sizeof cmp);
}


/* ------------------------------------------------------------------ the listening set (comp_test DEMODIR) --- */
/* three short pieces without a drum engine: ANALOG bass + pad (LADDER BASS, SILK CHORUS), WAVE chord stabs over a pad (DRAWBARS, DIGITAL PAD), PHYS / NOISE hits (FLOOR THUD, STATIC SNARE);
 * LAYER, MASTER at its default (2048). Each piece is rendered at 1:1 and at 2:1, 4:1 and 8:1, FAST (attack 3 ms, release 100 ms) and SLOW (attack 30 ms, release 700 ms), the threshold
 * chosen from the piece's own pre-MASTER peak (12 dB under it); the files are what the unit's output would be (after MASTER, the limiter, the soft clip) */
typedef struct { uint32_t t_ms, dur_ms; uint8_t part, note, vel; } demo_ev_t;
static int32_t dl_[12 * SEC], dr_[12 * SEC];
static double pre_peak;
static void demo_piece(uint32_t kind, uint32_t cfg, uint32_t secs, int32_t *ol, int32_t *or_)
{
    static demo_ev_t ev[400];
    static const uint8_t BASS[8] = {36, 36, 43, 36, 39, 36, 43, 46}, CH1[4] = {48, 55, 58, 63}, CH2[4] = {44, 51, 55, 60}, STAB[4] = {60, 63, 67, 70}, STAB2[4] = {58, 62, 65, 69};
    uint32_t n = 0, bar, i, b, nb = secs * FS / CTL, e0, p;
    static int32_t o[2 * CTL];
    uint32_t bm = 600u;                                 /* 100 BPM: a beat 600 ms */
    for (bar = 0; bar * 4u * bm < secs * 1000u - 1500u; bar++) {
        uint32_t t0 = bar * 4u * bm;
        if (kind == 0) {                                /* ANALOG LADDER BASS: eighths, accents; SILK CHORUS: a chord a bar, held */
            for (i = 0; i < 8u; i++) {
                ev[n].t_ms = t0 + i * bm / 2u, ev[n].dur_ms = bm * 2u / 5u, ev[n].part = 0, ev[n].note = BASS[i] + (bar & 1u ? 0u : 0u), ev[n].vel = i % 4u == 0u ? 120 : 92;
                n++;
            }
            for (i = 0; i < 4u; i++) {
                ev[n].t_ms = t0, ev[n].dur_ms = 4u * bm - 80u, ev[n].part = 1, ev[n].note = (bar & 1u ? CH2 : CH1)[i] + 12u, ev[n].vel = 84;
                n++;
            }
        } else if (kind == 1) {                         /* WAVE DRAWBARS stabs (1, 2.5, 3.5, 4) over a held DIGITAL PAD */
            static const uint16_t AT[4] = {0, 1500, 2100, 3000};
            uint32_t k;
            for (k = 0; k < 4u; k++)
                for (i = 0; i < 4u; i++) {
                    ev[n].t_ms = t0 + AT[k], ev[n].dur_ms = k == 3u ? 380u : 250u, ev[n].part = 0, ev[n].note = (bar & 1u ? STAB2 : STAB)[i], ev[n].vel = k == 0u ? 118 : 96;
                    n++;
                }
            for (i = 0; i < 3u; i++) {
                ev[n].t_ms = t0, ev[n].dur_ms = 4u * bm - 80u, ev[n].part = 1, ev[n].note = (bar & 1u ? STAB2 : STAB)[i] - 12u, ev[n].vel = 70;
                n++;
            }
        } else {                                        /* PHYS FLOOR THUD: 1, 2.5 (soft), 3, 3.75; NOISE STATIC SNARE: 2 and 4, a ghost on 3.5 */
            static const uint16_t KT[4] = {0, 1500, 1800, 2250}, ST[3] = {600, 1800 + 600, 2100};
            for (i = 0; i < 4u; i++) {
                ev[n].t_ms = t0 + KT[i] , ev[n].dur_ms = 300, ev[n].part = 0, ev[n].note = 36, ev[n].vel = i == 1u ? 70 : 118;
                n++;
            }
            for (i = 0; i < 3u; i++) {
                ev[n].t_ms = t0 + ST[i], ev[n].dur_ms = 200, ev[n].part = 1, ev[n].note = 60, ev[n].vel = i == 2u ? 52 : 112;
                n++;
            }
        }
    }
    dsp_clean(DM_LAYER);
    song.master_q12 = 2048;
    song.g[G_RTYPE] = 2;
    song.g[G_RLOCUT] = 0;
    if (kind == 0) {
        host_ds_load(&trk[0], scn_eng("ANALOG"), 0);
        host_ds_load(&trk[1], scn_eng("ANALOG"), 9);
    } else if (kind == 1) {
        host_ds_load(&trk[0], scn_eng("WAVE"), 4);
        host_ds_load(&trk[1], scn_eng("WAVE"), 14);
    } else {
        host_ds_load(&trk[0], scn_eng("PHYS"), 8);
        host_ds_load(&trk[1], scn_eng("NOISE"), 7);
    }
    for (p = 0; p < 2u; p++) {
        trk[p].p[P_VOICE] = kind == 0 && p == 0 ? V_MONO : trk[p].p[P_VOICE];
        trk[p].p[P_PAN] = p ? 20 : -10;
    }
    cmp_cfg = cfg;
    pre_peak = 0;
    for (b = 0; b < nb; b++) {
        uint32_t t = b * CTL, ms = FS / 1000u, tm = t / ms, tm2 = (t + CTL) / ms;
        for (e0 = 0; e0 < n; e0++) {
            if (ev[e0].t_ms >= tm && ev[e0].t_ms < tm2 + (tm == tm2))
                trk_note_on(&trk[ev[e0].part], ev[e0].note, ev[e0].vel);
            else if (ev[e0].t_ms + ev[e0].dur_ms >= tm && ev[e0].t_ms + ev[e0].dur_ms < tm2 + (tm == tm2))
                trk_note_off(&trk[ev[e0].part], ev[e0].note);
        }
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            double m = fabs((double)(mix_l[i] + (cfg & 0xF00u ? 0 : wet[i])));
            pre_peak = m > pre_peak ? m : pre_peak;
            ol[b * CTL + i] = o[2u * i];
            or_[b * CTL + i] = o[2u * i + 1u];
        }
    }
    cmp_cfg = 0;
    (void)i;
}
/* the spread of the short-term level: the loudest 100 ms window less the median one (dB), windows over -60 dBFS; a compressor makes it smaller */
static double spread_db(const int32_t *l, const int32_t *r, uint32_t n)
{
    static double w[400];
    uint32_t nw = 0, i, j, win = FS / 10u;
    double mx = -999;
    for (i = 0; i + win <= n && nw < 400u; i += win) {
        double e = 0;
        for (j = 0; j < win; j++)
            e += (double)l[i + j] * l[i + j] + (double)r[i + j] * r[i + j];
        w[nw] = 10.0 * log10(e / (2.0 * win) / (32768.0 * 32768.0) + 1e-12);
        mx = w[nw] > mx ? w[nw] : mx;
        if (w[nw] > -60.0)
            nw++;
    }
    for (i = 1; i < nw; i++) {
        double x = w[i];
        for (j = i; j > 0 && w[j - 1] > x; j--)
            w[j] = w[j - 1];
        w[j] = x;
    }
    return nw ? mx - w[nw / 2] : 0;
}
static void wav_out(const char *path, const int32_t *l, const int32_t *r, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    uint32_t i, data = n * 4u, v;
    uint16_t s;
    if (!f)
        return;
    fwrite("RIFF", 1, 4, f), v = 36u + data, fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f), v = 16, fwrite(&v, 4, 1, f);
    s = 1, fwrite(&s, 2, 1, f), s = 2, fwrite(&s, 2, 1, f);
    v = FS, fwrite(&v, 4, 1, f), v = FS * 4u, fwrite(&v, 4, 1, f);
    s = 4, fwrite(&s, 2, 1, f), s = 16, fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f), fwrite(&data, 4, 1, f);
    for (i = 0; i < n; i++) {
        int32_t a = clamp(l[i], -32768, 32767), c = clamp(r[i], -32768, 32767);
        s = (uint16_t)a, fwrite(&s, 2, 1, f);
        s = (uint16_t)c, fwrite(&s, 2, 1, f);
    }
    fclose(f);
}
static void demo(const char *dir)
{
    static const char *const TITLE[3] = {"ANALOG bass + pad: LADDER BASS and SILK CHORUS (eighth-note bass, a held chord a bar)",
                                         "WAVE chord: DRAWBARS stabs over a DIGITAL PAD", "PHYS FLOOR THUD and NOISE STATIC SNARE hits (no drum engine)"};
    static const char *const TAG[3] = {"bass_pad", "wave_chord", "hits"};
    static const uint32_t RATIO_N[3] = {2, 4, 8}, RIDX[3] = {2, 4, 6};   /* 2:1, 4:1, 8:1: the ratio list's indices */
    static const struct { const char *tag, *name; uint32_t atk, rel; } SP[2] = {{"fast", "FAST: attack 3 ms, release 100 ms", 9, 4}, {"slow", "SLOW: attack 30 ms, release 700 ms", 15, 9}};
    uint32_t k, v, sp, secs = 10, n = secs * FS / CTL * CTL, i;
    char path[500], idx[500];
    FILE *h;
    snprintf(idx, sizeof idx, "%s/index.html", dir);
    h = fopen(idx, "w");
    if (!h)
        return;
    fprintf(h, "<!doctype html><meta charset=utf-8><title>DUELL 0.2.21 COMP</title>"
               "<style>body{font:15px/1.4 system-ui;margin:24px;max-width:1150px}table{border-collapse:collapse}td,th{padding:4px 10px;border-bottom:1px solid #ccc;text-align:left}"
               "audio{width:260px;height:30px}h2{margin-top:28px}small{color:#666}</style>"
               "<h1>DUELL 0.2.21: the master compressor (COMP)</h1>"
               "<p>Three short pieces, each through the unit's master chain at the default MASTER: COMP off (RATIO 1:1), then 2:1, 4:1 and 8:1 "
               "fast and slow times. The threshold of each piece is 12 dB under its own loudest peak (the output's, MASTER at its default). The automatic makeup keeps a level of -8 dB as it is, so the average loudness should stay "
               "close to the OFF file's (the RMS is listed). Ten seconds each.</p>");
    for (k = 0; k < 3u; k++) {
        double rms_off = 0, pk_off = 0, sp_off;
        int thr_i = 23;
        fprintf(h, "<h2>%s</h2><table><tr><th>setting</th><th>FAST</th><th>SLOW</th></tr>", TITLE[k]);
        demo_piece(k, 0, secs, dl_, dr_);
        thr_i = (int)lrint(20.0 * log10(pre_peak / 65536.0) - 12.0) + 40;
        thr_i = thr_i < 0 ? 0 : thr_i > 40 ? 40 : thr_i;
        for (i = 0; i < n; i++) {
            double a = dl_[i], b = dr_[i];
            rms_off += a * a + b * b;
            pk_off = fabs(a) > pk_off ? fabs(a) : pk_off;
            pk_off = fabs(b) > pk_off ? fabs(b) : pk_off;
        }
        rms_off = sqrt(rms_off / (2.0 * n));
        snprintf(path, sizeof path, "%s/%s_off.wav", dir, TAG[k]);
        wav_out(path, dl_, dr_, n);
        sp_off = spread_db(dl_, dr_, n);
        fprintf(h, "<tr><td>OFF (1:1) <small>RMS %.1f dBFS, peak %.1f dBFS, loud-to-median %.1f dB; the peak %.1f dB re the output's full scale at the default MASTER, THRESH %d dB for the rest</small></td><td colspan=2><audio controls preload=none src=\"%s_off.wav\"></audio></td></tr>",
                20.0 * log10(rms_off / 32768.0), 20.0 * log10(pk_off / 32768.0), sp_off, 20.0 * log10(pre_peak / 65536.0), thr_i - 40, TAG[k]);
        for (v = 0; v < 3u; v++) {
            fprintf(h, "<tr><td>%u:1</td>", (unsigned)RATIO_N[v]);
            for (sp = 0; sp < 2u; sp++) {
                double rms = 0, pk = 0;
                uint32_t rat = RIDX[v];
                demo_piece(k, CFG(thr_i, rat, SP[sp].atk, SP[sp].rel), secs, dl_, dr_);
                for (i = 0; i < n; i++) {
                    double a = dl_[i], b = dr_[i];
                    rms += a * a + b * b;
                    pk = fabs(a) > pk ? fabs(a) : pk;
                    pk = fabs(b) > pk ? fabs(b) : pk;
                }
                rms = sqrt(rms / (2.0 * n));
                snprintf(path, sizeof path, "%s/%s_%u_%s.wav", dir, TAG[k], (unsigned)RATIO_N[v], SP[sp].tag);
                wav_out(path, dl_, dr_, n);
                fprintf(h, "<td><audio controls preload=none src=\"%s_%u_%s.wav\"></audio><br><small>%s: RMS %+.1f dB, peak %+.1f dB, loud-to-median %.1f dB (OFF %.1f)</small></td>", TAG[k], (unsigned)RATIO_N[v], SP[sp].tag, SP[sp].name,
                        20.0 * log10(rms / rms_off), 20.0 * log10(pk / pk_off), spread_db(dl_, dr_, n), sp_off);
                printf("comp: demo %s (RMS %+.1f dB, peak %+.1f dB vs OFF; loud-to-median %.1f dB, OFF %.1f)\n", path, 20.0 * log10(rms / rms_off), 20.0 * log10(pk / pk_off), spread_db(dl_, dr_, n), sp_off);
            }
            fprintf(h, "</tr>");
        }
        fprintf(h, "</table>");
    }
    fprintf(h, "</body>");
    fclose(h);
}

int main(int argc, char **argv)
{
    uint64_t h;
    if (argc > 1) {                                     /* the listening set into argv[1] (see demo) */
        demo(argv[1]);
        return 0;
    }
    h = mode_stereo_scenario();                /* (the process's first render: the baseline hash, before anything else touches the DSP) */
    printf("comp: STEREO scenario hash 0x%016llX (before COMP: 0x%016llX)\n", (unsigned long long)h, (unsigned long long)MODE_STEREO_HASH);
    expect("the busy two-part render (the process's first, COMP at 1:1) is bit for bit the one of the sources before COMP", h == MODE_STEREO_HASH);
    test_curve();
    test_times();
    test_link();
    test_click();
    test_mix();
    test_control();
    test_cost();
    printf("comp: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
