/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* I2S output (ALNK0 -> external codec) and the
 * audio ISR: per half buffer, blocks of CTL samples: mix_block (fx.c: events ->
 * each synth part -> dist -> level / pan -> sends -> buses -> master) -> USB audio (uac_tap) -> the metronome's
 * click (click.c, the DAC only) -> 24-bit stereo. */
/* registers: hal/fm1_audio.h */
#define HALF_WORDS (HALF_FRAMES * 2u)
#define DAC_TICKS 544u            /* TIMER4 ticks per I2S frame (24 MHz / 44,117.6 Hz) */
#define CPU_AVG (4096u / HALF_FRAMES)   /* halves the load meter averages: ~93 ms whatever the half */
#define OUT_SHIFT 7               /* Q15 -> 24-bit, -6 dBFS ceiling (M0f ran clean at -18 dBFS) */

static int32_t abuf[2u * HALF_WORDS] __attribute__((aligned(4)));

/* diagnostics, kept across resets and UBOOT entry: read with `fm1t memr` */
#define DBG_MAGIC 0x44424731u                       /* "DBG1" */
struct felucca_dbg {
    uint32_t magic, halves, max_us, nested, in_audio, late, timer_irqs, ui_frames;
    uint32_t last_us, cpu_q8, boots;
    uint32_t stage, page, home;           /* where the main loop is (breadcrumbs); home: always 0 since 0.2.14 (the OVERVIEW screen is gone; the field keeps the layout) */
    uint32_t prev_stage, prev_page, prev_home, prev_rst, prev_frames;   /* as found at boot */
    uint32_t erase_max_us, erase_last_us, prog_max_us, erases;   /* DUELL 0.2.11: the flash driver's own times (storage_hw.c), since boot */
} felucca_dbg __attribute__((section(".noinit")));
static volatile uint32_t audio_halves, audio_max_us;
static volatile uint32_t t5_nested_ticks;              /* TIMER4 ticks TIMER5 spent nested in this ISR (main.c) */
static uint32_t audio_cpu_rem;                         /* keep the fractional IIR step: no low-load bias */

/* DUELL CPU load meter (MENU > SYSTEM > TEST, ABOUT: ui_menu.c cpu_meter): what the audio ISR keeps per I2S half, three adds and
 * a compare, no division. cpu_sum/cpu_n: the sum of the halves' render times (us) and how many; cpu_pk: the longest half
 * (us, the UI reads and clears it); cpu_miss: halves that took longer than the half's time (HALF_FRAMES / FS) or that the DMA
 * overtook, counted since power-on. The time is fm1_ticks() across the half minus TIMER5's nested ticks (the key scan),
 * i.e. the audio's own work. Emulator and host: FELUCCA_METER_EST replaces it by a model (below): there is no clock there */
/* (the counters and CPU_BUDGET_US: core.h) */
#ifdef FELUCCA_METER_EST
/* an ESTIMATE, not a measurement: 16 % for the buses, plus 10 % a sounding voice (a voice in its release too), of the half's
 * time. 8 voices read 96 %, so the warning colours can be seen in the emulator; only the device measures */
static uint32_t meter_est_us(void)
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NLIVE; p++)
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].active != 0;
    return CPU_BUDGET_US * (16u + 10u * n) / 100u;
}
#endif

/* MASTER, the DAC's volume (DUELL 0.2.25, owner 2026-10-20: "the master volume glitches when I rotate it, in the headphone"). The codec has no volume
 * register the firmware writes (hal/fm1_audio.h: only its enable lines), so this is the only volume there is. mix_block makes the mix at the fixed level
 * MASTER_USB_REF and USB took it; here out (Q15) becomes Q15 x 2^OUT_SHIFT (the 24-bit stream's scale) times song.master_q12 / MASTER_USB_REF, with two
 * differences from the stage that was here (usb_fixed_dac: out * m >> 11 per block, in Q15):
 *  - the gain glides: mdac_g (Q24: Q12 x 2^12) follows the knob's level with a one-pole step every sample (1/512 of the way: tau 512 samples = 11.6 ms), so a
 *    knob step (the level is read every block and changes every main-loop pass) is a glide of a few ms, not a step of up to 6 dB at the low end of the
 *    knob (a step of s dB is a jump of (10^(s/20) - 1) x the signal at the block seam: a click). The step is rounded away from the target, so the gain
 *    reaches it exactly (settled, the result is the old stage's bit for bit at the same level and 2^OUT_SHIFT times its Q15 value);
 *  - the result keeps OUT_SHIFT fractional bits of Q15 (it used to be cut to Q15 first): a quiet knob position (gain 1/30) no longer loses 5 bits of the
 *    signal to truncation (distortion and grain in headphones at a low volume).
 * (0.2.30: the knob has no boost any more, its top is unity; the saturation stays as a safety.) A gain above 1 saturates at the old ceiling, never wraps. mdac_g < 0: not started (power-on: the first block takes the
 * knob's level, no fade-in). */
