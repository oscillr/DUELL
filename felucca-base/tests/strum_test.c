/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.1 item 144: STRUM (the CHORD page's K3, ext slot X27; chord.c strum_acts / strum_blocks, seq.c key_on / strum_block, midi_control.c midi_play)
 * and the NOTE-START STAGGER (voice.c trk_note_on_1 / vs_block / vs_cancel: at most VS_MAX new voices a POLY part starts in one block), through the real
 * keyboard, MIDI IN, arp, HOLD and voice code, one events_block (seq.c) a block as the audio interrupt runs it:
 *   STRUM: the tones start low to high exactly STRUM apart (in blocks of CTL samples), the lowest with the key; 0 = OFF plays as before; the range and
 *          its blocks; MIDI OUT follows the tones; a release before a tone's turn cancels it (keys and MIDI, LAYER's part 2 too), nothing stuck; a MIDI
 *          sustain pedal holds the strum on; HOLD ALL latches what started; ARP on / MONO / LEGATO / UNISON / CHRD OFF: no strum and the card hidden;
 *          LAYER: each part its own STRUM; a panic / STOP ALL / MODE change forgets the waiting tones; the card says OFF at 0, the MIDI map can reach it.
 *   STAGGER: a single key starts in its own block; a pair at once; a third tone (a triad's top) one block late; a fourth one block late too; a re-strike of sounding notes at once; 8 MIDI notes in one block start 2 + 2 + 2 + 2 on either
 *          part (both parts at once too); a note-off of a held-back note cancels it; UNISON and MONO are never held back; STOP ALL empties the queue;
 *          stealing at more notes than voices leaves nothing stuck; LAYER's XFADE share rides with a held-back note.
 *   A random walk of presses, releases, MIDI notes, the pedal, HOLD, ARP, voice modes, STRUM values and MODE changes, then everything let go: no voice
 *          left with its gate open, no tone waiting, the queues empty.
 * host_tests.sh "strum". */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#define K_C4 7u                                   /* keys from F3 (0): C4 7, D4 9, E4 11, G4 14 */
#define K_E4 11u
#define K_G4 14u

static int gate_note(const track_t *t, uint32_t note)
{
    for (uint32_t i = 0; i < NVOICE; i++) if (t->v[i].active && t->v[i].gate && t->v[i].note == note) return 1;
    return 0;
}
static uint32_t ngated(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].gate;
    return n;
}
static void blk(uint32_t n) { while (n--) events_block(CTL); }
static void silence(void)                         /* every voice faded out (a ringing note played again is a re-strike: never held back) */
{
    uint32_t p, i;
    for (p = 0; p < NTRK; p++)
        for (i = 0; i < NVOICE; i++)
            if (trk[p].v[i].active)
                voice_kill(&trk[p].v[i]);
}
static void kpress(uint32_t k) { fm1_in.notes |= 1u << k; host_notes |= 1u << k; events_block(CTL); }
static void krelease(uint32_t k) { fm1_in.notes &= ~(1u << k); host_notes &= ~(1u << k); events_block(CTL); }
static void mq(uint32_t st, uint32_t d1, uint32_t d2) { midi_enqueue(st >> 4 | st << 8 | d1 << 16 | d2 << 24, 1); }
static void midi(uint32_t st, uint32_t d1, uint32_t d2) { mq(st, d1, d2); events_block(CTL); }
static uint32_t mo_ons(uint32_t from, uint32_t *notes)   /* note-ons in MIDI OUT since from, their notes in order */
{
    uint32_t n = 0, i;
    for (i = from; i != mo_w; i++) {
        uint32_t e = midi_out_q[i % NELEM(midi_out_q)];
        if ((e & 0xFFu) == 0x09u && ((e >> 24) & 0x7Fu))
            notes[n++ % 16u] = (e >> 16) & 0x7Fu;
    }
    return n;
}
static uint32_t mo_offs(uint32_t from)
{
    uint32_t n = 0, i;
    for (i = from; i != mo_w; i++) {
        uint32_t e = midi_out_q[i % NELEM(midi_out_q)];
        n += (e & 0xFFu) == 0x08u || ((e & 0xFFu) == 0x09u && !((e >> 24) & 0x7Fu));
    }
    return n;
}
static int quiet(void)                            /* nothing sounding with its gate open, nothing waiting */
{
    uint32_t p, k, f, w = 0;
    for (p = 0; p < NTRK; p++)
        w |= ngated(&trk[p]) | vs_qn[p];
    for (k = 0; k < 27u; k++)
        w |= kb_sw[k] | kb_sw2[k] | kb_chn[k] | kb_chn2[k];
    for (f = 0; f < MCHORD_N; f++)
        w |= mchord[f].id;
    return !w;
}
static void reset(void)
{
    uint32_t p;
    ui_power_on();
    fm1_in.notes = kb_prev = 0; host_notes = 0; mo_w = mo_r = 0; usb.config = 1;
    for (p = 0; p < NTRK; p++) {
        track_t *t = &trk[p];
        if (ENGINES[t->eng_req % NENGINES] != &ENG_ANALOG) {   /* (both parts on ANALOG: 8 voices; the power-on FM6 of part 2 has 6) */
            uint32_t e;
            for (e = 0; e < NENGINES && ENGINES[e] != &ENG_ANALOG; e++)
                ;
            set_engine_of(t, e);
            apply_preset_to(t, 0);
        }
        t->p[P_TRANS] = 0; t->p[P_VOICE] = V_POLY; t->p[P_CHRD] = CH_OFF; t->p[P_VOIC] = VC_CLOSE; t->p[P_AMODE] = 0; t->p[P_AHOLD] = 0;
        t->p[P_SUS] = 127; t->p[P_MUTE] = 0;
        XV(t, X_STRUM) = 0;
    }
    scale_g = 0; root_g = 0; offscale_g = OS_PLAY; song.octave = 0;
    blk(8);                                        /* (the power-on sounds' engine switch) */
    for (p = 0; p < NTRK; p++)
        XV(&trk[p], X_STRUM) = 0, trk[p].p[P_CHRD] = CH_OFF, trk[p].p[P_AMODE] = 0, trk[p].p[P_VOICE] = V_POLY;   /* (a preset's own keys / arp, if any) */
}
/* the block (0 = the press's) each note of want[] first sounds, within `blocks` blocks after a press of key k / a MIDI note-on (k >= 27: note k - 27 on part p's channel) */
static void when(track_t *t, uint32_t k, const uint8_t *want, uint32_t n, uint32_t blocks, int32_t *at)
{
    uint32_t b, i;
    for (i = 0; i < n; i++) at[i] = -1;
    if (k < 27u) kpress(k); else midi(0x90u | trk_midi_ch(trk_index(t)), k - 27u, 100);
    for (b = 0; b <= blocks; b++) {
        for (i = 0; i < n; i++)
            if (at[i] < 0 && gate_note(t, want[i])) at[i] = (int32_t)b;
        events_block(CTL);
    }
}

