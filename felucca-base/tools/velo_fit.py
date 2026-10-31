#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""Read a key recording made by tools/velo_capture.py and suggest per-key settings (docs/VELO_CSV.md).

    tools/velo_fit.py velo_captures/velo_YYYYmmdd_HHMMSS.csv [--out DIR] [--bounce COUNTS]

Writes next to the recording (or into --out):  NAME_report.txt  (the short plain-text report, also printed),
NAME_presses.csv (one row per key press) and NAME_keys.csv (one row per key: the suggested settings).

How it works: the presses are found in the readings (a key reads about 1023 when open; a press lands when the key reads
below 420 with its row pulled up; the readings with the row driven right after it are the strength). The press labels
(hard / normal / soft) come from the guided session's markers in the file; a recording without markers is split per key
into its harder third, middle third and softer third. Each key's suggested hardest and softest readings are the LOWER QUARTILE
of its hard and the UPPER QUARTILE of its soft presses (item 123; they were the medians, which put half of the presses past the end of the scale). The velocities are then replayed through the firmware's own sense.c (tools/velo_replay.c,
nothing re-implemented): once as a fresh unit would play them, once with the suggested settings loaded."""
import argparse, csv, os, re, statistics, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import velo_replay  # noqa: E402

SEATED_MAX = 420      # sense_cal.h SC_SEATED_MAX: an undriven reading below this is a landed key
OPEN_MIN = 900        # SC_OPEN_MIN
GAP_US = 7000         # SC_GAP_US
D_MIN, D_MAX = 250, 1022   # SC_D_MIN / SC_D_MAX: driven readings that can be a pill (item 123: 760 before; the owner's unit reads 676 .. 1019)
STYLES = ("hard", "normal", "soft")
NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def key_name(k):
    m = 53 + k                      # key 0 = F3 (MIDI 53)
    return f"{NAMES[m % 12]}{m // 12 - 1}"


def med(v):
    return statistics.median(v) if v else None


def quart(v):
    v = sorted(v)
    n = len(v)
    return v[n // 4], v[n // 2], v[(3 * n) // 4]


class Unwrap:
    """ts_us is a 32 bit counter: make it increase"""
    def __init__(self):
        self.base, self.prev = 0, None

    def __call__(self, t):
        if self.prev is not None and t + self.base < self.prev - 2**31:
            self.base += 2**32
        self.prev = t + self.base
        return self.prev


def read_capture(path):
    rows, notes, segs, ranges, dropped = [], [], [], {}, 0
    uw = Unwrap()
    with open(path) as f:
        for ln in f:
            ln = ln.strip()
            if not ln or ln.startswith("ts_us"):
                continue
            if ln.startswith("#"):
                p = [x.strip() for x in ln[1:].split(",")]
                try:
                    if p[0] == "segment":
                        segs.append({"name": p[1], "key": int(p[2]), "style": p[3]})
                    elif p[0] == "note":
                        notes.append({"t": int(p[1]), "key": int(p[2]), "vel": int(p[3]), "floor": int(p[4]), "ceil": int(p[5]),
                                      "seg": len(segs) - 1})
                    elif p[0] == "range":
                        ranges[int(p[1])] = (int(p[2]), int(p[3]))
                    elif p[0] == "dropped":
                        dropped += int(p[1])
                except (ValueError, IndexError):
                    pass
                continue
            try:
                ts, key, adc, drv = (int(x) for x in ln.split(","))
            except ValueError:
                continue
            rows.append((uw(ts), key, adc, drv))
    return rows, notes, segs, ranges, dropped


def find_presses(rows):
    """-> list of dicts, one per press: key, t (landing, us), depth, d (first driven readings), hold"""
    by = {}
    for r in rows:
        by.setdefault(r[1], []).append(r)
    out = []
    for key, rs in sorted(by.items()):
        down, seen_open, open_run, last, p = False, False, 0, None, None

        def finish():
            nonlocal down, p, open_run
            if p is not None:
                dr = p.pop("_dr")
                p["d"] = [a for dt, a in dr if dt <= 20000][:3]
                hold = [a for dt, a in dr if 30000 <= dt <= 80000 and a < OPEN_MIN]
                p["hold"] = statistics.mean(hold) if hold else None
                out.append(p)
            down, p, open_run = False, None, 0
        for ts, _, adc, drv in rs:
            if down and last is not None and ts - last > GAP_US:
                finish()
            last = ts
            if not down:
                if adc >= OPEN_MIN:
                    seen_open = True
                elif not drv and adc < SEATED_MAX and seen_open:
                    down, open_run = True, 0
                    p = {"key": key, "t": ts, "depth": adc, "_dr": []}
            else:
                if adc >= OPEN_MIN:
                    open_run += 1
                    if open_run >= 2:
                        finish()
                        seen_open = True
                        continue
                else:
                    open_run = 0
                if drv:
                    p["_dr"].append((ts - p["t"], adc))
        if down:
            finish()
    out.sort(key=lambda p: p["t"])
    return out


def flag_suspects(presses, rows, guided, win=10000):
    """item 123: mark presses that may be mis-hits (big fingers, small keys). p["sus"] = list of reasons, empty = clean:
    a key that was not asked for; another key within 2 keys down at the landing or landing within `win` us of it; a second landing of the
    same key within 120 ms; a first reading lower than the next two by more than 25 (a bounce); an unsteady press (first three readings
    differ by more than 40 while the first is under 800)"""
    down, cur, last = {}, {}, {}
    for ts, k, a, d in rows:
        if d:
            continue
        if a < OPEN_MIN:
            cur.setdefault(k, ts)
        elif k in cur:
            down.setdefault(k, []).append((cur.pop(k), ts))
        last[k] = ts
    for k, t0 in cur.items():
        down.setdefault(k, []).append((t0, last[k]))
    for p in presses:
        r = []
        if guided and not p.get("style"):
            r.append("key not asked for")
        for k, iv in down.items():
            if k != p["key"] and abs(k - p["key"]) <= 2 and any(a - win <= p["t"] <= b or abs(a - p["t"]) <= win for a, b in iv):
                r.append("neighbour key down")
                break
        if any(q is not p and q["key"] == p["key"] and abs(q["t"] - p["t"]) < 120000 for q in presses):
            r.append("double landing")
        d = p["d"]
        if len(d) >= 3 and d[0] < min(d[1:3]) - 25:
            r.append("bounce")
        if len(d) >= 3 and max(d[:3]) - min(d[:3]) > 40 and d[0] < 800:
            r.append("unsteady")
        p["sus"] = r


def match_notes(presses, notes):
    """attach to each press the unit's note-on of the same key that follows its landing (within 60 ms)"""
    by = {}
    for n in notes:
        by.setdefault(n["key"], []).append(n)
    used = set()
    for p in presses:
        best = None
        for n in by.get(p["key"], []):
            if id(n) in used:
                continue
            dt = (n["t"] - p["t"]) & 0xFFFFFFFF
            if dt > 2**31:
                dt -= 2**32
            if -2000 <= dt <= 60000 and (best is None or abs(dt) < best[0]):
                best = (abs(dt), n)
        if best:
            used.add(id(best[1]))
            p["note"] = best[1]


