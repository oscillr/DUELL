#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""MAX-VOICES (owner, 2026-11-08): how many voices the unit REALLY plays, the voice limits off (bench builds with BENCH_NOLIMIT=1: no budget,
no VOICE CAP, no shedding, no LIGHT FX), the same suite on the 1.1 code and on today's code.

  python3 tools/gen_maxvoice_suite.py [M1 "NAME1" M2 "NAME2" M3 "NAME3"]     writes tools/bench_suites/MAX-VOICES.txt

The test sounds are made by bench words in the unit's working memory only (nothing saved, no slot used): the engine's dearest factory sound with
everything on and MOVING: MULTI filter 24 dB, TWIN filter LP in PARALLEL (BLEND BOTH), SUB, the CYCLE envelope, both LFOs running smooth
(SIN, TRI) on pitch / filter / shape, ENV 2 on filter / pitch / shape, all 8 matrix slots from LFO 1, LFO 2, CYCLE and ENV 2 to PITCH, MULTI CUTOFF,
TIMBRE, AMP, PAN, SATURATION, TWIN CUTOFF, TWIN RES; SAT, an INSERT, SPREAD, the three sends; MORPH rolled to an End B, measured at A and at B.
EQ and DUCK off (the owner's rule G25a). Engines: LOFI (cheapest), ANALOG, WAVE, PHYSICAL (dearest); plus three of the owner's multis as stored.
Each test: both parts, keys added one at a time every 0.25 s, alternating part 1 / part 2 (LAYER multis: a key plays both parts), no long notes;
the first dropout (the recording's USB break / click / gap, the unit's late half) says how many voices were sounding.
"""
import sys
from pathlib import Path

ENGINES = [("LOFI", "LOFI", 16), ("ANALOG", "ANALOG", 16), ("WAVE", "WAVE", 16), ("PHYS", "PHYSICAL", 3)]
GLOBALS = ["mods ALL ON", "mods SENDS ON", "master COMP 4", "master WIDTH 150", "master BM 3",
           "set g 15 3", "set g 24 3", "set g 26 1", "set g 17 50"]
STEP = 0.25
NOTES = ["C", "D", "E", "F", "G", "A", "B"]


def note(k, start=2):
    return f"{NOTES[k % 7]}{start + k // 7}"


def moving(p):
    s = [
        f"set {p} 45 52", f"set {p} 46 96", f"set {p} 47 40", f"set {p} 48 2",          # MULTI: CUT, RES, MODE moved, 24 dB
        f"set {p} 43 1",                                                                  # TWIN TYPE: LP (a plain type)
        f"set {p} 106 40", f"set {p} 101 0", f"set {p} 128 1",                            # TWIN CUTOFF, BLEND BOTH, ROUTE PARALLEL
        f"set {p} 131 40", f"set {p} 132 50", f"set {p} 133 -50",                         # TWIN ENV 2 / LFO 1 / LFO 2 depths
        f"set {p} 134 127", f"set {p} 135 0", f"set {p} 136 3", f"set {p} 137 127",       # SUB: full level, -2 octaves, PULSE, HARMONICS
        f"set {p} 139 30", f"set {p} 140 30",                                             # CYCLE: rise and fall (it cycles)
        f"set {p} 9 80", f"set {p} 10 0", f"set {p} 77 60", f"set {p} 78 1",              # LFO 1 SIN, LFO 2 TRI, both running
        f"set {p} 5 40", f"set {p} 6 10", f"set {p} 7 40",                                # ENV 2 to filter, pitch, shape
        f"set {p} 13 6", f"set {p} 14 -40", f"set {p} 15 30",                             # LFO 1 to pitch (vibrato), filter, shape
    ]
    srcs = [1, 12, 15, 11, 1, 12, 15, 11]                                                 # LFO 1, LFO 2, CYCLE, ENV 2
    dsts = [1, 2, 3, 4, 5, 6, 51, 52]                                                     # PITCH, MULTI CUTOFF, TIMBRE, AMP, PAN, SATURATION, TWIN CUTOFF, TWIN RES
    amts = [8, 50, 50, 30, 60, 40, 50, 30]
    for k in range(8):
        b = 49 + 3 * k
        s += [f"set {p} {b} {srcs[k]}", f"set {p} {b + 1} {dsts[k]}", f"set {p} {b + 2} {amts[k]}"]
    s += [
        f"set {p} 33 50",                                                                 # SAT
        f"set {p} 96 3", f"set {p} 97 90", f"set {p} 98 90", f"set {p} 99 90", f"set {p} 100 127",   # an INSERT, its A B C, MIX
        f"set {p} 95 127",                                                                # SPREAD
        f"set {p} 34 100", f"set {p} 35 100", f"set {p} 36 100",                          # the three sends
        f"set {p} 2 90", f"set {p} 3 110", f"set {p} 4 40",                               # AMP ENV: held
        f"morph {p} ROLL 11 100",                                                         # MORPH: an End B
    ]
    return s


def ramp(nkeys, both):
    ev, t = [], 0.0
    for k in range(nkeys):
        ch = 1 if (not both or k % 2 == 0) else 2
        ev.append(f"at {t:.2f}  note {note(k // (2 if both else 1), 2 if ch == 1 else 3)} vel 110 len {nkeys * STEP - t + 0.5:.2f} ch {ch}")
        t += STEP
    end = nkeys * STEP + 0.5
    return ev, end


def one_part():
    """MAX-VOICES-PART (owner, 2026-11-08): ONE part plays alone (the other part has the same sound loaded and is silent), keys added one at a time
    every 0.25 s up to 12 (PHYSICAL 3): how many voices that part gets from the two cores. Part 1 alone and part 2 alone, MORPH A and B."""
    out = ["# " + one_part.__doc__.strip().replace("\n", "\n# "), "suite MAX-VOICES-PART", ""]
    for eng, shown, per in ENGINES:
        nk = 3 if eng == "PHYS" else 12
        for end, fader in (("A", 0), ("B", 127)):
            for part in (1, 2):
                ev = [f"at {k * STEP:.2f}  note {note(k, 2)} vel 110 len {nk * STEP - k * STEP + 0.5:.2f} ch {part}" for k in range(nk)]
                t_end = nk * STEP + 0.5
                lines = [f"test MP_{shown}_{end}_P{part}  {shown}, everything on and moving, MORPH {end}, part {part} ALONE, keys added one by one up to {nk}",
                         "mode STEREO", f"sound 1 {eng} DEAR", f"sound 2 {eng} DEAR", *GLOBALS, *moving(1), *moving(2),
                         f"morph 1 FADER {fader}", f"morph 2 FADER {fader}", "budget OPEN", "tune core1 on", *ev,
                         f"at {t_end:.2f}  release", "tail 0.5", f"measure 0.0 {t_end:.2f}", "expect report", ""]
                out.append("\n".join(lines))
    Path(__file__).resolve().parents[1].joinpath("tools/bench_suites/MAX-VOICES-PART.txt").write_text("\n".join(out))


GROUPS = [   # (owner, 2026-11-08) the MORPH B feature ladder: the moving test patch's lines by group, added one group per test
    ("MULTI", lambda p: [f"set {p} 45 52", f"set {p} 46 96", f"set {p} 47 40", f"set {p} 48 2"]),
    ("TWIN", lambda p: [f"set {p} 43 1", f"set {p} 106 40", f"set {p} 101 0", f"set {p} 128 1", f"set {p} 131 40", f"set {p} 132 50", f"set {p} 133 -50"]),
    ("SUB", lambda p: [f"set {p} 134 127", f"set {p} 135 0", f"set {p} 136 3", f"set {p} 137 127"]),
    ("MOD", lambda p: [f"set {p} 139 30", f"set {p} 140 30", f"set {p} 9 80", f"set {p} 10 0", f"set {p} 77 60", f"set {p} 78 1",
                       f"set {p} 5 40", f"set {p} 6 10", f"set {p} 7 40", f"set {p} 13 6", f"set {p} 14 -40", f"set {p} 15 30"]),
    ("MATRIX", lambda p: [l for l in moving(p) if any(l.startswith(f"set {p} {b} ") for b in range(49, 73))]),
    ("FX", lambda p: [f"set {p} 33 50", f"set {p} 96 3", f"set {p} 97 90", f"set {p} 98 90", f"set {p} 99 90", f"set {p} 100 127",
                      f"set {p} 95 127", f"set {p} 34 100", f"set {p} 35 100", f"set {p} 36 100"]),
]


def ladder():
    """B-LADDER (owner, 2026-11-08): what each feature group costs a voice at MORPH B, on the unit: part 1 alone, 4 keys held (below the overload, so the
    load can be read), one feature group added per test, the End B rolled after the groups and the fader at B; the full patch at A too"""
    out = ["# " + ladder.__doc__.strip().replace("\n", "\n# "), "suite B-LADDER", ""]
    ev = [f"at {k * STEP:.2f}  note {note(k, 2)} vel 110 len {3.0 - k * STEP:.2f} ch 1" for k in range(4)]
    for eng, shown in (("ANALOG", "ANALOG"), ("WAVE", "WAVE")):
        steps = [("PLAIN", [])]
        acc = []
        for name, fn in GROUPS:
            acc.append(fn)
            steps.append((name, list(acc)))
        for i, (name, fns) in enumerate(steps):
            for end, fader in (("B", 127),) + ((("A", 0),) if name == "FX" else ()):
                lines = [f"test BL_{shown}_{i}{name}_{end}  {shown}, part 1 alone, 4 keys, up to {name} on, MORPH {end}",
                         "mode STEREO", f"sound 1 {eng} DEAR", f"sound 2 {eng} DEAR", *GLOBALS,
                         f"set 1 2 90", f"set 1 3 110", f"set 1 4 40", f"set 2 2 90", f"set 2 3 110", f"set 2 4 40"]
                for fn in fns:
                    lines += fn(1) + fn(2)
                lines += ["morph 1 ROLL 11 100", "morph 2 ROLL 11 100", f"morph 1 FADER {fader}", f"morph 2 FADER {fader}",
                          "budget OPEN", "tune core1 on", *ev, "at 3.00  release", "tail 0.3", "measure 1.5 3.0", "expect report", ""]
                out.append("\n".join(lines))
    Path(__file__).resolve().parents[1].joinpath("tools/bench_suites/B-LADDER.txt").write_text("\n".join(out))


def main():
    ladder()
    one_part()
    out = [__doc__.replace('"""', "").strip().replace("\n", "\n# ").join(["# ", ""]), "suite MAX-VOICES", ""]
    for eng, shown, per in ENGINES:
        for end, fader in (("A", 0), ("B", 127)):
            ev, t_end = ramp(2 * per, True)
            lines = [f"test MV_{shown}_{end}  {shown}, everything on and moving, MORPH {end}, both parts, keys added one by one up to {per} + {per}",
                     "mode STEREO", f"sound 1 {eng} DEAR", f"sound 2 {eng} DEAR", *GLOBALS, *moving(1), *moving(2),
                     f"morph 1 FADER {fader}", f"morph 2 FADER {fader}", "budget OPEN", "tune core1 on", *ev,
                     f"at {t_end:.2f}  release", "tail 0.5", f"measure 0.0 {t_end:.2f}", "expect report", ""]
            out.append("\n".join(lines))
    a = sys.argv[1:]
    for i in range(0, len(a) - 1, 2):
        slot, name = a[i], a[i + 1]
        ev, t = [], 0.0                                                                   # (low and high keys in turn: a SPLIT multi gets both parts too)
        for k in range(16):
            nm = note(k // 2, 2) if k % 2 == 0 else note(k // 2 + 4, 4)
            ev.append(f"at {t:.2f}  note {nm} vel 110 len {16 * STEP - t + 0.5:.2f} ch 1")
            t += STEP
        t_end = 16 * STEP + 0.5
        lines = [f"test MV_M{int(slot):02d}  M{int(slot):02d} {name} as stored, keys added one by one up to 16 (each key both parts)",
                 f'load multi {slot} "{name}"', "budget OPEN", "tune core1 on", *ev,
                 f"at {t_end:.2f}  release", "tail 0.5", f"measure 0.0 {t_end:.2f}", "expect report", ""]
        out.append("\n".join(lines))
    Path(__file__).resolve().parents[1].joinpath("tools/bench_suites/MAX-VOICES.txt").write_text("\n".join(out))


if __name__ == "__main__":
    main()
