/* SPDX-License-Identifier: GPL-3.0-only */
/* HANG (SPLIT, STEREO, LAYER; LAYER also steps ALGORITHM mid-sequence): a sound load (PRESETS turn) while notes play must never freeze the audio or the main loop. For several engines on
 * both parts: load a preset, hold a chord, render, step presets with the chord held, keep rendering + running the UI under
 * an alarm watchdog.  Built and run by DUELL/host_tests.sh (name: hang). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <signal.h>
#include <unistd.h>
static volatile uint32_t where_;
static void wd(int s) { (void)s; printf("hang: WATCHDOG at mode %u engine %u step %u\n", (unsigned)(where_ >> 16), (unsigned)((where_ >> 8) & 255u), (unsigned)(where_ & 255u)); abort(); }
int main(void)
{
    static const uint8_t ENGS[] = {7, 0, 1, 3, 5};
    int32_t o[2u * CTL];
    uint32_t m, a, part, s, b, k, bad = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGALRM, wd);
    for (m = 0; m < 3u; m++)
    for (a = 0; a < sizeof ENGS; a++)
        for (part = 0; part < 2u; part++) {
            alarm(20);
            ui_power_on();
            frame();
            duell_mode_req = (uint8_t)(m == 0 ? DM_SPLIT : m == 1 ? DM_STEREO : DM_LAYER);
            duell_mode_force(duell_mode_req);
            song.sel = (uint8_t)part;
            select_engine(ENGS[a]);
            apply_preset(ENGS[a] == 7 ? 4 : 0);
            song.sel = 0;                                /* (part 1 as well: it is the stepped part or the partner) */
            for (s = 0; s < 12u; s++) {
                where_ = (m << 16) | (ENGS[a] << 8) | s;
                if (m == 2u && s == 6u)
                    turn(EN_ALGO, 1);                    /* the selected part changes mid-sequence */
                if (m == 2u && s == 9u)
                    turn(EN_ALGO, -1);
                for (k = 0; k < 4u; k++) {
                    trk_note_on(&trk[0], 48u + 4u * k, 110);
                    if (m != 0u || part)
                        trk_note_on(&trk[1], 48u + 4u * k, 110);   /* (LAYER: the same chord on part 2) */
                }
                for (b = 0; b < 8u; b++)
                    mix_block(o, CTL);
                turn(EN_PRESET, 1);
                for (b = 0; b < 120u; b++) {
                    mix_block(o, CTL);
                    if (b % 4u == 0u)
                        frame();
                }
            }
            alarm(0);
        }
    printf("hang: %d engines x 2 parts x SPLIT/STEREO/LAYER, 12 preset steps each with a chord held: no hang  ok\n", (int)sizeof ENGS);
    return bad;
}
