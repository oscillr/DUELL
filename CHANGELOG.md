# Changelog

**DUELL**
MultiMorphing Bitimbral Synthesizer
by OSCILL/R

Each release: a one-line summary, then New, Fixed, Compatibility, Install, Files.

## DUELL 0.2.4 beta (first firmware beta)

**The first firmware release: the device package `duell-0.2.4-beta.fwsc`, the installer page and the browser emulator `duell-emulator-0.2.4-beta.zip`. It covers everything since 0.1 beta (the earlier builds 0.2.1 to 0.2.3 were never published). Tried on one FM-1, the author's; install at your own risk; no warranty; not affiliated with M-VAVE.**

### New (since 0.1 beta)
- **Chords and one global scale.** SCALE and ROOT are now device settings (default CHROMATIC, root C), not part of a patch, part, multi or factory sound. SCL button, page 1 SCALE (K1 SCALE, K2 ROOT, K3 OFF-SCALE) and page 2 CHORD (K1 CHRD, K2 VOIC, K3 TRN, the selected part's). OFF-SCALE KEYS: SNAP (default) moves a key or MIDI note outside the scale to the nearest scale note (a tie goes down), PLAY lets it pass. Both parts sound the same note for a key. DIA3 / DIA7 chords stack the scale's degrees; with the scale CHROMATIC they play a plain MAJ triad / MAJ7 and the card says so. The root of a chord is never re-pitched.
- **Universal filter** for the engines that had none of their own (PHASE, LOFI, VOICE, WHEEL, GRAIN, PHYS, FM6, SLICE), on a new EDIT > FILTER page: K1 CUT is one bipolar knob (centre OFF, left a low-pass, right a high-pass), K2 RES. Envelopes, LFOs, the modulation matrix and MIDI CC74 (CUT, 64 = OFF) and CC71 (RES) drive it; while CUT is OFF the sound is unchanged. ANALOG, TRIO, NOISE and SAMPLE keep their own filters.
- **SAVE flow.** A short press of SAVE opens the page that fits: PATTERNS from a sequencer page (STEP, GRID, AUTOMATION), otherwise MULTIS in LAYER and PATCHES in SPLIT and STEREO. **Hold SAVE saves over** the patch you are editing (BOTH patches on the MORPH page), and now also over the pattern or the multi picked on the PATTERNS and MULTIS lists ("SAVE OVER <name>?", OCT+ yes; an empty multi asks for a name first). Page buttons leave and cancel any dialog without writing. The PATCHES list shows at most three empty places at its end; FREE nn still shows the true number.
- **ERASE DATA** (MENU > SYSTEM > ERASE DATA, OCT+ asks, OCT+ yes) erases settings, favourites, autosave, your patches, multis and pattern slots (not your user samples) and restarts the unit as new (VIOLET, LAYER, M01). The installer has an optional tick box "Erase my saved data after installing" (set by what the unit runs, see Changed). A plain reinstall still keeps your data.
- **GLO page.** GLO is now GLO, MIDI, SIDECHAIN, MIDI LEARN, TEMPO. The keys play notes there. K1 MUTE 1 and K2 MUTE 2 act on one click (before: three clicks), K3 LEVEL 1, K4 LEVEL 2, with a meter per part; new TEMPO page (tempo, swing, tap) and MIDI page (channels, MIDI IN, clock source). MORPH's ROLL also acts on one click (A left, B right). Every part starts unmuted at power-on.
- **VIOLET theme** (the default) is now the darker, near-black, more saturated violet and pink.
- **Menu.** Page buttons (EDIT, FX, ENV, LFO, ARP, SEQ, SCL, GLO, SAVE) now work inside the MENU and close it. Menu settings are saved when they change (about 2 s after the knob rests). ABOUT shows the version, build date, USB link and CPU line.
- **Header and footer redone:** the page name sits in the top middle of every screen; the footer shows the engine, the full patch name and the page number.
- **Factory sounds and multis.** Four new factory multis M01 EERIE SIGNAL, M02 WIRED SNAIL, M03 DEAD BUFFER, M04 ACID ETHER. Several factory sounds were revised after audition on the device: #19 HORN SOLO, #22 SILVER FLUTE (no click at the loop wrap), #65 MOTOR VIBES, #66 ROSEWOOD, #67 OPEN SIZZLE, #68 STATIC SNARE, #72 WOOFER, #97 renamed SNARE BUZZ. LFO TRIG now defaults to FREE. Because of the new filter, 46 factory sounds on the filterless engines no longer move their tone controls from the cutoff routes (to be checked by ear).
- **Installer page.** BACK UP, INSTALL DUELL and RETURN TO OFFICIAL FIRMWARE are three separate cards with their own progress, a banner names the running action, the other buttons are locked meanwhile and closing the tab asks first.
- **One version source** (the `VERSION` file): the device (splash, ABOUT), the installer and the emulator show "DUELL 0.2.4 beta".

### Changed or removed
- Installer: the erase box is chosen by what the unit runs: ticked for a clean install unless DUELL is detected.
- The FX hold layer's performance effects are gone (REPEAT, REVERSE, TAPE STOP, FREEZE, OCT UP / DN, master FLANGER and PHASER, CRUSH, THROW, the master FILTER macro, black-key track mutes, MENU > CONTROL > FX LATCH). A short press of FX still opens the FX pages, holding it is the same, and keys pressed with FX held are notes. The GLO mutes, the metronome, the per-part INSERT and the master chain are as before.
- FLAT is the only look: MENU > DISPLAY > STYLE, LARGE and MENU > SYSTEM > HELP are removed (saved settings still load).
- The CPU test screen and the TEST multis M13..M16 are not in the release build (they remain a developer switch, DUELL_TEST_MODE). INFO is gone from the menu.
- SCALE LINK, the per-part SCALE / ROOT, QNT (OFF / SNAP / WHITE / SEQ) and SOLO are replaced by the global scale, OFF-SCALE KEYS and MUTE.

### Fixed
- Load-time CPU spikes: loading a patch or a multi, or changing the mode or the reverb, no longer misses audio blocks in normal use (very heavy multis can still click, see KNOWN_ISSUES). PHASE is much cheaper (PHASE + ANALOG measured on the unit: 63 % average, 71 % peak, 0 missed).
- LFO routes to PAN, DIST, sends and engine macros did nothing audible with an arp; an arp is now one LFO phrase.
- The arp now follows an external clock (USB / TRS) that runs without a Start; a Stop makes it wait again. The TEMPO and MIDI pages show NO CLK when the external clock sends nothing for 1 s.
- SPLIT checked for leakage over every send, reverb, insert, EQ, ducker and DIST (the other channel is digital silence); stereo-only controls that SPLIT does not use are shown faded with the reason SPLIT.
- A restored autosave never brings back a muted part.

### Compatibility
- As 0.1 beta: back up first; patches, MULTIs and PATTERNs saved with a beta may not load in other betas. Old patches read CUT OFF, RES 0.
- The device backup does not include MULTIs or PATTERN slots; restore is untested (see KNOWN_ISSUES).

### Install
Use the [installer page](https://oscillr.github.io/DUELL/install/) in Chrome or Edge; steps in [docs/INSTALL.md](docs/INSTALL.md); the way back to the official firmware (V15) and recovery notes in [docs/RECOVERY.md](docs/RECOVERY.md).

### Files
`duell-0.2.4-beta.fwsc` (device package), `duell-emulator-0.2.4-beta.zip` (browser emulator) and `SHA256SUMS`, attached to the release `v0.2.4-beta`. The M-VAVE firmware is not distributed here.

### Known issues
See [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md).

## DUELL 0.1 beta (emulator release; firmware to follow)

**First public beta: Felucca turned into a two-part, morphing synth for the FM-1. The browser emulator is published first. The firmware has been run on one real FM-1 by the author; the firmware package follows after more hardware testing (including the return to the official firmware, which is untested).**

### New (compared to Felucca)
- Two parts, with SPLIT, STEREO and LAYER modes (BALANCE, velocity XFADE, key SPLIT).
- MORPH between End A and End B, with smart random (ROLL A or B, UNDO, one AMOUNT), saved with the patch.
- PATCHES (one list, factory and yours; up to 48 stored patches, FREE nn on the card), 16 MULTIs, 16 named PATTERNs.
- ENV 2 and LFO 2, 8-slot modulation matrix.
- Effects: per-part EQ, per-part ducker with MIDI/pulse/part sidechain, ping-pong delay, SHIM and RESO reverbs (from JIANT).
- MIDI: a channel per part, clock in, program change and bank select, a CC map, mod wheel to morph.
- All MIDI settings on GLO's pages (MIDI with P1 CH, P2 CH, MIDI IN OWN CH / SELECTED and the clock source CLK; SIDECHAIN; MIDI LEARN with CLEAR), none in MENU. GLO's pages in order: GLO, MIDI, SIDECHAIN, MIDI LEARN, TEMPO (K1 TEMPO, K2 SWG, K3 TAP: a detent either way is one tap), TEST.
- GLO's main page no longer takes over the keys: they play notes. K1 MUTE and K2 SOLO turn left for part 1 and right for part 2 (one action per twist, the MORPH page's gesture), K3 LEVEL 1, K4 LEVEL 2, with a dB meter per part between the columns. MENU re-audited: OVERVIEW (SCOPE / PARTS) and HOME OPENS (MORPH, MODE, MIXER, OVERVIEW), INFO and ABOUT also open with KNOB 1. A new unit starts in VIOLET, LAYER, with factory multi M01 loaded.
- A sound palette sorted by category, nine colour themes, a navigation scheme where SELECT selects and knobs adjust.
- A CPU load meter (MENU > SYSTEM > INFO: 1 s average, peak hold, missed blocks), a power-on splash (DUELL title block over the morphing wave lines, any key skips), an ABOUT page with the title block, version and Felucca credit. VIOLET is the default theme.
- GLO > TEST, the CPU test page (the meter moved here from INFO; shows the multi, each part's engine and patch; K1 MORPH, K2 ROLL, K3 LOAD MULTI, K4 RESET) and four factory TEST multis M13..M16 (TEST 1 BASE, TEST 2 PHASE, TEST 3 HEAVY, TEST 4 MIX) for measuring the load on real hardware (docs/TESTING.md). ABOUT now opens on the splash animation (SELECT scrolls to the information).
- Installer page and docs (this release). The installer says "firmware not yet released" and its flow is disabled until the package `duell-0.1-beta.fwsc` is published; the device shows "DUELL 0.1 beta".

### Fixed
- Nothing to report: this is the first release.

### Compatibility
- Patches, MULTIs and PATTERNs saved with this beta may not load in older or later betas. Back up first.
- Felucca PROJECT data and Felucca's user preset banks are not migrated. Patches saved here may read as damaged on older firmware.
- The device backup does not include MULTIs or PATTERN slots.

### Install
There is nothing to install yet: use the [browser emulator](https://oscillr.github.io/DUELL/emu/). It stores its data in the browser: press "Download saved data" to back it up, and clear the site data after an update if saved data misbehaves. The firmware install steps are in [docs/INSTALL.md](docs/INSTALL.md) for later; recovery notes in [docs/RECOVERY.md](docs/RECOVERY.md). **No warranty; not affiliated with M-VAVE.**

### Files
This release carries the emulator only: `duell-emulator-0.1-beta.zip` (the hosted emulator is built from it). **No firmware file is attached yet**; it will appear on the [releases page](https://github.com/oscillr/DUELL/releases) after more hardware testing. To build the emulator yourself: `felucca-base/web/emu/build.sh` (output `felucca-base/build/emu`).

### Known issues
See [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md).
