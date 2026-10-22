# DUELL developer bench: how to use it (private)

> **Bench suite stage 1 (BENCHC1 and newer):** the automatic runner `felucca-base/tools/bench_run.py` drives the unit over the USB serial console (`bench ...` words; the console is on whatever MENU > USB SERIAL says), plays the tests of `tools/bench_suites/OWNER-1.txt` on the unit's own audio clock and writes the results. Read release_drafts/BENCH_STAGE1.md (in the DUELL folder). The K4 run of this guide still works.

Two developer packages, built from branch `wip/bench0226` (= the 0.2.25 test build T0225 plus the bench). Both say **DEVELOPER BENCH BUILD: not for normal use**.

| Package | What it does | Risk |
|---|---|---|
| **BENCH-A1** "stage timing" | Plays a fixed list of 88 settings one after the other and measures, for each, how much of the audio's time it takes on your unit. One core, nothing about the supply changed, core 1 never started. | Low: the sound engine is the normal one. |
| **BENCH-B1** "core 1 probe" | Everything of BENCH-A1, plus the second core: started only when you arm it, at M-VAVE's own supply levels, then 18 extra rows where core 1 runs a test load beside the audio, and a 10-minute stress run. | Higher: see "BENCH-B: the risks" below. |

**Flash BENCH-A1 first.** Run it, send the photos (and the log if you can). Only then BENCH-B1, if you want to.

- BENCH-A2 installer (A1 opened the GUIDE instead of the bench: use A2): <http://127.0.0.1:8792/bench/BENCH-A2/install/>
- BENCH-B3 installer (B2 + the flash-write fix, below): <http://127.0.0.1:8792/bench/BENCH-B3/install/>
- Back to the normal test build T0225: <http://127.0.0.1:8792/bench/latest/install/>

**This branch (`bench1`, the two-core line: stage1a + the bench, bench suite stage 0):** the package is **BENCHC0** (DUELL 0.2.25 bench BENCHC0, RAM placement on, one core): <http://127.0.0.1:8792/bench/BENCHC0/install/>. The same BENCH screen and the same 88 rows as BENCH-A2; K3 runs STRESS 10M (one core, no core 1 load); BENCH-B's core 1 rows are not in this line (two-core stage 1 brings core 1). The cost rows hold the NOTES budget open, the M16 rows play with it. The log has seven more columns at the end: notes1, notes2, why1, why2, budget, ramcode, boots. Details: release_drafts/BENCH_STAGE0.md.

Install exactly as for a test build (same installer page). Make a backup first on the installer page if you care about your saves.

## Before you start

- **Turn MASTER low** (about a quarter). The bench does not change the level itself (that would move the normal code around, see "What the numbers are"). Notes play at medium velocity.
- Unplug other MIDI gear and close any DAW that could send notes or clock to the unit.
- For the log: MENU > SYSTEM > USB SERIAL must be ON (it is OFF by default since 0.2.14: switch it on first). Bench builds only: the product has no console and no such row (memory trim 1); bench builds present the console whatever the row says.
- It takes about **7 minutes** (BENCH-A1) or about **8 minutes** (BENCH-B1). Do not touch the unit while it runs.

## Start it

1. Hold **HOME**: the MENU opens.
2. **SELECT** to the SYSTEM tab, to the row **BENCH** (in the bench builds it is in the place of GUIDE, just above ABOUT).
3. **K1 right** (or OCT+): the BENCH screen opens on page 1 (the state).
4. **K4 right** or **OCT+**: it starts. Page 1 shows the row it is on and the minutes left.
5. Hands off. **OCT-** stops it (the rows done so far stay).
6. At the end it says BENCH DONE and your session comes back as it was (your sounds, mode, master stages). Nothing of the bench is ever saved. Power off and on also brings back your last session.

**K1** turns the pages at any time. **OCT-** (not running) goes back to the menu's tabs; then HOME closes the menu. HOME and the page buttons do nothing on the BENCH screen.

If the unit restarts or crashes during the bench, it comes back in the normal way after the restart: the bench never starts by itself.

## What to photograph

After it is done: **page 1** (the state), then **every table page** with K1 (BENCH-A1: pages 2 to 8; BENCH-B1: pages 2 to 10). One photo per page, the whole screen.

## How to copy the log (instead of, or as well as, the photos)

`bench info` also tells how deep core 0's two stacks have been since power-on (`c0stack user/exception`, in bytes, of 24,320 / 7,936: painted at start-up, read back; memory trim 1, developer builds only). Each row is also sent as one text line to the USB serial console. On the Mac, in Terminal, in the DUELL folder:

```
python3 felucca-base/tools/bench_log.py
```

It finds the unit, then listens. Either start it before the bench (it records the lines as the rows are done), or after the bench: start it, then turn **K2 right** on the BENCH screen (SEND THE LOG). It writes `bench.csv` when the END line comes (Ctrl-C stops it earlier). Send me `bench.csv`. If you copied the lines from another terminal, `python3 felucca-base/tools/bench_log.py --from copied.txt` makes the CSV from them.

