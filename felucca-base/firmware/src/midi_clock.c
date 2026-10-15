/* SPDX-License-Identifier: GPL-3.0-only
 * Adapted from MIDI clock contributions by ChanceTheMaker and keremimo (2026).
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Audio ISR state. Input timestamps keep USB/TRS jitter outside the render loop. */
static struct {
    uint32_t pos, rendered, last_ms, start_ms, rem, interval_ms;
    uint32_t pulse_samples, interp_q8, tempo_ms;
    uint8_t mode, have_pulse, tempo_valid, tempo_n;
    uint8_t seen;                                  /* a clock pulse of the selected source has arrived (the UI's EXT / NO CLOCK) */
    uint8_t stopped;                               /* an external Stop (0xFC or this unit's STOP) until the next Start / Continue: the arp waits */
    uint8_t lock;                                  /* CLK AUTO: the port locked onto (1 USB, 2 TRS), 0 = none, the internal clock runs */
    uint8_t det_n[3];                              /* AUTO, unlocked: a port's plausible pulses in a row (det_t0: the first one's time) */
    uint32_t det_t0[3], det_last[3];
} midi_clock;

/* DUELL 0.2.14, CLK AUTO (SPEC 4.2, MIDI clock): a port locks after AUTO_LOCK_N pulses AUTO_MIN_MS..AUTO_MAX_MS apart (24 ppqn at ~31..312 BPM)
 * within AUTO_LOCK_MS, or at once on its Start / Continue; the other port is ignored while locked; no pulse for AUTO_DROP_MS: back to the
 * internal clock, at the last tempo and the current step phase, the transport as it was (seq.c events_block) */
#define AUTO_LOCK_N 3u
#define AUTO_LOCK_MS 250u
#define AUTO_MIN_MS 8u
#define AUTO_MAX_MS 80u
#define AUTO_DROP_MS 1000u

/* the ACTIVE clock source (core.h): 0 INT, 1 USB, 2 TRS */
static uint32_t clock_src(void)
{
    uint32_t m = (uint32_t)song.g[G_CLOCK];
    return m == CLK_AUTO ? midi_clock.lock : m < CLK_AUTO ? m : 0u;
}

/* CLK card / TEMPO page status: 0 INT, 1 external clock arriving, 2 external selected but no pulse for 1 s (NO CLOCK; never in AUTO: it falls back) */
static uint32_t clock_state(void)
{
    if (!clock_src())
        return 0;
    if (song.g[G_CLOCK] == CLK_AUTO)
        return 1;
    return midi_clock.seen && (int32_t)(fm1_ms - midi_clock.last_ms) < 1000 ? 1u : 2u;
}

/* the tempo changed from old_beat to new_beat samples a beat while playing: each track's fractional step phase kept (the next pulse, or the
 * internal clock's next step, lands on its new boundary) */
static __attribute__((noinline)) void clock_rescale(uint32_t old_beat, uint32_t new_beat)
{
    uint32_t i, ratio = (new_beat << 12) / old_beat;
    for (i = 0; i < NTRK; i++) {
        if (trk[i].seq_pos < (uint32_t)FS * 2u)
            trk[i].seq_pos = (uint32_t)(((uint64_t)trk[i].seq_pos * ratio + 2048u) >> 12);
        if (trk[i].seq_off)
            trk[i].seq_off = (uint32_t)(((uint64_t)trk[i].seq_off * ratio + 2048u) >> 12);
        if (trk[i].nd_late)            /* (NUDGE: the latched plays, seq_pos + 1, with the step) */
            trk[i].nd_late = (uint32_t)(((uint64_t)(trk[i].nd_late - 1u) * ratio + 2048u) >> 12) + 1u;
        if (trk[i].nd_early)
            trk[i].nd_early = (uint32_t)(((uint64_t)(trk[i].nd_early - 1u) * ratio + 2048u) >> 12) + 1u;
    }
    if (clk_pos < (uint32_t)FS * 2u)   /* (the metronome's beat as the steps) */
        clk_pos = (uint32_t)(((uint64_t)clk_pos * ratio + 2048u) >> 12);
}

static __attribute__((noinline)) void midi_clock_transport(uint32_t status, uint32_t ms)
{
    if (status == 0xFAu) {                         /* Start: step zero */
        midi_clock.pos = midi_clock.rendered = midi_clock.rem = 0;
        midi_clock.have_pulse = midi_clock.tempo_valid = 0;
        midi_clock.interval_ms = 0;
        midi_clock.start_ms = ms;
        midi_clock.stopped = 0;
        seq_start();
    } else if (status == 0xFBu) {                  /* Continue: preserve the step */
        midi_clock.pos = midi_clock.rendered;
        midi_clock.rem = 0;
        midi_clock.have_pulse = midi_clock.tempo_valid = 0;
        midi_clock.interval_ms = 0;
        midi_clock.start_ms = ms;
        midi_clock.stopped = 0;
        if (!song.playing)
            motion_begin();
        song.playing = 1;
    } else if (status == 0xFCu) {
        midi_clock.stopped = 1;                    /* (the arp waits for the next Start; the pulses are not counted) */
        seq_stop();
    }
}

