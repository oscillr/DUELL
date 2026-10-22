/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.23 WIDTH, a master stage (fx.c width_stage; params.c pages WIDTH, MASTER; core.h G_WD / song.ms; ui.c ms_sync; stored in the settings record's bytes [15][18..21]).
 * 0.2.25: the master saturator of 0.2.23 / 0.2.24 is removed (its tests with it); a stored DRIVE byte is ignored; the page MASTER holds BASS MONO alone.
 * On the firmware's sources (the DSP of hostsim.c and the UI of ui_test.c), against simulated NOR flash.
 *   build/host/master_test             (host_tests.sh master)
 * 1. neutral is bit for bit what it was: the busy two-part render (mode_scn.h, the process's first) hashes to MODE_STEREO_HASH; WIDTH 100 % is not run (zero cost).
 * 2. WIDTH: the mid (L + R) is never touched (exactly, any setting); 0 % = mono; the side's gain against frequency at 200 % (bass below the 125 Hz crossover barely moved, +6 dB above it)
 *    and at 50 % (the whole side, the bass too); no click (100 <-> 200 <-> 0 while a tone sounds); back at 100 % the stage rests and the mix is the never-ON mix bit for bit; BASS MONO after it
 *    keeps the low end mono; DUEL does nothing (and a change of the mode clears it); LAYER runs it.
 * 3. the control: the page (GLO > STEREO MASTER: K1 BASS MONO, K2 WIDTH, 0.2.25 layout round; then COMPRESSOR), the card, its values ("100" %), the knob, the DUEL hide rule and the panel's
 *    note, the stored bytes (a tag, WIDTH, a byte that is ignored; saved ~2 s later, power-on, ERASE DATA, every byte value read safely, an older record = the defaults), not in the multis / project, the
 *    stages' place in mix_block.
 * 4. cost, host instructions per sample (one channel pair). */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"
#include "mode_scn.h"
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif

#define MODE_STEREO_HASH 0xC04494F53D46F0F0ull   /* 0.2.30: merged line (cloud wave 6 + click fixes), re-based on purpose, was 0x1CC6C8241C84C0DA; before:  0.2.29: re-based on purpose, the scenario runs the HALL and its quiet lines now round to the nearest (fx.c hall_end; 0.2.21..0.2.28: 0xC970174114DFD9BA) */   /* the same constant as mode_test.c, bassmono_test.c and comp_test.c */

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
    printf("master: %-124s %s\n", what, ok ? "ok" : "FAIL");
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
    cmp_set = 0;
    cmp_last = 0xFFFFFFFFu;
    memset(song.cp, 0, sizeof song.cp);
    ms_last = 0xFFFFFFFFu;                              /* (the RAM is gone) */
    memset(song.ms, 0, sizeof song.ms);
    flash_ok = 1;
    persist_boot();
    settings_init();
    multi_boot();
    frame();
}
static void flush(void)
{
    uint32_t n;
    tails_over();
    for (n = 0; n < 8u; n++) {
        fm1_ms += 2000u;
        settings_poll();
    }
}
#define SEC FS
#define PI2 6.283185307179586
static double db(double a, double b) { return 20.0 * log10(a / b); }
static uint32_t xs = 0x2545F491u;
static int32_t noise(int32_t amp)
{
    xs ^= xs << 13;
    xs ^= xs >> 17;
    xs ^= xs << 5;
    return (int32_t)(((int64_t)(int32_t)xs * amp) / 2147483648LL);
}

/* ------------------------------------------------------------------ the stages alone --- */
static void wd_rest(uint32_t w) { memset(&wdn, 0, sizeof wdn); memset(wet, 0, sizeof wet); wd_sel = (uint8_t)w; }
static void wd_block(uint32_t n) { if (wd_sel != 100u || wdn.k) width_stage(n); }

/* the side's gain (dB) of a pure side tone (L = a sin, R = -a sin) at f Hz through WIDTH w; the mid's change (max |dL + dR|) */
static double side_gain(uint32_t w, double f, double amp, int32_t *mid_err)
{
    uint32_t i, k, settle = SEC / 2u, meas = SEC / 2u;
    double in = 0, out = 0;
    int32_t me = 0;
    wd_rest(100);
    wdn.k = ((int32_t)w - 100) * 32768 / 100;           /* (at the setting, no glide) */
    wd_sel = (uint8_t)w;
    for (i = 0; i < settle + meas; i += CTL) {
        int32_t l0[CTL], r0[CTL];
        for (k = 0; k < CTL; k++) {
            double s = amp * sin(PI2 * f * (double)(i + k) / (double)FS);
            l0[k] = mix_l[k] = (int32_t)lrint(s) + 3000;  /* (a DC mid: it must stay) */
            r0[k] = mix_r[k] = -(int32_t)lrint(s) + 3000;
        }
        wd_block(CTL);
        for (k = 0; k < CTL; k++) {
            int32_t e = (mix_l[k] + mix_r[k]) - (l0[k] + r0[k]);
            me = abs(e) > me ? abs(e) : me;
            if (i >= settle) {
                double a = fabs((double)(l0[k] - r0[k])), b = fabs((double)(mix_l[k] - mix_r[k]));
                in = a > in ? a : in;
                out = b > out ? b : out;
            }
        }
    }
    if (mid_err)
        *mid_err = me;
    return db(out, in);
}
static double hp2_db(double f) { double r = f / 125.0; return 20.0 * log10(r * r / sqrt(1.0 + r * r * r * r)); }   /* the crossover (2-pole Butterworth) */
static double ideal_side_db(uint32_t w, double f)       /* |1 + k H(f)| with H's phase: Butterworth HP2, s = j f / fc */
{
    double k = ((double)w - 100.0) / 100.0, r = f / 125.0, re, im, den;
    if (k <= 0)
        return 20.0 * log10(1.0 + k > 1e-9 ? 1.0 + k : 1e-9);
    /* H(jr) = -r^2 / (1 - r^2 + j sqrt2 r) */
    den = (1.0 - r * r) * (1.0 - r * r) + 2.0 * r * r;
    re = -r * r * (1.0 - r * r) / den;
    im = r * r * sqrt(2.0) * r / den;
    (void)hp2_db;
    return 20.0 * log10(sqrt((1.0 + k * re) * (1.0 + k * re) + (k * im) * (k * im)));
}
static void test_width(void)
{
    static const double F[7] = {30, 50, 80, 125, 250, 1000, 8000};
    uint32_t i, ok = 1, w;
    int32_t me, mmax = 0;
    double worst = 0;
    printf("master: WIDTH, the stage alone\n");
    for (i = 0; i < 7u; i++) {
        double g = side_gain(200, F[i], 8000.0, &me), want = ideal_side_db(200, F[i]);
        printf("master:   200 %%: side %5.0f Hz %+6.2f dB (ideal %+6.2f)\n", F[i], g, want);
        worst = fabs(g - want) > worst ? fabs(g - want) : worst;
        mmax = me > mmax ? me : mmax;
    }
    expect("200 %: the side's gain against frequency is 1 + (high-pass at 125 Hz): +6.0 dB at 1 and 8 kHz, < +0.6 dB at 50 Hz and below (within 0.15 dB of the ideal)", worst < 0.15 && side_gain(200, 50, 8000, 0) < 0.6 &&
           fabs(side_gain(200, 1000, 8000, 0) - 6.02) < 0.1);
    worst = 0;
    for (w = 0; w <= 150u; w += 25u)
        for (i = 0; i < 7u; i += 3u) {
            double g = side_gain(w, F[i], 8000.0, &me), want = w ? ideal_side_db(w, F[i]) : -99.0;
            if (w)
                worst = fabs(g - want) > worst ? fabs(g - want) : worst;
            else
                ok &= g < -60.0;
            mmax = me > mmax ? me : mmax;
        }
    expect("0 .. 75 %: the whole side scaled (the bass too: 30 Hz, 125 Hz, 8 kHz alike, within 0.15 dB), 0 % is mono (the side under -60 dB); 125 / 150 % as the ideal", ok && worst < 0.15);
    expect("  .. the mid (L + R) is not touched at any setting: exactly (a DC mid and side tones, every setting tried)", mmax == 0);
    {   /* mono stays mono at every setting: a mono source has no side */
        uint32_t k, same = 1;
        for (w = 0; w <= 200u; w += 20u) {
            wd_rest(w);
            for (i = 0; i < 100u; i++) {
                for (k = 0; k < CTL; k++)
                    mix_l[k] = mix_r[k] = noise(30000);
                wd_block(CTL);
                for (k = 0; k < CTL; k++)
                    same &= mix_l[k] == mix_r[k];
            }
        }
        expect("a mono source (L = R) comes out as it went in at every WIDTH (no side to scale)", same);
    }
    {   /* no click: 100 -> 200 -> 0 -> 100 with a 1 kHz side and mid sounding */
        static int32_t ol[3 * SEC];
        uint32_t k, n = 3u * SEC;
        double sl = 0, st = 0;
        wd_rest(100);
        for (i = 0; i < n; i += CTL) {
            wd_sel = i < SEC / 2u ? 100u : i < SEC ? 200u : i < 2u * SEC ? 0u : 100u;
            for (k = 0; k < CTL; k++) {
                double t = (double)(i + k) / FS;
                mix_l[k] = (int32_t)lrint(9000 * sin(PI2 * 1000 * t) + 6000 * sin(PI2 * 220 * t));
                mix_r[k] = (int32_t)lrint(-9000 * sin(PI2 * 1000 * t) + 6000 * sin(PI2 * 220 * t));
            }
            wd_block(CTL);
            for (k = 0; k < CTL; k++)
                ol[i + k] = mix_l[k];
        }
        for (i = 1; i < n; i++) {
            double d = fabs((double)(ol[i] - ol[i - 1]));
            st = d > st ? d : st;
        }
        sl = PI2 * 1000.0 / FS * 18000.0 + PI2 * 220.0 / FS * 6000.0;   /* (the steepest the input can be at 200 %) */
        printf("master:   WIDTH 100 -> 200 -> 0 -> 100: largest step %.0f (the signal's own slope at 200 %% %.0f)\n", st, sl);
        expect("no click: 100 -> 200 -> 0 -> 100 % while a tone sounds, no step faster than the signal's own slope (k glides 23 ms)", st <= sl * 1.02);
        expect("  .. back at 100 % the stage is at rest (k 0, the crossover's states dropped): the gate shuts", wdn.k == 0 && wdn.z0 == 0 && wdn.z1 == 0);
    }
    {   /* off-scale */
        uint32_t k;
        int32_t mx = 0;
        wd_rest(200);
        wdn.k = 32768;
        for (i = 0; i < 50u; i++) {
            for (k = 0; k < CTL; k++)
                mix_l[k] = noise(1500000), mix_r[k] = -noise(1500000);
            wd_block(CTL);
            for (k = 0; k < CTL; k++)
                mx = abs(mix_l[k]) > mx ? abs(mix_l[k]) : mx;
        }
        expect("off-scale input (+-1.5 million): no overflow (the output at most twice the input's reach)", mx < 6000000);
    }
    wd_rest(100);
}

/* ------------------------------------------------------------------ the whole mix --- */
static void dsp_clean(uint32_t mode)
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
    memset(&wdn, 0, sizeof wdn);
    cmp_cfg = 0;
    bm_sel = 0;
    wd_sel = 100;
    host_tracks_init();
    song.sel = 0;
    song.playing = 0;
    song.grid = 0;
}
#define SCN_N (3u * FS / CTL * CTL)
static int32_t rl[SCN_N], rr[SCN_N], ml_[SCN_N], mr_[SCN_N];
/* mode_scn.h's render in mode; WIDTH w0 from the start, w1 from block `at` on */
static void scene(uint32_t mode, uint32_t w0, uint32_t w1, uint32_t at)
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
    wd_sel = (uint8_t)w0;
    for (b = 0; b < nb; b++) {
        if (b == at)
            wd_sel = (uint8_t)w1;
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
    wd_sel = 100;
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
static void test_mix(void)
{
    uint64_t h0;
    uint32_t i, same, sum_same, mono;
    printf("master: the whole mix\n");
    scene(DM_STEREO, 100, 100, ~0u);
    h0 = hash_of();
    expect("WIDTH 100 % is bit for bit what it was: the busy two-part render (STEREO) hashes to the sources-before hash", h0 == MODE_STEREO_HASH && wdn.k == 0);
    memcpy(keep_l, rl, sizeof keep_l), memcpy(keep_r, rr, sizeof keep_r), memcpy(keep_ml, ml_, sizeof keep_ml), memcpy(keep_mr, mr_, sizeof keep_mr);
    /* WIDTH */
    scene(DM_STEREO, 170, 170, ~0u);
    sum_same = 1;
    for (i = 0; i < SCN_N; i++)
        sum_same &= ml_[i] + mr_[i] == keep_ml[i] + keep_mr[i];
    expect("WIDTH 170 %: the render differs, and the mix's L + R before the master is exactly the 100 % render's (only the side moved)", hash_of() != h0 && sum_same);
    scene(DM_STEREO, 0, 0, ~0u);
    mono = 1;
    for (i = 2000u; i < SCN_N; i++)
        mono &= abs(ml_[i] - mr_[i]) <= 1;
    expect("WIDTH 0 %: the whole mix is mono (L = R within 1 LSB of the pre-MASTER scale), the bass too", mono);
    scene(DM_STEREO, 200, 100, 1500u);
    same = 1;
    for (i = 1500u * CTL + 2000u; i < SCN_N; i++)
        same &= ml_[i] == keep_ml[i] && mr_[i] == keep_mr[i];
    expect("200 %, then 100 % at 1.09 s: after the glide (23 ms + 2000 samples) the mix is the never-ON mix's, bit for bit (the stage at rest)", same && wdn.k == 0);
    scene(DM_STEREO, 100, 150, 1200u);
    same = 1;
    for (i = 0; i < 1200u * CTL; i++)
        same &= rl[i] == keep_l[i] && rr[i] == keep_r[i];
    expect("100 %, then 150 % at 0.87 s: until the switch every output sample is the 100 % render's", same);
    /* LAYER */
    scene(DM_LAYER, 100, 100, ~0u);
    memcpy(keep_l, rl, sizeof keep_l);
    scene(DM_LAYER, 160, 160, ~0u);
    expect("LAYER runs WIDTH (160 % changes the render)", memcmp(keep_l, rl, sizeof rl) != 0);
    /* DUEL */
    scene(DM_SPLIT, 100, 100, ~0u);
    memcpy(keep_l, rl, sizeof keep_l), memcpy(keep_r, rr, sizeof keep_r);
    scene(DM_SPLIT, 200, 200, ~0u);
    expect("DUEL: WIDTH 200 % does nothing: the render is bit for bit the one without it (part 1 left, part 2 right); no state",
           !memcmp(keep_l, rl, sizeof rl) && !memcmp(keep_r, rr, sizeof rr) && wdn.k == 0);
    scene(DM_SPLIT, 0, 0, ~0u);
    expect("  .. WIDTH 0 % too", !memcmp(keep_l, rl, sizeof rl) && !memcmp(keep_r, rr, sizeof rr));
    {   /* the mode changed under them */
        static int32_t o[2 * CTL];
        uint32_t b;
        dsp_clean(DM_STEREO);
        host_preset(&trk[0], scn_eng("ANALOG"), 1);
        trk[0].p[P_SPRD] = 100;
        trk[0].p[P_VOICE] = V_POLY;
        trk[0].p[P_TRIM] = 120;
        wd_sel = 180;
        trk_note_on(&trk[0], 40, 120);
        for (b = 0; b < 400u; b++)
            mix_block(o, CTL);
        expect("STEREO, WIDTH 180 %, a spread note sounding: the stage running", wdn.k == (80 * 32768) / 100);
        duell_mode_force(DM_SPLIT);
        expect("a change to DUEL clears it (k 0, the states 0)", wdn.k == 0 && wdn.z0 == 0);
        for (b = 0; b < 100u; b++)
            mix_block(o, CTL);
        expect("  .. and in DUEL it stays at rest", wdn.k == 0);
        duell_mode_force(DM_STEREO);
        for (b = 0; b < 100u; b++)
            mix_block(o, CTL);
        expect("back in STEREO it runs again from rest (k glided to its setting)", wdn.k == (80 * 32768) / 100);
        wd_sel = 100;
        for (b = 0; b < 60u; b++)
            mix_block(o, CTL);
        expect("100 % again: at rest and the gate shut", wdn.k == 0 && !(wd_sel != 100u || wdn.k));
    }
    {   /* BASS MONO after WIDTH keeps the low end mono: a 60 Hz side through WIDTH 200 % and BASS MONO 160 Hz */
        uint32_t k;
        double in = 0, out = 0;
        wd_rest(200);
        wdn.k = 32768;
        memset(&bm, 0, sizeof bm);
        bm_sel = 7;                                      /* 160 Hz */
        for (i = 0; i < SEC; i += CTL) {
            for (k = 0; k < CTL; k++) {
                double s = 8000.0 * sin(PI2 * 60.0 * (double)(i + k) / FS);
                mix_l[k] = (int32_t)lrint(s), mix_r[k] = -(int32_t)lrint(s);
            }
            wd_block(CTL);
            bass_mono(CTL);
            if (i > SEC / 2u)
                for (k = 0; k < CTL; k++) {
                    double d = fabs((double)(mix_l[k] - mix_r[k]));
                    out = d > out ? d : out;
                }
        }
        in = 16000.0;
        printf("master:   a 60 Hz side through WIDTH 200 %% and BASS MONO 160 Hz: %.1f dB\n", db(out, in));
        expect("BASS MONO has the last word below its corner: a 60 Hz side through WIDTH 200 % and BASS MONO 160 Hz is at least 15 dB down", db(out, in) < -15.0);
        bm_sel = 0;
        memset(&bm, 0, sizeof bm);
        wd_rest(100);
    }
}

/* ------------------------------------------------------------------ the control --- */
static uint32_t page_of(const char *t)
{
    uint32_t i;
    for (i = 0; i < NPAGES && !str_eq(PAGES[i].title, t); i++)
        ;
    return i;
}
static int in_text(const char *hay, const char *a, const char *b)
{
    const char *p = strstr(hay, a), *q = strstr(hay, b);
    return p && q && p < q;
}
static void test_control(void)
{
    uint32_t v, ok, wp, sp;
    static char src[4000000];
    printf("master: the control (the pages WIDTH and MASTER, song.ms, the settings bytes; no MENU row)\n");
    memset(nor, 0xFF, sizeof nor);
    boot();
    duell_mode_force(DM_STEREO);
    wp = sp = page_of(MASTER_PAGE_TITLE);
    expect("a fresh unit: WIDTH 100 %; the audio side's gate shut; nothing written for it (the tag byte 0)", song.ms[0] == 100 && wd_sel == 100u && ui_ms_set[0] == 0u);
    expect("the page STEREO MASTER (0.2.25 layout round): GLO, SC_GLOBAL, K1 BASS MONO (G_BM), K2 WIDTH (G_WD), K3 K4 empty, right after SHIMMER and before COMPRESSOR (where WIDTH and MASTER were)",
           sp < NPAGES && PAGES[sp].fam == FAM_GLO && PAGES[sp].scope == SC_GLOBAL && str_eq(MASTER_PAGE_TITLE, "STEREO MASTER") &&
           PAGES[sp].graph == GR_MSTG && PAGES[sp].id[0] == G_BM && PAGES[sp].id[1] == G_WD && PAGES[sp].id[2] == 0xFFu && PAGES[sp].id[3] == 0xFFu &&
           str_eq(PAGES[sp - 1u].title, "SHIMMER") && str_eq(PAGES[sp + 1u].title, "COMPRESSOR") && page_of("WIDTH") == NPAGES && page_of("MASTER") == NPAGES);
    {
        int16_t *vp;
        const param_desc_t *a = page_desc(&PAGES[wp], 1, &vp);
        ok = a == &GP[G_WD] && vp == &song.ms[0] && str_eq(a->label, "WIDTH") && a->min == 0 && a->max == 200 && a->def == 100;
        ok &= G_WD == G_CL + 1 && sizeof song.ms == sizeof song.ms[0];
        expect("the card K2: WIDTH 0..200 (default 100) on song.ms[0]; the master state is WIDTH only (no DRIVE)", ok);
    }
    {
        char val[16];
        const char *unit;
        ok = 1;
        for (v = 0; v <= 200u; v++) {
            char want[8];
            snprintf(want, sizeof want, "%u", v);
            unit = "";
            param_format(&GP[G_WD], (int32_t)v, val, &unit);
            ok &= !strcmp(val, want) && !strcmp(unit, "%");
        }
        expect("the values as shown: WIDTH 0 .. 200 %", ok);
    }
    go_title(MASTER_PAGE_TITLE);
    expect("STEREO: the STEREO MASTER page shows the WIDTH card", page_visible(wp) && ui.page == wp && !card_hidden_vp(&song.ms[0]));
    turn(EN_K2, 1);
    frame();
    expect("K2 right: WIDTH 101 %, the audio side follows after the next frame", song.ms[0] == 101 && wd_sel == 101u);
    ok = 1;
    for (v = 0; v < 400u; v++)
        turn(EN_K2, 1);
    ok &= song.ms[0] == 200;
    for (v = 0; v < 400u; v++)
        turn(EN_K2, -1);
    ok &= song.ms[0] == 0;
    expect("the knob stops at 0 and 200 %", ok);
    ok = reset_desc(&GP[G_WD], &song.ms[0], 0, 0) == 1 && song.ms[0] == 100;
    expect("the quick reset (OCT- + the knob) puts WIDTH back to 100 %", ok);
    turn(EN_K3, 1);
    turn(EN_K4, 1);
    frame();
    expect("STEREO MASTER has two knobs: K3 and K4 do nothing (WIDTH and BASS MONO stay as they were)", song.ms[0] == 100 && song.bm == BM_DEF);
    /* DUEL */
    song.ms[0] = 150;
    frame();
    duell_mode_force(DM_SPLIT);
    expect("DUEL: the card is hidden (not drawn, the knob does nothing)", card_hidden_vp(&song.ms[0]));
    go_title(MASTER_PAGE_TITLE);
    turn(EN_K1, 1);
    turn(EN_K2, 1);
    expect("  .. the knobs change nothing in DUEL (BASS MONO, WIDTH); the WIDTH value is kept for STEREO", song.ms[0] == 150 && song.bm == BM_DEF);
    {   /* the panel says why: graph_mstg draws the note (the signature differs from the picture's) */
        ui.force = 1;
        draw_graph();
        expect("  .. the page draws its panel in DUEL without trouble (the note: NOT IN DUEL MODE, PART 1 LEFT, PART 2 RIGHT)", ui.page == sp);
    }
    duell_mode_force(DM_LAYER);
    expect("LAYER: the card is there again", !card_hidden_vp(&song.ms[0]));
    duell_mode_force(DM_STEREO);
    /* the stored bytes */
    song.ms[0] = 137;
    frame();
    fm1_ms += 2100u;
    frame();
    flush();
    {
        static persist_t p;
        ok = st_load(OBJ_SETTINGS, &p, sizeof p) > 0 && p.favorites.factory[15][18] == MS_TAG && p.favorites.factory[15][19] == 137u && p.favorites.factory[15][20] == 0u && p.favorites.factory[15][21] == (uint8_t)(BM_DEF + 1) &&
             p.magic == PERSIST_MAGIC;
        expect("a change is saved ~2 s later in the settings record's bytes [15][18..21]: the tag 0xB7, WIDTH 137, 0 (the removed DRIVE's byte), BASS MONO + 1 (0.2.24: 9 = 200 Hz) (same record, same size)", ok);
        boot();
        frame();
        expect("a power-on reads it back: WIDTH 137 %, to the audio side", song.ms[0] == 137 && wd_sel == 137u);
        p.favorites.factory[15][18] = MS_TAG, p.favorites.factory[15][19] = 137u, p.favorites.factory[15][20] = 71u, p.favorites.factory[15][21] = 0u;   /* a 0.2.24 record with DRIVE 71 */
        st_save(OBJ_SETTINGS, &p, sizeof p);
        boot();
        frame();
        fm1_ms += 2100u;
        frame();
        flush();
        expect("a record of 0.2.24 with DRIVE 71 in byte [20]: the value is ignored (WIDTH 137 % read, no saturator anywhere, nothing rewritten)", song.ms[0] == 137 && ui_ms_set[2] == 71u);
        ok = 1;
        for (v = 0; v < 256u; v++) {
            uint32_t b;
            for (b = 18u; b <= 21u; b++) {
                p.favorites.factory[15][18] = MS_TAG, p.favorites.factory[15][19] = 137u, p.favorites.factory[15][20] = 71u, p.favorites.factory[15][21] = 0u;
                p.favorites.factory[15][b] = (uint8_t)v;
                st_save(OBJ_SETTINGS, &p, sizeof p);
                boot();
                frame();
                ok &= song.ms[0] >= 0 && song.ms[0] <= 200 && wd_sel <= 200u;
                if (b == 18u && v != MS_TAG)
                    ok &= song.ms[0] == 100;   /* (no tag: the default) */
            }
        }
        expect("every byte value 0..255 in each of the four bytes (1024 power-ons): in range; without the tag the defaults (100 %, OFF)", ok);
        memset(p.favorites.factory[15] + 14, 0x5A, 10);   /* an old record: the FX layer's key map in [14..23] */
        p.favorites.factory[15][18] = 0x13;
        st_save(OBJ_SETTINGS, &p, sizeof p);
        boot();
        frame();
        expect("an older record with the FX layer's old key map in those bytes (no tag) reads as the default: WIDTH 100 %", song.ms[0] == 100 && wd_sel == 100u);
        memset(p.favorites.factory[15] + 14, 0, 10);
        st_save(OBJ_SETTINGS, &p, sizeof p);
        boot();
        frame();
        flush();
        ok = st_load(OBJ_SETTINGS, &p, sizeof p) > 0 && p.favorites.factory[15][18] == 0u && p.favorites.factory[15][19] == 0u;
        expect("a unit that never touches them keeps the bytes 0 (nothing written for them)", ok);
    }
    song.ms[0] = 40;
    frame();
    fm1_ms += 2100u;
    frame();
    flush();
    memset(nor, 0xFF, sizeof nor);                      /* ERASE DATA */
    boot();
    frame();
    expect("after ERASE DATA (no settings, the restart): the default, WIDTH 100 %", song.ms[0] == 100 && wd_sel == 100u);
    /* not in the multis, the project */
    boot();
    duell_mode_force(DM_STEREO);
    song.ms[0] = 60;
    frame();
    project_save_as(10, "MS TEST");
    song.ms[0] = 180;
    frame();
    project_load(10);
    frame();
    expect("a multi load leaves WIDTH alone (a device setting: nothing of it in the multi)", song.ms[0] == 180);
    {
        uint32_t k;
        ok = 1;
        for (k = 0; k < 16u; k++) {
            if (df_of(k) >= DF_N)
                continue;
            project_load(k);
            frame();
            ok &= song.ms[0] == 180;
        }
        expect("  .. nor does any factory multi", ok);
    }
    {
        project_t q;
        project_capture(&q);
        song.ms[0] = 5;
        project_restore_runtime(&q);
        frame();
        expect("  .. nor the project (project_restore_runtime)", song.ms[0] == 5);
    }
    {
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
            ok = mb && in_text(mb, "space_width(n);", "dm_ramp(n);") && in_text(mb, "dm_ramp(n);", "width_stage(n);") && in_text(mb, "width_stage(n);", "bass_mono(n);") &&
                 in_text(mb, "bass_mono(n);", "comp_stage(n, ") && in_text(mb, "comp_stage(n, ", "split_master(out, n, mg);") && in_text(mb, "comp_stage(n, ", "master_out(&l, &r);") &&
                 strstr(mb, "!dm_split && (wd_sel != 100u || wdn.k)") && !strstr(src, "sat_stage");
        }
        expect("mix_master (mix_block's master stages): SPACE's width, the MODE's dip, WIDTH, BASS MONO, COMP (WIDTH gated: not in DUEL, only off neutral or while fading), then the MASTER gain and master_out; no saturator stage anywhere in fx.c", ok);
    }
    ok = 1;
    for (v = 0; v < NTRK; v++)
        ok &= (int16_t *)&song.ms[0] < trk[v].p || (int16_t *)&song.ms[0] >= trk[v].p + P_COUNT;
    expect("the cards are no part parameter (not learnable, not morphed, as COMP)", ok && G_WD > G_COUNT);
    boot();
}

