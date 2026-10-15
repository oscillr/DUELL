/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.17: the factory multis render bit for bit as they did before the 14 new factory sounds (bank 114 -> 128).
 * A factory multi (tools/factory_multis/Mnn.bin) carries its sounds as parameter bytes plus the identity (engine, factory index);
 * adding sounds appends new indices per engine and moves only the browse numbers (#NN), so M01..M10 and the test multis M13..M16
 * must sound the same. Each multi is loaded as the LOAD page does it and played (both parts: a chord of 3 notes, 1.5 s, then the
 * release, 1 s), at MORPH fader 0, 63 and 127 (End A, the middle, End B: the seeds and rolls of the part's sound), and the output is
 * hashed (FNV-1a 64). Every render runs in its own fork()ed child (state as at boot: noise seeds, FX buffers, limiter).
 *   build/host/multi_render_test GOLDEN        compare with the file (tests/multi_render_golden.txt)
 *   MULTI_RENDER_UPDATE=1 ... GOLDEN           write it (ONLY on a firmware whose sounds are the reference) */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define HOST_IRQ_HOOK 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static void fm1_irq_off(void);
static void fm1_irq_on(void);
#include "ui_test.c"
static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { const uint8_t *s = src; uint32_t i; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static const uint8_t CH1[3] = {48, 55, 64}, CH2[3] = {60, 67, 72};
static const uint32_t SLOT[14] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 13, 14, 15};
static const uint32_t FAD[3] = {0, 63, 127};

static uint64_t render(uint32_t slot, uint32_t fader)
{
    uint64_t h = 1469598103934665603ull;
    int32_t o[2 * CTL];
    uint32_t i, b, nb = (uint32_t)(1.5 * FS) / CTL, nr = (uint32_t)(1.0 * FS) / CTL;
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    multi_boot();
    frame();
    song.master_q12 = 4096;
    project_load(slot);
    for (i = 0; i < 40; i++) { memset(o, 0, sizeof o); mix_block(o, CTL); while (wave_service()) ; }
    morph_set_target(1);
    morph_set_fader(fader);
    for (i = 0; i < 3; i++) { trk_note_on(&trk[0], CH1[i], 100); trk_note_on(&trk[1], CH2[i], 100); }
    for (b = 0; b < nb + nr; b++) {
        if (b == nb)
            for (i = 0; i < 3; i++) { trk_note_off(&trk[0], CH1[i]); trk_note_off(&trk[1], CH2[i]); }
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        while (wave_service()) ;
        for (i = 0; i < 2u * CTL; i++) { h ^= (uint32_t)o[i]; h *= 1099511628211ull; }
    }
    return h;
}

int main(int argc, char **argv)
{
    char name[14][32], line[128];
    uint64_t want[42] = {0}, got[42];
    uint32_t m, f, bad = 0, n = 0;
    int upd = getenv("MULTI_RENDER_UPDATE") != 0;
    FILE *g;
    if (argc < 2)
        return fprintf(stderr, "usage: multi_render_test GOLDEN\n"), 2;
    for (m = 0; m < 14u; m++)
        for (f = 0; f < 3u; f++) {
            int pf[2];
            uint64_t h = 0;
            if (pipe(pf))
                return 1;
            if (!fork()) {
                h = render(SLOT[m], FAD[f]);
                if (write(pf[1], &h, 8) != 8)
                    _exit(1);
                _exit(0);
            }
            if (read(pf[0], &h, 8) != 8)
                h = 0;
            wait(0);
            close(pf[0]); close(pf[1]);
            got[m * 3 + f] = h;
            snprintf(name[m], sizeof name[m], "M%02u", SLOT[m] + 1u);
        }
    if (upd) {
        g = fopen(argv[1], "w");
        if (!g)
            return perror(argv[1]), 1;
        fprintf(g, "# tests/multi_render_test.c: FNV-1a 64 of each factory multi's output (3-note chords on both parts, fader 0 / 63 / 127).\n"
                   "# Written on 0.2.16's sounds (114): the 14 new factory sounds of 0.2.17 must not change it.\n");
        for (m = 0; m < 14u; m++)
            for (f = 0; f < 3u; f++)
                fprintf(g, "%s f%u %016llx\n", name[m], FAD[f], (unsigned long long)got[m * 3 + f]);
        fclose(g);
        printf("multi_render: wrote %s\n", argv[1]);
        return 0;
    }
    g = fopen(argv[1], "r");
    if (!g)
        return perror(argv[1]), 1;
    while (fgets(line, sizeof line, g)) {
        char nm[8]; unsigned fd; unsigned long long hv;
        if (line[0] == '#' || sscanf(line, "%7s f%u %llx", nm, &fd, &hv) != 3)
            continue;
        for (m = 0; m < 14u; m++)
            for (f = 0; f < 3u; f++)
                if (!strcmp(nm, name[m]) && fd == FAD[f]) {
                    int ok = got[m * 3 + f] == hv;
                    printf("multi_render: %s fader %3u %016llx %s\n", nm, fd, (unsigned long long)got[m * 3 + f], ok ? "ok" : "CHANGED");
                    bad += !ok; n++;
                }
    }
    fclose(g);
    printf("multi_render: %u renders compared, %u changed\n", n, bad);
    return bad || n != 42u;
}
