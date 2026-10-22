#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL 0.2.24: tools/import_owner_edits.py against a synthetic PATCHES export made by the firmware's own code (tests/owner_export_gen.c).

  python3 tests/owner_import_test.py        (from felucca-base; needs build/gen from DUELL/gen_only.py; host_tests.sh runs it as `ownerimport`)

1. round trip: all 128 factory sounds saved and read back as PATCH_RAW reads them, imported: 128 equal to the recipe sounds, nothing written;
2. three knobs turned in three slots: exactly three sounds change in the resulting factory data (and only the turned values), everything else identical;
3. the same import twice: the files are byte for byte the same; the unchanged export afterwards drops the entries again (undo);
4b. the editor's duell-multis file: same record bytes out as in; a wrong CRC is skipped;
4. a synthetic backup file with a changed multi: its record is kept byte for byte, listed in owner_multis.py, the unchanged neighbour is not;
5. refusals: a changed engine and a name another sound has are reported, not imported, exit code 1;
6. the editor's own parser (web/editor/patchfile.js) reads the synthetic export (when node is found).
"""
import base64
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"
fails = 0


def ok(c, what):
    global fails
    print("owner_import: %-110s %s" % (what, "ok" if c else "FAIL"))
    fails += not c


def load(p, n):
    spec = importlib.util.spec_from_file_location(n, p)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def run(*a):
    r = subprocess.run([sys.executable, str(TOOLS / "import_owner_edits.py"), *a], cwd=ROOT, capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


imp = load(TOOLS / "import_owner_edits.py", "imp")
ctx = imp.Ctx()
gen = ROOT / "build" / "host" / "owner_export_gen"
FV = [l for l in (ROOT / "build/gen/duell_version.h").read_text().splitlines() if "DUELL_VERSION_STR" in l][0].split('"')[1]
cc = ["cc", "-O1", "-w", "-DDUELL_TEST_MODE=1", "-Ibuild/gen_test", "-Ibuild/gen", "-Ifirmware/src", "-Itests", '-DFELUCCA_VERSION="%s"' % FV, "-o", str(gen), "tests/owner_export_gen.c", "-lm"]
(ROOT / "build/gen_test").mkdir(parents=True, exist_ok=True)
subprocess.run([sys.executable, str(TOOLS / "gen_duell_multis.py"), "build/gen_test/duell_multis.h", "--test-mode"], cwd=ROOT, capture_output=True)
ok(subprocess.run(cc, cwd=ROOT).returncode == 0, "tests/owner_export_gen.c compiles")
tmp = Path(tempfile.mkdtemp(prefix="owner_import_"))
P = ctx.ids
base = {}


def build(lst):
    return imp.build_list(ctx, lst)


# 1: round trip
a = tmp / "unchanged.json"
subprocess.run([str(gen), str(a)], cwd=ROOT, check=True, capture_output=True)
rc, out = run("--dry-run", "--out", str(tmp / "o1"), str(a))
ok(rc == 0 and "128 patches read (0 edited, 128 equal to the recipe sound)" in out, "round trip: the 128 factory sounds, saved and read back, import as equal to the recipe sounds, nothing edited")
j = json.loads(a.read_text())
ok(len(j["patches"]) == 128 and all(zlib.crc32(base64.b64decode(p["record"])) == int(p["crc32"], 16) for p in j["patches"]), "the synthetic export has 128 records, each with its checksum")

# 2: three knobs
edits = [(1, "P_REV", 33), (20, "P_CHOR", 11), (77, "P_DLY", 44)]
b = tmp / "turned.json"
subprocess.run([str(gen), str(b)] + [str(x) for s, n, v in edits for x in (s, P[n], v)], cwd=ROOT, check=True, capture_output=True)
o2 = tmp / "o2"
o2.mkdir()
rc, out = run("--out", str(o2), str(b))
ok(rc == 0 and "(3 edited, 125 equal" in out, "three knobs turned in three slots: three edited, 125 equal")
own = load(o2 / "owner_sounds.py", "own2").OWNER_SOUNDS
ok(len(own) == 3, "owner_sounds.py holds three sounds")
base_list = list(ctx.base.values())
fac = build(base_list)
merged = [dict(next((o for o in own if (o["engine"], o["idx"]) == k), s)) for k, s in ctx.base.items()]
new = build(merged)
diffs = {k: (fac[k], new[k]) for k in fac if dict(fac[k]["pairs"]) != dict(new[k]["pairs"]) or fac[k]["name"] != new[k]["name"] or fac[k]["bamt"] != new[k]["bamt"]}
ok(len(diffs) == 3, "the factory data built with the overlay differs from the recipes in exactly three sounds (%d)" % len(diffs))
hit = True
for slot, nm, v in edits:
    eng_i, idx = ctx.browse[slot - 1]
    f, n = diffs.get((eng_i, idx), (None, None))
    if not f:
        hit = False
        continue
    pf, pn = dict(f["pairs"]), dict(n["pairs"])
    ch = {k for k in set(pf) | set(pn) if pf.get(k, ctx.default(eng_i, k)) != pn.get(k, ctx.default(eng_i, k))}
    hit &= ch == {P[nm]} and pn[P[nm]] == v
ok(hit, "each changed sound differs in the turned value alone (REV 33 in #1, CHO 11 in #20, DLY 44 in #77)")
# 3: twice, then undo
before = (o2 / "owner_sounds.py").read_bytes()
run("--out", str(o2), str(b))
ok((o2 / "owner_sounds.py").read_bytes() == before, "the same import twice writes the same file")
rc, out = run("--out", str(o2), str(a))
ok(rc == 0 and "owner entry dropped" in out and load(o2 / "owner_sounds.py", "own3").OWNER_SOUNDS == [], "the unchanged export afterwards drops the entries (undo)")

# 4: a multi
fm = TOOLS / "factory_multis"
m3, m4 = (fm / "M03.bin").read_bytes(), (fm / "M04.bin").read_bytes()
m3x = bytearray(m3)
gbpm = 20 + 2 * ctx.fm.p_ids().get("G_BPM", 0) if False else None
m3x[20] ^= 0x01                                         # the first global (TEMPO): changed by one
mo = bytearray(8)
mo[0:4] = b"MUL1"
mo[4:6] = len(m3x).to_bytes(2, "little")
mo[6:8] = len(m4).to_bytes(2, "little")
obj = bytes(mo) + bytes(m3x) + m4                        # object 14 = M03, M04
bk = {"format": "duell-backup", "version": 4, "objects": [{"id": 14, "size": len(obj), "crc": zlib.crc32(obj), "data": base64.b64encode(obj).decode()}]}
(tmp / "backup.json").write_text(json.dumps(bk))
o4 = tmp / "o4"
o4.mkdir()
rc, out = run("--out", str(o4), str(tmp / "backup.json"))
ok(rc == 0 and "1 multis edited" in out and (o4 / "factory_multis" / "M03.bin").read_bytes() == bytes(m3x) and not (o4 / "factory_multis" / "M04.bin").exists(),
   "a backup file with a changed M03 and an unchanged M04: M03's record is kept byte for byte, M04 is not touched")
om = load(o4 / "owner_multis.py", "om4").OWNER_MULTIS
ok(len(om) == 1 and om[0]["slot"] == 3 and om[0]["name"] == "PAD SPLIT" and [p["sound"] for p in om[0]["parts"]] == ["EVOLVING PAD", "EMBER GLOW"] and om[0]["mode"] == "LAYER",
   "owner_multis.py lists M03 PAD SPLIT, LAYER, #61 EVOLVING PAD + #44 EMBER GLOW")

# 4b: the editor's own multi file (duell-multis, stored.js multiFile): export-format file -> importer -> the same record bytes
mf = {"format": "duell-multis", "version": 1, "firmware": FV, "created": "2026-01-01T00:00:00.000Z",
      "multis": [{"slot": 3, "label": "M03", "name": "PAD SPLIT", "version": m3x[0], "size": len(m3x), "crc": zlib.crc32(bytes(m3x)), "data": base64.b64encode(bytes(m3x)).decode()}]}
(tmp / "m.duell-multis.json").write_text(json.dumps(mf))
o4b = tmp / "o4b"
o4b.mkdir()
rc, out = run("--out", str(o4b), str(tmp / "m.duell-multis.json"))
ok(rc == 0 and "1 multis edited" in out and (o4b / "factory_multis" / "M03.bin").read_bytes() == bytes(m3x), "a duell-multis file (editor export format): M03's record comes out byte for byte")
mf["multis"][0]["crc"] ^= 1
(tmp / "bad.duell-multis.json").write_text(json.dumps(mf))
o4c = tmp / "o4c"
o4c.mkdir()
rc, out = run("--out", str(o4c), str(tmp / "bad.duell-multis.json"))
ok("checksum" in out and not (o4c / "factory_multis" / "M03.bin").exists(), "a duell-multis entry with a wrong CRC is skipped, nothing written")

# 5: refusals (a record of another engine in slot 1; a name of another sound)
j = json.loads(a.read_text())
j["patches"][0]["record"] = j["patches"][1]["record"]            # slot 1 gets slot 2's record (same engine ANALOG, name FOLD BASS = #2's name)
j["patches"][0]["crc32"] = j["patches"][1]["crc32"]
j["patches"][2]["record"] = j["patches"][90]["record"]           # slot 3 gets a record of another engine
j["patches"][2]["crc32"] = j["patches"][90]["crc32"]
(tmp / "bad.json").write_text(json.dumps(j))
rc, out = run("--dry-run", str(tmp / "bad.json"))
ok(rc == 1 and out.count("REFUSED") >= 2 and "already" in out and "identity" in out, "a name another sound has and a changed engine are REFUSED with a reason, exit code 1")

# 6: the editor's own parser
node = next((str(n) for n in sorted(Path.home().glob("emsdk/node/*/bin/node"))), None)
if node:
    js = tmp / "parse.mjs"
    js.write_text('import {readFileSync} from "node:fs"; import {parseFile} from "%s/web/editor/patchfile.js"; const r = parseFile(readFileSync(process.argv[2], "utf8")); console.log(r.ok, r.items.length, r.problems.length);' % ROOT)
    r = subprocess.run([node, str(js), str(a)], capture_output=True, text=True)
    ok(r.stdout.split() == ["true", "128", "0"], "the editor's patchfile.js parseFile reads the synthetic export: 128 records, no problems (%s)" % (r.stdout.strip() or r.stderr.strip()[-80:]))
else:
    print("owner_import: (no emsdk node: the editor parser check skipped)")
print("owner_import: all ok" if not fails else "owner_import: FAILED (%d)" % fails)
sys.exit(1 if fails else 0)
