/* SPDX-License-Identifier: GPL-3.0-only */
/* tests/partindep_test.c's scenes (MIXSPLIT job 2): the scene builder and the render loop, kept apart from the test so the same scenes can be run
 * through the old combined path of the commit before the partstate work (tests/mixsplit_ref.h as it was in job 1): the OLD_HASH table in
 * partindep_test.c comes from that run. Needs ui_test.c, mode_scn.h and a blk function (void (*)(int32_t *, uint32_t)) from the includer.
 * No scene uses DUCK SRC PART or the FORMANT sound (the two job-3 blockers: they make the parts depend on each other's order). */
#define CFG(t, r, a, l) ((uint32_t)(t) | (uint32_t)(r) << 8 | (uint32_t)(a) << 16 | (uint32_t)(l) << 24)
#define MS(x) ((uint32_t)(x) * FS / 1000u / CTL)         /* a time in ms as blocks */
#define NSCENE 12                                        /* scene 6: both parts' S&H LFO (job 3); scenes 9, 10, 11: SUB, CYCLE and STEPS (cloud job F) */
static const char *const SCN[NSCENE] = {
    "STEREO: SPREAD, matrix PAN + bus offsets both parts, INSERT, EQ, SAT, PAN step, tail",
    "DUEL: EQ, SAT, PUMP, BALANCE, PART FX crossfade, MUTE, SPACE, a silent part",
    "LAYER: SPREAD both, MUTE, DRIVE, PART FX OFF + MUTE, SENDS OFF, SPACE, MODE change",
    "STEREO: MORPH both parts (fader moves), matrix on both, EQ, SAT",
    "LAYER: MORPH + SPREAD + EQ + PART FX crossfade + MUTE + PAN step, silent part 2",
    "DUEL: MORPH both, matrix bus offsets, PUMP, SAT, EQ, MODE change to STEREO",
    "S&H LFO on both parts, equal rate (each part its own generator for the voice stage)",
    "STEREO: the VOICE engine (FORMANT noise) on both parts",
    "STEREO: DUCK SRC PART on part 2 only, part 1 keys (one-sided ducking), EQ, SAT",
    "STEREO: SUB (SQUARE + HARMONICS, SINE -2) on both parts, CYCLE into CUTOFF and SUB LEVEL, WAVE STEPS with an LFO on POS, UNISON on part 2",
    "DUEL: the same: SUB, CYCLE, STEPS on each part's side",
    "LAYER: the same, a steal and a LEGATO step in it"};
/* cloud job E: with SCN_TWIN in the environment both parts play with the TWIN FILTER on (a type, ROUTE, DRIVE, BLEND and the amounts that depend on the scene and the part): the same scenes then show that filter 2's
 * state is the part's own (partindep: every schedule; twocore: the second core). The old hashes (OLD_HASH) are those of the sounds without it */
