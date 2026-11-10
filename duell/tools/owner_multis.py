# SPDX-License-Identifier: GPL-3.0-only
"""DUELL 0.2.24: multis the owner saved on his unit, written by tools/import_owner_edits.py (1.0: slots 1-5, 8-15 from duell-multis-13, copied byte for byte and CRC-checked) (docs/OWNER_REFINE.md). The record of each is tools/factory_multis/Mnn.bin,
kept byte for byte; tools/factory_multis.py apply_owner puts these in its list as `kept`, `owner` (exempt from the similarity check)."""

OWNER_MULTIS = [
    dict(slot=1, name='REESE SWAMP', mode='LAYER', parts=[dict(sound='REESE BASS'), dict(sound='SQUELCH')]),
    dict(slot=2, name='NIGHT ORGAN', mode='LAYER', parts=[dict(sound='PEDAL BASS'), dict(sound='DRAWBARS')]),
    dict(slot=3, name='PAD SPLIT', mode='LAYER', parts=[dict(sound='EVOLVING PAD'), dict(sound='EMBER GLOW')]),
    dict(slot=4, name='DUB CHORD', mode='LAYER', parts=[dict(sound='BIG STAB'), dict(sound='TIDAL SUB')]),
    dict(slot=5, name='WHISPER TIDE', mode='LAYER', parts=[dict(sound='DIGITAL PAD'), dict(sound='WHISPER PAD')]),
    dict(slot=6, name='TINE STRINGS', mode='LAYER', parts=[dict(sound='STRING MACH'), dict(sound='TINE ROOM')]),
    dict(slot=7, name='FINAL REEL', mode='LAYER', parts=[dict(sound='BRASS PAD'), dict(sound='STRING MACH')]),
    dict(slot=8, name='HAZY TENSOR', mode='LAYER', parts=[dict(sound='LADDER BASS'), dict(sound='GEARBOX')]),
    dict(slot=9, name='ACID ETHER', mode='LAYER', parts=[dict(sound='BREADBIN'), dict(sound='LADDER BASS')]),
    dict(slot=10, name='WIRED SNAIL', mode='LAYER', parts=[dict(sound='SFORZANDO'), dict(sound='GLASS SHARDS')]),
    dict(slot=11, name='EERIE SIGNAL', mode='LAYER', parts=[dict(sound='VESPERS'), dict(sound='JAZZ CLUB')]),
    dict(slot=12, name='DEAD BUFFER', mode='LAYER', parts=[dict(sound='FROZEN AIR'), dict(sound='MODEM')]),
    dict(slot=13, name='COLD CHAPEL', mode='LAYER', parts=[dict(sound='EVOLVING PAD'), dict(sound='SINGING BOWL')]),
    dict(slot=14, name='GARGLE ACID', mode='LAYER', parts=[dict(sound='GROWL BASS'), dict(sound='SQUELCH')]),
    dict(slot=15, name='WOOD & SKIN', mode='LAYER', parts=[dict(sound='MARBLE RUN'), dict(sound='TABLA TALK')]),   # 0.2.31: the owner's record v7 (487 B); 1.0: his duell-multis-13 file, crc32 3035247381
]
