/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The real audio ISR with a simulated DMA and clock: overload must free work
 * within one block without cutting another part's lead or bass. */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define FM1_AUDIO_HALF 0x80u
#define FM1_TICKS_PER_US 1u
static uint32_t host_clock, host_us, host_half_reads, host_dma_advanced, host_acks, host_nest;
static uint8_t host_pending;
static volatile uint32_t t5_nested_ticks;              /* audio.c's: TIMER5 nested in the render (main.c) */
static uint32_t fm1_ticks(void) { t5_nested_ticks = host_nest; host_clock += host_us; return host_clock; }
static uint8_t fm1_audio_pending(void) { return host_pending; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static uint32_t host_alt, host_alt_half;               /* (the flash duck test: the DMA alternates the halves) */
static uint32_t fm1_audio_free_half(void) { return host_alt ? host_alt_half : host_dma_advanced && host_half_reads++ > 0u; }
static void host_isr(void);                            /* audio.c audio_duck's wait: the ISR runs meanwhile */
#define AUDIO_DUCK_IDLE() host_isr()
static void fm1_audio_ack_half(void) { host_acks++; }
static void fm1_audio_init(int32_t *b, uint32_t n, void (*isr)(void), uint32_t p)
{ (void)b; (void)n; (void)isr; (void)p; }
void isr_alnk0(void) {}
#define FELUCCA_UAC 1                                  /* audio.c taps USB audio: here into host_tap */
static int32_t host_tap[2 * 64];
static uint32_t host_tap_n;
static void uac_tap(const int32_t *out, uint32_t n) { memcpy(host_tap, out, 8u * n); host_tap_n = n; }
static void uac_render_start(void) {}
#include "../firmware/src/audio.c"
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static int32_t host_pot = 1023;                        /* the MASTER pot as the ADC reads it (-1: timeout) */
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_MASTER ? host_pot : 620; }
#include "../firmware/src/master.c"

