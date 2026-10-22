#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Multi record converter, version 6 -> 7 (firmware/src/multi.c MU_VER 7; the cloud plan's job A, docs/design/EXTENSION_BLOCK_PLAN.md 3.6).

    tools/multi_upgrade.py [--check] [FILE ...]      default: tools/factory_multis/M*.bin, in place

What a version 7 record adds, per part, right after its np (P_COUNT) values: nx (one byte, NX = 16) and nx extension values (value + 64, each the
slot's default: firmware/src/up_xb.h XB_DEFS, read from there), then ne (one byte) = 0, no hand-set B edits. Nothing else changes: the header,
the globals, the End B blocks, the values, the steps, the FM6 patches, the automation and the three mix bytes are copied byte for byte (an owner's
record and a `kept` one are exactly the old bytes, with only the version byte 6 -> 7 and these 2 x 18 bytes added). A record that is already
version 7 is left as it is (so the tool can be run again); any other version, or a record that does not parse to its own length, stops it.
--check changes nothing: it lists what would be converted and exits 1 if anything would."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MU_G_AT, MB_N, PS_STEP, FM6_PACKED, NSHOWN = 20, 7, 9, 128, 2


def xb_defs():
    """XB_DEFS of firmware/src/up_xb.h: the stored (value + 64) defaults of the NX extension slots"""
    src = (ROOT / "firmware" / "src" / "up_xb.h").read_text()
    m = re.search(r"#define XB_DEFS \{([^}]*)\}", src)
    d = [int(x.strip().rstrip("uU")) for x in m.group(1).split(",")]
    nx = int(re.search(r"#define NX (\d+)", (ROOT / "firmware" / "src" / "ext.h").read_text()).group(1))
    if len(d) != nx:
        sys.exit("multi_upgrade: XB_DEFS has %d entries, NX is %d" % (len(d), nx))
    return bytes(d)


def upgrade(b, defs):
    """version 6 record -> version 7 record; None if b is already version 7. Stops (SystemExit) on anything it cannot parse exactly."""
    if b[0] == 7:
        return None
    if b[0] != 6:
        sys.exit("multi_upgrade: version %d, only 6 -> 7 is converted" % b[0])
    np_, ng = b[1], b[2]
    out, pos = bytearray(b[:MU_G_AT + 2 * ng]), MU_G_AT + 2 * ng
    out[0] = 7
    for _ in range(NSHOWN):
        fl = b[pos + 3]
        n = 4 + MB_N + np_                                  # engine, preset, AMOUNT, flags, End B block, the values
        out += b[pos:pos + n]
        out += bytes([len(defs)]) + defs + bytes([0])       # nx, the NX defaults, ne = 0
        pos += n
        steps = 8 + PS_STEP * sum(bin(x).count("1") for x in b[pos:pos + 8]) + (FM6_PACKED if fl & 1 else 0)
        out += b[pos:pos + steps]
        pos += steps
    nev = b[pos]
    tail = 1 + 3 * nev + 3                                  # the automation (count, events) and BALANCE, XFADE, SPLIT
    if pos + tail != len(b):
        sys.exit("multi_upgrade: the record does not parse to its own length (%d != %d)" % (pos + tail, len(b)))
    out += b[pos:]
    if len(out) > 1916:
        sys.exit("multi_upgrade: the converted record is %d bytes, over 1916" % len(out))
    return bytes(out)


def main(argv):
    check = "--check" in argv
    files = [Path(a) for a in argv[1:] if a != "--check"] or sorted((ROOT / "tools" / "factory_multis").glob("M*.bin"))
    defs, todo = xb_defs(), 0
    for f in files:
        b = f.read_bytes()
        new = upgrade(b, defs)
        if new is None:
            print("%s: version 7 already" % f.name)
            continue
        todo += 1
        print("%s: version 6 -> 7, %d -> %d bytes" % (f.name, len(b), len(new)))
        if not check:
            f.write_bytes(new)
    return 1 if check and todo else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
