/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.2: the UNIVERSAL FILTER (voice.c uf_run: a per-voice 2-pole SVF on the engines with no filter of their own, engine_t.ufilt;
 * P_FREEA CUT -64..63 with 0 = OFF, P_FREEB RES), on the firmware's sources (hostsim DSP, ui_test.c's UI).
 *   build/host/uf_test      (DUELL/host_tests.sh "uf")
 *   WHO       ufilt on PHASE LOFI VOICE WHEEL GRAIN PHYS FM6 SLICE; not on ANALOG SAMPLE TRIO NOISE (their own filters, unchanged) nor on the
 *             hidden DIGITAL, DRUM
 *   PARAMS    CUT (F_FILT, -64..63, default 0) and RES (0..127, default 0), shown "LP 47" / "HP 62" / "OFF"; automation takes them
 *   PAGE      the FILTER page (K1 CUT, K2 RES) only on the ufilt engines; ANALOG keeps its own; the other engines neither
 *   CENTRE    CUT 0: bit for bit (the RES and every cutoff route inert), and a +-1 CUT is nearly transparent on every ufilt engine
 *   SPECTRUM  PHASE and WHEEL: left of the centre a low-pass (the highs fall, the lows stay), right of it a high-pass
 *   ROUTES    ENV 1 / ENV 2 / LFO 1 / LFO 2 FLT, the matrix's CUT (ENV, VEL, AT), CC74, CC71 move the filter; ANALOG's own filter is unchanged
 *   STABLE    RES 127 at every CUT on loud chords: bounded
 *   ROLL      SMART: a CUT at 0 stays 0 (and its RES), a rolled CUT moves; WILD rolls from the centre; engines with a filter of their own: untouched
 *   OLD       a store of Felucca's layout (the SLICER's values on ids 45..48) reads CUT OFF, RES 0; a patch record keeps what it was given
 *   COST      host instructions per sample, PHASE 8 voices: filter off / LP / HP / RES 127 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <sys/mman.h>
#include <sys/wait.h>
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    printf("uf: %-118s %s\n", what, ok ? "ok" : "FAIL");
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

/* ------------------------------------------------------------------ the DSP --- */
static int32_t ob[2 * CTL];
#define MAXB (4u * FS / CTL)
static int32_t *rec_l;                                   /* shared with the render's child process (mmap) */
static volatile uint32_t *rec_nsh;
#define rec_n (*rec_nsh)
static int32_t *peak_sh;                                 /* the part's peak per block of the last render (t->peak), MAXB entries */
static void shared_init(void)
{
    uint32_t sz = MAXB * CTL * sizeof(int32_t) + MAXB * sizeof(int32_t) + 64u;
    uint8_t *m = mmap(0, sz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    rec_l = (int32_t *)m;
    peak_sh = (int32_t *)(m + MAXB * CTL * sizeof(int32_t));
    rec_nsh = (volatile uint32_t *)(m + MAXB * CTL * sizeof(int32_t) + MAXB * sizeof(int32_t));
}
static uint32_t eng_named(const char *n)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (str_eq(ENGINES[e]->name, n))
            return e;
    return 0;
}
static void dsp_clean(void)                              /* the DSP as at power-on, STEREO */
{
    memset(trk, 0, sizeof trk);
    memset(&mod, 0, sizeof mod);
    memset(mph, 0, sizeof mph);
    memset(&morph, 0, sizeof morph);
    memset(&mxf, 0, sizeof mxf);
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
    rng_state = 0x1234567u;
    fm1_in.notes = 0;
    kb_prev = 0;
    kb_layer = 0;
    mi_r = mi_w = 0;
    panic_req = 0;                                       /* (ui_power_on asks for every note off: not for this render) */
    duell_mode_force(DM_SPLIT);
    duell_mode_force(DM_STEREO);
    memset(dly_buf, 0, sizeof dly_buf);
    memset(&fx, 0, sizeof fx);
    memset(&fx_b, 0, sizeof fx_b);
    memset(&hl, 0, sizeof hl);
    memset(&hl_b, 0, sizeof hl_b);
    lim_env = lim_env_r = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    memset(lce, 0, sizeof lce);
    lc_l1 = lc_l2 = lc_r1 = lc_r2 = 0;
    memset(&sb, 0, sizeof sb);
    memset(&sb_r, 0, sizeof sb_r);
    fx_lowcut = 0;
    host_tracks_init();
    song.sel = 0;
    song.playing = 0;
    song.grid = 0;
}
static void mblk(uint32_t n)
{
    while (n--) {
        mix_block(ob, CTL);
        if (rec_n < MAXB * CTL) {
            peak_sh[rec_n / CTL] = trk[0].peak;              /* the part's own level this block (mix_part's running maximum) */
            trk[0].peak = 0;
            for (uint32_t i = 0; i < CTL; i++)
                rec_l[rec_n++] = ob[2u * i];
        }
    }
}
static uint64_t rec_hash(void)
{
    uint64_t h = 0xCBF29CE484222325ull;
    uint32_t i;
    for (i = 0; i < rec_n; i++)
        h = (h ^ (uint32_t)rec_l[i]) * 0x100000001B3ull;
    return h;
}

