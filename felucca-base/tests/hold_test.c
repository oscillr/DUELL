/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.9: HOLD (P_AHOLD) has three values, OFF / ARP / ALL. ALL latches the keys (and MIDI IN) even with the arp off:
 * the chord keeps sounding after the keys are up, the next chord replaces it, HOLD off / ARP releases it, STOP ALL clears it.
 * 1 (ARP) is exactly the old ON. */
#include <assert.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

static uint32_t gated(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].gate != 0;
    return n;
}
static int gated_note(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].gate && t->v[i].note == note)
            return 1;
    return 0;
}
static void reset(track_t *t, int arp, int hold)
{
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    kb_prev = 0;
    fm1_in.notes = 0;
    host_preset(t, 0, 0);                          /* ANALOG, POLY */
    t->engine = t->eng_req = 0;
    t->p[P_VOICE] = V_POLY;
    t->p[P_AMODE] = (int16_t)arp;                  /* UP when on */
    t->p[P_AOCT] = 1;
    t->p[P_QUANT] = 0;
    t->p[P_TRANS] = 0;
    t->p[P_AHOLD] = (int16_t)hold;
    events_block(CTL);                             /* (the ISR sees both) */
}
static void keys(uint32_t mask)
{
    fm1_in.notes = mask;
    keyboard_block();
}

int main(void)
{
    track_t *t = &trk[0];
    uint32_t a, b, c, d, i;
    host_tracks_init();

    /* the parameter: three words, 0 / 1 as before */
    assert(TP[P_AHOLD].max == 2 && TP[P_AHOLD].def == 0);
    assert(!strcmp(TP[P_AHOLD].names[0], "OFF") && !strcmp(TP[P_AHOLD].names[1], "ARP") && !strcmp(TP[P_AHOLD].names[2], "ALL"));

    /* ALL, arp off: a chord latches and sounds after the keys are up */
    reset(t, 0, 2);
    a = kb_map(t, 0); b = kb_map(t, 4); c = kb_map(t, 7); d = kb_map(t, 9);
    keys((1u << 0) | (1u << 4) | (1u << 7));
    assert(gated(t) == 3 && gated_note(t, a) && gated_note(t, b) && gated_note(t, c));
    keys(0);
    for (i = 0; i < 4; i++)
        events_block(CTL);
    assert(gated(t) == 3 && gated_note(t, a) && gated_note(t, b) && gated_note(t, c));   /* held, not retriggered, not released */
    assert(t->nheld == 3 && t->arp_phys == 0);
    /* the next chord (first key down with none held) replaces it */
    keys(1u << 9);
    assert(gated(t) == 1 && gated_note(t, d) && !gated_note(t, a));
    keys((1u << 9) | (1u << 0));                   /* a second key while one is down adds to the chord */
    assert(gated(t) == 2 && gated_note(t, d) && gated_note(t, a));
    keys(0);
    assert(gated(t) == 2 && t->nheld == 2);
    /* MIDI IN is the same path */
    input_on(t, 72, 100);
    assert(gated(t) == 1 && gated_note(t, 72));
    input_off(t, 72);
    assert(gated(t) == 1 && gated_note(t, 72));
    /* HOLD OFF releases (normal release: the gate goes) */
    t->p[P_AHOLD] = 0;
    events_block(CTL);
    assert(gated(t) == 0 && t->nheld == 0);
    /* ... and so does ARP with the arp off */
    t->p[P_AHOLD] = 2;
    events_block(CTL);
    keys(1u << 0);
    keys(0);
    assert(gated(t) == 1);
    t->p[P_AHOLD] = 1;
    events_block(CTL);
    assert(gated(t) == 0 && t->nheld == 0);

    /* ALL turned on with a key already down: its release still ends it (nothing sticks) */
    reset(t, 0, 0);
    keys(1u << 0);
    t->p[P_AHOLD] = 2;
    events_block(CTL);
    keys(0);
    assert(gated(t) == 0);

    /* arp ON + ALL: the chord is held as with ARP, turning the arp off keeps it as sounding voices, on again hands it back */
    reset(t, 1, 2);
    keys((1u << 0) | (1u << 4));
    keys(0);
    assert(t->nheld == 2 && t->arp_phys == 0);
    arp_tick(t, div_samples((uint32_t)t->p[P_ARATE]));
    assert(t->arp_note == a && gated(t) == 1);
    keys(1u << 7);                                 /* a new chord replaces it */
    assert(t->nheld == 1 && t->held[0] == c);
    t->p[P_AMODE] = 0;                             /* arp off: ALL must not drop the latch */
    events_block(CTL);
    assert(t->nheld == 1 && gated(t) == 1 && gated_note(t, c));
    t->p[P_AMODE] = 1;                             /* arp on: the arp plays it, the plain voice ends */
    events_block(CTL);
    assert(t->nheld == 1);
    arp_tick(t, div_samples((uint32_t)t->p[P_ARATE]));
    assert(gated(t) == 1 && t->arp_note == c);

    /* ARP (1) with the arp off: no latch, as before; with the arp on: the old latch, dropped when the arp goes off */
    reset(t, 0, 1);
    keys(1u << 0);
    assert(gated(t) == 1);
    keys(0);
    assert(gated(t) == 0 && t->nheld == 0);
    reset(t, 1, 1);
    keys((1u << 0) | (1u << 4));
    keys(0);
    assert(t->nheld == 2);
    t->p[P_AMODE] = 0;
    events_block(CTL);
    assert(t->nheld == 0 && gated(t) == 0);

    /* two parts: one drones, the other is played (DUEL) */
    reset(t, 0, 2);
    trk[1].p[P_AHOLD] = 0;
    input_on(t, 60, 100);
    input_off(t, 60);
    input_on(&trk[1], 64, 100);
    input_off(&trk[1], 64);
    assert(gated(t) == 1 && gated(&trk[1]) == 0);

    /* STOP ALL clears the latch and the voices */
    reset(t, 0, 2);
    keys((1u << 0) | (1u << 4));
    keys(0);
    assert(gated(t) == 2);
    transport_req = 4;
    events_block(CTL);
    assert(t->nheld == 0 && t->arp_phys == 0);
    for (i = 0; i < 8; i++)
        events_block(CTL);
    assert(gated(t) == 0);
    keys(1u << 7);                                 /* and the next chord starts clean */
    assert(gated(t) == 1);

    puts("hold: OFF / ARP / ALL: latch with the arp off, replace, release, arp on + ALL, two parts, STOP ALL, old ARP as before ok");
    return 0;
}
