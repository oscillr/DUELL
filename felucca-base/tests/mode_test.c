/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL build step 5: the output MODE (SPEC 5b.1): SPLIT, STEREO, LAYER. On the firmware's sources (the DSP of hostsim.c and
 * the UI against ui_test.c's stubs).  build/host/mode_test   (DUELL/host_tests.sh "mode")
 *   STEREO     bit for bit as before step 5: a busy two-part render (SPREAD, the matrix's PAN, an INSERT CHORUS, every
 *              send, HALL, BASS+; tests/mode_scn.h) hashes to what the sources
 *              before step 5 gave (MODE_STEREO_HASH). (regress's goldens: the same, unchanged)
 *   ISOLATION  SPLIT: a loud part 1 alone (every engine, chords at velocity 127, LEVEL 127, PAN pointing the other way,
 *              SPREAD 127, the matrix LFO -> PAN, DIST, each INSERT type, every send at 127, delay feedback,
 *              ROOM / SPRING / HALL, BASS+; part 2
 *              silent with all of that on too): the right channel is exactly 0 in every sample, and the reverse for
 *              part 2. Then both parts at once (every engine, with
 *              a partner): the left channel equals part 1's alone sample for sample, the right part 2's (no shared
 *              state: the limiters and BASS+ per channel, the buses per part), while the limiter works.
 *   LAYER      a note of part 1 plays part 2 too (the same note, velocity, bend, sustain): the keys (with part 2 selected
 *              too), MIDI channel 1, ROUT SEL; channel 2 not; each part's own arp; the mix is STEREO's (bit for bit the
 *              same as the notes sent to both parts by hand); the shared budget: at most 8 voices, the notes in pairs.
 *   HANGING    a MODE change with keys, MIDI notes, a pedal, an arp held: every note let go, nothing sounds after the
 *              releases; a fuzz of 6000 random events with mode changes (CC 112, the UI) between them: no voice left.
 *   CLICK      the switch SPLIT <-> STEREO: a ~50 ms dip (no sample step above the music's own), STEREO <-> LAYER none.
 *   CC 112     0-42 SPLIT, 43-85 STEREO, 86-127 LAYER on any channel; learned by MIDI LEARN: the user's.
 *   UI         HOME's MODE page (KNOB 1, the others nothing); the MIXER's PAN fixed in SPLIT, live in STEREO.
 *   SETTINGS   the MODE in the settings byte (favorites.factory[15][9]), saved, back after a power cycle (export,
 *              import); the default writes nothing; an unknown code reads SPLIT.
 *   COST       host instructions per sample of a two-part render, STEREO against SPLIT (the folding), idle, the dip. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "mode_scn.h"
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif
#include <sys/wait.h>

#define MODE_STEREO_HASH 0xBB6416D3547A055Cull   /* mode_stereo_scenario(); re-based in 0.2.2: the FX layer's THROW / FLANGER / PHASER left the scenario with the layer (the output idle is regress's goldens, unchanged); was 0x174A9561B9854C15, before that 0xA233C4BC0DC6D894 (the sources before step 5, commit bb60ab7) up to 8f45fc5; re-based at 0d28bf8: a bipolar LFO now swings the whole range of a per-block destination and the scenario routes LFO to PAN */

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    printf("mode: %-104s %s\n", what, ok ? "ok" : "FAIL");
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
#define MAXB (12u * FS / CTL)
static int32_t rec_l[MAXB * CTL], rec_r[MAXB * CTL];   /* a render, both channels */
static uint32_t rec_n;

static void dsp_clean(uint32_t mode)                    /* the DSP as at power-on (silence everywhere), in mode */
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
    mix_bal = 0;                                         /* (BALANCE 0, XFADE 0, SPLIT OFF: the mode page's other three) */
    mix_xf = mix_split = 0;
    vage = 0;                                            /* (the voice clock and the note RAND: from power-on, so two */
    mod_seed = 0x2545F491u;                              /* renders of the same notes are the same) */
    fm1_in.notes = 0;
    kb_prev = 0;
    kb_layer = 0;
    mi_r = mi_w = 0;
    duell_mode_force(mode == DM_SPLIT ? DM_STEREO : DM_SPLIT);   /* (a change of side: every line cleared) */
    duell_mode_force(mode);
    memset(dly_buf, 0, sizeof dly_buf);
    memset(&fx, 0, sizeof fx);
    memset(&fx_b, 0, sizeof fx_b);
    memset(&hl, 0, sizeof hl);
    memset(&hl_b, 0, sizeof hl_b);
    lim_env = lim_env_r = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    memset(lce, 0, sizeof lce);
    lc_l1 = lc_l2 = lc_r1 = lc_r2 = 0;
    memset(&sb, 0, sizeof sb);
    memset(&sb_r, 0, sizeof sb_r);
    fx_lowcut = 0;
    host_tracks_init();
    song.sel = 0;
    song.playing = 0;
    song.grid = 0;
}
static void mblk(uint32_t n)
{
    while (n--) {
        mix_block(ob, CTL);
        if (rec_n < MAXB * CTL)
            for (uint32_t i = 0; i < CTL; i++) {
                rec_l[rec_n] = ob[2u * i];
                rec_r[rec_n++] = ob[2u * i + 1u];
            }
    }
}
static uint32_t eng_of(const char *n) { return scn_eng(n); }
static double dbfs(int32_t a) { return a ? 20.0 * log10((double)a / 32768.0) : -999.0; }
static int32_t peak_of(const int32_t *x, uint32_t n)
{
    int32_t m = 0;
    for (uint32_t i = 0; i < n; i++)
        m = x[i] < 0 && -x[i] > m ? -x[i] : x[i] > m ? x[i] : m;
    return m;
}
static uint32_t vgated(const track_t *t, uint32_t note)   /* voices of t gated on note */
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active && t->v[i].gate && t->v[i].note == note;
    return n;
}
static uint32_t gated_any(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active && t->v[i].gate;
    return n;
}
static uint32_t active_any(void)
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NTRK; p++)
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].active;
    return n;
}
static void midi(uint32_t st, uint32_t ch, uint32_t d1, uint32_t d2)   /* a USB-MIDI packet, as the IN endpoint queues it */
{
    midi_in_event((st >> 4) | (st | ch) << 8 | d1 << 16 | d2 << 24);
}

/* a part with everything that can make stereo or reach the other side turned on */
static const uint8_t INS_TYPES[4] = {IT_CHORUS, IT_FLANGER, IT_PHASER, IT_FUZZ};
static void part_busy(track_t *t, uint32_t e, uint32_t k, int32_t pan)
{
    host_preset(t, e, k);
    t->p[P_LEVEL] = 127;
    t->p[P_VOICE] = k % 3u == 2u ? V_UNISON : V_POLY;
    t->p[P_DETUNE] = 60;
    t->p[P_SPRD] = 127;
    t->p[P_PAN] = (int16_t)pan;
    t->p[P_DIST] = 50;
    t->p[P_ITYPE] = INS_TYPES[k % 4u];
    t->p[P_IA] = 70;
    t->p[P_IB] = 100;
    t->p[P_IC] = 90;
    t->p[P_IMIX] = 127;
    t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 127;
    t->p[P_M1SRC] = MS_LFO;
    t->p[P_M1DST] = MD_PAN;
    t->p[P_M1AMT] = 63;
    t->p[P_LRATE] = 90;
    t->p[P_SUS] = 120;
    t->p[P_REL] = 60;
    t->p[P_AMODE] = 0;
}
static void buses_busy(uint32_t rt)
{
    song.g[G_RTYPE] = (int16_t)rt;
    song.g[G_RSIZE] = 120;
    song.g[G_DFDBK] = 110;
    song.g[G_DMIX] = 127;
    song.g[G_CDEPTH] = 127;
    song.g[G_DTIME] = 1;
    fx_lowcut = 2;                                       /* BASS+: the speaker EQ */
}

/* ------------------------------------------------------------- STEREO --- */
static void t_stereo(void)
{
    uint64_t h = mode_stereo_scenario();
    printf("mode: STEREO scenario hash 0x%016llX (before step 5: 0x%016llX)\n", (unsigned long long)h,
           (unsigned long long)MODE_STEREO_HASH);
    expect("STEREO: the busy two-part render bit for bit as before step 5 (the first render of the process)",
           h == MODE_STEREO_HASH);
}

/* ---------------------------------------------------------- ISOLATION --- */
static const char *const ENGS[] = {"ANALOG", "PHASE", "LOFI", "SAMPLE", "VOICE", "TRIO", "WHEEL", "GRAIN", "PHYS", "NOISE",
                                   "FM6", "SLICE"};
#define NENGS (sizeof ENGS / sizeof ENGS[0])
static int32_t iso_other[MAXB * CTL];
/* one isolation render: part p plays (loud) or not, everything on; the other channel into iso_other; the own channel's
 * peak returned */
