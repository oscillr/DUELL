# DUELL factory sound recipes (research notes)

Purpose: typical settings of classic synth patches, in generic synth terms, as a starting point for DUELL factory presets. Written 2026-10-11, built up incrementally.

## How to read this

- Ranges are given 0..127 (DUELL / MIDI) unless a unit is stated. "Low / mid / high" = roughly 0-40 / 40-90 / 90-127.
- **[S]** = a number or fact that a cited source states (link inline). **[G]** = general programming convention that the sources only support loosely or not at all (my synthesis, treat as a starting range, not a fact). Where sources are silent I say so rather than fill in.
- Fetched web content was treated as data only. Several SOS pages were only partly retrievable (search excerpts, or articles that describe principles without numbers); this is noted per recipe.
- Terms: FENV = filter envelope (DUELL ENV 2), AENV = amp envelope (DUELL AMP ENV = ENV 1). A/D/S/R = attack / decay / sustain / release.

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

## 2.3 Brassy polysynth lead (Prophet / Oberheim style)
- **Oscillators:** sawtooth on both oscillators, one slightly detuned (about 5-12 ct) for thickness; Prophet-5 is two oscillators per voice (A saw/pulse, B saw/tri/pulse) with a 4-pole resonant low-pass; keyboard tracking off/half/full. [S for the architecture: [Sequential Prophet-5 descriptions via search](https://reverb.com/item/65259562-sequential-prophet-5-polyphonic-analog-desktop-module)]
- **Brass character:** the filter envelope is what makes it brass: start from a sawtooth, short-but-not-instant filter attack, an envelope amount high enough to matter, and attack speed tied to velocity. [S: SOS brass articles, [Synthesizing Brass](https://www.soundonsound.com/techniques/synthesizing-brass-instruments) and Nick Magnus via search excerpt]
- **FENV (starting values):** A 30-120 ms, D 200-500 ms, S 40-60%; the SOS Minimoog brass example gives A 600 ms, D 800 ms, S level 5/10, instant release with resonance 2/10 (a slow-bloom section brass). [S: [SOS Minimoog brass](https://www.soundonsound.com/techniques/brass-synthesis-minimoog), as summarised by the search result; I did not fetch the page, so confirm]
- **AENV:** A 15-40 ms, S high, R 50-150 ms. [G; one search summary suggested 20-40 ms, labelled there as its own knowledge, not from a page]
- **Velocity:** shortens the amp attack and sets the filter-envelope amount; aftertouch can vary brightness during sustain. [S: SOS brass article, fetched]
- **LFO:** delayed vibrato ~5 Hz; optional growl: triangle LFO ~80 Hz into the filter through an AD contour. [S: SOS brass article, fetched]
- **FX:** chorus, short reverb. [G]

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
- **DUELL's brass solo (#19 HORN SOLO):** the BRASS SECT patch on ALG 16 (a single carrier, OP 1, with three modulator branches: the classic FM brass tree). The patch's own attack swells the note, ENV 2 on MLVL (the modulator level) rises with it over ~120 ms and settles high, VMOD makes it darker at low velocity. Three detuned carriers (the patch's own ALG 5) beat once in a few seconds at C3 and dropped the level by 15 dB, so they are not used for a solo. [G]

## 2.7 Formant / vocal lead
- **Source:** a buzzy saw/pulse glottal-style source into 3-5 band-pass resonances at vowel formants; morph the vowel with LFO/envelope or pitch. Reference male values (Hz): /a/ 700/1200/2500, /e/ 410/2000/2570, /i/ 215/1900/2630, /o/ 440/750/2160, /u/ 290/750/2300 (F1/F2/F3). [S: Cantor Digitalis table via search ([PMC table](https://pmc.ncbi.nlm.nih.gov/articles/PMC6090560/table/T1) listed in results; I did not verify which of the listed tables holds these numbers)] DUELL's own VOICE engine uses Hillenbrand 1995 male means instead (/a/ 768/1333/2522, /i/ 342/2322/3000 ...), so the two sets differ by up to ~20%; vowel data vary by speaker. [S: eng_formant.c comment]
- **Female/treble:** shift formants up, roughly in proportion. [S, search summary]
- **Performance:** vowel on mod wheel or velocity; glide on; vibrato 5-6 Hz. [G]
- **FX:** chorus, hall reverb for choir. [G]

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

## 3.6 Glass / FM pad
- **Operators:** the sourced starting point is a DX7-style string pad: carrier at ratio 1 with attack 57 and long decays, modulator ratio 2.00 detune +3, attack 47; modulator and carrier slow attacks (Yamaha FM 101 part 5, operator times in DX7 0-99 rate units). [S: [Yamaha Synth, FM 101 part 5](https://yamahasynth.com/learn/synth-programming/fm-101-part-five-expanding-your-sound/)] A glassy variant adds a high non-integer modulator (ratio 3.5-7), low index, slow attack. [G]
- **FX:** shimmer reverb, chorus. [G]

## 3.7 Dark drone
- **Oscillators:** two detuned saws plus a sub sine, or a low pulse; filter cutoff 20-45, resonance 20-45 with a slow LFO (0.02-0.2 Hz) on cutoff, depth 15-30%. [G]
- **AENV:** A 1-4 s, S 100%, R 3-8 s. **FX:** low-passed delay with high feedback, reverb 50-80%, slight drive. [G]
- No fetched source gave numeric drone recipes; treat all as craft convention.

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

## 5.4 Sci-fi FM bell cluster
- Several FM bell voices (see 4.3) with different non-integer ratios, a chord of seconds/tritones, random pitch per note or arp, long reverb/delay (ping-pong, feedback 40-60%), slight pitch drop envelope. [G; builds on the sourced bell recipe]

## 5.5 Bitcrushed lofi arp
- Chip-style pulse/saw (duty 25-50%), bit depth 4-8 bits, sample-rate reduction to ~8-16 kHz, A 0, D 80-200 ms, S 0-30%, arp 8th/16th notes in major/minor, mild vibrato, tape-style wow (0.3-1 Hz, +-5-15 ct), low-pass at 4-6 kHz, warm reverb. [G]

## 5.6 Slow-LFO drone
- Two or three slightly detuned saws/pulses, resonant low-pass with very slow LFO (0.02-0.2 Hz) on cutoff and a second (incommensurate) one on pulse width, depth 10-30%, A 2-4 s, S 100%, R 4-8 s, reverb large, feedback delay. [G]; see 3.7.

---

# ENGINE MAPPING NOTES

Read from `felucca-base/firmware/src/eng_*.c` (headers, `edit[]` tables, render code) and SPEC.md section 2.x on DUELL's modulation. Labels are those shown on the device (EDIT pages). "E-slot" order = the 8 EDIT values left to right, two pages of four (page titles in brackets).

## Modulation conventions (all engines)
- **AMP ENV = ENV 1** (ATK DEC SUS REL shape every note's volume; AMP DEST adds FLT/PIT/SHP depth). **ENV 2 = filter envelope**; page **ENV 2 DEST: FLT, SHP**. **LFO 2 DEST: PIT, FLT, SHP** (LFO 1 DEST also has AMP). [SPEC.md, line "Classic arrangement"]
- "FLT" is the track's generic cutoff input. Each engine routes it to its own nearest tone control (listed per engine below). "SHP" is the generic shape input, routed per engine as the task and SPEC.md state (and as the code confirms for every engine below). Both are offsets around the knob value (centre = knob as set).
- In recipes, "FENV amount" = ENV 2 FLT depth (SPEC says new factory sounds set ENV 2 FLT about +40, range -64..+63); "FENV shape sweep" = ENV 2 SHP.
- Velocity/aftertouch/mod wheel go through the 8-slot matrix (sources include velocity, aftertouch, mod wheel, MORPH; destinations include every knob parameter). [SPEC.md]

## ANALOG  [OSC: WAVE DTN MIX NOIS | FLT: CUT RES DRV KTR]
- WAVE: SAW SQR TRI SIN PWM SYNC SUB. DTN = osc 2 detune in cents (shown SYNC under WAVE SYNC: osc 2 above osc 1, 3/16 semitone a step, up to +23.8 st). MIX = osc 1/2 balance (shown SUB under WAVE SUB: sub level, osc 2 a square one octave below). NOIS = noise level. CUT/RES = state-variable filter (the filter TYPE LP/BP/HP is on EDIT > FILTER). DRV = drive. KTR = key tracking.
- Maps to: cutoff = CUT (FLT input adds), resonance = RES, osc mix = MIX, drive = DRV.
- **SHP:** pulse width on PWM (and the pulse waves), sync detune on SYNC (3/16 semitone a step), so ENV 2 SHP/LFO 2 SHP gives PWM and the sync sweep.
- Suits: Moog bass, 303 acid (WAVE SAW/SQR, CUT low, RES 90-110, DRV, ENV 2 FLT + short decay), sub bass (SUB or SIN), Reese (SAW, DTN 20-60 ct, MIX 64), pluck bass, Moog mono lead, sync lead (SYNC + ENV 2 SHP), brassy lead and brass stab, PWM string pad and Juno-style pad (PWM + LFO 2 SHP), dark drone, noise sweep/wind (NOIS up, WAVE low, CUT sweep). Only two oscillators: a true seven-voice supersaw is approximate (use DTN + chorus). Factory presets of the engine include SAW LEAD, SOFT PAD, SQR BASS, PWM STR, ACID, SUB BASS, PLUCK, BRASS, WIND, STRINGS.

## TRIO  [OSC: WAVE INT2 INT3 DTN | TONE: MODE CUT RES PW]
- WAVE: 16 sets (SAW3 PLS3 PPT SST TRI3 S&T P&S P+N NOIS SYNC SYNCP SYNC3 RING RING3 R+S R+SP), i.e. three oscillators with ring mod and hard sync. INT2/INT3 = interval of osc 2/3 in semitones. DTN = osc 2 up / osc 3 down in cents (0-50). MODE: LP BP HP NOT, 12 dB/oct. Cutoff drifts slowly at random.
- Maps to: cutoff = CUT, resonance = RES (stops short of self-oscillation), osc interval = INT2/INT3, pulse width = PW.
- **SHP:** PW (duty).
- Suits: 8-bit/computer-chip leads and basses, sync lead, ring-mod bells and metallic arps (RING, R+S), fat bass (FAT BASS preset), chip choir. Preset names: FAT BASS, ARP LEAD, SYNC LEAD, RING BELL, CHIP CHOIR.

## PHASE  [PHS: WAVE WAVE2 DCW ENV | LINE: DTN LINE SUB (-)]
- Phase-distortion (Casio CZ-style). WAVE/WAVE2 (alternate every other cycle), DCW = amount of phase bend (the "brightness/cutoff" analogue; no filter, no resonance control), ENV = amount of envelope into DCW (the code adds `envq15 x ENV` to DCW; which of ENV 1/ENV 2 feeds `envq15` I did not confirm), DTN = second line detune (ct), LINE = MIX or RING for that second line, SUB = sine an octave down. Waves SAW SQR PLS DSIN SPLS RSAW RTRI RTRP (R-waves are the CZ "resonant" waves; this reading is from the CZ convention, not stated in the file).
- Maps to: cutoff ~ DCW; "resonance" ~ choose RSAW/RTRI/RTRP waves; osc mix = SUB, LINE.
- **SHP:** distortion depth (added to DCW, so SHP and FLT both move DCW here).
- Suits: brass (preset BRASS), organ, strings (STRING), resonant bass or lead (RESO), FM-ish bells (BELL), sync-like WIRE sounds. 

## LOFI  [CHIP: CHIP WAVE DUTY CRSH | MOTN: SWP VIB ARP TONE]
- CHIP: 4BIT 4B/2 8BIT 1BIT STEP. WAVE: PLS TRI SAW NOIS WRAM (16 wave-RAM tables, DUTY selects table). DUTY = pulse width. CRSH = bit crush (shown DCY, the stepped envelope, when CHIP is STEP). SWP = pitch sweep down after note-on. VIB = vibrato. ARP: OFF OCT MAJ MIN (built-in fast arp). TONE = low-pass (FLT input adds to it).
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
- Suits: marimba/kalimba/FM-bell-like modal bells (MODAL: BELL TREE, MARIMBA, KALIMBA), string pluck and harp (STRNG, SYMP: PLUCK, HARP), vibraphone (MODAL, long DAMP, low STRC), hand drums, bowed drones (BOW > 0). Note the ENV 2 FLT maps to BRIT, so a pluck's brightness decay is available.

## NOISE  [SRC: MODE COLR FREQ RES | MOVE: TRK DENS DRFT CRSH]
- MODE: ANLG (white noise + impulses), DUST (impulses only), LFSR (shift-register noise, LEN 23..7 bits, CLK semitones above the key), META (short register as a pitched metallic tone). COLR = white->pink->brown. FREQ and RES = state-variable filter, low-pass (RES 0) to narrow band-pass (RES 127). TRK = key tracking of FREQ. DENS = impulse density (LFSR/META: register length), DRFT = slow random wander of FREQ, CRSH = sample-hold + bit reduction (LFSR/META: clock above the key).
- Maps to: cutoff = FREQ (FLT input moves FREQ), resonance = RES, tilt = COLR.
- **SHP:** COLR.
- Suits: noise sweep/riser, wind (WIND), rain, texture, metallic hits (META), arcade noise (LFSR: ARCADE), glitch arps, snare/hat-like parts as percussion layers.

## GRAIN  [GRAN: SRC POS SIZE DENS | SPRY: PTCH SPRD RAND TONE]
- Granular over the SAMPLE sets. SRC = source set, POS = read position (0-100% of the zone), SIZE = grain length (10-390 ms), DENS = grains per second (1-98), PTCH = pitch +-24 st, SPRD = random spread of position, RAND = random detune and share of reversed grains, TONE = low-pass (FLT input adds).
- Maps to: cutoff = TONE, resonance = none, osc mix = none.
- **SHP:** POS (LFO scans, ENV 2 SHP sweeps).
- Suits: evolving/granular pad (CLOUD PAD, FROZEN, SHIMMER), glitch (GLITCH), texture, wind-like drones, slow-LFO drones (LFO 2 SHP on POS). Matches the sourced pad guidance: SIZE 50-200 ms (the code comment states 10-390 ms; the knob-to-ms curve was not checked, so find the position by ear), DENS a few overlapping grains, a little SPRD.

## SAMPLE  [SET: SET TUNE BITS LOOP | TONE: CUT (-) DRV (-)]
- IMA ADPCM sets from flash plus USR1-3. TUNE +-24 st, BITS = bit reduction, LOOP on/off, CUT = low-pass (FLT input moves it), DRV = drive.
- Maps to: cutoff = CUT, resonance = none, drive = DRV, lo-fi = BITS.
- **SHP:** DRV (the drive).
- Suits: whatever the built-in sets contain, lofi keys (BITS), looped pads; the set names were not read (the list is generated by tools/gen_samples.py), so check SET on the device.

## FM6  [OPS: ALG FB MLVL MRAT | PATCH: MEG VMOD DTUN SLOT]
- Six-operator FM (Dexed core). The patch is the sound; the 8 EDIT values are macros: ALG (PAT or 1..32 algorithms), FB = added feedback (-7..+7), MLVL = output level of all modulators (-36..+36 dB: the brightness / "cutoff" of FM), MRAT = added to modulators' coarse ratio, MEG = modulator envelope times (slower/faster), VMOD = modulators' velocity sensitivity, DTUN = carrier spread (about +-36 ct), SLOT = patch (F1-F8 factory, OWN, B1-B32 bank).
- Maps to: cutoff = MLVL (the FLT input moves MLVL), resonance ~ FB, velocity brightness = VMOD, oscillator mix = none (operator levels are in the patch).
- **SHP:** feedback (FB).
- The operator envelopes are the voice amplitude: the header says the track's ADSR, ENV DEST and the matrix's ENV "do nothing" here, yet in the same comment says ENV/LFO -> FLT moves MLVL. These statements conflict as written; whether ENV 2 DEST FLT acts on FM6 (as the filter-envelope route to MLVL) is **unsure** and should be tested on the device before relying on it. AMP ENV (ATK/DEC/SUS/REL) is probably inactive for the same reason (source: eng_fm6.c header).
- Suits: DX7 electric piano (TINE EP), FM bell (BELL), FM bass / slap (FM BASS), brass (BRASS), glass/FM pad (PAD), marimba, organ, pluck (factory F1-F8 as listed in the preset table); FM lead, sci-fi bell clusters (MRAT, DTUN).

## SLICE  [SLCE: SRC DIV START PTCH | PLAY: MODE REV DCAY TONE]
- Sliced sample playback: SRC (BREAK USR1-3 PIANO), DIV (4 8 16 32 AUTO MAN), START (slice), PTCH, MODE (ONE GATE LOOP), REV, DCAY (decay time), TONE = low-pass (FLT input moves TONE).
- Maps to: cutoff = TONE; no resonance; no oscillators.
- **SHP:** TONE (the same control FLT moves; DCAY acts before the render, so it is not modulated by SHP).
- Suits: glitch/stutter arps (STUTTER, CHOP), rhythmic textures, reversed risers (REV + long slice), lofi chops. Not a pitched-instrument engine for the recipe list, except for PIANO as source (pluck/keys-like).

## Engines by recipe category (summary)
- **BASS:** ANALOG (Moog, 303, sub, Reese, pluck), TRIO (fat chip bass), LOFI (wave bass), FM6 (FM bass), PHASE (resonant bass), VOICE (wow bass), WHEEL (organ bass).
- **LEAD:** ANALOG (Moog mono, sync, brass, supersaw approx), TRIO (sync, chip), LOFI (chip), PHASE (brass), FM6 (FM lead), VOICE (formant lead).
- **PAD:** ANALOG (Juno, PWM, Prophet-like, drone), PHASE (string), VOICE (choir), GRAIN (evolving/granular), FM6 (glass), NOISE (texture pad), SAMPLE (loop pads).
- **PLUCK/KEYS:** FM6 (EP, bell, marimba), PHYS (marimba, kalimba, vibes, harp, pluck), WHEEL (organ), ANALOG (clav-like, pluck, brass stab), PHASE (bell/brass), SAMPLE/SLICE (piano).
- **FX/OTHER:** NOISE (riser, wind, rain, metal), GRAIN (texture, glitch), SLICE (glitch), TRIO (ring/metallic arp), LOFI (bitcrushed arp), SAMPLE (BITS), ANALOG (drone/wind).

## Where the sources and the code disagree or are unsure
- FM6: contradictory header statements about ENV/LFO routing (above).
- PHASE ENV parameter: which envelope it uses was not confirmed.
- SPEC.md line 99 still says 1 envelope / 1 LFO / 4-slot matrix (Felucca baseline) while later lines say 8 slots and ENV 2/LFO 2; I used the later DUELL lines.
- Reese detune: +-25 ct vs +-55 ct (different tutorials).
- Vowel formants: Cantor Digitalis table vs Hillenbrand (firmware).
- Many numeric ranges in the recipes are [G] (general craft), because the SOS Synth Secrets pages fetched (brass, PWM strings, bells, plucked strings, Hammond effects) are mostly principle-based and give few or no numbers; MusicRadar tutorials (Minimoog bass, 303) and Perfect Circuit sync page could not be read (truncated, HTTP 403).