#ifndef MDAC_SHIFT
#define MDAC_SHIFT 9                                   /* the glide: 1/512 of the way a sample (tau 512 samples = 11.6 ms) */
#endif
/* Saturation to the 24-bit scale's 16-bit-Q15 range: +4,194,303 .. -4,194,304 (the old stage clipped at +4,194,176 .. -4,194,304). Written as a test of the bits above and the sign
 * mask instead of two limits, because a limit of -4,194,304 as a literal is an address in the chip ROM to build.py's "no reference to the ROM" check (0xFFC00000..) */
static inline int32_t mdac_sat(int32_t v)
{
    int32_t s = v >> 31;
    return (v >> (15 + OUT_SHIFT)) != s ? s ^ ((1 << (15 + OUT_SHIFT)) - 1) : v;
}
static int32_t mdac_g = -1;
static inline int32_t mdac_apply(int32_t v, int32_t k, uint32_t s)   /* v (Q15) x the gain (k = g >> s) -> Q15 x 2^OUT_SHIFT, saturated */
{
    return mdac_sat((v * k) >> (23u - s - OUT_SHIFT));
}
static __attribute__((noinline)) void master_dac(int32_t *out, uint32_t n)   /* audio ISR, after uac_tap */
{
    uint32_t i, s, q = song.master_q12, p = master_g16;
    int32_t t = (int32_t)((p >> 4) == q ? p << 8 : q << 12), g = mdac_g < 0 ? t : mdac_g;   /* (the target, Q24) */
    for (i = 0; i < n && g != t; i++) {                /* gliding: the gain moves a step every sample */
        int32_t d = t - g, k;
        g += d > 0 ? (d + ((1 << MDAC_SHIFT) - 1)) >> MDAC_SHIFT : d >> MDAC_SHIFT;
        s = g < (1 << 21) ? 6u : 9u;                   /* (m below 512: 3 more bits of the gain, where a whole step of it is a large part of it; |out| <= 32768, k < 2^15: fits int32) */
        k = g >> s;
        out[2u * i] = mdac_apply(out[2u * i], k, s);
        out[2u * i + 1u] = mdac_apply(out[2u * i + 1u], k, s);
    }
    s = g < (1 << 21) ? 6u : 9u;                       /* at rest (the rest of the block, or all of it): one gain */
    for (; i < n; i++) {
        out[2u * i] = mdac_apply(out[2u * i], g >> s, s);
        out[2u * i + 1u] = mdac_apply(out[2u * i + 1u], g >> s, s);
    }
    mdac_g = g;
}

static void audio_block(int32_t *out, uint32_t n)       /* mix (fx.c), then Q15 -> 24 bit */
{
    uint32_t i;
    mix_block(out, n);
#if FELUCCA_UAC
    uac_tap(out, n);                                    /* the USB audio input: the same master output, at the fixed level */
#endif
    master_dac(out, n);                                 /* MASTER's volume for the DAC (speaker, headphones): out is Q15 x 2^OUT_SHIFT from here */
    if (!scope_mix)                                     /* the HOME scope (SCOPE OUT): the music, before the click */
        for (i = 1; i < n; i += 2u)                     /* (MIX: fx.c mix_block took it) */
            scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)(out[2u * i] >> OUT_SHIFT);
    click_render(out, n, OUT_SHIFT);                    /* the metronome (click.c): the DAC only, not USB */
}

/* DUELL 0.2.11, the flash duck (SPEC 6.3): a flash erase keeps the interrupts off for its whole time (~40 ms; the device's own
 * figure: felucca_dbg.erase_max_us, the TEST screen), and the DMA loops abuf meanwhile, so the buffer must hold silence. Before
 * an erase (and a long program burst) the main loop asks for a duck (audio_duck): the ISR fades the DAC output to 0 over one half
 * (2.9 ms, linear), then renders on in silence (voices, envelopes and effects run as ever: what sounded keeps sounding after) and
 * counts the silent halves; after two, both halves of abuf are 0 and the erase may stop the interrupts. audio_unduck: the next
 * half fades back in over one half. A soft dip instead of the old step to silence and back (two clicks). USB (uac_tap) is not ducked */
