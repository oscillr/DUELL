#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""DUELL automated bench suite, stage 2: the recorder (the Mac records the unit's USB audio while a test plays).

  python3 tools/bench_record.py --setup-text            the one-time Mac setup (44.1 kHz, microphone permission)
  python3 tools/bench_record.py --check                 is sox there, is the Mac's DUELL input at 44.1 kHz (refuses, never switches)
  python3 tools/bench_record.py --try 3                 record 3 s from the DUELL input, analyze it (checks the permission and the chain)
  python3 tools/bench_record.py --prune [--keep 5]      delete the audio of old runs (the newest N and the baselines keep theirs)
  python3 tools/bench_record.py --baseline RUNFOLDER    mark a run as a baseline (its audio is kept)

tools/bench_run.py --record uses this module: one WAV per test under bench_runs/<date>-<build>/audio/, started before the test's timeline and
stopped after it, then analyzed by tools/bench_analyze.py. Words: **recorder fault** = something went wrong in the Mac's capture (sox said
overrun / dropped / error, it ended early, the file is empty or its length does not fit the clock): the test is marked, the sound is not trusted.
"""
import argparse
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import bench_analyze as an  # noqa: E402

DEVICE = "DUELL"
RATE = 44100
KEEP_DEFAULT = 5

SETUP_TEXT = """\
ONE-TIME SETUP OF THE MAC FOR THE AUDIO RECORDINGS (about two minutes, once)

1. The Mac's DUELL input must run at 44.1 kHz, and stays there.
   - Plug the unit in. Open  Audio MIDI Setup  (Spotlight: Audio MIDI Setup).
   - In the list on the left click  DUELL  (the one with 2 in, 0 out).
   - Under  Input  set  Format  to  2 ch 16-bit 44100.0 Hz  (not 48000 Hz). Close the window.
   - Do not switch it back. A DAW that records from DUELL will then run at 44.1 kHz too (your decision 9).
   The tools only LOOK at this setting. If it is not 44.1 kHz they stop and print these steps; they never change it themselves,
   because a resampled recording (48 kHz) can not be compared sample for sample.

2. The first recording asks for permission to use the microphone.
   - The first time sox records from DUELL, macOS asks "Terminal would like to access the microphone". Click  OK.
   - If you clicked Don't Allow (or the question never appeared): System Settings > Privacy & Security > Microphone > switch Terminal on,
     then close Terminal completely (Cmd-Q) and open it again. (A different terminal program, such as iTerm, needs the same switch.)
   - Without the permission the recording is digital silence: the analysis then says "the recording is digital silence" (WARN).
   - Check it with one command:   python3 tools/bench_record.py --try 3     (while the unit plays something, or the tone build)

3. Tools: sox must be installed (it is: /opt/homebrew/bin/sox and /opt/homebrew/bin/rec). Check: python3 tools/bench_record.py --check

4. During a run: unit plugged in, MASTER down (the USB audio is recorded at its fixed level whatever MASTER says), no DAW using DUELL,
   hands off the unit. The Mac stays awake by itself (caffeinate).
