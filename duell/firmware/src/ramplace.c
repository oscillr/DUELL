/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL_RAMPLACE=1 (two-core stage 1a, developer builds only; felucca.c includes this file only then). Part 2's voice code runs from two
 * unused RAM ranges instead of the flash: range X (0x01C00000.., the render side of seven engines, tools/ram_fns_x.py) and range Z
 * (after the pool, the rest of the voice path, tools/ram_fns_z.py), linked there by firmware/app_ram.ld, loaded from the flash image.
 * Still everything on core 0: this build proves that the code can live and run there with the same sound.
 *
 * ramplace_copy: in main.c fm1_cstart right after the boot guard, before anything that could call X or Z code (memset and memcpy are in Z;
 * tools/elfcheck.py follows every call made before it). A start that dies in it counts as a failed one. Plain word loops, no call.
 * ramplace_boot: after the start-up copies, each range compared word for word with its load image; a difference: copied again and compared
 * again (twice). Still different: the RAM does not hold the code, nothing could run it (every caller jumps to the RAM address; there is no
 * second copy in the flash to run instead), so the unit goes to the update mode at once (as the boot guard does after two failed starts):
 * the DUELL installer puts the normal build back from there.
 * ramplace_check: the same comparison while the unit runs (MENU > ABOUT, once a second): finds code that something overwrote after boot. */
extern uint32_t _rx_start[], _rx_end[], _rx_load[], _rz_start[], _rz_end[], _rz_load[];

/* The variables in a section of their own: LLVM's GlobalMerge leaves globals with a section alone, so the other globals merge as in the
 * build without DUELL_RAMPLACE and every moved function is the same object code as there (tools/ram_place.py --compare checks it) */
#define RP_VAR __attribute__((section(".bss.ramplace")))
static uint8_t rp_boot_copies RP_VAR;           /* copies the start needed beyond the first (0: the first was right) */
static uint8_t rp_state RP_VAR;                 /* the last ramplace_check: 0 OK, bit 0 range X differs, bit 1 range Z differs */
static uint32_t rp_about_t RP_VAR;              /* ABOUT's last check (fm1_ms; ui_menu.c about_sig) */

static void rp_copy1(uint32_t *d, uint32_t *e, const uint32_t *s)
{
    while (d < e)
        *d++ = *s++;
}

static uint32_t rp_same(const uint32_t *d, const uint32_t *e, const uint32_t *s)
{
    while (d < e)
        if (*d++ != *s++)
            return 0;
    return 1;
}

static void ramplace_copy(void)
{
    rp_copy1(_rx_start, _rx_end, _rx_load);
    rp_copy1(_rz_start, _rz_end, _rz_load);
}

static uint32_t ramplace_check(void)
{
    uint32_t bad = 0;
    if (!rp_same(_rx_start, _rx_end, _rx_load))
        bad |= 1u;
    if (!rp_same(_rz_start, _rz_end, _rz_load))
        bad |= 2u;
    return bad;
}

static void ramplace_boot(void)                 /* (after the .bss clear: rp_boot_copies / rp_state are .bss) */
{
    uint32_t k;
    for (k = 0; k < 3u; k++) {
        rp_state = (uint8_t)ramplace_check();
        if (!rp_state)
            break;
        rp_boot_copies++;
        ramplace_copy();
    }
    if (rp_state) {
        bootguard.pending = 0;                  /* (not counted as a failed start: the update mode is meant) */
        fm1_enter_uboot();
    }
}

static uint32_t ramplace_bytes(void)
{
    return (uint32_t)((uintptr_t)_rx_end - (uintptr_t)_rx_start) + (uint32_t)((uintptr_t)_rz_end - (uintptr_t)_rz_start);
}
