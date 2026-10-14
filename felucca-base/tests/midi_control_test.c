/* SPDX-License-Identifier: GPL-3.0-only
 * End-to-end channel input and external-clock tests using the real queues,
 * UART parser, ownership model, sequencer, voice renderer and UI source. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void midi_test_reset(void)
{
    ui_power_on();
    memset(midi_ch, 0, sizeof midi_ch); memset(midi_notes, 0, sizeof midi_notes);
    memset(midi_owners, 0, sizeof midi_owners); memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target); memset(&midi_clock, 0, sizeof midi_clock);
    memset(&um, 0, sizeof um); memset(midi_in_source, 0, sizeof midi_in_source);
    mi_r = mi_w = 0; midi_in_overflow = 0; midi_beat_samples = 0;
    fm1_in.notes = kb_prev = 0; fm1_ms = 0; song.sel = 0;
    events_block(CTL);
}
static void queued(uint32_t st, uint32_t d1, uint32_t d2, uint32_t source)
{
    midi_enqueue((st >= 0xF8u ? 0xFu : st >> 4) | st << 8 | d1 << 16 | d2 << 24, source);
    events_block(CTL);
}
static int gate_note(const track_t *t, uint32_t note)
{
    for (uint32_t i = 0; i < NVOICE; i++) if (t->v[i].active && t->v[i].gate && t->v[i].note == note) return 1;
    return 0;
}
static int controls_test(void)
{
    int bad = 0; midi_test_reset(); track_t *t = &trk[0];
    queued(0xE0, 127, 127, 1);
    bad += check("USB maximum bend is exactly the default +2 semitones", midi_bend_target[0] == 512);
    queued(0xE0, 0, 0, 1);
    bad += check("USB minimum bend is exactly the default -2 semitones", midi_bend_target[0] == -512);
    queued(0xE0, 0, 64, 1);
    bad += check("bend centre is zero", midi_bend_target[0] == 0);
    queued(0xB0, 101, 0, 1); queued(0xB0, 100, 0, 1); queued(0xB0, 6, 12, 1); queued(0xB0, 38, 50, 1);
    queued(0xE0, 127, 127, 1);
    bad += check("RPN0 supports semitone and cent bend sensitivity", midi_ch[0].semis == 12 && midi_ch[0].cents == 50 && midi_bend_target[0] == 3200);
    queued(0xB0, 99, 0, 1); queued(0xB0, 6, 24, 1);
    bad += check("selecting NRPN cancels RPN data entry", midi_ch[0].semis == 12);
    queued(0x90, 60, 100, 1); int32_t out[CTL]; int32_t before = midi_bend_q8[0];
    track_render(t, out, CTL);
    bad += check("bend is smoothed toward the target in the real voice renderer", midi_bend_q8[0] > before && midi_bend_q8[0] < midi_bend_target[0]);
    for (uint32_t i = 0; i < 64; i++) track_render(t, out, CTL);
    bad += check("bend smoothing reaches the exact target without changing saved pitch", midi_bend_q8[0] == 3200 && t->p[P_TRANS] == 0 && t->v[0].note == 60);
    queued(0xB0, 121, 0, 1);
    bad += check("Reset Controllers centres bend and preserves RPN sensitivity", !midi_bend_target[0] && midi_ch[0].semis == 12 && gate_note(t, 60));
    queued(0xB0, 1, 87, 1); queued(0xB0, 11, 23, 1); queued(0xD0, 90, 0, 1);
    bad += check("existing modwheel/expression/aftertouch stay routed to the track", t->mw == 87 && t->ex_off == 104 && t->at == 90);
    queued(0xE3, 127, 127, 1);
    bad += check("drum track ignores pitch bend", !midi_bend_target[3]);
    return bad;
}
static int sustain_test(void)
{
    int bad = 0; midi_test_reset(); track_t *t = &trk[0];
    queued(0x90, 60, 100, 1); queued(0xB0, 64, 127, 1); queued(0x80, 60, 0, 1);
    bad += check("pedal holds a released synth note and its owner", gate_note(t, 60) && midi_notes[0][60] == (1u | MIDI_PEDAL_NOTE));
    queued(0xB0, 64, 0, 1);
    bad += check("pedal release releases its held note", !gate_note(t, 60) && !midi_notes[0][60] && !midi_owners[0]);
    queued(0x90, 62, 100, 1); queued(0xB0, 64, 127, 1); queued(0xB0, 123, 0, 1);
    bad += check("All Notes Off honours sustain rather than hard-killing", gate_note(t, 62) && (midi_notes[0][62] & MIDI_PEDAL_NOTE));
    queued(0xB0, 121, 0, 1);
    bad += check("Reset Controllers releases pedal-held notes", !gate_note(t, 62) && !midi_notes[0][62]);
    queued(0x93, 36, 100, 1); queued(0xB3, 64, 127, 1); queued(0x83, 36, 0, 1);
    bad += check("drum note-offs do not accumulate pedal-held owners", !midi_notes[3][36] && !midi_owners[3]);
    queued(0x90, 67, 100, 1); queued(0xB0, 64, 127, 1); queued(0x80, 67, 0, 1); queued(0xB0, 120, 0, 1);
    bad += check("All Sound Off ignores pedal and discards all track ownership", !midi_notes[0][67] && !midi_owners[0] && !t->nheld && !gate_note(t, 67));
    return bad;
}
static int ownership_test(void)
{
    int bad = 0; midi_test_reset(); song.g[G_ROUTE] = 1; track_t *t = &trk[0];
    queued(0x94, 60, 100, 1); queued(0x95, 60, 100, 2);
    bad += check("USB/TRS channels can share one sounding note with independent owners", midi_owners[0] == 2u && gate_note(t, 60));
    queued(0x84, 60, 0, 1);
    bad += check("one channel's note-off preserves another channel's hold", midi_owners[0] == 1u && gate_note(t, 60));
    queued(0x85, 60, 0, 2);
    bad += check("last channel owner releases the shared note", !midi_owners[0] && !gate_note(t, 60));
    fm1_in.notes = 1u; events_block(CTL); uint32_t local = kb_note[0];
    queued(0x94, local, 100, 1); queued(0x84, local, 0, 1);
    bad += check("MIDI release cannot cut a still-held local keyboard note", gate_note(t, local));
    fm1_in.notes = 0; events_block(CTL);
    bad += check("local release after MIDI release ends the shared note", !gate_note(t, local));
    queued(0x94, 65, 100, 1); song.sel = 1; queued(0x84, 65, 0, 1);
    bad += check("note-off follows its note-on across selected-track changes", !gate_note(&trk[0], 65) && !midi_notes[4][65]);
    song.sel = 0; queued(0x94, 67, 100, 1); queued(0xB4, 64, 127, 1); queued(0x84, 67, 0, 1);
    panic_req |= 1u; events_block(CTL);
    queued(0x90, 67, 100, 1); queued(0xB4, 64, 0, 1);
    bad += check("preset panic forgets old pedal owners so later pedal-up preserves new notes", !midi_notes[4][67] && gate_note(&trk[0], 67));
    for (uint32_t i = 0; i <= MQ; i++) midi_enqueue(0x09u | 0x90u << 8 | 70u << 16 | 100u << 24, 1);
    events_block(CTL);
    bad += check("queue overflow recovers ownership and releases stuck notes", !midi_in_overflow && mi_r == mi_w && !midi_owners[0] && !gate_note(&trk[0], 67));
    return bad;
}
static void clock_setup(uint32_t mode)
{
    midi_test_reset(); song.g[G_CLOCK] = (int16_t)mode;
    trk[0].p[P_SLEN] = 16; trk[0].p[P_SDIV] = 2;
    for (uint32_t i = 0; i < 16; i++) trk[0].step[i] = (step_t){{(uint8_t)(60 + i)}, 1, ST_NOTE, 0, 100};
    events_block(CTL);
}
static void clock_packet(uint32_t source, uint32_t status, uint32_t ms)
{
    fm1_ms = ms;
    if (source == 2u) { um_byte(status); events_block(CTL); }
    else queued(status, 0, 0, source);
}
static void clock_to(uint32_t source, uint32_t start, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) clock_packet(source, 0xF8, start + (i * 125u) / 6u);
}
static int clock_test(uint32_t source)
{
    int bad = 0; clock_setup(source);
    clock_packet(source == 1u ? 2u : 1u, 0xFA, 1);
    bad += check(source == 1 ? "USB mode ignores TRS Start" : "TRS mode ignores USB Start", !song.playing);
    clock_packet(source, 0xFA, 2);
    bad += check(source == 1 ? "USB Start begins at step zero" : "TRS Start begins at step zero", song.playing && trk[0].seq_idx == 0u);
    clock_to(source, 2, 7);
    bad += check(source == 1 ? "USB six pulses advance exactly one 16th step" : "TRS six pulses advance exactly one 16th step", trk[0].seq_idx == 1u && song.g[G_BPM] == 120);
    fm1_ms = 132; events_block(CTL); uint16_t idx = trk[0].seq_idx; uint32_t pos = trk[0].seq_pos;
    clock_packet(source, 0xFC, 133);
    bad += check("external Stop preserves step position and releases sequence notes", !song.playing && trk[0].seq_idx == idx && trk[0].seq_pos == pos && !trk[0].seq_n);
    clock_packet(source, 0xFB, 150);
    bad += check("external Continue resumes the existing step position", song.playing && trk[0].seq_idx == idx && trk[0].seq_pos == pos);
    clock_packet(source, 0xF8, 151); fm1_ms = 672; events_block(CTL);
    bad += check("lost clock times out and releases transport notes", !song.playing && !trk[0].seq_n);
    clock_packet(source, 0xFA, 700); clock_to(source, 700, 7);
    bad += check("Start after timeout restarts cleanly at the master's phase", song.playing && trk[0].seq_idx == 1u);
    /* Original base values remain after MIDI Stop even while motion sounds. */
    trk[0].p[P_REV] = 23; motion_clear(&trk[0]); motion_set_event(&trk[0], 0, P_REV, 100);
    clock_packet(source, 0xFA, 1000); clock_packet(source, 0xF8, 1000);
    bad += check("external-clock playback applies recorded motion", trk[0].p[P_REV] == 100);
    clock_packet(source, 0xFC, 1010);
    bad += check("external Stop restores the musical parameter base", trk[0].p[P_REV] == 23);
    return bad;
}
static int clock_arp_and_boundaries(void)
{
    int bad = 0; clock_setup(1); trk[0].p[P_AMODE] = 1; trk[0].p[P_ARATE] = 2;
    queued(0x90, 60, 100, 1); queued(0x90, 64, 100, 1);
    clock_packet(1, 0xFA, 10); clock_to(1, 10, 1); uint32_t before = trk[0].arp_idx;
    clock_to(2, 20, 7);
    bad += check("wrong-source clocks cannot advance the ARP", trk[0].arp_idx == before);
    clock_to(1, 10, 7);
    bad += check("selected-source MIDI clock advances the ARP", trk[0].arp_idx > before);
    clock_setup(2); fm1_ms = 100; um_byte(0x90); um_byte(60); um_byte(0xFA); um_byte(100); events_block(CTL);
    bad += check("TRS realtime interleaving preserves the incomplete note message", gate_note(&trk[0], 60) && song.playing);
    /* Millisecond counter wraps naturally during external-clock operation. */
    clock_setup(1); clock_packet(1, 0xFA, 0xFFFFFFF0u); clock_packet(1, 0xF8, 0xFFFFFFF0u);
    clock_packet(1, 0xF8, 5u); fm1_ms = 10; events_block(CTL);
    bad += check("external clock timestamps survive uint32 millisecond wrap", song.playing && midi_clock.last_ms == 5u);
    clock_setup(1); transport_req = 1; events_block(CTL); fm1_ms = 501; events_block(CTL);
    bad += check("local PLAY waiting for missing external clock also times out", !song.playing);
    return bad;
}
/* ARP HOLD latched on an external clock: a Stop, a lost clock or a tap while stopped must not leave the arp note
 * sounding; Continue plays the latched chord again; the internal clock's free-running arp is unchanged by STOP */