static int bad;
static void check(const char *what, int ok)
{
    printf("audio: %-74s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void fresh(void)
{
    uint32_t p;
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    for (p = 0; p < NPART; p++) host_preset(&trk[p], 0, 0);
    memset(&felucca_dbg, 0, sizeof felucca_dbg);
    audio_cpu_rem = audio_halves = audio_max_us = 0;
    shed_req = 0; shed_count = 0; shed_over = 0;
    host_clock = host_us = host_half_reads = host_dma_advanced = host_acks = host_nest = 0;
    host_pending = FM1_AUDIO_HALF;
    transport_req = panic_req = 0;
    duck_on = duck_zero = 0; duck_g = 32768u; host_alt = host_alt_half = 0;
}
static voice_t *held(track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].note == note) return &t->v[i];
    return 0;
}
static void overload(void)
{
    voice_t *lead, *bass, *extra;
    uint32_t mode, i;
    int32_t out[2 * CTL];
    fresh();
    trk[0].p[P_VOICE] = V_MONO;
    trk_note_on(&trk[0], 40, 100);
    trk[1].p[P_VOICE] = V_POLY;
    trk_note_on(&trk[1], 48, 100);
    trk_note_on(&trk[1], 60, 100);
    lead = held(&trk[0], 40); bass = held(&trk[1], 48); extra = held(&trk[1], 60);
    shed_voice();
    check("overload keeps each part's MONO lead and lowest POLY note",
          lead && bass && extra && lead->gate && bass->gate && extra->stage == 4u && shed_count == 1u);
    mix_block(out, CTL);
    check("overload frees its victim within one control block", extra && !extra->active && voices_busy() == 2u);
    shed_voice();
    check("a lead alone on each part stays protected", lead->gate && bass->gate && shed_count == 1u);

    fresh();
    host_preset(&trk[0], ENGI_DRUM, 0);
    trk_note_on(&trk[0], 36, 100); trk_note_on(&trk[0], 38, 100);
    lead = held(&trk[0], 36); extra = held(&trk[0], 38);
    shed_voice(); mix_block(out, CTL);
    check("overload frees a held one-shot drum without waiting for its decay",
          lead && extra && lead->active && !extra->active && voices_busy() == 1u);

    fresh();
    trk[0].p[P_VOICE] = V_UNISON;
    trk_note_on(&trk[0], 55, 100);
    shed_voice(); mix_block(out, CTL);
    check("UNISON loses an extra voice and retains its lead", trk[0].v[0].active && trk[0].v[0].gate && voices_busy() == NVOICE - 1u);

    for (mode = V_MONO; mode <= V_LEGATO; mode++) {
        fresh(); trk[0].p[P_VOICE] = V_POLY;
        for (i = 0; i < NVOICE; i++) trk_note_on(&trk[0], 48u + 2u * i, 100);
        trk[0].p[P_VOICE] = (int16_t)mode;
        shed_voice(); mix_block(out, CTL);
        check("overload can reclaim stale POLY voices after a MONO / LEGATO change",
              shed_count == 1u && trk[0].v[0].gate && voices_busy() == NVOICE - 1u);
    }

    fresh();
    trk[0].p[P_VOICE] = V_POLY;
    trk_note_on(&trk[0], 60, 100); trk_note_on(&trk[0], 64, 100);
    lead = held(&trk[0], 60); extra = held(&trk[0], 64);
    trk_note_off(&trk[0], 60); trk_note_off(&trk[0], 64);
    lead->env = 10000; extra->env = 10;
    shed_voice();
    check("released voices shed the quieter tail first", extra->stage == 4u && lead->stage != 4u);
}
static void dma(void)
{
    uint32_t i, expected;
    fresh();
    host_pending = 0;
    fm1_alnk0_irq();
    check("an auxiliary interrupt does not render a DMA half", !host_acks && !audio_halves && !felucca_dbg.in_audio);
    host_pending = FM1_AUDIO_HALF;
    host_us = 2700;                                    /* (93 % of the half: over 85 %, under 100 %) */
    fm1_alnk0_irq();
    check("one slow half (93 %) records elapsed load but does not shed yet", host_acks == 1u && !shed_req && felucca_dbg.last_us == 2700u && !felucca_dbg.late);
    fm1_alnk0_irq();
    check("a second slow half in a row requests shedding", host_acks == 2u && shed_req == SHED_HELD);
    shed_req = 0;
    host_half_reads = 0; host_dma_advanced = 1; host_us = 100;
    fm1_alnk0_irq();
    check("DMA advancing during a render is counted once", host_acks == 3u && felucca_dbg.late == 1u && !felucca_dbg.in_audio && !shed_req);
    fresh();
    expected = HALF_FRAMES * 1000000u / FS;            /* the half's deadline in us */
    host_us = expected * 90u / 100u; host_nest = expected * 30u / 100u;   /* the render 60 %, TIMER5 nested 30 % */
    fm1_alnk0_irq(); fm1_alnk0_irq();
    check("TIMER5 nested in the render counts toward the deadline", shed_req && felucca_dbg.last_us == host_us - host_nest);
    fresh();
    host_us = 2700;
    fm1_alnk0_irq(); host_us = 100; fm1_alnk0_irq(); host_us = 2700; fm1_alnk0_irq();
    check("slow halves (93 %) that are not consecutive do not shed", !shed_req);
    fresh();                                           /* DUELL 0.2.11: one half over 100 % sheds at once (released voices only) */
    host_us = 5000;
    fm1_alnk0_irq();
    check("0.2.11: a single half over 100 % requests the fast shed at once (not the held-note rule)", shed_req == SHED_FAST);
    fresh();
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_REL] = 100;                             /* (long tails: none ends by itself in the half) */
    for (i = 0; i < 6u; i++) trk_note_on(&trk[0], 48u + 2u * i, 100);
    host_us = 100;
    for (i = 0; i < 8u; i++) fm1_alnk0_irq();        /* (the attacks: a tail released at level 0 ends at once) */
    shed_req = 0; shed_count = 0;
    trk_note_off(&trk[0], 50); trk_note_off(&trk[0], 54); trk_note_off(&trk[0], 58);   /* released, oldest first: 50, 54, 58 */
    shed_req = SHED_FAST; host_us = 100;
    fm1_alnk0_irq();
    {
        uint32_t k, gone = 0x3Fu, held_ok = 1;      /* (the faded ones are gone after the half: their one-block fade) */
        for (k = 0; k < NVOICE; k++) {
            voice_t *v = &trk[0].v[k];
            if (v->active && v->stage != 4u) gone &= ~(1u << ((v->note - 48u) / 2u));
            if (v->gate && (!v->active || v->stage == 4u)) held_ok = 0;
        }
        check("0.2.11: the fast shed fades the 2 oldest released voices (50, 54), keeps the third tail (58) and every held note",
              gone == ((1u << 1) | (1u << 3)) && held_ok && shed_count == 2u);
    }
    fresh();
    trk[0].p[P_VOICE] = V_POLY;
    for (i = 0; i < 4u; i++) trk_note_on(&trk[0], 48u + 2u * i, 100);
    shed_req = SHED_FAST; host_us = 100;
    fm1_alnk0_irq();
    check("0.2.11: the fast shed with no released voice cuts no held note", shed_count == 0u && voices_busy() == 4u);
    fresh();
    trk[0].p[P_VOICE] = V_POLY;
    for (i = 0; i < 4u; i++) trk_note_on(&trk[0], 48u + 2u * i, 100);
    shed_req = SHED_FAST | SHED_HELD; host_us = 100;
    fm1_alnk0_irq();
    check("0.2.11: no released voice, the 85 % rule as well: one held note (not the bass) as before", shed_count == 1u && trk[0].v[0].gate);
    fresh();
    host_us = 580;
    for (i = 0; i < 512u; i++) fm1_alnk0_irq();
    expected = host_us * 256u / (HALF_FRAMES * 1000000u / FS);
    check("steady CPU load converges without integer smoothing bias", song.cpu_q8 >= expected - 1u && song.cpu_q8 <= expected);
    /* the CPU load meter's counters (core.h): one add per half, the longest half, the misses (over the half's time, or the DMA overtook) */
    cpu_sum = cpu_n = cpu_pk = cpu_miss = 0;
    fresh();
    host_us = 580;
    for (i = 0; i < 100u; i++) fm1_alnk0_irq();
    check("meter: 100 halves counted, their time summed, the longest kept, none missed", cpu_n == 100u && cpu_sum == 100u * 580u && cpu_pk == 580u && cpu_miss == 0u);
    host_us = HALF_FRAMES * 1000000u / FS + 10u;
    fm1_alnk0_irq();
    check("meter: a half over its time is a miss", cpu_miss == 1u && cpu_pk == host_us);
    host_us = 100; host_half_reads = 0; host_dma_advanced = 1;
    fm1_alnk0_irq();
    check("meter: a half the DMA overtook is a miss too", cpu_miss == 2u && cpu_pk > 100u);
}
/* MENU > USB LEVEL (fx.c fx_usb_fixed, audio.c audio_block): MASTER (default) as before, USB = the DAC's signal and
 * follows the knob; FIXED: USB is exactly MASTER mode's signal at master_q12 = MASTER_USB_REF (2048, the default knob)
 * whatever MASTER is (MASTER 0: USB still plays, the DAC is silent), the DAC exactly USB * master_q12 / 2048, saturated */