static int strum(void)
{
    int bad = 0, ok;
    track_t *t = &trk[0];
    static const uint8_t CM7[4] = {60, 64, 67, 71};
    int32_t at[4];
    uint32_t m0, nn[16], i, sb;
    reset();
    /* the range, the steps, the blocks */
    bad += check("STRUM: slot X27, F_INT 0..100 ms, default 0 (OFF), on the CHORD page as K3, learnable like CHRD / VOIC",
                 X_STRUM == 27 && str_eq(XTP[X_STRUM].label, "STRUM") && XTP[X_STRUM].fmt == F_INT && XTP[X_STRUM].min == 0 && XTP[X_STRUM].max == 100 &&
                 XTP[X_STRUM].def == 0 && str_eq(XTP[X_STRUM].unit, "ms") && xb_def(X_STRUM) == 64u && map_code_ok(XW_BASE + X_STRUM) && map_code_ok(P_CHRD));
    {
        const page_t *pg = 0;
        for (i = 0; i < NELEM(PAGES); i++) if (str_eq(PAGES[i].title, "CHORD")) pg = &PAGES[i];
        bad += check("  the CHORD page: K1 CHRD, K2 VOIC, K3 STRUM, K4 none", pg && pg->id[0] == P_CHRD && pg->id[1] == P_VOIC && pg->id[2] == XW_BASE + X_STRUM && pg->id[3] == 0xFF);
    }
    t->p[P_CHRD] = CH_MAJ7;
    midi_xv_set(t, X_STRUM, 127); ok = XV(t, X_STRUM) == 100;
    midi_xv_set(t, X_STRUM, 64); ok &= XV(t, X_STRUM) == 50;
    midi_xv_set(t, X_STRUM, 0); ok &= XV(t, X_STRUM) == 0;
    {
        map_info_t mi;
        song.sel = 0;
        map_target_info(XW_BASE + X_STRUM, t, &mi);
        ok &= mi.kind == MAP_K_CONTINUOUS && mi.saved == MAP_S_PATCH && !mi.inert;
        t->p[P_AMODE] = 1; map_target_info(XW_BASE + X_STRUM, t, &mi); ok &= mi.inert; t->p[P_AMODE] = 0;
    }
    bad += check("  a CC on STRUM: 0..127 over 0..100 ms (127 -> 100, 64 -> 50); the map says continuous, saved with the patch, inert while the arp plays", ok);
    XV(t, X_STRUM) = 1; ok = strum_blocks(t) == 1u;
    XV(t, X_STRUM) = 10; ok &= strum_blocks(t) == 14u;      /* 10 ms x 44100 / 32 = 13.8 */
    XV(t, X_STRUM) = 100; ok &= strum_blocks(t) == 138u;
    XV(t, X_STRUM) = 0; ok &= strum_blocks(t) == 0u;
    XV(t, X_STRUM) = 127; ok &= strum_blocks(t) == 138u;    /* (a stored value past the range: 100 ms) */
    XV(t, X_STRUM) = -3; ok &= strum_blocks(t) == 0u;
    bad += check("  blocks between tones: 1 ms 1, 10 ms 14, 100 ms 138 (0.725 ms a block); 0 and below OFF; above 100 as 100", ok);
    /* timing */
    XV(t, X_STRUM) = 10; sb = strum_blocks(t);
    m0 = mo_w;
    when(t, K_C4, CM7, 4, 4 * sb, at);
    bad += check("CMAJ7, STRUM 10 ms: C at the press, E G B 14, 28, 42 blocks later (10.2 ms apart), low to high",
                 at[0] == 0 && at[1] == (int32_t)sb && at[2] == 2 * (int32_t)sb && at[3] == 3 * (int32_t)sb);
    ok = mo_ons(m0, nn) == 4u && nn[0] == 60 && nn[1] == 64 && nn[2] == 67 && nn[3] == 71;
    bad += check("  MIDI OUT: the four note-ons in that order (each as its tone starts)", ok);
    m0 = mo_w;
    krelease(K_C4);
    bad += check("  release: all four end, four MIDI note-offs, nothing waits", !ngated(t) && mo_offs(m0) == 4u && quiet());
    XV(t, X_STRUM) = 0;
    silence();
    when(t, K_C4, CM7, 4, 4, at);
    bad += check("STRUM OFF: as before (from silence the third and fourth tone one block late: the voices' start limit of 2 a block, below)", at[0] == 0 && at[1] == 0 && at[2] == 1 && at[3] == 1);
    krelease(K_C4);
    /* cancel */
    XV(t, X_STRUM) = 20; sb = strum_blocks(t);
    m0 = mo_w;
    kpress(K_C4);
    blk(sb + 1u);
    ok = ngated(t) == 2u && gate_note(t, 60) && gate_note(t, 64);
    krelease(K_C4);
    blk(4u * sb);
    bad += check("released after two tones: those two end, the other two never start; MIDI OUT 2 on / 2 off", ok && !ngated(t) && quiet() && mo_ons(m0, nn) == 2u && mo_offs(m0) == 2u);
    kpress(K_C4); krelease(K_C4); blk(4u * sb);
    bad += check("  a tap shorter than one step: the lowest tone only, ended", !ngated(t) && quiet());
    /* STRUM changed while it runs, two keys strumming at once */
    XV(t, X_STRUM) = 30; sb = strum_blocks(t);
    kpress(K_C4); blk(2);
    XV(t, X_STRUM) = 0; blk(sb);
    bad += check("STRUM turned to OFF while a chord waits: the rest start at once at the next step", ngated(t) == 4u);
    krelease(K_C4);
    XV(t, X_STRUM) = 5; sb = strum_blocks(t);
    t->p[P_CHRD] = CH_MAJ;
    kpress(K_C4); kpress(K_G4);                     /* C E G and G B D: G shared */
    blk(3u * sb);
    ok = gate_note(t, 60) && gate_note(t, 64) && gate_note(t, 67) && gate_note(t, 71) && gate_note(t, 74) && ngated(t) == 5u;
    krelease(K_C4);
    ok &= !gate_note(t, 60) && !gate_note(t, 64) && gate_note(t, 67) && gate_note(t, 71);
    krelease(K_G4); blk(2);
    bad += check("two strummed chords sharing G: G sounds once, the C chord's release leaves G B D, then all end", ok && quiet());
    return bad;
}

