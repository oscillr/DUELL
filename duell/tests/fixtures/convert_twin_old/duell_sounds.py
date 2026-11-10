# SPDX-License-Identifier: GPL-3.0-only
"""DUELL factory sounds: the data (tools/gen_duell_sounds.py validates it and writes build/gen/duell_sounds.h).

Every sound: engine, idx (its factory index within the engine: the identity favourites, saved copies (shadows) and the
stable End B seed use; APPEND-ONLY per engine, never renumber), name (<= 12 characters), cat (BASS LEAD PAD PLUCK KEYS
FX OTHER), mono (True: VOICE LEG, as the old presets' flag), recipe (the DUELL/SOUND_RECIPES.md section), and p: the
parameters that differ from INIT (the track defaults, params.c TP, and the engine's EDIT defaults). Names: the
engine's EDIT labels (ANALOG: WAVE DTN MIX NOIS CUT RES DRV KTR), core.h's P_ ids without P_ (ATK DEC SUS REL TRIM
GLIDE GLMODE VOICE DIST CHOR DLY REV TWTYPE E2ATK.. E2FLT L2RATE.. L2PIT SPRD ITYPE IA IB IC IMIX ...) or the aliases
of gen_duell_sounds.py (ENV2 FLT, LFO2 RATE, ARP MODE ...); MOD1..MOD8 = (SRC, DST, AMT) matrix slots.

House rules (owner, 2026-10-11): ENV 2 is the filter envelope (ENV2 FLT / SHP) on every sound with a filter (ANALOG, TRIO, NOISE); on the universal-filter engines (PHASE LOFI VOICE WHEEL PHYS FM6 WAVE) the FLT depths and the matrix's CUT routes start at 0 (0.2.5: with CUT at OFF a route would now filter, and the sounds keep the sound they had); matrix slot 1 =
aftertouch (AT) to what suits the type; slot 2 = velocity -> CUT where the recipe asks for velocity-brightness
(velocity -> level is built in); no MODW routes (the mod wheel moves the MORPH fader); ARP only on arp sounds.
TRIM (the PATCH TRIM, 0.2.19; until 0.2.18 "LEVEL", the one level a part had; the MIXER LEVEL is the player's and no sound sets it): the trim from tests/sound_audit.c (SOUND_TRIM=1): one held C3 near -18 dBFS (ear-weighted) at its sustained level (the
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
the SHP inputs, +-12.5 % of the period at SHP depth 63; LFO 2 PIT 1 = +-3/16 semitone (+-19 cents, the smalles
non-zero vibrato); MIX = OSC 1 / OSC 2 balance (MIX 0: OSC 1 alone); in WAVE SUB, MIX is the sub (a square one octave
below) level. Velocity > 110 adds an accent (voice.c: the AMP ENV opens the filter by 24 steps).
[G] marks a value the recipe only bounds loosely or not at all (DUELL/SOUND_RECIPES.md's own tag).
"""
import math
import zlib


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
        "TRIM": 101,
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
        "TRIM": 92,
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
        "TRIM": 96,
    }),
    dict(engine="ANALOG", idx=3, name="RUBBER BAND", cat="BASS", bseed=49167, mono=True, recipe="1.6 Pluck bass", p={
        # saw with an optional sub [G]: WAVE SUB, sub at ~40 %
        "WAVE": "SUB", "DTN": 0, "MIX": pct(20),
        # cutoff low 20-50, resonance 20-50 [G] -> 35 / 35
        "CUT": 32, "RES": 48,
        # FENV amount 60-100 %, A 0, D 80-250 ms, S 0, R close to D [G]: the whole sound -> 75 %, 150 ms
        "ENV2 FLT": dep(95), "E2ATK": 0, "E2DEC": ms(110), "E2SUS": 0, "E2REL": ms(110),
        # AENV A 0, D 150-400 ms, S 0-20 %, R 50-150 ms [G] -> 380 ms, 32 %, 100 ms (a plucked note that settles; 0.2.20: tighter than the 500 ms / 45 % before, so it parts from LADDER BASS)
        "ATK": 0, "DEC": ms(380), "SUS": pct(32), "REL": ms(100),
        # light drive, the rubbery resonance up, saturation and the house bass shelf, a touch of reverb [G]
        "DRV": 18, "REV": pct(5), "DIST": 22, "EQHG": -4, "EQHF": -8,
        # velocity to the FENV amount [G] -> VEL -> CUT
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(20)),
        "TRIM": 109,
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
        "TRIM": 90,
    }),
    # ------------------------------------------------------------------------------------------------ ANALOG LEAD
    dict(engine="ANALOG", idx=5, name="TORCH SONG", cat="LEAD", bseed=58621, mono=True, recipe="2.5 warm solo lead (Minimoog-style triangle / square 'singing' lead, slow vibrato, hall)", p={
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
        "TRIM": 93,
    }),
    dict(engine="ANALOG", idx=6, name="RAZORWIRE", cat="LEAD", bseed=61032, mono=True, recipe="2.2 Hard-sync lead", p={
        # OSC 2 hard-synced to OSC 1, its pitch = the timbre [S]; follower +7..+24 st [G] -> +7 at rest; mostly the
        # synced oscillator heard [G]
        "WAVE": "SYNC", "DTN": st(7), "MIX": pct(80),
        # the sweep by an envelope, D 300-800 ms [G] -> ENV 2 SHP +75 % (+~9 st at the peak, so +16 st falling to +7)
        "ENV2 SHP": dep(100), "E2ATK": 0, "E2DEC": ms(260), "E2SUS": 0, "E2REL": ms(150),
        # filter fairly open 80-110, resonance low [G] -> 95, 20; a little of ENV 2 on it too [G]
        "CUT": 95, "RES": 20, "ENV2 FLT": dep(10),
        # slow pitch modulation of the sweep, LFO 0.2-1 Hz [S KVR / G rate] -> LFO 2 SHP, triangle 0.5 Hz
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.5), "LFO2 SHP": dep(25),
        # AENV A 0, S 100 %, R 100-200 ms [G] -> 150 ms; portamento (Ultravox) [S] -> 80 ms [G]
        "ATK": 0, "SUS": 127, "REL": ms(150), "GLMODE": "TIME", "GLIDE": gl(80),
        # drive, delay, phaser (flanging in the Ultravox note) [S/G]: DRV, DLY, the INSERT PHASR at half mix
        "DRV": 36, "DLY": pct(14), "INSERT": "PHASR", "INS A": hz(0.5), "INS MIX": pct(40),
        # "sweep the slave by hand" [S] -> aftertouch to SHP (the sync interval); velocity opens the sweep too (a harder key = a wider cry) [G]
        "MOD1": ("AT", "SHP", dep(40)), "MOD2": ("VEL", "SHP", dep(20)),
        "TRIM": 91,
    }),
    dict(engine="ANALOG", idx=7, name="FUNFARE", cat="LEAD", bseed=63704, recipe="2.3 Brassy polysynth lead", p={
        # sawtooths on both oscillators, one detuned 5-12 ct [S] -> 8 c
        "WAVE": "SAW", "DTN": 14, "MIX": pct(50),
        # the filter envelope makes the brass [S]: cutoff low enough for it to work [G] -> 52; resonance 2/10 [S] -> 1.4/10
        "CUT": 56, "RES": pct(14),
        # FENV A 30-120 ms, D 200-500 ms, S 40-60 % [S starting values] -> 60 ms, 350 ms, 50 %; amount "high enough to
        # matter" [S] -> 60 %
        "ENV2 FLT": dep(78), "E2ATK": ms(180), "E2DEC": ms(900), "E2SUS": pct(40), "E2REL": ms(100),
        # AENV A 15-40 ms, S high, R 50-150 ms [G] -> 180 ms (a played swell, not a stab: SFORZANDO is the stab), 100 %, 100 ms
        "ATK": ms(180), "DEC": ms(300), "SUS": 127, "REL": ms(100),
        # delayed vibrato ~5 Hz [S] -> LFO 2 TRI 5 Hz, PIT 1, FADE 400 ms [G delay]
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5), "LFO2 FADE": ms(400), "LFO2 PIT": 1,
        # aftertouch varies brightness in the sustain [S]; velocity sets the FENV amount [S] -> VEL -> CUT
        "MOD1": ("AT", "CUT", dep(30)), "MOD2": ("VEL", "CUT", dep(25)),
        # the polysynth's ensemble: the CHOR insert (a slow chorus on the voice itself, 0.4 Hz) instead of the send, short reverb [G]
        "INSERT": "CHOR", "INS A": hz(0.4), "INS B": 80, "INS C": 20, "INS MIX": pct(55),
        "REV": pct(20),
        "TRIM": 99,
    }),
    dict(engine="ANALOG", idx=8, name="WALL OF SAW", cat="LEAD", bamt=15, bseed=21203, recipe="2.4 Supersaw lead", p={
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
        # SOFT insert as a limiter (2026-10-12): flattens the crest so TRIM can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 80, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "TRIM": 91,
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
        "TRIM": 88,
    }),
    dict(engine="ANALOG", idx=10, name="EMBER GLOW", cat="PAD", bseed=44167, recipe="3.2 Prophet-style warm pad", p={
        # saws on A and B, 5-12 ct apart [S architecture, G cents] -> 9 c
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
        # SOFT insert as a limiter (2026-10-12): flattens the crest so TRIM can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 80, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "TRIM": 91,
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
        "TRIM": 100,
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
        "TRIM": 99,
    }),
    dict(engine="ANALOG", idx=13, name="SFORZANDO", cat="KEYS", bseed=46167, recipe="4.8 Brass stab", p={
        # saw stack, two oscillators, 5-10 ct [G] -> 7 c
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
        "TRIM": 101,
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
        # SOFT insert as a limiter (2026-10-12): flattens the crest so TRIM can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 40, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "TRIM": 93
    }),
    dict(engine="ANALOG", idx=15, name="GLASS SHARDS", cat="OTHER", bseed=17703, recipe="5.2 Metallic glitch arp", p={
        # short square tones [G]
        "WAVE": "SQR", "DTN": 0, "MIX": 0,
        # high-resonance band-pass [G] -> BP, RES 90, CUT 70. (Cloud job B retired ANALOG's filter TYPE switch, LP BP HP: the engine is a low-pass until job J takes its filter
        # out, so this sound plays the low-pass with CUT 70, RES 90 now; it was the only factory ANALOG sound that was not LP. Job J's conversion puts the band-pass back as TWIN FILTER BP)
        "CUT": 70, "RES": 90,
        "ENV2 FLT": dep(30), "E2ATK": 0, "E2DEC": ms(60), "E2SUS": 0, "E2REL": ms(60),
        # D 30-100 ms, S 0 [G] -> 100 ms (the long end: a longer ping is more level for the same peak; this sound's
        # level is set by its peak, the resonant band-pass rings ~21 dB over its RMS: -23 dBFS LOUD at LEVEL 115)
        "ATK": 0, "DEC": ms(220), "SUS": 0, "REL": ms(100),
        # arp 16ths up-down over 2-3 octaves, random gate [G] -> UPDN 1/16, 3 octaves, PROB ~80 %, short gate
        "ARP MODE": "UPDN", "ARP RATE": "1/16", "ARP OCT": 3, "ARP GATE": pct(65), "ARP PROB": pct(80),
        # light bitcrush (INSERT CRUSH, 8 bits = BITS knob at 90 since 0.2.14 (was 60), 11 kHz, half mix) and delay feedback 40-60 % (the global delay's) [G]
        "INSERT": "CRUSH", "INS A": 90, "INS B": 100, "INS MIX": pct(50), "DLY": pct(40),
        # aftertouch moves the band-pass
        "MOD1": ("AT", "CUT", dep(30)),
        "DRV": 127,
        "DIST": pct(25),
        "TRIM": 108,
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
        # 0.2.17: the slap is really ENV 2 on the modulator level now (ENV2 FLT; before the envelope was set but routed nowhere): a short,
        # sharp opening (70 ms to nothing, the modulators +10 over the patch) on a near-sine body, so the click is the sound and FRETLESS
        # (its own patch F11 now) stays round
        "MLVL": 10, "ENV2 FLT": dep(63), "E2ATK": 0, "E2DEC": ms(70), "E2SUS": 0, "E2REL": ms(60),
        # velocity to modulator level (hard hits brighter) [G]
        "VMOD": 3,
        # none / very light chorus; mono [G]; bass: HF-shelf cut, subtle PUMP ducker (house rule)
        "EQHG": -4, "EQHF": -8, "DKSRC": "PUMP", "DKRATE": "1/4", "DKDEP": 16,
        "MEG": 24,
        "DIST": 7,
        "TRIM": 116,
    }),
    dict(engine="FM6", idx=1, name="HAN SOLO", cat="LEAD", bseed=381, mono=True, recipe="2.6 FM lead", p={
        # the BRASS SECT patch (F4) on ALG 16: one carrier (OP 1) fed by three modulator branches, the classic FM brass layout.
        # (Its own algorithm, 5, has three carriers detuned 2 steps apart: they beat once in ~5 s, the level dropped 15 dB and
        # back, the "weird" solo.) Modulator index follows the envelope: the patch's own attack swells the note in, ENV 2 on MLVL
        # opens with it (A 117 ms) and settles at 87 %, darker at low velocity (VMOD) [G; Plugin Boutique "80s Brass" idea]
        "SLOT": "F4", "ALG": 16, "FB": -1, "MEG": 20,
        "MLVL": -1, "E2ATK": ms(117), "E2DEC": ms(717), "E2SUS": pct(87), "E2REL": ms(150),
        "VMOD": 3,
        # light vibrato, delayed (5 Hz, +-19 ct, 400 ms), pressure deepens it; portamento 60 ms [G]
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5), "LFO2 FADE": ms(400), "LFO2 PIT": 1,
        "MOD1": ("AT", "DEP2", dep(100)),
        "GLMODE": "TIME", "GLIDE": gl(60),
        # chorus + delay [G]
        "CHO": pct(28), "DLY": pct(22), "REV": pct(15),
        "DIST": pct(7),
        "TRIM": 117,
    }),
    dict(engine="FM6", idx=2, name="GLASS HALL", cat="PAD", bseed=28240, recipe="3.6 Glass / FM pad", p={
        # the SOFT PAD patch (F5): carrier ratio 1 with long slow attacks, a modulator at a higher ratio, low index [S Yamaha FM
        # 101 pt 5 DX7 string pad; the glassy high ratio is G]: MRAT +2, MEG slower, carriers spread a little (+3 detune in the source)
        "SLOT": "F5", "MRAT": 2, "MEG": 14, "MLVL": -8, "DTUN": pct(30), "FB": 1,
        # the glass opens slowly: ENV 2 on MLVL, A 1.5 s, S 55 % [G]; a slow LFO breathes i
        "E2ATK": ms(1500), "E2DEC": ms(2500), "E2SUS": pct(55), "E2REL": ms(1500),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.3),
        # shimmer reverb, chorus [G]
        "CHO": pct(55), "REV": pct(55),
        # chord keys instead of an arp: a major-seventh stack opened up (1-5-7-3)
        "CHRD": "MAJ7", "VOIC": "OPEN",
        # SOFT insert as a limiter (2026-10-12): flattens the crest so TRIM can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 100, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "TRIM": 94,
    }),
    dict(engine="FM6", idx=3, name="MALLET WOOD", cat="PLUCK", bseed=12489, recipe="4.4 Marimba / kalimba", p={
        # 0.2.20, SOUND_RECIPES 4.4 (marimba): the WOOD BARS patch (F6): a strong fundamental and, at the strike, a bright knock of the modulator
        # (ratio 4: the second mode of a tuned bar) that is gone in ~100 ms while the fundamental rings on. MLVL +14 brings the knock up and
        # MEG +25 lets it last a few tens of ms (before, MLVL -4 left a plain sine and the envelope values set here did nothing: FM6's amplitude
        # is the patch's own); VMOD 4: a harder key = a brighter knock. Room reverb only (a delay smeared the wood) [S/G as 4.4]
        "SLOT": "F6", "VMOD": 4, "MLVL": 14, "MEG": 25,
        "REV": pct(12),
        "TRIM": 121,
    }),
    dict(engine="FM6", idx=4, name="TINE ROOM", cat="KEYS", bseed=14977, recipe="4.1 DX7 electric piano", p={
        # the TINE EP patch (F1): 14:1 tine modulator decaying fast, body pairs, carriers detuned for warmth [S]
        "SLOT": "F1", "DTUN": pct(10),
        # velocity = modulator level is the whole expressiveness (soft = tone bar, hard = barking tine) [S]: VMOD +3, and
        # the tine brighter at the strike through ENV 2 [G]
        "VMOD": 3, "E2ATK": 0, "E2DEC": ms(500), "E2SUS": 0, "E2REL": ms(200),
        # stereo chorus, tremolo / auto-pan 0.5-5 Hz [G] -> LFO 2 on AMP, light reverb
        "LFO2 WAVE": "SIN", "LFO2 RATE": hz(3.5), "MOD3": ("LFO2", "AMP", dep(14)),
        "CHO": pct(40), "REV": pct(22),
        "MEG": 30,
        "TRIM": 111,
    }),
    dict(engine="FM6", idx=5, name="BIG STAB", cat="KEYS", bseed=37822, recipe="4.8 Brass stab", p={
        # the 80s FM version: the modulator's level peaks at the attack and falls quickly, darker at low velocity [S Plugin
        # Boutique]: BRASS SECT (F4), faster modulator times, ENV 2 on MLVL A 5 ms, D 280 ms, S 20 %
        "SLOT": "F4", "MEG": 0, "FB": 1, "MLVL": -8, "VMOD": 4,
        "E2ATK": ms(5), "E2DEC": ms(500), "E2SUS": pct(60), "E2REL": ms(120),
        # chorus + short reverb [G]
        "CHO": pct(25), "REV": pct(20),
        "DIST": pct(20),
        "TRIM": 115,
    }),
    dict(engine="FM6", idx=6, name="FROST BELLS", cat="KEYS", bseed=48288, recipe="4.3 FM bell / glass", p={
        # the GLASS BELL patch (F2): sine carriers with modulators at non-integer ratios, long decay, the modulators decaying
        # faster so it starts bright and ends a pure tone [S]
        "SLOT": "F2", "DTUN": pct(10), "VMOD": 2,
        "E2ATK": 0, "E2DEC": ms(1200), "E2SUS": 0, "E2REL": ms(600),
        # long hall reverb, little chorus [S]; bells: LF-shelf cut (house rule)
        "REV": pct(55), "CHO": pct(10), "EQLG": -5,
        # SOFT insert as a limiter (2026-10-12): flattens the crest so TRIM can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 40, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "TRIM": 106,
    }),
    dict(engine="FM6", idx=7, name="ALIEN CHIMES", cat="FX", bseed=40225, recipe="5.4 Sci-fi FM bell cluster", p={
        # several bell voices with different non-integer ratios: the GLASS BELL patch on another algorithm (ALG 4), modulator
        # ratios up (MRAT +5), carriers spread wide [G]
        "SLOT": "F2", "ALG": 4, "MRAT": 5, "DTUN": pct(55), "FB": 2,
        "E2ATK": 0, "E2DEC": ms(900), "E2SUS": 0, "E2REL": ms(500),
        # random pitch per note: arp RND 8ths over 2 octaves, short gate [G]; a slow LFO moves the brightness
        "ARP MODE": "RND", "ARP RATE": "1/8", "ARP OCT": 2, "ARP GATE": pct(35),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.2),
        # long reverb / ping-pong delay, feedback 40-60 % (global) [G]; LF-shelf cu
        "DLY": pct(50), "REV": pct(60), "EQLG": -6,
        "TRIM": 107,
    }),
    dict(engine="FM6", idx=8, name="TEMPLE BLOCK", cat="OTHER", bseed=28718, recipe="4.4 Marimba / kalimba (percussive)", p={
        # 0.2.20, SOUND_RECIPES 4.4 (percussive): a temple block is a hollow wooden shell: higher than the bars, short, a hollow (odd-harmonic)
        # tone. The WOOD BARS patch with the modulators at ratio 2 (MRAT -2: odd sidebands, hollow) and hard (MLVL +22); played 19 semitones up
        # (EXPR is 127 until a CC11 lowers it, so two routes of +63 and +38 are a constant +18.9 st: C3 sounds as G4; no pitch bend); ENV 2 on
        # AMP closes the note in ~130 ms (the patch's own envelope rings for half a second) [G]
        "SLOT": "F6", "MEG": 0, "MRAT": -2, "MLVL": 22, "VMOD": 3,
        "MOD4": ("EXPR", "PITCH", 63), "MOD5": ("EXPR", "PITCH", 38),
        "E2ATK": 0, "E2DEC": ms(130), "E2SUS": 0, "E2REL": ms(60), "MOD6": ("ENV2", "AMP", 63),
        "REV": pct(20),
        "TRIM": 120,
    }),
    # ====================================================================================================== PHYS (batch A)
    # PHYS facts (eng_phys.c): the model's own decay is the note (SUS 127, REL damps the key-off); FLT (ENV 2 FLT, matrix CUT)
    # moves BRIT, SHP moves STRC; ACC = strike accent at full velocity (velocity scales it); EXC = the exciter itself.
    # WOOD BASS (PHYS, was idx 0) deleted 2026-10-12 (owner); idx must stay contiguous, so PAN WHISTLE .. TABLA TALK moved down by one
    dict(engine="PHYS", idx=0, name="PAN WHISTLE", cat="LEAD", bseed=58847, mono=True, recipe="blown tube (4.4 MODAL, BOW)", p={
        # a blown pipe: the modal bank with harmonic partials (STRC 34), sustained by random hits (BOW) with a breath of the
        # exciter (EXC) [S eng_phys.c]
        "MODEL": "MODAL", "STRC": 34, "BRIT": 62, "DAMP": 100, "POS": 14, "ACC": 110, "BOW": 110, "EXC": 40,
        # the player's breath: A 60 ms, R 160 ms [G]; delayed vibrato 5 Hz [S SOS brass: low, delayed]
        "ATK": ms(60), "SUS": 127, "REL": ms(160),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5), "LFO2 FADE": ms(500), "LFO2 PIT": 1,
        "E2ATK": ms(40), "E2DEC": ms(350), "E2SUS": pct(70), "E2REL": ms(150),
        # pressure = more breath into the pipe (BOW), velocity = brightness
        "MOD1": ("AT", "BOW", dep(15)),
        "GLMODE": "TIME", "GLIDE": gl(40),
        "CHO": pct(15), "DLY": pct(20), "REV": pct(30),
        # SOFT insert as a limiter (2026-10-12): flattens the crest so TRIM can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 80, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "TRIM": 99,
    }),
    dict(engine="PHYS", idx=1, name="SINGING BOWL", cat="PAD", bseed=64412, recipe="3.7 Dark drone / bowed metal", p={
        # a bowed bowl: MODAL, inharmonic (STRC 98: 1 : 3.57 : 7.65), long ring, sustained by the bow [S eng_phys.c]
        "MODEL": "MODAL", "STRC": 92, "BRIT": 66, "DAMP": 92, "POS": 10, "ACC": 80, "BOW": 100, "EXC": 0,
        "ATK": ms(150), "SUS": 127, "REL": ms(1800),
        # the bowing swells brightness and a slow LFO drifts it [G]
        "E2ATK": ms(300), "E2DEC": ms(2500), "E2SUS": pct(70), "E2REL": ms(1500),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.15),
        "CHO": pct(30), "REV": pct(60), "DLY": pct(15),
        # chord keys instead of an arp: bowls ring in open fifths (power chord) (3 voices: PHYS' limit)
        "CHRD": "POW", "VOIC": "OPEN",
        "MOD1": ("AT", "BOW", dep(15)),
        "TRIM": 127,
    }),
    dict(engine="PHYS", idx=2, name="GHOST HARP", cat="PLUCK", bseed=40612, recipe="4.6 String pluck / harp (SYMP)", p={
        # SYMP: a plucked string with three sympathetic strings (resonant strings of a harp via the model, not the reverb)
        # [S eng_phys.c]: CHRD 8, SYMP 70, a touch of the curved bridge
        "MODEL": "SYMP", "E2": 8, "BRIT": 67, "DAMP": 104, "E5": 70, "ACC": 90, "E7": 30,
        "E2ATK": 0, "E2DEC": ms(300), "E2SUS": 0, "E2REL": ms(200),
        "ATK": 0, "SUS": 127, "REL": ms(250),
        "CHO": pct(20), "REV": pct(40),
        "TRIM": 113,   # (0.2.22: 112 -> 113 for the new reverb low cut, owner-approved) 0.2 dB over the trim (BRIT 67 with it): the universal filter (0.2.2) took the CUT routes off PHYS' BRIT; the body read -21.8 and the golden render's DC -430 at 110 (limit 400)
    }),
    dict(engine="PHYS", idx=3, name="NYLON GUT", cat="PLUCK", bseed=53812, recipe="4.6 String pluck (Karplus-Strong)", p={
        # one gut string: STRNG, STRC below the dispersion (a soft, slightly buzzy bridge), pluck position 1/3 [S: a pluck a
        # 1/n of the length cancels the n-th harmonics]
        "MODEL": "STRNG", "STRC": 56, "BRIT": 58, "DAMP": 110, "POS": 40, "ACC": 120, "EXC": 0,
        "E2ATK": 0, "E2DEC": ms(300), "E2SUS": 0, "E2REL": ms(150),
        "ATK": 0, "SUS": 127, "REL": ms(80),
        # arpeggiated picking: UP 16ths over two octaves
        "ARP MODE": "UP", "ARP RATE": "1/16", "ARP OCT": 2, "ARP GATE": pct(85),
        "CHO": pct(18), "REV": pct(25),
        "TRIM": 116,
    }),
    dict(engine="PHYS", idx=4, name="ROSEWOOD", cat="KEYS", bseed=55371, recipe="4.4 Marimba / kalimba (modal)", p={
        # a rosewood marimba bar: STRC 116 = 1 : 4 : 9 (a tuned bar's), a dark hard-mallet strike with a woody knock (EXC), a
        # short ring (DAMP low: the bar dies in a few hundred ms), dry, no motion at all [S eng_phys.c; the factory MARIMBA preset's
        # DAMP 40 / BRIT 38 as the starting point]
        "MODEL": "MODAL", "STRC": 116, "BRIT": 47, "DAMP": 57, "POS": 24, "ACC": 100, "BOW": 0, "EXC": 47,
        "E2ATK": 0, "E2DEC": ms(171), "E2SUS": 0, "E2REL": ms(60),
        "ATK": 0, "SUS": 127, "REL": ms(70),
        "REV": pct(11),
        "TRIM": 125,
    }),
    dict(engine="PHYS", idx=5, name="MOTOR VIBES", cat="KEYS", bseed=34914, recipe="4.7 Vibraphone", p={
        # a tuned metal bar, partials near 1 : 3.9 (STRC 110), a bright soft-mallet strike and a long ring (DAMP 127), the motor's
        # tremolo on the amplitude [G]
        "MODEL": "MODAL", "STRC": 110, "BRIT": 77, "DAMP": 127, "POS": 18, "ACC": 80, "BOW": 0, "EXC": 7,
        "E2ATK": 0, "E2DEC": ms(1700), "E2SUS": 0, "E2REL": ms(400),
        "ATK": 0, "SUS": 127, "REL": ms(400),
        # the motor: tremolo 3-7 Hz, depth 20-50 % [G] -> LFO 2 on AMP; reverb light-medium; LF-shelf cut (high sound)
        "LFO2 WAVE": "SIN", "LFO2 RATE": hz(5.5), "MOD3": ("LFO2", "AMP", dep(27)),
        "REV": pct(30), "DLY": pct(14), "EQLG": -4,
        "TRIM": 124,
    }),
    dict(engine="PHYS", idx=6, name="RAIN CHIMES", cat="FX", bseed=23020, recipe="5.4 Sci-fi bell cluster (MODAL)", p={
        # the bell tree: MODAL stretched 1 : 2.83 : 5.48 : 8.90 (a chime's), bright, long ring [S eng_phys.c]
        "MODEL": "MODAL", "STRC": 70, "BRIT": 100, "DAMP": 120, "POS": 12, "ACC": 100, "BOW": 0, "EXC": 6,
        "E2ATK": 0, "E2DEC": ms(900), "E2SUS": 0, "E2REL": ms(500),
        "ATK": 0, "SUS": 127, "REL": ms(500),
        # random notes of a chord falling like rain: arp RND 8ths over 3 octaves [G]
        "ARP MODE": "RND", "ARP RATE": "1/8", "ARP OCT": 3, "ARP GATE": pct(30), "ARP PROB": pct(85),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.25),
        "DLY": pct(45), "REV": pct(60), "EQLG": -6,
        "TRIM": 103,
    }),
    dict(engine="PHYS", idx=7, name="TABLA TALK", cat="OTHER", bseed=40371, recipe="hand drum (MEMB)", p={
        # MEMB: a loaded head (HARM 112: harmonic overtones, tabla-like), struck off-centre, a small pitch drop [S eng_phys.c]
        "MODEL": "MEMB", "E2": 112, "BRIT": 76, "DAMP": 56, "POS": 44, "ACC": 100, "E7": 14, "EXC": 24,
        "E2ATK": 0, "E2DEC": ms(150), "E2SUS": 0, "E2REL": ms(80),
        "ATK": 0, "SUS": 127, "REL": ms(60),
        "REV": pct(15),
        # pressure = more pitch drop (BEND), velocity = brightness
        "MOD1": ("AT", "E7", dep(20)),
        "TRIM": 111,
    }),
    # ====================================================================================================== WHEEL (batch A)
    # WHEEL facts (eng_wheel.c): an organ gate (ATK 0, SUS 127, short REL), no filter: the timbre is the drawbars; SHP = brightness
    # (the upper bars up, the sub bars down) so ENV 2 SHP / the matrix SHP open it; velocity is ignored on an organ (no VEL routes).
    dict(engine="WHEEL", idx=0, name="PEDAL BASS", cat="BASS", bseed=14321, mono=True, recipe="4.5 Hammond (bass registration)", p={
        "DIST": 30,   # SAT: organ pedals through a tube amp; own DIST (2026-10-12): a base above 0 keeps End B in the window
        # the left-hand registration 858000000, upper bars out [S/G]; key click and a little drive
        "REG": "BASS", "SUB": 0, "BODY": -4, "TOP": -8, "CLICK": 50, "DRV": 30, "ROTR": "OFF",
        # the pedal through a tape machine: a faint wow, a soft saturation, the high-cut [G]
        "INSERT": "TAPE", "INS A": 18, "INS B": 90, "INS C": 40, "INS MIX": 127,
        # the organ gate: A 1-5 ms, S 100 %, R 5-40 ms [G]; a short brightness blip on the key = ENV 2 SHP
        "ATK": ms(2), "DEC": ms(900), "SUS": pct(62), "REL": ms(25),
        "ENV2 SHP": dep(40), "E2ATK": 0, "E2DEC": ms(90), "E2SUS": 0, "E2REL": ms(30),
        # bass: HF-shelf cut, subtle PUMP ducker
        "EQHG": -4, "EQHF": -8, "DKSRC": "PUMP", "DKRATE": "1/4", "DKDEP": 14,
        "MOD1": ("AT", "SHP", dep(20)),
        "TRIM": 110,
    }),
    dict(engine="WHEEL", idx=1, name="SMOKE RING", cat="LEAD", bamt=11, bseed=53342, mono=True, recipe="4.5 Hammond (lead registration)", p={
        # a bright even-bar registration (008888000) played mono, driven; the legato keeps the percussion off [S: single trigger]
        "REG": "BRITE", "BODY": 0, "TOP": 1, "PERC": "OFF", "CLICK": 30, "DRV": 55, "ROTR": "SLOW",
        "ATK": ms(2), "SUS": 127, "REL": ms(40),
        # the scanner vibrato V3 (no rate in the sources [S]) -> 6.5 Hz, light [G]; portamento 50 ms
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(6.5), "LFO2 FADE": ms(250), "LFO2 PIT": 1,
        "GLMODE": "TIME", "GLIDE": gl(50),
        "ENV2 SHP": dep(30), "E2ATK": 0, "E2DEC": ms(400), "E2SUS": pct(40), "E2REL": ms(80),
        "DLY": pct(18), "REV": pct(20),
        "MOD1": ("AT", "SHP", dep(25)),
        "TRIM": 97,
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
        "TRIM": 102,
    }),
    dict(engine="WHEEL", idx=3, name="JAZZ CLUB", cat="KEYS", bseed=57778, recipe="4.5 Hammond (888000000)", p={
        # 888000000, the classic Jimmy Smith sound [S Wikipedia]; fast percussion on the 3rd harmonic, normal, the click
        # a design flaw players liked [S]; the drive mild [S SOS]
        "REG": "3BAR", "PERC": "3RD", "CLICK": 45, "DRV": 12, "ROTR": "SLOW",
        "ATK": ms(2), "SUS": 127, "REL": ms(25),
        "ENV2 SHP": dep(12), "E2ATK": 0, "E2DEC": ms(200), "E2SUS": 0, "E2REL": ms(30),
        # mod wheel = Leslie speed or drive [G] -> aftertouch to the drive
        "MOD1": ("AT", "DRV", dep(30)),
        "REV": pct(15),
        "TRIM": 104,
    }),
    dict(engine="WHEEL", idx=4, name="NIGHT BOX", cat="PLUCK", bseed=32458, recipe="4.5 Hammond (TOPS) as an additive bell", p={
        # the top bars only (800000888) as a simple additive bell tone [S eng_wheel.c], plucked by the gate: D 350 ms, S 0
        "REG": "TOPS", "BODY": -4, "TOP": 2, "PERC": "OFF", "CLICK": 20, "DRV": 0, "ROTR": "OFF",
        "ATK": 0, "DEC": ms(350), "SUS": 0, "REL": ms(180),
        "ENV2 SHP": dep(30), "E2ATK": 0, "E2DEC": ms(250), "E2SUS": 0, "E2REL": ms(100),
        # a little winding-up arp (UP 8ths over 2 octaves); high sound: LF-shelf cu
        "ARP MODE": "UP", "ARP RATE": "1/8", "ARP OCT": 2, "ARP GATE": pct(50),
        "REV": pct(40), "DLY": pct(20), "EQLG": -6,
        "MOD1": ("AT", "SHP", dep(25)),
        "TRIM": 108,
    }),
    dict(engine="WHEEL", idx=5, name="LESLIE STORM", cat="FX", bseed=30012, recipe="4.5 Hammond (Leslie, drive) as a swell", p={
        # every bar, driven hard, the rotor fast: an organ turned into weather [G]
        "REG": "FULL", "PERC": "OFF", "CLICK": 0, "DRV": 90, "ROTR": "FAST",
        "ATK": ms(1000), "SUS": 127, "REL": ms(1800),
        "ENV2 SHP": dep(50), "E2ATK": ms(1200), "E2DEC": 0, "E2SUS": 127, "E2REL": ms(800),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.12), "LFO2 SHP": dep(30),
        "DIST": pct(15), "DLY": pct(18), "REV": pct(33),
        "MOD1": ("AT", "SHP", dep(30)),
        # SOFT insert as a limiter (2026-10-12): flattens the crest so TRIM can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 40, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "TRIM": 101
    }),
    dict(engine="WHEEL", idx=6, name="CARNIVAL", cat="OTHER", bseed=13635, recipe="4.5 Hammond (reed registration), arp", p={
        # REED (008765432, brassy slope) with fast 2nd percussion and key click, played as a fairground arp [G]
        "REG": "REED", "PERC": "2ND", "CLICK": 60, "DRV": 25, "ROTR": "FAST",
        "ATK": ms(2), "SUS": 127, "REL": ms(30),
        "ARP MODE": "UPDN", "ARP RATE": "1/8", "ARP OCT": 2, "ARP GATE": pct(55), "ARP PROB": pct(90),
        "ENV2 SHP": dep(15), "E2ATK": 0, "E2DEC": ms(180), "E2SUS": 0, "E2REL": ms(30),
        "DLY": pct(30), "REV": pct(20),
        "MOD1": ("AT", "SHP", dep(20)),
        "TRIM": 100,
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
        "TRIM": 101,
    }),
    dict(engine="TRIO", idx=1, name="BANSHEE", cat="LEAD", bseed=16225, mono=True, recipe="2.2 Hard-sync lead (SYNC LEAD preset)", p={
        # saw osc 2 hard-synced to osc 1, 9 semitones up: its pitch is the timbre [S]; the sweep = ENV 2 on INT2 (stepped
        # semitones, chip-like): +9 falling back to rest within ~450 ms [G]
        "WAVE": "SYNC", "INT2": 12, "INT3": 0, "DTN": 0, "MODE": "LP", "CUT": 66, "RES": 30,
        "ENV2 FLT": dep(15), "E2ATK": ms(220), "E2DEC": ms(700), "E2SUS": pct(30), "E2REL": ms(150),
        "MOD3": ("ENV2", "INT2", dep(26)),
        # AENV A 0, S 100 %, R 100-200 ms [G]; portamento (Ultravox) [S] ~70 ms [G]
        "ATK": ms(320), "SUS": 127, "REL": ms(140), "GLMODE": "TIME", "GLIDE": gl(70),
        # delayed vibrato 5.5 Hz, +-19 ct, onset 350 ms; pressure opens it [S SOS brass: low, delayed]
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5.5), "LFO2 FADE": ms(350), "LFO2 PIT": 1,
        "MOD1": ("AT", "DEP2", dep(100)), "MOD2": ("VEL", "CUT", dep(15)),
        # the wail: a slow FLANG sweep (jet-like, 0.3 Hz) instead of the plain delay [G]
        "INSERT": "FLANG", "INS A": hz(0.3), "INS B": 60, "INS C": 40, "INS MIX": pct(45),
        "DLY": pct(10), "REV": pct(22),
        "TRIM": 97,
    }),
    dict(engine="TRIO", idx=2, name="CHIP CHAPEL", cat="PAD", bseed=18034, recipe="3.3 PWM string pad + CHIP CHOIR preset", p={
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
        "TRIM": 96,
    }),
    dict(engine="TRIO", idx=3, name="TIN TOLL", cat="KEYS", bseed=43504, recipe="4.3 Bell (ring-mod alternative) + RING BELL preset", p={
        # triangle ring-modulated by a triangle 17 semitones up (ratio ~2.7: inharmonic sum / difference partials) [S Nord: incommensurate
        # frequencies multiplied]; band-pass high, little resonance [preset 96 / 30 -> 100 / 28]
        "WAVE": "RING3", "INT2": 0, "INT3": 17, "DTN": 7, "MODE": "LP", "CUT": 108, "RES": 22,
        # struck: A 0, long decay, S 0, release ~ 0.8 s; the partials die as the cutoff closes (ENV 2)
        "ATK": 0, "DEC": ms(1300), "SUS": 0, "REL": ms(800),
        "ENV2 FLT": dep(25), "E2ATK": 0, "E2DEC": ms(500), "E2SUS": 0, "E2REL": ms(300),
        # long hall, little chorus; bells: LF-shelf cu
        "REV": pct(50), "CHO": pct(10), "EQLG": -5,
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "TRIM": 93,
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
        "TRIM": 102,
    }),
    dict(engine="TRIO", idx=5, name="CIRCUIT BEND", cat="FX", bseed=34291, recipe="5.2 Metallic glitch arp", p={
        # ring (triangle) + sync (saw): short metallic tones [G]; low-pass with resonance
        "WAVE": "R+S", "INT2": 7, "INT3": 0, "DTN": 9, "MODE": "LP", "CUT": 92, "RES": 45,
        "ATK": 0, "DEC": ms(220), "SUS": pct(35), "REL": ms(80),
        "ENV2 FLT": dep(30), "E2ATK": 0, "E2DEC": ms(110), "E2SUS": 0, "E2REL": ms(50),
        # arp 16ths up-down over three octaves, random gate; a slow LFO sweeps the band
        "ARP MODE": "UPDN", "ARP RATE": "1/16", "ARP OCT": 3, "ARP GATE": pct(60), "ARP PROB": pct(85),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.3), "LFO2 FLT": dep(30),
        # delay with feedback 40-60 % (global) [G]; LF-shelf cu
        "DLY": pct(45), "REV": pct(35), "EQLG": -4,
        "MOD1": ("AT", "CUT", dep(25)),
        "DIST": pct(25),
        "TRIM": 107,
    }),
    dict(engine="TRIO", idx=6, name="GRAVEL SNARE", cat="OTHER", bseed=20254, recipe="noise percussion (P+N: pulse tone + pitched noise)", p={
        # a pulse for the body and pitched noise (osc 3 two octaves up: the noise rate follows the pitch) for the rattle [G]
        "WAVE": "P+N", "INT2": 0, "INT3": 24, "DTN": 0, "MODE": "LP", "CUT": 100, "RES": 30, "PW": 50,
        "ATK": 0, "DEC": ms(170), "SUS": 0, "REL": ms(110),
        "ENV2 FLT": dep(40), "E2ATK": 0, "E2DEC": ms(130), "E2SUS": 0, "E2REL": ms(70),
        "REV": pct(18),
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "TRIM": 104,
    }),
    # ======================================================================================================== PHASE (batch B)
    # PHASE facts (eng_phase.c): phase distortion, no filter: DCW (the bend) is the brightness. FLT (ENV 2 FLT, the matrix CUT) and SHP both
    # add to DCW; ENV = how much of the AMP envelope (ENV 1) is added to DCW (so the brightness follows the note's loudness: a
    # sustaining note stays bright by ENV x SUS); the R waves (RSAW RTRI RTRP) are the "resonant" ones (a peak sweeping with DCW);
    # WAVE2 alternates with WAVE every other cycle; DTN = a second line (ct), LINE MIX / RING, SUB = sine an octave down.
    dict(engine="PHASE", idx=0, name="ELASTIC BASS", cat="BASS", bamt=11, bseed=60775, mono=True, recipe="1.2 TB-303 acid bass (resonant wave) + RESO preset", p={
        # the resonant sawtooth: the peak sweeps down as ENV falls (the squelch); a sine sub under it [G]
        "WAVE": "RSAW", "WAVE2": "-", "DCW": 38, "ENV": 66, "DTN": 0, "LINE": "MIX", "SUB": pct(45),
        "E2ATK": 0, "E2DEC": ms(190), "E2SUS": pct(10), "E2REL": ms(50),
        "ATK": ms(2), "DEC": ms(250), "SUS": pct(55), "REL": ms(55),
        "GLMODE": "TIME", "GLIDE": gl(40), "DIST": pct(10),
        "EQHG": -4, "EQHF": -8, "DKSRC": "PUMP", "DKRATE": "1/4", "DKDEP": 14,
        "TRIM": 110,
    }),
    dict(engine="PHASE", idx=1, name="BRAZEN", cat="LEAD", bseed=1618, mono=True, recipe="2.3 Brassy polysynth lead + BRASS preset", p={
        # sawtooth bend; the amp envelope opens DCW (ENV 88): brightness swells with the note, as a brass filter envelope [S SOS brass]
        "WAVE": "SAW", "WAVE2": "-", "DCW": 28, "ENV": 88, "DTN": 7, "LINE": "MIX", "SUB": 0,
        "ATK": ms(22), "DEC": ms(300), "SUS": pct(85), "REL": ms(120),
        "E2ATK": ms(40), "E2DEC": ms(350), "E2SUS": pct(50), "E2REL": ms(100),
        "GLMODE": "TIME", "GLIDE": gl(60),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5), "LFO2 FADE": ms(400), "LFO2 PIT": 1,
        "MOD1": ("AT", "DEP2", dep(100)),
        "CHO": pct(22), "DLY": pct(18), "REV": pct(18),
        "TRIM": 93,
    }),
    dict(engine="PHASE", idx=2, name="LAMPLIGHT", cat="PAD", bseed=62877, recipe="3.3 PWM string pad (phase-distortion strings) + STRING preset", p={
        # saw with a double-sine WAVE2 every other cycle, the second line 13 ct off: a built-in ensemble; a soft sub [Felucca STRING]
        "WAVE": "SAW", "WAVE2": "DSIN", "DCW": 46, "ENV": 36, "DTN": 13, "LINE": "MIX", "SUB": pct(15),
        "ATK": ms(800), "DEC": ms(1800), "SUS": pct(92), "REL": ms(1600),
        "E2ATK": ms(1100), "E2DEC": ms(2000), "E2SUS": pct(60), "E2REL": ms(1200),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.35),
        "CHO": pct(55), "REV": pct(33), "SPRD": pct(30),
        # chord keys instead of an arp: diatonic sevenths (the instrument's scale)
        "CHRD": "DIA7", "VOIC": "CLOSE",
        # SOFT insert as a limiter (2026-10-12): flattens the crest so TRIM can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 70, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "TRIM": 85
    }),
    dict(engine="PHASE", idx=3, name="WIRE HARP", cat="PLUCK", bseed=12692, recipe="4.6 String pluck / harp + WIRE preset", p={
        # soft double sine with the resonant triangle on every other cycle; ENV 70 so the bend (the brightness) dies with the note: a
        # pluck's decaying upper partials [S Karplus-Strong logic]
        "WAVE": "DSIN", "WAVE2": "RTRI", "DCW": 40, "ENV": 70, "DTN": 7, "LINE": "MIX", "SUB": pct(10),
        "ATK": 0, "DEC": ms(500), "SUS": pct(30), "REL": ms(160),
        "E2ATK": 0, "E2DEC": ms(240), "E2SUS": 0, "E2REL": ms(120),
        "ARP MODE": "UP", "ARP RATE": "1/8", "ARP OCT": 2, "ARP GATE": pct(60),
        "CHO": pct(20), "REV": pct(40), "DLY": pct(20),
        "TRIM": 98,
    }),
    dict(engine="PHASE", idx=4, name="CLAVICLE", cat="KEYS", bseed=5811, recipe="4.2 Clavinet-like (pulse wave opened by a short envelope)", p={
        # the bent square (a reedy wave; the PLS pulse leaves a DC offset the regress health check rejects) opened by the amp envelope's attack [S: envelope on the wave]; no second line
        "WAVE": "SQR", "WAVE2": "-", "DCW": 50, "ENV": 61, "DTN": 0, "LINE": "MIX", "SUB": 0,
        # AENV A 0, D 150-400 ms, S 0-10 %, R 30-80 ms (damper) [G]
        "ATK": 0, "DEC": ms(230), "SUS": pct(18), "REL": ms(50),
        "E2ATK": 0, "E2DEC": ms(120), "E2SUS": 0, "E2REL": ms(40),
        # velocity to the bend is the clav's expression [G]; light chorus
        "CHO": pct(12), "REV": pct(12),
        "TRIM": 105,
    }),
    dict(engine="PHASE", idx=5, name="CRYSTAL CAVE", cat="FX", bseed=36122, recipe="5.4 Sci-fi bell cluster + BELL preset", p={
        # the resonant triangle and resonant trapezoid ring-modulated (second line 53 ct off): inharmonic glassy tones [G]
        "WAVE": "RTRI", "WAVE2": "RTRP", "DCW": 72, "ENV": 50, "DTN": 53, "LINE": "RING", "SUB": 0,
        "ATK": 0, "DEC": ms(1500), "SUS": pct(25), "REL": ms(500),
        "E2ATK": 0, "E2DEC": ms(600), "E2SUS": 0, "E2REL": ms(300),
        # random notes falling like drops: arp RND 8ths over three octaves; a slow LFO moves the bend
        "ARP MODE": "RND", "ARP RATE": "1/8", "ARP OCT": 3, "ARP GATE": pct(35), "ARP PROB": pct(85),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.2),
        "DLY": pct(50), "REV": pct(64), "EQLG": -6,   # 0.2.22: 60 -> 64 % for the new reverb low cut (owner-approved: the body was 0.1 dB under the MED floor)
        "TRIM": 118,
    }),
    # ======================================================================================================== LOFI (batch B)
    # LOFI facts (eng_lofi.c): a chip voice. CHIP 4BIT / 4B/2 (half rate) / 8BIT / 1BIT / STEP (a 4-bit stepped envelope replaces the
    # ADSR curve: held at full, only REL ends it; CRSH is then its shape), WAVE PLS TRI SAW NOIS WRAM (DUTY / 8 picks one of 16 tables),
    # SWP = a pitch slide down that never stops (p / 8 steps a control tick: 8..15 = 1, ~86 semitones a second), VIB = LFO-free vibrato,
    # ARP = the chip's own fast arp (OCT MAJ MIN); TONE = the low-pass (FLT only opens it). Morph: SWP / VIB are pitch items (a factory End B
    # leaves them), CHIP / DUTY / ARP never roll, WAVE only PLS / TRI / SAW: no cliff to tag.
    dict(engine="LOFI", idx=0, name="CARTRIDGE", cat="BASS", bseed=28474, mono=True, recipe="1.1 Minimoog-style bass (as a Game Boy wave bass) + WAVE BASS preset", p={
        # wave RAM BAS2 (DUTY 64..71: table 8, 1 + 2 + 3 harmonics) in the 4-bit chip: the console's wave bass [G]
        "CHIP": "4BIT", "WAVE": "WRAM", "DUTY": 67, "CRSH": 0, "SWP": 0, "VIB": 0, "ARP": "OFF", "TONE": 72,
        "ATK": ms(2), "DEC": ms(200), "SUS": pct(80), "REL": ms(40),
        "E2ATK": 0, "E2DEC": ms(150), "E2SUS": 0, "E2REL": ms(40),
        "EQHG": -4, "EQHF": -8, "DKSRC": "PUMP", "DKRATE": "1/4", "DKDEP": 15,
        "TRIM": 99,
    }),
    dict(engine="LOFI", idx=1, name="PIXEL HERO", cat="LEAD", bamt=21, bseed=16641, mono=True, recipe="2.5 Chip / 8-bit lead + STEP LEAD preset", p={
        # the console's stepped chip at constant volume (CRSH 0: the envelope held): 25 % pulse (DUTY 49), S high [S 2A03]
        "CHIP": "STEP", "WAVE": "PLS", "DUTY": 49, "CRSH": 0, "SWP": 0, "VIB": 0, "ARP": "OFF", "TONE": 110,
        "ATK": 0, "SUS": 127, "REL": ms(30),
        # delayed vibrato 6 Hz +-19 ct, onset 300 ms [G]; pressure opens it; the width moves a little at the start (ENV 2 SHP)
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(6), "LFO2 FADE": ms(300), "LFO2 PIT": 1,
        "E2ATK": 0, "E2DEC": ms(300), "E2SUS": 0, "E2REL": ms(60), "ENV2 SHP": dep(18),
        "GLMODE": "TIME", "GLIDE": gl(35),
        "DLY": pct(22), "REV": pct(12),
        "MOD1": ("AT", "DEP2", dep(100)),
        "TRIM": 91,
    }),
    dict(engine="LOFI", idx=2, name="WOW FLUTTER", cat="PAD", bseed=65413, recipe="3.5 Choir / vowel pad (a 4-bit tape choir) + 5.5 tape-style wow", p={
        # wave RAM VOXA ("ah", DUTY 92: table 11) at half rate, a slow wow on the pitch; dark TONE [Felucca WAVE LEAD uses VOXA]
        "CHIP": "4B/2", "WAVE": "WRAM", "DUTY": 92, "CRSH": 0, "SWP": 0, "VIB": 0, "ARP": "OFF", "TONE": 52,
        "ATK": ms(700), "DEC": ms(1500), "SUS": pct(92), "REL": ms(1400),
        "E2ATK": ms(1100), "E2DEC": ms(2000), "E2SUS": pct(60), "E2REL": ms(1200),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.3),
        # the tape itself: the TAPE insert's WOW (a slow random wandering of the pitch, 0.5 .. 2 Hz) at a moderate depth with its fixed
        # flutter, a soft saturation and the high-cut [S SOUND_RECIPES 5.5 / 5.6; depth G]; the chip's own VIB is off (the wow is the tape's)
        "INSERT": "TAPE", "INS A": 52, "INS B": 40, "INS C": 78, "INS MIX": 127,
        "CHO": pct(50), "REV": pct(55), "SPRD": pct(30),
        # chord keys instead of an arp: minor sevenths in the natural minor
        "CHRD": "MIN7", "VOIC": "OPEN",
        "TRIM": 102,
    }),
    dict(engine="LOFI", idx=3, name="MARBLE RUN", cat="PLUCK", bseed=60893, recipe="4.4 Marimba / kalimba + ARP 8BIT preset", p={
        # a 4-bit triangle, short and round, dark TONE closing with the note [G]
        "CHIP": "4BIT", "WAVE": "TRI", "DUTY": 64, "CRSH": 0, "SWP": 0, "VIB": 0, "ARP": "OFF", "TONE": 96,
        "ATK": 0, "DEC": ms(300), "SUS": 0, "REL": ms(150),
        "E2ATK": 0, "E2DEC": ms(220), "E2SUS": 0, "E2REL": ms(100),
        "ARP MODE": "UP", "ARP RATE": "1/8", "ARP OCT": 2, "ARP GATE": pct(45),
        "REV": pct(35), "DLY": pct(30), "EQLG": -3,
        "TRIM": 112,
    }),
    dict(engine="LOFI", idx=4, name="DIVE BOMB", cat="FX", bseed=27733, recipe="5.1 Noise sweep / riser (inverted: a pitch fall) with SWP", p={
        # the pitch falls at once and never stops (SWP 9: the slowest, ~86 semitones a second): a siren dropping out of the sky
        "CHIP": "4BIT", "WAVE": "PLS", "DUTY": 40, "CRSH": 0, "SWP": 5, "VIB": 0, "ARP": "OFF", "TONE": 100,
        "ATK": ms(30), "DEC": ms(10000), "SUS": 127, "REL": ms(300),
        "E2ATK": 0, "E2DEC": ms(800), "E2SUS": pct(50), "E2REL": ms(200),
        "LFO2 WAVE": "SAW", "LFO2 RATE": hz(8), "LFO2 SHP": dep(30),
        "DLY": pct(45), "REV": pct(45),
        "TRIM": 91,
    }),
    dict(engine="LOFI", idx=5, name="DEAD BATTERY", cat="OTHER", bseed=16860, recipe="5.5 Bitcrushed lofi arp", p={
        # saw in the 4-bit chip, sample-rate reduced (CRSH 52: every 7th sample held), wow VIB 10, low-passed [G: duty / bits 4-8, rate
        # ~8-16 kHz, tape wow]
        "CHIP": "4BIT", "WAVE": "SAW", "DUTY": 64, "CRSH": 52, "SWP": 0, "VIB": 10, "ARP": "OFF", "TONE": 78,
        "ATK": 0, "DEC": ms(140), "SUS": pct(15), "REL": ms(90),
        "E2ATK": 0, "E2DEC": ms(120), "E2SUS": 0, "E2REL": ms(60),
        "ARP MODE": "UPDN", "ARP RATE": "1/16", "ARP OCT": 2, "ARP GATE": pct(50), "ARP PROB": pct(90),
        "DLY": pct(40), "REV": pct(30),
        "TRIM": 109,
    }),
    dict(engine="LOFI", idx=6, name="PIXEL KICK", cat="OTHER", bseed=33683, recipe="chip percussion (SWP: the pitch fall of a kick)", p={
        # a triangle that falls ~260 semitones a second (SWP 16: 2 steps a tick): the chip kick [G]
        "CHIP": "4BIT", "WAVE": "TRI", "DUTY": 64, "CRSH": 0, "SWP": 16, "VIB": 0, "ARP": "OFF", "TONE": 90,
        "ATK": 0, "DEC": ms(180), "SUS": 0, "REL": ms(80), "DIST": pct(15),
        "E2ATK": 0, "E2DEC": ms(90), "E2SUS": 0, "E2REL": ms(40),
        "REV": pct(8),
        # pressure = a deeper fall; velocity = brightness
        "MOD1": ("AT", "SWP", dep(20)),
        "TRIM": 118,
    }),
    # ======================================================================================================== VOICE (batch B)
    # VOICE facts (eng_formant.c): a Klatt formant synth, 4 voices. VOWL / VOWL2 = the two vowels (0 A, 32 E, 63 I, 95 O, 127 U); TALK = how
    # fast a note glides from VOWL to VOWL2 (0: stays on VOWL); SHIFT = formants in semitones; BUZZ = glottal tilt (brightness; velocity
    # adds to it already: no VEL route); BRTH = breath noise (the top fades the voicing: whisper); Q = bandwidth (and level); RAND = a random
    # vowel per note. FLT (ENV 2 FLT, the matrix CUT) moves BOTH vowels along the A..U axis (it is "wow", not a cutoff); SHP = BUZZ.
    # Morph: BRTH is capped at 100, TALK is logarithmic: no cliff to tag.
    dict(engine="VOICE", idx=0, name="BULLFROG", cat="BASS", bseed=36283, mono=True, recipe="1.4 slap / wow bass (formant) + WOW BASS preset", p={
        # an "oh" opening to "ah" (TALK 62) with a strong buzz: the wow bass [Felucca WOW BASS: BUZZ 100 / Q 80 -> 96 / 78]
        "VOWL": 95, "VOWL2": 6, "TALK": 62, "SHIFT": -4, "BUZZ": 96, "BRTH": 0, "Q": 84, "RAND": 0, "DIST": 24,
        # each note throws the vowels 18 steps toward U and lets them fall back (ENV 2 on FLT): the croak; formants four semitones down (a big frog)
        "E2ATK": 0, "E2DEC": ms(250), "E2SUS": 0, "E2REL": ms(80),
        "ATK": ms(3), "DEC": ms(300), "SUS": pct(80), "REL": ms(70),
        "GLMODE": "TIME", "GLIDE": gl(50),
        "EQHG": -4, "EQHF": -8, "DKSRC": "PUMP", "DKRATE": "1/4", "DKDEP": 15,
        "MOD1": ("AT", "SHP", dep(25)),
        "TRIM": 94,
    }),
    dict(engine="VOICE", idx=1, name="CROONER", cat="LEAD", bamt=11, bseed=2805, mono=True, recipe="2.7 Formant / vocal lead + VOX LEAD preset", p={
        # an "eh" closing toward "ah" at each note start (TALK 44), a slightly raised formant set (SHIFT 2), breath 9 [Felucca VOX LEAD]
        "VOWL": 22, "VOWL2": 0, "TALK": 44, "SHIFT": 2, "BUZZ": 88, "BRTH": 9, "Q": 70, "RAND": 0,
        "E2ATK": 0, "E2DEC": ms(300), "E2SUS": 0, "E2REL": ms(100),
        "ATK": ms(12), "SUS": 127, "REL": ms(110),
        # vibrato 5.5 Hz, +-19 ct, onset 450 ms; glide on [G]; pressure deepens the vibrato (velocity already brightens: BUZZ)
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5.5), "LFO2 FADE": ms(450), "LFO2 PIT": 1,
        "GLMODE": "TIME", "GLIDE": gl(55),
        "MOD1": ("AT", "DEP2", dep(100)),
        "CHO": pct(18), "DLY": pct(22), "REV": pct(30),
        "TRIM": 87,
    }),
    dict(engine="VOICE", idx=2, name="VESPERS", cat="PAD", bseed=34110, recipe="3.5 Choir / vowel pad + CHOIR AAH preset", p={
        # "ah" drifting toward "oh" over the note, a slight random vowel per note (RAND), breath at low level [S/G 3.5]
        "VOWL": 0, "VOWL2": 95, "TALK": 20, "SHIFT": 0, "BUZZ": 42, "BRTH": 21, "Q": 58, "RAND": 14,
        "ATK": ms(700), "DEC": ms(1500), "SUS": pct(92), "REL": ms(1400),
        "E2ATK": ms(1400), "E2DEC": ms(2000), "E2SUS": pct(60), "E2REL": ms(1000),
        # vibrato 5 Hz with a long delay [G: depth 5-12 ct; the smallest step is 19 ct]
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5), "LFO2 FADE": ms(900), "LFO2 PIT": 1,
        "CHO": pct(45), "REV": pct(70), "SPRD": pct(40),
        # chord keys instead of an arp: a diatonic triad (3 of the 4 voices) opened up (the instrument's scale)
        "CHRD": "DIA3", "VOIC": "OPEN",
        "MOD1": ("AT", "SHP", dep(30)),
        "TRIM": 86,
    }),
    dict(engine="VOICE", idx=3, name="DOO WOP", cat="PLUCK", bseed=29492, recipe="4.4 Marimba / kalimba (a vocal pluck) + 5.5 arp", p={
        # a short "oh" snapping to "ah" (TALK 88), narrow bandwidth (Q 92) so it rings like a bottle [G]
        "VOWL": 95, "VOWL2": 0, "TALK": 88, "SHIFT": 0, "BUZZ": 70, "BRTH": 12, "Q": 92, "RAND": 0,
        "ATK": 0, "DEC": ms(220), "SUS": 0, "REL": ms(150),
        "E2ATK": 0, "E2DEC": ms(180), "E2SUS": 0, "E2REL": ms(100),
        "ARP MODE": "UPDN", "ARP RATE": "1/8", "ARP OCT": 2, "ARP GATE": pct(55),
        "CHO": pct(18), "REV": pct(35), "DLY": pct(22),
        "MOD1": ("AT", "SHP", dep(25)),
        "TRIM": 103,
    }),
    dict(engine="VOICE", idx=4, name="GHOST VOICES", cat="FX", bseed=15764, recipe="5.3 Wind / texture (breath) + WHISPER preset", p={
        # nearly all breath (BRTH 118: the voicing fades), the vowel drifting "ah" -> between "i" and "o" (TALK 70) with a random vowel per
        # note; shifted up 3 semitones [Felucca WHISPER]
        "VOWL": 0, "VOWL2": 100, "TALK": 70, "SHIFT": 3, "BUZZ": 52, "BRTH": 118, "Q": 50, "RAND": 40,
        "ATK": ms(1200), "DEC": ms(1500), "SUS": 127, "REL": ms(1800),
        # a very slow LFO moves both vowels along the axis (0.1 Hz)
        "E2ATK": ms(1500), "E2DEC": ms(2500), "E2SUS": pct(70), "E2REL": ms(1500),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.1),
        "DLY": pct(24), "REV": pct(45), "CHO": pct(30),
        "MOD1": ("AT", "SHP", dep(25)),
        # SOFT insert as a limiter (2026-10-12): flattens the crest so TRIM can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 70, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "TRIM": 90
    }),
    dict(engine="VOICE", idx=5, name="VOCAL BEATS", cat="OTHER", bseed=31759, recipe="vocal percussion (breath + random vowel)", p={
        # a whispered burst (BRTH 100) with a random vowel per note (RAND 70 %), narrow bands (Q 110), cut short [G]
        "VOWL": 0, "VOWL2": 127, "TALK": 100, "SHIFT": 0, "BUZZ": 30, "BRTH": 100, "Q": 110, "RAND": pct(70),
        "ATK": 0, "DEC": ms(130), "SUS": 0, "REL": ms(80),
        "E2ATK": 0, "E2DEC": ms(110), "E2SUS": 0, "E2REL": ms(50),
        "REV": pct(15),
        "MOD1": ("AT", "SHP", dep(25)),
        "TRIM": 119,
    }),

    # ====================================================================================================== NOISE (batch C)
    # NOISE facts (eng_noise.c): ANLG / DUST / LFSR / META source (COLR tilt) into one SVF (FREQ, RES: LP -> narrow BP), TRK = FREQ follows
    # the key, DRFT a slow wander. FLT (ENV 2 FLT, matrix CUT) moves FREQ, SHP moves COLR. META LEN = DENS knob >> 4 (0..7), LFSR CLK = CRSH.
    # Morph: MODE / LEN never roll, CLK is a pitch item; DENS and CRSH of ANLG / DUST stay at 0 when 0 (morph.c: a noise that was clean
    # does not start to crackle or crush).
    dict(engine="NOISE", idx=0, name="GEARBOX", cat="BASS", bseed=14165, mono=True, recipe="1.1 Minimoog-style bass envelope on a META register (chip noise-channel bass, 2.5)", p={
        # the 63-step register (LEN 3) at the key's pitch: a rough, hollow buzz; low-passed, FREQ following the key [G]
        "MODE": "META", "COLR": 0, "FREQ": 52, "RES": 28, "TRK": 127, "DENS": 48, "CRSH": 0,
        "ENV2 FLT": dep(40), "E2ATK": ms(4), "E2DEC": ms(240), "E2SUS": pct(25), "E2REL": ms(60),
        "ATK": ms(2), "DEC": ms(200), "SUS": pct(85), "REL": ms(55),
        "EQHG": -4, "EQHF": -8, "DKSRC": "PUMP", "DKRATE": "1/4", "DKDEP": 15,
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(15)),
        "TRIM": 95,
    }),
    dict(engine="NOISE", idx=1, name="HORNET", cat="LEAD", bamt=21, bseed=33927, mono=True, recipe="2.5 Chip lead on an LFSR (the 127-step register, 7 octaves up)", p={
        # ARCADE's register clocked 84 st above the key: one period per key period, a pitched buzz; opened a little [S eng_noise.c]
        "MODE": "LFSR", "COLR": 0, "FREQ": 104, "RES": 22, "TRK": 127, "DENS": 112, "CRSH": 84,
        "ATK": ms(3), "SUS": 127, "REL": ms(70), "GLMODE": "TIME", "GLIDE": gl(45),
        "ENV2 FLT": dep(20), "E2ATK": 0, "E2DEC": ms(300), "E2SUS": pct(50), "E2REL": ms(80),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(6), "LFO2 FADE": ms(300), "LFO2 PIT": 1,
        "MOD1": ("AT", "DEP2", dep(100)), "MOD2": ("VEL", "CUT", dep(15)),
        "DLY": pct(20), "REV": pct(12),
        "TRIM": 96,
    }),
    dict(engine="NOISE", idx=2, name="SEA ORGAN", cat="PAD", bseed=1234, recipe="3.5 Choir pad on pitched noise (5.3 wind: resonant band-pass, key tracked)", p={
        # pink noise through a narrow band-pass that follows the key: a breathy tone on each chord note [G]
        "MODE": "ANLG", "COLR": 64, "FREQ": 72, "RES": 112, "TRK": 127, "DENS": 0, "DRFT": 24, "CRSH": 0,
        "ATK": ms(1100), "SUS": 127, "REL": ms(2500),
        "ENV2 FLT": dep(15), "E2ATK": ms(1500), "E2DEC": ms(2000), "E2SUS": pct(60), "E2REL": ms(1200),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.17), "LFO2 FLT": dep(10),
        "CHO": pct(40), "REV": pct(65), "SPRD": pct(40),
        "CHRD": "DIA3", "VOIC": "OPEN",
        "MOD1": ("AT", "CUT", dep(20)),
        "TRIM": 100,
    }),
    dict(engine="NOISE", idx=3, name="WHITE OUT", cat="FX", bamt=12, bseed=38918, recipe="5.1 Noise sweep / riser", p={
        # white noise, a low-pass swept up by ENV 2 over ~3 s with a small resonance bump [S MusicRadar / Violet]
        "MODE": "ANLG", "COLR": 14, "FREQ": 34, "RES": 40, "TRK": 0, "DENS": 0, "DRFT": 0, "CRSH": 0,
        "ENV2 FLT": 63, "E2ATK": 100, "E2DEC": 0, "E2SUS": 127, "E2REL": ms(300),
        "ATK": ms(900), "SUS": 127, "REL": ms(400),
        "REV": pct(24), "DLY": pct(16), "MOD3": ("ENV2", "REV", dep(50)), "EQLG": -4,
        "MOD1": ("AT", "CUT", dep(25)),
        # SOFT insert as a limiter (2026-10-12): flattens the crest so TRIM can raise the weighted loudness under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 100, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "TRIM": 88
    }),
    dict(engine="NOISE", idx=4, name="MOOR WIND", cat="FX", bamt=27, bseed=31000, recipe="5.3 Wind / texture", p={
        # pink-brown noise, resonant band-pass 40-80 %, the cutoff wandering (DRFT) and a slow LFO [G]
        "MODE": "ANLG", "COLR": 100, "FREQ": 44, "RES": 70, "TRK": 0, "DENS": 0, "DRFT": 90, "CRSH": 0,
        "ATK": ms(1300), "SUS": 127, "REL": ms(3000),
        "ENV2 FLT": dep(12), "E2ATK": ms(2000), "E2DEC": ms(2500), "E2SUS": pct(70), "E2REL": ms(1500),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.11), "LFO2 FLT": dep(30),
        "REV": pct(42), "DLY": pct(9), "EQLG": -4,
        "MOD1": ("AT", "CUT", dep(25)),
        # 0.2.20 (similarity audit: 3.96 from SEA ORGAN): browner noise (COLR 100), a lower and wider band (FREQ 44, RES 70) and a slow PHASR
        # sweep (0.13 Hz, 60 %): the swirl of wind round a corner, in place of the SOFT limiter insert [G]
        "INSERT": "PHASR", "INS A": hz(0.13), "INS B": 90, "INS C": 50, "INS MIX": pct(60),
        "TRIM": 118
    }),
    dict(engine="NOISE", idx=5, name="TIZZ HAT", cat="PERC", bseed=16914, recipe="noise percussion (closed hat: white noise, high band, 45 ms)", p={
        "MODE": "ANLG", "COLR": 0, "FREQ": 116, "RES": 52, "TRK": 0, "DENS": 0, "DRFT": 0, "CRSH": 0,
        "ATK": 0, "DEC": ms(45), "SUS": 0, "REL": ms(30),
        "ENV2 FLT": dep(15), "E2ATK": 0, "E2DEC": ms(40), "E2SUS": 0, "E2REL": ms(20),
        "REV": pct(10), "EQLG": -6,
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "TRIM": 117,
    }),
    dict(engine="NOISE", idx=6, name="OPEN SIZZLE", cat="PERC", bseed=25614, recipe="noise percussion (open hat: as TIZZ HAT, but ~500 ms, bright all through)", p={
        # an open hat: white noise, the filter wide open and kept there (no closing envelope: the sizzle stays bright to its
        # end), a long decay with a short release when the key goes (the choke)
        "MODE": "ANLG", "COLR": 0, "FREQ": 117, "RES": 41, "TRK": 0, "DENS": 0, "DRFT": 0, "CRSH": 0,
        "ATK": 0, "DEC": ms(717), "SUS": 0, "REL": ms(117),
        "ENV2 FLT": 0, "E2ATK": 0, "E2DEC": ms(250), "E2SUS": 127, "E2REL": ms(80),
        "REV": pct(17), "EQLG": -6,
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "TRIM": 117,
    }),
    dict(engine="NOISE", idx=7, name="STATIC SNARE", cat="PERC", bseed=58051, recipe="noise percussion (snare: noise burst with a closing filter + a ringing low band for the body)", p={
        # an analog snare: noise through a resonant band-pass; the filter envelope throws it open at the hit (the crack) and
        # drops it in ~60 ms onto the low resonance (the shell's body); short decay
        "MODE": "ANLG", "COLR": 0, "FREQ": 61, "RES": 71, "TRK": 0, "DENS": 0, "DRFT": 0, "CRSH": 0,
        "ATK": 0, "DEC": ms(171), "SUS": 0, "REL": ms(60),
        "ENV2 FLT": dep(71), "E2ATK": 0, "E2DEC": ms(57), "E2SUS": 0, "E2REL": ms(40),
        "REV": pct(18),
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "TRIM": 114,
    }),
    dict(engine="NOISE", idx=8, name="GEIGER", cat="OTHER", bseed=56918, recipe="5.3 texture (DUST: impulses through a ringing band-pass)", p={
        # ~60 clicks a second through a resonant band-pass; pressure raises the density
        "MODE": "DUST", "COLR": 0, "FREQ": 98, "RES": 92, "TRK": 0, "DENS": 110, "DRFT": 20, "CRSH": 0,
        "ATK": ms(4), "SUS": 127, "REL": ms(200),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.3), "LFO2 FLT": dep(20),
        "REV": pct(30), "DLY": pct(25),
        "MOD1": ("AT", "DENS", dep(30)),
        "DIST": pct(45),
        "TRIM": 106,
    }),
    dict(engine="NOISE", idx=9, name="MODEM", cat="OTHER", bseed=7821, recipe="5.2 metallic glitch arp (LFSR 63 steps) + 5.5 bitcrush", p={
        "MODE": "LFSR", "COLR": 0, "FREQ": 110, "RES": 30, "TRK": 127, "DENS": 48, "CRSH": 60,
        "ATK": 0, "DEC": ms(90), "SUS": 0, "REL": ms(60),
        "ENV2 FLT": dep(20), "E2ATK": 0, "E2DEC": ms(80), "E2SUS": 0, "E2REL": ms(40),
        "ARP MODE": "UPDN", "ARP RATE": "1/16", "ARP OCT": 3, "ARP GATE": pct(45), "ARP PROB": pct(85),
        "DLY": pct(40), "REV": pct(20),
        "MOD1": ("AT", "CUT", dep(25)),
        "TRIM": 114,
    }),
    # ====================================================================================================== WAVE (DUELL)
    # WAVE facts (eng_wave.c): TABLE picks a table built from a recipe (the table list: SPEC WAVE), POS scans it (0..127 = the
    # first .. last frame, adjacent frames mixed); the SHP routes (AMP DEST / ENV 2 / LFO 1 / LFO 2 SHP, the matrix's SHP) move POS,
    # SHP 63 = the whole scan. SUB a sine an octave below, DTN a second read DTN cents up (half and half). No filter of its own:
    # the multimode filter (UF CUT / UF RES / UF MODE / UF SLOPE; CUT 82 open, the default: 0.2.9, the old CUT -n is 82 - n, the same cutoff); the FLT depths start at 0 (house rule). TABLE never rolls in MORPH.
    dict(engine="WAVE", idx=0, name="PWM PAD", cat="PAD", bseed=44102, recipe="3.3 PWM string pad (the PWM table, an LFO on the width)", p={
        # the PWM table: POS 40 = a pulse of ~36 %; LFO 2 slow triangle on POS (SHP 19 = +-19 POS steps: ~44 % .. ~28 %): the
        # pulse-width sweep of a string pad [S 3.1 / 3.3: PWM by an LFO, 0.5..2 Hz: the slow end]; DTN 8 ct, the second
        # oscillator's detune [S 3.3: OSC2 detuned]; A 300 ms, S 100 %, R 800 ms [S 3.3 generic]; chorus essential [S 3.3]
        "TABLE": "PWM", "POS": 40, "DTN": 8,
        "ATK": ms(300), "DEC": ms(1500), "SUS": 127, "REL": ms(800),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.6), "LFO2 SHP": dep(30),
        "CHO": pct(45), "REV": pct(35), "SPRD": pct(30),
        # WAVE modulation (0.2.7): LFO 1, a slow unipolar triangle, adds a warp (BEND) that opens and closes: a second axis beside the width sweep [G, 0.17 Hz, 27 %]
        "LFO WAVE": "TRI", "LFO RATE": hz(0.17), "LFO POL": "UNI",
        "MOD3": ("LFO1", "BEND", dep(27)),
        "TRIM": 114,
    }),
    dict(engine="WAVE", idx=1, name="VOWEL PAD", cat="PAD", bseed=38170, recipe="3.5 Choir / vowel pad (the VOWEL table at AH, a slow drift)", p={
        # the VOWEL table at A (POS 0: F1 730, F2 1090, F3 2440 Hz) with a slow vowel drift toward E (LFO 2 on POS, +-8 steps of
        # the A-E-I-O-U scan) [S 3.5: a fixed vowel with slow vowel drift]; DTN 10 ct, an ensemble [G]
        "TABLE": "VOWEL", "POS": 8, "DTN": 10,
        # AENV A 300-900 ms, S 100 %, R 800-1500 ms [S 3.5]
        "ATK": ms(600), "DEC": ms(1500), "SUS": 127, "REL": ms(1200),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.15), "LFO2 SHP": dep(12),
        # chorus, a large hall reverb 60-90 % [S 3.5]
        "CHO": pct(40), "REV": pct(60), "SPRD": pct(30),
        # WAVE modulation (0.2.7): LFO 1, a slow unipolar triangle, brings the sub octave in and out [G, 0.13 Hz, 27 %]
        "LFO WAVE": "TRI", "LFO RATE": hz(0.13), "LFO POL": "UNI",
        "MOD3": ("LFO1", "SUB", dep(27)),
        "TRIM": 109,
    }),
    dict(engine="WAVE", idx=2, name="GLASS PAD", cat="PAD", bseed=55575, recipe="3.6 Glass / FM pad (FM 1:7 at a low index, a slow attack)", p={
        # the GLASS table (FM 1:7: a high modulator, a low index) at POS 34 (index ~1) [S 3.6: a high modulator ratio 3.5-7,
        # low index, slow attack]; ENV 2 on POS: the index rises slowly with the note and settles (the DX pad's slow modulator
        # attack [S 3.6]), +12 steps [G]; DTN 6 ct [G]
        "TABLE": "GLASS", "POS": 34, "DTN": 6,
        # AENV of a pad: A 200-800 ms, S 100 %, R 1-3 s [S 3.2]
        "ATK": ms(700), "DEC": ms(2000), "SUS": 127, "REL": ms(2000),
        "E2ATK": ms(1500), "E2DEC": ms(2500), "E2SUS": pct(40), "E2REL": ms(1500), "ENV2 SHP": dep(20),
        # chorus and reverb (the shimmer is the global REVERB TYPE's) [S 3.6]
        "CHO": pct(30), "REV": pct(45), "SPRD": pct(25),
        # WAVE modulation (0.2.7): LFO 2, a slow unipolar sine, warps the read (BEND) [G, 0.27 Hz, 21 %]
        "LFO2 WAVE": "SIN", "LFO2 RATE": hz(0.27), "LFO2 POL": "UNI",
        "MOD3": ("LFO2", "BEND", dep(21)),
        "TRIM": 103,
    }),
    dict(engine="WAVE", idx=3, name="EVOLVING PAD", cat="PAD", bseed=42897, recipe="5.6 Slow-LFO drone / 3.4 evolving pad (two slow LFOs on the scan)", p={
        # the COMB table (its notches move along the scan: a slow flanging colour) at the middle; two slow LFOs at
        # incommensurate rates (0.07 and 0.17 Hz) on POS, and the second also on a gentle low-pass [S 3.4 / 5.6: two LFOs at
        # 0.05-0.3 Hz, incommensurate, on position and filter, depth 10-30 %]; DTN 7 ct and a little SUB: detuned oscillators
        # and a sub [S 3.7]
        "TABLE": "COMB", "POS": 64, "DTN": 7, "SUB": pct(20),
        "LFO WAVE": "TRI", "LFO RATE": hz(0.07), "LFO SHP": dep(30),
        "LFO2 WAVE": "SIN", "LFO2 RATE": hz(0.17), "LFO2 SHP": dep(20), "LFO2 FLT": dep(15), "UF CUT": 62,
        # A 2-4 s, S 100 %, R 4-8 s, a large reverb and a feedback delay [S 5.6]
        "ATK": ms(2000), "DEC": ms(3000), "SUS": 127, "REL": ms(5000),
        "REV": pct(60), "DLY": pct(25), "SPRD": pct(30),
        # WAVE modulation (0.2.7): LFO 1 (already running at 0.07 Hz on the scan) also breathes the sub level around its 20 % [G, 37 %]
        "MOD3": ("LFO1", "SUB", dep(37)),
        "TRIM": 111,
    }),
    dict(engine="WAVE", idx=4, name="DRAWBARS", cat="KEYS", bseed=57282, recipe="4.5 Hammond drawbar registration (8' 4' and SUB as the 16')", p={
        # the ORGAN table at POS 32: drawbars 8' and 4' full (its second registration); SUB, a sine an octave below, is the 16'
        # (~8): the BLUES registration 888800000 without the 5 1/3' [S 4.5]; ENV 2 adds the 2 2/3' for a moment at the note's
        # start, a third-harmonic percussion (decay [G] 300 ms) [S 4.5: percussion on the 3rd harmonic]
        "TABLE": "ORGAN", "POS": 32, "SUB": pct(85),
        # organ gate: A 1-5 ms, S 100 %, R 5-40 ms [S 4.5]; organs ignore velocity, so no velocity route [S 4.5]
        "ATK": ms(2), "DEC": ms(100), "SUS": 127, "REL": ms(25),
        "E2ATK": 0, "E2DEC": ms(300), "E2SUS": 0, "E2REL": ms(30), "ENV2 SHP": dep(25),
        # the scanner chorus as the chorus send [S 4.5: chorus C1-C3]; a little drive (an amp pushed: the performer's addition
        # [S 4.5]); own DIST, as other sounds: SMART rolls a DIST of 0 upward and its first step costs ~3 dB (End B's window)
        "CHO": pct(35), "REV": pct(20), "DIST": 7,
        # WAVE modulation (0.2.7): ENV 2 (already the percussion) also gives the note's start a brief warp (BEND), a key-click edge [G, 17 %]
        "MOD3": ("ENV2", "BEND", dep(17)),
        "TRIM": 102,
    }),
    dict(engine="WAVE", idx=5, name="WAVE CHIMES", cat="KEYS", bseed=28858, recipe="4.3 FM bell / glass (bright onset decaying to a pure tone)", p={
        # the CHIME table (rounded chime / bar modes 1 3 5 9 13 19 ..); the amp envelope carries POS from 90 to 30 as it decays:
        # the sound starts bright and decays to a purer tone [S 4.3: the modulator's shorter decay]; velocity raises the
        # brightness [S 4.3: velocity to modulator level]
        "TABLE": "CHIME", "POS": 30, "AMP SHP": dep(95),
        # carrier: short attack, decay and release over 2 s [S 4.3]
        "ATK": 0, "DEC": ms(2800), "SUS": 0, "REL": ms(2200),
        "MOD2": ("VEL", "SHP", dep(20)),
        # a long hall reverb, a little chorus [S 4.3]
        "REV": pct(50), "CHO": pct(12),
        # WAVE modulation (0.2.7): the amp envelope warps the read (BEND) at the strike and lets it go as the note decays [G, 27 %]
        "MOD3": ("AENV", "BEND", dep(27)),
        "TRIM": 105,
    }),
    dict(engine="WAVE", idx=6, name="DIGI PLUCK", cat="PLUCK", bseed=63098, recipe="4.1 / 4.6 DX-style pluck (FM 1:1 whose index falls with the note)", p={
        # the DIGI table (FM 1:1) at POS 20, the amp envelope adding up to ~50 POS at the onset: the FM index (the brightness)
        # falls with the note [S 4.6: the brightness decays faster than the level]; velocity to the index [S 4.1: velocity to
        # modulator level is the expressiveness]
        "TABLE": "DIGI", "POS": 20, "AMP SHP": dep(80),
        # AENV D 0.5-2 s, S 0, R 100-300 ms [S 4.6]
        "ATK": 0, "DEC": ms(700), "SUS": 0, "REL": ms(250),
        "MOD2": ("VEL", "SHP", dep(25)),
        # chorus and a room reverb [S 4.6], a little delay [G]
        "CHO": pct(20), "REV": pct(25), "DLY": pct(18),
        # WAVE modulation (0.2.7): ENV 2 detunes the second read (DTN) at the onset and lets it settle [G, 27 %]
        "E2ATK": 0, "E2DEC": ms(450), "E2SUS": 0, "E2REL": ms(150),
        "MOD3": ("ENV2", "DTN", dep(27)),
        "TRIM": 127,
    }),
    dict(engine="WAVE", idx=7, name="SYNC LEAD", cat="LEAD", bseed=15854, mono=True, recipe="2.2 Hard-sync lead (ENV 2 sweeps SYNC down)", p={
        # the saw (SAWSQ at POS 0) hard-synced: SYNC 30 = the read at 1.6 x (+8.5 st) [S 2.2: the follower +7..+24 st above];
        # ENV 2 sweeps SYNC up at the note's start (+~50 steps, to ~+23 st) and back down over 500 ms: the "eeee-owww" [S 2.2:
        # sweep with ENV, D 300-800 ms]; aftertouch sweeps it too (slot 1: aftertouch to what suits the sound)
        "TABLE": "SAWSQ", "POS": 0, "SYNC": 30,
        "E2ATK": 0, "E2DEC": ms(500), "E2SUS": 0, "E2REL": ms(200),
        "MOD1": ("AT", "SYNC", dep(30)), "MOD2": ("ENV2", "SYNC", dep(40)),
        # AENV A 0, S 100 %, R 100-200 ms [S 2.2]
        "ATK": 0, "DEC": ms(300), "SUS": 127, "REL": ms(150),
        "GLMODE": "TIME", "GLIDE": gl(60),
        # a delayed vibrato [G], drive and delay [S 2.2: drive, delay]
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5), "LFO2 FADE": ms(500), "LFO2 PIT": 1,
        "DIST": pct(10), "DLY": pct(25), "REV": pct(15),
        # WAVE modulation (0.2.7): ENV 2 (already sweeping SYNC) also detunes the second read (DTN) at the onset [G, 17 %]
        "MOD3": ("ENV2", "DTN", dep(17)),
        "TRIM": 104,
    }),
    dict(engine="WAVE", idx=8, name="HARM BASS", cat="BASS", bseed=35823, mono=True, recipe="1.1 Minimoog-style bass (the HARM table's count as the filter)", p={
        # the HARM table: POS sets how many harmonics sound (POS 45: ~4), so ENV 2 on POS works as a filter envelope: ~12
        # harmonics at the onset falling to ~4 in 250 ms [S 1.1: FENV ~40 %, D 150-400 ms, sustain low-mid]; SUB a sine an
        # octave below for the 32' weight [S 1.1: saw at 16' and 32']; velocity to the envelope's opening [S 1.1]
        "TABLE": "HARM", "POS": 45, "SUB": pct(50),
        "E2ATK": 0, "E2DEC": ms(250), "E2SUS": pct(20), "E2REL": ms(60), "ENV2 SHP": dep(45),
        "MOD2": ("VEL", "SHP", dep(20)),
        # AENV A 0-3 ms, D 100-300 ms, S 70-100 %, R 20-80 ms [S 1.1]; no reverb on the low end [S 1.1]
        "ATK": ms(2), "DEC": ms(300), "SUS": pct(85), "REL": ms(50),
        "GLMODE": "TIME", "GLIDE": gl(40),
        # WAVE modulation (0.2.7): ENV 2 (already the harmonic count) also warps the read (BEND) at the onset [G, 17 %]
        "MOD3": ("ENV2", "BEND", dep(17)),
        "TRIM": 103,
    }),
    dict(engine="WAVE", idx=9, name="SYNTH BRASS", cat="KEYS", bseed=30305, recipe="2.3 / 4.8 Brass (the BRASS table's harmonics swell in with ENV 2)", p={
        # the BRASS table (its harmonics grow along the scan as a brass tone's with loudness) at POS 30; ENV 2 on POS: A 60 ms,
        # D 400 ms, S 50 % [S 2.3: the filter envelope makes it brass, A 30-120 ms, D 200-500 ms, S 40-60 %]; DTN 8 ct [S 2.3:
        # 5-12 ct]; aftertouch to the brightness in the sustain, velocity to the envelope's amount [S 2.3]
        "TABLE": "BRASS", "POS": 30, "DTN": 8,
        "E2ATK": ms(60), "E2DEC": ms(400), "E2SUS": pct(50), "E2REL": ms(150), "ENV2 SHP": dep(50),
        "MOD1": ("AT", "SHP", dep(20)), "MOD2": ("VEL", "SHP", dep(15)),
        # AENV A 15-40 ms, S high, R 50-150 ms [S 2.3]
        "ATK": ms(25), "DEC": ms(400), "SUS": pct(90), "REL": ms(100),
        # chorus, a short reverb [S 2.3]
        "CHO": pct(25), "REV": pct(20),
        # WAVE modulation (0.2.7): ENV 2 (already the swell) also widens the detune (DTN) with the swell [G, 17 %]
        "MOD3": ("ENV2", "DTN", dep(17)),
        "TRIM": 111,
    }),
    dict(engine="WAVE", idx=10, name="RESO SWEEP", cat="FX", bseed=36742, recipe="5.6 Slow-LFO drone (a resonant sweep, the RESO table on a slow LFO)", p={
        # the RESO table (a saw through a resonant low-pass, its cutoff along the scan) swept by a slow triangle over most of
        # the scan [S 5.6: a resonant low-pass with a very slow LFO, 0.02-0.2 Hz]; DTN 6 ct (detuned oscillators [S 5.6])
        "TABLE": "RESO", "POS": 64, "DTN": 6,
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.1), "LFO2 SHP": dep(70),
        # A 2-4 s would hide the sweep's start: A 200 ms [G]; S 100 %, R long; a large reverb and a feedback delay [S 5.6]
        "ATK": ms(200), "DEC": ms(2000), "SUS": 127, "REL": ms(1500),
        "REV": pct(45), "DLY": pct(35),
        # WAVE modulation (0.2.7): LFO 1, a slow unipolar triangle, brings the sub in and out beside the sweep [G, 0.13 Hz, 27 %]
        "LFO WAVE": "TRI", "LFO RATE": hz(0.13), "LFO POL": "UNI",
        "MOD3": ("LFO1", "SUB", dep(27)),
        "TRIM": 110,
    }),
    # DUELL 0.2.7: six sounds on the new tables (TABLE 14..25)
    dict(engine="WAVE", idx=11, name="GROWL BASS", cat="BASS", bseed=20849, mono=True, recipe="1.7 Growl bass (an LFO on the table's position, tempo-synced)", p={
        # the GROWL table (a saw under a man's vowels, U -> AH -> AE) at POS 40 (between short U and AW); LFO 2 at a half note
        # moves the vowel [S 1.7: LFO at 1/2 on the wavetable position, ~27-38 % of it]: +-19 POS steps (a third of the scan,
        # the gentle end) [G]; free-running like every factory LFO (house rule; the source retriggers it), synced to the tempo
        # it still lands on the beat; SUB: a sine an octave below for the weight [G, 1.5's sub layer]; a little drive [S 1.7:
        # distortion, drive 20 %]
        "TABLE": "GROWL", "POS": 40, "SUB": pct(40),
        "LFO2 WAVE": "SIN", "LFO2 SYNC": "1/2", "LFO2 SHP": dep(30),
        # AENV A 0-3 ms, S 70-100 %, R 20-80 ms [S 1.1, a bass]; no reverb on the low end [S 1.1]; mono, a short glide [G]
        "ATK": ms(2), "DEC": ms(300), "SUS": 127, "REL": ms(60),
        "GLMODE": "TIME", "GLIDE": gl(40),
        "DIST": pct(20),
        # WAVE modulation (0.2.7): LFO 1, a slow unipolar triangle, warps the read (BEND) beside the tempo-synced vowel [G, 0.27 Hz, 21 %]
        "LFO WAVE": "TRI", "LFO RATE": hz(0.27), "LFO POL": "UNI",
        "MOD3": ("LFO1", "BEND", dep(21)),
        "TRIM": 108,
    }),
    dict(engine="WAVE", idx=12, name="REESE BASS", cat="BASS", bseed=5220, mono=True, recipe="1.5 Reese bass (detuned saws: the REESE table and DTN)", p={
        # the REESE table (three saws drifting against each other in phase) at POS 20 and DTN 25 ct, the second read [S 1.5:
        # two saws +-25 ct]: real beating, and a slow LFO through the table's phasing [G]; SUB, a sine under it [S 1.5: a sine
        # sub layer]
        "TABLE": "REESE", "POS": 20, "DTN": 25, "SUB": pct(45),
        "LFO WAVE": "TRI", "LFO RATE": hz(0.2), "LFO SHP": dep(20),
        # a low-pass near 4 kHz (UF CUT 63), resonance low-mid [S 1.5]; ENV 2 adds ~15 % at the attack, a short decay [S 1.5]
        "UF CUT": 63, "UF RES": pct(25),
        "E2ATK": 0, "E2DEC": ms(150), "E2SUS": 0, "E2REL": ms(60), "ENV2 FLT": dep(15),
        # AENV A 0, S 100 %, R ~24 ms [S 1.5]; mono with portamento [S 1.5]; a little chorus and drive [S 1.5: common
        # additions] (own DIST, as other sounds: SMART rolls a DIST of 0 upward and its first step costs ~3 dB, End B's window)
        "ATK": 0, "DEC": ms(300), "SUS": 127, "REL": ms(24),
        "GLMODE": "TIME", "GLIDE": gl(60),
        "CHO": pct(15), "DIST": pct(8),
        # WAVE modulation (0.2.7): LFO 2, a slow triangle, moves the detune (DTN) around its 25 ct: the beating speeds up and slows down [G, 0.11 Hz, 27 %]
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.11),
        "MOD3": ("LFO2", "DTN", dep(27)),
        "TRIM": 110,
    }),
    dict(engine="WAVE", idx=13, name="HOOVER", cat="LEAD", bseed=14898, recipe="2.8 Hoover (octave-stacked pulse-saws, a fast LFO on the width, a pitch dive)", p={
        # the HOOVR table (saw-and-pulse waves at 16' 8' 4', no detune) at POS 50 (a ~30 % pulse) [S 2.8: three octaves, PWM
        # saws, locked, no detune]; LFO 2 fast on the width (POS) [S 2.8: a fast LFO into pulse width; rate and depth not
        # given: 5 Hz, +-16 steps G]; aftertouch widens it further (slot 1)
        "TABLE": "HOOVR", "POS": 50,
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5), "LFO2 SHP": dep(25),
        "MOD1": ("AT", "SHP", dep(20)),
        # the pitch starts slightly sharp and dives to the note in a fraction of a second [S 2.8]: ENV 2 -> PITCH, ~+1 st [G],
        # decay 150 ms [G]
        "E2ATK": 0, "E2DEC": ms(150), "E2SUS": 0, "E2REL": ms(60),
        "MOD2": ("ENV2", "PITCH", dep(8)),
        # a resonant low-pass with the top still bright (UF CUT 74, ~9 kHz; RES high) and a touch of drive [S 2.8]
        "UF CUT": 74, "UF RES": pct(55), "DIST": pct(10),
        # AENV [G]; a rich, deep, fast chorus [S 2.8]
        "ATK": ms(5), "DEC": ms(500), "SUS": 127, "REL": ms(200),
        "CHO": pct(70), "REV": pct(20),
        # WAVE modulation (0.2.7): LFO 1, a slow unipolar triangle, opens a detune (DTN) from 0 [G, 0.27 Hz, 17 %]
        "LFO WAVE": "TRI", "LFO RATE": hz(0.27), "LFO POL": "UNI",
        "MOD3": ("LFO1", "DTN", dep(17)),
        "TRIM": 107,
    }),
    dict(engine="WAVE", idx=14, name="DIGITAL PAD", cat="PAD", bseed=56298, recipe="3.8 Wavetable sweep pad (an envelope moves through the table)", p={
        # the BAND table (a band of harmonics moving up) from POS 10; ENV 2 carries it up slowly and settles half way: the
        # envelope's attack sets how fast the sound cycles through the table [S 3.8]; A 2.5 s, D 3 s, S 50 % [G, 3.2's FENV]
        "TABLE": "BAND", "POS": 10, "DTN": 7,
        "E2ATK": ms(2500), "E2DEC": ms(3000), "E2SUS": pct(50), "E2REL": ms(2000), "ENV2 SHP": dep(60),
        # a slow LFO keeps it moving [S 3.4: 0.05-0.3 Hz]; a 24 dB-like low-pass from digital to warmer [S 3.8]: UF CUT 70
        # (~6.4 kHz) [G]
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.12), "LFO2 SHP": dep(10),
        "UF CUT": 70,
        # AENV of a pad: A 200-800 ms, S 100 %, R 1-3 s [S 3.2]; chorus and reverb [S 3.2]
        "ATK": ms(400), "DEC": ms(2000), "SUS": 127, "REL": ms(1500),
        "CHO": pct(35), "REV": pct(50), "SPRD": pct(30),
        # WAVE modulation (0.2.7): LFO 1, a slow unipolar sine, warps the read (BEND) [G, 0.21 Hz, 21 %]
        "LFO WAVE": "SIN", "LFO RATE": hz(0.21), "LFO POL": "UNI",
        "MOD3": ("LFO1", "BEND", dep(21)),
        "TRIM": 104,
    }),
    dict(engine="WAVE", idx=15, name="BREATH PAD", cat="PAD", bseed=60243, recipe="3.9 Breathy flute pad (the AIR table, pressure to the breath)", p={
        # the AIR table (a soft flute with a breath of random harmonics) at POS 30; aftertouch adds breath and brightness
        # [S 3.9: blowing pressure to brightness, not loudness]; a low-pass at 2 kHz with moderate resonance [S 3.9]: UF CUT
        # 54, RES 25 %; LFO 2 at 5.5 Hz on that cutoff, +-1 step (~+-10 %), faded in after the onset [S 3.9: 5-6 Hz on the
        # cutoff, about +-10 %, delayed]
        "TABLE": "AIR", "POS": 30, "DTN": 6,
        "MOD1": ("AT", "SHP", dep(30)),
        "UF CUT": 54, "UF RES": pct(25),
        "LFO2 WAVE": "SIN", "LFO2 RATE": hz(5.5), "LFO2 FADE": ms(600), "LFO2 FLT": dep(2),
        # A neither instant nor slow, R long enough to join the notes [S 3.9], a pad's [S 3.5: A 300-900 ms, R 800-1500 ms];
        # chorus and a hall [S 3.5]; own DIST, a trace (SMART rolls a DIST of 0 upward and its first step costs ~3 dB, End B's
        # window), as DRAWBARS
        "ATK": ms(300), "DEC": ms(1500), "SUS": 127, "REL": ms(900),
        "CHO": pct(25), "REV": pct(55), "SPRD": pct(25), "DIST": 7,
        # WAVE modulation (0.2.7): LFO 1, a slow triangle, drifts the table position (POS) around its 30 [G, 0.17 Hz, 37 %]
        "LFO WAVE": "TRI", "LFO RATE": hz(0.17),
        "MOD3": ("LFO1", "POS", dep(37)),
        "TRIM": 109,
    }),
    dict(engine="WAVE", idx=16, name="PLATE BELL", cat="KEYS", bseed=19014, recipe="4.3 / 4.4 FM bell, modal bar (a struck plate: bright onset to the hum)", p={
        # the METAL table (the hum and a membrane's modes, shared between harmonics: clusters); the amp envelope carries POS
        # from ~74 down to 30 as it decays: the high modes die first and the hum remains [S 4.3: starts bright, decays to a
        # pure tone; S 4.4: the inharmonic modes die faster than the fundamental]; velocity to the brightness [S 4.3]
        "TABLE": "METAL", "POS": 30, "AMP SHP": dep(70),
        "MOD2": ("VEL", "SHP", dep(20)),
        # carrier: short attack, decay and release over 2 s [S 4.3]; a long hall reverb, a little chorus [S 4.3]
        "ATK": 0, "DEC": ms(2800), "SUS": 0, "REL": ms(2200),
        "REV": pct(50), "CHO": pct(12),
        # WAVE modulation (0.2.7): the amp envelope warps the read (BEND) at the strike and lets it go as the plate rings out [G, 21 %]
        "MOD3": ("AENV", "BEND", dep(21)),
        "TRIM": 106,
    }),
    # ====================================================================================================== DUELL 0.2.9 (batch R)
    # The sample engines SAMPLE, GRAIN and SLICE are retired (owner: WAVE replaces them); their 21 factory sounds are replaced here by
    # 21 sounds of the same categories on the remaining engines (recipes: SOUND_RECIPES.md 1.8 .. 6.3). Each engine's idx goes on
    # where it stood (append-only); TRIM from tests/sound_audit.c SOUND_TRIM=1, bseed from SOUND_BSEED=1.
    # ---------------------------------------------------------------------------------------------------------------- PERC
    dict(engine="PHASE", idx=6, name="DEEP KICK", cat="PERC", bseed=34356, recipe="6.1 Analogue bass drum (808 / 909 style)", p={
        # a sine (SAW at DCW 0: the bend off) [S 6.1: the 909's shaped saw "very close to a sine"]; a short knock of the bent wave at
        # the strike (ENV 56 since 0.2.20: the amp envelope into DCW) for the beater's click [S 6.1: a pulse mixed in for the beater]
        "WAVE": "SAW", "WAVE2": "-", "DCW": 0, "ENV": 56, "DTN": 0, "LINE": "MIX", "SUB": 0,
        # one decay for all of it [S 6.1: the rate is the same across the partials]; 450 ms [G]
        "ATK": 0, "DEC": ms(450), "SUS": 0, "REL": ms(120),
        # 0.2.20: a real kick, not a pitched tone at the key: the whole sound an octave down (EXPR -64 = -12 st; C3 gives a 65 Hz body), a harder
        # click (ENV 56) and a deeper pitch drop (ENV 2 60 ms, two routes: +15.6 st, 2.5x) [S 6.1: the 909's EG3 / the 808 falls from ~3x]
        # the pitch: ENV 2 (60 ms) throws it up at the strike [G: the 909's EG3, instant attack, its decay];
        # the amp envelope lets it settle by about 10 % over the body [S 6.1: "around 10 percent", a couple of semitones]
        "E2ATK": 0, "E2DEC": ms(60), "E2SUS": 0, "E2REL": ms(20),
        "MOD2": ("ENV2", "PITCH", 63), "MOD3": ("ENV2", "PITCH", 20), "MOD4": ("EXPR", "PITCH", -64), "AMP PIT": 9,
        # pressure = a harder knock (more bend) [G]; a trace of room; low end: the HF shelf down a little
        "MOD1": ("AT", "SHP", dep(20)),
        "EQHG": -2,
        "TRIM": 101,
    }),
    dict(engine="PHYS", idx=8, name="FLOOR THUD", cat="PERC", bseed=11677, recipe="6.2 Tom (a struck membrane)", p={
        # MEMB, an ideal head (HARM 0: the inharmonic modes of a circular membrane, toms and timpani) [S eng_phys.c; Felucca's TOMS
        # preset: HARM 0, BRIT 52, DAMP 40, POS 30, BEND 40, EXC 30]; a floor tom rings longer (DAMP 71) and drops less: BEND 17 =
        # about 1.6 semitones, the ~10 % of a struck head [S 6.1]
        "MODEL": "MEMB", "E2": 0, "BRIT": 52, "DAMP": 71, "POS": 30, "ACC": 100, "E7": 17, "EXC": 27,
        "ATK": 0, "SUS": 127, "REL": ms(170),
        "REV": pct(17),
        # pressure = more pitch drop [as TABLA TALK]
        "MOD1": ("AT", "E7", dep(17)),
        "TRIM": 104,
    }),
    dict(engine="TRIO", idx=7, name="FIELD CLANK", cat="PERC", bseed=50175, recipe="6.3 Cowbell (the TR-808 / CR-8000 way)", p={
        # two square oscillators a ratio of 1 : 1.44 apart (587 / 845 Hz) [S 6.3]: osc 2 six semitones up (1.414), osc 3 on osc 1;
        # a band-pass with a little resonance at about 2.6 kHz [S 6.3: 2.64 kHz, 12 dB/oct]
        "WAVE": "PLS3", "INT2": 6, "INT3": 0, "DTN": 0, "MODE": "BP", "CUT": fc(2640), "RES": 27, "PW": 64,
        # two stages: a short loud impact, then a longer tail [S 6.3]: the amp envelope's tail (D 300 ms) under ENV 2's 30 ms
        # opening of the band (the impact) [times G]
        "ATK": 0, "DEC": ms(300), "SUS": 0, "REL": ms(120),
        "ENV2 FLT": dep(30), "E2ATK": 0, "E2DEC": ms(30), "E2SUS": 0, "E2REL": ms(20),
        "REV": pct(11),
        "MOD1": ("AT", "CUT", dep(20)), "MOD2": ("VEL", "CUT", dep(25)),
        "TRIM": 121,
    }),
    # ---------------------------------------------------------------------------------------------------------------- FX
    dict(engine="WAVE", idx=17, name="FROZEN AIR", cat="FX", bamt=17, bseed=26635, recipe="5.7 Frozen texture (a slowly scanned wavetable; GRAIN's STRETCH role)", p={
        # the AIR table read like a frozen grain cloud: POS sits low and is scanned slowly [S 3.4: slow automation of the position,
        # an LFO at a low rate on it], the scan over 4 s (ENV 2 SHP) and a 0.09 Hz triangle on it [S 3.4: 0.05-0.3 Hz]
        "TABLE": "AIR", "POS": 20, "DTN": 7,
        "ENV2 SHP": dep(30), "E2ATK": ms(4000), "E2DEC": 0, "E2SUS": 127, "E2REL": ms(500),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.09), "LFO2 SHP": dep(20),
        # A 600 ms, R 3.5 s [S 3.4: A 400-1500 ms, R 1.5-4 s]; a big reverb and a delay [S 3.4: reverb 60-90 %]
        "ATK": ms(600), "SUS": 127, "REL": ms(3500),
        "REV": pct(80), "DLY": pct(25), "EQLG": -3,
        "MOD1": ("AT", "SHP", dep(25)),
        # WAVE modulation: a slow free LFO 1 drifts the second read's detune (DTN) [G, 0.13 Hz, 17 %]
        "LFO WAVE": "TRI", "LFO RATE": hz(0.13),
        "MOD3": ("LFO1", "DTN", dep(17)),
        # the multimode low-pass: CUT 60 = 2.98 kHz, RES 31, 12 dB; both LFOs move the cutoff through the matrix (+30 % depth each, about 19 CUT
        # steps), at their own unrelated slow rates (0.13 and 0.09 Hz), both free-running [G]
        "UF CUT": 60, "UF RES": 31,
        "MOD2": ("LFO1", "CUT", dep(30)), "MOD4": ("LFO2", "CUT", dep(30)),
        "TRIM": 107,
    }),
    dict(engine="NOISE", idx=10, name="REV CYMBAL", cat="FX", bseed=57441, recipe="5.8 Reverse cymbal", p={
        # white noise in a high band [S 5.1: noise source; G: a cymbal's band], swelling up and cut off at the end: a crash played
        # backwards [S 5.1: reversed hits / tails]; the band opens with the swell (ENV 2 on FREQ)
        "MODE": "ANLG", "COLR": 0, "FREQ": 107, "RES": 21, "TRK": 17, "DENS": 0, "DRFT": 0, "CRSH": 0,
        "ATK": ms(1100), "SUS": 127, "REL": ms(37),
        "ENV2 FLT": dep(31), "E2ATK": ms(1100), "E2DEC": 0, "E2SUS": 127, "E2REL": ms(30),
        "REV": pct(37), "EQLG": -6,
        "MOD1": ("AT", "CUT", dep(17)),
        "TRIM": 109,
    }),
    dict(engine="PHASE", idx=7, name="SONAR PING", cat="FX", bseed=64729, recipe="5.9 Sonar ping", p={
        # a pure sine ping (DCW 0) with a trace of bend at the strike, a long ring and a long echo [G]
        "WAVE": "SAW", "WAVE2": "-", "DCW": 0, "ENV": 11, "DTN": 0, "LINE": "MIX", "SUB": 0,
        "ATK": 0, "DEC": ms(700), "SUS": pct(41), "REL": ms(1700),   # (it rings on while held: an FX that holds its body)
        "DLY": pct(47), "REV": pct(61), "EQLG": -4,
        "MOD1": ("AT", "SHP", dep(17)),
        "TRIM": 101,
    }),
    # ---------------------------------------------------------------------------------------------------------------- PLUCK
    dict(engine="PHYS", idx=9, name="NURSERY KEYS", cat="PLUCK", bseed=13705, recipe="4.11 Toy piano (struck metal rods)", p={
        # hammers on metal rods fixed at one end [S 4.11]: MODAL with the partials stretched far apart (STRC 124: a clamped rod's
        # modes are far from harmonic [G]), bright (metal), struck near the end, a plastic knock (EXC) [G]
        "MODEL": "MODAL", "STRC": 124, "BRIT": 91, "DAMP": 71, "POS": 10, "ACC": 107, "BOW": 0, "EXC": 21,
        "ATK": 0, "SUS": 127, "REL": ms(170),
        "REV": pct(21), "DLY": pct(11),
        "MOD1": ("AT", "BRIT", dep(17)),
        "TRIM": 113,
    }),
    dict(engine="FM6", idx=9, name="ISLAND STEEL", cat="PLUCK", bseed=19220, recipe="4.12 Steel drum (harmonic modes 1 : 2 : 3)", p={
        # a pan's note areas are tuned so the lowest modes sit at the fundamental, the octave and the twelfth [S 4.12]: the NYLON
        # PICK patch (F8) has its carriers at ratios 1 and 2 (an octave apart) with modulators at 3 and 1, all harmonic [G]; the
        # modulators a little slower (MEG) for the metallic ring, low index (MLVL), velocity to brightness, a slight spread [G]
        "SLOT": "F8", "MEG": 11, "MLVL": -7, "VMOD": 3, "DTUN": pct(11),
        "REV": pct(27), "DLY": pct(14),
        "TRIM": 118,
    }),
    dict(engine="PHASE", idx=8, name="FAKE KOTO", cat="PLUCK", bseed=44371, recipe="4.10 Koto (a bright pluck near the bridge)", p={
        # a silk string plucked with a pick near the bridge: bright, the harmonics dying first [S 4.6: a pluck near the end keeps
        # the high harmonics]: the saw bent hard at the pluck and closing with the decay (ENV) [G]
        "WAVE": "SAW", "WAVE2": "-", "DCW": 37, "ENV": 71, "DTN": 0, "LINE": "MIX", "SUB": 0,
        "ATK": 0, "DEC": ms(1100), "SUS": 0, "REL": ms(270),
        # the string settles a little flat after the pluck (ENV 2 to the pitch, a fraction of a semitone) [G]
        "E2ATK": 0, "E2DEC": ms(70), "E2SUS": 0, "E2REL": ms(30),
        "MOD2": ("ENV2", "PITCH", 7),
        "MOD1": ("AT", "SHP", dep(17)),
        "REV": pct(27), "DLY": pct(11),
        "TRIM": 95,
    }),
    # ---------------------------------------------------------------------------------------------------------------- KEYS
    dict(engine="PHYS", idx=10, name="FAUX HARPSI", cat="KEYS", bseed=48968, recipe="4.9 Harpsichord (a quill pluck, no touch, a felt damper)", p={
        # a plectrum plucks the string [S 4.9]: STRNG; plucked close to the end for the nasal, high-harmonic tone [S 4.9; S 4.6],
        # thin steel strings with little stiffness (STRC 71) [G]; the quill's click (EXC) [G]
        "MODEL": "STRNG", "STRC": 71, "BRIT": 107, "DAMP": 44, "POS": 11, "ACC": 127, "BOW": 0, "EXC": 0,
        # the felt damper stops the note at the key's release [S 4.9]: a short release
        "ATK": 0, "SUS": 127, "REL": ms(70),
        # a small room, no chorus [G]; no touch dynamics on the instrument [S 4.9]: no velocity routes
        "REV": pct(17),
        "MOD1": ("AT", "BRIT", dep(11)),
        # SOFT insert as a limiter (as on several sounds since 2026-10-12): the string's strike peaks high over its body, the limiter
        # flattens the crest so TRIM can bring the body up under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 100, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "TRIM": 107,
    }),
    dict(engine="FM6", idx=10, name="SALOON KEYS", cat="KEYS", bseed=28718, recipe="4.13 Honky-tonk detune (on the FM electric piano)", p={
        # the TINE EP patch (F1) with its carriers spread far apart: the out-of-tune unison of a honky-tonk upright [S 4.13] (DTUN 71:
        # about +-20 ct); a harder hammer (MLVL up), velocity the expression [S 4.1]
        "SLOT": "F1", "DTUN": pct(56), "MLVL": 7, "VMOD": 3,
        "E2ATK": 0, "E2DEC": ms(400), "E2SUS": 0, "E2REL": ms(170),
        # dry and close: a little room [G]
        "REV": pct(14), "CHO": pct(7),
        "TRIM": 112,
    }),
    # ---------------------------------------------------------------------------------------------------------------- LEAD
    dict(engine="WAVE", idx=18, name="PURPLE FLUTE", cat="LEAD", bseed=63290, mono=True, recipe="2.9 Flute lead (SOS: saw into a low-pass at 2 kHz, cutoff vibrato)", p={
        # the AIR table low (the soft flute, little breath: POS 10) [G]; the low-pass at about 2 kHz with moderate resonance and a
        # 5-6 Hz cutoff modulation of about +-10 %, brought in after the onset; pressure to the cutoff, brighter not louder [S 3.9]
        "TABLE": "AIR", "POS": 10, "DTN": 0,
        "UF CUT": 54, "UF RES": pct(27),
        "LFO2 WAVE": "SIN", "LFO2 RATE": hz(5.5), "LFO2 FADE": ms(500), "LFO2 FLT": dep(2),
        "MOD1": ("AT", "CUT", dep(17)),
        # attack neither instant nor slow, full sustain, a short release [S 3.9]; a light glide (legato) [G]
        "ATK": ms(71), "SUS": 127, "REL": ms(170), "GLMODE": "TIME", "GLIDE": gl(40),
        "DLY": pct(21), "REV": pct(31),
        # WAVE modulation: ENV 2 adds a breath at the onset (POS up, then back) [G, 27 %]
        "E2ATK": 0, "E2DEC": ms(170), "E2SUS": 0, "E2REL": ms(70),
        "MOD3": ("ENV2", "POS", dep(27)),
        "TRIM": 101,
    }),
    dict(engine="WAVE", idx=19, name="LICORICE", cat="LEAD", bseed=46772, mono=True, recipe="2.10 Clarinet (odd harmonics: a square)", p={
        # a square's odd harmonics are the clarinet's [S 2.10]: the ODD table (sine -> square, odd harmonics one by one), POS 37:
        # about ten of them; a gentle low-pass (UF CUT) for the woody top [G]
        "TABLE": "ODD", "POS": 37, "DTN": 0,
        "UF CUT": 65, "UF RES": pct(11),
        # pressure: more harmonics, as blowing harder [G]; a classical clarinet plays without vibrato [G]
        "MOD1": ("AT", "SHP", dep(21)),
        "ATK": ms(37), "SUS": 127, "REL": ms(110), "GLMODE": "TIME", "GLIDE": gl(37),
        "REV": pct(27), "DLY": pct(11),
        # WAVE modulation: ENV 2 opens a few more harmonics at the tongued onset [G, 21 %]
        "E2ATK": 0, "E2DEC": ms(110), "E2SUS": 0, "E2REL": ms(70),
        "MOD3": ("ENV2", "POS", dep(21)),
        "TRIM": 101,
    }),
    dict(engine="TRIO", idx=8, name="OBOE-ISH", cat="LEAD", bseed=1212, mono=True, recipe="2.11 Oboe (a narrow pulse: nasal)", p={
        # narrowing the pulse toward 10 % makes the tone nasal, the oboe's [S 2.10 / 2.11]: PW 17 (about 13 %); all three on one
        # pitch, a hair detuned; a low-pass at 3.5 kHz [G]
        "WAVE": "PLS3", "INT2": 0, "INT3": 0, "DTN": 3, "MODE": "LP", "CUT": fc(3500), "RES": 27, "PW": 17,
        "ATK": ms(47), "SUS": 127, "REL": ms(120), "GLMODE": "TIME", "GLIDE": gl(37),
        "ENV2 FLT": dep(11), "E2ATK": ms(30), "E2DEC": ms(300), "E2SUS": pct(70), "E2REL": ms(100),
        # a delayed vibrato, pressure deepens it [as the other leads; G]
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(5.1), "LFO2 FADE": ms(470), "LFO2 PIT": 1,
        "MOD1": ("AT", "DEP2", dep(100)), "MOD2": ("VEL", "CUT", dep(17)),
        "REV": pct(27), "DLY": pct(17),
        "TRIM": 92,
    }),
    # ---------------------------------------------------------------------------------------------------------------- PAD
    dict(engine="TRIO", idx=9, name="STRING MACH", cat="PAD", bseed=52771, recipe="3.10 String machine (Solina-style ensemble)", p={
        # a narrow pulse (5-20 %) and a second oscillator an octave up (cellos and violins) [S 3.10, MusicRadar]: PW 17, osc 2 +12,
        # osc 3 on the root; detuned a little [G]
        "WAVE": "PLS3", "INT2": 12, "INT3": 0, "DTN": 7, "MODE": "LP", "CUT": fc(5000), "RES": 11, "PW": 17,
        # the machines' slow attack and long release [G]
        "ATK": ms(270), "DEC": ms(1500), "SUS": 127, "REL": ms(1100),
        "ENV2 FLT": dep(11), "E2ATK": ms(470), "E2DEC": ms(1500), "E2SUS": pct(70), "E2REL": ms(900),
        # the ensemble chorus is the sound [S 3.10: three modulated delay lines]; a hall [G]
        "CHO": pct(71), "REV": pct(41), "SPRD": pct(27),
        "MOD1": ("AT", "CUT", dep(21)),
        # subtle motion: a slow free LFO 2 on the cutoff [house rule, 0.17 Hz, 5 %]
        "LFO2 RATE": hz(0.17),
        "MOD4": ("LFO2", "CUT", 3),
        "TRIM": 90,
    }),
    dict(engine="VOICE", idx=6, name="WHISPER PAD", cat="PAD", bseed=40791, recipe="3.11 Whisper pad (Felucca's WHISPER preset, 3.5)", p={
        # Felucca's WHISPER: "ah" gliding toward "oh" (TALK 88), the formants a little up, breath at the top so the voicing fades to
        # a whisper [S eng_formant.c, the WHISPER preset: VOWL 0, VOWL2 95, TALK 88, SHIFT 3, BUZZ 50, BRTH 120, Q 50]
        "VOWL": 0, "VOWL2": 95, "TALK": 88, "SHIFT": 3, "BUZZ": 50, "BRTH": 120, "Q": 50, "RAND": 11,
        # a pad's envelope [S 3.5: A 300-900 ms, R 800-1500 ms]; chorus and a large hall [S 3.5]
        "ATK": ms(800), "DEC": ms(1500), "SUS": pct(87), "REL": ms(1500),
        "CHO": pct(41), "REV": pct(71), "DLY": pct(17), "SPRD": pct(37),
        "MOD1": ("AT", "SHP", dep(27)),
        # subtle motion: a slow free LFO 2 on the vowel it settles on [house rule, 0.11 Hz]
        "LFO2 RATE": hz(0.11),
        "MOD4": ("LFO2", "VOWL2", 4),
        "TRIM": 99,
    }),
    dict(engine="WAVE", idx=20, name="BRASS PAD", cat="PAD", bseed=17892, recipe="3.12 Brass pad (the brass swell, slowed)", p={
        # brass: the harmonics grow as the note swells [S 2.3]: the BRASS table (harmonics growing in along POS) carried up by
        # ENV 2 SHP (A 600 ms) and settling, a pad's slow amp envelope [G]; power chords as the brass section's [G]
        "TABLE": "BRASS", "POS": 31, "DTN": 9,
        "ENV2 SHP": dep(37), "E2ATK": ms(600), "E2DEC": ms(1500), "E2SUS": pct(60), "E2REL": ms(1100),
        "ATK": ms(370), "DEC": ms(1500), "SUS": 127, "REL": ms(1200),
        "CHO": pct(37), "REV": pct(51), "SPRD": pct(27),
        "CHRD": "POW", "VOIC": "OPEN",
        "MOD1": ("AT", "SHP", dep(21)),
        # WAVE modulation: a slow free LFO 1 on the second read's detune [G, 0.17 Hz, 17 %]
        "LFO WAVE": "TRI", "LFO RATE": hz(0.17),
        "MOD3": ("LFO1", "DTN", dep(17)),
        "TRIM": 106,
    }),
    # ---------------------------------------------------------------------------------------------------------------- BASS
    dict(engine="PHYS", idx=11, name="PLUCK BASS", cat="BASS", bseed=13381, recipe="1.8 Electric bass, fingered (a plucked string, long sustain)", p={
        # a plucked string, the fingertip a low-pass (dark: BRIT 47) [S 4.6]; plucked a third of the way along [S 4.6: a pluck at
        # 1/n cancels the n-th harmonic]; a solid body's long ring (DAMP 118) that the key's release damps [G]
        "MODEL": "STRNG", "STRC": 61, "BRIT": 47, "DAMP": 118, "POS": 41, "ACC": 127, "BOW": 0, "EXC": 11,
        "ATK": 0, "SUS": 127, "REL": ms(110),
        # bass: the HF shelf down, a subtle PUMP ducker (house rule)
        "EQHG": -4, "EQHF": -8, "DKSRC": "PUMP", "DKRATE": "1/4", "DKDEP": 14,
        "REV": pct(7),
        "MOD1": ("AT", "BRIT", dep(17)),
        # SOFT insert as a limiter (as on several sounds since 2026-10-12): the string's strike peaks high over its body, the limiter
        # flattens the crest so TRIM can bring the body up under the -3 dBFS peak cap
        "INSERT": "SOFT", "INS A": 100, "INS B": 127, "INS C": 112, "INS MIX": 127,
        "TRIM": 106,
    }),
    dict(engine="FM6", idx=11, name="FRETLESS", cat="BASS", bseed=1048, mono=True, recipe="1.9 Fretless bass (the 'mwah', slides)", p={
        # 0.2.17: its own patch F11 (before it shared the ROUND BASS patch F3 with SLAP DOCTOR and the two sounded alike): a warm, near-sine
        # bass, a 1:1 sine modulator that opens slowly after the pluck (the 'mwah', the tone opening as the string buzzes along the
        # fingerboard [G]) and a quiet carrier an octave down for the body; slides between notes [S 1.9]: legato with a 90 ms glide
        "SLOT": "F11", "MEG": 0, "MLVL": 0, "VMOD": 2, "FB": 0,
        "GLMODE": "TIME", "GLIDE": gl(90),
        "EQHG": -4, "EQHF": -8, "DKSRC": "PUMP", "DKRATE": "1/4", "DKDEP": 14,
        "CHO": pct(11), "DIST": 7,
        "TRIM": 120,
    }),
    # ---------------------------------------------------------------------------------------------------------------- OTHER
    dict(engine="LOFI", idx=7, name="DUSTY DRONE", cat="OTHER", bseed=63786, recipe="5.10 Lofi drone (5.5 / 5.6)", p={
        # a crushed saw, dark (TONE 57) [S/G 5.5: bit depth reduced, a low-pass]; no pitch wobble: the slow motion is on the tone
        # (LFO 2 to TONE, 0.27 Hz) [G 5.6: 0.02-0.2 Hz .. a little faster]; slow in, long out [G 5.6]
        "CHIP": "8BIT", "WAVE": "SAW", "DUTY": 64, "CRSH": 47, "SWP": 0, "VIB": 0, "ARP": "OFF", "TONE": 57,
        "ATK": ms(1000), "SUS": 127, "REL": ms(1700),
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(0.27),
        "MOD4": ("LFO2", "TONE", 7),
        "DLY": pct(27), "REV": pct(37), "EQLG": -3,
        # a dusty record under it: the VINYL insert, a few clicks and a faint noise bed, the warp barely there [G]
        "INSERT": "VINYL", "INS A": 40, "INS B": 36, "INS C": 18, "INS MIX": 127,
        "MOD1": ("AT", "TONE", dep(21)),
        "TRIM": 102,
    }),
    dict(engine="TRIO", idx=10, name="DIAL TONE", cat="OTHER", bseed=61579, recipe="5.11 Telephone dial tone (350 + 440 Hz)", p={
        # North America's dial tone: 350 Hz and 440 Hz together, continuous [S 5.11]; 440 / 350 = 1.257, four semitones (1.260):
        # osc 2 +4 over the key (F4 plays 349 + 440 Hz); triangles low-passed toward sines [G]
        "WAVE": "TRI3", "INT2": 4, "INT3": 0, "DTN": 0, "MODE": "LP", "CUT": fc(900), "RES": 0, "PW": 64,
        "ATK": ms(5), "SUS": 127, "REL": ms(17),
        "MOD1": ("AT", "CUT", dep(11)),
        "TRIM": 93,
    }),
]