#define DUCK_STEP (32768u / HALF_FRAMES)               /* Q15 a frame: 1 .. 0 over one half */
static volatile uint8_t duck_on;                       /* main loop: 1 fade out and stay silent, 0 fade back in */
static volatile uint8_t duck_zero;                     /* ISR: whole silent halves since the fade ended (up to 2: all of abuf) */
static uint32_t duck_g = 32768u;                       /* ISR: the gain, Q15 */
static __attribute__((noinline)) void duck_half(int32_t *o)   /* the half just rendered (24 bit) */
{
    uint32_t i, g0 = duck_g;
    for (i = 0; i < 2u * HALF_FRAMES; i += 2u) {
        if (duck_on)
            duck_g = duck_g > DUCK_STEP ? duck_g - DUCK_STEP : 0u;
        else
            duck_g = duck_g + DUCK_STEP < 32768u ? duck_g + DUCK_STEP : 32768u;
        o[i] = (int32_t)((int64_t)o[i] * duck_g >> 15);
        o[i + 1u] = (int32_t)((int64_t)o[i + 1u] * duck_g >> 15);
    }
    if (!duck_on)
        duck_zero = 0;
    else if (!g0 && duck_zero < 2u)
        duck_zero++;
}
#ifndef AUDIO_DUCK_IDLE
#define AUDIO_DUCK_IDLE() ((void)0)                    /* (host tests: their simulated ISR runs here) */
#endif
#define DUCK_WAIT_TICKS (20000u * FM1_TICKS_PER_US)    /* 20 ms at most (the audio not running yet, the IRQs off: go on as before) */
static void audio_duck(void)                           /* main loop: fade out, wait until both halves of abuf are silent */
{
    uint32_t t0 = fm1_ticks();
    duck_on = 1;
    while (duck_zero < 2u && audio_halves && fm1_ticks() - t0 < DUCK_WAIT_TICKS)
        AUDIO_DUCK_IDLE();
}
static void audio_unduck(void) { duck_on = 0; }       /* the next half fades back in */

/* Overload: two halves in a row above 85 % (the render plus TIMER5 nested in it), fade one voice over the
 * next block, and one more each half while it lasts. Keep each part's
 * lowest POLY note and MONO / LEGATO / UNISON lead, as the shared budget does.
 * DUELL 0.2.11 (SPEC 6.3): a single half over 100 % (it missed) sheds at once, without waiting for a second one: the
 * SHED_FAST_N oldest released voices (their tails) fade over the next block; held notes only by the rule above */
#define SHED_HELD 1u                                   /* shed_req bits: the 85 % rule (shed_voice) */
#define SHED_FAST 2u                                   /* .. a half over 100 %: released voices at once (shed_released) */
#define SHED_FAST_N 2u
static volatile uint8_t shed_req;
static uint8_t shed_over;                              /* bit k: the half k halves ago was over 85 % */
#define SHED_TICKS ((HALF_FRAMES * 1000000u / FS) * 85u / 100u * FM1_TICKS_PER_US)   /* 85 % of a half */
#define SHED_FULL_TICKS ((HALF_FRAMES * 1000000u / FS) * FM1_TICKS_PER_US)                 /* the whole half */
static uint32_t shed_count;

/* end of a half: all = its ticks, TIMER5 nested in it included -> the render alone in us. The deadline sees
 * both; shed after 2 overloaded halves in a row (out of line: keeps the ISR's render loops as they were) */
static __attribute__((noinline)) uint32_t shed_check(uint32_t all)
{
    shed_over = (uint8_t)(shed_over << 1 | (all > SHED_TICKS));
    if ((shed_over & 3u) == 3u)
        shed_req |= SHED_HELD;
    if (all > SHED_FULL_TICKS)
        shed_req |= SHED_FAST;
    ovl_step((shed_over & 3u) == 3u, song.cpu_q8);     /* (0.2.25: the overload guard, fx.c: the same rule; the meter's load lets it go) */
    return (all - t5_nested_ticks) / FM1_TICKS_PER_US;
}

static uint32_t shed_released(uint32_t n)             /* the n oldest released voices fade out; returns how many */
{
    uint32_t p, i, k;
    for (k = 0; k < n; k++) {
        voice_t *best = 0;
        for (p = 0; p < NPART; p++)
            for (i = 0; i < NVOICE; i++) {
                voice_t *v = &trk[p].v[i];
                if (v->active && !v->gate && v->stage != 4u && (!best || v->age < best->age))
                    best = v;
            }
        if (!best)
            break;
        voice_kill(best);
        shed_count++;
    }
    return k;
}

