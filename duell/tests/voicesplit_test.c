/* SPDX-License-Identifier: GPL-3.0-only */
/* VOICE SHARE stage B2, step 3 (design: working docs/voice-split-b2-design_2026-11-03.md, section 7): the render of part 1 may be split between two cores; the split must not change
 * a single output sample. Core 1 is simulated IN-THREAD through the test seam of voice.c (-DDUELL_VS_TEST): vs_mask_fn(block, nr) names the ranks of the render list that "core 1" renders;
 * track_render renders that set into pscr_c1 (cleared first) with the same function core 0 uses and merges it (vs_merge) before spread_sum. The take-over of a late core 1 is the same path
 * (core 0 renders the masked set itself), so this test covers it too. No threads, no timing: host only, each run in its own process from power-on (scene_run's way).
 *
 * The scene is scene 4 of tests/partindep_scn.h (LAYER, MORPH, SPREAD 60 on part 1, EQ, PART FX crossfade, a PAN step; PART 2 NEVER PLAYS, as the share needs). Only part 1's sound is
 * changed by a hook in the block function (the first block): the engine (host_preset), a long sustain, an attack of 4, POLY, and SPREAD 60 or OFF. The scene's own 4-note chord (block 10) stays.
 * On top of it this file plays an 8-note chord from block 2 (the start limit VS_MAX starts one voice a block), a note-off and re-strike of one note (blocks 60 and 61: the offp / offn path),
 * and the release of all at block 110; 150 blocks are rendered (the rest of the scene's 3 s are silent blocks, they cost nothing).
 *   EQUALITY    per engine and sound: the reference is the empty mask; every mask must give the same hash of every output sample of every block (mix_block's output, the part's peak) and of
 *               the voices' end state (voice_t fields and the SUB slots of part 1). ANALOG, WAVE: all 255 masks of 8 voices; FM6, VOICE, PHYS: all masks of the 8 notes the scene plays, or of the engine's own ceiling when lower (PHYS 3; FM6 holds 12 and VOICE every slot, so both sound all 8);
 *               TRIO, WHEEL: 8 singles, 8 complements, alternating, halves and 14 random masks. ANALOG with SPREAD off: the same sampled set. Masks are over the RANKS of the render list
 *               (the allocator leaves holes), a mask bit above the list is ignored by track_render. Also: a mask that changes EVERY block (3 seeds: voices migrate between the cores),
 *               and the alternating pair 0xAAAA / 0x5555 block by block; and the seam with no function at all (NULL) equals the empty mask.
 *   16 VOICES   (voice pool, owner rules G22 / G23, 2026-11-06: a part sounds up to its 16 slots; the budget is not asked here) a second chord of 8 notes (an 11th above the first eight) is played
 *               at block 40 and released at 110, so 16 voices sound (it was a copy of each voice into slot i + 8, when 8 was the most a part could sound): ANALOG, WAVE, TRIO (no engine cap, no COMB). All 16 single masks, all 16 complements, 0x5555, 0xAAAA, halves,
 *               all-on-core-1 and random masks (120 / 60 / 60). The slots 8..15 of the per-slot arrays of the engines are then touched: run it under ASan once.
 *   DIRTY       "engines only ADD into dst" (design section 4): part_voice is run by hand and the raw voice sum (pscr[0].buf after track_render, SPREAD folded in) is taken as the block's
 *               result with the garbage put in. Through the seam the mask function fills the part's out and SPREAD buffers with random words (full range, some at 0x7FFFFFFF / 0x80000000) AFTER
 *               the control pass and BEFORE the render, and pscr_c1 with random words before its clear. The sum minus the words put in must be the sum without them, bit for bit, and the voices'
 *               end state equal: for each of the 7 engines with the empty mask and with mask 0xAA, and ANALOG with 16 voices (0xAAAA). It proves the engines' adds, voice_step's baseline
 *               (dst[0] - b0) and that the merge's source is cleared.
 * Not covered: threads and timing (twocore_sim.c, step 6); a garbage-filled pscr_c1 that core 1 itself must clear (the seam clears it, and the clear is checked by the mutants: the mask
 * function dirties pscr_c1 first); f2s / drw_v / the engine memory slots are not hashed (padding of those structs is unknown), only seen through the audio that follows; part 2 sharing
 * (never) and the 7 engines not named above (LOFI, NOISE, PHASE).
 * Self-check: host_tests.sh voicesplit builds this file with -quick against copies of voice.c with ONE defect each (merge twice, pscr_c1 not cleared, SPREAD side not merged, a stale
 * control record, a voice in both sets, a part scalar written in the render) and each must make it FAIL.
 * Options: -quick (ANALOG 8 voices sampled, ANALOG 16 voices sampled, DIRTY ANALOG; stops at the first FAIL; the mutants run it).
 * Estimated run time (GUESS, not measured): about 1,100 runs of 150 blocks, 5 to 15 s.
 * Built and run by DUELL/host_tests.sh (name: voicesplit) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "mode_scn.h"                                   /* (scn_eng) */
