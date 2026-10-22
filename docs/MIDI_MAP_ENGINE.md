<!-- SPDX-License-Identifier: GPL-3.0-only -->
# The MIDI MAP engine (job C): what the unit page (job H) and the editor tab (job I) build on

The design is `docs/design/MIDI_MAP_DESIGN_2.md`; the shipped list is `docs/MIDI_MAP_DEFAULTS.csv` (a change of the map is a change of that file). Code: `felucca-base/firmware/src/midi_map.h` (constants),
`midi_control.c` (the table and the CC dispatch, ISR side), `midi_learn.c` (edits, names, facts, the learn page's calls, main loop), `settings_persist.c` (the record), `editor_map.c` and `editor_learn.c` (commands),
`tools/gen_midi_map.py` (csv to header). Protocol: `web/EDITOR_PROTOCOL.md` "MIDI MAP".

## The table

128 lines of 16 bits, `midi_map[cc]`, in the pool (256 bytes, the only copy: the last saved record is kept as its head plus a CRC-32 of the map, and a backup builds its settings record on demand in proj_scratch). The shipped list is copied in at the first use (power-on RAM) and replaced by the settings record's lines when a record is read.

| bits | meaning |
| --- | --- |
| 0..9 | target code: 0..126 part value by id (P_E0..P_E7 included), 127 never, 128..142 extension slot (128 + slot), 256 MORPH, 257 ROLL A, 258 ROLL B, 259 UNDO, 260 ROLL (one CC, centred: 0..42 End A, 43..84 nothing, 85..127 End B; fires on entering a zone), 261 AMOUNT, 262 TARGET, 263 SMART, 264 ARP, 265 PITCH, 266 MODE, 267 BALANCE, 268 XFADE, 269 SPLIT, 270 CUTOFF_SMART (CC74), 271 RES_SMART (CC71), 320 + G id a global value, 512..767 PERFORM macros (reserved), 1000 locked, 1023 empty |
| 10..11 | part rule: 0 the channel's part, 1 part 1, 2 part 2, 3 global (any channel, before the MIDI IN routing). **A learned line (who 2) is pinned to a track: the two bits are the track 0..3** (a learned CC has always been one track's, the hidden two included) |
| 12..13 | who: 0 shipped, 1 typed, 2 learned, 3 locked |
| 14 | spare ("also send out"; CC out is not built; always 0) |
| 15 | cleared by the user: an empty line with this bit is never filled by a later firmware's shipped list |

The 23 locked numbers (the 18 standard ones 0 6 32 38 96..101 120..127 and the 5 inputs 1 2 4 11 64: MIDI_MAP_FINAL) are locked lines: never assigned, handled by `midi_control` as before (CC 1, 11, 64 stay the inputs they are; 2 and 4 are kept for the matrix's BREATH and FOOT, not read yet).

The shipped map is `docs/MIDI_MAP_DEFAULTS.csv` = docs/design/MIDI_MAP_FINAL.md: 70 direct lines, 23 locked, 35 unused. The morph fader is CC70, ROLL CC102, AMOUNT CC103; UNDO, the four morph switches, BALANCE, XFADE, SPLIT and MODE have no number (learn-only; typed lines work). ROLL A / ROLL B stay as targets of typed lines.

## Calls (main loop; `midi_learn.c`)

| call | does |
| --- | --- |
| `map_set(cc, code, rule, who, force, name)` | MAP_OK, MAP_E_LOCKED, MAP_E_CONFLICT (the number moves something else: `name` gets the owner's, 24 bytes), MAP_E_BAD, MAP_E_FULL (16 learned). who TYPED: rule is a part rule; LEARNED: rule is the track. The parameter's old number is freed. `force` takes a taken number |
| `map_clear(cc)` | the number has no line, marked cleared |
| `map_reset()` | RESET MAP: exactly the shipped list |
| `map_target_info(code, track, &info)` | kind (continuous, switch, stepped, action), saved in (patch, multi, device), automation recordable, inert now |
| `map_name(line, buf)` | the owner's name ("CUTOFF", "T2 ATTACK") |
| `ml_learn(cc, k, id)` / `ml_learn_f(.., force, name)` | the learn page's call: 1 learned, 0 full, 2 never learned, 3 the number is another's (the footer says "CC74 IS CUTOFF"; default no; `ml_conflict_cc` is it + 1) |
| `ml_arm_row(code, rule)` | the editor's MAP_ARM: the next CC heard becomes the row's (typed) line |
| `map_sanitize()`, `map_merge_new()` | a stored map made valid; a later firmware's shipped lines into empty never-cleared numbers (settings import) |

Clearing a learned line (OCT-, CLEAR) gives its number back to the shipped line there, if its parameter has no other number. One parameter is (target code, part key); a channel-rule line and a pinned line of the same value are two parameters.

## Settings record

`persist_t` ends with `map[128]` and `map_tail[64]` (zero, reserved); `favorites.factory[14]` (the old learn table's 32 bytes) is the header: `[0]` version 1, `[1]` flags (bit 0: differs from the shipped list), `[2]` the shipped list's
revision (`MIDI_MAP_DEFAULTS_REV`, from the csv), `[3..31]` zero. Another size is no record; another version starts with the shipped list. **This job is the only owner of the record; nothing else is added to it.**

## Tests

`tests/midimap_test.c` (both the old and the new code under one roof, tests/midimap_refgen.py), `tests/midimap_gen_test.py`, the map and learn checks in `tests/editor_test.c`; `./host_tests.sh midimap midimapmut`. The old MIDI tests run on
`tests/midimap_legacy.csv` (today's 24 CCs): the harness's `build/gen_test/midi_map_defaults.h`.