/* ------------------------------------------------------------------ cost --- */
static void cost_one(const char *name, void (*fill)(void), void (*stage)(uint32_t), void (*move)(uint32_t), double lim_st, double lim_mv)
{
    uint32_t i, reps = 20000u;
    uint64_t t0, base, st, mv;
    for (i = 0; i < 100u; i++)
        fill(), stage(CTL);
    t0 = instr_now();
    for (i = 0; i < reps; i++)
        fill();
    base = instr_now() - t0;
    t0 = instr_now();
    for (i = 0; i < reps; i++)
        fill(), stage(CTL);
    st = instr_now() - t0 - base;
    t0 = instr_now();
    for (i = 0; i < reps; i++)
        fill(), move(i), stage(CTL);
    mv = instr_now() - t0 - base;
    if (!st) {
        printf("master:   (no instruction counter on this host: cost not measured)\n");
        return;
    }
    printf("master:   %s: steady %.1f, setting moving %.1f host instructions per sample (%.2f %% / %.2f %% at 33 a percent)\n", name, (double)st / reps / CTL, (double)mv / reps / CTL,
           (double)st / reps / CTL / 33.0, (double)mv / reps / CTL / 33.0);
    {
        char w[200];
        snprintf(w, sizeof w, "%s costs under %.0f host instructions per sample steady, under %.0f while the setting moves; nothing at neutral (the gate: a compare a block)", name, lim_st, lim_mv);
        expect(w, (double)st / reps / CTL < lim_st && (double)mv / reps / CTL < lim_mv);
    }
}
static void fill_noise(void)
{
    uint32_t k;
    for (k = 0; k < CTL; k++)
        mix_l[k] = noise(20000), mix_r[k] = noise(20000);
}
static void wd_steady(uint32_t n) { wd_block(n); }
static void wd_move(uint32_t i) { wd_sel = (uint8_t)(i & 1u ? 160u : 40u); }
static void test_cost(void)
{
    printf("master: cost (host instructions per sample of the stage, one channel pair)\n");
    wd_rest(170);
    wdn.k = 70 * 32768 / 100;
    cost_one("WIDTH (wider, the crossover)", fill_noise, wd_steady, wd_move, 40.0, 50.0);
    wd_rest(100);
}

#ifndef MASTER_TEST_NO_MAIN
int main(void)
{
    uint64_t h = mode_stereo_scenario();                /* (the process's first render: the baseline hash) */
    printf("master: STEREO scenario hash 0x%016llX (before: 0x%016llX)\n", (unsigned long long)h, (unsigned long long)MODE_STEREO_HASH);
    expect("the busy two-part render (the process's first, WIDTH 100 %) is bit for bit the one of the sources before them", h == MODE_STEREO_HASH);
    test_width();
    test_mix();
    test_control();
    test_cost();
    printf("master: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
#endif
