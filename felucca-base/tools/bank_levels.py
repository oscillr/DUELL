#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL 1.0: balance the levels of the 128 factory patches by their TRIM alone (owner, 2026-10-21, item 30; release_drafts/BANK_LEVELS.md).

    tools/bank_levels.py [--passes N] [--md FILE]

Measures with the recipe's level audit (tools/refine_audit.sh all: C3, velocity 100, held 10 s, ear-weighted, before MASTER), takes per kind (the category)
the bank's own MEDIAN of the kind's level, and moves every patch that sits more than WINDOW dB from that median onto it, in TRIM steps (0.5 dB each);
a patch inside the window keeps its TRIM. Nothing else of a patch is touched: the result is tools/factory_bank_trims.json, which tools/import_bank.py lays over
the bank (no other value changes). Rules:
  level      MED (the held body, median of seconds 1..6) for a patch that sustains, LOUD (the loudest 0.5 s window) for PLUCK and KEYS and for a patch that does not
             sustain; a HIT (PERC, and a one-shot OTHER: the audit's 'H' flag) by its PEAK, kept between -6 and -3 dBFS (the audit's hit rule)
  window     median +- 1.0 dB
  floors     a patch that sustains keeps its held body at -20.5 dBFS or above (the audit's quiet-body rule is -21) and no patch goes down so far that its
             short-note level (TAP) falls under -23.5 dBFS (the audit's rule is -24)
  ceiling    the peak stays at or under -1.0 dBFS (a clipping patch gets a lower TRIM; a patch that wants more is held at the ceiling and listed as limited)
Re-run until nothing moves; the passes and the result are printed.
"""
import argparse
import json
import re
import statistics
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"
WINDOW = 1.0
PEAK_CEIL = -1.0
MED_FLOOR = -20.5          # (the audit fails a held body under -21; a margin)
MED_PASS = -21.3           # (a held body above this passes the audit's quiet-body rule, give or take the rounding of the table)
TAP_FLOOR = -23.5          # (the audit fails a tap under -24; a margin)
HIT_LO, HIT_HI = -6.0, -3.0
STEP_DB = 0.5
ROW = re.compile(r"^  (\d+)\s+")                      # the audit's fixed columns: number, the name (12), the category (6), the engine, the numbers


def audit():
    out = subprocess.run(["bash", str(TOOLS / "refine_audit.sh"), "all"], cwd=ROOT, capture_output=True, text=True).stdout
    rows = {}
    for line in out.splitlines():
        m = ROW.match(line)
        if not m or len(line) < 60:
            continue
        cat, eng, rest = line[19:25].strip(), line[26:33].strip(), line[33:]
        nums = re.findall(r"-?\d+\.\d+", rest)
        flag = line.rstrip().split()[-1]
        peak, loud, l1, l3, med, t3, tap, split = (float(x) for x in nums[:8])
        n = int(m.group(1))
        mb = re.search(r"([+-]\s*\d+\.\d)\s*/\s*([+-]\s*\d+\.\d)\s+x([\d.]+)\s*/\s*x([\d.]+)", line)
        wide = cat in ("PAD", "FX")
        w, lo, hi = (3.5, 0.4, 2.5) if wide else (2.5, 0.5, 2.0)
        bwin = bool(mb) and all(abs(float(x.replace(" ", ""))) <= w for x in mb.groups()[:2]) and all(lo <= float(x) <= hi for x in mb.groups()[2:])
        rows[n] = dict(bwin=bwin, endb="FAIL (End B)" in line, n=n, name=line[6:18].strip(), cat=cat, eng=eng, peak=peak, loud=loud, med=med, tap=tap, hit=flag.endswith("H"), sus=flag == "S", verdict=line.split("   ")[-1].strip() if "FAIL" in line else "ok")
    if len(rows) != 128:
        sys.exit("bank_levels: the audit gave %d rows, not 128" % len(rows))
    return rows


def metric(r):
    if r["hit"]:
        return "peak", r["peak"]
    if r["cat"] in ("PLUCK", "KEYS") or not r["sus"]:
        return "loud", r["loud"]
    return "med", r["med"]


def kind(r):
    return "HITS" if r["hit"] else r["cat"]


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--passes", type=int, default=4)
    ap.add_argument("--md", default="")
    a = ap.parse_args(argv)
    trims_file = TOOLS / "factory_bank_trims.json"
    bank = {it["number"]: it for it in json.loads((TOOLS / "factory_bank.duell-patches.json").read_text())["patches"]}
    if trims_file.exists():
        trims_file.unlink()
    subprocess.run([sys.executable, "-I", str(TOOLS / "import_bank.py"), "--write"], cwd=ROOT, check=True, capture_output=True)
    before = audit()
    orig = {}
    import base64
    ctxm = __import__("importlib.util").util
    spec = ctxm.spec_from_file_location("ib", TOOLS / "import_bank.py")
    ib = ctxm.module_from_spec(spec)
    spec.loader.exec_module(ib)
    ctx = ib.Ctx()
    for n, it in bank.items():
        orig[n] = ib.parse_record(ctx, base64.b64decode(it["record"]))["values"][ctx.ids["P_TRIM"]]
    groups = {}
    for n, r in before.items():
        groups.setdefault(kind(r), []).append(metric(r)[1])
    median = {k: statistics.median(v) for k, v in groups.items()}
    trims = dict(orig)
    limited = set()
    atmax = set()
    peakfix = set()
    now = before
    for p in range(a.passes):
        moved = 0
        for n, r in now.items():
            k = kind(r)
            m = metric(r)[1]
            tgt = median[k]
            quiet = (not r["hit"]) and r["sus"] and r["med"] < MED_FLOOR              # a held body under the audit's -21 dBFS rule is moved whatever the window says
            if abs(m - tgt) <= WINDOW and r["peak"] <= PEAK_CEIL and not quiet and not (r["hit"] and not HIT_LO <= r["peak"] <= HIT_HI):
                continue
            d = tgt - m
            if r["hit"]:
                d = max(min(tgt, HIT_HI - 0.3), HIT_LO + 0.3) - r["peak"]       # a hit: its peak is the level
            if d < 0 and not r["hit"] and r["tap"] + d < TAP_FLOOR:                  # never so far down that a played-short note falls under the audit's -24 dBFS tap rule
                d = min(0.0, TAP_FLOOR - r["tap"])
            if d < 0 and not r["hit"] and r["med"] > MED_PASS and r["med"] + d < MED_FLOOR:   # (a plucked or struck sound whose held body at 1..6 s passes the audit now: it must keep passing; one far under -21 fails already and is free)
                d = min(0.0, MED_FLOOR - r["med"])
            cap = (HIT_HI - 0.3 if r["hit"] else PEAK_CEIL) - r["peak"]
            if d > cap:
                d = cap
                limited.add(n)
            steps = int(d / STEP_DB) if d > 0 else -int(-d / STEP_DB)            # toward zero: never over the ceiling, never past the target
            if abs(d) - abs(steps) * STEP_DB >= STEP_DB * 0.5 and (d < 0 or d + 0 <= cap - 0.25):
                steps += 1 if d > 0 else -1
            if steps == 0:
                continue
            new = max(0, min(127, trims[n] + steps))
            if new == trims[n]:
                atmax.add(n)                                                     # the TRIM range ends (0..127): nothing more to give
                continue
            trims[n] = new
            moved += 1
        trims_file.write_text(json.dumps({str(k): v for k, v in sorted(trims.items()) if v != orig[k]}, indent=0, sort_keys=False) + "\n")
        subprocess.run([sys.executable, "-I", str(TOOLS / "import_bank.py"), "--write"], cwd=ROOT, check=True, capture_output=True)
        now = audit()
        print("pass %d: %d patches moved" % (p + 1, moved))
        if not moved:
            break
    for p in range(6):                                       # a raised patch whose End B now fails ONLY by its peak (the dB / brightness window is fine): one step back at a time
        back = [n for n, r in now.items() if r["endb"] and r["bwin"] and trims[n] > orig[n]]
        if not back:
            break
        for n in back:
            trims[n] -= 1
            peakfix.add(n)
        trims_file.write_text(json.dumps({str(k): v for k, v in sorted(trims.items()) if v != orig[k]}, indent=0, sort_keys=False) + "\n")
        subprocess.run([sys.executable, "-I", str(TOOLS / "import_bank.py"), "--write"], cwd=ROOT, check=True, capture_output=True)
        now = audit()
        print("End B peak guard pass %d: %d patches one step down" % (p + 1, len(back)))
    lines = ["| # | NAME | kind | measure | TRIM before -> after | level before -> after (dBFS) | peak before -> after | note |", "|---|---|---|---|---|---|---|---|"]
    for n in sorted(now):
        b, r = before[n], now[n]
        mname, mb = metric(b)
        _, ma = metric(r)
        note = []
        if trims[n] == orig[n]:
            note.append("unchanged")
        if n in limited and r["peak"] > PEAK_CEIL - 0.6 and not r["hit"]:
            note.append("held by the -1 dBFS peak ceiling")
        if n in peakfix:
            note.append("lowered so that End B's peak stays under -1 dBFS")
        if n in atmax and trims[n] in (0, 127):
            note.append("TRIM at its range end (%d)" % trims[n])
        lines.append("| #%02d | %s | %s | %s | %d -> %d | %.1f -> %.1f | %.1f -> %.1f | %s |" % (n, r["name"], kind(r), mname.upper(), orig[n], trims[n], mb, ma, b["peak"], r["peak"], "; ".join(note)))
    if a.md:
        Path(a.md).write_text("\n".join(lines) + "\n\nmedians: " + ", ".join("%s %.1f" % (k, v) for k, v in sorted(median.items())) + "\n")
    print("\n".join(lines))
    print("medians:", {k: round(v, 2) for k, v in sorted(median.items())})
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
