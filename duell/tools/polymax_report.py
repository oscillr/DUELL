#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""DUELL 1.1 item 90: the POLY-MAX bench run (tools/gen_polymax_suite.py) as one table.

  python3 tools/polymax_report.py bench_runs/<the run's folder>/results.csv   -> polymax.csv beside it, and the table on the screen

A key count passes when the unit missed no half (miss 0), no USB underrun was added, part 2 was never silent (tcsilent 0) and the CPU peak stayed at or under PEAK_MAX (95 %).
The table: per engine, filter (MOFF / M12 / M12TW) and cores (2C / 1C) the most keys that passed (every smaller count passed too), and the CPU average and peak there.
The NOTES budget's numbers of the same settings come from the host (tests/poly_limits_test.c), to put beside it."""
import csv
import os
import re
import sys

PEAK_MAX = 95.0


def num(r, k):
    try:
        return float(r.get(k) or 0)
    except ValueError:
        return 0.0


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    rows = list(csv.DictReader(open(sys.argv[1])))
    groups = {}
    for r in rows:
        m = re.match(r"POLY_(\w+?)_(MOFF|M12|M12TW)_(1C|2C)_N(\d+)$", r.get("test", ""))
        if not m:
            continue
        ok = num(r, "miss") == 0 and num(r, "uac_under") == 0 and num(r, "tcsilent") == 0 and num(r, "peak") <= PEAK_MAX and r.get("status", "ok") in ("ok", "")
        groups.setdefault(m.group(1, 2, 3), []).append((int(m.group(4)), ok, num(r, "avg"), num(r, "peak"), num(r, "vox")))
    out = os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])), "polymax.csv")
    with open(out, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["engine", "filter", "cores", "max_keys", "avg_at_max", "peak_at_max", "voices_at_max", "first_fail_keys"])
        for (e, v, c), lst in sorted(groups.items()):
            lst.sort()
            best, at, fail = 0, None, ""
            for k, ok, avg, peak, vox in lst:
                if not ok:
                    fail = k
                    break
                best, at = k, (avg, peak, vox)
            w.writerow([e, v, c, best, *(at or ("", "", "")), fail])
            print(f"{e:8s} {v:6s} {c}: {best} keys" + (f"  (avg {at[0]:.1f} %, peak {at[1]:.1f} %, {at[2]:.1f} voices)" if at else "") + (f", fails at {fail}" if fail else ""))
    print(f"polymax: {out}")


if __name__ == "__main__":
    main()
