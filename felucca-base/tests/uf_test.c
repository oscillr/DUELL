/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.9: the MULTIMODE FILTER (voice.c uf_run: a per-voice state-variable filter on the engines with no filter of their own,
 * engine_t.ufilt; 0.2.2's universal filter): P_FREEA CUT (0..82 dial steps, 30 Hz .. 16 kHz, 82 open), P_FREEB RES, P_FREEC MODE (0 LP ..
 * 64 BP .. 127 HP), P_FREED SLOPE (1 12 dB, 2 24 dB), on the firmware's sources (hostsim DSP, ui_test.c's UI).
 *   build/host/uf_test      (DUELL/host_tests.sh "uf"; UF_TABLE=1 prints the whole response table)
 *   WHO       ufilt 1 on PHASE LOFI VOICE WHEEL PHYS FM6 WAVE (GRAIN, SLICE until 0.2.8); ufilt 2 on ANALOG TRIO NOISE: their own filters unchanged,
 *             the multimode filter IN SERIES after them (page FILTER), bypassed by default: every sound of before bit for bit
 *   PARAMS    ids, ranges, defaults (the bypass), the cards' texts (Hz / kHz; LP LP>BP BP BP>HP HP; 12dB 24dB); automation; CC74 / CC71
 *   PAGE      the FILTER page K1 CUT K2 RES K3 MODE K4 SLOPE on the ufilt 1 engines, FILTER on the ufilt 2 ones; SLOPE never 0; no card faded
 *   BYPASS    the defaults (MODE LP, CUT open) are bit for bit the render without a filter whatever RES / SLOPE / a route that only opens
 *   RESPONSE  measured gains (sines through uf_run): LP / BP / HP, 12 / 24 dB, at the ends and between, RES; the kill zones silent at the ends
 *   EXACT     a low-pass of before (0.2.8's uf_run, kept here) and today's at the same cutoff: bit for bit; a high-pass within 0.01 octave
 *   SWEEPS    MODE 0 .. 127 and back, CUT through both kill zones, SLOPE 12 <-> 24: no click (the largest step within the static renders')
 *   ROUTES    ENV 1 / ENV 2 / LFO 1 / LFO 2 FLT, the matrix's CUT (ENV, VEL, AT), CC74 move the filter (ufilt 1); on ufilt 2 they stay the engine's own filter's, FRES is the multimode RES
 *   STABLE    RES 127 at every CUT, MODE, SLOPE on loud chords: bounded
 *   MIGRATE   patch records, projects, multis of before 0.2.9 (SLOPE 0 / absent): 0.2.2's CUT -> CUT + MODE (LP exact, HP, OFF the bypass), their
 *             CUT automation with them; a record of today is kept; a store of Felucca's layout reads the bypass
 *   ROLL      SMART: a bypassed filter stays bypassed (CUT, RES, FLT depths, no CUT route); a set CUT rolls inside 0..82 and keeps out of the
 *             kill zones; MODE and SLOPE never roll (SMART, WILD); ANALOG TRIO NOISE: CUT and RES roll the same way
 *   COST      host instructions per sample, PHASE / WHEEL / FM6 at 8 voices: bypass / 12 dB / 24 dB */
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
    t->p[P_VOICE] = V_POLY;
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_SUS] = 127;
    t->p[P_ATK] = 0;
    t->p[P_AMODE] = 0;
    t->p[P_TRIM] = 60;                                     /* (well under the master limiter: its gain does not follow the filter) */
    t->p[P_ED_FLT] = t->p[P_LD_FLT] = t->p[P_E2FLT] = t->p[P_L2FLT] = 0;
    t->p[P_FREEA] = UF_STEPS;                               /* (the bypass: the defaults) */
    t->p[P_FREEB] = t->p[P_FREEC] = 0;
    t->p[P_FREED] = 1;
    if (setup)
        setup(t);
    rec_n = 0;
    for (i = 0; i < 4u; i++)
        trk_note_on(t, CH[i], vel);
    mblk(1.5 * FS / CTL);
}
/* each render in a child of the same parent (the engines keep state of their own: WHEEL's wheels: the same
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

static int32_t S_CUT = UF_STEPS, S_RES, S_MODE, S_SLOPE = 1;
static void set_f(track_t *t)
{
    t->p[P_FREEA] = (int16_t)S_CUT;
    t->p[P_FREEB] = (int16_t)S_RES;
    t->p[P_FREEC] = (int16_t)S_MODE;
    t->p[P_FREED] = (int16_t)S_SLOPE;
}
static void sf(int32_t cut, int32_t res, int32_t mode, int32_t slope) { S_CUT = cut; S_RES = res; S_MODE = mode; S_SLOPE = slope; }
static double uf_hz(int32_t step) { return 30.0 * pow(16000.0 / 30.0, (40 + 396.0 * step) / 256.0 / 127.0); }   /* a dial step's cutoff */

/* ------------------------------------------------------------------ 1 WHO, PARAMS --- */
static const char *const UF_ENGS[] = {"PHASE", "LOFI", "VOICE", "WHEEL", "PHYS", "FM6", "WAVE"};   /* (DUELL 0.2.9: GRAIN, SLICE retired) */
static const char *const SER_ENGS[] = {"ANALOG", "TRIO", "NOISE"};   /* ufilt 2: their own filter, then the multimode one in series */
static int fmt_is(uint32_t id, int32_t v, const char *val, const char *unit)
{
    char b[12];
    const char *u = "";
    param_format(&TP[id], v, b, &u);
    if (!str_eq(b, val) || !str_eq(u, unit))
        printf("uf:   (shown %d: \"%s\" \"%s\", wanted \"%s\" \"%s\")\n", (int)v, b, u, val, unit);
    return str_eq(b, val) && str_eq(u, unit);
}
static void t_who_params(void)
{
    uint32_t e, i, n = 0, ok = 1;
    for (e = 0; e < NENGINES; e++) {
        uint32_t want = 0;
        for (i = 0; i < NELEM(UF_ENGS); i++)
            want |= str_eq(ENGINES[e]->name, UF_ENGS[i]);
        for (i = 0; i < NELEM(SER_ENGS); i++)
            want |= 2u * str_eq(ENGINES[e]->name, SER_ENGS[i]);
        ok &= ENGINES[e]->ufilt == want;
        n += ENGINES[e]->ufilt != 0;
    }
    expect("engine_t.ufilt: 1 on PHASE LOFI VOICE WHEEL PHYS FM6 WAVE (7), 2 on ANALOG TRIO NOISE (own filter + the multimode one in series); the retired ones (DIGITAL, SAMPLE, GRAIN, DRUM, SLICE) none",
           ok && n == 10u && ENGINES[eng_named("ANALOG")]->ufilt == 2u && !ENGINES[10]->ufilt);
    expect("ids 45 CUT (F_FILT 0..82, default 82: open), 46 RES (0..127, 0), 47 MODE (F_FMODE 0..127, 0: LP), 48 SLOPE (enum 1..2, 1: 12 dB)",
           P_FREEA == 45u && TP[P_FREEA].fmt == F_FILT && TP[P_FREEA].min == 0 && TP[P_FREEA].max == UF_STEPS && TP[P_FREEA].def == UF_STEPS &&
           str_eq(TP[P_FREEA].label, "CUT") && P_FREEB == 46u && TP[P_FREEB].min == 0 && TP[P_FREEB].max == 127 && !TP[P_FREEB].def &&
           str_eq(TP[P_FREEB].label, "RES") && P_FREEC == 47u && TP[P_FREEC].fmt == F_FMODE && TP[P_FREEC].min == 0 &&
           TP[P_FREEC].max == 127 && !TP[P_FREEC].def && str_eq(TP[P_FREEC].label, "MODE") && P_FREED == 48u &&
           TP[P_FREED].fmt == F_ENUM && TP[P_FREED].min == 1 && TP[P_FREED].max == 2 && TP[P_FREED].def == 1 && str_eq(TP[P_FREED].label, "SLOPE"));
    ok = fmt_is(P_FREEA, UF_STEPS, "16.0", "kHz") && fmt_is(P_FREEA, 0, "30", "Hz") && fmt_is(P_FREEA, 54, "1.8", "kHz") &&
         fmt_is(P_FREEA, 20, "139", "Hz");
    expect("CUT shows its cutoff: 82 \"16.0 kHz\" (open), 54 \"1.8 kHz\", 20 \"139 Hz\", 0 \"30 Hz\"", ok);
    ok = fmt_is(P_FREEC, 0, "LP", "") && fmt_is(P_FREEC, 1, "LP>BP", "") && fmt_is(P_FREEC, 63, "LP>BP", "") && fmt_is(P_FREEC, 64, "BP", "") &&
         fmt_is(P_FREEC, 65, "BP>HP", "") && fmt_is(P_FREEC, 126, "BP>HP", "") && fmt_is(P_FREEC, 127, "HP", "") &&
         fmt_is(P_FREED, 1, "12dB", "") && fmt_is(P_FREED, 2, "24dB", "");
    expect("MODE shows LP / LP>BP / BP / BP>HP / HP (5 characters at most), SLOPE 12dB / 24dB", ok);
    expect("CUT and RES can be automated (a knob move is a parameter lock / event); MODE and SLOPE are settings (not)",
           motion_param(P_FREEA) && motion_param(P_FREEB) && !motion_param(P_FREEC) && !motion_param(P_FREED));
    ok = 1;
    for (i = 0; i < NELEM(MIDI_CC_ENG); i++)
        ok &= ENGINES[i]->ufilt == 1u ? MIDI_CC_ENG[i] == MCC_UF : ENGINES[i]->ufilt == 2u ? MIDI_CC_ENG[i] && MIDI_CC_ENG[i] != MCC_UF : 1;
    expect("CC74 / CC71 on every ufilt 1 engine: the multimode filter's CUT / RES; on ufilt 2 (ANALOG TRIO NOISE) still the engine's own (midi_control.c MIDI_CC_ENG)", ok);
    {   /* the matrix names the filter's destinations honestly: CUT, FRES (RES) */
        expect("the matrix destinations CUT and FRES are the filter's cutoff and resonance (mod.c: FRES -> id 46)",
               str_eq(N_MDST[MD_CUT], "CUT") && str_eq(N_MDST[MD_RES], "FRES") && mod_param(MD_RES) == P_FREEB);
    }
}

