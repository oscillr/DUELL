/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.1 item 160 (owner decision, third rule 2026-10-29): PERFORM's SELECT = BALANCE moves a position p between -1 (part 2 only) and +1 (part 1 only), 0 the middle,
 * 32 detents from the middle to each end. It changes only the two MIXER levels (LEVEL 1 and LEVEL 2, P_LEVEL), computed again from the base levels and the position each
 * time (ui_input.c perf_balance): the part that goes down g' = g x (1 - |p|), the part that goes up g' = g + (g0dB - g) x |p|, in linear gain, then the nearest level-table
 * index. A turn to the right moves toward part 2 (p falls), to the left toward part 1. Built and run by DUELL/host_tests.sh (name: perflevel); the mutants: perflevelmut.
 *   AUDIO      every mode (DUEL, STEREO, LAYER, LAYER + XFADE, LAYER + SPLIT), a busy two-part render: the output hash at p = 0 equals what the sources before item 160 gave
 *              (PL_HASH); and the knob turned in the middle of a render = the same levels written straight into P_LEVEL at the same block, sample for sample
 *   RULE       p = 0 gives the base levels; the two ends give OFF for the part that goes down and 0 dB (index 112) for the part that goes up; no level above index 112
 *              (a MIXER boost is never raised); the formula against an independent reference in double arithmetic; returning to p = 0 restores the base levels exactly after
 *              any walk; a level changed from outside while p is not 0 becomes the new base
 *   SCREEN     the footer row reads "SELECT = BALANCE" (no number), no header message */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <sys/wait.h>

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    printf("perflevel: %-110s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        bad++;
    return ok;
}

/* the reference: the owner's formula in double arithmetic and the nearest index of the real level table. detents: the position p x 32 (-32 .. +32, plus = part 1 louder);
 * up: this part goes up (else it goes down) */
static int32_t ref_level(int32_t base, int32_t detents, int32_t up)
{
    double p = (detents < 0 ? -detents : detents) / 32.0, g = LEVEL_Q12[base & 127], g0 = LEVEL_Q12[112], best = 1e30;
    int32_t i, bi = 0;
    if (detents == 0)
        return base;
    g = up ? g + (g0 - g) * p : g * (1.0 - p);
    for (i = 0; i < 128; i++) {
        double d = LEVEL_Q12[i] - g;
        d = d < 0 ? -d : d;
        if (d < best)
            best = d, bi = i;
    }
    return g <= 0.0 ? 0 : bi;
}

/* ------------------------------------------------------------------ the audio --- */
static uint32_t pl_eng(const char *n)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (str_eq(ENGINES[e]->name, n))
            return e;
    return 0;
}
static void pl_midi(uint32_t st, uint32_t d1, uint32_t d2)   /* a USB-MIDI packet on channel 1, as the IN endpoint queues it (mode_test.c midi) */
{
    midi_in_event((st >> 4) | st << 8 | d1 << 16 | d2 << 24);
}
enum { PM_DUEL, PM_STEREO, PM_LAYER, PM_LAYER_XF, PM_LAYER_SPLIT, PM_N };
static const char *const PM_NAME[PM_N] = {"DUEL", "STEREO", "LAYER", "LAYER + XFADE 60", "LAYER + SPLIT C4"};
/* one render in mode m (in a fresh child: the process's own power-on state). kb >= 0: at block kb the levels move: by perf_balance(steps) (how 1) or written
 * straight into P_LEVEL as the result of the same steps (how 2, the values passed in l1 / l2); how 0: nothing moves. The FNV hash of both channels */
