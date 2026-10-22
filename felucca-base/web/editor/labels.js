// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the editor's own words for every control (0.2.25, owner: "we do not save characters in the editor: spell things out").
// ONE table to revise. The unit keeps its short labels (layout.json comes from the firmware: tests/editordump.c); this file only changes
// what the EDITOR shows: the label of a control (by parameter id), the names of its values (same count, same order: only the words),
// a plain tooltip, the titles of the cards and tabs, and the reasons shown where a control is hidden or locked.
// No DOM. test_editor.mjs checks that every parameter on a page has a long label and a tooltip, and that a value list keeps its length.

// ---- part parameters (the firmware's TP[] ids): [long label, tooltip]
export const PART = {
  0: ["LEVEL", "The part's volume in the mix (the MIXER block; the unit's GLO > MIXER). Not part of the patch: a sound load keeps it; a multi stores it."],
  1: ["ATTACK", "Time from the key press to full level."],
  2: ["DECAY", "Time from full level down to the sustain level."],
  3: ["SUSTAIN", "Level held while the key is down."],
  4: ["RELEASE", "Time to fade out after the key is let go."],
  5: ["FILTER DEPTH", "How far the amp envelope moves the filter cutoff (negative closes it). Not shown in the editor (the filter envelope does this job)."],
  6: ["PITCH DEPTH", "How far the amp envelope moves the pitch."],
  7: ["SHAPE DEPTH", "How far the amp envelope moves the engine's tone shape."],
  8: ["LOW GAIN", "Equalizer: boost or cut of the low band."],
  9: ["RATE", "Speed of LFO 1 (hidden while TEMPO SYNC is on)."],
  10: ["WAVEFORM", "Shape of LFO 1."],
  11: ["START PHASE", "Where in its cycle LFO 1 starts when it is restarted by a note."],
  12: ["FADE IN", "Time for LFO 1 to reach its full depth after a note starts."],
  13: ["PITCH DEPTH", "How far LFO 1 moves the pitch (vibrato)."],
  14: ["LFO 1", "How far LFO 1 moves the filter cutoff."],
  15: ["SHAPE DEPTH", "How far LFO 1 moves the engine's tone shape."],
  16: ["AMP DEPTH", "How far LFO 1 moves the volume (tremolo)."],
  17: ["MODE", "Arpeggiator pattern. OFF plays the keys as they are; SEQUENCE plays the step pattern as offsets of the held keys."],
  18: ["RATE", "Arpeggiator speed as a note value of the tempo."],
  19: ["OCTAVES", "How many octaves the arpeggio covers."],
  20: ["GATE", "Length of each arpeggio note."],
  21: ["SWING", "Delays every second arpeggio note."],
  22: ["CHANCE", "Chance that each arpeggio note plays."],
  23: ["HOLD", "Latch: ARP keeps the arpeggio running after the keys are let go; ALL also holds the keys with the arpeggiator off."],
  24: ["NOTE ORDER", "Order of the held notes: by pitch or as they were played."],
  25: ["RESONANCE", "TWIN FILTER: emphasis at its cutoff. On COMB it is FEEDBACK: how long the comb rings (negative values ring with the odd harmonics)."],   // (cloud job B: the id of the retired ROOT is the TWIN FILTER's RES 2)
  26: ["DRIVE", "Soft overload of the sound between the two filters (in PARALLEL before both). OFF is not run at all."],            // (the retired SCALE's id)
  27: ["KEY TRACK", "How much the multimode filter cutoff follows the played note (+100 %: one octave per octave)."],
  28: ["TRANSPOSE", "Shifts the chord keys in semitones."],
  29: ["LENGTH", "Number of steps the pattern plays (1 to 64)."],
  30: ["DIVISION", "Length of one step as a note value of the tempo."],
  31: ["SWING", "Delays every second step of the pattern."],
  32: ["GATE", "Length of each step's note."],
  33: ["DRIVE", "Warm distortion of the part before its effects. 0 is off."],
  34: ["CHORUS", "How much of the part goes to the chorus (shared by both parts)."],
  35: ["DELAY", "How much of the part goes to the delay (shared by both parts)."],
  36: ["REVERB", "How much of the part goes to the reverb (shared by both parts)."],
  37: ["MODE", "POLY plays chords; MONO one note, every note restarts the envelope; LEGATO one note, held notes glide without a restart; UNISON one note on several detuned voices."],
  38: ["GLIDE", "Time or speed of the slide from one note's pitch to the next. 0 is off."],
  39: ["PAN", "Position of the part between left and right (the MIXER block). Not part of the patch."],
  40: ["MUTE", "Silences the part (the MIXER block). Not part of the patch."],
  41: ["GLIDE TYPE", "RATE: the same speed for every interval (wide jumps take longer). TIME: the same time for every interval."],
  42: ["PRIORITY", "Which held key sounds when one note plays at a time: the LAST pressed, the LOWEST or the HIGHEST."],
  43: ["TYPE", "TWIN FILTER's type: OFF leaves the sound as MULTI FILTER makes it; LOW-PASS, BAND-PASS, HIGH-PASS and NOTCH are TRIO's filter, LADDER a 24 dB low-pass, COMB a row of peaks tuned to the note (not on WAVE and PHYS)."],
  44: ["UNISON DETUNE", "How far the unison voices are detuned against each other."],
  45: ["CUTOFF", "Cutoff of the multimode filter (16 kHz with LOW-PASS is fully open: the filter is bypassed)."],
  46: ["RESONANCE", "Emphasis at the cutoff."],
  47: ["MODE", "Low-pass, band-pass, high-pass and every blend between."],
  48: ["SLOPE", "Steepness of the filter: 12 or 24 dB per octave."],
  73: ["ATTACK", "Filter envelope: time to full level."],
  74: ["DECAY", "Filter envelope: time down to the sustain level."],
  75: ["SUSTAIN", "Filter envelope: level held while the key is down."],
  76: ["RELEASE", "Filter envelope: time back to zero after the key is let go."],
  77: ["RATE", "Speed of LFO 2 (hidden while TEMPO SYNC is on)."],
  78: ["WAVEFORM", "Shape of LFO 2."],
  79: ["START PHASE", "Where in its cycle LFO 2 starts when a note restarts it."],
  80: ["FADE IN", "Time for LFO 2 to reach its full depth."],
  81: ["TYPE", "The chord one key plays."],
  82: ["VOICING", "How the chord's notes are spread."],
  83: ["TEMPO SYNC", "LFO 2's speed as a note value of the tempo; OFF uses RATE."],
  84: ["TRIGGER", "EVERY NOTE restarts LFO 2 at its start phase; FREE RUNNING never restarts it."],
  85: ["POLARITY", "BIPOLAR swings both ways around the setting; UNIPOLAR only upwards."],
  86: ["TARGET", "Which oscillators or operators the arpeggiator plays (the others stay on the held note)."],
  87: ["LOW FREQUENCY", "Equalizer: corner of the low band."],
  88: ["MID GAIN", "Equalizer: boost or cut of the middle band."],
  89: ["MID FREQUENCY", "Equalizer: centre of the middle band."],
  90: ["MID Q", "Equalizer: width of the middle band (higher is narrower)."],
  91: ["TEMPO SYNC", "LFO 1's speed as a note value of the tempo; OFF uses RATE."],
  92: ["TRIGGER", "EVERY NOTE restarts LFO 1 at its start phase; FREE RUNNING never restarts it."],
  93: ["POLARITY", "BIPOLAR swings both ways around the setting; UNIPOLAR only upwards."],
  94: ["QUANTIZE", "Live recording snaps notes to the steps."],
  95: ["SPREAD", "Spreads the voices left and right around PAN (POLY and UNISON only)."],
  96: ["TYPE", "The part's own effect. OFF bypasses it."],
  97: ["INSERT A", "The insert's first control: its name and meaning follow INSERT TYPE (for example DRIVE, BITS, RATE)."],
  98: ["INSERT B", "The insert's second control: its name follows INSERT TYPE (for example TONE, RATE, DEPTH)."],
  99: ["INSERT C", "The insert's third control: its name follows INSERT TYPE (for example LEVEL, LOW-PASS, FEEDBACK)."],
  100: ["MIX", "Dry / effect balance of the insert."],
  101: ["BLEND", "The mix of the two filters: the centre is BOTH, the left end MULTI FILTER alone, the right end TWIN FILTER alone; in between the name and how far from BOTH."],
  102: ["TEMPO SYNC", "ON: attack, decay and release of both envelopes (amp and filter) are note values of the tempo."],
  103: ["HIGH GAIN", "Equalizer: boost or cut of the high band."],
  104: ["HIGH FREQUENCY", "Equalizer: corner of the high band."],
  105: ["CURVE", "Shape of the ducking's release."],
  106: ["CUTOFF", "TWIN FILTER's cutoff. With LINK on it is OFFSET: how many whole semitones above or below MULTI FILTER's cutoff it sits (-36 to +36, three octaves). On COMB it is TUNE: semitones around the played note."],
  107: ["DEPTH", "How far the part is turned down when the ducking triggers."],
  108: ["ATTACK", "How fast the part is turned down."],
  109: ["PATCH LEVEL", "The sound's own loudness, saved with the patch (the mixer's LEVEL is the player's)."],
  110: ["RELEASE", "How fast the part comes back up."],
  111: ["SOURCE", "What turns the part down: a tempo PUMP, the OTHER PART's level, a MIDI NOTE, or OFF."],
  112: ["RATE", "Speed of the tempo pump as a note value."],
  113: ["THRESHOLD", "Level of the other part that starts the ducking."],
  114: ["FILTER ENV", "How far the filter envelope moves the cutoff."],
  115: ["SHAPE DEPTH", "How far the filter envelope moves the engine's tone shape."],
  116: ["PITCH DEPTH", "How far LFO 2 moves the pitch."],
  117: ["LFO 2", "How far LFO 2 moves the filter cutoff."],
  118: ["SHAPE DEPTH", "How far LFO 2 moves the engine's tone shape."],
};
// the modulation matrix: SRCn, DSTn, AMTn (ids 49..72)
for (let k = 0; k < 8; k++) {
  const b = 49 + 3 * k;
  PART[b] = [`SOURCE ${k + 1}`, "What moves the destination (an LFO, an envelope, velocity, a controller ...)."];
  PART[b + 1] = [`DESTINATION ${k + 1}`, "What the source moves."];
  PART[b + 2] = [`AMOUNT ${k + 1}`, "How far, and which way, the source moves the destination."];
}
// the extension slots (cloud job I; protocol scope 2, firmware ext.h / params.c XTP): [long label, tooltip] by slot number. The card's title carries the context (TWIN FILTER, SUB OSC, CYCLE ENV), so a
// control never says TWIN or FILTER again (naming rule R1); FILTER ENV is the one name with the word, as the owner wrote it for CUTOFF MOD
export const XPART = {
  0: ["ROUTE", "SERIES: the sound goes through MULTI FILTER and then TWIN FILTER. PARALLEL: both filters at once, mixed by BLEND (DRIVE comes before both)."],
  1: ["LINK", "ON: TWIN FILTER's cutoff follows MULTI FILTER's, OFFSET is the distance between them. OFF: TWIN FILTER has its own CUTOFF."],
  2: ["KEY TRACK", "How much TWIN FILTER's cutoff follows the played note (+100 %: one octave per octave)."],
  3: ["FILTER ENV", "How far the filter envelope moves TWIN FILTER's cutoff (on COMB its tuning)."],
  4: ["LFO 1", "How far LFO 1 moves TWIN FILTER's cutoff (on COMB its tuning)."],
  5: ["LFO 2", "How far LFO 2 moves TWIN FILTER's cutoff (on COMB its tuning)."],
  6: ["LEVEL", "Volume of the sub oscillator. 0 switches it off."],
  7: ["OCTAVE", "How many octaves below the played note the sub oscillator sounds."],
  8: ["WAVE", "Wave of the sub oscillator."],
  9: ["HARMONICS", "Adds harmonics to the sub oscillator (0 is a pure wave)."],
  10: ["MODE", "LOOP: the CYCLE envelope rises and falls again and again while the key is held. ONCE: one rise and fall per note."],
  11: ["RISE", "Time for the CYCLE envelope to rise (a note value with SYNC on)."],
  12: ["FALL", "Time for the CYCLE envelope to fall (a note value with SYNC on)."],
  13: ["CURVE", "Shape of the rise and fall: negative bends one way, positive the other, 0 is straight."],
  14: ["SYNC", "ON: RISE and FALL are note values of the tempo."],
};
// the value names of the slots (the firmware's short names -> the editor's words)
export const XVALUE = { 0: { PARAL: "PARALLEL" }, 8: { TRI: "TRIANGLE" } };
// the TWIN FILTER's CUTOFF 2 and RES 2 carry the name of the meaning in use (layout.js twDesc): the firmware's short label -> the editor's
export const TWIN_LONG = { OFFSET: "OFFSET", CUTOFF: "CUTOFF", TUNE: "TUNE", RES: "RESONANCE", FDBK: "FEEDBACK" };
// the INSERT's three controls follow its TYPE: their short labels (INS[]) -> long ones
export const INSERT_LONG = { "INS A": "INSERT A", "INS B": "INSERT B", "INS C": "INSERT C", DRIVE: "DRIVE", TONE: "TONE", LEVEL: "LEVEL", BITS: "BITS", RATE: "RATE", LPF: "LOW-PASS", DEPTH: "DEPTH", FDBK: "FEEDBACK", WOW: "WOW", SAT: "SATURATION", DUST: "DUST", NOISE: "NOISE", WARP: "WARP" };
// the delay's DEPTH card is CHANCE in RANDOM and OCTAVE UP (its own descriptor, label "CHANCE"): one name at a time, never both (0.2.25 layout round, owner)
export const DMD_CHANCE = ["CHANCE", "The chance that a clock edge fires an event (RANDOM and OCTAVE UP)."];
// labels the descriptor changes with ENVELOPE SYNC (L.ESYNC): note values, same meaning
export const ESYNC_LONG = { ATK: "ATTACK", DEC: "DECAY", REL: "RELEASE", ATK2: "ATTACK", DEC2: "DECAY", REL2: "RELEASE" };

