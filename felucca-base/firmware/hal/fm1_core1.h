/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* DUELL TWO-CORE, stage 1 (DUELL_TWOCORE=1 only: a normal build includes this file but compiles none of it): the chip's second core, the
 * DEVICE half of the hal of src/twocore.h (its computer half: tests/twocore_hal_host.h). docs/TWOCORE_STAGE1.md.
 * From DUELL's bench (wip/bench0226 hal/fm1_core1.h: ran clean on the owner's unit in BENCH-B3, 2026-10-18): the start, the entry, core 1's
 * own stack limit, the waiting loop in RAM. Changed for stage 1: the bench's mailbox (a function pointer, a job word and a check word) is
 * replaced by the stage-1 words (src/twocore.h tc_mb: separate 32-bit words, one writer each, a sequence number); the loop runs
 * tc_c1_step (core 1 answers every job once: claim + done, yld, or refused); the B1 interference loads (fm1_k_*) are left in the bench.
 *
 * Written for DUELL (no file copied). The register facts and the method come from these GPL-3.0-only firmwares and documents, credited
 * in NOTICE.md:
 *   - Felucca 1.6.1 (github.com/hugelton/Felucca, Leo Kuroshita / Huegelton Instruments: hal/fm1_cpu1.{h,S}), itself from X0X 1.0.5
 *     (github.com/charlesvestal/fm1-x0x), Melodee (github.com/keremimo/melodee, Kerem Kilic) and Jangada (github.com/zednaked/jangada):
 *     the start (the AC79 SDK's EnableOtherCpu: core 1's interrupt bank 0x1EEF300.. all off, its entry address in the word at 0x01C7FFF8,
 *     bit 3 of 0x10008 held while it starts, C1_CON 0x1EEE004 bit 3 set and bit 1 cleared, 20 ms for an "alive" word; the PC limits open
 *     meanwhile because core 1 boots through the chip ROM), the hold (hal/fm1_sys.h fm1_cpu1_halt), the mailbox with one writer per cache
 *     line, the check word of every job (a misreading core 1 runs nothing it read wrong), core 1's own stack limit (its EMU bank at 0x1EEF2D0).
 *   - DUELL's own earlier probe (wip/core1probe, 2026-10-14: read from M-VAVE's fm1_open runtime with objdump) and docs/CPU_PLAN.md.
 *
 * PLACEMENT (local stage 1 work, docs/TWOCORE_STAGE1.md "what the local build needs"; NOT in this repo's linker scripts yet, so a
 * DUELL_TWOCORE=1 link is not ready): core 1's loop and entry go to the section .c1_text, which must be KEEP'd inside the PC limit's RAM window
 * (the bench put it at the end of app_hot.ld's .ram_text, after the hot functions); the words tc_mb in .pool.ztwocore (the pool's tail:
 * a 64-byte alignment in .bss would move it); the record tc_nv in .noinit.ztwocore (tools/elfcheck.py knows it); part 2's
 * job code in the RAM ranges X and Z (docs/TWOCORE_STAGE1.md section 6).
 *
 * Core 1 runs with its interrupts off. Idle it waits in fm1_c1_main (RAM, no flash fetch), beating now and then. */
#pragma once
#if defined(DUELL_TWOCORE) && DUELL_TWOCORE
#include <stdint.h>
#include "fm1_sys.h"
#include "fm1_time.h"
#include "fm1_guard.h"

#define FM1_C1_CLK (*(volatile uint32_t *)0x00010008u)
#define FM1_C1_ENTRY (*(volatile uint32_t *)0x01C7FFF8u)
#define FM1_C1_ICFG(i) (*(volatile uint32_t *)(0x1EEF300u + 4u * (i)))
#define FM1_C1_EMU(off) (*(volatile uint32_t *)(0x1EEF2D0u + (off)))   /* core 1's EMU bank: CON +0, SSP H/L +8/+C, USP H/L +10/+14 */
#define FM1_C1_USER_BYTES 3072                                         /* core 1's user stack: 3 KB (memory trim 1; was 4 KB). Its lowest 256 B are the limit's margin (fm1_c1_main), so 2,816 B are usable;
                                                                        * the deepest use measured is 1,744 B in the bench runs BENCHM1 / BENCHT1 (tc_stack_peak counts the 256 B supervisor stack above it, so 1,488 B of user stack)
                                                                        * (a margin of 1.9 x); the plan's new code (chain, SUB, CYCLE) is expected to add one or two small frames */
#define FM1_C1_TOP_BYTES 3328                                          /* .. plus its supervisor stack (256 B): the whole array */
#define FM1_C1_STACK_WORDS (FM1_C1_USER_BYTES / 4u)
#define FM1_STR_(x) #x
#define FM1_STR(x) FM1_STR_(x)

FM1_INLINE void fm1_c1_sync(void) { __asm__ volatile("csync" ::: "memory"); }

/* ---- the hal of src/twocore.h, device side */
/* TC_LD / TC_ST call these two helpers: the volatile access stays in hal/, so src/twocore*.c/.h pass the build's
 * "no register access outside hal/" check (tools/build.py mmio_check: a macro whose body says volatile is a register macro). */
