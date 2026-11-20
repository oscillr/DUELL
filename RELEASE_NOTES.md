# DUELL 1.3.6

**DUELL**
**MultiMorphing Bitimbral Synthesizer**
**by OSCILL/R**

DUELL 1.3.6 runs on the M-VAVE FM-1 and the M-VAVE SMK-37 (Pro, Elite and MKE-P37).

**New in 1.3.6: built on GitHub.** GitHub builds every package from the source at this release and signs it, so anyone can check that the file they install is exactly the public code. The Firmware Hub shows it as "Built from source on GitHub". Nothing changes on the unit: if you run 1.3.5, you do not need to update.

## New in 1.3.5: DUELL on the SMK-37
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
| `duell-1.3.6.fwsc` | the FM-1 package (the installer page loads this one) |
| `duell-1.3.6-smk37-pro.fwsc` | the SMK-37 Pro package |
| `duell-1.3.6-smk37-elite.fwsc` | the SMK-37 Elite package |
| `duell-1.3.6-smk37-mke.fwsc` | the MKE-P37 package |
| `SHA256SUMS` | checksums |

`SHA256SUMS` on the release page lists the checksum of each package.

## Links
- Installer: <https://oscillr.github.io/DUELL/install/>
- Web editor: <https://oscillr.github.io/DUELL/editor/>
- Emulator: <https://oscillr.github.io/DUELL/emu/>

Known limits: [docs/KNOWN_LIMITS.md](https://github.com/oscillr/DUELL/blob/main/docs/KNOWN_LIMITS.md). Bugs: the [report form](https://github.com/oscillr/DUELL/issues/new/choose).
