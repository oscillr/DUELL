# Known issues: DUELL 0.1 beta

- **Emulator first, no firmware download yet.** The browser emulator is the public beta. The firmware has been flashed to and run on **one** real FM-1 by the author (it works), but broader hardware testing is pending and no firmware package is offered. The installer page says "firmware not yet released".
- **The way back to the official firmware is untested.** Single-bank layout, no known rollback. See [RECOVERY.md](RECOVERY.md).
- **Dual core is off and untested.** An optional second-core mode is planned, off by default; it has not been run on a unit.
- **MIDI is untested with many devices.** It follows the spec and the host tests; the emulator takes notes and controllers from a WebMIDI keyboard or controller (not clock, not SysEx). Real controllers, sequencers and DAWs on the hardware are still to be tested.
- **The patch store holds at most 48 patches** (copies of factory sounds count): the biggest possible patch is 435 bytes and 48 of them always fit, so a save never fails half way. At 48 the PATCHES page shows FREE 0 and says NO FREE PLACE (ERASE ONE); erase one to save another. Typical sounds are much smaller, so this is a deliberate limit, not what the flash could hold.
- **User sample slots are unused by factory sounds.** Slots 1 and 2 still work as in Felucca; slot 3's flash now holds the patch store.
- **Emulator: your data lives in the browser** (this site's storage in your browser, on this computer only). After an update of the emulator, clear the site data if saved data from an older build misbehaves. Press "Download saved data" first: it backs up everything the emulator stores. Clearing browser data or switching browser or computer loses it otherwise.
- **Backup is incomplete.** The device backup covers patches, settings, the old project data and user samples, but **not MULTIs and not the PATTERN slots**. Restore uses the editor, which is still Felucca's page and has not been adapted to DUELL or tested with it.
- **The editor is not adapted.** `felucca-base/web/editor.html` still says Felucca and DUELL's new parameters are only partly described there.
- **On-device backup/restore** is therefore only partly possible. In the emulator, "Download saved data" and "Load saved data" cover everything the emulator stores, but that file is emulator-only: there is no tool to move it onto a device.
- **Data may not carry across betas.** Patches, MULTIs and PATTERNs saved here may not load in older or later betas. Old Felucca PROJECT data is not migrated (it reads as empty). Patches saved by this build may read as damaged on older firmware.
- **Not built yet:** random-pattern action, tempo-synced ducker release, the universal filter, a web editor for DUELL.
- **Version text:** a release build made with `--release 0.1-beta` shows "DUELL 0.1 beta" on the device (splash, ABOUT, MENU > SYSTEM > INFO) and writes `duell-0.1-beta.fwsc`.
