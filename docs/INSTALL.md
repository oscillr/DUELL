# Installing DUELL beta

**DUELL**
MultiMorphing Bitimbral Synthesizer
by OSCILL/R

> **DUELL 0.2.20 beta is a firmware beta (0.2.4 was the first). Install at your own risk.** It has been flashed to and run on **one** real FM-1, the author's (it works). The way back to the official firmware (V15) has worked on that unit about three times and has not been tried on others. The FM-1 has a single-bank layout and there is no known rollback. No warranty. DUELL is not affiliated with M-VAVE, and M-VAVE's firmware is not distributed here. Read [RECOVERY.md](RECOVERY.md) and [KNOWN_ISSUES.md](KNOWN_ISSUES.md) first.
>
> The installer page is <https://oscillr.github.io/DUELL/install/>; the package `duell-0.2.20-beta.fwsc` is attached to the [release](https://github.com/oscillr/DUELL/releases) (compare its SHA-256 with the `SHA256SUMS` file there). If the page ever shows "DUELL firmware is not yet released", no package was found and the install flow stays hidden. You can try the [emulator](https://oscillr.github.io/DUELL/emu/) first.

> **Saves may change before the alpha.** Until the alpha release, the way patches, multis, settings and everything else are saved may change. Saves may stop loading between versions, and a backup made with one version may not restore in the next. Do not rely on them yet.

## What you need
- An M-VAVE FM-1.
- A computer with **Chrome or Edge** (the installer uses WebMIDI with SysEx; Safari and Firefox will not work).
- A **data** USB cable. Some cables only charge.
- Nothing to download for DUELL: the installer page loads the package itself (see "The file" below). Only for the way back you need M-VAVE's official V15 file.

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
| `tools/build.py --release` (recommended) | `duell-<VERSION>-beta.fwsc` (e.g. `duell-0.2.20-beta.fwsc`; the version is the `VERSION` file), plus a folder `release-<VERSION>-beta/` with the package, the app, `SHA256SUMS`, licences |
| `tools/build.py` (no flag) | the same package, without the release folder |

Every build also writes `duell-latest.fwsc` (the same file) and `duell-version.json` next to it.

(The package is named `duell-...`; the older name `felucca-0.1-beta.fwsc` is still found by the installer page. The device shows the version as "DUELL 0.2.20 beta" (the `VERSION` file). Build steps: [felucca-base/BUILDING.md](../felucca-base/BUILDING.md).)

The page reads the version from `duell-version.json` and looks for **`duell-latest.fwsc` next to itself** (then `duell-<VERSION>-beta.fwsc`, then the older names `duell-0.1-beta.fwsc`, `felucca-0.1-beta.fwsc`) (in `felucca-base/web/install/`). If it is not there, the page shows the notice **"DUELL firmware is not yet released"** and keeps the install flow hidden; the flow appears when the file exists. It accepts only a Felucca-loader package (identity `FM-1_9xx`, which `tools/build.py` makes `FM-1_901`), and it shows the SHA-256 so you can compare it with `SHA256SUMS`.

To view the page, serve the folder `felucca-base/web/` with any static web server, for example `python3 -m http.server -d felucca-base/web 8000`, and open `http://localhost:8000/install/`. (Opening the file straight from disk does not work, because the page loads modules.) The same page is published at <https://oscillr.github.io/DUELL/install/> (GitHub Pages), which loads the package attached to the latest release.

## The three cards
The installer page has three separate action cards, each with its own button, progress bar, status line and result line:
- **BACK UP** (step 2): reads the unit and saves a file. Nothing is written to the FM-1.
- **INSTALL DUELL** (step 5): writes DUELL. Shown only when a DUELL package is found or chosen. The bar says "Writing DUELL".
- **RETURN TO OFFICIAL FIRMWARE** (the section "Go back to the official firmware"): writes the official M-VAVE firmware back and removes DUELL. It has its own amber border and warning ("This writes the official M-VAVE firmware back. DUELL will be removed. Keep the cable connected until DONE."), and its bar says "Writing official firmware".

