/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
 * The pitch wheel's calibration: smk_wheel.h (M. Geisler, genkigenki/sloop-fm1 smk37, via OberVave), GPL-3.0. */
/* DUELL on the M-Vave SMK-37 (BOARD_SMK37 only; seq.c includes it at its end, keyboard_block calls smk_events_block first): the
 * interrupt side of the owner's control plan, in the audio interrupt's event step like the keys.
 *   Pads: the 16 PERFORM effects in reading order (top row 1..8, bottom row 9..16 = perform.c's PF_* order), held while pressed,
 *     whatever page is shown; the pressure of the pads held sets DEPTH while they are held, and DEPTH comes back when they are let go.
 *   Pitch wheel: the keys' part's bend, as a MIDI pitch bend on its channel (DUELL's bend range, +-2 by default).
 *   Mod wheel: a modulation source only (the matrix's MOD WHEEL, as MIDI CC 1), never MORPH (owner: the patch decides).
 *   The outer keys C3..E3 and G#5..C6 (the middle 27 are the FM-1's keys): notes with their velocity on the keys' part, as MIDI
 *     notes on its channel (they follow OCTAVE- / OCTAVE+). */
#include "smk_wheel.h"

static int8_t smk_depth_saved = -1;             /* DEPTH before the pads' pressure took it, -1: none held */
static uint8_t smk_ext_note[FM1_NKEY];          /* the note an outer key started (+1), 0 = none */

#define SMK_SPLIT_K37 18u                       /* F#4, the middle of the 37 keys: part 2 from it (owner 2026-11-18) */
static uint32_t smk_split_k37(void)               /* (owner 2026-11-20) MENU > PLAYING > SPLIT, the LAYER's setting, in STEREO and DUEL too: part 2 from that key (key 0 = C3 = note 48); OFF: F#4 */
{
    uint32_t s = mix_split;
    if (!s || s > 128u)
        return SMK_SPLIT_K37;
    s -= 1u;
    return s <= 48u ? 0u : s - 48u > 37u ? 37u : s - 48u;
}
static uint32_t smk_key_part(uint32_t k37) { return smk_kp ? smk_kp - 1u : k37 < smk_split_k37() ? 0u : 1u; }
static uint32_t smk_keys_part(void) { return duell_mode == DM_LAYER ? 0u : smk_kp ? smk_kp - 1u : song.sel; }
static uint8_t smk_ext_ch[FM1_NKEY];            /* the channel an outer key's note went to (its release goes there too) */

static void smk_pads_block(void)
{
    int32_t e;
    uint32_t p, mx = 0, any = 0;
    while ((e = smk_pad_take()) >= 0) {
        uint32_t pad = (uint32_t)e >> 8, vel = (uint32_t)e & 0xFFu;
        if (pad < PF_N)
            perf_press(pad, vel != 0u);
    }
    for (p = 0; p < SMK_NPAD; p++)
        if (fm1_in.pad_st[p] & 0x80u) {
            any = 1;
            if (fm1_in.pad_press[p] > mx)
                mx = fm1_in.pad_press[p];
        }
    if (any) {
        if (smk_depth_saved < 0)
            smk_depth_saved = perf_depth;
        perf_depth = (int8_t)(mx * 100u / 127u);
    } else if (smk_depth_saved >= 0) {
        perf_depth = smk_depth_saved;
        smk_depth_saved = -1;
    }
}

static void smk_wheels_block(void)
{
    enum { DEAD = 12, MOD_LO = 485, MOD_HI = 615 };   /* (OberVave's obv_smk.h smk_wheels) */
    static smk_pw_t pw;
    static int32_t mlo, mhi, last_b = 0x7FFFFFFF, last_m = -1;
    int32_t m = fm1_in.wheel[1], mw, b;
    uint32_t ch;
    if (fm1_in.frames < 300u)                       /* (the readings settle) */
        return;
    if (!mhi) {
        mlo = m < MOD_LO ? m : MOD_LO;
        mhi = m > MOD_HI ? m : MOD_HI;
    }
    if (m < mlo) mlo = m;
    if (m > mhi) mhi = m;
    mw = mhi - m <= DEAD ? 0 : (mhi - m - DEAD) * 127 / (mhi - mlo - DEAD > 1 ? mhi - mlo - DEAD : 1);   /* up = lower raw */
    mw = clamp(mw, 0, 127);
    b = smk_pw_bend(&pw, fm1_in.wheel[0], fm1_ms) * 8192 / (SMK_PW_ST * 4096);   /* -> the MIDI bend's -8192..8191 */
    b = clamp(b, -8192, 8191);
    {   /* the part(s) the keys play: LAYER part 1 (both through it), a part alone, else both (the keys split) */
        uint32_t pm = duell_mode == DM_LAYER ? 1u : smk_kp ? 1u << (smk_kp - 1u) : 3u, p, nb = b != last_b, nm = mw != last_m;
        last_b = b;
        last_m = mw;
        for (p = 0; p < 2u; p++) {
            if (!((pm >> p) & 1u) || (p && midi_pch[1] == midi_pch[0] && (pm & 1u)))
                continue;
            ch = midi_pch[p];
            if (nb) {                               /* (only on a change: a MIDI bend in still works at rest) */
                midi_channel(ch)->bend = (int16_t)b;
                midi_expression_channel(ch);
            }
            if (nm) {
                midi_channel(ch)->wheel = (uint8_t)mw;
                mod_midi(midi_track(ch), 0xB0u, 1u, (uint32_t)mw);
                if (duell_mode == DM_LAYER)
                    mod_midi(&trk[1], 0xB0u, 1u, (uint32_t)mw);
            }
        }
    }
}

static void smk_extkeys_block(void)
{
    int32_t e;
    while ((e = smk_extkey_take()) >= 0) {
        uint32_t k = (uint32_t)e >> 8, vel = (uint32_t)e & 0xFFu, ch = midi_pch[smk_keys_part()];
        uint32_t b = k < SMK_KEY_F3 ? k : k - 27u;
        if (k >= FM1_NKEY)
            continue;
        if (vel)
            smk_ext_down |= 1u << b;
        else
            smk_ext_down &= ~(1u << b);
        if (vel && (fm1_in.buttons & kb_mask)) {   /* a layer's button held (GLOBE, ARP): the key is the layer's, silent (ui_layer.c layer_keys) */
            smk_lay_ext |= 1u << b;
            continue;
        }
        if (vel) {
            uint32_t pt = duell_mode == DM_LAYER ? 0u : smk_key_part(k);
            const track_t *t = &trk[pt];
            ch = midi_pch[pt];
            int32_t n = 53 + ((int32_t)k - (int32_t)SMK_KEY_F3) + 12 * song.octave + t->p[P_TRANS];   /* (seq.c kb_pitch: key 0 = F3 = 53) */
            n = clamp(n, 0, 127);
            if (smk_ext_note[k])
                midi_note_event(smk_ext_ch[k], smk_ext_note[k] - 1u, 0);
            smk_ext_note[k] = (uint8_t)(n + 1);
            smk_ext_ch[k] = (uint8_t)ch;
            midi_note_event(ch, (uint32_t)n, vel);
        } else if (smk_ext_note[k]) {
            midi_note_event(smk_ext_ch[k], smk_ext_note[k] - 1u, 0);
            smk_ext_note[k] = 0;
        }
    }
}

static void smk_events_block(void)
{
    smk_pads_block();
    smk_wheels_block();
    smk_extkeys_block();
}
