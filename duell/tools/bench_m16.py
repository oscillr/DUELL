#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The bench's stress multi in M16, for one run only (owner 2026-11-18: no test multis on the unit).

The unit's M16 is SLOW THAW. A bench suite that loads `multi 16 "STRESS TEST"` gets the stress multi
(tools/bench_multis/STRESS TEST.bin) written into M16 before the run, and M16 is put back exactly as it was
(byte for byte: the owner's saved M16, or no saved record so the built-in SLOW THAW plays) when the run ends.
The write goes over the editor protocol's BACKUP_LIST / BACKUP_GET / BACKUP_PUT (web/EDITOR_PROTOCOL.md "v7"),
object 20 (M15 and M16, magic MUL1): M15's half is written back unchanged.
Before writing, the original object is saved to <out>/m16-before.bin, so a run that dies can be put right with
  python3 tools/bench_m16.py --restore <out>/m16-before.bin
Needs mido with python-rtmidi (LRRY/.venv). Writes only to a unit whose INFO says DUELL.
"""
import argparse
import os
import queue
import struct
import sys
import time
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
STRESS = os.path.join(HERE, "bench_multis", "STRESS TEST.bin")
OBJ = 20                                   # M15-16
MU_HDR, MU_MAGIC = 8, b"MUL1"
LIST, GET, PUT, INFO = 65, 66, 67, 1
CHUNK = 256


def u32(n):
    return [(n >> (i * 7)) & (15 if i == 4 else 127) for i in range(5)]


def r32(a, o):
    return a[o] | a[o + 1] << 7 | a[o + 2] << 14 | a[o + 3] << 21 | a[o + 4] << 28


def pack(b):
    out = []
    for o in range(0, len(b), 7):
        c = b[o:o + 7]
        out.append(sum((x >> 7) << i for i, x in enumerate(c)))
        out.extend(x & 127 for x in c)
    return out


def unpack(a, size):
    out, i = bytearray(), 0
    while len(out) < size:
        n, m = min(7, size - len(out)), a[i]
        i += 1
        for k in range(n):
            out.append(a[i] | (m >> k & 1) << 7)
            i += 1
    return bytes(out)


class Unit:
    """the editor link over MIDI SysEx: F0 7D 46 4C cmd args F7, one request at a time"""

    def __init__(self, want="LRRY"):
        import mido
        self.mido, self.q = mido, queue.Queue()
        ins = [n for n in mido.get_input_names() if want in n]
        outs = [n for n in mido.get_output_names() if want in n]
        if len(ins) != 1 or len(outs) != 1:
            raise SystemExit(f"bench_m16: need exactly one MIDI port with '{want}' (inputs {ins}, outputs {outs})")
        self.inp = mido.open_input(ins[0], callback=self._rx)
        self.out = mido.open_output(outs[0])
        time.sleep(0.3)

    def _rx(self, msg):
        d = list(msg.data) if msg.type == "sysex" else []
        if len(d) >= 4 and d[:3] == [0x7D, 0x46, 0x4C]:
            self.q.put(d[3:])

    def ask(self, cmd, args=(), timeout=6.0):
        while not self.q.empty():
            self.q.get_nowait()
        self.out.send(self.mido.Message("sysex", data=[0x7D, 0x46, 0x4C, cmd, *args]))
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            try:
                r = self.q.get(timeout=max(end - time.monotonic(), 0.01))
            except queue.Empty:
                break
            if r[0] == cmd:
                return r[1:]
        raise SystemExit(f"bench_m16: no answer to command {cmd}")

    def close(self):
        self.inp.close()
        self.out.close()

    def check_duell(self):
        txt = bytes(b for b in self.ask(INFO, timeout=2.0) if 32 <= b < 127)
        if b"DUELL" not in txt:
            raise SystemExit("bench_m16: the unit on this port is not DUELL (INFO has no DUELL); nothing written")

    def read_obj(self):
        a = self.ask(LIST, timeout=4.0)
        if a[0] != 1 or a[1]:
            raise SystemExit(f"bench_m16: LIST refused (rc {a[1]})")
        size = None
        for i in range(a[2]):
            p = 3 + i * 11
            if a[p] == OBJ:
                size, crc = r32(a, p + 1), r32(a, p + 6)
        if size is None:
            raise SystemExit("bench_m16: the unit does not list the multis M15-16")
        b = bytearray()
        for off in range(0, size, CHUNK):
            n = min(CHUNK, size - off)
            r = self.ask(GET, [OBJ, *u32(off), n & 127, n >> 7], timeout=2.0)
            if r[1] or r[0] != OBJ or r32(r, 2) != off:
                raise SystemExit(f"bench_m16: reading M15-16 failed (rc {r[1]})")
            b += unpack(r[9:], n)
        if size and zlib.crc32(b) != crc:
            raise SystemExit("bench_m16: M15-16 changed while it was read")
        return bytes(b)

    def write_obj(self, b):
        def put(args):
            r = self.ask(PUT, args)
            if r[2]:                                    # (the reply: op, id, rc)
                raise SystemExit(f"bench_m16: writing M15-16 failed (rc {r[2]}{': stop playback first' if r[2] == 3 else ''})")
        put([0, OBJ, *u32(len(b)), *u32(zlib.crc32(b) if b else 0)])
        for off in range(0, len(b), CHUNK):
            put([1, OBJ, *u32(off), *pack(b[off:off + CHUNK])])
        put([2, OBJ])
        if self.read_obj() != b:
            raise SystemExit("bench_m16: M15-16 reads back different from what was written")


def halves(b):
    if not b:
        return b"", b""
    if b[:4] != MU_MAGIC:
        raise SystemExit("bench_m16: M15-16 is no multi object")
    l0, l1 = struct.unpack_from("<HH", b, 4)
    return b[MU_HDR:MU_HDR + l0], b[MU_HDR + l0:MU_HDR + l0 + l1]


def obj(m15, m16):
    return MU_MAGIC + struct.pack("<HH", len(m15), len(m16)) + m15 + m16 if (m15 or m16) else b""


def put_stress(out_dir, unit=None):
    """M16 = the stress multi; the original object saved to out_dir/m16-before.bin. Returns the original bytes."""
    u = unit or Unit()
    try:
        u.check_duell()
        before = u.read_obj()
        with open(os.path.join(out_dir, "m16-before.bin"), "wb") as f:
            f.write(before)
        m15, _ = halves(before)
        u.write_obj(obj(m15, open(STRESS, "rb").read()))
        return before
    finally:
        if unit is None:
            u.close()


def put_back(before, unit=None):
    """M15-16 exactly as `before` (an empty object: no saved M15 / M16, the built-in multis play)"""
    u = unit or Unit()
    try:
        u.check_duell()
        u.write_obj(before)
    finally:
        if unit is None:
            u.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--stress", metavar="DIR", help="write the stress multi into M16; the original goes to DIR/m16-before.bin")
    g.add_argument("--restore", metavar="FILE", help="write M15-16 back from a saved m16-before.bin")
    a = ap.parse_args()
    if a.stress:
        put_stress(a.stress)
        print("M16 = STRESS TEST (original saved)")
    else:
        put_back(open(a.restore, "rb").read())
        print("M15-16 restored")


if __name__ == "__main__":
    sys.exit(main())
