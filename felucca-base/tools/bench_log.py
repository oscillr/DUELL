#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""DUELL BENCH log receiver (developer bench builds BENCH-A / BENCH-B; docs/BENCH_GUIDE.md).

The bench (firmware/src/bench.c) writes one plain text line per row to the unit's USB serial console
("BENCH,..." CSV): while it runs, and again for every row when K2 is turned on the BENCH screen. This
listens on that serial port and writes the rows as a CSV file. No extra Python packages: only the
standard library (termios), macOS or Linux.

  python3 tools/bench_log.py                     find the unit's port (/dev/cu.usbmodem*), listen until BENCH,END
  python3 tools/bench_log.py --port /dev/cu.usbmodem1101 --out run1.csv
  python3 tools/bench_log.py --from copied.txt   the lines copied from a terminal (screen, the Arduino monitor ...)

MENU > SYSTEM > USB SERIAL must be ON on the unit (OFF by default since 0.2.14: switch it on first)."""
import argparse
import glob
import os
import select
import sys
import termios
import time


def rows_of(lines):
    """the BENCH lines -> (info lines, header fields, rows (lists of fields), ended)"""
    info, head, rows, ended = [], None, [], False
    for ln in lines:
        ln = ln.strip()
        if not ln.startswith("BENCH,"):
            continue
        f = ln.split(",")
        if f[1] == "INFO":
            info.append(",".join(f[2:]))
        elif f[1] == "#":
            head = f[1:]
        elif f[1] == "END":
            ended = True
        elif f[1].isdigit():
            rows.append(f[1:])
    return info, head, rows, ended


def write_csv(path, info, head, rows):
    """the newest line of each row wins (a run and a K2 dump may both be in the stream)"""
    by = {}
    for r in rows:
        by[int(r[0])] = r
    with open(path, "w") as o:
        for i in info[-1:]:
            o.write("# " + i + "\n")
        if head:
            o.write(",".join("row" if h == "#" else h for h in head) + "\n")
        for k in sorted(by):
            o.write(",".join(by[k]) + "\n")
    return len(by)


def listen(port, timeout_s, lines):
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        a = termios.tcgetattr(fd)
        a[0] = 0                                      # iflag: raw
        a[1] = 0                                      # oflag
        a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        a[3] = 0                                      # lflag: no echo, no canonical mode
        a[4] = a[5] = termios.B115200                 # (the unit ignores the rate)
        termios.tcsetattr(fd, termios.TCSANOW, a)
        buf, t_last = b"", time.time()
        print(f"listening on {port}: start the bench (K4 or OCT+) or turn K2 on the BENCH screen to send the log; Ctrl-C ends")
        while True:
            r, _, _ = select.select([fd], [], [], 1.0)
            if r:
                try:
                    d = os.read(fd, 4096)
                except BlockingIOError:
                    d = b""
                if d:
                    t_last = time.time()
                    buf += d
                    while b"\n" in buf:
                        ln, buf = buf.split(b"\n", 1)
                        s = ln.decode("ascii", "replace").strip()
                        if s.startswith("BENCH,"):
                            print(s)
                            lines.append(s)
                            if s.startswith("BENCH,END"):
                                return
            if timeout_s and time.time() - t_last > timeout_s:
                print(f"nothing for {timeout_s} s: stopped")
                return
    finally:
        os.close(fd)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--port", help="the unit's serial port (default: the first /dev/cu.usbmodem* or /dev/ttyACM*)")
    ap.add_argument("--out", default="bench.csv", help="the CSV to write (default bench.csv)")
    ap.add_argument("--from", dest="src", help="read copied text instead of the port")
    ap.add_argument("--timeout", type=int, default=0, help="stop after this many silent seconds (default: wait for BENCH,END)")
    a = ap.parse_args()
    if a.src:
        with open(a.src, errors="replace") as f:
            lines = f.read().splitlines()
    else:
        port = a.port or next(iter(sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))), None)
        if not port:
            sys.exit("no serial port of the unit found: is it connected, is MENU > SYSTEM > USB SERIAL ON? (or --port)")
        lines = []
        try:
            listen(port, a.timeout, lines)
        except KeyboardInterrupt:
            print()
    info, head, rows, ended = rows_of(lines)
    if not rows:
        sys.exit("no BENCH rows found")
    n = write_csv(a.out, info, head, rows)
    print(f"{n} rows -> {a.out}" + ("" if ended else " (no BENCH,END seen: the run may be incomplete)"))


if __name__ == "__main__":
    main()