/* ------------------------------------------------------------------ 2 PAGE --- */
static uint32_t uf_page(void)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].id[0] == P_FREEA && PAGES[i].id[3] == P_FREED)   /* (0.2.20: CUT RES MODE SLOPE) */
            return i;
    return NPAGES;
}
static uint32_t page_named(const char *n)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == FAM_EDIT && PAGES[i].scope == SC_TRACK && str_eq(PAGES[i].title, n))
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
    uint32_t e, ok = 1, own = 1, k = uf_page(), r, ok2 = 1, only2 = 1, np = 0;
    uint32_t pv = page_named("VCF"), pt = page_named("TRIO FILTER"), pn = page_named("NOISE FILT"), nf = 0;
    int16_t e0[8];
    for (r = 0; r < NPAGES; r++)
        nf += PAGES[r].fam == FAM_EDIT && str_eq(PAGES[r].title, "MULTI FILTER");
    expect("one page called FILTER, the multimode filter's, on every engine: K1 CUT, K2 RES, K3 MODE, K4 SLOPE (no other page is called FILTER)",
           nf == 1u && k < NPAGES && PAGES[k].fam == FAM_EDIT && str_eq(PAGES[k].title, "MULTI FILTER") && PAGES[k].scope == SC_TRACK &&
           PAGES[k].id[0] == P_FREEA && PAGES[k].id[1] == P_FREEB && PAGES[k].id[2] == P_FREEC && PAGES[k].id[3] == P_FREED);
    expect("the engines' own filters have pages of their own, after FILTER: VCF (ANALOG), TRIO FILTER, NOISE FILT (titles of 11 characters at most)",
           pv == k + 1u && pt == k + 2u && pn == k + 3u && str_len(PAGES[pt].title) <= 11u && PAGES[pv].id[0] == P_FTYPE && PAGES[pt].id[0] == P_E4 &&
           PAGES[pn].id[0] == P_E2);
    for (r = 0; r < NENG_SHOWN; r++) {
        e = eng_vis(r);
        ui_power_on();
        set_engine_of(TSEL, e);
        ok &= page_visible(k) != 0;                       /* every engine shown has it */
        ok2 &= page_visible(pv) == (ENGINES[e] == &ENG_ANALOG) && page_visible(pt) == (ENGINES[e] == &ENG_TRIO) &&
               page_visible(pn) == (ENGINES[e] == &ENG_NOISE);
        np += ENGINES[e]->ufilt != 0;
    }
    (void)own; (void)only2;
    expect("the FILTER page shows on every engine (ten shown); VCF, TRIO FILTER, NOISE FILT only on their own engine", ok && ok2 && np == NENG_SHOWN);
    ui_power_on();
    set_engine_of(TSEL, eng_named("WHEEL"));
    go_title("EDIT 1"); frame();
    ui.page = (uint8_t)k;
    page_entered();
    frame();
    ok = TSEL->p[P_FREEA] == UF_STEPS && TSEL->p[P_FREEC] == 0 && TSEL->p[P_FREED] == 1;
    turn(EN_K1, -7);
    ok &= TSEL->p[P_FREEA] == UF_STEPS - 7;
    turn(EN_K2, 5);
    ok &= TSEL->p[P_FREEB] == 5;
    turn(EN_K3, 64);
    ok &= TSEL->p[P_FREEC] == 64;
    turn(EN_K4, 1);
    ok &= TSEL->p[P_FREED] == 2;
    turn(EN_K4, -3);
    ok &= TSEL->p[P_FREED] == 1;
    turn(EN_K1, 7);
    ok &= TSEL->p[P_FREEA] == UF_STEPS;
    expect("FILTER page: KNOB 1 CUT (82 the open end), KNOB 2 RES, KNOB 3 MODE (64 BP), KNOB 4 SLOPE 12 <-> 24 dB (never below 12)", ok);
    ui_power_on();
    set_engine_of(TSEL, eng_named("TRIO"));
    go_title("EDIT 1"); frame();
    ui.page = (uint8_t)k;
    page_entered();
    frame();
    memcpy(e0, TSEL->p + P_E0, sizeof e0);
    ok = TSEL->p[P_FREEA] == UF_STEPS && TSEL->p[P_FREEC] == 0 && TSEL->p[P_FREED] == 1 && str_eq(cur_page()->title, "MULTI FILTER");
    turn(EN_K1, -7);
    ok &= TSEL->p[P_FREEA] == UF_STEPS - 7;
    turn(EN_K2, 5);
    ok &= TSEL->p[P_FREEB] == 5;
    turn(EN_K3, 64);
    ok &= TSEL->p[P_FREEC] == 64;
    turn(EN_K4, 1);
    ok &= TSEL->p[P_FREED] == 2;
    ok &= !memcmp(TSEL->p + P_E0, e0, sizeof e0);
    expect("FILTER on TRIO: KNOB 1 CUT, 2 RES, 3 MODE, 4 SLOPE set ids 45..48 (TRIO's own MODE / CUT / RES on EDIT and TRIO FILTER stay)", ok);
    ui.page = (uint8_t)pt;
    page_entered();
    frame();
    turn(EN_K2, 3);
    expect("TRIO FILTER: KNOB 2 is TRIO's own CUT (E5), not the multimode one", str_eq(cur_page()->title, "TRIO FILTER") && TSEL->p[P_E5] != e0[5] &&
           TSEL->p[P_FREEA] == UF_STEPS - 7);
}

/* ------------------------------------------------------------------ 3 BYPASS --- */
static void res_hi(track_t *t) { t->p[P_FREEB] = 127; t->p[P_FREED] = 2; }
static void open_routes(track_t *t)                      /* routes that only open (positive, unipolar sources): at the open end they stay there */
{
    t->p[P_ED_FLT] = 40; t->p[P_E2FLT] = 60; t->p[P_E2SUS] = 127;
    t->p[P_M1SRC] = MS_VEL; t->p[P_M1DST] = MD_CUT; t->p[P_M1AMT] = 40;
}
static void routes_on(track_t *t)
{
    t->p[P_ED_FLT] = -40; t->p[P_LD_FLT] = 50; t->p[P_E2FLT] = 60; t->p[P_L2FLT] = 30;
    t->p[P_E2SUS] = 127; t->p[P_L2PIT] = 0; t->p[P_LRATE] = 90;
}
static void routes_zero(track_t *t)                      /* every route present but at depth 0 / amount 0 */
{
    t->p[P_ED_FLT] = t->p[P_LD_FLT] = t->p[P_E2FLT] = t->p[P_L2FLT] = 0;
    t->p[P_E2SUS] = 127; t->p[P_LRATE] = 90;
    t->p[P_M1SRC] = MS_VEL; t->p[P_M1DST] = MD_CUT; t->p[P_M1AMT] = 0;
    t->p[P_M2SRC] = MS_ENV2; t->p[P_M2DST] = MD_CUT; t->p[P_M2AMT] = 0;
}
static double diff_rel(const int32_t *ref, double ref_rms)   /* rms of (rec_l - ref) over the second half, against the signal's */
{
    double d = 0;
    uint32_t j;
    for (j = SPEC_FROM; j < rec_n; j++)
        d += ((double)rec_l[j] - ref[j]) * ((double)rec_l[j] - ref[j]);
    return sqrt(d / (rec_n - SPEC_FROM)) / (ref_rms + 1e-9);
}
static void t_bypass(void)
{
    uint32_t i, ok_res = 1, ok_open = 1, ok_routes = 1, ok_zero = 1, ok_near = 1;
    for (i = 0; i < NELEM(UF_ENGS); i++) {
        uint32_t e = eng_named(UF_ENGS[i]);
        uint64_t h0;
        double s0, rel;
        static int32_t keep[MAXB * CTL];
        render(e, 0, 100);
        h0 = rec_hash();
        s0 = rms_of(rec_l + SPEC_FROM, rec_n - SPEC_FROM);
        memcpy(keep, rec_l, rec_n * sizeof keep[0]);
        render(e, res_hi, 100);
        ok_res &= rec_hash() == h0;
        render(e, open_routes, 100);
        ok_open &= rec_hash() == h0;
        render(e, routes_zero, 100);
        ok_zero &= rec_hash() == h0;
        render(e, routes_on, 100);                         /* routes that close it: the filter runs */
        ok_routes &= rec_hash() != h0;
        sf(UF_STEPS - 1, 0, 0, 1);                         /* one step below the open end: running, nearly transparent */
        render(e, set_f, 100);
        rel = diff_rel(keep, s0);
        ok_near &= rel < 0.45 && s0 > 100;
        printf("uf:   %-6s CUT 81 (LP 14 kHz): %.3f of the signal's rms (%.0f)\n", UF_ENGS[i], rel, s0);
    }
    expect("the defaults (MODE LP, CUT 82): RES 127 and SLOPE 24 dB change nothing, bit for bit, on all nine engines (the filter does not run)", ok_res);
    expect("  .. routes that only open it (ENV 1 / ENV 2 FLT +, VEL -> CUT +) keep it at the open end: bit for bit", ok_open);
    expect("  .. every route assigned at 0 (depths 0, matrix CUT routes at AMT 0): bit for bit", ok_zero);
    expect("  .. routes that close it (ENV 1 FLT -40, the LFOs) run it: the render differs", ok_routes);
    expect("CUT 81 (one step in) is nearly transparent on every engine: it runs on all of them, additively", ok_near);
    {   /* the exact rule: the bypass is MODE LP at the open end; every other MODE runs there */
        uint32_t e = eng_named("PHASE");
        uint64_t h0, h1, h2;
        render(e, 0, 100);
        h0 = rec_hash();
        sf(UF_STEPS, 0, 1, 1);
        render(e, set_f, 100);
        h1 = rec_hash();
        sf(UF_STEPS, 0, 127, 1);
        render(e, set_f, 100);
        h2 = rec_hash();
        expect("the bypass is MODE LP (0) only: MODE 1 at the open end runs the filter, MODE HP there is the closed high-pass (silent)",
               h1 != h0 && h2 != h0 && peak_all(SPEC_FROM) == 0);
    }
}

