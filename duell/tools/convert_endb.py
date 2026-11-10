#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL: give the pre-loaded patches (tools/duell_sounds.py) their End B as exact values (owner 2026-10-29), once.

    tools/convert_endb.py [--mode split|stereo] [--write]

For each of the 128 patches the FIRMWARE's own code (tests/gen_sound_endb.c, compiled for the host) loads it as the PATCHES list does (End B rolled from its seed and
AMOUNT, the way the unit makes it today) and SAVES it as a patch record (upreset.c, PT_VER 4). The End B block (MBV) of that record is read (tools/endb.py) and written
into the sound as `bend` (the values that differ from the Original), `bpin` (marks, none for a rolled B) and `bfm` (FM6). Checked before anything is written: the block's AMOUNT is the sound's; the block packed again by tools/endb.py is byte for byte the firmware's.
--mode: the unit's MODE at the load. A roll depends on it (STEREO also moves PAN and SPREAD, and rolls the rest differently): default split = the power-on mode (DUEL).
Without --write only reports. A sound that has `bend` already is left as it is. The seeds ('bseed') stay in the data: the firmware reads them until it reads the blocks.
Afterwards: tools/gen_duell_sounds.py build/gen/duell_sounds.h, then `build/host/gen_sound_endb verify` (the old way against the new way, 0 differences)."""
import argparse
import importlib.util
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))
import endb


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def compile_harness():
    exe = ROOT / "build" / "host" / "gen_sound_endb"
    exe.parent.mkdir(parents=True, exist_ok=True)
    cc = ["cc", "-O1", "-w", "-DDUELL_TEST_MODE=1", "-Ibuild/gen_test", "-Ibuild/gen", "-Ifirmware/src", "-Itests", '-DFELUCCA_VERSION="x"',
          "-o", str(exe), "tests/gen_sound_endb.c", "-lm"]
    if subprocess.run(cc, cwd=ROOT).returncode:
        sys.exit("convert_endb: tests/gen_sound_endb.c does not compile")
    return exe


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--mode", choices=["split", "stereo"], default="split")
    ap.add_argument("--write", action="store_true")
    a = ap.parse_args(argv)
    gs = load(TOOLS / "gen_duell_sounds.py", "gen_duell_sounds")
    ioe = load(TOOLS / "import_owner_edits.py", "import_owner_edits")
    data = load(TOOLS / "duell_sounds.py", "duell_sounds")
    ctx = ioe.Ctx()
    sounds = {(x["e"], x["idx"]): x for x in gs.build(type("D", (), {"SOUNDS": data.SOUNDS, "NO_OWNER": True}))[0]}
    ids = gs.parse_ids()
    tp, names = gs.parse_tp(ids)
    wlabel = gs.bend_labels(ids, gs.parse_xtp(names))
    exe = compile_harness()
    out = subprocess.run([str(exe), "dump"] + (["stereo"] if a.mode == "stereo" else []), cwd=ROOT, capture_output=True, text=True, check=True).stdout
    recs, orig = {}, {}
    for l in out.splitlines():
        f = l.split()
        key = (int(f[1]), int(f[2]))
        if f[0] == "R":
            recs[key] = bytes.fromhex(f[4])
        else:
            orig[key] = [int(x) for x in f[3:]]
    bad = []
    text = (TOOLS / "duell_sounds.py").read_text()
    n_new = n_vals = n_fm = 0
    for key in sorted(recs):
        x = sounds[key]
        who = "%s %d %s" % (ctx.eng_names[key[0]], key[1], x["name"])
        rec = ioe.parse_record(ctx, recs[key])
        if not rec["mb"]:
            bad.append(who + ": the firmware saved no End B block")
            continue
        d = endb.decode(rec["mb"], key[0])
        if d["size"] != len(rec["mb"]):
            bad.append(who + ": block length")
            continue
        if d["amt"] != x["bamt"] + 1:
            bad.append("%s: the block's AMOUNT %d, the sound's %d" % (who, d["amt"] - 1, x["bamt"]))
            continue
        ref = {w: orig[key][w] for w in wlabel}
        again = endb.encode(key[0], d["amt"], d["vals"], ref, d["marks"], d["fm"])
        if again != bytes(rec["mb"]):
            bad.append(who + ": tools/endb.py packs the block differently from the firmware")
            continue
        n_new += 1
        n_vals += len(d["vals"])
        n_fm += d["fm"] is not None
        kw = endb.sound_kwargs({wlabel[w]: v for w, v in sorted(d["vals"].items())}, [wlabel[w] for w in d["marks"]], d["fm"])
        m = re.search(r"dict\(engine=%r, idx=%d,.*?\n         \}\)," % (ctx.eng_names[key[0]], key[1]), text, re.S)
        if not m or "bend=" in m.group(0):
            if m:
                n_new -= 1
                continue
            bad.append(who + ": not found in duell_sounds.py")
            continue
        new = m.group(0)[:-len("\n         }),")] + "\n         },\n         " + ",\n         ".join(kw) + "),"
        text = text[:m.start()] + new + text[m.end():]
    print("convert_endb (%s): %d patches read, %d to write, %d End B values in all, %d with FM6 bytes" % (a.mode, len(recs), n_new, n_vals, n_fm))
    for b in bad:
        print("  PROBLEM: " + b)
    if bad:
        return 1
    if a.write:
        (TOOLS / "duell_sounds.py").write_text(text)
        print("  wrote tools/duell_sounds.py")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