# DUELL 0.2.17 (owner, 2026-10-15): 14 new factory sounds, the bank 114 -> 128 (docs/NEW_PATCHES_PROPOSAL.md; the drafts' numbers were set by
# measurement and by the recipes of DUELL/SOUND_RECIPES.md 1.10..1.16, 2.12, 2.13, 4.14, 6.4; the owner listens). Every idx is appended
# to its engine, so favourites, saved patches, the factory multis and every End B seed keep their identity: only the browse numbers (#NN) move.
# House bass rule kept: the HF shelf down on every bass; the subtle PUMP ducker only on the longer, rolling basses (ARP BASS, FM BASS 2) and OFF on the
# hard ones (KICK BASS, FOLD BASS, SID BASS, SYNC BASS, FUZZ BASS) and on HARD KICK. Basses are dry (no chorus / delay / reverb send; FM BASS 2 has a touch of
# reverb for its CC0 voice). No vibrato route anywhere (the only routes to PITCH are the kick's ENV 2 drop).
_BASS_SHELF = {"EQHG": -4, "EQHF": -8}                                                     # (the HF shelf down)
_BASS_PUMP = {"EQHG": -4, "EQHF": -8, "DKSRC": "PUMP", "DKRATE": "1/4", "DKDEP": 14}       # (and the subtle PUMP ducker)
SOUNDS += [
    # ------------------------------------------------------------------------------------------------------------ BASS
    dict(engine="WAVE", idx=21, name="KICK BASS", cat="BASS", bseed=3922, mono=True, recipe="1.13 Hardstyle kick-bass (ENV 2 on pitch: the kick's drop, then a sine body)", p={
        # hardstyle kick: a fast pitch drop at the start, a sine body, distortion on the tail [S: EDMProd "how to make hardstyle", ADSR Sounds
        # "hardstyle kick with Serum": the drop is a few semitones to an octave over 10..30 ms]; played as a bass note [G]
        # HARM at its first frames: a sine with a few harmonics, a square sub under it [G]
        "TABLE": "HARM", "POS": 10, "SUB": pct(55),
        # ENV 2 -> PITCH on two matrix routes (63 each): about +23 semitones at the strike, falling in ~45 ms [G]
        "MOD3": ("ENV2", "PITCH", 63), "MOD4": ("ENV2", "PITCH", 63),
        # played where a kick sits: the whole sound one octave down (EXPR is 127, full, until a CC11 lowers it: a constant; one route of -64 is
        # exactly -12 semitones), so C4 gives a 65 Hz body and F3 (the lowest key) 44 Hz: the SUB (an octave below the table) is the fundamental
        "MOD5": ("EXPR", "PITCH", -64),
        "E2ATK": 0, "E2DEC": ms(95), "E2SUS": 0, "E2REL": ms(30),
        "ATK": 0, "DEC": ms(380), "SUS": pct(55), "REL": ms(60), "GLMODE": "TIME", "GLIDE": gl(0),
        # the distorted tail: the HARD insert and a little DIST [S: distortion after the sine; G: amounts]
        "INSERT": "HARD", "INS A": 112, "INS B": 90, "INS C": 96, "INS MIX": 127, "DIST": pct(32),
        "MOD1": ("AT", "SHP", dep(25)),
        **_BASS_SHELF,
        "TRIM": 107,
    }),
    dict(engine="ANALOG", idx=16, name="FOLD BASS", cat="BASS", bseed=17953, mono=True, recipe="1.12 Industrial wavefolder bass (filter, then fold; an envelope on the fold)", p={
        # a saw over a square an octave down (WAVE SUB), the filter BEFORE the folder, an envelope on the fold amount [S: Noise Engineering
        # "Getting started: wavefolders", AE Modular bass tutorial: fold after a filter, an envelope on the fold]; the sub keeps the bottom [G]
        "WAVE": "SUB", "DTN": 0, "MIX": pct(55), "CUT": 60, "RES": 20, "DRV": 20,
        "ENV2 FLT": dep(35), "E2ATK": 0, "E2DEC": ms(260), "E2SUS": pct(35), "E2REL": ms(60),
        "ATK": 0, "DEC": ms(260), "SUS": pct(90), "REL": ms(40), "GLMODE": "TIME", "GLIDE": gl(25),
        "INSERT": "FOLD", "INS A": 46, "INS B": 100, "INS C": 96, "INS MIX": 127,
        "MOD3": ("ENV2", "INS A", dep(26)),          # ENV 2 opens the fold on the strike
        "MOD1": ("AT", "INS A", dep(25)),
        **_BASS_SHELF,
        "TRIM": 111,
    }),
    dict(engine="TRIO", idx=11, name="SID BASS", cat="BASS", bseed=7135, mono=True, recipe="1.10 Chip pulse bass (SID-style PWM)", p={
        # three pulses, oscillator 3 an octave down, a few cents apart; the pulse width moved slowly by LFO 2 (the PWM of the SID's players:
        # c64-wiki, Rob Hubbard: PWM, ring mod and LFO were his tools; no source gives register values, the numbers are G)
        "WAVE": "PLS3", "INT2": 0, "INT3": -12, "DTN": 7, "MODE": "LP", "CUT": 72, "RES": 24, "PW": 40,
        "LFO2 WAVE": "TRI", "LFO2 RATE": hz(1.1), "LFO2 SHP": dep(28),
        "ENV2 FLT": dep(40), "E2ATK": 0, "E2DEC": ms(220), "E2SUS": pct(25), "E2REL": ms(60),
        "ATK": ms(2), "DEC": ms(240), "SUS": pct(85), "REL": ms(50), "GLMODE": "TIME", "GLIDE": gl(35),
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(15)),
        **_BASS_SHELF,
        "TRIM": 93,
    }),
    dict(engine="TRIO", idx=12, name="SYNC BASS", cat="BASS", bseed=43701, mono=True, recipe="1.11 Hard-sync bass (2.2 sync, in the low octave)", p={
        # oscillator 2 hard-synced to oscillator 1, a fifth up; ENV 2 sweeps the sync interval down on every note (the 'eow' of sync) [S 2.2
        # for the principle; the setting is G]
        "WAVE": "SYNC", "INT2": 7, "INT3": 0, "DTN": 0, "MODE": "LP", "CUT": 62, "RES": 30, "PW": 64,
        "MOD3": ("ENV2", "INT2", dep(26)),
        "ENV2 FLT": dep(35), "E2ATK": 0, "E2DEC": ms(190), "E2SUS": 0, "E2REL": ms(50),
        "ATK": ms(2), "DEC": ms(300), "SUS": pct(80), "REL": ms(45), "GLMODE": "TIME", "GLIDE": gl(30),
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(15)),
        **_BASS_SHELF,
        "TRIM": 97,
    }),
    dict(engine="PHASE", idx=9, name="FUZZ BASS", cat="BASS", bseed=57016, mono=True, recipe="1.14 Fuzz bass (resonant phase-distortion saw into a FUZZ insert)", p={
        # the 'distorted layer' bass: the resonant phase-distortion saw of ELASTIC BASS (the 303-like squelch) into a FUZZ insert [G]
        "WAVE": "RSAW", "WAVE2": "-", "DCW": 44, "ENV": 45, "DTN": 0, "LINE": "MIX", "SUB": pct(38),
        "E2ATK": 0, "E2DEC": ms(200), "E2SUS": pct(20), "E2REL": ms(50),
        "ATK": ms(2), "DEC": ms(260), "SUS": pct(88), "REL": ms(45), "GLMODE": "TIME", "GLIDE": gl(30),
        "INSERT": "FUZZ", "INS A": 82, "INS B": 90, "INS C": 90, "INS MIX": 110,
        **_BASS_SHELF,
        "TRIM": 113,
    }),
    dict(engine="FM6", idx=12, name="FM BASS 2", cat="BASS", bseed=17352, mono=True, recipe="1.15 FM sub bass (an imported FM voice, CC0, through our filter and effects)", p={
        # the imported voice F9 (tools/gen_fm6_patches.py FM_BASS_2: a round FM sub bass, algorithm 22; CC0 1.0 Universal, aquanodemusic). Made
        # ours by the sound: a gentle multimode low-pass (12 dB, ~3.7 kHz, a little resonance) takes the digital edge off the top, a SOFT insert
        # adds a little warmth, the house bass shelf and the PUMP ducker, a touch of reverb [G]
        "SLOT": "F9",
        "UF CUT": 64, "UF RES": pct(10), "UF MODE": 0, "UF SLOPE": 1,
        "INSERT": "SOFT", "INS A": 40, "INS B": 127, "INS C": 112, "INS MIX": 90,
        "REV": pct(5),
        **_BASS_PUMP,
        "TRIM": 104,
    }),
    dict(engine="LOFI", idx=8, name="ARP BASS", cat="BASS", bseed=16972, mono=True, recipe="1.16 Chip octave bass (the engine's octave arp on a pulse)", p={
        # a rolling octave bass of the chip tunes: the LOFI engine's built-in octave arp on a pulse, 8-bit, dry and hard [G]
        "CHIP": "8BIT", "WAVE": "PLS", "DUTY": 40, "CRSH": 0, "SWP": 0, "VIB": 0, "ARP": "OCT", "TONE": 84,
        "ATK": 0, "DEC": ms(150), "SUS": pct(70), "REL": ms(30),
        **_BASS_PUMP,
        "TRIM": 97,
    }),
    # ------------------------------------------------------------------------------------------------------------ LEAD
    dict(engine="ANALOG", idx=17, name="SQUARE LEAD", cat="LEAD", bseed=21704, mono=True, recipe="2.12 Square lead with portamento (2.1 Moog mono lead)", p={
        # the classic mono square lead: two square waves a few cents apart, the filter opening with the note, a glide between notes [G 2.1];
        # no vibrato (the owner's rule)
        "WAVE": "SQR", "DTN": 7, "MIX": pct(40), "CUT": 84, "RES": 16, "KTR": 64,
        "ENV2 FLT": dep(25), "E2ATK": ms(8), "E2DEC": ms(500), "E2SUS": pct(60), "E2REL": ms(200),
        "ATK": ms(5), "SUS": 127, "REL": ms(200), "GLMODE": "TIME", "GLIDE": gl(70),
        "MOD1": ("AT", "CUT", dep(25)), "MOD2": ("VEL", "CUT", dep(15)),
        "DLY": pct(18), "REV": pct(15),
        "TRIM": 93,
    }),
    dict(engine="WAVE", idx=22, name="SCREAMER", cat="LEAD", bseed=53917, mono=True, recipe="2.13 Screaming lead (the YELL table, driven)", p={
        # the hard-dance screech: the YELL table (a formant table with a harsh scan) through the multimode filter and a little DIST [G; the
        # WAVE engine's notes: 'screaming leads (YELL)']
        "TABLE": "YELL", "POS": 42, "DTN": 10,
        "UF CUT": 72, "UF RES": pct(28),
        "ATK": ms(3), "SUS": 127, "REL": ms(140), "GLMODE": "TIME", "GLIDE": gl(80), "DIST": pct(26),
        "MOD1": ("AT", "SHP", dep(30)), "MOD2": ("VEL", "CUT", dep(15)),
        "DLY": pct(16), "REV": pct(14),
        "TRIM": 115,
    }),
    # ------------------------------------------------------------------------------------------------------------ PAD
    dict(engine="VOICE", idx=7, name="PLATE CHOIR", cat="PAD", bseed=64679, recipe="3.5 Choir pad, voiced for a big reverb (PLATE)", p={
        # a choir that leans on the reverb: the vowel glides from 'ah' toward 'oh', breath and a slow random vowel drift; a long pad envelope and
        # a big REVERB send, open triads (the reverb type is global: on PLATE it is the studio-plate choir, on HALL a very wet one) [G 3.5]
        "VOWL": 10, "VOWL2": 60, "TALK": 40, "SHIFT": 0, "BUZZ": 38, "BRTH": 14, "Q": 52, "RAND": 6,
        "ATK": ms(900), "DEC": ms(1500), "SUS": pct(92), "REL": ms(1500),
        "CHO": pct(30), "REV": pct(90), "SPRD": pct(40), "CHRD": "DIA3", "VOIC": "OPEN",
        "TRIM": 95,
    }),
    # ------------------------------------------------------------------------------------------------------------ PLUCK
    dict(engine="ANALOG", idx=18, name="ARP PLUCK", cat="PLUCK", bseed=12417, recipe="1.6 Pluck bass / 4.6 string pluck, played up as an arp", p={
        # the classic synth pluck: a detuned saw pair, a filter that drops fast (ENV 2 FLT 66 %, 190 ms, no sustain) [S 1.6]; the arp, up in
        # sixteenths over two octaves, is the sound's (A UP 1/16, gate 60 %)
        "WAVE": "SAW", "DTN": 10, "MIX": pct(50), "CUT": 44, "RES": 26,
        "ENV2 FLT": dep(66), "E2ATK": 0, "E2DEC": ms(190), "E2SUS": 0, "E2REL": ms(120),
        "ATK": 0, "DEC": ms(380), "SUS": 0, "REL": ms(130),
        "ARP MODE": "UP", "ARP RATE": "1/16", "ARP OCT": 2, "ARP GATE": pct(60),
        "CHO": pct(22), "DLY": pct(24), "REV": pct(18), "MOD2": ("VEL", "CUT", dep(20)),
        "TRIM": 101,
    }),
    # ------------------------------------------------------------------------------------------------------------ KEYS
    dict(engine="FM6", idx=13, name="TUBE BELLS", cat="KEYS", bseed=22916, recipe="4.14 FM tubular bell (Chowning's bell, carrier : modulator 1 : 1.4 = 5 : 7)", p={
        # tubular bell by FM: carrier : modulator = 1 : 1.4 (5 : 7), the index and the amplitude both decaying [S: Chowning 1973 as taught in the SFU
        # Truax FM tutorial and a University of Washington handout: bell 200 Hz carrier, 280 Hz modulator, index ~10, exponential decay]; our own
        # patch F10 (tools/gen_fm6_patches.py), three such pairs at 1.0 / 2.76 / 5.4 [G]; a long ring and a touch of space
        "SLOT": "F10",
        "REV": pct(30), "CHO": pct(6),
        "TRIM": 107,
    }),
    # ------------------------------------------------------------------------------------------------------------ PERC
    dict(engine="PHASE", idx=10, name="HARD KICK", cat="PERC", bseed=14853, recipe="6.4 Hardstyle kick (a sine with a pitch drop, hard-clipped)", p={
        # a sine (the SAW at DCW 0, as DEEP KICK) with a pitch drop of ~75 ms (ENV 2 on PITCH, two routes) and a short click, then hard clipping
        # [S: EDMProd, ADSR Sounds: a fast drop, distortion, then EQ]; DEEP KICK's 6.1 for the sine; no ducker (it is the kick)
        "WAVE": "SAW", "WAVE2": "-", "DCW": 0, "ENV": 14, "DTN": 0, "LINE": "MIX", "SUB": 0,
        "E2ATK": 0, "E2DEC": ms(75), "E2SUS": 0, "E2REL": ms(20),
        "MOD3": ("ENV2", "PITCH", 63), "MOD4": ("ENV2", "PITCH", 63),
        # played where a kick sits: the whole sound two octaves down (EXPR is 127, full, until a CC11 lowers it: a constant; two routes of -64 are
        # exactly -24 semitones), so C4 gives a 65 Hz body and F3 (the lowest key) 44 Hz
        "MOD5": ("EXPR", "PITCH", -64), "MOD6": ("EXPR", "PITCH", -64),
        "ATK": 0, "DEC": ms(330), "SUS": 0, "REL": ms(100),
        "INSERT": "HARD", "INS A": 127, "INS B": 90, "INS C": 90, "INS MIX": 127, "DIST": pct(38),
        "TRIM": 113,
    }),
    # ------------------------------------------------------------------------------------------------------------ FX
    dict(engine="WHEEL", idx=7, name="TAPE DRONE", cat="FX", bseed=63114, recipe="5.6 Slow drone + tape wow (a held organ chord on a TAPE insert)", p={
        # a slow organ chord (the CHAPEL registration, power chord, open), drive and the rotor off, on the TAPE insert: its WOW (a slow random
        # wandering of the delay line) and a soft saturation are the sound [G 5.6]; the wow kept at a moderate depth, a little chorus and reverb
        "REG": "CHAPL", "PERC": "OFF", "CLICK": 0, "DRV": 20, "ROTR": "OFF",
        "ATK": ms(1400), "SUS": 127, "REL": ms(2000),
        "INSERT": "TAPE", "INS A": 50, "INS B": 71, "INS C": 110, "INS MIX": 127,
        "CHRD": "POW", "VOIC": "OPEN", "CHO": pct(25), "REV": pct(50),
        "TRIM": 102,
    }),
]