static int32_t iso_render(uint32_t e, uint32_t p, uint32_t rt, uint32_t k, uint32_t loud)
{
    static const uint8_t CH[4] = {36, 48, 60, 67};
    uint32_t i;
    dsp_clean(DM_SPLIT);
    buses_busy(rt);
    part_busy(&trk[0], e, k, 63);                        /* (PAN pointing at the other side) */
    part_busy(&trk[1], e, k + 1u, -64);
    rec_n = 0;
    mblk(8);
    if (loud)
        for (i = 0; i < 4u; i++)
            trk_note_on(&trk[p], CH[i] + rt, 127);
    mblk(FS / 2u / CTL);
    mblk(FS / 2u / CTL);
    if (loud)
        for (i = 0; i < 4u; i++)
            trk_note_on(&trk[p], CH[i] + 7u, 127);       /* (a second chord: steals, retriggers) */
    mblk(FS / 2u / CTL);
    for (i = 0; i < 4u; i++) {
        trk_note_off(&trk[p], CH[i] + rt);
        trk_note_off(&trk[p], CH[i] + 7u);
    }
    mblk(FS / 4u / CTL);
    mblk(2u * FS / CTL);                                 /* the tails: delay, reverb */
    for (i = 0; i < rec_n; i++)
        iso_other[i] = p ? rec_l[i] : rec_r[i];
    return peak_of(p ? rec_r : rec_l, rec_n);
}
static void t_isolation(void)
{
    static int32_t quiet[MAXB * CTL];
    uint32_t ei, p, rt, i, ok_zero = 1, ok_same = 1, ok_loud = 1, n = 0;
    int32_t worst = 0, quietest = 0x7FFFFFFF;
    for (ei = 0; ei < NENGS; ei++)
        for (p = 0; p < 2u; p++)
            for (rt = 0; rt < 3u; rt++) {
                uint32_t e = eng_of(ENGS[ei]), k = ei + rt, m;
                int32_t own, other;
                if (str_eq(ENGS[ei], "SLICE") && !e)
                    continue;
                iso_render(e, p, rt, k, 0);              /* the part silent: what the other channel has of its own */
                m = rec_n;
                memcpy(quiet, iso_other, m * sizeof quiet[0]);
                own = iso_render(e, p, rt, k, 1);
                other = peak_of(iso_other, rec_n);
                if (other > worst)
                    worst = other;
                if (own < quietest)
                    quietest = own;
                if (other) {
                    ok_zero = 0;
                    printf("mode:   leak: %s part %u reverb %u: the other channel peaks %d (%.1f dBFS)\n", ENGS[ei], p + 1u,
                           rt, other, dbfs(other));
                }
                if (m != rec_n || memcmp(quiet, iso_other, m * sizeof quiet[0])) {
                    ok_same = 0;
                    printf("mode:   leak: %s part %u reverb %u: the other channel not the same as with the part silent\n",
                           ENGS[ei], p + 1u, rt);
                }
                if (own < 3000)
                    ok_loud = 0;
                n++;
            }
    printf("mode: %u renders of 4.25 s: own channel peaks at least %.1f dBFS; the other channel at most %d\n", n,
           dbfs(quietest), worst);
    expect("SPLIT: part 1 alone, every engine, every effect on: the right channel is 0 in every sample", ok_zero);
    expect("  .. and part 2 alone: the left channel is 0 in every sample (the same renders)", ok_zero);
    expect("  .. the other channel bit for bit as with the part silent (nothing of the part in it)", ok_same);
    expect("  .. the part's own channel loud (above -21 dBFS) in every render", ok_loud);
}

/* a switch STEREO -> SPLIT while part 1 rings with everything on (its reverb and delay tails), then part 1 plays a new chord. The lines are cleared at the switch's
 * silent point, but the master's DC blocker and LOWCUT keep their state (a reset would click: the blocker's output at
 * silence is minus its DC estimate): what the right channel has after the switch is that state's own decay, sub-audio.
 * Checked in two processes from the silent point on, part 1 playing the new chord or not: the right channel the same
 * sample for sample (nothing of part 1's sound after the switch), and the residue a DC drift (no sample step above 8) */
static int32_t max_step(const int32_t *x, uint32_t a, uint32_t b);
static void switch_after(uint32_t play, uint32_t at, uint64_t *h, int32_t *pk, int32_t *st)
{
    static const uint8_t B[4] = {47, 54, 59, 66};
    int fd[2];
    int64_t v[3] = {0, 0, 0};
    pid_t pid;
    if (pipe(fd))
        return;
    pid = fork();
    if (!pid) {
        uint32_t i;
        uint64_t hh = 0xCBF29CE484222325ull;
        if (play)
            for (i = 0; i < 4u; i++)
                trk_note_on(&trk[0], B[i], 127);
        mblk(FS / CTL);
        mblk(FS / CTL);
        for (i = at; i < rec_n; i++)
            hh = (hh ^ (uint32_t)rec_r[i]) * 0x100000001B3ull;
        v[0] = (int64_t)hh;
        v[1] = peak_of(rec_r + at, rec_n - at);
        v[2] = max_step(rec_r, at, rec_n);
        if (play && peak_of(rec_l + at, rec_n - at) < 3000)
            v[0] = 0;                                    /* (the left must sound) */
        if (write(fd[1], v, sizeof v) != (ssize_t)sizeof v)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], v, sizeof v) != (ssize_t)sizeof v)
        v[0] = v[1] = v[2] = 0;
    close(fd[0]);
    waitpid(pid, NULL, 0);
    *h = (uint64_t)v[0];
    *pk = (int32_t)v[1];
    *st = (int32_t)v[2];
}
static void t_switch_iso(void)
{
    static const uint8_t A[4] = {45, 52, 57, 64};
    uint32_t rt, i, ok = 1, okdc = 1, at;
    int32_t worst = 0;
    for (rt = 0; rt < 3u; rt++) {
        uint64_t h1, h0;
        int32_t p1, p0, s1, s0;
        dsp_clean(DM_STEREO);
        buses_busy(rt);
        part_busy(&trk[0], eng_of("ANALOG"), rt, 0);
        part_busy(&trk[1], eng_of("FM6"), 1, 0);
        rec_n = 0;
        for (i = 0; i < 4u; i++)
            trk_note_on(&trk[0], A[i], 127);
        mblk(FS / CTL);
        duell_mode_req = DM_SPLIT;
        at = 0;
        while (!at) {                                    /* (the silent point: the mix switches) */
            mblk(1);
            if (dm_split)
                at = rec_n;
        }
        switch_after(1, at, &h1, &p1, &s1);
        switch_after(0, at, &h0, &p0, &s0);
        ok &= h1 && h1 == h0;
        okdc &= s1 <= 8;
        worst = p1 > worst ? p1 : worst;
        printf("mode:   reverb %u: after the switch the right channel: peak %d (%.1f dBFS), largest step %d\n", rt, p1,
               dbfs(p1), s1);
    }
    expect("STEREO -> SPLIT while part 1 rings: after the switch the right channel is the same whether part 1 plays or not",
           ok);
    expect("  .. what it has is the master's DC blocker / LOWCUT state decaying (sub-audio: no step above 8)", okdc);
}

/* both parts at once = each alone, per channel, sample for sample. Two things are global by design and are not sound: the
 * voice clock (voice.c vage: ANALOG's and TRIO's noise seed from a voice's age) and the note RAND (mod.c mod_seed). So that
 * a part alone gets the same seeds as with the other playing, the other part's notes go to a stand-in at LEVEL 0 (track
 * 3: rendered, its output exactly 0) in the same order. which: bit 0 part 1 plays, bit 1 part 2 */
static void two_part(uint32_t e1, uint32_t e2, uint32_t rt, uint32_t which, int32_t *l, int32_t *r)
{
    static const uint8_t A[3] = {40, 52, 59}, B[3] = {64, 71, 76};
    uint32_t i, k;
    dsp_clean(DM_SPLIT);
    buses_busy(rt);
    part_busy(&trk[0], e1, 0, 30);
    part_busy(&trk[1], e2, 1, -30);
    trk[0].p[P_VOICE] = trk[1].p[P_VOICE] = V_POLY;        /* (3 + 3 voices: no steal across the parts) */
    trk[0].p[P_M1SRC] = trk[1].p[P_M1SRC] = MS_LFO;
    trk[0].p[P_LWAVE] = trk[1].p[P_LWAVE] = 0;              /* (an LFO wave without random numbers) */
    part_busy(&trk[2], which & 1u ? e2 : e1, which & 1u ? 1u : 0u, 0);   /* (the stand-in) */
    trk[2].p[P_LEVEL] = 0;
    trk[2].p[P_VOICE] = V_POLY;
    rec_n = 0;
    mblk(4);
    for (i = 0; i < 3u; i++) {
        trk_note_on(&trk[which & 1u ? 0 : 2], A[i], 127);
        trk_note_on(&trk[which & 2u ? 1 : 2], B[i], 127);
    }
    mblk(FS / 2u / CTL);
    mblk(FS / 2u / CTL);
    for (i = 0; i < 3u; i++) {
        trk_note_off(&trk[0], A[i]);
        trk_note_off(&trk[1], B[i]);
        trk_note_off(&trk[2], A[i]);
        trk_note_off(&trk[2], B[i]);
    }
    mblk(FS / CTL);
    for (k = 0; k < rec_n; k++) {
        l[k] = rec_l[k];
        r[k] = rec_r[k];
    }
}
/* a two_part render in a child process (every engine's own state, the engines' memory, the buses: as the parent has
 * them, the same for each of the three renders), its channels' hashes back through a pipe; lim: the limiter worked */
static void two_part_h(uint32_t e1, uint32_t e2, uint32_t rt, uint32_t which, uint64_t *h, uint32_t *lim)
{
    int fd[2];
    uint64_t v[3] = {0, 0, 0};
    pid_t pid;
    if (pipe(fd))
        return;
    pid = fork();
    if (!pid) {
        static int32_t l[3u * FS], r[3u * FS];
        uint32_t k;
        two_part(e1, e2, rt, which, l, r);
        v[0] = v[1] = 0xCBF29CE484222325ull;
        for (k = 0; k < rec_n; k++) {
            v[0] = (v[0] ^ (uint32_t)l[k]) * 0x100000001B3ull;
            v[1] = (v[1] ^ (uint32_t)r[k]) * 0x100000001B3ull;
        }
        v[2] = (lim_env > LIM_T || lim_env_r > LIM_T || peak_of(l, rec_n) > 30000 || peak_of(r, rec_n) > 30000) |
               (uint64_t)rec_n << 1;
        if (write(fd[1], v, sizeof v) != (ssize_t)sizeof v)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], v, sizeof v) != (ssize_t)sizeof v)
        v[0] = v[1] = 0;
    close(fd[0]);
    waitpid(pid, NULL, 0);
    h[0] = v[0];
    h[1] = v[1];
    *lim |= (uint32_t)(v[2] & 1u);
}
static void t_superpose(void)
{
    uint32_t c, rt, ok = 1, lim = 0, n = 0;
    for (c = 0; c < NENGS; c++)
        for (rt = 0; rt < 3u; rt++) {
            uint32_t e1 = eng_of(ENGS[c]), e2 = eng_of(ENGS[(c + 5u) % NENGS]);
            uint64_t hb[2], h1[2], h2[2];
            two_part_h(e1, e2, rt, 3u, hb, &lim);
            two_part_h(e1, e2, rt, 1u, h1, &lim);
            two_part_h(e1, e2, rt, 2u, h2, &lim);
            n++;
            if (!hb[0] || hb[0] != h1[0] || hb[1] != h2[1]) {
                ok = 0;
                printf("mode:   %s + %s reverb %u: not the parts alone (L %s, R %s)\n", ENGS[c], ENGS[(c + 5u) % NENGS], rt,
                       hb[0] == h1[0] ? "same" : "differs", hb[1] == h2[1] ? "same" : "differs");
            }
        }
    printf("mode: %u engine pairs x reverb models, each rendered both / part 1 alone / part 2 alone\n", n);
    expect("SPLIT: both parts at once: left = part 1 alone, right = part 2 alone, sample for sample (every engine)", ok);
    expect("  .. loud enough that the limiter works (its envelope above the threshold), BASS+ on", lim != 0);
}