// ---- global parameters (GP[] ids)
export const GLOBAL = {
  0: ["TEMPO", "Tempo of the instrument (beats per minute). Hidden while an external clock leads."],
  1: ["SWING", "Swing of the whole instrument."],
  2: ["SOURCE", "Where the tempo comes from: INTERNAL, USB, TRS MIDI, or AUTO (the first that arrives)."],
  3: ["MASTER TUNE", "Pitch of A4 for the whole instrument."],
  4: ["TIME", "Delay time as a note value of the tempo."],
  5: ["FEEDBACK", "How long the repeats go on."],
  6: ["COLOR", "Tone of the repeats (darker or brighter)."],
  7: ["MIX", "Level of the delay's return."],
  8: ["SIZE", "Size of the reverb's room."],
  9: ["DAMPING", "How fast the high frequencies of the reverb die away."],
  10: ["RATE", "Speed of the chorus."],
  11: ["DEPTH", "Depth of the chorus."],
  12: ["MOD RATE", "Speed of the delay's own modulation."],
  13: ["MOD DEPTH", "Depth of the delay's own modulation. In RANDOM and OCTAVE UP this control is CHANCE: the chance of an event."],
  14: ["ROUTING", "OWN CHANNEL: each part listens on its own channel. SELECTED PART: every channel plays the selected part."],
  15: ["MODE", "DIGITAL, PING-PONG, RANDOM or OCTAVE UP repeats."],
  17: ["DRIFT", "Slow drift of the shimmer's pitch."],
  21: ["LOW CUT", "Removes the low end from the reverb's input."],
  24: ["TYPE", "The reverb's model."],
  25: ["SPREAD", "How far the PING-PONG repeats go left and right."],
  26: ["PITCH", "Pitch of the shimmered return: an octave up or down."],
};
// the MASTER stages and the SIDECHAIN setup (MENU settings, layout.json L.MASTER / L.COMP / L.MSTG / L.SIDECH)
export const MASTER = {
  BMONO: ["BASS MONO", "Below this frequency both channels play the same (a tight, centred bass). OFF passes the stereo as it is."],
  THRESH: ["THRESHOLD", "Level above which the compressor turns the mix down."],
  RATIO: ["RATIO", "How strongly the compressor turns the mix down. OFF (its lowest value) switches the compressor off; turning it up switches it on."],
  ATTACK: ["ATTACK", "How fast the compressor reacts."],
  RELEASE: ["RELEASE", "How fast the compressor lets go."],
  WIDTH: ["WIDTH", "100 % is the mix as it is; less narrows it towards mono, more widens it (only above 125 Hz)."],
  CH: ["CHANNEL", "MIDI channel that starts this part's ducking when its DUCK SOURCE is MIDI NOTE."],
  NOTE: ["NOTE", "MIDI note that starts this part's ducking when its DUCK SOURCE is MIDI NOTE."],
};