static __attribute__((noinline)) void midi_clock_pulse(uint32_t ms)
{
    uint32_t q;
    if (!midi_clock.tempo_valid) {
        midi_clock.tempo_valid = 1;
        midi_clock.tempo_ms = ms;
        midi_clock.tempo_n = 0;
    } else if (++midi_clock.tempo_n == 6u) {
        uint32_t dt = ms - midi_clock.tempo_ms;
        midi_clock.tempo_ms = ms;
        midi_clock.tempo_n = 0;
        /* Six clocks are a quarter of a beat. Reject gaps and corrupt bursts. */
        if (dt >= 62u && dt <= 375u) {
            uint32_t old_beat = beat_samples(), new_beat = (uint32_t)FS * dt / 250u;
            if (song.playing && new_beat != old_beat)
                clock_rescale(old_beat, new_beat);   /* (the master changed tempo: each track's step phase kept) */
            midi_beat_samples = new_beat;
            song.g[G_BPM] = (int16_t)clamp((int32_t)((15000u + dt / 2u) / dt), 40, 240);
        }
    }
    midi_clock.seen = 1;
    if (song.playing || !midi_clock.stopped) {     /* (a clock running without a Start still steps the ARP: events_block) */
        if (midi_clock.have_pulse) {
            uint32_t interval = ms - midi_clock.last_ms;
            if (interval >= 8u && interval <= 80u)
                midi_clock.interval_ms = interval;
            q = beat_samples() + midi_clock.rem;
            midi_clock.pos += q / 24u;
            midi_clock.rem = q % 24u;
        }
        midi_clock.have_pulse = 1;
    }
    midi_clock.pulse_samples = beat_samples() / 24u;
    if (!midi_clock.interval_ms)
        midi_clock.interval_ms = beat_samples() * 1000u / ((uint32_t)FS * 24u);
    midi_clock.interp_q8 = midi_clock.pulse_samples * 256u / midi_clock.interval_ms;
    midi_clock.last_ms = ms;
}

static uint32_t midi_clock_advance(uint32_t now)
{
    uint32_t target, elapsed, offset, n, extra = 0;
    if (!midi_clock.have_pulse)
        return 0;
    elapsed = now - midi_clock.last_ms;
    /* Interpolate to the next pulse, never across it before it arrives. */
    if (elapsed > midi_clock.interval_ms) {
        /* AUTO: a late pulse does not stall the music; past it the steps run on in real time (the measured tempo is the step length:
         * 24 pulses of beat_samples() / 24 a beat run at the audio rate), so a clock that stops hands over to the internal one without
         * a gap. A pulse that comes after all puts it right (rendered ahead of pos waits; after a gap: clock_auto) */
        if (song.g[G_CLOCK] == CLK_AUTO && elapsed < AUTO_DROP_MS)
            extra = (elapsed - midi_clock.interval_ms) * (uint32_t)FS / 1000u;
        elapsed = midi_clock.interval_ms;
    }
    offset = elapsed * midi_clock.interp_q8 >> 8;
    if (offset >= midi_clock.pulse_samples)
        offset = midi_clock.pulse_samples - 1u;
    offset += extra;
    target = midi_clock.pos + offset;
    n = (int32_t)(target - midi_clock.rendered) > 0 ? target - midi_clock.rendered : 0u;
    if (n > (uint32_t)FS / 8u)
        n = (uint32_t)FS / 8u;
    midi_clock.rendered += n;
    return n;
}


/* AUTO: lock onto port src (its pulses from now on are the clock). The steps keep their phase (the counters start from here: the next
 * pulses continue the current step), the tempo is the internal one until six pulses have measured the master's; a count-in on the
 * internal clock is cancelled (an external clock never counts in) */
static __attribute__((noinline)) void clock_lock(uint32_t src, uint32_t ms)
{
    midi_clock.lock = (uint8_t)src;
    midi_clock.pos = midi_clock.rendered = midi_clock.rem = 0;
    midi_clock.have_pulse = midi_clock.tempo_valid = midi_clock.tempo_n = 0;
    midi_clock.interval_ms = 0;
    midi_clock.start_ms = midi_clock.last_ms = ms;
    midi_clock.stopped = midi_clock.seen = 0;
    memset(midi_clock.det_n, 0, sizeof midi_clock.det_n);
    midi_beat_samples = 0;
    if (cin_left) {
        cin_left = 0;
        cin_flush = 0;
        memset(cin_n, 0, sizeof cin_n);
    }
}

