# The patch library in the web editor

**DUELL**
MultiMorphing Bitimbral Synthesizer
by OSCILL/R

The editor's **PATCHES** tab (GLOBAL column) is a library for the 128 patch slots on your unit. You can keep patches in files, move them between units, copy and swap slots, and bring in DX7 voices. It needs DUELL 0.2.19 or newer on the unit (the editor tells you if yours is older). Try it without a unit: open the editor with `?mock=1`.

Every slot is always writable. A slot holds either a patch you saved or the factory sound of that place; saving over a slot replaces what it holds, and **Restore** brings the factory sound back.

## What you can do

- **Export** one patch, a selection, or the whole bank to a file.
- **Import** from a patch file or a DX7 `.syx` file, with a preview and your choice of slots.
- **Drag a patch** onto another slot to copy it, or swap two slots in the wide view.
- **Drop a file or a patch on a part** to load it there without saving.
- **Wavetables** (the WAVETABLES tab): drop a WAV or a wavetable file, and export a table as a small file to share.

## The list and the wide view

The list shows all 128 slots: the number, the name, the engine and the type. Patches you saved are in bold. Click a slot to select it; Shift selects a range, Ctrl (or Cmd) adds one. The arrow keys move through the list, Space or Enter selects.

**Wide view** opens all 128 slots on the full width of the page, 16 to a row. It is for dragging a patch from one slot to another. Esc closes it.

## Export

- **Export this patch** (slot selected): one file with the patch whole.
- **Export selection**: the selected slots in one file.
- **Export bank...**: every patch you saved, each with its slot number. A tick box adds the factory sounds too (as copies of their values).

A patch you saved goes into the file **whole**, with everything the unit stores: End B, the arp, the pattern, the FM6 patch. A factory sound has no saved record, so only its **values** are copied (the arp block and the pattern are not). The file says which is which.

The file is plain JSON ("duell-patches"). Each patch has its name, engine, type, the unit's record (base64, with a checksum) and a readable list of what differs from the engine's defaults, so you can compare two patches in any text diff tool. You can edit the readable list, but it is not read back: the record is the patch.

## Import

Drop a patch file (or a `.syx` file) anywhere on the PATCHES tab, or press **Import file...**. A preview lists the patches by name, engine and type. Then you choose where they go:

- **The same slot numbers as in the file** (for a bank you exported), or
- **One after the other, from the slot ...** that you type (it starts at the slot you had selected). The tick box "skip slots that hold a saved patch" keeps what you saved.

You can also type a slot for any single patch. Two patches to one slot, or a patch with no slot, stop the import and say why.

The preview shows, slot by slot, what each target holds now ("#12 BELL (saved)") and how many saved patches will be replaced. Nothing is written until you press the button. When four or more slots are written and a saved patch would be lost, the editor first **downloads a safety copy** of those patches (you can untick it). Then it writes one patch at a time with a progress bar and **reads each one back** to check it. If the unit refuses (for example while it is playing) it stops and tells you how far it got. The unit stops playback to write, about a second a patch.

## DX7 voices

Drop a DX7 `.syx` file on the tab: a single voice (155 bytes, "VCED") or a bank of 32 (4104 bytes, "VMEM"). The preview lists the voices by name; pick the ones you want and the slots.

Each voice becomes an **FM6 patch that carries the voice** (the FM6 engine plays the generic 6-operator voice, so the sound is the voice). Our own sections start neutral and gentle: the MULTI FILTER is a 12 dB low-pass at 6.8 kHz with a trace of resonance, the reverb send is 7 %, the other sends are 0, the EQ is flat, polyphonic with the usual level (a bass voice is mono). The patch name is the voice's name, cleaned: capital letters, 12 characters at most. A patch is never named "DX7"; "DX7 sysex import" is only the label of where it came from. The type is a **guess** from the name (BASS, PAD, KEYS ...); change it later if it is wrong.

A damaged voice inside a bank is shown greyed out and left out; the others are read as usual.

The DX7 import was developed against public CC0 (public domain) DX7 banks. Those banks are **not shipped** with DUELL (they are not in the repository or in any package); the automatic tests use small synthetic banks generated in the test code. You can try the import with your own DX7 banks.

## Copy, move and swap

- **Drag** a slot onto another slot in the list to **copy** it (Shift: swap). In the wide view a drag **swaps** the two slots (Alt: copy). Every drag asks first and lists what it replaces.
- Without the mouse: **Copy to...** and **Move to...** on the selected slot. Copy to... takes several selected slots and copies them one after the other from the slot you type.
- A swap of a saved patch with a slot that holds a factory sound moves the patch; the place it left shows its own factory sound again. Moving or swapping never makes the unit forget a factory sound.

## Load into a part without saving

Drop a patch file, a DX7 file, or a slot from the list **on a part's column** (PART 1 or PART 2). The patch is loaded into that part and **not saved** in any slot. A file with several patches asks which one. This sets the sound the way the unit loads a slot: the engine, the parameters (the part's PAN, MUTE and arp settings stay), and an FM6 sound's own patch. The pattern steps and End B of a saved patch are not loaded this way; use the slot (Load) for those.

## Wavetables

On the WAVETABLES tab, drop a WAV file (a single-cycle wave or a wavetable like the ones Serum and Vital export) or a wavetable file on one of the three slots (anywhere else on the tab: the first empty slot). **Export file** saves a table from the unit as a small file ("duell-wavetable", about 1.7 KB) you can share; drop it on a slot of another unit to put it there. A WAV is reduced to 19 frames and 64 harmonics and the phases are not kept (the page says so); a wavetable file holds exactly what a unit stores, so nothing is lost on the way.

## Good to know

- Files from a unit with a newer record form may be refused by an older unit; the unit's own checks decide, and the editor shows the reason.
- A unit before 0.2.20 cannot take a patch whole; the editor hides the file buttons and says so.
- Everything here is beta. Keep a backup (the Backup page) before you write many slots.
