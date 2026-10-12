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
    {   /* CRUSH's steps are odd: -x gives minus what x gives (but at an exact half step, which rounds up), every BITS */
        int ok = 1;
        int32_t v, a;
        for (a = 0; a < 128; a += 8)
            for (v = -140000; v <= 140000; v += 7) {
                uint32_t sh = ins_bits_sh(a);
                int32_t half = sh && !((v + (1 << (sh - 1u))) & ((1 << sh) - 1));
                ok &= half || ins_shape(IT_CRUSH, -v, sh) == -ins_shape(IT_CRUSH, v, sh);
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
    int16_t v[P_COUNT], def[P_COUNT];
    uint32_t i, k;
    int ok;
    fresh(0, 0);
    for (k = 0; k < NTRK; k++)
        set_ins(&trk[k], (int32_t)(k * 2u + 1u), (int32_t)(10 + k), (int32_t)(20 + k), (int32_t)(30 + k), (int32_t)(100 + k));
    motion_set_event(&trk[1], 3, P_IMIX, 77);
    motion_set_lock(&trk[0], 5, P_IA, 9);               /* (DUELL: FUN11 stores parts A and B) */
    project_capture(&a);
    ok = proj_pack(&st, &a) && st.raw[66] == P_COUNT && P_COUNT == 111u && proj_import(&b, &st, sizeof st) &&
         !memcmp(&a.t, &b.t, NSHOWN * sizeof a.t[0]) && !memcmp(&a.motion, &b.motion, sizeof a.motion);
    for (k = 0; k < NSHOWN; k++)
        ok &= b.t[k].p[P_ITYPE] == (int16_t)(k * 2u + 1u) && b.t[k].p[P_IMIX] == (int16_t)(100 + k) &&
              b.t[k].p[P_IC] == (int16_t)(30 + k);
    ok &= 68u + PROJ_NTRK * (P_COUNT + 2u + NSTEP * 9u) + sizeof(chain_config_t) + sizeof(motion_store_t) + 1650u == PROJ_FM6_OFF;
    ck("project: FUN11 round trip of the INSERT (111 parameters, its motion; parts A B), 1650 spare bytes", ok);
    /* user presets: a record of 111 keeps them; one of 104 (1.2 .. 1.4) gets the INSERT OFF */
    memset(&r, 0, sizeof r);
    r.used = UP_USED; r.ver = UP_VER; r.engine = 0; r.np = P_COUNT;
    for (i = 0; i < P_COUNT; i++) { up_set_value(&r, i, trk[3].p[i]); def[i] = TP[i].def; }
    up_params(&r, v, def);
    ok = v[P_ITYPE] == 7 && v[P_IA] == 13 && v[P_IMIX] == 103;
    r.np = 104;
    for (i = 0; i < 96u; i++) up_set_value(&r, i, trk[3].p[i]);
    for (i = 0; i < 8u; i++) up_set_value(&r, 96u + i, (int16_t)(40 + i));
    up_params(&r, v, def);
    ok &= v[P_ITYPE] == 0 && v[P_IA] == 64 && v[P_IB] == 96 && v[P_IC] == 96 && v[P_IMIX] == 127 && v[P_SPRD] == trk[3].p[P_SPRD] &&
          v[P_E0] == 40 && v[P_E7] == 47;
    ck("user presets: 111 parameters kept; one of 104 (1.2 .. 1.4): the INSERT OFF, its engine values in place", ok);
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
        set_ins(t, IT_CRUSH, 64, 96, 96, 127);
        ok &= str_eq(track_desc(t, P_IA)->label, "BITS") && str_eq(track_desc(t, P_IB)->label, "RATE") &&
              str_eq(track_desc(t, P_IC)->label, "LPF");
        param_format(track_desc(t, P_IA), 64, val, &u);
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
/* the loop: track 1 an acid line (ANALOG ACID .. its first preset as the factory sets it), track 2 DRUM (kick, snare,
 * hats); the INSERT on both from bar 3 */
static int demos(const char *dir)
{
    static const uint8_t LINE[16] = {36, 36, 48, 36, 0, 39, 36, 46, 36, 0, 48, 43, 36, 39, 0, 41};
    static const uint8_t SET[IT_N][4] = {{0}, {100, 90, 96, 127}, {90, 100, 92, 127}, {70, 110, 100, 127},
                                         {110, 80, 106, 127}, {40, 72, 90, 127}, {50, 127, 100, 127},
                                         {40, 127, 110, 127}, {60, 110, 30, 127}};
    uint32_t ty, i, n = 0, bars = 4u, frames = bars * 4u * FS / 2u / CTL * CTL;   /* 120 BPM: a bar 2 s */
    for (ty = 1; ty < IT_N; ty++) {
        char name[32];
        FILE *f;
        uint32_t k;
        fresh(0, 0);
        host_preset(&trk[1], ENGI_DRUM, 0);
        for (i = 0; i < 16u; i++) {
            uint8_t nt = LINE[i];
            put_step(&trk[0], i, nt ? 1u : 0u, &nt, nt ? ST_NOTE : ST_REST, (i % 4u) == 2u ? SF_ACCENT : 0u);
            put_step(&trk[1], i, 0, 0, ST_NOTE, 0);
            trk[1].step[i].hit = (uint8_t)((i % 4u == 0u ? 1u : 0u) | (i % 8u == 4u ? 2u : 0u) | (i % 2u == 0u ? 8u : 0u));
            trk[1].step[i].vel = 100;
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

int main(int argc, char **argv)
{
    test_identity();
    test_types();
    test_glide();
    test_motion();
    test_stored();
    test_cost();
    if (argc > 1)
        printf("insert: %d demos in %s (each TYPE on a loop: two bars dry, two with it)\n", demos(argv[1]), argv[1]);
    if (bad) {
        printf("INSERT TEST FAILED (%d)\n", bad);
        return 1;
    }
    printf("insert: all ok\n");
    return 0;
}