/* part 1 on engine e (its first preset), dry, a sustained chord; setup(t) before the notes. 1.5 s rendered into rec_l */
typedef void (*setup_t)(track_t *t);
static void render_child(uint32_t e, setup_t setup, uint32_t vel)
{
    static const uint8_t CH[4] = {48, 55, 60, 64};
    track_t *t = &trk[0];
    uint32_t i;
    dsp_clean();
    host_preset(t, e, 0);
    if (ENGINES[e] == &ENG_SLICE) {                      /* a slice that loops while the key is held (CHOP plays each slice once) */
        t->p[P_E0 + 4] = SLC_LOOP;
        t->p[P_E0 + 6] = 127;
    }
    t->p[P_VOICE] = V_POLY;
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_SUS] = 127;
    t->p[P_ATK] = 0;
    t->p[P_AMODE] = 0;
    t->p[P_LEVEL] = 60;                                     /* (well under the master limiter: its gain does not follow the filter) */
    t->p[P_ED_FLT] = t->p[P_LD_FLT] = t->p[P_E2FLT] = t->p[P_L2FLT] = 0;
    t->p[P_FREEA] = t->p[P_FREEB] = 0;
    if (setup)
        setup(t);
    rec_n = 0;
    for (i = 0; i < 4u; i++)
        trk_note_on(t, CH[i], vel);
    mblk(1.5 * FS / CTL);
}
/* each render in a child of the same parent (the engines keep state of their own: WHEEL's wheels, GRAIN's random numbers: the same
 * start every time), its samples back through shared memory */
static void render(uint32_t e, setup_t setup, uint32_t vel)
{
    pid_t pid;
    fflush(stdout);
    if (!(pid = fork())) {
        render_child(e, setup, vel);
        _exit(0);
    }
    waitpid(pid, 0, 0);
}

/* ------------------------------------------------------------------ spectrum --- */
#define NFFT 4096u
static double band_db(uint32_t from, double lo_hz, double hi_hz)   /* the energy of rec_l[from ..] between lo and hi, dB re 1 */
{
    static double win[NFFT];
    double e = 0;
    uint32_t i, k, k0 = (uint32_t)(lo_hz * NFFT / FS), k1 = (uint32_t)(hi_hz * NFFT / FS);
    for (i = 0; i < NFFT; i++)
        win[i] = (double)rec_l[from + i] * (0.5 - 0.5 * cos(2 * M_PI * i / NFFT));
    for (k = k0; k <= k1 && k < NFFT / 2u; k++) {
        double re = 0, im = 0;
        for (i = 0; i < NFFT; i++) {
            double a = 2 * M_PI * ((uint64_t)k * i % NFFT) / NFFT;
            re += win[i] * cos(a);
            im -= win[i] * sin(a);
        }
        e += re * re + im * im;
    }
    return 10.0 * log10(e + 1.0);
}
#define SPEC_FROM (FS / 2u)                              /* the second half of the render: the attack is over */
static double lo_db(void) { return band_db(SPEC_FROM, 40.0, 220.0); }
static double hi_db(void) { return band_db(SPEC_FROM, 4000.0, 12000.0); }
static double rms_of(const int32_t *x, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += (double)x[i] * x[i];
    return sqrt(s / n);
}
static int32_t peak_all(uint32_t from)
{
    int32_t m = 0;
    uint32_t i;
    for (i = from; i < rec_n; i++)
        m = rec_l[i] < 0 && -rec_l[i] > m ? -rec_l[i] : rec_l[i] > m ? rec_l[i] : m;
    return m;
}

