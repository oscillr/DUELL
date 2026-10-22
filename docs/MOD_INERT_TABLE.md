# Modulation that has no effect, per engine (DUELL 0.2.25)

Owner rule: the hide rule applies to modulation depths and destinations too. A control or destination that does nothing for the current selection is not offered.

This table was read from each engine's render code. `tests/mod_inert_test.c` (`./host_tests.sh modinert`) then checked it by rendering: for every engine and every value of the slot that picks its wave, the render with the modulation is bit for bit the same exactly where the table says "no effect", and different everywhere else. 106 checks pass. The engines without a wave slot were rendered with their first sound.

What the unit does with it:
- The SHAPE depth cards (AMP DEST SHP, ENV 2 DEST SHP, LFO 1 DEST SHP, LFO 2 DEST SHP) are hidden, and the cards move left.
- The matrix DEST knob does not stop on SHP, E2SHP or the inert E slots.
- The code is in `mod.c` (`mod_shp_inert`, `mod_e_inert`, `mod_dst_offered`) and `ui_draw.c` (`card_hidden_vp`).
- The editor does the same from `layout.json` `MXH`, which the firmware writes.

A stored route to an inert destination stays and still plays (it just does nothing). The DEST card shows it dim, with the reason WAVE.

## The table

"SHAPE" is the shape input: the four SHP depth cards, the matrix's SHP, and ENV 2 -> SHAPE (E2SHP).

| engine | SHAPE moves | SHAPE has no effect with | E slots with no effect | removed from the DEST list (a list of choices) |
|---|---|---|---|---|
| ANALOG | pulse width (PWM), sync sweep (SYNC) | WAVE SAW, SQR, TRI, SIN, SUB | none | WAVE |
| PHASE | DCW (the distortion depth), every wave | never | slot 8 (unused, as before) | WAVE 1, WAVE 2, LINE |
| LOFI | pulse width around DUTY (PLS) | WAVE TRI, SAW, NOIS, WRAM | DUTY with WAVE TRI, SAW, NOIS (PLS takes its width, WRAM its table) | CHIP, WAVE, ARP |
| VOICE | BUZZ | never | none | none |
| TRIO | PW of the pulse oscillators | WAVE SAW3, SST, TRI3, S&T, NOIS, SYNC, SYNC3, RING, RING3, R+S (no pulse) | PW with no pulse (as SHAPE); INT2 with no oscillator 2 (P+N, NOIS, RING, RING3); INT3 with no oscillator 3 that sounds or rings (NOIS, SYNC, SYNCP); DTN with neither (NOIS) | WAVE, MODE |
| WHEEL | brightness tilt | never | none | none (REG and ROTR stay: the engine ramps them) |
| PHYS | STRC (MODAL, STRING, MEMB), BUZZ (SYMP) | never | none | MODEL |
| NOISE | COLR, every mode | never | none | MODE |
| FM6 | feedback | never | SLOT (it loads a voice, as before) | ALG |
| WAVE | table position (POS) | never | TABLE (a rebuild, as before), slots 7 and 8 (unused, as before) | none |

On every engine the EQ gains (EQLOW, EQMID, EQHI) and, since T0229, REVERB SIZE (RSIZE: the SPRING bends its pitch while SIZE moves) are removed from the DEST list as well.

T0229: four new destinations, the AMP ENV times (AATK, ADEC, ASUS, AREL; shown AMP / ATTACK .. on two lines). They do nothing on FM6 (its operators have their own envelopes): not offered there (reason OWN ENV).

## PITCH and CUTOFF depths

- **CUTOFF** (the FLT depths and the matrix's CUT) always does something.
  - On ANALOG, TRIO and NOISE it moves the engine's own filter. Their multimode filter after it takes only its CUT knob and KEY TRK. On these three engines, MULTI FILTER 2/2 shows KEY TRK alone (since 0.2.21).
  - On the seven other engines it moves the multimode filter.
  - On PHASE it also moves DCW, and on LOFI its TONE.
  - Nothing to hide.
- **PITCH** always does something, with one value-dependent exception: NOISE in mode ANLG or DUST with TRK 0. There the key does not reach FREQ, so pitch modulation has no effect. This is not hidden, because a value is never a reason to hide (owner rule).

## Shown dim, not hidden (the reason is a value)

- **FILTER RES (was FRES) on ANALOG, TRIO and NOISE** (T0229, owner): shown normally, never dimmed or hidden, also while CUT is open (the cutoff must not hide or fade the resonance). The unit shows it as FILTER / RES on two lines, the DEST line says FILTER RESONANCE.
- **VOICE VOWL 2** does nothing while TALK is 0.
- **PHASE LINE** does nothing while DTN is 0 and LINE is MIX.

These are values, not selections, so they stay offered.

## Factory data

- No factory sound has a route to a removed or inert destination.
- **M08 HAZY TENSOR** (the owner's), part 1 #3 LADDER BASS, slot 3: LFO 2 -> ANALOG WAVE, -45 %. ANALOG WAVE is now removed from the list. The route stays and plays as he made it, but the DEST knob no longer offers ANALOG WAVE.
- **M03 PAD SPLIT** (the owner's), part 2 #44 EMBER GLOW (ANALOG, not PWM or SYNC): LFO 2 DEST SHP is 6. That card is hidden now, and its value did nothing before either.
