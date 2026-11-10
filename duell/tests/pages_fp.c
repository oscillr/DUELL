/* SPDX-License-Identifier: GPL-3.0-only */
/* Cloud job H, the UI render fingerprints: every page of every family on every engine, and the QUICK layers, drawn on the host's stub display, each in four parts (header, cards, graph panel, footer) hashed apart.
 * It uses only what the base (cloud/base3-wave3) has too, so the same file is built against the base and against the work (host_tests.sh pages): one line a screen, `ENGINE|FAMILY:PAGE|head cards graph foot`.
 * tests/pages_fp_diff.py lists what differs: by design only the footer's page counter on the EDIT and ENV pages (the round has pages more) and the new pages. Never regenerates anything: it only prints. */
#define UI_TEST_NO_MAIN 1
#include <stdlib.h>
#include "ui_test.c"

static uint32_t region(uint32_t y0, uint32_t y1)
{
    uint32_t h = 2166136261u, y, x;
    for (y = y0; y < y1; y++)
        for (x = 0; x < 240u; x++)
            h = (h ^ host_screen[y * 240u + x]) * 16777619u;
    return h;
}
static void line(const char *eng, const char *key)
{
    printf("%s|%s|%08x %08x %08x %08x\n", eng, key, region(0, Y_LABEL), region(Y_LABEL, Y_GRAPH), region(Y_GRAPH, Y_FOOT), region(Y_FOOT, 240));
}
static const char *const FAMN[] = {"HOME", "ENV", "LFO", "FX", "SCL", "EDIT", "GLO", "SAVE", "ARP", "SEQ", "TRK", "?"};   /* (params.c FAM_*) */
int main(void)
{
    uint32_t e, i, l;
    static const struct { uint8_t b; const char *n; } LY[] = {{B_GLO, "QUICK GLO"}, {B_SCL, "QUICK SCL"}, {B_EDIT, "QUICK MULTI"}, {B_LFO, "QUICK LFO"}, {B_ENV, "QUICK ENV"}, {B_ARP, "QUICK ARP"}};
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e))
            continue;
        for (i = 0; i < NPAGES; i++) {
            char key[64];
            ui_power_on();
            song.sel = 0;
            set_engine_of(TSEL, e);
            go_main_to(HP_MORPH);
            frames(64);
            if (!page_visible(i))
                continue;
            ui.page = (uint8_t)i;
            page_entered();
            frames(160);
            snprintf(key, sizeof key, "%s:%s", FAMN[PAGES[i].fam % 12u], PAGES[i].title);
            line(ENGINES[e]->name, key);
        }
        for (l = 0; l < NELEM(LY); l++) {
            ui_power_on();
            song.sel = 0;
            set_engine_of(TSEL, e);
            go_main_to(HP_MORPH);
            frames(64);
            btn_down(LY[l].b);
            frames(480);
            line(ENGINES[e]->name, LY[l].n);
            btn_up(LY[l].b);
            frames(64);
        }
    }
    return 0;
}