/* -------------------------------------------------------------- LAYER --- */
static uint32_t key_of(uint32_t note)                   /* the key whose note (octave 0, part 1's scale) is note: or 27 */
{
    for (uint32_t k = 0; k < 27u; k++)
        if (kb_map(&trk[0], k) == note)
            return k;
    return 27u;
}
/* an arp is one phrase for the LFO: TRIG NOTE restarts LFO 1 at the key press, not at every arp step (LAYER, part 2 on its own arp;
 * the owner's GLASS SHARDS: LFO -> PAN/DIST did nothing because every 1/16 step restarted the LFO at phase 0) */
static void t_arp_lfo(void)
{
    uint32_t i, drops = 0, steps = 0, last_note = 0, pl, rate = 20;
    dsp_clean(DM_LAYER);
    host_preset(&trk[0], eng_of("ANALOG"), 0);
    host_preset(&trk[1], eng_of("ANALOG"), 0);
    trk[0].p[P_AMODE] = 0;
    trk[1].p[P_AMODE] = 1;                               /* UP */
    trk[1].p[P_ARATE] = 2;
    trk[1].p[P_AGATE] = 64;
    trk[1].p[P_LTRIG] = 0;                               /* NOTE (the option; the default is FREE since 0.2.2) */
    trk[1].p[P_LRATE] = (int16_t)rate;
    trk[1].p[P_M1SRC] = 1;                               /* LFO 1 -> PAN, +63 */
    trk[1].p[P_M1DST] = 5;
    trk[1].p[P_M1AMT] = 63;
    mblk(2);
    fm1_in.notes = 1u << key_of(60) | 1u << key_of(64) | 1u << key_of(67);
    mblk(4);
    pl = trk[1].lfo_ph;
    for (i = 0; i < FS / CTL; i++) {                     /* 1 s of arp steps (1/16: about 8 at the default tempo) */
        mblk(1);
        drops += trk[1].lfo_ph < pl;
        pl = trk[1].lfo_ph;
        if (trk[1].arp_note != last_note && trk[1].arp_note) {
            steps++;
            last_note = trk[1].arp_note;
        }
    }
    expect("LAYER: part 2's arp steps do not restart LFO 1 (TRIG NOTE): its phase runs on through 1 s of steps",
           steps >= 3u && drops == 0u && LFO_INC[rate] * (FS / CTL) < 0x80000000u);
    fm1_in.notes = 0;
    mblk(FS / 4u / CTL);
    fm1_in.notes = 1u << key_of(60);                     /* a new press after the keys were let go: a new phrase, the LFO restarts */
    pl = trk[1].lfo_ph;
    mblk(FS / 4u / CTL);
    expect("  .. letting go and pressing again is a new phrase: the phase restarts", trk[1].lfo_ph < pl);
    fm1_in.notes = 0;
    mblk(FS / 4u / CTL);
}
/* LFO TRIG (DUELL 0.2.2): FREE is the default of both LFOs (the table, a fresh power-on, a sound load / INIT SOUND, the track's
 * defaults); NOTE, the option, still restarts the LFO at PHS on a new phrase and FREE never does */
static void t_lfo_trig_default(void)
{
    track_t *t = &trk[0];
    uint32_t i;
    int ok;
    expect("LFO TRIG: the table's default is FREE for both LFOs", TP[P_LTRIG].def == 1 && TP[P_L2TRIG].def == 1);
    ui_power_on();
    expect("  a fresh part (power-on) has both FREE", trk[0].p[P_LTRIG] == 1 && trk[0].p[P_L2TRIG] == 1 &&
           trk[1].p[P_LTRIG] == 1 && trk[1].p[P_L2TRIG] == 1);
    for (i = 0; i < NENGINES; i++) {
        if (!eng_ok(i) || i == ENGI_DIGITAL)
            continue;
        t->p[P_LTRIG] = t->p[P_L2TRIG] = 0;
        set_engine_of(t, i);                              /* INIT SOUND (set_engine): the engine's first sound over the defaults */
        if (t->p[P_LTRIG] != 1 || t->p[P_L2TRIG] != 1)
            break;
    }
    expect("  INIT SOUND / an engine change on every engine: FREE", i == NENGINES);
    t->p[P_LTRIG] = t->p[P_L2TRIG] = 0;
    track_defaults(t);
    expect("  the track's defaults (CLEAR / project reset): FREE", t->p[P_LTRIG] == 1 && t->p[P_L2TRIG] == 1);
    dsp_clean(DM_LAYER);
    host_preset(t, eng_of("ANALOG"), 0);
    /* a new phrase: NOTE restarts both LFOs at PHS, FREE leaves the phase alone, FADE starts over with either */
    for (i = 0; i < 2u; i++) {
        t->p[P_LPHASE] = 32;
        t->p[P_L2PHASE] = 32;
        t->p[P_LTRIG] = t->p[P_L2TRIG] = (int16_t)i;     /* i 0 NOTE, 1 FREE */
        trk_all_off(t);
        mblk(FS / 4u / CTL);
        t->lfo_ph = t->lfo2.ph = 0x55555555u;
        t->lfo_fade = t->lfo2.fade = 32767;
        trk_note_on(t, 60, 100);
        ok = t->lfo_fade == 0 && t->lfo2.fade == 0;
        ok &= i ? t->lfo_ph == 0x55555555u && t->lfo2.ph == 0x55555555u : t->lfo_ph == 32u << 25 && t->lfo2.ph == 32u << 25;
        expect(i ? "  TRIG FREE: a new phrase keeps the phase of both LFOs (FADE starts over)" :
                   "  TRIG NOTE: a new phrase restarts both LFOs at PHS (FADE starts over)", ok);
    }
    trk_all_off(t);
}
static void t_layer(void)
{
    uint32_t k, n, ok, i;
    dsp_clean(DM_LAYER);
    host_preset(&trk[0], eng_of("ANALOG"), 0);
    host_preset(&trk[1], eng_of("FM6"), 3);
    trk[0].p[P_VOICE] = trk[1].p[P_VOICE] = V_POLY;
    trk[0].p[P_AMODE] = trk[1].p[P_AMODE] = 0;
    mblk(2);
    k = key_of(60);
    fm1_in.notes = 1u << k;
    mblk(2);
    n = kb_note[k];
    expect("LAYER: a key plays both parts, the same note and velocity",
           vgated(&trk[0], n) == 1u && vgated(&trk[1], n) == 1u && trk[0].v[trk[0].m_vi].vel == trk[1].v[trk[1].m_vi].vel);
    fm1_in.notes = 0;
    mblk(2);
    expect("  .. let go: both released", !gated_any(&trk[0]) && !gated_any(&trk[1]));
    song.sel = 1;
    fm1_in.notes = 1u << k;
    mblk(2);
    expect("  .. with part 2 selected: still both (the keys play part 1, the layer)", vgated(&trk[0], n) == 1u &&
           vgated(&trk[1], n) == 1u);
    fm1_in.notes = 0;
    mblk(2);
    song.sel = 0;
    expect("  .. let go: both released", !gated_any(&trk[0]) && !gated_any(&trk[1]));

    song.g[G_ROUTE] = 0;                                 /* ROUT CH1-4 */
    midi(0x90, 0, 62, 99);
    mblk(1);
    expect("LAYER: MIDI channel 1 plays both parts (note 62, velocity 99 on each)",
           vgated(&trk[0], 62) == 1u && vgated(&trk[1], 62) == 1u && trk[1].v[trk[1].m_vi].vel == 99u);
    midi(0x90, 1, 65, 99);
    mblk(1);
    expect("  .. channel 2 (part 2's) is not heard: no part plays 65", !vgated(&trk[0], 65) && !vgated(&trk[1], 65));
    midi(0xE0, 0, 0, 127);                               /* bend up, full */
    mblk(1);
    expect("  .. channel 1's bend bends both parts", midi_bend_target[0] > 0 && midi_bend_target[1] == midi_bend_target[0]);
    midi(0xE0, 1, 0, 0);
    mblk(1);
    expect("  .. channel 2's bend: ignored", midi_bend_target[1] == midi_bend_target[0]);
    midi(0xB0, 0, 64, 127);                              /* sustain */
    midi(0x80, 0, 62, 0);
    mblk(1);
    expect("  .. sustain held: the note-off keeps both sounding", vgated(&trk[0], 62) == 1u && vgated(&trk[1], 62) == 1u);
    midi(0xB0, 0, 64, 0);
    mblk(1);
    expect("  .. pedal up: both released", !gated_any(&trk[0]) && !gated_any(&trk[1]));
    midi(0xB0, 0, 1, 100);                               /* the mod wheel */
    mblk(1);
    expect("  .. the mod wheel of channel 1 is part 2's too (the matrix's MODW)", trk[0].mw == 100u && trk[1].mw == 100u);
    midi(0xE0, 0, 0, 64);
    song.g[G_ROUTE] = 1;                                 /* ROUT SEL: every channel the selected part: the layer */
    mblk(1);
    midi(0x90, 5, 70, 80);
    mblk(1);
    expect("LAYER, ROUT SEL: a note on any channel plays both parts", vgated(&trk[0], 70) == 1u && vgated(&trk[1], 70) == 1u);
    midi(0x80, 5, 70, 0);
    mblk(1);
    expect("  .. its note-off ends both", !gated_any(&trk[0]) && !gated_any(&trk[1]));
    song.g[G_ROUTE] = 0;
    mblk(1);

    /* each part runs its own arp on the held notes: part 1 UP, part 2 OFF (holds the chord); then part 2 DN too */
    trk[0].p[P_AMODE] = 1;                               /* UP */
    trk[0].p[P_ARATE] = 2;
    trk[1].p[P_AMODE] = 0;
    fm1_in.notes = 1u << key_of(60) | 1u << key_of(64) | 1u << key_of(67);
    mblk(2);
    for (ok = 0, i = 0; i < FS / CTL; i++) {
        mblk(1);
        if (trk[0].arp_note && vgated(&trk[0], trk[0].arp_note) && vgated(&trk[1], 60) && vgated(&trk[1], 64) &&
            vgated(&trk[1], 67) && !(vgated(&trk[0], 60) && vgated(&trk[0], 64) && vgated(&trk[0], 67)))
            ok++;
    }
    expect("LAYER: part 1 arp UP arpeggiates single notes while part 2 (arp off) sustains the whole chord", ok > 100u);
    trk[1].p[P_AMODE] = 2;                               /* DN */
    trk[1].p[P_ARATE] = 2;
    fm1_in.notes = 0;                                    /* (a part's arp takes the notes pressed after it is on) */
    mblk(FS / 4u / CTL);
    fm1_in.notes = 1u << key_of(60) | 1u << key_of(64) | 1u << key_of(67);
    mblk(4);
    {
        uint32_t diff = 0, both = 0;
        for (i = 0; i < 4u * FS / CTL; i++) {
            mblk(1);
            if (trk[0].arp_note && trk[1].arp_note) {
                both++;
                diff += trk[0].arp_note != trk[1].arp_note;
            }
        }
        expect("  .. part 2 arp DN on: both arpeggiate, independently (different notes at the same time)",
               both > 20u && diff > 5u && !(vgated(&trk[1], 60) && vgated(&trk[1], 64) && vgated(&trk[1], 67)));
    }
    fm1_in.notes = 0;
    mblk(FS / 4u / CTL);
    expect("  .. keys let go: both stop", !gated_any(&trk[0]) && !gated_any(&trk[1]));
    trk[0].p[P_AMODE] = trk[1].p[P_AMODE] = 0;

    /* the voice budget: 6 notes held on both POLY parts: 8 voices at most, the sounding notes in pairs */
    {
        static const uint8_t N6[6] = {48, 52, 55, 59, 62, 65};
        uint32_t pairs = 1, busy;
        for (i = 0; i < 6u; i++) {
            midi(0x90, 0, N6[i], 100);
            mblk(2);
        }
        mblk(4);
        busy = voices_busy();
        for (i = 0; i < 6u; i++)
            pairs &= vgated(&trk[0], N6[i]) == vgated(&trk[1], N6[i]);
        printf("mode: LAYER budget: 6 notes held -> %u voices busy; notes sounding on both: ", busy);
        for (i = 0; i < 6u; i++)
            if (vgated(&trk[0], N6[i]))
                printf("%u ", N6[i]);
        printf("\n");
        expect("LAYER: 6 notes held: at most 8 voices (the shared budget: 4 notes x 2 parts)", busy <= 8u);
        expect("  .. each note sounding sounds on both parts (the steal takes a note's two voices together)", pairs);
        expect("  .. the lowest (the bass) kept on both", vgated(&trk[0], 48) == 1u && vgated(&trk[1], 48) == 1u);
        for (i = 0; i < 6u; i++)
            midi(0x80, 0, N6[i], 0);
        mblk(2);
        expect("  .. all let go: nothing gated", !gated_any(&trk[0]) && !gated_any(&trk[1]));
    }

    /* LAYER's mix is STEREO's: the notes of part 1 in LAYER = the same notes sent to both parts by hand in STEREO */
    {
        static const uint8_t C[3] = {57, 60, 64};
        uint64_t h[2];
        uint32_t m;
        for (m = 0; m < 2u; m++) {
            dsp_clean(m ? DM_STEREO : DM_LAYER);
            buses_busy(2);
            part_busy(&trk[0], eng_of("ANALOG"), 1, -20);
            part_busy(&trk[1], eng_of("PHASE"), 2, 30);
            trk[0].p[P_VOICE] = trk[1].p[P_VOICE] = V_POLY;
            rec_n = 0;
            mblk(4);
            for (i = 0; i < 3u; i++) {
                trk_note_on(&trk[0], C[i], 110);
                trk_note_on(&trk[1], C[i], 110);       /* (LAYER: no mirroring in the voices any more: both by hand) */
            }
            mblk(FS / CTL);
            for (i = 0; i < 3u; i++) {
                trk_note_off(&trk[0], C[i]);
                trk_note_off(&trk[1], C[i]);
            }
            mblk(FS / CTL);
            h[m] = 0xCBF29CE484222325ull;
            for (i = 0; i < rec_n; i++) {
                h[m] = (h[m] ^ (uint32_t)rec_l[i]) * 0x100000001B3ull;
                h[m] = (h[m] ^ (uint32_t)rec_r[i]) * 0x100000001B3ull;
            }
        }
        expect("LAYER: the mix bit for bit STEREO's with the notes sent to both parts (SPREAD, PANs, HALL: full stereo)",
               h[0] == h[1]);
    }
}