# DUELL 0.2.2 (owner): the LFOs run FREE on every factory sound, both of them, set explicitly (not left to the default,
# params.c P_LTRIG / P_L2TRIG). A FADE restarts with each fresh phrase all the same (voice.c trk_note_on); only the PHASE is
# no longer put back at the key press. A sound that wants the key trigger sets "LFO TRIG": "NOTE" itself.
for _s in SOUNDS:
    _s["p"].setdefault("LFO TRIG", "FREE")
    _s["p"].setdefault("LFO2 TRIG", "FREE")

# DUELL 0.2.7 (owner): "spice up the original engines a little bit, with subtle modulation on about 21 % of the patches, with a
# maximum of 11 % random modulation, using the mod matrix destinations with the LFOs and envelopes". 16 of the 76 sounds on
# ANALOG FM6 PHYS WHEEL TRIO PHASE LOFI VOICE NOISE (sounds that hold or evolve) get one or two routes in the free slots 4 / 5:
# a slow LFO (0.1 .. 0.35 Hz, FREE) or ENV 2 on a tone control (filter, detune, the engine's brightness / vowel / colour knob);
# never PITCH or AMP (no vibrato, no tremolo). Each AMT is drawn from a fixed seed (SUBTLE_SEED: a rebuild gives the same
# numbers): 2..7 of 64, the MATRIX page's +3 .. +10 %. Nothing else of these sounds changes, but for the LFO's rate where the
# sound's LFO was not in use (its other settings stay the defaults: SIN, BI, no DEST depths) and a TRIM nudge where a route
# moved the audit's level (HAN SOLO, PAN WHISTLE); End B seeds re-searched where End B left its window. A bipolar LFO swings a
# 0..127 engine knob, CUT or SHP by about +-AMT steps around its value (SPEC 2.4a).
SUBTLE_SEED = 1717


