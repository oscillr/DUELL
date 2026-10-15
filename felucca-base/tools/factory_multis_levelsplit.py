#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL 0.2.19: the level split in the factory multi records (tools/factory_multis/M*.bin): version 4 -> 5, nothing else.

    tools/factory_multis_levelsplit.py [DIR]       (default tools/factory_multis; rewrites the files in place; a version 5 file is left as it is)

Through 0.2.18 a part had ONE level, P_LEVEL (id 0), the sound's loudness and the player's fader at once. 0.2.19 (owner, 2026-10-15) splits it:
P_TRIM (the PATCH TRIM, stored with a patch) takes the id of the legacy DUCK HOLD, and P_LEVEL is the MIXER level, unity (112 = 0 dB) by default. Per part of a
record: the TRIM value := the old LEVEL value, the LEVEL value := 112, the version byte -> 5. The part plays at TRIM x MIXER LEVEL (fx.c part_lvl) and
LEVEL_Q12[112] = 4096, so every factory multi renders bit for bit as before (tests/multi_render_test.c is unchanged and must pass). Values are +64, a byte
each, after the part's 4 + 7 header bytes (multi.c); the part starts at MU_G_AT + 2 x ng, the second one after the first one's steps and FM6 patch.
"""
import re
import sys
from pathlib import Path

MU_G_AT, MB_N, PS_STEP, UNITY = 20, 7, 9, 112
SRC = Path(__file__).resolve().parent.parent / "firmware" / "src"


def ids():
    core = re.sub(r"//[^\n]*", " ", re.sub(r"/\*.*?\*/", " ", (SRC / "core.h").read_text(), flags=re.S))
    m = re.search(r"enum\s*\{\s*P_LEVEL\b(.*?)P_COUNT", core, re.S)
    names = ["P_LEVEL"] + re.findall(r"\b(P_\w+)\b", m.group(1))
    return names.index("P_LEVEL"), names.index("P_TRIM"), len(names)


def patch(b):
    b = bytearray(b)
    if b[0] == 5:
        return bytes(b)
    lv, tr, count = ids()
    if b[0] != 4 or b[1] != count:
        raise SystemExit("record is not version 4 with %d parameters: not rewritten" % count)
    np_, pos = b[1], MU_G_AT + 2 * b[2]
    for _ in range(2):
        v = pos + 4 + MB_N
        b[v + tr] = b[v + lv]                        # the trim: what the part's one level was
        b[v + lv] = UNITY + 64                       # the mixer level: unity
        flags = b[pos + 3]
        pos = v + np_
        pos += 8 + PS_STEP * sum(bin(x).count("1") for x in b[pos:pos + 8]) + (128 if flags & 1 else 0)
    b[0] = 5
    return bytes(b)


def main():
    d = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent / "factory_multis"
    for f in sorted(d.glob("M*.bin")):
        old = f.read_bytes()
        new = patch(old)
        if new != old:
            f.write_bytes(new)
            print("%s: version %d -> 5" % (f.name, old[0]))


if __name__ == "__main__":
    main()