/* ---------------------------------------------- BALANCE, XFADE, SPLIT --- */
static void t_mix(void)
{
    uint32_t ok, kl = 27, kh = 27, k;
    int32_t a1, a2, b1, b2, p0, p1;
    /* (a) XFADE 100 %, LAYER: velocity 20 favours part 1, 120 part 2; XFADE 0: the voices untouched (xg 0, equal) */
    dsp_clean(DM_LAYER);
    host_preset(&trk[0], eng_of("ANALOG"), 0);
    host_preset(&trk[1], eng_of("ANALOG"), 0);
    song.g[G_ROUTE] = 0;
    mix_xf = 100;
    midi(0x90, 0, 60, 20);
    mblk(4);
    a1 = trk[0].v[trk[0].m_vi].env_out;
    a2 = trk[1].v[trk[1].m_vi].env_out;
    midi(0x80, 0, 60, 0);
    mblk(FS / CTL);
    midi(0x90, 0, 62, 120);
    mblk(4);
    b1 = trk[0].v[trk[0].m_vi].env_out;
    b2 = trk[1].v[trk[1].m_vi].env_out;
    midi(0x80, 0, 62, 0);
    mblk(FS / CTL);
    mix_xf = 0;
    midi(0x90, 0, 64, 20);
    mblk(4);
    ok = trk[0].v[trk[0].m_vi].xg == 0 && trk[0].v[trk[0].m_vi].env_out == trk[1].v[trk[1].m_vi].env_out;
    midi(0x80, 0, 64, 0);
    mblk(2);
    expect("XFADE 100 %, LAYER: velocity 20 part 1 >> part 2, velocity 120 the reverse; XFADE 0: both voices equal, no gain",
           a1 > 3 * a2 && a2 > 0 && b2 > 3 * b1 && ok);
    /* (b) SPLIT at C4: below only part 1, at or above only part 2, keys and MIDI; moving it while held hangs nothing */
    dsp_clean(DM_LAYER);
    host_preset(&trk[0], eng_of("ANALOG"), 0);
    host_preset(&trk[1], eng_of("ANALOG"), 0);
    song.g[G_ROUTE] = 0;
    mix_split = 61;
    midi(0x90, 0, 48, 100);
    midi(0x90, 0, 72, 100);
    mblk(2);
    ok = vgated(&trk[0], 48) == 1u && !vgated(&trk[1], 48) && !vgated(&trk[0], 72) && vgated(&trk[1], 72) == 1u;
    mix_split = 0;                                       /* moved (off) while held: the releases still end what started */
    midi(0x80, 0, 48, 0);
    midi(0x80, 0, 72, 0);
    mblk(2);
    ok = ok && !gated_any(&trk[0]) && !gated_any(&trk[1]);
    mix_split = 61;
    for (k = 0; k < 27u; k++) {
        if (kb_map(&trk[0], k) < 60u && kl == 27u)
            kl = k;
        if (kb_map(&trk[0], k) >= 60u && kh == 27u)
            kh = k;
    }
    fm1_in.notes = (1u << kl) | (1u << kh);
    mblk(2);
    ok = ok && kl < 27u && kh < 27u && vgated(&trk[0], kb_note[kl]) == 1u && !vgated(&trk[1], kb_note[kl]) &&
         !vgated(&trk[0], kb_note[kh]) && vgated(&trk[1], kb_note[kh]) == 1u;
    mix_split = 0;
    fm1_in.notes = 0;
    mblk(2);
    expect("SPLIT C4, LAYER: 48 and a low key only part 1, 72 and a high key only part 2; moved while held: no hanging note",
           ok && !gated_any(&trk[0]) && !gated_any(&trk[1]));
    /* (c) BALANCE +100: part 1 silent (part 2 not); -100 the reverse; 0: both heard */
    dsp_clean(DM_STEREO);
    host_preset(&trk[0], eng_of("ANALOG"), 0);
    host_preset(&trk[1], eng_of("ANALOG"), 0);
    mix_bal = 100;
    trk_note_on(&trk[0], 60, 100);
    mblk(8);
    p0 = peak_of(rec_l, 0);
    rec_n = 0;
    mblk(8);
    p0 = peak_of(rec_l, rec_n) | peak_of(rec_r, rec_n);
    trk_note_off(&trk[0], 60);
    mblk(FS / CTL);
    mix_bal = 100;
    trk_note_on(&trk[1], 60, 100);
    rec_n = 0;
    mblk(8);
    p1 = peak_of(rec_l, rec_n) | peak_of(rec_r, rec_n);
    trk_note_off(&trk[1], 60);
    mblk(FS / CTL);
    mix_bal = 0;
    trk_note_on(&trk[0], 60, 100);
    rec_n = 0;
    mblk(8);
    ok = (peak_of(rec_l, rec_n) | peak_of(rec_r, rec_n)) > 0;
    expect("BALANCE +100: part 1 silent, part 2 heard; BALANCE 0: part 1 heard", p0 == 0 && p1 > 0 && ok);
}

