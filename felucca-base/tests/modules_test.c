/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.14 GLO > MODULES (SPEC "Modules"; core.h MODS_*, ui.c mods_*, seq.c arp_mode / seq_play, fx.c snd_* / pfx_*), against simulated NOR
 * flash and the real sound:
 *   SETTINGS   a fresh unit, ERASE DATA and an older record read all four ON; each switch kept across a save and a power-on; a multi load
 *              never changes them; the page: K1 ARP K2 SEQ K3 SENDS K4 PART FX, right ON / left OFF, OCT- + a knob = ON (RESET), MIDI LEARN
 *              cannot pick them; the editor (MENU_DESC / MENU_SET ids 28..31) is editor_test's
 *   ARP OFF    a key sounds at once and no arp ever steps (the stored MODE kept); chord keys build their chord; HOLD ARP latches nothing,
 *              HOLD ALL latches the keys; REC as HOLD toggles OFF <-> ALL; ARP back ON: the arp plays again; a latched arp chord ends
 *   SEQ OFF    PLAY (internal clock), an external Start with pulses and the count-in play no stored step (M03's pattern: notes with SEQ ON,
 *              none with SEQ OFF), the automation does not play; the arp's SEQUENCER mode plays the key itself (M01's part 2);
 *              STEP PREVIEW still sounds a step
 *   SENDS OFF  after the ~22 ms fade the mix is the dry mix bit for bit (STEREO with HALL, PING-PONG and the delay's modulation; DUEL),
 *              no step at the switch; ON again: the return is back (its energy as a run never switched), the stored values untouched
 *   PART FX    after the fade each part is its dry block bit for bit (DIST, FLANGER insert, EQ, a PUMP DUCK, SPREAD's side too); ON again:
 *              the effects back from rest
 *   PERFORM    its stages (FLANGER, DRIVE, MUTE 2) still act with all four OFF
 *   UI         the cards each module hides (not drawn, the knob does nothing), the header on PLAY
 * Built and run by DUELL/host_tests.sh (name: modules). */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
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
    printf("modules: %-118s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
    return ok;
}
static void tails_over(void)
{
    uint32_t k, i;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NVOICE; i++)
            trk[k].v[i].active = 0;
}
static void boot(void)                                  /* main.c's start: the RAM gone, the flash read, the settings */
{
    ui_power_on();
    tails_over();
    memset(&settings, 0, sizeof settings);
    first_boot_pending = 0;
    memset(&persist_saved, 0, sizeof persist_saved);
    persist_pending = 0;
    flash_ok = 1;
    persist_boot();
    settings_init();
    multi_boot();
    frame();
}
static void flush(void)                                 /* the settings written (settings_poll, nothing sounding) */
{
    uint32_t n;
    tails_over();
    for (n = 0; n < 8u; n++) {
        fm1_ms += 2000u;
        settings_poll();
    }
}
static int stored_mods(void)                            /* the bits as the flash holds them */
{
    static persist_t p;
    return st_load(OBJ_SETTINGS, &p, sizeof p) > 0 ? (int)((p.favorites.factory[15][10] >> 1) & 15u) : -1;
}
static int card_is(uint32_t c, const char *label, const char *val)   /* the card's cache key: "LABEL|VALUE|UNIT..." */
{
    char want[40];
    snprintf(want, sizeof want, "%s|%s|", label, val);
    return !strncmp(ui.col[c], want, strlen(want));
}
static int card_hid(uint32_t c)                         /* the card is not drawn (ui_draw.c draw_column_hide: the cache key is the one mark \x7f) */
{
    return ui.col[c][0] == 0x7f;
}
static void show(const char *title) { go_title(title); ui.force = 1; frame(); }
static void set(uint32_t bit, uint32_t off) { mods_put(bit, off); }

/* ---- the sound ---- */
#define ENG_AN 0u                                       /* ANALOG (engines.c ENGINES[0]) */
static int32_t ob[2 * CTL];
static uint32_t ms_acc, t_blk;                          /* (t_blk: the blocks since quiet_start) */
static void rblocks(uint32_t n)
{
    while (n--) {
        t_blk++;
        mix_block(ob, CTL);
        ms_acc += CTL * 1000u;
        while (ms_acc >= FS) {
            ms_acc -= FS;
            fm1_ms++;
        }
    }
}
#define MS(x) ((uint32_t)((x) * (FS / 1000u) / CTL))
static void until(uint32_t ms) { while (t_blk < MS(ms)) rblocks(1); }   /* to ms after quiet_start (every run on the same block grid) */
static void sound(track_t *t, uint32_t e, uint32_t k)
{
    host_ds_load(t, e, k);
    t->p[P_SUS] = 127;
    t->p[P_REL] = 10;
    t->p[P_AMODE] = 0;
    t->p[P_AHOLD] = 0;
    t->p[P_CHRD] = 0;
    t->p[P_VOICE] = V_POLY;
    t->p[P_TRIM] = 90;
}
static void quiet_start(void)
{
    uint32_t k;
    ui_power_on();
    tails_over();
    for (k = 0; k < NTRK; k++)
        midi_forget_track(k);
    fm1_in.notes = kb_prev = 0;
    mi_r = mi_w = 0;
    memset(&midi_clock, 0, sizeof midi_clock);
    ms_acc = t_blk = 0;
    song.master_q12 = MASTER_USB_REF;                   /* (MASTER at its default) */
    snd_st = SND_ON, snd_x = 0, snd_clr = 0;           /* (the audio side as at power-on) */
    memset(pfx_x, 0, sizeof pfx_x);
}
static uint32_t n_gated(const track_t *t)                 /* voices held (gate on) */
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active && t->v[i].gate;
    return n;
}
static int gated_note(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note == note)
            return 1;
    return 0;
}