## The table

| Column | Meaning |
|---|---|
| ROW | the setting (list below) |
| AVG | the audio's average time per half buffer (2.9 ms) over the 3 s of the row, in %: the same figure as the CPU bar and ABOUT |
| PEAK | the longest single half of those 3 s, in %. Above 100 a half was late |
| MISS | halves that were late (a click or a dropout is possible) |
| MDL | what the host model (the Mac) predicts for exactly this row |
| X | AVG / MDL: the unit against the model (1.12 = the unit needs 12 % more than the model says) |

Colours: **red** name = halves missed; **amber** name = during that row the unit cut voices itself (the overload rule) or the overload guard turned LIGHT FX on; dim `--` = not run, `SKIP` = could not run.

BENCH-B1's core 1 pages (rows starting IDL / TYP / HVY with a load name) show **C0** (core 0's AVG), PEAK, MISS, **C1** (core 1's busy share of each half, %) and **LATE** (halves in which core 1 could not start its work in time: its missed deadlines). The busier core is the larger of C0 and C1.

The CSV has more: the voices that sounded (`vox`), the voices cut (`shed`), the share of time with LIGHT FX (`lfx`), the row it is compared with (`ref`) and the difference (`delta`, the stage's own cost on the unit) next to the model's (`model_delta`), the line of the host bench it matches (`base`), core 1's peak, its late and over-80 % halves, its work done per half (`c1units`), how often it was parked for a flash access (`c1holds`), the busier core, and CLEAN / MISSED / C1 FAILED.

## The rows

- **IDLE ST / LAYER / DUEL**: nothing playing (the mixer and the effect buses as at power-on). **NO FX ST**: the same with SENDS OFF (the mixer alone).
- **ANALOG 1 .. WAVE 4**: each engine's dearest factory sound (the one the model finds dearest: ANALOG GLASS SHARDS, FM6 HAN SOLO, PHASE WIRE HARP, LOFI WOW FLUTTER, VOICE VESPERS, TRIO CHIP CHAPEL, WHEEL LESLIE STORM, PHYS PAN WHISTLE, NOISE SEA ORGAN, WAVE PWM PAD), 1 and 4 notes, STEREO, no effect.
- **INS SOFT .. INS VINYL, SAT 64, EQ 1/2/3 BANDS, DUCK PUMP**: each part stage on one ANALOG voice (compare with ANALOG 1).
- **FLT OPEN / 12DB / 24DB**: the multimode filter on 4 WAVE voices.
- **BUS OFF / BUS ON / CHORUS / DL DIGI, PING, RAND, OCT+ / RV ROOM .. PLATE / SHIM DN**, each in STEREO (ST) and DUEL (DU): both parts one ANALOG voice, one effect fed at 100 (compare with BUS OFF). RV SHIM is SHIM with PITCH UP.
- **COMP 4:1, WIDTH 150, BASS MONO**: the master stages (STEREO).
- **TYP / HVY, LAY / ST / DUEL, 4+4 / 8+8**: both parts ANALOG GLASS SHARDS. Typical: PHASR insert, SAT, one EQ band, chorus, DIGITAL delay, HALL, COMP. Heavy: the same with RANDOM delay, SHIM PITCH DOWN with MOD 50, WIDTH and BASS MONO. **The voice pool has 8 voices**: 8 + 8 asked for plays 8 at most (the `vox` column says how many sounded); in LAYER 4 notes are 8 voices.
- **M16 1 / 2 / 3 KEYS**: the multi in slot 16 (M16 STRESS TEST unless you saved over it), LAYER, the keys C4, E4, G4 played as MIDI notes.
- BENCH-B1 only, **IDL / TYP / HVY + C1 IDLE, TIGHT 50, SRAM 50, FL8K 50, FL40K 50, FL40K 90**: core 0 plays IDLE ST, TYP ST 4+4 or HVY DUEL 8+8 while core 1 runs a test load for 50 % (or 90 %) of each half: TIGHT = a small loop that stays in the cache (pure computing), SRAM = reading and writing 8 KB of memory, FL8K / FL40K = running 8 KB / 40 KB of code from the flash (the 40 KB one cannot stay in the 32 KB cache that both cores share). Compare each with its C1 IDLE row: how much the cores slow each other.
- BENCH-B1 only, **STRESS 10M** (K3, not in the automatic run): HVY DUEL 8+8 for 10 minutes with core 1 at FL40K 90.

## What the numbers are (and are not)

- AVG / PEAK / MISS are measured on your unit, by the audio interrupt itself, exactly as the CPU bar does it. Nothing of the audio is held off for the bench: if the unit cuts voices or engages LIGHT FX, that is what it really does (the name turns amber).
- MDL is the Mac's instruction count for the same row (the scale of `./host_tests.sh bench`: 33 instructions a sample = 1 %). X is the calibration you asked for.
- The bench build is not byte-identical to T0225 (it cannot be: it has the bench in it). The audio code is the same code, the hot RAM block is identical, every audio function has the same size; in BENCH-A1 the engines' render code sits 18 to 96 bytes later in the flash than in T0225 (the bench's two hooks in the UI code). That can move the readings by a little (a cache effect, expected small, not measured); the differences between rows (a stage's cost) are not affected. BENCH-B1 moves the code a little more (its safety hooks), so compare B1's rows with B1's own C1 IDLE rows, not with BENCH-A1.

## Back to the normal build

Open <http://127.0.0.1:8792/bench/latest/install/> and install T0225 as usual. Your saves stay (the bench never writes any).

## BENCH-B: the risks, and what to do

What it does differently, only after you arm it:

1. On the BENCH screen **K3 right** once (it asks), **K3 right again**: the unit restarts at once (an unsaved change of the last seconds may be lost).
2. At that power-on only: the two supplies are raised to the levels M-VAVE's own firmware uses (VDC14 1.60 V, then the core supply SYSVDD 1.38 V; never higher), then core 1 is started before the audio. It waits in its own loop in RAM.
3. After the splash: "CORE 1 RUNNING: MENU > BENCH". Page 1 shows CORE 1 RUNNING and 1.38 V / 1.60 V.
4. **K4** or **OCT+** runs all rows (the 18 core 1 rows at the end). **K3** runs the 10-minute STRESS row.
5. The arm is used once: the next power-on is normal again (one core, the boot loader's supply).

The risks:

- On some FM-1s the second core reads memory wrong (reported by other firmwares; DUELL has never run it on your unit). The bench checks every job, a heartbeat, a test pattern each half and a deadline. Any problem: core 1 is stopped at once, the screen shows **CORE 1 FAILED, FELL BACK** with the reason, the rest of the core 1 rows are skipped, and the audio goes on with one core.
- A flash access (a save from the editor, a patch load) while core 1 runs its test code from the flash parks core 1 in RAM for that moment and then lets it go on: page 1 counts these as FLASH HOLDS. That is normal. Only a park that takes longer than 20 ms is a failure. (In BENCH-B2 the unit's own autosave wrote during the silent IDL rows and that ended the test as HELD FOR A FLASH WRITE; BENCH-B3 holds the autosave and the settings write while the bench runs.)
- Worse: a crash or a freeze of the whole unit. A crash shows the crash screen and restarts. A freeze is ended by the watchdog after about 8 s (the unit restarts). Either way the next start is normal (one core), and page 1 of the BENCH screen says **LAST TIME: CRASH WHILE IT RAN** or **WATCHDOG (HANG) WHILE IT RAN**. Please photograph that.
- The raised supply: these are the stock firmware's own levels, which every FM-1 runs under M-VAVE's firmware. DUELL raises them only for the armed session. Whether they stay after a restart (until you power off) is not known.

If the unit misbehaves:

- Stuck or silent and the watchdog does not restart it: power it off (unplug USB, switch off), wait 5 s, power on. It starts normally.
- It does not get past the splash, twice: the boot guard then starts the update mode by itself (black screen, the installer can still talk to it). Open the T0225 installer and install.
- At any time: hold **OCT- and OCT+** together for 5 s: the update mode (UBOOT). Then install T0225 with the installer.
- `docs/RECOVERY.md` has the rest (the way back to the official firmware).

## Where B1 and B2 stand

- **B1 (in BENCH-B1)**: core 1 runs a synthetic load each half (in the cache, in SRAM, from the flash) while core 0 runs real scenes. It answers the key unknown: how much the two cores slow each other through the shared instruction cache, the bus and the SPI flash, and whether your unit's core 1 runs clean at the stock supply.
- **B2 (not built)**: part 2's voices and DUEL's bus B rendered on core 1 for real (docs/CPU_PLAN.md stage 3). It still needs: the coupling between the parts untied (DUCK SRC PART, the morph and matrix values prepared before the parts, per-part scratch buffers, FM6's operator buses per core, the shared random generator); the hand-over in the audio interrupt every block with the deadline inside the half and core 0 rendering part 2 itself when core 1 is late; host tests with the job as a real thread (ThreadSanitizer) bit-identical to one core for every golden; and B1's numbers from your unit, to know whether running the engines from the flash on both cores pays at all.

## For the developer: how they are built

```
cd felucca-base
DUELL_BENCH=1 BENCH_TAG=A1 tools/build.py                        # build/duell-0.2.25-bench-A1.fwsc
DUELL_BENCH=1 DUELL_CORE1_PROBE=1 BENCH_TAG=B1 tools/build.py    # build/duell-0.2.25-bench-B1.fwsc
```

(with `JIELI_TOOLCHAIN`, `AC79_SDK`, and `DYLD_FALLBACK_LIBRARY_PATH=/opt/homebrew/lib` for the generators). A plain `tools/build.py` from the same branch gives the T0225 package byte for byte. The host side: `./host_tests.sh benchsim` (the bench run on the Mac); `BENCH_MODEL_UPDATE=1 felucca-base/build/host/bench_sim1` rewrites `firmware/src/bench_model.h` after a change of the sound code.
