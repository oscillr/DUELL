/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 DUELL contributors */
/* THE MIDI MAP (docs/design/MIDI_MAP_DESIGN_2.md): one table of 128 lines, one per CC number. A parameter with no line is
 * "learn only"; learning writes a line. The table replaces the fixed lists of midi_control.c (the standard map, the morph's
 * and the MODE's numbers) and the 16 learn slots of midi_learn.c. Names and constants only, shared by the firmware, the
 * generated defaults (tools/gen_midi_map.py) and the tests.
 *
 * A line is 16 bits, indexed by the CC number:
 *   bits 0..9    the target code (below)
 *   bits 10..11  the part rule: MAP_R_CHANNEL (the part the CC's channel plays), MAP_R_PART1, MAP_R_PART2, MAP_R_GLOBAL.
 *                A LEARNED line (who 2) is pinned to a track instead: the two bits are the track 0..3 (a learned CC has always
 *                been one track's, the two hidden tracks included: LEARN_GET / LEARN_SET carry the track), never a rule.
 *   bits 12..13  who made the line: MAP_W_SHIPPED, MAP_W_TYPED (the editor or the unit's page), MAP_W_LEARNED, MAP_W_LOCKED
 *   bit 14       spare: "also send out" (CC out is not built; the bit stays, always 0)
 *   bit 15       "cleared by the user": a shipped line the user took away or moved (an empty line with this bit is never
 *                filled again by a later firmware's shipped list)
 * The 23 locked numbers (MIDI_MAP_FINAL: the 18 standard ones 0 6 32 38 96..101 120..127 and the 5 inputs 1 2 4 11 64) are lines of kind LOCKED: shown, never assignable, handled
 * by midi_control.c as they always were. An empty line has the target MAP_T_NONE. */
#ifndef DUELL_MIDI_MAP_H
#define DUELL_MIDI_MAP_H

#define MAP_N 128u                      /* lines: one per CC number */
#define MAP_LEARN_MAX 16u               /* learned lines (the learn page's old size); typed lines have no cap */

/* --- target codes (10 bits) ----------------------------------------------------------------------------------------- */
/* 0..126   the part values by id (P_*, the eight engine slots P_E0..P_E7 included); 127 is the motion escape, never a target
 * 128..255 the extension slots (128 + slot: ext.h XWID)
 * 256..    the actions                                                                                                   */
enum {
    MAP_T_MORPH = 256,                  /* the MORPH fader 0..127 */
    MAP_T_ROLL_A, MAP_T_ROLL_B,         /* a roll of End A / End B: 64 and up fires once, below 64 re-arms (no shipped line: the shipped map has the one ROLL below; kept for typed lines and the reference tests) */
    MAP_T_UNDO,                         /* UNDO: 1..63 End A, 64..127 End B, 0 re-arms */
    MAP_T_ROLL,                         /* ROLL, one CC centred like the knob (MIDI_MAP_FINAL): 0..42 rolls End A, 43..84 nothing, 85..127 rolls End B; a roll fires when the value ENTERS its zone */
    MAP_T_AMOUNT,                       /* the morph AMOUNT 0..127 -> 0..100 % */
    MAP_T_TARGET,                       /* the morph TARGET 0-42 PART 1, 43-85 PART 2, 86-127 BOTH */
    MAP_T_SMART, MAP_T_ARP, MAP_T_PITCH,    /* the morph's three switches: 64 and up on */
    MAP_T_MODE,                         /* the output MODE 0-42 SPLIT, 43-85 STEREO, 86-127 LAYER (global) */
    MAP_T_BALANCE,                      /* -100..+100, 64 = 0 (global) */
    MAP_T_XFADE,                        /* 0..100 % (global) */
    MAP_T_SPLIT,                        /* 0 OFF, 1..127 the note (global) */
    MAP_T_CUTOFF_SMART,                 /* CC74: the engine's own cutoff, or the multimode filter's (MIDI_CC_ENG) */
    MAP_T_RES_SMART,                    /* CC71: the same for the resonance */
    MAP_T_ACT_END,
    MAP_T_G0 = 320,                     /* 320 + G_* id: a global value (the effects block); up to 351 */
    MAP_T_G_END = 352,
    MAP_T_PERF0 = 512,                  /* 512..767: PERFORM macros (reserved, not used) */
    MAP_T_PERF_END = 768,
    MAP_T_LOCKED = 1000,                /* the target of a LOCKED line */
    MAP_T_NONE = 1023                   /* an empty line */
};

/* --- the 16-bit line --------------------------------------------------------------------------------------------------- */
enum { MAP_R_CHANNEL, MAP_R_PART1, MAP_R_PART2, MAP_R_GLOBAL };
enum { MAP_W_SHIPPED, MAP_W_TYPED, MAP_W_LEARNED, MAP_W_LOCKED };
#define MAP_OUT 0x4000u
#define MAP_CLEARED 0x8000u
#define MAP_LINE(target, rule, who) ((uint16_t)((uint32_t)(target) | (uint32_t)(rule) << 10 | (uint32_t)(who) << 12))
#define MAP_EMPTY MAP_LINE(MAP_T_NONE, 0, MAP_W_SHIPPED)
#define MAP_TARGET(l) ((uint32_t)(l) & 0x3FFu)
#define MAP_RULE(l) (((uint32_t)(l) >> 10) & 3u)
#define MAP_WHO(l) (((uint32_t)(l) >> 12) & 3u)
#define MAP_IS_EMPTY(l) (MAP_TARGET(l) == MAP_T_NONE)
#define MAP_IS_LOCKED(l) (MAP_WHO(l) == MAP_W_LOCKED)
#define MAP_T_G(g) (MAP_T_G0 + (g))     /* the code of global value g (the generated defaults use these) */
#define MAP_T_X(k) (128 + (k))          /* the code of extension slot k (= ext.h XWID) */

/* the 23 locked numbers: never assigned. 1 (the wheel), 11 (expression), 64 (the pedal) stay the inputs they are; 2 (BREATH) and 4 (FOOT) are inputs
 * of the matrix as well (MIDI_MAP_FINAL: reserved for it, not assignable) */
#define MAP_LOCKED_CC(cc) ((cc) == 0u || (cc) == 1u || (cc) == 2u || (cc) == 4u || (cc) == 6u || (cc) == 11u || (cc) == 32u || (cc) == 38u || (cc) == 64u || \
                           ((cc) >= 96u && (cc) <= 101u) || ((cc) >= 120u && (cc) <= 127u))

/* --- what the editor and the unit's page may ask of a target (map_target_info) ---------------------------------------------- */
enum { MAP_K_CONTINUOUS, MAP_K_SWITCH, MAP_K_STEPPED, MAP_K_ACTION };      /* kind */
enum { MAP_S_PATCH, MAP_S_MULTI, MAP_S_DEVICE };                          /* where the value is saved */
typedef struct {
    uint8_t kind, saved, automation, inert;     /* automation: AUTOMATION records a CC move of it (1/0); inert: does nothing now (1/0) */
} map_info_t;

/* --- results of map_set / map_clear ------------------------------------------------------------------------------------ */
enum { MAP_OK, MAP_E_LOCKED, MAP_E_CONFLICT, MAP_E_BAD, MAP_E_FULL };

#endif
