/* SPDX-License-Identifier: GPL-3.0-only */
/* Job 3, one-sided ducking: DUCK SRC PART on ONE part only (params.c duck_set_part). The ducked part reads the other part's key level from the BLOCK BEFORE
 * (the block's snapshot, fx.c duck_part). Before, a ducked part 2 read part 1's key of THIS block (part 1 had already run): one block (0.73 ms) earlier.
 * This test renders each scene through the old path (tests/mixsplit_ref.h with the old duck_part below, the shared key read live) and the new one
 * (mix_block) and reports the difference: the largest sample difference and the RMS of the difference against the RMS of the new output, in dB.
 *   scene 0  part 2 ducked by part 1 (the case that changes: expected to differ slightly)
 *   scene 1  part 1 ducked by part 2 (read the key of the block before already: expected bit for bit equal)
 * Built and run by DUELL/host_tests.sh (name: duckpart) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "mode_scn.h"                                   /* (scn_eng) */
#include <unistd.h>
#include <sys/wait.h>

/* the duck_part of before job 3, verbatim but for the key read: `ok->key` live (the keying part's follower as it stands when this part runs) */
static void old_duck_part(track_t *t, const int32_t *b, uint32_t n, const blockstate_t *bs)
{
    uint32_t id = (uint32_t)(t - trk) % NTRK, o = id ^ 1u, i;
    duck_t *k = &duck_st[id], *ok = &duck_st[o];
    (void)bs;
    if (trk[o].p[P_DKSRC] == 1) {
        int32_t pk = 0, c;
        if (b)
            for (i = 0; i < n; i++) {
                int32_t a = b[i] < 0 ? -b[i] : b[i];
                if (a > pk)
                    pk = a;
            }
        pk = ((pk >> 2) * part_lvl(t)) >> 10;
        pk = pk > 32767 ? 32767 : pk;
        c = (int32_t)(n * 32768u / (n + (pk > k->key ? 88u : 2646u)));
        k->key += ((pk - k->key) * c) >> 15;
    }
    if (t->p[P_DKSRC] == 1 && t->p[P_DKDEP] > 0) {
        int32_t thr = clamp(t->p[P_DKTHR], 0, 100) * (DUCK_KEY_FS / 100), kv = ok->key, raw = 0;
        int32_t top = (int32_t)((uint32_t)clamp(t->p[P_DKDEP], 0, 100) << 16) / 100;
        if (kv > thr && thr < DUCK_KEY_FS) {
            int32_t f = ((kv - thr) << 12) / (DUCK_KEY_FS - thr);
            raw = (top * (f > 4096 ? 4096 : f)) >> 12;
        }
        k->on = raw > 0;
        k->tg = raw;
        if (raw > 0 || k->tg || k->dn)
            k->ph = 4;
    } else if (k->ph == 4)
        k->ph = 3;
}
#define duck_part old_duck_part
#include "mixsplit_ref.h"
#undef duck_part

