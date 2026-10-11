/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* DUELL: the per-track SLICER insert (GATE / STUT, P_SLCR..P_SLDEPTH) is removed: no page, no processing; its four ids are
 * free (core.h P_FREEA..P_FREED, reserved for the universal filter). What stays here is what the FX layer (perform.c) uses:
 *   sl_buf      32 KB in .pool, the FX hold layer's stereo loop of 8192 frames at 22.05 kHz (PB_FRAMES)
 *   SL_RAMP*    the 2.9 ms ramp (128 samples) and SL_SLOPE, the gains' slew a sample (perform.c, fx.c's INSERT)
 * (the buffer kept its name and shape: perform.c addresses it as &sl_buf[0][0]) */
#define SL_LEN 4096u                    /* 16-bit samples a track: NTRK x SL_LEN = 8192 stereo frames (perform.c PB_FRAMES) */
#define SL_RAMP_LOG2 7
#define SL_RAMP (1 << SL_RAMP_LOG2)     /* 128 samples, 2.9 ms */
#define SL_SLOPE (32768 / SL_RAMP)

static int16_t sl_buf[NTRK][SL_LEN] __attribute__((section(".pool")));