static int strum_midi_hold(void)
{
    int bad = 0, ok;
    track_t *t = &trk[0], *t2 = &trk[1];
    static const uint8_t CMAJ[3] = {60, 64, 67};
    int32_t at[4];
    uint32_t sb, f, n;
    reset();
    t->p[P_CHRD] = CH_MAJ; XV(t, X_STRUM) = 10; sb = strum_blocks(t);
    when(t, 27u + 60u, CMAJ, 3, 3 * sb, at);
    bad += check("MIDI IN note C: its chord strummed as a key's (0, 14, 28 blocks)", at[0] == 0 && at[1] == (int32_t)sb && at[2] == 2 * (int32_t)sb);
    midi(0x80, 60, 0);
    bad += check("  its note-off ends the three, the chord table empty", !ngated(t) && quiet());
    midi(0x90, 60, 100); blk(sb);
    ok = ngated(t) == 2u;
    midi(0x80, 60, 0); blk(3u * sb);
    bad += check("  a note-off after two tones: the third never starts, nothing stuck", ok && !ngated(t) && quiet());
    midi(0xB0, 64, 127);                           /* the sustain pedal */
    midi(0x90, 60, 100); midi(0x80, 60, 0);
    blk(3u * sb);
    ok = ngated(t) == 3u;
    midi(0xB0, 64, 0); blk(2);
    bad += check("  sustain pedal down: the note let go keeps strumming to its third tone; pedal up ends all", ok && !ngated(t) && quiet());
    /* HOLD ALL, the arp off: the keys latch */
    t->p[P_AHOLD] = 2;
    kpress(K_C4); blk(3u * sb); krelease(K_C4); blk(4);
    ok = ngated(t) == 3u;
    kpress(K_E4);
    ok &= !gate_note(t, 60) && !gate_note(t, 67) && gate_note(t, 64);
    blk(3u * sb); krelease(K_E4); blk(4);
    ok &= ngated(t) == 3u && gate_note(t, 64) && gate_note(t, 68) && gate_note(t, 71);
    bad += check("HOLD ALL: a strummed chord latches; the next key's chord replaces it (strummed too)", ok);
    t->p[P_AHOLD] = 0; blk(2);
    bad += check("  HOLD off: the latched chord ends", !ngated(t) && quiet());
    t->p[P_AHOLD] = 2;
    kpress(K_C4); blk(sb / 2u); krelease(K_C4); blk(3u * sb);
    ok = ngated(t) == 1u && gate_note(t, 60);
    t->p[P_AHOLD] = 0; blk(2);
    bad += check("  HOLD ALL, released before the second tone: the root alone latches, ends with HOLD off", ok && !ngated(t) && quiet());
    /* arp, mono: no strum, the card hidden */
    song.sel = 0;
    ok = !card_hidden_vp(&XV(t, X_STRUM));
    t->p[P_AMODE] = 1;                             /* UP */
    ok &= card_hidden_vp(&XV(t, X_STRUM)) && !strum_blocks(t);
    kpress(K_C4);
    ok &= t->nheld == 3u && !kb_sw[K_C4];
    krelease(K_C4); blk(2);
    t->p[P_AMODE] = 0;
    bad += check("ARP on: STRUM does nothing (the arp holds the three at once), its card hidden", ok && quiet());
    mods_off = MODS_ARP;
    bad += check("  MODULES ARP OFF: the keys play the chord again, STRUM acts and shows", (t->p[P_AMODE] = 1, !card_hidden_vp(&XV(t, X_STRUM)) && strum_blocks(t) == sb));
    mods_off = 0; t->p[P_AMODE] = 0;
    for (n = V_MONO; n <= V_UNISON; n++) {
        t->p[P_VOICE] = (int16_t)n;
        ok = card_hidden_vp(&XV(t, X_STRUM)) && !strum_blocks(t);
        kpress(K_C4); blk(3u * sb);
        ok &= gate_note(t, 60) && !gate_note(t, 64) && !kb_sw[K_C4];
        krelease(K_C4); blk(2);
        bad += check(n == V_MONO ? "MONO: the root alone, no strum, card hidden; released clean" : n == V_LEGATO ? "LEGATO: the same" : "UNISON: the same (all its voices on the root)",
                     ok && !ngated(t) && quiet());
    }
    t->p[P_VOICE] = V_POLY;
    t->p[P_CHRD] = CH_OFF;
    bad += check("CHRD OFF: the card hidden", card_hidden_vp(&XV(t, X_STRUM)));
    t->p[P_CHRD] = CH_MAJ;
    /* the card's text */
    {
        char val[PV_LEN];
        const char *u;
        param_format(&XTP[X_STRUM], 25, val, &u);
        bad += check("  the card: '25' 'ms' (0 says OFF: ui_draw.c)", str_eq(val, "25") && str_eq(u, "ms"));
    }
    /* LAYER: each part its own STRUM */
    duell_mode_force(DM_LAYER); blk(2);
    t2->p[P_CHRD] = CH_MAJ; XV(t2, X_STRUM) = 0;
    kpress(K_C4);
    ok = ngated(t) == 1u && ngated(t2) == 2u;     /* (part 2's third tone: the start limit, one block later) */
    blk(sb);
    ok &= ngated(t) == 2u && ngated(t2) == 3u;
    krelease(K_C4); blk(3u * sb);
    bad += check("LAYER: part 1 strums (10 ms), part 2 (OFF) plays its chord at once (its top tone one block later); the release ends both", ok && !ngated(t) && !ngated(t2) && quiet());
    XV(t2, X_STRUM) = 20;
    kpress(K_C4); blk(sb);
    ok = ngated(t) == 2u && ngated(t2) == 1u;
    krelease(K_C4); blk(4u * strum_blocks(t2));
    bad += check("  part 2 with its own 20 ms: released early, both parts' waiting tones cancelled", ok && quiet());
    midi(0x90, 60, 100); blk(sb);
    ok = ngated(t) == 2u && ngated(t2) == 1u;
    midi(0x80, 60, 0); blk(4u * strum_blocks(t2));
    bad += check("  the same from MIDI IN (part 1's channel plays both)", ok && quiet());
    /* the panics */
    duell_mode_force(DM_SPLIT); blk(2);
    kpress(K_C4); blk(1);
    duell_mode_req = DM_STEREO; blk(3u * sb);
    ok = !ngated(t) && !kb_sw[K_C4];
    krelease(K_C4); blk(2);
    bad += check("a MODE change while a chord strums: its waiting tones are forgotten, nothing sounds after", ok && quiet());
    kpress(K_C4); blk(1);
    transport_req = 4; blk(3u * sb);               /* STOP ALL */
    ok = !ngated(t) && !kb_sw[K_C4];
    krelease(K_C4); blk(2);
    bad += check("STOP ALL while a chord strums: the same", ok && quiet());
    kpress(K_C4); blk(1);
    panic_req = 1; blk(3u * sb);                   /* a sound load's panic of part 1 */
    ok = !ngated(t) && !kb_sw[K_C4];
    krelease(K_C4); blk(2);
    bad += check("a sound load (panic of the part) while a chord strums: the same", ok && quiet());
    midi(0x90, 60, 100); blk(1);
    midi(0xB0, 123, 0); blk(3u * sb);              /* All Notes Off */
    for (f = 0, ok = 1; f < MCHORD_N; f++) ok &= !mchord[f].id;
    bad += check("  MIDI All Notes Off while a MIDI chord strums: forgotten", ok && !ngated(t));
    duell_mode_force(DM_SPLIT);
    return bad;
}