FM1_INLINE uint32_t fm1_c1_ld(const uint32_t *p) { return *(volatile const uint32_t *)p; }
FM1_INLINE void fm1_c1_st(uint32_t *p, uint32_t v) { *(volatile uint32_t *)p = v; }
#define TC_LD(p) fm1_c1_ld(p)
#define TC_ST(p, v) fm1_c1_st((p), (uint32_t)(v))
#define TC_SYNC() fm1_c1_sync()
#define TC_TICKS() fm1_ticks()
#define TC_TICKS_PER_US FM1_TICKS_PER_US
#define TC_C1_START() fm1_cpu1_start()
#define TC_C1_HALT() fm1_cpu1_halt()
#define TC_SUPPLY_STOCK() fm1_supply_stock()
#define TC_C1_INLINE static inline __attribute__((always_inline))
#define TC_MB_ATTR __attribute__((aligned(64), section(".pool.ztwocore")))
#define TC_CTX_SECT __attribute__((section(".pool.ztwocore")))   /* (job 5: the block's snapshot, src/twocore_run.c tc_ctx, 64 B, beside tc_mb) */
static int fm1_cpu1_start(void);
#include "twocore.h"
tc_nv_t tc_nv __attribute__((section(".noinit.ztwocore")));   /* the record across a reset (src/twocore.c; tools/elfcheck.py NOINIT_OBJECTS:
                                                         * 12 B; not static, so no optimiser assumes it 0) */

uint32_t fm1_c1_stack[FM1_C1_STACK_WORDS + 64u] __attribute__((aligned(8)));   /* core 1's user stack (3 KB), then its supervisor stack (256 B),
                                                         * one block so its stack limit covers both (not static: the entry's asm names it) */

_Static_assert(sizeof fm1_c1_stack == FM1_C1_TOP_BYTES && FM1_C1_TOP_BYTES == FM1_C1_USER_BYTES + 256, "fm1_core1.h: the stack's size and the entry's addresses");

/* core 1: the waiting loop. In .c1_text (RAM). It calls nothing but the job (tc_c1_step is inlined) */
void fm1_c1_main(void);
void __attribute__((section(".c1_text"), noreturn, used, noinline)) fm1_c1_main(void)
{
    uint32_t lo = (uint32_t)(uintptr_t)fm1_c1_stack + 256u, hi = (uint32_t)(uintptr_t)(fm1_c1_stack + FM1_C1_STACK_WORDS + 64u) - 1u;
    tc_c1_t c;
    FM1_C1_EMU(0x08) = hi;                      /* its own stack limit over both its stacks less the lowest 256 bytes (Melodee's, via Felucca) */
    FM1_C1_EMU(0x0C) = lo;
    FM1_C1_EMU(0x10) = hi;
    FM1_C1_EMU(0x14) = lo;
    FM1_C1_EMU(0x00) = (FM1_C1_EMU(0x00) & ~((1u << 2) | (0x1Fu << 16))) | (1u << 3);
    tc_c1_init(&c);
    TC_ST(&tc_mb.alive, 1u);
    fm1_c1_sync();
    for (;;)
        tc_c1_step(&c);
}
/* core 1's entry (as the SDK's startup and Felucca's fm1_cpu1.S): interrupts off, no interrupt configuration, its stacks, then rti into its loop */
__asm__(".section .c1_text,\"ax\",@progbits\n"
        "\t.p2align 2\n"
        "\t.globl fm1_c1_entry\n"
        "fm1_c1_entry:\n"
        "\tcli\n"
        "\ticfg = 0\n"
        "\tusp = fm1_c1_stack+" FM1_STR(FM1_C1_USER_BYTES) "\n"
        "\tsp = fm1_c1_stack+" FM1_STR(FM1_C1_TOP_BYTES) "\n"
        "\tssp = fm1_c1_stack+" FM1_STR(FM1_C1_TOP_BYTES) "\n"
        "\treti = fm1_c1_main\n"
        "\trti\n"
        "\t.previous\n");
extern char fm1_c1_entry[];

/* core 0: start core 1 (power-on, interrupts off, before audio_init: started later it does not reach its entry, X0X / Felucca).
 * 0: it answered within 20 ms; -1: no answer, held again. The PC limit is opened for its ROM boot and armed again before the return.
 * Every word of tc_mb is set here while core 1 is held (the one-writer rule holds from the start on) */
static int fm1_cpu1_start(void)
{
    uint32_t i, saved, t0;
    volatile uint32_t *w = (volatile uint32_t *)&tc_mb;
    fm1_cpu1_halt();
    for (i = 0; i < sizeof tc_mb / 4u; i++)
        w[i] = 0;
    tc_stack_fill(fm1_c1_stack, FM1_C1_STACK_WORDS + 64u);   /* (the stack mark: src/twocore.c tc_stack_peak) */
    fm1_c1_sync();
    for (i = 0; i < 32u; i++)
        FM1_C1_ICFG(i) = 0;                     /* its interrupt bank: all off */
    FM1_C1_ENTRY = (uint32_t)(uintptr_t)fm1_c1_entry;
    fm1__dbg_unlock();
    FM1_PC_LIMIT0_L = 0;                        /* open: core 1 runs the chip ROM first */
    FM1_PC_LIMIT0_H = 0xFFFFFFFFu;
    fm1__dbg_lock();
    saved = FM1_C1_CLK;
    FM1_C1_CLK = saved | 0x8u;
    fm1_c1_sync();
    FM1_C1_CON |= 0x8u;
    FM1_C1_CON &= ~0x2u;
    t0 = fm1_ticks();
    while (!TC_LD(&tc_mb.alive) && fm1_ticks() - t0 < 20000u * FM1_TICKS_PER_US)
        fm1_c1_sync();
    FM1_C1_CLK = saved;
    if (!TC_LD(&tc_mb.alive))
        fm1_cpu1_halt();
    else
        fm1_c1_live = 1;
    FM1_C1_ENTRY = (uint32_t)(uintptr_t)(fm1_fatal_stubs + 6u * 126u);   /* (vector 126 back to its fatal stub) */
    fm1_guard_enable(FM1_GUARD_PC);             /* armed again (core 1 runs .c1_text and part 2's code: inside the RAM window, local stage 1) */
    return TC_LD(&tc_mb.alive) ? 0 : -1;
}
#endif
