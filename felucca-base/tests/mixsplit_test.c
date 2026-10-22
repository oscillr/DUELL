/* SPDX-License-Identifier: GPL-3.0-only */
/* MIXSPLIT (job 1 of the second-core work): fx.c's mix_block is a chain of stages (mix_events, per part part_voice / part_rest / part_fx /
 * part_mix / part_end, mix_sends, mix_master), the refactor of the code that was inline. This test renders a few scenes three ways, each in a
 * child process from the power-on state (the DSP has no reset), and compares every output sample:
 *   REF      the old combined path: mix_part and mix_block as they were before the refactor (tests/mixsplit_ref.h, a verbatim copy)
 *   FIRM     the firmware's mix_block as it is now (the audio ISR's call)
 *   STAGED   the stage functions called in sequence from this file (what a second core would call: the order is the contract)
 * All three must be bit for bit equal (a 64-bit hash of every output sample and of each part's peak after every block, and the sample-by-sample
 * compare through a pipe of the hashes). The scenes are chosen to run every branch of the stages: STEREO with SPREAD, a matrix PAN, an INSERT and
 * every send; DUEL (SPLIT) with EQ, SAT, the PUMP duck and BALANCE; STEREO with DUCK SRC PART, EQ, SAT, a PAN step (the gain ramp), BASS MONO,
 * WIDTH, COMP, a part going silent (the tail runs out: part_rest); LAYER with PERFORM's MUTE and DRIVE held, PART FX and SENDS turned OFF in the
 * middle (the crossfades), SPACE to the right and a MODE change (the dip). Also checked: no scene is silent (it would compare nothing).
 * Built and run by DUELL/host_tests.sh (name: mixsplit) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "mode_scn.h"                                   /* (scn_eng) */
#include "mixsplit_ref.h"
#include <unistd.h>
#include <sys/wait.h>

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    if (!ok) {
        printf("mixsplit: FAIL %s\n", what);
        bad++;
    }
    return ok;
}