static void arp_latch(uint32_t mode)
{
    midi_test_reset(); song.g[G_CLOCK] = (int16_t)mode; events_block(CTL);
    trk[0].p[P_AMODE] = 1; trk[0].p[P_AHOLD] = 1; trk[0].p[P_ARATE] = 2; trk[0].p[P_AGATE] = 120; trk[0].p[P_AOCT] = 2;
    events_block(CTL);
}
static uint32_t arp_ext_run(uint32_t ms)       /* Start, tap 60 (latched), clocks until the arp plays 72 */
{
    uint32_t i;
    clock_packet(1, 0xFA, ms); queued(0x90, 60, 100, 1); queued(0x80, 60, 0, 1);
    for (i = 0; i < 400 && trk[0].arp_note != 72; i++) clock_packet(1, 0xF8, ms + i * 21u);
    return ms + i * 21u;
}
static void idle_ms(uint32_t ms)               /* about ms of audio blocks, no MIDI */
{
    uint32_t i, ms0 = fm1_ms;
    for (i = 0; i < ms * 23u / 16u; i++) { fm1_ms = ms0 + i * 16u / 23u; events_block(CTL); }
}
static int arp_ext_stop_test(void)
{
    int bad = 0; uint32_t ms, i, off;
    arp_latch(1); ms = arp_ext_run(10);
    bad += check("ARP HOLD on the external clock plays the upper octave", trk[0].arp_note == 72 && gate_note(&trk[0], 72));
    clock_packet(1, 0xFC, ms + 5); idle_ms(2000);
    bad += check("external Stop ends the sounding ARP note", !trk[0].arp_note && !gate_note(&trk[0], 72) && !gate_note(&trk[0], 60));
    bad += check("external Stop keeps the latched HOLD chord", trk[0].nheld == 1u && trk[0].held[0] == 60u);
    ms = fm1_ms; clock_packet(1, 0xFB, ms);
    for (i = 1; i < 40 && !trk[0].arp_note; i++) clock_packet(1, 0xF8, ms + i * 21u);
    bad += check("Continue plays the latched chord again", trk[0].arp_note != 0);
    arp_latch(1); arp_ext_run(10); idle_ms(2000);
    bad += check("a lost external clock stops the transport and ends the ARP note", !song.playing && !trk[0].arp_note && !gate_note(&trk[0], 72));
    arp_latch(1); queued(0x90, 64, 100, 1); queued(0x80, 64, 0, 1);
    bad += check("a tap while the external transport is stopped sounds", trk[0].arp_note == 64);
    idle_ms(2000);
    bad += check("its ARP gate still ends in real time", !trk[0].arp_note && !gate_note(&trk[0], 64));
    arp_latch(0); transport_req = 1; events_block(CTL); queued(0x90, 60, 100, 1); queued(0x80, 60, 0, 1);
    for (i = 0; i < 4000 && trk[0].arp_note != 72; i++) events_block(CTL);
    off = trk[0].arp_off;
    transport_req = 2; events_block(CTL);
    bad += check("internal clock: STOP leaves the free-running ARP note to its gate", trk[0].arp_note == 72 && gate_note(&trk[0], 72) && trk[0].arp_off == off - CTL);
    for (i = 0; i < 4000 && (!trk[0].arp_note || trk[0].arp_note == 72); i++) events_block(CTL);
    bad += check("internal clock: the ARP keeps stepping after STOP", trk[0].arp_note && trk[0].arp_note != 72 && trk[0].nheld == 1u);
    return bad;
}
/* ARP on an external clock (owner report, Akai Force): the held notes follow the keys at once, whatever the clock does; only the STEPPING
 * waits for pulses. Stale notes never survive a release; Start / Stop / Continue create no notes; a clock that runs without a Start steps
 * the arp; the CLK status says EXT or NO CLOCK. */
