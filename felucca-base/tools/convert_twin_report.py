#!/usr/bin/env python3
"""Compare two sets of renders (the old build's and the new build's) for the TWIN conversion proof.

  convert_twin_report.py OLD_DIR NEW_DIR [--names FILE] [--md OUT]

Both directories hold the dumps of tests/sound_audit.c (SOUND_DUMP, E<engine>_<index>.i32) or of
tests/multi_render_test.c (MULTI_DUMP, <name>_f<fader>.i32): int32 left / right pairs, the mix before MASTER, 48 kHz.
Columns per file: peak dB, RMS dB (new minus old), spectral centroid shift (Hz), the largest and the mean 1/3-octave band
difference (dB) over the bands within 40 dB of the peak band (of either render). Outliers: RMS > 1.5 dB or a band > 3 dB.
Nothing is tuned here: this only measures."""
import argparse
import os
import sys

import numpy as np

FS = 48000
BANDS = [1000.0 * 2 ** (k / 3.0) for k in range(-15, 13)]       # 31.5 Hz .. 16 kHz centres


def load(path):
    a = np.fromfile(path, dtype="<i4").astype(np.float64)
    a = a[: len(a) // 2 * 2].reshape(-1, 2)
    return a


def spectrum(x):
    n = 8192
    w = np.hanning(n)
    if len(x) < n:
        x = np.pad(x, (0, n - len(x)))
    acc = np.zeros(n // 2 + 1)
    c = 0
    for i in range(0, len(x) - n + 1, n // 2):
        acc += np.abs(np.fft.rfft(x[i:i + n] * w)) ** 2
        c += 1
    return np.fft.rfftfreq(n, 1.0 / FS), acc / max(c, 1)


def band_levels(f, p):
    out = []
    for fc in BANDS:
        lo, hi = fc / 2 ** (1 / 6.0), fc * 2 ** (1 / 6.0)
        m = (f >= lo) & (f < hi)
        out.append(10 * np.log10(p[m].sum() + 1e-30) if m.any() else -300.0)
    return np.array(out)


def measure(a):
    mono = a.mean(axis=1)
    pk = np.abs(a).max()
    rms = np.sqrt((a ** 2).mean())
    f, p = spectrum(mono)
    cen = (f * p).sum() / p.sum() if p.sum() > 0 else 0.0
    return dict(peak=20 * np.log10(pk / 32768.0 + 1e-12), rms=20 * np.log10(rms / 32768.0 + 1e-12), cen=cen, bands=band_levels(f, p))


def compare(po, pn):
    a, b = measure(load(po)), measure(load(pn))
    top = max(a["bands"].max(), b["bands"].max())
    m = (a["bands"] > top - 40) | (b["bands"] > top - 40)
    d = np.abs(b["bands"] - a["bands"])[m] if m.any() else np.zeros(1)
    return dict(dpeak=b["peak"] - a["peak"], drms=b["rms"] - a["rms"], dcen=b["cen"] - a["cen"], bmax=d.max(), bmean=d.mean(),
                peak=b["peak"], rms=b["rms"])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("old")
    ap.add_argument("new")
    ap.add_argument("--names")
    ap.add_argument("--md")
    ap.add_argument("--only", help="a text file with one dump name per line (the rest is skipped)")
    ap.add_argument("--title", default="renders")
    o = ap.parse_args()
    names = {}
    if o.names:
        for ln in open(o.names):
            k, _, v = ln.rstrip("\n").partition("\t")
            names[k] = v
    keep = None
    if o.only:
        keep = set(x.strip() for x in open(o.only) if x.strip())
    rows = []
    for fn in sorted(os.listdir(o.new)):
        if not fn.endswith(".i32") or not os.path.exists(os.path.join(o.old, fn)):
            continue
        key = fn[:-4]
        if keep is not None and key not in keep:
            continue
        r = compare(os.path.join(o.old, fn), os.path.join(o.new, fn))
        r["key"] = key
        r["name"] = names.get(key, key)
        r["out"] = abs(r["drms"]) > 1.5 or r["bmax"] > 3.0
        rows.append(r)
    rows.sort(key=lambda r: (not r["out"], -max(abs(r["drms"]) / 1.5, r["bmax"] / 3.0)))
    lines = ["| render | dPeak dB | dRMS dB | dCentroid Hz | band max dB | band mean dB | flag |", "|---|---:|---:|---:|---:|---:|---|"]
    for r in rows:
        lines.append("| %s | %+.2f | %+.2f | %+.0f | %.2f | %.2f | %s |" % (r["name"], r["dpeak"], r["drms"], r["dcen"], r["bmax"], r["bmean"], "OUTLIER" if r["out"] else ""))
    txt = "\n".join(lines)
    n_out = sum(r["out"] for r in rows)
    head = "%s: %d compared, %d outliers (RMS > 1.5 dB or a band > 3 dB)\n" % (o.title, len(rows), n_out)
    if o.md:
        open(o.md, "w").write(head + "\n" + txt + "\n")
    print(head + txt)
    return 0


if __name__ == "__main__":
    sys.exit(main())