/* ---- SETTINGS ---- */
static void test_settings(void)
{
    static persist_t p;
    memset(nor, 0xFF, sizeof nor);
    boot();
    expect("a fresh unit: all four ON (the settings byte's bits 1..4 clear; the audio side too)", mods_get() == 0u && mods_off == 0u && !(ui_idle & 0x1Eu));
    show("MODULES");
    expect("GLO > MODULES: K1 ARP, K2 SEQ, K3 SENDS, K4 PART FX, each ON", card_is(0, "ARP", "ON") && card_is(1, "SEQ", "ON") &&
           card_is(2, "SENDS", "ON") && card_is(3, "PART FX", "ON"));
    expect("  .. a global page (both badges), the footer's first row empty", page_global(cur_page()));
    turn(EN_K1, -1);
    expect("K1 left: ARP OFF, the header says what it does, the card OFF (dim)", mods_get() == MODS_ARP && mods_off == MODS_ARP &&
           msg_is("ARP OFF: KEYS PLAY") && card_is(0, "ARP", "OFF"));
    turn(EN_K1, -1);
    expect("  .. left again: stays OFF", mods_get() == MODS_ARP);
    turn(EN_K2, -1); turn(EN_K3, -1);
    expect("K3 left: SENDS OFF, SAVES CPU said", mods_get() == (MODS_ARP | MODS_SEQ | MODS_SENDS) && msg_is("SENDS OFF: SAVES CPU"));
    turn(EN_K4, -2);
    expect("K4 left: PART FX OFF, SAVES CPU said; the byte's bits 1..4 set, bit 0 (ANIM IDLE) untouched", mods_get() == MODS_ALL &&
           msg_is("PART FX OFF: SAVES CPU") && (ui_idle & 0x1Fu) == 0x1Eu);
    frames(2200);
    expect("  .. saved about 2 s later (the settings record holds them)", stored_mods() == (int)MODS_ALL);
    boot();
    expect("power-on: all four still OFF", mods_get() == MODS_ALL && mods_off == MODS_ALL);
    project_load(0);
    frame();
    expect("a multi load (M01) never changes them", mods_get() == MODS_ALL);
    show("MODULES");
    turn(EN_K3, 1);
    expect("K3 right: SENDS ON, said", !(mods_get() & MODS_SENDS) && msg_is("SENDS ON"));
    frames(2200);
    boot();
    expect("power-on: ARP, SEQ, PART FX OFF, SENDS ON, as saved", mods_get() == (MODS_ARP | MODS_SEQ | MODS_PFX));
    show("MODULES");
    btn_down(B_OCTDN); frame();
    turn(EN_K4, -3);
    expect("OCT- + K4: PART FX to its default, ON, \"RESET PART FX\"", !(mods_get() & MODS_PFX) && msg_is("RESET PART FX"));
    btn_up(B_OCTDN); frame(); frame();
    expect("  .. no octave moved", song.octave == 0);
    ml_toggle();
    ui.msg_t = 0;
    turn(EN_K2, 1);
    expect("MIDI LEARN on: K2 still switches SEQ ON, but nothing is picked (\"SEQ: NO LEARN\")", !(mods_get() & MODS_SEQ) && !ui.ml.pick &&
           msg_is("SEQ: NO LEARN"));
    ml_off();
    ui.msg_t = 0;
    set(MODS_ALL, 1u);
    settings_save();
    flush();
    expect("  .. all OFF, saved", stored_mods() == (int)MODS_ALL);
    expect("ERASE DATA: erased", erase_data_run() == 0);
    erase_at = 0;
    boot();
    expect("  .. the restart reads all four ON", mods_get() == 0u && mods_off == 0u);

    /* an older record (0.2.13 or Felucca: the 0.2.14 flag [15][24] bit 7 clear) with junk in [15][10] bits 1..7 and ANIM IDLE on */
    set(MODS_ALL, 1u);
    settings_save();
    flush();
    expect(" (a record with all OFF saved)", st_load(OBJ_SETTINGS, &p, sizeof p) > 0 && ((p.favorites.factory[15][10] >> 1) & 15u) == 15u);
    p.favorites.factory[15][10] = 0xFFu;
    p.favorites.factory[15][24] &= 0x7Fu;
    st_save(OBJ_SETTINGS, &p, sizeof p);
    boot();
    expect("an older record (no 0.2.14 flag), its bits 1..7 set: read all four ON once, ANIM IDLE kept", mods_get() == 0u && (ui_idle & 1u) &&
           !(ui_idle & 0xFEu));
}