static int32_t S_CUT, S_RES;
static void set_cut(track_t *t) { t->p[P_FREEA] = (int16_t)S_CUT; t->p[P_FREEB] = (int16_t)S_RES; }

/* ------------------------------------------------------------------ 1 --- */
static const char *const UF_ENGS[] = {"PHASE", "LOFI", "VOICE", "WHEEL", "GRAIN", "PHYS", "FM6", "SLICE"};
static const char *const OWN_ENGS[] = {"ANALOG", "SAMPLE", "TRIO", "NOISE"};
static void t_who_params(void)
{
    uint32_t e, i, n = 0, ok = 1;
    char v[12];
    const char *u;
    for (e = 0; e < NENGINES; e++) {
        uint32_t want = 0;
        for (i = 0; i < NELEM(UF_ENGS); i++)
            want |= str_eq(ENGINES[e]->name, UF_ENGS[i]);
        ok &= ENGINES[e]->ufilt == want;
        n += ENGINES[e]->ufilt != 0;
    }
    expect("engine_t.ufilt: PHASE LOFI VOICE WHEEL GRAIN PHYS FM6 SLICE (8); ANALOG SAMPLE TRIO NOISE, DIGITAL and DRUM have none",
           ok && n == 8u && !ENGINES[eng_named("ANALOG")]->ufilt && !ENGINES[ENGI_DRUM]->ufilt);
    expect("CUT: id 45, F_FILT -64..63, default 0 (OFF); RES: id 46, 0..127, default 0; 47, 48 stay free",
           P_FREEA == 45u && TP[P_FREEA].fmt == F_FILT && TP[P_FREEA].min == -64 && TP[P_FREEA].max == 63 && !TP[P_FREEA].def &&
           str_eq(TP[P_FREEA].label, "CUT") && P_FREEB == 46u && TP[P_FREEB].min == 0 && TP[P_FREEB].max == 127 &&
           !TP[P_FREEB].def && str_eq(TP[P_FREEB].label, "RES") && TP[P_FREEC].label[0] == '-' && TP[P_FREED].label[0] == '-');
    param_format(&TP[P_FREEA], 0, v, &u);
    ok = str_eq(v, "OFF") && !u[0];
    param_format(&TP[P_FREEA], -30, v, &u);
    ok &= str_eq(v, "47") && str_eq(u, "LP");
    param_format(&TP[P_FREEA], 40, v, &u);
    ok &= str_eq(v, "63") && str_eq(u, "HP");
    param_format(&TP[P_FREEA], -64, v, &u);
    ok &= str_eq(v, "100") && str_eq(u, "LP");
    expect("CUT shows OFF at the centre, \"47 LP\" left of it, \"63 HP\" right of it (the share of the dial, 100 at -64)", ok);
    expect("CUT and RES can be automated (a knob move is a parameter lock / event)", motion_param(P_FREEA) && motion_param(P_FREEB) &&
           !motion_param(P_FREEC));
    ok = 1;
    for (i = 0; i < NELEM(MIDI_CC_ENG); i++)
        ok &= !ENGINES[i]->ufilt || MIDI_CC_ENG[i] == MCC_UF;
    expect("CC74 / CC71 on every ufilt engine: the universal filter's CUT / RES (midi_control.c MIDI_CC_ENG)", ok);
}