"""


class RateError(Exception):
    pass


# ------------------------------------------------------------------------------------------------ the device
def device_rate(name=DEVICE):
    """the sample rate the Mac has set on the input device `name` (Hz), or None if the device is not there. Reads, never sets."""
    try:
        out = subprocess.run(["system_profiler", "SPAudioDataType", "-json"], capture_output=True, text=True, timeout=30).stdout
        items = json.loads(out)["SPAudioDataType"][0]["_items"]
    except (OSError, subprocess.SubprocessError, ValueError, KeyError, IndexError):
        return None
    return rate_from_items(items, name)


def rate_from_items(items, name=DEVICE):
    for it in items:
        if it.get("_name") == name and it.get("coreaudio_device_input"):
            try:
                return int(it["coreaudio_device_srate"])
            except (KeyError, ValueError):
                return None
    return None


def require_rate(name=DEVICE, rate_fn=None):
    """returns normally only if the DUELL input is at 44.1 kHz; otherwise RateError with the steps (nothing is switched)"""
    r = (rate_fn or device_rate)(name)
    if r is None:
        raise RateError(f'The Mac does not list an input device "{name}": is the unit plugged in and switched on?')
    if r != RATE:
        raise RateError(f'The Mac\'s "{name}" input is set to {r} Hz. The recordings need 44100 Hz, and this tool does not switch it.\n\n' +
                        SETUP_TEXT.split("2. The first")[0])
    return r


def find_sox():
    return shutil.which("sox") or ("/opt/homebrew/bin/sox" if os.access("/opt/homebrew/bin/sox", os.X_OK) else None)


# ------------------------------------------------------------------------------------------------ fault lines
FAULT_RE = re.compile(r"overrun|overflow|underrun|xrun|dropp?ed|lost|glitch|\bWARN\b|\bFAIL\b|error|can't|cannot|unable|failed", re.I)


def fault_lines(text):
    """the lines of sox's messages that mean a recording problem"""
    return [ln.strip() for ln in text.splitlines() if ln.strip() and FAULT_RE.search(ln)]


class RecInfo:
    def __init__(self, path):
        self.path, self.frames, self.rate, self.wall_s, self.faults = path, 0, RATE, 0.0, []
        self.run_s = self.mark_s = self.lead_s = None    # (BENCHR2) where in the file the run command was sent, where the test's mark is, the silent lead-in: seconds from the file's start

    @property
    def seconds(self):
        return self.frames / self.rate if self.rate else 0.0


def wav_frames(path):
    """(frames, rate) of a WAV as it is on disk; (0, RATE) when missing or empty"""
    if not os.path.exists(path):
        return 0, RATE
    try:
        x, rate = an.read_wav(path)
        return x.shape[0], rate
    except an.WavError:
        return 0, RATE


def length_fault(info, tol_abs=0.5, tol_rel=0.02):
    """the recorded length against the clock it ran by: a capture that lost frames (or stalled) is shorter than the time that passed"""
    if info.wall_s <= 0:
        return None
    d = info.seconds - info.wall_s
    if abs(d) > max(tol_abs, tol_rel * info.wall_s):
        return f"the recording is {info.seconds:.2f} s long but ran {info.wall_s:.2f} s ({d:+.2f} s)"
    return None


