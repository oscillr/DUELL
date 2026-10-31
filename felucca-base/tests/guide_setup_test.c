/* SPDX-License-Identifier: GPL-3.0-only */
/* HANDOFF_V2 item 109: the first-start SETUP questions (ui_guide.c): K1..K4 and SELECT each step the option, no HOME message on a turn,
 * the key legend is only OCT NEXT and HOME SKIP; the GUIDE pages (info) and the MENU copy keep their behaviour. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static int bad;
static void expect(const char *w, int ok) { printf("guide_setup: %-100s %s\n", w, ok ? "ok" : "FAIL"); bad += !ok; }
static void open_at(uint32_t pg, uint32_t menu) { ui.menu = 0; guide_open(menu); ui.guide = (uint8_t)pg; ui.msg_t = 0; ui.msg[0] = 0; frames(100); }
int main(void)
{
    uint32_t k;
    const uint32_t K[5] = {EN_K1, EN_K2, EN_K3, EN_K4, EN_SELECT};
    ui_power_on();
    open_at(1, 0);
    for (k = 0; k < 5; k++) {
        guide_pick[0] = 1;
        turn(K[k], 1);  expect("SETUP 1/5: right steps STEREO -> DUEL", guide_pick[0] == 2u && ui.guide == 1u);
        turn(K[k], 1);  expect("  .. stops at the last option", guide_pick[0] == 2u);
        turn(K[k], -1); turn(K[k], -1); turn(K[k], -1); expect("  .. left steps to LAYER and stops", guide_pick[0] == 0u && ui.guide == 1u);
    }
    turn(EN_PRESET, 1); turn(EN_ALGO, 1);
    expect("SETUP 1/5: no HOME message from any knob", ui.msg_t == 0u);
    open_at(5, 0);
    for (k = 0; k < 5; k++) {
        guide_pick[1] = 0; turn(K[k], 1); expect("SETUP 5/5: right picks NOW", guide_pick[1] == 1u && ui.guide == 5u);
        turn(K[k], -1); expect("  .. left picks LATER", guide_pick[1] == 0u);
    }
    turn(EN_PRESET, 1); turn(EN_ALGO, 1);
    expect("SETUP 5/5: no HOME message from any knob", ui.msg_t == 0u);
    open_at(3, 0); turn(EN_K1, 1);
    expect("GUIDE 3/5 (info page): K1 changes no pick, says HOME ends the guide", ui.msg_t && str_eq(ui.msg, GUIDE_KNOB_MSG));
    open_at(2, 1); guide_pick[0] = 1; guide_pick[1] = 1; turn(EN_K1, 1);
    expect("MENU > GUIDE copy (no questions): K1 picks nothing", guide_pick[0] == 1u && guide_pick[1] == 1u);
    open_at(1, 0); ui.force = 1; guide_draw();
    expect("legend SETUP 1/5: OCT+ NEXT, HOME SKIP only", guide_kh[0].key == KC_OCTUP && !strcmp(guide_kh[0].act, "NEXT") && guide_kh[1].key == KC_HOME && !strcmp(guide_kh[1].act, "SKIP"));
    open_at(5, 0); ui.force = 1; guide_draw();
    expect("legend SETUP 5/5: OCT+ FINISH, HOME SKIP only", guide_kh[0].key == KC_OCTUP && !strcmp(guide_kh[0].act, "FINISH") && guide_kh[1].key == KC_HOME && !strcmp(guide_kh[1].act, "SKIP"));
    printf("guide_setup: %s\n", bad ? "FAILED" : "ALL OK");
    return bad != 0;
}
