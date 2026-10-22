#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL factory multis (0.2.23; 0.2.24: the owner's multis back; 0.2.25: COLD CHAPEL in M13): the ONE source of the sixteen factory multis M01..M16 and the tool that builds them.

    tools/factory_multis.py build      the records tools/factory_multis/M01.bin..M16.bin (needs build/gen from DUELL/gen_only.py)
    tools/factory_multis.py check      the records against this list (version, name, sounds, MODE, split) and the similarity check
    tools/factory_multis.py list       the list as text (slot, name, mode, parts '#NN NAME', owner / mine)
    tools/factory_multis.py selftest   the similarity check catches each kind of pair (and lets an owner pair through, reported)

How a record is made: MULTIS below says, per slot, the two parts' factory sounds (by name), what the multi sets on top of each sound
(raw parameter values, core.h P_* ids; an engine EDIT value as "E:LABEL", e.g. "E:CUT" on an ANALOG part), the SEQUENCER arp patterns,
End B, the globals (core.h G_* ids), the output MODE, BALANCE, XFADE and SPLIT. `build` writes that as a C table (build/gen_mm/factory_multis_def.h)
and runs tests/gen_factory_multis.c, which loads each sound with the firmware's own code (set_engine_of + apply_preset_to, as PATCHES loads it),
applies the table and saves with multi.c's packer (project_save_as): the records are byte for byte what a unit saves (version MU_VER 7; a kept record of an older version is converted by tools/multi_upgrade.py). A slot
marked `kept` is a record made earlier that is kept byte for byte (owner, 0.2.20: the factory multis do not follow reworked sounds): it is not
rebuilt, only checked and, when its slot moved, moved (git mv). A slot marked `owner` is a multi the owner made or edited himself (0.2.24, owner:
never drop or change them): kept, and exempt from the similarity check (a pair with one is reported, never a failure). The reverb LOW CUT of
every built multi is the unit's default (G_RLOCUT 75 = 299 Hz) unless the multi sets it; DUCK SRC is PUMP / PART / MIDI (0 / 1 / 2, MU_VER 6); MU_VER 7 adds, per part, the NX extension slots and the hand-set B edit list after the values (tools/multi_upgrade.py).

The owner's own edits of a multi come back into this list through tools/import_owner_edits.py (docs/OWNER_REFINE.md): it writes
tools/owner_multis.py, read below (apply_owner), whose entries replace a slot's record (kept, owner) and its line here.

Old one-off converters (factory_multis_levelsplit.py, _locut.py, _ducksrc.py, _delaymode.py, _lfo_free.py) are gone: every built
record is made in today's format, and the kept ones were converted by them before (0.2.14 .. 0.2.22).