static uint64_t pl_render(uint32_t m, uint32_t how, int32_t steps, int32_t l1, int32_t l2)
{
    static const uint8_t C1[3] = {48, 55, 64}, C2[3] = {60, 67, 72};
    static int32_t o[2 * CTL];
    uint64_t h = 0xCBF29CE484222325ull;
    uint32_t i, b, nb = 2u * FS / CTL, kb = nb / 3u;
    host_tracks_init();
    duell_mode_force(m == PM_DUEL ? DM_STEREO : DM_SPLIT);
    duell_mode_force(m == PM_DUEL ? DM_SPLIT : m == PM_STEREO ? DM_STEREO : DM_LAYER);
    mix_xf = m == PM_LAYER_XF ? 60u : 0u;
    mix_split = m == PM_LAYER_SPLIT ? 61u : 0u;
    host_preset(&trk[0], pl_eng("ANALOG"), 1);
    host_preset(&trk[1], pl_eng("PHASE"), 2);
    song.g[G_RTYPE] = 2;
    song.g[G_DFDBK] = 70;
    song.g[G_DMIX] = 100;
    trk[0].p[P_VOICE] = V_POLY;
    trk[1].p[P_VOICE] = V_POLY;
    trk[0].p[P_SPRD] = 90;                              /* (part 1 through mix_spread and the SPREAD post-pass, part 2 the plain part_mix) */
    trk[0].p[P_PAN] = -20;
    trk[1].p[P_PAN] = 30;
    trk[0].p[P_LEVEL] = 106;                            /* -3.0 dB */
    trk[1].p[P_LEVEL] = 116;                            /* +2.0 dB (a MIXER boost) */
    for (i = 0; i < 2u; i++) {
        trk[i].p[P_TRIM] = 112;
        trk[i].p[P_CHOR] = 60;
        trk[i].p[P_DLY] = 50;
        trk[i].p[P_REV] = 80;
        trk[i].p[P_SUS] = 110;
        trk[i].p[P_REL] = 60;
    }
    for (b = 0; b < nb; b++) {
        if (b == 8u)
            for (i = 0; i < 3u; i++) {
                if (m >= PM_LAYER) {                    /* (LAYER: the notes reach part 2 through the layer, as the keys / channel 1 do) */
                    pl_midi(0x90, C1[i], 110);
                    pl_midi(0x90, C2[i], 100);
                } else {
                    trk_note_on(&trk[0], C1[i], 110);
                    trk_note_on(&trk[1], C2[i], 100);
                }
            }
        if (b == kb && how == 1u)
            perf_balance(steps);
        if (b == kb && how == 2u)
            trk[0].p[P_LEVEL] = (int16_t)l1, trk[1].p[P_LEVEL] = (int16_t)l2;
        if (b == nb / 2u)
            for (i = 0; i < 3u; i++) {
                if (m >= PM_LAYER) {
                    pl_midi(0x80, C1[i], 0);
                    pl_midi(0x80, C2[i], 0);
                } else {
                    trk_note_off(&trk[0], C1[i]);
                    trk_note_off(&trk[1], C2[i]);
                }
            }
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++)
            h = (h ^ (uint32_t)o[i]) * 0x100000001B3ull;
    }
    return h;
}
static uint64_t pl_child(uint32_t m, uint32_t how, int32_t steps, int32_t l1, int32_t l2)
{
    int fd[2];
    uint64_t v = 0;
    pid_t pid;
    if (pipe(fd))
        return 0;
    pid = fork();
    if (!pid) {
        v = pl_render(m, how, steps, l1, l2);
        if (write(fd[1], &v, sizeof v) != (ssize_t)sizeof v)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], &v, sizeof v) != (ssize_t)sizeof v)
        v = 0;
    close(fd[0]);
    waitpid(pid, NULL, 0);
    return v;
}
/* the renders of the tree BEFORE item 160 (old balance 0, the shipped state), how 0, by mode: built with -DPL_RECORD=1 against that tree */
static const uint64_t PL_HASH[PM_N] = {
    0xD60A01CCF705A4B9ull, 0x1FD88D10F5B987DAull, 0xF49F6F3CADC3896Full, 0x88D3570D3A4EC29Eull, 0x73B251DCBB68F496ull,   /* (item 160: the tree before it, 2026-10-28) */
};
static void t_audio(void)
{
    uint32_t m;
    uint64_t h[PM_N];
    for (m = 0; m < PM_N; m++) {
        h[m] = pl_child(m, 0u, 0, 0, 0);
        printf("perflevel: render %-18s 0x%016llXull\n", PM_NAME[m], (unsigned long long)h[m]);
    }
#ifndef PL_RECORD
    for (m = 0; m < PM_N; m++) {
        char w[120];
        snprintf(w, sizeof w, "%s: the output bit for bit as before item 160 (old balance 0)", PM_NAME[m]);
        expect(w, h[m] != 0u && h[m] == PL_HASH[m]);
    }
    {   /* the knob's steps at block kb = the same levels written as the MIXER writes them; and they do change the sound */
        static const int32_t K[3] = {+5, -4, -12};   /* detents from the middle (base L1 106 / L2 116, a MIXER boost); the written levels come from the reference */
        for (m = 0; m < PM_N; m++) {
            uint32_t k, same = 1, moved = 1;
            for (k = 0; k < 3u; k++) {
                uint64_t a = pl_child(m, 1u, K[k], 0, 0), b = pl_child(m, 2u, 0, ref_level(106, -K[k], K[k] < 0), ref_level(116, -K[k], K[k] > 0));
                same &= a == b && a != 0u;
                moved &= a != h[m];
            }
            char w[140];
            snprintf(w, sizeof w, "%s: SELECT turned mid-render = the same LEVELs written straight (3 turns), and it is heard", PM_NAME[m]);
            expect(w, same && moved);
        }
    }
#endif
}