// ---- engine controls: by the engine's short label (the same short label means the same thing on every engine, except the few by engine below)
const ENGINE_COMMON = {
  WAVE: "WAVEFORM", WAVE2: "WAVEFORM 2", DTN: "DETUNE", MIX: "MIX", NOIS: "NOISE", CUT: "CUTOFF", RES: "RESONANCE", DRV: "DRIVE", KTR: "KEY TRACK",
  SYNC: "SYNC", SUB: "SUB", STEPS: "STEPS", DCW: "SHAPING", ENV: "SHAPING ENV", LINE: "LINE MODE", BITS: "MODEL", DUTY: "PULSE WIDTH",
  CRSH: "BIT CRUSH", SWP: "PITCH SWEEP", VIB: "VIBRATO", ARP: "ARPEGGIO", TONE: "TONE", "WAV#": "WAVE NUMBER", DCY: "DECAY", VOWL: "VOWEL", VOWL2: "VOWEL 2", "VOWL 1": "VOWEL 1", "VOWL 2": "VOWEL 2",
  TALK: "TALK SPEED", SHIFT: "FORMANT SHIFT", BUZZ: "BUZZ", BRTH: "BREATH", Q: "RESONANCE", RAND: "RANDOM", INT2: "INTERVAL 2", INT3: "INTERVAL 3", MODE: "MODE",
  PW: "PULSE WIDTH", REG: "REGISTRATION", BODY: "BODY", TOP: "TOP", PERC: "PERCUSSION", CLICK: "KEY CLICK", ROTR: "ROTARY SPEAKER", MODEL: "MODEL", STRC: "STRUCTURE",
  BRIT: "BRIGHTNESS", DAMP: "DAMPING", POS: "POSITION", ACC: "ACCENT", BOW: "BOW", EXC: "EXCITATION", HARM: "HARMONICS", BEND: "BEND", CHRD: "CHORD", SYMP: "SYMPATHETIC STRINGS",
  COLR: "COLOR", FREQ: "FREQUENCY", TRK: "KEY TRACK", DENS: "DENSITY", DRFT: "DRIFT", LEN: "LENGTH", CLK: "CLOCK", ALG: "ALGORITHM", FB: "FEEDBACK",
  MLVL: "MOD LEVEL", MRAT: "MOD RATIO", MEG: "MOD ENV", VMOD: "VELOCITY MOD", DTUN: "DETUNE", SLOT: "SOURCE", TABLE: "TABLE",
};
const ENGINE_BY = {
  WAVE: { POS: "POSITION", BEND: "PHASE BEND", SYNC: "HARD SYNC" },
  PHYS: { POS: "STRIKE POSITION" },
  PHASE: { WAVE: "WAVE 1", WAVE2: "WAVE 2", "WAVE 1": "WAVE 1", "WAVE 2": "WAVE 2" },   // (owner, 0.2.25 evening: WAVE 1 and WAVE 2, as the unit)
  TRIO: { MODE: "FILTER MODE" },                       // (T0229: the one engine card holds the oscillators and the filter: MODE alone would not say whose)
  FM6: { SLOT: "SOURCE" },
};
const ENGINE_TIP = {
  FM6: {
    ALG: "Algorithm: 0 uses the voice's own algorithm, 1 to 32 another one (how the six operators are connected).",
    FB: "Offset on the voice's feedback.", MLVL: "Offset on the output level of the modulator operators (brightness).",
    MRAT: "Offset on the frequency ratio of the modulator operators.", MEG: "Offset on the envelope times of the modulator operators.",
    VMOD: "How much velocity moves the modulator levels.", DTUN: "Detune of the operators against each other.",
    SLOT: "Where this part's FM6 voice comes from: a factory voice (F1 to F11), the part's OWN voice (edited in the ENGINE tab's OPERATORS editor or on the unit, saved with the patch), or a voice of the bank (B1 to B32).",
  },
  WAVE: { TABLE: "The wavetable this part plays: a factory table, or USER 1 to 3 (your own, imported from a WAV file on the WAVETABLES tab; an empty one is marked EMPTY).",
          STEPS: "Rounds the scan position POS to 2 to 19 places across the table, so it steps from frame to frame instead of gliding. OFF is the smooth scan." },
};
export function engineLong(engName, short) { return ENGINE_BY[engName]?.[short] ?? ENGINE_COMMON[short] ?? short; }
export function engineTip(engName, short) { return ENGINE_TIP[engName]?.[short] ?? `${engName} engine: ${engineLong(engName, short).toLowerCase()}.`; }