/* ---- ARP OFF ---- */
static uint32_t key_note(uint32_t k) { return kb_map(&trk[song.sel], k); }
static void key(uint32_t k, int down) { if (down) fm1_in.notes |= 1u << k; else fm1_in.notes &= ~(1u << k); rblocks(1); }
static void arp_part(uint32_t mode, uint32_t hold)
{
    quiet_start();
    duell_mode_force(DM_STEREO);
    sound(&trk[0], ENG_AN, 0);
    trk[0].p[P_AMODE] = (int16_t)mode;
    trk[0].p[P_AHOLD] = (int16_t)hold;
    trk[0].p[P_ARATE] = 2;                              /* (1/16) */
    song.sel = 0;
    rblocks(MS(50));
}
static void test_arp(void)
{
    uint32_t k = key_at(0, 5), n, v0, idx;
    arp_part(1u, 0u);                                   /* ARP ON, MODE UP: the control */
    v0 = vage;
    key(k, 1);
    rblocks(MS(1000));
    n = vage - v0;
    key(k, 0);
    expect("ARP ON (MODE UP, 1/16): a key held 1 s, the arp steps (control)", n >= 6u);

    arp_part(1u, 0u);
    set(MODS_ARP, 1u);
    v0 = vage;
    idx = trk[0].arp_idx;
    key(k, 1);
    expect("ARP OFF: a key down sounds its note at once (the first block)", gated_note(&trk[0], key_note(k)) && vage - v0 == 1u);
    rblocks(MS(1000));
    expect("  .. held 1 s: the one voice, the arp never steps (arp_idx untouched), the stored MODE still UP",
           vage - v0 == 1u && trk[0].arp_idx == idx && n_gated(&trk[0]) == 1u && trk[0].p[P_AMODE] == 1);
    key(k, 0);
    rblocks(MS(20));
    expect("  .. let go: the note ends (gate off)", !n_gated(&trk[0]));
    trk[0].p[P_CHRD] = CH_MAJ;
    v0 = vage;
    key(k, 1);
    rblocks(MS(300));
    expect("  .. chord keys (MAJ): one key, its three notes at once, nothing more", vage - v0 == 3u && n_gated(&trk[0]) == 3u);
    key(k, 0);
    rblocks(MS(20));
    trk[0].p[P_CHRD] = 0;

    trk[0].p[P_AHOLD] = 1;                              /* HOLD ARP: "the arp keeps its chord": no arp, nothing to keep */
    rblocks(2);
    key(k, 1); rblocks(MS(100)); key(k, 0); rblocks(MS(50));
    expect("  .. HOLD ARP with ARP OFF: no latch (the note ends when let go)", !n_gated(&trk[0]) && trk[0].p[P_AHOLD] == 1);
    trk[0].p[P_AHOLD] = 2;                              /* HOLD ALL: the keys latch */
    rblocks(2);
    key(k, 1); rblocks(MS(100)); key(k, 0); rblocks(MS(300));
    expect("  .. HOLD ALL with ARP OFF: the chord latched, as a drone (no arp step)", gated_note(&trk[0], key_note(k)) && n_gated(&trk[0]) == 1u);
    v0 = vage;
    set(MODS_ARP, 0u);
    rblocks(MS(1000));
    expect("ARP ON again (HOLD ALL latched): the arp plays the latched chord", vage - v0 >= 6u);
    set(MODS_ARP, 1u);
    rblocks(MS(200));
    expect("ARP OFF again: the latched chord is held by the voices (HOLD ALL), no arp step", n_gated(&trk[0]) == 1u);
    trk[0].p[P_AHOLD] = 0;
    rblocks(MS(100));
    expect("  .. HOLD OFF: it ends", !n_gated(&trk[0]));

    arp_part(1u, 1u);                                   /* HOLD ARP, ARP ON: a latched arp chord, then ARP OFF */
    key(k, 1); rblocks(MS(100)); key(k, 0); rblocks(MS(300));
    v0 = vage;
    rblocks(MS(300));
    expect("ARP ON, HOLD ARP: the chord latched, the arp steps", vage - v0 >= 2u);
    set(MODS_ARP, 1u);
    rblocks(MS(300));
    v0 = vage;
    rblocks(MS(500));
    expect("  .. ARP OFF: the arp's chord dropped, nothing sounds or steps on", !n_gated(&trk[0]) && vage == v0);

    /* REC as HOLD (on HOME: not a sequencer page): with ARP OFF the keys latch, OFF <-> ALL */
    arp_part(1u, 0u);
    set(MODS_ARP, 1u);
    go_main_to(HP_MORPH);
    frame();
    press(B_REC);
    expect("REC (HOLD) with ARP OFF on an arp part: HOLD ALL (the keys latch), HOLD ON said", trk[0].p[P_AHOLD] == 2 && msg_is("HOLD ON"));
    frames(600);
    press(B_REC);
    expect("  .. again: HOLD OFF", trk[0].p[P_AHOLD] == 0 && msg_is("HOLD OFF"));
    trk[0].p[P_AHOLD] = 1;
    frames(600);
    press(B_REC);
    expect("  .. a stored HOLD ARP counts as off (it latches nothing): REC sets ALL", trk[0].p[P_AHOLD] == 2);
    set(MODS_ARP, 0u);
    trk[0].p[P_AHOLD] = 0;
    frames(600);
    press(B_REC);
    expect("  .. ARP ON again: REC sets HOLD ARP as ever", trk[0].p[P_AHOLD] == 1);
}