/* AUTO: the locked port went silent (AUTO_DROP_MS): the internal clock takes over at the last tempo (song.g[G_BPM], set by the pulses; the
 * steps rescaled from the measured to the whole-BPM beat) and the current phase; a playing transport plays on, a stopped one stays stopped */
static __attribute__((noinline)) void clock_unlock(void)
{
    uint32_t old_beat = beat_samples(), new_beat;
    midi_clock.lock = 0;
    midi_beat_samples = 0;
    new_beat = beat_samples();
    if (song.playing && new_beat != old_beat)
        clock_rescale(old_beat, new_beat);
    midi_clock.pos = midi_clock.rendered = midi_clock.rem = 0;
    midi_clock.have_pulse = midi_clock.tempo_valid = midi_clock.tempo_n = 0;
    midi_clock.interval_ms = 0;
    midi_clock.stopped = midi_clock.seen = 0;
    memset(midi_clock.det_n, 0, sizeof midi_clock.det_n);
}

/* AUTO: a real-time byte (0xF8 .. 0xFF) from port src (1 USB, 2 TRS) at ms */
static __attribute__((noinline)) void clock_auto(uint32_t src, uint32_t status, uint32_t ms)
{
    uint32_t d;
    if (src > 2u)
        return;
    if (midi_clock.lock) {
        if (src != midi_clock.lock)
            return;                                /* the other port: ignored while locked (no ping-pong between two masters) */
        if (status == 0xF8u) {
            /* a pulse after a gap (a master that paused without a Stop): our steps went on in real time (midi_clock_advance); they keep
             * their phase rather than stand still until the count catches up */
            uint32_t gap = midi_clock.have_pulse && (int32_t)(ms - midi_clock.last_ms) > (int32_t)AUTO_MAX_MS;
            midi_clock_pulse(ms);
            if (gap)
                midi_clock.pos = midi_clock.rendered;   /* (this pulse is where the steps are now: no jump, no wait) */
        } else
            midi_clock_transport(status, ms);
        return;
    }
    if (status == 0xFAu || status == 0xFBu) {      /* Start / Continue: this port at once (the first pulse after it is the downbeat) */
        clock_lock(src, ms);
        midi_clock_transport(status, ms);
        return;
    }
    if (status != 0xF8u)
        return;                                    /* (a Stop of a port nobody follows: nothing to stop) */
    d = ms - midi_clock.det_last[src];
    if (midi_clock.det_n[src] && d >= AUTO_MIN_MS && d <= AUTO_MAX_MS)
        midi_clock.det_n[src]++;
    else {
        midi_clock.det_n[src] = 1;
        midi_clock.det_t0[src] = ms;
    }
    midi_clock.det_last[src] = ms;
    if (midi_clock.det_n[src] >= AUTO_LOCK_N && ms - midi_clock.det_t0[src] <= AUTO_LOCK_MS) {
        clock_lock(src, ms);
        midi_clock_pulse(ms);                      /* (the third pulse: the clock's first) */
    }
}

/* the CLK choice changed (the knob, the editor, a restore): the transport stops and the clock starts clean only when the ACTIVE source
 * changes (INT -> USB, USB -> INT, ..., as ever); AUTO <-> the source it plays on keeps everything (INT -> AUTO with no clock arriving, a
 * fixed USB with pulses arriving -> AUTO locked on USB, AUTO on USB -> USB) */
static __attribute__((noinline)) void clock_choice(uint32_t mode)
{
    uint32_t was = midi_clock.mode == CLK_AUTO ? midi_clock.lock : midi_clock.mode < CLK_AUTO ? midi_clock.mode : 0u, now;
    if (mode == CLK_AUTO)
        now = was && midi_clock.seen && (int32_t)(fm1_ms - midi_clock.last_ms) < (int32_t)AUTO_DROP_MS ? was : 0u;
    else
        now = mode < CLK_AUTO ? mode : 0u;
    if (now != was) {
        seq_stop();
        memset(&midi_clock, 0, sizeof midi_clock);
        midi_beat_samples = 0;
    }
    midi_clock.mode = (uint8_t)mode;
    midi_clock.lock = (uint8_t)(mode == CLK_AUTO ? now : 0u);
    memset(midi_clock.det_n, 0, sizeof midi_clock.det_n);
}