While one of them runs, a coloured banner at the top says which ("BACKING UP", "INSTALLING DUELL" or "RETURNING TO THE OFFICIAL FIRMWARE: do not unplug or close this page until it says DONE"), the other two buttons, the file pickers and the checklist are locked (grey, with a note), and the browser asks before the tab is closed. A progress bar appears only in the card that is running. A failure message stays in the card that failed: press the same button again and do not unplug.

## Steps
1. **Checklist.** Tick the four boxes on the page.
2. **Back up.** Press "Save a backup file". It downloads a `DUELL-backup-DATE.json`. Limits: it works only if the FM-1 already runs Felucca or DUELL (not the official firmware), **it does not include MULTIs or PATTERN slots**, and the restore side (the editor) has not been adapted to DUELL. More in [KNOWN_ISSUES.md](KNOWN_ISSUES.md).
3. **The file.** The page loads the DUELL package that sits next to it; you do not choose it. (Only the official M-VAVE file for the way back is chosen by hand.)
4. **Connect** the FM-1 and allow MIDI/SysEx in the browser.
5. **Install.** Wait for "Done".
6. **Unplug, plug in again**, then hold HOME and open ABOUT to see the version.

If anything stops: replug the cable and press Install again. The device stays in update mode until the write is finished. See [TROUBLESHOOTING.md](TROUBLESHOOTING.md) and [RECOVERY.md](RECOVERY.md).

## Keep or erase your data
- **A reinstall keeps your data.** Installing DUELL again, even after going back to the official firmware, does not touch the area where the unit keeps its saved data, so it comes back as you left it: your settings, patches, multis and patterns.
- **To start from scratch while installing:** the INSTALL DUELL card has the box **"Erase my saved data after installing (start from scratch)"**. The page checks what the FM-1 runs: if it runs DUELL, the box is unticked (an update keeps your patches and settings); otherwise it is ticked (clean install). The line "Detected: ..." says which case it found; if the page has not talked to the unit yet it says so (press "Check the unit"). You can tick or untick the box yourself at any time, and your choice always wins. If it is ticked, after the install has finished and the unit has restarted into DUELL, the page connects to it, shows "Erasing your data ..." (the card stays locked: do not unplug) and then "Done. Your saved data was erased. The unit starts as new." If the unit cannot be reached or refuses (for example while it is playing), the card says that the install itself succeeded, that your data was **not** erased, and to use the menu item below.
- **To start from scratch any time:** on the unit, hold HOME, go to SYSTEM > ERASE DATA, press OCT+ and confirm with OCT+ (OCT- or any page button cancels).
- **What is erased:** the settings (theme, scale, MIDI channels, favourites, preferences), the autosave, all your saved patches (every slot holds its factory sound again), your multis and your pattern slots. The factory sounds, the factory multis M01..M10 and the factory patterns come back, and the unit starts as new (VIOLET, LAYER, M01). **Not erased:** the FM6 voice bank and the firmware itself.
- **It cannot be undone: back up first** (step 2). The backup does not include MULTIs or PATTERN slots (see [KNOWN_ISSUES.md](KNOWN_ISSUES.md)), so write down or keep what you need from them.

## Return to the official firmware
1. Download the official FM-1 firmware (`FM-1.fwsc`, **V15**) yourself from M-VAVE: <https://www.m-vave.com/download>. This project does not host M-VAVE's files.
2. On the installer page, choose that file under "Return to the official firmware". It is checked in your browser (size and SHA-256 must match the unmodified V15; nothing is uploaded).
3. Press "Back up and return to official V15" in the amber RETURN card. The page saves a backup first (if the unit runs DUELL), then writes V15. All progress shows in that card only, under an amber banner. Keep the cable connected until the card says "Done. The official firmware is back. Switch the unit off and on." Then switch the unit off and on.

Your DUELL data cannot be opened by the official firmware. This path is the same method as for installing. It has worked on the author's unit about three times; it is untested on other units.

## Data compatibility
Patches, MULTIs and projects saved with 0.2.13 or earlier do not load after the update to 0.2.20: back up first. Patches, MULTIs and PATTERNs saved with this beta may also not load in older or later betas.