static void scn_twin_setup(uint32_t id)
{
    uint32_t k;
    for (k = 0; k < 2u; k++) {
        track_t *t = &trk[k];
        uint32_t ty = 1u + (id + 2u * k) % 6u;
        t->p[P_TWTYPE] = (int16_t)ty;
        t->p[P_RES2] = (int16_t)(ty == TT_COMB ? 40 - (int32_t)k * 70 : 30 + (int32_t)id * 9);
        t->p[P_CUT2] = (int16_t)(ty == TT_COMB ? (int32_t)k * 7 - 3 : -(int32_t)(10 + id * 3u));
        t->p[P_DRV2] = (int16_t)(20 + id * 10u);
        t->p[P_BLEND] = (int16_t)((int32_t)((id + k) % 3u) * 20 - 20);
        XV(t, X_ROUTE) = (int16_t)((id + k) & 1u);
        XV(t, X_LINK) = (int16_t)(k == 1u && id == 4u ? 0 : 1);
        XV(t, X_KTRK2) = 20; XV(t, X_ENV2) = 30; XV(t, X_LFO1) = 15; XV(t, X_LFO2) = -10;
    }
}
static uint64_t hsh;
static int32_t peakmax;
static void hfeed(uint32_t v)
{
    hsh ^= v;
    hsh *= 0x100000001B3ull;
}
typedef void (*blk_t)(int32_t *, uint32_t);
/* one scene, all blocks through blk; returns the hash (0 when silent) */
static uint64_t scene(uint32_t id, blk_t blk)
{
    static const uint8_t C1[4] = {48, 55, 60, 64}, C2[3] = {67, 72, 76};
    static int32_t o[2 * CTL];
    uint32_t k, i, b, nb = (id == 0u || id == 3u ? 3u : 2u) * FS / CTL;
    hsh = 0xCBF29CE484222325ull;
    peakmax = 0;
    host_tracks_init();
    host_preset(&trk[0], scn_eng(id == 7u ? "VOICE" : "ANALOG"), 1);
    host_preset(&trk[1], scn_eng(id == 7u ? "VOICE" : "PHASE"), id == 7u ? 1 : 2);
    song.g[G_RTYPE] = id == 2u ? 1 : 2;
    song.g[G_RLOCUT] = 0;
    song.g[G_DFDBK] = 90;
    song.g[G_DMIX] = 110;
    song.g[G_CDEPTH] = 100;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_SPRD] = (int16_t)(id == 0u || id == 2u || id == 4u ? 100 : 0);
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
    if (id >= 9u) {                                     /* cloud job F: SUB, CYCLE and STEPS on both parts (part 2 on WAVE) */
        host_preset(&trk[1], ENGI_WAVE, 0);
        wave_build_now(&trk[1]);
        for (k = 0; k < 2u; k++) {
            track_t *t = &trk[k];
            t->p[P_TRIM] = 120; t->p[P_CHOR] = 100; t->p[P_DLY] = 90; t->p[P_REV] = 110; t->p[P_SUS] = 110; t->p[P_REL] = 70;
            XV(t, X_CYMODE) = (int16_t)k; XV(t, X_CYRISE) = (int16_t)(30 + 10 * k); XV(t, X_CYFALL) = (int16_t)(40 - 10 * k); XV(t, X_CYCURVE) = (int16_t)(k ? -30 : 25);
            t->p[P_M3SRC] = MS_CYCLE; t->p[P_M3DST] = MD_CUT; t->p[P_M3AMT] = 30;
            t->p[P_M4SRC] = MS_CYCLE; t->p[P_M4DST] = MD_SUBLVL; t->p[P_M4AMT] = 20;
        }
        XV(&trk[0], X_SUBLVL) = 90; XV(&trk[0], X_SUBWAVE) = 2; XV(&trk[0], X_SUBHARM) = 80; XV(&trk[0], X_SUBOCT) = 1;
        XV(&trk[1], X_SUBLVL) = 70; XV(&trk[1], X_SUBWAVE) = 0; XV(&trk[1], X_SUBHARM) = 30; XV(&trk[1], X_SUBOCT) = 0;
        trk[1].p[P_E1] = 40; trk[1].p[P_E6] = 7;                                  /* WAVE: POS 40, STEPS 8 places, an LFO on POS across them */
        trk[1].p[P_LD_SHP] = 20; trk[1].p[P_LRATE] = 70;
        trk[1].p[P_VOICE] = id == 10u ? V_UNISON : id == 11u ? V_LEGATO : V_UNISON;
        trk[1].p[P_DETUNE] = 40;
        trk[1].p[P_GLIDE] = 30;
        trk[0].p[P_SPRD] = id == 11u ? 60 : trk[0].p[P_SPRD];
    }
    if (id == 1u || id == 5u || id == 10u) {            /* DUEL */
        duell_mode_force(DM_SPLIT);
        if (!dm_split)
            return 0;
    }
    if (id == 0u || id == 3u) {                         /* the matrix on both parts: DECAY-free routes into the bus values (mod_goff of each part) and the parts' own */
        trk[0].p[P_M2SRC] = MS_LFO; trk[0].p[P_M2DST] = MD_RDAMP; trk[0].p[P_M2AMT] = 30;
        trk[1].p[P_M1SRC] = MS_LFO; trk[1].p[P_M1DST] = MD_PAN; trk[1].p[P_M1AMT] = -40;
        trk[1].p[P_M2SRC] = MS_LFO; trk[1].p[P_M2DST] = MD_RSIZE; trk[1].p[P_M2AMT] = 40;
        trk[0].p[P_EQLG] = 15; trk[0].p[P_EQMG] = -15; trk[0].p[P_EQHG] = 10;
        trk[1].p[P_EQLG] = -10; trk[1].p[P_EQMG] = 12; trk[1].p[P_EQHG] = 6;
        trk[1].p[P_DIST] = 60;
    }
    if (id == 1u || id == 5u || id == 10u) {
        mix_bal = id == 1u ? 20 : -15;
        for (k = 0; k < 2u; k++) {
            trk[k].p[P_EQLG] = 20; trk[k].p[P_EQMG] = -20; trk[k].p[P_EQHG] = 20;
            trk[k].p[P_DKSRC] = 0; trk[k].p[P_DKDEP] = 80; trk[k].p[P_DKATK] = 10; trk[k].p[P_DKRLS] = 40; trk[k].p[P_DKRATE] = 64;   /* PUMP, not SRC PART */
        }
    }
    if (id == 2u || id == 4u || id == 11u) {
        duell_mode_force(DM_LAYER);
        trk[0].p[P_SPRD] = 60;
        trk[1].p[P_SPRD] = (int16_t)(id == 2u ? 80 : 0);
        trk[0].p[P_EQLG] = 12; trk[0].p[P_EQMG] = -12; trk[0].p[P_EQHG] = 12;
        trk[1].p[P_EQLG] = 12; trk[1].p[P_EQMG] = -12; trk[1].p[P_EQHG] = 12;
        trk[1].p[P_M1SRC] = MS_LFO; trk[1].p[P_M1DST] = MD_PAN; trk[1].p[P_M1AMT] = 30;
    }
    if (id == 5u) {
        trk[0].p[P_M2SRC] = MS_LFO; trk[0].p[P_M2DST] = MD_RDAMP; trk[0].p[P_M2AMT] = 30;
        trk[1].p[P_M2SRC] = MS_LFO; trk[1].p[P_M2DST] = MD_RSIZE; trk[1].p[P_M2AMT] = 40;
        trk[1].p[P_DIST] = 60;
    }
    if (id == 3u || id == 4u || id == 5u) {             /* MORPH on both parts: ends rolled, the fader moved in the loop below */
        morph_set_target(1);
        morph_set_amount(id == 4u ? 55u : 70u);
        morph_set_fader(30);
        morph_roll(0);
        morph_roll(1);
    }
    if (id == 6u) {                                     /* the S&H of both parts' LFO 1 wraps in the same blocks: each draws from the one shared generator (voice.c lfo_rand) */
        for (k = 0; k < 2u; k++) {
            trk[k].p[P_LWAVE] = 4; trk[k].p[P_LRATE] = 70; trk[k].p[P_LD_PIT] = 60; trk[k].p[P_LTRIG] = 0; trk[k].p[P_LPHASE] = 0;
        }
    }
    if (id == 8u) {                                     /* part 1 keys (SRC OFF), part 2 is ducked by it */
        trk[0].p[P_DKSRC] = 3;
        trk[1].p[P_DKSRC] = 1; trk[1].p[P_DKDEP] = 90; trk[1].p[P_DKTHR] = 10; trk[1].p[P_DKATK] = 10; trk[1].p[P_DKRLS] = 40;
        trk[0].p[P_EQLG] = 15; trk[0].p[P_EQMG] = -15; trk[0].p[P_EQHG] = 10;
        trk[1].p[P_DIST] = 60;
        trk[0].p[P_REL] = 10;
    }
    if (id == 2u)
        song.g[G_DMODE] = DMD_PING;                     /* PING-PONG: the repeats left and right */
    if (getenv("SCN_TWIN"))
        scn_twin_setup(id);
    for (b = 0; b < nb; b++) {
        if (b == 10u) {
            for (i = 0; i < 4u; i++)
                trk_note_on(&trk[0], C1[i], 120);
            if (id != 4u)                               /* scene 4: part 2 never plays (silent from the start) */
                for (i = 0; i < 3u; i++)
                    trk_note_on(&trk[1], C2[i], 110);
        }
        if (b == nb / 2u) {
            for (i = 0; i < 4u; i++)
                trk_note_off(&trk[0], C1[i]);
            for (i = 0; i < 3u; i++)
                trk_note_off(&trk[1], C2[i]);
        }
        if (id == 0u) {
            if (b == MS(300)) { trk[0].p[P_PAN] = 50; trk[1].p[P_DIST] = 110; }               /* the PAN ramp, the SAT slew */
            if (b == nb / 2u + 62u) trk[1].p[P_ITYPE] = 0;                                      /* TYPE OFF at the end of the tail (ins_rest) */
            if (b == nb / 2u + MS(300)) trk[1].p[P_PAN] = -20;                                  /* a silent part: its pan taken at once */
            if (b == MS(600)) mods_off |= MODS_PFX;                                             /* PART FX OFF: the crossfade */
            if (b == MS(900)) mods_off &= (uint8_t)~MODS_PFX;
        }
        if (id == 1u) {
            if (b == MS(200)) mods_off |= MODS_PFX;                                             /* the PART FX crossfade, DUEL */
            if (b == MS(300)) perf_held = PF_BIT(PF_M1);                                        /* MUTE 1 in the crossfade */
            if (b == MS(500)) { mods_off &= (uint8_t)~MODS_PFX; perf_held = PF_BIT(PF_M2); }
            if (b == MS(700)) perf_held = 0;
            if (b == MS(800)) perf_space = 60;                                                  /* SPACE right of the middle in DUEL (bus A's floors) */
            if (b == nb / 2u + MS(200)) trk[1].p[P_DIST] = 100;
        }
        if (id == 2u) {
            if (b == MS(100)) perf_held = PF_BIT(PF_M1) | PF_BIT(PF_DRV);                       /* MUTE 1 and DRIVE held */
            if (b == MS(200)) perf_held = PF_BIT(PF_M2);
            if (b == MS(300)) perf_held = 0;
            if (b == MS(400)) mods_off |= MODS_PFX;                                             /* PART FX OFF: the crossfade .. */
            if (b == MS(400) + 3u) perf_held = PF_BIT(PF_M2);                                   /* .. with a MUTE in it (pfx_mute) */
            if (b == MS(450)) perf_held = 0;
            if (b == MS(600)) mods_off &= (uint8_t)~MODS_PFX;
            if (b == MS(700)) perf_space = 60;                                                  /* SPACE right of the middle */
            if (b == MS(800)) mods_off |= MODS_SENDS;                                           /* SENDS OFF: the fade */
            if (b == MS(900)) { duell_mode = DM_SPLIT; duell_mode_req = DM_SPLIT; }             /* the MODE's dip, LAYER -> DUEL */
            if (b == nb / 2u + MS(300)) perf_held = PF_BIT(PF_M1);                              /* a MUTE held on a silent part (perf_mute_rest) */
        }
        if (id == 11u && b == MS(300))                  /* a LEGATO step on part 2 (the cycle runs on) */
            trk_note_on(&trk[1], 79, 100);
        if (id == 11u && b == MS(400))                  /* part 1: more notes than voices: steals (the sub and the cycle of a stolen voice go on from where they stand) */
            for (i = 0; i < 7u; i++)
                trk_note_on(&trk[0], 36u + i * 3u, 100);
        if (id == 8u && b > 10u && b % MS(250) == 0u) {
            for (i = 0; i < 4u; i++)
                trk_note_off(&trk[0], C1[i]);
            for (i = 0; i < 4u; i++)
                trk_note_on(&trk[0], C1[i], 120);
        }
        if (id == 3u || id == 4u || id == 5u) {
            if (b % MS(200) == 0u)
                morph_set_fader((b / MS(200)) * 37u % 128u);                                    /* the fader on its way: the cross-fades and the ramp */
        }
        if (id == 4u) {
            if (b == MS(250)) { mods_off |= MODS_PFX; }                                         /* PART FX crossfade with MORPH */
            if (b == MS(350)) perf_held = PF_BIT(PF_M1);                                        /* MUTE 1 */
            if (b == MS(500)) { mods_off &= (uint8_t)~MODS_PFX; perf_held = 0; }
            if (b == MS(600)) trk[0].p[P_PAN] = 55;                                             /* a PAN step */
        }
        if (id == 5u && b == MS(900)) { duell_mode = DM_STEREO; duell_mode_req = DM_STEREO; }   /* the MODE's dip, DUEL -> STEREO */
        if (id == 1u || id == 5u)
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
