#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""DUELL automated bench suite, stage 1: the Mac's runner (developer bench builds, DUELL_BENCH=1).

  python3 tools/bench_run.py                          the suite OWNER-1 on the unit (about 30 minutes with both soaks)
  python3 tools/bench_run.py --only M07-CHORDS,SILENCE  some tests
  python3 tools/bench_run.py --list / --dry-run       what the suite holds / the console words it would send, no unit
  python3 tools/bench_run.py --soak 60                every soak this long (seconds)       --no-soak  leave the soaks out
  python3 tools/bench_run.py --sim build/host/bench_sim   the Mac twin instead of the unit (host tests)
  python3 tools/bench_run.py --record [--two-core] [--suite NAME]   (stage 2) also record the unit's USB audio during every test and analyze it
                                                                     (clicks, gaps, level, DC, centroid, tail: tools/bench_analyze.py); --keep N runs keep their audio
  python3 tools/bench_run.py --record --repeat 3 --only STRESS-DEF   (BENCHR2) the test three times back to back; the summary has an aggregate line per test
  python3 tools/bench_run.py --record --suite RECORD-2              (BENCHR2) the recorder suite with repeats, about 22 minutes: release_drafts/BENCH_RECORD2.md
  python3 tools/bench_run.py --prove-chain [--minutes N]   (stage 2, a FELUCCA_UAC_TONE build such as BENCHA1) record the unit's test triangle for 10 minutes and check
                                                           every frame follows the one before: zero breaks proves the recorder end to end

The unit is driven over its USB serial console (bench builds present it whatever MENU > USB SERIAL says): the runner sends the console
words `bench ...` (firmware/src/bench.c), the unit plays the test itself on its own audio clock and answers with one result line
("BT,..."). Standard library only (termios), macOS or Linux. Hands off the unit while it runs; turn MASTER down.

