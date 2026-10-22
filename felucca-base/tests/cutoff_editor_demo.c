/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.25, the owner's report "the cutoff crackles when I move it in the editor on #83 TINE ROOM": a measurement, no check (exit 0). TINE ROOM (FM6, factory
 * sound 4; the multimode filter is bypassed at CUT 82, KEY TRK at the new-sound default) holds a four-note chord through the real voice path (uf_test.c's
 * harness: hostsim, mblk); the multimode filter's CUT (P_FREEA) is set before each audio block (32 samples) the way the editor's writes reach the unit:
 *   A  CUT still at 50 (the filter running): the reference
 *   B  50 Hz writes of 1 dial step (a slow drag)        C  60 Hz writes of 6 steps (a fast drag, the editor before 0.2.25)
 *   D  60 Hz writes of 20 steps (a flick)               E  50 Hz writes of at most 4 steps (the editor's slew, 0.2.25)
 *   F  every block 1 step (the smoothest a SET stream can be)
 *   G  CUT toggled 82 <-> 81 at 50 Hz (in and out of the BYPASS: the filter restarts from rest each time)
 * For each: the largest sample-to-sample step and the largest second difference (|x[n] - 2 x[n-1] + x[n-2]|: a click is a corner) over the 1.2 s after 0.3 s, and
 * the same measured only in the blocks right after a change. Build: cc -O1 -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests
 *   -DFELUCCA_VERSION=... -o build/host/cutoff_editor_demo tests/cutoff_editor_demo.c -lm */
#define UF_TEST_NO_MAIN 1
#include "uf_test.c"

static int32_t SCH;                                       /* the scenario */
static int32_t cur;
static uint8_t changed_at[MAXB];
static void fb_sched(track_t *t, uint32_t b)
{
    int32_t start = 300 * FS / 1000 / CTL, per = SCH == 2 || SCH == 3 ? 25 : 30, to = 18;
    int32_t was = cur;
    if (b == 0)
        cur = SCH == 6 ? 82 : SCH == 0 ? 50 : 82;
    else if ((int32_t)b >= start && SCH != 0) {
        if (SCH == 5) cur = cur > to ? cur - 1 : cur;
        else if (((int32_t)b - start) % per == 0) {
            int32_t st = SCH == 1 ? 1 : SCH == 2 ? 6 : SCH == 3 ? 20 : SCH == 4 ? 4 : 0;
            if (SCH == 6) cur = cur == 82 ? 81 : 82;
            else cur = cur - st < to ? to : cur - st;
        }
    }
    changed_at[b] = cur != was && b > 0;
    t->p[P_FREEA] = (int16_t)cur;
}
static void child(void)
{
    static const uint8_t CH[4] = {48, 55, 60, 64};
    track_t *t = &trk[0];
    uint32_t i, b, e = eng_named("FM6");
    dsp_clean();
    host_preset(t, e, 4);                                 /* TINE ROOM */
    rec_n = 0;
    for (i = 0; i < 4u; i++)
        trk_note_on(t, CH[i], 100);
    for (b = 0; b < (uint32_t)(1.5 * FS / CTL); b++) {
        fb_sched(t, b);
        mblk(1);
    }
}
static void measure(const char *name)
{
    uint32_t from = 300u * FS / 1000u, i;
    int32_t mstep = 0, md2 = 0, cd2 = 0;
    pid_t pid;
    static uint8_t ch[MAXB];
    uint8_t *sh = mmap(0, MAXB, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    fflush(stdout);
    if (!(pid = fork())) { child(); memcpy(sh, changed_at, MAXB); _exit(0); }
    waitpid(pid, 0, 0);
    memcpy(ch, sh, MAXB);
    for (i = from + 2u; i < rec_n; i++) {
        int32_t d = rec_l[i] - rec_l[i - 1u], d2 = rec_l[i] - 2 * rec_l[i - 1u] + rec_l[i - 2u];
        d = d < 0 ? -d : d; d2 = d2 < 0 ? -d2 : d2;
        if (d > mstep) mstep = d;
        if (d2 > md2) md2 = d2;
        if ((ch[i / CTL] || (i / CTL && ch[i / CTL - 1u])) && d2 > cd2) cd2 = d2;
    }
    printf("%-60s largest step %7d   largest corner %7d   corner in a changed block %7d   peak %7d\n", name, mstep, md2, cd2, peak_all(from));
}
int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    shared_init();
    printf("TINE ROOM (FM6 #4), a C3 G3 C4 E4 chord, the multimode CUT set before each 32-sample block (48 kHz)\n");
    const char *n[] = {"A  CUT 50, still (reference)", "B  50 Hz, 1 step a write", "C  60 Hz, 6 steps a write (editor before 0.2.25, fast drag)",
                       "D  60 Hz, 20 steps a write (a flick)", "E  50 Hz, at most 4 steps a write (editor 0.2.25)", "F  1 step every block",
                       "G  82 <-> 81 at 33 Hz (in and out of the bypass)"};
    for (SCH = 0; SCH < 7; SCH++)
        measure(n[SCH]);
    return 0;
}