// ---- value names: [short, long] by parameter id (part) or "g<id>" (global); the long list keeps the order and the length of the firmware's
const WAVES = { SIN: "SINE", TRI: "TRIANGLE", SAW: "SAW", SQR: "SQUARE", "S&H": "SAMPLE AND HOLD" };
const TRIGS = { NOTE: "EVERY NOTE", FREE: "FREE RUNNING" };
const POLS = { BI: "BIPOLAR", UNI: "UNIPOLAR" };
export const VALUE_NAMES = {
  10: WAVES, 78: WAVES, 84: TRIGS, 92: TRIGS, 85: POLS, 93: POLS,
  17: { DN: "DOWN", UPDN: "UP AND DOWN", RND: "RANDOM", ORD: "AS PLAYED", DNUP: "DOWN AND UP", "UP+8": "UP PLUS OCTAVE", CONV: "CONVERGE", DIVG: "DIVERGE", SEQ: "SEQUENCE" },
  24: { NOTE: "BY PITCH", PLAY: "AS PLAYED" },
  37: { LEG: "LEGATO", UNI: "UNISON" },
  42: { LOW: "LOWEST", HIGH: "HIGHEST" },
  43: { LP: "LOW-PASS", BP: "BAND-PASS", HP: "HIGH-PASS" },
  48: { "12dB": "12 dB", "24dB": "24 dB" },
  81: { DIA3: "DIATONIC TRIAD", DIA7: "DIATONIC SEVENTH", MAJ: "MAJOR", MIN: "MINOR", DOM7: "DOMINANT 7TH", MAJ7: "MAJOR 7TH", MIN7: "MINOR 7TH", SUS4: "SUSPENDED 4TH", POW: "POWER CHORD" },
  82: { INV1: "FIRST INVERSION", INV2: "SECOND INVERSION", "+OCT": "ROOT OCTAVE DOWN" },
  96: { SOFT: "SOFT CLIP", HARD: "HARD CLIP", FOLD: "WAVEFOLDER", CRUSH: "BIT CRUSHER", PHASR: "PHASER", FLANG: "FLANGER", CHOR: "CHORUS" },
  105: { LIN: "LINEAR", EXP: "EXPONENTIAL" },
  111: { PART: "OTHER PART", MIDI: "MIDI NOTE", PUMP: "TEMPO PUMP" },
  // the matrix lists (owner, T0229: release_drafts/MATRIX_LIST_PREVIEW.md, word for word; the groups: MATRIX_GROUPS below)
  src: { LFO1: "LFO 1", AENV: "AMP ENV", VEL: "VELOCITY", KEY: "KEY", RAND: "RANDOM", MODW: "MOD WHEEL", AT: "AFTERTOUCH", EXPR: "EXPRESSION", "S&H": "SAMPLE AND HOLD", SLEW: "SAMPLE AND HOLD SLEW", ENV2: "FILTER ENV", LFO2: "LFO 2", MORPH: "MORPH", DUCK: "DUCK", CYCLE: "CYCLE" },
  dst: { PITCH: "PITCH", VIB: "LFO 1 PITCH DEPTH", CUT: "MULTI FILTER CUTOFF", FRES: "MULTI FILTER RESONANCE", FMODE: "MULTI FILTER MODE", E2FLT: "FILTER ENV AMOUNT", AMP: "AMP LEVEL", TRIM: "PATCH LEVEL", PAN: "PAN",
    SHP: "SHAPE", E2SHP: "FILTER ENV SHAPE AMOUNT", SAT: "SATURATION DRIVE", "INS A": "INSERT A", "INS B": "INSERT B", "INS C": "INSERT C", IMIX: "INSERT MIX", DUCK: "DUCK DEPTH",
    EQLOW: "EQ LOW GAIN", EQMID: "EQ MID GAIN", EQHI: "EQ HIGH GAIN", CHO: "CHORUS SEND", DLY: "DELAY SEND", REV: "REVERB SEND", CRT: "CHORUS RATE", CDP: "CHORUS DEPTH",
    FDBK: "DELAY FEEDBACK", DMDEP: "DELAY MOD DEPTH", DMRAT: "DELAY MOD RATE", RSIZE: "REVERB SIZE", RDAMP: "REVERB DAMPING", RAT1: "LFO 1 RATE", DEP1: "LFO 1 LEVEL",
    RAT2: "LFO 2 RATE", DEP2: "LFO 2 LEVEL", AATK: "AMP ENV ATTACK", ADEC: "AMP ENV DECAY", ASUS: "AMP ENV SUSTAIN", AREL: "AMP ENV RELEASE", ATK2: "FILTER ENV ATTACK", DEC2: "FILTER ENV DECAY", SUS2: "FILTER ENV SUSTAIN", REL2: "FILTER ENV RELEASE",
    CUT2: "TWIN FILTER CUTOFF", FRES2: "TWIN FILTER RESONANCE", FDRV: "FILTER DRIVE", FBLND: "FILTER BLEND", SUBLV: "SUB LEVEL" },
  g2: { INT: "INTERNAL", TRS: "TRS MIDI" },
  g14: { "OWN CH": "OWN CHANNEL", SELECTED: "SELECTED PART" },
  g15: { DIGI: "DIGITAL", PING: "PING-PONG", RAND: "RANDOM", "OCT+": "OCTAVE UP", "OCT UP": "OCTAVE UP" },
  g24: { SHIM: "SHIMMER", RESO: "RESONATOR" },
  wave: { USR1: "USER 1", USR2: "USER 2", USR3: "USER 3", SAWSQ: "SAW TO SQUARE", HARM: "HARMONICS", HOOVR: "HOOVER", WOBBL: "WOBBLE" },
};
// note values (DIV, arp RATE, PUMP RATE, the LFOs' TEMPO SYNC, the delay TIME)
const NOTES = { "8T": "1/8 TRIPLET", "16T": "1/16 TRIPLET", "4T": "1/4 TRIPLET", "2T": "1/2 TRIPLET", "32T": "1/32 TRIPLET", "2D": "1/2 DOTTED", "4D": "1/4 DOTTED", "8D": "1/8 DOTTED", "16D": "1/16 DOTTED", "2BAR": "2 BARS", "4BAR": "4 BARS", "1/1": "1 BAR" };
for (const k of [18, 30, 112, 83, 91, "g4"]) VALUE_NAMES[k] = { ...(VALUE_NAMES[k] || {}), ...NOTES };
VALUE_NAMES[86] = { OP1: "OPERATOR 1", OP2: "OPERATOR 2", OP3: "OPERATOR 3", OP4: "OPERATOR 4", OP5: "OPERATOR 5", OP6: "OPERATOR 6", OSC1: "OSCILLATOR 1", OSC2: "OSCILLATOR 2", OSC3: "OSCILLATOR 3", A: "OSCILLATOR A", B: "OSCILLATOR B" };
// engine value names (the engines' own lists): only plain abbreviations, the sound names stay as the unit has them
export const ENGINE_VALUES = { SIN: "SINE", TRI: "TRIANGLE", SQR: "SQUARE", PLS: "PULSE", NOIS: "NOISE", ANLG: "ANALOG", STRNG: "STRING", MEMB: "MEMBRANE", SYMP: "SYMPATHETIC", MELLO: "MELLOW", HOLLW: "HOLLOW", SMOOT: "SMOOTH", GOSPL: "GOSPEL", "2ND": "2ND HARMONIC", "3RD": "3RD HARMONIC", "2SOFT": "2ND SOFT", "3SOFT": "3RD SOFT", "2SLOW": "2ND SLOW", "3SLOW": "3RD SLOW" };
// the unit's MENU names (SETTINGS tab), where the unit abbreviates
export const MENU_LONG = { "KNOB ACCEL": "KNOB ACCELERATION", "COUNT-IN": "COUNT-IN", "PERF LEAVE": "LEAVING PERFORM", "ARP": "ARPEGGIATOR", "SEQ": "SEQUENCER", "PART FX": "PART EFFECTS", "SCALE LEDS": "SCALE LIGHTS", "LEDS": "LIGHTS" };
const SRC_IDS = new Set([49, 52, 55, 58, 61, 64, 67, 70]), DST_IDS = new Set([50, 53, 56, 59, 62, 65, 68, 71]);
export const valueMapOf = (key) => (typeof key === "number" && SRC_IDS.has(key) ? VALUE_NAMES.src : typeof key === "number" && DST_IDS.has(key) ? VALUE_NAMES.dst : VALUE_NAMES[key]);
export const longValue = (key, short) => valueMapOf(key)?.[short] ?? short;

