# DUELL factory sound recipes (research notes)

Purpose: typical settings of classic synth patches, in generic synth terms, as a starting point for DUELL factory presets. Written 2026-10-11, built up incrementally.

## How to read this

- Ranges are given 0..127 (DUELL / MIDI) unless a unit is stated. "Low / mid / high" = roughly 0-40 / 40-90 / 90-127.
- **[S]** = a number or fact that a cited source states (link inline). **[G]** = general programming convention that the sources only support loosely or not at all (my synthesis, treat as a starting range, not a fact). Where sources are silent I say so rather than fill in.
- Fetched web content was treated as data only. Several SOS pages were only partly retrievable (search excerpts, or articles that describe principles without numbers); this is noted per recipe.
- Terms: FENV = filter envelope (DUELL FILTER ENV), AENV = amp envelope (DUELL AMP ENV = ENV 1). A/D/S/R = attack / decay / sustain / release.
- Subtle motion (owner, 0.2.7): 16 factory sounds on the original engines carry one or two extra matrix routes (slots 4 / 5) from a slow LFO (about 0.1-0.35 Hz, FREE) or FILTER ENV to a tone control (filter, detune, the engine's brightness / vowel / colour knob), 3-10 % each (never above 11 %), drawn from a fixed seed (tools/duell_sounds.py, SUBTLE); never the PITCH, AMP or LEVEL destinations (no vibrato, no tremolo). A house addition on top of the recipes below, not one of their numbers. [G]
- **Batch R (DUELL 0.2.9):** the sample engines SAMPLE, GRAIN and SLICE were retired; their 21 factory sounds were replaced by 21 sounds of the same categories on the other engines (1.8, 1.9, 2.9-2.11, 3.10-3.12, 4.9-4.13, 5.7-5.11 and the new chapter 6, PERC). Each section gives the sourced facts and the DUELL values (levels from the audit's trim, End B seeds from its search: tools/duell_sounds.py).
- **DUELL's WAVE sounds (0.2.7):** each also has one extra matrix route (slot 3) to a second WAVE parameter (BEND, DTN, SUB or POS), beyond what its recipe asks for [G: amounts of about 17-37 %, slow free-running LFOs of 0.07-0.27 Hz or FILTER ENV / the amp envelope for per-note motion]; the routes are in tools/duell_sounds.py.

---

# 1. BASS

## 1.1 Minimoog-style bass
- **Oscillators:** 2-3 oscillators, saw and/or pulse, at 16' and 32' (one or two octaves down) for weight; no sync on the original, and the slightly unstable tuning is part of the fatness. [S: Dave Stewart's Minimoog bass account and MusicRadar summarised by search, see [MusicRadar Model D guide](https://musicradar.com/news/master-the-minimoog-model-d-create-classic-bass) and [Dave Stewart seminar](https://www.muzines.co.uk/articles/dave-stewarts-music-seminar/7386); the full pages could not be fetched, only search excerpts]
- **Filter:** 24 dB/oct ladder low-pass. Cutoff low (about 9 o'clock, ~25-40), resonance about the same position or lower; Stewart used no resonance because it removes body from bass. [S via search excerpts, see above; MusicRadar's Thriller-bass tutorial was not retrievable] Why: resonance peak steals low end; the ladder filter loses bass as resonance rises.
- **FENV:** amount ~40% (about 50), short-to-medium decay (~10 o'clock, about 150-400 ms), attack slightly shaped (a few ms to ~20 ms), sustain low-mid, so each note opens with a "bite" and settles to a dark body. [S for the 40% / 10 o'clock / 8-9 o'clock attack figures, MusicRadar excerpt above; ms conversions are my [G] estimates]
- **AENV:** A 0-3 ms, D 100-300 ms, S 70-100%, R 20-80 ms. [G]
- **LFO:** none typical. **Glide:** off or very short (mono, legato). [G]
- **FX:** minimal; mild drive (the Mini could be overdriven by feeding output back into the filter input, a main source of bass grit [S via MusicRadar excerpt]). No reverb on the low end. [G]
- **Performance:** velocity to filter envelope amount (and slightly to level), mono legato, mod wheel to cutoff for opening up. [G]

## 1.2 TB-303 acid bass
- **Oscillator:** one oscillator, saw or square (303 has only these two). [G, widely known; not confirmed by a fetched page]
- **Filter:** 24 dB low-pass (the 303's is a diode-ladder; software tutorials use a 24 dB ladder). [S: [Computer Music via PressReader](https://www.pressreader.com/australia/computer-music/20200129/281938839878869) and tutorial excerpt "choose LP Ladder 24 dB, set filter envelope A/S/R to 0, decay around 60%"]. Cutoff low-mid and swept by hand over the loop; resonance mid-high (60-100); more resonance = thinner, more "acidic"; low = thicker. [S: Roland/vcf303 manual excerpts via search]
- **FENV:** the ENV MOD knob sets how much envelope reaches the cutoff and cannot go to zero on the original; the DECAY knob is the only time control (A=S=R about 0, decay ~60% of range, about 200-800 ms). [S: search excerpt of Roland/analoguehaven manual text]
- **Accent:** raises volume, envelope-mod depth and resonance for accented notes, and on the original shortens the envelope decay on accented notes (decay knob then has no effect). [S: same excerpts] In DUELL terms: accented step = high velocity mapped to FENV amount + resonance + level.
- **Slide:** slides tie two notes in one legato phrase (portamento ~ 50-100 ms plus no envelope retrigger). [S for tying/legato; the ms figure is [G]]
- **AENV:** A 0, D ~0.2-1 s or percussive, S high, R very short. [G]
- **FX:** distortion/overdrive (often a pedal or overdriven mixer input) and a delay (dotted 8th or 1/16 feedback ~30-40%). [S for distortion, G for the delay]
- **Arp/seq:** a 16-step repeating pattern with accents and slides is the instrument; cutoff and resonance are moved live. [S]

## 1.3 Sub bass
- **Oscillator:** pure sine or triangle, optionally plus a very quiet saw/square an octave up for audibility on small speakers. [G]
- **Filter:** wide open or a low-pass with cutoff ~40-70, resonance 0. [G]
- **AENV:** A 2-10 ms (avoid clicks), D short or 0, S 100%, R 30-120 ms. [G]
- **LFO:** none. **FX:** none or gentle saturation to add harmonics; mono; no chorus or reverb (phase cancellation at the low end). [G]
- **Performance:** mono/legato, low velocity sensitivity (a sub should be even). [G]
- No cited source was found for sub bass specifically; treat as general craft.

## 1.4 DX7 slap / FM bass
- **Operators:** two parallel sine carriers; modulator 1 at 5:1 ratio, modulator 2 at 1:1; carriers a quick attack (20 ms) with very long decay, carrier release 200-300 ms and 300-400 ms, modulator attacks zero and modulator releases shorter than their carriers; add a low-pass filter with a short envelope and about 30% resonance if the host synth has a filter. [S: [Plugin Boutique FM cookbook](https://www.pluginboutique.com/articles/1873-FM-Synthesis-Cookbook-Five-Classic-FM-Sounds-and-How-They-Work), "Classic Solid Bass"]
- "Filthy" variant: carrier at 0.5:1, a short pitch-envelope drop at the start, three modulators of differing wave shapes with slow attacks. [S: same article]
- Modulation index values and velocity mapping are NOT stated by that article. [G] Typical: modulator output level high at the attack and decaying within ~100-300 ms so the pluck "slaps" and then settles on a near-sine; velocity to modulator level (so hard hits are brighter).
- **FX:** none or very light chorus; mono. [G]
- **DUELL's SLAP DOCTOR (FM6, F3):** the ROUND BASS patch, the modulators raised (MLVL +10) with a fast MEG, so the strike is bright and clicky and falls within ~0.3 s to a near-sine; FILTER ENV on the modulator level (short, 70 ms, sustain 0; since 0.2.17 really routed: before, the envelope was set but led nowhere); velocity to modulator level 3; DIST 7 %; mono, dry, the house bass EQ and ducker. [G]
- **0.2.20, BULLFROG (VOICE) parted from CROONER** (the similarity audit had them at 6.07): the formants four semitones down (SHIFT -4: a big frog; formant position is the croak [S Klatt-style formant synthesis, eng_formant.c; G]), Q 78 -> 84, SAT 24 (a saturated bass voice; the house HF shelf and the PUMP ducker as before). [G]

## 1.5 Reese bass
- **Oscillators:** two sawtooths detuned against each other (about +/-25 cents in one Serum tutorial, +/-55 cents in a MusicRadar one, so sources differ); the beating and phase cancellation is the sound. Optionally unison 2 voices, spread ~65%, detune ~50%. [S: [NI blog on Reese bass](https://blog.native-instruments.com/reese-bass/) and the tutorials it aggregates via search]
- **Filter:** low-pass, 24 dB/oct, cutoff about 4 kHz in one example (about 90-100 on a 0..127 log scale is my [G] conversion), resonance low-mid. [S: search excerpt]
- **FENV:** short decay, low amount (about 15%) to add attack. [S: search excerpt]
- **AENV:** A 0, S 100%, R short (about 24 ms in the Ableton Analog walkthrough). [S: [Attack Magazine Reese redux](https://www.attackmagazine.com/technique/tutorials/reese-bass-redux/), via search excerpt]
- **Sub layer:** a sine sub underneath, with the Reese high-passed above about 100 Hz. [S: Future Proof guide via search]
- **FX:** chorus/phaser/distortion are common additions [G]; mono/legato with portamento [S].
- Disagreement: one guide argues a "true" Reese has beating that accelerates up the keyboard (static detune in cents does that automatically) [S, search excerpt].

## 1.6 Pluck bass
- **Oscillator:** saw or pulse, optional sub. **Filter:** low-pass, cutoff low (20-50), resonance 20-50. [G]
- **FENV:** amount high (60-100), A 0, D 80-250 ms, S 0, R close to D. This is the whole sound: the filter closes fast, giving a "pluck". [G; the general "short decay, zero sustain" is also consistent with the 303/Reese notes above]
- **AENV:** A 0, D 150-400 ms, S 0-20%, R 50-150 ms. [G]
- **FX:** light drive, short plate reverb or slapback. **Performance:** velocity to FENV amount. [G]
- **0.2.20, RUBBER BAND (ANALOG) parted from LADDER BASS** (audit 5.99): a tighter pluck, as above but the sub at 20 % (was 40 %: more saw, less of LADDER BASS's saw-over-sub), CUT 32, D 380 ms / S 32 % (was 500 ms / 45 %), the filter envelope deeper and shorter (95 %, 110 ms, S 0), RES 48 (the rubbery resonance), DRV 18, SAT 22, the house HF shelf, reverb 5 % (was 12 %: a bass stays nearly dry). [G, within the recipe's ranges]

## 1.7 Growl bass (dubstep)
- **Oscillators:** a wavetable (a "growl" table) at a set position, its warp (Serum's Bend +/-) and position moved by one LFO: in one walkthrough LFO 1 at a rate of 1/2 (tempo-synced), retriggered with each note, to the wavetable position by about 27-38 % and to the warp by 19-41 %; a second oscillator under it. [S: [Bass Gorilla, Serum super growl bass](https://bassgorilla.com/serum-tutorial-super-growl-bass-in-ableton-live/)]
- **Filter / FX:** a notch filter and a formant filter both moved by the same LFO, distortion (Diode, drive 20 %), flanger, phaser, multiband compression; reverb low and short. [S: same]
- **Not stated:** envelopes, unison, sub, glide, mono. A sine sub and mono legato are the usual bass practice [G, 1.3 / 1.5]. A growl is the vowel moving ("wow", "yoi"): a formant table scanned by the LFO does that directly. [G]
- **DUELL's GROWL BASS (WAVE):** the GROWL table (a saw under a man's vowels U -> AH -> AE) at POS 40, LFO 2 synced at 1/2 on POS (+-19 steps, the gentle end of the source's range), free-running as every factory LFO (the house rule; synced to the tempo it still lands on the beat), SUB 40 %, DIST 20 %, mono with a short glide. [S for the LFO and drive; the rest G]

## 1.8 Electric bass, fingered (PLUCK BASS, batch R)
- **String:** a plucked string; the fingertip acts as a low-pass on the pluck, and a pluck at 1/n of the length cancels the n-th harmonic. [S 4.6: [SOS, Synthesizing plucked strings](https://www.soundonsound.com/techniques/synthesizing-plucked-strings)]
- **Sustain:** a solid-body bass rings on for many seconds; the key's release damps it (the player's fretting hand). [G]
- **DUELL's PLUCK BASS (PHYS, STRNG):** STRC 61, BRIT 47 (dark: the fingertip), DAMP 118 (a long ring), POS 41 (about a third along), ACC 127, EXC 11 (a little finger thump), REL 110 ms; the HF shelf down and a subtle PUMP ducker (the house bass rule); a SOFT insert as a limiter: a plucked string's strike peaks far over its body, the limiter lets LEVEL bring the body up under the peak cap. [S / G as marked]

## 1.9 Fretless bass (FRETLESS, batch R)
- **Character:** with no frets the player slides between notes ("more fluid slides between notes"). [S: [Wikipedia, Fretless bass](https://en.wikipedia.org/wiki/Fretless_bass)] The vocal "mwah", the tone opening a moment after the pluck as the string buzzes along the fingerboard, is common player knowledge, not stated by that page. [G]
- **DUELL's FRETLESS (FM6), since 0.2.17 its own patch F11:** before, it used the ROUND BASS patch (F3, the 1.4 layout) of SLAP DOCTOR with slower modulators and the two sounded alike. Now: a warm, near-sine bass, a sine carrier at 1:1 with a sine modulator at 1:1 whose level opens slowly after the pluck (attack rate 52: the index swells over about 0.2 s, the "mwah") and settles low, a quiet carrier an octave down (ratio 0.5) for the body; velocity to brightness 2; mono legato with a 90 ms glide; CHO 11 %; the house bass EQ and ducker. A test (`tests/fm_bass_diff_test.c`) keeps it apart from SLAP DOCTOR. [S / G as marked]

## 1.10 Chip pulse bass (SID BASS, 0.2.17)
- **Character:** three pulse waves, the third an octave down, a few cents apart, the pulse width moved slowly (PWM): the bass of the SID chip's players. [S: [c64-wiki, Rob Hubbard](https://www.c64-wiki.com/wiki/Rob_Hubbard): PWM, ring modulation and LFO were his tools.] No source gives register values; every number below is [G].
- **DUELL's SID BASS (TRIO):** PLS3, INT3 -12, DTN 7, low-pass CUT 72, RES 24, PW 40; LFO 2 (triangle, 1.1 Hz) on the pulse width, FILTER ENV on the cutoff (40 %, 220 ms); AMP A 2, D 240, S 85 %, R 50; mono, glide 35 ms; the HF shelf down, no ducker (a hard bass), dry. [G]

## 1.11 Hard-sync bass (SYNC BASS, 0.2.17)
- The hard-sync of 2.2 in the low octave: oscillator 2 synced to oscillator 1 a fifth up, FILTER ENV sweeping the sync interval down on every note (the "eow" of sync). [S for the principle: 2.2; the setting is G]
- **DUELL's SYNC BASS (TRIO):** WAVE SYNC, INT2 +7, CUT 62, RES 30; FILTER ENV to INT2 (+26 %) and to the cutoff (35 %, 190 ms); AMP A 2, D 300, S 80 %, R 45; mono, glide 30 ms; HF shelf, no ducker, dry. TRIO has no DC blocker of its own (the master's removes it).

## 1.12 Wavefolder bass (FOLD BASS, 0.2.17)
- Fold after a filter, an envelope on the fold amount; keep the weight in the sub first. [S: [Noise Engineering, getting started with wavefolders](https://noiseengineering.us/blogs/loquelic-literitas-the-blog/getting-started-wavefolders); [AE Modular bass tutorial on MatrixSynth](https://www.matrixsynth.com/2019/12/ae-modular-synth-bass-sound-tutorial.html).] Values [G].
- **DUELL's FOLD BASS (ANALOG):** WAVE SUB (MIX 55 %), CUT 60, RES 20, DRV 20; the FOLD insert (A 46, B 100, C 96, full mix) with FILTER ENV and aftertouch on the fold amount; AMP D 260, S 90 %; mono, glide 25 ms; HF shelf, no ducker, dry.

## 1.13 Hardstyle kick-bass (KICK BASS, 0.2.17)
- A hardstyle kick: a fast pitch drop at the start (a few semitones to an octave over 10 to 30 ms), a sine body, distortion on the tail, then EQ. No source states one recipe with numbers. [S: [EDMProd, how to make hardstyle](https://www.edmprod.com/how-to-make-hardstyle/); [ADSR Sounds, hardstyle kick with Serum](https://www.adsrsounds.com/serum-tutorials/hardstyle-kick-sound-design-with-serum/).] Played as a bass note. [G]
- **DUELL's KICK BASS (WAVE):** TABLE HARM at POS 10 (a sine with a few harmonics), SUB 55 %; FILTER ENV (D 95 ms) on PITCH through two matrix routes (63 each: about +23 semitones at the strike); AMP A 0, D 380, S 55 %, R 60; the HARD insert (A 112) and DIST 32 %; one octave down in all (EXPR, which is full unless a CC11 lowers it, on PITCH at -64: -12 semitones, 0.2.20: F3 gives the sub at 44 Hz); mono; HF shelf, no ducker, dry.

## 1.14 Fuzz bass (FUZZ BASS, 0.2.17)
- The "distorted layer" bass: a resonant phase-distortion saw (the squelch of 1.2) into a FUZZ insert. [G]
- **DUELL's FUZZ BASS (PHASE):** RSAW, DCW 44, ENV 45, SUB 38 %; FILTER ENV D 200; FUZZ insert (A 82, B 90, C 90, mix 110); AMP A 2, D 260, S 88 %, R 45; mono, glide 30 ms; HF shelf, no ducker, dry.

## 1.15 FM sub bass (FM BASS 2, 0.2.17)
- **The voice:** a 6-operator FM bass voice of the CC0 collection of aquanodemusic (Aquanode Soundscapes, CC0 1.0 Universal, <https://github.com/aquanodemusic/aquanode-soundscapes>), imported from its DX7 voice file (Aquanode Aquazone Dexed Presets, voice 12) into the FM6 engine's factory patch F9: algorithm 22, a round sub with a bright strike. Changes: the name, and the level 3 of the two carriers raised (from 0 to 95 / 96) so that a held key holds its note (the original voice falls away, a plucked bass; a factory BASS must hold).
- **Made ours by the sound (all [G]):** a gentle multimode low-pass (12 dB, CUT 64 about 3.7 kHz, RES 10 %), a SOFT insert at 70 % mix for a little warmth, the HF shelf down and the subtle PUMP ducker, REV 5 %. Mono.

## 1.16 Chip octave bass (ARP BASS, 0.2.17)
- A rolling octave bass of the chip tunes: the LOFI engine's built-in octave arp on a pulse (the arp runs at the chip's own rate, not the tempo). [G; 2.5]
- **DUELL's ARP BASS (LOFI):** 8BIT PLS, DUTY 40, ARP OCT, TONE 84; AMP A 0, D 150, S 70 %, R 30; mono; HF shelf and the subtle PUMP ducker (a rolling bass keeps it), dry.

---

# 2. LEAD

## 2.1 Moog mono lead with glide
- **Oscillators:** 2-3 saw/pulse oscillators, one detuned a few cents (3-10 ct) or a fifth/octave stack; mono, last-note priority. [G; no fetched page gave numbers]
- **Filter:** 24 dB low-pass, cutoff mid (60-90), resonance low-mid (10-40), key tracking 50-100% so the tone stays even across the keyboard. [G]
- **FENV:** amount 30-60%, A 0-10 ms, D 200-600 ms, S 40-70%, R 100-300 ms. [G]
- **AENV:** A 0-5 ms, D short, S 100%, R 80-250 ms. [G]
- **Glide:** portamento ~30-150 ms (legato-only: glide only when notes overlap). [G]
- **LFO:** triangle/sine ~4.5-6 Hz to pitch (vibrato), depth 5-15 cents, brought in by mod wheel; optional onset delay of 200-500 ms. The SOS brass article uses about 5 Hz vibrato at very low depth with delayed onset via an AR ramp (for brass, but the same technique applies). [S: [SOS, Synthesizing Brass](https://www.soundonsound.com/techniques/synthesizing-brass-instruments)]
- **FX:** subtle delay (dotted 8th, 20-30% mix), short reverb, light drive. [G]
- **Performance:** mod wheel = vibrato depth or cutoff, aftertouch = vibrato or cutoff, velocity = level (slightly) and filter. [G]

## 2.2 Hard-sync lead
- **Oscillators:** two oscillators, osc 2 (follower/slave) hard-synced to osc 1 (leader). The follower's pitch sets the timbre; sweeping it produces the classic "eeee-owww" harmonic sweep. [S: [Wikipedia, Oscillator sync](https://en.wikipedia.org/wiki/Oscillator_sync) via search excerpt; [Perfect Circuit](https://www.perfectcircuit.com/signal/what-is-oscillator-sync) page returned HTTP 403, not read]
- **Modulation:** a pitch envelope on the master (sync) oscillator is a common route to the swoop; for Ultravox-style leads use portamento, slow pitch modulation, distortion and flanging, and sweep the slave by hand. [S: KVR thread via search excerpt, informal source]
- **Setting guesses:** follower +7 to +24 semitones above the leader (typical sweep range, [G]), sweep with ENV (D 300-800 ms) or LFO (0.2-1 Hz) or mod wheel; filter fairly open (cutoff 80-110), resonance low; AENV A 0, S 100%, R 100-200 ms. [G]
- **FX:** drive, delay, phaser. [G]
- Disagreement/uncertainty: no source gave numeric sweep ranges.
- **0.2.20, RAZORWIRE and BANSHEE told apart** (audit 3.63): RAZORWIRE keeps the Ultravox hard-sync cry but sweeps faster and wider (FILTER ENV on the sync interval 100 %, D 260 ms), more drive (36), velocity also opens the sweep (a harder key = a wider cry), delay 14 %. BANSHEE (TRIO) becomes the slow wail: the slave a full octave up (INT2 +12), the sweep rises over 220 ms before it falls, the note swells in over 320 ms, a lower cutoff (CUT 66), and a slow FLANG insert (0.3 Hz, 45 %) in place of the delay. [G, on the source's 'sweep the slave'; S: SOS brass notes for the delayed onset]

## 2.3 Brassy polysynth lead (Prophet / Oberheim style)
- **Oscillators:** sawtooth on both oscillators, one slightly detuned (about 5-12 ct) for thickness; Prophet-5 is two oscillators per voice (A saw/pulse, B saw/tri/pulse) with a 4-pole resonant low-pass; keyboard tracking off/half/full. [S for the architecture: [Sequential Prophet-5 descriptions via search](https://reverb.com/item/65259562-sequential-prophet-5-polyphonic-analog-desktop-module)]
- **Brass character:** the filter envelope is what makes it brass: start from a sawtooth, short-but-not-instant filter attack, an envelope amount high enough to matter, and attack speed tied to velocity. [S: SOS brass articles, [Synthesizing Brass](https://www.soundonsound.com/techniques/synthesizing-brass-instruments) and Nick Magnus via search excerpt]
- **FENV (starting values):** A 30-120 ms, D 200-500 ms, S 40-60%; the SOS Minimoog brass example gives A 600 ms, D 800 ms, S level 5/10, instant release with resonance 2/10 (a slow-bloom section brass). [S: [SOS Minimoog brass](https://www.soundonsound.com/techniques/brass-synthesis-minimoog), as summarised by the search result; I did not fetch the page, so confirm]
- **AENV:** A 15-40 ms, S high, R 50-150 ms. [G; one search summary suggested 20-40 ms, labelled there as its own knowledge, not from a page]
- **Velocity:** shortens the amp attack and sets the filter-envelope amount; aftertouch can vary brightness during sustain. [S: SOS brass article, fetched]
- **LFO:** delayed vibrato ~5 Hz; optional growl: triangle LFO ~80 Hz into the filter through an AD contour. [S: SOS brass article, fetched]
- **FX:** chorus, short reverb. [G]
- **0.2.20, FUNFARE (ANALOG) parted from SFORZANDO and SYNC BASS** (audit 4.22 / 5.48): the lead is a played swell, not a stab: AMP A 180 ms, S 100 %, FILTER ENV on the cutoff opens over 180 ms and closes slowly (D 900 ms to S 40 %, depth 78 %), CUT 56, RES 14 %, detune 14 ct; the CHO send becomes a CHOR insert (0.4 Hz, 55 %): the polysynth's ensemble. [S: Prophet / Oberheim brass leads open with a filter swell; numbers G]

## 2.4 Supersaw lead
- **Oscillator:** seven detuned sawtooths with free-running (random) phase; JP-8000's supersaw has a centre saw plus six side saws, a DETUNE control with a deliberately non-linear curve (small spread over the lower half of the knob), and a MIX control for centre vs side level. Relative frequencies at maximum detune were reported as 0.893, 0.939, 0.980, 1.0, 1.020, 1.064, 1.110 by a forum summary of Szabo's analysis. [S: [Wikipedia, Roland JP-8000](https://en.wikipedia.org/wiki/Roland_JP-8000); numbers from a KVR forum post citing Adam Szabo's 2010 thesis "How to Emulate the Super Saw", via search; the forum itself questions the method, so treat the numbers as indicative]
- **Setting guidance:** detune low-mid (20-60) for pads and lead body, higher for an aggressive trance lead; mix 60-100 (more side saws). [G]
- **Filter:** low-pass cutoff 70-110, resonance 0-30; the original was aliased and bright, possibly with a high-pass at the low end (hedged by the source). [S/hedged]
- **FENV:** amount small (10-30%), D 300 ms. **AENV:** A 0-10 ms, S 100%, R 100-400 ms. [G]
- **FX:** this is where the sound lives: chorus/ensemble, delay, big reverb, side-chain pumping for trance. [G]
- **Trance usage:** leads and chord stabs, "Anthem Trance". [S: Wikipedia]

## 2.5 Chip / 8-bit lead
- **Waveforms:** pulse with selectable duty (NES 2A03 has two pulse channels with 4 duty settings, a 4-bit stepped triangle, an LFSR noise channel and a 1-bit delta PCM channel). [S: [Wikipedia, Ricoh 2A03](https://en.wikipedia.org/wiki/Ricoh_2A03); the page does not give the duty percentages. The usual 12.5/25/50/75% values are from general knowledge and from this firmware's own comments in eng_lofi.c, not a fetched source]
- **Amp:** gate-like, A 0, a stepped (4-bit) volume decay, S high, R ~0-30 ms. [G]
- **Vibrato:** delayed vibrato 5-7 Hz depth +-10-30 ct; pitch sweep/bend at note start. [G]
- **Arp:** fast 2-3 note arpeggios (octave, major, minor) at ~50-60 Hz frame rate stand in for chords on a mono channel. [G, widely known; no fetched source]
- **Filter:** none on the original; bit reduction/downsampling provides the grit. **FX:** little or none; short delay. [G]

## 2.6 FM lead
- **Operators:** carrier sine, one modulator at 1:1 or 2:1 ratio with moderate feedback; modulator envelope with slow decay and level that follows velocity, giving a singing lead; light vibrato. [G; the Plugin Boutique "80s Brass" recipe (triangle carrier and modulator, modulator envelope peaks at the attack then reduces, darker at low velocity) is the closest sourced example: [Plugin Boutique](https://www.pluginboutique.com/articles/1873-FM-Synthesis-Cookbook-Five-Classic-FM-Sounds-and-How-They-Work)]
- **Brightness:** modulator output level is the "cutoff" of FM. [G, standard FM practice]
- **AENV:** A 0-10 ms, S 100%, R 80-200 ms. **FX:** chorus + delay. [G]
- **DUELL's brass solo (#30 HAN SOLO):** the BRASS SECT patch on ALG 16 (a single carrier, OP 1, with three modulator branches: the classic FM brass tree). The patch's own attack swells the note, FILTER ENV on MLVL (the modulator level) rises with it over ~120 ms and settles high, VMOD makes it darker at low velocity. Three detuned carriers (the patch's own ALG 5) beat once in a few seconds at C3 and dropped the level by 15 dB, so they are not used for a solo. [G]

## 2.7 Formant / vocal lead
- **Source:** a buzzy saw/pulse glottal-style source into 3-5 band-pass resonances at vowel formants; morph the vowel with LFO/envelope or pitch. Reference male values (Hz): /a/ 700/1200/2500, /e/ 410/2000/2570, /i/ 215/1900/2630, /o/ 440/750/2160, /u/ 290/750/2300 (F1/F2/F3). [S: Cantor Digitalis table via search ([PMC table](https://pmc.ncbi.nlm.nih.gov/articles/PMC6090560/table/T1) listed in results; I did not verify which of the listed tables holds these numbers)] DUELL's own VOICE engine uses Hillenbrand 1995 male means instead (/a/ 768/1333/2522, /i/ 342/2322/3000 ...), so the two sets differ by up to ~20%; vowel data vary by speaker. [S: eng_formant.c comment]
- **Female/treble:** shift formants up, roughly in proportion. [S, search summary]
- **Performance:** vowel on mod wheel or velocity; glide on; vibrato 5-6 Hz. [G]
- **FX:** chorus, hall reverb for choir. [G]

## 2.9 Flute lead (PURPLE FLUTE, batch R)
- The SOS flute patch of 3.9 played as a lead: a single sawtooth into a low-pass at about 2 kHz with moderate resonance; attack neither instant nor slow, full sustain, a short release; no pitch vibrato but a 5-6 Hz modulation of the cutoff of about +-10 %, brought in after the onset; pressure to the cutoff (brighter, not louder). [S: [SOS, Practical Flute Synthesis](https://www.soundonsound.com/techniques/practical-flute-synthesis)]
- **DUELL's PURPLE FLUTE (WAVE):** the AIR table low (POS 10: the soft flute with little breath), the multimode filter's low-pass at about 2 kHz (CUT 54) with RES 27 %, LFO 2 at 5.5 Hz on that cutoff (+-1 step) faded in over 500 ms, aftertouch to the cutoff; A 71 ms, R 170 ms, a light 40 ms glide (mono legato); FILTER ENV adds a short breath at the onset (to POS). [S / G as marked]

## 2.10 Clarinet (LICORICE, batch R)
- **Waveform:** the clarinet's spectrum is dominated by the odd harmonics, much like a square wave ("square waves produce remarkably good clarinet tones"); even harmonics are weak, at least low down. [S: [Yamaha, MX study: more synth basics](https://yamahasynth.com/learn/synth-programming/more-synth-basics-mx-study/); [Tufts, clarinet spectra](https://mma.pages.tufts.edu/musiceng/research/clar_sum_02/SumA.htm), via search summaries]
- **Vibrato:** a classical clarinet is played without vibrato. [G]
- **DUELL's LICORICE (WAVE):** the ODD table (sine to square, the odd harmonics one by one) at POS 37 (about ten of them), a gentle low-pass of the multimode filter (CUT 65, RES 11 %), aftertouch to POS (blowing harder: more harmonics), FILTER ENV opening a few more at the tongued onset; A 37 ms, R 110 ms, a short glide, mono; no vibrato. [S / G as marked]

## 2.11 Oboe (OBOE-ISH, batch R)
- **Waveform:** narrowing a pulse toward 10 % moves the tone from hollow to nasal; the oboe and the clavinet are the "pinched", nasal tones. [S: [Yamaha, MX study: more synth basics](https://yamahasynth.com/learn/synth-programming/more-synth-basics-mx-study/)]
- **DUELL's OBOE-ISH (TRIO):** three pulses on one pitch (DTN 3 ct), PW 17 (about 13 %), a low-pass at 3.5 kHz with RES 27, a little FILTER ENV on it; A 47 ms, R 120 ms, a short glide, mono; a delayed 5.1 Hz vibrato (the smallest step, faded in over 470 ms) that pressure deepens, as the other leads. [S for the pulse; the rest G]

---

## 2.8 Hoover (the Alpha Juno "What the..." lead)
- **Oscillators:** three PWM sawtooths an octave apart (root, octave up, two octaves up), locked, not detuned; a PWM-saw shape where the synth has one, else a saw mixed with a pulse. [S: [Music Production Wiki, How to recreate the hoover sound](https://musicproductionwiki.com/articles/how-to-recreate-the-hoover-sound); a forum description of the Juno's PWM saw "cutting two holes into the saw", [Scope users forum](https://www.forums.scopeusers.com/viewtopic.php?p=80112), via a search summary]
- **Modulation:** a fast LFO into the pulse width (rate and depth not given; the Alpha Juno's LFO is a triangle); optionally a little of it on pitch. [S: same, and [KVR, Alpha Juno-2 PWM sawtooth](https://www.kvraudio.com/forum/viewtopic.php?p=8367138) via a search summary]
- **Pitch envelope:** the note starts slightly sharp and dives to its pitch over the first fraction of a second (the amount is not given). [S: Music Production Wiki]
- **Filter / FX:** a resonant low-pass, high resonance, the top kept bright; a touch of drive; a rich, wide, fairly deep and fast chorus (two stacked or an ensemble). [S: same] Envelope times and glide are not given. [G]
- **DUELL's HOOVER (WAVE):** the HOOVR table at POS 50, LFO 2 TRI 5 Hz on POS (+-16 steps) [G for the rate and depth], aftertouch on POS, FILTER ENV -> PITCH about +1 semitone decaying in 150 ms [G for the amount], the universal filter at about 9 kHz with RES 55 %, DIST 10 %, the chorus send at 70 % (the chorus rate is the global one), no pitch LFO (no vibrato). [S / G as marked]

## 2.12 Square lead (SQUARE LEAD, 0.2.17)
- The mono square lead of 2.1 with portamento: two square waves a few cents apart, the filter opening with the note. No vibrato. [G; 2.1]
- **DUELL's SQUARE LEAD (ANALOG):** WAVE SQR, DTN 7, MIX 40 %, CUT 84, RES 16, KTR 64; FILTER ENV on the cutoff (25 %, A 8, D 500, S 60 %); AMP A 5, S 100 %, R 200; mono, glide 70 ms; aftertouch and velocity on the cutoff; DLY 18 %, REV 15 %.

## 2.13 Screaming lead (SCREAMER, 0.2.17)
- The hard-dance screech: the YELL table (a formant table with a harsh scan) through the multimode filter and a little drive. [G; the WAVE engine's own notes: "screaming leads (YELL)"]
- **DUELL's SCREAMER (WAVE):** TABLE YELL at POS 42, DTN 10; the multimode low-pass CUT 72, RES 28 %; DIST 26 %; aftertouch on POS; mono, glide 80 ms; DLY 16 %, REV 14 %.

---

# 3. PAD

## 3.1 Juno-style chorus pad
- **Oscillators:** saw at full, pulse/square slider about halfway, sub-square to taste; PWM switch on LFO with PWM depth moderate; triangle LFO with rate and delay. [S: Attack Magazine Juno pad recipe via search excerpt, [Attack Magazine](https://www.attackmagazine.com/?p=133239); the Roland manual lists PWM depth 0..63, [JUNO-106 manual](https://www.rolandcloud.com/getmedia/3d48e0af-afa3-4368-a68b-6d297cab4625/JUNO-106-Manual-E.pdf?ext=.pdf)]
- **AENV:** A 0, D top, S half, R long enough for notes to overlap (an Attack Magazine suggestion; it describes the Roland Cloud version with two envelopes). [S, caveat as noted] Slow-attack variants (A 200-800 ms) are common but unsourced. [G]
- **Filter:** low-pass cutoff ~40-80, resonance low, small envelope amount. [G]
- **Chorus:** Juno has chorus I and II (II is thicker); no source recommended one for pads (my suggestion: I for subtle shimmer, II for lush, [G]). The chorus is what makes a single-oscillator-per-voice synth sound wide. [S for chorus function: [SOS, Creative delays](https://www.soundonsound.com/techniques/more-creative-synthesis-delays) via search]
- **LFO:** pulse-width LFO 0.5-2 Hz; keep pitch depth low. [G for rates]

## 3.2 Prophet-style warm pad
- **Oscillators:** saws on A and B slightly detuned (5-12 ct) with one at low pulse width; Poly Mod from filter envelope (or osc B) into osc A pulse width / filter for slow movement. [S: Prophet-5 Poly Mod routing, reverb/Sequential descriptions via search; settings [G]]
- **Filter:** 4-pole LP, cutoff 40-70, resonance 10-25, key tracking half. [G]
- **FENV:** A 300-1500 ms, D 1-3 s, S 40-70%, R 800-2000 ms. **AENV:** A 200-800 ms, S 100%, R 1-3 s. [G]
- **FX:** gentle chorus, plate/hall reverb 25-40%. [G]

## 3.3 PWM string pad
- **Settings from SOS (Korg T2 patch):** both oscillators sawtooth at 16', OSC2 detune about 10, PWM LFO square wave rate 60 and intensity 02 on OSC2 (those are the T2's own units), filter cutoff 75 with a little key tracking, attack rate 10, release 49; chorus and reverb added afterwards. [S: [SOS, Synthesizing Strings: PWM and String Sounds](https://www.soundonsound.com/techniques/synthesizing-strings-pwm-string-sounds)]
- **Roland Super JX-10 patch (same article):** both DCOs saw 8', DCO2 fine tune -4, PWM LFO square rate 78 with DCO1 LFO depth 02, LPF 47, resonance 0, envelope amount 14, key follow 64; Env 1: A15 D0 S99 R39; Env 2: A45 D0 S99 R54; chorus on. [S: same article] Units are patch values of those synths, not Hz or ms.
- **Why:** divide-down string machines start from a thin sawtooth and rely on ensemble chorus; the Solina splits the saw into three vibrato-modulated paths. [S: [SOS Creating analogue sounds on digital synths part 2](https://www.soundonsound.com/techniques/creating-analogue-sounds-digital-synths-part-2), search excerpt]
- In generic terms: AENV A 100-400 ms, S 100%, R 400-900 ms; no resonance; chorus/ensemble essential. [G, consistent with the patch units above]

## 3.4 Evolving / granular pad
- **Grain settings:** smooth pad textures use grain size about 50-200 ms (50-100 ms in one guide), density about 5 overlapping grains (MainStage manual's pad suggestion with 100 ms), a little spray (random offset on the read position; Granulator II walkthrough used 32 ms), and slow automation of position, optionally an LFO on position with a low rate to shimmer around one region. Short grains (< 20 ms) turn buzzy/percussive/glitchy. [S: [Splice, Granular synthesis guide](https://splice.com/blog/intro-granular-synthesis/), [Violet Recording](https://violetrecording.com/how-to-use-granular-synthesis/), Apple MainStage manual, via search summaries]
- **Source:** sustained tonal material (vocals, bowed strings, cymbal washes), not drum loops. [S: same]
- **AENV:** A 400-1500 ms, S 100%, R 1.5-4 s. [G] **FX:** big reverb 60-90%, chorus or shimmer. [G]
- **Slow modulation:** two LFOs at 0.05-0.3 Hz (incommensurate rates) on position and filter. [G]

## 3.5 Choir / vowel pad
- **Source:** either a sampled choir or a formant (VOICE-style) engine at a fixed vowel (/a/ or /o/) with slow vowel drift; formant data as in 2.7. [S for the formant numbers]
- **AENV:** A 300-900 ms, S 100%, R 800-1500 ms. Slight random vowel per note (the "RAND" idea) and a breath-noise component at low level. [G]
- **LFO:** vibrato 5 Hz with delay, depth 5-12 ct; **FX:** chorus, large hall reverb (60-90%). [G]
- **0.2.20, WOW FLUTTER (LOFI) gets the TAPE insert** (owner request; audit: 5.81 from DUSTY DRONE): wow is a slow speed variation of a tape machine (below about 6 Hz) and flutter the faster one [S: [Wikipedia, Wow (recording)](https://en.wikipedia.org/wiki/Wow_(recording)); [Vintage Digital glossary](https://www.vintagedigital.com.au/glossary/wow-and-flutter/)]. DUELL's TAPE insert is exactly that (a random 0.5 .. 2 Hz wandering of the delay line plus a fixed 6 .. 9 Hz flutter of ~3 cents): WOW 52 (moderate), SAT 40, TONE 78 (the high-cut). The chip's own VIB (14) is off, the wow is the tape's; TONE 70 -> 52 (darker). [G for the depths]

## 3.6 Glass / FM pad
- **Operators:** the sourced starting point is a DX7-style string pad: carrier at ratio 1 with attack 57 and long decays, modulator ratio 2.00 detune +3, attack 47; modulator and carrier slow attacks (Yamaha FM 101 part 5, operator times in DX7 0-99 rate units). [S: [Yamaha Synth, FM 101 part 5](https://yamahasynth.com/learn/synth-programming/fm-101-part-five-expanding-your-sound/)] A glassy variant adds a high non-integer modulator (ratio 3.5-7), low index, slow attack. [G]
- **FX:** shimmer reverb, chorus. [G]

## 3.7 Dark drone
- **Oscillators:** two detuned saws plus a sub sine, or a low pulse; filter cutoff 20-45, resonance 20-45 with a slow LFO (0.02-0.2 Hz) on cutoff, depth 15-30%. [G]
- **AENV:** A 1-4 s, S 100%, R 3-8 s. **FX:** low-passed delay with high feedback, reverb 50-80%, slight drive. [G]
- No fetched source gave numeric drone recipes; treat all as craft convention.

## 3.8 Wavetable sweep pad (the PPG way)
- **Principle:** on the PPG Wave 2.3 the oscillator plays one wave of a 64-wave wavetable; a knob assigns Env 1 to the wave played, so the envelope's attack rate sets the speed at which the sound cycles through the wavetable, and Waves-Sub sets the starting wave. The 24 dB/oct low-pass goes from "obviously digital" wide open to mellower, warmer tones. [S: [SOS, PPG Wave 2.3 and Waveterm B review](https://www.soundonsound.com/reviews/ppg-wave-23-waveterm-b)] The PPG's tables include resonant ones (harmonics 1-8 very strong, simulating a resonant filter) and sweeps from sine to saw or square. [S: search excerpts of [Preset Patch, PPG wavetable list](https://www.presetpatch.com/article/ppg-wavetable-list); the page itself could not be fetched]
- No envelope times are given; a pad's: AENV A 200-800 ms, S 100 %, R 1-3 s; the sweep's envelope slower (A 1-3 s), settling part way. [G, 3.2]
- **DUELL's DIGITAL PAD (WAVE):** the BAND table (a band of harmonics moving up, in the spirit of the resonant tables) from POS 10, FILTER ENV SHP +60 % with A 2.5 s, D 3 s, S 50 % (the envelope carries the band up and settles half way), LFO 2 0.12 Hz on POS (+-6 steps), DTN 7 ct, the multimode filter's low-pass at about 6.4 kHz (CUT 70), chorus, reverb. [S for the principle; the numbers G]

## 3.9 Breathy flute pad
- **Flute patch (SOS):** a single sawtooth into a low-pass at about 2 kHz with moderate resonance (it adds edge without sounding synthetic) and a few percent of key tracking; one envelope for amplitude and filter, attack "neither instant nor slow", no decay, full sustain, a short release lengthened until notes join; no pitch vibrato or tremolo but a 5-6 Hz modulation of the cutoff of about +-10 % (blowing pressure), brought in after the onset; pressure (aftertouch) to the cutoff: harder blowing brighter, not louder. The author leaves out white noise. [S: [SOS, Practical Flute Synthesis](https://www.soundonsound.com/techniques/practical-flute-synthesis)]
- **Breath:** a breath-noise component at a low level is the usual pad addition [G, 3.5]; a flute's first harmonics are weaker than a saw's [S via notes, [Synthesizing wind instruments](https://publish.obsidian.md/arendleejessurun/Atlas/Musicianship/Synthesis/Synthesizing+wind+instruments), not SOS].
- **DUELL's BREATH PAD (WAVE):** the AIR table (a soft flute and a breath of random harmonics growing along the scan) at POS 30, aftertouch to POS (more breath, brighter), the multimode filter's low-pass at 2 kHz (CUT 54) with RES 25 %, LFO 2 5.5 Hz on that cutoff, +-1 step (about +-10 %), faded in over 600 ms, A 300 ms, R 900 ms (a pad's, 3.5), DTN 6 ct, chorus, hall reverb. [S for the filter, LFO and pressure; the times G]

## 3.10 String machine (STRING MACH, batch R)
- **Source:** string machines (Solina, ARP String Ensemble) used divide-down oscillators with a sawtooth-like wave that sounds thin raw; the ensemble effect (three modulated delay lines) makes the sound. [S: [SOS, Synthesizing Strings: String Machines](https://soundonsound.com/techniques/synthesizing-strings-string-machines); [Wikipedia, ARP String Ensemble](https://en.wikipedia.org/wiki/ARP_String_Ensemble)] A recreation: a pulse of 5-20 % width and a second oscillator 12 semitones up ("cellos" and "violins"), into a chorus (a second chorus after it for the vintage sound). [S: [MusicRadar, How to recreate the sound of the Solina String Ensemble](https://www.musicradar.com/how-to/synth-string-synth-choir)]
- **DUELL's STRING MACH (TRIO):** PLS3, PW 17, osc 2 +12, osc 3 on the root, DTN 7, a low-pass at 5 kHz; A 270 ms, R 1.1 s (the machines' slow controls), the chorus send at 71 %, a hall; a slow free LFO 2 nudges the cutoff (the house subtle motion). [S / G as marked]

## 3.11 Whisper pad (WHISPER PAD, batch R)
- Felucca's own WHISPER preset on the formant engine: "ah" gliding toward "oh" (VOWL 0, VOWL2 95, TALK 88), the formants a little up (SHIFT 3), BUZZ 50, breath at the top (BRTH 120: the voicing fades to a whisper), Q 50. [S: `eng_formant.c` FORMANT_PRESETS, WHISPER] Pad envelope and effects as 3.5. [S/G 3.5]
- **DUELL's WHISPER PAD (VOICE):** the preset's values, RAND 11, A 800 ms, R 1.5 s, chorus, a large hall, a little delay; a slow free LFO 2 on VOWL2 (the vowel it settles on). [S / G as marked]

## 3.12 Brass pad (BRASS PAD, batch R)
- The brass principle of 2.3 slowed into a pad: the harmonics grow as the note swells (the filter or the wave opened by an envelope), a section's power chords. [S 2.3 for the principle; G for the pad times]
- **DUELL's BRASS PAD (WAVE):** the BRASS table (harmonics growing in along POS) from POS 31, FILTER ENV SHP +37 % (A 600 ms, D 1.5 s, S 60 %) carrying the swell, A 370 ms, R 1.2 s, DTN 9 ct, chorus, a hall; chord keys POW (the power chord), OPEN voicing, as the retired BRASS CHOIR. [S / G as marked]

---

# 4. PLUCK / KEYS

## 4.1 DX7 electric piano (tine / Rhodes-like)
- **Structure:** pairs of modulator -> carrier. In the classic algorithm-5 voice, op 1 (carrier) at ratio 1.00 modulated by op 2 at ratio 14.00 gives the "tine" ping; pairs 4->3 and 6->5 give the body. [S: *Making Music* 1987 via [muzines DX7 article](https://www.muzines.co.uk/articles/yamaha-dx7/4733), search excerpt]
- **Modern recreation (Yamaha MODX/Montage, algorithm 67, not a DX7):** feedback 6 on op 1; body modulators Level 81-91 with velocity sensitivity +6 and rate/key scaling 3; carrier decays: time 56-70 to level 95 then to 0; the 14:1 tine modulator Level 60, velocity +7, fast decay (D1 30, D2 46) so it vanishes within a fraction of a second; carriers detuned +-7 / +3 for warmth. [S: [Yamaha FM 101 part 5](https://yamahasynth.com/learn/synth-programming/fm-101-part-five-expanding-your-sound/)] Base ratios for ops 1-4 and 6 were not stated.
- **Other recipe:** first carrier decay 2-4 s with its 14:1 modulator decaying faster; second carrier slightly detuned with a 2:1 or 3:1 modulator decaying in about 500 ms. [S: [Plugin Boutique](https://www.pluginboutique.com/articles/1873-FM-Synthesis-Cookbook-Five-Classic-FM-Sounds-and-How-They-Work)]
- **Performance:** velocity to modulator level is the whole expressiveness: soft = mellow tone-bar, hard = barking tine. [S: velocity sensitivity values above]
- **FX:** stereo chorus and tremolo/auto-pan (0.5-5 Hz, [G]), light reverb.

## 4.2 Clavinet-like
- **Oscillator:** narrow pulse or reedy wave (Clav string pickup character), with the wave/pulse-width opened by a short envelope. [S for the envelope-on-wave idea: [MusicRadar Clavinet patch](https://www.musicradar.com/how-to/clavinet-patch) via search; article text cut off before filter/decay values]
- **Envelope (general practice, not sourced):** AENV A 0, D 150-400 ms, S 0-10%, R 30-80 ms (damper mute); filter low-pass with short decay and velocity to cutoff; follow with an auto-wah/envelope filter driven by velocity. [G]
- **Real instrument notes:** D6 has pickup selector and in/out-of-phase switches and tone filters; phase switching is key to the sound. [S: physical-modelling paper via search]
- **FX:** wah, compressor, light chorus; funk 16th patterns.

## 4.3 FM bell / glass
- **Operators:** sine carrier with a sine modulator at a higher non-integer ratio (at least 4-5 per one cookbook) for inharmonic partials; carrier short attack, long decay and release (anything over 2 s for both operators), modulator with a shorter decay so the sound starts bright and decays to a pure tone. [S: [Plugin Boutique](https://www.pluginboutique.com/articles/1873-FM-Synthesis-Cookbook-Five-Classic-FM-Sounds-and-How-They-Work)]
- **Physical-bell analysis:** the strike tone relates to partials near 2:3:4 over a (possibly missing) fundamental; two hum sines about an octave apart but slightly detuned for beating; a clapper impact with shorter decay than the body. [S: [SOS, Synthesizing Bells](https://www.soundonsound.com/techniques/synthesizing-bells); no numeric envelope times given there]
- **Ring-mod alternative:** incommensurate frequencies multiplied give sum/difference inharmonic partials. [S: Nord Modular percussion reference via search]
- **FX:** long hall reverb, little chorus. **Velocity:** to modulator level.

## 4.4 Marimba / kalimba (modal)
- **Physical (general knowledge, no fetched source):** struck bars have a strong fundamental and a few inharmonic modes that die faster than the fundamental; a free uniform bar's first overtone is about 2.76x the fundamental, and real marimba/vibraphone bars are carved so the overtones sit nearer 4x (marimba) or ~3.9x (vibraphone) [G, from memory; unverified]. Kalimba: similar with a short pluck "thunk" and longer ring. DUELL's PHYS MODAL model (STRC stretches partials, below 0.25 compressed, 0.25-0.3 harmonic, above inharmonic) covers this directly. [S: eng_phys.c header comment]
- **Generic synth recipe:** sine/triangle plus a short burst of a higher partial; AENV A 0, D 150-500 ms (higher notes shorter), S 0, R 50-150 ms; filter low-pass with a fast closing envelope to make the upper partials decay faster than the fundamental. [G]
- **FX:** short room/plate reverb, light stereo delay. **Velocity:** to brightness and level.
- **0.2.20 MALLET WOOD (marimba, FM6 WOOD BARS):** a marimba bar is carved so its second mode sits at about 4x the fundamental [G, as above]; the DX7-style marimba is a strong fundamental plus a modulator at ratio 4 whose index falls within ~100 ms, the 'knock' of the mallet on wood [S: the factory WOOD BARS voice of tools/gen_fm6_patches.py is built that way; Dexed/DX7 'MARIMBA' voices]. Before, MLVL -4 left the modulator inaudible (a plain sine blip) and the FILTER ENV values set did nothing (FM6's amplitude is the patch's); now MLVL +14, MEG +25 (the knock lasts a few tens of ms), VMOD 4, room reverb 12 %, no delay (it smeared the knock). Measured at C3: the 300 Hz .. 1 kHz band is 3 dB under the total in the first frame and 33 dB under it by 100 ms.
- **0.2.20 TEMPLE BLOCK:** a hollow wooden shell, higher than the bars and shorter, a hollow (odd-harmonic) tone [G, from memory: wood blocks and temple blocks sound at about 700 .. 2000 Hz; no source found]. The same patch with the modulators at ratio 2 (MRAT -2: odd sidebands, the hollow colour), MLVL +22, played 18.9 semitones up (EXPR is constant 127 until a CC11: two matrix routes of +63 and +38 on PITCH, no bend, C3 sounds as G4 = 392 Hz), and FILTER ENV on AMP closing the note in ~130 ms. Dry-ish: reverb 20 %, no delay.

## 4.5 Hammond organ drawbar registrations
- **Footage to harmonic:** 16' sub-octave, 5 1/3' (the 3rd harmonic of the sub, i.e. a fifth above the fundamental), 8' unison, 4' octave, 2 2/3' twelfth, 2' double octave, 1 3/5' and 1 1/3' the 17th and 19th, 1' the top. Levels 0-8. [S: [Wikipedia, Hammond organ](https://en.wikipedia.org/wiki/Hammond_organ) and SOS tonewheel articles via search]
- **Registrations:** 888000000 is the classic Jimmy Smith sound [S: Wikipedia]. Others in DUELL's WHEEL engine (FLUTE 008000000, MELLO 008400000, BLUES 888800000, GOSPL 888600006, ROCK 888000888, FULL 888888888 ...) are the firmware author's own list of "common registrations" (eng_wheel.c), not independently verified here.
- **Envelope:** organ gate: A 1-5 ms, S 100%, R 5-40 ms (no decay). [G]
- **Percussion:** a decaying 2nd (4') or 3rd (2 2/3') harmonic at note start, normal/soft, fast/slow; single triggering (only strikes if no other key is held); with percussion on the 1' bar is muted. [S: [SOS, Synthesizing Hammond organ effects](https://www.soundonsound.com/techniques/synthesizing-hammond-organ-effects), Wikipedia] No decay times were given in either source.
- **Key click:** originally a design flaw that players liked. [S: Wikipedia, SOS] 
- **Vibrato/chorus:** scanner vibrato V1-V3 / chorus C1-C3. No rate in Hz was given by either source. [S]
- **Leslie:** slow (chorale) and fast (tremolo) rotor speeds; no rpm in the fetched sources (common figures of about 40/340 rpm for the horn are from general memory, unverified here, [G]).
- **Drive:** the amp was meant to be clean; mild compression when adding notes; overdrive is a performer's/amp addition. [S: SOS]
- **Performance:** mod wheel = Leslie speed or drive; velocity ignored (organs are not velocity sensitive). [G]
- **0.2.20, PEDAL BASS (WHEEL) parted from JAZZ CLUB** (audit 6.02): the bass registration with the body bars -4 and the top -8 (no upper-harmonic shimmer, the sub bars alone), SAT 30 (the pedal through a tube amp), a TAPE insert (WOW 18 faint, SAT 90, TONE 40: the high-cut) and a percussive gate (D 900 ms to S 62 % instead of a flat 100 %: a pedal note that settles); the key-click blip on the brightness (FILTER ENV on SHP) deeper and shorter (40 %, 90 ms). The HF shelf and the subtle PUMP ducker stay. [G]

## 4.6 String pluck / harp
- **Physics:** a pluck at the middle yields a triangle wave of odd harmonics (amplitude 1/n^2); a pluck at 1/n of the length cancels the n-th harmonics; a soft fingertip acts as a low-pass. SOS concludes subtractive analogue cannot reproduce a guitar authentically. No numeric settings given. [S: [SOS, Synthesizing plucked strings](https://www.soundonsound.com/techniques/synthesizing-plucked-strings)]
- **Karplus-Strong:** a noise burst one period long in a feedback delay with an averaging low-pass; buffer length sets pitch; a loss factor sets decay. [S: [Stanford Karplus-Strong page](https://theory.stanford.edu/~blynn/sound/karplusstrong.html)]
- **Subtractive approximation (general):** saw/pulse, filter cutoff 40-70 with a fast envelope (amount 50-80%, D 100-400 ms, S 0), AENV D 0.5-2 s, S 0, R 100-300 ms; harp: longer decay, soft pluck, resonant strings (sympathetic) via reverb. [G]
- **FX:** chorus + room reverb.

## 4.7 Vibraphone
- No fetched source covers vibraphone synthesis; the recipe below is general convention [G]: sine/triangle with a soft FM or ring-mod shimmer (modulator ratio ~3.9-4 with low index), AENV A 0-3 ms, D 1.5-4 s, S 0, R 300-800 ms; tremolo (motor) as amplitude LFO 3-7 Hz, depth 20-50%, on or off by mod wheel; mallet transient short noise/FM click; reverb light-medium.

## 4.8 Brass stab
- Supersaw/saw stack (two oscillators, detune 5-10 ct, optionally an octave stack); filter cutoff 30-60 with strong envelope (amount 60-90%, A 0-20 ms, D 150-350 ms, S 30-60%); AENV A 0-10 ms, D 200-400 ms, S 40-70%, R 80-200 ms; velocity to filter amount; chorus + short reverb. [G, extends the sourced brass principles in 2.3: sawtooth, filter envelope does the work, velocity to attack]
- Stabs are typically layered chords (octave + fifth), and the 80s FM version: triangle carrier + triangle modulator whose envelope peaks at the attack and falls quickly, darker at low velocity. [S: Plugin Boutique]

## 4.9 Harpsichord (FAUX HARPSI, batch R)
- **Mechanism:** a key lifts a jack whose plectrum (quill or plastic) plucks the string; a note sounds equally loud however hard the key is pressed; on the key's return a felt damper stops the string; plucking closer to the nut emphasises the higher harmonics (a nasal quality). [S: [Wikipedia, Harpsichord](https://en.wikipedia.org/wiki/Harpsichord)] Staccato playing helps it cut through. [S: [E&MM Patchwork, Electric Harpsichord](https://www.muzines.co.uk/articles/patchwork/8013)]
- **DUELL's FAUX HARPSI (PHYS, STRNG):** STRC 71 (thin strings, little stiffness), BRIT 107, POS 11 (plucked near the end), ACC 127, EXC 11 (the quill), DAMP 64, REL 70 ms (the felt damper), a small room; no velocity routes (the instrument has no touch dynamics); a SOFT insert as a limiter (as 1.8). [S / G as marked]

## 4.10 Koto (FAKE KOTO, batch R)
- A silk string plucked near the bridge with a pick: bright, the high harmonics dying first (4.6: a pluck near the end keeps them). [S 4.6] The string settles a little after the pluck (a slight pitch fall). [G]
- **DUELL's FAKE KOTO (PHASE):** SAW with DCW 37 and the amp envelope's bend (ENV 71) closing with the decay, D 1.1 s, S 0, R 270 ms; FILTER ENV to the pitch, a fraction of a semitone, 70 ms; reverb, a little delay. [S / G as marked]

## 4.11 Toy piano (NURSERY KEYS, batch R)
- **Mechanism:** hammers strike metal bars or rods fixed at one end (a celesta-like action). [S: [Wikipedia, Toy piano](https://en.wikipedia.org/wiki/Toy_piano)] A rod clamped at one end has strongly inharmonic overtones. [G]
- **DUELL's NURSERY KEYS (PHYS, MODAL):** STRC 124 (the partials stretched far apart), BRIT 91 (metal), DAMP 71, POS 10 (struck near the end), ACC 107, EXC 21 (the hammer's knock), R 170 ms, reverb, a little delay. (The retired SAMPLE NURSERY KEYS played the piano sample an octave up; this one models the instrument.) [S / G as marked]

## 4.12 Steel drum (ISLAND STEEL, batch R)
- **Tuning:** a pan's note areas are tuned so the lowest three partials are the fundamental, the octave and the twelfth (or the double octave; the twelfth mellower, the double octave brighter). [S: [Hansen & Rossing, The Caribbean steelpan](https://www.hangblog.org/the-caribbean-steelpan-and-some-offsprings); [Rossing, ASA press room](https://acoustics.org/pressroom/httpdocs/139th/rossing.htm)]
- **DUELL's ISLAND STEEL (FM6):** the NYLON PICK patch (F8): carriers at ratios 1 and 2 (an octave apart) with modulators at 3 and 1, all harmonic [G: the pan's 1 : 2 : 3]; the modulators a little slower (MEG +11) for the ring, a low index (MLVL -7), velocity to brightness 3, a slight carrier spread (DTUN 11 %); reverb, a little delay. [S / G as marked]

## 4.13 Honky-tonk detune (SALOON KEYS, batch R)
- A honky-tonk upright's unison strings are out of tune with each other: the beating is the sound. [G] Here applied to the DX7-style electric piano of 4.1 (not a piano model: the name says keys).
- **DUELL's SALOON KEYS (FM6):** the TINE EP patch (F1) with the carriers spread far apart (DTUN 56 %), a harder strike (MLVL +7), velocity the expression (VMOD 3, S 4.1), dry with a small room and a little chorus. [S / G as marked]

## 4.14 FM tubular bell (TUBE BELLS, 0.2.17)
- **Method:** Chowning's FM bell: carrier : modulator = 1 : 1.4 (that is 5 : 7), the modulation index and the amplitude both falling exponentially. [S: John Chowning, 1973, as taught in the [SFU Truax FM tutorial](https://www.sfu.ca/~truax/fmtut.html) and a [University of Washington handout](https://courses.cs.washington.edu/courses/cse466/10wi/pdfs/FMSynthesisExamples.pdf): a bell of 200 Hz carrier, 280 Hz modulator, index about 10.] The decay times in seconds are not stated.
- **DUELL's TUBE BELLS (FM6, our own patch F10):** algorithm 5 (three pairs), the pairs at 1.0, 2.76 and 5.4 with a 1.4 modulator each [G], feedback 2, key and rate scaling; every operator falls to nothing (the bell rings out within about 9 s and a release of about half a second), REV 30 %, CHO 6 %.

---

# 5. FX / OTHER

## 5.1 Noise sweep / riser
- **Source:** white (or pink) noise, low-pass filter with a small resonance bump, cutoff swept up over 4-16 beats by envelope/LFO/automation; or high-pass/band-pass sweep; slow volume swell. [S: [MusicRadar sweep FX](https://www.musicradar.com/how-to/synth-sweep-fx) and [Violet Recording](https://violetrecording.com/how-to-make-risers-and-sweeps/) via search summaries]
- **Envelope:** FENV A = the riser length (2-16 s), D 0, S 100%, R short; AENV A long (1-8 s), S 100%, R 100-500 ms; downsweep/impact at the end. [G, consistent with sources]
- **FX:** reverb (dry -> very wet), delay, phaser with modulated rate, distortion or bitcrush at the peak, sidechain from kick. [S: same]
- **Variants:** pitch riser from a saw with pitch automated up; reversed hits/tails. [S: same]

## 5.2 Metallic glitch arp
- Short square/ring-mod/metallic-noise tones (inharmonic) with D 30-100 ms, S 0, arp 16ths up-down over 2-3 octaves, random gate, high resonance band-pass, light bitcrush, delay with feedback 40-60%. [G; metallic = ring mod or short shift-register noise, see sources on inharmonic spectra in 4.3]

## 5.3 Wind / texture
- Noise through a resonant band-pass (resonance 40-80%), cutoff moved by slow random S&H or two slow LFOs (0.05-0.5 Hz), colour from white to pink, AENV A 1-3 s, S 100%, R 2-4 s; long reverb. [G]
- **0.2.20, MOOR WIND (NOISE) parted from SEA ORGAN** (audit 3.96): browner noise (COLR 100), the band lower and wider (FREQ 44, RES 70), attack 1.3 s, and a slow PHASR insert (0.13 Hz, 60 %) in place of the SOFT limiter: the swirl of wind round a corner; End B (AMOUNT 27, seed 31000) still passes. [G]

## 5.4 Sci-fi FM bell cluster
- Several FM bell voices (see 4.3) with different non-integer ratios, a chord of seconds/tritones, random pitch per note or arp, long reverb/delay (ping-pong, feedback 40-60%), slight pitch drop envelope. [G; builds on the sourced bell recipe]

## 5.5 Bitcrushed lofi arp
- Chip-style pulse/saw (duty 25-50%), bit depth 4-8 bits, sample-rate reduction to ~8-16 kHz, A 0, D 80-200 ms, S 0-30%, arp 8th/16th notes in major/minor, mild vibrato, tape-style wow (0.3-1 Hz, +-5-15 ct), low-pass at 4-6 kHz, warm reverb. [G]

## 5.6 Slow-LFO drone
- Two or three slightly detuned saws/pulses, resonant low-pass with very slow LFO (0.02-0.2 Hz) on cutoff and a second (incommensurate) one on pulse width, depth 10-30%, A 2-4 s, S 100%, R 4-8 s, reverb large, feedback delay. [G]; see 3.7.

## 5.7 Frozen texture (FROZEN AIR, batch R; GRAIN's STRETCH role, DEAD BUFFER part 1: M03, M05 since 0.2.13)
- A granular "freeze": the read position held in one region and moved slowly, a few overlapping grains, slow automation or a low-rate LFO on the position; long envelopes, a big reverb. [S 3.4]
- **DUELL's FROZEN AIR (WAVE):** the AIR table read low (POS 20) and scanned slowly: FILTER ENV on POS (+30 % over 4 s) and LFO 2, a 0.09 Hz triangle, on it (+-20 %), aftertouch moves it too; A 600 ms, R 3.5 s, reverb 80 %, delay 25 %, the low EQ down 3; a slow free LFO 1 drifts the second read (DTN). The multimode filter is a 12 dB low-pass at CUT 60 (about 3 kHz) with RES 31, and both LFOs move the cutoff a little through the matrix, each at +30 % depth (a swing of about 19 CUT steps each way): LFO 1 (0.13 Hz triangle) and LFO 2 (0.09 Hz triangle) with the same sign, at unrelated rates, both free-running, so the brightness drifts slowly and never repeats; LEVEL 107 (was 106) makes up for the darker top. Its envelopes, scan and effects are STRETCH's, so M03 keeps its shape. [S 3.4 / G]

## 5.8 Reverse cymbal (REV CYMBAL, batch R)
- A reversed crash: noise in a high band swelling up and cut off at the end, as a tape played backwards. [S 5.1: reversed hits / tails as risers; G for the band and times]
- **DUELL's REV CYMBAL (NOISE):** white noise (ANLG, COLR 0) in a band at FREQ 107 with RES 21, the swell A 1.1 s with FILTER ENV opening the band along with it, R 37 ms (the abrupt end), reverb 37 %. [S / G as marked]

## 5.9 Sonar ping (SONAR PING, batch R)
- A pure sine ping with a long ring and a long echo (the submarine sonar of films). [G]
- **DUELL's SONAR PING (PHASE):** a sine (DCW 0) with a trace of bend at the strike (ENV 11), D 700 ms to a held 41 % (it rings on while held), R 1.7 s, delay 47 %, reverb 61 %. [G]

## 5.10 Lofi drone (DUSTY DRONE, batch R; SAMPLE's DECK WARP role)
- A crushed, dark, slowly moving drone (5.5 and 5.6): reduced bit depth, a low-pass, a very slow modulation of the tone, slow in, long out. [G 5.5 / 5.6] No pitch wobble (the house rule: no vibrato on a sound that is not a lead).
- **DUELL's DUSTY DRONE (LOFI):** 8BIT SAW, CRSH 47, TONE 57, LFO 2 at 0.27 Hz on TONE, A 400 ms, R 1.7 s, delay, reverb. [G]
- **0.2.20, DUSTY DRONE (LOFI):** a VINYL insert (DUST 40 clicks, NOISE 36, WARP 18): 'dusty' is now literal; A 1 s (was 400 ms: a drone that fades in, apart from STRING MACH, which the VINYL band-narrowing had moved it toward) [G]; parted from WOW FLUTTER (audit 5.81).

## 5.11 Telephone dial tone (DIAL TONE, batch R)
- North America's precise tone plan: the dial tone is 350 Hz and 440 Hz together, continuous (ringback 440 + 480 Hz, busy 480 + 620 Hz). [S: [Wikipedia, Precise tone plan](https://en.wikipedia.org/wiki/Precise_tone_plan)]
- **DUELL's DIAL TONE (TRIO):** two triangles four semitones apart (1.260; 440 / 350 = 1.257) low-passed toward sines (CUT at 900 Hz); F4 plays 349 + 440 Hz. [S / G as marked]

---

# 6. PERC (batch R: the PERC sounds of the retired SLICE replaced)

## 6.1 Analogue bass drum (DEEP KICK)
- **TR-909:** a sawtooth through a waveshaper, "very close to a sine wave", its pitch set by an envelope with an instant attack and a slow decay; a click (a pulse) mixed in for the beater. **TR-808:** a bridged-T oscillator that decays by itself, dropping slightly in pitch as it decays. [S: [SOS, Practical Bass Drum Synthesis](https://www.soundonsound.com/techniques/practical-bass-drum-synthesis)]
- **Physics:** a struck head is at a higher pitch at the start and falls as it decays, by "around 10 percent" (a couple of semitones), while the loudness falls by 100 %; one decay rate for the important partials. [S: [SOS, Synthesizing Drums: The Bass Drum](https://www.soundonsound.com/techniques/synthesizing-drums-bass-drum)]
- **DUELL's DEEP KICK (PHASE):** a sine (SAW at DCW 0) with a short bend at the strike (ENV 20: the click), D 450 ms, S 0; FILTER ENV (40 ms) throws the pitch up an octave at the strike [G], the amp envelope lets it settle by about 10 % (AMP PIT 9) [S]; pressure to the bend. A hit: trimmed on its peak. [S / G as marked]
- **0.2.20, DEEP KICK is a kick, not a tone at the key** (audit: 4.76 from MALLET WOOD): it sat at the key's pitch (130 Hz at C3, a marimba's fundamental) with a one-octave drop. Now the whole sound is one octave down (a 65 Hz body at C3; one EXPR route of -64 = exactly -12 semitones), the pitch falls 15.6 semitones (2.5x) in 60 ms (FILTER ENV on PITCH, routes +63 and +20), the click is harder (ENV 56), no reverb, and the 450 ms sub tail is unchanged. [S 6.1: the 909 / 808 drop and decay; numbers G]

## 6.2 Tom (FLOOR THUD)
- A struck membrane: the modes of a circular head (inharmonic for an ideal head, toms and timpani), the pitch falling by about 10 % after the strike, one decay. [S: [SOS, Synthesizing Drums: The Bass Drum](https://www.soundonsound.com/techniques/synthesizing-drums-bass-drum); `eng_phys.c` MEMB] Felucca's own TOMS preset is the starting point (MEMB, HARM 0, BRIT 52, DAMP 40, POS 30, BEND 40, EXC 30). [S: `eng_phys.c` PHYS_PRESETS]
- **DUELL's FLOOR THUD (PHYS, MEMB):** HARM 0, BRIT 52, DAMP 71 (a floor tom rings longer), POS 30, ACC 100, BEND 17 (about 1.6 semitones: the 10 %), EXC 27 (the stick), R 170 ms; pressure to the drop. [S / G as marked]

## 6.3 Cowbell (FIELD CLANK)
- **TR-808 / CR-8000:** two of the pulse oscillators, about 587 Hz and 845 Hz (1 : 1.44), through a band-pass that removes the upper and lower partials, under one contour of two stages: a short, high-amplitude impact, then a longer tail. A recreation used a 2.64 kHz band-pass, 12 dB/oct, a little resonance. [S: [SOS, Synthesizing Cowbells & Claves](https://www.soundonsound.com/techniques/synthesizing-cowbells-claves)] The CB808 module: "two square wave oscillators ... band pass filtering". [S: [Tiptop Audio CB808](https://www.ctrl-mod.com/products/tiptop-audio-cb808)]
- **DUELL's FIELD CLANK (TRIO):** PLS3 (squares), osc 2 six semitones up (1.414), osc 3 on osc 1, BP at 2.64 kHz with RES 27; the amp envelope's tail (D 300 ms) under FILTER ENV's 30 ms opening of the band (the impact) [times G]; it follows the key: around C5 it sits at the 808's pitch. [S / G as marked]

## 6.4 Hardstyle kick (HARD KICK, 0.2.17)
- As 1.13 for the drop and the clipping. [S: EDMProd, ADSR Sounds; numbers G.] The sine is DEEP KICK's (6.1).
- **DUELL's HARD KICK (PHASE):** a sine (SAW at DCW 0) with FILTER ENV (D 75 ms) on PITCH through two routes, ENV 14 for the click; AMP D 330, S 0, R 100; the HARD insert (A 127) and DIST 38 %; two octaves down in all (two routes EXPR to PITCH at -64: -24 semitones, 0.2.20: F3 gives 44 Hz, C4 65 Hz); no ducker, dry.

---

# 7. FACTORY MULTIS (DUELL 0.2.11: M05..M10; 0.2.12: driven by the keys; 0.2.13: in M01, M02, M07..M10; 0.2.23: all sixteen slots)

**DUELL 0.2.25: the owner's edited multis M02..M06 are the factory multis** (his backup of 2026-10-17, imported by `tools/import_owner_edits.py`, byte for byte; no value changed). M02 NIGHT ORGAN and M03 PAD SPLIT have no split any more (M03's part 1 is #61 EVOLVING PAD, was #19 PLUCK BASS), M04 DUB CHORD and M05 WHISPER TIDE are edited, M06 is now his TINE STRINGS (#54 STRING MACH + #83 TINE ROOM, 77 BPM) in place of COLD CHAPEL; the 0.2.23 TINE STRINGS (7.11) is VELVET TINES in the spare list. The descriptions of M02..M06 (7.2, 7.7, 7.8, 7.10) are deleted: they described sounds he has since changed; names, parts and tempos stay, the records are the truth.

**DUELL 0.2.25 (owner): his HAZY TENSOR is M08** ("Mono-dual is out"): his multi file, byte for byte (`felucca-base/tools/factory_multis/M08.bin`, 7.17). MONO DUEL (7.12), which was M08 and the only factory multi in DUEL, is spare data now (`tests/scenes/spare/M08_MONO_DUEL.bin`, the generator's SPARE list). No factory multi uses DUEL since.

**DUELL 0.2.25 (owner): his original COLD CHAPEL (7.1) is a factory multi again, in M13** (M06 stays his edited TINE STRINGS; M01..M06 are his and are not touched). It took the slot of BASILDON (7.13), the multi least different from the rest by the similarity measure (the three candidates MONO DUEL, BASILDON and WOOD & SKIN scored against all the other multis; BASILDON the highest). BASILDON is spare data now (`tests/scenes/spare/M13_BASILDON.bin`, the generator's SPARE list) and can come back.

**DUELL 0.2.24: the owner's multis back** (owner, 2026-10-16: his own multis are never dropped or changed). 7.1's COLD CHAPEL, 7.3's GARGLE ACID and ACID ETHER (0.2.1) are factory multis again, byte for byte the 0.2.22 records, in M06, M14 and M09 (`owner` in `tools/factory_multis.py`: the similarity check reports a pair with one, never fails it). Out of the sixteen for them, as spare data (`tests/scenes/spare/`, the generator's SPARE list): 7.11 TINE STRINGS and 7.14 SEVENOAKS (both a PAD + KEYS layer in LAYER, as the owner's EERIE SIGNAL: the check's own measure) and 7.9's drone (it shared SINGING BOWL with COLD CHAPEL and had taken its name; BOWL & WIND now). FINAL REEL is M07, MONO DUEL M08. M03 PAD SPLIT's pad: 7.8.

**DUELL 0.2.23: sixteen factory multis** (the list and the order: SPEC 5b.4; the values: `felucca-base/tools/factory_multis.py`, which builds them). Kept byte for byte: M01 REESE SWAMP (7.4), M05 WHISPER TIDE (7.2), M08 FINAL REEL (7.6), and the three of 0.2.1, M10 WIRED SNAIL, M11 EERIE SIGNAL, M12 DEAD BUFFER. Retired in 0.2.23 (back in 0.2.24, above): GARGLE ACID (7.3) and ACID ETHER (each an acid line over a bass, as REESE SWAMP), and 7.1's COLD CHAPEL (a pad and a slow latched arp, as WHISPER TIDE); the name COLD CHAPEL was a new drone in 0.2.23 (7.9). NIGHT ORGAN (7.5) is rebuilt as a split (7.7). New: 7.7 to 7.15. In LAYER the keys give part 2 the note part 1 maps (part 1's TRN moves both parts; part 2's TRN is not heard from the keys), so the octave between two parts comes from part 1's TRN, the SEQUENCER offsets or the arp's OCT. Loudness (`gen_factory_multis loud`, MODE RECALL ON, C4 E4 G4): the new ones between -16.7 and -20.4 dB (the kept ones -11.3 to -20.1), End B within 1.5 dB of End A; the part LEVELs (the mixer, 0.5 dB a step) are set for that, the sounds' TRIMs are their own.

Six multis built from existing factory sounds (**0.2.13 order (owner): M01 REESE SWAMP, M02 WHISPER TIDE, M03..M06 the four older ones (EERIE SIGNAL, WIRED SNAIL, DEAD BUFFER, ACID ETHER), M07 COLD CHAPEL, M08 GARGLE ACID, M09 NIGHT ORGAN, M10 FINAL REEL**; a new unit starts with M01; no new sound; the numbers are PATCHES-list numbers, docs/PATCH_NUMBERS.md) with per-part settings on top. All six are in **LAYER** (the keys play both parts). **0.2.12 (owner): they are for performing from the keys.** Whatever is held, both parts answer it; nothing plays on its own and PLAY is not needed: there are no stored step patterns that play from fixed notes, every groove is an **arp on the held keys**, which runs on the internal clock from the key-down (SPEC 2.3). The riffs are SEQUENCER arps: one entry a step, a note **offset** in semitones from the held key (stored as C4 + the offset, what the SEQUENCER mode reads), so a riff follows the key; a SEQUENCER part's steps are the arp's alone (PLAY does not play them from C4 too, seq.c `seq_play`). The SEQUENCER arp has no slide (its gate is the arp's GATE, every step alike) and ties are rests, so the 0.2.11 slides are gone. Speeds (owner): the ambient and cinematic ones slow (1/2, 1/1 or held chords), 1/16 only for the short acid lines, the house one calmer (1/8, a half-time stab). The global SCALE is not stored in a multi; offsets are in semitones (moved onto the scale when one is set) and the chord keys use a fixed shape (MIN7), never DIA3 / DIA7. The global SWING is the step sequencer's: the arps have their own (ARP 2/3 SWG, 0 here). TEMPO is stored in a multi. Built by `felucca-base/tests/gen_factory_multis.c` (the values are in its table); loudness by its `loud` mode: each multi in its own MODE (as MODE RECALL ON: LAYER; 0.2.12: before, with MODE RECALL OFF, the keys played part 1 alone), K-weighted (BS.1770 filters, no gate) on the master output after MASTER at its default, seconds 2..10 of a 10 s run with C4 E4 G4 held (and C4 alone). The four older multis measure EERIE SIGNAL -11.4, WIRED SNAIL -19.4, DEAD BUFFER -17.7, ACID ETHER -19.7 dB (M03..M06 since 0.2.13); EERIE SIGNAL is the known loud one (its part 1), so the new six aim at the other three: with the chord held every one lands between -18.4 and -20.2 dB, the part LEVELs set for that (noted per part). Pattern notation: one entry per arp step, "." a rest, "+12" / "0" / "-5" the offset from the held key, (acc) an accent. CPU (`CPU_SCEN=N` of `tests/cpu_scenarios_test.c`, 4 + 4 notes or 8 on part 1, the MORPH fader at 63; 0.2.11 numbering): M05 60 / 66 %, M06 82 / 88 %, M07 38 / 30 %, M08 39 / 31 %, M09 42 / 49 %, M10 70 / 68 % (0.2.13, in the new slots: M07 COLD CHAPEL 70 / 86 %, M02 WHISPER TIDE 82 / 88 %, M08 GARGLE ACID 38 / 30 %, M01 REESE SWAMP 39 / 33 % with End B at 17 %, M09 42 / 49 %, M10 70 / 68 %) (that scenario holds 4 notes on part 2 without its arp: an arp part plays fewer).

Sources for the playing styles (fetched 2026-10-14, data only):
- Ambient pads and drones: slow attack, long release, lots of reverb and processing, very slow LFOs (about 0.01 Hz unsynced, or 8-bar synced) on the wavetable position and the filter. [S: [MusicRadar, ambient drone](https://musicradar.com/how-to/create-ambient-drone-dune-3), [MusicRadar, ambient pad](https://musicradar.com/how-to/how-to-create-an-ambient-synth-pad-from-scratch), [Future Music producer's guide](https://www.pressreader.com/australia/future-music-9629/20170309/281535110777069)] The latch (HOLD) is not covered by these sources: it is the owner's playing method for these two (keys and HOLD, no pattern). [G]
- Acid line: 1.2 above (a 16-step pattern with accents and slides is the instrument; overdrive and a dotted delay). [S] Growl: 1.7 (an LFO synced to the tempo on the table and on a filter). [S] Reese: 1.5. [S] Wobble: an LFO synced to a note value on the low-pass is the same idea as 1.7's filters moved by the LFO. [G]
- House organ stab: an organ (the Korg M1's organ patches are the classic source) as a minor-seventh chord from one key, short attack and decay, no sustain, on the off-beats between the kicks. [S: [MusicRadar, classic rave chord stab](https://musicradar.com/how-to/rave-chord-stab), [MusicRadar, classic dance sounds](https://musicradar.com/how-to/classic-dance-sounds), [The Producer School, chord stabs](https://theproducerschool.com/blogs/featured-blogs/how-to-make-chord-stabs-like-modern-rave-the-hard-house-and-uk-garage-sound)] House bass: on the off-beats between the kicks (16-step positions 3, 7, 11, 15), the "double off-beat" pairs (3-4, 7-8, ...), the root with octave or fifth moves; swing about 8-20 %. [S: [Violet Recording, house beats](https://violetrecording.com/how-to-make-house-beats/), [The Producer School, hard house bass patterns](https://theproducerschool.com/blogs/featured-blogs/5-essential-bass-patterns-that-define-hard-house-and-trance-music), [Amped Studio, house drum patterns](https://ampedstudio.com/blog/how-tomake-a-house-beat/)]

## 7.1 M13 COLD CHAPEL (the owner's original; ambient, slow, LAYER, 71 BPM; M05 until 0.2.12, M07 in 0.2.13 .. 0.2.22, M06 in 0.2.24, M13 since 0.2.25 in the slot of BASILDON: byte for byte the 0.2.22 record)
- **Part 1: #61 EVOLVING PAD** (WAVE; 3.4 / 5.6): the slow drone bed, its own two slow LFOs on the scan. Per part: HOLD ALL (the keys latch, the next chord replaces it), arp OFF, REV 81, DLY 37, LEVEL 90. Matrix slot 8 MORPH -> CUT +37 % (End B opens its low-pass) [G]; End B seed 38479. [G]
- **Part 2: #56 SINGING BOWL** (PHYS; 3.7): the bowed metal texture, an octave up (TRN +12), its own chord keys POW / OPEN (fifths). **0.2.12: an arp on the latched chord**: MODE UP, RATE 1/2, GATE 115 (90 %: one bowl note rings into the next), OCT 1, HOLD ALL (it goes on after the keys are let go; the next chord replaces it), VOICE MONO (one bowl at a time: the PHYS voices were the CPU's 96 %). REV 87, DLY 27, LEVEL 93; slot 8 MORPH -> BRIT (E3) +27 % [G]; End B seed 19072. [G]
- **Playing:** a chord: the pad holds it, the bowl walks up its notes (and their fifths) a half note each. End B opens both.
- **Globals:** delay 1/4 dotted, FDBK 71, MIX 97, PING-PONG on, delay modulation 17 % (a slow tape-like drift); HALL, SIZE 117, DAMP 57 (3.7 / 5.6: a large reverb and a feedback delay [G]). No steps.
- Loudness -20.2 dB (C4 E4 G4); End B -19.7 dB and brighter (x1.5 of the RMS frequency).

## 7.2 M05 WHISPER TIDE (the owner's, LAYER, 77 BPM; M06 until 0.2.12, M02 in 0.2.13 .. 0.2.22)
- The owner's multi (he edited it on his unit; imported byte for byte in 0.2.25, `felucca-base/tools/factory_multis/Mnn.bin`). Parts: #60 DIGITAL PAD + #52 WHISPER PAD. No description here: the record is the truth.

## 7.3 M14 GARGLE ACID (bass, fast, LAYER, 127 BPM; M07 until 0.2.12, M08 in 0.2.13 .. 0.2.22, out in 0.2.23, back as M14 in 0.2.24: the owner's, byte for byte)
- **Part 1: #21 GROWL BASS** (WAVE; 1.7): **0.2.12: plays the held key**, two octaves down (TRN -24; LEGATO, its own), arp OFF, HOLD OFF. The moving vowel (LFO 2 at 1/2 on the table) plus a gargle: LFO 1 synced to 1/8 (it already warps the read) also opens the multimode low-pass through matrix slot 8 (LFO1 -> CUT +27 %), the low-pass at about 700 Hz (CUT 41), RES 31 % [S 1.7: the filters moved by the same LFO; the numbers G]. LEVEL 110.
- **Part 2: #5 SQUELCH** (ANALOG; 1.2): the 303 line an octave above the bass (TRN -12) as a **SEQUENCER arp on the held key**: RATE 1/16, GATE 64 (50 %), HOLD OFF, LEVEL 92. Pattern (16, offsets from the key):
  `0(acc) . +12 0 . +10 +12(acc) . 0 . +3 0 +7(acc) . +12 .` (root, octave jumps, the minor seventh and third, the fifth: the acid idiom of 1.2 [S]; the notes [G]).
- **Globals:** delay 1/8 dotted, FDBK 37 (1.2: dotted, 30-40 % [G]), ROOM SIZE 51, PING-PONG off. A key: the bass holds it, the riff runs on it; let go and both stop.
- Loudness -18.6 dB (C4 E4 G4), -19.2 dB (C4).

## 7.4 M01 REESE SWAMP (bass, fast, LAYER, 124 BPM; M08 until 0.2.12; the first start's multi since 0.2.13)
- **Part 1: #24 REESE BASS** (WAVE; 1.5): **0.2.12: plays the held key**, two octaves down (TRN -24), arp OFF. The detuned saws, the low-pass lowered to about 1.6 kHz (CUT 52) and wobbled at 1/4 (LFO 1 synced, slot 8 LFO1 -> CUT +33 %; it also moves the table's phasing) [G]. LEVEL 113.
- **Part 2: #5 SQUELCH** (ANALOG; 1.2): an 8-step **SEQUENCER arp on the held key** (LEN 8: twice a bar), two octaves above the bass (TRN 0): RATE 1/16, GATE 61, HOLD OFF, LEVEL 87. Pattern (8):
  `. 0 +12(acc) +10 . +3(acc) 0 +7`.
- **Globals:** delay 1/8 dotted, FDBK 41, ROOM SIZE 57. **0.2.12: no key split** (it was applied only with MODE RECALL ON anyway).
- Loudness -18.4 dB (C4 E4 G4), -20.0 dB (C4).
- **0.2.13 (owner: "End B barely changes anything" at the sounds' AMOUNT 7 %): AMOUNT 17 % on both parts**, End B rolled again at 17 % (part 1 seed 11710, part 2 seed 41045; `gen_factory_multis search` / `eval`): End B is clearly brighter, its level the same. Measured (`eval 1`, both faders at B): C4 E4 G4 -18.4 dB / 411 Hz at End A, -18.6 dB / 638 Hz at End B (brightness +55 %); C4 -19.8 / 378 Hz, -19.6 / 550 Hz (+46 %); the Reese alone +91 % (part 1 at B: more DIST, its filter MODE and the table position moved, less detune), the acid line alone +18 %. Before (7 %, seeds 5220 / 17703): End B -19.3 dB and 18 % darker (334 Hz). CPU (`cpuscen` Q, 4 + 4 notes): 34.6 % at A, 39.0 % at the fader's middle, 39.1 % at B.

## 7.5 M09 NIGHT ORGAN of 0.2.11 .. 0.2.22 (house, calm, LAYER, 121 BPM; 0.2.23: rebuilt as a split, 7.7)
- **Part 1: #91 DRAWBARS** (WAVE; 4.5): the organ stab. Chord keys MIN7 / CLOSE (a held key plays a minor seventh, the classic house stab; MIN7 rather than DIA7, which needs the global scale). **0.2.12 (owner: the 16th stabs were too fast): a SEQUENCER arp of 8 steps at 1/8** that re-strikes the chord of the held key on the off-beats of beats 1 and 3 (a half-time feel), GATE 100 (78 % of an eighth, about 190 ms), HOLD OFF; DLY 17, REV 21, LEVEL 106. Pattern (8):
  `. 0 . . . 0(acc) . .`.
- **Part 2: #3 LADDER BASS** (ANALOG; 1.1): the off-beat house bass (SEQUENCER arp, 8 steps at 1/8, TRN -12), the key on every off-beat 8th, the last an octave up (SPEC 3.7's P03 OFFBEAT idiom [S]). GATE 80, HOLD OFF, LEVEL 99. Pattern (8):
  `. 0 . 0 . 0 . +12`.
- **Globals:** swing 17 % kept (it moves the step sequencer only; the arps run straight), delay 1/8 dotted FDBK 31, ROOM SIZE 71. The downbeats are left open for a kick from a drum machine. A key: the chord and the bass on its root; another key moves both.
- Loudness -18.4 dB (C4 E4 G4: three MIN7 chords), -17.9 dB (C4).

## 7.6 M07 FINAL REEL (the owner's, LAYER, 71 BPM; M10 until 0.2.22, M08 in 0.2.23, refined by him in 0.2.25)
- The owner's multi (he refined it on his unit; imported byte for byte in 0.2.25 from his `duell-multis` file, `felucca-base/tools/factory_multis/Mnn.bin`). Parts: #58 BRASS PAD + #54 STRING MACH. No description here: the record is the truth. (The 0.2.11 version, brass and strings latched with HOLD ALL, HALL, 71 BPM, is in git.)

## 7.7 M02 NIGHT ORGAN (the owner's, LAYER, 121 BPM; 0.2.23, edited by him in 0.2.25)
- The owner's multi (he edited it on his unit; imported byte for byte in 0.2.25, `felucca-base/tools/factory_multis/Mnn.bin`). Parts: #18 PEDAL BASS + #91 DRAWBARS. No description here: the record is the truth.

## 7.8 M03 PAD SPLIT (the owner's, LAYER, 97 BPM; 0.2.23, edited by him in 0.2.25)
- The owner's multi (he edited it on his unit; imported byte for byte in 0.2.25, `felucca-base/tools/factory_multis/Mnn.bin`). Parts: #61 EVOLVING PAD + #44 EMBER GLOW. No description here: the record is the truth.

## 7.9 BOWL & WIND (cinematic drone, LAYER, 71 BPM; M07 in 0.2.23 under the name COLD CHAPEL; 0.2.24: spare, `tests/scenes/spare/M07_BOWL_AND_WIND.bin`, the record still says COLD CHAPEL)
- A drone: a bowed metal bowl and wind, both latched, a large hall and a long ping-pong delay with a slow drift (3.7 / 5.3 / 5.6: slow, long, lots of reverb [S, the ambient sources above]).
- **Part 1: #56 SINGING BOWL** (PHYS; 3.7): HOLD ALL, TRN -12 (an octave down; its own POW / OPEN chord keys), REV 87, DLY 27; slot 8 MORPH -> E3 +27 %; End B rolled again (seed 7869 at 27 %: End B within 0.2 dB, brighter); LEVEL 95.
- **Part 2: #112 MOOR WIND** (NOISE; 5.3): HOLD ALL (the wind does not follow the key: TRK 0), REV 71, DLY 17; slot 8 MORPH -> CUT +17 %; LEVEL 97.
- **Globals:** delay 1/4 dotted FDBK 71 MIX 97, PING-PONG, MDEPTH 17; HALL SIZE 117 DAMP 57. Loudness -17.1 dB, End B -17.3 dB and brighter.

## 7.10 M04 DUB CHORD (the owner's, LAYER, 117 BPM; 0.2.23, edited by him in 0.2.25)
- The owner's multi (he edited it on his unit; imported byte for byte in 0.2.25, `felucca-base/tools/factory_multis/Mnn.bin`). Parts: #80 BIG STAB + #6 TIDAL SUB. No description here: the record is the truth.

## 7.11 VELVET TINES (named TINE STRINGS until 0.2.24; electric piano and strings, LAYER, XFADE 37, 77 BPM; M06 in 0.2.23; 0.2.24: spare, `tests/scenes/spare/M06_TINE_STRINGS.bin`)
- The 80s / 90s keyboard layer: a DX electric piano over a string pad, the velocity bringing the piano forward [G]; a velocity crossfade blends two layers by how hard the key is played [S: [VSL forum on velocity crossfade](https://forum.vsl.co.at/post/132242)].
- **Part 1: #46 VELVET BOW** (ANALOG; 3.3, the PWM strings): REV 47; slot 8 MORPH -> CUT +33 %; End B rolled again (seed 25230 at 27 %: within 0.4 dB, brighter); LEVEL 101.
- **Part 2: #83 TINE ROOM** (FM6; 4.1): DLY 11, REV 31; AMOUNT 27 %; LEVEL 112.
- **XFADE 37 %:** soft notes lean to the strings, hard ones to the tines (keys count as velocity 100). **Globals:** delay 1/4 dotted FDBK 27 MIX 71; PLATE SIZE 77 DAMP 61. Loudness -16.7 dB, End B -16.4 dB.

## 7.12 MONO DUEL (two mono voices for outboard effects, DUEL, 117 BPM; M09 in 0.2.23, M08 in 0.2.24 .. 0.2.25; spare since 0.2.25, `tests/scenes/spare/M08_MONO_DUEL.bin`: the owner's HAZY TENSOR has M08; it was the only factory multi in DUEL)
- DUEL puts part 1 hard left and part 2 hard right, nothing of one on the other's side (SPEC 5b.1): two dry mono synths for two pedal chains [G].
- **Part 1: #15 BREADBIN** (TRIO; 1.1, the Minimoog-style bass): LEGATO, TRN -12, no sends, no ducker; LEVEL 117. **Part 2: #27 SQUARE LEAD** (ANALOG; 2.12, the square lead with glide): LEGATO, no sends; LEVEL 117.
- The keys play the selected part (ALGORITHM picks it), MIDI channel 2 plays part 2. Loudness (part 1) -20.4 dB.

## 7.13 BASILDON (tribute: Essex synth-pop, LAYER, 117 BPM; M13 in 0.2.23 .. 0.2.25; spare since 0.2.25, `tests/scenes/spare/M13_BASILDON.bin`)
- A tribute (owner: at most 2 or 3 tribute multis): early synth-pop, a step-sequenced bass in 8ths under a metallic riff [S: [MusicRadar, Depeche Mode synths](https://www.musicradar.com/how-to/depeche-mode-synths) (16-step sequencing, the ARP 2600), [Syntorial, Everything Counts bass](https://www.syntorial.com/preset-recipe/depeche-mode-everything-counts-bass/)]; no melody copied.
- **Part 1: #7 FM BASS 2** (FM6; 1.15): TRN -12, a SEQUENCER arp at 1/8, GATE 71: `0(acc) 0 0 0 0 0 0 +7` (SPEC 3.7 P06 8TH PULSE); LEVEL 109.
- **Part 2: #86 TIN TOLL** (TRIO; 4.3, ring modulation: the metal): a SEQUENCER arp at 1/16, GATE 57: `+12(acc) . +19 +12 . +22 +19 . +12(acc) . +15 +17 . +19 . +10` (root, fifth, octave, minor seventh, third and fourth: a minor riff an octave over the bass); DLY 31, REV 41; LEVEL 101.
- **Globals:** delay 1/8 dotted, PING-PONG, FDBK 31; PLATE SIZE 71 DAMP 57 (the 80s plate [G]). Loudness -18.4 dB.

## 7.14 SEVENOAKS (tribute: Kent's rave-era duo, LAYER, 127 BPM; M14 in 0.2.23; 0.2.24: spare, `tests/scenes/spare/M14_SEVENOAKS.bin`)
- A tribute: a fast arpeggio over several octaves cascading over a held pad, a dotted delay on it [S: [MusicRadar, Jupiter-style arpeggio](https://musicradar.com/how-to/how-to-create-a-jupiter-style-synth-arpeggio), [Tunee, arpeggio production](https://www.tunee.ai/music-generator/arpeggio-production)]; no melody copied.
- **Part 1: #48 LAMPLIGHT** (PHASE; 3.3): chord keys OFF (the player's chord), REV 57; slot 8 MORPH -> SHP +27 % (DCW opens); LEVEL 90.
- **Part 2: #81 FROST BELLS** (FM6; 4.3): an arp UP at 1/16 over 2 octaves, GATE 71, DLY 37, REV 47; LEVEL 93.
- **Globals:** delay 1/8 dotted PING-PONG FDBK 41; HALL SIZE 97 DAMP 61. Loudness -18.3 dB.

## 7.15 M15 WOOD & SKIN (drum-less percussion, LAYER, 107 BPM; 0.2.23)
- Two pitched percussion parts that interlock, no drum sounds: a mallet arpeggio and a hand drum line [G].
- **Part 1: #71 MARBLE RUN** (LOFI; 4.4, an 8-bit marimba with its own arp UP 1/8 over 2 octaves): DLY 27, REV 27; LEVEL 104.
- **Part 2: #126 TABLA TALK** (PHYS; hand drum): a SEQUENCER arp at 1/16, GATE 64: `0(acc) . 0 +7 . 0 . +5(acc) 0 . +7 . 0(acc) . +12 .`; REV 21; LEVEL 112.
- **Globals:** delay 1/8 dotted FDBK 27 MIX 71; ROOM SIZE 61. Loudness -17.6 dB.

## 7.16 M16 STRESS TEST (a CPU test, not music; 0.2.23)
- The owner plays it to see how many notes the unit takes (docs/TESTING.md). The two dearest sounds by the host count (`cpu_scenarios_test` per voice: PHYS about 17 to 20 %, WAVE about 8 to 9.5 %): **#37 PAN WHISTLE** (PHYS, POLY) and **#60 DIGITAL PAD** (WAVE, POLY, an arp UP 1/16 over 2 octaves, GATE 127, its long release overlapping). Both: the multimode filter HP 24 dB at 61 Hz (RES 17: it runs, the sound barely changes), an insert (FLANG / CHOR, MIX 97), SAT 41, EQ +4 / -3 / +3 dB, the DUCK PUMP 1/4 DEPTH 41 %, CHO 71, DLY 61, REV 71, LEVEL 112. **Globals:** 137 BPM, chorus DEPTH 77, the delay RANDOM (CHANCE 71 %) 1/8 FDBK 61, SHIM SIZE 107 with PITCH BOTH and MOD 51. The master COMP is the unit's (turn it on by hand). Host model (`CPU_SCEN=S`): LAYER 1 key 78 %, 2 keys 110 %, 3 and more 136 % (8 voices); DUEL with part 2 on channel 2: 92 / 124 / 150 %.

---

# ENGINE MAPPING NOTES

Read from `felucca-base/firmware/src/eng_*.c` (headers, `edit[]` tables, render code) and SPEC.md section 2.x on DUELL's modulation. Labels are those shown on the device (EDIT pages). "E-slot" order = the 8 EDIT values left to right, two pages of four (page titles in brackets).


## 7.17 M08 HAZY TENSOR (the owner's, LAYER, 117 BPM; 0.2.25, in the slot of MONO DUEL)
- The owner's multi (he made it on his unit; imported byte for byte in 0.2.25 from his `duell-multis` file, `felucca-base/tools/factory_multis/Mnn.bin`). Parts: #3 LADDER BASS + #20 GEARBOX. No description here: the record is the truth.

## Modulation conventions (all engines)
- **AMP ENV = ENV 1** (ATK DEC SUS REL shape every note's volume; AMP DEST adds FLT/PIT/SHP depth). **FILTER ENV = filter envelope**; page **FILTER ENV DEST: FLT, SHP**. **LFO 2 DEST: PIT, FLT, SHP** (LFO 1 DEST also has AMP). [SPEC.md, line "Classic arrangement"]
- "FLT" is the track's generic cutoff input. Each engine routes it to its own nearest tone control (listed per engine below). "SHP" is the generic shape input, routed per engine as the task and SPEC.md state (and as the code confirms for every engine below). Both are offsets around the knob value (centre = knob as set).
- In recipes, "FENV amount" = FILTER ENV FLT depth (SPEC says new factory sounds set FILTER ENV FLT about +40, range -64..+63); "FENV shape sweep" = FILTER ENV SHP.
- Velocity/aftertouch/mod wheel go through the 8-slot matrix (sources include velocity, aftertouch, mod wheel, MORPH; destinations include every knob parameter). [SPEC.md]

## ANALOG  [OSC: WAVE DTN MIX NOIS | FLT: CUT RES DRV KTR]
- WAVE: SAW SQR TRI SIN PWM SYNC SUB. DTN = osc 2 detune in cents (shown SYNC under WAVE SYNC: osc 2 above osc 1, 3/16 semitone a step, up to +23.8 st). MIX = osc 1/2 balance (shown SUB under WAVE SUB: sub level, osc 2 a square one octave below). NOIS = noise level. CUT/RES = state-variable filter (the filter TYPE LP/BP/HP is on EDIT > FILTER). DRV = drive. KTR = key tracking.
- Maps to: cutoff = CUT (FLT input adds), resonance = RES, osc mix = MIX, drive = DRV.
- **SHP:** pulse width on PWM (and the pulse waves), sync detune on SYNC (3/16 semitone a step), so FILTER ENV SHP/LFO 2 SHP gives PWM and the sync sweep.
- Suits: Moog bass, 303 acid (WAVE SAW/SQR, CUT low, RES 90-110, DRV, FILTER ENV FLT + short decay), sub bass (SUB or SIN), Reese (SAW, DTN 20-60 ct, MIX 64), pluck bass, Moog mono lead, sync lead (SYNC + FILTER ENV SHP), brassy lead and brass stab, PWM string pad and Juno-style pad (PWM + LFO 2 SHP), dark drone, noise sweep/wind (NOIS up, WAVE low, CUT sweep). Only two oscillators: a true seven-voice supersaw is approximate (use DTN + chorus). Factory presets of the engine include SAW LEAD, SOFT PAD, SQR BASS, PWM STR, ACID, SUB BASS, PLUCK, BRASS, WIND, STRINGS.

## TRIO  [OSC: WAVE INT2 INT3 DTN | TONE: MODE CUT RES PW]
- WAVE: 16 sets (SAW3 PLS3 PPT SST TRI3 S&T P&S P+N NOIS SYNC SYNCP SYNC3 RING RING3 R+S R+SP), i.e. three oscillators with ring mod and hard sync. INT2/INT3 = interval of osc 2/3 in semitones. DTN = osc 2 up / osc 3 down in cents (0-50). MODE: LP BP HP NOT, 12 dB/oct. Cutoff drifts slowly at random.
- Maps to: cutoff = CUT, resonance = RES (stops short of self-oscillation), osc interval = INT2/INT3, pulse width = PW.
- **SHP:** PW (duty).
- Suits: 8-bit/computer-chip leads and basses, sync lead, ring-mod bells and metallic arps (RING, R+S), fat bass (FAT BASS preset), chip choir. Preset names: FAT BASS, ARP LEAD, SYNC LEAD, RING BELL, CHIP CHOIR.

## PHASE  [PHS: WAVE WAVE2 DCW ENV | LINE: DTN LINE SUB (-)]
- Phase-distortion (Casio CZ-style). WAVE/WAVE2 (alternate every other cycle), DCW = amount of phase bend (the "brightness/cutoff" analogue; no filter, no resonance control), ENV = amount of envelope into DCW (the code adds `envq15 x ENV` to DCW; which of ENV 1/FILTER ENV feeds `envq15` I did not confirm), DTN = second line detune (ct), LINE = MIX or RING for that second line, SUB = sine an octave down. Waves SAW SQR PLS DSIN SPLS RSAW RTRI RTRP (R-waves are the CZ "resonant" waves; this reading is from the CZ convention, not stated in the file).
- Maps to: cutoff ~ DCW; "resonance" ~ choose RSAW/RTRI/RTRP waves; osc mix = SUB, LINE.
- **SHP:** distortion depth (added to DCW, so SHP and FLT both move DCW here).
- Suits: brass (preset BRASS), organ, strings (STRING), resonant bass or lead (RESO), FM-ish bells (BELL), sync-like WIRE sounds. 

## LOFI  [LOFI: BITS WAVE DUTY CRSH | MOTN: SWP VIB ARP TONE]
- BITS: 4BIT 4B/2 8BIT 1BIT STEP. WAVE: PLS TRI SAW NOIS WRAM (16 wave-RAM tables, DUTY selects table). DUTY = pulse width. CRSH = bit crush (shown DCY, the stepped envelope, when BITS is STEP). SWP = pitch sweep down after note-on. VIB = vibrato. ARP: OFF OCT MAJ MIN (built-in fast arp). TONE = low-pass (FLT input adds to it).
- Maps to: cutoff = TONE, resonance = none, osc mix = none (single voice).
- **SHP:** pulse width around DUTY (PLS and STEP's pulse, smooth, +-12.5%; not the WRAM table).
- Suits: chip/8-bit lead (PULSE LD, WAVE LEAD, STEP LEAD), wave bass (WAVE BASS), 8-bit and bitcrushed lofi arp (ARP, CRSH), glitch.

## WHEEL  [BARS: REG SUB BODY TOP | TONE: PERC CLICK DRV ROTR]
- Tonewheel organ with nine sine partials. REG = 16 drawbar registrations (FLUTE, MELLO, HOLLW, SMOOT, 3BAR, BLUES, GOSPL, ROCK, TOPS, CLARI, REED, STRNG, CHAPL, BRITE, BASS, FULL), SUB/BODY/TOP = -8..+8 offsets for the 16'+5 1/3' bars, 8'+4', and 2 2/3'..1' bars. PERC = OFF/2ND/3RD/2SOFT/3SOFT/2SLOW/3SLOW (single trigger, mutes 1' bar). CLICK = key-click noise. DRV = tanh drive. ROTR = OFF/SLOW/FAST rotary.
- Maps to: "cutoff" = TOP vs SUB balance (drawbar mix is the timbre control; no filter, no resonance); osc mix = REG/SUB/BODY/TOP.
- **SHP:** brightness (upper bars toward full, sub bars toward silence, or the reverse; the 1' bar silent with PERC on).
- Suits: Hammond registrations only (FULL ORGAN, JAZZ PERC, GOSPEL, SOFT FLUTE, ROCK DRIVE); also a flute/organ pad (REG FLUTE/CHAPL with slow attack), simple additive bell/vibes-like tones using TOP. The firmware's REG list is its own list of "common registrations", not verified against a patch book here.

## VOICE (eng_formant.c)  [VOWL: VOWL VOWL2 TALK SHIFT | TONE: BUZZ BRTH Q RAND]
- Klatt-style formant synth, 4 voices. VOWL and VOWL2 = the two vowels (0-127, morphable); TALK = time taken to glide from VOWL to VOWL2 within a note; SHIFT = formant shift in semitones (-12..+12, use for female/child vs male); BUZZ = glottal open-quotient and spectral tilt (brightness of the source); BRTH = breath noise (at the top voicing fades to a whisper); Q = formant bandwidth (x2.5 .. x0.4) and output level (narrow rings louder); RAND = random vowel per note.
- Maps to: cutoff = BUZZ (tilt) together with vowel position (FLT input shifts the vowel position); resonance = Q; noise = BRTH.
- **SHP:** BUZZ (plus velocity). 
- Suits: formant/vocal lead (VOX LEAD), choir/vowel pad (CHOIR AAH, WHISPER), talking/wow bass (WOW BASS). Vowel data: Hillenbrand et al. 1995 male means (see 2.7).

## PHYS  [BODY: MODEL STRC BRIT DAMP | EXCT: POS ACC BOW EXC]
- MODEL: MODAL (12 modes: bells, bars), STRNG (extended Karplus-Strong), MEMB (drum head), SYMP (plucked string with 3 sympathetic strings). STRC = structure (partial stretch, or string dispersion/bridge curve), BRIT = brightness (this is the cutoff analogue; FLT input moves BRIT), DAMP = decay, POS = strike/pluck position, ACC = accent at full velocity, BOW = sustained excitation (bowed/blown), EXC = exciter mix. SYMP model also uses CHRD (sympathetic string tuning), SYMP amount, BUZZ; MEMB uses HARM and BEND (these replace some labels per model).
- Maps to: cutoff = BRIT, resonance = none (DAMP controls ring time), osc mix = EXC.
- **SHP:** STRC (SYMP model: BUZZ).
- Suits: marimba/kalimba/FM-bell-like modal bells (MODAL: BELL TREE, MARIMBA, KALIMBA), string pluck and harp (STRNG, SYMP: PLUCK, HARP), vibraphone (MODAL, long DAMP, low STRC), hand drums, bowed drones (BOW > 0). Note the FILTER ENV FLT maps to BRIT, so a pluck's brightness decay is available.

## NOISE  [SRC: MODE COLR FREQ RES | MOVE: TRK DENS DRFT CRSH]
- MODE: ANLG (white noise + impulses), DUST (impulses only), LFSR (shift-register noise, LEN 23..7 bits, CLK semitones above the key), META (short register as a pitched metallic tone). COLR = white->pink->brown. FREQ and RES = state-variable filter, low-pass (RES 0) to narrow band-pass (RES 127). TRK = key tracking of FREQ. DENS = impulse density (LFSR/META: register length), DRFT = slow random wander of FREQ, CRSH = sample-hold + bit reduction (LFSR/META: clock above the key).
- Maps to: cutoff = FREQ (FLT input moves FREQ), resonance = RES, tilt = COLR.
- **SHP:** COLR.
- Suits: noise sweep/riser, wind (WIND), rain, texture, metallic hits (META), arcade noise (LFSR: ARCADE), glitch arps, snare/hat-like parts as percussion layers.

## GRAIN  [GRAN: SRC POS SIZE DENS | SPRY: PTCH SPRD RAND TONE]
- **Retired in DUELL 0.2.9** (the engine, its built-in samples and the user sample slots are gone; its number stays reserved, a stored sound of it plays silent: SPEC 2.1). Kept below as a record of what its factory sounds used.
- Granular over the SAMPLE sets. SRC = source set, POS = read position (0-100% of the zone), SIZE = grain length (10-390 ms), DENS = grains per second (1-98), PTCH = pitch +-24 st, SPRD = random spread of position, RAND = random detune and share of reversed grains, TONE = low-pass (FLT input adds).
- Maps to: cutoff = TONE, resonance = none, osc mix = none.
- **SHP:** POS (LFO scans, FILTER ENV SHP sweeps).
- Suits: evolving/granular pad (CLOUD PAD, FROZEN, SHIMMER), glitch (GLITCH), texture, wind-like drones, slow-LFO drones (LFO 2 SHP on POS). Matches the sourced pad guidance: SIZE 50-200 ms (the code comment states 10-390 ms; the knob-to-ms curve was not checked, so find the position by ear), DENS a few overlapping grains, a little SPRD.

## SAMPLE  [SET: SET TUNE BITS LOOP | TONE: CUT (-) DRV (-)]
- **Retired in DUELL 0.2.9** (the engine, its built-in samples and the user sample slots are gone; its number stays reserved, a stored sound of it plays silent: SPEC 2.1). Kept below as a record of what its factory sounds used.
- IMA ADPCM sets from flash plus USR1-3. TUNE +-24 st, BITS = bit reduction, LOOP on/off, CUT = low-pass (FLT input moves it), DRV = drive.
- Maps to: cutoff = CUT, resonance = none, drive = DRV, lo-fi = BITS.
- **SHP:** DRV (the drive).
- Suits: whatever the built-in sets contain, lofi keys (BITS), looped pads; the set names were not read (the list is generated by tools/gen_samples.py), so check SET on the device.

## FM6  [PATCH: SLOT ALG FB MLVL | MACRO: MRAT MEG VMOD DTUN]   (0.2.18: SLOT first; until 0.2.17 OPS: ALG FB MLVL MRAT | PATCH: MEG VMOD DTUN SLOT)
- Six-operator FM (Dexed core). The patch is the sound; the 8 EDIT values are macros: ALG (PAT or 1..32 algorithms), FB = added feedback (-7..+7), MLVL = output level of all modulators (-36..+36 dB: the brightness / "cutoff" of FM), MRAT = added to modulators' coarse ratio, MEG = modulator envelope times (slower/faster), VMOD = modulators' velocity sensitivity, DTUN = carrier spread (about +-36 ct), SLOT = patch (F1-F8 factory, OWN, B1-B32 bank).
- Maps to: cutoff = MLVL (the FLT input moves MLVL), resonance ~ FB, velocity brightness = VMOD, oscillator mix = none (operator levels are in the patch).
- **SHP:** feedback (FB).
- The operator envelopes are the voice amplitude: the header says the track's ADSR, ENV DEST and the matrix's ENV "do nothing" here, yet in the same comment says ENV/LFO -> FLT moves MLVL. These statements conflict as written; whether FILTER ENV DEST FLT acts on FM6 (as the filter-envelope route to MLVL) is **unsure** and should be tested on the device before relying on it. AMP ENV (ATK/DEC/SUS/REL) is probably inactive for the same reason (source: eng_fm6.c header).
- Suits: DX7 electric piano (TINE EP), FM bell (BELL), FM bass / slap (FM BASS), brass (BRASS), glass/FM pad (PAD), marimba, organ, pluck (factory F1-F8 as listed in the preset table); FM lead, sci-fi bell clusters (MRAT, DTUN).

## SLICE  [SLCE: SRC DIV START PTCH | PLAY: MODE REV DCAY TONE]
- **Retired in DUELL 0.2.9** (the engine, its built-in samples and the user sample slots are gone; its number stays reserved, a stored sound of it plays silent: SPEC 2.1). Kept below as a record of what its factory sounds used.
- Sliced sample playback: SRC (BREAK USR1-3 PIANO), DIV (4 8 16 32 AUTO MAN), START (slice), PTCH, MODE (ONE GATE LOOP), REV, DCAY (decay time), TONE = low-pass (FLT input moves TONE).
- Maps to: cutoff = TONE; no resonance; no oscillators.
- **SHP:** TONE (the same control FLT moves; DCAY acts before the render, so it is not modulated by SHP).
- Suits: glitch/stutter arps (STUTTER, CHOP), rhythmic textures, reversed risers (REV + long slice), lofi chops. Not a pitched-instrument engine for the recipe list, except for PIANO as source (pluck/keys-like).

## WAVE  [TABLE: TABLE POS BEND SYNC | OSC: SUB DTN (-) (-)]  (DUELL 0.2.5, `eng_wave.c`)
- Wavetable: TABLE picks one of 26 tables built from recipes (SAWSQ PWM HARM ODD ORGAN VOWEL BRASS BOWED RESO COMB DIGI GLASS CHIME ROUGH, and since 0.2.7 BAND DUAL THIN PIPE REESE HOOVR GROWL WOBBL YELL METAL STEPS AIR: SPEC 2.6), POS scans it (adjacent frames mixed), BEND warps the read phase (a pulse width for any wave), SYNC hard sync 1..8x, SUB a sine an octave down, DTN a second read (ct).
- Maps to: no filter of its own: cutoff / resonance = the universal filter (UF CUT / UF RES); the "filter envelope" of a recipe can be the scan itself on tables whose POS adds harmonics (HARM: the harmonic count; BRASS: harmonics growing in; RESO: a resonant cutoff baked in; DIGI: the FM index), driven by FILTER ENV SHP or AMP SHP; osc mix = SUB, DTN.
- **SHP:** POS (63 = the whole scan). The FLT depths and CUT routes drive the universal filter, as on the other universal-filter engines.
- Suits: PWM pads (PWM), vowel / choir pads (VOWEL), glass and FM pads (GLASS), drawbar organ (ORGAN, SUB = the 16'), chimes and bells (CHIME), DX-style plucks (DIGI), hard-sync leads (SAWSQ + SYNC), basses (HARM + SUB), synth brass (BRASS), slow evolving pads and drones (COMB / VOWEL / RESO with slow LFOs on POS). Since 0.2.7: PPG-style sweep pads (BAND, DUAL with FILTER ENV on POS), a church organ (PIPE), Reese and growl basses (REESE + DTN, GROWL or WOBBL with a synced LFO on POS), the hoover (HOOVR with a fast LFO on POS), screaming leads (YELL), metallic bells (METAL with the amp envelope on POS), lo-fi and stepped tones (STEPS), breathy flutes and pads (AIR with aftertouch on POS).
- Factory sounds and their recipes: PWM PAD 3.3, VOWEL PAD 3.5, GLASS PAD 3.6, EVOLVING PAD 3.4 / 5.6, DRAWBARS 4.5, WAVE CHIMES 4.3, DIGI PLUCK 4.1 / 4.6, SYNC LEAD 2.2, HARM BASS 1.1, SYNTH BRASS 2.3 / 4.8, RESO SWEEP 5.6; 0.2.7: GROWL BASS 1.7, REESE BASS 1.5, HOOVER 2.8, DIGITAL PAD 3.8, BREATH PAD 3.9, PLATE BELL 4.3 / 4.4.

## Engines by recipe category (summary)
- **BASS:** ANALOG (Moog, 303, sub, Reese, pluck), TRIO (fat chip bass), LOFI (wave bass), FM6 (FM bass, fretless), PHASE (resonant bass), VOICE (wow bass), WHEEL (organ bass), PHYS (fingered electric bass), WAVE (harmonic-count, growl, Reese).
- **LEAD:** ANALOG (Moog mono, sync, brass, supersaw approx), TRIO (sync, chip, oboe), LOFI (chip), PHASE (brass), FM6 (FM lead), VOICE (formant lead), WAVE (hard sync, hoover, flute, clarinet).
- **PAD:** ANALOG (Juno, PWM, Prophet-like, drone), PHASE (string), VOICE (choir, whisper), FM6 (glass), NOISE (texture pad), TRIO (string machine), WAVE (PWM, vowel, glass, evolving, wavetable sweep, breath, brass). (GRAIN and SAMPLE: retired in 0.2.9.)
- **PLUCK/KEYS:** FM6 (EP, bell, marimba, steel drum), PHYS (marimba, kalimba, vibes, harp, pluck, harpsichord, toy piano), WHEEL (organ), ANALOG (clav-like, pluck, brass stab), PHASE (bell/brass, koto), WAVE (drawbars, chimes, DX pluck, brass, plate bell). (SAMPLE / SLICE's piano: retired in 0.2.9.)
- **FX/OTHER:** NOISE (riser, wind, rain, metal, reverse cymbal), TRIO (ring/metallic arp, dial tone), LOFI (bitcrushed arp, drone), PHASE (sonar ping), ANALOG (drone/wind), WAVE (resonant sweep, frozen texture). (GRAIN, SLICE, SAMPLE: retired in 0.2.9.)
- **PERC (0.2.9):** PHASE (analogue kick), PHYS (tom: MEMB), TRIO (cowbell), NOISE (hats, snare).

## Where the sources and the code disagree or are unsure
- FM6: contradictory header statements about ENV/LFO routing (above).
- PHASE ENV parameter: which envelope it uses was not confirmed.
- SPEC.md line 99 still says 1 envelope / 1 LFO / 4-slot matrix (Felucca baseline) while later lines say 8 slots and FILTER ENV/LFO 2; I used the later DUELL lines.
- Reese detune: +-25 ct vs +-55 ct (different tutorials).
- Vowel formants: Cantor Digitalis table vs Hillenbrand (firmware).
- Many numeric ranges in the recipes are [G] (general craft), because the SOS Synth Secrets pages fetched (brass, PWM strings, bells, plucked strings, Hammond effects) are mostly principle-based and give few or no numbers; MusicRadar tutorials (Minimoog bass, 303) and Perfect Circuit sync page could not be read (truncated, HTTP 403).
