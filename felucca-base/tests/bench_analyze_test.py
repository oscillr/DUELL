#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""Host test of tools/bench_analyze.py and tools/bench_record.py (bench suite stage 2): synthetic signals with planted faults (the detector must find
exactly the planted ones and nothing in the clean ones), the self-check, the tone proof, the recorder's pieces (rate check, fault lines, disk hygiene),
and the whole recorded path against the Mac twin (build/host/bench_sim --serve writes its own USB audio). Needs numpy. Deterministic: fixed seeds."""
import csv
import io
import os
import re
import struct
import sys
import tempfile
import contextlib

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import bench_analyze as an  # noqa: E402
import bench_record as brec  # noqa: E402
import bench_run as br  # noqa: E402

SIM = os.path.join(ROOT, "build", "host", "bench_sim")
bad = 0
R = 44100


def check(name, ok):
    global bad
    print(f"benchaudio: {name:<120} {'ok' if ok else 'FAIL'}")
    bad += not ok


def st(m, right=None):
    """float arrays (-1..1) to int16 frames"""
    r = m if right is None else right
    return np.clip(np.round(np.stack([m, r], 1) * 32768), -32768, 32767).astype(np.int16)


def t_of(sec):
    return np.arange(int(sec * R)) / R


def near(a, b, tol):
    return abs(a - b) <= tol


tt = t_of(3.0)
sine = 0.25 * np.sin(2 * np.pi * 440 * tt)                    # -12 dBFS peak
X = st(sine)

# ------------------------------------------------------------------------------------------------ clean signals: nothing found
r = an.analyze(X)
check("clean sine 440 Hz: 0 clicks, 0 gaps, 0 clipped runs, PASS", r["clicks"] == 0 and r["gaps"] == 0 and r["clips"] == 0 and an.audio_verdict(r)[0] == "PASS")
check("clean sine: peak -12.0 dBFS, RMS -15.0 dBFS (within 0.1), DC below -90 dBFS", near(r["peak_dbfs"], -12.04, 0.1) and near(r["rms_dbfs"], -15.05, 0.1) and r["dc"] < -90)
check("clean sine: spectral centroid 440 Hz within 3 %", near(r["centroid_hz"], 440, 13))
sw = 0.25 * np.sin(2 * np.pi * (40 * tt + (8000 - 40) / (2 * 3.0) * tt ** 2))
r = an.analyze(st(sw))
check("clean linear sweep 40 Hz .. 8 kHz: nothing found", r["clicks"] == 0 and r["gaps"] == 0)
saw = 0.25 * (2 * ((110 * tt) % 1.0) - 1)                      # a naive saw: a jump of half the amplitude every period
sq = 0.2 * np.sign(np.sin(2 * np.pi * 220 * tt))
r = an.analyze(st(saw + sq))
check("the sound's own edges (a naive saw 110 Hz + a naive square 220 Hz, a jump every period) are not clicks", r["clicks"] == 0 and r["gaps"] == 0)
rng = np.random.default_rng(7)
noise = 0.01 * rng.standard_normal(len(tt))
r = an.analyze(st(noise))
check("white noise -40 dBFS rms: nothing found (the seed is fixed)", r["clicks"] == 0 and r["gaps"] == 0)
burst = np.zeros(len(tt))
env = np.exp(-np.arange(R) / 3000.0)
burst[R // 2:R // 2 + R] = 0.5 * rng.standard_normal(R) * env      # a hat: noise starts at full level in one sample
r = an.analyze(st(burst))
check("a noise burst that starts hard (a hi-hat) is not a click, its decay is not a gap", r["clicks"] == 0 and r["gaps"] == 0)

# ------------------------------------------------------------------------------------------------ planted clicks
def plant_spike(x, n, v, ch=0):
    y = x.copy()
    y[n, ch] = np.clip(int(y[n, ch]) + v, -32768, 32767)
    return y


def plant_step(x, n, v, ch=0):
    y = x.astype(np.int32).copy()
    y[n:, ch] += v
    return np.clip(y, -32768, 32767).astype(np.int16)


for label, y, where in (("a one-sample spike +8000", plant_spike(X, 66150, 8000), 66150),
                        ("a one-sample spike -5000 in the right channel only", plant_spike(X, 66150, -5000, 1), 66150),
                        ("a step +3000 (the waveform jumps and goes on from there)", plant_step(X, 66150, 3000), 66150),
                        ("a step -2000 in the right channel", plant_step(X, 66150, -2000, 1), 66150),
                        ("a small step of 1 % of full scale (330 steps)", plant_step(X, 66150, 330), 66150)):
    r = an.analyze(y)
    check(f"sine + {label}: exactly 1 click, 0 gaps, FAIL, at the right place",
          r["clicks"] == 1 and r["gaps"] == 0 and an.audio_verdict(r)[0] == "FAIL" and abs(r["click_at"][0] * R - where) < 4)
y = X.copy()
planted = [20000, 45000, 70000, 95000, 120000]
for i, n in enumerate(planted):
    y = plant_spike(y, n, 4000 + 1500 * i, i % 2)
r = an.analyze(y)
check("sine + 5 spikes (different sizes, both channels): exactly 5 clicks at the 5 places", r["clicks"] == 5 and all(abs(a * R - b) < 4 for a, b in zip(r["click_at"], planted)))
y = X.copy()
y[80000:80008, 0] = np.clip(y[80000:80008, 0].astype(int) + np.array([3000, -3000, 3000, -3000, 3000, -3000, 3000, -3000]), -32768, 32767)
r = an.analyze(y)
check("sine + a burst of 8 alternating samples: one click, not eight", r["clicks"] == 1)
y = plant_spike(st(saw + sq), 70000, 20000)
r = an.analyze(y)
check("a naive saw + square with one spike +20000 planted: exactly 1 click (edges everywhere else)", r["clicks"] == 1 and abs(r["click_at"][0] * R - 70000) < 4)
y = plant_spike(st(noise), 70000, 14000)
r = an.analyze(y)
check("white noise -40 dBFS with one spike +14000 planted: exactly 1 click", r["clicks"] == 1)
mouse = X.copy()
mouse[70000:70020, 0] = 0
mouse[70000:70020, 1] = 0
r = an.analyze(mouse)
check("sine with 20 frames of zeros (0.45 ms): 1 gap, and its two edges are not counted as clicks", r["gaps"] == 1 and r["clicks"] == 0 and an.audio_verdict(r)[0] == "FAIL")

# ------------------------------------------------------------------------------------------------ planted gaps
for label, n, ln in (("64 frames of zeros (1.5 ms)", 60000, 64), ("32 frames (one block)", 60000, 32), ("16 frames (the shortest counted)", 60000, 16), ("4410 frames (100 ms)", 60000, 4410)):
    y = X.copy()
    y[n:n + ln] = 0
    r = an.analyze(y)
    check(f"sine + {label} of digital silence: exactly 1 gap, 0 clicks, FAIL, length found", r["gaps"] == 1 and r["clicks"] == 0 and an.audio_verdict(r)[0] == "FAIL"
          and near(r["gap_at"][0][1], ln * 1000.0 / R, 0.05) and abs(r["gap_at"][0][0] * R - n) < 2)
y = X.copy()
y[60000:60040] = y[59999]
r = an.analyze(y)
check("sine + the last frame repeated for 40 frames (a starved USB stream): 1 gap, 0 clicks", r["gaps"] == 1 and r["clicks"] == 0)
y = X.copy()
y[60000:60008] = 0
r = an.analyze(y)
check("sine + 8 zero frames (below GAP_MIN): no gap, but its edges are 1 click", r["gaps"] == 0 and r["clicks"] == 1)
y = X.copy()
for n in (30000, 70000, 110000):
    y[n:n + 48] = 0
r = an.analyze(y)
check("sine + 3 gaps of 48 frames: exactly 3 gaps", r["gaps"] == 3 and r["clicks"] == 0)
r = an.analyze(y, chunk=4096)
check("the same with 4096-frame chunks: the same result", r["gaps"] == 3 and r["clicks"] == 0)
y = X.copy()
y[4096 * 5 - 20:4096 * 5 + 20] = 0
r = an.analyze(y, chunk=4096)
check("a gap that straddles a chunk edge: found once", r["gaps"] == 1 and r["clicks"] == 0)
y = plant_spike(X, 4096 * 6, 9000)
r = an.analyze(y, chunk=4096)
check("a click exactly on a chunk edge: found once", r["clicks"] == 1)
# natural silences are not gaps
rel = sine.copy()
rel[:] = 0
a0, a1 = 20000, 50000
rel[a0:a1] = 0.25 * np.sin(2 * np.pi * 440 * (tt[a0:a1] - tt[a0])) * np.minimum(1.0, (tt[a0:a1] - tt[a0]) * 200.0) * np.exp(-(tt[a0:a1] - tt[a0]) * 40.0)   # a note that dies away by itself, starting at zero phase
rel[90000:120000] = 0.25 * np.sin(2 * np.pi * 330 * (tt[90000:120000] - tt[90000])) * np.minimum(1.0, (tt[90000:120000] - tt[90000]) * 200.0) * np.exp(-(tt[90000:120000] - tt[90000]) * 40.0)
r = an.analyze(st(rel))
check("two notes that decay by themselves with silence between: no gap, no click, no abrupt end", r["gaps"] == 0 and r["clicks"] == 0 and not r["abrupt_end"])
check("their tail: the sound takes some ms to fall from -20 dB to -70 dBFS (tail_ms > 0)", r["tail_ms"] > 20)
cut = sine.copy()
cut[100000:] = 0
r = an.analyze(st(cut))
check("a sound that stops at full level and then silence: abrupt end found, 0 gaps (nothing sounds after it)", r["abrupt_end"] and r["gaps"] == 0)
quiet = 0.001 * np.sin(2 * np.pi * 50 * tt)                      # -60 dBFS: zero crossings give runs of equal small values
r = an.analyze(st(quiet))
check("a very quiet 50 Hz tone (runs of equal frames near its zero crossings): no gap", r["gaps"] == 0)

# ------------------------------------------------------------------------------------------------ level, DC, clipping, silence
dc = X.astype(np.int32) + 1638
r = an.analyze(np.clip(dc, -32768, 32767).astype(np.int16))
check("sine + DC offset 5 % of full scale: DC -26.0 dBFS (within 0.3), no click, WARN", near(r["dc"], -26.02, 0.3) and r["clicks"] == 0 and an.audio_verdict(r)[0] == "WARN")
r = an.analyze(st(np.clip(6.0 * sine, -1, 1)))
check("sine 6x too loud, clipped: clipped runs found, peak 0 dBFS, the corners are not clicks, WARN", r["clips"] > 100 and r["clicks"] == 0 and r["gaps"] == 0 and r["peak_dbfs"] > -0.01 and an.audio_verdict(r)[0] == "WARN")
Z = np.zeros((R * 2, 2), dtype=np.int16)
r = an.analyze(Z)
check("digital silence: RMS and peak -120, no click, no gap; PASS for a test called SILENCE, WARN for any other", r["rms_dbfs"] == -120 and r["peak_dbfs"] == -120 and r["clicks"] == 0 and r["gaps"] == 0
      and an.audio_verdict(r, "SILENCE")[0] == "PASS" and an.audio_verdict(r, "M07-CHORDS")[0] == "WARN")
r = an.analyze(np.zeros((0, 2), dtype=np.int16))
check("an empty recording does not crash and is silent", r["silent"] and r["seconds"] == 0)
r1, r2 = an.analyze(X), an.analyze(X)
check("deterministic: the same signal analyzed twice gives the same numbers", {k: v for k, v in r1.items()} == {k: v for k, v in r2.items()})
r3 = an.analyze(plant_spike(X, 66150, 8000), chunk=8192)
r4 = an.analyze(plant_spike(X, 66150, 8000))
check("deterministic: any chunk size gives the same result", {k: v for k, v in r3.items() if k != "thresholds"} == {k: v for k, v in r4.items() if k != "thresholds"})
r = an.analyze(st(np.sin(2 * np.pi * 1000 * tt) * 0.25))
check("centroid of a 1 kHz sine: 1000 Hz within 3 %", near(r["centroid_hz"], 1000, 30))
check("every threshold is settable and shown (click_k, click_floor, gap_min, gap_level ...)", all(k in an.THRESHOLDS for k in ("click_k", "click_floor", "gap_min", "gap_level", "dc_warn", "clip_warn")))
r = an.analyze(plant_step(X, 66150, 330), click_floor=500.0)
check("a higher click_floor (500) lets that 1 % step pass: the thresholds act", r["clicks"] == 0)
r = an.analyze(X.copy().astype(np.int16), gap_min=4)
check("a lower gap_min (4) on a clean sine: still nothing", r["gaps"] == 0)

# ------------------------------------------------------------------------------------------------ note events
y = plant_spike(X, 66150, 8000)
r = an.analyze(y, events=[1.4, 2.5])
check("a click 0.1 s after the note event at 1.4 s... is outside the guard (60 ms): still a click", r["clicks"] == 1 and r["attack_clicks"] == 0)
r = an.analyze(y, events=[0.0, 1.495])
check("a click 5 ms after a note event is the note's attack: attack_clicks 1, clicks 0", r["clicks"] == 0 and r["attack_clicks"] == 1)

# ------------------------------------------------------------------------------------------------ the self-check
ra, rb = an.analyze(X), an.analyze(X)
ok, rows = an.selfcheck(ra, rb)
check("selfcheck: the same recording twice agrees", ok)
rb = an.analyze(plant_spike(X, 66150, 8000))
ok, rows = an.selfcheck(ra, rb)
check("selfcheck: one has a click and the other not: DIFFERENT", not ok and any("clicks" in x and "DIFFERENT" in x for x in rows))
rb = an.analyze(st(sine * 10 ** (0.2 / 20)))
ok, _ = an.selfcheck(ra, rb)
check("selfcheck: 0.2 dB louder is within the tolerance (0.5 dB)", ok)
rb = an.analyze(st(sine * 10 ** (3.0 / 20)))
ok, _ = an.selfcheck(ra, rb)
check("selfcheck: 3 dB louder is DIFFERENT", not ok)
rb = an.analyze(X[:-R])
ok, _ = an.selfcheck(ra, rb)
check("selfcheck: one second shorter is DIFFERENT (length tolerance 0.1 s)", not ok)

# ------------------------------------------------------------------------------------------------ files and the command line
tmp = tempfile.mkdtemp(prefix="benchaudio_")
fa, fb, fc = (os.path.join(tmp, n) for n in ("a.wav", "b.wav", "c.wav"))
an.write_wav(fa, X)
an.write_wav(fb, plant_spike(X, 66150, 8000))
an.write_wav(fc, X)
x2, rate = an.read_wav(fa)
check("WAV round trip: 44100 Hz, the same frames", rate == R and np.array_equal(np.asarray(x2), X))
raw = open(fa, "rb").read()
open(os.path.join(tmp, "dead.wav"), "wb").write(raw[:4] + struct.pack("<I", 0xFFFFFFFF) + raw[8:40] + struct.pack("<I", 0xFFFFFFFF) + raw[44:-6])
x3, _ = an.read_wav(os.path.join(tmp, "dead.wav"))
check("a WAV whose size fields are wrong (a recorder that was killed) and ends mid-frame still opens: whole frames only", x3.shape[0] == X.shape[0] - 2 and np.array_equal(np.asarray(x3), X[:-2]))
an.write_wav(os.path.join(tmp, "48k.wav"), X, 48000)
r = an.analyze_file(os.path.join(tmp, "48k.wav"))
check("a 48 kHz file is refused as FAIL (the recordings must be 44.1 kHz)", an.audio_verdict(r)[0] == "FAIL")


def cli(*args):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        code = an.main(list(args))
    return code, buf.getvalue()


code, out = cli(fa)
check("CLI: a clean file: exit 0, one line with the seven columns", code == 0 and "PASS" in out and out.count("\n") >= 3)
code, out = cli(fa, fb)
check("CLI: a file with a click: exit 1, FAIL in its line", code == 1 and "FAIL" in out and "1 clicks" in out)
code, out = cli("--csv", fb)
rows = list(csv.reader(io.StringIO(out)))
check("CLI --csv: the header has file, clicks, gaps, peak_dbfs, rms_dbfs, dc, centroid_hz, tail_ms, verdict", rows[0][:9] == ["file", "clicks", "gaps", "peak_dbfs", "rms_dbfs", "dc", "centroid_hz", "tail_ms", "verdict"] and rows[1][1] == "1")
code, out = cli("--selfcheck", fa, fc)
check("CLI --selfcheck: the same twice: exit 0, SELFCHECK PASS", code == 0 and "SELFCHECK PASS" in out)
code, out = cli("--selfcheck", fa, fb)
check("CLI --selfcheck: with and without a click: exit 1", code == 1 and "SELFCHECK FAIL" in out)
code, out = cli("--click-floor", "5000", fb)
check("CLI: --click-floor 5000 is taken (the 8000 spike still found, the thresholds line shows it)", "click_floor=5000.0" in out)

# ------------------------------------------------------------------------------------------------ the tone proof
def tone(n, start_ph=0, lead=0):
    ph = (np.arange(n) + start_ph) % 100
    q = np.where(ph < 50, ph, 100 - ph)
    q2 = np.abs(50 - ph)
    fr = np.stack([q * 1000 - 25000, q2 * 1000 - 25000], 1).astype(np.int16)
    return np.concatenate((np.zeros((lead, 2), dtype=np.int16), fr))


T = tone(44100 * 2, 7, 333)
t = an.tone_check(T)
check("tone: a clean triangle after 333 frames of silence, starting at phase 7: 0 breaks, all frames checked", t["breaks"] == 0 and t["start"] == 333 and t["checked"] == 44100 * 2)
t = an.tone_check(np.concatenate((T[:30000], T[30001:])))
check("tone: one frame lost: 1 break at the right frame", t["breaks"] == 1 and abs(t["first_break"] - 30000) <= 1)
t = an.tone_check(np.concatenate((T[:30000], T[29999:30000], T[30000:])))
check("tone: one frame repeated: 1 break", t["breaks"] == 1 and abs(t["first_break"] - 30000) <= 2)
t = an.tone_check(np.concatenate((T[:30000], np.zeros((44, 2), dtype=np.int16), T[30000:])))
check("tone: a hole of 44 zero frames inserted: 1 break", t["breaks"] == 1)
t = an.tone_check(np.concatenate((T[:30000], T[30044:])))
check("tone: a lost packet (44 frames): 1 break", t["breaks"] == 1)
U = T.copy()
U[20000, 0] += 1
t = an.tone_check(U)
check("tone: one sample off by 1: 1 break", t["breaks"] == 1)
V = np.concatenate((T[:20000], T[20001:50000], T[50001:]))
t = an.tone_check(V)
check("tone: two separate lost frames: 2 breaks", t["breaks"] == 2)
t = an.tone_check(np.concatenate((T[:30000], T[30001:])), chunk=4096)
check("tone: the same with 4096-frame chunks: the same 1 break", t["breaks"] == 1)
t = an.tone_check(np.concatenate((T[:4096 * 3 - 1], T[4096 * 3:])), chunk=4096)
check("tone: a loss exactly at a chunk edge: 1 break (not 0, not 2)", t["breaks"] == 1)
t = an.tone_check(X)
check("tone: a sine instead of the triangle: nearly every frame is off the triangle (it is not the tone)", t["bad"] > 0.9 * t["checked"])
t = an.tone_check(Z)
check("tone: silence: nothing checked", t["checked"] == 0)
sys.path.insert(0, os.path.join(ROOT, "tools"))
import importlib.util  # noqa: E402
spec = importlib.util.spec_from_file_location("uac_tone_check", os.path.join(ROOT, "tools", "uac_tone_check.py"))
old = importlib.util.module_from_spec(spec)
spec.loader.exec_module(old)
small = np.concatenate((tone(3000, 3, 50)[:1500], tone(3000, 3, 50)[1501:2500], tone(3000, 3, 50)[2530:]))
an.write_wav(os.path.join(tmp, "tone_small.wav"), small)
with contextlib.redirect_stdout(io.StringIO()):
    old_ok = old.check(os.path.join(tmp, "tone_small.wav"))
check("tone: the same verdict as the older tools/uac_tone_check.py on a recording with two losses", (an.tone_check(small)["breaks"] == 2) and not old_ok)

# ------------------------------------------------------------------------------------------------ the recorder's pieces
items = [{"_name": "DUELL", "coreaudio_device_input": 2, "coreaudio_device_srate": 48000}, {"_name": "MacBook Pro Speakers", "coreaudio_device_output": 2, "coreaudio_device_srate": 48000}]
check("rate_from_items: the DUELL input at 48000 is read as 48000; a missing device as None", brec.rate_from_items(items) == 48000 and brec.rate_from_items(items[1:]) is None)
try:
    brec.require_rate("DUELL", lambda n: 48000)
    refused = False
    msg = ""
except brec.RateError as e:
    refused, msg = True, str(e)
check("require_rate: 48 kHz is REFUSED (nothing is switched) and the message holds the Audio MIDI Setup steps", refused and "48000" in msg and "Audio MIDI Setup" in msg and "44100" in msg and "never change" in msg)
try:
    brec.require_rate("DUELL", lambda n: None)
    nodev = False
except brec.RateError as e:
    nodev = "plugged in" in str(e)
check("require_rate: no DUELL device: refused with a plain message", nodev)
check("require_rate: 44.1 kHz passes", brec.require_rate("DUELL", lambda n: 44100) == 44100)
check("the setup text names 44.1 kHz, Audio MIDI Setup, the microphone permission for Terminal, and the check command",
      all(w in brec.SETUP_TEXT for w in ("44100.0 Hz", "Audio MIDI Setup", "microphone", "Terminal", "bench_record.py --try")))
c = brec.SoxRecorder("DUELL", sox="/x/sox").command("/tmp/a.wav")
check("the sox command: no dither (-D), CoreAudio device DUELL, 44100 Hz, 16 bit, 2 channels, signed integer, the file", "-D" in c and c[c.index("-t") + 1] == "coreaudio" and "DUELL" in c and c[c.index("-r") + 1] == "44100"
      and c[c.index("-b") + 1] == "16" and c[c.index("-c") + 1] == "2" and c[-1] == "/tmp/a.wav")
check("fault_lines: overrun / dropped / WARN / error lines count, a plain info line does not",
      len(brec.fault_lines("sox WARN coreaudio: input overrun\nsox INFO foo\nDropped 32 frames\nfine\nsox FAIL formats: can't open")) == 3)
i = brec.RecInfo("x")
i.frames, i.wall_s = int(R * 9.0), 10.0
check("length_fault: 9.0 s recorded in 10.0 s of the clock is a fault, 9.8 s is not", brec.length_fault(i) is not None and (setattr(i, "frames", int(R * 9.8)) or brec.length_fault(i) is None))

# disk hygiene
runs = os.path.join(tmp, "runs")
for k, name in enumerate(["2026-10-01-A", "2026-10-02-B", "2026-10-03-C", "2026-10-04-D", "2026-10-05-E", "2026-10-06-F", "2026-10-07-G"]):
    d = os.path.join(runs, name, "audio")
    os.makedirs(d)
    open(os.path.join(runs, name, "summary.txt"), "w").write("x")
    os.utime(os.path.join(runs, name, "summary.txt"), (1.7e9 + k * 1000, 1.7e9 + k * 1000))
    os.utime(os.path.join(runs, name), (1.7e9 + k * 1000, 1.7e9 + k * 1000))
    open(os.path.join(d, "T.wav"), "wb").write(b"0" * 1000)
os.makedirs(os.path.join(runs, "builds", "X", "audio"))
open(os.path.join(runs, "builds", "X", "audio", "keep.wav"), "wb").write(b"1")
brec.mark_baseline(os.path.join(runs, "2026-10-01-A"))
os.makedirs(os.path.join(runs, "baselines"))
open(os.path.join(runs, "baselines", "tonebase"), "w").write("2026-10-03-C\n")
would = brec.prune(runs, 2, dry=True)
check("prune --dry-run: lists B, D and E (A is a BASELINE, C is in baselines/, F and G are the newest two) and deletes nothing",
      sorted(os.path.basename(p) for p, _ in would) == ["2026-10-02-B", "2026-10-04-D", "2026-10-05-E"] and
      all(os.path.exists(os.path.join(runs, n, "audio", "T.wav")) for n in os.listdir(runs) if n[:2] == "20"))
freed = brec.prune(runs, 2)
gone = sorted(n for n in os.listdir(runs) if n[:2] == "20" and not os.path.exists(os.path.join(runs, n, "audio", "T.wav")))
check("prune: exactly B, D and E lost their audio; A (BASELINE), C (baselines/), F and G (the newest two) kept it; builds/ untouched",
      gone == ["2026-10-02-B", "2026-10-04-D", "2026-10-05-E"] and os.path.exists(os.path.join(runs, "builds", "X", "audio", "keep.wav")) and
      os.path.exists(os.path.join(runs, "2026-10-02-B", "audio", "PRUNED.txt")) and len(freed) == 3)
check("prune: a second pass finds nothing more", brec.prune(runs, 2) == [])

# apply_audio: a recorder fault is a WARN, clicks are a FAIL
an.write_wav(os.path.join(tmp, "ok.wav"), X)
inf = brec.RecInfo(os.path.join(tmp, "ok.wav"))
inf.frames = X.shape[0]
res = {"test": "T", "verdict": "PASS", "why": ""}
brec.apply_audio(res, inf, tmp)
check("apply_audio: a clean recording leaves a PASS test PASS and fills the seven columns", res["verdict"] == "PASS" and res["clicks"] == "0" and res["gaps"] == "0" and res["peak_dbfs"] == "-12.0" and res["audio"] == "ok.wav")
inf.faults = ["sox: overrun"]
res = {"test": "T", "verdict": "PASS", "why": ""}
brec.apply_audio(res, inf, tmp)
check("apply_audio: a recorder fault makes the test WARN and says the capture, not the unit, is at fault", res["verdict"] == "WARN" and "CAPTURE FAULT" in res["why"] and res["audio_fault"])
inf2 = brec.RecInfo(fb)
inf2.frames = X.shape[0]
res = {"test": "T", "verdict": "WARN", "why": "2 very late blocks"}
brec.apply_audio(res, inf2, tmp)
check("apply_audio: a click makes the test FAIL and keeps the unit's own reason beside it", res["verdict"] == "FAIL" and "very late" in res["why"] and "1 clicks" in res["why"])
inf3 = brec.RecInfo(os.path.join(tmp, "nothing.wav"))
inf3.faults = ["zero-length recording"]
res = {"test": "T", "verdict": "PASS", "why": ""}
brec.apply_audio(res, inf3, tmp)
check("apply_audio: no file at all (zero-length): marked, no crash", res["verdict"] == "WARN" and "zero-length" in res["audio_fault"])
txt = brec.fmt_audio([dict(res), {"test": "U", "audio": "u.wav", "clicks": "0", "gaps": "0", "peak_dbfs": "-12.0", "rms_dbfs": "-15.0", "dc": "-120.0", "centroid_hz": "440", "tail_ms": "0", "audio_verdict": "PASS", "audio_why": "", "audio_fault": ""}])
check("fmt_audio: the AUDIO section lists the tests and counts the clean ones and the capture faults", "AUDIO" in txt and "1 with a recorder fault" in txt)


# the proof of the chain, with a fake recorder that writes the tone
class FakeRec:
    def __init__(self, data, faults=None):
        self.data, self.faults, self.p, self.t0 = data, faults or [], None, 0

    def check(self):
        return 44100

    def path_for(self, outdir, name):
        os.makedirs(os.path.join(outdir, "audio"), exist_ok=True)
        return os.path.join(outdir, "audio", name + ".wav")

    def start(self, path):
        self.path = path
        self.t0 = __import__("time").monotonic()

    def stop(self):
        an.write_wav(self.path, self.data)
        i = brec.RecInfo(self.path)
        i.frames, i.faults = self.data.shape[0], list(self.faults)
        i.wall_s = self.data.shape[0] / R
        return i


good = tone(R * 3, 11, 2000)
ok_, text = brec.prove_chain(os.path.join(tmp, "pr"), 0.05, recorder=FakeRec(good), link=False, out_fn=lambda s: None)
check("prove_chain: a clean 3 s triangle (0.05 minutes asked): PROVED, 0 breaks", ok_ and "PROVED" in text and "0 break" in text)
ok_, text = brec.prove_chain(os.path.join(tmp, "pr"), 0.05, recorder=FakeRec(np.concatenate((good[:100000], good[100001:]))), link=False, out_fn=lambda s: None)
check("prove_chain: one frame missing: NOT PROVED, 1 break", (not ok_) and "NOT PROVED" in text and "1 break" in text)
ok_, text = brec.prove_chain(os.path.join(tmp, "pr"), 0.05, recorder=FakeRec(good, ["sox: overrun"]), link=False, out_fn=lambda s: None)
check("prove_chain: the recorder reports a fault: NOT PROVED even if the frames are right", (not ok_) and "RECORDER FAULTS" in text)
ok_, text = brec.prove_chain(os.path.join(tmp, "pr"), 0.05, recorder=FakeRec(X[:R * 3]), link=False, out_fn=lambda s: None)
check("prove_chain: the unit plays music, not the tone (a normal build): NOT PROVED, says so", (not ok_) and "tone build" in text)

# ------------------------------------------------------------------------------------------------ the whole recorded path against the twin
if not os.path.exists(SIM):
    print("benchaudio: build/host/bench_sim not built: the recorded path is not run against the twin")
else:
    out = os.path.join(tmp, "twin")
    runsd = os.path.join(tmp, "twinruns")
    suite = os.path.join(tmp, "mini.txt")
    open(suite, "w").write("""suite MINI