# ------------------------------------------------------------------------------------------------ recorders
class SoxRecorder:
    """sox reading the CoreAudio device `device`: 44.1 kHz, 16-bit, stereo, no dither (-D), one file per start/stop"""

    def __init__(self, device=DEVICE, sox=None, rate_fn=None):
        self.device, self.sox, self.p, self.info, self.logf = device, sox or find_sox(), None, None, None
        self.rate_fn = rate_fn
        self.t0 = 0.0

    def command(self, path):
        return [self.sox, "-D", "-q", "-V2", "-t", "coreaudio", self.device, "-r", str(RATE), "-b", "16", "-c", "2", "-e", "signed-integer", path]

    def check(self):
        if not self.sox:
            raise RateError("sox is not installed (brew install sox)")
        require_rate(self.device, self.rate_fn or device_rate)

    def path_for(self, outdir, name):
        d = os.path.join(outdir, "audio")
        os.makedirs(d, exist_ok=True)
        return os.path.join(d, re.sub(r"[^A-Za-z0-9_.+-]", "_", name) + ".wav")

    def start(self, path, wait=6.0):
        self.info = RecInfo(path)
        if os.path.exists(path):
            os.remove(path)
        if not getattr(self, "_atexit", False):                 # a Ctrl-C of the runner must not leave sox recording
            import atexit
            atexit.register(self.abort)
            self._atexit = True
        self.logf = path[:-4] + ".rec.log"
        lf = open(self.logf, "w")
        self.p = subprocess.Popen(self.command(path), stdin=subprocess.DEVNULL, stdout=lf, stderr=lf)
        lf.close()
        end = time.monotonic() + wait
        while time.monotonic() < end:                           # the data must be flowing before the test starts
            if self.p.poll() is not None:
                break
            if os.path.exists(path) and os.path.getsize(path) > 44 + 4096:
                self.t0 = time.monotonic()
                return
            time.sleep(0.02)
        self.t0 = time.monotonic()
        # (not flowing: stop() reports it as a fault)

    def position(self):
        """seconds of sound the file holds NOW, counted from its size on disk (44-byte header, 4 bytes a frame). It is what sox has written, so it lags the sound the Mac
        has captured by sox's write buffer (a few 10 ms: UNKNOWN until measured on the unit; the first-sound column onset_lag_s of every test measures it)"""
        try:
            return max(0, os.path.getsize(self.info.path) - 44) / 4.0 / RATE
        except (OSError, AttributeError):
            return max(0.0, time.monotonic() - self.t0)

    def stop(self):
        info = self.info
        if self.p is None or info is None:
            return info
        t1 = time.monotonic()
        died = self.p.poll()
        if died is None:
            self.p.send_signal(signal.SIGINT)                   # sox closes the file properly on SIGINT
            try:
                self.p.wait(timeout=8)
            except subprocess.TimeoutExpired:
                self.p.kill()
                self.p.wait()
                info.faults.append("sox did not stop and was killed")
        else:
            info.faults.append(f"sox ended by itself during the test (exit {died})")
        info.wall_s = t1 - self.t0
        self.p = None
        text = open(self.logf).read() if self.logf and os.path.exists(self.logf) else ""
        info.faults += [f"sox: {ln}" for ln in fault_lines(text)]
        info.frames, info.rate = wav_frames(info.path)
        if info.frames == 0:
            info.faults.append("zero-length recording")
        elif info.rate != RATE:
            info.faults.append(f"the file is {info.rate} Hz")
        else:
            lf = length_fault(info)
            if lf:
                info.faults.append(lf)
        self.info = None
        return info

    def abort(self):
        try:
            return self.stop()
        except Exception:
            return None


class TwinRecorder:
    """the Mac twin (tests/bench_sim.c --serve) writes its own USB audio to a file: '@rec PATH' ... '@recstop'. Time is the twin's."""

    def __init__(self, link):
        self.link, self.info, self.t0 = link, None, 0.0

    def check(self):
        return RATE

    def path_for(self, outdir, name):
        d = os.path.join(outdir, "audio")
        os.makedirs(d, exist_ok=True)
        return os.path.join(d, re.sub(r"[^A-Za-z0-9_.+-]", "_", name) + ".wav")

    def position(self):
        """seconds of sound recorded so far: the twin renders exactly the time the runner lets pass"""
        return max(0.0, self.link.now() - self.t0)

    def start(self, path):
        self.info = RecInfo(path)
        out = self.link._talk("@rec " + os.path.abspath(path))
        if not any(x.startswith("OK") for x in out):
            self.info.faults.append("the twin could not open the file")
        self.t0 = self.link.now()

    def stop(self):
        info = self.info
        if info is None:
            return None
        info.wall_s = self.link.now() - self.t0
        out = self.link._talk("@recstop")
        info.frames, info.rate = wav_frames(info.path)
        if info.frames == 0:
            info.faults.append("zero-length recording")
        self.info = None                  # (the twin's clock moves in the runner's steps, so its length is not compared with it)
        return info

    def abort(self):
        try:
            return self.stop()
        except Exception:
            return None


# ------------------------------------------------------------------------------------------------ into the runner's results
AUDIO_COLS = ["clicks", "gaps", "peak_dbfs", "rms_dbfs", "dc", "centroid_hz", "tail_ms", "clips", "attack_clicks", "abrupt_end", "audio_s", "audio_verdict", "audio_why", "audio_fault", "audio",
              "setup_clicks", "setup_gaps", "note_events", "click_times", "gap_times", "run_s", "mark_s", "lead_s", "onset_lag_s"]
LEVELS = ("PASS", "WARN", "FAIL")


