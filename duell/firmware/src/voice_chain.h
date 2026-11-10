/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The VOICE CHAIN (cloud job B, docs/design/TWIN_FILTER_DESIGN.md 2.1, SUB_AND_CYCLE_DESIGN.md A1): the per-note path "engine at full level, SUB, filter 1, filter 2 (with DRIVE), then the amp".
 * Job B builds the sequence and the empty places; job E fills chain_f2.c (filter 2 and DRIVE: chain_f2_on, chain_f2), job F chain_sub.c (the SUB LAYER: chain_sub_on, chain_sub; the CYCLE
 * source: cycle_value). voice.c includes these files before track_render, which has ONE branch for the chain (voice.c, "the voice chain"); with both _on functions at 0 (the product, until
 * jobs E and F) a voice never enters it and plays on track_render's plain path, bit for bit as before.
 *
 * voice_t.chn (core.h) says where a voice was last block: VCH_OFF the plain path; VCH_RUN the chain; VCH_FRESH a note that has not rendered yet (voice_start); VCH_NEW is what the stage hooks see on
 * the first block a voice is in the chain: they start from rest (filter 2's state, the sub's phase). A voice that enters or leaves the chain while it sounds is cross-faded over one block
 * (chain_block): both paths run, the block goes from one to the other (the way voice.c uf_edge does for filter 1's bypass edge). The stage hooks may be called for any voice in the chain
 * and run in the audio ISR, on core 0 or core 1 (the part's own): no global shared by the parts, no locks */
#ifndef DUELL_VOICE_CHAIN_H
#define DUELL_VOICE_CHAIN_H
int chain_f2_on(track_t *t, voice_t *v);   /* filter 2 (TYPE not OFF) runs for voice v this block (chain_f2.c, job E) */
int chain_sub_on(track_t *t, voice_t *v);  /* the SUB LAYER (LEVEL above 0) runs for voice v this block (chain_sub.c, job F) */
/* the chain runs for voice v this block */
#ifdef DUELL_CHAIN_FORCE
static uint8_t chain_force;                /* (a test: tests/xvalues_test.c forces the chain on, with the stages empty: the sequence is proved against the plain path) */
#define CHAIN_FORCED chain_force
#else
#define CHAIN_FORCED 0
#endif
static inline uint32_t chain_on(track_t *t, voice_t *v) { return (uint32_t)(chain_f2_on(t, v) | chain_sub_on(t, v)) | CHAIN_FORCED; }
#endif /* DUELL_VOICE_CHAIN_H */
