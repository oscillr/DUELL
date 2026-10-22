#!/usr/bin/env python3
"""The pool-tail rule: 8 KiB in every build, 4,096 B only in a DUELL_BENCH developer build (tools/build.py pool_tail_min)."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
import build

ok = True
def eq(name, got, want):
    global ok
    good = got == want
    ok &= good
    print(("ok   " if good else "FAIL ") + f"{name}: {got} (want {want})")

eq("normal build", build.pool_tail_min({}), 8192)
eq("RAMPLACE only", build.pool_tail_min({"DUELL_RAMPLACE": "1"}), 8192)
eq("TWOCORE without bench", build.pool_tail_min({"DUELL_TWOCORE": "1"}), 8192)
eq("DUELL_BENCH=1 without a tag", build.pool_tail_min({"DUELL_BENCH": "1"}), 8192)
eq("DUELL_BENCH=0 with a tag", build.pool_tail_min({"DUELL_BENCH": "0", "BENCH_TAG": "X"}), 8192)
eq("bench developer build", build.pool_tail_min({"DUELL_BENCH": "1", "BENCH_TAG": "BENCHT1"}), build.POOL_TAIL_MIN_BENCH)
eq("bench constant", build.POOL_TAIL_MIN_BENCH, 4096)
eq("constant", build.POOL_TAIL_MIN, 8192)
sys.exit(0 if ok else 1)
