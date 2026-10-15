# The piano roll in the editor

**DUELL**
MultiMorphing Bitimbral Synthesizer
by OSCILL/R

The web editor has a piano roll for the step pattern of each part. It is new in 0.2.20 beta and untested on the real unit so far (the simulated unit in the editor, `/editor/?mock=1`, is where it was tried). The firmware did not change.

## Where it is
- Each part column has a **PIANO ROLL** tab, after ARP / SEQ.
- The **ROLL** button in that tab opens the same roll across the whole window. There you pick **PART 1**, **PART 2** or **BOTH** (one under the other). **Esc** or **Close** goes back.

## What you see
- Left to right are the steps of the pattern (as many as LEN). A line every 4 steps, a stronger one every 16.
- Up and down is pitch, C1 to C7 (wider if the pattern has notes outside that), with a keyboard on the left. Scroll to see the rest. The **-** and **+** at the top left of the grid make the steps narrower or wider.
- **Scale** (a root and a scale) only colours the rows. The editor cannot read the unit's SCALE and ROOT, so choose them here if you want to see them. Nothing is sent.
- Under the notes is a lane for **VEL**ocity, **CHANCE** or **RATCH**et (pick one), then a row of **ACCENT** buttons and a row of **SLIDE** buttons, one per step.
- LEN, DIV, SWG and GATE are above the grid: the same knobs as the PATTERN page.
- There is no moving playhead. The editor protocol has no command that tells the editor where the sequencer is.

## Mouse
- **Click** an empty cell: a note. Keep the button down and drag right to make it longer.
- **Drag** a note: move it (step and pitch).
- **Drag the right edge** of a note: longer or shorter.
- **Right-click** or **double-click** a note: delete it.
- In a lane, press and drag across the bars to set the value of each step. Only steps that have a note have a bar.

## Keyboard
Click the grid or Tab to it, then:
- **Arrows** move the selected note (up and down: a semitone, left and right: a step). **Shift+Up / Down**: an octave. **Shift+Left / Right**: length.
- With no note selected the arrows move a dashed cursor, and **Enter** adds a note there.
- **Delete** or Backspace removes the note. **Escape** unselects.
- **A** accent, **S** slide, **+** and **-** velocity, **R** ratchet (1 to 4).
- **Ctrl+Z** undo, **Ctrl+Shift+Z** (or Ctrl+Y) redo. The history is kept in the editor, up to 200 edits per part.

## Sending
- **Send live** is on by default: each finished edit (a click, the end of a drag) is written to the unit at once, only the steps that changed. Nothing is written while you are still dragging.
- Switch it off to edit without touching the unit. **Send** writes everything that is waiting, **Revert** throws it away and reads the unit again.
- If the unit does not answer a step, the roll says so and keeps the step as waiting; the next edit sends it again.
- A step you change on the unit's panel shows up in the roll. A step you changed here and have not sent is not overwritten.
- **Copy** takes the pattern (the first LEN steps) and **Paste** puts it over the start of any part's pattern, so you can copy from part 1 to part 2. **Clear** empties the first LEN steps. Undo works on both.

## What the unit's steps can and cannot say
The roll shows the unit's step model as it is, so a few things are not what a normal piano roll does:
- A step holds up to **4 notes**. A fifth is refused with a message.
- A note's **length** is its step plus the TIE steps after it, inside LEN (the unit's own ROLL page works the same). A TIE step has no notes of its own, so **all notes of a chord have the same length**. Making a note longer stops at the next step that has notes, and at LEN.
- **Velocity, accent, slide, chance and ratchet belong to the step**, not to one note of a chord.
- A note starts on a step. There is no note starting between steps (the unit has NUDGE for that, which the roll keeps but does not edit).
- Clicking on the tied part of a long note with another pitch cuts the long note there: the unit cannot keep two notes there.
- A TIE at step 1 that follows a note at the end of the pattern (the unit loops it) is not drawn as a bar.
- Parameter locks and automation are not edited here.

## When a control does not apply
- **GLO > MODULES > SEQ OFF:** the unit plays no step. A line says so; editing still works and still writes to the unit.
- **ARP MODE = SEQ:** the steps are not played by the pattern. They are offsets for the keys you hold (note 60, C4, is no offset), timed by the arp's RATE and GATE. The roll shows one note per step, hides the length, slide, velocity, chance and ratchet, and hides DIV, SWG and GATE (the ARP / SEQ tab keeps them). With **MODULES > ARP OFF** it says that this pattern plays nothing.
- The unit's ARP and SEQ switches are read when the roll opens and again every few seconds.

## Not done yet
- A playhead, audition by clicking the keyboard, nudge and parameter locks in the roll, QNTZ in the roll (it is on the ARP / SEQ tab), more than one note selected at once.