static void routes_on_res(track_t *t) { routes_on(t); res_hi(t); }
/* ufilt 2 (ANALOG, TRIO, NOISE): the multimode filter in series after the engine's own; the defaults are the bypass, bit for bit what the engine
 * rendered before (the goldens: regress); the engine's routes drive its own filter and never this one */
static void t_bypass_series(void)
{
    uint32_t i, ok_res = 1, ok_routes = 1, ok_own = 1, ok_run = 1, ok_lp = 1, ok_hp = 1, ok_slope = 1, ok_near = 1;
    for (i = 0; i < NELEM(SER_ENGS); i++) {
        uint32_t e = eng_named(SER_ENGS[i]);
        uint64_t h0, h1;
        double hi0, lo0, hi12, hi24, lo_hp;
        render(e, 0, 100);
        h0 = rec_hash();
        hi0 = hi_db();
        lo0 = lo_db();
        render(e, res_hi, 100);
        ok_res &= rec_hash() == h0;                        /* RES 127 and SLOPE 24 dB at MODE LP, CUT 82: not run */
        render(e, routes_on, 100);                         /* ENV 1 / ENV 2 / LFO 1 / LFO 2 FLT: the engine's own filter moves ... */
        h1 = rec_hash();
        ok_own &= h1 != h0;
        render(e, routes_on_res, 100);                     /* ... and never close the multimode filter: it stays bypassed (else RES 127 / 24 dB would change it) */
        ok_routes &= rec_hash() == h1;
        sf(UF_STEPS - 1, 0, 0, 1);
        render(e, set_f, 100);
        ok_near &= rec_hash() != h0;                        /* one step below the open end it runs */
        sf(58, 0, 0, 1);                                   /* LP at 2.6 kHz, 12 dB */
        render(e, set_f, 100);
        hi12 = hi_db();
        ok_lp &= hi12 < hi0 - 6.0;
        sf(58, 0, 0, 2);
        render(e, set_f, 100);
        hi24 = hi_db();
        ok_slope &= hi24 <= hi12 + 0.5 && (hi0 < 90.0 || hi24 < hi12 - 3.0);   /* (TRIO's highs are at the measuring floor (62 dB) already) */
        sf(60, 0, UF_HP, 1);                               /* HP at 3.4 kHz */
        render(e, set_f, 100);
        lo_hp = lo_db();
        ok_hp &= lo_hp < lo0 - 10.0;
        sf(UF_STEPS, 0, UF_HP, 1);                         /* MODE HP at the open end: the closed high-pass, silent */
        render(e, set_f, 100);
        ok_run &= peak_all(SPEC_FROM) == 0;
        printf("uf:   %-6s highs (4..12 kHz) %.0f dB bypassed, LP CUT 58: %.0f (12 dB) %.0f (24 dB); lows (40..220 Hz) %.0f dB, HP CUT 60: %.0f\n",
               SER_ENGS[i], hi0, hi12, hi24, lo0, lo_hp);
    }
    sf(UF_STEPS, 0, 0, 1);
    expect("ANALOG TRIO NOISE, the defaults (MODE LP, CUT 82): RES 127 and SLOPE 24 dB change nothing, bit for bit (the multimode filter does not run)", ok_res);
    expect("  .. the engine's routes (ENV 1 / ENV 2 / LFO 1 / LFO 2 FLT) move its own filter (the render differs) and never switch the multimode one on", ok_own && ok_routes);
    expect("  .. CUT 81 (one step in) runs it; a low-pass at CUT 58 takes the highs down 6 dB or more, 24 dB 3 dB more than 12 dB; a high-pass at CUT 60 the lows 10 dB",
           ok_near && ok_lp && ok_slope && ok_hp);
    expect("  .. MODE HP at CUT 82 (the closed high-pass) is silent: any MODE but LP runs the filter at the open end", ok_run);
}

/* ------------------------------------------------------------------ 4 RESPONSE --- */
#define TAMP 16000.0
/* the gain of uf_run at frequency f (a sine, from rest: 0.3 s to settle, 0.3 s measured), CUT step c (Q8 any), MODE, RES, SLOPE; the
 * bypass (MODE LP at the open end): 1 */
static double uf_gain(int32_t cq8, int32_t mode, int32_t res, int32_t slope, double f)
{
    voice_t v;
    int16_t p[P_COUNT];
    int32_t b[CTL], in[CTL];
    double si = 0, sq = 0, oi = 0, oq = 0;
    uint32_t i, j, N = (uint32_t)(0.6 * FS) / CTL * CTL, from = (uint32_t)(0.3 * FS);
    if (cq8 >= UF_TOP && !mode)
        return 1.0;
    memset(&v, 0, sizeof v);
    memset(p, 0, sizeof p);
    p[P_FREEB] = (int16_t)res;
    p[P_FREEC] = (int16_t)mode;
    p[P_FREED] = (int16_t)slope;
    for (i = 0; i < N; i += CTL) {
        for (j = 0; j < CTL; j++)
            in[j] = b[j] = (int32_t)lround(TAMP * sin(2 * M_PI * f * (i + j) / FS));
        uf_run(&v, b, CTL, cq8, p);
        if (i >= from)
            for (j = 0; j < CTL; j++) {
                double ph = 2 * M_PI * f * (i + j) / FS, s_ = sin(ph), c_ = cos(ph);
                si += in[j] * s_; sq += in[j] * c_; oi += b[j] * s_; oq += b[j] * c_;
            }
    }
    return sqrt(oi * oi + oq * oq) / sqrt(si * si + sq * sq);
}
static double db_of(double g) { return g <= 0 ? -999.0 : 20 * log10(g); }
static void pr_db(double d)
{
    if (d <= -998)
        printf("   -inf");
    else
        printf(" %6.1f", d);
}
/* the -3 dB corner (the passband's side: LP the first frequency above any peak where the gain falls under -3, HP the last below it)
 * and the slope over the octave beyond it, dB */
