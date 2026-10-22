# TWO-CORE diagnosis (BENCHT2): what core 1's jobs measure, and the runtime switches

Branch `twocore3` (from `twocore2`). Developer bench builds only (`DUELL_BENCH=1`, which turns on `TC_MEAS` in `src/twocore.h`); a normal build and a TWOCORE build without the bench have none of it. Owner's document: `release_drafts/BENCH_TWOCORE2.md`.

## What is measured, per job (TIMER4 ticks, 24 a microsecond)

| Figure | From | To | Where |
|---|---|---|---|
| start delay | core 0's clock just before it posts the job | core 1's clock when it sees the new job word | core 1 writes `t_seen`; core 0 reads it |
| run time | core 1's clock at the sight | core 1's clock at the return of `tc_job_run` | core 1 writes `t_seen`, `t_end` |
| finish margin | `t_end` | the block's deadline `tcr.dl` | core 0 (negative: after the deadline) |
| start in the half | the half's start (`tcr.t0` at the post) | `t_seen` | core 0; per block of the half |
| deadline in the half | the half's start | `tcr.dl` | core 0; per block of the half |

Core 1 adds two clock reads and three stores per job (`t_seen`, `t_end`, `mj`, the job number they belong to), all before the `done` word, in its own mailbox line (the words come from the 7 spare words of line 1; the object stays 128 bytes). Core 0 collects when it sees `done` (in time, or very late but finished) and, for a very late job that finished after its half, when `tcr_settle` closes the job at a later block. A job core 0 took back (late) or that was refused has no figures. A clock that does not fit (negative, or more than four halves) is counted in `badclk` and not binned.

Kept per test (`tcm`, cleared by the audio interrupt at the first block after the main loop sets `tcm.clr`; `bench run` and `bench reset` set it): jobs measured, maxima and sums (delay, run, margin), three 8-bin histograms (edges in microseconds: delay <2 <5 <10 <25 <50 <100 <250; run <100 <150 <200 <300 <400 <600 <900; margin <-500 <-100 <0 <100 <250 <500 <1000), the very late jobs (count, run, delay, block of the half 0..3), core 1's busiest half (the run times summed per half), per block of the half the mean start and mean deadline, a ring of the newest 16 very late jobs (`bench tcm`), and the jobs posted and the blocks core 0 played part 2 itself split by reason: core 1 down, flash park, paused, deadline already past (`tcm.own[]`; they add up to `tcs.own`).

No cache or flash counter is read: nothing in the repository's hal, the DUELL notes or the AC79 SDK sources on this Mac documents one (the only cache word used anywhere is `CACHE_CON` bit 14, "cache idle"). Reading undocumented registers from core 1 on a live unit was not done.

## Runtime switches (bench console words, until `bench reset` / `bench end`)