#include <unistd.h>
#include <sys/wait.h>
#include "partindep_scn.h"                              /* (scene, hfeed) */

static int bad, ncheck, quick;
static int expect(const char *what, int ok)
{
    ncheck++;
    if (!ok) {
        printf("voicesplit: FAIL %s\n", what);
        bad++;
        if (quick)
            exit(1);
    }
    return ok;
}

#define VS_NBLK 150u                                    /* blocks rendered of the scene's 3 s */
enum { VM_FIX, VM_ALT, VM_MIG, VM_NULL };               /* the mask function: one mask; 0xAAAA / 0x5555 by block; a new random mask every block; no function */
static struct {
    uint32_t eng;
    int mode, clone, sprd, garbage, dirty;
    uint32_t fix, seed;
    uint32_t bc, calls, used, maxnr;
    uint32_t inj[CTL];                                  /* the words put into the part's buffers this block (their sum per sample) */
} vsm;

static uint32_t xs(uint32_t *s)
{
    *s ^= *s << 13;
    *s ^= *s >> 17;
    *s ^= *s << 5;
    return *s;
}

/* the seam: the ranks "core 1" renders; with GARBAGE also the dirt (after the control pass, before the render) */
static uint32_t vs_test_mask(uint32_t block, uint32_t nr)
{
    uint32_t m = 0, k, s;
    vsm.calls++;
    if (nr > vsm.maxnr)
        vsm.maxnr = nr;
    switch (vsm.mode) {
    case VM_FIX:
        m = vsm.fix;
        break;
    case VM_ALT:
        m = block & 1u ? 0x5555u : 0xAAAAu;
        break;
    default:
        s = vsm.seed ^ (block * 2654435761u);
        if (!s)
            s = 1;
        xs(&s);
        m = xs(&s) & 0xFFFFu;
        break;
    }
    if (vsm.garbage) {
        s = 0x9E3779B9u ^ (block * 40503u);
        for (k = 0; k < CTL; k++) {
            uint32_t g = xs(&s);
            if (block % 4u == 1u)
                g = 0x7FFFFFFFu;
            else if (block % 4u == 2u)
                g = 0x80000000u;
            pscr[0].buf[k] = (int32_t)g;
            vsm.inj[k] = g;
        }
        if (pscr[0].spread_on)
            for (k = 0; k < CTL; k++) {
                uint32_t g = xs(&s);
                pscr[0].sbuf[k] = (int32_t)g;
                vsm.inj[k] += g;
            }
        for (k = 0; k < CTL; k++) {
            pscr_c1.buf[k] = (int32_t)xs(&s);
            pscr_c1.sbuf[k] = (int32_t)xs(&s);
        }
    }
    if (nr && (m & ((1u << nr) - 1u)))
        vsm.used++;
    return m;
}

