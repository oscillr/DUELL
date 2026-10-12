# Recovery: if a flash is interrupted or fails

**DUELL beta**, MultiMorphing Bitimbral Synthesizer by OSCILL/R

Plain version: **DUELL 0.2.4 beta has run on only one real FM-1 (the author's), the way back to the official firmware (V15) has worked about three times on that unit and has not been tried on others, and nobody can promise that a failed flash is fixable.** You install at your own risk. This page says what we know, what we do not know, and what to try if you end up in this situation anyway.

## What is known
- The FM-1 has a **single-bank layout**: there is one copy of the firmware, not a spare. We know of **no rollback** built into the unit.
- The installer works in two steps. Step 1: the running firmware stages a small **update loader** (nothing is written to the main firmware yet; if the page says "stopped" or "lost" here, it also says nothing was written). Step 2: the loader writes the new firmware. The unit **stays in update mode until a write has finished**, so pressing Install again resumes it. This comes from Felucca's installer code, which DUELL keeps.
- The installer can write the official firmware (V15) back, if you supply M-VAVE's own file.

## What is not known
- Whether DUELL beta behaves the same on other units. It has been flashed to and run on one FM-1 by the author, and works there; every other unit is untested.
- Whether the **return to the official firmware (V15)** works on every unit. It worked about three times, on the author's unit, with this installer's return button; the unit booted normally on V15.
- What happens if power or USB is lost in the middle of step 2 on your particular unit. Felucca's author points to an external recovery tool (FM-1-transporter) for a unit that no longer starts. DUELL's author owns no such tool, and **we have not tested it**.
- Whether every unit behaves alike.

## What to try, in this order
1. **Do not panic and do not keep pulling cables.** Let the page show its message.
2. **Replug.** Unplug the USB cable, plug it back in (data cable, straight into the computer, no hub), reload the installer page and press Install again.
3. **Re-enter update mode.** If the unit does not respond normally, the installer detects an unit already waiting in update mode and resumes the write. If the page cannot see the unit, try another USB port, another data cable, another browser tab with no other MIDI program open. Switch the unit off and on.
4. **Re-flash the official firmware.** On the installer page use the amber RETURN TO OFFICIAL FIRMWARE card ("Go back to the official firmware") with your own copy of the official V15 `FM-1.fwsc` from <https://www.m-vave.com/download>. If the unit is in update mode and you already saved a backup, tick "Resume an interrupted return".
   While it runs, the page shows an amber banner "RETURNING TO THE OFFICIAL FIRMWARE" and the progress "Writing official firmware" only in the RETURN card; the DUELL install card stays empty and is locked. If you see that banner, **it is not installing DUELL again. Do not unplug and do not close the page until it says DONE.** If it fails, the message stays in the RETURN card: press the same button again.
5. **Ask for help** with an issue on GitHub: say which step stopped, the page's message, and what the screen shows. Do not post your device's data.
6. If none of this brings the unit back, you may need hardware recovery tools (such as the community FM-1-transporter) or M-VAVE support. We cannot promise either.

## Your saved data
A flash (also going back to the official firmware and installing DUELL again) keeps your saved data, so the unit comes back as it was left. To start from scratch use MENU > SYSTEM > ERASE DATA on the unit, or the installer's optional "Erase my saved data after installing" box ([INSTALL.md](INSTALL.md), "Keep or erase your data"). Back up first; it cannot be undone.

## Before you flash
Try the [browser emulator](https://oscillr.github.io/DUELL/emu/) first if you like. Then follow the checklist in [INSTALL.md](INSTALL.md): data cable, no hub, back up, never interrupt, replug afterwards. **You install at your own risk.**