static void corner(int32_t c, int32_t mode, int32_t res, int32_t slope, int hp, double *fc, double *sl)
{
    double fs_[80], g[80], pk = -999, pkf = 0, f;
    int n = 0, q;
    *fc = 0; *sl = 0;
    for (f = 10; f < 20000 && n < 80; f *= pow(2, 1.0 / 6)) {
        fs_[n] = f;
        g[n++] = db_of(uf_gain(c << 8, mode, res, slope, f));
    }
    for (q = 0; q < n; q++)
        if (g[q] > pk) { pk = g[q]; pkf = fs_[q]; }
    if (!hp) {
        for (q = 1; q < n; q++)
            if (g[q] < -3 && fs_[q] > (pk > 0.5 ? pkf : 0) && g[q - 1] >= -3) {
                *fc = fs_[q - 1] * pow(fs_[q] / fs_[q - 1], (g[q - 1] + 3) / (g[q - 1] - g[q]));
                break;
            }
    } else {
        for (q = n - 2; q >= 0; q--)
            if (g[q] < -3 && fs_[q] < (pk > 0.5 ? pkf : 1e9) && g[q + 1] >= -3) {
                *fc = fs_[q + 1] * pow(fs_[q] / fs_[q + 1], (g[q + 1] + 3) / (g[q + 1] - g[q]));
                break;
            }
    }
    if (*fc > 0) {
        double a = hp ? *fc / 2 : *fc * 2, b = hp ? *fc / 4 : *fc * 4;
        if (b > 10 && b < 20000)
            *sl = db_of(uf_gain(c << 8, mode, res, slope, b)) - db_of(uf_gain(c << 8, mode, res, slope, a));
    }
}
static void t_response(void)
{
    static const double F[] = {50, 100, 200, 500, 1000, 2000, 5000, 10000};
    static const struct { int32_t cut, mode; } R[] = {
        {0, 0}, {2, 0}, {4, 0}, {20, 0}, {41, 0}, {62, 0}, {81, 0}, {82, 0},
        {0, 127}, {20, 127}, {41, 127}, {62, 127}, {78, 127}, {80, 127}, {82, 127},
        {41, 64}, {41, 32}, {41, 96}, {0, 64}, {82, 64}};
    uint32_t r, q, s, full = getenv("UF_TABLE") != 0, res;
    int ok_kill = 1, ok_lp24 = 1, ok_lp12 = 1, ok_hp = 1, ok_slope = 1, ok_bp = 1, ok_blend = 1, ok_res = 1;
    printf("uf: the response, dB at 50 100 200 500 1k 2k 5k 10k Hz (a sine through uf_run); the -3 dB corner and the slope over the octave beyond it\n");
    for (res = 0; res < (full ? 2u : 1u); res++)
        for (s = 1; s <= 2u; s++) {
            printf("uf:   SLOPE %s, RES %s   CUT (nominal)  MODE |", s == 2u ? "24 dB" : "12 dB", res ? "80 %" : "0");
            for (q = 0; q < NELEM(F); q++)
                printf(" %6.0f", F[q]);
            printf(" |  corner  slope/oct\n");
            for (r = 0; r < NELEM(R); r++) {
                double d[NELEM(F)], fc = 0, sl = 0;
                int32_t c = R[r].cut, m = R[r].mode, rr = res ? 102 : 0;
                for (q = 0; q < NELEM(F); q++)
                    d[q] = db_of(uf_gain(c << 8, m, rr, (int32_t)s, F[q]));
                if (m == 0 || m == 127)
                    corner(c, m, rr, (int32_t)s, m == 127, &fc, &sl);
                printf("uf:   %21d %6.0f Hz %-5s |", (int)c, uf_hz(c), m == 0 ? "LP" : m == 64 ? "BP" : m == 127 ? "HP" : m < 64 ? "LP>BP" : "BP>HP");
                for (q = 0; q < NELEM(F); q++)
                    pr_db(d[q]);
                if (c == UF_STEPS && !m)
                    printf(" |  bypass\n");
                else if (fc > 0)
                    printf(" | %6.0f Hz  %+5.1f\n", fc, sl);
                else
                    printf(" |\n");
                if (res)
                    continue;
                if ((c == 0 && m <= 64) || (c == UF_STEPS && m >= 64))   /* a closed end: the kill zone, silent */
                    for (q = 0; q < NELEM(F); q++)
                        ok_kill &= d[q] <= -998;
                if (c == 4 && !m) {                         /* just out of the kill zone (41 Hz): 1 kHz far down */
                    if (s == 2u)
                        ok_lp24 &= d[4] < -60.0;
                    else
                        ok_lp12 &= d[4] < -40.0;
                }
                if (m == 127 && c == 78)                    /* the high-pass near its top (11.8 kHz): everything below 2 kHz far down */
                    ok_hp &= d[5] < (s == 2u ? -60.0 : -30.0) && d[7] > (s == 2u ? -12.0 : -6.0);
                if (((m == 0 && (c == 20 || c == 41)) || (m == 127 && (c == 41 || c == 62))) && fc > 0)   /* the slope: 12 / 24 dB an octave, near enough
                                                         * (away from the table's ends: the bilinear warp near 22 kHz, the fixed point's floor near 30 Hz) */
                    ok_slope &= s == 2u ? sl < -20.0 && sl > -27.0 : sl < -9.0 && sl > -13.5;
                if (m == 64 && c == 41) {                   /* the band-pass round 690 Hz: its peak there, both sides down */
                    ok_bp &= d[3] > -3.0 && d[4] > (s == 2u ? -4.5 : -3.5) && d[0] < -12.0 && d[7] < -12.0;
                }
                if (m == 32 && c == 41)                     /* LP>BP: between the two, nothing beyond either */
                    ok_blend &= d[0] < 0.5 && d[0] > -12.0 && d[7] < -12.0;
                if (m == 96 && c == 41)
                    ok_blend &= d[7] < 0.5 && d[7] > -12.0 && d[0] < -12.0;
            }
        }
    {   /* RES: a peak at the cutoff, 12 and 24 dB */
        double a12 = db_of(uf_gain(41 << 8, 0, 0, 1, uf_hz(41))), b12 = db_of(uf_gain(41 << 8, 0, 127, 1, uf_hz(41)));
        double a24 = db_of(uf_gain(41 << 8, 0, 0, 2, uf_hz(41))), b24 = db_of(uf_gain(41 << 8, 0, 127, 2, uf_hz(41)));
        printf("uf:   at the cutoff (CUT 41, %.0f Hz): 12 dB RES 0 %.1f dB, RES 127 %.1f dB; 24 dB RES 0 %.1f dB, RES 127 %.1f dB\n", uf_hz(41), a12,
               b12, a24, b24);
        ok_res = b12 > a12 + 12.0 && b24 > a24 + 12.0 && b24 < 24.0 && b12 < 24.0;
    }
    expect("KILL ZONES: the closed ends are silent (LP / BP at CUT 0, HP / BP at CUT 82: the output exactly 0), 12 and 24 dB", ok_kill);
    expect("LP 24 dB at CUT 4 (41 Hz, out of the kill zone): 1 kHz more than 60 dB down", ok_lp24);
    expect("LP 12 dB at CUT 4: 1 kHz more than 40 dB down", ok_lp12);
    expect("HP at CUT 78 (11.8 kHz): 2 kHz more than 30 dB (12 dB) / 60 dB (24 dB) down, 10 kHz within 6 dB (12 dB) / 12 dB (24 dB)", ok_hp);
    expect("the slope beyond the corner: 9..13.5 dB an octave at 12 dB, 20..27 at 24 dB (LP at CUT 20 and 41, HP at 41 and 62)", ok_slope);
    expect("BP (MODE 64) at CUT 41: the band round 690 Hz within 3.5 dB (24 dB: 4.5), 50 Hz and 10 kHz 12 dB down or more", ok_bp);
    expect("MODE 32 (LP>BP) and 96 (BP>HP): the blends, one end kept, the other 12 dB down, nothing above 0.5 dB", ok_blend);
    expect("RES 127 raises the cutoff by 12 dB or more over RES 0 (12 and 24 dB), below 24 dB", ok_res);
}

/* ------------------------------------------------------------------ 4b BASS AT HIGH RES --- */
/* a dial step (Q8) for a cutoff in Hz: the inverse of uf_hz */
static int32_t uf_q8_of_hz(double hz) { return (int32_t)lround((log(hz / 30.0) / log(16000.0 / 30.0) * 127.0 * 256.0 - 40.0) / 396.0 * 256.0); }
/* the peak of uf_run's output for a saw of amplitude amp and frequency f (after 0.5 s to settle, 1 s measured) */
static int32_t uf_saw_peak(int32_t cq8, int32_t res, int32_t slope, double f, int32_t amp)
{
    voice_t v;
    int16_t p[P_COUNT];
    int32_t b[CTL], pk = 0;
    uint32_t i, j, N = (uint32_t)(1.5 * FS) / CTL * CTL;
    memset(&v, 0, sizeof v);
    memset(p, 0, sizeof p);
    p[P_FREEB] = (int16_t)res;
    p[P_FREED] = (int16_t)slope;
    for (i = 0; i < N; i += CTL) {
        for (j = 0; j < CTL; j++) {
            double ph = fmod(f * (i + j) / FS, 1.0);
            b[j] = (int32_t)lround(amp * (2 * ph - 1));
        }
        uf_run(&v, b, CTL, cq8, p);
        if (i >= (uint32_t)(0.5 * FS))
            for (j = 0; j < CTL; j++)
                if (abs(b[j]) > pk)
                    pk = abs(b[j]);
    }
    return pk;
}
static void t_bass(void)
{
    static const double CUTS[] = {100, 200, 500, 1000, 4000}, TONES[] = {40, 60, 100, 200}, RESS[] = {0, 64, 100, 127};
    uint32_t s, c, r, q;
    double worst = 0;                                           /* the largest drop of a tone at least an octave and a half below the cutoff, dB */
    printf("uf: bass at high RES (LP): dB of a sine at 40 60 100 200 Hz, and the resonant peak (dB, a sine swept round the cutoff, max)\n");
    for (s = 1; s <= 2u; s++)
        for (c = 0; c < NELEM(CUTS); c++)
            for (r = 0; r < NELEM(RESS); r++) {
                int32_t cq = uf_q8_of_hz(CUTS[c]);
                double pk = -999, f;
                printf("uf:   SLOPE %s CUT %5.0f Hz RES %3d |", s == 2u ? "24" : "12", CUTS[c], (int)RESS[r]);
                for (q = 0; q < NELEM(TONES); q++) {
                    double d = db_of(uf_gain(cq, 0, (int32_t)RESS[r], (int32_t)s, TONES[q])), d0 = db_of(uf_gain(cq, 0, 0, (int32_t)s, TONES[q]));
                    pr_db(d);
                    if (TONES[q] * 2.8 <= CUTS[c] && d0 - d > worst)
                        worst = d0 - d;
                    if (TONES[q] * 2.8 <= CUTS[c] && d - d0 > worst)
                        worst = d - d0;
                }
                for (f = CUTS[c] * 0.5; f < CUTS[c] * 2; f *= 1.04) {
                    double d = db_of(uf_gain(cq, 0, (int32_t)RESS[r], (int32_t)s, f));
                    if (d > pk)
                        pk = d;
                }
                printf(" | peak %6.1f\n", pk);
            }
    {   /* a full-scale saw (the voice's level, 48000) at a 100 Hz cutoff, RES 127, both slopes: the output's peak against the knee */
        int32_t cq = uf_q8_of_hz(100);
        double fo[] = {27.5, 55, 82.4, 110, 220};
        uint32_t k;
        int32_t worstpk = 0, amps[2] = {24000, 48000}, a;
        for (a = 0; a < 2; a++)
          for (s = 1; s <= 2u; s++)
            for (k = 0; k < NELEM(fo); k++) {
                int32_t pk = uf_saw_peak(cq, 127, (int32_t)s, fo[k], amps[a]), p0 = uf_saw_peak(cq, 0, (int32_t)s, fo[k], amps[a]);
                printf("uf:   saw +-%d at %5.1f Hz, CUT 100 Hz, SLOPE %s: peak RES 0 %d, RES 127 %d (%+.1f dB re the input peak)\n",
                       amps[a], fo[k], s == 2u ? "24" : "12", p0, pk, 20 * log10((double)pk / amps[a]));
                if (a == 0 && pk > worstpk)
                    worstpk = pk;
            }
        printf("uf:   largest peak at RES 127 of a +-24000 saw (the voice's full level): %d (%.1f dB re 24000; the knee starts at 48000)\n", worstpk, 20 * log10(worstpk / 24000.0));
        expect("RES 127: a full-level saw (+-24000) at a 100 Hz cutoff never passes the knee's ceiling (65536), 12 and 24 dB (measured 59144 = +7.8 dB re the voice's level at 82.4 Hz, SLOPE 12)", worstpk < 65536);
    }
    printf("uf:   largest bass change with RES (tones 2.8x or more under the cutoff): %.2f dB\n", worst);
    expect("bass is kept at high RES: a tone 2.8x or more under the cutoff stays within 1 dB of its RES 0 level at RES 64..127 (12 and 24 dB)", worst <= 1.0);
}

