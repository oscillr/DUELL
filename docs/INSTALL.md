# Installing DUELL 0.1 beta

**DUELL**
MultiMorphing Bitimbral Synthesizer
by OSCILL/R

> **The firmware is not released yet. There is nothing to install at the moment.** The public beta is the browser emulator: <https://oscillr.github.io/DUELL/emu/>. The firmware has been flashed to and run on **one** real FM-1 by its author (it works), but the way back to the official firmware has not been tested and broader testing is pending. The installer page (<https://oscillr.github.io/DUELL/install/>) says "firmware not yet released" and its flow is disabled. **Do not flash anything yet.**
>
> What follows describes the installer as it is built, for when the firmware package is released. The FM-1 has a single-bank layout and there is no known rollback. DUELL is not affiliated with M-VAVE. See [RECOVERY.md](RECOVERY.md).
> Until then, use the emulator. A release for the firmware will be announced on the [releases page](https://github.com/oscillr/DUELL/releases).

## What you need
- An M-VAVE FM-1.
- A computer with **Chrome or Edge** (the installer uses WebMIDI with SysEx; Safari and Firefox will not work).
- A **data** USB cable. Some cables only charge.
- The DUELL firmware file (see "The file" below).

## Flash checklist
1. Data USB cable, plugged **straight into the computer**. No hub, no dock.
2. **Back up first** (step 2 below).
3. Close other programs that use MIDI (DAWs, other browser tabs).
4. **Do not interrupt** the write: do not unplug, close the tab or let the computer sleep.
5. When it says Done, **unplug the FM-1 and plug it in again** before you use it.

## The file
The installer page is `felucca-base/web/install/index.html`. It keeps Felucca's install method (the running firmware stages an update loader, the loader writes the package) and works with a file that **`felucca-base/tools/build.py`** builds:

| Command | File written to `felucca-base/build/` |
|---|---|
| `tools/build.py --release 0.1-beta` (recommended) | `duell-0.1-beta.fwsc`, plus a folder `release-0.1-beta/` with the package, the app, `SHA256SUMS`, licences |
| `tools/build.py` (no flag) | `felucca.fwsc` |

(The package is named `duell-...`; the older name `felucca-0.1-beta.fwsc` is still found by the installer page. The device shows the version as "DUELL 0.1 beta". Build steps: [felucca-base/BUILDING.md](../felucca-base/BUILDING.md).)

The page looks for **`duell-0.1-beta.fwsc` next to itself** (or the older name `felucca-0.1-beta.fwsc`) (in `felucca-base/web/install/`). If it is not there, the page shows the notice **"DUELL firmware is not yet released"** and keeps the install flow hidden; the flow appears when the file exists. It accepts only a Felucca-loader package (identity `FM-1_9xx`, which `--release 0.1-beta` makes `FM-1_901`), and it shows the SHA-256 so you can compare it with `SHA256SUMS`.

To view the page, serve the folder `felucca-base/web/` with any static web server, for example `python3 -m http.server -d felucca-base/web 8000`, and open `http://localhost:8000/install/`. (Opening the file straight from disk does not work, because the page loads modules.) The same page is published at <https://oscillr.github.io/DUELL/install/> (GitHub Pages), which loads the package attached to the latest release.

## Steps
1. **Checklist.** Tick the four boxes on the page.
2. **Back up.** Press "Save a backup file". It downloads a `DUELL-backup-DATE.json`. Limits: it works only if the FM-1 already runs Felucca or DUELL (not the official firmware), **it does not include MULTIs or PATTERN slots**, and the restore side (the editor) has not been adapted to DUELL. More in [KNOWN_ISSUES.md](KNOWN_ISSUES.md).
3. **Load the file.** Automatic if the build is attached, otherwise choose it.
4. **Connect** the FM-1 and allow MIDI/SysEx in the browser.
5. **Install.** Wait for "Done".
6. **Unplug, plug in again**, then hold HOME and open ABOUT to see the version.

If anything stops: replug the cable and press Install again. The device stays in update mode until the write is finished. See [TROUBLESHOOTING.md](TROUBLESHOOTING.md) and [RECOVERY.md](RECOVERY.md).

## Return to the official firmware
1. Download the official FM-1 firmware (`FM-1.fwsc`, **V15**) yourself from M-VAVE: <https://www.m-vave.com/download>. This project does not host M-VAVE's files.
2. On the installer page, choose that file under "Return to the official firmware". It is checked in your browser (size and SHA-256 must match the unmodified V15; nothing is uploaded).
3. Press "Back up and return to official V15". The page saves a backup first (if the unit runs DUELL), then installs V15.

Your DUELL data cannot be opened by the official firmware. This path is the same method as for installing, and is **not yet tested** on real hardware.

## Data compatibility
Patches, MULTIs and PATTERNs saved with this beta may not load in older or later betas. Back up first.
