/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* Tests of KEY SENSE, after Velo's method (Charles Vestal, github.com/charlesvestal/fm1-velocity, GPL-3.0-only; see firmware/src/sense.c). */
/* The hooks of KEY SENSE in the firmware (seq.c key_on / keyboard_block / velo_at_block, DUELL_VELO) with the real sound code, on the
 * host: velocity from the keys into the voices, chords, LAYER, MIDI out, the arp, the fixed velocity of the top 7 keys, the note's
 * wait for its velocity, aftertouch as channel pressure; and, built twice (host_tests.sh velo: -DDUELL_VELO=0 the firmware as it was,
 * -DDUELL_VELO=1 with the setting OFF), the same long scenes with every output sample hashed: the hash lists must be equal.
 * The setting ON half runs only in the DUELL_VELO=1 build. */
#define MODE_TEST_NO_MAIN 1
#ifndef DUELL_VELO
#define DUELL_VELO 1
#endif
#if DUELL_VELO
#include "../firmware/src/sense.c"
/* (velo_cal_load / velo_cal_save: project.c's store with its RAM stand-in on the host; no stub of our own) */
#endif
#include "mode_test.c"

static int vh_fails;
static int vh_check(const char *what, int ok)
{
    printf("velo: %-100s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        vh_fails++;
    return ok;
}
static uint64_t vh_fnv(const void *p, uint32_t bytes, uint64_t h)
{
    const uint8_t *b = p;
    for (uint32_t i = 0; i < bytes; i++) {
        h ^= b[i];
        h *= 1099511628211ull;
    }
    return h;
}
static uint32_t vh_xs = 0x2545F491u;
static uint32_t vh_rnd(uint32_t n)
{
    vh_xs ^= vh_xs << 13;
    vh_xs ^= vh_xs >> 17;
    vh_xs ^= vh_xs << 5;
    return (vh_xs >> 8) % n;
}
static uint64_t vh_h;
static void vh_blk(uint32_t n)
{
    while (n--) {
        mix_block(ob, CTL);
        vh_h = vh_fnv(ob, sizeof ob, vh_h);
        while (mo_r != mo_w) {                          /* the MIDI out note-ons and note-offs the keys made, packet by packet */
            vh_h = vh_fnv(&midi_out_q[mo_r % MQ], sizeof midi_out_q[0], vh_h);
            mo_r++;
        }
    }
}
static void vh_setup(uint32_t mode)
{
    uint32_t k;
    dsp_clean(mode);
    vh_h = 1469598103934665603ull;
    song.g[G_BPM] = 120;
    song.g[G_CLOCK] = 0;
    for (k = 0; k < 2u; k++) {
        host_preset(&trk[k], eng_of("ANALOG"), 0);
        trk[k].p[P_TRIM] = 100;
        trk[k].p[P_PAN] = k ? 40 : -40;
        trk[k].p[P_SUS] = 127;
        trk[k].p[P_REL] = 60;
        trk[k].p[P_AMODE] = 0;
        trk[k].p[P_VOICE] = V_POLY;
        trk[k].p[P_CHOR] = trk[k].p[P_DLY] = trk[k].p[P_REV] = 0;
    }
    kb_boot_hold = 0;
    usb.config = 1;                                      /* (MIDI out is open: its packets are part of what is compared) */
    mo_r = mo_w = 0;
}
/* a long scene of keys: random presses and releases on all 27 keys, a block a step, every output sample hashed */
static void vh_scene(uint32_t mode, uint32_t arp, uint32_t chord, uint32_t xfade, const char *name)
{
    uint32_t i, mask = 0;
    vh_setup(mode);
    vh_xs = 0x2545F491u + mode * 7u + arp * 3u + chord * 5u;
    trk[0].p[P_AMODE] = (int16_t)arp;
    trk[0].p[P_CHRD] = (int16_t)chord;
    mix_xf = (int32_t)xfade;
    for (i = 0; i < 700u; i++) {
        if (vh_rnd(5) == 0) {
            uint32_t k = vh_rnd(27);
            mask ^= 1u << k;
            if (__builtin_popcount(mask) > 6)
                mask &= mask - 1u;
        }
        fm1_in.notes = mask;
        vh_blk(1);
    }
    fm1_in.notes = 0;
    vh_blk(60);
    printf("HASH %s %016llx\n", name, (unsigned long long)vh_h);
}

#if DUELL_VELO
static uint32_t T;
static void feed(uint32_t k, uint32_t adc, uint32_t drv, uint32_t dt)
{
    T += dt;
    sense_update(k, adc, drv, T);
}
static void prime(uint32_t k)                           /* a settled range 400 (hard) .. 580 (soft) */
{
    uint32_t i;
    sense_ev_t e;
    for (i = 0; i < 14u; i++) {
        feed(k, 1023, 0, 2200);
        feed(k, 200, 0, 2200);
        feed(k, 400u + (i * 53u) % 181u, 1, 2200);
        feed(k, 1023, 0, 2200);
        feed(k, 1023, 0, 2200);
    }
    while (sense_event_pop(&e))
        ;
}
static uint32_t vh_gated(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].gate != 0;
    return n;
}
static int all_mvel(const track_t *t, uint32_t vel, uint32_t n)
{
    uint32_t i, c = 0;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].gate) {
            if (t->v[i].mvel != vel)
                return 0;
            c++;
        }
    return c == n;
}
/* the real keyboard path: key k closed in the digital scan; the sense readings of its landing arrive in two steps */
static void keys_now(uint32_t mask)
{
    fm1_in.notes = mask;
    keyboard_block();
}
static void sense_on(void)
{
    sense_enable(0);
    sense_at_enable(0);
    sense_reset();
    sense_enable(1);
    sense_at_enable(1);                                   /* (DUELL 1.0: AFTERTOUCH is its own line; these scenes use both) */
    sense_curve_set(100);
    T = 1000000u;
}
static uint32_t pkt_vel(uint32_t i) { return midi_out_q[i % MQ] >> 24; }