static void key_src(uint32_t src, uint32_t on, uint32_t note)   /* a note from the external keyboard on USB (1) or TRS (2), channel 1 */
{
    if (src == 2u) { um_byte(on ? 0x90u : 0x80u); um_byte(note); um_byte(on ? 100u : 0u); events_block(CTL); }
    else queued(on ? 0x90u : 0x80u, note, on ? 100u : 0u, 1);
}
static uint32_t ext_gates(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].gate;
    return n;
}
static void arp_ext_setup(uint32_t src, uint32_t mode)
{
    midi_test_reset(); song.g[G_CLOCK] = (int16_t)src; events_block(CTL);
    trk[0].p[P_AMODE] = (int16_t)mode; trk[0].p[P_ARATE] = 2; trk[0].p[P_AGATE] = 64; trk[0].p[P_AOCT] = 1;
    memset(trk[0].step, 0, sizeof trk[0].step);
    events_block(CTL);
}
/* runs n pulses; returns how many blocks had an arp note sounding */
static uint32_t pulses(uint32_t src, uint32_t *ms, uint32_t n)
{
    uint32_t i, on = 0;
    for (i = 0; i < n; i++) { *ms += 21u; clock_packet(src, 0xF8, *ms); on += trk[0].arp_note != 0 || ext_gates(&trk[0]) != 0; }
    return on;
}
static int arp_ext_clock_test(void)
{
    int bad = 0, cs;
    static const uint8_t MODES[] = {1, 2, 3, 4, 14, 15};            /* UP DOWN UPDN RND CHORD SEQ */
    static const char *const MN[] = {"UP", "DOWN", "UPDOWN", "RANDOM", "CHORD", "SEQ"};
    for (cs = 1; cs <= 2; cs++)
        for (uint32_t m = 0; m < 6u; m++) {
            char w[120]; uint32_t ms = 100, blocks, seen = 0, i; int ok;
            const char *cn = cs == 1 ? "USB" : "TRS";
            /* (i) no clock yet: notes on and off from the external keyboard, then Start and pulses: nothing is held, nothing plays */
            arp_ext_setup((uint32_t)cs, MODES[m]);
            key_src((uint32_t)cs, 1, 60); key_src((uint32_t)cs, 0, 60); key_src((uint32_t)cs, 1, 64); key_src((uint32_t)cs, 0, 64);
            idle_ms(200);
            ok = !trk[0].nheld && !trk[0].arp_note && !ext_gates(&trk[0]);
            clock_packet((uint32_t)cs, 0xFA, ms);
            blocks = pulses((uint32_t)cs, &ms, 96);
            sprintf(w, "CLK=%s ARP %s: notes pressed and released before Start leave nothing held: Start and 96 pulses play nothing", cn, MN[m]);
            bad += check(w, ok && !trk[0].nheld && !blocks);
            if (MODES[m] == 15) continue;                        /* (SEQ with an empty pattern: rests) */
            /* (ii) a held note, pulses running: the arp plays; released: it ends within the gate */
            key_src((uint32_t)cs, 1, 60); key_src((uint32_t)cs, 1, 64);
            blocks = pulses((uint32_t)cs, &ms, 48);
            ok = trk[0].nheld == 2u && blocks > 0u;
            key_src((uint32_t)cs, 0, 60); key_src((uint32_t)cs, 0, 64);
            blocks = pulses((uint32_t)cs, &ms, 12);
            sprintf(w, "CLK=%s ARP %s: plays the held notes with the clock, ends cleanly on release", cn, MN[m]);
            bad += check(w, ok && !trk[0].nheld && !trk[0].arp_note && !ext_gates(&trk[0]));
            /* (iii) Stop with a note held ends the arp note; Start again plays it again; release: silence */
            key_src((uint32_t)cs, 1, 60); pulses((uint32_t)cs, &ms, 12);
            clock_packet((uint32_t)cs, 0xFC, ms);
            ok = trk[0].nheld == 1u && !trk[0].arp_note && !ext_gates(&trk[0]);
            blocks = pulses((uint32_t)cs, &ms, 24);
            ok &= !blocks;                                           /* (a Stop: the pulses that still come do not step the arp) */
            clock_packet((uint32_t)cs, 0xFA, ms);
            blocks = pulses((uint32_t)cs, &ms, 24);
            ok &= blocks > 0u && trk[0].nheld == 1u;
            key_src((uint32_t)cs, 0, 60); pulses((uint32_t)cs, &ms, 12);
            sprintf(w, "CLK=%s ARP %s: Stop with a note held is silent, Start plays it again, release ends it", cn, MN[m]);
            bad += check(w, ok && !trk[0].nheld && !trk[0].arp_note && !ext_gates(&trk[0]));
            /* a note pressed and released while the clock is silent must not play when it starts */
            arp_ext_setup((uint32_t)cs, MODES[m]); ms = 100;
            key_src((uint32_t)cs, 1, 67); idle_ms(300); key_src((uint32_t)cs, 0, 67); idle_ms(100);
            clock_packet((uint32_t)cs, 0xFA, ms);
            for (i = 0; i < 4; i++) seen += pulses((uint32_t)cs, &ms, 24);
            sprintf(w, "CLK=%s ARP %s: a stale note (pressed, released with no clock) does not play on Start", cn, MN[m]);
            bad += check(w, !seen && !trk[0].nheld);
        }
    /* a clock that runs without a Start steps the arp (and the status says so); a Stop makes it wait again */
    {
        uint32_t ms = 100, blocks;
        arp_ext_setup(1, 1);
        key_src(1, 1, 60); key_src(1, 1, 64);
        idle_ms(300);
        bad += check("CLK=USB, a key pressed, no clock: the arp waits (its first note gate ends in real time), the status is NO CLOCK",
                     !trk[0].arp_note && !ext_gates(&trk[0]) && clock_state() == 2u);
        blocks = pulses(1, &ms, 48);
        bad += check("pulses without a Start step the arp from the held keys; the transport stays stopped; the status is EXT",
                     blocks > 0u && !song.playing && clock_state() == 1u);
        clock_packet(1, 0xFC, ms);
        blocks = pulses(1, &ms, 24);
        bad += check("a Stop ends it; pulses after it do not step the arp (the held notes stay)", !blocks && trk[0].nheld == 2u);
        key_src(1, 0, 60); key_src(1, 0, 64);
        bad += check("released while waiting: nothing held", !trk[0].nheld);
        fm1_ms = ms; idle_ms(1500);
        bad += check("the clock stops arriving: NO CLOCK again after 1 s", clock_state() == 2u);
    }
    /* the status line: INT none, USB / TRS EXT while pulses arrive, NO CLOCK after 1 s, back to none on INT */
    {
        uint32_t ms = 100;
        arp_ext_setup(0, 1);
        bad += check("CLK=INT: no status", clock_state() == 0 && !clock_why());
        song.g[G_CLOCK] = 1; events_block(CTL);
        bad += check("CLK=USB, nothing arrived: NO CLOCK", clock_state() == 2u && str_eq(clock_why(), "NO CLK"));
        pulses(1, &ms, 4);
        bad += check("CLK=USB, pulses arrive: EXT, the source shown", clock_state() == 1u && str_eq(clock_why(), "USB"));
        pulses(2, &ms, 4);
        bad += check("pulses of the other source do not count", clock_state() == 1u);
        fm1_ms = ms; idle_ms(1200);
        bad += check("no pulse for 1 s: NO CLOCK", clock_state() == 2u);
        song.g[G_CLOCK] = 2; events_block(CTL);
        bad += check("CLK=TRS: NO CLOCK until its pulses arrive", clock_state() == 2u && str_eq(clock_why(), "NO CLK"));
        pulses(2, &ms, 4);
        bad += check("CLK=TRS, pulses arrive: EXT (TRS)", clock_state() == 1u && str_eq(clock_why(), "TRS"));
        song.g[G_CLOCK] = 0; events_block(CTL);
        bad += check("back to INT: no status", clock_state() == 0 && !clock_why());
    }
    /* CLK switched INT <-> USB with notes held and released in between: the bookkeeping follows the keys */
    {
        uint32_t ms = 100, blocks;
        arp_ext_setup(0, 1);
        key_src(1, 1, 60);
        for (uint32_t i = 0; i < 4000 && !trk[0].arp_note; i++) events_block(CTL);
        bad += check("INT: a held key arps", trk[0].nheld == 1u && trk[0].arp_note);
        song.g[G_CLOCK] = 1; events_block(CTL);
        bad += check("switched to USB with the key held: still held, the arp note ended", trk[0].nheld == 1u && !trk[0].arp_note);
        key_src(1, 0, 60);
        key_src(1, 1, 64); key_src(1, 0, 64);
        song.g[G_CLOCK] = 0; events_block(CTL);
        bad += check("released on USB, back to INT: nothing held", !trk[0].nheld);
        idle_ms(300);
        song.g[G_CLOCK] = 1; events_block(CTL);
        clock_packet(1, 0xFA, ms);
        blocks = pulses(1, &ms, 48);
        bad += check("INT / USB switches leave no stale notes: USB Start plays nothing", !blocks && !trk[0].arp_note);
    }
    /* the stored steps of a multi are the user's content: an external Start plays them (INT PLAY does the same); the ARP itself stays silent */
    {
        uint32_t ms = 100, i, started = 0, arp_on = 0; uint8_t last = 0;
        midi_test_reset(); project_load(2); events_block(CTL);   /* (M03 EERIE SIGNAL since 0.2.13, M01 before: part 2 holds steps) */
        song.g[G_CLOCK] = 1; events_block(CTL);
        clock_packet(1, 0xFA, ms);
        for (i = 0; i < 96u; i++) {
            ms += 21u; clock_packet(1, 0xF8, ms);
            if (trk[1].seq_n && trk[1].seq_notes[0] != last) started++;
            last = trk[1].seq_n ? trk[1].seq_notes[0] : 0;
            arp_on += trk[0].arp_note != 0 || trk[1].arp_note != 0;
        }
        bad += check("M03 EERIE SIGNAL on USB Start, no keys: the stored steps of part 2 play (16 notes in 96 pulses), the ARPs play nothing",
                     started == 16u && !arp_on && !trk[0].nheld && !trk[1].nheld);
    }
    return bad;
}
/* USB-MIDI back-pressure: full 64-byte bulk packets (16 events) faster than the audio ISR drains the ring are
 * left in the endpoint (NAK) instead of overflowing it, which would panic every sounding note */