static int32_t usb_lim_min;                            /* the lowest lim_env of the last usb_run (the limiter's engagement) */
static int usb_same_as_ref;                            /* the last FIXED usb_run's USB feed equals the reference run's, sample for sample */
static int32_t usb_ref_tap[400][64];
static int usb_perf;                                   /* PERFORM busy during usb_run: DRIVE held and K3 FILTER a little low-pass (fx.c perf_master) */
static double block_rms(const int32_t *b, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = 0; i < 2u * n; i++) s += (double)b[i] * b[i];
    return sqrt(s / (2.0 * n));
}
static double usb_run(uint32_t fixed, uint32_t master, int dac_is_tap_scaled_ok[1], double *dac_rms)
{
    int32_t out[2 * 32];
    uint32_t b, i;
    double usb = 0, dac = 0;
    fresh();
    lim_env = LIM_T; dc_l = dc_r = dce_l = dce_r = 0;    /* (the master's state as at power-on: runs compare) */
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_DIST] = trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;   /* (no tails between runs) */
    trk_note_on(&trk[0], 48, 110); trk_note_on(&trk[0], 55, 110); trk_note_on(&trk[0], 64, 110);
    memset(&pf, 0, sizeof pf);                         /* (PERFORM from rest, as perform_test.c perf_reset: runs compare) */
    pf.src = pf.next = PF_N;
    pf.lc = PF_TOP;
    pf.pbeat = perf_beat;
    pf.mg[0] = pf.mg[1] = pf.mg[2] = pf.mg[3] = 32768;
    perf_held = perf_act = 0;
    perf_kill = 0;
    perf_space = 0;
    perf_depth = 100;
    perf_filt = usb_perf ? -20 : 0;
    if (usb_perf)
        perf_press(PF_DRV, 1);
    fx_usb_fixed = (uint8_t)fixed;
    song.master_q12 = master;
    dac_is_tap_scaled_ok[0] = 1;
    usb_lim_min = LIM_T; usb_same_as_ref = 1;
    for (b = 0; b < 400u; b++) {                       /* ~0.3 s */
        audio_block(out, 32);
        if (lim_env < usb_lim_min) usb_lim_min = lim_env;
        for (i = 0; i < 64u; i++) {
            int32_t v = fixed ? (host_tap[i] * (int32_t)master) >> 11 : host_tap[i];
            int32_t want = (v > 32767 ? 32767 : v < -32768 ? -32768 : v) << OUT_SHIFT;
            if (out[i] != want) dac_is_tap_scaled_ok[0] = 0;
            if (!fixed && master == 2048u) usb_ref_tap[b][i] = host_tap[i];   /* (the reference: MASTER at 2048) */
            else if (fixed && usb_ref_tap[b][i] != host_tap[i]) usb_same_as_ref = 0;
        }
        if (b >= 100u) { usb += block_rms(host_tap, 32); dac += block_rms(out, 32) / (1 << OUT_SHIFT); }
    }
    fx_usb_fixed = 0;
    if (usb_perf)
        perf_press(PF_DRV, 0);
    perf_filt = 0;
    *dac_rms = dac / 300.0;
    return usb / 300.0;
}
static void usb_level(void)
{
    int ok_full, ok_quiet, ok_off, ok_fixed, ok_fixed0, ok_ref, ok_hi, ok_def;
    double d_full, d_quiet, d_off, d_fixed, d_fixed0, d_ref, d_hi, d_def, d_loop;
    int32_t lim_fixed_min = LIM_T, k, same = 1;
    double u_full = usb_run(0, 4096, &ok_full, &d_full), u_quiet = usb_run(0, 1024, &ok_quiet, &d_quiet);
    double u_off = usb_run(0, 0, &ok_off, &d_off);
    double u_ref = usb_run(0, 2048, &ok_ref, &d_ref), u_fixed, u_fixed0, u_hi, u_def;
    int32_t lim_ref_min = usb_lim_min;                 /* (the reference run: MASTER at 2048) */
    static const uint32_t knobs[] = {0, 1024, 2048, 3000, 4088};
    u_def = usb_run(1, 2048, &ok_def, &d_def);         /* (FIXED at the default knob: the DAC = USB) */
    same &= usb_same_as_ref;
    u_fixed = usb_run(1, 1024, &ok_fixed, &d_fixed); same &= usb_same_as_ref; if (usb_lim_min < lim_fixed_min) lim_fixed_min = usb_lim_min;
    u_fixed0 = usb_run(1, 0, &ok_fixed0, &d_fixed0); same &= usb_same_as_ref; if (usb_lim_min < lim_fixed_min) lim_fixed_min = usb_lim_min;
    u_hi = usb_run(1, 4088, &ok_hi, &d_hi); same &= usb_same_as_ref; if (usb_lim_min < lim_fixed_min) lim_fixed_min = usb_lim_min;
    for (k = 0; k < 5; k++) {
        usb_run(1, knobs[k], &ok_def, &d_loop); same &= usb_same_as_ref && ok_def;
        if (usb_lim_min < lim_fixed_min) lim_fixed_min = usb_lim_min;
    }
    printf("audio: USB / DAC rms: MASTER full %.0f / %.0f, 2048 %.0f / %.0f, 1/4 %.0f / %.0f, 0 %.0f / %.0f; FIXED 1/4 %.0f / %.0f, 0 %.0f / %.0f, 4088 %.0f / %.0f; limiter env min: MASTER 2048 %d, FIXED (any knob) %d\n",
           u_full, d_full, u_ref, d_ref, u_quiet, d_quiet, u_off, d_off, u_fixed, d_fixed, u_fixed0, d_fixed0, u_hi, d_hi,
           (int)lim_ref_min, (int)lim_fixed_min);
    check("USB LEVEL MASTER (default): USB is the DAC's signal and follows the knob (0: silent)",
          ok_full && ok_quiet && ok_off && ok_ref && u_full > 1000.0 && u_quiet < u_full * 0.5 && u_off == 0.0);
    check("USB LEVEL FIXED: USB is exactly MASTER mode's signal at master_q12 = 2048 (MASTER_USB_REF), whatever the knob (0, 1/4, 4088)",
          same && u_fixed == u_ref && u_fixed0 == u_ref && u_hi == u_ref && u_def == u_ref);
    check("USB LEVEL FIXED: the DAC gets exactly USB * master_q12 / 2048, saturated (0: silent, 2048: = USB, 1/4: -6 dB)",
          ok_fixed && ok_fixed0 && ok_hi && d_fixed0 == 0.0 && fabs(d_def - u_def) < 1e-9 && fabs(20.0 * log10(d_fixed / u_fixed) + 6.02) < 0.2);
    check("USB LEVEL FIXED: the limiter works no harder than MASTER mode at 2048, at any knob (the held chord)",
          lim_fixed_min >= lim_ref_min);
    {   /* the DAC feed saturates, never wraps: a hot signal at the knob's top (4088: x1.996) */
        int32_t b[8] = {32767, -32768, 30000, -30000, 20000, -20000, 16383, 0};
        int ok = 1;
        song.master_q12 = 4088;
        usb_fixed_dac(b, 4);
        ok &= b[0] == 32767 && b[1] == -32768 && b[2] == 32767 && b[3] == -32768 && b[4] == 32767 && b[5] == -32768
              && b[6] == 32702 && b[7] == 0;
        song.master_q12 = 2048;
        { int32_t c[4] = {32767, -32768, 1234, -1234}; usb_fixed_dac(c, 2); ok &= c[0] == 32767 && c[1] == -32768 && c[2] == 1234 && c[3] == -1234; }
        check("USB LEVEL FIXED: usb_fixed_dac saturates at int16 at master_q12 4088 (no wrap) and is unity at 2048", ok);
    }
    {   /* PERFORM busy (DRIVE held, K3 FILTER low-pass): perf_master takes the level, so FIXED must give it MASTER_USB_REF too */
        int okp, okr;
        double dp, dr, ur, up;
        int32_t lref, lmin = LIM_T, samep = 1;
        usb_perf = 1;
        ur = usb_run(0, 2048, &okr, &dr);
        lref = usb_lim_min;
        for (k = 0; k < 5; k++) {
            up = usb_run(1, knobs[k], &okp, &dp);
            samep &= usb_same_as_ref && okp && up == ur;
            if (usb_lim_min < lmin) lmin = usb_lim_min;
        }
        usb_perf = 0;
        memset(&pf, 0, sizeof pf);                     /* (PERFORM at rest again for the tests after) */
        pf.src = pf.next = PF_N;
        pf.lc = PF_TOP;
        pf.pbeat = perf_beat;
        pf.mg[0] = pf.mg[1] = pf.mg[2] = pf.mg[3] = 32768;
        printf("audio: USB LEVEL FIXED, PERFORM busy: USB rms %.0f (MASTER 2048 and FIXED at every knob); limiter env min: MASTER 2048 %d, FIXED %d\n",
               ur, (int)lref, (int)lmin);
        check("USB LEVEL FIXED with PERFORM busy (DRIVE, FILTER): USB is exactly MASTER mode's at 2048, the DAC USB * knob / 2048, the limiter no harder",
              okr && samep && ur > 1000.0 && lmin >= lref);
    }
}

