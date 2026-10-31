/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* DUELL 1.1 PERFORM POINTS BUDGET (firmware/src/perform.c PF_BUDGET / PF_W; DUELL/release_drafts/PERFORM_CPU_V1.md option B16): the effects held
 * together weigh 16 points at most; a press over it releases the OLDEST costly effect held. Built on perform_test.c's harness (its main left out):
 *   HELD      FLANGER + DRIVE + PUMP (16) and OCT UP + PHASER (16) are all kept; ROLL A / B, MUTE 1 / 2 weigh 0 and never release anything
 *   RELEASE   OCT UP + PHASER + PUMP (19): OCT UP (the oldest) is released, PHASER + PUMP stay; the order of release over a longer press series
 *   GROUP     the buffer group is one slot (the last pressed plays): two buffer effects held weigh the one that runs, a third press releases by that
 *   FADE      the stages still ramping out (the 2.9 ms) count: at no block do the running + ramping stages weigh more than the budget, and the
 *             new effect starts after the ramp (it does run)
 *   COST      the song's instructions a sample with FLANGER + DRIVE + PUMP held again (should be at or below the old 16 points, about 12 % device)
 * Host simulation only. Built and run by host_tests.sh (name: performcap). */
#define PERFORM_NO_MAIN 1
#include "perform_test.c"

static int ck(const char *what, int ok) { return expect(what, ok); }
static uint32_t held_now(void) { return perf_held; }
static uint32_t B(uint32_t e) { return PF_BIT(e); }

/* the points of the stages sounding now, from the stages' own state (not perf_cost): a stage counts while its key is down or its share is not 0 */
static uint32_t sounding_points(void)
{
    uint32_t c = 0;
    if ((pf.act & B(PF_FLG)) || pf.fw) c += PF_W[PF_FLG];
    if ((pf.act & B(PF_PHS)) || pf.pw) c += PF_W[PF_PHS];
    if ((pf.act & B(PF_DRV)) || pf.dw) c += PF_W[PF_DRV];
    if ((pf.act & B(PF_PUMP)) || pf.pph || pf.pdn) c += PF_W[PF_PUMP];
    if (pf.mode != BM_NONE) c += perf_bw;           /* the buffer: one at a time */
    return c;
}
static uint32_t pk_max, pk_blocks_over;
static void watch_at(uint32_t t)
{
    uint32_t p;
    (void)t;
    p = sounding_points();
    if (p > pk_max) pk_max = p;
    if (p > PF_BUDGET) pk_blocks_over++;
}

static void cap_reset(void)
{
    host_tracks_init();
    perf_reset();
    perf_rel = 0;
    perf_bw = 0;
}
static void down(uint32_t e) { perf_press(e, 1); }
static void up(uint32_t e) { perf_press(e, 0); }

static void t_cap_state(void)
{
    cap_reset();
    down(PF_FLG); down(PF_DRV); down(PF_PUMP);
    ck("HELD: FLANGER + DRIVE + PUMP (16 points) all kept", held_now() == (B(PF_FLG) | B(PF_DRV) | B(PF_PUMP)) && !perf_rel);
    cap_reset();
    down(PF_OUP); down(PF_PHS);
    ck("HELD: OCT UP + PHASER (16 points) both kept", held_now() == (B(PF_OUP) | B(PF_PHS)) && !perf_rel);
    down(PF_PUMP);
    ck("RELEASE: + PUMP (19 points) releases OCT UP, the oldest; PHASER + PUMP stay", held_now() == (B(PF_PHS) | B(PF_PUMP)) && perf_rel == B(PF_OUP));
    up(PF_OUP);
    ck("  .. the released key let go: nothing left over, its mark cleared", held_now() == (B(PF_PHS) | B(PF_PUMP)) && !perf_rel);
    cap_reset();
    down(PF_M1); down(PF_M2); down(PF_ROLLA); down(PF_ROLLB); down(PF_OUP); down(PF_PHS);
    ck("HELD: MUTE 1 / 2, ROLL A / B weigh 0: with OCT UP + PHASER nothing is released", held_now() == (B(PF_M1) | B(PF_M2) | B(PF_OUP) | B(PF_PHS)) && !perf_rel);

    cap_reset();   /* release order over a series: FLANGER, PHASER, DRIVE (21: FLANGER goes), PUMP (17: PHASER), OCT UP (17: DRIVE) */
    down(PF_FLG); down(PF_PHS); down(PF_DRV);
    ck("ORDER: FLANGER, PHASER, DRIVE (21 points): FLANGER, the oldest, released", held_now() == (B(PF_PHS) | B(PF_DRV)) && perf_rel == B(PF_FLG));
    down(PF_PUMP);
    ck("  .. + PUMP (17): PHASER, now the oldest, released", held_now() == (B(PF_DRV) | B(PF_PUMP)) && perf_rel == (B(PF_FLG) | B(PF_PHS)));
    down(PF_OUP);
    ck("  .. + OCT UP (17): DRIVE released; PUMP + OCT UP stay", held_now() == (B(PF_PUMP) | B(PF_OUP)) && perf_rel == (B(PF_FLG) | B(PF_PHS) | B(PF_DRV)));
    up(PF_FLG); up(PF_PHS); up(PF_DRV); up(PF_PUMP); up(PF_OUP);
    ck("  .. every key let go: all clear", !held_now() && !perf_rel);

    cap_reset();   /* the buffer group is one slot */
    down(PF_PHS); down(PF_R16); down(PF_OUP);
    ck("GROUP: PHASER + REPEAT 1/16 + OCT UP held: the group counts once (OCT UP, 8): 16 points, nothing released", held_now() == (B(PF_PHS) | B(PF_R16) | B(PF_OUP)) && !perf_rel);
    down(PF_PUMP);
    ck("  .. + PUMP (19): PHASER, pressed before the group's last, is the oldest costly slot and goes", held_now() == (B(PF_R16) | B(PF_OUP) | B(PF_PUMP)) && perf_rel == B(PF_PHS));
    cap_reset();
    down(PF_OUP); down(PF_PHS); down(PF_R8);
    ck("  .. REPEAT 1/8 (5) pressed over OCT UP + PHASER: the group is now REPEAT (5): 13 points, all kept", held_now() == (B(PF_OUP) | B(PF_PHS) | B(PF_R8)) && !perf_rel);
    down(PF_FLG);
    ck("  .. + FLANGER (20): the group counts as REPEAT 1/8, the last pressed (newer than PHASER): PHASER goes, the shadowed OCT UP key stays held", held_now() == (B(PF_OUP) | B(PF_R8) | B(PF_FLG)) && perf_rel == B(PF_PHS));
    cap_reset();
    down(PF_OUP); down(PF_PHS); down(PF_ODN);
    ck("  .. OCT UP then OCT DN over PHASER: one slot (8), 16 points, kept", held_now() == (B(PF_OUP) | B(PF_PHS) | B(PF_ODN)) && !perf_rel);
}

