/* SPDX-License-Identifier: GPL-3.0-only */
/* MIXSPLIT job 2 (partstate): the two parts' stages are independent of each other. The per-part stages of fx.c (part_voice, part_rest / part_fx,
 * part_mix + part_end, and the morph window around them) are run by this file in several schedules, each scene from power-on in its own process:
 *   A  part 1 whole, then part 2 whole (the old order, what mix_block does)     B  part 2 whole, then part 1 whole (reversed)
 *   C  stage by stage, 1 then 2:  V1 V2 F1 F2 M1 M2 E1 E2                       D  stage by stage, 2 then 1:  V2 V1 F2 F1 M2 M1 E2 E1
 *   E  V1 V2 F2 F1 M1 M2 E2 E1 (mixed)                                           F  V2 V1 F1 F2 M2 M1 E1 E2 (mixed)
 *   (V voice stage incl. morph_begin, F effects (or the silent way out), M mix + part_end, E morph_end; after them mix_gather, mix_sends, mix_master)
 * The interleaved schedules C..F are what two cores could do: every stage of one part sees the other part's stage before it, or after it. All six
 * must give the same output bit for bit (a hash of every output sample and of each part's peak), equal to
 *   REF     the old combined path (tests/mixsplit_ref.h: mix_part / mix_block with the shared accumulators)
 *   FIRM    the firmware's mix_block
 *   OLD     the hashes of the commit before the partstate work (job 1, d524422), from its verbatim mixsplit_ref.h: they pin REF itself
 * The scenes (tests/partindep_scn.h) run STEREO, DUEL and LAYER with SPREAD, EQ, SAT, a PART FX crossfade, a PAN step, MUTE, a MODE change, silent
 * parts, MORPH on both parts and the matrix's bus offsets of both parts. They use neither DUCK SRC PART nor the FORMANT sound: those two make a
 * part depend on the other one's order (the job 3 blockers, marked TWO-CORE BLOCKER in fx.c duck_part and eng_formant.c).
 * Scene 6 (both parts' LFO on S&H, the same rate, so they wrap in the same blocks) was the third blocker, found by this test: the S&H of LFO 1 of parts 1 and 2 was
 * drawn from ONE generator (voice.c lfo_rand, rng_state) in the voice stage, so the draws followed the order the parts ran. Job 3: part 2 has a generator of its own
 * (dsp.c part_rand), part 1 keeps the shared one; the scene is a passing scene now, its job 1 hash differs (OLD_HASH[6] is the new one), the other five are job 1's.
 * The self-check is in host_tests.sh (partindep): the same test is built against a copy of the firmware with ONE shared write put back (the block
 * scratch, the morph window or the matrix window of the parts made one), and must FAIL.
 * Built and run by DUELL/host_tests.sh (name: partindep) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "mode_scn.h"                                   /* (scn_eng) */
#include "mixsplit_ref.h"
#include <unistd.h>
#include <sys/wait.h>
#include "partindep_scn.h"

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    if (!ok) {
        printf("partindep: FAIL %s\n", what);
        bad++;
    }
    return ok;
}