/* ------------------------------------------------------------------ 5 EXACT --- */
/* 0.2.8's uf_run (voice.c), kept verbatim as the reference: the bipolar CUT c (Q8, -64..63 << 8), 0.2.2's dial */
typedef struct { int32_t uf[2]; uint8_t uft; } ref_v_t;
static void ref_uf_run(ref_v_t *v, int32_t *buf, uint32_t n, int32_t c, int32_t k)
{
    tsvf_t f;
    uint32_t i, hp = c > 0, kind = 1u + hp;
    int32_t ic1 = v->uf[0], ic2 = v->uf[1];
    if (v->uft != kind) {
        ic1 = ic2 = 0;
        v->uft = (uint8_t)kind;
    }
    tsvf_coef_k(&f, hp ? (c * 99) >> 6 : (127 << 8) + ((c * 99) >> 6), k);
    if (!hp && !ic1 && !ic2)
        ic2 = buf[0] >> 1;
    for (i = 0; i < n; i++) {
        int32_t x = buf[i] >> 1, v3 = x - ic2, v1 = (f.a1 * ic1 + f.a2 * v3) >> 13, v2 = ic2 + ((f.a2 * ic1 + f.a3 * v3) >> 13), y;
        ic1 = clamp(2 * v1 - ic1, -150000, 150000);
        ic2 = clamp(2 * v2 - ic2, -150000, 150000);
        y = hp ? x - ((k * v1) >> 12) - v2 : v2;
        buf[i] = soft_knee(y << 1, UF_KNEE);
    }
    v->uf[0] = ic1;
    v->uf[1] = ic2;
}
static void t_exact(void)
{
    int32_t c, ok = 1, res;
    uint32_t blk, j, nsame = 0, nblk = 0;
    double dmax = 0, dres = 0;
    for (c = -(64 << 8); c < 0; c++)                   /* the cutoff index, every effective value of a low-pass of before */
        ok &= (127 << 8) + ((c * 99) >> 6) == 40 + ((((UF_STEPS << 8) + c) * 99) >> 6);
    expect("every low-pass of before (effective -64..-1/256): today's dial (82 + old) gives the same cutoff index exactly", ok);
    for (res = 0; res <= 127; res += 127) {               /* a saw with noise, the cutoff and RES moving every block: both filters, bit for bit */
        for (int hp = 0; hp < 2; hp++) {
            ref_v_t rv;
            voice_t v;
            int16_t p[P_COUNT];
            uint32_t rs = 12345u, ph = 0;
            double e = 0, d = 0;
            memset(&rv, 0, sizeof rv);
            memset(&v, 0, sizeof v);
            memset(p, 0, sizeof p);
            p[P_FREEB] = (int16_t)res;
            p[P_FREEC] = hp ? UF_HP : 0;
            p[P_FREED] = 1;
            for (blk = 0; blk < 2000u; blk++) {
                int32_t a[CTL], b[CTL], cq;
                rs = rs * 1664525u + 1013904223u;
                cq = hp ? (int32_t)(1 + (rs >> 8) % (63u << 8)) : -(int32_t)(1 + (rs >> 8) % (64u << 8));   /* (old: -64..-1 LP, 1..63 HP, Q8) */
                for (j = 0; j < CTL; j++) {
                    ph += 0x01234567u;
                    rs = rs * 1664525u + 1013904223u;
                    a[j] = b[j] = (int32_t)(ph >> 17) - 16384 + (int32_t)(rs >> 22) - 512;
                }
                ref_uf_run(&rv, a, CTL, cq, UF_K(res));
                uf_run(&v, b, CTL, hp ? cq : (UF_STEPS << 8) + cq, p);
                if (!hp)
                    nsame += !memcmp(a, b, sizeof a);
                for (j = 0; j < CTL; j++) {
                    e += (double)a[j] * a[j];
                    d += ((double)a[j] - b[j]) * ((double)a[j] - b[j]);
                }
                nblk += !hp;
            }
            if (hp && !res)
                dmax = sqrt(d / e);
            else if (hp)
                dres = sqrt(d / e);
        }
    }
    printf("uf:   2 x 2000 blocks, a saw + noise, the cutoff moving every block: LP %u of %u blocks bit for bit; HP difference %.4f of the signal "
           "(rms) at RES 0, %.4f at RES 127 (a resonant peak moves with its 0.01 octave)\n", nsame, nblk, dmax, dres);
    expect("a low-pass of before through 0.2.8's uf_run and through today's (MODE LP, CUT 82 + old): bit for bit, RES 0 and 127", nsame == nblk);
    expect("a high-pass of before through today's (MODE HP, CUT = old: 0.16 of a table step higher): within 2 % of the signal (rms) at RES 0, 10 % at 127",
           dmax < 0.02 && dres < 0.10);
}

