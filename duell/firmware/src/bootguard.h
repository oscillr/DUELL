/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* The boot guard's rules (main.c), plain C so that the computer's test (tests/bootguard_test.c) runs the very same lines. They are
 * macros on purpose: they expand to exactly the statements main.c had, on the .noinit struct itself (no pointers to its words: the
 * .noinit/LTO trouble of tools/elfcheck.py).
 * bootguard (core.h, .noinit) keeps three words over a chip reset: magic, failed, pending.
 *   pending  set at every start, cleared once the start has lived 30 s or when the reset was meant (update, UBOOT, ERASE DATA)
 *   failed   counted at a start that finds pending still set (the last start died, or was reset without saying so); tc_boot (twocore.c)
 *            then keeps core 1 off for this session (SKIPBOOT)
 * BG_START sets `enter` to 1 when two starts in a row died (main.c then enters the update mode). */
#ifndef BOOTGUARD_H
#define BOOTGUARD_H
#define BG_START(bg, want_magic, enter)  \
    do {                                 \
        (enter) = 0;                     \
        if ((bg).magic != (want_magic)) {\
            (bg).magic = (want_magic);   \
            (bg).failed = 0;             \
            (bg).pending = 0;            \
        }                                \
        if ((bg).pending)                \
            (bg).failed++;               \
        (bg).pending = 1;                \
        if ((bg).failed >= 2u) {         \
            (bg).failed = 0;             \
            (bg).pending = 0;            \
            (enter) = 1;                 \
        }                                \
    } while (0)
/* a reset that was meant (ERASE DATA's restart; the update and UBOOT paths clear pending the same way): not a failed start */
#define BG_MEANT_RESET(bg) do { (bg).pending = 0; (bg).failed = 0; } while (0)
/* ERASE DATA (DUELL 1.1, item 92 addendum 2): the guard is wiped whole, the magic too, so the next start finds RAM it does not know and begins
 * from nothing: no pending start, no failed count (the update-mode counter), nothing carried over */
#define BG_ERASE_ALL(bg) do { (bg).magic = 0; (bg).pending = 0; (bg).failed = 0; } while (0)
#endif