/* the hashes of the old combined path (the scenes of partindep_scn.h). Not to be regenerated: a difference is a change of the sound.
 * join0233 (the T0229 sound line joined to the two-core line): scenes 0..5 are the hashes of wip/editor0232's own mix_block (T0229, one core, run by
 * the same scenes on that line's tree: the joined line must sound exactly as T0229 there; they differ from job 1's, which were
 * 0xb0a8c23a1b9a79d6 0x7c4e44c1e0b1e8c2 (2: unchanged) 0x979dc4e72cd132e6 0x570b95caf83aee89 0x941749b31097241f, because T0229 changed the sound:
 * the matrix's source slew, the mix and SAT ramps, the HALL tail, the SAT follower). Scenes 6, 7, 8 (S&H LFO of both parts, the VOICE engine's FORMANT noise,
 * one-sided DUCK SRC PART) need both lines (a generator per part and the one-sided duck are job 3's; T0229's changes act on them too): pinned at the join,
 * not provable on either line alone (job 3's own pins were 0x55425a340f33a3fb 0x22458dd549d92db4 0xe9b61d8bc6534f59).
 * hunt1 (release_drafts/CLICK_HUNT1.md), on purpose: scene 5 (MORPH on both parts, the fader jumping every 200 ms) was 0xfe89a66bd64ff6bd: the fader's steps of the sends,
 * PATCH LEVEL and PAN are now ramped inside the block (mod.c mod_mixmv counts the open morph window); the largest difference to the render before is 3,211 of a peak of 30,221 at
 * the first fader jump (0.219 s), a step of the old render.
 * hunt1, on purpose, the SPREAD mix (fx.c spread_run / spread_post: the gains of a SPREAD part in a line inside the block while a matrix route or the MORPH window moves them, SAT's
 * blend ramp): scene 0 was 0xabe8df4fd946cf01 (largest difference to the old render 995 of a peak of 19,305), scene 2 0x6ffaaf71bc8eca2c (261 of 18,252), scene 3
 * 0x04da56e70cc2b28b (1,861 of 20,685), scene 4 0x173a1347e4264436 (3,201 of 20,192): the old render stepped the gains at every block (the stored PAN's follower alone is 11 % of a channel
 * a block for a pan step of 55). Scenes 1, 5, 6, 7, 8 (no SPREAD part with a moving gain, or the plain mix) are unchanged.
 * hunt1, on purpose, LFO 1 and LFO 2 for their own depths glide at most MX_SLEW a block (voice.c track_render): scene 3 was 0x04da56e70cc2b28b before F2 and 0xb047bc25045d3897 after it,
 * scene 5 0x19ca5e538a668b32, scene 6 0xaf557e1adf2c7c71 (the S&H LFO of both parts: its steps glide). Scenes 3 and 5 move the fader (the open morph window cross-fades the LFOs' waves and
 * restarts them), 6 has the S&H steps; 0, 2, 4, 7, 8 never clamp a used LFO and are unchanged. After a pitch step that comes a few ms later the phase differs, so the samples differ by up
 * to the peak from there on (not a click).
 * pedal hunt, on purpose: the ducker keeps 2 more bits (fx.c duck_run: scenes 1, 5, 8 have a PUMP / PART ducker) and PHASE slews a jumping DCW (scene 3's PHASE part): scenes 1, 3, 5, 8 re-made.
 * final line (2026-10-21), on purpose: re-based to the merged line (cloud wave 6 engines / chain / filters + the click fixes of hunt1..hunt4): all scenes but 7 and 9 re-made; the paths still equal each other bit for bit in every scene.
 * hunt4, on purpose: PHASE's moving DCW no longer steps at the block seams (eng_phase.c phase_render_0; part 2 plays a PHASE preset in every scene but 7, which is VOICE, and 4, where it is silent): scenes 0, 1, 2, 3, 5, 6, 8 re-made. Were: 0x30990c9d0364261d, 0xe12760713fbd74c9, 0xac583d0d32b8789e, 0x68e070f9f3a10cc9, -, 0x73bd71f5bd2850ae, 0x5e8c1fa40e774a01, -, 0x1dba4f8a604d00e7. */
/* 0.2.32, on purpose: scenes 3, 5, 6 re-made (the direct depth cards reach 100 % at +63: DEP_AMT in voice.c / chain_f2.c; with DEP_AMT(a) = (a) the old pins hold). */
/* 1.0 (job F1), on purpose: scenes 0, 2, 3, 5, 6, 7, 8, 9, 11 re-made (TWIN OFFSET in whole semitones, the matrix amount +63 acts as 64). */
static const uint64_t OLD_HASH[NSCENE] = {
    0xabba23f00aee3ed6ull, 0x7fa9bda50c85b0dbull, 0xdcff3a6b5145f5ecull, 0x80a2f6749bf5f4e8ull, 0x1944ec41f483add5ull, 0x2af9f9ff2df82c86ull, 0x7c45166298417266ull, 0xdcc9597a423fea0dull, 0xa96361ed8013e0a3ull,
    0xa5f0c865fc37f4d4ull, 0x77259f9e9e14f727ull, 0xe2610882d86b2590ull};   /* 0.2.31: re-based on purpose (SUB HARMONICS is disabled: the scenes' HARM values play as 0), were 0x57241fc736950a76, 0xae179d4cb7f62068, 0x653c0c9666564fdf. Scenes 9, 10, 11 (cloud job F: SUB, CYCLE, STEPS) have no commit before: pinned at the commit that made them, from the old combined path (the feature is in track_render, which it shares) */

