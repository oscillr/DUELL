#!/usr/bin/env python3
"""List the objects in a DUELL emulator saved-data file (duell-flash-*.bin, from the emulator page's
"Download saved data"), and optionally extract the multis.

File: "DUELLFLASH1", u16 range count, per range u32 address + u32 length (little endian), then the bytes.
Objects (firmware/src/storage.c): A/B sector pairs; a sector = 32-byte commit record (magic "FELU", u16 type,
u16 slot = copy, u32 seq, u32 len, u32 crc, 2 x u32 reserved, u32 header crc), payload from offset 256. The valid
copy with the newest seq wins. Multis (firmware/src/multi.c): objects 13..20 (OBJ_MULTI0..7), payload "MUL1",
u16 len1, u16 len2, record 1, record 2 -> slots M(2k+1), M(2k+2).

  flash_dump.py FILE [--extract-multis DIR]
"""
import argparse, os, struct, sys, zlib

MAGIC = b"DUELLFLASH1"
SECTOR, PAYLOAD_OFF, PAYLOAD_MAX = 4096, 256, 4096 - 256
ST_MAGIC = 0x554C4546
MU_MAGIC = b"MUL1"
OBJ_SETTINGS, OBJ_UPRESET0, OBJ_UPFM6, OBJ_AUTOSAVE, OBJ_PATSLOT0, OBJ_MULTI0, OBJ_PATCH0, OBJ_COUNT = 0, 5, 8, 9, 10, 13, 21, 27
ENGINES = ["ANALOG", "DIGITAL", "PHASE", "LOFI", "SAMPLE", "VOICE", "TRIO", "WHEEL", "GRAIN", "PHYS",
           "DRUM", "NOISE", "FM6", "SLICE"]
MODES = ["SPLIT", "STEREO", "LAYER"]
MB_N, PS_STEP, FM6_PACKED, MU_G_AT = 7, 9, 128, 20


def read_file(path):
    d = open(path, "rb").read()
    if d[:11] != MAGIC:
        sys.exit("%s: not a DUELL saved-data file" % path)
    n, = struct.unpack_from("<H", d, 11)
    at = 13 + 8 * n
    flash = {}                                   # sector address -> 4096 bytes
    for i in range(n):
        addr, ln = struct.unpack_from("<II", d, 13 + 8 * i)
        if ln % SECTOR or addr % SECTOR or at + ln > len(d):
            sys.exit("%s: damaged ranges" % path)
        for o in range(0, ln, SECTOR):
            flash[addr + o] = d[at + o:at + o + SECTOR]
        at += ln
    if at != len(d):
        sys.exit("%s: extra bytes at the end" % path)
    return flash


def sector_addr(obj, copy):
    if obj == OBJ_SETTINGS:
        return 0xFC000 + copy * SECTOR
    if obj >= OBJ_PATCH0:                        # the PATCH store: chunks 0..2 USR3's old place, 3..4 the old preset banks, 5 the FM6 bank's pair
        c = obj - OBJ_PATCH0
        if c < 3:
            return 0xC8000 + c * 2 * SECTOR + copy * SECTOR
        if c < 5:
            return 0xDC000 + (c - 3) * 2 * SECTOR + copy * SECTOR
        return 0xFE000 if copy else 0x9F000
    if obj >= OBJ_MULTI0:
        k = obj - OBJ_MULTI0
        return (0x97000 + k * 2 * SECTOR if k < 4 else 0xCE000 + (k - 4) * 2 * SECTOR) + copy * SECTOR
    if obj >= OBJ_PATSLOT0:
        return 0xD6000 + (obj - OBJ_PATSLOT0) * 2 * SECTOR + copy * SECTOR
    if obj == 7 or obj == OBJ_UPFM6:
        return 0xFE000 if copy else 0x9F000
    if obj == OBJ_AUTOSAVE:
        return 0xE5000 + copy * SECTOR
    if obj >= OBJ_UPRESET0:
        return 0xDC000 + (obj - OBJ_UPRESET0) * 2 * SECTOR + copy * SECTOR
    return None


def head(flash, obj, copy):
    """(seq, payload) of a valid copy, else None (storage.c st_head + st_body)."""
    s = flash.get(sector_addr(obj, copy))
    if s is None:
        return None
    magic, typ, slot, seq, ln, crc, _r0, _r1, hcrc = struct.unpack_from("<IHHIIIIII", s, 0)
    if magic != ST_MAGIC or typ != obj or slot != copy or ln > PAYLOAD_MAX:
        return None
    if hcrc != zlib.crc32(s[:28]):
        return None
    pay = s[PAYLOAD_OFF:PAYLOAD_OFF + ln]
    if zlib.crc32(pay) != crc:
        return None
    return seq, pay