static int usb_burst_test(void)
{
    int bad = 0; uint8_t pkt[64]; uint32_t e, k, taken = 0;
    midi_test_reset();
    queued(0x90, 48, 100, 1);
    for (e = 0; e < 16; e++) {                       /* CC7 = 100 on 16 channels */
        pkt[4 * e] = 0x0B; pkt[4 * e + 1] = (uint8_t)(0xB0 | e); pkt[4 * e + 2] = 7; pkt[4 * e + 3] = 100;
    }
    for (k = 0; k < 5; k++)                          /* 5 polls at 2 kHz before the next half drains */
        taken += (uint32_t)ep1_take(pkt, sizeof pkt);
    bad += check("a USB burst fills the MIDI ring without overflowing it", taken == 3u && !midi_in_overflow && mi_w - mi_r == 48u);
    bad += check("the packet that does not fit is refused (left for the next poll)", !ep1_take(pkt, sizeof pkt) && mi_w - mi_r == 48u);
    for (e = 0; e < 8; e++)                          /* TRS MIDI meanwhile: its 8 slots are free */
        midi_in_event(0x0Bu | 0xB1u << 8 | 1u << 16 | e << 24);
    bad += check("TRS MIDI into a ring full of USB still fits (no overflow)", !midi_in_overflow && mi_w - mi_r == 56u);
    events_block(CTL);
    bad += check("0.2.11: one block takes 16 events (MIDI_DRAIN), the other 40 wait", gate_note(&trk[0], 48) && mi_w - mi_r == 40u);
    bad += check("the refused packet is taken once the ring has room", ep1_take(pkt, sizeof pkt) && mi_w - mi_r == 56u);
    for (k = 0; k < 4; k++) events_block(CTL);
    bad += check("the ring drains with no panic (4 blocks): the held note keeps sounding", gate_note(&trk[0], 48) && mi_r == mi_w && !midi_in_overflow);
    bad += check("the held note survives all 88 events", gate_note(&trk[0], 48) && !midi_in_overflow);
    for (k = 0; k < 3; k++) ep1_take(pkt, sizeof pkt);
    midi_in_event(0x09u | 0x90u << 8 | 50u << 16 | 100u << 24);   /* one more event: 49 used, 15 free */
    bad += check("free space below one packet refuses a whole packet", !ep1_take(pkt, sizeof pkt) && !midi_in_overflow);
    events_block(CTL);
    return bad;
}
/* DUELL 0.2.11 (SPEC 6.3): the MIDI-in drain takes at most MIDI_DRAIN (16) events a block, the rest the next block in order; real-time
 * bytes are taken at once (in their order, once), an All Notes Off pulls the drain up to it (in order) */
static int drain_test(void)
{
    int bad = 0; uint32_t i; int16_t ref[P_COUNT], got[P_COUNT];
    midi_test_reset();
    queued(0xB0, 74, 16, 1);                         /* the reference: CC74 = 16 alone */
    memcpy(ref, trk[0].p, sizeof ref);
    midi_test_reset();
    for (i = 1; i <= 40u; i++) midi_enqueue(0x0Bu | 0xB0u << 8 | 74u << 16 | i << 24, 1);
    events_block(CTL);
    memcpy(got, trk[0].p, sizeof got);
    bad += check("0.2.11 drain: 40 CC74 in the ring: the first block takes the first 16, in order (as CC74 = 16 alone)",
                 mi_w - mi_r == 24u && !memcmp(ref, got, sizeof ref));
    events_block(CTL); events_block(CTL);
    bad += check("0.2.11 drain: the 24 others in the next two blocks (16 + 8), the ring empty (CC74 = 40 now: the CC does move a value)",
                 mi_r == mi_w && !midi_in_overflow && memcmp(ref, trk[0].p, sizeof ref));
    clock_setup(1);                                  /* real time: Start and 3 clocks behind 20 CCs */
    for (i = 0; i < 20u; i++) midi_enqueue(0x0Bu | 0xB0u << 8 | 74u << 16 | i << 24, 1);
    fm1_ms = 10;
    midi_enqueue(0xFu | 0xFAu << 8, 1);
    for (i = 0; i < 3u; i++) midi_enqueue(0xFu | 0xF8u << 8, 1);
    events_block(CTL);
    bad += check("0.2.11 drain: real time behind the limit: Start and the clocks taken in the same block", song.playing && midi_clock.tempo_valid && midi_clock.tempo_n == 2u && mi_w - mi_r == 8u);
    events_block(CTL);
    bad += check("0.2.11 drain: .. and never twice (the next block skips them), the ring empty", song.playing && midi_clock.tempo_n == 2u && mi_r == mi_w);
    midi_test_reset();                               /* All Notes Off behind the limit: in order, at once */
    queued(0x90, 60, 100, 1);
    for (i = 0; i < 20u; i++) midi_enqueue(0x0Bu | 0xB0u << 8 | 74u << 16 | i << 24, 1);
    midi_enqueue(0x0Bu | 0xB0u << 8 | 123u << 16, 1);
    midi_enqueue(0x09u | 0x90u << 8 | 62u << 16 | 100u << 24, 1);
    events_block(CTL);
    bad += check("0.2.11 drain: All Notes Off behind the limit ends the note before it in the same block; the note after it waits",
                 !gate_note(&trk[0], 60) && !gate_note(&trk[0], 62) && mi_w - mi_r == 1u);
    events_block(CTL);
    bad += check("0.2.11 drain: .. and plays the next block", gate_note(&trk[0], 62) && mi_r == mi_w);
    return bad;
}
/* MENU > MIDI > MIDI IN (ROUT; up to 1.1.5 GLO > SYSTEM, #68): CH1-4 listens to channels 1..4 only, channels 5..16 are free for other instruments
 * (notes, bend, CCs, aftertouch, panic and reset all ignored, from USB and TRS); SEL plays the selected track
 * from every channel; a switch to CH1-4 lets go of what channels 5..16 held */
