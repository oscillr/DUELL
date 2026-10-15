#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL 0.2.16: the delay MODE in the factory multi records (tools/factory_multis/M*.bin): version 3 -> 4, nothing else.

    tools/factory_multis_delaymode.py [DIR]        (default tools/factory_multis; rewrites the files in place; a version 4 file is left as it is)

0.2.15 stored the delay's mode on global id 15 (0 DELAY, 1 RAND, 2 OCT+) and its PING-PONG switch on id 26 (0 / 1). 0.2.16 has one MODE on id 15
(0 DIGITAL, 1 PING-PONG, 2 RANDOM, 3 OCT UP; core.h G_DMODE) and id 26 is unused (0). Per record: DELAY with PING ON -> 1, DELAY with PING OFF -> 0,
RAND -> 2, OCT+ -> 3 (the ping-pong of an event mode had no effect on its sound but the cross-fade into it); id 26 -> 0; the version byte -> 4. The
globals are 16-bit little endian from byte 20 (multi.c MU_G_AT).
"""
import struct
import sys
from pathlib import Path

MU_G_AT, G_DMODE, G_FREE26 = 20, 15, 26


def patch(b):
    b = bytearray(b)
    if b[0] == 4:
        return bytes(b)
    if b[0] != 3 or b[2] != 27:
        raise SystemExit("record is not version 3 with 27 globals: not rewritten")
    old, ping = (struct.unpack_from("<h", b, MU_G_AT + 2 * i)[0] for i in (G_DMODE, G_FREE26))
    new = (1 if ping == 1 else 0) if old == 0 else {1: 2, 2: 3}.get(old, 0)
    struct.pack_into("<h", b, MU_G_AT + 2 * G_DMODE, new)
    struct.pack_into("<h", b, MU_G_AT + 2 * G_FREE26, 0)
    b[0] = 4
    return bytes(b)


def main(argv):
    d = Path(argv[1]) if len(argv) > 1 else Path(__file__).resolve().parent / "factory_multis"
    for f in sorted(d.glob("M*.bin")):
        old = f.read_bytes()
        new = patch(old)
        f.write_bytes(new)
        print("%s: %d bytes, %d changed" % (f.name, len(new), sum(x != y for x, y in zip(old, new))))


if __name__ == "__main__":
    main(sys.argv)