A suite is a plain text file (tools/bench_suites/*.txt), one test after another, times in seconds from the test's start:

  test    NAME   text                       a test (NAME has no spaces)           cycle NAME period S  .. end     a block, never run alone
  load    multi 7 "FINAL REEL"              a multi (the name is checked on the unit; a wrong name: the test is SKIPPED)
  load    patch 1 #61 "EVOLVING PAD"        patch slot #NN to part 1 or 2
  mode    DUEL|STEREO|LAYER                 scene "HVY DUEL 8+8"   a row of the bench list (the NOTES budget held open)
  set     1 CUT 60   /  set g BPM 120       any part or global value (the unit's label or id; enum names too)
  matrix  1 slot 8 LFO2 S&H CUT +63 [rate 100]   source [LFO 2's wave] destination amount
  at 0.0  chord C4 E4 G4 vel 90 [ch 1]      note C4 vel 90 len 0.5 | release | morph 0 127 over 3 [curve smooth]
  at 2.0  aftertouch 0 127 over 2 | wheel 0 127 over 2 | run C2 C6 step 0.03 legato | save-test | settle S
  tail 5       measure 1.0 12.0      the test ends 5 s after its last event; the CPU window (default: all)
  expect  exact|close|report         how a later comparison judges it (stage 3; recorded only)
  soak    600 repeat M07-CYCLE       the block repeated for 600 s            check 60     the RAM check and counters every 60 s
  tune    slack 10443 | calm 2500 | pause off | lrun 4 | vrun 1 | vwin 8 2000 | floor 0     (BENCHT2 and later; vwin and floor BENCHT3) the runner's margin settings for this test (`bench slack ...`); the unit puts them back at the next `bench reset`
  ref     41.8 62.2                  (BENCHT3) the CPU average and peak the same test gave in an earlier build; the summary prints them beside this run's
  onecore-of NAME                    (inside a test, with --record) this test is the ONE-CORE REFERENCE of the test NAME: it must play the same scene with core 1 given no jobs
                                     (`tune slack 400000`: every block's deadline is already past, core 0 plays part 2 itself; no power-cycle). The summary compares the
                                     recorded clicks and gaps of NAME with this one's: only those above the reference count against NAME (BENCH_JOIN1.md)
  repeat N                           (inside a test) run this test N times back to back (--repeat N on the command line overrides it); each repeat does its own bench reset
  diag                               (a line of its own, before the first test) a diagnosis suite: very late blocks and pauses are WARN, not FAIL; inside a test: that test only (BENCHT4)
Notes: C4 = 60. Channel 1 is part 1, channel 2 part 2 (GLO > MIDI). `curve` is accepted; the ramps are linear.
"""
import argparse
import csv
import datetime
import glob
import hashlib
import os
import re
import select
import shlex
import subprocess
import sys
import termios
import time

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_SUITE = os.path.join(HERE, "bench_suites", "OWNER-1.txt")
RUNS_DEFAULT = "bench_runs"


class SuiteError(Exception):
    pass


class LinkDown(Exception):
    """the unit's port went away (a restart, a cable)"""


# ------------------------------------------------------------------------------------------------ the suite file
NOTE_RE = re.compile(r"^([A-Ga-g])([#b]?)(-?\d)$")
SEMI = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def note_of(s):
    m = NOTE_RE.match(s)
    if m:
        n = (int(m.group(3)) + 1) * 12 + SEMI[m.group(1).upper()] + (1 if m.group(2) == "#" else -1 if m.group(2) == "b" else 0)
    elif s.lstrip("-").isdigit():
        n = int(s)
    else:
        raise SuiteError(f"not a note: {s}")
    if not 0 <= n <= 127:
        raise SuiteError(f"note out of range: {s}")
    return n


class Test:
    def __init__(self, name, text, cycle=False, period=0.0):
        self.name, self.text, self.cycle, self.period = name, text, cycle, period
        self.setup = []            # (kind, args): the console words before the run, in order
        self.events = []           # (t_ms, order, line): the timeline's `bench ev` words (the T is filled in when sent)
        self.tail, self.measure, self.expect = 2.0, None, "report"
        self.soak = None           # (seconds, cycle name)
        self.check = 60
        self.check_on = False       # `check S` was given: the RAM check and the counters are read every S seconds
        self.settle = 1.5
        self.sounding = {}         # (ch, note) -> True while held, to release
        self.end = 0.0             # the last event's time, s
        self.diag = False          # a diagnosis suite: very late blocks are information (WARN)
        self.ref = None            # (avg, peak) of an earlier build, for the summary
        self.repeat = 1            # (BENCHR2) how many times the test is run back to back
        self.onecore_of = None     # (RECORD-1) the name of the two-core test this test is the one-core reference of

    def add(self, t, line, dur=0.0):
        self.events.append((int(round(t * 1000)), len(self.events), line))
        self.end = max(self.end, t + dur)


def parse_ramp(t, test, kw, rest, ch):
    # morph A B over S [curve X]   (rest: A B over S ...)
    if len(rest) < 4 or rest[2] != "over":
        raise SuiteError(f"usage: {kw} A B over S")
    a, b, s = int(rest[0]), int(rest[1]), float(rest[3])
    cc = {"morph": "20", "wheel": "1", "aftertouch": "AT"}[kw]
    test.add(t, f"ramp {ch} {cc} {a} {b} {int(round(s * 1000))}", s)


def parse_at(test, tok):
    t = float(tok[1])
    if t < 0:
        raise SuiteError("negative time")
    kw, rest = tok[2], tok[3:]
    ch = 1
    if "ch" in rest:
        i = rest.index("ch")
        ch = int(rest[i + 1])
        rest = rest[:i] + rest[i + 2:]
    vel = 90
    if "vel" in rest:
        i = rest.index("vel")
        vel = int(rest[i + 1])
        rest = rest[:i] + rest[i + 2:]
    if kw == "chord":
        for n in rest:
            nn = note_of(n)
            test.add(t, f"on {ch} {nn} {vel}")
            test.sounding[(ch, nn)] = True
    elif kw == "note":
        ln = 0.5
        if "len" in rest:
            i = rest.index("len")
            ln = float(rest[i + 1])
            rest = rest[:i] + rest[i + 2:]
        nn = note_of(rest[0])
        test.add(t, f"on {ch} {nn} {vel}")
        test.add(t + ln, f"off {ch} {nn}", ln)
    elif kw == "release":
        for (c, n) in sorted(test.sounding):
            if rest and "all" not in rest and c != ch:
                continue
            test.add(t, f"off {c} {n}")
            test.sounding.pop((c, n))
    elif kw in ("morph", "aftertouch", "wheel"):
        if "curve" in rest:
            i = rest.index("curve")
            rest = rest[:i] + rest[i + 2:]
        parse_ramp(t, test, kw, rest, ch)
    elif kw == "run":
        # run C2 C6 step 0.03 legato: up, then back down
        lo, hi, step, legato = note_of(rest[0]), note_of(rest[1]), 0.03, "legato" in rest
        if "step" in rest:
            step = float(rest[rest.index("step") + 1])
        seq = list(range(lo, hi + 1)) + list(range(hi - 1, lo - 1, -1))
        for i, n in enumerate(seq):
            test.add(t + i * step, f"on {ch} {n} {vel}")
            test.add(t + i * step + step * (1.5 if legato else 0.9), f"off {ch} {n}", step * 1.5)
        test.end = max(test.end, t + len(seq) * step)
    elif kw == "save-test":
        test.add(t, "flash")
    else:
        raise SuiteError(f"unknown action at {t}: {kw}")


def parse_suite(text):
    tests, cycles, cur, name, diag = [], {}, None, "", False
    for n, raw in enumerate(text.splitlines(), 1):
        line = "" if re.match(r"\s*#(\s|$)", raw) else re.sub(r"\s#\s.*$", "", raw).strip()   # (a comment is "# " at the start or after a space; #61 is a patch)
        if not line:
            continue
        try:
            if line.split(None, 1)[0] in ("test", "suite"):    # (the rest is free text: apostrophes and all)
                tok = line.split(None, 2)
            else:
                tok = shlex.split(line)
        except ValueError as e:
            raise SuiteError(f"line {n}: {e}")
        k = tok[0]
        try:
            if k == "suite":
                name = tok[1]
            elif k == "test":
                cur = Test(tok[1], tok[2] if len(tok) > 2 else "")
                tests.append(cur)
            elif k == "cycle":
                cur = Test(tok[1], "block", cycle=True, period=float(tok[tok.index("period") + 1]))
                cycles[cur.name] = cur
            elif k == "end":
                cur = None
            elif k == "diag":
                if cur is None:
                    diag = True                                # (before the first test: the whole suite)
                else:
                    cur.diag = True                            # (BENCHT4: inside a test: that test only, e.g. a contrast run that switches the old pause rules on)
            elif cur is None:
                raise SuiteError("outside a test")
            elif k == "load":
                if tok[1] == "multi":
                    cur.setup.append(("multi", int(tok[2]), tok[3] if len(tok) > 3 else ""))
                elif tok[1] == "patch":
                    cur.setup.append(("patch", int(tok[2]), int(tok[3].lstrip("#")), tok[4] if len(tok) > 4 else ""))
                else:
                    raise SuiteError("load multi N \"NAME\" | load patch PART #NN \"NAME\"")
            elif k == "mode":
                cur.setup.append(("mode", tok[1].upper()))
            elif k == "scene":
                cur.setup.append(("scene", " ".join(tok[1:])))
            elif k == "budget":
                cur.setup.append(("budget", tok[1].upper()))
            elif k == "set":
                cur.setup.append(("set", tok[1:]))
            elif k == "tune":
                if not ((tok[1] in ("slack", "calm", "pause", "lrun", "vrun", "floor") and len(tok) == 3) or (tok[1] == "vwin" and len(tok) in (3, 4))):
                    raise SuiteError("tune slack N | calm MS | pause on|off | lrun N | vrun N | vwin K [MS] | floor 0|1")
                cur.setup.append(("tune", tok[1:]))
            elif k == "ref":
                cur.ref = (tok[1], tok[2])
            elif k == "repeat":
                if len(tok) != 2 or not 1 <= int(tok[1]) <= 50:
                    raise SuiteError("repeat N (1 to 50)")
                cur.repeat = int(tok[1])
            elif k == "onecore-of":
                if len(tok) != 2:
                    raise SuiteError("onecore-of TESTNAME")
                cur.onecore_of = tok[1]
            elif k == "matrix":
                # matrix PART slot N SRC [WAVE] DEST AMT [rate R]
                p, slot, rest = tok[1], int(tok[3]), tok[4:]
                rate = None
                if "rate" in rest:
                    i = rest.index("rate")
                    rate = rest[i + 1]
                    rest = rest[:i] + rest[i + 2:]
                src, amt, dst, wave = rest[0], rest[-1].lstrip("+"), rest[-2], rest[1] if len(rest) == 4 else None
                dst = {"LEVEL": "TRIM"}.get(dst, dst)
                for w in ([("WAVE2", wave)] if wave else []) + ([("RATE2", rate)] if rate else []):
                    cur.setup.append(("set", [p, w[0], w[1]]))
                cur.setup.append(("set", [p, f"SRC{slot}", src]))
                cur.setup.append(("set", [p, f"DST{slot}", dst]))
                cur.setup.append(("set", [p, f"AMT{slot}", amt]))
            elif k == "at":
                parse_at(cur, tok)
            elif k == "tail":
                cur.tail = float(tok[1])
            elif k == "measure":
                cur.measure = (float(tok[1]), float(tok[2]))
            elif k == "expect":
                cur.expect = tok[1]
            elif k == "settle":
                cur.settle = float(tok[1])
            elif k == "check":
                cur.check = int(tok[1])
                cur.check_on = True
            elif k == "soak":
                if tok[2] != "repeat":
                    raise SuiteError("soak SECONDS repeat CYCLE")
                cur.soak = (float(tok[1]), tok[3])
            else:
                raise SuiteError(f"unknown word {k}")
        except (IndexError, ValueError) as e:
            raise SuiteError(f"line {n}: {line!r}: {e}")
        except SuiteError as e:
            raise SuiteError(f"line {n}: {e}")
    for t in tests:
        if t.soak and t.soak[1] not in cycles:
            raise SuiteError(f"{t.name}: no cycle {t.soak[1]}")
        t.cycles = cycles
        t.diag = t.diag or diag
    return name, tests


def expand(test, soak_override=None, lead_ms=0):
    """the test's timeline: [(t_ms, 'ev ...' word)] sorted by time (stable), and its length in ms. lead_ms (BENCHR2, recorded runs): every event is held back by this many
    ms, and the length grows by it: a silent lead-in after the `bench run` word, in which the unit does its per-test reset (bench_reset_phases) before the first note"""
    evs = list(test.events)
    end = test.end
    if test.soak:
        secs = soak_override if soak_override is not None else test.soak[0]
        cyc = test.cycles[test.soak[1]]
        k = 0
        while k * cyc.period < secs:
            for (t, o, line) in cyc.events:
                if k * cyc.period * 1000 + t < secs * 1000:
                    evs.append((int(round(k * cyc.period * 1000)) + t, len(evs), line))
            k += 1
        end = secs
        # every note still sounding at the end is released
        held = {}
        for (t, o, line) in sorted(evs):
            w = line.split()
            if w[0] == "on":
                held[(w[1], w[2])] = 1
            elif w[0] == "off":
                held.pop((w[1], w[2]), None)
        for (c, n) in sorted(held):
            evs.append((int(end * 1000), len(evs), f"off {c} {n}"))
    else:
        held = {}
        for (t, o, line) in sorted(evs):
            w = line.split()
            if w[0] == "on":
                held[(w[1], w[2])] = 1
            elif w[0] == "off":
                held.pop((w[1], w[2]), None)
        for (c, n) in sorted(held):                  # a test that forgot a release: released at its end
            evs.append((int(end * 1000), len(evs), f"off {c} {n}"))
    evs.sort(key=lambda e: (e[0], e[1]))
    dur = int(round((end + test.tail) * 1000))
    dur = max(dur, int(end * 1000) + 100)
    return [(t + lead_ms, f"ev {t + lead_ms} {line}") for (t, o, line) in evs], dur + lead_ms


def setup_words(test):
    """the console words that set the scene, in order: (word, expected name or None)"""
    out = []
    for s in test.setup:
        if s[0] == "multi":
            out.append((f"load multi {s[1]}", s[2]))
        elif s[0] == "patch":
            out.append((f"load patch {s[1]} {s[2]}", s[3]))
        elif s[0] == "mode":
            out.append((f"mode {s[1]}", None))
        elif s[0] == "scene":
            out.append((f"scene {s[1]}", None))
        elif s[0] == "budget":
            out.append((f"budget {s[1]}", None))
        elif s[0] == "set":
            out.append(("set " + " ".join(s[1]), None))
        elif s[0] == "tune":
            out.append((" ".join(s[1]), None))
    return out


# ------------------------------------------------------------------------------------------------ links
class SerialLink:
    """the unit's console over its USB serial port"""

    synchronous = False

    def __init__(self, port):
        self.port, self.fd, self.buf = port, None, b""

    def open(self):
        self.fd = os.open(self.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        a = termios.tcgetattr(self.fd)
        a[0], a[1], a[2], a[3] = 0, 0, termios.CS8 | termios.CREAD | termios.CLOCAL, 0
        a[4] = a[5] = termios.B115200                 # (the unit ignores the rate)
        termios.tcsetattr(self.fd, termios.TCSANOW, a)
        self.buf = b""

    def close(self):
        if self.fd is not None:
            try:
                os.close(self.fd)
            except OSError:
                pass
        self.fd = None

    def now(self):
        return time.monotonic()

    def send(self, line):
        try:
            os.write(self.fd, line.encode("ascii") + b"\r")
        except (OSError, TypeError) as e:
            raise LinkDown(str(e))

    def read(self, secs):
        """the lines that arrive within secs (the partial last line is kept)"""
        out, end = [], time.monotonic() + secs
        while True:
            left = end - time.monotonic()
            try:
                r, _, _ = select.select([self.fd], [], [], max(left, 0))
                if r:
                    d = os.read(self.fd, 4096)
                    if not d:
                        raise LinkDown("end of file")
                    self.buf += d
            except (OSError, ValueError, TypeError) as e:
                raise LinkDown(str(e))
            while b"\n" in self.buf:
                ln, self.buf = self.buf.split(b"\n", 1)
                out.append(ln.decode("ascii", "replace").strip("\r "))
            if left <= 0:
                return out

    def sleep(self, secs):
        return self.read(secs)


class SimLink:
    """the Mac twin (tests/bench_sim.c --serve) as the unit: its time moves only when the runner waits"""

    synchronous = True

    def __init__(self, exe, boots=3):
        self.exe, self.boots, self.p, self.clock, self.pending = exe, boots, None, 0.0, []

    def open(self):
        self.p = subprocess.Popen([self.exe, "--serve", "--boots", str(self.boots)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
        self.clock = 0.0
        self.pending = []

    def close(self):
        if self.p:
            try:
                self.p.stdin.close()
            except OSError:
                pass
            self.p.wait()
        self.p = None

    def now(self):
        return self.clock

    def _talk(self, line):
        try:
            self.p.stdin.write(line + "\n")
            self.p.stdin.flush()
        except (OSError, ValueError, AttributeError) as e:
            raise LinkDown(str(e))
        out = []
        while True:
            ln = self.p.stdout.readline()
            if not ln:
                raise LinkDown("the simulated unit ended")
            ln = ln.rstrip("\r\n")
            if ln == "@done":
                return out
            out.append(ln)

    def send(self, line):
        self.pending = self._talk(line)

    def read(self, secs):
        out, self.pending = self.pending, []              # (the answer to the last word; the time moves only in sleep)
        return out

    def sleep(self, secs):
        self.clock += secs
        return self._talk(f"@adv {int(secs * 1000)}")

    def crash(self):
        try:
            self.p.stdin.write("@crash\n")
            self.p.stdin.flush()
        except OSError:
            pass


# ------------------------------------------------------------------------------------------------ the unit
REPLY_TAGS = ("OK", "ERR", "BENCH,STATE", "BENCH,INFO")


class Unit:
    """the console words and the unit's answers; a lost port raises LinkDown"""

    def __init__(self, link, log, reopen=None, wait_s=30.0):
        self.link, self.log, self.reopen, self.wait_s = link, log, reopen, wait_s
        self.bt = []                 # result rows (dicts) not yet taken
        self.head = None
        self.restarts = []

    def _take(self, lines):
        """log every line, pick the BT result lines out, return the others (the console echoes what is typed and its prompt: '> ')"""
        rest = []
        for ln in lines:
            self.log.write(ln + "\n")
            m = re.search(r"(?:^|> )(BTH|BT),(.*)$", ln)
            if m:
                f = m.group(2).split(",")
                if m.group(1) == "BTH":
                    self.head = f
                elif self.head and len(f) == len(self.head):
                    self.bt.append(dict(zip(self.head, f)))
                continue
            rest.append(re.sub(r"^(> )+", "", ln))
        self.log.flush()
        return rest

    def cmd(self, line, timeout=5.0):
        """send a console word, return its reply line (OK .. / ERR ..; `bench info` and `bench state`: their BENCH, line)"""
        self.log.write(f"## {line}\n")
        self.link.send(line)
        end = self.link.now() + timeout
        while True:
            for ln in self._take(self.link.read(0.1)):
                if ln.startswith(REPLY_TAGS):
                    return ln
            if self.link.synchronous or self.link.now() > end:
                raise LinkDown(f"no answer to {line!r}")

    def ok(self, line, timeout=5.0):
        r = self.cmd(line, timeout)
        if not r.startswith("OK"):
            raise RuntimeError(f"{line}: {r}")
        return r

    def lines_of(self, line, quiet=0.4, timeout=4.0):
        """a word with a many-line reply (status, crash): every line until the console is quiet"""
        self.log.write(f"## {line}\n")
        self.link.send(line)
        out, end = [], self.link.now() + timeout
        while self.link.now() < end:
            got = self._take(self.link.read(quiet))
            out += got
            if self.link.synchronous or (not got and out):
                break
        return out

    def sleep(self, secs):
        self._take(self.link.sleep(secs))

    def kv(self, word, quiet=0.4):
        d = {}
        for ln in self.lines_of(word, quiet=quiet):
            m = re.match(r"^([a-z_0-9]+) (-?\d+|0x[0-9A-Fa-f]+)$", ln.strip())
            if m:
                d[m.group(1)] = int(m.group(2), 0)
        return d

    def recover(self):
        """the port went away: wait for the unit (up to wait_s), then read what it says about the restart"""
        self.link.close()
        t0 = time.monotonic()
        while True:
            try:
                self.reopen()
                if self.kv("status").get("boots") is not None:
                    break
            except (LinkDown, OSError):
                pass
            if time.monotonic() - t0 > self.wait_s:
                raise LinkDown(f"the unit did not come back within {self.wait_s:.0f} s")
            time.sleep(0.5)
        st = self.kv("status")
        cr = "; ".join(x.strip() for x in self.lines_of("crash") if x.strip() and not x.startswith("crash"))
        info = {"boots": st.get("boots"), "uptime_ms": st.get("uptime_ms"), "crash": cr}
        self.restarts.append(info)
        return info


# ------------------------------------------------------------------------------------------------ one test
class Result(dict):
    pass


LEVELS = ("PASS", "WARN", "FAIL")

# ---- core 1 (BENCHT1 and later, DUELL_TWOCORE builds): what the unit says about its second core
TM_COLS = ["tmjobs", "tmposted", "tmowndown", "tmownpark", "tmownpause", "tmowndl", "tmbadclk", "tmsdmax", "tmsdmean", "tmrtmax", "tmrtmean", "tmmgmin", "tmmgmean",
           "tmvn", "tmvrtmax", "tmvrtmean", "tmvsdmax", "tmvblk0", "tmvblk1", "tmvblk2", "tmvblk3",
           *[f"hsd{i}" for i in range(8)], *[f"hrt{i}" for i in range(8)], *[f"hmg{i}" for i in range(8)],
           "tmstmax", *[f"tmst{i}" for i in range(4)], *[f"tmdl{i}" for i in range(4)], "tmslack", "tmcalm", "tmlrun", "tmvrun",
           "tmownfl", "tmvwink", "tmvwinms", "tmfloor"]
HIST_EDGES = {"hsd": ["<2", "<5", "<10", "<25", "<50", "<100", "<250", ">=250"],
              "hrt": ["<100", "<150", "<200", "<300", "<400", "<600", "<900", ">=900"],
              "hmg": ["<-500", "<-100", "<0", "<100", "<250", "<500", "<1000", ">=1000"]}
TC_COLS = ["tc", "tcwhy", "tcblocks", "tcintime", "tclate", "tcvlate", "tcsilent", "tcwait", "tcown", "tcrefused", "tcnoans", "tcpauses",
           "tcresumes", "tctrims", "tcparks", "tcvlok", "tcpfault", "tcprun", "tcpwin", "tcplate", "c1stack"]
TC_WHY = {"OK": "core 1 plays part 2",
          "NOANSWER": "core 1 did not answer at the start (the unit plays on one core)",
          "NOTARMED": "the start was not armed",
          "SKIPBOOT": "the boot guard skipped the start (the last start died within 30 s)",
          "NOTBUILT": "this build has no second core"}


def why_text(w):
    if w in TC_WHY:
        return TC_WHY[w]
    if w.startswith("FAIL-"):
        return f"core 1 was stopped during the session ({w[5:]}, a strike)"
    if w.startswith("PREV-"):
        return f"the last session ended in a strike ({w[5:]}): one core this power-on, the next power-on tries again"
    return w


def parse_cores(info_line):
    """(mode, why) from a `BENCH,INFO,...,cores 2,why OK` line: mode '2', '1', '-' (no core 1 in the build) or None (an older build)"""
    m = re.search(r"cores (\S+?),why (\S+)", info_line or "")
    return (m.group(1), m.group(2)) if m else (None, None)


def cores_line(mode, why):
    """the line at the top of the summary and printed at the start of a run"""
    if mode == "2":
        return "CORES: two cores active (core 1 plays part 2)"
    if mode == "1":
        return f"CORES: one core, reason {why} ({why_text(why)})"
    if mode == "-":
        return "CORES: one core, reason NOTBUILT (this build has no second core)"
    return "CORES: not reported (a build older than BENCHT1)"


def tc_int(r, k):
    try:
        return int(r.get(k) or 0)
    except ValueError:
        return 0


def tc_info(r):
    """the info text of a result (not a verdict): the blocks core 0 played part 2 itself ('own'), the pauses, the flash parks, the stack peak"""
    if r.get("tc") not in ("1", "2"):
        return ""
    bits = []
    if tc_int(r, "tcown"):
        bits.append(f"own {r['tcown']} (core 0 played part 2 itself)")
        if r.get("tmjobs", "") != "":
            bits[-1] += f" = {tc_int(r, 'tmowndown')} core 1 down + {tc_int(r, 'tmownpark')} flash park + {tc_int(r, 'tmownpause')} paused + {tc_int(r, 'tmowndl')} deadline already past"
            if r.get("tmownfl", "") != "":
                bits[-1] += f" + {tc_int(r, 'tmownfl')} floor (core 1's measured run time does not fit before the latest deadline; not a fault)"
    if tc_int(r, "tcpauses"):
        bits.append(f"{r['tcpauses']} pauses" + (f" (fault {tc_int(r, 'tcpfault')}, run {tc_int(r, 'tcprun')}, window {tc_int(r, 'tcpwin')}, late run {tc_int(r, 'tcplate')})" if r.get("tcvlok", "") != "" else ""))
    if tc_int(r, "tcparks"):
        bits.append(f"{r['tcparks']} flash parks")
    return "; ".join(bits)


def verdict(r, base, test):
    """PASS / WARN / FAIL and the reasons (stage 1: no baseline run yet, so only the thresholds of BENCH_SUITE_DESIGN.md 5.4 that need none)"""
    why, level = [], 0

    def bump(l, text):
        nonlocal level
        level = max(level, l)
        why.append(text)
    if r.get("status") != "DONE":
        bump(2, r.get("status", "?"))
    try:
        miss = int(r["miss"])
        pk = float(r["peak"])
        if miss:
            bump(1 if "STRESS" in test.name else 2, f"{miss} missed halves")
        if pk > 100:
            bump(2, f"peak {pk} %")
        elif pk >= 95:
            bump(1, f"peak {pk} %")
        if r["ramcode"] not in ("OK", "-"):
            bump(2, f"RAM CODE {r['ramcode']}")
        if int(r["lost"]):
            bump(2, f"{r['lost']} events lost")
        if int(r["boots"]) != base["boots"]:
            bump(2, "the unit restarted")
        for k in ("uac_under", "uac_over", "uac_missed", "uac_stalls"):
            d = int(r[k]) - base.get(k, 0)
            lim = base.get("under_limit", 0) if k == "uac_under" else 0       # (BENCHR2: the underrun threshold is a setting; the others stay at 0)
            if d > lim:
                bump(2, f"USB audio {k[4:]} +{d}" + (" after the mark" if base.get("marked") else "") + (f" (limit {lim})" if lim else ""))
        if int(r["uac_hz"]) != 44100:
            bump(1, f"USB audio rate {r['uac_hz']}")
        if test.name.startswith("FLASH") and int(r["flashes"]) < 1:
            bump(2, "no flash write happened")
        if r.get("tc") in ("1", "2"):                       # core 1 (rules of BENCH_TWOCORE1.md): very late or silent = FAIL, late = WARN, own = info
            vl, sl, lt = tc_int(r, "tcvlate"), tc_int(r, "tcsilent"), tc_int(r, "tclate")
            if base.get("cores") == "2" and r["tc"] == "1":
                bump(2, f"core 1 stopped during the session ({r['tcwhy']})")
            diag = getattr(test, "diag", False)
            if r.get("tcvlok", "") != "":                   # BENCHT3 and later: a very late block whose job finished in its half and paused nothing is a WARN with its count; a pause is a FAIL
                vok, ps = tc_int(r, "tcvlok"), tc_int(r, "tcpauses")
                if vok:
                    bump(1, f"{vok} very late blocks (the job finished inside its wait limit; nothing paused)")
                if ps:
                    bump(1 if diag else 2, f"{ps} pauses (fault {tc_int(r, 'tcpfault')}, run of very late {tc_int(r, 'tcprun')}, window {tc_int(r, 'tcpwin')}, run of late {tc_int(r, 'tcplate')}): core 0 played part 2 itself for the calm period" + (" (diagnosis run: counted)" if diag else ""))
            elif vl:
                if diag:
                    bump(1, f"{vl} very late blocks (diagnosis run: counted, not a fault here)")
                else:
                    bump(2, f"{vl} very late blocks (core 1 missed the deadline)")
            if sl:
                bump(2, f"{sl} silent blocks (part 2 was silent)")
            if tc_int(r, "tcrefused"):
                bump(2, f"{r['tcrefused']} refused jobs")
            if tc_int(r, "tcnoans"):
                bump(2, f"{r['tcnoans']} blocks core 1 did not answer")
            if lt:
                bump(1, f"{lt} late blocks")
    except (KeyError, ValueError) as e:
        bump(2, f"result unreadable ({e})")
    return LEVELS[level], "; ".join(why)


class RecCfg:
    """(BENCHR2) the settings of a recorded test; every one is a command line option of bench_run.py
    settle      seconds the recording runs before anything is set up: the USB stream starts and settles (the unit's underrun counter moves at a stream start); the counters'
                baseline is read AFTER it                                                                                                        --rec-settle
    setup_wait  seconds to wait after the scene is set, for the setup noise to pass (None: the test's own `settle`, 1.5 s). The unit has no ready marker: its only answer to
                the setup words is `OK`, and the per-test reset comes with `bench run`, so a fixed wait is the marker                              --setup-wait
    lead        seconds of silence the timeline is held back after `bench run`: the unit's per-test reset (the effect lines and phases are cleared in the first half of the run)
                makes its noise there, before the first note                                                                                      --lead
    mark_after  seconds after the run command at which the START MARK is put (see bench_record.mark_position)                                      --mark-after
    under_limit USB underruns after the mark that are still a PASS (0: any underrun after the mark is a FAIL)                                       --under-limit
    match_s     the click of a two-core test is "the same" as one of its one-core reference when both are within this many seconds of the first note  --match-ms"""

    def __init__(self, settle=1.0, setup_wait=None, lead=0.6, mark_after=0.35, under_limit=0, match_s=0.06):
        self.settle, self.setup_wait, self.lead, self.mark_after, self.under_limit, self.match_s = settle, setup_wait, lead, mark_after, under_limit, match_s

    def check(self):
        if self.lead < self.mark_after + 0.1:
            raise SuiteError(f"--lead {self.lead:g} s must be at least 0.1 s more than --mark-after {self.mark_after:g} s (the mark has to fall before the first note)")
        if min(self.settle, self.lead, self.mark_after) < 0:
            raise SuiteError("the recording settings cannot be negative")

    def text(self):
        return (f"RECORDING: stream settles {self.settle:g} s before the setup; setup wait " + (f"{self.setup_wait:g} s" if self.setup_wait is not None else "the test's own settle") +
                f"; silent lead-in {self.lead:g} s; mark {self.mark_after:g} s after the run command; USB underruns after the mark allowed: {self.under_limit}; "
                f"a one-core click counts as the same within {self.match_s * 1000:g} ms")


COUNTER_KEYS = ("uac_underruns", "uac_overruns", "uac_missed", "uac_stalls")


def counters(unit, quiet=0.4):
    """the unit's USB audio counters now (a `status` read; the unit's answer is made at once, `quiet` is how long the Mac waits for the rest of the list)"""
    st = {}
    for q in (quiet, 0.4, 0.8):
        st = unit.kv("status", quiet=q)
        if all(k in st for k in COUNTER_KEYS) or unit.link.synchronous:
            break
    return {k: st.get(k, 0) for k in COUNTER_KEYS}


def run_test(unit, test, soak_override, base, after_run=None, recorder=None, outdir=None, rc=None, audio_name=None):
    """one run of a test. Without a recorder: as before. With one (BENCHR2) the order is: start the recording; wait for the stream to settle; read the USB counters (baseline);
    reset and set the scene; wait for the setup noise to pass; send `bench run` (the Mac notes how much has been recorded: run_s); the unit resets its phases and holds the
    timeline back by the lead-in; at mark_after the counters are read again (the mark); the result's counters give the end. Setup = before the mark, test = after it."""
    res = Result(test=test.name, text=test.text, expect=test.expect)
    rc = rc or RecCfg()
    lead_ms = int(round(rc.lead * 1000)) if recorder else 0
    evs, dur = expand(test, soak_override, lead_ms)
    res["dur_s"] = (dur - lead_ms) / 1000.0
    unit.bt.clear()
    c0 = None
    if recorder:                                            # the recording runs first: the stream start, the scene load and the reset are all in it, before the mark
        recorder.start(recorder.path_for(outdir, audio_name or test.name))
        unit.sleep(rc.settle)
        c0 = counters(unit)                                 # the baseline AFTER the stream has settled
    unit.ok("bench reset")
    unit.sleep(0.3)
    for (word, want) in setup_words(test):
        try:
            rep = unit.ok("bench " + word)
        except RuntimeError as e:
            if "unknown word" in str(e) and word.split()[0] in ("slack", "calm", "pause", "lrun", "vrun", "vwin", "floor"):
                raise RuntimeError(f"{word}: this build has no runtime switches (BENCHT3 or newer needed for vwin and floor, BENCHT2 for the others): {e}")
            raise
        if want and want.upper() not in rep.upper():
            res.update(status="SKIPPED", verdict="SKIP", why=f"{word}: the unit says {rep!r}, the suite expects {want!r}")
            if recorder:
                recorder.abort()
            return res
    unit.sleep(rc.setup_wait if (recorder and rc.setup_wait is not None) else test.settle)
    if test.measure:
        unit.ok(f"bench measure {int(test.measure[0] * 1000) + lead_ms} {int(test.measure[1] * 1000) + lead_ms}")
    elif test.soak:
        unit.ok(f"bench measure {2000 + lead_ms} {dur - 1000}")
    elif lead_ms:
        unit.ok(f"bench measure {lead_ms} {dur}")               # (the CPU window is the test itself, not the silent lead-in)
    queue = list(evs)
    free = 256
    late = 0
    checks = []

    def feed(limit_ms):
        """send the events due before limit_ms of the test's clock, while the unit has room"""
        nonlocal free, late
        while queue and queue[0][0] <= limit_ms and free > 4:
            rep = unit.cmd("bench " + queue[0][1])
            if rep.startswith("ERR full"):
                free = 0
                return
            if not rep.startswith("OK"):
                raise RuntimeError(f"{queue[0][1]}: {rep}")
            free = int(rep.split()[1])
            queue.pop(0)
    feed(10 ** 9)                                           # before the run: as many as fit
    run_pos = recorder.position() if recorder else None     # the START MARK's anchor: how much sound the recording holds as the run command goes out
    unit.ok(f"bench run {test.name[:19]} {dur}")
    t_run = unit.link.now()
    if after_run:
        after_run()
    cm = None
    if recorder:
        unit.sleep(rc.mark_after)                           # the unit's reset noise and the first part of the lead-in pass; this is the mark
        cm = counters(unit, 0.15)
    next_check = test.check if (test.soak or test.check_on) else None
    deadline = t_run + dur / 1000.0 + 20.0
    while not unit.bt:
        el = unit.link.now() - t_run
        if queue and free <= 4:                              # the ring was full: ask how many places the unit has now
            st = unit.cmd("bench state")
            m = re.search(r"evfree (\d+)", st)
            free = int(m.group(1)) if m else 0
        if queue and queue[0][0] < (el - 0.05) * 1000 and free > 4:
            late += 1                                        # (it plays at once: a warning)
        feed((el + 15.0) * 1000)
        if next_check is not None and el >= next_check:
            c = unit.kv("status")
            inf = [x for x in unit.lines_of("bench info") if x.startswith("BENCH,INFO")]
            m = re.search(r"ramcode ([A-Z0-9 -]+?),", inf[0]) if inf else None
            checks.append((int(el), m.group(1) if m else "?", c.get("boots"), c.get("cpu_pct"), c.get("audio_late"), c.get("voices_shed")))
            next_check += test.check
        if unit.bt:
            break
        unit.sleep(0.2)
        if unit.link.now() > deadline:
            raise LinkDown("no result line: the unit hangs or the link is lost")
    row = unit.bt.pop(0)
    rec_info = None
    if recorder:
        unit.sleep(0.4)                                      # (the USB audio is a little behind the unit's own clock)
        rec_info = recorder.stop()
    res.update(row)
    tbase = base
    if rec_info is not None and cm is not None:             # BENCHR2: the counters are judged from the mark, not from the start of the session
        rec_info.run_s, rec_info.lead_s = run_pos, rc.lead
        from bench_record import mark_position
        rec_info.mark_s = mark_position(run_pos, rc.mark_after)
        tbase = {**base, "uac_under": cm["uac_underruns"], "uac_over": cm["uac_overruns"], "uac_missed": cm["uac_missed"], "uac_stalls": cm["uac_stalls"],
                 "under_limit": rc.under_limit, "marked": True}
        res["under_pre"] = cm["uac_underruns"] - c0["uac_underruns"]                 # before the mark (stream start, setup, the unit's reset): information
        res["under_post"] = int(row.get("uac_under", 0)) - cm["uac_underruns"]       # after the mark: the test
        res["lead_s"] = rc.lead
    res["vlate_ev"] = []
    if tc_int(row, "tmvn"):                                  # BENCHT2: the newest very late jobs (when, which block, voices, delay / run / margin in us)
        for ln in unit.lines_of("bench tcm"):
            f = ln.strip().split(",")
            if len(f) == 9 and f[0] == "BENCH" and f[1] == "TCM" and all(re.fullmatch(r"-?\d+", x) for x in f[2:]):
                res["vlate_ev"].append(f[2:])
    res["late_events"] = late
    res["checks"] = checks
    res["verdict"], res["why"] = verdict(row, tbase, test)
    res["info"] = tc_info(row)
    if res.get("under_pre") and rec_info is not None:
        res["info"] = (res["info"] + "; " if res["info"] else "") + f"USB underruns before the mark: +{res['under_pre']} (stream start and setup, not counted)"
    if late:
        res["verdict"] = LEVELS[max(LEVELS.index(res["verdict"]), 1)]
        res["why"] = (res["why"] + "; " if res["why"] else "") + f"{late} events sent late"
    bad = [c for c in checks if not (c[1] == "-" or c[1].split(" ")[0] == "OK")]   # (INFO says "OK 42 KB": only FAIL ... is bad)
    if bad:
        res["verdict"], res["why"] = "FAIL", f"RAM CODE {bad[0][1]} at {bad[0][0]} s"
    res["_rec_info"] = rec_info                              # (stage 2: bench_record.apply_audio analyzes it after the counters' verdict)
    return res


# ------------------------------------------------------------------------------------------------ the run
def pick_port(a):
    if a.port:
        return a.port
    ports = sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))
    if len(ports) != 1:
        raise SystemExit(("no serial port of the unit found" if not ports else f"more than one port ({', '.join(ports)}): use --port") +
                         ": is it plugged in? bench builds present the console by themselves")
    return ports[0]


def tc_cell(r):
    """the core 1 column of the summary: '-' (none), '1c' (one core), '2c ok' or '2c L3 V0 S0' (late, very late, silent blocks)"""
    if r.get("tc") not in ("1", "2"):
        return "-"
    if r["tc"] == "1":
        return "1c"
    return "2c ok" if not (tc_int(r, "tclate") or tc_int(r, "tcvlate") or tc_int(r, "tcsilent")) else \
        f"2c L{tc_int(r, 'tclate')} V{tc_int(r, 'tcvlate')} S{tc_int(r, 'tcsilent')}"


def audio_cell(r):
    """the audio columns of the main summary table (--record): 'clicks/gaps', and '+n/+m' above the one-core reference's when there is one; '-' not recorded, '?' capture fault"""
    if not r.get("audio"):
        return "-"
    if r.get("audio_fault"):
        return "? (capture fault)"
    c = f"{r.get('clicks', '?')}/{r.get('gaps', '?')}"
    if r.get("clk_vs_1c", "") != "":
        c += f" (+{r['clk_vs_1c']}/+{r['gap_vs_1c']})" if int(r["clk_vs_1c"]) > 0 or int(r["gap_vs_1c"]) > 0 else " (=1c)"
    elif r.get("onecore_of"):
        c += " (1c ref)"
    return c


def lab(r):
    """a result's name in the tables: NAME, or NAME#k when the test was repeated"""
    return r.get("label") or r["test"]


def _swap_audio_why(row, new):
    """put a new audio reason into a row's why in place of the one apply_audio wrote"""
    old = row.get("audio_why", "")
    w = row.get("why", "")
    if old and ("audio: " + old) in w:
        w = w.replace("audio: " + old, "").strip("; ").replace(";;", ";")
    row["audio_why"] = new
    row["why"] = (w + "; " if w else "") + "audio: " + new


def _unmatched(ts, ref_ts, tol):
    """the times in ts that have no time in ref_ts within tol"""
    return [t for t in ts if not any(abs(t - u) <= tol for u in ref_ts)]


def apply_onecore(results, tests, match_s=0.06):
    """RECORD-1/2: the one-core reference. For every test that says `onecore-of NAME`, the recorded clicks and gaps of the reference are what the scene has anyway; NAME's
    clicks and gaps that the reference does not have are the two-core excess (columns clk_vs_1c, gap_vs_1c). The reference itself never fails on clicks or gaps (WARN: they are the
    scene's own); NAME fails on audio only for the excess (every click of NAME also in the reference: WARN). The counters' verdict (verdict_cnt, before the audio) is kept.
    BENCHR2: repeat k of NAME is compared with repeat k of the reference (with all the reference's repeats when it has fewer), and a click or gap is "the same" when it lies within
    match_s of one in the reference, in seconds after the first note (the clicks' times are known only for a recording with a start mark; without them the comparison is by count,
    as in RECORD-1). A reference on which core 1 was still given jobs is no reference."""
    def rows_of(name):
        return sorted((r for r in results if r["test"] == name), key=lambda r: int(r.get("rep", 1) or 1))
    for t in tests:
        ref_name = getattr(t, "onecore_of", None)
        if not ref_name:
            continue
        refs, twos = rows_of(t.name), rows_of(ref_name)
        if not refs or not twos:
            continue
        good = []
        for ref in refs:
            if not ref.get("audio"):
                continue
            ref["onecore_of"] = ref_name
            if ref.get("audio_fault"):
                continue                                   # (a capture fault: no comparison, both stay WARN)
            posted, blocks = tc_int(ref, "tmposted"), tc_int(ref, "tcblocks")
            if ref.get("tc") == "2" and blocks and posted * 100 > blocks:   # core 1 was given jobs: this was not a one-core run (the slack did not take, or the twin, which has no core 1 to switch off)
                _swap_audio_why(ref, f"NOT A ONE-CORE RUN: core 1 was given {posted} jobs in {blocks} blocks; no comparison")
                ref["verdict"] = LEVELS[max(LEVELS.index(ref.get("verdict", "PASS")), 1)]
                continue
            good.append(ref)
            ref["clk_vs_1c"] = ref["gap_vs_1c"] = ""
            base_ref = ref.get("verdict_cnt", "PASS")
            if ref.get("audio_verdict") == "FAIL":
                ref["audio_verdict"] = "WARN"
                _swap_audio_why(ref, f"ONE-CORE REFERENCE: {ref['clicks']} clicks, {ref['gaps']} gaps the scene has anyway")
                ref["verdict"] = LEVELS[max(LEVELS.index(base_ref), 1)]
            elif ref.get("audio_verdict") == "PASS":
                ref["audio_why"] = ref.get("audio_why") or "ONE-CORE REFERENCE: clean"
        if not good:
            continue
        for i, two in enumerate(twos):
            if not two.get("audio") or two.get("audio_fault"):
                continue
            mate = [g for g in good if int(g.get("rep", 1) or 1) == int(two.get("rep", 1) or 1)]
            pool = mate or good                              # (the same repeat; else every repeat of the reference)
            rc_, rg_ = sum(int(g["clicks"]) for g in pool), sum(int(g["gaps"]) for g in pool)
            tcl, tg = int(two["clicks"]), int(two["gaps"])
            timed = "_clicks_t" in two and all("_clicks_t" in g for g in pool)
            if timed:
                un_c = _unmatched(two["_clicks_t"], [u for g in pool for u in g["_clicks_t"]], match_s)
                un_g = _unmatched(two["_gaps_t"], [u for g in pool for u in g["_gaps_t"]], match_s)
                ex_c, ex_g = len(un_c), len(un_g)
            else:
                ex_c, ex_g = max(0, tcl - rc_), max(0, tg - rg_)
            two["clk_vs_1c"], two["gap_vs_1c"] = str(ex_c), str(ex_g)
            if two.get("audio_verdict") == "FAIL":
                base_two = two.get("verdict_cnt", "PASS")
                if ex_c or ex_g:
                    txt = f"{ex_c} clicks and {ex_g} gaps more than on one core ({rc_} / {rg_} there, {tcl} / {tg} here)"
                    if timed:
                        txt += " at " + ", ".join([f"click {t:.3f}" for t in un_c[:6]] + [f"gap {t:.3f}" for t in un_g[:6]]) + " s after the first note, not in the one-core reference at that time"
                    lvl = 2
                else:
                    two["audio_verdict"] = "WARN"
                    txt = (f"{tcl} clicks, {tg} gaps, none more than on one core ({rc_} / {rg_})" +
                           (" (each also in the one-core reference at the same repeat and time)" if timed else "") + ": the scene's own")
                    lvl = 1
                _swap_audio_why(two, txt)
                two["verdict"] = LEVELS[max(LEVELS.index(base_two), lvl)]
    return results


def _f(r, k):
    try:
        return float(r.get(k, ""))
    except (TypeError, ValueError):
        return None


def _ints(rows, k):
    out = []
    for r in rows:
        try:
            out.append(int(r.get(k, "")))
        except (TypeError, ValueError):
            out.append(None)
    return out


def aggregate(results, tests):
    """(BENCHR2) one record per test that was run (a repeated test: all its repeats together). A test passes only if every repeat is clean (PASS); the reasons name the repeat
    numbers and the click times. Lists are per repeat, in order; None = not measured (a test that was not recorded, a repeat that broke off)."""
    out, seen = [], set()
    for t in tests:
        rows = [r for r in results if r["test"] == t.name]
        if not rows or t.name in seen:
            continue
        seen.add(t.name)
        done = [r for r in rows if r.get("status") in ("DONE", "STOPPED")]
        avg = [x for x in (_f(r, "avg") for r in done) if x is not None]
        pk = [x for x in (_f(r, "peak") for r in done) if x is not None]
        lv = [LEVELS.index(r["verdict"]) for r in rows if r.get("verdict") in LEVELS]
        if not lv:
            verdict_ = "SKIP"
        else:
            verdict_ = LEVELS[max(lv)]
        reasons = []
        multi = len(rows) > 1
        for r in rows:
            if r.get("verdict") in ("WARN", "FAIL") or r.get("verdict") == "SKIP":
                reasons.append((f"repeat {r.get('rep', 1)}: " if multi else "") + (r.get("why") or r.get("verdict")))
        ev = _ints(rows, "note_events")
        out.append(dict(test=t.name, reps=len(rows), rows=rows, verdict=verdict_, passes=sum(1 for r in rows if r.get("verdict") == "PASS"),
                        clicks=_ints(rows, "clicks"), gaps=_ints(rows, "gaps"), setup_clicks=_ints(rows, "setup_clicks"), setup_gaps=_ints(rows, "setup_gaps"),
                        under_post=_ints(rows, "under_post"), under_pre=_ints(rows, "under_pre"), edge=_ints(rows, "attack_clicks"), note_events=next((e for e in ev if e), None),
                        lag=[_f(r, "onset_lag_s") for r in rows],
                        cpu_mean=sum(avg) / len(avg) if avg else None, cpu_min=min(avg) if avg else None, cpu_max=max(avg) if avg else None, peak_max=max(pk) if pk else None,
                        reasons=reasons, recorded=any(r.get("audio") for r in rows)))
    return out


def _lst(v):
    return "[" + ", ".join("-" if x is None else (f"{x:g}" if isinstance(x, float) else str(x)) for x in v) + "]"


def fmt_repeats(aggs):
    """the REPEATS section of summary.txt: an aggregate line per test (empty unless some test ran more than once)"""
    if not any(a["reps"] > 1 for a in aggs):
        return ""
    L = ["", "REPEATS (a test passes only if every repeat is clean; lists are per repeat, in order; clicks and gaps are those AFTER the mark; setup = before the mark, never a FAIL)"]
    for a in aggs:
        cpu = f"CPU avg {a['cpu_mean']:.1f}/{a['cpu_min']:.1f}/{a['cpu_max']:.1f} (mean/min/max) peak max {a['peak_max']:.1f}" if a["cpu_mean"] is not None else "CPU -"
        rec = (f"clicks {_lst(a['clicks'])}  gaps {_lst(a['gaps'])}  " if a["recorded"] else "not recorded  ")
        L.append(f"{a['test']:<14} runs {a['reps']}  pass {a['passes']}/{a['reps']}  {rec}{cpu}  USB underruns after mark {_lst(a['under_post'])}  {a['verdict']}")
        if a["recorded"]:
            sc = _lst([f"{c}/{g}" if c is not None else None for c, g in zip(a["setup_clicks"], a["setup_gaps"])])
            L.append(f"{'':<14} setup clicks/gaps {sc}   USB underruns before mark {_lst(a['under_pre'])}   note-edge clicks excluded {_lst(a['edge'])}"
                     + (f" (of {a['note_events']} note events)" if a["note_events"] else "") + f"   first-note lag s {_lst(a['lag'])}")
        for rs in a["reasons"][:6]:
            L.append(f"{'':<14} {a['verdict'] if a['verdict'] != 'PASS' else ''} {rs}"[:400])
    n = {v: sum(1 for a in aggs if a["verdict"] == v) for v in ("PASS", "WARN", "FAIL", "SKIP")}
    L.append(f"{len(aggs)} tests: {n['PASS']} PASS  {n['WARN']} WARN  {n['FAIL']} FAIL  {n['SKIP']} SKIP")
    return "\n".join(L)


def pause_word(r):
    lr, vr = tc_int(r, "tmlrun"), tc_int(r, "tmvrun")
    if r.get("tmvwink", "") == "":                       # BENCHT2: one very late block pauses
        if lr >= 1000 and vr >= 1000:
            return "off"
        return "on" if (lr, vr) == (4, 1) else f"L{lr}V{vr}"
    wk = tc_int(r, "tmvwink")
    if lr >= 1000 and ((vr >= 1000) or (vr == 0 and wk == 0)):   # (BENCHT3: vrun 1000; BENCHT4: vrun 0 and no window)
        return "off"
    if (lr, vr, wk) == (4, 0, 0):                        # BENCHT4's rule: only faults, an unfinished job, a run of late blocks
        return "new"
    if (lr, vr, wk) == (4, 4, 8):                        # BENCHT3's rule: also 4 very late in a row, or 8 in 2 s
        return "b3"
    return "old" if (lr, vr) == (4, 1) else f"L{lr}V{vr}W{wk}"


def fmt_timing(results):
    """BENCHT2: the core 1 timing section of the summary (empty when no result carries the figures)"""
    rs = [r for r in results if r.get("status") in ("DONE", "STOPPED") and r.get("tmjobs", "") != "" and r.get("tc") == "2"]
    if not rs:
        return ""
    L = ["", "CORE 1 TIMING (us unless said; jobs = jobs core 1 ran and finished; margin = the block's deadline less core 1's finish, negative = after it; BENCH_TWOCORE2.md)",
         f"{'test':<18}{'slack':>6}{'calm':>6}{'pause':>6}{'floor':>6}{'jobs':>8}{'vlate':>6}{'vl-ok':>6}{'late':>6}{'paus':>6} {'f/r/w':>8} | {'own: down':>9}{'park':>5}{'paused':>8}{'dl-past':>8}{'floor':>6} | {'delay m/max':>12}{'run m/max':>12}{'margin min/mean':>17}{'busy%':>7}"]
    for r in rs:
        new = r.get("tcvlok", "") != ""
        L.append(f"{lab(r):<18}{tc_int(r, 'tmslack'):>6}{tc_int(r, 'tmcalm'):>6}{pause_word(r):>6}{(r.get('tmfloor') or '-') if new else '-':>6}{tc_int(r, 'tmjobs'):>8}{tc_int(r, 'tcvlate'):>6}{tc_int(r, 'tcvlok') if new else '-':>6}{tc_int(r, 'tclate'):>6}{tc_int(r, 'tcpauses'):>6} "
                 f"{(str(tc_int(r, 'tcpfault')) + '/' + str(tc_int(r, 'tcprun')) + '/' + str(tc_int(r, 'tcpwin'))) if new else '-':>8} | "
                 f"{tc_int(r, 'tmowndown'):>9}{tc_int(r, 'tmownpark'):>5}{tc_int(r, 'tmownpause'):>8}{tc_int(r, 'tmowndl'):>8}{tc_int(r, 'tmownfl') if new else '-':>6} | "
                 f"{str(tc_int(r, 'tmsdmean')) + '/' + str(tc_int(r, 'tmsdmax')):>12}{str(tc_int(r, 'tmrtmean')) + '/' + str(tc_int(r, 'tmrtmax')):>12}"
                 f"{str(tc_int(r, 'tmmgmin')) + '/' + str(tc_int(r, 'tmmgmean')):>17}{r.get('c1', ''):>7}")
    L += ["", "very late jobs, as they were measured: run mean/max, delay max, and which block of the half (0..3) they were in"]
    for r in rs:
        if tc_int(r, "tmvn"):
            L.append(f"  {lab(r):<18}{tc_int(r, 'tmvn'):>6} jobs   run {tc_int(r, 'tmvrtmean')}/{tc_int(r, 'tmvrtmax')}   delay max {tc_int(r, 'tmvsdmax')}   blocks 0/1/2/3: "
                     f"{tc_int(r, 'tmvblk0')}/{tc_int(r, 'tmvblk1')}/{tc_int(r, 'tmvblk2')}/{tc_int(r, 'tmvblk3')}")
    L += ["", "where in the half (us after the half's start; mean over the jobs of each block of the half): the job's start / its deadline, and the room between them"]
    for r in rs:
        L.append(f"  {lab(r):<18}" + "   ".join(f"b{i} {tc_int(r, 'tmst' + str(i)):>4}/{tc_int(r, 'tmdl' + str(i)):>4} ({tc_int(r, 'tmdl' + str(i)) - tc_int(r, 'tmst' + str(i)):>4})" for i in range(4)) + f"   latest start {tc_int(r, 'tmstmax')}")
    for key, title in (("hrt", "run time of a job"), ("hmg", "finish margin"), ("hsd", "start delay")):
        L += ["", f"histogram, {title} (jobs per bin, us):  " + "  ".join(f"{e:>6}" for e in HIST_EDGES[key])]
        for r in rs:
            L.append(f"  {lab(r):<18}" + "  ".join(f"{tc_int(r, key + str(i)):>6}" for i in range(8)))
    return "\n".join(L)


def fmt_summary(name, build, tests, results, cores=None):
    refs = {t.name: t.ref for t in tests if getattr(t, "ref", None)}
    aud = any(r.get("audio") for r in results)           # (--record: the audio columns stand beside the core 1 counters)
    marked = aud and any(r.get("mark_s") not in (None, "") for r in results)     # (BENCHR2: the setup column: clicks/gaps before the mark)
    lines = [cores or cores_line(None, None), f"DUELL {build}  suite {name}  {datetime.datetime.now():%Y-%m-%d %H:%M}",
             f"{'test':<20}{'CPU avg/peak/miss':<24}" + (f"{'ref avg/peak':<18}" if refs else "") + f"{'voices':>7}{'NOTES':>8}  {'core 1':<16}" + (f"{'clk/gap (+vs 1c)':<18}" if aud else "") + (f"{'setup c/g':<10}" if marked else "") + f"{'verdict':<8} why"]
    for r in results:
        if r.get("status") not in ("DONE", "STOPPED"):
            lines.append(f"{lab(r):<20}{'-':<24}" + (f"{'':<18}" if refs else "") + f"{'':>7}{'':>8}  {'-':<16}" + (f"{audio_cell(r):<18}" if aud else "") + (f"{'-':<10}" if marked else "") + f"{r['verdict']:<8} {r.get('why', '')}")
            continue
        cpu = f"{r['avg']} / {r['peak']} / {r['miss']}"
        why = r.get("why", "")
        if r.get("info"):
            why += (" " if why else "") + f"[info: {r['info']}]"
        rf = refs.get(r["test"])
        lines.append(f"{lab(r):<20}{cpu:<24}" + (f"{(rf[0] + ' / ' + rf[1]) if rf else '':<18}" if refs else "") + f"{r['vox']:>7}{r['notes1'] + '+' + r['notes2']:>8}  {tc_cell(r):<16}" + (f"{audio_cell(r):<18}" if aud else "")
                     + (f"{(str(r.get('setup_clicks', '')) + '/' + str(r.get('setup_gaps', ''))) if r.get('mark_s') not in (None, '') else '-':<10}" if marked else "") + f"{r['verdict']:<8} {why}")
    n = {v: sum(1 for r in results if r.get("verdict") == v) for v in ("PASS", "WARN", "FAIL", "SKIP")}
    lines.append(f"{n['PASS']} PASS  {n['WARN']} WARN  {n['FAIL']} FAIL  {n['SKIP']} SKIP")
    rp = fmt_repeats(aggregate(results, tests))
    return "\n".join(lines) + (rp if rp else "") + fmt_timing(results)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0], formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--suite", default=DEFAULT_SUITE, help="the suite file, or a name in tools/bench_suites (default OWNER-1)")
    ap.add_argument("--port", help="the unit's serial port (default: the only /dev/cu.usbmodem*)")
    ap.add_argument("--out", help="the folder for this run (default: bench_runs/<date>-<build>/ )")
    ap.add_argument("--runs", default=os.environ.get("DUELL_BENCH_RUNS", RUNS_DEFAULT), help="the bench_runs folder (default as the owner's decision 10)")
    ap.add_argument("--only", help="test names, comma separated (a * matches any)")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--dry-run", action="store_true", help="print the console words of every test, no unit")
    ap.add_argument("--soak", type=float, help="every soak this many seconds")
    ap.add_argument("--no-soak", action="store_true")
    ap.add_argument("--sim", help="the Mac twin (build/host/bench_sim) instead of a unit")
    ap.add_argument("--sim-crash-in", help="(host test) the simulated unit restarts during this test")
    ap.add_argument("--two-core", action="store_true", help="require two cores: stop before the first test if the unit's session is on one core. "
                    "The Mac cannot restart the unit, so this arms nothing: a BENCHT1 build arms core 1 itself at every power-on (unplug and plug in)")
    ap.add_argument("--no-caffeinate", action="store_true")
    ap.add_argument("--record", action="store_true", help="stage 2: record the unit's USB audio (DUELL input at 44.1 kHz) during every test and analyze it")
    ap.add_argument("--keep", type=int, default=5, help="with --record: the audio of this many newest runs is kept (and every baseline); older audio is deleted")
    ap.add_argument("--keep-all", action="store_true", help="with --record: never delete any audio (the 5-runs rule is off)")
    ap.add_argument("--repeat", type=int, help="run every selected test this many times back to back (overrides the suite's `repeat N` lines); a test passes only if all repeats are clean")
    ap.add_argument("--rec-settle", type=float, default=1.0, help="with --record: seconds the recording runs before the setup, for the USB stream to settle; the USB counters' baseline is read after it (default 1.0)")
    ap.add_argument("--setup-wait", type=float, help="with --record: seconds to wait after the scene is set, for the setup noise to pass (default: the test's own `settle`, 1.5)")
    ap.add_argument("--lead", type=float, default=0.6, help="with --record: seconds of silence between `bench run` and the timeline's first event, in which the unit's per-test reset makes its noise (default 0.6)")
    ap.add_argument("--mark-after", type=float, default=0.35, help="with --record: the start mark is this many seconds after the run command in the recording (default 0.35; the lead must be 0.1 s longer)")
    ap.add_argument("--under-limit", type=int, default=0, help="with --record: USB underruns after the mark that still pass (default 0: any underrun after the mark is a FAIL; before the mark they are information)")
    ap.add_argument("--match-ms", type=float, default=60.0, help="with --record: a two-core click is the one-core reference's when within this many ms of it (default 60)")
    ap.add_argument("--rec-device", default="DUELL", help="the Mac's input device of the unit")
    ap.add_argument("--prove-chain", action="store_true", help="stage 2: needs a tone build (BENCHA1): record the test triangle and check every frame")
    ap.add_argument("--minutes", type=float, default=10.0, help="with --prove-chain: how long to record (default 10)")
    a = ap.parse_args(argv)
    if a.prove_chain:
        return prove_chain(a)
    path = a.suite if os.path.exists(a.suite) else os.path.join(HERE, "bench_suites", a.suite + ".txt")
    with open(path) as f:
        text = f.read()
    try:
        sname, tests = parse_suite(text)
    except SuiteError as e:
        raise SystemExit(f"{path}: {e}")
    sname = sname or os.path.basename(path)
    sel = [t for t in tests if not (a.no_soak and t.soak)]
    if a.only:
        pats = [re.compile("^" + re.escape(p).replace(r"\*", ".*") + "$") for p in a.only.split(",")]
        sel = [t for t in sel if any(p.match(t.name) for p in pats)]
        if not sel:
            raise SystemExit("no test matches --only")
    rc = RecCfg(a.rec_settle, a.setup_wait, a.lead, a.mark_after, a.under_limit, a.match_ms / 1000.0)
    try:
        rc.check()
    except SuiteError as e:
        raise SystemExit(str(e))
    a.rc = rc
    if a.repeat is not None:
        if not 1 <= a.repeat <= 50:
            raise SystemExit("--repeat N: 1 to 50")
        for t in sel:
            t.repeat = a.repeat
    if a.list or a.dry_run:
        tot = 0.0
        for t in sel:
            evs, dur = expand(t, a.soak)
            tot += (dur / 1000 + t.settle + 0.6 + ((rc.settle + rc.lead + 0.4) if a.record else 0.0)) * t.repeat
            print(f"{t.name:<20} {dur / 1000:7.1f} s  {len(evs):4d} events  {t.text}" + (f"   x {t.repeat}" if t.repeat > 1 else ""))
            if a.dry_run:
                for w, want in setup_words(t):
                    print(f"    bench {w}" + (f"    (name {want})" if want else ""))
                for _, w in evs[:6]:
                    print(f"    bench {w}")
                if len(evs) > 6:
                    print(f"    ... {len(evs) - 6} more")
        print(f"{len(sel)} tests, about {tot / 60:.1f} minutes")
        return 0
    if a.sim:
        link = SimLink(a.sim)
    else:
        link = SerialLink(pick_port(a))
    os.makedirs(a.runs, exist_ok=True)
    stamp = datetime.date.today().isoformat()
    tmp_out = a.out or os.path.join(a.runs, f"{stamp}-run")
    os.makedirs(tmp_out, exist_ok=True)
    log = open(os.path.join(tmp_out, "console.log"), "w")
    caf = None
    if not a.no_caffeinate and not a.sim and shutil_which("caffeinate"):
        caf = subprocess.Popen(["caffeinate", "-i", "-w", str(os.getpid())])
    try:
        return run_all(a, link, log, sname, sel, text, tmp_out)
    finally:
        log.close()
        if caf:
            caf.terminate()
        link.close()


def note_times(test, soak_override=None):
    """the times (s from the test's start) of the test's note-ons and note-offs, for the click analysis' guard windows (None: no notes)"""
    evs, _ = expand(test, soak_override)
    ts = sorted(t_ms / 1000.0 for t_ms, w in evs if len(w.split()) > 2 and w.split()[2] in ("on", "off"))   # (the word is "ev T on CH NOTE VEL")
    return ts or None


def prove_chain(a):
    import bench_record as brec
    if a.sim:
        raise SystemExit("--prove-chain needs the real unit with a tone build (BENCHA1)")
    caf = subprocess.Popen(["caffeinate", "-i", "-w", str(os.getpid())]) if shutil_which("caffeinate") else None
    try:
        ok, text = brec.prove_chain(a.runs, a.minutes, a.rec_device, a.port, a.out)
    except brec.RateError as e:
        raise SystemExit(f"--prove-chain: {e}")
    finally:
        if caf:
            caf.terminate()
    print(text)
    return 0 if ok else 1


def shutil_which(x):
    for d in os.environ.get("PATH", "").split(os.pathsep):
        if os.access(os.path.join(d, x), os.X_OK):
            return os.path.join(d, x)
    return None


def run_all(a, link, log, sname, sel, text, out):
    def reopen():
        if isinstance(link, SimLink):
            link.boots += 1
        link.open()
    recorder = None
    if a.record:                                         # stage 2: refuse before anything is touched if the Mac's DUELL input is not at 44.1 kHz
        import bench_record as brec
        if a.sim:
            recorder = brec.TwinRecorder(link)
        else:
            recorder = brec.SoxRecorder(a.rec_device)
            try:
                recorder.check()
            except brec.RateError as e:
                raise SystemExit(f"--record: {e}")
    for attempt in range(20):                            # the port may take a moment after a plug-in
        try:
            link.open()
            break
        except OSError as e:
            if attempt == 19:
                raise SystemExit(f"cannot open the unit's port: {e}")
            time.sleep(1)
    unit = Unit(link, log, reopen)
    try:
        info = [x for x in unit.lines_of("bench info") if x.startswith("BENCH,INFO")]
    except (LinkDown, OSError):
        info = []
    if not info:
        raise SystemExit("the unit does not answer `bench info`: is a developer bench build (BENCHC1 or newer) installed, and is MASTER down? (nothing else may use the port)")
    f = info[0].split(",")
    version, tag = f[2], f[3].replace("tag ", "")
    cmode, cwhy = parse_cores(info[0])
    cores = cores_line(cmode, cwhy)
    print(cores)
    sm = re.search(r",slack (\d+),calm (\d+),lrun (\d+),vrun (\d+)", info[0])
    if sm:                                                # BENCHT2 and later: the margin settings the build starts with (a test changes them with `tune`, bench reset puts them back)
        wm = re.search(r",vwin (\d+)/(\d+),floor (\d+)", info[0])                  # BENCHT3 and later
        print(f"SETTINGS: slack {sm.group(1)} ticks ({int(sm.group(1)) / 24:.0f} us), calm {sm.group(2)} ms, late run {sm.group(3)}, very late run {sm.group(4)}" +
              (f", window {wm.group(1)} very late blocks in {wm.group(2)} ms, floor {'on' if wm.group(3) == '1' else 'off'}" if wm else ""))
        b4 = wm and (sm.group(4), wm.group(1), wm.group(3)) == ("0", "0", "0")           # BENCHT4 and later start with the run and window rules and the floor off
        if sm.group(1) == "0" or sm.group(2) == "0" or sm.group(3) == "0" or (not b4 and (sm.group(4) == "0" or (wm and (wm.group(1) == "0" or wm.group(3) != "1")))):
            print("WARNING: a setting reads 0 at the start: the build's defaults were not loaded (the values should be 3481 / 10000 / 4 / " + ("very late run 0 / window 0 / floor 0 (BENCHT4) or 4 / window 8 in 2000 / floor 1 (BENCHT3)" if wm else "1") + "). Tell the developer before trusting this run")
    if a.two_core and cmode != "2":
        raise SystemExit(f"--two-core: {cores}. Nothing was run. The Mac cannot restart the unit: unplug it and plug it in again (a BENCHT1 build arms core 1 itself at power-on), then run again")
    st = unit.kv("status")
    base = {"boots": st.get("boots", 0), **{k: st.get(k, 0) for k in ("uac_underruns", "uac_overruns", "uac_missed", "uac_stalls")}}
    base = {"cores": cmode, "boots": base["boots"], "uac_under": base["uac_underruns"], "uac_over": base["uac_overruns"], "uac_missed": base["uac_missed"], "uac_stalls": base["uac_stalls"]}
    if st.get("uac_hz", 44100) != 44100:
        print(f"WARNING: the unit's USB audio runs at {st['uac_hz']} Hz: set the Mac's DUELL input to 44.1 kHz (stage 2 records at 44.1 kHz)")
    build = f"{tag}" if tag else version
    if not a.out:                                          # the folder is named after the build now that it is known
        dst = os.path.join(a.runs, f"{datetime.date.today().isoformat()}-{build}")
        log.close()
        if os.path.abspath(dst) != os.path.abspath(out):
            os.makedirs(dst, exist_ok=True)
            for fn in os.listdir(out):
                os.replace(os.path.join(out, fn), os.path.join(dst, fn))
            try:
                os.rmdir(out)
            except OSError:
                pass
            out = dst
        log = open(os.path.join(out, "console.log"), "a")
        unit.log = log
    with open(os.path.join(out, "suite.txt"), "w") as o:
        o.write(text)
    sha = ""
    sums = os.path.join(a.runs, "builds", tag, "SHA256SUMS")
    if os.path.exists(sums):
        sha = open(sums).read().strip()
    with open(os.path.join(out, "unit.txt"), "w") as o:
        o.write(f"version {version}\ntag {tag}\n{info[0]}\nboots at start {base['boots']}\nuptime_ms at start {st.get('uptime_ms')}\n")
        o.write(f"{cores}\nuac_hz {st.get('uac_hz')}\nstarted {datetime.datetime.now().isoformat(timespec='seconds')}\nbuild SHA-256 {sha or '(not found under bench_runs/builds)'}\n")
    print(f"unit: {version}  boots {base['boots']}  -> {out}")
    if recorder:
        print(a.rc.text())
        with open(os.path.join(out, "unit.txt"), "a") as o:
            o.write(a.rc.text() + "\n")
    results = []
    expected_boots = base["boots"]
    t_all = time.monotonic()
    jobs = [(t, k, t.repeat) for t in sel for k in range(1, t.repeat + 1)]
    gone = False
    for i, (t, rep_no, reps) in enumerate(jobs, 1):
        evs, dur = expand(t, a.soak)
        name = t.name + (f"#{rep_no}" if reps > 1 else "")
        print(f"[{i}/{len(jobs)}] {name:<18} {dur / 1000:6.1f} s ...", end=" ", flush=True)
        try:
            hook = link.crash if isinstance(link, SimLink) and a.sim_crash_in == t.name else None
            r = run_test(unit, t, a.soak, {**base, "boots": expected_boots}, hook, recorder, out, a.rc, t.name + (f"_r{rep_no}" if reps > 1 else ""))
        except (LinkDown, OSError) as e:
            if recorder:
                recorder.abort()
            try:
                rinfo = unit.recover()
                if rinfo["boots"] is not None and rinfo["boots"] > expected_boots:
                    expected_boots = rinfo["boots"]
                    r = Result(test=t.name, text=t.text, status="RESTART", verdict="FAIL",
                               why=("CRASH " + rinfo["crash"]) if rinfo["crash"] else "WATCHDOG or power: the unit restarted (no crash record)")
                else:
                    r = Result(test=t.name, text=t.text, status="LOST", verdict="FAIL", why=f"the link broke ({e}) but the unit did not restart")
            except LinkDown as e2:
                r = Result(test=t.name, text=t.text, status="GONE", verdict="FAIL", why=str(e2))
                gone = True
        except RuntimeError as e:
            if recorder:
                recorder.abort()
            r = Result(test=t.name, text=t.text, status="ERROR", verdict="FAIL", why=str(e))
        r["rep"], r["reps"], r["label"] = rep_no, reps, name
        if recorder and r.get("_rec_info") is not None:
            brec.apply_audio(r, r.pop("_rec_info"), out, events=note_times(t, a.soak))
        results.append(r)
        if gone:
            print("FAIL (the unit is gone)")
            break
        print(f"{r['verdict']}  {r.get('why', '')}")
    try:
        unit.cmd("bench end")
    except (LinkDown, OSError):
        pass
    cols = ["test", "rep", "reps", "text", "verdict", "why", "status", "dur_s", "avg", "peak", "miss", "shed", "lfx", "vox", "halves", "notes1", "notes2", "why1", "why2",
            "budget", "ramcode", "boots", "lost", "flashes", "uac_hz", "uac_under", "uac_over", "uac_missed", "uac_stalls", "erase_max_us", "c1", "c1pk", "c1late", "c1over", "c1holds", *TC_COLS, *TM_COLS, "info", "late_events", "expect"]
    if recorder:
        apply_onecore(results, sel, a.rc.match_s)
        cols += [*brec.AUDIO_COLS, "clk_vs_1c", "gap_vs_1c", "onecore_of", "under_pre", "under_post"]
    with open(os.path.join(out, "results.csv"), "w", newline="") as o:
        w = csv.DictWriter(o, fieldnames=cols, extrasaction="ignore", restval="")
        w.writeheader()
        for r in results:
            w.writerow(r)
    with open(os.path.join(out, "soak_checks.csv"), "w", newline="") as o:
        w = csv.writer(o)
        w.writerow(["test", "second", "ramcode", "boots", "cpu_pct", "audio_late", "voices_shed"])
        for r in results:
            for c in r.get("checks", []):
                w.writerow([lab(r), *c])
    if any(r.get("vlate_ev") for r in results):
        with open(os.path.join(out, "vlate_events.csv"), "w", newline="") as o:
            w = csv.writer(o)
            w.writerow(["test", "ms_since_test_start", "block_in_half", "voices_part1", "voices_part2", "delay_us", "run_us", "margin_us"])
            for r in results:
                for e in r.get("vlate_ev", []):
                    w.writerow([lab(r), *e])
    aggs = aggregate(results, sel)
    if any(x["reps"] > 1 for x in aggs):                   # (BENCHR2) one line per test, the repeats together
        with open(os.path.join(out, "repeats.csv"), "w", newline="") as o:
            w = csv.writer(o)
            w.writerow(["test", "runs", "pass", "verdict", "clicks_per_run", "gaps_per_run", "setup_clicks_per_run", "setup_gaps_per_run", "underruns_after_mark_per_run", "underruns_before_mark_per_run",
                        "edge_excluded_per_run", "note_events", "cpu_avg_mean", "cpu_avg_min", "cpu_avg_max", "cpu_peak_max", "reasons"])
            for x in aggs:
                w.writerow([x["test"], x["reps"], x["passes"], x["verdict"]] + [" ".join("-" if v is None else str(v) for v in x[k]) for k in ("clicks", "gaps", "setup_clicks", "setup_gaps", "under_post", "under_pre", "edge")]
                           + [x["note_events"] or "", *[("" if x[k] is None else f"{x[k]:.1f}") for k in ("cpu_mean", "cpu_min", "cpu_max", "peak_max")], " | ".join(x["reasons"])])
    if recorder and not a.keep_all:                        # (BENCHR2) every repeat of a test that is not clean keeps its audio beyond the newest-5-runs rule; the audio of clean repeats does not
        names = [os.path.basename(r["audio"]) for x in aggs if x["verdict"] in ("FAIL", "WARN") for r in x["rows"] if r.get("audio")]
        brec.write_keep(os.path.join(out, "audio"), names)
    summary = fmt_summary(sname, build, sel, results, cores)
    n_rest = len(unit.restarts)
    summary += f"\nrestarts {n_rest}   run time {(time.monotonic() - t_all) / 60:.1f} min   " + ("(audio recorded: see the AUDIO section)" if recorder else "(audio is not recorded: run with --record)") + "\n"
    if recorder:
        summary += brec.fmt_audio(results)
    with open(os.path.join(out, "summary.txt"), "w") as o:
        o.write(summary)
    print()
    print(summary)
    print(f"-> {out}")
    if recorder and a.keep >= 0 and not a.keep_all:
        for fo, sz in brec.prune(a.runs, a.keep):
            print(f"audio of {fo} deleted ({sz / 1e6:.0f} MB): only the newest {a.keep} runs and the baselines keep theirs")
    return 1 if any(r["verdict"] == "FAIL" for r in results) else 0


if __name__ == "__main__":
    sys.exit(main())
