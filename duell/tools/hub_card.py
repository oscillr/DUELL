"""Firmware Hub card for a DUELL update: DUELL header 2 (800 x 600) with the
version and the highlight capsules in the dark band at the bottom.

Usage:
  python3 hub_card.py <version> "<capsule 1>" "<capsule 2>" ... <out.jpg>
Example (1.2):
  python3 hub_card.py 1.2 "Up to 16 Voices" "Shared Voice Pool" "Works with Newer Units" duell-card-1.2.jpg

Background: DUELL/gfx/editor header/DUELL-header-2.jpg (owner's artwork, stays 800 x 600).
Capsules: Oswald 600, violet (139, 92, 246), like the launch deck cover capsule.
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
HEADER = os.path.normpath(os.path.join(
    HERE, "..", "..", "..", "gfx", "editor header", "DUELL-header-2.jpg"))
FONTS = os.path.expanduser("~/Library/Fonts")


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    version, caps, out = sys.argv[1], sys.argv[2:-1], sys.argv[-1]
    S = 3  # draw at 3x, scale down for clean edges
    img = Image.open(HEADER).convert("RGB").resize((800 * S, 600 * S), Image.LANCZOS)
    d = ImageDraw.Draw(img)
    osw = ImageFont.truetype(os.path.join(FONTS, "Oswald-VariableFont_wght.ttf"), 22 * S)
    osw.set_variation_by_axes([600])
    ver = ImageFont.truetype(os.path.join(FONTS, "Inter_18pt-Bold.ttf"), 20 * S)
    x, y, h, padx, gap = 75 * S, 470 * S, 42 * S, 14 * S, 10 * S
    label = "v" + version
    d.text((x, y + h // 2), label, font=ver, fill=(245, 240, 255), anchor="lm")
    x += int(d.textlength(label, font=ver)) + 22 * S
    for c in caps:
        w = int(d.textlength(c, font=osw)) + 2 * padx
        d.rounded_rectangle((x, y, x + w, y + h), radius=12 * S, fill=(139, 92, 246))
        d.text((x + w // 2, y + h // 2), c, font=osw, fill=(255, 255, 255), anchor="mm")
        x += w + gap
    if x - gap > 790 * S:
        sys.exit("Capsules run past the right edge (%d px): shorter texts needed." % ((x - gap) // S))
    img.resize((800, 600), Image.LANCZOS).save(out, quality=95)
    print("saved", out)


if __name__ == "__main__":
    main()
