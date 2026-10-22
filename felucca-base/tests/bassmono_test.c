/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.19 BASS MONO (fx.c bass_mono; params.c master-stage page, core.h G_BM / song.bm; stored in project.c byte 65; NOT in a multi since 0.2.21, multi.c byte 5 bits 1..4 are written 0 and ignored): the master mono below a chosen
 * frequency, stereo above it. On the firmware's sources (the DSP of hostsim.c and the UI of ui_test.c), against simulated NOR flash.
 *   build/host/bassmono_test         (host_tests.sh bassmono)
 * 1. OFF is bit for bit what it was: a busy two-part render (mode_scn.h, the process's first render) hashes to MODE_STEREO_HASH, the hash the sources
 *    before BASS MONO gave; a render that had it ON and then OFF is, after the fade, sample for sample the render that never had it.
 * 2. the stage on synthetic stereo: L + R untouched sample for sample (random noise); an in-phase tone passes bit for bit; a 40 Hz out-of-phase tone becomes
 *    mono (side removed) while a 2 kHz out-of-phase tone passes unchanged; the -3 dB corner of the side within 0.5 dB of the printed Hz at all 11 settings,
 *    12 dB an octave below it; a DC side is removed; loud and off-scale input does not overflow.
 * 3. no click: the setting changed across its whole range (every pair, the ends) and OFF <-> ON while a bass tone sounds in the side: no step above the
 *    signal's own slope; exact silence after the sound stops (and after the fade-out the state is dropped).
 * 4. DUEL (SPLIT): the stage does not run (the render with BASS MONO set is bit for bit the render without), STEREO and LAYER run it; a change of
 *    the mode clears its state.
 * 5. the control: the master-stage page (GLO family, after REVERB / SHIMMER and before MODULES (0.2.21), title BASS MONO; K1 BASS MONO, OFF the default, OFF + 11 corners 40 .. 400 Hz), no MENU row (AUDIO back to its six
 *    rows), not in DUEL (the page is skipped, the card hidden), the knob; not in the settings record ([15][29] ignored, any byte); the multis carry it (a multi saved
 *    with 100 Hz restores it, a factory multi loads OFF, a code past 11 reads OFF), the project (byte 65); the editor's id 33 is editor_test's.
 * 6. cost, host instructions per sample (steady, gliding), and the mix_block gate when OFF. */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"
#include "mode_scn.h"
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif

#define MODE_STEREO_HASH 0xC04494F53D46F0F0ull   /* 0.2.30: merged line (cloud wave 6 + click fixes), re-based on purpose, was 0x1CC6C8241C84C0DA; before:  0.2.29: re-based on purpose, the scenario runs the HALL and its quiet lines now round to the nearest (fx.c hall_end; 0.2.21..0.2.28: 0xC970174114DFD9BA) */   /* the same constant as mode_test.c (0.2.21: re-based with the STEREO CHORUS; the sources before BASS MONO gave 0xA77A7B40656EB88D, and still do with the second chorus tap set to the first, -DCHO_OFF=0u) */

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
    printf("bassmono: %-118s %s\n", what, ok ? "ok" : "FAIL");
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

/* ------------------------------------------------------------------ the stage on synthetic stereo --- */
#define SEC FS
static uint32_t xs = 0x2545F491u;
static int32_t noise(int32_t amp)
{
    xs ^= xs << 13;
    xs ^= xs >> 17;
    xs ^= xs << 5;
    return (int32_t)(((int64_t)(int32_t)xs * amp) / 2147483648LL);
}
static void stage_rest(uint32_t sel)                    /* the stage at rest, setting sel */
{
    memset(&bm, 0, sizeof bm);
    bm_sel = (uint8_t)sel;
}
/* one block through the stage as mix_block gates it (not in DUEL: the caller's case) */
static void stage_block(uint32_t n)
{
    if (bm_sel || bm.a)
        bass_mono(n);
}
/* a tone pair: left = lt * sin, right = rt * sin (rt negative: out of phase), f Hz; the run of n samples from sample i0 on; returns the output rms
 * of the side (L - R) / 2 and of the mid (L + R) / 2 over the last `meas` samples */
typedef struct { double side, mid, l, r; } rms_t;
static rms_t tone_run(uint32_t sel, double f, double lt, double rt, uint32_t warm, uint32_t meas)
{
    uint32_t i, k, tot = warm + meas;
    double ss = 0, sm = 0, sl = 0, sr = 0;
    rms_t o;
    stage_rest(sel);
    for (i = 0; i < tot; i += CTL) {
        for (k = 0; k < CTL; k++) {
            double s = sin(6.283185307179586 * f * (double)(i + k) / (double)FS);
            mix_l[k] = (int32_t)lrint(lt * s);
            mix_r[k] = (int32_t)lrint(rt * s);
        }
        stage_block(CTL);
        if (i >= warm)
            for (k = 0; k < CTL; k++) {
                double s = (double)(mix_l[k] - mix_r[k]) / 2.0, m = (double)(mix_l[k] + mix_r[k]) / 2.0;
                ss += s * s, sm += m * m, sl += (double)mix_l[k] * mix_l[k], sr += (double)mix_r[k] * mix_r[k];
            }
    }
    o.side = sqrt(ss / meas), o.mid = sqrt(sm / meas), o.l = sqrt(sl / meas), o.r = sqrt(sr / meas);
    return o;
}
static double db(double a, double b) { return 20.0 * log10(a / b); }

static void test_stage(void)
{
    uint32_t i, k, sel, nbad;
    int32_t tl[CTL], tr[CTL];
    int ok;
    double worst = 0;
    printf("bassmono: the stage (fx.c bass_mono, %u corners, table check against g = tan(pi f / FS))\n", (unsigned)BM_N);
    ok = 1;
    for (sel = 0; sel < BM_N; sel++) {                  /* the table: the constants are the formula's */
        double g = tan(3.14159265358979323846 * BM_HZ[sel] / (double)FS), kk = sqrt(2.0), a1 = 1.0 / (1.0 + g * (g + kk));
        double w[3] = {a1, g * a1, g * g * a1};
        for (k = 0; k < 3u; k++)
            ok &= fabs((double)BM_C[sel][k] / 1073741824.0 - w[k]) <= 1.0 / 1073741824.0 + 1e-12;
    }
    expect("the coefficient table is a1 a2 a3 of the trapezoidal Butterworth high-pass at 40 .. 400 Hz (Q30, each within 1 LSB)", ok);

    /* L + R: untouched, sample for sample, noise in both channels (and the side), each setting, the fade-in and a change of setting inside */
    nbad = 0;
    for (sel = 1; sel <= BM_N; sel++) {
        stage_rest(sel);
        for (i = 0; i < 6000u; i++) {                   /* ~8 s of blocks is not needed: the fade-in and the steady state both */
            for (k = 0; k < CTL; k++) {
                mix_l[k] = tl[k] = noise(40000) + (int32_t)(6000.0 * sin(0.0143 * (double)(i * CTL + k)));
                mix_r[k] = tr[k] = noise(40000) - (int32_t)(6000.0 * sin(0.0143 * (double)(i * CTL + k)));
            }
            if (i == 3000u)
                bm_sel = (uint8_t)(sel % BM_N + 1u);
            stage_block(CTL);
            for (k = 0; k < CTL; k++)
                nbad += (mix_l[k] + mix_r[k]) != (tl[k] + tr[k]);
        }
    }
    expect("L + R is not touched: 66 x 6000 blocks of noise and a side tone, every setting and a change inside: the sum equal in every sample", nbad == 0u);

    /* in phase: bit for bit */
    nbad = 0;
    stage_rest(1);
    for (i = 0; i < 4000u; i++) {
        for (k = 0; k < CTL; k++)
            mix_l[k] = mix_r[k] = tl[k] = noise(60000);
        stage_block(CTL);
        for (k = 0; k < CTL; k++)
            nbad += mix_l[k] != tl[k] || mix_r[k] != tl[k];
    }
    expect("a mono signal (L = R, noise) passes bit for bit at 40 Hz (the side is exactly 0)", nbad == 0u);

    {   /* the two tones of the request, at 160 Hz and 100 Hz */
        rms_t a = tone_run(7, 40.0, 8000, -8000, 4410, 2 * SEC), b = tone_run(7, 2000.0, 8000, -8000, 4410, 2 * SEC);
        rms_t c = tone_run(5, 40.0, 8000, -8000, 4410, 2 * SEC), d = tone_run(5, 2000.0, 8000, -8000, 4410, 2 * SEC);
        double in_rms = 8000.0 / sqrt(2.0);
        printf("bassmono:   40 Hz out of phase at BASS MONO 160 Hz: L %.1f R %.1f (side %.2f of %.0f) = %.1f dB; at 100 Hz %.1f dB\n", a.l, a.r, a.side, in_rms,
               db(a.side, in_rms), db(c.side, in_rms));
        expect("40 Hz, out of phase, BASS MONO 160 Hz: the side is gone (down >= 23 dB: mono); the mid is 0 as it was", db(a.side, in_rms) <= -23.0 && a.mid < 1.0);
        expect("40 Hz, out of phase, BASS MONO 100 Hz: the side down >= 15.5 dB (12 dB / oct: -16.0 dB)", db(c.side, in_rms) <= -15.5 && db(c.side, in_rms) >= -16.6);
        expect("2 kHz, out of phase, BASS MONO 160 Hz: passes unchanged (within 0.01 dB), 100 Hz alike",
               fabs(db(b.side, in_rms)) < 0.01 && fabs(db(d.side, in_rms)) < 0.01 && fabs(db(b.l, in_rms)) < 0.01);
    }
    {   /* the corner against the printed Hz, and the slope */
        char w[160];
        double wc = 0, w2 = 0, w4 = 0;
        ok = 1;
        for (sel = 1; sel <= BM_N; sel++) {
            double f = BM_HZ[sel - 1], in = 8000.0 * 2.0 / 2.0 / sqrt(2.0) * 1.0;
            rms_t at = tone_run(sel, f, 8000, -8000, 6u * FS / 10u, 2 * SEC);
            double e = db(at.side, in) + 3.0103;
            wc = e > wc ? e : wc < -e ? -e : wc;
            worst = fabs(e) > worst ? fabs(e) : worst;
            if (fabs(e) >= 0.5)
                ok = 0;
        }
        snprintf(w, sizeof w, "the -3 dB corner of the side within 0.5 dB of the printed Hz at all %u settings (worst %.3f dB off)", (unsigned)BM_N, worst);
        expect(w, ok);
        ok = 1;
        for (sel = 1; sel <= BM_N; sel++) {
            double f = BM_HZ[sel - 1], in = 8000.0 / sqrt(2.0) * 1.0;
            rms_t lo = tone_run(sel, f / 2.0, 8000, -8000, 6u * FS / 10u + 2u * FS / 5u, 4 * SEC), hi = tone_run(sel, f * 4.0, 8000, -8000, 6u * FS / 10u, 2 * SEC);
            double e2 = db(lo.side, in) - 10.0 * log10(0.0625 / 1.0625), e4 = db(hi.side, in);
            w2 = fabs(e2) > w2 ? fabs(e2) : w2;
            w4 = fabs(e4) > w4 ? fabs(e4) : w4;
            ok &= fabs(e2) < 0.5 && fabs(e4) < 0.05;
        }
        snprintf(w, sizeof w, "an octave below the corner -12.3 dB (worst %.2f dB off), two octaves above unchanged (worst %.3f dB)", w2, w4);
        expect(w, ok);
        (void)wc;
    }
    {   /* a DC side is removed; loud / off-scale input: nothing wraps */
        int32_t mx = 0;
        stage_rest(3);
        for (i = 0; i < 4u * SEC / CTL; i++) {
            for (k = 0; k < CTL; k++)
                mix_l[k] = 3000, mix_r[k] = -3000;
            stage_block(CTL);
        }
        expect("a DC in the side (L = +3000, R = -3000) is gone after 4 s (both channels within 1 of 0)",
               abs(mix_l[0]) <= 1 && abs(mix_r[0]) <= 1 && abs(mix_l[CTL - 1]) <= 1);
        stage_rest(11);
        for (i = 0; i < 3000u; i++) {
            for (k = 0; k < CTL; k++) {
                mix_l[k] = (noise(1) & 1) ? 2000000000 : -2000000000;
                mix_r[k] = -mix_l[k];
            }
            stage_block(CTL);
            for (k = 0; k < CTL; k++) {
                int32_t m = abs(mix_l[k]);
                mx = m > mx ? m : mx;
            }
        }
        expect("off-scale input (+-2e9, side clamped at 8 x full scale): finite, no wrap (|out| stays within the input's own range)", mx <= 2000000000 + (1 << 20) && mx >= 1);
        stage_rest(11);
        for (i = 0; i < 3000u; i++) {                   /* loud noise: the output bounded by the input + the filter's own part (<= 1 x the clamp) */
            for (k = 0; k < CTL; k++) {
                mix_l[k] = noise(300000);
                mix_r[k] = noise(300000);
            }
            stage_block(CTL);
            for (k = 0; k < CTL; k++) {
                int32_t m = abs(mix_l[k]) > abs(mix_r[k]) ? abs(mix_l[k]) : abs(mix_r[k]);
                mx = m > mx ? m : mx;
            }
        }
        expect("loud noise (+-300000, 9 x full scale): the filter is stable (bounded output)", mx < 2000000000 + (1 << 20));
    }
    {   /* silence: the sound stops, the output is exactly 0, and after OFF's fade the state is dropped */
        uint32_t z = 0;
        stage_rest(5);
        for (i = 0; i < 2u * SEC / CTL; i++) {
            for (k = 0; k < CTL; k++) {
                mix_l[k] = noise(20000);
                mix_r[k] = noise(20000);
            }
            stage_block(CTL);
        }
        for (i = 0; i < 2u * SEC / CTL; i++) {
            memset(mix_l, 0, sizeof mix_l);
            memset(mix_r, 0, sizeof mix_r);
            stage_block(CTL);
        }
        for (k = 0; k < CTL; k++)
            z |= (uint32_t)(mix_l[k] | mix_r[k]);
        expect("after the sound stops (2 s of zeros), the stage's output is exactly 0 (no stalled state, no DC)", z == 0u);
        bm_sel = 0;
        for (i = 0; i < 40u; i++) {
            for (k = 0; k < CTL; k++)
                mix_l[k] = noise(20000), mix_r[k] = noise(20000);
            stage_block(CTL);
        }
        expect("OFF: the share fades out (23 ms), then the stage is at rest (share 0, states dropped), the gate stays shut", bm.a == 0 && bm.z0 == 0 && bm.z1 == 0 && !(bm_sel || bm.a));
    }
}

/* ------------------------------------------------------------------ no click --- */
static int32_t ol[10 * SEC / 2], orr[10 * SEC / 2];
static void test_click(void)
{
    uint32_t i, k, n = 0, a, b, nbad = 0, sels[8];
    double sig_slope = 0, worst = 0;
    printf("bassmono: no click\n");
    /* a bass side (40 Hz + 63 Hz + a 100 Hz, near full scale) with a mid on top; the setting walks round while it sounds */
    sels[0] = 1, sels[1] = 11, sels[2] = 3, sels[3] = 9, sels[4] = 0, sels[5] = 6, sels[6] = 0, sels[7] = 11;
    stage_rest(sels[0]);
    for (a = 0; a < 8u; a++) {
        bm_sel = (uint8_t)sels[a];
        for (b = 0; b < 3000u / CTL * 8u; b++) {        /* ~0.54 s each */
            for (k = 0; k < CTL; k++, n++) {
                double s = 9000.0 * sin(6.283185307179586 * 40.0 * n / FS) + 7000.0 * sin(6.283185307179586 * 63.0 * n / FS) + 5000.0 * sin(6.283185307179586 * 100.0 * n / FS);
                double m = 6000.0 * sin(6.283185307179586 * 700.0 * n / FS);
                mix_l[k] = (int32_t)lrint(m + s);
                mix_r[k] = (int32_t)lrint(m - s);
            }
            for (k = 0; k < CTL; k++) {
                ol[n - CTL + k] = mix_l[k];
                orr[n - CTL + k] = mix_r[k];
            }
            stage_block(CTL);
            for (k = 0; k < CTL; k++) {
                ol[n - CTL + k] = mix_l[k] - ol[n - CTL + k];     /* (kept: the output minus the input: what the stage did) */
                orr[n - CTL + k] = mix_r[k] - orr[n - CTL + k];
            }
        }
    }
    /* the stage's own change (the removed part d/2 added to the channels) must itself move smoothly: its largest step against the largest slope of the side */
    for (i = 1; i < n; i++) {
        double d = fabs((double)(ol[i] - ol[i - 1]));
        worst = d > worst ? d : worst;
    }
    sig_slope = 2.0 * 3.141592653589793 * (9000.0 * 40.0 + 7000.0 * 63.0 + 5000.0 * 100.0) / FS;   /* the side's steepest possible slope a sample */
    printf("bassmono:   the stage's own change: largest step %.1f a sample; the side's steepest slope %.1f; walk 40 -> 400 -> 63 -> 250 -> OFF -> 125 -> OFF -> 400\n", worst, sig_slope);
    expect("the setting walked over its range (jumps 1 -> 11 -> 3 -> 9 -> OFF -> 6 -> OFF -> 11) with a bass side sounding: the stage's change never steps faster than 1.3 x the side's steepest slope (the removed part, the complement of the high-pass, peaks +1.8 dB at the corner)",
           worst <= 1.3 * sig_slope);
    (void)nbad;
    /* ON <-> OFF alone, a plain 50 Hz side at 20000 */
    n = 0;
    worst = 0;
    stage_rest(0);
    for (a = 0; a < 6u; a++) {
        bm_sel = (uint8_t)((a & 1u) ? 0u : 5u);
        for (b = 0; b < 2000u / CTL * 8u; b++) {
            for (k = 0; k < CTL; k++, n++) {
                double s = 20000.0 * sin(6.283185307179586 * 50.0 * n / FS);
                mix_l[k] = (int32_t)lrint(s);
                mix_r[k] = (int32_t)lrint(-s);
                ol[n] = mix_l[k];
            }
            stage_block(CTL);
            for (k = 0; k < CTL; k++)
                orr[n - CTL + k] = mix_l[k];
        }
    }
    for (i = 1; i < n; i++) {
        double d = fabs((double)(orr[i] - orr[i - 1]));
        worst = d > worst ? d : worst;
    }
    printf("bassmono:   ON / OFF x6 on a 50 Hz side of 20000: largest output step %.1f a sample (the tone's steepest slope %.1f)\n", worst,
           2.0 * 3.141592653589793 * 20000.0 * 50.0 / FS);
    expect("OFF <-> ON six times while the side sounds: the output never steps faster than 1.3 x the tone's own slope (the 23 ms fade)", worst <= 2.0 * 3.141592653589793 * 20000.0 * 50.0 / FS * 1.3);
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
    host_tracks_init();
    song.sel = 0;
    song.playing = 0;
    song.grid = 0;
}
#define SCN_N (3u * FS / CTL * CTL)
static int32_t rl[SCN_N], rr[SCN_N], ml_[SCN_N], mr_[SCN_N];   /* the outputs; mix_l / mix_r after the stage (before the master) */
/* mode_scn.h's render (two parts, SPREAD, every send, HALL) in mode, recorded; BASS MONO sel0 from the start, sel1 from block `at` on */
static void scene(uint32_t mode, uint32_t sel0, uint32_t sel1, uint32_t at)
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
    bm_sel = (uint8_t)sel0;
    for (b = 0; b < nb; b++) {
        if (b == at)
            bm_sel = (uint8_t)sel1;
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
    bm_sel = 0;
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
static double side_energy(const int32_t *l, const int32_t *r, uint32_t a, uint32_t b)
{
    double e = 0;
    uint32_t i;
    for (i = a; i < b; i++) {
        double s = (double)(l[i] - r[i]);
        e += s * s;
    }
    return e;
}

static void test_mix(void)
{
    uint64_t h0, h1;
    uint32_t i, same = 1, nd;
    double e_off, e_on;
    printf("bassmono: the whole mix\n");
    scene(DM_STEREO, 0, 0, ~0u);
    h0 = hash_of();
    expect("OFF is bit for bit what it was: the busy two-part render (STEREO) hashes to the sources-before-BASS-MONO hash", h0 == MODE_STEREO_HASH);
    memcpy(keep_l, rl, sizeof keep_l), memcpy(keep_r, rr, sizeof keep_r), memcpy(keep_ml, ml_, sizeof keep_ml), memcpy(keep_mr, mr_, sizeof keep_mr);

    scene(DM_STEREO, 5, 5, ~0u);
    h1 = hash_of();
    e_off = side_energy(keep_l, keep_r, SCN_N / 4u, SCN_N);
    e_on = side_energy(rl, rr, SCN_N / 4u, SCN_N);
    expect("ON (100 Hz): the render differs from OFF (the stage runs in STEREO), the side's energy is lower", h1 != h0 && e_on < e_off);
    printf("bassmono:   side energy ON / OFF %.3f (the scene's low side)\n", e_on / e_off);
    {   /* the mono sum: mix_block's L + R is the master of the sum, nonlinear after (limiter, soft clip): not identical; compare the fade/return below */
        scene(DM_STEREO, 5, 0, 1500u);                  /* ON, then OFF at block 1500 (1.09 s): after the fade the render is OFF's, bit for bit */
        for (i = 1500u * CTL + 3000u; i < SCN_N; i++)
            same &= ml_[i] == keep_ml[i] && mr_[i] == keep_mr[i];
        expect("ON then OFF at 1.09 s: after the 23 ms fade (+ 3000 samples) the mix before the master is the never-ON mix's, bit for bit", same);
        for (i = 3u * SCN_N / 4u, nd = 0; i < SCN_N; i++)
            nd += rl[i] != keep_l[i] || rr[i] != keep_r[i];
        printf("bassmono:   (the output of that render: %u of the last %u samples differ from the never-ON render: the limiter's memory of the loud side, not the stage)\n", nd, SCN_N / 4u);
        scene(DM_STEREO, 0, 5, 1200u);                  /* OFF, ON at block 1200: before it, bit for bit OFF's */
        same = 1;
        for (i = 0; i < 1200u * CTL; i++)
            same &= rl[i] == keep_l[i] && rr[i] == keep_r[i];
        expect("OFF, ON at 0.87 s: until the switch every sample is OFF's, bit for bit", same);
    }
    scene(DM_LAYER, 0, 0, ~0u);
    memcpy(keep_l, rl, sizeof keep_l), memcpy(keep_r, rr, sizeof keep_r);
    scene(DM_LAYER, 5, 5, ~0u);
    expect("LAYER runs it too (the render with 100 Hz differs from LAYER's OFF)", memcmp(keep_l, rl, sizeof rl) != 0);

    scene(DM_SPLIT, 0, 0, ~0u);
    memcpy(keep_l, rl, sizeof keep_l), memcpy(keep_r, rr, sizeof keep_r);
    scene(DM_SPLIT, 5, 5, ~0u);
    expect("DUEL: BASS MONO set (100 Hz) does nothing: the render is bit for bit the one without it (part 1 left, part 2 right)",
           !memcmp(keep_l, rl, sizeof rl) && !memcmp(keep_r, rr, sizeof rr));
    scene(DM_SPLIT, 11, 11, ~0u);
    expect("  .. at 400 Hz too; and the stage holds no state in DUEL (share 0)", !memcmp(keep_l, rl, sizeof rl) && !memcmp(keep_r, rr, sizeof rr) && bm.a == 0);
    {   /* the mode changed under it: STEREO (running) -> DUEL: cleared at the switch, back to STEREO: faded in from rest */
        static int32_t o[2 * CTL];
        uint32_t b;
        dsp_clean(DM_STEREO);
        host_preset(&trk[0], scn_eng("ANALOG"), 1);
        trk[0].p[P_SPRD] = 100;
        trk[0].p[P_VOICE] = V_POLY;
        trk[0].p[P_TRIM] = 120;
        bm_sel = 5;
        trk_note_on(&trk[0], 40, 120);
        for (b = 0; b < 400u; b++)
            mix_block(o, CTL);
        expect("STEREO, BASS MONO 100 Hz, a spread note sounding: the stage is running (share Q15 full)", bm.a == 32768);
        duell_mode_force(DM_SPLIT);
        expect("a change to DUEL clears the stage (share 0, states 0)", bm.a == 0 && bm.z0 == 0 && bm.z1 == 0);
        for (b = 0; b < 100u; b++)
            mix_block(o, CTL);
        expect("  .. and in DUEL it stays at rest", bm.a == 0);
        duell_mode_force(DM_STEREO);
        for (b = 0; b < 100u; b++)
            mix_block(o, CTL);
        expect("back in STEREO it runs again from rest (the share fades in: full after 1024 samples)", bm.a == 32768);
    }
    bm_sel = 0;
    memset(&bm, 0, sizeof bm);
}

/* ------------------------------------------------------------------ the control --- */
static uint32_t master_page(void)
{
    uint32_t i;
    for (i = 0; i < NPAGES && !str_eq(PAGES[i].title, MASTER_PAGE_TITLE); i++)
        ;
    return i;
}
static void test_control(void)
{
    uint32_t v, r, ok, mp;
    printf("bassmono: the control (the master-stage page, song.bm; no MENU row)\n");
    memset(nor, 0xFF, sizeof nor);
    boot();
    duell_mode_force(DM_STEREO);
    mp = master_page();
    expect("a fresh unit: BASS MONO ON at 200 Hz (0.2.24: song.bm 8 = BM_HZ[7], bm_sel 8 on the audio side; nothing written for it until it is touched)", song.bm == 8 && BM_HZ[7] == 200u && BM_DEF == 8 && bm_sel == 8u && ui_ms_set[3] == 0u);
    expect("the master page STEREO MASTER exists (0.2.25 layout round): GLO family, after SHIMMER and before COMPRESSOR, SC_GLOBAL only (no part's control), K1 BASS MONO, K2 WIDTH, K3 K4 empty", mp < NPAGES && PAGES[mp].fam == FAM_GLO &&
           PAGES[mp].scope == SC_GLOBAL && PAGES[mp].graph == GR_MSTG && PAGES[mp].id[0] == G_BM && PAGES[mp].id[1] == G_WD && PAGES[mp].id[2] == 0xFFu && PAGES[mp].id[3] == 0xFFu &&
           str_eq(PAGES[mp].title, "STEREO MASTER") && str_eq(PAGES[mp - 1u].title, "SHIMMER") && str_eq(PAGES[mp + 1u].title, "COMPRESSOR") && page_global(&PAGES[mp]));
    {
        const param_desc_t *d = glob_desc(G_BM);
        int16_t *vp;
        char val[16];
        const char *unit;
        ok = d == &GP[G_BM] && str_eq(d->label, "BMONO") && d->fmt == F_BMONO && d->min == 0 && d->max == (int16_t)BM_N && d->def == BM_DEF &&
             page_desc(&PAGES[mp], 0, &vp) == d && vp == &song.bm;
        unit = "";
        param_format(d, 0, val, &unit);
        ok &= !strcmp(val, "OFF") && !unit[0];
        for (v = 1; v <= BM_N; v++) {
            char want[16];
            snprintf(want, sizeof want, "%u", (unsigned)BM_HZ[v - 1]);
            unit = "";
            param_format(d, (int32_t)v, val, &unit);
            ok &= !strcmp(val, want) && !strcmp(unit, "Hz");
        }
        ok &= str_eq(desc_shown(d), "BASS\nMONO");
        expect("the card: the stored label \"BMONO\", shown BASS / MONO (two lines, 0.2.25 layout round), OFF + the 11 corners shown as the number and Hz (BM_HZ), on song.bm", ok);
    }
    ok = 1;
    for (r = 0; r < MI_COUNT; r++)
        ok &= strcmp(MI_NAME[r], "BASS MONO") != 0 && strcmp(MI_NAME[r], "OUTPUT EQ") != 0;
    expect("no MENU row is called BASS MONO or OUTPUT EQ; the AUDIO tab: CLICK, CLICK LEVEL, COUNT-IN, TUNE (4 rows, one page; 0.2.25: USB LEVEL removed)",
           ok && mtab_rows(MTAB_AUDIO) == 4u && mtab_pages(MTAB_AUDIO) == 1u && mtab_first(MTAB_AUDIO) == MI_CLICK && menu_place(MI_TUNE, &r) == 0u && r == 3u);

    /* the knob */
    go_title(MASTER_PAGE_TITLE);
    expect("STEREO: the page is visible and the card is drawn", page_visible(mp) && !card_hidden_vp(&song.bm) && ui.page == mp);
    song.bm = 0;
    frame();
    turn(EN_K1, 1);
    frame();
    expect("K1 right: 40 Hz (one corner a detent), the audio side after the next frame; the header says \"BASS MONO 40 Hz\" (the card's label is BMONO)", song.bm == 1 && bm_sel == 1u && str_eq(ui.msg, "BASS MONO 40 Hz"));
    song.bm = 6;
    frame();
    ok = bm_sel == 6u;
    turn(EN_K1, 1);
    ok &= song.bm == 7;
    turn(EN_K1, -1);
    turn(EN_K1, -1);
    ok &= song.bm == 5;
    for (v = 0; v < 30u; v++)
        turn(EN_K1, -1);
    ok &= song.bm == 0;
    for (v = 0; v < 30u; v++)
        turn(EN_K1, 1);
    ok &= song.bm == (int16_t)BM_N;
    expect("the knob steps one corner a detent and stops at both ends (OFF .. 400 Hz)", ok);
    song.bm = 0;

    /* DUEL: the card is not there (the page stays: it is the master-stage page, BASS MONO is its only card for now) */
    song.bm = 5;
    duell_mode_force(DM_SPLIT);
    expect("DUEL: the card is hidden (not drawn, its knob does nothing)", card_hidden_vp(&song.bm));
    go_title(MASTER_PAGE_TITLE);
    turn(EN_K1, 1);
    expect("  .. K1 changes nothing in DUEL; the value is kept for STEREO and the stage does not run (fx.c, test 4)", song.bm == 5);
    duell_mode_force(DM_LAYER);
    expect("LAYER: the card is there again", !card_hidden_vp(&song.bm));
    duell_mode_force(DM_STEREO);
    ok = 0;
    {   /* SELECT in the GLO round reaches it from REVERB (SHIMMER is skipped unless TYPE is SHIM) */
        go_title("REVERB");
        turn(EN_SELECT, 1);
        ok = ui.page == mp;                             /* (0.2.25 layout round: STEREO MASTER (BASS MONO, WIDTH) where WIDTH and MASTER were, then COMPRESSOR) */
        turn(EN_SELECT, 1);
        ok &= str_eq(cur_page()->title, "COMPRESSOR");  /* (0.2.21: the master compressor's page follows) */
        turn(EN_SELECT, 1);
        ok &= str_eq(cur_page()->title, "MODULES");
    }
    expect("STEREO: the GLO round (SELECT) goes REVERB, STEREO MASTER, COMPRESSOR, MODULES", ok);

    /* a DEVICE setting (0.2.24): the settings record's byte [15][21], stored + 1 (0 = the default, ON at 200 Hz) */
    boot();
    duell_mode_force(DM_STEREO);
    song.bm = 5;
    frame();
    fm1_ms += 2100u;
    frame();
    flush();
    {
        static persist_t p;
        ok = st_load(OBJ_SETTINGS, &p, sizeof p) > 0 && p.favorites.factory[15][18] == 0xB7u && p.favorites.factory[15][21] == 6u && p.favorites.factory[15][29] == 0u;
        expect("a change is saved ~2 s later in the settings record: byte [15][21] = setting + 1 (6 = 100 Hz), under the WIDTH / DRIVE tag; [15][29] (the retired STYLE) stays 0", ok);
        boot();
        expect("  .. a restart reads it back (100 Hz), the audio side follows", song.bm == 5 && bm_sel == 5u);
        p.favorites.factory[15][21] = 1u;
        st_save(OBJ_SETTINGS, &p, sizeof p);
        boot();
        expect("  .. stored 1 is OFF (the user's own choice stays OFF, it is not the default)", song.bm == 0 && bm_sel == 0u);
        ok = 1;
        for (v = 0; v < 256u; v++) {                    /* every byte value: in range; 0 and past BM_N + 1 are the default */
            p.favorites.factory[15][21] = (uint8_t)v;
            st_save(OBJ_SETTINGS, &p, sizeof p);
            boot();
            ok &= song.bm >= 0 && song.bm <= (int16_t)BM_N && bm_sel <= BM_N && song.bm == (v >= 1u && v <= BM_N + 1u ? (int16_t)v - 1 : (int16_t)BM_DEF);
        }
        expect("every byte value 0..255 in [15][21]: in range; 0 and anything past 12 read the default (200 Hz)", ok);
        memset(&p, 0, sizeof p);
        ok = st_load(OBJ_SETTINGS, &p, sizeof p) > 0;
        for (v = 0; v < 256u; v++) {                    /* whatever an early 0.2.19 build left in [15][29] is ignored */
            p.favorites.factory[15][21] = 0;
            p.favorites.factory[15][29] = (uint8_t)v;
            st_save(OBJ_SETTINGS, &p, sizeof p);
            boot();
            ok &= song.bm == (int16_t)BM_DEF && bm_sel == (uint8_t)BM_DEF;
        }
        expect("every byte value 0..255 in [15][29] (STYLE's 0 and 1, an early build's 0x80 + n): ignored, the default", ok);
        memset(&p, 0, sizeof p);
        p.favorites.factory[15][21] = 3;                /* a record with the byte set but no tag: the tag decides (an older key map left bytes there) */
        st_save(OBJ_SETTINGS, &p, sizeof p);
        boot();
        expect("  .. a byte [15][21] without the tag byte [15][18] is no setting: the default", song.bm == (int16_t)BM_DEF);
    }
    memset(nor, 0xFF, sizeof nor);
    boot();
    expect("after ERASE DATA (no settings, the restart): ON at 200 Hz again", song.bm == (int16_t)BM_DEF && bm_sel == (uint8_t)BM_DEF);

    /* the multis do not carry it (0.2.21, owner: a master effect is the device's, not part of a multi) */
    boot();
    duell_mode_force(DM_STEREO);
    song.bm = 5;
    project_save_as(10, "BM TEST");                     /* M11 (saved over its factory multi) */
    song.bm = 0;
    project_load(10);
    expect("a multi saved with BASS MONO 100 Hz does not bring it back on load (song.bm stays 0, as the unit has it)", song.bm == 0);
    song.bm = 8;
    project_load(10);
    expect("  .. and a load leaves the unit's own 315 Hz as it is", song.bm == 8);
    {
        project_t q;
        uint32_t cut;
        uint8_t blk[NSHOWN][MB_N], rec[MU_REC_MAX];
        uint32_t len;
        memset(blk, 0, sizeof blk);
        song.bm = 5;
        project_capture(&q);
        len = mu_pack(rec, &q, blk, &cut);
        ok = len > MU_G_AT && q.bm == 0 && rec[5] == (uint8_t)(morph.target == MT_BOTH);
        expect("a save writes byte 5 bits 1..4 as 0 and project byte 65 as 0 (BASS MONO 100 Hz on the unit)", ok);
        rec[5] = (uint8_t)((rec[5] & 1u) | 7u << 1);    /* a record of 0.2.19 / 0.2.20 with BASS MONO 125 Hz in it */
        {
            uint8_t amt[NSHOWN], set[6];
            project_t q2;
            project_capture(&q2);
            q2.bm = 3;
            ok = mu_unpack(&q2, blk, amt, set, rec, len) && q2.bm == 3u;
            rec[5] = (uint8_t)((rec[5] & 1u) | 12u << 1);   /* a code past BM_N */
            ok &= mu_unpack(&q2, blk, amt, set, rec, len) && q2.bm == 3u;
            rec[5] |= 0x20u;                            /* a reserved bit set: not a multi this firmware loads */
            ok &= !mu_unpack(&q2, blk, amt, set, rec, len);
        }
        expect("  .. an older record with BASS MONO in it loads with the unit's value kept (any code); a stray bit 5..7 in byte 5 is still no multi", ok);
    }
    song.bm = 0;
    project_save_as(11, "BM OFF");
    song.bm = 9;
    project_load(11);
    expect("a multi saved with OFF leaves the 400 Hz that was set", song.bm == 9);
    ok = 1;
    for (v = 0; v < DF_N; v++) {                        /* the factory multis: byte 5 holds no BASS MONO bit */
        ok &= (DM_REC[DM_AT[v] + 5u] >> 1) == 0u;
    }
    {
        uint32_t k;
        for (k = 0; k < 16u && ok; k++) {
            if (df_of(k) >= DF_N)
                continue;
            song.bm = 7;
            project_load(k);
            ok &= song.bm == 7;
        }
    }
    expect("every factory multi (M01.., byte 5 bits 1..4 clear) leaves BASS MONO as it is", ok);

    /* the project: byte 65 is written 0 and not read (the format keeps its size): BASS MONO is the unit's (0.2.24) */
    boot();
    duell_mode_force(DM_STEREO);
    song.bm = 8;
    {
        project_t q;
        static project_store_t store;
        project_capture(&q);
        ok = q.bm == 0 && proj_pack(&store, &q) && store.raw[65] == 0u;
        memset(&q, 0, sizeof q);
        ok &= proj_import(&q, store.raw, (int)sizeof store) && q.bm == 0u;
        store.raw[65] = 12;                             /* a code past BM_N: the hash no longer matches, so it is no project */
        ok &= !proj_import(&q, store.raw, (int)sizeof store);
        expect("the project writes 0 in byte 65 (it held BASS MONO up to 0.2.23; the format is the same); packed, imported, the hash covers the byte", ok);
    }
    {
        uint32_t s8 = autosave_sig();
        song.bm = 0;
        expect("  .. the autosave does NOT see a change of it (a device setting: autosave_sig is the same)", autosave_sig() == s8);
    }
    ok = 1;
    song.bm = 3;
    {
        project_t q;
        project_capture(&q);
        q.bm = 4;
        q.sum = proj_sum(&q);
        project_restore_runtime(&q);
        ok &= song.bm == 3;
        q.bm = 200;
        q.sum = proj_sum(&q);
        project_restore_runtime(&q);
        ok &= song.bm == 3;
        expect("project_restore_runtime leaves the unit's BASS MONO as it is (3 stays 3)", ok);
    }
    song.bm = 0;
    boot();
}

/* ------------------------------------------------------------------ cost --- */
static void test_cost(void)
{
    uint32_t i, k, reps = 20000u;
    uint64_t t0, t1, base, st, gl;
    printf("bassmono: cost (host instructions per sample of the stage, one channel pair)\n");
    stage_rest(5);
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
        bass_mono(CTL);
    }
    t1 = instr_now();
    st = t1 - t0 - base;
    t0 = instr_now();
    for (i = 0; i < reps; i++) {
        for (k = 0; k < CTL; k++)
            mix_l[k] = noise(20000), mix_r[k] = noise(20000);
        bm_sel = (uint8_t)(1u + (i & 1u) * 10u);        /* the corner moving every block: the gliding loop */
        bass_mono(CTL);
    }
    t1 = instr_now();
    gl = t1 - t0 - base;
    if (st && gl) {
        printf("bassmono:   steady %.1f, gliding %.1f host instructions per sample (a block of %u: %.0f / %.0f)\n", (double)st / reps / CTL, (double)gl / reps / CTL, CTL,
               (double)st / reps, (double)gl / reps);
        expect("the stage costs under 60 host instructions per sample steady, under 80 gliding (the mix_block gate when OFF: two compares a block)",
               (double)st / reps / CTL < 60.0 && (double)gl / reps / CTL < 80.0);
    } else {
        printf("bassmono:   (no instruction counter on this host: cost not measured)\n");
    }
    bm_sel = 0;
    memset(&bm, 0, sizeof bm);
}

int main(void)
{
    uint64_t h = mode_stereo_scenario();                /* (the process's first render: the baseline hash, before anything else touches the DSP) */
    printf("bassmono: STEREO scenario hash 0x%016llX (before BASS MONO: 0x%016llX)\n", (unsigned long long)h, (unsigned long long)MODE_STEREO_HASH);
    expect("the busy two-part render (the process's first, BASS MONO OFF) is bit for bit the one of the sources before BASS MONO", h == MODE_STEREO_HASH);
    test_stage();
    test_click();
    test_mix();
    test_control();
    test_cost();
    printf("bassmono: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
