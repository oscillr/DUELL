# Conversion twin report (cloud job J)

Old build: `cloud/base3-wave4` (828d04b). New build: this branch. Same note, same hold, STEREO, the mix before MASTER, 48 kHz, as measured by
`tests/sound_audit.c` (`SOUND_DUMP`) and `tests/multi_render_test.c` (`MULTI_DUMP`). Compared by `tools/convert_twin_report.py`. Nothing was tuned.

Columns: peak and RMS are new minus old (dB). Centroid is the spectral centroid shift (Hz). Band max and band mean are the largest and the mean absolute
difference of the 1/3-octave band levels, over the bands within 40 dB of the loudest band of either render. Outlier: RMS change above 1.5 dB or a band above 3 dB.
Outliers are listed first in every table.

Counts: 128 factory sounds in the list. 43 TWIN sounds (ANALOG 19, TRIO 13, NOISE 11), 6 WAVE sounds with a SUB, 5 PHASE or WHEEL sounds with a SUB
that the conversion drops, 74 others (these only get filter 1's RES step, if filter 1 runs). The brief counted 85 "other" sounds, this list has 79 outside
the 43 + 6 (74 + the 5 SUB sounds). 35 multis (16 factory, 14 test scenes, 5 X scenes) at three faders: 105 renders, all 105 hash-changed.

## 1. The 43 TWIN sounds

twin: 43 compared, 10 outliers (RMS > 1.5 dB or a band > 3 dB)

| render | dPeak dB | dRMS dB | dCentroid Hz | band max dB | band mean dB | flag |
|---|---:|---:|---:|---:|---:|---|
| SEA ORGAN (NOISE) | -6.57 | -7.18 | -156 | 9.19 | 5.54 | OUTLIER |
| TIN TOLL (TRIO) | -0.34 | +0.00 | +11 | 8.02 | 0.58 | OUTLIER |
| REV CYMBAL (NOISE) | -0.05 | +0.79 | +1584 | 7.94 | 0.59 | OUTLIER |
| GRAVEL SNARE (TRIO) | +0.31 | -0.05 | +53 | 6.58 | 1.29 | OUTLIER |
| HORNET (NOISE) | +0.20 | +0.04 | -64 | 6.49 | 0.48 | OUTLIER |
| GEIGER (NOISE) | -0.66 | -1.94 | -58 | 5.22 | 2.35 | OUTLIER |
| STATIC SNARE (NOISE) | -2.02 | -2.45 | -340 | 3.95 | 1.77 | OUTLIER |
| MOOR WIND (NOISE) | -0.10 | -1.04 | -52 | 4.02 | 2.29 | OUTLIER |
| OBOE-ISH (TRIO) | +0.01 | +0.01 | +5 | 3.44 | 0.30 | OUTLIER |
| CHIP CHAPEL (TRIO) | -0.01 | -0.06 | +14 | 3.30 | 0.37 | OUTLIER |
| GEARBOX (NOISE) | +4.01 | +1.23 | -33 | 2.88 | 1.07 |  |
| STRING MACH (TRIO) | -0.01 | -0.00 | +1 | 2.80 | 0.20 |  |
| TIZZ HAT (NOISE) | -1.96 | -1.36 | -1800 | 2.76 | 0.54 |  |
| WHITE OUT (NOISE) | +0.14 | -0.14 | -68 | 2.08 | 0.29 |  |
| OPEN SIZZLE (NOISE) | -1.50 | -0.77 | -994 | 1.74 | 0.19 |  |
| SQUELCH (ANALOG) | +0.00 | -0.14 | +1 | 1.70 | 0.41 |  |
| PEBBLE POP (TRIO) | +0.02 | +0.00 | +3 | 1.47 | 0.14 |  |
| BREADBIN (TRIO) | -0.05 | -0.02 | -3 | 0.93 | 0.28 |  |
| ARP PLUCK (ANALOG) | -0.04 | -0.04 | -6 | 0.93 | 0.23 |  |
| SID BASS (TRIO) | +0.00 | -0.01 | -3 | 0.92 | 0.22 |  |
| STORM FRONT (ANALOG) | +0.02 | -0.00 | +1 | 0.91 | 0.07 |  |
| MODEM (NOISE) | +0.12 | +0.03 | -60 | 0.88 | 0.14 |  |
| EMBER GLOW (ANALOG) | -0.01 | -0.02 | -3 | 0.85 | 0.31 |  |
| FIELD CLANK (TRIO) | +0.03 | +0.02 | -0 | 0.75 | 0.04 |  |
| CIRCUIT BEND (TRIO) | -0.10 | -0.01 | -7 | 0.65 | 0.11 |  |
| SQUARE LEAD (ANALOG) | -0.02 | -0.01 | -3 | 0.61 | 0.18 |  |
| PIZZICATO (ANALOG) | -0.03 | -0.03 | -2 | 0.53 | 0.21 |  |
| FOLD BASS (ANALOG) | -0.02 | +0.00 | -1 | 0.51 | 0.14 |  |
| VELVET BOW (ANALOG) | -0.01 | +0.02 | +1 | 0.47 | 0.18 |  |
| SYNC BASS (TRIO) | -0.04 | -0.03 | -2 | 0.46 | 0.18 |  |
| FUNFARE (ANALOG) | +0.01 | -0.01 | -3 | 0.40 | 0.14 |  |
| RUBBER BAND (ANALOG) | -0.01 | +0.03 | +1 | 0.34 | 0.15 |  |
| LADDER BASS (ANALOG) | +0.02 | +0.06 | +1 | 0.32 | 0.19 |  |
| DIAL TONE (TRIO) | -0.00 | +0.02 | +0 | 0.31 | 0.12 |  |
| GLASS SHARDS (ANALOG) | +0.02 | -0.04 | -2 | 0.27 | 0.08 |  |
| SFORZANDO (ANALOG) | -0.01 | -0.01 | -1 | 0.25 | 0.11 |  |
| WALL OF SAW (ANALOG) | +0.00 | -0.00 | -1 | 0.25 | 0.04 |  |
| BEEHIVE (ANALOG) | +0.00 | +0.00 | +1 | 0.23 | 0.05 |  |
| RAZORWIRE (ANALOG) | +0.04 | -0.01 | -1 | 0.21 | 0.06 |  |
| TIDAL SUB (ANALOG) | +0.01 | +0.03 | +0 | 0.19 | 0.09 |  |
| BANSHEE (TRIO) | +0.00 | -0.01 | -0 | 0.10 | 0.03 |  |
| TORCH SONG (ANALOG) | +0.00 | -0.00 | -0 | 0.03 | 0.01 |  |
| SILK CHORUS (ANALOG) | +0.01 | +0.00 | +0 | 0.02 | 0.01 |  |

### Outliers, read

* NOISE loudness is expected: SEA ORGAN, STATIC SNARE, GEIGER, MOOR WIND, TIZZ HAT, OPEN SIZZLE, GEARBOX. The loudness make-up used to read the engine's own
  FREQ, it now reads filter 1's effective cutoff, and the conversion maps the cutoff by the dial formula. The differences are the owner's to retune.
* TRIO drift is expected: TIN TOLL, GRAVEL SNARE, OBOE-ISH, CHIP CHAPEL (and the small ones below the line). The TRIO filter became filter 2 (series, after
  the 12 dB filter 1 at the open end), whose cutoff law and drift are the multimode ones.
* REV CYMBAL, HORNET are NOISE sounds with a band difference only (RMS within 0.8 dB): the same cause.

## 2. The 6 WAVE sounds (SUB converted to SUB LAYER, SINE, octave -1, matched level)

wave: 6 compared, 0 outliers (RMS > 1.5 dB or a band > 3 dB)

| render | dPeak dB | dRMS dB | dCentroid Hz | band max dB | band mean dB | flag |
|---|---:|---:|---:|---:|---:|---|
| EVOLVING PAD (WAVE) | +0.01 | +0.03 | -1 | 0.25 | 0.03 |  |
| KICK BASS (WAVE) | -0.03 | +0.00 | +0 | 0.10 | 0.03 |  |
| GROWL BASS (WAVE) | +0.00 | +0.02 | -3 | 0.08 | 0.01 |  |
| DRAWBARS (WAVE) | -0.02 | +0.03 | -0 | 0.06 | 0.02 |  |
| REESE BASS (WAVE) | -0.01 | -0.01 | +0 | 0.04 | 0.01 |  |
| HARM BASS (WAVE) | -0.03 | -0.01 | +0 | 0.02 | 0.00 |  |

The level match holds: RMS within 0.03 dB, bands within 0.25 dB.

## 3. PHASE and WHEEL sounds that had a SUB value (the knob is gone)

These are dropped without a SUB LAYER (the brief: PHASE and WHEEL sounds are retuned by the owner). The SUB values are listed so the owner can set the SUB LAYER:

* WHEEL PEDAL BASS: SUB 0 dropped (the knob is gone; retune with the SUB LAYER)
* WHEEL CHOIR LOFT: SUB 1 dropped (the knob is gone; retune with the SUB LAYER)
* PHASE ELASTIC BASS: SUB 57 dropped (the knob is gone; retune with the SUB LAYER)
* PHASE BRAZEN: SUB 0 dropped (the knob is gone; retune with the SUB LAYER)
* PHASE LAMPLIGHT: SUB 19 dropped (the knob is gone; retune with the SUB LAYER)
* PHASE WIRE HARP: SUB 13 dropped (the knob is gone; retune with the SUB LAYER)
* PHASE CLAVICLE: SUB 0 dropped (the knob is gone; retune with the SUB LAYER)
* PHASE CRYSTAL CAVE: SUB 0 dropped (the knob is gone; retune with the SUB LAYER)
* PHASE DEEP KICK: SUB 0 dropped (the knob is gone; retune with the SUB LAYER)
* PHASE SONAR PING: SUB 0 dropped (the knob is gone; retune with the SUB LAYER)
* PHASE FAKE KOTO: SUB 0 dropped (the knob is gone; retune with the SUB LAYER)
* PHASE FUZZ BASS: SUB 48 dropped (the knob is gone; retune with the SUB LAYER)
* PHASE HARD KICK: SUB 0 dropped (the knob is gone; retune with the SUB LAYER)

phsub: 5 compared, 4 outliers (RMS > 1.5 dB or a band > 3 dB)

| render | dPeak dB | dRMS dB | dCentroid Hz | band max dB | band mean dB | flag |
|---|---:|---:|---:|---:|---:|---|
| ELASTIC BASS (PHASE) | -1.79 | -1.73 | +182 | 60.90 | 6.09 | OUTLIER |
| FUZZ BASS (PHASE) | -1.58 | -0.53 | +144 | 58.68 | 10.93 | OUTLIER |
| CHOIR LOFT (WHEEL) | -0.21 | +0.25 | +9 | 17.14 | 4.12 | OUTLIER |
| WIRE HARP (PHASE) | +0.49 | -0.18 | +21 | 8.80 | 1.71 | OUTLIER |
| LAMPLIGHT (PHASE) | -0.35 | -0.05 | +6 | 1.76 | 0.41 |  |

## 4. The other sounds

rest: 74 compared, 2 outliers (RMS > 1.5 dB or a band > 3 dB)

| render | dPeak dB | dRMS dB | dCentroid Hz | band max dB | band mean dB | flag |
|---|---:|---:|---:|---:|---:|---|
| VOWEL PAD (WAVE) | -0.45 | -0.31 | +61 | 6.60 | 0.88 | OUTLIER |
| RESO SWEEP (WAVE) | -0.15 | -0.23 | +32 | 6.47 | 0.65 | OUTLIER |
| FAUX HARPSI (PHYS) | +0.00 | -0.02 | +3 | 0.47 | 0.08 |  |
| TEMPLE BLOCK (FM6) | +0.01 | +0.00 | +0 | 0.28 | 0.06 |  |
| CROONER (VOICE) | -0.01 | -0.00 | -0 | 0.25 | 0.02 |  |
| VOCAL BEATS (VOICE) | -0.00 | -0.00 | +1 | 0.24 | 0.05 |  |
| MARBLE RUN (LOFI) | -0.00 | +0.00 | +0 | 0.12 | 0.02 |  |
| PLATE BELL (WAVE) | +0.00 | -0.00 | +0 | 0.11 | 0.01 |  |
| DIGI PLUCK (WAVE) | -0.00 | +0.00 | +0 | 0.08 | 0.02 |  |
| NYLON GUT (PHYS) | +0.01 | -0.00 | +0 | 0.05 | 0.00 |  |
| MALLET WOOD (FM6) | +0.00 | -0.00 | +0 | 0.04 | 0.01 |  |
| PURPLE FLUTE (WAVE) | -0.02 | -0.00 | -0 | 0.04 | 0.02 |  |
| ISLAND STEEL (FM6) | -0.03 | -0.00 | -0 | 0.04 | 0.01 |  |
| DEAD BATTERY (LOFI) | -0.00 | -0.00 | -0 | 0.03 | 0.01 |  |
| TUBE BELLS (FM6) | -0.00 | -0.00 | +0 | 0.03 | 0.01 |  |
| PAN WHISTLE (PHYS) | +0.03 | -0.00 | -0 | 0.03 | 0.01 |  |
| FROZEN AIR (WAVE) | -0.01 | -0.00 | -1 | 0.03 | 0.01 |  |
| FAKE KOTO (PHASE) | -0.00 | +0.00 | +0 | 0.03 | 0.00 |  |
| SINGING BOWL (PHYS) | +0.00 | -0.00 | +0 | 0.02 | 0.01 |  |
| SCREAMER (WAVE) | -0.00 | +0.00 | +0 | 0.02 | 0.00 |  |
| NIGHT BOX (WHEEL) | -0.00 | -0.00 | +0 | 0.02 | 0.01 |  |
| ROSEWOOD (PHYS) | -0.00 | +0.00 | +0 | 0.02 | 0.01 |  |
| DOO WOP (VOICE) | +0.00 | -0.00 | +0 | 0.02 | 0.00 |  |
| WAVE CHIMES (WAVE) | +0.00 | +0.00 | -0 | 0.01 | 0.00 |  |
| NURSERY KEYS (PHYS) | -0.00 | -0.00 | -0 | 0.01 | 0.00 |  |
| RAIN CHIMES (PHYS) | -0.00 | -0.00 | +0 | 0.01 | 0.00 |  |
| GLASS PAD (WAVE) | +0.01 | -0.00 | -0 | 0.01 | 0.00 |  |
| SYNTH BRASS (WAVE) | +0.01 | -0.00 | -0 | 0.01 | 0.00 |  |
| PWM PAD (WAVE) | -0.00 | -0.00 | -0 | 0.01 | 0.00 |  |
| PLATE CHOIR (VOICE) | +0.00 | -0.00 | -0 | 0.01 | 0.00 |  |
| SONAR PING (PHASE) | -0.00 | -0.00 | +0 | 0.01 | 0.00 |  |
| DIGITAL PAD (WAVE) | +0.01 | -0.00 | -0 | 0.01 | 0.00 |  |
| WOW FLUTTER (LOFI) | -0.00 | -0.00 | +0 | 0.01 | 0.00 |  |
| DUSTY DRONE (LOFI) | +0.00 | -0.00 | -0 | 0.01 | 0.00 |  |
| WHISPER PAD (VOICE) | -0.00 | -0.00 | -0 | 0.01 | 0.00 |  |
| DIVE BOMB (LOFI) | +0.01 | -0.00 | -0 | 0.01 | 0.00 |  |
| FROST BELLS (FM6) | -0.01 | -0.00 | +0 | 0.01 | 0.00 |  |
| BRASS PAD (WAVE) | -0.00 | -0.00 | -0 | 0.01 | 0.00 |  |
| PIXEL HERO (LOFI) | +0.01 | -0.00 | -0 | 0.01 | 0.00 |  |
| JAZZ CLUB (WHEEL) | -0.00 | -0.00 | -0 | 0.01 | 0.00 |  |
| CRYSTAL CAVE (PHASE) | +0.00 | -0.00 | +0 | 0.01 | 0.00 |  |
| HOOVER (WAVE) | -0.01 | -0.00 | +0 | 0.01 | 0.00 |  |
| TAPE DRONE (WHEEL) | -0.00 | -0.00 | +0 | 0.01 | 0.00 |  |
| GHOST VOICES (VOICE) | +0.00 | -0.00 | -0 | 0.01 | 0.00 |  |
| ALIEN CHIMES (FM6) | -0.00 | -0.00 | -0 | 0.01 | 0.00 |  |
| VESPERS (VOICE) | -0.00 | -0.00 | +0 | 0.01 | 0.00 |  |
| SYNC LEAD (WAVE) | +0.00 | -0.00 | -0 | 0.01 | 0.00 |  |
| TINE ROOM (FM6) | +0.00 | +0.00 | -0 | 0.00 | 0.00 |  |
| MOTOR VIBES (PHYS) | +0.00 | -0.00 | +0 | 0.01 | 0.00 |  |
| BIG STAB (FM6) | +0.00 | -0.00 | +0 | 0.01 | 0.00 |  |
| CARNIVAL (WHEEL) | +0.01 | -0.00 | +0 | 0.01 | 0.00 |  |
| LESLIE STORM (WHEEL) | -0.00 | -0.00 | -0 | 0.01 | 0.00 |  |
| TABLA TALK (PHYS) | -0.00 | -0.00 | -0 | 0.01 | 0.00 |  |
| HAN SOLO (FM6) | +0.00 | -0.00 | -0 | 0.00 | 0.00 |  |
| GLASS HALL (FM6) | -0.00 | -0.00 | -0 | 0.00 | 0.00 |  |
| BREATH PAD (WAVE) | -0.01 | -0.00 | +0 | 0.00 | 0.00 |  |
| FM BASS 2 (FM6) | +0.00 | -0.00 | -0 | 0.00 | 0.00 |  |
| BRAZEN (PHASE) | +0.00 | -0.00 | -0 | 0.00 | 0.00 |  |
| SALOON KEYS (FM6) | +0.00 | -0.00 | -0 | 0.00 | 0.00 |  |
| GHOST HARP (PHYS) | -0.00 | -0.00 | +0 | 0.00 | 0.00 |  |
| SMOKE RING (WHEEL) | -0.01 | -0.00 | -0 | 0.00 | 0.00 |  |
| LICORICE (WAVE) | -0.02 | -0.00 | +0 | 0.00 | 0.00 |  |
| CLAVICLE (PHASE) | -0.01 | +0.00 | -0 | 0.00 | 0.00 |  |
| SLAP DOCTOR (FM6) | +0.00 | +0.00 | +0 | 0.00 | 0.00 |  |
| FRETLESS (FM6) | +0.00 | +0.00 | +0 | 0.00 | 0.00 |  |
| HARD KICK (PHASE) | +0.00 | +0.00 | +0 | 0.00 | 0.00 |  |
| DEEP KICK (PHASE) | +0.00 | +0.00 | +0 | 0.00 | 0.00 |  |
| CARTRIDGE (LOFI) | +0.00 | +0.00 | +0 | 0.00 | 0.00 |  |
| PIXEL KICK (LOFI) | +0.00 | +0.00 | +0 | 0.00 | 0.00 |  |
| ARP BASS (LOFI) | +0.00 | +0.00 | +0 | 0.00 | 0.00 |  |
| BULLFROG (VOICE) | +0.00 | +0.00 | +0 | 0.00 | 0.00 |  |
| PEDAL BASS (WHEEL) | +0.00 | +0.00 | +0 | 0.00 | 0.00 |  |
| PLUCK BASS (PHYS) | +0.00 | +0.00 | +0 | 0.00 | 0.00 |  |
| FLOOR THUD (PHYS) | +0.00 | +0.00 | +0 | 0.00 | 0.00 |  |

VOWEL PAD and RESO SWEEP (WAVE) carry a route LFO 1 to SUB: it now moves SUB LEVEL by the same amount, which is a different law than WAVE's own SUB knob.

## 5. Multis (16 factory, 14 test scenes, 5 X scenes, faders 0 / 63 / 127)

multis: 105 compared, 71 outliers (RMS > 1.5 dB or a band > 3 dB)

Every multi hash changed: all have at least one part with filter 1 in use (the RES step), most have ANALOG, TRIO, NOISE or WAVE parts. The band columns of
the multis are large because a multi is long, has arps and LFOs that come and go, and a band that is quiet in one render is compared with the same band in
the other: read the RMS and peak columns first.

| render | dPeak dB | dRMS dB | dCentroid Hz | band max dB | band mean dB | flag |
|---|---:|---:|---:|---:|---:|---|
| S08_GARGLE_ACID_f63 | -1.86 | +0.73 | +367 | 45.52 | 13.79 | OUTLIER |
| S08_GARGLE_ACID_f127 | -1.73 | +0.57 | +339 | 45.17 | 12.87 | OUTLIER |
| S08_GARGLE_ACID_f0 | -1.59 | +0.53 | +423 | 42.55 | 13.83 | OUTLIER |
| S01_REESE_SWAMP_f0 | -1.68 | -0.47 | +268 | 40.97 | 11.93 | OUTLIER |
| S07_COLD_CHAPEL_f63 | -0.25 | -0.10 | +7 | 40.53 | 2.75 | OUTLIER |
| S01_REESE_SWAMP_f63 | -1.18 | -0.16 | +225 | 40.36 | 10.29 | OUTLIER |
| S09_NIGHT_ORGAN_f0 | -0.44 | +1.36 | +329 | 40.00 | 19.86 | OUTLIER |
| S01_REESE_SWAMP_f127 | -0.33 | -0.13 | +214 | 39.96 | 8.06 | OUTLIER |
| S09_NIGHT_ORGAN_f63 | +0.00 | +0.97 | +274 | 37.43 | 17.88 | OUTLIER |
| S07_COLD_CHAPEL_f0 | +0.06 | -0.08 | -11 | 37.42 | 4.88 | OUTLIER |
| S14_TEST_2_PHASE_f0 | +0.41 | +0.79 | +260 | 35.62 | 7.61 | OUTLIER |
| S09_NIGHT_ORGAN_f127 | -0.05 | +0.85 | +305 | 35.40 | 16.62 | OUTLIER |
| S10_FINAL_REEL_f127 | +0.99 | +0.59 | +39 | 33.79 | 2.72 | OUTLIER |
| X07_BOWL_AND_WIND_f0 | +1.17 | +1.00 | +360 | 31.59 | 5.81 | OUTLIER |
| X06_TINE_STRINGS_f0 | -0.00 | -0.08 | +93 | 30.40 | 6.36 | OUTLIER |
| S07_COLD_CHAPEL_f127 | -0.24 | -0.24 | +27 | 30.36 | 2.57 | OUTLIER |
| S13_TEST_1_BASE_f0 | +0.51 | -0.09 | +166 | 30.04 | 9.20 | OUTLIER |
| S10_FINAL_REEL_f63 | +0.90 | +0.88 | +35 | 28.94 | 3.74 | OUTLIER |
| X06_TINE_STRINGS_f63 | +0.42 | -0.04 | +86 | 28.68 | 6.79 | OUTLIER |
| M13_f127 | -0.16 | -0.13 | +35 | 28.46 | 2.46 | OUTLIER |
| X08_MONO_DUEL_f0 | +0.13 | +0.49 | +258 | 26.82 | 5.44 | OUTLIER |
| X08_MONO_DUEL_f127 | +0.15 | +0.30 | +214 | 25.56 | 3.88 | OUTLIER |
| S14_TEST_2_PHASE_f63 | -0.20 | +0.07 | +84 | 25.52 | 4.18 | OUTLIER |
| M05_f127 | -0.54 | -0.25 | +59 | 25.36 | 3.40 | OUTLIER |
| S04_WIRED_SNAIL_f0 | -0.09 | -0.49 | +272 | 23.58 | 4.57 | OUTLIER |
| M13_f63 | +0.21 | -0.05 | +19 | 22.16 | 1.94 | OUTLIER |
| S04_WIRED_SNAIL_f63 | -0.06 | -0.05 | +68 | 22.08 | 3.91 | OUTLIER |
| X08_MONO_DUEL_f63 | +0.14 | +0.39 | +214 | 21.44 | 4.29 | OUTLIER |
| S13_TEST_1_BASE_f63 | +0.12 | +0.01 | +43 | 19.96 | 3.79 | OUTLIER |
| M05_f63 | -0.12 | -0.07 | +13 | 18.73 | 2.59 | OUTLIER |
| M16_f127 | -0.04 | -0.27 | +241 | 18.10 | 3.77 | OUTLIER |
| S14_TEST_2_PHASE_f127 | -0.02 | -0.10 | +28 | 18.09 | 3.45 | OUTLIER |
| M12_f127 | +0.07 | -0.03 | -8 | 17.98 | 1.56 | OUTLIER |
| M06_f63 | +0.00 | -1.43 | -49 | 17.44 | 5.10 | OUTLIER |
| M14_f127 | +0.52 | +1.35 | +5 | 17.33 | 2.65 | OUTLIER |
| S10_FINAL_REEL_f0 | +1.33 | +0.43 | +177 | 17.19 | 2.41 | OUTLIER |
| S06_ACID_ETHER_f127 | +0.43 | +0.40 | +214 | 16.50 | 4.20 | OUTLIER |
| X06_TINE_STRINGS_f127 | -0.06 | +0.44 | +25 | 14.86 | 2.52 | OUTLIER |
| M06_f127 | -0.03 | -1.41 | +19 | 14.80 | 6.05 | OUTLIER |
| S02_WHISPER_TIDE_f127 | -0.18 | -0.39 | -53 | 14.43 | 2.33 | OUTLIER |
| S06_ACID_ETHER_f0 | -0.36 | +2.85 | +265 | 13.37 | 5.73 | OUTLIER |
| M12_f63 | -0.05 | -0.01 | -14 | 13.26 | 0.92 | OUTLIER |
| S16_TEST_4_MIX_f0 | -0.09 | +0.14 | -110 | 13.12 | 5.40 | OUTLIER |
| X07_BOWL_AND_WIND_f127 | +0.10 | -0.14 | -97 | 13.09 | 4.60 | OUTLIER |
| S13_TEST_1_BASE_f127 | -0.05 | -0.03 | +17 | 12.93 | 1.75 | OUTLIER |
| S05_DEAD_BUFFER_f127 | -0.51 | -0.03 | -2 | 12.56 | 1.39 | OUTLIER |
| S04_WIRED_SNAIL_f127 | -0.03 | +0.30 | -35 | 11.77 | 2.00 | OUTLIER |
| M16_f63 | +0.08 | -0.08 | +72 | 10.84 | 1.94 | OUTLIER |
| M01_f127 | +0.14 | +1.30 | -31 | 10.41 | 2.26 | OUTLIER |
| S02_WHISPER_TIDE_f63 | +0.02 | -0.11 | -31 | 9.07 | 1.34 | OUTLIER |
| S06_ACID_ETHER_f63 | +0.17 | +0.70 | +164 | 8.93 | 2.90 | OUTLIER |
| X13_BASILDON_f127 | +0.13 | -0.01 | +8 | 8.64 | 1.58 | OUTLIER |
| M14_f63 | +0.36 | +0.37 | +27 | 8.35 | 1.01 | OUTLIER |
| S16_TEST_4_MIX_f63 | +0.34 | -0.01 | +137 | 8.20 | 3.68 | OUTLIER |
| M07_f127 | -1.35 | -0.78 | -166 | 7.77 | 2.51 | OUTLIER |
| S16_TEST_4_MIX_f127 | -0.02 | -0.17 | +202 | 7.49 | 2.94 | OUTLIER |
| S05_DEAD_BUFFER_f63 | -0.16 | -0.02 | -14 | 7.13 | 0.52 | OUTLIER |
| M08_f0 | -0.31 | -0.14 | +6 | 7.09 | 1.40 | OUTLIER |
| X07_BOWL_AND_WIND_f63 | +0.29 | -0.06 | +3 | 7.05 | 2.36 | OUTLIER |
| M03_f63 | -0.06 | +0.07 | +8 | 6.16 | 0.98 | OUTLIER |
| M03_f127 | +0.27 | +0.27 | +4 | 6.07 | 1.21 | OUTLIER |
| M09_f127 | +0.30 | -0.25 | +70 | 5.59 | 1.37 | OUTLIER |
| M07_f63 | -0.27 | -0.30 | +2 | 4.75 | 1.11 | OUTLIER |
| M10_f127 | -0.08 | -1.52 | -75 | 4.65 | 1.89 | OUTLIER |
| M01_f63 | +0.07 | +0.43 | -6 | 4.46 | 0.72 | OUTLIER |
| X13_BASILDON_f63 | +0.13 | -0.01 | +4 | 3.97 | 0.55 | OUTLIER |
| M08_f63 | -0.04 | -0.06 | -25 | 3.88 | 1.18 | OUTLIER |
| M16_f0 | +0.01 | -0.00 | +1 | 3.61 | 0.22 | OUTLIER |
| M02_f127 | +0.12 | +0.42 | -13 | 3.20 | 0.64 | OUTLIER |
| X14_SEVENOAKS_f63 | +0.01 | -0.00 | +9 | 3.16 | 0.42 | OUTLIER |
| X14_SEVENOAKS_f127 | -0.04 | +0.01 | +3 | 3.05 | 0.45 | OUTLIER |
| M08_f127 | -0.10 | +0.11 | -54 | 2.88 | 1.30 |  |
| X13_BASILDON_f0 | -0.02 | -0.01 | +4 | 2.57 | 0.34 |  |
| M10_f63 | +0.02 | -0.44 | -0 | 2.45 | 1.03 |  |
| M11_f127 | +0.00 | -0.89 | +11 | 2.42 | 1.13 |  |
| X14_SEVENOAKS_f0 | +0.00 | +0.01 | +3 | 2.30 | 0.29 |  |
| S03_EERIE_SIGNAL_f127 | +0.05 | -0.44 | +20 | 2.10 | 1.19 |  |
| S15_TEST_3_HEAVY_f0 | +0.00 | -0.03 | -39 | 1.98 | 0.25 |  |
| S03_EERIE_SIGNAL_f63 | -0.12 | -0.17 | -3 | 1.89 | 0.42 |  |
| M11_f63 | -0.01 | -0.15 | -2 | 1.76 | 0.38 |  |
| M11_f0 | -0.01 | -0.03 | -6 | 1.76 | 0.32 |  |
| S03_EERIE_SIGNAL_f0 | -0.04 | +0.01 | -5 | 1.65 | 0.30 |  |
| M09_f63 | -0.02 | +0.06 | +6 | 1.51 | 0.63 |  |
| M03_f0 | -0.03 | -0.02 | -5 | 1.30 | 0.20 |  |
| S15_TEST_3_HEAVY_f63 | -0.04 | -0.02 | -25 | 1.28 | 0.28 |  |
| M06_f0 | +0.08 | +0.22 | +5 | 0.99 | 0.42 |  |
| M13_f0 | +0.04 | +0.01 | -2 | 0.96 | 0.12 |  |
| S15_TEST_3_HEAVY_f127 | -0.12 | -0.07 | -2 | 0.85 | 0.34 |  |
| S05_DEAD_BUFFER_f0 | +0.65 | +0.02 | +10 | 0.73 | 0.10 |  |
| M02_f0 | +0.00 | -0.00 | -0 | 0.71 | 0.08 |  |
| S02_WHISPER_TIDE_f0 | -0.06 | -0.03 | -5 | 0.55 | 0.11 |  |
| M10_f0 | -0.01 | -0.04 | +1 | 0.40 | 0.07 |  |
| M09_f0 | +0.04 | +0.01 | +1 | 0.33 | 0.06 |  |
| M01_f0 | +0.00 | +0.01 | +1 | 0.29 | 0.03 |  |
| M14_f0 | -0.03 | +0.07 | -4 | 0.29 | 0.05 |  |
| M07_f0 | +0.09 | +0.04 | +4 | 0.29 | 0.09 |  |
| M04_f0 | +0.01 | +0.02 | +0 | 0.16 | 0.05 |  |
| M02_f63 | -0.00 | +0.03 | -1 | 0.15 | 0.06 |  |
| M04_f127 | -0.00 | +0.02 | +0 | 0.14 | 0.04 |  |
| M04_f63 | +0.02 | +0.01 | +0 | 0.08 | 0.03 |  |
| M15_f127 | -0.02 | -0.00 | -0 | 0.04 | 0.01 |  |
| M05_f0 | +0.01 | -0.00 | -0 | 0.03 | 0.00 |  |
| M12_f0 | +0.00 | -0.00 | -0 | 0.02 | 0.00 |  |
| M15_f63 | -0.01 | -0.00 | -0 | 0.02 | 0.00 |  |
| M15_f0 | +0.00 | +0.00 | +0 | 0.00 | 0.00 |  |

## 6. Conversion notes and the cases the owner decides

* M03 part 2 (ANALOG): converted; filter 1 was in use as well as the VCF: the VCF is filter 2 (LP), filter 1 is as it was
* M08 part 1 (ANALOG): converted; filter 1 was in use as well as the VCF: the VCF is filter 2 (LP), filter 1 is as it was
* M08 part 2 (NOISE): CONFLICT: filter 1 (CUT 73, MODE 59) was in use as well as the engine's filter; the owner decides

M08 part 2 (NOISE with filter 1 in use as well as the engine's filter) is a CONFLICT: the tool reports it and leaves it as it was. Filter 1 stays what it was, the NOISE
filter is gone, so M08 part 2 now plays filter 1 only. The owner decides.