// ---- the descriptor as the editor shows it: the same object for the same input (a Cell is rebuilt only when its descriptor object changes)
const memo = new WeakMap();
// d: the firmware's descriptor; key: the part id (number), "g<id>" for a global, "m:<label>" for a MASTER / SIDECHAIN card, "e:<ENGINE>" for an engine slot; label: an override
export function longDesc(d, key, label) {
  if (!d) return d;
  let m = memo.get(d); if (!m) memo.set(d, (m = new Map()));
  const k = `${key}|${label ?? ""}`;
  let r = m.get(k);
  if (!r) {
    const base = typeof key === "number" ? PART[key]?.[0] : typeof key === "string" && key[0] === "g" ? GLOBAL[+key.slice(1)]?.[0] : null;
    let long = label ?? base ?? d.label;
    if (key === "g13" && d.label === "CHANCE") long = label ?? DMD_CHANCE[0];   // (the delay's DEPTH id in RANDOM / OCT UP)
    if (typeof key === "number" && key >= 97 && key <= 99) long = INSERT_LONG[d.label] ?? d.label;   // (the INSERT's A B C follow its TYPE)
    if (typeof key === "number" && ESYNC_LONG[d.label] && PART[key]) long = PART[key][0];
    if (typeof key === "string" && key.startsWith("m:")) long = label ?? MASTER[key.slice(2)]?.[0] ?? d.label;
    if (typeof key === "string" && key.startsWith("e:")) long = label ?? engineLong(key.slice(2), d.label);
    if (typeof key === "string" && /^x\d+$/.test(key)) long = label ?? XPART[+key.slice(1)]?.[0] ?? d.label;
    if ((key === 25 || key === 106) && TWIN_LONG[d.label]) long = label ?? TWIN_LONG[d.label];
    const vm = typeof key === "string" && key.startsWith("e:") ? (key === "e:WAVE" ? { ...ENGINE_VALUES, ...VALUE_NAMES.wave } : ENGINE_VALUES) : typeof key === "string" && /^x\d+$/.test(key) ? XVALUE[+key.slice(1)] : valueMapOf(key);
    const names = d.names && vm ? d.names.map((n) => vm[n] ?? n) : d.names;
    r = { ...d, label: long, short: d.label, names };
    if (key === 101) r.blend = true;                                      // id 101 is the TWIN BLEND (P_BLEND): MULTI / BOTH / TWIN, not -100 / +98 %
    if (key === 26) r.zeroText = "OFF";                                   // id 26 is the TWIN DRIVE (firmware P_DRV2): 0 is not run at all, the control says OFF there
    if (key === "e:WAVE" && d.label === "STEPS" && d.names) { r.texts = d.names; r.names = null; r.order = null; }   // STEPS: OFF, 2 .. 19 as a slider with words, not a list
    m.set(k, r);
  }
  return r;
}
export function tipOf(key, d) {
  if (typeof key === "number") return PART[key]?.[1] ?? "";
  if (key === "g13" && d?.short === "CHANCE") return DMD_CHANCE[1];
  if (typeof key === "string" && key[0] === "g") return GLOBAL[+key.slice(1)]?.[1] ?? "";
  if (typeof key === "string" && key.startsWith("m:")) return MASTER[key.slice(2)]?.[1] ?? "";
  if (typeof key === "string" && key.startsWith("e:")) return engineTip(key.slice(2), d?.short ?? d?.label);
  if (typeof key === "string" && /^x\d+$/.test(key)) return XPART[+key.slice(1)]?.[1] ?? "";
  return "";
}