static int stagger(void)
{
    int bad = 0, ok;
    track_t *t = &trk[0], *t2 = &trk[1];
    uint32_t i, h0;
    reset();
    kpress(K_C4);
    bad += check("STAGGER: a single key starts in its own block (no added latency)", gate_note(t, 60));
    krelease(K_C4);
    t->p[P_CHRD] = CH_MAJ;
    silence();
    kpress(K_C4);
    ok = ngated(t) == 2u;
    blk(1);
    bad += check("  a triad: the lowest two in the key's block, the third one block (0.73 ms) later", ok && ngated(t) == 3u);
    krelease(K_C4);
    t->p[P_CHRD] = CH_MAJ7;
    silence();
    kpress(K_C4);
    ok = ngated(t) == 2u && !gate_note(t, 71);
    blk(1);
    bad += check("  a four-note chord: the lowest two at once, the other two one block (0.73 ms) later", ok && ngated(t) == 4u);
    for (i = 0; i < 4u; i++) trk_note_on(t, 60u + (i == 3u ? 11u : 4u * i - (i == 2u ? 1u : 0u)), 90);   /* the same four again, sounding: a re-strike */
    bad += check("  a re-strike of the four sounding notes (an arp / sequencer step on a held chord): all at once, nothing held back", !vs_qn[0] && ngated(t) == 4u);
    krelease(K_C4); blk(1);
    t->p[P_CHRD] = CH_OFF;
    silence();
    for (i = 0; i < 8u; i++) mq(0x91, 48u + 3u * i, 100);   /* part 2's channel: 8 notes in one block */
    h0 = vs_held;
    blk(1); ok = ngated(t2) == 2u;
    blk(1); ok &= ngated(t2) == 4u;
    blk(1); ok &= ngated(t2) == 6u;
    blk(1); ok &= ngated(t2) == 8u && !vs_qn[1];
    bad += check("8 MIDI notes in one block on part 2: they start 2 + 2 + 2 + 2 (the last 2.18 ms late), 6 held back", ok && vs_held - h0 == 6u);
    for (i = 0; i < 8u; i++) mq(0x81, 48u + 3u * i, 0);
    blk(2);
    bad += check("  all eight end", !ngated(t2) && quiet());
    silence();
    for (i = 0; i < 8u; i++) { mq(0x90, 48u + 3u * i, 100); mq(0x91, 50u + 3u * i, 100); }
    blk(1); ok = ngated(t) == 2u && ngated(t2) == 2u;
    blk(3); ok &= ngated(t) == 8u && ngated(t2) == 8u;
    bad += check("  8 + 8 on both parts in one block: each part 2 a block, both complete after 4 blocks", ok);
    for (i = 0; i < 8u; i++) { mq(0x80, 48u + 3u * i, 0); mq(0x81, 50u + 3u * i, 0); }
    blk(2);
    bad += check("  all sixteen end", quiet());
    silence();
    for (i = 0; i < 8u; i++) mq(0x91, 48u + 3u * i, 100);
    mq(0x81, 48u + 3u * 7u, 0);                     /* the eighth let go in the same block, before its turn */
    blk(4);
    bad += check("a note-off of a held-back note cancels it: 7 sound, the eighth never", ngated(t2) == 7u && !gate_note(t2, 48 + 21) && !vs_qn[1]);
    for (i = 0; i < 8u; i++) mq(0x81, 48u + 3u * i, 0);
    blk(2);
    silence();
    for (i = 0; i < 8u; i++) mq(0x91, 48u + 3u * i, 100);
    blk(1);
    transport_req = 4; blk(4);                      /* STOP ALL with five waiting */
    bad += check("STOP ALL empties the queue: nothing starts after it", !ngated(t2) && !vs_qn[1]);
    for (i = 0; i < 8u; i++) mq(0x81, 48u + 3u * i, 0);
    blk(2);
    t2->p[P_VOICE] = V_UNISON;
    mq(0x91, 60, 100); blk(1);
    ok = ngated(t2) == trk_nvoice(t2) && !vs_qn[1];
    mq(0x81, 60, 0); blk(1);
    t2->p[P_VOICE] = V_MONO;
    for (i = 0; i < 6u; i++) mq(0x91, 60u + i, 100);
    blk(1);
    ok &= gate_note(t2, 65) && !vs_qn[1];
    for (i = 0; i < 6u; i++) mq(0x81, 60u + i, 0);
    blk(2);
    bad += check("UNISON: all its voices start with the key; MONO: never held back (the last of 6 notes sounds at once)", ok && !ngated(t2));
    t2->p[P_VOICE] = V_POLY;
    for (i = 0; i < 12u; i++) mq(0x91, 40u + 2u * i, 100);   /* more notes than voices: stealing */
    blk(7);
    ok = ngated(t2) <= NVOICE && gate_note(t2, 40 + 22) && !vs_qn[1];
    for (i = 0; i < 12u; i++) mq(0x81, 40u + 2u * i, 0);
    blk(2);
    bad += check("12 notes at once on 8 voices: the latest sound (stealing as ever), all end", ok && quiet());
    /* LAYER's XFADE share rides with a held-back note */
    duell_mode_force(DM_LAYER); blk(2);
    mix_xf = 60;
    for (i = 0; i < 5u; i++) mq(0x90, 60u + i, 30u + 20u * i);
    blk(1);
    for (i = 0, ok = 1; i < 5u; i++) {
        uint32_t j, xg = xf_gain(0, 30u + 20u * i), f = 0;
        blk(i == 2u || i == 4u ? 1u : 0u);
        for (j = 0; j < NVOICE; j++) if (t->v[j].gate && t->v[j].note == 60 + i) f = t->v[j].xg == xg;
        ok &= f;
    }
    bad += check("LAYER XFADE: a held-back note keeps its own velocity's share (part 1)", ok);
    for (i = 0; i < 5u; i++) mq(0x80, 60u + i, 0);
    blk(2);
    mix_xf = 0;
    duell_mode_force(DM_SPLIT);
    bad += check("  all end", quiet());
    return bad;
}

