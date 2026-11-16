/* SPDX-License-Identifier: GPL-3.0-only */
/* HANDOFF_V2 item 109: the first-start SETUP questions (ui_guide.c): K1..K4 and SELECT each step the option, no HOME message on a turn.
 * DUELL 1.3 (owner 2026-11-15): SETUP 1/5 asks the device (KEYS, CIRCUIT TRACKS, MPC/FORCE: the MIDI PRESET), SETUP 2/5 the mode with the device's mode
 * recommended and ticked; the legend in fixed places (OCT- BACK left, HOME middle, OCT+ right; a key with no job on that page has no word: no BACK on
 * 1/5, no HOME on the last page); the GUIDE pages (info) and the copy in SETTINGS keep their behaviour. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static int bad;
static void expect(const char *w, int ok) { printf("guide_setup: %-100s %s\n", w, ok ? "ok" : "FAIL"); bad += !ok; }
static void open_at(uint32_t pg, uint32_t menu) { ui.menu = 0; guide_open(menu); ui.guide = (uint8_t)pg; ui.msg_t = 0; ui.msg[0] = 0; frames(100); }
static int legend(const char *a0, const char *a1, const char *a2)   /* the three places' words (0: that key has no word there) */
{
    const char *w[3] = {a0, a1, a2};
    uint32_t i;
    if (guide_kh[0].key != KC_OCTDN || guide_kh[1].key != KC_HOME || guide_kh[2].key != KC_OCTUP)
        return 0;
    for (i = 0; i < 3u; i++)
        if ((w[i] == 0) != (guide_kh[i].act == 0) || (w[i] && strcmp(w[i], guide_kh[i].act)))
            return 0;
    return 1;
}
int main(void)
{
    uint32_t k;
    const uint32_t K[5] = {EN_K1, EN_K2, EN_K3, EN_K4, EN_SELECT};   /* (owner 2026-11-16: every knob and SELECT choose; the knobs never turn pages) */
    ui_power_on();
    open_at(1, 0);
    for (k = 0; k < 5; k++) {
        guide_pick[0] = 1;
        turn(K[k], 1);  expect("SETUP 1/5: right steps CIRCUIT TRACKS -> MPC/FORCE", guide_pick[0] == 2u && ui.guide == 1u);
        turn(K[k], 1);  expect("  .. stops at the last option", guide_pick[0] == 2u);
        turn(K[k], -1); turn(K[k], -1); turn(K[k], -1); expect("  .. left steps to KEYS and stops", guide_pick[0] == 0u && ui.guide == 1u);
    }
    turn(EN_PRESET, 1); turn(EN_ALGO, 1);
    expect("SETUP 1/5: no HOME message from any knob", ui.msg_t == 0u);
    /* the device sets the MIDI PRESET and ticks its mode on 2/5 */
    {
        static const uint32_t PRE[3] = {MPRE_STANDARD, MPRE_CIRCUIT, MPRE_MPC}, REC[3] = {0u, 2u, 1u};   /* KEYS LAYER, CIRCUIT TRACKS DUEL, MPC/FORCE STEREO */
        for (k = 0; k < 3u; k++) {
            open_at(1, 0);
            guide_pick[0] = (uint8_t)k;
            press(B_OCTUP);
            frames(8);
            expect("SETUP 1/5 -> 2/5: the device's MIDI PRESET is set, its mode ticked and RECOMMENDED", ui.guide == 2u && mac_preset_get() == PRE[k] && guide_pick[1] == REC[k] && guide_rec_mode() == REC[k]);
        }
    }
    open_at(2, 0);
    for (k = 0; k < 5; k++) {
        guide_pick[1] = 0; turn(K[k], 1); expect("SETUP 2/5: right steps LAYER -> STEREO", guide_pick[1] == 1u && ui.guide == 2u);
        turn(K[k], -1); expect("  .. left back to LAYER", guide_pick[1] == 0u);
    }
    turn(EN_PRESET, 1); turn(EN_ALGO, 1);
    expect("SETUP 2/5: no HOME message from any knob", ui.msg_t == 0u);
    open_at(2, 0); guide_pick[1] = 2; press(B_OCTUP); frames(8);
    expect("SETUP 2/5 -> 3/5: DUEL is the mode asked for", ui.guide == 3u && duell_mode_req == DM_SPLIT);
    open_at(3, 0); turn(EN_K2, 1);
    expect("GUIDE 3/5 (info page): K2 changes no pick, says HOME closes the guide", ui.msg_t && str_eq(ui.msg, GUIDE_KNOB_MSG) && ui.guide == 3u);
    open_at(2, 0); guide_pick[1] = 1; turn(EN_K1, 1); frames(8);
    expect("KNOB 1 right on SETUP 2/5: the next option, the same page (the knobs never turn pages)", ui.guide == 2u && guide_pick[1] == 2u);
    turn(EN_K1, -1); turn(EN_K1, -1); frames(8);
    expect("KNOB 1 left: the options back, the same page", ui.guide == 2u && guide_pick[1] == 0u);
    open_at(4, 0); turn(EN_K1, 1); frames(8);
    expect("GUIDE 4/5 (info page): KNOB 1 turns no page, says HOME closes the guide", ui.guide == 4u && ui.msg_t && str_eq(ui.msg, GUIDE_KNOB_MSG));
    open_at(3, 1); guide_pick[0] = 1; guide_pick[1] = 1; turn(EN_K2, 1);
    expect("SETTINGS > SYSTEM > GUIDE copy (no questions): K2 picks nothing", guide_pick[0] == 1u && guide_pick[1] == 1u);
    open_at(1, 0); ui.force = 1; guide_draw();
    expect("legend SETUP 1/5: HOME SKIP, OCT+ NEXT, no BACK", legend(0, "SKIP", "NEXT"));
    open_at(2, 0); ui.force = 1; guide_draw();
    expect("legend SETUP 2/5: OCT- BACK, HOME SKIP, OCT+ NEXT", legend("BACK", "SKIP", "NEXT"));
    open_at(GUIDE_N, 0); ui.force = 1; guide_draw();
    expect("legend GUIDE 5/5 (the last page): OCT- BACK, OCT+ FINISH, no HOME", legend("BACK", 0, "FINISH"));
    open_at(3, 1); ui.force = 1; guide_draw();
    expect("legend SETTINGS' copy 1/3: HOME CLOSE, OCT+ NEXT, no BACK", legend(0, "CLOSE", "NEXT"));
    open_at(GUIDE_N, 1); ui.force = 1; guide_draw();
    expect("legend SETTINGS' copy 3/3: OCT- BACK, OCT+ FINISH, no HOME", legend("BACK", 0, "FINISH"));
    printf("guide_setup: %s\n", bad ? "FAILED" : "ALL OK");
    return bad != 0;
}