def subtle(engine, idx, slot):
    """the route's AMT: 2..7 of 64, from SUBTLE_SEED and the route's place (engine, factory idx, slot), not the list's order"""
    return 2 + zlib.crc32(f"{SUBTLE_SEED}:{engine}:{idx}:{slot}".encode()) % 6


SUBTLE = [
    # (engine, idx, name, the LFO setup it needs (and a TRIM nudge where a route moved the level), [(slot, SRC, DST), ..])
    ("ANALOG", 2, "BEEHIVE", {"LFO2 RATE": hz(0.17)}, [(4, "LFO2", "DTN")]),            # the Reese's two saws drift
    ("ANALOG", 8, "WALL OF SAW", {"LFO2 RATE": hz(0.13)}, [(4, "LFO2", "CUT")]),
    ("ANALOG", 10, "EMBER GLOW", {"LFO2 RATE": hz(0.11)}, [(4, "LFO2", "CUT"), (5, "LFO2", "DTN")]),
    ("ANALOG", 11, "VELVET BOW", {"LFO RATE": hz(0.21)}, [(4, "LFO1", "CUT")]),          # (LFO 2 is its PWM)
    ("FM6", 1, "HAN SOLO", {"TRIM": 115}, [(4, "ENV2", "MLVL")]),                      # the brass index follows its ENV 2 (TRIM 117 -> 115: 1 dB louder with it)
    ("FM6", 2, "GLASS HALL", {}, [(4, "LFO2", "MLVL"), (5, "ENV2", "MLVL")]),           # its slow LFO 2 and ENV 2 (set, unused)
    ("PHYS", 0, "PAN WHISTLE", {"LFO RATE": hz(0.27), "TRIM": 97}, [(4, "LFO1", "BRIT")]),   # (LFO 2 is its vibrato; TRIM 99 -> 97: 1 dB louder with it)
    ("PHYS", 1, "SINGING BOWL", {}, [(4, "LFO2", "BRIT")]),                             # its slow LFO 2 (set, unused); (ENV 2 on BRIT
    #                                                                                     made the onset 6 dB louder: not subtle, left out)
    ("WHEEL", 3, "JAZZ CLUB", {"LFO2 RATE": hz(0.17)}, [(4, "LFO2", "SHP")]),           # SHP: the drawbar brightness
    ("TRIO", 2, "CHIP CHAPEL", {"LFO RATE": hz(0.31)}, [(4, "LFO1", "CUT")]),           # (LFO 2 is its PWM)
    ("PHASE", 2, "LAMPLIGHT", {}, [(4, "LFO2", "DCW"), (5, "ENV2", "DCW")]),            # its slow LFO 2 and ENV 2 (set, unused)
    ("LOFI", 2, "WOW FLUTTER", {}, [(4, "LFO2", "TONE")]),                              # its slow LFO 2 (set, unused)
    ("VOICE", 2, "VESPERS", {"LFO RATE": hz(0.13)}, [(4, "LFO1", "VOWL2")]),            # the vowel it settles on (TALK glides there); LFO 2 is its vibrato
    ("VOICE", 4, "GHOST VOICES", {}, [(4, "LFO2", "VOWL"), (5, "LFO2", "VOWL2")]),      # its 0.1 Hz LFO 2 (set, unused): both vowels
    ("NOISE", 2, "SEA ORGAN", {"LFO RATE": hz(0.21)}, [(4, "LFO1", "COLR")]),           # (LFO 2 is on its FREQ)
    ("NOISE", 4, "MOOR WIND", {"LFO RATE": hz(0.17)}, [(4, "LFO1", "RES")]),            # (LFO 2 is on its FREQ)
]
for _e, _k, _n, _setup, _routes in SUBTLE:
    _s = next(x for x in SOUNDS if x["engine"] == _e and x["idx"] == _k)
    assert _s["name"] == _n, (_e, _k, _n)
    for _id, _v in _setup.items():
        assert (_id == "TRIM") == (_id in _s["p"]), (_n, _id)   # (only an LFO the sound does not use; TRIM: the nudge)
        _s["p"][_id] = _v
    for _slot, _src, _dst in _routes:
        assert f"MOD{_slot}" not in _s["p"], (_n, _slot)     # (a free slot: the sound's own routes stay)
        _s["p"][f"MOD{_slot}"] = (_src, _dst, subtle(_e, _k, _slot))
