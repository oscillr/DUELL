#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The splash lockup (run by tools/build.py generate(), gen_only.py).

  gen_splash_logo.py OUT.h [--png PREVIEW.png] [--svg assets/splash_logo.svg]

assets/splash_logo.svg (the art director's lockup, viewBox 480 x 480 = the 240 x 240 screen at 2x, all white on transparent)
is the source of truth: DUELL as an embedded PNG, the tagline and 'by OSCILL/R' as vector outlines. Here it is rasterised to
a 240 x 240 coverage mask at 1x: the vector paths directly at the target scale (beziers/arcs flattened, filled even-odd at
8x8 supersampling, box-filtered down), the embedded PNG (alpha, or luminance x alpha) resampled by 0.5 on the same
supersampled grid (Lanczos up, box down). The mask is cropped to its ink box, quantised to 4 bits (0 .. 15 -> x 17) and
stored as a byte-run list: byte = value << 4 | (run - 1), runs of 1 .. 16 pixels, row after row from the top-left of the box.
Only small Pillow is needed (no SVG library). The firmware (ui_draw.c splash_frame) blends T_TEXT by the coverage.
"""
import argparse, base64, io, math, re, sys
import xml.etree.ElementTree as ET
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw

SS = 8                                   # supersampling per axis, at 1x
DEFAULT_SVG = Path(__file__).resolve().parent.parent / "assets" / "splash_logo.svg"


def _tokens(d):
    for m in re.finditer(r"([MmLlHhVvCcSsQqTtAaZz])|(-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)", d):
        yield m.group(1) or float(m.group(2))


def _arc(x0, y0, rx, ry, phi, fa, fs, x, y, out):
    if rx == 0 or ry == 0:
        out.append((x, y)); return
    rx, ry = abs(rx), abs(ry)
    c, s = math.cos(math.radians(phi)), math.sin(math.radians(phi))
    dx, dy = (x0 - x) / 2, (y0 - y) / 2
    x1, y1 = c * dx + s * dy, -s * dx + c * dy
    lam = x1 * x1 / (rx * rx) + y1 * y1 / (ry * ry)
    if lam > 1:
        rx *= math.sqrt(lam); ry *= math.sqrt(lam)
    num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1
    den = rx * rx * y1 * y1 + ry * ry * x1 * x1
    k = math.sqrt(max(0, num / den)) * (-1 if fa == fs else 1)
    cx1, cy1 = k * rx * y1 / ry, -k * ry * x1 / rx
    cx, cy = c * cx1 - s * cy1 + (x0 + x) / 2, s * cx1 + c * cy1 + (y0 + y) / 2
    ang = lambda ux, uy, vx, vy: math.atan2(ux * vy - uy * vx, ux * vx + uy * vy)
    t1 = ang(1, 0, (x1 - cx1) / rx, (y1 - cy1) / ry)
    dt = ang((x1 - cx1) / rx, (y1 - cy1) / ry, (-x1 - cx1) / rx, (-y1 - cy1) / ry)
    if not fs and dt > 0: dt -= 2 * math.pi
    if fs and dt < 0: dt += 2 * math.pi
    n = max(4, int(abs(dt) * max(rx, ry) / 0.25))
    for i in range(1, n + 1):
        t = t1 + dt * i / n
        ex, ey = rx * math.cos(t), ry * math.sin(t)
        out.append((c * ex - s * ey + cx, s * ex + c * ey + cy))


def flatten(d, tol_n=16):
    """path data -> list of closed polygons [(x, y), ...] in user units"""
    tk = list(_tokens(d))
    subs, cur = [], []
    x = y = sx = sy = 0.0
    cmd = None; lastc = None; lastq = None
    pos = 0
    def nxt():
        nonlocal pos
        v = tk[pos]; pos += 1; return v
    while pos < len(tk):
        if isinstance(tk[pos], str):
            cmd = tk[pos]; pos += 1
        rel = cmd.islower(); C = cmd.upper()
        if C == "Z":
            if cur: subs.append(cur)
            cur = []; x, y = sx, sy; lastc = lastq = None; continue
        ox, oy = (x, y) if rel else (0.0, 0.0)
        pc = pq = None
        if C == "M":
            if cur: subs.append(cur)
            x, y = ox + nxt(), oy + nxt(); sx, sy = x, y; cur = [(x, y)]
            cmd = "l" if rel else "L"
        elif C == "L":
            x, y = ox + nxt(), oy + nxt(); cur.append((x, y))
        elif C == "H":
            x = (x if rel else 0.0) + nxt(); cur.append((x, y))
        elif C == "V":
            y = (y if rel else 0.0) + nxt(); cur.append((x, y))
        elif C in "CS":
            if C == "C":
                x1, y1 = ox + nxt(), oy + nxt()
            else:
                x1, y1 = (2 * x - lastc[0], 2 * y - lastc[1]) if lastc else (x, y)
            x2, y2 = ox + nxt(), oy + nxt(); x3, y3 = ox + nxt(), oy + nxt()
            for k in range(1, tol_n + 1):
                t = k / tol_n; u = 1 - t
                cur.append((u**3 * x + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t**3 * x3,
                            u**3 * y + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t**3 * y3))
            pc = (x2, y2); x, y = x3, y3
        elif C in "QT":
            if C == "Q":
                x1, y1 = ox + nxt(), oy + nxt()
            else:
                x1, y1 = (2 * x - lastq[0], 2 * y - lastq[1]) if lastq else (x, y)
            x2, y2 = ox + nxt(), oy + nxt()
            for k in range(1, tol_n + 1):
                t = k / tol_n; u = 1 - t
                cur.append((u * u * x + 2 * u * t * x1 + t * t * x2, u * u * y + 2 * u * t * y1 + t * t * y2))
            pq = (x1, y1); x, y = x2, y2
        elif C == "A":
            rx, ry, phi, fa, fs = nxt(), nxt(), nxt(), int(nxt()), int(nxt())
            ex, ey = ox + nxt(), oy + nxt()
            _arc(x, y, rx, ry, phi, fa, fs, ex, ey, cur); x, y = ex, ey
        lastc, lastq = pc, pq
    if cur: subs.append(cur)
    return subs


def area(p):
    return sum(p[i][0] * p[(i + 1) % len(p)][1] - p[(i + 1) % len(p)][0] * p[i][1] for i in range(len(p))) / 2


def render(svg_path, scale=0.5, ss=SS):
    """-> 240 x 240 'L' coverage image"""
    root = ET.parse(svg_path).getroot()
    vb = [float(v) for v in root.get("viewBox").split()]
    W, H = round(vb[2] * scale), round(vb[3] * scale)
    k = scale * ss                                   # user unit -> supersampled pixel
    acc = Image.new("L", (W * ss, H * ss), 0)
    n_paths = n_img = 0
    for el in root.iter():
        tag = el.tag.split("}")[-1]
        if tag == "path":
            subs = flatten(el.get("d"))
            m = Image.new("1", acc.size, 0)
            for sub in subs:                          # even-odd by XOR of subpaths (holes wind the other way: checked)
                if len(sub) < 3: continue
                one = Image.new("1", acc.size, 0)
                ImageDraw.Draw(one).polygon([(px * k, py * k) for px, py in sub], fill=1)
                m = ImageChops.logical_xor(m, one)
            acc = ImageChops.lighter(acc, m.convert("L"))
            if sum(1 for s in subs if area(s) > 0) and sum(1 for s in subs if area(s) < 0) == 0 and len(subs) > 1:
                sys.exit("path with several subpaths of one winding: even-odd vs nonzero would differ")
            n_paths += 1
        elif tag == "image":
            href = el.get("{http://www.w3.org/1999/xlink}href") or el.get("href")
            im = Image.open(io.BytesIO(base64.b64decode(href.split(",", 1)[1])))
            im = im.convert("RGBA")
            r, g, b, a = im.split()
            lum = Image.merge("RGB", (r, g, b)).convert("L")
            cov = ImageChops.multiply(lum, a)          # luminance x alpha (white art: = alpha)
            t = el.get("transform"); tx = ty = 0.0
            if t:
                mm = re.match(r"translate\(\s*([-\d.]+)[ ,]+([-\d.]+)\s*\)", t); tx, ty = float(mm.group(1)), float(mm.group(2))
            w, h = float(el.get("width")), float(el.get("height"))
            sw, sh = round(w * k), round(h * k)
            big = cov.resize((sw, sh), Image.LANCZOS)
            lay = Image.new("L", acc.size, 0)
            lay.paste(big, (round(tx * k), round(ty * k)))
            acc = ImageChops.lighter(acc, lay)
            n_img += 1
    out = acc.resize((W, H), Image.BOX)
    return out, n_paths, n_img


def encode(vals):
    out = bytearray(); i = 0
    while i < len(vals):
        v = vals[i]; n = 1
        while i + n < len(vals) and vals[i + n] == v and n < 16: n += 1
        out.append(v << 4 | (n - 1)); i += n
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out"); ap.add_argument("--png"); ap.add_argument("--svg", default=str(DEFAULT_SVG))
    a = ap.parse_args()
    img, npath, nimg = render(a.svg)
    q = img.point(lambda v: (v * 15 + 127) // 255)
    box = q.getbbox()
    crop = q.crop(box)
    x0, y0 = box[0], box[1]; w, h = crop.size
    data = encode(list(crop.tobytes()))
    if a.png:
        crop.point(lambda v: v * 17).save(a.png)
    with open(a.out, "w") as f:
        f.write("/* generated by tools/gen_splash_logo.py from assets/splash_logo.svg: do not edit */\n")
        f.write(f"/* the splash lockup, 4-bit coverage run list: byte = value << 4 | (run - 1), runs of 1 .. 16 pixels, {w} x {h} from ({x0}, {y0}) */\n")
        f.write(f"#define SPL_LOGO_X0 {x0}\n#define SPL_LOGO_Y0 {y0}\n#define SPL_LOGO_W {w}\n#define SPL_LOGO_H {h}\n")
        f.write(f"static const uint8_t SPL_LOGO[{len(data)}] = {{\n")
        for i in range(0, len(data), 24):
            f.write("    " + ",".join(f"0x{b:02X}" for b in data[i:i + 24]) + ",\n")
        f.write("};\n")
    print(f"gen_splash_logo: {npath} paths + {nimg} image -> {w}x{h} at ({x0},{y0}), {len(data)} bytes")


if __name__ == "__main__":
    main()
