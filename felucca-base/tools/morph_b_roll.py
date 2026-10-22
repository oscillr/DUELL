#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL 1.0: re-roll MORPH End B of every factory multi M01..M15 with the firmware's own ROLL B (release_drafts/MORPH_B_ROLL_V1.md).

    tools/morph_b_roll.py [--apply] [--note FILE]

Builds and runs tests/morph_b_roll.c (the firmware's morph_roll_seed: the part's own AMOUNT knob, the multi's morph settings, SMART as stored; one seed for both
parts as the ROLL B button gives; morph_new_seed from the fixed start 1717 + 7 * slot, so the result is repeatable), which prints each part's End B block as
morph_blk_make writes it. --apply puts those 7 bytes into tools/factory_multis/Mnn.bin (End B's block is all that changes: End A, names, values, the
hand-set B edits of a record stay), --note writes the per-multi note. Run from anywhere; needs build/gen (host_tests.sh makes it)."""
import argparse
import importlib.util
import os
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("fm", ROOT / "tools" / "factory_multis.py")
fm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fm)


# the k-th seed of each slot's fixed sequence (M01..M15): the first whose End B at fader 127 is within 2 dB of End A in loudness and under -1 dBFS (or A's own peak), and at fader 63 too for M04 (its stab peaks in the middle; M04: k = 20, the first of 1..30 that fits both),
# found with ML_K=k ML_FADER=127 build/host/multi_loudness (tests/multi_loudness.c) for k = 1..24; 1 = the first roll already did
OLD_REF = "1e74232"      # the commit of the volume check: the records as the owner's file made them, with the TRIM change, before the re-roll
SEED_K = [3, 1, 2, 20, 4, 1, 7, 1, 1, 2, 1, 1, 5, 3, 3]


def run(env=None):
    cc = ["cc", "-O2", "-w", "-DHOST_MIX_FOLLOWS_MASTER=1", "-DDUELL_TEST_MODE=1", "-Ibuild/gen_test", "-Ibuild/gen", "-Ifirmware/src", "-Itests",
          '-DFELUCCA_VERSION="x"', "-o", "build/host/morph_b_roll", "tests/morph_b_roll.c", "-lm"]
    subprocess.run(["python3", "tools/gen_duell_multis.py", "build/gen_test/duell_multis.h", "--test-mode"], cwd=ROOT, check=True, capture_output=True)
    subprocess.run(cc, cwd=ROOT, check=True)
    return subprocess.run(["build/host/morph_b_roll"], cwd=ROOT, env=dict(os.environ, MBR_K=",".join(map(str, SEED_K)), **(env or {})), capture_output=True, text=True, check=True).stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--note", default="")
    a = ap.parse_args()
    P = fm.p_ids()
    names = sorted(P, key=P.get)
    blocks, vec = {}, {}
    for line in run().splitlines():
        m = re.match(r"M(\d+) (\d) seed (\d+) amount (\d+) stereo (\d) ((?:[0-9a-f]{2} ?){7})", line)
        blocks[(int(m.group(1)), int(m.group(2)))] = (bytes.fromhex(m.group(6).replace(" ", "")), int(m.group(3)), int(m.group(4)))
    for line in run({"MBR_VEC": "1"}).splitlines():
        m = re.match(r"V M(\d+) (\d) eng (\d+) (.*)", line)
        if m:
            vec[(int(m.group(1)), int(m.group(2)))] = [tuple(int(x) for x in t.split(",")) for t in m.group(4).split()]
    notes = []
    for slot in range(1, 16):
        f = fm.DIR / ("M%02d.bin" % slot)
        b = bytearray(f.read_bytes())
        parts, _ = fm.parts_of(bytes(b))
        ob = subprocess.check_output(["git", "show", "%s:felucca-base/tools/factory_multis/M%02d.bin" % (OLD_REF, slot)], cwd=ROOT)   # the record before any re-roll
        for t, (_, _, at) in enumerate(parts):
            blk, seed, amt = blocks[(slot, t + 1)]
            old = bytes(ob[at + 4:at + 4 + fm.MB_N])
            v = vec[(slot, t + 1)]
            ch = [(abs(x[2] - x[1]), names[k], x[0], x[1], x[2]) for k, x in enumerate(v) if x[1] != x[2]]
            ch.sort(reverse=True)
            nb_old = sum(1 for x in v if x[1] != x[0])
            nb_new = sum(1 for x in v if x[2] != x[0])
            notes.append((slot, t + 1, old, blk, seed, amt, len(ch), nb_old, nb_new, ch[:5]))
            if a.apply:
                b[at + 4:at + 4 + fm.MB_N] = blk
        if a.apply:
            f.write_bytes(bytes(b))
    if a.note:
        out = ["| multi | part | seed old -> new (k-th of the sequence) | AMOUNT | cfg old -> new | values that differ from A in B (old -> new) | values of B that moved | biggest moves (value A: old B -> new B) |", "|---|---|---|---|---|---|---|---|"]
        for slot, t, old, blk, seed, amt, n, no, nn, top in notes:
            out.append("| M%02d | %d | %d -> %d (k=%d) | %d | %d -> %d | %d -> %d | %d | %s |" % (slot, t, old[1] | old[2] << 8, seed, SEED_K[slot - 1], amt, old[4], blk[4], no, nn, n,
                       "; ".join("%s %d: %d -> %d" % (nm, av, o, nw) for _, nm, av, o, nw in top)))
        Path(a.note).write_text("\n".join(out) + "\n")


main()
