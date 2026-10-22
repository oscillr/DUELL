<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Key recording format (VELO_CSV)

The raw logger on the unit and `felucca-base/tools/velo_replay.py` use one CSV, one row per converter reading of a key:

```
ts_us,key,adc,drive
```

- `ts_us`: the time of the reading in microseconds (unsigned 32 bit, wraps after 71 minutes; only differences are used).
- `key`: the note key 0 to 26 (F3 = 0 ... G5 = 26). Keys 20 to 26 have no converter and are ignored.
- `adc`: the converter value, 0 to 1023.
- `drive`: 1 for a reading taken with the key's row driven, 0 for the row pulled up.

A header line, blank lines and lines starting with `#` are skipped. Rows must be in time order. Leave the readings in: a key that is open reads about 1023 and that is how a release is seen. Skipping stretches is fine as long as a few open readings of the key come before a contact and after a release (a gap of more than 7 ms between two readings of a key counts as a gap, and a landing straight after a gap has an unknown strength).

`velo_replay.py RECORDING.csv` runs the firmware's own sense core (`firmware/src/sense.c`) over it from a cold start (the typical key, nothing stored) and prints, per key and overall, the number of landings, the velocity quartiles, and the share at 110 or more and below 10. `--landings OUT.csv` also writes every landing (`ts_us,key,velocity,from_driven`). `--curve PCT` sets the MENU curve (0 = flat 100, 100 = the measured range).

`tests/velo_test.c --csv FILE [REF]` writes such a recording from the simulated hand (and the landings the core made of it); `host_tests.sh velo` replays it and requires the same landings.
