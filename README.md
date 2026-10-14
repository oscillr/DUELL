# DUELL

**MultiMorphing Bitimbral Synthesizer**<br>
**by OSCILL/R**

> **Public beta: the firmware beta for the FM-1 (DUELL 0.2.13 beta) and the browser emulator are available now.**
> Try the emulator: **<https://oscillr.github.io/DUELL/emu/>**. Firmware package and installer: see the [releases page](https://github.com/oscillr/DUELL/releases) and [docs/INSTALL.md](docs/INSTALL.md) (source: <https://github.com/oscillr/DUELL>).

**Two synths in one small box.** DUELL is free, open-source firmware for the M-VAVE FM-1. It turns the FM-1 into a bitimbral instrument: two complete parts, three ways to play them together, and a morph that glides between two versions of every sound.

> **Status: DUELL 0.2.13 beta.** DUELL is developed and tested in a browser emulator. The firmware has been flashed to and run on **one real FM-1, the author's, and it works**. The way back to the official firmware (V15) has worked on that unit about three times; it has not been tried on others. Broader hardware testing is pending. Flash at your own risk. Read [Status and safety](#status-and-safety) and [docs/RECOVERY.md](docs/RECOVERY.md).

<p align="center"><picture><source media="(prefers-color-scheme: dark)" srcset="docs/logo/duell-logo-white.png"><img src="docs/logo/duell-logo-black.png" alt="DUELL" width="420"></picture></p>

## Highlights

### Two parts
Part 1 and Part 2 each have their own sound, effects, EQ, ducker and arp.

### Three modes
| Mode | What you get |
|---|---|
| **DUEL** (default) | Dual mono. Part 1 hard left, part 2 hard right, perfectly isolated, effects included. |
| **STEREO** | Both parts in one stereo field, for headphones. |
| **LAYER** | Both parts on the keys at once, with balance, velocity crossfade and key split. The heart of DUELL. |

### MORPH
Glide between two versions of the same patch with one fader.
- **Smart random:** a randomizer that creates new variants from your starting point.

### Saving
- Up to 48 stored patches (copies of factory sounds count), always guaranteed to fit.
- 16 **MULTI** slots: both parts, effects, arps, patterns, mode and morph settings in one slot.
- 16 named, overwritable **PATTERN** slots.
- **Hold SAVE saves over the patch you are editing** (SAVE OVER <name>?, OCT+ yes). A short press of SAVE opens the page that fits: PATTERNS from the sequencer, otherwise MULTIS in LAYER and PATCHES in DUEL and STEREO.

### Modulation
2 envelopes, 2 LFOs and an 8-slot mod matrix.

### Filter
Every engine has a filter.

### Effects
New in DUELL: a ducker with sidechain from external MIDI, EQ per part, a ping-pong delay and the reverbs SHIM and RESO (from JIANT). Still there from Felucca: distortion, insert, chorus, delay and the hall, room and spring reverbs.

### MIDI
A MIDI channel for each part. Clock in, program change and bank select, and CCs for morph, roll, undo, amount and mode. The mod wheel can move the morph and is a mod matrix source. Not yet tested with a range of MIDI devices.

### Sounds
A new factory palette sorted by category (BASS, LEAD, PAD, PLUCK, KEYS, FX, PERC, OTHER) across the engines (ANALOG, PHASE, LOFI, VOICE, TRIO, WHEEL, PHYS, NOISE, FM6, WAVE). Each sound has a stable morph End B and an aftertouch route. Arps and chord keys are set where they fit.

### Revised UI
SELECT selects, knobs adjust, and every control does what its label says.

## What is new compared to Felucca

DUELL is a fork of Felucca and keeps its engines. The additions are the two-part architecture and modes, MORPH with the smart random, MULTIs and named PATTERNS, ENV 2 / LFO 2 / modulation matrix, the new effects, the MIDI work, the new factory sounds and the interface changes above. See [SPEC.md](SPEC.md) for the full design.

## Try it

