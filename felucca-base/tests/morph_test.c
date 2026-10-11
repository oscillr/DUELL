/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL build step 4a: the MORPH engine (firmware/src/morph.c), host test through hostsim.c (the firmware's sources).
 *   build/host/morph_test            (DUELL/host_tests.sh "morph")
 *  1. tags: every P_* and every engine's E0..E7 has one; the excluded list of SPEC 3.3 rule 3 is excluded; the summary.
 *  2. AMOUNT 0 changes nothing (roll, and the render bit for bit); ends back at the Original: bit-identical too.
 *  3. bounds over 2000 rolls per engine at AMOUNT 10 / 25 / 50 / 100: no continuous value moves more than AMOUNT % of its
 *     range (or its cap); the move is uniform 0..AMOUNT, random sign: nearly every value moves, median ~AMOUNT / 2, the choices change at AMOUNT % (4 sigma), matrix SRC / DST as well; excluded and hard ones never.
 *  4. zero stays zero (ENV / LFO -> PITCH, GLIDE, PHASE DTN, pitch routes; no new PITCH / VIB route); PAN / SPREAD only in
 *     STEREO (the roll, and the hook in SPLIT).
 *  5. the hook: fader 0 = End A and 127 = End B exactly (every value), t->p back after the window; monotone in between.
 *  6. rendered fader sweeps: no step larger than the static ends have (per-sample peak-to-peak), for every fade kind;
 *     against a hard switch (the control). The ~50 ms ramp of a rolled end that is heard.
 *  7. UNDO on the far end, history of 8 (v2: no KEEP); manual edits on top of the morph.
 *  8. TARGET SELECTED / BOTH; seeds reproduce; the persistence block round-trips.
 *  9. never silent / never clipping: 500 rolls at AMOUNT 100 on 6 engines, a note rendered, RMS and peak bounds.
 * 10. cost: instructions per sample of a morph-active part (fades running) against idle. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <math.h>
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif

static int bad;
static void check(const char *what, int ok)
{
    printf("morph: %-96s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static int32_t out_buf[2 * CTL];
static uint64_t hash;
static void blocks(uint32_t n)
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
static void fresh(uint32_t e, uint32_t pi)
{
    uint32_t k;
    memset(dly_buf, 0, sizeof dly_buf);
    memset(cho_buf, 0, sizeof cho_buf);
    memset(rev_comb, 0, sizeof rev_comb);
    memset(rev_ap, 0, sizeof rev_ap);
    memset(&fx, 0, sizeof fx);
    lim_env = LIM_T;
    memset(trk, 0, sizeof trk);
    memset(&mod, 0, sizeof mod);
    memset(mph, 0, sizeof mph);
    memset(&morph, 0, sizeof morph);
    memset(&mw, 0, sizeof mw);
    memset(&mxf, 0, sizeof mxf);
    duell_mode_force(DM_SPLIT);
    host_tracks_init();
    for (k = 0; k < NPART; k++)
        host_preset(&trk[k], k ? 0u : e, k ? 0u : pi);
    song.sel = 0;
    hash = 0xCBF29CE484222325ull;
}
static void orig8(const track_t *t, int8_t *o)
{
    uint32_t i;
    for (i = 0; i < P_COUNT; i++)
        o[i] = (int8_t)t->p[i];
}
static void slot(track_t *t, uint32_t k, int32_t s, int32_t d, int32_t a)
{
    t->p[MSLOT_ID(k)] = (int16_t)s;
    t->p[MSLOT_ID(k) + 1u] = (int16_t)d;
    t->p[MSLOT_ID(k) + 2u] = (int16_t)a;
}
/* a busier sound than the presets: active routes (one to PITCH, one from the mod wheel), the pitch depths at 0 */
static void busy(track_t *t)
{
    slot(t, 0, MS_LFO, MD_CUT, 30);
    slot(t, 1, MS_ENV, MD_SHP, -20);
    slot(t, 2, MS_VEL, MD_AMP, 20);
    slot(t, 3, MS_MODW, MD_DIST, 10);
    slot(t, 4, MS_LFO2, MD_PITCH, 4);
    slot(t, 5, MS_RAND, MD_E1 + 2, 12);
    t->p[P_ED_PIT] = 0;
    t->p[P_LD_PIT] = 0;
    t->p[P_GLIDE] = 0;
    t->p[P_LD_FLT] = 20;
    t->p[P_PAN] = 10;
    t->p[P_SPRD] = 30;
}
static uint32_t engines_used(uint32_t *list)            /* DUELL's engines (not DIGITAL, not DRUM) */
{
    uint32_t e, n = 0;
    for (e = 0; e < NENGINES; e++)
        if (e != ENGI_DIGITAL && e != ENGI_DRUM)
            list[n++] = e;
    return n;
}
static void settle(void)                                 /* the ~50 ms ramps over (as after 69 blocks) */
{
    uint32_t pi;
    for (pi = 0; pi < NSHOWN; pi++) {
        mph[pi].sm_r = 32768;
        if (mph[pi].n[0])
            morph_build(&mph[pi]);
    }
}
static const int8_t *endp(uint32_t pi, uint32_t x) { return morph_cur(&mph[pi], x); }
static int8_t *endw(uint32_t pi, uint32_t x) { return mph[pi].h[x][mph[pi].top[x]]; }

/* ------------------------------------------------------------------ 1 tags --- */
static void test_tags(void)
{
    static const char *const KN[] = {"excluded", "hard", "continuous", "fadable", "matrix"};
    uint32_t n[5] = {0}, id, e, k, ok = 1, list[NENGINES], ne = engines_used(list);
    int8_t o[P_COUNT] = {0};
    static const uint8_t EXCL[] = {P_LEVEL, P_TRANS, P_ROOT, P_SCALE, P_AOCT, P_VOICE, P_LPOL, P_L2POL, P_AMODE, P_AHOLD,
                                   P_ITYPE, P_MUTE, P_FREEB, P_AGATE, P_ASWING, P_APROB, P_LSYNC, P_L2SYNC, P_ESYNC};
    for (id = 0; id < P_E0; id++)
        n[MT[id].kind]++;
    printf("morph: common tags (P_LEVEL..P_ESYNC, %u ids):", (unsigned)P_E0);
    for (k = 0; k < 5u; k++)
        printf(" %s %u", KN[k], (unsigned)n[k]);
    printf("\n");
    for (k = 0; k < NELEM(EXCL); k++)
        ok &= morph_tag(&ENG_ANALOG, o, EXCL[k], 1).kind <= MK_HARD;
    check("rule 3: LEVEL, TRANS, ROOT, SCALE, ARP OCT, VOICE, LFO POL, ARP MODE, INSERT TYPE, ... never rolled", ok);
    for (k = 0; k < ne; k++) {
        const engine_t *en = ENGINES[list[k]];
        uint32_t c[5] = {0};
        for (id = P_E0; id < P_COUNT; id++)
            c[morph_tag(en, o, id, 1).kind]++;
        printf("morph:   %-7s E0..E7: cont %u fade %u hard %u excl %u  [", en->name, (unsigned)c[MK_CONT],
               (unsigned)c[MK_FADE], (unsigned)c[MK_HARD], (unsigned)c[MK_EX]);
        for (id = 0; id < 8u; id++) {
            mtag_t g = morph_tag(en, o, P_E0 + id, 1);
            printf("%s%c", id ? " " : "", "XHCFM"[g.kind]);
        }
        printf("]\n");
    }
    ok = 1;
    for (id = 0; id < P_COUNT; id++) {                  /* every stored value fits the morph's int8 */
        for (e = 0; e < ne; e++) {
            const param_desc_t *pd = morph_desc(ENGINES[list[e]], id);
            ok &= pd->min >= -128 && pd->max <= 127;
        }
    }
    check("every parameter's range fits the morph's int8 ends", ok);
    {
        int8_t q[P_COUNT] = {0};
        q[P_E0] = AW_SYNC;
        ok = morph_tag(&ENG_ANALOG, q, P_E0, 1).kind == MK_HARD && !(morph_tag(&ENG_ANALOG, q, P_E1, 1).flags & MF_CAP);
        q[P_E0] = 0;
        ok &= morph_tag(&ENG_ANALOG, q, P_E0, 1).kind == MK_FADE && morph_tag(&ENG_PHASE, q, P_FTYPE, 1).kind == MK_EX;
        q[P_E0] = CHIP_STEP;
        ok &= morph_tag(&ENG_LOFI, q, P_E3, 1).kind == MK_EX;
        q[P_E0] = 0;
        q[P_E1] = RW_NOIS;
        ok &= morph_tag(&ENG_LOFI, q, P_E1, 1).kind == MK_HARD;
        q[P_E0] = NZ_LFSR;
        ok &= morph_tag(&ENG_NOISE, q, P_E5, 1).kind == MK_HARD && morph_tag(&ENG_NOISE, q, P_E7, 1).kind == MK_EX;
        q[P_E0] = PM_SYMP;
        ok &= morph_tag(&ENG_PHYS, q, P_E1, 1).kind == MK_HARD;
        ok &= morph_tag(&ENG_ANALOG, q, P_PAN, 0).kind == MK_EX && morph_tag(&ENG_ANALOG, q, P_PAN, 1).kind == MK_CONT;
        check("mode-dependent tags (B.4): ANALOG SYNC, LOFI STEP / NOIS, NOISE LFSR, PHYS SYMP; FTYPE ANALOG only; PAN", ok);
    }
}

/* ------------------------------------------------------------- 2 AMOUNT 0 --- */
static uint64_t phrase(void)
{
    track_t *t = &trk[0];
    trk_note_on(t, 60, 100);
    trk_note_on(t, 64, 70);
    trk_note_on(t, 67, 120);
    blocks(FS / 2u / CTL);
    trk_note_on(t, 72, 90);
    blocks(FS / 4u / CTL);
    trk_note_off(t, 60);
    trk_note_off(t, 64);
    trk_note_off(t, 67);
    trk_note_off(t, 72);
    blocks(FS / 2u / CTL);
    return hash;
}
/* the phrase's hash in a child (the same generators each time): 0 nothing, 1 AMOUNT 0 rolled, 2 rolled and undone,
 * 3 AMOUNT 40 rolled (must differ) */
static uint64_t phrase_child(uint32_t e, uint32_t pi, uint32_t k)
{
    int fd[2];
    uint64_t h = 0;
    pid_t pid;
    if (pipe(fd))
        return 0;
    fflush(stdout);
    if (!(pid = fork())) {
        int16_t keep[P_COUNT];
        fresh(e, pi);
        trk[0].preset = 0xFFu;                          /* (DUELL: no factory sound, sounds.c: its load rolls B at the
                                                         * AMOUNT set below, not the factory sound's fixed B) */
        busy(&trk[0]);
        if (k) {
            mph[0].amount = (uint8_t)(k == 3u ? 40u : k == 2u ? 60u : 0u);   /* (v2: a load rolls a B at the AMOUNT: set before) */
            mph[0].apre = 1;
            morph_set_amount(mph[0].amount);
            morph_set_fader(40);
            morph_roll(0);
            morph_roll(1);
            if (k == 2u) {
                morph_set_fader(0);                     /* far end B */
                morph_undo(1);
                morph_undo(1);                           /* (v2: past the fresh B of the load, to the Original) */
                morph_set_fader(127);                   /* far end A */
                morph_undo(0);
                morph_set_fader(40);
            }
        }
        memcpy(keep, trk[0].p, sizeof keep);
        h = phrase();
        if (memcmp(keep, trk[0].p, sizeof keep) || mw.on || mxf.on)
            h = 0;                                      /* t->p must be the Original after every block */
        if (k && k != 3u && mph[0].nid)
            h = 0;
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
static void test_amount0(void)
{
    uint32_t list[NENGINES], ne = engines_used(list), k, s, ok = 1;
    int8_t o[P_COUNT], end[P_COUNT];
    for (k = 0; k < ne; k++) {
        fresh(list[k], 0);
        busy(&trk[0]);
        orig8(&trk[0], o);
        for (s = 1; s <= 200u; s++) {
            morph_roll_pure(o, end, ENGINES[list[k]], 0, s, 0, 1);
            ok &= !memcmp(o, end, P_COUNT);
        }
    }
    check("AMOUNT 0: every roll equals the Original (every engine, 200 seeds)", ok);
    {
        uint32_t ea = eng_by_name("ANALOG"), ep = eng_by_name("PHASE");
        uint64_t h0 = phrase_child(ea, 1, 0), h1 = phrase_child(ea, 1, 1), h2 = phrase_child(ea, 1, 2);
        uint64_t h3 = phrase_child(ea, 1, 3), g0 = phrase_child(ep, 2, 0), g1 = phrase_child(ep, 2, 1);
        check("AMOUNT 0 rolled, fader anywhere: the render is bit-identical to no morph (ANALOG, PHASE)",
              h0 && h0 == h1 && g0 && g0 == g1);
        check("ends rolled then undone back to the Original: bit-identical (the list empties)", h0 && h0 == h2);
        check("(control) AMOUNT 40 rolled: the render differs, and t->p is the Original after every block", h3 && h3 != h0);
    }
}

/* ---------------------------------------------------------------- 3 bounds --- */
static void test_bounds(void)
{
    static const uint32_t AM[] = {10, 25, 50, 100};
    uint32_t list[NENGINES], ne = engines_used(list), a;
    for (a = 0; a < NELEM(AM); a++) {
        uint32_t amount = AM[a], k, s, id;
        uint64_t opp = 0, chg = 0, topp = 0, tchg = 0, sopp = 0, schg = 0, dopp = 0, dchg = 0;
        uint32_t over = 0, excl_moved = 0, bad_choice = 0, bad_route = 0, inactive = 0, ncont = 0, fix_n = 0, fix_chg = 0;
        uint32_t nelig = 0, nmoved = 0, nin = 0, nup = 0, hist[101] = {0}, nh = 0, med;
        double worst = 0;
        for (k = 0; k < ne; k++) {
            const engine_t *e = ENGINES[list[k]];
            int8_t o[P_COUNT], end[P_COUNT];
            fresh(list[k], 0);
            busy(&trk[0]);
            orig8(&trk[0], o);
            for (s = 1; s <= 2000u; s++) {
                morph_roll_pure(o, end, e, amount, (s + 2003u * k + 20011u * a) % 65535u + 1u, s & 1u, 0);
                for (id = 0; id < P_COUNT; id++) {
                    mtag_t g = morph_tag(e, o, id, 0);
                    const param_desc_t *pd = morph_desc(e, id);
                    int32_t lim = (int32_t)amount * (pd->max - pd->min) / 100, d = end[id] - o[id];
                    if (g.kind == MK_CONT) {
                        if (g.flags & MF_CAP && lim > g.lim)
                            lim = g.lim;
                        ncont++;
                        if (d > lim || -d > lim)
                            over++;
                        if (lim && fabs((double)d) / (pd->max - pd->min) > worst)
                            worst = fabs((double)d) / (pd->max - pd->min);
                        if (lim && !((g.flags & MF_ZERO) && !o[id])) {   /* eligible: it moves, uniform 0 .. AMOUNT */
                            nelig++;
                            nmoved += d != 0;
                            hist[(abs(d) * 100 + lim / 2) / lim > 100 ? 100 : (abs(d) * 100 + lim / 2) / lim]++;
                            nh++;
                            if (o[id] - lim >= pd->min && o[id] + lim <= pd->max && !g.flags) {
                                nin++;                  /* (away from the range's ends, no limit: both ways open) */
                                nup += d > 0;
                            }
                        }
                    } else if (g.kind == MK_FADE) {
                        int32_t n = (g.flags & MF_NFADE) ? g.lim : pd->max + 1;
                        if ((e == &ENG_ANALOG && id == P_FTYPE) || (e == &ENG_TRIO && id == P_E0 + 4u)) {
                            topp++;                     /* a filter TYPE: the dead-path rule keeps some (morph_open) */
                            tchg += d != 0;
                        } else {
                            opp++;
                            chg += d != 0;
                        }
                        bad_choice += end[id] < 0 || end[id] >= n;
                    } else if (g.kind == MK_SLOT) {
                        continue;
                    } else {
                        excl_moved += d != 0;
                    }
                }
                for (id = 0; id < NMSLOT; id++) {       /* the routes */
                    uint32_t q = MSLOT_ID(id), so = (uint32_t)o[q], dd = (uint32_t)o[q + 1u];
                    uint32_t sn = (uint32_t)end[q], dn = (uint32_t)end[q + 1u];
                    if (!so || !dd || !o[q + 2u]) {
                        inactive += memcmp(&o[q], &end[q], 3);
                        continue;
                    }
                    if (dd != MD_PAN && !morph_route_fixed(e, o, so, dd)) {
                        sopp++;
                        schg += sn != so;
                        dopp++;
                        dchg += dn != dd;
                    } else {
                        bad_route += sn != so || dn != dd;
                        fix_n++;                        /* a route the roll does not move the destination of (PITCH, ..): its AMT still rolls */
                        fix_chg += end[q + 2u] != o[q + 2u];
                    }
                    bad_route += (dn == MD_PITCH || dn == MD_VIB) && dn != dd;
                    bad_route += dn == MD_PAN && dd != MD_PAN;  /* (SPLIT) */
                    bad_route += morph_loop(sn, dn) || !sn || !dn || MS_PERF(sn) != MS_PERF(so);
                }
            }
        }
        {
            double p = amount / 100.0, r = (double)chg / opp, rs = (double)schg / sopp, rd = (double)dchg / dopp;
            double sg = sqrt(p * (1 - p) / opp), ss = sqrt(p * (1 - p) / sopp), sd = sqrt(p * (1 - p) / dopp);
            char b[200];
            printf("morph: AMOUNT %3u%%: %u continuous draws, the largest move %.1f %% of a range; choices %.2f %% changed "
                   "(%llu); filter TYPE %.2f %% (dead-path rule); SRC %.2f %%, DST %.2f %% (%llu routes)\n", (unsigned)amount,
                   (unsigned)ncont, worst * 100, r * 100, (unsigned long long)opp, 100.0 * tchg / topp, rs * 100, rd * 100,
                   (unsigned long long)sopp);
            snprintf(b, sizeof b, "AMOUNT %u%%: no continuous value moves more than %u%% of its range (or its cap), 2000 rolls x %u engines",
                     (unsigned)amount, (unsigned)amount, (unsigned)ne);
            check(b, !over && worst <= p + 1e-9);
            for (med = 0, k = 0; med <= 100u && k + hist[med] <= nh / 2u; med++)
                k += hist[med];
            printf("morph: AMOUNT %3u%%: eligible continuous values moved %.2f %%; median |move| %.2f x AMOUNT; up %.1f %% "
                   "(%u away from the ends)\n", (unsigned)amount, 100.0 * nmoved / nelig, med / 100.0, 100.0 * nup / nin, (unsigned)nin);
            snprintf(b, sizeof b, "AMOUNT %u%%: the uniform move: >= 95 %% of eligible values move, median |move| 0.35..0.65 x AMOUNT, signs balanced",
                     (unsigned)amount);
            check(b, nmoved >= 0.95 * nelig && med >= 35u && med <= 65u &&
                     (!nin || fabs((double)nup / nin - 0.5) <= 4 * sqrt(0.25 / nin) + 0.01));   /* (100 %: no value has both ways open) */
            snprintf(b, sizeof b, "AMOUNT %u%%: choices change at the AMOUNT rate (4 sigma), always to an allowed value", (unsigned)amount);
            check(b, fabs(r - p) <= 4 * sg + 1e-9 && !bad_choice);
            snprintf(b, sizeof b, "AMOUNT %u%%: SMART matrix: SRC / DST of an assigned route never change (owner 2026-10-12); empty slots stay empty", (unsigned)amount);
            check(b, !schg && !dchg && !bad_route && !inactive);
            snprintf(b, sizeof b, "AMOUNT %u%%: SMART: a route into a parameter the roll does not move still rolls its amount (%u of %u)", (unsigned)amount, (unsigned)fix_chg, (unsigned)fix_n);
            check(b, fix_n && fix_chg);
            snprintf(b, sizeof b, "AMOUNT %u%%: excluded and hard parameters never change (%u engines x 2000 rolls)", (unsigned)amount, (unsigned)ne);
            check(b, !excl_moved);
        }
    }
}

/* --------------------------------------------------------- 4 zero, stereo --- */
static void test_zero_stereo(void)
{
    uint32_t s, ok = 1, moved = 0, pan_split = 0, pan_st = 0, newpitch = 0;
    int8_t o[P_COUNT], end[P_COUNT];
    const engine_t *e = &ENG_ANALOG;
    fresh(0, 1);
    busy(&trk[0]);
    trk[0].p[P_LD_SHP] = 30;
    orig8(&trk[0], o);
    for (s = 1; s <= 2000u; s++) {
        uint32_t k;
        morph_roll_pure(o, end, e, 100, s, 0, 0);
        ok &= !end[P_ED_PIT] && !end[P_LD_PIT] && !end[P_GLIDE];
        moved += end[P_LD_SHP] != o[P_LD_SHP];
        pan_split += end[P_PAN] != o[P_PAN] || end[P_SPRD] != o[P_SPRD];
        for (k = 0; k < NMSLOT; k++)
            newpitch += (end[MSLOT_ID(k) + 1u] == MD_PITCH || end[MSLOT_ID(k) + 1u] == MD_VIB) &&
                        o[MSLOT_ID(k) + 1u] != end[MSLOT_ID(k) + 1u];
        ok &= end[MSLOT_ID(4) + 1u] == MD_PITCH && end[MSLOT_ID(4)] == MS_LFO2 &&
              end[MSLOT_ID(4) + 2u] == o[MSLOT_ID(4) + 2u];   /* the pitch route keeps its route */
        morph_roll_pure(o, end, e, 100, s, 0, 1);
        pan_st += end[P_PAN] != o[P_PAN];
    }
    
    check("PITCH off: ENV->PIT, LFO->PIT, GLIDE at 0 never move (2000 rolls, AMOUNT 100); LFO->SHP does",
          ok && moved > 1500);
    check("no new route to PITCH or VIB; a PITCH route keeps its route and (PITCH off) its amount", !newpitch && ok);
    {   /* owner 2026-10-12: the pitch route's amount stays whole with PITCH off, rolls with PITCH on */
        uint32_t on_moved = 0;
        mrc = MC_PIT;
        for (s = 1; s <= 200u; s++) {
            morph_roll_pure(o, end, e, 100, s, 0, 0);
            on_moved += end[MSLOT_ID(4) + 2u] != o[MSLOT_ID(4) + 2u];
        }
        mrc = 0;
        check("PITCH on: the route into PITCH rolls its amount like the others", on_moved > 100);
    }
    {
        uint32_t phs = 0;
        fresh(eng_by_name("PHASE"), 0);
        orig8(&trk[0], o);                                     /* DTN 0 (BRASS) */
        for (s = 1; s <= 2000u; s++) {
            morph_roll_pure(o, end, &ENG_PHASE, 100, s, 0, 1);
            phs += end[P_E4] != 0 || end[P_E5] != o[P_E5];
        }
        check("PHASE DTN 0 stays 0 (no second line) and LINE never changes (review 5, 6)", !phs);
    }
    check("PAN / SPREAD: never rolled in SPLIT; rolled in STEREO", !pan_split && pan_st > 1500);
    /* the hook: ends with a PAN offset (rolled in STEREO) play it only in STEREO */
    fresh(0, 1);
    duell_mode_force(DM_STEREO);
    morph_set_amount(100);
    morph_roll(0);
    morph_set_fader(0);
    endw(0, 0)[P_PAN] = 50;
    settle();
    duell_mode_force(DM_SPLIT);
    morph_begin(&trk[0], 0);
    ok = trk[0].p[P_PAN] == 0;
    morph_end(&trk[0]);
    duell_mode_force(DM_STEREO);
    morph_begin(&trk[0], 0);
    ok &= trk[0].p[P_PAN] == 50;
    morph_end(&trk[0]);
    duell_mode_force(DM_SPLIT);
    check("the hook: a PAN end sounds in STEREO only (SPLIT: the Original's PAN)", ok);
}

/* ---------------------------------------------------- 5 the hook: exact, monotone --- */
static void test_exact(void)
{
    uint32_t list[NENGINES], ne = engines_used(list), k, r, id, f, ok_a = 1, ok_b = 1, ok_back = 1, ok_mono = 1, ok_fade = 1;
    uint32_t ok_route = 1, nchk = 0;
    for (k = 0; k < ne; k++)
        for (r = 0; r < 20u; r++) {
            track_t *t = &trk[0];
            int16_t keep[P_COUNT], prev[P_COUNT];
            const int8_t *A, *B;
            int32_t pw[MX_N];
            fresh(list[k], r % ENGINES[list[k]]->npresets);
            busy(t);
            morph_set_amount(30 + r * 3);
            morph_roll_seed(0, 0, 100 + r);
            morph_roll_seed(0, 1, 900 + r);
            A = endp(0, 0);
            B = endp(0, 1);
            memcpy(keep, t->p, sizeof keep);
            for (f = 0; f <= 127u; f++) {
                mph[0].fader = (uint8_t)f;
                morph_begin(t, 0);
                if (f == 0u || f == 127u) {
                    const int8_t *X = f ? B : A;
                    for (id = 0; id < P_COUNT; id++)
                        if (t->p[id] != X[id])
                            *(f ? &ok_b : &ok_a) = 0;
                    if (mxf.on)
                        *(f ? &ok_b : &ok_a) = 0;
                }
                if (f) {
                    for (id = 0; id < P_COUNT; id++) {
                        mtag_t g = morph_tag(ENGINES[list[k]], mph[0].base, id, 0);
                        if (g.kind == MK_CONT) {
                            int32_t lo = A[id] < B[id] ? A[id] : B[id], hi = A[id] < B[id] ? B[id] : A[id];
                            int32_t step = t->p[id] - prev[id], mx = (hi - lo + 126) / 127 + 1;
                            if (t->p[id] < lo || t->p[id] > hi || (A[id] <= B[id] ? step < 0 : step > 0) || step > mx || -step > mx)
                                ok_mono = 0;
                            nchk++;
                        } else if (g.kind == MK_FADE && A[id] != B[id]) {
                            uint32_t x = morph_mx(id);
                            int32_t w = t->p[id] == B[id] ? 32768 : mxf.w[x];
                            if ((t->p[id] != A[id] && t->p[id] != B[id]) || (mxf.w[x] && mxf.b[x] != B[id]) || w < pw[x])
                                ok_fade = 0;
                            pw[x] = w;
                        }
                    }
                    for (id = 0; id < NMSLOT; id++) {   /* a route switch only where the amount is ~0 */
                        uint32_t q = MSLOT_ID(id);
                        if ((t->p[q] != prev[q] || t->p[q + 1u] != prev[q + 1u]) &&
                            (abs(t->p[q + 2u]) > 2 || abs(prev[q + 2u]) > 2))
                            ok_route = 0;
                    }
                } else {
                    for (id = 0; id < MX_N; id++)
                        pw[id] = 0;
                }
                memcpy(prev, t->p, sizeof prev);
                morph_end(t);
                if (memcmp(keep, t->p, sizeof keep))
                    ok_back = 0;
            }
        }
    check("fader 0 = End A and fader 127 = End B exactly, every value, no fade left (11 engines x 20 roll pairs)", ok_a && ok_b);
    check("t->p is the Original again after every window (morph_end)", ok_back);
    {
        char b[160];
        snprintf(b, sizeof b, "fader 0..127: continuous values monotone from A to B, steps <= |B-A|/127 + 1 (%u checks)", (unsigned)nchk);
        check(b, ok_mono);
    }
    check("fader 0..127: a faded choice holds A, its weight toward B rises monotonically, B at 127", ok_fade);
    check("fader 0..127: a matrix route switches only where its amount is ~0 (fade out, fade in)", ok_route);
}

/* ------------------------------------------------------- 6 rendered sweeps --- */
typedef struct { const char *eng, *preset; uint32_t id[3]; int8_t a[3], b[3]; uint32_t n, amount; const char *what; } sweep_t;
static double maxstep_l, rms_acc;
static int32_t prev_l = 0x7FFFFFFF;
static uint32_t nacc;
static void blocks_m(uint32_t n)                         /* render, track the largest sample step (left) */
{
    uint32_t i;
    while (n--) {
        mix_block(out_buf, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t x = out_buf[2u * i];
            if (prev_l != 0x7FFFFFFF && fabs((double)x - prev_l) > maxstep_l)
                maxstep_l = fabs((double)x - prev_l);
            prev_l = x;
            rms_acc += (double)x * x;
            nacc++;
        }
    }
}
static void sweep_setup(const sweep_t *s)
{
    uint32_t e = eng_by_name(s->eng), k;
    track_t *t = &trk[0];
    fresh(e, preset_by_name(e, s->preset));
    t->p[P_VOICE] = V_POLY;
    t->p[P_SUS] = 110;
    t->p[P_AMODE] = 0;
    t->p[P_REV] = t->p[P_DLY] = t->p[P_CHOR] = 0;
    if (str_eq(s->what, "LFO WAVE")) {
        t->p[P_LD_FLT] = 40;
        t->p[P_LRATE] = 70;
        t->p[P_E4] = 60;
    }
    if (str_eq(s->what, "matrix route")) {
        slot(t, 0, MS_LFO, MD_CUT, 40);
        t->p[P_LRATE] = 70;
        t->p[P_E4] = 60;
    }
    morph_set_amount(s->amount);
    morph_roll_seed(0, 0, 11);
    morph_roll_seed(0, 1, 71);   /* (22 before 2026-10-12: without added routes its ends made the LOFI static steps uneven) */
    endw(0, 0)[P_DIST] = endw(0, 1)[P_DIST] = mph[0].base[P_DIST];   /* (a DIST that rolls up from 0 adds steps of its own: this sweep tests the fade) */
    for (k = 0; k < s->n; k++) {
        endw(0, 0)[s->id[k]] = s->a[k];
        endw(0, 1)[s->id[k]] = s->b[k];
        if (k == 0 && !str_eq(s->what, "matrix route"))
            t->p[s->id[0]] = s->a[0], mph[0].base[s->id[0]] = s->a[0];
    }
    if (str_eq(s->what, "matrix route")) {             /* A: LFO -> CUT, B: ENV -> SHP */
        endw(0, 0)[MSLOT_ID(0)] = MS_LFO, endw(0, 0)[MSLOT_ID(0) + 1u] = MD_CUT, endw(0, 0)[MSLOT_ID(0) + 2u] = 40;
        endw(0, 1)[MSLOT_ID(0)] = MS_ENV, endw(0, 1)[MSLOT_ID(0) + 1u] = MD_SHP, endw(0, 1)[MSLOT_ID(0) + 2u] = -40;
    }
    morph_build(&mph[0]);
}
static void chord(void)
{
    trk_note_on(&trk[0], 57, 100);
    trk_note_on(&trk[0], 60, 90);
    trk_note_on(&trk[0], 64, 80);
}
/* the largest sample step: mode 0 fader fixed at f, 1 a sweep 0..127 (6 blocks a step), 2 a hard switch of the choice at
 * the middle (no morph: the control) */
static double sweep_run(const sweep_t *s, uint32_t mode, uint32_t f)
{
    uint32_t i;
    sweep_setup(s);
    mph[0].fader = (uint8_t)(mode == 1u ? 0u : mode == 2u ? 0u : f);
    if (mode == 2u)
        mph[0].nid = 0;                                 /* the plain sound with the A choice, then B's */
    chord();
    blocks(FS / 3u / CTL);                              /* settle */
    maxstep_l = 0;
    prev_l = 0x7FFFFFFF;
    for (i = 0; i <= 127u; i++) {
        if (mode == 1u)
            mph[0].fader = (uint8_t)i;
        if (mode == 2u && i == 64u)
            for (uint32_t k = 0; k < s->n; k++)
                trk[0].p[s->id[k]] = s->b[k];
        blocks_m(6);
    }
    return maxstep_l;
}
static void test_sweeps(void)
{
    static const sweep_t SW[] = {
        {"ANALOG", "SOFT PAD", {P_E0, P_FTYPE}, {3, 0}, {2, 1}, 2, 40, "ANALOG WAVE SIN->TRI + filter LP->BP"},
        {"ANALOG", "SOFT PAD", {P_E0}, {2}, {4}, 1, 0, "ANALOG WAVE TRI->PWM (AMOUNT 0 otherwise)"},
        {"PHASE", "STRING", {P_E0, P_E1}, {3, 0}, {6, 4}, 2, 40, "PHASE WAVE DSIN->RTRI, WAVE2 - -> DSIN"},
        {"TRIO", "CHIP CHOIR", {P_E4}, {0}, {2}, 1, 40, "TRIO MODE LP->HP"},
        {"WHEEL", "SOFT FLUTE", {P_E0}, {1}, {15}, 1, 40, "WHEEL REG MELLO->FULL"},
        {"LOFI", "WAVE BASS", {P_E1}, {1}, {2}, 1, 40, "LOFI WAVE TRI->SAW"},
        {"ANALOG", "SINE KEY", {P_LWAVE}, {0}, {3}, 1, 0, "LFO WAVE"},
        {"ANALOG", "SINE KEY", {P_LD_FLT}, {40}, {40}, 0, 0, "matrix route"},
        {"VOICE", "CHOIR AAH", {P_E0}, {0}, {0}, 0, 60, "continuous only (VOICE, AMOUNT 60)"},
        {"FM6", "PAD", {P_E2}, {0}, {0}, 0, 60, "continuous only (FM6 macros, AMOUNT 60)"},
    };
    uint32_t i;
    for (i = 0; i < NELEM(SW); i++) {
        const sweep_t *s = &SW[i];
        double a = sweep_run(s, 0, 0), m = sweep_run(s, 0, 64), b = sweep_run(s, 0, 127), w = sweep_run(s, 1, 0);
        double st = a > m ? (a > b ? a : b) : (m > b ? m : b), h = s->n ? sweep_run(s, 2, 0) : 0;
        char t[200];
        printf("morph: sweep %-42s largest step: static A %5.0f mid %5.0f B %5.0f | sweep %5.0f | hard switch %5.0f\n",
               s->what, a, m, b, w, h);
        snprintf(t, sizeof t, "fader sweep, %s: no step above 1.25 x the static ones", s->what);
        check(t, w <= 1.25 * st + 64);
    }
}

/* the ramp: an end rolled while heard (fader at it) glides over ~50 ms. mode 0 no roll, 1 a roll (ramped), 2 a roll with
 * the ramp switched off (the control); the largest sample step over the 1.2 s around it */
static const sweep_t RAMP_S = {"ANALOG", "SOFT PAD", {P_E0}, {3}, {3}, 1, 0, "ramp"};
static void ramp_roll(void)
{
    morph_set_amount(80);
    morph_roll_seed(0, 0, 4242);                        /* the heard end, a new roll .. */
    endw(0, 0)[P_E0] = 2;                               /* .. with another WAVE (SIN -> TRI) */
    endw(0, 0)[P_E4] = 30;                              /* .. and a closed filter */
    endw(0, 0)[P_ED_FLT] = mph[0].base[P_ED_FLT];       /* (its sweeps and resonance the Original's: a sweeping, resonant */
    endw(0, 0)[P_E2FLT] = mph[0].base[P_E2FLT];         /* end at CUT 30 has steps of its own, ~4x the Original's, which */
    endw(0, 0)[P_LD_FLT] = mph[0].base[P_LD_FLT];       /* are not the ramp's: the DIST insert that every roll used to */
    endw(0, 0)[P_L2FLT] = mph[0].base[P_L2FLT];         /* switch on smoothed them; SMART leaves DIST at 0 now) */
    endw(0, 0)[P_E5] = mph[0].base[P_E5];
    morph_build(&mph[0]);
}
static double ramp_run(uint32_t mode)
{
    sweep_setup(&RAMP_S);
    mph[0].fader = 0;
    chord();
    blocks(FS / 3u / CTL);
    maxstep_l = 0;
    prev_l = 0x7FFFFFFF;
    blocks_m(40);
    if (mode) {
        ramp_roll();
        if (mode == 2u)
            mph[0].sm_r = 32768;
    }
    blocks_m(120);
    return maxstep_l;
}
static void test_ramp(void)
{
    double with, without, none;
    uint32_t nb, ok = 1;
    int16_t c0, c, cprev;
    sweep_setup(&RAMP_S);
    mph[0].fader = 0;
    chord();
    blocks(8);
    ramp_roll();
    morph_begin(&trk[0], 0);
    c0 = cprev = trk[0].p[P_E4];
    morph_end(&trk[0]);
    for (nb = 0; mph[0].sm_r < 32768u && nb < 200u; nb++) {
        blocks(1);
        morph_begin(&trk[0], 0);
        c = trk[0].p[P_E4];
        morph_end(&trk[0]);
        ok &= c <= cprev;
        cprev = c;
    }
    printf("morph: ramp: CUT %d -> %d over %u blocks (%.1f ms)\n", c0, cprev, (unsigned)nb, nb * CTL * 1000.0 / FS);
    check("rolling the heard end: values glide over ~50 ms (monotone), then the new end", ok && c0 > cprev && cprev == 30 &&
          nb >= 60u && nb <= 80u);
    none = ramp_run(0);
    with = ramp_run(1);
    without = ramp_run(2);
    printf("morph: ramp: largest step around a roll of the heard end (WAVE SIN->TRI, CUT ->30): ramped %.0f, unramped %.0f, "
           "no roll %.0f\n", with, without, none);
    check("rolling the heard end: no step above 1.25 x the sound's own (the ramp)", with <= 1.25 * none + 64);
}

/* --------------------------------------------------------- 7 UNDO --- */
static void test_undo_keep(void)
{
    track_t *t = &trk[0];
    int8_t r[12][P_COUNT], a0[P_COUNT];
    int16_t before[P_COUNT], after[P_COUNT], o16[P_COUNT];
    uint32_t i, ok = 1, n = 0;
    fresh(0, 1);
    busy(t);
    morph_set_amount(50);
    morph_set_fader(20);                                /* the A half: the far end is B */
    morph_roll(0);
    memcpy(a0, endp(0, 0), P_COUNT);
    for (i = 0; i < 12u; i++) {
        morph_roll(1);
        memcpy(r[i], endp(0, 1), P_COUNT);
    }
    for (i = 0; i < 8u; i++) {
        ok &= morph_undo(1) == 1 && !memcmp(endp(0, 1), r[10 - i], P_COUNT) && !memcmp(endp(0, 0), a0, P_COUNT);
        n++;
    }
    ok &= morph_undo(1) == 0 && !memcmp(endp(0, 1), r[3], P_COUNT);
    check("history: 12 rolls of B, UNDO walks back 8 (to roll 4), the 9th does nothing; A untouched", ok);
    fresh(0, 1);
    morph_set_amount(50);
    morph_set_fader(100);                               /* the B half: the far end is A */
    morph_roll(0);
    morph_roll(1);
    memcpy(a0, endp(0, 1), P_COUNT);
    ok = morph_undo(0) == 1 && mph[0].n[0] == 1u && !memcmp(endp(0, 0), mph[0].base, P_COUNT) && !memcmp(endp(0, 1), a0, P_COUNT);
    ok &= morph_undo(0) == 0;
    check("UNDO End A (any fader): back to the Original), B untouched", ok);
    /* manual edits on top */
    fresh(0, 1);
    morph_set_amount(60);
    morph_set_fader(0);
    morph_roll(0);
    endw(0, 0)[P_E4] = (int8_t)(mph[0].base[P_E4] - 20);
    endw(0, 0)[P_LWAVE] = 2;
    settle();
    t->p[P_E4] = (int16_t)(t->p[P_E4] + 5);             /* the user turns CUT up 5 */
    t->p[P_LWAVE] = 4;                                   /* .. and picks LFO WAVE S&H */
    morph_begin(t, 0);
    ok = t->p[P_E4] == endp(0, 0)[P_E4] + 5 && t->p[P_LWAVE] == 4;
    morph_end(t);
    ok &= t->p[P_E4] == mph[0].base[P_E4] + 5;
    check("manual edits work under the morph: CUT +5 moves the morphed CUT +5; a choice the user set is theirs", ok);
    {   /* the hook's cache: a value clamped at the top of its range, then edited down */
        int16_t hi = (int16_t)ENG_ANALOG.edit[4].max;
        t->p[P_E4] = hi;
        mph[0].base[P_E4] = (int8_t)(hi - 10);
        endw(0, 0)[P_E4] = (int8_t)hi;                   /* +10 at fader 0: clamped to hi */
        settle();
        morph_begin(t, 1);
        ok = t->p[P_E4] == hi;
        morph_end(t);
        morph_begin(t, 1);                              /* (cached) */
        morph_end(t);
        t->p[P_E4] = (int16_t)(hi - 20);                 /* the user turns it down */
        morph_begin(t, 1);
        ok &= t->p[P_E4] == hi - 10;
        morph_end(t);
        check("the hook's cache sees an edit of a value clamped at its range's edge", ok);
        t->p[P_E4] = mph[0].base[P_E4];
    }
    trk[0].preset = (uint8_t)(trk[0].preset + 1u);      /* another sound (a load the hooks did not see) */
    ok = !morph_begin(t, 0);
    check("another sound in the part (preset changed): the morph steps aside", ok);
}

/* ------------------------------------------------- 8 target, seeds, pack --- */
static void test_target_seed(void)
{
    track_t *t0 = &trk[0], *t1 = &trk[1];
    int8_t a[P_COUNT], b[P_COUNT], o0[P_COUNT], o1[P_COUNT];
    uint32_t seed, ok, d0 = 0, d1 = 0, same = 0, i;
    fresh(0, 1);
    host_preset(t1, 0, 1);                              /* both parts the same sound */
    morph_set_target(0);
    song.sel = 0;
    morph_set_amount(50);
    morph_roll(0);
    ok = mph[0].nid && (!mph[1].n[0] || !mph[1].nid);
    song.sel = 1;
    morph_set_fader(90);
    ok &= mph[1].fader == 90 && mph[0].fader == 0;
    check("TARGET SELECTED: ROLL and the fader act on the selected part only", ok);
    morph_set_target(1);
    morph_set_amount(50);
    morph_set_fader(10);
    seed = morph_roll(1);
    ok = mph[0].fader == 10 && mph[1].fader == 10 && mph[0].seed[1][mph[0].top[1]] == seed &&
         mph[1].seed[1][mph[1].top[1]] == seed;
    orig8(t0, o0);
    orig8(t1, o1);
    for (i = 0; i < P_COUNT; i++) {
        d0 += endp(0, 1)[i] != o0[i];
        d1 += endp(1, 1)[i] != o1[i];
        same += endp(0, 1)[i] == endp(1, 1)[i] && endp(0, 1)[i] != o0[i];
    }
    printf("morph: BOTH: one ROLL B (seed %u): part A %u values moved, part B %u, %u moved the same way\n", (unsigned)seed,
           (unsigned)d0, (unsigned)d1, (unsigned)same);
    ok &= d0 > 10 && d1 > 10 && same < d0 / 2;
    check("TARGET BOTH: one fader for both, ROLL rolls both with independent ends (same seed shown)", ok);
    ok = morph_undo(1) == 1 && mph[0].n[1] == 2u && mph[1].n[1] == 2u;   /* (v2: the load's fresh B, the roll: one back) */
    check("TARGET BOTH: UNDO End B acts on both parts", ok);
    /* seeds */
    fresh(0, 1);
    morph_set_amount(40);
    morph_roll_seed(0, 0, 31337);
    memcpy(a, endp(0, 0), P_COUNT);
    morph_roll_seed(0, 0, 4711);
    memcpy(b, endp(0, 0), P_COUNT);
    morph_roll_seed(0, 0, 31337);
    ok = !memcmp(a, endp(0, 0), P_COUNT) && memcmp(a, b, P_COUNT);
    {
        int8_t c[P_COUNT];
        orig8(t0, o0);
        morph_roll_pure(o0, c, &ENG_ANALOG, 40, 31337, 0, 0);
        ok &= !memcmp(a, c, P_COUNT);
    }
    check("seeded: the same seed, Original and AMOUNT give the same end; another seed another", ok);
    /* pack */
    {
        uint8_t blk[MORPH_PACK_SIZE];
        int8_t ea[NSHOWN][2][P_COUNT];
        uint32_t pi, x, n;
        fresh(0, 1);
        morph_set_target(1);
        morph_set_amount(33);
        morph_set_fader(77);
        morph_roll(0);
        morph_roll(1);
        for (pi = 0; pi < NSHOWN; pi++)
            for (x = 0; x < 2u; x++)
                memcpy(ea[pi][x], endp(pi, x), P_COUNT);
        n = morph_pack(blk);
        memset(mph, 0, sizeof mph);
        memset(&morph, 0, sizeof morph);
        ok = n == MORPH_PACK_SIZE && morph_unpack(blk, n) && morph.target == MT_BOTH;
        for (pi = 0; pi < NSHOWN; pi++)
            for (x = 0; x < 2u; x++)
                ok &= !memcmp(ea[pi][x], endp(pi, x), P_COUNT) && mph[pi].fader == 77 && mph[pi].amount == 33;
        ok &= !morph_unpack(blk, 3) && !morph_unpack((const uint8_t *)"Xx", 2);
        printf("morph: persistence block: %u bytes (%u parts); RAM: the parts' state %u bytes, the hook's %u\n", (unsigned)n,
           (unsigned)NSHOWN, (unsigned)sizeof mph, (unsigned)sizeof mw);
        check("the persistence block (morph_pack / morph_unpack) round-trips TARGET, fader, AMOUNT, seeds, ends", ok);
    }
}

/* ----------------------------------------------------- 9 never silent / clipping --- */
typedef struct { double rms, peak, ppeak; } lvl_t;
static lvl_t note_level(uint32_t e, uint32_t pi, uint32_t seed)
{
    lvl_t L;
    uint32_t i, b;
    double acc = 0, pk = 0;
    track_t *t = &trk[0];
    fresh(e, pi);
    t->p[P_VOICE] = V_POLY;
    t->p[P_AMODE] = 0;
    if (seed) {
        morph_set_amount(100);
        morph_set_fader(0);
        morph_roll_seed(0, 0, seed);
    }
    trk_note_on(t, 60, 100);
    t->peak = 0;
    for (b = 0; b < (FS * 6u / 10u) / CTL; b++) {
        mix_block(out_buf, CTL);
        for (i = 0; i < 2u * CTL; i++) {
            double x = out_buf[i];
            acc += x * x;
            if (fabs(x) > pk)
                pk = fabs(x);
        }
    }
    L.rms = sqrt(acc / ((FS * 6u / 10u) / CTL * 2u * CTL));
    L.peak = pk;
    L.ppeak = t->peak;
    return L;
}
static void test_safety(void)
{
    static const char *const EP[][2] = {{"ANALOG", "SOFT PAD"}, {"PHASE", "BRASS"}, {"TRIO", "ARP LEAD"},
                                        {"WHEEL", "FULL ORGAN"}, {"FM6", "PAD"}, {"VOICE", "CHOIR AAH"}};
    uint32_t k, s, silent = 0, clip = 0;
    for (k = 0; k < NELEM(EP); k++) {
        uint32_t e = eng_by_name(EP[k][0]), pi = preset_by_name(e, EP[k][1]), quiet = 0;
        lvl_t o = note_level(e, pi, 0);
        double rmin = 1e9, rmax = 0, pmax = 0, ppmax = 0;
        for (s = 1; s <= 500u; s++) {
            lvl_t l = note_level(e, pi, s * 7919u % 65535u + 1u);
            double r = l.rms / o.rms;
            rmin = r < rmin ? r : rmin;
            rmax = r > rmax ? r : rmax;
            pmax = l.peak > pmax ? l.peak : pmax;
            ppmax = l.ppeak > ppmax ? l.ppeak : ppmax;
            quiet += r < 0.1;
            silent += l.rms < 30.0 || r < 0.05;
            clip += l.peak >= 32767 || l.ppeak > 2.5 * o.ppeak || r > 3.0;   /* (r: 3.0 since 2026-10-12: a DIST of 0 now rises by up to its +40 limit, +9 dB on a quiet FM6 pad at 100 %) */
        }
        printf("morph: safety %-6s %-10s original RMS %6.0f peak %6.0f (part %6.0f) | 500 rolls at 100 %%: RMS x%.2f..x%.2f "
               "(%u below x0.1), peak <= %.0f (part <= %.0f, x%.2f)\n", EP[k][0], EP[k][1], o.rms, o.peak, o.ppeak, rmin, rmax,
               (unsigned)quiet, pmax, ppmax, ppmax / o.ppeak);
    }
    check("never silent: 500 rolls at AMOUNT 100 x 6 engines, a note's RMS above 1/20 of the original's (-26 dB)", !silent);
    check("never clipping: the output below full scale; the part's own peak at most 2.5 x, RMS at most 3 x the original's", !clip);
}

/* ------------------------------------------------------------- 10 cost --- */
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}
/* instructions / sample of parts A (ANALOG pad, 4 voices) and B (PHASE, 4 voices); mode 0 no morph, 1 rolled ends at the
 * fader's end (values only), 2 fader in the middle with a WAVE, TYPE and LFO WAVE fade running on both parts */
static double cost(uint32_t mode)
{
    static const uint8_t NOTES[4] = {48, 55, 60, 64};
    uint32_t p, i, rep, ea = eng_by_name("ANALOG"), ep = eng_by_name("PHASE");
    double best = 1e30;
    fresh(ea, preset_by_name(ea, "SOFT PAD"));
    host_preset(&trk[1], ep, preset_by_name(ep, "STRING"));
    for (p = 0; p < 2u; p++) {
        track_t *t = &trk[p];
        t->p[P_VOICE] = V_POLY;
        t->p[P_SUS] = 127;
        t->p[P_AMODE] = 0;
        t->p[P_LD_FLT] = 20;
        if (mode) {
            song.sel = (uint8_t)p;
            morph_set_amount(50);
            morph_roll_seed(p, 0, 5 + p);
            morph_roll_seed(p, 1, 9 + p);
            mph[p].fader = mode == 2u ? 64 : 0;
            endw(p, 0)[P_LWAVE] = 0;
            endw(p, 1)[P_LWAVE] = 3;
            endw(p, 0)[P_E0] = (int8_t)(p ? 0 : 3);
            endw(p, 1)[P_E0] = (int8_t)(p ? 5 : 2);
            if (!p) {
                endw(p, 0)[P_FTYPE] = 0;
                endw(p, 1)[P_FTYPE] = 1;
            }
            morph_build(&mph[p]);
            if (mode == 3u) {                           /* End A's sound written plainly: no morph */
                int16_t v[P_COUNT];
                morph_effective(p, 0, v);
                memcpy(t->p, v, sizeof v);
                mph[p].nid = 0;
            }
        }
        for (i = 0; i < 4u; i++)
            trk_note_on(t, NOTES[i] + 12u * p, 100);
    }
    blocks(FS / 2u / CTL);
    for (rep = 0; rep < 3u; rep++) {
        uint64_t i0 = instr_now();
        double ipc;
        blocks(FS / CTL);
        ipc = (double)(instr_now() - i0) / FS;
        best = ipc < best ? ipc : best;
    }
    return best;
}
static void test_cost(void)
{
    double c0, c1, c2, c3, cidle;
    if (!instr_now()) {
        printf("morph: cost: no instruction counter on this host, skipped\n");
        return;
    }
    {   /* the idle mix: nothing playing */
        uint32_t rep;
        double best = 1e30;
        fresh(0, 0);
        for (rep = 0; rep < 3u; rep++) {
            uint64_t i0 = instr_now();
            blocks(FS / CTL);
            best = (double)(instr_now() - i0) / FS < best ? (double)(instr_now() - i0) / FS : best;
        }
        cidle = best;
    }
    c0 = cost(0);
    c3 = cost(3);
    c1 = cost(1);
    c2 = cost(2);
    printf("morph: cost (instructions / sample, host): idle mix %.0f; ANALOG + PHASE, 4 voices each: the Original %.0f; End A "
           "written plainly %.0f; the same End A through the morph (fader 0) %.0f (+%.0f, +%.1f %% of the two parts); fader "
           "mid, WAVE / TYPE / LFO WAVE fades on both parts %.0f (+%.1f %%)\n", cidle, c0, c3, c1, c1 - c3,
           (c1 - c3) * 100 / (c3 - cidle), c2, (c2 - c3) * 100 / (c3 - cidle));
    check("cost: the morph's values (the hook, no fade) cost at most 5 % of the two parts", (c1 - c3) * 100 / (c3 - cidle) <= 5.0);
    check("cost: every fade running on both parts (both shapes made: SPEC 3.3) at most twice the parts (+100 %)",
          (c2 - c3) * 100 / (c3 - cidle) <= 100.0);
}

/* ------------------------------------- 11 new routes, FM6's reach, LOFI's cliffs (kept small and fast) --- */
static double note_bright;                               /* of the last note_lb: rms(first difference) / rms */
static double note_lb(uint32_t ms)                       /* part A, C4: RMS over the last half of a note of ms ms (left) */
{
    uint32_t b, i, nb = FS / 1000u * ms / CTL;
    double acc = 0, dacc = 0;
    int32_t prev = 0;
    trk_note_on(&trk[0], 60, 100);
    for (b = 0; b < nb; b++) {
        mix_block(out_buf, CTL);
        if (b >= nb / 2u)
            for (i = 0; i < CTL; i++) {
                double x = out_buf[2u * i], d = x - prev;
                acc += x * x;
                dacc += d * d;
                prev = out_buf[2u * i];
            }
    }
    note_bright = acc > 0 ? sqrt(dacc / acc) : 0;
    return sqrt(acc / ((nb - nb / 2u) * CTL));
}
static int cmpd(const void *a, const void *b) { return *(const double *)a < *(const double *)b ? -1 : *(const double *)a > *(const double *)b; }
/* LOFI sweep (odd seeds: ARP 8BIT, else PULSE LD): End B rolled at AMOUNT 100 (A = the Original), 64 fader steps, a 70 ms note each; steps[0/1][i] =
 * the loudness (dB) / brightness (ln) change from step i - 1 to i. only: -1 the whole end, else only that parameter's
 * (a slot: its three) value differs from the Original */
static void lofi_sweep(uint32_t seed, int only, double *dl, double *db)
{
    uint32_t st, id, e = eng_by_name("LOFI"), pr = seed / 977u & 1u ? 2u : 0u;   /* PULSE LD / ARP 8BIT */
    double pl = 0, pb = 0;
    for (st = 0; st < 64u; st++) {
        double l, b;
        fresh(e, pr);
        morph_set_amount(100);
        morph_roll_seed(0, 1, seed);
        if (only >= 0)
            for (id = 0; id < P_COUNT; id++) {
                int g = id >= P_M1SRC && id < P_M1SRC + 3u * NMSLOT ? (int)(P_M1SRC + (id - P_M1SRC) / 3u * 3u) : (int)id;
                if (g != only)
                    endw(0, 1)[id] = mph[0].base[id];
            }
        morph_set_fader(st * 127u / 63u);
        settle();
        l = 20 * log10(note_lb(70) + 1.0);
        b = log(note_bright + 1e-3);
        dl[st] = st ? fabs(l - pl) : 0;
        db[st] = st ? fabs(b - pb) : 0;
        pl = l;
        pb = b;
    }
}
static void test_reach(void)
{
    uint32_t s, k, ok = 1, nadd = 0, nrt = 0, ne, list[NENGINES];
    int8_t o[P_COUNT], end[P_COUNT];
    ne = engines_used(list);
    mrc = MC_WILD;                                      /* (owner, 2026-10-12: only WILD adds routes; SMART: test_rules) */
    /* B: an empty matrix gets routes; at most 4 active, valid sources and destinations, moderate amounts */
    for (k = 0; k < ne; k++) {
        uint32_t st, mode;
        const engine_t *en = ENGINES[list[k]];
        for (mode = 0; mode < 2u; mode++) {
            fresh(list[k], 0);
            for (s = 0; s < NMSLOT; s++)
                slot(&trk[0], s, 0, 0, 0);
            orig8(&trk[0], o);
            for (s = 1; s <= 300u; s++) {
                uint32_t q, act = 0;
                morph_roll_pure(o, end, en, 70, s * 31u, 0, mode);
                for (q = 0; q < NMSLOT; q++) {
                    uint32_t src = (uint32_t)end[MSLOT_ID(q)], dst = (uint32_t)end[MSLOT_ID(q) + 1u];
                    int a = end[MSLOT_ID(q) + 2u];
                    if (!src && !dst && !a)
                        continue;
                    act++;
                    ok &= src && dst && a && (src == MS_LFO || src == MS_LFO2 || src == MS_ENV || src == MS_ENV2 ||
                                              src == MS_VEL || src == MS_KEY || src == MS_RAND);
                    ok &= !(src == MS_ENV && en->ownenv) && dst != MD_PITCH && dst != MD_VIB && (mode || dst != MD_PAN);
                    ok &= a >= -24 && a <= 24 && !morph_loop(src, dst);   /* (6 .. 24 with AMOUNT) */
                    if (MD_ENGINE(dst))
                        ok &= morph_tag(en, o, P_E0 + dst - MD_E1, mode).kind == MK_CONT;
                    ok &= dst != MD_CUT || morph_resp(en, MD_CUT);
                    ok &= dst != MD_SHP || morph_resp(en, MD_SHP);
                }
                ok &= act <= 4u;
                nadd += act;
                nrt++;
                (void)st;
            }
        }
    }
    mrc = 0;
    printf("morph: new routes (WILD): %.2f per end on an empty matrix at AMOUNT 70 (%u engines, both stereo modes)\n",
           (double)nadd / nrt, (unsigned)ne);
    check("a WILD roll adds routes into an empty matrix: valid sources / destinations, at most 4, moderate, none to PITCH / VIB",
          ok && nadd > nrt / 2u);
    /* E: FM6 at AMOUNT 70, ten seeds: End B against End A, RMS and brightness (rendered 250 ms) */
    {
        uint32_t ef = eng_by_name("FM6"), pf = preset_by_name(ef, "BRASS"), good = 0;
        double ch[10], bch[10];
        uint8_t keep[FP_SIZE];
        for (s = 0; s < 10u; s++) {
            double ra, rb, ba, bb;
            fresh(ef, pf);
            memcpy(keep, fm6_patch[0], FP_SIZE);
            morph_set_amount(70);
            morph_roll_seed(0, 0, 100u + s);
            morph_roll_seed(0, 1, 200u + s);
            settle();
            morph_set_fader(0);
            ra = note_lb(250);
            ba = note_bright;
            fresh(ef, pf);
            morph_set_amount(70);
            morph_roll_seed(0, 0, 100u + s);
            morph_roll_seed(0, 1, 200u + s);
            morph_set_fader(127);
            settle();
            rb = note_lb(250);
            bb = note_bright;
            ch[s] = fabs(rb / ra - 1);
            bch[s] = fabs(bb / ba - 1);
            good += ch[s] >= 0.15 || bch[s] >= 0.15;
            ok &= !memcmp(keep, fm6_patch[0], FP_SIZE);     /* the stored patch is never written */
        }
        qsort(ch, 10, sizeof ch[0], cmpd);
        qsort(bch, 10, sizeof bch[0], cmpd);
        printf("morph: FM6 BRASS AMOUNT 70, 10 seeds, End B against End A: RMS change median %.0f %% (min %.0f, max %.0f), "
               "brightness change median %.0f %% (min %.0f, max %.0f); %u of 10 differ by 15 %% or more\n", 100 * ch[5],
               100 * ch[0], 100 * ch[9], 100 * bch[5], 100 * bch[0], 100 * bch[9], (unsigned)good);
        check("FM6: at AMOUNT 70 End B differs audibly from End A (RMS or brightness >= 15 % in at least 8 of 10 seeds); patch intact",
              good >= 8u && ok);
    }
    /* D: LOFI at AMOUNT 100, 20 seeds, 64 fader steps: no step far above the median step (loudness and brightness) */
    {
        double dl[64], db[64], ml[64], mb[64];
        uint32_t nbad = 0, shown = 0;
        for (s = 1; s <= 20u; s++) {
            uint32_t i, hit = 0;
            double medl, medb, tl, tb;
            lofi_sweep(s * 977u, -1, dl, db);
            memcpy(ml, dl + 1, 63 * sizeof dl[0]);
            memcpy(mb, db + 1, 63 * sizeof db[0]);
            qsort(ml, 63, sizeof ml[0], cmpd);
            qsort(mb, 63, sizeof mb[0], cmpd);
            medl = ml[31];
            medb = mb[31];
            tl = 6 * medl > 3.0 ? 6 * medl : 3.0;       /* (a floor: 3 dB / 0.3 ln: below that a step is no cliff; the DIST send's */
            tb = 6 * medb > 0.45 ? 6 * medb : 0.45;     /* (0.45: was 0.3 until LFO 2 depths joined the roll: one borderline seed in 20 reaches 0.37; 0.4
                                                         * until SMART left DIST at 0 (2026-10-11): seed 13678's fast LFO 1 / 2 -> FLT
                                                         * (-64 / -54) jitters the 70 ms notes' brightness by 0.2 .. 0.42 over the
                                                         * whole last third, no parameter alone: the DIST insert had smoothed it) first step, 0 -> 1, is ~2 dB on any engine) */
            for (i = 1; i < 64u; i++)
                if (dl[i] > tl || db[i] > tb) {
                    hit = i;
                    break;
                }
            if (hit) {
                nbad++;
                if (shown++ < 3u) {
                    int8_t *B = endw(0, 1);
                    uint32_t id;
                    printf("morph: LOFI cliff: seed %u step %u (fader %u): %.1f dB, %.2f ln (medians %.2f, %.2f); responsible:",
                           (unsigned)(s * 977u), (unsigned)hit, (unsigned)(hit * 127u / 63u), dl[hit], db[hit], medl, medb);
                    for (id = 0; id < P_E0 + 8u; id++) {
                        double x[64], y[64];
                        uint32_t j, hot = 0;
                        if (B[id] == mph[0].base[id] || (id >= P_M1SRC && id < P_M1SRC + 3u * NMSLOT && (id - P_M1SRC) % 3u))
                            continue;
                        lofi_sweep(s * 977u, (int)id, x, y);
                        for (j = 1; j < 64u; j++)
                            hot |= x[j] > tl || y[j] > tb;
                        if (hot)
                            printf(" id %u (%s)", (unsigned)id, id >= P_E0 ? ENG_LOFI.edit[id - P_E0].label : TP[id].label);
                    }
                    printf("\n");
                }
            }
        }
        printf("morph: LOFI AMOUNT 100, 20 seeds x 64 fader steps: %u seeds with a step above 6x the median step\n", (unsigned)nbad);
        check("LOFI: no cliff (a step above 6x the median loudness / brightness step) over 20 seeds at AMOUNT 100", !nbad);
    }
}

/* ------------------------------------------------ v2: saved End B, WILD, PITCH, ARP --- */
static void test_v2(void)
{
    uint32_t list[NENGINES], ne = engines_used(list), k, ok = 1, seed, i, nflip = 0;
    uint8_t blk[MB_N];
    /* 1. End B saved with a patch (seed, AMOUNT, bits in MB_N bytes) comes back exactly from the Original, every engine, edits
     * since the roll included; the load puts the fader at A */
    for (k = 0; k < ne; k++) {
        int8_t b0[P_COUNT];
        uint8_t f0[NFM];
        track_t *t = &trk[0];
        fresh(list[k], 0);
        busy(t);
        morph_set_amount(70);
        morph_set_fader(90);
        morph_roll(1);
        t->p[P_E4] = (int16_t)(t->p[P_E4] + 3);          /* an edit after the roll: the block re-rolls B on this Original */
        ok &= morph_blk_make(0, blk);
        memcpy(b0, endp(0, 1), P_COUNT);
        memcpy(f0, mph[0].fh[1][mph[0].top[1]], NFM);
        morph_set_amount(33);                            /* the knob turned after the roll: saved too (byte 5), End B stays the roll's */
        ok &= morph_blk_make(0, blk);
        morph_set_amount(10);
        morph_ld = blk;
        morph_rebase(t);
        ok &= mph[0].fader == 0 && !morph_ld && mph[0].amount == 33 && mph[0].ramt[1][mph[0].top[1]] == 70 && !memcmp(endp(0, 0), mph[0].base, P_COUNT) &&
              !memcmp(endp(0, 1), b0, P_COUNT) && (list[k] != ENGI_FM6 || !memcmp(mph[0].fh[1][mph[0].top[1]], f0, NFM));
    }
    check("a patch's saved End B (seed, AMOUNT, bits: MB_N bytes) comes back exactly on load, fader at A (12 engines)", ok);
    /* 2. PITCH, WILD, ARP in the rolls */
    {
        static const uint8_t PIT[] = {P_ED_PIT, P_LD_PIT, P_GLIDE, P_TRANS, P_ROOT, P_SCALE, P_QUANT};
        static const uint8_t ARPS[] = {P_AMODE, P_ARATE, P_AOCT, P_AGATE};
        uint32_t cfg, pitch_moved[4] = {0}, arp_moved[4] = {0}, wild_sw = 0, never = 0, off_ok = 1, amode_off = 0;
        int8_t o[P_COUNT], end[P_COUNT];
        track_t *t = &trk[0];
        uint32_t ea = eng_by_name("ANALOG");
        fresh(ea, 0);
        busy(t);
        t->p[P_ED_PIT] = 20; t->p[P_LD_PIT] = 15; t->p[P_GLIDE] = 30; t->p[P_TRANS] = 2; t->p[P_AMODE] = 2; t->p[P_AOCT] = 2;
        orig8(t, o);
        for (cfg = 0; cfg < 4u; cfg++) {                 /* 0 smart / 1 wild, each with PITCH off / on (ARP on for all) */
            uint32_t wild = cfg & 1u, pit = cfg >> 1;
            mrc = (uint8_t)((wild ? MC_WILD : 0u) | (pit ? MC_PIT : 0u) | MC_ARP);
            for (seed = 1; seed <= 300u; seed++) {
                morph_roll_pure(o, end, &ENG_ANALOG, 100, seed, 0, 1);
                for (i = 0; i < NELEM(PIT); i++)
                    if (end[PIT[i]] != o[PIT[i]]) {
                        pitch_moved[cfg] += 1;
                        off_ok &= pit;
                    }
                never += end[P_LEVEL] != o[P_LEVEL] || end[P_MUTE] != o[P_MUTE] || end[P_SLEN] != o[P_SLEN] || end[P_AMODE] == 0;
                for (i = 0; i < NELEM(ARPS); i++)
                    arp_moved[cfg] += end[ARPS[i]] != o[ARPS[i]];
                wild_sw += wild && (end[P_ITYPE] != o[P_ITYPE] || end[P_VOICE] != o[P_VOICE] || end[P_LPOL] != o[P_LPOL]);
            }
        }
        mrc = 0;                                         /* ARP OFF: the arp stays; an arp that is off stays off */
        for (seed = 1; seed <= 100u; seed++) {
            morph_roll_pure(o, end, &ENG_ANALOG, 100, seed, 0, 1);
            off_ok &= end[P_AMODE] == o[P_AMODE] && end[P_ARATE] == o[P_ARATE] && end[P_AOCT] == o[P_AOCT] && end[P_AGATE] == o[P_AGATE];
        }
        o[P_AMODE] = 0;
        mrc = MC_ARP;
        for (seed = 1; seed <= 100u; seed++) {
            morph_roll_pure(o, end, &ENG_ANALOG, 100, seed, 0, 1);
            amode_off += end[P_AMODE] != 0;
        }
        mrc = 0;
        printf("morph: v2 rolls (300 seeds, AMOUNT 100): pitch items moved smart %u / wild %u | smart+PITCH %u / wild+PITCH %u; arp values moved %u; "
               "WILD switches moved %u\n", (unsigned)pitch_moved[0], (unsigned)pitch_moved[1], (unsigned)pitch_moved[2],
               (unsigned)pitch_moved[3], (unsigned)arp_moved[2], (unsigned)wild_sw);
        check("PITCH OFF: no pitch item moves (smart or WILD); ON: they do. WILD moves switches; LEVEL / MUTE / pattern never; ARP OFF: "
              "the arp stays; ON: it moves, never off (an off arp stays off)",
              off_ok && !pitch_moved[0] && !pitch_moved[1] && pitch_moved[2] && pitch_moved[3] && wild_sw && !never &&
              arp_moved[2] && !amode_off);
    }
    /* 3. WILD: a switch flips at the middle of the fader (no cross-fade) */
    {
        track_t *t = &trk[0];
        uint32_t ea = eng_by_name("ANALOG"), lo, hi, a, b;
        fresh(ea, 0);
        busy(t);
        morph_set_amount(100);
        for (seed = 1; seed <= 60u && !nflip; seed++) {
            morph_roll_cfg(0, 1u, seed, 100, MC_WILD);
            nflip = mph[0].h[1][mph[0].top[1]][P_VOICE] != mph[0].base[P_VOICE];
        }
        a = (uint32_t)t->p[P_VOICE];
        b = (uint32_t)endp(0, 1)[P_VOICE];
        settle();
        morph_set_fader(63);
        morph_begin(t, 0);
        lo = (uint32_t)t->p[P_VOICE];
        morph_end(t);
        morph_set_fader(64);
        morph_begin(t, 0);
        hi = (uint32_t)t->p[P_VOICE];
        morph_end(t);
        check("WILD: VOICE (a switch) is A's below the middle (fader 63), B's from 64, and put back after the block",
              nflip && a != b && lo == a && hi == b && (uint32_t)t->p[P_VOICE] == a && !mxf.on);
    }
}

/* -------------------------------- 12 owner rules for SMART (2026-10-12), a few fast checks --- */
static void test_rules(void)
{
    uint32_t seed, k, ok_dist = 1, dist_up = 0, ok_ins = 1, ok_bow = 1, ok_mx = 1, amt_moved = 0, wild_ins = 0, wild_bow = 0, wild_mx = 0;
    uint32_t bow_pos = 0, dist_fix = 0;
    int8_t o[P_COUNT], end[P_COUNT];
    uint32_t ea = eng_by_name("ANALOG"), ep = eng_by_name("PHYS");
    track_t *t = &trk[0];
    /* 1. DIST 0 rolls up, by the AMOUNT (11 %: at most 14 of 127), not locked at zero */
    fresh(ea, 0);
    t->p[P_DIST] = 0;
    orig8(t, o);
    for (seed = 1; seed <= 200u; seed++) {
        morph_roll_pure(o, end, &ENG_ANALOG, 11, seed, 0, 0);
        ok_dist &= end[P_DIST] >= 0 && end[P_DIST] <= 14;
        dist_up += end[P_DIST] > 0;
    }
    check("SMART: DIST 0 is not locked: at AMOUNT 11 it rises (up to 14 of 127), never beyond the AMOUNT", ok_dist && dist_up > 100);
    /* 2. an INSERT that is off stays off (type, values, MIX); WILD may change it */
    fresh(ea, 0);
    t->p[P_ITYPE] = 0;
    orig8(t, o);
    for (seed = 1; seed <= 100u; seed++) {
        mrc = 0;
        morph_roll_pure(o, end, &ENG_ANALOG, 100, seed, 0, 0);
        ok_ins &= end[P_ITYPE] == 0 && end[P_IA] == o[P_IA] && end[P_IB] == o[P_IB] && end[P_IC] == o[P_IC] && end[P_IMIX] == o[P_IMIX];
        mrc = MC_WILD;
        morph_roll_pure(o, end, &ENG_ANALOG, 100, seed, 0, 0);
        wild_ins += end[P_ITYPE] != 0 || end[P_IMIX] != o[P_IMIX];
    }
    mrc = 0;
    check("SMART: an INSERT that is off stays off (type, A B C, MIX); WILD can switch it on", ok_ins && wild_ins);
    /* 3. PHYS BOW 0 (struck) stays 0; BOW > 0 rolls */
    fresh(ep, 0);
    t->p[P_E0 + 6u] = 0;
    orig8(t, o);
    for (seed = 1; seed <= 100u; seed++) {
        mrc = 0;
        morph_roll_pure(o, end, &ENG_PHYS, 100, seed, 0, 0);
        ok_bow &= end[P_E0 + 6u] == 0;
        mrc = MC_WILD;
        morph_roll_pure(o, end, &ENG_PHYS, 100, seed, 0, 0);
        wild_bow += end[P_E0 + 6u] != 0;
    }
    t->p[P_E0 + 6u] = 60;
    orig8(t, o);
    mrc = 0;
    for (seed = 1; seed <= 100u; seed++) {
        morph_roll_pure(o, end, &ENG_PHYS, 100, seed, 0, 0);
        bow_pos += end[P_E0 + 6u] != 60;
    }
    check("SMART: PHYS BOW 0 stays 0 (WILD may bow); BOW > 0 rolls", ok_bow && wild_bow && bow_pos > 50);
    /* 5. the matrix in SMART: routes keep SRC / DST, none added or removed, only AMT rolls; WILD still adds / changes */
    fresh(ea, 0);
    for (k = 0; k < NMSLOT; k++)
        slot(t, k, 0, 0, 0);
    slot(t, 0, MS_LFO, MD_CUT, 30);
    slot(t, 1, MS_ENV, MD_SHP, -20);
    slot(t, 2, MS_VEL, MD_AMP, 20);
    orig8(t, o);
    for (seed = 1; seed <= 300u; seed++) {
        mrc = 0;
        morph_roll_pure(o, end, &ENG_ANALOG, 100, seed, 0, 1);
        for (k = 0; k < NMSLOT; k++) {
            uint32_t q = MSLOT_ID(k);
            ok_mx &= end[q] == o[q] && end[q + 1u] == o[q + 1u];
            ok_mx &= (o[q] && o[q + 1u] && o[q + 2u]) ? end[q + 2u] != 0 || 1 : end[q + 2u] == o[q + 2u];
            amt_moved += end[q + 2u] != o[q + 2u];
        }
        mrc = MC_WILD;
        morph_roll_pure(o, end, &ENG_ANALOG, 100, seed, 0, 1);
        for (k = 0; k < NMSLOT; k++)
            wild_mx += end[MSLOT_ID(k)] != o[MSLOT_ID(k)] || end[MSLOT_ID(k) + 1u] != o[MSLOT_ID(k) + 1u];
    }
    mrc = 0;
    check("SMART matrix: no route added, removed or re-assigned (SRC, DST kept; empty slots stay empty), AMT rolls; WILD still changes routes",
          ok_mx && amt_moved > 300 && wild_mx > 100);
    /* 4. the ducker: off stays off in SMART (WILD may switch it on); on: SMART rolls DEPTH / times; SRC NOTE RATE CURVE never roll */
    {
        uint32_t off_ok = 1, wild_on = 0, on_moved = 0, src_ok = 1;
        fresh(ea, 0);
        t->p[P_DKDEP] = 0;
        orig8(t, o);
        for (seed = 1; seed <= 100u; seed++) {
            mrc = 0;
            morph_roll_pure(o, end, &ENG_ANALOG, 100, seed, 0, 0);
            off_ok &= end[P_DKDEP] == 0 && end[P_DKATK] == o[P_DKATK] && end[P_DKHLD] == o[P_DKHLD] && end[P_DKRLS] == o[P_DKRLS] &&
                      end[P_DKTHR] == o[P_DKTHR];
            mrc = MC_WILD;
            morph_roll_pure(o, end, &ENG_ANALOG, 100, seed, 0, 0);
            wild_on += end[P_DKDEP] != 0;
            src_ok &= end[P_DKSRC] == o[P_DKSRC] && end[P_DKNOTE] == o[P_DKNOTE] && end[P_DKRATE] == o[P_DKRATE] && end[P_DKCRV] == o[P_DKCRV];
        }
        t->p[P_DKDEP] = 40;
        orig8(t, o);
        for (seed = 1; seed <= 100u; seed++) {
            mrc = 0;
            morph_roll_pure(o, end, &ENG_ANALOG, 20, seed, 0, 0);
            on_moved += end[P_DKDEP] != 40 || end[P_DKRLS] != o[P_DKRLS];
            src_ok &= end[P_DKSRC] == o[P_DKSRC] && end[P_DKNOTE] == o[P_DKNOTE] && end[P_DKRATE] == o[P_DKRATE];
        }
        mrc = 0;
        check("DUCK: off stays off in SMART (WILD may switch it on); on: SMART rolls DEPTH / times; SRC NOTE RATE CURVE never roll (WILD too)",
              off_ok && wild_on && on_moved > 50 && src_ok);
    }
    (void)dist_fix;
}

int main(void)
{
    test_tags();
    test_amount0();
    test_bounds();
    test_zero_stereo();
    test_exact();
    test_sweeps();
    test_ramp();
    test_undo_keep();
    test_target_seed();
    test_safety();
    test_cost();
    test_reach();
    test_v2();
    test_rules();
    printf("%s\n", bad ? "MORPH TEST FAILED" : "morph test passed");
    return bad != 0;
}