/* ------------------------------------------------------------------ 6 SWEEPS --- */
/* part 1 on engine e, a sustained chord, the filter set by the block: fn(t, block) before each block. 1.5 s into rec_l */
typedef void (*blkfn_t)(track_t *t, uint32_t b);
static blkfn_t S_FN;
static void render_blk_child(uint32_t e)
{
    static const uint8_t CH[4] = {48, 55, 60, 64};
    track_t *t = &trk[0];
    uint32_t i, b;
    dsp_clean();
    host_preset(t, e, 0);
    t->p[P_VOICE] = V_POLY;
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_SUS] = 127; t->p[P_ATK] = 0; t->p[P_AMODE] = 0; t->p[P_TRIM] = 60;
    t->p[P_ED_FLT] = t->p[P_LD_FLT] = t->p[P_E2FLT] = t->p[P_L2FLT] = 0;
    set_f(t);
    rec_n = 0;
    for (i = 0; i < 4u; i++)
        trk_note_on(t, CH[i], 100);
    for (b = 0; b < (uint32_t)(1.5 * FS / CTL); b++) {
        S_FN(t, b);
        mblk(1);
    }
}
static void render_blk(uint32_t e, blkfn_t fn)
{
    pid_t pid;
    S_FN = fn;
    fflush(stdout);
    if (!(pid = fork())) {
        render_blk_child(e);
        _exit(0);
    }
    waitpid(pid, 0, 0);
}
static int32_t max_step_from(uint32_t from)               /* the largest sample-to-sample jump of the render from sample `from` */
{
    int32_t m = 0;
    uint32_t i;
    for (i = from + 1u; i < rec_n; i++) {
        int32_t d = rec_l[i] - rec_l[i - 1u];
        m = d < 0 ? (-d > m ? -d : m) : (d > m ? d : m);
    }
    return m;
}
static void fb_static(track_t *t, uint32_t b) { (void)b; set_f(t); }
static void fb_mode(track_t *t, uint32_t b)              /* MODE 0 .. 127 .. 0 a step a block, from block 200 */
{
    uint32_t k = b < 200u ? 0u : (b - 200u) % 254u;
    set_f(t);
    t->p[P_FREEC] = (int16_t)(k < 127u ? k : 254u - k);
}
static void fb_mode_jump(track_t *t, uint32_t b)         /* MODE LP <-> HP every 100 blocks (a jump of the whole range) */
{
    set_f(t);
    t->p[P_FREEC] = (int16_t)(b / 100u % 2u ? UF_HP : 0);
}
static void fb_cut(track_t *t, uint32_t b)               /* CUT 30 down to 0 and back, a step every 4 blocks (through the low kill zone) */
{
    uint32_t k = b < 200u ? 0u : (b - 200u) / 4u % 60u;
    set_f(t);
    t->p[P_FREEA] = (int16_t)(k < 30u ? 30u - k : k - 30u);
}
static void fb_cut_hp(track_t *t, uint32_t b)            /* HP: CUT 60 up to 82 and back (through the high kill zone) */
{
    uint32_t k = b < 200u ? 0u : (b - 200u) / 4u % 44u;
    set_f(t);
    t->p[P_FREEA] = (int16_t)(k < 22u ? 60u + k : 104u - k);
}
static void fb_slope(track_t *t, uint32_t b) { set_f(t); t->p[P_FREED] = (int16_t)(b / 100u % 2u ? 2 : 1); }
static void t_sweeps(void)
{
    static const char *const E[3] = {"PHASE", "WHEEL", "WAVE"};
    uint32_t k, ok_mode = 1, ok_cut = 1, ok_hp = 1, ok_slope = 1, ok_jump = 1;
    for (k = 0; k < 3u; k++) {
        uint32_t e = eng_named(E[k]), from = 150u * CTL;
        int32_t ref = 0, st, m, r;
        for (m = 0; m <= 127; m += 32) {                 /* the static renders' own largest steps: MODE 0 32 64 96 127 at CUT 41, RES 0 / 100 */
            for (r = 0; r <= 100; r += 100) {
                sf(41, r, m > 127 ? 127 : m, 1);
                render_blk(e, fb_static);
                st = max_step_from(from);
                ref = st > ref ? st : ref;
            }
        }
        sf(41, 100, 127, 1);
        render_blk(e, fb_static);
        ref = max_step_from(from) > ref ? max_step_from(from) : ref;
        sf(41, 100, 0, 1);
        render_blk(e, fb_mode);
        st = max_step_from(from);
        ok_mode &= st <= ref * 5 / 4 + 64 && peak_all(0) < 32768;
        printf("uf:   %-6s MODE swept 0 .. 127 .. 0 (CUT 41, RES 100): largest step %d (the static renders' %d)\n", E[k], st, ref);
        sf(41, 100, 0, 1);
        render_blk(e, fb_mode_jump);
        st = max_step_from(from);
        ok_jump &= st <= ref * 5 / 4 + 64;
        printf("uf:   %-6s MODE jumping LP <-> HP (a knob turned fast): largest step %d\n", E[k], st);
        sf(30, 60, 0, 2);
        render_blk(e, fb_cut);
        st = max_step_from(from);
        ok_cut &= st <= ref * 5 / 4 + 64;
        printf("uf:   %-6s LP 24 dB, CUT 30 .. 0 .. 30 (the low kill zone): largest step %d\n", E[k], st);
        sf(60, 60, 127, 2);
        render_blk(e, fb_cut_hp);
        st = max_step_from(from);
        ok_hp &= st <= ref * 5 / 4 + 64;
        printf("uf:   %-6s HP 24 dB, CUT 60 .. 82 .. 60 (the high kill zone): largest step %d\n", E[k], st);
        sf(41, 60, 0, 1);
        render_blk(e, fb_slope);
        st = max_step_from(from);
        ok_slope &= st <= ref * 5 / 4 + 64;
        printf("uf:   %-6s SLOPE 12 <-> 24 dB every 100 blocks: largest step %d\n", E[k], st);
    }
    expect("MODE swept LP .. BP .. HP and back a step a block: no click (largest step within 1.25 x the static renders'), PHASE WHEEL WAVE", ok_mode);
    expect("MODE jumping LP <-> HP (a whole-range jump between two blocks): the weights ramp over the block, no click", ok_jump);
    expect("CUT through the low kill zone (LP 24 dB, 30 .. 0 .. 30): no click", ok_cut);
    expect("CUT through the high kill zone (HP 24 dB, 60 .. 82 .. 60): no click", ok_hp);
    expect("SLOPE 12 <-> 24 dB while sounding: no click (the second stage fades in / out over a block)", ok_slope);
}

/* ------------------------------------------------------------------ 7 ROUTES --- */
static void r_env1(track_t *t) { set_f(t); t->p[P_ED_FLT] = 40; }
static void r_env2(track_t *t) { set_f(t); t->p[P_E2FLT] = 40; t->p[P_E2SUS] = 127; }
static void r_lfo1(track_t *t) { set_f(t); t->p[P_LD_FLT] = 60; t->p[P_LRATE] = 100; t->p[P_LWAVE] = 2; }
static void r_lfo2(track_t *t) { set_f(t); t->p[P_L2FLT] = 60; t->p[P_L2RATE] = 100; t->p[P_L2WAVE] = 2; }
static void r_vel(track_t *t) { set_f(t); t->p[P_M1SRC] = MS_VEL; t->p[P_M1DST] = MD_CUT; t->p[P_M1AMT] = 63; }
static void r_env(track_t *t) { set_f(t); t->p[P_M1SRC] = MS_ENV; t->p[P_M1DST] = MD_CUT; t->p[P_M1AMT] = 40; }
static void r_at(track_t *t) { set_f(t); t->p[P_M1SRC] = MS_AT; t->p[P_M1DST] = MD_CUT; t->p[P_M1AMT] = 40; t->at = 127; }
static void r_lfo2m(track_t *t) { set_f(t); t->p[P_M1SRC] = MS_LFO2; t->p[P_M1DST] = MD_CUT; t->p[P_M1AMT] = 40; t->p[P_L2RATE] = 100; t->p[P_L2WAVE] = 2; }
static void r_fres(track_t *t) { set_f(t); t->p[P_M1SRC] = MS_VEL; t->p[P_M1DST] = MD_RES; t->p[P_M1AMT] = 63; }
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
    uint32_t i, k, ok = 1, s;
    static const char *const E[2] = {"PHASE", "WAVE"};   /* (GRAIN until 0.2.8) */
    for (s = 1; s <= 2u; s++)
        for (k = 0; k < 2u; k++) {
            double base;
            sf(37, 0, 0, (int32_t)s);                      /* (0.2.2's CUT -45: 82 - 45) */
            render(eng_named(E[k]), set_f, 100);
            base = hi_db();
            for (i = 0; i < NELEM(R); i++) {
                double h, sw;
                render(eng_named(E[k]), R[i].s, 100);
                h = hi_db();
                sw = hi_swing();
                printf("uf:   %-6s %s CUT 37 + %-24s highs %+.1f dB over CUT alone, swing %.1f dB\n", E[k], s == 2u ? "24 dB" : "12 dB", R[i].n, h - base, sw);
                if (R[i].moves)
                    ok &= sw > 1.5;
                else
                    ok &= h > base + 4.0;
            }
        }
    expect("PHASE, WAVE at CUT 37, 12 and 24 dB: ENV 1 / ENV 2 FLT, matrix VEL / ENV / AT -> CUT open the filter; LFO 1 / LFO 2 FLT, matrix LFO2 -> CUT sweep it", ok);
    {   /* the sum clamps: a deep route cannot leave the dial (the open end at most) */
        sf(70, 0, 0, 1);
        render(eng_named("PHASE"), r_vel, 100);
        expect("  .. the sum is clamped to 0..82 (CUT 70 + 63: the open end, a sound, no overflow)", peak_all(SPEC_FROM) > 100 && peak_all(0) < 32768);
    }
    {   /* RES from the matrix (FRES) */
        double a, b;
        sf(54, 40, 0, 1);
        render(eng_named("PHASE"), set_f, 100);
        a = band_db(SPEC_FROM, 1400.0, 2600.0);
        render(eng_named("PHASE"), r_fres, 100);
        b = band_db(SPEC_FROM, 1400.0, 2600.0);
        expect("matrix VEL -> FRES +63 raises the resonance (the band round the cutoff, 1.8 kHz, up 2 dB or more)", b > a + 2.0);
    }
    {   /* ANALOG: its own filter, unchanged; the multimode one in series after it (FILTER) */
        uint64_t h0, h1, h2, h3, h4;
        uint32_t an = eng_named("ANALOG");
        sf(UF_STEPS, 0, 0, 1);
        render(an, 0, 100);
        h0 = rec_hash();
        sf(20, 100, 64, 2);
        render(an, set_f, 100);
        h1 = rec_hash();
        sf(UF_STEPS, 0, 0, 1);
        render(an, r_env2, 100);
        h2 = rec_hash();
        render(an, 0, 100);
        h3 = rec_hash();
        sf(UF_STEPS, 127, 0, 2);
        render(an, r_env2, 100);
        h4 = rec_hash();
        expect("ANALOG: FILTER (BP, 24 dB) changes the render; ENV 2 FLT still moves ANALOG's own CUT and nothing else (RES 127 / 24 dB do not matter bypassed); defaults again: as at first",
               h1 != h0 && h2 != h0 && h3 == h0 && h4 == h2);
    }
    {   /* the matrix: FRES is FILTER's RES on ANALOG TRIO NOISE (live now, not faded); CUT stays the engine's own filter's */
        double a, b;
        uint32_t k, ok3 = 1;
        sf(54, 40, 0, 1);
        render(eng_named("ANALOG"), set_f, 100);
        a = band_db(SPEC_FROM, 1400.0, 2600.0);
        render(eng_named("ANALOG"), r_fres, 100);
        b = band_db(SPEC_FROM, 1400.0, 2600.0);
        expect("ANALOG: matrix VEL -> FRES +63 raises FILTER's resonance (the band round its 1.8 kHz cutoff, up 2 dB or more)", b > a + 2.0);
        ui_power_on();
        for (k = 0; k < NELEM(SER_ENGS); k++) {
            set_engine_of(&trk[0], eng_named(SER_ENGS[k]));
            ok3 &= !mod_dst_why(&trk[0], MD_RES);
        }
        trk[0].eng_req = 10;                                 /* (a retired engine: set_engine_of would give ANALOG) */
        expect("the FRES card is live on ANALOG TRIO NOISE (it was faded ENGINE); still faded on an engine with no multimode filter (a retired one)",
               ok3 && mod_dst_why(&trk[0], MD_RES));
        set_engine_of(&trk[0], eng_named("ANALOG"));
        {
            char ln[32];
            const char *more;
            mod_dst_line(&trk[0], MD_RES, ln, &more);
            expect("  .. its DEST line says MULTI FILTER RESONANCE (not ANALOG's own RES, an EDIT slot: no DST name is an engine knob label)", str_eq(ln, "DEST: MULTI FILTER RESONANCE") && !more);
        }
    }
    {   /* CC74 / CC71 on a ufilt engine set the new ids over their ranges; on ANALOG its own */
        uint32_t ok2;
        ui_power_on();
        set_engine_of(&trk[0], eng_named("PHASE"));
        midi_cc_map(&trk[0], 74, 100);
        midi_cc_map(&trk[0], 71, 90);
        ok2 = trk[0].p[P_FREEA] == (100 * UF_STEPS + 63) / 127 && trk[0].p[P_FREEB] == 90;
        midi_cc_map(&trk[0], 74, 127);
        ok2 &= trk[0].p[P_FREEA] == UF_STEPS;
        midi_cc_map(&trk[0], 74, 0);
        ok2 &= trk[0].p[P_FREEA] == 0;
        set_engine_of(&trk[0], 0u);
        trk[0].p[P_FREEA] = UF_STEPS;
        midi_cc_map(&trk[0], 74, 20);
        midi_cc_map(&trk[0], 71, 33);
        ok2 &= trk[0].p[P_E4] == 20 && trk[0].p[P_FREEA] == UF_STEPS && trk[0].p[P_FREEB] != 33 && trk[0].p[P_E5] == 33;
        expect("CC74 on PHASE: 0..127 over the whole CUT dial (0 30 Hz, 100 step 65, 127 the open end; 64 is no longer OFF), CC71 RES; on ANALOG CC74 is ANALOG's own CUT", ok2);
    }
}

