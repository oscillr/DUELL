/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.11: the first-start GUIDE (ui_guide.c; SPEC "First-start guide"), against simulated NOR flash:
 *   FLAG       a fresh unit (no valid settings) shows it after the splash, which stays GUIDE_SPLASH_MS longer; settings with the bit set
 *              do not (the splash as before); an older settings record (the bit clear: every build before 0.2.11) shows it once;
 *              leaving it sets the bit and writes the settings once (one erase), so the next start has no guide; the MENU's GUIDE
 *              row opens the same pages and never touches the bit (clear stays clear, set stays set); powered off with it up: shown again
 *   KEYS       SELECT right / OCT+ next, SELECT left / OCT- back, both stopping at the ends; OCT+ on the last page finishes; HOME and
 *              every page button (EDIT FX ENV LFO ARP SCL GLO SEQ SAVE) leave at once, the button then doing its job; from the MENU
 *              HOME closes the menu too, OCT+ at the end goes back to the menu
 *   PLAY       the keys sound under it, PLAY/STOP plays and stops (twice: STOP ALL), REC is the HOLD switch; KNOB 1-4, PRESETS and
 *              ALGORITHM change nothing; the octave does not move
 *   STUCK      a random soak of buttons, knobs and keys with the guide up: the page always in 1..GUIDE_N, and HOME always leaves
 * Built and run by DUELL/host_tests.sh (name: guide): the UI sources against ui_test.c's stub display and knobs. */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"

static uint8_t nor[0x100000];
static uint32_t erases;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { erases++; memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if ((off & 0xFFu) + n > 256u)
        return -1;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    printf("guide: %-110s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
    return ok;
}
/* (0.2.11, with the settings write waiting for silence, project.c autosave_quiet: this harness runs no audio, so a voice the
 * keys started never ends its release by itself; the release tails are over once the audio has run, as on the unit) */
static void tails_over(void)
{
    uint32_t k, i;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NVOICE; i++)
            trk[k].v[i].active = 0;
}
/* main.c fm1_main's start: the flash read, the settings, the splash (the RAM gone) */
static void boot(void)
{
    ui_power_on();
    tails_over();
    memset(&settings, 0, sizeof settings);
    first_boot_pending = 0;
    memset(&persist_saved, 0, sizeof persist_saved);
    persist_pending = 0;
    flash_ok = 1;
    persist_boot();
    ps_dirty = 0;                                       /* (the pattern slots' defaults of a blank flash are written at the first quiet pass: not what these erase counts are about) */
    settings_init();
    if (autosave_boot(1))
        first_boot_pending = 0;
    splash_begin();
}
static int stored_seen(void)                            /* the bit as the flash holds it */
{
    static persist_t p;
    return st_load(OBJ_SETTINGS, &p, sizeof p) > 0 && ((p.favorites.factory[15][24] >> 5) & 1u);
}
static int page_is(uint32_t n) { char tb[24]; uint32_t pi; head_page_title(tb, &pi); return ui.guide == n && tb[0] == 'G' && tb[6] == (char)('0' + n); }