static int any_gate(void)
{
    for (uint32_t i = 0; i < NTRK * NVOICE; i++) if (trk[i / NVOICE].v[i % NVOICE].active && trk[i / NVOICE].v[i % NVOICE].gate) return 1;
    return 0;
}
static int route_test(void)
{
    int bad = 0; uint32_t ch, src, owned = 0;
    midi_test_reset(); song.sel = 2;
    bad += check("ROUT defaults to CH1-4", song.g[G_ROUTE] == 0);
    for (src = 1; src <= 2u; src++)
        for (ch = 4; ch < 16u; ch++) queued(0x90 | ch, 60 + ch, 100, src);
    for (ch = 4; ch < 16u; ch++) owned |= midi_notes[ch][60 + ch];
    bad += check("CH1-4: note-ons on channels 5..16 (USB and TRS) play nothing", !any_gate() && !owned && !midi_owners[2] && !midi_hint);
    queued(0x91, 62, 100, 1); queued(0x93, 40, 100, 2);
    bad += check("PARTS: channel 2 still plays part 2; channel 4 (no part's) plays nothing", gate_note(&trk[1], 62) && midi_notes[1][62] == 2u && !midi_notes[3][40] && !gate_note(&trk[3], 40));
    queued(0xE4, 127, 127, 1); queued(0xEF, 0, 0, 2); queued(0xE9, 127, 127, 1);
    bad += check("CH1-4: pitch bend on channels 5, 10, 16 bends no part", !midi_bend_target[0] && !midi_bend_target[1] && !midi_bend_target[2] && !midi_ch[4].bend && !midi_ch[15].bend);
    queued(0xB4, 1, 99, 1); queued(0xB9, 11, 10, 2); queued(0xDF, 77, 0, 1);
    bad += check("CH1-4: CC1 / CC11 and aftertouch on channels 5..16 reach no MOD source", !trk[2].mw && !trk[2].ex_off && !trk[2].at && !trk[0].mw && !trk[0].at);
    queued(0xB4, 101, 0, 1); queued(0xB4, 100, 0, 1); queued(0xB4, 6, 24, 1);
    bad += check("CH1-4: RPN on channel 5 changes nothing", midi_ch[4].semis != 24u);
    queued(0xB4, 64, 127, 1); queued(0x81, 62, 0, 1);
    bad += check("CH1-4: a channel 5 sustain pedal does not hold channel 2's note", !gate_note(&trk[1], 62) && !midi_notes[1][62]);
    queued(0x91, 62, 100, 1); queued(0xB1, 1, 50, 1);
    queued(0xB4, 120, 0, 1); queued(0xB9, 123, 0, 2); queued(0xBF, 121, 0, 1);
    bad += check("PARTS: CC120 / CC123 / CC121 on channels that are no part's leave part 2 sounding", gate_note(&trk[1], 62) && midi_owners[1] == 1u && trk[1].mw == 50);
    queued(0xB1, 123, 0, 1); queued(0x83, 40, 0, 2);
    bad += check("CH1-4: panic on channel 2 still works", !gate_note(&trk[1], 62) && !midi_owners[1] && !midi_owners[3]);

    midi_test_reset(); song.sel = 1; song.g[G_ROUTE] = 1; trk[1].p[P_VOICE] = V_POLY; trk[1].p[P_SUS] = 127; events_block(CTL);
    queued(0x90, 60, 100, 1); queued(0x94, 62, 100, 2); queued(0x99, 64, 100, 1); queued(0x9F, 65, 100, 2);
    bad += check("SEL: channels 1, 5, 10, 16 all play the selected track", gate_note(&trk[1], 60) && gate_note(&trk[1], 62) && gate_note(&trk[1], 64) && gate_note(&trk[1], 65) && !gate_note(&trk[0], 60) && midi_owners[1] == 4u);
    queued(0xEF, 127, 127, 1); queued(0xB9, 1, 66, 2); queued(0xD4, 33, 0, 1);
    bad += check("SEL: bend, CC1 and aftertouch from channels 5..16 reach the selected track", midi_bend_target[1] == 512 && trk[1].mw == 66 && trk[1].at == 33);
    queued(0xB4, 64, 127, 1); queued(0x84, 62, 0, 2);
    bad += check("SEL: channel 5 pedal holds its note", gate_note(&trk[1], 62) && (midi_notes[4][62] & MIDI_PEDAL_NOTE));
    song.g[G_ROUTE] = 0; events_block(CTL);
    bad += check("SEL -> CH1-4 releases channels 5..16's notes (pedal-held too), keeps channel 1's",
                 !gate_note(&trk[1], 62) && !gate_note(&trk[1], 64) && !gate_note(&trk[1], 65) && gate_note(&trk[1], 60) &&
                 !midi_notes[4][62] && !midi_notes[9][64] && !midi_notes[15][65] && midi_owners[1] == 1u && !midi_ch[4].pedal);
    bad += check(".. and resets their bend: each part follows its own channel 1..4", !midi_bend_target[1] && !midi_ch[15].bend);
    queued(0x80, 60, 0, 1);
    bad += check("channel 1's note-off still releases its note after the switch", !any_gate() && !midi_owners[1]);
    return bad;
}
/* 1.2 (Discussion #143): ROUT CH5-8, CH9-12, CH13-16: tracks 1..4 on those four channels (notes, bend, CCs, the CC map,
 * panic), every other channel ignored, as CH1-4 does with 1..4; a switch between blocks lets go of the old block's
 * notes; the clock (realtime) is not a channel's: it runs whatever ROUT says; the keys send on the block's channel */
