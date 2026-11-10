#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Check a recording of the USB audio input of a FELUCCA_UAC_TONE=1 build (src/usb.c): 16-bit stereo WAV,
recorded at the rate the FM-1 was set to (44.1 or 48 kHz; the triangles are made at that rate, so they
must arrive exactly). Frame n of the stream is phase ph = n % 100 of
    L = q * 1000 - 25000,  R = q2 * 1000 - 25000,  q = ph < 50 ? ph : 100 - ph,  q2 = |50 - ph|
so every frame must be the one after the frame before: a lost frame or a repeated one shows as a break.
The leading silence (the stream starting) and the end of the file are skipped.
  tools/uac_tone_check.py REC.wav [more.wav ...]     exit 1 if any file has a break"""
import sys
import wave
from array import array


def tri(ph):
    q = ph if ph < 50 else 100 - ph
    return q * 1000 - 25000, abs(50 - ph) * 1000 - 25000


TRI = [tri(p) for p in range(100)]


def check(path):
    with wave.open(path, "rb") as w:
        ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if ch != 2 or width != 2:
        print(f"{path}: {ch} ch, {8 * width} bit: need a 16-bit stereo recording")
        return False
    a = array("h", raw)
    if sys.byteorder == "big":
        a.byteswap()
    fr = [(a[2 * i], a[2 * i + 1]) for i in range(len(a) // 2)]
    i = 0
    while i < len(fr) and fr[i] == (0, 0):
        i += 1
    start, breaks, first = i, [], None
    ph = None
    while i < len(fr):
        if ph is None:                                   # (re)sync: the phase that fits this frame and the next
            cand = [p for p in range(100) if TRI[p] == fr[i] and i + 1 < len(fr) and TRI[(p + 1) % 100] == fr[i + 1]]
            if not cand:
                i += 1
                continue
            ph = cand[0]
        elif TRI[ph] != fr[i]:
            breaks.append(i)
            if first is None:
                first = i
            ph = None
            continue
        ph = (ph + 1) % 100
        i += 1
    good = len(fr) - start
    print(f"{path}: {rate} Hz, {len(fr)} frames ({len(fr) / rate:.1f} s), from frame {start}: "
          f"{len(breaks)} break(s)" + (f", first at frame {first} ({first / rate:.3f} s)" if breaks else "")
          + (f"; {good} frames checked" if good else ""))
    return not breaks and good > 0


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    ok = all([check(p) for p in sys.argv[1:]])
    sys.exit(0 if ok else 1)
