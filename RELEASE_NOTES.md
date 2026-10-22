# DUELL 1.0

Text for the GitHub release page. Tag `v1.0`, title `DUELL 1.0`. Paste everything below the line.

---
**DUELL**
**MultiMorphing Bitimbral Synthesizer**
**by OSCILL/R**

**Two synths in one small box.** DUELL is free, open-source firmware (GPL-3.0) for the M-VAVE FM-1, a fork of Felucca: a bitimbral synthesizer with two independent parts, playable in three modes (DUEL, STEREO, LAYER), with a morph that glides between two versions of every sound. This release is the device package, the web installer and the web editor.

## What is in 1.0
- **128 factory patches**, levelled by their TRIM, including the new **#25 DRIVEN SAW** (a driven lead that uses both filters). **15 factory multis** and the CPU test M16, **32 patterns** in one list.
- **Two filters on every engine:** the **MULTI FILTER** and the **TWIN FILTER** (types OFF, LP, BP, HP, NOTCH, LADDER, COMB; DRIVE, SERIES or PARALLEL, BLEND, LINK with the offset in whole semitones). A **SUB OSC** layer, **WAVE STEPS** and a **CYCLE** envelope.
- **Two cores:** part 2 plays on the chip's second core from every power-on, with an automatic fallback to one core if it does not answer or misbehaves.
- **The update screen:** while the firmware is written the unit shows DUELL over moving wave lines, WRITING, the percent, a bar of 20 segments and DO NOT SWITCH OFF.
- **MIDI CC map** with 70 direct CCs (list in the README), MIDI LEARN for the rest.
- **Key velocity and aftertouch** (menu: AFTERTOUCH on, KEY VELOCITY off by default).
- **Web editor** with the ENGINE selector, the patch library, the multis, a piano roll, wavetable files, DX7 `.syx` import and a Backup page.
- Morph: ROLL, UNDO and AMOUNT, an End B you can set by hand.
- Effects: per part distortion, INSERT, EQ, ducker; chorus, a delay with four modes, six reverbs, BASS MONO, a master compressor and a stereo WIDTH.

## Changed since the 0.2.20 beta (the last public one)
Everything of the 0.2.21 to 0.2.33 test builds is in [CHANGELOG.md](https://github.com/oscillr/DUELL/blob/main/CHANGELOG.md). The ones you will notice first:
- The bank is new (128 patches); **patch numbers changed**, a slot you never saved shows the factory sound at its place.
- The two filters are called MULTI FILTER and TWIN FILTER everywhere.
- The MENU > SYSTEM > TEST screen is not in this release; M16 STRESS TEST stays as a multi you may overwrite.

## Read this before you flash
- DUELL has run on **only one FM-1, the author's**. Other units are untested. The way back to the official firmware (V15) has worked on that unit; it is not tested elsewhere. The FM-1 has a single copy of its firmware and no known rollback.
- **You install at your own risk. No warranty** (GPL-3.0). Use a data USB cable straight into the computer and never interrupt the write.
- DUELL is **not affiliated with M-VAVE**. M-VAVE's firmware is **not distributed here**: to go back you download V15 yourself from <https://www.m-vave.com/download>.
- Recovery notes: [docs/RECOVERY.md](https://github.com/oscillr/DUELL/blob/main/docs/RECOVERY.md).

## Assets
| File | What |
|---|---|
| `duell-1.0.fwsc` | the device package (the installer page loads this one) |
| `SHA256SUMS` | checksums |

```
SHA256: f0fd3b46ec013febc8585fa153f782a7414a0b84e9bfe4f1da8147cfe83ba462  duell-1.0.fwsc   (size: 614,666 bytes)
```

## How to install
Open the installer page in **Chrome or Edge**: <https://oscillr.github.io/DUELL/install/>. Save a backup, connect the FM-1, press Install and wait for Done; then switch the unit off and on once. Steps: [docs/INSTALL.md](https://github.com/oscillr/DUELL/blob/main/docs/INSTALL.md). The page shows the package's SHA-256 so you can compare it with the one above.

The web editor (Chrome or Edge, unit connected over USB): <https://oscillr.github.io/DUELL/editor/>.

## Known limits
Tested on one unit only; 11 pattern slots share one storage object of 3,584 bytes; some LFO depth destinations are hidden; KEY VELOCITY is off by default; MIDI is not tested with many devices. The whole list: [docs/KNOWN_LIMITS.md](https://github.com/oscillr/DUELL/blob/main/docs/KNOWN_LIMITS.md) and [docs/KNOWN_ISSUES.md](https://github.com/oscillr/DUELL/blob/main/docs/KNOWN_ISSUES.md). Bugs: the [report form](https://github.com/oscillr/DUELL/issues/new/choose).

## Credits
Special thanks to: Felucca and its author Leo Kuroshita, JIANT (reverbs), CHOMPI Club (plate reverb and delay ideas), Emilie Gillet of Mutable Instruments (reverb design), Charles Vestal (Velo, the method for key velocity and aftertouch, and X0X), aquanodemusic (a CC0 FM voice), Baud Girl (for starting it all), keremimo (Melodee), Quixotic7 (ChoralRoot), zvenson (zp12), isod89 (SLOOP), and the whole community. Every third-party part and its licence: [CREDITS.md](https://github.com/oscillr/DUELL/blob/main/CREDITS.md), [NOTICE.md](https://github.com/oscillr/DUELL/blob/main/NOTICE.md).