static uint32_t out_last(void) { return mo_w != mo_r ? midi_out_q[(mo_w - 1u) % MQ] : 0u; }
/* #103: the standard CC map, on the track the channel plays, as a knob turn (and as AUTOMATION records one) */
static int cc_map_test(void)
{
    static const struct { uint8_t cc; uint8_t id; } COMMON[] = {
        {5, P_GLIDE}, {7, P_LEVEL}, {72, P_REL}, {73, P_ATK}, {75, P_DEC}, {91, P_REV}, {93, P_CHOR}, {94, P_DLY},
    };
    int bad = 0, ok = 1; uint32_t i, e; int16_t before[P_COUNT], cut0;
    midi_test_reset();
    bad += check("power-on parts 1 and 2: ANALOG, FM6", trk[0].eng_req == 0u && trk[1].eng_req == ENGI_FM6);
    for (i = 0; i < NELEM(COMMON); i++) {
        queued(0xB0, COMMON[i].cc, 127, 1);
        ok &= trk[0].p[COMMON[i].id] == TP[COMMON[i].id].max;
        queued(0xB0, COMMON[i].cc, 0, 1);
        ok &= trk[0].p[COMMON[i].id] == TP[COMMON[i].id].min;
        queued(0xB0, COMMON[i].cc, 37, 1);
        ok &= trk[0].p[COMMON[i].id] == 37;
    }
    bad += check("CC5 7 72 73 75 91 93 94: GLIDE LEVEL REL ATK DEC REV CHO DLY, 0..127 over their range", ok);
    queued(0xB0, 10, 0, 1); ok = trk[0].p[P_PAN] == -64;
    queued(0xB0, 10, 127, 1); ok &= trk[0].p[P_PAN] == 63;
    queued(0xB0, 10, 64, 1); ok &= trk[0].p[P_PAN] == 0;
    bad += check("CC10 PAN: 0 left, 64 centre, 127 right", ok);
    queued(0xB0, 74, 127, 1); queued(0xB0, 71, 0, 1);
    bad += check("CC74 / CC71 on ANALOG: CUT / RES", trk[0].p[P_E4] == 127 && trk[0].p[P_E5] == 0);
    queued(0xB1, 74, 64, 1);
    ok = trk[1].p[P_FREEA] == 41;
    queued(0xB1, 74, 127, 1);
    bad += check("CC74 on FM6 (channel 2, no filter of its own): the multimode filter's CUT over its dial (0: 30 Hz, 64: step 41, 127: the open end 82)",
                 ok && trk[1].p[P_FREEA] == UF_STEPS && trk[1].p[P_E2] == 0);
    queued(0xB1, 71, 99, 1);
    bad += check("CC71 on FM6: the multimode filter's RES (0..127)", trk[1].p[P_FREEB] == 99);
    queued(0xB0, 74, 20, 1); queued(0xB0, 71, 20, 1);
    bad += check("  ANALOG (a filter of its own): CC74 / CC71 leave the multimode filter's ids alone", trk[0].p[P_FREEA] == TP[P_FREEA].def && trk[0].p[P_FREEB] == 0);
    queued(0xB0, 74, 127, 1); queued(0xB0, 71, 0, 1);
    for (e = 0, ok = 1; e < NENGINES; e++) {   /* the table names a continuous EDIT value of every engine */
        uint32_t m = MIDI_CC_ENG[e], k;
        for (k = 0; k < 2u; k++) {
            uint32_t n = (m >> (k ? 0 : 4)) & 15u;
            const param_desc_t *d = n ? &ENGINES[e]->edit[n - 1u] : 0;
            ok &= (n <= 8u && (!d || (d->fmt != F_ENUM && d->max - d->min >= 24))) || (n == 15u && ENGINES[e]->ufilt == 1u);
        }
        ok &= ENGINES[e]->ufilt != 1u || m == 0xFFu;       /* (an engine with the multimode filter in place of its own: CUT, RES both; ufilt 2 keeps its own filter's) */
    }
    bad += check("every engine's CC74 / CC71 target is a continuous EDIT value (or none), or the multimode filter's (engines with ufilt 1)", ok);
    trk[1].eng_req = trk[1].engine = 7u;               /* WHEEL: no filter of its own: the universal one */
    queued(0xB1, 74, 3, 1); queued(0xB1, 71, 4, 1);
    bad += check("CC74 / CC71 on WHEEL: CUT 2 (3 x 82 / 127), RES 4", trk[1].p[P_FREEA] == 2 && trk[1].p[P_FREEB] == 4);
    trk[1].eng_req = trk[1].engine = ENGI_FM6;
    memcpy(before, trk[0].p, sizeof before);
    queued(0xB0, 121, 0, 1);
    bad += check("CC121 resets no mapped parameter (RP-015)", !memcmp(before, trk[0].p, sizeof before));
    queued(0xB0, 20, 99, 1); queued(0xB0, 76, 99, 1); queued(0xB0, 95, 99, 1);
    bad += check("CCs outside the map change nothing", !memcmp(before, trk[0].p, sizeof before));
    memcpy(before, trk[2].p, sizeof before);
    queued(0xB4, 7, 5, 1); queued(0xB9, 74, 5, 2); queued(0xBF, 91, 5, 1);
    ok = !memcmp(before, trk[2].p, sizeof before) && trk[0].p[P_LEVEL] != 5 && trk[3].p[P_LEVEL] != 5;
    bad += check("ROUT CH1-4: mapped CCs on channels 5..16 reach no part", ok);
    song.g[G_ROUTE] = 1; song.sel = 2; events_block(CTL);
    queued(0xB9, 7, 5, 2);
    bad += check("ROUT SEL: channel 10's CC7 sets the selected track's LEVEL", trk[2].p[P_LEVEL] == 5 && trk[0].p[P_LEVEL] != 5);
    song.g[G_ROUTE] = 0; song.sel = 0; events_block(CTL);

    /* AUTOMATION: the selected track armed and playing records the CC as a knob move; else it is the new base */
    midi_test_reset();
    cut0 = trk[0].p[P_E4];
    song.rec = 1; seq_start(); events_block(CTL);
    queued(0xB0, 74, 20, 1);
    bad += check("armed and playing: CC74 records a motion event of CUT on the selected track", motion.count == 1u &&
                 motion.event[0].param == P_E4 && motion.event[0].value == 20 && (motion.event[0].place >> 6) == 0u);
    queued(0xB1, 74, 20, 1);
    bad += check("  another track (channel 2, not selected): not recorded", motion.count == 1u && trk[1].p[P_FREEA] == 13);
    seq_stop(); events_block(CTL);
    bad += check("  stopped: the patch's own CUT comes back", trk[0].p[P_E4] == cut0);
    song.rec = 0;
    queued(0xB0, 74, 77, 1);
    bad += check("not armed: the CC is the new value, no event", trk[0].p[P_E4] == 77 && motion.count == 1u);
    return bad;
}
/* 1.5 (Discussion #170): MIDI LEARN. GLO + D4 on, a knob picks, a CC is learned; the map in favorites.factory[14] */
static int ml_entry(uint32_t cc, uint32_t k, uint32_t id) { return ml_cc_of(k, id) == cc + 1u; }
static int learn_test(void)
{
    int bad = 0, ok;
    uint32_t i;
    int16_t before[P_COUNT], cut, lvl;
    midi_test_reset();
    bad += check("MIDI LEARN: nothing learned at power-on (every older setting: all 0)", !ui.ml.on && !ml_count() &&
                 !ml_tab[0] && !memcmp(ml_tab, ml_tab + 1, 31));
    memcpy(before, trk[0].p, sizeof before);
    queued(0xB0, 20, 99, 1);
    bad += check("  an unlearned CC changes nothing (as before)", !memcmp(before, trk[0].p, sizeof before));

    /* on: GLO > MIDI LEARN, KNOB 1 right */
    go_title("MIDI LEARN");
    turn(EN_K1, 1);
    ok = ui.ml.on && msg_is("MIDI LEARN ON");
    turn(EN_K1, 1);                                                  /* (already on: nothing) */
    ok &= ui.ml.on;
    bad += check("GLO > MIDI LEARN: KNOB 1 right = MIDI LEARN on", ok);
    queued(0xB0, 20, 99, 1); frame();
    bad += check("  no parameter picked yet: a CC learns nothing, sets nothing", !ml_count() &&
                 !memcmp(before, trk[0].p, sizeof before));

    /* a knob picks (as it edits), a CC is learned */
    frames(300);                                                     /* (#39: the knobs quiet after a layer) */
    go_title("EDIT 2");
    cut = trk[0].p[P_E4];
    turn(EN_K1, -3);
    ok = ui.ml.pick && ui.ml.trk == 0u && ui.ml.id == P_E4 && trk[0].p[P_E4] == cut - 3 && ml_arm;
    { char b[24]; ml_line(b); ok &= str_eq(b, "T1 CUT: SEND A CC"); }
    queued(0xB0, 1, 99, 1); queued(0xB0, 64, 127, 1); queued(0xB0, 64, 0, 1); queued(0xB0, 0, 5, 1);
    frame();
    ok &= !ml_count() && ui.ml.pick;                                   /* (CC1, CC64, CC0: never learned) */
    queued(0xB0, 20, 100, 1); frame();
    ok &= ml_count() == 1u && ml_entry(20, 0, P_E4) && msg_is("CC20 -> T1 CUT") && !ui.ml.pick && !ml_arm;
    ok &= trk[0].p[P_E4] == cut - 3;                                /* (the learning CC sets nothing itself) */
    { char b[24]; ml_line(b); ok &= str_eq(b, "LEARN: TURN A KNOB"); }
    bad += check("  EDIT 2 KNOB 1 picks T1 CUT; CC1 / CC64 / CC0 are passed over; CC20 is learned", ok);
    queued(0xB0, 20, 127, 1);
    ok = trk[0].p[P_E4] == 127;
    queued(0xB1, 20, 0, 1);                                          /* (channel 2: part 2's, still T1 CUT) */
    ok &= trk[0].p[P_E4] == 0;
    turn(EN_K1, 1);
    { char b[24]; ml_line(b); ok &= str_eq(b, "T1 CUT = CC20"); }
    bad += check("  CC20 sets T1 CUT on any channel ROUT hears; the knob again shows its CC", ok);
    song.g[G_ROUTE] = 0; queued(0xB9, 20, 64, 1);
    bad += check("  ROUT CH1-4: CC20 on channel 10 is ignored as before", trk[0].p[P_E4] == 1);

    /* the standard map: a learned CC no longer reaches it; GLO's LEVEL knobs pick a part's LEVEL */
    go_title("GLO");
    turn(EN_K4, 2);
    ok = ui.ml.pick && ui.ml.trk == 1u && ui.ml.id == P_LEVEL;
    frames(300);
    queued(0xB0, 74, 10, 1); frame();
    ok &= ml_entry(74, 1, P_LEVEL) && msg_is("CC74 -> T2 LVL");
    cut = trk[0].p[P_E4];
    queued(0xB0, 74, 127, 1);
    ok &= trk[1].p[P_LEVEL] == TP[P_LEVEL].max && trk[0].p[P_E4] == cut;
    bad += check("  GLO KNOB 4 (LEVEL 2) picks T2 LEVEL; CC74 learned: T2 LEVEL, no more T1 CUT (the standard map)", ok);
    ml_off(); ui.ml.on = 1;
    frames(300);
    turn(EN_K2, 2);                                     /* (a nudge: nothing fires, nothing picked) */
    frames(600);
    turn(EN_K2, MORPH_DET);
    ok = ui.ml.pick && ui.ml.trk == 1u && ui.ml.id == P_MUTE;
    frames(600);
    turn(EN_K2, -MORPH_DET);
    trk[1].p[P_MUTE] = 0;
    bad += check("  GLO KNOB 2 (MUTE 2) picks T2 MUTE", ok);

    /* one CC one parameter, a parameter one CC */
    go_title("EDIT 2");
    turn(EN_K1, 1);
    queued(0xB0, 21, 0, 1); frame();
    ok = ml_count() == 2u && ml_entry(21, 0, P_E4) && !ml_entry(20, 0, P_E4);
    queued(0xB0, 20, 99, 1);
    ok &= trk[0].p[P_E4] != 99;
    turn(EN_K2, 1);                                                  /* T1 RES */
    queued(0xB0, 21, 0, 1); frame();
    ok &= ml_count() == 2u && ml_entry(21, 0, P_E5) && !ml_cc_of(0, P_E4);
    bad += check("  learning again replaces: CUT on CC21 drops CC20; CC21 on RES drops CUT's", ok);

    /* OCT- clears the picked parameter's CC; no octave */
    turn(EN_K2, -1);
    press(B_OCTDN);
    ok = !ml_cc_of(0, P_E5) && msg_is("T1 RES CC CLEARED") && song.octave == 0 && ui.ml.on;
    press(B_OCTDN);
    ok &= msg_is("NO CC TO CLEAR") && song.octave == 0;
    memcpy(before, trk[0].p, sizeof before);
    queued(0xB0, 21, 50, 1);
    ok &= !memcmp(before, trk[0].p, sizeof before) && ml_count() == 1u;
    bad += check("  OCT- clears the picked parameter's CC (OCT- again: none), no octave", ok);

    /* review 1.5: OCT- on one parameter keeps CC16 learned elsewhere (16 = ML_N was the "no CC" mark) */
    ml_learn(16, 2, P_LEVEL);
    ml_learn(30, 0, P_E5);
    turn(EN_K2, -1);                                                   /* T1 RES picked again */
    press(B_OCTDN);                                                    /* T1 RES picked: drops CC30 only */
    ok = ml_entry(16, 2, P_LEVEL) && !ml_cc_of(0, P_E5) && msg_is("T1 RES CC CLEARED");
    press(B_OCTDN);
    ok &= ml_entry(16, 2, P_LEVEL) && msg_is("NO CC TO CLEAR");
    ml_drop(16, NTRK, P_COUNT);
    bad += check("  OCT- keeps CC16 learned on another parameter", ok);

    /* full: ML_N; a CC never learned */
    for (i = 0; i < ML_N - 1u; i++)
        ml_learn(40u + i, 2, P_ATK + i);
    turn(EN_K1, 1);
    queued(0xB0, 90, 5, 1); frame();
    ok = msg_is("LEARN FULL") && ml_count() == ML_N && !ml_cc_of(0, P_E4);
    ok &= ml_learn(64, 0, P_E4) == 2u && ml_learn(1, 0, P_E4) == 2u && ml_learn(123, 0, P_E4) == 2u;
    bad += check("  16 learned: the 17th says LEARN FULL, nothing replaced; CC1 / 64 / 123 are never learned", ok);

    /* OCT+: done; a CC is no longer taken */
    press(B_OCTUP);
    ok = !ui.ml.on && msg_is("LEARN DONE") && song.octave == 0 && !ml_arm;
    queued(0xB0, 74, 0, 1);
    ok &= trk[1].p[P_LEVEL] == TP[P_LEVEL].min;
    press(B_OCTUP);
    ok &= song.octave == 1;
    song.octave = 0;
    bad += check("OCT+: LEARN DONE (the map stays, OCT± shift the octave again)", ok);
    go_title("MIDI LEARN"); turn(EN_K1, 1);
    ok = ui.ml.on && msg_is("MIDI LEARN ON");
    turn(EN_K1, -1);
    bad += check("GLO > MIDI LEARN: KNOB 1 right = on, left = LEARN DONE", ok && !ui.ml.on && msg_is("LEARN DONE"));

    /* GLO > MIDI LEARN KNOB 2 CLEAR (DUELL 0.1; MENU > MIDI > LEARN CLEAR up to 1.5): right clears every learned CC */
    ok = !strcmp(cur_page()->title, "MIDI LEARN") && ml_count() == ML_N;
    for (i = 0; i < MI_COUNT; i++) ok &= !str_eq(MI_NAME[i], "LEARN CLEAR");     /* (MENU has no MIDI row) */
    turn(EN_K2, -3);
    ok &= ml_count() == ML_N;                                           /* (left: nothing) */
    turn(EN_K2, 1);
    ok &= !ml_count() && msg_is("LEARN CLEARED") && song.octave == 0;
    memcpy(before, trk[1].p, sizeof before);
    go_title("AMP ENV");
    queued(0xB0, 74, 3, 1);
    ok &= !memcmp(before, trk[1].p, sizeof before) && trk[0].p[P_E4] == 3;   /* (CC74: the standard map again) */
    go_title("MIDI LEARN"); turn(EN_K2, 1);
    ok &= msg_is("NOTHING LEARNED");
    bad += check("GLO > MIDI LEARN KNOB 2 right: every CC cleared (LEARN CLEARED; none: NOTHING LEARNED)", ok);

    /* the stored codes with 1.5's parameters (the INSERT 96..100, TYPE 101, ESYNC 102, P_E0 103): a common parameter
     * id + 1, below ML_E0; an engine one ML_E0 + k whatever P_E0 is; every id back as it went in */
    ok = P_E0 < ML_E0 && P_COUNT - P_E0 == 8u;
    for (i = 0; i < P_COUNT; i++)
        ok &= ml_id(ml_code(i)) == i && (i < P_E0 ? ml_code(i) == i + 1u && ml_code(i) < ML_E0 : ml_code(i) == ML_E0 + i - P_E0);
    ok &= ml_id(0) == P_COUNT;                              /* (no such parameter; between P_E0 and ML_E0 too, if there is room) */
    for (i = P_E0 + 1u; i < ML_E0; i++) ok &= ml_id(i) == P_COUNT;
    memset(favorites.factory[14], 0, 32);
    ok &= ml_learn(30, 1, P_IMIX) == 1u && ml_learn(31, 2, P_ESYNC) == 1u && ml_learn(33, 3, P_E0 + 3u) == 1u;
    ok &= ml_get(0) == (30u | 1u << 7 | (P_IMIX + 1u) << 9) && ml_get(1) == (31u | 2u << 7 | (P_ESYNC + 1u) << 9) &&
          ml_get(2) == (33u | 3u << 7 | (ML_E0 + 3u) << 9);
    queued(0xB0, 30, 0, 1);
    queued(0xB0, 31, 127, 1);
    queued(0xB0, 33, 127, 1);
    ok &= trk[1].p[P_IMIX] == 0 && trk[2].p[P_ESYNC] == 1 && trk[3].p[P_E0 + 3u] == param_desc_of(eng_idx(trk[3].eng_req), P_E0 + 3u)->max;
    bad += check("MIDI LEARN codes: common ids + 1 (INSERT MIX, ESYNC), engine ML_E0 + k; all ids round trip", ok);
    memset(favorites.factory[14], 0, 32);
    return bad;
}
/* DUELL: PLAY STOP. A short press toggles at once; a second press within 300 ms or a hold of 0.6 s is STOP ALL (seq.c stop_all): transport
 * stopped, arps and their latches gone, every voice faded out within a few blocks (not the 500 ms+ release), held-note state cleared,
 * All Sound Off (CC120) and All Notes Off (CC123) out on both parts' channels */
