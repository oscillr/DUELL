# Refine every patch and multi, and keep the result

**DUELL**
MultiMorphing Bitimbral Synthesizer
by OSCILL/R

This is the owner's way to go through all 128 patches and 16 multis in the web editor, change what he wants, and make his versions the **factory set**: after it, RESTORE on the unit brings his versions back, and every new firmware contains them. Nothing here changes the firmware by itself; he hands two files to the developer, who imports them (step 6).

## Before you start

- Use a unit with DUELL 0.2.19 or newer and the editor (`editor/index.html`, Chrome). Connect it.
- Make a backup first: the **Backup** page (`editor/backup.html`), **Back up to a file**. Keep that file. It is your way back.
- Stop playback on the unit while you save (the editor and the unit say so).

## 1. Refine a patch

1. In the editor, **PATCHES** tab: click the slot (for example #44 EMBER GLOW), then drop it on **PART 1** or **PART 2**, or use the unit's own PATCHES page to load it.
2. Change it on the ENGINE, MOD, FX and the other tabs, or on the unit. Listen. The MORPH End B you get is saved with it.
3. Save it **into the same slot number**: select the slot in the PATCHES tab and press **Save...** under "Save a part's sound in #NN" (pick the part and keep the name, or type a new name of up to 12 characters). On the unit it is SAVE held, then OCT+, over the same slot.
4. Go on with the next slot. A slot you never save stays the factory sound, which is fine.

The slot number matters: slot #44 is the factory sound #44. Saving a patch into another slot makes a copy there; the importer refuses it (see "What is not taken").

## 2. Refine a multi

1. **MULTIS** tab: **Load** the multi, change the music of both parts, the tempo, the effects, the arps.
2. **Save here...** into the same slot (M01 to M16). The unit stops playback and writes it.
3. The multi keeps its own copy of the sounds' values, so changing a patch does not change a multi, and the other way round. Refine both if you want both.

## 3. Take the files out

Do this when you want to hand in your work (once at the end, or after each session; each file holds everything you saved so far).

1. **PATCHES** tab, **Export bank...**: a file `duell-bank-NN-YYYY-MM-DD.duell-patches.json` appears in your Downloads. It holds every patch you saved, each with its slot number. (Leave the box "factory sounds too" unticked.)
2. **Backup** page, **Back up to a file**: a file `duell-backup-YYYY-MM-DD-HHMM.json` appears in your Downloads. It holds the saved patches again and, what the first file cannot, **your saved multis**.

## 4. Hand them over

Put both files in the project folder `DUELL/owner_edits/` (make it if it is not there) or attach them in the chat, and say "import my edits". Keep the files; do not edit them.

## 5. What happens next (the developer)

`felucca-base/tools/import_owner_edits.py` reads both files, compares every saved patch with its factory sound and every saved multi with its factory record, and writes only what is different:

- `felucca-base/tools/owner_sounds.py`: your patches, as the factory data (each replaces the factory sound of its slot);
- `felucca-base/tools/owner_multis.py` and `felucca-base/tools/factory_multis/Mnn.bin`: your multis, byte for byte.

Then the build, the tests and the sound audit run. **Your values are never changed to pass a test.** If the audit finds something (for example a patch much quieter than the others, or one that clips), the developer tells you which patch and what the audit says, and you decide.

## 6. The result

- A new firmware build contains your versions as the factory set.
- **RESTORE** (one patch) and **RESTORE ALL** in the PATCHES tab bring back your versions, not the old ones. ERASE on a multi brings back your multi.
- On a unit that already holds your saved copies, nothing changes: the saved copy and the new factory sound are the same.

## Undo

- On the unit, before the import: **Restore...** in the PATCHES tab (one slot), **Restore all...** (every slot), ERASE on a multi. Or restore your first backup file on the Backup page.
- After the import: tell the developer which slot to take back. The import is plain files in the project (git); one patch goes back by dropping its entry from `owner_sounds.py`, or by saving the old sound and importing again (an entry equal to the old sound is dropped by itself). Everything goes back by deleting the two `owner_*.py` files and restoring the multi records with git.

## What is not taken (the importer says so, line by line)

- A patch saved in another slot than its own number, with another **engine**, or with a **name another sound already has**: refused with the reason. Tell the developer if you want a sound replaced by another engine's.
- A **new name** for a patch is taken, but multis name their sounds, so a multi that uses a renamed sound needs one more line from the developer. Tell him when you rename.
- The **FM6 operator data** (the 6-operator voice itself) and **SEQUENCER steps** saved in a patch are not part of the factory sound list: the values around them are taken, the voice and the steps are not (yet).
- Factory sounds you did not save are only a copy of their values in an export; the importer skips them.

## What the editor lacks today (proposals, not built)

- **Export multis:** only the Backup page carries them. A MULTIS tab button **Export all** (one `duell-multis` file with each record, base64 and checksum, like the patch file) would remove the need for the Backup file, and **Import file...** the other way round.
- **Export bank** only holds patches you saved; a tick "only the ones that differ from the factory sound" would show at a glance what you changed.
- **One button "Export my edits"** that writes the patch bank and the multis together into one file, named with the date.
- **A "changed" mark** in the PATCHES list and the MULTIS list for slots that differ from the factory (the editor knows: no saved record, no mark).