/* ---- the schedules */
enum { SV, SF, SM, SE };                                /* the steps: voice (+ morph_begin), effects / silent way out, mix + end, morph_end */
typedef struct { uint8_t step, part; } st_t;
#define S(s, p) {s, p}
static const st_t SCHED[6][8] = {
    {S(SV, 0), S(SF, 0), S(SM, 0), S(SE, 0), S(SV, 1), S(SF, 1), S(SM, 1), S(SE, 1)},   /* A */
    {S(SV, 1), S(SF, 1), S(SM, 1), S(SE, 1), S(SV, 0), S(SF, 0), S(SM, 0), S(SE, 0)},   /* B */
    {S(SV, 0), S(SV, 1), S(SF, 0), S(SF, 1), S(SM, 0), S(SM, 1), S(SE, 0), S(SE, 1)},   /* C */
    {S(SV, 1), S(SV, 0), S(SF, 1), S(SF, 0), S(SM, 1), S(SM, 0), S(SE, 1), S(SE, 0)},   /* D */
    {S(SV, 0), S(SV, 1), S(SF, 1), S(SF, 0), S(SM, 0), S(SM, 1), S(SE, 1), S(SE, 0)},   /* E */
    {S(SV, 1), S(SV, 0), S(SF, 0), S(SF, 1), S(SM, 1), S(SM, 0), S(SE, 0), S(SE, 1)}};  /* F */
static const char SNAME[6] = {'A', 'B', 'C', 'D', 'E', 'F'};
static uint32_t cur;                                    /* the schedule the next scene runs */

static blockstate_t bs;
static mixpart_t pm[NLIVE];
static int alive[NLIVE], mo[NLIVE];
static void do_step(const st_t *s, uint32_t n)
{
    uint32_t p = s->part;
    track_t *t = &trk[p];
    switch (s->step) {
    case SV:
        mo[p] = morph_begin(t, 1);
        alive[p] = part_voice(&pm[p], t, n, &bs);
        break;
    case SF:
        if (alive[p])
            part_fx(&pm[p]);
        else
            part_rest(&pm[p]);
        break;
    case SM:
        if (alive[p]) {
            part_mix(&pm[p]);
            part_end(&pm[p]);
        }
        break;
    default:
        if (mo[p])
            morph_end(t);
    }
}
static void sched_block(int32_t *out, uint32_t n)
{
    uint32_t i;
    int perf;
    int32_t mg = MIX_MG();
    mix_events(n);
    sp_tap = snd_st != SND_OFF && (perf_space > 0 || sv.spc > 0);
    if (perf_idle()) {
        perf_tick(n);
        perf = 0;
    } else {
        perf = perf_begin(n);
    }
    blk_snap(&bs, sp_tap);
    for (i = 0; i < 8u; i++)
        do_step(&SCHED[cur][i], n);
    mix_gather(n, &bs);
    mix_sends(n);
    if (meter_on) {
        meter_on--;
        if (dm_split)
            meter_scan(n);
    }
    if (scope_mix)
        scope_take_mix(n);
    mix_master(out, n, perf, mg);
}

/* a scene by a schedule, in its own process */
static uint64_t sched_run(uint32_t id, uint32_t sc)
{
    cur = sc;
    return scene_run(id, sched_block);
}

/* the scenes must exercise what they claim: each scene runs once more through mix_block with a probe that records, after every block, which of the paths
 * below have been taken (the bits), and the scene must have taken all of those it names in NEED */
