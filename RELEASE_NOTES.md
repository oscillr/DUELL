# DUELL 1.3

**DUELL**
**MultiMorphing Bitimbral Synthesizer**
**by OSCILL/R**

DUELL 1.3 is an update for the M-VAVE FM-1. It plays with Novation Circuit Tracks and Akai MPC / Force out of the box, and adds MACROS and your own PERFORM knobs.

## What is new
- **MIDI PRESETS for Novation Circuit Tracks and Akai MPC / Force.** Pick your device at the first start and DUELL is set up for it, with the right mode recommended.
- **MACROS.** TIMBRE, ENV MACRO, FILTER MACRO, CHARACTER and MOTION: each turns several sound settings at once, and the middle is always the sound as saved. Play them on HOME, with SEL held, or over MIDI CC 56 to 62.
- **Your own PERFORM knobs.** Choose what KNOBS 2, 3 and 4 do on PERFORM.
- **MORPH in the mod matrix.** An LFO, an envelope or any other source can move the MORPH fader.

## Changed
- SETTINGS in seven sections, with one key rule on every page: SELECT moves, OCT+ opens, OCT- goes back, the knobs change values.
- VELOCITY > FILTER ENV is set in each patch.
- ANALOG PULSE WIDTH for the PWM wave. TRIO INTERVAL 2 and 3 reach plus or minus 36 semitones.
- LOFI shows more AVAILABLE VOICES.
- The web pages have the DUELL look.

## Fixed
- MIDI notes follow the part's TRANSPOSE ([issue #4](https://github.com/oscillr/DUELL/issues/4), thanks @thecandybars).
- The installer's backup works, and the way back to the official firmware takes the V15 or V16 download ([issue #5](https://github.com/oscillr/DUELL/issues/5), thanks @jaggednz).
- A matrix route to MORPH no longer costs voices.
- PERFORM BALANCE, ANALOG PULSE WIDTH and MIDI LEARN screen details.

## Read this before you flash
- DUELL has run on **two FM-1 units, the author's**, and more than 100 people have installed it through the Firmware Hub. The FM-1 has a single copy of its firmware and no known rollback besides the way back to the official firmware (V15 or V16).
- **You install at your own risk. No warranty** (GPL-3.0). Use a data USB cable straight into the computer and never interrupt the write.
- DUELL is **not affiliated with M-VAVE**. M-VAVE's firmware is **not distributed here**: to go back you download V15 or V16 yourself from <https://www.m-vave.com/download>.
- Recovery notes: [docs/RECOVERY.md](https://github.com/oscillr/DUELL/blob/main/docs/RECOVERY.md).

## Assets
| File | What |
|---|---|
| `duell-1.3.fwsc` | the device package (the installer page loads this one) |
| `SHA256SUMS` | checksums |

```
SHA256: a28fcd89ba4622eb8d1d6db5e0bba4c386a287756903523271e1863ff4b9345b  duell-1.3.fwsc
```

## Links
- Installer: <https://oscillr.github.io/DUELL/install/>
- Web editor: <https://oscillr.github.io/DUELL/editor/>
- Emulator: <https://oscillr.github.io/DUELL/emu/>

Known limits: [docs/KNOWN_LIMITS.md](https://github.com/oscillr/DUELL/blob/main/docs/KNOWN_LIMITS.md). Bugs: the [report form](https://github.com/oscillr/DUELL/issues/new/choose).
