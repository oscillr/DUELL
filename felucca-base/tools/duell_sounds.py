# SPDX-License-Identifier: GPL-3.0-only
"""DUELL factory sounds: the data (tools/gen_duell_sounds.py validates it and writes build/gen/duell_sounds.h).

Every sound: engine, idx (its factory index within the engine: the identity favourites, saved copies (shadows) and the
stable End B seed use; APPEND-ONLY per engine, never renumber), name (<= 12 characters), cat (BASS LEAD PAD PLUCK KEYS
FX OTHER), mono (True: VOICE LEG, as the old presets' flag), recipe (the DUELL/SOUND_RECIPES.md section), and p: the
parameters that differ from INIT (the track defaults, params.c TP, and the engine's EDIT defaults). Names: the
engine's EDIT labels (ANALOG: WAVE DTN MIX NOIS CUT RES DRV KTR), core.h's P_ ids without P_ (ATK DEC SUS REL LEVEL
GLIDE GLMODE VOICE DIST CHOR DLY REV FTYPE E2ATK.. E2FLT L2RATE.. L2PIT SPRD ITYPE IA IB IC IMIX ...) or the aliases
of gen_duell_sounds.py (ENV2 FLT, LFO2 RATE, ARP MODE ...); MOD1..MOD8 = (SRC, DST, AMT) matrix slots.

House rules (owner, 2026-10-11): ENV 2 is the filter envelope (ENV2 FLT / SHP) on every sound; matrix slot 1 =
aftertouch (AT) to what suits the type; slot 2 = velocity -> CUT where the recipe asks for velocity-brightness
(velocity -> level is built in); no MODW routes (the mod wheel moves the MORPH fader); ARP only on arp sounds.
LEVEL: the trim from tests/sound_audit.c (SOUND_TRIM=1): one held C3 near -18 dBFS (ear-weighted) at its sustained level (the
median of seconds 1-6 of a 10 s hold; a sound that does not sustain: its loudest 0.5 s), its peak at most -3 dBFS; a hit (PERC,
a one-shot OTHER) by its peak, -4.5. The audit fails a quiet body, a slow rise or start, a fall-away and a quiet tap (its header).
FM6 sounds cannot be shaped by ATK..REL (the patch's operator envelopes are the amplitude): their body is the patch's carrier
sustain levels (tools/gen_fm6_patches.py).
End B: 'bamt' (the AMOUNT, by category unless the sound says otherwise: gen_duell_sounds.py CAT_BAMT) and 'bseed' (from
tests/sound_audit.c SOUND_BSEED=1: a seed whose End B keeps A's loudness and roughly its brightness, SPLIT and STEREO).

Units. The helpers below turn the recipes' physical numbers into DUELL's 0..127 curves (tools/gen_tables.py):
  ms(t)   ATK DEC REL / ATK2.. / FADE: 1 ms .. 10 s exponential, value = 127 * log10(ms) / 4
  hz(f)   LFO RATE: 0.05 .. 40 Hz exponential
  fc(f)   CUT: 30 Hz .. 16 kHz exponential (one CUT step = 0.071 octave)
  pct(x)  a 0..127 percentage (SUS, sends, MIX, RES ...): x % of 127
  dep(x)  a -64..63 depth (ENV 2 FLT, LFO 2 PIT/FLT/SHP, matrix AMT): x % of 64. ENV2 FLT 64 = +63 CUT steps
          (~4.5 octaves) at the envelope's peak
  gl(t)   GLIDE with GLMODE TIME: t ms for any interval (voice.c glide_set: 1 + G^2 / 8 ticks of 0.726 ms)
  st(n)   ANALOG WAVE SYNC: DTN (shown SYNC) in 3/16 semitone steps
Engine facts used below (eng_analog.c, voice.c): KTR 127 = ~42 % of full key tracking (the most it reaches; KTR 64 =
~21 %, centred on C4); the filter is a 12 dB/oct state-variable one (the recipes' 24 dB ladders are approximated);
RES 0 = damping 2, 127 = near self-oscillation; DRV 0..127 = 1x .. 3x into a soft clipper; PWM's width moves only by
the SHP inputs, +-12.5 % of the period at SHP depth 63; LFO 2 PIT 1 = +-3/16 semitone (+-19 cents, the smallest
non-zero vibrato); MIX = OSC 1 / OSC 2 balance (MIX 0: OSC 1 alone); in WAVE SUB, MIX is the sub (a square one octave
below) level. Velocity > 110 adds an accent (voice.c: the AMP ENV opens the filter by 24 steps).
[G] marks a value the recipe only bounds loosely or not at all (DUELL/SOUND_RECIPES.md's own tag).
"""
import math


def ms(t):
    return max(0, min(127, round(127 * math.log10(max(t, 1.0)) / 4)))


def hz(f):
    return max(0, min(127, round(127 * math.log(f / 0.05) / math.log(800))))


def fc(f):
    return max(0, min(127, round(127 * math.log(f / 30) / math.log(16000 / 30))))


def pct(x):
    return max(0, min(127, round(x * 127 / 100)))


def dep(x):
    return max(-64, min(63, round(x * 64 / 100)))


def gl(t):
    return max(0, min(127, round(math.sqrt(max(0.0, t / (32 / 44.1) - 1) * 8))))


def st(n):
    return max(0, min(127, round(n * 16 / 3)))