static void t_cap_fade(void)
{
    static const ev_t ev_a[] = {{0, PF_FLG + 1}, {2 * 128, PF_PHS + 1}, {14000, PF_OUP + 1}};
    static const ev_t ev_b[] = {{0, PF_FLG + 1}, {2 * 128, PF_DRV + 1}, {4 * 128, PF_PUMP + 1}, {14000, PF_PHS + 1}, {14000 + 128 * 3, PF_OUP + 1}, {40000, PF_FLG + 1}};
    char what[160];
    test_signal(300, 8000, 410, 8000);
    pk_max = pk_blocks_over = 0;
    run_at = watch_at;
    run(120, 1, ev_a, 3, 30000);
    run_at = 0;
    snprintf(what, sizeof what, "FADE: FLANGER + PHASER, then OCT UP (23): FLANGER released, ramping + running never over %u points (peak %u)", PF_BUDGET, pk_max);
    ck(what, pk_max <= PF_BUDGET && !pk_blocks_over);
    ck("  .. the released FLANGER is gone and OCT UP + PHASER run after the ramp", !(perf_act & B(PF_FLG)) && !pf.fw && (perf_act & B(PF_PHS)) && pf.mode == BM_HARM);
    ck("  .. the cap released FLANGER, not PHASER", (perf_held & B(PF_FLG)) == 0 && (perf_held & B(PF_PHS)) != 0);
    pk_max = pk_blocks_over = 0;
    run_at = watch_at;
    run(120, 1, ev_b, 6, 60000);
    run_at = 0;
    snprintf(what, sizeof what, "  .. a longer series with presses a block apart: still never over %u points (peak %u)", PF_BUDGET, pk_max);
    ck(what, pk_max <= PF_BUDGET && !pk_blocks_over);
}

static void t_cap_cost(void)
{
    static const int HEARD[] = {PF_FLG, PF_DRV, PF_PUMP}, PAIR[] = {PF_OUP, PF_PHS}, OVER[] = {PF_OUP, PF_PHS, PF_PUMP};
    double idle, heard, pair, over;
    char what[200];
    cost_mode = DM_STEREO;
    idle = cost_run(0, 0, 100);
    heard = cost_run(HEARD, 3, 100);
    pair = cost_run(PAIR, 2, 100);
    over = cost_run(OVER, 3, 100);
    if (!idle) {
        printf("perform: cap cost: no instruction counter on this host\n");
        return;
    }
    snprintf(what, sizeof what, "COST: the song %.0f idle; FLANGER + DRIVE + PUMP held +%.0f (device ~%.1f %%), OCT UP + PHASER +%.0f, OCT UP + PHASER + PUMP +%.0f (PHASER + PUMP after the release)",
             idle, heard - idle, (heard - idle) * 0.017, pair - idle, over - idle);
    ck(what, (heard - idle) * 0.017 <= 12.0 && (pair - idle) * 0.017 <= 14.5 && over < pair);
}

int main(void)
{
    t_cap_state();
    t_cap_fade();
    t_cap_cost();
    printf("%s (%d checks)\n", bad ? "PERFORM CAP TEST FAILED" : "perform cap test passed", ncheck);
    return bad != 0;
}