def mark_position(run_s, mark_after):
    """(BENCHR2) the START MARK: where in the recording the test begins, seconds from the file's start. The Mac notes how much sound the recording holds when it sends the
    `bench run` command (run_s) and puts the mark `mark_after` seconds later. Why later and not earlier: the unit does its per-test reset (the effect lines and phases
    are cleared, firmware/src/bench.c bench_reset_phases) in the first half of the run, so the reset's own noise comes AFTER the command, and the timeline's first
    note is held back by a silent lead-in (the runner's --lead). Both are delayed by the same unknown pipeline offset O (the unit's USB buffer, CoreAudio, sox's write
    buffer; 0 to about 0.1 s). The mark must fall after the reset noise (O + a few ms) and before the first note (O + lead): with O at most 0.1 s and the lead 0.6 s the
    middle, 0.35 s, is 0.25 s from both. The accuracy needed is therefore +-0.2 s, which the file size and the clock give."""
    return run_s + mark_after


def split_at(x, mark_s, rate=RATE):
    """(before, after) the mark: the setup and the test"""
    m = max(0, min(int(round(mark_s * rate)), x.shape[0]))
    return x[:m], x[m:]


def fmt_times(ts, n=8):
    return ", ".join(f"{t:.3f}" for t in ts[:n]) + (" .." if len(ts) > n else "")


def apply_audio(res, info, outdir, analysis_kw=None, events=None):
    """analyze the recording of one test and put the columns into its result; the test's verdict is raised to the audio verdict
    (clicks or gaps = FAIL; a recorder fault = WARN: the sound can not be trusted). `info` None: the test was not recorded.
    BENCHR2: when the recording has a start mark (info.mark_s), the part BEFORE the mark is the setup (stream start, scene load, the unit's reset): its clicks and
    gaps go to the columns setup_clicks / setup_gaps and never to the verdict; the part AFTER the mark is the test: it is analyzed alone, its click and gap times
    are seconds after the first note (the first sound after the mark)."""
    if info is None:
        return res
    kw = analysis_kw or {}
    res["audio"] = os.path.relpath(info.path, outdir) if outdir else info.path
    res["audio_fault"] = "; ".join(info.faults)
    mark_s = getattr(info, "mark_s", None)
    split = False
    if mark_s is not None and info.frames:
        x, rate = an.read_wav(info.path)
        split = rate == RATE
    if split:
        pre, post = split_at(x, mark_s, rate)
        rp = an.analyze(pre, rate, **kw)
        r = an.analyze(post, rate, events=events, **kw)
        t0 = r.get("first_sound_s") or 0.0
        r["click_at"] = [round(t - t0, 4) for t in r["click_at"]]
        r["gap_at"] = [(round(g[0] - t0, 4), g[1]) for g in r["gap_at"]]
        r["rel_times"], r["t_first"] = True, t0
        res["setup_clicks"], res["setup_gaps"] = rp["clicks"], rp["gaps"]
        res["_setup"] = rp
        res["mark_s"], res["run_s"] = f"{mark_s:.3f}", f"{info.run_s:.3f}" if info.run_s is not None else ""
        res["lead_s"] = f"{info.lead_s:.2f}" if info.lead_s is not None else ""
        if r.get("first_sound_s") is not None and info.run_s is not None and info.lead_s is not None:
            res["onset_lag_s"] = f"{mark_s + r['first_sound_s'] - info.run_s - info.lead_s:.3f}"    # the pipeline offset O, measured: the first note's sound against the command + lead
        res["note_events"] = len(events) if events else 0
        res["click_times"] = ";".join(f"{t:.3f}" for t in r["click_at"])
        res["gap_times"] = ";".join(f"{g[0]:.3f}" for g in r["gap_at"])
        res["_clicks_t"], res["_gaps_t"] = list(r["click_at"]), [g[0] for g in r["gap_at"]]
    else:
        r = an.analyze_file(info.path, events=events, **kw) if info.frames else an.analyze(an.np.zeros((0, 2), dtype=an.np.int16), **kw)
    v, why = an.audio_verdict(r, res.get("test", ""))
    if split and r.get("first_sound_s") is not None and r["first_sound_s"] < 0.01 and not r["silent"]:
        v = "WARN" if v == "PASS" else v
        why = ("the sound is already on at the mark (the mark may be late or the scene sounds before the test)" + ("; " + why if why else ""))
    cells = an.fmt_cells(r)
    res.update(cells)
    res.update(clips=r["clips"], attack_clicks=r["attack_clicks"], abrupt_end=int(r["abrupt_end"]), audio_s=f"{info.seconds:.2f}")
    if info.faults:
        v = "WARN" if v == "PASS" else v
        why = ("CAPTURE FAULT (not the unit's): " + "; ".join(info.faults) + ("; " + why if why else ""))
    res["audio_verdict"], res["audio_why"] = v, why
    res["verdict_cnt"] = res.get("verdict", "")              # (the counters' verdict before the audio raised it: the one-core comparison of bench_run.apply_onecore starts from it)
    if res.get("verdict") in LEVELS:
        top = max(LEVELS.index(res["verdict"]), LEVELS.index(v))
        if v != "PASS" and LEVELS.index(v) >= LEVELS.index(res["verdict"]):
            res["why"] = (res["why"] + "; " if res.get("why") else "") + "audio: " + why
        res["verdict"] = LEVELS[top]
    res["_analysis"] = r
    return res


