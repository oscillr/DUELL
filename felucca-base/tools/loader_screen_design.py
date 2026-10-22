#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL 1.0: the DESIGN picture of the screen shown while the firmware installs (nothing in the firmware or the loader uses this; release_drafts/LOADER_SCREEN_DESIGN.md).

    tools/loader_screen_design.py OUT.png [--states 0,45,100]      (needs Pillow: DUELL/.venv/bin/python)

The panel is 240 x 240, RGB565. The picture is made from the device's own pieces: the VIOLET palette (build/gen/ui_palettes.h), the splash's morphing wave lines
(a port of ui_draw.c splash_frame: the same ribbon, the part 1 colour at the top to the part 2 colour at the bottom, T_BG blended in 18 %), the DUELL word of the splash lockup
(build/gen/splash_logo.h, rows 0..33 of the 166 x 124 lockup, blended in T_TEXT over the lines like the splash does) and the UI's own typeface Inter Tight
(assets/fonts/InterTight[wght].ttf: M 15 px at weight 500 for WRITING and the percent, M 15 px at weight 500 for the warning line too, in T_TEXT white; the sizes of tools/gen_aa_font.py).
One 240 x 240 screen per state side by side, 8 px apart; OUT.png at 1x and OUT_3x.png enlarged 3x without smoothing."""
import argparse
import math
import re
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
FONT = str(ROOT / "assets" / "fonts" / "InterTight[wght].ttf")
SINE = [round(32767 * math.sin(2 * math.pi * i / 1024)) for i in range(1024)]
SPL_N = 15


def sine_i(ph):
    ph &= 0xFFFFFFFF
    i = ph >> 22
    a, b = SINE[i], SINE[(i + 1) & 1023]
    return a + (((b - a) * ((ph >> 7) & 0x7FFF)) >> 15)


def rgb(v):
    return ((v >> 11 & 31) * 255 // 31, (v >> 5 & 63) * 255 // 63, (v & 31) * 255 // 31)


def mix(a, b, pct):
    return tuple(a[k] + ((b[k] - a[k]) * pct + (50 if b[k] >= a[k] else -50)) // 100 for k in range(3))


def palette():
    t = (ROOT / "build/gen/ui_palettes.h").read_text()
    m = re.search(r'\{"VIOLET", \{(0x\w+), 0x\w+\}, 0x\w+, \{(0x\w+), (0x\w+)\}, (0x\w+), (0x\w+), \{(0x\w+), (0x\w+)\}, \{(0x\w+), (0x\w+)\}, (0x\w+), \{(0x\w+), (0x\w+), (0x\w+)', t)
    v = [int(x, 16) for x in m.groups()]
    return dict(bg=rgb(v[0]), surf=rgb(v[1]), text=rgb(v[3]), mid=rgb(v[4]), line=rgb(v[5]), raise_=rgb(v[7]), rec=rgb(v[9]), c0=rgb(v[10]), c2=rgb(v[12]))


def logo():
    t = (ROOT / "build/gen/splash_logo.h").read_text()
    w = int(re.search(r"SPL_LOGO_W (\d+)", t).group(1))
    body = re.search(r"SPL_LOGO\[\d+\] = \{(.*?)\};", t, re.S).group(1)
    cov = []
    for b in (int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]{2}", body)):
        cov += [(b >> 4) * 100 // 15] * ((b & 15) + 1)
    return w, 34, cov[:w * 34]                  # (the DUELL word: rows 0..33; the tagline below it is left out)


def line(img, x0, y0, x1, y1, c):
    dx, sx = abs(x1 - x0), 1 if x0 < x1 else -1
    dy, sy = -abs(y1 - y0), 1 if y0 < y1 else -1
    err = dx + dy
    for _ in range(2000):
        if 0 <= x0 < 240 and 0 <= y0 < 240:
            img.putpixel((x0, y0), c)
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


def waves(img, P, st):
    """ui_draw.c splash_frame, the lines: SPL_N lines of a ribbon, step st of the animation"""
    m = (st * 0x00800000) & 0xFFFFFFFF
    col = [mix(mix(P["c0"], P["c2"], j * 100 // (SPL_N - 1)), P["bg"], 18) for j in range(SPL_N)]
    for j in range(SPL_N):
        j2 = j * 2 - (SPL_N - 1)
        pen, px, py = 0, 0, 0
        for x in range(0, 240, 2):
            xt = x * 17895697 // 100
            mc = 120 + ((sine_i(xt * 90 + m) * 22) >> 15)
            w = 170 + ((sine_i(xt * 130 + m * 2 + 0x40000000) * 40) >> 15)
            mo = max(0, min(256, (x - 120) * 256 // 240 + 128 + (sine_i(m) >> 8)))
            calm = sine_i(xt * 100 + j * 0x05000000 + m)
            rip = int((sine_i(xt * 370 + j * 0x0C000000 + m * 3) * 3 + sine_i(xt * 710 - j * 0x1B000000 + m * 5) * 2) / 5)
            y = mc + int(j2 * w / (2 * (SPL_N - 1))) + ((calm * 5 + (rip * mo // 256) * 4) >> 15)
            if pen:
                line(img, px, py, x, y, col[j])
            px, py, pen = x, y, 1


def font(px, wght):
    f = ImageFont.truetype(FONT, px)
    f.set_variation_by_axes([wght])
    return f


def screen(P, lg, pct, st=12):
    img = Image.new("RGB", (240, 240), P["bg"])
    waves(img, P, st)
    w, h, cov = lg
    x0, y0 = 22, 40                                             # the splash's left edge (SPL_LOGO_X0), the word higher up
    for i, a in enumerate(cov):
        if a:
            img.putpixel((x0 + i % w, y0 + i // w), mix(img.getpixel((x0 + i % w, y0 + i // w)), P["text"], a))
    d = ImageDraw.Draw(img)
    fm, fs = font(15, 500), font(12, 400)
    d.text((22, 108), "WRITING", font=fm, fill=P["text"])
    d.text((218, 108), "%d%%" % pct, font=fm, fill=P["text"], anchor="ra")
    n, sw, gp, bx, by, bh = 20, 8, 1, 22, 134, 10                # 20 segments of 5 percent, 196 px wide (8 + 1 gap)
    step = sw + gp + (1 if False else 0)
    wd = (218 - bx + gp) // n                                    # 9.8: spread the 20 segments over 22 .. 218
    for k in range(n):
        xa = bx + k * (218 - bx + gp) // n
        xb = bx + (k + 1) * (218 - bx + gp) // n - gp
        col = mix(P["c0"], P["c2"], k * 100 // (n - 1)) if k < pct // 5 else P["raise_"]
        d.rectangle((xa, by, xb - 1, by + bh - 1), fill=col)
    d.text((22, 176), "DO NOT SWITCH OFF", font=fm, fill=P["text"])
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--states", default="0,45,100")
    a = ap.parse_args()
    P, lg = palette(), logo()
    sts = [int(x) for x in a.states.split(",")]
    sheet = Image.new("RGB", (240 * len(sts) + 8 * (len(sts) - 1), 240), (60, 60, 60))
    for i, p in enumerate(sts):
        sheet.paste(screen(P, lg, p), (i * 248, 0))
    out = Path(a.out)
    sheet.save(out)
    sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST).save(out.with_name(out.stem + "_3x.png"))
    print("wrote", out, sheet.size)


main()
