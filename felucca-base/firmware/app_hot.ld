/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FELUCCA memory map for the DUELL_HOTRAM experiment (bench builds R1..R3 only; tools/build.py uses this file instead of app.ld
 * when DUELL_HOTRAM is 1..3). Everything is as in app.ld except where the RAM code goes:
 *   .ram_text (the flash driver, section .ram_text: no calls) and after it the hot audio functions (sections .hotx.*: build.py
 *   renames the listed functions' .text.<name> in felucca.o, tools/hot_fns.py; the C and the compiled code are bench A's) are
 *   one block, RAMCODE = 0x01C7C100..0x01C7FD4C: the top of RAM after .noinit, up to boot_info.
 * Why there: a direct call (pi32v2 "call", relocation R_PI32V2_LONG_JUMP_23M2) reaches +-4 MiB. From 0x01C00000 (app.ld's
 * RAMTEXT) the flash at 0x02000120 is 0x400120 away: out of reach (hal/fm1_flash.h calls its RAM driver through a pointer for
 * that reason). From 0x01C7C100 the whole image (0x02000120 .. the end of .text, < 0x0206E700) is within 0x3F3000: hot code can
 * call flash code and be called by it directly. The linker fails ("relocation truncated to fit") if a call does not reach.
 * One block: the PC limit (hal/fm1_guard.h) allows _rt_start.._rt_end-1 as its RAM window, unchanged.
 * NOINIT shrinks to 0x100 (.noinit is 0xDC at 0x01C7C000, the same address and objects as app.ld; tools/elfcheck.py checks it).
 * .data, .bss, the pool and the stacks keep app.ld's addresses; app.ld's RAMTEXT (0x01C00000..0x01C08000) is left unused.
 * DUELL bench builds (src/bench.c, DUELL_BENCH; none of these input sections exists in a normal build, whose image is unchanged): the bench's
 * code and constants (.bench_text, .bench_rodata) go after all the other code and constants, its variables (.bench_bss) after the other .bss,
 * its session copy (.pool.zbench) after the other pool objects (.pool), so nothing of the normal build moves for them. */
MEMORY
{
    XIP    (rx)  : ORIGIN = 0x02000120, LENGTH = 0x8DFBC
    RAM    (rwx) : ORIGIN = 0x01C08000, LENGTH = 72K
    POOL   (rw)  : ORIGIN = 0x01C1A000, LENGTH = 0x5A000
    NOINIT (rw)  : ORIGIN = 0x01C7C000, LENGTH = 0x100
    RAMCODE (rwx): ORIGIN = 0x01C7C100, LENGTH = 0x3C4C
}
PROVIDE(_hot_pad = 0);      /* R7: tools/build.py --defsym=_hot_pad=N pads the hot block (zeros at its end) to N bytes; 0 = no padding */
SECTIONS
{
    .text : { KEEP(*(.text.crt0)) *(.text*) *(.rodata*) *(.bench_text*) *(.bench_rodata*) . = ALIGN(4); _etext = .; } > XIP
    .ram_text : { _rt_start = .; KEEP(*(.ram_text*)) . = ALIGN(4); _hot_start = .; *(.hotx.*) . = MAX(., _hot_start + _hot_pad); . = ALIGN(4); _rt_end = .; } > RAMCODE AT > XIP
    _rt_load = LOADADDR(.ram_text);
    .data : { . = ALIGN(4); _data_start = .; *(.data*) *(.sdata*) *(.bench_data*) . = ALIGN(4); _data_end = .; } > RAM AT > XIP
    _data_load = LOADADDR(.data);
    .bss (NOLOAD) : { _bss_start = .; *(.bss*) *(.sbss*) *(COMMON) *(.bench_bss*) . = ALIGN(4); _bss_end = .; } > RAM
    .pool (NOLOAD) : { _pool_start = .; *(.pool) *(.pool.*) . = ALIGN(4); _pool_end = .; } > POOL
    .noinit (NOLOAD) : { *(.noinit*) } > NOINIT
    _guard0 = 0x01C74000; _ustack_lo = 0x01C74100; _ustack_top = 0x01C7A000;
    _guard1 = 0x01C7A000; _sstack_lo = 0x01C7A100; _sstack_top = 0x01C7C000;
    /DISCARD/ : { *(.comment) *(.note*) }
}