/* the voices' end state: the fields a render writes, and the SUB slots of part 1 */
static void vs_state_feed(void)
{
    uint32_t i, k;
    for (i = 0; i < NVOICE; i++) {
        const voice_t *v = &trk[0].v[i];
        hfeed((uint32_t)v->note | (uint32_t)v->vel << 8 | (uint32_t)v->gate << 16 | (uint32_t)v->active << 24);
        hfeed((uint32_t)v->stage | (uint32_t)v->side << 8 | (uint32_t)v->stage2 << 16 | (uint32_t)v->chn << 24);
        hfeed((uint32_t)v->env); hfeed((uint32_t)v->env_out); hfeed((uint32_t)v->env2);
        hfeed((uint32_t)v->pitch16); hfeed((uint32_t)v->pitch_cur); hfeed((uint32_t)v->fine);
        for (k = 0; k < 3u; k++)
            hfeed(v->ph[k]);
        for (k = 0; k < 8u; k++)
            hfeed((uint32_t)v->s[k]);
        for (k = 0; k < 4u; k++)
            hfeed((uint32_t)v->uf[k]);
        for (k = 0; k < 3u; k++)
            hfeed((uint32_t)(uint16_t)v->ufw[k]);
        hfeed((uint32_t)v->uft | (uint32_t)v->offn << 8 | (uint32_t)v->offp << 16);
        hfeed((uint32_t)v->apitch); hfeed((uint32_t)v->again); hfeed(v->age);
        hfeed((uint32_t)v->lastc); hfeed((uint32_t)v->lastd); hfeed((uint32_t)v->offc); hfeed((uint32_t)v->msl);
        hfeed(sub_ph[0][i]); hfeed(sub_g[0][i]); hfeed(sub_cfg[0][i]);
    }
}

/* the part's voice stage by hand (partindep_test.c's SV step), the raw sum minus the dirt put in as the block's result */
static void vs_dirty_block(int32_t *o, uint32_t n)
{
    static blockstate_t bs;
    static mixpart_t pm;
    uint32_t k;
    int mo;
    mix_events(n);
    blk_snap(&bs, 0);
    for (k = 0; k < CTL; k++)
        vsm.inj[k] = 0;
    mo = morph_begin(&trk[0], 1);
    part_voice(&pm, &trk[0], n, &bs);
    for (k = 0; k < CTL; k++)
        o[k] = (int32_t)((uint32_t)pscr[0].buf[k] - vsm.inj[k]);
    if (mo)
        morph_end(&trk[0]);
}

/* the block function of the scene: setup in block 0, the chord, the clones, the release; then mix_block or the dirty stage */
static void vs_blockfn(int32_t *o, uint32_t n)
{
    static const uint8_t N8[8] = {40, 43, 47, 50, 53, 57, 62, 67};
    uint32_t b = vsm.bc++, i;
    if (b >= VS_NBLK)
        return;
    if (b == 0) {
        host_preset(&trk[0], vsm.eng, 0);
        trk[0].p[P_VOICE] = V_POLY;
        trk[0].p[P_ATK] = 4;
        trk[0].p[P_SUS] = 110;
        trk[0].p[P_REL] = 70;
        if (vsm.sprd >= 0)
            trk[0].p[P_SPRD] = (int16_t)vsm.sprd;
    }
    if (b == 2u)
        for (i = 0; i < 8u; i++)
            trk_note_on(&trk[0], N8[i], 110);
    if (b == 40u && vsm.clone)                          /* (voice pool: the firmware sounds 16 voices a part, so the second eight are real notes: an 11th above the first eight, none on a note of them) */
        for (i = 0; i < 8u; i++)
            trk_note_on(&trk[0], (uint8_t)(N8[i] + 11u), 110);
    if (b == 60u)
        trk_note_off(&trk[0], N8[0]);
    if (b == 61u)
        trk_note_on(&trk[0], N8[0], 100);
    if (b == 110u) {
        for (i = 0; i < 8u; i++)
            trk_note_off(&trk[0], N8[i]);
        if (vsm.clone)
            for (i = 0; i < 8u; i++)
                trk_note_off(&trk[0], (uint8_t)(N8[i] + 11u));
    }
    if (vsm.dirty)
        vs_dirty_block(o, n);
    else
        mix_block(o, n);
    if (b == VS_NBLK - 1u)
        vs_state_feed();
}