static void shed_voice(void)
{
    uint32_t p, i;
    voice_t *best = 0;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[p].v[i];
            if (v->active && !v->gate && v->stage != 4u && (!best || v->env < best->env))
                best = v;
        }
    if (best) {
        voice_kill(best);
        shed_count++;
        return;
    }
    for (p = 0; p < NPART; p++) {
        const track_t *t = &trk[p];
        uint32_t mode = trk_vmode(t), low = mode == V_POLY ? lowest_held(t) : 0u;
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[p].v[i];
            if (v->active && v->gate && v->stage != 4u &&
                ((mode == V_POLY && i != low) || (mode != V_POLY && i > 0u))) {
                if (!best || v->age < best->age)
                    best = v;
            }
        }
    }
    if (best) {
        voice_kill(best);
        shed_count++;
    }
}

void fm1_alnk0_irq(void)                       /* via isr_alnk0 (hal/fm1_isr.S) */
{
    uint8_t p;
    uint32_t t0;
    felucca_dbg.in_audio = 1;                   /* first: TIMER5 nests from here on (main.c) */
    p = fm1_audio_pending();
#if DUELL_BENCH
    if (bench_tl_on && (p & FM1_AUDIO_HALF))            /* (bench builds only, bench suite stage 1: the timeline's due events into the MIDI-in ring, */
        bench_half();                                   /* before the clock starts: the half's own figure does not count it; src/bench.c) */
#endif
    t0 = fm1_ticks();
    t5_nested_ticks = 0;
    fm1_audio_ack_aux(p);
    if (p & FM1_AUDIO_HALF) {
        uint32_t half = fm1_audio_free_half(), b, us, late;
        int32_t *o = &abuf[half * HALF_WORDS];
        if (shed_req) {
            uint32_t r = shed_req;
            shed_req = 0;
            if (!((r & SHED_FAST) && shed_released(SHED_FAST_N)) && (r & SHED_HELD))
                shed_voice();
        }
#if FELUCCA_UAC
        uac_render_start();
#endif
#if DUELL_TWOCORE
        tc_half_begin(t0);                              /* (the half's clock for core 0's deadlines: twocore_run.c) */
#endif
        for (b = 0; b < HALF_FRAMES; b += CTL) {
#ifdef FM1_INPUT_LAT
            kb_out_tick = t0 + (HALF_FRAMES + b) * DAC_TICKS;   /* when this block plays (seq.c kb_lat) */
#endif
            audio_block(o + 2u * b, CTL);
        }
        if (duck_on || duck_g < 32768u)                 /* (the flash duck: only around a flash write) */
            duck_half(o);
        fm1_audio_ack_half();
        audio_halves++;
        us = shed_check(fm1_ticks() - t0);
#ifdef FELUCCA_METER_EST
        us = meter_est_us();
#endif
        if (us > audio_max_us)
            audio_max_us = us;
        cpu_sum += us;                                  /* the meter: no division here */
        cpu_n++;
        if (us > cpu_pk)
            cpu_pk = us;
        {
            uint32_t load = song.cpu_q8 * (CPU_AVG - 1u) + (us * 256u) / (HALF_FRAMES * 1000000u / FS) + audio_cpu_rem;
            song.cpu_q8 = load / CPU_AVG;
            audio_cpu_rem = load % CPU_AVG;
        }
        late = fm1_audio_free_half() != half;
        cpu_miss += (us > CPU_BUDGET_US) | late;        /* (the meter's: over the half's time, or overtaken) */
        if (late)
            felucca_dbg.late++;                         /* the DMA moved on while we rendered */
        felucca_dbg.halves++;
        felucca_dbg.last_us = us;
        if (us > felucca_dbg.max_us)
            felucca_dbg.max_us = us;
        felucca_dbg.cpu_q8 = song.cpu_q8;
    }
    felucca_dbg.in_audio = 0;
}
extern void isr_alnk0(void);

static void audio_init(void)                   /* codec and ALNK0 bring-up (fm1_audio.h) */
{
    uint32_t i;
    for (i = 0; i < 2u * HALF_WORDS; i++)
        abuf[i] = 0;
    fm1_audio_init(abuf, HALF_WORDS, isr_alnk0, 3);
}
