/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Engine switches on a part, hashed (1.5: the engines' per-part state shares one region, engines.c eng_mem).
 *   build/host/union_test [GOLDEN_FILE]      (run_tests.sh builds and runs it against tests/union_golden.txt)
 *
 * Every pair (and A -> B -> A) of the engines that keep state per part (GRAIN, PHYS, DRUM, FM6, SLICE), with
 * ANALOG, SAMPLE and WHEEL (state outside the region) among them, on part 1 while part 3 plays other engines
 * alongside, at three presets: a chord and a low note held, released, still ringing at the next switch (the
 * fade on the old engine), and once more with the next notes inside the fade (they wait for the switch).
 * Each sequence runs in its own fork()ed child from boot state; its mix is one 64-bit FNV-1a hash. The file
 * holds the hashes of 1.4 (the engines' own arrays, before the shared region): any difference fails. The 380
 * sequences that play SAMPLE's PIANO (SAMPLE presets 0 / 1, GRAIN's FROZEN / SHIMMER) were rewritten in 1.5, when
 * PIANO went back to the full piano of 1.1.5; the other 340 are 1.4's.
 * env: GOLDEN_UPDATE=1 rewrites GOLDEN_FILE (only with a reason: a switch is to sound as it did). */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <sys/wait.h>

static uint64_t uh;
static void ublock(void)
{
    int32_t o[2 * CTL];
    uint32_t i;
    mix_block(o, CTL);
    for (i = 0; i < 2u * CTL; i++) {
        uh ^= (uint32_t)o[i];
        uh *= 0x100000001B3ull;
    }
}
static void urun(uint32_t ms)
{
    uint32_t n = ms * (FS / 1000u) / CTL;
    while (n--)
        ublock();
}

/* engine e (preset pi) on t: notes base + a chord and base - 24; wait_ms before them (0: inside the fade) */
static void uplay(track_t *t, uint32_t e, uint32_t pi, uint32_t base, uint32_t wait_ms)
{
    static const uint8_t CH[4] = {0, 4, 7, 12};
    uint32_t i;
    host_preset_req(t, e, pi);                   /* eng_req: the audio side switches (voice.c engine_block) */
    urun(wait_ms);
    for (i = 0; i < 4u; i++)
        input_on(t, base + CH[i], 100);
    urun(400);
    input_on(t, base - 24u, 110);
    urun(300);
    for (i = 0; i < 4u; i++)
        input_off(t, base + CH[i]);
    input_off(t, base - 24u);
    urun(250);                                   /* still ringing at the next switch */
}

static const uint32_t UE[] = {0 /* ANALOG */, ENGI_SAMPLE, 7 /* WHEEL */, 8 /* GRAIN */, ENGI_PHYS, ENGI_DRUM, ENGI_FM6,
#if FELUCCA_SLICE
                              13 /* SLICE */
#endif
};
#define UNE (sizeof UE / sizeof UE[0])

static uint64_t useq(const uint32_t *e, uint32_t n, uint32_t pi, uint32_t wait_ms)
{
    pid_t pid;
    int fd[2];
    uint64_t h = 0;
    if (pipe(fd))
        return 0;
    if (!(pid = fork())) {
        uint32_t k;
        host_tracks_init();
        uh = 0xcbf29ce484222325ull;
        for (k = 0; k < n; k++) {
            uplay(&trk[0], e[k], pi + k, 60, wait_ms);
            uplay(&trk[2], UE[(k + 3u) % UNE], pi, 55, 10);   /* another part busy alongside */
        }
        urun(1000);
        if (write(fd[1], &uh, 8) != 8)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], &h, 8) != 8)
        h = 0;
    close(fd[0]);
    waitpid(pid, 0, 0);
    return h;
}

int main(int argc, char **argv)
{
    static char got[4096][64];
    uint32_t a, b, pi, w, n = 0, bad = 0, i;
    const char *gf = argc > 1 ? argv[1] : 0;
    FILE *f;
    for (w = 0; w < 2u; w++)
        for (pi = 0; pi < 3u; pi++)
            for (a = 0; a < UNE; a++)
                for (b = 0; b < UNE; b++) {
                    uint32_t s2[2] = {UE[a], UE[b]}, s3[3] = {UE[b], UE[a], UE[b]};
                    uint32_t wait = w ? 0u : 10u;
                    snprintf(got[n++], 64, "%u-%u p%u w%u %016llx", UE[a], UE[b], pi, wait,
                             (unsigned long long)useq(s2, 2, pi, wait));
                    if (a != b)
                        snprintf(got[n++], 64, "%u-%u-%u p%u w%u %016llx", UE[b], UE[a], UE[b], pi, wait,
                                 (unsigned long long)useq(s3, 3, pi, wait));
                }
    if (!gf) {
        for (i = 0; i < n; i++)
            printf("%s\n", got[i]);
        return 0;
    }
    if (getenv("GOLDEN_UPDATE")) {
        if (!(f = fopen(gf, "w")))
            return 1;
        for (i = 0; i < n; i++)
            fprintf(f, "%s\n", got[i]);
        fclose(f);
        printf("union_test: %u hashes written to %s\n", n, gf);
        return 0;
    }
    if (!(f = fopen(gf, "r"))) {
        printf("union_test: no %s\n", gf);
        return 1;
    }
    for (i = 0; i < n; i++) {
        char ln[96];
        if (!fgets(ln, sizeof ln, f))
            ln[0] = 0;
        ln[strcspn(ln, "\n")] = 0;
        if (strcmp(ln, got[i])) {
            if (bad < 20u)
                printf("union_test: FAIL %s (expected %s)\n", got[i], ln);
            bad++;
        }
    }
    fclose(f);
    printf("union_test: %u engine switch sequences, %u differ from 1.4: %s\n", n, bad, bad ? "FAIL" : "ok");
    return bad != 0;
}