/* ------------------------------------------------------------------ the rule --- */
static int lv_is(int32_t a, int32_t b) { return trk[0].p[P_LEVEL] == a && trk[1].p[P_LEVEL] == b; }
/* the MIXER writes both levels from outside: the position goes back to the middle at the next SELECT step (here at once, so equal values cannot hide a change) */
static void lv_set(int32_t a, int32_t b) { trk[0].p[P_LEVEL] = (int16_t)a; trk[1].p[P_LEVEL] = (int16_t)b; perf_bal_pos = 0; }
static void perform_on(void) { hold(B_FX); frame(); }
/* the levels the reference gives for the position pos (detents, plus = part 1 louder) from the base levels */
static int lv_ref(int32_t b1, int32_t b2, int32_t pos)
{
    return lv_is(ref_level(b1, pos, pos > 0), ref_level(b2, pos, pos < 0));
}
static void t_rule(void)
{
    static const int32_t BASE[][2] = {{112, 106}, {106, 112}, {100, 90}, {112, 112}, {80, 120}, {120, 116}, {2, 60}, {0, 70}, {70, 0}, {127, 127}, {1, 1}};
    char l[24], r[24];
    uint32_t i, n;
    int ok;
    ui_power_on();
    frame();
    perform_on();
    expect("PERFORM on", ui.perf != 0);
    /* the middle: a step and the step back, and walks of any length, return exactly to the base levels */
    ok = 1;
    for (n = 0; n < sizeof BASE / sizeof BASE[0]; n++) {
        lv_set(BASE[n][0], BASE[n][1]);
        turn(EN_SELECT, 1);
        turn(EN_SELECT, -1);
        ok &= lv_is(BASE[n][0], BASE[n][1]);
        turn(EN_SELECT, 17);
        turn(EN_SELECT, -40);
        turn(EN_SELECT, 63);
        turn(EN_SELECT, -3);
        turn(EN_SELECT, 11);
        turn(EN_SELECT, -2);
        turn(EN_SELECT, -64);                             /* (the walk ends held at the left end) */
        turn(EN_SELECT, 32);                              /* and 32 right: the middle */
        ok &= lv_is(BASE[n][0], BASE[n][1]);
    }
    expect("p = 0 (after a step and back, and after long walks to both ends and back): exactly the base levels, 11 base pairs (OFF, boost, 0 dB, equal levels)", ok);
    /* the ends */
    ok = 1;
    for (n = 0; n < sizeof BASE / sizeof BASE[0]; n++) {
        lv_set(BASE[n][0], BASE[n][1]);
        turn(EN_SELECT, -32);                             /* to the right end: part 2 only */
        ok &= lv_is(0, 112);
        turn(EN_SELECT, -5);
        ok &= lv_is(0, 112);                              /* (no further) */
        turn(EN_SELECT, 64);                              /* to the left end: part 1 only */
        ok &= lv_is(112, 0);
        turn(EN_SELECT, 5);
        ok &= lv_is(112, 0);
        turn(EN_SELECT, -32);
        ok &= lv_is(BASE[n][0], BASE[n][1]);
    }
    expect("the ends: p = +1 (32 detents left): LEVEL 1 at 0 dB (112), LEVEL 2 OFF; p = -1 (32 right): the mirror; more turning stays; back to the middle = the base levels", ok);
    /* no level above index 112 (unless the base itself is above and then it is not raised) and the direction of each part */
    {
        uint32_t above = 0, wrongway = 0;
        int32_t pos;
        for (n = 0; n < sizeof BASE / sizeof BASE[0]; n++) {
            int32_t prev1 = BASE[n][0], prev2 = BASE[n][1];
            lv_set(BASE[n][0], BASE[n][1]);
            for (pos = 1; pos <= 32; pos++) {             /* one detent at a time toward part 1 (to the left) */
                int32_t m1 = BASE[n][0] > 112 ? BASE[n][0] : 112, m2 = BASE[n][1] > 112 ? BASE[n][1] : 112;
                turn(EN_SELECT, -1);
                above += trk[0].p[P_LEVEL] > m1 || trk[1].p[P_LEVEL] > m2;
                wrongway += trk[1].p[P_LEVEL] > prev2;    /* (part 2 only falls) */
                if (BASE[n][0] <= 112)
                    wrongway += trk[0].p[P_LEVEL] < prev1;    /* (part 1 only rises) */
                prev1 = trk[0].p[P_LEVEL], prev2 = trk[1].p[P_LEVEL];
            }
        }
        printf("perflevel:   %u levels above their limit, %u parts moving the wrong way\n", above, wrongway);
        expect("no level above index 112 (a MIXER boost is never raised); the part that goes down only falls, the part that goes up only rises", !above && !wrongway);
    }
    /* every position of every base pair against the reference formula */
    {
        uint32_t wrong = 0, total = 0;
        int32_t pos;
        for (n = 0; n < sizeof BASE / sizeof BASE[0]; n++)
            for (pos = -32; pos <= 32; pos++) {
                lv_set(BASE[n][0], BASE[n][1]);
                turn(EN_SELECT, -pos);                    /* (one turn from the middle: pos detents left = plus) */
                wrong += !lv_ref(BASE[n][0], BASE[n][1], pos);
                total++;
            }
        printf("perflevel:   formula: %u of %u positions off the reference\n", wrong, total);
        expect("g' = g x (1 - |p|) for the part that goes down, g' = g + (g0dB - g) x |p| for the part that goes up, nearest table index: all 65 positions of 11 base pairs", !wrong);
    }
    /* random walks: every step by 1..70 detents, either way; each result = the formula at the stored position; at the end back to the middle = the base */
    {
        uint32_t seed = 0x1234567u, wrong = 0, notbase = 0, wl;
        for (wl = 0; wl < 40u; wl++) {
            int32_t pos = 0, b1 = (int32_t)((seed >> 8) % 128u), b2 = (int32_t)((seed >> 16) % 128u);
            seed = seed * 1664525u + 1013904223u;
            b1 = (int32_t)((seed >> 8) % 128u), b2 = (int32_t)((seed >> 16) % 128u);
            lv_set(b1, b2);
            for (i = 0; i < 200u; i++) {
                int32_t d;
                seed = seed * 1664525u + 1013904223u;
                d = (int32_t)((seed >> 9) % 70u) + 1;
                d = (seed >> 7) & 1u ? d : -d;
                turn(EN_SELECT, d);
                pos -= d;
                pos = pos < -32 ? -32 : pos > 32 ? 32 : pos;
                wrong += !lv_ref(b1, b2, pos);
            }
            turn(EN_SELECT, pos);                         /* (back to the middle) */
            notbase += !lv_is(b1, b2);
        }
        printf("perflevel:   8000 random turns: %u off the formula, %u walks not back at the base\n", wrong, notbase);
        expect("40 random walks of 200 turns (1..70 detents, either way): every result = the formula at the stored position; back at p = 0 = the base levels exactly (no drift)", !wrong && !notbase);
    }
    /* a level changed from outside while p is not 0 becomes the new base, p = 0 */
    {
        ok = 1;
        lv_set(112, 100);
        perf_bal_pos = 0;
        turn(EN_SELECT, 10);                              /* p = -10/32 */
        ok &= lv_ref(112, 100, -10);
        trk[1].p[P_LEVEL] = 90;                           /* the MIXER's knob (or MIDI) moves LEVEL 2; LEVEL 1 stays as the balance left it */
        {
            int32_t b1 = trk[0].p[P_LEVEL];
            turn(EN_SELECT, -1);                          /* one step toward part 1 from the new base: p = +1/32 */
            ok &= lv_ref(b1, 90, 1);
            turn(EN_SELECT, 1);
            ok &= lv_is(b1, 90);                          /* (back in the middle: the new base, not the old one) */
        }
        expect("a level changed from outside while p is not 0: the levels then standing are the new base, p is 0 (the next step starts from the middle, back to p = 0 = the new base)", ok);
    }
    lv_set(112, 106);
    ui.msg_t = 0;
    turn(EN_SELECT, 1);
    frame();
    expect("SCREEN: the footer row reads \"SELECT = BALANCE\" (no number), ALGORITHM's row as before",
           perf_foot(l, r) && str_eq(l, "SELECT = BALANCE") && str_eq(r, "ALGORITHM = PART 1"));
    expect("  no header message for a step", !ui.msg_t);
    expect("  still PERFORM, no page change, no sound loaded", ui.perf && cur_page()->scope == SC_PERF);
    {
        char v[PV_LEN];
        const char *u;
        param_format(&TP[P_LEVEL], trk[0].p[P_LEVEL], v, &u);
        printf("perflevel:   MIXER LEVEL 1 now shows \"%s%s\"\n", v, u ? u : "");
        param_format(&TP[P_LEVEL], trk[1].p[P_LEVEL], v, &u);
        printf("perflevel:   MIXER LEVEL 2 now shows \"%s%s\"\n", v, u ? u : "");
        expect("  the MIXER's LEVELs are the moved values (one detent to the right: the formula at p = -1/32): what the MIXER page and the editor show", lv_ref(112, 106, -1));
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    t_audio();                                          /* (first: each child from the process's power-on state) */
#ifndef PL_RECORD
    t_rule();
#endif
    printf("perflevel: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
