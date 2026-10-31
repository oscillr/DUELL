/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* DUELL 1.1: MIDI OUT of the ARP and the SEQUENCER (seq.c mo_on / mo_off / mo_flush), a loopback test: the packets the audio code puts on the MIDI
 * output ring (midi_out_q, as usb.c sends them) are read back and compared with the notes that sound. The step sequencer and the arp (CLASSIC and SEQ mode)
 * send note-on with the velocity played and a note-off, on the part's MIDI channel; a played key with the ARP on sends nothing itself (no double note);
 * a stop, a panic and a channel change leave no note hanging; LAYER sends part 1 only. Built and run by host_tests.sh (name midiout). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int ev_n;
static uint32_t ev[256];                                /* packets read back */
static void drain(void)
{
    while (mo_r != mo_w && ev_n < 256) ev[ev_n++] = midi_out_q[mo_r % MQ], mo_r++;
    mo_r = mo_w;
}
static void clr(void) { mo_w = mo_r = 0; ev_n = 0; }
#define ST(i) (((ev[i] >> 8) & 0xF0u))
#define CHN(i) (((ev[i] >> 8) & 0x0Fu))
#define NOTE(i) ((ev[i] >> 16) & 0x7Fu)
#define VEL(i) ((ev[i] >> 24) & 0x7Fu)
static int count(uint32_t st, uint32_t note)
{
    int i, n = 0;
    for (i = 0; i < ev_n; i++) n += ST(i) == st && NOTE(i) == note;
    return n;
}
/* every note-on is balanced by a note-off, and no note-on comes twice in a row without its off */
static int balanced(void)
{
    int on[128] = {0}, i, ok = 1;
    for (i = 0; i < ev_n; i++) {
        uint32_t n = NOTE(i);
        if (ST(i) == 0x90u) { if (on[n]++) ok = 0; }
        else if (ST(i) == 0x80u) { if (!on[n]--) ok = 0; }
    }
    for (i = 0; i < 128; i++) if (on[i]) ok = 0;
    return ok;
}
static void reset(void)
{
    ui_power_on();
    memset(kb_chn, 0, sizeof kb_chn); memset(mchord, 0, sizeof mchord);
    fm1_in.notes = kb_prev = 0; usb.config = 1; clr();
    memset(mo_bits, 0, sizeof mo_bits); kb_mo = 0;
    trk[0].p[P_VOICE] = V_POLY; scale_g = 0; root_g = 0;
    midi_pch[0] = 0; midi_pch[1] = 1;
    events_block(CTL);
    clr();
}