`bench slack TICKS|default` (0..400000; the build's value 3,481 = 5 % of a half), `bench calm MS|default` (0..80000; 10,000), `bench pause on|off` (off: the late and very late runs set to 1000, so nothing pauses the jobs), `bench lrun N` and `bench vrun N` (1..1000; 4 and 1), `bench tcm`. The values live in `tcm_cfg` (`src/twocore_run.c`, `.data`); without `TC_MEAS` the four names are the constants `TCR_SLACK_TICKS`, `TCR_CALM_TICKS`, `TCR_LATE_RUN`, `TCR_VLATE_RUN`. The settings in force are in the `bench info` line and in every result row (`tmslack`, `tmcalm`, `tmlrun`, `tmvrun`).

## Result line and CSV

`c1` (core 1's busy share: the run times over the test, x10 %) and `c1pk` (its busiest half) are now filled; `c1late` and `c1over` are the late and very late blocks as before. 58 columns follow `c1stack`: see the header `BTH,` (`tmjobs ... tmvrun`) and `tools/bench_run.py` `TM_COLS`. The Mac runner adds a CORE 1 TIMING section to `summary.txt` and `vlate_events.csv`.

## Tests

`./host_tests.sh tcmeas` (the thread simulation built with `-DTC_MEAS=1`, plus five mutants), `benchsim` (the words, limits, defaults, 105 fields), `benchrun` (the TWOCORE-1 suite, the tune words, the columns, the summary section).

## BENCHT3 (branch `twocore4`): the pause rule and the deadline floor

Written after BENCHT2's run (release_drafts/BENCHT2_RUN1.md); the description for the owner is release_drafts/BENCH_TWOCORE3.md.

1. **Pause rule.** A very late block (core 1 had started, was not done by the deadline) whose job is done inside its half, with a good answer, no longer pauses the jobs by itself. The pause stays for: a wrong or refused answer, no answer, a silent block, a job not finished in its half (all "fault", unchanged, with the strikes of `tc_fail`), a run of `TCR_VLATE_RUN` (4) very late blocks in a row ("run"), `TCR_VWIN_K` (8) of them within `TCR_VWIN_MS` (2,000 ms) ("window": a ring of the last 16 times; a gap longer than the window starts the chain again), and a run of `TCR_LATE_RUN` late blocks (unchanged, "late"). `TCR_CALM_MS` (10 s) stays. Counters (`tcs`): `vlate_ok` (very late, finished, paused nothing), `pause_fault`, `pause_run`, `pause_win`, `pause_late` (they add up to `pauses`).
2. **Floor.** `tcr_deadline(now, &nofit)`: `cap` = half end less the table's need (core 0's own latest moment); table deadline = `cap - slack`; with the floor on and a recent maximum run time `rm` (the largest `t_end - t_seen` of the last 128 to 256 blocks, TC_MEAS builds only: it needs core 1's clock reads): if the table deadline is still ahead and `now + rm > cap` the job cannot finish before core 0's latest moment whatever the deadline: `nofit` (no post, own reason `TCM_OWN_FL`); else deadline = max(table deadline, min(`now + rm + slack`, `cap`)). The buckets turn over by blocks, so a maximum that stopped the jobs fades. A normal (non-bench) build has no floor: nothing measures there.
3. **Switches** (`bench`, TC_MEAS builds): `vrun N` (default 4; `vrun 1` = the BENCHT2 rule), `vwin K [MS]` (default 8 2000; K 0 = off), `floor 0|1` (default 1); `pause off` also turns the window off; `reset` and `end` put them back.
4. **Tests:** `twocore` (new faults `vrun`, `vwin`; `late_post` now pauses nothing), `tcmeas` (new faults `floor`, `floorx`, `nofloor`; every posted deadline is at or before the cap), mutants for each rule.

## BENCHT4 (branch `twocore5`, from `twocore4` 12bb007): "pause only for real faults"

Written after BENCHT3's run (release_drafts/BENCHT3_RUN1.md); the description for the owner is release_drafts/BENCH_TWOCORE4.md.

1. **Pause rule.** The jobs pause only for: a fault (wrong or refused answer, no answer to the take within `TCR_ANSWER_US`, a crash/hang strike), a job not finished by its WAIT LIMIT (a silent block), and a run of `TCR_LATE_RUN` (4) late blocks (kept: core 1 not picking up jobs; never seen on the unit, it is not a very late block). A very late block whose job finished by the wait limit never pauses, however many in a row or in a window. The BENCHT3 run and window rules stay in the code, reachable only through `bench vrun N` / `bench vwin K [MS]`; their defaults are now `TCR_VLATE_RUN 0` and `TCR_VWIN_K 0` (0 = off). `vrun 0` is allowed.
2. **Floor** default off (`bench floor 1` switches it on).
3. **Wait limit** (`tcr.lim`, set in `tcr_deadline`). Up to BENCHT3 core 0 waited for a claimed job until the half's end. A job finishing between the deadline and the half's end can leave core 0 less time than the rest of the half needs (the later blocks at the whole one-core cost), so since vrun/vwin are off this wait was a way to miss the half. Now: `lim` = half end - (the cost table's need - part 2's own cost) = `cap` + part 2's own cost, i.e. the latest moment at which core 1's done word still leaves core 0 the globals of this block and every later block of the half. `tcr_part2` waits for the answer to the take and for done only until `lim` (never later than the half's end); not done by then = the old silent path (part 2 silent for the block, pause as a fault, the open job settled later). The deadline plus the slack (`cap`) and `lim` are both from the same table; the spare time core 0 can give to a very late job is `slack + part 2's cost`.
4. **Counters** unchanged (`vlate_ok` stays the visible count of very late blocks that paused nothing); `bench info` ends `...,vrun 0,vwin 0/2000,floor 0`. The runner: pause word `new` (BENCHT4 defaults), `b3` (BENCHT3's run 4 + window 8), `old` (vrun 1), `off`.
5. **Tests:** `twocore` (new faults `voff`, `noans`, `vlong`; every run checks core 0 never left its wait later than `lim` and never later than the half's end), `tcmeas` (`vrun`/`vwin` switch BENCHT3's rules on; the floor runs switch the floor on), mutants (a very late finished job pauses again; the window default; a real fault no longer pauses (no answer; any); an unfinished job no longer pauses; the wait runs to the half's end), `benchsim`, `benchrun` (suite TWOCORE-3, verdicts, `diag` inside one test).