/* ------------------------------------------------------------------ 2 PAGE --- */
static uint32_t uf_page(void)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].id[0] == P_FREEA)
            return i;
    return NPAGES;
}
static uint32_t ftype_page(void)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].id[0] == P_FTYPE)
            return i;
    return NPAGES;
}
static void t_page(void)
{
    uint32_t e, ok = 1, own = 1, k = uf_page(), r, an = 0;
    char w[96];
    expect("one FILTER page for the universal filter, in EDIT's family beside ANALOG's: K1 CUT, K2 RES, K3 K4 empty",
           k < NPAGES && PAGES[k].fam == FAM_EDIT && str_eq(PAGES[k].title, "FILTER") && PAGES[k].scope == SC_TRACK &&
           PAGES[k].id[1] == P_FREEB && PAGES[k].id[2] == 0xFFu && PAGES[k].id[3] == 0xFFu);
    for (r = 0; r < NENG_SHOWN; r++) {
        e = eng_vis(r);
        ui_power_on();
        set_engine_of(TSEL, e);
        snprintf(w, sizeof w, "%s", ENGINES[e]->name);
        if (ENGINES[e]->ufilt)
            ok &= page_visible(k) != 0;
        else
            own &= !page_visible(k);
        an += ENGINES[e] == &ENG_ANALOG && page_visible(ftype_page());
    }
    expect("the FILTER page shows on PHASE LOFI VOICE WHEEL GRAIN PHYS FM6 SLICE", ok);
    expect("  .. not on ANALOG, SAMPLE, TRIO, NOISE (their own filters show theirs as today: ANALOG's FILTER page, TRIO / NOISE / SAMPLE on EDIT)",
           own && an == 1u);
    ui_power_on();
    set_engine_of(TSEL, eng_named("WHEEL"));
    go_title("EDIT 1"); frame();
    ui.page = (uint8_t)k;
    page_entered();
    frame();
    ok = TSEL->p[P_FREEA] == 0 && uf_idle(TSEL);
    turn(EN_K1, -7);
    ok &= TSEL->p[P_FREEA] == -7 && !uf_idle(TSEL);
    turn(EN_K2, 5);
    ok &= TSEL->p[P_FREEB] == 5;
    turn(EN_K1, 7);
    ok &= TSEL->p[P_FREEA] == 0 && uf_idle(TSEL);
    expect("FILTER page: KNOB 1 turns CUT through the centre (OFF at 0), KNOB 2 RES; uf_idle says the filter is off (RES and the FLT depths faded)", ok);
    ui_power_on();
    ok = !uf_idle(TSEL);                                   /* ANALOG: no universal filter: nothing to be idle */
    TSEL->p[P_FREEA] = -9;
    ok &= !uf_idle(TSEL);
    expect("an engine with its own filter: no faded cards (uf_idle is 0 whatever the ids hold)", ok);
}

/* ------------------------------------------------------------------ 3 CENTRE --- */
#define NEAR_P 0.6                                      /* CUT +1 / -1: the difference's rms, of the signal's (a voice that was dropped: 1.0) */
#define NEAR_M 0.45
static void routes_on(track_t *t)
{
    t->p[P_ED_FLT] = 40; t->p[P_LD_FLT] = 50; t->p[P_E2FLT] = 60; t->p[P_L2FLT] = 30;
    t->p[P_E2SUS] = 127; t->p[P_L2PIT] = 0; t->p[P_LRATE] = 90;
    t->p[P_M1SRC] = MS_VEL; t->p[P_M1DST] = MD_CUT; t->p[P_M1AMT] = 40;
}
static void res_hi(track_t *t) { t->p[P_FREEB] = 127; }
static double diff_rel(const int32_t *ref, double ref_rms)   /* rms of (rec_l - ref) over the second half, against the signal's */
{
    double d = 0;
    uint32_t j;
    for (j = SPEC_FROM; j < rec_n; j++)
        d += ((double)rec_l[j] - ref[j]) * ((double)rec_l[j] - ref[j]);
    return sqrt(d / (rec_n - SPEC_FROM)) / (ref_rms + 1e-9);
}
static void t_centre(void)
{
    uint32_t i, ok_res = 1, ok_routes = 1, ok_near = 1;
    for (i = 0; i < NELEM(UF_ENGS); i++) {
        uint32_t e = eng_named(UF_ENGS[i]);
        uint64_t h0, h1, h2;
        double s0, rel_p, rel_m;
        static int32_t keep[MAXB * CTL];
        render(e, 0, 100);
        h0 = rec_hash();
        s0 = rms_of(rec_l + SPEC_FROM, rec_n - SPEC_FROM);
        memcpy(keep, rec_l, rec_n * sizeof keep[0]);
        render(e, res_hi, 100);
        h1 = rec_hash();
        render(e, routes_on, 100);                         /* every cutoff route set, CUT still at the centre */
        h2 = rec_hash();
        ok_res &= h1 == h0;
        ok_routes &= h2 == h0;
        S_CUT = 1; S_RES = 0;
        render(e, set_cut, 100);
        rel_p = diff_rel(keep, s0);
        S_CUT = -1;
        render(e, set_cut, 100);
        rel_m = diff_rel(keep, s0);
        ok_near &= rel_p < NEAR_P && rel_m < NEAR_M && s0 > 100;
        printf("uf:   %-6s CUT +1: %.3f, CUT -1: %.3f of the signal's rms (%.0f)\n", UF_ENGS[i], rel_p, rel_m, s0);
    }
    expect("CUT 0: RES 127 changes nothing, bit for bit, on all eight engines (the filter does not run)", ok_res);
    expect("CUT 0: every cutoff route set (ENV 1 / ENV 2 / LFO 1 / LFO 2 FLT, the matrix's CUT) is inert too, bit for bit", ok_routes);
    expect("CUT +1 (HP 33 Hz) and -1 (LP 14 kHz) are nearly transparent on every engine: it runs on all of them, additively", ok_near);
}

