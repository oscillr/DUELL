#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/gen_midi_map.py: the shipped csv and the legacy csv build; each of its failures fails (exit 1, a message), and a small change of the map is a
change of the csv only.   python3 -I tests/midimap_gen_test.py"""
import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
spec = importlib.util.spec_from_file_location("gen_midi_map", ROOT / "tools" / "gen_midi_map.py")
gen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)

fails = 0


def check(what, ok):
    global fails
    print("midimap_gen: %-110s %s" % (what, "ok" if ok else "FAIL"))
    fails += 0 if ok else 1


shipped = (ROOT.parent / "docs" / "MIDI_MAP_DEFAULTS.csv").read_text()
legacy = (HERE / "midimap_legacy.csv").read_text()
tmp = Path(tempfile.mkdtemp())


def run(text, expect):
    """-> True when the generator fails with a message containing `expect` (None: it must succeed)."""
    f = tmp / "t.csv"
    f.write_text(text)
    out = tmp / "t.h"
    if out.exists():
        out.unlink()
    r = subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "gen_midi_map.py"), "--csv", str(f), str(out)], capture_output=True, text=True)
    if expect is None:
        return r.returncode == 0 and out.exists()
    return r.returncode == 1 and expect in r.stderr and not out.exists()


def drop(text, cc):
    return "".join(l for l in text.splitlines(True) if not (l.startswith("%d," % cc) and ",REMOVED," not in l))


def edit(text, cc, new):
    return "".join((new + "\n") if (l.startswith("%d," % cc) and ",REMOVED," not in l) else l for l in text.splitlines(True))


lines, rev = gen.build(ROOT.parent / "docs" / "MIDI_MAP_DEFAULTS.csv")
direct = [c for c, v in lines.items() if v[2] != 3]
check("the shipped csv builds: 70 direct lines and the 23 locked numbers (MIDI_MAP_FINAL), a revision 1..255", len(direct) == 70 and len(lines) - len(direct) == 23 and 1 <= rev <= 255)
lines, _ = gen.build(HERE / "midimap_legacy.csv")
check("the legacy csv builds: today's 24 CCs and the 23 locked numbers", len([c for c, v in lines.items() if v[2] != 3]) == 24 and len(lines) == 47)
check("the shipped csv runs through the command line", run(shipped, None))

check("FAIL 1: a duplicate CC", run(shipped + "5,P_PAN,P,SHIPPED\n", "duplicate CC 5"))
check("FAIL 2: an unknown target", run(edit(shipped, 5, "5,P_NOSUCH,P,SHIPPED"), "unknown target 'P_NOSUCH'"))
check("FAIL 2: an unknown target (a value of core.h that is not a name: P_COUNT)", run(edit(shipped, 5, "5,P_COUNT,P,SHIPPED"), "unknown target 'P_COUNT'"))
check("FAIL 3: a locked standard number given to a parameter (64 sustain)", run(edit(shipped, 64, "64,P_ATK,P,SHIPPED"), "locked standard number"))
check("FAIL 3: a locked standard number given to a parameter (120 to a part value, tagged SHIPPED)", run(edit(shipped, 120, "120,P_PAN,P,SHIPPED"), "locked standard number"))
check("FAIL 3: LOCKED on a number that is not one of the 23", run(edit(shipped, 5, "5,LOCKED,P,LOCKED"), "not one of the 23"))
check("FAIL 4: one of today's 24 targets nowhere (CUTOFF_SMART dropped)", run(drop(shipped, 74), "CUTOFF_SMART"))
check("FAIL 4: one of today's 24 targets nowhere (MORPH dropped)", run(drop(shipped, 70), "MORPH"))
check("  .. unless a line says REMOVED on its old number (CC 20 said MORPH)", run(drop(shipped, 70) + "20,REMOVED,P,SHIPPED\n", None))
check("  .. REMOVED on a number that is not one of today's 24, or twice, is refused", run(drop(shipped, 70) + "30,REMOVED,P,SHIPPED\n", "not one of today's 24") and run(drop(shipped, 70) + "20,REMOVED,P,SHIPPED\n20,REMOVED,P,SHIPPED\n", "REMOVED twice"))
check("FAIL 5: a value above 127", run(shipped + "128,P_ATK,P,SHIPPED\n", "above 127"))
check("a parameter on two numbers is refused (one parameter has one CC)", run(edit(shipped, 18, "18,P_PAN,P,SHIPPED"), "P_PAN is on CC 10"))
check("a part value with the rule G, a global value with the rule P are refused", run(edit(shipped, 5, "5,P_GLIDE,G,SHIPPED"), "cannot have the part rule") and run(edit(shipped, 80, "80,G_CRATE,P,SHIPPED"), "cannot have the part rule"))
check("an unknown rule or tag is refused; a missing header too", run(edit(shipped, 5, "5,P_GLIDE,Q,SHIPPED"), "unknown part rule") and run(edit(shipped, 5, "5,P_GLIDE,P,MAYBE"), "unknown tag") and
      run("5,P_GLIDE,P,SHIPPED\n", "header"))
check("a locked number missing its line is refused", run(drop(shipped, 99), "locked standard number 99") or run(drop(shipped, 99), "locked"))
check("the same name list as the firmware: the five TWIN aliases, the extension slots and a global value are targets", run(
    edit(edit(edit(shipped, 21, "21,P_CUT2,P,SHIPPED"), 114, "114,X_CYMODE,P,SHIPPED"), 80, "80,G_CRATE,G,SHIPPED"), None))
sys.exit(1 if fails else 0)