/* ---- SEQ OFF ---- */
static void multi_start(uint32_t slot)
{
    quiet_start();
    memset(nor, 0xFF, sizeof nor);
    flash_ok = 1;
    multi_boot();
    frame();
    project_load(slot);
    frame();
    rblocks(MS(100));
    tails_over();
}
static uint32_t play_count(uint32_t ms)                 /* PLAY from stop, ms on: the voices started */
{
    uint32_t v0 = vage;
    transport_req = 1;
    rblocks(MS(ms));
    return vage - v0;
}
static uint32_t ext_count(uint32_t pulses)              /* a MIDI Start on USB, then pulses at 120 BPM: the voices started */
{
    uint32_t v0 = vage, i;
    midi_enqueue(0x0Fu | 0xFAu << 8, 1);
    for (i = 0; i < pulses; i++) {
        midi_enqueue(0x0Fu | 0xF8u << 8, 1);
        rblocks(29);                                     /* (~21 ms, the pulse's spacing at 120 BPM) */
    }
    return vage - v0;
}
static void test_seq(void)
{
    uint32_t n, i, m3 = 2u;                             /* M03: part 2's 16 stored steps (arp RND: the steps play from PLAY) */
    uint16_t at;
    multi_start(m3);
    for (i = n = 0; i < 16u; i++)
        n += step_on(&trk[1].step[i]);
    expect("M03 holds a stored pattern on part 2 (16 steps with notes), its arp not SEQUENCER", n == 16u && trk[1].p[P_AMODE] != AM_SEQ);
    n = play_count(2000);
    expect("SEQ ON: PLAY plays them (control)", n >= 8u && song.playing);
    multi_start(m3);
    set(MODS_SEQ, 1u);
    n = play_count(2000);
    expect("SEQ OFF: PLAY starts the transport, no stored step plays", n == 0u && song.playing);
    transport_req = 2;
    rblocks(4);
    multi_start(m3);
    n = ext_count(96);
    expect("SEQ ON: an external MIDI Start + 96 pulses plays the steps (control)", n >= 8u && song.playing);
    multi_start(m3);
    set(MODS_SEQ, 1u);
    n = ext_count(96);
    expect("SEQ OFF: an external MIDI Start + 96 pulses: the transport runs, no step plays", n == 0u && song.playing);
    multi_start(m3);
    set(MODS_SEQ, 1u);
    cin_bars = 1;
    song.rec = 2;
    n = play_count(3500);
    expect("SEQ OFF: PLAY with part 2 armed and COUNT-IN 1 BAR: counts in, then no step plays", n == 0u && song.playing && !cin_left);
    cin_bars = 0;
    song.rec = 0;
    multi_start(m3);
    set(MODS_SEQ, 1u);
    trk[1].p[P_REV] = 23;
    motion_clear(&trk[1]);
    motion_set_event(&trk[1], 0, P_REV, 100);
    motion_set_enabled(&trk[1], 1);
    play_count(600);
    expect("SEQ OFF: the automation does not play either (REV stays the sound's 23)", trk[1].p[P_REV] == 23);
    transport_req = 2;
    rblocks(4);
    set(MODS_SEQ, 0u);
    play_count(600);
    expect("  .. SEQ ON: it plays (REV 100 from step 1)", trk[1].p[P_REV] == 100);
    transport_req = 2;
    rblocks(4);
    multi_start(m3);
    set(MODS_SEQ, 1u);
    at = 0;
    while (at < 16u && !step_on(&trk[1].step[at]))
        at++;
    n = vage;
    aud_req = (uint16_t)(at << 8 | 2u);
    rblocks(2);
    expect("SEQ OFF: STEP PREVIEW still sounds a step (stopped)", vage - n >= 1u);

    /* M01: part 2's arp is SEQUENCER (its steps are the arp's): SEQ OFF switches it off too, the key plays itself */
    multi_start(0);
    expect("M01 part 2: the arp's SEQUENCER mode", trk[1].p[P_AMODE] == AM_SEQ);
    song.sel = 1;
    set(MODS_SEQ, 1u);
    n = vage;
    key(key_at(0, 7), 1);
    rblocks(MS(800));
    expect("  .. SEQ OFF: a key on part 2 sounds its own note at once, no step (one voice in 0.8 s)",
           gated_note(&trk[1], key_note(key_at(0, 7))) && vage - n == 1u);
    key(key_at(0, 7), 0);
    rblocks(MS(50));
    set(MODS_SEQ, 0u);
    n = vage;
    key(key_at(0, 7), 1);
    rblocks(MS(800));
    expect("  .. SEQ ON: the SEQUENCER arp plays its steps on the key again", vage - n >= 2u);
    key(key_at(0, 7), 0);
    rblocks(MS(50));
    song.sel = 0;
}