/* ------------------------------------------------------------------ 8 STABLE --- */
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
    static const int32_t C[5] = {5, 20, 41, 62, 77};
    static const char *const E[4] = {"PHASE", "WHEEL", "FM6", "PHYS"};
    uint32_t i, k, m, s, ok = 1, nb;
    int32_t worst = 0, ratio_worst = 0;
    for (k = 0; k < 4u; k++) {
        int32_t ref;
        render(eng_named(E[k]), 0, 127);
        nb = rec_n / CTL;
        ref = part_peak(nb / 4u, nb);
        for (s = 1; s <= 2u; s++)
            for (m = 0; m <= 127u; m += 63u)
                for (i = 0; i < 5u; i++) {
                    int32_t a, b, r;
                    sf(C[i], 127, (int32_t)(m > 127u ? 127u : m == 126u ? 127u : m), (int32_t)s);
                    render(eng_named(E[k]), set_f, 127);
                    a = part_peak(nb / 4u, nb / 2u);
                    b = part_peak(nb / 2u, nb);
                    r = ref ? (a * 10) / ref : 0;
                    worst = a > worst ? a : worst;
                    ratio_worst = r > ratio_worst ? r : ratio_worst;
                    ok &= b <= a + a / 4 + 64 && a < (1 << 24) && peak_all(0) < 32768;
                }
        printf("uf:   %-6s RES 127, CUT 5 .. 77, LP / BP / HP, 12 / 24 dB: the part's peak up to %d x10 of the unfiltered %d (the master's output below %d)\n",
               E[k], ratio_worst, ref, peak_all(0));
    }
    expect("RES 127 at every CUT, MODE and SLOPE (loud chords, 1.5 s): bounded (no runaway: the second half never above the first by a quarter)", ok);
}

/* ------------------------------------------------------------------ 9 MIGRATE --- */
static void rec_fill(up_rec_t *r, uint32_t ver, uint32_t np, uint32_t eng)   /* a record of ver with every value the store's default */
{
    uint32_t k;
    memset(r, 0, sizeof *r);
    r->used = UP_USED;
    r->ver = (uint8_t)ver;
    r->engine = (uint8_t)eng;
    r->np = (uint8_t)np;
    for (k = 0; k < np; k++)
        up_set_value(r, k, k < np - 8u ? (int16_t)up_def(eng, k) : ENGINES[eng]->edit[k - (np - 8u)].def);
}
static int ufv(const int16_t *v, int32_t cut, int32_t mode, int32_t slope)
{
    return v[P_FREEA] == cut && v[P_FREEC] == mode && v[P_FREED] == slope;
}
static void t_migrate(void)
{
    int16_t v[P_COUNT];
    up_rec_t r;
    uint32_t i, ph = eng_named("PHASE");
    /* DUELL 0.2.14: nothing converts 0.2.2's bipolar CUT or the SLICER's values (ids 45..48 of Felucca's layout) any more: those stores are no longer
     * read. The patch store still keeps 0 as its default for CUT and SLOPE (the format leaves out what equals it) */
    expect("the patch store keeps 0 as its default for CUT and SLOPE (a record of today always stores SLOPE, 1 or 2)",
           up_def(ph, P_FREEA) == 0 && up_def(ph, P_FREED) == 0 && up_def(ph, P_FREEB) == 0 && up_def(ph, P_FREEC) == 0);
    rec_fill(&r, UP_VER, P_COUNT, ph);                    /* a record that holds no SLOPE (0): clamped, never converted */
    up_set_value(&r, P_FREEA, -28); up_set_value(&r, P_FREEB, 40);
    up_values(&r, v);
    expect("a record with SLOPE 0 and a CUT of -28 (0.2.2's): values clamped to their ranges (CUT 0, SLOPE 12 dB), RES kept, no conversion",
           v[P_FREEA] >= 0 && v[P_FREEA] <= UF_STEPS && v[P_FREED] == 1 && v[P_FREEB] == 40 && v[P_FREEA] == 0);
    rec_fill(&r, UP_VER, P_COUNT, ph);                    /* a record of today: SLOPE stored */
    up_set_value(&r, P_FREEA, 30); up_set_value(&r, P_FREEC, 90); up_set_value(&r, P_FREED, 2);
    up_values(&r, v);
    expect("a record of today (SLOPE 24 dB stored): CUT 30, MODE 90 kept as they are", ufv(v, 30, 90, 2));
    up_set_value(&r, P_FREEA, 0); up_set_value(&r, P_FREEC, 0); up_set_value(&r, P_FREED, 1);
    up_values(&r, v);
    expect("  .. CUT 0 (the closed end, the pair left out as the store's default) with SLOPE 12 dB stored: CUT 0", ufv(v, 0, 0, 1));
    {   /* ANALOG TRIO NOISE (ufilt 2): a record of today keeps its CUT, RES, MODE, SLOPE */
        uint32_t k, ok3 = 1;
        for (k = 0; k < NELEM(SER_ENGS); k++) {
            uint32_t se = eng_named(SER_ENGS[k]);
            rec_fill(&r, UP_VER, P_COUNT, se);                                       /* today's: SLOPE stored, kept as it is */
            up_set_value(&r, P_FREEA, 30); up_set_value(&r, P_FREEB, 7); up_set_value(&r, P_FREEC, 90); up_set_value(&r, P_FREED, 2);
            up_values(&r, v);
            ok3 &= ufv(v, 30, 90, 2) && v[P_FREEB] == 7;
        }
        expect("ANALOG TRIO NOISE: a record of today (SLOPE stored) keeps its CUT, RES, MODE, SLOPE on these engines", ok3);
    }
    {   /* the record as the store writes it: SLOPE always there (and CUT unless 0); read back the same */
        ui_power_on();
        set_engine_of(TSEL, ph);
        TSEL->p[P_FREEA] = UF_STEPS; TSEL->p[P_FREEC] = 0; TSEL->p[P_FREED] = 1;
        rec_fill(&r, UP_VER, P_COUNT, ph);
        for (i = 0; i < P_COUNT; i++)
            up_set_value(&r, i, TSEL->p[i]);
        {
            uint32_t len = pt_encode(5u, 1u, &r, 0), n = 0, at, has48 = 0, has45 = 0;
            const uint8_t *o = pt_enc;
            if (!(o[4] & PTF_BM)) {
                n = o[19];
                at = PT_FIX;
                for (i = 0; i < n; i++) {
                    has45 |= o[at + 2u * i] == P_FREEA && o[at + 2u * i + 1u] == UF_STEPS + 64;
                    has48 |= o[at + 2u * i] == P_FREED && o[at + 2u * i + 1u] == 1 + 64;
                }
            } else {
                has45 = o[PT_FIX + P_FREEA / 8u] >> (P_FREEA % 8u) & 1u;
                has48 = o[PT_FIX + P_FREED / 8u] >> (P_FREED % 8u) & 1u;
            }
            expect("a patch of today (the bypass) is stored with CUT 82 and SLOPE 12 dB (4 bytes more than before: the mark of today's CUT)", len > PT_FIX && has45 && has48);
        }
    }
    {   /* projects: 0.2.14 converts no 0.2.2 CUT (SLOPE 0) any more; a project of today restores as it is, the CUT automation too */
        static project_t q;
        ui_power_on();
        set_engine_of(&trk[0], ph);
        set_engine_of(&trk[1], eng_named("WAVE"));
        trk[0].p[P_FREEA] = 54; trk[0].p[P_FREEC] = 0; trk[0].p[P_FREED] = 1;
        trk[1].p[P_FREEA] = 30; trk[1].p[P_FREEC] = UF_HP; trk[1].p[P_FREED] = 2;
        project_capture(&q);
        memset(&q.motion, 0, sizeof q.motion);
        q.motion.count = 2;
        q.motion.event[0] = (motion_event_t){0u << 6 | 3u, P_FREEA, 42};
        q.motion.event[1] = (motion_event_t){1u << 6 | 3u, P_FREEA, 45};
        q.sum = proj_sum(&q);
        expect("a project of today restores (project_restore_runtime)", !project_restore_runtime(&q));
        expect("  .. part A (CUT 54, LP, 12 dB) and part B (CUT 30, HP, 24 dB) and their CUT automation come back as they were",
               ufv(trk[0].p, 54, 0, 1) && ufv(trk[1].p, 30, UF_HP, 2) && motion.count == 2u && motion.event[0].value == 42 &&
               motion.event[1].value == 45);
        project_capture(&q);
        project_restore_runtime(&q);
        expect("  .. captured and restored again: unchanged", ufv(trk[0].p, 54, 0, 1) && ufv(trk[1].p, 30, UF_HP, 2) &&
               motion.event[0].value == 42);
    }
}

