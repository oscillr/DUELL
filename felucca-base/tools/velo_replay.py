#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""Replay a key recording through the firmware's sense core and report the velocity spread (docs/VELO_CSV.md).

    tools/velo_replay.py RECORDING.csv [--curve PCT] [--landings OUT.csv]

RECORDING.csv: `ts_us,key,adc,drive` (the raw logger on the unit writes exactly this). The C core (firmware/src/sense.c) is
compiled from tools/velo_replay.c with the host's `cc` and run over it; this script only reads what it prints. Output: per
key and overall, the number of landings, the velocity quartiles (sorted[n/4], sorted[n/2], sorted[3n/4]), and the share at
110 or more and below 10. After Velo's method (see sense.c)."""
import argparse, os, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))


def build(outdir):
    exe = os.path.join(outdir, "velo_replay")
    src = os.path.join(HERE, "velo_replay.c")
    subprocess.run(["cc", "-O2", "-o", exe, src], check=True)
    return exe


def quartiles(v):
    v = sorted(v)
    n = len(v)
    return v[n // 4], v[n // 2], v[(3 * n) // 4]


def line(name, v):
    q1, med, q3 = quartiles(v)
    hi = 100.0 * sum(1 for x in v if x >= 110) / len(v)
    lo = 100.0 * sum(1 for x in v if x < 10) / len(v)
    return f"{name:>8} {len(v):6d}   {q1:3d} / {med:3d} / {q3:3d}   {hi:5.1f} %   {lo:5.1f} %"


def replay(csv_path, curve=100):
    """-> list of (ts_us, key, velocity, from_driven) for every landing"""
    with tempfile.TemporaryDirectory() as d:
        exe = build(d)
        r = subprocess.run([exe, "--curve", str(curve), csv_path], capture_output=True, text=True, check=True)
    out = []
    for ln in r.stdout.splitlines():
        ts, key, vel, drv = (int(x) for x in ln.split(","))
        out.append((ts, key, vel, drv))
    return out, r.stderr.strip()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("recording")
    ap.add_argument("--curve", type=int, default=100, help="MENU curve 0 (flat 100) .. 100 (measured range)")
    ap.add_argument("--landings", help="write the landings as CSV: ts_us,key,velocity,from_driven")
    a = ap.parse_args(argv)
    lands, note = replay(a.recording, a.curve)
    if not lands:
        print("no landings found", file=sys.stderr)
        return 1
    if a.landings:
        with open(a.landings, "w") as f:
            f.write("ts_us,key,velocity,from_driven\n")
            for ts, key, vel, drv in lands:
                f.write(f"{ts},{key},{vel},{drv}\n")
    print(note)
    print(f"{'key':>8} {'n':>6}   {'quartiles':^15}   {'>=110':>7}   {'<10':>7}")
    by = {}
    for ts, key, vel, drv in lands:
        by.setdefault(key, []).append(vel)
    for key in sorted(by):
        print(line(str(key), by[key]))
    print(line("all", [x[2] for x in lands]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
