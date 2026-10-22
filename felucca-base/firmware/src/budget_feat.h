/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* NOTES budget: the new features' items (cloud job B's stub). bd_feat adds what the TWIN FILTER, the SUB LAYER and the CYCLE envelope cost a part to the item r (budget.c bd_part calls it once, at
 * the end): jobs E (TWIN), F (SUB, CYCLE) fill their own marked section of budget_feat.c and no other. Host instructions a sample, as the table of budget.c; the numbers come from the
 * Mac bench (BD_* rows), a cloud job writes ratio estimates marked est. Included by budget.c after bd_part_t */
#ifndef DUELL_BUDGET_FEAT_H
#define DUELL_BUDGET_FEAT_H
static void bd_feat(const track_t *t, bd_part_t *r);
#endif
