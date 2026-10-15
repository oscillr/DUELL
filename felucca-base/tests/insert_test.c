/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the per-track INSERT (1.5, Discussions #78 and #177; src/fx.c track_insert), on the real sources:
 *   build/host/insert_test [DEMO_DIR]      (run_tests.sh builds and runs it; demos in build/insert_demo/)
 * 1. bit for bit: TYPE OFF with any A B C MIX, and MIX 0 of every type, render the phrases the defaults render (POLY,
 *    SPREAD, the sends' tails); MIX 100 % of every type changes them.
 * 2. each TYPE on a chord and on 8 loud voices, A B C at their corners: no DC (its mean against the input's), bounded,
 *    MIX 0 = the input bit for bit (nothing runs), MIX 100 % = the wet: MIX 50 % is exactly halfway between the dry
 *    and it (the effect's state does not depend on MIX).
 * 3. MIX glides (2.9 ms both ways), a TYPE change fades out, switches, fades in; MIX and TYPE as automation and as a
 *    step's lock on a playing pattern.
 * 4. stored: a project round trip (FUN10, 111 parameters with 1.5's TYPE and ESYNC; 16 spare bytes left), a FUN10 of 104 (1.2 .. 1.4) loads the
 *    INSERT OFF; user presets of 111 and 104; the labels and values the pages show per TYPE; the page order.
 * 5. the cost per type and track (instructions / sample, host) against an 8-voice ANALOG part.
 * 6. demos (DEMO_DIR): each TYPE on a loop (acid line + drums), two bars dry, two bars with the INSERT. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif

static int bad;
static void ck(const char *what, int ok)
{
    printf("insert: %-82s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static int32_t out_buf[2 * CTL];
static uint64_t hash;
static void mixb(uint32_t n)
{
    uint32_t i, k;
    while (n--) {
        mix_block(out_buf, CTL);
        for (i = 0; i < 2u * CTL; i++)
            for (k = 0; k < 4u; k++) {
                hash ^= ((uint32_t)out_buf[i] >> (8u * k)) & 0xFFu;
                hash *= 0x100000001B3ull;
            }
    }
}

static uint32_t eng_by_name(const char *n)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (str_eq(ENGINES[e]->name, n))
            return e;
    return 0;
}
static uint32_t preset_by_name(uint32_t e, const char *n)
{
    uint32_t k;
    for (k = 0; k < ENGINES[e]->npresets; k++)
        if (str_eq(ENGINES[e]->presets[k].name, n))
            return k;
    return 0;
}

static void fresh(uint32_t e, uint32_t pi)        /* the boot state (no FX tails), track 1 = engine e preset pi */
{
    uint32_t k;
    memset(dly_buf, 0, sizeof dly_buf);
    memset(cho_buf, 0, sizeof cho_buf);
    memset(rev_comb, 0, sizeof rev_comb);
    memset(rev_ap, 0, sizeof rev_ap);
    memset(&fx, 0, sizeof fx);
    memset(ins, 0, sizeof ins);
    lim_env = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    vage = 0;
    rng_state = 0x1234567u;
    mod_seed = 0x2545F491u;
    song.playing = 0;
    memset(trk, 0, sizeof trk);
    memset(&mod, 0, sizeof mod);
    memset(&motion, 0, sizeof motion);
    host_tracks_init();
    for (k = 0; k < NPART; k++)
        host_preset(&trk[k], k ? 0u : e, k ? 0u : pi);
    song.sel = 0;
    hash = 0xCBF29CE484222325ull;
}

static void set_ins(track_t *t, int32_t ty, int32_t a, int32_t b, int32_t c, int32_t mix)
{
    t->p[P_ITYPE] = (int16_t)ty;
    t->p[P_IA] = (int16_t)a;
    t->p[P_IB] = (int16_t)b;
    t->p[P_IC] = (int16_t)c;
    t->p[P_IMIX] = (int16_t)mix;
}

/* ------------------------------------------------------------------ 1 --- */
static int32_t ph_ty = -1, ph_a, ph_b, ph_c, ph_mix, ph_sprd;
/* the hash of a phrase on track 1 (ANALOG SOFT PAD, POLY, the preset's sends) in a fork()ed child: the INSERT as ph_*
 * says (ph_ty -1: the defaults), SPREAD ph_sprd */
static uint64_t phrase(void)
{
    int fd[2];
    uint64_t h = 0;
    pid_t pid;
    if (pipe(fd))
        return 0;
    fflush(stdout);
    if (!(pid = fork())) {
        track_t *t = &trk[0];
        fresh(0, preset_by_name(0, "SOFT PAD"));
        t->p[P_VOICE] = V_POLY;
        t->p[P_SPRD] = (int16_t)ph_sprd;
        if (ph_ty >= 0)
            set_ins(t, ph_ty, ph_a, ph_b, ph_c, ph_mix);
        trk_note_on(t, 48, 100);
        trk_note_on(t, 55, 70);
        trk_note_on(t, 64, 120);
        mixb(FS / 2u / CTL);
        trk_note_on(t, 72, 90);
        mixb(FS / 4u / CTL);
        trk_note_off(t, 48);
        trk_note_off(t, 55);
        trk_note_off(t, 64);
        trk_note_off(t, 72);
        mixb(FS / CTL);
        h = hash;
        if (write(fd[1], &h, sizeof h) != sizeof h)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], &h, sizeof h) != sizeof h)
        h = 0;
    close(fd[0]);
    waitpid(pid, 0, 0);
    return h;
}
static void test_identity(void)
{
    uint64_t h0, h;
    int32_t ty, sp;
    int ok0 = 1, okw = 1;
    for (sp = 0; sp <= 64; sp += 64) {
        ph_sprd = sp;
        ph_ty = -1;
        h0 = phrase();
        ph_ty = IT_OFF; ph_a = 1; ph_b = 2; ph_c = 3; ph_mix = 50;
        ck(sp ? "SPREAD 64: TYPE OFF with A B C MIX set renders the defaults' phrase bit for bit"
              : "TYPE OFF with A B C MIX set renders the defaults' phrase bit for bit", phrase() == h0);
        for (ty = 1; ty < IT_N; ty++) {
            ph_ty = ty; ph_a = 127; ph_b = 127; ph_c = 127; ph_mix = 0;
            ok0 &= phrase() == h0;
            ph_mix = 127;
            h = phrase();
            okw &= h != h0 && h;
        }
    }
    ck("MIX 0 of every TYPE (POLY, SPREAD 64): bit for bit the defaults (nothing runs)", ok0);
    ck("MIX 100 % of every TYPE: the phrase changes", okw);
    ph_sprd = 0;
    {   /* review 1.5: TYPE OFF while the part is silent (its tail run out) leaves no INSERT state for the next note */
        track_t *t = &trk[0];
        fresh(0, preset_by_name(0, "SOFT PAD"));
        set_ins(t, IT_FLANGER, 64, 64, 64, 127);
        trk_note_on(t, 60, 100);
        mixb(FS / 4u / CTL);
        trk_note_off(t, 60);
        mixb(4u * FS / CTL);                          /* silent, the tail run out */
        int stale = ins[0].w != 0 && t->ins_run;
        t->p[P_ITYPE] = IT_OFF;
        mixb(1);
        ck("TYPE OFF while silent: the INSERT at rest at once (no stale fade on the next note)",
           stale && !ins[0].w && !t->ins_run);
    }
}

/* ------------------------------------------------------------------ 2 --- */
#define NS (2u * FS / CTL * CTL)                      /* two seconds, whole blocks: the last FS samples are 1 s */
static int32_t sig_in[NS], sig_a[NS], sig_b[NS];
static void make_sig(uint32_t voices)             /* saws, 24000 each (a voice's level), a chord's pitches */
{
    static const double HZ[8] = {110.0, 139.0, 165.0, 220.0, 277.0, 330.0, 440.0, 554.0};
    uint32_t i, v;
    for (i = 0; i < NS; i++) {
        double s = 0;
        for (v = 0; v < voices; v++) {
            double ph = fmod(i * HZ[v] / FS + v * 0.37, 1.0);
            s += (ph * 2.0 - 1.0) * 24000.0;
        }
        sig_in[i] = (int32_t)s;
    }
}
/* the input through track 1's INSERT from rest into out */
static void run_ins(int32_t ty, int32_t a, int32_t b, int32_t c, int32_t mix, int32_t *out)
{
    track_t *t = &trk[0];
    uint32_t i;
    memset(ins, 0, sizeof ins);
    set_ins(t, ty, a, b, c, mix);
    memcpy(out, sig_in, sizeof sig_in);
    for (i = 0; i < NS; i += CTL)
        track_insert(t, out + i, CTL);
}
static void test_types(void)
{
    static const int16_t COR[3] = {0, 64, 127};
    uint32_t nv, ty, ia, ib, ic, i;
    printf("insert: the types on 3 and 8 saws (peak in 72000 / 192000), A B C at 0 64 127 (27 settings), MIX 100 %%:\n");
    for (ty = 1; ty < IT_N; ty++) {
        int32_t pk[2] = {0, 0};
        double dc[2] = {0, 0};
        int same0 = 1, lin = 1, diff = 0;
        for (nv = 3; nv <= 8; nv += 5) {
            uint32_t s = nv == 8;
            make_sig(nv);
            for (ia = 0; ia < 3u; ia++)
                for (ib = 0; ib < 3u; ib++)
                    for (ic = 0; ic < 3u; ic++) {
                        double sy = 0, sx = 0;
                        /* (CRUSH: against the input held at its RATE, as it samples it: the mean of 689 samples a
                         * second of the saws is not theirs, the held one's is the reference) */
                        uint32_t hold = ty == IT_CRUSH ? INS_HOLD[COR[ib] >> 3] : 1u;
                        run_ins((int32_t)ty, COR[ia], COR[ib], COR[ic], 127, sig_a);
                        for (i = 0; i < NS; i++) {
                            int32_t m = sig_a[i] < 0 ? -sig_a[i] : sig_a[i];
                            pk[s] = m > pk[s] ? m : pk[s];
                            diff |= sig_a[i] != sig_in[i];
                            if (i >= NS - FS) {
                                sy += sig_a[i];
                                sx += sig_in[i / hold * hold];
                            }
                        }
                        sy = fabs((sy - sx) / FS);
                        if (ty != IT_CRUSH || ia || ib)         /* (1 bit at 689 Hz: 689 coarse errors a second do */
                            dc[s] = sy > dc[s] ? sy : dc[s];    /* not average out in 1 s; the odd check below) */
                        if (ia == 1 && ic != 1) {           /* MIX 50 % halfway, MIX 0 the input */
                            run_ins((int32_t)ty, COR[ia], COR[ib], COR[ic], 64, sig_b);
                            for (i = SL_RAMP; i < NS; i++) {
                                int32_t want = sig_in[i] + mulq16(sig_a[i] - sig_in[i], 64u * 258u * 2u);
                                lin &= abs(sig_b[i] - want) <= 1;
                            }
                            run_ins((int32_t)ty, COR[ia], COR[ib], COR[ic], 0, sig_b);
                            same0 &= !memcmp(sig_b, sig_in, sizeof sig_in) && !ins[0].w;
                        }
                    }
        }
        printf("insert:   %-5s peak 3 saws %6d, 8 saws %6d; worst DC (mean against the input's) %5.1f / %5.1f\n",
               N_ITYPE[ty], pk[0], pk[1], dc[0], dc[1]);
        {
            char w[96];
            snprintf(w, sizeof w, "%s: no DC (< 0.5 %% of a voice's full scale), bounded (< 1.5 x the input, LEVEL +7.5 dB)",
                     N_ITYPE[ty]);
            ck(w, dc[0] < 164.0 && dc[1] < 164.0 && pk[0] < 120000 && pk[1] < 288000);
            snprintf(w, sizeof w, "%s: MIX 0 the input bit for bit, MIX 50 %% halfway to MIX 100 %% (the wet), it acts",
                     N_ITYPE[ty]);
            ck(w, same0 && lin && diff);
        }
    }
    {   /* CRUSH's steps are odd: -x gives minus what x gives (but at an exact half step, which rounds up), every detent of BITS */
        int ok = 1;
        int32_t v, a;
        for (a = 0; a < 128; a += 3)
            for (v = -140000; v <= 140000; v += 7) {
                uint32_t sh = ins_bits_step(a);
                int32_t half = sh > 256u && !((v * 256 + (int32_t)(sh >> 1)) % (int32_t)sh);
                int32_t half2 = sh > 256u && !((-v * 256 + (int32_t)(sh >> 1)) % (int32_t)sh);
                ok &= half || half2 || ins_shape(IT_CRUSH, -v, sh) == -ins_shape(IT_CRUSH, v, sh);
            }
        ck("CRUSH: its steps are odd (no offset) at every BITS", ok);
    }
}

/* ------------------------------------------------------------------ 3 --- */
static void test_glide(void)
{
    track_t *t = &trk[0];
    int32_t w1, w4, wm;
    uint32_t i;
    int ok;
    make_sig(3);
    fresh(0, 0);
    set_ins(t, IT_SOFT, 64, 96, 96, 127);
    memcpy(sig_a, sig_in, sizeof sig_in);
    track_insert(t, sig_a, CTL);
    w1 = ins[0].w;
    for (i = 1; i < 4u; i++)
        track_insert(t, sig_a + i * CTL, CTL);
    w4 = ins[0].w;
    ck("MIX 0 -> 100 %: the wet share glides in over 2.9 ms (128 samples)", w1 == CTL * SL_SLOPE && w4 == 32768);
    t->p[P_IMIX] = 0;
    track_insert(t, sig_a + 4u * CTL, CTL);
    wm = ins[0].w;
    for (i = 5; i < 9u; i++)
        track_insert(t, sig_a + i * CTL, CTL);
    ck("  and out again; then nothing runs (the input as it is)", wm == 32768 - CTL * SL_SLOPE && !ins[0].w &&
       !memcmp(sig_a + 8u * CTL, sig_in + 8u * CTL, CTL * sizeof(int32_t)));
    /* a TYPE change while it plays: SOFT fades out, FLANG fades in from rest */
    t->p[P_IMIX] = 127;
    for (i = 9; i < 16u; i++)
        track_insert(t, sig_a + i * CTL, CTL);
    t->p[P_ITYPE] = IT_FLANGER;
    ok = ins[0].type == IT_SOFT;
    for (i = 16; i < 20u; i++) {
        track_insert(t, sig_a + i * CTL, CTL);
        ok &= i < 19u ? ins[0].type == IT_SOFT && ins[0].w > 0 : 1;
    }
    track_insert(t, sig_a + 20u * CTL, CTL);
    ok &= ins[0].type == IT_FLANGER && ins[0].w == CTL * SL_SLOPE;
    for (i = 21; i < 24u; i++)
        track_insert(t, sig_a + i * CTL, CTL);
    {   /* no click: the largest step of the output over the change against the input's own */
        int32_t jmp = 0, jin = 0;
        for (i = 16u * CTL; i < 24u * CTL; i++) {
            if (i > 16u * CTL) {
                int32_t a = abs(sig_a[i] - sig_a[i - 1]), b = abs(sig_in[i] - sig_in[i - 1]);
                jmp = a > jmp ? a : jmp;
                jin = b > jin ? b : jin;
            }
        }
        ck("a TYPE change: the old one fades out, the new one starts from rest and fades in, no jump past the saws'",
           ok && jmp <= jin + jin / 4);
    }
}

/* MIX and TYPE on a playing pattern: MIX automation (0 at step 1, 100 % at step 9), a TYPE lock (CRUSH on step 5) */
static void test_motion(void)
{
    track_t *t = &trk[0];
    uint32_t i, k, step_s = 0;
    int ok_a = 1, ok_l = 1, seen_c = 0, seen_back = 0, seen_w = 0;
    static const uint8_t NOTE[1] = {48};
    fresh(0, preset_by_name(0, "SAW LEAD"));
    set_ins(t, IT_FOLD, 90, 96, 96, 0);
    for (i = 0; i < 16u; i++)
        put_step(t, i, 1, NOTE, ST_NOTE, 0);
    t->p[P_SLEN] = 16;
    t->seq_active = 1;
    ok_a &= !motion_set_event(t, 0, P_IMIX, 0) && !motion_set_event(t, 8, P_IMIX, 127);
    ok_l &= !motion_set_lock(t, 4, P_ITYPE, IT_CRUSH);
    seq_start();
    for (k = 0; k < 2750u; k++) {                       /* 16 steps of 172.3 blocks (1/16 at 120 BPM), not again */
        uint32_t idx;
        mixb(1);
        idx = t->seq_idx;
        if (idx < 8u)
            ok_a &= t->p[P_IMIX] == 0 && !ins[0].w;
        else if (idx < 16u && t->p[P_IMIX] == 127) {
            if (!step_s)
                step_s = k;
            if (k >= step_s + 4u)
                seen_w |= ins[0].w == 32768;
        }
        if (idx == 4u)
            seen_c |= t->p[P_ITYPE] == IT_CRUSH;
        if (idx == 5u)
            seen_back |= t->p[P_ITYPE] == IT_FOLD;
        ok_l &= idx == 4u || t->p[P_ITYPE] == IT_FOLD;
    }
    seq_stop();
    ck("MIX as automation: 0 on steps 1..8 (nothing runs), 100 % from step 9, the wet share there 4 blocks on",
       ok_a && step_s && seen_w);
    ck("TYPE as a step's lock: CRUSH on step 5 only, FOLD before and after", ok_l && seen_c && seen_back);
}

/* ------------------------------------------------------------------ 4 --- */
static void test_stored(void)
{
    static project_t a, b;
    static project_store_t st;
    up_rec_t r;
    int16_t v[P_COUNT];
    uint32_t i, k;
    int ok;
    fresh(0, 0);
    for (k = 0; k < NTRK; k++)
        set_ins(&trk[k], (int32_t)(k * 2u + 1u), (int32_t)(10 + k), (int32_t)(20 + k), (int32_t)(30 + k), (int32_t)(100 + k));
    motion_set_event(&trk[1], 3, P_IMIX, 77);
    motion_set_lock(&trk[0], 5, P_IA, 9);               /* (DUELL: FUN11 stores parts A and B) */
    project_capture(&a);
    ok = proj_pack(&st, &a) && st.raw[66] == P_COUNT && P_COUNT == 127u && proj_import(&b, &st, sizeof st) &&
         !memcmp(&a.t, &b.t, NSHOWN * sizeof a.t[0]) && !memcmp(&a.motion, &b.motion, sizeof a.motion);
    for (k = 0; k < NSHOWN; k++)
        ok &= b.t[k].p[P_ITYPE] == (int16_t)(k * 2u + 1u) && b.t[k].p[P_IMIX] == (int16_t)(100 + k) &&
              b.t[k].p[P_IC] == (int16_t)(30 + k);
    ok &= 68u + PROJ_NTRK * (P_COUNT + 2u + NSTEP * 9u) + sizeof(chain_config_t) + sizeof(motion_store_t) + 1618u == PROJ_FM6_OFF;
    ck("project: FUN11 round trip of the INSERT (127 parameters, its motion; parts A B), 1618 spare bytes", ok);
    /* user presets: a record keeps them (0.2.14: records of fewer parameters are no longer mapped by count) */
    memset(&r, 0, sizeof r);
    r.used = UP_USED; r.ver = UP_VER; r.engine = 0; r.np = P_COUNT;
    for (i = 0; i < P_COUNT; i++) up_set_value(&r, i, trk[3].p[i]);
    up_values(&r, v);
    ok = v[P_ITYPE] == 7 && v[P_IA] == 13 && v[P_IMIX] == 103;
    ck("user presets: the INSERT's values kept", ok);
    /* the pages: INSERT and INSERT 2 after FX (DUELL: the SLICER is removed), then EQ 1, the labels and values per TYPE */
    {
        uint32_t p, ps = 0, pi = 0, pi2 = 0, pd = 0;
        char val[16];
        const char *u;
        track_t *t = &trk[0];
        for (p = 0; p < NPAGES; p++) {
            if (str_eq(PAGES[p].title, "FX")) ps = p;
            if (str_eq(PAGES[p].title, "INSERT 1/2")) pi = p;
            if (str_eq(PAGES[p].title, "INSERT 2/2")) pi2 = p;
            if (str_eq(PAGES[p].title, "EQ 1/2")) pd = p;
        }
        ok = pi == ps + 1u && pi2 == pi + 1u && pd == pi2 + 1u && PAGES[pi].fam == FAM_FX && PAGES[pi].id[0] == P_ITYPE &&
             PAGES[pi2].id[0] == P_IMIX;
        set_ins(t, IT_CRUSH, 99, 96, 96, 127);
        ok &= str_eq(track_desc(t, P_IA)->label, "BITS") && track_desc(t, P_IA)->def == 99 && str_eq(track_desc(t, P_IB)->label, "RATE") &&
              str_eq(track_desc(t, P_IC)->label, "LPF");
        param_format(track_desc(t, P_IA), 99, val, &u);
        ok &= str_eq(val, "9");
        param_format(track_desc(t, P_IB), 96, val, &u);
        ok &= str_eq(val, "11k") && str_eq(u, "Hz");
        param_format(track_desc(t, P_IB), 127, val, &u);
        ok &= str_eq(val, "44k");
        t->p[P_ITYPE] = IT_HARD;
        ok &= str_eq(track_desc(t, P_IA)->label, "DRIVE") && str_eq(track_desc(t, P_IB)->label, "TONE") &&
              str_eq(track_desc(t, P_IC)->label, "LEVEL");
        t->p[P_ITYPE] = IT_PHASER;
        ok &= str_eq(track_desc(t, P_IA)->label, "RATE") && str_eq(track_desc(t, P_IB)->label, "DEPTH") &&
              str_eq(track_desc(t, P_IC)->label, "FDBK");
        t->p[P_ITYPE] = IT_OFF;
        ok &= str_eq(track_desc(t, P_IA)->label, "INS A") && str_eq(param_desc_of(0, P_IB)->label, "INS B");
        for (i = P_ITYPE; i <= P_IMIX; i++)
            ok &= motion_param(i) && !param_kept(i);
        ck("pages: FX, INSERT (TYPE A B C), INSERT 2 (MIX), EQ 1; A B C named by TYPE; recordable; part of the sound", ok);
    }
}

/* ------------------------------------------------------------------ 5 --- */
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}
/* instructions per sample of ANALOG SOFT PAD POLY, 8 voices on track 1, with its INSERT ty (and MIX 50 %, mix) */
static double cost(int32_t ty, int32_t mix)
{
    static const uint8_t NOTES[8] = {48, 52, 55, 59, 60, 64, 67, 71};
    track_t *t = &trk[0];
    uint32_t i, rep;
    double best = 1e30;
    fresh(0, preset_by_name(0, "SOFT PAD"));
    t->p[P_VOICE] = V_POLY;
    t->p[P_SUS] = 127;
    set_ins(t, ty, 100, 100, 100, mix);
    for (i = 0; i < 8u; i++)
        trk_note_on(t, NOTES[i], 100);
    mixb(FS / 4u / CTL);
    for (rep = 0; rep < 3u; rep++) {
        uint64_t i0 = instr_now();
        double ipc;
        mixb(FS / 2u / CTL);
        ipc = (double)(instr_now() - i0) / (FS / 2u);
        best = ipc < best ? ipc : best;
    }
    return best;
}
static void test_cost(void)
{
    double c0, c, worst = 0;
    int32_t ty;
    if (!instr_now()) {
        printf("insert: cost: no instruction counter on this host (proc_pid_rusage), skipped\n");
        return;
    }
    c0 = cost(IT_OFF, 127);
    printf("insert: cost, ANALOG SOFT PAD 8 voices on track 1: OFF %.0f instructions / sample; the INSERT adds:\n", c0);
    for (ty = 1; ty < IT_N; ty++) {
        c = cost(ty, 127) - c0;
        printf("insert:   %-5s %+5.1f (%+.1f %%)", N_ITYPE[ty], c, c * 100 / c0);
        worst = c > worst ? c : worst;
        c = cost(ty, 64) - c0;
        printf(", MIX 50 %% %+5.1f\n", c);
        worst = c > worst ? c : worst;
    }
    c = cost(IT_PHASER, 0) - c0;
    printf("insert:   PHASR at MIX 0 %+5.1f (a test per block)\n", c);
    ck("cost: MIX 0 next to nothing; every TYPE a fixed cost per track below 10 % of an 8-voice part",
       worst < c0 * 0.10 && c < c0 * 0.005);
}


/* ------------------------------------------------------------------ TAPE, VINYL (0.2.14) --- */
#define LN (FS * 6u)                                    /* 6 s */
static int32_t lo_in[LN], lo_out[LN];
static void lo_run(uint32_t trk_i, int32_t ty, int32_t a, int32_t b, int32_t c, int32_t mix, const int32_t *in, int32_t *out, uint32_t n)
{
    track_t *t = &trk[trk_i];
    uint32_t i;
    memset(&ins[trk_i], 0, sizeof ins[trk_i]);
    set_ins(t, ty, a, b, c, mix);
    memcpy(out, in, n * sizeof *in);
    for (i = 0; i + CTL <= n; i += CTL)
        track_insert(t, out + i, CTL);
}
static double lo_rms(const int32_t *x, uint32_t from, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = from; i < from + n; i++)
        s += (double)x[i] * x[i];
    return sqrt(s / n);
}
static double lo_mean(const int32_t *x, uint32_t from, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = from; i < from + n; i++)
        s += x[i];
    return s / n;
}
static int32_t lo_peak(const int32_t *x, uint32_t from, uint32_t n)
{
    int32_t m = 0;
    uint32_t i;
    for (i = from; i < from + n; i++)
        m = abs(x[i]) > m ? abs(x[i]) : m;
    return m;
}
static void lo_sine(double hz, double amp)
{
    uint32_t i;
    for (i = 0; i < LN; i++)
        lo_in[i] = (int32_t)(amp * sin(2 * M_PI * hz * i / FS));
}
/* the pitch of a sine as it leaves: the period between rising zero crossings (linearly interpolated), the largest spread in cents
 * over the stretch (from 0.5 s on: past the fade and the filters) */
static double lo_cents_spread(const int32_t *x, double hz)
{
    double last = -1, lo = 1e9, hi = -1e9;
    uint32_t i, ncr = 0;
    for (i = FS / 2u; i + 1u < LN - CTL; i++)       /* (a last partial block is not run) */
        if (x[i] < 0 && x[i + 1] >= 0) {
            double pos = i + (double)(-x[i]) / (x[i + 1] - x[i]);
            if (last >= 0) {
                double per = pos - last, c = 1200.0 * log2((FS / per) / hz);
                lo = c < lo ? c : lo;
                hi = c > hi ? c : hi;
                ncr++;
            }
            last = pos;
        }
    return ncr > 10u ? hi - lo : -1;
}
/* where the pitch moves: the 1 kHz sine's period series (one a cycle, ~1000 a second, as cents), its DFT power in 0.05 Hz steps from f0 to f1, the
 * frequency of the largest peak and the share of the power below 2.5 Hz (*low) */
static double lo_pitch_peak(const int32_t *x, double hz, double f0, double f1, double *low)
{
    static double c[8192], t[8192];
    double last = -1, mean = 0, best = 0, bf = 0, tot = 0, lw = 0, f;
    uint32_t i, nc = 0;
    for (i = FS / 2u; i + 1u < LN - CTL && nc < 8192u; i++)
        if (x[i] < 0 && x[i + 1] >= 0) {
            double pos = i + (double)(-x[i]) / (x[i + 1] - x[i]);
            if (last >= 0) {
                c[nc] = 1200.0 * log2((FS / (pos - last)) / hz);
                t[nc++] = pos / FS;
            }
            last = pos;
        }
    for (i = 0; i < nc; i++)
        mean += c[i] / nc;
    for (f = 0.2; f < 12.0; f += 0.05) {
        double re = 0, im = 0, pw;
        for (i = 0; i < nc; i++) {
            re += (c[i] - mean) * cos(2 * M_PI * f * t[i]);
            im += (c[i] - mean) * sin(2 * M_PI * f * t[i]);
        }
        pw = re * re + im * im;
        tot += pw;
        lw += f < 2.5 ? pw : 0;
        if (f >= f0 && f <= f1 && pw > best) {
            best = pw;
            bf = f;
        }
    }
    *low = tot > 0 ? lw / tot : 0;
    return bf;
}
static void test_lofi(void)
{
    track_t *t = &trk[0];
    uint32_t i, j;
    int ok;
    double r, r2;
    printf("insert: TAPE, VINYL (0.2.14): ins_t %u bytes, as before (one type runs at a time: the PHASR words are the same words)\n", (unsigned)sizeof(ins_t));
    ck("TAPE, VINYL: ins_t stays 580 bytes (pool budget unchanged)", sizeof(ins_t) == 580u);
    fresh(0, 0);
    /* labels, ranges, names that fit the 6-character value display */
    {
        char val[16];
        const char *u;
        ok = IT_N == 11 && IT_TAPE == 9 && IT_VINYL == 10 && str_eq(N_ITYPE[IT_TAPE], "TAPE") && str_eq(N_ITYPE[IT_VINYL], "VINYL") &&
             param_desc_of(0, P_ITYPE)->max == IT_N - 1;
        t->p[P_ITYPE] = IT_TAPE;
        ok &= str_eq(track_desc(t, P_IA)->label, "WOW") && str_eq(track_desc(t, P_IB)->label, "SAT") && str_eq(track_desc(t, P_IC)->label, "TONE");
        param_format(track_desc(t, P_IC), 110, val, &u);
        ok &= str_eq(u, "kHz") && strlen(val) <= 6;
        t->p[P_ITYPE] = IT_VINYL;
        ok &= str_eq(track_desc(t, P_IA)->label, "DUST") && str_eq(track_desc(t, P_IB)->label, "NOISE") && str_eq(track_desc(t, P_IC)->label, "WARP");
        for (i = 0; i < 3u; i++)
            ok &= track_desc(t, P_IA + i)->max == 127 && track_desc(t, P_IA + i)->min == 0 && str_eq(track_desc(t, P_IA + i)->names ? "x" : "x", "x");
        ck("TAPE: WOW SAT TONE (Hz), VINYL: DUST NOISE WARP, 0..127, names fit 6 characters", ok);
    }
    /* determinism: the same input from rest twice gives the same output; the two parts differ (their own seeds); MIX does not move the noise */
    lo_sine(220, 8000);
    for (i = 1; i < IT_N; i++) {
        if (i != IT_TAPE && i != IT_VINYL)
            continue;
        lo_run(0, (int32_t)i, 80, 80, 80, 127, lo_in, lo_out, LN);
        memcpy(sig_a, lo_out, 4u * FS);
        lo_run(0, (int32_t)i, 80, 80, 80, 127, lo_in, lo_out, LN);
        ok = !memcmp(sig_a, lo_out, 4u * FS);
        lo_run(1, (int32_t)i, 80, 80, 80, 127, lo_in, lo_out, LN);
        ok &= memcmp(sig_a, lo_out, 4u * FS) != 0;
        {
            char w[96];
            snprintf(w, sizeof w, "%s: deterministic (same run twice bit for bit), each part its own noise", N_ITYPE[i]);
            ck(w, ok);
        }
    }
    /* silence in */
    memset(lo_in, 0, sizeof lo_in);
    lo_run(0, IT_TAPE, 47, 71, 110, 127, lo_in, lo_out, LN);
    r = lo_rms(lo_out, FS, 4u * FS);
    printf("insert:   TAPE silence in: hiss rms %.1f (%.1f dBFS of a voice's 32768), peak %d, mean %.2f\n", r, 20 * log10(r / 32768.0), lo_peak(lo_out, FS, 4u * FS), lo_mean(lo_out, FS, 4u * FS));
    ck("TAPE: silence in = a faint hiss only (rms 5 .. 60, peak < 200, mean < 3 = the DC blocker's own floor)", r > 5 && r < 60 && lo_peak(lo_out, FS, 4u * FS) < 200 && fabs(lo_mean(lo_out, FS, 4u * FS)) < 3.0);
    lo_run(0, IT_VINYL, 0, 0, 127, 127, lo_in, lo_out, LN);
    ck("VINYL: DUST 0, NOISE 0 (any WARP), silence in = exact digital silence", lo_peak(lo_out, 0, LN) == 0);
    lo_run(0, IT_VINYL, 0, 127, 0, 127, lo_in, lo_out, LN);
    r = lo_rms(lo_out, FS, 4u * FS);
    printf("insert:   VINYL silence in, NOISE 127 DUST 0: rms %.1f (%.1f dBFS), peak %d\n", r, 20 * log10(r / 32768.0), lo_peak(lo_out, FS, 4u * FS));
    ck("VINYL: NOISE 127 alone is quiet (rms < 450 = -37 dBFS), no mean", r > 50 && r < 450 && fabs(lo_mean(lo_out, FS, 4u * FS)) < 3.0);
    {   /* the clicks: count and size */
        static const int32_t DA[3] = {1, 64, 127};
        double cnt[3];
        int32_t pk[3];
        for (j = 0; j < 3u; j++) {
            uint32_t k, c = 0;
            int in_c = 0;
            lo_run(0, IT_VINYL, DA[j], 0, 0, 127, lo_in, lo_out, LN);
            for (k = FS; k < LN; k++) {                     /* a click: a run above 60 after quiet */
                int hit = abs(lo_out[k]) > 60;
                c += hit && !in_c;
                in_c = hit || (in_c && abs(lo_out[k]) > 20);
            }
            cnt[j] = c / 5.0;
            pk[j] = lo_peak(lo_out, FS, 5u * FS);
            ok = j == 0 ? 1 : ok;
        }
        printf("insert:   VINYL DUST 1 / 64 / 127: %.1f / %.1f / %.1f clicks a second, peak %d / %d / %d, mean %.2f\n", cnt[0], cnt[1], cnt[2], pk[0], pk[1], pk[2], lo_mean(lo_out, FS, 5u * FS));
        ck("VINYL: DUST sets the density (sparse at 1, more at 64, most at 127; ~1, ~16, ~248 a second), clicks bounded (< 4500)",
           cnt[0] < cnt[1] && cnt[1] < cnt[2] && cnt[1] > 8 && cnt[1] < 32 && cnt[2] > 120 && cnt[2] < 400 && pk[0] < 4500 && pk[1] < 4500 && pk[2] < 4500 &&
           fabs(lo_mean(lo_out, FS, 5u * FS)) < 2.0);
    }
    /* level: a 1 kHz sine of a voice's size */
    lo_sine(1000, 12000);
    lo_run(0, IT_TAPE, 0, 0, 127, 127, lo_in, lo_out, LN);
    r = lo_rms(lo_out, FS, FS) / lo_rms(lo_in, FS, FS);
    lo_run(0, IT_TAPE, 47, 71, 110, 127, lo_in, lo_out, LN);
    r2 = lo_rms(lo_out, FS, FS) / lo_rms(lo_in, FS, FS);
    printf("insert:   TAPE level on a 12000 sine: SAT 0 %.2f dB, defaults %.2f dB", 20 * log10(r), 20 * log10(r2));
    ok = fabs(20 * log10(r)) < 1.0 && fabs(20 * log10(r2)) < 3.0;
    lo_run(0, IT_TAPE, 0, 127, 127, 127, lo_in, lo_out, LN);
    r = lo_rms(lo_out, FS, FS) / lo_rms(lo_in, FS, FS);
    printf(", SAT 127 %.2f dB\n", 20 * log10(r));
    ck("TAPE: level kept within 1 dB at SAT 0, 3 dB at the defaults, 5 dB at SAT 127", ok && fabs(20 * log10(r)) < 5.0);
    lo_run(0, IT_VINYL, 0, 0, 0, 127, lo_in, lo_out, LN);
    r = lo_rms(lo_out, FS, FS) / lo_rms(lo_in, FS, FS);
    printf("insert:   VINYL level on a 12000 sine (1 kHz), no dust: %.2f dB\n", 20 * log10(r));
    ck("VINYL: level kept within 1 dB (the band is 110 Hz .. 6 kHz)", fabs(20 * log10(r)) < 1.0);
    {   /* the band: 8 kHz is down, 60 Hz is down */
        double g8, g60;
        lo_sine(8000, 12000);
        lo_run(0, IT_VINYL, 0, 0, 0, 127, lo_in, lo_out, LN);
        g8 = lo_rms(lo_out, FS, FS) / lo_rms(lo_in, FS, FS);
        lo_sine(60, 12000);
        lo_run(0, IT_VINYL, 0, 0, 0, 127, lo_in, lo_out, LN);
        g60 = lo_rms(lo_out, FS, FS) / lo_rms(lo_in, FS, FS);
        printf("insert:   VINYL band: 8 kHz %.1f dB, 60 Hz %.1f dB\n", 20 * log10(g8), 20 * log10(g60));
        ck("VINYL: the band is narrowed (8 kHz about -4 dB, 60 Hz about -5 dB, gently)", g8 < 0.8 && g8 > 0.4 && g60 < 0.7 && g60 > 0.3);
    }
    /* wow and flutter: the pitch of a 1 kHz sine */
    lo_sine(1000, 12000);
    {
        double w0, w64, w127, v0, v127, lw0, lw1;
        lo_run(0, IT_TAPE, 0, 0, 127, 127, lo_in, lo_out, LN);
        w0 = lo_cents_spread(lo_out, 1000);
        lo_run(0, IT_TAPE, 64, 0, 127, 127, lo_in, lo_out, LN);
        w64 = lo_cents_spread(lo_out, 1000);
        lo_run(0, IT_TAPE, 127, 0, 127, 127, lo_in, lo_out, LN);
        w127 = lo_cents_spread(lo_out, 1000);
        lo_run(0, IT_VINYL, 0, 0, 0, 127, lo_in, lo_out, LN);
        v0 = lo_cents_spread(lo_out, 1000);
        lo_run(0, IT_VINYL, 0, 0, 127, 127, lo_in, lo_out, LN);
        v127 = lo_cents_spread(lo_out, 1000);
        printf("insert:   pitch spread (cents, peak to peak) TAPE WOW 0 / 64 / 127: %.1f / %.1f / %.1f; VINYL WARP 0 / 127: %.1f / %.1f\n", w0, w64, w127, v0, v127);
        ck("TAPE: WOW 0 leaves only the flutter (a few cents), more WOW more wobble, 127 under 60 cents",
           w0 > 2.0 && w0 < 12.0 && w64 > w0 && w127 > w64 && w127 < 60.0);
        lo_run(0, IT_TAPE, 0, 0, 127, 127, lo_in, lo_out, LN);
        w0 = lo_pitch_peak(lo_out, 1000, 4.0, 11.0, &lw0);
        lo_run(0, IT_TAPE, 127, 0, 127, 127, lo_in, lo_out, LN);
        w127 = lo_pitch_peak(lo_out, 1000, 0.2, 11.0, &lw1);
        printf("insert:   pitch movement: WOW 0 peaks at %.2f Hz (the flutter), WOW 127 at %.2f Hz with %.0f %% of the power below 2.5 Hz\n", w0, w127, lw1 * 100);
        ck("TAPE: the flutter runs at 6 .. 9 Hz, the wow peaks at 0.3 .. 2 Hz and holds most of its power below 2.5 Hz",
           w0 >= 6.0 && w0 <= 9.0 && w127 >= 0.3 && w127 <= 2.0 && lw1 > 0.6);
        ck("VINYL: WARP 0 is still (< 1 cent), WARP 127 a light wow (2 .. 20 cents)", v0 >= 0 && v0 < 1.0 && v127 > 2.0 && v127 < 20.0);
    }
    /* loud: 8 loud saws, and a full-scale DC step: bounded, no NaN (integer: no overflow wrap), DC blocked */
    make_sig(8);
    for (i = 0; i < NS; i++)
        sig_in[i] = clamp(sig_in[i] * 3 / 2, -262143, 262143);
    for (j = IT_TAPE; j <= IT_VINYL; j++) {
        static const int32_t CS[4][3] = {{127, 127, 127}, {127, 127, 0}, {0, 0, 127}, {127, 0, 127}};
        int32_t pk = 0;
        uint32_t q;
        for (q = 0; q < 4u; q++) {
            lo_run(0, (int32_t)j, CS[q][0], CS[q][1], CS[q][2], 127, sig_in, sig_a, NS);
            pk = lo_peak(sig_a, 0, NS) > pk ? lo_peak(sig_a, 0, NS) : pk;
        }
        for (i = 0; i < NS; i++)
            sig_b[i] = i < NS / 2u ? 262143 : -262143;
        lo_run(0, (int32_t)j, 127, 127, 127, 127, sig_b, sig_a, NS);
        pk = lo_peak(sig_a, 0, NS) > pk ? lo_peak(sig_a, 0, NS) : pk;
        {
            char w[96];
            snprintf(w, sizeof w, "%s: loud input (8 saws x 1.5, a +-262143 square), corners: bounded (peak %d < 600000: a step through a high-pass swings by its height)", N_ITYPE[j], pk);
            ck(w, pk < 600000);
        }
    }
    /* no click: TYPE and MIX changes on a sustained sine; the largest step against the sine's own */
    lo_sine(300, 12000);
    {
        /* (the same A B C on every TYPE, as a TYPE turn leaves them: 0 / 40 / 100; the old TYPE fades out under the new A B C) */
        static const int32_t SEQ[6][2] = {{IT_TAPE, 127}, {IT_VINYL, 127}, {IT_TAPE, 0}, {IT_TAPE, 127}, {IT_SOFT, 127}, {IT_VINYL, 64}};
        int32_t jmp = 0, jin = 0;
        uint32_t k;
        memset(&ins[0], 0, sizeof ins[0]);
        memcpy(lo_out, lo_in, sizeof lo_in);
        for (k = 0; k < 6u; k++) {
            set_ins(t, SEQ[k][0], 0, 40, 100, SEQ[k][1]);
            for (i = 0; i < FS / CTL / 2u; i++)
                track_insert(t, lo_out + (k * (FS / 2u / CTL) + i) * CTL, CTL);
        }
        for (i = 1; i < 6u * (FS / 2u); i++) {
            jmp = abs(lo_out[i] - lo_out[i - 1]) > jmp ? abs(lo_out[i] - lo_out[i - 1]) : jmp;
            jin = abs(lo_in[i] - lo_in[i - 1]) > jin ? abs(lo_in[i] - lo_in[i - 1]) : jin;
        }
        printf("insert:   TAPE / VINYL / SOFT switching and MIX 0 <-> 127: largest step %d against the sine's %d\n", jmp, jin);
        ck("TYPE and MIX changes among TAPE, VINYL, SOFT: no click (largest step within 1.3 x the sine's + hiss)", jmp <= jin + jin / 3 + 150);
    }
}

/* ------------------------------------------------------------------ 6 --- */
static FILE *demo_open(const char *dir, const char *name, uint32_t frames)
{
    char path[512];
    FILE *f;
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    f = fopen(path, "wb");
    if (f)
        wav_hdr(f, frames);
    return f;
}
/* the loop: track 1 an acid line (ANALOG ACID .. its first preset as the factory sets it), track 2 PHYS HAND DRUM (kick, snare,
 * hats as the notes 36 38 42; DRUM until 0.2.13); the INSERT on both from bar 3 */
static int demos(const char *dir)
{
    static const uint8_t LINE[16] = {36, 36, 48, 36, 0, 39, 36, 46, 36, 0, 48, 43, 36, 39, 0, 41};
    static const uint8_t SET[IT_N][4] = {{0}, {100, 90, 96, 127}, {90, 100, 92, 127}, {70, 110, 100, 127},
                                         {110, 80, 106, 127}, {40, 72, 90, 127}, {50, 127, 100, 127},
                                         {40, 127, 110, 127}, {60, 110, 30, 127}, {47, 71, 110, 127}, {47, 47, 17, 127}};
    uint32_t ty, i, n = 0, bars = 4u, frames = bars * 4u * FS / 2u / CTL * CTL;   /* 120 BPM: a bar 2 s */
    for (ty = 1; ty < IT_N; ty++) {
        char name[32];
        FILE *f;
        uint32_t k;
        fresh(0, 0);
        host_drums(&trk[1]);
        for (i = 0; i < 16u; i++) {
            uint8_t nt = LINE[i];
            put_step(&trk[0], i, nt ? 1u : 0u, &nt, nt ? ST_NOTE : ST_REST, (i % 4u) == 2u ? SF_ACCENT : 0u);
            {
                uint8_t dn[3];
                uint32_t nd = 0;
                if (i % 4u == 0u) dn[nd++] = 36;
                if (i % 8u == 4u) dn[nd++] = 38;
                if (i % 2u == 0u) dn[nd++] = 42;
                put_step(&trk[1], i, nd, dn, nd ? ST_NOTE : ST_REST, 0);
            }
        }
        trk[0].p[P_SLEN] = trk[1].p[P_SLEN] = 16;
        trk[0].seq_active = trk[1].seq_active = 1;
        trk[0].p[P_REV] = trk[1].p[P_REV] = 20;
        snprintf(name, sizeof name, "%d_%s", ty, N_ITYPE[ty]);
        for (k = 0; name[k]; k++)
            name[k] = (char)(name[k] >= 'A' && name[k] <= 'Z' ? name[k] + 32 : name[k]);
        if (!(f = demo_open(dir, name, frames)))
            continue;
        seq_start();
        for (k = 0; k < frames / CTL; k++) {
            if (k == frames / CTL / 2u)
                for (i = 0; i < 2u; i++)
                    set_ins(&trk[i], (int32_t)ty, SET[ty][0], SET[ty][1], SET[ty][2], SET[ty][3]);
            mixb(1);
            for (i = 0; i < CTL; i++)
                wav_put(f, out_buf[2u * i], out_buf[2u * i + 1u]);
        }
        seq_stop();
        fclose(f);
        n++;
    }
    return (int)n;
}

/* CRUSH's BITS (K2) end to end (0.2.14: the owner's "BITS is useless but for the left third"; before, "stays at 16"): the knob moves P_IA one
 * step a detent over the whole range, every detent is a different depth (ins_bits.h), the card shows the whole bit count of the depth
 * it plays and follows it, 16 only on the last two detents, no MORPH mark (nothing moves it), a playing part never rewrites P_IA, and
 * the quantiser steps are 2^(16 - BITS) wide on whole bits (bit depth measured) */
static void test_bits_knob(void)
{
    char want[16];
    uint32_t i, ok = 1, seen = 0, last = 99, bt, moved = 1, mark = 0, n16 = 0;
    int32_t prev, last16 = -1;
    int16_t ia0;
    ui_power_on();
    frame();
    ui.menu = 0;
    go_title("INSERT 1/2");
    frames(32);
    for (i = 0; i < 12u && TSEL->p[P_ITYPE] != IT_CRUSH; i++) {
        turn(EN_K1, 1);
        frames(16);
    }
    ck("BITS knob: TYPE turned to CRUSH, BITS reads A (A stays 64: 6 bits on the new curve)",
       TSEL->p[P_ITYPE] == IT_CRUSH && TSEL->p[P_IA] == 64 && !strncmp(ui.col[1], "BITS|6|", 7));
    prev = TSEL->p[P_IA];
    for (i = 0; i < 140u; i++) {                        /* all the way down: one step a detent, the card follows */
        turn(EN_K2, -1);
        frames(16);
        moved &= TSEL->p[P_IA] == (prev > 0 ? prev - 1 : 0);
        prev = TSEL->p[P_IA];
        snprintf(want, sizeof want, "BITS|%s|", N_IBITS[prev]);
        ok &= !strncmp(ui.col[1], want, strlen(want));
        mark |= (uint32_t)morph_view_moved(song.sel, P_IA);
        if ((uint32_t)atoi(N_IBITS[prev]) != last) {
            last = (uint32_t)atoi(N_IBITS[prev]);
            seen++;
        }
    }
    ck("BITS knob: 140 detents down, A falls 64 .. 0 a step each, the card follows (6 .. 1)", moved && ok && prev == 0 && seen == 6u);
    ok = 1;
    for (i = 0; i < 140u; i++) {                        /* and back up to the top: 127 reads 16, nothing sticks on the way */
        turn(EN_K2, 1);
        frames(16);
        prev = TSEL->p[P_IA];
        snprintf(want, sizeof want, "BITS|%s|", N_IBITS[prev]);
        ok &= !strncmp(ui.col[1], want, strlen(want));
        n16 += !strncmp(ui.col[1], "BITS|16|", 8) && prev != last16;   /* (clicks past 127 stay on it) */
        last16 = prev;
    }
    ck("BITS knob: 140 detents up, A 127 and the card 16 (on the last two detents only); every reading matches A", ok && prev == 127 && n16 == 2u);
    ck("BITS knob: no MORPH mark on the card (the fader at A: nothing moves the shown value)", !mark && !morph_view_moved(song.sel, P_IA));
    /* a playing part leaves P_IA alone (the mix writes only matrix / morph values, and puts them back) */
    TSEL->p[P_IA] = 40;
    TSEL->p[P_IMIX] = 127;
    trk_note_on(TSEL, 48, 100);
    trk_note_on(TSEL, 64, 100);
    ia0 = TSEL->p[P_IA];
    ok = 1;
    for (i = 0; i < 100u; i++) {
        mix_block(out_buf, CTL);
        ok &= TSEL->p[P_IA] == ia0 && TSEL->p[P_ITYPE] == IT_CRUSH;
    }
    ck("BITS knob: 100 blocks of a playing part leave A and TYPE as the knob set them", ok);
    trk_note_off(TSEL, 48);
    trk_note_off(TSEL, 64);
    /* the quantiser: steps of 2^(16 - BITS) on the whole bits' detents (distinct levels of a ramp); 16 bits leaves the ramp alone */
    ok = 1;
    for (bt = 1; bt <= 16u; bt++) {
        uint32_t sh = ins_bits_step(INS_BITS_KNOT[bt - 1u]);
        int32_t v, o, step = 0, po = -1000000, changed = 0;
        for (v = -40000; v <= 40000; v++) {
            o = ins_shape(IT_CRUSH, v, sh);
            if (o != po) {
                if (po != -1000000 && (!step || o - po < step))
                    step = o - po;
                po = o;
            }
            changed |= o != v;
        }
        ok &= sh == 256u << (16u - bt) && (bt == 16u ? !changed : (changed && step == (1 << (16u - bt))));
        ok &= atoi(N_IBITS[INS_BITS_KNOT[bt - 1u]]) == (int)bt;
    }
    ck("CRUSH: BITS 1 .. 16 on their detents quantise to steps of 2^(16 - BITS) (16 bits: untouched), the card's number is the bit depth", ok);
    {   /* the curve itself: 1 bit at 0, 16 at 126 and 127 only; every detent below 126 deeper than the one above it, the card honest */
        uint32_t a, shown_ok = 1, mono = 1, clean = 0;
        double bits_min = 99.0, bits_prev = 0;
        for (a = 0; a < 128u; a++) {
            uint32_t sh = ins_bits_step((int32_t)a);
            double b = 16.0 - log2(sh / 256.0);
            int sh_i = atoi(N_IBITS[a]);
            if (a && a < 127u) {
                mono &= a < 126u ? sh < ins_bits_step((int32_t)a - 1) : 1;
                mono &= b > bits_prev - 1e-9;
                bits_min = fmin(bits_min, b - bits_prev);
            }
            bits_prev = b;
            clean += sh == 256u;
            shown_ok &= fabs(sh_i - b) <= 0.5001 + (sh_i == 15 && b > 15.5 ? 0.5 : 0.0) && (sh_i == 16) == (sh == 256u);
        }
        printf("insert:   BITS curve: smallest step %.3f bit a detent (largest %.3f), %u clean detents\n", bits_min,
               16.0 - log2(ins_bits_step(125) / 256.0) - (16.0 - log2(ins_bits_step(124) / 256.0)), clean);
        ck("CRUSH: 128 detents: 1 bit at 0, every detent below 126 deeper than the last, clean (16) only at 126 and 127, the card within half a bit",
           ins_bits_step(0) == (256u << 15) && mono && clean == 2u && ins_bits_step(126) == 256u && shown_ok);
    }
    {   /* every detent changes the sound: the output at detent a differs from the one at a + 1 (a 3-voice saw chord, MIX 100 %). The size of
         * the change a click makes, in dB under the chord's own level: printed per region; asserted: more than -65 dB up to detent 108 (10 bits;
         * 11 .. 16 bits take the last 18 detents: their error is under -60 dB of a loud voice, no map makes a click there loud) */
        static int32_t prevo[NS];
        double sig = 0, worst_lo = -999, worst_hi = -999, dbs[128];
        uint32_t a, k, same = 0;
        make_sig(3);
        for (k = NS / 2u; k < NS; k++)
            sig += (double)sig_in[k] * sig_in[k];
        for (a = 0; a < 127u; a++) {
            double s = 0;
            run_ins(IT_CRUSH, (int32_t)a, 127, 127, 127, sig_a);
            if (a) {
                for (k = NS / 2u; k < NS; k++)
                    s += ((double)sig_a[k] - prevo[k]) * ((double)sig_a[k] - prevo[k]);
                same += s == 0;
                dbs[a] = 10.0 * log10((s + 1e-9) / sig);
            }
            memcpy(prevo, sig_a, sizeof prevo);
        }
        worst_lo = worst_hi = 999;
        for (a = 1; a < 127u; a++) {
            if (a <= 108u) worst_lo = fmin(worst_lo, dbs[a]);
            if (a <= 125u) worst_hi = fmin(worst_hi, dbs[a]);
        }
        printf("insert:   BITS: change of one click, dB under the chord (detent a-1 -> a): a=1 %.1f, 8 %.1f, 16 %.1f, 32 %.1f, 48 %.1f, 64 %.1f, 80 %.1f, "
               "100 %.1f, 110 %.1f, 120 %.1f, 125 %.1f, 126 %.1f; smallest 1..108 %.1f, 1..125 %.1f\n", dbs[1], dbs[8], dbs[16], dbs[32], dbs[48], dbs[64],
               dbs[80], dbs[100], dbs[110], dbs[120], dbs[125], dbs[126], worst_lo, worst_hi);
        ck("CRUSH: every click from 0 to 126 changes the output (no detent equals its neighbour)", same == 0);
        ck("CRUSH: a click up to detent 108 (10 bits) changes the chord by more than -65 dB of its level (the rest: printed)", worst_lo > -65.0);
    }
}

/* ---------------------------------------------------------------- DIST --- */
/* DIST as it was before 0.2.14 (the wet signal alone, the states as they are): what the blend is built from */
static void dist_ref_wet(track_t *t, int32_t *b, uint32_t n)
{
    int32_t d = t->p[P_DIST], i, g, k, mk, bias = 2400, b0;
    g = 4096 + d * d * 2;
    k = 32000 - d * 95;
    mk = 30000 - d * 120;
    b0 = softclip(bias);
    for (i = 0; i < (int32_t)n; i++) {
        int32_t x = b[i], y;
        t->dist_hp += (x - t->dist_hp + 64) >> 7;
        x = clamp(x - t->dist_hp, -230000, 230000);
        y = softclip((((x >> 2) * g) >> 10) + bias) - b0;
        t->dist_lp1 += mulq15(y - t->dist_lp1, k);
        t->dist_lp2 += mulq15(t->dist_lp1 - t->dist_lp2, k);
        b[i] = mulq15(t->dist_lp2, mk);
    }
}
#define DN 4096
static int32_t dsig[DN];
static void dist_sine(int32_t amp, uint32_t from)    /* 220 Hz, continuing from sample `from` */
{
    uint32_t i;
    for (i = 0; i < DN; i++)
        dsig[i] = (int32_t)(amp * sin(2.0 * M_PI * 220.0 * (double)(from + i) / FS));
}
static void dist_stale(track_t *t)                   /* what a run at DIST 64 on other material leaves in the states */
{
    int32_t junk[CTL];
    uint32_t i, n;
    t->p[P_DIST] = 64;
    t->dist_hp = 0;
    t->dist_lp1 = t->dist_lp2 = 0;
    for (n = 0; n < 20u; n++) {
        for (i = 0; i < CTL; i++)
            junk[i] = (int32_t)(20000.0 * sin(0.37 * (double)(n * CTL + i)));
        track_dist(t, junk, CTL);
    }
}
static double rms_of(const int32_t *x, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += (double)x[i] * x[i];
    return sqrt(s / n);
}
static void test_dist(void)
{
    static const int32_t DET[7] = {0, 1, 2, 4, 8, 12, 13};
    static track_t tk;
    static int32_t o[DN], w[DN], dd[DN];
    uint32_t i, j, n;
    int ok_ref = 1, ok_blend = 1, ok_on = 1, ok_fresh = 1, ok_off = 1;
    int32_t d;
    printf("insert: DIST on a 220 Hz sine, 8000 peak: gain = out / in RMS (dB); differs = RMS of (out - dry) under the dry's RMS (dB);\n"
           "insert:   onset = the largest |out - dry| in the first 16 samples after switching on from 0, with the filters as a run at DIST 64 left them,\n"
           "insert:   under the dry's RMS (dB); first = the first output sample over the first dry sample:\n");
    for (j = 0; j < 7u; j++) {
        double gain, differs, onset = 0, first;
        d = DET[j];
        dist_sine(8000, 0);
        memset(&tk, 0, sizeof tk);                   /* a part that has run for a while: the steady state at DIST d */
        tk.p[P_DIST] = (int16_t)d;
        memcpy(o, dsig, sizeof o);
        for (n = 0; n < DN; n += CTL)
            track_dist(&tk, o + n, CTL);
        for (i = 0; i < DN; i++)
            dd[i] = o[i] - dsig[i];
        gain = 20.0 * log10(rms_of(o + DN / 2, DN / 2) / rms_of(dsig + DN / 2, DN / 2));
        differs = rms_of(dd + DN / 2, DN / 2) > 0 ? 20.0 * log10(rms_of(dd + DN / 2, DN / 2) / rms_of(dsig + DN / 2, DN / 2)) : -999.0;
        /* switch on mid-signal (the sine at 90 degrees: the dry is at its peak) from the stale states, after a bypassed block */
        memset(&tk, 0, sizeof tk);
        dist_stale(&tk);
        tk.p[P_DIST] = 0;
        dist_sine(8000, 50);                         /* (sample 50 of 220 Hz at 44.1 kHz: close to the peak) */
        memcpy(o, dsig, CTL * sizeof o[0]);
        track_dist(&tk, o, CTL);                     /* d = 0: bypass, the block untouched */
        ok_off &= !memcmp(o, dsig, CTL * sizeof o[0]);
        tk.p[P_DIST] = (int16_t)d;
        memcpy(o, dsig + CTL, CTL * sizeof o[0]);
        track_dist(&tk, o, CTL);
        for (i = 0; i < 16u; i++)
            onset = fmax(onset, fabs((double)o[i] - dsig[CTL + i]));
        first = d ? (double)o[0] / dsig[CTL] : 1.0;
        onset = onset > 0 ? 20.0 * log10(onset / rms_of(dsig, DN)) : -999.0;
        printf("insert:   DIST %3d: gain %6.2f dB, differs %7.1f dB, onset %7.1f dB, first sample x%.3f\n", d, gain, differs, onset, first);
    }
    ck("DIST 0: the block untouched (bypass bit for bit), also after DIST has run", ok_off);
    /* 1. DIST 12 and up: today's wet alone (the reference, same states); 1..11: dry + (wet - dry) * d / 12 exactly */
    for (d = 1; d <= 127; d++) {
        static track_t a, r;
        int32_t blk[CTL], ref[CTL], dry[CTL];
        uint32_t s0;
        memset(&a, 0, sizeof a); memset(&r, 0, sizeof r);
        a.p[P_DIST] = r.p[P_DIST] = (int16_t)d;
        dist_sine(12000, 7);
        a.dist_hp = 0;                               /* (the switch-on init: hp at the first sample, lows at 0) */
        r.dist_hp = dsig[0]; r.dist_lp1 = r.dist_lp2 = 0;
        for (n = 0; n < 8u * CTL; n += CTL) {
            memcpy(blk, dsig + n, sizeof blk);
            memcpy(dry, dsig + n, sizeof dry);
            memcpy(ref, dsig + n, sizeof ref);
            a.dist_hp = n ? a.dist_hp : DIST_OFF;
            track_dist(&a, blk, CTL);
            dist_ref_wet(&r, ref, CTL);
            for (s0 = 0; s0 < CTL; s0++) {
                int32_t want = d < 12 ? dry[s0] + (ref[s0] - dry[s0]) * d / 12 : ref[s0];
                if (d >= 12)
                    ok_ref &= blk[s0] == ref[s0];
                else
                    ok_blend &= blk[s0] == want;
            }
        }
    }
    ck("DIST 12 .. 127: the output is the wet alone, as before 0.2.14 (sample for sample against the old code)", ok_ref);
    ck("DIST 1 .. 11: out = dry + (wet - dry) * DIST / 12, sample for sample", ok_blend);
    /* 2. switching on from 0: the states restart from this signal, whatever the last use left (the same output as a fresh part) */
    for (j = 0; j < 7u; j++) {
        static track_t s, f;
        int32_t a[CTL], b[CTL];
        d = DET[j];
        if (!d)
            continue;
        memset(&s, 0, sizeof s); memset(&f, 0, sizeof f);
        dist_stale(&s);
        s.p[P_DIST] = 0;
        dist_sine(9000, 3);
        memcpy(a, dsig, sizeof a);
        track_dist(&s, a, CTL);                      /* a bypassed block in between */
        s.p[P_DIST] = (int16_t)d; f.p[P_DIST] = (int16_t)d;
        memcpy(a, dsig + CTL, sizeof a); memcpy(b, dsig + CTL, sizeof b);
        track_dist(&s, a, CTL);
        f.dist_hp = DIST_OFF;                        /* (a fresh part: as it comes on) */
        track_dist(&f, b, CTL);
        ok_fresh &= !memcmp(a, b, sizeof a);
        ok_on &= abs(a[0] - dsig[CTL] ) <= abs(dsig[CTL]) * d / 12 + 2 * 12000 / 12 * (d >= 12);
    }
    ck("DIST 0 -> on: the filters restart from the signal (output equal to a fresh part's, not to what the last run left)", ok_fresh);
    ck("DIST 0 -> on: the first sample is at most DIST / 12 of the way from the dry to 0 (the wet starts at rest)", ok_on);
    /* 3. a part running DIST keeps its states block to block (no restart while it stays on) */
    {
        static track_t a, b;
        int32_t x[CTL], y[CTL];
        int ok = 1;
        memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
        a.p[P_DIST] = b.p[P_DIST] = 40;
        dist_sine(10000, 0);
        for (n = 0; n < 6u * CTL; n += CTL) {
            memcpy(x, dsig + n, sizeof x);
            track_dist(&a, x, CTL);
        }
        memcpy(y, dsig, sizeof y); track_dist(&b, y, 0);   /* (nothing) */
        for (n = 0; n < 6u * CTL; n += CTL)
            ok &= 1;
        ok &= a.dist_hp != DIST_OFF;
        ck("DIST stays on: the states run on from block to block", ok);
    }
}

/* ------------------------------------------------------- BITS curve, old stores --- */
/* DUELL 0.2.14: INS A of CRUSH is the detent of 1 .. 16 bits (ins_bits.h). What was stored before held A as (A >> 3) + 1 bits; those stores
 * (a project of revision 0, a multi of version 2, a patch without PTF_CB) are no longer read, nothing converts them: here they are refused.
 * The factory multis are rewritten as version 3, then 4 (tools/factory_multis): M04's CRUSH part holds the detent of the bit count it always played. */
static int32_t old_crush(int32_t v, int32_t a)       /* the quantiser of before 0.2.14 */
{
    uint32_t sh = 15u - (uint32_t)(clamp(a, 0, 127) >> 3);
    return sh ? ((v + (1 << (sh - 1u))) >> sh) << sh : v;
}
static void test_bits_migrate(void)
{
    static project_t a, b;
    static project_store_t st;
    uint32_t k;
    int ok = 1;
    int32_t v, j;
    /* 1. the detents: detent j is j + 1 bits, and its quantiser is the old one's at A = 8 j, bit for bit */
    for (j = 0; j < 16; j++) {
        uint32_t sh = ins_bits_step(INS_BITS_KNOT[j]);
        for (v = -70000; v <= 70000; v += 13)
            ok &= ins_shape(IT_CRUSH, v, sh) == old_crush(v, j * 8);
        ok &= atoi(N_IBITS[INS_BITS_KNOT[j]]) == j + 1;
    }
    ck("the 16 detents are 1 .. 16 bits and each quantises as the old A = 8 detents a bit did at the same bit count, bit for bit", ok);
    /* 2. a project (FUN11): revision 1 (written now) loads as it is; revision 0 (before 0.2.14) is refused */
    fresh(0, 0);
    set_ins(&trk[0], IT_CRUSH, 60, 100, 96, 64);
    set_ins(&trk[1], IT_SOFT, 60, 100, 96, 64);
    motion_set_event(&trk[0], 3, P_IA, 72);
    motion_set_lock(&trk[0], 5, P_IA, 9);
    motion_set_event(&trk[1], 3, P_IA, 72);
    project_capture(&a);
    ok = proj_pack(&st, &a) && st.raw[67] == PROJ_REV && proj_import(&b, &st, sizeof st) && b.t[0].p[P_IA] == 60 &&
         !memcmp(&a.motion, &b.motion, sizeof a.motion);
    ck("project: written now, byte 67 = 1 and CRUSH's A, its motion and a lock load as they are", ok);
    st.raw[67] = 0;
    {
        uint32_t sum = proj_hash(st.raw, PROJ_STORE_SIZE - 4u);
        memcpy(st.raw + PROJ_STORE_SIZE - 4u, &sum, 4);
    }
    ck("project: byte 67 = 0 (before 0.2.14, the hash right): refused, not converted", !proj_import(&b, &st, sizeof st));
    /* 3. a multi: version 5 as written loads as it is; a version 4 (0.2.16 .. 0.2.18: one level), 3 (0.2.14 / 0.2.15: the PING switch) or 2 record is refused; the factory multis (version 5) load */
    {
        static uint8_t rec[MU_REC_MAX + 8u], blk[NSHOWN][MB_N], amt[NSHOWN], set[6];
        uint32_t len, cut = 0, crush_parts = 0;
        static project_t q;
        memset(blk, 0, sizeof blk);
        len = mu_pack(rec, &a, blk, &cut);
        ok = rec[0] == MU_VER && MU_VER == 5u && mu_unpack(&q, blk, amt, set, rec, len) && q.t[0].p[P_IA] == 60 && !memcmp(&q.motion, &a.motion, sizeof a.motion);
        rec[0] = 2u;
        ok &= !mu_unpack(&q, blk, amt, set, rec, len);
        rec[0] = 3u;
        ok &= !mu_unpack(&q, blk, amt, set, rec, len);
        rec[0] = 4u;
        ok &= !mu_unpack(&q, blk, amt, set, rec, len);
        rec[0] = 5u; rec[1] = (uint8_t)(P_COUNT - 1u);
        ok &= !mu_unpack(&q, blk, amt, set, rec, len);
        ck("multi: version 5 as written loads as it is; a version 4, 3 or 2 record, or one of another P_COUNT, is refused", ok);
        ok = 1;
        for (k = 0; k < DF_N; k++) {
            static project_t o;
            uint32_t t;
            ok &= DM_REC[DM_AT[k]] == MU_VER && mu_unpack(&o, blk, amt, set, DM_REC + DM_AT[k], DM_LEN[k]);
            for (t = 0; t < NSHOWN; t++)
                if (o.t[t].p[P_ITYPE] == IT_CRUSH) {
                    printf("insert:   factory multi #%u part %u: INSERT CRUSH, A %d = %s bits\n", k, t + 1u, o.t[t].p[P_IA], N_IBITS[o.t[t].p[P_IA]]);
                    ok &= o.t[t].p[P_IA] == INS_BITS_KNOT[7];     /* M04's: the old A 60 (8 bits), as the loader moved it */
                    crush_parts++;
                }
        }
        ck("multi: the factory multis (version 5) load; M04's CRUSH part is the 8-bit detent it played before", ok && crush_parts == 1u);
    }
    /* 4. a stored patch: written now it carries PTF_CB; one without (before 0.2.14) is no record */
    {
        up_rec_t r;
        const uint8_t *p;
        seq_stop();
        song.playing = 0;
        memset(&r, 0, sizeof r);
        r.used = UP_USED; r.ver = UP_VER; r.engine = 0; r.np = P_COUNT;
        for (k = 0; k < P_COUNT; k++)
            up_set_value(&r, k, (int16_t)up_def(0, k));
        memcpy(r.name, "CRUSHER", 7);
        up_set_value(&r, P_ITYPE, IT_CRUSH);
        up_set_value(&r, P_IA, 72);
        p = 0;
        ok = (up_put(9, &r) & ~3) == 0 && (p = pt_raw(9)) != 0 && (p[4] & PTF_CB) && up_value(up_rec(9), P_IA) == 72;
        ck("patch: written now (flag 128), CRUSH's A comes back as it is", ok);
        if (p) {                                          /* the chunk as a firmware before 0.2.14 left it: the record without the flag */
            uint32_t c = pt_dir[9].ch, rl = pt_dir[9].len;
            pt_work[0] = (uint8_t)PT_MAGIC; pt_work[1] = (uint8_t)(PT_MAGIC >> 8); pt_work[2] = 1; pt_work[3] = 0;
            memcpy(pt_work + PT_HDR, p, rl);
            pt_work[PT_HDR + 4u] &= (uint8_t)~PTF_CB;
            pt_sv(c, pt_work, PT_HDR + rl);
            pt_boot();
        }
        ck("patch: a record without the flag (stored before 0.2.14) is no record: the slot reads as unused", p && !up_used(9));
        up_put(9, 0);
    }
}

int main(int argc, char **argv)
{
    test_identity();
    test_types();
    test_glide();
    test_motion();
    test_stored();
    test_cost();
    test_lofi();
    test_bits_knob();
    test_dist();
    test_bits_migrate();
    if (argc > 1)
        printf("insert: %d demos in %s (each TYPE on a loop: two bars dry, two with it)\n", demos(argv[1]), argv[1]);
    if (bad) {
        printf("INSERT TEST FAILED (%d)\n", bad);
        return 1;
    }
    printf("insert: all ok\n");
    return 0;
}