int main(void)
{
    int bad = 0;
    track_t *t;
    uint32_t i, per;

    /* 1. the step sequencer: C4 100, E4 accent, G4 rest in between; velocity as played, channel 1 */
    reset();
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = 3; t->p[P_SGATE] = 64; t->p[P_SSWING] = 0; song.g[G_SWING] = 0;
    t->step[0] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    t->step[1] = (step_t){{64}, 1, ST_NOTE, SF_ACCENT, 100};
    t->step[2] = (step_t){{67}, 1, ST_NOTE, 0, 100};
    seq_start();
    for (i = 0; i < 4000 && ev_n < 6; i++) { seq_tick(t, CTL); drain(); }
    bad += check("SEQ: first note-on C4 channel 1 velocity as played (100)", ev_n >= 2 && ST(0) == 0x90u && NOTE(0) == 60u && CHN(0) == 0u && VEL(0) == 100u);
    bad += check("SEQ: its note-off follows (same note, same channel)", ev_n >= 2 && ST(1) == 0x80u && NOTE(1) == 60u && CHN(1) == 0u);
    bad += check("SEQ: the accented E4 goes out with velocity 127", count(0x90u, 64) >= 1 && ev_n >= 4 && VEL(2) == 127u);
    {
        int voices = 0, k; for (k = 0; k < NVOICE; k++) voices += t->v[k].active;
        bad += check("SEQ: one note-on per step (no doubles): C4 once before E4", ev_n >= 3 && count(0x90u, 60) == 1 && NOTE(2) == 64u && voices > 0);
    }
    /* stop: nothing hangs */
    seq_stop(); drain();
    bad += check("SEQ: STOP leaves every note ended (balanced stream)", balanced());

    /* 2. part 2 on channel 6 (its own MIDI channel) */
    reset();
    midi_pch[1] = 5;
    t = &trk[1];
    track_defaults_steps(t);
    t->p[P_SLEN] = 1; t->p[P_SGATE] = 64; t->step[0] = (step_t){{62}, 1, ST_NOTE, 0, 90};
    seq_start();
    for (i = 0; i < 400; i++) { seq_tick(t, CTL); drain(); if (ev_n >= 2) break; }
    bad += check("SEQ part 2: channel 6, note D4, velocity as played", ev_n >= 1 && ST(0) == 0x90u && CHN(0) == 5u && NOTE(0) == 62u);
    seq_stop(); drain();
    bad += check("  STOP: balanced", balanced());

    /* 3. the arp, classic UP: held C E G, velocity 100; stop by the keys up; a key sends no note itself */
    reset();
    t = &trk[0];
    t->p[P_AMODE] = AM_UP; t->p[P_AOCT] = 1; t->p[P_APROB] = 127; t->p[P_AGATE] = 64; t->p[P_ARATE] = 2;
    per = div_samples(2);
    t->nheld = 3; t->held[0] = 60; t->held[1] = 64; t->held[2] = 67; memset(held_key[0], 0, sizeof held_key[0]);
    t->arp_pos = 0xFFFFFFF; t->arp_idx = 0xFFFFFFFFu;
    for (i = 0; i < 6; i++) { arp_tick(t, per); arp_tick(t, per / 2u); drain(); }
    bad += check("ARP: six steps, six note-ons C E G C E G in order", count(0x90u, 60) == 2 && count(0x90u, 64) == 2 && count(0x90u, 67) == 2 && NOTE(0) == 60u && NOTE(2) == 64u && NOTE(4) == 67u);
    bad += check("ARP: velocity 100 on channel 1, a note-off before the next note-on", VEL(0) == 100u && CHN(0) == 0u && ST(1) == 0x80u && NOTE(1) == 60u);
    t->nheld = 0; t->p[P_AMODE] = 0; arp_tick(t, per); drain();
    arp_forget(t); trk_all_off(t); drain();
    bad += check("ARP: switched off / forgotten, nothing hangs (balanced)", balanced());

    /* 4. a key with the arp on: its own note-on is not sent; only the arp's notes are */
    reset();
    t = &trk[0];
    t->p[P_AMODE] = AM_UP; t->p[P_AOCT] = 1; t->p[P_APROB] = 127; t->p[P_AGATE] = 64; t->p[P_ARATE] = 2;
    fm1_in.notes = 1u << 7; keyboard_block(); events_block(CTL); drain();
    bad += check("ARP on, key C4 pressed: one note-on only (the arp's, none from the key itself)", count(0x90u, 60) == 1);
    for (i = 0; i < 4; i++) { events_block(CTL); arp_tick(t, per); drain(); }
    bad += check("  the arp's own note-on for the held key does go out", count(0x90u, 60) >= 1);
    fm1_in.notes = 0; keyboard_block(); events_block(CTL); arp_off(t); drain();
    bad += check("  key up: balanced, nothing hangs", balanced());

    /* 5. a key with the arp OFF still sends its note (as before) */
    reset();
    fm1_in.notes = 1u << 7; keyboard_block(); events_block(CTL); drain();
    bad += check("ARP off: the key's note-on still goes out (as before)", count(0x90u, 60) == 1);
    fm1_in.notes = 0; keyboard_block(); events_block(CTL); drain();

    /* 6. a channel change while an arp note sounds: the note-off goes to the channel the note-on used; panic ends the rest */
    reset();
    t = &trk[0];
    t->p[P_AMODE] = AM_UP; t->p[P_AOCT] = 1; t->p[P_APROB] = 127; t->p[P_AGATE] = 127; t->p[P_ARATE] = 2;
    t->nheld = 1; t->held[0] = 60; memset(held_key[0], 0, sizeof held_key[0]);
    t->arp_pos = 0xFFFFFFF; t->arp_idx = 0xFFFFFFFFu;
    arp_tick(t, per); drain();
    midi_pch[0] = 9;
    arp_off(t); drain();
    bad += check("channel changed while sounding: the note-off uses the note-on's channel (1)", ev_n == 2 && ST(1) == 0x80u && CHN(1) == 0u && CHN(0) == 0u);
    clr();
    arp_tick(t, per); drain();
    bad += check("  the next note-on goes to the new channel (10)", ev_n >= 1 && ST(0) == 0x90u && CHN(0) == 9u);
    clr(); arp_forget(t); trk_all_off(t); drain();
    bad += check("  a panic (arp_forget) ends the sounding note on channel 10", ev_n == 1 && ST(0) == 0x80u && CHN(0) == 9u && NOTE(0) == 60u);

    /* 7. LAYER: part 1 sends, part 2 does not */
    reset();
    duell_mode_force(DM_LAYER);
    trk[1].p[P_AMODE] = AM_UP; trk[1].p[P_AOCT] = 1; trk[1].p[P_APROB] = 127; trk[1].p[P_AGATE] = 64; trk[1].p[P_ARATE] = 2;
    trk[1].nheld = 1; trk[1].held[0] = 65; memset(held_key[1], 0, sizeof held_key[1]);
    trk[1].arp_pos = 0xFFFFFFF; trk[1].arp_idx = 0xFFFFFFFFu;
    arp_tick(&trk[1], per); drain();
    bad += check("LAYER: part 2's arp sends nothing (part 1's channel counts)", ev_n == 0);
    trk[0].p[P_AMODE] = AM_UP; trk[0].p[P_AOCT] = 1; trk[0].p[P_APROB] = 127; trk[0].p[P_AGATE] = 64; trk[0].p[P_ARATE] = 2;
    trk[0].nheld = 1; trk[0].held[0] = 60; memset(held_key[0], 0, sizeof held_key[0]);
    trk[0].arp_pos = 0xFFFFFFF; trk[0].arp_idx = 0xFFFFFFFFu;
    arp_tick(&trk[0], per); drain();
    bad += check("LAYER: part 1's arp sends on channel 1", ev_n >= 1 && ST(0) == 0x90u && CHN(0) == 0u && NOTE(0) == 60u);
    duell_mode_force(DM_SPLIT);

    {   /* the width of the LAYER note on GLO > MIDI (ui_graph.c graph_midi, AF_S at x = 12, the screen is 240 wide) */
        int32_t w = text_w(&AF_S, "LAYER MODE: PART 1 CHANNEL COUNTS");
        printf("  LAYER note width %d px (x 12 .. %d of 240)\n", (int)w, (int)(12 + w));
        bad += check("LAYER note fits inside the 240 px screen (the longest other line on that page is 203 px)", 12 + w <= 240);
    }
    printf("%s\n", bad ? "MIDI OUT ARP/SEQ TEST FAILED" : "midi out arp/seq test passed");
    return bad != 0;
}