/* the random walk: anything, then everything let go; nothing may stay */
static int walk(void)
{
    int bad = 0;
    uint32_t seed = 0x1447u, it, k, worst = 0, run;
    for (run = 0; run < 3u; run++) {
        reset();
        for (it = 0; it < 6000u; it++) {
            uint32_t r;
            seed = seed * 1664525u + 1013904223u;
            r = seed >> 8;
            switch (r % 16u) {
            case 0: case 1: case 2: case 3:
                k = (r >> 4) % 27u;
                fm1_in.notes ^= 1u << k; host_notes = fm1_in.notes;
                break;
            case 4: case 5:
                mq((r & 0x100u ? 0x90u : 0x80u) | ((r >> 9) & 1u), 40u + (r >> 10) % 40u, 90);
                break;
            case 6: mq(0xB0u | ((r >> 9) & 1u), 64, (r >> 10) & 1u ? 127u : 0u); break;
            case 7: XV(&trk[(r >> 5) & 1u], X_STRUM) = (int16_t)((r >> 6) % 4u == 0u ? 0 : (r >> 8) % 30u); break;
            case 8: trk[(r >> 5) & 1u].p[P_CHRD] = (int16_t)((r >> 6) % 10u); break;
            case 9: if ((r >> 6) % 8u == 0u) trk[(r >> 5) & 1u].p[P_VOICE] = (int16_t)((r >> 7) % 4u); break;
            case 10: if ((r >> 6) % 6u == 0u) trk[(r >> 5) & 1u].p[P_AHOLD] = (int16_t)((r >> 7) % 3u); break;
            case 11: if ((r >> 6) % 8u == 0u) trk[(r >> 5) & 1u].p[P_AMODE] = (int16_t)((r >> 7) % 3u); break;
            case 12: if ((r >> 6) % 40u == 0u) duell_mode_req = (uint8_t)((r >> 7) % DM_N); break;
            case 13: if ((r >> 6) % 30u == 0u) song.sel = (r >> 7) & 1u; break;
            case 14: if ((r >> 6) % 4u == 0u) {     /* a burst: 4..9 MIDI note-ons (or offs) in one block */
                    uint32_t j, nb = 4u + (r >> 8) % 6u;
                    for (j = 0; j < nb; j++) mq((r & 0x10u ? 0x90u : 0x80u) | ((r >> 9) & 1u), 40u + ((r >> 10) + 5u * j) % 40u, 90);
                } break;
            default: break;
            }
            events_block(CTL);
            {
                uint32_t q = vs_qn[0] + vs_qn[1];
                worst = q > worst ? q : worst;
            }
        }
        /* let go of everything */
        fm1_in.notes = host_notes = 0;
        for (k = 0; k < 2u; k++) {
            mq(0xB0u | k, 64, 0);
            trk[k].p[P_AHOLD] = 0;
            trk[k].p[P_AMODE] = 0;
        }
        events_block(CTL);
        for (k = 40; k < 80u; k++) { mq(0x80, k, 0); mq(0x81, k, 0); events_block(CTL); }
        blk(400);
        bad += check(run == 0 ? "random walk 1 (6000 blocks: keys, MIDI, pedal, STRUM, CHRD, VOICE, HOLD, ARP, MODE, SEL), all let go: nothing sounds or waits" :
                     run == 1 ? "random walk 2: the same" : "random walk 3: the same", quiet());
        if (!quiet())
            printf("   left: part 1 %u gated, part 2 %u, queues %u %u\n", ngated(&trk[0]), ngated(&trk[1]), vs_qn[0], vs_qn[1]);
    }
    printf("   (the most note-ons held back at once in the walks: %u)\n", worst);
    return bad;
}

int main(void)
{
    int bad = strum() + strum_midi_hold() + stagger() + walk();
    printf("%s\n", bad ? "STRUM TEST FAILED" : "strum / stagger test passed");
    return bad != 0;
}
