/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FM1_INLINE: register helpers that must inline into their caller, so the
 * SFR access sequence (and its timing, in ISRs and IRQ-off sections) is the
 * one written at the call site; -Os would otherwise out-line a helper with
 * several callers. RAM-resident code (.ram_text) may only use these, never
 * a called helper (build.py checks .ram_text for calls). */
#pragma once

#define FM1_INLINE static inline __attribute__((always_inline))

/* BOARD_SMK37: 1 builds for the M-Vave SMK-37 Pro / Elite / MKE-P37 (same chip, other panel and flash map);
 * 0 (default) is the FM-1, unchanged. Everything SMK-only sits behind it. */
#ifndef BOARD_SMK37
#define BOARD_SMK37 0
#endif
/* SMK_MODEL: the model's name in the package identity and the update record ("SMK-37 Pro", "SMK-37 Elite", "MKE-P37": the
 * same keyboard, but M-Vave's updater takes only a unit's own name; the build sets it) */
#if BOARD_SMK37 && !defined(SMK_MODEL)
#define SMK_MODEL "SMK-37 Elite"
#endif
/* SMK_SCREEN: the SMK-37's two-part screen (src/smk_screen.c), on with BOARD_SMK37 (tests/ui_render.c sets it alone for the host renders) */
#ifndef SMK_SCREEN
#define SMK_SCREEN BOARD_SMK37
#endif
