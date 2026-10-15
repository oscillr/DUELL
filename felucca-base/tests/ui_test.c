/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the UI (firmware/src/ui*.c with the sound sources of hostsim.c;
 * the display and the HAL are stubs, the buttons and knobs are driven through them):
 *   SOUNDS     a sound load (PRESETS knob / page, TOOLS INIT, user preset) changes the sound only: the
 *              steps, LEN DIV SWING GATE, ARP, SCL and the mix stay the track's. Power-on:
 *              every sequencer empty.
 *   PATTERNS   SAVE > PHRASES: KNOB 1 PAT (factory patterns, then user presets that hold one), OCT+
 *              LOAD; over the user's steps the REPLACE? dialog (OCT- / OCT+); the PRESETS hint.
 *   UNDO       a load keeps a copy of the track; SAVE held swaps back what the loads changed (again:
 *              redo); browsing keeps the copy from before the first load; power-on and projects: none.
 *   REC        a tap on every screen arms the selected track and starts PLAY, without navigation.
 *              Long REC never clears. On STEP, armed and playing: keys do not write the
 *              cursor step. With the ARP on, the arp's notes are recorded, not the keys.
 *   MIDI IN    MENU > MIDI > MIDI IN (ROUT): CH1-4 (channels 5..16 ignored) / SEL, note-offs follow their note-ons.
 *   SAVE       refused while playing ("STOP TO SAVE"); a used slot asks OVERWRITE? (OCT+ / OCT-).
 *   ACTIONS    PATTERNS, USER, PROJECT, TOOLS: the knobs pick, OCT+ does it, OCT- cancels or goes
 *              HOME, on release, never both together; the OCT LEDs (OCT- lit, OCT+ blinks).
 *   TRACKS     the PRESETS knob does nothing there; KNOB 1 is MUTE.
 *   GRID       SEQ > STEP on a DRUM track: white keys toggle the selected lane's steps of the page, black
 *              keys 1..8 select the lane (and only they play), 9 held = ACC, 10 / 11 the page; KNOB 1..4 STEP
 *              LANE HIT ACC; the key LEDs (hits, the playhead inverted, the lane, ACC, the page keys); a step's
 *              GM notes shown on their lanes and made the lane's own by an edit; live recording into the grid
 *              (lane keys on the grid, GM keys elsewhere; other GM drums as notes; no TIE holds); 12 BEAT from
 *              PATTERNS fills the grid (undo); sound loads keep it, and other engines play it as GM notes.
 *   ROLL       the rolling digits of the card values and the header BPM: strips only, the last frame static,
 *              direction, retarget, the snaps (fast turn, names, shape, page, track, palette, force).
 *   SOAK       #63: 150000 random frames (turns, pages, transport, REC / motion, wrapping clocks): no roll
 *              outlives ROLL_FRAMES, the cards always equal a forced redraw once the rolls are over.
 *   FM6 OPS    (1.2, #76) EDIT > OPERATOR, OP ENV, OPERATOR 2 on an FM6 track: KNOB 1 the operator, each of the 84 bytes
 *              the pages edit written alone, SLOT OWN, the sound following, one undo per gesture, the macros on top,
 *              projects and user presets keep the patch; FM6 only, not automatable.
 *   USB SERIAL #67: MENU > USB SERIAL ON (default, the descriptors as before) / OFF (audio + MIDI only, class 0):
 *              the menu, the settings, the descriptor at boot (built with the console, as the firmware: FELUCCA_CDC).
 *   SCREEN OFF (1.1.5): MENU > DISPLAY > SCREEN OFF against a simulated clock and the panel's commands (lcd_power):
 *              dark after the time, nothing drawn, any input wakes it (SLPOUT, 120 ms, redraw, DISPON) and is
 *              swallowed; dialogs / count-in / UPDATE MODE keep it on; saved; the sound bit for bit while dark.
 * Built and run by tests/run_tests.sh. */
#ifndef FELUCCA_CDC
#define FELUCCA_CDC 1                                    /* (as src/felucca.c: the console built in, MENU > USB SERIAL) */
#endif
#include <stddef.h>
#include <stdint.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

/* ------------------------------------------------------------ HAL stubs --- */
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL];
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL], fm1_led_breath[FM1_NCOL], fm1_led_mid[FM1_NCOL];
static uint8_t fm1_led_scl[FM1_NCOL], fm1_led_root[FM1_NCOL];   /* (0.2.9 rev. 2: the keyboard's sets at their levels, */
static uint8_t fm1_led_kout[FM1_NCOL], fm1_led_ktop[FM1_NCOL];   /* hal/fm1_input.h fm1_led_key_levels) */
#define FM1_LED_KL_FULL 240u
static uint32_t host_kl[5];                       /* .. the levels the UI asked for: out, scale, root, played peak, played trough */
static uint32_t host_pu_ms = 99u, host_pu_ph;
static void fm1_led_pulse(uint32_t ms, uint32_t ph) { host_pu_ms = ms; host_pu_ph = ph; }
static void fm1_led_key_levels(uint32_t o, uint32_t s, uint32_t r, uint32_t t, uint32_t l) { host_kl[0] = o; host_kl[1] = s; host_kl[2] = r; host_kl[3] = t; host_kl[4] = l; }
static uint32_t host_dim_lo;                      /* hal/fm1_input.h: the glow MENU > LEDS asked for (1: LOW) */
static void fm1_led_dim_level(uint32_t lo) { host_dim_lo = lo; }
static int host_anim;                             /* hal/fm1_input.h: the power-on sweep running (it has the LEDs) */
static int host_idle, host_idle_glow;             /* .. the idle animation (1.2: MENU > ANIM IDLE) running, its glow */
static int fm1_led_anim_on(void) { return host_anim || host_idle; }
static void fm1_led_idle_start(uint32_t glow) { host_idle = 1; host_idle_glow = (int)glow; }
static int fm1_led_idle_on(void) { return host_idle; }
static void fm1_led_anim_stop(void) { host_anim = host_idle = 0; }
#define FM1_TICKS_PER_US 1u
static uint32_t host_ticks, host_pressed, host_notes;
static int32_t host_enc[7];
static uint32_t fm1_ticks(void) { return host_ticks; }
static uint32_t fm1_input_edges(int x) { uint32_t p = host_pressed; (void)x; host_pressed = 0; return p; }
static uint32_t fm1_input_note_edges(void) { uint32_t n = host_notes; host_notes = 0; return n; }
/* host_enc_late: detents the TIMER5 scan counts just after a read (#102: while ui_input runs); the next read has them */
static int32_t host_enc_late[7];
static int32_t fm1_enc_take(uint32_t e)
{
    int32_t s = host_enc[e % 7u];
    host_enc[e % 7u] = host_enc_late[e % 7u];
    host_enc_late[e % 7u] = 0;
    return s;
}
static void (*host_wdt_hook)(void);                /* (a test's: time passing in a blocking loop, as panel_setup's) */
static void fm1_wdt_feed(void) { if (host_wdt_hook) host_wdt_hook(); }
static void (*host_irq_hook)(int on);              /* (tests/loadcost_test.c: times the interrupts-off windows of the main loop) */
static void fm1_irq_off(void) { if (host_irq_hook) host_irq_hook(0); }
static void fm1_irq_on(void) { if (host_irq_hook) host_irq_hook(1); }
static uint16_t host_screen[240 * 240];
/* #181 a blank frame: while host_fl_on, every write of a frame is watched (host_fl_begin .. host_fl_end); a pixel that
 * shows, between two writes, a value that is neither the one before the frame nor the one after (a fill of the
 * background before a canvas lands over it) flashed */
static uint8_t host_fl_on, host_fl_n[240 * 240];
static uint16_t host_fl_was[240 * 240], host_fl_mid[240 * 240];
static void host_put(uint32_t k, uint16_t v)
{
    if (host_fl_on) {
        if (!host_fl_n[k])
            host_fl_was[k] = host_screen[k];
        else if (host_screen[k] != host_fl_was[k] && host_fl_n[k] < 2u)
            host_fl_mid[k] = host_screen[k], host_fl_n[k] = 2;   /* (2: an in-between value kept) */
        if (!host_fl_n[k])
            host_fl_n[k] = 1;
    }
    host_screen[k] = v;
}
static void host_fl_begin(void) { memset(host_fl_n, 0, sizeof host_fl_n); host_fl_on = 1; }
static uint32_t host_fl_end(uint32_t y0, uint32_t y1)   /* the rows y0..y1-1: pixels that flashed */
{
    uint32_t k, n = 0;
    host_fl_on = 0;
    for (k = y0 * 240u; k < y1 * 240u; k++)
        n += host_fl_n[k] == 2u && host_fl_mid[k] != host_screen[k];
    return n;
}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
#ifdef UI_FILL_HOOK
    UI_FILL_HOOK(x, y, w, h);                     /* (tests/ui_render.c: what a fill covers is gone) */
#endif
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            host_put((y + j) * 240u + x + i, (uint16_t)((c >> 8) | (c << 8)));
}
static void lcd_sync(void) {}
/* MENU > SCREEN OFF (lcd.c lcd_power / lcd_wake_now): what the panel was told, in order (0 off: DISPOFF, SLPIN;
 * 1 SLPOUT; 2 DISPON; 3 lcd_wake_now), with the time; host_bl the panel showing (the backlight itself stays on: PA2 is
 * the key matrix's /OE too, lcd.c lcd_power) */
static uint32_t host_pw_n, host_pw[256], host_pw_ms[256];
static int host_bl = 1;
static void host_pw_log(uint32_t s)
{
    if (host_pw_n < 256u) { host_pw[host_pw_n] = s; host_pw_ms[host_pw_n] = fm1_ms; }
    host_pw_n++;
    host_bl = s != 0u;
}
static void lcd_power(uint32_t s) { host_pw_log(s); }
static void lcd_wake_now(void) { host_pw_log(3u); }
/* (the HOME oscilloscope's buffer, scope_buf: fx.c, with hostsim.c) */
static uint32_t host_blit_rows;                    /* rows blitted (what a frame draws) */
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    host_blit_rows += h;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++) host_put((y + j) * 240u + x + i, p[j * w + i]);
}
static struct { uint32_t stage, erase_max_us; } felucca_dbg;   /* (audio.c's; erase_max_us: the TEST screen, 0.2.11) */
#ifndef FELUCCA_FLASH
#define FELUCCA_FLASH 0
#endif
#if FELUCCA_FLASH                                        /* (a test of the stores on simulated NOR: tests/patslot_test.c;
                                                          * it gives the flash hooks below) */
static uint32_t irq_save(void);
static void irq_restore(uint32_t f);
static uint32_t fl_jedec_ram(void);
static void fl_plain_window_init(void);
#define FL_FAR(fn) (fn)
static uint8_t flash_ok;                                 /* (storage_hw.c's) */
#endif
#include "duell_version.h"
#ifndef FELUCCA_VERSION
#define FELUCCA_VERSION DUELL_VERSION_STR
#endif
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
static int at_home(void) { return cur_page()->fam == FAM_TRK; }   /* HOME's round (MORPH, MORPH SET, MODE, MIXER); the OVERVIEW screen (ui.home) is gone */
#include "../firmware/src/menu_items.c"
#include "../firmware/src/icons.c"
#include "../firmware/src/ui_graph.c"
#include "../firmware/src/ui_draw.c"
#include "../firmware/src/ui_menu.c"
#include "../firmware/src/ui_input.c"
#include "../firmware/src/ui_layer.c"
#if FELUCCA_FLASH
#include "../firmware/src/storage.c"
#endif
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"

static uint8_t ui_old_uslot;                             /* (the retired SAVE > USER page's slot: the stale tests below still set it) */
/* the ids Felucca's layout gave to DIGITAL's operators, DRUM's lanes and P_ED_FX (core.h PL_*), and the SLICER's: no EDIT page shows them */
static int ui_id_retired(uint32_t id)
{
    return id == PL_ED_FX || (id >= PL_FM1_ATK && id <= PL_FM4_LEVEL) || (id >= PL_LN0 && id <= PL_LN7) || (id >= P_FREEA && id <= P_FREED);
}
static int check(const char *what, int ok)
{
    printf("ui: %-74s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* main.c felucca_init */
static void ui_power_on(void)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(&ui, 0, sizeof ui);
    page_over = 0;                                /* (no layer's knobs; roll_settle's test page gone) */
    cin_left = cin_total = 0;                     /* (no count-in) */
    memset(&nm, 0, sizeof nm);                    /* NAME closed, no project name */
    proj_name[0] = 0;
    proj_cur = PROJ_NO_SLOT;
    memset(&favorites, 0, sizeof favorites);
    ml_off();                                     /* (MIDI LEARN off, nothing learned) */
    memset(&undo, 0, sizeof undo);
    memset(mph, 0, sizeof mph);                   /* DUELL 4b: the MORPH as at power-on (.bss): nothing rolled, */
    memset(&morph, 0, sizeof morph);              /* SELECTED, no MIDI action queued, the CC triggers armed */
    mq_r = mq_w = mq_lost = 0;
    memset(midi_morph_fired, 0, sizeof midi_morph_fired);
    duell_mode_force(DM_SPLIT);                   /* DUELL step 5: the MODE of a power-on (the settings byte 0: SPLIT) */
    scale_g = 0; root_g = 0; offscale_g = OS_SNAP; scale_last = 0xFFFFFFFFu;   /* the global scale: the settings bytes are 0 (favorites cleared above): CHROMATIC, C, SNAP */
    fop_op = fop_stg = 0;                         /* (FM6's operator pages: OP 1, stage 1, as at power-on) */
    memset(pat_last, 0, sizeof pat_last);
    memset(pat_from, 0, sizeof pat_from);                /* (the header's slot letter: none) */
    multi_ram_reset();                                   /* DUELL: the multis (project.c multi.c) */
    up_ram_reset();                                      /* (DUELL: no patch stored; the PATCH store, upreset.c) */
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    undo_depth++;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        set_engine_of(t, TRK_DEF[i][0]);
        apply_preset_to(t, TRK_DEF[i][1]);
        t->engine = t->eng_req;
        track_defaults_steps(t);
        if (TRK_DEF[i][2])
            load_pat16(t, PATTERNS[TRK_DEF[i][2] - 1u].note, PATTERNS[TRK_DEF[i][2] - 1u].flags);
        pat_sig[i] = steps_sig(t);
        pat_last[i] = TRK_DEF[i][2];
    }
    undo_depth--;
    song.sel = 0;
    go_main_to(HP_MORPH);
    ui.force = 1;
    panel = PANEL_DEFAULT;
    settings.palette = UI_GREY_INDEX;
    palette_set(UI_GREY_INDEX);
    transport_req = 0;
    panic_req = 0;
    mi_r = mi_w = midi_in_overflow = 0;
    memset(midi_sel_on, 0, sizeof midi_sel_on);
    memset(midi_ch, 0, sizeof midi_ch);
    memset(midi_owners, 0, sizeof midi_owners);
    memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target);
    memset(kb_chn, 0, sizeof kb_chn);                   /* chord.c: no key, MIDI chord or last chord */
    memset(mchord, 0, sizeof mchord);
    memset(chord_last, 0, sizeof chord_last);
    kb_layer = 0;                                       /* no layer key held */
    kb_perf = perf_held = perf_act = 0;                 /* DUELL 0.2.5 PERFORM: off, no key, the knobs neutral (.bss at power-on) */
    mods_off = 0;                                       /* DUELL 0.2.14 GLO > MODULES: the audio side's copy as at power-on (.bss; the settings say ON) */
    perf_keys = perf_fx_hit = perf_kill = 0;
    perf_fxb = 0;
    perf_roll_n[0] = perf_roll_n[1] = 0;
    perf_space = perf_filt = 0;
    perf_depth = 100;
    kb_mask = layer_bits();                             /* (ui_input sets it every frame) */
    memset(&lys, 0, sizeof lys);
    settings_hold = HOLD_DEF;
    fm1_in.buttons = fm1_in.notes = 0;
    memset(&motion, 0, sizeof motion);                  /* (main.c: no motion at power-on) */
    memset(motion_active, 0, sizeof motion_active);
    memset(motion_locked, 0, sizeof motion_locked);
    motion_base_valid = motion_full = 0;
    memset(snd_said, 0, sizeof snd_said);               /* (.bss at power-on: nothing said yet) */
    memset(&scrn, 0, sizeof scrn);                        /* MENU > SCREEN OFF: on, idle from now (main.c) */
    scrn.idle = fm1_ms;
    kb_asleep = 0;
    panel_moved = 0;
}
/* card slot drawn with MOTION's mark (ui_draw.c card_mot: its icon, or a long label in the theme colour) */
static int card_motion(uint32_t slot) { return (card_mot >> slot) & 1u; }

/* one UI frame (input, then the drawing on the stub display) */
static void frame(void)
{
    host_ticks += 16000u;
    fm1_ms += 16u;
    ui_input();
    while (wave_service())                        /* main.c: the WAVE table builds (its wait loop: done within a frame here) */
        ;
    vcap_poll();                                  /* main.c: the VOICE CAP (DUELL 0.2.11, voice.c) */
    ui_draw();
}
static void press(uint32_t label)                 /* a tap: down, a frame, up, a frame */
{
    fm1_in.buttons |= 1u << panel.btn[label];
    host_pressed |= 1u << panel.btn[label];
    frame();
    fm1_in.buttons &= ~(1u << panel.btn[label]);
    frame();
}
static void frames(uint32_t ms) { uint32_t i; for (i = 0; i < ms / 16u; i++) frame(); }
static void btn_down(uint32_t label) { fm1_in.buttons |= 1u << panel.btn[label]; host_pressed |= 1u << panel.btn[label]; }
static void btn_up(uint32_t label) { fm1_in.buttons &= ~(1u << panel.btn[label]); }
static void hold(uint32_t label)                  /* held 0.8 s, then let go */
{
    uint32_t k;
    fm1_in.buttons |= 1u << panel.btn[label];
    host_pressed |= 1u << panel.btn[label];
    for (k = 0; k < 52u; k++)
        frame();
    fm1_in.buttons &= ~(1u << panel.btn[label]);
    frame();
}
static void turn(uint32_t role, int32_t s)
{
    host_enc[panel.enc[role]] += s * panel.dir[role];
    frame();
    host_ticks += 200000u;                        /* (no knob acceleration between the turns) */
}
static void bpm_nudge(int32_t n)                  /* the tempo moved n (SELECT never does; GLO > CLOCK KNOB 1, MIDI clock, TAP) */
{
    song.g[G_BPM] = (int16_t)(song.g[G_BPM] + n);
    frame();
    host_ticks += 200000u;
}
static void stop_transport(void) { transport_req = 0; song.playing = 0; }
static int stored_multi(uint32_t slot, project_t *p)   /* DUELL: multi slot decoded (project.c multi.c) */
{
    uint8_t blk[NSHOWN][MB_N], amt[NSHOWN], set[3];
    uint32_t len;
    const uint8_t *r;
    mu_obj_read(slot / 2u);
    r = mu_rec(slot & 1u, &len);
    memset(p, 0, sizeof *p);
    return len && mu_unpack(p, blk, amt, set, r, len);
}
static int16_t stored_param(uint32_t slot, uint32_t track, uint32_t id)
{
    static project_t p;
    return stored_multi(slot, &p) ? p.t[track].p[id] : -32768;
}
static uint8_t stored_note(uint32_t slot, uint32_t track, uint32_t step)
{
    static project_t p;
    return stored_multi(slot, &p) ? p.t[track].step[step].note[0] : 0;
}
static int msg_is(const char *s) { return ui.msg_t && str_eq(ui.msg, s); }
static void my_steps(track_t *t)                  /* "recorded" steps */
{
    uint32_t i;
    track_defaults_steps(t);
    for (i = 0; i < 32u; i += 3u) {
        t->step[i].note[0] = (uint8_t)(40 + i);
        t->step[i].n = 1;
        t->step[i].time = ST_NOTE;
        t->step[i].vel = 99;
    }
    t->p[P_SLEN] = 32;
}

static void go_page(uint32_t graph)               /* the page that draws graph */
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].graph == graph)
            break;
    ui.page = (uint8_t)i;
    page_entered();
}
/* SEQ > AUTOMATION (1.2): its first row (PLAY, CLEAR); + ADD with id (a parameter, EV_CHANCE, EV_RATCH) on step */
static void go_auto_top(void)
{
    go_page(GR_EVENTS);
    ui.ev_row = 0;
    ui.ev_keep = 0;
}
static void go_auto_add(uint32_t id, uint32_t step)
{
    go_page(GR_EVENTS);
    ui.ev_row = 0xFFFFu;                          /* (ev_row: the last, + ADD) */
    ui.ev_keep = 0;
    ui.ev_id = (uint8_t)id;
    ui.ev_step = (uint8_t)step;
}
/* the row showing c (EVC), selected; 0: not listed */
static int auto_pick(uint32_t c)
{
    uint16_t rw[EV_ROWS];
    uint32_t n = ev_list(rw), r;
    for (r = 0; r < n && rw[r] != c; r++)
        ;
    if (r == n)
        return 0;
    ui.ev_row = (uint16_t)r;
    return 1;
}
static int steps_are(const track_t *t, uint32_t n)  /* steps 1..16 = PATTERNS[n], 17..64 empty, LEN 16 */
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++) {
        uint8_t want = i < 16u ? PATTERNS[n].note[i] : 0u;
        if (t->step[i].note[0] != want || t->step[i].n != (want ? 1u : 0u))
            return 0;
    }
    return t->p[P_SLEN] == 16;
}
/* the sound of a track: engine, preset and the parameters that are not the track's own (param_kept) */
static int same_sound(const track_t *a, const track_t *b)
{
    uint32_t i;
    for (i = 0; i < P_COUNT; i++)
        if (!param_kept(i) && a->p[i] != b->p[i])
            return 0;
    return a->eng_req == b->eng_req && a->preset == b->preset;
}

/* sounds never touch the steps; the arp, the scale and the pattern parameters are the track's */
static int test_sound_loads(void)
{
    int bad = 0;
    uint32_t i, empty = 1;
    track_t *t = &trk[0], before;
    ui_power_on();
    for (i = 0; i < NTRK; i++)
        empty &= (uint32_t)seq_is_empty(&trk[i]);
    bad += check("power-on: the four sounds, every sequencer empty, no undo copy", empty && undo.trk == 0 && undo_depth == 0 &&
                 trk[3].eng_req == TRK_DEF[3][0] && trk[3].preset == 0u && trk[0].preset == TRK_DEF[0][1]);
    my_steps(t);
    t->p[P_E0 + 1] = 77;                          /* a sound edit */
    t->p[P_AMODE] = 2;                            /* and the track's own settings */
    t->p[P_AOCT] = 3;
    t->p[P_SCALE] = 2;
    t->p[P_TRANS] = 5;
    t->p[P_SDIV] = 1;
    t->p[P_LEVEL] = 90;
    before = *t;
    turn(EN_PRESET, 1);                           /* HOME: the next preset */
    bad += check("PRESETS on HOME loads the sound: steps, LEN, DIV untouched", t->preset == (uint8_t)((before.preset + 1u) % ENGINES[0]->npresets) &&
                 !memcmp(t->step, before.step, sizeof t->step) && t->p[P_SLEN] == 32 && t->p[P_SDIV] == 1);
    bad += check("..ARP, SCL and the mix stay the track's", t->p[P_AMODE] == 2 && t->p[P_AOCT] == 3 && t->p[P_SCALE] == 2 &&
                 t->p[P_TRANS] == 5 && t->p[P_LEVEL] == 90);
    bad += check("..and no warning about the sequence", !msg_is("T1 SEQ REPLACED"));
    for (i = 0; i < ENGINES[0]->npresets; i++)
        if (str_eq(ENGINES[0]->presets[i].name, "RAVE"))
            break;
    t->p[P_AMODE] = 0;
    t->preset = (uint8_t)(i - 1u);
    turn(EN_PRESET, 1);
    bad += check("RAVE (an ARP preset) leaves the arp off; it suggests pattern 13 ARP", t->preset == i && t->p[P_AMODE] == 0 &&
                 preset_pat_hint() == 12 && ui.ppick == 12 && str_eq(PATTERNS[12].name, "ARP"));
    t->p[P_AMODE] = 2;
    before = *t;
    for (i = 0; i < 6u; i++)                      /* several loads, into the next engine */
        turn(EN_PRESET, 1);
    bad += check("browsing on, into another engine: the steps still untouched", t->eng_req != before.eng_req &&
                 !memcmp(t->step, before.step, sizeof t->step) && t->p[P_AMODE] == 2);
    bad += check("browsing keeps the copy from before the first load", undo.trk == 1u && undo.what == UNDO_SOUND &&
                 undo.p[P_E0 + 1] == before.p[P_E0 + 1] && undo.eng == before.eng_req && undo.preset == before.preset);
    t->step[0].note[0] = 99;                      /* recorded after the loads */
    hold(B_SAVE);
    bad += check("SAVE held: the sound back (UNDO/REDO T1); steps recorded since stay", same_sound(t, &before) &&
                 t->step[0].note[0] == 99 && msg_is("UNDO/REDO T1"));
    bad += check("SAVE held does not open the SAVE pages", at_home());
    hold(B_SAVE);
    bad += check("SAVE held again: the loads again (redo)", t->eng_req != before.eng_req && t->step[0].note[0] == 99);
    hold(B_SAVE);
    t->p[P_LEVEL] = 50;                           /* a mix change after the undo */
    turn(EN_PRESET, 1);                           /* a load after an undo copies the track as it is now */
    hold(B_SAVE);
    bad += check("a load after the undo: its own copy; the mix (LEVEL) is never swapped", same_sound(t, &before) && t->p[P_LEVEL] == 50);
    press(B_SAVE);
    bad += check("SAVE tap opens the SAVE pages (on release)", !at_home() && cur_page()->fam == FAM_SAVE);
    /* TOOLS INIT, user preset load: the sound only, undoable; one copy for all tracks */
    ui_power_on();
    my_steps(&trk[1]);
    trk[1].p[P_SCALE] = 3;                        /* (a track setting: kept) */
    before = trk[1];
    track_select(1);
    set_engine(trk[1].eng_req);                   /* TOOLS INIT */
    bad += check("TOOLS INIT: the sound only (steps and SCL kept), a copy of T2", undo.trk == 2u &&
                 !memcmp(trk[1].step, before.step, sizeof before.step) && trk[1].p[P_SCALE] == 3);
    track_select(0);
    my_steps(&trk[0]);
    trk[0].p[P_SDIV] = 2;
    up_store(3, "MINE");
    bad += check("a user preset keeps its pattern in the record (format unchanged)", up_has_pat(3) && up_rec(3)->note[0] == 40);
    trk[0].p[P_SDIV] = 0;
    track_defaults_steps(&trk[0]);
    trk[0].step[5].note[0] = 33;
    trk[0].step[5].n = 1;
    before = trk[0];
    up_load(3);
    bad += check("a user preset load: the sound only (steps, DIV kept); the copy is now T1's", trk[0].user == 4u && undo.trk == 1u &&
                 !memcmp(trk[0].step, before.step, sizeof before.step) && trk[0].p[P_SDIV] == 0);
    undo_swap();
    bad += check("undo of a user preset load", trk[0].user == 0 && !memcmp(trk[0].step, before.step, sizeof before.step));
    /* project load: no copy, and the old one is gone */
    project_save(1);
    up_load(3);
    project_load(1);
    bad += check("a project load drops the copy and takes none", undo.trk == 0 && undo_depth == 0);
    ui_power_on();
    {   /* the retired engines (core.h ENGI_RETIRED): DIGITAL 1, SAMPLE 4, GRAIN 8, DRUM 10, SLICE 13 */
        static const uint8_t GONE5[5] = {1, 4, 8, 10, 13};
        uint32_t k, ok = 1;
        for (k = 0; k < 5u; k++)
            ok &= ENGI_RETIRED(GONE5[k]) && !ENGINES[GONE5[k]]->npresets && !eng_ok(GONE5[k]) && str_eq(ENGINES[GONE5[k]]->name, "-") &&
                  !ds_count(GONE5[k]);
        for (k = 0; k < NENGINES; k++)
            ok &= (uint32_t)ENGI_RETIRED(k) == (k == 1u || k == 4u || k == 8u || k == 10u || k == 13u);
        bad += check("DIGITAL, SAMPLE, GRAIN, DRUM, SLICE (retired): no factory presets, never offered, their number is the silent placeholder", ok);
        set_engine_of(t, 4);                          /* a pick of a retired engine (the editor's ENG, INIT on such a part): ANALOG */
        bad += check("picking SAMPLE (retired): ANALOG, its first sound", t->eng_req == 0u && t->preset == 0u);
        set_engine_of(t, 10);
        bad += check("picking DRUM (retired in 0.2.14, hidden before): ANALOG, its first sound", t->eng_req == 0u && t->preset == 0u);
    }
    ui_power_on();
    my_steps(&trk[3]);
    trk[3].p[P_SDIV] = 3;
    before = trk[3];
    project_save(1);
    {   /* DUELL 0.2.14: the old formats are no longer read. An old project (0.2.13's FUN11: revision 0; Felucca's FUN10: another magic)
         * is refused as it is restored (the editor's restore of an old archive, the autosave at power-on) and changes nothing */
        project_t cur, back;
        project_store_t st;
        uint32_t sum, magic = 0x46554E41u;                /* "FUNA" (Felucca's FUN10) */
        int16_t tempo = song.g[G_BPM];
        project_capture(&cur);
        bad += check("a project written now packs and imports (revision 1)", proj_pack(&st, &cur) && st.raw[67] == 1u &&
                     proj_import(&back, &st, sizeof st));
        st.raw[67] = 0;                                   /* 0.2.13's FUN11: byte 67 was 0 */
        sum = proj_hash(st.raw, sizeof st - 4u); memcpy(st.raw + sizeof st - 4u, &sum, 4);
        bad += check("old projects: a FUN11 of before 0.2.14 (revision 0, valid hash) is refused", !proj_import(&back, &st, sizeof st));
        st.raw[67] = 1; memcpy(st.raw, &magic, 4);
        sum = proj_hash(st.raw, sizeof st - 4u); memcpy(st.raw + sizeof st - 4u, &sum, 4);
        bad += check("old projects: Felucca's FUN10 (its magic, valid hash) is refused; the music is untouched",
                     !proj_import(&back, &st, sizeof st) && song.g[G_BPM] == tempo);
    }
    printf("ui: undo copy %u bytes\n", (unsigned)sizeof undo);
    return bad;
}

/* SAVE > PHRASES: K1 PAT, OCT+ LOAD; the dialog over the user's steps; undo */
static int test_patterns(void)
{
    int bad = 0;
    track_t *t = &trk[0], before;
    ui_power_on();
    go_page(GR_PATS);
    bad += check("1.2: PHRASES is a SAVE page (SAVE > PHRASES)", cur_page()->fam == FAM_SAVE &&
                 str_eq(cur_page()->title, "PHRASES") && !song.seq_mode);
    before = *t;
    turn(EN_K2, 1);
    bad += check("KNOB 2 loads nothing (the knobs only pick)", seq_is_empty(t) && ui.confirm == CF_NONE);
    press(B_OCTUP);                               /* LOAD 01 ACID into the empty sequencer */
    bad += check("LOAD into an empty sequencer: at once, LOADED 01 ACID", ui.confirm == CF_NONE && steps_are(t, 0) &&
                 msg_is("LOADED 01 ACID"));
    bad += check("..the sound and the track's settings untouched", same_sound(t, &before) && t->p[P_SDIV] == before.p[P_SDIV] &&
                 t->p[P_AMODE] == before.p[P_AMODE]);
    turn(EN_K1, 2);
    press(B_OCTUP);
    bad += check("a loaded pattern, untouched: the next one without asking (03 MELODY)", ui.confirm == CF_NONE && steps_are(t, 2) &&
                 msg_is("LOADED 03 MELODY"));
    hold(B_SAVE);
    bad += check("SAVE held: back past both loads (empty again), the sound untouched", seq_is_empty(t) && same_sound(t, &before) &&
                 t->p[P_SLEN] == before.p[P_SLEN]);
    hold(B_SAVE);                                 /* redo */
    t->step[3].note[0] = 70;                      /* an edit: the user's steps now */
    t->step[3].n = 1;
    t->step[3].time = ST_NOTE;
    press(B_OCTUP);
    bad += check("edited steps: the REPLACE T1 SEQUENCE? dialog", ui.confirm == CF_LOAD_PAT && ui.confirm_trk == 0u &&
                 t->step[3].note[0] == 70);
    press(B_OCTDN);
    bad += check("OCT- keeps them", ui.confirm == CF_NONE && t->step[3].note[0] == 70);
    my_steps(t);
    before = *t;
    turn(EN_K1, 9);                               /* 12 BEAT */
    press(B_OCTUP);
    press(B_OCTUP);
    bad += check("OCT+ loads (12 BEAT), then HOLD SAVE: UNDO", ui.confirm == CF_NONE && steps_are(t, 11) &&
                 msg_is("LOADED 12 BEAT") && str_eq(ui.msg2, "[SAVE] HOLD TO UNDO"));
    hold(B_SAVE);
    bad += check("SAVE held: the user's steps and LEN back", !memcmp(t->step, before.step, sizeof t->step) && t->p[P_SLEN] == 32);
    /* a user preset's pattern: listed after the factory ones as U06, with its LEN DIV SWING GATE */
    t->p[P_SDIV] = 2;
    t->p[P_SSWING] = 40;
    up_store(5, "UPAT");
    t->p[P_SDIV] = 0;
    t->p[P_SSWING] = 0;
    up_store(6, "NOPAT");
    memset(up_rec(6)->note, 0, sizeof up_rec(6)->note);   /* .. a record with no pattern: not listed */
    bad += check("user presets with a pattern are listed after the factory ones", pat_count() == NPATTERNS + 1u);
    track_defaults_steps(t);
    turn(EN_K1, 20);                              /* (to the end of the list) */
    press(B_OCTUP);
    bad += check("LOAD U06: its 16 steps, LEN 16 (at most), DIV and SWING", msg_is("LOADED U06 UPAT") && t->step[0].note[0] == 40 &&
                 t->step[3].note[0] == 43 && !t->step[30].n && t->p[P_SLEN] == 16 && t->p[P_SDIV] == 2 && t->p[P_SSWING] == 40);
    /* a project's steps count as the user's */
    project_save(2);
    project_load(2);
    press(B_OCTUP);
    bad += check("over a project's steps: the dialog", ui.confirm == CF_LOAD_PAT);
    press(B_OCTDN);
    /* the hint: PRESETS page K3 PAT, and a load moves the pick there */
    go_page(GR_BROWSE);
    turn(EN_K1, 1);
    bad += check("PRESETS: a factory preset's suggested pattern becomes the pick", preset_pat_hint() >= 0 &&
                 ui.ppick == (uint8_t)preset_pat_hint());
    return bad;
}

static int test_rec(void)
{
    static const uint8_t FAMS[3] = {FAM_TRK, FAM_SEQ, FAM_ARP};
    int bad = 0;
    uint32_t k;
    for (k = 0; k < 3u; k++) {
        ui_power_on();
        track_select(2);
        open_family(FAMS[k]);
        press(B_REC);
        bad += check(k == 0 ? "REC on TRACKS: arms the selected track, PLAY starts" :
                     k == 1 ? "#133 REC on SEQ > STEP: arms the selected track for step recording, PLAY does not start" :
                     "REC on ARP: arms the selected track, PLAY starts",
                     song.rec == 4u && (k == 1u ? transport_req == 0u && msg_is("KEYS WRITE STEPS") : transport_req == 1u));
        song.playing = 1;
        transport_req = 0;
        frames(400);                                   /* (not a double tap: that locks the REC layer) */
        press(B_REC);
        bad += check("REC again disarms (the transport runs on)", song.rec == 0u && transport_req == 0u);
        frames(400);
        hold(B_REC);                                   /* 1.1.5: held, the REC layer (ui_layer.c); the arming stays */
        bad += check("REC held: its layer, closed when let go; no dialog, the arming as it was",
                     ui.confirm == CF_NONE && song.rec == 0u && !ui.layer && !transport_req);
        ui.confirm = 0;
    }
    ui_power_on();
    press(B_REC);
    bad += check("REC on HOME records without changing the screen", at_home() && song.rec == 1u && transport_req == 1u);
    /* STEP page: armed + playing -> the keys record live only */
    ui_power_on();
    open_family(FAM_SEQ);
    bad += check("SEQ opens STEP", cur_page()->scope == SC_STEP);
    press(B_REC);
    song.playing = 1;
    transport_req = 0;
    fm1_in.notes = host_notes = 1u << 7;
    frame();
    fm1_in.notes = 0;
    frame();
    bad += check("STEP, armed and playing: a key does not write the cursor step", !trk[0].step[0].n && ui.cursor == 0);
    song.rec = 0;
    fm1_in.notes = host_notes = 1u << 7;
    frame();
    fm1_in.notes = 0;
    frame();
    bad += check("#133 STEP, not armed: the key only plays (nothing written, the cursor stays)",
                 !trk[0].step[0].n && ui.cursor == 0);
    song.rec = 1;
    song.playing = 0;
    fm1_in.notes = host_notes = 1u << 7;
    frame();
    fm1_in.notes = 0;
    frame();
    bad += check("#133 STEP, armed and stopped: the key writes the cursor step (and moves on)",
                 trk[0].step[0].n == 1 && ui.cursor == 1);
    /* ARP: the arp's notes are recorded, not the key */
    ui_power_on();
    {
        track_t *t = &trk[0];
        uint32_t i, n = 0, notes = 0;
        t->engine = t->eng_req;
        t->p[P_AMODE] = 1;
        t->p[P_AOCT] = 2;
        t->p[P_APROB] = 127;
        song.rec = 1;
        song.playing = 1;
        input_on(t, 60, 100);
        for (i = 0; i < NSTEP; i++)
            n += t->step[i].n;
        bad += check("ARP on: the key held is not recorded", n == 0);
        arp_tick(t, 1);
        for (i = 0; i < NSTEP; i++)
            if (t->step[i].n)
                notes = t->step[i].note[0];
        bad += check("ARP on: the note the arp plays is recorded", notes == 60u);
        t->p[P_AMODE] = 0;
        input_off(t, 60);
    }
    return bad;
}

static uint32_t gated_notes(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++) n += t->v[i].gate != 0;
    return n;
}

static void go_title(const char *title);
static int test_midi(void)
{
    int bad = 0;
    track_t *a, *b;
    ui_power_on();
    song.sel = 2;
    midi_hint = 0;
    midi_event(0x90, 0, 60, 100); midi_event(0x90, 9, 61, 100);
    bad += check("ROUT CH1-4 (default): channel 1 -> part 1, channel 10 ignored",
                 song.g[G_ROUTE] == 0 && midi_sel_on[0][60] == 1u && !midi_sel_on[9][61] && !gated_notes(&trk[2]));
    bad += check("a note into another track tells the UI", midi_hint == 1u);
    frame(); bad += check("MIDI IN -> T1 shown", msg_is("MIDI IN -> T1"));
    song.g[G_ROUTE] = 1;
    midi_event(0x80, 0, 60, 0); midi_event(0x90, 0, 62, 100);
    bad += check("ROUT changes: note-off releases original owner, new note uses selection",
                 !gated_notes(&trk[0]) && midi_sel_on[0][62] == 3u && midi_track(3) == &trk[2]);
    song.sel = 1; midi_event(0x80, 0, 62, 0);
    bad += check("note-off after selection follows the note-on's track", !midi_sel_on[0][62]);
    midi_event(0x80, 9, 61, 0);
    ui_power_on();
    song.g[G_ROUTE] = 1;
    midi_event(0x90, 0, 60, 100);
    song.sel = 1;
    midi_event(0x90, 0, 60, 100);
    bad += check("same MIDI channel/pitch on a new selected track releases its old owner",
                 !gated_notes(&trk[0]) && gated_notes(&trk[1]) && midi_sel_on[0][60] == 2u);
    midi_event(0x80, 0, 60, 0);
    midi_event(0x80, 0, 60, 0);
    bad += check("both note-offs after a MIDI ownership change leave neither track held",
                 !gated_notes(&trk[0]) && !gated_notes(&trk[1]) && !midi_sel_on[0][60]);
    ui_power_on();
    song.sel = 2; song.g[G_ROUTE] = 1;
    midi_event(0x90, 0, 61, 100);
    song.g[G_ROUTE] = 0;
    midi_event(0x90, 0, 61, 100);
    midi_event(0x80, 0, 61, 0);
    bad += check("same channel/pitch across a ROUT change cannot leave its previous part held",
                 !gated_notes(&trk[2]) && !gated_notes(&trk[0]) && !midi_sel_on[0][61]);
    {
        uint32_t hold;
        for (hold = 0; hold < 2u; hold++) {
            ui_power_on();
            song.g[G_ROUTE] = 1;
            trk[0].p[P_AMODE] = 1; trk[0].p[P_AHOLD] = (int16_t)hold;
            midi_event(0x90, 0, 60, 100);
            song.sel = 1;
            midi_event(0x90, 0, 60, 100);
            bad += check("MIDI ownership transfer releases old ARP keys and respects explicit HOLD",
                         !trk[0].arp_phys && trk[0].nheld == hold && midi_sel_on[0][60] == 2u);
        }
    }
    ui_power_on();
    trk[1].p[P_AMODE] = trk[1].p[P_AHOLD] = 1;
    midi_event(0x90, 0, 60, 100);
    midi_event(0x90, 1, 64, 100);
    song.playing = 1;
    {
        uint32_t i, held = 0;
        for (i = 0; i < MQ; i++) midi_in_event(0x643C9909u);
        midi_in_event(0x003C8908u);                      /* note-off lost to a full ring */
        events_block(CTL);
        for (i = 0; i < NTRK * NVOICE; i++) held += trk[i / NVOICE].v[i % NVOICE].gate;
        bad += check("MIDI overflow releases held notes and latched ARP, keeps transport running",
                     !held && !trk[1].nheld && !trk[1].arp_phys && !midi_sel_on[0][60] &&
                     !midi_sel_on[1][64] && !midi_in_overflow && mi_r == mi_w && song.playing);
        midi_in_event(0x643D9009u);
        events_block(CTL);
        bad += check("MIDI accepts a fresh note after overflow recovery", midi_sel_on[0][61] == 1u && trk[0].v[0].gate);
    }
    {   /* DUELL 0.1: MIDI IN lives on GLO > CHANNELS K3 (was MENU > MIDI > MIDI IN, up to 1.1.5 GLO > SYSTEM K3 ROUT): the project's
         * G_ROUTE, stored 0 / 1 as ever; right = SELECTED, left = OWN CH; no MENU row any more */
        int16_t r0 = song.g[G_ROUTE];
        uint32_t i;
        int ok = 1;
        ui.menu = 0;
        go_title("MIDI");
        song.g[G_ROUTE] = 0;
        ok &= cur_page()->graph == GR_MIDI && str_eq(GP[G_ROUTE].names[0], "OWN CH") && str_eq(GP[G_ROUTE].names[1], "SELECTED");
        turn(EN_K3, 1);
        ok &= song.g[G_ROUTE] == 1;
        turn(EN_K3, 5);
        ok &= song.g[G_ROUTE] == 1;
        turn(EN_K3, -1);
        ok &= song.g[G_ROUTE] == 0;
        turn(EN_K1, 2);                                   /* (P1 CH moves, MIDI IN stays) */
        ok &= song.g[G_ROUTE] == 0 && midi_ch_get(0) == 2u;
        turn(EN_K1, -2);
        for (i = 0; i < MI_COUNT; i++)                    /* no MENU row is a MIDI setting any more */
            ok &= !str_eq(MI_NAME[i], "MIDI IN") && !str_eq(MI_NAME[i], "LEARN CLEAR");
        for (i = 0; i < MTAB_COUNT; i++)
            ok &= !str_eq(MTAB_NAME[i], "MIDI");
        song.g[G_ROUTE] = r0;
        bad += check("GLO > CHANNELS K3: MIDI IN OWN CH / SELECTED (G_ROUTE 0 / 1); MENU holds no MIDI row or tab", ok);
    }
    return bad;
}

static int test_save(void)
{
    int bad = 0;
    uint32_t i;
    ui_power_on();
    trk[0].p[P_E0] = 5;
    press(B_SAVE);
    bad += check("SAVE opens USER with SAVE selected, without writing a slot",
                 !at_home() && cur_page()->graph == GR_USER && ui.act == 4u && !up_used(0));
    press(B_OCTUP);
    bad += check("SAVE then OCT+ opens NAME (a random name: no name to keep), writes nothing yet",
                 name_on() && nm.kind == NK_USER_SAVE && nm.len >= 1u && nm.len <= NM_LEN && strchr(nm.s, ' ') &&
                 nm.cur == nm.len && !up_used(0));
    {
        char nmb[16], was[16];
        str_cpy(was, nm.s, sizeof was);
        press(B_OCTUP);
        up_name(0, nmb);
        bad += check("..OCT+ again stores the edited sound with that name (SAVE, OCT+, OCT+)",
                     !name_on() && up_used(0) && up_value(up_rec(0), P_E0) == 5 && ui.act == 0u && str_eq(nmb, was));
    }
    press(B_SAVE);
    bad += check("1.5 (#173): SAVE again: PRESETS (the engines and sounds, one tap on)", cur_page()->graph == GR_BROWSE);
    press(B_SAVE);
    bad += check("1.2: SAVE again: PHRASES (the patterns, after the sounds)", cur_page()->graph == GR_PATS);
    press(B_SAVE);
    bad += check("SAVE again still cycles to PROJECT", cur_page()->graph == GR_SLOTS);
    go_main_to(HP_MORPH);
    press(B_SAVE);
    bad += check("SAVE from HOME returns directly to USER, even after visiting PROJECT",
                 cur_page()->graph == GR_USER && ui.act == 4u);
    press(B_OCTUP);
    bad += check("direct SAVE still asks before overwriting an occupied slot",
                 ui.confirm == CF_OVR_USER && up_value(up_rec(0), P_E0) == 5);
    press(B_OCTDN);
    ui_power_on();
    song.playing = 1;
    project_save(0);
    bad += check("project save while playing: refused, STOP TO SAVE", !project_used(0) && msg_is("STOP TO SAVE"));
    bad += check("user preset save while playing: refused", up_store(0, "X") == 2 && !up_used(0));
    song.playing = 0;
    transport_req = 1;
    project_save(0);
    bad += check("a pending PLAY cannot save a project", !project_used(0) && msg_is("STOP TO SAVE"));
    bad += check("a pending PLAY cannot save a user preset", up_store(0, "X") == 2 && !up_used(0));
    transport_req = 0;
    song.playing = 1;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].graph == GR_SLOTS)
            break;
    ui.page = (uint8_t)i;
    page_entered();
    ui.msg_t = 0;
    turn(EN_K4, 1);                               /* SAVE picked */
    bad += check("PROJECT KNOB 4 picks SAVE, saves nothing", ui.act == 4u && !project_used(0) && !ui.msg_t);
    press(B_OCTUP);
    bad += check("OCT+ while playing: STOP TO SAVE, no NAME", msg_is("STOP TO SAVE") && !project_used(0) &&
                 song.g[G_SAVE] == 0 && !name_on());
    stop_transport();
    turn(EN_K4, 1);
    press(B_OCTUP);
    bad += check("stopped, an empty slot: OCT+ opens NAME (no name yet: a random one)", name_on() && nm.kind == NK_PROJ_SAVE &&
                 nm.len >= 1u && nm.len <= NM_LEN && str_eq(nm.ph, "PROJECT A") && !project_used(0));
    press(B_OCTUP);
    bad += check("..OCT+ saves it, SAVE dropped", !name_on() && project_used(0) && msg_is("SAVED (RAM)") &&
                 ui.act == 0u);
    turn(EN_K4, 1);
    press(B_OCTUP);
    bad += check("a used slot: the OVERWRITE? dialog", ui.confirm == CF_OVR_PROJ && ui.confirm_trk == 0);
    trk[0].p[P_E0] = 5;
    press(B_OCTDN);
    bad += check("OCT- keeps the slot", ui.confirm == CF_NONE && stored_param(0, 0, P_E0) != 5);
    turn(EN_K4, 1);
    press(B_OCTUP);
    press(B_OCTUP);
    bad += check("OCT+ (YES): NAME, the slot not written yet", name_on() && stored_param(0, 0, P_E0) != 5);
    press(B_OCTUP);
    bad += check("OCT+ overwrites it", ui.confirm == CF_NONE && !name_on() && stored_param(0, 0, P_E0) == 5);
    trk[0].p[P_LEVEL] = 11;
    turn(EN_K3, 1);
    press(B_OCTUP);
    bad += check("KNOB 3 LOAD, OCT+: the project back; LOAD stays picked", trk[0].p[P_LEVEL] != 11 &&
                 trk[0].p[P_LEVEL] == stored_param(0, 0, P_LEVEL) && msg_is("LOADED") && ui.act == 3u);
    /* user presets */
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].graph == GR_USER)
            break;
    ui.page = (uint8_t)i;
    page_entered();
    up_store(4, "OLD");
    ui_old_uslot = 4;
    turn(EN_K4, 1);
    press(B_OCTUP);
    bad += check("USER SAVE over a used slot: the OVERWRITE? dialog", ui.confirm == CF_OVR_USER && ui.confirm_trk == 4u);
    press(B_OCTUP);
    bad += check("YES: NAME with the sound's name (not the old slot's)", name_on() && !str_eq(nm.s, "OLD"));
    press(B_OCTUP);
    {
        char nm[16];
        up_name(4, nm);
        bad += check("OCT+ stores the sound there", ui.confirm == CF_NONE && !str_eq(nm, "OLD"));
    }
    song.playing = 1;
    ui.msg_t = 0;
    turn(EN_K3, 1);                               /* ERASE */
    press(B_OCTUP);
    bad += check("USER ERASE while playing: STOP TO SAVE", msg_is("STOP TO SAVE") && up_used(4));
    stop_transport();
    return bad;
}

static void go_title(const char *title)
{
    uint32_t i;
    for (i = 0; i < NPAGES && !str_eq(PAGES[i].title, title); i++)
        ;
    if (i >= NPAGES) fprintf(stderr, "go_title: no page titled \"%s\"\n", title);   /* (a scene on a renamed page) */
    ui.page = (uint8_t)i;
    page_entered();
}
static uint32_t oct_leds_seen(int all)            /* over 1 s: the OCT LED bits lit at some point (all: throughout) */
{
    uint32_t k, any = 0, every = 3u | OCT_BREATH;
    for (k = 0; k < 64u; k++) {
        frame();
        any |= oct_leds();
        every &= oct_leds();
    }
    return all ? every : any;
}

/* the action pages: the knobs pick, OCT+ does it, OCT- cancels / goes HOME; the LEDs; chords */
static int test_actions(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    ui_power_on();
    press(B_OCTUP);
    bad += check("HOME: OCT+ shifts the octave (lit)", song.octave == 1 && oct_leds() == 2u);
    go_page(GR_PATS);
    bad += check("PATTERNS: OCT+ breathes (a pattern to load), OCT- lit", oct_leds_seen(0) == (1u | OCT_BREATH) &&
                 oct_leds_seen(1) == (1u | OCT_BREATH));
    press(B_OCTUP);
    bad += check("..OCT+ loads it, the octave stays", steps_are(t, 0) && song.octave == 1 && !at_home());
    bad += check("..the same pattern again would change nothing: OCT+ dark", oct_leds_seen(0) == 1u);
    turn(EN_K1, 1);
    bad += check("..another one picked: OCT+ breathes again", oct_leds_seen(0) == (1u | OCT_BREATH));
    fm1_in.buttons |= 1u << panel.btn[B_OCTUP];   /* both together (the UPDATE MODE hold): nothing */
    host_pressed |= 1u << panel.btn[B_OCTUP];
    frame();
    fm1_in.buttons |= 1u << panel.btn[B_OCTDN];
    host_pressed |= 1u << panel.btn[B_OCTDN];
    frame();
    fm1_in.buttons &= ~(1u << panel.btn[B_OCTUP]);
    frame();
    fm1_in.buttons &= ~(1u << panel.btn[B_OCTDN]);
    frame();
    bad += check("OCT- + OCT+ together: no load, no HOME, no octave change", steps_are(t, 0) && !at_home() && song.octave == 1);
    go_main_to(HP_MORPH);
    fm1_in.buttons |= 1u << panel.btn[B_OCTUP];   /* pressed on HOME, let go on PATTERNS: no load */
    host_pressed |= 1u << panel.btn[B_OCTUP];
    frame();
    go_page(GR_PATS);
    fm1_in.buttons &= ~(1u << panel.btn[B_OCTUP]);
    frame();
    bad += check("a press that began elsewhere does not load", steps_are(t, 0) && song.octave == 2);
    press(B_OCTDN);
    bad += check("PATTERNS: OCT- goes HOME (the octave stays)", at_home() && song.octave == 2);
    /* TOOLS: CLRSQ and INIT are picked, not done, by their knobs */
    my_steps(t);
    go_title("TOOLS");
    bad += check("TOOLS: nothing picked, OCT+ dark", act_cols() == 15u && act_col() == 0u && oct_leds_seen(0) == 1u);
    turn(EN_K1, 3);
    bad += check("KNOB 1 picks CLEAR, the steps stay; OCT+ breathes", ui.act == 1u && !seq_is_empty(t) &&
                 oct_leds_seen(0) == (1u | OCT_BREATH));
    turn(EN_K1, -1);
    bad += check("KNOB 1 left drops it", ui.act == 0u);
    turn(EN_K1, 1);
    press(B_OCTDN);
    bad += check("OCT- cancels the pick (still on TOOLS)", ui.act == 0u && !at_home() && !seq_is_empty(t));
    turn(EN_K1, 1);
    press(B_OCTUP);
    bad += check("TOOLS CLEAR first asks confirmation", ui.confirm == CF_CLEAR_SEQ && !seq_is_empty(t));
    press(B_OCTUP);
    bad += check("OCT+ clears the pattern, the pick dropped", seq_is_empty(t) && msg_is("PATTERN CLEARED") && ui.act == 0u);
    turn(EN_K1, 1);
    bad += check("nothing to clear: OCT+ dark", oct_leds_seen(0) == 1u);
    turn(EN_K2, 1);
    t->p[P_E0] = (int16_t)(t->p[P_E0] + 1);
    press(B_OCTUP);
    bad += check("TOOLS INIT first asks confirmation", ui.confirm == CF_INIT_SOUND);
    press(B_OCTUP);
    bad += check("KNOB 2 INIT, OCT+: SOUND INIT", msg_is("SOUND INIT") && ui.act == 0u);
    press(B_OCTDN);
    bad += check("OCT- with nothing picked: HOME", at_home());
    /* the dialogs and the menu */
    open_family(FAM_SEQ);
    my_steps(t);
    confirm_open(CF_CLEAR_SEQ, song.sel);
    frame();
    bad += check("a dialog: OCT+ breathes, OCT- lit", ui.confirm == CF_CLEAR_SEQ && oct_leds_seen(0) == (1u | OCT_BREATH) &&
                 oct_leds_seen(1) == (1u | OCT_BREATH));
    fm1_in.buttons |= 1u << panel.btn[B_OCTUP];
    host_pressed |= 1u << panel.btn[B_OCTUP];
    frame();
    bad += check("..it answers on release, not on the press", ui.confirm == CF_CLEAR_SEQ && !seq_is_empty(t));
    fm1_in.buttons &= ~(1u << panel.btn[B_OCTUP]);
    frame();
    bad += check("..OCT+ let go: cleared", ui.confirm == CF_NONE && seq_is_empty(t));
    bad += check("back on SEQ: the octave LEDs again", oct_leds() == 2u);
    return bad;
}

static int test_tracks(void)
{
    int bad = 0;
    track_t before;
    ui_power_on();
    ui_mode_set = DM_STEREO;                      /* (DUELL step 5: STEREO, PAN live; HOME's first page is MODE: the MIXER) */
    frame();
    go_page(GR_TRK);
    before = trk[0];
    turn(EN_PRESET, 1);
    bad += check("#94 MIXER: the PRESETS knob loads the next sound; the steps and the mix stay",
                 trk[0].preset != before.preset && !memcmp(trk[0].step, before.step, sizeof before.step) &&
                 trk[0].p[P_LEVEL] == before.p[P_LEVEL] && cur_page()->graph == GR_TRK);
    hold(B_SAVE);
    bad += check("#94 MIXER: SAVE held undoes it", trk[0].preset == before.preset && !memcmp(trk[0].p, before.p, sizeof before.p));
    turn(EN_K4, 1);
    bad += check("MIXER KNOB 4 right: MUTE ON (the selected track stays)", trk[0].p[P_MUTE] == 1 && song.sel == 0);
    turn(EN_K1, -3);
    bad += check("KNOB 1 changes LEVEL only (MUTE stays)", trk[0].p[P_MUTE] == 1 && trk[0].p[P_LEVEL] == before.p[P_LEVEL] - 3);
    turn(EN_K2, 2);
    bad += check("KNOB 2 changes PAN", trk[0].p[P_PAN] == before.p[P_PAN] + 2);
    turn(EN_K4, -1);
    bad += check("MIXER KNOB 4 left: MUTE OFF", trk[0].p[P_MUTE] == 0);
    turn(EN_ALGO, 1);
    bad += check("ALGORITHM selects the track", song.sel == 1);
    return bad;
}

/* key k of colour black (1) / white (0) at place p (seq.c key_place) */
static uint32_t key_at(int black, uint32_t p)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (key_black(k) == black && key_place(k) == p)
            return k;
    return 0;
}
static void tap_key(uint32_t k)                   /* a key down for a frame, then up */
{
    fm1_in.notes |= 1u << k;
    host_notes |= 1u << k;
    frame();
    fm1_in.notes &= ~(1u << k);
    frame();
}
static int test_screen(void)
{
    int bad = 0;
    ui_power_on();
    ui_say("STOP TO SAVE", "");
    ui.force = 1;
    ui_draw();
    bad += check("a message keeps the header's transport and BPM (drawn right of x 80)", ui.msg_t != 0);
    ui.uboot = 3;
    ui.menu = 1;
    ui.force = 1;
    ui_draw();
    bad += check("UPDATE MODE countdown drawn over the menu", ui.force == 0);
    ui.uboot = 0;
    ui.menu = 0;
    return bad;
}

static int test_favorites(void)
{
    int bad = 0;
    uint32_t total, pos;
    track_t before;
    ui_power_on();
    my_steps(TSEL);
    TSEL->p[P_AMODE] = 2;
    go_page(GR_BROWSE);
    before = *TSEL;
    turn(EN_K3, 1);
    turn(EN_K4, 1);
    pos = preset_pos(&total);
    bad += check("FAV and LIST mark/filter the current sound without loading or touching its steps",
                 preset_favorite() && favorites.filter && total == 1 && pos == 0 &&
                 !memcmp(TSEL, &before, sizeof before));
    bad += check("one favorite occupies one display row, without repeated copies",
                 preset_visible(pos, total, 0) == 0 && preset_visible(pos, total, 1) == total);
    select_engine(ENGI_WAVE);
    before = *TSEL;
    turn(EN_K3, 1);
    bad += check("a WAVE sound can be a favorite on any track", preset_favorite() &&
                 favorite_has(ENGI_WAVE, 0) && !memcmp(TSEL, &before, sizeof before));
    turn(EN_PRESET, 1);
    bad += check("filtered browsing crosses WAVE and synth sounds while retaining the track's pattern and ARP",
                 TSEL->eng_req == 0 && preset_favorite() && !memcmp(TSEL->step, before.step, sizeof before.step) &&
                 TSEL->p[P_AMODE] == 2);
    turn(EN_K3, -1);
    pos = preset_pos(&total);
    bad += check("unmarking the current sound leaves it loaded even when outside the filtered list",
                 !preset_favorite() && total == 1 && pos == total && TSEL->eng_req == 0);
    turn(EN_PRESET, -1);
    bad += check("browsing from a nonfavorite selects the last favorite", TSEL->eng_req == ENGI_WAVE);
    turn(EN_K3, -1);
    before = *TSEL;
    turn(EN_PRESET, 1);
    bad += check("empty favorites explains LIST ALL and retains the current sound and steps",
                 msg_is("NO FAVORITES") && !memcmp(TSEL, &before, sizeof before));
    turn(EN_K4, -1);
    preset_pos(&total);
    bad += check("LIST ALL restores the full browser without an implicit load", !favorites.filter && total > 1 &&
                 !memcmp(TSEL, &before, sizeof before));
    up_store(31, "Favorite");
    up_load(31);
    turn(EN_K3, 1);
    bad += check("user slot 32 has its own favorite reference", favorite_has(NENGINES, 31) && preset_favorite());
    up_store(31, "Renamed");
    bad += check("overwriting or renaming a user slot retains its star", favorite_has(NENGINES, 31));
    up_put(31, 0);
    bad += check("successful erasure removes the user slot star and source label",
                 !favorite_has(NENGINES, 31) && !up_used(31) && !TSEL->user);
    up_store(31, "New sound");
    bad += check("reusing the erased slot does not restore its old star", !favorite_has(NENGINES, 31));
    return bad;
}

/* 1.2 (Discussion #90): sound categories. The factory table covers every preset; PRESETS > LIST (KNOB 4) steps ALL FAV
 * BASS .. OTHER without a wrap and browses only that category (factory and user presets), kept with the settings in
 * favorites.factory[15][11] with favorites.filter 0 / 1 as before; the NAME screen's KNOB 3 sets a user preset's
 * category (from the sound's own), written with the name and kept by a rename; a record of before (version 2) gets
 * one by a rename, its sound unchanged; PROJECT's KNOB 3 a random name */
static int test_categories(void)
{
    int bad = 0, ok = 1;
    uint32_t e, k, total, pos, n, i;
    char b[16], s1[16];
    ui_power_on();
    memset(&favorites, 0, sizeof favorites);
    up_ram_reset();
    for (e = 0; e < NENGINES; e++)                         /* DUELL: the factory sounds (sounds.c), DRUM never */
        for (k = 0; k < ds_count(e); k++)
            ok &= preset_cat(e, k) >= CAT_BASS && preset_cat(e, k) <= CAT_OTHER && preset_cat(e, k) <= CAT_OTHER;
    bad += check("categories: every factory sound has one (BASS .. OTHER); an engine with none: its engine's",
                 ok && preset_cat(0, 0) == CAT_BASS && preset_cat(10, 0) == CAT_PERC && preset_cat(13, 0) == CAT_PERC && preset_cat(ENGI_WAVE, 99) == CAT_OTHER &&
                 (ds_count(ENGI_FM6) || preset_cat(ENGI_FM6, 0) == CAT_OTHER) && preset_cat(0, 99) == CAT_OTHER);
    go_page(GR_BROWSE);
    select_engine(0);
    apply_preset(5);                                       /* ANALOG TORCH SONG */
    turn(EN_K4, 1);
    turn(EN_K4, 1);
    pos = preset_pos(&total);
    for (n = 0, e = 0; e < NENGINES; e++)
        for (k = 0; eng_ok(e) && k < ds_count(e); k++)
            n += preset_cat(e, k) == CAT_BASS;
    bad += check("  KNOB 4: ALL, FAV, then BASS: the list holds the factory basses only, the sound not loaded",
                 list_mode() == 2u && str_eq(list_name(list_mode()), "BASS") && !favorites.filter && ui_lcat == CAT_BASS &&
                 total == n && n >= 5u && pos == total && TSEL->eng_req == 0u && TSEL->preset == 5u);
    ok = 1;
    for (i = 0; i < total; i++) {
        turn(EN_PRESET, 1);
        ok &= track_cat(TSEL) == CAT_BASS;
    }
    bad += check("  the PRESETS knob steps through basses only (every engine), and round", ok && preset_pos(&total) == total - 1u);
    turn(EN_K4, 20);
    bad += check("  .. right to OTHER and no further (no wrap)", list_mode() == LIST_N - 1u && str_eq(list_name(list_mode()), "OTHER"));
    turn(EN_PRESET, 1);
    bad += check("  OTHER: the PRESETS knob loads a factory OTHER sound (GLASS SHARDS)", track_cat(TSEL) == CAT_OTHER);
    {   /* the settings: the category's byte, unknown = none; FAV clears it */
        persist_t p;
        list_set(1u + CAT_PAD);
        settings_export(&p);
        ok = p.favorites.factory[15][11] == CAT_PAD && p.favorites.filter == 0u;
        p.favorites.factory[15][11] = 9;
        ok &= settings_import(&p, sizeof p) == 1 && list_mode() == 0u;
        p.favorites.factory[15][11] = CAT_FX;
        ok &= settings_import(&p, sizeof p) == 1 && list_mode() == 1u + CAT_FX;
        turn(EN_K4, -20);
        turn(EN_K4, 1);
        bad += check("  kept with the settings ([15][11], an unknown value: none); FAV clears it, ALL at the left end",
                     ok && list_mode() == 1u && favorites.filter == 1u && ui_lcat == 0u);
        turn(EN_K4, -1);
    }
    /* NAME: USER SAVE takes the sound's category; KNOB 3 changes it; written with the name */
    select_engine(0);
    apply_preset(0);                                       /* LADDER BASS */
    song.playing = 0;
    go_page(GR_USER);
    ui_old_uslot = 4;
    press(B_OCTUP);
    bad += check("NAME: USER SAVE starts at the sound's category (a factory BASS)", name_on() && nm.cat == CAT_BASS);
    turn(EN_K3, 1);
    ok = nm.cat == CAT_LEAD;
    turn(EN_K3, -5);
    ok &= nm.cat == CAT_BASS;
    turn(EN_K3, 1);
    press(B_OCTUP);
    bad += check("  KNOB 3 steps the category (no wrap below BASS); OCT+ writes it with the name",
                 ok && !name_on() && up_used(4) && up_cat(4) == CAT_LEAD && up_rec(4)->packed[UP_CAT_AT] == CAT_LEAD);
    list_set(1u + CAT_LEAD);
    up_load(4);
    pos = preset_pos(&total);
    bad += check("  the saved slot is in the LEAD list (at its slot number), not in BASS",
                 pos < total && preset_at(pos) == 4u &&
                 (list_set(1u + CAT_BASS), preset_pos(&total) == total));
    list_set(0);
    go_page(GR_USER);
    ui_old_uslot = 4;
    press(B_EDIT);
    ok = name_on() && nm.kind == NK_USER_RENAME && nm.cat == CAT_LEAD;
    turn(EN_K3, 3);                                        /* LEAD -> KEYS */
    press(B_OCTUP);
    up_name(4, b);
    bad += check("  a rename starts at the slot's own category and writes the new one, the sound as it was",
                 ok && up_cat(4) == CAT_KEYS && up_engine(4) == 0u);
    up_load(4);
    name_open(NK_USER_SAVE, 5);
    ok = name_on() && nm.cat == CAT_KEYS;
    press(B_OCTDN);
    bad += check("  saving a sound loaded from a user preset starts at that preset's category", ok && !name_on() && !up_used(5));
    {   /* DUELL 0.2.14: a decoded record of another version (Felucca's 1..5, a drum grid's 7) is no record */
        up_rec_t r2;
        memset(&r2, 0, sizeof r2);
        r2.used = UP_USED; r2.ver = UP_VER; r2.engine = 0; r2.np = P_COUNT;
        str_cpy(r2.name, "NEW KIT", 12);
        ok = up_valid(&r2);
        for (r2.ver = 0; r2.ver < 10u; r2.ver++)
            ok &= (r2.ver == UP_VER) == (up_valid(&r2) != 0);
        bad += check("  a record is valid in the one version UP_VER only (1..5 and 7 are no record)", ok);
    }
    /* PROJECT: KNOB 3 a random name */
    go_page(GR_SLOTS);
    name_open(NK_PROJ_SAVE, 1);
    turn(EN_K3, 1);
    str_cpy(s1, nm.s, sizeof s1);
    ok = nm.len >= 8u && nm.len <= 12u && nm.cur == nm.len && str_len(s1) == nm.len;
    for (i = 0, k = 0; i < 16u; i++)
        k |= (uint32_t)(str_len(NM_RA[i]) > 5u || str_len(NM_RB[i]) > 6u);
    turn(EN_K3, 1);
    bad += check("NAME: PROJECT's KNOB 3 types a random name (adjective + noun, at most 12), another one each detent",
                 ok && !k && !str_eq(s1, nm.s) && nm.len <= 12u && strchr(nm.s, ' '));
    name_close();
    /* KNOB 4 on every NAME screen: a new random name, either direction */
    for (i = 0; i < 4u; i++) {
        static const uint8_t kd[4] = {NK_USER_SAVE, NK_USER_RENAME, NK_PROJ_SAVE, NK_PROJ_RENAME};
        up_store(3, "KEEP");
        go_page(i < 2u ? GR_USER : GR_SLOTS);
        name_open(kd[i], i == 1u ? 3u : 0u);
        str_cpy(s1, nm.s, sizeof s1);
        ok = i == 1u ? str_eq(s1, "KEEP") : nm.len >= 1u;       /* a rename keeps its name */
        turn(EN_K4, i & 1u ? -1 : 1);
        ok &= !str_eq(s1, nm.s) && nm.len >= 8u && nm.len <= NM_LEN && nm.cur == nm.len && strchr(nm.s, ' ');
        bad += check("NAME: KNOB 4 rolls a new random name (slot save / rename, MULTI save / rename)", ok);
        name_close();
    }
    up_ram_reset();
    memset(&favorites, 0, sizeof favorites);
    return bad;
}

static int test_display_preferences(void)
{
    int bad = 0, k;
    track_t sounds[NTRK];
    uint16_t before[240 * 240];
    ui_power_on();
    memcpy(sounds, trk, sizeof sounds);
    hold(B_HOME);
    ui.menu_sel = MI_COLOR;
    ui.force = 1; ui_draw();
    memcpy(before, host_screen, sizeof before);
    settings.palette = 5; palette_set(5);
    press(B_OCTUP);
    bad += check("COLOR OCT+ previews the next palette (PAPER, light) without closing the menu",
                 settings.palette == 6 && T_BG == UI_PALETTES[6].bg && ux.light && ui.menu == 1 && !song.octave &&
                 memcmp(before, host_screen, sizeof before) && !memcmp(sounds, trk, sizeof sounds));
    turn(EN_K1, 1);
    bad += check("COLOR KNOB 1 steps on to HI-CON, NIGHT (#50: true black), MONO (1.0.2: black and white), then wraps to GREY",
                 settings.palette == 7 && (turn(EN_K1, 1), settings.palette == 8u) && T_BG == 0u &&
                 str_eq(UI_PALETTES[8].name, "NIGHT") && (turn(EN_K1, 1), settings.palette == UI_BW_INDEX) &&
                 str_eq(UI_PALETTES[UI_BW_INDEX].name, "MONO") && T_BG == 0u && T_TEXT == 0xFFFFu &&
                 (turn(EN_K1, 1), settings.palette == UI_GREY_INDEX) && str_eq(UI_PALETTES[UI_GREY_INDEX].name, "GREY"));
    menu_put(MI_USB, 1);
    ui.menu_sel = MI_USB;
    press(B_OCTUP);
    k = menu_get(MI_USB) == 1u;                        /* (OCT+ stops at the end, as the knobs) */
    press(B_OCTDN);
    k &= menu_get(MI_USB) == 0u;
    press(B_OCTDN);
    bad += check("the expanded menu retains both USB LEVEL values (OCT+ / OCT- step them, stopping at the ends; 0.2.19: the OUTPUT EQ row they were checked on is gone)",
                 k && menu_get(MI_USB) == 0u && ui.menu == 1);
    press(B_OCTDN);
    k = ui.menu == 1 && menu_get(MI_USB) == 0u;         /* (OCT- at the first value: nothing, the menu stays) */
    press(B_HOME);
    bad += check("HOME leaves display preferences without changing musical state (OCT- no longer closes)",
                 k && !ui.menu && settings.palette == UI_GREY_INDEX && !memcmp(sounds, trk, sizeof sounds));
    return bad;
}

/* 1.0.5: the MENU in tabs (ui_menu.c, menu_items.c MI_TAB): every tab's rows in a run; ALGORITHM and PRESETS do nothing, SELECT walks
 * the rows across the tabs, any of KNOB 1..4 and OCT+ / OCT- change a value, OCT+ opens BUTTON MAP and ABOUT, HOME closes (OCT- not);
 * reopened at the same tab and row; the tabs slide with ANIM ON (only their strip drawn again), snap with ANIM OFF.
 * DUELL 0.2.14 (ported from the stale groups, run again): rows of one or two units (a two-line label), at most MPAGE_UNITS (7) a page,
 * every label and its widest value fit the row (208 px between the label's pen and the value's right edge), every editor name at most
 * 12 characters, the tabs DISPLAY CONTROL AUDIO SYSTEM with the owner's rows; a tab with pages says "SYSTEM 1/2" in its pill */
static void wdt_ms(void) { fm1_ms++; }
static void wdt_key(void) { fm1_ms++; if (fm1_ms % 50u == 0u) fm1_in.notes = 1u; }   /* a key pressed after a moment */
static int32_t menu_value_w(uint32_t row, uint32_t v)  /* the width a row's value takes at the right (ui_menu.c menu_row) */
{
    const char *n = menu_vname(row, v);
    char b[8] = "0.4";
    if (!menu_valued(row))
        return 12;                                       /* (the chevron) */
    if (row == MI_HOLD || row == MI_TUNE)
        return text_w(&AF_S, row == MI_TUNE ? "ct" : "s") + 3 + text_w(&AF_M, row == MI_TUNE ? n : b);
    if (menu_n(row) == 2u && (str_eq(n, "ON") || str_eq(n, "OFF")))
        return 26;                                       /* (the switch) */
    if (row == MI_COLOR)
        return 74 + 6 + text_w(&AF_M, n);
    return text_w(&AF_M, n);
}
#define MENU_FIT_GAP 8                                   /* (at least this much between a label and its value) */
static int menu_row_fits(uint32_t row, int print)
{
    const char *l = menu_label(row);
    char l1[24];
    uint32_t n = 0, v, k;
    int32_t wv = 0, w1, w2 = 0;
    while (l[n] && l[n] != '\n' && n + 1u < sizeof l1) { l1[n] = l[n]; n++; }
    l1[n] = 0;
    w1 = text_w(&AF_M, l1);
    if (l[n] == '\n')
        w2 = text_w(&AF_S, l + n + 1);
    k = menu_valued(row) ? (row == MI_TUNE ? 2u : menu_n(row)) : 1u;
    for (v = 0; v < k; v++) {                           /* (TUNE: its widest, "-50" and "+50") */
        int32_t w = menu_value_w(row, row == MI_TUNE ? (v ? TUNE_N - 1u : 0u) : v);
        if (w > wv) wv = w;
    }
    if (print)
        printf("    menu row %-12s label %3d / %3d, value %3d, room %3d\n", MI_NAME[row], w1, w2, wv, MR_VR - MR_LX);
    return w1 + MENU_FIT_GAP + wv <= MR_VR - MR_LX && w2 + MENU_FIT_GAP + wv <= MR_VR - MR_LX && str_len(MI_NAME[row]) <= 12u;
}
static int test_menu_tabs(void)
{
    int bad = 0, ok = 1, fit = 1;
    uint32_t t, i, n, u, pg;
    static const uint8_t WANT[MTAB_COUNT][8] = {
        {MI_COLOR, MI_LEDS, MI_SCLLED, MI_ANIM, MI_SCROFF, MI_CPU, 0xFF},
        {MI_MODERECALL, MI_HOMEPG, MI_ACCEL, MI_HOLD, MI_CHADD, MI_PREVIEW, MI_PERFLEAVE, 0xFF},
        {MI_USB, MI_CLICK, MI_CLKLVL, MI_COUNTIN, MI_TUNE, 0xFF},
#if DUELL_TEST_MODE
        {MI_RESTORE, MI_PANEL, MI_SERIAL, MI_TEST, MI_GUIDE, MI_ABOUT, MI_ERASE, 0xFF}};
#else
        {MI_RESTORE, MI_PANEL, MI_SERIAL, MI_GUIDE, MI_ABOUT, MI_ERASE, 0xFF}};
#endif
    static const char *const NAMES[] = {"COLOR", "LEDS", "SCALE LEDS", "ANIMATION", "SCREEN OFF", "CPU METER",
                                        "MULTI LOAD", "HOME OPENS", "KNOB ACCEL", "BUTTON HOLD", "STEP KEYS", "STEP PREVIEW", "PERF LEAVE",
                                        "USB LEVEL", "CLICK", "CLICK LEVEL", "COUNT-IN", "TUNE",
                                        "RESTORE LAST", "BUTTON MAP", "USB SERIAL"};
    for (t = 0; t < MTAB_COUNT; t++) {
        uint32_t f = mtab_first(t), used[4] = {0, 0, 0, 0};
        n = mtab_rows(t);
        ok &= n >= 1u && f < MI_COUNT;
        for (i = 0; i < MI_COUNT; i++)                  /* its rows in one run */
            ok &= (MI_TAB[i] == t) == (i >= f && i < f + n);
        for (i = 0; i < n; i++) {                       /* the owner's rows, in his order */
            ok &= WANT[t][i] == f + i;
            pg = menu_place(f + i, &u);
            ok &= pg < 4u && u + menu_units(f + i) <= MPAGE_UNITS && (menu_units(f + i) == 1u || menu_units(f + i) == 2u);
            if (pg < 4u) used[pg] += menu_units(f + i);
            if (i) {                                    /* (each row after the one before it: same page further down, or the next page's top) */
                uint32_t u0, p0 = menu_place(f + i - 1u, &u0);
                ok &= (pg == p0 && u == u0 + menu_units(f + i - 1u)) || (pg == p0 + 1u && u == 0u);
            }
            if (!menu_row_fits(f + i, 0)) {
                menu_row_fits(f + i, 1);
                fit = 0;
            }
        }
        ok &= WANT[t][n] == 0xFF;
        for (pg = 0; pg < mtab_pages(t); pg++)
            ok &= used[pg] >= 1u && used[pg] <= MPAGE_UNITS;
    }
    for (i = 0; i < NELEM(NAMES); i++)
        ok &= str_eq(MI_NAME[i], NAMES[i]);
    ok &= mu_y(MPAGE_UNITS) - 1 + MP_PAD <= MK_Y - 6;    /* (a full page: the panel ends 6 px above the key hints) */
    ok &= menu_units(MI_SERIAL) == 2u && str_eq(menu_label(MI_SERIAL), "USB SERIAL\nDEVELOPER CONSOLE") && str_eq(menu_label(MI_ACCEL), "KNOB ACCEL");
    ok &= mtab_pages(MTAB_DISPLAY) == 1u && mtab_pages(MTAB_CONTROL) == 1u && mtab_pages(MTAB_AUDIO) == 1u &&
          mtab_pages(MTAB_SYSTEM) == 1u + DUELL_TEST_MODE;   /* (SYSTEM: 7 units in the release, 8 with TEST: 2 pages) */
    for (i = 0; i < MI_COUNT; i++) ok &= MI_TAB[i] < MTAB_COUNT;
    {   /* the tab bar: MT_GAP between every two tabs at every place of the slide, inside MT_X0..MT_X1, whichever row (and page) is selected */
        int32_t a[MTAB_COUNT], x[MTAB_COUNT], w[MTAB_COUNT], p;
        for (n = 0; n < MI_COUNT; n++)
            for (p = 0; p <= (int32_t)(MTAB_COUNT - 1u) * MT_ONE; p += 4) {
                ui.menu_sel = (uint8_t)n;
                mt.pos = (int16_t)p;
                mt_layout(a, x, w);
                ok &= x[0] >= MT_X0 && x[MTAB_COUNT - 1u] + w[MTAB_COUNT - 1u] <= MT_X1;
                for (t = 1; t < MTAB_COUNT; t++) ok &= x[t] - (x[t - 1u] + w[t - 1u]) == MT_GAP;
            }
    }
    ui_prefs = 0;
    bad += check("0.2.14 MENU: DISPLAY CONTROL AUDIO SYSTEM with the owner's rows and names, in runs; pages of at most 7 units; "
                 "tab gaps constant", ok);
    bad += check("0.2.14 MENU: every label (both lines) and its widest value fit the row, every editor name at most 12 characters", fit);
    {   /* the pages: SELECT onto a row of another page draws that page; the pill says SYSTEM 1/2 / 2/2 (test builds: TEST makes 8 units) */
        char b[16];
        ui_power_on();
        hold(B_HOME);
        ui.menu_sel = MI_ABOUT; ui.force = 1; frames(300);   /* (the tabs slid over from DISPLAY: settled) */
        mtab_label(MTAB_SYSTEM, b);
        ok = str_eq(b, DUELL_TEST_MODE ? "SYSTEM 1/2" : "SYSTEM");
#if DUELL_TEST_MODE
        {
            static uint16_t p1[240 * 240];
            memcpy(p1, host_screen, sizeof p1);
            turn(EN_SELECT, 1);                         /* ABOUT -> ERASE DATA: page 2 (RESTORE LAST 1, BUTTON MAP 1, USB SERIAL 2, TEST,
                                                         * GUIDE, ABOUT 1 each: 7 units) */
            frame();
            mtab_label(MTAB_SYSTEM, b);
            ok &= ui.menu_sel == MI_ERASE && menu_page() == 1u && str_eq(b, "SYSTEM 2/2") && memcmp(p1, host_screen, sizeof p1);
            menu_place(MI_ERASE, &u);
            ok &= u == 0u && menu_place(MI_ABOUT, &u) == 0u && u == 6u;
            turn(EN_SELECT, -1); frame();
            ok &= ui.menu_sel == MI_ABOUT && menu_page() == 0u && !memcmp(p1, host_screen, sizeof p1);   /* (page 1 again, as it was) */
        }
#endif
        menu_close();
        bad += check("0.2.14 MENU: SELECT walks onto the next page and back (the page drawn), the pill reads SYSTEM 1/2 / 2/2", ok);
    }

    ui_power_on();
    hold(B_HOME);
    ok = ui.menu == 1 && ui.menu_sel == MI_COLOR && menu_tab() == MTAB_DISPLAY;
    for (i = 0; i < 5u; i++) turn(EN_SELECT, 1);
    ok &= ui.menu_sel == MI_CPU;
    turn(EN_SELECT, 1); ok &= ui.menu_sel == MI_MODERECALL && menu_tab() == MTAB_CONTROL;   /* (SPEC 1.1a: on into the next tab) */
    turn(EN_SELECT, -1); ok &= ui.menu_sel == MI_CPU && menu_tab() == MTAB_DISPLAY;
    for (i = 0; i < 4u; i++) turn(EN_SELECT, -1);
    ok &= ui.menu_sel == MI_LEDS;
    bad += check("MENU: SELECT moves the row and goes on across the tab ends (select_test.c: all of them, both ways)", ok);
    ui.menu_sel = MI_LEDS;
    turn(EN_ALGO, 1); turn(EN_ALGO, -1); turn(EN_PRESET, 1); turn(EN_PRESET, -1);
    bad += check("MENU: ALGORITHM and PRESETS do nothing (no tab, no row)", ui.menu_sel == MI_LEDS && ui.menu == 1);
    ui.menu_sel = MI_HOLD;                             /* CONTROL: from BUTTON HOLD up to KNOB ACCEL */
    turn(EN_SELECT, -1);
    ok = ui.menu_sel == MI_ACCEL && !(ui_prefs & PREF_ACCEL);
    for (i = 0; i < 4u; i++) {                         /* any of KNOB 1..4: right ON, left OFF */
        turn(EN_K1 + i, 1); ok &= (ui_prefs & PREF_ACCEL) != 0u;
        turn(EN_K1 + i, -1); ok &= !(ui_prefs & PREF_ACCEL);
    }
    ui.menu_sel = MI_USB;                              /* AUDIO: USB LEVEL */
    turn(EN_K4, 1); ok &= (ui_prefs & PREF_USB_FIXED) && fx_usb_fixed;
    turn(EN_K2, -1); ok &= !(ui_prefs & PREF_USB_FIXED) && !fx_usb_fixed;
    bad += check("MENU: any of KNOB 1..4 changes the selected row's value in any tab, the menu stays", ok && ui.menu == 1);

    /* OCT+ / OCT-: the next / previous value on every kind of row (as the knobs: stopping at the ends; COLOR wraps) */
    ok = 1;
    for (i = 0; i < MI_COUNT; i++) {
        uint32_t n = menu_n(i), v, j, want;
        if (!menu_valued(i))                           /* (BUTTON MAP, TEST, GUIDE, ABOUT, ERASE DATA) */
            continue;
        uint8_t seen[128] = {0};                       /* (1.2: TUNE has 101 values) */
        ui.menu_sel = (uint8_t)i;
        for (j = 0; j <= n; j++)                       /* down to the first value (COLOR: round) */
            press(B_OCTDN);
        seen[menu_get(i) & 127u] = 1;
        for (j = 0; j <= n; j++) {                     /* up through every value */
            want = menu_step(i, 1);
            press(B_OCTUP);
            ok &= menu_get(i) == want && ui.menu == 1;
            seen[menu_get(i) & 127u] = 1;
        }
        for (j = 0; j < 128u; j++)
            ok &= seen[j] == (j < n);
        for (j = 0; j <= n; j++) {                     /* and down again */
            want = menu_step(i, -1);
            press(B_OCTDN);
            ok &= menu_get(i) == want && ui.menu == 1;
        }
        v = menu_get(i);                               /* the ends: COLOR wraps, the rest stop */
        if (i == MI_COLOR) {
            menu_put(i, n - 1u); press(B_OCTUP); ok &= menu_get(i) == 0u;
            press(B_OCTDN); ok &= menu_get(i) == n - 1u;
        } else {
            press(B_OCTDN); ok &= menu_get(i) == v;
            for (j = 0; j < n; j++) press(B_OCTUP);
            v = menu_get(i);
            press(B_OCTUP); ok &= menu_get(i) == v && v != menu_step(i, -1);
        }
        want = menu_step(i, -1);                       /* a knob steps as OCT does */
        turn(EN_K3, -1); ok &= menu_get(i) == want && ui.menu == 1;
    }
    ui_prefs = 0; settings_hold = HOLD_DEF; settings_leds = LEDS_DIM;
    menu_put(MI_COLOR, UI_GREY_INDEX);
    song.g[G_TUNE] = 0; song.g[G_ROUTE] = 0;
    bad += check("MENU: OCT+ / OCT- step every value row up / down (COLOR wraps, the rest stop), the menu stays", ok);
    {   /* the ON / OFF rows: up (a knob right, OCT+) is ON, the switch's knob to the right, whichever bit it is */
        static const uint8_t SW[] = {MI_ANIM, MI_ACCEL, MI_SERIAL, MI_RESTORE};
        ok = 1;
        for (i = 0; i < NELEM(SW); i++) {
            ui.menu_sel = SW[i];
            press(B_OCTDN); ok &= str_eq(menu_vname(SW[i], menu_get(SW[i])), "OFF");
            press(B_OCTUP); ok &= str_eq(menu_vname(SW[i], menu_get(SW[i])), "ON");
            turn(EN_K2, -1); ok &= str_eq(menu_vname(SW[i], menu_get(SW[i])), "OFF");
            turn(EN_K2, 1); ok &= str_eq(menu_vname(SW[i], menu_get(SW[i])), "ON");
        }
        ui_prefs = 0;
        bad += check("MENU: on an ON / OFF row OCT+ / a knob right is ON, OCT- / left OFF (ANIM, USB SERIAL, RESTORE LAST too)", ok);
    }
    ui.menu_sel = MI_USB;
    press(B_HOME);
    ok = !ui.menu;
    hold(B_HOME);
    ok &= ui.menu == 1 && ui.menu_sel == MI_USB && menu_tab() == MTAB_AUDIO;
    frames(500);
    ok &= ui.menu == 1;                                 /* (the hold's release: no tap, the menu stays) */
    hold(B_HOME);
    ok &= !ui.menu;
    frames(1000);
    ok &= !ui.menu;                                     /* (the hold that closed it: no reopen) */
    hold(B_HOME);
    bad += check("MENU: HOME pressed closes it, held too (no reopen); held opens it again at the same tab and row",
                 ok && ui.menu == 1 && ui.menu_sel == MI_USB);
    ui.menu_sel = MI_USB; for (i = MI_USB; i < MI_ABOUT; i++) turn(EN_SELECT, 1);   /* on to SYSTEM: ABOUT */
    ok = ui.menu_sel == MI_ABOUT;
    frame();
    ok &= oct_leds() == OCT_BREATH;                     /* (OCT+ opens, OCT- does nothing: dark) */
    press(B_OCTDN); ok &= ui.menu == 1 && ui.menu_sel == MI_ABOUT;   /* (no close, no back) */
    turn(EN_K1, -1); ok &= ui.menu == 1 && ui.menu_sel == MI_ABOUT;   /* (a knob left: nothing; right opens it, SPEC 6.2) */
    press(B_OCTUP); ok &= ui.menu == 2;
    ok &= oct_leds() == 1u;                             /* (ABOUT: OCT- back, lit) */
    turn(EN_K1, 1); ok &= ui.menu == 2 && ui.menu_scroll == 18;   /* (0.2.9: KNOB 1 scrolls ABOUT, SELECT goes between its sections) */
    turn(EN_K1, -1); ok &= ui.menu == 2 && ui.menu_scroll == 0;
    turn(EN_SELECT, 2); ok &= ui.menu == 2 && ui.menu_scroll > 1;
    turn(EN_PRESET, 4); ok &= ui.menu == 2;
    press(B_OCTDN); ok &= ui.menu == 1 && ui.menu_sel == MI_ABOUT;
#if DUELL_TEST_MODE
    turn(EN_SELECT, -2); ok &= ui.menu_sel == MI_TEST;   /* (1.2: INFO, DUELL 0.2.2: TEST) as ABOUT, no scroll (past GUIDE) */
    frame();
    ok &= oct_leds() == OCT_BREATH;
    press(B_OCTDN); ok &= ui.menu == 1 && ui.menu_sel == MI_TEST;
    press(B_OCTUP); ok &= ui.menu == 3 && oct_leds() == 1u;
    turn(EN_PRESET, 3); turn(EN_K1, 1); turn(EN_ALGO, 1);
    ok &= ui.menu == 3 && ui.menu_sel == MI_TEST;      /* (ABOUT's scroll is kept from before: not checked here) */
    press(B_OCTDN); ok &= ui.menu == 1 && ui.menu_sel == MI_TEST;
#endif
    ui.menu_sel = MI_PANEL; frame();                    /* (0.2.14: BUTTON MAP, between RESTORE LAST and USB SERIAL) */
    press(B_OCTDN); ok &= ui.menu == 1 && ui.menu_sel == MI_PANEL && oct_leds() == OCT_BREATH;
    {   /* BUTTON MAP: OCT+ starts the setup (nothing pressed here: it times out and keeps the table) */
        panel_t p0 = panel;
        host_wdt_hook = wdt_ms;
        press(B_OCTUP);
        host_wdt_hook = 0;
        ok &= !memcmp(&p0, &panel, sizeof panel) && msg_is("SETUP CANCELLED") && ui.menu == 1;
    }
    {   /* BUTTON MAP: a piano key cancels at once (no 30 s wait), the table kept */
        panel_t p0 = panel;
        uint32_t t = fm1_ms;
        host_wdt_hook = wdt_key;
        press(B_OCTUP);
        host_wdt_hook = 0;
        fm1_in.notes = 0;
        ok &= !memcmp(&p0, &panel, sizeof panel) && msg_is("SETUP CANCELLED") && ui.menu == 1 && fm1_ms - t < 1000u;
    }
    bad += check("MENU SYSTEM: OCT+ opens ABOUT, TEST (OCT- back to the tab) and BUTTON MAP; OCT- does nothing on them", ok);
    ui.menu_sel = MI_ABOUT; press(B_OCTUP);             /* ABOUT, HOME: closed, from the document */
    ok = ui.menu == 2;
    press(B_HOME);
    ok &= !ui.menu && cur_page()->fam == FAM_TRK;   /* (0.2.14: the page HOME OPENS says, one of HOME's) */
    hold(B_HOME); ok &= ui.menu == 1 && ui.menu_sel == MI_ABOUT;   /* (opened at the tabs, ABOUT's row) */
    ui.menu_sel = MI_USB; frame();                      /* (a value row) */
    {   /* a value row: OCT- and OCT+ both breathe, each while that way has a value to go to; none lit */
        uint32_t r = ui.menu_sel, want = (menu_step(r, -1) != menu_get(r) ? OCT_BREATH_DN : 0u) |
                                         (menu_step(r, 1) != menu_get(r) ? OCT_BREATH : 0u);
        ok &= menu_valued(r) && oct_leds() == want && want != 0u;
        menu_put(MI_HOLD, 0u); ui.menu_sel = MI_HOLD;   /* HOLD at its first value: OCT- dark, OCT+ breathing */
        ok &= oct_leds() == OCT_BREATH;
        menu_put(MI_HOLD, 1u);                          /* in between: both breathe */
        ok &= oct_leds() == (OCT_BREATH | OCT_BREATH_DN);
    }
    menu_close();
    bad += check("MENU: HOME closes ABOUT too; reopened at the tabs; a value row breathes OCT- / OCT+ where they can go", ok);

    /* the slide: ANIM ON, a few frames from DISPLAY to CONTROL drawing only the tab strip; ANIM OFF, at once */
    ui_power_on();
    hold(B_HOME);
    ui.menu_sel = MI_CPU;                               /* (DISPLAY's last row: SELECT goes on to CONTROL) */
    frame();
    turn(EN_SELECT, 1);
    ok = mt.pos > 0 && mt.pos < MT_ONE;                 /* (a step on the way) */
    host_blit_rows = 0; frame();
    ok &= host_blit_rows == (uint32_t)(MP_Y - H_HEAD);  /* (only the strip) */
    for (i = 0; i < 8u && mt.pos != MT_ONE; i++) frame();
    ok &= mt.pos == MT_ONE && i <= 6u;
    host_blit_rows = 0; frame();
    ok &= host_blit_rows == 0u;                         /* (settled: nothing drawn) */
    ui_prefs |= PREF_ANIM_OFF;
    ui.menu_sel = (uint8_t)(mtab_first(MTAB_AUDIO) - 1u);
    turn(EN_SELECT, 1);
    ok &= mt.pos == 2 * MT_ONE;
    ui_prefs &= ~PREF_ANIM_OFF;
    menu_close();
    bad += check("MENU tabs slide with ANIM ON (their strip only, settled within 6 frames), snap with ANIM OFF", ok);
    return bad;
}

/* 1.2: MENU > AUDIO > TUNE and MENU > MIDI > MIDI IN edit the project's G_TUNE and G_ROUTE (up to 1.1.5 GLO > GLOBAL K4
 * and GLO > SYSTEM K3): the same values, stored with the project as before (a PROJECT save / load, the autosave), never
 * in the settings record */
static int test_menu_project_rows(void)
{
    int bad = 0, ok;
    persist_t p0, p1;
    uint32_t sig;
    ui_power_on();
    memset(&p0, 0, sizeof p0); memset(&p1, 0, sizeof p1);   /* (export keeps some fields as they were) */
    settings_export(&p0);
    sig = autosave_sig();
    hold(B_HOME);
    ui.menu_sel = MI_TUNE;
    ok = ui.menu == 1 && menu_tab() == MTAB_AUDIO && song.g[G_TUNE] == 0 && str_eq(menu_vname(MI_TUNE, menu_get(MI_TUNE)), "0");
    press(B_OCTUP); ok &= song.g[G_TUNE] == 1 && str_eq(menu_vname(MI_TUNE, menu_get(MI_TUNE)), "+1");
    turn(EN_K2, 11); ok &= song.g[G_TUNE] == 12;         /* (a fast turn: as far as the knob went) */
    turn(EN_K1, -40); ok &= song.g[G_TUNE] == -28 && str_eq(menu_vname(MI_TUNE, menu_get(MI_TUNE)), "-28");
    turn(EN_K3, -100); ok &= song.g[G_TUNE] == -50;      /* (the ends: -50 .. +50 cents) */
    press(B_OCTDN); ok &= song.g[G_TUNE] == -50;
    turn(EN_K4, 200); ok &= song.g[G_TUNE] == 50 && str_eq(menu_vname(MI_TUNE, menu_get(MI_TUNE)), "+50");
    turn(EN_K4, -31); ok &= song.g[G_TUNE] == 19;
    bad += check("MENU > AUDIO > TUNE: the project's G_TUNE, -50 .. +50 cents (OCT one, a knob as far as it turned)", ok);
    menu_close();
    settings_export(&p1);
    ok = !memcmp(&p0, &p1, sizeof p0) && autosave_sig() != sig;
    bad += check("TUNE / MIDI IN: nothing in the settings record; the autosave sees the change (the music's)", ok);
    project_save(2);                                     /* (PROJECT C) */
    song.g[G_TUNE] = 0; song.g[G_ROUTE] = 0;
    project_load(2);
    ok = song.g[G_TUNE] == 19 && song.g[G_ROUTE] == 3 && menu_get(MI_TUNE) == 69u;
    song.g[G_TUNE] = -7; song.g[G_ROUTE] = 4;            /* (another project's values: the rows follow the load) */
    project_save(3);
    project_load(2);
    ok &= song.g[G_TUNE] == 19 && song.g[G_ROUTE] == 3;
    project_load(3);
    ok &= song.g[G_TUNE] == -7 && song.g[G_ROUTE] == 4 && str_eq(menu_vname(MI_TUNE, menu_get(MI_TUNE)), "-7");
    bad += check("TUNE / MIDI IN: saved with the project and loaded back (round trip; each project its own)", ok);
    ui_power_on();
    return bad;
}

static int test_information(void)
{
    uint16_t header[240 * H_HEAD], footer[240 * 19], battery[240 * H_HEAD];
    track_t sounds[NTRK];
    int bad = 0, quiet = 1;
    uint32_t x, y;
    ui_power_on();
    memcpy(sounds, trk, sizeof sounds);
    hold(B_HOME);
    ui.menu_sel = MI_ABOUT;
    press(B_OCTUP);
    ui_draw();
    bad += check("ABOUT opens one document with scrollable license text and credits", ui.menu == 2 && !ui.menu_scroll && menu_scroll_max() > MENU_DOC_H);
    for (y = 146; y < 220; y++)
        for (x = 158; x < 232; x++)
            if (x < 166 || x >= 224 || y < 154 || y >= 212)
                quiet &= host_screen[y * 240 + x] == swap16(UI_QR_LIGHT);
    bad += check("ABOUT QR keeps its complete white quiet zone above the fixed footer", quiet);
    memcpy(header, host_screen, sizeof header);
    memcpy(footer, host_screen + 221 * 240, sizeof footer);
    turn(EN_PRESET, 3);                                 /* (PRESETS: nothing, SPEC 1.1a) */
    ui_draw();
    bad += check("PRESETS does not scroll ABOUT", ui.menu == 2 && !ui.menu_scroll);
    turn(EN_SELECT, 3);
    ui_draw();
    bad += check("SELECT scrolls ABOUT with its header and controls fixed",
                 ui.menu == 2 && ui.menu_scroll > 0 && !memcmp(header, host_screen, sizeof header) &&
                 !memcmp(footer, host_screen + 221 * 240, sizeof footer));
    turn(EN_SELECT, 12);
    ui_draw();
    bad += check("SELECT continues from ABOUT into CREDITS without changing pages",
                 ui.menu == 2 && ui.menu_scroll > 216 && !memcmp(header, host_screen, sizeof header) &&
                 !memcmp(footer, host_screen + 221 * 240, sizeof footer));
    uint16_t position = ui.menu_scroll;
    press(B_OCTUP);
    bad += check("OCT+ does not switch pages or alter the octave in the document",
                 ui.menu == 2 && ui.menu_scroll == position && song.octave == 0);
    turn(EN_SELECT, 10000);
    turn(EN_SELECT, 10000);
    ui_draw();
    bad += check("the continuous document stops at its final credit",
                 ui.menu == 2 && ui.menu_scroll == menu_scroll_max());
    turn(EN_SELECT, -10000);
    bad += check("scrolling back through CREDITS returns to ABOUT's top", ui.menu == 2 && !ui.menu_scroll);
    press(B_OCTDN);
    bad += check("OCT- returns to the menu without changing sounds or patterns",
                 ui.menu == 1 && !memcmp(sounds, trk, sizeof sounds));
    press(B_OCTUP);
    bad += check("reopening ABOUT starts the continuous document at the top", ui.menu == 2 && !ui.menu_scroll);
    menu_close();
    ui.msg_t = 0;
    usb.config = 1; usb.suspended = 0;
    song.batt_raw = 530;
    ui.force = 1; draw_head(); ui.force = 0;
    memcpy(battery, host_screen, sizeof battery);
    uint32_t signature = ui.head_sig;
    for (x = 0; x < 4; x++) { fm1_ms += 600; draw_head(); }
    bad += check("USB power shows a steady bolt inside the battery without animation",
                 batt_shown() == 4 && ui.head_sig == signature && !memcmp(battery, host_screen, sizeof battery) &&
                 host_screen[6 * 240 + 217] != 0 && host_screen[9 * 240 + 225] != 0);
    usb.suspended = 1;
    draw_head();
    bad += check("USB suspension restores the battery outline and measured level",
                 batt_shown() == 0 && ui.head_sig != signature && host_screen[6 * 240 + 217] != 0);
    usb.config = 0; usb.suspended = 0;
    song.batt_raw = 531; int levels = batt_shown() == 1;
    song.batt_raw = 561; levels &= batt_shown() == 2;
    song.batt_raw = 591; levels &= batt_shown() == 3;
    bad += check("battery operation retains all three ADC level thresholds", levels);
    return bad;
}

/* GREY is grayscale on every screen: every page of every engine, HOME, the menu, ABOUT, the dialogs (RGB565 gray:
 * R = B, G = 2 R). MONO (black and white, blended per channel) is neutral on every screen: R = B, G within 2 of 2 R
 * (the 5-6-5 rounding of its edges) */
static int screen_gray(void)
{
    uint32_t i;
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);
        if ((c >> 11) != (c & 31u) || ((c >> 5) & 63u) != (c >> 11) * 2u)
            return 0;
    }
    return 1;
}
static int screen_neutral(void)
{
    uint32_t i;
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);
        int32_t d = (int32_t)((c >> 5) & 63u) - (int32_t)(c >> 11) * 2;
        if ((c >> 11) != (c & 31u) || d < -2 || d > 2)
            return 0;
    }
    return 1;
}
static int mono_screens(uint32_t pal, int (*chk)(void), uint32_t *n)
{
    int ok = 1;
    uint32_t e, i;
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e))
            continue;                               /* (DIGITAL without FELUCCA_FM4: never a track's) */
        for (i = 0; i < NPAGES; i++) {
            ui_power_on(); set_engine_of(TSEL, e); song.playing = 1; song.rec = 1;
            ui.page = (uint8_t)i; page_entered();
            if (!page_visible(i)) continue;
            settings.palette = pal; palette_set(pal);
            memset(host_screen, 0, sizeof host_screen);
            ui.force = 1; ui_draw();
            ok &= chk();
            (*n)++;
        }
        ui_power_on(); set_engine_of(TSEL, e); go_main_to(HP_MORPH);
        settings.palette = pal; palette_set(pal);
        ui.force = 1; ui_draw(); ok &= chk();
    }
    for (i = 0; i < 4u; i++) {
        ui_power_on(); song.rec = 3;
        settings.palette = pal; palette_set(pal);
        if (i < 2u) { ui.menu = (uint8_t)(i + 1u); ui.menu_scroll = 300; }
        else if (i == 2u) ui.confirm = CF_OVR_PROJ;
        else ui.uboot = 2;
        ui.force = 1; ui_draw(); ok &= chk();
    }
    return ok;
}
static int test_mono_screens(void)
{
    int bad = 0, ok;
    uint32_t e, n = 0, nb = 0;
    ok = mono_screens(UI_GREY_INDEX, screen_gray, &n);
    bad += check("GREY: every pixel of every page, HOME, menu, ABOUT, dialog and countdown is gray", ok && n > 200u);
    ok = mono_screens(UI_BW_INDEX, screen_neutral, &nb);
    bad += check("MONO (black and white): every pixel of every page, HOME, menu, ABOUT, dialog and countdown is neutral",
                 ok && nb == n);
    ui_power_on();
    for (e = 0; e < NPALETTES; e++) {               /* the other palettes are not gray: the check sees colour */
        ui_power_on(); palette_set(e); ui.force = 1; ui_draw();
        ok = screen_gray();
        if (e == UI_GREY_INDEX ? !ok : ok) bad += check("palette colour shows on HOME", 0);
    }
    palette_set(UI_GREY_INDEX);
    return bad;
}

/* ROLL: the rolling digits of the four cards and the header BPM (ui_draw.c roll_*). The old GLOBAL page's four (1.2: a
 * test page over HOME > CLOCK, whose KNOB 4 is empty since TUNE went to the MENU): BPM (also in the header), SWG, CLK (a
 * name), TUNE (signed). A roll frame differs from the static render only inside the
 * value strip, its last frame is the static render; unchanged digits stay; the direction follows the sign; a
 * change mid-roll retargets; a fast turn, another shape, a page / track / palette change and ui.force snap. */
static uint16_t roll_shot[240 * 240];
static const page_t ROLL_PG = {"CLOCK", FAM_TRK, SC_GLOBAL, GR_NONE, {G_BPM, G_SWING, G_CLOCK, G_TUNE}};
static void roll_settle(void)                     /* CLOCK + TUNE, nothing rolling, no knob hot */
{
    uint32_t k;
    go_title("CLOCK");
    page_over = &ROLL_PG;                         /* (cur_page(): its knobs, its cards; a layer's way, ui_layer.c) */
    ui.hot_t = 0; ui.bpm_t = 0;
    for (k = 0; k < 12u; k++) frame();
}
/* host_screen against roll_shot: the pixels that differ inside the box (in = 1) or outside it */
static uint32_t roll_diff(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int in)
{
    uint32_t n = 0;
    int32_t x, y;
    for (y = 0; y < 240; y++)
        for (x = 0; x < 240; x++) {
            int inside = x >= x0 && x < x1 && y >= y0 && y < y1;
            if (inside == in && host_screen[y * 240 + x] != roll_shot[y * 240 + x]) n++;
        }
    return n;
}
/* the mean row (x16) of the ink (not the card surface) in columns x0..x1-1, rows y0..y1-1 of a screen */
static int32_t roll_ink_row(const uint16_t *s, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    int32_t x, y, n = 0, sum = 0;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            if (s[y * 240 + x] != swap16(T_SURF)) { n++; sum += y * 16; }
    return n ? sum / n : -1;
}
static int roll_static_now(void)                  /* the screen is the static render (a forced redraw changes nothing) */
{
    memcpy(roll_shot, host_screen, sizeof roll_shot);
    ui.force = 1;
    ui_draw();
    return roll_diff(0, 0, 0, 0, 0) == 0;
}
#define STRIP(c) CARD_X(c), Y_LABEL + ROLL_Y, CARD_X(c) + COL_W, Y_LABEL + ROLL_Y + ROLL_H
static int test_roll(void)
{
    int bad = 0, ok;
    uint32_t k, n, e, id;
    int32_t up, down, st;
    static const struct { const char *a, *b; int d; } DIRS[] = {
        {"120", "121", 1}, {"121", "120", -1}, {"129", "130", 1}, {"-12", "-13", -1}, {"-13", "-12", 1}, {"+5", "+6", 1},
        {"1.25", "1.30", 1}, {"0.9", "0.8", -1}, {"9", "10", 0}, {"0", "-1", 0}, {"+5", "-5", 0}, {"C3", "C4", 0},
        {"1/4", "1/8", 0}, {"OFF", "ON", 0}, {"2X", "4X", 0}, {"120", "120", 0}, {"123456", "123457", 1},
        {"1234567", "1234568", 0}, {"12%", "13%", 0}};

    ok = 1;
    for (k = 0; k < sizeof DIRS / sizeof DIRS[0]; k++)
        if (roll_dir(DIRS[k].a, DIRS[k].b) != DIRS[k].d) { printf("  roll_dir %s -> %s\n", DIRS[k].a, DIRS[k].b); ok = 0; }
    bad += check("roll: numbers of one shape roll by the sign of the change; names, length, sign changes snap", ok);
    ok = 1;                                         /* every page of every engine: names never roll, numbers do */
    n = 0;
    for (e = 0; e < NENGINES; e++)
        for (id = 0; id < NPAGES && eng_ok(e); id++) {
            uint32_t c;
            ui_power_on(); set_engine_of(TSEL, e);
            ui.page = (uint8_t)id; page_entered();
            if (!page_visible(id) || PAGES[id].graph == GR_MOD) continue;
            ui.force = 1; ui_draw();
            for (c = 0; c < 4u; c++) {
                int16_t *vp;
                const param_desc_t *d = PAGES[id].scope == SC_GLOBAL || PAGES[id].scope == SC_TRACK ||
                                        PAGES[id].scope == SC_ENGINE ? page_desc(cur_page(), c, &vp) : 0;
                int32_t v, v0;
                if (!d || !d->label || d->label[0] == '-' || ((act_cols() >> c) & 1u)) continue;
                v0 = *vp;
                for (v = d->min; v < d->max && v - d->min < 40; v++) {
                    *vp = (int16_t)v; ui.frame += ROLL_SNAP; draw_columns();
                    *vp = (int16_t)(v + 1); ui.frame += ROLL_SNAP; draw_columns();
                    if (ui.roll[c].from[0] && (d->fmt == F_ENUM || d->fmt == F_NOTE || d->fmt == F_ONOFF)) {
                        printf("  %s/%s %s: %s rolls\n", ENGINES[e]->name, PAGES[id].title, d->label, ui.roll[c].from);
                        ok = 0;
                    }
                    n += ui.roll[c].from[0] != 0;
                }
                *vp = (int16_t)v0;
            }
        }
    ok &= n > 500u;
    bad += check("roll: every page and engine: a name (enum, even ALG 1..8, note, ON/OFF) snaps, numbers roll", ok);

    /* TUNE 0 -> 1 (card 4): a roll of ROLL_FRAMES frames, the last one the static render */
    ui_power_on(); roll_settle();
    turn(EN_K4, 1);
    ok = song.g[G_TUNE] == 1 && str_eq(ui.roll[3].from, "0") && ui.roll[3].dir == 1;
    for (n = 1; ui.roll[3].from[0] && n < 20u; n++) frame();
    ok &= n == ROLL_FRAMES;
    bad += check("roll: a card's value rolls for 9 frames (TUNE 0 -> 1), the value itself at once", ok);
    bad += check("roll: the roll's last frame is pixel-identical to the static render", roll_static_now());

    /* mid-roll: only the value strip differs from the static render */
    ui_power_on(); roll_settle();
    turn(EN_K4, 1); frame(); frame(); frame();
    memcpy(roll_shot, host_screen, sizeof roll_shot);
    ui.force = 1; ui_draw();                       /* (snaps: the static render) */
    ok = roll_diff(STRIP(3), 0) == 0 && roll_diff(STRIP(3), 1) > 20u && !ui.roll[3].from[0];
    bad += check("roll: a mid-roll frame differs from the static render inside the value strip only", ok);

    /* BPM 120 -> 121: the BPM card rolls (no unit: the label says BPM: set in M, it rolls like any number); the header has no tempo any more */
    ui_power_on(); roll_settle();
    bpm_nudge(1);
    ok = song.g[G_BPM] == 121 && !ui.roll[ROLL_BPM].from[0] &&
         str_eq(ui.roll[0].from, "120") && ui.roll[0].dir == 1;
    frame(); frame();
    memcpy(roll_shot, host_screen, sizeof roll_shot);
    ui.force = 1; ui_draw();
    ok &= roll_diff(0, 0, 240, H_HEAD, 1) == 0;       /* (the header: nothing rolls there) */
    ok &= roll_diff(CARD_X(0), Y_LABEL + ROLL_Y, CARD_X(0) + 5 + text_w(&AF_M, "12"), Y_LABEL + ROLL_Y + ROLL_H, 1) == 0;
    /* TUNE 10 -> 11 on a card: the 1 stays */
    ui_power_on(); song.g[G_TUNE] = 10; roll_settle();
    turn(EN_K4, 1); frame(); frame();
    memcpy(roll_shot, host_screen, sizeof roll_shot);
    ui.force = 1; ui_draw();
    ok &= song.g[G_TUNE] == 11 && roll_diff(STRIP(3), 1) > 20u && roll_diff(STRIP(3), 0) == 0;
    ok &= roll_diff(CARD_X(3), Y_LABEL + ROLL_Y, CARD_X(3) + 5 + text_w(&AF_M, "1"), Y_LABEL + ROLL_Y + ROLL_H, 1) == 0;
    bad += check("roll: BPM card 120 -> 121 and a card 10 -> 11: only the strip changes, unchanged digits stay put", ok);

    /* the direction: 5 -> 6 and -5 -> -6 at frame 4: the new digit comes from below (up) / above (down) */
    ui_power_on(); song.g[G_TUNE] = 5; roll_settle();
    st = roll_ink_row(host_screen, CARD_X(3) + 5, CARD_X(3) + 16, Y_LABEL + ROLL_Y, Y_LABEL + ROLL_Y + ROLL_H);
    turn(EN_K4, 1); frame(); frame(); frame();
    up = roll_ink_row(host_screen, CARD_X(3) + 5, CARD_X(3) + 16, Y_LABEL + ROLL_Y, Y_LABEL + ROLL_Y + ROLL_H);
    ok = ui.roll[3].dir == 1;
    ui_power_on(); song.g[G_TUNE] = 7; roll_settle();
    turn(EN_K4, -1); frame(); frame(); frame();
    down = roll_ink_row(host_screen, CARD_X(3) + 5, CARD_X(3) + 16, Y_LABEL + ROLL_Y, Y_LABEL + ROLL_Y + ROLL_H);
    ok &= ui.roll[3].dir == -1 && st > 0 && up > st + 16 && down < st - 16;
    ui_power_on(); song.g[G_TUNE] = -5; roll_settle();
    turn(EN_K4, -1);
    ok &= song.g[G_TUNE] == -6 && str_eq(ui.roll[3].from, "-5") && ui.roll[3].dir == -1;
    bad += check("roll: an increase rolls up, a decrease down (-5 -> -6 is down)", ok);

    /* retarget: a change mid-roll restarts from the value it was going to; a fast turn snaps */
    ui_power_on(); song.g[G_TUNE] = 1; roll_settle();
    turn(EN_K4, 1); frame(); frame(); frame();
    turn(EN_K4, 1);
    ok = song.g[G_TUNE] == 3 && str_eq(ui.roll[3].from, "2") && (uint8_t)(ui.frame - ui.roll[3].t0) == 0;
    for (n = 1; ui.roll[3].from[0] && n < 20u; n++) frame();
    ok &= n == ROLL_FRAMES && roll_static_now();
    bad += check("roll: a change mid-roll retargets (from the last value, restarted) and ends static", ok);
    ui_power_on(); song.g[G_TUNE] = 1; roll_settle();
    turn(EN_K4, 1); turn(EN_K4, 1);
    ok = song.g[G_TUNE] == 3 && !ui.roll[3].from[0] && roll_static_now();
    bad += check("roll: a change within 40 ms of the last one snaps (a fast turn)", ok);

    /* snaps: another shape, a name, the page, the track, the palette, ui.force */
    ui_power_on(); roll_settle();
    turn(EN_K4, -1);
    ok = song.g[G_TUNE] == -1 && !ui.roll[3].from[0] && roll_static_now();
    for (k = 0; k < 5u; k++) frame();
    turn(EN_K3, 1);
    ok &= song.g[G_CLOCK] != 0 && !ui.roll[2].from[0] && roll_static_now();
    bad += check("roll: a sign / length change (0 -> -1) and a name (CLK) snap", ok);
    ok = 1;
    for (k = 0; k < 5u; k++) {
        ui_power_on(); roll_settle();
        turn(EN_K4, 1);
        ok &= ui.roll[3].from[0] != 0;
        if (k == 0) { page_over = 0; go_title("AMP ENV"); }
        else if (k == 1) turn(EN_ALGO, 1);
        else if (k == 2) { settings.palette = 1; palette_set(1); }          /* (no ui.force) */
        else if (k == 3) set_engine_of(TSEL, eng_step(TSEL->eng_req, 1));   /* (no ui.force) */
        else ui.force = 1;
        frame();
        ok &= !ui.roll[3].from[0] && !ui.roll[ROLL_BPM].from[0];
    }
    ui_power_on(); roll_settle();
    bpm_nudge(1);
    ok &= ui.roll[0].from[0] != 0;
    ui.force = 1; frame();
    ok &= !ui.roll[0].from[0];
    bad += check("roll: a page, track, palette or engine change and ui.force snap", ok);
    palette_set(UI_GREY_INDEX);

    /* MONO: every roll frame is gray */
    ui_power_on(); roll_settle();
    ok = 1;
    bpm_nudge(9);
    turn(EN_K4, -1);
    for (k = 0; k < ROLL_FRAMES; k++) { ok &= screen_gray(); frame(); }
    bad += check("roll: MONO roll frames are gray", ok);
    page_over = 0;
    ui_power_on();
    return bad;
}

/* #63 ("parameters moving on their own, fixed by a page change"): a soak of the cards and the rolling digits.
 * ~40 min of UI frames: random turns of KNOB 1..4 and SELECT (bursts back and forth), page / HOME / track
 * changes, the transport and REC (motion recorded and played back), LFO rates, uneven frame times, ui.frame
 * and fm1_ms wrapping. After every frame no roll is older than ROLL_FRAMES; at random quiet moments (no
 * roll, nothing hot) the cards are pixel-identical to a forced redraw (what a page change would show). */
static int test_soak_cards(void)
{
    uint32_t seed = 63u, f, quiet = 0, old = 0, stale = 0, cmp = 0, c;
    static uint16_t shot[240 * CARD_H];
#define SOAK_RND(n) ((seed = seed * 1103515245u + 12345u) >> 8) % (n)
    ui_power_on();
    ui.frame = 0xFFFFFF00u;
    fm1_ms = 0xFFFFFFFFu - 60000u;
    for (f = 0; f < 150000u; f++) {
        uint32_t r = SOAK_RND(1000u);
        if (r < 25u) {                                  /* a knob, 1..3 detents either way */
            uint32_t role = SOAK_RND(5u) ? EN_K1 + SOAK_RND(4u) : EN_SELECT;
            int32_t s = (int32_t)SOAK_RND(3u) + 1;
            host_enc[panel.enc[role]] += (SOAK_RND(2u) ? s : -s) * panel.dir[role];
            quiet = 0;
        } else if (r < 29u) {                           /* a page */
            ui.confirm = 0;                             /* (ENGINE's KNOB 4 asks INIT: DUELL 3.6; nothing answers it here) */
            uint32_t i = SOAK_RND(NPAGES);
            if (page_visible(i)) { ui.page = (uint8_t)i; page_entered(); }
            quiet = 0;
        } else if (r < 30u) { go_main_to(HP_MORPH); page_entered(); quiet = 0; }
        else if (r < 31u) transport_req = song.playing ? 2u : 1u;
        else if (r == 31u && SOAK_RND(3u) == 0u) rec_tap();
        else if (r == 32u) { host_enc[panel.enc[EN_ALGO]] += SOAK_RND(2u) ? 1 : -1; quiet = 0; }
        else if (r == 33u) trk[SOAK_RND(NTRK)].p[P_LRATE] = (int16_t)SOAK_RND(128u);
        fm1_ms += SOAK_RND(50u) ? 16u : 1u + SOAK_RND(200u);
        host_ticks += 16000u + SOAK_RND(3u) * 4000u;
        ui_input();
        if (song.playing || transport_req) events_block(FS * 16u / 1000u);
        ui_draw();
        quiet++;
        if (ui.menu || ui.layer || ui.confirm || name_on() || ui.uboot)
            continue;                                   /* (the cards are not drawn) */
        for (c = 0; c < 5u; c++)
            old += ui.roll[c].from[0] && (uint8_t)(ui.frame - ui.roll[c].t0) >= ROLL_FRAMES;
        if (quiet > 45u && !ui.hot_t && !ui.bpm_t && !ui.msg_t && SOAK_RND(8u) == 0u) {
            for (c = 0; c < 5u && !ui.roll[c].from[0]; c++) {}
            if (c < 5u)
                continue;
            memcpy(shot, &host_screen[Y_LABEL * 240], sizeof shot);
            ui.force = 1;
            ui_draw();
            cmp++;
            stale += memcmp(shot, &host_screen[Y_LABEL * 240], sizeof shot) != 0;
        }
    }
#undef SOAK_RND
    if (old || stale || cmp < 500u) printf("  soak: %u old rolls, %u stale cards in %u checks\n", old, stale, cmp);
    stop_transport();
    memset(&motion, 0, sizeof motion);                  /* (what it recorded: the tests below start without) */
    memset(motion_active, 0, sizeof motion_active);
    motion_base_valid = motion_full = 0;
    ui_power_on();
    return check("#63 soak: 150000 random frames, wrapping clocks, motion: no roll lasts, cards = a forced redraw",
                 !old && !stale && cmp >= 500u);
}

static void screen_save(const char *dir, const char *name)
{
    char path[512];
    uint32_t i;
    FILE *f;
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    f = fopen(path, "wb");
    if (!f) return;
    ui.msg_t = 0; ui.hot_t = 0; ui.force = 1;
    ui_draw();
    fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);           /* canvas stores panel-order RGB565 */
        uint8_t b[3] = {(uint8_t)((c >> 11) * 255u / 31u), (uint8_t)(((c >> 5) & 63u) * 255u / 63u), (uint8_t)((c & 31u) * 255u / 31u)};
        fwrite(b, 1, 3, f);
    }
    fclose(f);
}
/* Discussions #92 / #94: the PRESETS knob on every page. STEP and the DRUM grid: the step cursor (a step held: its
 * NUDGE, 1.2: tests/fun10_test.c), the sound and the steps untouched; AUTOMATION, USER, PROJECT, PHRASES, SONG: the
 * selection, as KNOB 1; TOOLS: nothing; HOME, PRESETS and every other page: the next sound, the steps untouched, SAVE
 * held undoes it */
static int test_presets_knob(void)
{
    int bad = 0, cur_ok = 1, sel_ok = 1, snd_ok = 1, undo_ok = 1, tools_ok = 1;
    uint32_t i, n_snd = 0, n_cur = 0;
    for (i = 0; i < NPAGES; i++) {
        track_t before;
        uint32_t g = PAGES[i].graph, uslot, ppick;
        int16_t slot;
        ui_power_on();
        song.sel = 0;
        my_steps(TSEL);
        ui.page = (uint8_t)i; page_entered();
        if (!page_visible(i))
            continue;
        frame();
        before = *TSEL;
        uslot = ui_old_uslot; ppick = pat_pick(); slot = song.g[G_SLOT];
        turn(EN_PRESET, 1);
        {
            int same_snd = TSEL->preset == before.preset && TSEL->eng_req == before.eng_req &&
                           !memcmp(TSEL->p, before.p, sizeof before.p);
            int same_steps = !memcmp(TSEL->step, before.step, sizeof before.step);
            if (g == GR_ROLL) {
                cur_ok &= ui.cursor == 0u && same_snd && same_steps && ui.page == i;
                n_cur++;
            } else if (g == GR_USER) {
                sel_ok &= ui_old_uslot == uslot && same_snd && same_steps;
            } else if (g == GR_SLOTS) {
                sel_ok &= song.g[G_SLOT] == slot && same_snd && same_steps;
            } else if (g == GR_PATS) {
                sel_ok &= pat_pick() == ppick && same_snd && same_steps;
            } else if (0) {
            } else if (g == GR_EVENTS) {               /* (AUTOMATION: its row, PLAY's to the first record's) */
                sel_ok &= same_snd && same_steps && ui.page == i && ui.ev_row == 0u;
            } else if (g == GR_TOOLS) {
                tools_ok &= same_snd && same_steps && !ui.confirm;
            } else {
                snd_ok &= !same_snd && same_steps && ui.page == i && !at_home();
                hold(B_SAVE);
                undo_ok &= TSEL->preset == before.preset && TSEL->eng_req == before.eng_req &&
                           !memcmp(TSEL->p, before.p, sizeof before.p) && !memcmp(TSEL->step, before.step, sizeof before.step);
                n_snd++;
            }
        }
    }
    bad += check("#92 PRESETS on STEP: nothing (SPEC 1.1a); the sound and the steps stay", cur_ok && n_cur == 1u);
    bad += check("#94 PRESETS on USER, PROJECT, PHRASES, SONG, AUTOMATION: nothing (no second navigation); nothing loaded", sel_ok);
    bad += check("#94 PRESETS on TOOLS does nothing", tools_ok);
    bad += check("#94 PRESETS elsewhere (EDIT, ENV, LFO, FX, SCL, ARP, MIXER, GLOBAL, ...): the next sound, the steps stay",
                 snd_ok && n_snd >= 15u);
    bad += check("#94   SAVE held undoes each of those loads", undo_ok);
    ui_power_on();
    hold(B_HOME);
    i = ui.menu_sel;
    turn(EN_PRESET, 1);
    bad += check("#94 the menu: PRESETS does not move the selection (SELECT does)", ui.menu == 1 && ui.menu_sel == i);
    return bad;
}

/* 1.2 (Discussion #153): the pages by button. HOME: HOME > MIXER > CLOCK; SEQ: STEP <-> AUTOMATION, held SEQ TOOLS
 * everywhere; GLO: SONG (GLOBAL became CLOCK, SYSTEM the MENU's); SAVE: USER -> PRESETS -> PHRASES -> PROJECT -> TOOLS (1.5). AUTOMATION's list edits a step's
 * CHANCE / RATCH (the step's own fields, nothing else), one undo each; a remembered page that left its family */
static int steps_but(const track_t *t, const step_t *ref, uint32_t at, const step_t *want)   /* the steps = ref, at: want */
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++)
        if (memcmp(&t->step[i], i == at ? want : &ref[i], sizeof ref[i]))
            return 0;
    return 1;
}
static uint32_t auto_cur(void) { uint16_t rw[EV_ROWS]; return ev_cur(rw, 0); }
static int test_page_cycles(void)
{
    static const char *const SAVE_CYC[6] = {"USER", "PRESETS", "PHRASES", "PROJECT", "TOOLS", "USER"};   /* (1.5, #173) */
    int bad = 0, ok = 1;
    uint32_t i, n0;
    step_t ref[NSTEP], want;
    int16_t p0[P_COUNT];
    uint16_t rw[EV_ROWS];
    /* the cycles: each button's taps, in order */
    ui_power_on();
    for (i = 0; i < 6u; i++) {
        press(B_SAVE); frames(320);
        ok &= !at_home() && cur_page()->fam == FAM_SAVE && str_eq(cur_page()->title, SAVE_CYC[i]);
    }
    bad += check("1.5 (#173): SAVE cycles USER > PRESETS > PHRASES > PROJECT > TOOLS > USER", ok);
    ok = 1;
    for (i = 0; i < NPAGES; i++) {                       /* every page in a family its button reaches, and its LED */
        uint32_t f = PAGES[i].fam;
        ok &= f != FAM_SEQ || PAGES[i].graph == GR_ROLL || PAGES[i].graph == GR_EVENTS;
        ok &= f != FAM_GLO || str_eq(PAGES[i].title, "CLOCK");   /* (DUELL: GLO's only page; no SONG) */
        ok &= f != FAM_TRK || PAGES[i].graph == GR_TRK || str_eq(PAGES[i].title, "CLOCK");   /* HOME's: MIXER CLOCK */
        ok &= PAGES[i].graph != GR_TRK || f == FAM_TRK;
        ui_power_on(); ui.page = (uint8_t)i; page_entered();
        ok &= FAM_BTN[cur_fam()] == (f == FAM_TRK ? B_HOME : f == FAM_GLO ? B_GLO : f == FAM_SEQ ? B_SEQ :
                                    f == FAM_SAVE ? B_SAVE : FAM_BTN[f]);
    }
    for (i = 0; i < NPAGES; i++)
        ok &= !str_eq(PAGES[i].title, "PATTERN") && !str_eq(PAGES[i].title, "CHANCE") && !str_eq(PAGES[i].title, "AUTO LIST");
    bad += check("1.2: SEQ has STEP and AUTOMATION only; SONG is GLO's only page, PHRASES SAVE's, MIXER and CLOCK HOME's "
                 "(their LED HOME)", ok);
    {   /* 1.2: GLOBAL and SYSTEM are gone; their knobs: BPM SWG CLK on HOME > CLOCK, TUNE and ROUT on no page (the MENU's),
         * MIDI SYNC CPU on none (INFO shows the USB state and the CPU) */
        uint32_t k, clock = 0;
        ok = 1;
        for (i = 0; i < NPAGES; i++) {
            ok &= !str_eq(PAGES[i].title, "GLOBAL") && !str_eq(PAGES[i].title, "SYSTEM");
            for (k = 0; k < 4u && PAGES[i].scope == SC_GLOBAL; k++)
                ok &= PAGES[i].id[k] != G_TUNE && PAGES[i].id[k] != G_ROUTE;   /* (0.2.5: G_MIDI and G_SYNC carry the delay's modulation RATE and DEPTH, on DELAY 2/2; 0.2.16: G_INFO the delay MODE, on DELAY 1/2) */
            if (str_eq(PAGES[i].title, "CLOCK"))
                clock = i + 1u;
        }
        ok &= clock && PAGES[clock - 1u].fam == FAM_TRK && PAGES[clock - 1u].scope == SC_GLOBAL &&
              PAGES[clock - 1u].id[0] == G_BPM && PAGES[clock - 1u].id[1] == G_SWING && PAGES[clock - 1u].id[2] == G_CLOCK &&
              PAGES[clock - 1u].id[3] == 0xFFu;
        bad += check("1.2: no GLOBAL, no SYSTEM page; HOME > CLOCK has BPM SWG CLK (three knobs), TUNE and ROUT on no page", ok);
    }
    /* remembered pages: one that left its family, or out of range, gives the family's first (1.2: GLO's whatever it was,
     * GLOBAL and SYSTEM too, now SONG; HOME's never remembered: from HOME the MIXER) */
    ui_power_on();
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].graph == GR_TRK) ui.fam_last[FAM_GLO] = (uint8_t)i;
        else if (PAGES[i].graph == GR_PATS) ui.fam_last[FAM_SEQ] = (uint8_t)i;
    ui.fam_last[FAM_SAVE] = 200u;
    ui.fam_last[FAM_ENV] = 250u;
    open_family(FAM_GLO); ok = str_eq(cur_page()->title, "SONG");
    open_family(FAM_SEQ); ok &= cur_page()->graph == GR_ROLL;
    open_family(FAM_ENV); ok &= str_eq(cur_page()->title, "AMP ENV");
    open_family(FAM_SAVE); ok &= str_eq(cur_page()->title, "USER");
    open_family(FAM_GLO); open_family(FAM_GLO);          /* SONG again (its only page) */
    ok &= str_eq(cur_page()->title, "SONG");
    go_main_to(HP_MORPH); open_family(FAM_GLO); ok &= str_eq(cur_page()->title, "SONG");
    for (i = 0; i < 256u; i++) {                         /* any remembered index (the old GLOBAL 25, SYSTEM 27, ..) */
        go_main_to(HP_MORPH); ui.fam_last[FAM_GLO] = (uint8_t)i; open_family(FAM_GLO);
        ok &= str_eq(cur_page()->title, "SONG") && ui.fam_last[FAM_GLO] == ui.page;
        go_main_to(HP_MORPH); ui.fam_last[FAM_TRK] = (uint8_t)i; press(B_HOME);
        ok &= cur_page()->graph == GR_TRK && !at_home();
    }
    bad += check("1.2: a remembered page no longer in its family (or none) lands on the family's first; GLO always SONG, "
                 "HOME always the MIXER first", ok);

    /* AUTOMATION: + ADD CHANCE / RATCH, the knobs, EDIT: the step's fields only */
    ui_power_on();
    my_steps(TSEL);
    memcpy(ref, TSEL->step, sizeof ref);
    memcpy(p0, TSEL->p, sizeof p0);
    go_auto_top(); frame();
    n0 = ev_list(rw);
    ok = n0 == 3u && rw[0] == EVC(EVK_TOP, 0) && rw[1] == EVC(EVK_QNT, 0) && rw[2] == EVC(EVK_ADD, 0);
    go_auto_add(EV_CHANCE, 3); frame();
    press(B_OCTUP);
    want = ref[3]; step_set_chance(&want, 50);
    ok &= steps_but(TSEL, ref, 3, &want) && auto_cur() == EVC(EVK_CHANCE, 3) && msg_is("CHANCE ADDED") && !motion_count(TSEL);
    bad += check("AUTOMATION + ADD CHANCE: step 4's CHANCE 50 %, nothing else; its row selected", ok);
    turn(EN_K4, 10);
    want = ref[3]; step_set_chance(&want, 60);
    ok = steps_but(TSEL, ref, 3, &want);
    turn(EN_K2, 1);                                     /* to step 5 (at its default): step 4 back to 100 % */
    want = ref[4]; step_set_chance(&want, 60);
    ok &= steps_but(TSEL, ref, 4, &want) && auto_cur() == EVC(EVK_CHANCE, 4);
    bad += check("  KNOB 4 the CHANCE, KNOB 2 moves it to the next step (the old one back to 100 %)", ok);
    hold(B_SAVE);
    want = ref[3]; step_set_chance(&want, 60);
    ok = steps_but(TSEL, ref, 3, &want) && !memcmp(TSEL->p, p0, sizeof p0);
    hold(B_SAVE);
    want = ref[4]; step_set_chance(&want, 60);
    ok &= steps_but(TSEL, ref, 4, &want) && !memcmp(TSEL->p, p0, sizeof p0);
    bad += check("  SAVE held: the move undone (the sound and LEN untouched); held again: redone", ok);
    turn(EN_K4, 1000);                                  /* back at 100 %: the row stays while edited */
    ok = steps_but(TSEL, ref, 0xFFu, 0) && auto_cur() == EVC(EVK_CHANCE, 4);
    turn(EN_K1, -1);                                    /* (another row: it leaves the list) */
    ok &= auto_cur() == EVC(EVK_QNT, 0) && ev_list(rw) == 3u;
    bad += check("  at 100 % it stays listed while edited; another row picked: gone (nothing stored)", ok);
    memcpy(ref, TSEL->step, sizeof ref);
    go_auto_add(EV_RATCH, 6); frame();
    press(B_OCTUP);
    turn(EN_K4, 1); turn(EN_K4, 1); turn(EN_K4, 1);     /* x2 -> x4 (the end holds) */
    want = ref[6]; step_set_ratchet(&want, 4);
    ok = steps_but(TSEL, ref, 6, &want) && auto_cur() == EVC(EVK_RATCH, 6) && step_chance(&TSEL->step[6]) == 100u;
    hold(B_SAVE);                                       /* the turn (one undo): x2 again */
    want = ref[6]; step_set_ratchet(&want, 2);
    ok &= steps_but(TSEL, ref, 6, &want);
    hold(B_SAVE);
    press(B_EDIT);                                      /* EDIT: back to x1 */
    ok &= steps_but(TSEL, ref, 0xFFu, 0) && msg_is("DELETED");
    hold(B_SAVE);
    want = ref[6]; step_set_ratchet(&want, 4);
    ok &= steps_but(TSEL, ref, 6, &want);
    bad += check("AUTOMATION RATCH: + ADD x2, KNOB 4 to x4, EDIT x1; each one undo (SAVE held), the step's flags only", ok);
    {   /* the order: PLAY, QUANTIZE, then by step (CHANCE, RATCH, NUDGE, the records), + ADD; a lock on the same step
         * after them */
        go_auto_add(EV_CHANCE, 6); frame(); press(B_OCTUP);
        go_auto_add(EV_NUDGE, 6); frame(); press(B_OCTUP);
        motion_set_lock(TSEL, 6, P_ATK, 3);
        motion_set_lock(TSEL, 2, P_ATK, 5);
        n0 = ev_list(rw);
        ok = n0 == 8u && rw[0] == EVC(EVK_TOP, 0) && rw[1] == EVC(EVK_QNT, 0) && rw[2] >> 8 == EVK_REC &&
             rw[3] == EVC(EVK_CHANCE, 6) && rw[4] == EVC(EVK_RATCH, 6) && rw[5] == EVC(EVK_NUDGE, 6) &&
             rw[6] >> 8 == EVK_REC && rw[7] == EVC(EVK_ADD, 0);
        go_auto_add(EV_CHANCE, 6); frame(); press(B_OCTUP);
        ok &= msg_is("ALREADY LISTED") && auto_cur() == EVC(EVK_CHANCE, 6);
        bad += check("  the rows: PLAY, QUANTIZE, by step (CHANCE, RATCH, NUDGE, then its locks / events), + ADD; one there: "
                     "ALREADY LISTED", ok);
        auto_pick(EVC(EVK_NUDGE, 6));
        press(B_EDIT);                                  /* (the NUDGE out again: the rest as before) */
        ok = step_nudge(&TSEL->step[6]) == 0;
        bad += check("  EDIT on the NUDGE row: back to 0", ok);
    }
    return bad;
}

static int led_of(const uint8_t *l, uint32_t id);
static int test_product_ux(void)
{
    int bad = 0, ok = 1;
    uint32_t i, p, b;
    for (i = 0; i < NPAGES; i++) {
        ui_power_on(); set_engine_of(TSEL, 1);         /* (DIGITAL, retired: ANALOG; no OP pages) */
        ui.page = (uint8_t)i; page_entered();
        if (!page_visible(i))
            continue;
        press(B_REC);
        if (PAGES[i].graph == GR_ROLL)            /* #133: STEP's roll arms step recording, stopped */
            ok &= song.rec == 1 && !transport_req && ui.page == i && !at_home();
        else ok &= song.rec == 1 && transport_req == 1 && ui.page == i && !at_home();
    }
    bad += check("REC stays on every page, SONG stopped asks for pattern recording (STEP: step recording, stopped)", ok);
    ui_power_on(); hold(B_HOME); press(B_REC);
    bad += check("REC in the MENU does nothing (the menu stays, no arm, no transport start)",
                 ui.menu == 1 && song.rec == 0 && !transport_req);
    ui.menu_sel = MI_ABOUT; press(B_OCTUP); press(B_REC);
    bad += check("REC in ABOUT does nothing (the document and its scroll stay)",
                 ui.menu == 2 && !ui.menu_scroll && song.rec == 0 && !transport_req);
    ui_power_on(); press(B_HOME); press(B_HOME);   /* (DUELL step 5: MODE first, then the MIXER) */
    bad += check("1.2: HOME tapped on HOME twice opens the MIXER", cur_page()->graph == GR_TRK && !at_home() &&
                 cur_fam() == FAM_TRK && FAM_BTN[cur_fam()] == B_HOME);
    int16_t len = TSEL->p[P_SLEN], send = TSEL->p[P_REV];
    perf_space = 0;
    turn(EN_K3, 1);
    bad += check("MIXER K3 is SPACE (the global value, PERFORM's K2), never sequence length nor the REV send",
                 TSEL->p[P_SLEN] == len && TSEL->p[P_REV] == send && perf_space == 1);
    perf_space = 0;
    press(B_HOME);
    ok = !at_home() && !ui.menu && str_eq(cur_page()->title, "CLOCK") && cur_fam() == FAM_TRK && FAM_BTN[cur_fam()] == B_HOME;
    {   /* (CLOCK: HOME's LED lit, as on MIXER; GLO's not) */
        uint8_t pos0[sizeof led_pos];
        ui_leds();                                      /* (its positions made once, then ours) */
        memcpy(pos0, led_pos, sizeof pos0);
        memset(led_pos, 0xFF, sizeof led_pos);
        led_pos[panel.btn[B_HOME]] = 0u << 3 | 1u;
        led_pos[panel.btn[B_GLO]] = 0u << 3 | 2u;
        frame(); ui_leds();
        ok &= led_of(fm1_led, panel.btn[B_HOME]) && !led_of(fm1_led, panel.btn[B_GLO]);
        memcpy(led_pos, pos0, sizeof pos0);
    }
    {   /* CLOCK's knobs: BPM SWG CLK as GLO > GLOBAL's were; KNOB 4 nothing */
        int16_t bpm = song.g[G_BPM], swg = song.g[G_SWING], clk = song.g[G_CLOCK], tune = song.g[G_TUNE];
        turn(EN_K1, 1); turn(EN_K2, 1); turn(EN_K3, 1); turn(EN_K4, 5);
        ok &= song.g[G_BPM] == bpm + 1 && song.g[G_SWING] == swg + 1 && song.g[G_CLOCK] != clk && song.g[G_TUNE] == tune;
        song.g[G_CLOCK] = clk;
    }
    press(B_HOME);
    ok &= at_home() && !ui.menu;
    press(B_HOME);
    ok &= cur_page()->graph == GR_TRK;
    go_title("AMP ENV"); press(B_HOME);
    ok &= at_home();
    go_title("CLOCK"); press(B_HOME);
    ok &= at_home();
    bad += check("1.2: HOME cycles HOME > MIXER > CLOCK > HOME (CLOCK: BPM SWG CLK, HOME's LED); from any other page HOME", ok);
    hold(B_HOME);
    bad += check("1.2: HOME held still opens the menu", ui.menu == 1);
    ui_power_on();
    frames(320);                                       /* (taps further apart than a double tap: #83) */
    press(B_GLO); frames(320); ok = cur_page()->fam == FAM_GLO && str_eq(cur_page()->title, "SONG");
    press(B_GLO); frames(320); ok &= str_eq(cur_page()->title, "SONG") && cur_page()->fam == FAM_GLO && !at_home();
    go_title("CLOCK"); press(B_GLO); frames(320); ok &= str_eq(cur_page()->title, "SONG");
    bad += check("1.2: GLO opens SONG directly, again SONG (no GLOBAL, no SYSTEM, no MIXER)", ok);
    go_main_to(HP_MORPH); btn_down(B_SEQ); frames(500);
    ok = ui.layer == LAYER_SEQ && at_home();
    btn_up(B_SEQ); frame();
    bad += check("1.2: SEQ held on HOME is SEQ TOOLS (no SONG, no tap on release)", ok && at_home() && !ui.layer);
    ui_power_on(); open_family(FAM_EDIT);
    ok = 1;
    for (i = 0; i < 8u; i++) { open_family(FAM_EDIT); ok &= page_visible(ui.page) && !(cur_page()->fam == FAM_EDIT && ui_id_retired(cur_page()->id[0])); }
    bad += check("operator envelope pages are hidden on non-DIGITAL instruments", ok);
    set_engine_of(TSEL, 1); open_family(FAM_EDIT); ok = TSEL->eng_req == 0u;
    for (i = 0; i < 12u; i++) { ok &= !(cur_page()->fam == FAM_EDIT && ui_id_retired(cur_page()->id[0])); open_family(FAM_EDIT); }
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == FAM_EDIT && ui_id_retired(PAGES[i].id[0]))
            ok &= !page_visible(i);
    bad += check("DIGITAL retired: engine 1 asked for loads ANALOG; the OP ENV / OP LEVEL pages never show", ok);
    ui_power_on(); go_auto_add(EV_CHANCE, 0); press(B_OCTUP); turn(EN_K4, 15);
    bad += check("CHANCE is per step and starts at backward-compatible 100 percent", step_chance(&TSEL->step[0]) == 65 && step_chance(&TSEL->step[1]) == 100);
    turn(EN_K4, -1000); ok = step_chance(&TSEL->step[0]) == 0;
    turn(EN_K4, 1000); ok &= step_chance(&TSEL->step[0]) == 100;
    bad += check("CHANCE controls clamp to 0..100 without changing adjacent steps", ok);
    ui_power_on(); song.rec = 1; seq_start(); go_main_to(HP_MORPH);
    int16_t baseline = TSEL->p[P_E0]; turn(EN_K1, 1);
    bad += check("REC plus a sound knob records motion through the UI", motion_count(TSEL) == 1 && motion_enabled(TSEL));
    press(B_PLAY); events_block(32);
    bad += check("stopping returns the original sound after a recorded knob gesture", TSEL->p[P_E0] == baseline);
    go_auto_top(); turn(EN_K4, -1);
    bad += check("#93 the page is AUTOMATION (GR_EVENTS inside)", str_eq(cur_page()->title, "AUTOMATION"));
    bad += check("MOTION playback OFF preserves its stored events", !motion_enabled(TSEL) && motion_count(TSEL) == 1);
    press(B_OCTUP);
    bad += check("MOTION clear always asks confirmation", ui.confirm == CF_CLEAR_MOTION && motion_count(TSEL) == 1);
    {
        char qa[24], qb[24];
        confirm_text(qa, qb);
        bad += check("#93   the dialog asks CLEAR T1 AUTOMATION?", str_eq(qa, "CLEAR T1 AUTOMATION?"));
    }
    press(B_OCTDN); press(B_OCTUP); press(B_OCTUP);
    bad += check("confirmed MOTION clear removes events and SAVE hold restores them", motion_count(TSEL) == 0 &&
                 msg_is("AUTOMATION CLEARED"));
    hold(B_SAVE);
    bad += check("motion-clear UNDO restores the recorded events", motion_count(TSEL) == 1);
    motion_full = 1; frame();
    bad += check("#93 the events used up: AUTOMATION FULL", msg_is("AUTOMATION FULL") && !motion_full);
    {   /* #63: a card shows MOTION's icon while the track's motion changes its parameter */
        uint32_t id = ENGINES[TSEL->eng_req]->knob[0];
        ui_power_on(); go_main_to(HP_MORPH); frame();
        ok = !card_motion(0) && !card_motion(1);
        song.rec = 1; seq_start(); go_main_to(HP_MORPH); turn(EN_K1, 1); frame();
        ok &= card_motion(0) && !card_motion(1) && !card_motion(2) && !card_motion(3);
        bad += check("MOTION: a recorded knob marks its HOME card (and no other)", ok && motion_count(TSEL) == 1);
        song.rec = 0; press(B_PLAY); events_block(32); frame();
        ok = card_motion(0);
        go_auto_top(); turn(EN_K4, -1); go_main_to(HP_MORPH); frame();
        ok &= !card_motion(0) && motion_count(TSEL) == 1;
        bad += check("MOTION: PLAY OFF takes the icon off the card, the events stay", ok);
        go_auto_top(); turn(EN_K4, 1); go_main_to(HP_MORPH); frame();
        ok = card_motion(0);
        track_select(1); frame();
        ok &= !card_motion(0);                           /* (another track: its own motion, none) */
        track_select(0); frame();
        ok &= card_motion(0);
        bad += check("MOTION: PLAY ON again shows it; only on the track that has the motion", ok);
        go_page(GR_TRK); frame();
        ok = !card_motion(0) && !card_motion(1) && !card_motion(2);
        motion_set_event(TSEL, 3, P_PAN, 20); frame();
        ok &= card_motion(1) && !card_motion(0) && !card_motion(2) && !card_motion(3);
        bad += check("MOTION: a mix parameter's event marks its MIXER card (PAN)", ok);
        go_auto_top(); press(B_OCTUP); press(B_OCTUP);
        ok = motion_count(TSEL) == 0;
        go_main_to(HP_MORPH); frame();
        ok &= !card_motion(0) && !card_motion(1);
        go_page(GR_TRK); frame();
        ok &= !card_motion(1);
        bad += check("MOTION: CLEAR takes the icon off every card", ok && id < P_COUNT);
    }
    ui_power_on(); ui_leds();
    memset(led_pos, 0xFF, sizeof led_pos);
    led_pos[panel.btn[B_PLAY]] = 1; led_pos[panel.btn[B_REC]] = 2;
    song.playing = song.rec = 1; ok = 1;
    for (i = 0; i < 120u; i++) { fm1_ms += 17; ui_leds(); ok &= (fm1_led[0] & 6u) == 4u && (fm1_led[8] & 2u); }
    bad += check("PLAY's green and REC LEDs stay lit across time and audio block phase", ok);
    {   /* the power-on sweep (hal/fm1_input.h fm1_led_anim_on) has the LEDs: the UI writes none of them while it runs,
         * all of its own picture on the next frame after */
        uint8_t l0[FM1_NCOL], d0[FM1_NCOL];
        memset(fm1_led, 0x5A, sizeof fm1_led);
        memset(fm1_led_dim, 0xA5, sizeof fm1_led_dim);
        memcpy(l0, fm1_led, sizeof l0);
        memcpy(d0, fm1_led_dim, sizeof d0);
        host_anim = 1;
        ui_leds();
        ok = !memcmp(l0, fm1_led, sizeof l0) && !memcmp(d0, fm1_led_dim, sizeof d0);
        host_anim = 0;
        ui_leds();
        ok &= (fm1_led[0] & 6u) == 4u && (fm1_led[8] & 2u) && fm1_led[1] != 0x5A;
        bad += check("the power-on LED sweep: the UI leaves the LEDs alone while it runs, takes them after", ok);
    }
    led_pos_init();
    ok = 1;
    for (p = 0; p < NPALETTES; p++) for (b = 0; b < 1u; b++) {
        uint16_t graph[240 * H_GRAPH], columns[240 * (Y_SEP_END - Y_LABEL)];
        ui_power_on(); palette_set(p); settings.zoom = 1;
        open_family(FAM_ENV); frame();
        memcpy(graph, host_screen + Y_GRAPH * 240, sizeof graph);
        memcpy(columns, host_screen + Y_LABEL * 240, sizeof columns);
        ui.hot_col = 1; ui.hot_t = 40; ui.force = 1; ui_draw();
        ok &= !memcmp(graph, host_screen + Y_GRAPH * 240, sizeof graph);
        int changed = 0;
        for (uint32_t y = 0; y < Y_SEP_END - Y_LABEL; y++) for (uint32_t x = 0; x < 240; x++) {
            uint16_t old = columns[y * 240 + x], now = host_screen[(Y_LABEL + y) * 240 + x];
            if (x >= 64 && x < 119) changed |= old != now;
            else ok &= old == now;
        }
        ok &= changed;
    }
    bad += check("all palettes: active column is subtle, stable-size, no zoom over graph", ok);
    ui_power_on(); return bad;
}

static int test_panel(void)
{
    int bad = 0;
    panel = PANEL_DEFAULT;
    panel.btn[B_PLAY] = panel.btn[B_REC];
    panel_init();
    bad += check("duplicate button mappings recover to the default panel", !memcmp(&panel, &PANEL_DEFAULT, sizeof panel));
    panel.enc[EN_K1] = panel.enc[EN_K2];
    panel_init();
    bad += check("duplicate knob mappings recover to the default panel", !memcmp(&panel, &PANEL_DEFAULT, sizeof panel));
    panel.btn[B_PLAY] = 255;
    panel_init();
    bad += check("out-of-range panel data recovers without an invalid shift", !memcmp(&panel, &PANEL_DEFAULT, sizeof panel));
    panel.dir[EN_K1] = -1;
    panel_init();
    bad += check("a valid reversed knob mapping is retained", panel.dir[EN_K1] == -1);
    return bad;
}

/* ------------------------------------------------- the FX hold layer --- */
static void key_down(uint32_t k) { fm1_in.notes |= 1u << k; host_notes |= 1u << k; keyboard_block(); }
static void key_up(uint32_t k) { fm1_in.notes &= ~(1u << k); keyboard_block(); }
static uint32_t gates(void)                        /* voices of every track with their key down */
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NTRK; p++)
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].gate != 0;
    return n;
}
static uint32_t white(uint32_t w) { return key_at(0, w); }

/* a layer's key LEDs as the old 250 ms blink read them: lit, and the breathing ones (#119) "on" at even quarters,
 * so (a ^ b) over ms 0 and 250 = the ones that can be pressed, (a & b) = lit */
static uint32_t leds_at(uint32_t ms)
{
    uint32_t br, m;
    fm1_ms = ms;
    m = layer_leds(&br);
    return m | (((ms / 250u) & 1u) ? 0u : br);
}

static int test_layer(void)
{
    int bad = 0, ok, flash;
    uint32_t i, mo, k;
    /* tap: on release, the page; held briefly: no map ever */
    ui_power_on();
    btn_down(B_FX); frame();
    ok = at_home();
    frames(64); btn_up(B_FX); frame();
    bad += check("FX tap: nothing on press, the FX page when let go", ok && !at_home() && cur_page()->fam == FAM_FX);
    frames(320);                                       /* (not a double tap: #83) */
    press(B_FX);
    bad += check("  a second tap: the next page of the family (INSERT 1)", str_eq(cur_page()->title, "INSERT 1/2"));
    ui_power_on();
    btn_down(B_FX);
    for (flash = 0, i = 0; i < 22u; i++) { frame(); flash |= ui.layer; }      /* 0.35 s */
    btn_up(B_FX); frame();
    bad += check("  held 0.35 s (HOLD 0.4): still a tap, the map never shown", !flash && !at_home());
    /* DUELL 0.2.2: no FX hold layer any more: FX held past HOLD shows no map; let go, it is a tap, the FX page */
    ui_power_on();
    btn_down(B_FX); frames(432);
    ok = !ui.layer && at_home();
    btn_up(B_FX); frame();
    bad += check("FX held past 0.4 s (0.2.2: no hold layer): no map; let go: the FX page, as a tap",
                 ok && !ui.layer && !at_home() && cur_page()->fam == FAM_FX);
    /* the HOLD setting: 0.6 s */
    ui_power_on();
    hold(B_HOME); ui.menu_sel = MI_HOLD; turn(EN_K1, 1); turn(EN_K1, 1); ok = settings_hold == 3u; hold(B_HOME);
    btn_down(B_SCL); frames(512); flash = ui.layer; btn_up(B_SCL); frame();
    bad += check("menu HOLD 0.6 s (KNOB 1): SCL held 0.5 s is a tap, no map", ok && !flash && !at_home() && !ui.menu);
    {
        persist_t p = {0};
        p.magic = PERSIST_MAGIC; p.panel = panel;
        settings_export(&p); settings_hold = HOLD_DEF;
        bad += check("  HOLD is saved with the settings and read back", settings_import(&p, sizeof p) && settings_hold == 3u);
    }
    /* the LEDS setting: KNOB 1 / OCT+ up to MAX, left / OCT- down to OFF; saved in the retired zoom field */
    ui_power_on();
    hold(B_HOME); ui.menu_sel = MI_LEDS;
    ok = settings_leds == LEDS_DIM;
    turn(EN_K1, 1); ok &= settings_leds == LEDS_INV;
    turn(EN_K1, 1); ok &= settings_leds == LEDS_INV;
    turn(EN_K1, -1); ok &= settings_leds == LEDS_DIM;
    turn(EN_K1, -1); ok &= settings_leds == LEDS_DIM_LO;
    turn(EN_K1, -1); ok &= settings_leds == LEDS_OFF;
    turn(EN_K1, -1); ok &= settings_leds == LEDS_OFF;
    press(B_OCTUP); ok &= settings_leds == LEDS_DIM_LO && ui.menu == 1u;
    press(B_OCTUP); ok &= settings_leds == LEDS_DIM;
    press(B_OCTUP); ok &= settings_leds == LEDS_INV;
    press(B_OCTUP); ok &= settings_leds == LEDS_INV && ui.menu == 1u;   /* (stops at the end) */
    for (k = 0; k < 4u; k++) press(B_OCTDN);
    ok &= settings_leds == LEDS_OFF && ui.menu == 1u;
    ok &= !strcmp(LEDS_NAME[LEDS_OFF], "OFF") && !strcmp(LEDS_NAME[LEDS_DIM_LO], "LOW") &&
          !strcmp(LEDS_NAME[LEDS_DIM], "MEDIUM") && !strcmp(LEDS_NAME[LEDS_INV], "MAX");
    {   /* a setting changed from elsewhere (the editor's MENU_SET: menu_put) while the MENU is shown: redrawn */
        static uint16_t before[240 * 240];
        frame();
        memcpy(before, host_screen, sizeof before);
        menu_put(MI_LEDS, 3);                          /* (OFF -> MAX, the row shown) */
        frame();
        ok &= settings_leds == LEDS_INV && memcmp(before, host_screen, sizeof before) && menu_get(MI_LEDS) == 3u;
        menu_put(MI_LEDS, 0);
        frame();
        ok &= !memcmp(before, host_screen, sizeof before);
    }
    hold(B_HOME);
    bad += check("menu LEDS: MEDIUM by default; KNOB 1 OFF < LOW < MEDIUM < MAX (stops at the ends), OCT+ / OCT- the same; "
                 "a change from the editor redraws the shown MENU", ok && !ui.menu);
    {
        persist_t p = {0};
        uint32_t m;
        p.magic = PERSIST_MAGIC; p.panel = panel;
        ok = 1;
        settings.zoom = 0;                              /* (MEDIUM keeps an older record's 0 / 1 as saved) */
        for (m = 0; m < LEDS_COUNT; m++) {
            settings_leds = (uint8_t)m;
            settings_export(&p); settings_leds = LEDS_DIM;
            ok &= settings_import(&p, sizeof p) && settings_leds == m && leds_stored_ok(p.zoom) &&
                  p.zoom == (m == LEDS_DIM ? 0u : LEDS_TAG | m);
            settings_export(&p);                    /* (idempotent: saved again, the same field) */
            ok &= p.zoom == (m == LEDS_DIM ? 0u : LEDS_TAG | m);
        }
        settings_leds = LEDS_DIM;
        settings_export(&p);
        bad += check("  LEDS is saved with the settings and read back (all four); MEDIUM saves the field as before (0)",
                     ok && p.zoom == 0u && settings_import(&p, sizeof p) && settings_leds == LEDS_DIM);
    }
    /* DUELL 0.2.2: FX is no layer: a key pressed with FX held is a note */
    ui_power_on();
    usb.config = 1;
    btn_down(B_FX); key_down(white(2)); frame();
    ok = !ui.layer && gates() > 0 && !((kb_layer >> white(2)) & 1u);
    key_up(white(2)); btn_up(B_FX); frame();
    bad += check("FX held + a key (0.2.2: no FX layer): the key plays its note, no map; FX let go: the FX page",
                 ok && !ui.layer && !kb_layer && !at_home() && cur_page()->fam == FAM_FX);
    /* REVERB > TYPE (FX family, global): ROOM / SPRING / HALL on KNOB 1 (HALL at power-on since 1.2), kept by a
     * project */
    ui_power_on();
    go_title("REVERB");
    ok = cur_page()->fam == FAM_FX && cur_page()->id[0] == G_RTYPE && cur_page()->id[1] == G_RSIZE &&
         cur_page()->id[2] == G_RDAMP && song.g[G_RTYPE] == 2 && str_eq(GP[G_RTYPE].names[2], "HALL");
    turn(EN_K1, -2);
    ok &= song.g[G_RTYPE] == 0 && str_eq(GP[G_RTYPE].names[song.g[G_RTYPE]], "ROOM");
    turn(EN_K1, 1);
    ok &= song.g[G_RTYPE] == 1 && str_eq(GP[G_RTYPE].names[song.g[G_RTYPE]], "SPRING");
    turn(EN_K1, 5);
    ok &= song.g[G_RTYPE] == 2;
    turn(EN_K1, -1);
    ok &= song.g[G_RTYPE] == 1;
    song.playing = 0;
    project_save(2);
    turn(EN_K1, -1);
    ok &= song.g[G_RTYPE] == 0;
    project_load(2);
    bad += check("REVERB page: TYPE HALL (power-on) / ROOM / SPRING on KNOB 1 (SIZE, DAMP beside it); a project keeps SPRING",
                 ok && song.g[G_RTYPE] == 1);
    go_title("CHORUS");
    bad += check("  CHORUS page: CRT CDP", cur_page()->fam == FAM_FX && cur_page()->id[0] == G_CRATE &&
                 cur_page()->id[1] == G_CDEPTH && cur_page()->id[2] == 0xFFu);
    /* chording: a key held before FX stays a note, its note-off arrives */
    ui_power_on();
    mo = mo_w;
    key_down(white(4)); frame(); btn_down(B_FX); frame(); key_up(white(4)); frame(); btn_up(B_FX); frame();
    for (i = 0; i < 4u; i++) events_block(CTL);
    bad += check("a key held before FX stays a note: note-on, note-off, nothing left held", mo_w == mo + 2u && !gates() &&
                 !kb_layer);
    /* buttons in the layer: PLAY works, SAVE, HOME, a page button are swallowed */
    ui_power_on();
    btn_down(B_SCL); frame();
    press(B_PLAY);
    ok = transport_req == 1u && ui.layer == LAYER_SCL;
    transport_req = 0;
    press(B_SAVE); ok &= at_home();
    hold(B_SAVE); ok &= !msg_is("UNDO/REDO T1") && at_home();
    press(B_ENV); ok &= at_home();
    hold(B_HOME); ok &= !ui.menu && at_home();
    btn_up(B_SCL); frame();
    bad += check("in the layer: PLAY plays; SAVE (tap and hold), ENV, HOME (hold) do nothing", ok && at_home() && !ui.menu);
    btn_down(B_SCL); frame(); btn_down(B_ENV); frame(); btn_up(B_SCL); frame(); btn_up(B_ENV); frame();
    bad += check("  a page button pressed with FX, let go after it: still nothing", at_home());
    /* release-act: every page button acts when let go */
    {
        static const uint8_t PB[7] = {B_ENV, B_LFO, B_SCL, B_ARP, B_GLO, B_EDIT, B_FX};
        static const uint8_t PF[7] = {FAM_ENV, FAM_LFO, FAM_SCL, FAM_ARP, FAM_GLO, FAM_EDIT, FAM_FX};
        ok = 1;
        for (i = 0; i < 7u; i++) {
            ui_power_on();
            btn_down(PB[i]); frame();
            ok &= at_home();
            btn_up(PB[i]); frame();
            ok &= !at_home() && cur_page()->fam == PF[i];
            if (PB[i] == B_SCL || PB[i] == B_GLO || PB[i] == B_EDIT)
                continue;                             /* (a layer of their own: held long is a peek) */
            ui_power_on();
            hold(PB[i]);                              /* (no layer of their own: a long press is a tap too) */
            ok &= !at_home() && cur_page()->fam == PF[i];
        }
        bad += check("ENV LFO SCL ARP GLO EDIT FX act when let go, not on press (ENV LFO ARP FX held long: the same)", ok);
    }
    ui_power_on();
    go_page(GR_ROLL); my_steps(TSEL); ui.cursor = 0;
    btn_down(B_EDIT); frame();
    ok = step_on(&TSEL->step[0]) && ui.cursor == 0;
    btn_up(B_EDIT); frame();
    bad += check("EDIT on STEP clears the step when let go (not on press)", ok && !step_on(&TSEL->step[0]) && ui.cursor == 1);
    ui_power_on(); hold(B_HOME);
    ok = ui.menu == 1; hold(B_HOME); ok &= !ui.menu;
    go_main_to(HP_MORPH); hold(B_SEQ); ok &= at_home() && !ui.layer;
    bad += check("HOME (menu) hold of 0.7 s unchanged; SEQ held: its layer, no page (1.2)", ok);
    /* no layer in the menu or a dialog: SCL + a key is a note */
    ui_power_on(); hold(B_HOME);
    btn_down(B_SCL); frame(); key_down(white(3)); frame();
    ok = !ui.layer && !kb_mask && gates() > 0 && !kb_layer;
    key_up(white(3)); btn_up(B_SCL); frame();
    bad += check("in the menu: no layer, SCL + a key plays the key; SCL let go: no page", ok && ui.menu == 1);
    ui_power_on(); ui.confirm = CF_CLEAR_SEQ;
    btn_down(B_SCL); frames(512);
    ok = !ui.layer;
    btn_up(B_SCL); frame();
    bad += check("in a dialog: SCL held shows no map and taps nothing", ok && ui.confirm == CF_CLEAR_SEQ);
    /* UBOOT: the countdown closes the map; OCT- + OCT+ are still seen */
    ui_power_on();
    btn_down(B_SCL); key_down(white(5)); frame();
    btn_down(B_OCTDN); btn_down(B_OCTUP); frame();
    ok = ui.layer == LAYER_SCL && (fm1_in.buttons & 3u << panel.btn[B_OCTDN]) != 0u;
    ui.uboot = 3; frame();                          /* (main.c: OCT- + OCT+ held 2 s) */
    ok &= !ui.layer;
    ok &= (fm1_in.buttons & ((1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]))) ==
          ((1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]));
    ui.uboot = 0;
    btn_up(B_OCTDN); btn_up(B_OCTUP); key_up(white(5)); btn_up(B_SCL); frame();
    bad += check("UBOOT countdown over the layer: the map closes; OCT- + OCT+ untouched", ok && !kb_layer);
    usb.config = 0;
    return bad;
}

/* ------------------------------------------------------------ NAME --- */
static void nm_tap(uint32_t k)                    /* a key tapped in NAME: down a frame, up a frame (32 ms) */
{
    fm1_in.notes |= 1u << k; host_notes |= 1u << k; frame();
    fm1_in.notes &= ~(1u << k); frame();
}
static uint32_t nm_black_key(uint32_t f, uint32_t octave)   /* the black key of function f (NB_*: F# G# A# C# D#), octave 0 / 1 */
{
    return key_at(1, f + 5u * octave);
}
static int test_name(void)
{
    int bad = 0, ok;
    uint32_t i, mo;
    char b[16];
    ui_power_on();
    go_page(GR_USER);
    ui_old_uslot = 2;
    press(B_OCTUP);                               /* (SAVE picked on entry) */
    up_auto_name(b, TSEL->eng_req, 2);
    bad += check("NAME: USER SAVE into an empty slot opens NAME, prefilled with a random name",
                 name_on() && nm.kind == NK_USER_SAVE && nm.slot == 2u && nm.len >= 1u && nm.len <= NM_LEN &&
                 !str_eq(nm.s, b) && nm.cur == nm.len);
    for (i = 0; i < 12u; i++) nm_tap(nm_black_key(NB_DEL, 0));
    bad += check("  DELETE removes the character before the cursor, down to empty", nm.len == 0u && nm.cur == 0u && !nm.s[0]);
    /* multi-tap: the same key cycles, another key commits, 0.8 s commits */
    nm_tap(white(0));
    ok = nm.len == 1u && nm.s[0] == 'A' && nm.key == 1u && nm.cur == 0u;
    nm_tap(white(0));
    ok &= nm.s[0] == 'B' && nm.len == 1u;
    nm_tap(white(0));
    bad += check("  a white key types its first letter; tapped again within 0.8 s: the next one, cycling (A B A)",
                 ok && nm.s[0] == 'A' && nm.len == 1u && nm.cur == 0u);
    nm_tap(white(1));
    bad += check("  another key keeps the letter and types its own (A, C)", nm.len == 2u && str_eq(nm.s, "AC") && nm.cur == 1u &&
                 nm.key == 2u);
    frames(800);
    bad += check("  0.8 s without a tap keeps it: the cursor moves on", nm.key == 0u && nm.cur == 2u && str_eq(nm.s, "AC"));
    nm_tap(white(1));
    frames(400);
    nm_tap(white(1));
    bad += check("  a second tap within 0.8 s (0.43 s later) still cycles (C -> D)", str_eq(nm.s, "ACD") && nm.key == 2u);
    frames(800);
    nm_tap(white(4)); nm_tap(white(4)); nm_tap(white(4));   /* IJK: K */
    nm_tap(nm_black_key(NB_SPACE, 1));                       /* SPACE (the upper octave's G#) keeps K first */
    bad += check("  a three-letter key (IJK) cycles to K; SPACE keeps it and adds a space", str_eq(nm.s, "ACDK ") && nm.cur == 5u &&
                 !nm.key);
    nm_tap(nm_black_key(NB_DEL, 1));
    nm_tap(nm_black_key(NB_LEFT, 0));
    nm_tap(nm_black_key(NB_LEFT, 0));
    nm_tap(white(2));                             /* E inserted before D */
    frames(800);
    bad += check("  DELETE, cursor left twice, a letter inserted at the cursor", str_eq(nm.s, "ACEDK") && nm.cur == 3u);
    nm_tap(nm_black_key(NB_RIGHT, 1));
    nm_tap(nm_black_key(NB_RIGHT, 0));
    nm_tap(nm_black_key(NB_RIGHT, 0));
    bad += check("  cursor right stops at the end", nm.cur == 5u);
    /* 123 */
    nm_tap(nm_black_key(NB_MODE, 0));
    nm_tap(white(1)); nm_tap(white(9)); nm_tap(white(15));
    bad += check("  D# switches to 123: one tap, one character (2, 0, +), no cycling", nm.num && str_eq(nm.s, "ACEDK20+") &&
                 nm.cur == 8u && !nm.key);
    nm_tap(nm_black_key(NB_MODE, 1));
    nm_tap(white(15)); nm_tap(white(15)); nm_tap(white(15));
    frames(800);
    bad += check("  back to ABC: the last key (0-.) gives 0, then -, then .", !nm.num && str_eq(nm.s, "ACEDK20+.") && nm.cur == 9u);
    /* the knobs */
    turn(EN_K1, -2);
    turn(EN_K2, 1);
    bad += check("  KNOB 1 moves the cursor, KNOB 2 changes the character there (+ -> SPACE, wrapping)",
                 nm.cur == 7u && str_eq(nm.s, "ACEDK20 ."));
    turn(EN_K2, 5);
    bad += check("  KNOB 2 on: SPACE + 5 = E", nm.s[7] == 'E');
    turn(EN_K1, 9);
    turn(EN_K2, 1);
    bad += check("  KNOB 1 to the end, KNOB 2 adds a character there (A)", str_eq(nm.s, "ACEDK20E.A") && nm.cur == 9u && nm.len == 10u);
    turn(EN_K2, -2);
    bad += check("  .. and turns it back past SPACE to the last symbol (+)", nm.s[9] == '+');
    /* full */
    nm_tap(nm_black_key(NB_RIGHT, 0));
    nm_tap(white(7)); frames(800); nm_tap(white(7)); frames(800);
    ui.msg_t = 0;
    nm_tap(white(7));
    bad += check("  12 characters at most: NAME FULL, nothing typed", nm.len == 12u && msg_is("NAME FULL") && str_eq(nm.s, "ACEDK20E.+PP"));
    /* held DELETE repeats */
    fm1_in.notes |= 1u << nm_black_key(NB_DEL, 0); host_notes |= 1u << nm_black_key(NB_DEL, 0);
    frames(16 * 40);
    fm1_in.notes &= ~(1u << nm_black_key(NB_DEL, 0)); frame();
    bad += check("  DELETE held 0.64 s: one, then it repeats after 0.45 s, every 0.09 s (3 in all)", nm.len == 9u);
    /* the keys never sound, record or send MIDI */
    usb.config = 1; mo = mo_w; song.rec = 1;
    {
        step_t before[NSTEP];
        memcpy(before, TSEL->step, sizeof before);
        key_down(white(3)); key_down(nm_black_key(NB_SPACE, 0)); frame();
        for (i = 0; i < 8u; i++) events_block(CTL);
        ok = !gates() && mo_w == mo && !memcmp(before, TSEL->step, sizeof before) && song.grid == 2u;
        key_up(white(3)); key_up(nm_black_key(NB_SPACE, 0)); frame();
        bad += check("  keys in NAME: no voice, no MIDI, nothing recorded (seq.c: song.grid 2)", ok && mo_w == mo);
    }
    song.rec = 0; usb.config = 0;
    {   /* LEDs: all 27 keys do something; the cycling key blinks */
        uint32_t a, c, t0;
        nm_tap(nm_black_key(NB_DEL, 0)); nm_tap(nm_black_key(NB_DEL, 0));
        nm_tap(white(5));
        t0 = (fm1_ms / 500u + 1u) * 500u;
        fm1_ms = t0; a = name_leds(); fm1_ms = t0 + 250u; c = name_leds();
        nm.t = fm1_ms;                            /* (the letter still cycling) */
        bad += check("  LEDs: every key lit, the cycling key (LM) blinks", ((a | c) == 0x7FFFFFFu) && ((a ^ c) == (1u << white(5))));
    }
    /* cancel */
    {
        uint32_t page = ui.page;
        press(B_OCTDN);
        bad += check("  OCT- cancels: nothing written, back on the USER page", !name_on() && !up_used(2) && ui.page == page && !at_home());
    }
    /* typing a name and saving it */
    ui.act = 4;
    press(B_OCTUP);
    for (i = 0; i < 12u; i++) nm_tap(nm_black_key(NB_DEL, 0));
    nm_tap(white(1)); nm_tap(white(0)); nm_tap(white(0)); nm_tap(nm_black_key(NB_RIGHT, 0));    /* C B */
    nm_tap(nm_black_key(NB_SPACE, 0)); nm_tap(nm_black_key(NB_MODE, 0)); nm_tap(white(1));        /* " 2" */
    nm_tap(nm_black_key(NB_SPACE, 0));                                                           /* a trailing space */
    song.playing = 1;
    press(B_OCTUP);
    bad += check("  OCT+ while playing: STOP TO SAVE, NAME stays", name_on() && msg_is("STOP TO SAVE") && !up_used(2));
    stop_transport();
    press(B_OCTUP);
    up_name(2, b);
    bad += check("  OCT+ stopped: saved as typed, the trailing space dropped (CB 2)", !name_on() && up_used(2) && str_eq(b, "CB 2") &&
                 msg_is("SAVED (RAM)"));
    /* SAVE held does nothing in NAME; a layer's button neither */
    ui.act = 4; press(B_OCTUP); press(B_OCTUP);   /* (U03 used: OVERWRITE?, YES) */
    bad += check("  overwrite: the dialog, then NAME", name_on() && nm.kind == NK_USER_SAVE);
    ui.msg_t = 0;
    hold(B_SAVE);
    btn_down(B_SCL); frames(600); ok = !ui.layer; btn_up(B_SCL); frame();
    bad += check("  SAVE held, SCL held: nothing in NAME (no undo, no map, no page)", name_on() && ok && cur_page()->graph == GR_USER &&
                 !ui.msg_t);
    press(B_OCTDN);
    /* rename: EDIT on USER */
    ui_old_uslot = 5;
    press(B_EDIT);
    bad += check("  EDIT on an empty slot: EMPTY SLOT", !name_on() && msg_is("EMPTY SLOT") && cur_page()->graph == GR_USER);
    ui_old_uslot = 2;
    up_load(2);                                   /* (the sound now comes from U03) */
    trk[0].p[P_E0] = 33;                          /* an edit: a rename must not store it */
    press(B_EDIT);
    bad += check("  EDIT on a used slot: NAME (rename) with its name", name_on() && nm.kind == NK_USER_RENAME && str_eq(nm.s, "CB 2"));
    nm_tap(nm_black_key(NB_DEL, 0));
    nm_tap(white(10)); nm_tap(white(10));          /* VW: W */
    press(B_OCTUP);
    up_name(2, b);
    bad += check("  OCT+: renamed (CB W), the sound stored there unchanged", !name_on() && str_eq(b, "CB W") &&
                 up_value(up_rec(2), P_E0) != 33 && msg_is("RENAMED (RAM)"));
    press(B_EDIT);
    for (i = 0; i < 12u; i++) nm_tap(nm_black_key(NB_DEL, 0));
    press(B_OCTUP);
    {
        char au[16];
        up_name(2, b);
        up_auto_name(au, up_rec(2)->engine, 2);
        bad += check("  a name deleted to nothing: the automatic one", str_eq(b, au));
    }
    /* the sound's own name is the prefill */
    trk[0].user = 3;
    ui_old_uslot = 7; ui.act = 4;
    press(B_OCTUP);
    up_name(2, b);
    bad += check("  SAVE of a sound that came from U03: prefilled with U03's name", name_on() && str_eq(nm.s, b));
    press(B_OCTDN);
    /* projects */
    go_page(GR_SLOTS);
    song.g[G_SLOT] = 2;
    turn(EN_K4, 1);
    press(B_OCTUP);
    for (i = 0; i < 4u; i++) { nm_tap(white(7)); nm_tap(white(6)); frames(800); }   /* P N P N .. */
    press(B_OCTUP);
    {
        char pn[16];
        int named = project_name(1, pn);
        bad += check("PROJECT SAVE: NAME, typed, saved: the slot has the name, the project too", named && str_eq(pn, "PNPNPNPN") &&
                     str_eq(proj_name, "PNPNPNPN"));
    }
    proj_name[0] = 0;
    project_load(1);
    bad += check("  loading it brings its name back (the next save's prefill)", str_eq(proj_name, "PNPNPNPN"));
    trk[0].p[P_LEVEL] = 7;
    press(B_EDIT);
    for (i = 0; i < 4u; i++) nm_tap(nm_black_key(NB_DEL, 1));
    press(B_OCTUP);
    {
        char pn[16];
        project_name(1, pn);
        bad += check("  EDIT renames a project (PNPN); its music unchanged", str_eq(pn, "PNPN") && stored_param(1, 0, P_LEVEL) != 7 &&
                     msg_is("RENAMED (RAM)"));
    }
    song.g[G_SLOT] = 4;
    press(B_EDIT);
    bad += check("  EDIT on an empty project slot: EMPTY SLOT", !name_on() && msg_is("EMPTY SLOT"));
    song.g[G_SLOT] = 2;
    press(B_EDIT);
    hold(B_HOME);
    bad += check("  HOME held in NAME: the menu, NAME closed", ui.menu && !name_on());
    hold(B_HOME);
    return bad;
}

/* the EDIT cycle (DUELL 3.6: ENGINE first, then the engine's pages; tests/engine_page_test.c has the rest) and its memory */
static int engine_cycle(const char *const *want, uint32_t n)   /* EDIT tapped from HOME, then n - 1 times more */
{
    uint32_t i;
    int ok = 1;
    frames(320);
    go_main_to(HP_MORPH); frame();
    for (i = 0; i < n; i++) {
        press(B_EDIT);
        frames(320);                                  /* (taps further apart than a double tap: #83 locks) */
        ok &= str_eq(cur_page()->title, want[i]);
    }
    return ok;
}
static int test_edit_cycle(void)
{
    static const char *const CYC_A[] = {"ENGINE", "EDIT 1", "EDIT 2", "MULTI FILTER", "VCF", "VOICE 1/2", "VOICE 2/2", "ENGINE"};   /* (1.5: FILTER; 0.2.9: the multimode FILTER first, ANALOG's own filter is VCF) */
    static const char *const CYC_D[] = {"ENGINE", "EDIT 1", "EDIT 2", "OP1 ENV", "OP2 ENV", "OP3 ENV", "OP4 ENV",
                                        "OP LEVEL", "VOICE 1/2", "VOICE 2/2", "ENGINE"};
    int bad = 0, ok;
    uint32_t i;
    ui_power_on();
    btn_down(B_EDIT); frame();
    ok = at_home();
    btn_up(B_EDIT); frame();
    bad += check("EDIT opens ENGINE, when let go (not on press)", ok && str_eq(cur_page()->title, "ENGINE"));
    for (i = 0, ok = 0; i < NPAGES; i++)
        ok += str_eq(PAGES[i].title, "ENGINE") && PAGES[i].fam == FAM_EDIT;
    bad += check("one ENGINE page, EDIT's (DUELL 3.6)", ok == 1);
    set_engine_of(TSEL, 0);
    bad += check("EDIT cycle (ANALOG): EDIT 1 EDIT 2 FILTER VCF VOICE 1/2 VOICE 2/2 EDIT 1", engine_cycle(CYC_A, NELEM(CYC_A)));
    set_engine_of(TSEL, 1);
    bad += check("EDIT cycle (engine 1 asked for: DIGITAL retired in 0.2.14): ANALOG's", TSEL->eng_req == 0u && engine_cycle(CYC_A, NELEM(CYC_A)));
    for (i = 0, ok = 1; i < NPAGES; i++)
        ok &= !str_eq(PAGES[i].title, "LANES") && !str_eq(PAGES[i].title, "LANES 2") && !str_eq(PAGES[i].title, "OP1 ENV");
    bad += check("DUELL: no LANES / OP ENV pages of Felucca's (their ids are DUELL's)", ok);
    go_title("EDIT 2"); ui.fam_last[FAM_EDIT] = ui.page;
    go_main_to(HP_MORPH); press(B_EDIT);
    bad += check("EDIT remembers its last page (as every family)", str_eq(cur_page()->title, "EDIT 2"));
    return bad;
}

/* DUELL 0.2.9: the RETIRED engines SAMPLE, GRAIN and SLICE (core.h ENGI_RETIRED; engines.c ENG_RETIRED). Stored data that names
 * one loads safely: a patch record of each (as 0.2.8 saved it) stays in the PATCH store, is listed in ALL but under no engine
 * filter, loads onto the part with its engine number and every value kept, plays silent, says NO ENGINE once (not on its notes),
 * shows blank EDIT cards no knob changes, MATRIX slots UNUSED; a ROLL, a MULTI save + load and a project (FUN) round trip keep
 * it; the ENGINE knob leaves it for a real engine, INIT gives ANALOG; ERASE removes the record. Nothing hangs */
static uint64_t part_energy(track_t *t, uint32_t note)   /* |output| over a held note and its release (MASTER at 1.0) */
{
    int32_t o[2u * CTL];
    uint64_t e = 0;
    uint32_t b, i, mq = song.master_q12;
    song.master_q12 = 4096;
    for (b = 0; b < 256u && (t->xf_on || t->eng_req != t->engine); b++)   /* (an engine switch done: its fade) */
        mix_block(o, CTL);
    trk_note_on(t, note, 100);
    for (b = 0; b < 96u; b++) {
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++)
            e += (uint64_t)(o[i] < 0 ? -o[i] : o[i]);
    }
    trk_note_off(t, note);
    for (b = 0; b < 96u; b++) {
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++)
            e += (uint64_t)(o[i] < 0 ? -o[i] : o[i]);
    }
    song.master_q12 = mq;
    return e;
}
static int test_retired_engines(void)
{
    static const uint8_t GONE[5] = {1, 4, 8, 10, 13};
    static const char *const NM[5] = {"DIGITAL", "SAMPLE", "GRAIN", "DRUM", "SLICE"};
    static const int16_t EV[8] = {3, -12, 41, 1, 112, 17, 100, 127};   /* (stored values of theirs: SET 3, TUNE -12, ..) */
    static project_t q, c;
    static project_store_t st;
    int bad = 0, ok;
    uint32_t g, i, k, n, tot, slot;
    char w[96];
    bad += check("retired: DIGITAL 1, SAMPLE 4, GRAIN 8, DRUM 10, SLICE 13 are in no list (NENG_SHOWN 10), named \"-\", silent placeholders, no factory sound",
                 NENG_SHOWN == 10u && !eng_ok(1) && !eng_ok(4) && !eng_ok(8) && !eng_ok(10) && !eng_ok(13) && ENGINES[1] == ENGINES[4] &&
                 ENGINES[4] == ENGINES[8] && ENGINES[8] == ENGINES[10] && ENGINES[10] == ENGINES[13] &&
                 str_eq(ENGINES[4]->name, "-") && !ENGINES[4]->npresets && !ds_count(1) && !ds_count(4) && !ds_count(8) && !ds_count(10) && !ds_count(13) &&
                 eng_ok(ENGI_WAVE) && eng_vis(NENG_SHOWN - 1u) == ENGI_WAVE);
    for (g = 0; g < 5u; g++) {
        uint32_t e = GONE[g], other;
        track_t *t;
        up_rec_t r;
        int16_t keep[P_COUNT];
        slot = 100u + g;
        ui_power_on();
        frames(200);
        t = TSEL;
        other = (song.sel + 1u) % NSHOWN;
        trk[other].p[P_MUTE] = 1;
        memset(&r, 0, sizeof r);                         /* a stored patch of the retired engine, as 0.2.8 saved it */
        r.used = UP_USED; r.ver = UP_VER; r.engine = (uint8_t)e; r.np = P_COUNT;
        memcpy(r.name, "OLD SOUND", 9);
        for (i = 0; i < P_COUNT; i++)
            up_set_value(&r, i, i >= P_E0 && i < P_E0 + 8u ? EV[i - P_E0] : t->p[i]);
        up_set_cat(&r, CAT_LEAD);
        k = (uint32_t)up_put(slot, &r);
        ok = (k == 0u || k == 3u) && up_used(slot) && up_engine_raw(slot) == e;   /* (3: no flash in this build, kept in RAM) */
        snprintf(w, sizeof w, "retired %s: a stored patch record of it is accepted and stays in the store", NM[g]);
        bad += check(w, ok);
        ok = sl_engine(slot) == e && !eng_ok(sl_engine(slot));
        for (k = 0; k < NENG_SHOWN; k++)
            ok &= eng_vis(k) != e;
        for (n = 0, k = 0; n < UP_SLOTS; n++)
            k += n == slot;                              /* (the list is the 128 slots: it is the one entry, in its slot's place) */
        ok &= k == 1u && list_keep(slot);
        bad += check("  listed once in ALL (the owner finds and erases it), under no engine filter", ok);
        up_load(slot);
        frame();
        ok = t->eng_req == e && !memcmp(&t->p[P_E0], EV, sizeof EV) && str_eq(ENGINES[t->eng_req % NENGINES]->name, "-") &&
             (msg_is(MSG_NO_ENGINE) || str_eq(ui.msg2, MSG_NO_ENGINE));
        bad += check("  loaded: its engine number and every value kept, the engine \"-\", NO ENGINE said", ok);
        frames(1500);
        ok = part_energy(t, 60) == 0u && part_energy(t, 72) == 0u && t->eng_req == e;
        frames(200);
        ok &= !ui.msg_t;
        bad += check("  it plays silent (two notes, the other part muted), NO ENGINE not said again on its notes", ok);
        memcpy(keep, t->p, sizeof keep);
        go_title("EDIT 1");
        frame();
        turn(EN_K1, 5); turn(EN_K2, -3); turn(EN_K3, 2); turn(EN_K4, 7);
        go_title("EDIT 2");
        frame();
        turn(EN_K1, 5); turn(EN_K4, -7);
        ok = !memcmp(keep, t->p, sizeof keep) && str_eq(ENGINES[e]->edit[0].label, "-");
        bad += check("  EDIT 1 / 2: blank cards, no knob changes a value", ok);
        ok = 1;
        for (k = 0; k < 8u; k++)
            ok &= mod_dst_why(t, MD_E1 + (int32_t)k) && str_eq(mod_dst_why(t, MD_E1 + (int32_t)k), "UNUSED");
        bad += check("  MATRIX: every engine slot faded UNUSED", ok);
        go_main_to(HP_MORPH);
        frame();
        morph_roll(1);
        frames(200);
        ok = t->eng_req == e && !memcmp(&t->p[P_E0], EV, sizeof EV);
        bad += check("  a MORPH ROLL: the part stays on it, its engine values untouched (no engine rules)", ok);
        stop_transport();
        project_save(10);                                /* a MULTI (M11) with the part on it: saved, loaded back */
        set_engine_of(t, 0);
        frames(1500);
        project_load(10);
        frame();
        ok = t->eng_req == e && !memcmp(&t->p[P_E0], EV, sizeof EV) && !strncmp(ui.msg, "LOADED", 6) && str_eq(ui.msg2, MSG_NO_ENGINE);
        if (!ok)
            printf("ui:   (multi: engine %u, E0 %d E1 %d, msg \"%s\" / \"%s\")\n", t->eng_req, t->p[P_E0], t->p[P_E1], ui.msg, ui.msg2);
        bad += check("  a MULTI save and load: the part on it again, every value kept; LOADED, then NO ENGINE", ok);
        project_capture(&q);
        ok = proj_pack(&st, &q) && proj_import(&c, &st, sizeof st) && c.t[song.sel].engine == e &&
             !memcmp(&c.t[song.sel].p[P_E0], EV, sizeof EV);
        bad += check("  a project (FUN: autosave, backup) round trip keeps it", ok);
        go_title("ENGINE");
        frame();
        turn(EN_K1, 1);
        frames(100);
        ok = eng_ok(t->eng_req) && t->eng_req != e;
        bad += check("  ENGINE KNOB 1 leaves it for a real engine", ok);
        t->eng_req = (uint8_t)e;
        set_engine(t->eng_req);                          /* INIT on such a part, the editor's ENG */
        ok = t->eng_req == 0u;
        bad += check("  INIT (a pick of the retired number): ANALOG", ok);
        patch_restore_do(slot);
        bad += check("  RESTORE removes the stored record (the slot is its factory sound again)", !up_used(slot));
        trk[other].p[P_MUTE] = 0;
    }
    ui_power_on();
    return bad;
}

/* ------------------------------------------------ the GLO SCL EDIT layers --- */
#define LY_INIT 0u   /* (0.2.11: QUICK FILTER has no INIT key; the parked, stale groups below still name it) */
static uint32_t black(uint32_t b) { return key_at(1, b); }
static void lay_combo(uint32_t btn, uint32_t k) { btn_down(btn); key_down(k); frame(); }
static void oct_back(void) { btn_down(B_OCTDN); frame(); btn_up(B_OCTDN); frame(); }

static int test_quick_layers(void)
{
    static const uint8_t LB[3] = {B_GLO, B_SCL, B_EDIT};
    static const uint8_t LL[3] = {LAYER_GLO, LAYER_SCL, LAYER_EDIT};
    static const char *const HINT[3] = {"HOLD [GLO] QUICK", "HOLD [SCL] QUICK", "HOLD [EDIT] QUICK"};
    int bad = 0, ok, flash, hint;
    uint32_t i, k, a, b2, mo;
    track_t before;
    /* tap / peek / combo, the hint once */
    ok = 1; hint = 1;
    for (i = 0; i < 3u; i++) {
        ui_power_on();
        btn_down(LB[i]);
        for (flash = 0, k = 0; k < 20u; k++) { frame(); flash |= ui.layer; }      /* 0.32 s */
        btn_up(LB[i]); frame();
        ok &= !flash && !at_home() && (LB[i] == B_GLO ? str_eq(cur_page()->title, "SONG") :
                                     LB[i] == B_SCL ? cur_page()->fam == FAM_SCL : str_eq(cur_page()->title, "ENGINE"));
        hint &= msg_is(HINT[i]);
        go_main_to(HP_MORPH); frame();
        btn_down(LB[i]); frames(368);
        ok &= !ui.layer;
        frames(64);
        ok &= ui.layer == LL[i] && ((layer_seen >> LL[i]) & 1u);
        btn_up(LB[i]); frame();
        ok &= at_home() && !ui.layer;
        ui.msg_t = 0;
        press(LB[i]);
        hint &= !msg_is(HINT[i]);
        go_main_to(HP_MORPH); frame();
        btn_down(LB[i]); key_down(white(9)); frame();
        ok &= ui.layer == LL[i] && !gates();
        key_up(white(9)); btn_up(LB[i]); frame();
        ok &= at_home() && !ui.layer;
    }
    bad += check("GLO SCL EDIT: a tap opens the page, held past HOLD the map (no page), a key: the map at once", ok);
    bad += check("  after a tap \"HOLD [BTN] QUICK\" until the layer has been opened once", hint);
    {
        persist_t p = {0};
        p.magic = PERSIST_MAGIC; p.panel = panel;
        layer_seen = 0x1Cu;
        settings_export(&p); layer_seen = 0;
        bad += check("  the seen bits are kept with the settings (a spare favorites byte)",
                     settings_import(&p, sizeof p) && layer_seen == 0x1Cu);
    }

    /* GLO: mutes latch (lit = sounding), UNMUTE ALL, TAP, levels, OCT- */
    ui_power_on();
    usb.config = 1; mo = mo_w;
    lay_combo(B_GLO, black(1));
    ok = trk[1].p[P_MUTE] == 1 && ui.layer == LAYER_GLO && !gates() && mo_w == mo;
    a = leds_at(0); b2 = leds_at(250);
    ok &= ((a & b2) >> black(0)) & 1u && !(((a | b2) >> black(1)) & 1u);           /* T1 sounding lit, T2 muted dark */
    ok &= ((a ^ b2) >> white(0)) & 1u && ((a ^ b2) >> white(7)) & 1u && ((a ^ b2) >> white(5)) & 1u &&   /* (D4: LEARN) */
          !(((a | b2) >> white(6)) & 1u);
    key_up(black(1)); btn_up(B_GLO); frame();
    bad += check("GLO + black key 2: T2 MUTE latched (SET), silent, no MIDI; LEDs: sounding lit, muted dark", ok &&
                 trk[1].p[P_MUTE] == 1 && !ui.layer);
    lay_combo(B_GLO, black(1)); key_up(black(1)); frame();
    key_down(white(4)); key_up(white(4)); frame();
    lay_combo(B_GLO, black(2)); key_up(black(2)); frame();
    ok = trk[1].p[P_MUTE] == 0 && trk[2].p[P_MUTE] == 1;
    key_down(white(4)); frame(); key_up(white(4)); frame();
    btn_up(B_GLO); frame();
    bad += check("  again: unmuted; C4: UNMUTE ALL", ok && !trk[2].p[P_MUTE] && !trk[1].p[P_MUTE]);
    /* (the GLO + key SOLO check is gone with SOLO) */
    song.g[G_BPM] = 100;
    btn_down(B_GLO); frame();
    for (i = 0; i < 3u; i++) { key_down(white(7)); frame(); key_up(white(7)); frames(480); }
    ok = song.g[G_BPM] == 120;
    btn_up(B_GLO); frame();
    song.g[G_CLOCK] = 1;
    lay_combo(B_GLO, white(7)); key_up(white(7)); frame();
    ok &= msg_is("TAP: CLK IS EXT") && song.g[G_BPM] == 120;
    btn_up(B_GLO); frame(); song.g[G_CLOCK] = 0;
    bad += check("GLO + F4 x3 at 0.5 s: TAP 120 BPM; with CLK EXT: the key says why", ok);
    {
        int16_t l2 = trk[2].p[P_LEVEL], atk = TSEL->p[P_ATK];
        go_title("AMP ENV");
        btn_down(B_GLO); frame();
        turn(EN_K3, -10);
        ok = trk[2].p[P_LEVEL] == l2 - 10 && TSEL->p[P_ATK] == atk && ui.layer == LAYER_GLO;
        key_down(black(0)); frame(); key_up(black(0)); frame();
        ok &= trk[0].p[P_MUTE] == 1 && song.octave == 0;
        oct_back();
        ok &= trk[2].p[P_LEVEL] == l2 && !trk[0].p[P_MUTE] && song.octave == 0 && ui.layer == LAYER_GLO;
        btn_up(B_GLO); frame();
        bad += check("GLO KNOB 3: T3 LEVEL from any page; OCT-: mutes and levels as it opened, no octave", ok &&
                     str_eq(cur_page()->title, "AMP ENV"));
    }
    song.playing = 1; transport_req = 0;
    btn_down(B_GLO); frame(); press(B_PLAY);
    ok = transport_req == 3u;
    events_block(CTL);
    ok &= song.playing && transport_req == 0u;
    btn_up(B_GLO); frame();
    stop_transport();
    btn_down(B_GLO); frame(); press(B_PLAY);
    ok &= transport_req == 1u;
    btn_up(B_GLO); frame(); transport_req = 0;
    bad += check("GLO + PLAY: RESTART playing (from the top, not stopped); stopped: PLAY", ok);
    usb.config = 0;

    /* SCL (GLOBAL, core.h): a key is ROOT, KNOB 1 the scale, KNOB 3 OFF-SCALE, the LEDs, OCT-; the parts' own CHRD VOIC TRN are not here */
    ui_power_on();
    root_g = 0; scale_g = 1; offscale_g = OS_SNAP;                 /* C MAJ */
    TSEL->p[P_ROOT] = 0; TSEL->p[P_CHRD] = CH_MAJ; TSEL->p[P_TRANS] = 2;
    lay_combo(B_SCL, black(4));                                    /* D#4 */
    ok = root_g == 3 && ui.layer == LAYER_SCL && !gates();
    key_up(black(4)); frame();
    key_down(white(1)); key_up(white(1)); frame();                  /* G3 */
    ok &= root_g == 7;
    turn(EN_K1, 1);
    ok &= scale_g == 2;
    turn(EN_K3, 1);
    ok &= offscale_g == OS_PLAY;
    turn(EN_K4, 1); turn(EN_K3, -1);
    ok &= offscale_g == OS_SNAP && TSEL->p[P_CHRD] == CH_MAJ && TSEL->p[P_TRANS] == 2 && TSEL->p[P_ROOT] == 0;   /* (K4 nothing; K3 back; the part's own untouched) */
    a = leds_at(0); b2 = leds_at(250);
    ok &= ((a & b2) >> white(1)) & 1u && ((a & b2) >> white(8)) & 1u;   /* G3 G4: the root, lit */
    ok &= ((a ^ b2) >> white(2)) & 1u && ((a ^ b2) >> black(4)) & 1u;   /* A, A# (G minor): blink */
    ok &= !(((a | b2) >> black(0)) & 1u);                               /* F#: not in G minor, dark */
    btn_up(B_SCL); frame();
    bad += check("SCL + D#4, then G3: ROOT D#, G (latched); KNOB 1: SCALE, KNOB 3 OFF-SCALE (the global three); LEDs: root lit, the scale breathes", ok &&
                 root_g == 7 && scale_g == 2 && at_home());
    btn_down(B_SCL); frame(); key_down(white(4)); key_up(white(4)); frame(); turn(EN_K1, 3);
    ok = root_g == 0 && scale_g == 5;
    oct_back();
    btn_up(B_SCL); frame();
    bad += check("  OCT- in SCL: ROOT, SCALE and OFF-SCALE as the layer opened", ok && root_g == 7 && scale_g == 2 && offscale_g == OS_SNAP);

    /* EDIT: no engine keys (SPEC 1.1a), sound loads (KNOB 1 2 3) with UNDO, INIT (F3) with the dialog */
    ui_power_on();
    set_engine_of(TSEL, 0); go_main_to(HP_MORPH); frame();
    my_steps(TSEL); TSEL->p[P_SCALE] = 3; song.playing = 1;
    before = *TSEL;
    sync_reload = 0;
    lay_combo(B_EDIT, white(1));                       /* (no engine keys: EDIT + a white key picks nothing, SPEC 1.1a) */
    ok = TSEL->eng_req == 0u && ui.layer == LAYER_EDIT && !sync_reload && !ui.confirm;
    key_up(white(1)); frame();
    for (i = 0; i < 16u; i++) {
        if (i == LY_INIT) continue;
        key_down(white(i)); key_up(white(i)); frame();
        ok &= TSEL->eng_req == 0u && !ui.confirm;
    }
    ok &= !memcmp(TSEL->step, before.step, sizeof before.step) && TSEL->p[P_SLEN] == before.p[P_SLEN] &&
          TSEL->p[P_SCALE] == 3;
    btn_up(B_EDIT); frame();
    bad += check("EDIT + a white key (not INIT): no engine picked, no sound loaded, while playing; steps, LEN, SCL stay", ok);
    btn_down(B_EDIT); frames(480);                      /* (0.2.9: QUICK FILTER, KNOB 1..4 CUT RES MODE REL; up to 0.2.8 ENG No. FAV) */
    ok = ui.layer == LAYER_EDIT;
    {
        const page_t *qp = ly_page(LAYER_EDIT);
        int16_t c0 = qp->id[0] != 0xFFu ? TSEL->p[qp->id[0]] : 0;
        turn(EN_K1, -3);
        ok &= qp->id[0] == 0xFFu || TSEL->p[qp->id[0]] != c0;
        oct_back();
        ok &= qp->id[0] == 0xFFu || TSEL->p[qp->id[0]] == c0;
    }
    btn_up(B_EDIT); frame();
    bad += check("  QUICK FILTER: KNOB 1 CUT; OCT-: the four as the layer opened", ok && at_home());
    song.playing = 0;
    lay_combo(B_EDIT, white(LY_INIT));
    ok = ui.confirm == CF_INIT_SOUND;
    frame();
    ok &= !ui.layer;
    key_up(white(LY_INIT)); btn_up(B_EDIT); frame();
    ok &= at_home() && ui.confirm == CF_INIT_SOUND;
    TSEL->p[P_E0] = (int16_t)(TSEL->p[P_E0] + 5);
    press(B_OCTUP);
    bad += check("EDIT + F3 (INIT): INITIALIZE SOUND? dialog (closes the layer, no tap); OCT+ inits", ok && !ui.confirm &&
                 msg_is("SOUND INIT") && TSEL->p[P_E0] == ENGINES[TSEL->eng_req % NENGINES]->presets[0].e[0]);

    /* no layer in the menu or a dialog: GLO + a key plays */
    ui_power_on(); hold(B_HOME);
    btn_down(B_GLO); frame(); key_down(white(3)); frame();
    ok = !ui.layer && gates() > 0 && !kb_layer;
    key_up(white(3)); btn_up(B_GLO); frame();
    bad += check("in the menu: no GLO layer, GLO + a key plays it; GLO let go: no page", ok && ui.menu == 1);
    ui_power_on(); ui.confirm = CF_CLEAR_SEQ;
    btn_down(B_SCL); frames(512);
    ok = !ui.layer;
    btn_up(B_SCL); frame();
    bad += check("in a dialog: SCL held shows no map and taps nothing", ok && ui.confirm == CF_CLEAR_SEQ);
    /* one layer at a time: a second layer button is ignored, its keys stay notes after */
    ui_power_on();
    btn_down(B_GLO); frame(); btn_down(B_EDIT); frame(); key_down(white(1)); frame();
    ok = ui.layer == LAYER_GLO && TSEL->eng_req == trk[0].eng_req;
    key_up(white(1)); btn_up(B_GLO); frame();
    key_down(white(1)); frame();
    ok &= gates() > 0 && !ui.layer;
    key_up(white(1)); btn_up(B_EDIT); frame();
    bad += check("GLO then EDIT: GLO's layer; EDIT ignored (no layer, no page, its keys notes)", ok && at_home());
    return bad;
}

/* bug fixes (UI): one regression each */
static int test_bughunt_ui(void)
{
    int bad = 0, ok;
    /* 1: REC in NAME, the menu or a dialog: nothing (PLAY cannot stop the transport there) */
    ui_power_on();
    go_page(GR_USER); ui_old_uslot = 3;
    press(B_OCTUP);                                     /* NAME opens */
    press(B_REC);
    bad += check("REC in NAME does nothing (no arm, no transport start); NAME stays",
                 name_on() && !song.rec && !transport_req && !song.playing);
    ui_power_on(); hold(B_HOME); press(B_REC); press(B_PLAY);
    bad += check("REC in the menu does nothing (then PLAY: nothing either)", ui.menu && !song.rec && !transport_req);
    ui_power_on();
    up_ui(2, 5);
    go_page(GR_USER); ui_old_uslot = 5; press(B_OCTUP);
    press(B_REC);
    ok = ui.confirm == CF_OVR_USER && !song.rec && !transport_req;
    press(B_OCTUP);
    bad += check("REC in the OVERWRITE? dialog does nothing; OCT+ then opens NAME", ok && name_on());
    {   /* 2: UNDO of a sound load on an FM6 track: the track's own (edited) patch, not PTCH's factory one */
        uint8_t mine[FP_SIZE + 1u], next[FP_SIZE + 1u];
        ui_power_on();
        track_select(1); frame();
        undo_depth++; set_engine_of(TSEL, ENGI_FM6); undo_depth--;   /* (no undo copy of this) */
        frame();
        memcpy(mine, fm6_patch[1], sizeof mine);
        mine[0] ^= 0x15; mine[5] ^= 0x22; mine[40] ^= 0x07;   /* an edited patch (the editor, a project) */
        fm6_set_patch(1, mine);
        memcpy(mine, fm6_patch[1], FP_SIZE);
        turn(EN_PRESET, 1);                             /* browse one sound */
        memcpy(next, fm6_patch[1], FP_SIZE);
        hold(B_SAVE);                                   /* UNDO */
        frame();
        ok = TSEL->eng_req == ENGI_FM6 && !memcmp(mine, fm6_patch[1], FP_SIZE);
        hold(B_SAVE);                                   /* REDO */
        frame();
        bad += check("UNDO of a sound load on FM6: the track's edited patch back; REDO: the load's",
                     ok && memcmp(mine, next, FP_SIZE) && !memcmp(next, fm6_patch[1], FP_SIZE));
        hold(B_SAVE); frame();                          /* (the edited patch again) */
        btn_down(B_EDIT); frames(500);                  /* EDIT layer: another engine, then OCT- */
        turn(EN_K1, 1);
        ok = ui.layer == LAYER_EDIT && TSEL->eng_req != ENGI_FM6;
        oct_back();
        btn_up(B_EDIT); frame();
        bad += check("  the EDIT layer's OCT- after an engine pick: FM6 with the track's edited patch",
                     ok && TSEL->eng_req == ENGI_FM6 && !memcmp(mine, fm6_patch[1], FP_SIZE));
    }
    {   /* 2b: a patch the editor sends between two preset loads is a new starting point: UNDO brings it back */
        uint8_t ed[FP_SIZE + 1u];
        ui_power_on();
        track_select(1); frame();
        undo_depth++; set_engine_of(TSEL, ENGI_FM6); undo_depth--;
        frame();
        turn(EN_PRESET, 1); frame();                    /* load 1 */
        memcpy(ed, fm6_patch[1], sizeof ed);
        ed[0] ^= 0x15; ed[40] ^= 0x07;
        fm6_set_patch(1, ed);                           /* the editor's FM6_PUT */
        memcpy(ed, fm6_patch[1], FP_SIZE);
        turn(EN_PRESET, 1); frame();                    /* load 2 */
        hold(B_SAVE); frame();                          /* UNDO */
        bad += check("UNDO after load, editor patch, load: the editor's patch back",
                     !memcmp(ed, fm6_patch[1], FP_SIZE));
    }
    {   /* the ARP button flashes on the beat while an ARP plays (the bar's first beat longer); dark otherwise */
        uint32_t q, b, r[5];
        ui_power_on(); frame();
        b = beat_samples();
        r[0] = arp_led();                               /* no ARP playing */
        trk[2].p[P_AMODE] = 1; trk[2].nheld = 1; trk[2].held[0] = 60;
        beat_n = 1; beat_pos = 0; r[1] = arp_led();
        beat_pos = b / 3u; r[2] = arp_led();
        beat_n = 0; r[3] = arp_led();
        beat_pos = b * 3u / 4u; r[4] = arp_led();
        bad += check("ARP LED: none without an ARP; flashes on the beat, the bar's first beat longer",
                     r[0] == 2u && r[1] == 1u && r[2] == 0u && r[3] == 1u && r[4] == 0u);
        for (q = 0; q < 4u * b / 128u + 4u; q++) events_block(128);
        bad += check("  the beat counter runs (a bar of blocks wraps the beat of the bar)", beat_n < 4u && beat_pos < b);
        trk[2].p[P_AMODE] = 0; trk[2].nheld = 0;
    }
    /* 3: USER ERASE asks first (ERASE U02?): OCT- keeps the preset, OCT+ erases it */
    ui_power_on();
    go_page(GR_USER); ui_old_uslot = 1;
    up_ui(2, 1);
    turn(EN_K1 + 2, 1);                                 /* KNOB 3: ERASE picked */
    press(B_OCTUP);
    ok = ui.confirm == CF_RESTORE && ui.confirm_trk == 1u && up_used(1);
    {
        char a[24], b[24];
        confirm_text(a, b);
        ok &= str_eq(a, "ERASE U02?") && b[0];
    }
    press(B_OCTDN);
    ok &= !ui.confirm && up_used(1);
    turn(EN_K1 + 2, 1); press(B_OCTUP); press(B_OCTUP);
    bad += check("USER ERASE: the ERASE U02? dialog; OCT- keeps the preset, OCT+ erases it",
                 ok && !ui.confirm && !up_used(1));
    turn(EN_K1 + 2, 1); press(B_OCTUP);
    bad += check("  an empty slot: no dialog, EMPTY SLOT", !ui.confirm && msg_is("EMPTY SLOT"));
    {   /* 4: AUTOMATION (CHANCE's since 1.2) is not STEP: no key entry, no EDIT clear (its knobs only) */
        step_t s0;
        ui_power_on(); go_auto_top(); frame();
        my_steps(TSEL);
        s0 = TSEL->step[0];
        tap_key(white(5)); frame();
        ok = !memcmp(&s0, &TSEL->step[0], sizeof s0) && ui.cursor == 0u;
        press(B_EDIT);
        ok &= !memcmp(&s0, &TSEL->step[0], sizeof s0) && !msg_is("STEP CLEARED");
        ui_power_on(); go_auto_add(EV_CHANCE, 0); frame();
        s0 = TSEL->step[0];
        press(B_OCTUP);                                 /* (100 % by default: + ADD CHANCE, 50 %) */
        bad += check("AUTOMATION on a melodic track: a key writes no step, EDIT clears none; + ADD CHANCE the chance",
                     ok && step_chance(&TSEL->step[0]) != step_chance(&s0));
        ui_power_on(); go_page(GR_ROLL); frame(); song.rec = 1;   /* (#133: armed, stopped) */
        tap_key(white(5)); frame();
        bad += check("  STEP still: armed, a key writes the cursor step", step_on(&TSEL->step[0]) || TSEL->step[0].n);
        song.rec = 0;
    }
    {   /* 5: ALGORITHM does nothing while a layer's button is held (as PRESETS); after it, the track again */
        int16_t r0;
        ui_power_on();
        r0 = root_g;
        btn_down(B_SCL); frames(500);
        turn(EN_ALGO, 1);
        ok = ui.layer == LAYER_SCL && song.sel == 0u;
        key_down(white(1)); frame(); key_up(white(1)); frame();   /* ROOT on T1 */
        ok &= root_g != r0;
        oct_back();
        ok &= root_g == r0;
        btn_up(B_SCL); frame();
        btn_down(B_EDIT); turn(EN_ALGO, 1);             /* armed, not open yet: ignored too */
        ok &= song.sel == 0u;
        btn_up(B_EDIT); frame(); go_main_to(HP_MORPH); frame();
        turn(EN_ALGO, 1);
        bad += check("ALGORITHM ignored with a layer's button held (OCT- puts back all); then T2",
                     ok && song.sel == 1u);
    }
    /* 6: TOOLS: each column its own ready check; nothing to do says so (not STOP TO EDIT) */
    ui_power_on();
    go_title("TOOLS"); frame();
    track_defaults_steps(TSEL);                         /* DUELL: CLEAR PAT, INIT SOUND (no SECTION, SONG) */
    turn(EN_K1, 1);                                     /* CLEAR PAT on an empty track */
    ok = !act_ready();
    press(B_OCTUP);
    ok &= !ui.confirm && msg_is("NOTHING TO CLEAR");
    turn(EN_K1 + 1, 1);                                 /* INIT: its dialog */
    ok &= act_ready();
    press(B_OCTUP);
    bad += check("TOOLS: CLEAR PAT with nothing there: NOTHING TO CLEAR; INIT: its dialog", ok && ui.confirm == CF_INIT_SOUND);
    /* 7: REC works in the layers, as PLAY: it arms the track (PLAY starts); the layer stays, no page */
    ok = 1;
    {
        static const uint8_t LB[3] = {B_GLO, B_SCL, B_EDIT};
        uint32_t i;
        for (i = 0; i < 3u; i++) {
            ui_power_on();
            btn_down(LB[i]); frame();
            press(B_REC);
            ok &= song.rec == 1u && transport_req == 1u && ui.layer != 0u;
            btn_up(LB[i]); frame();
            ok &= at_home();
        }
    }
    bad += check("REC in every layer: arms the track and starts PLAY; the layer stays, no page", ok);
    /* 8: the OCT- LED lit in the SET layers (OCT- = UNDO), OCT+ dark */
    ok = 1;
    {
        static const uint8_t LB[3] = {B_GLO, B_SCL, B_EDIT};
        uint32_t i;
        for (i = 0; i < 3u; i++) {
            ui_power_on();
            song.octave = 1;                            /* (outside: OCT+ lit) */
            btn_down(LB[i]); frames(500);
            ok &= layer_set_open() && oct_leds_seen(1) == 1u && oct_leds_seen(0) == 1u;
            btn_up(LB[i]); frame();
            ok &= oct_leds() == 2u;
        }
    }
    bad += check("SET layers: the OCT- LED lit (UNDO), OCT+ dark; after: the octave", ok);
    /* 9: no "HOLD [EDIT] QUICK" over NAME (EDIT tapped on USER / PROJECT renames) */
    ui_power_on();
    layer_seen = 0;
    up_ui(2, 0);
    go_page(GR_USER); ui_old_uslot = 0; frame();
    ui.msg_t = 0;
    press(B_EDIT);
    ok = name_on() && !msg_is("HOLD [EDIT] QUICK");
    ui_power_on();
    layer_seen = 0;
    press(B_EDIT);
    bad += check("EDIT tap = RENAME on USER: no layer hint over NAME (elsewhere the hint as before)",
                 ok && msg_is("HOLD [EDIT] QUICK"));
    return bad;
}

/* the SCL family (owner 2026-10-12): page 1 SCALE is GLOBAL (K1 SCALE, K2 ROOT, K3 OFF-SCALE; the header lights both parts), page 2 CHORD the
 * selected part's (K1 CHRD, K2 VOIC, K3 TRN; the header lights the selected part); no page mixes the two. The chord page names the last chord
 * (MONO gray); a sound load keeps them; the SCL layer is the global three */
static int test_chord_page(void)
{
    int bad = 0, ok;
    uint32_t i;
    track_t *t;
    ui_power_on();
    t = TSEL;
    t->p[P_VOICE] = V_POLY;
    go_main_to(HP_MORPH); frame();
    press(B_SCL); frames(400);
    ok = str_eq(cur_page()->title, "SCALE") && cur_page()->scope == SC_SCALE && cur_page()->fam == FAM_SCL && page_global(cur_page());
    bad += check("SCL: page 1 is SCALE, a GLOBAL page (K1 SCALE, K2 ROOT, K3 OFF-SCALE; no part parameter id)", ok &&
                 cur_page()->id[0] == 0xFFu && cur_page()->id[1] == 0xFFu && cur_page()->id[2] == 0xFFu && cur_page()->id[3] == 0xFFu);
    song.sel = 1;
    ok = badge_lit() == 3u;
    song.sel = 0;
    ok &= badge_lit() == 3u;
    turn(EN_K1, 2); turn(EN_K2, 3); turn(EN_K3, 1);
    ok &= scale_g == 2 && root_g == 3 && offscale_g == OS_PLAY && t->p[P_SCALE] == 0 && t->p[P_ROOT] == 0 && t->p[P_QUANT] == 0;
    turn(EN_K3, -1);
    ok &= offscale_g == OS_SNAP;
    bad += check("  the header badge lights BOTH parts whichever is selected; K1 SCALE, K2 ROOT, K3 OFF-SCALE edit the globals, never a part's own", ok);
    song.g[G_BPM] = song.g[G_BPM];
    scale_g = 0; root_g = 0;
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    bad += check("  CHROMATIC: the page draws (OFF-SCALE not drawn), MONO gray", screen_gray());
    press(B_SCL); frames(400);
    ok = str_eq(cur_page()->title, "CHORD") && cur_page()->graph == GR_CHORD && cur_page()->fam == FAM_SCL && !page_global(cur_page());
    ok &= badge_lit() == 1u;
    song.sel = 1; ok &= badge_lit() == 2u; song.sel = 0;
    bad += check("SCL tapped again: the CHORD page (CHRD VOIC TRN: the selected part's, its badge alone lit)", ok && cur_page()->id[0] == P_CHRD &&
                 cur_page()->id[1] == P_VOIC && cur_page()->id[2] == P_TRANS);
    for (i = 0, ok = 1; i < NPAGES; i++)                   /* no page of the family mixes global and part controls */
        if (PAGES[i].fam == FAM_SCL) {
            uint32_t k, ids = 0;
            for (k = 0; k < 4u; k++) ids += PAGES[i].id[k] != 0xFFu;
            ok &= PAGES[i].scope == SC_SCALE ? ids == 0u : (PAGES[i].scope == SC_TRACK && ids > 0u);
        }
    bad += check("  no page of the SCL family mixes global and per-part controls", ok);
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    bad += check("  CHRD OFF: the page says what to do, MONO gray", screen_gray());
    turn(EN_K1, 1); frame();
    turn(EN_K2, 1); frame();
    bad += check("  KNOB 1: CHRD DIA3, KNOB 2: VOIC OPEN", t->p[P_CHRD] == CH_DIA3 && t->p[P_VOIC] == VC_OPEN);
    key_down(7); frame();                                          /* C4, CHROMATIC: DIA3 plays the plain MAJ triad (C E G) */
    ok = gates() == 3u && chord_last[song.sel].root == 60;
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    ok &= screen_gray();
    key_up(7); frame();
    bad += check("  a key plays the chord, the graph shows it (C), MONO gray", ok && !gates());
    t->p[P_CHRD] = CH_MIN7; t->p[P_VOIC] = VC_BASS;
    apply_preset_to(t, 1);
    bad += check("  a sound load keeps CHRD and VOIC (SCL settings)", t->p[P_CHRD] == CH_MIN7 && t->p[P_VOIC] == VC_BASS);
    ui_power_on();
    t = TSEL;
    t->p[P_CHRD] = CH_OFF; t->p[P_VOIC] = VC_CLOSE;
    go_main_to(HP_MORPH); frame();
    btn_down(B_SCL); frames(800);
    ok = ui.layer == LAYER_SCL && badge_lit() == 3u;
    turn(EN_K3, 2); frame();
    turn(EN_K4, 1); frame();
    ok &= t->p[P_CHRD] == CH_OFF && t->p[P_VOIC] == VC_CLOSE && t->p[P_TRANS] == 0 && offscale_g == OS_PLAY;
    turn(EN_K1, 2); frame();
    ok &= scale_g == 2;
    oct_back();
    btn_up(B_SCL); frame();
    bad += check("SCL layer: the global three (K1 SCALE, K3 OFF-SCALE; CHRD VOIC TRN are not here, both badges lit); OCT- puts them back", ok &&
                 scale_g == 0 && offscale_g == OS_SNAP && t->p[P_CHRD] == CH_OFF && at_home());
    btn_down(B_SCL); frames(800);
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    ok = screen_gray();
    btn_up(B_SCL); frame();
    bad += check("  its map and cards MONO gray", ok);
    return bad;
}

/* the STEP page's piano roll (ui_graph.c graph_roll), read back from the screen: per column the rows whose centre
 * pixel (the bar's middle, x + 6) is a bar colour, against the notes the step sounds (its own; a TIE: the note
 * before's; a REST: none) */
static int pr_is_bar(uint32_t x, uint32_t y)
{
    uint16_t c = swap16(host_screen[y * 240u + x]);
    return c == T_THEME || c == T_ACCENT || c == T_TEXT;
}
static int pr_columns_match(const track_t *t, uint32_t *nbars)
{
    uint32_t i, r, j, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    int ok = 1;
    for (i = 0; i < 16u && base + i < len; i++) {
        uint32_t want = 0, got = 0, s = pr_src(t, base + i, len);
        for (j = 0; s < NSTEP && j < 4u; j++) {
            int32_t n = pr_note(&t->step[s], j), rr = (int32_t)proll.lo + PR_ROWS - 1 - n;
            if (n >= 0 && rr >= 0 && rr < PR_ROWS) want |= 1u << rr;
        }
        for (r = 0; r < PR_ROWS; r++)
            if (pr_is_bar((uint32_t)(PR_X0 + (int32_t)i * PR_CW + 6), Y_GRAPH + PR_Y0 + r * PR_RH + 2u)) got |= 1u << r;
        if (got != want) {
            printf("ui:   roll column %u: rows %05x drawn, %05x wanted\n", base + i, got, want);
            ok = 0;
        }
        for (r = 0; r < PR_ROWS; r++) *nbars += (got >> r) & 1u;
    }
    return ok;
}
/* 1.2, Discussion #133: step recording on SEQ > STEP. Not armed, the keys only play (being on SEQ writes nothing);
 * REC there arms step recording without starting the transport; armed and stopped a key (keys held together: a
 * chord) writes the cursor step and letting go moves the cursor on one step, wrapping inside LEN; PLAY records live
 * (at the play head, not the cursor). The DRUM grid keeps its own keys (white keys = steps, REC starts PLAY) */
static int test_step_rec(void)
{
    int bad = 0, ok;
    track_t *t;
    step_t before[NSTEP];
    ui_power_on();
    t = TSEL;
    t->p[P_VOICE] = V_POLY;
    track_defaults_steps(t);
    go_page(GR_ROLL);
    cursor_set(3);
    frame();
    memcpy(before, t->step, sizeof before);
    key_down(7); frame();
    ok = kb_chn[7] != 0u;                                             /* it sounds */
    key_up(7); frame();
    ok &= !memcmp(before, t->step, sizeof before) && ui.cursor == 3u && !song.rec && !transport_req;
    bad += check("#133 STEP, not armed, stopped: a key sounds, writes nothing, the cursor stays", ok);
    song.playing = 1;
    key_down(7); frame(); key_up(7); frame();
    song.playing = 0;
    bad += check("#133 STEP, not armed, playing: nothing written either", !memcmp(before, t->step, sizeof before) &&
                 ui.cursor == 3u);
    song.rec = 2u;                                                    /* T2 armed, T1 selected */
    key_down(7); frame(); key_up(7); frame();
    bad += check("#133 another track armed: the selected track's STEP writes nothing",
                 !memcmp(before, t->step, sizeof before) && ui.cursor == 3u);
    song.rec = 0;
    press(B_REC);
    bad += check("#133 REC on STEP arms step recording: stopped, KEYS WRITE STEPS",
                 song.rec == 1u && !transport_req && !song.playing && !seq_counting() && msg_is("KEYS WRITE STEPS"));
    key_down(7); frame();
    ok = t->step[3].n == 1u && t->step[3].note[0] == kb_map(t, 7) && t->step[3].time == ST_NOTE && ui.cursor == 3u;
    key_up(7); frame();
    ok &= ui.cursor == 4u && !memcmp(before, t->step, 3u * sizeof before[0]) &&
          !memcmp(before + 4, t->step + 4, (NSTEP - 4u) * sizeof before[0]);
    bad += check("#133 armed, stopped: a key writes its note into the cursor step (only), letting go moves on one", ok);
    key_down(7); frame(); key_down(11); frame(); key_down(14); frame();
    ok = t->step[4].n == 3u && ui.cursor == 4u;
    key_up(7); frame();
    ok &= ui.cursor == 4u;                                            /* (two keys still down: the step stays open) */
    key_up(11); key_up(14); frame();
    ok &= ui.cursor == 5u && t->step[4].note[0] == kb_map(t, 7) && t->step[4].note[1] == kb_map(t, 11) &&
          t->step[4].note[2] == kb_map(t, 14);
    bad += check("#133 keys held together: one chord step (POLY), the cursor moves on once all are up", ok);
    t->p[P_VOICE] = V_MONO;
    key_down(7); frame(); key_down(9); frame(); key_up(7); key_up(9); frame();
    bad += check("#133 MONO: one note, the last key", t->step[5].n == 1u && t->step[5].note[0] == kb_map(t, 9) &&
                 ui.cursor == 6u);
    t->p[P_SLEN] = 8;
    cursor_set(7);
    frame();
    key_down(9); frame(); key_up(9); frame();
    bad += check("#133 the last step of LEN 8: written, the cursor wraps to step 1", t->step[7].n == 1u && ui.cursor == 0u);
    cursor_set(4);                                                    /* (an empty step, away from the play head) */
    step_clear(&t->step[4]);
    frame();
    press(B_PLAY);
    events_block(CTL);
    ok = song.playing && live_rec_sel() && t->seq_idx < 2u;
    key_down(7); frame(); key_up(7); frame();
    ok &= ui.cursor == 4u && !t->step[4].n && (t->step[0].n || t->step[1].n);
    bad += check("#133 PLAY with the track armed: live recording at the play head (the cursor and its step stay)", ok);
    stop_transport();
    press(B_REC);                                                     /* disarm: the keys only play again */
    memcpy(before, t->step, sizeof before);
    key_down(7); frame(); key_up(7); frame();
    bad += check("#133 REC again disarms: the keys only play, nothing written",
                 !song.rec && !memcmp(before, t->step, sizeof before) && ui.cursor == 4u);
    song.rec = 0;
    return bad;
}

/* 1.2: MENU > CONTROL > STEP PREVIEW (Discussion #169) and CHORD ENTRY (#155), MENU > DISPLAY > SCOPE (#165; its sound
 * path: click_test.c). Every new setting 0 by default (today's behaviour), a bit of its own byte */
static uint32_t gated(const track_t *t, uint32_t note)   /* voices of t sounding note with their gate on */
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active && t->v[i].gate && (note > 127u || t->v[i].note == note);
    return n;
}
static void blocks(uint32_t n) { while (n--) events_block(CTL); }
static void tap_k(uint32_t k) { key_down(k); frame(); key_up(k); frame(); }
static int test_step_preview_chord_entry(void)
{
    int bad = 0, ok;
    track_t *t;
    step_t before[NSTEP];
    uint32_t i, rec0, mc0;
    ui_power_on();
    ok = !ui_prefs2 && !menu_get(MI_PREVIEW) && !menu_get(MI_CHADD) && !scope_mix &&   /* (0.2.14: SCOPE removed; CHORD ENTRY is STEP KEYS) */
         str_eq(menu_vname(MI_PREVIEW, 1), "ON") && str_eq(menu_vname(MI_CHADD, 0), "WRITE") &&
         str_eq(menu_vname(MI_CHADD, 1), "ADD") && MI_TAB[MI_PREVIEW] == MTAB_CONTROL &&
         MI_TAB[MI_CHADD] == MTAB_CONTROL;
    menu_put(MI_PREVIEW, 1); menu_put(MI_CHADD, 1); ok &= ui_prefs2 == 6u && !ui_prefs && !(ui_rec_prefs & 0x40u);
    menu_put(MI_PREVIEW, 0); menu_put(MI_CHADD, 0); ok &= !ui_prefs2 && !scope_mix;
    bad += check("1.2 MENU: STEP PREVIEW OFF / ON, STEP KEYS WRITE / ADD: 0 by default, a bit each", ok);

    /* STEP PREVIEW */
    t = TSEL;
    t->p[P_VOICE] = V_POLY;
    track_defaults_steps(t);
    for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
    t->step[3] = (step_t){{60, 64}, 2, ST_NOTE, 0, 100};
    t->step[4] = (step_t){{67}, 1, ST_NOTE, 0, 100};
    t->step[4].probability = 101;                       /* (chance 0: the preview plays it all the same) */
    go_page(GR_ROLL);
    cursor_set(2);
    frame();
    memcpy(before, t->step, sizeof before);
    rec0 = song.rec; mc0 = motion.count;
    turn(EN_K1, 1);
    blocks(2);
    ok = ui.cursor == 3u && !aud_req && !gated(t, 128u);
    bad += check("STEP PREVIEW OFF (the default): moving the cursor sounds nothing", ok);
    menu_put(MI_PREVIEW, 1);
    turn(EN_K1, 1);
    ok = ui.cursor == 4u && aud_req;
    blocks(1);
    ok &= gated(t, 67) == 1u && gated(t, 128u) == 1u && !aud_req;
    turn(EN_K1, -1);
    blocks(1);
    ok &= ui.cursor == 3u && gated(t, 60) && gated(t, 64) && !gated(t, 67);
    blocks(div_samples(2) / CTL + 2u);
    ok &= !gated(t, 128u);
    bad += check("STEP PREVIEW ON: KNOB 1 to a step sounds it once on the track (a chord whole; chance not rolled), a 1/16 at GATE", ok);
    turn(EN_PRESET, 1);                                 /* (PRESETS: no second cursor) */
    blocks(1);
    ok = ui.cursor == 3u && !aud_req;
    turn(EN_K1, 1);
    blocks(1);
    ok &= ui.cursor == 4u && gated(t, 67) == 1u;
    blocks(div_samples(2) / CTL + 2u);
    turn(EN_K1, -3);                                    /* (an empty step: nothing) */
    blocks(1);
    ok &= ui.cursor == 1u && !gated(t, 128u);
    bad += check("STEP PREVIEW: PRESETS does not move the cursor; KNOB 1 sounds it; an empty step sounds nothing", ok);
    ok = !memcmp(before, t->step, sizeof before) && song.rec == rec0 && motion.count == mc0 && !song.playing;
    bad += check("STEP PREVIEW never writes: the steps, the arming and the motion as they were", ok);
    song.playing = 1;
    turn(EN_K1, 2);
    ok = ui.cursor == 3u && !aud_req;
    aud_req = (uint16_t)(3u << 8 | 1u);                 /* (one asked for just before PLAY: dropped) */
    blocks(1);
    ok &= !aud_req && !gated(t, 60);
    song.playing = 0;
    blocks(div_samples(2) / CTL + 2u);
    song.rec = 1u;                                      /* armed: step recording, stopped: it sounds */
    turn(EN_K1, 1);
    blocks(1);
    ok &= ui.cursor == 4u && gated(t, 67) == 1u && !memcmp(before, t->step, sizeof before);
    blocks(div_samples(2) / CTL + 2u);
    tap_k(7);                                           /* step entry's moving on (HOLD): no preview */
    blocks(1);
    ok &= ui.cursor == 5u && t->step[4].note[0] == kb_map(t, 7) && !aud_req;
    song.rec = 0;
    bad += check("STEP PREVIEW: not while playing (live recording included); armed and stopped it sounds; step entry's move does not", ok);
    menu_put(MI_PREVIEW, 0);
    menu_put(MI_PREVIEW, 0);

    /* CHORD ENTRY ADD */
    ui_power_on();
    menu_put(MI_CHADD, 1);
    t = TSEL;
    t->p[P_VOICE] = V_POLY;
    track_defaults_steps(t);
    for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
    t->step[5] = (step_t){{50}, 1, ST_NOTE, 0, 100};
    go_page(GR_ROLL);
    cursor_set(3);
    frame();
    press(B_REC);
    ok = song.rec == 1u && !transport_req && !song.playing && msg_is("KEYS ADD TO STEP");
    bad += check("#155 CHORD ENTRY ADD: REC on STEP arms step recording, KEYS ADD TO STEP", ok);
    tap_k(7);
    ok = t->step[3].n == 1u && t->step[3].note[0] == kb_map(t, 7) && t->step[3].time == ST_NOTE && ui.cursor == 3u;
    tap_k(11); tap_k(14);
    ok &= t->step[3].n == 3u && t->step[3].note[1] == kb_map(t, 11) && t->step[3].note[2] == kb_map(t, 14) && ui.cursor == 3u;
    tap_k(16); tap_k(18);
    ok &= t->step[3].n == 4u && t->step[3].note[3] == kb_map(t, 16) && ui.cursor == 3u;   /* (a 5th: no room) */
    bad += check("#155 ADD: keys one after another build the cursor step's chord (up to 4), the cursor stays", ok);
    tap_k(11);
    ok = t->step[3].n == 3u && t->step[3].note[0] == kb_map(t, 7) && t->step[3].note[1] == kb_map(t, 14) &&
         t->step[3].note[2] == kb_map(t, 16) && ui.cursor == 3u;
    tap_k(18);
    ok &= t->step[3].n == 4u && t->step[3].note[3] == kb_map(t, 18);
    bad += check("#155 ADD: a key whose note is there takes it out (the others keep their order); then there is room", ok);
    turn(EN_K1, 2);
    tap_k(9);
    ok = ui.cursor == 5u && t->step[5].n == 2u && t->step[5].note[0] == 50u && t->step[5].note[1] == kb_map(t, 9) &&
         t->step[3].n == 4u && !t->step[4].n;
    tap_k(9);
    ok &= t->step[5].n == 1u && t->step[5].note[0] == 50u && t->step[5].time == ST_NOTE;
    turn(EN_K1, -1);
    tap_k(9); tap_k(9);
    ok &= ui.cursor == 4u && !t->step[4].n && t->step[4].time == ST_REST;
    bad += check("#155 ADD: KNOB 1 moves on; a written step gets notes beside its own; the last one out leaves a REST", ok);
    t->step[6].time = ST_TIE;
    turn(EN_K1, 2);
    tap_k(7);
    ok = ui.cursor == 6u && t->step[6].time == ST_NOTE && t->step[6].n == 1u;
    t->p[P_VOICE] = V_MONO;
    turn(EN_K1, 1);
    tap_k(7); tap_k(9);
    ok &= ui.cursor == 7u && t->step[7].n == 1u && t->step[7].note[0] == kb_map(t, 9);
    tap_k(9);
    ok &= !t->step[7].n;
    bad += check("#155 ADD: a TIE step becomes a note step; MONO keeps one note (another key replaces it, the same takes it out)", ok);
    t->p[P_VOICE] = V_POLY;
    turn(EN_K1, 1);
    key_down(7); frame(); key_down(11); frame();       /* keys held together: each adds */
    key_up(7); key_up(11); frame();
    ok = ui.cursor == 8u && t->step[8].n == 2u;
    press(B_EDIT);
    ok &= !t->step[8].n && ui.cursor == 9u;             /* (EDIT still clears the step and moves on) */
    bad += check("#155 ADD: keys held together add each; EDIT clears the step and moves on as before", ok);
    song.rec = 0;
    memcpy(before, t->step, sizeof before);
    tap_k(7);
    bad += check("#155 ADD, not armed: the keys only play, nothing written", !memcmp(before, t->step, sizeof before));
    menu_put(MI_CHADD, 0);
    return bad;
}

static int test_piano_roll(void)
{
    int bad = 0, ok;
    uint32_t i, nb = 0, lo0, steps = 0;
    track_t *t;
    ui_power_on();
    t = TSEL;
    load_pat16(t, PATTERNS[0].note, PATTERNS[0].flags);               /* ACID: A2..A3, ties, accents, slides */
    root_g = 9; scale_g = 2;
    go_page(GR_ROLL); ui.cursor = 0; ui.bank = 0;
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    ok = pr_columns_match(t, &nb);
    bad += check("piano roll: ACID's bars drawn at their notes' rows, ties carried, rests empty", ok && nb >= 10u);
    bad += check("  the view holds the page's notes (A2..A3 in 21 rows), MONO gray", proll.lo <= 45 && proll.lo + PR_ROWS - 1 >= 57 &&
                 screen_gray());
    t->p[P_VOICE] = V_POLY;                                           /* chords: up to 4 bars in a column */
    for (i = 0; i < 16u; i += 4u) {
        step_t *s = &t->step[i];
        s->time = ST_NOTE; s->n = 4; s->note[0] = 57; s->note[1] = 60; s->note[2] = 64; s->note[3] = 67;
        t->step[i + 1u].time = ST_TIE;
    }
    nb = 0;
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    bad += check("  4-note chords stacked, their ties carry all four", pr_columns_match(t, &nb));
    for (i = 0; i < NSTEP; i++) {                                     /* LEN 32, page 2 */
        step_clear(&t->step[i]);
        if (i >= 16u && i % 2u == 0u) { t->step[i].time = ST_NOTE; t->step[i].n = 1; t->step[i].note[0] = (uint8_t)(72 + i % 7u); }
    }
    t->p[P_SLEN] = 32; cursor_set(18);
    nb = 0;
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    bad += check("  LEN 32, the cursor on page 2: its 16 steps drawn", ui.bank == 1u && pr_columns_match(t, &nb) && nb == 8u);
    lo0 = proll.lo;                                                   /* two octaves up: the view follows in steps */
    for (i = 16; i < 32u; i++) if (t->step[i].n) t->step[i].note[0] += 24;
    for (i = 0; i < 20u && proll.lo != lo0 + 24u; i++) { frame(); steps++; }
    bad += check("  notes moved two octaves: the view follows over frames (not at once), then rests",
                 proll.lo == lo0 + 24u && steps >= 3u && steps < 20u);
    {
        uint32_t sig = ui.graph_sig;
        frame(); frame();
        bad += check("  nothing changed: the graph is not drawn again", ui.graph_sig == sig);
    }
    nb = 0;
    bad += check("  after the follow the bars match again", pr_columns_match(t, &nb));
    song.rec = 1;                                                     /* (#133: armed, the key writes the step) */
    key_down(7); frames(200);                                         /* a key held: its row lit on the strip */
    {
        int32_t r = (int32_t)proll.lo + PR_ROWS - 1 - (int32_t)kb_chord[7][0];
        uint16_t c = r >= 0 && r < PR_ROWS ? swap16(host_screen[(Y_GRAPH + PR_Y0 + (uint32_t)r * PR_RH + 1u) * 240u + PR_KX + 7]) : 0;
        bad += check("  a key held: its row on the keyboard strip is the accent", kb_chn[7] && c == T_ACCENT);
    }
    key_up(7); frames(100);
    song.rec = 0;
    return bad;
}

/* bug fixes, second round (UI) */
static uint32_t accent_in(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    uint32_t n = 0;
    int32_t x, y;
    uint16_t a = (uint16_t)((T_ACCENT >> 8) | (T_ACCENT << 8));
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            n += host_screen[y * 240 + x] == a;
    return n;
}
static int test_bughunt_ui2(void)
{
    int bad = 0;
    uint32_t i;
    {   /* 1. a layer button pressed under a dialog and still held after it closes: dead, its keys stay notes */
        static const uint32_t BTN[2] = {B_SCL, B_GLO};
        for (i = 0; i < 2u; i++) {
            uint32_t k = white(0), ok;
            ui_power_on(); TSEL->p[P_VOICE] = V_POLY;
            go_main_to(HP_MORPH); frame();
            confirm_open(CF_CLEAR_SEQ, 0); frame();
            btn_down(BTN[i]); frame(); frames(100);
            btn_down(B_OCTDN); frame(); btn_up(B_OCTDN); frame();   /* the dialog closes, the button still held */
            frames(800);
            ok = !ui.confirm && (ui.ly_t0 & LY_DEAD) && !ui.layer && !kb_mask;
            key_down(k); frame();
            ok = ok && kb_note[k] != KB_SILENT && gates() == 1u && !((kb_layer >> k) & 1u) && !ui.layer;
            key_up(k); frame();
            btn_up(BTN[i]); frame();
            bad += check(i ? "dead GLO held from a dialog: no map, the key plays a note"
                           : "dead SCL held from a dialog: no map, the key plays a note", ok);
        }
    }
    {   /* 2. STEP entry with CHRD on writes what the key sounds, as live recording does */
        static const uint32_t KEY[2] = {7u, 8u};                      /* C4, C#4 (out of C major) */
        uint32_t v, j, ok = 1;
        for (v = 0; v < 2u; v++)
            for (j = 0; j < 2u; j++) {
                track_t *t;
                step_t e, r;
                ui_power_on(); t = TSEL; t->p[P_VOICE] = v ? V_MONO : V_POLY; t->p[P_CHRD] = CH_DIA3; scale_g = 1; root_g = 0;
                track_defaults_steps(t); go_page(GR_ROLL); cursor_set(0); frame(); song.rec = 1;   /* (#133: armed) */
                key_down(KEY[j]); frame();
                e = t->step[0];
                key_up(KEY[j]); frame(); song.rec = 0;
                ok &= e.n == (v ? 1u : 3u) && e.note[0] == 60u && (v || (e.note[1] == 64u && e.note[2] == 67u));
                ui_power_on(); t = TSEL; t->p[P_VOICE] = v ? V_MONO : V_POLY; t->p[P_CHRD] = CH_DIA3; scale_g = 1; root_g = 0;
                track_defaults_steps(t); go_main_to(HP_MORPH); frame();
                song.rec = 1; song.playing = 1; t->seq_idx = 3; t->seq_pos = 0;
                fm1_in.notes = 1u << KEY[j]; keyboard_block();
                r = t->step[3];
                fm1_in.notes = 0; keyboard_block();
                song.rec = 0; song.playing = 0;
                ok &= r.n == e.n && !memcmp(r.note, e.note, e.n);
            }
        bad += check("STEP entry with CHRD: POLY the chord, MONO its root, as live recording writes", ok);
    }
    {   /* 3. MIXER: the knob just turned is the one drawn in the accent (K1 LEVEL, K2 PAN, K3 REV, K4 the MUTE badge) */
        uint32_t k, a[4][4], ok = 1;
        for (k = 0; k < 4u; k++) {
            int32_t X = CARD_X(0), Y = Y_GRAPH;
            ui_power_on();
            settings.palette = 1; palette_set(1);
            go_page(GR_TRK); frame(); frame();
            turn(EN_K1 + k, 1);                                       /* (K4 right: MUTE on, its badge shown) */
            ui.force = 1; frame();
            a[k][0] = accent_in(X + 4, Y + 37, X + 38, Y + 71);       /* LEVEL */
            a[k][1] = accent_in(X + 3, Y + 84, X + 28, Y + 115);      /* PAN */
            a[k][2] = accent_in(X + 30, Y + 84, X + 55, Y + 115);     /* REV */
            a[k][3] = accent_in(X + 22, Y + 4, X + 56, Y + 20);       /* the MUTE badge */
        }
        for (k = 0; k < 4u; k++) {
            uint32_t j;
            for (j = 0; j < 4u; j++)
                ok &= j == k ? a[k][j] > 0u : a[k][j] == 0u;
        }
        bad += check("MIXER: K1..K4 light LEVEL / PAN / REV / the MUTE badge, nothing else", ok);
    }
    {   /* 4. renaming the slot the music came from renames the music: the next SAVE prefills the new name */
        char pn[16];
        uint32_t ok;
        ui_power_on();
        go_page(GR_SLOTS); song.g[G_SLOT] = 2;
        project_save_as(1, "OLD");
        project_save_as(2, "OTHER");
        project_load(1); frame();
        ok = str_eq(proj_name, "OLD");
        project_rename(2, "ELSE");
        ok &= str_eq(proj_name, "OLD");                               /* another slot: the music's name stays */
        press(B_EDIT);                                                /* rename B on the slot list */
        while (nm.len) { nm_do(NB_DEL); }
        nm_white(13); frames(900);
        str_cpy(nm.s, "NEW", sizeof nm.s); nm.len = nm.cur = 3; nm.key = 0;
        press(B_OCTUP);
        project_name(1, pn);
        ok &= str_eq(pn, "NEW") && str_eq(proj_name, "NEW");
        turn(EN_K4, 1); press(B_OCTUP);                               /* SAVE to B: OVERWRITE? */
        press(B_OCTUP);                                               /* YES: NAME */
        ok &= name_on() && str_eq(nm.s, "NEW");
        press(B_OCTDN);
        bad += check("rename of the loaded slot: the music's name follows, SAVE prefills it", ok);
    }
    {   /* 5. NAME: PLAY stops a transport started meanwhile (so the name can be saved), never starts it; REC ignored */
        uint32_t ok;
        ui_power_on(); go_page(GR_USER); ui_old_uslot = 3; press(B_OCTUP);
        ok = name_on();
        press(B_PLAY); frame();
        ok &= !transport_req && !song.playing && name_on();          /* stopped: PLAY starts nothing */
        press(B_REC); frame();
        ok &= !song.rec && !transport_req && name_on();
        song.playing = 1;                                             /* an external MIDI Start / the editor */
        press(B_OCTUP);
        ok &= name_on() && msg_is("STOP TO SAVE");
        press(B_REC); frame();
        ok &= !song.rec && !transport_req;
        press(B_PLAY); frame();
        ok &= transport_req == 2u && name_on();                       /* PLAY stops it, NAME stays */
        stop_transport();
        press(B_OCTUP);
        ok &= !name_on() && up_used(3);
        bad += check("NAME: PLAY stops a running transport (never starts), REC ignored, then OCT+ saves", ok);
    }
    {   /* 6. the piano roll's lowest C name ("C-1") stays inside the panel: nothing drawn left of x 3 */
        uint32_t p, x, y, out = 0;
        uint16_t bg;
        for (p = 0; p < NPALETTES; p++) {
            track_t *t;
            uint8_t lo = 0, hi = 127;
            ui_power_on(); t = TSEL; track_defaults_steps(t); t->p[P_SLEN] = 16;
            settings.palette = (uint8_t)p; palette_set(p);
            t->step[0].time = t->step[5].time = ST_NOTE; t->step[0].n = t->step[5].n = 1;
            t->step[0].note[0] = lo; t->step[5].note[0] = hi;
            go_page(GR_ROLL); cursor_set(0); ui.force = 1; frame(); frame();
            bg = (uint16_t)((T_BG >> 8) | (T_BG << 8));
            for (y = Y_GRAPH; y < Y_GRAPH + H_GRAPH; y++)
                for (x = 0; x < 3u; x++)
                    out += host_screen[y * 240u + x] != bg;
        }
        bad += check("piano roll: the C-1 label stays inside the panel (every palette)", !out);
    }
    {   /* 7. EDIT: INIT is drawn in the first cell (F3), and that white key is the one that inits */
        uint32_t p, ok = LY_INIT == 0u;                                /* (layer_edit: INIT in cell 0) */
        for (p = 0; p < 16u; p++) {
            ui_power_on(); go_main_to(HP_MORPH); frame();
            btn_down(B_EDIT); key_down(white(p)); frame(); frames(50);
            ok &= (ui.confirm == CF_INIT_SOUND) == (p == LY_INIT);
            key_up(white(p)); btn_up(B_EDIT); frame();
        }
        bad += check("EDIT: INIT's map cell (the first, F3) and its key agree", ok);
    }
    return bad;
}

#ifndef UI_TEST_NO_MAIN
/* L is a sparse face (tools/gen_aa_font.py L_CHARS): every string drawn in it has all its glyphs */
static int test_large_face(void)
{
    static const char *const FIXED[] = {"FELUCCA", "0123456789"};   /* main.c, ui_menu.c; ui_draw.c draw_uboot */
    uint32_t i, missing = 0;
    const char *s;
    for (i = 0; i < NB + NE + 2u; i++)                              /* ui_input.c setup_show: the control names */
        for (s = i < NB ? B_NAME[i] : i < NB + NE ? E_NAME[i - NB] : FIXED[i - NB - NE]; *s; s++)
            missing += glyph_at(&AF_L, (uint8_t)*s) < 0;
    for (i = 0; i < GUIDE_N; i++)                                   /* DUELL 0.2.11: the GUIDE's headings ("#..." lines, ui_guide.c) */
        for (s = GUIDE_TXT[i]; *s; s++)
            if (*s == '#' && (s == GUIDE_TXT[i] || s[-1] == '\n'))
                for (s++; *s && *s != '\n'; s++)
                    missing += glyph_at(&AF_L, (uint8_t)*s) < 0;
    return check("the L face holds every glyph of the strings drawn in it", !missing);
}

/* The FM6 algorithm charts (ui_graph.c graph_fm6: FM6_CELL, its routes fm6_routes and feedback operator
 * fm6_fb_op read from fm6_core.c FM6_ALG) against the 32 algorithms of 6-operator FM written out here as
 * "source>destination" routes, the carriers and the operator with feedback (fm6_core.c's: on algorithms 4 and
 * 6 the loop is the sixth operator's own, as msfa plays it). Also against the core itself: the carriers
 * fm6_carriers (fm6_car_ops), the feedback flags. The layout: the carriers on the bottom row and only they, a modulator one
 * row above everything it modulates, one operator per cell, columns 0..5, at most 4 rows. (The charts as drawn:
 * tests/ui_render.c's lint, fm6_lint.) */
static int test_fm6_charts(void)
{
    static const struct { const char *r, *car; char fb; } ALGS[32] = {
        {"2>1 4>3 5>4 6>5", "13", '6'}, {"2>1 4>3 5>4 6>5", "13", '2'}, {"2>1 3>2 5>4 6>5", "14", '6'},
        {"2>1 3>2 5>4 6>5", "14", '6'}, {"2>1 4>3 6>5", "135", '6'}, {"2>1 4>3 6>5", "135", '6'},
        {"2>1 4>3 5>3 6>5", "13", '6'}, {"2>1 4>3 5>3 6>5", "13", '4'}, {"2>1 4>3 5>3 6>5", "13", '2'},
        {"2>1 3>2 5>4 6>4", "14", '3'}, {"2>1 3>2 5>4 6>4", "14", '6'}, {"2>1 4>3 5>3 6>3", "13", '2'},
        {"2>1 4>3 5>3 6>3", "13", '6'}, {"2>1 4>3 5>4 6>4", "13", '6'}, {"2>1 4>3 5>4 6>4", "13", '2'},
        {"2>1 3>1 5>1 4>3 6>5", "1", '6'}, {"2>1 3>1 5>1 4>3 6>5", "1", '2'}, {"2>1 3>1 4>1 5>4 6>5", "1", '3'},
        {"2>1 3>2 6>4 6>5", "145", '6'}, {"3>1 3>2 5>4 6>4", "124", '3'}, {"3>1 3>2 6>4 6>5", "1245", '3'},
        {"2>1 6>3 6>4 6>5", "1345", '6'}, {"3>2 6>4 6>5", "1245", '6'}, {"6>3 6>4 6>5", "12345", '6'},
        {"6>4 6>5", "12345", '6'}, {"3>2 5>4 6>4", "124", '6'}, {"3>2 5>4 6>4", "124", '3'},
        {"2>1 4>3 5>4", "136", '5'}, {"4>3 6>5", "1235", '6'}, {"4>3 5>4", "1236", '5'},
        {"6>5", "12345", '6'}, {"", "123456", '6'}};
    uint32_t a, k, j, routes = 1, layout = 1, core = 1;
    for (a = 0; a < 32u; a++) {
        uint8_t m[6], want[6] = {0, 0, 0, 0, 0, 0};
        uint32_t car = 0, ok = 1, fbs = 0;
        const char *s;
        for (s = ALGS[a].r; *s; s++)
            if (*s == '>') want[s[1] - '1'] |= (uint8_t)(1u << (s[-1] - '1'));
        for (s = ALGS[a].car; *s; s++) car |= 1u << (*s - '1');
        fm6_routes(a, m);
        if (memcmp(m, want, 6) || fm6_fb_op(a) != (uint32_t)(ALGS[a].fb - '1')) {
            printf("ui: FM6 algorithm %u: the chart's routes or feedback differ from the written ones\n", a + 1u);
            routes = 0;
        }
        for (k = 0; k < 6u; k++)                                    /* the core: who writes the output, who has FB */
            fbs += (FM6_ALG[a][k] & 0xC0u) == 0xC0u;
        if (fm6_car_ops(a) != car || fbs != 1u) {
            printf("ui: FM6 algorithm %u: fm6_core.c's carriers / feedback differ\n", a + 1u);
            core = 0;
        }
        for (k = 0; k < 6u; k++) {
            uint32_t row = FM6_CELL[a][k] >> 4, outs = 0;
            if ((row == 0u) != ((car >> k) & 1u) || (FM6_CELL[a][k] & 0x0Fu) > 5u || row > 3u) ok = 0;
            for (j = 0; j < 6u; j++) {
                if (j != k && FM6_CELL[a][j] == FM6_CELL[a][k]) ok = 0;
                if (m[j] >> k & 1u) { outs++; if (FM6_CELL[a][k] >> 4 != (FM6_CELL[a][j] >> 4) + 1u) ok = 0; }
            }
            if (!outs && !((car >> k) & 1u)) ok = 0;               /* every operator heard */
        }
        if (!ok) { printf("ui: FM6 algorithm %u: the chart's layout is off\n", a + 1u); layout = 0; }
    }
    return check("FM6 charts: the 32 algorithms' routes, carriers and feedback operator equal fm6_core.c's", routes && core) +
           check("FM6 charts: carriers on the bottom row, modulators a row up, one per cell, at most 4 rows", layout);
}

/* DUELL WAVE (engine 14): its favourites live in row 1 (DIGITAL's, retired) bytes 16..31, never in row 14 (the MIDI LEARN map) or
 * DIGITAL's byte 0 (which the settings import clears); a setting saved and read back keeps them */
static int test_wave_favorites(void)
{
    persist_t p;
    uint8_t ml[32], dg0;
    int ok;
    ui_power_on();
    memcpy(ml, favorites.factory[14], sizeof ml);
    dg0 = favorites.factory[1][0];
    ok = !favorite_has(ENGI_WAVE, 0) && favorite_set(ENGI_WAVE, 0, 1) && favorite_set(ENGI_WAVE, 9, 1) && favorite_has(ENGI_WAVE, 0) &&
         favorite_has(ENGI_WAVE, 9) && !favorite_has(ENGI_WAVE, 1) && !favorite_set(ENGI_WAVE, 128, 1);
    ok &= !memcmp(ml, favorites.factory[14], sizeof ml) && favorites.factory[1][0] == dg0 &&
          favorites.factory[1][16] == 1u && favorites.factory[1][17] == 2u;
    settings_export(&p);
    memset(&favorites, 0, sizeof favorites);
    ok &= settings_import(&p, sizeof p) == 1 && favorite_has(ENGI_WAVE, 0) && favorite_has(ENGI_WAVE, 9);
    ok &= favorite_set(ENGI_WAVE, 0, 0) && favorite_set(ENGI_WAVE, 9, 0) && !favorites.factory[1][16] &&
          !favorites.factory[1][17];
    return check("WAVE's favourites (engine 14): row 1 bytes 16..31, the MIDI LEARN map and DIGITAL's byte untouched, kept through a settings save", ok);
}

static int test_engines_shown(void)
{
    int bad = 0, ok = 1;
    uint32_t i, k, e, total, seen = 0, all = ((1u << NENGINES) - 1u) & ~((1u << 1) | (1u << 4) | (1u << 8) | (1u << 10) | (1u << 13));   /* (retired: DIGITAL SAMPLE GRAIN DRUM SLICE) */
    int16_t p[P_COUNT];
    uint8_t v[FP_SIZE + 1u];
    ui_power_on();
    bad += check("power-on: track 2 is FM6 PAD (TRK_DEF, was DIGITAL PAD)", trk[1].eng_req == ENGI_FM6 &&
                 str_eq(ENGINES[ENGI_FM6]->presets[trk[1].preset].name, "PAD"));
    for (i = 0; i < UP_SLOTS; i++)                      /* (0.2.17: the list is the 128 slots, a fresh unit shows the factory sounds in all) */
        seen |= 1u << sl_engine(i);
    total = UP_SLOTS;
    bad += check("PRESETS: the list holds every engine's presets but the retired DIGITAL SAMPLE GRAIN DRUM SLICE",
                 seen == all && NENG_SHOWN == NENGINES - 5u);
    for (i = 0, seen = 0, e = 0; i < NENG_SHOWN; i++) {   /* (DUELL: PATCHES' KNOB 2 is TYPE; the engine step is the table's) */
        e = eng_step(e, 1);
        seen |= 1u << e;
    }
    bad += check("engine step: the engines in order, DIGITAL, DRUM and the retired ones skipped, back to the first",
                 seen == all && e == 0u && eng_step(0, 1) == ENGI_FM6 && eng_step(ENGI_FM6, 1) == 2u &&
                 eng_step(ENGI_FM6, -1) == 0u && eng_step(0, -1) == eng_vis(NENG_SHOWN - 1u));
    {   /* the display order (engines.c ENGINE_ORDER): every engine one can pick once; the PRESETS list follows it */
        static const char *const ORDER[] = {"ANALOG", "FM6", "PHASE", "LOFI", "VOICE", "TRIO", "WHEEL", "PHYS", "NOISE", "WAVE"};
        uint32_t r = 0, n = 0;
        ok = NENG_SHOWN == NELEM(ORDER);
        for (i = 0; ok && i < NENG_SHOWN; i++)
            ok &= str_eq(ENGINES[eng_vis(i)]->name, ORDER[i]) && eng_rank(eng_vis(i)) == i && eng_ok(eng_vis(i));
        for (i = 0; i < UP_SLOTS; i++) {               /* (DUELL: the PRESETS list is sorted by category across engines, not by engine) */
            e = sl_engine(i);
            if (e < NENGINES && !(r & (1u << e))) {
                r |= 1u << e;
                n++;
            }
        }
        bad += check("engines shown ANALOG FM6 PHASE LOFI VOICE TRIO WHEEL PHYS NOISE WAVE (ENGINE_ORDER; DIGITAL, DRUM hidden; SAMPLE GRAIN SLICE retired); the PRESETS list holds each",
                     ok && r == all && n == NENG_SHOWN);
    }
    /* a user preset stored with a retired engine (DIGITAL, 1): kept as it is (no conversion since 0.2.14), it loads onto the silent placeholder */
    {
        up_rec_t r;
        memset(&r, 0, sizeof r);
        r.used = UP_USED;
        r.ver = UP_VER;
        r.engine = 1;
        r.np = P_COUNT;
        memcpy(r.name, "OLD PAD", 7);
        for (i = 0; i < P_COUNT; i++)
            up_set_value(&r, i, param_desc_of(0, i)->def);
        up_put(7, &r);
    }
    up_load(7);
    bad += check("a DIGITAL saved patch (retired): kept in the bank, loads onto the placeholder engine 1 (silent, NO ENGINE), not converted",
                 TSEL->eng_req == 1u && up_rec(7)->engine == 1u && TSEL->user == 8u && ENGINES[1]->npresets == 0u);
    set_engine_of(TSEL, 1);
    bad += check("engine 1 asked for (the editor's ENG, INIT on such a part): ANALOG, its first sound", TSEL->eng_req == 0u && TSEL->preset == 0u);
    {   /* favourites of DIGITAL presets (settings PER4): cleared on import, not moved */
        persist_t pe;
        memset(&pe, 0, sizeof pe);
        pe.magic = PERSIST_MAGIC;
        pe.panel = PANEL_DEFAULT;
        pe.favorites.factory[1][0] = 1u << 5 | 1u << 7;
        settings_import(&pe, (int)sizeof pe);
        bad += check("favourites of DIGITAL presets: engine 1's byte cleared, none moved to FM6", !favorites.factory[1][0] &&
                     !favorite_has(ENGI_FM6, 4) && !favorite_has(ENGI_FM6, 7));
        memset(&favorites, 0, sizeof favorites);
    }
    return bad;
}

/* #38: the keys of the notes the selected track's sequencer and ARP sound light while it plays */
static int test_play_leds(void)
{
    int bad = 0, ok;
    track_t *t = &trk[0];
    uint32_t i;
    ui_power_on();
    song.sel = 0;
    t->step[0] = (step_t){.note = {60, 64}, .n = 2, .time = ST_NOTE};
    t->step[1] = (step_t){.note = {100}, .n = 1, .time = ST_NOTE};
    trk[1].step[0] = (step_t){.note = {67}, .n = 1, .time = ST_NOTE};
    t->seq_active = trk[1].seq_active = 1;
    bad += check("stopped, nothing held: no key lit", key_leds(0) == 0u && play_leds() == 0u);
    seq_start();
    events_block(CTL);
    bad += check("playing: C4 and E4 of step 1 light keys 8 and 12 (from F3), track 2's G4 does not",
                 t->seq_n == 2u && trk[1].seq_n == 1u && key_leds(0) == (1u << 7 | 1u << 11));
    song.sel = 1;
    bad += check("the selected track's notes: track 2's G4 on key 15", play_leds() == 1u << 14);
    song.sel = 0;
    fm1_in.notes = 1u << 0;
    bad += check("a key held still lights with them", key_leds(0) == (1u << 0 | 1u << 7 | 1u << 11));
    fm1_in.notes = 0;
    song.octave = -1;
    ok = play_leds() == (1u << 19 | 1u << 23);
    song.octave = 1;
    ok &= play_leds() == 0u;                           /* (C4 and E4 below the keys an octave up) */
    song.octave = 0;
    t->p[P_TRANS] = 2;
    ok &= play_leds() == (1u << 5 | 1u << 9);
    t->p[P_TRANS] = 0;
    bad += check("the keys follow the octave and TRN; notes off the keyboard are not shown", ok);
    offscale_g = OS_SNAP;                              /* SNAP in C major: C#4 gives C4 too (a tie goes down) */
    scale_g = 1;
    root_g = 0;
    ok = kb_map(t, 8) == 60u && play_leds() == (1u << 7 | 1u << 11);
    scale_g = 0;
    bad += check("OFF-SCALE SNAP: only the key of the note itself lights, not the keys snapping onto it", ok);
    t->arp_note = 65;
    bad += check("the ARP's note lights its key too", play_leds() == (1u << 7 | 1u << 11 | 1u << 12));
    t->arp_note = 0;
    events_block(div_samples((uint32_t)t->p[P_SDIV]) + CTL);
    bad += check("a note above the keyboard (G#7) lights nothing", t->seq_n == 1u && t->seq_notes[0] == 100u &&
                 play_leds() == 0u);
    transport_req = 2;
    events_block(CTL);
    bad += check("stopped: the notes end, their keys go dark", !song.playing && key_leds(0) == 0u);
    /* the layer keeps its keys */
    seq_start();
    events_block(CTL);
    ui.layer = 1;
    ok = key_leds(0) == layer_leds(0);
    ui.layer = 0;
    bad += check("the layer's map keeps its keys", ok);
    transport_req = 2; events_block(CTL);
    return bad;
}

/* Discussion #89: on SEQ > STEP (the piano roll) while stopped the keys of the cursor step's notes light, at the
 * octave now (the lowest key that gives each); with the keys held; playing, what plays instead (#38); not on
 * other pages, not for a REST step */
static int test_cursor_step_leds(void)
{
    int bad = 0, ok;
    track_t *t;
    ui_power_on();
    song.sel = 0;
    t = TSEL;
    t->step[0] = (step_t){.note = {60, 64}, .n = 2, .time = ST_NOTE};
    t->step[1] = (step_t){.note = {100}, .n = 1, .time = ST_NOTE};
    t->step[2] = (step_t){.note = {62}, .n = 1, .time = ST_REST};
    t->step[3] = (step_t){.note = {67}, .n = 1, .time = ST_TIE};
    t->seq_active = 1;
    bad += check("#89 HOME, stopped: no step's keys lit", key_leds(0) == 0u);
    go_page(GR_ROLL);
    frame();
    bad += check("#89 STEP, stopped: the cursor step's C4 and E4 light keys 8 and 12",
                 ui.cursor == 0u && key_leds(0) == (1u << 7 | 1u << 11));
    song.octave = -1;
    ok = key_leds(0) == (1u << 19 | 1u << 23);
    song.octave = 1;
    ok &= key_leds(0) == 0u;                            /* (below the keys an octave up) */
    song.octave = 0;
    bad += check("#89 the step's keys follow the octave; notes off the keyboard are not shown", ok);
    fm1_in.notes = 1u << 0;
    bad += check("#89 a key held lights with them", key_leds(0) == (1u << 0 | 1u << 7 | 1u << 11));
    fm1_in.notes = 0;
    turn(EN_K1, 1);
    ok = ui.cursor == 1u && key_leds(0) == 0u;          /* G#7: off the keyboard */
    turn(EN_K1, 1);
    ok &= ui.cursor == 2u && key_leds(0) == 0u;         /* a REST */
    turn(EN_K1, 1);
    ok &= ui.cursor == 3u && key_leds(0) == 1u << 14;   /* a TIE carries its G4 */
    turn(EN_K1, 1);
    ok &= ui.cursor == 4u && key_leds(0) == 0u;         /* empty */
    bad += check("#89 KNOB 1 moves the cursor and the keys follow its step (REST, empty, off the keys: dark)", ok);
    turn(EN_K1, -4);
    seq_start();
    events_block(CTL);
    bad += check("#89 playing: the keys show what plays, not the cursor step", song.playing && ui.cursor == 0u &&
                 key_leds(0) == play_leds());
    turn(EN_K1, 3);
    bad += check("#89   (the cursor on G4's step changes nothing while playing)", key_leds(0) == play_leds() &&
                 !((key_leds(0) >> 14) & 1u));
    transport_req = 2;
    events_block(CTL);
    frame();
    bad += check("#89 stopped again: the cursor step's keys", !song.playing && key_leds(0) == 1u << 14);
    go_page(GR_EVENTS);
    frame();
    bad += check("#89 AUTOMATION (same cursor): no step keys", key_leds(0) == 0u);
    return bad;
}

/* Discussion #81: the notes MIDI IN holds on the selected track light their keys as the sequencer's do: USB and TRS,
 * routed by ROUT; at the octave now, the lowest key; off the keyboard nothing; the pedal holds them; the layer's map
 * keeps its keys */
static void usb_note(uint32_t ch, uint32_t note, uint32_t vel)   /* a USB-MIDI packet, as usb.c queues it */
{
    uint32_t st = (vel ? 0x90u : 0x80u) | ch;
    midi_in_event((st >> 4) | st << 8 | note << 16 | vel << 24);
    events_block(CTL);
}
static void trs_note(uint32_t a, uint32_t b, uint32_t c)        /* TRS MIDI IN: midi_uart.c's parser */
{
    um_byte(a); um_byte(b); um_byte(c);
    events_block(CTL);
}
static int test_midi_leds(void)
{
    int bad = 0, ok;
    ui_power_on();
    memset(&um, 0, sizeof um);
    song.sel = 0;
    usb_note(0, 60, 100);                              /* channel 1 -> T1 (ROUT CH1-4) */
    bad += check("#81 USB MIDI C4 into the selected track lights key 8 (C4)", midi_sel_on[0][60] == 1u &&
                 key_leds(0) == 1u << 7 && play_leds() == 1u << 7);
    usb_note(1, 67, 100);                              /* channel 2 -> T2: not the selected track */
    ok = key_leds(0) == 1u << 7;
    song.sel = 1;
    ok &= key_leds(0) == 1u << 14;
    song.sel = 0;
    bad += check("  another track's MIDI notes light nothing; selected, its G4 lights key 15", ok);
    usb_note(1, 67, 0);
    usb_note(0, 60, 0);
    bad += check("  note-off: dark", key_leds(0) == 0u && !midi_owners[0]);
    trs_note(0x90, 64, 90);                           /* TRS: E4 on channel 1 */
    ok = key_leds(0) == 1u << 11;
    trs_note(0x80, 64, 0);
    bad += check("  TRS MIDI IN: E4 lights key 12 while held", ok && key_leds(0) == 0u);
    song.g[G_ROUTE] = 1;                               /* ROUT SEL: every channel to the selected track */
    song.sel = 2;
    usb_note(9, 62, 100);
    ok = midi_sel_on[9][62] == 3u && key_leds(0) == 1u << 9;
    usb_note(9, 62, 0);
    song.g[G_ROUTE] = 0;
    song.sel = 0;
    bad += check("  ROUT SEL: channel 10 into the selected track lights its key", ok && key_leds(0) == 0u);
    usb_note(0, 60, 100);
    song.octave = -1;
    ok = key_leds(0) == 1u << 19;
    song.octave = 1;
    ok &= key_leds(0) == 0u;                            /* (C4 below the keys an octave up) */
    song.octave = 0;
    usb_note(0, 60, 0);
    usb_note(0, 100, 100);                             /* G#7: above the keyboard */
    usb_note(0, 40, 100);                              /* E2: below it */
    ok &= key_leds(0) == 0u && midi_owners[0] == 2u;
    usb_note(0, 100, 0);
    usb_note(0, 40, 0);
    bad += check("  the keys follow the octave; notes off the keyboard are not shown", ok);
    offscale_g = OS_SNAP;                              /* SNAP in C major: C#4 gives C4 too (a tie goes down) */
    scale_g = 1;
    root_g = 0;
    usb_note(0, 60, 100);
    ok = kb_map(&trk[0], 8) == 60u && key_leds(0) == 1u << 7;
    usb_note(0, 60, 0);
    scale_g = 0;
    bad += check("  OFF-SCALE SNAP: only the lowest key giving the note lights", ok && key_leds(0) == 0u);
    midi_event(0xB0, 0, 64, 127);                      /* the pedal down: a released note still sounds */
    usb_note(0, 65, 100);
    usb_note(0, 65, 0);
    ok = key_leds(0) == 1u << 12;
    midi_event(0xB0, 0, 64, 0);
    bad += check("  a note the pedal holds stays lit until the pedal is up", ok && key_leds(0) == 0u);
    fm1_in.notes = 1u << 0;                            /* with a key held and the sequencer */
    usb_note(0, 60, 100);
    ok = key_leds(0) == (1u << 0 | 1u << 7);
    fm1_in.notes = 0;
    trk[0].step[0] = (step_t){.note = {64}, .n = 1, .time = ST_NOTE};
    trk[0].seq_active = 1;
    seq_start();
    events_block(CTL);
    ok &= key_leds(0) == (1u << 7 | 1u << 11);
    bad += check("  with a key held and the sequencer's notes: all of them lit", ok);
    ui.layer = LAYER_SCL;
    {
        uint32_t b1, b2;
        ok = key_leds(&b1) == layer_leds(&b2) && b1 == b2 && b1;
    }
    ui.layer = 0;
    open_family(FAM_SEQ);                              /* STEP on a melodic track: the keys as elsewhere */
    frame();
    ok &= (key_leds(0) & (1u << 7)) != 0u;
    bad += check("  the layer's map keeps its keys over the MIDI notes", ok);
    transport_req = 2;
    events_block(CTL);
    usb_note(0, 60, 0);
    return bad;
}

/* #119: what can be pressed breathes (hal/fm1_input.h fm1_led_breath: dark .. ~60 % of lit), never a hard blink: every
 * layer, held and locked (double tap), in every MENU > LEDS mode: its keys that can be pressed and its button in the
 * breath set only (neither lit nor the glow), the ones in effect lit, the picture the same at any time (no 250 ms
 * toggle left); OCT+ in a dialog likewise; closing the layer leaves nothing breathing; the glow's level and
 * the breath's peak follow LEDS (fm1_led_dim_level: LOW the darker glow and ~30 %, else MEDIUM's and ~60 %) */
static uint32_t breath_keys(void)                     /* the breathing keys, bit k = key k (the real positions) */
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++) {
        uint8_t q = led_pos[14u + k];
        m |= (uint32_t)(q != 0xFF && ((fm1_led_breath[q >> 3] >> (q & 7u)) & 1u)) << k;
    }
    return m;
}
static uint32_t lit_keys(const uint8_t *l)
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++) {
        uint8_t q = led_pos[14u + k];
        m |= (uint32_t)(q != 0xFF && ((l[q >> 3] >> (q & 7u)) & 1u)) << k;
    }
    return m;
}
static int led_of(const uint8_t *l, uint32_t id)
{
    uint8_t q = led_pos[id];
    return q != 0xFF && ((l[q >> 3] >> (q & 7u)) & 1u);
}
static int test_breath(void)
{
    static const uint8_t LB[3] = {B_GLO, B_SCL, B_EDIT};
    static const uint8_t LL[3] = {LAYER_GLO, LAYER_SCL, LAYER_EDIT};
    static const uint8_t MODES[4] = {LEDS_OFF, LEDS_DIM_LO, LEDS_DIM, LEDS_INV};
    int bad = 0, ok = 1, okl = 1, okm = 1, oko = 1;
    uint32_t i, m, lk, t, held;
    ui_power_on();
    ui_leds();
    for (i = 0; i < 41u; i++)                           /* (the stub key map has none: 4 LEDs a column) */
        led_pos[i] = (uint8_t)((i / 4u) << 3 | (1u + i % 4u));
    for (i = 0; i < 3u; i++)
        for (lk = 0; lk < 2u; lk++)
            for (m = 0; m < 4u; m++) {
                uint32_t br0 = 0, lit0 = 0, want_br, want_lit, hk = LL[i] == LAYER_GLO ? white(1) : white(2);
                ui_power_on();
                settings_leds = MODES[m];
                go_title("AMP ENV"); frame();
                if (lk) {                                   /* locked (#83), then a key held */
                    press(LB[i]); frames(64); press(LB[i]); frames(100);
                    okl &= ui.lock == LL[i];
                } else {
                    btn_down(LB[i]);
                }
                key_down(hk); frame();
                okl &= ui.layer == LL[i];
                want_lit = layer_leds(&want_br);
                held = LL[i] == LAYER_GLO;  /* (HOLD keys: lit while held) */
                for (t = 0; t < 8u; t++) {                  /* 0 .. 1.75 s: the same picture */
                    fm1_ms += 250u;
                    ui_leds();
                    if (!t) {
                        br0 = breath_keys();
                        lit0 = lit_keys(fm1_led);
                    }
                    ok &= breath_keys() == br0 && lit_keys(fm1_led) == lit0 && !lit_keys(fm1_led_dim) &&
                          led_of(fm1_led_breath, panel.btn[LB[i]]) && !led_of(fm1_led, panel.btn[LB[i]]) &&
                          !led_of(fm1_led_dim, panel.btn[LB[i]]);
                }
                ok &= br0 == want_br && lit0 == want_lit && br0 && !(br0 & lit0);
                ok &= !held || (((lit0 >> hk) & 1u) && !((br0 >> hk) & 1u));   /* held: lit, not breathing */
                okm &= host_dim_lo == (MODES[m] == LEDS_DIM_LO);
                key_up(hk); frame();
                if (lk)
                    press(LB[i]);
                else
                    btn_up(LB[i]);
                frames(64);
                ui_leds();
                for (t = 0; t < FM1_NCOL; t++)
                    okl &= !ui.layer && !fm1_led_breath[t];
            }
    settings_leds = LEDS_DIM;
    bad += check("#119 FX GLO SCL EDIT, held and locked, every LEDS mode: the keys that can be pressed breathe", ok);
    bad += check("  ..held / in effect lit, never the glow, the layer's button breathes, steady over time", ok);
    bad += check("  ..closed: nothing breathes", okl);
    bad += check("  ..the breath's peak follows LEDS (LOW ~30 % of lit, else ~60 %)", okm);
    /* OCT+ in a dialog: breathing, OCT- lit, in every mode */
    for (m = 0; m < 4u; m++) {
        ui_power_on();
        settings_leds = MODES[m];
        go_title("AMP ENV"); frame();
        ui.confirm = CF_CLEAR_SEQ;
        for (t = 0; t < 4u; t++) {
            fm1_ms += 250u;
            ui_leds();
            oko &= led_of(fm1_led_breath, panel.btn[B_OCTUP]) && !led_of(fm1_led, panel.btn[B_OCTUP]) &&
                   !led_of(fm1_led_dim, panel.btn[B_OCTUP]) && led_of(fm1_led, panel.btn[B_OCTDN]) ==
                   (MODES[m] != LEDS_INV) && !led_of(fm1_led_breath, panel.btn[B_OCTDN]);
        }
        ui.confirm = 0;
        ui_leds();
        oko &= !led_of(fm1_led_breath, panel.btn[B_OCTUP]);
    }
    settings_leds = LEDS_DIM;
    led_pos_init();
    bad += check("#119 a dialog: OCT+ breathes (not lit, not the glow), OCT- lit (MAX: dark), every LEDS mode", oko);
    ui_power_on();
    return bad;
}

/* #35: every idle button and key glows dim, the active ones are lit; a map of the keys' own stays lit / dark */
static int test_idle_glow(void)
{
    int bad = 0, ok;
    ui_power_on();
    ui_leds();
    memset(led_pos, 0xFF, sizeof led_pos);
    led_pos[panel.btn[B_HOME]] = 0u << 3 | 1u;
    led_pos[panel.btn[B_ENV]] = 0u << 3 | 2u;
    led_pos[panel.btn[B_PLAY]] = 0u << 3 | 3u;
    led_pos[14u + 0u] = 1u << 3 | 1u;              /* keys 1 and 2 */
    led_pos[14u + 1u] = 1u << 3 | 2u;
    go_main_to(HP_MORPH);
    settings_leds = LEDS_DIM;                      /* MEDIUM: the idle ones at the mid level (1 frame in 4), no glow */
    ui_leds();
    bad += check("MEDIUM: HOME lit, ENV at the mid level (PLAY's red LED dark when stopped, its green lit), the keys too, no glow, none lit",
                 fm1_led[0] == 2u && fm1_led_mid[0] == 6u && fm1_led_dim[0] == 0u && fm1_led[1] == 0u && fm1_led_mid[1] == 6u &&
                 fm1_led_dim[1] == 0u);
    settings_leds = LEDS_DIM_LO;                   /* LOW: the minimum glow */
    ui_leds();
    bad += check("HOME: its button lit, ENV glows (PLAY stopped: green), the keys glow, none lit",
                 fm1_led[0] == 2u && fm1_led_dim[0] == 6u && fm1_led[1] == 0u && fm1_led_dim[1] == 6u && fm1_led[8] == 2u);
    fm1_in.notes = 1u;
    song.playing = 1;
    ui_leds();
    ok = fm1_led[0] == 10u && (fm1_led_dim[0] & 8u) == 0u && fm1_led[8] == 0u && fm1_led[1] == 0u && fm1_led_ktop[1] == 2u &&
         fm1_led_dim[1] == 4u && host_kl[3] == 120u && host_kl[4] == 84u;
    fm1_in.notes = 0;
    song.playing = 0;
    bad += check("a key held at LOW's played pulse (0.2.11: 50 % .. 35 %), no glow under it; PLAY running: its red LED lit, its green dark", ok);
    ui.layer = 1;
    ui_leds();
    ok = fm1_led_dim[1] == 0u && fm1_led_dim[0] == 6u;
    ui.layer = 0;
    ui_leds();
    ok &= fm1_led_dim[1] == 6u;
    bad += check("a layer's map: the keys lit or dark (no glow), the buttons still glow", ok);
    /* MENU > LEDS MAX: the active ones dark, the idle ones lit (stock), no glow; a breath as in DIM (#119); a map
     * of the keys' own as in DIM */
    settings_leds = LEDS_INV;
    ui_leds();
    ok = fm1_led[0] == 4u && fm1_led_dim[0] == 0u && fm1_led[1] == 6u && fm1_led_dim[1] == 0u && fm1_led[8] == 2u;   /* HOME dark; PLAY (red) dark, green lit */
    fm1_in.notes = 1u;
    song.playing = 1;
    ui_leds();
    ok &= fm1_led[0] == 12u && fm1_led_dim[0] == 0u && fm1_led[1] == 4u && fm1_led_dim[1] == 0u && fm1_led[8] == 0u;   /* key 1 dark; PLAY's red lit (not inverted), green dark */
    fm1_in.notes = 0;
    song.playing = 0;
    bad += check("LEDS MAX: the page's button and a key held go dark, the rest lit, PLAY red when running / green when stopped, no glow", ok);
    ui.layer = 1;
    led_pos[panel.btn[layer_btn()]] = 0u << 3 | 4u;   /* the layer's button */
    {
        uint32_t seen = 0, t;
        for (t = 0; t < 4u; t++) {                  /* the layer's button breathes: never lit (inverted), never the glow */
            fm1_ms = t * 250u;
            ui_leds();
            seen |= ((fm1_led[0] >> 4) & 1u ? 1u : 2u) | ((fm1_led_dim[0] >> 4) & 1u ? 4u : 0u) |
                    ((fm1_led_breath[0] >> 4) & 1u ? 8u : 0u);
        }
        ok = seen == 10u && fm1_led_dim[1] == 0u && fm1_led[1] == (uint8_t)((layer_leds(0) & 3u) << 1);
    }
    ui.layer = 0;
    fm1_ms = 0;
    bad += check("LEDS MAX: the layer's button breathes (not inverted); a layer's map stays lit / dark as in DIM", ok);
    settings_leds = LEDS_DIM;
    led_pos_init();
    return bad;
}

/* "On the step sequencer all the LEDs go out": SEQ > STEP, the piano roll and the DRUM grid, stopped and playing,
 * in every LEDS mode, with the LEDs where the hardware has them (hal/fm1_input.h FM1_KEYMAP). The buttons keep
 * their glow and their active states; on the piano roll the keys glow under the held and playing ones; on the
 * grid its map is lit and its keys that do something glow (LOW / HI). Before: keys_own() took the grid's keys
 * out of the glow, so in DIM 26 of 27 keys of an empty 16-step grid went fully dark */
/* the white keys of the grid's steps (bit p = step p of the page shown), bit k = key k */
static uint32_t step_keys(uint32_t steps)
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++)
        if (!key_black(k) && key_place(k) < 16u && ((steps >> key_place(k)) & 1u))
            m |= 1u << k;
    return m;
}
static int test_step_leds(void)
{
    static const int8_t MAP[6][FM1_NCOL] = {     /* (hal/fm1_input.h FM1_KEYMAP; the host stub is zero) */
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
        {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
        {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
        { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    };
    static const uint8_t MODES[LEDS_COUNT] = {LEDS_OFF, LEDS_DIM_LO, LEDS_DIM, LEDS_INV};
    int bad = 0;
    uint32_t id, p, r, mi, run, k;
    ui_power_on();
    ui_leds();
    for (id = 0; id < 41u; id++) {
        led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (MAP[r][p] == (int8_t)id)
                    led_pos[id] = (uint8_t)(p << 3 | r);
    }
    for (run = 0; run < 2u; run++)
            for (mi = 0; mi < LEDS_COUNT; mi++) {
                uint32_t m = MODES[mi], glow = m == LEDS_DIM || m == LEDS_DIM_LO, keys_lit = 0, keys_dim = 0, btn_bad = 0;
                uint32_t want_dim = 0, kc, keys;
                char what[128];
                ui_power_on();
                song.sel = 0;
                TSEL->step[0] = (step_t){.note = {60}, .n = 1, .time = ST_NOTE};   /* C4 on step 1 */
                TSEL->seq_active = 1;
                settings_leds = (uint8_t)m;
                open_family(FAM_SEQ);
                frame();
                for (k = 0; k < NPAGES && cur_page()->graph != GR_ROLL; k++) {
                    open_family(FAM_SEQ);
                    frame();
                }
                if (run) {
                    seq_start();
                    events_block(CTL);
                }
                ui_leds();
                for (k = 0; k < NB; k++) {                  /* a button: lit if active, else glow / dark / lit */
                    uint8_t q = led_pos[panel.btn[k]];
                    uint32_t lit = (fm1_led[q >> 3] >> (q & 7u)) & 1u, dim = (fm1_led_dim[q >> 3] >> (q & 7u)) & 1u;
                    int active = k == B_SEQ || (k == B_PLAY && run);
                    if (k == B_PLAY) {                      /* PLAY's own (red) LED: lit while running, else dark, in every mode; no glow */
                        btn_bad += lit != (uint32_t)run || dim;
                        continue;
                    }
                    btn_bad += m == LEDS_INV ? lit == (uint32_t)active || dim :
                               active ? !lit : lit || dim != glow;
                }
                keys = key_leds(0);
                for (k = 0; k < 27u; k++) {
                    uint8_t q = led_pos[14u + k];
                    keys_lit += (fm1_led[q >> 3] >> (q & 7u)) & 1u;
                    keys_dim += ((fm1_led_dim[q >> 3] & ~fm1_led[q >> 3]) >> (q & 7u)) & 1u;
                }
                kc = (uint32_t)__builtin_popcount(keys);
                if (glow)
                    want_dim = (uint32_t)__builtin_popcount(0x7FFFFFFu & ~keys);
                snprintf(what, sizeof what, "STEP %s, %s, LEDS %s: buttons and keys as the mode says (%u lit, %u glow)",
                         "piano roll", run ? "playing" : "stopped", LEDS_NAME[m],
                         (unsigned)keys_lit, (unsigned)keys_dim);
                bad += check(what, !btn_bad && host_dim_lo == (m == LEDS_DIM_LO) &&
                                       (!run || kc) &&
                                       keys_dim == want_dim && keys_lit == (m == LEDS_INV ? 27u - kc : kc) &&
                                       (!glow || keys_lit + keys_dim >= 25u) &&
                                       ((fm1_led[LED_PLAY_GREEN >> 3] >> (LED_PLAY_GREEN & 7u)) & 1u) == !run);
                bad += check("  the mid level: never a lit key", lit_keys(fm1_led_mid) == 0u &&
                             !(lit_keys(fm1_led_mid) & lit_keys(fm1_led)));
                if (run) {
                    transport_req = 2;
                    events_block(CTL);
                }
            }
    settings_leds = LEDS_DIM;
    led_pos_init();
    ui_power_on();
    return bad;
}

/* Discussion #127 (1.2): MENU > SCALE LEDS ON, outside the keys' own maps the keys of the selected track's scale glow
 * (the others dark), the root's at the mid level; the keys held / playing lit over it; LEDS OFF / LOW / MEDIUM
 * alike, MAX turned round (root lit, scale mid, the others glowing, the active dark); TRN; QNT WHITE: the white
 * keys; a DRUM track, a layer, the default OFF: as before */
/* MENU > DISPLAY > SCREEN OFF (1.1.5, ui.c scr_*): the screen and its backlight off after NEVER / 5 / 15 / 30 (default)
 * / 60 minutes without panel input; the press, key or turn that wakes it does nothing else; the dialogs, the count-in
 * and the UPDATE MODE countdown keep it on and wake it; the sound goes on as if it were on (forked sessions, bit for
 * bit); the setting saved (a spare favorites byte, 0 = NEVER since 1.1.5.1) and offered in the MENU */
static void scr_idle(uint32_t ms)                  /* nobody touches it for ms (a frame a minute, then frames) */
{
    while (ms > 60000u) { fm1_ms += 60000u - 16u; host_ticks += 60000u * 1000u; frame(); ms -= 60000u; }
    frames(ms);
}
static int scr_woke(uint32_t from)                 /* since host_pw_n was `from`: SLPOUT >= 120 ms after the SLPIN,
                                                    * DISPON + backlight >= 120 ms after the SLPOUT, nothing else */
{
    return host_pw_n == from + 2u && host_pw[from] == 1u && host_pw[from + 1u] == 2u && host_bl &&
           host_pw_ms[from] - host_pw_ms[from - 1u] >= 120u && host_pw_ms[from + 1u] - host_pw_ms[from] >= 120u &&
           scrn.st == SCR_ON;
}
static uint64_t scr_session(uint32_t setting, uint32_t *flags)
{
    int32_t o[2u * CTL];
    uint64_t h = 1469598103934665603ull, e = 0;
    uint32_t f, b, i, off = 0;
    ui_power_on();
    scr_put(setting);
    song.master_q12 = 4096;
    my_steps(&trk[0]);
    transport_req = 1;
    for (f = 0; f < 400u; f++) {
        if (f == 50u)
            fm1_ms += 6u * 60000u;                 /* six minutes untouched: 5 MIN goes dark */
        frame();
        off |= !host_bl;
        for (b = 0; b < 4u; b++) {
            mix_block(o, CTL);
            for (i = 0; i < 2u * CTL; i++) {
                h = (h ^ (uint32_t)o[i]) * 1099511628211ull;
                e += (uint64_t)(o[i] < 0 ? -o[i] : o[i]);
            }
        }
    }
    *flags = off | (song.playing ? 2u : 0u) | (e ? 4u : 0u);
    stop_transport();
    return h;
}
static uint64_t scr_session_fork(uint32_t setting, uint32_t *flags)   /* from the same state each time */
{
    int fd[2];
    uint64_t r[2] = {0, 0};
    pid_t p;
    if (pipe(fd))
        return 0;
    p = fork();
    if (!p) {
        uint32_t fl = 0;
        r[0] = scr_session(setting, &fl);
        r[1] = fl;
        if (write(fd[1], r, sizeof r) != sizeof r) _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], r, sizeof r) != sizeof r) r[0] = 0;
    close(fd[0]);
    waitpid(p, 0, 0);
    *flags = (uint32_t)r[1];
    return r[0];
}
/* (1.2's MOMENTARY, LFO held + a knob, is gone in DUELL 0.2.9: LFO held is the QUICK LFO layer; test_quick_edit) */
/* 1.2 (Discussion #135): MENU > ANIM IDLE, the LEDs' idle animation (ui_input.c idle_leds; the picture and the scan:
 * tests/input_test.c). After a minute without panel input it starts (ui_leds then writes none), with the glow as LEDS
 * says; a key or a button held, a knob turned end it and act as ever (the turn changes its value, the key plays);
 * never while playing, armed, in a layer, the menu, a dialog or NAME, with a MIDI note lit, with ANIM ON or OFF; kept
 * with the settings ([15][10] bit 0) */
static int test_idle_leds(void)
{
    int bad = 0, ok;
    int16_t v;
    persist_t p;
    ui_power_on();
    host_idle = host_anim = 0;
    menu_put(MI_ANIM, 2);
    settings_leds = LEDS_DIM;
    frames(160);
    fm1_ms += IDLE_LEDS_MS - 1000u; frame(); ui_leds();
    ok = !host_idle;
    fm1_ms += 1000u; frame(); ui_leds();
    bad += check("ANIM IDLE: the idle animation after a minute without input (not before), with the glow (LEDS DIM)",
                 ok && host_idle && host_idle_glow);
    memset(fm1_led, 0x5A, sizeof fm1_led);
    ui_leds();
    bad += check("  it has the LEDs: ui_leds writes none", fm1_led[0] == 0x5A && fm1_led[3] == 0x5A);
    v = TSEL->p[ENGINES[TSEL->eng_req]->knob[0]];
    go_title("EDIT 1");
    v = TSEL->p[cur_page()->id[0]];
    turn(EN_K1, 3); ui_leds();
    bad += check("  a knob turned ends it at once and is not swallowed (the value turns)", !host_idle &&
                 TSEL->p[cur_page()->id[0]] != v && fm1_led[0] != 0x5A);
    frame(); fm1_ms += IDLE_LEDS_MS; frame(); ui_leds();   /* (the turn counted as input: a minute from it) */
    ok = host_idle;
    fm1_in.notes = 1u << 4; host_notes |= 1u << 4; frame(); ui_leds();
    ok &= !host_idle;
    fm1_in.notes = 0; frame(); ui_leds();
    ok &= !host_idle;
    fm1_ms += IDLE_LEDS_MS - 100u; frame(); ui_leds();
    bad += check("  a key held ends it (the scan does at once) and counts as input: a minute again", ok && !host_idle);
    fm1_ms += 200u; frame(); ui_leds();
    ok = host_idle;
    song.playing = 1; frame(); ui_leds();
    ok &= !host_idle;
    song.playing = 0; song.rec = 1; frame(); ui_leds();
    ok &= !host_idle;
    song.rec = 0; ui.menu = 1; frame(); ui_leds();
    ok &= !host_idle;
    ui.menu = 0; ui.confirm = CF_CLEAR_SEQ; frame(); ui_leds();
    ok &= !host_idle;
    ui.confirm = 0; ui.lock = LAYER_GLO; ui.layer = LAYER_GLO; ui_leds();
    ok &= !host_idle;
    ui.lock = 0; ui.layer = 0; fm1_ms += IDLE_LEDS_MS; ui_leds();   /* (a dialog counts as input, as for SCREEN OFF) */
    ok &= host_idle;
    bad += check("  never while playing, armed, in the menu, a dialog or a layer; back when they end", ok);
    menu_put(MI_ANIM, 0); ui_leds();
    ok = !host_idle;
    menu_put(MI_ANIM, 1); ui_leds();
    ok &= !host_idle;
    menu_put(MI_ANIM, 2); settings_leds = LEDS_OFF; ui_leds();
    bad += check("  ANIM ON / OFF: none; LEDS OFF: no glow", ok && host_idle && !host_idle_glow);
    settings_export(&p);
    ok = p.favorites.factory[15][10] == 1u && !(p.favorites.factory[15][30] & PREF_ANIM_OFF);
    p.favorites.factory[15][10] = 0;
    ok &= settings_import(&p, sizeof p) == 1 && menu_get(MI_ANIM) == 0u;
    bad += check("  kept with the settings ([15][10] bit 0; 0, every older setting: ANIM ON)", ok);
    host_idle = 0;
    ui_idle = 0;
    settings_leds = LEDS_DIM;
    return bad;
}
static int test_screen_off(void)
{
    int bad = 0, ok;
    uint32_t n, rows, i;
    int16_t *vp, v0;

    ui_power_on();
    ok = ui_scr == 0u && scr_get() == 0u && SCR_MIN[scr_get()] == 0u && str_eq(MI_NAME[MI_SCROFF], "SCREEN OFF") &&
         MI_TAB[MI_SCROFF] == MTAB_DISPLAY && menu_n(MI_SCROFF) == 5u && str_eq(menu_vname(MI_SCROFF, 0), "NEVER") &&
         str_eq(menu_vname(MI_SCROFF, 1), "5 MIN") && str_eq(menu_vname(MI_SCROFF, 2), "15 MIN") &&
         str_eq(menu_vname(MI_SCROFF, 3), "30 MIN") && str_eq(menu_vname(MI_SCROFF, 4), "60 MIN") &&
         menu_get(MI_SCROFF) == 0u;
    bad += check("SCREEN OFF: MENU > DISPLAY row, NEVER 5 15 30 60 MIN, NEVER by default (stored 0, 1.1.5.1)", ok);
    menu_put(MI_SCROFF, 3u);                         /* 30 MIN for what follows */

    host_pw_n = 0;
    scr_idle(30u * 60000u - 2000u);
    ok = host_pw_n == 0u && host_bl && scrn.st == SCR_ON;
    scr_idle(2000u);
    ok &= host_pw_n == 1u && host_pw[0] == 0u && !host_bl && scrn.st == SCR_OFF && kb_asleep;
    rows = host_blit_rows;
    ui_message("SAVED");
    scr_idle(5000u);
    ok &= host_blit_rows == rows && host_pw_n == 1u && !ui.msg_t;
    bad += check("SCREEN OFF: 30 MIN untouched -> DISPOFF + SLPIN once; dark: nothing drawn", ok);

    n = host_pw_n;
    ok = at_home() && !ui.menu;
    btn_down(B_ENV); frame();                         /* a page button wakes it, does not open its page */
    ok &= scrn.st == SCR_SLPOUT && host_pw_n == n + 1u && host_pw[n] == 1u;   /* (SLPIN long ago: SLPOUT at once) */
    frames(100u);
    ok &= host_pw_n == n + 1u && host_blit_rows == rows;   /* (dark until 120 ms after it) */
    rows = host_blit_rows;
    frames(200u);
    ok &= scr_woke(n) && host_blit_rows - rows >= 200u;   /* (the frame before DISPON redrew it all) */
    btn_up(B_ENV); frame(); frames(100u);
    ok &= at_home() && !ui.menu && !ui.layer && !scrn.eat && !kb_asleep;
    press(B_ENV);                                     /* awake: the button works again */
    ok &= !at_home() && cur_page()->fam == FAM_ENV;
    bad += check("SCREEN OFF: a button wakes it (SLPOUT, 120 ms, the page redrawn, DISPON), swallowed", ok);

    go_main_to(HP_MORPH);
    for (i = 0; i < 3u; i++) {                        /* HOME, SAVE, REC held across the wake: no menu, undo, clear */
        static const uint32_t B[3] = {B_HOME, B_SAVE, B_REC};
        scr_idle(31u * 60000u);
        n = host_pw_n;
        ok = !host_bl;
        hold(B[i]); frames(300u);
        ok &= scr_woke(n) && at_home() && !ui.menu && !ui.confirm && !song.rec && !song.playing && !transport_req;
        bad += check(i == 0 ? "SCREEN OFF: HOME held to wake: no menu, no HOME tap after" :
                     i == 1 ? "SCREEN OFF: SAVE held to wake: no undo, no SAVE page" :
                              "SCREEN OFF: REC held to wake: nothing armed, no clear, no transport", ok);
    }

    scr_idle(31u * 60000u);
    n = host_pw_n;
    ok = !host_bl && !gates();
    key_down(white(0)); frame();                      /* a key: wakes it, no note (seq.c kb_asleep) */
    ok &= !gates();
    frames(300u);
    ok &= scr_woke(n) && !gates();
    key_up(white(0)); frame(); frames(50u);
    ok &= !gates() && !kb_asleep;
    key_down(white(0)); frame();
    ok &= gates() == 1u;                              /* awake: the key plays */
    key_up(white(0)); frame();
    bad += check("SCREEN OFF: a key wakes it silently (no note on, none off), the next one plays", ok);

    scr_idle(31u * 60000u);
    n = host_pw_n;
    go_title("EDIT 1");
    vp = &TSEL->p[cur_page()->id[0]];
    v0 = *vp;
    for (i = 0; i < 20u; i++) turn(EN_K1, 1);        /* a knob turned to wake it, still turning (on past the wake) */
    ok = scr_woke(n) && *vp == v0;
    turn(EN_PRESET, 1); turn(EN_ALGO, 1); turn(EN_SELECT, 1); turn(EN_K4, -1);
    ok &= *vp == v0 && song.sel == 0u && song.g[G_BPM] == GP[G_BPM].def && scrn.eat;
    frames(400u);                                     /* at rest SCR_EAT_MS: the knobs are the page's again */
    turn(EN_K1, 1);
    ok &= *vp != v0;
    bad += check("SCREEN OFF: a knob wakes it; turns until it rests 0.3 s change nothing (K1..4, PRESETS, ALGO, SELECT)", ok);

    menu_put(MI_SCROFF, 0u);                         /* NEVER */
    n = host_pw_n;
    scr_idle(3u * 3600000u);
    ok = host_pw_n == n && host_bl && ui_scr == 0u;
    menu_put(MI_SCROFF, 1u);                         /* 5 MIN */
    scr_idle(4u * 60000u + 50000u);
    ok &= host_pw_n == n && host_bl;
    scr_idle(15000u);
    ok &= host_pw_n == n + 1u && !host_bl;
    press(B_PLAY); frames(300u);                      /* PLAY wakes it, does not start the transport */
    ok &= scr_woke(n + 1u) && !song.playing && !transport_req;
    bad += check("SCREEN OFF: NEVER stays on for hours; 5 MIN goes dark after 5; PLAY to wake starts nothing", ok);

    n = host_pw_n;
    ui.confirm = CF_CLEAR_SEQ; ui.force = 1;          /* a dialog: never dark */
    scr_idle(20u * 60000u);
    ok = host_pw_n == n && host_bl;
    ui.confirm = 0; ui.force = 1;
    cin_left = 1000u;                                 /* the count-in */
    scr_idle(20u * 60000u);
    ok &= host_pw_n == n && host_bl;
    cin_left = 0;
    scr_idle(6u * 60000u);
    ok &= host_pw_n == n + 1u && !host_bl;
    ui.confirm = CF_CLEAR_SEQ; ui.force = 1;          /* one opening while dark (the editor, MIDI): it wakes */
    frames(300u);
    ok &= scr_woke(n + 1u) && ui.confirm == CF_CLEAR_SEQ;
    ui.confirm = 0; ui.force = 1;
    scr_idle(6u * 60000u);
    n = host_pw_n;
    ui.uboot = 3; ui.force = 1;                       /* the UPDATE MODE countdown */
    frames(300u);
    ok &= scr_woke(n);
    ui.uboot = 0; ui.force = 1;
    scr_idle(6u * 60000u);
    n = host_pw_n;
    scr_wake_now();                                   /* main.c: the crash screen, UPDATE, UBOOT */
    ok &= host_pw_n == n + 1u && host_pw[n] == 3u && host_bl && scrn.st == SCR_ON;
    bad += check("SCREEN OFF: a dialog, the count-in, UPDATE MODE keep it on and wake it; the crash screen at once", ok);

    {   /* the setting with the settings: SCR_CODE in favorites.factory[15][27]; 1.1.5's codes: 0 (its 30 MIN, its
         * default) now NEVER, 3 (its NEVER) NEVER, 2 1 7 as they were; 30 MIN is 4 now; unknown NEVER */
        persist_t p = {0};
        p.magic = PERSIST_MAGIC; p.panel = panel;
        menu_put(MI_SCROFF, 4u);
        settings_export(&p); ui_scr = 0;
        ok = p.favorites.factory[15][27] == 7u && settings_import(&p, sizeof p) && scr_get() == 4u;
        p.favorites.factory[15][27] = 0;
        ok &= settings_import(&p, sizeof p) && scr_get() == 0u;
        p.favorites.factory[15][27] = 3;              /* 1.1.5's NEVER */
        ok &= settings_import(&p, sizeof p) && scr_get() == 0u;
        p.favorites.factory[15][27] = 2;              /* 5 MIN, as on 1.1.5 */
        ok &= settings_import(&p, sizeof p) && scr_get() == 1u;
        menu_put(MI_SCROFF, 3u); ok &= ui_scr == 4u && scr_get() == 3u;
        p.favorites.factory[15][27] = 6;              /* unknown: NEVER */
        ok &= settings_import(&p, sizeof p) && scr_get() == 0u;
        ui_scr = 0x55; ok &= scr_get() == 0u;
        bad += check("SCREEN OFF: saved with the settings (0, 1.1.5's 30 MIN, and unknown = NEVER)", ok);
    }

    ui_power_on();                                    /* in the MENU: OCT+ / a knob step it */
    menu_put(MI_SCROFF, 3u);
    hold(B_HOME);
    for (i = 0; i < 5u; i++) turn(EN_PRESET, 1);
    ok = ui.menu == 1 && ui.menu_sel == MI_SCROFF;
    press(B_OCTUP); ok &= scr_get() == 4u;
    press(B_OCTUP); ok &= scr_get() == 4u;           /* (stops at the end) */
    for (i = 0; i < 4u; i++) turn(EN_K2, -1);
    ok &= scr_get() == 0u;
    press(B_OCTDN); ok &= scr_get() == 0u && ui.menu == 1;
    press(B_OCTUP); press(B_OCTUP); press(B_OCTUP); ok &= scr_get() == 3u;
    press(B_HOME); ok &= ui.menu == 0;
    bad += check("SCREEN OFF: the MENU row steps NEVER .. 60 MIN (OCT-/+, any knob), stops at the ends", ok);

    {
        uint32_t f0 = 0, f1 = 0;
        uint64_t a = scr_session_fork(0u, &f0), b = scr_session_fork(1u, &f1);
        bad += check("SCREEN OFF: the sound bit for bit as with NEVER while dark (sequencer playing)",
                     a && a == b && (f0 & 6u) == 6u && (f1 & 6u) == 6u && !(f0 & 1u) && (f1 & 1u));
    }
    ui_power_on();
    return bad;
}

static int test_scale_leds(void)
{
    static const int8_t MAP[6][FM1_NCOL] = {     /* (hal/fm1_input.h FM1_KEYMAP; the host stub is zero) */
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
        {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
        {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
        { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    };
    static const uint8_t MODES[LEDS_COUNT] = {LEDS_OFF, LEDS_DIM_LO, LEDS_DIM, LEDS_INV};
    const uint32_t ALL = 0x7FFFFFFu, C_KEYS = 1u << 7 | 1u << 19;
    int bad = 0, ok;
    uint32_t id, p, r, k, mi, root = 0, sc, white = 0;
    track_t *t;
    ui_power_on();
    for (id = 0; id < 41u; id++) {
        led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (MAP[r][p] == (int8_t)id)
                    led_pos[id] = (uint8_t)(p << 3 | r);
    }
    for (k = 0; k < 27u; k++)
        white |= (uint32_t)!key_black(k) << k;
    song.sel = 0;
    t = TSEL;
    scale_g = 1;                                        /* C major */
    root_g = 0;
    settings_leds = LEDS_DIM;
    ui_rec_prefs = 0x40u;                               /* an old setting with SCALE LEDS ON (the retired bit 6): AUTO */
    ui_leds();
    ok = !menu_get(MI_SCLLED) && str_eq(MI_NAME[MI_SCLLED], "SCALE LEDS") && MI_TAB[MI_SCLLED] == MTAB_DISPLAY &&   /* (0.2.14: DISPLAY) */
         str_eq(menu_vname(MI_SCLLED, 0), "AUTO") && str_eq(menu_vname(MI_SCLLED, 1), "OFF") && menu_n(MI_SCLLED) == 2u;
    ok &= scale_leds(&root) == white && root == C_KEYS;
    ui_rec_prefs = 0;                                   /* an old setting with SCALE LEDS OFF / a new unit: AUTO too */
    sc = scale_leds(&root);
    bad += check("0.2.5 SCALE LEDS AUTO by default (old settings, bit clear or the retired bit 6 set, load as AUTO); C major: the white keys, the root C4 C5",
                 ok && !menu_get(MI_SCLLED) && sc == white && root == C_KEYS);
    for (mi = 0; mi < LEDS_COUNT; mi++) {              /* 0.2.11: the keyboard's levels per mode, neighbours >= 1.4x apart in duty */
        const uint8_t *kv = KEY_LV[MODES[mi]];
        uint32_t out = kv[KL_OUT] ? kv[KL_OUT] : 4u;    /* (0: the glow, ~1/60 = 4 /240) */
        char what[200];
        /* out < in-scale < played trough < root = played peak (MAX: its played keys are dark..dim, below the scale, the root above it) */
        ok = out * 14u <= kv[KL_SCL] * 10u && kv[KL_SCL] * 14u <= kv[KL_ROOT] * 10u;
        if (MODES[mi] == LEDS_INV)
            ok &= kv[KL_LOW] == 0u && kv[KL_TOP] > 0u && kv[KL_TOP] < kv[KL_SCL] && kv[KL_ROOT] == 240u;
        else
            ok &= kv[KL_SCL] * 14u <= kv[KL_LOW] * 10u && kv[KL_LOW] * 14u <= kv[KL_ROOT] * 10u && kv[KL_TOP] == kv[KL_ROOT] &&
                  kv[KL_LOW] * 14u <= kv[KL_TOP] * 10u;
        snprintf(what, sizeof what, "0.2.11 KEY_LV %s: out %u, scale %u, trough %u, root = peak %u /240: adjacent states >= 1.4x apart (MAX: played %u..%u)",
                 LEDS_NAME[MODES[mi]], (unsigned)kv[KL_OUT], (unsigned)kv[KL_SCL], (unsigned)kv[KL_LOW], (unsigned)kv[KL_ROOT],
                 (unsigned)kv[KL_LOW], (unsigned)kv[KL_TOP]);
        bad += check(what, ok);
    }
    ok = KEY_LV[LEDS_DIM][KL_SCL] == 120u && KEY_LV[LEDS_DIM][KL_ROOT] == 240u && KEY_LV[LEDS_DIM][KL_LOW] == 168u &&
         KEY_LV[LEDS_DIM_LO][KL_ROOT] < 240u && KEY_LV[LEDS_DIM_LO][KL_SCL] < KEY_LV[LEDS_DIM][KL_SCL] &&
         KEY_LV[LEDS_INV][KL_SCL] > KEY_LV[LEDS_DIM][KL_SCL] && KEY_LV[LEDS_INV][KL_ROOT] == 240u && !KEY_LV[LEDS_INV][KL_LOW];
    bad += check("0.2.11 KEY_LV: MEDIUM's scale keys at 1/2, its root full; LOW dimmer overall; MAX brighter (the root lit, the played keys dim..dark)", ok);
    for (mi = 0; mi < LEDS_COUNT; mi++) {
        uint32_t m = MODES[mi], held = 1u << 1 | 1u << 9;   /* F#3 (out of the scale) and D4 (in it) held */
        uint32_t scale = white & ~C_KEYS, k_all;
        uint32_t ht = held;                             /* 0.2.11: the keys held pulse (the played set), never lit steadily */
        uint32_t r0 = C_KEYS & (0u - C_KEYS), outs = ALL & ~white, ko = KEY_LV[m][KL_OUT] ? outs : 0u, kd = ko ? 0u : outs;
        char what[160];
        settings_leds = (uint8_t)m;
        fm1_in.notes = 0;
        ui_leds();
        ok = !lit_keys(fm1_led) && lit_keys(fm1_led_root) == C_KEYS && lit_keys(fm1_led_scl) == scale &&
             !lit_keys(fm1_led_dim) && !lit_keys(fm1_led_mid) && !lit_keys(fm1_led_breath) && !lit_keys(fm1_led_kout) &&
             !lit_keys(fm1_led_ktop) &&
             !memcmp(host_kl, (uint32_t[5]){KEY_LV[m][0], KEY_LV[m][1], KEY_LV[m][2], KEY_LV[m][3], KEY_LV[m][4]}, sizeof host_kl);
        fm1_in.notes = held;
        ui_leds();
        k_all = lit_keys(fm1_led) | lit_keys(fm1_led_dim) | lit_keys(fm1_led_mid) | lit_keys(fm1_led_scl) |
                lit_keys(fm1_led_root) | lit_keys(fm1_led_kout) | lit_keys(fm1_led_ktop);
        ok &= !lit_keys(fm1_led) && lit_keys(fm1_led_ktop) == ht && lit_keys(fm1_led_scl) == (scale & ~held) &&
              lit_keys(fm1_led_root) == C_KEYS && !lit_keys(fm1_led_dim) && !lit_keys(fm1_led_mid) &&
              k_all == ((white & ~held) | ht);
        fm1_in.notes = held | r0;                       /* and the lower root key held: the held level, out of the root's */
        ui_leds();
        ok &= !lit_keys(fm1_led) && lit_keys(fm1_led_ktop) == (held | r0) &&
              lit_keys(fm1_led_root) == (C_KEYS & (C_KEYS - 1u));
        snprintf(what, sizeof what, "0.2.9 LEDS %s: out of the scale dark, the scale and the root at the mode's levels, the root STEADY (its own set, no pulse), the keys held pulse (a root key too)",
                 LEDS_NAME[m]);
        bad += check(what, ok);
        offscale_g = OS_PLAY;                           /* OFF-SCALE PLAY: the keys out of the scale glow (MAX: a level), the rest as above */
        fm1_in.notes = 0;
        ui_leds();
        ok = !lit_keys(fm1_led) && lit_keys(fm1_led_root) == C_KEYS && lit_keys(fm1_led_scl) == scale && lit_keys(fm1_led_dim) == kd &&
             lit_keys(fm1_led_kout) == ko && !lit_keys(fm1_led_mid) && !lit_keys(fm1_led_breath);
        fm1_in.notes = held;
        ui_leds();
        ok &= !lit_keys(fm1_led) && lit_keys(fm1_led_ktop) == ht && lit_keys(fm1_led_root) == C_KEYS &&
              lit_keys(fm1_led_scl) == (scale & ~held) && lit_keys(fm1_led_dim) == (kd & ~held) && lit_keys(fm1_led_kout) == (ko & ~held) &&
              !lit_keys(fm1_led_mid);
        snprintf(what, sizeof what, "0.2.9 LEDS %s, OFF-SCALE PLAY: the keys out of the scale %s (SNAP: dark), a held one at the held level",
                 LEDS_NAME[m], ko ? "at their level" : "glow");
        bad += check(what, ok);
        offscale_g = OS_SNAP;
    }
    fm1_in.notes = 0;
    settings_leds = LEDS_DIM;
    scale_g = 0;                                        /* CHROMATIC: no scale display; every mode as without it */
    for (mi = 0; mi < LEDS_COUNT; mi++) {
        uint32_t held = 1u << 1 | 1u << 9;
        settings_leds = MODES[mi];
        fm1_in.notes = 0;
        ui_leds();
        ok = !scale_leds(&root) && !root && !lit_keys(fm1_led_scl) && !lit_keys(fm1_led_root) &&
             !lit_keys(fm1_led_kout) && !lit_keys(fm1_led_ktop) &&
             (MODES[mi] == LEDS_INV ? lit_keys(fm1_led) == ALL && !lit_keys(fm1_led_dim) && !lit_keys(fm1_led_mid)
              : !lit_keys(fm1_led) && lit_keys(fm1_led_dim) == (MODES[mi] == LEDS_DIM_LO ? ALL : 0u) &&   /* LOW: the glow, */
                lit_keys(fm1_led_mid) == (MODES[mi] == LEDS_DIM ? ALL : 0u));                           /* MEDIUM: 1 frame in 4 */
        bad += check("0.2.5 SCALE LEDS AUTO with CHROMATIC: nothing to show, the keys as without it (each LEDS mode)", ok);
        fm1_in.notes = held;
        ui_leds();
        ok = (MODES[mi] == LEDS_INV ? lit_keys(fm1_led) == (ALL & ~held) : !lit_keys(fm1_led)) && lit_keys(fm1_led_ktop) == held &&
             lit_keys(fm1_led_dim) == (MODES[mi] == LEDS_DIM_LO ? ALL & ~held : 0u) &&   /* (the glow and the mid level stop under the pulse) */
             lit_keys(fm1_led_mid) == (MODES[mi] == LEDS_DIM ? ALL & ~held : 0u);
        bad += check("0.2.11 CHROMATIC, keys held: they pulse (peak..trough) in every mode, MAX among the lit ones, no glow under them", ok);
    }
    {   /* 0.2.11 the played keys' pulse follows the notes: one pulse a beat while the transport runs or an arp sounds, phase-locked, else free */
        uint32_t bpm0 = song.g[G_BPM];
        settings_leds = LEDS_DIM;
        song.g[G_BPM] = 120;
        song.playing = 0;
        fm1_in.notes = 1u;
        host_pu_ms = 99u;
        ui_leds();
        ok = host_pu_ms == 0u;                          /* a held key, nothing running: free, ~0.9 Hz */
        song.playing = 1;
        ui_leds();
        ok &= host_pu_ms == 500u && host_pu_ph < 65536u;   /* running at 120 BPM: a pulse every beat (500 ms) */
        song.g[G_BPM] = 240;
        ui_leds();
        ok &= host_pu_ms == 250u;
        song.g[G_BPM] = 30;
        ui_leds();
        ok &= host_pu_ms == 2000u;                      /* (the hardware clamps to 150..2000 ms: 30 BPM is the slowest it follows) */
        song.playing = 0;
        song.g[G_BPM] = 120;
        trk[0].p[P_AMODE] = 1;                          /* an arp sounding (a note held), the transport stopped: the beat too */
        trk[0].nheld = 1;
        ui_leds();
        ok &= host_pu_ms == 500u;
        trk[0].p[P_AMODE] = 0;
        trk[0].nheld = 0;
        song.g[G_BPM] = (uint8_t)bpm0;
        fm1_in.notes = 0;
        ui_leds();
        ok &= host_pu_ms == 0u;
        bad += check("0.2.11 played-key pulse: free (~0.9 Hz) with nothing running, one pulse per beat while the transport runs (500 ms at 120, 250 at 240)", ok);
    }
    fm1_in.notes = 0;
    scale_g = 1;
    settings_leds = LEDS_DIM;
    menu_put(MI_SCLLED, 1);                             /* OFF: its own bit (7), the retired 6 left alone */
    ok = menu_get(MI_SCLLED) == 1u && (ui_rec_prefs & 0x80u) && !(ui_rec_prefs & 0x40u) && !ui_prefs && !scale_leds(&root);
    ui_leds();
    ok &= lit_keys(fm1_led_mid) == ALL && !lit_keys(fm1_led_dim) && !lit_keys(fm1_led_scl) && !lit_keys(fm1_led_root);
    bad += check("0.2.5 SCALE LEDS OFF: the scale is not shown, every key glows as before", ok);
    menu_put(MI_SCLLED, 0);
    ok = !(ui_rec_prefs & 0x80u) && scale_leds(&root) == white;
    bad += check("0.2.5 SCALE LEDS AUTO again: the bit clears", ok);
    fm1_in.notes = 0;
    settings_leds = LEDS_DIM;
    root_g = 2;                                         /* D major: F# and C# in, F and C out, D the root */
    sc = scale_leds(&root);
    ok = (sc >> 1 & 1u) && (sc >> 8 & 1u) && !(sc & 1u) && !(sc >> 7 & 1u) && root == (1u << 9 | 1u << 21) &&
         __builtin_popcount(sc) == 16;
    root_g = 0;
    t->p[P_TRANS] = 2;                                  /* C major, TRN +2: F3 plays G3 (in), A#3 plays C4 (the root) */
    sc = scale_leds(&root);
    ok &= (sc & 1u) && !(sc >> 1 & 1u) && root == (1u << 5 | 1u << 17);
    t->p[P_TRANS] = 0;
    bad += check("#127 the keys as they play: ROOT D, TRN +2", ok);
    offscale_g = OS_SNAP;                               /* the keys of the scale (SNAP: the others play a scale note; PLAY: they play as they are) */
    sc = scale_leds(&root);
    ok = sc == white && root == C_KEYS;
    scale_g = 5;                                        /* PEN: five notes an octave, the roots the same C keys */
    sc = scale_leds(&root);
    ok &= sc != white && __builtin_popcount(sc) < __builtin_popcount(white) && root == C_KEYS;
    ui_leds();
    ok &= lit_keys(fm1_led_scl) == (sc & ~root) && lit_keys(fm1_led_root) == root && !lit_keys(fm1_led);
    scale_g = 0;                                        /* CHROMATIC: no scale display */
    sc = scale_leds(&root);
    ok &= !sc && !root;
    bad += check("#127 SCALE LEDS follow the global scale: C major the white keys, PEN its five, CHROMATIC none (nothing to tell apart)", ok);
    scale_g = 1;
    ui.layer = LAYER_SCL;                               /* a layer: its own map, no scale display under it */
    ui_leds();
    ok = !lit_keys(fm1_led_mid) && !lit_keys(fm1_led_dim) && !lit_keys(fm1_led_scl) && !lit_keys(fm1_led_root);
    ui.layer = 0;
    bad += check("#127 a layer keeps its LEDs: no scale display over it", ok);
    ui_rec_prefs = 0;
    settings_leds = LEDS_DIM;
    led_pos_init();
    ui_power_on();
    return bad;
}

/* #39: KNOB 1..4 with a layer's button held are the layer's only, also as it is let go: a knob turned in the frame
 * the button is let go, while its map still shows (an effect key held after FX), or just after: the page under the
 * layer (ARP: ARP MODE on) or HOME kept its values. GLO / SCL / EDIT change only what their own knobs edit */
static int16_t lk_p[NTRK][P_COUNT], lk_g[G_COUNT];
static void lk_snap(void)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++)
        memcpy(lk_p[i], trk[i].p, sizeof lk_p[i]);
    memcpy(lk_g, song.g, sizeof lk_g);
}
static int lk_same(uint32_t l)                      /* nothing changed but the layer's own (l: the layer) */
{
    uint32_t i, j;
    for (i = 0; i < NTRK; i++)
        for (j = 0; j < P_COUNT; j++) {
            int own = (l == LAYER_GLO && j == P_LEVEL) ||
                      (l == LAYER_EDIT && i == 0u && (j < P_AMODE || j > P_AORDER));   /* (a load keeps the ARP) */
            if (lk_p[i][j] != trk[i].p[j] && !own)
                return 0;
        }
    for (j = 0; j < G_COUNT; j++)
        if (lk_g[j] != song.g[j] && l != LAYER_EDIT)
            return 0;
    return 1;
}
static void lk_turn(int32_t s)                       /* KNOB 1..4 together, no frame */
{
    uint32_t k;
    for (k = 0; k < 4u; k++)
        host_enc[panel.enc[EN_K1 + k]] += s * panel.dir[EN_K1 + k];
}
static int test_layer_knob_leak(void)
{
    static const uint8_t LB[3] = {B_GLO, B_SCL, B_EDIT};
    static const uint8_t LL[3] = {LAYER_GLO, LAYER_SCL, LAYER_EDIT};
    uint32_t b, sc, pg, fails = 0;
    int bad = 0, ok;
    for (b = 0; b < 3u; b++)
        for (sc = 0; sc < 4u; sc++)
            for (pg = 0; pg <= NPAGES; pg++) {          /* every page, HOME last */
                ui_power_on();
                if (pg < NPAGES) { ui.page = (uint8_t)pg; page_entered(); }
                frames(100);
                lk_snap();
                btn_down(LB[b]);
                if (sc == 3u) key_down(white(1));
                frame();
                lk_turn(5); frame();
                if (sc == 0u) { lk_turn(2); btn_up(LB[b]); frame(); }          /* turning as it is let go */
                if (sc == 1u) { btn_up(LB[b]); frame(); lk_turn(2); frame(); } /* .. the next frame */
                if (sc == 2u) { btn_up(LB[b]); frames(160); lk_turn(2); frame(); }   /* .. 160 ms after */
                if (sc == 3u) { btn_up(LB[b]); frame(); lk_turn(3); frame(); key_up(white(1)); frame(); }   /* the map */
                frames(400);
                ok = lk_same(LL[b]);
                if (!ok) {
                    fails++;
                    if (fails < 6u)
                        printf("ui:   leak: %s scenario %u page %s\n", B_NAME[LB[b]], sc,
                               pg < NPAGES ? PAGES[pg].title : "HOME");
                }
            }
    bad += check("#39 GLO / SCL / EDIT + KNOB 1..4, let go while turning: no page under the layer changes", !fails);
    return bad;
}

/* #102: the TIMER5 scan counts the knobs' detents while ui_input runs; one that came after the layer's read of
 * KNOB 1..4 (host_enc_late) was read again by the page's: the layer's control and the page under it both moved
 * (HOME's KNOB 1 with FX's FILTER). Every layer (FX GLO SCL EDIT) x KNOB 1..4 x held past HOLD / a combo / locked
 * by a double tap, over every page and HOME, with late detents on the knob, PRESETS and ALGORITHM while it is open
 * and on the knob as it closes: the same state as with the detents read in time (the layer gets them all, one a
 * frame later), the layer's own control moved, and nothing under the layer changed (lk_same) */
typedef struct {
    int16_t p[NTRK][P_COUNT], g[G_COUNT];
    uint32_t sel, snd[NTRK], mot[NTRK];
    uint8_t home, page;
} rc_state_t;
static uint32_t rc_snd(const track_t *t) { return t->eng_req | (uint32_t)t->preset << 8 | (uint32_t)t->user << 16; }
static uint32_t rc_snd0;
static void rc_take(rc_state_t *st)
{
    uint32_t i;
    memset(st, 0, sizeof *st);
    for (i = 0; i < NTRK; i++) {
        memcpy(st->p[i], trk[i].p, sizeof st->p[i]);
        st->snd[i] = rc_snd(&trk[i]);
        st->mot[i] = motion_count(&trk[i]);
    }
    memcpy(st->g, song.g, sizeof st->g);
    st->sel = song.sel;
    st->home = at_home();
    st->page = ui.page;
}
static void rc_enc(uint32_t role, int32_t s, int late)
{
    if (late)
        host_enc_late[panel.enc[role]] += s * panel.dir[role];
    else
        host_enc[panel.enc[role]] += s * panel.dir[role];
}
/* one run: layer button lb, mode (0 held past HOLD, 1 a combo, 2 locked), knob k, page pg (NPAGES: HOME); late:
 * the detents come after the reads. a: the state with the layer open, b: after it closed */
static void rc_run(uint32_t lb, uint32_t mode, uint32_t k, uint32_t pg, int late, rc_state_t *a, rc_state_t *b)
{
    int32_t d = lb == B_GLO ? -1 : 1;   /* (GLO LEVEL: room to move) */
    uint32_t i;
    ui_power_on();
    if (pg < NPAGES) { ui.page = (uint8_t)pg; page_entered(); }
    frames(64);
    lk_snap();
    rc_snd0 = rc_snd(TSEL);
    if (mode == 2u) {
        press(lb); frames(64); press(lb); frames(64);
    } else {
        btn_down(lb);
        frame();
        if (mode == 0u)
            frames(560);                                /* (past HOLD: the map, a peek) */
    }
    for (i = 0; i < 3u; i++) {
        rc_enc(EN_K1 + k, d, late && (mode != 1u || i));   /* (a combo: the first in time, it opens the map) */
        if (i == 1u) {
            rc_enc(EN_PRESET, 1, late);                 /* (in time: the layer's read drops them) */
            rc_enc(EN_ALGO, 1, late);
        }
        frame();
    }
    frame();                                            /* (the last late one) */
    rc_take(a);
    if (mode == 2u) {
        press(lb);
    } else {
        btn_up(lb);
        frame();
    }
    for (i = 0; i < 3u; i++) {                          /* turning on as it closes (#39: KNOB 1..4 quiet) */
        rc_enc(EN_K1 + k, d, late);
        frame();
    }
    frames(400);
    rc_take(b);
}
static int test_layer_knob_race(void)
{
    static const uint8_t LB[3] = {B_GLO, B_SCL, B_EDIT};
    static const uint8_t LL[3] = {LAYER_GLO, LAYER_SCL, LAYER_EDIT};
    static rc_state_t a0, b0, a1, b1;
    uint32_t l, mode, k, pg, fails = 0, runs = 0;
    int bad = 0, ok;
    for (l = 0; l < 3u; l++)
        for (mode = 0; mode < 3u; mode++)
            for (k = 0; k < 4u; k++)
                for (pg = 0; pg <= NPAGES; pg++) {
                    if (pg < NPAGES && !page_visible(pg))
                        continue;
                    if (mode == 2u && LB[l] == B_EDIT && pg < NPAGES &&   /* (EDIT's tap acts there: no lock) */
                        (PAGES[pg].graph == GR_USER || PAGES[pg].graph == GR_SLOTS || PAGES[pg].graph == GR_ROLL ||
                         PAGES[pg].graph == GR_EVENTS))
                        continue;
                    rc_run(LB[l], mode, k, pg, 0, &a0, &b0);
                    rc_run(LB[l], mode, k, pg, 1, &a1, &b1);
                    ok = !memcmp(&a0, &a1, sizeof a0) && !memcmp(&b0, &b1, sizeof b0) &&
                         lk_same(LL[l] == LAYER_EDIT && k == 2u ? 0u : LL[l]) && song.sel == 0u;
                    if (LL[l] == LAYER_GLO)                 /* the layer's own control moved */
                        ok &= a1.p[k][P_LEVEL] == lk_p[k][P_LEVEL] - 3;
                    else if (LL[l] == LAYER_SCL)
                        ok &= a1.p[0][LY_SCL.id[k]] != lk_p[0][LY_SCL.id[k]];
                    else if (k < 2u)                        /* (EDIT KNOB 3 FAV: a mark, KNOB 4 nothing) */
                        ok &= a1.snd[0] != rc_snd0;
                    runs++;
                    if (!ok) {
                        fails++;
                        if (fails < 6u)
                            printf("ui:   race: %s mode %u KNOB %u page %s\n", B_NAME[LB[l]], mode, k + 1u,
                                   pg < NPAGES ? PAGES[pg].title : "HOME");
                    }
                }
    bad += check("#102 GLO SCL EDIT (held, combo, locked) x KNOB 1..4, detents counted mid-pass: the layer's only",
                 !fails && runs > 300u);
    /* HOME, REC armed and playing: GLO's T2..T4 levels record nothing into T1's automation */
    ok = 1;
    for (k = 1; k < 4u; k++) {
        int16_t *vp, c;
        uint32_t i;
        ui_power_on();
        song.rec = 1; seq_start(); go_title("EDIT 1"); frames(64);
        vp = &TSEL->p[cur_page()->id[k]];
        c = *vp;
        btn_down(B_GLO); frames(560);
        for (i = 0; i < 4u; i++) { rc_enc(EN_K1 + k, -1, 1); frame(); }
        frame();
        btn_up(B_GLO); frames(400);
        ok &= !motion_count(TSEL) && *vp == c;
        stop_transport();
        song.rec = 0;
    }
    bad += check("  HOME with REC: GLO's KNOB 2..4 record no automation, HOME's sound stays", ok);
    /* no layer: a late detent is HOME's KNOB 1's, once (the next pass) */
    ui_power_on();
    go_title("EDIT 1"); frames(64);
    {
        int16_t *vp, c;
        vp = &TSEL->p[cur_page()->id[0]];
        c = *vp;
        rc_enc(EN_K1, 1, 1); frame(); frame();
        ok = *vp == c + 1;
    }
    bad += check("  no layer: a detent counted mid-pass is HOME's KNOB 1, once", ok);
    return bad;
}

/* Discussion #83: the layer lock. A double tap of FX / GLO / SCL / EDIT (within LY_DTAP_MS, both let go before HOLD)
 * opens the map with no button held, over the page the first tap left (put back); the keys and KNOB 1..4 act as held;
 * a tap closes it (no page), so does another page button (which opens its page), HOME, the menu, a dialog; OCT- puts
 * back as in the layer; a single tap, a peek, a combo and two slow taps are as before; the header shows the lock */
/* ------------------------------------------------------ SEQ TOOLS (1.2) --- */
static void tl_note(step_t *s, uint32_t n) { memset(s, 0, sizeof *s); s->time = ST_NOTE; s->n = 1; s->note[0] = (uint8_t)n; s->vel = 96; }
static void tl_tie(step_t *s) { memset(s, 0, sizeof *s); s->time = ST_TIE; }
/* the selected track: LEN 8, C4 tied, E4, G4 (steps 1 2, 4, 6), a note past LEN (21), a lock on step 4 */
static void tl_melody(void)
{
    uint32_t i;
    track_defaults_steps(TSEL);
    for (i = 0; i < NSTEP; i++)
        step_clear(&TSEL->step[i]);
    tl_note(&TSEL->step[0], 60);
    tl_tie(&TSEL->step[1]);
    tl_note(&TSEL->step[3], 64);
    tl_note(&TSEL->step[5], 67);
    tl_note(&TSEL->step[20], 72);
    TSEL->p[P_SLEN] = 8;
    motion_set_lock(TSEL, 3, P_REV, 50);
}
static int tl_lock_at(uint32_t step) { int16_t v; return motion_lock_get(TSEL, step, P_REV, &v) && v == 50; }
static void tl_key(uint32_t w) { key_down(white(w)); frame(); key_up(white(w)); frame(); }
static int tl_in_key(const track_t *t)                       /* every note of steps 1..LEN in the track's scale */
{
    uint32_t i, k;
    for (i = 0; i < (uint32_t)t->p[P_SLEN]; i++)
        for (k = 0; k < t->step[i].n; k++)
            if (t->step[i].time == ST_NOTE && !tl_in_scale(t, t->step[i].note[k]))
                return 0;
    return 1;
}
static int tl_times_are(const track_t *t, const step_t *ref, uint32_t len)   /* the rhythm: the same NOTE / TIE / REST */
{
    uint32_t i;
    for (i = 0; i < len; i++)
        if (t->step[i].time != ref[i].time || (t->step[i].n != 0) != (ref[i].n != 0))
            return 0;
    return 1;
}
static int test_seq_tools(void)
{
    int bad = 0, ok, okc;
    uint32_t i, k, a, b2, pg, seed;
    step_t ref[NSTEP], mid[NSTEP];

    /* where it opens (1.2): every page; up to 1.1.5 away from the SEQ pages SEQ held opened SONG */
    ok = 1;
    for (i = 0; i <= NPAGES; i++) {
        ui_power_on();
        if (i == NPAGES) go_main_to(HP_MORPH); else { ui.page = (uint8_t)i; page_entered(); }
        if (i < NPAGES && !page_visible(i))
            continue;
        frame();
        pg = ui.page;
        btn_down(B_SEQ); frames(500);
        ok &= ui.layer == LAYER_SEQ && ui.page == pg && !ui.confirm;
        btn_up(B_SEQ); frame();
        ok &= !ui.layer && ui.page == pg && at_home() == (i == NPAGES);
    }
    bad += check("1.2: SEQ held opens SEQ TOOLS on every page and HOME (no SONG, the page stays)", ok);
    ui_power_on();
    go_page(GR_ROLL); frame();
    press(B_SEQ); frames(320);
    ok = cur_page()->graph == GR_EVENTS && str_eq(cur_page()->title, "AUTOMATION") && msg_is("HOLD [SEQ] QUICK");
    press(B_SEQ); frames(320);
    ok &= cur_page()->graph == GR_ROLL;
    press(B_SEQ); frames(320);
    ok &= cur_page()->graph == GR_EVENTS;
    bad += check("1.2: a tap on STEP: AUTOMATION, then STEP again (two SEQ pages; the hint until the layer was opened)", ok);
    go_title("AMP ENV"); frame();
    press(B_SEQ); frames(320);
    ok = cur_page()->graph == GR_EVENTS;                /* (the SEQ page shown last) */
    go_main_to(HP_MORPH); frame();
    for (k = 0; k < NPAGES; k++)                        /* a remembered page that is no SEQ page any more: STEP */
        if (PAGES[k].fam != FAM_SEQ) ui.fam_last[FAM_SEQ] = (uint8_t)k;
    press(B_SEQ); frames(320);
    ok &= cur_page()->graph == GR_ROLL;
    bad += check("  from elsewhere the SEQ page shown last; one remembered that left SEQ: STEP", ok);

    /* the whole sequence: REVERSE (a note's ties stay after it), SHIFT, CLEAR, locks, LEN, silent keys, undo */
    ui_power_on();
    tl_melody();
    memcpy(ref, TSEL->step, sizeof ref);
    go_page(GR_ROLL); frame();
    usb.config = 1;
    {
        uint32_t mo = mo_w;
        btn_down(B_SEQ); frame();
        tl_key(1);                                      /* G3: REVERSE */
        ok = ui.layer == LAYER_SEQ && !gates() && mo_w == mo && msg_is("REVERSED");
    }
    usb.config = 0;
    ok &= TSEL->step[6].time == ST_NOTE && TSEL->step[6].note[0] == 60 && TSEL->step[7].time == ST_TIE &&
          TSEL->step[4].note[0] == 64 && TSEL->step[2].note[0] == 67 && TSEL->step[0].time == ST_REST &&
          TSEL->step[20].note[0] == 72 && tl_lock_at(4) && !tl_lock_at(3);
    bad += check("  G3 REVERSE: steps 1..LEN mirrored, C4 + its tie as C4 then the tie, the lock moves with E4", ok);
    btn_up(B_SEQ); frame();
    hold(B_SAVE);
    bad += check("  SAVE held: the reverse undone (steps and the lock)", !memcmp(TSEL->step, ref, sizeof ref) && tl_lock_at(3));
    btn_down(B_SEQ); frame();
    tl_key(3);                                          /* B3: SHIFT > */
    ok = TSEL->step[1].note[0] == 60 && TSEL->step[2].time == ST_TIE && TSEL->step[4].note[0] == 64 && tl_lock_at(4) &&
         msg_is("SHIFTED RIGHT");
    tl_key(3);
    tl_key(2); tl_key(2);                               /* A3: SHIFT < twice */
    ok &= !memcmp(TSEL->step, ref, sizeof ref) && tl_lock_at(3);
    TSEL->p[P_SLEN] = 6;
    tl_key(2);
    ok &= TSEL->step[5].note[0] == 60 && TSEL->step[2].note[0] == 64 && TSEL->step[6].time == ST_REST &&
          TSEL->step[20].note[0] == 72;                 /* (inside LEN 6: step 7 and 21 stay) */
    tl_key(3);
    TSEL->p[P_SLEN] = 8;
    ok &= !memcmp(TSEL->step, ref, sizeof ref);
    bad += check("  B3 / A3 SHIFT > <: one step round inside LEN (the lock with it); back again: as it was", ok);
    tl_key(0);                                          /* F3: CLEAR */
    for (ok = msg_is("SEQUENCE CLEARED"), i = 0; i < 8u; i++)
        ok &= TSEL->step[i].time == ST_REST && !TSEL->step[i].n;
    ok &= TSEL->step[20].note[0] == 72 && !motion_lock_count(TSEL);
    bad += check("  F3 CLEAR: steps 1..LEN empty (no dialog), past LEN kept, the locks of the steps cleared go", ok);
    oct_back();
    ok = !memcmp(TSEL->step, ref, sizeof ref) && tl_lock_at(3) && TSEL->p[P_SLEN] == 8 && msg_is("SEQUENCE PUT BACK") &&
         ui.layer == LAYER_SEQ && song.octave == 0;
    turn(EN_K1, -2);
    ok &= TSEL->p[P_SLEN] == 6;                         /* KNOB 1: LEN (the PATTERN page's four) */
    oct_back();
    ok &= TSEL->p[P_SLEN] == 8;
    btn_up(B_SEQ); frame();
    hold(B_SAVE);                                       /* (OCT-'s put back is one undo too: the tools again) */
    ok &= TSEL->step[0].time == ST_REST;
    hold(B_SAVE);
    ok &= !memcmp(TSEL->step, ref, sizeof ref);
    bad += check("  OCT-: the track as the layer opened (steps, locks, LEN); KNOB 1..4 LEN DIV SWING GATE", ok);

    /* RANDOM / COOK on a melodic track (fixed seeds): the rhythm stays, the notes in the scale, near the middle */
    ok = okc = 1;
    for (seed = 1; seed <= 40u; seed++) {
        ui_power_on();
        tl_melody();
        root_g = 2; scale_g = 2;
        memcpy(ref, TSEL->step, sizeof ref);
        go_page(GR_ROLL); frame();
        rng_state = 0x9E3779B9u * seed;
        btn_down(B_SEQ); frame();
        tl_key(4);                                      /* C4: RANDOM */
        ok &= msg_is("RANDOM NOTES") && tl_times_are(TSEL, ref, NSTEP) && tl_in_key(TSEL) && TSEL->step[20].note[0] == 72;
        for (i = 0; i < 8u; i++)
            if (TSEL->step[i].n)
                ok &= TSEL->step[i].note[0] >= 54 && TSEL->step[i].note[0] <= 72;
        for (k = 0; k < 12u; k++) {                     /* D4: COOK, again and again */
            memcpy(mid, TSEL->step, sizeof mid);
            tl_key(5);
            okc &= tl_times_are(TSEL, ref, NSTEP) && tl_in_key(TSEL) && TSEL->step[20].note[0] == 72 &&
                   memcmp(mid, TSEL->step, sizeof mid) != 0;
        }
        okc &= msg_is("COOKED 12");
        btn_up(B_SEQ); frame();
    }
    bad += check("  C4 RANDOM (melodic): the rhythm kept, new notes in the scale near the middle (40 seeds)", ok);
    bad += check("  D4 COOK x12: each press changes it, the rhythm and the scale kept, \"COOKED 12\"", okc);
    hold(B_SAVE);
    bad += check("  SAVE held after COOKs: the last one undone only", !memcmp(TSEL->step, mid, sizeof mid));
    ui_power_on();
    track_defaults_steps(TSEL);
    TSEL->p[P_SLEN] = 16;
    go_page(GR_ROLL); frame();
    rng_state = 12345u;
    btn_down(B_SEQ); frame();
    tl_key(5);
    ok = msg_is("NOTHING TO COOK") && !undo.trk;
    tl_key(4);
    for (k = 0, i = 0; i < 16u; i++) {
        ok &= i % 2u ? TSEL->step[i].time == ST_REST : 1;
        k += (uint32_t)tl_gate(&TSEL->step[i]);
    }
    ok &= tl_gate(&TSEL->step[0]) && k >= 3u && tl_in_key(TSEL);
    btn_up(B_SEQ); frame();
    bad += check("  an empty pattern: COOK says NOTHING TO COOK; RANDOM writes 8ths with rests, in the scale", ok);

    ui_power_on();
    tl_melody();
    go_page(GR_ROLL); frame();
    btn_down(B_SEQ); frames(500);                       /* (the map: a peek) */
    memcpy(ref, TSEL->step, sizeof ref);
    a = leds_at(0); b2 = leds_at(250);
    ok = ((a ^ b2) >> white(5)) & 1u && !(((a | b2) >> white(6)) & 1u) && !(((a | b2) >> black(0)) & 1u);
    tl_key(6); tl_key(10);
    key_down(black(2)); frame(); key_up(black(2)); frame();
    ok &= !memcmp(TSEL->step, ref, sizeof ref) && !gates();
    btn_up(B_SEQ); frame();
    bad += check("  LEDs: the six tools breathe (CLEAR REVERSE SHIFT SHIFT RANDOM COOK); no other key does anything (0.2.14: no BEAT, no lane keys)", ok);

    /* the lock (a double tap), a song playing */
    ui_power_on();
    tl_melody();
    go_page(GR_ROLL); frame();
    pg = ui.page;
    press(B_SEQ); frames(64); press(B_SEQ); frames(500);
    ok = ui.lock == LAYER_SEQ && ui.layer == LAYER_SEQ && ui.page == pg;
    tl_key(1);
    ok &= TSEL->step[6].note[0] == 60;
    press(B_SEQ); frames(16);
    ok &= !ui.lock && !ui.layer && ui.page == pg;
    bad += check("  double tapped on STEP: locked over STEP, the keys act; a tap closes it", ok);
    return bad;
}

static int test_layer_lock(void)
{
    static const uint8_t LB[3] = {B_GLO, B_SCL, B_EDIT};
    static const uint8_t LL[3] = {LAYER_GLO, LAYER_SCL, LAYER_EDIT};
    int bad = 0, ok, okk;
    uint32_t i, pg;
    ok = 1; okk = 1;
    for (i = 0; i < 3u; i++) {                          /* double tap on ENV: locked, ENV under it */
        ui_power_on();
        go_title("AMP ENV"); frame();
        pg = ui.page;
        press(LB[i]);
        ok &= !ui.lock && !ui.layer;                    /* the first tap: its page at once */
        frames(64);
        press(LB[i]);
        frames(500);                                    /* (no button held, long past HOLD) */
        ok &= ui.lock == LL[i] && ui.layer == LL[i] && layer_locked() && !at_home() && ui.page == pg &&
              !fm1_in.buttons && ((layer_seen >> LL[i]) & 1u) && !msg_is("HOLD [GLO] QUICK");
        draw_head();
        okk &= layer_locked();
        press(LB[i]);                                   /* a tap: closed, no page */
        frames(16);
        okk &= !ui.lock && !ui.layer && !at_home() && ui.page == pg;
    }
    bad += check("#83 GLO SCL EDIT double tapped: the map stays open with no button held, over the page as it was", ok);
    bad += check("  a tap of the button closes it, no page opens", okk);
    {   /* #183: in every lock PLAY starts and stops (GLO's lock no RESTART: that needs GLO held) */
        static const uint8_t PB[5] = {B_GLO, B_SCL, B_EDIT, B_SEQ, B_REC};
        static const uint8_t PL[5] = {LAYER_GLO, LAYER_SCL, LAYER_EDIT, LAYER_SEQ, LAYER_REC};
        ok = 1;
        for (i = 0; i < 5u; i++) {
            uint32_t k;
            ui_power_on();
            go_title("AMP ENV"); frame();
            press(PB[i]); frames(64); press(PB[i]); frames(100);
            transport_req = 0;
            okk = ui.lock == PL[i];
            press(B_PLAY);                              /* stopped: PLAY */
            okk &= transport_req == 1u;
            events_block(CTL);
            okk &= song.playing != 0;
            for (k = 0; k < 2u; k++) {                  /* playing: PLAY stops, twice over */
                press(B_PLAY);
                okk &= transport_req == 2u;
                events_block(CTL);
                okk &= !song.playing && ui.lock == PL[i];
                press(B_PLAY);
                okk &= transport_req == 1u;
                events_block(CTL);
                okk &= song.playing != 0;
            }
            stop_transport();
            transport_req = 0;
            if (!okk)
                printf("    #183: %s locked, PLAY does not start / stop\n", LAYERS[PL[i]].head);
            ok &= okk;
        }
        bad += check("#183 GLO SCL EDIT SEQ REC locked: PLAY starts and stops (no RESTART without GLO held)", ok);
    }
    /* the keys and KNOB 1..4 act as held */
    ui_power_on();
    go_title("AMP ENV"); frame();
    press(B_GLO); press(B_GLO); frames(100);
    usb.config = 1;
    {
        uint32_t mo = mo_w;
        int16_t l0 = trk[0].p[P_LEVEL];
        key_down(black(1)); frame(); key_up(black(1)); frame();
        ok = trk[1].p[P_MUTE] == 1 && !gates() && mo_w == mo;
        turn(EN_K1, 3);
        ok &= trk[0].p[P_LEVEL] == l0 + 3 && str_eq(cur_page()->title, "AMP ENV");
        press(B_OCTDN);                                 /* OCT-: the mix put back, still locked */
        ok &= !trk[1].p[P_MUTE] && trk[0].p[P_LEVEL] == l0 && msg_is("MIX PUT BACK") && ui.lock == LAYER_GLO;
    }
    bad += check("  GLO locked: a black key mutes (silent, no MIDI), KNOB 1 T1 LEVEL, OCT- puts back, still locked", ok);
    ui_power_on();
    go_title("AMP ENV"); frame();
    press(B_SCL); press(B_SCL); frames(100);
    key_down(white(1)); frame(); key_up(white(1)); frame();   /* G3: ROOT G */
    ok = root_g == 7 && !gates();
    turn(EN_K1, 1);
    ok &= scale_g == 1 && str_eq(cur_page()->title, "AMP ENV");
    bad += check("  SCL locked: a key sets ROOT, KNOB 1 SCALE (the global), the page stays ENV", ok);
    ui_power_on();
    go_title("AMP ENV"); frame();
    press(B_EDIT); press(B_EDIT); frames(100);
    turn(EN_K1, 1);
    ok = TSEL->eng_req != trk[1].eng_req || TSEL->eng_req != TRK_DEF[0][0];
    ok &= str_eq(cur_page()->title, "AMP ENV") && ui.lock == LAYER_EDIT;
    key_down(white(LY_INIT)); frame(); key_up(white(LY_INIT)); frame();   /* INIT: the dialog closes it */
    ok &= ui.confirm == CF_INIT_SOUND && !ui.lock && !ui.layer;
    press(B_OCTDN);
    bad += check("  EDIT locked: KNOB 1 the engine; INIT's dialog closes the lock", ok);
    /* closing: another page button opens its page, HOME goes home, another layer's tap its page, the menu */
    ui_power_on();
    go_title("AMP ENV"); frame();
    press(B_GLO); press(B_GLO); frames(100);
    press(B_LFO);
    ok = !ui.lock && !ui.layer && str_eq(cur_page()->title, "LFO 1");
    press(B_SCL); press(B_SCL); frames(100);
    ok &= ui.lock == LAYER_SCL && str_eq(cur_page()->title, "LFO 1");
    press(B_HOME); frames(16);
    ok &= !ui.lock && !ui.layer && at_home();
    press(B_GLO); press(B_GLO); frames(100);
    press(B_SCL); frames(400);
    ok &= !ui.lock && !ui.layer && cur_page()->fam == FAM_SCL;
    press(B_EDIT); press(B_EDIT); frames(100);
    hold(B_HOME);
    ok &= ui.menu && !ui.lock && !ui.layer;
    hold(B_HOME);
    bad += check("  LFO, HOME, another layer's button, the menu close it (and act as always)", ok);
    ui_power_on();
    go_title("AMP ENV"); frame();
    press(B_GLO); press(B_GLO); frames(100);
    btn_down(B_GLO); frames(500);                       /* its button held: the map stays (a peek), let go: closed */
    ok = !ui.lock && ui.layer == LAYER_GLO;
    btn_up(B_GLO); frame();
    ok &= !ui.layer && str_eq(cur_page()->title, "AMP ENV");
    frames(400);
    {
        int16_t a = TSEL->p[P_ATK];
        turn(EN_K1, 1);
        ok &= TSEL->p[P_ATK] == a + 1;                  /* (the page's knobs again after the quiet time) */
    }
    bad += check("  its button held after the lock: a peek, let go closes it; KNOB 1 is ENV's again", ok);
    /* unchanged: a single tap, two slow taps, a tap and a peek, a tap and a combo, PLAY / REC in the lock */
    ui_power_on();
    go_page(GR_TRK); frame();
    press(B_SEQ); frames(400);                          /* (1.2: SEQ, GLO's one page cycles no more) */
    ok = str_eq(cur_page()->title, "STEP") && !ui.lock;
    press(B_SEQ); frames(400);
    ok &= str_eq(cur_page()->title, "AUTOMATION") && !ui.lock && !ui.layer;
    press(B_SEQ);
    btn_down(B_SEQ); frames(500);
    ok &= ui.layer == LAYER_SEQ && !ui.lock;
    btn_up(B_SEQ); frame();
    ok &= !ui.layer && !ui.lock && str_eq(cur_page()->title, "STEP");
    press(B_GLO); frames(400);
    ok &= str_eq(cur_page()->title, "SONG") && !ui.lock;
    press(B_GLO);
    btn_down(B_GLO); frames(500);
    ok &= ui.layer == LAYER_GLO && !ui.lock;
    btn_up(B_GLO); frame();
    ok &= !ui.layer && !ui.lock && str_eq(cur_page()->title, "SONG");
    press(B_SCL);
    lay_combo(B_SCL, white(2));
    key_up(white(2)); btn_up(B_SCL); frame(); frames(400);
    ok &= !ui.layer && !ui.lock && root_g == 9 && cur_page()->fam == FAM_SCL;
    bad += check("  unchanged: a tap opens the page, slow taps cycle, a tap then a peek or a combo: no lock", ok);
    ui_power_on();
    go_page(GR_TRK); frame();
    press(B_GLO); frames(16);
    press(B_LFO); frames(16);
    press(B_GLO); frames(400);
    ok = !ui.lock && str_eq(cur_page()->title, "SONG");   /* (another button between the taps: two taps) */
    ui_power_on();
    go_title("AMP ENV"); frame();
    press(B_SCL); press(B_SCL); frames(100);
    press(B_PLAY);
    ok &= song.playing || transport_req;
    ok &= ui.lock == LAYER_SCL;
    stop_transport();
    bad += check("  another button between the taps: no lock; PLAY in a locked layer plays, the lock stays", ok);
    {   /* EDIT on STEP clears a step: two quick taps clear two (no lock) */
        track_t *t;
        ui_power_on();
        t = TSEL;
        my_steps(t);
        go_page(GR_ROLL); frame();
        ui.cursor = 0;
        press(B_EDIT); press(B_EDIT); frames(100);
        bad += check("  EDIT on STEP (it clears the step): two quick taps clear two steps, no lock",
                     !ui.lock && !ui.layer && ui.cursor == 2u && t->step[0].time == ST_REST);
    }
    return bad;
}

/* 1.1.5: the REC layer (ui_layer.c). REC held past HOLD on any page opens it (up to 1.1.4 REC held on SEQ asked "CLEAR Tn
 * SEQUENCE?", Discussion #91: gone, and its "HOLD: CLEAR" hint and the footer's REC + trash with it); a tap still
 * arms / disarms. F3 CLEAR (no dialog, SAVE held undoes it), G3 CLICK, A3 COUNT-IN, B3 CLICK LEVEL (each the next
 * value, the same store as MENU > AUDIO, saved when the layer closes), KNOB 1..3 the same; OCT- puts back; the LEDs;
 * the double-tap lock (the arming as before the taps); REC in another layer still arms */
static void rec_down(void) { btn_down(B_REC); }
static void rec_up(void) { btn_up(B_REC); frame(); }
static uint32_t menu_rec(uint32_t r) { return menu_get(r == 0u ? MI_CLICK : r == 1u ? MI_COUNTIN : MI_CLKLVL); }
static int test_rec_layer(void)
{
    static const uint8_t G[8] = {GR_ROLL, GR_EVENTS, GR_USER, GR_PATS, GR_CHORD, GR_TOOLS, GR_ADSR, GR_TRK};
    int bad = 0, ok = 1;
    uint32_t k, i, n;
    step_t before[NSTEP];
    for (k = 0; k < 9u; k++) {                          /* every page (and HOME): held, the layer; let go, nothing */
        ui_power_on();
        song.sel = 1;
        my_steps(&trk[1]);
        if (k < 8u)
            go_page(G[k]);
        frame();
        memcpy(before, trk[1].step, sizeof before);
        n = ui.page;
        rec_down(); frames(512);
        ok &= ui.layer == LAYER_REC && ui.ly == LAYER_REC && !ui.confirm && song.rec == 0u && !transport_req &&
              !str_eq(ui.msg, "HOLD: CLEAR");
        rec_up();
        ok &= !ui.layer && !ui.confirm && song.rec == 0u && !transport_req && ui.page == n && (k < 8u ? !at_home() : at_home()) &&
              !memcmp(trk[1].step, before, sizeof before);
    }
    bad += check("REC layer: REC held on STEP AUTOMATION USER PHRASES CHORD TOOLS ENV MIXER HOME opens it", ok);
    bad += check("  no CLEAR dialog any more, nothing armed, cleared or started (#91's hold replaced)", ok);
    ok = 1;
    for (k = 0; k < 4u; k++) {                          /* the menu's HOLD time, as the other layers */
        ui_power_on();
        settings_hold = (uint8_t)k;
        rec_down();
        for (i = 0; i < 40u && !ui.layer; i++)
            frame();
        ok &= ui.layer == LAYER_REC && i * 16u >= HOLD_MS[k] && i * 16u <= HOLD_MS[k] + 32u;
        rec_up();
    }
    bad += check("  it opens after MENU > HOLD (0.3 .. 0.6 s), not the old 0.7 s", ok);
    ui_power_on();
    press(B_REC);
    ok = at_home() && song.rec == 1u && transport_req == 1u && !ui.layer;
    frames(400); transport_req = 0; song.playing = 1;
    press(B_REC);
    ok &= song.rec == 0u && !ui.layer && song.playing;
    song.playing = 0;
    go_page(GR_ROLL); frame(); frames(400);
    press(B_REC);
    ok &= song.rec == 1u && transport_req == 0u && msg_is("KEYS WRITE STEPS");
    bad += check("  a tap still arms / disarms when let go (HOME starts PLAY, STEP: step recording)", ok);
    ui_power_on();
    rec_down(); frame();
    press(B_PLAY);                                      /* REC + PLAY: PLAY plays, REC let go arms (no map) */
    ok = transport_req == 1u && !ui.layer;
    song.playing = 1; transport_req = 0;
    rec_up();
    ok &= song.rec == 1u && !ui.layer && song.playing;
    bad += check("  REC held + PLAY: plays, and REC let go before HOLD still arms (no combo)", ok);
    ui_power_on();
    song.sel = 2;
    frames(400);
    btn_down(B_GLO); frame();
    press(B_REC);                                       /* REC in GLO's layer: a tap, armed */
    ok &= song.rec == 4u && ui.ly == LAYER_GLO;
    btn_up(B_GLO); frame();
    frames(400);
    btn_down(B_GLO); frames(512); press(B_REC);
    ok &= song.rec == 0u && ui.layer == LAYER_GLO;
    btn_up(B_GLO); frame();
    bad += check("  a song playing: STOP TO RECORD; REC in another layer still arms / disarms", ok);

    /* F3 CLEAR: no dialog, SAVE held undoes it; an empty track: NOTHING TO CLEAR; a song playing: STOP TO EDIT */
    ui_power_on();
    song.sel = 1;
    my_steps(&trk[1]);
    go_page(GR_ROLL); frame();
    memcpy(before, trk[1].step, sizeof before);
    rec_down(); frame();
    key_down(white(0)); frame();                        /* a combo: the map at once, cleared */
    ok = ui.layer == LAYER_REC && seq_is_empty(&trk[1]) && !ui.confirm && msg_is("SEQUENCE CLEARED") && !gates() &&
         str_eq(ui.msg2, "[SAVE] HOLD TO UNDO");
    key_up(white(0)); frame();
    rec_up();
    ok &= !ui.layer && song.rec == 0u && !transport_req && seq_is_empty(&trk[1]);
    bad += check("REC + F3: CLEAR T2's sequence at once (no dialog, the key silent), nothing armed", ok);
    frames(400);
    hold(B_SAVE);
    bad += check("  SAVE held undoes the clear", !memcmp(trk[1].step, before, sizeof before));
    rec_down(); frame(); key_down(white(0)); frame(); key_up(white(0)); frame(); rec_up();
    track_defaults_steps(&trk[1]);
    rec_down(); frame(); key_down(white(0)); frame();
    ok = msg_is("NOTHING TO CLEAR");
    key_up(white(0)); frame(); rec_up();
    my_steps(&trk[1]);
    song.playing = 1; song.rec = 2u;                    /* recording live: a retake, the arming and PLAY stay */
    rec_down(); frame(); key_down(white(0)); frame();
    ok &= seq_is_empty(&trk[1]) && song.rec == 2u && song.playing;
    key_up(white(0)); frame(); rec_up();
    song.playing = 0;
    bad += check("  empty: NOTHING TO CLEAR; a song playing: STOP TO EDIT; armed and playing: clears, records on", ok);

    /* G3 A3 B3: the settings, the next value each press, MENU > AUDIO's values; KNOB 1..3 */
    ui_power_on();
    ui_rec_prefs = 0;
    rp_apply();
    ok = 1;
    rec_down(); frames(512);
    for (k = 0; k < 3u; k++) {
        static const char *const NM[3][3] = {{"CLICK REC", "CLICK ON", "CLICK OFF"},
                                             {"COUNT-IN 1 BAR", "COUNT-IN 2 BARS", "COUNT-IN OFF"},
                                             {"CLICK LEVEL HIGH", "CLICK LEVEL LOW", "CLICK LEVEL MID"}};
        static const uint32_t WANT[3][3] = {{1, 2, 0}, {1, 2, 0}, {2, 0, 1}};
        for (i = 0; i < 3u; i++) {
            key_down(white(k + 1u)); frame(); key_up(white(k + 1u)); frame();
            ok &= menu_rec(k) == WANT[k][i] && msg_is(NM[k][i]) && !gates() &&
                  str_eq(menu_vname(k == 0u ? MI_CLICK : k == 1u ? MI_COUNTIN : MI_CLKLVL, menu_rec(k)),
                         rl_name(k, rp_get(RL_F[k])));
        }
    }
    ok &= ui_rec_prefs == 0u;                           /* (round: as it was) */
    bad += check("REC + G3 CLICK OFF>REC>ON, A3 COUNT-IN OFF>1 BAR>2 BARS, B3 LEVEL MID>HIGH>LOW: MENU's values", ok);
    key_down(white(1)); frame(); key_up(white(1)); frame();        /* CLICK REC */
    key_down(white(2)); frame(); key_up(white(2)); frame();        /* COUNT-IN 1 BAR */
    key_down(white(3)); frame(); key_up(white(3)); frame();        /* LEVEL HIGH */
    ok = click_mode == 1u && cin_bars == 1u && click_lvl == 2u && lys.rp_dirty;
    rec_up();
    ok &= !ui.layer && !lys.rp_dirty;                  /* (closed: settings_save) */
    {
        persist_t p = {0};
        uint8_t was = ui_rec_prefs;
        p.magic = PERSIST_MAGIC; p.panel = panel;
        settings_export(&p); ui_rec_prefs = 0;
        ok &= settings_import(&p, sizeof p) && ui_rec_prefs == was && menu_get(MI_CLICK) == 1u &&
              menu_get(MI_COUNTIN) == 1u && menu_get(MI_CLKLVL) == 2u;
    }
    bad += check("  applied at once (click.c, seq.c), saved when the layer closes, kept with the settings", ok);
    rec_down(); frames(512);
    turn(EN_K1, 1);                                     /* CLICK REC -> ON */
    turn(EN_K2, -1);                                    /* COUNT-IN 1 BAR -> OFF */
    turn(EN_K3, -5);                                    /* LEVEL HIGH -> LOW (stops at the end) */
    turn(EN_K4, 3);                                     /* nothing */
    ok = menu_get(MI_CLICK) == 2u && menu_get(MI_COUNTIN) == 0u && menu_get(MI_CLKLVL) == 0u && at_home();
    rec_up();
    bad += check("  KNOB 1..3 CLICK COUNT-IN LEVEL, stopping at the ends; KNOB 4 nothing; the page untouched", ok);

    /* OCT- puts back the settings and the track as the layer opened */
    ui_power_on();
    ui_rec_prefs = 0; rp_apply();
    song.sel = 1;
    my_steps(&trk[1]);
    memcpy(before, trk[1].step, sizeof before);
    rec_down(); frames(512);
    key_down(white(1)); frame(); key_up(white(1)); frame();
    key_down(white(0)); frame(); key_up(white(0)); frame();
    ok = seq_is_empty(&trk[1]) && click_mode == 1u;
    press(B_OCTDN);
    ok &= !memcmp(trk[1].step, before, sizeof before) && ui_rec_prefs == 0u && click_mode == 0u &&
          msg_is("REC PUT BACK") && song.octave == 0 && ui.layer == LAYER_REC;
    rec_up();
    bad += check("  OCT- puts back CLICK and the cleared track; no octave shift", ok);

    /* the LEDs: CLEAR breathes when there is something; a setting on lit (LEVEL: with the click on), else breathing */
    ui_power_on();
    ui_rec_prefs = 0; rp_apply();
    my_steps(TSEL);
    rec_down(); frames(512);
    {
        uint32_t a = leds_at(0), b = leds_at(250), w0 = 1u << white(0), w1 = 1u << white(1), w2 = 1u << white(2),
                 w3 = 1u << white(3);
        ok = ((a ^ b) & (w0 | w1 | w2 | w3)) == (w0 | w1 | w2 | w3) && !(a & b) && !((a | b) & ~(w0 | w1 | w2 | w3));
        rp_put(RP_CLICK, 2); rp_put(RP_COUNTIN, 2);
        a = leds_at(0); b = leds_at(250);
        ok &= (a & b) == (w1 | w2 | w3) && (a ^ b) == w0;
        track_defaults_steps(TSEL);
        a = leds_at(0); b = leds_at(250);
        ok &= !((a | b) & w0);
    }
    rec_up();
    bad += check("  LEDs: CLEAR breathes (dark when empty), CLICK / COUNT-IN lit when on, LEVEL with the click", ok);

    /* the double-tap lock: the arming as before the taps, the keys and knobs as held, a tap of REC closes it */
    ui_power_on();
    ui_rec_prefs = 0; rp_apply();
    go_title("AMP ENV"); frame();
    n = ui.page;
    press(B_REC);
    ok = song.rec == 1u && transport_req == 1u && msg_is("HOLD [REC] QUICK");   /* (until it has been opened once) */
    frames(64);
    press(B_REC);
    frames(500);
    ok &= ui.lock == LAYER_REC && ui.layer == LAYER_REC && layer_locked() && song.rec == 0u && transport_req == 0u &&
          ui.page == n && !fm1_in.buttons && !msg_is("HOLD [REC] QUICK") && ((layer_seen >> LAYER_REC) & 1u);
    key_down(white(1)); frame(); key_up(white(1)); frame();
    turn(EN_K2, 1);
    ok &= click_mode == 1u && cin_bars == 1u && !gates() && ui.lock == LAYER_REC;
    press(B_PLAY);                                      /* PLAY works in it */
    ok &= transport_req == 1u && ui.lock == LAYER_REC;
    transport_req = 0;
    press(B_REC);                                       /* a tap: closed, nothing armed */
    frames(16);
    ok &= !ui.lock && !ui.layer && song.rec == 0u && ui.page == n && !lys.rp_dirty;
    bad += check("#83 REC double tapped: locked, the arming as before; keys, knobs, PLAY; a REC tap closes it", ok);
    ui_power_on();
    song.playing = 1;
    press(B_REC); frames(64); press(B_REC); frames(100);
    ok = ui.lock == LAYER_REC && song.rec == 0u && song.playing && transport_req == 0u;
    press(B_REC); frames(16);
    song.playing = 0;
    press(B_GLO); press(B_GLO); frames(100);            /* another layer locked: REC still arms, the lock stays */
    press(B_REC);
    ok &= ui.lock == LAYER_GLO && song.rec == 1u;
    bad += check("  playing: the transport stays; in GLO's lock a REC tap arms, GLO stays locked", ok);
    return bad;
}

/* #48: the note divisions step by their length, longest first (triplets between their neighbours), on every list
 * that has them (SEQ DIV, ARP RATE, the delay's TIME); the stored values and their sound stay */
static int test_div_order(void)
{
    int bad = 0, ok = 1;
    uint32_t id, c, k, found = 0;
    static const param_desc_t *const LISTS[] = {&TP[P_SDIV], &TP[P_ARATE], &GP[G_DTIME]};
    ui_power_on();
    for (k = 0; k < NELEM(LISTS); k++) {           /* left end -> right end: each one shorter than the one before */
        const param_desc_t *d = LISTS[k];
        int32_t v = d->def, n, seen = 0;
        uint32_t prev = 0xFFFFFFFFu, len;
        for (n = 0; n < 40; n++) v = param_turn(d, v, -1);
        for (n = 0; n <= d->max - d->min; n++) {
            len = div_samples((uint32_t)v);
            if (!(len < prev && enum_rank(d, v) == d->min + n && !((seen >> v) & 1))) {
                printf("  %s: %s (value %d, place %d, length %u) after %u\n", d->label, d->names[v], (int)v,
                       (int)enum_rank(d, v), len, prev);
                ok = 0;
            }
            seen |= 1 << v;
            prev = len;
            v = param_turn(d, v, 1);
        }
        ok &= seen == (1 << (d->max - d->min + 1)) - 1;
    }
    ok &= str_eq(N_DIV[2], "1/16") && str_eq(N_DIV[4], "8T") && str_eq(N_DIV[9], "4BAR") && TP[P_SDIV].def == 2;
    ok &= param_turn(&TP[P_SDIV], 2, 1) == 5 && param_turn(&TP[P_SDIV], 2, -1) == 4 &&   /* 1/16: right 16T, left 8T */
          param_turn(&TP[P_SDIV], 3, 1) == 3 && param_turn(&TP[P_SDIV], 9, -1) == 9 &&   /* the ends hold */
          param_turn(&TP[P_SDIV], 2, -6) == 8 && param_turn(&TP[P_SDIV], 2, 40) == 3;    /* 6 left: 2BAR; far: clamped */
    bad += check("#48 DIV / RATE / TIME step longest -> shortest, every value once, the ends hold", ok);
    ok = 1;                                         /* the real knob on the pages that show them, the gauge follows */
    for (id = 0; id < NPAGES; id++)
        for (c = 0; c < 4u; c++) {
            int16_t *vp;
            const param_desc_t *d;
            ui_power_on();
            ui.page = (uint8_t)id; page_entered();
            if (!page_visible(id) || !(d = page_desc(cur_page(), c, &vp)) || !vp || !enum_order(d))
                continue;
            found++;
            *vp = d->names == N_DIV ? 2 : 1;        /* 1/16 */
            turn(EN_K1 + c, 1);
            ok &= *vp == (d->names == N_DIV ? 5 : 4);  /* 16T */
            turn(EN_K1 + c, -2);
            ok &= *vp == (d->names == N_DIV ? 4 : 3);  /* 8T */
        }
    ui_power_on();                                  /* SEQ's DIV: SEQ TOOLS' KNOB 2 (1.2: the PATTERN page is gone) */
    go_page(GR_ROLL); frame();
    btn_down(B_SEQ); frames(500);
    TSEL->p[P_SDIV] = 2;
    turn(EN_K2, 1);
    ok &= ui.layer == LAYER_SEQ && TSEL->p[P_SDIV] == 5;
    turn(EN_K2, -2);
    ok &= TSEL->p[P_SDIV] == 4;
    btn_up(B_SEQ); frame();
    bad += check("#48 the pages' knobs step the divisions by length (SEQ TOOLS, ARP, DLY)", ok && found >= 3u);
    return bad;
}

/* #52 MENU > KNOB ACCEL (OFF by default, #23): ON, a sustained fast turn of a wide value goes x2..x4 (x8 over a range
 * above 64, #126); a slow turn, the start of a turn, a lone quick detent and a reversal are one step per detent; the
 * sign is always the detents' */
static uint32_t acc_ms(uint32_t role, const uint16_t *gaps, uint32_t n, int32_t dir, int32_t range, int32_t *out)
{
    uint32_t k, sum = 0;
    for (k = 0; k < n; k++) {
        fm1_ms += gaps[k];
        out[k] = accel(role, dir, range);
        sum += (uint32_t)(out[k] < 0 ? -out[k] : out[k]);
    }
    return sum;
}
/* DUELL 0.2.16: KNOB ACCEL ON, PATCHES' K1 steps by the turn speed and loads only the sound the turn ends on (ui.c pl_step / pl_poll);
 * OFF: one place a detent, each loaded, as before */
static uint32_t snd_sig(void) { return (uint32_t)TSEL->eng_req * 100003u + (uint32_t)TSEL->preset * 131u + (uint32_t)TSEL->user; }
static int test_patch_accel(void)
{
    int bad = 0, ok;
    uint32_t k, total, c0, c1, n, loads, sig, sig0;
    ui_power_on();
    ui_prefs |= PREF_ACCEL;
    memset(ui.enc_t, 0, sizeof ui.enc_t);
    go_page(GR_BROWSE);
    frames(500);
    c0 = pl_cursor(&total);
    ok = total > 100u && c0 < total;
    fm1_ms += 1000;
    sig0 = sig = snd_sig();
    for (k = 0, loads = 0; k < 12u; k++) {            /* a detent a frame (16 ms): 1 + 1 + 3 x 10 places */
        turn(EN_K1, 1);
        if (snd_sig() != sig) { loads++; sig = snd_sig(); }
    }
    c1 = pl_cursor(&total);
    ok &= c1 > c0 + 12u + 12u && pl_pend_on && loads <= 2u;
    ok &= snd_sig() != sig0 && snd_sig() == sig;      /* (the two slow detents loaded; the fast ones did not) */
    bad += check("PATCHES K1 with KNOB ACCEL ON: 12 fast detents move the cursor 32 places, only the first two (slow) detents load", ok);
    frames(40);                                      /* before the settle time: still waiting */
    ok = pl_pend_on && pl_cursor(&total) == c1;
    frames(300);
    (void)preset_pos(&n);
    ok &= !pl_pend_on && preset_pos(&n) == c1 && snd_sig() != sig;
    bad += check("  .. and after 150 ms the sound under the cursor loads (once), the cursor stays", ok);
    fm1_ms += 1000;                                  /* a turn left: the same speed back */
    for (k = 0; k < 40u; k++) turn(EN_K1, -1);
    ok = pl_pend_on && pl_cursor(&total) == 0u;
    frames(300);
    ok &= !pl_pend_on && preset_pos(&n) == 0u;
    bad += check("  .. a fast turn to the start stops at the first place (no wrap), the sound there loads", ok);
    fm1_ms += 1000;
    turn(EN_K1, -1);                                 /* a slow detent at the start: round to the last place, loaded at once */
    ok = !pl_pend_on && pl_cursor(&total) == total - 1u;
    bad += check("  .. a single slow detent wraps round the end and loads at once", ok);
    /* TYPE changes while a step waits: the waiting place is dropped */
    fm1_ms += 1000;
    for (k = 0; k < 10u; k++) turn(EN_K1, -1);
    ok = pl_pend_on;
    turn(EN_K2, 1);
    ok &= !pl_pend_on;
    bad += check("  .. TYPE (K2) drops the waiting place", ok);
    list_set(0u);
    /* KNOB ACCEL OFF: one place a detent, each loaded */
    ui_prefs &= ~PREF_ACCEL;
    frames(300);
    c0 = pl_cursor(&total);
    sig = snd_sig();
    for (k = 0, loads = 0; k < 12u; k++) {
        turn(EN_K1, 1);
        if (snd_sig() != sig) { loads++; sig = snd_sig(); }
    }
    c1 = pl_cursor(&total);
    ok = c1 == (c0 + 12u) % total && !pl_pend_on && loads >= 11u;
    bad += check("PATCHES K1 with KNOB ACCEL OFF: 12 detents are 12 places, each loaded, nothing waits", ok);
    return bad;
}
static int test_knob_accel(void)
{
    int bad = 0, ok;
    uint32_t k, seed = 12345u;
    int32_t o[16];
    static const uint16_t SLOW[8] = {500, 60, 60, 45, 41, 60, 80, 60}, MID[8] = {500, 30, 30, 30, 26, 30, 30, 30},
                          F20[8] = {500, 20, 20, 20, 20, 20, 20, 20}, F10[8] = {500, 10, 10, 10, 10, 10, 10, 10},
                          BOUNCE[8] = {500, 2, 300, 1, 300, 3, 300, 2};
    ui_power_on();
    ok = acc_ms(EN_K1, F10, 8, 1, 127, o) == 8u;                  /* OFF (the default) */
    ui_prefs = PREF_ACCEL;
    memset(ui.enc_t, 0, sizeof ui.enc_t);
    ok &= acc_ms(EN_K1, SLOW, 8, 1, 127, o) == 8u && acc_ms(EN_K2, MID, 8, -1, 127, o) == 8u && o[7] == -1;
    ok &= acc_ms(EN_K3, BOUNCE, 8, 1, 127, o) == 8u;
    bad += check("#52 KNOB ACCEL: OFF by default; ON, slow (>= 26 ms a detent) and lone quick detents: 1 step each", ok);
    fm1_ms += 1000;
    acc_ms(EN_K1, F20, 8, 1, 64, o);
    ok = o[0] == 1 && o[1] == 1 && o[2] == 2 && o[7] == 2;          /* 20 ms a detent: x2 from the third */
    acc_ms(EN_K1, F10, 8, 1, 64, o);                               /* (a pause of 500 ms first: a new turn) */
    ok &= o[0] == 1 && o[1] == 1 && o[2] == 4 && o[7] == 4;         /* 10 ms: x4 (the cap of a range <= 64) */
    fm1_ms += 15; ok &= accel(EN_K1, 1, 64) == 3;                  /* 15 ms: x3 */
    fm1_ms += 10; ok &= accel(EN_K1, -1, 64) == -1;                /* reversed: one step, the right sign */
    fm1_ms += 10; ok &= accel(EN_K1, -1, 64) == -1;                /* (the turn starts again) */
    fm1_ms += 10; ok &= accel(EN_K1, -1, 64) == -4;
    fm1_ms += 30; ok &= accel(EN_K1, -3, 64) == -12;               /* 3 detents in one read (10 ms each): x4 */
    bad += check("#52 KNOB ACCEL ON: 20 ms a detent x2, 15 ms x3, 10 ms x4 from a turn's third detent; reversal x1", ok);
    {   /* #126 a range above 64: 10 ms x5, 8 ms x6, 6 ms and less x8; a flick of 12 detents in ~120 ms, faster in
         * the middle, moves a -100..100 value ~50 (was 38); 12 even ones 52 */
        static const uint16_t FLICK[12] = {500, 16, 12, 10, 8, 7, 7, 8, 10, 12, 16, 20}, F6[8] = {500, 6, 6, 5, 4, 6, 6, 6};
        uint32_t f;
        fm1_ms += 1000;
        f = acc_ms(EN_K2, FLICK, 12, 1, 200, o);
        ok = f == 47u && o[0] == 1 && o[1] == 1;
        fm1_ms += 1000;
        ok &= acc_ms(EN_K2, F10, 8, -1, 200, o) == 2u + 6u * 5u && o[7] == -5;
        fm1_ms += 1000;
        ok &= acc_ms(EN_K2, F6, 8, 1, 127, o) == 2u + 6u * 8u;
        fm1_ms += 1000;
        ok &= acc_ms(EN_K2, F6, 8, 1, 64, o) == 2u + 6u * 4u;      /* (a range <= 64: x4 at most) */
        fm1_ms += 1000;
        ok &= acc_ms(EN_K2, F6, 8, 1, 0, o) == 8u;                  /* (a list) */
        printf("  #126 a flick of 12 detents over %u ms on -100..100: %u steps\n", 126u, (unsigned)f);
        bad += check("#126 KNOB ACCEL ON over a range above 64: 10 ms x5 .. 6 ms x8; a 12-detent flick ~120 ms: 47", ok);
    }
    fm1_ms += 1000; acc_ms(EN_K4, F10, 8, 1, 32, o);
    ok = o[7] == 1;                                                /* a narrow range (<= 32): never */
    fm1_ms += 1000; acc_ms(EN_K4, F10, 8, 1, 0, o);
    ok &= o[7] == 1;                                               /* a list of names (range 0 at the callers) */
    for (k = 0; k < 20000u && ok; k++) {                           /* random turns: the sign is the detents', at */
        int32_t s, r, gap;                                         /* most x8, a slow detent exactly one step */
        seed = seed * 1103515245u + 12345u;
        s = (int32_t)((seed >> 16) % 7u) - 3;
        gap = (int32_t)((seed >> 8) % 90u);
        fm1_ms += (uint32_t)gap;
        r = accel(EN_K1 + (seed & 3u), s, 127);
        ok &= s ? (r > 0) == (s > 0) && r * s <= 8 * s * s && r * s >= s * s : r == 0;
        if (gap >= 40 * (s < 0 ? -s : s) && r != s) ok = 0;
    }
    bad += check("#52 KNOB ACCEL: never on narrow values or lists; 20000 random reads keep the sign, x1..x8, slow = x1", ok);
    {   /* the real knob: LEVEL on MIXER, a detent a frame (16 ms): 1 + 1 + 3 x 6 = 20 for 8 detents; OFF: 8 */
        int16_t v0;
        ui_power_on(); ui_prefs = PREF_ACCEL;
        go_title("MIXER");
        TSEL->p[P_LEVEL] = 40; v0 = TSEL->p[P_LEVEL];
        fm1_ms += 1000;
        for (k = 0; k < 8u; k++) turn(EN_K1, 1);
        ok = TSEL->p[P_LEVEL] - v0 == 20;
        ui_prefs = 0; TSEL->p[P_LEVEL] = 40; fm1_ms += 1000;
        for (k = 0; k < 8u; k++) turn(EN_K1, 1);
        ok &= TSEL->p[P_LEVEL] == 48;
        bad += check("#52 KNOB ACCEL on MIXER LEVEL: a fast turn of 8 detents moves 20 (OFF: 8)", ok);
    }
    {   /* #126 GLO's LEVEL (KNOB 2 with GLO held): a flick of 12 detents ~16 ms apart goes round accel */
        int32_t g;
        ui_power_on(); ui_prefs = PREF_ACCEL;
        go_main_to(HP_MORPH); frames(64);
        trk[1].p[P_LEVEL] = 0;
        btn_down(B_GLO); frames(560);
        for (k = 0; k < 12u; k++) {
            host_enc[panel.enc[EN_K2]] += panel.dir[EN_K2];
            fm1_ms -= 6u;
            frame();
        }
        g = trk[1].p[P_LEVEL];
        btn_up(B_GLO); frames(400);
        ok = g > 12 + 12;
        bad += check("#126 KNOB ACCEL on GLO LEVEL (a 12-detent flick > 24)", ok);
    }
    {   /* saved with the settings; settings from before it: OFF */
        persist_t p;
        ui_prefs = PREF_ACCEL;
        settings_export(&p);
        ui_prefs = 0;
        ok = settings_import(&p, (int)sizeof p) == 1 && ui_prefs == PREF_ACCEL;
        memset(&p.favorites, 0, sizeof p.favorites);
        p.magic = PERSIST_MAGIC;                     /* (a record with no favourites bytes: the flag clear, as 0.2.13 left it)*/
        ok &= settings_import(&p, (int)sizeof p) == 1 && !ui_prefs;
        bad += check("#52 KNOB ACCEL saved with the settings; settings from before it: OFF", ok);
    }
    ui_prefs = 0;
    return bad;
}

/* #51 ("a melody appears when I change presets"): browsing every sound in the list (every engine's presets, then user
 * presets, one of them holding a pattern), on HOME and the PRESETS page, stopped and playing, on an empty track: no
 * step is written, the ARP stays off, nothing is held, the transport is not started, no message about a pattern. (In
 * 0.9 a preset carried a 16-step pattern and replaced the steps; since 1.0 only SEQ > PHRASES loads one.) */
static int test_browse_no_pattern(void)
{
    int bad = 0, ok = 1;
    uint32_t pass, n, total, loads = 0, pat_hints = 0;
    track_t *t;
    for (pass = 0; pass < 4u; pass++) {
        ui_power_on();
        t = TSEL;
        my_steps(t); t->p[P_SDIV] = 2;
        up_store(3, "MINE");                          /* a user preset that holds a pattern */
        up_store(5, "PLAIN");
        track_defaults_steps(t);
        ok &= seq_is_empty(t) && up_has_pat(3);
        if (pass & 1u) song.playing = 1;              /* playing: the other tracks run, this one is empty */
        if (pass & 2u) go_page(GR_BROWSE);
        preset_pos(&total);
        for (n = 0; n < total + 2u; n++) {
            turn(EN_PRESET, 1);
            loads++;
            pat_hints += preset_pat_hint() >= 0;
            if (!seq_is_empty(t) || t->p[P_AMODE] || t->nheld || (!(pass & 1u) && (song.playing || transport_req))) {
                printf("  pass %u, sound %u (%s %u): steps %d ARP %d held %u playing %d\n", pass, n,
                       ENGINES[t->eng_req % NENGINES]->name, t->preset, !seq_is_empty(t), t->p[P_AMODE], t->nheld,
                       song.playing);
                ok = 0;
                break;
            }
        }
    }
    if (!ok || loads < 200u || pat_hints <= 10u) printf("  ok %d loads %u hints %u\n", ok, loads, pat_hints);
    bad += check("#51 every sound browsed (HOME, PRESETS; stopped, playing): no step, ARP off, nothing held, no start",
                 ok && loads >= 200u && pat_hints > 10u);
    return bad;
}

/* MENU > USB LEVEL: MASTER (the default, every older setting) / FIXED; the audio follows at once (audio_test: the levels) */
static int test_usb_level(void)
{
    int bad = 0, ok;
    ui_power_on();
    frame();
    ok = !fx_usb_fixed && str_eq(MI_NAME[MI_USB], "USB LEVEL") && str_eq(menu_flag(MI_USB)->name[0], "MASTER");
    hold(B_HOME);
    ui.menu_sel = MI_USB;
    ui.force = 1; ui_draw();
    turn(EN_K1, 1); ok &= (ui_prefs & PREF_USB_FIXED) && fx_usb_fixed;      /* right: FIXED */
    turn(EN_K1, -1); ok &= !(ui_prefs & PREF_USB_FIXED) && !fx_usb_fixed;   /* left: MASTER */
    press(B_OCTUP); ok &= (ui_prefs & PREF_USB_FIXED) && fx_usb_fixed && ui.menu;
    press(B_OCTDN); ok &= !(ui_prefs & PREF_USB_FIXED) && !fx_usb_fixed && ui.menu;
    press(B_OCTUP);
    press(B_HOME);
    ok &= !ui.menu && fx_usb_fixed;
    {
        persist_t p;
        ui_prefs2 |= PREF_DEFAULTS >> 16;              /* (a 0.2.14 record: the one-time defaults done, read back as saved) */
        settings_export(&p);
        ui_prefs = 0;
        ok &= settings_import(&p, (int)sizeof p) == 1 && ui_prefs == PREF_USB_FIXED;
        memset(&p.favorites, 0, sizeof p.favorites);
        p.magic = PERSIST_MAGIC;                     /* (a record with no favourites bytes: the flag clear, as 0.2.13 left it)*/
        ok &= settings_import(&p, (int)sizeof p) == 1 && !(ui_prefs & PREF_USB_FIXED);   /* (0.2.14: + the new defaults' bits) */
        frame();
        ok &= !fx_usb_fixed;
    }
    bad += check("MENU > USB LEVEL: MASTER by default, KNOB 1 / OCT+ FIXED, left / OCT- MASTER, saved, the audio follows", ok);
    ui_prefs = 0;
    ui_prefs2 = 0;
    frame();
    return bad;
}

/* 1.1 (Discussion #131) MENU > AUDIO CLICK (OFF REC ON), CLICK LEVEL (LOW MID HIGH), COUNT-IN (OFF 1 BAR 2 BARS): OFF / MID /
 * OFF by default and in every older setting; a knob applies at once (click.c, seq.c); saved and read back, a stored 3
 * (no value) the default. The count-in from the UI: REC while stopped arms and counts in, the screen counts 4 3 2 1 (the
 * beat's cell moves), PLAY's green LED on each beat (its own dark), the STEP keys' entry paused, PLAY stops it (the page
 * back); at its end the sequencer plays and the page is back */
static uint16_t px_at(uint32_t x, uint32_t y) { return host_screen[y * 240u + x]; }
static void cin_beat(void)                       /* the audio until the count-in's next beat (or its end) */
{
    uint32_t left = cin_left, k;
    for (k = 0; k < 4000u && cin_left == left && seq_counting(); k++)
        events_block(CTL);
}
static int test_click_menu(void)
{
    int bad = 0, ok;
    uint32_t i;
    ui_power_on();
    frame();
    ok = !ui_rec_prefs && click_mode == CLICK_OFF && click_lvl == 1u && !cin_bars &&
         str_eq(MI_NAME[MI_CLICK], "CLICK") && str_eq(MI_NAME[MI_CLKLVL], "CLICK LEVEL") && str_eq(MI_NAME[MI_COUNTIN], "COUNT-IN") &&
         MI_TAB[MI_CLICK] == MTAB_AUDIO && MI_TAB[MI_CLKLVL] == MTAB_AUDIO && MI_TAB[MI_COUNTIN] == MTAB_AUDIO &&
         menu_get(MI_CLICK) == 0u && menu_get(MI_CLKLVL) == 1u && menu_get(MI_COUNTIN) == 0u;
    for (i = 0; i < 3u; i++)
        ok &= str_eq(menu_vname(MI_CLICK, i), (const char *[]){"OFF", "REC", "ON"}[i]) &&
              str_eq(menu_vname(MI_CLKLVL, i), (const char *[]){"LOW", "MID", "HIGH"}[i]) &&
              str_eq(menu_vname(MI_COUNTIN, i), (const char *[]){"OFF", "1 BAR", "2 BARS"}[i]);
    bad += check("MENU > AUDIO: CLICK OFF / REC / ON, CLICK LEVEL LOW / MID / HIGH, COUNT-IN OFF / 1 BAR / 2 BARS; OFF MID OFF by default", ok);
    hold(B_HOME);
    ui.menu_sel = MI_USB; ui.force = 1; ui_draw();
    turn(EN_PRESET, 1); turn(EN_PRESET, 1);
    ok = ui.menu_sel == MI_CLICK;
    turn(EN_K1, 1); ok &= click_mode == CLICK_REC;
    turn(EN_K2, 1); ok &= click_mode == CLICK_ON;
    turn(EN_K2, 1); ok &= click_mode == CLICK_ON;      /* (stops at the end) */
    turn(EN_PRESET, 1); ok &= ui.menu_sel == MI_CLKLVL;
    press(B_OCTDN); ok &= click_lvl == 0u;
    press(B_OCTUP); press(B_OCTUP); ok &= click_lvl == 2u;
    turn(EN_PRESET, 1); ok &= ui.menu_sel == MI_COUNTIN;
    turn(EN_K4, 1); ok &= cin_bars == 1u;
    turn(EN_K4, 1); ok &= cin_bars == 2u;
    turn(EN_PRESET, 1); ok &= ui.menu_sel == MI_TUNE;     /* (1.2: TUNE, AUDIO's last) */
    turn(EN_PRESET, 1); ok &= ui.menu_sel == MI_USB;   /* (wraps within AUDIO) */
    press(B_HOME);
    ok &= !ui.menu;
    bad += check("MENU > AUDIO: PRESETS to CLICK, CLICK LEVEL, COUNT-IN, TUNE; knobs / OCT step them, applied at once", ok);
    {
        persist_t p;
        settings_export(&p);
        ui_rec_prefs = 0; frame();
        ok = settings_import(&p, (int)sizeof p) == 1 && (frame(), click_mode == CLICK_ON && click_lvl == 2u && cin_bars == 2u);
        p.favorites.factory[15][28] = 3u | 3u << 2 | 1u << 4;   /* CLICK and LEVEL no value, COUNT-IN 1 BAR */
        ok &= settings_import(&p, (int)sizeof p) == 1 && p.favorites.factory[15][28] == 1u << 4;
        frame();
        ok &= click_mode == CLICK_OFF && click_lvl == 1u && cin_bars == 1u;
        memset(&p.favorites, 0, sizeof p.favorites);   /* settings from before the favorites (PER3): the defaults */
        p.magic = PERSIST_MAGIC;                     /* (a record with no favourites bytes: the flag clear, as 0.2.13 left it)*/
        ok &= settings_import(&p, (int)sizeof p) == 1 && !ui_rec_prefs;
        frame();
        ok &= click_mode == CLICK_OFF && click_lvl == 1u && !cin_bars;
    }
    bad += check("CLICK / CLICK LEVEL / COUNT-IN saved and read back; a stored 3 the default; older settings OFF / MID / OFF", ok);

    /* the count-in from the UI */
    ui_power_on();
    menu_put(MI_COUNTIN, 1u);
    go_page(GR_ROLL);
    frame();
    press(B_REC);                                       /* (#133: on STEP REC arms step recording, stopped) */
    ok = (song.rec & 1u) && transport_req == 0u && !live_rec_sel();
    press(B_PLAY);
    ok &= transport_req == 1u;
    events_block(CTL);
    ok &= seq_counting() && !song.playing && cin_left == 4u && live_rec_sel();
    frame();
    {
        uint16_t now0 = px_at(16, CI_Y + 4), next0 = px_at(16 + 56, CI_Y + 4);
        ok &= now0 != next0;                            /* the first beat's cell lit, the next not */
        ui_leds();
        ok &= (fm1_led[8] & 2u) != 0u;                  /* PLAY's green: on the beat */
        for (i = 0; i < 400u; i++) events_block(CTL);    /* (~0.3 s: past the first quarter of the beat) */
        ui_leds();
        ok &= !(fm1_led[8] & 2u);
        cin_beat();
        frame();
        ok &= cin_left == 3u && px_at(16 + 56, CI_Y + 4) == now0 && px_at(16, CI_Y + 4) == next0;   /* the cell moved */
        ui_leds();
        ok &= (fm1_led[8] & 2u) != 0u;
    }
    bad += check("count-in from the UI: REC on STEP arms, PLAY counts in (STEP entry paused); 4 3 2 1 on screen; PLAY's green on the beats", ok);
    press(B_PLAY);
    ok = transport_req == 2u;
    events_block(CTL);
    frame();
    ok &= !seq_counting() && !song.playing && (song.rec & 1u) && px_at(16, CI_Y + 4) == px_at(16 + 56, CI_Y + 4);
    bad += check("PLAY while counting in stops it: nothing plays, still armed, the page back", ok);
    press(B_PLAY);
    events_block(CTL);
    ok = seq_counting() && cin_left == 4u;
    for (i = 0; i < 4u; i++) cin_beat();
    frame();
    ok &= !seq_counting() && song.playing && trk[0].seq_idx == 0u && px_at(16, CI_Y + 4) == px_at(16 + 56, CI_Y + 4);
    bad += check("PLAY armed and stopped: the count-in again; at its end the sequencer plays from step 1, the page back", ok);
    press(B_PLAY);
    events_block(CTL);
    menu_put(MI_COUNTIN, 0u);
    stop_transport();
    return bad;
}

/* #67 MENU > USB SERIAL: ON by default (and in every older setting): the console presented, the descriptors as
 * before. OFF: audio + MIDI only, device class 0, bcdUSB 1.10 (CFG_DESC_PLAIN: a FELUCCA_CDC=0 build's bytes), applied
 * when the menu closes (one re-enumeration); saved; at boot the saved choice before USB starts (never on then off) */
static int dev_class(void)                              /* the device class GET_DESCRIPTOR sends now (-1: none) */
{
    const uint8_t *d;
    uint16_t n;
    return get_desc(0x0100, &d, &n) && n == 18u ? d[4] : -1;
}
static int dev_is(const uint8_t *want)                  /* the device descriptor sent now = want */
{
    const uint8_t *d;
    uint16_t n;
    return get_desc(0x0100, &d, &n) && n == 18u && !memcmp(d, want, 18);
}
static int plain_desc(void)                             /* device + configuration = the console-less bytes */
{
    const uint8_t *d, *c;
    uint16_t n, m;
    return get_desc(0x0100, &d, &n) && get_desc(0x0200, &c, &m) && n == 18u && !memcmp(d, DEV_DESC_PLAIN, 18) &&
           m == sizeof CFG_DESC_PLAIN && !memcmp(c, CFG_DESC_PLAIN, m);
}
static int test_usb_serial(void)
{
    int bad = 0, ok;
    ui_power_on();
    usb.up = 0;                                          /* (no SIE here: usb_cdc_switch only flips the descriptors) */
    frame();
    ok = usb_cdc_on && dev_class() == 0xEF && !(ui_prefs & PREF_SERIAL_OFF) && str_eq(MI_NAME[MI_SERIAL], "USB SERIAL") &&
         str_eq(menu_flag(MI_SERIAL)->name[0], "ON") && str_eq(menu_flag(MI_SERIAL)->name[1], "OFF");
    ok &= dev_is(DEV_DESC);
    bad += check("#67 MENU > USB SERIAL: the bit clear is ON, the console presented (misc / IAD, as before; 0.2.14: OFF is the default, persistence_test)", ok);
    hold(B_HOME);
    ui.menu_sel = MI_SERIAL;
    ui.force = 1; ui_draw();
    turn(EN_K1, -1);
    ok = (ui_prefs & PREF_SERIAL_OFF) && usb_cdc_on;    /* left: OFF; applied when the menu closes */
    turn(EN_K1, 1); ok &= !(ui_prefs & PREF_SERIAL_OFF);   /* right: ON (the switch's knob right) */
    press(B_OCTDN); ok &= (ui_prefs & PREF_SERIAL_OFF) && ui.menu && usb_cdc_on;   /* OCT-: OFF */
    press(B_HOME);
    ok &= ui.menu == 0 && usb_cdc_on;                 /* (HOME acts as it is let go: closed this frame) */
    frame();
    ok &= !ui.menu && !usb_cdc_on && !usb.config && plain_desc() && dev_class() == 0;
    bad += check("  KNOB 1 left / OCT- OFF, right ON; the menu closed (HOME): audio + MIDI only, class 0", ok);
    {
        persist_t p;
        ui_prefs2 |= PREF_DEFAULTS >> 16;              /* (a 0.2.14 record: the one-time defaults done, read back as saved) */
        settings_export(&p);
        ui_prefs = 0;
        ok = settings_import(&p, (int)sizeof p) == 1 && ui_prefs == PREF_SERIAL_OFF;
        ok &= settings_import(&p, (int)sizeof p) == 1 && ui_prefs == PREF_SERIAL_OFF;     /* (again: the same) */
        /* boot (main.c): the settings, usb_serial_apply, then usb_start: the console-less descriptors from the start */
        usb_cdc_on = 1;
        usb.up = 0;
        usb_serial_apply();
        ok &= !usb_cdc_on && !usb.up && plain_desc();
        settings_export(&p);                            /* a 0.2.14 record with ON (the user's choice): ON, the console */
        p.favorites.factory[15][30] &= (uint8_t)~PREF_SERIAL_OFF;
        ok &= settings_import(&p, (int)sizeof p) == 1 && !(ui_prefs & PREF_SERIAL_OFF);
        usb_serial_apply();
        ok &= usb_cdc_on && dev_is(DEV_DESC);
        memset(&p.favorites, 0, sizeof p.favorites);   /* settings from before it (PER3): ON there, OFF once now (0.2.14's default) */
        p.magic = PERSIST_MAGIC;                     /* (a record with no favourites bytes: the flag clear, as 0.2.13 left it)*/
        ok &= settings_import(&p, (int)sizeof p) == 1 && (ui_prefs & PREF_SERIAL_OFF);
        usb_serial_apply();
        ok &= !usb_cdc_on && plain_desc();
    }
    bad += check("  saved with the settings (a 0.2.14 ON stays ON; older ones: OFF once); at boot the device enumerates with the saved choice", ok);
    {   /* OFF: the soft key and the update frames still come in (USB-MIDI SysEx, EP1) */
        static const uint8_t KEY[8] = {0x04, 0xF0, 0x22, 0x24, 0x07, 0x35, 0x7D, 0xF7};
        ui_prefs = PREF_SERIAL_OFF;
        frame();
        usb.uboot_req = 0;
        ok = !usb_cdc_on && ep1_take(KEY, sizeof KEY) && usb.uboot_req;
        usb.uboot_req = 0;
        bad += check("  OFF: the soft key (SysEx on the MIDI endpoint) still asks for UPDATE MODE", ok);
    }
    ui_prefs = 0;
    ui_prefs2 = 0;
    frame();
    return bad;
}

/* MENU (1.0.5: in tabs): every row reachable with SELECT, each tab its own page; #46 MENU > ANIM: ON by
 * default (and in every older setting), OFF snaps the rolling digits, the piano roll and the menu's tabs */
static int test_menu_prefs(void)
{
    int bad = 0, ok = 1;
    uint32_t k, j, seen = 0;
    static uint16_t shot[MTAB_COUNT][240 * 240];
    ui_power_on();
    ui_prefs = 0;
    hold(B_HOME);
    ui.menu_sel = 0;
    for (k = 0; k < MTAB_COUNT; k++) {              /* SELECT right through every row, across the tabs */
        ok &= menu_tab() == k && ui.menu_sel == mtab_first(k);
        for (j = 0; j < mtab_rows(k); j++) {
            seen |= 1u << ui.menu_sel;
            ok &= menu_tab() == k;
            if (j + 1u < mtab_rows(k)) turn(EN_SELECT, 1);
        }
        ui.force = 1; ui_draw();                    /* (the tab's last row shown) */
        memcpy(shot[k], host_screen, sizeof shot[k]);
        for (j = 0; j < k; j++) ok &= memcmp(shot[j], shot[k], sizeof shot[k]) != 0;
        turn(EN_SELECT, 1);                         /* the next tab's first row; the last: stays */
    }
    ok &= seen == (1u << MI_COUNT) - 1u && ui.menu_sel == MI_COUNT - 1u;
    bad += check("MENU: SELECT reaches every row across the tabs, each tab a page of its own", ok);
    ui.menu_sel = 0;
    /* ANIM */
    ui.menu_sel = MI_ANIM;
    ok = str_eq(MI_NAME[MI_ANIM], "ANIMATION") && !(ui_prefs & PREF_ANIM_OFF);
    turn(EN_K1, 1); ok &= !(ui_prefs & PREF_ANIM_OFF) && (ui_idle & 1u) && menu_get(MI_ANIM) == 2u;   /* right: IDLE (1.2) */
    turn(EN_K1, 1); ok &= (ui_idle & 1u) && str_eq(menu_vname(MI_ANIM, menu_get(MI_ANIM)), "IDLE");  /* (the end) */
    turn(EN_K1, -1); ok &= !(ui_prefs & PREF_ANIM_OFF) && !ui_idle && menu_get(MI_ANIM) == 0u;      /* left: ON */
    turn(EN_K1, -1); ok &= (ui_prefs & PREF_ANIM_OFF) != 0 && !ui_idle;                             /* left: OFF */
    turn(EN_K1, -1); ok &= (ui_prefs & PREF_ANIM_OFF) != 0;                                         /* (the end) */
    turn(EN_K1, 1); ok &= !(ui_prefs & PREF_ANIM_OFF) && !ui_idle;                                  /* right: ON */
    press(B_OCTDN); ok &= (ui_prefs & PREF_ANIM_OFF) && ui.menu;  /* OCT-: OFF */
    ok &= !(ui_prefs & ~(uint32_t)PREF_ANIM_OFF);                 /* (no other setting moved) */
    press(B_HOME);
    bad += check("#46 MENU > ANIM: ON by default, KNOB 1 / OCT steps OFF ON IDLE (1.2), the values 0 ON 1 OFF 2 IDLE",
                 ok && !ui.menu && str_eq(ANIM_N[0], "ON") && str_eq(ANIM_N[1], "OFF"));
    ui_prefs = 0;
    ui_idle = 0;
    ui_power_on(); roll_settle();
    turn(EN_K4, 1);
    ok = song.g[G_TUNE] == 1 && ui.roll[3].from[0];              /* ON: TUNE 0 -> 1 rolls */
    ui_power_on(); ui_prefs = PREF_ANIM_OFF; roll_settle();      /* (ui_power_on clears the favorites record) */
    turn(EN_K4, 1);
    ok &= song.g[G_TUNE] == 1 && !ui.roll[3].from[0] && roll_static_now();   /* OFF: the value at once, static */
    bpm_nudge(1);
    ok &= !ui.roll[ROLL_BPM].from[0];
    bad += check("#46 ANIM OFF: the card values and the header BPM snap (no rolling digits), the frame is static", ok);
    {   /* the piano roll's view: ON glides toward the notes over frames, OFF jumps there at once */
        track_t *t = TSEL;
        uint32_t lo_on;
        ui_power_on();
        for (k = 0; k < NSTEP; k++) step_clear(&t->step[k]);
        t->step[0].n = 1; t->step[0].note[0] = 30; t->step[0].time = ST_NOTE;
        ui.force = 0;
        proll.init = 0; pr_follow(t); ui.frame++;
        t->step[0].note[0] = 90; pr_follow(t); lo_on = proll.lo;
        ui_prefs = PREF_ANIM_OFF;
        proll.init = 0; t->step[0].note[0] = 30; pr_follow(t); ui.frame++;
        t->step[0].note[0] = 90; pr_follow(t);
        ok = proll.moving == 0 && proll.lo > lo_on && lo_on != 0u;
        if (!ok) printf("  piano roll view: ON %u, OFF %u (moving %u)\n", lo_on, proll.lo, proll.moving);
        bad += check("#46 ANIM OFF: the piano roll's view jumps to the notes (ON: it glides)", ok);
    }
    {   /* saved with the settings; settings from before it: ON */
        persist_t p;
        ui_prefs = PREF_ANIM_OFF;
        settings_export(&p);
        ui_prefs = 0;
        ok = settings_import(&p, (int)sizeof p) == 1 && (ui_prefs & PREF_ANIM_OFF);
        memset(&p.favorites, 0, sizeof p.favorites);
        p.magic = PERSIST_MAGIC;                     /* (a record with no favourites bytes: the flag clear, as 0.2.13 left it)*/
        ok &= settings_import(&p, (int)sizeof p) == 1 && !(ui_prefs & PREF_ANIM_OFF);   /* (0.2.14: + the new defaults) */
        bad += check("#46 ANIM saved with the settings; settings from before it: ON", ok);
    }
    ui_prefs = 0;
    return bad;
}

/* DUELL 0.2.2: FLAT is the only look. MENU has no STYLE (and no LARGE, no HELP: their ids are retired); a stored STYLE byte of
 * an older setting (LINE 1, the retired PIXEL 2, anything) and a stored LARGE / HELP bit are ignored: the SURF cards stay, the
 * normal layout stays, and the settings record round-trips untouched (idempotent) */
static int test_flat_only(void)
{
    int bad = 0, ok = 1;
    uint32_t i;
    persist_t p, q;
    ui_power_on();
    frame();
    for (i = 0; i < MI_COUNT; i++)
        ok &= strcmp(MI_NAME[i], "STYLE") && strcmp(MI_NAME[i], "LARGE") && strcmp(MI_NAME[i], "HELP");
    ok &= ux.surf != ux.bg && T_SURF != T_BG && ui_prefs == 0u;
    bad += check("MENU: no STYLE, LARGE or HELP row; the cards are SURF (FLAT)", ok);
    settings_export(&p);
    p.favorites.factory[15][29] = 1;                    /* LINE of an older setting */
    p.favorites.factory[15][30] = (uint8_t)(p.favorites.factory[15][30] | 32u);       /* LARGE ON */
    p.favorites.factory[15][24] = (uint8_t)(p.favorites.factory[15][24] | 8u);        /* HELP ON */
    ok = settings_import(&p, (int)sizeof p) == 1;
    q = p;
    ok &= settings_import(&q, (int)sizeof q) == 1 && !memcmp(&p, &q, sizeof p);
    ui.force = 1; frame();
    ok &= ux.surf != ux.bg && T_SURF != T_BG;           /* still the FLAT look */
    p.favorites.factory[15][29] = 2;                    /* the retired PIXEL, and an unknown value */
    ok &= settings_import(&p, (int)sizeof p) == 1;
    p.favorites.factory[15][29] = 7;
    ok &= settings_import(&p, (int)sizeof p) == 1;
    ui.force = 1; frame();
    ok &= ux.surf != ux.bg && T_SURF != T_BG;
    bad += check("an older setting with STYLE LINE (or PIXEL, or junk), LARGE ON, HELP ON: imported, ignored, FLAT drawn", ok);
    ui_power_on(); frame();
    return bad;
}

/* the header (ui_draw.c draw_head, 1.2's three slots): its elements measured in the render (every pixel off BG; an
 * element is a run of inked columns, split at 2 empty columns and at the battery's cell), each one's ink centred on row
 * 12 (top + bottom + 1 within 1 px of H_HEAD); the track cushion's ink from x 4; the battery's in 214..236, nothing
 * between 208 and it; the page name's ink within 2 px of x 120 (the octave shown: left of it); a message / a label centred
 * on x 120 (its ink between 40 and 205, within 3 px). Every state of the header, every palette */
#define HEAD_EL 32
static int32_t hel[HEAD_EL][4], nhel;                  /* x0 x1 (last inked column) top bot */
static int head_centres(const char *what, int print)
{
    uint16_t bg = swap16(T_BG);
    int32_t x = 0, ok = 1;
    nhel = 0;
    while (x < 240) {
        int32_t x0, x1, top = 99, bot = -1, gap = 0, y, ink;
        for (; x < 240; x++) {                        /* the next inked column */
            for (y = 0, ink = 0; y < H_HEAD; y++) ink |= host_screen[(Y_HEAD + y) * 240 + x] != bg;
            if (ink) break;
        }
        if (x >= 240) break;
        x0 = x1 = x;
        for (; x < 240 && gap < 2; x++) {
            if (x == HEAD_BAT_X && x != x0) break;     /* the battery's cell ends the element */
            for (y = 0, ink = 0; y < H_HEAD; y++)
                if (host_screen[(Y_HEAD + y) * 240 + x] != bg) {
                    ink = 1;
                    if (y < top) top = y;
                    if (y > bot) bot = y;
                }
            gap = ink ? 0 : gap + 1;
            if (ink) x1 = x;
        }
        x = x1 + 1;
        if (print) printf("    head %-10s x %3d..%3d ink rows %2d..%2d centre %4.1f (%+.1f)\n", what, x0, x1, top, bot,
                          (top + bot + 1) / 2.0, (top + bot + 1) / 2.0 - H_HEAD / 2);
        ok &= top + bot + 1 >= H_HEAD - 2 && top + bot + 1 <= H_HEAD + 2;
        if (nhel < HEAD_EL) { hel[nhel][0] = x0; hel[nhel][1] = x1; hel[nhel][2] = top; hel[nhel][3] = bot; nhel++; }
    }
    return ok;
}
/* the elements inside columns a .. b as one span (x0 .. x1): 2 x its centre, 0 none */
static int32_t head_span2(int32_t a, int32_t b, int32_t *x0, int32_t *x1)
{
    int32_t i;
    *x0 = 999; *x1 = -1;
    for (i = 0; i < nhel; i++)
        if (hel[i][0] >= a && hel[i][1] <= b) {
            if (hel[i][0] < *x0) *x0 = hel[i][0];
            if (hel[i][1] > *x1) *x1 = hel[i][1];
        }
    return *x1 < 0 ? 0 : *x0 + *x1 + 1;
}
enum { HS_STOPPED, HS_PLAY_REC, HS_MSG, HS_OCT, HS_USB, HS_KEYCAP, HS_REC_OTHER,
       HS_NOFILE, HS_REC_LAYER, HS_LAYER_LOCK, HS_GLO_LAYER, HS_MSG2, HS_COUNTIN, HS_BATT1, HS_BATT0, HS_LONG, HS_LONG_OCT, HS_N };
static const char *const HS_NAME[HS_N] = {"stopped", "play+REC", "message", "OCT", "USB", "keycap", "REC other",
    "NO SAMPLE", "REC layer", "EDIT lock", "GLO layer", "msg2", "count-in", "battery 1", "battery 0", "long name", "long+OCT"};
static void head_state(uint32_t s)
{
    song.playing = s == HS_PLAY_REC || s == HS_OCT;
    song.rec = s == HS_PLAY_REC || s == HS_COUNTIN ? 1u << song.sel : s == HS_REC_OTHER ? 1u << ((song.sel + 1u) % NTRK) : 0u;
    if (s == HS_MSG) ui_message("SAVED 03 BASS");
    if (s == HS_MSG2) ui_say("LOADED ", "07 A VERY LONG PATTERN NAME");
    if (s == HS_OCT || s == HS_LONG_OCT) song.octave = -8;
    if (s == HS_LONG || s == HS_LONG_OCT) { page_over = 0; go_title("OP TUNE"); song.rec = 1u << song.sel; }
    usb.config = s == HS_USB; usb.suspended = 0;
    song.batt_raw = s == HS_BATT1 ? 540 : s == HS_BATT0 ? 500 : 600;
    if (s == HS_KEYCAP) ui_message("[OCT+] SAVE");
    if (s == HS_NOFILE) ui_message(MSG_NO_ENGINE);
    if (s == HS_REC_LAYER) { song.rec = 1u << song.sel; ui.layer = LAYER_REC; }
    if (s == HS_LAYER_LOCK) ui.layer = ui.lock = LAYER_EDIT;
    if (s == HS_GLO_LAYER) ui.layer = LAYER_GLO;
    if (s == HS_COUNTIN) { cin_total = 4; cin_left = 3; }
}
static void head_clear(void)
{
    ui.msg_t = 0; song.octave = 0; usb.config = 0; song.playing = 0; song.rec = 0; ui.bpm_t = 0;
    ui.layer = ui.lock = 0; cin_total = cin_left = 0; song.batt_raw = 600;
}
static int test_head_centres(void)
{
    int bad = 0, ok = 1, place = 1;
    uint32_t st, p, s;
    for (st = 0; st < 1u; st++)
        for (p = 0; p < NPALETTES; p++)
            for (s = 0; s < HS_N; s++) {
                int32_t a, b, c2, msg = s == HS_MSG || s == HS_KEYCAP || s == HS_NOFILE || s == HS_REC_LAYER ||
                                        s == HS_LAYER_LOCK || s == HS_MSG2;
                int pr = !st && !p, okp;
                ui_power_on();
                settings.palette = p; palette_set(p);
                head_state(s);
                memset(host_screen, 0, sizeof host_screen);
                ui.force = 1; draw_head(); ui.force = 0;
                ok &= head_centres(HS_NAME[s], pr);
                okp = nhel > 0 && hel[0][0] == 4;      /* the track cushion first, its ink from x 4 */
                okp &= head_span2(HEAD_BAT_X, 239, &a, &b) && a >= 214 && b <= 236;   /* the battery last, in its cell; nothing between 208 and it */
                okp &= !head_span2(HEAD_GRP_R, HEAD_BAT_X - 1, &a, &b) || s == HS_OCT || s == HS_LONG_OCT;
                if (!msg && s != HS_OCT && s != HS_LONG_OCT) {   /* the page name's ink on x 120 (between the part badges and the battery) */
                    c2 = head_span2(50, 205, &a, &b);
                    okp &= c2 >= 236 && c2 <= 244;
                } else if (!msg) {                     /* the octave shown: the name left of it, both inside the room */
                    c2 = head_span2(50, 205, &a, &b);
                    okp &= c2 > 0 && b <= HEAD_GRP_R;
                    okp &= head_span2(HEAD_GRP_R - 40, HEAD_GRP_R, &a, &b) && b >= HEAD_GRP_R - 3 && b <= HEAD_GRP_R;   /* the octave, ending at 208 */
                } else {                               /* the message centred on x 120 */
                    c2 = head_span2(40, 205, &a, &b);
                    okp &= c2 >= 234 && c2 <= 246 && a >= 40 && b <= 205;
                }
                if (pr && !okp) printf("    head %s: misplaced\n", HS_NAME[s]);
                place &= okp;
                head_clear();
            }
    bad += check("header: every element's ink centred on row 12 (+-1 px), every state, every palette", ok);
    bad += check("header: three slots: track at 4, page name / message on x 120, octave to 208, battery 214..236", place);
    ui_power_on();
    return bad;
}

/* DUELL 0.2.9: the CPU bar under the header's battery (ui_draw.c cpu_bar_poll / draw_cpu_bar; MENU > DISPLAY > CPU METER). The load of the
 * last second from the audio counters (cpu_sum / cpu_n / cpu_miss), 8 steps, amber from 70 %, red from 85 %, the whole bar red for the
 * second after a missed half; the header redraws only when the step or colour changes; the setting's bit (ui_prefs2 bit 4, clear = ON) */
static void cpu_window(uint32_t pct, uint32_t miss)    /* a second of 100 halves at pct % of their time, then the bar's poll */
{
    fm1_ms += 1000u;
    cpu_n += 100u;
    cpu_sum += pct * CPU_BUDGET_US;
    cpu_miss += miss;
    cpu_bar_poll();
}
static uint16_t cpu_px(int32_t dx, int32_t dy)         /* the header's pixel at the bar's (dx, dy) */
{
    return swap16(host_screen[(uint32_t)(Y_HEAD + HEAD_CPU_Y + dy) * 240u + (uint32_t)(HEAD_CPU_X + dx)]);
}
static int test_cpu_bar(void)
{
    int bad = 0, ok = 1;
    static const struct { uint16_t pct; uint8_t step, lvl; } TH[] = {
        {0, 0, 0}, {1, 1, 0}, {12, 1, 0}, {13, 2, 0}, {50, 4, 0}, {69, 6, 0}, {70, 6, 1}, {84, 7, 1}, {85, 7, 2}, {100, 8, 2}, {170, 8, 2}};
    uint32_t i, p, sig, a, b;
    persist_t pr;
    ui_power_on();
    ui_prefs2 = 7u;                                     /* an older setting (the three bits of 1.2): the bar is ON */
    ok = !menu_get(MI_CPU) && str_eq(MI_NAME[MI_CPU], "CPU METER") && MI_TAB[MI_CPU] == MTAB_DISPLAY && menu_n(MI_CPU) == 2u &&
         str_eq(menu_vname(MI_CPU, 0), "ON") && str_eq(menu_vname(MI_CPU, 1), "OFF") && mtab_pages(MTAB_DISPLAY) == 1u;
    bad += check("0.2.9 MENU > DISPLAY > CPU METER: ON / OFF, ON in an older setting (bit clear), the DISPLAY tab still fits", ok);
    menu_put(MI_CPU, 1);
    ok = menu_get(MI_CPU) == 1u && ui_prefs2 == (7u | 0x10u) && !ui_prefs && !ui_rec_prefs && menu_step(MI_CPU, 1) == 0u;
    ui_prefs2 |= 0x80u;                                 /* (0.2.14: a record with the new defaults done, so the bytes read back as saved) */
    settings_export(&pr);
    menu_put(MI_CPU, 0);
    ok &= !menu_get(MI_CPU) && ui_prefs2 == (7u | 0x80u) && menu_step(MI_CPU, -1) == 1u;
    ok &= settings_import(&pr, sizeof pr) == 1 && menu_get(MI_CPU) == 1u && ui_prefs2 == (7u | 0x10u | 0x80u);   /* the saved OFF comes back */
    menu_put(MI_CPU, 0);
    settings_export(&pr);
    menu_put(MI_CPU, 1);
    ok &= settings_import(&pr, sizeof pr) == 1 && !menu_get(MI_CPU);
    pr.favorites.factory[15][24] = 7;                   /* an older record: its byte holds the three old bits only */
    ok &= settings_import(&pr, sizeof pr) == 1 && !menu_get(MI_CPU) && ui_prefs2 == (7u | 0x80u);   /* (+ 0.2.14's flag: its defaults done once) */
    bad += check("0.2.9 CPU METER OFF is saved with the settings (its own bit, the others stay) and read back; an older record loads ON", ok);
    ui_prefs2 = 0;
    ok = 1;
    for (i = 0; i < NELEM(TH); i++) {                   /* the steps and colour levels by load */
        memset(&cpub, 0, sizeof cpub);
        cpu_window(0, 0);                               /* (the first poll only sets the window) */
        cpu_window(TH[i].pct, 0);
        ok &= cpub.step == TH[i].step && cpub.lvl == TH[i].lvl;
        if (!ok) { printf("    CPU bar %u %%: step %u level %u (want %u %u)\n", TH[i].pct, cpub.step, cpub.lvl, TH[i].step, TH[i].lvl); break; }
    }
    bad += check("0.2.9 CPU bar: 8 steps (any load shows one), theme colour below 70 %, amber from 70 %, red from 85 %, full from 100 %", ok);
    memset(&cpub, 0, sizeof cpub);
    cpu_window(0, 0);
    cpu_window(20, 0);
    ok = cpub.step == 2u && cpub.lvl == 0u;
    cpu_window(20, 1);                                  /* a half missed its time: the whole bar red */
    ok &= cpub.step == 8u && cpub.lvl == 2u;
    fm1_ms += 500u;
    cpu_bar_poll();                                     /* (inside the window: unchanged, no second read) */
    ok &= cpub.step == 8u && cpub.lvl == 2u;
    cpu_window(20, 0);                                  /* the next second without a miss: back to the load */
    ok &= cpub.step == 2u && cpub.lvl == 0u;
    fm1_ms += 9000u;                                    /* no header drawn for a long time (a menu): a fresh window, the old miss is not shown */
    cpu_n += 100u; cpu_sum += 20u * CPU_BUDGET_US; cpu_miss += 3u;
    cpu_bar_poll();
    ok &= cpub.step == 2u && cpub.lvl == 0u;
    bad += check("0.2.9 CPU bar: a missed block flashes the whole bar red for the next second; a long gap starts a fresh window", ok);
    /* the header: redraws when the level changes only; the bar's pixels */
    ok = 1;
    for (p = 0; p < NPALETTES; p++) {
        ui_power_on(); settings.palette = p; palette_set(p);
        ui_prefs2 = 0;
        memset(&cpub, 0, sizeof cpub);
        cpu_window(0, 0);
        cpu_window(55, 0);                              /* step 5 of 8, theme colour: 13 px of 22 */
        memset(host_screen, 0, sizeof host_screen);
        ui.force = 1; draw_head(); ui.force = 0;
        sig = ui.head_sig;
        ok &= cpub.step == 5u && cpu_px(0, 0) != T_BG && cpu_px(0, 0) != T_DIM && cpu_px(12, 4) == cpu_px(0, 0) &&
              cpu_px(13, 1) == T_DIM && cpu_px(21, 1) == T_DIM && cpu_px(-1, 1) == T_BG && cpu_px(22, 1) == T_BG &&
              cpu_px(0, -1) == T_BG;
        ok &= ux_cr(cpu_px(0, 0), T_BG) >= 300u;        /* legible against the header in every palette (graphic 3:1) */
        cpu_window(55, 0); draw_head();                 /* the same level a second later: no redraw */
        ok &= ui.head_sig == sig;
        cpu_window(56, 0); draw_head();                 /* 56 % is step 5 too (4.48 rounds up) */
        ok &= ui.head_sig == sig;
        cpu_window(75, 0); draw_head();                 /* amber: step 6, another colour */
        ok &= ui.head_sig != sig && cpub.step == 6u && cpub.lvl == 1u && cpu_px(0, 0) == cpu_col(1u, T_BG) && cpu_px(16, 1) == T_DIM;
        if (!ux.mono && !ux.bw) {
            a = cpu_px(0, 0);
            cpu_window(90, 0); draw_head();
            b = cpu_px(0, 0);
            ok &= b == T_REC && a != b && cpu_px(21, 1) == b;
        }
        cpu_window(10, 1); draw_head();                 /* a missed half: all 22 px, REC red (MONO: the text colour) */
        ok &= cpub.step == 8u && cpu_px(21, 0) == cpu_col(2u, T_BG) && cpu_px(0, 4) == cpu_px(21, 0);
        ui_prefs2 = PREF_CPU_OFF >> 16;                 /* OFF: no pixel of it, the header redraws */
        draw_head();
        {
            uint32_t x, y, ink = 0;
            for (y = 0; y < 5u; y++)
                for (x = 0; x < 22u; x++)
                    ink |= cpu_px((int32_t)x, (int32_t)y) != T_BG;
            ok &= !ink && ui.head_sig != sig;
        }
        if (!ok) { printf("    CPU bar: palette %u (%s) fails\n", p, UI_PALETTES[p].name); break; }
    }
    bad += check("0.2.9 CPU bar: x 215..236, y 19..23 (5 px) in the header, the fill by the step, the track dim, theme / amber / REC colours legible in every palette; redraws on a level change only; OFF draws nothing", ok);
    {   /* the battery's ink stays above the bar (rows < 19) and the bar takes no room from the octave / page name (they end at x 208 / 212) */
        uint32_t x, y;
        ok = 1;
        ui_power_on(); ui_prefs2 = 0;
        memset(&cpub, 0, sizeof cpub);
        song.batt_raw = 600; song.octave = -8;
        ui.force = 1; draw_head(); ui.force = 0;
        for (y = 0; y < 24u; y++)
            for (x = 208; x < 240u; x++)
                if (swap16(host_screen[y * 240u + x]) != T_BG && x < 214u)
                    ok = 0;                             /* (nothing between the octave's end at 208 and the battery) */
        for (y = 19; y < 24u; y++)
            for (x = 214; x < 240u; x++)
                if (swap16(host_screen[y * 240u + x]) != T_BG && !(y >= 19u && y <= 23u && x >= 215u && x <= 236u))
                    ok = 0;                             /* (the battery's ink ends above row 19: only the bar's 22 x 5 px are below) */
        ok &= HEAD_CPU_X == 215 && HEAD_CPU_Y == 19 && HEAD_CPU_W == 22 && HEAD_CPU_H == 5;
        song.octave = 0;
        bad += check("0.2.9 CPU bar: under the battery (its ink ends above row 19), nothing else moved: no ink between x 208 and 214", ok);
    }
    ui_prefs2 = 0;
    ui_power_on();
    return bad;
}

/* DUELL 0.2.10 (owner): the header has no multi number, no asterisk and no REC mark, on any page. The armed-for-recording mark is a red dot
 * and "REC" in the footer's second row (ui_draw.c foot_rec_mark), on every page while any part is armed: filled dot = the selected part,
 * outlined = another one only */
static uint32_t head_ink_cols(int32_t x0, int32_t x1)  /* header columns x0..x1 with anything not BG */
{
    uint32_t n = 0;
    for (int32_t x = x0; x <= x1; x++)
        for (int32_t y = 2; y < 22; y++)
            if (swap16(host_screen[(uint32_t)y * 240u + (uint32_t)x]) != T_BG) { n++; break; }
    return n;
}
static uint32_t foot_rec_px(void)                       /* footer pixels in the REC colour */
{
    uint32_t n = 0, x, y;
    for (y = 0; y < H_FOOT; y++)
        for (x = 0; x < 240u; x++)
            n += swap16(host_screen[(Y_FOOT + y) * 240u + x]) == T_REC;
    return n;
}
static int test_head_rec_foot(void)
{
    int bad = 0, ok = 1, fok = 1;
    uint32_t pg, n_self = 0, n_other = 0;
    ui_power_on();
    settings.palette = 0; palette_set(0);
    frames(50);
    project_save(1); ui.msg_t = 0; frames(50);
    trk[0].step[0].note[0] = 70; trk[0].step[0].n = 1;
    for (pg = 0; pg < 4u; pg++) {
        if (pg == 1u) open_family(FAM_SEQ);
        if (pg == 2u) open_family(FAM_EDIT);
        if (pg == 3u) go_main_to(HP_MORPH);
        song.rec = 1u << song.sel;
        memset(host_screen, 0, sizeof host_screen);
        ui.force = 1; draw_head(); draw_foot(); ui.force = 0;
        ok &= !head_ink_cols(33, 46);                   /* nothing after the part badges: no number, no asterisk, no REC mark */
        n_self = foot_rec_px();
        song.rec = 1u << ((song.sel + 1u) % NTRK);
        memset(host_screen, 0, sizeof host_screen);
        ui.force = 1; draw_head(); draw_foot(); ui.force = 0;
        ok &= !head_ink_cols(33, 46);
        n_other = foot_rec_px();
        song.rec = 0;
        memset(host_screen, 0, sizeof host_screen);
        ui.force = 1; draw_foot(); ui.force = 0;
        fok &= n_self > 20u && n_other >= 1u && n_other < n_self && foot_rec_px() == 0u;
    }
    bad += check("header: no multi number, no asterisk and no REC mark after the part badges, armed or not, on every page", ok);
    bad += check("footer: a red dot and REC on every page while a part is armed (filled: the selected part, outlined: another), none otherwise", fok);
    ui_power_on();
    return bad;
}

/* FM6's operator pages (EDIT > OPERATOR, OP ENV, OPERATOR 2: ui_fm6op.c, Discussion #76): in an FM6 track's EDIT cycle
 * only; KNOB 1 the operator (shared by the three pages), KNOB 2..4 one byte each of the track's patch: every operator's
 * 14 bytes, each detent exactly that byte, SLOT OWN; the sound follows (a modulator's LEVEL brightens, a carrier's
 * lowers it, COARSE doubles the pitch, a slower attack, FIXED); a gesture is one undo (SAVE held: the patch and SLOT
 * back, held again: redone); projects and user presets keep the edited patch; the macros stay on top of it */
typedef struct { double e, early, hf; uint32_t zc; } fm6_snd_t;
static fm6_snd_t fm6_sound(void)                    /* the selected track's C4 at velocity 100, 128 blocks held */
{
    track_t *t = TSEL;
    fm6_snd_t r = {0, 0, 0, 0};
    int32_t o[2u * CTL], prev = 0;
    uint32_t b, i, n = 0, mq = song.master_q12;
    song.master_q12 = 4096;
    for (b = 0; b < 256u && (b < 1u || t->xf_on || t->eng_req != t->engine); b++)
        mix_block(o, CTL);                           /* (a block first: a pending panic_req, as SAVE held's, done) */
    trk_note_on(t, 60, 100);
    for (b = 0; b < 128u; b++) {
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++, n++) {
            double x = o[2u * i], d = x - prev;
            r.e += x * x;
            r.hf += d * d;
            if (n < 2048u) r.early += x * x;
            if (n && ((prev < 0) != (o[2u * i] < 0))) r.zc++;
            prev = o[2u * i];
        }
    }
    trk_note_off(t, 60);
    for (b = 0; b < 400u; b++)                       /* (the release, the voice ends: the next note from silence) */
        mix_block(o, CTL);
    r.hf = r.e > 0 ? r.hf / r.e : 0;
    song.master_q12 = mq;
    return r;
}
static int fm6_same(const fm6_snd_t *a, const fm6_snd_t *b)   /* the same sound (the reverb's and LFO's state aside) */
{
    double d = a->e > b->e ? a->e - b->e : b->e - a->e;
    if (d > 0.002 * a->e || a->zc + 1u < b->zc || b->zc + 1u < a->zc)
        printf("    not the same: energy %.6g / %.6g, zero crossings %u / %u\n", a->e, b->e, a->zc, b->zc);
    return d <= 0.002 * a->e && a->zc + 1u >= b->zc && b->zc + 1u >= a->zc;
}
static int patch_diff(const uint8_t *a, const uint8_t *b, uint32_t *at)   /* bytes that differ (the last at *at) */
{
    int n = 0;
    uint32_t i;
    for (i = 0; i < FP_SIZE; i++)
        if (a[i] != b[i]) { n++; *at = i; }
    return n;
}
static void fop_pick(uint32_t opn)                   /* KNOB 1: OP opn (1..6) */
{
    turn(EN_K1, -6);
    turn(EN_K1, (int32_t)opn - 1);
}
static int slot_own(void) { return TSEL->p[P_E7] == FM6_OWN && fm6_slot[song.sel] == FM6_OWN; }
static int test_fm6_ops(void)
{
    static const char *const CYC_F[] = {"ENGINE", "EDIT 1", "EDIT 2", "MULTI FILTER", "OP TUNE", "OP ENV", "OP MODE", "VOICE 1/2", "VOICE 2/2", "ENGINE"};
    static const struct { const char *page; uint32_t k, off; } F[] = {
        {"OP TUNE", EN_K2, FP_FC}, {"OP TUNE", EN_K3, FP_FF}, {"OP TUNE", EN_K4, FP_OL},
        {"OP MODE", EN_K2, FP_MODE}, {"OP MODE", EN_K3, FP_DET}, {"OP MODE", EN_K4, FP_KVS},
        {"OP ENV", EN_K3, FP_R1}, {"OP ENV", EN_K4, FP_L1}};
    int bad = 0, ok;
    uint32_t opn, f, s, at, n = 0, tr;
    uint8_t before[FP_SIZE + 1u], v[FP_SIZE + 1u], mine[FP_SIZE + 1u];
    fm6_snd_t s0, s1;
    ui_power_on();
    song.sel = 1;                                    /* T2: FM6 PAD (SLOT F5) */
    tr = song.sel;
    bad += check("FM6 ops: FM6's EDIT cycle EDIT 1 EDIT 2 OPERATOR OP ENV OPERATOR 2 VOICE VOICE 2 EDIT 1",
                 TSEL->eng_req == ENGI_FM6 && engine_cycle(CYC_F, NELEM(CYC_F)));
    ok = TSEL->p[P_E7] == 4 && fm6_slot[tr] == 4u;
    go_title("OP TUNE"); frame();
    fop_pick(3);
    memcpy(before, fm6_patch[tr], FP_SIZE);
    turn(EN_K4, -5);                                 /* OP 3 LEVEL, one gesture of two turns */
    turn(EN_K4, -2);
    ok &= patch_diff(before, fm6_patch[tr], &at) == 1 && at == 3u * FP_OP + FP_OL &&
          fm6_patch[tr][at] == before[at] - 7u && slot_own() && fop_op == 2u;
    bad += check("FM6 ops: OPERATOR KNOB 4 on OP 3: its LEVEL byte only; F5 -> SLOT OWN", ok);
    hold(B_SAVE);
    ok = !memcmp(before, fm6_patch[tr], FP_SIZE) && TSEL->p[P_E7] == 4 && fm6_slot[tr] == 4u && msg_is("UNDO/REDO T2");
    hold(B_SAVE);
    ok &= fm6_patch[tr][3u * FP_OP + FP_OL] == before[3u * FP_OP + FP_OL] - 7u && slot_own();
    frames(1600);
    turn(EN_K3, 4);                                  /* another gesture: its own copy */
    hold(B_SAVE);
    ok &= fm6_patch[tr][3u * FP_OP + FP_OL] == before[3u * FP_OP + FP_OL] - 7u &&
          fm6_patch[tr][3u * FP_OP + FP_FF] == before[3u * FP_OP + FP_FF] && slot_own();
    bad += check("FM6 ops: SAVE held undoes the gesture (patch and SLOT F5 back), again redoes; one undo per gesture", ok);

    ok = 1;                                          /* every operator, every byte the pages edit: that byte only */
    for (opn = 1; opn <= 6u; opn++)
        for (f = 0; f < NELEM(F); f++)
            for (s = 0; s < (F[f].off == FP_R1 || F[f].off == FP_L1 ? 4u : 1u); s++) {
                uint32_t b = (6u - opn) * FP_OP + F[f].off + s, max = fm6_max(b);
                int32_t d;
                go_title(F[f].page); frame();
                fop_pick(opn);
                if (F[f].off == FP_R1 || F[f].off == FP_L1) { turn(EN_K2, -4); turn(EN_K2, (int32_t)s); }
                memcpy(v, fm6_patch[tr], FP_SIZE);
                d = v[b] < max ? 1 : -1;
                frames(1600);
                turn(F[f].k, d);
                if (patch_diff(v, fm6_patch[tr], &at) != 1 || at != b || fm6_patch[tr][b] != (uint8_t)(v[b] + d) || !slot_own()) {
                    printf("ui: FM6 ops: OP %u %s KNOB %u stage %u: byte %u, not %u\n", opn, F[f].page, F[f].k - EN_K1 + 1u,
                           s + 1u, at, b);
                    ok = 0;
                }
                turn(F[f].k, 200);                   /* the range: never past the byte's highest, nor below 0 */
                ok &= fm6_patch[tr][b] == max;
                turn(F[f].k, -200);
                ok &= fm6_patch[tr][b] == 0u;
                n++;
            }
    bad += check("FM6 ops: 6 operators x 14 bytes, each knob writes its byte only (+-1, 0..its highest), SLOT OWN", ok && n == 84u);

    /* the sound: the init voice (ALG 1: OP 1 the carrier at 99, OP 2 its modulator at 0), the track's other sends off */
    TSEL->p[P_CHOR] = TSEL->p[P_DLY] = TSEL->p[P_REV] = TSEL->p[P_DIST] = 0;
    TSEL->p[P_LD_PIT] = TSEL->p[P_LD_FLT] = TSEL->p[P_LD_SHP] = TSEL->p[P_LD_AMP] = 0;
    TSEL->p[P_M1SRC] = TSEL->p[P_M2SRC] = TSEL->p[P_M3SRC] = TSEL->p[P_M4SRC] = 0;
    for (f = 0; f < 7u; f++) TSEL->p[P_E0 + f] = 0;
    fm6_unpack(FM6_INIT, v);
    fm6_set_patch(tr, v);
    fm6_adopt(tr);
    s0 = fm6_sound();
    go_title("OP TUNE"); frame();
    fop_pick(2);
    turn(EN_K4, 80);                                 /* OP 2 LEVEL 0 -> 80: FM, brighter */
    s1 = fm6_sound();
    ok = s1.hf > 2.0 * s0.hf && fm6_patch[tr][4u * FP_OP + FP_OL] == 80u;
    printf("    OP 2 LEVEL 80: HF share %.4f -> %.4f\n", s0.hf, s1.hf);
    turn(EN_K4, -80);
    s1 = fm6_sound();
    ok &= fm6_same(&s0, &s1);
    bad += check("FM6 ops: the sound: a modulator's LEVEL brightens it; back to 0 the same sound again", ok);
    fop_pick(1);
    turn(EN_K4, -20);                                /* OP 1 (the carrier) LEVEL 99 -> 79 */
    s1 = fm6_sound();
    ok = s1.e < 0.2 * s0.e && s1.e > 0;
    printf("    OP 1 LEVEL 79: energy x%.3f", s1.e / s0.e);
    turn(EN_K4, 20);
    turn(EN_K2, 1);                                  /* OP 1 COARSE 1 -> 2: an octave up */
    s1 = fm6_sound();
    ok &= s1.zc > 1.8 * s0.zc && s1.zc < 2.2 * s0.zc;
    printf("; COARSE 2: zero crossings %u -> %u", s0.zc, s1.zc);
    turn(EN_K2, -1);
    go_title("OP ENV"); frame();
    turn(EN_K2, -4);
    turn(EN_K3, -70);                                /* stage 1 (ATK) RATE 99 -> 29: a slow attack */
    s1 = fm6_sound();
    ok &= s1.early < 0.5 * s0.early;
    printf("; ATK RATE 29: the first 2048 samples' energy x%.3f", s1.early / s0.early);
    turn(EN_K3, 70);
    go_title("OP MODE"); frame();
    turn(EN_K2, 1);                                  /* OP 1 MODE FIXED (COARSE 1: 10 Hz x 10 ^ FINE / 100) */
    s1 = fm6_sound();
    ok &= s1.zc < s0.zc / 4u;
    printf("; FIXED: zero crossings %u\n", s1.zc);
    turn(EN_K2, -1);
    s1 = fm6_sound();
    ok &= fm6_same(&s0, &s1);
    bad += check("FM6 ops: a carrier's LEVEL lowers it, COARSE 2 an octave up, ATK RATE 29 slower, FIXED 10 Hz; undone: as before",
                 ok);

    /* the macros on top: MRAT / MLVL move the edited patch as they move any (the patch itself stays) */
    go_title("OP TUNE"); frame();
    fop_pick(2);
    turn(EN_K4, 70);                                 /* OP 2 LEVEL 70, COARSE 1 -> 3 */
    turn(EN_K2, 2);
    memcpy(mine, fm6_patch[tr], FP_SIZE);
    TSEL->p[P_E3] = 2;                               /* MRAT +2 */
    s1 = fm6_sound();
    ok = fm6_eff[tr].p[4u * FP_OP + FP_FC] == 5u && !memcmp(mine, fm6_patch[tr], FP_SIZE);
    turn(EN_K2, 1);                                  /* an edit under MRAT: the macro still adds to it */
    s1 = fm6_sound();
    ok &= fm6_eff[tr].p[4u * FP_OP + FP_FC] == 6u && fm6_patch[tr][4u * FP_OP + FP_FC] == 4u;
    TSEL->p[P_E3] = 0;
    s0 = fm6_sound();
    TSEL->p[P_E2] = -64;                             /* MLVL -64: the modulators down, darker */
    s1 = fm6_sound();
    ok &= s1.hf < 0.5 * s0.hf && fm6_patch[tr][4u * FP_OP + FP_OL] == 70u;
    TSEL->p[P_E2] = 0;
    bad += check("FM6 ops: the macros apply on top of the edited patch (MRAT adds to COARSE, MLVL darker); the patch stays", ok);

    /* kept: a project and a user preset hold the edited patch (SLOT OWN) */
    memcpy(mine, fm6_patch[tr], FP_SIZE);
    stop_transport();
    project_save(2);
    f = (uint32_t)up_store(9, "OPS");
    ok = f == 0u || f == 3u;                         /* (3: the host has no flash, kept in RAM) */
    TSEL->p[P_E7] = 0;                               /* SLOT F1: another patch */
    fm6_poll();
    ok &= memcmp(mine, fm6_patch[tr], FP_SIZE) != 0;
    project_load(2);
    ok &= !memcmp(mine, fm6_patch[tr], FP_SIZE) && slot_own();
    TSEL->p[P_E7] = 1; fm6_poll();
    up_load(9);
    ok &= !memcmp(mine, fm6_patch[tr], FP_SIZE) && slot_own();
    bad += check("FM6 ops: the edited patch round-trips through a project and a user preset (SLOT OWN)", ok);

    /* nothing else: not on other engines, not automatable */
    ok = 1;
    song.sel = 0;                                    /* T1 ANALOG: no operator page */
    for (f = 0; f < NPAGES; f++)
        if (PAGES[f].graph >= GR_FMOP && PAGES[f].graph <= GR_FMENV) ok &= !page_visible(f);
    song.sel = 1;
    go_title("OP TUNE"); frame();
    song.rec = 2u; song.playing = 1;
    f = motion.count;
    turn(EN_K3, 3);
    ok &= motion.count == f;
    stop_transport(); song.rec = 0;
    bad += check("FM6 ops: the pages on FM6 only; a turn while recording writes no automation", ok);
    return bad;
}

/* DUELL 0.2.2: a short press of SAVE opens the page that fits: PATTERNS from the sequencer (SEQ's pages), else by the output MODE
 * (LAYER: MULTIS; SPLIT, STEREO: PATCHES); TOOLS never; hold SAVE on PATTERNS / MULTIS saves over the slot picked */
static int test_save_opens(void)
{
    int bad = 0, ok;
    static const struct { const char *from; uint8_t mode; uint32_t want; } T[] = {
        {"LFO 1", DM_LAYER, GR_SLOTS},   {"ENV 2", DM_SPLIT, GR_BROWSE},     {"STEP", DM_LAYER, GR_PATS},
        {"STEP", DM_SPLIT, GR_PATS},     {"AUTOMATION", DM_STEREO, GR_PATS}, {"MIXER", DM_STEREO, GR_BROWSE},
        {"MORPH", DM_LAYER, GR_SLOTS},   {"REVERB", DM_STEREO, GR_BROWSE},   {"AMP ENV", DM_SPLIT, GR_BROWSE},
        {"MODE", DM_LAYER, GR_SLOTS},
    };
    uint32_t i;
    for (i = 0; i < NELEM(T); i++) {
        ui_power_on();
        duell_mode_req = ui_mode_set = T[i].mode;
        go_title(T[i].from);
        press(B_SAVE);
        ok = !at_home() && cur_page()->fam == FAM_SAVE && cur_page()->graph == T[i].want;
        if (!ok)
            printf("  save_opens %s mode %u -> %s\n", T[i].from, (unsigned)T[i].mode, cur_page()->title);
        bad += check("SAVE opens the page that fits (SEQ: PATTERNS; LAYER: MULTIS; SPLIT, STEREO: PATCHES)", ok);
    }
    ui_power_on();                                      /* OVERVIEW (HOME) counts as any other page; TOOLS is never the first */
    duell_mode_req = ui_mode_set = DM_LAYER;
    go_main_to(HP_MORPH);
    press(B_SAVE);
    bad += check("  from HOME in LAYER: MULTIS", cur_page()->graph == GR_SLOTS);
    duell_mode_req = ui_mode_set = DM_SPLIT;
    press(B_SAVE);
    bad += check("  SAVE again inside the family: the next page (TOOLS after MULTIS), as before", cur_page()->graph == GR_TOOLS);
    press(B_SAVE);
    bad += check("  .. and round to PATCHES (TOOLS is never the first page)", cur_page()->graph == GR_BROWSE);
    turn(EN_SELECT, -1);
    bad += check("  SELECT moves between the four", cur_page()->fam == FAM_SAVE && cur_page()->graph != GR_BROWSE);

    /* hold SAVE on PATTERNS: SAVE OVER <name>? */
    ui_power_on();
    duell_mode_req = ui_mode_set = DM_SPLIT;
    go_title("PATTERNS");
    ui.ppick = 4;
    hold(B_SAVE);
    bad += check("hold SAVE on PATTERNS: the dialog for the slot picked (not the patch's)", ui.confirm == CF_HOLD_PSLOT && ui.confirm_trk == 4u);
    press(B_OCTDN);
    bad += check("  OCT-: nothing saved", !ui.confirm && !ps_used(4));
    hold(B_SAVE);
    press(B_OCTUP);
    bad += check("  OCT+: saved at once, no NAME screen", !ui.confirm && !name_on() && ps_used(4) && msg_is("SAVED P05"));
    /* hold SAVE on MULTIS */
    go_title("MULTIS");
    song.g[G_SLOT] = 12;
    hold(B_SAVE);
    bad += check("hold SAVE on MULTIS, an empty slot: the NAME screen", !ui.confirm && name_on() && !project_used(11));
    name_close();
    project_save_as(11, "KEEPME");
    go_title("MULTIS");
    hold(B_SAVE);
    bad += check("hold SAVE on MULTIS, a used slot: SAVE OVER <name>?", ui.confirm == CF_HOLD_MULTI && ui.confirm_trk == 11u);
    press(B_OCTDN);
    bad += check("  OCT-: nothing changes", !ui.confirm && !name_on());
    hold(B_SAVE);
    press(B_OCTUP);
    {
        char nm[13];
        project_name(11, nm);
        bad += check("  OCT+: saved at once under the same name, no NAME screen", !ui.confirm && !name_on() && msg_is("SAVED M12") && str_eq(nm, "KEEPME"));
    }
    return bad;
}

/* #181: a sound load (PRESETS) or another track (ALGORITHM) draws the page again (ui.force) with no blank frame on the
 * way: MIXER cleared its panel to the background before its four columns landed, a flash on every detent. Every pixel goes
 * from what it showed straight to what it shows next (host_fl_*), on MIXER, HOME (MORPH) and ENV; a full redraw still covers the screen */
static int test_no_flash(void)
{
    static const char *const VIEW[3] = {"MIXER", "HOME", "AMP ENV"};
    static const uint8_t KN[2] = {EN_PRESET, EN_ALGO};
    int bad = 0, ok = 1;
    uint32_t v, k, n, miss;
    for (v = 0; v < 3u; v++)
                for (k = 0; k < 2u; k++) {
                    ui_power_on();
                    if (v == 1u) {
                        go_main_to(HP_MORPH);
                        ui.force = 1;
                    } else {
                        go_title(VIEW[v]);
                    }
                    frames(100);
                    miss = song.sel * 65536u + TSEL->eng_req * 256u + TSEL->preset;
                    host_fl_begin();
                    turn(KN[k], 1);                     /* (a sound loaded / the next track: the page drawn again) */
                    n = host_fl_end(0, 240);
                    ok &= miss != song.sel * 65536u + TSEL->eng_req * 256u + TSEL->preset;
                    if (n)
                        printf("    #181 %s, %s: %u pixels flashed\n", VIEW[v], k ? "ALGORITHM" : "PRESETS", n);
                    ok &= n == 0u && ui.force == 0;
                    for (n = 0; n < 240u * 240u; n++)    /* and the redraw still covers the whole screen */
                        host_screen[n] = 0x5A5A;
                    ui.force = 1;
                    frame();
                    for (n = miss = 0; n < 240u * 240u; n++)
                        miss += host_screen[n] == 0x5A5A;
                    if (miss)
                        printf("    #181 %s: %u pixels not drawn\n", VIEW[v], miss);
                    ok &= miss == 0u;
                }
    ui_prefs = 0;
    bad += check("#181 PRESETS / ALGORITHM on MIXER, HOME, ENV: redrawn, no blank frame", ok);
    return bad;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    int bad = 0;
    bad += test_large_face();
    bad += test_midi();
    bad += test_screen();
    bad += test_mono_screens();
    bad += test_panel();
    bad += test_menu_prefs();
    bad += test_menu_tabs();                        /* (0.2.14: ported, run again: rows, names, pages, fit) */
    bad += test_flat_only();
    bad += test_save_opens();
    bad += test_usb_level();
    bad += test_usb_serial();
    bad += test_edit_cycle();
    bad += test_retired_engines();                  /* (DUELL 0.2.9: SAMPLE, GRAIN, SLICE; 0.2.14: DIGITAL, DRUM) */
    bad += test_idle_glow();
    bad += test_idle_leds();
    bad += test_fm6_charts();
    bad += test_engines_shown();
    bad += test_wave_favorites();
    bad += test_scale_leds();                       /* (0.2.5: ported to the automatic scale display, run again) */
    bad += test_cpu_bar();                          /* (DUELL 0.2.9: the CPU bar under the battery) */
    bad += test_head_rec_foot();                    /* (DUELL 0.2.10: no number / REC mark in the header; the footer's armed mark) */
    bad += test_patch_accel();                      /* (DUELL 0.2.16: PATCHES' K1 with KNOB ACCEL ON) */
#ifdef UI_TEST_STALE
    /* Felucca-era groups whose expectations no longer match DUELL's UI (DRUM lanes, FX map, 4 tracks, old pages, PROJECT, song...):
     * compiled, not run. Port a group to the current UI, then move its call up. Build with -DUI_TEST_STALE to see how they stand. */
    bad += test_sound_loads();
    bad += test_patterns();
    bad += test_rec();
    bad += test_step_rec();
    bad += test_step_preview_chord_entry();
    bad += test_presets_knob();
    bad += test_save();
    bad += test_actions();
    bad += test_tracks();
    
    bad += test_favorites();
    bad += test_categories();
    bad += test_display_preferences();
    bad += test_menu_project_rows();
    bad += test_information();
    bad += test_product_ux();
    bad += test_page_cycles();
    bad += test_roll();
    bad += test_soak_cards();
    bad += test_layer();
    bad += test_layer_knob_leak();
    bad += test_layer_knob_race();
    bad += test_layer_lock();
    bad += test_rec_layer();
    bad += test_seq_tools();
    bad += test_head_centres();
    bad += test_div_order();
    bad += test_knob_accel();
    bad += test_click_menu();
    bad += test_browse_no_pattern();
    bad += test_name();
    bad += test_quick_layers();
    bad += test_chord_page();
    bad += test_bughunt_ui();
    bad += test_bughunt_ui2();
    bad += test_piano_roll();
    bad += test_play_leds();
    bad += test_cursor_step_leds();
    bad += test_midi_leds();
    bad += test_breath();
    bad += test_step_leds();
    bad += test_screen_off();
    bad += test_fm6_ops();
    bad += test_no_flash();
#endif

    printf(bad ? "ui: %d FAILED\n" : "ui: all passed\n", bad);
    return bad ? 1 : 0;
}
#endif
