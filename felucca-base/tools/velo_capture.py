#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""Record the raw key readings of a DUELL developer build with the velocity capture (docs/VELO_CSV.md).

    python3 tools/velo_capture.py                  guided session: you are told which key to play, how hard, how often
    python3 tools/velo_capture.py --free           free recording of 60 seconds (--seconds N for another length)
    python3 tools/velo_capture.py --port /dev/cu.usbmodemXXXX     if the unit's port is not found by itself

Needs no extra package (it talks to the serial port like tools/bench_run.py does). On the unit: KEY VELOCITY must be ON
(MENU > CONTROL) and the play screen showing. The recording lands in velo_captures/velo_YYYYmmdd_HHMMSS.csv (next to tools/).
Stop early with Ctrl-C: what was recorded so far is kept. Then run tools/velo_fit.py on the file."""
import argparse, datetime, glob, os, re, select, sys, termios, time

HERE = os.path.dirname(os.path.abspath(__file__))
NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
# F3 = key 0 ... C5 = key 19. F3, C4, G4, C5 as asked, plus two more white keys (A3, E4)
SESSION_KEYS = [0, 4, 7, 11, 14, 19]
STYLES = [("hard", "HARD", "hit it firmly, like a loud note"),
          ("normal", "NORMAL", "play it the way you normally would"),
          ("soft", "SOFT", "press it gently, as softly as it still sounds")]
PRESSES = 5


def key_name(k):
    m = 53 + k
    return f"{NAMES[m % 12]}{m // 12 - 1}"


class Port:
    def __init__(self, path):
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        a = termios.tcgetattr(self.fd)
        a[0], a[1], a[2], a[3] = 0, 0, termios.CS8 | termios.CREAD | termios.CLOCAL, 0
        a[4] = a[5] = termios.B115200
        termios.tcsetattr(self.fd, termios.TCSANOW, a)
        self.buf = b""

    def send(self, line):
        os.write(self.fd, line.encode("ascii") + b"\r")

    def lines(self, secs):
        out, end = [], time.monotonic() + secs
        while True:
            left = end - time.monotonic()
            r, _, _ = select.select([self.fd], [], [], max(left, 0))
            if r:
                d = os.read(self.fd, 4096)
                if not d:
                    raise EOFError("the port closed (cable out?)")
                self.buf += d
            while b"\n" in self.buf:
                ln, self.buf = self.buf.split(b"\n", 1)
                out.append(ln.decode("ascii", "replace").strip("\r "))
            if left <= 0 or out:
                return out

    def close(self):
        os.close(self.fd)


def find_port(given):
    if given:
        return given
    ports = sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))
    if len(ports) != 1:
        raise SystemExit(("No serial port of the unit found" if not ports else f"More than one port ({', '.join(ports)}): use --port PATH")
                         + ".\nIs the unit plugged in with USB and switched on? Is this the capture build (DUELL ... velocapture)?")
    return ports[0]


class Session:
    def __init__(self, f):
        self.f = f
        self.rows = self.notes = self.dropped = 0
        self.last_note_ts = 0
        self.key_presses = {}
        self.rows_by_key = {}
        self.status = None
        self.t_first_line = None

    def take(self, ln):
        """one line from the unit -> file; returns ('note', key) for a note-on, else None"""
        m = re.search(r"VC,(.*)$", ln)
        if not m:
            return None
        p = m.group(1).split(",")
        try:
            if p[0] == "V" and len(p) == 5:
                self.f.write(f"{p[1]},{p[2]},{p[3]},{p[4]}\n")
                self.rows += 1
                k = int(p[2])
                self.rows_by_key[k] = self.rows_by_key.get(k, 0) + 1
            elif p[0] == "N" and len(p) == 6:
                self.f.write(f"# note,{p[1]},{p[2]},{p[3]},{p[4]},{p[5]}\n")
                self.notes += 1
                self.last_note_ts = int(p[1])
                k = int(p[2])
                self.key_presses[k] = self.key_presses.get(k, 0) + 1
                return ("note", k)
            elif p[0] == "R" and len(p) == 4:
                self.f.write(f"# range,{p[1]},{p[2]},{p[3]}\n")
            elif p[0].startswith("dropped"):
                n = int(p[0].split()[1])
                self.dropped += n
                self.f.write(f"# dropped,{n}\n")
            elif p[0] == "S" and len(p) == 5:
                self.f.write(f"# status,{','.join(p[1:])}\n")
                self.status = p[1:]
        except (ValueError, IndexError):
            pass
        return None


def counter(sess, t0, extra=""):
    s = f"\r  {time.monotonic() - t0:5.0f} s | readings {sess.rows:6d} | key presses {sess.notes:3d} | lost {sess.dropped}  {extra}"
    sys.stdout.write(s.ljust(100)[:100])
    sys.stdout.flush()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--port", help="the unit's serial port (default: the only /dev/cu.usbmodem*)")
    ap.add_argument("--free", action="store_true", help="free recording instead of the guided session")
    ap.add_argument("--seconds", type=int, default=60, help="length of a free recording (default 60)")
    ap.add_argument("--countdown", type=float, default=3.0, help="seconds of get-ready before each block (default 3)")
    ap.add_argument("--out", help="folder (default: velo_captures/ next to tools/)")
    a = ap.parse_args(argv)
    outdir = a.out or os.path.join(os.path.dirname(HERE), "velo_captures")
    os.makedirs(outdir, exist_ok=True)
    path = os.path.join(outdir, "velo_" + datetime.datetime.now().strftime("%Y%m%d_%H%M%S") + ".csv")
    port = Port(find_port(a.port))
    print(f"Unit found. Recording into: {path}")
    f = open(path, "w")
    f.write("# DUELL key recording (docs/VELO_CSV.md), " + ("free" if a.free else "guided") + " session\nts_us,key,adc,drive\n")
    sess = Session(f)
    port.send("")
    port.lines(0.3)
    port.send("velocap on")
    ok = False
    end = time.monotonic() + 4
    while time.monotonic() < end and not ok:
        for ln in port.lines(0.2):
            if "OK velocap on" in ln:
                ok = True
            sess.take(ln)
    if not ok:
        print("The unit did not answer 'velocap on'. This is not the velocity capture build, or the console is off\n"
              "(MENU > SYSTEM > USB SERIAL must be ON). Nothing recorded.")
        f.close()
        os.remove(path)
        port.close()
        return 1
    print("Capture is on. Make sure KEY VELOCITY is ON (MENU > CONTROL) and the play screen is showing. Ctrl-C stops early.\n")
    t0 = time.monotonic()
    steps = []
    if not a.free:
        for k in SESSION_KEYS:
            for sid, big, how in STYLES:
                steps.append((k, sid, big, how))
    cur = -1
    step_start = 0.0
    step_count = 0
    step_done = 0.0
    warned_nodata = warned_nokv = False
    last_row_t = time.monotonic()
    try:
        while True:
            now = time.monotonic()
            if a.free and now - t0 >= a.seconds:
                break
            if not a.free:
                if cur < 0 or (step_count >= PRESSES and now - step_done > 1.2):
                    cur += 1
                    if cur >= len(steps):
                        break
                    k, sid, big, how = steps[cur]
                    f.write(f"# segment,{key_name(k)},{k},{sid},{sess.last_note_ts}\n")
                    sys.stdout.write("\r".ljust(100) + "\r")
                    print(f"\n[{cur + 1}/{len(steps)}] Play {key_name(k)} {big}, {PRESSES} times: {how}.")
                    print("      One press about every 2 seconds, lift your finger completely each time.")
                    cd = a.countdown
                    while cd > 0:
                        print(f"      get ready... {int(cd + 0.99)}", end="\r", flush=True)
                        for ln in port.lines(min(1.0, cd)):
                            sess.take(ln)
                        cd -= 1.0
                    print("      GO" + " " * 20)
                    step_start = time.monotonic()
                    step_count = 0
                    step_done = 0.0
            for ln in port.lines(0.1):
                r = sess.take(ln)
                if ln:
                    last_row_t = time.monotonic() if "VC,V" in ln else last_row_t
                if r and not a.free and cur >= 0 and step_count < PRESSES:
                    k, sid, big, how = steps[cur]
                    if r[1] == k:
                        step_count += 1
                        print(f"      {key_name(k)} {big}: press {step_count} of {PRESSES} counted" + ("  -> done, next one coming" if step_count >= PRESSES else ""))
                        if step_count >= PRESSES:
                            step_done = time.monotonic()
                    else:
                        print(f"      (that was {key_name(r[1])}, not {key_name(k)}: kept in the file, not counted)")
            if not a.free and cur >= 0 and step_count >= PRESSES and step_done == 0.0:
                step_done = time.monotonic()
            extra = ""
            if not a.free and 0 <= cur < len(steps):
                extra = f"now {key_name(steps[cur][0])} {steps[cur][2]} {step_count}/{PRESSES}"
            counter(sess, t0, extra)
            if not warned_nokv and sess.notes and sess.rows == 0 and time.monotonic() - t0 > 3:
                print("\n  Notes arrive but no readings: switch KEY VELOCITY ON (MENU > CONTROL).")
                warned_nokv = True
            if not warned_nodata and time.monotonic() - t0 > 8 and sess.notes == 0 and sess.rows == 0 and not a.free and cur >= 0:
                print("\n  Nothing has come in yet. Is the play screen showing and KEY VELOCITY ON? Is a key being pressed?")
                warned_nodata = True
    except KeyboardInterrupt:
        print("\nStopped early (Ctrl-C). What was recorded is kept.")
    except EOFError as e:
        print(f"\n{e}")
    try:
        port.send("velocap off")
        for ln in port.lines(0.5):
            sess.take(ln)
    except OSError:
        pass
    f.write(f"# end,{sess.rows},{sess.notes},{sess.dropped}\n")
    f.close()
    port.close()
    print(f"\n\nDone. {sess.rows} readings, {sess.notes} key presses, {sess.dropped} readings lost.")
    per = ", ".join(f"{key_name(k)} {n}" for k, n in sorted(sess.key_presses.items()))
    print(f"Presses per key: {per or 'none'}")
    if sess.dropped:
        print("Some readings were lost because the unit's queue was full (too many keys held at once). Tell me.")
    print(f"File: {path}\nNext: python3 tools/velo_fit.py \"{path}\"   and send the file back.")
    return 0 if sess.rows else 1


if __name__ == "__main__":
    sys.exit(main())