The test scenes of the host tests (the CPU TEST multis of before 0.2.23 and the factory multis of 0.2.22) are in tests/scenes/,
not in a factory slot (tests/scenes.h). SPARE below lists multis made for 0.2.23 that left the sixteen in 0.2.24 (their records are in
tests/scenes/spare/, so they can come back: move the entry into MULTIS, give it a slot and run `build`).
"""
import importlib.util
import math
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIR = ROOT / "tools" / "factory_multis"
GEN = ROOT / "build" / "gen_mm"
REC_MAX, MU_G_AT, MB_N, PS_STEP, NAME_MAX = 1916, 20, 7, 9, 12
ENGINES = ["ANALOG", "DIGITAL", "PHASE", "LOFI", "SAMPLE", "VOICE", "TRIO", "WHEEL", "GRAIN", "PHYS", "DRUM", "NOISE", "FM6", "SLICE", "WAVE"]
MODES = {"DUEL": 0, "STEREO": 1, "LAYER": 2}           # core.h DM_SPLIT (shown DUEL), DM_STEREO, DM_LAYER: the record's byte 4
NOTE = {n: i for i, n in enumerate(["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"])}


def note(s):
    """'F3' -> 53 (C4 = 60)"""
    m = re.fullmatch(r"([A-G]#?)(-?\d)", s)
    return 12 * (int(m.group(2)) + 1) + NOTE[m.group(1)]


# ---- the multis ---------------------------------------------------------------------------------------------------------------
# A part: sound (factory sound name), set {P_id: value or 'NAME' of the parameter's list; "E:LABEL": the engine's EDIT value}, steps (the
# SEQUENCER arp's pattern: offsets in semitones from the held key, '.' a rest, suffix A an accent; LEN = its length), bseed / amt (End B rolled
# again with this seed / at this AMOUNT %; 0 the sound's own). A multi: g {G_id: value}, mode, bal (-100..100), xf (0..100), split (a note:
# below it part 1, at or above part 2; compared with the key as played, before any TRN: F3 is the lowest key at octave 0, so F4 is the middle
# of the keyboard), what (one line, the character).
# Units are the stored ones (params.c): sends / SAT / RES 0..127, LEVEL / TRIM 0..127 (112 = 0 dB), PAN -64..63, EQ dB -12..12,
# DUCK DEPTH 0..100, arp GATE 1..127, the lists by name. Owner rules (0.2.23): at most 2 or 3 tributes (none in the sixteen since 0.2.25: BASILDON is spare),
# no Moroder multi (it is a pattern), classic honest names, no two multis alike (check_similar). Owner (0.2.24): his own multis
# (COLD CHAPEL, ACID ETHER, GARGLE ACID of 0.2.22; DEAD BUFFER holds his edits) are never dropped: `owner`.
# In LAYER each part maps a played key with its own TRN (seq.c key_on, 0.2.23: until 0.2.22 part 2 got part 1's note and the split compared it),
# so an octave between the parts can be set with either part's TRN.
# Order (0.2.24): 1-4 bass grooves and splits, 5-8 pads, layers and the dry pair, 9-12 the four multis of 0.2.1 as made (the owner's),
# 13-15 for performing (0.2.25: 13 is the owner's original COLD CHAPEL, in the slot of BASILDON, the multi least different from the rest), 16 the stress test.
SEND = ("P_CHOR", "P_DLY", "P_REV")
MULTIS = [
    # ---- 1-4: bass grooves and splits ----
    dict(slot=1, name="REESE SWAMP", kept=True, mode="LAYER", parts=[dict(sound="REESE BASS"), dict(sound="SQUELCH")],
         what="Reese bass wobbling at 1/4 under a 1/16 acid loop (0.2.11, M01 since 0.2.13: the first start's multi)"),
    dict(slot=2, name="NIGHT ORGAN", mode="LAYER", split="F4",
         g=dict(G_BPM=121, G_SWING=17, G_DTIME="8D", G_DFDBK=31, G_DMIX=90, G_RTYPE="ROOM", G_RSIZE=71, G_DMODE="DIGI"),
         parts=[dict(sound="PEDAL BASS", set=dict(P_AMODE="SEQ", P_ARATE="1/8", P_AGATE=80, P_AHOLD="OFF", P_TRANS=-12, P_VOICE="LEG", P_LEVEL=112),
                     steps=". 0 . 0 . 0 . +12"),
                dict(sound="DRAWBARS", set=dict(P_AMODE="SEQ", P_ARATE="1/8", P_AGATE=100, P_AHOLD="OFF", P_CHRD="MIN7", P_VOIC="CLOSE",
                                                 P_DLY=17, P_REV=21, P_DKSRC="PUMP", P_DKRATE="1/4", P_DKDEP=21, P_LEVEL=104),
                     steps=". 0 . . . 0A . .")]),
    # M03 (0.2.24, owner: the pad sounded dark): the pad's own CUT opened in the multi from the sound's 55 (about 460 Hz) to 71 (about 1 kHz);
    # measured on the pad alone, keys E4 / A4 / C5: spectral centroid 528 / 652 / 760 Hz -> 653 / 808 / 925 Hz, the 2-4 kHz band +3 dB, 4-8 kHz
    # +6 dB, level the same within 0.4 dB. #44 EMBER GLOW itself is unchanged (the override lives in this record only). End B rolled again
    # (seed 23709 at 27 %): the sound's own End B (seed 44167) closed the pad in this multi (its roll took AMP ENV -> FLT to -26: the pad alone
    # -14.2 dB at End B on C4 E4 G4, `gen_factory_multis eval 3`), against the promise "MORPH opens the pad"; the new seed keeps the level
    # (+0.2 dB) and is brighter (739 -> 1117 Hz, `search 3 2 27 40`).
    dict(slot=3, name="PAD SPLIT", mode="LAYER", split="F4",
         g=dict(G_BPM=97, G_DTIME="4D", G_DFDBK=37, G_DMIX=71, G_RTYPE="HALL", G_RSIZE=97, G_RDAMP=61, G_DMODE="DIGI"),
         parts=[dict(sound="PLUCK BASS", set=dict(P_TRANS=-12, P_VOICE="LEG", P_LEVEL=112)),
                dict(sound="EMBER GLOW", set={"E:CUT": 71, "P_DLY": 17, "P_REV": 57, "P_M8SRC": "MORPH", "P_M8DST": "CUT", "P_M8AMT": 21, "P_LEVEL": 101},
                     bseed=23709, amt=27)]),
    dict(slot=4, name="DUB CHORD", mode="LAYER",
         g=dict(G_BPM=117, G_DTIME="8D", G_DFDBK=71, G_DCOLOR=47, G_DMIX=97, G_DMODE="DIGI", G_DMDEP=17, G_RTYPE="SPRING", G_RSIZE=77, G_RDAMP=71),
         parts=[dict(sound="BIG STAB", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=37, P_AHOLD="OFF", P_CHRD="MIN7", P_VOIC="CLOSE",
                                                 P_FREEA="u_uf(1700)", P_FREEB=17, P_DLY=77, P_REV=37, P_CHOR=0,
                                                 P_DKSRC="PUMP", P_DKRATE="1/4", P_DKDEP=37, P_LEVEL=109),
                     steps=". . 0 . . . . . . . 0 . . . . ."),
                dict(sound="TIDAL SUB", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=120, P_AHOLD="OFF", P_VOICE="LEG", P_LEVEL=117),
                     steps="-24A . . . . . . . -24 . . -24 . . -19 .")]),
    # ---- 5-8: pads and layers (0.2.25: the dry pair MONO DUEL is spare) ----
    dict(slot=5, name="WHISPER TIDE", kept=True, mode="LAYER", parts=[dict(sound="DIGITAL PAD"), dict(sound="WHISPER PAD")]),
    # slot 6: the owner's TINE STRINGS (0.2.25), from tools/owner_multis.py; his COLD CHAPEL, which was here until 0.2.24, is M13 since 0.2.25
    # slot 7: the owner's refined FINAL REEL (0.2.25), from tools/owner_multis.py (his file duell-multi-M07-FINAL_REEL.duell-multis.json); the old kept record was the 0.2.11 one
    # slot 8: the owner's HAZY TENSOR (0.2.25), from tools/owner_multis.py (his file duell-multi-M08-HAZY_TENSOR.duell-multis.json); MONO DUEL, which was here, is spare (below)
    # ---- 9-12: the four multis of 0.2.1, kept as made (the owner's) ----
    dict(slot=9, name="ACID ETHER", kept=True, owner=True, mode="LAYER", parts=[dict(sound="RUBBER BAND"), dict(sound="LADDER BASS")],
         what="an acid line over a Moog-style bass, the chorus wobbling it (the owner's, 0.2.1: M04, M06 in 0.2.13 .. 0.2.22; back in 0.2.24)"),
    dict(slot=10, name="WIRED SNAIL", kept=True, mode="LAYER", parts=[dict(sound="SFORZANDO"), dict(sound="GLASS SHARDS")],
         what="brass stab in unison two octaves down under a three-octave glass arp (kept as made in 0.2.1)"),
    dict(slot=11, name="EERIE SIGNAL", kept=True, mode="LAYER", parts=[dict(sound="VESPERS"), dict(sound="JAZZ CLUB")],
         what="choir in diatonic triads, an organ picking notes at random at 1/8 (kept as made in 0.2.1)"),
    dict(slot=12, name="DEAD BUFFER", kept=True, owner=True, mode="LAYER", parts=[dict(sound="FROZEN AIR"), dict(sound="MODEM")],
         what="frozen wavetable texture under a modem-noise arp (kept as made in 0.2.1, with the owner's own edits)"),
    # ---- 13-15: performance (one tribute, the owner's fast acid groove, a percussive pair) ----
    dict(slot=13, name="COLD CHAPEL", kept=True, owner=True, mode="LAYER", parts=[dict(sound="EVOLVING PAD"), dict(sound="SINGING BOWL")],
         what="evolving pad latched, a singing bowl walking up the chord a half note each, ping-pong delay in a large hall (the owner's original, 0.2.11 .. 0.2.22: M07; M06 in 0.2.24; M13 since 0.2.25, byte for byte)"),
    dict(slot=14, name="GARGLE ACID", kept=True, owner=True, mode="LAYER", parts=[dict(sound="GROWL BASS"), dict(sound="SQUELCH")],
         what="a growl bass two octaves down holding the key, a 16-step acid riff on it at 127 BPM (the owner's, 0.2.11 .. 0.2.22: M08; back in 0.2.24)"),
    # slot 15: the owner's WOOD & SKIN (0.2.31), from tools/owner_multis.py (his file duell-multi-M15-WOOD_SKIN.duell-multis.json, record v7, 487 B, crc32 975420133); the 0.2.23 record built here differed in one byte (index 52)
    # ---- 16: the STRESS TEST (owner, 0.2.23: how many notes the unit takes; a test, free to overwrite) ----
    dict(slot=16, name="STRESS TEST", mode="LAYER", stress=True,
         what="CPU stress test, not music: the two dearest sounds, every stage on (24 dB multimode filters, FLANG insert + SAT on the whistle, EQ + ducker on the pad, chorus, RANDOM delay, SHIM BOTH), "
              "a 1/16 arp pad with a short gate and release (0.2.24: calibrated so one key plays on the unit: host model 1 key ~62 %, 2 keys ~78 %, 3 keys ~95 %)",
         g=dict(G_BPM=137, G_DTIME="1/8", G_DFDBK=61, G_DMIX=90, G_DMODE="RAND", G_DMDEP=71, G_DMRATE=44, G_CDEPTH=77, G_CRATE=41,
                G_RTYPE="SHIM", G_RSIZE=107, G_RDAMP=51, G_SHPITCH="BOTH", G_SHMOD=51),
         parts=[dict(sound="PAN WHISTLE", set=dict(P_VOICE="POLY", P_FREEA="u_uf(61)", P_FREEB=17, P_FREEC=127, P_FREED=2, P_ITYPE="FLANG", P_IMIX=97, P_DIST=41,
                                                    P_CHOR=71, P_DLY=61, P_REV=71, P_LEVEL=112)),
                dict(sound="DIGITAL PAD", set=dict(P_VOICE="POLY", P_FREEA="u_uf(61)", P_FREEB=17, P_FREEC=127, P_FREED=2, P_AMODE="UP", P_ARATE="1/16", P_AOCT=2, P_AGATE=71, P_AHOLD="OFF",
                                                    P_REL=47, P_EQLG=4, P_EQMG=-3, P_EQHG=3,
                                                    P_DKSRC="PUMP", P_DKRATE="1/4", P_DKDEP=41, P_CHOR=71, P_DLY=61, P_REV=71, P_LEVEL=112))]),
]

# ---- spare multis (0.2.24): made for 0.2.23, out of the sixteen so the owner's COLD CHAPEL, ACID ETHER and GARGLE ACID could come back.
# Chosen by the similarity check's own measure against the restored and kept ones: VELVET TINES and SEVENOAKS are both a PAD + KEYS layer
# like the owner's EERIE SIGNAL (VESPERS + JAZZ CLUB), and the 0.2.23 drone shared SINGING BOWL with his COLD CHAPEL and took its name
# (renamed here). The 0.2.23 TINE STRINGS is VELVET TINES here since 0.2.25: the owner's M06 has that name (its record still says TINE STRINGS). Their records of 0.2.23 are tests/scenes/spare/ (rendered and hashed by tests/multi_render_test.c). Not built.
SPARE = [
    dict(name="VELVET TINES", scene="spare/M06_TINE_STRINGS", mode="LAYER", xf=37,
         what="electric piano over a string pad: harder notes bring the tines forward (XFADE), MORPH opens the strings (M06 in 0.2.23)",
         g=dict(G_BPM=77, G_DTIME="4D", G_DFDBK=27, G_DMIX=71, G_DMODE="DIGI", G_RTYPE="PLATE", G_RSIZE=77, G_RDAMP=61),
         parts=[dict(sound="VELVET BOW", set=dict(P_REV=47, P_M8SRC="MORPH", P_M8DST="CUT", P_M8AMT=21, P_LEVEL=101), bseed=25230, amt=27),
                dict(sound="TINE ROOM", set=dict(P_DLY=11, P_REV=31, P_LEVEL=112), amt=27)]),
    dict(name="BOWL & WIND", scene="spare/M07_BOWL_AND_WIND", mode="LAYER",
         what="cinematic drone: a bowed singing bowl an octave down over cold moor wind, both latched; MORPH opens them (M07 in 0.2.23, named COLD CHAPEL then: the record still says so)",
         g=dict(G_BPM=71, G_DTIME="4D", G_DFDBK=71, G_DMIX=97, G_RTYPE="HALL", G_RSIZE=117, G_RDAMP=57, G_DMODE="PING", G_DMDEP=17),
         parts=[dict(sound="SINGING BOWL", set=dict(P_AHOLD="ALL", P_TRANS=-12, P_REV=87, P_DLY=27, P_M8SRC="MORPH", P_M8DST="E3", P_M8AMT=17, P_LEVEL=95),
                     bseed=7869, amt=27),
                dict(sound="MOOR WIND", set=dict(P_AHOLD="ALL", P_REV=71, P_DLY=17, P_M8SRC="MORPH", P_M8DST="CUT", P_M8AMT=11, P_LEVEL=97), amt=27)]),
    dict(name="SEVENOAKS", scene="spare/M14_SEVENOAKS", mode="LAYER", tribute=True,
         what="tribute to Kent's rave-era duo: a 1/16 FM bell arpeggio over two octaves above a phase-distortion string pad (M14 in 0.2.23)",
         g=dict(G_BPM=127, G_DTIME="8D", G_DFDBK=41, G_DMIX=90, G_DMODE="PING", G_RTYPE="HALL", G_RSIZE=97, G_RDAMP=61),
         parts=[dict(sound="LAMPLIGHT", set=dict(P_CHRD="OFF", P_REV=57, P_M8SRC="MORPH", P_M8DST="SHP", P_M8AMT=17, P_LEVEL=90)),
                dict(sound="FROST BELLS", set=dict(P_AMODE="UP", P_ARATE="1/16", P_AOCT=2, P_AGATE=71, P_AHOLD="OFF", P_DLY=37, P_REV=47, P_LEVEL=93))]),
    dict(name="BASILDON", scene="spare/M13_BASILDON", mode="LAYER", tribute=True,
         what="tribute to Essex synth-pop: an 8th-note FM bass sequence under a ring-modulated metal riff, 80s plate (M13 in 0.2.23 .. 0.2.25)",
         g=dict(G_BPM=117, G_DTIME="8D", G_DFDBK=31, G_DMIX=77, G_DMODE="PING", G_RTYPE="PLATE", G_RSIZE=71, G_RDAMP=57),
         parts=[dict(sound="FM BASS 2", set=dict(P_AMODE="SEQ", P_ARATE="1/8", P_AGATE=71, P_AHOLD="OFF", P_TRANS=-12, P_LEVEL=109),
                     steps="0A 0 0 0 0 0 0 +7"),
                dict(sound="TIN TOLL", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=57, P_AHOLD="OFF", P_DLY=31, P_REV=41, P_LEVEL=101),
                     steps="+12A . +19 +12 . +22 +19 . +12A . +15 +17 . +19 . +10")]),
    dict(name="MONO DUEL", scene="spare/M08_MONO_DUEL", mode="DUEL",
         what="two dry mono voices for outboard effects: a chip-Moog bass hard left, a square lead with glide hard right (M08 in 0.2.23 .. 0.2.25; the owner: Mono-dual is out; the only factory multi in DUEL)",
         g=dict(G_BPM=117, G_DTIME="1/8", G_DFDBK=31, G_DMIX=77, G_DMODE="DIGI", G_RTYPE="ROOM", G_RSIZE=51),
         parts=[dict(sound="BREADBIN", set=dict(P_VOICE="LEG", P_TRANS=-12, P_CHOR=0, P_DLY=0, P_REV=0, P_DKDEP=0, P_LEVEL=117)),
                dict(sound="SQUARE LEAD", set=dict(P_VOICE="LEG", P_CHOR=0, P_DLY=0, P_REV=0, P_LEVEL=117))]),
]


def apply_owner(multis):
    """tools/owner_multis.py (written by tools/import_owner_edits.py): OWNER_MULTIS = [dict(slot, name, mode, parts=[dict(sound), dict(sound)], what, ...)],
    each a record the owner saved on his unit, kept byte for byte in tools/factory_multis/Mnn.bin: it replaces the slot's entry here (kept, owner)"""
    f = ROOT / "tools" / "owner_multis.py"
    if not f.exists():
        return multis
    spec = importlib.util.spec_from_file_location("owner_multis", f)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    out = list(multis)
    for o in mod.OWNER_MULTIS:
        if o.get("slot") not in range(1, 17):
            sys.exit("factory_multis: owner_multis.py: slot %r" % o.get("slot"))
        e = dict(o, kept=True, owner=True)
        out = [m for m in out if m["slot"] != e["slot"]] + [e]
    return sorted(out, key=lambda m: m["slot"])


