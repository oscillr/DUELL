#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""TEST ONLY, no unit needed: a made-up DUELL capture unit for tools/velo_capture.py and tools/velo_fit.py.

    tools/velo_fake_unit.py --serve         opens a fake serial port (prints its path); run velo_capture.py --port PATH --countdown 0.2
    tools/velo_fake_unit.py --csv OUT.csv   writes the recording a guided session would make, in the capture file's format

The hand is a simplified version of tests/velo_pill.h (a pill per key with its own hardest reading and span, a bounce now and
then, the strength settling after landing). The velocities in the note lines come from the firmware's own sense.c (velo_replay.c)."""
import argparse, os, pty, random, select, sys, tempfile, termios, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import velo_replay  # noqa: E402
import velo_capture  # noqa: E402

STRENGTH = {"hard": (0.93, 0.05), "normal": (0.60, 0.10), "soft": (0.22, 0.07)}


def make_session(seed=7, keys=None, bounce_p=0.12):
    """-> rows [(ts,key,adc,drive)], plan [(key, style)] in order, landings-by-press"""
    rnd = random.Random(seed)
    keys = keys or velo_capture.SESSION_KEYS
    pill = {k: {"hard": rnd.uniform(389, 510), "span": rnd.uniform(100, 150), "rest": rnd.uniform(170, 200)} for k in keys}
    rows, plan, t = [], [], 1_000_000
    for k in keys:
        for sid, _, _ in velo_capture.STYLES:
            for i in range(velo_capture.PRESSES):
                plan.append((k, sid))
                mu, sd = STRENGTH[sid]
                s = min(1.1, max(0.0, rnd.gauss(mu, sd)))
                pl = pill[k]
                t += rnd.randint(1_500_000, 2_000_000)
                for j in range(3):                      # open readings before
                    rows.append((t + j * 2200, k, 1023, 0))
                tc = t + 3 * 2200 + rnd.randint(0, 2000)
                hold_n = rnd.randint(30, 90)
                bounce = rnd.random() < bounce_p
                for j in range(hold_n):
                    tt = tc + j * 2200
                    dt = tt - tc
                    if j % 2 == 0 and j > 0:            # driven (the first one right after the landing, then every second reading)
                        pass
                    drive = 1 if j % 2 == 1 else 0
                    se = s * (0.75 + 0.25 * pow(2.718, -dt / 15000))
                    if drive:
                        v = pl["hard"] + pl["span"] * (1 - se) + rnd.gauss(0, 4)
                        if j == 1 and bounce:
                            v = rnd.uniform(650, 1023)
                    else:
                        v = pl["rest"] + (185 + 175 * (1 - s) - pl["rest"]) * pow(2.718, -dt / 30000) + rnd.gauss(0, 3)
                    rows.append((tt, k, int(max(0, min(1023, v))), drive))
                for j in range(5):                      # release
                    rows.append((tc + (hold_n + j) * 2200, k, 1023, 0))
                t = tc + (hold_n + 5) * 2200
    rows.sort(key=lambda r: r[0])
    return rows, plan


def write_rows(path, rows):
    with open(path, "w") as f:
        f.write("ts_us,key,adc,drive\n")
        for r in rows:
            f.write("%d,%d,%d,%d\n" % r)


def notes_for(rows):
    """the unit's note-ons: one per landing the firmware's sense core makes, with the key's range at that moment (cold replay, so the
    range shown is the typical key's: 417 / 593)"""
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, "r.csv")
        write_rows(p, rows)
        lands, _, _ = velo_replay.replay_full(p)
    return [(ts + 4000, k, v, 417, 593) for ts, k, v, _ in lands]


def write_capture_csv(path, seed):
    rows, plan = make_session(seed)
    notes = notes_for(rows)
    f = open(path, "w")
    f.write("# fake capture\nts_us,key,adc,drive\n")
    ev = [(r[0], 1, r) for r in rows] + [(n[0], 0, n) for n in notes]
    ev.sort(key=lambda e: (e[0], e[1]))
    pi, cnt = -1, 0
    seg_left = 0
    for ts, kind, x in ev:
        if kind == 0:
            if seg_left == 0:
                pi += 5 if pi >= 0 else 0
                pi = max(pi, 0) if pi < 0 else pi
                k, sid = plan[min(len(plan) - 1, cnt)]
                f.write(f"# segment,{velo_capture.key_name(k)},{k},{sid},{0}\n")
                seg_left = velo_capture.PRESSES
            seg_left -= 1
            cnt += 1
            f.write(f"# note,{x[0]},{x[1]},{x[2]},{x[3]},{x[4]}\n")
        else:
            f.write("%d,%d,%d,%d\n" % x)
    f.close()


def serve(seed, speed):
    rows, plan = make_session(seed)
    notes = notes_for(rows)
    m, s = pty.openpty()
    a = termios.tcgetattr(m)
    a[3] = 0
    termios.tcsetattr(m, termios.TCSANOW, a)
    print("fake unit port:", os.ttyname(s), flush=True)
    out = []
    for k in range(20):
        out.append(f"VC,R,{k},417,593\r\n")
    ev = [(r[0], 1, r) for r in rows] + [(n[0], 0, n) for n in notes]
    ev.sort(key=lambda e: (e[0], e[1]))
    on, i, buf, nsent = False, 0, b"", 0
    t0, ts0 = time.monotonic(), ev[0][0]
    while True:
        r, _, _ = select.select([m], [], [], 0.005)
        if r:
            buf += os.read(m, 256)
            while b"\r" in buf:
                ln, buf = buf.split(b"\r", 1)
                w = ln.decode().strip()
                if w == "velocap on":
                    on = True
                    os.write(m, b"OK velocap on\r\n> ")
                    for l in out:
                        os.write(m, l.encode())
                    t0, ts0 = time.monotonic(), ev[i][0] if i < len(ev) else 0
                elif w == "velocap off":
                    os.write(m, b"OK velocap off\r\n> ")
                    return
        if on and i < len(ev):
            while i < len(ev) and (ev[i][0] - ts0) / 1e6 / speed <= time.monotonic() - t0:
                ts, kind, x = ev[i]
                os.write(m, (("VC,V,%d,%d,%d,%d\r\n" % x) if kind else ("VC,N,%d,%d,%d,%d,%d\r\n" % x)).encode())
                i += 1
                if not kind:
                    nsent += 1
                    if nsent % 5 == 0:                   # the player waits for the next prompt: 2.5 s real time (the unit's own clock stands still)
                        end = time.monotonic() + 2.5
                        while time.monotonic() < end:
                            select.select([m], [], [], 0.05)
                        t0 += 2.5
                        break


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--serve", action="store_true")
    ap.add_argument("--csv")
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--speed", type=float, default=20.0, help="how much faster than real time the fake plays (serve)")
    a = ap.parse_args()
    if a.csv:
        write_capture_csv(a.csv, a.seed)
    elif a.serve:
        serve(a.seed, a.speed)
    else:
        ap.print_help()
