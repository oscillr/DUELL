# Known limits: DUELL 1.0

**DUELL**
MultiMorphing Bitimbral Synthesizer
by OSCILL/R

What you can meet in 1.0, in plain words. More detail in [KNOWN_ISSUES.md](KNOWN_ISSUES.md).

- **One unit tested.** DUELL has run on one real FM-1, the author's. Other units, cables and hubs are untested.
- **The way back (V15)** has worked on that unit only. There is no known rollback beyond it. See [RECOVERY.md](RECOVERY.md).
- **Pattern slots share storage.** The 32 patterns are stored in groups: one storage object holds 11 slots in 3,584 bytes, so the 11 slots of a group share that room.
- **Hidden destinations.** LFO DEPTH and DEPTH 2 do not show as matrix destinations (the depth cards of the pages move them). Two factory patches, #28 TORCH SONG and #108 YO YA, still carry an aftertouch route to the hidden DEPTH 2, so their LFO 2 does nothing without aftertouch.
- **KEY VELOCITY is off by default.** With it on, a very hard press can come out quiet; the feel has not been measured. AFTERTOUCH is on by default.
- **BREATH (CC2) and FOOT (CC4)** are reserved as matrix sources and are not read yet.
- **The editor has no MIDI MAP tab.** Assign CCs with MIDI LEARN on the unit.
- **SUB HARMONICS is switched off.** The HARM card is not on the SUB OSC page; CC17 has no effect.
- **No menu switch and no notice for the second core.** The fallback to one core is automatic and silent.
- **No error screen in the update loader.** The update screen shows progress only; if a write fails the installer page tells you.
- **Switching multis with presets** is not built.
- **MIDI** is not tested with many devices.
- **Heavy multis** can miss an audio block very rarely (a short click). The NOTES budget limits the notes per part to avoid it; the CPU figures in the documents are a computer model, the unit can read higher.