/* ---- SENDS OFF, PART FX OFF: the mix before the master, bit for bit ---- */
#define CAP_MS 500u
static int32_t cap[3][2][MS(CAP_MS) * CTL];
static void pre_master(int32_t *l, int32_t *r)          /* this block's mix before MASTER (fx.c: mix_l / mix_r and wet) */
{
    uint32_t i;
    for (i = 0; i < CTL; i++) {
        l[i] = mix_l[i] + wet[i];
        r[i] = mix_r[i] + wet[i];
    }
}
static void capture(uint32_t k, uint32_t nb)
{
    uint32_t b;
    for (b = 0; b < nb; b++) {
        rblocks(1);
        pre_master(&cap[k][0][b * CTL], &cap[k][1][b * CTL]);
    }
}
static int same(uint32_t a, uint32_t b, uint32_t n)
{
    if (getenv("MOD_DBG")) { uint32_t i; for (i = 0; i < n && cap[a][0][i] == cap[b][0][i] && cap[a][1][i] == cap[b][1][i]; i++) ; if (i < n) printf("  differ at %u: %d %d vs %d %d\n", i, cap[a][0][i], cap[a][1][i], cap[b][0][i], cap[b][1][i]); }
    return !memcmp(cap[a][0], cap[b][0], n * sizeof(int32_t)) && !memcmp(cap[a][1], cap[b][1], n * sizeof(int32_t));
}
static double energy_diff(uint32_t a, uint32_t b, uint32_t n)
{
    double e = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double x = (double)cap[a][0][i] - cap[b][0][i], y = (double)cap[a][1][i] - cap[b][1][i];
        e += x * x + y * y;
    }
    return e;
}
static int32_t max_step(uint32_t a, uint32_t from, uint32_t to)   /* the largest sample-to-sample move, either channel */
{
    int32_t m = 0;
    uint32_t i, c;
    for (c = 0; c < 2u; c++)
        for (i = from + 1u; i < to; i++) {
            int32_t d = cap[a][c][i] - cap[a][c][i - 1u];
            d = d < 0 ? -d : d;
            m = d > m ? d : m;
        }
    return m;
}
/* two parts holding chords, STEREO with HALL, PING-PONG and the delay's modulation (every side mix), or DUEL; sends 0 / 1 */
static void send_scene(uint32_t mode, uint32_t sends)
{
    uint32_t i;
    quiet_start();
    duell_mode_force(mode);
    sound(&trk[0], ENG_AN, 0);
    sound(&trk[1], ENG_AN, 3);
    trk[1].p[P_SPRD] = 60;
    song.g[G_RTYPE] = 2;
    song.g[G_DMODE] = DMD_PING;
    song.g[G_DMDEP] = 60;
    song.g[G_DMRATE] = 40;
    for (i = 0; i < 2u; i++) {
        trk[i].p[P_CHOR] = (int16_t)(sends ? 60 : 0);
        trk[i].p[P_DLY] = (int16_t)(sends ? 60 : 0);
        trk[i].p[P_REV] = (int16_t)(sends ? 80 : 0);
    }
    for (i = 0; i < 3u; i++) {
        trk_note_on(&trk[0], 48u + 4u * i, 100);
        trk_note_on(&trk[1], 60u + 3u * i, 100);
    }
}
static void test_sends(uint32_t mode)
{
    uint32_t n = MS(CAP_MS) * CTL, sw = MS(300) * CTL;
    const char *mn = mode == DM_SPLIT ? "DUEL" : "STEREO";
    char w[200];
    int32_t before, at;
    /* A: sends on, SENDS OFF at 1.0 s (the 300 ms before it captured: the music's own steps), the switch's 100 ms, then 1.1 .. 1.6 s */
    send_scene(mode, 1u);
    until(700);
    capture(2, MS(300));
    before = max_step(2, 0, sw);
    set(MODS_SENDS, 1u);
    capture(2, MS(100));                                /* (the fade, 22.5 ms; the lines cleared, 9 blocks) */
    at = max_step(2, 0, MS(100) * CTL);
    until(1100);
    capture(0, MS(CAP_MS));
    expect("SENDS OFF: faded out, the buses skipped (OFF)", snd_st == SND_OFF);
    /* B: the same with every send at 0, SENDS ON: the dry mix (the buses fed silence give silence) */
    send_scene(mode, 0u);
    until(1100);
    capture(1, MS(CAP_MS));
    snprintf(w, sizeof w, "SENDS OFF (%s, HALL, PING-PONG, delay modulation, SPREAD): 100 ms on, the mix is the dry mix bit for bit (500 ms)", mn);
    expect(w, same(0, 1, n));
    snprintf(w, sizeof w, "  .. no step at the switch: the largest sample move %d within the music's own %d (+25 %%)", (int)at, (int)before);
    expect(w, at <= before + before / 4 + 64);
}
static void test_sends_on_again(void)
{
    uint32_t n = MS(CAP_MS) * CTL;
    double ea, ec;
    char w[220];
    send_scene(DM_STEREO, 1u);                          /* A: OFF at 0.5 s, ON at 1.0 s, captured 3.0 .. 3.5 s */
    until(500);
    set(MODS_SENDS, 1u);
    until(1000);
    set(MODS_SENDS, 0u);
    until(3000);
    capture(0, MS(CAP_MS));
    expect("SENDS ON again: the buses run again (the state ON after its fade in)", snd_st == SND_ON && snd_x == 0);
    expect("  .. the stored sends and the FX pages' values untouched", trk[0].p[P_CHOR] == 60 && trk[0].p[P_DLY] == 60 && trk[0].p[P_REV] == 80 &&
           song.g[G_RTYPE] == 2 && song.g[G_DMODE] == DMD_PING && song.g[G_DMDEP] == 60);
    send_scene(DM_STEREO, 0u);                          /* B: the dry mix, the same times */
    until(3000);
    capture(1, MS(CAP_MS));
    send_scene(DM_STEREO, 1u);                          /* C: never switched */
    until(3000);
    capture(2, MS(CAP_MS));
    ea = energy_diff(0, 1, n);
    ec = energy_diff(2, 1, n);
    snprintf(w, sizeof w, "  .. the return is back: its energy %.3g, a run never switched %.3g (within 3 dB; not bit for bit: the lines restart clean)", ea, ec);
    expect(w, ec > 0 && ea > ec / 2.0 && ea < ec * 2.0);
}
/* PART FX: DIST, a SOFT insert, EQ and a PUMP DUCK on part 1, DIST and EQ on part 2 (SPREAD: mix_spread's side); no sends (the buses would
 * keep what the effects sent before). The insert has no LFO of its own, so ON again can be compared with a run never switched */
