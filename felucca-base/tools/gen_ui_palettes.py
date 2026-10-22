#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The UI palettes (run by tools/build.py generate()).

  gen_ui_palettes.py OUT.h [--report]

DUELL: six themes (the colour brief: Voltage Purple, Orange Circuit, Arctic Mint, Arctic Blue, Deep Ocean, High Contrast)
and, kept because they are cheap, GREY and MONO. A theme has per part (1, 2) a morph colour pair A, B and a background,
and shared surfaces:
  BG      part 1's / part 2's background (BG1, BG2); LAYER mode: BGL (the mix of both, solid for High Contrast)
  SURF    the panel; RAISE the elevated surface (chips, slots, wells; a light theme whose elevated surface is not
          visibly off the panel uses the borders colour); TEXT; MID secondary text; LINE borders (dividers, tracks)
  c[4]    the brief's colours part1A part1B part2A part2B (fills: the MORPH page's bar)
  r[4]    THEME: the same colours (values, curves, gauges)
  a[4]    ACCENT: the pair swapped (the one active thing: hot knob, cursor, playhead)
  g, ga   0.2.24 (owner): the GLOBAL colour and its accent, a third colour apart from both parts' hues, for the pages that edit the
          instrument and not a part (GLO's pages, SCALE: params.c page_global; src/gfx.c part_theme / theme_follow): values, gauges,
          selections there; the part badges stay lit both. GREY: a gray; MONO: white
The firmware (src/gfx.c part_theme) blends A -> B with the part's MORPH fader (High Contrast: jumps at the middle) and derives
with fixed blends (the same integer maths as mix() here), from the background in use:
  DIM  = mix(BG, TEXT, 42 %)    inactive, empty steps, disabled
  SEL  = mix(BG, THEME, 80 %)   selection fill and gauge fill
  TINT = mix(BG, THEME, 12 %)   a faint area
  KEY  = mix(BG, TEXT, 78 %)    a keycap's fill (its label INK)
  INK  = BG: text on a selection or THEME fill
The brief's colours are used exactly (THEME = the blend of A, B); the contrast checks of the themes are informational
(--report prints the misses, nothing is altered or failed). ACCENT is the other end's colour (the pair swapped).
Where a part colour is hard to read as text the firmware draws that text in the primary text colour (gfx.c vt()).
GREY is pure grayscale; MONO is black and white (set, not blended: src/gfx.c palette_set). Neither morphs.
Saved settings name palettes by a tagged id (PAL_TAG + index); an untagged one (older firmware saved an index into its 20
palettes) is the first theme (firmware/src/gfx.c palette_from_stored; DUELL 0.2.14 has no table for it any more).
--report prints the WCAG contrast of every pairing. The tool fails (exit 1) on any miss.
"""
import argparse
import sys
from pathlib import Path

# DUELL themes (colour brief, owner's Voltage Purple revision): name, bg1, bg2, bglayer (None: mix 50 %), surf (panel),
# elevated, text, secondary, borders, (part1 A, part1 B, part2 A, part2 B), hc (no gradients: the morph colour jumps)
def H(s): return tuple(int(s[i:i + 2], 16) for i in (1, 3, 5))
# Contrast pass: dark themes have a deep ground, near-white text, panels a visible step lighter, strong borders; light themes have strong text and borders
THEMES = [
    ("VIOLET", "#080108", "#0C020E", None, "#0A0414", "#201040", "#FFFFFF", "#F0E0FF", "#9A3FE0",
     ("#DC38FF", "#BC30F0", "#FF1F8E", "#E00B78"), 0, ("#12051A", "#300E3E", "#CC3490")),
    ("ORANGE", "#130904", "#180A04", None, "#241610", "#38241A", "#FFF8F0", "#CDBBA9", "#7A5640",
     ("#FF6A1A", "#E04A12", "#FF9A1F", "#D9690C"), 0),
    ("MINT", "#BDF2D8", "#C2F1EA", None, "#FFFFFF", "#E2F6EC", "#0E2A24", "#3F5F57", "#8FCDB6",
     ("#3DF2B0", "#0A8F5E", "#36E0CF", "#0A8078"), 0),
    ("COBALT", "#06080F", "#080B14", None, "#141B2C", "#1F2A42", "#F2F7FF", "#A9BAD3", "#34445F",
     ("#2F7BFF", "#1647B8", "#4CC3FF", "#1B78B8"), 0),
    ("PALE", "#F7F4EE", "#F4F6F2", None, "#FFFFFF", "#FBF9F5", "#2A2A2E", "#77727A", "#DDD6CA",
     ("#8E9BFF", "#5E6BD8", "#FF9E8A", "#E0705C"), 0),
    ("OCEAN", "#101B3A", "#0B2935", None, "#17263D", "#20344E", "#F0F7FF", "#A4BAD0", "#304762",
     ("#4263EB", "#252F91", "#36D9E8", "#087E9B"), 0),
    ("HI-CON", "#141619", "#151923", "#171A21", "#1D2026", "#272B33", "#F5F7FA", "#A7AFBC", "#3B414D",
     ("#FF2638", "#B80024", "#176BFF", "#1238B8"), 1),
]
# 0.2.24: the GLOBAL colour of each theme (g; its accent ga is g moved toward the text colour, dark themes, or toward black, light ones): apart from both parts' hues
GLOBAL = {"VIOLET": "#FFB84D", "ORANGE": "#3DD6C6", "MINT": "#5B2FB8", "COBALT": "#FFB84D", "PALE": "#0F7A66", "OCEAN": "#FFA040", "HI-CON": "#FFD600",
          "GREY": (200, 200, 200), "MONO": (255, 255, 255)}
# Appended AFTER the legacy rows (GREY, MONO): saved theme ids are indices, so existing ids keep their theme. (None now.)
THEMES_EXTRA = []
# name, bg, surf, text, theme, accent, theme2 (8-bit RGB; stored as RGB565). theme2: DUELL's part 2 colour (SPEC 5a)
LEGACY = [
    ("GREY",   (14, 14, 14),    (36, 36, 36),    (204, 204, 204), (232, 232, 232), (255, 255, 255), (150, 150, 150)),
    ("GREEN",  (6, 18, 10),     (16, 40, 26),    (226, 244, 230), (84, 214, 120),  (255, 214, 92), (190, 236, 64)),
    ("AMBER",  (18, 14, 8),     (42, 32, 18),    (246, 236, 216), (255, 168, 40),  (96, 214, 230), (255, 104, 64)),
    ("ICE",    (8, 16, 24),     (20, 36, 52),    (228, 240, 248), (80, 184, 236),  (255, 140, 100), (64, 224, 200)),
    ("VIOLET", (16, 12, 28),    (36, 28, 60),    (240, 232, 252), (178, 136, 246), (110, 228, 168), (244, 128, 200)),
    ("ROSE",   (24, 10, 18),    (52, 22, 40),    (252, 232, 242), (244, 114, 182), (255, 214, 110), (176, 132, 248)),
    ("PAPER",  (244, 239, 228), (226, 218, 202), (34, 30, 24),    (10, 84, 70),    (172, 56, 8), (132, 50, 28)),
    ("HI-CON", (0, 0, 0),       (40, 40, 40),    (255, 255, 255), (255, 232, 0),   (0, 230, 255), (0, 208, 255)),
    # appended (saved ids are indices: append only). #50, the 0.9 look: true black, green-tinted text and lines
    # (the derived MID / DIM / LINE are tints of TEXT), phosphor-green values, white for the one active thing
    ("NIGHT",  (0, 0, 0),       (0, 24, 10),     (150, 230, 170), (56, 220, 100),  (255, 255, 255), (64, 216, 236)),
    # 1.0.2: black and white (the derived tokens are set, not blended: bw()); the old MONO is GREY (id 0)
    ("MONO",   (0, 0, 0),       (0, 0, 0),       (255, 255, 255), (255, 255, 255), (255, 255, 255), (255, 255, 255)),
]
BW = "MONO"
BW_GREY = (82, 82, 82)             # MONO's one mid grey (RGB565 10/20/10): DIM LINE RAISE LANE
# the 20 palettes of the earlier firmware (index order) -> the new palette
OLD = ["GREEN", "AMBER", "CYAN", "RED", "MONO", "VIOLET", "PINK", "ICE", "WARM", "OCEAN", "DUSK", "HI-CON",
       "LIGHT", "PAPER", "SKY", "MINT", "LILAC", "ROSE", "SAND", "L-HICON"]
OLD_TO_NEW = {}
PAL_TAG = 64                       # stored id = PAL_TAG + index; below 20: an old id
REC_DARK, REC_LIGHT = (255, 72, 72), (190, 24, 40)
QR_LIGHT, QR_DARK, CRASH_BG, CRASH_INK = (255, 255, 255), (0, 0, 0), (160, 0, 0), (255, 255, 255)
PCT = {"MID": 70, "DIM": 42, "LINE": 18, "SEL": 80, "TINT": 12, "KEY": 78}
RAISE_PCT = 12                     # SURF -> TEXT


def to565(c, exact=False):
    r, g, b = c
    if r == g == b and not exact:                     # a gray stays on the RGB565 gray axis (G = 2 R)
        return ((r >> 3) << 11) | ((r >> 3) << 6) | (r >> 3)
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def from565(v):
    return ((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)


def cdiv(a, b):
    return a // b if a >= 0 else -((-a) // b)


def mix(a, b, pct, mono=False):
    """src/gfx.c ux_mix: a + (b - a) * pct / 100 per channel, rounded; GREY, MONO on the 5-bit red channel"""
    if mono:
        x, y = a >> 11, b >> 11
        d = (y - x) * pct
        v = x + cdiv(d + (50 if d >= 0 else -50), 100)
        return (v << 11) | (v << 6) | v
    out = 0
    for sh, mask in ((11, 31), (5, 63), (0, 31)):
        x, y = (a >> sh) & mask, (b >> sh) & mask
        d = (y - x) * pct
        out |= (x + cdiv(d + (50 if d >= 0 else -50), 100)) << sh
    return out


def luma(c):
    """src/gfx.c ux_luma: 0..255, integer"""
    r, g, b = c >> 11, (c >> 5) & 63, c & 31
    return (r * 255 // 31 * 54 + g * 255 // 63 * 183 + b * 255 // 31 * 19) >> 8


def lum(c565):
    def lin(v):
        v /= 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = from565(c565)
    return 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b)


def contrast(a, b):
    la, lb = lum(a), lum(b)
    return (max(la, lb) + 0.05) / (min(la, lb) + 0.05)


ACC_PCT = 30
VAL_MIN = 3.0


def fit(col, toward, against, lim):
    """col moved toward `toward` in 2 % steps until it reaches `lim` contrast on every colour of `against` (565 values)"""
    for pct in range(0, 101, 2):
        c = mix(col, toward, pct)
        if all(contrast(c, g) >= lim for g in against):
            return c
    return toward


def shift(c565, delta):
    """c565 + delta (per 8-bit channel, clamped), back to RGB565"""
    r, g, b = from565(c565)
    return to565(tuple(max(0, min(255, v + d)) for v, d in zip((r, g, b), delta)))


def build_theme(t):
    name, b1, b2, bl, surf, elev, text, mid, line, cols, hc, *extra = t
    bg1, bg2, surf, elev, text, mid, line = (to565(H(x)) for x in (b1, b2, surf, elev, text, mid, line))
    bgl = to565(H(bl)) if bl else mix(bg1, bg2, 50)
    bgs = [bg1, bg2, bgl]
    dep = []
    light = lum(bg1) > 0.2
    # panel, elevated surface and borders are per part (src/gfx.c part_theme swaps them like the background): part 2's are
    # explicit (extra) or part 1's moved by the same per-channel offset as the backgrounds (a light theme: the panel stays,
    # elevated and borders move half as far); LAYER mode mixes the two parts' values 50 %
    if extra:
        surf2, elev2, line2 = (to565(H(x)) for x in extra[0])
    else:
        delta = tuple(y - x for x, y in zip(from565(bg1), from565(bg2)))
        if light:
            half = tuple(d // 2 for d in delta)
            surf2, elev2, line2 = surf, shift(elev, half), shift(line, half)
        else:
            surf2, elev2, line2 = shift(surf, delta), shift(elev, delta), shift(line, delta)
    surfs, elevs, lines0 = [surf, surf2], [elev, elev2], [line, line2]
    lines, raises = [], []
    for k in range(2):
        l = fit(lines0[k], text, [bgs[k]], 1.3)
        if l != lines0[k]: dep.append(f"part {k + 1} borders darkened for dividers")
        lines.append(l)
        r_ = elevs[k] if contrast(elevs[k], surfs[k]) >= 1.15 else l
        if r_ != elevs[k]: dep.append(f"part {k + 1} RAISE = borders (elevated surface too close to the panel)")
        raises.append(r_)
    c = [to565(H(x)) for x in cols]
    r = list(c)
    a = [c[1], c[0], c[3], c[2]]
    rec = fit(to565(REC_LIGHT if light else REC_DARK), 0x0000 if light else 0xFFFF, bgs, 4.5)
    g = to565(H(GLOBAL[name]))
    ga = mix(g, 0x0000 if light else text, ACC_PCT)
    return dict(name=name, bg=[bg1, bg2], bgl=bgl, surf=surfs, text=text, mid=mid, line=lines, raise_=raises,
                c=c, r=r, a=a, g=g, ga=ga, rec=rec, hc=hc, fixed=False, dep=dep)


def build_legacy(p):
    name, bg, surf, text, theme, accent, theme2 = p
    mono = name == "GREY"
    bgv, sf, tx, th, ac, t2 = (to565(x, name == BW) for x in (bg, surf, text, theme, accent, theme2))
    g = to565(GLOBAL[name], name == BW)
    d = dict(name=name, bg=[bgv, bgv], bgl=bgv, surf=[sf, sf], text=tx, mid=mix(bgv, tx, PCT["MID"], mono),
             line=[mix(bgv, tx, PCT["LINE"], mono)] * 2, raise_=[mix(sf, tx, RAISE_PCT, mono)] * 2, c=[th, th, t2, t2],
             r=[th, th, t2, t2], a=[ac] * 4, g=g, ga=ac, rec=ac if mono or name == BW else to565(REC_LIGHT if luma(bgv) > 128 else REC_DARK), hc=0, fixed=True, dep=[], mono=mono)
    if name == BW:
        grey = to565(BW_GREY)
        d.update(mid=tx, line=[grey] * 2, raise_=[grey] * 2)
    return d


def tokens(d, bg, k, e):
    """the tokens for part k at its end e (0 A, 1 B) on background bg (what src/gfx.c part_theme derives)"""
    mono = d.get("mono", False)
    th, ac = d["r"][2 * k + e], d["a"][2 * k + e]
    t = dict(bg=bg, surf=d["surf"][k], text=d["text"], mid=d["mid"], line=d["line"][k], raise_=d["raise_"][k], theme=th, accent=ac,
             dim=mix(bg, d["text"], PCT["DIM"], mono), sel=mix(bg, th, PCT["SEL"], mono), key=mix(bg, d["text"], PCT["KEY"], mono),
             ink=bg)
    if not d["fixed"] and contrast(d["text"], th) > contrast(bg, th):   # src/gfx.c: the better of BG, TEXT on the part colour
        t["ink"] = d["text"]
    t["rec"] = d["rec"]
    if d["name"] == BW:
        t.update(sel=th, key=d["text"], dim=d["line"][k])
    return t


CHECKS = [("text", "bg", 9.0, "body text"), ("text", "surf", 9.0, "body text on a surface"),
          ("mid", "bg", 5.0, "labels"), ("mid", "surf", 4.0, "labels on a surface"),
          ("dim", "bg", 2.2, "inactive (not read)"), ("line", "bg", 1.25, "1 px dividers"),
          ("theme", "bg", VAL_MIN, "values, curves"), ("theme", "surf", VAL_MIN, "values on a surface"),
          ("accent", "bg", 4.5, "hot value, cursor, playhead"), ("accent", "surf", 4.5, "hot on a surface"),
          ("ink", "sel", VAL_MIN, "text on a selection"), ("ink", "theme", VAL_MIN, "text on a THEME fill"),
          ("sel", "bg", 1.8, "gauge fill vs track"), ("rec", "bg", 3.0, "REC (graphic)"),
          ("text", "raise_", 7.0, "text on a RAISE chip or button"), ("ink", "rec", 4.5, "text on the REC chip"),
          ("ink", "key", 4.5, "a keycap's label"), ("key", "bg", 3.0, "a keycap on the background"),
          ("key", "surf", 2.5, "a keycap on a surface"), ("ink", "dim", 1.8, "an unavailable keycap's label"),
          ("ink", "accent", 4.5, "text on the ARM badge")]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?")
    ap.add_argument("--report", action="store_true")
    a = ap.parse_args()
    fails = []
    rows = [build_theme(t) for t in THEMES] + [build_legacy(p) for p in LEGACY if p[0] in ("GREY", BW)] + [build_theme(t) for t in THEMES_EXTRA]
    assert len(rows) <= 63
    for d in rows:
        name = d["name"]
        if name == "GREY":
            for k, v in d.items():
                if k in ("c", "r", "a", "bg", "surf", "line", "raise_"):
                    vs = v
                elif isinstance(v, int) and not isinstance(v, bool):
                    vs = [v]
                else:
                    continue
                for x in vs:
                    if not (x >> 11 == x & 31 and ((x >> 5) & 63) == (x >> 11) << 1):
                        fails.append(f"{name} is not grayscale: {k} = {x:#06x}")
        if name == BW:
            for k in ("bg", "bgl", "surf", "text", "mid", "line", "raise_"):
                for v in (d[k] if isinstance(d[k], list) else [d[k]]):
                    if v not in (0x0000, 0xFFFF) and not (v == to565(BW_GREY) and k in ("line", "raise_")):
                        fails.append(f"{name} is not black and white: {k} = {v:#06x}")
        worst = {}
        seen = set()
        for bgn, bg in (("bg1", d["bg"][0]), ("bg2", d["bg"][1]), ("layer", d["bgl"])):
            for k in range(2):
                for e in range(2):
                    if d["hc"] == 0 and False: pass
                    t = tokens(d, bg, k, e)
                    for f, b, lim, why in CHECKS:
                        c = contrast(t[f], t[b])
                        key = (f, b)
                        worst[key] = min(worst.get(key, 99), c)
                        if c < lim and not d["fixed"]:
                            d["dep"].append(f"info {bgn} part{k + 1}{'AB'[e]}: {f}/{b} {c:.2f} < {lim} ({why})") if (f, b, bgn, k, e) not in seen else None
                            seen.add((f, b, bgn, k, e))
                        elif c < lim:
                            fails.append(f"{name} {bgn} part{k + 1}{'AB'[e]}: {f}/{b} {c:.2f} < {lim} ({why})")
        d["worst"] = worst
        for bgn, bg in (("bg1", d["bg"][0]), ("bg2", d["bg"][1]), ("layer", d["bgl"])):   # 0.2.24: the GLOBAL colour reads as a value on every background and surface, its accent as a hot value
            for sfn, sf in (("bg", bg), ("surf1", d["surf"][0]), ("surf2", d["surf"][1])):
                if contrast(d["g"], sf) < VAL_MIN:
                    fails.append(f"{name} {bgn} {sfn}: global/{sfn} {contrast(d['g'], sf):.2f} < {VAL_MIN} (global values)")
                if contrast(d["ga"], sf) < 4.5:
                    fails.append(f"{name} {bgn} {sfn}: global accent/{sfn} {contrast(d['ga'], sf):.2f} < 4.5 (global hot value)")
            ink = bg if contrast(bg, d["g"]) >= contrast(d["text"], d["g"]) else d["text"]
            if contrast(ink, d["g"]) < VAL_MIN:
                fails.append(f"{name} {bgn}: ink/global {contrast(ink, d['g']):.2f} < {VAL_MIN} (text on a global fill)")
            # (the third colour is told apart by HUE, chosen by eye: amber against purple / blue, teal against orange, violet against mint, teal against periwinkle / salmon)
    if a.report:
        print(f"{'palette':8s} " + " ".join(f"{f}/{b}".ljust(12) for f, b, _, _ in CHECKS))
        for d in rows:
            print(f"{d['name']:8s} " + " ".join(f"{d['worst'][(f, b)]:5.1f}".ljust(12) for f, b, _, _ in CHECKS))
        print("minimum  " + " ".join(f"{lim:<12.2f}" for _, _, lim, _ in CHECKS))
        for d in rows:
            for m in d["dep"]:
                print(f"departure {d['name']}: {m}")
    if a.out:
        names = [d["name"] for d in rows]
        out = ["/* generated by tools/gen_ui_palettes.py: the UI palettes (themes with per-part A/B colours and backgrounds) */",
               "#pragma once", "#include <stdint.h>", "",
               "/* ui_pal_t {name, bg[2], bgl, surf[2], text, mid, line[2], raise[2], rec, c[4], r[4], a[4], g, ga, hc} (src/gfx.c; 0.2.24: g / ga the GLOBAL colour and its accent) */",
               "static const ui_pal_t UI_PALETTES[] = {"]
        f4 = lambda v: "{" + ", ".join(f"0x{x:04x}" for x in v) + "}"
        for d in rows:
            out.append(f'    {{"{d["name"]}", {f4(d["bg"])}, 0x{d["bgl"]:04x}, {f4(d["surf"])}, 0x{d["text"]:04x}, 0x{d["mid"]:04x}, '
                       f'{f4(d["line"])}, {f4(d["raise_"])}, 0x{d["rec"]:04x}, {f4(d["c"])}, {f4(d["r"])}, {f4(d["a"])}, 0x{d["g"]:04x}, 0x{d["ga"]:04x}, {d["hc"]}}},')
        out += ["};", f"#define UI_NPALETTES {len(rows)}u", f"#define UI_DEFAULT_INDEX 0u          /* the first theme */",
                f"#define UI_GREY_INDEX {names.index('GREY')}u          /* GREY (hidden-test palette, last but one) */",
                f"#define UI_BW_INDEX {names.index(BW)}u          /* MONO: black and white (src/gfx.c palette_set) */",
                f"#define UI_BW_GREY 0x{to565(BW_GREY):04x}u          /* MONO's one grey: DIM LINE RAISE LANE */",
                f"#define UI_PAL_TAG {PAL_TAG}u          /* stored id = UI_PAL_TAG + index; an untagged id is the first theme */",
                f"#define UI_ACC_PCT {ACC_PCT}"]
        out += [f"#define UI_{k}_PCT {v}" for k, v in PCT.items()] + [f"#define UI_RAISE_PCT {RAISE_PCT}   /* SURF -> TEXT */"]
        out += [f"#define UI_REC_DARK 0x{to565(REC_DARK):04x}u", f"#define UI_REC_LIGHT 0x{to565(REC_LIGHT):04x}u",
                f"#define UI_QR_LIGHT 0x{to565(QR_LIGHT):04x}u", f"#define UI_QR_DARK 0x{to565(QR_DARK):04x}u",
                f"#define UI_CRASH_BG 0x{to565(CRASH_BG):04x}u", f"#define UI_CRASH_INK 0x{to565(CRASH_INK):04x}u", "",
                ""]
        Path(a.out).write_text("\n".join(out))
    for f in fails:
        print("FAIL", f)
    if fails:
        sys.exit(1)


if __name__ == "__main__":
    main()
