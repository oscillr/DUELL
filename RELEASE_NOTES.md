# DUELL 1.3.5

**DUELL**
**MultiMorphing Bitimbral Synthesizer**
**by OSCILL/R**

DUELL 1.3.5 runs on the M-VAVE FM-1 and now also on the M-VAVE SMK-37 (Pro, Elite and MKE-P37).

## New: DUELL on the SMK-37
- **Both parts on one screen.** KNOB 1 to 4 set part 1 and KNOB 5 to 8 set part 2, on every page.
- **The faders** set each part's LEVEL and MORPH.
- **The 16 pads are PERFORM's effects**: hold one, press harder for more. The keys keep playing notes.
- **The keys split at F#4** in STEREO and DUEL, and SPLIT moves the split point. GLOBE + CHN 1 or CHN 2 plays one part alone.
- **A start-up guide** shows what the SMK-37's buttons do.
- **One installer page for every unit:** it picks the right package for the keyboard you connect. The editor works with the SMK-37 too and opens the FM-1 Cheat Sheet or the SMK-37 Cheat Sheet, for the unit you connect.
- **SMK-37 Cheat Sheet:** <https://oscillr.github.io/DUELL/cheatsheet/smk37/>

## What has changed on both
- **PAN shows L 100, CENTER, R 100.**
- Small fix: PERFORM DRIVE is never louder than the dry sound.

Everything since 1.3 is in the [changelog](https://github.com/oscillr/DUELL/blob/main/CHANGELOG.md).

## Read this before you flash
- **Install at your own risk.** Back up first. No warranty (GPL-3.0). Use a data USB cable straight into the computer and never interrupt the write.
- **SMK-37:** each model has its own package (Pro, Elite, MKE-P37). The installer page takes the SMK-37 Elite's official `SMK37-Elite-V16.fwsc` for the way back. For the SMK-37 Pro and the MKE-P37, use M-VAVE's own updater to go back.
- DUELL is **not affiliated with M-VAVE**. M-VAVE's firmware is **not distributed here**: to go back you download it yourself from M-VAVE (<https://www.m-vave.com/download>, for the SMK-37 <https://open.m-vave.com>).
- Recovery notes: [docs/RECOVERY.md](https://github.com/oscillr/DUELL/blob/main/docs/RECOVERY.md).

## Assets
| File | What |
|---|---|
| `duell-1.3.5.fwsc` | the FM-1 package (the installer page loads this one) |
| `duell-1.3.5-smk37-pro.fwsc` | the SMK-37 Pro package |
| `duell-1.3.5-smk37-elite.fwsc` | the SMK-37 Elite package |
| `duell-1.3.5-smk37-mke.fwsc` | the MKE-P37 package |
| `SHA256SUMS` | checksums |

```
SHA256: 0515ae00a9c393a361574a2ef9875d3ac22e206b66f6b88ec1bbca46200bf202  duell-1.3.5.fwsc
SHA256: 924b2ba398e3b30c4c58f712435fce06ac2cbf2a46396e720d6e80a2b224773f  duell-1.3.5-smk37-pro.fwsc
SHA256: 0a8fd7dcaf27584a45bcaed1cc4249453732040230324cd57de14113b587c7fb  duell-1.3.5-smk37-elite.fwsc
SHA256: 74edffb02edc36620d9c82ef6d21e9b8d7b658fb716dde7e1d29a17b0051559b  duell-1.3.5-smk37-mke.fwsc
```

## Links
- Installer: <https://oscillr.github.io/DUELL/install/>
- Web editor: <https://oscillr.github.io/DUELL/editor/>
- Emulator: <https://oscillr.github.io/DUELL/emu/>

Known limits: [docs/KNOWN_LIMITS.md](https://github.com/oscillr/DUELL/blob/main/docs/KNOWN_LIMITS.md). Bugs: the [report form](https://github.com/oscillr/DUELL/issues/new/choose).
