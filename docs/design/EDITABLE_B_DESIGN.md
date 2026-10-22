# EDITABLE B: design study for the MORPH's B end (for your decision; nothing is built)

**Built (cloud job G, branch cloud/feat-editb):** the fader at 127 makes every sound page edit End B; the hand-set list (up to 32) is stored in the patch record (extension block section 2), the multi and the project (PROJ_REV 7 also carries End B's 7-byte block per part, so a power cycle keeps B); the recommended answers 1 to 5 are the behaviour; FM6 operator bytes are not in the list in version 1 (they move both ends, the line says OPS: SAME ON A AND B). Code: morph.c (morph_b_mode, morph_b_set, morph_edits, morph_pin_load), ui_input.c (edit_param, knob_reset), ui_draw.c (head_ab).

Written 2026-10-19. Research and design only: no code was changed (when this was written). Every claim is marked **CONFIRMED** (I read it in the code or the documents) or **HYPOTHESIS** (my reasoning, not checked). Numbers marked "est." are estimates.

**Words used here** (plain meanings; GLOSSARY.md is not changed by this study):
- **Original**: the sound as you load and edit it: the patch itself. End A is the Original unless you ROLL A.
- **End A / End B**: the two ends of the MORPH fader (0 and 127).
- **Seed**: a number that makes the same random roll every time. Today End B is saved as its seed, not as values.
- **Record**: one saved patch in the unit's memory. **Chunk**: a block of flash memory that holds 8 records.
- **Extension block**: the planned extra section of the patch record for values that do not fit the 127 parameter numbers (TWIN FILTER, SUB, CYCLE). Planned in HEADLINE_FEATURES.md "stage A"; EXTENSION_BLOCK_PLAN.md did not exist yet when I finished.
- **Hand-set value**: a value you turned yourself at the B end. This study calls it "set by hand" (in code: "pinned").
- **Cross-fade**: both versions play for a moment, one fading out, one fading in, so a switch does not click.

---

## Verdict (half a page)

**You already designed this, on 2026-10-13, and your design is the right one.** Your words, after I listed "make the B end editable" among your questions:

> "Okay, so the B editable, it's very good. The way to we should have the have it the clear indicator. Is that next to the one, two on top, we should have a AB uh, marking also. On the top left, and where it normally shows the A, but if somebody which I think the only place you should be able to tweak the B is when you are at the B end, so until then, whatever if you edit, I think it should be editing the A, and then uh, the B if you don't have anything against it." (2026-10-13, 11:01)

> "I would take the multi number from the top left corner, and add A or B there, depending on which is the one you are currently editing, if you tweak a knob." (2026-10-13, 20:42)

**My recommendation: build exactly that, with five details settled.** Here is what it means for you:
- **The fader decides which end you edit.** Fader at 127 means every sound page edits B. Anywhere else, edits go to A, as today.
- **You can see it.** The letter A or B sits next to the 1 2 badges. The screen colour already turns into the B colour at the B end (CONFIRMED, `gfx.c` part_theme follows the fader).
- **You can always hear what you edit.** At the B end you hear B alone. So you cannot edit the end you are not hearing. That is why this beats every other option I compared (section 2).
- **How B is kept.** Saved B = today's seed (the rolled part) plus a short list of the values you set by hand, up to 32 values. That costs 1 to 65 bytes a patch, so it needs the larger patch record of stage A.
- **What it costs.** The sound engine already holds End B as a full set of values in RAM (CONFIRMED). So editing B costs **no CPU while playing**, about 0.3 to 0.5 KB of RAM and 1.5 to 2.5 KB of code (est.).
- **Effort:** 2 to 3 rounds plus one session on your unit (est.).

**What I advise against for the first version:**
- **SNAPSHOT** changes A, not B. It is a different feature, and close to the KEEP you removed.
- **B FROM SLOT** cannot honestly give "the other sound". The morph never moves the engine, FM6's algorithm and ratios, the wave table, the insert type and similar values. A "B from slot" would only be a likeness of that sound. It would also overflow the 32-value list.
- **A lock switch, a counter, a hold-a-key recording gesture**: section 2 explains why.

**One finding you should know about, whatever you decide.** Today a saved End B is re-made from its seed every time it loads. Helpers have changed the roll rules many times, and every time some B ends changed: dozens of factory B seeds were re-picked between 0.2.5 and 0.2.11 (CONFIRMED, CHANGELOG and reports). From the alpha on, your saves must stay stable. So either the roll rules freeze at the alpha (new values draw from their own random sequence, which the code already does: `MF_DRAW`, the separate streams rs3 / rs4), or B must be stored as values. The values you set by hand are immune either way, because they are stored as values.

**The cheapest test costs nothing and works on your unit today** (section 5). Use the matrix's MORPH source with AMOUNT 0: that gives you a hand-designed B for up to 8 values. Try it on two or three sounds you pick. You will learn whether designing B ends is worth it, and how many values a designed B really touches.

---

## 1. How B works today (all CONFIRMED in `morph.c`, `upreset.c`, `multi.c`, SPEC 3.1 to 3.5 unless marked)

**Runtime: a full second copy, not a difference.**
- Each part keeps the Original (`base`, 127 values), and for each end a history of 9 full copies of all 127 values. The current end is the top entry; the 8 below are what UNDO walks back to (`h[2][9][127]`).
- On FM6, each end also holds 65 bytes of the FM6 patch (operator levels, envelope rates and levels, pitch envelope, LFO).
- RAM for the morph today: about 5 KB a part, about 10 KB for both (est., summed from `morph_part_t`).
- So the sound engine never knows whether B came from a roll. It just blends two full value sets. **This is why editable B costs nothing while playing.**

**How a roll makes B** (SPEC 3.2, 3.3):
- **Continuous values** (levels, times, cutoffs, depths): the Original plus a random step of 1 up to AMOUNT % of the range, with a random sign. A move that would leave the range goes the other way.
- **Choices** (LFO wave, oscillator wave, filter mode): AMOUNT % chance of a different allowed choice.
- **Smart rules**: no dead filter, levels only rise a little, nothing that is off gets switched on (except at half the chance for the filter, DIST, sends and LFO depths), pitch stays untouched unless PITCH is ON, insert type never changes, and so on.
- "Zero stays zero" is now "zero is not a lock" (rule 3e). Only the real on/off switches stay off.

**How the fader blends each kind of value** (`morph_apply`):

| Kind | Examples | What the fader does |
|---|---|---|
| Continuous | ATK, CUT, RES, sends, LFO depths, engine knobs | A straight line from A to B. Times and cutoffs blend in their musical scale (octaves, not Hz). |
| Faded choice | LFO WAVE, ANALOG WAVE (first five), filter TYPE, PHASE WAVE, TRIO MODE, WHEEL REG, multimode SLOPE | Both versions run, cross-faded along the fader. |
| Matrix slot | SRC, DST, AMOUNT | Same route at both ends: the amount blends. A different route: A's fades out over the first half, B's fades in over the second (WILD only). |
| Step (WILD only) | a hard switch that differs | Flips at the middle of the fader. |
| Never moved | engine, PATCH LEVEL, voice mode, octave and transpose (PITCH OFF), arp on/off, insert TYPE, WAVE TABLE, PHYS MODEL | Always A's value. |
| FM6 patch bytes | 65 of them | Straight line, blended into a copy each block; the stored patch is never touched. |

**Your edits on top of the morph (today):**
- A knob always edits the Original, at any fader position.
- For a continuous value, the morph adds its offset on top: what you hear is your value plus (blend minus Original). So **an edit at the B end today moves A and B together.** You cannot change B alone. That is exactly the gap this feature closes.
- A choice you changed by hand is "yours" and stops fading.
- The cards show what you hear at the current fader, with a small mark where the morph moves the value (`ui_draw.c card_vv`). **At the B end, the marks already show where B differs from A.**

**Saving, loading, the controls:**
- **SAVE x2 is gone** (replaced 2026-10-11). Holding SAVE asks "OVERWRITE #NN ...?" and writes End A (the Original) plus End B's block.
- **End B's block is 7 bytes:** a mark, the seed (2 bytes), the AMOUNT it was rolled with, the roll settings (SMART/WILD, ARP, PITCH, stereo, part), the AMOUNT knob, a check byte.
- On load, B is rolled again from that seed (`morph_rebase_part`). Before saving, `morph_blk_make` re-rolls B from the seed if you edited A since. So what a load will give is what you hear.
- **Bytes for B today:** 7 a patch record, 7 a part in a multi. The autosave keeps none: `morph_pack` exists but nothing calls it.
- **Loading always puts the fader at A.** B is the saved one, or for a shipped sound its own seed and AMOUNT, or else a fresh roll.
- **UNDO:** "far end" is retired. UNDO names its end like ROLL does (twist left = A, right = B), with 8 steps per end. Replacing an end you are hearing glides over 50 ms. A changed choice "dips" (its end's weight goes to 0 and back), so nothing clicks.
- **MORPH SET** holds SMART/WILD, ARP and PITCH (device settings, not the patch's), and K4 is empty.
- **The MORPH page is HOME's first page:** K1 fader, K2 ROLL, K3 UNDO, K4 AMOUNT.
- The fader is also MIDI CC 20 and the matrix source MORPH.

---

## 2. Designing the B edit

### 2.1 The six ways compared

| Way | Clarity | Risk of editing the wrong end | UI cost | Label law fit | Verdict |
|---|---|---|---|---|---|
| **(a) Edit B in place; the fader at the B end is the switch** (your design) | High: you hear the end you edit; the letter and the screen colour say it | **Lowest**: you can only edit B while hearing B alone | Smallest: one letter in the header, no new knob | Good: every knob keeps its label; the fader already means "A to B" | **Recommended** |
| (a') The same with a separate A/B toggle (e.g. MORPH SET's empty K4) | Medium | **High**: edit B while hearing A, or forget the toggle is on B | One knob plus a state to remember | A knob on one page changes what every other page does | Alternative 1 (not advised) |
| (b) SNAPSHOT: the sound in between becomes A (or B) | High for its own job | Medium: it replaces a whole end at once (UNDO needed) | One action | Fine | Not editable B; park |
| (c) Per-value edits with a visible count ("B EDITS 12/32") | Medium | Same as its host way | A counter on screen | You dislike counts (the patch list has none) | Keep the limit, show it only when reached |
| (d) Copy a value or a group A to B, or B FROM SLOT | Medium | Low | Needs a picker for values or slots | Fine | Not in v1: "from slot" cannot copy what the morph never moves, and it overflows the 32-value list. Editor-only later at most |
| (e) Lock A or B | Low (one more state) | Low | A switch | One more hidden mode | Not needed: (a) makes the wrong end unreachable |
| (f) Turn knobs while holding a key = write to B | Low | Medium | No knob cost | **Breaks it**: keys play notes | Rejected |

**Why (a) beats the others: you hear the end you edit.**
- At fader 127, B's weight is exactly 100 % (`morph_wb` returns 32768 at 127; CONFIRMED). Every edit is heard as it is, with no blend.
- Anywhere below 127 you are editing A, as every synth player expects. The marker says A.
- Every other way lets you change an end you are not hearing.

**The one weak spot of (a): the middle of the fader.**
- At fader 64 a knob edits A, but you hear half of the change. The card shows the blended value, which moves by about half as much as the knob.
- That is today's behaviour too, and the letter "A" makes it honest.
- I would not grey anything out there: A is still being edited, and you hear part of it.

### 2.2 The recommended interaction, button by button (a proposal; you decide)

**On the unit**

1. **HOME (MORPH page): turn K1 fully right.** The fader is at B (127) and the screen turns into the part's B colour (that already happens today). On the panel, the fader thumb reads "B".
2. **Go to any sound page** (EDIT, ENV, LFO, FX part pages, the quick hold layers). The header shows **1 2 B**: the letter in a small filled cushion, in B's colour, right of the badges (where the multi number used to be; it was removed in 0.2.10, so the place is free: CONFIRMED `ui_draw.c` header notes).
3. **Turn K1 to K4.** Only B changes. A stays exactly as it was. The cards show B's values; a card where B now differs from A carries the morph mark that already exists.
4. **Values the morph never moves** (engine, octave/transpose with PITCH OFF, voice mode, arp on/off, insert TYPE, WAVE TABLE, PHYS MODEL, PATCH LEVEL): see question 4. My recommendation: the knob still works, changes the sound at both ends, and the message line says `SAME ON A AND B` once.
5. **Back to HOME, turn K1 left.** You hear A blend into your B.
6. **UNDO B** (K3 twist right): takes back all your hand edits since you last arrived at the B end, in one step; the next UNDO B goes back to the roll before. So the 8-step history stays one list. Each stay at the B end with edits counts as one entry.
7. **ROLL B** (K2 twist right): a new random B. My recommendation: it replaces your hand edits too, and UNDO B brings them back (question 2).
8. **Hold SAVE, then OCT+**: saves A, the B seed and the values you set by hand. On load the fader is at A and B is back exactly.
9. **At the 33rd hand-set value**, the knob does not move and the message line says `B FULL: 32 VALUES` (question 5). Changing a value that is already set by hand never counts again.

**The header letter: when it shows (proposal).**
- On the sound pages, whenever the part has a live morph (an End B that differs from A).
- **A** in the A colour, **B** in the B colour.
- It does not show on the global pages (GLO, SCALE, MENU, the SAVE pages). It does not show on MORPH, MORPH SET and PERFORM either, where the fader itself is drawn.
- Your 20:42 words say "if you tweak a knob". I recommend showing it all the time on the sound pages, so you see B *before* the first turn (question 3).
- **Layout check (HYPOTHESIS):** the page title is centred in the room between the badges and the battery. A 12 px letter takes that room away from long titles, which then drop to the small font, as already happens when space runs out. This needs a layout test before you see it.

**What does NOT follow the fader (recommendation).**
- MIDI CCs to sound values (CC 74, 71 ...), MIDI LEARN, automation playback and recording, and the web editor's value writes **always edit A**. They cannot see the letter. Automation playing at the B end would otherwise rewrite B on every step.
- At the B end, a knob turn is not recorded as automation. The message line says `B: NOT RECORDED` once. (Risk list, section 4.)

**In the web editor** (later; it needs two new protocol commands, est.)
- An **A | B switch** in each part's PATCH block header: "SHOWING: A | B".
- With B shown, every card shows B's value. A card where B differs from A carries the same morph mark as on the unit. A value you set by hand gets a filled mark, a rolled one an outline mark (a diff view).
- First version: B is shown but not edited. The editor reads B from the saved record (PATCH_RAW already carries End B's block whole: CONFIRMED EDITOR_PROTOCOL.md). Editing B from the editor comes in a later round.

### 2.3 The two alternatives (only if you dislike the recommendation)
- **Alternative 1: an EDIT END switch** (A / B) on MORPH SET's empty K4 column, independent of the fader. Pro: you can edit B with the fader anywhere. Con: you can edit B while hearing A, and a forgotten B setting silently edits the wrong end. Your own rule rejects that.
- **Alternative 2: no editing, only B FROM SLOT** (B taken from another saved sound of the same engine). Pro: no change to how pages behave. Con: it is not your request; what the morph cannot move stays A's, so "B" is only a likeness of that sound; and it overflows a 32-value list.

---

## 3. Storage

### 3.1 Two ways to keep an edited B

| | **Seed + hand-set list** (recommended) | **B as values** (every value where B differs from A) |
|---|---|---|
| What is saved | today's 7 bytes + 1 count byte + 2 bytes per hand-set value (number, value) | a map of which values differ (16 bytes) + 1 byte each, + FM6: a 9-byte map + up to 65 bytes |
| Typical patch | +3 to +31 bytes (1 to 15 hand values; est.) | +55 to +90 bytes (a rolled B moves about 40 to 70 values at AMOUNT 15; est.) |
| Worst case | +65 bytes (32 values) | about +217 bytes (all values plus all FM6 bytes) |
| Survives firmware updates | hand-set values: yes; the rolled rest: only if the roll rules stay frozen from the alpha | yes, fully |
| Fits the larger record (about 596 bytes, 6 a chunk, stage A) | yes: 434 + 65 + about 35 for the extension block = about 534 (est.) | **no**: 434 + 217 + 35 = about 686, over 596; even 5 a chunk would need 26 chunks, and only 23 exist (est.) |

The "rolled B + up to 32 hand edits" idea of the first helpers is the right base. Two corrections to it:
- the hand edits are **values**, not offsets (question 1);
- the FM6 operator bytes must be editable too, so the list needs a second number range for them (65 bytes) next to the 127 parameters and the extension values.

**Bytes and limits** (CONFIRMED inputs, my arithmetic):
- **Today's room is not enough.** The biggest record is 434 bytes. 8 records share a chunk of 3,584 bytes with a 4-byte header, so a record can be at most 447 bytes: only 13 bytes of room. That is 6 hand-set values at most, too few to be useful. **Editable B depends on stage A's larger record** (6 a chunk, about 596 bytes, est. in HEADLINE_FEATURES.md).
- **Multis** hold both parts' End B block (7 bytes each today). With the list, a multi grows by up to 2 x 65 = 130 bytes. A multi is at most 1,916 bytes, and the worst one today (two FM6 parts, 64 steps each) takes 1,775. Automation gets what is left, so the worst multi keeps 3 automation events instead of 46. The existing `AUTOMATION CUT` message handles this. A normal multi is not affected.
- **The autosave** (what comes back after power-off) keeps no morph block today (CONFIRMED: `morph_pack` has no caller). Your hand-set B must survive a power cycle, so the autosave must carry the list too.
  - HYPOTHESIS, to check first: today a *saved* patch's B may come back as a fresh roll after a power cycle, because the power-on path does not seem to pass the record's block to the morph. If so, that is a small bug today and a real loss with hand-set B.

### 3.2 Rules for a B that is partly rolled and partly set by hand (proposal)
- **You set one value at the B end:** that value becomes "set by hand" and keeps exactly what you set. Every other value of B stays rolled from the seed.
- **You edit A afterwards:**
  - the rolled values of B follow A, as today (B stays a variation of A);
  - the values you set by hand stay where you set them (question 1).
  - Saving re-makes the rolled part from the seed (today's `morph_blk_make`) and puts your hand-set values back on top. Without that step, the save would lose them (a coding trap, section 4).
- **ROLL B:** new seed, and the hand-set values go (UNDO B brings them back; question 2). **AMOUNT** changes nothing until the next ROLL, as today.
- **UNDO B down to the Original, then edits by hand:** B is the Original plus your values, with no seed. Today "seed 0" means "no B saved, roll a fresh one at load", so a saved "seed 0 + list" must be read as a valid B (a small format rule).
- **The smart rules (never silent, cutoff floors, "zero stays zero") apply to rolls only.** A value you set by hand is what you chose; you hear it at the B end before you save.
- **Which values B may differ in:**
  - every continuous value, faded choice and matrix amount;
  - the new switches that already have a planned short cross-fade (TWIN TYPE, ROUTE, LINK; SUB OCTAVE and WAVE; CYCLE MODE; WAVE STEPS OFF to a count), which switch at the middle of the fader;
  - everything else is shared by both ends (question 4).

### 3.3 Shipped sounds' B ends (you design them; I never author sounds)
- Today each shipped sound has a seed and an AMOUNT in the sound table (`tools/duell_sounds.py` bseed / bamt; CONFIRMED in reports). For sounds you pick, add the list of values you set by hand.
- **Your workflow (proposal):**
  1. Load the sound and go to the B end.
  2. Shape it.
  3. Hold SAVE into a slot.
  4. Export the slot (PATCH_RAW / backup).
  5. A small tool copies the record's B list into the sound table.
- **One rule for helpers:** a sound with a B you designed is never given a new seed by the loudness audit's seed search. Helpers did that dozens of times for rolled B ends. The audit reports it to you instead.

### 3.4 With the new features (TWIN FILTER, SUB, CYCLE, STEPS)
- Their values live in the extension block, and SUB_AND_CYCLE_DESIGN.md already asks for them to get places in the part's RAM value array after the 127 parameters. The morph's arrays grow by the same amount: about 18 bytes per extra value per part (9 history copies x 2 ends; est.). The hand-set list must be able to name them.
- **Continuous values blend:** OFFSET, RES 2, DRIVE, BLEND and the four amounts (TWIN); SUB LEVEL and HARMONICS; CYCLE RISE, FALL, CURVE; STEPS between two counts (rounded).
- **Type switches change at the middle of the fader with their planned short cross-fade:**
  - TWIN TYPE, ROUTE, LINK: 8 blocks, 6 ms (TWIN_FILTER_DESIGN_2.md 3.5);
  - SUB OCTAVE, WAVE: 4 blocks (SUB_AND_CYCLE_DESIGN.md);
  - CYCLE MODE;
  - STEPS OFF to a count.
  - CYCLE's own SYNC switch: shared by both ends, as ENV SYNC is today (proposal).
- **Old patches:** no conversion. You have said that no backward compatibility is needed before the alpha. A record with only today's 7-byte block loads as "rolled B, nothing set by hand". The B list format must be in the **alpha** format, together with the extension block. Adding it later would need a conversion of saves.

---

## 4. Cost and risks

| Item | Estimate | Basis |
|---|---|---|
| CPU while playing | **0** | The blend already reads B as full values (CONFIRMED). |
| CPU at an edit | one rebuild of the morph's list per knob step, with interrupts off: tens of microseconds (est.) | `morph_build` runs locked (CONFIRMED); it can be made to update only the one value |
| RAM | +0.3 to 0.5 KB, ordinary RAM, not hot RAM (est.): a "set by hand" mark per value per B history entry (about 29 bytes x 9 x 2 parts) | struct sizes |
| Code (flash) | +1.5 to 2.5 KB (est.) | edit routing, list save/load, header letter, UNDO, messages |
| Patch bytes | +1 to +65 a patch (typical 3 to 31); +up to 130 a multi; the autosave the same | section 3 |
| Effort | 2 to 3 local rounds + 1 session on your unit (est.); after stage A | |

**Risks:**
1. **Editing the wrong end.** It is mostly removed by the fader rule, the letter and the B colour. What remains: a MIDI controller sends CC 20 = 127 while you edit, and edits then go to B. The letter changes to B, so it is visible. A turn that is already running could keep its end for half a second (small extra).
2. **Switches at the middle.** A B set by hand on a switch makes a jump at 50 %. Only the switches with a planned short cross-fade may differ. Everything else stays shared (question 4), so SMART never gets a cliff at the middle from hand edits of old switches.
3. **Clicks while editing B.** No new risk. At the B end an edit is like an edit at A today. The 50 ms ramp is only needed when a whole end is replaced (ROLL, UNDO), and that already exists.
4. **Saving loses the hand-set values**, if the existing "re-make B from the seed before saving" runs without putting them back. A test must check: edit A, edit B, save, load, compare.
5. **B ends drift after the alpha** (the finding in the verdict). The roll rules must freeze for existing values from the alpha on, with a test that rolls stored seeds and compares them to the alpha's results. Values set by hand are immune.
6. **Two undo systems.** The EDIT layer's OCT- undo of loads and the morph's UNDO A / B. Hand edits of B belong to UNDO B only. The edit undo must never put A back over a B edit. Test both orders.
7. **Power cycle** (section 3.1): the autosave must carry the list, and today's possible loss of a saved B after power-on must be checked first.

**What suits the cloud sessions** (all of it testable on a computer, no unit needed): the store logic, edit routing, save/load round trips, power-cut tests and the frozen-seed test. **What stays local:** the header letter on the unit, the feel of the B end on the hardware, and your session.

---

## 5. The cheapest ways to try it before building

1. **Zero code, on your unit today (recommended first).** This is a "designed B" for up to 8 values with what exists:
   1. Load a sound you pick.
   2. Set AMOUNT (MORPH K4) to 0 and ROLL B (K2 right). B is now practically A. (HYPOTHESIS: the smart rules might still nudge a value by a step.)
   3. In the matrix, route the source **MORPH** to the values you want B to change (CUT, RES, the envelope or LFO depths, and whatever else the destination list offers), each with the amount that sounds right at fader 127.
   4. Hold SAVE: it keeps the routes and the AMOUNT-0 B.

   It works because the fader feeds the matrix even when nothing was rolled (CONFIRMED: `morph_begin` sets the source before anything else). Try two or three sounds. It answers the two questions that decide the size of this feature: do you enjoy designing B ends, and how many values does a designed B touch? (If it is usually under 10, the 32-value list is plenty.)
2. **A paper walkthrough.** The button sequence of section 2.2, written out for 3 sounds you name, step by step: what you press, what the header shows, what you hear. It costs one short read and finds confusing steps before any code exists.
3. **Editor-only trial** (web editor code only, no unit firmware, 1 round est.). The editor keeps an A and a B set of values for one part and has its own A to B slider. It sends the blended values to the unit as ordinary value changes, with the unit's own fader left at A. That gives a feel for designed B ends with the editor's mouse. Its limits: no FM6 operator bytes, and a slower, steppier blend than the real morph.
4. **A Mac render** of A, a designed B and three in-between positions as WAV files. Useful for demos, but it does not test the gestures. Last choice.

---

## 6. Questions for you (each with my recommendation)

1. **You set B's cutoff by hand, then you go back to A and change A's cutoff. Should B's cutoff stay where you set it, or move along with A?** *Recommended: it stays where you set it.* A value you set by hand is part of the B sound you designed. The values the roll made still move along with A, as today.
2. **ROLL B after you set values by hand: should it replace everything, your hand-set values too, or keep your values and re-roll only the rest?** *Recommended: replace everything.* ROLL then always means "a new random B", and UNDO B brings your values back.
3. **The A / B letter next to the 1 2 badges: always shown on the sound pages, or only after you turn a knob (as you said on 13 October)?** *Recommended: always shown on the sound pages while the morph is live*, so you see "B" before the first turn. It is hidden on the MORPH, MORPH SET, PERFORM and global pages.
4. **At the B end, what happens to a knob for something the morph never changes (engine, octave, voice mode, arp on/off, insert type, wave table)?** *Recommended: it keeps working and changes the sound at both ends, with the message "SAME ON A AND B" once.* The other way is to hide those cards at the B end, but that would make pages change shape when you move the fader.
5. **How many values may you set by hand on one B?** *Recommended: 32 per patch.* At the 33rd new value the knob does not move and the screen says "B FULL: 32 VALUES". There is no counter on screen otherwise. More (48) costs about 32 more bytes in every worst-case patch record.

(SNAPSHOT and B FROM SLOT are left out of the first version on purpose. If you want either later, it is a separate small decision.)

---

## Evidence

| What | Where | Status |
|---|---|---|
| Your editable-B wording (fader at B, A/B mark by the badges) | transcript 2026-10-13 11:01:33 (session 3c42d9f5) | CONFIRMED (quoted) |
| Your header wording (A or B instead of the multi number) | transcript 2026-10-13 20:42:19 | CONFIRMED (quoted) |
| My 10:50 proposal you answered (B stored as values, record size) | transcript 2026-10-13 10:50:22 | CONFIRMED |
| The decisions as recorded | memory `project-duell-fm1-firmware.md` (OWNER DECISIONS 2026-10-13, two entries); `project-headline-features-direction.md` ("his own earlier request") | CONFIRMED |
| Your original MORPH wording (A and B ends, one percentage, save to the patch) | transcript 2026-10-10 07:03:19 | CONFIRMED |
| Runtime: Original + 9 full copies per end, FM6 65 bytes per end | `morph.c` `morph_part_t` (lines ~986-1013) | CONFIRMED |
| Blend per kind; B weight exactly 100 % at 127 | `morph.c` `morph_apply`, `morph_wb` | CONFIRMED |
| Edits at any fader position move the Original; ends follow by offset | `morph.c` header notes, `morph_apply` ("p + v - base"), `morph_fold` | CONFIRMED |
| End B block 7 bytes; re-roll before save; load re-rolls from seed | `morph.c` `MB_N`, `morph_blk_make`, `morph_rebase_part`; `upreset.c` `UP_MB_N`, `morph_ld` | CONFIRMED |
| UNDO of the load's B returns to the Original; seed 0 = no block | `morph.c` `morph_rebase_part`, `morph_undo_part`, `morph_blk_make` | CONFIRMED |
| Matrix MORPH source works with nothing rolled | `morph.c` `morph_begin` (t->mf set first) | CONFIRMED |
| AMOUNT 0 gives no continuous or choice moves | `morph.c` `morph_cont`, `morph_move` (maxoff 0) | CONFIRMED (the smart rules' extra nudges: HYPOTHESIS) |
| Record 434 bytes worst, 8 a chunk, 3,584-byte chunk, so 447 max | `upreset.c` `PT_W_SEQ`, `PT_CAP`, `PT_K`; SPEC 3.6 | CONFIRMED (arithmetic mine) |
| Larger record 6 a chunk, about 596 bytes, 23 chunks possible | HEADLINE_FEATURES.md section 2 | est. there |
| Multi: 7-byte B block per part, 1,916 max, worst 1,775 | `multi.c` header comment, `MU_PART_FIX` | CONFIRMED |
| Autosave keeps no morph block | `morph_pack` has no caller outside `morph.c` | CONFIRMED |
| Saved patch's B after power-on may be a fresh roll | power-on path not traced | HYPOTHESIS |
| Header left slot free since 0.2.10 | `ui_draw.c` header comment | CONFIRMED |
| Screen colour follows the fader A to B | `gfx.c` `part_theme`, `theme_follow` | CONFIRMED |
| Cards show the heard value and mark moved values | `ui_draw.c` `card_vv` | CONFIRMED |
| No MORPH command in the editor protocol; PATCH_RAW carries End B | `web/editor/layout.js` lines 217-218; `web/EDITOR_PROTOCOL.md` | CONFIRMED |
| New features' morph rules (middle switch, short cross-fades) | TWIN_FILTER_DESIGN_2.md 3.5; SUB_AND_CYCLE_DESIGN.md | CONFIRMED there (designs, not built) |
| Many B seeds re-picked after rule changes | CHANGELOG.md; reports of 2026-10-12 to 10-14 in the transcript | CONFIRMED |
| RAM, code, CPU-at-edit and typical-byte numbers | my estimates from struct sizes and roll rules | est. |
| EXTENSION_BLOCK_PLAN.md | not present when I finished | not read |