def lab(r):
    """a result's name in the tables: NAME, or NAME#k when the test was repeated"""
    return r.get("label") or r["test"]


def fmt_audio(results, kw=None):
    """the AUDIO section of summary.txt: one line per recorded test, then the totals"""
    rec = [r for r in results if r.get("audio")]
    if not rec:
        return ""
    marked = any(r.get("mark_s") not in (None, "") for r in rec)
    lines = ["", "AUDIO (recorded from the unit's USB output, 44.1 kHz 16-bit stereo; clicks or gaps after the mark = FAIL, a capture fault = WARN"
             + ("; setup = before the mark, never a FAIL" if marked else "") + ")", an.HEADER + ("   setup c/g" if marked else "")]
    for r in rec:
        c = {k: r.get(k, "") for k in an.COLS}
        extra = f" (+{r['attack_clicks']} at note edges, not counted)" if r.get("attack_clicks") else ""
        su = f"{r.get('setup_clicks', '')}/{r.get('setup_gaps', '')}" if r.get("mark_s") not in (None, "") else ""
        lines.append(f"{lab(r):<22}{c['clicks']:>7}{c['gaps']:>6}{c['peak_dbfs']:>10}{c['rms_dbfs']:>10}{c['dc']:>8}{c['centroid_hz']:>10}{c['tail_ms']:>9}  "
                     f"{r.get('audio_verdict', ''):<5}" + (f" {r['audio_why']}" if r.get("audio_why") else "") + extra + (f"   setup {su}" if su else ""))
    for r in rec:                                              # where the clicks and gaps are: to compare two recordings of one scene
        an_ = r.get("_analysis") or {}
        rel = bool(an_.get("rel_times"))
        unit_ = "s after the first note" if rel else "s from the file's start"
        if an_.get("click_at") or an_.get("gap_at"):
            ca, ga = an_.get("click_at", []), an_.get("gap_at", [])
            lines.append(f"  {lab(r)}: " + (f"clicks at {', '.join(f'{t:.2f}' for t in ca[:12])}{' ..' if len(ca) > 12 else ''} s" if ca else "") + ("; " if ca and ga else "")
                         + (f"gaps at {', '.join(f'{g[0]:.2f} ({g[1]:g} ms)' for g in ga[:12])}{' ..' if len(ga) > 12 else ''} s" if ga else "") + f"  ({unit_})")
        sp = r.get("_setup") or {}
        if sp.get("click_at") or sp.get("gap_at"):
            lines.append(f"  {lab(r)}: setup (before the mark, from the file's start): " + (f"clicks at {fmt_times(sp['click_at'])} s" if sp.get("click_at") else "")
                         + ("; " if sp.get("click_at") and sp.get("gap_at") else "") + (f"gaps at {', '.join(f'{g[0]:.2f}' for g in sp['gap_at'][:8])} s" if sp.get("gap_at") else ""))
    n_fault = sum(1 for r in rec if r.get("audio_fault"))
    n_bad = sum(1 for r in rec if r.get("audio_verdict") == "FAIL")
    n_ok = sum(1 for r in rec if r.get("audio_verdict") == "PASS")
    lines.append(f"{len(rec)} recorded: {n_ok} clean (no click, no gap{' after the mark' if marked else ''}), {n_bad} FAIL, {n_fault} with a recorder fault"
                 + ("   " + "PROVED: no clicks, no gaps in any recorded test" if n_ok == len(rec) else ""))
    th = rec[0].get("_analysis", {}).get("thresholds") if rec[0].get("_analysis") else None
    if th:
        lines.append("thresholds: " + ", ".join(f"{k}={v}" for k, v in th.items() if k != "chunk"))
    return "\n".join(lines) + "\n"