MULTIS = apply_owner(MULTIS)


# ---- the firmware's names --------------------------------------------------------------------------------------------------------
def _lists():
    src = (ROOT / "firmware" / "src" / "params.c").read_text()
    out = {}
    for m in re.finditer(r"static const char \*const (N_\w+)\[\]\s*=\s*\{(.*?)\};", src, re.S):
        body = re.sub(r"/\*.*?\*/", "", m.group(2), flags=re.S)
        out[m.group(1)] = [s for s in re.findall(r'"((?:[^"\\]|\\.)*)"|\b0\b', body)]
    return out


LISTS = _lists()
P_LIST = {"P_AMODE": "N_AMODE", "P_ARATE": "N_DIV", "P_AHOLD": "N_HOLD", "P_CHRD": "N_CHRD", "P_VOIC": "N_VOIC", "P_VOICE": "N_VOICE",
          "P_LSYNC": "N_LSYNC", "P_ITYPE": "N_ITYPE", "P_DKSRC": "N_DKSRC", "P_DKRATE": "N_DIV", "P_M8SRC": "N_MSRC", "P_M8DST": "N_MDST",
          "P_SDIV": "N_DIV"}
G_LIST = {"G_DTIME": "N_DIV", "G_RTYPE": "N_RTYPE", "G_DMODE": "N_DMODE", "G_SHPITCH": "N_SHP"}
RETIRED = {("G_SHPITCH", "BOTH"): 2}               # 0.2.25 (owner): SHIM's PITCH BOTH removed; a stored 2 plays UP (project.c, fx_bus.c rev_shimx). M16 STRESS TEST holds it


