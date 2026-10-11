/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise the real keyboard, recording, arp and MIDI-out paths on the host.
 * Build with the same generated headers and flags as hostsim.c. */
#include <assert.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

static const uint8_t WHITE_KEYS[] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const struct { uint8_t count, notes[12]; } EXPECTED[] = {
    {12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    {7, {0, 2, 4, 5, 7, 9, 11}}, {7, {0, 2, 3, 5, 7, 8, 10}},
    {7, {0, 2, 3, 5, 7, 9, 10}}, {7, {0, 2, 4, 5, 7, 9, 10}},
    {5, {0, 2, 4, 7, 9}}, {5, {0, 3, 5, 7, 10}},
    {7, {0, 2, 3, 5, 7, 8, 11}}, {7, {0, 1, 3, 5, 7, 8, 10}},
    {7, {0, 2, 4, 6, 7, 9, 11}}, {7, {0, 1, 3, 5, 6, 8, 10}},
    {7, {0, 2, 3, 5, 7, 9, 11}}, {6, {0, 3, 5, 6, 7, 10}},
    {6, {0, 2, 4, 6, 8, 10}}, {8, {0, 1, 3, 4, 6, 7, 9, 10}},
    {8, {0, 2, 3, 5, 6, 8, 9, 11}},
};

static void mapping_test(void)
{
    track_t *t = &trk[0];
    uint32_t s, k, w;
    int root, oct, trans;
    assert(TP[P_SCALE].max + 1 == sizeof EXPECTED / sizeof EXPECTED[0]);
    assert(sizeof SCALE_MASK / sizeof SCALE_MASK[0] == sizeof EXPECTED / sizeof EXPECTED[0]);
    t->p[P_QUANT] = 2;
    for (s = 0; s <= (uint32_t)TP[P_SCALE].max; s++) {
        uint32_t mask = 0;
        t->p[P_SCALE] = (int16_t)s;
        for (k = 0; k < EXPECTED[s].count; k++)
            mask |= 1u << EXPECTED[s].notes[k];
        assert(scale_mask(t) == mask);
        for (root = 0; root < 12; root++)
            for (oct = -3; oct <= 3; oct++)
                for (trans = -24; trans <= 24; trans++) {
                    t->p[P_ROOT] = (int16_t)root;
                    song.octave = (int8_t)oct;
                    t->p[P_TRANS] = (int16_t)trans;
                    for (k = w = 0; k < 27u; k++) {
                        uint32_t actual = kb_map(t, k);
                        if (k == WHITE_KEYS[w]) {
                            /* Four white keys precede C4; use a positive cycle
                             * offset to independently handle the lower degrees. */
                            int d = (int)w - 4 + 12 * EXPECTED[s].count;
                            int want = 60 + root + 12 * (oct + d / EXPECTED[s].count - 12)
                                + EXPECTED[s].notes[d % EXPECTED[s].count] + trans;
                            assert(actual == (uint32_t)clamp(want, 0, 127));
                            w++;
                        } else {
                            assert(actual == KB_SILENT);
                        }
                    }
                }
    }
    t->p[P_QUANT] = 0;
    song.octave = 0;
    t->p[P_TRANS] = -5;
    for (k = 0; k < 27u; k++)
        assert(kb_map(t, k) == 48u + k);
    t->p[P_QUANT] = 2;
    host_preset(t, ENGI_DRUM, 0);              /* DRUM (the GM map, any part; SAMPLE PERC until 1.0.2): the first */
    t->engine = t->eng_req = ENGI_DRUM;        /* C is the kick */
    for (k = 0; k < 27u; k++)
        assert(kb_map(t, k) == 29u + k);
    t->engine = t->eng_req = 0;                /* SNAP (QNT 1, the old ON): every key, rounded down */
    t->p[P_QUANT] = 1;
    t->p[P_SCALE] = 2;                         /* C minor */
    t->p[P_ROOT] = 0;
    t->p[P_TRANS] = 0;
    song.octave = 0;
    assert(kb_map(t, 11) == 63u && kb_map(t, 10) == 63u && kb_map(t, 7) == 60u && kb_map(t, 8) == 60u);
    puts("scales: all 16 scales, 12 roots, octave/transpose ranges, bypass, drums and SNAP ok");
}

static void key_events_test(void)
{
    track_t *t = &trk[0];
    uint32_t before;
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    kb_prev = 0;
    usb.config = 1;
    mo_w = mo_r = 0;
    t->p[P_QUANT] = 2;
    t->p[P_SCALE] = 2;                      /* C minor: E key plays Eb */
    t->p[P_AMODE] = 1;
    song.playing = song.rec = 1;
    fm1_in.notes = 1u << 8;                /* C# is silent */
    keyboard_block();
    assert(t->arp_phys == 0 && t->nheld == 0 && t->step[0].n == 0 && mo_w == 0);
    t->p[P_QUANT] = 0;                    /* releasing a muted key stays silent */
    fm1_in.notes = 0;
    keyboard_block();
    assert(mo_w == 0);
    t->p[P_QUANT] = 2;
    fm1_in.notes = (1u << 11) | (1u << 10); /* E and D#: only Eb sounds/records */
    keyboard_block();
    assert(t->arp_phys == 1 && t->nheld == 1 && t->held[0] == 63);
    assert(t->step[0].n == 0);             /* ARP on: the arp's notes are recorded, not the keys */
    arp_tick(t, 1);
    assert(t->step[0].n == 1 && t->step[0].note[0] == 63);
    assert(mo_w == 1 && ((midi_out_q[0] >> 16) & 127u) == 63);
    t->p[P_SCALE] = 9;
    t->p[P_ROOT] = 6;
    t->p[P_TRANS] = 12;
    song.octave = 1;
    song.sel = 1;                          /* key-up follows the original note/part */
    fm1_in.notes = 0;
    keyboard_block();
    assert(t->arp_phys == 0 && t->nheld == 0);
    assert(mo_w == 2 && ((midi_out_q[1] >> 16) & 127u) == 63);
    assert(((midi_out_q[1] >> 8) & 255u) == 0x80u);
    song.sel = 0;
    t->p[P_QUANT] = 0;
    fm1_in.notes = 1u << 8;                /* held black key must release after enabling mode */
    keyboard_block();
    before = mo_w;
    assert(t->arp_phys == 1);
    t->p[P_QUANT] = 2;
    fm1_in.notes = 0;
    keyboard_block();
    assert(t->arp_phys == 0 && t->nheld == 0 && mo_w == before + 1);
    puts("scales: silent keys, arp, live recording, MIDI out and held-note changes ok");
}

/* the voices of t holding a note (gate on), and whether note n is one of them */
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

/* a voice of t held (gate on) by a note the sequencer does not hold: a note-off that will never come */
static int stray(const track_t *t)
{
    uint32_t i, k;
    for (i = 0; i < NVOICE; i++) {
        if (!t->v[i].gate)
            continue;
        for (k = 0; k < t->seq_n && t->seq_notes[k] != t->v[i].note; k++)
            ;
        if (k == t->seq_n)
            return 1;
    }
    return 0;
}

/* QNT SEQ (#37): the sequencer's notes snap to the scale as they play, the steps stay as written; a scale
 * changed while notes ring ends exactly the notes that sounded (no stuck notes); drum kits never snap */
static void seq_quant_test(void)
{
    track_t *t = &trk[0];
    uint32_t period, i;
    static const step_t STEPS[] = {
        {.note = {61, 64}, .n = 2, .time = ST_NOTE},  /* C#4 E4: C major -> C4 E4 */
        {.note = {66}, .n = 1, .time = ST_NOTE},      /* F#4 -> F4 */
        {.note = {61}, .n = 1, .time = ST_NOTE},      /* held through two TIEs */
        {.time = ST_TIE},
        {.time = ST_TIE},
        {.note = {61, 60}, .n = 2, .time = ST_NOTE},  /* both snap to C4: one note */
        {.time = ST_REST},
    };
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    song.g[G_BPM] = 120;
    host_preset(t, 0, 0);                          /* ANALOG, POLY */
    t->engine = t->eng_req = 0;
    t->p[P_VOICE] = V_POLY;
    t->p[P_SLEN] = (int16_t)NELEM(STEPS);
    for (i = 0; i < NELEM(STEPS); i++)
        t->step[i] = STEPS[i];
    t->seq_active = 1;
    t->p[P_SCALE] = 1;                             /* C major */
    t->p[P_ROOT] = 0;
    period = div_samples((uint32_t)t->p[P_SDIV]);

    t->p[P_QUANT] = 0;                             /* OFF and SNAP: the sequence plays as written */
    seq_start();
    events_block(CTL);
    assert(t->seq_n == 2 && t->seq_notes[0] == 61 && t->seq_notes[1] == 64 && gated_note(t, 61));
    seq_stop();
    t->p[P_QUANT] = 1;
    seq_start();
    events_block(CTL);
    assert(t->seq_n == 2 && t->seq_notes[0] == 61 && gated_note(t, 61));
    seq_stop();
    assert(gated(t) == 0);

    t->p[P_QUANT] = 3;                             /* SEQ */
    seq_start();
    events_block(CTL);
    assert(t->seq_n == 2 && t->seq_notes[0] == 60 && t->seq_notes[1] == 64 && gated_note(t, 60) &&
           gated_note(t, 64) && !gated_note(t, 61) && gated(t) == 2);
    assert(t->step[0].note[0] == 61 && t->step[0].note[1] == 64);   /* stored as written */
    t->p[P_SCALE] = 2;                             /* C minor while C4 E4 ring: E4 would be Eb4 now */
    events_block(period);                          /* the gate ends them, step 2 plays F#4 -> F4 */
    assert(!gated_note(t, 60) && !gated_note(t, 64) && !gated_note(t, 63));
    assert(t->seq_idx == 1 && t->seq_n == 1 && t->seq_notes[0] == 65 && gated(t) == 1);
    events_block(period);                          /* step 3: C#4 -> C4, held into the TIEs */
    assert(t->seq_idx == 2 && t->seq_n == 1 && t->seq_notes[0] == 60 && gated_note(t, 60) && gated(t) == 1);
    t->p[P_SCALE] = 0;                             /* CHR while it is held: C#4 itself now, but C4 sounds */
    events_block(period);
    t->p[P_ROOT] = 1;                              /* and a new root */
    t->p[P_SCALE] = 1;
    events_block(period);
    assert(t->seq_idx == 4 && gated_note(t, 60) && gated(t) == 1);
    t->p[P_ROOT] = 0;
    events_block(period);                          /* step 6: C#4 and C4 both C4 now: one note, C4 retriggered */
    assert(t->seq_idx == 5 && t->seq_n == 1 && t->seq_notes[0] == 60 && gated(t) == 1);
    t->p[P_SCALE] = 5;                             /* PEN while it rings */
    events_block(period);                          /* REST: released */
    assert(t->seq_idx == 6 && t->seq_n == 0 && gated(t) == 0);
    events_block(period);                          /* round again, then stop with notes sounding */
    assert(t->seq_idx == 0 && gated(t) == 2 && gated_note(t, 60) && gated_note(t, 64));
    t->p[P_SCALE] = 2;
    seq_stop();
    assert(gated(t) == 0);
    for (i = 0; i < NELEM(STEPS); i++)
        assert(!memcmp(&t->step[i], &STEPS[i], sizeof STEPS[i]));

    /* a scale change on every block for a while, slides and TIEs included: nothing left held */
    t->step[1].flags = SF_SLIDE;
    seq_start();
    for (i = 0; i < 4000u; i++) {
        t->p[P_SCALE] = (int16_t)(i * 7u % 16u);
        t->p[P_ROOT] = (int16_t)(i * 5u % 12u);
        events_block(CTL);
        assert(!stray(t));
    }
    seq_stop();
    assert(gated(t) == 0 && t->seq_n == 0);
    t->step[1].flags = 0;

    /* drum kits and the DRUM engine never snap: their notes are GM drums */
    host_preset(t, ENGI_DRUM, 0);
    t->engine = t->eng_req = ENGI_DRUM;
    t->p[P_SCALE] = 1;
    t->p[P_ROOT] = 0;
    t->step[0] = (step_t){.note = {37, 39}, .n = 2, .hit = 1u << 0, .time = ST_NOTE};
    seq_start();
    events_block(CTL);
    assert(t->seq_n == 3 && t->seq_notes[0] == 37 && t->seq_notes[1] == 39 && t->seq_notes[2] == DRUM_LANE_NOTE[0]);
    seq_stop();
    assert(gated(t) == 0);
    puts("scales: QNT SEQ snaps the sequence as it plays (steps unchanged), no stuck notes over scale changes, kits never");
}

/* #191: the arp's notes from the keys follow TRN, ROOT, SCALE and QNT as they change (latched with HOLD or held),
 * the octave they were pressed in; MIDI IN's notes play as they came; with nothing changed, exactly as before */
static uint32_t arp_seen[8];
static uint32_t arp_run(track_t *t, uint32_t steps)  /* the notes of `steps` arp steps into arp_seen; the count sounding */
{
    uint32_t period = div_samples((uint32_t)t->p[P_ARATE]), i;
    for (i = 0; i < steps && i < 8u; i++) {
        arp_tick(t, period);
        arp_seen[i] = t->arp_note;
    }
    return steps;
}
static int arp_is(track_t *t, uint32_t a, uint32_t b)   /* UP over two notes: a b a b */
{
    arp_run(t, 4);
    return arp_seen[0] == a && arp_seen[1] == b && arp_seen[2] == a && arp_seen[3] == b;
}
static void arp_reset(track_t *t)
{
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    kb_prev = 0;
    fm1_in.notes = 0;
    host_preset(t, 0, 0);                          /* ANALOG, POLY */
    t->engine = t->eng_req = 0;
    t->p[P_VOICE] = V_POLY;
    t->p[P_AMODE] = 1;                             /* UP */
    t->p[P_AOCT] = 1;
    t->p[P_SCALE] = 2;                             /* C minor */
    t->p[P_ROOT] = 0;
    t->p[P_TRANS] = 0;
}

static void arp_follow_test(void)
{
    track_t *t = &trk[0];
    uint32_t i;

    /* the report: QNT SNAP, HOLD, a chord latched, then TRN */
    arp_reset(t);
    t->p[P_QUANT] = QN_SNAP;
    t->p[P_AHOLD] = 1;
    events_block(CTL);                             /* (the ISR sees HOLD on) */
    fm1_in.notes = (1u << 7) | (1u << 11);         /* C4, E4 -> Eb4 in C minor */
    keyboard_block();
    fm1_in.notes = 0;
    keyboard_block();
    assert(t->nheld == 2 && t->held[0] == 60 && t->held[1] == 63);
    assert(arp_is(t, 60, 63));
    t->p[P_TRANS] = 2;                             /* D4, F#4 -> F4: transposed, then onto the scale */
    assert(arp_is(t, 62, 65));
    t->p[P_TRANS] = 1;                             /* C#4 -> C4, F4 */
    assert(arp_is(t, 60, 65));
    t->p[P_TRANS] = 0;
    t->p[P_SCALE] = 1;                             /* SCALE: C major, E4 itself now */
    assert(arp_is(t, 60, 64));
    t->p[P_ROOT] = 2;                              /* ROOT: D major, C4 -> B3, E4 */
    assert(arp_is(t, 59, 64));
    t->p[P_ROOT] = 0;
    t->p[P_SCALE] = 2;                             /* back as it was: exactly the notes latched */
    assert(arp_is(t, 60, 63));
    song.octave = 1;                               /* the octave buttons: the next keys, not the latched ones */
    assert(arp_is(t, 60, 63));
    song.octave = 0;
    assert(t->nheld == 2 && t->held[0] == 60 && t->held[1] == 63);   /* (what the keys started: their releases) */
    t->p[P_AHOLD] = 0;                             /* HOLD off: the latched chord goes, nothing left sounding */
    events_block(CTL);
    arp_run(t, 2);
    assert(t->nheld == 0 && t->arp_note == 0 && gated(t) == 0);

    /* QNT OFF, no HOLD: the keys held follow TRN too; their release ends them, whatever TRN is now */
    arp_reset(t);
    t->p[P_QUANT] = QN_OFF;
    fm1_in.notes = (1u << 7) | (1u << 11);         /* C4, E4 */
    keyboard_block();
    assert(arp_is(t, 60, 64));
    t->p[P_TRANS] = -3;
    assert(arp_is(t, 57, 61));
    fm1_in.notes = 1u << 7;                        /* E4 up (it plays C#4 now) */
    keyboard_block();
    assert(t->nheld == 1);
    arp_run(t, 2);
    assert(arp_seen[0] == 57 && arp_seen[1] == 57);
    fm1_in.notes = 0;
    keyboard_block();
    arp_run(t, 2);
    assert(t->nheld == 0 && t->arp_note == 0 && gated(t) == 0);

    /* MIDI IN into the arp: its notes are not the keyboard's, TRN never moved them (nor does it now) */
    arp_reset(t);
    t->p[P_QUANT] = QN_SNAP;
    t->p[P_AHOLD] = 1;
    input_on(t, 61, 100);
    input_on(t, 66, 100);
    input_off(t, 61);
    input_off(t, 66);
    assert(arp_is(t, 61, 66));
    t->p[P_TRANS] = 5;
    t->p[P_SCALE] = 1;
    assert(arp_is(t, 61, 66));

    /* CHRD: a key's chord moves with it (its notes as far above the key's note) */
    arp_reset(t);
    t->p[P_QUANT] = QN_OFF;
    t->p[P_AHOLD] = 1;
    t->p[P_CHRD] = CH_POW;                         /* C G C' */
    fm1_in.notes = 1u << 7;
    keyboard_block();
    fm1_in.notes = 0;
    keyboard_block();
    assert(t->nheld == 3);
    arp_run(t, 3);
    assert(arp_seen[0] == 60 && arp_seen[1] == 67 && arp_seen[2] == 72);
    t->p[P_TRANS] = 2;
    arp_run(t, 3);
    assert(arp_seen[0] == 62 && arp_seen[1] == 69 && arp_seen[2] == 74);
    t->p[P_CHRD] = 0;

    /* QNT WHITE after the keys: a black key latched is silent now (left out); all of them: the arp rests */
    arp_reset(t);
    t->p[P_QUANT] = QN_OFF;
    t->p[P_AHOLD] = 1;
    fm1_in.notes = (1u << 7) | (1u << 8);          /* C4, C#4 */
    keyboard_block();
    fm1_in.notes = 0;
    keyboard_block();
    assert(arp_is(t, 60, 61));
    t->p[P_QUANT] = QN_WHITE;                      /* C4 is the root, C#4 silent */
    arp_run(t, 4);
    for (i = 0; i < 4u; i++)
        assert(arp_seen[i] == 60);
    t->p[P_QUANT] = QN_OFF;
    fm1_in.notes = 1u << 8;                        /* a new chord: C#4 alone */
    keyboard_block();
    fm1_in.notes = 0;
    keyboard_block();
    t->p[P_QUANT] = QN_WHITE;
    arp_run(t, 4);
    assert(t->nheld == 1 && t->arp_note == 0 && gated(t) == 0);
    t->p[P_QUANT] = QN_OFF;
    arp_run(t, 1);
    assert(arp_seen[0] == 61);

    /* the DRUM engine's own key map: as it came (TRN is not its) */
    arp_reset(t);
    host_preset(t, ENGI_DRUM, 0);
    t->engine = t->eng_req = ENGI_DRUM;
    t->p[P_AMODE] = 1;
    t->p[P_AOCT] = 1;
    t->p[P_AHOLD] = 1;
    fm1_in.notes = (1u << 7) | (1u << 9);
    keyboard_block();
    fm1_in.notes = 0;
    keyboard_block();
    assert(held_key[0][0] == 0 && held_key[0][1] == 0);
    assert(arp_is(t, kb_map(t, 7), kb_map(t, 9)));
    t->p[P_TRANS] = 7;
    assert(arp_is(t, t->held[0], t->held[1]));
    puts("scales: the arp's latched / held keys follow TRN, ROOT, SCALE and QNT (#191); octave, MIDI IN, kits as before");
}

int main(void)
{
    host_tracks_init();
    mapping_test();
    key_events_test();
    seq_quant_test();
    arp_follow_test();
    return 0;
}