/* ------------------------------------------------------------------ 4 SPECTRUM --- */
static void t_spectrum(void)
{
    static const char *const E[2] = {"PHASE", "WHEEL"};
    uint32_t i, ok_lp = 1, ok_hp = 1;
    for (i = 0; i < 2u; i++) {
        uint32_t e = eng_named(E[i]);
        double l0, h0, ll, hl, lh, hh;
        render(e, 0, 100);
        l0 = lo_db(); h0 = hi_db();
        S_CUT = -40; S_RES = 0;
        render(e, set_cut, 100);
        ll = lo_db(); hl = hi_db();
        S_CUT = 40;
        render(e, set_cut, 100);
        lh = lo_db(); hh = hi_db();
        printf("uf:   %-6s off: low %.1f high %.1f dB; LP 62: low %.1f high %.1f; HP 62: low %.1f high %.1f\n", E[i], l0, h0, ll, hl, lh, hh);
        ok_lp &= hl < h0 - 12.0 && ll > l0 - 3.0 && ll < l0 + 3.0;
        ok_hp &= lh < l0 - 12.0 && hh > h0 - 3.0 && hh < h0 + 3.0;
    }
    expect("PHASE, WHEEL: CUT -40 is a low-pass (the highs 12 dB down or more, the lows kept within 3 dB)", ok_lp);
    expect("PHASE, WHEEL: CUT +40 is a high-pass (the lows 12 dB down or more, the highs kept within 3 dB)", ok_hp);
    {   /* RES: a peak at the cutoff */
        double a, b;
        uint32_t e = eng_named("PHASE");
        S_CUT = -20; S_RES = 0;
        render(e, set_cut, 100);
        a = band_db(SPEC_FROM, 1200.0, 3500.0);
        S_RES = 110;
        render(e, set_cut, 100);
        b = band_db(SPEC_FROM, 1200.0, 3500.0);
        expect("PHASE CUT -20: RES 110 raises the band round the cutoff over RES 0", b > a + 2.0);
    }
}

