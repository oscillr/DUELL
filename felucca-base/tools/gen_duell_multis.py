#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL factory multis: tools/factory_multis/M01.bin..M10.bin (and M13.bin..M16.bin with the test mode) (raw multi records, firmware/src/multi.c format) -> build/gen/duell_multis.h.

    tools/gen_duell_multis.py build/gen/duell_multis.h      (tools/build.py generate(), DUELL/gen_only.py)
    DUELL_TEST_MODE=1 tools/gen_duell_multis.py ...         (or --test-mode: developer builds, host tests) also puts the four CPU test multis M13..M16 in

The release default leaves M13..M16 out (their slots read empty, "--", like M11..M12); the M13..M16.bin sources stay in the tree. The header says
which it is (DF_TEST_MULTIS) and multi.c refuses a header that does not match its DUELL_TEST_MODE.

The records are the source (built in the emulator, downloaded with the saved-data file, extracted with
tools/flash_dump.py --extract-multis, renamed in place); the firmware shows them in the slots M01..M10 while the slot's
flash record is empty, and ERASE of a saved overwrite restores them (multi.c). Checked here: version 5 (multi.c MU_VER: the only one it loads; 0.2.19 moved the level split: tools/factory_multis_levelsplit.py; none of the records holds CRUSH, so 0.2.14 changed no byte but the version; 0.2.16 moved the ping-pong switch into the delay MODE: tools/factory_multis_delaymode.py), the name is the
expected one (ASCII, at most 12), each part's engine + factory index names the intended sound, the record is not longer than MU_REC_MAX (1916).
M13..M16 (test mode only) are the CPU TEST multis (TEST 1 BASE, TEST 2 PHASE, TEST 3 HEAVY, TEST 4 MIX), built by tests/gen_test_multis.c with the firmware's own code
(the sound loader and multi.c's packer); 0.2.11's six (since 0.2.13 in M01 M02 M07..M10) are built the same way by tests/gen_factory_multis.c; M11, M12 have no factory multi.
DUELL 0.2.13 order (owner): M01 REESE SWAMP (the first start loads it), M02 WHISPER TIDE, M03 EERIE SIGNAL, M04 WIRED SNAIL, M05 DEAD BUFFER, M06 ACID ETHER
(M03..M06: the four emulator-made records, M01..M04 before, byte for byte), M07 COLD CHAPEL, M08 GARGLE ACID, M09 NIGHT ORGAN, M10 FINAL REEL. DUELL_MULTIS_BOOTSTRAP=1 lets M13..M16 be missing (only to build that tool).
Output: DF_N, DF_SLOT[] (the slot of each), DM_LEN[], DM_REC[] (one concatenated const array) and DM_AT[] (offsets).
"""
import importlib.util
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIR = ROOT / "tools" / "factory_multis"
SLOTS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 13, 14, 15]   # M01..M10, M13..M16 (0-based)
NAMES = ["REESE SWAMP", "WHISPER TIDE", "EERIE SIGNAL", "WIRED SNAIL", "DEAD BUFFER", "ACID ETHER",
         "COLD CHAPEL", "GARGLE ACID", "NIGHT ORGAN", "FINAL REEL",
         "TEST 1 BASE", "TEST 2 PHASE", "TEST 3 HEAVY", "TEST 4 MIX"]
NFACT = 10                                           # the release's factory multis: M01..M10 (the rest are the test mode's)
REC_MAX, MU_G_AT, MB_N, PS_STEP = 1916, 20, 7, 9
ENGINES = ["ANALOG", "DIGITAL", "PHASE", "LOFI", "SAMPLE", "VOICE", "TRIO", "WHEEL", "GRAIN", "PHYS", "DRUM", "NOISE", "FM6", "SLICE", "WAVE"]
# (SAMPLE, GRAIN, SLICE: retired in DUELL 0.2.9, no factory sound: a part on one stops the build here)
# the sound each part must load (engine, factory index -> name): a changed sound list stops the build here
PARTS = [("REESE BASS", "SQUELCH"), ("DIGITAL PAD", "WHISPER PAD"),
         ("VESPERS", "JAZZ CLUB"), ("SFORZANDO", "GLASS SHARDS"), ("FROZEN AIR", "MODEM"), ("RUBBER BAND", "LADDER BASS"),
         ("EVOLVING PAD", "SINGING BOWL"), ("GROWL BASS", "SQUELCH"),
         ("DRAWBARS", "LADDER BASS"), ("BRASS PAD", "STRING MACH"),
         ("JAZZ CLUB", "SILK CHORUS"), ("CRYSTAL CAVE", "SILK CHORUS"), ("TINE ROOM", "CRYSTAL CAVE"), ("SEA ORGAN", "WOW FLUTTER")]
TEST_MODE = {10: 2, 11: 2, 12: 0, 13: 2}             # the TEST multis' output MODE byte (header [4]): LAYER, LAYER, SPLIT, LAYER


def die(msg):
    sys.exit("gen_duell_multis: " + msg)


def check(k, b, counts):
    slot = SLOTS[k] + 1
    if len(b) > REC_MAX or len(b) < MU_G_AT or b[0] != 5:
        die("M%02d: not a version 5 record of at most %d bytes" % (slot, REC_MAX))
    raw = b[8:20].split(b"\0")[0]
    if raw.decode("ascii", "replace") != NAMES[k] or any(not 32 <= c <= 126 for c in raw):
        die("M%02d: name is %r, expected %r" % (slot, raw, NAMES[k]))
    np_, ng, pos = b[1], b[2], MU_G_AT + 2 * b[2]
    for t in range(2):
        eng, idx = b[pos], b[pos + 1]
        have = counts.get((ENGINES[eng] if eng < len(ENGINES) else "?", idx))
        if have != PARTS[k][t]:
            die("M%02d part %d: engine %d sound %d is %r, expected %r" % (slot, t + 1, eng, idx, have, PARTS[k][t]))
        if not any(b[pos + 4:pos + 4 + MB_N]):
            die("M%02d part %d: no End B block" % (slot, t + 1))
        if b[pos + 4 + MB_N + 48] - 64 not in (1, 2):    # SLOPE (P_FREED, id 48): 1 or 2; 0 is a record of before 0.2.9, which 0.2.14 no longer converts
            die("M%02d part %d: the multimode filter's SLOPE is %d, not 1 or 2 (a record of before 0.2.9: convert it)" % (slot, t + 1, b[pos + 4 + MB_N + 48] - 64))
        pos += 4 + MB_N + np_
        pos += 8 + PS_STEP * sum(bin(x).count("1") for x in b[pos:pos + 8]) + (128 if b[pos - 4 - MB_N - np_ + 3] & 1 else 0)
    n = b[pos]
    if pos + 1 + 3 * n + 3 != len(b):
        die("M%02d: record length does not match its parts" % (slot))
    if k < NFACT and b[4] != 2:                       # (0.2.13: all ten are LAYER; M01 is the first start's multi)
        die("M%02d: output MODE byte is %d, expected 2 (LAYER)" % (slot, b[4]))
    if k in TEST_MODE and b[4] != TEST_MODE[k]:
        die("M%02d: output MODE byte is %d, expected %d" % (slot, b[4], TEST_MODE[k]))


def sound_names():
    """(engine name, factory index) -> sound name, from tools/duell_sounds.py."""
    spec = importlib.util.spec_from_file_location("duell_sounds", ROOT / "tools" / "duell_sounds.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return {(s["engine"], s["idx"]): s["name"] for s in mod.SOUNDS}


def main(argv):
    args = [a for a in argv[1:] if a != "--test-mode"]
    test_mode = len(args) != len(argv) - 1 or os.environ.get("DUELL_TEST_MODE") == "1"
    if len(args) != 1:
        die("usage: gen_duell_multis.py [--test-mode] OUT.h")
    out = Path(args[0])
    counts = sound_names()
    boot = os.environ.get("DUELL_MULTIS_BOOTSTRAP") == "1"
    files = [DIR / ("M%02d.bin" % (SLOTS[k] + 1)) for k in range(len(NAMES))]
    keep = [k for k in range(len(NAMES)) if k < NFACT or (test_mode and (files[k].exists() or not boot))]
    recs = [files[k].read_bytes() for k in keep]
    for i, k in enumerate(keep):
        check(k, recs[i], counts)
    lines = ["/* generated by tools/gen_duell_multis.py from the M*.bin files in tools/factory_multis: do not edit */",
             "#define DF_TEST_MULTIS %d" % (1 if test_mode else 0),
             "#define DF_N %du" % len(recs),
             "static const uint8_t DF_SLOT[DF_N] = {%s};" % ", ".join(str(SLOTS[k]) for k in keep),
             "static const uint16_t DM_LEN[DF_N] = {%s};" % ", ".join(str(len(b)) for b in recs),
             "static const uint16_t DM_AT[DF_N] = {%s};" % ", ".join(str(sum(len(x) for x in recs[:k])) for k in range(len(recs)))]
    flat = b"".join(recs)
    lines.append("static const uint8_t DM_REC[%d] = {" % len(flat))
    for i in range(0, len(flat), 24):
        lines.append("    " + ", ".join("%d" % c for c in flat[i:i + 24]) + ",")
    lines.append("};")
    out.write_text("\n".join(lines) + "\n")
    print("duell_multis.h: %d factory multis, %d bytes%s" % (len(recs), len(flat), " (test mode: M13..M16 in)" if test_mode else ""))


if __name__ == "__main__":
    main(sys.argv)