/* ------------------------------------------------------------- HANGING --- */
static uint32_t dip_blocks(void)                        /* blocks until the mix is in its mode at full level */
{
    uint32_t b = 0;
    while ((duell_mode_req != duell_mode || dm_cur != duell_mode || dm_g != DM_ONE || dm_clr) && b < 1000u) {
        mblk(1);
        b++;
    }
    return b;
}
static void t_hanging(void)
{
    uint32_t k, i;
    /* LAYER -> SPLIT with a key, a pedalled MIDI note and a latched arp chord held */
    dsp_clean(DM_LAYER);
    host_preset(&trk[0], eng_of("ANALOG"), 0);
    host_preset(&trk[1], eng_of("PHASE"), 1);
    trk[0].p[P_REL] = trk[1].p[P_REL] = 30;
    trk[0].p[P_VOICE] = trk[1].p[P_VOICE] = V_POLY;
    k = key_of(60);
    fm1_in.notes = 1u << k;
    midi(0xB0, 0, 64, 127);
    midi(0x90, 0, 72, 100);
    midi(0x80, 0, 72, 0);
    mblk(2);
    expect("held: the key's note and the pedalled MIDI note on both parts", vgated(&trk[0], kb_note[k]) && vgated(&trk[1], 72));
    duell_mode_req = DM_SPLIT;                           /* (the MODE page's knob) */
    mblk(1);
    expect("LAYER -> SPLIT with notes held: every note let go at once (nothing gated on either part)",
           !gated_any(&trk[0]) && !gated_any(&trk[1]) && duell_mode == DM_SPLIT);
    i = dip_blocks();
    fm1_in.notes = 0;                                    /* the key let go after the switch: nothing to end */
    midi(0xB0, 0, 64, 0);
    mblk(FS / CTL);
    expect("  .. the key and the pedal let go after: nothing sounds (no stuck note)", !active_any());
    /* SPLIT: part 2 held on its channel; -> LAYER: channel 2's note-off is not heard there, the note must not hang */
    song.g[G_ROUTE] = 0;
    midi(0x90, 1, 55, 100);
    mblk(2);
    expect("SPLIT: channel 2 plays part 2", vgated(&trk[1], 55) == 1u && !gated_any(&trk[0]));
    midi(0xB0, 0, MIDI_MODE_CC, 127);                    /* CC 112: LAYER */
    mblk(1);
    midi(0x80, 1, 55, 0);                                /* (not heard in LAYER) */
    mblk(FS / CTL);
    expect("  .. CC 112 -> LAYER, channel 2's note-off unheard: part 2's note ended by the change all the same",
           duell_mode == DM_LAYER && !active_any());
    /* STEREO <-> LAYER: a latched arp chord (HOLD) dropped, no dip */
    duell_mode_force(DM_STEREO);
    trk[0].p[P_AMODE] = 1;
    trk[0].p[P_AHOLD] = 1;
    fm1_in.notes = 1u << key_of(60) | 1u << key_of(64);
    mblk(4);
    fm1_in.notes = 0;
    mblk(FS / 4u / CTL);
    expect("STEREO: an arp chord latched (HOLD) plays on", trk[0].nheld == 2u && trk[0].arp_note);
    duell_mode_req = DM_LAYER;
    mblk(1);
    i = dip_blocks();
    mblk(FS / CTL);
    expect("  .. -> LAYER: the latched chord dropped, nothing plays (no dip between STEREO and LAYER)",
           !trk[0].nheld && !active_any() && i <= 1u);
    trk[0].p[P_AMODE] = trk[0].p[P_AHOLD] = 0;

    /* fuzz: random keys, MIDI notes on channels 1 and 2, pedals, the arp, mode changes from CC 112 and the UI */
    {
        uint32_t seed = 12345u, ev, changes = 0, sounding_max = 0, c;
        uint8_t on[2][128];
        memset(on, 0, sizeof on);
        dsp_clean(DM_SPLIT);
        host_preset(&trk[0], eng_of("ANALOG"), 2);
        host_preset(&trk[1], eng_of("TRIO"), 1);
        trk[0].p[P_REL] = trk[1].p[P_REL] = 40;
        song.g[G_ROUTE] = 0;
        for (ev = 0; ev < 6000u; ev++) {
            uint32_t r;
            seed = seed * 1664525u + 1013904223u;
            r = seed >> 8;
            c = (r >> 20) & 1u;
            switch (r % 16u) {
            case 0: case 1: case 2:                      /* a key */
                fm1_in.notes ^= 1u << ((r >> 4) % 27u);
                break;
            case 3: case 4: case 5: case 6: {             /* a MIDI note on / off on channel 1 or 2 */
                uint32_t nt = 36u + (r >> 5) % 48u;
                midi(on[c][nt] ? 0x80 : 0x90, c, nt, 90);
                on[c][nt] ^= 1u;
                break;
            }
            case 7:                                      /* a pedal */
                midi(0xB0, c, 64, (r >> 6) & 1u ? 127 : 0);
                break;
            case 8:                                      /* the arp of a part on / off, HOLD */
                trk[c].p[P_AMODE] = (int16_t)((r >> 7) & 1u);
                trk[c].p[P_AHOLD] = (int16_t)((r >> 9) % 5u == 0u);
                break;
            case 9:
                if ((r >> 6) % 6u == 0u) {               /* a mode change: CC 112 or the UI */
                    if ((r >> 9) & 1u)
                        midi(0xB0, (r >> 10) % 16u, MIDI_MODE_CC, (r >> 11) % 128u);
                    else
                        duell_mode_req = (uint8_t)((r >> 11) % 3u);
                    changes++;
                }
                break;
            case 10:
                song.sel = (uint8_t)((r >> 5) & 1u);
                break;
            default:
                break;
            }
            mblk(1 + (r >> 24) % 4u);
            if (voices_busy() > sounding_max)
                sounding_max = voices_busy();
        }
        /* everything let go: keys, every note sent, the pedals, the arps off */
        fm1_in.notes = 0;
        for (c = 0; c < 2u; c++) {
            for (k = 0; k < 128u; k++)
                if (on[c][k])
                    midi(0x80, c, k, 0);
            midi(0xB0, c, 64, 0);
        }
        trk[0].p[P_AMODE] = trk[1].p[P_AMODE] = trk[0].p[P_AHOLD] = trk[1].p[P_AHOLD] = 0;
        mblk(4);
        expect("fuzz (6000 events, mode changes among them): everything let go -> nothing gated", !gated_any(&trk[0]) &&
               !gated_any(&trk[1]));
        mblk(2u * FS / CTL);
        printf("mode: fuzz: %u mode changes, at most %u voices busy\n", changes, sounding_max);
        expect("  .. and nothing sounds 2 s later (no stuck note); the budget kept (8)", !active_any() && sounding_max <= 8u);
    }
}

/* -------------------------------------------------------------- CLICK --- */
static int32_t max_step(const int32_t *x, uint32_t a, uint32_t b)
{
    int32_t m = 0;
    for (uint32_t i = a + 1u; i < b; i++) {
        int32_t d = x[i] - x[i - 1u];
        d = d < 0 ? -d : d;
        m = d > m ? d : m;
    }
    return m;
}
static void t_click(void)
{
    static const uint32_t FROM[3] = {DM_STEREO, DM_SPLIT, DM_STEREO}, TO[3] = {DM_SPLIT, DM_STEREO, DM_LAYER};
    uint32_t c, ok = 1, okd = 1;
    for (c = 0; c < 3u; c++) {
        uint32_t at, b, i;
        int32_t s0, s1, s2, s3;
        dsp_clean(FROM[c]);
        host_preset(&trk[0], eng_of("ANALOG"), 5);       /* (SINE KEY: a smooth tone, so a click would show) */
        host_preset(&trk[1], eng_of("ANALOG"), 5);
        for (i = 0; i < 2u; i++) {
            trk[i].p[P_SUS] = 127;
            trk[i].p[P_REL] = 90;                        /* (a long release: still sounding at the dip) */
            trk[i].p[P_CHOR] = trk[i].p[P_DLY] = trk[i].p[P_REV] = 0;
            trk[i].p[P_LEVEL] = 120;
            trk[i].p[P_PAN] = (int16_t)(i ? 40 : -40);
        }
        rec_n = 0;
        trk_note_on(&trk[0], 57, 127);
        trk_note_on(&trk[1], 64, 127);
        mblk(FS / 2u / CTL);
        at = rec_n;
        duell_mode_req = (uint8_t)TO[c];
        b = dip_blocks();
        mblk(8);
        s0 = max_step(rec_l, at - FS / 4u, at);
        s1 = max_step(rec_r, at - FS / 4u, at);
        s2 = max_step(rec_l, at, rec_n);
        s3 = max_step(rec_r, at, rec_n);
        printf("mode: %s -> %s: %u blocks (%.1f ms) to full level; largest sample step L %d / R %d before, %d / %d during\n",
               DM_NAME[FROM[c]], DM_NAME[TO[c]], b, b * CTL * 1000.0 / FS, s0, s1, s2, s3);
        ok &= s2 <= s0 + 2 && s3 <= s1 + 2;
        okd &= c < 2u ? b * CTL * 1000u / FS >= 40u && b * CTL * 1000u / FS <= 60u : b <= 1u;
    }
    expect("the switch has no click: no sample step during it above the steady tone's (SPLIT <-> STEREO, STEREO -> LAYER)", ok);
    expect("  .. SPLIT <-> STEREO: a dip of 40..60 ms; STEREO -> LAYER: none (the same mix)", okd);
}

/* --------------------------------------------------------------- CC 112 --- */
static void t_cc(void)
{
    static const uint8_t V[6] = {0, 42, 43, 85, 86, 127}, W[6] = {DM_SPLIT, DM_SPLIT, DM_STEREO, DM_STEREO, DM_LAYER, DM_LAYER};
    static const uint8_t CHS[4] = {0, 1, 9, 15};
    uint32_t i, c, ok = 1;
    dsp_clean(DM_SPLIT);
    song.g[G_ROUTE] = 0;                                 /* ROUT CH1-4: channels 3..16 not played */
    for (c = 0; c < 4u; c++)
        for (i = 0; i < 6u; i++) {
            uint32_t j = (i + 3u * c) % 6u;
            midi(0xB0, CHS[c], MIDI_MODE_CC, V[j]);
            mblk(1);
            ok &= duell_mode_req == W[j] && duell_mode == W[j];
        }
    expect("CC 112: 0-42 SPLIT, 43-85 STEREO, 86-127 LAYER, global, on channels 1, 2, 10, 16 (ROUT CH1-4)", ok);
    {
        uint32_t lvl = (uint32_t)trk[0].p[P_LEVEL];
        uint16_t e = (uint16_t)(MIDI_MODE_CC | 0u << 7 | (P_LEVEL + 1u) << 9);
        duell_mode_force(DM_SPLIT);
        ml_tab[0] = (uint8_t)e;                          /* MIDI LEARN: CC 112 -> part 1's LEVEL */
        ml_tab[1] = (uint8_t)(e >> 8);
        midi(0xB0, 0, MIDI_MODE_CC, 127);
        mblk(1);
        expect("CC 112 learned by MIDI LEARN: the user's (part 1's LEVEL moves), the MODE stays",
               duell_mode == DM_SPLIT && (uint32_t)trk[0].p[P_LEVEL] != lvl);
        ml_tab[0] = ml_tab[1] = 0;
    }
    {
        uint32_t m = 0, i2;
        for (i2 = 0; i2 < NELEM(MIDI_CC_MAP); i2++)
            m |= MIDI_CC_MAP[i2][0] == MIDI_MODE_CC;
        expect("CC 112: not in the standard map, not a MORPH CC, free for MIDI LEARN (ml_free_cc)",
               !m && !MIDI_MORPH_CC(MIDI_MODE_CC) && ml_free_cc(MIDI_MODE_CC));
    }
}

