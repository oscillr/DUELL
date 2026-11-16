"""Firmware Hub card for a DUELL update: the owner's square hub card background
(1024 x 1024) with the version right-aligned on the "by OSCILL/R" line and the
highlight capsules centred in the empty area below it, saved at 1024 x 1024 (the Hub maximum).

Usage:
  python3 hub_card.py <version> "<row 1 capsule>|<row 1 capsule>" "<row 2 capsule>" ... <out.jpg>
Each argument is one row of capsules, capsules in a row separated by "|".
Example (1.2):
  python3 hub_card.py 1.2 "Up to 16 Voices|Shared Voice Pool" "Works with Newer Units" duell-card.jpg

Background: DUELL/gfx/hub card/hub card background.jpg (owner's artwork).
Layout (measured on that background, 1024 px space): text margins left 62,
right 962; author line baseline 617, its text ends at 626. The widest capsule
row reaches the right margin; the capsule block sits in the middle between the
author line and the bottom edge.
Capsules: Oswald 600, violet (139, 92, 246), like the launch deck cover capsule.
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
BACKGROUND = os.path.normpath(os.path.join(
    HERE, "..", "..", "..", "gfx", "hub card", "hub card background.jpg"))
FONTS = os.path.expanduser("~/Library/Fonts")

W, L, R, BASE, AUTHOR_END = 1024, 62, 962, 617, 626
OUT = 1024  # Hub maximum: square, 240 x 240 to 1024 x 1024


def oswald(size):
    f = ImageFont.truetype(os.path.join(FONTS, "Oswald-VariableFont_wght.ttf"), size)
    f.set_variation_by_axes([600])
    return f


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    version, rows, out = sys.argv[1], [a.split("|") for a in sys.argv[2:-1]], sys.argv[-1]
    S = 3  # draw at 3x, scale down for clean edges
    img = Image.open(BACKGROUND).convert("RGB").resize((W * S, W * S), Image.LANCZOS)
    d = ImageDraw.Draw(img)
    gap = 18 * S
    # biggest capsule font where the widest row fits between the margins
    for size in range(80, 20, -1):
        f = oswald(size * S)
        pad = int(0.45 * size) * S
        widths = [sum(d.textlength(c, font=f) + 2 * pad for c in row) + gap * (len(row) - 1)
                  for row in rows]
        if max(widths) <= (R - L) * S:
            break
    # stretch the padding so the widest row reaches the right margin exactly
    pad += ((R - L) * S - max(widths)) / (2 * len(rows[widths.index(max(widths))]))
    h = int(1.75 * size) * S
    block = len(rows) * h + (len(rows) - 1) * gap
    y = (AUTHOR_END + W) * S // 2 - block // 2

    ver = ImageFont.truetype(os.path.join(FONTS, "Inter_18pt-Bold.ttf"), 45 * S)
    d.text((R * S, BASE * S), "v" + version, font=ver, fill=(245, 240, 255), anchor="rs")
    for row in rows:
        x = L * S
        for c in row:
            w = d.textlength(c, font=f) + 2 * pad
            d.rounded_rectangle((x, y, x + w, y + h), radius=int(0.4 * size) * S, fill=(139, 92, 246))
            d.text((x + w / 2, y + h / 2), c, font=f, fill=(255, 255, 255), anchor="mm")
            x += w + gap
        y += h + gap
    img.resize((OUT, OUT), Image.LANCZOS).save(out, quality=95)
    print("saved", out)


if __name__ == "__main__":
    main()