def value(lists, key, v):
    if isinstance(v, int):
        return v
    if v.startswith("u_uf("):                      # the multimode filter's CUT for a frequency in Hz (tools/duell_sounds.py, core.h UF_STEPS 82)
        hz = float(v[5:-1])
        return max(0, min(82, round(82 * math.log(hz / 30) / math.log(16000 / 30))))
    names = LISTS[lists[key]]
    if (key, v) in RETIRED:                        # (0.2.25: a value the unit no longer offers; the record keeps its byte, the unit reads it as the stand-in)
        return RETIRED[(key, v)]
    if v not in names:
        sys.exit("factory_multis: %s has no value %r (%s)" % (key, v, ", ".join(names)))
    return names.index(v)


def _gen_sounds():
    spec = importlib.util.spec_from_file_location("gen_duell_sounds", ROOT / "tools" / "gen_duell_sounds.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def sounds():
    spec = importlib.util.spec_from_file_location("duell_sounds", ROOT / "tools" / "duell_sounds.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return _gen_sounds().with_owner(mod.SOUNDS)        # (0.2.24: the owner's edited sounds, tools/owner_sounds.py, as the build sees them)


def resolve(snd):
    by = {s["name"]: s for s in snd}
    for m in MULTIS + SPARE:
        for p in m["parts"]:
            if p["sound"] not in by:
                sys.exit("factory_multis: M%02d %s: no factory sound %r" % (m.get("slot", 0), m["name"], p["sound"]))
            s = by[p["sound"]]
            p["eng"], p["idx"] = ENGINES.index(s["engine"]), s["idx"]


_EDIT = {}


def edit_key(eng, label):
    """'E:CUT' on a part of engine eng -> 'P_E0 + k' (the engine's EDIT table, eng_*.c, read by tools/gen_duell_sounds.py)"""
    if not _EDIT:
        engines, _ = _gen_sounds().parse_engines()
        for i, e in engines.items():
            _EDIT[e["name"]] = {r[0]: k for k, r in enumerate(e["edit"])}
    labels = _EDIT.get(ENGINES[eng], {})
    if label not in labels:
        sys.exit("factory_multis: %s has no EDIT value %r (%s)" % (ENGINES[eng], label, ", ".join(labels)))
    return "P_E0 + %d" % labels[label]


# ---- the C table ------------------------------------------------------------------------------------------------------------------
def emit(path):
    lines = ["/* generated by tools/factory_multis.py: the factory multis built by tests/gen_factory_multis.c (do not edit) */",
             "static const multi_t MM[] = {"]
    for m in MULTIS:
        if m.get("kept"):
            continue
        g = []
        for k, v in m.get("g", {}).items():
            g += [k, str(value(G_LIST, k, v))]
        split = note(m["split"]) + 1 if m.get("split") else 0
        lines.append('    {"%s", %d, %d, %d, %d, %d, {%s}, {' % (m["name"], m["slot"] - 1, MODES[m["mode"]], m.get("bal", 0), m.get("xf", 0), split,
                                                         ", ".join(g + ["END"])))
        for p in m["parts"]:
            st = []
            for k, v in p.get("set", {}).items():
                if k.startswith("E:"):
                    st += [edit_key(p["eng"], k[2:]), str(v)]
                else:
                    st += [k, str(value(P_LIST, k, v))]
            steps = p.get("steps")
            n = len(steps.split()) if steps else 16
            lines.append('        {%d, %d, "%s", {%s}, %s, %d, %d, %d},' % (p["eng"], p["idx"], p["sound"], ", ".join(st + ["END"]),
                                                                       '"%s"' % steps if steps else "0", n, p.get("bseed", 0), p.get("amt", 0)))
        lines.append("    }},")
    lines.append("};")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n")


BOOT_H = """/* tools/factory_multis.py: no factory multis (the build of the generator itself) */
#define DF_TEST_MULTIS DUELL_TEST_MODE
#define DF_N 0u
static const uint8_t DF_SLOT[1] = {0};
static const uint16_t DM_LEN[1] = {0};
static const uint16_t DM_AT[1] = {0};
static const uint8_t DM_REC[1] = {0};
"""


def build():
    snd = sounds()
    resolve(snd)
    emit(GEN / "factory_multis_def.h")
    (GEN / "duell_multis.h").write_text(BOOT_H)
    exe = ROOT / "build" / "host" / "gen_factory_multis"
    exe.parent.mkdir(parents=True, exist_ok=True)
    cc = ["cc", "-O1", "-w", "-DDUELL_TEST_MODE=1", "-I" + str(GEN), "-Ibuild/gen", "-Ifirmware/src", "-Itests", '-DFELUCCA_VERSION="x"',
          "-o", str(exe), "tests/gen_factory_multis.c", "-lm"]
    if subprocess.run(cc, cwd=ROOT).returncode:
        sys.exit("factory_multis: tests/gen_factory_multis.c does not compile (run DUELL/gen_only.py first: build/gen)")
    if subprocess.run([str(exe), "build", str(DIR)], cwd=ROOT).returncode:
        sys.exit("factory_multis: the build failed")
    want = {"M%02d.bin" % m["slot"] for m in MULTIS}
    for f in DIR.glob("M*.bin"):
        if f.name not in want:
            f.unlink()
            print("removed", f.name)
    check(snd)


# ---- checks -------------------------------------------------------------------------------------------------------------------------
def parts_of(b):
    """the (engine, factory index) of both parts of record b, the end position (after the automation)"""
    np_, pos, out = b[1], MU_G_AT + 2 * b[2], []
    for _ in range(2):
        out.append((b[pos], b[pos + 1], pos))
        flags = b[pos + 3]
        pos += 4 + MB_N + np_
        pos += 1 + b[pos]                               # MU_VER 7: nx, the extension slots
        pos += 1 + 2 * b[pos]                           # .. ne, the hand-set B edits (wide id, value + 64)
        pos += 8 + PS_STEP * sum(bin(x).count("1") for x in b[pos:pos + 8]) + (128 if flags & 1 else 0)
    return out, pos


def check_record(m, b, by_idx, P):
    slot, err = m["slot"], []
    if len(b) > REC_MAX or len(b) < MU_G_AT or b[0] != 7:
        return ["M%02d: not a version 7 record of at most %d bytes" % (slot, REC_MAX)]
    raw = b[8:20].split(b"\0")[0]
    if raw.decode("ascii", "replace") != m["name"] or any(not 32 <= c <= 126 for c in raw) or len(m["name"]) > NAME_MAX:
        err.append("M%02d: name is %r, expected %r" % (slot, raw, m["name"]))
    if b[4] != MODES[m["mode"]]:
        err.append("M%02d: MODE byte %d, expected %d (%s)" % (slot, b[4], MODES[m["mode"]], m["mode"]))
    parts, pos = parts_of(b)
    for t, (eng, idx, at) in enumerate(parts):
        have = by_idx.get((ENGINES[eng] if eng < len(ENGINES) else "?", idx))
        if have != m["parts"][t]["sound"]:
            err.append("M%02d part %d: engine %d sound %d is %r, expected %r" % (slot, t + 1, eng, idx, have, m["parts"][t]["sound"]))
        v = at + 4 + MB_N                               # (the P_COUNT values follow the End B block)
        if not any(b[at + 4:v]):
            err.append("M%02d part %d: no End B block" % (slot, t + 1))
        if b[v + P["P_FREED"]] - 64 not in (1, 2):
            err.append("M%02d part %d: the multimode filter's SLOPE is not 1 or 2" % (slot, t + 1))
        if b[v + P["P_DKSRC"]] - 64 not in (0, 1, 2, 3):          # (3 = OFF, 0.2.24: the default of a new sound)
            err.append("M%02d part %d: DUCK SRC is not PUMP / PART / MIDI / OFF" % (slot, t + 1))
    n = b[pos]
    if pos + 1 + 3 * n + 3 != len(b):
        err.append("M%02d: record length does not match its parts" % slot)
    else:
        split = b[-1]
        want = note(m["split"]) + 1 if m.get("split") else 0
        if not m.get("kept") and split != want:
            err.append("M%02d: SPLIT byte %d, expected %d" % (slot, split, want))
    locut = b[MU_G_AT + 2 * 21] | b[MU_G_AT + 2 * 21 + 1] << 8
    if locut != 75 and not m.get("owner"):          # (an owner's record keeps whatever he set)
        err.append("M%02d: reverb LOW CUT %d, not the default 75" % (slot, locut))
    return err


def p_ids():
    core = re.sub(r"//[^\n]*", " ", re.sub(r"/\*.*?\*/", " ", (ROOT / "firmware" / "src" / "core.h").read_text(), flags=re.S))
    m = re.search(r"enum\s*\{\s*P_LEVEL\b(.*?)P_COUNT", core, re.S)
    names = ["P_LEVEL"] + re.findall(r"\b(P_\w+)\b", m.group(1))
    return {n: i for i, n in enumerate(names)}


def check_similar(entries):
    """No two multis alike (owner, 0.2.23: "no two multis near-duplicates"). entries: (slot, name, mode, (sound 1, sound 2), split,
    ((engine, category) of part 1, of part 2), owner). Alike: the same two sounds (either order); one sound in the same place with the same
    MODE and split; or the same kind: both parts of the same engines and categories (either order) in the same MODE. Returns (bad, allowed):
    the clashes as text; a pair with an owner multi (0.2.24: his are never dropped or changed) is `allowed`, reported and not a failure."""
    bad, allowed = [], []
    for i, a in enumerate(entries):
        for b in entries[i + 1:]:
            why = None
            if set(a[3]) == set(b[3]):
                why = "the same two sounds"
            elif any(a[3][t] == b[3][t] for t in (0, 1)) and a[2] == b[2] and a[4] == b[4]:
                why = "the same part %d sound (%s) in the same MODE and split" % (1 if a[3][0] == b[3][0] else 2, a[3][0] if a[3][0] == b[3][0] else a[3][1])
            elif sorted(a[5]) == sorted(b[5]) and a[2] == b[2]:
                why = "the same kind: %s + %s in the same MODE" % ("/".join(a[5][0]), "/".join(a[5][1]))
            if why:
                line = "M%02d %s and M%02d %s: %s" % (a[0], a[1], b[0], b[1], why)
                owner = (len(a) > 6 and a[6]) or (len(b) > 6 and b[6])
                (allowed if owner else bad).append(line + (" (allowed: the owner's)" if owner else ""))
    return bad, allowed


def selftest():
    """check_similar must catch each kind of pair, pass distinct ones, and let an owner pair through (reported)"""
    k = lambda e1, c1, e2, c2: ((e1, c1), (e2, c2))
    base = (1, "A", 2, ("REESE BASS", "SQUELCH"), 0, k("WAVE", "BASS", "ANALOG", "BASS"), False)
    cases = [((2, "B", 2, ("SQUELCH", "REESE BASS"), 0, k("ANALOG", "BASS", "WAVE", "BASS"), False), 1, 0),      # the same two sounds, swapped
             ((2, "B", 2, ("REESE BASS", "DRAWBARS"), 0, k("WAVE", "BASS", "WAVE", "KEYS"), False), 1, 0),        # part 1's sound, same MODE and split
             ((2, "B", 2, ("GROWL BASS", "LADDER BASS"), 0, k("WAVE", "BASS", "ANALOG", "BASS"), False), 1, 0),   # the same kind
             ((2, "B", 0, ("REESE BASS", "DRAWBARS"), 0, k("WAVE", "BASS", "WAVE", "KEYS"), False), 0, 0),        # the sound shared, another MODE
             ((2, "B", 2, ("TINE ROOM", "VELVET BOW"), 0, k("FM6", "KEYS", "ANALOG", "PAD"), False), 0, 0),       # nothing shared
             ((2, "B", 2, ("GROWL BASS", "SQUELCH"), 0, k("WAVE", "BASS", "ANALOG", "BASS"), True), 0, 1)]        # the owner's: allowed, reported
    ok = True
    for e, want_bad, want_allowed in cases:
        bad, allowed = check_similar([base, e])
        ok &= len(bad) == want_bad and len(allowed) == want_allowed
    print("factory_multis: selftest of the similarity check %s" % ("ok" if ok else "FAILED"))
    return ok


def check(snd=None):
    snd = snd or sounds()
    resolve(snd)
    by_idx = {(s["engine"], s["idx"]): s["name"] for s in snd}
    cat = {s["name"]: (s["engine"], s["cat"]) for s in snd}
    P = p_ids()
    err, entries = [], []
    if len({m["slot"] for m in MULTIS}) != len(MULTIS) or len({m["name"] for m in MULTIS + SPARE}) != len(MULTIS) + len(SPARE):
        err.append("two multis share a slot or a name (the spare ones counted)")
    if sum(1 for m in MULTIS if m.get("tribute")) > 3:
        err.append("more than three tribute multis (owner: at most 2 or 3)")
    for m in MULTIS:
        f = DIR / ("M%02d.bin" % m["slot"])
        if not f.exists():
            err.append("M%02d: %s missing" % (m["slot"], f.name))
            continue
        b = f.read_bytes()
        err += check_record(m, b, by_idx, P)
        if not m.get("stress"):
            snds = (m["parts"][0]["sound"], m["parts"][1]["sound"])
            entries.append((m["slot"], m["name"], b[4], snds, b[-1], (cat[snds[0]], cat[snds[1]]), bool(m.get("owner"))))
    for m in SPARE:                                   # (the spare records exist and name their sounds)
        f = ROOT / "tests" / "scenes" / (m["scene"] + ".bin")
        if not f.exists():
            err.append("spare %s: %s missing" % (m["name"], f.name))
        else:
            parts, _ = parts_of(f.read_bytes())
            for t, (eng, idx, _at) in enumerate(parts):
                if by_idx.get((ENGINES[eng], idx)) != m["parts"][t]["sound"]:
                    err.append("spare %s part %d: the record holds %r, the list says %r" % (m["name"], t + 1, by_idx.get((ENGINES[eng], idx)), m["parts"][t]["sound"]))
    sim, allowed = check_similar(entries)
    for e in err + sim + allowed:
        print("factory_multis:", e)
    print("factory_multis: %d multis checked, %d record problems, multi similarity: %d pairs of %d alike, %d allowed (the owner's); %d spare" % (
        len(MULTIS), len(err), len(sim), len(entries) * (len(entries) - 1) // 2, len(allowed), len(SPARE)))
    return not err and not sim


def listing():
    snd = sounds()
    resolve(snd)
    num = patch_numbers(snd)
    for m in MULTIS:
        print("M%02d %-12s %-6s %-5s %s + %s%s | %s" % (m["slot"], m["name"], m["mode"], "owner" if m.get("owner") else "kept" if m.get("kept") else "mine",
                                                       *("#%d %s" % (num[p["sound"]], p["sound"]) for p in m["parts"]),
                                                       "  split " + m["split"] if m.get("split") else "", m.get("what", "")))
    for m in SPARE:
        print("--  %-12s %-6s spare %s + %s | %s" % (m["name"], m["mode"], *("#%d %s" % (num[p["sound"]], p["sound"]) for p in m["parts"]), m.get("what", "")))


def patch_numbers(snd):
    """sound name -> #NN, from docs/PATCH_NUMBERS.md (the PATCHES list)"""
    out = {}
    for line in (ROOT.parent / "docs" / "PATCH_NUMBERS.md").read_text().splitlines():
        m = re.match(r"\|\s*(\d+)\s*\|\s*(.+?)\s*\|", line)
        if m:
            out[m.group(2).replace(" (NEW)", "")] = int(m.group(1))
    return out


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    os.chdir(ROOT)
    if cmd == "build":
        build()
        sys.exit(0 if check() else 1)
    if cmd == "check":
        sys.exit(0 if check() else 1)
    if cmd == "selftest":
        sys.exit(0 if selftest() else 1)
    if cmd == "list":
        listing()
        sys.exit(0)
    sys.exit(__doc__)