# ------------------------------------------------------------------------------------------------ disk hygiene
def run_folders(runs):
    """the run folders under bench_runs (newest first): those with a summary.txt or an audio folder"""
    out = []
    for d in os.listdir(runs):
        p = os.path.join(runs, d)
        if d in ("builds", "baselines") or not os.path.isdir(p):
            continue
        if not (os.path.exists(os.path.join(p, "summary.txt")) or os.path.isdir(os.path.join(p, "audio"))):
            continue
        marks = [os.path.getmtime(os.path.join(p, f)) for f in ("summary.txt", "results.csv", "unit.txt") if os.path.exists(os.path.join(p, f))]
        key = max(marks) if marks else os.path.getmtime(p)       # (not the folder's own time: a BASELINE mark or a pruned audio folder would make it look new)
        out.append((key, p))
    return [p for _, p in sorted(out, reverse=True)]


def baselines(runs):
    """run folders that are baselines: a BASELINE file in them, or an entry in bench_runs/baselines/ (a link, or a file holding the path)"""
    keep = set()
    for d in os.listdir(runs):
        p = os.path.join(runs, d)
        if os.path.isfile(os.path.join(p, "BASELINE")):
            keep.add(os.path.realpath(p))
    bd = os.path.join(runs, "baselines")
    if os.path.isdir(bd):
        for e in os.listdir(bd):
            p = os.path.join(bd, e)
            if os.path.islink(p) or os.path.isdir(p):
                keep.add(os.path.realpath(p))
            elif os.path.isfile(p):
                t = open(p).read().strip().splitlines()
                if t:
                    q = t[0].strip()
                    keep.add(os.path.realpath(q if os.path.isabs(q) else os.path.join(runs, q)))
    return keep


KEEP_FILE = "KEEP.txt"


def protected_audio(audio_dir):
    """the WAV names a run asked to keep whatever its age (the repeats of a test that failed): audio/KEEP.txt, one file name a line"""
    p = os.path.join(audio_dir, KEEP_FILE)
    return {ln.strip() for ln in open(p).read().splitlines() if ln.strip()} if os.path.exists(p) else set()


def write_keep(audio_dir, names):
    """(BENCHR2) every repeat of a failing test keeps its audio beyond the newest-5-runs rule: list the files in audio/KEEP.txt"""
    p = os.path.join(audio_dir, KEEP_FILE)
    if names:
        with open(p, "w") as o:
            o.write("\n".join(sorted(set(names))) + "\n")
    elif os.path.exists(p):
        os.remove(p)


def prune(runs, keep=KEEP_DEFAULT, dry=False):
    """delete the audio of every run folder except the newest `keep` and the baselines, and except the files an old run listed in audio/KEEP.txt
    (the repeats of its failing tests). Returns [(folder, bytes freed)]"""
    folders = run_folders(runs)
    safe = {os.path.realpath(p) for p in folders[:max(keep, 0)]} | baselines(runs)
    freed = []
    for p in folders:
        a = os.path.join(p, "audio")
        if os.path.realpath(p) in safe or not os.path.isdir(a):
            continue
        keepf = protected_audio(a)
        gone = [f for f in os.listdir(a) if f.endswith(".wav") and f not in keepf]
        size = sum(os.path.getsize(os.path.join(a, f)) for f in gone)
        if not size:
            continue
        if not dry:
            for f in gone:
                os.remove(os.path.join(a, f))
            with open(os.path.join(a, "PRUNED.txt"), "w") as o:
                o.write(f"The audio of this run was deleted by bench_record.py --prune (only the newest {keep} runs and the baselines keep theirs"
                        + (f"; the files listed in {KEEP_FILE} are the repeats of failing tests and stay" if keepf else "") + ").\n"
                        "To keep a run's audio: python3 tools/bench_record.py --baseline <this folder>\n")
        freed.append((p, size))
    return freed