// ---- why a control is hidden (the codes of layout.js inertReason / ginert and the editor's own rules) -> a plain line
export const WHY = {
  "INSERT OFF": "INSERT TYPE is OFF", "SYNC ON": "TEMPO SYNC is on: the speed is a note value", "OWN ENV": "FM6: each operator has its own envelope (OPERATORS editor)",
  "DUCK OFF": "DUCK SOURCE is OFF", UNUSED: "this engine does not use it", "NO ENGINE": "no engine",
  "SEQ ARP": "the arpeggiator's SEQUENCE mode times the steps", "RATIO OFF": "RATIO is OFF: the compressor is off",
  "NOT IN THIS FIRMWARE": "this unit's firmware does not have it", DIGI: "not in DIGITAL mode", RAND: "not in RANDOM mode",
  "OCT+": "not in OCTAVE UP mode", "INS OFF": "INSERT TYPE is OFF", ENGINE: "this engine has its own filter",
  REBUILD: "read only when the table is rebuilt", LOADS: "loads a voice: it cannot be modulated",
  WAVE: "the selected wave does not use it",
  "VOICE POLY": "only with MONO, LEGATO or UNISON", "VOICE ONE NOTE": "only with POLY",
  "GLIDE REUSES": "GLIDE above 0 always takes the nearest voice", "VOICE NOT UNISON": "only with UNISON",
  "GLIDE 0": "only when GLIDE is above 0", "VOICE ONE VOICE": "only with POLY or UNISON",
  "MODULES ARP": "ARP is OFF in SETTINGS > MODULES", "MODULES SEQ": "SEQ is OFF in SETTINGS > MODULES", "MODULES SENDS": "SENDS is OFF in SETTINGS > MODULES",
  "MODULES PART FX": "PART FX is OFF in SETTINGS > MODULES",
  "TWIN OFF": "the second filter's TYPE is OFF", "TWIN COMB": "the comb follows the played note by itself",
};
const REVERB_TYPES = ["ROOM", "SPRING", "HALL", "SHIM", "RESO", "PLATE"];
export const whyText = (code) => (!code || code === "UNUSED" ? "" : WHY[code] ?? (REVERB_TYPES.includes(code) ? `only with REVERB TYPE SHIMMER (it is ${longValue("g24", code)})` : code));