static void pfx_scene(uint32_t fx)
{
    uint32_t i;
    quiet_start();
    duell_mode_force(DM_STEREO);
    sound(&trk[0], ENG_AN, 0);
    sound(&trk[1], ENG_AN, 3);
    trk[1].p[P_SPRD] = 60;
    for (i = 0; i < 2u; i++)
        trk[i].p[P_CHOR] = trk[i].p[P_DLY] = trk[i].p[P_REV] = 0;
    if (fx) {
        trk[0].p[P_DIST] = 40;
        trk[0].p[P_ITYPE] = 1;
        trk[0].p[P_IMIX] = 100;
        trk[0].p[P_EQLG] = 6; trk[0].p[P_EQMG] = -5; trk[0].p[P_EQHG] = 4;
        trk[0].p[P_DKSRC] = 0; trk[0].p[P_DKDEP] = 80; trk[0].p[P_DKRATE] = 2;
        trk[1].p[P_EQLG] = -4; trk[1].p[P_EQHG] = 6;
        trk[1].p[P_DIST] = 20;
    }
    transport_req = 1;
    for (i = 0; i < 3u; i++) {
        trk_note_on(&trk[0], 48u + 4u * i, 100);
        trk_note_on(&trk[1], 60u + 3u * i, 100);
    }
}
static void test_pfx(void)
{
    uint32_t n = MS(CAP_MS) * CTL;
    double e_on, e_ref;
    int32_t before, at;
    char w[220];
    pfx_scene(1u);
    until(700);
    capture(2, MS(300));
    before = max_step(2, 0, MS(300) * CTL);
    set(MODS_PFX, 1u);
    capture(2, MS(100));
    at = max_step(2, 0, MS(100) * CTL);
    until(1100);
    capture(0, MS(CAP_MS));
    expect("  (PART FX OFF: the stored DIST, INSERT, EQ, DUCK DEPTH still as set)", trk[0].p[P_DIST] == 40 && trk[0].p[P_ITYPE] == 1 &&
           trk[0].p[P_EQLG] == 6 && trk[0].p[P_DKDEP] == 80);
    pfx_scene(0u);
    until(1100);
    capture(1, MS(CAP_MS));
    expect("PART FX OFF (DIST, SOFT insert, EQ, PUMP DUCK; SPREAD's side): 100 ms on, each part is its dry block bit for bit (500 ms)",
           same(0, 1, n));
    snprintf(w, sizeof w, "  .. no step at the switch: the largest sample move %d within the music's own %d (+25 %%)", (int)at, (int)before);
    expect(w, at <= before + before / 4 + 64);
    /* ON again: the effects from rest, faded in; 1 s later the output against a run never switched */
    pfx_scene(1u);
    until(500);
    set(MODS_PFX, 1u);
    until(1000);
    set(MODS_PFX, 0u);
    until(2000);
    capture(0, MS(CAP_MS));
    pfx_scene(1u);
    until(2000);
    capture(2, MS(CAP_MS));
    pfx_scene(0u);
    until(2000);
    capture(1, MS(CAP_MS));
    e_on = energy_diff(0, 2, n);
    e_ref = energy_diff(2, 1, n);
    snprintf(w, sizeof w, "PART FX ON again: 1 s on, the effects back (vs a run never switched: %.3g; the effects' own share %.3g)", e_on, e_ref);
    expect(w, e_ref > 0 && e_on < e_ref / 1000.0);
}
static void test_perform(void)
{
    double e0 = 0, e1 = 0, r0 = 0, r1 = 0, d = 0;
    uint32_t i, b;
    static int32_t ref[MS(200)][2 * CTL];
    send_scene(DM_SPLIT, 1u);                           /* the reference: all four OFF, no PERFORM */
    set(MODS_ALL, 1u);
    until(400);
    for (b = 0; b < MS(200); b++) {
        rblocks(1);
        memcpy(ref[b], ob, sizeof ob);
    }
    send_scene(DM_SPLIT, 1u);
    set(MODS_ALL, 1u);
    until(350);
    perf_press(PF_FLG, 1);
    perf_press(PF_DRV, 1);
    perf_press(PF_M2, 1);
    until(400);
    for (b = 0; b < MS(200); b++) {
        rblocks(1);
        for (i = 0; i < CTL; i++) {
            double x = (double)ob[2u * i] - ref[b][2u * i];
            d += x * x;
            e0 += (double)ref[b][2u * i] * ref[b][2u * i];
            r0 += (double)ref[b][2u * i + 1u] * ref[b][2u * i + 1u];
            e1 += (double)ob[2u * i] * ob[2u * i];
            r1 += (double)ob[2u * i + 1u] * ob[2u * i + 1u];
        }
    }
    perf_press(PF_FLG, 0);
    perf_press(PF_DRV, 0);
    perf_press(PF_M2, 0);
    if (getenv("MOD_DBG")) printf("  e0 %g e1 %g r0 %g r1 %g d %g\n", e0, e1, r0, r1, d);
    expect("PERFORM with all four OFF (DUEL): FLANGER + DRIVE change part 1's side, MUTE 2 silences part 2's", e0 > 0 && d > e0 / 100.0 && r0 > 0 &&
           r1 < r0 / 1000.0);
}