The quickest way is the [hosted emulator](https://oscillr.github.io/DUELL/emu/): open it, click the page once to start the sound. To build and run your own copy of the browser emulator (needs [emsdk](https://emscripten.org/docs/getting_started/downloads.html)):

```bash
python3 gen_only.py
. ~/emsdk/emsdk_env.sh
sh felucca-base/web/emu/build.sh
```

To run your own copy, serve `felucca-base/build/emu` with any static web server and open it in a browser. The emulator keeps its flash in the browser and has Download and Load buttons for saved data.

Run the host tests:

```bash
./host_tests.sh regress sounds morph morphui multi hang ui select
```

Building a device image is described in [felucca-base/BUILDING.md](felucca-base/BUILDING.md).

## Status and safety

- **DUELL 0.2.13 beta.** Developed emulator-first. The firmware has been flashed and run on **one** real FM-1 by the author (it works). The **return to the official firmware worked about three times, on the author's unit**, and has not been tried on others; broader testing is pending. The FM-1 has a single-bank layout with no known rollback. **Do not flash** a unit you cannot afford to lose. See [docs/RECOVERY.md](docs/RECOVERY.md) for what is known and unknown.
- An optional dual-core mode is planned. It will be **off by default** and is untested.
- A web editor adapted to DUELL is planned. Felucca's editor pages are still in `felucca-base/web/` and have not been adapted.
- **No warranty.** DUELL is licensed under the GPL-3.0, which includes its disclaimer of warranty. You install it at your own risk. DUELL is not affiliated with M-VAVE.
- Data saved with this beta (patches, MULTIs, PATTERNs) may not load in older or later betas. Back up first. See [CHANGELOG.md](CHANGELOG.md).

## Install, learn, test, report

| I want to... | Read |
|---|---|
| Try DUELL now | [Browser emulator](https://oscillr.github.io/DUELL/emu/) |
| Install the beta | [docs/INSTALL.md](docs/INSTALL.md), [installer page](https://oscillr.github.io/DUELL/install/) (source: `felucca-base/web/install/index.html`) |
| Play it | [docs/QUICKSTART.md](docs/QUICKSTART.md) |
| Fix a problem | [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) |
| Recover from a failed flash | [docs/RECOVERY.md](docs/RECOVERY.md) |
| Know what is broken or untested | [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) |
| Help test (emulator or FM-1) | [docs/TESTING.md](docs/TESTING.md), then open an issue with the [bug report form](https://github.com/oscillr/DUELL/issues/new/choose) |
| See what changed | [CHANGELOG.md](CHANGELOG.md) |
| Read the full design | [SPEC.md](SPEC.md) |

## Credits and licence

- DUELL is a fork of **Felucca** by Leo Kuroshita (@kurogedelic), Hügelton Instruments, and, like it, is licensed under the **GPL-3.0**. See [felucca-base/LICENSE](felucca-base/LICENSE) and [felucca-base/LICENSING.md](felucca-base/LICENSING.md). The upstream commit is recorded in [UPSTREAM_COMMIT.txt](UPSTREAM_COMMIT.txt).
- Thanks to **Baud Girl**, **Leo Kuroshita** and the whole community. The SHIM and RESO reverbs come from **JIANT**.
- DUELL is an independent project and is not affiliated with M-VAVE.

Third-party parts that ship with DUELL (from [felucca-base/LICENSING.md](felucca-base/LICENSING.md) and the licence texts in `felucca-base/LICENSES/`):

| Part | Licence |
|---|---|
| **JIANT** (juanjiant-bit): the SHIM and RESO reverb models, and only those | GPL-3.0 |
| Inter Tight font (UI text) | SIL OFL 1.1 |
| DotGothic16 font (emulator labels) | SIL OFL 1.1 |
| Fukiai icon font (Hügelton Instruments) | MIT |
| DaisySP, Rings (PHYS engine) | MIT |
| msfa from Dexed (FM6 engine) | Apache-2.0 |
| Three JieLi AC79 SDK files inside every firmware package | Apache-2.0 |
| Browser emulator design after X0X (charlesvestal) | GPL-3.0 |

The DUELL logo and artwork are the work of the owner (OSCILL/R) and are not part of the GPL licence. The code and the splash graphic as shipped in the firmware are GPL-3.0-only.
