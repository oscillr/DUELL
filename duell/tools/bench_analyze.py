#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""DUELL automated bench suite, stage 2: the sound analysis (Python + numpy only, deterministic: no random numbers, no threads).

  python3 tools/bench_analyze.py A.wav [B.wav ...]          one line per file: clicks gaps peak_dbfs rms_dbfs dc centroid_hz tail_ms + verdict
  python3 tools/bench_analyze.py --selfcheck A.wav B.wav     the same test recorded twice must give the same numbers (exit 1 if not)
  python3 tools/bench_analyze.py --tone REC.wav              the USB test triangle of a FELUCCA_UAC_TONE build: every frame must follow the one before
  options: --csv (machine line), --name TEST (the SILENCE test may be silent), and every threshold below (see THRESHOLDS)

Words. **frame** = one stereo sample pair (L and R); 44,100 a second. **FS** = full scale (32768). **dBFS** = dB below full scale.
**click** = a sudden jump of the waveform that the sound around it does not explain: the second difference d2[n] = x[n+1] - 2 x[n] + x[n-1]
(a step or a spike in the waveform gives one big d2; a smooth tone or a smooth sweep gives a tiny one) is larger than an absolute floor, larger than
CLICK_K times the average |d2| of the sound around it (10 ms either side, the 3 frames around the point left out; the larger side) AND larger than
CLICK_M times the largest |d2| in the 40 ms either side (16 frames around the point left out): the sound's own edges, such as a saw's jump every
period or a sync, are as large as their neighbours and so are not clicks. A train of small equal steps (zipper) is not either: see BENCH_STAGE2.md.
Hits within 16 frames of each other, in either channel, are one click. **gap** = a run of at least GAP_MIN frames that are all the same frame
(exact digital silence, or the last frame repeated, which is what a starved USB stream does) with sound right before it and right after it
(within 8 frames the movement, measured from the held value, reaches GAP_LEVEL). A dropout also makes two jumps at its edges; those are counted as the gap, not as clicks.
**clip** = a run of frames at full scale; the corners where the wave meets full scale are not clicks (they are reported as clips and are a WARN).
**sounding** = the 10 ms windows whose RMS is above SOUND_DB; RMS, DC and the centroid are measured over them.
**DC** = the mean of the whole recording, as dBFS of the larger channel (0 = nothing; -120 means exactly zero).
**centroid** = the power-weighted mean frequency of the sounding part (Hann windows of 2048 frames). **tail** = from the last 10 ms window within
20 dB of the loudest one to the last window above TAIL_FLOOR; **abrupt end** = the sound stops from above TAIL_FLOOR - 24 dB within one 10 ms window
(the HALL gate class). HYPOTHESIS: the thresholds are tuned on the Mac twin's renders and synthetic faults (tests/bench_analyze_test.py), not yet on
real recordings of the unit; they are settable and each run prints the ones in force.
"""
import argparse
import csv
import io
import os
import struct
import sys

import numpy as np

RATE = 44100

# name: (default, text). Settable with --NAME (lower case, dashes) and as keyword arguments of analyze().
THRESHOLDS = {
    "click_k": (4.0, "a click: |d2| above this many times the local average |d2|"),
    "click_m": (3.0, "a click: |d2| above this many times the largest |d2| of the neighbourhood (the sound's own edges are as large as their neighbours)"),
    "click_nbr_ms": (40.0, "the neighbourhood for that rule: this many ms either side"),
    "click_floor": (16.0, "a click: |d2| at least this many 16-bit steps (16 = -66 dBFS)"),
    "click_ref_ms": (10.0, "the local average is taken this many ms either side of the point"),
    "click_merge": (16, "hits closer than this many frames are one click"),
    "gap_min": (16, "a gap: at least this many equal frames in a row (16 = 0.36 ms)"),
    "gap_level": (-50.0, "a gap: the sound on both sides (8 frames, measured from the held value) reaches this level, dBFS (a note ending on its own fades below it)"),
    "sound_db": (-70.0, "a 10 ms window is sounding above this RMS, dBFS"),
    "tail_floor": (-70.0, "the tail ends at the last 10 ms window above this RMS, dBFS"),
    "abrupt_db": (24.0, "an abrupt end: the level falls by more than this many dB from one 10 ms window to the next"),
    "dc_warn": (-60.0, "WARN when the DC is above this, dBFS"),
    "clip_warn": (1, "WARN when at least this many clip runs (0 = never)"),
    "event_pre_ms": (20.0, "clicks this long before a note event (note on or off) are the note's attack or release, counted apart"),
    "event_post_ms": (60.0, "clicks this long after a note event are counted apart (attack_clicks), not as clicks"),
    "chunk": (1 << 21, "frames per processing chunk (does not change the result)"),
}
SILENT_DB = -120.0
WIN = 441          # 10 ms
HANN = 2048


class WavError(Exception):
    pass


# ------------------------------------------------------------------------------------------------ the file
def read_wav(path):
    """(frames int16 array (n, 2) as a memory map or array, rate). Reads the header itself so a file whose size fields are
    wrong (a recorder that was killed) still opens: the data chunk is whatever is in the file."""
    size = os.path.getsize(path)
    with open(path, "rb") as f:
        head = f.read(12)
        if len(head) < 12 or head[:4] != b"RIFF" or head[8:12] != b"WAVE":
            raise WavError(f"{path}: not a WAV file")
        rate = ch = bits = tag = None
        pos = 12
        while True:
            f.seek(pos)
            h = f.read(8)
            if len(h) < 8:
                raise WavError(f"{path}: no data chunk")
            cid, ln = h[:4], struct.unpack("<I", h[4:])[0]
            if cid == b"fmt ":
                fm = f.read(min(ln, 40))
                tag, ch, rate, _, _, bits = struct.unpack("<HHIIHH", fm[:16])
                if tag == 0xFFFE and len(fm) >= 26:
                    tag = struct.unpack("<H", fm[24:26])[0]
            elif cid == b"data":
                start = pos + 8
                n = min(ln, size - start) if ln not in (0, 0xFFFFFFFF) else size - start
                break
            pos += 8 + ln + (ln & 1)
    if tag != 1 or bits != 16 or ch != 2:
        raise WavError(f"{path}: need 16-bit stereo PCM (this is format {tag}, {ch} ch, {bits} bit)")
    n -= n % 4
    if n <= 0:
        return np.zeros((0, 2), dtype=np.int16), rate
    return np.memmap(path, dtype="<i2", mode="r", offset=start, shape=(n // 4, 2)), rate


def write_wav(path, x, rate=RATE):
    """a plain 16-bit stereo WAV from an int16 array (n, 2)"""
    x = np.ascontiguousarray(x, dtype="<i2")
    n = x.shape[0] * 4
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + n) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, 2, rate, rate * 4, 4, 16))
        f.write(b"data" + struct.pack("<I", n))
        f.write(x.tobytes())


def db(v):
    return SILENT_DB if v <= 1e-6 else max(SILENT_DB, 20.0 * np.log10(v))


# ------------------------------------------------------------------------------------------------ gaps and clips
def _runs(mask):
    """(starts, ends) of the runs of True in a bool array (end exclusive)"""
    m = np.concatenate(([False], mask, [False]))
    d = np.diff(m.astype(np.int8))
    return np.flatnonzero(d == 1), np.flatnonzero(d == -1)


def find_gaps(x, gap_min, gap_level):
    """runs of equal frames (>= gap_min) with sound on both sides: list of (start, length). x: int16 (n, 2)"""
    n = x.shape[0]
    if n < 2:
        return []
    ctx = 8
    lim = 32768.0 * 10.0 ** (gap_level / 20.0)
    eq = np.zeros(n - 1, dtype=bool)                  # eq[i]: frame i+1 equals frame i (a bool a frame: 26 MB for ten minutes)
    step = 1 << 22
    for s in range(0, n - 1, step):
        e = min(n - 1, s + step)
        a = np.asarray(x[s:e + 1])
        eq[s:e] = np.all(a[1:] == a[:-1], axis=1)
    st, en = _runs(eq)
    out = [(int(b), int(c) - int(b) + 1) for b, c in zip(st, en)]     # frames b .. c are equal: c - b + 1 frames
    res = []
    for b, ln in out:
        if ln < gap_min:
            continue
        held = x[b]
        if np.any(np.abs(held.astype(np.int32)) >= 32767):     # a flat top at full scale is clipping
            continue
        pre = np.asarray(x[max(0, b - ctx):b])
        post = np.asarray(x[b + ln:b + ln + ctx])
        if pre.size == 0 or post.size == 0:
            continue                                           # (the file's start or end: nothing on that side)
        h = held.astype(np.int32)                              # (the sound is measured from the held value: a slow drift around a DC offset is not sound)
        if np.abs(pre.astype(np.int32) - h).max() >= lim and np.abs(post.astype(np.int32) - h).max() >= lim:
            res.append((b, ln))
    return res


def count_clips(x):
    """runs of frames at full scale (either channel): number of runs"""
    n = x.shape[0]
    total = 0
    carry = False
    step = 1 << 22
    for s in range(0, n, step):
        a = np.asarray(x[s:s + step]).astype(np.int32)
        m = np.any((a >= 32767) | (a <= -32768), axis=1)
        st, en = _runs(m)
        total += len(st)
        if carry and len(st) and st[0] == 0:
            total -= 1
        carry = bool(m[-1])
    return total


# ------------------------------------------------------------------------------------------------ clicks
def _slide_max(a, L):
    """m[i] = max(a[i : i + L]) for every i (windows cut at the end), O(n): block-wise prefix and suffix maxima (van Herk / Gil-Werman)"""
    n = a.shape[0]
    if L <= 1:
        return a.copy()
    pad = (-(n + L - 1)) % L
    z = np.concatenate((a, np.zeros(L - 1 + pad)))           # (zeros: the window running over the end sees nothing there)
    r = z.reshape(-1, L)
    g = np.maximum.accumulate(r, axis=1).ravel()
    h = np.maximum.accumulate(r[:, ::-1], axis=1)[:, ::-1].ravel()
    return np.maximum(h[:n], g[L - 1:L - 1 + n])


def _click_hits(ch, lo, hi, ref_w, nbr_w, excl, floor, k, m):
    """positions n in [lo, hi) of one channel (a float array, the chunk with its margin) where |d2| is a click:
    |d2| > floor, > k x the mean |d2| of the sound around (ref_w frames each side, the 3 around the point left out; the larger side), and
    > m x the largest |d2| of the neighbourhood (nbr_w frames each side, excl frames around the point left out). The second rule is what keeps the
    sound's own edges (a saw's or a sync's jump once a period, an attack) from being clicks: they are as large as their neighbours."""
    n = ch.shape[0]
    a = np.zeros(n)
    a[1:-1] = np.abs(ch[2:] - 2.0 * ch[1:-1] + ch[:-2])
    cs = np.concatenate(([0.0], np.cumsum(a)))
    idx = np.arange(lo, hi)
    E = 3
    l0, l1 = np.maximum(idx - ref_w, 0), np.maximum(idx - E, 0)
    r0, r1 = np.minimum(idx + E + 1, n), np.minimum(idx + ref_w + 1, n)
    lm = (cs[l1] - cs[l0]) / np.maximum(l1 - l0, 1)
    rm = (cs[r1] - cs[r0]) / np.maximum(r1 - r0, 1)
    thr = np.maximum(k * np.maximum(lm, rm), floor)
    cand = np.flatnonzero(a[lo:hi] > thr)
    if len(cand) == 0:
        return cand + lo
    L = nbr_w - excl
    M = _slide_max(a, L)                                       # M[i] = max a[i .. i+L-1]
    pos = cand + lo
    left = np.where(pos - nbr_w >= 0, M[np.maximum(pos - nbr_w, 0)], 0.0)
    right = np.where(pos + excl + 1 < n, M[np.minimum(pos + excl + 1, n - 1)], 0.0)
    nb = np.maximum(left, right)
    return pos[a[pos] > m * nb]


