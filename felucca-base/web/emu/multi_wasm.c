/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL) */
/* The factory multis' golden renders (tests/multi_render_test.c, tests/multi_render_golden.txt) as WebAssembly, flags as host_tests.sh
 * (-DHOST_MIX_FOLLOWS_MASTER=1 -DDUELL_TEST_MODE=1, build/gen_test). One render per fresh instance (the test forks per render). Test only. */
#define main mrender_main
#include "../../tests/multi_render_test.c"
#undef main
#define EXPORT __attribute__((used, visibility("default")))
EXPORT uint32_t m_count(void) { return (uint32_t)NR; }
/* the test scenes and spare multis are read from tests/scenes/*.bin (files); the module has no file system, so only the sixteen factory multis (no scene) are rendered here */
EXPORT uint32_t m_scene(uint32_t i) { return RN[i].scene != 0; }
EXPORT const char *m_name(uint32_t i) { return RN[i].name; }
static uint64_t m_h;
EXPORT void m_run(uint32_t i, uint32_t f) { m_h = render(&RN[i], FAD[f]); }
EXPORT uint32_t m_hash_lo(void) { return (uint32_t)m_h; }
EXPORT uint32_t m_hash_hi(void) { return (uint32_t)(m_h >> 32); }
