# What core 1 really reaches (branch twocore1, both flags on)

Measured with `tools/ram_reach.py build/felucca_ram.o tc_job_run fm1_c1_main` on a `DUELL_TWOCORE=1 DUELL_RAMPLACE=1` build (device code, -Os). It follows every relocation of the compiled object: calls, addresses taken in code, and the pointers in data tables (the engines' descriptors). The 283-function list of `TWOCORE_JOB5_REACH.txt` came from a host build with nothing inlined; this is the device's own graph.

- Reached: 153 functions (the job `tc_job_run` is inlined into core 1's loop `fm1_c1_main`).
- Already in RAM on the RAM1 line: 109 (hot block 22 = 8,984 B, range X 39 = 30,632 B, range Z 48 = 12,422 B).
- Not yet in RAM: 44 = `.c1_text` (584 B) + 28 functions the job calls (7,270 B: part_fx, part_mix, part_rest and what they call; EQ, inserts, ducking, sends, mutes; `tc_ctx_hash`, `tc_c1_init`, `retired_render`) + 15 that hang only on the engine table (`*_note_on`, `*_desc`, `retired_note_on`: 1,684 B; core 0's note events and the UI call them, the job does not: `voice_start` and the parameter UI are not in the graph). Those 15 stay in the flash.
- Now: the 28 are in range Z (`tools/ram_fns_z.py` TWOCORE_GROUPS, only with `DUELL_TWOCORE=1`), `.c1_text` is in range X (`firmware/app_ram.ld`: `KEEP(*(.c1_text*))` in `.ramx`), inside the PC window. The 115 placed functions have the same object code and relocations as without the move (`tools/ram_place.py --compare`).