def find_clicks(x, k, floor, ref_ms, nbr_ms, m, merge, rate, chunk):
    """sorted positions (frames) of the click hits of both channels, before merging. Same result for any chunk size."""
    n = x.shape[0]
    ref_w = max(4, int(round(ref_ms * 1e-3 * rate)))
    nbr_w = max(8, int(round(nbr_ms * 1e-3 * rate)))
    margin = max(ref_w, nbr_w) + 8
    hits = []
    for s in range(0, n, chunk):
        e = min(n, s + chunk)
        a0, a1 = max(0, s - margin), min(n, e + margin)
        seg = np.asarray(x[a0:a1]).astype(np.float64)
        for c in range(2):
            h = _click_hits(seg[:, c], s - a0, e - a0, ref_w, nbr_w, int(merge), floor, k, m)
            hits.append(h + a0)
    if not hits:
        return np.zeros(0, dtype=np.int64)
    return np.unique(np.concatenate(hits))


def merge_hits(hits, merge):
    if len(hits) == 0:
        return []
    groups, g0, last = [], int(hits[0]), int(hits[0])
    for h in hits[1:]:
        h = int(h)
        if h - last > merge:
            groups.append((g0, last))
            g0 = h
        last = h
    groups.append((g0, last))
    return groups


# ------------------------------------------------------------------------------------------------ levels
def window_levels(x, chunk):
    """RMS (both channels, FS = 1) of every whole 10 ms window: float array"""
    n = x.shape[0] // WIN
    out = np.zeros(n)
    per = max(1, chunk // WIN)
    for w in range(0, n, per):
        e = min(n, w + per)
        a = np.asarray(x[w * WIN:e * WIN]).astype(np.float64) / 32768.0
        out[w:e] = np.sqrt((a * a).reshape(e - w, WIN * 2).mean(axis=1))
    return out


def spectral_centroid(x, sounding, rate):
    """power-weighted mean frequency over the Hann windows (2048 frames, no overlap) that lie in sounding 10 ms windows"""
    n = x.shape[0] // HANN
    if n == 0 or not sounding.any():
        return 0.0
    win = np.hanning(HANN)
    freqs = np.fft.rfftfreq(HANN, 1.0 / rate)
    acc = np.zeros(HANN // 2 + 1)
    cnt = 0
    per = 256
    for b in range(0, n, per):
        e = min(n, b + per)
        a = np.asarray(x[b * HANN:e * HANN]).astype(np.float64) / 32768.0
        mono = a.mean(axis=1).reshape(e - b, HANN)
        # keep a frame if the 10 ms windows it covers are mostly sounding
        keep = []
        for i in range(b, e):
            w0, w1 = (i * HANN) // WIN, min(len(sounding), ((i + 1) * HANN) // WIN)
            keep.append(w1 > w0 and sounding[w0:w1].mean() > 0.5)
        keep = np.array(keep)
        if not keep.any():
            continue
        sp = np.abs(np.fft.rfft(mono[keep] * win, axis=1)) ** 2
        acc += sp.sum(axis=0)
        cnt += int(keep.sum())
    tot = acc.sum()
    return float((freqs * acc).sum() / tot) if cnt and tot > 0 else 0.0


# ------------------------------------------------------------------------------------------------ the analysis
def analyze(x, rate=RATE, events=None, **over):
    """x: int16 frames (n, 2). events: the times (seconds from the test's start, sorted) of the note-ons and note-offs the test played, or None; the
    first note-on is taken to be the first sound in the file, which aligns them. Clicks inside the guard windows around the events (event_pre_ms,
    event_post_ms) are a note's own attack or release: counted as attack_clicks, not as clicks. Returns a dict: clicks, gaps, peak_dbfs,
    rms_dbfs, dc, centroid_hz, tail_ms (the seven columns) and clips, attack_clicks, abrupt_end, cut, silent, seconds, click_at (seconds),
    gap_at ((seconds, ms)), thresholds."""
    th = {k: v[0] for k, v in THRESHOLDS.items()}
    for k, v in over.items():
        if k not in th:
            raise KeyError(k)
        th[k] = v
    n = x.shape[0]
    r = dict(seconds=n / rate, clicks=0, gaps=0, peak_dbfs=SILENT_DB, rms_dbfs=SILENT_DB, dc=SILENT_DB, centroid_hz=0.0, tail_ms=0.0,
             clips=0, attack_clicks=0, abrupt_end=False, cut=False, silent=True, click_at=[], gap_at=[], thresholds=th, first_sound_s=None)
    if n == 0:
        return r
    # peak and DC
    pk, sums = 0, np.zeros(2)
    step = 1 << 22
    for s in range(0, n, step):
        a = np.asarray(x[s:s + step])
        pk = max(pk, int(np.abs(a.astype(np.int32)).max()))
        sums += a.astype(np.int64).sum(axis=0)
    r["peak_dbfs"] = db(pk / 32768.0)
    r["dc"] = db(float(np.abs(sums / n).max()) / 32768.0)
    r["clips"] = count_clips(x)
    r["silent"] = pk == 0
    # windows, sounding, tail
    lev = window_levels(x, th["chunk"])
    if len(lev):
        ldb = np.array([db(v) for v in lev])
        sounding = ldb > th["sound_db"]
        if sounding.any():
            r["first_sound_s"] = round(float(np.flatnonzero(sounding)[0]) * WIN / rate, 4)       # (the start of the first 10 ms window that sounds: where the first note is, to 10 ms)
            sq = np.sum(lev[sounding] ** 2) / sounding.sum()
            r["rms_dbfs"] = db(float(np.sqrt(sq)))
            r["centroid_hz"] = spectral_centroid(x, sounding, rate)
            loud = ldb.max()
            main_end = int(np.flatnonzero(ldb >= loud - 20.0)[-1])
            audible = np.flatnonzero(ldb > th["tail_floor"])
            end = int(audible[-1])
            r["tail_ms"] = max(0, end - main_end) * WIN * 1000.0 / rate
            if end + 1 < len(ldb):
                r["abrupt_end"] = bool(ldb[end] - ldb[end + 1] > th["abrupt_db"])
            else:
                r["cut"] = True            # the recording ends while the sound is still there
    # gaps, then clicks (a click next to a gap or a clip is the gap's or the clip's edge)
    gaps = find_gaps(x, th["gap_min"], th["gap_level"])
    r["gaps"] = len(gaps)
    r["gap_at"] = [(round(b / rate, 4), round(ln * 1000.0 / rate, 3)) for b, ln in gaps]
    hits = find_clicks(x, th["click_k"], th["click_floor"], th["click_ref_ms"], th["click_nbr_ms"], th["click_m"], th["click_merge"], rate, int(th["chunk"]))
    if len(hits):
        drop = np.zeros(len(hits), dtype=bool)
        for b, ln in gaps:
            drop |= (hits >= b - 3) & (hits <= b + ln + 3)
        if r["clips"]:                                    # the corners of a clipped wave: next to a full-scale frame
            for i, h in enumerate(hits):
                if not drop[i]:
                    seg = np.asarray(x[max(0, h - 3):h + 5]).astype(np.int32)
                    drop[i] = bool(np.any((seg >= 32767) | (seg <= -32768)))
        hits = hits[~drop]
    groups = merge_hits(hits, int(th["click_merge"]))
    if events is not None and len(events) and len(lev):
        first = np.flatnonzero(ldb > th["sound_db"])
        if len(first):
            off = first[0] * WIN / rate - events[0]                  # the first sound is the first event
            ev = np.asarray(events, dtype=np.float64) + off
            lo, hi = ev - th["event_pre_ms"] * 1e-3, ev + th["event_post_ms"] * 1e-3
            keep = []
            for g in groups:
                t0, t1 = g[0] / rate, g[1] / rate
                inside = bool(np.any((t1 >= lo) & (t0 <= hi)))
                r["attack_clicks"] += inside
                if not inside:
                    keep.append(g)
            groups = keep
    r["clicks"] = len(groups)
    r["click_at"] = [round(g[0] / rate, 4) for g in groups]
    return r


def analyze_file(path, events=None, **over):
    x, rate = read_wav(path)
    if rate != RATE:
        r = analyze(np.zeros((0, 2), dtype=np.int16), rate, **over)
        r["error"] = f"{path}: {rate} Hz, need {RATE} Hz"
        return r
    r = analyze(x, rate, events=events, **over)
    r["file"] = path
    return r


# ------------------------------------------------------------------------------------------------ the verdict
def audio_verdict(r, name=""):
    """('PASS' | 'WARN' | 'FAIL', reasons): clicks or gaps = FAIL; clipping, DC, an abrupt end, silence = WARN"""
    th, level, why = r["thresholds"], 0, []
    if r.get("error"):
        return "FAIL", r["error"]
    if r["clicks"]:
        level = 2
        why.append(f"{r['clicks']} clicks " + (f"at {', '.join(f'{t:.3f}' for t in r['click_at'][:8])}{' ..' if len(r['click_at']) > 8 else ''} s after the first note" if r.get("rel_times")
                                               else f"(first at {r['click_at'][0]:.3f} s)"))
    if r["gaps"]:
        level = 2
        why.append(f"{r['gaps']} gaps " + (f"at {', '.join(f'{g[0]:.3f} ({g[1]:g} ms)' for g in r['gap_at'][:8])}{' ..' if len(r['gap_at']) > 8 else ''} s after the first note" if r.get("rel_times")
                                           else f"(first at {r['gap_at'][0][0]:.3f} s, {r['gap_at'][0][1]:.2f} ms)"))
    if r["silent"] and not name.upper().startswith("SILENCE"):
        level = max(level, 1)
        why.append("the recording is digital silence")
    if th["clip_warn"] and r["clips"] >= th["clip_warn"]:
        level = max(level, 1)
        why.append(f"{r['clips']} clipped runs")
    if r["dc"] > th["dc_warn"]:
        level = max(level, 1)
        why.append(f"DC {r['dc']:.1f} dBFS")
    if r["abrupt_end"]:
        level = max(level, 1)
        why.append("the sound ends abruptly")
    return ("PASS", "WARN", "FAIL")[level], "; ".join(why)


COLS = ["clicks", "gaps", "peak_dbfs", "rms_dbfs", "dc", "centroid_hz", "tail_ms"]


def fmt_cells(r):
    return {"clicks": str(r["clicks"]), "gaps": str(r["gaps"]), "peak_dbfs": f"{r['peak_dbfs']:.1f}", "rms_dbfs": f"{r['rms_dbfs']:.1f}",
            "dc": f"{r['dc']:.1f}", "centroid_hz": f"{r['centroid_hz']:.0f}", "tail_ms": f"{r['tail_ms']:.0f}"}


def line(name, r):
    v, why = audio_verdict(r, name)
    c = fmt_cells(r)
    return (f"{name:<22}{c['clicks']:>7}{c['gaps']:>6}{c['peak_dbfs']:>10}{c['rms_dbfs']:>10}{c['dc']:>8}{c['centroid_hz']:>10}{c['tail_ms']:>9}  {v:<5}" +
            (f" {why}" if why else ""))


HEADER = f"{'test':<22}{'clicks':>7}{'gaps':>6}{'peak_dbfs':>10}{'rms_dbfs':>10}{'dc':>8}{'centroid':>10}{'tail_ms':>9}  verdict"


# ------------------------------------------------------------------------------------------------ the self-check
SELF_TOL = {"clicks": 0, "gaps": 0, "peak_dbfs": 0.5, "rms_dbfs": 0.5, "dc": 3.0, "centroid_hz_rel": 0.02, "tail_ms": 50.0, "seconds": 0.1}


def selfcheck(ra, rb, tol=None):
    """the same test recorded twice: (ok, [lines]). Counts must be equal; levels, the centroid, the tail and the length within SELF_TOL"""
    t = dict(SELF_TOL, **(tol or {}))
    rows, ok = [], True

    def one(name, a, b, d, tolv, unit=""):
        nonlocal ok
        good = abs(a - b) <= tolv
        ok &= good
        rows.append(f"  {name:<12}{a:>12.2f}{b:>12.2f}   diff {d:>8.2f}{unit}  (tolerance {tolv:g})  {'ok' if good else 'DIFFERENT'}")
    one("clicks", ra["clicks"], rb["clicks"], ra["clicks"] - rb["clicks"], t["clicks"])
    one("gaps", ra["gaps"], rb["gaps"], ra["gaps"] - rb["gaps"], t["gaps"])
    one("peak_dbfs", ra["peak_dbfs"], rb["peak_dbfs"], ra["peak_dbfs"] - rb["peak_dbfs"], t["peak_dbfs"], " dB")
    one("rms_dbfs", ra["rms_dbfs"], rb["rms_dbfs"], ra["rms_dbfs"] - rb["rms_dbfs"], t["rms_dbfs"], " dB")
    one("dc", ra["dc"], rb["dc"], ra["dc"] - rb["dc"], t["dc"], " dB")
    ca, cb = ra["centroid_hz"], rb["centroid_hz"]
    rel = abs(ca - cb) / max(ca, cb, 1.0)
    one("centroid_hz", ca, cb, ca - cb, t["centroid_hz_rel"] * max(ca, cb, 1.0), " Hz")
    one("tail_ms", ra["tail_ms"], rb["tail_ms"], ra["tail_ms"] - rb["tail_ms"], t["tail_ms"], " ms")
    one("seconds", ra["seconds"], rb["seconds"], ra["seconds"] - rb["seconds"], t["seconds"], " s")
    return ok, rows


# ------------------------------------------------------------------------------------------------ the tone proof
def _tone_tables():
    ph = np.arange(100)
    q = np.where(ph < 50, ph, 100 - ph)
    q2 = np.abs(50 - ph)
    L = q * 1000 - 25000
    R = q2 * 1000 - 25000
    return L, R


TONE_L, TONE_R = _tone_tables()


def _tone_phase(a):
    """a: int array (m, 2) of frames; the phase 0..99 of every frame except the last, using the next frame to tell the two mirror phases
    apart; -1 where the pair is not a step of the triangle"""
    L, R = a[:-1, 0], a[:-1, 1]
    L2, R2 = a[1:, 0], a[1:, 1]
    ok = np.zeros(len(L), dtype=bool)
    ph = np.full(len(L), -1, dtype=np.int64)
    for p in range(100):
        nx = (p + 1) % 100
        m = (L == TONE_L[p]) & (R == TONE_R[p]) & (L2 == TONE_L[nx]) & (R2 == TONE_R[nx]) & ~ok
        ph[m] = p
        ok |= m
    return ph


def tone_check(x, rate=RATE, chunk=1 << 21):
    """FELUCCA_UAC_TONE=1: frame n of the stream is the triangle's phase n % 100 (usb.c). Skips the leading all-zero frames (the stream
    starting). A break = a maximal run of frames that do not follow the one before (a lost frame, a repeated frame, a hole of zeros, music).
    Returns dict(frames, start, checked, breaks, bad (frames that do not follow), first_break (frame), break_at (up to 20 frame numbers))."""
    n = x.shape[0]
    start = 0
    step = 1 << 20
    while start < n:
        a = np.asarray(x[start:start + step])
        nz = np.flatnonzero(np.any(a != 0, axis=1))
        if len(nz):
            start += int(nz[0])
            break
        start += len(a)
    else:
        return dict(frames=n, start=n, checked=0, breaks=0, bad=0, first_break=None, break_at=[], rate=rate)
    breaks, at, prev_bad, nbad = 0, [], False, 0
    pos = start
    while pos < n - 1:
        e = min(n, pos + chunk + 2)
        a = np.asarray(x[pos:e]).astype(np.int64)
        if len(a) < 3:
            break
        ph = _tone_phase(a)                         # len(a) - 1 phases
        m = len(ph) - 1                             # pairs (j, j+1) for j in 0 .. m-1
        bad = (ph[:m] < 0) | (ph[1:m + 1] != (ph[:m] + 1) % 100)
        take = min(m, chunk) if e < n else m
        bad = bad[:take]
        if len(bad):
            nbad += int(bad.sum())
            st, en = _runs(bad)
            for b in st:
                if b == 0 and prev_bad:
                    continue
                breaks += 1
                if len(at) < 20:
                    at.append(int(pos + b + 1))
            prev_bad = bool(bad[-1])
        pos += take
        if e >= n:
            break
    return dict(frames=n, start=start, checked=n - start, breaks=breaks, bad=nbad, first_break=at[0] if at else None, break_at=at, rate=rate)


def tone_line(path, t):
    s = f"{path}: {t['rate']} Hz, {t['frames']} frames ({t['frames'] / t['rate']:.1f} s), from frame {t['start']}: {t['breaks']} break(s) ({t['bad']} frames off the triangle)"
    if t["breaks"]:
        s += f", first at frame {t['first_break']} ({t['first_break'] / t['rate']:.3f} s)"
    return s + f"; {t['checked']} frames checked"


# ------------------------------------------------------------------------------------------------ command line
def main(argv=None):
    ap = argparse.ArgumentParser(description="DUELL bench suite stage 2: analysis of recorded sound (see the top of this file for the words)")
    ap.add_argument("files", nargs="*", help="WAV files (16-bit stereo, 44.1 kHz)")
    ap.add_argument("--selfcheck", action="store_true", help="two files of the same test: the numbers must agree")
    ap.add_argument("--tone", action="store_true", help="the files are recordings of a FELUCCA_UAC_TONE build: check the triangle")
    ap.add_argument("--csv", action="store_true", help="print CSV (file + the seven columns + verdict) instead of the table")
    ap.add_argument("--events", default="", help="comma separated times (s from the test's start) of the test's note-ons and note-offs; the first one is the first sound")
    ap.add_argument("--name", default="", help="the test's name for the verdict (SILENCE... may be silent)")
    for k, (d, text) in THRESHOLDS.items():
        ap.add_argument("--" + k.replace("_", "-"), type=type(d), default=None, help=f"{text} (default {d})")
    a = ap.parse_args(argv)
    over = {k: getattr(a, k) for k in THRESHOLDS if getattr(a, k) is not None}
    if not a.files:
        ap.error("give at least one WAV file")
    try:
        if a.tone:
            bad = False
            for p in a.files:
                x, rate = read_wav(p)
                t = tone_check(x, rate)
                print(tone_line(p, t))
                bad |= bool(t["breaks"]) or t["checked"] == 0
            return 1 if bad else 0
        if a.selfcheck:
            if len(a.files) != 2:
                ap.error("--selfcheck takes exactly two files")
            ra, rb = analyze_file(a.files[0], **over), analyze_file(a.files[1], **over)
            ok, rows = selfcheck(ra, rb)
            print(f"selfcheck {a.files[0]}  against  {a.files[1]}")
            print("\n".join(rows))
            print("SELFCHECK " + ("PASS: the two recordings agree" if ok else "FAIL: the same test gave different numbers"))
            return 0 if ok else 1
        out = io.StringIO()
        w = csv.writer(out) if a.csv else None
        if a.csv:
            w.writerow(["file", *COLS, "verdict", "why"])
        else:
            print(HEADER)
        fail = False
        for p in a.files:
            r = analyze_file(p, events=[float(t) for t in a.events.split(",") if t.strip()] or None, **over)
            v, why = audio_verdict(r, a.name or os.path.basename(p))
            fail |= v == "FAIL"
            if a.csv:
                c = fmt_cells(r)
                w.writerow([p, *[c[k] for k in COLS], v, why])
            else:
                print(line(os.path.basename(p), r))
        if a.csv:
            sys.stdout.write(out.getvalue())
        else:
            print("thresholds: " + ", ".join(f"{k}={over.get(k, d[0])}" for k, d in THRESHOLDS.items() if k != "chunk"))
        return 1 if fail else 0
    except WavError as e:
        print(e, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