static uint32_t out_cc(uint32_t ch, uint32_t cc)
{
    uint32_t n = 0, i;
    for (i = mo_r; i != mo_w; i++) {
        uint32_t p = midi_out_q[i % MQ];
        n += ((p >> 8) & 0xFFu) == (0xB0u | ch) && ((p >> 16) & 0x7Fu) == cc;
    }
    return n;
}
static uint32_t voices_up(void)
{
    uint32_t n = 0, k, i;
    for (k = 0; k < NTRK; k++) for (i = 0; i < NVOICE; i++) n += trk[k].v[i].active != 0;
    return n;
}
static void sound_up(void)                              /* a held chord on each part, an arp latched on part 1, long releases */
{
    uint32_t k;
    int32_t out[CTL];
    midi_test_reset(); usb.config = 1; mo_w = mo_r = 0;
    for (k = 0; k < NPART; k++) { trk[k].p[P_REL] = 127; trk[k].p[P_SUS] = 127; }
    trk[0].p[P_AMODE] = 1; trk[0].p[P_AHOLD] = 1;
    queued(0x90, 60, 100, 1); queued(0x90, 64, 100, 1); queued(0x91, 67, 100, 1);
    queued(0x80, 60, 0, 1); queued(0x80, 64, 0, 1);       /* (the arp's chord stays latched: HOLD) */
    for (k = 0; k < 8u; k++) { track_render(&trk[0], out, CTL); track_render(&trk[1], out, CTL); }
    mo_w = mo_r = 0;
}
static int stop_all_check(const char *what)
{
    int32_t out[CTL];
    uint32_t k;
    int ok;
    events_block(CTL);
    for (k = 0; k < 4u; k++) { track_render(&trk[0], out, CTL); track_render(&trk[1], out, CTL); events_block(CTL); }   /* ~3 ms of blocks */
    ok = !song.playing && !voices_up() && !trk[0].nheld && !trk[0].arp_note && !trk[0].arp_phys && !trk[1].nheld && !midi_owners[0] && !midi_owners[1] &&
         out_cc(0, 120) == 1u && out_cc(0, 123) == 1u && out_cc(1, 120) == 1u && out_cc(1, 123) == 1u && msg_is("STOP ALL");
    return check(what, ok);
}
static int stop_all_test(void)
{
    int bad = 0, ok;
    midi_test_reset();
    press(B_PLAY); events_block(CTL);
    ok = song.playing && !msg_is("STOP ALL");
    frames(500);
    press(B_PLAY); events_block(CTL);
    bad += check("PLAY STOP: a short press starts, another (500 ms later) stops: the toggle, no STOP ALL", ok && !song.playing && !msg_is("STOP ALL"));
    midi_test_reset();
    btn_down(B_PLAY); frames(160);                        /* stopped: held, nothing starts on the press */
    ok = transport_req == 0u && !song.playing;
    events_block(CTL);
    ok = ok && !song.playing;
    btn_up(B_PLAY); frame();                              /* a short press (under 0.6 s): starts on the release */
    bad += check("  stopped: a short press does not start on the press, it starts on the release", ok && transport_req == 1u);
    events_block(CTL);
    bad += check("  .. and the transport runs", song.playing);
    frames(500);
    press(B_PLAY); events_block(CTL);
    bad += check("  running: a press stops at once", !song.playing && !msg_is("STOP ALL"));

    sound_up();
    midi_test_reset(); song.playing = 0; transport_req = 0;
    btn_down(B_PLAY); frames(700);                        /* stopped, held 0.6 s: STOP ALL, nothing ever starts */
    ok = transport_req == 4u || msg_is("STOP ALL");
    events_block(CTL);
    btn_up(B_PLAY); frame(); events_block(CTL);
    bad += check("  stopped, held 0.6 s: STOP ALL, the transport never started, the release starts nothing", ok && !song.playing && transport_req == 0u);

    sound_up();
    ok = voices_up() >= 1u && trk[0].nheld;
    song.playing = 1;
    press(B_PLAY); events_block(CTL);                     /* first press: stop (toggle) */
    press(B_PLAY);                                        /* second within 300 ms: STOP ALL */
    bad += check("PLAY STOP double press: both parts sounding, an arp latched, then", ok && transport_req == 4u);
    bad += stop_all_check("  double press: transport stopped, voices dead in 4 blocks, arp latch gone, CC120 + CC123 on channels 1 and 2, STOP ALL shown");

    sound_up();
    btn_down(B_PLAY); frames(700); btn_up(B_PLAY); frame();
    bad += stop_all_check("PLAY STOP held 0.6 s: the same, once (the release does nothing more)");
    ok = transport_req == 0u && !song.playing;
    press(B_PLAY); events_block(CTL);
    bad += check("  and the next short press starts again", ok && song.playing);
    midi_test_reset(); song.playing = 0; transport_req = 0;
    sound_up();
    press(B_PLAY);                                        /* stopped: the first tap (starts on its release) .. */
    ok = transport_req == 1u;
    press(B_PLAY);                                        /* .. and the second within 300 ms: STOP ALL */
    bad += check("  stopped, double press: the first tap starts on release, the second is STOP ALL", ok && transport_req == 4u);
    bad += stop_all_check("  .. and everything is stopped");
    return bad;
}
int main(void)
{
    int bad = controls_test() + sustain_test() + ownership_test() + clock_test(1) + clock_test(2) + clock_arp_and_boundaries() +
              arp_ext_stop_test() + arp_ext_clock_test() + usb_burst_test() + drain_test() + route_test() + cc_map_test() + learn_test() + stop_all_test();
    printf("%s\n", bad ? "MIDI CONTROL/CLOCK TEST FAILED" : "MIDI control/clock integration tests passed"); return bad != 0;
}