/* one run in a child process (power-on state): the hash and the seam's counters through a pipe */
typedef struct { uint64_t h; uint32_t calls, used, maxnr; } vres_t;
static vres_t vrun(uint32_t eng, int mode, uint32_t fix, uint32_t seed, int clone, int sprd, int garbage, int dirty)
{
    int fd[2];
    pid_t pid;
    vres_t r = {0, 0, 0, 0};
    int st = 0;
    fflush(stdout);
    if (pipe(fd))
        return r;
    pid = fork();
    if (pid == 0) {
        vres_t c;
        ssize_t w;
        memset(&vsm, 0, sizeof vsm);
        vsm.eng = eng; vsm.mode = mode; vsm.fix = fix; vsm.seed = seed; vsm.clone = clone; vsm.sprd = sprd; vsm.garbage = garbage; vsm.dirty = dirty;
        vs_mask_fn = mode == VM_NULL ? 0 : vs_test_mask;
        vs_blk = 0;
        c.h = scene(4, vs_blockfn);
        c.calls = vsm.calls; c.used = vsm.used; c.maxnr = vsm.maxnr;
        w = write(fd[1], &c, sizeof c);
        _exit(w == (ssize_t)sizeof c ? 0 : 3);
    }
    close(fd[1]);
    if (read(fd[0], &r, sizeof r) != (ssize_t)sizeof r)
        memset(&r, 0, sizeof r);
    close(fd[0]);
    waitpid(pid, &st, 0);
    if (!WIFEXITED(st) || WEXITSTATUS(st))
        memset(&r, 0, sizeof r);
    return r;
}

static uint32_t gn, gbad;
static void gcheck(const char *grp, const char *what, uint64_t h, uint64_t ref)
{
    gn++;
    ncheck++;
    if (h != ref) {
        gbad++;
        bad++;
        if (gbad <= 3u)
            printf("voicesplit: FAIL %s: %s: hash %016llx, reference %016llx\n", grp, what, (unsigned long long)h, (unsigned long long)ref);
        if (quick)
            exit(1);
    }
}
static void gend(const char *grp)
{
    if (gbad)
        printf("voicesplit: FAIL %s: %u of %u runs differ from the reference\n", grp, gbad, gn);
    else
        printf("voicesplit: %s: %u runs equal to the reference ok\n", grp, gn);
    gn = gbad = 0;
}