def mark_baseline(folder):
    with open(os.path.join(folder, "BASELINE"), "w") as o:
        o.write("baseline: its audio is kept by bench_record.py --prune\n")


# ------------------------------------------------------------------------------------------------ the proof of the chain
TONE_TEXT = """\
HOW THE PROOF WORKS. A FELUCCA_UAC_TONE build (BENCHA1) sends, instead of the music, a triangle wave of 100 frames per period
on the USB audio (left and right in opposite directions, steps of 1000). Frame n of the stream is always the one after frame n-1. So the
recording must be that triangle without a single break: a lost frame, a repeated frame, a hole of zeros or a wrong value shows as a break.
Zero breaks over ten minutes means the chain  unit USB -> cable -> CoreAudio -> sox -> file  loses and changes nothing.
What it can not see: a block the unit itself drops before it reaches the stream (the unit's overrun counter, read before and after, covers that)."""


def prove_chain(runs, minutes=10.0, device=DEVICE, port=None, out=None, recorder=None, link=None, out_fn=print):
    """record `minutes` of the tone build's USB audio and check every frame. Returns (ok, text). `recorder`, `link`: for the tests."""
    import datetime
    rec = recorder or SoxRecorder(device)
    rec.check()
    unit, tag, before = None, "", {}
    names = ("boots", "uac_hz", "uac_underruns", "uac_overruns", "uac_missed", "uac_stalls")
    try:                                                  # the console is a help, not a need: its counters make the proof stronger
        import bench_run as br
        if link is None:
            ports = sorted(__import__("glob").glob("/dev/cu.usbmodem*"))
            link = br.SerialLink(port or ports[0]) if (port or len(ports) == 1) else None
        if link is not None:
            link.open()
            unit = br.Unit(link, open(os.devnull, "w"))
            inf = [x for x in unit.lines_of("bench info") if x.startswith("BENCH,INFO")]
            if inf:
                tag = inf[0].split(",")[3].replace("tag ", "")
            st = unit.kv("status")
            before = {k: st.get(k) for k in names}
    except Exception as e:                                # (no console: the recording is still checked)
        out_fn(f"console: not used ({e})")
        unit = None
    if before.get("uac_hz") not in (None, RATE):
        out_fn(f"WARNING: the unit says its USB audio runs at {before['uac_hz']} Hz")
    folder = out or os.path.join(runs, f"{datetime.date.today().isoformat()}-{tag or 'TONE'}")
    os.makedirs(os.path.join(folder, "audio"), exist_ok=True)
    path = rec.path_for(folder, "PROVE-CHAIN")
    out_fn(f"recording {minutes:g} minutes from {device} into {path}" + (f"   (unit build {tag})" if tag else ""))
    rec.start(path)
    t_end = time.monotonic() + minutes * 60.0
    last = 0
    while time.monotonic() < t_end:
        time.sleep(min(2.0, max(0.0, t_end - time.monotonic())))
        if getattr(rec, "p", None) is not None and rec.p.poll() is not None:
            break
        m = int((minutes * 60 - (t_end - time.monotonic())) // 60)
        if m != last:
            last = m
            out_fn(f"  {m} of {minutes:g} minutes")
    info = rec.stop()
    after = {}
    if unit is not None:
        try:
            st = unit.kv("status")
            after = {k: st.get(k) for k in names}
        except Exception as e:
            after = {"error": str(e)}
        finally:
            link.close()
    x, rate = an.read_wav(path) if info.frames else (an.np.zeros((0, 2), dtype=an.np.int16), RATE)
    t = an.tone_check(x, rate)
    notes, ok = [], True
    notes.append(an.tone_line(os.path.basename(path), t))
    if info.faults:
        ok = False
        notes.append("RECORDER FAULTS: " + "; ".join(info.faults))
    if t["breaks"]:
        ok = False
    carried = t["checked"] - t["bad"]
    if carried < 0.98 * max(info.wall_s, 1.0) * RATE:
        ok = False
        notes.append(f"only {carried} frames carried the tone ({carried / RATE:.1f} s of {info.wall_s:.1f} s): is this a tone build, and was the stream running?")
    if before and after and "error" not in after:
        diffs = {k: (after[k] or 0) - (before[k] or 0) for k in ("uac_underruns", "uac_overruns", "uac_missed", "uac_stalls") if before.get(k) is not None and after.get(k) is not None}
        notes.append("unit counters during the recording (underruns, overruns, missed, stalls): " + ", ".join(f"{k[4:]} +{v}" for k, v in diffs.items()))
        if any(diffs.values()):
            ok = False
        if before.get("boots") is not None and after.get("boots") != before.get("boots"):
            ok = False
            notes.append("the unit restarted during the recording")
    elif unit is not None:
        notes.append(f"the unit's counters could not be read afterwards ({after.get('error')})")
        ok = False
    else:
        notes.append("no console: the unit's own counters were not read (a block the unit drops before the stream would not show in the triangle)")
    text = (f"PROVE-CHAIN {'PROVED' if ok else 'NOT PROVED'}: {t['breaks']} break(s) in {t['checked']} frames ({t['checked'] / RATE / 60:.1f} minutes)\n" +
            "\n".join("  " + n for n in notes) + "\n\n" + TONE_TEXT + "\n")
    with open(os.path.join(folder, "prove_chain.txt"), "w") as o:
        o.write(text)
    return ok, text


# ------------------------------------------------------------------------------------------------ command line
def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0], formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--setup-text", action="store_true")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--try", dest="try_s", type=float, help="record this many seconds from the DUELL input and analyze it")
    ap.add_argument("--prune", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--keep", type=int, default=KEEP_DEFAULT, help=f"runs whose audio is kept (default {KEEP_DEFAULT}; baselines always)")
    ap.add_argument("--baseline", metavar="RUNFOLDER")
    ap.add_argument("--runs", default=os.environ.get("DUELL_BENCH_RUNS", "bench_runs"))
    ap.add_argument("--device", default=DEVICE)
    a = ap.parse_args(argv)
    if a.setup_text:
        print(SETUP_TEXT)
        return 0
    if a.baseline:
        mark_baseline(a.baseline)
        print(f"{a.baseline}: marked as a baseline")
        return 0
    if a.prune:
        freed = prune(a.runs, a.keep, a.dry_run)
        for p, s in freed:
            print(f"{'would delete' if a.dry_run else 'deleted'} the audio of {p}: {s / 1e6:.0f} MB")
        print(f"{len(freed)} runs, {sum(s for _, s in freed) / 1e6:.0f} MB")
        return 0
    rec = SoxRecorder(a.device)
    try:
        rec.check()
    except RateError as e:
        print(e, file=sys.stderr)
        return 2
    print(f"sox: {rec.sox}   {a.device} input: {RATE} Hz: ok")
    if a.check or a.try_s is None:
        return 0
    import tempfile
    path = os.path.join(tempfile.mkdtemp(prefix="duell_try_"), "try.wav")
    rec.start(path)
    time.sleep(a.try_s)
    info = rec.stop()
    print(f"recorded {info.frames} frames ({info.seconds:.2f} s) in {path}; recorder faults: {info.faults or 'none'}")
    r = an.analyze_file(path)
    print(an.HEADER)
    print(an.line("try", r))
    if r["silent"]:
        print("The recording is digital silence: either the unit plays nothing, or Terminal has no microphone permission (see --setup-text, step 2).")
    return 1 if info.faults else 0


if __name__ == "__main__":
    sys.exit(main())