/* ---------------------------------------------------------- UI, SETTINGS --- */
static uint32_t page_of(const char *title)
{
    uint32_t i;
    for (i = 0; i < NPAGES && !str_eq(PAGES[i].title, title); i++)
        ;
    return i;
}
static void t_ui(void)
{
    char ti[20];
    int16_t pan;
    ui_power_on();
    frame();
    expect("power-on: MODE SPLIT (the settings byte 0)", duell_mode_req == DM_SPLIT && !ui_mode_set);
    press(B_HOME);                                      /* (OVERVIEW -> MORPH -> MORPH SET -> MODE) */
    frames(64);
    press(B_HOME);
    frames(64);
    press(B_HOME);
    frames(64);
    page_title(ti);
    expect("HOME tapped three times from OVERVIEW: the MODE page (HOME's third sub-page), its footer MODE 3/5",
           !ui.home && ui.page == page_of("MODE") && str_eq(ti, "MODE 3/5"));
    turn(EN_K1, -1);
    expect("MODE: KNOB 1 left at SPLIT stays SPLIT", duell_mode_req == DM_SPLIT);
    turn(EN_K1, 1);
    expect("  .. right: STEREO", duell_mode_req == DM_STEREO);
    turn(EN_K1, 1);
    turn(EN_K1, 1);
    expect("  .. right, right: LAYER (and no further)", duell_mode_req == DM_LAYER);
    turn(EN_K2, 3);
    turn(EN_K3, -3);
    turn(EN_K4, 2);
    expect("  .. KNOB 2-4: BALANCE, XFADE (0 at the left end), SPLIT move; the mode stays",
           duell_mode_req == DM_LAYER && mix_bal == 3 && mix_xf == 0 && mix_split == 2);
    mix_bal = 0;                                         /* (back to the defaults: later tests, the settings) */
    mix_split = 0;
    turn(EN_K1, -2);
    frame();
    expect("  .. back to SPLIT", duell_mode_req == DM_SPLIT);
    press(B_HOME);
    frames(64);
    expect("HOME again: the MIXER", ui.page == page_of("MIXER"));
    pan = TSEL->p[P_PAN];
    turn(EN_K2, 5);
    expect("MIXER in SPLIT: KNOB 2 (PAN) changes nothing (fixed: part 1 left, part 2 right)", TSEL->p[P_PAN] == pan &&
           str_eq(ui.msg, "SPLIT: PAN IS FIXED"));
    duell_mode_req = DM_STEREO;
    frame();
    turn(EN_K2, 5);
    expect("MIXER in STEREO: KNOB 2 moves PAN", TSEL->p[P_PAN] != pan);
}
static void t_settings(void)
{
    persist_t p;
    ui_power_on();
    frames(2500);
    expect("the default (SPLIT) writes nothing: the byte stays 0, no save queued", !ui_mode_set && !ui.mset_dirty);
    go_title("MODE");
    frames(32);
    turn(EN_K1, 2);
    frame();
    expect("MODE LAYER into the settings byte (2), a save queued", ui_mode_set == DM_LAYER && ui.mset_dirty);
    frames(2200);
    expect("  .. saved ~2 s later", !ui.mset_dirty);
    settings_export(&p);
    expect("  .. in what the flash gets (settings_export)", p.favorites.factory[15][9] == DM_LAYER);
    ui_power_on();                                       /* the power cycle: RAM gone, SPLIT */
    expect("power cycle: RAM back to SPLIT", duell_mode_req == DM_SPLIT);
    expect("  .. the stored settings import", settings_import(&p, sizeof p) == 1);
    frame();
    expect("  .. LAYER again after the power cycle", duell_mode_req == DM_LAYER);
    mblk(1);
    expect("  .. and the audio side follows at its next block", duell_mode == DM_LAYER);
    midi(0xB0, 3, MIDI_MODE_CC, 60);
    mblk(1);
    frame();
    expect("CC 112 (STEREO) is kept too: the byte follows", ui_mode_set == DM_STEREO && ui.mset_dirty);
    ui_mode_set = 7;
    frame();
    expect("an unknown code reads as SPLIT", duell_mode_req == DM_SPLIT);
}

/* ---------------------------------------------------------------- COST --- */
static double cost_run(uint32_t mode, uint32_t notes)
{
    static const uint8_t A[4] = {48, 55, 60, 64}, B[3] = {67, 71, 74};
    uint32_t i, nb = FS / CTL;
    uint64_t i0;
    dsp_clean(mode);
    buses_busy(2);
    fx_lowcut = 0;
    part_busy(&trk[0], eng_of("ANALOG"), 1, -20);
    part_busy(&trk[1], eng_of("PHASE"), 2, 20);
    trk[0].p[P_VOICE] = trk[1].p[P_VOICE] = V_POLY;
    trk[0].p[P_ITYPE] = trk[1].p[P_ITYPE] = 0;
    trk[0].p[P_DIST] = trk[1].p[P_DIST] = 0;
    if (notes)
        for (i = 0; i < 4u; i++) {
            trk_note_on(&trk[0], A[i], 110);
            if (i < 3u)
                trk_note_on(&trk[1], B[i], 110);
        }
    for (i = 0; i < nb / 4u; i++)
        mix_block(ob, CTL);
    i0 = instr_now();
    for (i = 0; i < nb; i++)
        mix_block(ob, CTL);
    return i0 ? (double)(instr_now() - i0) / (nb * CTL) : 0.0;
}
static void t_cost(void)
{
    double st = cost_run(DM_STEREO, 1), sp = cost_run(DM_SPLIT, 1), si = cost_run(DM_STEREO, 0), spi = cost_run(DM_SPLIT, 0);
    double ly = cost_run(DM_LAYER, 1);
    uint64_t i0;
    uint32_t b = 0;
    printf("mode: cost (host instructions per sample, mix_block, 2 parts: ANALOG 4 voices + PHASE 3, SPREAD, every send, HALL)\n");
    printf("mode:   STEREO %.0f   LAYER %.0f (the notes of part 1 only: 4 x 2 voices)   SPLIT %.0f  (SPLIT - STEREO: %+.0f, %+.1f %%)\n", st, ly,
           sp, sp - st, (sp - st) * 100.0 / st);
    printf("mode:   idle (no note, the buses running): STEREO %.0f   SPLIT %.0f  (%+.0f)\n", si, spi, spi - si);
    dsp_clean(DM_STEREO);
    duell_mode_req = DM_SPLIT;
    i0 = instr_now();
    b = dip_blocks();
    printf("mode:   the switch STEREO -> SPLIT: %u blocks, %.0f instructions per sample over them (the 8 silent blocks clear "
           "16 KB each)\n", b, i0 ? (double)(instr_now() - i0) / (b * CTL) : 0.0);
    expect("cost measured (SPLIT runs both buses: at most twice STEREO's whole mix)", st > 0 && sp > 0 && sp < 2.0 * st);
}

/* ------------------------------------------------- JIANT's SHIM, RESO --- */
/* a held chord through the ported reverbs (3 SHIM, 4 RESO), in STEREO and in SPLIT on part 1 and part 2: no blowup, a tail
 * that is heard after the release and ends in exact silence, SPLIT's other channel exactly 0 in every sample */
static void jiant_render(uint32_t mode, uint32_t p, uint32_t rt, uint32_t size)
{
    static const uint8_t CH[4] = {48, 52, 55, 59};
    uint32_t i;
    dsp_clean(mode);
    song.g[G_RTYPE] = (int16_t)rt;
    song.g[G_RSIZE] = (int16_t)size;
    song.g[G_RDAMP] = 60;
    song.g[G_DFDBK] = 0;
    for (i = 0; i < 2u; i++) {
        host_preset(&trk[i], eng_of("ANALOG"), i);
        trk[i].p[P_LEVEL] = 127;
        trk[i].p[P_REV] = 127;
        trk[i].p[P_DLY] = trk[i].p[P_CHOR] = trk[i].p[P_DIST] = 0;
        trk[i].p[P_REL] = 20;
    }
    rec_n = 0;
    for (i = 0; i < 4u; i++)
        trk_note_on(&trk[p], CH[i], 110);
    mblk(FS / CTL);                                      /* 1 s held */
    for (i = 0; i < 4u; i++)
        trk_note_off(&trk[p], CH[i]);
    mblk(10u * FS / CTL);                                /* 10 s of tail */
}
static void t_jiant_rev(void)
{
    uint32_t rt, m, p, i, ok;
    int32_t pk, tail, last;
    for (rt = 3; rt <= 4u; rt++)
        for (m = 0; m < 2u; m++)
            for (p = 0; p < (m ? 2u : 1u); p++) {
                static const char *const MN[] = {"STEREO", "SPLIT"};
                char w[120];
                jiant_render(m ? DM_SPLIT : DM_STEREO, p, rt, rt == 3u ? 100 : 110);   /* (RESO rings on longer per SIZE) */
                pk = peak_of(rec_l, rec_n) > peak_of(rec_r, rec_n) ? peak_of(rec_l, rec_n) : peak_of(rec_r, rec_n);
                tail = peak_of(rec_l + 13u * FS / 5u, FS / 2u);            /* 0.3 s after the release (SPLIT part 2: the right) */
                tail = (m && p) ? peak_of(rec_r + 13u * FS / 5u, FS / 2u) : tail;
                last = peak_of(rec_l + rec_n - FS / 2u, FS / 2u);
                last = peak_of(rec_r + rec_n - FS / 2u, FS / 2u) > last ? peak_of(rec_r + rec_n - FS / 2u, FS / 2u) : last;
                snprintf(w, sizeof w, "%s %s %s: peak %d, tail %d, last 0.5 s peak %d: bounded, audible tail, silent end",
                         rt == 3u ? "SHIM" : "RESO", MN[m], m ? (p ? "part 2" : "part 1") : "both", pk, tail, last);
                expect(w, pk > 0 && pk < 40000 && tail > 0 && last == 0);
                if (m) {                                        /* the other channel: exactly 0 */
                    const int32_t *o = p ? rec_l : rec_r;
                    for (i = 0, ok = 1; i < rec_n; i++)
                        ok &= o[i] == 0;
                    snprintf(w, sizeof w, "%s SPLIT part %u: the other channel exactly 0 in every sample", rt == 3u ? "SHIM" : "RESO", p + 1u);
                    expect(w, ok);
                }
            }
}