/* ------------------------------------------------------------------ 5 ROUTES --- */
static void r_env1(track_t *t) { set_cut(t); t->p[P_ED_FLT] = 40; }
static void r_env2(track_t *t) { set_cut(t); t->p[P_E2FLT] = 40; t->p[P_E2SUS] = 127; }
static void r_lfo1(track_t *t) { set_cut(t); t->p[P_LD_FLT] = 60; t->p[P_LRATE] = 100; t->p[P_LWAVE] = 2; }
static void r_lfo2(track_t *t) { set_cut(t); t->p[P_L2FLT] = 60; t->p[P_L2RATE] = 100; t->p[P_L2WAVE] = 2; }
static void r_vel(track_t *t) { set_cut(t); t->p[P_M1SRC] = MS_VEL; t->p[P_M1DST] = MD_CUT; t->p[P_M1AMT] = 63; }
static void r_env(track_t *t) { set_cut(t); t->p[P_M1SRC] = MS_ENV; t->p[P_M1DST] = MD_CUT; t->p[P_M1AMT] = 40; }
static void r_at(track_t *t) { set_cut(t); t->p[P_M1SRC] = MS_AT; t->p[P_M1DST] = MD_CUT; t->p[P_M1AMT] = 40; t->at = 127; }
static void r_lfo2m(track_t *t) { set_cut(t); t->p[P_M1SRC] = MS_LFO2; t->p[P_M1DST] = MD_CUT; t->p[P_M1AMT] = 40; t->p[P_L2RATE] = 100; t->p[P_L2WAVE] = 2; }
static double hi_swing(void)                              /* the high band over eight windows: max - min, dB */
{
    double mn = 1e9, mx = -1e9;
    uint32_t w;
    for (w = 0; w < 8u; w++) {
        double h = band_db(SPEC_FROM / 2u + w * 2048u, 4000.0, 12000.0);
        mn = h < mn ? h : mn;
        mx = h > mx ? h : mx;
    }
    return mx - mn;
}
static void t_routes(void)
{
    static const struct { const char *n; setup_t s; int moves; } R[] = {
        {"ENV 1 FLT +40", r_env1, 0}, {"ENV 2 FLT +40", r_env2, 0}, {"matrix VEL -> CUT +63", r_vel, 0}, {"matrix ENV -> CUT +40", r_env, 0},
        {"matrix AT -> CUT +40", r_at, 0}, {"LFO 1 FLT +60", r_lfo1, 1}, {"LFO 2 FLT +60", r_lfo2, 1}, {"matrix LFO2 -> CUT +40", r_lfo2m, 1}};
    uint32_t i, k, ok = 1;
    static const char *const E[2] = {"PHASE", "GRAIN"};
    for (k = 0; k < 2u; k++) {
        double base;
        S_CUT = -45; S_RES = 0;
        render(eng_named(E[k]), set_cut, 100);
        base = hi_db();
        for (i = 0; i < NELEM(R); i++) {
            double h, sw;
            render(eng_named(E[k]), R[i].s, 100);
            h = hi_db();
            sw = hi_swing();
            printf("uf:   %-6s CUT -45 + %-24s highs %+.1f dB over CUT alone, swing %.1f dB\n", E[k], R[i].n, h - base, sw);
            if (R[i].moves)
                ok &= sw > 1.5;
            else
                ok &= h > base + 4.0;
        }
    }
    expect("PHASE, GRAIN at CUT -45: ENV 1 / ENV 2 FLT, matrix VEL / ENV / AT -> CUT open the filter; LFO 1 / LFO 2 FLT and matrix LFO2 -> CUT sweep it", ok);
    {   /* the modulation adds to the dial and clamps: a deep one cannot leave it */
        double h;
        S_CUT = -60; S_RES = 0;
        render(eng_named("PHASE"), r_vel, 100);
        h = hi_db();
        expect("  .. the sum is clamped to -64..63 (CUT -60 + 63: a high-pass, no overflow, a sound)", h > 0 && peak_all(SPEC_FROM) > 100 && peak_all(0) < 32768);
    }
    {   /* ANALOG: its own filter, unchanged */
        uint64_t h0, h1, h2, h3;
        render(0, 0, 100);
        h0 = rec_hash();
        S_CUT = -40; S_RES = 100;
        render(0, set_cut, 100);
        h1 = rec_hash();
        render(0, r_env2, 100);
        h2 = rec_hash();
        render(0, 0, 100);
        h3 = rec_hash();
        expect("ANALOG (its own filter): CUT / RES ids set change nothing, bit for bit; ENV 2 FLT still moves ANALOG's CUT", h1 == h0 && h2 != h0 && h3 == h0);
    }
    {   /* CC74 / CC71 on a ufilt engine set the new ids; on ANALOG its own */
        ui_power_on();
        set_engine_of(&trk[0], eng_named("PHASE"));
        midi_cc_map(&trk[0], 74, 100);
        midi_cc_map(&trk[0], 71, 90);
        ok = trk[0].p[P_FREEA] == 36 && trk[0].p[P_FREEB] == 90;
        midi_cc_map(&trk[0], 74, 64);
        ok &= trk[0].p[P_FREEA] == 0;
        set_engine_of(&trk[0], 0u);
        midi_cc_map(&trk[0], 74, 20);
        ok &= trk[0].p[P_FREEA] == 0 && trk[0].p[P_E4] == 20;
        expect("CC74 100 on PHASE: CUT +36, CC71 90 RES 90, CC74 64 the centre; on ANALOG CC74 is ANALOG's own CUT", ok);
    }
}

