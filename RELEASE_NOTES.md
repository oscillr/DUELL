# DUELL 1.3.1

**DUELL**
**MultiMorphing Bitimbral Synthesizer**
**by OSCILL/R**

DUELL 1.3.1 is a small update for the M-VAVE FM-1: DUELL can now speak.

## What is new
- **Screen reader support.** Open Charles Vestal's [FM-1 Screen Reader](https://charlesvestal.github.io/fm1-reader/) page in Chrome or Edge with your FM-1 on USB, and DUELL says what is on its screen: the page and its knobs, each knob as you turn it, SETTINGS rows, messages, questions and the first-start guide. Use your own screen reader (VoiceOver, NVDA, JAWS, TalkBack) or the page's own voice. Without the reader page DUELL stays silent. Thanks for the idea, @woodjackmusic-JD ([issue #2](https://github.com/oscillr/DUELL/issues/2)).
- **The web emulator speaks too.** Its new Screen reader section reads the screen with your screen reader or its own voice.

Everything new in 1.3 is in the [changelog](https://github.com/oscillr/DUELL/blob/main/CHANGELOG.md).

## Read this before you flash
- DUELL has run on **two FM-1 units, the author's**, and more than 100 people have installed it through the Firmware Hub. The FM-1 has a single copy of its firmware and no known rollback besides the way back to the official firmware (V15 or V16).
- **You install at your own risk. No warranty** (GPL-3.0). Use a data USB cable straight into the computer and never interrupt the write.
- DUELL is **not affiliated with M-VAVE**. M-VAVE's firmware is **not distributed here**: to go back you download V15 or V16 yourself from <https://www.m-vave.com/download>.
- Recovery notes: [docs/RECOVERY.md](https://github.com/oscillr/DUELL/blob/main/docs/RECOVERY.md).

## Assets
| File | What |
|---|---|
| `duell-1.3.1.fwsc` | the device package (the installer page loads this one) |
| `SHA256SUMS` | checksums |

```
SHA256: 36184f90ad6a02e69850aedcb13084711b4eed2679eacdaaa2703efc6f810440  duell-1.3.1.fwsc
```

## Links
- Installer: <https://oscillr.github.io/DUELL/install/>
- Web editor: <https://oscillr.github.io/DUELL/editor/>
- Emulator: <https://oscillr.github.io/DUELL/emu/>

Known limits: [docs/KNOWN_LIMITS.md](https://github.com/oscillr/DUELL/blob/main/docs/KNOWN_LIMITS.md). Bugs: the [report form](https://github.com/oscillr/DUELL/issues/new/choose).
