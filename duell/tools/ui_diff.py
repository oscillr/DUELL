#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Pixel test of the firmware UI against the FINAL design pictures (maintainer tool, numbers only).

  tests/run: build tests/ui_render.c, run  ui_render --target build/ui_target ; python3 tests/ui_render.py --target build/ui_target
  python3 tools/ui_diff.py [--targets DIR] [--renders DIR] [--tol N] [--grow N]

For each of the ten pictures NN_name (FINAL_V1_NN_name.png in DIR, default DUELL/working docs/ui_redesign) it compares the firmware
render build/ui_target/NN_name.png with it, pixel by pixel (a pixel differs when the summed RGB distance is above --tol, default 24),
and writes
  build/ui_target/compare/report.txt     the table: differing pixels in %, regions (differing pixels grown by --grow px, default 2, then
                                         connected) with bounding box, size, zone (HEADER rows 0..22, BODY, FOOTER rows 218..239) and
                                         the dominant change (colour only / shape or text / missing in render / extra in render)
  build/ui_target/compare/NN_name.png    target left, render right, 1x, nothing drawn on them (240 + 240 wide)
Exit 0 always (it measures; the maintainer judges). Needs Pillow and numpy."""
import argparse
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
DEF_T = ROOT.parent.parent.parent / "DUELL" / "working docs" / "ui_redesign"
NAMES = ["01_morph", "02_engine", "03_patches", "04_fx-send", "05_env", "06_lfo", "07_sequencer", "07b_sequencer_help", "08_setup", "09_mixer", "10_battery"]
H_END, F_START = 22, 218


def zone(y0, y1):
    z = set()
    if y0 <= H_END:
        z.add("HEADER")
    if y1 >= F_START:
        z.add("FOOTER")
    if y1 > H_END and y0 < F_START:
        z.add("BODY")
    return "+".join(s for s in ("HEADER", "BODY", "FOOTER") if s in z)


def grow(m, n):
    out = m.copy()
    for _ in range(n):
        o = out.copy()
        o[1:] |= out[:-1]
        o[:-1] |= out[1:]
        o[:, 1:] |= out[:, :-1]
        o[:, :-1] |= out[:, 1:]
        out = o
    return out


def label(m):
    """4-connected components of a boolean mask: (label image, count); iterative flood fill"""
    lab = np.zeros(m.shape, np.int32)
    n = 0
    h, w = m.shape
    for y in range(h):
        for x in range(w):
            if m[y, x] and not lab[y, x]:
                n += 1
                st = [(y, x)]
                lab[y, x] = n
                while st:
                    cy, cx = st.pop()
                    for ny, nx in ((cy - 1, cx), (cy + 1, cx), (cy, cx - 1), (cy, cx + 1)):
                        if 0 <= ny < h and 0 <= nx < w and m[ny, nx] and not lab[ny, nx]:
                            lab[ny, nx] = n
                            st.append((ny, nx))
    return lab, n


def classify(t, r, raw):
    """what differs inside one region (t, r: the region's pixels, raw: the differing mask)"""
    bg = np.array([8, 0, 8])
    tink = (np.abs(t - bg).sum(axis=2) > 40)
    rink = (np.abs(r - bg).sum(axis=2) > 40)
    nt, nr = int((tink & raw).sum()), int((rink & raw).sum())
    d = int(raw.sum())
    both = int((tink & rink & raw).sum())
    if nr <= 0.15 * d and nt > 0.5 * d:
        return "missing in render (target draws, firmware does not)"
    if nt <= 0.15 * d and nr > 0.5 * d:
        return "extra in render (firmware draws, target does not)"
    if both > 0.7 * d:
        # same places inked: shape the same? compare ink masks
        if int((tink != rink).sum()) < 0.15 * d:
            return "colour only (same shapes, other colour)"
        return "shape or text differs (both ink here)"
    return "shape, size or position differs"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--targets", default=str(DEF_T))
    ap.add_argument("--renders", default=str(ROOT / "build" / "ui_target"))
    ap.add_argument("--tol", type=int, default=24)
    ap.add_argument("--grow", type=int, default=2)
    a = ap.parse_args()
    tdir, rdir = Path(a.targets), Path(a.renders)
    cdir = rdir / "compare"
    cdir.mkdir(parents=True, exist_ok=True)
    lines = ["UI PIXEL TEST: firmware render vs FINAL design pictures (tol %d summed RGB, regions grown %d px)" % (a.tol, a.grow), ""]
    lines.append("%-14s %8s %8s  %s" % ("page", "diff px", "diff %", "regions (count)  zones"))
    detail = []
    for nm in NAMES:
        tp, rp = tdir / ("FINAL_V1_%s.png" % nm), rdir / ("%s.png" % nm)
        if not tp.exists() or not rp.exists():
            lines.append("%-14s missing %s" % (nm, tp if not tp.exists() else rp))
            continue
        t = np.array(Image.open(tp).convert("RGB")).astype(int)
        r = np.array(Image.open(rp).convert("RGB")).astype(int)
        raw = np.abs(t - r).sum(axis=2) > a.tol
        n = int(raw.sum())
        pct = 100.0 * n / raw.size
        lab, cnt = label(grow(raw, a.grow))
        regs = []
        for i in range(1, cnt + 1):
            ys, xs = np.nonzero(lab == i)
            x0, x1, y0, y1 = int(xs.min()), int(xs.max()), int(ys.min()), int(ys.max())
            sub = raw[y0:y1 + 1, x0:x1 + 1] & (lab[y0:y1 + 1, x0:x1 + 1] == i)
            dpx = int(sub.sum())
            regs.append((dpx, (x0, y0, x1, y1), zone(y0, y1), classify(t[y0:y1 + 1, x0:x1 + 1], r[y0:y1 + 1, x0:x1 + 1], sub)))
        regs.sort(reverse=True)
        zc = {}
        for dpx, _, z, _ in regs:
            zc[z] = zc.get(z, 0) + dpx
        lines.append("%-14s %8d %7.2f%%  %d regions  %s" % (nm, n, pct, cnt, ", ".join("%s %d px" % kv for kv in sorted(zc.items(), key=lambda kv: -kv[1]))))
        detail.append("")
        detail.append("%s  %.2f%% (%d px), %d regions" % (nm, pct, n, cnt))
        for dpx, bb, z, c in regs[:10]:
            detail.append("   %5d px  box x%d..%d y%d..%d  %-12s %s" % (dpx, bb[0], bb[2], bb[1], bb[3], z, c))
        if len(regs) > 10:
            detail.append("   ... %d more regions" % (len(regs) - 10))
        sheet = Image.new("RGB", (480, 240))
        sheet.paste(Image.open(tp).convert("RGB"), (0, 0))
        sheet.paste(Image.open(rp).convert("RGB"), (240, 0))
        sheet.save(cdir / ("%s.png" % nm))
    lines += detail
    txt = "\n".join(lines) + "\n"
    (cdir / "report.txt").write_text(txt)
    print(txt)


if __name__ == "__main__":
    sys.exit(main())