/* ------------------------------------------------------------------ 6 STABLE --- */
static int32_t part_peak(uint32_t from_blk, uint32_t to_blk)   /* the part's largest block peak (before the master) */
{
    int32_t m = 0;
    uint32_t i;
    for (i = from_blk; i < to_blk; i++)
        m = peak_sh[i] > m ? peak_sh[i] : m;
    return m;
}
static void t_stable(void)
{
    static const int32_t C[6] = {-60, -30, -5, 5, 30, 60};
    static const char *const E[4] = {"PHASE", "WHEEL", "FM6", "PHYS"};
    uint32_t i, k, ok = 1, nb = rec_n / CTL;
    int32_t worst = 0, ratio_worst = 0;
    for (k = 0; k < 4u; k++) {
        int32_t ref;
        render(eng_named(E[k]), 0, 127);
        nb = rec_n / CTL;
        ref = part_peak(nb / 4u, nb);
        for (i = 0; i < 6u; i++) {
            int32_t a, b, r;
            S_CUT = C[i]; S_RES = 127;
            render(eng_named(E[k]), set_cut, 127);
            a = part_peak(nb / 4u, nb / 2u);
            b = part_peak(nb / 2u, nb);
            r = ref ? (a * 10) / ref : 0;
            worst = a > worst ? a : worst;
            ratio_worst = r > ratio_worst ? r : ratio_worst;
            ok &= b <= a + a / 4 && a < (1 << 24) && peak_all(0) < 32768;
        }
        printf("uf:   %-6s RES 127, CUT -60 .. +60: the part's peak up to %d x10 of the unfiltered %d (the master's output below %d)\n", E[k],
               ratio_worst, ref, peak_all(0));
    }
    printf("uf:   RES 127, loud chords, PHASE WHEEL FM6 PHYS: the part's peak at most %d, %.1f x the unfiltered\n", worst, ratio_worst / 10.0);
    expect("RES 127 at every CUT (loud chords, 1.5 s): bounded (no runaway: the second half never above the first by a quarter, the master within range)", ok);
}

/* ------------------------------------------------------------------ 7 ROLL --- */
static void t_roll(void)
{
    static int8_t o[P_COUNT], end[P_COUNT];
    uint32_t seed, i, moved = 0, stayed = 1, res_stay = 1, own_same = 1, wild_moves = 0, inrange = 1;
    const engine_t *e = ENGINES[eng_named("PHASE")];
    uint8_t sv = mrc;
    mrc = 0;
    for (i = 0; i < P_COUNT; i++)
        o[i] = (int8_t)TP[i].def;
    o[P_FREEB] = 60;
    for (seed = 1; seed <= 200u; seed++) {
        morph_roll_pure(o, end, e, 40, seed, 0, 1);
        stayed &= end[P_FREEA] == 0;
        res_stay &= end[P_FREEB] == 60;
    }
    o[P_FREEA] = -30;
    for (seed = 1; seed <= 200u; seed++) {
        morph_roll_pure(o, end, e, 40, seed, 0, 1);
        moved += end[P_FREEA] != -30;
        inrange &= end[P_FREEA] >= -64 && end[P_FREEA] <= 63 && end[P_FREEB] >= 0 && end[P_FREEB] <= 127 &&
                   abs(end[P_FREEA] + 30) <= 40 * 127 / 100;
    }
    {   /* an engine with its own filter: the ids are not rolled whatever they hold */
        const engine_t *a = ENGINES[eng_named("ANALOG")];
        for (seed = 1; seed <= 200u; seed++) {
            morph_roll_pure(o, end, a, 40, seed, 0, 1);
            own_same &= end[P_FREEA] == -30 && end[P_FREEB] == 60;
        }
    }
    o[P_FREEA] = 0;
    mrc = MC_WILD;
    for (seed = 1; seed <= 200u; seed++) {
        morph_roll_pure(o, end, e, 40, seed, 0, 1);
        wild_moves += end[P_FREEA] != 0;
    }
    mrc = sv;
    expect("ROLL SMART: a CUT at 0 stays OFF in 200 rolls (and its RES with it): a roll never switches the filter on", stayed && res_stay);
    expect("  .. a CUT off the centre rolls (most of 200 rolls move it), inside -64..63, at most AMOUNT % of the range", moved > 120u && inrange);
    expect("  .. an engine with its own filter (ANALOG): CUT / RES never rolled", own_same);
    expect("  .. WILD may roll from the centre", wild_moves > 20u);
}