/* ---- UI: the cards each module hides ---- */
static void test_ui(void)
{
    int16_t v;
    quiet_start();
    frame();
    set(MODS_SENDS, 1u);
    trk[0].p[P_REV] = 50;
    show("FX");
    expect("SENDS OFF, the FX page: DST live, CHO DLY REV not drawn (no card, no footer line)",
           !card_hid(0) && card_hid(1) && card_hid(2) && card_hid(3));
    show("CHORUS");
    expect("  .. CHORUS: both cards not drawn", card_hid(0) && card_hid(1));
    show("DELAY 1/2");
    expect("  .. DELAY 1/2: all four not drawn", card_hid(0) && card_hid(1) && card_hid(2) && card_hid(3));
    show("DELAY 2/2");
    expect("  .. DELAY 2/2: all four not drawn", card_hid(0) && card_hid(1) && card_hid(2) && card_hid(3));
    show("REVERB");
    expect("  .. REVERB: TYPE SIZE DAMP not drawn", card_hid(0) && card_hid(1) && card_hid(2));
    show("FX");
    ui.msg_t = 0;
    turn(EN_K4, 3);
    expect("  .. REV turned: the knob does nothing (the value kept at 50, no message)", trk[0].p[P_REV] == 50 && !ui.msg_t);
    go_page(GR_TRK);
    ui.force = 1; frame();
    expect("  .. the MIXER's SPACE card not drawn", card_hid(2) && !card_hid(0));
    ui.msg_t = 0;
    turn(EN_K3, 2);
    expect("  .. SPACE turned there: nothing moves, nothing said", perf_space == 0 && !ui.msg_t);
    show("AMP ENV");
    expect("  .. other pages: every card drawn", !card_hid(0) && !card_hid(3));
    set(MODS_PFX, 1u);
    show("FX");
    expect("PART FX OFF too: DST not drawn", card_hid(0));
    set(MODS_SENDS, 0u);
    show("INSERT 1/2");
    expect("PART FX OFF: INSERT 1/2 not drawn", card_hid(0) && card_hid(3));
    show("INSERT 2/2");
    expect("  .. INSERT 2/2 (MIX)", card_hid(0));
    show("EQ 1/2");
    expect("  .. EQ 1/2", card_hid(0) && card_hid(3));
    show("EQ 2/2");
    expect("  .. EQ 2/2", card_hid(0) && card_hid(2));
    show("DUCK 1/2");
    expect("  .. DUCK 1/2", card_hid(0) && card_hid(1) && card_hid(2));
    show("DUCK 2/2");
    expect("  .. DUCK 2/2", card_hid(0) && card_hid(2));
    show("SIDECHAIN");
    expect("  .. GLO > SIDECHAIN (the DUCK's SRC and NOTE), not drawn", card_hid(0) && card_hid(1));
    show("FX");
    expect("  .. the FX page: CHO DLY REV live again (SENDS ON)", !card_hid(1) && !card_hid(3));
    set(MODS_PFX, 0u);
    show("FX");
    expect("all ON: every card drawn", !card_hid(0) && !card_hid(1));

    trk[0].p[P_AMODE] = 1;
    trk[0].p[P_AHOLD] = 0;
    set(MODS_ARP, 1u);
    show("ARP 1/3");
    expect("ARP OFF: ARP 1/3's MODE RATE GATE not drawn, HOLD OFF drawn",
           card_hid(0) && !card_hid(1) && card_hid(2) && card_hid(3));
    trk[0].p[P_AHOLD] = 1;
    show("ARP 1/3");
    expect("  .. HOLD ARP not drawn (no chord to keep)", card_hid(1));
    trk[0].p[P_AHOLD] = 2;
    show("ARP 1/3");
    expect("  .. HOLD ALL drawn (it latches the keys)", !card_hid(1));
    show("ARP 2/3");
    expect("  .. ARP 2/3 all not drawn", card_hid(0) && card_hid(1) && card_hid(2) && card_hid(3));
    show("ARP 3/3");
    expect("  .. ARP 3/3 (TARGET) not drawn", card_hid(0));
    ui.msg_t = 0;
    show("ARP 1/3");
    v = trk[0].p[P_AMODE];
    turn(EN_K1, 1);
    expect("  .. MODE turned: the knob does nothing, nothing said", trk[0].p[P_AMODE] == v && !ui.msg_t);
    trk[0].p[P_AMODE] = 1;
    go_main_to(HP_MORPH); frame();
    btn_down(B_ARP);
    frames(800);
    expect("  .. QUICK ARP's four not drawn too", ui.layer == LAYER_ARP && card_hid(0) && card_hid(3));
    btn_up(B_ARP); frame(); frames(300);
    set(MODS_ARP, 0u);
    show("ARP 1/3");
    expect("ARP ON: the ARP pages live", !card_hid(0) && !card_hid(2));

    set(MODS_SEQ, 1u);
    show("ARP 1/3");
    expect("SEQ OFF, an arp in UP: the ARP pages live", !card_hid(0));
    trk[0].p[P_AMODE] = AM_SEQ;
    show("ARP 1/3");
    expect("  .. an arp in SEQUENCER: its cards not drawn, no step strip",
           card_hid(0) && card_hid(2) && !foot_steps());
    trk[0].p[P_AMODE] = 0;
    go_page(GR_EVENTS); ui.force = 1; frame();
    expect("  .. AUTOMATION: PLAY not drawn (the step strip stays)", card_hid(3) && foot_steps());
    go_page(GR_ROLL); ui.force = 1; frame();
    expect("  .. STEP: its editing cards live", !card_hid(0) && !card_hid(1));
    go_main_to(HP_MORPH); frame();
    ui.msg_t = 0;
    press(B_PLAY);
    frames(48);
    expect("  .. PLAY: the transport starts, the header says \"MODULES: SEQ OFF\"", (transport_req == 1u || song.playing) && msg_is("MODULES: SEQ OFF"));
    transport_req = 0;
    song.playing = 0;
    set(MODS_SEQ, 0u);

    set(MODS_SENDS, 1u);
    perf_enter();
    ui.force = 1; frame();
    expect("PERFORM with SENDS OFF: K2 SPACE not drawn ", card_hid(1) && !card_hid(0) && !card_hid(2));
    set(MODS_SENDS, 0u);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    test_settings();
    test_arp();
    test_seq();
    test_sends(DM_STEREO);
    test_sends(DM_SPLIT);
    test_sends_on_again();
    test_pfx();
    test_perform();
    test_ui();
    printf("modules: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
