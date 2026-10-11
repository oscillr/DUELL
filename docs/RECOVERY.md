# Recovery: if a flash is interrupted or fails

**DUELL 0.1 beta**, MultiMorphing Bitimbral Synthesizer by OSCILL/R

Plain version: **DUELL has run on only one real FM-1 (the author's), the way back to the official firmware has not been tested, and nobody can promise that a failed flash is fixable.** That is why no firmware package is offered yet and nothing here is an instruction to flash. This page says what we know, what we do not know, and what to try if you end up in this situation anyway.

## What is known
- The FM-1 has a **single-bank layout**: there is one copy of the firmware, not a spare. We know of **no rollback** built into the unit.
- The installer works in two steps. Step 1: the running firmware stages a small **update loader** (nothing is written to the main firmware yet; if the page says "stopped" or "lost" here, it also says nothing was written). Step 2: the loader writes the new firmware. The unit **stays in update mode until a write has finished**, so pressing Install again resumes it. This comes from Felucca's installer code, which DUELL keeps.
- The installer can write the official firmware (V15) back, if you supply M-VAVE's own file.

## What is not known
- Whether DUELL 0.1 beta behaves the same on other units. It has been flashed to and run on one FM-1 by the author, and works there; every other unit is untested.
- Whether the **return to the official firmware (V15)** works on a real unit. It has not been tested yet.
- What happens if power or USB is lost in the middle of step 2 on your particular unit. Felucca's author points to an external recovery tool (FM-1-transporter) for a unit that no longer starts. DUELL's author owns no such tool, and **we have not tested it**.
- Whether every unit behaves alike.

## What to try, in this order
1. **Do not panic and do not keep pulling cables.** Let the page show its message.
2. **Replug.** Unplug the USB cable, plug it back in (data cable, straight into the computer, no hub), reload the installer page and press Install again.
3. **Re-enter update mode.** If the unit does not respond normally, the installer detects an unit already waiting in update mode and resumes the write. If the page cannot see the unit, try another USB port, another data cable, another browser tab with no other MIDI program open. Switch the unit off and on.
4. **Re-flash the official firmware.** On the installer page use "Return to the official firmware" with your own copy of the official V15 `FM-1.fwsc` from <https://www.m-vave.com/download>. If the unit is in update mode and you already saved a backup, tick "Resume an interrupted return".
5. **Ask for help** with an issue on GitHub: say which step stopped, the page's message, and what the screen shows. Do not post your device's data.
6. If none of this brings the unit back, you may need hardware recovery tools (such as the community FM-1-transporter) or M-VAVE support. We cannot promise either.

## Before you flash
The firmware is not released yet; wait for it and use the [browser emulator](https://oscillr.github.io/DUELL/emu/) meanwhile. When it is released, follow the checklist in [INSTALL.md](INSTALL.md): data cable, no hub, back up, never interrupt, replug afterwards. **You install at your own risk.**