static void hooks(void)
{
    uint32_t k = 4, v_hard, v_soft, i;
    printf("-- setting ON: the hooks\n");
    vh_setup(DM_STEREO);
    sense_on();
    prime(k);
    prime(7);
    prime(9);
    prime(11);
    /* a key closed in the digital scan, its velocity not in: the note waits; then it starts with the measured velocity */
    feed(k, 1023, 0, 2200);
    feed(k, 200, 0, 2200);
    keys_now(1u << k);
    vh_check("a closed key whose strength is not in yet starts nothing", vh_gated(&trk[0]) == 0u);
    feed(k, 372, 1, 2200);
    keys_now(1u << k);
    v_hard = trk[0].m_vel;
    vh_check("  .. the next block after the reading, the note starts with the measured velocity (hard: > 110)", vh_gated(&trk[0]) == 1u && v_hard > 110u && all_mvel(&trk[0], v_hard, 1));
    keys_now(0);
    feed(k, 1023, 0, 2200);
    feed(k, 1023, 0, 2200);
    vh_blk(40);
    feed(k, 1023, 0, 2200);
    feed(k, 200, 0, 2200);
    feed(k, 610, 1, 2200);
    keys_now(1u << k);
    v_soft = trk[0].m_vel;
    vh_check("a soft landing plays softer (< 30) in the same block the reading is there", vh_gated(&trk[0]) >= 1u && v_soft < 30u);
    keys_now(0);

    /* the top 7 keys: fixed 100, no wait */
    vh_blk(40);
    keys_now(1u << 22);
    vh_check("a top key (note 22) starts at once with velocity 100", vh_gated(&trk[0]) >= 1u && trk[0].m_vel == 100u);
    keys_now(0);
    vh_blk(40);

    /* a chord: every note of the key has the key's velocity; MIDI out carries it */
    trk[0].p[P_CHRD] = 3;
    mo_r = mo_w = 0;
    feed(7, 1023, 0, 2200);
    feed(7, 200, 0, 2200);
    feed(7, 390, 1, 2200);
    keys_now(1u << 7);
    {
        uint32_t n = vh_gated(&trk[0]), vel = trk[0].m_vel, ok = n >= 2u && all_mvel(&trk[0], vel, n), pk = 1;
        for (i = mo_r; i != mo_w; i++)
            pk &= (midi_out_q[i % MQ] & 0xFu) == 9u && pkt_vel(i) == vel;
        if (getenv("VELO_DEBUG")) {
            printf("chord: %u notes gated, m_vel %u:", n, vel);
            for (i = 0; i < NVOICE; i++)
                if (trk[0].v[i].gate)
                    printf(" [note %u mvel %u]", trk[0].v[i].note, trk[0].v[i].mvel);
            printf("\n");
        }
        vh_check("a chord key: every chord note has the key's velocity", ok && vel > 90u);
        vh_check("  .. and so does every MIDI out note-on", mo_w - mo_r == n && pk);
    }
    keys_now(0);
    vh_blk(60);
    trk[0].p[P_CHRD] = 0;

    /* LAYER: both parts get it */
    vh_setup(DM_LAYER);
    sense_on();
    prime(9);
    mix_xf = 50;
    feed(9, 1023, 0, 2200);
    feed(9, 200, 0, 2200);
    feed(9, 385, 1, 2200);
    keys_now(1u << 9);
    vh_check("LAYER: both parts start the note with the key's velocity", vh_gated(&trk[0]) == 1u && vh_gated(&trk[1]) == 1u && trk[0].m_vel > 100u &&
             all_mvel(&trk[0], trk[0].m_vel, 1) && all_mvel(&trk[1], trk[0].m_vel, 1));
    keys_now(0);
    vh_blk(60);

    /* the arp: its notes stay at 100, whatever the key's velocity */
    vh_setup(DM_STEREO);
    sense_on();
    prime(11);
    trk[0].p[P_AMODE] = 1;
    feed(11, 1023, 0, 2200);
    feed(11, 200, 0, 2200);
    feed(11, 600, 1, 2200);
    keys_now(1u << 11);
    vh_blk(40);
    vh_check("the arp plays its notes at 100 (the key said soft)", vh_gated(&trk[0]) >= 1u && trk[0].m_vel == 100u);
    keys_now(0);
    trk[0].p[P_AMODE] = 0;
    vh_blk(60);

    /* a key let go while it waits for its velocity: it plays one block, then ends */
    vh_setup(DM_STEREO);
    sense_on();
    prime(k);
    feed(k, 1023, 0, 2200);
    feed(k, 200, 0, 2200);
    keys_now(1u << k);
    keys_now(0);
    vh_check("a tap shorter than the wait still sounds once: the note starts in that block ..", vh_gated(&trk[0]) == 1u);
    keys_now(0);
    vh_check("  .. and its release follows in the next", vh_gated(&trk[0]) == 0u || trk[0].v[0].gate == 0u);

    /* the note waits at most SC_GATE_MAX_US when the analog never sees the key */
    vh_setup(DM_STEREO);
    sense_on();
    keys_now(1u << 13);
    vh_check("no landing seen: the note waits first", vh_gated(&trk[0]) == 0u);
    for (i = 0; i < 8u; i++) {
        feed(13, 1023, 0, 2200);
        keys_now(1u << 13);
    }
    vh_check("  .. and starts with the fixed velocity after SC_GATE_MAX_US", vh_gated(&trk[0]) == 1u && trk[0].m_vel == SC_VEL_UNKNOWN);
    keys_now(0);
    vh_blk(40);

    /* aftertouch: channel pressure to the part the keys play */
    vh_setup(DM_STEREO);
    sense_on();
    prime(k);
    feed(k, 1023, 0, 2200);
    feed(k, 200, 0, 2200);
    feed(k, 500, 1, 2200);
    keys_now(1u << k);
    for (i = 0; i < 12u; i++) {
        feed(k, 220, 0, 2200);
        feed(k, 500, 1, 2200);
    }
    keys_now(1u << k);
    vh_check("a light hold: part aftertouch about zero", trk[0].at <= 8u);
    for (i = 0; i < 30u; i++) {
        feed(k, 220, 0, 2200);
        feed(k, 500u - (uint32_t)(70.0 * (i + 1) / 30.0), 1, 2200);
    }
    keys_now(1u << k);
    vh_check("a squeeze: the part's aftertouch (t->at, the matrix source) goes up (> 100), the other part's does not",
             trk[0].at > 100u && trk[1].at == 0u);
    feed(k, 1023, 0, 2200);
    feed(k, 1023, 0, 2200);
    keys_now(0);
    keys_now(0);
    vh_check("  .. and back to 0 when the key is let go", trk[0].at == 0u);
    trk[0].at = 33;                                      /* MIDI IN's pressure: left alone while no analog key is held */
    keys_now(0);
    vh_check("  MIDI IN's channel pressure is not overwritten while no key presses", trk[0].at == 33u);
    sense_enable(0);
    sense_at_enable(0);
    keys_now(0);
    vh_check("setting OFF: no pressure is sent either", trk[0].at == 33u);

    vh_setup(DM_LAYER);
    sense_on();
    prime(k);
    feed(k, 1023, 0, 2200);
    feed(k, 200, 0, 2200);
    feed(k, 500, 1, 2200);
    keys_now(1u << k);
    for (i = 0; i < 30u; i++) {
        feed(k, 220, 0, 2200);
        feed(k, 500u - (uint32_t)(70.0 * (i + 1) / 30.0), 1, 2200);
    }
    keys_now(1u << k);
    vh_check("LAYER: both parts get the pressure", trk[0].at > 100u && trk[1].at > 100u);
    keys_now(0);
    sense_enable(0);
    sense_at_enable(0);
}

