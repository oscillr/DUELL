/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
 * The SMK-37 flash facts (protected ranges, store windows) follow the fm1-to-smk37 kit and OberVave's
 * fm1_flash_map.h (zvenson/obervave, from genkigenki/sloop-fm1 branch smk37, M. Geisler), GPL-3.0. */
/* DUELL's flash map on the M-Vave SMK-37 Pro / Elite / MKE-P37 (BOARD_SMK37 only; fm1_flash.h includes it).
 * Macros and compile-time checks only, so host tests can include it.
 *
 * The SMK-37's flash (1 MiB, JEDEC 0xEB6014):
 *   0x00000-0x03FFF  head (SPL, isd_config)                         NEVER
 *   0x04000-0x9CFFF  app area                                       update loader only
 *   0x9D000-0xBAFFF  VM: DUELL store, sector-tail-safe objects only (the loader's record window)
 *   0xBB000-0xBFFFF  VM: update staging (ota.c)
 *   0xC0000-0xC0FFF  BTIF (Bluetooth pairing)                       NEVER
 *   0xC1000-0xC1FFF  USRTRIM (factory key / pad / wheel calibration, "trimdat")   NEVER
 *   0xC2000-0xF3FFF  USRFLASH + the free tail: DUELL store, whole-sector objects
 *   0xF4000-0xFDFFF  USR (stock resources, kept for the way back)  NEVER
 *   0xFE000-0xFEFFF  free, unused by DUELL
 *   0xFF000-0xFFFFF  key_mac                                        NEVER
 * 0xEA000..0xF3FFF is free and unprotected in OberVave's map but no port wrote there before DUELL (owner's choice,
 * 2026-11-10: use it so nothing is cut). Every sector below is an explicit allow-list (FL_STORE_OK), and the RAM
 * driver refuses every protected range on its own as well (FL_RANGE_OK = allow-list AND not FL_PROTECTED).
 *
 * DUELL's objects (each an A/B pair of 4 KiB sectors; FM-1 address in brackets):
 *   0x9D000-0xA2FFF  pattern slots 0..2        (0xD6000)   payload ends at 0xF00
 *   0xA3000          FM6 voice bank            (0xE7000)   payload ends at 0xF00
 *   0xA4000-0xB9FFF  patch chunks 6..16        (0xA6000)   payload ends at 0xF00 (OBJ_PATCHX0 .. +10)
 *   0xBA000          spare
 *   0xBB000-0xBFFFF  update staging            (0xE0000)
 *   0xC2000-0xC3FFF  settings                  (0xFC000)
 *   0xC4000-0xC5FFF  autosave                  (0xE5000)
 *   0xC6000-0xCBFFF  user wavetables 1..3      (0xA0000)
 *   0xCC000-0xDBFFF  multis objects 0..7       (0x97000, 0xCE000)
 *   0xDC000-0xE7FFF  patch chunks 0..5         (0xC8000, 0xDC000, 0x9F000/0xFE000)
 *   0xE8000-0xF1FFF  patch chunks 17..21       (0xBA000)   (OBJ_PATCHX0+11 .. +15)
 *   0xF2000-0xF3FFF  spare
 * DUELL's velocity calibration object (FM-1 0xC6000) has no place: the SMK's factory calibration replaces it. */
#pragma once
#include <stdint.h>

/* protected */
#define SMK_HEAD_HI      0x00004000u
#define SMK_CAL_LO       0x000C0000u                /* BTIF + USRTRIM */
#define SMK_CAL_HI       0x000C2000u
#define SMK_USRRES_LO    0x000F4000u
#define SMK_USRRES_HI    0x000FE000u
#define SMK_KEYMAC_LO    0x000FF000u

/* windows */
#define SMK_APP_LO       0x00004000u
#define SMK_APP_HI       0x0009D000u
#define SMK_VM_LO        0x0009D000u                /* DUELL store, tail-safe objects */
#define SMK_VM_HI        0x000BB000u
#define SMK_OTA_LO       0x000BB000u                /* update staging */
#define SMK_OTA_HI       0x000C0000u
#define SMK_USR_LO       0x000C2000u                /* DUELL store, whole-sector objects */
#define SMK_USR_HI       0x000F4000u

/* objects (copy A; copy B is the next sector) */
#define SMK_PATSLOT_AT   0x0009D000u                /* 3 pairs */
#define SMK_VBANK_AT     0x000A3000u                /* 1 sector */
#define SMK_PATCHX_LO_AT 0x000A4000u                /* chunks 6..16: 11 pairs */
#define SMK_PATCHX_LO_N  11u
#define SMK_SETTINGS_AT  0x000C2000u                /* 1 pair */
#define SMK_AUTOSAVE_AT  0x000C4000u                /* 1 pair */
#define SMK_WTAB_AT      0x000C6000u                /* 3 pairs */
#define SMK_MULTI_AT     0x000CC000u                /* 8 pairs */
#define SMK_PATCH_AT     0x000DC000u                /* chunks 0..5: 6 pairs */
#define SMK_PATCHX_HI_AT 0x000E8000u                /* chunks 17..21: 5 pairs */
#define SMK_PATCHX_HI_N  5u

