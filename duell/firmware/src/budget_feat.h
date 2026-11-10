/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* NOTES budget: the new features' items (cloud job B's stub). bd_feat adds what the TWIN FILTER, the SUB LAYER and the CYCLE envelope cost a part to the item r (budget.c bd_part calls it once, at
 * the end): jobs E (TWIN), F (SUB, CYCLE) fill their own marked section of budget_feat.c and no other. Host instructions a sample, as the table of budget.c; the numbers come from the
 * Mac bench (BD_* rows), a cloud job writes ratio estimates marked est. Included by budget.c after bd_part_t */
#ifndef DUELL_BUDGET_FEAT_H
#define DUELL_BUDGET_FEAT_H
typedef struct { uint32_t ty, drive, par, mod; } bd_tw_t;   /* the TWIN's items: the dearest TYPE's cost, and whether DRIVE, PARALLEL, the modulation run in any of the states (budget_feat.c) */
static void bd_feat(const track_t *t, bd_part_t *r, const bd_tw_t *wm);   /* wm: the TWIN of the part's MORPH ends as they sound (DUELL 1.1 item 142: bd_part blends each end once) */
#endif
