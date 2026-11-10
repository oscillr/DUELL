#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The verdict of a recorded bench run for loud sounds (2026-11-07, C-CHECK): a dropout or a wrap, not the analyser's "gaps".

  python3 tools/wrap_check.py RUN_FOLDER          (a bench_run.py --record output: results.csv + audio/*.wav)

PASS per test: no missed half, no silent block, no USB underrun after the mark, no click (bench_analyze), and no FLIP-AND-BACK: a sample-to-sample jump
over 0.5 of full scale followed within 6 samples by an opposite one over 0.5. That is a wrap's signature (the pan step's 32-bit overflow of 2026-11-07:
4 / 5 / 27 of them in CLICK-CHECK on bench 1.1-bench-2026-11-07-c, 0 after the fix). A hard-clipped stack of saws makes one-way jumps of up to about 0.85
(its edges lined up) and holds still between edges, which the analyser counts as "gaps": both are listed, neither fails.
Very late blocks (core 1's job finished inside its wait limit, nothing paused) are listed, not failed.
"""
import csv
import sys
import wave
from pathlib import Path

import numpy as np


def flips(s, big=0.5, within=6):
    d = np.diff(s)
    idx = np.where(np.abs(d) > big)[0]
    return sum(1 for k in idx if any(np.sign(d[k]) != np.sign(d[j]) and abs(d[j]) > big for j in range(k + 1, min(k + 1 + within, len(d)))))


def main(run):
    run = Path(run)
    rows = list(csv.DictReader(open(run / "results.csv")))
    g = lambda r, k: int(float(r.get(k) or 0))
    print(f"{'test':26} {'shown':7} {'miss':>4} {'silent':>6} {'v.late':>6} {'under':>5} {'clicks':>6} {'flips':>5} {'max jump':>8} {'gaps':>5}  verdict")
    bad = 0
    for r in rows:
        t = r["test"]
        w = wave.open(str(run / "audio" / f"{t}.wav"))
        x = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").reshape(-1, 2) / 32768.0
        on = int(np.argmax(np.abs(x[:, 0]) > 0.01))
        s = x[on + 4410:on + 44100 * 5]                  # after the attack, the held chord
        fl = flips(s[:, 0]) + flips(s[:, 1])
        mj = float(np.abs(np.diff(s, axis=0)).max()) if len(s) > 1 else 0.0
        fail = g(r, "miss") + g(r, "tcsilent") + g(r, "under_post") + g(r, "clicks") + fl > 0
        bad += fail
        print(f"{t:26} {r.get('notes1', '')}+{r.get('notes2', ''):4} {g(r, 'miss'):4} {g(r, 'tcsilent'):6} {g(r, 'tcvlate'):6} {g(r, 'under_post'):5} {g(r, 'clicks'):6} {fl:5} {mj:8.2f} {g(r, 'gaps'):5}  {'FAIL' if fail else 'PASS'}")
    print(f"{len(rows) - bad} of {len(rows)} PASS")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
