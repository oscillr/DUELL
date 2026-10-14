# Known issues: DUELL 0.2.13 beta

- **Tested on one unit.** The firmware has been flashed to and run on **one** real FM-1, the author's (it works). Other units, cables and USB hubs are untested. Use a data cable straight into the computer, no hub.
- **The way back to the official firmware** (V15) has worked on the author's unit about three times: the installer's return button wrote V15 and the unit booted normally. Other units are untested. Single-bank layout, no known rollback beyond that. See [RECOVERY.md](RECOVERY.md).
- **Backup is incomplete and restore is untested.** The device backup covers patches, settings and the old project data, but **not MULTIs and not the PATTERN slots**. Restore uses the editor, which has not been adapted to DUELL, so it has not been tested. Write down or keep what you need from your multis and patterns before an ERASE DATA.
- **The editor is not adapted.** `felucca-base/web/editor.html` in the source tree still says Felucca and DUELL's new parameters are only partly described there. A web editor for DUELL is not built yet.
- **Very rare missed audio blocks.** When you switch to a very heavy multi, a block of audio can be late once in a while; you may hear a short click. Normal loads of patches and multis do not do this.
- **Factory sounds can be overwritten.** Saving over a factory sound stores your own copy; ERASE on that copy brings the factory sound back. Copies count toward the store.
- **The patch store holds at most 48 patches** (copies of factory sounds count): the biggest possible patch is 435 bytes and 48 of them always fit, so a save never fails half way. At 48 the PATCHES page shows FREE 0 and says NO FREE PLACE (ERASE ONE); erase one to save another.
- **Dual core is off and untested.** An optional second-core mode is planned, off by default; it has not been run on a unit.
- **MIDI is untested with many devices.** It follows the spec and the host tests; real controllers, sequencers and DAWs are still to be tested. The emulator takes notes and controllers from a WebMIDI device (not clock, not SysEx).
- **User sample slots:** gone since 0.2.9, with the sample engines. Slot 3's flash now holds the patch store.
- **No rename on the device** for a stored patch or multi for now (the page buttons leave the NAME screen without writing); the editor can rename.
- **Data may not carry across betas.** Patches, MULTIs and PATTERNs saved here may not load in older or later betas. Old Felucca PROJECT data is not migrated (it reads as empty). Patches saved by this build may read as damaged on older firmware. Back up first.
- **Emulator: your data lives in the browser** (this site's storage, on this computer only). After an emulator update, clear the site data if saved data from an older build misbehaves. Press "Download saved data" first. That file is emulator-only: there is no tool to move it onto a device.
- **Not built yet:** random-pattern action, tempo-synced ducker release.
- **Version text:** the device shows "DUELL 0.2.13 beta" (splash, ABOUT). The release has no TEST screen and no test multis.