/* the stages in sequence, as mix_block calls them (mix_part's order is part_voice, then part_rest, or part_fx, part_mix, part_end) */
static void staged_part(track_t *t, uint32_t n, const blockstate_t *bs)
{
    mixpart_t m;
    if (!part_voice(&m, t, n, bs)) {
        part_rest(&m);
        return;
    }
    part_fx(&m);
    part_mix(&m);
    part_end(&m);
}
static void staged_block(int32_t *out, uint32_t n)
{
    uint32_t i;
    int perf;
    blockstate_t bs;
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
    for (i = 0; i < NLIVE; i++) {
        int mo = morph_begin(&trk[i], 1);
        staged_part(&trk[i], n, &bs);
        if (mo)
            morph_end(&trk[i]);
    }
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

typedef void (*blk_t)(int32_t *, uint32_t);
#define CFG(t, r, a, l) ((uint32_t)(t) | (uint32_t)(r) << 8 | (uint32_t)(a) << 16 | (uint32_t)(l) << 24)
#define MS(x) ((uint32_t)(x) * FS / 1000u / CTL)         /* a time in ms as blocks */
#define NSCENE 5
static const char *const SCN[NSCENE] = {"STEREO busy (SPREAD, matrix PAN, INSERT, HALL)", "DUEL: EQ, SAT, PUMP, BALANCE",
                                        "STEREO: DUCK SRC PART, PAN step, master stages, a silent tail", "LAYER: MUTE, DRIVE, PART FX OFF, SENDS OFF, SPACE, MODE change",
                                        "DUEL: SPREAD, DUCK SRC PART, BALANCE, SPACE, a silent part"};
static uint64_t hsh;
static int32_t peakmax;
static void hfeed(uint32_t v)
{
    hsh ^= v;
    hsh *= 0x100000001B3ull;
}
/* one scene, all blocks through blk; returns the hash (0 when silent) */
static uint64_t scene(uint32_t id, blk_t blk)
{
    static const uint8_t C1[4] = {48, 55, 60, 64}, C2[3] = {67, 72, 76};
    static int32_t o[2 * CTL];
    uint32_t k, i, b, nb = (id ? 2u : 3u) * FS / CTL;
    hsh = 0xCBF29CE484222325ull;
    peakmax = 0;
    host_tracks_init();
    host_preset(&trk[0], scn_eng("ANALOG"), 1);
    host_preset(&trk[1], scn_eng("PHASE"), 2);
    song.g[G_RTYPE] = id == 3u ? 1 : 2;
    song.g[G_RLOCUT] = 0;
    song.g[G_DFDBK] = 90;
    song.g[G_DMIX] = 110;
    song.g[G_CDEPTH] = 100;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_SPRD] = (int16_t)(id == 0u || id == 4u ? 100 : 0);
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
    if (id == 1u || id == 4u) {                         /* DUEL */
        duell_mode_force(DM_SPLIT);
        if (!dm_split)
            return 0;
    }
    if (id == 1u) {
        mix_bal = 20;
        for (k = 0; k < 2u; k++) {
            trk[k].p[P_EQLG] = 20; trk[k].p[P_EQMG] = -20; trk[k].p[P_EQHG] = 20;
            trk[k].p[P_DKSRC] = 0; trk[k].p[P_DKDEP] = 80; trk[k].p[P_DKATK] = 10; trk[k].p[P_DKRLS] = 40; trk[k].p[P_DKRATE] = 64;
        }
    }
    if (id == 2u) {
        trk[1].p[P_DKSRC] = 1; trk[1].p[P_DKDEP] = 90; trk[1].p[P_DKTHR] = 10; trk[1].p[P_DKATK] = 10; trk[1].p[P_DKRLS] = 40;
        trk[0].p[P_DKSRC] = 3;                          /* (job 3: DUCK SRC PART on one part only: part 1 keys, it has no ducker) */
        trk[0].p[P_EQLG] = 15; trk[0].p[P_EQMG] = -15; trk[0].p[P_EQHG] = 10;
        trk[1].p[P_DIST] = 60;
        trk[0].p[P_REL] = 10;
        mix_bal = -30;
    }
    if (id == 3u) {
        duell_mode_force(DM_LAYER);
        trk[0].p[P_SPRD] = 60;
        trk[0].p[P_EQLG] = 12; trk[0].p[P_EQMG] = -12; trk[0].p[P_EQHG] = 12;
        trk[1].p[P_EQLG] = 12; trk[1].p[P_EQMG] = -12; trk[1].p[P_EQHG] = 12;
    }
    if (id == 4u)
        trk[0].p[P_SPRD] = 80;
    if (id == 2u || id == 4u) {                         /* the matrix on a bus value (mod_bus): part 2's LFO -> REVERB SIZE */
        trk[1].p[P_M2SRC] = MS_LFO; trk[1].p[P_M2DST] = MD_RSIZE; trk[1].p[P_M2AMT] = 40;
    }
    if (id == 2u)
        song.g[G_DMODE] = DMD_PING;                     /* PING-PONG: the repeats left and right */
    for (b = 0; b < nb; b++) {
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
        if (id == 2u) {
            if (b == MS(300)) { trk[0].p[P_PAN] = 50; trk[1].p[P_DIST] = 110; }               /* the PAN ramp, the SAT slew */
            if (b == MS(400)) { bm_sel = 7; wd_sel = 170; cmp_cfg = CFG(23, 4, 12, 6); }        /* BASS MONO, WIDTH, COMP */
            if (b == nb / 2u + 62u) trk[1].p[P_ITYPE] = 0;                                      /* TYPE OFF at the end of the tail (ins_rest) */
            if (b == nb / 2u + MS(300)) trk[1].p[P_PAN] = -20;                                  /* a silent part: its pan taken at once */
            scope_mix = 1;
        }
        if (id == 3u) {
            if (b == MS(100)) perf_held = PF_BIT(PF_M1) | PF_BIT(PF_DRV);                       /* MUTE 1 and DRIVE held */
            if (b == MS(200)) perf_held = PF_BIT(PF_M2);
            if (b == MS(300)) perf_held = 0;
            if (b == MS(400)) mods_off |= MODS_PFX;                                             /* PART FX OFF: the crossfade .. */
            if (b == MS(400) + 3u) perf_held = PF_BIT(PF_M2);                                   /* .. with a MUTE in it (pfx_mute) */
            if (b == MS(450)) perf_held = 0;
            if (b == MS(600)) mods_off &= (uint8_t)~MODS_PFX;
            if (b == MS(700)) perf_space = 60;                                                  /* SPACE right of the middle */
            if (b == MS(800)) mods_off |= MODS_SENDS;                                           /* SENDS OFF: the fade */
            if (b == MS(900)) { duell_mode = DM_SPLIT; duell_mode_req = DM_SPLIT; }             /* the MODE's dip, STEREO / LAYER -> DUEL */
            if (b == nb / 2u + MS(300)) perf_held = PF_BIT(PF_M1);                              /* a MUTE held on a silent part (perf_mute_rest) */
        }
        if (id == 4u && b == MS(300)) { trk[1].p[P_DKSRC] = 1; trk[1].p[P_DKDEP] = 70; mix_bal = -40; }
        if (id == 4u && b == MS(500)) perf_space = 60;                                          /* SPACE right of the middle in DUEL (bus A's floors) */
        if (id == 1u || id == 4u)
            meter_on = 3;                                                                       /* GLO's meters watching */
        blk(o, CTL);
        for (i = 0; i < 2u * CTL; i++) {
            int32_t a = o[i] < 0 ? -o[i] : o[i];
            peakmax = a > peakmax ? a : peakmax;
            hfeed((uint32_t)o[i]);
        }
        hfeed((uint32_t)trk[0].peak);
        hfeed((uint32_t)trk[1].peak);
    }
    return peakmax > 1000 ? hsh : 0;
}
/* a scene in a child process (the DSP's own power-on state), its hash read through a pipe; 0 on any failure */
static uint64_t scene_run(uint32_t id, blk_t blk)
{
    int fd[2];
    pid_t pid;
    uint64_t h = 0;
    int st = 0;
    fflush(stdout);
    if (pipe(fd))
        return 0;
    pid = fork();
    if (pid == 0) {
        uint64_t r = scene(id, blk);
        ssize_t w = write(fd[1], &r, sizeof r);
        _exit(w == (ssize_t)sizeof r ? 0 : 3);
    }
    close(fd[1]);
    if (read(fd[0], &h, sizeof h) != (ssize_t)sizeof h)
        h = 0;
    close(fd[0]);
    waitpid(pid, &st, 0);
    return WIFEXITED(st) && WEXITSTATUS(st) == 0 ? h : 0;
}

int main(void)
{
    uint32_t s;
    for (s = 0; s < NSCENE; s++) {
        uint64_t hr = scene_run(s, ref_mix_block), hf = scene_run(s, mix_block), hs = scene_run(s, staged_block);
        char what[160];
        printf("mixsplit: scene %u %-62s ref %016llx firm %016llx staged %016llx\n", s, SCN[s], (unsigned long long)hr, (unsigned long long)hf, (unsigned long long)hs);
        snprintf(what, sizeof what, "scene %u (%s) is not silent", s, SCN[s]);
        expect(what, hr != 0);
        snprintf(what, sizeof what, "scene %u: mix_block equals the old combined path", s);
        expect(what, hf == hr);
        snprintf(what, sizeof what, "scene %u: the stages in sequence equal the old combined path", s);
        expect(what, hs == hr);
    }
    printf("mixsplit: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
