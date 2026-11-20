# DUELL 1.3.3

**DUELL**
**MultiMorphing Bitimbral Synthesizer**
**by OSCILL/R**

DUELL 1.3.3 is a small update for the M-VAVE FM-1: a few updates and small fixes.

## What has changed
- **ALGORITHM is remembered:** each multi keeps its own, and it comes back after power-on. MACROS 1/2 and 2/2 have PART 1 / BOTH / PART 2, like MORPH.
- **A " *" after the patch name** shows that you have changed the sound since you loaded or saved it. A part of a multi that is not an exact patch shows the multi's name.
- **HOLD stays as you set it** when you load another sound.
- The patches, multis and patterns a slot holds until you save over it are called "default" on the unit, in the editor and in the installer.
- Small fixes: INSERT and SAT work on the whole sound with SPREAD up, and the unit shows a full-screen message when it plays on one core.
- **The CHORD list starts with MAJ.** DIA3 and DIA7 are at the end.
- **M16 is SLOW THAW**, an FM6 glass sequence over a wide PHASE pad.
- **Flat EQ** on the patches and multis, except the basses: they keep a gentle treble cut and get a small low lift.

Everything since 1.3 is in the [changelog](https://github.com/oscillr/DUELL/blob/main/CHANGELOG.md).

## Read this before you flash
- DUELL has run on **two FM-1 units, the author's**, and more than 100 people have installed it through the Firmware Hub. The FM-1 has a single copy of its firmware and no known rollback besides the way back to the official firmware (V15 or V16).
- **You install at your own risk. No warranty** (GPL-3.0). Use a data USB cable straight into the computer and never interrupt the write.
- DUELL is **not affiliated with M-VAVE**. M-VAVE's firmware is **not distributed here**: to go back you download V15 or V16 yourself from <https://www.m-vave.com/download>.
- Recovery notes: [docs/RECOVERY.md](https://github.com/oscillr/DUELL/blob/main/docs/RECOVERY.md).

## Assets
| File | What |
|---|---|
| `duell-1.3.3.fwsc` | the device package (the installer page loads this one) |
| `SHA256SUMS` | checksums |

```
SHA256: cbf6c53d1e11b2ac6fcd65eff961799b170e422ceff222f9a7792b195a04ac2c  duell-1.3.3.fwsc
```

## Links
- Installer: <https://oscillr.github.io/DUELL/install/>
- Web editor: <https://oscillr.github.io/DUELL/editor/>
- Emulator: <https://oscillr.github.io/DUELL/emu/>

Known limits: [docs/KNOWN_LIMITS.md](https://github.com/oscillr/DUELL/blob/main/docs/KNOWN_LIMITS.md). Bugs: the [report form](https://github.com/oscillr/DUELL/issues/new/choose).