test SILENCE  5 s of nothing
tail 2
test M07-CHORDS  M07 FINAL REEL, four chords
load multi 7 "FINAL REEL"
at 0.0 chord C3 E3 G3 B3 vel 90
at 3.0 chord A2 C3 E3 G3 vel 90
at 6.0 release
tail 2
""")
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        try:
            code = br.main(["--sim", SIM, "--record", "--suite", suite, "--out", out, "--runs", runsd, "--keep", "5"])
        except SystemExit as e:
            code = e.code
    o = buf.getvalue()
    rows = list(csv.DictReader(open(os.path.join(out, "results.csv")))) if os.path.exists(os.path.join(out, "results.csv")) else []
    check("twin, --record: a WAV per test under <run>/audio/, the audio columns in results.csv", len(rows) == 2 and all(os.path.exists(os.path.join(out, r["audio"])) for r in rows)
          and all(k in rows[0] for k in ("clicks", "gaps", "peak_dbfs", "rms_dbfs", "dc", "centroid_hz", "tail_ms")))
    summ = open(os.path.join(out, "summary.txt")).read()
    check("twin, --record: the summary has the AUDIO section, the seven columns, and no recorder fault", "AUDIO" in summ and "centroid" in summ and all(not r["audio_fault"] for r in rows))
    chords = [r for r in rows if r["test"] == "M07-CHORDS"][0]
    sil = [r for r in rows if r["test"] == "SILENCE"][0]
    check("twin: SILENCE records digital silence (-120 dBFS), M07-CHORDS sounds (peak above -20 dBFS) and has 0 clicks, 0 gaps",
          sil["rms_dbfs"] == "-120.0" and float(chords["peak_dbfs"]) > -20 and chords["clicks"] == "0" and chords["gaps"] == "0")
    wav = os.path.join(out, chords["audio"])
    xs, rate = an.read_wav(wav)
    xs = np.array(xs)
    n0 = np.flatnonzero(np.any(xs != 0, axis=1))[0]
    check("twin: the recording is the setup (1.0 s stream settle + 0.3 + 1.5 s setup wait) + the 0.6 s lead-in + the test (6 s of notes + 2 s tail): 11..13 s, silent until the first note at about 3.4 s",
          11 * R < xs.shape[0] < 13 * R and 3.2 * R < n0 < 3.7 * R)
    # planted faults into the twin's own sound: found, exactly
    pos = int(n0 + 4.5 * R)
    y = plant_spike(xs, pos, 6000)
    r = an.analyze(y)
    base = an.analyze(xs)
    check("twin render + one planted spike +6000 mid-chord: exactly one click more than the clean render", r["clicks"] == base["clicks"] + 1 and r["gaps"] == base["gaps"])
    y = xs.copy()
    y[pos:pos + 64] = 0
    r = an.analyze(y)
    check("twin render + 64 zero frames mid-chord: exactly one gap more, no new click", r["gaps"] == base["gaps"] + 1 and r["clicks"] == base["clicks"])
    y = xs.copy()
    y[pos:pos + 40] = y[pos - 1]
    r = an.analyze(y)
    check("twin render + the last frame repeated 40 times mid-chord: exactly one gap more", r["gaps"] == base["gaps"] + 1)
    y = plant_step(xs, pos, 4000)
    r = an.analyze(y)
    check("twin render + a step of 4000 (12 % of full scale, a third of this chord's peak) mid-chord: found as one click", r["clicks"] == base["clicks"] + 1)
    # the same test recorded twice gives the same numbers (the twin is exact)
    out2 = os.path.join(tmp, "twin2")
    with contextlib.redirect_stdout(io.StringIO()):
        br.main(["--sim", SIM, "--record", "--suite", suite, "--out", out2, "--runs", runsd, "--keep", "5"])
    w2 = os.path.join(out2, "audio", "M07-CHORDS.wav")
    ra, rb = an.analyze_file(wav), an.analyze_file(w2)
    ok_, rws = an.selfcheck(ra, rb)
    check("twin: the same test recorded twice: selfcheck PASS, and the samples are bit for bit equal", ok_ and np.array_equal(np.asarray(an.read_wav(wav)[0]), np.asarray(an.read_wav(w2)[0])))
    # --record refuses when the Mac's DUELL input is not at 44.1 kHz
    saved = brec.device_rate
    brec.device_rate = lambda name="DUELL": 48000
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            br.main(["--record", "--suite", suite, "--out", os.path.join(tmp, "refused"), "--runs", runsd, "--only", "SILENCE"])
        refused = False
        why = ""
    except SystemExit as e:
        refused, why = True, str(e.code)
    finally:
        brec.device_rate = saved
    check("--record on the unit with the Mac's DUELL input at 48 kHz: REFUSED before anything runs, with the setup steps; nothing created",
          refused and "44100" in why and "Audio MIDI Setup" in why and not os.path.exists(os.path.join(tmp, "refused", "audio")))
    # a run folder prune through the runner's --keep: the old audio goes
    old_run = os.path.join(runsd, "2026-01-01-OLD")
    os.makedirs(os.path.join(old_run, "audio"))
    open(os.path.join(old_run, "summary.txt"), "w").write("x")
    os.utime(os.path.join(old_run, "summary.txt"), (1.6e9, 1.6e9))
    os.utime(old_run, (1.6e9, 1.6e9))
    open(os.path.join(old_run, "audio", "T.wav"), "wb").write(b"0" * 100)
    # (the runs of this test were given --out, so they are not under runsd: the check is on runsd's own old folder)
    with contextlib.redirect_stdout(io.StringIO()):
        br.main(["--sim", SIM, "--record", "--suite", suite, "--runs", runsd, "--keep", "0", "--only", "SILENCE"])
    check("runner --keep 0: the audio of the old run folder is deleted after the run", not os.path.exists(os.path.join(old_run, "audio", "T.wav")))

# ------------------------------------------------------------------------------------------------ the join (BENCHR1): RECORD-1, the one-core reference, the audio columns beside the core 1 counters
nmr, tr = br.parse_suite(open(os.path.join(ROOT, "tools", "bench_suites", "RECORD-1.txt")).read())
totr = sum(br.expand(t)[1] / 1000.0 + t.settle + 0.6 for t in tr)
wr = {t.name: [w for w, _ in br.setup_words(t)] for t in tr}
check("RECORD-1: 4 tests (each scene's one-core reference first, then the BENCHT4 defaults), the references say onecore-of, about 12 minutes (%.1f)" % (totr / 60),
      nmr == "RECORD-1" and [t.name for t in tr] == ["STRESS-1CORE", "STRESS-DEF", "M07-1CORE", "M07-DEF"] and [t.onecore_of for t in tr] == ["STRESS-DEF", None, "M07-DEF", None]
      and 11 * 60 < totr < 13 * 60)
check("RECORD-1: the references set 'slack 400000' (the largest slack: no job is ever posted), the defaults set nothing", "slack 400000" in wr["STRESS-1CORE"] and "slack 400000" in wr["M07-1CORE"]
      and not [w for w in wr["STRESS-DEF"] + wr["M07-DEF"] if w.split()[0] in ("slack", "calm", "pause", "lrun", "vrun", "vwin", "floor")])


def fake(test, clicks, gaps, vcnt="PASS", posted=0, audio_v=None, fault=""):
    """a result as run_all has it after apply_audio (the counters' verdict vcnt, then raised by the audio)"""
    v = audio_v or ("FAIL" if clicks or gaps else "PASS")
    top = brec.LEVELS[max(brec.LEVELS.index(vcnt), brec.LEVELS.index(v))]
    return {"test": test, "audio": "audio/%s.wav" % test, "clicks": str(clicks), "gaps": str(gaps), "audio_verdict": v, "audio_why": "%d clicks" % clicks if clicks else "",
            "verdict": top, "verdict_cnt": vcnt, "why": "", "audio_fault": fault, "tc": "2", "tcblocks": "100000", "tmposted": str(posted)}


tt = [br.Test("S-1C", "", ), br.Test("S-DEF", "")]
tt[0].onecore_of = "S-DEF"
rs = br.apply_onecore([fake("S-1C", 5, 0), fake("S-DEF", 5, 0)], tt)
check("one-core comparison: the same 5 clicks on both: the reference is WARN (the scene's own), the two-core test WARN with 'none more than on one core', excess 0/0",
      rs[0]["verdict"] == "WARN" and "ONE-CORE REFERENCE" in rs[0]["audio_why"] and rs[1]["verdict"] == "WARN" and "none more than on one core" in rs[1]["audio_why"]
      and (rs[1]["clk_vs_1c"], rs[1]["gap_vs_1c"]) == ("0", "0") and br.audio_cell(rs[1]).endswith("(=1c)"))
rs = br.apply_onecore([fake("S-1C", 5, 0), fake("S-DEF", 8, 2)], tt)
check("one-core comparison: 8 clicks + 2 gaps against 5 + 0 on one core: the two-core test FAILs with '3 clicks and 2 gaps more', cells '8/2 (+3/+2)'",
      rs[1]["verdict"] == "FAIL" and "3 clicks and 2 gaps more than on one core" in rs[1]["audio_why"] and br.audio_cell(rs[1]) == "8/2 (+3/+2)" and rs[0]["verdict"] == "WARN")
rs = br.apply_onecore([fake("S-1C", 0, 0), fake("S-DEF", 2, 0)], tt)
check("one-core comparison: a clean reference, 2 clicks on two cores: FAIL, +2 (a clean scene stays strict)", rs[0]["verdict"] == "PASS" and rs[1]["verdict"] == "FAIL" and rs[1]["clk_vs_1c"] == "2")
rs = br.apply_onecore([fake("S-1C", 5, 0), fake("S-DEF", 3, 0, vcnt="WARN")], tt)
check("one-core comparison: fewer clicks than the reference: WARN; the counters' own WARN is kept", rs[1]["verdict"] == "WARN" and rs[1]["clk_vs_1c"] == "0")
rs = br.apply_onecore([fake("S-1C", 5, 0, vcnt="FAIL"), fake("S-DEF", 0, 0)], tt)
check("one-core comparison: a reference whose counters FAIL stays FAIL (only the clicks and gaps are excused)", rs[0]["verdict"] == "FAIL")
rs = br.apply_onecore([fake("S-1C", 0, 0, posted=90000), fake("S-DEF", 4, 0)], tt)
check("one-core comparison: core 1 was given jobs in the reference: 'NOT A ONE-CORE RUN', WARN, no comparison (the two-core test keeps its own FAIL)",
      rs[0]["verdict"] == "WARN" and "NOT A ONE-CORE RUN" in rs[0]["audio_why"] and "clk_vs_1c" not in rs[1] and rs[1]["verdict"] == "FAIL")
rs = br.apply_onecore([fake("S-1C", 0, 0, audio_v="WARN", fault="sox overrun"), fake("S-DEF", 4, 0)], tt)
check("one-core comparison: a capture fault in the reference: no comparison, nothing excused", "clk_vs_1c" not in rs[1] and rs[1]["verdict"] == "FAIL")

if os.path.exists(SIM):
    outr = os.path.join(tmp, "rec1")
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        try:
            codr = br.main(["--sim", SIM, "--two-core", "--record", "--suite", "RECORD-1", "--only", "STRESS-1CORE,STRESS-DEF", "--out", outr, "--runs", os.path.join(tmp, "rec1runs"), "--keep", "5"])
        except SystemExit as e:
            codr = e.code
    summr = open(os.path.join(outr, "summary.txt")).read() if os.path.exists(os.path.join(outr, "summary.txt")) else ""
    rowsr = list(csv.DictReader(open(os.path.join(outr, "results.csv")))) if os.path.exists(os.path.join(outr, "results.csv")) else []
    hdr = [ln for ln in summr.splitlines() if ln.startswith("test ") and "core 1" in ln]
    check("twin, RECORD-1 --record: the main table has the audio column 'clk/gap (+vs 1c)' right after the core 1 column, a row each ('2c ok' then '0/0'), and the AUDIO section follows",
          len(hdr) == 1 and hdr[0].index("core 1") < hdr[0].index("clk/gap") < hdr[0].index("verdict") and re.search(r"STRESS-DEF .*2c ok +0/0", summr) is not None and "AUDIO (" in summr)
    check("twin, RECORD-1: the twin has no core 1 to switch off, so the runner says so: STRESS-1CORE 'NOT A ONE-CORE RUN' (WARN, no comparison), columns clk_vs_1c / gap_vs_1c / onecore_of in results.csv",
          len(rowsr) == 2 and rowsr[0]["test"] == "STRESS-1CORE" and "NOT A ONE-CORE RUN" in rowsr[0]["audio_why"] and rowsr[0]["verdict"] in ("WARN",)
          and all(k in rowsr[0] for k in ("clk_vs_1c", "gap_vs_1c", "onecore_of")) and rowsr[0]["onecore_of"] == "STRESS-DEF" and rowsr[1]["clk_vs_1c"] == "")
    # --prove-chain is parsed by the merged runner (the twin refuses it: it has no tone build); --only still selects
    with contextlib.redirect_stdout(io.StringIO()):
        try:
            br.main(["--sim", SIM, "--prove-chain", "--minutes", "0.1"])
            pc = None
        except SystemExit as e:
            pc = str(e.code)
    check("merged runner: --prove-chain --minutes 0.1 is parsed and runs prove_chain (against the twin it stops with 'needs the real unit with a tone build')", pc is not None and "tone build" in pc)


# ------------------------------------------------------------------------------------------------ BENCHR2: the start mark, the setup / test split, repeats, the verdict rules
check("mark_position: the run command at 2.80 s of recording and a delay of 0.35 s: the mark is at 3.15 s", near(brec.mark_position(2.80, 0.35), 3.15, 1e-9))
fx = os.path.join(tmp, "grow.wav")
an.write_wav(fx, np.zeros((R, 2), dtype=np.int16))
rcx = brec.SoxRecorder("x", sox="/bin/true")
rcx.info = brec.RecInfo(fx)
check("SoxRecorder.position: a file of one second (44-byte header + 44100 frames) reads 1.000 s, a missing file falls back to the clock", near(rcx.position(), 1.0, 1e-6) and (setattr(rcx.info, "path", fx + ".none") or rcx.position() >= 0.0))


class FakeLink:
    def __init__(self):
        self.t = 5.0

    def now(self):
        return self.t


lk = FakeLink()
tw = brec.TwinRecorder(lk)
tw.t0 = 5.0
lk.t = 7.25
check("TwinRecorder.position: the twin's clock since the recording started", near(tw.position(), 2.25, 1e-9))


def synth(setup_spike=False, test_spike=False, setup_gap=False, test_gap=False):
    """a 6 s recording: silence, a setup tone burst 0.6..1.0 s (with a planted fault), the mark at 2.0 s, the test's first note at 2.4 s (a tone to the end); the mark is in the silence between"""
    x = np.zeros(6 * R)
    a, b = int(0.6 * R), int(1.0 * R)
    ramp = np.minimum(1.0, np.minimum(np.arange(b - a), np.arange(b - a)[::-1]) / (0.05 * R))        # (50 ms fades: a note, not a kink)
    x[a:b] = 0.25 * ramp * np.sin(2 * np.pi * 440 * np.arange(b - a) / R)
    c = int(2.4 * R)
    x[c:] = 0.25 * np.minimum(1.0, np.arange(len(x) - c) / (0.05 * R)) * np.sin(2 * np.pi * 330 * np.arange(len(x) - c) / R)
    y = st(x)
    if setup_spike:
        y = plant_spike(y, int(0.8 * R), 9000)
    if test_spike:
        y = plant_spike(y, int(3.4 * R), 9000)
    if setup_gap:
        y[int(0.8 * R):int(0.8 * R) + 80] = y[int(0.8 * R) - 1]
    if test_gap:
        y[int(3.4 * R):int(3.4 * R) + 80] = y[int(3.4 * R) - 1]
    return y


def rec_res(y, name, mark=2.0, run=1.65, lead=0.6, events=None, verdict="PASS"):
    pth = os.path.join(tmp, name + ".wav")
    an.write_wav(pth, y)
    inf = brec.RecInfo(pth)
    inf.frames, inf.wall_s = y.shape[0], y.shape[0] / R
    inf.run_s, inf.mark_s, inf.lead_s = run, mark, lead
    res = {"test": name, "verdict": verdict, "why": "", "status": "DONE"}
    return brec.apply_audio(res, inf, tmp, events=events)


r_clean = rec_res(synth(), "split_clean")
check("split: a recording with no fault: setup 0/0, test 0/0, PASS", r_clean["setup_clicks"] == 0 and r_clean["setup_gaps"] == 0 and r_clean["clicks"] == "0" and r_clean["gaps"] == "0" and r_clean["verdict"] == "PASS")
r_su = rec_res(synth(setup_spike=True), "split_setup_click")
check("split: a click at 0.8 s (before the mark at 2.0 s): setup_clicks 1, the test has 0 clicks, the verdict stays PASS (setup noise never causes a FAIL)",
      r_su["setup_clicks"] == 1 and r_su["clicks"] == "0" and r_su["verdict"] == "PASS" and r_su["audio_verdict"] == "PASS")
r_sg = rec_res(synth(setup_gap=True), "split_setup_gap")
check("split: a gap at 0.8 s (before the mark): setup_gaps 1, the test 0 gaps, PASS", r_sg["setup_gaps"] == 1 and r_sg["gaps"] == "0" and r_sg["verdict"] == "PASS")
r_tc = rec_res(synth(test_spike=True), "split_test_click")
check("split: a click at 3.4 s (after the mark, 1.0 s after the first note at 2.4): clicks 1 at 1.00 s after the first note, setup 0, FAIL with the time in the reason",
      r_tc["clicks"] == "1" and r_tc["setup_clicks"] == 0 and r_tc["verdict"] == "FAIL" and near(r_tc["_clicks_t"][0], 1.0, 0.03) and "1.0" in r_tc["audio_why"] and "after the first note" in r_tc["audio_why"])
r_both = rec_res(synth(setup_spike=True, test_spike=True, test_gap=False), "split_both")
check("split: a click before and a click after the mark: setup_clicks 1 and clicks 1, FAIL only for the one after", r_both["setup_clicks"] == 1 and r_both["clicks"] == "1" and r_both["verdict"] == "FAIL")
r_tg = rec_res(synth(test_gap=True), "split_test_gap")
check("split: a gap after the mark: gaps 1 (time after the first note about 1.0 s), FAIL", r_tg["gaps"] == "1" and near(r_tg["_gaps_t"][0], 1.0, 0.03) and r_tg["verdict"] == "FAIL")
check("split: the columns: mark_s 2.000, run_s 1.650, lead_s 0.60, the first note's lag (2.4 s against 1.65 + 0.6) 0.15 s, note_events counts the events given",
      r_clean["mark_s"] == "2.000" and r_clean["run_s"] == "1.650" and r_clean["lead_s"] == "0.60" and near(float(r_clean["onset_lag_s"]), 0.15, 0.02)
      and rec_res(synth(), "split_ev", events=[0.0, 1.0, 2.0])["note_events"] == 3)
# the mark in the middle of sound: the warning that the mark is late
yl = synth()
nb = int(0.3 * R)
yl[int(1.9 * R):int(1.9 * R) + nb] = st(0.25 * np.minimum(1.0, np.minimum(np.arange(nb), np.arange(nb)[::-1]) / (0.05 * R)) * np.sin(2 * np.pi * 220 * np.arange(nb) / R))   # a note that spans the mark
r_late = rec_res(yl, "split_late_mark")
check("split: sound already on at the mark (the mark would cut a note): WARN, says so", r_late["verdict"] == "WARN" and "already on at the mark" in r_late["audio_why"])
r_old = rec_res(synth(setup_spike=True), "nomark")  # (no mark: the older whole-file analysis, all of it counts)
inf0 = brec.RecInfo(os.path.join(tmp, "nomark.wav"))
inf0.frames = synth().shape[0]
res0 = brec.apply_audio({"test": "nomark2", "verdict": "PASS", "why": ""}, inf0, tmp)
check("no mark (an older caller): the whole file is the test, as before: the click at 0.8 s is a FAIL, no setup columns", res0["verdict"] == "FAIL" and "setup_clicks" not in res0)
# note-edge exclusion survives the split, and is reported
ye = synth()
ye = plant_spike(ye, int(2.4 * R) + int(0.02 * R), 9000)          # 20 ms after the first note: the note's own attack
r_e = rec_res(ye, "split_edge", events=[0.0, 1.0])
check("note-edge exclusion after the split: a click 20 ms after the first note is counted apart (attack_clicks 1, clicks 0, PASS)", r_e["attack_clicks"] == 1 and r_e["clicks"] == "0" and r_e["verdict"] == "PASS")


def rrow(test, rep, clicks, gaps, ct=None, gt=None, avg="50.0", pk="70.0", vcnt="PASS", under_post=0, under_pre=13, edge=1, setup=(0, 0), ev=17, why=""):
    """a result as run_all has it after apply_audio, with click and gap times (seconds after the first note)"""
    ct = ct if ct is not None else [0.5 + i for i in range(clicks)]
    gt = gt if gt is not None else [0.3 + i for i in range(gaps)]
    v = "FAIL" if clicks or gaps else "PASS"
    return {"test": test, "rep": rep, "reps": 3, "label": f"{test}#{rep}", "status": "DONE", "audio": f"audio/{test}_r{rep}.wav", "clicks": str(clicks), "gaps": str(gaps), "_clicks_t": ct, "_gaps_t": gt,
            "audio_verdict": v, "audio_why": f"{clicks} clicks at {ct}" if clicks else "", "verdict": brec.LEVELS[max(brec.LEVELS.index(vcnt), brec.LEVELS.index(v))], "verdict_cnt": vcnt,
            "why": ("audio: " + f"{clicks} clicks at {ct}") if clicks else why, "audio_fault": "", "tc": "2", "tcblocks": "100000", "tmposted": "0", "avg": avg, "peak": pk, "under_post": under_post, "under_pre": under_pre,
            "attack_clicks": edge, "setup_clicks": setup[0], "setup_gaps": setup[1], "note_events": ev, "mark_s": "3.15", "onset_lag_s": "0.05"}


tt2 = [br.Test("S-1C", ""), br.Test("S-DEF", "")]
tt2[0].onecore_of = "S-DEF"
# the click in the one-core reference at the same repeat and time: WARN; a click only on two cores: FAIL
rs = br.apply_onecore([rrow("S-1C", 1, 1, 0, ct=[0.20]), rrow("S-DEF", 1, 1, 0, ct=[0.23])], tt2)
check("one-core, by time: the two-core click at 0.23 s, the reference's at 0.20 s (same repeat, within 60 ms): WARN, 'each also in the one-core reference at the same repeat and time', excess 0",
      rs[1]["verdict"] == "WARN" and "same repeat and time" in rs[1]["audio_why"] and rs[1]["clk_vs_1c"] == "0" and rs[0]["verdict"] == "WARN")
rs = br.apply_onecore([rrow("S-1C", 1, 1, 0, ct=[0.20]), rrow("S-DEF", 1, 1, 0, ct=[0.50])], tt2)
check("one-core, by time: the same count of clicks but at 0.50 s against 0.20 s: FAIL, excess 1, the time is named (the count rule alone would have let it pass)",
      rs[1]["verdict"] == "FAIL" and rs[1]["clk_vs_1c"] == "1" and "click 0.500" in rs[1]["audio_why"])
rs = br.apply_onecore([rrow("S-1C", 1, 0, 0), rrow("S-1C", 2, 1, 0, ct=[0.50]), rrow("S-DEF", 1, 1, 0, ct=[0.50]), rrow("S-DEF", 2, 1, 0, ct=[0.50])], tt2)
check("one-core, by repeat: a click at 0.50 s in two-core repeat 1 is NOT excused by the one-core repeat 2 that has it (FAIL), but repeat 2 is (WARN)",
      rs[2]["verdict"] == "FAIL" and rs[3]["verdict"] == "WARN" and [r["verdict"] for r in rs[:2]] == ["PASS", "WARN"])
rs = br.apply_onecore([rrow("S-1C", 1, 1, 0, ct=[0.50]), rrow("S-DEF", 1, 0, 0), rrow("S-DEF", 2, 1, 0, ct=[0.50])], tt2)
check("one-core, fewer reference repeats than two-core repeats: the repeat without a mate is compared with all the reference's repeats (WARN)", rs[2]["verdict"] == "WARN")
rs = br.apply_onecore([rrow("S-1C", 1, 0, 1, gt=[0.40]), rrow("S-DEF", 1, 0, 1, gt=[0.41])], tt2)
check("one-core, by time: a gap is compared the same way (0.41 s against 0.40 s: WARN)", rs[1]["verdict"] == "WARN" and rs[1]["gap_vs_1c"] == "0")

# the aggregate of repeats
tt3 = [br.Test("S-DEF", "")]
agg_rows = [rrow("S-DEF", 1, 0, 0, avg="60.0", pk="80.0", under_post=0), rrow("S-DEF", 2, 2, 0, ct=[0.115, 0.205], avg="62.0", pk="83.0", under_post=0, edge=2),
            rrow("S-DEF", 3, 0, 0, avg="61.0", pk="81.0", under_post=1)]
ag = br.aggregate(agg_rows, tt3)[0]
check("aggregate: 3 runs, clicks per run [0, 2, 0], gaps [0, 0, 0], pass 2 of 3, CPU mean 61.0 min 60.0 max 62.0, peak max 83.0, underruns after the mark [0, 0, 1], before [13, 13, 13], edge-excluded [1, 2, 1], 17 note events",
      ag["reps"] == 3 and ag["clicks"] == [0, 2, 0] and ag["gaps"] == [0, 0, 0] and ag["passes"] == 2 and near(ag["cpu_mean"], 61.0, 1e-9) and ag["cpu_min"] == 60.0 and ag["cpu_max"] == 62.0 and ag["peak_max"] == 83.0
      and ag["under_post"] == [0, 0, 1] and ag["under_pre"] == [13, 13, 13] and ag["edge"] == [1, 2, 1] and ag["note_events"] == 17)
check("aggregate: a test with one click-repeat FAILs as a whole; the reason names repeat 2 and its click times", ag["verdict"] == "FAIL" and len(ag["reasons"]) == 1 and ag["reasons"][0].startswith("repeat 2:") and "0.115" in ag["reasons"][0] and "0.205" in ag["reasons"][0])
txt = br.fmt_repeats([ag])
check("REPEATS section: the aggregate line has runs, pass, clicks per run, gaps per run, CPU mean/min/max, underruns per run; the second line has the setup, the before-mark underruns and the excluded note edges",
      re.search(r"S-DEF\s+runs 3\s+pass 2/3\s+clicks \[0, 2, 0\]\s+gaps \[0, 0, 0\]\s+CPU avg 61.0/60.0/62.0 \(mean/min/max\).*USB underruns after mark \[0, 0, 1\]\s+FAIL", txt) is not None
      and "setup clicks/gaps [0/0, 0/0, 0/0]" in txt and "before mark [13, 13, 13]" in txt and "note-edge clicks excluded [1, 2, 1] (of 17 note events)" in txt and "repeat 2: " in txt)
ag2 = br.aggregate([rrow("S-DEF", k, 0, 0) for k in (1, 2, 3)], tt3)[0]
ag3 = br.aggregate([rrow("S-DEF", 1, 0, 0), rrow("S-DEF", 2, 0, 0, vcnt="WARN"), rrow("S-DEF", 3, 0, 0)], tt3)[0]
check("aggregate: three clean repeats PASS (3/3); one WARN repeat makes the test WARN, not PASS", ag2["verdict"] == "PASS" and ag2["passes"] == 3 and ag3["verdict"] == "WARN" and ag3["passes"] == 2 and br.fmt_repeats(br.aggregate([rrow("S-DEF", 1, 0, 0)], tt3)[:1]) == "")
check("REPEATS section: only when some test ran more than once (a single run prints none)", br.fmt_repeats([dict(ag, reps=1)]) == "")

# KEEP and prune
rk = os.path.join(tmp, "keepruns")
for i, nm in enumerate(("a", "b", "c")):
    d = os.path.join(rk, f"2026-01-0{i + 1}-X", "audio")
    os.makedirs(d)
    for f in ("T_r1.wav", "T_r2.wav", "U.wav"):
        an.write_wav(os.path.join(d, f), np.zeros((1000, 2), dtype=np.int16))
    open(os.path.join(rk, f"2026-01-0{i + 1}-X", "summary.txt"), "w").write("x")
    os.utime(os.path.join(rk, f"2026-01-0{i + 1}-X", "summary.txt"), (1000 + i, 1000 + i))
brec.write_keep(os.path.join(rk, "2026-01-01-X", "audio"), ["T_r1.wav", "T_r2.wav"])
freed = brec.prune(rk, 1)
left = {n: sorted(f for f in os.listdir(os.path.join(rk, n, "audio")) if f.endswith(".wav")) for n in sorted(os.listdir(rk))}
check("prune: the newest run keeps everything; an older run loses its clean audio (U.wav) but keeps the repeats of its failing test (KEEP.txt); a run without KEEP.txt loses all",
      left["2026-01-03-X"] == ["T_r1.wav", "T_r2.wav", "U.wav"] and left["2026-01-01-X"] == ["T_r1.wav", "T_r2.wav"] and left["2026-01-02-X"] == [] and len(freed) == 2)

# the whole recorded path against the twin: repeats, the mark, the order of the console words, --keep-all
if os.path.exists(SIM):
    suite3 = os.path.join(tmp, "rep.txt")
    open(suite3, "w").write("""suite REP
test M07-CHORDS  M07 FINAL REEL, four chords
load multi 7 "FINAL REEL"
repeat 2
at 0.0 chord C3 E3 G3 B3 vel 90
at 3.0 chord A2 C3 E3 G3 vel 90
at 6.0 release
tail 2
test SILENCE  nothing
tail 2
""")
    outp = os.path.join(tmp, "twinrep")
    runsp = os.path.join(tmp, "twinrepruns")
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        try:
            codp = br.main(["--sim", SIM, "--record", "--suite", suite3, "--out", outp, "--runs", runsp, "--keep", "5"])
        except SystemExit as e:
            codp = e.code
    rowp = list(csv.DictReader(open(os.path.join(outp, "results.csv")))) if os.path.exists(os.path.join(outp, "results.csv")) else []
    sump = open(os.path.join(outp, "summary.txt")).read() if os.path.exists(os.path.join(outp, "summary.txt")) else ""
    check("twin, repeat 2 in the suite: three result lines (M07-CHORDS #1, #2, SILENCE), rep / reps columns, a WAV per repeat (M07-CHORDS_r1, _r2; SILENCE.wav), the rows labelled NAME#k in the summary",
          [(x["test"], x["rep"], x["reps"]) for x in rowp] == [("M07-CHORDS", "1", "2"), ("M07-CHORDS", "2", "2"), ("SILENCE", "1", "1")]
          and sorted(f for f in os.listdir(os.path.join(outp, "audio")) if f.endswith(".wav")) == ["M07-CHORDS_r1.wav", "M07-CHORDS_r2.wav", "SILENCE.wav"] and "M07-CHORDS#2" in sump and "SILENCE " in sump)
    check("twin: the REPEATS section has the aggregate line for M07-CHORDS (runs 2, pass 2/2) and for SILENCE; repeats.csv has one line per test; the exit code is 0 when all is clean",
          re.search(r"M07-CHORDS\s+runs 2\s+pass \d/2\s+clicks \[0, 0\]\s+gaps \[0, 0\]", sump) is not None and re.search(r"SILENCE\s+runs 1", sump) is not None
          and len(list(csv.DictReader(open(os.path.join(outp, "repeats.csv"))))) == 2 and "2 tests:" in sump)
    mk = rowp[0]
    check("twin: the mark: run_s 2.800 s (1.0 settle + 0.3 + 1.5 setup wait), mark_s = run_s + 0.35, the lead-in 0.60, no sound before run_s + lead (the first note at about 3.4 s: lag small), setup 0/0, note events 13 (8 note-ons, 5 note-offs)",
          mk["run_s"] == "2.800" and mk["mark_s"] == "3.150" and mk["lead_s"] == "0.60" and abs(float(mk["onset_lag_s"])) < 0.03 and (mk["setup_clicks"], mk["setup_gaps"]) == ("0", "0") and mk["note_events"] == "13")
    check("twin: the underrun counters: before the mark and after it are in the columns (the twin has none: 0 and 0)", mk["under_pre"] == "0" and mk["under_post"] == "0")
    wavp = os.path.join(outp, mk["audio"])
    xp, _ = an.read_wav(wavp)
    n0p = np.flatnonzero(np.any(np.asarray(xp) != 0, axis=1))[0]
    check("twin: the first sound comes after the mark (%.2f s > %.2f s) and after the run command plus the lead-in (%.2f s)" % (n0p / R, float(mk["mark_s"]), float(mk["run_s"]) + 0.6),
          n0p / R > float(mk["mark_s"]) and n0p / R >= float(mk["run_s"]) + 0.6 - 0.01)
    # the console words, in the order of BENCHR2: recorder first, settle, counters, reset, setup, run, counters at the mark
    words = [ln[3:] for ln in open(os.path.join(outp, "console.log")).read().splitlines() if ln.startswith("## ")]
    first_reset = words.index("bench reset")
    first_run = [i for i, w in enumerate(words) if w.startswith("bench run")][0]
    check("console order of one recorded test: a `status` (the counters' baseline, after the stream settled) before `bench reset`; then the setup, `bench measure`, the events, `bench run`, and a `status` at the mark right after it",
          words[first_reset - 1] == "status" and words.index("bench measure 600 8600") > first_reset and first_run > first_reset + 3 and words[first_run + 1] == "status" and words[first_run - 1].startswith("bench ev "))
    # a click planted in the setup part and in the test part of a twin recording, analysed with the run's own marks
    pth = os.path.join(tmp, "plant.wav")
    xq = np.array(xp)
    xq = plant_spike(xq, int(1.5 * R), 9000)                              # 1.5 s: in the setup (silence before the run command at 2.8 s): a click in silence
    xq = plant_spike(xq, int(float(mk["mark_s"]) * R + 4.5 * R), 6000)     # 4.5 s into the test, in a chord
    an.write_wav(pth, xq)
    inf = brec.RecInfo(pth)
    inf.frames = xq.shape[0]
    inf.run_s, inf.mark_s, inf.lead_s = float(mk["run_s"]), float(mk["mark_s"]), 0.6
    rq = brec.apply_audio({"test": "plant", "verdict": "PASS", "why": ""}, inf, tmp, events=[0, 3.0, 6.0])
    check("twin recording with a click planted in the setup (1.5 s) and one in the test (4.5 s after the mark): setup_clicks 1 (the one in silence), clicks 1 in the test, FAIL for the one after the mark only",
          rq["setup_clicks"] == 1 and rq["clicks"] == "1" and rq["verdict"] == "FAIL")
    # --keep-all and the KEEP list at the run level
    outk = os.path.join(tmp, "twinkeep")
    runsk = os.path.join(tmp, "twinkeepruns")
    os.makedirs(os.path.join(runsk, "2025-01-01-OLD", "audio"))
    an.write_wav(os.path.join(runsk, "2025-01-01-OLD", "audio", "old.wav"), np.zeros((100, 2), dtype=np.int16))
    open(os.path.join(runsk, "2025-01-01-OLD", "summary.txt"), "w").write("x")
    os.utime(os.path.join(runsk, "2025-01-01-OLD", "summary.txt"), (10, 10))
    with contextlib.redirect_stdout(io.StringIO()):
        br.main(["--sim", SIM, "--record", "--suite", suite3, "--only", "SILENCE", "--out", outk, "--runs", runsk, "--keep", "0", "--keep-all"])
    check("--keep-all: the 5-runs rule is off (the old run's audio stays even with --keep 0)", os.path.exists(os.path.join(runsk, "2025-01-01-OLD", "audio", "old.wav")))
    with contextlib.redirect_stdout(io.StringIO()):
        br.main(["--sim", SIM, "--record", "--suite", suite3, "--only", "SILENCE", "--out", os.path.join(tmp, "twinkeep2"), "--runs", runsk, "--keep", "0"])
    check("without --keep-all, --keep 0 deletes the old run's audio (the rule as before)", not os.path.exists(os.path.join(runsk, "2025-01-01-OLD", "audio", "old.wav")))
    # --repeat on the command line overrides the suite; settings are refused when wrong
    outr2 = os.path.join(tmp, "twinrep3")
    with contextlib.redirect_stdout(io.StringIO()):
        br.main(["--sim", SIM, "--record", "--suite", suite3, "--only", "M07-CHORDS", "--repeat", "3", "--out", outr2, "--runs", runsp, "--keep", "5"])
    r3 = list(csv.DictReader(open(os.path.join(outr2, "results.csv"))))
    check("--repeat 3 --only M07-CHORDS: three lines, rep 1..3 of 3 (the command line beats the suite's `repeat 2`)", [(x["rep"], x["reps"]) for x in r3] == [("1", "3"), ("2", "3"), ("3", "3")])
    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
        try:
            br.main(["--sim", SIM, "--record", "--suite", suite3, "--only", "SILENCE", "--lead", "0.3", "--out", os.path.join(tmp, "bad1"), "--runs", runsp])
            refused = False
        except SystemExit as e:
            refused = "mark" in str(e.code)
    check("--lead 0.3 with the mark at 0.35 s is refused before anything runs", refused and not os.path.exists(os.path.join(tmp, "bad1", "results.csv")))
    # without --record nothing of this appears: no lead-in, the original timeline
    outn = os.path.join(tmp, "norec")
    with contextlib.redirect_stdout(io.StringIO()):
        br.main(["--sim", SIM, "--suite", suite3, "--only", "M07-CHORDS", "--out", outn, "--runs", runsp])
    rn = list(csv.DictReader(open(os.path.join(outn, "results.csv"))))
    check("without --record: no lead-in (dur_s 8.0 as before), no audio columns; a `repeat 2` in the suite still runs twice", len(rn) == 2 and rn[0]["dur_s"] == "8.0" and "audio" not in rn[0])

print("benchaudio:", "all ok" if not bad else f"{bad} FAILED")
sys.exit(1 if bad else 0)