/* ------------------------------------------------------------------ 10 ROLL --- */
/* DUELL 0.2.9 (owner, 2026-10-14): the whole filter rolls (CUT RES MODE SLOPE) in SMART and WILD on every engine, bypassed or not; every result audible */
static int uf_audible(const int8_t *end, const int8_t *o)   /* the same rule as morph.c morph_uf_safe, written out from SPEC 2.5 */
{
    int32_t m = end[P_FREEC], c = end[P_FREEA], oc = o[P_FREEA], lo = 0, hi = UF_STEPS;
    int32_t bw = m <= 64 ? m : (127 - m) * 64 / 63, lw = m < 64 ? 64 - m : 0, hw = m > 64 ? (m - 64) * 64 / 63 : 0;
    int32_t near = abs(m - o[P_FREEC]) <= 8;
    int ok = end[P_FREEB] <= (o[P_FREEB] > 60 ? o[P_FREEB] : 60) && c >= 0 && c <= UF_STEPS && m >= 0 && m <= UF_HP && (end[P_FREED] == 1 || end[P_FREED] == 2);
    if (bw >= 16) {
        lo = lw >= 16 ? 45 : hw >= 16 ? 24 : 36;
        hi = hw >= 16 ? 30 : 50;
        ok = ok && end[P_FREED] == 1 && end[P_FREEB] <= (o[P_FREEB] > 40 ? o[P_FREEB] : 40);
    } else if (lw >= 16) {
        lo = 45;
    } else if (hw >= 16) {
        hi = 25;
    }
    if (near && oc < lo)
        lo = oc;
    if (near && oc > hi)
        hi = oc;
    return ok && c >= lo && c <= hi;
}
static void t_roll(void)
{
    static int8_t o[P_COUNT], end[P_COUNT];
    uint32_t seed, i, k, m, on = 0, cut_moved = 0, mode_moved = 0, slope_moved = 0, audible = 1, other_same = 1, ana_on = 0, hp = 0, lp = 0;
    uint8_t sv = mrc;
    static const char *const ALL[] = {"PHASE", "ANALOG", "TRIO", "NOISE", "WAVE"};
    for (m = 0; m < 2u; m++) {                           /* SMART, WILD */
        mrc = m ? MC_WILD : 0;
        for (k = 0; k < NELEM(ALL); k++) {
            const engine_t *e = ENGINES[eng_named(ALL[k])];
            static const struct { int8_t cut, res, mode, slope; } OR[] = {{82, 0, 0, 1}, {52, 60, 0, 1}, {30, 20, UF_HP, 2}, {60, 0, 64, 1}, {5, 0, 0, 1}};
            for (i = 0; i < NELEM(OR); i++) {
                for (seed = 1; seed <= 200u; seed++) {
                    uint32_t j;
                    for (j = 0; j < P_COUNT; j++)
                        o[j] = (int8_t)TP[j].def;
                    o[P_FREEA] = OR[i].cut; o[P_FREEB] = OR[i].res; o[P_FREEC] = OR[i].mode; o[P_FREED] = OR[i].slope;
                    morph_roll_pure(o, end, e, m ? 100u : 40u + 30u * (seed & 1u), seed, 0, 1);
                    audible &= uf_audible(end, o);
                    if (!i) {
                        on += end[P_FREEA] != UF_STEPS || end[P_FREEC] != 0;
                        ana_on += k == 1u && (end[P_FREEA] != UF_STEPS || end[P_FREEC] != 0);
                    }
                    cut_moved += end[P_FREEA] != o[P_FREEA];
                    mode_moved += end[P_FREEC] != o[P_FREEC];
                    slope_moved += end[P_FREED] != o[P_FREED];
                    hp += end[P_FREEC] > 64;
                    lp += end[P_FREEC] < 64;
                }
            }
        }
    }
    {   /* the stream: an engine's other values do not depend on the filter's draws (a saved seed: the same End B elsewhere) */
        const engine_t *e = ENGINES[eng_named("PHASE")];
        static int8_t e1[P_COUNT];
        for (i = 0; i < P_COUNT; i++)
            o[i] = (int8_t)TP[i].def;
        mrc = 0;
        morph_roll_pure(o, e1, e, 50, 77u, 0, 1);
        o[P_FREEA] = 40; o[P_FREEB] = 30;
        morph_roll_pure(o, end, e, 50, 77u, 0, 1);
        for (i = 0; i < P_COUNT; i++)
            if (i < P_FREEA || i > P_FREED)
                other_same &= (i == P_ED_FLT || i == P_LD_FLT || i == P_E2FLT || i == P_L2FLT) ? 1 : e1[i] == end[i];
    }
    mrc = sv;
    expect("ROLL SMART and WILD: a bypassed filter (MODE LP, CUT 82) is switched on by a roll in most rolls, on PHASE and on ANALOG TRIO NOISE too", on > 700u && ana_on > 200u);
    expect("  .. CUT, RES and MODE move (LP through BP to HP, both sides rolled) and SLOPE flips: MODE moved, HP and LP ends both seen, SLOPE flipped",
           cut_moved > 3000u && mode_moved > 1000u && hp > 300u && lp > 300u && slope_moved > 100u);
    expect("  .. every result audible (25000 rolls from 5 origins, 5 engines): a share of a quarter or more: band-pass CUT 36..50 + 12 dB + RES <= 40, low-pass CUT >= 45, with a high-pass share too CUT 24..30, high-pass alone CUT <= 25, RES <= 60", audible);
    expect("  .. the filter's draws are on their own stream: the other values of a seed are the same whatever the filter holds", other_same);
}

/* ------------------------------------------------------------------ 11 COST --- */
static double cost_run(uint32_t e, int32_t cut, int32_t res, int32_t mode, int32_t slope)
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
    t->p[P_FREEC] = (int16_t)mode;
    t->p[P_FREED] = (int16_t)slope;
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
    static const char *const E[4] = {"PHASE", "WHEEL", "FM6", "ANALOG"};   /* (ANALOG: ufilt 2, its bypass is the engine as before) */
    uint32_t k, ok = 1;
    for (k = 0; k < 4u; k++) {
        uint32_t e = eng_named(E[k]);
        double off = cost_run(e, UF_STEPS, 0, 0, 1), lp = cost_run(e, 52, 20, 0, 1), bl = cost_run(e, 52, 20, 40, 1);
        double l24 = cost_run(e, 52, 20, 0, 2), h24 = cost_run(e, 30, 20, UF_HP, 2);
        printf("uf:   %-6s 8 voices, instructions per sample: bypass %.0f, 12 dB LP %.0f, 12 dB LP>BP %.0f, 24 dB LP %.0f, 24 dB HP %.0f"
               "  (per voice and sample: 12 dB +%.1f, 24 dB +%.1f)\n", E[k], off, lp, bl, l24, h24, (lp - off) / 8.0, (l24 - off) / 8.0);
        ok &= !off || ((lp - off) / 8.0 < 70.0 && (l24 - off) / 8.0 < 110.0);
    }
    expect("cost: 12 dB under 70, 24 dB under 110 host instructions per voice and sample at 8 voices (bypass: the voice code as before)", ok);
}


int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    shared_init();
    t_who_params();
    t_page();
    t_bypass();
    t_bypass_series();
    t_response();
    t_bass();
    t_exact();
    t_sweeps();
    t_routes();
    t_stable();
    t_migrate();
    t_roll();
    t_cost();
    printf(bad ? "uf: %d of %d checks FAILED\n" : "uf: all %d checks passed\n", bad ? bad : ncheck, ncheck);
    return bad ? 1 : 0;
}