/* #137: keys pressed under the splash. main.c: master_boot before audio_init (the pot's level from the first block),
 * kb_boot_hold until the splash ends (seq.c keyboard_block: the keys sound nothing; one held across stays silent until
 * pressed again); the splash polls the pot (master_poll). boot 0: as before the fix */
static void boot_fresh(void)
{
    fresh();
    lim_env = LIM_T; dc_l = dc_r = dce_l = dce_r = 0;
    memset(kb_note, 0, sizeof kb_note); kb_prev = 0; kb_layer = 0;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_DIST] = trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
}
static double boot_run(int32_t pot, int boot, uint32_t *splash_peak, uint32_t *first_master, uint32_t *busy)
{
    int32_t out[2 * 32];
    uint32_t b, i, peak = 0;
    double r = 0;
    boot_fresh();
    song.master_q12 = 2048;                            /* felucca_init */
    master_knob = 512 * 16;
    host_pot = pot;
    if (boot) {
        master_boot();
        kb_boot_hold = 1;
    }
    *first_master = song.master_q12;
    for (b = 0; b < 600u; b++) {                       /* the splash, ~0.43 s: keys hammered, one held at its end */
        fm1_in.notes = b >= 580u ? 1u << 12 : (b / 40u) & 1u ? (1u << 7) | (1u << 11) : 1u << 4;
        if (b % 14u == 0u && boot)
            master_poll();
        audio_block(out, 32);
        for (i = 0; i < 64u; i++)
            if ((uint32_t)abs(out[i]) > peak) peak = (uint32_t)abs(out[i]);
    }
    *splash_peak = peak;
    kb_boot_hold = 0;                                  /* the UI */
    *busy = 0;
    for (b = 0; b < 400u; b++) {                       /* the held key (silent), let go; then a chord pressed */
        fm1_in.notes = b < 40u ? 1u << 12 : b < 60u ? 0u : (1u << 0) | (1u << 4) | (1u << 7);
        audio_block(out, 32);
        if (b >= 60u) r += block_rms(out, 32);
        if (b == 39u) *busy = voices_busy();
    }
    fm1_in.notes = 0;
    return r / 340.0;
}
static void boot_keys(void)
{
    uint32_t peak, peak0, peak_old, m, m0, m_old, busy, busy0, busy_old, ref_m, b;
    double quiet, silent, old, ref = 0;
    int32_t out[2 * 32];
    quiet = boot_run(300, 1, &peak, &m, &busy);
    silent = boot_run(0, 1, &peak0, &m0, &busy0);
    old = boot_run(300, 0, &peak_old, &m_old, &busy_old);
    boot_fresh();                                      /* the reference: the chord, MASTER at the pot's level by hand */
    song.master_q12 = ref_m = master_of_pot(300);
    for (b = 0; b < 340u; b++) {
        fm1_in.notes = (1u << 0) | (1u << 4) | (1u << 7);
        audio_block(out, 32);
        ref += block_rms(out, 32);
    }
    fm1_in.notes = 0;
    ref /= 340.0;
    printf("audio: boot, pot 300 (MASTER %u): splash peak %u, then rms %.0f (by hand %.0f); before the fix: "
           "MASTER %u, splash peak %u, then rms %.0f\n", m, peak, quiet, ref, m_old, peak_old, old);
    check("#137 boot: MASTER is the pot's level before the first block (pot 300, pot 0)",
          m == ref_m && m0 == 0u && m_old == 2048u);
    check("#137 boot: keys hammered under the splash sound nothing (before the fix: they did)",
          peak == 0u && peak0 == 0u && peak_old > 0u);
    check("#137 boot: a key held across the splash's end stays silent until pressed again",
          busy == 0u && busy0 == 0u && busy_old > 0u);
    check("#137 boot: after the splash the keys play at the pot's level, as set by hand (pot 0: silent)",
          ref > 100.0 && fabs(quiet - ref) < 1e-9 && silent == 0.0);
}