/* LEAK MAP (owner report, 2026-10-12: "part 2 sounds on both sides in SPLIT"): one feature at a time (then all together), each
 * part alone with a held chord on a sustained sound, the other channel measured (exactly 0 = digital silence), the own
 * channel must sound. The feature is set on BOTH parts and the globals (the silent part has it on too) */
typedef struct { const char *name; void (*set)(uint32_t v); uint32_t nv; } leak_t;
static void lk_part(track_t *t, uint32_t e)
{
    host_preset(t, e, 0);
    t->p[P_LEVEL] = 100;
    t->p[P_VOICE] = V_POLY;
    t->p[P_SUS] = 120;
    t->p[P_REL] = 60;
    t->p[P_AMODE] = 0;
    t->p[P_PAN] = 0;
    t->p[P_SPRD] = 0;
    t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_DIST] = 0;
    t->p[P_ITYPE] = 0;
}
static void lk_none(uint32_t v) { (void)v; }
static void lk_cho(uint32_t v) { (void)v; trk[0].p[P_CHOR] = trk[1].p[P_CHOR] = 127; song.g[G_CDEPTH] = 127; }
static void lk_dly(uint32_t v) { trk[0].p[P_DLY] = trk[1].p[P_DLY] = 127; song.g[G_DFDBK] = 110; song.g[G_DMIX] = 127; song.g[G_DTIME] = 1; song.g[G_PINGPONG] = (int16_t)v; song.g[G_PINGSPREAD] = 100; }
static void lk_rev(uint32_t v) { trk[0].p[P_REV] = trk[1].p[P_REV] = 127; song.g[G_RTYPE] = (int16_t)v; song.g[G_RSIZE] = 120; }
static void lk_ins(uint32_t v)
{
    for (uint32_t k = 0; k < 2u; k++) {
        trk[k].p[P_ITYPE] = (int16_t)(v + 1u);
        trk[k].p[P_IA] = 70; trk[k].p[P_IB] = 100; trk[k].p[P_IC] = 90; trk[k].p[P_IMIX] = 127;
    }
}
static void lk_spread(uint32_t v) { (void)v; for (uint32_t k = 0; k < 2u; k++) { trk[k].p[P_SPRD] = 127; trk[k].p[P_VOICE] = k ? V_UNISON : V_POLY; trk[k].p[P_DETUNE] = 60; trk[k].p[P_CHOR] = 100; } }
static void lk_pan(uint32_t v) { trk[0].p[P_PAN] = (int16_t)(v ? -64 : 63); trk[1].p[P_PAN] = (int16_t)(v ? 63 : -64); }
static void lk_lfopan(uint32_t v)
{
    (void)v;
    for (uint32_t k = 0; k < 2u; k++) {
        trk[k].p[P_M1SRC] = MS_LFO; trk[k].p[P_M1DST] = MD_PAN; trk[k].p[P_M1AMT] = 63; trk[k].p[P_LRATE] = 90;
    }
}
static void lk_eq(uint32_t v)
{
    (void)v;
    for (uint32_t k = 0; k < 2u; k++) { trk[k].p[P_EQLG] = 20; trk[k].p[P_EQMG] = -20; trk[k].p[P_EQHG] = 20; }
}
static void lk_duck(uint32_t v)
{
    (void)v;
    for (uint32_t k = 0; k < 2u; k++) { trk[k].p[P_DKSRC] = 0; trk[k].p[P_DKDEP] = 80; trk[k].p[P_DKATK] = 10; trk[k].p[P_DKRLS] = 40; trk[k].p[P_DKRATE] = 64; }
}
static void lk_dist(uint32_t v) { (void)v; trk[0].p[P_DIST] = trk[1].p[P_DIST] = 90; }
static void lk_bass(uint32_t v) { (void)v; fx_lowcut = 2; }
static void lk_all(uint32_t v)
{
    lk_cho(0); lk_dly(1); lk_rev(v % 5u); lk_ins(v % 8u); lk_spread(0); lk_pan(0); lk_lfopan(0); lk_eq(0); lk_duck(0);
    lk_dist(0); lk_bass(0);
}
static const leak_t LEAKS[] = {
    {"nothing (the dry part)", lk_none, 1}, {"CHORUS send", lk_cho, 1}, {"DELAY send", lk_dly, 1}, {"DELAY send PING-PONG", lk_dly, 1},
    {"REVERB ROOM", lk_rev, 1}, {"REVERB SPRING", lk_rev, 1}, {"REVERB HALL", lk_rev, 1}, {"REVERB SHIM", lk_rev, 1},
    {"REVERB RESO", lk_rev, 1}, {"INSERT SOFT", lk_ins, 1}, {"INSERT HARD", lk_ins, 1}, {"INSERT FOLD", lk_ins, 1},
    {"INSERT FUZZ", lk_ins, 1}, {"INSERT CRUSH", lk_ins, 1}, {"INSERT PHASR", lk_ins, 1}, {"INSERT FLANG", lk_ins, 1},
    {"INSERT CHOR", lk_ins, 1}, {"SPREAD 127 (+ chorus send)", lk_spread, 1}, {"PAN hard (part 1 right, part 2 left)", lk_pan, 1},
    {"PAN hard the other way", lk_pan, 1}, {"LFO -> PAN", lk_lfopan, 1}, {"EQ +-20", lk_eq, 1}, {"DUCKER (PUMP)", lk_duck, 1},
    {"DIST 90", lk_dist, 1}, {"BASS+ (speaker EQ)", lk_bass, 1}, {"ALL together", lk_all, 1}};
#define NLEAKS (sizeof LEAKS / sizeof LEAKS[0])
/* the variant of entry i for its set function */
static uint32_t lk_var(uint32_t i)
{
    return i == 3u ? 1u : i == 2u ? 0u : i >= 4u && i <= 8u ? i - 4u : i >= 9u && i <= 16u ? i - 9u : i == 19u ? 1u : i == 25u ? 4u : 0u;
}
static int32_t lk_render(uint32_t i, uint32_t p, uint32_t mode, int32_t *other_pk)
{
    static const uint8_t CH[4] = {40, 47, 55, 64};
    uint32_t k;
    dsp_clean(mode);
    song.g[G_RTYPE] = 0;
    song.g[G_PINGPONG] = 0;
    for (k = 0; k < 2u; k++)
        lk_part(&trk[k], eng_of("ANALOG"));
    LEAKS[i].set(lk_var(i));
    rec_n = 0;
    mblk(8);
    for (k = 0; k < 4u; k++)
        trk_note_on(&trk[p], CH[k], 127);
    mblk(FS / CTL);
    for (k = 0; k < 4u; k++)
        trk_note_off(&trk[p], CH[k]);
    mblk(3u * FS / CTL);                                 /* the release and the tails */
    *other_pk = peak_of(p ? rec_l : rec_r, rec_n);
    return peak_of(p ? rec_r : rec_l, rec_n);
}
static void t_leakmap(void)
{
    uint32_t i, p, ok = 1, ok_own = 1, st_wide = 0;
    printf("mode: LEAK MAP, SPLIT: the opposite channel's peak (int, 0 = digital silence; dB re full scale) with ONE part sounding\n");
    for (i = 0; i < NLEAKS; i++) {
        int32_t own[2], oth[2], so, sown;
        for (p = 0; p < 2u; p++)
            own[p] = lk_render(i, p, DM_SPLIT, &oth[p]);
        printf("mode:   %-40s part 1: own %6d, other %d (%s)   part 2: own %6d, other %d (%s)\n", LEAKS[i].name, own[0], oth[0],
               oth[0] ? "LEAK" : "-inf", own[1], oth[1], oth[1] ? "LEAK" : "-inf");
        if (oth[0] || oth[1])
            ok = 0;
        if (own[0] < 500 || own[1] < 500)
            ok_own = 0;
        sown = lk_render(i, 1, DM_STEREO, &so);          /* (STEREO: PAN / SPREAD / PING-PONG / HALL may well reach the left) */
        if (so > sown / 100)
            st_wide++;
    }
    expect("SPLIT, one part alone, each effect and stereo feature on its own and all together: the other channel is digital silence", ok);
    expect("  .. and the part's own channel sounds in every case", ok_own);
    expect("  .. STEREO still shows its width (some of these put part 2 on the left there)", st_wide > 0);
}

