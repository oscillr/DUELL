#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL 1.0: level balance of the factory multis by TRIM (release_drafts/VOLUME_CHECK_V1.md).

    tools/multi_trim.py MEASURE.txt [--target DB] [--window DB] [--apply]

MEASURE.txt is the output of build/host/multi_loudness (tests/multi_loudness.c: C3 held on both parts, the loudest-window level LOUD in dBFS before MASTER).
A multi whose LOUD is more than WINDOW dB from TARGET gets BOTH parts' TRIM moved by the same number of steps (0.5 dB each), so the ratio between its
two parts is unchanged; nothing else of the record changes. A part's TRIM stays 0..127, and TRIM + LEVEL together stay within the +7.5 dB the mixer
allows (fx.c lvl_of); the peak (before MASTER) stays at or under -1 dBFS, which can leave a loud-peaked multi under the window; a multi that cannot get all the way is moved as far as both parts allow and reported. M16 STRESS TEST (a CPU test) is skipped.
Prints the plan (slot, LOUD, steps, TRIM before -> after); --apply writes tools/factory_multis/Mnn.bin."""
import argparse
import importlib.util
import re
from pathlib import Path

PEAK_CEIL = -1.0
ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("fm", ROOT / "tools" / "factory_multis.py")
fm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fm)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("measure")
    ap.add_argument("--target", type=float, default=-18.0)
    ap.add_argument("--window", type=float, default=3.0)
    ap.add_argument("--apply", action="store_true")
    a = ap.parse_args()
    P = fm.p_ids()
    loud, peak = {}, {}
    for line in open(a.measure):
        m = re.match(r"M(\d+) .*LOUD=(-?[\d.]+) PEAK=(-?[\d.]+)", line)
        if m:
            loud[int(m.group(1))], peak[int(m.group(1))] = float(m.group(2)), float(m.group(3))
    for slot in range(1, 16):
        f = fm.DIR / ("M%02d.bin" % slot)
        b = bytearray(f.read_bytes())
        parts, _ = fm.parts_of(bytes(b))
        at = [x[2] + 4 + fm.MB_N for x in parts]
        tr = [b[v + P["P_TRIM"]] - 64 for v in at]
        lv = [b[v + P["P_LEVEL"]] - 64 for v in at]
        d = a.target - loud[slot]
        want = int(round(d / 0.5)) if abs(d) > a.window else 0
        if peak[slot] + want * 0.5 > PEAK_CEIL:      # the peak stays at or under the ceiling (as the patches' balance does), the level may then sit under the window
            want = -int(-(PEAK_CEIL - peak[slot]) // 0.5) if False else int((PEAK_CEIL - peak[slot]) // 0.5)
        if not want:
            print("M%02d LOUD %.1f PEAK %.1f  inside the window  TRIM %d/%d" % (slot, loud[slot], peak[slot], tr[0], tr[1]))
            continue
        up = min(127 - tr[0], 127 - tr[1], 127 - (tr[0] + lv[0] - 112), 127 - (tr[1] + lv[1] - 112))
        dn = min(tr[0], tr[1])
        st = max(-dn, min(up, want))
        print("M%02d LOUD %.1f  wanted %+.1f dB (%+d steps), done %+d steps (%+.1f dB)  TRIM %d/%d -> %d/%d%s" % (
            slot, loud[slot], d, want, st, st * 0.5, tr[0], tr[1], tr[0] + st, tr[1] + st, "  LIMITED" if st != want else ""))
        if a.apply and st:
            for v, t in zip(at, tr):
                b[v + P["P_TRIM"]] = t + st + 64
            f.write_bytes(bytes(b))


main()