// ---- titles
export const ENGINE_TITLES = { OSC: "OSCILLATORS", FLT: "FILTER", PHS: "PHASE", LINE: "LINE", LOFI: "LOFI", MOTN: "MOTION", VOWL: "VOWEL", TONE: "TONE", BARS: "DRAWBARS",
  BODY: "BODY", EXCT: "EXCITER", SRC: "SOURCE", MOVE: "MOVEMENT", PATCH: "VOICE MACROS", MACRO: "MORE MACROS", TABLE: "WAVETABLE" };
// T0227 (owner): ONE card for all of an engine's own controls (E1..E8), named after what the engine is; the unit splits them over its ENGINE / EDIT pages, the editor does not
export const ENGINE_CARD = { ANALOG: "OSCILLATORS AND FILTER", PHASE: "PHASE DISTORTION", LOFI: "LOFI", VOICE: "SPEECH", TRIO: "OSCILLATORS AND FILTER", WHEEL: "DRAWBARS AND TONE",
  PHYS: "BODY AND EXCITER", NOISE: "NOISE SOURCE", FM6: "MACROS", WAVE: "WAVETABLE" };
export const TAB = { engine: "ENGINE", filter: "FILTER", envelope: "ENVELOPE", lfo: "LFO", matrix: "MATRIX", fx: "FX", arpseq: "ARP", roll: "SEQ", global: "GLOBAL",
  patches: "PATCHES", multis: "MULTIS", patterns: "PATTERNS", midi: "MIDI", tables: "WAVETABLES", settings: "SETTINGS" };