SOUNDS = [
    # ------------------------------------------------------------------------------------------------ ANALOG BASS
    dict(engine="ANALOG", idx=0, name="LADDER BASS", cat="BASS", bseed=26217, mono=True, recipe="1.1 Minimoog-style bass", p={
        # saw + a square an octave below (WAVE SUB): the recipe's saw/pulse at 16' and 32'; a few cents of DTN on the
        # sub = the Mini's slightly unstable tuning ("part of the fatness") [G: 6 ct]
        "WAVE": "SUB", "DTN": 6, "MIX": pct(50),
        # cutoff low, ~9 o'clock = 25..40 on 0..127 [S]; resonance low (Stewart used none: it steals the low end) [S]
        "CUT": 32, "RES": 12,
        # mild drive: the Mini's output fed back into the filter input, its grit [S, amount G]
        "DRV": 24,
        # FENV amount ~40 % [S]; attack a few ms..20 ms [S] -> 10 ms; decay ~10 o'clock 150-400 ms -> 250 ms; sustain
        # low-mid -> 30 % [G]; release with the amp's
        "ENV2 FLT": dep(40), "E2ATK": ms(10), "E2DEC": ms(250), "E2SUS": pct(30), "E2REL": ms(50),
        # AENV A 0-3 ms, D 100-300 ms, S 70-100 %, R 20-80 ms [G] -> 2 ms, 200 ms, 85 %, 50 ms
        "ATK": ms(2), "DEC": ms(200), "SUS": pct(85), "REL": ms(50),
        # FX minimal, no reverb on the low end [G]: all sends 0 (INIT)
        # mod wheel to cutoff (the recipe) -> aftertouch to cutoff; velocity to the filter envelope -> VEL -> CUT [G]
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(15)),
        "LEVEL": 101,
    }),
    dict(engine="ANALOG", idx=1, name="TIDAL SUB", cat="BASS", bseed=1207, mono=True, recipe="1.3 Sub bass", p={
        "DIST": 7,   # own DIST (2026-10-12): SMART now rolls a DIST of 0 upward, and the first step costs ~2 dB; a base of 7 keeps End B in the window
        # a pure sine, one oscillator (MIX 0) [G]
        "WAVE": "SIN", "DTN": 0, "MIX": 0,
        # low-pass 40..70, resonance 0 [G] -> 60; gentle saturation adds harmonics [G] -> DRV 16
        "CUT": 60, "RES": 0, "DRV": 16,
        # (the recipe has no filter envelope: a small, short one only gives the saturated harmonics an edge [G])
        "ENV2 FLT": dep(10), "E2ATK": 0, "E2DEC": ms(150), "E2SUS": 0, "E2REL": ms(80),
        # AENV A 2-10 ms (no click), S 100 %, R 30-120 ms [G] -> 5 ms, 127, 80 ms
        "ATK": ms(5), "SUS": 127, "REL": ms(80),
        # no chorus / reverb (phase cancellation low down) [G]: sends 0. A sine has nothing above its cutoff, so
        # aftertouch drives the saturation instead of the cutoff [G]
        "MOD1": ("AT", "DRV", dep(25)),
        "LEVEL": 92,
    }),
    dict(engine="ANALOG", idx=2, name="BEEHIVE", cat="BASS", bseed=18967, mono=True, recipe="1.5 Reese bass", p={
        # two saws detuned against each other: sources say +-25 / +-55 ct, the engine notes DTN 20-60 -> 30 ct [S range]
        "WAVE": "SAW", "DTN": 30, "MIX": pct(50),
        # LP 24 dB, cutoff ~4 kHz [S]; resonance low-mid [S] -> 30
        "CUT": fc(4000), "RES": 30,
        # FENV short decay, ~15 % amount, to add attack [S]; decay 150 ms [G]
        "ENV2 FLT": dep(15), "E2ATK": 0, "E2DEC": ms(150), "E2SUS": 0, "E2REL": ms(24),
        # AENV A 0, S 100 %, R ~24 ms (the Ableton walkthrough) [S]
        "ATK": 0, "SUS": 127, "REL": ms(24),
        # mono / legato with portamento [S]: ~80 ms [G]
        "GLMODE": "TIME", "GLIDE": gl(80),
        # distortion is a common addition [G]: a little DIST send
        "DIST": pct(15),
        # (the sine sub layer and the 100 Hz high-pass of the recipe need a second part: not in one ANALOG sound)
        "MOD1": ("AT", "RES", dep(20)),
        "LEVEL": 96,
    }),
    dict(engine="ANALOG", idx=3, name="RUBBER BAND", cat="BASS", bseed=49167, mono=True, recipe="1.6 Pluck bass", p={
        # saw with an optional sub [G]: WAVE SUB, sub at ~40 %
        "WAVE": "SUB", "DTN": 0, "MIX": pct(40),
        # cutoff low 20-50, resonance 20-50 [G] -> 35 / 35
        "CUT": 35, "RES": 35,
        # FENV amount 60-100 %, A 0, D 80-250 ms, S 0, R close to D [G]: the whole sound -> 75 %, 150 ms
        "ENV2 FLT": dep(75), "E2ATK": 0, "E2DEC": ms(150), "E2SUS": pct(20), "E2REL": ms(150),
        # AENV A 0, D 150-400 ms, S 0-20 %, R 50-150 ms [G] -> 300 ms, 10 %, 100 ms
        "ATK": 0, "DEC": ms(500), "SUS": pct(45), "REL": ms(100),
        # light drive, a short reverb [G]
        "DRV": 30, "REV": pct(12),
        # velocity to the FENV amount [G] -> VEL -> CUT
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(20)),
        "LEVEL": 106,
    }),
    dict(engine="ANALOG", idx=4, name="SQUELCH", cat="BASS", bseed=17703, mono=True, recipe="1.2 TB-303 acid bass", p={
        # one oscillator, saw [G] (MIX 0: OSC 1 alone)
        "WAVE": "SAW", "DTN": 0, "MIX": 0,
        # cutoff low-mid, swept by hand; resonance 60-100 [S] (engine notes: RES 90-110) -> 95
        "CUT": 40, "RES": 95,
        # ENV MOD (never zero on the original) ~50 % [G]; A = S = R ~ 0, DECAY ~60 % of its range, 200-800 ms [S] -> 300 ms
        "ENV2 FLT": dep(50), "E2ATK": 0, "E2DEC": ms(300), "E2SUS": 0, "E2REL": 0,
        # AENV A 0, D 0.2-1 s, S high, R very short [G] -> 500 ms, 87 %, 10 ms
        "ATK": 0, "DEC": ms(500), "SUS": pct(87), "REL": ms(10),
        # slide: legato, ~50-100 ms portamento [S tie, G time] -> 60 ms
        "GLMODE": "TIME", "GLIDE": gl(60),
        # overdrive [S] and a delay, feedback 30-40 % (the delay time and feedback are the global FX's) [G]
        "DRV": 30, "DIST": pct(20), "DLY": pct(25),
        # the cutoff is moved live [S] -> aftertouch; accent = velocity -> resonance (+ the built-in accent: > 110
        # opens the filter, velocity sets the level) [S]
        "MOD1": ("AT", "CUT", dep(30)), "MOD2": ("VEL", "RES", dep(12)),
        "LEVEL": 90,
    }),
    # ------------------------------------------------------------------------------------------------ ANALOG LEAD
    dict(engine="ANALOG", idx=5, name="TORCH SONG", cat="LEAD", bseed=23143, mono=True, recipe="2.5 warm solo lead (Minimoog-style triangle / square 'singing' lead, slow vibrato, hall)", p={
        # vs RAZORWIRE (hard-sync saw, bright, harsh, phaser): a TRIANGLE pair (soft, nearly sine-like, few harmonics), the second 4 ct off
        "WAVE": "TRI", "DTN": 7, "MIX": pct(25),
        # a dark, rounded filter: cutoff 70, almost no resonance; key tracking full so high notes stay mellow [G]
        "CUT": 95, "RES": 8, "KTR": 127,
        # a soft bloom on the note: ENV 2 gently opens the filter, A 60 ms, D 600 ms, S 70 %
        "ENV2 FLT": dep(25), "E2ATK": ms(60), "E2DEC": ms(600), "E2SUS": pct(70), "E2REL": ms(300),
        # a breathy, crooner's attack and a long tail: A 25 ms, S 100 %, R 350 ms
        "ATK": ms(25), "SUS": 127, "REL": ms(350),
        # slow legato glide, 120 ms
        "GLMODE": "TIME", "GLIDE": gl(120),
        # the voice-like vibrato that makes the torch song: triangle ~5 Hz on pitch, blooming in after 500 ms (always on, not an aftertouch extra)
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5), "LFO2 FADE": ms(500), "LFO2 PIT": 1,
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(15)),
        # no drive, no delay: a warm chorus and a big hall reverb instead
        "CHO": pct(30), "REV": pct(40), "DIST": 7,
        "LEVEL": 93,
    }),
    dict(engine="ANALOG", idx=6, name="RAZORWIRE", cat="LEAD", bseed=13750, mono=True, recipe="2.2 Hard-sync lead", p={
        # OSC 2 hard-synced to OSC 1, its pitch = the timbre [S]; follower +7..+24 st [G] -> +7 at rest; mostly the
        # synced oscillator heard [G]
        "WAVE": "SYNC", "DTN": st(7), "MIX": pct(80),
        # the sweep by an envelope, D 300-800 ms [G] -> ENV 2 SHP +75 % (+~9 st at the peak, so +16 st falling to +7)
        "ENV2 SHP": dep(75), "E2ATK": 0, "E2DEC": ms(500), "E2SUS": 0, "E2REL": ms(150),
        # filter fairly open 80-110, resonance low [G] -> 95, 15; a little of ENV 2 on it too [G]
        "CUT": 95, "RES": 15, "ENV2 FLT": dep(10),
        # slow pitch modulation of the sweep, LFO 0.2-1 Hz [S KVR / G rate] -> LFO 2 SHP, triangle 0.5 Hz
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.5), "LFO2 SHP": dep(25),
        # AENV A 0, S 100 %, R 100-200 ms [G] -> 150 ms; portamento (Ultravox) [S] -> 80 ms [G]
        "ATK": 0, "SUS": 127, "REL": ms(150), "GLMODE": "TIME", "GLIDE": gl(80),
        # drive, delay, phaser (flanging in the Ultravox note) [S/G]: DRV, DLY, the INSERT PHASR at half mix
        "DRV": 24, "DLY": pct(20), "INSERT": "PHASR", "INS A": hz(0.5), "INS MIX": pct(40),
        # "sweep the slave by hand" [S] -> aftertouch to SHP (the sync interval)
        "MOD1": ("AT", "SHP", dep(40)),
        "LEVEL": 93,
    }),
    dict(engine="ANALOG", idx=7, name="FANFARE", cat="LEAD", bseed=30593, recipe="2.3 Brassy polysynth lead", p={
        # sawtooths on both oscillators, one detuned 5-12 ct [S] -> 8 ct
        "WAVE": "SAW", "DTN": 8, "MIX": pct(50),
        # the filter envelope makes the brass [S]: cutoff low enough for it to work [G] -> 45; resonance 2/10 [S]
        "CUT": 45, "RES": pct(20),
        # FENV A 30-120 ms, D 200-500 ms, S 40-60 % [S starting values] -> 60 ms, 350 ms, 50 %; amount "high enough to
        # matter" [S] -> 60 %
        "ENV2 FLT": dep(60), "E2ATK": ms(60), "E2DEC": ms(350), "E2SUS": pct(50), "E2REL": ms(100),
        # AENV A 15-40 ms, S high, R 50-150 ms [G] -> 25 ms, 90 %, 100 ms
        "ATK": ms(25), "DEC": ms(300), "SUS": pct(90), "REL": ms(100),
        # delayed vibrato ~5 Hz [S] -> LFO 2 TRI 5 Hz, PIT 1, FADE 400 ms [G delay]
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5), "LFO2 FADE": ms(400), "LFO2 PIT": 1,
        # aftertouch varies brightness in the sustain [S]; velocity sets the FENV amount [S] -> VEL -> CUT
        "MOD1": ("AT", "CUT", dep(30)), "MOD2": ("VEL", "CUT", dep(25)),
        # chorus, short reverb [G]
        "CHO": pct(25), "REV": pct(20),
        "LEVEL": 94,
    }),
    dict(engine="ANALOG", idx=8, name="WALL OF SAW", cat="LEAD", bseed=59188, recipe="2.4 Supersaw lead", p={
        # seven detuned saws on the JP-8000 [S]; two here (engine notes): saws, DTN low-mid -> 25 ct, equal mix [G]
        "WAVE": "SAW", "DTN": 25, "MIX": pct(50),
        # low-pass 70-110, resonance 0-30 [S/hedged] -> 95, 10
        "CUT": 95, "RES": 10,
        # FENV small 10-30 %, D 300 ms [G] -> 20 %
        "ENV2 FLT": dep(20), "E2ATK": 0, "E2DEC": ms(300), "E2SUS": 0, "E2REL": ms(250),
        # AENV A 0-10 ms, S 100 %, R 100-400 ms [G] -> 5 ms, 250 ms
        "ATK": ms(5), "SUS": 127, "REL": ms(250),
        # the FX are where it lives: chorus/ensemble, delay, big reverb [G]; the voices spread in STEREO [G]
        "CHO": pct(50), "DLY": pct(25), "REV": pct(35), "SPRD": pct(40),
        # leads and chord stabs [S]: POLY; aftertouch to brightness (lead)
        "MOD1": ("AT", "CUT", dep(25)),
        # SOFT insert as a limiter (2026-10-12): flattens the crest so LEVEL can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 80, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "LEVEL": 91,
    }),
    # ------------------------------------------------------------------------------------------------- ANALOG PAD
    dict(engine="ANALOG", idx=9, name="SILK CHORUS", cat="PAD", bseed=33289, recipe="3.1 Juno-style chorus pad", p={
        # one DCO per voice (MIX 0), pulse with PWM from a triangle LFO [S]; PWM depth moderate, 0.5-2 Hz [G] -> 1 Hz
        "WAVE": "PWM", "DTN": 0, "MIX": 0,
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(1), "LFO2 SHP": dep(60),
        # low-pass ~40-80, resonance low, small envelope amount [G] -> 60, 10, 15 %
        "CUT": 60, "RES": 10,
        # the Juno's one envelope: A 0, D top, S half, R long enough to overlap [S] -> R 1.5 s [G]
        "ENV2 FLT": dep(15), "E2ATK": 0, "E2DEC": 127, "E2SUS": pct(70), "E2REL": ms(1500),
        "ATK": 0, "DEC": 127, "SUS": pct(80), "REL": ms(1500),
        # the chorus makes it wide (Juno chorus II: lush) [S function, G amount]; a little reverb [G]
        "CHO": pct(80), "REV": pct(20),
        # pad: aftertouch brightens
        "MOD1": ("AT", "CUT", dep(25)),
        "LEVEL": 88,
    }),
    dict(engine="ANALOG", idx=10, name="EMBER GLOW", cat="PAD", bseed=48246, recipe="3.2 Prophet-style warm pad", p={
        # saws on A and B, 5-12 ct apart [S architecture, G cents] -> 9 ct
        "WAVE": "SAW", "DTN": 9, "MIX": pct(50),
        # 4-pole LP, cutoff 40-70, resonance 10-25, key tracking half [G] -> 55, 18, KTR 127 (the engine's ~42 %)
        "CUT": 55, "RES": 18, "KTR": 127,
        # FENV A 300-1500 ms, D 1-3 s, S 40-70 %, R 800-2000 ms [G] -> 800 ms, 2 s, 55 %, 1.5 s; amount unstated -> 30 % [G]
        "ENV2 FLT": dep(30), "E2ATK": ms(800), "E2DEC": ms(2000), "E2SUS": pct(55), "E2REL": ms(1500),
        # AENV A 200-800 ms, S 100 %, R 1-3 s [G] -> 500 ms, 2 s
        "ATK": ms(500), "SUS": 127, "REL": ms(2000),
        # gentle chorus, plate/hall reverb 25-40 % [G] -> REV 35 %
        "CHO": pct(25), "REV": pct(35),
        "MOD1": ("AT", "CUT", dep(25)),
        # SOFT insert as a limiter (2026-10-12): flattens the crest so LEVEL can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 80, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "LEVEL": 91,
    }),
    dict(engine="ANALOG", idx=11, name="VELVET BOW", cat="PAD", bseed=34517, recipe="3.3 PWM string pad", p={
        # two oscillators (SOS T2 / JX-10), pulse-width modulated (engine notes: PWM + LFO 2 SHP); OSC 2 detuned
        # (T2 ~10, JX -4: a few cents) -> 8 ct [S units of those synths, G cents]
        "WAVE": "PWM", "DTN": 8, "MIX": pct(50),
        # PWM LFO: rate 60-78 / intensity 02 in the synths' units (small) [S] -> 1.5 Hz, depth 40 % [G]
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(1.5), "LFO2 SHP": dep(40),
        # filter: T2 75 / JX 47 (resonance 0, key follow 64, env amount 14) [S] -> CUT 70, RES 0, KTR 64, ENV2 11 %
        "CUT": 70, "RES": 0, "KTR": 64,
        "ENV2 FLT": dep(11), "E2ATK": ms(100), "E2DEC": ms(100), "E2SUS": 125, "E2REL": ms(600),
        # AENV A 100-400 ms, S 100 %, R 400-900 ms [G] -> 250 ms, 600 ms
        "ATK": ms(250), "SUS": 127, "REL": ms(600),
        # chorus / ensemble essential, reverb added [S]
        "CHO": pct(85), "REV": pct(30),
        "MOD1": ("AT", "CUT", dep(20)),
        "LEVEL": 100,
    }),
    # ----------------------------------------------------------------------------------------- ANALOG PLUCK / KEYS
    dict(engine="ANALOG", idx=12, name="PIZZICATO", cat="PLUCK", bseed=65304, recipe="4.6 String pluck (subtractive)", p={
        # saw [G]; one oscillator
        "WAVE": "SAW", "DTN": 0, "MIX": 0,
        # cutoff 40-70 with a fast envelope: amount 50-80 %, D 100-400 ms, S 0 [G] -> 50, 65 %, 250 ms; RES unstated [G]
        "CUT": 50, "RES": 20,
        "ENV2 FLT": dep(65), "E2ATK": 0, "E2DEC": ms(250), "E2SUS": 0, "E2REL": ms(200),
        # AENV D 0.5-2 s, S 0, R 100-300 ms [G] -> 1 s, 200 ms
        "ATK": 0, "DEC": ms(1000), "SUS": 0, "REL": ms(200),
        # chorus + room reverb [S list]
        "CHO": pct(30), "REV": pct(25),
        # velocity to brightness (pluck) [house rule]; aftertouch: brightness
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 99,
    }),
    dict(engine="ANALOG", idx=13, name="SFORZANDO", cat="KEYS", bseed=46167, recipe="4.8 Brass stab", p={
        # saw stack, two oscillators, 5-10 ct [G] -> 7 ct
        "WAVE": "SAW", "DTN": 7, "MIX": pct(50),
        # cutoff 30-60 with a strong envelope: amount 60-90 %, A 0-20 ms, D 150-350 ms, S 30-60 % [G] -> 45, 75 %,
        # 10 ms, 250 ms, 45 %; resonance unstated [G]
        "CUT": 45, "RES": 15,
        "ENV2 FLT": dep(75), "E2ATK": ms(10), "E2DEC": ms(250), "E2SUS": pct(45), "E2REL": ms(120),
        # AENV A 0-10 ms, D 200-400 ms, S 40-70 %, R 80-200 ms [G] -> 5 ms, 300 ms, 55 %, 120 ms
        "ATK": ms(5), "DEC": ms(300), "SUS": pct(55), "REL": ms(120),
        # velocity to the filter amount [G]; chorus + short reverb [G]
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(25)),
        "CHO": pct(25), "REV": pct(20),
        "LEVEL": 101,
    }),
    # -------------------------------------------------------------------------------------------- ANALOG FX / OTHER
    dict(engine="ANALOG", idx=14, name="STORM FRONT", cat="FX", bamt=14, bseed=36047, recipe="5.1 Noise sweep / riser", p={
        # white noise (NOIS full); the oscillators cannot be switched off, so a quiet-ish sine sits under it [G]
        "WAVE": "SIN", "MIX": 0, "NOIS": 127, "KTR": 0,
        # low-pass with a small resonance bump, swept up by the envelope over the riser's length [S]: FENV A = the
        # riser length 2-16 s -> ~3 s, D 0, S 100 % [G]; the sweep as wide as ENV 2 goes (+63 steps), from CUT 47
        # (~300 Hz: a rumble from the first moment; from 30, ~130 Hz, a note shorter than the sweep was nearly silent)
        "CUT": 56, "RES": 30,
        "ENV2 FLT": dep(75), "E2ATK": 111, "E2DEC": 0, "E2SUS": 127, "E2REL": ms(300),
        # AENV A long 1-8 s -> ~1.2 s (the short end: the filter's sweep is the rise; 3 s left a held note silent for its
        # first second), S 100 %, R 100-500 ms -> 300 ms [G]
        "ATK": 97, "SUS": 127, "REL": ms(300),
        # reverb dry -> very wet [S]: a REV send that ENV 2 opens (matrix slot 3), and some delay [S list]
        "REV": pct(20), "DLY": pct(16), "MOD3": ("ENV2", "REV", dep(50)),
        "MOD1": ("AT", "CUT", dep(25)),
        "DIST": 7,
        # SOFT insert as a limiter (2026-10-12): flattens the crest so LEVEL can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 40, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "LEVEL": 93
    }),
    dict(engine="ANALOG", idx=15, name="GLASS SHARDS", cat="OTHER", bseed=17703, recipe="5.2 Metallic glitch arp", p={
        # short square tones [G]
        "WAVE": "SQR", "DTN": 0, "MIX": 0,
        # high-resonance band-pass [G] -> BP, RES 90, CUT 70
        "FILTER": "BP", "CUT": 70, "RES": 90,
        "ENV2 FLT": dep(30), "E2ATK": 0, "E2DEC": ms(60), "E2SUS": 0, "E2REL": ms(60),
        # D 30-100 ms, S 0 [G] -> 100 ms (the long end: a longer ping is more level for the same peak; this sound's
        # level is set by its peak, the resonant band-pass rings ~21 dB over its RMS: -23 dBFS LOUD at LEVEL 115)
        "ATK": 0, "DEC": ms(220), "SUS": 0, "REL": ms(100),
        # arp 16ths up-down over 2-3 octaves, random gate [G] -> UPDN 1/16, 3 octaves, PROB ~80 %, short gate
        "ARP MODE": "UPDN", "ARP RATE": "1/16", "ARP OCT": 3, "ARP GATE": pct(65), "ARP PROB": pct(80),
        # light bitcrush (INSERT CRUSH, 8 bits, 11 kHz, half mix) and delay feedback 40-60 % (the global delay's) [G]
        "INSERT": "CRUSH", "INS A": 60, "INS B": 100, "INS MIX": pct(50), "DLY": pct(40),
        # aftertouch moves the band-pass
        "MOD1": ("AT", "CUT", dep(30)),
        "DRV": 127,
        "DIST": pct(25),
        "LEVEL": 108,
    }),
    # ====================================================================================================== FM6 (batch A)
    # FM6 facts (eng_fm6.c, voice.c): the patch (SLOT F1..F8) holds the operator envelopes, which ARE the amplitude and end the
    # voice; so ATK DEC SUS REL are not set (they do nothing). The EDIT macros are the sound: ALG FB MLVL (modulator level =
    # the FM "cutoff") MRAT MEG (modulator envelope times, + slower) VMOD (modulator velocity sensitivity) DTUN SLOT. ENV 2 FLT
    # and the matrix's CUT reach MLVL (ENV 2 runs for every engine), LFO 2 FLT too; SHP = feedback. Velocity -> brightness is
    # VMOD (inside the patch) plus a matrix VEL -> CUT where the recipe wants more.
    dict(engine="FM6", idx=0, name="SLAP DOCTOR", cat="BASS", bseed=45536, mono=True, recipe="1.4 DX7 slap / FM bass", p={
        # the ROUND BASS patch (F3): two parallel carriers with modulators; the recipe's 5:1 / 1:1 pair [S]
        "SLOT": "F3", "FB": 1,
        # modulators low at rest, the slap = ENV 2 opening MLVL at the attack and falling within 100-300 ms [G] -> 220 ms
        "MLVL": -16, "ENV2 FLT": dep(60), "E2ATK": 0, "E2DEC": ms(500), "E2SUS": pct(30), "E2REL": ms(100),
        # velocity to modulator level (hard hits brighter) [G]
        "VMOD": 3,
        # none / very light chorus; mono [G]; bass: HF-shelf cut, subtle PUMP ducker (house rule)
        "EQHG": -4, "EQHF": -8, "DKSRC": "PUMP", "DKRATE": "1/4", "DKDEP": 16,
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(15)),
        "MEG": 24,
        "DIST": 7,
        "LEVEL": 117,
    }),
    dict(engine="FM6", idx=1, name="HORN SOLO", cat="LEAD", bseed=24199, mono=True, recipe="2.6 FM lead", p={
        # the BRASS SECT patch (F4) on ALG 16: one carrier (OP 1) fed by three modulator branches, the classic FM brass layout.
        # (Its own algorithm, 5, has three carriers detuned 2 steps apart: they beat once in ~5 s, the level dropped 15 dB and
        # back, the "weird" solo.) Modulator index follows the envelope: the patch's own attack swells the note in, ENV 2 on MLVL
        # opens with it (A 117 ms) and settles at 87 %, darker at low velocity (VMOD) [G; Plugin Boutique "80s Brass" idea]
        "SLOT": "F4", "ALG": 16, "FB": -1, "MEG": 20,
        "MLVL": -1, "ENV2 FLT": dep(31), "E2ATK": ms(117), "E2DEC": ms(717), "E2SUS": pct(87), "E2REL": ms(150),
        "VMOD": 3,
        # light vibrato, delayed (5 Hz, +-19 ct, 400 ms), pressure deepens it; portamento 60 ms [G]
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5), "LFO2 FADE": ms(400), "LFO2 PIT": 1,
        "MOD1": ("AT", "DEP2", dep(100)), "MOD2": ("VEL", "CUT", dep(20)),
        "GLMODE": "TIME", "GLIDE": gl(60),
        # chorus + delay [G]
        "CHO": pct(28), "DLY": pct(22), "REV": pct(15),
        "DIST": pct(7),
        "LEVEL": 117,
    }),
    dict(engine="FM6", idx=2, name="GLASS HALL", cat="PAD", bseed=53248, recipe="3.6 Glass / FM pad", p={
        # the SOFT PAD patch (F5): carrier ratio 1 with long slow attacks, a modulator at a higher ratio, low index [S Yamaha FM
        # 101 pt 5 DX7 string pad; the glassy high ratio is G]: MRAT +2, MEG slower, carriers spread a little (+3 detune in the source)
        "SLOT": "F5", "MRAT": 2, "MEG": 14, "MLVL": -8, "DTUN": pct(30), "FB": 1,
        # the glass opens slowly: ENV 2 on MLVL, A 1.5 s, S 55 % [G]; a slow LFO breathes it
        "ENV2 FLT": dep(20), "E2ATK": ms(1500), "E2DEC": ms(2500), "E2SUS": pct(55), "E2REL": ms(1500),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.3), "LFO2 FLT": dep(12),
        # shimmer reverb, chorus [G]
        "CHO": pct(55), "REV": pct(55),
        # chord keys instead of an arp: a major-seventh stack opened up (1-5-7-3)
        "CHRD": "MAJ7", "VOIC": "OPEN",
        "MOD1": ("AT", "CUT", dep(25)),
        # SOFT insert as a limiter (2026-10-12): flattens the crest so LEVEL can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 100, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "LEVEL": 94,
    }),
    dict(engine="FM6", idx=3, name="MALLET WOOD", cat="PLUCK", bseed=37591, recipe="4.4 Marimba / kalimba", p={
        # the WOOD BARS patch (F6): a strong fundamental plus inharmonic modes that die faster (modulator ratio 4 [G])
        "SLOT": "F6", "VMOD": 2, "MLVL": -4,
        # a fast-closing envelope so the upper partials decay faster than the fundamental [G]
        "ENV2 FLT": dep(30), "E2ATK": 0, "E2DEC": ms(250), "E2SUS": 0, "E2REL": ms(100),
        # short room reverb, light stereo delay; velocity to brightness [G]
        "REV": pct(25), "DLY": pct(14),
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 121,
    }),
    dict(engine="FM6", idx=4, name="TINE ROOM", cat="KEYS", bseed=64195, recipe="4.1 DX7 electric piano", p={
        # the TINE EP patch (F1): 14:1 tine modulator decaying fast, body pairs, carriers detuned for warmth [S]
        "SLOT": "F1", "DTUN": pct(10),
        # velocity = modulator level is the whole expressiveness (soft = tone bar, hard = barking tine) [S]: VMOD +3, and
        # the tine brighter at the strike through ENV 2 [G]
        "VMOD": 3, "ENV2 FLT": dep(20), "E2ATK": 0, "E2DEC": ms(500), "E2SUS": 0, "E2REL": ms(200),
        # stereo chorus, tremolo / auto-pan 0.5-5 Hz [G] -> LFO 2 on AMP, light reverb
        "LFO2 WAVE": "SIN", "LFO2 RATE": hz(3.5), "MOD3": ("LFO2", "AMP", dep(14)),
        "CHO": pct(40), "REV": pct(22),
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(20)),
        "MEG": 30,
        "LEVEL": 111,
    }),
    dict(engine="FM6", idx=5, name="BIG STAB", cat="KEYS", bseed=37822, recipe="4.8 Brass stab", p={
        # the 80s FM version: the modulator's level peaks at the attack and falls quickly, darker at low velocity [S Plugin
        # Boutique]: BRASS SECT (F4), faster modulator times, ENV 2 on MLVL A 5 ms, D 280 ms, S 20 %
        "SLOT": "F4", "MEG": 0, "FB": 1, "MLVL": -8, "VMOD": 4,
        "ENV2 FLT": dep(55), "E2ATK": ms(5), "E2DEC": ms(500), "E2SUS": pct(60), "E2REL": ms(120),
        # chorus + short reverb [G]
        "CHO": pct(25), "REV": pct(20),
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(20)),
        "DIST": pct(20),
        "LEVEL": 115,
    }),
    dict(engine="FM6", idx=6, name="FROST BELLS", cat="KEYS", bseed=16792, recipe="4.3 FM bell / glass", p={
        # the GLASS BELL patch (F2): sine carriers with modulators at non-integer ratios, long decay, the modulators decaying
        # faster so it starts bright and ends a pure tone [S]
        "SLOT": "F2", "DTUN": pct(10), "VMOD": 2,
        "ENV2 FLT": dep(35), "E2ATK": 0, "E2DEC": ms(1200), "E2SUS": 0, "E2REL": ms(600),
        # long hall reverb, little chorus [S]; bells: LF-shelf cut (house rule)
        "REV": pct(55), "CHO": pct(10), "EQLG": -5,
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        # SOFT insert as a limiter (2026-10-12): flattens the crest so LEVEL can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 40, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "LEVEL": 106,
    }),
    dict(engine="FM6", idx=7, name="ALIEN CHIMES", cat="FX", bseed=3052, recipe="5.4 Sci-fi FM bell cluster", p={
        # several bell voices with different non-integer ratios: the GLASS BELL patch on another algorithm (ALG 4), modulator
        # ratios up (MRAT +5), carriers spread wide [G]
        "SLOT": "F2", "ALG": 4, "MRAT": 5, "DTUN": pct(55), "FB": 2,
        "ENV2 FLT": dep(25), "E2ATK": 0, "E2DEC": ms(900), "E2SUS": 0, "E2REL": ms(500),
        # random pitch per note: arp RND 8ths over 2 octaves, short gate [G]; a slow LFO moves the brightness
        "ARP MODE": "RND", "ARP RATE": "1/8", "ARP OCT": 2, "ARP GATE": pct(35),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.2), "LFO2 FLT": dep(25),
        # long reverb / ping-pong delay, feedback 40-60 % (global) [G]; LF-shelf cut
        "DLY": pct(50), "REV": pct(60), "EQLG": -6,
        "MOD1": ("AT", "CUT", dep(25)),
        "LEVEL": 107,
    }),
    dict(engine="FM6", idx=8, name="TEMPLE BLOCK", cat="OTHER", bseed=37921, recipe="4.4 Marimba / kalimba (percussive)", p={
        # the WOOD BARS patch with the modulators' times as fast as they go and their ratio up: a short woody tok [G]
        "SLOT": "F6", "MEG": -40, "MRAT": 4, "MLVL": 2, "VMOD": 3,
        "ENV2 FLT": dep(20), "E2ATK": 0, "E2DEC": ms(90), "E2SUS": 0, "E2REL": ms(60),
        "REV": pct(20), "DLY": pct(12),
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 120,
    }),
    # ====================================================================================================== PHYS (batch A)
    # PHYS facts (eng_phys.c): the model's own decay is the note (SUS 127, REL damps the key-off); FLT (ENV 2 FLT, matrix CUT)
    # moves BRIT, SHP moves STRC; ACC = strike accent at full velocity (velocity scales it); EXC = the exciter itself.
    # WOOD BASS (PHYS, was idx 0) deleted 2026-10-12 (owner); idx must stay contiguous, so PAN WHISTLE .. TABLA TALK moved down by one
    dict(engine="PHYS", idx=0, name="PAN WHISTLE", cat="LEAD", bseed=17213, mono=True, recipe="blown tube (4.4 MODAL, BOW)", p={
        # a blown pipe: the modal bank with harmonic partials (STRC 34), sustained by random hits (BOW) with a breath of the
        # exciter (EXC) [S eng_phys.c]
        "MODEL": "MODAL", "STRC": 34, "BRIT": 62, "DAMP": 100, "POS": 14, "ACC": 110, "BOW": 110, "EXC": 40,
        # the player's breath: A 60 ms, R 160 ms [G]; delayed vibrato 5 Hz [S SOS brass: low, delayed]
        "ATK": ms(60), "SUS": 127, "REL": ms(160),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5), "LFO2 FADE": ms(500), "LFO2 PIT": 1,
        "ENV2 FLT": dep(18), "E2ATK": ms(40), "E2DEC": ms(350), "E2SUS": pct(70), "E2REL": ms(150),
        # pressure = more breath into the pipe (BOW), velocity = brightness
        "MOD1": ("AT", "BOW", dep(15)), "MOD2": ("VEL", "CUT", dep(20)),
        "GLMODE": "TIME", "GLIDE": gl(40),
        "CHO": pct(15), "DLY": pct(20), "REV": pct(30),
        # SOFT insert as a limiter (2026-10-12): flattens the crest so LEVEL can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 80, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "LEVEL": 99,
    }),
    dict(engine="PHYS", idx=1, name="SINGING BOWL", cat="PAD", bseed=31716, recipe="3.7 Dark drone / bowed metal", p={
        # a bowed bowl: MODAL, inharmonic (STRC 98: 1 : 3.57 : 7.65), long ring, sustained by the bow [S eng_phys.c]
        "MODEL": "MODAL", "STRC": 92, "BRIT": 66, "DAMP": 92, "POS": 10, "ACC": 80, "BOW": 100, "EXC": 0,
        "ATK": ms(150), "SUS": 127, "REL": ms(1800),
        # the bowing swells brightness and a slow LFO drifts it [G]
        "ENV2 FLT": dep(20), "E2ATK": ms(300), "E2DEC": ms(2500), "E2SUS": pct(70), "E2REL": ms(1500),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.15), "LFO2 FLT": dep(18),
        "CHO": pct(30), "REV": pct(60), "DLY": pct(15),
        # chord keys instead of an arp: bowls ring in open fifths (power chord) (3 voices: PHYS' limit)
        "CHRD": "POW", "VOIC": "OPEN",
        "MOD1": ("AT", "BOW", dep(15)),
        "LEVEL": 127,
    }),
    dict(engine="PHYS", idx=2, name="GHOST HARP", cat="PLUCK", bseed=64964, recipe="4.6 String pluck / harp (SYMP)", p={
        # SYMP: a plucked string with three sympathetic strings (resonant strings of a harp via the model, not the reverb)
        # [S eng_phys.c]: CHRD 8, SYMP 70, a touch of the curved bridge
        "MODEL": "SYMP", "E2": 8, "BRIT": 67, "DAMP": 104, "E5": 70, "ACC": 90, "E7": 30,
        "ENV2 FLT": dep(25), "E2ATK": 0, "E2DEC": ms(300), "E2SUS": 0, "E2REL": ms(200),
        "ATK": 0, "SUS": 127, "REL": ms(250),
        "CHO": pct(20), "REV": pct(40),
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 112,   # 0.2 dB over the trim (BRIT 67 with it): the universal filter (0.2.2) took the CUT routes off PHYS' BRIT; the body read -21.8 and the golden render's DC -430 at 110 (limit 400)
    }),
    dict(engine="PHYS", idx=3, name="NYLON GUT", cat="PLUCK", bseed=33009, recipe="4.6 String pluck (Karplus-Strong)", p={
        # one gut string: STRNG, STRC below the dispersion (a soft, slightly buzzy bridge), pluck position 1/3 [S: a pluck at
        # 1/n of the length cancels the n-th harmonics]
        "MODEL": "STRNG", "STRC": 56, "BRIT": 58, "DAMP": 110, "POS": 40, "ACC": 120, "EXC": 0,
        "ENV2 FLT": dep(25), "E2ATK": 0, "E2DEC": ms(300), "E2SUS": 0, "E2REL": ms(150),
        "ATK": 0, "SUS": 127, "REL": ms(80),
        # arpeggiated picking: UP 16ths over two octaves
        "ARP MODE": "UP", "ARP RATE": "1/16", "ARP OCT": 2, "ARP GATE": pct(85),
        "CHO": pct(18), "REV": pct(25),
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 116,
    }),
    dict(engine="PHYS", idx=4, name="ROSEWOOD", cat="KEYS", bseed=55371, recipe="4.4 Marimba / kalimba (modal)", p={
        # a rosewood marimba bar: STRC 116 = 1 : 4 : 9 (a tuned bar's), a dark hard-mallet strike with a woody knock (EXC), a
        # short ring (DAMP low: the bar dies in a few hundred ms), dry, no motion at all [S eng_phys.c; the factory MARIMBA preset's
        # DAMP 40 / BRIT 38 as the starting point]
        "MODEL": "MODAL", "STRC": 116, "BRIT": 47, "DAMP": 57, "POS": 24, "ACC": 100, "BOW": 0, "EXC": 47,
        "ENV2 FLT": dep(30), "E2ATK": 0, "E2DEC": ms(171), "E2SUS": 0, "E2REL": ms(60),
        "ATK": 0, "SUS": 127, "REL": ms(70),
        "REV": pct(11),
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 125,
    }),
    dict(engine="PHYS", idx=5, name="MOTOR VIBES", cat="KEYS", bseed=43809, recipe="4.7 Vibraphone", p={
        # a tuned metal bar, partials near 1 : 3.9 (STRC 110), a bright soft-mallet strike and a long ring (DAMP 127), the motor's
        # tremolo on the amplitude [G]
        "MODEL": "MODAL", "STRC": 110, "BRIT": 77, "DAMP": 127, "POS": 18, "ACC": 80, "BOW": 0, "EXC": 7,
        "ENV2 FLT": dep(25), "E2ATK": 0, "E2DEC": ms(1700), "E2SUS": 0, "E2REL": ms(400),
        "ATK": 0, "SUS": 127, "REL": ms(400),
        # the motor: tremolo 3-7 Hz, depth 20-50 % [G] -> LFO 2 on AMP; reverb light-medium; LF-shelf cut (high sound)
        "LFO2 WAVE": "SIN", "LFO2 RATE": hz(5.5), "MOD3": ("LFO2", "AMP", dep(27)),
        "REV": pct(30), "DLY": pct(14), "EQLG": -4,
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 124,
    }),
    dict(engine="PHYS", idx=6, name="RAIN CHIMES", cat="FX", bseed=53988, recipe="5.4 Sci-fi bell cluster (MODAL)", p={
        # the bell tree: MODAL stretched 1 : 2.83 : 5.48 : 8.90 (a chime's), bright, long ring [S eng_phys.c]
        "MODEL": "MODAL", "STRC": 70, "BRIT": 100, "DAMP": 120, "POS": 12, "ACC": 100, "BOW": 0, "EXC": 6,
        "ENV2 FLT": dep(20), "E2ATK": 0, "E2DEC": ms(900), "E2SUS": 0, "E2REL": ms(500),
        "ATK": 0, "SUS": 127, "REL": ms(500),
        # random notes of a chord falling like rain: arp RND 8ths over 3 octaves [G]
        "ARP MODE": "RND", "ARP RATE": "1/8", "ARP OCT": 3, "ARP GATE": pct(30), "ARP PROB": pct(85),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.25), "LFO2 FLT": dep(25),
        "DLY": pct(45), "REV": pct(60), "EQLG": -6,
        "MOD1": ("AT", "CUT", dep(25)),
        "LEVEL": 103,
    }),
    dict(engine="PHYS", idx=7, name="TABLA TALK", cat="OTHER", bseed=27571, recipe="hand drum (MEMB)", p={
        # MEMB: a loaded head (HARM 112: harmonic overtones, tabla-like), struck off-centre, a small pitch drop [S eng_phys.c]
        "MODEL": "MEMB", "E2": 112, "BRIT": 76, "DAMP": 56, "POS": 44, "ACC": 100, "E7": 14, "EXC": 24,
        "ENV2 FLT": dep(15), "E2ATK": 0, "E2DEC": ms(150), "E2SUS": 0, "E2REL": ms(80),
        "ATK": 0, "SUS": 127, "REL": ms(60),
        "REV": pct(15),
        # pressure = more pitch drop (BEND), velocity = brightness
        "MOD1": ("AT", "E7", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 111,
    }),
    # ====================================================================================================== WHEEL (batch A)
    # WHEEL facts (eng_wheel.c): an organ gate (ATK 0, SUS 127, short REL), no filter: the timbre is the drawbars; SHP = brightness
    # (the upper bars up, the sub bars down) so ENV 2 SHP / the matrix SHP open it; velocity is ignored on an organ (no VEL routes).
    dict(engine="WHEEL", idx=0, name="PEDAL BASS", cat="BASS", bseed=14321, mono=True, recipe="4.5 Hammond (bass registration)", p={
        "DIST": 7,   # own DIST (2026-10-12): SMART now rolls a DIST of 0 upward, and the first step costs ~2 dB; a base of 7 keeps End B in the window
        # the left-hand registration 858000000, upper bars out [S/G]; key click and a little drive
        "REG": "BASS", "TOP": -6, "CLICK": 50, "DRV": 30, "ROTR": "OFF",
        # the organ gate: A 1-5 ms, S 100 %, R 5-40 ms [G]; a short brightness blip on the key = ENV 2 SHP
        "ATK": ms(2), "SUS": 127, "REL": ms(25),
        "ENV2 SHP": dep(15), "E2ATK": 0, "E2DEC": ms(150), "E2SUS": 0, "E2REL": ms(30),
        # bass: HF-shelf cut, subtle PUMP ducker
        "EQHG": -4, "EQHF": -8, "DKSRC": "PUMP", "DKRATE": "1/4", "DKDEP": 14,
        "MOD1": ("AT", "SHP", dep(20)),
        "LEVEL": 104,
    }),
    dict(engine="WHEEL", idx=1, name="SMOKE RING", cat="LEAD", bseed=3021, mono=True, recipe="4.5 Hammond (lead registration)", p={
        # a bright even-bar registration (008888000) played mono, driven; the legato keeps the percussion off [S: single trigger]
        "REG": "BRITE", "BODY": 0, "TOP": 1, "PERC": "OFF", "CLICK": 30, "DRV": 55, "ROTR": "SLOW",
        "ATK": ms(2), "SUS": 127, "REL": ms(40),
        # the scanner vibrato V3 (no rate in the sources [S]) -> 6.5 Hz, light [G]; portamento 50 ms
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(6.5), "LFO2 FADE": ms(250), "LFO2 PIT": 1,
        "GLMODE": "TIME", "GLIDE": gl(50),
        "ENV2 SHP": dep(30), "E2ATK": 0, "E2DEC": ms(400), "E2SUS": pct(40), "E2REL": ms(80),
        "DLY": pct(18), "REV": pct(20),
        "MOD1": ("AT", "SHP", dep(25)),
        "LEVEL": 97,
    }),
    dict(engine="WHEEL", idx=2, name="CHOIR LOFT", cat="PAD", bseed=8609, recipe="4.5 Hammond (flute / chapel pad)", p={
        # CHAPL (004432000) soft and churchy, slow attack, the rotary slow [S/G]
        "REG": "CHAPL", "SUB": 1, "TOP": 2, "PERC": "OFF", "CLICK": 0, "DRV": 0, "ROTR": "SLOW",
        "ATK": ms(700), "SUS": 127, "REL": ms(1400),
        "ENV2 SHP": dep(25), "E2ATK": ms(1200), "E2DEC": ms(2000), "E2SUS": pct(60), "E2REL": ms(1200),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.25), "LFO2 SHP": dep(18),
        "CHO": pct(40), "REV": pct(55),
        # chord keys instead of an arp: the chapel's diatonic triad (the instrument's scale; MAJ triad when it is CHROMATIC)
        "CHRD": "DIA3", "VOIC": "CLOSE",
        "MOD1": ("AT", "SHP", dep(25)),
        "LEVEL": 102,
    }),
    dict(engine="WHEEL", idx=3, name="JAZZ CLUB", cat="KEYS", bseed=39110, recipe="4.5 Hammond (888000000)", p={
        # 888000000, the classic Jimmy Smith sound [S Wikipedia]; fast percussion on the 3rd harmonic, normal, the click
        # a design flaw players liked [S]; the drive mild [S SOS]
        "REG": "3BAR", "PERC": "3RD", "CLICK": 45, "DRV": 12, "ROTR": "SLOW",
        "ATK": ms(2), "SUS": 127, "REL": ms(25),
        "ENV2 SHP": dep(12), "E2ATK": 0, "E2DEC": ms(200), "E2SUS": 0, "E2REL": ms(30),
        # mod wheel = Leslie speed or drive [G] -> aftertouch to the drive
        "MOD1": ("AT", "DRV", dep(30)),
        "REV": pct(15),
        "LEVEL": 104,
    }),
    dict(engine="WHEEL", idx=4, name="MUSIC BOX", cat="PLUCK", bseed=13407, recipe="4.5 Hammond (TOPS) as an additive bell", p={
        # the top bars only (800000888) as a simple additive bell tone [S eng_wheel.c], plucked by the gate: D 350 ms, S 0
        "REG": "TOPS", "BODY": -4, "TOP": 2, "PERC": "OFF", "CLICK": 20, "DRV": 0, "ROTR": "OFF",
        "ATK": 0, "DEC": ms(350), "SUS": 0, "REL": ms(180),
        "ENV2 SHP": dep(30), "E2ATK": 0, "E2DEC": ms(250), "E2SUS": 0, "E2REL": ms(100),
        # a little winding-up arp (UP 8ths over 2 octaves); high sound: LF-shelf cut
        "ARP MODE": "UP", "ARP RATE": "1/8", "ARP OCT": 2, "ARP GATE": pct(50),
        "REV": pct(40), "DLY": pct(20), "EQLG": -6,
        "MOD1": ("AT", "SHP", dep(25)),
        "LEVEL": 108,
    }),
    dict(engine="WHEEL", idx=5, name="LESLIE STORM", cat="FX", bseed=16688, recipe="4.5 Hammond (Leslie, drive) as a swell", p={
        # every bar, driven hard, the rotor fast: an organ turned into weather [G]
        "REG": "FULL", "PERC": "OFF", "CLICK": 0, "DRV": 90, "ROTR": "FAST",
        "ATK": ms(1000), "SUS": 127, "REL": ms(1800),
        "ENV2 SHP": dep(50), "E2ATK": ms(1200), "E2DEC": 0, "E2SUS": 127, "E2REL": ms(800),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.12), "LFO2 SHP": dep(30),
        "DIST": pct(15), "DLY": pct(18), "REV": pct(33),
        "MOD1": ("AT", "SHP", dep(30)),
        # SOFT insert as a limiter (2026-10-12): flattens the crest so LEVEL can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 40, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "LEVEL": 101
    }),
    dict(engine="WHEEL", idx=6, name="CARNIVAL", cat="OTHER", bseed=65354, recipe="4.5 Hammond (reed registration), arp", p={
        # REED (008765432, brassy slope) with fast 2nd percussion and key click, played as a fairground arp [G]
        "REG": "REED", "PERC": "2ND", "CLICK": 60, "DRV": 25, "ROTR": "FAST",
        "ATK": ms(2), "SUS": 127, "REL": ms(30),
        "ARP MODE": "UPDN", "ARP RATE": "1/8", "ARP OCT": 2, "ARP GATE": pct(55), "ARP PROB": pct(90),
        "ENV2 SHP": dep(15), "E2ATK": 0, "E2DEC": ms(180), "E2SUS": 0, "E2REL": ms(30),
        "DLY": pct(30), "REV": pct(20),
        "MOD1": ("AT", "SHP", dep(20)),
        "LEVEL": 100,
    }),

    # ======================================================================================================== TRIO (batch B)
    # TRIO facts (eng_trio.c): three oscillators (WAVE picks one of 16 sets: levels, RING = osc 1 x osc 3's phase bit, SYNC = osc 2
    # restarts with osc 1) into a 12 dB SVF (MODE LP BP HP NOT, a slowly drifting cutoff, resonance stops short of
    # self-oscillation). INT2 / INT3 = osc 2 / 3 intervals in semitones, DTN = osc 2 up / osc 3 down (ct). FLT (ENV 2, the matrix
    # CUT) moves CUT, SHP moves PW (the pulse sets only). Morph: WAVE never rolls, MODE only LP <-> NOT, INT2 / INT3 are pitch items
    # (a factory End B leaves them): no struck or mode-flip cliff to tag.
    dict(engine="TRIO", idx=0, name="BREADBIN", cat="BASS", bseed=30953, mono=True, recipe="1.1 Minimoog-style bass + FAT BASS preset (chip bass)", p={
        # three saws, osc 3 an octave down, a few cents apart: the chip's fat bass [Felucca FAT BASS: DTN 9 -> 11]
        "WAVE": "SAW3", "INT2": 0, "INT3": -12, "DTN": 11, "MODE": "LP",
        # cutoff low, resonance low-mid (the recipe: low cutoff, little resonance so the low end stays) [S]; preset 62 / 45 -> 58 / 41
        "CUT": 58, "RES": 41,
        # FENV ~40 % amount, A a few ms, D ~250 ms, S low-mid [S/G]
        "ENV2 FLT": dep(45), "E2ATK": ms(4), "E2DEC": ms(230), "E2SUS": pct(22), "E2REL": ms(60),
        # AENV A 0-3 ms, D 100-300 ms, S 70-100 %, R 20-80 ms [G]
        "ATK": ms(2), "DEC": ms(210), "SUS": pct(85), "REL": ms(55),
        "GLMODE": "TIME", "GLIDE": gl(45),
        # bass: HF-shelf cut, subtle PUMP ducker
        "EQHG": -4, "EQHF": -8, "DKSRC": "PUMP", "DKRATE": "1/4", "DKDEP": 15,
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(15)),
        "LEVEL": 101,
    }),
    dict(engine="TRIO", idx=1, name="BANSHEE", cat="LEAD", bseed=16225, mono=True, recipe="2.2 Hard-sync lead (SYNC LEAD preset)", p={
        # saw osc 2 hard-synced to osc 1, 9 semitones up: its pitch is the timbre [S]; the sweep = ENV 2 on INT2 (stepped
        # semitones, chip-like): +9 falling back to rest within ~450 ms [G]
        "WAVE": "SYNC", "INT2": 9, "INT3": 0, "DTN": 0, "MODE": "LP", "CUT": 86, "RES": 27,
        "ENV2 FLT": dep(15), "E2ATK": 0, "E2DEC": ms(450), "E2SUS": 0, "E2REL": ms(150),
        "MOD3": ("ENV2", "INT2", dep(18)),
        # AENV A 0, S 100 %, R 100-200 ms [G]; portamento (Ultravox) [S] ~70 ms [G]
        "ATK": ms(3), "SUS": 127, "REL": ms(140), "GLMODE": "TIME", "GLIDE": gl(70),
        # delayed vibrato 5.5 Hz, +-19 ct, onset 350 ms; pressure opens it [S SOS brass: low, delayed]
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5.5), "LFO2 FADE": ms(350), "LFO2 PIT": 1,
        "MOD1": ("AT", "DEP2", dep(100)), "MOD2": ("VEL", "CUT", dep(15)),
        "DLY": pct(22), "REV": pct(15),
        "LEVEL": 92,
    }),
    dict(engine="TRIO", idx=2, name="CHIP CHAPEL", cat="PAD", bseed=3753, recipe="3.3 PWM string pad + CHIP CHOIR preset", p={
        # three pulses, osc 3 an octave up, detuned 7 ct; band-pass with resonance for the vowel-like chip choir [Felucca CHIP CHOIR: BP
        # CUT 62 RES 95 -> 66 / 85]
        "WAVE": "PLS3", "INT2": 0, "INT3": 12, "DTN": 7, "MODE": "BP", "CUT": 66, "RES": 85, "PW": 40,
        # PWM from a slow triangle (0.7 Hz): the string-pad recipe [S PWM, rate G]
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.7), "LFO2 SHP": dep(45),
        "ATK": ms(900), "DEC": ms(1500), "SUS": pct(92), "REL": ms(1500),
        "ENV2 FLT": dep(20), "E2ATK": ms(1200), "E2DEC": ms(2000), "E2SUS": pct(60), "E2REL": ms(1200),
        "CHO": pct(45), "REV": pct(55), "SPRD": pct(35),
        # chord keys instead of an arp: diatonic triad opened up (the instrument's scale)
        "CHRD": "DIA3", "VOIC": "OPEN",
        "MOD1": ("AT", "CUT", dep(25)),
        "LEVEL": 96,
    }),
    dict(engine="TRIO", idx=3, name="TIN TOLL", cat="KEYS", bseed=43504, recipe="4.3 Bell (ring-mod alternative) + RING BELL preset", p={
        # triangle ring-modulated by a triangle 17 semitones up (ratio ~2.7: inharmonic sum / difference partials) [S Nord: incommensurate
        # frequencies multiplied]; band-pass high, little resonance [preset 96 / 30 -> 100 / 28]
        "WAVE": "RING3", "INT2": 0, "INT3": 17, "DTN": 7, "MODE": "LP", "CUT": 108, "RES": 22,
        # struck: A 0, long decay, S 0, release ~ 0.8 s; the partials die as the cutoff closes (ENV 2)
        "ATK": 0, "DEC": ms(1300), "SUS": 0, "REL": ms(800),
        "ENV2 FLT": dep(25), "E2ATK": 0, "E2DEC": ms(500), "E2SUS": 0, "E2REL": ms(300),
        # long hall, little chorus; bells: LF-shelf cut
        "REV": pct(50), "CHO": pct(10), "EQLG": -5,
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 93,
    }),
    dict(engine="TRIO", idx=4, name="PEBBLE POP", cat="PLUCK", bseed=63877, recipe="4.2 Clavinet-like (narrow pulse opened by a short envelope) + ARP LEAD preset", p={
        # two pulses an octave apart and a triangle; the pulse starts narrow (PW 22) and ENV 2 SHP opens it [S: envelope on the wave]
        "WAVE": "PPT", "INT2": 12, "INT3": 0, "DTN": 7, "MODE": "LP", "CUT": 74, "RES": 28, "PW": 22,
        "ENV2 FLT": dep(55), "E2ATK": 0, "E2DEC": ms(160), "E2SUS": 0, "E2REL": ms(80),
        "ENV2 SHP": dep(30),
        # AENV A 0, D 150-400 ms, S 0, R 30-80 ms (damper) [G]
        "ATK": 0, "DEC": ms(170), "SUS": 0, "REL": ms(70),
        # picked arp: up 16ths over two octaves
        "ARP MODE": "UP", "ARP RATE": "1/16", "ARP OCT": 2, "ARP GATE": pct(55),
        "DLY": pct(30), "REV": pct(20),
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 102,
    }),
    dict(engine="TRIO", idx=5, name="CIRCUIT BEND", cat="FX", bseed=34291, recipe="5.2 Metallic glitch arp", p={
        # ring (triangle) + sync (saw): short metallic tones [G]; low-pass with resonance
        "WAVE": "R+S", "INT2": 7, "INT3": 0, "DTN": 9, "MODE": "LP", "CUT": 92, "RES": 45,
        "ATK": 0, "DEC": ms(220), "SUS": pct(35), "REL": ms(80),
        "ENV2 FLT": dep(30), "E2ATK": 0, "E2DEC": ms(110), "E2SUS": 0, "E2REL": ms(50),
        # arp 16ths up-down over three octaves, random gate; a slow LFO sweeps the band
        "ARP MODE": "UPDN", "ARP RATE": "1/16", "ARP OCT": 3, "ARP GATE": pct(60), "ARP PROB": pct(85),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.3), "LFO2 FLT": dep(30),
        # delay with feedback 40-60 % (global) [G]; LF-shelf cut
        "DLY": pct(45), "REV": pct(35), "EQLG": -4,
        "MOD1": ("AT", "CUT", dep(25)),
        "DIST": pct(25),
        "LEVEL": 107,
    }),
    dict(engine="TRIO", idx=6, name="GRAVEL SNARE", cat="OTHER", bseed=20254, recipe="noise percussion (P+N: pulse tone + pitched noise)", p={
        # a pulse for the body and pitched noise (osc 3 two octaves up: the noise rate follows the pitch) for the rattle [G]
        "WAVE": "P+N", "INT2": 0, "INT3": 24, "DTN": 0, "MODE": "LP", "CUT": 100, "RES": 30, "PW": 50,
        "ATK": 0, "DEC": ms(170), "SUS": 0, "REL": ms(110),
        "ENV2 FLT": dep(40), "E2ATK": 0, "E2DEC": ms(130), "E2SUS": 0, "E2REL": ms(70),
        "REV": pct(18),
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 104,
    }),
    # ======================================================================================================== PHASE (batch B)
    # PHASE facts (eng_phase.c): phase distortion, no filter: DCW (the bend) is the brightness. FLT (ENV 2 FLT, the matrix CUT) and SHP both
    # add to DCW; ENV = how much of the AMP envelope (ENV 1) is added to DCW (so the brightness follows the note's loudness: a
    # sustaining note stays bright by ENV x SUS); the R waves (RSAW RTRI RTRP) are the "resonant" ones (a peak sweeping with DCW);
    # WAVE2 alternates with WAVE every other cycle; DTN = a second line (ct), LINE MIX / RING, SUB = sine an octave down.
    dict(engine="PHASE", idx=0, name="ELASTIC BASS", cat="BASS", bseed=64702, mono=True, recipe="1.2 TB-303 acid bass (resonant wave) + RESO preset", p={
        # the resonant sawtooth: the peak sweeps down as ENV falls (the squelch); a sine sub under it [G]
        "WAVE": "RSAW", "WAVE2": "-", "DCW": 38, "ENV": 66, "DTN": 0, "LINE": "MIX", "SUB": pct(45),
        "ENV2 FLT": dep(40), "E2ATK": 0, "E2DEC": ms(190), "E2SUS": pct(10), "E2REL": ms(50),
        "ATK": ms(2), "DEC": ms(250), "SUS": pct(55), "REL": ms(55),
        "GLMODE": "TIME", "GLIDE": gl(40), "DIST": pct(10),
        "EQHG": -4, "EQHF": -8, "DKSRC": "PUMP", "DKRATE": "1/4", "DKDEP": 14,
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(15)),
        "LEVEL": 110,
    }),
    dict(engine="PHASE", idx=1, name="BRAZEN", cat="LEAD", bseed=1618, mono=True, recipe="2.3 Brassy polysynth lead + BRASS preset", p={
        # sawtooth bend; the amp envelope opens DCW (ENV 88): brightness swells with the note, as a brass filter envelope [S SOS brass]
        "WAVE": "SAW", "WAVE2": "-", "DCW": 28, "ENV": 88, "DTN": 7, "LINE": "MIX", "SUB": 0,
        "ATK": ms(22), "DEC": ms(300), "SUS": pct(85), "REL": ms(120),
        "ENV2 FLT": dep(30), "E2ATK": ms(40), "E2DEC": ms(350), "E2SUS": pct(50), "E2REL": ms(100),
        "GLMODE": "TIME", "GLIDE": gl(60),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5), "LFO2 FADE": ms(400), "LFO2 PIT": 1,
        "MOD1": ("AT", "DEP2", dep(100)), "MOD2": ("VEL", "CUT", dep(25)),
        "CHO": pct(22), "DLY": pct(18), "REV": pct(18),
        "LEVEL": 93,
    }),
    dict(engine="PHASE", idx=2, name="LAMPLIGHT", cat="PAD", bseed=23204, recipe="3.3 PWM string pad (phase-distortion strings) + STRING preset", p={
        # saw with a double-sine WAVE2 every other cycle, the second line 13 ct off: a built-in ensemble; a soft sub [Felucca STRING]
        "WAVE": "SAW", "WAVE2": "DSIN", "DCW": 46, "ENV": 36, "DTN": 13, "LINE": "MIX", "SUB": pct(15),
        "ATK": ms(800), "DEC": ms(1800), "SUS": pct(92), "REL": ms(1600),
        "ENV2 FLT": dep(20), "E2ATK": ms(1100), "E2DEC": ms(2000), "E2SUS": pct(60), "E2REL": ms(1200),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.35), "LFO2 FLT": dep(15),
        "CHO": pct(55), "REV": pct(33), "SPRD": pct(30),
        # chord keys instead of an arp: diatonic sevenths (the instrument's scale)
        "CHRD": "DIA7", "VOIC": "CLOSE",
        "MOD1": ("AT", "CUT", dep(25)),
        # SOFT insert as a limiter (2026-10-12): flattens the crest so LEVEL can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 70, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "LEVEL": 85
    }),
    dict(engine="PHASE", idx=3, name="WIRE HARP", cat="PLUCK", bseed=12692, recipe="4.6 String pluck / harp + WIRE preset", p={
        # soft double sine with the resonant triangle on every other cycle; ENV 70 so the bend (the brightness) dies with the note: a
        # pluck's decaying upper partials [S Karplus-Strong logic]
        "WAVE": "DSIN", "WAVE2": "RTRI", "DCW": 40, "ENV": 70, "DTN": 7, "LINE": "MIX", "SUB": pct(10),
        "ATK": 0, "DEC": ms(500), "SUS": pct(30), "REL": ms(160),
        "ENV2 FLT": dep(25), "E2ATK": 0, "E2DEC": ms(240), "E2SUS": 0, "E2REL": ms(120),
        "ARP MODE": "UP", "ARP RATE": "1/8", "ARP OCT": 2, "ARP GATE": pct(60),
        "CHO": pct(20), "REV": pct(40), "DLY": pct(20),
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 98,
    }),
    dict(engine="PHASE", idx=4, name="CLAVICLE", cat="KEYS", bseed=20728, recipe="4.2 Clavinet-like (pulse wave opened by a short envelope)", p={
        # the bent square (a reedy wave; the PLS pulse leaves a DC offset the regress health check rejects) opened by the amp envelope's attack [S: envelope on the wave]; no second line
        "WAVE": "SQR", "WAVE2": "-", "DCW": 50, "ENV": 61, "DTN": 0, "LINE": "MIX", "SUB": 0,
        # AENV A 0, D 150-400 ms, S 0-10 %, R 30-80 ms (damper) [G]
        "ATK": 0, "DEC": ms(230), "SUS": pct(18), "REL": ms(50),
        "ENV2 FLT": dep(30), "E2ATK": 0, "E2DEC": ms(120), "E2SUS": 0, "E2REL": ms(40),
        # velocity to the bend is the clav's expression [G]; light chorus
        "CHO": pct(12), "REV": pct(12),
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(30)),
        "LEVEL": 105,
    }),
    dict(engine="PHASE", idx=5, name="CRYSTAL CAVE", cat="FX", bseed=37002, recipe="5.4 Sci-fi bell cluster + BELL preset", p={
        # the resonant triangle and resonant trapezoid ring-modulated (second line 53 ct off): inharmonic glassy tones [G]
        "WAVE": "RTRI", "WAVE2": "RTRP", "DCW": 72, "ENV": 50, "DTN": 53, "LINE": "RING", "SUB": 0,
        "ATK": 0, "DEC": ms(1500), "SUS": pct(25), "REL": ms(500),
        "ENV2 FLT": dep(20), "E2ATK": 0, "E2DEC": ms(600), "E2SUS": 0, "E2REL": ms(300),
        # random notes falling like drops: arp RND 8ths over three octaves; a slow LFO moves the bend
        "ARP MODE": "RND", "ARP RATE": "1/8", "ARP OCT": 3, "ARP GATE": pct(35), "ARP PROB": pct(85),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.2), "LFO2 FLT": dep(25),
        "DLY": pct(50), "REV": pct(60), "EQLG": -6,
        "MOD1": ("AT", "CUT", dep(25)),
        "LEVEL": 118,
    }),
    # ======================================================================================================== LOFI (batch B)
    # LOFI facts (eng_lofi.c): a chip voice. CHIP 4BIT / 4B/2 (half rate) / 8BIT / 1BIT / STEP (a 4-bit stepped envelope replaces the
    # ADSR curve: held at full, only REL ends it; CRSH is then its shape), WAVE PLS TRI SAW NOIS WRAM (DUTY / 8 picks one of 16 tables),
    # SWP = a pitch slide down that never stops (p / 8 steps a control tick: 8..15 = 1, ~86 semitones a second), VIB = LFO-free vibrato,
    # ARP = the chip's own fast arp (OCT MAJ MIN); TONE = the low-pass (FLT only opens it). Morph: SWP / VIB are pitch items (a factory End B
    # leaves them), CHIP / DUTY / ARP never roll, WAVE only PLS / TRI / SAW: no cliff to tag.
    dict(engine="LOFI", idx=0, name="CARTRIDGE", cat="BASS", bseed=54341, mono=True, recipe="1.1 Minimoog-style bass (as a Game Boy wave bass) + WAVE BASS preset", p={
        # wave RAM BAS2 (DUTY 64..71: table 8, 1 + 2 + 3 harmonics) in the 4-bit chip: the console's wave bass [G]
        "CHIP": "4BIT", "WAVE": "WRAM", "DUTY": 67, "CRSH": 0, "SWP": 0, "VIB": 0, "ARP": "OFF", "TONE": 72,
        "ATK": ms(2), "DEC": ms(200), "SUS": pct(80), "REL": ms(40),
        "ENV2 FLT": dep(25), "E2ATK": 0, "E2DEC": ms(150), "E2SUS": 0, "E2REL": ms(40),
        "EQHG": -4, "EQHF": -8, "DKSRC": "PUMP", "DKRATE": "1/4", "DKDEP": 15,
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(15)),
        "LEVEL": 99,
    }),
    dict(engine="LOFI", idx=1, name="PIXEL HERO", cat="LEAD", bamt=21, bseed=49156, mono=True, recipe="2.5 Chip / 8-bit lead + STEP LEAD preset", p={
        # the console's stepped chip at constant volume (CRSH 0: the envelope held): 25 % pulse (DUTY 49), S high [S 2A03]
        "CHIP": "STEP", "WAVE": "PLS", "DUTY": 49, "CRSH": 0, "SWP": 0, "VIB": 0, "ARP": "OFF", "TONE": 110,
        "ATK": 0, "SUS": 127, "REL": ms(30),
        # delayed vibrato 6 Hz +-19 ct, onset 300 ms [G]; pressure opens it; the width moves a little at the start (ENV 2 SHP)
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(6), "LFO2 FADE": ms(300), "LFO2 PIT": 1,
        "ENV2 FLT": dep(12), "E2ATK": 0, "E2DEC": ms(300), "E2SUS": 0, "E2REL": ms(60), "ENV2 SHP": dep(18),
        "GLMODE": "TIME", "GLIDE": gl(35),
        "DLY": pct(22), "REV": pct(12),
        "MOD1": ("AT", "DEP2", dep(100)), "MOD2": ("VEL", "CUT", dep(15)),
        "LEVEL": 91,
    }),
    dict(engine="LOFI", idx=2, name="WOW FLUTTER", cat="PAD", bseed=13549, recipe="3.5 Choir / vowel pad (a 4-bit tape choir) + 5.5 tape-style wow", p={
        # wave RAM VOXA ("ah", DUTY 92: table 11) at half rate, a slow wow on the pitch; dark TONE [Felucca WAVE LEAD uses VOXA]
        "CHIP": "4B/2", "WAVE": "WRAM", "DUTY": 92, "CRSH": 0, "SWP": 0, "VIB": 14, "ARP": "OFF", "TONE": 70,
        "ATK": ms(700), "DEC": ms(1500), "SUS": pct(92), "REL": ms(1400),
        "ENV2 FLT": dep(20), "E2ATK": ms(1100), "E2DEC": ms(2000), "E2SUS": pct(60), "E2REL": ms(1200),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.3), "LFO2 FLT": dep(18),
        "CHO": pct(50), "REV": pct(55), "SPRD": pct(30),
        # chord keys instead of an arp: minor sevenths in the natural minor
        "CHRD": "MIN7", "VOIC": "OPEN",
        "MOD1": ("AT", "CUT", dep(25)),
        "LEVEL": 101,
    }),
    dict(engine="LOFI", idx=3, name="MARBLE RUN", cat="PLUCK", bseed=8179, recipe="4.4 Marimba / kalimba + ARP 8BIT preset", p={
        # a 4-bit triangle, short and round, dark TONE closing with the note [G]
        "CHIP": "4BIT", "WAVE": "TRI", "DUTY": 64, "CRSH": 0, "SWP": 0, "VIB": 0, "ARP": "OFF", "TONE": 96,
        "ATK": 0, "DEC": ms(300), "SUS": 0, "REL": ms(150),
        "ENV2 FLT": dep(30), "E2ATK": 0, "E2DEC": ms(220), "E2SUS": 0, "E2REL": ms(100),
        "ARP MODE": "UP", "ARP RATE": "1/8", "ARP OCT": 2, "ARP GATE": pct(45),
        "REV": pct(35), "DLY": pct(30), "EQLG": -3,
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 112,
    }),
    dict(engine="LOFI", idx=4, name="DIVE BOMB", cat="FX", bseed=40354, recipe="5.1 Noise sweep / riser (inverted: a pitch fall) with SWP", p={
        # the pitch falls at once and never stops (SWP 9: the slowest, ~86 semitones a second): a siren dropping out of the sky
        "CHIP": "4BIT", "WAVE": "PLS", "DUTY": 40, "CRSH": 0, "SWP": 5, "VIB": 0, "ARP": "OFF", "TONE": 100,
        "ATK": ms(30), "DEC": ms(10000), "SUS": 127, "REL": ms(300),
        "ENV2 FLT": dep(20), "E2ATK": 0, "E2DEC": ms(800), "E2SUS": pct(50), "E2REL": ms(200),
        "LFO2 WAVE": "SAW", "LFO2 RATE": hz(8), "LFO2 SHP": dep(30),
        "DLY": pct(45), "REV": pct(45),
        "MOD1": ("AT", "CUT", dep(25)),
        "LEVEL": 91,
    }),
    dict(engine="LOFI", idx=5, name="DEAD BATTERY", cat="OTHER", bseed=41266, recipe="5.5 Bitcrushed lofi arp", p={
        # saw in the 4-bit chip, sample-rate reduced (CRSH 52: every 7th sample held), wow VIB 10, low-passed [G: duty / bits 4-8, rate
        # ~8-16 kHz, tape wow]
        "CHIP": "4BIT", "WAVE": "SAW", "DUTY": 64, "CRSH": 52, "SWP": 0, "VIB": 10, "ARP": "OFF", "TONE": 78,
        "ATK": 0, "DEC": ms(140), "SUS": pct(15), "REL": ms(90),
        "ENV2 FLT": dep(20), "E2ATK": 0, "E2DEC": ms(120), "E2SUS": 0, "E2REL": ms(60),
        "ARP MODE": "UPDN", "ARP RATE": "1/16", "ARP OCT": 2, "ARP GATE": pct(50), "ARP PROB": pct(90),
        "DLY": pct(40), "REV": pct(30),
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(20)),
        "LEVEL": 109,
    }),
    dict(engine="LOFI", idx=6, name="PIXEL KICK", cat="OTHER", bseed=33683, recipe="chip percussion (SWP: the pitch fall of a kick)", p={
        # a triangle that falls ~260 semitones a second (SWP 16: 2 steps a tick): the chip kick [G]
        "CHIP": "4BIT", "WAVE": "TRI", "DUTY": 64, "CRSH": 0, "SWP": 16, "VIB": 0, "ARP": "OFF", "TONE": 90,
        "ATK": 0, "DEC": ms(180), "SUS": 0, "REL": ms(80), "DIST": pct(15),
        "ENV2 FLT": dep(12), "E2ATK": 0, "E2DEC": ms(90), "E2SUS": 0, "E2REL": ms(40),
        "REV": pct(8),
        # pressure = a deeper fall; velocity = brightness
        "MOD1": ("AT", "SWP", dep(20)), "MOD2": ("VEL", "CUT", dep(20)),
        "LEVEL": 118,
    }),
    # ======================================================================================================== VOICE (batch B)
    # VOICE facts (eng_formant.c): a Klatt formant synth, 4 voices. VOWL / VOWL2 = the two vowels (0 A, 32 E, 63 I, 95 O, 127 U); TALK = how
    # fast a note glides from VOWL to VOWL2 (0: stays on VOWL); SHIFT = formants in semitones; BUZZ = glottal tilt (brightness; velocity
    # adds to it already: no VEL route); BRTH = breath noise (the top fades the voicing: whisper); Q = bandwidth (and level); RAND = a random
    # vowel per note. FLT (ENV 2 FLT, the matrix CUT) moves BOTH vowels along the A..U axis (it is "wow", not a cutoff); SHP = BUZZ.
    # Morph: BRTH is capped at 100, TALK is logarithmic: no cliff to tag.
    dict(engine="VOICE", idx=0, name="BULLFROG", cat="BASS", bseed=51174, mono=True, recipe="1.4 slap / wow bass (formant) + WOW BASS preset", p={
        # an "oh" opening to "ah" (TALK 62) with a strong buzz: the wow bass [Felucca WOW BASS: BUZZ 100 / Q 80 -> 96 / 78]
        "VOWL": 95, "VOWL2": 6, "TALK": 62, "SHIFT": 0, "BUZZ": 96, "BRTH": 0, "Q": 78, "RAND": 0,
        # each note throws the vowels 18 steps toward U and lets them fall back (ENV 2 on FLT): the croak
        "ENV2 FLT": dep(18), "E2ATK": 0, "E2DEC": ms(250), "E2SUS": 0, "E2REL": ms(80),
        "ATK": ms(3), "DEC": ms(300), "SUS": pct(80), "REL": ms(70),
        "GLMODE": "TIME", "GLIDE": gl(50),
        "EQHG": -4, "EQHF": -8, "DKSRC": "PUMP", "DKRATE": "1/4", "DKDEP": 15,
        "MOD1": ("AT", "SHP", dep(25)),
        "LEVEL": 91,
    }),
    dict(engine="VOICE", idx=1, name="CROONER", cat="LEAD", bseed=41697, mono=True, recipe="2.7 Formant / vocal lead + VOX LEAD preset", p={
        # an "eh" closing toward "ah" at each note start (TALK 44), a slightly raised formant set (SHIFT 2), breath 9 [Felucca VOX LEAD]
        "VOWL": 22, "VOWL2": 0, "TALK": 44, "SHIFT": 2, "BUZZ": 88, "BRTH": 9, "Q": 70, "RAND": 0,
        "ENV2 FLT": dep(8), "E2ATK": 0, "E2DEC": ms(300), "E2SUS": 0, "E2REL": ms(100),
        "ATK": ms(12), "SUS": 127, "REL": ms(110),
        # vibrato 5.5 Hz, +-19 ct, onset 450 ms; glide on [G]; pressure deepens the vibrato (velocity already brightens: BUZZ)
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5.5), "LFO2 FADE": ms(450), "LFO2 PIT": 1,
        "GLMODE": "TIME", "GLIDE": gl(55),
        "MOD1": ("AT", "DEP2", dep(100)),
        "CHO": pct(18), "DLY": pct(22), "REV": pct(30),
        "LEVEL": 87,
    }),
    dict(engine="VOICE", idx=2, name="VESPERS", cat="PAD", bseed=45717, recipe="3.5 Choir / vowel pad + CHOIR AAH preset", p={
        # "ah" drifting toward "oh" over the note, a slight random vowel per note (RAND), breath at low level [S/G 3.5]
        "VOWL": 0, "VOWL2": 95, "TALK": 20, "SHIFT": 0, "BUZZ": 42, "BRTH": 21, "Q": 58, "RAND": 14,
        "ATK": ms(700), "DEC": ms(1500), "SUS": pct(92), "REL": ms(1400),
        "ENV2 FLT": dep(12), "E2ATK": ms(1400), "E2DEC": ms(2000), "E2SUS": pct(60), "E2REL": ms(1000),
        # vibrato 5 Hz with a long delay [G: depth 5-12 ct; the smallest step is 19 ct]
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5), "LFO2 FADE": ms(900), "LFO2 PIT": 1,
        "CHO": pct(45), "REV": pct(70), "SPRD": pct(40),
        # chord keys instead of an arp: a diatonic triad (3 of the 4 voices) opened up (the instrument's scale)
        "CHRD": "DIA3", "VOIC": "OPEN",
        "MOD1": ("AT", "SHP", dep(30)),
        "LEVEL": 86,
    }),
    dict(engine="VOICE", idx=3, name="DOO WOP", cat="PLUCK", bseed=8057, recipe="4.4 Marimba / kalimba (a vocal pluck) + 5.5 arp", p={
        # a short "oh" snapping to "ah" (TALK 88), narrow bandwidth (Q 92) so it rings like a bottle [G]
        "VOWL": 95, "VOWL2": 0, "TALK": 88, "SHIFT": 0, "BUZZ": 70, "BRTH": 12, "Q": 92, "RAND": 0,
        "ATK": 0, "DEC": ms(220), "SUS": 0, "REL": ms(150),
        "ENV2 FLT": dep(30), "E2ATK": 0, "E2DEC": ms(180), "E2SUS": 0, "E2REL": ms(100),
        "ARP MODE": "UPDN", "ARP RATE": "1/8", "ARP OCT": 2, "ARP GATE": pct(55),
        "CHO": pct(18), "REV": pct(35), "DLY": pct(22),
        "MOD1": ("AT", "SHP", dep(25)),
        "LEVEL": 103,
    }),
    dict(engine="VOICE", idx=4, name="GHOST VOICES", cat="FX", bseed=15764, recipe="5.3 Wind / texture (breath) + WHISPER preset", p={
        # nearly all breath (BRTH 118: the voicing fades), the vowel drifting "ah" -> between "i" and "o" (TALK 70) with a random vowel per
        # note; shifted up 3 semitones [Felucca WHISPER]
        "VOWL": 0, "VOWL2": 100, "TALK": 70, "SHIFT": 3, "BUZZ": 52, "BRTH": 118, "Q": 50, "RAND": 40,
        "ATK": ms(1200), "DEC": ms(1500), "SUS": 127, "REL": ms(1800),
        # a very slow LFO moves both vowels along the axis (0.1 Hz)
        "ENV2 FLT": dep(15), "E2ATK": ms(1500), "E2DEC": ms(2500), "E2SUS": pct(70), "E2REL": ms(1500),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.1), "LFO2 FLT": dep(35),
        "DLY": pct(24), "REV": pct(45), "CHO": pct(30),
        "MOD1": ("AT", "SHP", dep(25)),
        # SOFT insert as a limiter (2026-10-12): flattens the crest so LEVEL can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 70, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "LEVEL": 90
    }),
    dict(engine="VOICE", idx=5, name="BEATBOX", cat="OTHER", bseed=31759, recipe="vocal percussion (breath + random vowel)", p={
        # a whispered burst (BRTH 100) with a random vowel per note (RAND 70 %), narrow bands (Q 110), cut short [G]
        "VOWL": 0, "VOWL2": 127, "TALK": 100, "SHIFT": 0, "BUZZ": 30, "BRTH": 100, "Q": 110, "RAND": pct(70),
        "ATK": 0, "DEC": ms(130), "SUS": 0, "REL": ms(80),
        "ENV2 FLT": dep(25), "E2ATK": 0, "E2DEC": ms(110), "E2SUS": 0, "E2REL": ms(50),
        "REV": pct(15),
        "MOD1": ("AT", "SHP", dep(25)),
        "LEVEL": 119,
    }),

    # ====================================================================================================== NOISE (batch C)
    # NOISE facts (eng_noise.c): ANLG / DUST / LFSR / META source (COLR tilt) into one SVF (FREQ, RES: LP -> narrow BP), TRK = FREQ follows
    # the key, DRFT a slow wander. FLT (ENV 2 FLT, matrix CUT) moves FREQ, SHP moves COLR. META LEN = DENS knob >> 4 (0..7), LFSR CLK = CRSH.
    # Morph: MODE / LEN never roll, CLK is a pitch item; DENS and CRSH of ANLG / DUST stay at 0 when 0 (morph.c: a noise that was clean
    # does not start to crackle or crush).
    dict(engine="NOISE", idx=0, name="GEARBOX", cat="BASS", bseed=12477, mono=True, recipe="1.1 Minimoog-style bass envelope on a META register (chip noise-channel bass, 2.5)", p={
        # the 63-step register (LEN 3) at the key's pitch: a rough, hollow buzz; low-passed, FREQ following the key [G]
        "MODE": "META", "COLR": 0, "FREQ": 52, "RES": 28, "TRK": 127, "DENS": 48, "CRSH": 0,
        "ENV2 FLT": dep(40), "E2ATK": ms(4), "E2DEC": ms(240), "E2SUS": pct(25), "E2REL": ms(60),
        "ATK": ms(2), "DEC": ms(200), "SUS": pct(85), "REL": ms(55),
        "EQHG": -4, "EQHF": -8, "DKSRC": "PUMP", "DKRATE": "1/4", "DKDEP": 15,
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(15)),
        "LEVEL": 95,
    }),
    dict(engine="NOISE", idx=1, name="HORNET", cat="LEAD", bamt=21, bseed=49660, mono=True, recipe="2.5 Chip lead on an LFSR (the 127-step register, 7 octaves up)", p={
        # ARCADE's register clocked 84 st above the key: one period per key period, a pitched buzz; opened a little [S eng_noise.c]
        "MODE": "LFSR", "COLR": 0, "FREQ": 104, "RES": 22, "TRK": 127, "DENS": 112, "CRSH": 84,
        "ATK": ms(3), "SUS": 127, "REL": ms(70), "GLMODE": "TIME", "GLIDE": gl(45),
        "ENV2 FLT": dep(20), "E2ATK": 0, "E2DEC": ms(300), "E2SUS": pct(50), "E2REL": ms(80),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(6), "LFO2 FADE": ms(300), "LFO2 PIT": 1,
        "MOD1": ("AT", "DEP2", dep(100)), "MOD2": ("VEL", "CUT", dep(15)),
        "DLY": pct(20), "REV": pct(12),
        "LEVEL": 96,
    }),
    dict(engine="NOISE", idx=2, name="SEA ORGAN", cat="PAD", bseed=64587, recipe="3.5 Choir pad on pitched noise (5.3 wind: resonant band-pass, key tracked)", p={
        # pink noise through a narrow band-pass that follows the key: a breathy tone on each chord note [G]
        "MODE": "ANLG", "COLR": 64, "FREQ": 72, "RES": 112, "TRK": 127, "DENS": 0, "DRFT": 24, "CRSH": 0,
        "ATK": ms(1100), "SUS": 127, "REL": ms(2500),
        "ENV2 FLT": dep(15), "E2ATK": ms(1500), "E2DEC": ms(2000), "E2SUS": pct(60), "E2REL": ms(1200),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.17), "LFO2 FLT": dep(10),
        "CHO": pct(40), "REV": pct(65), "SPRD": pct(40),
        "CHRD": "DIA3", "VOIC": "OPEN",
        "MOD1": ("AT", "CUT", dep(20)),
        "LEVEL": 100,
    }),
    dict(engine="NOISE", idx=3, name="WHITE OUT", cat="FX", bamt=12, bseed=38918, recipe="5.1 Noise sweep / riser", p={
        # white noise, a low-pass swept up by ENV 2 over ~3 s with a small resonance bump [S MusicRadar / Violet]
        "MODE": "ANLG", "COLR": 14, "FREQ": 34, "RES": 40, "TRK": 0, "DENS": 0, "DRFT": 0, "CRSH": 0,
        "ENV2 FLT": 63, "E2ATK": 100, "E2DEC": 0, "E2SUS": 127, "E2REL": ms(300),
        "ATK": ms(900), "SUS": 127, "REL": ms(400),
        "REV": pct(24), "DLY": pct(16), "MOD3": ("ENV2", "REV", dep(50)), "EQLG": -4,
        "MOD1": ("AT", "CUT", dep(25)),
        # SOFT insert as a limiter (2026-10-12): flattens the crest so LEVEL can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 100, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "LEVEL": 88
    }),
    dict(engine="NOISE", idx=4, name="MOOR WIND", cat="FX", bseed=56189, recipe="5.3 Wind / texture", p={
        # pink-brown noise, resonant band-pass 40-80 %, the cutoff wandering (DRFT) and a slow LFO [G]
        "MODE": "ANLG", "COLR": 74, "FREQ": 60, "RES": 88, "TRK": 0, "DENS": 0, "DRFT": 90, "CRSH": 0,
        "ATK": ms(1800), "SUS": 127, "REL": ms(3000),
        "ENV2 FLT": dep(12), "E2ATK": ms(2000), "E2DEC": ms(2500), "E2SUS": pct(70), "E2REL": ms(1500),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.11), "LFO2 FLT": dep(30),
        "REV": pct(42), "DLY": pct(9), "EQLG": -4,
        "MOD1": ("AT", "CUT", dep(25)),
        # SOFT insert as a limiter (2026-10-12): flattens the crest so LEVEL can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 100, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "LEVEL": 91
    }),
    dict(engine="NOISE", idx=5, name="TIZZ HAT", cat="PERC", bseed=49614, recipe="noise percussion (closed hat: white noise, high band, 45 ms)", p={
        "MODE": "ANLG", "COLR": 0, "FREQ": 116, "RES": 52, "TRK": 0, "DENS": 0, "DRFT": 0, "CRSH": 0,
        "ATK": 0, "DEC": ms(45), "SUS": 0, "REL": ms(30),
        "ENV2 FLT": dep(15), "E2ATK": 0, "E2DEC": ms(40), "E2SUS": 0, "E2REL": ms(20),
        "REV": pct(10), "EQLG": -6,
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 117,
    }),
    dict(engine="NOISE", idx=6, name="OPEN SIZZLE", cat="PERC", bseed=25614, recipe="noise percussion (open hat: as TIZZ HAT, but ~500 ms, bright all through)", p={
        # an open hat: white noise, the filter wide open and kept there (no closing envelope: the sizzle stays bright to its
        # end), a long decay with a short release when the key goes (the choke)
        "MODE": "ANLG", "COLR": 0, "FREQ": 117, "RES": 41, "TRK": 0, "DENS": 0, "DRFT": 0, "CRSH": 0,
        "ATK": 0, "DEC": ms(717), "SUS": 0, "REL": ms(117),
        "ENV2 FLT": 0, "E2ATK": 0, "E2DEC": ms(250), "E2SUS": 127, "E2REL": ms(80),
        "REV": pct(17), "EQLG": -6,
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 117,
    }),
    dict(engine="NOISE", idx=7, name="STATIC SNARE", cat="PERC", bseed=58051, recipe="noise percussion (snare: noise burst with a closing filter + a ringing low band for the body)", p={
        # an analog snare: noise through a resonant band-pass; the filter envelope throws it open at the hit (the crack) and
        # drops it in ~60 ms onto the low resonance (the shell's body); short decay
        "MODE": "ANLG", "COLR": 0, "FREQ": 61, "RES": 71, "TRK": 0, "DENS": 0, "DRFT": 0, "CRSH": 0,
        "ATK": 0, "DEC": ms(171), "SUS": 0, "REL": ms(60),
        "ENV2 FLT": dep(71), "E2ATK": 0, "E2DEC": ms(57), "E2SUS": 0, "E2REL": ms(40),
        "REV": pct(18),
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 114,
    }),
    dict(engine="NOISE", idx=8, name="GEIGER", cat="OTHER", bseed=56918, recipe="5.3 texture (DUST: impulses through a ringing band-pass)", p={
        # ~60 clicks a second through a resonant band-pass; pressure raises the density
        "MODE": "DUST", "COLR": 0, "FREQ": 98, "RES": 92, "TRK": 0, "DENS": 110, "DRFT": 20, "CRSH": 0,
        "ATK": ms(4), "SUS": 127, "REL": ms(200),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.3), "LFO2 FLT": dep(20),
        "REV": pct(30), "DLY": pct(25),
        "MOD1": ("AT", "DENS", dep(30)),
        "DIST": pct(45),
        "LEVEL": 106,
    }),
    dict(engine="NOISE", idx=9, name="MODEM", cat="OTHER", bseed=7821, recipe="5.2 metallic glitch arp (LFSR 63 steps) + 5.5 bitcrush", p={
        "MODE": "LFSR", "COLR": 0, "FREQ": 110, "RES": 30, "TRK": 127, "DENS": 48, "CRSH": 60,
        "ATK": 0, "DEC": ms(90), "SUS": 0, "REL": ms(60),
        "ENV2 FLT": dep(20), "E2ATK": 0, "E2DEC": ms(80), "E2SUS": 0, "E2REL": ms(40),
        "ARP MODE": "UPDN", "ARP RATE": "1/16", "ARP OCT": 3, "ARP GATE": pct(45), "ARP PROB": pct(85),
        "DLY": pct(40), "REV": pct(20),
        "MOD1": ("AT", "CUT", dep(25)),
        "LEVEL": 114,
    }),
    # ====================================================================================================== GRAIN (batch C)
    # GRAIN facts (eng_grain.c): over the built-in SAMPLE sets (SRC 0 PIANO, 2 FLUTE, 3 SAX; USR1/USR2 hold the owner's own material,
    # empty in a build and in the audit: a factory sound there would play the sine fallback, so none uses them). SIZE 10..390 ms
    # (441 * 2^(SIZE/24) samples), DENS grains/s, SPRD random spread of POS (0: frozen), RAND detune and reversed share, TONE low-pass.
    # FLT moves TONE, SHP moves POS (an LFO scans). 3 voices (chords of three). Morph: SRC never rolls, PTCH is a pitch item.
    dict(engine="GRAIN", idx=0, name="FLUTE FOG", cat="PAD", bseed=27235, recipe="3.4 Evolving / granular pad (flute source)", p={
        # grains ~100 ms (SIZE 88), ~5 overlapping (DENS 75), a little spray, slow scan of the position [S Splice / Violet / MainStage]
        "SRC": 2, "POS": 50, "SIZE": 88, "DENS": 77, "PTCH": 0, "E6": 37, "RAND": 12, "TONE": 100,
        "ATK": ms(1200), "SUS": 127, "REL": ms(3000),
        "ENV2 FLT": dep(15), "E2ATK": ms(1500), "E2DEC": ms(2500), "E2SUS": pct(60), "E2REL": ms(1500),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.07), "LFO2 SHP": dep(25),
        "CHO": pct(35), "REV": pct(75), "STEREO SPRD": pct(35),
        "CHRD": "MIN7", "VOIC": "OPEN",
        "MOD1": ("AT", "CUT", dep(25)),
        "LEVEL": 101,
    }),
    dict(engine="GRAIN", idx=1, name="BROKEN CHOIR", cat="PAD", bseed=43420, recipe="3.5 Choir pad, granulated sax", p={
        "SRC": 3, "POS": 40, "SIZE": 96, "DENS": 71, "PTCH": 0, "E6": 61, "RAND": 31, "TONE": 90,
        "ATK": ms(900), "SUS": 127, "REL": ms(2500),
        "ENV2 FLT": dep(15), "E2ATK": ms(1200), "E2DEC": ms(2000), "E2SUS": pct(60), "E2REL": ms(1200),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.13), "LFO2 SHP": dep(30),
        "CHO": pct(45), "REV": pct(70), "STEREO SPRD": pct(40),
        "CHRD": "DIA3", "VOIC": "CLOSE",
        "MOD1": ("AT", "CUT", dep(25)),
        "LEVEL": 95,
    }),
    dict(engine="GRAIN", idx=2, name="STRETCH", cat="FX", bseed=581, recipe="3.4 granular time-stretch (piano held at one position, slowly scanned)", p={
        "SRC": 0, "POS": 8, "SIZE": 104, "DENS": 61, "PTCH": 0, "E6": 9, "RAND": 7, "TONE": 105,
        "ATK": ms(600), "SUS": 127, "REL": ms(3500),
        "ENV2 SHP": dep(30), "E2ATK": ms(4000), "E2DEC": 0, "E2SUS": 127, "E2REL": ms(500),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.09), "LFO2 SHP": dep(20),
        "REV": pct(80), "DLY": pct(25), "EQLG": -3,
        "MOD1": ("AT", "SHP", dep(25)),
        "LEVEL": 96,
    }),
    dict(engine="GRAIN", idx=3, name="BACKWARDS", cat="FX", bamt=12, bseed=49192, recipe="3.4 granular texture, many reversed grains an octave down", p={
        "SRC": 3, "POS": 55, "SIZE": 98, "DENS": 64, "PTCH": -12, "E6": 50, "RAND": 100, "TONE": 80,
        "ATK": ms(600), "SUS": 127, "REL": ms(3000),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.2), "LFO2 FLT": dep(25),
        "ENV2 FLT": dep(15), "E2ATK": ms(700), "E2DEC": ms(2000), "E2SUS": pct(70), "E2REL": ms(1500),
        "REV": pct(42), "DLY": pct(18), "CHO": pct(30),
        "MOD1": ("AT", "CUT", dep(25)),
        # SOFT insert as a limiter (2026-10-12): flattens the crest so LEVEL can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 100, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "LEVEL": 85
    }),
    dict(engine="GRAIN", idx=4, name="FELT PETALS", cat="PLUCK", bseed=40371, recipe="4.6 string pluck / 4.4 kalimba (the piano's attack as short grains)", p={
        # grains from the first part of the piano note (POS 0), 30 ms, dense and nearly frozen: a soft felted pluck
        "SRC": 0, "POS": 2, "SIZE": 50, "DENS": 77, "PTCH": 0, "E6": 7, "RAND": 4, "TONE": 108,
        "ATK": 0, "DEC": ms(900), "SUS": pct(30), "REL": ms(200),
        "ENV2 FLT": dep(25), "E2ATK": 0, "E2DEC": ms(300), "E2SUS": 0, "E2REL": ms(150),
        "ARP MODE": "UP", "ARP RATE": "1/8", "ARP OCT": 2, "ARP GATE": pct(60),
        "REV": pct(40), "DLY": pct(25), "EQLG": -4,
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 103,
    }),
    # CRICKETS (GRAIN, was idx 5) deleted 2026-10-12 (owner); the generator needs contiguous idx, so GHOST SAX and TAR PIT moved down to 5 and 6
    dict(engine="GRAIN", idx=5, name="GHOST SAX", cat="LEAD", bseed=25202, mono=True, recipe="2.7 vocal lead idea on a granulated sax (dense, nearly frozen)", p={
        "SRC": 3, "POS": 30, "SIZE": 86, "DENS": 107, "PTCH": 0, "E6": 5, "RAND": 4, "TONE": 110,
        "ATK": ms(25), "SUS": 127, "REL": ms(180), "GLMODE": "TIME", "GLIDE": gl(55),
        "ENV2 FLT": dep(15), "E2ATK": ms(30), "E2DEC": ms(400), "E2SUS": pct(60), "E2REL": ms(120),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5.5), "LFO2 FADE": ms(450), "LFO2 PIT": 1,
        "MOD1": ("AT", "DEP2", dep(100)), "MOD2": ("VEL", "CUT", dep(20)),
        "CHO": pct(18), "DLY": pct(22), "REV": pct(28),
        "LEVEL": 97,
    }),
    dict(engine="GRAIN", idx=6, name="TAR PIT", cat="BASS", bseed=64371, mono=True, recipe="1.3 sub-ish bass from sax grains an octave down, dark TONE", p={
        "SRC": 3, "POS": 20, "SIZE": 100, "DENS": 110, "PTCH": -12, "E6": 11, "RAND": 6, "TONE": 58,
        "ATK": ms(5), "SUS": 127, "REL": ms(90),
        "ENV2 FLT": dep(25), "E2ATK": 0, "E2DEC": ms(250), "E2SUS": pct(30), "E2REL": ms(80),
        "EQHG": -4, "EQHF": -8, "DKSRC": "PUMP", "DKRATE": "1/4", "DKDEP": 14,
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(15)),
        "LEVEL": 98,
    }),
    # ====================================================================================================== SAMPLE (batch C)
    # SAMPLE facts (eng_sample.c): the built-in sets 0 PIANO (5 zones, one-shot), 2 FLUTE and 3 SAX (3 zones each, looped, sustaining).
    # TUNE, BITS (0 clean .. 12 bits removed), LOOP, CUT (low-pass), DRV; FLT moves CUT, SHP moves DRV. USR1 / USR2 are the owner's
    # own samples (nothing in a build, the audit plays the sine fallback there): no factory sound uses them. Morph: SET / LOOP never,
    # BITS stays 0 when 0 (morph.c).
    dict(engine="SAMPLE", idx=0, name="BABY GRAND", cat="KEYS", bseed=52014, recipe="4.1 / 4.4 sampled piano, bright with velocity, short room", p={
        "SET": 0, "TUNE": 0, "BITS": 0, "LOOP": "OFF", "CUT": 112, "DRV": 0,
        "ATK": 0, "SUS": 127, "REL": ms(250),
        "ENV2 FLT": dep(15), "E2ATK": 0, "E2DEC": ms(500), "E2SUS": pct(40), "E2REL": ms(200),
        "REV": pct(22), "CHO": pct(8),
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(30)),
        "LEVEL": 96,
    }),
    dict(engine="SAMPLE", idx=1, name="ATTIC KEYS", cat="KEYS", bseed=12427, recipe="5.5 lofi + honky-tonk upright: the piano crushed (4 bits fewer), dark and dry, a hammer-bright attack that closes at once, sour wobble, wide detuned chorus", p={
        # vs BABY GRAND (clean, open, reverb, sustaining): crushed + driven + dark base cutoff, a fast-closing ENV 2 (felt hammer: bright
        # for ~250 ms, then muffled), a shortish release, an out-of-tune wobble (LFO 2 PIT 2 = +-38 ct at 3.2 Hz) and a heavy CHORUS
        # (the detuned second string of a honky-tonk) with almost no room
        "SET": 0, "TUNE": 0, "BITS": 41, "LOOP": "OFF", "CUT": 50, "DRV": 44,
        "ATK": 0, "SUS": 127, "REL": ms(120),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(3.2), "LFO2 PIT": 2,
        "ENV2 FLT": dep(45), "E2ATK": 0, "E2DEC": ms(220), "E2SUS": 0, "E2REL": ms(100),
        "CHO": pct(70), "REV": pct(6),
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 84,
    }),
    dict(engine="SAMPLE", idx=2, name="TOY PIANO", cat="PLUCK", bseed=5508, recipe="4.4 kalimba / music-box idea: the piano an octave up, crushed, short", p={
        "SET": 0, "TUNE": 12, "BITS": 41, "LOOP": "OFF", "CUT": 100, "DRV": 0,
        "ATK": 0, "DEC": ms(300), "SUS": 0, "REL": ms(120),
        "ENV2 FLT": dep(20), "E2ATK": 0, "E2DEC": ms(250), "E2SUS": 0, "E2REL": ms(100),
        "REV": pct(30), "DLY": pct(15), "EQLG": -4,
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 106,
    }),
    dict(engine="SAMPLE", idx=3, name="TENOR SOLO", cat="LEAD", bseed=65359, mono=True, recipe="2.1 mono lead with glide and delayed vibrato (sampled sax)", p={
        "SET": 3, "TUNE": 0, "BITS": 0, "LOOP": "ON", "CUT": 108, "DRV": 17,
        "ATK": ms(20), "SUS": 127, "REL": ms(160), "GLMODE": "TIME", "GLIDE": gl(70),
        "ENV2 FLT": dep(20), "E2ATK": ms(15), "E2DEC": ms(400), "E2SUS": pct(60), "E2REL": ms(100),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5.5), "LFO2 FADE": ms(400), "LFO2 PIT": 1,
        "MOD1": ("AT", "DEP2", dep(100)), "MOD2": ("VEL", "CUT", dep(20)),
        "DLY": pct(22), "REV": pct(22),
        "LEVEL": 99,
    }),
    dict(engine="SAMPLE", idx=4, name="SILVER FLUTE", cat="LEAD", bseed=34351, mono=True, recipe="2.1 mono lead (sampled flute, breath attack)", p={
        "SET": 2, "TUNE": 0, "BITS": 0, "LOOP": "ON", "CUT": 118, "DRV": 0,
        # a quick breath: the sample's own chiff is the attack (ATK 17 ms, was 45; ENV 2 opens with it), the arp is off
        "ATK": ms(17), "SUS": 127, "REL": ms(180), "GLMODE": "TIME", "GLIDE": gl(40),
        "ARP MODE": "OFF",
        "ENV2 FLT": dep(12), "E2ATK": ms(17), "E2DEC": ms(300), "E2SUS": pct(70), "E2REL": ms(100),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5), "LFO2 FADE": ms(500), "LFO2 PIT": 1,
        "MOD1": ("AT", "DEP2", dep(100)), "MOD2": ("VEL", "CUT", dep(15)),
        "DLY": pct(20), "REV": pct(30), "EQLG": -3,
        # SOFT insert as a limiter (2026-10-12): flattens the crest so LEVEL can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 40, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "LEVEL": 97,
    }),
    # BREATH PAD (SAMPLE, was idx 5) deleted 2026-10-12 (owner); idx must stay contiguous, so BRASS CHOIR, BARREL BASS and DECK WARP moved down to 5, 6, 7
    dict(engine="SAMPLE", idx=5, name="BRASS CHOIR", cat="PAD", bseed=2165, recipe="3.2 warm pad on a looped sax, power chords", p={
        "SET": 3, "TUNE": 0, "BITS": 0, "LOOP": "ON", "CUT": 82, "DRV": 11,
        "ATK": ms(600), "SUS": 127, "REL": ms(1800),
        "ENV2 FLT": dep(25), "E2ATK": ms(800), "E2DEC": ms(2000), "E2SUS": pct(55), "E2REL": ms(1500),
        "CHO": pct(35), "REV": pct(55), "SPRD": pct(30),
        "CHRD": "POW", "VOIC": "OPEN",
        "MOD1": ("AT", "CUT", dep(25)),
        "LEVEL": 88,
    }),
    dict(engine="SAMPLE", idx=6, name="BARREL BASS", cat="BASS", bseed=12156, mono=True, recipe="1.1 / 1.6 bass from the sax an octave down, driven, dark", p={
        "SET": 3, "TUNE": -12, "BITS": 0, "LOOP": "ON", "CUT": 60, "DRV": 41,
        "ATK": ms(3), "DEC": ms(250), "SUS": pct(85), "REL": ms(60),
        "ENV2 FLT": dep(35), "E2ATK": 0, "E2DEC": ms(220), "E2SUS": pct(30), "E2REL": ms(60),
        "EQHG": -4, "EQHF": -8, "DKSRC": "PUMP", "DKRATE": "1/4", "DKDEP": 15,
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(15)),
        "LEVEL": 90,
    }),
    dict(engine="SAMPLE", idx=7, name="DECK WARP", cat="OTHER", bseed=44735, recipe="5.5 lofi drone: the sax two octaves down, 7 bits, slow filter", p={
        "SET": 3, "TUNE": -24, "BITS": 71, "LOOP": "ON", "CUT": 74, "DRV": 31,
        "ATK": ms(300), "SUS": 127, "REL": ms(900),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.35), "LFO2 FLT": dep(30),
        "ENV2 FLT": dep(15), "E2ATK": ms(500), "E2DEC": ms(1500), "E2SUS": pct(60), "E2REL": ms(600),
        "DLY": pct(21), "REV": pct(24),
        "MOD1": ("AT", "CUT", dep(25)),
        # SOFT insert as a limiter (2026-10-12): flattens the crest so LEVEL can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 40, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "LEVEL": 98
    }),
    # ====================================================================================================== SLICE (batch C)
    # SLICE facts (eng_slice.c): the built-in BREAK (one bar of 16ths, kick on 0 2 7 10, snare 4 12 (ghosts 9 14), hats) and PIANO (one
    # note). Slice j of a key = (note - 60 - ROOT + START) mod count: the keyboard's lowest key plays slice START, the audit's C3 (note 48)
    # slice (START - 12) mod count. Slices play at the source pitch times PTCH (no key pitch). USR1 / USR2 = the owner's own loops (not in a
    # build: not used). Morph: SRC / DIV / START / MODE / REV never roll.
    dict(engine="SLICE", idx=0, name="DUSTY KIT", cat="PERC", bseed=64943, recipe="break kit: 16 slices from the low key, crushed and dark", p={
        # audit: C3 = slice 4, the snare
        "SRC": "BREAK", "DIV": "16", "START": 0, "PTCH": 0, "MODE": "ONE", "E6": 0, "DCAY": 127, "TONE": 82,
        "ATK": 0, "SUS": 127, "REL": ms(60),
        "INSERT": "CRUSH", "INS A": 70, "INS B": 100, "INS MIX": pct(25),
        "REVERB": pct(12),
        "MOD1": ("AT", "CUT", dep(25)),
        "LEVEL": 111,
    }),
    dict(engine="SLICE", idx=1, name="HAT TICKS", cat="PERC", bseed=1794, recipe="break kit: 32nds, the hat ticks from the low key up", p={
        # audit: C3 = slice 12 of 32 (the hat on step 6)
        "SRC": "BREAK", "DIV": "32", "START": 24, "PTCH": 0, "MODE": "ONE", "E6": 0, "DCAY": 80, "TONE": 127,
        "ATK": 0, "SUS": 127, "REL": ms(30), "REVERB": pct(10), "EQLG": -6,
        "MOD1": ("AT", "CUT", dep(20)),
        "LEVEL": 127,
    }),
    dict(engine="SLICE", idx=2, name="WOOFER", cat="PERC", bseed=54997, recipe="kick: the break's lone kick (step 7, no hat under it) a fifth down, dark and tight", p={
        "DIST": 7,
        # audit: C3 = slice 0 of the DIV, so START 19 plays slice 7 of 16: the kick on step 7 alone (slice 0 holds the kick WITH the
        # closed hat on the one, which is what made the old WOOFER sound odd)
        "SRC": "BREAK", "DIV": "16", "START": 19, "PTCH": -5, "MODE": "ONE", "E6": 0, "DCAY": 91, "TONE": 71,
        "ATK": 0, "SUS": 127, "REL": ms(60),
        "EQHG": -4, "EQHF": -8,
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(15)),
        "LEVEL": 118,
    }),
    dict(engine="SLICE", idx=3, name="BACKSPIN", cat="FX", bseed=40304, recipe="5.1 riser idea: a reversed half-beat of the break, filter opening", p={
        "DIST": 7,   # own DIST (2026-10-12): SMART now rolls a DIST of 0 upward, and the first step costs ~2 dB; a base of 7 keeps End B in the window
        "SRC": "BREAK", "DIV": "4", "START": 0, "PTCH": -5, "MODE": "ONE", "E6": 1, "DCAY": 127, "TONE": 50,
        "ATK": ms(20), "SUS": 127, "REL": ms(300),
        "ENV2 FLT": 40, "E2ATK": ms(400), "E2DEC": 0, "E2SUS": 127, "E2REL": ms(200),
        "REVERB": pct(45), "DLY": pct(35),
        "MOD1": ("AT", "CUT", dep(25)),
        "LEVEL": 114,
    }),
    dict(engine="SLICE", idx=4, name="SNARE BUZZ", cat="OTHER", bseed=51030, recipe="glitch stutter: a 32nd of the break looped while the key is held", p={
        "DIST": 7,   # own DIST (2026-10-12): SMART now rolls a DIST of 0 upward, and the first step costs ~2 dB; a base of 7 keeps End B in the window
        # audit: C3 = slice 8 of 32 (the snare's start)
        "SRC": "BREAK", "DIV": "32", "START": 20, "PTCH": 0, "MODE": "LOOP", "E6": 0, "DCAY": 127, "TONE": 100,
        "ATK": 0, "SUS": 127, "REL": ms(40),
        "ENV2 FLT": dep(15), "E2ATK": 0, "E2DEC": ms(300), "E2SUS": pct(40), "E2REL": ms(60),
        "DLY": pct(25), "REVERB": pct(15),
        "MOD1": ("AT", "CUT", dep(25)),
        "LEVEL": 109,
    }),
    dict(engine="SLICE", idx=5, name="PIANO SHARD", cat="PLUCK", bseed=54127, recipe="4.6 pluck idea: the head of the piano note, played gated", p={
        # audit: C3 = slice 0 of 4 (the attack); the same pitch on every key
        "SRC": "PIANO", "DIV": "4", "START": 0, "PTCH": 0, "MODE": "GATE", "E6": 0, "DCAY": 100, "TONE": 110,
        "ATK": 0, "SUS": 127, "REL": ms(120),
        "ENV2 FLT": dep(20), "E2ATK": 0, "E2DEC": ms(200), "E2SUS": 0, "E2REL": ms(100),
        "REVERB": pct(35), "DLY": pct(20),
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "LEVEL": 105,
    }),
]

# DUELL 0.2.2 (owner): the LFOs run FREE on every factory sound, both of them, set explicitly (not left to the default,
# params.c P_LTRIG / P_L2TRIG). A FADE restarts with each fresh phrase all the same (voice.c trk_note_on); only the PHASE is
# no longer put back at the key press. A sound that wants the key trigger sets "LFO TRIG": "NOTE" itself.
for _s in SOUNDS:
    _s["p"].setdefault("LFO TRIG", "FREE")
    _s["p"].setdefault("LFO2 TRIG", "FREE")