enum { PB_MORPH0, PB_MORPH1, PB_MXF, PB_GON0, PB_GON1, PB_SPREAD0, PB_SPREAD1, PB_PFXFADE, PB_SILENT0, PB_SILENT1, PB_MUTE, PB_SPLIT, PB_SPACE, PB_EQ0, PB_EQ1, PB_DUCK };
#define PB(b) (1u << PB_##b)
static uint32_t pflags;
static void probe_block(int32_t *out, uint32_t n)
{
    uint32_t p, act[2] = {0, 0}, i;
    mix_block(out, n);
    for (p = 0; p < 2u; p++)
        for (i = 0; i < NVOICE; i++)
            act[p] |= trk[p].v[i].active;
    pflags |= (mph[0].nid || mph[0].fnid ? PB(MORPH0) : 0) | (mph[1].nid || mph[1].fnid ? PB(MORPH1) : 0) | (mph[0].cxf.on || mph[1].cxf.on ? PB(MXF) : 0) |
              (trk[0].mgon ? PB(GON0) : 0) | (trk[1].mgon ? PB(GON1) : 0) | (pscr[0].spread_on ? PB(SPREAD0) : 0) | (pscr[1].spread_on ? PB(SPREAD1) : 0) |
              ((pfx_x[0] > 0 && pfx_x[0] < 32768) || (pfx_x[1] > 0 && pfx_x[1] < 32768) ? PB(PFXFADE) : 0) |
              (!act[0] && !trk[0].tail ? PB(SILENT0) : 0) | (!act[1] && !trk[1].tail ? PB(SILENT1) : 0) | (pf.mute ? PB(MUTE) : 0) |
              (dm_split ? PB(SPLIT) : 0) | (sp_tap ? PB(SPACE) : 0) | (trk[0].eqlv ? PB(EQ0) : 0) | (trk[1].eqlv ? PB(EQ1) : 0) | (duck_st[1].dn > 0 ? PB(DUCK) : 0);
}
static const uint32_t NEED[NSCENE] = {
    PB(GON0) | PB(GON1) | PB(SPREAD0) | PB(PFXFADE) | PB(SILENT1) | PB(EQ0) | PB(EQ1),
    PB(SPLIT) | PB(PFXFADE) | PB(MUTE) | PB(SPACE) | PB(SILENT1) | PB(EQ0) | PB(EQ1),
    PB(SPREAD0) | PB(SPREAD1) | PB(MUTE) | PB(PFXFADE) | PB(SPACE) | PB(EQ0) | PB(EQ1),
    PB(MORPH0) | PB(MORPH1) | PB(MXF) | PB(GON0) | PB(GON1) | PB(EQ0) | PB(EQ1),
    PB(MORPH0) | PB(MORPH1) | PB(SPREAD0) | PB(PFXFADE) | PB(MUTE) | PB(SILENT1) | PB(EQ0),
    PB(SPLIT) | PB(MORPH0) | PB(MORPH1) | PB(GON0) | PB(GON1),
    0,
    0,
    PB(DUCK) | PB(EQ0),
    0,
    PB(SPLIT),
    PB(SPREAD0)};
static uint32_t probe(uint32_t id)
{
    int fd[2];
    pid_t pid;
    uint32_t r = 0;
    int st = 0;
    fflush(stdout);
    if (pipe(fd))
        return 0;
    pid = fork();
    if (pid == 0) {
        ssize_t w;
        pflags = 0;
        scene(id, probe_block);
        w = write(fd[1], &pflags, sizeof pflags);
        _exit(w == (ssize_t)sizeof pflags ? 0 : 3);
    }
    close(fd[1]);
    if (read(fd[0], &r, sizeof r) != (ssize_t)sizeof r)
        r = 0;
    close(fd[0]);
    waitpid(pid, &st, 0);
    return r;
}

int main(void)
{
    uint32_t s, k, got;
    for (s = 0; s < NSCENE; s++) {
        uint64_t hr = scene_run(s, ref_mix_block), hf = scene_run(s, mix_block), hs[6];
        char what[200];
        for (k = 0; k < 6u; k++)
            hs[k] = sched_run(s, k);
        printf("partindep: scene %u %-82s ref %016llx firm %016llx A %016llx\n", s, SCN[s], (unsigned long long)hr, (unsigned long long)hf, (unsigned long long)hs[0]);
        snprintf(what, sizeof what, "scene %u (%s) is not silent", s, SCN[s]);
        expect(what, hr != 0);
        got = probe(s);
        snprintf(what, sizeof what, "scene %u takes the paths it names (have %05x, need %05x)", s, got, NEED[s]);
        expect(what, (got & NEED[s]) == NEED[s]);
        snprintf(what, sizeof what, "scene %u: the old combined path equals the hash of job 1 (the commit before partstate)", s);
        expect(what, hr == OLD_HASH[s] || getenv("SCN_TWIN"));
        snprintf(what, sizeof what, "scene %u: mix_block equals the old combined path", s);
        expect(what, hf == hr);
        for (k = 0; k < 6u; k++) {
            snprintf(what, sizeof what, "scene %u: schedule %c equals the old combined path (%016llx)", s, SNAME[k], (unsigned long long)hs[k]);
            expect(what, hs[k] == hr);
        }
    }
    printf("partindep: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