#define SMK_IN(off, n, lo, hi) ((uint32_t)(off) >= (lo) && (uint32_t)(off) <= (hi) && \
                                (uint32_t)(n) <= (hi) - (uint32_t)(off))
#define SMK_HITS(off, n, lo, hi) ((uint32_t)(off) < (hi) && \
                                  ((uint32_t)(off) >= (lo) || (uint32_t)(n) > (lo) - (uint32_t)(off)))
/* [off, off + n) touches a protected range */
#define SMK_PROTECTED(off, n) (SMK_HITS(off, n, 0u, SMK_HEAD_HI) || SMK_HITS(off, n, SMK_CAL_LO, SMK_CAL_HI) || \
                               SMK_HITS(off, n, SMK_USRRES_LO, SMK_USRRES_HI) || (uint32_t)(off) >= SMK_KEYMAC_LO || \
                               (uint32_t)(n) > SMK_KEYMAC_LO - (uint32_t)(off))
/* DUELL's own sectors only (the spare sectors are not in it) */
#define SMK_STORE_OK(off, n) (SMK_IN(off, n, SMK_PATSLOT_AT, SMK_PATCHX_LO_AT + SMK_PATCHX_LO_N * 0x2000u) || \
                              SMK_IN(off, n, SMK_SETTINGS_AT, SMK_PATCHX_HI_AT + SMK_PATCHX_HI_N * 0x2000u))

/* a window [lo, hi): sector aligned, non-empty, clear of every protected range */
#define SMK_WIN_SAFE(lo, hi) ((lo) % 0x1000u == 0u && (hi) % 0x1000u == 0u && (lo) < (hi) && \
                              (lo) >= SMK_HEAD_HI && (hi) <= SMK_KEYMAC_LO && \
                              ((hi) <= SMK_CAL_LO || (lo) >= SMK_CAL_HI) && \
                              ((hi) <= SMK_USRRES_LO || (lo) >= SMK_USRRES_HI))
_Static_assert(SMK_WIN_SAFE(SMK_VM_LO, SMK_VM_HI), "SMK: VM store overlaps a protected range");
_Static_assert(SMK_WIN_SAFE(SMK_OTA_LO, SMK_OTA_HI), "SMK: staging overlaps a protected range");
_Static_assert(SMK_WIN_SAFE(SMK_USR_LO, SMK_USR_HI), "SMK: USR store overlaps a protected range");
_Static_assert(SMK_WIN_SAFE(SMK_APP_LO, SMK_APP_HI) && SMK_APP_HI <= SMK_VM_LO, "SMK: app area");
_Static_assert(SMK_VM_HI <= SMK_OTA_LO && SMK_OTA_HI <= SMK_CAL_LO, "SMK: windows overlap");
/* the objects in order, each inside its window, none overlapping */
_Static_assert(SMK_PATSLOT_AT == SMK_VM_LO && SMK_PATSLOT_AT + 3u * 0x2000u <= SMK_VBANK_AT, "SMK: pattern slots");
_Static_assert(SMK_VBANK_AT + 0x1000u <= SMK_PATCHX_LO_AT, "SMK: voice bank");
_Static_assert(SMK_PATCHX_LO_AT + SMK_PATCHX_LO_N * 0x2000u <= SMK_VM_HI, "SMK: patch chunks 6..16 outside the VM store");
_Static_assert(SMK_SETTINGS_AT == SMK_USR_LO && SMK_SETTINGS_AT + 0x2000u <= SMK_AUTOSAVE_AT, "SMK: settings");
_Static_assert(SMK_AUTOSAVE_AT + 0x2000u <= SMK_WTAB_AT, "SMK: autosave");
_Static_assert(SMK_WTAB_AT + 3u * 0x2000u <= SMK_MULTI_AT, "SMK: wavetables");
_Static_assert(SMK_MULTI_AT + 8u * 0x2000u <= SMK_PATCH_AT, "SMK: multis");
_Static_assert(SMK_PATCH_AT + 6u * 0x2000u <= SMK_PATCHX_HI_AT, "SMK: patch chunks 0..5");
_Static_assert(SMK_PATCHX_HI_AT + SMK_PATCHX_HI_N * 0x2000u <= SMK_USR_HI, "SMK: patch chunks 17..21 outside the USR store");
_Static_assert(SMK_PATCHX_LO_N + SMK_PATCHX_HI_N == 16u, "SMK: all 16 extension chunks (22 chunks in all, 128 patches)");
_Static_assert(!SMK_PROTECTED(SMK_PATSLOT_AT, SMK_PATCHX_LO_AT + SMK_PATCHX_LO_N * 0x2000u - SMK_PATSLOT_AT) &&
               !SMK_PROTECTED(SMK_SETTINGS_AT, SMK_PATCHX_HI_AT + SMK_PATCHX_HI_N * 0x2000u - SMK_SETTINGS_AT),
               "SMK: an object touches a protected range");