#define NB (2u * FS / CTL)
static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    if (!ok) {
        printf("duckpart: FAIL %s\n", what);
        bad++;
    }
    return ok;
}
typedef void (*blk_t)(int32_t *, uint32_t);
/* a scene: the keying part plucks a chord every 58 ms, the ducked part holds one; the ducked part's SRC is PART, DEPTH 90, THR 10 */
static void scene(uint32_t id, blk_t blk, int32_t *res)
{
    static const uint8_t C1[3] = {36, 43, 48}, C2[3] = {60, 64, 67};
    uint32_t kp = id ? 1u : 0u, dp = kp ^ 1u, b, i;
    host_tracks_init();
    host_preset(&trk[kp], scn_eng("ANALOG"), 1);
    host_preset(&trk[dp], scn_eng("PHASE"), 2);
    for (i = 0; i < 2u; i++) {
        trk[i].p[P_VOICE] = V_POLY;
        trk[i].p[P_SUS] = 110;
        trk[i].p[P_REL] = 40;
        trk[i].p[P_PAN] = i ? 30 : -30;
        trk[i].p[P_CHOR] = trk[i].p[P_DLY] = trk[i].p[P_REV] = 30;
    }
    trk[kp].p[P_DKSRC] = 3;                             /* the keying part has no ducker (OFF) */
    trk[dp].p[P_DKSRC] = 1;
    trk[dp].p[P_DKDEP] = 90;
    trk[dp].p[P_DKTHR] = 10;
    trk[dp].p[P_DKATK] = 10;
    trk[dp].p[P_DKRLS] = 40;
    for (i = 0; i < 3u; i++)
        trk_note_on(&trk[dp], C2[i], 100);
    for (b = 0; b < NB; b++) {
        int32_t o[2 * CTL];
        if (b % 80u == 0u)
            for (i = 0; i < 3u; i++)
                trk_note_on(&trk[kp], C1[i], 120);
        if (b % 80u == 40u)
            for (i = 0; i < 3u; i++)
                trk_note_off(&trk[kp], C1[i]);
        blk(o, CTL);
        memcpy(res + b * 2u * CTL, o, sizeof o);
    }
}
static int32_t out_old[NB * 2u * CTL], out_new[NB * 2u * CTL];
static int run_child(uint32_t id, blk_t blk, int32_t *dst)
{
    int fd[2], st = 0;
    size_t got = 0, want = sizeof out_old;
    pid_t pid;
    fflush(stdout);
    if (pipe(fd))
        return 0;
    pid = fork();
    if (pid == 0) {
        static int32_t r[NB * 2u * CTL];
        size_t sent = 0;
        scene(id, blk, r);
        while (sent < want) {
            ssize_t w = write(fd[1], (char *)r + sent, want - sent);
            if (w <= 0)
                _exit(3);
            sent += (size_t)w;
        }
        _exit(0);
    }
    close(fd[1]);
    while (got < want) {
        ssize_t r = read(fd[0], (char *)dst + got, want - got);
        if (r <= 0)
            break;
        got += (size_t)r;
    }
    close(fd[0]);
    waitpid(pid, &st, 0);
    return got == want && WIFEXITED(st) && WEXITSTATUS(st) == 0;
}
/* the rule in the parameter layer: set, load (norm), the hidden cards, MIDI */
static void rules(void)
{
    host_tracks_init();
    trk[0].p[P_DKSRC] = 0;                              /* PUMP */
    trk[1].p[P_DKSRC] = 1;
    duck_set_part(1);
    expect("rule: part 2 set to PART takes part 1's ducker away (OFF)", trk[0].p[P_DKSRC] == 3 && trk[1].p[P_DKSRC] == 1);
    trk[0].p[P_DKSRC] = 1;
    duck_set_part(0);
    expect("rule: part 1 set to PART afterwards (the part set last wins): part 2 is OFF", trk[1].p[P_DKSRC] == 3 && trk[0].p[P_DKSRC] == 1);
    trk[0].p[P_DKSRC] = trk[1].p[P_DKSRC] = 1;
    duck_src_norm();
    expect("rule: both loaded as PART: the lower index keeps PART, the other is OFF", trk[0].p[P_DKSRC] == 1 && trk[1].p[P_DKSRC] == 3);
    trk[0].p[P_DKSRC] = 2;
    trk[1].p[P_DKSRC] = 1;
    duck_src_norm();
    expect("rule: part 2 PART, part 1 MIDI (a pair loaded): part 1 is OFF", trk[0].p[P_DKSRC] == 3 && trk[1].p[P_DKSRC] == 1);
    trk[0].p[P_DKSRC] = 0;
    trk[1].p[P_DKSRC] = 2;
    duck_src_norm();
    expect("rule: no PART: nothing changes", trk[0].p[P_DKSRC] == 0 && trk[1].p[P_DKSRC] == 2);
    trk[0].p[P_DKSRC] = 3;
    trk[1].p[P_DKSRC] = 1;
    song.sel = 0;
    expect("hide: the OFF-forced part's SRC card is not drawn", duck_forced(&trk[0]) && card_hidden_vp(&trk[0].p[P_DKSRC]) && card_hidden_vp(&trk[0].p[P_DKDEP]) && card_hidden_vp(&trk[0].p[P_DKATK]));
    song.sel = 1;
    expect("hide: the PART part keeps its cards", !duck_forced(&trk[1]) && !card_hidden_vp(&trk[1].p[P_DKSRC]) && !card_hidden_vp(&trk[1].p[P_DKDEP]) && !card_hidden_vp(&trk[1].p[P_DKTHR]));
    trk[1].p[P_DKSRC] = 0;
    song.sel = 0;
    trk[0].p[P_DKSRC] = 0;
    expect("hide: no PART: the SRC card is there", !duck_forced(&trk[0]) && !card_hidden_vp(&trk[0].p[P_DKSRC]));
    trk[0].p[P_DKSRC] = 0;
    midi_cc_set(&trk[0], P_DKSRC, 127);
    expect("MIDI: a CC does not set DUCK SRC", trk[0].p[P_DKSRC] == 0);
}
int main(void)
{
    uint32_t id, i;
    rules();
    for (id = 0; id < 2u; id++) {
        double sn = 0, sd = 0, db;
        int32_t mx = 0, pk = 0;
        char what[160];
        snprintf(what, sizeof what, "scene %u renders", id);
        if (!expect(what, run_child(id, ref_mix_block, out_old) && run_child(id, mix_block, out_new)))
            continue;
        for (i = 0; i < NB * 2u * CTL; i++) {
            int32_t d = out_new[i] - out_old[i], a = d < 0 ? -d : d, p = out_new[i] < 0 ? -out_new[i] : out_new[i];
            mx = a > mx ? a : mx;
            pk = p > pk ? p : pk;
            sd += (double)d * d;
            sn += (double)out_new[i] * out_new[i];
        }
        db = sd > 0 ? 10.0 * log10(sd / sn) : -999.0;
        if (sd > 0)
            printf("duckpart: scene %u (%s ducked by the other): largest sample difference %d (output peak %d), RMS of the difference %.1f dB below the output\n", id,
                   id ? "part 1" : "part 2", mx, pk, -db);
        else
            printf("duckpart: scene %u (%s ducked by the other): no difference (output peak %d)\n", id, id ? "part 1" : "part 2", pk);
        snprintf(what, sizeof what, "scene %u: the output is not silent", id);
        expect(what, pk > 1000);
        if (id == 0u) {
            expect("scene 0 (part 2 ducked by part 1): the new path DIFFERS (the key is a block older)", mx > 0);
            expect("scene 0: the difference is small (RMS under -20 dB of the output)", db < -20.0);
        } else {
            expect("scene 1 (part 1 ducked by part 2): bit for bit equal to the old path", mx == 0);
        }
    }
    printf("duckpart: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