/* DUELL 0.2.11, the flash duck (audio.c audio_duck / audio_unduck, storage_hw.c around st_erase): a simulated erase. The ISR
 * renders while the main loop waits; the output fades to 0 over one half (never above the linear ramp), both halves of abuf
 * are 0 before the erase (the DMA loops them meanwhile), the notes keep sounding, and the output comes back within a half */
static int32_t host_last[2u * HALF_WORDS];
static uint32_t host_isrs;
static void host_isr(void)
{
    host_pending = FM1_AUDIO_HALF;
    fm1_alnk0_irq();
    memcpy(host_last, &abuf[host_alt_half * HALF_WORDS], sizeof(int32_t) * HALF_WORDS);
    host_alt_half ^= 1u;
    host_isrs++;
}
static int32_t half_peak(const int32_t *o, uint32_t from, uint32_t to)
{
    int32_t m = 0;
    uint32_t i;
    for (i = 2u * from; i < 2u * to; i++) m = abs(o[i]) > m ? abs(o[i]) : m;
    return m;
}
static void flash_duck(void)
{
    uint32_t i, busy, isrs, ok_ramp = 1, ok_zero = 1;
    int32_t steady = 0, fade[2u * HALF_WORDS], back;
    fresh();
    host_alt = 1; host_us = 100;
    trk[0].p[P_VOICE] = V_POLY;
    trk_note_on(&trk[0], 48, 110); trk_note_on(&trk[0], 55, 110); trk_note_on(&trk[0], 64, 110);
    for (i = 0; i < 60u; i++) { host_isr(); if (i >= 40u) steady = half_peak(host_last, 0, HALF_FRAMES) > steady ? half_peak(host_last, 0, HALF_FRAMES) : steady; }
    busy = voices_busy();
    isrs = host_isrs;
    duck_on = 1;                                       /* (the fade half on its own first: its shape) */
    host_isr();
    memcpy(fade, host_last, sizeof fade);
    duck_on = 0; duck_g = 32768u; duck_zero = 0;
    for (i = 0; i < HALF_FRAMES; i++)                   /* |x| never above the steady peak x the ramp (1 - (i + 1) / 128), 1 % slack */
        if (abs(fade[2u * i]) > (int32_t)((int64_t)steady * (HALF_FRAMES - 1u - i) / HALF_FRAMES) + steady / 100 + 1 ||
            abs(fade[2u * i + 1u]) > (int32_t)((int64_t)steady * (HALF_FRAMES - 1u - i) / HALF_FRAMES) + steady / 100 + 1)
            ok_ramp = 0;
    for (i = 0; i < 4u; i++) host_isr();
    isrs = host_isrs;
    audio_duck();                                      /* the simulated erase: duck, then the IRQs "off" (nothing runs), then back */
    for (i = 0; i < 2u * HALF_WORDS; i++) ok_zero &= abuf[i] == 0;
    printf("audio: flash duck: steady peak %d, the duck waited %u halves (the fade + 2 silent), fade half: first 8 frames %d, last 8 %d\n",
           (int)steady, host_isrs - isrs, (int)half_peak(fade, 0, 8), (int)half_peak(fade, HALF_FRAMES - 8u, HALF_FRAMES));
    check("flash duck: the fade half stays under a linear ramp from the level to 0 over one half (no step)", ok_ramp && steady > 1000 && half_peak(fade, 0, 8) > steady / 4);
    check("flash duck: before the erase both halves of the buffer are 0 (the DMA loops silence), within 3 halves",
          ok_zero && duck_zero == 2u && host_isrs - isrs == 3u);
    audio_unduck();
    host_isr();
    back = half_peak(host_last, HALF_FRAMES / 2u, HALF_FRAMES);
    check("flash duck: after the erase the output fades back in over one half; the held notes kept sounding",
          half_peak(host_last, 0, 4) < steady / 8 && back > steady / 4 && voices_busy() == busy && busy == 3u);
    host_isr();
    check("flash duck: the half after is at the full level again (gain 1, no duck left)", duck_g == 32768u && half_peak(host_last, 0, HALF_FRAMES) > steady / 2);
    host_alt = 0;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    usb_level();
    boot_keys();
    overload(); dma();
    flash_duck();
    printf(bad ? "audio: %d FAILED\n" : "audio: all passed\n", bad);
    return bad != 0;
}
