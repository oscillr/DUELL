# Felucca editor protocol (SysEx over USB-MIDI)

The firmware side is `firmware/src/editor.c`. Commands 1-15, track selection, dumps, step reads
and parameter writes (27, 29, 30, 31) were checked on hardware. `tests/editor_test.c` also exercises the real C
handler, malformed transfers and stop timeouts against simulated flash. User-preset
flash writes and live pushes (16-26, 32) have host coverage; that does not establish
their hardware behavior. The step chance byte, motion (64) and the full backup (65-67) have host
coverage only (`tests/editor_test.c`, `tests/backup_test.c`, `web/test_backup.mjs`).

**v3 (four tracks):** the device has four tracks, each a synth part (since 1.0; before it, track 4
was a GM drum track: see "Track 4 since 1.0" below). One
of them is *selected* (ALGORITHM on the device, or `TRACK`). Every v1 / v2 command acts on the
selected track (its parameters, engine, preset, steps, the user presets it stores or loads); `TRACK`,
`TRACK_MIX`, `TRACK_DUMP` and `TRACK_STEP` reach any track. Command numbers 1-26 are unchanged.

**v4 (any track's parameters):** `TRACK_PARAM` gets or sets a parameter of any track without changing
the selection, and the `TRACK_CHANGED` push follows level, pan and mute of the tracks that are not
selected. The v1-v3 commands are byte for byte as before; v4 is asked for with bit 1 of `WATCH`.

**v5 (the drum grid):** a step also holds lane hits and their accents (see "v5: the drum grid" below).
`STEP_GET` / `STEP_SET` / `TRACK_STEP` replies get 3 more bytes, `STEP_SET` / `TRACK_STEP` take them
optionally, and `UP_GET` / `UP_PUT` carry a user preset's 16-step grid after its pattern. Every
earlier byte is where it was; an editor that reads the first bytes of a reply keeps working.

**v7 (motion, chance, backup, 89 parameters):** a step gets a chance byte after its hits. `MOTION` (64) reads
and edits a track's recorded knob moves. `BACKUP_LIST` / `BACKUP_GET` / `BACKUP_PUT` (65-67) read and
restore the whole device. P_COUNT is 89 and P_E0 is 81: twenty FM operator parameters (ids 61..80) went in
before the engine parameters, which moved from 61..68 to 81..88. Always take P_E0 from `INFO`. INFO ends with
tagged capability blocks for these (see `INFO`).

**MENU settings (1.0.4):** `MENU_DESC` (72) and `MENU_SET` (73) read and set the device's MENU settings, described
by the device (see "MENU settings" at the end); INFO advertises them with `4E 01 count`. 1.0.5: a `MENU_DESC` reply
ends with the item's tab on the device (index and name), after the bytes 1.0.4 sent. 1.3: item 23 HOME (DISPLAY),
appended (`4E 01 24`).

**Ratchet (1.0.5, INFO `52 01 04`):** a step gets a ratchet byte (its hits, 1..4) after its chance: one more byte at
the end of the step replies, and an optional last byte of a step write (see "Ratchet" below). Nothing else changed.

**Parameter locks (1.1, INFO `4C 01 01`):** a motion record can be a lock: its value sounds on its step only and
goes back after it. `MOTION` (64) gets ops 5..7 (set a lock, clear a step's locks, the query with the kinds); the
query and ops 1..4 reply byte for byte as before (see "Parameter locks" below). A lock takes no new bytes in a
project (bit 7 of a motion record's id); 1.1 projects are FUN9 for the DRUM lane levels below.

**Chord keys (91 parameters, 1.0):** two track parameters, `CHRD` (81) and `VOIC` (82), went in before the
engine parameters, which moved from 81..88 to 83..90: P_COUNT 91, P_E0 83. No command changed; an editor that
takes P_COUNT and P_E0 from `INFO` keeps working (see "The chord keys" below).

**DRUM lane levels (99 parameters, 1.1):** eight track parameters, `KICK` .. `BELL` (83..90, the DRUM engine's lane
levels), went in before the engine parameters, which moved from 83..90 to 91..98: P_COUNT 99, P_E0 91. No command
changed; projects are FUN9 (3648 bytes, see "Projects (FUN9)"). See "The DRUM lane levels" below.

**1.2 (FUN10: 104 parameters, 128 motion records, NUDGE, song sections; INFO `41 01 00 01`, `54 01 16` and `57 01 4`):** four track parameters,
`SYNC` `TRIG` `POL` (91..93, LFO 2) and `QNTZ` (94, QUANTIZE), then `SPRD` (95, SPREAD, before FUN10's release),
went in before the engine parameters, which moved from 91..98 to 96..103: P_COUNT 104, P_E0 96 (builds of 1.2 before
SPREAD: 103, 95). The four tracks share 128 motion records (64 before): `MOTION` replies as
before 1.2 (max 64, at most a track's first 64 records), op 8 lists them all. A step gets a nudge byte after its
ratchet. A song's rows became sections with a slot per track (`SONG` ops 4..7; ops 0..3 as before). Projects are FUN10
(3840 bytes). See "1.2: LFO 2, NUDGE, 128 records (FUN10)" and "1.2: song sections" below.

**1.4.1 (FM6's voice bank; INFO `56 01 32`):** FM6's SLOT (P_E0 + 7) runs to 40: 9..40 = B1..B32, the voices of a
32-voice bank the editor writes whole (cmds 74..77). Nothing else changed. See "FM6 voice bank (74-77)" below.

**1.5 (111 parameters, FUN10 as it was):** seven track parameters went in before the engine parameters, which moved
from 96..103 to 103..110: P_COUNT 111, P_E0 103. The INSERT, `INSRT` `INS A` `INS B` `INS C` `MIX` (96..100, a per-track
insert effect with a dry / wet mix, Discussions #78 #177), then `TYPE` (101, ANALOG's filter LP / BP / HP, #104) and
`ESYNC` (102, ENV SYNC: ATK DEC REL as note values of the tempo, #175). No command changed; an editor that takes
P_COUNT and P_E0 from `INFO` keeps working. Projects stay FUN10 (3840 bytes, 111 at byte 66; 16 bytes to spare). See
"1.5: the INSERT" and "1.5: filter TYPE, ENV SYNC" below.

**DUELL step 3 (still 111 parameters, P_E0 103; projects FUN11):** DUELL reuses the 29 ids Felucca gave to things DUELL
does not have, so P_COUNT stays 111 and every 7-bit id carrier (this protocol, motion records, MIDI LEARN, user preset
records) is unchanged. **The meaning of these ids changed** (an editor must take labels from `DESC`, not assume Felucca's):

| id | DUELL | was (Felucca) |
| --- | --- | --- |
| 8 | `-` free (fmt INT, 0..0) | `FX` (ENV DEST, never read) |
| 61..72 | matrix slots 5..8: `SRC5 DST5 AMT5` .. `SRC8 DST8 AMT8` (as slots 1..4) | DIGITAL's OP1..OP3 ENV / LVL |
| 73..76 | ENV 2: `ATK2` `DEC2` (TIME) `SUS2` (PCT) `REL2` (TIME), defaults 10 70 90 60; with `ESYNC` (102) on, note values as ENV's | OP3..OP4 ENV / LVL |
| 77..80 | LFO 2: `RATE2` (LFOHZ, 60) `WAVE2` (SIN TRI SAW SQR S&H) `PHS2` (0..127) `FADE2` (TIME) | OP4 ENV / LVL |
| 83..85 | LFO 2: `SYNC2` (as SYNC 91) `TRIG2` (NOTE FREE) `POL2` (BI UNI) | DRUM lane levels KICK SNARE CLAP |
| 86..90 | `-` free (fmt INT, 0..0) | DRUM lane levels HATCL .. BELL |

**DUELL 0.2.9: the multimode filter (ids 45..48, still 127 parameters).** Ids 45..48 were the SLICER's SLCR PAT RATE DEPTH in Felucca, `-` free in DUELL up to 0.2.1, and 0.2.2 .. 0.2.8 used 45 (a bipolar `CUT`, -64..63, 0 OFF, negative a low-pass, positive a high-pass) and 46 (`RES`). Since 0.2.9 they are the four knobs of EDIT > FILTER:
45 **`CUT`** (fmt 23 `FILT`, 0..82, default 82: a dial step shown as its cutoff, step d = table index (40 + 396 d) / 256 of the 30 Hz .. 16 kHz cutoff table, so 0 is 30 Hz and 82 16 kHz, the open end; each step is 0.2.2's step),
46 **`RES`** (fmt PCT, 0..127, default 0),
47 **`MODE`** (fmt 24 `FMODE`, 0..127, default 0: 0 LP, 64 BP, 127 HP and every blend between; shown LP, LP>BP, BP, BP>HP, HP),
48 **`SLOPE`** (fmt ENUM, **1..2**, default 1: 12dB, 24dB; the first enum whose range does not start at 0: `DESC` sends the names of min .. max).
They act on the engines with no filter of their own (PHASE LOFI VOICE WHEEL PHYS FM6 WAVE: engine bytes 2 3 5 7 9 12 14), where the ENV / LFO FLT depths, the matrix's CUT and `CC` 74 / 71 drive them. Since the same release they also run on ANALOG, TRIO and NOISE (engine bytes 0 6 11) **in series after the engine's own filter** (the device's page `FILTER`, as on every engine; their own filters are on the pages VCF (ANALOG), TRIO FILTER and NOISE FILT; an editor lists the same four ids as a page of those engines): there the engine's filter, its routes and `CC` 74 / 71 are exactly what they were, the multimode filter's effective cut is its CUT alone, and the matrix destination 38 FRES is its RES (live there, no longer faded; the DEST line reads "FILTER RESONANCE"). They are bypassed by default there too, so every sound of before is bit for bit the same. On the retired engines (4 8 13) and DRUM they are stored and do nothing. They are the SOUND's (a sound load sets them), not the track's. Take the labels and ranges from `DESC`. Automation (`MOTION`) records CUT and RES like any knob move (MODE and SLOPE are not automated). The MORPH rolls all four (SPEC 2.5, rule 3g). `CC` 74 / 71 are CUT / RES on those engines: CC74 0..127 spans the whole CUT dial (0.2.2's "64 = OFF" is gone); a MIDI LEARN map on CUT spans it the same way. The effective cut is CUT plus every modulation (ENV 1 / ENV 2 / LFO 1 / LFO 2 FLT depths and the matrix destination CUT, 1/256 dial steps as before), clamped to 0..82. **The BYPASS: MODE LP (0) with the effective cut at the open end: the filter is not run and the sound is bit for bit as without it (the defaults; RES and SLOPE do not matter there).** Over the last 4 dial steps at a closed end the output fades to silence (LP / BP at 0, HP / BP at 82). No card is faded for a value of 0.
**Old stores:** SLOPE is never 0 in 0.2.9; a stored sound with 48 at 0 (or without 48: a patch record of before) holds 0.2.2's bipolar CUT at 45, and the device converts it when it loads (patch records, projects, multis, the autosave, `UP_PUT`): OFF -> CUT 82, MODE LP (the bypass); a low-pass c (-64..-1) -> CUT 82 + c, MODE LP (the same cutoff exactly); a high-pass c (1..63) -> CUT c, MODE HP (0.16 of a table step higher); SLOPE 12 dB. Its CUT automation goes with it. An editor that sends a sound of before with `SET`s should convert it the same way first (web/editor.html `ufLegacy`); one that sends it whole (`UP_PUT`) need not. On ANALOG, TRIO and NOISE nothing was ever meant by 45..48, so a stored sound of before with 48 at 0 there takes the bypass whatever it holds (CUT 82, RES 0, MODE LP, SLOPE 12 dB; a leftover from another engine must not switch the filter on), and its CUT automation goes to the open end; the editor's `ufLegacy(p, serial)` does the same. A store of Felucca's layout (projects FUN10 and before, user preset records of versions 1..5) reads 45..48 as the defaults (the bypass).

ENV 2 and LFO 2 have no destination amounts of their own: they reach the sound only through the matrix. The matrix's
SRC lists grew (append-only): 11 ENV2, 12 LFO2; DST: 21 RATE2 (LFO 2's rate), 22 DEPTH2 (LFO 2's depth). Automation
(motion) records ATK2..REL2 and RATE2..FADE2 (as their ENV / LFO twins), not SYNC2 TRIG2 POL2 and not the matrix.
Stores of Felucca's layout (projects FUN10 and before, user preset records of versions 1..5, settings without DUELL's
MIDI LEARN mark) load with every id above at its DUELL default (matrix slots 5..8 OFF): their old values never reach a
DUELL parameter. A DIGITAL sound in such a store still converts with its OP ENV values (read before the reset).
User presets DUELL writes are version 6 (7 with a drum grid); `UP_PUT` records are taken as DUELL's layout.
Projects are FUN11 (`46 55 4E 42`, "FUNB", 3840 bytes): FUN10's layout with only parts A and B (tracks 1..2) stored,
their FM6 patches at 3824 − 2 × 128; tracks 3..4 load with their power-on sound. FUN10 and older load as before, then
as above, tracks 3..4 not kept. Firmware before DUELL step 3 (and Felucca) refuses a FUN11.

## Framing

A request is `F0 7D 46 4C <cmd> <args...> F7`:

- `7D` is the non-commercial SysEx ID.
- `46 4C` is "FL".

Every valid request gets exactly one reply, with the same header and the same `<cmd>`.
Unknown commands and invalid fixed argument lengths get no reply; argument errors
follow the command-specific rules below.
Fixed-size commands require exactly the lengths in the tables; step writes accept the
8-byte legacy step, the 11-byte grid step, the 12-byte step with its chance (a chance above 100 gets
no reply) or the 13-byte step with its ratchet (a ratchet outside 1..4 gets no reply). Every data byte is 7 bit.
MIDI realtime bytes may occur inside SysEx; another status aborts the partial frame. While the editor
watches (v2, `WATCH`), the device also sends push frames (cmds 23, 24, 26) at any time.

| Item | Encoding |
| --- | --- |
| value (v14) | 2 bytes, LSB first, holding value + 8192, so the range is -8192..8191. `[lo, hi]`: value = (lo \| hi << 7) − 8192 |
| u32 (v7) | 5 bytes, 7 bits each, LSB first; the last byte is 0..15. Used by the backup commands |
| string | ASCII bytes, ended by a 0 byte |
| scope | 0 = parameter of the selected track (`P_*`, 0..P_COUNT−1); 1 = global parameter (`G_*`, 0..G_COUNT−1) |
| track | 0..3: tracks 1..4 (synth parts) |
| engine byte | 0..NENGINES−1 (firmware before 1.0: NENGINES = its drum track, no engine). The numbers are fixed, new engines are appended: 0 ANALOG, 1 reserved (DIGITAL before 1.0: see below), 2 PHASE, 3 LOFI, 4 reserved (SAMPLE until DUELL 0.2.8), 5 VOICE, 6 TRIO, 7 WHEEL, 8 reserved (GRAIN until 0.2.8), 9 PHYS, 10 DRUM, 11 NOISE, 12 FM6, 13 reserved (SLICE until 0.2.8), 14 WAVE (DUELL 0.2.5: NENGINES 15; before it 14). **DUELL 0.2.9 retired SAMPLE, GRAIN and SLICE:** INFO still lists 15 names, theirs are `-` (as DIGITAL's); `DESC` of their E slots gives `-` (fmt INT, -64..127, so a stored value is kept); a part or patch that names one plays silent and keeps its values; `SET G_ENGSEL` or `PRESET` with one gives ANALOG's first sound. The device and the editor list the pickable ones in another order (ANALOG FM6 PHASE LOFI VOICE TRIO WHEEL PHYS NOISE WAVE, DRUM hidden: `ENGINE_ORDER`, 10 shown); the numbers stay |

The engine parameters are `P_E0..P_E7`: P_COUNT−8 .. P_COUNT−1 (103..110 since 1.5; 96..103 in 1.2 .. 1.4; 91..98 in
1.1), and `INFO` gives `P_E0`.
Their meaning, range and names depend on the current engine, so re-read `DESC` for them
after an engine change.

The modulation matrix (the MOD and MOD 2 pages, `firmware/src/mod.c`) is 24 track parameters at ids 49..72 (DUELL: 8
slots; Felucca had 12 at 49..60 since P_COUNT 69): slot k (1..8) is `SRCk`, `DSTk`, `AMTk` at ids 49 + 3 (k − 1) ..
51 + 3 (k − 1). Slots 5..8 are as slots 1..4 (the table's ids + 12).

| id | label | values |
| --- | --- | --- |
| 49, 52, 55, 58 | SRC1..SRC4 | enum: 0 OFF, 1 LFO, 2 ENV, 3 VEL, 4 KEY, 5 RAND, 6 MODW (CC1), 7 AT (channel aftertouch), 8 EXPR (CC11), 9 S&H (a random value held for one cycle of the track LFO), 10 SLEW (S&H gliding from the cycle before's value), 11 ENV2 (the voice's ENV 2, DUELL), 12 LFO2 (the track's LFO 2, DUELL) |
| 50, 53, 56, 59 | DST1..DST4 | enum (46 names since DUELL 0.2.7; 23 before): 0 OFF, 1 PITCH, 2 CUT, 3 SHP, 4 AMP (per voice); 5 PAN, 6 DIST, 7 CHO, 8 DLY, 9 REV, 10 RATE (LFO rate), 11 VIB (LFO pitch depth), 12..19 E1..E8 = `P_E0..P_E7`, 20 DEPTH (the track LFO's depth: a gain on LFO DEST and the matrix's LFO source), 21 RATE2 (LFO 2's rate), 22 DEPTH2 (LFO 2's depth) (per block); DUELL 0.2.7, appended (per block, the part's own ids): 23 LEVEL (id 0), 24 INS A, 25 INS B, 26 INS C (ids 97..99, named by the INSERT TYPE as `DESC` of those ids does), 27 IMIX (100), 28 EQLOW (8), 29 EQMID (88), 30 EQHI (103), 31 DUCK (DEPTH, 107), 32 ATK2, 33 DEC2, 34 SUS2, 35 REL2 (73..76), 36 E2FLT (114), 37 E2SHP (115), 38 FRES (46, the multimode filter's RES); bus values (an offset per part on a global value: in DUEL on the part's own bus, in STEREO / LAYER both parts' offsets add): 39 CRT (global 10), 40 CDP (11), 41 FDBK (5), 42 DMDEP (13, the delay modulation's DEPTH), 43 DMRAT (12, its RATE), 44 RSIZE (8), 45 RDAMP (9). The delay's TIME is no destination |
| 51, 54, 57, 60 | AMT1..AMT4 | −64..63 (fmt BIPCT) |

`DESC` names E1..E8 as such; the device shows the engine's label of that parameter instead (`DESC` of
`P_E0 + n`), and so does the editor. (DUELL 0.2.7) So it does for INS A..C (`DESC` of ids 97..99, the INSERT TYPE's labels), and
the MATRIX page names the selected destination in full ("DEST: WAVE SYNC", "DEST: REVERB SIZE"). A destination the part cannot
move with its current choices does nothing and the device shows it faded with the reason: an engine slot labelled "-" (UNUSED),
WAVE's TABLE (REBUILD), FM6's SLOT (LOADS), (until 0.2.8: SAMPLE SET and SLICE SRC DIV START REV, AT NOTE: read only as a note starts), PAN in
DUEL, INS A..C and IMIX while the INSERT TYPE is OFF, RES on an engine with a filter of its own; an editor may do the same. A
stored DST keeps its number (append-only): a sound saved with 0.2.6 means the same; a sound with DST 23..45 loaded by older
firmware is outside that firmware's list (it does not have them). A slot with SRC, DST or AMT at 0 does nothing; every default is 0
(a factory preset sets all twelve to 0). The modulation never changes the stored values: `GET`,
`DUMP` and the pushes report what was set.

**DIGITAL (engine 1) was replaced by FM6 (Dexed-based)** in 1.0. The number stays reserved: `INFO` names it
"-" (a build with `FELUCCA_FM4=1` has DIGITAL back, named "DIGITAL"), `NAMES 1` lists no presets, its EDIT
descriptors are "-" with DIGITAL's ranges, and no track ever has it. Whatever brings a DIGITAL sound plays it as
FM6 with a patch converted from its values (`firmware/src/fm4_convert.c`; the patch is the track's own, `FM6_GET`
reads it, a project keeps it): `SET` of `G_ENGSEL` = 1 (DIGITAL's first preset), `PRESET` 1 k (its preset k),
`UP_LOAD` of a slot stored with engine 1 (`UP_PUT` still takes engine 1: the record keeps the DIGITAL values, every
load converts them), projects with DIGITAL tracks (their motion on E1..E8 and ids 61..80 is dropped). The track
reports engine 12 (FM6) and the FM6 preset that covers the sound. The editor converts DIGITAL patches of library
files the same way (it equals the firmware's, `test_web.mjs`).

(DUELL: the next two paragraphs describe Felucca's layout, as stores of Felucca hold it; on a DUELL device ids 61..80
and 83..90 are DUELL's, see "DUELL step 3" above.) The four FM operator envelopes (the DIGITAL engine's OP1..OP4 ENV
and OP LEVEL pages, `eng_digital.c`) are twenty track parameters at ids 61..80, five per operator: operator k (1..4) has `ATK`, `DEC`, `SUS`, `REL`,
`LVL` at ids 61 + 5 (k − 1) .. 65 + 5 (k − 1). Since DIGITAL was retired they are inert: no page and no editor
layout shows them, only the conversion of a DIGITAL sound reads them (its FM6 tracks hold the defaults). The ids
and labels stay (library files key values by label).

| id | label | fmt, range, default |
| --- | --- | --- |
| 61, 66, 71, 76 | ATK | TIME, 0..127, 0 |
| 62, 67, 72, 77 | DEC | TIME, 0..127, 0 |
| 63, 68, 73, 78 | SUS | PCT, 0..127, 127 |
| 64, 69, 74, 79 | REL | TIME, 0..127, 0 |
| 65, 70, 75, 80 | LVL | PCT, 0..127, 127 |

The defaults change nothing: the sound is the same as before the parameters existed. The master `ENV`
(ids 1..4) shapes the whole voice; these shape each operator on top of it. Other engines ignore them, and the
device shows the pages for DIGITAL only. Descriptors are the same for every engine.

The chord keys (SCL > CHORD on the device, `firmware/src/chord.c`) are two track parameters at ids 81, 82:

| id | label | values |
| --- | --- | --- |
| 81 | CHRD | enum: 0 OFF, 1 DIA3, 2 DIA7 (the triad / seventh of the instrument's global SCALE and ROOT on the key: every tone in key; plain MAJ / MAJ7 while the scale is CHROMATIC), 3 MAJ, 4 MIN, 5 DOM7, 6 MAJ7, 7 MIN7, 8 SUS4, 9 POW (fixed shapes) |
| 82 | VOIC | enum: 0 CLOSE, 1 OPEN (1-5-3), 2 INV1, 3 INV2, 4 +OCT (the root an octave down; a seventh drops its fifth) |

With CHRD on, a key, a MIDI note of that track and so the arp's held notes play the chord (at most 4 notes),
and live recording writes it into one step; MONO / LEGATO / UNISON play its root; a kit (DRUM, SLICE)
ignores it. Both defaults are 0: nothing changes until CHRD is set. They are the track's, like ARP: a
sound load (a factory preset, `UP_LOAD`, an audition) keeps them, and motion never records them.

The DRUM lane levels (EDIT > LANES / LANES 2 on the device, `firmware/src/eng_drum.c`) are eight track parameters at
ids 83..90, one per lane: 83 KICK, 84 SNARE, 85 CLAP, 86 HATCL, 87 HATOP, 88 TOM, 89 RIM, 90 BELL. Percent (`F_PCT`),
0..127, default 127 = 100 % (the kit as designed), square law (64: about −12 dB). Only the DRUM engine reads them; the
editor shows them on a DRUM track only (the device's pages). They are the sound's (not the track's): a factory preset
sets them back to 127, user presets and projects store them (older ones load at 127), motion records them.

Other enums that grew: `P_SDIV` (DIV, id 30) has 10 names (1/4, 1/8, 1/16, 1/32, 8T, 16T, 1/2, 1/1, 2BAR,
4BAR; the first six keep their numbers), `P_AMODE` (id 17) 15 (OFF, UP, DN, UPDN, RND, ORD, REPEAT, and since 1.2 DNUP, UP+8, CONV, DIVG, PINKY, THUMB, WALK,
CHORD: values 7..14, appended; an editor that knows only 7 names shows the value by `DESC`'s name), `P_AHOLD` (HOLD, id 22; DUELL 0.2.9) 3 (OFF, ARP, ALL: 0 and 1 are the old OFF / ON, so stored data stays valid; ALL, value 2, latches the keys even with the arp off; max 2, not morphed or rolled) and the
global `G_CLOCK` (id 2, label "CLK") 3 (INT, USB, TRS). `G_MIDI` (id 12) was an enum of USB / TRS that nothing read; since DUELL 0.2.5 it is the delay modulation's RATE (`DESC`: label "RATE", LFOHZ, 0..80, default 44) and `G_SYNC` (id 13, was the one-name placeholder "--") is its DEPTH (label "DEPTH", PCT, 0..100, default 0; 0 = off). Both are global parameters on the device's DELAY 2/2 page, stored with the project and a multi; an old store holds 0 (or, for id 12, 0/1) there, which is DEPTH 0 = no change.

**Retired enum values (aliases).** A value that no longer exists keeps its number, so stored sounds stay valid:
`DESC` names it like the value it now plays (an alias), the device never holds it (a `SET` of it lands on that value
and the reply says so) and its knob steps over it. Of a repeated name, the original is the first value when that has
the name, else the last one with it; an editor should list only the originals and show an alias by its name when
it ever sees one. Today: DRUM KIT (engine 10, E1)
1, 2, 3 (once HAND, CYM, H+CYM; 1.0.5) = 6 (66), 5 (10), 8 (77): its names are STD 66 10 77 80 10 66 55 77, the
device's knob steps STD 80 10 66 55 77. (SAMPLE SET and GRAIN SRC 1 and 4, once TRANH, PERC, were aliases of 0 PIANO until DUELL
0.2.9 retired both engines.) An editor of before 1.0.5 (first of a name = original) offers 66 10 77
at 1..3 and hides 6 5 8: what it sets still plays the right kit, and the device answers with 6 5 8.

## Commands

| cmd | Request args | Reply args |
| --- | --- | --- |
| 1 INFO | — | version string, NENGINES, P_COUNT, G_COUNT, NSTEP, P_E0, then NENGINES engine-name strings, then (v3) NTRK (4), then (v6) CHAIN_ROWS (16), then the tagged blocks `55 01 uiCaps`, `4D 01 64 01`, `42 01 3`, `46 01 nfactory nbank`, `53 01 3`, (1.0.3) `50 01 3` and (1.0.4) `4E 01 count` (MENU settings; 12, 1.1: 15, 1.2: 17, 1.1.5: 18, 1.2: 23, 1.3: 24, 1.5: 25) and (1.0.5) `52 01 4` (ratchet) and (1.1) `4C 01 1` (parameter locks) and (1.2) `41 01 00 01` (128 motion records: two 7-bit bytes, LSB first; `4D 01 64 01` keeps saying 64) and `54 01 16` (a step's nudge, 1/16 of a step) and `57 01 4` (song sections: a slot per track, SONG ops 4..7) and (1.4.1) `56 01 32` (FM6's voice bank: cmds 74..77, SLOT 9..40) and (1.5) `43 01 16` (MIDI LEARN's map: cmds 78, 79) (below); older firmware ends earlier |
| 2 GET | scope, id | scope, id, v14 |
| 3 SET | scope, id, v14 | scope, id, v14 (the value after clamping). Setting global `G_ENGSEL` (id from DESC label "ENG") changes the engine: its defaults, then its first preset (as on the device) |
| 4 DUMP | — | engine, preset, then P_COUNT × v14 (the selected track), then G_COUNT × v14 (globals) |
| 5 DESC | scope, id | scope, id, fmt, min v14, max v14, def v14, label string, unit string, then for an enum (fmt 8) one name string per value (at most 64 since DUELL 0.2.7, the matrix DST has 46; before: at most 24; firmware before the matrix: at most 16) |
| 6 STEP_GET | index 0..NSTEP−1 | index, n (0..4 notes), note0..note3, time (0 NOTE, 1 TIE, 2 REST), flags (1 accent, 2 slide), vel, then (v5) hits (3 bytes, below), then (v7) chance 0..100, then (`52 01`) ratchet 1..4, then (`54 01`, 1.2) nudge + 8 (0..15: −8..+7 sixteenths of the step) |
| 7 STEP_SET | index, n, note0..3, time, flags, vel [, hits (3 bytes, v5) [, chance 0..100 (v7) [, ratchet 1..4 [, nudge + 8, 0..15 (1.2)]]]] | same as STEP_GET (after the write). Without the hits the step keeps its own; without the chance, the ratchet or the nudge it keeps its own. The chance can only follow the hits, the ratchet the chance, the nudge the ratchet; a nudge byte above 15: no reply, nothing written |
| 8 PRESET | engine, preset | engine, preset (applies the preset's sound to the selected track and sends; the steps and the track's own parameters stay, see "Sound loads and undo"). For another track, select it with `TRACK` first |
| 9 PROJECT | op (0 load, 1 save, 2 query), slot 0..3 (DUELL: 0..15, the multis M01..M16 of SAVE > MULTI) | op, slot, used (1/0). Save writes flash: allow ~2 s; it stops the transport first (see "Saves while playing") |
| 10 NAMES | engine | engine, count, count preset-name strings, then the two edit-page titles |
| 11 SMP_BEGIN | slot 0..2 | slot, rc **6 (unsupported)**. DUELL 0.2.9 has no user sample slots (the SAMPLE, GRAIN and SLICE engines are retired); nothing is written. (Before: erased the slot's header sector, rc 0 ok) |
| 12 SMP_WRITE | slot, offset (3 × 7 bit, LSB first), pack7 data (≤ 256 bytes) | slot, offset, rc **6 (unsupported)**; nothing is written. (Before: rc 0 ok, 1 arguments, 2 erase, 3 write, 4 slot in use) |
| 13 SMP_END | slot, pack7 header (480 bytes) | slot, rc **6 (unsupported)**; nothing is written. (Before: rc 0 ok, 1 size, 2 header, 3 data CRC, 4 flash, 5 zones) |
| 14 SMP_ERASE | slot 0..2 | slot, rc **0**: there is nothing to erase and nothing is written (so a backup archive's empty sample slot restores as before) |
| 15 SMP_INFO | — | **0, 0**: no slots, slot KiB 0 (DUELL 0.2.9). (Before: slots, slot KiB (the largest), then per slot: zone count, name string, data KiB; then per slot its own size in KiB) |
| 16 UP_LIST | start, count (1..16) | start, count, total slots, then per slot: used (0/1), engine, name string ("" if unused); 1.2: then one category byte per slot listed (below; 0 if unused) |
| 17 UP_GET | slot | slot, used, engine, name, P_COUNT × v14, 16 × (note, flags), then (v5) kind (0 a note pattern, 1 a drum grid) and for kind 1 16 × hi; 1.2: then the category |
| 18 UP_PUT | slot, engine, name, P_COUNT × v14, 16 × (note, flags) [, kind 0 or 1, 16 × hi (v5) [, category (1.2)]] | slot, rc (0 ok, 1 args, 2 flash). Writes flash: allow 1 s |
| 19 UP_STORE | slot, name | slot, rc. Stores the current sound: engine, parameters, the first 16 sequencer steps as the pattern (TIE steps → flag 4) |
| 20 UP_LOAD | slot | slot, rc (0 ok, 1 empty/invalid). Applies its sound (not its pattern; the steps stay) |
| 21 UP_ERASE | slot | slot, rc |
| 22 WATCH | on (0/1; v4: 3 = also `TRACK_CHANGED`) | on (0/1; v4 firmware: 3 when 3 was asked for). While on, the device pushes cmds 23, 24, 26 (and 32 with bit 1) |
| 23 CHANGED (push) | — | scope, id, v14 |
| 24 RELOAD (push) | — | engine, preset, then (v3) the selected track |
| 25 PING | — | 0 |
| 26 STEP_CHANGED (push) | — | index, then (v3) the selected track |

| cmd (v3) | Request args | Reply args |
| --- | --- | --- |
| 27 TRACK | — (query), or track (select it) | selected track, NTRK, then per track: engine byte, preset, level v14, mute (0/1), armed (0/1, live recording) |
| 28 TRACK_MIX | track (get), or track, level v14 (0..127), mute (set) | track, level v14, mute: the track's `P_LEVEL` and `P_MUTE` |
| 29 TRACK_DUMP | track | track, engine byte, preset, P_COUNT × v14 (that track's parameters; no globals) |
| 30 TRACK_STEP | track, index (get), or track, index, n, note0..3, time, flags, vel [, hits (v5) [, chance (v7) [, ratchet [, nudge + 8 (1.2)]]]] (set) | track, index, n, note0..3, time, flags, vel, then (v5) hits, then (v7) chance, then (`52 01`) ratchet, then (`54 01`) nudge + 8 |

| cmd (v4) | Request args | Reply args |
| --- | --- | --- |
| 31 TRACK_PARAM | track, id (get), or track, id, v14 (set); id = `P_*` (0..P_COUNT−1) | track, id, v14 (the value after clamping, as `SET`). The selection does not change; no push about the editor's own write |
| 32 TRACK_CHANGED (push) | — | track, id, v14: `P_LEVEL`, `P_PAN` or `P_MUTE` of a track that is not selected changed on the device (only while `WATCH` was sent with bit 1) |

| cmd (v7) | Request args | Reply args |
| --- | --- | --- |
| 64 MOTION | track (query); track, 1, on 0/1 (play on / off); track, 2 (clear); track, 3, step, id, v14 (set an event); track, 4, step, id (delete an event or a lock); (1.1) track, 5, step, id, v14 (set a lock); track, 6, step (clear the step's locks; step 127: every step's); track, 7 (query with the kinds); (1.2) track, 8 (every record, with the kinds) | track, rc, on (0/1), count (this track's events and locks), max (64), then count × (step, id, v14); after ops 5..7 (1.1) then count × kind (0 automation, 1 lock), in the same order. 1.2: ops 1..7 list at most the track's first 64 records (count ≤ 64, max 64: as before); op 8 answers track, rc, on, count lo, count hi, max lo, max hi (two 7-bit bytes each, LSB first; max 128), count × (step, id, v14), count × kind |
| 65 BACKUP_LIST | — | 1, rc, count (11), then per object: id, size u32, crc u32 |
| 66 BACKUP_GET | id, offset u32, count lo, count hi (≤ 256) | id, rc, offset u32, count lo, count hi, pack7 data |
| 67 BACKUP_PUT | op 0 begin: 0, id, size u32, crc u32; op 1 data: 1, id, offset u32, pack7 data; op 2 commit: 2, id; op 3 abort: 3, id | op, id, rc |

A slot number above 2 (or a request too short) gets no reply. (Before DUELL 0.2.9, without flash `SMP_BEGIN`, `SMP_WRITE`, `SMP_END`
and `SMP_ERASE` got no reply.)

**pack7:** groups of up to 7 bytes, each preceded by one byte holding their top bits
(bit j = bit 7 of byte j). A mask must have at least one following data byte;
unused mask bits in the last group must be zero. Oversized or incomplete transfers
are rejected; BACKUP_PUT decodes at most 256 bytes a piece.

**User sample slots (retired, DUELL 0.2.9).** The sample engines and their slots are gone; the slot format (header "FSMP" at 0, IMA
ADPCM data at 512, SLICE's hand-set slices "SLM1" in the last sector) is no longer read or written, and the flash it used
(0xA0000..0xC7FFF) is left unused, free for future WAVE user wavetable slots. The reference uploader `tools/fm1_sample_upload.py` is
removed; `tools/sampleio.py` stays as a WAV helper of the host tests.

`fmt` values (`firmware/src/core.h`):

| Value | Name | Value | Name | Value | Name |
| --- | --- | --- | --- | --- | --- |
| 0 | INT | 5 | CUTOFF | 10 | NOTE |
| 1 | PCT | 6 | DB | 11 | ONOFF |
| 2 | BIPCT | 7 | SEMI | 12 | OCT |
| 3 | TIME | 8 | ENUM | 13 | STEPS |
| 4 | LFOHZ | 9 | BPM | | |
| 14..22 | EQDB, EQLO, EQMID, EQHI, EQQ, DKCH, DKNOTE, DKRLS, DKHLD (DUELL's EQ and DUCK) | 23 | FILT (DUELL 0.2.9: the multimode filter's CUT, a dial step 0..82 shown as its cutoff in Hz / kHz; 0.2.2 .. 0.2.8: the bipolar CUT, 0 OFF) | 24 | FMODE (DUELL 0.2.9: the filter's MODE, LP LP>BP BP BP>HP HP) |

The editor should show the value with the unit; formatting it exactly like the device does
is not required.

## v2: user presets

A user preset = engine (0..NENGINES−1), name (1..12 chars, ASCII 32..126; the device shows it upper
case), all P_COUNT instrument parameters (v14 each, the same order as `DUMP`), and a 16-step pattern:
16 × (note 0..127 (0 = rest), flags: 1 accent, 2 slide, 4 tie, 8 | 16 the ratchet − 1 (firmware with `52 01`;
before, 0: x1, and such firmware drops those bits when it loads the pattern)). Loading one applies the engine and
the parameters of the sound, as a factory preset (the track's own parameters, the steps and LEN stay; see
"Sound loads and undo"). The stored pattern is kept and returned by `UP_GET`; on the device SAVE > PHRASES
lists it as "U07" and loads it, with the stored LEN (at most 16), DIV, SWING and GATE. The slots are
numbered 0..31 (the device shows U01..U32).

**DUELL (2026-10-12): the PATCH store.** Slots 0..127 (128; `UP_LIST`'s "total slots" byte is 127, the most a data byte holds:
the slots are all there), a patch being a user patch or a factory sound's copy (the SAVE > PATCHES page). The wire
format of `UP_GET` / `UP_PUT` / `UP_LIST` / `UP_STORE` / `UP_ERASE` is unchanged. At most **48 slots are in use** at a
time (`PT_MAX`, upreset.c): a new patch (`UP_PUT` / `UP_STORE` of an empty slot) with 48 stored answers rc 2 (the device says
NO FREE PLACE); a write to a slot already stored always fits. Flash: six storage objects (`OBJ_PATCH0..5`, A/B pairs at
0xC8000..0xCDFFF, 0xDC000..0xDFFFF, and 0x9F000 / 0xFE000) hold sparse records (only the values that differ from the engine's
INIT; an FM6 sound carries its patch inside the record; `upreset.c`): the biggest record is 435 bytes and 8 fit a chunk, hence 48.
`BACKUP` ids 6, 7, 9, 10, 11 and 12 are the six chunks (ids 10..12 since the store grew; firmware before answers rc 1 at the begin of
one, which the web page skips; an archive that has only 6, 7 and 9 empties 10..12 on restore; id 8, the old FM6 patch bank, stays
empty); an archive of Felucca's banks is refused.

- `UP_LIST`: count is cut at 16 and at the last slot (start ≥ 32: count 0, no entries).
- `UP_GET` of an empty slot has the same shape with used 0, engine 0, name "" and all values 0.
  Values come back in the current parameter order, inside their ranges.
- `UP_PUT`: rc 1 for a slot ≥ 32, an engine ≥ NENGINES, a name that is empty, longer than 12 or has
  bytes outside 32..126, or an incomplete or oversized frame. Values are clamped to their ranges for that
  engine. A note with flag 4 is stored as a tie (note 0); flags on a rest are dropped.
- **DRUM** (engine 10) was PHYS's MODEL 4 (DRUM) before 1.0. A record of PHYS with E1 (MODEL) = 4,
  stored then or sent by `UP_PUT` from an older editor, is the DRUM engine: the device rewrites it (engine
  10; E1..E8 {MODEL, TUNE, TONE, DECY, SNAP, ACC, KICK 0..127, PERC 0..127} become {KIT = PERC / 32, TUNE,
  TONE, DECY, SNAP, ACC, KICK 0 PUNCH / 1 ROUND (old ≥ 64), DRV 0}), and `UP_GET` / `UP_LIST` give it so.
  Projects do the same. PHYS's MODEL is 0..3 (MODAL STRNG MEMB SYMP) now.
- **SAMPLE SET 4** was PERC, the General MIDI drum kit, until 1.0.2. A record of SAMPLE (engine 4) with E1 (SET)
  = 4, stored then or sent by `UP_PUT`, is the DRUM engine with its default kit: the device rewrites it (engine 10,
  E1..E8 = DRUM KIT's {0, 64, 70, 64, 64, 100, 0, 0}, the other values and the pattern as they were). Projects
  and `PRESET` 4 / 4 do the same (a project's track keeps its steps). SET 4 itself stays (`DESC` names it "PIANO":
  an alias, as SET 1; a `SET` of 4 lands on 0), and USR1..USR3 stay 5..7; GRAIN's SRC 4 plays PIANO.
- **DRUM KIT 1..3** were HAND, CYM and H+CYM until 1.0.4 (STD with a conga / claves, a cymbal, both). Since 1.0.5
  they play the VA kits 66, 10 and 77, and a record holding one loads as that kit: `UP_GET` / `UP_LOAD` give KIT 6, 5
  or 8 (the record itself is not rewritten). Projects, motion events and `SET` do the same.
- `UP_STORE`: name "" stores with the automatic name the device uses (engine name + slot number,
  "ANALOG 07"). rc 1 for a bad slot or name.
- rc 2 = the flash write failed or there is no flash. A failed flash write keeps the previous
  record in RAM and flash, including its name. Without flash, a valid change is RAM-only until power-off.
  (`UP_PUT` / `UP_STORE` / `UP_ERASE` stop the transport first, see "Saves while playing"; a device
  that cannot stop it in 100 ms answers rc 2 and writes nothing.)
- Frames stay below 640 bytes (`UP_PUT` is 5 + 1 + 1 + 13 + 2 × P_COUNT + 32 + 1, + 18 with the category).
- **Categories (1.2, Discussion #90).** A user preset has a sound category: 0 none, 1 BASS, 2 LEAD, 3 PAD, 4 PLUCK,
  5 KEYS, 6 DRUM, 7 FX, 8 OTHER (append-only; the device's PRESETS > LIST filters by it, its NAME screen sets it).
  `UP_LIST` sends one byte per listed slot after all the slot entries (0 for an empty slot); `UP_GET` ends with it
  (after kind 0, or after kind 1's 16 bytes). A record saved without one (before 1.2, or by an editor that sends none)
  reports its engine's: 6 DRUM for DRUM and SLICE, else 8 OTHER. `UP_PUT` takes it as one byte after the kind and its
  16 bytes (send kind 0 and 16 zeros for a note pattern); 0 stores none, 9 or more is rc 1. An older editor reads up to
  where it knows and ignores the rest; firmware before 1.2 rejects an `UP_PUT` with the category (rc 1), so send it
  only to a device that answers `UP_GET` with it. The device keeps it in a byte of the record no value uses (the
  record's layout is unchanged; older firmware keeps it). The factory presets' categories are the device's
  (`src/category.c`), not sent.

**On the device:** SAVE > USER page: KNOB 1 picks the slot, KNOB 2 LOAD, KNOB 3 ERASE, KNOB 4 SAVE;
OCT+ executes the selected action, OCT- goes back. SAVE over a used slot asks
"OVERWRITE U07?" with OCT+ / OCT-. SAVE uses the automatic name. The PRESETS knob and the SAVE > PRESETS browser continue past the factory presets into the used user
presets.

**Flash** (`firmware/src/upreset.c`): two storage objects (`OBJ_UPRESET0/1`, A/B sector pairs at
0xDC000..0xDFFFF), 16 records of 192 bytes each, behind a bank header (magic "UPB1", record size,
slot count; a mismatch reads as an empty bank). A record keeps its layout version (versions 1..5 are read; another: empty)
and the P_COUNT it was stored with; another count is mapped by count (last 8 values = P_E0..P_E7, the
first ones = P_LEVEL.. in order, missing ones = defaults). Versions 4 (a note pattern) and 5 (a drum grid)
store each value as one byte, value + 64, so P_COUNT can reach 127; versions 1..3 stored 16-bit values.
The wire format is unchanged: `UP_GET` / `UP_PUT` still carry v14 values, and `UP_PUT` now answers rc 1
for a value outside −64..127. P_COUNT was 53 (P_E0 45) until the SLICER
parameters (SLCR, PAT, RATE, DEPTH: ids 45..48) went in just before P_E0: P_COUNT 57, P_E0 49; then
the twelve matrix parameters (ids 49..60): P_COUNT 69, P_E0 61. An editor takes both from `INFO`;
records stored with 53 load with the SLICER off and every matrix slot off, with 57 with every slot off. The
twenty FM operator parameters (ids 61..80) went in before P_E0 after that: P_COUNT 89, P_E0 81. Records stored with 69 load with
those at their defaults, which change nothing. Then the chord keys (ids 81, 82): P_COUNT 91, P_E0 83; records
stored with 89 (or 69) load with CHRD OFF and VOIC CLOSE, their engine values at 83..90. Then the DRUM lane levels
(ids 83..90): P_COUNT 99, P_E0 91; records stored with 91 (1.0.x) or fewer load with every lane at 127 (100 %), their
engine values at 91..98.

## v5: the drum grid

A DRUM track (engine 10) is edited on the device as a grid of 8 lanes × the steps (SEQ > STEP). The
grid lives in the steps themselves, so every engine has it:

- **Hits.** Each step has a lane mask `hit` and an accent mask `acc` (bit l = lane l, `acc` only on
  lanes that hit). The lanes and the General MIDI note each plays: 0 KICK 36, 1 SNARE 38, 2 CLAP 39,
  3 HAT CL 42, 4 HAT OP 46, 5 TOM 45, 6 RIM 37, 7 BELL 56 (a VA kit plays its own piece on each: KIT 66 a conga
  on lane 5, 77 claves on lane 6, 10 and 77 a cymbal on lane 7). A NOTE step plays its notes and then its hits, on any engine (DRUM strikes its
  lanes, a synth plays the pitches); an accented hit at velocity 127, the others at
  the step's velocity (0 = 96). A TIE or REST step plays no hits.
- **On the wire** (after `vel`): `hit & 127`, `acc & 127`, then `(hit >> 7) | (acc >> 7) << 1`. A
  `STEP_SET` / `TRACK_STEP` of 8 step bytes (an editor of before v5) leaves the step's hits as they are;
  send `0, 0, 0` to clear them.
- **Notes on lanes.** A step's notes keep playing as before; the device shows each note on the lane
  it strikes (GM 35..81, the others folded into their octave of 36..47: a low tom 41 on TOM, a crash 49 on
  BELL). When the device edits a lane of a step, notes on that lane become the lane's hit. Patterns
  loaded on the device into a DRUM track (SAVE > PHRASES, e.g. 12 BEAT), and DRUM tracks of projects saved
  before the grid, get their lanes' own notes as hits (the same notes and velocities play).
- **Sound loads never convert steps** (as before): switching a track to or from DRUM keeps its notes
  and hits.
- **User presets.** A user preset stored from a DRUM track whose first 16 steps strike a lane holds a
  16-step grid instead of a note pattern (record version 3; version 5 with byte values). `UP_GET` gives its low 7 bits in the 16
  (note, flags) pairs (hits, accents), then kind 1 and 16 bytes of bit 7s (bit 0 hits, bit 1 accents);
  a note pattern ends with kind 0. `UP_PUT` with kind 1 and the 16 bytes stores a grid (accents only on
  hits); without an extension, a note pattern as before. If an extension is sent, it must be
  exactly 17 bytes: kind 0 or 1 followed by 16 bytes (kind 0 ignores those bytes).
  Unknown kinds, truncated extensions and extra bytes are rejected without changing the slot (1.2: one byte after
  them is the category, below).
  Firmware before v5 shows a grid record as empty.
- **Projects** in v5 used format 5 ("FUN5"): 10-byte steps and 40 reserved bytes per track.
  FUN6 retained those fields and added the song chain (v6 below); FUN7 is current (v7 below). Older formats
  are converted on load.

## v2: live sync

- `WATCH 1` starts the pushes. Watching ends by itself 3 s after the last request of any kind (send
  `PING` about every 1 s), on a USB reset, and when the host goes away; `WATCH 0` ends it at once.
- **CHANGED** (scope, id, v14): a parameter changed on the device (knob, menu, sequencer edit of a
  `P_*`), not by the editor's own `SET`. Coalesced: each (scope, id) at most every 20 ms, with the
  latest value.
- **RELOAD** (engine, preset): the engine, a preset, a user preset or a project was loaded; re-read
  `DESC` of the engine parameters, `DUMP` and the steps. It is also sent after loads the editor asked
  for with `PROJECT` load and `UP_LOAD`. After the editor's own `PRESET` and `SET` of G_ENGSEL it is not
  (firmware with INFO `53 01`, bit 1): the device takes the load as known (the engine, the preset and the
  parameters the load changed), since the editor re-reads `DUMP` after the reply; a device change still
  pending at that moment (a load or another track selected on the device, a knob turned) is pushed as before.
  Older firmware sends `RELOAD` after those too; an editor that skips its own echoes should do so only when
  bit 1 is clear.
- **WATCH while watching** (firmware with INFO `53 01`, bit 0): `WATCH 1` (or 3) while already watching on
  the same USB connection keeps what has not been pushed yet, so a change made just before is still pushed.
  Older firmware takes everything as known again (re-read `DUMP` after a re-`WATCH` there). `WATCH 0`, the 3 s
  timeout and a USB reset end watching; the next `WATCH` starts from the values as they are.
- **STEP_CHANGED** (index): a sequencer step changed on the device (record, clear, step edit,
  pattern load); not after the editor's own `STEP_SET`.
- Push frames have the normal header. Accept them at any time, also while waiting for a reply:
  match replies by cmd (23, 24 and 26 are never replies). The device sends at most a few per
  ~5 ms pass, and only when its USB send queue has room, so a push never delays a reply.

## v3: tracks

- **Track 4 since 1.0** is a synth part like tracks 1..3: `DUMP` / `RELOAD` / `TRACK` / `TRACK_DUMP`
  give its real engine byte (power-on: DRUM, preset DRUM KIT, the General MIDI map), and `PRESET`, `SET` of
  `G_ENGSEL`, `UP_LOAD` and `UP_STORE` work on it; its level is its `P_LEVEL`. The commands are byte for
  byte as before; only the meaning changed. Firmware before 1.0 had a GM drum track there: engine byte
  NENGINES, no presets (`UP_LOAD` / `UP_STORE` rc 1), its level the global `G_DRLVL`. Drums are now the
  DRUM engine on any part: the first C key is the kick (C2 = 36), notes are GM numbers.
- The globals `G_DRCH`, `G_DRLVL`, `G_DRREV` (ids 24..26: the old drum track's MIDI channel, level and
  reverb send) keep their ids, and `G_COUNT` stays 27. `G_DRLVL` and `G_DRREV` are inert: `DESC` gives
  label "-", range 0..0, and no page shows them. Id 24 (`G_DRCH`, never read since 1.0) is `G_RTYPE`
  since 1.0: the reverb's model, `DESC` label "TYPE", enum ROOM (0) / SPRING (1) / HALL (2, since 1.2) / SHIM (3) / RESO (4), on the
  REVERB page (FX). The power-on value (`DESC` def) is HALL since 1.2, ROOM before; a project keeps its own, and
  projects of formats before FUN7 load it as ROOM. MIDI channel 10 is no longer special (channels 1..4 play
  tracks 1..4, channels 5..16 are ignored since 1.0.3; with ROUT SEL every channel the selected track).
- Selecting a track with `TRACK` does not push `RELOAD` (the editor re-reads `DUMP`, the steps and the
  engine `DESC` itself); selecting one on the device does (`RELOAD` with the new track).
- Pushes are about the selected track only: `CHANGED` (scope 0) and `STEP_CHANGED` refer to it, and
  changes to other tracks (live recording from MIDI into another track, `TRACK_*` writes) push nothing.
- Level and mute are also `P_LEVEL` / `P_MUTE` of the selected track (`SET`); `TRACK_MIX` reaches the
  others. Presets and user presets change a part's sound but keep its `P_LEVEL`, `P_PAN`, `P_MUTE`.
- Projects (`PROJECT`) save and load all four tracks, the selection, the song chain and the motion (FUN7;
  FUN5 added the drum grid, FUN6 the chain). Formats 6..1 are converted; a format 1 project loads into track 1. A project saved before
  1.0 loads its drum track as track 4 with DRUM's kit (SAMPLE PERC until 1.0.2), its steps kept.
- Older firmware (no NTRK in `INFO`): one instrument; skip the track UI.

## v4: any track's parameters

- **Finding out:** send `WATCH 3`. v4 firmware answers 3; v3 (0.8) firmware answers 1, does not know
  cmds 31 / 32 (no reply) and never pushes `TRACK_CHANGED`. `WATCH 1` behaves exactly as in v2 / v3
  (reply 1, no `TRACK_CHANGED`). Match the `WATCH` reply by bit 0.
- `TRACK_PARAM` clamps like `SET` scope 0: to the range of that parameter; the engine parameters
  `P_E0..P_E7` to the ranges of that track's engine. A parameter
  with a fixed range (min = max) keeps its value. A track ≥ NTRK or an id ≥ P_COUNT gets no reply.
  For the selected track it is the same as `SET` scope 0.
- `TRACK_CHANGED` is never about the selected track (its changes stay `CHANGED` scope 0). Coalesced like
  `CHANGED` (each track and id at most every 20 ms, latest value), and not sent for the editor's own
  `TRACK_PARAM` / `TRACK_MIX` writes. After a selection change (`RELOAD`, or the editor's `TRACK`) the
  device takes the current values as known.

## Notes for the editor

- **One request at a time.** Wait for the reply, about 10–50 ms, before sending the next, from one queue
  for every request, keep-alive `PING` included. The device holds one incoming SysEx frame until the main
  loop has answered it; a frame that arrives before then is dropped whole (no reply). A frame is never
  taken in part: USB delivers every packet (acknowledged, retried), and a dropped frame leaves nothing.
  Any frame size up to the largest request (a 256-byte `BACKUP_PUT` / `SMP_WRITE` piece: 306 bytes on the
  wire, 640 bytes of buffer) is safe at full speed; pacing between bytes is not needed. With a second
  request in flight (pipelined, or a keep-alive from another thread) the second is lost: no reply, and a
  following `BACKUP_PUT` / `SMP_WRITE` piece is then refused (rc 1, the offset). Measured on an FM-1 with
  1.0 over USB: 256-byte pieces one at a time, the next sent from the reply callback, 560 of 560 taken
  (about 10 ms each); with the next piece sent before the reply, every second piece was dropped.
- **Following the device.** With v2 firmware, `WATCH` and `PING` (above). Older firmware pushes
  nothing (no reply to `PING`): poll `DUMP` about every 300–500 ms while the page is visible.
- **Port.** The device's MIDI port is named "Felucca" (USB 1209:0001). Updates use the same
  port with other SysEx (the `F0 22 24 35 …` keys, `00 59 …` frames); never send those
  from the editor. Since 1.0 the same USB device also has an audio input ("Felucca",
  44.1 kHz stereo; bcdDevice 3.11; since 1.1 44.1 or 48 kHz, the host's choice, bcdDevice 3.21
  (3.20 with MENU > USB SERIAL OFF)); the MIDI port and this protocol are unchanged, and both
  work while the computer records.
- **Global ids.** `G_ROUTE` (id 14, label "ROUT"; on the device MENU > MIDI > MIDI IN since 1.2, up to 1.1.5
  GLO > SYSTEM ROUT) was a placeholder ("--", range 0..0);
  since 1.0 it is the MIDI IN routing: 0 "CH1-4" (channels 1..4 → tracks 1..4; what projects stored
  before. Until 1.0.2 every other channel played the selected track; since 1.0.3 channels 5..16 are
  ignored: notes, bend, CCs, aftertouch, panic and reset), 1 "SEL" (every channel → the selected track); since 1.2
  (append-only) 2 "CH5-8", 3 "CH9-12", 4 "CH13-16": channels 5..8, 9..12 or 13..16 → tracks 1..4, the others
  ignored as with CH1-4, and the keys' MIDI out on the track's channel of that block. `DESC` of id 14 gives range
  0..4 and the five names; firmware before 1.2 clamps 2..4 to 1 (SEL) when it loads such a project.
  Neither setting affects this SysEx protocol or MIDI clock. The
  id, `G_COUNT` (27) and the project format are unchanged; it is saved and loaded with the project like
  the other globals. The editor shows it under GLOBAL > MIDI. 1.2: the GLO > GLOBAL and GLO > SYSTEM pages are
  gone; BPM, SWG and CLK (ids 0..2) are HOME > CLOCK's, TUNE (id 3) and ROUT (id 14) are edited in the MENU (AUDIO >
  TUNE, MIDI > MIDI IN; also MENU settings 22 and 21, below). Their ids, ranges, names and storage (the project) are
  unchanged. `G_MIDI`, `G_SYNC`, `G_INFO` (12, 13, 15) are on no page any more (MIDI's value was never read: the
  SYSTEM page's card showed the USB state; SYNC a placeholder; CPU a reading): the device's MENU > SYSTEM > ABOUT shows (until DUELL 0.2.1 the INFO item)
  the USB state and the CPU load; `DESC` of them is unchanged.
- **Sound loads and undo.** A load that changes a track's sound (`PRESET`, `SET` of `G_ENGSEL`, `UP_LOAD`,
  and the same on the device) changes the sound only: the engine and the parameters of the sound. It
  never changes the steps, nor the track's own parameters: `P_LEVEL`, `P_PAN`, `P_MUTE` (0, 39, 40), the
  ARP pages, SCL and LEN / DIV / SWING / GATE (17..32) (Felucca: also the SLICER, 45..48; in DUELL those ids are the
  sound's filter, since 0.2.2). It does drop the track's recorded
  motion (the device's SAVE held brings it back). (Before 1.0 a preset also
  set the arp and replaced the steps with its pattern; a factory preset turned the SLICER off.) The byte
  layout of every command is unchanged. Patterns are loaded on the device (SAVE > PHRASES) or written by
  the editor with `STEP_SET` / `TRACK_STEP`.
  The device keeps one copy of the track from before the last load (any track); SAVE held 0.7 s on the
  device swaps back what the load changed (the sound; after a SAVE > PHRASES load, the steps and LEN / DIV
  / SWING / GATE). Loads in a row on one track with nothing changed between them keep the copy from before
  the first; `SET` (scope 0) within 1.5 s of a load on the selected track counts as part of that load, so
  an audition (`G_ENGSEL`, then the patch's values by `SET`, the track's own parameters skipped as
  `UP_LOAD` skips them) and the next one still undo to the state before the first. `PROJECT` load takes no
  copy.
- **Saves while playing.** A flash erase silences the audio and stalls the sequencer for a moment. The
  device refuses its own saves while the transport plays ("STOP TO SAVE"); `PROJECT` save, `UP_PUT`,
  `UP_STORE`, `UP_ERASE`, `BACKUP_LIST` and `BACKUP_PUT` from the editor stop the transport (the sample commands did too until 0.2.9; now they touch nothing)
  first (`BACKUP_GET` does not: it answers rc 3 while the transport runs). If it does not stop within 100 ms, no flash operation starts: user-preset commands
  return rc 2, sample commands a nonzero rc, backup commands rc 3. PROJECT retains its existing reply shape
  (op, slot, used); a failed PROJECT save gets no reply, allowing the editor to report
  a timeout instead of confirming the previous used slot. The previous slot is kept.
  A pending PLAY or SONG start also prevents a device save.
- **Published samples** (until DUELL 0.2.8; no sample slots since 0.2.9). SMP_END repeated with an identical committed header
  succeeded without another write; a different header was rejected (rc 2) until SMP_BEGIN.
- **Safety.** `PROJECT` save, `UP_PUT` / `UP_STORE` / `UP_ERASE`, and the tagged preference writes below write flash, and only in
  Felucca's own storage; never the app or the update area.


## v6: song chain

INFO appends CHAIN_ROWS (16) after NTRK. Earlier INFO bytes and command numbers
stay where they were. No trailing byte means no SONG command: hide its controls.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 33 SONG | op 0 query; op 1 set, count 0..16, count × (slot 0..3, repeat 1..16); op 2 start; op 3 stop | op, rc, count, running 0/1, row 0..15, remaining repeats, count × (slot, repeat) |

A set must have exactly `2 + count × 2` argument bytes. rc 0 = accepted,
1 = invalid rows (or an empty chain on start), 2 = busy, 3..6 = source slot 0..3
is empty. Rejected sets keep every previous row. Set edits RAM; PROJECT save
persists it, PROJECT load recalls it. A stop/start is acknowledged before the
next audio block; query reports the actual running state. `row` and `remaining`
are meaningful only while running. Rows always start from 0, with track 1's
loop as the repeat and transition boundary; the last row stops.

A row takes all four tracks' steps (with their chance) and LEN / DIV / SWING / GATE from its saved
project slot, retaining the current sounds, mix, ARP, scales and effects. Since 1.2 a row is a section with a slot per
track (see "1.2: song sections"): ops 0..3 still work, a row of theirs is a section with its four tracks on that slot. The
four sources are copied before starting, with no flash operation in playback.
Original editable patterns and timing are restored on stop. Sources are shared
project slots; overwriting a slot changes its uses on the next start.

STEP_GET / TRACK_STEP and WATCH report the playing source. During chain
playback, STEP_SET / TRACK_STEP writes, SET / TRACK_PARAM writes to
LEN / DIV / SWING / GATE and MOTION edits (rc 3) are ignored, returning the current value. Other
sound and mix parameters remain editable. Recording and panel pattern edits
also require STOP. Older editors keep working; older firmware gets no SONG
requests from the new editor.

Projects now write FUN7 (below), the same 3388 bytes in the same A/B sectors. FUN6 has the same size;
FUN5 (3352 bytes) and FUN1–FUN4 convert with an empty chain, without changing the sample-slot layout.
A chain row also plays the motion of its source project (events for an FM operator parameter are skipped
when the track's engine is not the saved one).

## v7: chance, motion, MIDI clock

- **Chance.** Each step has a chance, 0..100 %, as the last byte of the step reply (and the optional last byte
  of a step write). 100 is the default and what every older pattern holds; 0 means the step never plays. The
  device rolls once each time a step comes round, for all of it: its notes, its drum hits and a TIE. A failed
  roll plays nothing and releases what rang before. Chance is saved in projects (FUN7) and in song rows; a
  user preset does not store it. Changing it on the device pushes `STEP_CHANGED`.
- **Motion** is knob moves recorded per step: 64 events shared by the four tracks. An event is (track, step
  0..63, parameter id, value). While a track plays its motion, the step sets the value at the step and it
  holds until another step changes it; the loop restarts from the sound's own value, and stopping puts the
  sound's own values back. Parameters that can be recorded (`motion_param`): ids 0..16 (LEVEL, ENV, LFO),
  33..36 (DIST, CHO, DLY, REV), 38 (GLIDE), 39 (PAN), 44 (DETUNE), 61..80 (the FM operator parameters) and
  83..90 (the DRUM lane levels), 91..98 (the engine parameters; not the chord keys 81, 82). The device records them while the track is armed, playing and selected, from its knobs
  and from `SET` / `TRACK_PARAM` alike.
  - `MOTION` with the track alone is the query. `on` 0 keeps the data and stops playing it; 1 plays it. Clear
    drops the track's events and turns it off; on the device it is undone by SAVE held.
  - Set: `value` must be inside the parameter's range for that track's engine (and −64..127); a
    new event when the 64 are used gets rc 2 ("MOTION FULL" on the device), an event that already exists is
    updated. A set turns the track's motion on. A delete of an event that does not exist succeeds.
  - rc: 0 ok, 1 invalid (step ≥ 64, an id that cannot be recorded, a value outside its range), 2 full, 3 a song is playing.
    A track ≥ NTRK, `on` above 1 or any other length gets no reply.
  - Events come back in the order they are stored. The device does not push motion changes: poll `MOTION`
    (the web editor does, while the sequencer tab shows).
  - `DUMP`, `TRACK_DUMP`, a user preset store and a project save give the sound's own (base) values;
    `GET`, `SET` and the pushes give what is sounding now, which differs while a motion event is in effect.
  - A pattern load and a pattern clear on the device drop the track's motion; the device's UNDO brings it back.
    `MOTION` clear does the same through the same undo copy. A sound load (a preset, `PRESET`, `G_ENGSEL`, `UP_LOAD`,
    INIT SOUND) keeps it since 1.1.5 when the engine stays the same; when the engine changes the records on the
    engine's own ids (E1..E8 `P_E0..P_E7`, DIGITAL's 61..80, the DRUM lane levels) go and the rest (both kinds)
    stays, PLAY as it was (until 1.1 a sound load dropped the whole motion). Poll `MOTION` after a load.
  - The device's own lock edits (a step held + a knob, SEQ > AUTOMATION (1.1.5's AUTO LIST)) go through its UNDO; `MOTION` ops
    3..7 do not (the protocol is unchanged in 1.1.5).
- **Projects (FUN7).** 3388 bytes, little endian, in the same A/B sectors as before: `46 55 4E 37` ("FUN7"),
  size u32 (3388), the 27 globals as i16 (bytes 8..61), sel, parts, phys (62..64), P_COUNT as stored (byte 66),
  then from byte 68 for each of the 4 tracks: P_COUNT bytes (value + 64), engine, preset, 64 steps of 9 bytes
  (4 notes; n | time << 3 | flags << 5; vel; hit; acc; chance byte where 0 = 100 %, 1..100, 101 = never; firmware
  with `52 01`: the ratchet − 1 in bit 7 of vel (bit 0) and of the chance byte (bit 1), see "Ratchet");
  then the song chain, then the motion (260 bytes: count, on mask, 2 reserved, 64 × (track << 6 | step, id,
  i16 value); 1.1: bit 7 of the id byte marks a parameter lock, see "Parameter locks"); the reserved tail is zero up to byte 3371; bytes 3372..3383 are the project's name (since
  1.0: ASCII 32..126, upper case, 0-padded; all zero = no name, as firmware before wrote them; a byte
  outside 32..126 reads as no name, it never refuses the project); the last 4 bytes are an FNV-1a hash of all
  before (the name included). The device names projects itself (SAVE > PROJECT, NAME); the editor's PROJECT
  save keeps the current name, and backups carry it as part of the 3388 bytes. FUN6..FUN1 load with no name
  and are bounded to today's ranges. A count smaller than P_COUNT is mapped as for user presets, and so are the
  motion events' ids: a FUN7 of 89 parameters has its engine parameters' events at 81..88, which load as 83..90
  (the ids below its P_E0 stay). 68 + 4 × (91 + 2 + 576) + chain + motion = 3040 bytes: 332 to spare.
- **Projects (FUN8, with FM6).** FUN7 laid out the same way, `46 55 4E 38` ("FUN8"), size 3584: the data up to
  byte 3040 as above (16 bytes to spare), the four tracks' FM6 patches as packed 128-byte records (see "FM6
  patches") at bytes 3056..3567, the name at 3568..3579, the hash last. FUN7 (3388) and older load with the
  init patch on every track. Backups, `PROJECT` and the editor's project files carry the 3584 bytes.
- **Projects (FUN9, since 1.1).** FUN8 laid out the same way, `46 55 4E 39` ("FUN9"), size 3648: P_COUNT 99 made
  the data 68 + 4 × (99 + 2 + 576) + chain + motion = 3072 bytes, which no longer fit before FUN8's patches. The
  FM6 patches at 3120..3631 (48 bytes to spare before them), the name at 3632..3643, the hash last. FUN8 (3584,
  91 parameters) loads mapped by count (the lane levels 127, the engine values and their motion events and locks
  from 83..90 to 91..98, a lock's bit 7 kept); FUN7 and older as before. Firmware 1.0.x refuses a FUN9 (its size, magic and P_COUNT). Backups,
  `PROJECT` and the editor's project files carry the 3648 bytes; a backup `PUT` takes 3648, 3584 or 3388.
- **Projects (FUN10, since 1.2).** FUN9 laid out the same way, `46 55 4E 41` ("FUNA", FUN10 in these notes), size
  3840 (a whole storage object): P_COUNT 104 at byte 66 and 104 bytes per track (103 in builds before SPREAD: loaded by count); each step's nudge (−8..+7, two's
  complement in 4 bits) in bit 7 of its four note bytes (bit j of the nudge in note j; every older format wrote them 0);
  the motion block 388 bytes: count, on mask, 2 reserved, 128 × (track << 6 | step, id (bit 7 a lock), value as a signed
  byte −64..127); the song's chain is its sections (84 bytes, see "1.2: song sections"). 68 + 4 × (104 + 2 + 576) + chain + motion = 3268; the FM6 patches at 3312..3823 (44 bytes to spare
  before them), the name at 3824..3835, the hash last. FUN9 (3648, 99 parameters, 64 records of 4 bytes) loads mapped by
  count (LFO 2 off, QUANTIZE ON, SPREAD 0, the engine values and their motion from 91..98 to 96..103), every nudge 0; FUN8 and
  older as before. Firmware before 1.2 refuses a FUN10 (its size and magic), so an editor must not send one there
  (`BACKUP_PUT` begin answers rc 1 for its size). Backups, `PROJECT` and the editor's project files carry the 3840 bytes.
- **MIDI clock** has no SysEx. `G_CLOCK` selects the source: 0 INT, 1 USB, 2 TRS. With 1 or 2 the sequencer
  steps on that port's Clock pulses (the other port's are ignored), Start (0xFA) restarts from step 0,
  Continue (0xFB) resumes and Stop (0xFC) stops; BPM follows the incoming tempo (40..240), and 500 ms
  without a pulse stops the transport. Changing `G_CLOCK` stops it.
- Pitch bend, sustain (CC64), RPN 0 (bend range, ±0..24 semitones), CC120 / 121 / 123 are MIDI only and
  have no parameters, protocol or saved state. The standard CC map (1.1: CC5 7 10 71..75 91 93 94, README "MIDI
  in") sets parameters as a knob does: the editor sees those changes as `CHANGED` / `TRACK_CHANGED` pushes.

## Ratchet

INFO advertises `52 01 4` after the MENU settings tag (`4E 01 count`): a step plays up to 4 times. Firmware without the tag has no
ratchet byte (read every step as 1).

- A NOTE step's ratchet 2..4 plays all of it (its notes and its drum hits) that many times, in equal parts of the
  step as swung, each part retriggered and held for GATE of the part. The chance is rolled once for the step (a
  failed roll: no part). A slide into it glides into the first part; it never slides or ties into the next step. 1
  (the default, what every older pattern holds) is the step as before. TIE and REST steps ignore it.
- `STEP_GET` / `STEP_SET` / `TRACK_STEP`: the byte after the chance, 1..4. The flags byte stays 1 accent, 2 slide in
  both directions; a write without the ratchet byte (an older editor) keeps the step's ratchet.
- On the device: a RATCH row of SEQ > AUTOMATION (1.2; up to 1.1.5 SEQ > CHANCE, KNOB 3), KNOB 4 x1..x4; the piano roll and the DRUM grid draw a
  ratcheted step in its parts. Changing it pushes `STEP_CHANGED`.
- Saved in projects (FUN7 / FUN8 layout unchanged: bit 7 of the velocity byte is the ratchet − 1's bit 0, bit 7 of
  the chance byte its bit 1; 0 in every older project, which load x1), so also in backups and song rows. Firmware
  before the ratchet refuses a project that holds one. A user preset's note pattern keeps it in its flags (8 | 16);
  a drum grid record has no room for it and loads x1.

## Parameter locks

INFO advertises `4C 01 1` after the ratchet tag (`52 01 4`). Firmware without it has no locks.

- A **lock** is a motion record (the same 64 shared by the four tracks, one per track, step and id, of either kind)
  whose value sounds on its step only. When the step plays (its chance passed; a step that does not play applies
  none of its locks) the value is set at the step's start, before its notes, and lasts the whole step (its ratchet
  parts too); the next step puts back what sounds without it: the latest automation event of that id at a step up
  to that one in this pass, else the sound's own value. An **automation event** (what the device records with REC
  armed) sets its value at its step and holds it, as before. The ids that can be locked are the ones motion can
  record (`motion_param`, see above); AUTOMATION's PLAY OFF bypasses locks too, its CLEAR clears them.
- `MOTION` 5 sets a lock (track, 5, step, id, v14): as op 3, the value inside the parameter's range for the track's
  engine, rc 2 when the 64 are used. A lock and an automation event on the same step and id are one record: op 3
  over a lock makes it an automation event, op 5 over an automation event makes it a lock. Op 4 deletes either
  kind. Op 6 (track, 6, step) removes that step's locks (step 127: every step's), its automation events stay.
  Op 7 (track, 7) is the query. Ops 5..7 reply as the query and then one kind byte per record, in the same order
  (0 automation, 1 lock); the query and ops 1..4 reply as before 1.1, where a lock reads as an automation event.
- In projects (FUN9; the motion block is laid out as in FUN7 / FUN8) a lock is a motion record with bit 7 of its id
  byte set (ids are below 128: P_COUNT is 99). Every project before 1.1 has none; a FUN8 / FUN7 of a 1.1
  development build may hold some, and on load their ids move to today's positions (83..90 to 91..98 for a FUN8)
  with bit 7 kept. Firmware before 1.1 refuses a project that holds a lock (an id out of its range), as it does a
  newer format; so do backups and song rows.
- On the device: on SEQ > STEP hold a step and turn KNOB 1..4. The knobs lock the four parameters of the sound
  page shown last before (HOME's four, ENV, LFO, FX, EDIT 1 / 2, ..; the cards show them while a step is held). On
  the DRUM grid a step is its white key (several held: each gets the lock; a key pressed on a hit takes it away
  only when let go without a knob turned); on the piano roll the step being entered (its note keys held). [EDIT]
  tapped with a step held clears its locks; EDIT alone clears the step and its locks. The roll and the grid mark
  the steps that hold a lock; AUTOMATION shows the count (LOCK).

## 1.2: LFO 2, NUDGE, 128 records (FUN10)

INFO advertises `41 01 00 01` (the motion capacity, 128 as two 7-bit bytes, LSB first) and `54 01 16` (a step's nudge,
in 1/16 of a step) after the locks tag (`4C 01 1`). Firmware without them: 64 records, no nudge byte, P_COUNT 99.

- **Parameters** (from `INFO` / `DESC` as always): 91 `SYNC` (enum OFF 4BAR 2BAR 1/1 1/2 1/4 1/8 8T 1/16 16T 1/32,
  default OFF: one LFO cycle per note value of the tempo, internal or the external clock's), 92 `TRIG` (NOTE FREE,
  default FREE since DUELL 0.2.2 (it was NOTE): NOTE restarts at PHS on a fresh phrase, FREE never), 93 `POL` (BI UNI, default BI: −1..+1, or 0..+1 with the
  same depths reaching as far, one way), 94 `QNTZ` (OFF ON, default ON: every step on its start; OFF plays the
  nudges). The defaults play as before 1.2 (except TRIG, FREE since DUELL 0.2.2). `SYNC` `TRIG` `POL` are the sound's (a sound load sets them; user presets
  store them); `QNTZ` is the track's (like LEN DIV SWING GATE: a sound load keeps it; an editor's audition must not
  send it, as for the other track-own ids). None of the four can be recorded as motion (`MOTION` 3 / 5 answer rc 1)
  and no MIDI CC sets them. 95 `SPRD` (0..127, PCT, default 0: every voice on PAN, as before): a POLY / UNISON
  track's voices alternate left and right of PAN by SPRD / 2 (127: hard left / right); MONO and LEGATO ignore it. It is
  the sound's (a factory preset sets 0, user presets store it) and can be recorded as motion. The engine parameters are
  96..103. ANALOG's `WAVE` (E0) gained 5 SYNC (OSC 2 hard-synced to OSC 1, DTN its interval, the SHP routings sweep it)
  and 6 SUB (OSC 2 a square an octave below, MIX its level); `DESC` keeps DTN / MIX's labels (the device relabels them).
- **Nudge:** the byte after the ratchet in `STEP_GET` / `STEP_SET` / `TRACK_STEP` (a change on the device pushes
  `STEP_CHANGED`), nudge + 8 (0..15:
  −8 = half a step early .. +7 = 7/16 late, 8 = on the step). A write without it keeps the step's. The device records it
  live and sets it on SEQ > AUTOMATION's NUDGE rows (and STEP, steps held + PRESETS); it plays with the track's `QNTZ` OFF
  (on the device: AUTOMATION's QUANTIZE row; not for a ratcheted step: its parts divide the step).
- **Motion:** 128 records shared by the four tracks, both kinds. Ops 1..7 reply as before 1.2 (`max` 64, at most the
  track's first 64 records in store order: an editor of before keeps working while a track holds no more); op 8
  (track, 8) answers every record with count and max as two 7-bit bytes and the kinds. A set when the 128 are used: rc 2.
- **Projects** are FUN10 (above); user presets store 104 parameters (1.5: 111; UP_GET / UP_PUT: P_COUNT values, as INFO says).

## 1.5: the INSERT

Five track parameters before the engine parameters, ahead of 1.5's TYPE and ESYNC (below; with them P_COUNT 111, P_E0
103; take both from `INFO` as always):

| id | label | values |
| --- | --- | --- |
| 96 | INSRT | enum (append-only): 0 OFF, 1 SOFT, 2 HARD, 3 FOLD, 4 FUZZ, 5 CRUSH, 6 PHASR, 7 FLANG, 8 CHOR; default OFF |
| 97 | INS A | 0..127, default 64: SOFT .. FUZZ DRIVE (PCT), CRUSH BITS (1 + v / 8: 1..16), PHASR / FLANG / CHOR RATE (LFOHZ) |
| 98 | INS B | 0..127, default 96: SOFT .. FUZZ TONE (CUTOFF), CRUSH RATE (v / 8 picks 689 919 1.1k 1.4k 1.8k 2.2k 2.8k 3.7k 4.4k 5.5k 7.4k 8.8k 11k 15k 22k 44k Hz), the swept ones DEPTH (PCT) |
| 99 | INS C | 0..127, default 96: SOFT .. FUZZ LEVEL (DB, as LVL: 112 = 0 dB), CRUSH LPF (CUTOFF), the swept ones FDBK (PCT) |
| 100 | MIX | 0..127 (PCT), default 127: dry / wet; 0 = the dry track, bit for bit |

`DESC` gives the generic labels and formats above (`INS A` .. `INS C`, PCT): the meaning of 97..99 depends on 96, as
the engine parameters' on the engine; an editor that shows them by TYPE uses the table (the device does:
`params.c ins_desc`). All five are the sound's (a factory preset sets OFF and the defaults, a user preset stores them,
an audition sends them) and can be recorded as motion and locks (`MOTION` 3 / 5: rc 0). With TYPE OFF (or MIX 0) a
track plays exactly as before 1.5. Older stores load by count: a FUN10 of 104 (1.2 .. 1.4) or 103 parameters, FUN9 and
older, user presets of 104 and fewer: the INSERT OFF, their engine values and motion moved up to 103..110. Firmware
before 1.5 shows a FUN10 of 111 parameters as EMPTY and refuses it in a backup `PUT` (its P_COUNT is larger than its
own); a user preset record of 111 already in its flash it maps by count (the INSERT dropped, the engine values
found). `UP_PUT` takes the device's own P_COUNT values, as always.

## 1.2: song sections

INFO advertises `57 01 4` (after `54 01 16`): `SONG` (33) has ops 4..7, ops 0..3 with sections instead of rows. A section
names the PROJECT slot each of the four tracks plays (0..3 = A..D, 4 = "-": the track plays nothing there) and its
repeats (1..16); up to 16 sections.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 33 SONG | op 4 query; op 5 set, count 0..16, count × (slot T1, slot T2, slot T3, slot T4 (0..4), repeat 1..16); op 6 start; op 7 stop | op (4..7), rc, count, running 0/1, section 0..15, remaining repeats, count × (slot T1..T4, repeat) |

A set must have exactly `2 + count × 5` argument bytes; rc as for ops 0..3 (1 = invalid sections, 2 = busy, 3..6 = a
slot a lane plays is empty; a silent lane needs none, a song of "-" only plays silence). Ops 0..3 keep their meaning: a
set (op 1) makes each row a section with the four tracks on its slot (a slot of 4 there is invalid, rc 1); their replies
list a row per section, the slot of its clock lane (below; every lane "-": 0) and its repeats, so an editor of before
sees the song's order but not its lanes, and a set from it makes the song lane-less again.

Playback: each track reads its own slot's steps (CHANCE, RATCH, NUDGE), automation and locks, LEN / DIV / SWING / GATE
and QUANTIZE, for that track only; the sounds stay current. Sections keep one boundary for all four tracks: a section
lasts `repeat` loops of its clock lane, the first track not "-" there (track 1 when it plays: every song of rows counts
as before); every lane "-": a 4/4 bar (16 × 1/16). At each section all four start together from their step 1. A "-"
lane releases its sequenced notes at the section's start and its automation goes back to the sound's own values; live
keys still play it. While a song plays, `SET` / `TRACK_PARAM` writes to LEN / DIV / SWING / GATE and QNTZ (94) are
ignored (the slots set them; STOP restores the editable values).

Projects (FUN10) store the sections: 4 + 16 × 5 = 84 bytes (count, 3 reserved, then per section the four slots and the
repeats) right after the tracks' steps, before the motion: 68 + 4 × (104 + 2 + 576) + 84 + 388 = 3268, 44 bytes to spare
before the FM6 patches at 3312. FUN9 and older store rows {slot, repeat} (36 bytes); they load as sections with the four
tracks on the row's slot: the same song.

## 1.5: filter TYPE, ENV SYNC

No new command or INFO tag: P_COUNT 111 and P_E0 103 (from `INFO`, with the INSERT) say it, and `DESC` describes the two as always.

| id | label | values |
| --- | --- | --- |
| 101 | `TYPE` | enum LP BP HP, default LP. ANALOG's filter (Discussion #104): the state-variable filter's low-pass (as before, bit for bit), band-pass or high-pass output, CUT / RES / KTR and every cutoff routing as they are. Other engines ignore it. On the device: EDIT > FILTER (TYPE CUT RES KTR), an ANALOG track's page after EDIT 2 |
| 102 | `ESYNC` | enum OFF ON, default OFF. ENV SYNC (Discussion #175): ON, the amp envelope's ATK DEC REL (ids 1, 2, 4) are note values of the tempo (BPM, or the external clock's measured tempo) instead of 1 ms .. 10 s. On the device: ENV DEST's KNOB 4 |

Both are the sound's (a factory preset or TOOLS INIT sets LP / OFF; user presets and library files store them). Neither can
be recorded as motion (`MOTION` 3 / 5 answer rc 1) and no MIDI CC sets them.

With `ESYNC` ON the stored values of ATK DEC REL stay 0..127; `DESC` still describes them as times (TIME), but they mean a
note value: value v is name `ESYNC_NAMES[v × 25 / 128]` (integer division) of the 25, shortest first:

`0` `1/64T` `1/64` `1/32T` `1/64D` `1/32` `1/16T` `1/32D` `1/16` `1/8T` `1/16D` `1/8` `1/4T` `1/8D` `1/4` `1/2T` `1/4D`
`1/2` `1/1T` `1/2D` `1/1` `1/1D` `2BAR` `3BAR` `4BAR`

(T a triplet, D dotted; in 1/384 of a 4/4 bar: 0 4 6 8 9 12 16 18 24 32 36 48 64 72 96 128 144 192 256 288 384 576 768 1152
1536; the time is that × a quarter / 96.) `0` is the shortest time, as ATK / DEC / REL 0 with SYNC OFF. The device shows
these names and its knob steps one name a detent, landing on the first value of each: `(k × 128 + 24) / 25` for name k
(params.c `param_turn`). An editor should show and step them the same way while the track's `ESYNC` is ON (and keep the
raw value when it goes OFF: the same number is a time again). The attack reaches the top after that time, the decay
(to SUS) and the release fall 40 dB in it.

Projects: FUN10 unchanged; one written by 1.5 holds 111 at byte 66 (16 bytes to spare before the FM6 patches, room for 4
more). A FUN10 of 104 (1.2 .. 1.4) loads by count (the INSERT OFF, TYPE LP, ESYNC OFF; engine values and their motion
moved up by seven). Firmware 1.2 .. 1.4 refuses a FUN10 of 111 (more parameters than it has: its slot shows EMPTY, a
backup PUT of one answers rc 1); user presets of 111 load there by count (the seven left out). Library files of 104 map
by label: the INSERT, TYPE and ESYNC unset.

## v7: full backup (65-67)

INFO advertises `42 01 caps`: bit 0 = `BACKUP_LIST` / `BACKUP_GET` (read), bit 1 = `BACKUP_PUT` (restore);
this firmware sends 3. Requests name objects, never flash addresses.

| id | object | size |
| --- | --- | --- |
| 0 | runtime: the music being played now, as a FUN10 project (1.1: FUN9, 3648; 1.0.x: FUN8, 3584; firmware before FM6: FUN7, 3388) | 3840 |
| 1 | settings (palette, speaker, HOLD time, favorites, panel calibration, ...) | the settings record's size |
| 2..5 | PROJECT slots 1..4 (FUN10; 1.1: FUN9, 3648; 1.0.x: FUN8, 3584) | 3840, or 0 if empty |
| 6, 7 | user preset banks (slots 1..16, 17..32) | the bank's size, or 0 if empty |
| 8 | the FM6 patch bank of 1.0..1.0.2 (B1..B27). Since 1.0.3 always listed empty (see below) | 3472, or 0 if empty |
| 9 | the user presets' FM6 patches (1.0.3; `up_fm6.c`: per slot a tag and the packed patch) | 3728, or 0 if none |
| 32..34 | user sample slots 1..3 (DUELL 0.2.9: retired, always listed empty; the archive keeps its ids) | 0 |

Reading: `BACKUP_LIST` (no arguments) stops the transport, then takes a snapshot of the runtime object and
answers `1, rc, count` (13 since 1.0.3, 12 with FM6 up to 1.0.2, 11 before) and, per object in the order above, `id, size u32, crc u32` (CRC-32, zlib). The other objects are read as
they are in RAM or flash. Then `BACKUP_GET` reads an object in pieces: `id, offset u32, count lo, count hi`
(count 1..256, LSB first 7 bit pair) answers `id, rc, offset u32, count lo, count hi` and the data as pack7. Check each object's CRC
against the list; if it differs the device changed, so start again.

Restoring: `BACKUP_PUT` takes ids 0..9 (firmware before 1.0.3: 0..8, id 9 answers rc 1 at begin and nothing is written) (the samples were written with `SMP_BEGIN` / `SMP_WRITE` / `SMP_END`, or `SMP_ERASE` for an empty slot; since DUELL 0.2.9 an
empty slot's `SMP_ERASE` answers rc 0 and a slot with data gets rc 6 at `SMP_BEGIN`: the web editor's restore then skips it and
restores the rest). Begin: `0, id, size u32, crc u32`: size is 3840 (FUN10), 3648 (FUN9), 3584 (FUN8) or 3388 (FUN7, FUN6) for id 0,
the settings record's size for id 1, 3840, 3648, 3584, 3388 or 0 (empty the slot) for ids 2..5, the bank's size or 0 for 6 and 7,
3472 or 0 for 8 (the FM6 bank: its magic, layout and every byte below 128 are checked), 3728 or 0 for 9 (its magic,
version and slot count checked; 0 clears the patches). FUN9, FUN8, FUN7 and FUN6 become FUN10 (1.2; firmware before
1.2 refuses a 3840-byte project at begin, rc 1).
An archive without id 8 (written before FM6) still restores; the web editor reads all three kinds (11, 12, 13 objects).
Since 1.0.3 an id 8 with data (an archive of 1.0..1.0.2) is not stored: like the first boot after the update, every FM6
user preset whose stored SLOT is a B slot (8..34) and has no patch yet gets that bank slot's patch, and id 9 is written.
So restore ids 6 and 7 before 8 (the web editor does); an empty id 8 is taken and ignored (rc 0). Old archives
therefore restore on both old and new firmware; a 1.0.3 archive on older firmware loses only id 9 (the web editor
skips it when the device answers rc 1). Data: `1, id, offset u32, pack7` with the next offset (they must follow each other)
and at most 256 decoded bytes (256-byte pieces are fine at full speed, one request at a time: see "Notes for
the editor"; a piece that gets no reply was not taken: abort and begin the object again). Commit: `2, id`: the device checks the length and the CRC, validates the
content, and then writes. Abort: `3, id`. Id 0 replaces the music now playing (RAM only, no flash);
ids 1..9 are written to flash (settings and presets are applied too). Nothing is written before the commit.
The web editor sends ids 2..7 and the samples first, then 1, then 0 last. A failed restore can leave
earlier objects restored; the file is still the source.

| rc | Meaning |
| --- | --- |
| 0 | ok |
| 1 | invalid: arguments, id, size, offset, count, or an id `BACKUP_PUT` does not take |
| 2 | validation failed: CRC, length or content at commit (and a runtime object that cannot be packed) |
| 3 | stop playback first (`LIST` and `PUT` stop it themselves, and answer 3 if it does not stop in 100 ms; `GET` answers 3 while it plays) |
| 4 | flash write failed |
| 5 | stale: no `LIST` yet, the USB bus was reset, 15 s with no `PUT` request, or a project save / load reused the staging RAM (for `GET`, of id 0 only). Start again with `LIST` / `BACKUP_PUT` begin |

`BACKUP_LIST` answers `1, rc, 0` when rc is not 0. `GET` and `PUT` answer id 127 when they got no arguments.
A `LIST` replaces the snapshot, and a `PUT` begin ends it: a `GET` after a begin gets rc 5.

## FM6 patches (68-71)

The FM6 engine (12) plays a 6-operator patch per track; its eight EDIT parameters are macros on top of it
(ALG 0 = the patch's algorithm, 1..32 another; FB, MLVL, MRAT, MEG, VMOD offsets; DTUN; SLOT). The patch itself
only travels through these commands. INFO advertises `46 01 nfactory nbank` after the backup tag (1.0.3:
`46 01 08 00`; 1.0..1.0.2: `46 01 08 1B`); firmware without it has no FM6 and does not answer 68..71.
After it, `53 01 caps` (live sync, v2 above): bit 0 = `WATCH` while watching keeps what is not pushed yet,
bit 1 = no `RELOAD` after the editor's own `PRESET` / `SET` of G_ENGSEL. This firmware sends 3.
Then (1.0.3) `50 01 caps`, FM6 v2: bit 0 = no patch bank (SLOT is F1..F8 = 0..7 and 8 = OWN; the bank target
answers rc 3), bit 1 = user presets carry their patch (target 3; backup id 9). This firmware sends 3. Firmware
without the tag has the bank and SLOT 0..34 (F1..F8, B1..B27): an editor should not offer the bank either way.

SLOT (P_E0 + 7), since 1.0.3: 0..7 load that factory patch into the track; 8 (OWN) is the track's own patch (what
a project, a user preset, a converted DIGITAL sound or an FM6_PUT to the track put there). Every such load sets SLOT
to F n when the patch is that factory patch unchanged, else OWN, and nothing reloads over it. Setting SLOT from OWN
to F n keeps the own patch aside; setting it back to OWN brings it back. 1.4.1: 9..40 (B1..B32) load a voice of
the voice bank (below) the same way; a `SET` to a bank slot without a voice is refused (SLOT goes back to the value
it had: a `GET` or the `CHANGED` push shows it), and the device's knob passes over such slots. SLOT is never stored as
a bank slot: a stored 9..40 (in a project, a user preset or a motion record; 1.0.2 wrote 8..34 for its B1..B27) loads
as 8, OWN, with the patch that came with it (DESC: SLOT max 40; firmware before 1.4.1: 8).

Since 1.2 the device edits the patch too (EDIT > OPERATOR, OP ENV, OPERATOR 2: an operator's coarse / fine
frequency, output level, mode, detune, velocity sensitivity, R1..R4 and L1..L4). Each such edit writes that byte of
the track's patch and sets SLOT to 8 (OWN), which the editor sees as any SLOT change; the patch it holds for the
track may then be stale: read it again with `FM6_GET` (target 0). Nothing new on the wire: no command, parameter
id or INFO tag (the operator values are the patch, not parameters: no `SET`, no automation).

A patch is the 128-byte packed record of the generic 6-operator voice (the 32-voice bank's record; every byte is
7-bit, so it travels as it is, no pack7). Operators come sixth first: per operator 17 bytes (R1..R4, L1..L4,
break point, left / right depth, curves `LC | RC << 2`, `RS | DET << 3`, `AMS | KVS << 2`, output level,
`MODE | FC << 1`, fine), then pitch EG rates and levels (102..109), algorithm 0..31 (110), `FB | OKS << 3`,
LFO speed, delay, PMD, AMD, `SYNC | WAVE << 1 | PMS << 4`, transpose (24 = none), the name (10 ASCII bytes).
The device stores every value clamped into its range.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 68 FM6_GET | target, index | target, index, rc, then (rc 0) the 128 bytes |
| 69 FM6_PUT | target, index, the 128 bytes | target, index, rc |
| 70 FM6_LIST | — | nfactory, nbank, then per slot (factory first): used (0/1), name string ("" if empty); 1.0.3: nbank 0, the factory patches only |
| 71 FM6_ERASE | bank index | index, rc (1.0.3: 3, no bank) |

target:
- 0 a track's own patch (index 0..3: what it plays and what its project saves). A PUT is heard at once; SLOT becomes
  OWN (F n if it is that factory patch unchanged), and the device does not reload a factory patch over it.
- 1 the patch bank. 1.0..1.0.2: index 0..26 = B1..B27 in flash (A 0x9F000, B 0xFE000; backup id 8). Since 1.0.3:
  GET, PUT and ERASE answer rc 3 ("no bank") and change nothing; LIST reports nbank 0.
- 2 a factory patch (0..7, GET only).
- 3 a user preset's patch (1.0.3; index 0..31 = U01..U32). It is kept beside the record and counts only for an FM6
  record as it was when the patch was stored (a rename keeps it; an `UP_PUT` that changes the record, `UP_ERASE` or another store
  drops it). `UP_STORE` / SAVE of an FM6 track stores the track's patch with it; `UP_LOAD` plays it. GET: rc 2 when
  the slot has none (not an FM6 sound, or stored without one: it then loads SLOT's factory patch, or the init voice
  for OWN). PUT (after an `UP_PUT` of the record): rc 1 when the slot is not a used FM6 record; it writes flash (stops
  the transport, allow 1 s).

rc: 0 ok, 1 arguments (an unknown target, an index out of range, a record that is not 128 bytes), 2 empty (GET) or a
flash error / transport that did not stop (PUT), 3 no bank (1.0.3, target 1 and ERASE).

The web editor (6-OP FM tab) imports the generic SysEx files of the format: a single voice `F0 43 0n 00 01 1B`, the
155-byte unpacked voice, checksum, `F7` (163 bytes), and 32 voices `F0 43 0n 09 20 00`, 32 x 128 packed, checksum,
`F7` (4104 bytes); the checksum is the two's complement of the data's sum, 7 bits. Raw 155 / 4096-byte files are
read too. Pick a voice, edit it, send it to a track (target 0); to keep it, SAVE it as a user preset (or save the
project) on the device. It exports single voices. Its librarian reads and writes a user preset's patch (target 3)
with the record when the firmware has FM6 v2 bit 1, and keeps it in library files as `fm6` (128 numbers).

## FM6 voice bank (74-77; 1.4.1)

32 voices in flash, written whole from the editor (a 32-voice SysEx file: its 128-byte packed records, the format
`FM6_PUT` takes), picked on the device with SLOT B1..B32 (9..40). Loading one copies it into the track's patch as F1..F8
do: the patch is the track's from then on, saved with projects and user presets (`FM6_GET` target 0 reads it), so a
project made with a bank voice sounds the same after the bank was replaced. INFO advertises `56 01 32` (32 voices)
after `57 01 4`; firmware without the tag answers none of 74..77 and has SLOT 0..8.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 74 FM6B_BEGIN | — | rc. Starts a transfer (stops the transport; erases a sector: allow 1 s) |
| 75 FM6B_WRITE | index 0..31, the 128 bytes | index, rc. Each index at most once per transfer; voices not written are empty in the new bank |
| 76 FM6B_END | name (10 ASCII bytes, space-padded), CRC-32 (zlib's) of the written records, 128 bytes each in index order (5 × 7 bits, LSB first: bits 0..6, 7..13, .., 28..31) | rc, voices (how many the bank the device holds now has). rc 0: the new bank replaced the old one (allow 1 s) |
| 77 FM6B_LIST | — | 32, bank (1 there is one, 0 none), its name string, then per voice: used (0/1), name string ("" if empty) |

rc: 0 ok, 1 arguments (an index past 31 or written twice, a record that is not 128 bytes, an END that is not 15
bytes), 2 the CRC did not match (nothing changed: the old bank stays; start again with `FM6B_BEGIN`), 3 playback did
not stop, 4 flash (no flash, an erase or a write failed), 5 no transfer (no `FM6B_BEGIN`, an END already answered, or
the device's autosave took the transfer's flash between two requests, after 10 s without one: start again).

- An END with no WRITE (the CRC of nothing: 0) writes an empty bank: every B slot is empty.
- The old bank stays until END answers rc 0; a power cut at any point leaves the old bank or the new one, never a mix.
- A track on a bank slot when a new bank arrives keeps the voice it played, as its own patch: SLOT becomes OWN.
- The editor's import (6-OP FM tab): a 32-voice file's voices, each packed as for a single voice (every value into
  its range); an empty slot of a shorter file is not written. Flash: the bank is at 0xE7000; a transfer is staged in
  the autosave's older sector first (0xE5000 / 0xE6000), which the next autosave writes over anyway.

## MIDI LEARN (78-79; 1.5)

The device can set a controller's CC to any knob's parameter of a track (Discussion #170): GLO + D4 turns MIDI LEARN
on, a knob turned picks its parameter on the selected track (GLO's KNOB 1..4: T1..T4 LEVEL), the next CC that comes is
set to it. Up to 16 CCs, each to one parameter of one track (0..3), whatever channel it comes on (a channel MIDI IN /
ROUT ignores stays ignored) and whichever track that channel plays; a CC 0..127 over the parameter's range, as a knob
(and as AUTOMATION records a knob). A learned CC no longer reaches the standard CC map (CC74 learned is no longer
T1's CUT on channel 1). One CC sets one parameter and a parameter has one CC: learning either again replaces the old
entry. Never learned: CC0, 1, 6, 11, 32, 38, 64, 96..101, 120..127 (their meaning: bank select, MODW, RPN data, EXPR,
the pedal, data increment / NRPN / RPN, channel mode). The map is part of the settings (kept across power-off, not in
projects or user presets; a full backup carries it with the settings object); empty in every older setting. An engine
parameter is learned by its place (P_E0 + k): on a track that changes engine the CC sets that engine's k-th value.
INFO advertises `43 01 16` (16 entries) after `56 01 32`; firmware without the tag answers neither command. The
device's MENU > MIDI > LEARN CLEAR (OCT+: every entry cleared) is an action, not a setting: `MENU_DESC` does not offer
it (the MENU settings' count stays); `LEARN_SET` op 2 does the same.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 78 LEARN_GET | — | 16, then 16 × (used 0/1, CC, track, parameter id); an empty entry `0 0 0 0` |
| 79 LEARN_SET | op 0 learn: 0, CC, track, parameter id; op 1 clear a CC: 1, CC; op 2 clear all: 2 | rc, then LEARN_GET's reply |

The parameter id is this firmware's (`P_*`, P_E0 from `INFO`). rc: 0 applied and saved, 1 arguments (an unknown op, a
CC that is never learned or past 127, a track past 3, an id past P_COUNT − 1), 3 applied in RAM only (no flash, a
failed write), 4 applied, saved when the transport stops, 5 full (16 others: clear one first). Op 1 on a CC not
learned, op 2 on an empty map: rc 0, nothing changes. The device does not tell the editor when it learns: read the map
again (LEARN_GET) when it matters (e.g. with the settings, or after the user says so). Example: `F0 7D 46 4C 4F 00 4A
03 62 F7` (op 0: CC74, T4, id 98: P_E0 + 2, DRUM's TONE, where P_E0 is 96 as in 1.2 .. 1.4.1) answers `F0 7D 46 4C 4F 00 10 01 4A 03 62 ...
F7` (rc 0, 16 entries, the first CC74 on T4 id 98).

## ERASE ALL (80; DUELL 0.2.2)

The same erase as the device's MENU > SYSTEM > ERASE DATA, for the installer's optional "Erase my saved data after installing".
It erases the settings (theme, scale, MIDI channels and learn map, favourites, preferences, panel table), the autosave, the patch
store (every stored patch and factory shadow copy), the multis and the pattern slots; the FM6 voice bank and the firmware stay
(the user sample slots are gone since 0.2.9; their flash is not touched). The unit then restarts by itself about 0.7 s after the reply (USB drops and comes back) and starts as a
new unit (VIOLET, LAYER, factory multi M01). Order and power cuts: see `src/erase_data.c`; the settings go last, each object is
erased copy by copy, so a cut leaves every object whole or empty and a unit that still boots. Not advertised in INFO (id 80 is the
next free id; all earlier ids are unchanged); firmware without it does not answer, so a client treats a timeout as "not erased".

| cmd | Request args | Reply args |
| --- | --- | --- |
| 80 ERASE_ALL | the nine ASCII bytes `45 52 41 53 45 20 41 4C 4C` ("ERASE ALL") and nothing else | rc |

rc: 0 erased (the restart follows), 1 refused: any other payload or length (nothing touched), 3 the transport is running
(nothing touched; the op does not stop it), 4 the flash failed (some objects may be gone, the settings are not: send it again).
Allow up to 5 s for the reply (up to 38 sectors are erased, about 45 ms each, skipping the empty ones). Example:
`F0 7D 46 4C 50 45 52 41 53 45 20 41 4C 4C F7`.

## WAVE (engine 14; DUELL 0.2.5)

The wavetable engine (firmware `src/eng_wave.c`, SPEC.md 2.6). `INFO` lists its name `WAVE` as engine 14; `DESC` (scope 0) of
`P_E0..P_E7` on a track playing it gives:

| E | id (`P_E0` + E) | label | fmt | range, default | meaning |
| --- | --- | --- | --- | --- | --- |
| 0 | 119 | TABLE | 8 ENUM | 0..25, 0: SAWSQ PWM HARM ODD ORGAN VOWEL BRASS BOWED RESO COMB DIGI GLASS CHIME ROUGH BAND DUAL THIN PIPE REESE HOOVR GROWL WOBBL YELL METAL STEPS AIR (append-only; 14..25 since 0.2.7) | which table; a change builds the table: the track's notes fade out, are silent for the build (a few hundredths of a second), fade in |
| 1 | 120 | POS | 1 PCT | 0..127, 0 | the place in the table's scan |
| 2 | 121 | BEND | 1 PCT | 0..127, 0 | the read's warp (0: none) |
| 3 | 122 | SYNC | 1 PCT | 0..127, 0 | hard sync, 1 .. 8 x (0: none) |
| 4 | 123 | SUB | 1 PCT | 0..127, 0 | a sine an octave below |
| 5 | 124 | DTN | 0 INT, unit `ct` | 0..127, 0 | a second read DTN cents up (0: none) |
| 6, 7 | 125, 126 | `-` | 0 INT | 0..0 | unused |

The multimode filter (45..48: CUT RES MODE SLOPE, 0.2.9) applies to WAVE (as to PHASE). The SHP depths (ENV 1 / ENV 2 / LFO 1 / LFO 2
DEST SHP, the matrix's SHP) move POS. Its factory sounds are listed by `FAV_GET` / the browser under engine 14 (factory index
0..10). Nothing else in the protocol changed: no new command, no new id; only NENGINES (15) and the engine-name list in `INFO`.

## Tagged device preferences v1

INFO appends `0x55, 1, uiCaps` after the existing `NTRK, CHAIN_ROWS` bytes.
Only this complete tagged extension enables commands 34–38. An untagged byte
from an experimental firmware is not a SONG or preference capability.
Command 33 remains SONG. Older editors can ignore the additional INFO bytes.

`uiCaps`: bit 0 palette, bit 1 font weight (retired: firmware since the 1.0 UI has one
weight and no longer sets it), bit 2 reserved for a MIDI monitor, bit 3 favorites. This build
advertises 9 (palette and favorites).

| cmd | Request args | Reply args |
| --- | --- | --- |
| 34 UI_STATE | none | caps, palette, font, monitor, filter, favoriteSig u28, bankSig u28 |
| 35 UI_SET | id, value | rc, id, value, UI_STATE payload |
| 36 UI_PALETTES | none | count, count × name string |
| 37 FAV_GET | engine, start v14, count 1..32 | rc; on success: engine, start v14, count, count × on/off |
| 38 FAV_SET | engine, preset v14, on/off | rc; when applied: engine, preset v14, on/off |

UI_SET ids: 0 palette (0..count-1, the order of UI_PALETTES: GREY GREEN AMBER ICE VIOLET ROSE PAPER
HI-CON NIGHT MONO; 1.0.2: GREY is the MONO of 1.0.1, MONO a new black and white palette), 1 font (retired: rc 2), 2 reserved, 3 preset filter (0 all, 1 favorites). Unsupported state
fields are 127.
Both u28 signatures are four least-significant-first 7-bit bytes; compare them
to refresh changed favorites and user slots. Factory references use stable
engine/preset ids; engine NENGINES denotes a user slot (15 since DUELL 0.2.5, which added engine 14 WAVE; 14 before: take it
from `INFO`, never as a constant). An empty user slot
cannot be marked. Factory DRUM sounds use the same favorite path.

rc 0 = applied and saved; 1 = invalid arguments; 2 = unsupported feature;
3 = applied in RAM but not saved (no flash or a failed write);
4 = applied and queued to save after STOP. A failed settings write is retried
by the same persistence path as the panel. These changes never stop playback.
Unchanged writes do not erase flash. Replies echo preference ids/values and
favorite ranges so the editor rejects replies to a different request.

## MENU settings (72, 73; 1.0.4)

The device describes the settings of its MENU (HOME held) to the editor, which builds its settings from what it
is told: no list of settings is fixed in the editor. INFO advertises `4E 01 count` after the FM6 v2 tag
(`50 01 caps`): `count` items, MENU_DESC index 0..count−1. Firmware without the tag answers neither command.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 72 MENU_DESC | index (0..count−1) | index, id, kind, value v14, min v14, max v14, name string, then for kind 0 (max−min+1) value-name strings, for kind 1 a unit string; then (1.0.5) tab, tab-name string. An index past the list: index, 127 (no more bytes) |
| 73 MENU_SET | id, value v14 | rc, id, value v14: the device's value after the write (clamped to min..max) |

- **index** is the position in the device's list: up to 1.0.5 the menu's order; from 1.1 a new setting is appended
  (index = id), whatever tab it shows in, so an index keeps the setting it had and an older editor lists the ones it
  knew where they were. Shown by tab (below), each tab's items in index order, they appear as on the device. **id**
  names the setting and never changes its meaning: a new setting takes a new id (append-only), wherever it shows in
  the menu. Ids are 0..126; 127 means "no item". Remember settings by id, not by index.
- **kind**: 0 an enum (the value is an index into the names, min..max), 1 a number (min..max, its unit after the
  name; 1.2: TUNE, id 22, -50..50 "ct", the first). An editor shows a kind it does not know read-only, or not at all.
- Values are v14 like the rest of the protocol, so a setting may go past 127 or below 0; up to 1.2's TUNE every value
  was 0..9.
- **rc** as `UI_SET`: 0 applied and saved, 3 applied in RAM only (no flash or a failed write; retried by the
  settings path), 4 applied and saved after STOP; 1 an unknown id (the request's id and value are echoed,
  nothing changes). A value out of range is not refused: it is clamped and the reply says which value the device
  took. A wrong argument length gets no reply. 1.2: MIDI IN (21) and TUNE (22) answer rc 0: they are not settings
  but the current project's `G_ROUTE` and `G_TUNE` (global ids 14 and 3), set as a `SET` of that global sets them
  (a `GET` / `DUMP` reads the same value, the device pushes `CHANGED` for it while WATCHing); they are kept with the
  project (PROJECT SAVE, the autosave), not in the settings record, and a project load brings its own values.
- A MENU_SET is applied exactly as the menu's KNOB 1 / OCT+ applies it (`src/menu_items.c` `menu_put`, shared by
  both), and saved through the same settings record (`settings_save`; unchanged values write nothing; MIDI IN and
  TUNE: the project's, above). If the
  device shows the MENU page, it redraws with the new value. There is no push: an editor that wants to follow
  changes made on the device reads MENU_DESC again (for instance when its settings page opens).
- CALIBRATION, TEST and ABOUT have no value and are not offered. (DUELL 0.2.2: the item that was INFO (1.2) is now TEST, the CPU test screen, present in developer builds only (`DUELL_TEST_MODE=1`; the release has no TEST row); its menu id stays reserved and no other id changes, so nothing in an editor moves.) COLOR (id 0) is the same setting as `UI_SET` id 0.
- **tab** (1.0.5): the MENU shows its settings in tabs (ALGORITHM steps between them on the device). After the
  names (kind 0) or the unit (kind 1) the reply carries the item's tab: its index (0.., the device's tab order,
  left to right) and its name (≤ 12 characters). An editor that wants the device's grouping shows the items by tab,
  in tab-index order, each tab's items in index order; the tab names come from the device, as the item names do.
  Firmware 1.0.4 ends the reply after the names: no tab (show the items as one list). An editor written for 1.0.4
  reads exactly the names and ignores what follows, so the tab changes nothing for it. Tab indexes are not ids: an
  item may move to another tab in a later firmware, and tabs may be added (1.2: MIDI, index 3, came in before SYSTEM,
  whose index went from 3 to 4); remember settings by id and show the tabs by the names the device gives.
  A tab may hold items with no value that are not offered (this firmware: SYSTEM also holds CALIBRATION, TEST (was INFO) and
  ABOUT).
  For a kind it does not know, an editor cannot find the tab (it does not know that kind's bytes).

This firmware (count 25; tabs 0 DISPLAY, 1 CONTROL, 2 AUDIO, 3 MIDI, 4 SYSTEM (1.2; before: 3 SYSTEM); 1.0.5 had the
first 12, 1.1 the first 15, 1.2 before SCALE LEDS the first 16, before SCREEN OFF the first 17, 1.1.5 the first 18, 1.2
before MIDI IN the first 21, 1.2 the first 23, 1.3 / 1.4 the first 24):

| index | id | name | kind | values (min 0) | default | tab |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | 0 | COLOR | 0 | 0..9: GREY GREEN AMBER ICE VIOLET ROSE PAPER HI-CON NIGHT MONO (the order of `UI_PALETTES`) | GREY | 0 DISPLAY |
| ~~1~~ | | ~~STYLE~~ | | retired in DUELL 0.2.2 (FLAT is the only look; a stored LINE is ignored). The id is never reused. | | |
| ~~2~~ | | ~~LARGE~~ | | retired in DUELL 0.2.2 (the LARGE view was removed; a stored ON is ignored). The id is never reused. | | |
| 3 | 3 | ANIM | 0 | 0 ON, 1 OFF, 2 IDLE (1.2: ON plus the LEDs' idle animation; firmware before: max 1) | ON | 0 DISPLAY |
| 4 | 4 | LEDS | 0 | 0 OFF, 1 LOW, 2 MEDIUM, 3 MAX (darkest first, as the menu steps them) | MEDIUM | 0 DISPLAY |
| 5 | 5 | HOLD | 0 | 0 "0.3 s", 1 "0.4 s", 2 "0.5 s", 3 "0.6 s" | 0.4 s | 1 CONTROL |
| 6 | 6 | KNOB ACCEL | 0 | 0 OFF, 1 ON | OFF | 1 CONTROL |
| ~~7~~ | | ~~FX LATCH~~ | | retired in DUELL 0.2.2 (the FX hold layer was removed; a stored ON is ignored). The id is never reused. | | |
| - | 8 | (BPM LOCK: retired in DUELL, id never reused; the indexes after it are their id - 1) | | | | |
| 9 | 9 | SPEAKER EQ | 0 | 0 FLAT, 1 LOWCUT, 2 BASS+ | FLAT | 2 AUDIO |
| 10 | 10 | USB LEVEL | 0 | 0 MASTER, 1 FIXED | MASTER | 2 AUDIO |
| 11 | 11 | USB SERIAL | 0 | 0 ON, 1 OFF | ON | 4 SYSTEM (up to 1.2's MIDI tab: 3) |
| 12 | 12 | CLICK | 0 | 0 OFF, 1 REC, 2 ON (1.1) | OFF | 2 AUDIO |
| 13 | 13 | CLICK LEVEL | 0 | 0 LOW, 1 MID, 2 HIGH (1.1) | MID | 2 AUDIO |
| 14 | 14 | COUNT-IN | 0 | 0 OFF, 1 "1 BAR", 2 "2 BARS" (1.1) | OFF | 2 AUDIO |
| 15 | 15 | RESTORE LAST | 0 | 0 ON, 1 OFF (1.2) | ON | 4 SYSTEM (up to 1.2's MIDI tab: 3) |
| 16 | 16 | SCALE LEDS | 0 | 0 OFF, 1 ON (1.2) | OFF | 1 CONTROL |
| 17 | 17 | SCREEN OFF | 0 | 0 NEVER, 1 "5 MIN", 2 "15 MIN", 3 "30 MIN", 4 "60 MIN" (1.1.5) | NEVER (1.1.5.1; 1.1.5: 30 MIN) | 0 DISPLAY |
| 18 | 18 | SCOPE | 0 | 0 OUT, 1 MIX (1.2) | OUT | 0 DISPLAY |
| 19 | 19 | STEP PREVIEW | 0 | 0 OFF, 1 ON (1.2) | OFF | 1 CONTROL |
| 20 | 20 | CHORD ENTRY | 0 | 0 HOLD, 1 ADD (1.2) | HOLD | 1 CONTROL |
| (21) | 21 | MIDI IN | | **DUELL 0.1: retired, never reused, not offered** (MIDI IN is GLO > CHANNELS K3 on the device; the value is still the project's `G_ROUTE`, global 14: 0 OWN CH, 1 SELECTED, settable as a `SET` of that global) | | |
| 22 | 22 | TUNE | 1 | -50..50, unit "ct" (1.2: the project's `G_TUNE`, global 3, cents) | 0 | 2 AUDIO |
| 23 | 23 | HOME (DUELL 0.1: named OVERVIEW) | 0 | 0 SCOPE, 1 TRACKS (1.3; DUELL 0.1: PARTS), 2 LEVELS (1.5; DUELL 0.1: no such value, max 1) | SCOPE | 0 DISPLAY |
| ~~24~~ | | ~~HELP~~ | | retired in DUELL 0.2.2 (the on-device hints were removed for now; a stored ON is ignored). The id is never reused. | | |
| 25 | 25 | HOME OPENS (DUELL 0.1) | 0 | 0 MORPH, 1 MODE, 2 MIXER, 3 OVERVIEW | MORPH | 1 CONTROL |
| 26 | 26 | CPU METER (DUELL 0.2.9) | 0 | 0 ON, 1 OFF: the thin CPU load bar under the header's battery (stored as a bit that is clear = ON, so every older settings record reads ON) | ON | 0 DISPLAY |
| 27 | 27 | MODE RECALL (DUELL 0.2.11) | 0 | 0 OFF, 1 ON: ON = loading a multi (the page, MIDI program change, the editor) also sets the output MODE saved in it; OFF = a load keeps the unit's current MODE (stored as bit 6 of settings byte [15][24], clear = OFF, so every older record reads OFF). The CPU TEST multis M13..M16 (developer builds) always set theirs | OFF | 1 CONTROL |

**DUELL 0.2.2: ids 1 (STYLE), 2 (LARGE), 7 (FX LATCH) and 24 (HELP) are retired: `MENU_DESC` no longer lists them (the count in `4E 01 count` was 20 on DUELL 0.2.2 and is 22 since DUELL 0.2.11, which appended id 27 (21 since 0.2.9, id 26); the list walks ids 0, 3..6, 9..20, 22, 23, 25, 26, 27 in index order) and a `MENU_SET` to one of them answers rc 1 (unknown id) and changes nothing. Their bits in the settings record stay where they were (older records import unchanged; the firmware ignores them).** Every other id keeps its meaning.

DUELL 0.1: the MENU has four tabs (0 DISPLAY, 1 CONTROL, 2 AUDIO, 3 SYSTEM: the MIDI tab is gone, SYSTEM's tab index 4 became 3; a tab's index is no id), so the table's tab column holds Felucca's numbering for the rows above; the device's reply is authoritative. The list MENU_DESC walks is 24 items (index order: ids 0..7, 9..20, 22, 23, 24, 25).

The device's AUDIO tab shows SPEAKER EQ, USB LEVEL, CLICK, CLICK LEVEL, COUNT-IN and (1.2) TUNE (index 9, 10, 12, 13,
14, 22).
CLICK: the metronome while the transport runs: OFF, REC (while a track is armed), ON (always); CLICK LEVEL its
level; COUNT-IN: PLAY from stop with a track armed counts in that many bars first (internal clock only). The click
goes to the headphones / speaker, never into USB audio or a recording (README: Metronome and count-in). Applied at
once, as on the device.

RESTORE LAST (id 15, 1.2; the SYSTEM tab shows USB SERIAL and RESTORE LAST, index 11 and 15, then CALIBRATION, TEST (INFO until 0.2.2)
and ABOUT): ON (the default)
keeps the music as it is in an autosave while the device is stopped and idle, and brings it back at power-on; OFF
writes no autosave and starts with the power-on sounds (README: Restore the last session). DUELL 0.2.2: a restored
autosave starts with no part muted.

SCALE LEDS (id 16, 1.2, Discussion #127; the CONTROL tab showed HOLD, KNOB ACCEL, FX LATCH (retired in DUELL 0.2.2), BPM LOCK and SCALE LEDS,
index 5..8 and 16): ON shows the selected track's scale on the key LEDs outside the layers (README: Key LEDs).
Applied at once.

SCREEN OFF (id 17, 1.1.5; the DISPLAY tab shows COLOR, STYLE, LARGE, ANIM, LEDS and SCREEN OFF, index 0..4 and 17):
after that long without input on the panel (a button, a key, a knob; not MIDI, not the editor) the screen and its
backlight go off while the sound, the sequencer, MIDI and USB go on; the next touch wakes it and does nothing else.
NEVER keeps it on. A change counts from then (a MENU_SET to 5 MIN does not turn it off at once). Applied at once.

SCOPE (id 18, 1.2, Discussion #165; DISPLAY now shows COLOR, STYLE, LARGE, ANIM, LEDS, SCREEN OFF and SCOPE, index
0..4, 17, 18): what HOME's oscilloscope draws. OUT (the default, as before) the sound after MASTER, so it shrinks with
the volume; MIX the mix before MASTER (and before SPEAKER EQ and the limiter), drawn at the size MASTER
at its top would give, whatever the knob. The metronome's click is in neither. Applied at once.

STEP PREVIEW (id 19, 1.2, Discussion #169) and CHORD ENTRY (id 20, 1.2, #155; CONTROL now shows HOLD, KNOB ACCEL, FX
LATCH, BPM LOCK, SCALE LEDS, STEP PREVIEW and CHORD ENTRY, index 5..8, 16, 19, 20). STEP PREVIEW ON: on SEQ > STEP
(the piano roll and the DRUM grid) with the transport stopped, moving the cursor with KNOB 1 or PRESETS (or the
grid's page keys) plays the step it lands on once on the selected track, through its sound (a 1/16 at the track's
GATE); never while playing, and it never writes. CHORD ENTRY ADD: in step recording (STEP, stopped, the track armed)
each key adds its notes to the cursor step (up to 4) or takes them out when its note is there already, and letting
go does not move the cursor (KNOB 1 / PRESETS move it); HOLD (the default) is the keys held together writing the step
and moving on, as before. The editor's own step editing (`STEP_SET`) is not affected by either. Applied at once.

MIDI IN (id 21, 1.2; the MIDI tab, its only row) and TUNE (id 22, 1.2; AUDIO's last row): the project's MIDI IN
routing and tuning, edited up to 1.1.5 on GLO > SYSTEM (ROUT) and GLO > GLOBAL (TUNE), the same values as globals 14
and 3 (see Global ids and rc above). TUNE is the first kind 1 item: value, min -50 (`4E 3F`), max 50 (`32 40`), the
name, then the unit "ct" (no value names), then the tab. Applied at once (MIDI IN: a block that changes lets go of the
notes it held, as the knob did; TUNE: the next block of sound).

Example (bytes in hex): `F0 7D 46 4C 48 04 F7` asks for index 4; the reply
`F0 7D 46 4C 48 04 04 00 02 40 00 40 03 40 4C 45 44 53 00 4F 46 46 00 44 49 4D 20 4C 4F 00 44 49 4D 20 48 49 00 49 4E 56 00 00 44 49 53 50 4C 41 59 00 F7`
is index 4, id 4, kind 0, value 2 (`02 40`), min 0 (`00 40`), max 3 (`03 40`), "LEDS", then "OFF" "LOW"
"MEDIUM" "MAX", then tab 0 "DISPLAY" (1.0.4 firmware: the same reply without `00 44 49 53 50 4C 41 59 00`). `F0 7D 46 4C 49 04 03 40 F7` sets LEDS to MAX and answers `F0 7D 46 4C 49 00 04 03 40 F7` (rc 0).
`F0 7D 46 4C 49 00 32 40 F7` (COLOR 50) answers `F0 7D 46 4C 49 00 00 09 40 F7` (clamped to 9, MONO).
`F0 7D 46 4C 49 0E 01 40 F7` sets COUNT-IN to 1 BAR and answers `F0 7D 46 4C 49 00 0E 01 40 F7` (1.1).
`F0 7D 46 4C 49 16 74 3F F7` (TUNE -12) answers `F0 7D 46 4C 49 00 16 74 3F F7` (rc 0, 1.2).
`F0 7D 46 4C 48 19 F7` answers `F0 7D 46 4C 48 19 7F F7` (no index 25; 1.3 / 1.4: none past 23, `18`; 1.2: none past 22, `17`; before MIDI IN: none past 20, `15`; before SCOPE: none past 17, `12`; before SCREEN OFF: none past 16, `11`; 1.2 before SCALE LEDS: none past 15, `10`; 1.1 firmware: none past 14, `0F`; 1.0.5: none past 11, `0C`).

HOME (id 23, 1.3, Discussions #112 / #134; DISPLAY now shows COLOR, STYLE, LARGE, ANIM, LEDS, SCREEN OFF, SCOPE and
HOME, index 0..4, 17, 18, 23): what the device's HOME page shows under its four cards. SCOPE (the default, as before)
the oscilloscope; TRACKS the four tracks as rows: each track's cushion, engine icon and sound name, the 16 steps of its
pattern's page that plays (LEN > 16: the play head's page, stopped the selected track's cursor page; a pip per page),
a MUTE badge, the output meter; the selected track's row highlighted. Only the look changes: HOME's knobs, ALGORITHM
and the HOME button do what they do on SCOPE. Kept in the settings record (its own byte; a value an older firmware does
not know reads as SCOPE there). Applied at once. 1.5 (#134): LEVELS (2), the rows of TRACKS with KNOB 1..4 the LEVEL
of T1..T4 (the GLO layer's knobs) in place of the selected track's engine's four; firmware before 1.5 reads it as SCOPE.

HELP (id 24, 1.5, Discussion #156; SYSTEM now shows USB SERIAL, RESTORE LAST and HELP, index 11, 15, 24, then
CALIBRATION, TEST and ABOUT): ON shows a short hint (key caps and a few words) in the footer for about 2 s when a page
opens, and a second footer row under a quick layer's map. OFF (the default) shows none. A bit of the settings record
(MENU's 1.2 flags byte, bit 3; older firmware keeps it and ignores it). Applied at once.

**USB SERIAL (id 11).** The menu applies it when it closes; a MENU_SET applies it about 200 ms after its reply
(`usb_serial_apply`), so the reply leaves first (if the device shows the MENU at that moment: when it closes).
A change (ON <-> OFF; setting the value it already has does nothing) re-enumerates the whole device: it drops off
the bus (its D+ pull-up off) and comes back with the other descriptors, so USB-MIDI, USB audio and the serial
console all go away and come back. On the device the gap is about 25 ms plus one main-loop pass (`usb_retry`,
`usb_start`), or up to 1 s when it re-enumerated in the second before; the host then debounces the attach
(>= 100 ms), resets and enumerates the device. Measured on macOS (CoreMIDI): OFF -> ON, the reply at ~0.04 s, the
device gone at ~0.3 s, the MIDI port gone at ~0.57 s and back at ~1.15 s; ON -> OFF, the serial port goes
at ~0.6 s while the MIDI port was not seen to disappear at all (so do not wait for a disconnect event: after
a real change, wait ~1.5 s and open again). What the editor sends in that window is lost, `WATCH` ends with the
bus reset (send `INFO` and `WATCH` again after reconnecting), and a backup in progress answers rc 5 (stale).
