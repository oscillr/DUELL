/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* DUELL: the per-track SLICER insert (GATE / STUT, P_SLCR..P_SLDEPTH) is removed, and so is the FX hold layer (0.2.2) that
 * used its 32 KB loop buffer: no buffer is left. What stays is the 2.9 ms ramp every gain glide uses (fx.c's INSERT, the
 * DUCK): SL_RAMP* and SL_SLOPE, the gains' slew a sample */
#define SL_RAMP_LOG2 7
#define SL_RAMP (1 << SL_RAMP_LOG2)     /* 128 samples, 2.9 ms */
#define SL_SLOPE (32768 / SL_RAMP)