enum { K_EXH, K_SAMP };
/* one sound: the reference (empty mask), the masks, the migrating and alternating masks */
static void group(const char *eng, int clone, int sprd, uint32_t kind, uint32_t nrand)
{
    static uint32_t ml[600];
    uint32_t e = scn_eng(eng), poly = ENGINES[e]->poly, cap = poly && poly < 8u ? poly : 8u, w = clone ? 16u : cap;   /* (the scene's chord is 8 notes: an engine that holds more, or every slot, sounds all 8) */
    uint32_t full = (1u << w) - 1u, nm = 0, i, s = 0x2545F491u + w * 977u + nrand, sd;
    char grp[120], what[120];
    vres_t ref, r;
    if (quick) {
        kind = K_SAMP;
        nrand = 16;
    }
    snprintf(grp, sizeof grp, "%s %u voices, SPREAD %s, masks", eng, w, sprd == 0 ? "OFF" : "60");
    if (kind == K_EXH) {
        for (i = 1; i <= full; i++)
            ml[nm++] = i;
    } else {
        for (i = 0; i < w; i++) {
            ml[nm++] = 1u << i;
            ml[nm++] = full ^ (1u << i);
        }
        ml[nm++] = 0x5555u & full; ml[nm++] = 0xAAAAu & full; ml[nm++] = 0x00FFu & full; ml[nm++] = 0xFF00u & full; ml[nm++] = 0x0F0Fu & full; ml[nm++] = full;
        for (i = 0; i < nrand; i++) {
            uint32_t m = xs(&s) & full;
            ml[nm++] = m ? m : 1u;
        }
    }
    ref = vrun(e, VM_FIX, 0, 0, clone, sprd, 0, 0);
    snprintf(what, sizeof what, "%s: the reference is silent", grp);
    expect(what, ref.h != 0);
    snprintf(what, sizeof what, "%s: the scene reaches %u voices (only %u)", grp, w, ref.maxnr);
    expect(what, ref.maxnr >= w);
    r = vrun(e, VM_NULL, 0, 0, clone, sprd, 0, 0);
    gcheck(grp, "no mask function", r.h, ref.h);
    for (i = 0; i < nm; i++) {
        r = vrun(e, VM_FIX, ml[i], 0, clone, sprd, 0, 0);
        snprintf(what, sizeof what, "mask %04x", ml[i]);
        gcheck(grp, what, r.h, ref.h);
        if (ml[i] & ((1u << ref.maxnr) - 1u)) {              /* a mask that names a voice that exists must really send it to the other set */
            snprintf(what, sizeof what, "%s: mask %04x was never used (the seam is dead)", grp, ml[i]);
            expect(what, r.used > 0);
        }
    }
    for (sd = 1; sd <= (quick ? 1u : 3u); sd++) {
        r = vrun(e, VM_MIG, 0, sd * 7919u, clone, sprd, 0, 0);
        snprintf(what, sizeof what, "a new random mask every block, seed %u", sd);
        gcheck(grp, what, r.h, ref.h);
        snprintf(what, sizeof what, "%s: the migrating masks were never used", grp);
        expect(what, r.used > 0);
    }
    r = vrun(e, VM_ALT, 0, 0, clone, sprd, 0, 0);
    gcheck(grp, "0xAAAA / 0x5555 by block", r.h, ref.h);
    gend(grp);
}

/* "engines only add": the dirty runs against the clean ones, with the empty mask and with a mask */
static void dirty(const char *eng, int clone)
{
    uint32_t e = scn_eng(eng), mk = clone ? 0xAAAAu : 0xAAu;
    char grp[120], what[120];
    vres_t r0 = vrun(e, VM_FIX, 0, 0, clone, -1, 0, 1), r;
    snprintf(grp, sizeof grp, "%s %s dirty buffers", eng, clone ? "16 voices" : "8 voices");
    snprintf(what, sizeof what, "%s: the clean run is silent", grp);
    expect(what, r0.h != 0);
    r = vrun(e, VM_FIX, 0, 0, clone, -1, 1, 1);
    gcheck(grp, "empty mask, garbage in out and SPREAD", r.h, r0.h);
    r = vrun(e, VM_FIX, mk, 0, clone, -1, 0, 1);
    gcheck(grp, "mask, no garbage", r.h, r0.h);
    r = vrun(e, VM_FIX, mk, 0, clone, -1, 1, 1);
    gcheck(grp, "mask, garbage in out, SPREAD and pscr_c1", r.h, r0.h);
    gend(grp);
}

int main(int argc, char **argv)
{
    static const char *const DE[7] = {"ANALOG", "WAVE", "FM6", "VOICE", "PHYS", "TRIO", "WHEEL"};
    uint32_t k;
    quick = argc > 1 && !strcmp(argv[1], "-quick");
    group("ANALOG", 0, -1, K_EXH, 0);
    group("ANALOG", 1, -1, K_SAMP, 120);
    dirty("ANALOG", 0);
    if (!quick) {
        group("WAVE", 0, -1, K_EXH, 0);
        group("FM6", 0, -1, K_EXH, 0);
        group("VOICE", 0, -1, K_EXH, 0);
        group("PHYS", 0, -1, K_EXH, 0);
        group("TRIO", 0, -1, K_SAMP, 14);
        group("WHEEL", 0, -1, K_SAMP, 14);
        group("ANALOG", 0, 0, K_SAMP, 12);
        group("WAVE", 1, -1, K_SAMP, 60);
        group("TRIO", 1, 0, K_SAMP, 60);
        for (k = 1; k < 7u; k++)
            dirty(DE[k], 0);
        dirty("ANALOG", 1);
    }
    printf("voicesplit: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
