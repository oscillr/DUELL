/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FELUCCA memory map for DUELL_RAMPLACE=1 (two-core stage 1a, developer builds only; tools/build.py uses this file instead of app_hot.ld
 * then). Everything is as in app_hot.ld (the hot block, .data, .bss, the pool, the stacks, .noinit keep their addresses) plus two ranges
 * of RAM code, loaded from the flash image and copied by main.c fm1_cstart (after the boot guard) before anything can run them:
 *   .ramx  range X, RAMX = 0x01C00000..0x01C07F00 (app.ld's old RAMTEXT; 0x01C07F00.. stays crt0's debug area): the render side of seven
 *          engines (tools/ram_fns_x.py). Out of a direct call's reach of the flash both ways: entered through engine_t pointers only, and
 *          calls nothing but RAM code (tools/ram_place.py, tools/elfcheck.py check it).
 *   .ramz  range Z, after .pool in the POOL region, below the stack guard 0x01C74000: the rest of part 2's voice path
 *          (tools/ram_fns_z.py). In direct-call reach of the whole image both ways (like the hot block).
 * The PC limit's RAM window (hal/fm1_guard.h) runs from _pc_ram_lo (= the start of X) to the end of the hot block (_rt_end): one window
 * over X, Z and the hot block (and the data between them: a wider window guards less; there are two windows, the other is the flash).
 * DUELL bench builds (src/bench.c): .bench_text / .bench_rodata after all other code, .bench_bss after .bss, .pool.zbench (the bench's
 * session copy) after the other pool objects; range Z then starts that much later (elfcheck checks Z against the pool's real end). */
MEMORY
{
    XIP    (rx)  : ORIGIN = 0x02000120, LENGTH = 0x8DFBC
    RAMX   (rwx) : ORIGIN = 0x01C00000, LENGTH = 0x7F00
    RAM    (rwx) : ORIGIN = 0x01C08000, LENGTH = 72K
    POOL   (rw)  : ORIGIN = 0x01C1A000, LENGTH = 0x5A000
    NOINIT (rw)  : ORIGIN = 0x01C7C000, LENGTH = 0x100
    RAMCODE (rwx): ORIGIN = 0x01C7C100, LENGTH = 0x3C4C
}
PROVIDE(_hot_pad = 0);
SECTIONS
{
    .text : { KEEP(*(.text.crt0)) *(.text*) *(.bench_text*) *(.rodata*) *(.bench_rodata*) . = ALIGN(4); _etext = .; } > XIP   /* (.bench_text before the constants: it is the farthest code from range Z and the two-core bench build sits at the 4 MiB call reach; no call goes to the constants) */
    .ram_text : { _rt_start = .; KEEP(*(.ram_text*)) . = ALIGN(4); _hot_start = .; *(.hotx.*) . = MAX(., _hot_start + _hot_pad); . = ALIGN(4); _rt_end = .; } > RAMCODE AT > XIP
    _rt_load = LOADADDR(.ram_text);
    .data : { . = ALIGN(4); _data_start = .; *(.data*) *(.sdata*) *(.bench_data*) . = ALIGN(4); _data_end = .; } > RAM AT > XIP
    _data_load = LOADADDR(.data);
    .bss (NOLOAD) : { _bss_start = .; *(.bss*) *(.sbss*) *(COMMON) *(.bench_bss*) . = ALIGN(4); _bss_end = .; } > RAM
    .pool (NOLOAD) : { _pool_start = .; *(.pool) *(.pool.*) . = ALIGN(4); _pool_end = .; } > POOL
    .ramz : { . = ALIGN(4); _rz_start = .; *(.ramz.*) . = ALIGN(4); _rz_end = .; } > POOL AT > XIP
    _rz_load = LOADADDR(.ramz);
    .ramx : { _rx_start = .; KEEP(*(.c1_text*)) *(.ramx.*) . = ALIGN(4); _rx_end = .; } > RAMX AT > XIP   /* (.c1_text: DUELL_TWOCORE=1 only, core 1's loop and entry) */
    _rx_load = LOADADDR(.ramx);
    _pc_ram_lo = _rx_start;
    .noinit (NOLOAD) : { *(.noinit*) } > NOINIT
    _guard0 = 0x01C74000; _ustack_lo = 0x01C74100; _ustack_top = 0x01C7A000;
    _guard1 = 0x01C7A000; _sstack_lo = 0x01C7A100; _sstack_top = 0x01C7C000;
    ASSERT(_rz_end <= _guard0, "range Z runs into the stack guard")
    /DISCARD/ : { *(.comment) *(.note*) }
}