def attach_velocity(presses, lands, field):
    by = {}
    for ts, key, vel, drv in lands:
        by.setdefault(key, []).append((ts, vel))
    for p in presses:
        best = None
        for ts, vel in by.get(p["key"], []):
            dt = abs(((ts - p["t"] + 2**31) & 0xFFFFFFFF) - 2**31)
            if dt <= 30000 and (best is None or dt < best[0]):
                best = (dt, vel)
        p[field] = best[1] if best else None


def d1_of(p):
    return p["d"][0] if p["d"] else None


def bounced(p, thr):
    d = p["d"]
    if len(d) < 2:
        return None
    if d[0] > D_MAX:                # (item 123: 900 .. 1022 is a very soft pill on this unit, not an open key; only the saturated 1023 is)
        return True
    return abs(d[0] - statistics.median(d[1:3])) > thr


def fmt_dist(v):
    if not v:
        return "      -"
    q1, m, q3 = quart(v)
    return f"{min(v):3d} {q1:3d} {m:3d} {q3:3d} {max(v):3d}"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("recording")
    ap.add_argument("--out", help="folder for the report and the CSV files (default: next to the recording)")
    ap.add_argument("--bounce", type=int, default=25, help="counts the first reading may differ from the next two before it is called off (default 25)")
    a = ap.parse_args(argv)
    rows, notes, segs, ranges, dropped = read_capture(a.recording)
    if not rows:
        print("No key readings in the file. Was KEY VELOCITY switched on (MENU > CONTROL) while recording?", file=sys.stderr)
        return 1
    presses = find_presses(rows)
    match_notes(presses, notes)
    guided = bool(segs)
    for p in presses:
        p["style"] = None
        n = p.get("note")
        if guided and n and 0 <= n["seg"] < len(segs) and segs[n["seg"]]["key"] == p["key"]:
            p["style"] = segs[n["seg"]]["style"]
    if not guided:      # terciles per key, by the first driven reading (lower reading = harder)
        for k in {p["key"] for p in presses}:
            ps = sorted([p for p in presses if p["key"] == k and d1_of(p) is not None], key=d1_of)
            for i, p in enumerate(ps):
                p["style"] = STYLES[min(2, i * 3 // len(ps))]
    flag_suspects(presses, rows, guided)
    usable = [p for p in presses if p["style"]]
    keys = sorted({p["key"] for p in presses})

    # ---- suggested settings (lower quartile of the hard presses, upper quartile of the soft ones)
    sug = {}
    for k in keys:
        h = [d1_of(p) for p in usable if p["key"] == k and p["style"] == "hard" and not p["sus"] and d1_of(p) is not None and D_MIN <= d1_of(p) <= D_MAX]
        s = [d1_of(p) for p in usable if p["key"] == k and p["style"] == "soft" and not p["sus"] and d1_of(p) is not None and D_MIN <= d1_of(p) <= D_MAX]
        if len(h) < 3 or len(s) < 3:
            sug[k] = (None, None, "too few usable hard or soft presses")
        elif med(s) < med(h) + 20:
            sug[k] = (None, None, "hard and soft presses read almost the same")
        else:
            sug[k] = (round(sorted(h)[len(h) // 4]), round(sorted(s)[(3 * len(s)) // 4]), "")

    base = os.path.splitext(os.path.basename(a.recording))[0]
    outdir = a.out or os.path.dirname(os.path.abspath(a.recording))
    os.makedirs(outdir, exist_ok=True)
    # ---- replays through the firmware's own sense core
    lands0, ranges0, _ = velo_replay.replay_full(a.recording)
    attach_velocity(presses, lands0, "vel_before")
    cal = os.path.join(tempfile.mkdtemp(), "cal.csv")
    with open(cal, "w") as f:
        for k, (lo, hi, why) in sug.items():
            if lo is not None:
                f.write(f"{k},{lo},{hi}\n")
    lands1, ranges1, _ = velo_replay.replay_full(a.recording, cal=cal)
    attach_velocity(presses, lands1, "vel_after")
    for p in presses:
        n = p.get("note")
        p["vel_unit"] = n["vel"] if n else None

    # ---- the report
    L = []
    w = L.append
    w(f"KEY VELOCITY FIT   {os.path.basename(a.recording)}")
    w("")
    w("1. What was recorded")
    w(f"   {len(rows)} key readings, {len(presses)} key presses found on {len(keys)} keys ({', '.join(key_name(k) for k in keys)}).")
    w(f"   Guided session markers: {'yes' if guided else 'no (presses split into harder / middle / softer third per key)'}.")
    if dropped:
        w(f"   WARNING: the unit dropped {dropped} readings because its queue was full; results may miss some presses.")
    unmatched = [p for p in presses if "note" not in p]
    if notes and unmatched:
        w(f"   {len(unmatched)} presses had no matching note-on from the unit (the unit may have held them back).")
    if not notes:
        w("   No note-on lines in the file: the column 'unit played' below stays empty.")
    w("")
    w("2. How each key reads when pressed (the number the key gives: LOWER = HARDER press)")
    w("   first reading after landing: lowest / middle / highest, per kind of press")
    w(f"   {'key':<5} " + "   ".join(f"{s:^22}" for s in STYLES))
    for k in keys:
        cells = []
        for s in STYLES:
            v = [d1_of(p) for p in usable if p["key"] == k and p["style"] == s and d1_of(p) is not None]
            cells.append(f"{len(v):2d}x {min(v):3d} / {int(med(v)):3d} / {max(v):3d}" if v else "-")
        w(f"   {key_name(k):<5} " + "   ".join(f"{c:^22}" for c in cells))
    w("")
    w("2b. Suspect presses (mis-hits: a key not asked for, a neighbour key down at the same moment, a double landing, a bounce, an unsteady press)")
    w(f"   {'key':<5} " + "   ".join(f"{s:^14}" for s in STYLES))
    repeat = []
    for k in keys:
        cells = []
        for s in STYLES:
            ps = [p for p in usable if p["key"] == k and p["style"] == s]
            bad = [p for p in ps if p["sus"]]
            cells.append(f"{len(bad)} of {len(ps)}")
            if ps and len(ps) - len(bad) < 5:
                repeat.append(f"{key_name(k)} {s} ({len(ps) - len(bad)} clean)")
        w(f"   {key_name(k):<5} " + "   ".join(f"{c:^14}" for c in cells))
    extra = [p for p in presses if not p["style"]]
    if extra:
        w(f"   {len(extra)} presses on keys that were not asked for: " + ", ".join(sorted({key_name(p['key']) for p in extra})))
    for s in STYLES:
        al = [d1_of(p) for p in usable if p["style"] == s and d1_of(p) is not None]
        cl = [d1_of(p) for p in usable if p["style"] == s and not p["sus"] and d1_of(p) is not None]
        if al and cl:
            w(f"   {s:<7} all {len(al):2d}: quarter {quart(al)[0]} middle {quart(al)[1]}   clean {len(cl):2d}: quarter {quart(cl)[0]} middle {quart(cl)[1]}")
    if repeat:
        w("   REPEAT THESE PROMPTS (fewer than 5 clean presses): " + "; ".join(repeat))
    w("")
    w(f"3. How often the first reading is wrong (it differs from the next two by more than {a.bounce} counts)")
    tot = tb = 0
    for k in keys:
        b = [bounced(p, a.bounce) for p in presses if p["key"] == k]
        b = [x for x in b if x is not None]
        tot += len(b)
        tb += sum(b)
        if b:
            w(f"   {key_name(k):<5} {sum(b):2d} of {len(b):2d}  ({100 * sum(b) / len(b):3.0f} %)")
    if tot:
        w(f"   all    {tb:2d} of {tot:2d}  ({100 * tb / tot:3.0f} %)")
    if tot:
        up = dn = 0
        for p in presses:
            d = p["d"]
            if len(d) >= 3:
                x = d[0] - statistics.median(d[1:3])
                up += x > a.bounce
                dn += x < -a.bounce
        w(f"   of those: {up} read HIGHER first (the finger is still pressing in: the reading falls as the force builds, normal and soft presses), {dn} read LOWER first (a real bounce)")
    rel = [p["hold"] - d1_of(p) for p in presses if p.get("hold") is not None and d1_of(p) is not None and d1_of(p) < OPEN_MIN]
    if rel:
        w(f"   After landing the reading settles by {statistics.median(rel):+.0f} counts (middle value) by 30 to 80 ms.")
    w("")
    w("4. Suggested settings for each key (lower quarter point of the hard presses, upper quarter point of the soft ones)")
    w(f"   {'key':<5} {'hardest':>8} {'softest':>8}   {'unit now':>14}   {'unit would use':>16}")
    for k in keys:
        lo, hi, why = sug[k]
        now = ranges.get(k)
        use = ranges1.get(k)
        w(f"   {key_name(k):<5} {lo if lo is not None else '-':>8} {hi if hi is not None else '-':>8}   "
          f"{(str(now[0]) + ' / ' + str(now[1])) if now else '-':>14}   {(str(use[0]) + ' / ' + str(use[1])) if lo is not None and use else why:>16}")
    w("   (floor / ceiling in counts. 'unit now' is what the unit held when the recording started.)")
    w("")
    w("5. Velocity (1 to 127) for hard, normal and soft presses: lowest / quarter / middle / three-quarter / highest")
    w("   'unit played' = what the unit really played during the recording")
    w("   'fresh unit' = the same readings replayed on a unit that knows nothing yet (as after ERASE)")
    w("   'with suggestions' = the same replay with the suggested settings loaded")
    w("   (the suggestions are measured on these same presses: the real test is to load them and play again)")
    for s in STYLES:
        ps = [p for p in usable if p["style"] == s]
        w(f"   {s.upper()} presses ({len(ps)})")
        for lab, f in (("unit played", "vel_unit"), ("fresh unit", "vel_before"), ("with suggestions", "vel_after")):
            v = [p[f] for p in ps if p[f] is not None]
            w(f"      {lab:<17} {fmt_dist(v)}   ({len(v)} counted)")
    w("")
    w("   Per key, middle velocity  hard / normal / soft   (fresh unit -> with suggestions)")
    for k in keys:
        cells = []
        for s in STYLES:
            ps = [p for p in usable if p["key"] == k and p["style"] == s]
            b = [p["vel_before"] for p in ps if p["vel_before"] is not None]
            af = [p["vel_after"] for p in ps if p["vel_after"] is not None]
            cells.append(f"{int(med(b)) if b else '-':>3}->{int(med(af)) if af else '-':<3}")
        w(f"   {key_name(k):<5} " + "    ".join(cells))
    w("")
    w("6. Send back")
    w(f"   the recording ({os.path.basename(a.recording)}) and this report. Nothing was changed on the unit.")
    text = "\n".join(L)
    print(text)
    with open(os.path.join(outdir, base + "_report.txt"), "w") as f:
        f.write(text + "\n")
    with open(os.path.join(outdir, base + "_presses.csv"), "w", newline="") as f:
        cw = csv.writer(f)
        cw.writerow(["key", "key_name", "kind", "landing_us", "landing_depth", "first_reading", "second_reading", "third_reading",
                     "first_reading_off", "unit_played", "fresh_unit", "with_suggestions"])
        for p in presses:
            d = p["d"] + [""] * (3 - len(p["d"]))
            cw.writerow([p["key"], key_name(p["key"]), p["style"] or "", p["t"], p["depth"], *d, bounced(p, a.bounce) if bounced(p, a.bounce) is not None else "",
                         p["vel_unit"] if p["vel_unit"] is not None else "", p["vel_before"] if p["vel_before"] is not None else "",
                         p["vel_after"] if p["vel_after"] is not None else ""])
    with open(os.path.join(outdir, base + "_keys.csv"), "w", newline="") as f:
        cw = csv.writer(f)
        cw.writerow(["key", "key_name", "hardest", "softest", "unit_floor", "unit_ceiling", "note"])
        for k in keys:
            lo, hi, why = sug[k]
            use = ranges1.get(k) if lo is not None else None
            cw.writerow([k, key_name(k), lo if lo is not None else "", hi if hi is not None else "", use[0] if use else "", use[1] if use else "", why])
    print(f"\nwritten: {os.path.join(outdir, base)}_report.txt  _presses.csv  _keys.csv", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