#ifndef MODE_TEST_NO_MAIN
/* DUELL step 6: the parts' MIDI channels (GLO > MIDI) and the level meters' data (GLO's page) */
static void t_midi_ch(void)
{
    persist_t p;
    uint32_t ok, pc;
    ui_power_on();
    frames(2500);
    dsp_clean(DM_STEREO);
    song.g[G_ROUTE] = 0;
    go_title("MIDI");
    frames(32);
    expect("MIDI: defaults, PART 1 CH 1 and PART 2 CH 2, nothing written to the settings",
           midi_pch[0] == 0u && midi_pch[1] == 1u && !ui_midi_p1 && !ui_midi_p2);
    midi(0x90, 1, 62, 99);
    mblk(1);
    expect("  .. channel 2 plays part 2 alone", vgated(&trk[1], 62) == 1u && !gated_any(&trk[0]));
    midi(0x80, 1, 62, 0);
    mblk(1);
    turn(EN_K2, 3);                                      /* 2 -> 5 */
    frame();
    expect("KNOB 2 right x3: PART 2 CH 5 (the settings byte 5, a save queued)", midi_pch[1] == 4u && ui_midi_p2 == 5u && ui.mset_dirty);
    midi(0x90, 4, 64, 99);
    midi(0x90, 1, 65, 99);
    midi(0x90, 0, 67, 99);
    mblk(1);
    expect("  .. channel 5 plays part 2 and part 1 ignores it", vgated(&trk[1], 64) == 1u && !vgated(&trk[0], 64));
    expect("  .. channel 2 (the old one) plays nothing", !vgated(&trk[0], 65) && !vgated(&trk[1], 65));
    expect("  .. channel 1 still plays part 1 alone", vgated(&trk[0], 67) == 1u && !vgated(&trk[1], 67));
    turn(EN_K1, 9);                                      /* part 1: 1 -> 10 */
    frame();
    mblk(1);
    expect("KNOB 1 right x9: PART 1 CH 10; notes held on channels no part hears any more are let go",
           midi_pch[0] == 9u && ui_midi_p1 == 10u && !vgated(&trk[0], 67));
    midi(0x90, 9, 69, 99);
    midi(0x90, 0, 71, 99);
    mblk(1);
    expect("  .. channel 10 plays part 1, channel 1 plays nothing", vgated(&trk[0], 69) == 1u && !vgated(&trk[0], 71) && !vgated(&trk[1], 71));
    midi(0xB0, 4, 7, 33);
    midi(0xB0, 9, 7, 44);
    midi(0xB0, 0, 7, 55);
    mblk(1);
    expect("  .. CC 7 on channel 5 / 10 / 1: part 2's LEVEL, part 1's LEVEL, nobody's",
           trk[1].p[P_LEVEL] == 33 && trk[0].p[P_LEVEL] == 44);
    pc = pcq_w;
    midi(0xC0, 4, 3, 0);
    midi(0xC0, 9, 4, 0);
    midi(0xC0, 0, 5, 0);
    mblk(1);
    expect("  .. PROGRAM CHANGE: channel 5 queues part 2 (program 3), channel 10 part 1 (4), channel 1 nothing",
           (uint8_t)(pcq_w - pc) == 2u && (pcq_buf[pc % PCQ_N] & 0xFFu) == 1u && (pcq_buf[pc % PCQ_N] >> 8 & 0xFFu) == 3u &&
           (pcq_buf[(pc + 1u) % PCQ_N] & 0xFFu) == 0u && (pcq_buf[(pc + 1u) % PCQ_N] >> 8 & 0xFFu) == 4u);
    midi(0x80, 9, 69, 0);
    midi(0x80, 4, 64, 0);
    mblk(1);
    turn(EN_K2, 5);                                      /* part 2 onto part 1's channel: part 1 takes it */
    frame();
    midi(0x90, 9, 72, 99);
    mblk(1);
    expect("both parts on one channel: part 1 plays it, part 2 gets nothing", midi_pch[1] == 9u && vgated(&trk[0], 72) == 1u && !vgated(&trk[1], 72));
    midi(0x80, 9, 72, 0);
    mblk(1);
    turn(EN_K2, -5);
    frame();
    duell_mode_force(DM_LAYER);
    midi(0x90, 9, 74, 99);
    midi(0x90, 4, 76, 99);
    mblk(1);
    expect("LAYER: part 1's channel (10) plays both parts, part 2's (5) is not heard",
           vgated(&trk[0], 74) == 1u && vgated(&trk[1], 74) == 1u && !vgated(&trk[0], 76) && !vgated(&trk[1], 76));
    midi(0x80, 9, 74, 0);
    mblk(1);
    duell_mode_force(DM_STEREO);
    song.g[G_ROUTE] = 1;
    song.sel = 1;
    midi(0x90, 13, 77, 99);
    mblk(1);
    expect("ROUT SEL: any channel plays the selected part", vgated(&trk[1], 77) == 1u && !vgated(&trk[0], 77));
    midi(0x80, 13, 77, 0);
    mblk(1);
    song.g[G_ROUTE] = 0;
    song.sel = 0;
    /* the settings: kept, back after a power cycle; an older settings blob (the bytes 0) means channels 1 and 2 */
    frames(2200);
    expect("  .. saved ~2 s after the last change", !ui.mset_dirty);
    settings_export(&p);
    expect("  .. in what the flash gets: [15][0] = 10, [15][1] = 5", p.favorites.factory[15][0] == 10u && p.favorites.factory[15][1] == 5u);
    ui_power_on();
    frame();
    ok = midi_pch[0] == 0u && midi_pch[1] == 1u;
    settings_import(&p, sizeof p);
    frame();
    expect("power cycle: channels 1 / 2, then the stored settings give 10 / 5 back", ok && midi_pch[0] == 9u && midi_pch[1] == 4u);
    p.favorites.factory[15][0] = p.favorites.factory[15][1] = 0;
    settings_import(&p, sizeof p);
    frame();
    expect("an older settings blob (both bytes 0): the defaults, channel 1 and 2", midi_pch[0] == 0u && midi_pch[1] == 1u);
    p.favorites.factory[15][0] = 99;                       /* an unknown code reads as the default */
    settings_import(&p, sizeof p);
    frame();
    expect("an unknown code reads as the default", midi_pch[0] == 0u);
}
static void t_meter(void)
{
    uint32_t k;
    dsp_clean(DM_SPLIT);
    trk[0].p[P_LEVEL] = 110;
    meter_on = 255;
    meter_bus[0] = meter_bus[1] = 0;
    trk[0].peak = trk[1].peak = 0;
    input_on(&trk[0], 60, 127);
    for (k = 0; k < 40u; k++)
        mblk(1);
    expect("level meters: SPLIT, part 1 sounding: its peak and its bus are above 0, part 2's are 0",
           trk[0].peak > 0 && meter_bus[0] > 0 && !trk[1].peak && !meter_bus[1]);
    expect("  .. units: a bus peak maps inside -48 .. +6 dBFS", gm_units(meter_bus[0]) > 112 && gm_units(meter_bus[0]) < 252);
    expect("  .. gm_units: 32768 is 240 (0 dBFS), 16384 is 224 (-6 dB), 4096 is 192, under 16 is 0",
           gm_units(32768) == 240 && gm_units(16384) == 224 && gm_units(4096) == 192 && gm_units(15) == 0);
    dsp_clean(DM_STEREO);
    meter_on = 0;
    meter_bus[0] = 0;
    input_on(&trk[1], 60, 127);
    for (k = 0; k < 20u; k++)
        mblk(1);
    expect("  .. STEREO: part 2's peak is kept, the buses are not scanned", trk[1].peak > 0 && !meter_bus[0] && !meter_bus[1]);
}
/* ------------------------------------------- the outputs, the speaker, MUTE --- */
/* DUELL 0.2.2 (owner: "only the RIGHT side in the headphones, the speaker silent"). The firmware has one stereo stream for the
 * headphones, line out, USB and the built-in speaker (hardware picks); there is no speaker feed of its own to sum. So the facts the
 * start relies on: SPLIT with only part 2 sounding is right only (a speaker that plays the left channel is silent: the reported
 * symptom), a part 1 that is MUTED is silent in every mode, and LAYER (the opening setup) carries a part in BOTH channels, equal, so
 * whichever channel the speaker uses it hears it at the level of one headphone channel (no summing, no gain step when both parts play) */
static void out_render(uint32_t mode, uint32_t mute1, uint32_t p1, uint32_t p2)
{
    uint32_t i, e = eng_of("ANALOG");
    dsp_clean(mode);
    host_preset(&trk[0], e, 0);
    host_preset(&trk[1], e, 1);
    trk[0].p[P_LEVEL] = trk[1].p[P_LEVEL] = 80;     /* (under the limiter: it would hide the sum) */
    trk[0].p[P_PAN] = trk[1].p[P_PAN] = 0;
    trk[0].p[P_MUTE] = (int16_t)mute1;
    rec_n = 0;
    mblk(8);
    for (i = 0; i < 3u; i++) {
        if (p1)
            trk_note_on(&trk[0], 48u + 7u * i, 110);
        if (p2)
            trk_note_on(&trk[1], 48u + 7u * i, 110);
    }
    mblk(FS / 2u / CTL);
}
static int32_t diff_peak(void)                           /* max |L - R| of the render */
{
    uint32_t i;
    int32_t m = 0;
    for (i = 0; i < rec_n; i++) {
        int32_t d = rec_l[i] - rec_r[i];
        m = d < 0 && -d > m ? -d : d > m ? d : m;
    }
    return m;
}
static void t_outputs(void)
{
    int32_t pl, pr, both_l, one_l;
    out_render(DM_SPLIT, 0, 0, 1);
    pl = peak_of(rec_l, rec_n);
    pr = peak_of(rec_r, rec_n);
    printf("mode: SPLIT, part 2 alone: left %d, right %d\n", pl, pr);
    expect("SPLIT, only part 2 sounds: the left channel is silent, the right loud (hard pan stays on every output)", pl == 0 && pr > 3000);
    out_render(DM_LAYER, 1, 1, 1);
    pl = peak_of(rec_l, rec_n);
    pr = peak_of(rec_r, rec_n);
    printf("mode: LAYER, part 1 muted, part 2 plays: left %d, right %d, |L-R| %d\n", pl, pr, diff_peak());
    expect("LAYER with part 1 MUTED: part 2 is in BOTH channels, equal (a speaker on either channel hears it)",
           pl > 3000 && pr > 3000 && diff_peak() <= 2);
    out_render(DM_LAYER, 0, 0, 1);
    one_l = peak_of(rec_l, rec_n);
    out_render(DM_LAYER, 0, 1, 1);
    both_l = peak_of(rec_l, rec_n);
    printf("mode: LAYER peak, part 2 alone %d, both parts %d (%.1f dB)\n", one_l, both_l, 20.0 * log10((double)both_l / (one_l ? one_l : 1)));
    expect("LAYER: both parts play: both channels equal, the peak up over one part by less than 6 dB (the headroom rule: no gain step, master limiter only)",
           diff_peak() <= 2 && both_l > one_l && both_l < one_l * 2);
    out_render(DM_LAYER, 1, 1, 0);
    pl = peak_of(rec_l, rec_n) + peak_of(rec_r, rec_n);
    out_render(DM_SPLIT, 1, 1, 0);
    pr = peak_of(rec_l, rec_n) + peak_of(rec_r, rec_n);
    expect("MUTE: a muted part 1 sounds nothing, in LAYER (with part 2 not playing) and in SPLIT", pl == 0 && pr == 0);
}
int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    t_stereo();                                          /* (first: the process's own power-on state) */
    t_outputs();
    t_isolation();
    t_leakmap();
    t_jiant_rev();
    t_switch_iso();
    t_superpose();
    t_layer();
    t_arp_lfo();
    t_lfo_trig_default();
    t_mix();
    t_hanging();
    t_click();
    t_cc();
    t_ui();
    t_settings();
    t_midi_ch();
    t_meter();
    t_cost();
    printf("mode: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
#endif
