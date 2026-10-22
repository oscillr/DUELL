#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""Host test of tools/bench_run.py (bench suite stage 1): the suite parser, the OWNER-1 suite, and the runner against the Mac twin
(build/host/bench_sim --serve): a whole short run, the result files, a name that does not match, and a unit that restarts mid-test."""
import csv
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import bench_run as br  # noqa: E402

SIM = os.path.join(ROOT, "build", "host", "bench_sim")
bad = 0


def check(name, ok):
    global bad
    print(f"bench_run: {name:<110} {'ok' if ok else 'FAIL'}")
    bad += not ok


# ---- the parser
check("note names: C4 = 60, A0 = 21, Eb2 = 39, F#4 = 66, 72", [br.note_of(x) for x in ("C4", "A0", "Eb2", "F#4", "72")] == [60, 21, 39, 66, 72])
name, tests = br.parse_suite(open(br.DEFAULT_SUITE).read())
check("OWNER-1: 36 tests, names unique, the two soaks and the cycle", name == "OWNER-1" and len(tests) == 36 and len({t.name for t in tests}) == 36 and sum(1 for t in tests if t.soak) == 2 and "STRESS-HOLD" in tests[0].cycles and "M07-CYCLE" in tests[0].cycles)
check("OWNER-1: the 20 engine patches, M07 x3, M16 x3, DUEL, 4 matrix, WAVE run, flash, 2 soaks, silence",
      sum(1 for t in tests if re.match(r"[A-Z0-9]+-\d+$", t.name)) == 20 and sum(t.name.startswith("M07-") for t in tests) == 3 and sum(t.name.startswith("M16-") for t in tests) == 3
      and sum(t.name.startswith("MATRIX-") for t in tests) == 4 and {"DUEL-SCENE", "WAVE-RUN", "FLASH-WRITE", "SILENCE"} <= {t.name for t in tests})
longest, total = 0, 0.0
for t in tests:
    evs, dur = br.expand(t)
    total += dur / 1000.0
    ts = [e[0] for e in evs]
    longest = max([longest] + [len("bench " + w) for _, w in evs] + [len("bench " + w) for w, _ in br.setup_words(t)])
    if ts != sorted(ts):
        check(f"{t.name}: events in time order", False)
    held = {}
    for _, w in evs:
        f = w.split()
        if f[2] == "on":
            held[(f[3], f[4])] = 1
        elif f[2] == "off":
            held.pop((f[3], f[4]), None)
    if held:
        check(f"{t.name}: every note released", False)
check("OWNER-1: every console word is under the console's 64 characters (the longest %d)" % longest, longest < 64)
check("OWNER-1: about 9 minutes without the two soaks, about 29 with them (%.1f min of tests, settling not counted)" % (total / 60), 26 * 60 < total < 30 * 60)
soak = [t for t in tests if t.name == "SOAK-M07"][0]
evs, dur = br.expand(soak)
check("SOAK-M07: 600 s, a chord every 4 s for 10 minutes (150), the cycle's ramps, all released at the end",
      dur == 603000 and sum(1 for _, w in evs if " on 1 " in w and w.split()[2] == "on") >= 4 * 150 and any("ramp 1 20 " in w for _, w in evs))
evs, dur = br.expand(soak, 30)
check("--soak 30 shortens a soak (events inside 30 s, released at 30 s)", dur == 33000 and max(e[0] for e in evs) <= 30000)
m = [t for t in tests if t.name == "MATRIX-SH-CUT"][0]
check("matrix: slot 8 LFO2 S&H to CUT +63 rate 100 becomes set words",
      [w for w, _ in br.setup_words(m)][-5:] == ["set 1 WAVE2 S&H", "set 1 RATE2 100", "set 1 SRC8 LFO2", "set 1 DST8 CUT", "set 1 AMT8 63"])
# the patch names against the owner's list (docs/PATCH_NUMBERS.md, when the file is there)
pn = os.path.join(os.path.dirname(ROOT), "docs", "PATCH_NUMBERS.md")
if os.path.exists(pn):
    nums = {int(a): b.strip() for a, b in re.findall(r"^\| (\d+) \| ([^|]+?)(?: \(NEW\))? \|", open(pn).read(), re.M)}
    wrong = [(s[2], s[3]) for t in tests for s in t.setup if s[0] == "patch" and nums.get(s[2], "").upper() != s[3].upper()]
    check("OWNER-1: every patch number and name matches docs/PATCH_NUMBERS.md (%d patches)" % sum(1 for t in tests for s in t.setup if s[0] == "patch"), not wrong)
    if wrong:
        print("   ", wrong)
else:
    print("bench_run: docs/PATCH_NUMBERS.md not found: the patch names are not compared with it")
try:
    br.parse_suite("test A x\nat 0 frobnicate C4\n")
    check("a wrong word in a suite is an error with its line", False)
except br.SuiteError as e:
    check("a wrong word in a suite is an error with its line", "line 2" in str(e))

# ---- the runner against the Mac twin
if not os.path.exists(SIM):
    print("bench_run: build/host/bench_sim not built: the runner is not run against it")
    sys.exit(1 if bad else 0)
quick = os.path.join(ROOT, "tests", "bench_quick.txt")
with tempfile.TemporaryDirectory() as tmp:
    out = os.path.join(tmp, "run")
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "bench_run.py"), "--sim", SIM, "--suite", quick, "--out", out, "--runs", tmp], capture_output=True, text=True)
    summ = open(os.path.join(out, "summary.txt")).read() if os.path.exists(os.path.join(out, "summary.txt")) else ""
    rows = list(csv.DictReader(open(os.path.join(out, "results.csv")))) if os.path.exists(os.path.join(out, "results.csv")) else []
    by = {x["test"]: x for x in rows}
    check("a whole run against the twin: the files (results.csv, summary.txt, unit.txt, console.log, suite.txt, soak_checks.csv)",
          all(os.path.exists(os.path.join(out, f)) for f in ("results.csv", "summary.txt", "unit.txt", "console.log", "suite.txt", "soak_checks.csv")))
    check("..7 tests, 6 PASS, the wrong name SKIPPED with the reason, exit code 0", len(rows) == 7 and by["WRONG-NAME"]["verdict"] == "SKIP" and "expects" in by["WRONG-NAME"]["why"]
          and sum(x["verdict"] == "PASS" for x in rows) == 6 and r.returncode == 0)
    check("..the CPU window: PATCH-61 measured 2.5 s = 862 halves; the flash test wrote once; the matrix test ran",
          by["PATCH-61"]["halves"] == "862" and by["FLASH-WRITE"]["flashes"] == "1" and by["MATRIX-SH"]["verdict"] == "PASS" and by["RUN-LEGATO"]["verdict"] == "PASS")
    sk = list(csv.DictReader(open(os.path.join(out, "soak_checks.csv"))))
    check("..the soak: the RAM check and the counters read every 5 s (3 times in 20 s)", len(sk) >= 3 and all(x["ramcode"] in ("-", "OK") for x in sk))
    check("..the log has the BT lines and the echo of the words", "BT,SOAK-SHORT,DONE" in open(os.path.join(out, "console.log")).read() and "## bench reset" in open(os.path.join(out, "console.log")).read())
    check("..the summary lists every test with CPU, voices, NOTES, verdict", all(n in summ for n in ("SILENCE", "PATCH-61", "WRONG-NAME", "SOAK-SHORT")) and "6 PASS  0 WARN  0 FAIL  1 SKIP" in summ)

    out2 = os.path.join(tmp, "crash")
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "bench_run.py"), "--sim", SIM, "--suite", quick, "--out", out2, "--runs", tmp, "--sim-crash-in", "FLASH-WRITE"], capture_output=True, text=True)
    rows = list(csv.DictReader(open(os.path.join(out2, "results.csv"))))
    by = {x["test"]: x for x in rows}
    check("a unit that restarts during a test: that test FAILs (CRASH with the record), the run goes on and the later tests PASS, exit code 1",
          by["FLASH-WRITE"]["verdict"] == "FAIL" and "CRASH" in by["FLASH-WRITE"]["why"] and by["MATRIX-SH"]["verdict"] == "PASS" and by["SOAK-SHORT"]["verdict"] == "PASS" and r.returncode == 1)
    # --only, --list, --dry-run need no unit
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "bench_run.py"), "--dry-run", "--only", "DUEL*,SOAK-M07"], capture_output=True, text=True)
    check("--dry-run --only prints the console words of the tests picked, no unit", "bench load patch 1 15" in r.stdout and "bench load patch 2 27" in r.stdout and "bench mode DUEL" in r.stdout and "2 tests" in r.stdout)
    # ---- core 1 (the two-core build's columns): the twin plays scenarios (BENCH_SIM_TC), the runner's rules for them (BENCH_TWOCORE1.md)
    def tc_run(scn, *extra):
        o = os.path.join(tmp, "tc_" + scn + "_" + str(len(extra)))
        env = dict(os.environ, BENCH_SIM_TC=scn)
        r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "bench_run.py"), "--sim", SIM, "--suite", quick, "--only", "SILENCE,PATCH-61",
                            "--out", o, "--runs", tmp, *extra], capture_output=True, text=True, env=env)
        rows = list(csv.DictReader(open(os.path.join(o, "results.csv")))) if os.path.exists(os.path.join(o, "results.csv")) else []
        sm = open(os.path.join(o, "summary.txt")).read() if os.path.exists(os.path.join(o, "summary.txt")) else ""
        return r, {x["test"]: x for x in rows}, sm

    r, by, sm = tc_run("ok")
    check("core 1 ok: the summary starts 'two cores active', results.csv carries tc=2 and the counters (blocks, in time), PASS, exit 0",
          sm.startswith("CORES: two cores active") and by["PATCH-61"]["tc"] == "2" and by["PATCH-61"]["tcwhy"] == "OK" and int(by["PATCH-61"]["tcblocks"]) > 100
          and by["PATCH-61"]["tcblocks"] == by["PATCH-61"]["tcintime"] and by["PATCH-61"]["c1stack"] == "1208" and by["PATCH-61"]["verdict"] == "PASS" and r.returncode == 0)
    check("..the counters are per test (SILENCE's blocks are not in PATCH-61's) and the summary has the core 1 column '2c ok'",
          0 < int(by["SILENCE"]["tcblocks"]) < 4 * 700 and "2c ok" in sm)
    r, by, sm = tc_run("late")
    check("late blocks: WARN with the count, exit 0", by["PATCH-61"]["verdict"] == "WARN" and re.search(r"\d+ late blocks", by["PATCH-61"]["why"]) and r.returncode == 0
          and int(by["PATCH-61"]["tclate"]) > 0 and by["PATCH-61"]["c1late"] == by["PATCH-61"]["tclate"] and "2c L" in sm)
    r, by, sm = tc_run("vlate")
    check("very late blocks that finished in their half and paused nothing (BENCHT3): WARN with the count, exit 0",
          by["PATCH-61"]["verdict"] == "WARN" and re.search(r"\d+ very late blocks \(the job finished inside its wait limit; nothing paused\)", by["PATCH-61"]["why"]) and r.returncode == 0
          and by["PATCH-61"]["c1over"] == by["PATCH-61"]["tcvlate"] == by["PATCH-61"]["tcvlok"] and by["PATCH-61"]["tcpauses"] == "0")
    r, by, sm = tc_run("vlaterun")
    check("a very late block that paused the jobs (BENCHT3: by a run): FAIL naming the pause and its reason, exit 1",
          by["PATCH-61"]["verdict"] == "FAIL" and re.search(r"\d+ pauses \(fault 0, run of very late \d+, window 0, run of late 0\)", by["PATCH-61"]["why"]) and r.returncode == 1 and int(by["PATCH-61"]["tcprun"]) == int(by["PATCH-61"]["tcpauses"]) > 0)
    r, by, sm = tc_run("silent")
    check("silent blocks: FAIL (with the silent count in the reason); the pause they cause is counted as a fault", by["PATCH-61"]["verdict"] == "FAIL" and "silent blocks" in by["PATCH-61"]["why"] and r.returncode == 1
          and int(by["PATCH-61"]["tcpfault"]) == int(by["PATCH-61"]["tcpauses"]) > 0)
    r, by, sm = tc_run("floor")
    check("own blocks because of the floor (BENCHT3): not a fault (PASS), named in the info and in the own split, which still adds up to own",
          by["PATCH-61"]["verdict"] == "PASS" and "floor (core 1's measured run time" in by["PATCH-61"]["info"] and int(by["PATCH-61"]["tmownfl"]) == int(by["PATCH-61"]["tcown"]) > 0 and r.returncode == 0)
    r, by, sm = tc_run("own")
    check("own blocks (core 0 played part 2 itself): PASS, reported as info only",
          by["PATCH-61"]["verdict"] == "PASS" and "[info: own " in sm and by["PATCH-61"]["info"].startswith("own ") and r.returncode == 0)
    for scn, word in (("noanswer", "NOANSWER"), ("notarmed", "NOTARMED"), ("prev", "PREV-HANG")):
        r, by, sm = tc_run(scn)
        check(f"one core ({scn}): the summary's first line says 'one core, reason {word}', results.csv tc=1 and the same reason, PASS (own is info), exit 0",
              sm.startswith(f"CORES: one core, reason {word}") and by["PATCH-61"]["tc"] == "1" and by["PATCH-61"]["tcwhy"] == word and by["PATCH-61"]["verdict"] == "PASS"
              and "1c" in sm and r.returncode == 0)
    r, by, sm = tc_run("failhang")
    check("core 1 stopped in the middle of the session (HANG): FAIL, the reason named", by["SILENCE"]["verdict"] == "FAIL" and "core 1 stopped" in by["SILENCE"]["why"]
          and "FAIL-HANG" in by["SILENCE"]["why"] and r.returncode == 1)
    # ---- BENCHT2 (BENCH_TWOCORE2.md): the diagnosis suite TWOCORE-1, the tune words, the timing columns, the summary section, the very late events
    nm2, t2 = br.parse_suite(open(os.path.join(ROOT, "tools", "bench_suites", "TWOCORE-1.txt")).read())
    tot2 = sum(br.expand(t)[1] / 1000.0 + t.settle + 0.6 for t in t2)
    tot2l = sum(br.expand(t, 180)[1] / 1000.0 + t.settle + 0.6 for t in t2)
    words2 = {t.name: [w for w, _ in br.setup_words(t)] for t in t2}
    check("TWOCORE-1: 12 tests (CHORDS, M07, STRESS, each under S1 S3 S6 POFF), unique names, a diagnosis suite, about 10 minutes (%.1f), 26 with --soak 180 (%.1f)" % (tot2 / 60, tot2l / 60),
          nm2 == "TWOCORE-1" and len(t2) == 12 and len({t.name for t in t2}) == 12 and all(t.diag for t in t2) and 9.5 * 60 < tot2 < 10.5 * 60 and 25 * 60 < tot2l < 27 * 60)
    check("TWOCORE-1: the settings are the current slack 3481, 3x = 10443, 6x = 20886 and pause off; every console word is under 64 characters",
          words2["CHORDS-S1"][-1] == "slack 3481" and words2["M07-S3"][-1] == "slack 10443" and words2["STRESS-S6"][-1] == "slack 20886" and words2["CHORDS-POFF"][-1] == "pause off"
          and all(len("bench " + w) < 64 for t in t2 for w in [x for x, _ in br.setup_words(t)]))
    try:
        br.parse_suite("test A x\ntune slack\n")
        check("a wrong tune word is an error with its line", False)
    except br.SuiteError as e:
        check("a wrong tune word is an error with its line", "line 2" in str(e))
    o = os.path.join(tmp, "t2")
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "bench_run.py"), "--sim", SIM, "--suite", "TWOCORE-1", "--only", "CHORDS-S1,CHORDS-S3,CHORDS-POFF", "--out", o, "--runs", tmp],
                       capture_output=True, text=True, env=dict(os.environ, BENCH_SIM_TC="vlate"))
    rows = list(csv.DictReader(open(os.path.join(o, "results.csv")))) if os.path.exists(os.path.join(o, "results.csv")) else []
    by = {x["test"]: x for x in rows}
    sm = open(os.path.join(o, "summary.txt")).read() if os.path.exists(os.path.join(o, "summary.txt")) else ""
    check("TWOCORE-1 against the twin: the settings reach the unit and come back in the columns (slack 3481 / 10443, pause off = late run 1000, very late run 0 (BENCHT4: off)), reset puts them back",
          len(rows) == 3 and by["CHORDS-S1"]["tmslack"] == "3481" and by["CHORDS-S3"]["tmslack"] == "10443" and by["CHORDS-S3"]["tmlrun"] == "4" and by["CHORDS-POFF"]["tmlrun"] == "1000"
          and by["CHORDS-POFF"]["tmvrun"] == "0" and by["CHORDS-POFF"]["tmslack"] == "3481" and by["CHORDS-POFF"]["tmcalm"] == "10000")
    x = by.get("CHORDS-S1", {})
    check("..the timing columns: jobs, the three histograms add up to the jobs, c1 and c1pk filled, the very late jobs counted with their block",
          x and int(x["tmjobs"]) > 100 and sum(int(x["hsd%d" % i]) for i in range(8)) == int(x["tmjobs"]) == sum(int(x["hrt%d" % i]) for i in range(8)) == sum(int(x["hmg%d" % i]) for i in range(8))
          and x["c1"] != "" and x["c1pk"] != "" and int(x["tmvn"]) == int(x["tcvlate"]) > 0 and sum(int(x["tmvblk%d" % i]) for i in range(4)) == int(x["tmvn"]) and x["c1over"] == x["tcvlate"])
    check("..a diagnosis run: very late blocks are WARN with the count, exit 0; the summary has the CORE 1 TIMING section with the settings and the histograms",
          x and x["verdict"] == "WARN" and "very late blocks" in x["why"] and r.returncode == 0 and "CORE 1 TIMING" in sm and "histogram, run time of a job" in sm and "histogram, finish margin" in sm
          and "histogram, start delay" in sm and "where in the half" in sm and by["CHORDS-S1"]["tmdl3"] == "2300" and by["CHORDS-S1"]["tmst3"] == "1900" and re.search(r"CHORDS-S3\s+10443\s+10000\s+new\s+0", sm) and re.search(r"CHORDS-POFF\s+3481\s+10000\s+off\s+0", sm))
    ev = list(csv.DictReader(open(os.path.join(o, "vlate_events.csv")))) if os.path.exists(os.path.join(o, "vlate_events.csv")) else []
    check("..vlate_events.csv: the newest very late jobs of each test (block in the half, voices, delay, run, margin), only numbers", len(ev) >= 3 and {e["test"] for e in ev} == {"CHORDS-S1", "CHORDS-S3", "CHORDS-POFF"}
          and all(e["run_us"].lstrip("-").isdigit() and e["margin_us"].lstrip("-").isdigit() for e in ev))
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "bench_run.py"), "--dry-run", "--suite", "TWOCORE-1", "--only", "STRESS-S3,CHORDS-POFF"], capture_output=True, text=True)
    check("--dry-run prints the tune words ('bench slack 10443', 'bench pause off')", "bench slack 10443" in r.stdout and "bench pause off" in r.stdout and "2 tests" in r.stdout)
    r, by, sm = tc_run("own")
    check("own blocks are split: the info names core 1 down / flash park / paused / deadline already past, and the four add up to own",
          "paused" in by["PATCH-61"]["info"] and "deadline already past" in by["PATCH-61"]["info"] and int(by["PATCH-61"]["tmownpause"]) == int(by["PATCH-61"]["tcown"]) > 0
          and sum(int(by["PATCH-61"][k]) for k in ("tmowndown", "tmownpark", "tmownpause", "tmowndl", "tmownfl")) == int(by["PATCH-61"]["tcown"]))
    # ---- BENCHT3 (BENCH_TWOCORE3.md): the suite TWOCORE-2, the tune words vwin and floor, the new columns and counters
    nm3, t3 = br.parse_suite(open(os.path.join(ROOT, "tools", "bench_suites", "TWOCORE-2.txt")).read())
    tot3 = sum(br.expand(t)[1] / 1000.0 + t.settle + 0.6 for t in t3)
    words3 = {t.name: [w for w, _ in br.setup_words(t)] for t in t3}
    check("TWOCORE-2: 15 tests (CHORDS, M07, STRESS, each under NEW-F1 NEW-F0 OLD-F1 OLD-F0 POFF), unique names, a diagnosis suite, every test carries the BENCHT2 numbers, about 11 minutes (%.1f)" % (tot3 / 60),
          nm3 == "TWOCORE-2" and len(t3) == 15 and len({t.name for t in t3}) == 15 and all(t.diag and t.ref for t in t3) and 10 * 60 < tot3 < 12 * 60)
    check("TWOCORE-2: the five settings are the words 'floor 1' / 'floor 0' / 'vrun 1' (the old rule) / 'pause off'; every console word is under 64 characters",
          words3["M07-NEW-F1"][-1] == "floor 1" and words3["M07-NEW-F0"][-1] == "floor 0" and words3["M07-OLD-F1"][-2:] == ["vrun 1", "floor 1"] and words3["M07-OLD-F0"][-2:] == ["vrun 1", "floor 0"]
          and words3["STRESS-POFF"][-2:] == ["pause off", "floor 0"] and all(len("bench " + w) < 64 for t in t3 for w in words3[t.name]))
    try:
        br.parse_suite("test A x\ntune floor\n")
        check("a wrong tune floor word is an error with its line", False)
    except br.SuiteError as e:
        check("a wrong tune floor word is an error with its line", "line 2" in str(e))
    check("tune vwin takes K or K MS", [w for w, _ in br.setup_words(br.parse_suite("test A x\ntune vwin 8 2000\ntune vwin 5\n")[1][0])] == ["vwin 8 2000", "vwin 5"])
    o = os.path.join(tmp, "t3")
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "bench_run.py"), "--sim", SIM, "--suite", "TWOCORE-2", "--only", "CHORDS-NEW-F1,CHORDS-NEW-F0,CHORDS-OLD-F0,CHORDS-POFF", "--out", o, "--runs", tmp],
                       capture_output=True, text=True, env=dict(os.environ, BENCH_SIM_TC="vlate"))
    rows = list(csv.DictReader(open(os.path.join(o, "results.csv")))) if os.path.exists(os.path.join(o, "results.csv")) else []
    by = {x["test"]: x for x in rows}
    sm = open(os.path.join(o, "summary.txt")).read() if os.path.exists(os.path.join(o, "summary.txt")) else ""
    check("TWOCORE-2 against the twin: the settings reach the unit and come back in the columns (BENCHT4 defaults: vrun 0, window 0; NEW-F1 sets floor 1; old = vrun 1; pause off = late run 1000, very late run 0, window 0), reset puts them back",
          len(rows) == 4 and (by["CHORDS-NEW-F1"]["tmvrun"], by["CHORDS-NEW-F1"]["tmvwink"], by["CHORDS-NEW-F1"]["tmvwinms"], by["CHORDS-NEW-F1"]["tmfloor"]) == ("0", "0", "2000", "1")
          and by["CHORDS-NEW-F0"]["tmfloor"] == "0" and by["CHORDS-NEW-F0"]["tmvrun"] == "0" and by["CHORDS-OLD-F0"]["tmvrun"] == "1" and by["CHORDS-OLD-F0"]["tmfloor"] == "0"
          and (by["CHORDS-POFF"]["tmvrun"], by["CHORDS-POFF"]["tmlrun"], by["CHORDS-POFF"]["tmvwink"], by["CHORDS-POFF"]["tmfloor"]) == ("0", "1000", "0", "0"))
    check("..the new counters: very late blocks that paused nothing and the pauses by reason are columns; very late = WARN in a diagnosis suite, exit 0; the summary table names them (vl-ok, f/r/w, floor) and the pause rule (new, old, off)",
          all(by[k]["tcvlok"] == by[k]["tcvlate"] and by[k]["tcpfault"] == "0" for k in by) and r.returncode == 0 and all(by[k]["verdict"] == "WARN" for k in by) and "vl-ok" in sm and "f/r/w" in sm
          and re.search(r"CHORDS-NEW-F1\s+3481\s+10000\s+new\s+1", sm) and re.search(r"CHORDS-OLD-F0\s+3481\s+10000\s+old\s+0", sm) and re.search(r"CHORDS-POFF\s+3481\s+10000\s+off\s+0", sm))
    check("..the summary prints the reference CPU (ref) beside this run's (OLD-F0 and the others 41.8 / 62.2, POFF 34.3 / 52.8)", "ref avg/peak" in sm and re.search(r"CHORDS-OLD-F0 .*41\.8 / 62\.2", sm) and re.search(r"CHORDS-POFF .*34\.3 / 52\.8", sm))
    o = os.path.join(tmp, "t3b")
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "bench_run.py"), "--sim", SIM, "--suite", "TWOCORE-2", "--only", "CHORDS-OLD-F1", "--out", o, "--runs", tmp],
                       capture_output=True, text=True, env=dict(os.environ, BENCH_SIM_TC="vlatepause"))
    rows = list(csv.DictReader(open(os.path.join(o, "results.csv")))) if os.path.exists(os.path.join(o, "results.csv")) else []
    check("..a diagnosis suite counts pauses as WARN (not FAIL), naming the reason; exit 0", len(rows) == 1 and rows[0]["verdict"] == "WARN" and "pauses (fault 0, run of very late" in rows[0]["why"] and r.returncode == 0)
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "bench_run.py"), "--dry-run", "--suite", "TWOCORE-2", "--only", "M07-OLD-F1,STRESS-POFF"], capture_output=True, text=True)
    check("--dry-run prints the TWOCORE-2 words ('bench vrun 1', 'bench floor 0', 'bench pause off')", "bench vrun 1" in r.stdout and "bench floor 0" in r.stdout and "bench pause off" in r.stdout and "2 tests" in r.stdout)
    # ---- BENCHT4 (BENCH_TWOCORE4.md): the suite TWOCORE-3 and its verdicts
    nm4, t4 = br.parse_suite(open(os.path.join(ROOT, "tools", "bench_suites", "TWOCORE-3.txt")).read())
    tot4 = sum(br.expand(t)[1] / 1000.0 + t.settle + 0.6 for t in t4)
    words4 = {t.name: [w for w, _ in br.setup_words(t)] for t in t4}
    check("TWOCORE-3: 4 tests (CHORDS, M07, STRESS with the BENCHT4 defaults, STRESS with the old run and window rules), unique names, only the contrast test is a diagnosis test, every test has a ref, about 8 minutes (%.1f)" % (tot4 / 60),
          nm4 == "TWOCORE-3" and [t.name for t in t4] == ["CHORDS-DEF", "M07-DEF", "STRESS-DEF", "STRESS-RUNWIN"] and [t.diag for t in t4] == [False, False, False, True] and all(t.ref for t in t4) and 7 * 60 < tot4 < 9 * 60)
    check("TWOCORE-3: the three default tests set nothing (the build's defaults), the contrast sets 'vrun 4' and 'vwin 8 2000'; every console word is under 64 characters",
          not any(w.split()[0] in ("vrun", "vwin", "floor", "pause", "slack", "calm", "lrun") for n in ("CHORDS-DEF", "M07-DEF", "STRESS-DEF") for w in words4[n]) and words4["STRESS-RUNWIN"][-2:] == ["vrun 4", "vwin 8 2000"]
          and all(len("bench " + w) < 64 for t in t4 for w in words4[t.name]))
    check("a `diag` line inside a test makes that test alone a diagnosis test; before the first test it makes them all",
          [t.diag for t in br.parse_suite("test A x\ntest B y\ndiag\n")[1]] == [False, True] and [t.diag for t in br.parse_suite("diag\ntest A x\ntest B y\n")[1]] == [True, True])
    o = os.path.join(tmp, "t4")
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "bench_run.py"), "--sim", SIM, "--suite", "TWOCORE-3", "--only", "CHORDS-DEF,STRESS-RUNWIN", "--out", o, "--runs", tmp],
                       capture_output=True, text=True, env=dict(os.environ, BENCH_SIM_TC="vlaterun"))
    rows = list(csv.DictReader(open(os.path.join(o, "results.csv")))) if os.path.exists(os.path.join(o, "results.csv")) else []
    by = {x["test"]: x for x in rows}
    sm = open(os.path.join(o, "summary.txt")).read() if os.path.exists(os.path.join(o, "summary.txt")) else ""
    check("TWOCORE-3 against the twin with a pause by a run of very late blocks: the default test FAILs (a pause), the contrast test is WARN (diagnosis), exit 1; the settings come back (default: vrun 0 window 0 floor 0 'new'; contrast: vrun 4 window 8 'b3')",
          len(rows) == 2 and by["CHORDS-DEF"]["verdict"] == "FAIL" and "pauses (fault 0, run of very late" in by["CHORDS-DEF"]["why"] and by["STRESS-RUNWIN"]["verdict"] == "WARN" and r.returncode == 1
          and (by["CHORDS-DEF"]["tmvrun"], by["CHORDS-DEF"]["tmvwink"], by["CHORDS-DEF"]["tmfloor"]) == ("0", "0", "0") and (by["STRESS-RUNWIN"]["tmvrun"], by["STRESS-RUNWIN"]["tmvwink"], by["STRESS-RUNWIN"]["tmvwinms"]) == ("4", "8", "2000")
          and re.search(r"CHORDS-DEF\s+3481\s+10000\s+new\s+0", sm) and re.search(r"STRESS-RUNWIN\s+3481\s+10000\s+b3\s+0", sm))
    o = os.path.join(tmp, "t4b")
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "bench_run.py"), "--sim", SIM, "--suite", "TWOCORE-3", "--only", "CHORDS-DEF", "--out", o, "--runs", tmp],
                       capture_output=True, text=True, env=dict(os.environ, BENCH_SIM_TC="vlate"))
    rows = list(csv.DictReader(open(os.path.join(o, "results.csv")))) if os.path.exists(os.path.join(o, "results.csv")) else []
    check("TWOCORE-3, very late blocks that paused nothing: WARN with the count, exit 0 (the count stays visible in the vl-ok column)",
          len(rows) == 1 and rows[0]["verdict"] == "WARN" and re.search(r"\d+ very late blocks \(the job finished inside its wait limit", rows[0]["why"]) and r.returncode == 0 and rows[0]["tcvlok"] == rows[0]["tcvlate"] != "0")
    o = os.path.join(tmp, "t4c")
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "bench_run.py"), "--sim", SIM, "--suite", "TWOCORE-3", "--only", "CHORDS-DEF", "--out", o, "--runs", tmp],
                       capture_output=True, text=True, env=dict(os.environ, BENCH_SIM_TC="silent"))
    rows = list(csv.DictReader(open(os.path.join(o, "results.csv")))) if os.path.exists(os.path.join(o, "results.csv")) else []
    check("TWOCORE-3, a silent block (a job not finished by its wait limit): FAIL, exit 1", len(rows) == 1 and rows[0]["verdict"] == "FAIL" and "silent blocks" in rows[0]["why"] and r.returncode == 1)
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "bench_run.py"), "--dry-run", "--suite", "TWOCORE-3"], capture_output=True, text=True)
    check("--dry-run prints the TWOCORE-3 words ('bench vrun 4', 'bench vwin 8 2000') and 4 tests", "bench vrun 4" in r.stdout and "bench vwin 8 2000" in r.stdout and "4 tests" in r.stdout)
    r, by, sm = tc_run("noanswer", "--two-core")
    check("--two-core on a one-core session: stops before any test, says why and that the Mac cannot restart the unit, exit non-zero",
          r.returncode != 0 and "NOANSWER" in (r.stderr + r.stdout) and "Nothing was run" in (r.stderr + r.stdout) and not by)
    r, by, sm = tc_run("ok", "--two-core")
    check("--two-core on a two-core session runs as usual", r.returncode == 0 and by["PATCH-61"]["verdict"] == "PASS")

    # ---- the serial path: a pseudo-terminal with the twin behind it, speaking as the unit's console does (the typed characters echoed, a reply after CR LF, the
    # prompt "> ", the result line arriving by itself), 10 times faster than real time
    import os as _os
    import select as _select
    import threading
    import time as _time
    master, slave = _os.openpty()
    import tty
    tty.setraw(slave)                                    # (no echo, no line editing: the unit's USB serial does none)
    pty_name = _os.ttyname(slave)
    stop = threading.Event()
    sim = subprocess.Popen([SIM, "--serve"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)

    def talk(line):
        sim.stdin.write(line + "\n")
        sim.stdin.flush()
        out = []
        while True:
            ln = sim.stdout.readline()
            if ln.rstrip("\r\n") == "@done":
                return out
            out.append(ln.rstrip("\r\n"))

    def console():
        buf = b""
        _os.write(master, b"\r\nDUELL console - 'help'\r\n> ")
        last = _time.monotonic()
        while not stop.is_set():
            r, _, _ = _select.select([master], [], [], 0.01)
            if r:
                d = _os.read(master, 256)
                for c in d:
                    if c in (13, 10):
                        if c == 10 and not buf:
                            continue
                        _os.write(master, b"\r\n")
                        for ln in talk(buf.decode()):
                            _os.write(master, (ln + "\r\n").encode())
                        _os.write(master, b"> ")
                        buf = b""
                    else:
                        buf += bytes([c])
                        _os.write(master, bytes([c]))
            now = _time.monotonic()
            if now - last >= 0.02:
                last = now
                for ln in talk("@adv 200"):                     # (200 ms of the twin's time every 20 ms: 10 times real time)
                    _os.write(master, (ln + "\r\n").encode())
                    _os.write(master, b"> ")
    th = threading.Thread(target=console, daemon=True)
    th.start()
    out3 = os.path.join(tmp, "serial")
    t0 = _time.monotonic()
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "bench_run.py"), "--port", pty_name, "--suite", quick, "--out", out3, "--runs", tmp, "--only", "SILENCE,PATCH-61,FLASH-WRITE,SOAK-SHORT", "--no-caffeinate"],
                       capture_output=True, text=True, timeout=120)
    stop.set()
    th.join(2)
    sim.stdin.close()
    sim.wait()
    rows = list(csv.DictReader(open(os.path.join(out3, "results.csv")))) if os.path.exists(os.path.join(out3, "results.csv")) else []
    check("the serial path (a pseudo-terminal speaking as the console does): 4 tests, all PASS, the result lines found among the echo and the prompts",
          len(rows) == 4 and all(x["verdict"] == "PASS" for x in rows) and r.returncode == 0 and rows[1]["halves"] == "862")
    if r.returncode or len(rows) != 4:
        print(r.stdout[-1500:], r.stderr[-800:])
# ---- BENCHR2: `repeat N`, the lead-in of a recorded timeline, the suite RECORD-2
rn, rt = br.parse_suite("test A x\nrepeat 3\nat 0 chord C4 vel 90\ntail 1\ntest B y\nat 0 note C4 len 1\n")
check("the suite word `repeat N` sets the test's repeat count (A 3, B 1)", [t.repeat for t in rt] == [3, 1])
bad_repeat = []
for txt in ("test A x\nrepeat 0\n", "test A x\nrepeat 51\n", "test A x\nrepeat\n", "repeat 2\n"):
    try:
        br.parse_suite(txt)
        bad_repeat.append(txt)
    except br.SuiteError:
        pass
check("`repeat 0`, `repeat 51`, `repeat` without a number and `repeat` outside a test are refused", not bad_repeat)
e0, d0 = br.expand(rt[0])
e1, d1 = br.expand(rt[0], None, 600)
check("expand with a lead-in of 600 ms: every event 600 ms later (the T inside the word too), the length 600 ms longer; with no lead-in as before",
      [t + 600 for t, _ in e0] == [t for t, _ in e1] and d1 == d0 + 600 and all(w1.startswith(f"ev {t1} ") for t1, w1 in e1) and br.expand(rt[0], None, 0) == (e0, d0))
nm2, t2 = br.parse_suite(open(os.path.join(ROOT, "tools", "bench_suites", "RECORD-2.txt")).read())
rc_ = br.RecCfg()
tot2 = sum((br.expand(t)[1] / 1000.0 + t.settle + 0.6 + rc_.settle + rc_.lead + 0.4) * t.repeat for t in t2)
check("RECORD-2: STRESS-1CORE x3, STRESS-DEF x3, M07-1CORE x1, M07-DEF x1, the references say onecore-of, about 22 minutes with the recorder's settling and lead-in (%.1f)" % (tot2 / 60),
      nm2 == "RECORD-2" and [(t.name, t.repeat) for t in t2] == [("STRESS-1CORE", 3), ("STRESS-DEF", 3), ("M07-1CORE", 1), ("M07-DEF", 1)] and [t.onecore_of for t in t2] == ["STRESS-DEF", None, "M07-DEF", None]
      and 20 * 60 <= tot2 <= 25 * 60)
nm1, t1_ = br.parse_suite(open(os.path.join(ROOT, "tools", "bench_suites", "RECORD-1.txt")).read())
check("RECORD-1 and the other suites still parse and run once (repeat 1)", nm1 == "RECORD-1" and all(t.repeat == 1 for t in t1_) and all(x.repeat == 1 for x in br.parse_suite(open(br.DEFAULT_SUITE).read())[1]))
w2 = {t.name: [w for w, _ in br.setup_words(t)] for t in t2}
check("RECORD-2: the references set 'slack 400000', the defaults set nothing; every console word is under 64 characters",
      "slack 400000" in w2["STRESS-1CORE"] and "slack 400000" in w2["M07-1CORE"] and not [w for n in ("STRESS-DEF", "M07-DEF") for w in w2[n] if w.split()[0] in ("slack", "calm", "pause", "lrun", "vrun", "vwin", "floor")]
      and all(len("bench " + w) < 64 for t in t2 for w in w2[t.name]))
try:
    br.RecCfg(lead=0.4, mark_after=0.35).check()
    lead_ok = False
except br.SuiteError:
    lead_ok = True
check("the recording settings: a lead-in that is not 0.1 s longer than the mark's delay is refused (the mark must fall before the first note); the defaults are 1.0 / 0.6 / 0.35 / 0 / 60 ms",
      lead_ok and (br.RecCfg().settle, br.RecCfg().lead, br.RecCfg().mark_after, br.RecCfg().under_limit, br.RecCfg().match_s) == (1.0, 0.6, 0.35, 0, 0.06) and br.RecCfg().setup_wait is None)
# the USB underrun rule: judged from the mark, with a threshold
row = {"status": "DONE", "miss": "0", "peak": "50", "ramcode": "OK", "lost": "0", "boots": "1", "uac_under": "13", "uac_over": "0", "uac_missed": "0", "uac_stalls": "0", "uac_hz": "44100", "tc": "-"}
tst = br.Test("STRESS-X", "")
v_old, w_old = br.verdict(row, {"boots": 1, "uac_under": 0, "uac_over": 0, "uac_missed": 0, "uac_stalls": 0}, tst)
v_a, w_a = br.verdict(row, {"boots": 1, "uac_under": 13, "uac_over": 0, "uac_missed": 0, "uac_stalls": 0, "marked": True, "under_limit": 0}, tst)
v_b, w_b = br.verdict(row, {"boots": 1, "uac_under": 12, "uac_over": 0, "uac_missed": 0, "uac_stalls": 0, "marked": True, "under_limit": 0}, tst)
v_c, w_c = br.verdict(row, {"boots": 1, "uac_under": 11, "uac_over": 0, "uac_missed": 0, "uac_stalls": 0, "marked": True, "under_limit": 2}, tst)
v_d, w_d = br.verdict(row, {"boots": 1, "uac_under": 10, "uac_over": 0, "uac_missed": 0, "uac_stalls": 0, "marked": True, "under_limit": 2}, tst)
check("underruns: +13 since the session start FAILs an unmarked run as before; 0 after the mark PASSes; +1 after the mark FAILs ('after the mark'); with the limit 2: +2 PASS, +3 FAIL",
      v_old == "FAIL" and "under +13" in w_old and "after the mark" not in w_old and v_a == "PASS" and v_b == "FAIL" and "under +1 after the mark" in w_b and v_c == "PASS" and v_d == "FAIL" and "(limit 2)" in w_d)
print("bench_run: " + ("FAIL" if bad else "all checks passed"))
sys.exit(1 if bad else 0)