int main(void)
{
    static persist_t pb;
    uint32_t i, e0, k, n;
    setvbuf(stdout, NULL, _IONBF, 0);

    /* the pages fit: every line within GD_W (M; a heading L, its glyphs all in the sparse L face), every page above the key hints */
    {
        uint32_t p, ok = 1, glyphs = 1, maxw = 0, maxh = 0;
        for (p = 0; p < GUIDE_N; p++) {
            const char *s = GUIDE_TXT[p];
            int32_t h = 0;
            while (*s) {
                char ln[32];
                uint32_t n = 0;
                int head = *s == '#', gap = *s == '~', key = *s == '*';
                int32_t w;
                s += head || gap || key;
                while (*s && *s != '\n' && n + 1u < sizeof ln)
                    ln[n++] = *s++;
                ln[n] = 0;
                ok &= !*s || *s == '\n';               /* (no line cut by the buffer) */
                if (*s == '\n')
                    s++;
                if (gap) { h += GD_H_GAP; continue; }
                w = text_w(head ? &AF_L : &AF_M, ln);
                ok &= w <= GD_W && n > 0u;
                maxw = w > (int32_t)maxw ? (uint32_t)w : maxw;
                for (n = 0; head && ln[n]; n++)
                    glyphs &= glyph_at(&AF_L, (uint8_t)ln[n]) >= 0;
                h += head ? GD_H_HEAD : GD_H_LINE;
            }
            ok &= GD_TOP + h <= GD_HINT_Y;
            maxh = (uint32_t)h > maxh ? (uint32_t)h : maxh;
        }
        printf("guide: the widest line %u px of %d, the tallest page %u px of %d\n", maxw, GD_W, maxh, GD_HINT_Y - GD_TOP);
        expect("every page's lines fit (GD_W) and every page fits above the key hints", ok);
        expect("every heading's glyphs are in the L face", glyphs);
    }

    /* a fresh unit */
    memset(nor, 0xFF, sizeof nor);
    boot();
    expect("fresh unit: no valid settings, the bit clear", first_boot_pending == 1u && !guide_seen());
    frames(1300);
    expect("  the splash is still up after 1.3 s (SPLASH_MS 1.2 s + GUIDE_SPLASH_MS)", splash_on && !ui.guide);
    frames(500);
    expect("  .. over by 1.8 s and the guide is up on its first page (header GUIDE 1/6)", !splash_on && page_is(1u));
    expect("  .. the first start's M01 is loaded under it, its LOADED not over the guide's header", first_boot_pending == 0u && !ui.msg_t);
    frames(3000);
    expect("  it stays up by itself (no time-out) and has written nothing of its own yet", ui.guide == 1u && !stored_seen());

    /* the keys: next, back, the ends */
    turn(EN_SELECT, 1);
    expect("SELECT right: page 2", page_is(2u));
    press(B_OCTUP);
    expect("OCT+: page 3", page_is(3u));
    press(B_OCTDN);
    expect("OCT-: page 2", page_is(2u));
    turn(EN_SELECT, -1);
    expect("SELECT left: page 1", page_is(1u));
    turn(EN_SELECT, -3); press(B_OCTDN);
    expect("  .. and stays on page 1 at the start", page_is(1u));
    turn(EN_SELECT, 20);
    expect("SELECT right far: the last page (GUIDE_N), no further", page_is(GUIDE_N));
    turn(EN_SELECT, 1);
    expect("  .. SELECT right on the last page stays there", page_is(GUIDE_N));
    expect("the octave never moved (OCT- / OCT+ were the guide's)", (frames(200), song.octave == 0));

    /* playing under it */
    {
        static int16_t keep[2][P_COUNT];
        uint32_t pre = TSEL->preset, sel = song.sel, s0 = trk[0].eng_req, f0 = morph_fader_of(0), oct = (uint32_t)song.octave;
        memcpy(keep[0], trk[0].p, sizeof keep[0]);
        memcpy(keep[1], trk[1].p, sizeof keep[1]);
        turn(EN_K1, 5); turn(EN_K2, -4); turn(EN_K3, 3); turn(EN_K4, 2); turn(EN_PRESET, 3); turn(EN_ALGO, 1);
        expect("  .. the header says why (\"[HOME] CLOSES GUIDE\", no silently ignored knob)", ui.msg_t && str_eq(ui.msg, GUIDE_KNOB_MSG));
        frames(400);
        expect("KNOB 1-4, PRESETS, ALGORITHM: nothing changed (every parameter, the sound, the part, the morph fader, the page)",
               !memcmp(keep[0], trk[0].p, sizeof keep[0]) && !memcmp(keep[1], trk[1].p, sizeof keep[1]) && TSEL->preset == pre && song.sel == sel &&
               trk[0].eng_req == s0 && morph_fader_of(0) == f0 && (uint32_t)song.octave == oct && ui.guide == GUIDE_N);
        events_block(CTL);                              /* (the audio side takes the MODE: LAYER, as multi_test does) */
        key_down(7);
        for (k = 0, n = 0; n < 200u && !k; n++) {
            events_block(CTL);
            for (i = 0; i < NVOICE; i++)
                k |= trk[0].v[i].active || trk[1].v[i].active;
        }
        key_up(7);
        expect("a key sounds under it", k != 0u && ui.guide == GUIDE_N);
        frames(500);
        press(B_PLAY); frames(100);
        expect("PLAY/STOP plays", song.playing || transport_req == 1u || seq_counting());
        song.playing = 1; transport_req = 0;
        frames(500);
        press(B_PLAY); frames(50);
        expect("  .. and stops", transport_req == 2u);
        transport_req = 0;
        frames(500);
        btn_down(B_PLAY); frame(); btn_up(B_PLAY); frame(); btn_down(B_PLAY); frame(); btn_up(B_PLAY); frame();
        expect("  .. twice: STOP ALL (the header says it)", transport_req == 4u && ui.guide == GUIDE_N && ui.msg_t && str_eq(ui.msg, "STOP ALL"));
        transport_req = 0; ui.msg_t = 0;
        song.playing = 0;                               /* (no audio here to stop it: the settings write waits for silence, the transport stopped too) */
        tails_over();
        TSEL->p[P_AHOLD] = 0;
        press(B_REC);
        expect("REC: the HOLD switch (HOLD ON), still in the guide", TSEL->p[P_AHOLD] != 0 && ui.guide == GUIDE_N && str_eq(ui.msg, "HOLD ON"));
        press(B_REC);
        expect("  .. again: HOLD OFF", TSEL->p[P_AHOLD] == 0 && ui.guide == GUIDE_N);
        ui.msg_t = 0;
    }

    /* finishing: OCT+ on the last page sets the bit, one settings write */
    e0 = erases;
    press(B_OCTUP);
    frames(3000);
    settings_poll();                                    /* (the main loop's pass: since 0.2.11 the write is queued and made there once it is quiet) */
    expect("OCT+ on the last page: done, back to the page under it (HOME), the bit set", !ui.guide && at_home() && guide_seen());
    expect("  .. the settings written once (one erase), the flash holds the bit", erases - e0 == 1u && stored_seen());

    /* the next start: no guide, the splash as before */
    boot();
    expect("next start: the bit read back", guide_seen() && !first_boot_pending);
    frames(1300);
    expect("  the splash is over after SPLASH_MS (no longer one), no guide", !splash_on && !ui.guide);
    frames(2000);
    expect("  .. and none later", !ui.guide);

    /* MENU > SYSTEM > GUIDE: the same pages, the bit untouched */
    ui.menu = 1; ui.menu_sel = MI_GUIDE; ui.force = 1; frame();
    e0 = erases;
    turn(EN_K1, 1);
    expect("MENU > SYSTEM > GUIDE, KNOB 1 right: the guide on page 1, the menu under it", page_is(1u) && ui.menu == 1u);
    turn(EN_SELECT, 20);
    press(B_OCTUP);
    expect("  OCT+ at its end: back to the MENU at the GUIDE row", !ui.guide && ui.menu == 1u && ui.menu_sel == MI_GUIDE);
    press(B_OCTUP);
    expect("  OCT+ on the row opens it again", page_is(1u));
    press(B_HOME);
    frames(100);
    expect("  HOME: the guide and the menu closed, HOME's page (HOME OPENS: MORPH)", !ui.guide && !ui.menu && (at_home() || cur_fam() == FAM_TRK));
    frames(3000);
    settings_poll();
    expect("  the bit untouched (still set), nothing written", guide_seen() && erases == e0);

    /* an older settings record (0.2.10: bit 5 of [15][24] never written): shown once */
    memset(&pb, 0, sizeof pb);
    settings_export(&pb);
    pb.favorites.factory[15][24] = 0x10u;               /* (an older record: CPU METER OFF set, bit 5 clear) */
    pb.palette = palette_to_stored(3u);
    memset(nor, 0xFF, sizeof nor);
    expect("an older (0.2.10) settings record is stored", st_save(OBJ_SETTINGS, &pb, sizeof pb) == 0);
    boot();
    expect("  read as valid (not a new unit), theme kept, the bit clear", !first_boot_pending && settings.palette == 3u && !guide_seen() &&
           (ui_prefs2 & (PREF_CPU_OFF >> 16)));
    frames(1800);
    expect("  the guide is up after the splash", !splash_on && page_is(1u));
    {   /* the MENU's GUIDE does not touch a clear bit either: from the menu while the first-start guide is not up (opened by hand) */
        ui.guide = 0; ui.force = 1;
        ui.menu = 1; ui.menu_sel = MI_GUIDE; frame();
        e0 = erases;
        turn(EN_K1, 1);
        press(B_HOME);
        frames(2600);
        settings_poll();
        expect("  (MENU's GUIDE left with HOME while the bit is clear: still clear, nothing written)", !guide_seen() && erases == e0 && !ui.menu);
    }
    boot();                                             /* powered off with the bit still clear: shown again */
    frames(1800);
    expect("  a power-off before it was left: shown again", page_is(1u));
    turn(EN_SELECT, 2);
    e0 = erases;
    press(B_EDIT);
    expect("  EDIT leaves it at once and opens EDIT (its tap, as from the MENU)", !ui.guide && !at_home() && cur_page()->fam == FAM_EDIT);
    frames(3000);
    settings_poll();
    expect("  .. the bit set, written once, the other settings kept", guide_seen() && stored_seen() && erases - e0 == 1u && settings.palette == 3u &&
           (ui_prefs2 & (PREF_CPU_OFF >> 16)));

    /* every page button and HOME leave at once, from any page */
    {
        static const uint8_t LV[] = {B_EDIT, B_FX, B_ENV, B_LFO, B_ARP, B_SCL, B_GLO, B_SEQ, B_SAVE, B_HOME};
        uint32_t ok = 1;
        for (k = 0; k < sizeof LV; k++) {
            uint32_t p;
            for (p = 1; p <= GUIDE_N; p++) {
                ui_power_on();
                guide_open(0);
                ui.guide = (uint8_t)p;
                frame();
                press(LV[k]);
                frames(100);
                ok &= !ui.guide && guide_seen() && !ui.menu && !ui.confirm && !ui.perf;
                ok &= LV[k] == B_HOME ? at_home() != 0 : !at_home() && FAM_BTN[cur_fam()] == LV[k];
            }
        }
        expect("EDIT FX ENV LFO ARP SCL GLO SEQ SAVE HOME on every page: the guide gone, the bit set, the button's own page", ok);
    }
    {   /* held: a page button leaves at the press, then its hold is its own (EDIT: QUICK FILTER); SAVE held: SAVE OVER's dialog */
        ui_power_on();
        guide_open(0);
        hold(B_SAVE);
        expect("SAVE held: the guide gone at the press, then SAVE OVER's dialog as on any page", !ui.guide && ui.confirm == CF_SAVE_OVER);
        ui.confirm = 0;
        ui_power_on();
        guide_open(1);
        ui.menu = 1;
        press(B_GLO);
        expect("from the MENU: GLO leaves the guide and the menu, GLO's page", !ui.guide && !ui.menu && cur_page()->fam == FAM_GLO);
    }

    /* no stuck state: random input with the guide up; HOME always leaves */
    {
        uint32_t seed = 12345u, ok = 1, left = 0;
        static const uint8_t SAFE[] = {B_OCTUP, B_OCTDN, B_PLAY, B_REC};
        for (n = 0; n < 400u; n++) {
            ui_power_on();
            guide_open(n & 1u);
            if (n & 1u) ui.menu = 1;
            for (i = 0; i < 30u; i++) {
                seed = seed * 1103515245u + 12345u;
                switch ((seed >> 16) % 5u) {
                case 0: press(SAFE[(seed >> 8) % sizeof SAFE]); break;
                case 1: turn((uint32_t)((seed >> 9) % NE), (int32_t)((seed >> 4) % 7u) - 3); break;
                case 2: tap_key((seed >> 7) % 27u); break;
                case 3: frames(16u * ((seed >> 5) % 20u)); break;
                default: frame(); break;
                }
                ok &= ui.guide <= GUIDE_N && !ui.confirm && !name_on() && !ui.layer;
                if (!ui.guide) { left++; break; }   /* (OCT+ on the last page finished it) */
            }
            if (ui.guide) {
                press(B_HOME);
                ok &= !ui.guide && !ui.menu;
            }
            transport_req = 0;
        }
        expect("400 random runs (OCT, PLAY, REC, every knob, keys, waits): the page always in range, no dialog, layer or NAME; HOME leaves", ok && left > 0u);
    }
    printf("guide: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
