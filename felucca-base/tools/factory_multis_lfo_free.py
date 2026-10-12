#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL 0.2.2: set both LFOs' TRIG to FREE in the factory multi records (tools/factory_multis/M*.bin), nothing else.

    tools/factory_multis_lfo_free.py [DIR]        (default tools/factory_multis; rewrites the files in place, idempotent)

A multi record (firmware/src/multi.c) stores every parameter of each part as a byte (value + 64), so the LFO default FREE
(params.c, DUELL 0.2.2) does not reach a record on its own. This rewrites exactly four bytes a record (P_LTRIG and P_L2TRIG of
part 1 and part 2) to FREE (1 + 64) and leaves every other byte as it was, then re-checks the record length with the same walk
gen_duell_multis.py check() uses. M13..M16 (tests/gen_test_multis.c) come out the same as that generator's output (M16 keeps its
older per-part SCALE bytes, which a load ignores). Records must be version 2 with np == P_COUNT (the ids are read from core.h).
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_duell_sounds as g  # noqa: E402  (parse_ids: the P_ ids from core.h)

MU_G_AT, MB_N, PS_STEP, FREE = 20, 7, 9, 1


def patch(b, ids):
    b = bytearray(b)
    np_, ng, pos = b[1], b[2], MU_G_AT + 2 * b[2]
    if b[0] != 2 or np_ != len(ids):
        raise SystemExit("record is not version 2 with np == P_COUNT (%d): not rewritten" % len(ids))
    for _ in range(2):
        vals = pos + 4 + MB_N
        for pid in ("P_LTRIG", "P_L2TRIG"):
            b[vals + ids[pid]] = FREE + 64
        flags = b[pos + 3]
        pos = vals + np_
        pos += 8 + PS_STEP * sum(bin(x).count("1") for x in b[pos:pos + 8]) + (128 if flags & 1 else 0)
    n = b[pos]
    if pos + 1 + 3 * n + 3 != len(b):
        raise SystemExit("record length does not match its parts")
    return bytes(b)


def main(argv):
    d = Path(argv[1]) if len(argv) > 1 else Path(__file__).resolve().parent / "factory_multis"
    ids = g.parse_ids()
    for f in sorted(d.glob("M*.bin")):
        old = f.read_bytes()
        new = patch(old, ids)
        f.write_bytes(new)
        print("%s: %d bytes, %d changed" % (f.name, len(new), sum(x != y for x, y in zip(old, new))))


if __name__ == "__main__":
    main(sys.argv)