/* ------------------------------------------------------------------ 8 OLD --- */
static void rec_fill(up_rec_t *r, uint32_t ver, uint32_t np, uint32_t eng)   /* a record of ver with every value the default */
{
    uint32_t k;
    memset(r, 0, sizeof *r);
    r->used = UP_USED;
    r->ver = (uint8_t)ver;
    r->engine = (uint8_t)eng;
    r->np = (uint8_t)np;
    for (k = 0; k < np; k++)
        up_set_value(r, k, k < np - 8u ? TP[k].def : ENGINES[eng]->edit[k - (np - 8u)].def);
}
static void t_old(void)
{
    int16_t v[P_COUNT], def[P_COUNT];
    up_rec_t r;
    uint32_t i;
    for (i = 0; i < P_COUNT; i++) {
        def[i] = TP[i].def;
        v[i] = def[i];
    }
    v[P_FREEA] = 1; v[P_FREEB] = 1; v[P_FREEC] = 127; v[P_FREED] = 5;    /* a store of Felucca's layout: the SLICER's SLCR PAT RATE DEPTH */
    params_retire(v, def);
    expect("a store of Felucca's layout (the SLICER's values on ids 45..48) loads CUT OFF, RES 0 (params_retire)",
           v[P_FREEA] == 0 && v[P_FREEB] == 0 && v[P_FREEC] == 0 && v[P_FREED] == 0);
    rec_fill(&r, UP_VER_FEL, 111u, eng_named("PHASE"));                     /* a Felucca-layout patch record with the SLICER on */
    up_set_value(&r, 45u, 2); up_set_value(&r, 46u, 1); up_set_value(&r, 47u, 127); up_set_value(&r, 48u, 127);
    up_values(&r, v);
    expect("a user patch of Felucca's layout (SLCR 2, PAT 1, RATE 127, DEPTH 127 on ids 45..48): CUT OFF, RES 0 when it loads",
           v[P_FREEA] == 0 && v[P_FREEB] == 0 && v[P_FREEC] == 0 && v[P_FREED] == 0);
    rec_fill(&r, UP_VER, P_COUNT, eng_named("PHASE"));                      /* DUELL's layout: a record with no CUT stored reads OFF */
    up_values(&r, v);
    expect("a DUELL patch record with no CUT / RES stored (every record before 0.2.2): CUT OFF, RES 0 (up_def: the defaults)",
           v[P_FREEA] == 0 && v[P_FREEB] == 0 && up_def(eng_named("PHASE"), P_FREEA) == 0 && up_def(eng_named("PHASE"), P_FREEB) == 0);
    up_set_value(&r, P_FREEA, -33); up_set_value(&r, P_FREEB, 99);
    up_values(&r, v);
    expect("  .. and one that holds CUT -33, RES 99 gives them back", v[P_FREEA] == -33 && v[P_FREEB] == 99);
}

/* ------------------------------------------------------------------ 9 COST --- */
static double cost_run(uint32_t e, int32_t cut, int32_t res)
{
    static const uint8_t N[8] = {48, 52, 55, 59, 60, 64, 67, 71};
    track_t *t;
    uint64_t i0, i1;
    uint32_t i, nb = 2u * FS / CTL;
    dsp_clean();
    t = &trk[0];
    host_preset(t, e, 0);
    t->p[P_VOICE] = V_POLY;
    t->p[P_SUS] = 127;
    t->p[P_ATK] = 0;
    t->p[P_FREEA] = (int16_t)cut;
    t->p[P_FREEB] = (int16_t)res;
    for (i = 0; i < 8u; i++)
        trk_note_on(t, N[i], 100);
    for (i = 0; i < FS / CTL; i++)
        mix_block(ob, CTL);
    i0 = instr_now();
    for (i = 0; i < nb; i++)
        mix_block(ob, CTL);
    i1 = instr_now();
    return (double)(i1 - i0) / ((double)nb * CTL);
}
static void t_cost(void)
{
    static const char *const E[3] = {"PHASE", "WHEEL", "FM6"};
    uint32_t k, ok = 1;
    for (k = 0; k < 3u; k++) {
        uint32_t e = eng_named(E[k]);
        double off = cost_run(e, 0, 0), lp = cost_run(e, -30, 20), hp = cost_run(e, 30, 20), hr = cost_run(e, -30, 127);
        printf("uf:   %-6s 8 voices, instructions per sample: filter OFF %.0f, LP %.0f, HP %.0f, LP RES 127 %.0f  (+%.1f per voice and sample, LP)\n",
               E[k], off, lp, hp, hr, (lp - off) / 8.0);
        ok &= !off || (lp - off) / 8.0 < 60.0;
    }
    expect("cost: the filter adds under 60 host instructions per voice and sample at 8 voices (OFF: the voice code as before)", ok);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    shared_init();
    (void)OWN_ENGS;
    t_who_params();
    t_page();
    t_centre();
    t_spectrum();
    t_routes();
    t_stable();
    t_roll();
    t_old();
    t_cost();
    printf(bad ? "uf: %d of %d checks FAILED\n" : "uf: all %d checks passed\n", bad ? bad : ncheck, ncheck);
    return bad ? 1 : 0;
}
