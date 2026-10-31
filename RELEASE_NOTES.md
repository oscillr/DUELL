# DUELL 1.1

**DUELL**
**MultiMorphing Bitimbral Synthesizer**
**by OSCILL/R**

DUELL 1.1 is an update for the M-VAVE FM-1. The whole user interface is redesigned, so you see more at a glance, and PERFORM, STRUM and MORPH have new things to play with.

## What is new

**The new design**
- Every page is redesigned.
- The part marker in the header shows the available voices in POLY, and M, L or U in MONO, LEGATO and UNISON.
- The quick pages GLO, SEQ, REC, ENV, LFO and EDIT look and behave the same in PERFORM as outside it. While you hold one, the white keys follow that page.
- The matrix and the CYCLE, SUB and TWIN MOD cards show each value as it sounds at the current morph position, and matrix names are written out in full.

**Play**
- STRUM on the CHORD page (K3): OFF, or 1 to 100ms between the notes of a chord, saved with the patch.
- On the PERFORM page the SELECT knob is the balance between the two parts.
- SPLIT starts on the middle key: turn it up from OFF and the first step lands on F#4.

**MORPH**
- ROLL is chained: each roll continues from the sound that end has now. UNDO goes back up to two rolls.
- End B is saved and loaded exactly as it sounds. You can edit B and it is saved with the patch.
- The SMART and WILD randomizers are reworked.

**Sequencer**
- Parameter locks are recorded by turning any knob while REC is armed. The PARAMETER LOCKS page is the list where you change, delete and switch what was recorded.
- New tools TRANS + and TRANS - move the whole pattern by one semitone. SHIFT < and SHIFT > move it in time.
- Swing runs from 50 to 75 percent on ARP, QUICK SEQ and TEMPO.

## Read this before you flash
- DUELL has run on **two FM-1 units, the author's**, and more than 100 people have installed it through the Firmware Hub. The FM-1 has a single copy of its firmware and no known rollback besides the way back to the official firmware (V15).
- **You install at your own risk. No warranty** (GPL-3.0). Use a data USB cable straight into the computer and never interrupt the write.
- DUELL is **not affiliated with M-VAVE**. M-VAVE's firmware is **not distributed here**: to go back you download V15 yourself from <https://www.m-vave.com/download>.
- Recovery notes: [docs/RECOVERY.md](https://github.com/oscillr/DUELL/blob/main/docs/RECOVERY.md).

## Assets
| File | What |
|---|---|
| `duell-1.1.fwsc` | the device package (the installer page loads this one) |
| `SHA256SUMS` | checksums |

```
SHA256: f8f8cdee93bc72061c2ac1b82a4d3fa3704aa5a2247b498c6802f7c518c37dc3  duell-1.1.fwsc
```

## Links
- Installer: <https://oscillr.github.io/DUELL/install/>
- Web editor: <https://oscillr.github.io/DUELL/editor/>
- Emulator: <https://oscillr.github.io/DUELL/emu/>

Known limits: [docs/KNOWN_LIMITS.md](https://github.com/oscillr/DUELL/blob/main/docs/KNOWN_LIMITS.md). Bugs: the [report form](https://github.com/oscillr/DUELL/issues/new/choose).
