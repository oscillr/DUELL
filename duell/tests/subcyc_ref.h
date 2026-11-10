/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.1 audit: REF 0, 2, 3, 4 re-based to the 1.0 line (identical on 1.0 and 1.1): the chain path for every note with filter 1 on (voice_chain.c CHAIN_ALL, cloud job J) and the depth cards' x 1.0156 rule (ext.h MX_AMT / DEP_AMT) changed these renders; with both put back to the old values (CHAIN_ALL 0, MX_AMT(a) = a) the five hashes come out exactly as the old ones, 0x73ffe0ef5c4d093c 0xcb3d40d4f986cd97 0x18ee7f80e1c5c0c8 0x1cfb662d4bc9f049 0xdbe2ea0ceeab6eb7.
/* final line 2026-10-21: REF 1 and 2 re-based on purpose to the merged line (FM6 + PHASE DUEL: PHASE seam bridge; ANALOG SPREAD: SPREAD's mix ramp, hunt1 / hunt4); were 0xfd4cf58f67e29032 and 0x78f918e85c3a6ad8 (cloud/base3-wave2).
 * tests/subcyc_test.c: the hashes of its REF scenes on cloud/base3-wave2 (made by building the test with -DSUBCYC_BASE -DSUBCYC_REFGEN against it: the sub's values set with LEVEL 0, CYCLE's
 * values set and read by no slot, STEPS OFF) */
static const unsigned long long SUBCYC_REF[5] = {0x2a1af16253f75c09ull, 0xcb3d40d4f986cd97ull, 0xbaa1b7c4f91ad3ccull, 0x57670c2dca3477b2ull, 0x4632a35b6ca03a1full};
