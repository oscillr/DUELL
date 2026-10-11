# Troubleshooting

Symptom, then fix. For a failed or interrupted flash read [RECOVERY.md](RECOVERY.md).

| Symptom | Fix |
|---|---|
| Installer says the browser cannot do it | Use Chrome or Edge on a computer. Safari and Firefox lack WebMIDI SysEx. |
| "MIDI (SysEx) access was not allowed" | Allow MIDI in the site settings in the address bar, then try again. |
| "FM-1 not found" | Use a **data** USB cable, straight into the computer (no hub). Close DAWs and other tabs using MIDI. Switch the unit off and on. |
| "The connected device is not an FM-1" | Unplug other MIDI devices and try again. |
| Install button stays grey | Tick all four checklist boxes, and load a firmware file. "Build not yet attached" means there is no file yet: choose one you built. |
| "This file cannot be installed" | The file is not a DUELL/Felucca package. Rebuild with `tools/build.py --release 0.1-beta`. Never use a patched official file. |
| It stopped halfway / "USB connection was lost" | Replug and press Install again; the unit stays in update mode and the page resumes. |
| "The update loader did not appear" | Replug the cable and press Install again. |
| Done, but the unit does not behave | Unplug and plug it in again. Check HOME (hold) > ABOUT for the version. |
| Backup says the firmware cannot make a backup | The unit runs the official or an old firmware. Nothing was saved; back up on a Felucca/DUELL build only. |
| Emulator shows old or broken saved data after an update | Download saved data first, then clear the site's browser data (cookies and site data) and reload. |
| Emulator is silent | Click the page once (browsers block audio until you do) and raise MASTER. |
| Emulator pitch sounds off | The browser plays at a rate other than 44.1 kHz; the console warns about it. |
| A sound does not change when I turn SELECT | SELECT switches sub-pages and moves rows; knobs 1-4 adjust. See [QUICKSTART.md](QUICKSTART.md). |
| MIDI notes play the wrong part | Part 1 listens on channel 1 and part 2 on channel 2. In LAYER, channel 1 plays both parts. |
| A saved patch will not load after an update | Saved formats may change between betas. Restore from a backup made on the same beta, or re-save. |
| NO FREE PLACE (ERASE ONE) | 48 patches are stored (FREE 0 on the PATCHES card). Erase a patch you do not need (PATCHES, knob 4 ERASE), then save again. Saving over a patch you already stored always works. (STORE FULL should never appear; if it does, erase a patch.) |
| STOP TO SAVE | Saving needs the transport stopped. Press PLAY/STOP. |
| Anything else | Open a bug report with the [form on GitHub](https://github.com/oscillr/DUELL/issues/new/choose). |
