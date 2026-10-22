# SPDX-License-Identifier: GPL-3.0-only
"""DUELL factory sounds: the data (tools/gen_duell_sounds.py validates it and writes build/gen/duell_sounds.h).

DUELL 1.0 (owner, 2026-10-21): the 128 factory sounds are the owner's bank, the records of tools/factory_bank.duell-patches.json as exported (no refinement,
no retuning, the names of the file), written here by tools/import_bank.py. The earlier recipe list (SOUND_RECIPES.md, the refinement passes) is in git history.
Every sound: engine, idx (its factory index within the engine: the identity favourites and the shadows use; APPEND-ONLY per engine), name, cat, p: the
parameters that differ from INIT (params.c TP defaults and the engine's EDIT defaults; E1..E8 are the engine's eight; MOD1..MOD8 are (SRC, DST, AMT)
matrix slots; extension slots as "<group> <label>"), bseed / bamt: End B (the roll's seed and the AMOUNT %). Do not edit by hand: import again.
"""

# CONVERT_TWIN: the engine filters of the earlier sound lists are the twin filter's already (tools/convert_twin.py, a second run is refused).
CONVERT_TWIN = 1

SOUNDS = [
    dict(engine='ANALOG', idx=2, name='BEHIVE', cat='BASS', bseed=18967,  # #1
         p={
            'ATK': 0, 'SUS': 127, 'REL': 44, 'LRATE': 51, 'LPHASE': 32, 'LD_FLT': 9, 'LD_AMP': 39, 'RES2': 33, 'DRV2': 61, 'KTRK': 13, 'DIST': 19,
            'VOICE': 2, 'GLIDE': 32, 'GLMODE': 1, 'TWTYPE': 2, 'FREEA': 46, 'FREEB': 53, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 69, 'E2SUS': 0, 'E2REL': 44,
            'L2RATE': 23, 'CHRD': 9, 'VOIC': 1, 'LTRIG': 0, 'BLEND': -7, 'CUT2': 38, 'TRIM': 104, 'E2FLT': 6, 'L2FLT': 29, 'E2': 30, 'E4': 4, 'E7': 48,
            'MOD1': ('AT', 'FRES', 13), 'MOD2': ('CYCLE', 'RAT1', 55), 'L2TRIG': 1, 'TWIN ROUTE': 1, 'TWIN LINK': 0, 'TWIN LFO1': -17, 'TWIN LFO2': 15,
            'SUB LEVEL': 95, 'SUB WAVE': 1, 'SUB HARM': 40, 'CYCLE CURVE': -33, 'CYCLE SYNC': 1,
         }),
    dict(engine='ANALOG', idx=16, name='FOLD BASS', cat='BASS', bseed=17953,  # #2
         p={
            'ATK': 0, 'DEC': 77, 'SUS': 114, 'REL': 51, 'RES2': 17, 'DRV2': 31, 'KTRK': 13, 'VOICE': 2, 'GLIDE': 16, 'GLMODE': 1, 'TWTYPE': 1,
            'FREEA': 44, 'FREEB': 21, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 77, 'E2SUS': 44, 'E2REL': 56, 'L2RATE': 27, 'L2WAVE': 1, 'ITYPE': 3, 'IA': 46,
            'IB': 100, 'EQHG': -4, 'EQHF': -8, 'CUT2': -7, 'DKDEP': 41, 'TRIM': 111, 'DKSRC': 0, 'E2FLT': 17, 'L2FLT': 4, 'E1': 6, 'E2': 0, 'E3': 70,
            'E7': 20, 'MOD1': ('AT', 'INS A', 16), 'MOD2': ('ENV2', 'INS A', 17), 'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1,
            'SUB LEVEL': 31, 'CYCLE RISE': 97, 'CYCLE FALL': 97,
         }),
    dict(engine='ANALOG', idx=0, name='LADDER BASS', cat='BASS', bseed=26217,  # #3
         p={
            'DEC': 73, 'SUS': 108, 'REL': 54, 'KTRK': 13, 'VOICE': 2, 'FREEA': 24, 'FREEB': 17, 'FREED': 2, 'E2ATK': 32, 'E2DEC': 76, 'E2SUS': 38,
            'E2REL': 54, 'L2RATE': 27, 'L2WAVE': 1, 'TRIM': 96, 'E2FLT': 27, 'L2FLT': 4, 'E1': 6, 'E2': 6, 'E7': 24, 'MOD1': ('AT', 'FRES', 17),
            'MOD2': ('VEL', 'CUT', 6), 'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 31, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         }),
    dict(engine='ANALOG', idx=3, name='RUBBER BAND', cat='BASS', bseed=49167,  # #4
         p={
            'ATK': 0, 'DEC': 82, 'SUS': 41, 'REL': 64, 'KTRK': 13, 'DIST': 22, 'REV': 6, 'VOICE': 2, 'FREEA': 27, 'FREEB': 47, 'E2ATK': 0, 'E2DEC': 65,
            'E2SUS': 0, 'E2REL': 65, 'L2RATE': 21, 'EQHG': -4, 'EQHF': -8, 'TRIM': 112, 'E2FLT': 37, 'L2FLT': 4, 'E1': 6, 'E2': 0, 'E3': 25, 'E7': 18,
            'MOD1': ('AT', 'SAT', 7), 'MOD2': ('VEL', 'CUT', 8), 'MOD3': ('CYCLE', 'FRES', 11), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 31,
            'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 81,
         }),
    dict(engine='ANALOG', idx=4, name='SQUELCH', cat='BASS', bseed=17703,  # #5
         p={
            'ATK': 0, 'DEC': 86, 'SUS': 110, 'REL': 32, 'KTRK': 13, 'DIST': 25, 'DLY': 32, 'VOICE': 2, 'GLIDE': 26, 'GLMODE': 1, 'FREEA': 26,
            'FREEB': 97, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 61, 'E2SUS': 0, 'E2REL': 0, 'L2RATE': 21, 'TRIM': 93, 'E2FLT': 27, 'L2FLT': 4, 'E2': 0,
            'E3': 0, 'E7': 30, 'MOD1': ('AT', 'CUT', 7), 'MOD2': ('VEL', 'FRES', 8), 'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 120, 'CYCLE FALL': 0,
         }),
    dict(engine='ANALOG', idx=1, name='TIDAL SUB', cat='BASS', bseed=1207,  # #6
         p={
            'ATK': 22, 'SUS': 127, 'KTRK': 13, 'DIST': 7, 'VOICE': 2, 'FREEA': 41, 'E2ATK': 0, 'E2DEC': 69, 'E2SUS': 0, 'L2RATE': 17, 'L2WAVE': 1,
            'TRIM': 88, 'E2FLT': 4, 'L2FLT': 4, 'E1': 3, 'E2': 0, 'E3': 0, 'E7': 16, 'MOD1': ('AT', 'SUBLV', 17), 'MOD2': ('CYCLE', 'E7', 7),
            'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 17, 'CYCLE RISE': 105, 'CYCLE FALL': 105,
         }),
    dict(engine='FM6', idx=12, name='FM BASS 2', cat='BASS', bseed=17352,  # #7
         p={
            'LRATE': 30, 'LWAVE': 1, 'LD_FLT': 4, 'RES2': 21, 'KTRK': 0, 'REV': 6, 'VOICE': 2, 'TWTYPE': 1, 'FREEA': 51, 'FREEB': 11, 'E2ATK': 0,
            'E2DEC': 71, 'E2SUS': 0, 'E2REL': 61, 'ITYPE': 1, 'IA': 40, 'IB': 127, 'IC': 112, 'IMIX': 90, 'EQHG': -4, 'EQHF': -8, 'CUT2': -17,
            'DKDEP': 51, 'DKSRC': 0, 'E2FLT': 17, 'E8': 41, 'MOD1': ('AT', 'E5', 21), 'MOD2': ('ENV2', 'E3', 17), 'MOD3': ('CYCLE', 'E3', 4),
            'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 61, 'CYCLE RISE': 91, 'CYCLE FALL': 91,
         }),
    dict(engine='FM6', idx=11, name='FRETLESS', cat='BASS', bseed=1048,  # #8
         p={
            'KTRK': 0, 'DIST': 7, 'CHOR': 14, 'VOICE': 2, 'GLIDE': 31, 'GLMODE': 1, 'FREEA': 47, 'FREEB': 21, 'E2ATK': 71, 'E2DEC': 87, 'E2SUS': 41,
            'E2REL': 51, 'L2RATE': 21, 'L2WAVE': 1, 'EQHG': -4, 'EQHF': -8, 'DKDEP': 14, 'TRIM': 120, 'DKSRC': 0, 'E2FLT': 27, 'L2FLT': 4, 'E6': 2,
            'E8': 43, 'MOD1': ('AT', 'E3', 4), 'MOD2': ('ENV2', 'E3', 5), 'MOD3': ('CYCLE', 'E3', 4), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 100,
            'CYCLE FALL': 100,
         }),
    dict(engine='FM6', idx=0, name='SLAP DOCTOR', cat='BASS', bseed=45536,  # #9
         p={
            'KTRK': 0, 'DIST': 7, 'VOICE': 2, 'FREEA': 41, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 59, 'E2SUS': 0, 'E2REL': 56, 'L2RATE': 27, 'L2WAVE': 1,
            'EQHG': -4, 'EQHF': -8, 'DKDEP': 16, 'TRIM': 116, 'DKSRC': 0, 'E2FLT': 21, 'L2FLT': 4, 'E2': 1, 'E3': 10, 'E5': 24, 'E6': 3, 'E8': 2,
            'MOD1': ('AT', 'E7', 37), 'MOD2': ('ENV2', 'E3', 11), 'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 41,
            'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 81,
         }),
    dict(engine='PHASE', idx=0, name='ELASTIC BASS', cat='BASS', bseed=60775, bamt=11,  # #10
         p={
            'DEC': 76, 'SUS': 70, 'REL': 55, 'KTRK': 0, 'DIST': 13, 'VOICE': 2, 'GLIDE': 21, 'GLMODE': 1, 'FREEA': 37, 'FREEB': 31, 'FREED': 2,
            'E2ATK': 0, 'E2DEC': 71, 'E2SUS': 13, 'E2REL': 54, 'L2RATE': 27, 'L2WAVE': 1, 'EQHG': -4, 'EQHF': -8, 'DKDEP': 14, 'TRIM': 110, 'DKSRC': 0,
            'E2FLT': 27, 'L2FLT': 4, 'E1': 5, 'E3': 38, 'E4': 66, 'MOD1': ('AT', 'E3', 11), 'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 100, 'CYCLE FALL': 100,
         }),
    dict(engine='PHASE', idx=9, name='FUZZ BASS', cat='BASS', bseed=57016,  # #11
         p={
            'DEC': 77, 'SUS': 112, 'REL': 52, 'KTRK': 0, 'VOICE': 2, 'GLIDE': 18, 'GLMODE': 1, 'FREEA': 41, 'FREEB': 17, 'FREED': 2, 'E2ATK': 0,
            'E2DEC': 71, 'E2SUS': 25, 'E2REL': 54, 'L2RATE': 21, 'ITYPE': 4, 'IA': 82, 'IB': 90, 'IC': 90, 'IMIX': 110, 'EQHG': -4, 'EQHF': -8,
            'TRIM': 111, 'E2FLT': 21, 'L2FLT': 4, 'E1': 5, 'E3': 44, 'E4': 45, 'MOD1': ('AT', 'FRES', 17), 'MOD2': ('CYCLE', 'INS A', 7), 'LTRIG': 1,
            'L2TRIG': 1, 'CYCLE RISE': 97, 'CYCLE FALL': 97,
         }),
    dict(engine='LOFI', idx=8, name='ARP BASS', cat='BASS', bseed=16972,  # #12
         p={
            'ATK': 0, 'DEC': 69, 'SUS': 89, 'REL': 47, 'KTRK': 0, 'VOICE': 2, 'FREEA': 51, 'FREEB': 21, 'E2ATK': 0, 'E2DEC': 71, 'E2SUS': 0,
            'E2REL': 51, 'L2RATE': 40, 'L2WAVE': 1, 'EQHG': -4, 'EQHF': -8, 'DKDEP': 14, 'TRIM': 97, 'DKSRC': 0, 'E2FLT': 17, 'L2SHP': 11, 'E1': 2,
            'E3': 40, 'E7': 1, 'E8': 84, 'MOD1': ('AT', 'SHP', 17), 'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 31,
            'SUB WAVE': 2, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         }),
    dict(engine='LOFI', idx=0, name='CARTRIDGE', cat='BASS', bseed=28474,  # #13
         p={
            'DEC': 73, 'SUS': 102, 'REL': 51, 'KTRK': 0, 'VOICE': 2, 'FREEA': 47, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 69, 'E2SUS': 0, 'E2REL': 51,
            'L2RATE': 17, 'L2WAVE': 1, 'EQHG': -4, 'EQHF': -8, 'DKDEP': 15, 'TRIM': 99, 'DKSRC': 0, 'E2FLT': 17, 'L2FLT': 4, 'E2': 4, 'E3': 67,
            'E8': 72, 'MOD1': ('AT', 'E8', 17), 'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 41, 'CYCLE RISE': 100,
            'CYCLE FALL': 100,
         }),
    dict(engine='VOICE', idx=0, name='BULLFROG', cat='BASS', bseed=36283,  # #14
         p={
            'ATK': 15, 'DEC': 79, 'SUS': 102, 'REL': 59, 'KTRK': 0, 'DIST': 24, 'VOICE': 2, 'GLIDE': 23, 'GLMODE': 1, 'FREEA': 52, 'FREEB': 17,
            'E2ATK': 0, 'E2DEC': 76, 'E2SUS': 0, 'L2RATE': 21, 'L2WAVE': 1, 'EQHG': -4, 'EQHF': -8, 'DKDEP': 15, 'TRIM': 94, 'DKSRC': 0, 'E2FLT': 17,
            'L2FLT': 4, 'E1': 95, 'E2': 6, 'E3': 62, 'E4': -4, 'E5': 96, 'E6': 0, 'E7': 84, 'MOD1': ('AT', 'E4', 5), 'MOD2': ('CYCLE', 'SHP', -9),
            'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 97, 'CYCLE FALL': 97,
         }),
    dict(engine='TRIO', idx=0, name='BREADBIN', cat='BASS', bseed=30953,  # #15
         p={
            'DEC': 74, 'SUS': 108, 'REL': 55, 'RES2': 41, 'KTRK': 0, 'VOICE': 2, 'GLIDE': 22, 'GLMODE': 1, 'TWTYPE': 1, 'FREEA': 47, 'FREEB': 11,
            'E2ATK': 19, 'E2DEC': 75, 'E2SUS': 28, 'E2REL': 56, 'L2RATE': 27, 'L2WAVE': 1, 'EQHG': -4, 'EQHF': -8, 'CUT2': 37, 'DKDEP': 15,
            'TRIM': 101, 'DKSRC': 0, 'E4': 11, 'MOD1': ('AT', 'E4', 17), 'MOD2': ('VEL', 'CUT2', 6), 'MOD3': ('CYCLE', 'CUT2', 7), 'LTRIG': 1,
            'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 19, 'TWIN LFO2': 4, 'SUB LEVEL': 31, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         }),
    dict(engine='TRIO', idx=11, name='SID BASS', cat='BASS', bseed=7135,  # #16
         p={
            'DEC': 76, 'SUS': 108, 'REL': 54, 'RES2': 31, 'KTRK': 0, 'VOICE': 2, 'GLIDE': 19, 'GLMODE': 1, 'TWTYPE': 1, 'FREEA': 52, 'FREEB': 11,
            'E2ATK': 0, 'E2DEC': 74, 'E2SUS': 32, 'E2REL': 56, 'L2RATE': 59, 'L2WAVE': 1, 'EQHG': -4, 'EQHF': -8, 'CUT2': 46, 'TRIM': 93, 'L2SHP': 18,
            'E1': 1, 'E4': 7, 'E8': 40, 'MOD1': ('AT', 'SHP', 17), 'MOD2': ('VEL', 'CUT2', 6), 'MOD3': ('CYCLE', 'CUT2', 7), 'LTRIG': 1, 'L2TRIG': 1,
            'TWIN LINK': 0, 'TWIN ENV2': 17, 'SUB LEVEL': 31, 'SUB WAVE': 2, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         }),
    dict(engine='TRIO', idx=12, name='SYNC BASS', cat='BASS', bseed=43701,  # #17
         p={
            'DEC': 79, 'SUS': 102, 'REL': 44, 'RES2': 87, 'DRV2': 21, 'KTRK': 0, 'DIST': 31, 'VOICE': 2, 'GLIDE': 18, 'GLMODE': 1, 'TWTYPE': 5,
            'FREEA': 31, 'FREEB': 77, 'E2ATK': 0, 'E2SUS': 0, 'E2REL': 54, 'L2RATE': 30, 'L2WAVE': 1, 'EQHG': -4, 'EQHF': -8, 'CUT2': 40, 'TRIM': 114,
            'E2FLT': 31, 'E1': 9, 'E2': 7, 'E3': 0, 'E4': 0, 'MOD1': ('AT', 'FDRV', 21), 'MOD2': ('VEL', 'CUT2', 6), 'MOD3': ('ENV2', 'E2', 17),
            'MOD4': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 31, 'TWIN LFO2': 5, 'SUB LEVEL': 41, 'SUB WAVE': 2,
            'CYCLE RISE': 41, 'CYCLE FALL': 41, 'CYCLE SYNC': 1,
         }),
    dict(engine='WHEEL', idx=0, name='PEDAL BASS', cat='BASS', bseed=14321,  # #18
         p={
            'DEC': 94, 'SUS': 127, 'REL': 44, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 4, 'KTRK': 0, 'DIST': 30, 'VOICE': 2, 'FREEA': 36, 'FREEB': 11,
            'E2ATK': 0, 'E2DEC': 62, 'E2SUS': 0, 'E2REL': 47, 'ITYPE': 9, 'IA': 18, 'IB': 90, 'IC': 40, 'EQHG': -4, 'EQHF': -8, 'TRIM': 104,
            'E2FLT': 11, 'E2SHP': 26, 'E1': 14, 'E3': -4, 'E4': -8, 'E6': 50, 'E7': 30, 'E8': 0, 'MOD1': ('AT', 'E7', 21),
            'MOD2': ('CYCLE', 'CUT', 11), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 80, 'CYCLE CURVE': -21,
         }),
    dict(engine='PHYS', idx=11, name='PLUCK BASS', cat='BASS', bseed=13381,  # #19
         p={
            'ATK': 0, 'SUS': 127, 'REL': 65, 'KTRK': 0, 'REV': 9, 'FREEA': 36, 'FREEB': 17, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 67, 'E2SUS': 0,
            'E2REL': 56, 'L2RATE': 27, 'L2WAVE': 1, 'ITYPE': 1, 'IA': 100, 'IB': 127, 'IC': 112, 'EQHG': -4, 'EQHF': -8, 'TRIM': 106, 'E2FLT': 31,
            'L2FLT': 4, 'E1': 1, 'E2': 61, 'E3': 47, 'E4': 118, 'E5': 41, 'E6': 127, 'E8': 11, 'MOD1': ('AT', 'E3', 11), 'MOD2': ('CYCLE', 'CUT', 7),
            'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         }),
    dict(engine='NOISE', idx=0, name='GEARBOX', cat='BASS', bseed=14165,  # #20
         p={
            'DEC': 73, 'SUS': 108, 'REL': 55, 'LWAVE': 3, 'LD_FLT': 7, 'RES2': 31, 'VOICE': 2, 'TWTYPE': 1, 'FREEA': 56, 'FREEB': 21, 'E2ATK': 19,
            'E2DEC': 76, 'E2SUS': 32, 'E2REL': 56, 'LSYNC': 8, 'EQHG': -4, 'EQHF': -8, 'CUT2': -11, 'DKDEP': 41, 'TRIM': 95, 'DKSRC': 0, 'E2FLT': 17,
            'E1': 3, 'E2': 0, 'E6': 48, 'MOD1': ('AT', 'FRES', 21), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('VEL', 'CUT', 6), 'LTRIG': 1, 'L2TRIG': 1,
            'KTRK': 32, 'SUB LEVEL': 31, 'SUB WAVE': 2, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         }),
    dict(engine='WAVE', idx=11, name='GROWL BASS', cat='BASS', bseed=20849,  # #21
         p={
            'DEC': 79, 'SUS': 127, 'REL': 56, 'LRATE': 32, 'LWAVE': 1, 'LD_FLT': 8, 'RES2': 31, 'DRV2': 21, 'KTRK': 0, 'DIST': 25, 'VOICE': 2,
            'GLIDE': 21, 'GLMODE': 1, 'TWTYPE': 5, 'FREEA': 40, 'FREEB': 31, 'E2ATK': 0, 'E2DEC': 73, 'E2SUS': 0, 'E2REL': 56, 'L2SYNC': 4, 'LPOL': 1,
            'CUT2': -7, 'DKDEP': 71, 'TRIM': 124, 'DKSRC': 0, 'E2FLT': 11, 'L2SHP': 19, 'E1': 20, 'E2': 40, 'MOD1': ('AT', 'SUBLV', 11),
            'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO1', 'E3', 13), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 62, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         }),
    dict(engine='WAVE', idx=8, name='HARM BASS', cat='BASS', bseed=35823,  # #22
         p={
            'DEC': 79, 'SUS': 108, 'REL': 54, 'KTRK': 0, 'VOICE': 2, 'GLIDE': 21, 'GLMODE': 1, 'FREEA': 36, 'FREEB': 31, 'FREED': 2, 'E2ATK': 0,
            'E2DEC': 76, 'E2SUS': 25, 'E2REL': 56, 'L2RATE': 27, 'L2WAVE': 1, 'TRIM': 103, 'E2FLT': 21, 'E2SHP': 29, 'L2FLT': 4, 'E1': 2, 'E2': 45,
            'MOD1': ('AT', 'SAT', 9), 'MOD2': ('VEL', 'SHP', 13), 'MOD3': ('ENV2', 'E3', 11), 'MOD4': ('CYCLE', 'SHP', 11), 'LTRIG': 1, 'L2TRIG': 1,
            'SUB LEVEL': 70, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         }),
    dict(engine='WAVE', idx=21, name='KICK BASS', cat='BASS', bseed=3922,  # #23
         p={
            'ATK': 0, 'DEC': 82, 'SUS': 70, 'REL': 56, 'KTRK': 0, 'DIST': 41, 'VOICE': 2, 'GLMODE': 1, 'E2ATK': 0, 'E2DEC': 63, 'E2SUS': 0,
            'E2REL': 47, 'ITYPE': 2, 'IA': 112, 'IB': 90, 'EQHG': -4, 'EQHF': -8, 'TRIM': 107, 'E1': 2, 'E2': 10, 'MOD1': ('AT', 'SHP', 16),
            'MOD2': ('ENV2', 'PITCH', 63), 'MOD3': ('ENV2', 'PITCH', 63), 'MOD4': ('EXPR', 'PITCH', -64), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 74,
         }),
    dict(engine='WAVE', idx=12, name='REESE BASS', cat='BASS', bseed=5220,  # #24
         p={
            'ATK': 0, 'DEC': 79, 'SUS': 127, 'REL': 44, 'LRATE': 26, 'LWAVE': 1, 'LD_FLT': 8, 'LD_SHP': 13, 'RES2': 21, 'DRV2': 31, 'KTRK': 0,
            'DIST': 10, 'VOICE': 2, 'GLIDE': 26, 'GLMODE': 1, 'TWTYPE': 1, 'FREEA': 46, 'FREEB': 21, 'E2ATK': 0, 'E2DEC': 69, 'E2SUS': 0, 'E2REL': 56,
            'L2RATE': 15, 'L2WAVE': 1, 'CUT2': -8, 'DKDEP': 41, 'TRIM': 110, 'DKSRC': 0, 'E2FLT': 11, 'E1': 18, 'E2': 20, 'E6': 25,
            'MOD1': ('AT', 'E6', 17), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO2', 'E6', 17), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 65,
            'CYCLE RISE': 90, 'CYCLE FALL': 90,
         }),
    dict(engine='ANALOG', idx=19, name='DRIVEN SAW', cat='LEAD', bseed=64169,  # #25
         p={
            'ATK': 0, 'DEC': 71, 'SUS': 127, 'REL': 51, 'RES2': 17, 'DRV2': 71, 'DLY': 17, 'REV': 21, 'VOICE': 2, 'GLIDE': 17, 'GLMODE': 1,
            'TWTYPE': 3, 'FREEA': 71, 'FREEB': 37, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 71, 'E2SUS': 31, 'E2REL': 51, 'CUT2': 11, 'TRIM': 94, 'E2FLT': 47,
            'E2': 37, 'E3': 107, 'E7': 17, 'MOD1': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'SUB LEVEL': 47, 'SUB WAVE': 2,
            'CYCLE RISE': 97, 'CYCLE FALL': 97,
         }),
    dict(engine='ANALOG', idx=7, name='FUNFARE', cat='LEAD', bseed=63704,  # #26
         p={
            'ATK': 72, 'DEC': 79, 'SUS': 127, 'REL': 64, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 4, 'REV': 25, 'FREEA': 31, 'FREEB': 11, 'E2ATK': 87,
            'E2DEC': 91, 'E2SUS': 71, 'E2REL': 69, 'L2RATE': 87, 'L2WAVE': 1, 'L2FADE': 83, 'ITYPE': 8, 'IA': 40, 'IB': 80, 'IC': 20, 'IMIX': 70,
            'TRIM': 99, 'E2FLT': 37, 'L2PIT': 1, 'E2': 14, 'MOD1': ('AT', 'CUT', 17), 'MOD2': ('VEL', 'CUT', 10), 'MOD3': ('CYCLE', 'CUT', 7),
            'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 110, 'CYCLE FALL': 110,
         }),
    dict(engine='ANALOG', idx=6, name='RAZORWIRE', cat='LEAD', bseed=61032,  # #27
         p={
            'ATK': 0, 'SUS': 127, 'REL': 69, 'LRATE': 40, 'LWAVE': 1, 'LD_FLT': 5, 'DRV2': 41, 'DLY': 18, 'VOICE': 2, 'GLIDE': 30, 'GLMODE': 1,
            'TWTYPE': 1, 'FREEA': 47, 'FREEB': 57, 'E2ATK': 0, 'E2DEC': 77, 'E2SUS': 0, 'E2REL': 69, 'L2RATE': 44, 'L2WAVE': 1, 'ITYPE': 6, 'IA': 44,
            'IMIX': 51, 'CUT2': 66, 'TRIM': 91, 'E2FLT': 17, 'E2SHP': 63, 'L2SHP': 16, 'E1': 5, 'E2': 37, 'E3': 102, 'E7': 36,
            'MOD1': ('AT', 'SHP', 26), 'MOD2': ('VEL', 'SHP', 13), 'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0,
            'TWIN LFO2': 4, 'CYCLE RISE': 97, 'CYCLE FALL': 97,
         }),
    dict(engine='ANALOG', idx=17, name='SQUARE LEAD', cat='LEAD', bseed=21704,  # #28
         p={
            'ATK': 22, 'SUS': 127, 'REL': 73, 'LRATE': 40, 'LWAVE': 1, 'LD_FLT': 4, 'RES2': 107, 'DLY': 23, 'REV': 19, 'VOICE': 2, 'GLIDE': 28,
            'GLMODE': 1, 'TWTYPE': 1, 'FREEA': 44, 'FREEB': 77, 'E2ATK': 29, 'E2DEC': 86, 'E2SUS': 76, 'E2REL': 73, 'L2RATE': 30, 'L2WAVE': 1,
            'CUT2': 56, 'TRIM': 93, 'E1': 1, 'E2': 7, 'E3': 51, 'MOD1': ('AT', 'CUT2', 17), 'MOD2': ('VEL', 'CUT', 6), 'MOD3': ('CYCLE', 'CUT', 5),
            'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 17, 'TWIN LFO2': 4, 'CYCLE RISE': 97, 'CYCLE FALL': 97,
         }),
    dict(engine='ANALOG', idx=5, name='TORCH SONG', cat='LEAD', bseed=58621,  # #29
         p={
            'ATK': 44, 'SUS': 127, 'REL': 81, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 4, 'DIST': 7, 'CHOR': 38, 'REV': 51, 'VOICE': 2, 'GLIDE': 36,
            'GLMODE': 1, 'FREEA': 49, 'FREEB': 11, 'FREED': 2, 'E2ATK': 56, 'E2DEC': 88, 'E2SUS': 89, 'E2REL': 79, 'L2RATE': 87, 'L2WAVE': 1,
            'L2FADE': 86, 'TRIM': 93, 'E2FLT': 17, 'L2PIT': 1, 'E1': 2, 'E2': 7, 'E3': 32, 'MOD1': ('AT', 'DEP2', 11), 'MOD2': ('VEL', 'CUT', 6),
            'MOD3': ('CYCLE', 'CUT', 5), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 105, 'CYCLE FALL': 105,
         }),
    dict(engine='ANALOG', idx=8, name='WALL OF SAW', cat='LEAD', bseed=21203, bamt=15,  # #30
         p={
            'ATK': 22, 'SUS': 127, 'REL': 76, 'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 4, 'CHOR': 64, 'DLY': 32, 'REV': 44, 'TWTYPE': 3, 'FREEA': 47,
            'FREEB': 11, 'E2ATK': 0, 'E2DEC': 79, 'E2SUS': 0, 'E2REL': 76, 'L2RATE': 18, 'SPRD': 51, 'ITYPE': 1, 'IA': 80, 'IB': 127, 'IC': 112,
            'BLEND': -16, 'CUT2': 56, 'TRIM': 85, 'E2FLT': 11, 'E2': 25, 'MOD1': ('AT', 'FBLND', 17), 'MOD2': ('CYCLE', 'CUT', 7),
            'MOD3': ('LFO2', 'CUT', 2), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN ROUTE': 1, 'TWIN LINK': 0, 'CYCLE RISE': 110, 'CYCLE FALL': 110,
         }),
    dict(engine='FM6', idx=1, name='HAN SOLO', cat='LEAD', bseed=381,  # #31
         p={
            'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 4, 'DIST': 9, 'CHOR': 36, 'DLY': 28, 'REV': 19, 'VOICE': 2, 'GLIDE': 26, 'GLMODE': 1, 'FREEA': 50,
            'FREEB': 21, 'E2ATK': 66, 'E2DEC': 91, 'E2SUS': 110, 'E2REL': 69, 'L2RATE': 87, 'L2WAVE': 1, 'L2FADE': 83, 'TRIM': 115, 'E2FLT': 21,
            'L2PIT': 1, 'E1': 16, 'E2': -1, 'E3': -1, 'E5': 20, 'E6': 3, 'E8': 3, 'MOD1': ('AT', 'E3', 7), 'MOD2': ('CYCLE', 'CUT', 5),
            'MOD3': ('ENV2', 'E3', 4), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 105, 'CYCLE FALL': 105,
         }),
    dict(engine='PHASE', idx=1, name='BRAZEN', cat='LEAD', bseed=1618,  # #32
         p={
            'ATK': 43, 'DEC': 79, 'SUS': 108, 'REL': 66, 'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 4, 'RES2': 21, 'CHOR': 28, 'DLY': 23, 'REV': 23,
            'VOICE': 2, 'GLIDE': 26, 'GLMODE': 1, 'TWTYPE': 1, 'FREEA': 52, 'FREEB': 17, 'E2ATK': 66, 'E2DEC': 81, 'E2SUS': 64, 'E2REL': 64,
            'L2RATE': 87, 'L2WAVE': 1, 'L2FADE': 83, 'CUT2': -8, 'TRIM': 93, 'E2FLT': 21, 'L2PIT': 1, 'E3': 28, 'E4': 88, 'E5': 7,
            'MOD1': ('AT', 'SHP', 7), 'MOD2': ('CYCLE', 'CUT', 5), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 105, 'CYCLE FALL': 105,
         }),
    dict(engine='LOFI', idx=1, name='PIXEL HERO', cat='LEAD', bseed=16641, bamt=21,  # #33
         p={
            'ATK': 0, 'SUS': 127, 'REL': 47, 'LRATE': 21, 'LWAVE': 1, 'DLY': 28, 'REV': 15, 'VOICE': 2, 'GLIDE': 19, 'GLMODE': 1, 'TWTYPE': 4,
            'FREEA': 66, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 79, 'E2SUS': 0, 'E2REL': 56, 'L2RATE': 91, 'L2WAVE': 1, 'L2FADE': 79, 'CUT2': 56,
            'TRIM': 91, 'E2FLT': 11, 'E2SHP': 12, 'L2PIT': 1, 'E1': 4, 'E3': 49, 'E8': 110, 'MOD1': ('AT', 'E8', -17), 'MOD2': ('CYCLE', 'CUT', 5),
            'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO1': 7, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         }),
    dict(engine='TRIO', idx=1, name='BANSHEE', cat='LEAD', bseed=16225,  # #34
         p={
            'ATK': 21, 'SUS': 127, 'REL': 68, 'LRATE': 40, 'LWAVE': 1, 'LD_FLT': 7, 'DRV2': 67, 'KTRK': 31, 'DLY': 13, 'REV': 28, 'VOICE': 2,
            'GLIDE': 28, 'GLMODE': 1, 'TWTYPE': 1, 'FREEA': 47, 'FREEB': 61, 'E2ATK': 74, 'E2DEC': 90, 'E2SUS': 38, 'E2REL': 69, 'L2RATE': 88,
            'L2WAVE': 1, 'L2FADE': 81, 'ITYPE': 7, 'IA': 34, 'IB': 60, 'IC': 40, 'IMIX': 57, 'CUT2': 67, 'TRIM': 97, 'E2FLT': 17, 'L2PIT': 1, 'E1': 9,
            'E2': 12, 'E3': 0, 'E4': 0, 'MOD1': ('AT', 'E4', 17), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('ENV2', 'E2', 17), 'LTRIG': 1, 'L2TRIG': 1,
            'TWIN LINK': 0, 'CYCLE RISE': 97, 'CYCLE FALL': 97,
         }),
    dict(engine='TRIO', idx=8, name='OBOE-ISH', cat='LEAD', bseed=1212,  # #35
         p={
            'ATK': 53, 'SUS': 127, 'REL': 66, 'LRATE': 21, 'LWAVE': 1, 'KTRK': 31, 'DLY': 22, 'REV': 34, 'VOICE': 2, 'GLIDE': 20, 'GLMODE': 1,
            'TWTYPE': 4, 'FREEA': 57, 'FREEB': 27, 'E2ATK': 47, 'E2DEC': 79, 'E2SUS': 89, 'E2REL': 64, 'L2RATE': 87, 'L2WAVE': 1, 'L2FADE': 85,
            'CUT2': 51, 'TRIM': 95, 'E2FLT': 11, 'L2PIT': 1, 'E1': 1, 'E3': 0, 'E4': 3, 'E8': 17, 'MOD1': ('AT', 'FRES', 17),
            'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO1': 11, 'CYCLE RISE': 91, 'CYCLE FALL': 91,
         }),
    dict(engine='WHEEL', idx=1, name='SMOKE RING', cat='LEAD', bseed=53342, bamt=11,  # #36
         p={
            'SUS': 127, 'REL': 51, 'LRATE': 27, 'LWAVE': 1, 'LD_FLT': 5, 'KTRK': 31, 'DLY': 23, 'REV': 25, 'VOICE': 2, 'GLIDE': 23, 'GLMODE': 1,
            'FREEA': 64, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 83, 'E2SUS': 51, 'L2RATE': 92, 'L2WAVE': 1, 'L2FADE': 76, 'TRIM': 97, 'E2FLT': 7,
            'E2SHP': 19, 'L2PIT': 1, 'E1': 13, 'E4': 1, 'E6': 30, 'E7': 55, 'MOD1': ('AT', 'E7', 21), 'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1,
            'L2TRIG': 1, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         }),
    dict(engine='PHYS', idx=0, name='PAN WHISTLE', cat='LEAD', bseed=58847,  # #37
         p={
            'ATK': 56, 'SUS': 127, 'REL': 70, 'LRATE': 32, 'LD_FLT': 4, 'KTRK': 31, 'CHOR': 19, 'DLY': 25, 'REV': 38, 'VOICE': 2, 'GLIDE': 21,
            'GLMODE': 1, 'FREEA': 61, 'FREEB': 21, 'E2ATK': 51, 'E2DEC': 81, 'E2SUS': 89, 'E2REL': 69, 'L2RATE': 87, 'L2WAVE': 1, 'L2FADE': 86,
            'ITYPE': 1, 'IA': 80, 'IB': 127, 'IC': 112, 'TRIM': 101, 'E2FLT': 7, 'L2PIT': 1, 'E2': 34, 'E3': 62, 'E4': 100, 'E5': 14, 'E6': 110,
            'E7': 110, 'E8': 40, 'MOD1': ('AT', 'E7', 10), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO1', 'E3', 5), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 103, 'CYCLE FALL': 103,
         }),
    dict(engine='NOISE', idx=1, name='HORNET', cat='LEAD', bseed=33927, bamt=21,  # #38
         p={
            'ATK': 15, 'SUS': 127, 'REL': 59, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 5, 'KTRK': 63, 'DLY': 25, 'REV': 15, 'VOICE': 2, 'GLIDE': 22,
            'GLMODE': 1, 'FREEA': 61, 'FREEB': 31, 'FREEC': 11, 'E2ATK': 0, 'E2DEC': 79, 'E2SUS': 64, 'L2RATE': 87, 'L2WAVE': 1, 'L2FADE': 79,
            'TRIM': 96, 'E2FLT': 11, 'L2PIT': 1, 'E1': 2, 'E2': 0, 'E6': 112, 'E8': 84, 'MOD1': ('AT', 'FRES', 21), 'MOD2': ('CYCLE', 'CUT', 7),
            'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 97, 'CYCLE FALL': 97,
         }),
    dict(engine='WAVE', idx=13, name='HOOVER', cat='LEAD', bseed=14898,  # #39
         p={
            'ATK': 22, 'DEC': 86, 'SUS': 127, 'REL': 73, 'LRATE': 32, 'LWAVE': 1, 'LD_FLT': 4, 'DIST': 13, 'CHOR': 89, 'REV': 25, 'TWTYPE': 3,
            'FREEA': 62, 'FREEB': 61, 'E2ATK': 0, 'E2DEC': 69, 'E2SUS': 0, 'E2REL': 56, 'L2RATE': 87, 'L2WAVE': 1, 'LPOL': 1, 'BLEND': -17, 'CUT2': 57,
            'TRIM': 112, 'E2FLT': 7, 'L2SHP': 16, 'E1': 19, 'E2': 50, 'MOD1': ('AT', 'CUT', 12), 'MOD2': ('ENV2', 'PITCH', 5),
            'MOD3': ('LFO1', 'E6', 11), 'MOD4': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN ROUTE': 1, 'TWIN LINK': 0, 'CYCLE RISE': 97,
            'CYCLE FALL': 97,
         }),
    dict(engine='WAVE', idx=19, name='LICORICE', cat='LEAD', bseed=46772,  # #40
         p={
            'ATK': 50, 'SUS': 127, 'REL': 65, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 4, 'DLY': 14, 'REV': 34, 'VOICE': 2, 'GLIDE': 20, 'GLMODE': 1,
            'FREEA': 61, 'FREEB': 27, 'E2ATK': 0, 'E2DEC': 65, 'E2SUS': 0, 'E2REL': 59, 'TRIM': 101, 'E2FLT': 11, 'E1': 3, 'E2': 37,
            'MOD1': ('AT', 'SHP', 13), 'MOD2': ('CYCLE', 'SHP', 11), 'MOD3': ('ENV2', 'E2', 13), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 103,
            'CYCLE FALL': 103,
         }),
    dict(engine='WAVE', idx=18, name='PURPLE FLUTE', cat='LEAD', bseed=63290,  # #41
         p={
            'ATK': 59, 'SUS': 127, 'REL': 71, 'LRATE': 17, 'LWAVE': 1, 'LD_SHP': 7, 'DLY': 27, 'REV': 39, 'VOICE': 2, 'GLIDE': 21, 'GLMODE': 1,
            'FREEA': 56, 'FREEB': 31, 'E2ATK': 0, 'E2DEC': 71, 'E2SUS': 0, 'E2REL': 59, 'L2RATE': 88, 'L2FADE': 86, 'TRIM': 101, 'E2FLT': 7,
            'L2FLT': 5, 'E1': 25, 'E2': 10, 'MOD1': ('AT', 'CUT', 11), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('ENV2', 'E2', 17), 'LTRIG': 1,
            'L2TRIG': 1, 'CYCLE RISE': 103, 'CYCLE FALL': 103,
         }),
    dict(engine='WAVE', idx=22, name='SCREAMER', cat='LEAD', bseed=53917,  # #42
         p={
            'ATK': 15, 'SUS': 127, 'REL': 68, 'LRATE': 40, 'LWAVE': 1, 'LD_FLT': 7, 'RES2': 21, 'DRV2': 77, 'DIST': 33, 'DLY': 20, 'REV': 18,
            'VOICE': 2, 'GLIDE': 30, 'GLMODE': 1, 'TWTYPE': 1, 'FREEA': 58, 'FREEB': 51, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 83, 'E2SUS': 51, 'E2REL': 69,
            'L2RATE': 87, 'L2WAVE': 1, 'L2FADE': 83, 'CUT2': 68, 'TRIM': 111, 'E2FLT': 17, 'L2PIT': 1, 'E1': 22, 'E2': 42, 'E6': 10,
            'MOD1': ('AT', 'SAT', 17), 'MOD2': ('CYCLE', 'SHP', 11), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'CYCLE RISE': 91, 'CYCLE FALL': 91,
         }),
    dict(engine='WAVE', idx=7, name='SYNC LEAD', cat='LEAD', bseed=15854,  # #43
         p={
            'ATK': 0, 'DEC': 79, 'SUS': 127, 'REL': 69, 'LRATE': 50, 'LWAVE': 1, 'LD_FLT': 7, 'DIST': 13, 'DLY': 32, 'REV': 19, 'VOICE': 2,
            'GLIDE': 26, 'GLMODE': 1, 'FREEA': 61, 'FREEB': 31, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 86, 'E2SUS': 0, 'E2REL': 73, 'L2RATE': 87,
            'L2WAVE': 1, 'L2FADE': 86, 'L2PIT': 1, 'E4': 30, 'MOD1': ('AT', 'E4', 17), 'MOD2': ('ENV2', 'E4', 26), 'MOD3': ('ENV2', 'E6', 11),
            'MOD4': ('CYCLE', 'SHP', 17), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 91, 'CYCLE FALL': 91,
         }),
    dict(engine='ANALOG', idx=10, name='EMBER GLOW', cat='PAD', bseed=44167,  # #44
         p={
            'ATK': 86, 'SUS': 127, 'REL': 105, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 7, 'RES2': 61, 'KTRK': 27, 'CHOR': 32, 'REV': 44, 'TWTYPE': 1,
            'FREEA': 47, 'FREEB': 17, 'FREED': 2, 'E2ATK': 92, 'E2DEC': 105, 'E2SUS': 70, 'E2REL': 101, 'L2RATE': 15, 'ITYPE': 1, 'IA': 80, 'IB': 127,
            'IC': 112, 'CUT2': 51, 'TRIM': 80, 'E2FLT': 17, 'E2': 9, 'MOD1': ('AT', 'CUT2', 11), 'MOD2': ('CYCLE', 'CUT2', 9),
            'MOD3': ('LFO2', 'CUT', 3), 'MOD4': ('LFO2', 'E2', 3), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO2': -11, 'CYCLE RISE': 115,
            'CYCLE FALL': 115,
         }),
    dict(engine='ANALOG', idx=9, name='SILK CHORUS', cat='PAD', bseed=33289,  # #45
         p={
            'ATK': 0, 'DEC': 127, 'SUS': 102, 'REL': 101, 'LRATE': 27, 'LWAVE': 1, 'LD_FLT': 5, 'KTRK': 17, 'CHOR': 102, 'REV': 25, 'FREEA': 51,
            'FREEB': 11, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 127, 'E2SUS': 89, 'E2REL': 101, 'L2RATE': 57, 'L2WAVE': 1, 'TRIM': 88, 'E2FLT': 7,
            'L2SHP': 38, 'E1': 4, 'E2': 0, 'E3': 0, 'MOD1': ('AT', 'CHO', 31), 'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 110,
            'CYCLE FALL': 110,
         }),
    dict(engine='ANALOG', idx=11, name='VELVET BOW', cat='PAD', bseed=34517,  # #46
         p={
            'ATK': 76, 'SUS': 127, 'REL': 88, 'LRATE': 27, 'KTRK': 17, 'CHOR': 108, 'REV': 38, 'FREEA': 43, 'FREEB': 11, 'E2ATK': 90, 'E2DEC': 101,
            'E2SUS': 108, 'E2REL': 88, 'L2RATE': 65, 'L2WAVE': 1, 'TRIM': 89, 'E2FLT': 17, 'L2SHP': 26, 'E1': 4, 'E2': 8, 'MOD1': ('AT', 'FRES', 17),
            'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO1', 'CUT', 4), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 110, 'CYCLE FALL': 110,
         }),
    dict(engine='FM6', idx=2, name='GLASS HALL', cat='PAD', bseed=28240,  # #47
         p={
            'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 5, 'CHOR': 70, 'REV': 70, 'FREEA': 64, 'FREEB': 11, 'E2ATK': 101, 'E2DEC': 108, 'E2SUS': 70,
            'E2REL': 101, 'L2RATE': 34, 'L2WAVE': 1, 'CHRD': 6, 'VOIC': 1, 'ITYPE': 1, 'IA': 100, 'IB': 127, 'IC': 112, 'TRIM': 84, 'E2FLT': 11,
            'E2': 1, 'E3': -8, 'E4': 2, 'E5': 14, 'E7': 38, 'E8': 4, 'MOD1': ('AT', 'E3', 7), 'MOD2': ('CYCLE', 'E3', 5), 'MOD3': ('LFO2', 'E3', 3),
            'MOD4': ('ENV2', 'E3', 5), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 115, 'CYCLE FALL': 115,
         }),
    dict(engine='PHASE', idx=2, name='LAMPLIGHT', cat='PAD', bseed=62877,  # #48
         p={
            'ATK': 92, 'DEC': 103, 'SUS': 117, 'REL': 102, 'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 5, 'RES2': 21, 'CHOR': 70, 'REV': 42, 'TWTYPE': 1,
            'FREEA': 57, 'FREEB': 11, 'E2ATK': 97, 'E2DEC': 105, 'E2SUS': 76, 'E2REL': 98, 'L2RATE': 37, 'L2WAVE': 1, 'CHRD': 2, 'SPRD': 38,
            'ITYPE': 1, 'IA': 70, 'IB': 127, 'IC': 112, 'CUT2': -11, 'TRIM': 79, 'E2FLT': 11, 'E2': 4, 'E3': 46, 'E4': 36, 'E5': 13,
            'MOD1': ('AT', 'SHP', 7), 'MOD2': ('CYCLE', 'SHP', 6), 'MOD3': ('LFO2', 'E3', 3), 'MOD4': ('ENV2', 'E3', 5), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 105, 'CYCLE FALL': 105,
         }),
    dict(engine='LOFI', idx=2, name='WOW FLUTTER', cat='PAD', bseed=65413,  # #49
         p={
            'ATK': 90, 'DEC': 101, 'SUS': 117, 'REL': 100, 'CHOR': 64, 'REV': 70, 'FREEA': 47, 'FREEB': 17, 'E2ATK': 97, 'E2DEC': 105, 'E2SUS': 76,
            'E2REL': 98, 'L2RATE': 34, 'L2WAVE': 1, 'CHRD': 7, 'VOIC': 1, 'SPRD': 38, 'ITYPE': 9, 'IA': 52, 'IB': 40, 'IC': 78, 'TRIM': 102,
            'E2FLT': 17, 'L2FLT': 5, 'E1': 1, 'E2': 4, 'E3': 92, 'E8': 52, 'MOD1': ('AT', 'E8', 21), 'MOD2': ('CYCLE', 'CUT', 8),
            'MOD3': ('LFO2', 'E8', 6), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 115, 'CYCLE FALL': 115,
         }),
    dict(engine='VOICE', idx=6, name='PLATE CHOIR', cat='PAD', bseed=64679,  # #50
         p={
            'ATK': 94, 'DEC': 101, 'SUS': 117, 'REL': 101, 'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 4, 'CHOR': 38, 'REV': 114, 'FREEA': 61, 'FREEB': 7,
            'E2ATK': 97, 'E2DEC': 105, 'E2SUS': 89, 'E2REL': 101, 'L2RATE': 27, 'L2WAVE': 1, 'CHRD': 1, 'VOIC': 1, 'SPRD': 51, 'TRIM': 95, 'E2FLT': 11,
            'L2SHP': 8, 'E1': 10, 'E2': 60, 'E3': 40, 'E5': 38, 'E6': 14, 'E7': 52, 'E8': 6, 'MOD1': ('AT', 'E4', 4), 'MOD2': ('CYCLE', 'E2', 6),
            'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 110, 'CYCLE FALL': 110,
         }),
    dict(engine='VOICE', idx=1, name='VESPERS', cat='PAD', bseed=34110,  # #51
         p={
            'ATK': 90, 'DEC': 101, 'SUS': 117, 'REL': 100, 'LRATE': 18, 'LWAVE': 1, 'LD_FLT': 4, 'CHOR': 57, 'REV': 89, 'FREEA': 57, 'FREEB': 11,
            'E2ATK': 100, 'E2DEC': 105, 'E2SUS': 76, 'E2REL': 95, 'L2RATE': 87, 'L2WAVE': 1, 'L2FADE': 94, 'CHRD': 1, 'VOIC': 1, 'SPRD': 51,
            'TRIM': 86, 'E2FLT': 17, 'L2PIT': 1, 'E2': 95, 'E3': 20, 'E5': 42, 'E6': 21, 'E7': 58, 'E8': 14, 'MOD1': ('AT', 'E6', 24),
            'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO1', 'E2', 6), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 107, 'CYCLE FALL': 112,
         }),
    dict(engine='VOICE', idx=5, name='WHISPER PAD', cat='PAD', bseed=40791,  # #52
         p={
            'ATK': 92, 'DEC': 101, 'SUS': 110, 'REL': 101, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 5, 'CHOR': 52, 'DLY': 22, 'REV': 90, 'FREEA': 61,
            'FREEB': 17, 'E2ATK': 98, 'E2DEC': 105, 'E2SUS': 89, 'E2REL': 101, 'L2RATE': 15, 'SPRD': 47, 'TRIM': 99, 'E2FLT': 11, 'E2': 95, 'E3': 88,
            'E4': 3, 'E5': 50, 'E6': 120, 'E7': 50, 'E8': 11, 'MOD1': ('AT', 'SHP', 16), 'MOD2': ('CYCLE', 'E6', -8), 'MOD3': ('LFO2', 'E2', 4),
            'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 105, 'CYCLE FALL': 109,
         }),
    dict(engine='TRIO', idx=2, name='CHIP CHAPEL', cat='PAD', bseed=18034,  # #53
         p={
            'ATK': 94, 'DEC': 101, 'SUS': 117, 'REL': 101, 'LRATE': 35, 'LWAVE': 1, 'LD_FLT': 4, 'RES2': 85, 'KTRK': 0, 'CHOR': 57, 'REV': 70,
            'TWTYPE': 2, 'FREEA': 57, 'FREEB': 11, 'E2ATK': 98, 'E2DEC': 105, 'E2SUS': 76, 'E2REL': 98, 'L2RATE': 50, 'L2WAVE': 1, 'CHRD': 1,
            'VOIC': 1, 'SPRD': 44, 'CUT2': 43, 'TRIM': 96, 'E2FLT': 11, 'L2SHP': 29, 'E1': 1, 'E3': 12, 'E4': 7, 'E8': 40, 'MOD1': ('AT', 'CUT2', 12),
            'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO1', 'CUT2', 2), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 8, 'CYCLE RISE': 100,
            'CYCLE FALL': 105,
         }),
    dict(engine='TRIO', idx=9, name='STRING MACH', cat='PAD', bseed=52771,  # #54
         p={
            'ATK': 77, 'DEC': 101, 'SUS': 127, 'REL': 97, 'LRATE': 40, 'LWAVE': 1, 'LD_SHP': 12, 'KTRK': 0, 'CHOR': 90, 'REV': 52, 'TWTYPE': 3,
            'FREEA': 61, 'FREEB': 7, 'E2ATK': 85, 'E2DEC': 101, 'E2SUS': 89, 'E2REL': 94, 'L2RATE': 23, 'SPRD': 34, 'CUT2': 11, 'TRIM': 90, 'E2FLT': 7,
            'E1': 1, 'E2': 12, 'E3': 0, 'E4': 7, 'E8': 17, 'MOD1': ('AT', 'E4', 16), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO2', 'CUT2', 2),
            'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'CYCLE RISE': 107, 'CYCLE FALL': 108,
         }),
    dict(engine='WHEEL', idx=2, name='CHOIR LOFT', cat='PAD', bseed=8609,  # #55
         p={
            'ATK': 90, 'SUS': 127, 'REL': 100, 'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 4, 'CHOR': 51, 'REV': 70, 'FREEA': 47, 'FREEB': 11, 'E2ATK': 98,
            'E2DEC': 105, 'E2SUS': 76, 'E2REL': 98, 'L2RATE': 31, 'L2WAVE': 1, 'CHRD': 1, 'TRIM': 102, 'E2FLT': 14, 'E2SHP': 16, 'L2SHP': 3, 'E1': 12,
            'E4': 2, 'E6': 0, 'MOD1': ('AT', 'SHP', 5), 'MOD2': ('CYCLE', 'CUT', 6), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 115, 'CYCLE FALL': 119,
         }),
    dict(engine='PHYS', idx=1, name='SINGING BOWL', cat='PAD', bseed=64412,  # #56
         p={
            'ATK': 69, 'SUS': 127, 'REL': 103, 'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 4, 'CHOR': 38, 'DLY': 19, 'REV': 76, 'FREEA': 57, 'FREEB': 11,
            'E2ATK': 79, 'E2DEC': 108, 'E2SUS': 89, 'E2REL': 101, 'L2RATE': 21, 'L2WAVE': 1, 'CHRD': 9, 'VOIC': 1, 'TRIM': 127, 'E2FLT': 14,
            'L2FLT': 4, 'E2': 92, 'E3': 66, 'E4': 92, 'E5': 10, 'E6': 80, 'E7': 100, 'MOD1': ('AT', 'E7', 10), 'MOD2': ('CYCLE', 'CUT', 7),
            'MOD3': ('LFO2', 'E3', 4), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 112, 'CYCLE FALL': 112,
         }),
    dict(engine='NOISE', idx=2, name='SEA ORGAN', cat='PAD', bseed=1234,  # #57
         p={
            'ATK': 97, 'SUS': 127, 'REL': 108, 'LRATE': 27, 'RES2': 61, 'KTRK': 63, 'CHOR': 51, 'REV': 83, 'TWTYPE': 2, 'FREEA': 47, 'FREEB': 91,
            'FREEC': 64, 'E2ATK': 101, 'E2DEC': 105, 'E2SUS': 76, 'E2REL': 98, 'L2RATE': 23, 'L2WAVE': 1, 'CHRD': 1, 'VOIC': 1, 'SPRD': 51, 'CUT2': 41,
            'TRIM': 107, 'E2FLT': 7, 'L2FLT': 4, 'E7': 47, 'MOD1': ('AT', 'FRES', 24), 'MOD2': ('CYCLE', 'CUT2', 14), 'MOD3': ('LFO1', 'E2', 2),
            'LTRIG': 1, 'L2TRIG': 1, 'TWIN ROUTE': 1, 'TWIN LINK': 0, 'TWIN KTRK2': 63, 'TWIN LFO2': 8, 'CYCLE RISE': 115, 'CYCLE FALL': 115,
         }),
    dict(engine='WAVE', idx=20, name='BRASS PAD', cat='PAD', bseed=17892,  # #58
         p={
            'ATK': 82, 'DEC': 101, 'SUS': 127, 'REL': 98, 'LRATE': 23, 'LWAVE': 1, 'CHOR': 47, 'REV': 65, 'FREEA': 41, 'FREEB': 11, 'E2ATK': 88,
            'E2DEC': 101, 'E2SUS': 76, 'E2REL': 97, 'L2RATE': 87, 'L2FADE': 100, 'CHRD': 9, 'VOIC': 1, 'SPRD': 34, 'TRIM': 106, 'E2FLT': 24,
            'E2SHP': 24, 'L2PIT': 1, 'E1': 6, 'E2': 31, 'E6': 9, 'MOD1': ('AT', 'CUT', 16), 'MOD2': ('CYCLE', 'SHP', 16), 'MOD3': ('LFO1', 'E6', 11),
            'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 91, 'CYCLE FALL': 127,
         }),
    dict(engine='WAVE', idx=15, name='BREATH PAD', cat='PAD', bseed=60243,  # #59
         p={
            'ATK': 79, 'DEC': 101, 'SUS': 127, 'REL': 94, 'LRATE': 23, 'LWAVE': 1, 'DIST': 7, 'CHOR': 32, 'REV': 70, 'FREEA': 51, 'FREEB': 41,
            'E2ATK': 84, 'E2DEC': 101, 'E2SUS': 89, 'E2REL': 94, 'L2RATE': 89, 'L2FADE': 88, 'SPRD': 32, 'TRIM': 109, 'E2FLT': 14, 'L2FLT': 1,
            'E1': 25, 'E2': 30, 'E6': 6, 'MOD1': ('AT', 'SHP', 12), 'MOD2': ('CYCLE', 'CUT', 6), 'MOD3': ('LFO1', 'E2', 24), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 105, 'CYCLE FALL': 109,
         }),
    dict(engine='WAVE', idx=14, name='DIGITAL PAD', cat='PAD', bseed=56298,  # #60
         p={
            'ATK': 83, 'DEC': 105, 'SUS': 127, 'REL': 101, 'LRATE': 27, 'CHOR': 44, 'REV': 64, 'FREEA': 57, 'FREEB': 17, 'FREED': 2, 'E2ATK': 108,
            'E2DEC': 110, 'E2SUS': 64, 'E2REL': 105, 'L2RATE': 17, 'L2WAVE': 1, 'LPOL': 1, 'SPRD': 38, 'E2FLT': 17, 'E2SHP': 38, 'L2SHP': 6, 'E1': 14,
            'E2': 10, 'E6': 7, 'MOD1': ('AT', 'E3', 8), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO1', 'E3', 13), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 110, 'CYCLE FALL': 113,
         }),
    dict(engine='WAVE', idx=3, name='EVOLVING PAD', cat='PAD', bseed=42897,  # #61
         p={
            'ATK': 105, 'DEC': 110, 'SUS': 127, 'REL': 117, 'LRATE': 6, 'LWAVE': 1, 'LD_SHP': 19, 'DLY': 32, 'REV': 76, 'FREEA': 61, 'FREEB': 11,
            'E2ATK': 108, 'E2DEC': 110, 'E2SUS': 89, 'E2REL': 110, 'L2RATE': 23, 'SPRD': 38, 'TRIM': 111, 'E2FLT': 11, 'L2FLT': 10, 'L2SHP': 13,
            'E1': 9, 'E2': 64, 'E6': 7, 'MOD1': ('AT', 'DLY', 30), 'MOD2': ('CYCLE', 'E3', 12), 'MOD3': ('LFO1', 'SUBLV', 24), 'LTRIG': 1, 'L2TRIG': 1,
            'SUB LEVEL': 39, 'CYCLE RISE': 117, 'CYCLE FALL': 120,
         }),
    dict(engine='WAVE', idx=2, name='GLASS PAD', cat='PAD', bseed=55575,  # #62
         p={
            'ATK': 90, 'DEC': 105, 'SUS': 127, 'REL': 105, 'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 4, 'CHOR': 38, 'REV': 57, 'FREEA': 61, 'FREEB': 7,
            'E2ATK': 101, 'E2DEC': 108, 'E2SUS': 51, 'E2REL': 101, 'L2RATE': 32, 'L2POL': 1, 'SPRD': 32, 'TRIM': 103, 'E2FLT': 11, 'E2SHP': 13,
            'E1': 11, 'E2': 34, 'E6': 6, 'MOD1': ('AT', 'CHO', 30), 'MOD2': ('CYCLE', 'SHP', 8), 'MOD3': ('LFO2', 'E3', 13), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 113, 'CYCLE FALL': 117,
         }),
    dict(engine='WAVE', idx=0, name='PWM PAD', cat='PAD', bseed=44102,  # #63
         p={
            'ATK': 79, 'DEC': 101, 'SUS': 127, 'REL': 92, 'LRATE': 23, 'LWAVE': 1, 'CHOR': 57, 'REV': 44, 'FREEA': 51, 'FREEB': 11, 'E2ATK': 94,
            'E2DEC': 105, 'E2SUS': 89, 'E2REL': 98, 'L2RATE': 47, 'L2WAVE': 1, 'LPOL': 1, 'SPRD': 38, 'TRIM': 117, 'E2FLT': 14, 'L2SHP': 19, 'E1': 1,
            'E2': 40, 'E6': 8, 'MOD1': ('AT', 'CUT', 12), 'MOD2': ('CYCLE', 'CUT', 8), 'MOD3': ('LFO1', 'E3', 17), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 113, 'CYCLE FALL': 115,
         }),
    dict(engine='WAVE', idx=1, name='VOWEL PAD', cat='PAD', bseed=38170,  # #64
         p={
            'ATK': 88, 'DEC': 101, 'SUS': 127, 'REL': 98, 'LRATE': 18, 'LWAVE': 1, 'CHOR': 51, 'REV': 76, 'FREEA': 57, 'FREEB': 11, 'E2ATK': 97,
            'E2DEC': 105, 'E2SUS': 89, 'E2REL': 98, 'L2RATE': 21, 'L2WAVE': 1, 'LPOL': 1, 'SPRD': 38, 'TRIM': 109, 'E2FLT': 11, 'L2SHP': 8, 'E1': 5,
            'E2': 8, 'E6': 10, 'MOD1': ('AT', 'SHP', 8), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO1', 'SUBLV', 17), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 111, 'CYCLE FALL': 111,
         }),
    dict(engine='ANALOG', idx=18, name='ARP PLUCK', cat='PLUCK', bseed=12417,  # #65
         p={
            'ATK': 0, 'DEC': 77, 'SUS': 0, 'REL': 61, 'LRATE': 27, 'LWAVE': 1, 'LD_FLT': 5, 'AMODE': 1, 'AOCT': 2, 'AGATE': 76, 'RES2': 71, 'KTRK': 27,
            'CHOR': 28, 'DLY': 30, 'REV': 23, 'TWTYPE': 5, 'FREEA': 41, 'FREEB': 17, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 61, 'E2SUS': 0, 'E2REL': 61,
            'L2RATE': 17, 'L2WAVE': 1, 'CUT2': 40, 'TRIM': 103, 'E2FLT': 31, 'E2': 17, 'MOD1': ('AT', 'FRES', 17), 'MOD2': ('VEL', 'CUT', 8),
            'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 37, 'TWIN LFO2': 7, 'CYCLE RISE': 103,
            'CYCLE FALL': 103,
         }),
    dict(engine='ANALOG', idx=12, name='PIZZICATO', cat='PLUCK', bseed=65304,  # #66
         p={
            'ATK': 0, 'DEC': 95, 'SUS': 0, 'REL': 73, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 4, 'KTRK': 27, 'CHOR': 38, 'REV': 32, 'FREEA': 43,
            'FREEB': 11, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 71, 'E2SUS': 0, 'E2REL': 61, 'TRIM': 99, 'E2FLT': 31, 'E2': 0, 'E3': 0,
            'MOD1': ('AT', 'DLY', 27), 'MOD2': ('VEL', 'CUT', 10), 'MOD3': ('CYCLE', 'CUT', 11), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1,
            'CYCLE RISE': 0, 'CYCLE FALL': 71,
         }),
    dict(engine='FM6', idx=9, name='ISLAND STEEL', cat='PLUCK', bseed=19220,  # #67
         p={
            'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 5, 'KTRK': 31, 'DLY': 18, 'REV': 34, 'FREEA': 62, 'FREEB': 11, 'E2ATK': 0, 'E2DEC': 67, 'E2SUS': 0,
            'E2REL': 61, 'TRIM': 118, 'E2FLT': 7, 'E3': -7, 'E5': 11, 'E6': 3, 'E7': 14, 'E8': 7, 'MOD1': ('AT', 'E3', 7), 'MOD2': ('CYCLE', 'E3', 4),
            'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 41, 'SUB OCTAVE': 0, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 100, 'CYCLE CURVE': -17,
         }),
    dict(engine='FM6', idx=3, name='MALLET WOOD', cat='PLUCK', bseed=12489,  # #68
         p={
            'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 4, 'KTRK': 31, 'REV': 15, 'FREEA': 58, 'FREEB': 11, 'E2ATK': 0, 'E2DEC': 61, 'E2SUS': 0, 'E2REL': 51,
            'TRIM': 124, 'E2FLT': 11, 'E3': 14, 'E5': 25, 'E6': 4, 'E8': 5, 'MOD1': ('AT', 'E5', 11), 'MOD2': ('CYCLE', 'E3', 7), 'LTRIG': 1,
            'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 71,
         }),
    dict(engine='PHASE', idx=8, name='FAKE KOTO', cat='PLUCK', bseed=44371,  # #69
         p={
            'ATK': 0, 'DEC': 97, 'SUS': 0, 'REL': 77, 'LRATE': 21, 'LWAVE': 1, 'LD_SHP': 5, 'KTRK': 31, 'DLY': 14, 'REV': 34, 'FREEA': 64, 'FREEB': 21,
            'E2ATK': 0, 'E2DEC': 59, 'E2SUS': 0, 'E2REL': 47, 'L2RATE': 27, 'L2WAVE': 1, 'TRIM': 95, 'E2FLT': 11, 'L2FLT': 4, 'E3': 37, 'E4': 71,
            'MOD1': ('AT', 'SAT', 21), 'MOD2': ('ENV2', 'PITCH', 7), 'MOD3': ('CYCLE', 'SHP', 11), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1,
            'CYCLE RISE': 0, 'CYCLE FALL': 77,
         }),
    dict(engine='PHASE', idx=3, name='WIRE HARP', cat='PLUCK', bseed=12692,  # #70
         p={
            'ATK': 0, 'DEC': 86, 'SUS': 38, 'REL': 70, 'LRATE': 40, 'LWAVE': 1, 'LD_SHP': 4, 'AMODE': 1, 'ARATE': 1, 'AOCT': 2, 'AGATE': 76,
            'KTRK': 27, 'CHOR': 25, 'DLY': 25, 'REV': 51, 'FREEA': 60, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 71, 'E2SUS': 0, 'E2REL': 66, 'L2RATE': 17,
            'L2WAVE': 1, 'TRIM': 95, 'E2FLT': 11, 'L2FLT': 5, 'E1': 3, 'E2': 7, 'E3': 40, 'E4': 70, 'E5': 7, 'MOD1': ('AT', 'SHP', 7),
            'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 105, 'CYCLE FALL': 105,
         }),
    dict(engine='LOFI', idx=3, name='MARBLE RUN', cat='PLUCK', bseed=60893,  # #71
         p={
            'ATK': 0, 'DEC': 79, 'SUS': 0, 'REL': 69, 'EQLG': -3, 'AMODE': 1, 'ARATE': 1, 'AOCT': 2, 'AGATE': 57, 'DLY': 38, 'REV': 44, 'FREEB': 40,
            'E2ATK': 0, 'E2DEC': 74, 'E2SUS': 0, 'E2REL': 64, 'TRIM': 112, 'E2': 1, 'E8': 96, 'LTRIG': 1, 'L2TRIG': 1,
         }),
    dict(engine='VOICE', idx=2, name='DOO WOP', cat='PLUCK', bseed=29492,  # #72
         p={
            'ATK': 0, 'DEC': 74, 'SUS': 0, 'REL': 69, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 5, 'AMODE': 3, 'ARATE': 1, 'AOCT': 2, 'AGATE': 70, 'KTRK': 17,
            'CHOR': 23, 'DLY': 28, 'REV': 44, 'FREEA': 61, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 72, 'E2SUS': 0, 'E2REL': 64, 'L2RATE': 40, 'L2WAVE': 1,
            'TRIM': 98, 'E2FLT': 11, 'L2SHP': 7, 'E1': 95, 'E2': 0, 'E3': 88, 'E5': 70, 'E6': 12, 'E7': 92, 'MOD1': ('AT', 'E4', 7),
            'MOD2': ('CYCLE', 'CUT', 11), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 71,
         }),
    dict(engine='TRIO', idx=4, name='PEBBLE POP', cat='PLUCK', bseed=63877,  # #73
         p={
            'ATK': 0, 'DEC': 71, 'SUS': 0, 'REL': 59, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 5, 'AMODE': 1, 'AOCT': 2, 'AGATE': 70, 'RES2': 31, 'KTRK': 17,
            'DLY': 38, 'REV': 25, 'TWTYPE': 1, 'FREEA': 66, 'FREEB': 17, 'E2ATK': 0, 'E2SUS': 0, 'L2RATE': 40, 'L2WAVE': 1, 'CUT2': 47, 'TRIM': 102,
            'E2FLT': 11, 'E2SHP': 19, 'L2SHP': 11, 'E1': 2, 'E2': 12, 'E3': 0, 'E4': 7, 'E8': 22, 'MOD1': ('AT', 'E4', 17),
            'MOD2': ('VEL', 'CUT2', 10), 'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 27, 'CYCLE RISE': 103,
            'CYCLE FALL': 103,
         }),
    dict(engine='WHEEL', idx=4, name='NIGHT BOX', cat='PLUCK', bseed=32458,  # #74
         p={
            'ATK': 0, 'DEC': 81, 'SUS': 0, 'REL': 72, 'EQLG': -6, 'LRATE': 27, 'LWAVE': 1, 'LD_FLT': 5, 'AMODE': 1, 'ARATE': 1, 'AOCT': 2, 'KTRK': 31,
            'DLY': 25, 'REV': 51, 'FREEA': 66, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 76, 'E2SUS': 0, 'E2REL': 64, 'L2RATE': 21, 'L2WAVE': 1, 'TRIM': 111,
            'E2FLT': 11, 'E2SHP': 19, 'L2SHP': 2, 'E1': 8, 'E3': -4, 'E4': 2, 'E6': 20, 'E8': 0, 'MOD1': ('AT', 'E7', 17),
            'MOD2': ('CYCLE', 'CUT', 11), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 71,
         }),
    dict(engine='PHYS', idx=2, name='GHOST HARP', cat='PLUCK', bseed=40612,  # #75
         p={
            'ATK': 0, 'SUS': 127, 'REL': 76, 'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 5, 'KTRK': 31, 'CHOR': 25, 'REV': 51, 'FREEA': 62, 'FREEB': 11,
            'E2ATK': 0, 'E2DEC': 79, 'E2SUS': 0, 'E2REL': 73, 'L2RATE': 10, 'L2WAVE': 1, 'TRIM': 113, 'E2FLT': 7, 'E1': 3, 'E2': 8, 'E3': 67,
            'E4': 104, 'E5': 70, 'E7': 30, 'MOD1': ('AT', 'E5', 17), 'MOD2': ('LFO2', 'E3', 5), 'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 110, 'CYCLE FALL': 110,
         }),
    dict(engine='PHYS', idx=9, name='NURSERY KEYS', cat='PLUCK', bseed=13705,  # #76
         p={
            'ATK': 0, 'SUS': 127, 'REL': 71, 'LRATE': 27, 'LWAVE': 1, 'LD_FLT': 4, 'KTRK': 17, 'DLY': 14, 'REV': 27, 'FREEA': 67, 'FREEB': 17,
            'E2ATK': 0, 'E2DEC': 61, 'E2SUS': 0, 'E2REL': 51, 'L2RATE': 30, 'L2WAVE': 1, 'TRIM': 113, 'E2FLT': 11, 'E2': 124, 'E3': 91, 'E4': 71,
            'E5': 10, 'E6': 107, 'E8': 21, 'MOD1': ('AT', 'E3', 11), 'MOD2': ('LFO2', 'E4', 5), 'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 71,
         }),
    dict(engine='PHYS', idx=3, name='NYLON GUT', cat='PLUCK', bseed=53812,  # #77
         p={
            'ATK': 0, 'SUS': 127, 'LRATE': 27, 'LWAVE': 1, 'LD_FLT': 5, 'AMODE': 1, 'AOCT': 2, 'AGATE': 108, 'KTRK': 31, 'CHOR': 23, 'REV': 32,
            'FREEA': 56, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 79, 'E2SUS': 0, 'E2REL': 69, 'L2RATE': 17, 'L2WAVE': 1, 'TRIM': 121, 'E2FLT': 11, 'E1': 1,
            'E2': 56, 'E3': 58, 'E4': 110, 'E5': 40, 'E6': 120, 'MOD1': ('AT', 'REV', 27), 'MOD2': ('LFO2', 'E5', 5), 'MOD3': ('CYCLE', 'CUT', 11),
            'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 71,
         }),
    dict(engine='WAVE', idx=6, name='DIGI PLUCK', cat='PLUCK', bseed=63098,  # #78
         p={
            'ATK': 0, 'DEC': 90, 'SUS': 0, 'REL': 76, 'ED_SHP': 51, 'LRATE': 27, 'LWAVE': 1, 'LD_SHP': 9, 'KTRK': 27, 'CHOR': 25, 'DLY': 23, 'REV': 32,
            'FREEA': 58, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 84, 'E2SUS': 0, 'E2REL': 69, 'L2RATE': 27, 'L2WAVE': 1, 'TRIM': 124, 'E2FLT': 11,
            'L2FLT': 5, 'E1': 10, 'E2': 20, 'MOD1': ('AT', 'SHP', 17), 'MOD2': ('VEL', 'SHP', 16), 'MOD3': ('ENV2', 'E6', 17),
            'MOD4': ('CYCLE', 'SHP', 9), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 77,
         }),
    dict(engine='ANALOG', idx=13, name='SFORZANDO', cat='KEYS', bseed=46167,  # #79
         p={
            'ATK': 22, 'DEC': 79, 'SUS': 70, 'REL': 66, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 4, 'KTRK': 17, 'CHOR': 32, 'REV': 25, 'FREEA': 31,
            'FREEB': 17, 'E2ATK': 32, 'E2DEC': 76, 'E2SUS': 57, 'E2REL': 66, 'TRIM': 101, 'E2FLT': 37, 'E2': 7, 'MOD1': ('AT', 'CUT', 11),
            'MOD2': ('VEL', 'CUT', 10), 'MOD3': ('CYCLE', 'CUT', 5), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         }),
    dict(engine='FM6', idx=5, name='BIG STAB', cat='KEYS', bseed=37822,  # #80
         p={
            'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 5, 'KTRK': 17, 'DIST': 25, 'CHOR': 32, 'REV': 25, 'FREEA': 64, 'FREEB': 11, 'E2ATK': 22, 'E2DEC': 86,
            'E2SUS': 76, 'E2REL': 66, 'TRIM': 111, 'E2FLT': 17, 'E2': 1, 'E3': -8, 'E6': 4, 'E8': 3, 'MOD1': ('AT', 'E7', 31),
            'MOD2': ('ENV2', 'E3', 17), 'MOD3': ('CYCLE', 'E3', 4), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         }),
    dict(engine='FM6', idx=6, name='FROST BELLS', cat='KEYS', bseed=48288,  # #81
         p={
            'EQLG': -5, 'CHOR': 13, 'REV': 70, 'FREEB': 40, 'E2ATK': 0, 'E2DEC': 98, 'E2SUS': 0, 'E2REL': 88, 'ITYPE': 1, 'IA': 40, 'IB': 127,
            'IC': 112, 'TRIM': 97, 'E6': 2, 'E7': 13, 'E8': 1, 'MOD1': ('AT', 'E3', 4), 'MOD2': ('CYCLE', 'E3', 4), 'LTRIG': 1, 'L2TRIG': 1,
            'SUB LEVEL': 51, 'SUB OCTAVE': 0, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 100, 'CYCLE CURVE': -21,
         }),
    dict(engine='FM6', idx=10, name='SALOON KEYS', cat='KEYS', bseed=28718,  # #82
         p={
            'KTRK': 27, 'CHOR': 9, 'REV': 18, 'FREEA': 57, 'FREEB': 11, 'E2ATK': 0, 'E2DEC': 83, 'E2SUS': 0, 'E2REL': 71, 'L2RATE': 27, 'TRIM': 108,
            'E2FLT': 17, 'E3': 7, 'E6': 3, 'E7': 71, 'MOD1': ('AT', 'E5', 7), 'MOD2': ('ENV2', 'E3', 7), 'MOD3': ('LFO2', 'E7', 5),
            'MOD4': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 100, 'CYCLE FALL': 103,
         }),
    dict(engine='FM6', idx=4, name='TINE ROOM', cat='KEYS', bseed=14977,  # #83
         p={
            'KTRK': 27, 'CHOR': 51, 'REV': 28, 'FREEA': 64, 'FREEB': 7, 'E2ATK': 0, 'E2DEC': 86, 'E2SUS': 0, 'E2REL': 73, 'L2RATE': 81, 'TRIM': 107,
            'E2FLT': 11, 'E5': 30, 'E6': 3, 'E7': 13, 'MOD1': ('AT', 'E7', 17), 'MOD2': ('ENV2', 'E3', 7), 'MOD3': ('LFO2', 'AMP', 9),
            'MOD4': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 51, 'SUB OCTAVE': 0, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         }),
    dict(engine='FM6', idx=13, name='TUBE BELLS', cat='KEYS', bseed=22916,  # #84
         p={
            'CHOR': 8, 'REV': 38, 'FREEB': 40, 'TRIM': 107, 'E8': 42, 'MOD1': ('AT', 'DLY', 31), 'MOD2': ('CYCLE', 'E3', 4), 'LTRIG': 1, 'L2TRIG': 1,
            'SUB LEVEL': 41, 'SUB OCTAVE': 0, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 105, 'CYCLE CURVE': -31,
         }),
    dict(engine='PHASE', idx=4, name='CLAVICLE', cat='KEYS', bseed=5811,  # #85
         p={
            'ATK': 0, 'DEC': 75, 'SUS': 23, 'REL': 54, 'LRATE': 40, 'LD_SHP': 6, 'KTRK': 27, 'CHOR': 15, 'REV': 15, 'FREEA': 56, 'FREEB': 17,
            'E2ATK': 0, 'E2DEC': 66, 'E2SUS': 0, 'E2REL': 51, 'TRIM': 105, 'E2FLT': 31, 'E1': 1, 'E3': 50, 'E4': 61, 'MOD1': ('AT', 'SHP', 7),
            'MOD2': ('CYCLE', 'SHP', 11), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 77,
         }),
    dict(engine='TRIO', idx=3, name='TIN TOLL', cat='KEYS', bseed=43504,  # #86
         p={
            'ATK': 0, 'DEC': 99, 'SUS': 0, 'REL': 92, 'EQLG': -5, 'RES2': 22, 'KTRK': 0, 'CHOR': 13, 'REV': 64, 'TWTYPE': 1, 'FREEA': 56, 'FREEB': 17,
            'E2ATK': 0, 'E2DEC': 86, 'E2SUS': 0, 'E2REL': 79, 'L2RATE': 32, 'CUT2': 67, 'TRIM': 102, 'E2FLT': 17, 'L2FLT': 4, 'E1': 13, 'E3': 17,
            'E4': 7, 'MOD1': ('AT', 'CUT2', 7), 'MOD2': ('VEL', 'CUT2', 10), 'MOD3': ('CYCLE', 'CUT2', 11), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0,
            'TWIN ENV2': 10, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 95, 'CYCLE CURVE': -21,
         }),
    dict(engine='WHEEL', idx=3, name='JAZZ CLUB', cat='KEYS', bseed=57778,  # #87
         p={
            'SUS': 127, 'REL': 44, 'REV': 19, 'FREEA': 67, 'E2ATK': 0, 'E2DEC': 73, 'E2SUS': 0, 'E2REL': 47, 'L2RATE': 23, 'E2SHP': 8, 'E5': 2,
            'E6': 45, 'E7': 12, 'MOD1': ('AT', 'E7', 19), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO2', 'SHP', 3), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 103, 'CYCLE FALL': 105,
         }),
    dict(engine='PHYS', idx=10, name='FAUX HARPSI', cat='KEYS', bseed=48968,  # #88
         p={
            'ATK': 0, 'SUS': 127, 'REL': 59, 'KTRK': 27, 'REV': 22, 'FREEA': 64, 'FREEB': 11, 'E2ATK': 0, 'E2DEC': 71, 'E2SUS': 0, 'E2REL': 56,
            'L2RATE': 23, 'ITYPE': 1, 'IA': 100, 'IB': 127, 'IC': 112, 'TRIM': 111, 'E2FLT': 11, 'L2FLT': 4, 'E1': 1, 'E2': 71, 'E3': 107, 'E4': 44,
            'E5': 11, 'E6': 127, 'MOD1': ('AT', 'E3', 7), 'MOD2': ('CYCLE', 'E5', 11), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 0,
            'CYCLE FALL': 77,
         }),
    dict(engine='PHYS', idx=5, name='MOTOR VIBES', cat='KEYS', bseed=34914,  # #89
         p={
            'ATK': 0, 'SUS': 127, 'REL': 83, 'EQLG': -4, 'KTRK': 27, 'DLY': 18, 'REV': 38, 'FREEA': 60, 'FREEB': 11, 'E2ATK': 0, 'E2DEC': 103,
            'E2SUS': 0, 'E2REL': 83, 'L2RATE': 89, 'TRIM': 120, 'E2FLT': 17, 'E2': 110, 'E3': 77, 'E4': 127, 'E5': 18, 'E6': 80, 'E8': 7,
            'MOD1': ('AT', 'E5', 17), 'MOD2': ('LFO2', 'AMP', 17), 'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 100,
            'CYCLE FALL': 103,
         }),
    dict(engine='PHYS', idx=4, name='ROSEWOOD', cat='KEYS', bseed=55371,  # #90
         p={
            'ATK': 0, 'SUS': 127, 'REL': 59, 'LRATE': 40, 'LD_FLT': 4, 'RES2': 21, 'KTRK': 27, 'REV': 14, 'TWTYPE': 1, 'FREEA': 41, 'FREEB': 17,
            'FREED': 2, 'E2ATK': 0, 'E2DEC': 71, 'E2SUS': 0, 'E2REL': 56, 'L2RATE': 23, 'CUT2': 47, 'TRIM': 127, 'E2FLT': 31, 'E2': 116, 'E3': 47,
            'E4': 57, 'E5': 24, 'E6': 100, 'E8': 47, 'MOD1': ('AT', 'E4', 11), 'MOD2': ('CYCLE', 'E5', 17), 'MOD3': ('LFO2', 'E3', 4), 'LTRIG': 1,
            'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 17, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 71,
         }),
    dict(engine='WAVE', idx=4, name='DRAWBARS', cat='KEYS', bseed=57282,  # #91
         p={
            'DEC': 64, 'SUS': 127, 'REL': 44, 'LRATE': 32, 'LD_SHP': 6, 'DIST': 7, 'CHOR': 44, 'REV': 25, 'FREEA': 67, 'E2ATK': 0, 'E2DEC': 79,
            'E2SUS': 0, 'E2REL': 47, 'TRIM': 99, 'E2SHP': 16, 'E1': 4, 'E2': 32, 'MOD1': ('AT', 'SAT', 17), 'MOD2': ('ENV2', 'E3', 11),
            'MOD3': ('CYCLE', 'CHO', 11), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 98, 'CYCLE RISE': 110, 'CYCLE FALL': 110,
         }),
    dict(engine='WAVE', idx=16, name='PLATE BELL', cat='KEYS', bseed=19014,  # #92
         p={
            'ATK': 0, 'DEC': 109, 'SUS': 0, 'REL': 106, 'ED_SHP': 45, 'KTRK': 27, 'CHOR': 15, 'REV': 64, 'FREEA': 55, 'FREEB': 11, 'E2ATK': 0,
            'E2DEC': 100, 'E2SUS': 0, 'E2REL': 88, 'L2RATE': 23, 'TRIM': 108, 'E2FLT': 21, 'L2SHP': 5, 'E1': 23, 'E2': 30, 'MOD1': ('AT', 'REV', 31),
            'MOD2': ('VEL', 'SHP', 13), 'MOD3': ('AENV', 'E3', 13), 'MOD4': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1,
            'CYCLE RISE': 0, 'CYCLE FALL': 100, 'CYCLE CURVE': -21,
         }),
    dict(engine='WAVE', idx=9, name='SYNTH BRASS', cat='KEYS', bseed=30305,  # #93
         p={
            'ATK': 44, 'DEC': 83, 'SUS': 114, 'REL': 64, 'LRATE': 27, 'LD_FLT': 4, 'KTRK': 27, 'CHOR': 32, 'REV': 25, 'FREEA': 44, 'FREEB': 11,
            'E2ATK': 56, 'E2DEC': 83, 'E2SUS': 64, 'E2REL': 69, 'L2RATE': 87, 'L2FADE': 100, 'TRIM': 111, 'E2FLT': 24, 'E2SHP': 32, 'L2PIT': 1,
            'E1': 6, 'E2': 30, 'E6': 8, 'MOD1': ('AT', 'CUT', 17), 'MOD2': ('VEL', 'SHP', 10), 'MOD3': ('ENV2', 'E6', 11),
            'MOD4': ('CYCLE', 'SHP', 11), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 91, 'CYCLE FALL': 127,
         }),
    dict(engine='WAVE', idx=5, name='WAVE CHIMES', cat='KEYS', bseed=28858,  # #94
         p={
            'ATK': 0, 'DEC': 109, 'SUS': 0, 'REL': 106, 'ED_SHP': 61, 'KTRK': 27, 'CHOR': 15, 'REV': 64, 'FREEA': 70, 'FREEB': 7, 'E2ATK': 0,
            'E2DEC': 100, 'E2SUS': 0, 'E2REL': 88, 'L2RATE': 27, 'TRIM': 105, 'E2FLT': 11, 'L2FLT': 5, 'E1': 12, 'E2': 30, 'MOD1': ('AT', 'SHP', 11),
            'MOD2': ('VEL', 'SHP', 13), 'MOD3': ('AENV', 'E3', 17), 'MOD4': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1,
            'CYCLE RISE': 0, 'CYCLE FALL': 95,
         }),
    dict(engine='PHASE', idx=6, name='DEEP KICK', cat='PERC', bseed=34356,  # #95
         p={
            'ATK': 0, 'DEC': 84, 'SUS': 0, 'REL': 66, 'ED_PIT': 9, 'KTRK': 0, 'E2ATK': 0, 'E2DEC': 56, 'E2SUS': 0, 'E2REL': 41, 'EQHG': -2,
            'TRIM': 101, 'E3': 0, 'E4': 56, 'MOD1': ('AT', 'SHP', 13), 'MOD2': ('ENV2', 'PITCH', 63), 'MOD3': ('ENV2', 'PITCH', 20),
            'MOD4': ('EXPR', 'PITCH', -64), 'LTRIG': 1, 'L2TRIG': 1,
         }),
    dict(engine='PHASE', idx=10, name='HARD KICK', cat='PERC', bseed=14853,  # #96
         p={
            'ATK': 0, 'DEC': 80, 'SUS': 0, 'REL': 64, 'KTRK': 0, 'DIST': 48, 'E2ATK': 0, 'E2DEC': 60, 'E2SUS': 0, 'E2REL': 41, 'ITYPE': 2, 'IA': 127,
            'IB': 90, 'IC': 90, 'TRIM': 113, 'E3': 0, 'E4': 14, 'MOD1': ('ENV2', 'PITCH', 63), 'MOD2': ('ENV2', 'PITCH', 63),
            'MOD3': ('EXPR', 'PITCH', -64), 'MOD4': ('EXPR', 'PITCH', -64), 'LTRIG': 1, 'L2TRIG': 1,
         }),
    dict(engine='TRIO', idx=7, name='FIELD CLANK', cat='PERC', bseed=50175,  # #97
         p={
            'ATK': 0, 'DEC': 79, 'SUS': 0, 'REL': 66, 'RES2': 27, 'KTRK': 0, 'REV': 14, 'TWTYPE': 2, 'FREEA': 70, 'FREEB': 11, 'E2ATK': 0, 'E2DEC': 47,
            'E2SUS': 0, 'E2REL': 41, 'L2RATE': 27, 'L2WAVE': 1, 'CUT2': 59, 'TRIM': 124, 'E1': 1, 'E2': 6, 'E3': 0, 'E4': 0, 'MOD1': ('AT', 'E4', 17),
            'MOD2': ('CYCLE', 'CUT2', 11), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 12, 'TWIN LFO2': 4, 'CYCLE MODE': 1, 'CYCLE RISE': 0,
            'CYCLE FALL': 80,
         }),
    dict(engine='PHYS', idx=8, name='FLOOR THUD', cat='PERC', bseed=11677,  # #98
         p={
            'ATK': 0, 'SUS': 127, 'REL': 71, 'KTRK': 0, 'REV': 22, 'E1': 2, 'E2': 0, 'E3': 52, 'E4': 71, 'E5': 30, 'E6': 100, 'E7': 17, 'E8': 27,
            'MOD1': ('AT', 'E7', 11), 'LTRIG': 1, 'L2TRIG': 1,
         }),
    dict(engine='NOISE', idx=6, name='OPEN SIZZLE', cat='PERC', bseed=25614,  # #99
         p={
            'ATK': 0, 'DEC': 91, 'SUS': 0, 'REL': 66, 'EQLG': -6, 'RES2': 17, 'KTRK': 0, 'REV': 22, 'TWTYPE': 2, 'FREEA': 56, 'FREEB': 21,
            'FREEC': 127, 'E2ATK': 0, 'E2DEC': 76, 'E2SUS': 127, 'L2RATE': 29, 'L2WAVE': 1, 'CUT2': 71, 'TRIM': 109, 'E2': 0, 'MOD1': ('AT', 'E8', 11),
            'MOD2': ('RAND', 'CUT2', 5), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO2': 3,
         }),
    dict(engine='NOISE', idx=7, name='STATIC SNARE', cat='PERC', bseed=58051,  # #100
         p={
            'ATK': 0, 'DEC': 71, 'SUS': 0, 'REL': 56, 'RES2': 41, 'KTRK': 0, 'DIST': 31, 'REV': 23, 'TWTYPE': 2, 'FREEA': 39, 'FREEB': 73, 'FREEC': 36,
            'FREED': 2, 'E2ATK': 0, 'E2DEC': 56, 'E2SUS': 0, 'E2REL': 51, 'L2RATE': 18, 'L2WAVE': 1, 'BLEND': -17, 'CUT2': 57, 'TRIM': 122,
            'E2FLT': 29, 'E2': 0, 'MOD1': ('AT', 'SAT', 17), 'MOD2': ('RAND', 'CUT2', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN ROUTE': 1, 'TWIN LINK': 0,
            'TWIN LFO2': 3,
         }),
    dict(engine='NOISE', idx=5, name='TIZZ HAT', cat='PERC', bseed=16914,  # #101
         p={
            'ATK': 0, 'DEC': 52, 'SUS': 0, 'REL': 47, 'EQLG': -6, 'RES2': 21, 'KTRK': 0, 'REV': 13, 'TWTYPE': 2, 'FREEA': 61, 'FREEB': 27,
            'FREEC': 127, 'E2ATK': 0, 'E2DEC': 51, 'E2SUS': 0, 'E2REL': 41, 'L2RATE': 32, 'L2WAVE': 1, 'CUT2': 74, 'TRIM': 117, 'E2FLT': 6, 'E2': 0,
            'MOD1': ('AT', 'REV', 17), 'MOD2': ('RAND', 'CUT2', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO2': 3,
         }),
    dict(engine='ANALOG', idx=14, name='STORM FRONT', cat='FX', bseed=36047, bamt=14,  # #102
         p={
            'ATK': 97, 'SUS': 127, 'REL': 79, 'LRATE': 23, 'LWAVE': 1, 'LD_FLT': 2, 'RES2': 41, 'KTRK': 0, 'DIST': 7, 'DLY': 20, 'REV': 25,
            'TWTYPE': 2, 'FREEA': 37, 'FREEB': 31, 'E2ATK': 111, 'E2DEC': 0, 'E2SUS': 127, 'E2REL': 79, 'ITYPE': 1, 'IA': 40, 'IB': 127, 'IC': 112,
            'BLEND': -17, 'CUT2': 17, 'TRIM': 84, 'E2FLT': 31, 'E1': 3, 'E3': 0, 'E4': 127, 'MOD1': ('AT', 'FRES', 27), 'MOD2': ('ENV2', 'REV', 32),
            'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN ROUTE': 1, 'CYCLE RISE': 103, 'CYCLE FALL': 107,
         }),
    dict(engine='FM6', idx=7, name='ALIEN CHIMES', cat='FX', bseed=40225,  # #103
         p={
            'EQLG': -6, 'AMODE': 4, 'ARATE': 1, 'AOCT': 2, 'AGATE': 44, 'DLY': 64, 'REV': 76, 'TWTYPE': 4, 'FREEA': 67, 'FREEB': 21, 'E2ATK': 0,
            'E2DEC': 94, 'E2SUS': 0, 'E2REL': 86, 'L2RATE': 26, 'L2WAVE': 1, 'CUT2': 47, 'TRIM': 112, 'L2FLT': 4, 'E1': 4, 'E2': 2, 'E4': 5, 'E7': 70,
            'E8': 1, 'MOD1': ('AT', 'E3', 9), 'MOD2': ('CYCLE', 'CUT2', 9), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'CYCLE RISE': 99,
            'CYCLE FALL': 103,
         }),
    dict(engine='PHASE', idx=5, name='CRYSTAL CAVE', cat='FX', bseed=36122,  # #104
         p={
            'ATK': 0, 'DEC': 101, 'SUS': 32, 'REL': 86, 'EQLG': -6, 'AMODE': 4, 'ARATE': 1, 'AOCT': 3, 'AGATE': 44, 'APROB': 108, 'RES2': 41,
            'DLY': 64, 'REV': 81, 'TWTYPE': 2, 'FREEA': 66, 'FREEB': 21, 'E2ATK': 0, 'E2DEC': 88, 'E2SUS': 0, 'E2REL': 79, 'L2RATE': 26, 'L2WAVE': 1,
            'BLEND': -17, 'CUT2': 58, 'TRIM': 127, 'L2SHP': 4, 'E1': 6, 'E2': 8, 'E3': 72, 'E4': 50, 'E5': 53, 'E6': 1, 'MOD1': ('AT', 'FBLND', 27),
            'MOD2': ('CYCLE', 'CUT2', 9), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN ROUTE': 1, 'TWIN LINK': 0, 'TWIN LFO2': 5, 'CYCLE RISE': 105,
            'CYCLE FALL': 110,
         }),
    dict(engine='PHASE', idx=7, name='SONAR PING', cat='FX', bseed=64729,  # #105
         p={
            'ATK': 0, 'DEC': 90, 'SUS': 52, 'REL': 103, 'EQLG': -4, 'LRATE': 18, 'LWAVE': 1, 'LD_FLT': 3, 'DLY': 60, 'REV': 77, 'FREEA': 49,
            'FREEB': 41, 'E2ATK': 0, 'E2DEC': 76, 'E2SUS': 0, 'E2REL': 66, 'TRIM': 101, 'E2FLT': 21, 'E3': 0, 'E4': 11, 'MOD1': ('AT', 'FRES', 27),
            'MOD2': ('CYCLE', 'SHP', 7), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 101,
         }),
    dict(engine='LOFI', idx=4, name='DIVE BOMB', cat='FX', bseed=27733,  # #106
         p={
            'ATK': 47, 'DEC': 127, 'SUS': 127, 'REL': 79, 'DIST': 27, 'DLY': 57, 'REV': 57, 'TWTYPE': 4, 'FREEA': 54, 'FREEB': 41, 'E2ATK': 0,
            'E2DEC': 92, 'E2SUS': 64, 'E2REL': 73, 'L2RATE': 96, 'L2WAVE': 2, 'CUT2': 50, 'TRIM': 94, 'E2FLT': 17, 'L2SHP': 19, 'E3': 40, 'E5': 5,
            'E8': 100, 'MOD1': ('AT', 'E8', -21), 'MOD2': ('CYCLE', 'PITCH', 63), 'MOD3': ('CYCLE', 'CUT2', -13), 'LTRIG': 1, 'L2TRIG': 1,
            'TWIN LINK': 0, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 103,
         }),
    dict(engine='VOICE', idx=3, name='GHOST VOICES', cat='FX', bseed=15764,  # #107
         p={
            'ATK': 98, 'DEC': 101, 'SUS': 127, 'REL': 103, 'LRATE': 18, 'LWAVE': 1, 'CHOR': 38, 'DLY': 30, 'REV': 57, 'TWTYPE': 4, 'FREEA': 58,
            'FREEB': 11, 'E2ATK': 101, 'E2DEC': 108, 'E2SUS': 89, 'E2REL': 101, 'L2RATE': 13, 'L2WAVE': 1, 'ITYPE': 1, 'IA': 70, 'IB': 127, 'IC': 112,
            'CUT2': 52, 'TRIM': 82, 'E2FLT': 11, 'E2': 100, 'E3': 70, 'E4': 3, 'E5': 52, 'E6': 118, 'E7': 50, 'E8': 40, 'MOD1': ('AT', 'E5', 17),
            'MOD2': ('CYCLE', 'SHP', 9), 'MOD3': ('LFO2', 'E1', 6), 'MOD4': ('LFO2', 'E2', 6), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO1': 9,
            'CYCLE RISE': 108, 'CYCLE FALL': 111,
         }),
    dict(engine='VOICE', idx=7, name='YO YA', cat='FX',  # #108
         p={
            'ATK': 34, 'SUS': 127, 'REL': 65, 'RES2': 7, 'CHOR': 23, 'DLY': 28, 'REV': 38, 'VOICE': 2, 'GLIDE': 24, 'GLMODE': 1, 'TWTYPE': 1,
            'FREEB': 40, 'E2ATK': 0, 'E2DEC': 79, 'E2SUS': 0, 'E2REL': 64, 'L2RATE': 89, 'L2WAVE': 1, 'L2FADE': 84, 'CUT2': -21, 'TRIM': 86,
            'L2PIT': 1, 'E1': 22, 'E2': 0, 'E3': 44, 'E4': 2, 'E5': 88, 'E6': 9, 'E7': 70, 'MOD1': ('AT', 'DEP2', 63), 'MOD2': ('CYCLE', 'CUT2', 7),
            'LTRIG': 1, 'L2TRIG': 1,
         }),
    dict(engine='TRIO', idx=5, name='CIRCUIT BEND', cat='FX', bseed=34291,  # #109
         p={
            'ATK': 0, 'DEC': 74, 'SUS': 44, 'EQLG': -4, 'AMODE': 3, 'AOCT': 3, 'AGATE': 76, 'APROB': 108, 'RES2': 45, 'DRV2': 41, 'KTRK': 0,
            'DIST': 32, 'DLY': 57, 'REV': 44, 'TWTYPE': 1, 'FREEA': 72, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 65, 'E2SUS': 0, 'E2REL': 54, 'L2RATE': 34,
            'L2WAVE': 1, 'CUT2': 59, 'TRIM': 104, 'E1': 14, 'E2': 7, 'E3': 0, 'E4': 9, 'MOD1': ('AT', 'FDRV', 27), 'MOD2': ('CYCLE', 'CUT2', 11),
            'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 12, 'TWIN LFO2': 12, 'CYCLE RISE': 83, 'CYCLE FALL': 88,
         }),
    dict(engine='WHEEL', idx=5, name='LESLIE STORM', cat='FX', bseed=30012,  # #110
         p={
            'ATK': 95, 'SUS': 127, 'REL': 103, 'LRATE': 23, 'LWAVE': 1, 'DIST': 19, 'DLY': 23, 'REV': 42, 'TWTYPE': 4, 'FREEA': 56, 'FREEB': 21,
            'E2ATK': 98, 'E2DEC': 0, 'E2SUS': 127, 'E2REL': 92, 'L2RATE': 17, 'L2WAVE': 1, 'ITYPE': 1, 'IA': 40, 'IB': 127, 'IC': 112, 'CUT2': 51,
            'TRIM': 90, 'E2FLT': 17, 'E2SHP': 32, 'L2SHP': 19, 'E1': 15, 'E6': 0, 'E7': 90, 'E8': 2, 'MOD1': ('AT', 'E7', 11),
            'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO1': 9, 'CYCLE RISE': 109, 'CYCLE FALL': 112,
         }),
    dict(engine='WHEEL', idx=6, name='TAPE DRONE', cat='FX', bseed=63114,  # #111
         p={
            'ATK': 100, 'SUS': 127, 'REL': 105, 'LRATE': 17, 'LWAVE': 1, 'CHOR': 32, 'REV': 64, 'TWTYPE': 4, 'FREEA': 58, 'FREEB': 11, 'L2RATE': 21,
            'L2WAVE': 1, 'CHRD': 9, 'VOIC': 1, 'ITYPE': 9, 'IA': 50, 'IB': 71, 'IC': 110, 'CUT2': 52, 'TRIM': 102, 'L2FLT': 3, 'E1': 12, 'E6': 0,
            'E7': 20, 'E8': 0, 'MOD1': ('AT', 'E4', 17), 'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO1': 11,
            'CYCLE RISE': 115, 'CYCLE FALL': 119,
         }),
    dict(engine='PHYS', idx=6, name='RAIN CHIMES', cat='FX', bseed=23020,  # #112
         p={
            'ATK': 0, 'SUS': 127, 'REL': 86, 'EQLG': -6, 'AMODE': 4, 'ARATE': 1, 'AOCT': 3, 'AGATE': 38, 'APROB': 108, 'RES2': 41, 'DLY': 57,
            'REV': 76, 'TWTYPE': 2, 'FREEA': 64, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 94, 'E2SUS': 0, 'E2REL': 86, 'L2RATE': 31, 'L2WAVE': 1, 'CUT2': 61,
            'TRIM': 111, 'E2FLT': 9, 'E2': 70, 'E3': 100, 'E4': 120, 'E5': 12, 'E6': 100, 'E8': 6, 'MOD1': ('AT', 'E5', 21),
            'MOD2': ('CYCLE', 'CUT2', 9), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN ROUTE': 1, 'TWIN LINK': 0, 'TWIN LFO2': 5, 'CYCLE RISE': 103,
            'CYCLE FALL': 107,
         }),
    dict(engine='NOISE', idx=4, name='MOOR WIND', cat='FX', bseed=31000, bamt=27,  # #113
         p={
            'ATK': 99, 'SUS': 127, 'REL': 110, 'EQLG': -4, 'LRATE': 23, 'RES2': 51, 'KTRK': 0, 'DLY': 11, 'REV': 53, 'TWTYPE': 2, 'FREEA': 31,
            'FREEB': 71, 'FREEC': 35, 'E2ATK': 105, 'E2DEC': 108, 'E2SUS': 89, 'E2REL': 101, 'L2RATE': 15, 'L2WAVE': 1, 'ITYPE': 6, 'IA': 18, 'IB': 90,
            'IC': 50, 'IMIX': 76, 'BLEND': -17, 'CUT2': 51, 'TRIM': 127, 'E2FLT': 5, 'L2FLT': 12, 'E2': 100, 'E7': 90, 'MOD1': ('AT', 'E2', 17),
            'MOD2': ('CYCLE', 'CUT2', 9), 'MOD3': ('LFO1', 'FRES', 6), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN ROUTE': 1, 'TWIN LINK': 0, 'TWIN LFO2': 9,
            'CYCLE RISE': 112, 'CYCLE FALL': 117,
         }),
    dict(engine='NOISE', idx=10, name='REV CYMBAL', cat='FX', bseed=57441,  # #114
         p={
            'ATK': 97, 'SUS': 127, 'REL': 50, 'EQLG': -6, 'RES2': 17, 'KTRK': 8, 'REV': 47, 'TWTYPE': 3, 'FREEA': 47, 'FREEB': 41, 'FREEC': 17,
            'FREED': 2, 'E2ATK': 97, 'E2DEC': 0, 'E2SUS': 127, 'E2REL': 47, 'CUT2': 41, 'TRIM': 109, 'E2FLT': 25, 'E2': 0, 'MOD1': ('AT', 'E8', 17),
            'MOD2': ('CYCLE', 'CUT2', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'CYCLE RISE': 59, 'CYCLE FALL': 60,
         }),
    dict(engine='NOISE', idx=3, name='WHITE OUT', cat='FX', bseed=38918, bamt=12,  # #115
         p={
            'ATK': 94, 'SUS': 127, 'REL': 83, 'EQLG': -4, 'KTRK': 0, 'DLY': 20, 'REV': 30, 'FREEA': 21, 'FREEB': 51, 'FREEC': 20, 'FREED': 2,
            'E2ATK': 100, 'E2DEC': 0, 'E2SUS': 127, 'E2REL': 79, 'L2RATE': 27, 'L2WAVE': 1, 'ITYPE': 1, 'IA': 100, 'IB': 127, 'IC': 112, 'TRIM': 81,
            'E2FLT': 51, 'L2FLT': 5, 'E2': 14, 'MOD1': ('AT', 'FRES', 24), 'MOD2': ('ENV2', 'REV', 32), 'LTRIG': 1, 'L2TRIG': 1,
         }),
    dict(engine='WAVE', idx=17, name='FROZEN AIR', cat='FX', bseed=26635, bamt=17,  # #116
         p={
            'ATK': 88, 'SUS': 127, 'REL': 113, 'EQLG': -3, 'LRATE': 18, 'LWAVE': 1, 'RES2': 41, 'DLY': 32, 'REV': 102, 'TWTYPE': 2, 'FREEA': 57,
            'FREEB': 31, 'E2ATK': 114, 'E2DEC': 0, 'E2SUS': 127, 'E2REL': 86, 'L2RATE': 11, 'L2WAVE': 1, 'BLEND': -17, 'CUT2': 63, 'TRIM': 114,
            'E2SHP': 19, 'L2SHP': 13, 'E1': 25, 'E2': 20, 'E6': 7, 'MOD1': ('AT', 'E3', 8), 'MOD2': ('LFO1', 'CUT', 11), 'MOD3': ('LFO1', 'E6', 11),
            'MOD4': ('LFO2', 'CUT', 7), 'MOD5': ('CYCLE', 'CUT2', 11), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN ROUTE': 1, 'TWIN LINK': 0, 'CYCLE RISE': 110,
            'CYCLE FALL': 112,
         }),
    dict(engine='WAVE', idx=10, name='RESO SWEEP', cat='FX', bseed=36742,  # #117
         p={
            'ATK': 73, 'DEC': 105, 'SUS': 127, 'REL': 101, 'LRATE': 18, 'LWAVE': 1, 'DLY': 44, 'REV': 57, 'TWTYPE': 4, 'FREEA': 67, 'FREEB': 27,
            'L2RATE': 13, 'L2WAVE': 1, 'LPOL': 1, 'CUT2': 51, 'TRIM': 113, 'L2SHP': 45, 'E1': 8, 'E2': 64, 'E6': 6, 'MOD1': ('AT', 'SHP', 17),
            'MOD2': ('CYCLE', 'CUT', 9), 'MOD3': ('LFO1', 'SUBLV', 17), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO1': 11, 'CYCLE RISE': 100,
            'CYCLE FALL': 103,
         }),
    dict(engine='ANALOG', idx=15, name='GLASS SHARDS', cat='OTHER', bseed=17703,  # #118
         p={
            'ATK': 0, 'DEC': 74, 'SUS': 0, 'REL': 64, 'AMODE': 3, 'AOCT': 3, 'AGATE': 83, 'APROB': 102, 'RES2': 77, 'KTRK': 13, 'DIST': 32, 'DLY': 51,
            'TWTYPE': 2, 'FREEA': 51, 'FREEB': 41, 'E2ATK': 0, 'E2DEC': 56, 'E2SUS': 0, 'E2REL': 56, 'L2RATE': 27, 'L2WAVE': 1, 'ITYPE': 5, 'IA': 90,
            'IB': 100, 'IMIX': 64, 'CUT2': 57, 'TRIM': 119, 'E2FLT': 21, 'L2FLT': 5, 'E1': 1, 'E2': 0, 'E3': 0, 'E7': 127, 'MOD1': ('AT', 'CUT2', 17),
            'MOD2': ('CYCLE', 'CUT2', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 11, 'CYCLE RISE': 41, 'CYCLE FALL': 41,
            'CYCLE SYNC': 1,
         }),
    dict(engine='FM6', idx=8, name='TEMPLE BLOCK', cat='OTHER', bseed=28718,  # #119
         p={
            'REV': 25, 'FREEB': 40, 'E2ATK': 0, 'E2DEC': 67, 'E2SUS': 0, 'E2REL': 56, 'TRIM': 120, 'E3': 22, 'E4': -2, 'E6': 3, 'E8': 5,
            'MOD1': ('EXPR', 'PITCH', 63), 'MOD2': ('EXPR', 'PITCH', 38), 'MOD3': ('ENV2', 'AMP', 63), 'LTRIG': 1, 'L2TRIG': 1,
         }),
    dict(engine='LOFI', idx=5, name='DEAD BATTERY', cat='OTHER', bseed=16860,  # #120
         p={
            'ATK': 0, 'DEC': 68, 'SUS': 19, 'REL': 62, 'AMODE': 3, 'AOCT': 2, 'APROB': 114, 'DLY': 51, 'REV': 38, 'TWTYPE': 4, 'FREEA': 57,
            'FREEB': 31, 'E2ATK': 0, 'E2DEC': 66, 'E2SUS': 0, 'E2REL': 56, 'L2RATE': 35, 'L2WAVE': 1, 'CUT2': 56, 'TRIM': 111, 'E2FLT': 17, 'L2FLT': 5,
            'E2': 2, 'E4': 52, 'E6': 10, 'E8': 78, 'MOD1': ('AT', 'E8', -17), 'MOD2': ('CYCLE', 'CUT2', -11), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0,
            'CYCLE MODE': 1, 'CYCLE RISE': 60, 'CYCLE FALL': 80,
         }),
    dict(engine='LOFI', idx=7, name='DUSTY DRONE', cat='OTHER', bseed=63786,  # #121
         p={
            'ATK': 95, 'SUS': 127, 'REL': 103, 'EQLG': -3, 'LRATE': 17, 'LWAVE': 1, 'DLY': 34, 'REV': 47, 'TWTYPE': 4, 'FREEA': 57, 'FREEB': 17,
            'L2RATE': 32, 'L2WAVE': 1, 'ITYPE': 10, 'IA': 40, 'IB': 36, 'IC': 18, 'CUT2': 51, 'TRIM': 102, 'E1': 2, 'E2': 2, 'E4': 47, 'E8': 57,
            'MOD1': ('AT', 'E4', 11), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO2', 'E8', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO1': 11,
            'CYCLE RISE': 115, 'CYCLE FALL': 119,
         }),
    dict(engine='LOFI', idx=6, name='PIXEL KICK', cat='OTHER', bseed=33683,  # #122
         p={
            'ATK': 0, 'DEC': 72, 'SUS': 0, 'KTRK': 0, 'DIST': 19, 'REV': 10, 'E2ATK': 0, 'E2DEC': 62, 'E2SUS': 0, 'E2REL': 51, 'TRIM': 118, 'E2': 1,
            'E5': 16, 'E8': 90, 'MOD1': ('AT', 'E5', 13), 'LTRIG': 1, 'L2TRIG': 1,
         }),
    dict(engine='VOICE', idx=4, name='VOCAL BEATS', cat='OTHER', bseed=31759,  # #123
         p={
            'ATK': 0, 'DEC': 67, 'SUS': 0, 'REV': 19, 'FREEB': 40, 'E2ATK': 0, 'E2DEC': 65, 'E2SUS': 0, 'E2REL': 54, 'TRIM': 119, 'E2': 127, 'E3': 100,
            'E5': 30, 'E6': 100, 'E7': 110, 'E8': 89, 'MOD1': ('AT', 'SHP', 16), 'LTRIG': 1, 'L2TRIG': 1,
         }),
    dict(engine='TRIO', idx=10, name='DIAL TONE', cat='OTHER', bseed=61579,  # #124
         p={
            'ATK': 22, 'SUS': 127, 'REL': 39, 'KTRK': 0, 'TWTYPE': 1, 'CUT2': 45, 'TRIM': 90, 'E1': 4, 'E2': 4, 'E3': 0, 'E4': 0,
            'MOD1': ('AT', 'CUT2', 5), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0,
         }),
    dict(engine='TRIO', idx=6, name='GRAVEL SNARE', cat='OTHER', bseed=20254,  # #125
         p={
            'ATK': 0, 'DEC': 71, 'SUS': 0, 'REL': 65, 'RES2': 30, 'KTRK': 0, 'REV': 23, 'TWTYPE': 1, 'E2ATK': 0, 'E2DEC': 67, 'E2SUS': 0, 'E2REL': 59,
            'CUT2': 65, 'E1': 7, 'E3': 24, 'E4': 0, 'E8': 50, 'MOD1': ('AT', 'CUT2', 8), 'MOD2': ('VEL', 'CUT2', 10), 'LTRIG': 1, 'L2TRIG': 1,
            'TWIN LINK': 0, 'TWIN ENV2': 17,
         }),
    dict(engine='PHYS', idx=7, name='TABLA TALK', cat='OTHER', bseed=40371,  # #126
         p={
            'ATK': 0, 'SUS': 127, 'REL': 56, 'REV': 19, 'FREEB': 40, 'E2ATK': 0, 'E2DEC': 69, 'E2SUS': 0, 'TRIM': 111, 'E1': 2, 'E2': 112, 'E3': 76,
            'E4': 56, 'E5': 44, 'E6': 100, 'E7': 14, 'E8': 24, 'MOD1': ('AT', 'E7', 13), 'LTRIG': 1, 'L2TRIG': 1,
         }),
    dict(engine='NOISE', idx=8, name='GEIGER', cat='OTHER', bseed=56918,  # #127
         p={
            'ATK': 19, 'SUS': 127, 'REL': 73, 'KTRK': 0, 'DIST': 57, 'DLY': 32, 'REV': 38, 'FREEA': 61, 'FREEB': 87, 'FREEC': 63, 'L2RATE': 34,
            'L2WAVE': 1, 'TRIM': 102, 'L2FLT': 8, 'E1': 1, 'E2': 0, 'E6': 110, 'E7': 20, 'MOD1': ('AT', 'E6', 19), 'MOD2': ('CYCLE', 'E6', 9),
            'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 105, 'CYCLE FALL': 110,
         }),
    dict(engine='NOISE', idx=9, name='MODEM', cat='OTHER', bseed=7821,  # #128
         p={
            'ATK': 0, 'DEC': 62, 'SUS': 0, 'REL': 56, 'AMODE': 3, 'AOCT': 3, 'AGATE': 57, 'APROB': 108, 'KTRK': 63, 'DLY': 51, 'REV': 25, 'FREEA': 71,
            'FREEB': 31, 'FREEC': 15, 'E2ATK': 0, 'E2DEC': 60, 'E2SUS': 0, 'E2REL': 51, 'TRIM': 114, 'E2FLT': 8, 'E1': 2, 'E2': 0, 'E6': 48, 'E8': 60,
            'MOD1': ('AT', 'CUT', 10), 'LTRIG': 1, 'L2TRIG': 1,
         }),
]
