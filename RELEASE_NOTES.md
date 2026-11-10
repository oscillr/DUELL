# DUELL 1.2

**DUELL**
**MultiMorphing Bitimbral Synthesizer**
**by OSCILL/R**

DUELL 1.2 is an update for the M-VAVE FM-1. Up to 16 voices a part from one shared voice pool over both cores, and it works with newer FM-1 units.

## What is new

**More voices**
- Both parts play from one shared pool of voices, spread over both cores. The old limit of 8 voices a part is gone: a part plays as many voices as the pool holds, up to 16.
- FM6 plays up to 12 voices (6 while the TWIN FILTER is COMB). VOICE is no longer held to 4.
- The number on the screen is what really plays.

**Fair sharing between the parts**
- Each part's AVAILABLE VOICES shows how many more notes that part can play right now. It drops when the other part plays.
- A key on a part that shows 0 takes the oldest note of the part that holds more.

## Fixed
- No crackle when chords start, when knobs or MIDI CCs move, or when the MORPH fader moves.
- WAVE chords in CHORD mode play clean.
- ANALOG no longer clicks on loud parts.
- WHEEL no longer freezes with 8 voices.
- The panel lights stay on while the unit saves.
- FM6: OP MODE, OP TUNE and OP ENV come right after the EDIT pages.
- Updated SCALE and CHORD keyboards.
- UI improvements: the header fits two-digit voice numbers, the voice number stays steady with the arp, lines under each parameter, text aligned on its baseline.
- Newer FM-1 units with the UC25HQ80 flash chip can save and update ([issue #3](https://github.com/oscillr/DUELL/issues/3)).
- The installer finds any FM-1, whatever its MIDI port is called.
- Other FM-1 firmwares can be installed from DUELL.

## Read this before you flash
- DUELL has run on **two FM-1 units, the author's**, and more than 100 people have installed it through the Firmware Hub. The FM-1 has a single copy of its firmware and no known rollback besides the way back to the official firmware (V15).
- **You install at your own risk. No warranty** (GPL-3.0). Use a data USB cable straight into the computer and never interrupt the write.
- DUELL is **not affiliated with M-VAVE**. M-VAVE's firmware is **not distributed here**: to go back you download V15 yourself from <https://www.m-vave.com/download>.
- Recovery notes: [docs/RECOVERY.md](https://github.com/oscillr/DUELL/blob/main/docs/RECOVERY.md).

## Assets
| File | What |
|---|---|
| `duell-1.2.fwsc` | the device package (the installer page loads this one) |
| `SHA256SUMS` | checksums |

```
SHA256: 031a3feef898b407537214f2694cb0c7e5014e2c02553c8d57b4865308aa3690  duell-1.2.fwsc
```

## Links
- Installer: <https://oscillr.github.io/DUELL/install/>
- Web editor: <https://oscillr.github.io/DUELL/editor/>
- Emulator: <https://oscillr.github.io/DUELL/emu/>

Known limits: [docs/KNOWN_LIMITS.md](https://github.com/oscillr/DUELL/blob/main/docs/KNOWN_LIMITS.md). Bugs: the [report form](https://github.com/oscillr/DUELL/issues/new/choose).