def current(flash, obj):
    """The newest valid copy (A on a tie; sequence compared with wrap), or None."""
    a, b = head(flash, obj, 0), head(flash, obj, 1)
    if a and b:
        d = (b[0] - a[0]) & 0xFFFFFFFF
        return (b if d and d < 0x80000000 else a) + ("B" if d and d < 0x80000000 else "A",)
    if a:
        return a + ("A",)
    if b:
        return b + ("B",)
    return None


def summarize(rec):
    """One line about a multi record (multi.c layout), or None if it is not one this reader understands."""
    if len(rec) < MU_G_AT or not rec[0]:
        return None
    ver, np_, ng = rec[0], rec[1], rec[2]
    mode = MODES[rec[4]] if rec[4] < len(MODES) else "?%d" % rec[4]
    pos = MU_G_AT + 2 * ng
    if pos > len(rec):
        return None
    tempo, = struct.unpack_from("<h", rec, MU_G_AT)       # g[G_BPM]
    parts = []
    for _ in range(2):
        if pos + 4 + MB_N + np_ + 8 > len(rec):
            return None
        eng, preset, fl = rec[pos], rec[pos + 1], rec[pos + 3]
        pos += 4 + MB_N + np_
        steps = sum(bin(x).count("1") for x in rec[pos:pos + 8])
        pos += 8 + steps * PS_STEP + (FM6_PACKED if fl & 1 else 0)
        parts.append("%s #%d (%d steps)" % (ENGINES[eng] if eng < len(ENGINES) else "?%d" % eng, preset, steps))
    return "mode %s, tempo %d, part 1 %s, part 2 %s, v%d" % (mode, tempo, parts[0], parts[1], ver)


def name_of(rec):
    raw = rec[8:20].split(b"\0")[0]
    return raw.decode("ascii", "replace") if all(32 <= c <= 126 for c in raw) else ""


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("file")
    ap.add_argument("--extract-multis", metavar="DIR", help="write each used multi's raw record to DIR/Mnn.bin")
    args = ap.parse_args()
    flash = read_file(args.file)
    print("%s: %d sectors" % (args.file, len(flash)))
    names = {OBJ_SETTINGS: "settings", OBJ_AUTOSAVE: "autosave", OBJ_UPFM6: "user presets' FM6 patches (retired)"}
    for obj in range(OBJ_COUNT):
        if OBJ_MULTI0 <= obj < OBJ_PATCH0 or 1 <= obj <= 4 or obj == 7:
            continue
        c = current(flash, obj)
        if c:
            label = names.get(obj) or ("patch store chunk %d" % (obj - OBJ_PATCH0) if obj >= OBJ_PATCH0 else
                                       "pattern slots %d" % (obj - OBJ_PATSLOT0) if obj >= OBJ_PATSLOT0 else "user presets %d (retired)" % (obj - OBJ_UPRESET0))
            print("  object %2d %-26s copy %s seq %d, %d bytes" % (obj, label, c[2], c[0], len(c[1])))
    if args.extract_multis:
        os.makedirs(args.extract_multis, exist_ok=True)
    used = 0
    for k in range(8):
        c = current(flash, OBJ_MULTI0 + k)
        if not c:
            continue
        pay = c[1]
        if len(pay) < 8 or pay[:4] != MU_MAGIC:
            print("  multi object %d (copy %s): payload is not MUL1" % (k, c[2]))
            continue
        l0, l1 = struct.unpack_from("<HH", pay, 4)
        if 8 + l0 + l1 != len(pay):
            print("  multi object %d (copy %s): lengths do not match" % (k, c[2]))
            continue
        for h, rec in enumerate((pay[8:8 + l0], pay[8 + l0:])):
            if not rec:
                continue
            slot = 2 * k + h + 1
            used += 1
            s = summarize(rec)
            print("  M%02d  %-12s  %s  [object %d copy %s seq %d, %d bytes]" % (
                slot, name_of(rec) or "(no name)", s or "unreadable record", OBJ_MULTI0 + k, c[2], c[0], len(rec)))
            if args.extract_multis:
                with open(os.path.join(args.extract_multis, "M%02d.bin" % slot), "wb") as f:
                    f.write(rec)
    print("%d multi(s) stored" % used)
    print("(an M01..M04 not listed above shows its factory multi, tools/factory_multis)")


if __name__ == "__main__":
    main()