// the matrix drop-downs' groups (owner, T0229: MATRIX_LIST_PREVIEW.md): [heading, [stored values]] in the list's order; "" is no heading (OFF). The stored numbers stay;
// a value that is not offered (layout.js dstOffered) is left out of its group, except the one a slot holds (a stored route keeps its name)
export const MATRIX_GROUPS = {
  src: [["", [0]], ["LFO", [1, 12, 9, 10]], ["ENVELOPE", [2, 11, 15]], ["PLAYING", [3, 4, 5, 6, 7, 8]], ["OTHER", [13, 14]]],
  dst: [["", [0]], ["PITCH", [1, 11]], ["FILTER", [2, 38, 46, 36, 51, 52, 53, 54]], ["AMP", [4, 23, 5]], ["ENGINE", [12, 13, 14, 15, 16, 17, 18, 19, 3, 37, 55]],
    ["EFFECTS: THIS PART", [6, 24, 25, 26, 27, 31, 28, 29, 30]], ["EFFECTS: SENDS", [7, 8, 9]], ["EFFECTS: SHARED", [39, 40, 41, 42, 43, 45, 44]],
    ["LFO", [10, 20, 21, 22]], ["ENVELOPE TIMES", [47, 48, 49, 50, 32, 33, 34, 35]]],
};
export const CATEGORY_NAMES = ["NONE", "BASS", "LEAD", "PAD", "PLUCK", "KEYS", "PERCUSSION", "EFFECTS", "OTHER"];   // the unit's CAT_NAME (category.c: -, BASS .. PERC, FX, OTHER), spelled out
