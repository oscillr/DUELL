/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 DUELL contributors */
/* BYPASS for the editor (web/EDITOR_PROTOCOL.md "BYPASS"; INFO 5B 01 caps, bit 0 = command 89). Temporary switches for sound design, RAM only (core.h byp_m, byp_sh): no parameter id, not in DUMP,
 * a patch, a multi, a project, a backup or the morph, no undo step.
 *   BYPASS 89  0                 GET: replies 0, lo part 1, hi part 1, lo part 2, hi part 2, shared
 *              1, part, lo, hi   SET a part (part 0..1; bits above 10 dropped): replies 1, part, lo, hi as held
 *              2, mask           SET the shared mask (bits above 4 dropped): replies 2, mask as held
 *              anything else     no reply */
enum { ED_BYPASS = 89 };
static int ed_byp_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint32_t i;
    if (cmd != ED_BYPASS)
        return 0;
    if (n == 1u && a[0] == 0u) {
        ed_b(0);
        for (i = 0; i < 2u; i++) {
            ed_b(byp_m[i]);
            ed_b(byp_m[i] >> 7);
        }
        ed_b(byp_sh);
        return 1;
    }
    if (n == 4u && a[0] == 1u && a[1] < 2u) {
        byp_m[a[1]] = (uint16_t)((a[2] | (uint32_t)a[3] << 7) & BY_ALL);
        ui.force = 1;                                   /* (the pages draw BYPASS in place of the values) */
        ed_b(1); ed_b(a[1]); ed_b(byp_m[a[1]]); ed_b(byp_m[a[1]] >> 7);
        return 1;
    }
    if (n == 2u && a[0] == 2u) {
        byp_sh = (uint8_t)(a[1] & BYS_ALL);
        ui.force = 1;
        ed_b(2); ed_b(byp_sh);
        return 1;
    }
    return 0;
}