/* MENU > CONTROL > KEY VELOCITY (menu_items.c MI_KEYVEL): OFF / ON on sense_enable, default OFF (the bit clear; set = ON since 0.2.31), saved in bit 6 of the byte [15][10] */
static void menu_line(void)
{
    uint8_t keep = ui_idle;
    vh_check("KEY VELOCITY: the row is on CONTROL, named, two values OFF / ON", MI_TAB[MI_KEYVEL] == MTAB_CONTROL && str_eq(MI_NAME[MI_KEYVEL], "KEY VELOCITY") && menu_n(MI_KEYVEL) == 2u &&
             str_eq(menu_vname(MI_KEYVEL, 0), "OFF") && str_eq(menu_vname(MI_KEYVEL, 1), "ON"));
    ui_idle &= (uint8_t)~KEYVEL_ON_BIT;
    sense_enable(0);
    vh_check("KEY VELOCITY: a clear bit (a new unit, an older record) = OFF, the default", menu_get(MI_KEYVEL) == 0u && sense_enabled() == 0u);
    menu_put(MI_KEYVEL, 1);
    vh_check("KEY VELOCITY ON: sense_enable(1), the bit set", menu_get(MI_KEYVEL) == 1u && sense_enabled() == 1u && (ui_idle & KEYVEL_ON_BIT));
    vh_check("KEY VELOCITY: a step up turns it ON, a step down OFF", menu_step(MI_KEYVEL, 1) == 1u && menu_step(MI_KEYVEL, -1) == 0u);
    vh_check("KEY VELOCITY ON: the other bits of the byte are kept", (ui_idle & (uint8_t)~KEYVEL_ON_BIT) == (keep & (uint8_t)~KEYVEL_ON_BIT));
    menu_put(MI_KEYVEL, 0);
    vh_check("KEY VELOCITY OFF: sense_enable(0), the bit clear", menu_get(MI_KEYVEL) == 0u && sense_enabled() == 0u && !(ui_idle & KEYVEL_ON_BIT));
    ui_idle = keep;
    sense_enable(0);
    /* DUELL 1.0 (owner): AFTERTOUCH on its own line, the same switch, bit 7 of the same byte (set = OFF), default ON since 0.2.32 */
    vh_check("AFTERTOUCH: the row is on CONTROL, named in whole words, two values OFF / ON", MI_TAB[MI_AFTERT] == MTAB_CONTROL && str_eq(MI_NAME[MI_AFTERT], "AFTERTOUCH") && menu_n(MI_AFTERT) == 2u &&
             str_eq(menu_vname(MI_AFTERT, 0), "OFF") && str_eq(menu_vname(MI_AFTERT, 1), "ON"));
    ui_idle &= (uint8_t)~AFTERT_OFF_BIT;
    sense_at_enable(menu_get(MI_AFTERT));
    vh_check("AFTERTOUCH: a clear bit (a new unit, an older record) = ON, the default (0.2.32); KEY VELOCITY stays OFF", menu_get(MI_AFTERT) == 1u && sense_at_enabled() == 1u && menu_get(MI_KEYVEL) == 0u);
    menu_put(MI_AFTERT, 0);
    vh_check("AFTERTOUCH OFF: the bit set", menu_get(MI_AFTERT) == 0u && sense_at_enabled() == 0u && (ui_idle & AFTERT_OFF_BIT));
    menu_put(MI_AFTERT, 1);
    vh_check("AFTERTOUCH ON: sense_at_enable(1), the bit set, KEY VELOCITY untouched", menu_get(MI_AFTERT) == 1u && sense_at_enabled() == 1u && !(ui_idle & AFTERT_OFF_BIT) && menu_get(MI_KEYVEL) == 0u && sense_enabled() == 0u);
    vh_check("AFTERTOUCH: a step up turns it ON, a step down OFF", menu_step(MI_AFTERT, 1) == 1u && menu_step(MI_AFTERT, -1) == 0u);
    vh_check("AFTERTOUCH ON: the other bits of the byte are kept", (ui_idle & (uint8_t)~AFTERT_OFF_BIT) == (keep & (uint8_t)~AFTERT_OFF_BIT));
    menu_put(MI_AFTERT, 0);
    vh_check("AFTERTOUCH OFF: the bit clear", menu_get(MI_AFTERT) == 0u && sense_at_enabled() == 0u && (ui_idle & AFTERT_OFF_BIT));
    menu_put(MI_KEYVEL, 1);
    menu_put(MI_AFTERT, 1);
    vh_check("both lines are separate bits: ON and ON", (ui_idle & KEYVEL_ON_BIT) && !(ui_idle & AFTERT_OFF_BIT) && sense_running());
    menu_put(MI_KEYVEL, 0);
    vh_check("  KEY VELOCITY OFF leaves AFTERTOUCH ON (the sensing runs for it)", !(ui_idle & KEYVEL_ON_BIT) && !(ui_idle & AFTERT_OFF_BIT) && sense_enabled() == 0u && sense_at_enabled() == 1u && sense_running());
    menu_put(MI_AFTERT, 0);
    vh_check("  both OFF: the sensing stops", !sense_running());
    ui_idle = keep;
    sense_enable(0);
    sense_at_enable(0);
}
/* the two lines act alone: KEY VELOCITY ON with AFTERTOUCH OFF measures the velocity and sends no pressure; AFTERTOUCH ON with KEY VELOCITY OFF sends the pressure and the notes play at 100 with no wait */
static void lines_alone(void)
{
    uint32_t k = 5, i, vmax = 0;
    sense_enable(0);
    sense_at_enable(0);
    sense_reset();
    sense_enable(1);                                      /* KEY VELOCITY alone */
    sense_curve_set(100);
    T = 1000000u;
    vh_setup(DM_STEREO);
    prime(k);
    feed(k, 1023, 0, 2200);
    feed(k, 200, 0, 2200);
    feed(k, 500, 1, 2200);
    keys_now(1u << k);
    for (i = 0; i < 30u; i++) {
        feed(k, 220, 0, 2200);
        feed(k, 500u - (uint32_t)(70.0 * (i + 1) / 30.0), 1, 2200);
    }
    keys_now(1u << k);
    vh_check("KEY VELOCITY alone: a squeeze sends no pressure (the part's aftertouch stays 0)", trk[0].at == 0u && sense_at_value() == 0u);
    keys_now(0);
    sense_enable(0);
    sense_reset();
    sense_at_enable(1);                                   /* AFTERTOUCH alone */
    sense_curve_set(100);
    T = 1000000u;
    vh_setup(DM_STEREO);
    prime(k);
    feed(k, 1023, 0, 2200);
    feed(k, 200, 0, 2200);
    feed(k, 500, 1, 2200);
    keys_now(1u << k);
    for (i = 0; i < 30u; i++) {
        feed(k, 220, 0, 2200);
        feed(k, 500u - (uint32_t)(70.0 * (i + 1) / 30.0), 1, 2200);
        keys_now(1u << k);
        if (trk[0].at > vmax)
            vmax = trk[0].at;
    }
    vh_check("AFTERTOUCH alone: a squeeze sends the pressure to the part (> 100)", vmax > 100u);
    vh_check("AFTERTOUCH alone: the velocity is the fixed 100 and no key waits", sense_vel(k) == 100u && sense_gate(1u << k, 0) == (1u << k));
    keys_now(0);
    sense_at_enable(0);
}
#endif

int main(void)
{
#if DUELL_VELO
    vh_check("setting OFF (the default since 0.2.31): the module starts OFF", sense_enabled() == 0u && sense_at_enabled() == 0u);
    sense_enable(0);                                      /* (the scenes below compare the flag-on build with the flag-off one: the setting OFF) */
#endif
    vh_setup(DM_STEREO);
    vh_scene(DM_STEREO, 0, 0, 0, "stereo");
    vh_scene(DM_STEREO, 0, 3, 0, "stereo+chord");
    vh_scene(DM_STEREO, 1, 0, 0, "stereo+arp");
    vh_scene(DM_LAYER, 0, 0, 50, "layer xfade");
    vh_scene(DM_LAYER, 0, 3, 100, "layer chord");
    vh_scene(DM_SPLIT, 0, 0, 0, "duel");
#if DUELL_VELO
    hooks();
    menu_line();
    lines_alone();
#else
    printf("velo: (the firmware as it was: no hooks in this build)\n");
#endif
    printf("velo hooks: %d failed\n", vh_fails);
    return vh_fails != 0;
}
