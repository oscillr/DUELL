# Changelog

**DUELL**
MultiMorphing Bitimbral Synthesizer
by OSCILL/R

Each release: a one-line summary, then New, Fixed, Compatibility, Install, Files.

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
