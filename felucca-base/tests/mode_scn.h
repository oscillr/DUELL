/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL step 5: a busy two-part STEREO render on the DSP of hostsim.c, written against the sources as they were before
 * step 5 (only what they had), so the same function gives the same hash there and here: tests/mode_test.c compares its
 * hash with the one the sources before step 5 gave (MODE_STEREO_HASH, made by building this file against them).
 * Two parts: part 1 ANALOG with SPREAD 100 (POLY), a PAN, DIST, the matrix's LFO -> PAN; part 2 PHASE with the INSERT
 * (CHORUS, MIX 100) and its PAN the other way; both with every send up; HALL, BASS+ (MENU > SPEAKER), the delay with
 * feedback. Chords on both, released, the tails. Must run
 * from the process's own power-on state (the first render of the process) */
static uint32_t scn_eng(const char *n)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (str_eq(ENGINES[e]->name, n))
            return e;
    return 0;
}
static uint64_t mode_stereo_scenario(void)
{
    static const uint8_t C1[4] = {48, 55, 60, 64}, C2[3] = {67, 72, 76};
    static int32_t o[2 * CTL];
    uint64_t h = 0xCBF29CE484222325ull;
    uint32_t k, i, b, nb = 3u * FS / CTL;
    host_tracks_init();
    host_preset(&trk[0], scn_eng("ANALOG"), 1);
    host_preset(&trk[1], scn_eng("PHASE"), 2);
    song.g[G_RTYPE] = 2;                          /* HALL (stereo) */
    song.g[G_DFDBK] = 90;
    song.g[G_DMIX] = 110;
    song.g[G_CDEPTH] = 100;
    fx_lowcut = 2;                                /* BASS+ */
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_SPRD] = 100;
    trk[0].p[P_PAN] = -30;
    trk[0].p[P_DIST] = 40;
    trk[0].p[P_LTRIG] = 0;                        /* NOTE: the scenario's hash predates the FREE default (0.2.2) and stays the same */
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
        trk[k].p[P_LEVEL] = 120;
        trk[k].p[P_CHOR] = 100;
        trk[k].p[P_DLY] = 90;
        trk[k].p[P_REV] = 110;
        trk[k].p[P_SUS] = 110;
        trk[k].p[P_REL] = 70;
    }
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
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++) {
            h ^= (uint32_t)o[i];
            h *= 0x100000001B3ull;
        }
    }
    fx_lowcut = 0;
    return h;
}
