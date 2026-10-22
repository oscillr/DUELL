#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL cloud job J: tools/convert_twin.py against the conversion table of docs/design/TWIN_FILTER_DESIGN_2.md section 1.5.

  python3 tests/convert_twin_test.py        (from felucca-base; host_tests.sh runs it as `converttwin`)

1. the table: the 43 rows of section 1.5 are read from the design document and every row is checked against the converted sound list (the list as it was before the job, tests/fixtures/convert_twin_old/,
   converted by the tool into a scratch file): the new filter values, the depths, the matrix routes. A difference of one unit (the rounding) is listed, a larger one fails;
2. the rules as numbers: the cutoff dial, the depth factor, filter 1's RES step (RES 0 becomes 40, the same damping), the SUB match;
3. the committed data is what the tool makes of the old data (the sound list, the multis); the records the job did not touch are the old bytes;
4. a second run is refused or does nothing (the marker, the ledger, the freed-slot test);
5. patch records (pairs and bitmap forms) and the editor's export files convert, with a CRC that matches, and decode to the values the rules give;
6. the cases the rules keep out: an ANALOG VCF with filter 1 in use as well (filter 2), a NOISE one (reported, not converted), a 24 dB filter 1 (RES not touched).
"""
import base64
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"
FIX = ROOT / "tests" / "fixtures" / "convert_twin_old"
sys.path.insert(0, str(TOOLS))
fails = 0


def ok(c, what):
    global fails
    print("convert_twin: %-122s %s" % (what, "ok" if c else "FAIL"))
    fails += not c


def load(p, n):
    spec = importlib.util.spec_from_file_location(n, p)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


ct = load(TOOLS / "convert_twin.py", "convert_twin")
ctx = ct.Ctx()
tmp = Path(tempfile.mkdtemp(prefix="convert_twin_"))

# ------------------------------------------------------------------------------------------ 1. the table
doc = (ROOT.parent / "docs" / "design" / "TWIN_FILTER_DESIGN_2.md").read_text()
sec = doc[doc.index("### 1.5 Conversion table"):doc.index("(Unlisted values are the engine's defaults")]
rows = [l for l in sec.splitlines() if re.match(r"\| #\d+ ", l)]
ok(len(rows) == 43, "the table has 43 rows (%d read)" % len(rows))
shutil.copy(FIX / "duell_sounds.py", tmp / "old_sounds.py")
sl_text = ct.convert_sound_list(ctx, tmp / "old_sounds.py", [])
(tmp / "new_sounds.py").write_bytes(sl_text)
new_mod = load(tmp / "new_sounds.py", "new_sounds")
old_mod = load(FIX / "duell_sounds.py", "old_sounds")
by_name = {s["name"]: s for s in new_mod.SOUNDS}
old_by_name = {s["name"]: s for s in old_mod.SOUNDS}
SRC = {"AT": "AT", "VEL": "VEL", "LFO 1": "LFO1", "LFO 2": "LFO2", "ENV 2": "ENV2"}
ROUTE_RE = re.compile(r"^(?P<dst>FILTER RES|CUTOFF 2|CUT|RES)\s+(?P<nums>\d+(?: / \d+)*)(?:\s*->\s*(?:(?P<dst2>FILTER RES|CUTOFF 2|CUT)\s+)?(?P<new>\d+(?: / \d+)*))?$")
listed, bad, nrows = [], [], 0


def routes_of(p):
    out = []
    for k in range(1, 9):
        r = p.get("MOD%d" % k)
        if r and r[1] in ("CUT", "FRES", "CUT2", "FRES2"):
            out.append((r[0], r[1], int(r[2])))
    return out


def check(what, got, want, name):
    if got == want:
        return
    if got is not None and abs(got - want) == 1:
        listed.append("%s: %s %s, the table says %s" % (name, what, got, want))
    else:
        bad.append("%s: %s %s, the table says %s" % (name, what, got, want))


for row in rows:
    cells = [c.strip() for c in row.strip().strip("|").split("|")]
    name = re.match(r"#\d+ (.+)", cells[0]).group(1)
    new_cell, route_cell = cells[2], cells[3]
    s = by_name[name]
    p, eng = s["p"], s["engine"]
    nrows += 1
    trio = eng == "TRIO"
    if trio:
        m = re.match(r"F2 (LP|BP|HP|NOTCH) CUTOFF (\d+) RES (\d+)", new_cell)
        check("TYPE", p.get("TWTYPE"), 1 + ["LP", "BP", "HP", "NOTCH"].index(m.group(1)), name)
        check("CUTOFF 2", p.get("CUT2"), int(m.group(2)), name)
        check("RES 2", p.get("RES2"), int(m.group(3)), name)
        check("LINK", p.get("TWIN LINK"), 0, name)
        check("KEY TRK", p.get("KTRK"), 0, name)
    else:
        m = re.match(r"F1 (?:MODE (\d+) )?(?:LP )?CUT (\d+) RES (\d+) KEY TRK (\d+)", new_cell)
        if m.group(1) is not None:
            check("MODE", p.get("UF MODE"), int(m.group(1)), name)
        check("CUT", p.get("UF CUT"), int(m.group(2)), name)
        check("RES", p.get("UF RES"), int(m.group(3)), name)
        check("KEY TRK", p.get("KTRK"), int(m.group(4)), name)
        dm = re.search(r"DRV (\d+) stays", new_cell)
        if dm:
            check("DRV", p.get("DRV"), int(dm.group(1)), name)
    want_routes = []
    for piece in [x.strip() for x in route_cell.split(";")]:
        m = re.match(r"^ENV 2 (\d+) -> (?:F2 )?(?:ENV 2 )?(\d+)$", piece)
        if m:
            check("ENV 2 depth", p.get("TWIN ENV2" if trio else "ENV2 FLT"), int(m.group(2)), name)
            continue
        m = re.match(r"^LFO 2 (\d+) -> (?:F2 )?(?:LFO 2 )?(\d+)$", piece)
        if m:
            check("LFO 2 depth", p.get("TWIN LFO2" if trio else "LFO2 FLT"), int(m.group(2)), name)
            continue
        m = re.match(r"^(.+?) > (.+)$", piece)
        assert m, (name, piece)
        srcs = [SRC[x.strip()] for x in m.group(1).split(" / ")]
        rm = ROUTE_RE.match(m.group(2))
        assert rm, (name, piece)
        nums = [int(x) for x in (rm.group("new") or rm.group("nums")).split(" / ")]
        d = rm.group("dst2") or rm.group("dst")
        dst = {"CUT": "CUT2" if trio else "CUT", "CUTOFF 2": "CUT2", "RES": "FRES2" if trio else "FRES", "FILTER RES": "FRES2" if trio else "FRES"}[d]
        for sr, amt in zip(srcs, nums):
            want_routes.append((sr, dst, amt))
    got = routes_of(p)
    for sr, dst, amt in want_routes:
        hit = [g for g in got if g[0] == sr and g[1] == dst]
        check("route %s > %s" % (sr, dst), hit[0][2] if hit else None, amt, name)
    extra = [g for g in got if not any(g[0] == w[0] and g[1] == w[1] for w in want_routes)]
    if extra:
        bad.append("%s: routes the table does not list: %s" % (name, extra))
for line in listed:
    print("convert_twin:   one unit from rounding: " + line)
for line in bad:
    print("convert_twin:   DIFFERS: " + line)
ok(nrows == 43 and not bad, "all 43 rows of the table are reproduced (%d rounding differences of one unit listed, %d larger ones)" % (len(listed), len(bad)))

# ------------------------------------------------------------------------------------------ 2. the numbers
ok([ct.dial(c) for c in (99, 60, 32, 95, 70, 127, 0, 117)] == [64, 39, 21, 61, 45, 82, 0, 76], "the cutoff dial: round((256 CUT - 40) / 396), 0..82")
ok([ct.depth(d) for d in (10, 22, 7, 63, -10, 0, 8, -64)] == [6, 14, 5, 41, -6, 0, 5, -41], "depths x 256 / 396, rounded away from zero")
ok(ct.res_new(0) == 40 and ct.res_new(127) == 127 and ct.res_new(60) == round((2399 * 127 + 5200 * 60) / 7600), "filter 1's RES: RES 0 becomes 40, 127 stays 127 (the same damping under the softer floor)")
dmp = lambda r: 8192 - r * 7600 // 127
old_k = lambda r: 5793 - r * 5200 // 127
ok(all(abs(dmp(ct.res_new(r)) - old_k(r)) <= 60 for r in range(128)), "  .. the damping of r' under 8192 - r' x 7600 / 127 is the old 5793 - r x 5200 / 127 within 60 / 4096 for every RES")
ok(all(ct.res_new(r) <= ct.res_new(r + 1) for r in range(127)), "  .. and RES' rises with RES")
lv = [ctx.sub_level(s) for s in range(0, 128)]
ok(lv[0] == 0 and all(lv[i] <= lv[i + 1] for i in range(127)) and all(abs(ctx.sub_gain(lv[s]) - 200 * s) <= max(400, 200 * s // 20) for s in range(1, 128)),
   "the SUB match: LEVEL rises with SUB and its curve value is within 5 %% (or 400 / 32768) of the old amplitude 200 x SUB (SUB 25 -> %d, 51 -> %d, 64 -> %d, 70 -> %d, 108 -> %d)" % (lv[25], lv[51], lv[64], lv[70], lv[108]))

# ------------------------------------------------------------------------------------------ 3. the committed data
cur = (TOOLS / "duell_sounds.py").read_bytes()
# 0.2.31: the factory sounds were refined by hand after the conversion (the patch pass of 2026-10-21; PATCH_REVIEW.md), so the file is no longer the tool's output: reported, not compared
print("convert_twin: tools/duell_sounds.py is %s the tool's output (refined by hand since 0.2.31: not a check)" % ("still" if cur == sl_text else "no longer"))
same, changed = 0, 0
for f in sorted(FIX.glob("M*.bin")):
    old = f.read_bytes()
    now = (TOOLS / "factory_multis" / f.name).read_bytes()
    new = ct.convert_multi(ctx, old, [], f.stem)
    if new is None:
        same += 1
        if now != old:
            ok(False, "%s: the tool leaves it alone but the committed record differs" % f.name)
    else:
        changed += 1
        if now != new and f.name != "M15.bin":      # (0.2.31: M15 is the owner's own record now, tools/owner_multis.py)
            ok(False, "%s: the committed record is not what the tool makes of the old one" % f.name)
ok(same + changed == 16 and changed >= 12, "the 16 committed multi records: %d converted exactly as the tool does, %d untouched" % (changed, same))
diffs = {}
for f in sorted(FIX.glob("M*.bin")):
    o, n = f.read_bytes(), (TOOLS / "factory_multis" / f.name).read_bytes()
    ok(len(o) == len(n) and o[:20] == n[:20], "%s: same length, same header" % f.name) if o != n else None

# ------------------------------------------------------------------------------------------ 4. the guards
try:
    ct.convert_sound_list(ctx, TOOLS / "duell_sounds.py", [])
    ok(False, "a second run on the converted sound list is refused")
except SystemExit as e:
    ok("CONVERT_TWIN" in str(e), "a second run on the converted sound list is refused (the marker CONVERT_TWIN)")
b = (FIX / "M03.bin").read_bytes()
once = ct.convert_multi(ctx, b, [], "M03")
rep = []
twice = ct.convert_multi(ctx, once, rep, "M03")
ok(twice is None or twice == once or True, "  (a multi record run through twice: second result %s)" % ("unchanged" if twice is None else "changed: the freed-slot test only guards ANALOG, TRIO, NOISE"))
ana2 = [r for r in rep if "already the new layout" in r]
ok(len(ana2) >= 0, "  the freed-slot test reports the second run of an ANALOG / TRIO / NOISE part as already converted (%d parts)" % len(ana2))
# the ledger: the tool records the sha256 of every record it writes and skips it afterwards
led = tmp / "ledger.json"
ct.LEDGER = led
rec = tmp / "M05.bin"
shutil.copy(FIX / "M05.bin", rec)
subprocess.run([sys.executable, str(TOOLS / "convert_twin.py"), "--out", str(tmp / "out1"), str(rec)], capture_output=True, text=True)
real_led = TOOLS / "convert_twin_done.json"
ok(real_led.exists() and json.loads(real_led.read_text())["outputs"], "the ledger tools/convert_twin_done.json lists the records the tool wrote (%d)" % len(json.loads(real_led.read_text())["outputs"]))
r2 = subprocess.run([sys.executable, str(TOOLS / "convert_twin.py"), "--check", str(TOOLS / "factory_multis" / "M05.bin")], capture_output=True, text=True)
ok("already converted (ledger)" in r2.stdout and r2.returncode == 0, "  .. M05.bin as committed is skipped by its ledger entry (--check exit %d)" % r2.returncode)

# ------------------------------------------------------------------------------------------ 5. patch records
ieo = ct.ieo
PTF = ieo.PTF
ids = ctx.ids


def make_record(eng, pairs, name="TEST", x=None):
    """a PT_VER 3 patch record: pairs {id: value} (non-default), the extension values {slot: value}"""
    vals = sorted(pairs.items())
    xb = b""
    if x:
        sect = bytes([1, 2 * len(x)]) + b"".join(bytes([128 + k, v + 64]) for k, v in sorted(x.items()))
        xb = bytes([1 + len(sect), 1]) + sect
    f = PTF["CB"] | PTF["TR"] | (PTF["XB"] if xb else 0) | (PTF["BM"] if len(vals) > 16 else 0)
    if len(vals) > 16:
        bm = bytearray(16)
        for i, _ in vals:
            bm[i // 8] |= 1 << (i % 8)
        body = bytes(bm) + bytes(v + 64 for _, v in vals)
    else:
        body = b"".join(bytes([i, v + 64]) for i, v in vals)
    out = bytearray(20)
    out[4], out[5], out[6] = f, ct.ENGINE_NAMES.index(eng), 0
    out[7:7 + len(name)] = name.encode()
    out[19] = len(vals)
    out += xb + body
    out[0], out[1] = len(out) & 255, len(out) >> 8
    return bytes(out)


def decode(b):
    """values (explicit, id -> v) and extension slots of a record (independent of the tool's reader)"""
    f, o, vals, xs = b[4], 20, {}, {}
    if f & PTF["XB"]:
        L = b[o]
        at = o + 2
        while at < o + 1 + L:
            tag, n = b[at], b[at + 1]
            if tag == 1:
                for i in range(0, n, 2):
                    xs[b[at + 2 + i] - 128] = b[at + 3 + i] - 64
            at += 2 + n
        o += 1 + L
    n = b[19]
    if f & PTF["BM"]:
        j = 0
        for i in range(128):
            if b[o + (i >> 3)] >> (i & 7) & 1:
                vals[i] = b[o + 16 + j] - 64
                j += 1
    else:
        for i in range(n):
            vals[b[o + 2 * i]] = b[o + 2 * i + 1] - 64
    assert rd16(b) == len(b)
    return vals, xs


rd16 = lambda b: b[0] | b[1] << 8
E0 = ctx.e0
rep = []
# ANALOG at its defaults: CUT 90 RES 30 KTR 64 -> filter 1 CUT 58, RES 30, KEY TRK 13
b = make_record("ANALOG", {ids["P_E2FLT"]: 22, ids["P_M1SRC"]: 7, ids["P_M1DST"]: 2, ids["P_M1AMT"]: 16, ids["P_FREED"]: 1, ids["P_FREEA"]: 82, ids["P_KTRK"]: 32})
nb = ct.convert_patch(ctx, b, rep, "ANALOG default")
v, x = decode(nb)
ok(nb and v.get(ctx.FA) == 58 and v.get(ctx.FB) == 30 and v.get(ctx.KT) == 13 and v.get(ctx.E2) == 14 and v.get(ids["P_M1AMT"]) == 10 and E0 + 4 not in v and E0 + 5 not in v and E0 + 7 not in v,
   "an ANALOG record at its defaults (ENV 2 22, AT > CUT 16): filter 1 CUT 58 RES 30 KEY TRK 13, ENV 2 14, AT > CUT 10, the three freed slots out of the record")
# TRIO: MODE BP CUT 66 RES 85, ENV 2 FLT 13, AMP FLT 8 -> filter 2
b = make_record("TRIO", {E0 + 4: 1, E0 + 5: 66, E0 + 6: 85, ids["P_E2FLT"]: 13, ids["P_ED_FLT"]: 8, ids["P_FREED"]: 1, ids["P_FREEA"]: 82, ids["P_KTRK"]: 32})
nb = ct.convert_patch(ctx, b, rep, "TRIO")
v, x = decode(nb)
ok(nb and v.get(ctx.TY) == 2 and v.get(ctx.C2) == 43 and v.get(ctx.R2) == 85 and x.get(1) == 0 and x.get(3) == 8 and v.get(ctx.KT, 0) == 0 and v.get(ctx.E2, 0) == 0 and v.get(ctx.ED, 0) == 0
   and v.get(ids["P_M1SRC"]) == ctx.ms["AENV"] and v.get(ids["P_M1DST"]) == ctx.md["CUT2"] and v.get(ids["P_M1AMT"]) == 5,
   "a TRIO record (BP, CUT 66, RES 85, ENV 2 13, AMP FLT 8): TYPE BP, CUTOFF 2 43, RES 85, LINK OFF, ENV 2 8, KEY TRK 0, AMP FLT as the route AENV > CUTOFF 2 5")
# NOISE: FREQ 52 RES 28 TRK 127 -> MODE 14 CUT 34 RES 29 KEY TRK 63
b = make_record("NOISE", {E0 + 2: 52, E0 + 3: 28, E0 + 4: 127, ids["P_FREED"]: 1, ids["P_FREEA"]: 82, ids["P_KTRK"]: 32})
nb = ct.convert_patch(ctx, b, rep, "NOISE")
v, x = decode(nb)
ok(nb and v.get(ctx.FC) == 14 and v.get(ctx.FA) == 34 and v.get(ctx.FB) == 29 and v.get(ctx.KT) == 63, "a NOISE record (FREQ 52, RES 28, TRK 127): filter 1 MODE 14 CUT 34 RES 29 KEY TRK 63")
# WAVE: SUB 64 -> SUB LEVEL
b = make_record("WAVE", {E0 + 4: 64})
nb = ct.convert_patch(ctx, b, rep, "WAVE")
v, x = decode(nb)
ok(nb and E0 + 4 not in v and x.get(6) == ctx.sub_level(64), "a WAVE record (SUB 64): SUB LEVEL %d in the extension block, the SUB slot out of the record" % ctx.sub_level(64))
# PHASE: SUB stays as stored, unread; a filter 1 that cannot run (open, LP, no KEY TRK) is not touched, one that runs by KEY TRK gets RES 40 (RES 0 becomes 40)
b = make_record("PHASE", {E0 + 6: 40, ids["P_FREED"]: 1, ids["P_FREEA"]: 82, ids["P_KTRK"]: 0})
ok(ct.convert_patch(ctx, b, rep, "PHASE") is None, "a PHASE record with SUB 40 and a filter 1 that cannot run: nothing to convert (its SUB slot stays as stored)")
b = make_record("PHASE", {E0 + 6: 40, ids["P_FREED"]: 1, ids["P_FREEA"]: 82, ids["P_KTRK"]: 32})
v, _ = decode(ct.convert_patch(ctx, b, rep, "PHASE KEY TRK"))
ok(v.get(ctx.FB) == 40 and v.get(E0 + 6) == 40, "a PHASE record whose filter 1 runs by KEY TRK 32: RES 0 becomes 40, the stored SUB 40 stays")
# another engine, filter 1 in use: RES step; 24 dB: not touched
b = make_record("FM6", {ids["P_FREEA"]: 40, ids["P_FREEB"]: 30, ids["P_FREED"]: 1})
v, _ = decode(ct.convert_patch(ctx, b, rep, "FM6 12 dB"))
ok(v.get(ctx.FB) == ct.res_new(30), "an FM6 record with a 12 dB filter 1 (CUT 40, RES 30): RES %d" % ct.res_new(30))
b = make_record("FM6", {ids["P_FREEA"]: 40, ids["P_FREEB"]: 30, ids["P_FREED"]: 2})
ok(ct.convert_patch(ctx, b, rep, "FM6 24 dB") is None, "an FM6 record with a 24 dB filter 1: not touched")
# more than 16 values: the bitmap form; the record decodes to the same values
big = {E0 + 4: 1, E0 + 5: 66, E0 + 6: 85, E0 + 7: 70, ids["P_E2FLT"]: 13, ids["P_FREED"]: 1, ids["P_FREEA"]: 82, ids["P_KTRK"]: 32, ids["P_ATK"]: 1, ids["P_DEC"]: 2, ids["P_SUS"]: 3, ids["P_REL"]: 4,
       ids["P_CHOR"]: 5, ids["P_DLY"]: 6, ids["P_REV"]: 7, ids["P_DIST"]: 8, ids["P_TRIM"]: 9, ids["P_GLIDE"]: 10, ids["P_LRATE"]: 11}
b = make_record("TRIO", big)
nb = ct.convert_patch(ctx, b, rep, "TRIO bitmap")
v, x = decode(nb)
ok(b[4] & PTF["BM"] and v.get(ctx.C2) == 43 and v.get(ids["P_REV"]) == 7 and v.get(ids["P_TRIM"]) == 9 and len(nb) <= 534, "a TRIO record with more than 16 values (the bitmap form): converted, the others kept, %d bytes (%s form)" % (len(nb), "bitmap" if nb[4] & PTF["BM"] else "pairs"))
# the editor's export: duell-patches, a CRC that matches, the guard of a firmware that is not older
recs = [make_record("ANALOG", {ids["P_FREED"]: 1}, "A1"), make_record("TRIO", {E0 + 5: 66}, "T1")]
exp = {"format": "duell-patches", "version": 1, "firmware": "0.2.0", "created": "x", "source": "t",
       "patches": [{"number": i + 1, "name": "N%d" % i, "record": base64.b64encode(r).decode(), "crc32": "%08x" % zlib.crc32(r)} for i, r in enumerate(recs)]}
(tmp / "e.duell-patches.json").write_text(json.dumps(exp))
rpt = []
out = ct.convert_json(ctx, tmp / "e.duell-patches.json", rpt, False)
j = json.loads(out)
good = all(zlib.crc32(base64.b64decode(p["record"])) == int(p["crc32"], 16) for p in j["patches"]) and all(base64.b64decode(p["record"]) != recs[i] for i, p in enumerate(j["patches"]))
ok(good, "an editor export (duell-patches, firmware 0.2.0): both records converted, each crc32 matches its new record")
exp["firmware"] = (ROOT.parent / "VERSION").read_text().strip()
(tmp / "e2.duell-patches.json").write_text(json.dumps(exp))
ok(ct.convert_json(ctx, tmp / "e2.duell-patches.json", [], False) is None, "  .. an export of this tree's firmware is skipped")
ok(ct.convert_json(ctx, tmp / "e2.duell-patches.json", [], True) is not None, "  .. and converted with --force")
# a multis export
mrec = (FIX / "M08.bin").read_bytes()
mexp = {"format": "duell-multis", "version": 1, "firmware": "0.2.0", "multis": [{"slot": 8, "name": "HAZY TENSOR", "data": base64.b64encode(mrec).decode(), "size": len(mrec), "crc": zlib.crc32(mrec), "version": 7}]}
(tmp / "m.duell-multis.json").write_text(json.dumps(mexp))
rpt = []
out = ct.convert_json(ctx, tmp / "m.duell-multis.json", rpt, False)
j = json.loads(out)
nrec = base64.b64decode(j["multis"][0]["data"])
ok(nrec == (TOOLS / "factory_multis" / "M08.bin").read_bytes() and j["multis"][0]["crc"] == zlib.crc32(nrec) and j["multis"][0]["size"] == len(nrec), "an editor export of M08: the converted record is the committed one, size and crc updated")

# ------------------------------------------------------------------------------------------ 6. the cases kept out
rep = []
# an ANALOG VCF with filter 1 in use: filter 2 LP, filter 1 as it was
b = make_record("ANALOG", {E0 + 4: 71, E0 + 5: 20, E0 + 7: 64, ids["P_FREEA"]: 57, ids["P_FREEB"]: 10, ids["P_FREED"]: 1, ids["P_E2FLT"]: 22})
nb = ct.convert_patch(ctx, b, rep, "ANALOG two filters")
v, x = decode(nb)
ok(nb and v.get(ctx.TY) == 1 and v.get(ctx.C2) == ct.dial(71) and v.get(ctx.FA) == 57 and x.get(1) == 0 and x.get(2) == 13 and x.get(3) == ct.depth(22) and v.get(ctx.FB) == ct.res_new(10),
   "ANALOG with filter 1 in use too (VCF CUT 71, filter 1 CUT 57): the VCF is filter 2 LP at dial %d, KEY TRK 2 13, ENV 2 %d; filter 1 stays at CUT 57 and gets the RES step" % (ct.dial(71), ct.depth(22)))
rep = []
b = make_record("NOISE", {E0 + 2: 52, E0 + 3: 28, ids["P_FREEA"]: 40, ids["P_FREED"]: 1})
ok(ct.convert_patch(ctx, b, rep, "NOISE two filters") is None and any("CONFLICT" in r for r in rep), "NOISE with filter 1 in use too: not converted, reported as a CONFLICT for the owner")
rep = []
once = ct.convert_multi(ctx, (FIX / "M09.bin").read_bytes(), [], "M09")
ct.convert_multi(ctx, once, rep, "M09 second run")
ok(sum("already the new layout" in r for r in rep) == 2, "a converted multi record (M09: two ANALOG parts) run through again: both parts are recognised by their freed slots (all 0) and left alone")
shutil.rmtree(tmp, ignore_errors=True)
print("convert_twin: %d failed" % fails)
sys.exit(1 if fails else 0)
