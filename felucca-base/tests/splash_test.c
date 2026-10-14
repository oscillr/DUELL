/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL: the power-on splash, the VIOLET default, the version name and the CPU load meter's window maths.
 *   VIOLET     a fresh unit's theme is VIOLET (UI_DEFAULT_INDEX = 0, the first palette)
 *   VERSION    the shown version is "DUELL <VERSION> beta" (the VERSION file -> build/gen/duell_version.h)
 *   SPLASH     begun at once, it lasts SPLASH_MS (1.2 s) and then the UI is drawn again; any button, key or knob ends it and is
 *              used up by that (the page does not change); a MIDI note does not end it and sounds; ANIM OFF: one still frame
 *   METER      cpu_meter_poll: the 1 s average and the held peak from the audio ISR's counters, the hold falling after
 *              CPU_HOLD_MS, a miss red for CPU_RECENT_MS, the levels at 80 % and 95 %
 * Built and run by DUELL/host_tests.sh (name: splash): the UI sources against ui_test.c's stub display and knobs. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    if (!ok) {
        printf("splash: FAIL %s\n", what);
        bad++;
    }
    return ok;
}
static int near(uint32_t v, uint32_t w) { return v + 1u >= w && v <= w + 1u; }   /* (the integer us of the windows round down) */
static void window(uint32_t halves, uint32_t pct, uint32_t peak_pct, uint32_t miss)   /* one second of the ISR's counters */
{
    cpu_n += halves;
    cpu_sum += halves * (CPU_BUDGET_US * pct / 100u);
    cpu_pk = CPU_BUDGET_US * peak_pct / 100u;
    cpu_miss += miss;
    fm1_ms += CPU_WIN_MS;
    cpu_meter_poll();
}
int main(void)
{
    uint32_t i, p0, page0;
    setvbuf(stdout, NULL, _IONBF, 0);
    expect("the default theme is VIOLET", UI_DEFAULT_INDEX == 0u && !strcmp(UI_PALETTES[UI_DEFAULT_INDEX].name, "VIOLET"));
    expect("a fresh unit (settings blank) starts in VIOLET", (settings.magic = 0, settings_init(), settings.palette == UI_DEFAULT_INDEX));
    expect("the version is DUELL " DUELL_VERSION " beta (the VERSION file)", !strcmp(FELUCCA_VERSION, "DUELL " DUELL_VERSION " beta") && !strcmp(FELUCCA_VERSION, DUELL_VERSION_STR));

    ui_power_on();
    ui_prefs2 |= (uint8_t)(PREF_GUIDE_SEEN >> 16);     /* (DUELL 0.2.11: the GUIDE seen, so the splash is the plain one; the first start's: guide_test.c) */
    frames(200);
    page0 = ui.page;
    splash_begin();
    frames(500);
    expect("the splash is up after 0.5 s", splash_on);
    expect("MIDI-side notes do not end it and sound", (trk_note_on(&trk[0], 60, 100), frames(300), splash_on && trk[0].v[0].active));
    frames(450);
    expect("it is over after SPLASH_MS", !splash_on && ui.force | 1);
    for (i = 0; i < 5u; i++) frame();
    splash_begin();
    frames(100);
    p0 = ui.page;
    press(B_SAVE);
    expect("a button ends it and is used up (no page, no menu, no dialog)", !splash_on && ui.page == p0 && !ui.menu && !ui.confirm && !name_on());
    splash_begin();
    frames(100);
    turn(EN_K1, 1);
    expect("a knob ends it", !splash_on);
    splash_begin();
    frames(100);
    tap_key(5);
    expect("a key ends it", !splash_on);
    ui_prefs |= PREF_ANIM_OFF;
    splash_begin();
    frames(500);
    expect("ANIM OFF: still the first frame (step 0), still up", splash_on && splash_fr == 0u);
    ui_prefs &= ~PREF_ANIM_OFF;
    frames(1500);
    expect("  .. and over after SPLASH_MS", !splash_on);
    (void)page0;

    /* the meter */
    cpu_n = cpu_sum = cpu_pk = cpu_miss = 0;
    cpu_meter_open();
    window(344, 50, 70, 0);
    expect("meter: 50 % average, 70 % peak after one window", near(cpum.avg, 50) && near(cpum.peak, 70) && near(cpum.hold, 70) && cpu_level() == 0);
    window(344, 60, 40, 0);
    expect("meter: the peak is held (70 %) while the window's own peak is lower", near(cpum.avg, 60) && near(cpum.peak, 40) && near(cpum.hold, 70));
    for (i = 0; i < 4u; i++) window(344, 60, 40, 0);
    expect("meter: the hold falls to the latest after CPU_HOLD_MS", near(cpum.hold, 40));
    window(344, 82, 90, 0);
    expect("meter: 80 % and up is level 1 (NEAR), no miss", near(cpum.avg, 82) && cpu_level() == 1 && !cpu_recent());
    window(344, 96, 99, 0);
    expect("meter: 95 % and up is level 2", cpu_level() == 2);
    window(344, 30, 40, 2);
    expect("meter: a miss makes level 2 (and the word) at once, whatever the average", cpum.miss == 2 && cpu_recent() && cpu_level() == 2);
    for (i = 0; i < 5u; i++) window(344, 30, 40, 0);
    expect("meter: and clears after CPU_RECENT_MS; the total stays", !cpu_recent() && cpum.miss == 2 && cpu_level() == 0);
    printf("splash: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
