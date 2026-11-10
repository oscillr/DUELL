# SPDX-License-Identifier: GPL-3.0-only
"""DUELL factory sounds: the data (tools/gen_duell_sounds.py validates it and writes build/gen/duell_sounds.h).

DUELL 1.0 (owner, 2026-10-21): the 128 factory sounds are the owner's bank, the records of tools/factory_bank.duell-patches.json as exported (no refinement,
no retuning, the names of the file), written here by tools/import_bank.py. The earlier recipe list (SOUND_RECIPES.md, the refinement passes) is in git history.
Every sound: engine, idx (its factory index within the engine: the identity favourites and the shadows use; APPEND-ONLY per engine), name, cat, p: the
parameters that differ from INIT (params.c TP defaults and the engine's EDIT defaults; E1..E8 are the engine's eight; MOD1..MOD8 are (SRC, DST, AMT)
matrix slots; extension slots as "<group> <label>"), bseed / bamt: End B (the roll's seed and the AMOUNT %). Do not edit by hand: import again.
End B as exact values (2026-10-29, tools/convert_endb.py; the format is tools/endb.py's = the patch record's End B block): bend {label: End B value} (where it differs from the Original),
bpin [labels] (marks of values set by hand), bfm [65 FM6 bytes of End B]. The seeds stay until the firmware reads the blocks.
"""

# CONVERT_TWIN: the engine filters of the earlier sound lists are the twin filter's already (tools/convert_twin.py, a second run is refused).
CONVERT_TWIN = 1

SOUNDS = [
    dict(engine='ANALOG', idx=2, name='BEHIVE', cat='BASS', bseed=18967,  # #1
         p={
            'ATK': 0, 'SUS': 127, 'REL': 44, 'LRATE': 51, 'LPHASE': 32, 'LD_FLT': 9, 'LD_AMP': 39, 'RES2': 33, 'DRV2': 61, 'KTRK': 13, 'TRANS': -12,
            'DIST': 19, 'VOICE': 2, 'GLIDE': 32, 'GLMODE': 1, 'TWTYPE': 2, 'FREEA': 46, 'FREEB': 53, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 69, 'E2SUS': 0,
            'E2REL': 44, 'L2RATE': 23, 'CHRD': 9, 'VOIC': 1, 'LTRIG': 0, 'BLEND': -7, 'CUT2': 38, 'TRIM': 104, 'E2FLT': 6, 'L2FLT': 29, 'E2': 30,
            'E4': 4, 'E7': 48, 'MOD1': ('AT', 'FRES', 13), 'MOD2': ('CYCLE', 'RAT1', 55), 'L2TRIG': 1, 'TWIN ROUTE': 1, 'TWIN LINK': 0,
            'TWIN LFO1': -17, 'TWIN LFO2': 15, 'SUB LEVEL': 95, 'SUB WAVE': 1, 'SUB HARM': 40, 'CYCLE CURVE': -33, 'CYCLE SYNC': 1,
         },
         bend={
            'ATK': 1, 'DEC': 72, 'SUS': 126, 'REL': 36, 'ED_SHP': 8, 'LRATE': 56, 'LPHASE': 30, 'LFADE': 8, 'LD_FLT': 2, 'LD_SHP': -4, 'LD_AMP': 44,
            'RES2': 34, 'DRV2': 64, 'KTRK': 6, 'DIST': 27, 'CHOR': 7, 'DLY': 3, 'REV': 7, 'DETUNE': 35, 'FREEA': 51, 'FREEB': 52, 'FREEC': 7,
            'M1AMT': 12, 'M2AMT': 57, 'M3AMT': 23, 'E2ATK': 2, 'E2DEC': 61, 'E2SUS': 4, 'E2REL': 48, 'L2RATE': 17, 'L2PHASE': 5, 'L2FADE': 4,
            'BLEND': -6, 'CUT2': 36, 'E2FLT': 7, 'E2': 35, 'E3': 66, 'E4': 7, 'E7': 52, 'TWIN KTRK2': -8, 'TWIN ENV2': -3, 'TWIN LFO1': -25,
            'TWIN LFO2': 14, 'SUB LEVEL': 101, 'CYCLE RISE': 4, 'CYCLE FALL': 64, 'CYCLE CURVE': -39, 'AMPENV SATN': 2, 'FILTERENV FBLND': 8,
            'FILTERENV FDRV': -7, 'FILTERENV MRES': 6, 'CYCLE TCUT': 7, 'CYCLE PAN': -4, 'LFO2 TRES': -3, 'LFO2 MMODE': 2, 'LFO2 FBLND': -3,
         }),
    dict(engine='ANALOG', idx=16, name='FOLD BASS', cat='BASS', bseed=17953,  # #2
         p={
            'ATK': 0, 'DEC': 77, 'SUS': 114, 'REL': 51, 'RES2': 17, 'DRV2': 31, 'KTRK': 13, 'TRANS': -12, 'VOICE': 2, 'GLIDE': 16, 'GLMODE': 1,
            'TWTYPE': 1, 'FREEA': 44, 'FREEB': 21, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 77, 'E2SUS': 44, 'E2REL': 56, 'L2RATE': 27, 'L2WAVE': 1,
            'ITYPE': 3, 'IA': 46, 'IB': 100, 'EQHG': -4, 'EQHF': -8, 'CUT2': -7, 'DKDEP': 41, 'TRIM': 111, 'DKSRC': 0, 'E2FLT': 17, 'L2FLT': 4,
            'E1': 6, 'E2': 0, 'E3': 70, 'E7': 20, 'MOD1': ('AT', 'INS A', 16), 'MOD2': ('ENV2', 'INS A', 17), 'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1,
            'L2TRIG': 1, 'SUB LEVEL': 31, 'CYCLE RISE': 97, 'CYCLE FALL': 97,
         },
         bend={
            'ATK': 7, 'DEC': 73, 'SUS': 108, 'REL': 49, 'ED_SHP': 5, 'LRATE': 65, 'LPHASE': 7, 'LFADE': 4, 'LD_FLT': 8, 'LD_SHP': 5, 'LD_AMP': 7,
            'RES2': 9, 'DRV2': 39, 'KTRK': 14, 'DIST': 8, 'CHOR': 6, 'DLY': 2, 'REV': 3, 'DETUNE': 37, 'FREEB': 16, 'FREEC': 5, 'M1AMT': 11,
            'M2AMT': 21, 'M3AMT': 14, 'M4AMT': 5, 'E2ATK': 8, 'E2DEC': 75, 'E2SUS': 41, 'E2REL': 52, 'L2RATE': 31, 'L2PHASE': 6, 'L2FADE': 4,
            'IA': 45, 'IB': 108, 'IC': 94, 'IMIX': 119, 'BLEND': 6, 'CUT2': -4, 'DKDEP': 37, 'E2FLT': 14, 'E2': 8, 'E3': 76, 'E4': 3, 'E7': 13,
            'TWIN KTRK2': -4, 'TWIN ENV2': -7, 'TWIN LFO1': 2, 'TWIN LFO2': 6, 'SUB LEVEL': 39, 'CYCLE RISE': 96, 'CYCLE FALL': 104,
            'CYCLE CURVE': -8, 'AMPENV SATN': -8, 'FILTERENV FBLND': 6, 'FILTERENV FDRV': -1, 'FILTERENV MRES': 3, 'CYCLE TCUT': 5, 'CYCLE PAN': 8,
            'LFO2 TRES': 2, 'LFO2 MMODE': -5, 'LFO2 FBLND': -7,
         }),
    dict(engine='ANALOG', idx=0, name='LADDER BASS', cat='BASS', bseed=26217,  # #3
         p={
            'DEC': 73, 'SUS': 108, 'REL': 54, 'KTRK': 13, 'TRANS': -12, 'VOICE': 2, 'FREEA': 24, 'FREEB': 17, 'FREED': 2, 'E2ATK': 32, 'E2DEC': 76,
            'E2SUS': 38, 'E2REL': 54, 'L2RATE': 27, 'L2WAVE': 1, 'TRIM': 96, 'E2FLT': 27, 'L2FLT': 4, 'E1': 6, 'E2': 6, 'E7': 24,
            'MOD1': ('AT', 'FRES', 17), 'MOD2': ('VEL', 'CUT', 6), 'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 31,
            'CYCLE RISE': 100, 'CYCLE FALL': 100,
         },
         bend={
            'ATK': 4, 'DEC': 77, 'SUS': 114, 'REL': 57, 'ED_SHP': -1, 'LRATE': 58, 'LPHASE': 8, 'LFADE': 4, 'LD_FLT': -1, 'LD_SHP': 2, 'LD_AMP': 8,
            'KTRK': 14, 'DIST': 7, 'CHOR': 2, 'DLY': 1, 'REV': 8, 'DETUNE': 41, 'FREEA': 28, 'FREEB': 23, 'FREEC': 5, 'M1AMT': 19, 'M2AMT': 12,
            'M3AMT': 14, 'M4AMT': 3, 'E2ATK': 38, 'E2DEC': 79, 'E2SUS': 36, 'E2REL': 62, 'L2RATE': 22, 'L2PHASE': 2, 'L2FADE': 6, 'E2FLT': 21,
            'E2': 12, 'E3': 66, 'E4': 4, 'E7': 16, 'SUB LEVEL': 23, 'CYCLE RISE': 103, 'CYCLE FALL': 102, 'CYCLE CURVE': -3, 'AMPENV SATN': 5,
            'FILTERENV MRES': -6, 'CYCLE PAN': 2, 'LFO2 MMODE': 8,
         }),
    dict(engine='ANALOG', idx=3, name='RUBBER BAND', cat='BASS', bseed=49167,  # #4
         p={
            'ATK': 0, 'DEC': 82, 'SUS': 41, 'REL': 64, 'KTRK': 13, 'TRANS': -12, 'DIST': 22, 'REV': 6, 'VOICE': 2, 'FREEA': 27, 'FREEB': 47,
            'E2ATK': 0, 'E2DEC': 65, 'E2SUS': 0, 'E2REL': 65, 'L2RATE': 21, 'EQHG': -4, 'EQHF': -8, 'TRIM': 112, 'E2FLT': 37, 'L2FLT': 4, 'E1': 6,
            'E2': 0, 'E3': 25, 'E7': 18, 'MOD1': ('AT', 'SAT', 7), 'MOD2': ('VEL', 'CUT', 8), 'MOD3': ('CYCLE', 'FRES', 11), 'LTRIG': 1, 'L2TRIG': 1,
            'SUB LEVEL': 31, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 81,
         },
         bend={
            'ATK': 3, 'DEC': 83, 'SUS': 47, 'REL': 70, 'ED_SHP': 5, 'LRATE': 52, 'LPHASE': 6, 'LFADE': 6, 'LD_FLT': 8, 'LD_SHP': -7, 'LD_AMP': 1,
            'KTRK': 7, 'DIST': 18, 'CHOR': 4, 'DLY': 1, 'REV': 7, 'DETUNE': 37, 'FREEB': 51, 'FREEC': 5, 'M1AMT': 0, 'M2AMT': 9, 'M3AMT': 14,
            'M4AMT': 2, 'E2ATK': 8, 'E2DEC': 61, 'E2SUS': 8, 'E2REL': 61, 'L2RATE': 16, 'L2PHASE': 1, 'L2FADE': 3, 'E2FLT': 43, 'E2': 8, 'E3': 18,
            'E4': 1, 'E7': 22, 'SUB LEVEL': 28, 'CYCLE RISE': 6, 'CYCLE FALL': 76, 'CYCLE CURVE': -2, 'AMPENV SATN': -3, 'FILTERENV MRES': -6,
            'CYCLE PAN': 4, 'LFO2 MMODE': 2,
         }),
    dict(engine='ANALOG', idx=4, name='SQUELCH', cat='BASS', bseed=17703,  # #5
         p={
            'ATK': 0, 'DEC': 86, 'SUS': 110, 'REL': 32, 'KTRK': 13, 'TRANS': -12, 'DIST': 25, 'DLY': 32, 'VOICE': 2, 'GLIDE': 26, 'GLMODE': 1,
            'FREEA': 26, 'FREEB': 97, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 61, 'E2SUS': 0, 'E2REL': 0, 'L2RATE': 21, 'TRIM': 93, 'E2FLT': 27, 'L2FLT': 4,
            'E2': 0, 'E3': 0, 'E7': 30, 'MOD1': ('AT', 'CUT', 7), 'MOD2': ('VEL', 'FRES', 8), 'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 120, 'CYCLE FALL': 0,
         },
         bend={
            'ATK': 5, 'DEC': 85, 'SUS': 102, 'REL': 39, 'ED_SHP': 4, 'LRATE': 54, 'LPHASE': 8, 'LFADE': 6, 'LD_FLT': -5, 'LD_SHP': 5, 'LD_AMP': 5,
            'KTRK': 18, 'DIST': 29, 'CHOR': 1, 'DLY': 35, 'REV': 8, 'DETUNE': 41, 'FREEA': 31, 'FREEB': 94, 'FREEC': 5, 'M1AMT': 10, 'M2AMT': 6,
            'M3AMT': 15, 'M4AMT': -3, 'E2ATK': 2, 'E2DEC': 65, 'E2SUS': 3, 'E2REL': 8, 'L2RATE': 26, 'L2PHASE': 5, 'L2FADE': 1, 'E2FLT': 21, 'E2': 3,
            'E3': 8, 'E4': 6, 'E7': 28, 'SUB LEVEL': 5, 'CYCLE RISE': 113, 'CYCLE FALL': 8, 'CYCLE CURVE': -7, 'AMPENV SATN': 1, 'FILTERENV MRES': 7,
            'CYCLE PAN': -8, 'LFO2 MMODE': -2,
         }),
    dict(engine='ANALOG', idx=1, name='TIDAL SUB', cat='BASS', bseed=1207,  # #6
         p={
            'ATK': 22, 'SUS': 127, 'KTRK': 13, 'TRANS': -12, 'DIST': 7, 'VOICE': 2, 'FREEA': 41, 'E2ATK': 0, 'E2DEC': 69, 'E2SUS': 0, 'L2RATE': 17,
            'L2WAVE': 1, 'TRIM': 88, 'E2FLT': 4, 'L2FLT': 4, 'E1': 3, 'E2': 0, 'E3': 0, 'E7': 16, 'MOD1': ('AT', 'SUBLV', 17),
            'MOD2': ('CYCLE', 'E7', 7), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 17, 'CYCLE RISE': 105, 'CYCLE FALL': 105,
         },
         bend={
            'ATK': 20, 'DEC': 64, 'SUS': 123, 'REL': 59, 'ED_SHP': 1, 'LRATE': 52, 'LPHASE': 3, 'LFADE': 4, 'LD_FLT': 1, 'LD_SHP': -4, 'LD_AMP': 3,
            'KTRK': 10, 'DIST': 10, 'CHOR': 7, 'DLY': 3, 'REV': 1, 'DETUNE': 41, 'FREEA': 43, 'FREEB': 6, 'FREEC': 5, 'M1AMT': 12, 'M2AMT': 13,
            'M3AMT': -2, 'E2ATK': 6, 'E2DEC': 62, 'E2SUS': 1, 'E2REL': 54, 'L2RATE': 18, 'L2PHASE': 4, 'L2FADE': 3, 'E2FLT': -4, 'E2': 2, 'E3': 7,
            'E4': 3, 'E7': 21, 'SUB LEVEL': 16, 'CYCLE RISE': 113, 'CYCLE FALL': 103, 'CYCLE CURVE': 3, 'AMPENV SATN': 6, 'FILTERENV MRES': -6,
            'CYCLE PAN': -4, 'LFO2 MMODE': -4,
         }),
    dict(engine='FM6', idx=12, name='FM BASS 2', cat='BASS', bseed=17352,  # #7
         p={
            'LRATE': 30, 'LWAVE': 1, 'LD_FLT': 4, 'RES2': 21, 'KTRK': 0, 'TRANS': -12, 'REV': 6, 'VOICE': 2, 'TWTYPE': 1, 'FREEA': 51, 'FREEB': 11,
            'E2ATK': 0, 'E2DEC': 71, 'E2SUS': 0, 'E2REL': 61, 'ITYPE': 1, 'IA': 40, 'IB': 127, 'IC': 112, 'IMIX': 90, 'EQHG': -4, 'EQHF': -8,
            'CUT2': -17, 'DKDEP': 51, 'DKSRC': 0, 'E2FLT': 17, 'E8': 41, 'MOD1': ('AT', 'E5', 21), 'MOD2': ('ENV2', 'E3', 17),
            'MOD3': ('CYCLE', 'E3', 4), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 61, 'CYCLE RISE': 91, 'CYCLE FALL': 91,
         },
         bend={
            'ATK': 8, 'DEC': 7, 'SUS': 115, 'REL': 3, 'ED_SHP': -4, 'LRATE': 36, 'LPHASE': 6, 'LFADE': 6, 'LD_FLT': 9, 'LD_SHP': -6, 'LD_AMP': 8,
            'RES2': 14, 'DRV2': 8, 'KTRK': -2, 'DIST': 8, 'CHOR': 1, 'DLY': 1, 'REV': 3, 'DETUNE': 33, 'FREEA': 49, 'FREEB': 6, 'FREEC': 2,
            'M1AMT': 29, 'M2AMT': 18, 'M3AMT': 3, 'E2ATK': 5, 'E2DEC': 70, 'E2SUS': 8, 'E2REL': 64, 'L2RATE': 56, 'L2PHASE': 7, 'L2FADE': 6,
            'IA': 37, 'IB': 123, 'IC': 108, 'IMIX': 82, 'BLEND': 1, 'CUT2': -15, 'DKDEP': 46, 'E2FLT': 10, 'E3': -8, 'E5': 6, 'E7': 2,
            'TWIN KTRK2': -2, 'TWIN ENV2': 8, 'TWIN LFO1': 1, 'TWIN LFO2': -3, 'SUB LEVEL': 62, 'CYCLE RISE': 83, 'CYCLE FALL': 83,
            'CYCLE CURVE': -4, 'AMPENV SATN': -8, 'FILTERENV FBLND': 2, 'FILTERENV FDRV': 3, 'FILTERENV MRES': -1, 'CYCLE TCUT': 7, 'CYCLE PAN': 4,
            'LFO2 TRES': -4, 'LFO2 MMODE': 2, 'LFO2 FBLND': -4,
         },
         bfm=[
            98, 94, 36, 97, 93, 96, 2, 6, 71, 95, 98, 45, 79, 98, 93, 98, 0, 1, 94, 97, 70, 98, 98, 98, 52, 0, 1, 95, 97, 25, 98, 98, 96, 97, 0, 98,
            93, 93, 36, 27, 96, 96, 3, 1, 74, 98, 37, 14, 47, 98, 98, 96, 0, 88, 99, 67, 95, 60, 50, 50, 50, 26, 2, 0, 1,
         ]),
    dict(engine='FM6', idx=11, name='FRETLESS', cat='BASS', bseed=1048,  # #8
         p={
            'KTRK': 0, 'TRANS': -12, 'DIST': 7, 'CHOR': 14, 'VOICE': 2, 'GLIDE': 31, 'GLMODE': 1, 'FREEA': 47, 'FREEB': 21, 'E2ATK': 71, 'E2DEC': 87,
            'E2SUS': 41, 'E2REL': 51, 'L2RATE': 21, 'L2WAVE': 1, 'EQHG': -4, 'EQHF': -8, 'DKDEP': 14, 'TRIM': 120, 'DKSRC': 0, 'E2FLT': 27, 'L2FLT': 4,
            'E6': 2, 'E8': 43, 'MOD1': ('AT', 'E3', 4), 'MOD2': ('ENV2', 'E3', 5), 'MOD3': ('CYCLE', 'E3', 4), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 100, 'CYCLE FALL': 100,
         },
         bend={
            'ATK': 57, 'DEC': 86, 'SUS': 106, 'REL': 53, 'ED_SHP': -1, 'LRATE': 61, 'LPHASE': 4, 'LFADE': 8, 'LD_FLT': -4, 'LD_SHP': 1, 'LD_AMP': 3,
            'KTRK': 8, 'DIST': 2, 'CHOR': 22, 'DLY': 4, 'REV': 4, 'DETUNE': 37, 'FREEA': 45, 'FREEB': 22, 'FREEC': 4, 'M1AMT': 3, 'M2AMT': 1,
            'M3AMT': -3, 'M4AMT': 12, 'E2ATK': 70, 'E2DEC': 84, 'E2SUS': 37, 'E2REL': 57, 'L2RATE': 15, 'L2PHASE': 2, 'L2FADE': 1, 'DKDEP': 15,
            'E2FLT': 25, 'E3': 7, 'E5': -1, 'E7': 6, 'SUB LEVEL': 3, 'CYCLE RISE': 93, 'CYCLE FALL': 99, 'CYCLE CURVE': -1, 'AMPENV SATN': 6,
            'FILTERENV MRES': 5, 'CYCLE PAN': -6, 'LFO2 MMODE': -2,
         },
         bfm=[
            97, 95, 94, 98, 93, 98, 94, 3, 5, 96, 96, 93, 95, 98, 98, 98, 0, 1, 60, 34, 29, 46, 78, 72, 58, 4, 35, 54, 35, 34, 51, 98, 93, 89, 0, 59,
            53, 31, 35, 54, 89, 86, 71, 5, 81, 52, 34, 31, 53, 98, 93, 89, 0, 98, 99, 99, 99, 99, 50, 50, 50, 41, 4, 0, 3,
         ]),
    dict(engine='FM6', idx=0, name='SLAP DOCTOR', cat='BASS', bseed=45536,  # #9
         p={
            'KTRK': 0, 'TRANS': -12, 'DIST': 7, 'VOICE': 2, 'FREEA': 41, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 59, 'E2SUS': 0, 'E2REL': 56, 'L2RATE': 27,
            'L2WAVE': 1, 'EQHG': -4, 'EQHF': -8, 'DKDEP': 16, 'TRIM': 116, 'DKSRC': 0, 'E2FLT': 21, 'L2FLT': 4, 'E2': 1, 'E3': 10, 'E5': 24, 'E6': 3,
            'E8': 2, 'MOD1': ('AT', 'E7', 37), 'MOD2': ('ENV2', 'E3', 11), 'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 41,
            'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 81,
         },
         bend={
            'ATK': 8, 'DEC': 92, 'SUS': 110, 'REL': 23, 'ED_SHP': 4, 'LRATE': 66, 'LPHASE': 3, 'LFADE': 7, 'LD_FLT': 2, 'LD_SHP': 4, 'LD_AMP': 5,
            'KTRK': -7, 'DIST': 0, 'CHOR': 5, 'DLY': 2, 'REV': 6, 'DETUNE': 46, 'FREEA': 43, 'FREEB': 10, 'FREEC': 1, 'M1AMT': 39, 'M2AMT': 5,
            'M3AMT': 13, 'M4AMT': 5, 'E2ATK': 3, 'E2DEC': 56, 'E2SUS': 1, 'E2REL': 51, 'L2RATE': 35, 'L2PHASE': 5, 'L2FADE': 4, 'DKDEP': 23,
            'E2FLT': 24, 'E3': 6, 'E5': 27, 'E7': 8, 'SUB LEVEL': 48, 'CYCLE RISE': 6, 'CYCLE FALL': 79, 'CYCLE CURVE': -4, 'AMPENV SATN': 8,
            'FILTERENV MRES': 1, 'CYCLE PAN': 8, 'LFO2 MMODE': 4,
         },
         bfm=[
            98, 96, 95, 97, 98, 94, 94, 6, 4, 97, 95, 97, 97, 98, 98, 98, 0, 1, 93, 77, 54, 73, 95, 61, 41, 4, 73, 93, 51, 44, 72, 98, 93, 89, 0, 81,
            98, 64, 43, 77, 98, 68, 50, 4, 86, 97, 39, 26, 72, 98, 97, 93, 0, 98, 99, 99, 99, 99, 50, 50, 50, 34, 2, 0, 3,
         ]),
    dict(engine='PHASE', idx=0, name='ELASTIC BASS', cat='BASS', bseed=60775, bamt=11,  # #10
         p={
            'DEC': 76, 'SUS': 70, 'REL': 55, 'KTRK': 0, 'TRANS': -12, 'DIST': 13, 'VOICE': 2, 'GLIDE': 21, 'GLMODE': 1, 'FREEA': 37, 'FREEB': 31,
            'FREED': 2, 'E2ATK': 0, 'E2DEC': 71, 'E2SUS': 13, 'E2REL': 54, 'L2RATE': 27, 'L2WAVE': 1, 'EQHG': -4, 'EQHF': -8, 'DKDEP': 14, 'TRIM': 110,
            'DKSRC': 0, 'E2FLT': 27, 'L2FLT': 4, 'E1': 5, 'E3': 38, 'E4': 66, 'MOD1': ('AT', 'E3', 11), 'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1,
            'L2TRIG': 1, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         },
         bend={
            'ATK': 13, 'DEC': 74, 'SUS': 71, 'REL': 59, 'ED_SHP': 11, 'LRATE': 48, 'LPHASE': 8, 'LFADE': 7, 'LD_FLT': -2, 'LD_SHP': 2, 'LD_AMP': 7,
            'KTRK': 6, 'DIST': 3, 'CHOR': 12, 'DLY': 8, 'REV': 4, 'DETUNE': 34, 'FREEB': 27, 'FREEC': 4, 'M1AMT': 24, 'M2AMT': 1, 'M3AMT': 1,
            'E2ATK': 9, 'E2DEC': 82, 'E2SUS': 23, 'E2REL': 66, 'L2RATE': 36, 'L2PHASE': 10, 'L2FADE': 7, 'DKDEP': 21, 'E2FLT': 26, 'E3': 29,
            'E4': 75, 'E5': 1, 'SUB LEVEL': 11, 'CYCLE RISE': 104, 'CYCLE FALL': 105, 'CYCLE CURVE': 13, 'AMPENV SATN': 2, 'FILTERENV MRES': -6,
            'CYCLE PAN': -11, 'LFO2 MMODE': 4,
         }),
    dict(engine='PHASE', idx=9, name='FUZZ BASS', cat='BASS', bseed=57016,  # #11
         p={
            'DEC': 77, 'SUS': 112, 'REL': 52, 'KTRK': 0, 'TRANS': -12, 'VOICE': 2, 'GLIDE': 18, 'GLMODE': 1, 'FREEA': 41, 'FREEB': 17, 'FREED': 2,
            'E2ATK': 0, 'E2DEC': 71, 'E2SUS': 25, 'E2REL': 54, 'L2RATE': 21, 'ITYPE': 4, 'IA': 82, 'IB': 90, 'IC': 90, 'IMIX': 110, 'EQHG': -4,
            'EQHF': -8, 'TRIM': 111, 'E2FLT': 21, 'L2FLT': 4, 'E1': 5, 'E3': 44, 'E4': 45, 'MOD1': ('AT', 'FRES', 17), 'MOD2': ('CYCLE', 'INS A', 7),
            'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 97, 'CYCLE FALL': 97,
         },
         bend={
            'ATK': 16, 'DEC': 72, 'SUS': 119, 'REL': 51, 'ED_SHP': -3, 'LRATE': 59, 'LPHASE': 7, 'LFADE': 3, 'LD_FLT': -3, 'LD_SHP': 5, 'LD_AMP': 7,
            'KTRK': 2, 'DIST': 4, 'CHOR': 8, 'DLY': 2, 'REV': 8, 'DETUNE': 38, 'FREEA': 42, 'FREEB': 14, 'FREEC': 6, 'M1AMT': 11, 'M2AMT': 6,
            'M3AMT': 7, 'E2ATK': 5, 'E2DEC': 75, 'E2SUS': 19, 'E2REL': 52, 'L2RATE': 20, 'L2PHASE': 2, 'L2FADE': 2, 'IA': 79, 'IB': 89, 'IC': 95,
            'IMIX': 118, 'E2FLT': 24, 'E3': 52, 'E4': 50, 'E5': 1, 'SUB LEVEL': 3, 'CYCLE RISE': 93, 'CYCLE FALL': 92, 'CYCLE CURVE': -8,
            'AMPENV SATN': 3, 'FILTERENV MRES': 4, 'CYCLE PAN': 3, 'LFO2 MMODE': 5,
         }),
    dict(engine='LOFI', idx=8, name='ARP BASS', cat='BASS', bseed=16972,  # #12
         p={
            'ATK': 0, 'DEC': 69, 'SUS': 89, 'REL': 47, 'KTRK': 0, 'TRANS': -12, 'VOICE': 2, 'FREEA': 51, 'FREEB': 21, 'E2ATK': 0, 'E2DEC': 71,
            'E2SUS': 0, 'E2REL': 51, 'L2RATE': 40, 'L2WAVE': 1, 'EQHG': -4, 'EQHF': -8, 'DKDEP': 14, 'TRIM': 97, 'DKSRC': 0, 'E2FLT': 17, 'L2SHP': 11,
            'E1': 2, 'E3': 40, 'E7': 1, 'E8': 84, 'MOD1': ('AT', 'SHP', 17), 'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 31,
            'SUB WAVE': 2, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         },
         bend={
            'ATK': 4, 'DEC': 76, 'SUS': 97, 'REL': 42, 'ED_SHP': 6, 'LRATE': 63, 'LPHASE': 6, 'LFADE': 4, 'LD_FLT': -8, 'LD_SHP': -5, 'LD_AMP': 3,
            'KTRK': 2, 'DIST': 3, 'CHOR': 4, 'DLY': 7, 'REV': 7, 'DETUNE': 43, 'FREEA': 50, 'FREEB': 28, 'FREEC': 6, 'M1AMT': 12, 'M2AMT': 0,
            'M3AMT': 17, 'E2ATK': 3, 'E2DEC': 65, 'E2SUS': 2, 'E2REL': 46, 'L2RATE': 48, 'L2PHASE': 8, 'L2FADE': 1, 'DKDEP': 7, 'E2FLT': 11,
            'E3': 35, 'E4': 5, 'E5': 4, 'E6': 8, 'E8': 82, 'SUB LEVEL': 28, 'CYCLE RISE': 97, 'CYCLE FALL': 102, 'CYCLE CURVE': 5, 'AMPENV SATN': 1,
            'FILTERENV MRES': -3, 'CYCLE PAN': -8, 'LFO2 MMODE': 5,
         }),
    dict(engine='LOFI', idx=0, name='CARTRIDGE', cat='BASS', bseed=28474,  # #13
         p={
            'DEC': 73, 'SUS': 102, 'REL': 51, 'KTRK': 0, 'TRANS': -12, 'VOICE': 2, 'FREEA': 47, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 69, 'E2SUS': 0,
            'E2REL': 51, 'L2RATE': 17, 'L2WAVE': 1, 'EQHG': -4, 'EQHF': -8, 'DKDEP': 15, 'TRIM': 99, 'DKSRC': 0, 'E2FLT': 17, 'L2FLT': 4, 'E2': 4,
            'E3': 67, 'E8': 72, 'MOD1': ('AT', 'E8', 17), 'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 41, 'CYCLE RISE': 100,
            'CYCLE FALL': 100,
         },
         bend={
            'ATK': 13, 'DEC': 68, 'SUS': 109, 'REL': 54, 'ED_SHP': 3, 'LRATE': 52, 'LPHASE': 7, 'LFADE': 6, 'LD_FLT': 2, 'LD_SHP': -2, 'LD_AMP': 5,
            'KTRK': 8, 'DIST': 6, 'CHOR': 8, 'DLY': 4, 'REV': 5, 'DETUNE': 39, 'FREEA': 45, 'FREEB': 9, 'FREEC': 2, 'M1AMT': 21, 'M2AMT': 3,
            'M3AMT': 10, 'E2ATK': 7, 'E2DEC': 68, 'E2SUS': 4, 'E2REL': 43, 'L2RATE': 25, 'L2PHASE': 3, 'L2FADE': 6, 'DKDEP': 16, 'E2FLT': 25,
            'E3': 59, 'E4': 6, 'E5': 5, 'E6': 5, 'E8': 64, 'SUB LEVEL': 48, 'CYCLE RISE': 106, 'CYCLE FALL': 93, 'CYCLE CURVE': -3, 'AMPENV SATN': 1,
            'FILTERENV MRES': -7, 'CYCLE PAN': 8, 'LFO2 MMODE': -8,
         }),
    dict(engine='VOICE', idx=0, name='BULLFROG', cat='BASS', bseed=36283,  # #14
         p={
            'ATK': 15, 'DEC': 79, 'SUS': 102, 'REL': 59, 'KTRK': 0, 'TRANS': -12, 'DIST': 24, 'VOICE': 2, 'GLIDE': 23, 'GLMODE': 1, 'FREEA': 52,
            'FREEB': 17, 'E2ATK': 0, 'E2DEC': 76, 'E2SUS': 0, 'L2RATE': 21, 'L2WAVE': 1, 'EQHG': -4, 'EQHF': -8, 'DKDEP': 15, 'TRIM': 94, 'DKSRC': 0,
            'E2FLT': 17, 'L2FLT': 4, 'E1': 95, 'E2': 6, 'E3': 62, 'E4': -4, 'E5': 96, 'E6': 0, 'E7': 84, 'MOD1': ('AT', 'E4', 5),
            'MOD2': ('CYCLE', 'SHP', -9), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 97, 'CYCLE FALL': 97,
         },
         bend={
            'ATK': 7, 'DEC': 86, 'SUS': 106, 'REL': 67, 'ED_SHP': -4, 'LRATE': 67, 'LPHASE': 8, 'LFADE': 1, 'LD_FLT': 3, 'LD_SHP': -8, 'LD_AMP': 8,
            'KTRK': 2, 'DIST': 19, 'CHOR': 3, 'DLY': 6, 'REV': 4, 'DETUNE': 44, 'FREEA': 57, 'FREEB': 9, 'FREEC': 1, 'M1AMT': -1, 'M2AMT': -14,
            'M3AMT': 9, 'E2ATK': 1, 'E2DEC': 71, 'E2SUS': 1, 'E2REL': 57, 'L2RATE': 14, 'L2PHASE': 7, 'L2FADE': 4, 'DKDEP': 20, 'E2FLT': 25,
            'E1': 103, 'E2': 8, 'E3': 54, 'E4': -3, 'E5': 98, 'E6': 6, 'E7': 83, 'E8': 2, 'SUB LEVEL': 6, 'CYCLE RISE': 91, 'CYCLE FALL': 100,
            'CYCLE CURVE': -8, 'AMPENV SATN': -2, 'FILTERENV MRES': -1, 'CYCLE PAN': -4, 'LFO2 MMODE': 5,
         }),
    dict(engine='TRIO', idx=0, name='BREADBIN', cat='BASS', bseed=30953,  # #15
         p={
            'DEC': 74, 'SUS': 108, 'REL': 55, 'RES2': 41, 'KTRK': 0, 'TRANS': -12, 'VOICE': 2, 'GLIDE': 22, 'GLMODE': 1, 'TWTYPE': 1, 'FREEA': 47,
            'FREEB': 11, 'E2ATK': 19, 'E2DEC': 75, 'E2SUS': 28, 'E2REL': 56, 'L2RATE': 27, 'L2WAVE': 1, 'EQHG': -4, 'EQHF': -8, 'CUT2': 37,
            'DKDEP': 15, 'TRIM': 101, 'DKSRC': 0, 'E4': 11, 'MOD1': ('AT', 'E4', 17), 'MOD2': ('VEL', 'CUT2', 6), 'MOD3': ('CYCLE', 'CUT2', 7),
            'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 19, 'TWIN LFO2': 4, 'SUB LEVEL': 31, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         },
         bend={
            'ATK': 3, 'DEC': 80, 'SUS': 116, 'REL': 51, 'ED_SHP': 3, 'LRATE': 63, 'LPHASE': 6, 'LFADE': 5, 'LD_FLT': -7, 'LD_SHP': 5, 'LD_AMP': 8,
            'RES2': 38, 'DRV2': 3, 'KTRK': 8, 'DIST': 3, 'CHOR': 7, 'DLY': 3, 'REV': 2, 'DETUNE': 37, 'FREEA': 46, 'FREEB': 10, 'FREEC': 4,
            'M1AMT': 9, 'M2AMT': 8, 'M3AMT': 0, 'E2ATK': 24, 'E2DEC': 82, 'E2SUS': 30, 'E2REL': 48, 'L2RATE': 33, 'L2PHASE': 8, 'L2FADE': 6,
            'BLEND': -8, 'DKDEP': 19, 'E2FLT': 6, 'E4': 13, 'E8': 72, 'TWIN KTRK2': 2, 'TWIN ENV2': 23, 'TWIN LFO1': -6, 'TWIN LFO2': -4,
            'SUB LEVEL': 26, 'CYCLE RISE': 103, 'CYCLE FALL': 104, 'CYCLE CURVE': -6, 'AMPENV SATN': -1, 'FILTERENV FBLND': -2, 'FILTERENV FDRV': -1,
            'FILTERENV MRES': -8, 'CYCLE TCUT': 2, 'CYCLE PAN': -3, 'LFO2 TRES': -7, 'LFO2 MMODE': 3, 'LFO2 FBLND': 8,
         }),
    dict(engine='TRIO', idx=11, name='SID BASS', cat='BASS', bseed=7135,  # #16
         p={
            'DEC': 76, 'SUS': 108, 'REL': 54, 'RES2': 31, 'KTRK': 0, 'TRANS': -12, 'VOICE': 2, 'GLIDE': 19, 'GLMODE': 1, 'TWTYPE': 1, 'FREEA': 52,
            'FREEB': 11, 'E2ATK': 0, 'E2DEC': 74, 'E2SUS': 32, 'E2REL': 56, 'L2RATE': 59, 'L2WAVE': 1, 'EQHG': -4, 'EQHF': -8, 'CUT2': 46, 'TRIM': 93,
            'L2SHP': 18, 'E1': 1, 'E4': 7, 'E8': 40, 'MOD1': ('AT', 'SHP', 17), 'MOD2': ('VEL', 'CUT2', 6), 'MOD3': ('CYCLE', 'CUT2', 7), 'LTRIG': 1,
            'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 17, 'SUB LEVEL': 31, 'SUB WAVE': 2, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         },
         bend={
            'ATK': 9, 'DEC': 84, 'SUS': 107, 'REL': 59, 'ED_SHP': 8, 'LRATE': 63, 'LPHASE': 4, 'LFADE': 5, 'LD_FLT': 2, 'LD_SHP': -7, 'LD_AMP': 1,
            'RES2': 42, 'DRV2': 3, 'KTRK': 4, 'DIST': 7, 'CHOR': 5, 'DLY': 2, 'REV': 5, 'DETUNE': 38, 'FREEA': 57, 'FREEB': 10, 'FREEC': 2,
            'M1AMT': 13, 'M2AMT': -1, 'M3AMT': 3, 'M4AMT': 11, 'E2ATK': 5, 'E2DEC': 82, 'E2SUS': 36, 'E2REL': 63, 'L2RATE': 58, 'L2PHASE': 4,
            'L2FADE': 1, 'BLEND': 8, 'CUT2': 49, 'E2FLT': 3, 'E4': 9, 'E8': 41, 'TWIN KTRK2': 1, 'TWIN ENV2': 24, 'TWIN LFO1': -5, 'TWIN LFO2': -7,
            'SUB LEVEL': 32, 'CYCLE RISE': 107, 'CYCLE FALL': 94, 'CYCLE CURVE': -6, 'AMPENV SATN': -5, 'FILTERENV FBLND': -3, 'FILTERENV FDRV': -3,
            'FILTERENV MRES': -5, 'CYCLE TCUT': 3, 'CYCLE PAN': -7, 'LFO2 TRES': -5, 'LFO2 MMODE': 2, 'LFO2 FBLND': -2,
         }),
    dict(engine='TRIO', idx=12, name='SYNC BASS', cat='BASS', bseed=43701,  # #17
         p={
            'DEC': 79, 'SUS': 102, 'REL': 44, 'RES2': 87, 'DRV2': 21, 'KTRK': 0, 'TRANS': -12, 'DIST': 31, 'VOICE': 2, 'GLIDE': 18, 'GLMODE': 1,
            'TWTYPE': 5, 'FREEA': 31, 'FREEB': 77, 'E2ATK': 0, 'E2SUS': 0, 'E2REL': 54, 'L2RATE': 30, 'L2WAVE': 1, 'EQHG': -4, 'EQHF': -8, 'CUT2': 40,
            'TRIM': 114, 'E2FLT': 31, 'E1': 9, 'E2': 7, 'E3': 0, 'E4': 0, 'MOD1': ('AT', 'FDRV', 21), 'MOD2': ('VEL', 'CUT2', 6),
            'MOD3': ('ENV2', 'E2', 17), 'MOD4': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 31, 'TWIN LFO2': 5,
            'SUB LEVEL': 41, 'SUB WAVE': 2, 'CYCLE RISE': 41, 'CYCLE FALL': 41, 'CYCLE SYNC': 1,
         },
         bend={
            'ATK': 4, 'DEC': 81, 'SUS': 107, 'REL': 51, 'ED_SHP': 6, 'LRATE': 56, 'LPHASE': 4, 'LFADE': 5, 'LD_FLT': 1, 'LD_SHP': 7, 'LD_AMP': 8,
            'RES2': 81, 'DRV2': 19, 'KTRK': -4, 'DIST': 36, 'CHOR': 2, 'DLY': 1, 'REV': 3, 'DETUNE': 41, 'FREEA': 32, 'FREEC': 3, 'M1AMT': 28,
            'M2AMT': 13, 'M3AMT': 12, 'M4AMT': 14, 'E2ATK': 2, 'E2DEC': 74, 'E2SUS': 6, 'E2REL': 62, 'L2RATE': 25, 'L2PHASE': 6, 'L2FADE': 1,
            'BLEND': -4, 'CUT2': 45, 'E2FLT': 29, 'E4': 2, 'E8': 60, 'TWIN KTRK2': 8, 'TWIN ENV2': 35, 'TWIN LFO1': -8, 'TWIN LFO2': 6,
            'SUB LEVEL': 35, 'CYCLE RISE': 37, 'CYCLE FALL': 43, 'CYCLE CURVE': 5, 'AMPENV SATN': -3, 'FILTERENV FBLND': -6, 'FILTERENV FDRV': 6,
            'FILTERENV MRES': -1, 'CYCLE TCUT': -2, 'CYCLE PAN': 2, 'LFO2 TRES': -3, 'LFO2 MMODE': -2, 'LFO2 FBLND': -8,
         }),
    dict(engine='WHEEL', idx=0, name='PEDAL BASS', cat='BASS', bseed=14321,  # #18
         p={
            'DEC': 94, 'SUS': 127, 'REL': 44, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 4, 'KTRK': 0, 'TRANS': -12, 'DIST': 30, 'VOICE': 2, 'FREEA': 36,
            'FREEB': 11, 'E2ATK': 0, 'E2DEC': 62, 'E2SUS': 0, 'E2REL': 47, 'ITYPE': 9, 'IA': 18, 'IB': 90, 'IC': 40, 'EQHG': -4, 'EQHF': -8,
            'TRIM': 104, 'E2FLT': 11, 'E2SHP': 26, 'E1': 14, 'E3': -4, 'E4': -8, 'E6': 50, 'E7': 30, 'E8': 0, 'MOD1': ('AT', 'E7', 21),
            'MOD2': ('CYCLE', 'CUT', 11), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 80, 'CYCLE CURVE': -21,
         },
         bend={
            'ATK': 2, 'DEC': 90, 'SUS': 124, 'REL': 47, 'ED_SHP': -2, 'LRATE': 13, 'LPHASE': 2, 'LFADE': 4, 'LD_FLT': 9, 'LD_SHP': -2, 'LD_AMP': 1,
            'KTRK': 3, 'DIST': 27, 'CHOR': 7, 'DLY': 3, 'REV': 6, 'DETUNE': 39, 'FREEA': 41, 'FREEB': 3, 'FREEC': 6, 'M1AMT': 15, 'M2AMT': 18,
            'M3AMT': 34, 'E2ATK': 4, 'E2DEC': 54, 'E2SUS': 6, 'E2REL': 45, 'L2RATE': 66, 'L2PHASE': 6, 'L2FADE': 4, 'IA': 21, 'IB': 88, 'IC': 36,
            'IMIX': 119, 'E2FLT': 8, 'E3': -3, 'E4': -7, 'E6': 46, 'E7': 38, 'SUB LEVEL': 2, 'CYCLE RISE': 8, 'CYCLE FALL': 76, 'CYCLE CURVE': -15,
            'AMPENV SATN': -2, 'FILTERENV MRES': 6, 'CYCLE PAN': -4, 'LFO2 MMODE': 7,
         }),
    dict(engine='PHYS', idx=11, name='PLUCK BASS', cat='BASS', bseed=13381,  # #19
         p={
            'ATK': 0, 'SUS': 127, 'REL': 65, 'KTRK': 0, 'TRANS': -12, 'REV': 9, 'FREEA': 36, 'FREEB': 17, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 67,
            'E2SUS': 0, 'E2REL': 56, 'L2RATE': 27, 'L2WAVE': 1, 'ITYPE': 1, 'IA': 100, 'IB': 127, 'IC': 112, 'EQHG': -4, 'EQHF': -8, 'TRIM': 106,
            'E2FLT': 31, 'L2FLT': 4, 'E1': 1, 'E2': 61, 'E3': 47, 'E4': 118, 'E5': 41, 'E6': 127, 'E8': 11, 'MOD1': ('AT', 'E3', 11),
            'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         },
         bend={
            'ATK': 5, 'DEC': 74, 'SUS': 119, 'REL': 68, 'ED_SHP': 5, 'LRATE': 67, 'LPHASE': 6, 'LFADE': 4, 'LD_FLT': -8, 'LD_SHP': -6, 'LD_AMP': 4,
            'KTRK': -3, 'DIST': 1, 'CHOR': 1, 'DLY': 1, 'REV': 3, 'DETUNE': 33, 'FREEA': 38, 'FREEB': 10, 'FREEC': 4, 'M1AMT': 13, 'M2AMT': 3,
            'M3AMT': 12, 'E2ATK': 4, 'E2DEC': 65, 'E2SUS': 7, 'E2REL': 55, 'L2RATE': 35, 'L2PHASE': 6, 'L2FADE': 2, 'IA': 101, 'IB': 125, 'IC': 115,
            'IMIX': 123, 'E2FLT': 28, 'E2': 59, 'E3': 49, 'E4': 112, 'E5': 34, 'E6': 120, 'E7': 2, 'E8': 19, 'SUB LEVEL': 7, 'CYCLE RISE': 98,
            'CYCLE FALL': 105, 'CYCLE CURVE': -2, 'AMPENV SATN': -5, 'FILTERENV MRES': 7, 'CYCLE PAN': -4, 'LFO2 MMODE': 5,
         }),
    dict(engine='NOISE', idx=0, name='GEARBOX', cat='BASS', bseed=14165,  # #20
         p={
            'DEC': 73, 'SUS': 108, 'REL': 55, 'LWAVE': 3, 'LD_FLT': 7, 'RES2': 31, 'TRANS': -12, 'VOICE': 2, 'TWTYPE': 1, 'FREEA': 56, 'FREEB': 21,
            'E2ATK': 19, 'E2DEC': 76, 'E2SUS': 32, 'E2REL': 56, 'LSYNC': 8, 'EQHG': -4, 'EQHF': -8, 'CUT2': -11, 'DKDEP': 41, 'TRIM': 95, 'DKSRC': 0,
            'E2FLT': 17, 'E1': 3, 'E2': 0, 'E6': 48, 'MOD1': ('AT', 'FRES', 21), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('VEL', 'CUT', 6), 'LTRIG': 1,
            'L2TRIG': 1, 'KTRK': 32, 'SUB LEVEL': 31, 'SUB WAVE': 2, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         },
         bend={
            'ATK': 8, 'DEC': 70, 'SUS': 106, 'REL': 62, 'ED_SHP': 3, 'LRATE': 66, 'LPHASE': 1, 'LFADE': 2, 'LD_FLT': 0, 'LD_SHP': 5, 'LD_AMP': 6,
            'RES2': 29, 'DRV2': 4, 'KTRK': 28, 'DIST': 2, 'CHOR': 8, 'DLY': 3, 'REV': 1, 'DETUNE': 42, 'FREEA': 55, 'FREEB': 26, 'FREEC': 3,
            'M1AMT': 28, 'M2AMT': 15, 'M3AMT': 10, 'E2ATK': 15, 'E2DEC': 81, 'E2SUS': 35, 'E2REL': 52, 'L2RATE': 55, 'L2PHASE': 4, 'L2FADE': 8,
            'BLEND': 2, 'CUT2': -13, 'DKDEP': 45, 'E2FLT': 15, 'E2': 3, 'E7': 3, 'TWIN KTRK2': 6, 'TWIN ENV2': -4, 'TWIN LFO1': 6, 'TWIN LFO2': -3,
            'SUB LEVEL': 35, 'CYCLE RISE': 99, 'CYCLE FALL': 94, 'CYCLE CURVE': 3, 'AMPENV SATN': 6, 'FILTERENV FBLND': 4, 'FILTERENV FDRV': 1,
            'FILTERENV MRES': -8, 'CYCLE TCUT': -1, 'CYCLE PAN': -7, 'LFO2 TRES': -2, 'LFO2 MMODE': 4, 'LFO2 FBLND': 4,
         }),
    dict(engine='WAVE', idx=11, name='GROWL BASS', cat='BASS', bseed=20849,  # #21
         p={
            'DEC': 79, 'SUS': 127, 'REL': 56, 'LRATE': 32, 'LWAVE': 1, 'LD_FLT': 8, 'RES2': 31, 'DRV2': 21, 'KTRK': 0, 'TRANS': -12, 'DIST': 25,
            'VOICE': 2, 'GLIDE': 21, 'GLMODE': 1, 'TWTYPE': 5, 'FREEA': 40, 'FREEB': 31, 'E2ATK': 0, 'E2DEC': 73, 'E2SUS': 0, 'E2REL': 56, 'L2SYNC': 4,
            'LPOL': 1, 'CUT2': -7, 'DKDEP': 71, 'TRIM': 124, 'DKSRC': 0, 'E2FLT': 11, 'L2SHP': 19, 'E1': 20, 'E2': 40, 'MOD1': ('AT', 'SUBLV', 11),
            'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO1', 'E3', 13), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 62, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         },
         bend={
            'ATK': 13, 'DEC': 73, 'SUS': 123, 'REL': 51, 'ED_SHP': 3, 'LRATE': 38, 'LPHASE': 4, 'LFADE': 5, 'LD_FLT': 11, 'LD_SHP': 6, 'LD_AMP': 2,
            'RES2': 27, 'DRV2': 15, 'KTRK': -5, 'DIST': 26, 'CHOR': 2, 'DLY': 4, 'REV': 2, 'DETUNE': 44, 'FREEB': 32, 'FREEC': 1, 'M1AMT': 10,
            'M2AMT': 10, 'M3AMT': 10, 'M4AMT': 11, 'E2ATK': 3, 'E2DEC': 68, 'E2SUS': 4, 'E2REL': 53, 'L2RATE': 53, 'L2PHASE': 8, 'L2FADE': 4,
            'BLEND': -8, 'CUT2': -1, 'DKDEP': 66, 'E2FLT': 4, 'E2': 47, 'E3': 2, 'E4': 4, 'E6': 7, 'TWIN KTRK2': -5, 'TWIN ENV2': 3, 'TWIN LFO1': 7,
            'TWIN LFO2': -4, 'SUB LEVEL': 67, 'CYCLE RISE': 94, 'CYCLE FALL': 105, 'CYCLE CURVE': 6, 'AMPENV SATN': 3, 'FILTERENV FBLND': -3,
            'FILTERENV FDRV': -2, 'FILTERENV MRES': 6, 'CYCLE TCUT': -5, 'CYCLE PAN': -2, 'LFO2 TRES': -1, 'LFO2 MMODE': -8, 'LFO2 FBLND': -8,
         }),
    dict(engine='WAVE', idx=8, name='HARM BASS', cat='BASS', bseed=35823,  # #22
         p={
            'DEC': 79, 'SUS': 108, 'REL': 54, 'KTRK': 0, 'TRANS': -12, 'VOICE': 2, 'GLIDE': 21, 'GLMODE': 1, 'FREEA': 36, 'FREEB': 31, 'FREED': 2,
            'E2ATK': 0, 'E2DEC': 76, 'E2SUS': 25, 'E2REL': 56, 'L2RATE': 27, 'L2WAVE': 1, 'TRIM': 103, 'E2FLT': 21, 'E2SHP': 29, 'L2FLT': 4, 'E1': 2,
            'E2': 45, 'MOD1': ('AT', 'SAT', 9), 'MOD2': ('VEL', 'SHP', 13), 'MOD3': ('ENV2', 'E3', 11), 'MOD4': ('CYCLE', 'SHP', 11), 'LTRIG': 1,
            'L2TRIG': 1, 'SUB LEVEL': 70, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         },
         bend={
            'ATK': 14, 'DEC': 86, 'SUS': 116, 'REL': 61, 'ED_SHP': -4, 'LRATE': 58, 'LPHASE': 5, 'LFADE': 4, 'LD_FLT': -8, 'LD_SHP': 3, 'LD_AMP': 6,
            'KTRK': 7, 'DIST': 6, 'CHOR': 8, 'DLY': 3, 'REV': 4, 'DETUNE': 41, 'FREEB': 34, 'FREEC': 1, 'M1AMT': 12, 'M2AMT': 12, 'M3AMT': 8,
            'M4AMT': 18, 'M5AMT': 34, 'M6AMT': 5, 'E2ATK': 1, 'E2DEC': 68, 'E2SUS': 20, 'E2REL': 57, 'L2RATE': 20, 'L2PHASE': 8, 'L2FADE': 4,
            'E2FLT': 14, 'E2': 37, 'E3': 5, 'E4': 7, 'E6': 2, 'SUB LEVEL': 67, 'CYCLE RISE': 102, 'CYCLE FALL': 104, 'CYCLE CURVE': -3,
            'AMPENV SATN': -7, 'FILTERENV MRES': 4, 'CYCLE PAN': -8, 'LFO2 MMODE': 2,
         }),
    dict(engine='WAVE', idx=21, name='KICK BASS', cat='BASS', bseed=3922,  # #23
         p={
            'ATK': 0, 'DEC': 82, 'SUS': 70, 'REL': 56, 'KTRK': 0, 'TRANS': -12, 'DIST': 41, 'VOICE': 2, 'GLMODE': 1, 'E2ATK': 0, 'E2DEC': 63,
            'E2SUS': 0, 'E2REL': 47, 'ITYPE': 2, 'IA': 112, 'IB': 90, 'EQHG': -4, 'EQHF': -8, 'TRIM': 107, 'E1': 2, 'E2': 10,
            'MOD1': ('AT', 'SHP', 16), 'MOD2': ('ENV2', 'PITCH', 63), 'MOD3': ('ENV2', 'PITCH', 63), 'MOD4': ('EXPR', 'PITCH', -64), 'LTRIG': 1,
            'L2TRIG': 1, 'SUB LEVEL': 74,
         },
         bend={
            'ATK': 6, 'DEC': 77, 'SUS': 63, 'REL': 53, 'ED_SHP': -8, 'LRATE': 58, 'LPHASE': 8, 'LFADE': 4, 'LD_FLT': -2, 'LD_SHP': -6, 'LD_AMP': 7,
            'KTRK': 5, 'DIST': 38, 'CHOR': 3, 'DLY': 7, 'REV': 6, 'DETUNE': 38, 'FREEA': 77, 'FREEB': 4, 'FREEC': 7, 'M1AMT': 23, 'E2ATK': 4,
            'E2DEC': 67, 'E2SUS': 5, 'E2REL': 50, 'L2RATE': 65, 'L2PHASE': 3, 'L2FADE': 2, 'IA': 113, 'IB': 96, 'IC': 97, 'IMIX': 125, 'E2FLT': -4,
            'E2': 13, 'E3': 6, 'E4': 4, 'E6': 1, 'SUB LEVEL': 73, 'CYCLE RISE': 3, 'CYCLE FALL': 67, 'CYCLE CURVE': -4, 'AMPENV SATN': -1,
            'FILTERENV MRES': -2, 'CYCLE PAN': 4, 'LFO2 MMODE': 4,
         }),
    dict(engine='WAVE', idx=12, name='REESE BASS', cat='BASS', bseed=5220,  # #24
         p={
            'ATK': 0, 'DEC': 79, 'SUS': 127, 'REL': 44, 'LRATE': 26, 'LWAVE': 1, 'LD_FLT': 8, 'LD_SHP': 13, 'RES2': 21, 'DRV2': 31, 'KTRK': 0,
            'TRANS': -12, 'DIST': 10, 'VOICE': 2, 'GLIDE': 26, 'GLMODE': 1, 'TWTYPE': 1, 'FREEA': 46, 'FREEB': 21, 'E2ATK': 0, 'E2DEC': 69, 'E2SUS': 0,
            'E2REL': 56, 'L2RATE': 15, 'L2WAVE': 1, 'CUT2': -8, 'DKDEP': 41, 'TRIM': 110, 'DKSRC': 0, 'E2FLT': 11, 'E1': 18, 'E2': 20, 'E6': 25,
            'MOD1': ('AT', 'E6', 17), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO2', 'E6', 17), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 65,
            'CYCLE RISE': 90, 'CYCLE FALL': 90,
         },
         bend={
            'ATK': 6, 'DEC': 75, 'SUS': 126, 'REL': 38, 'ED_SHP': -4, 'LRATE': 19, 'LPHASE': 2, 'LFADE': 4, 'LD_FLT': 7, 'LD_SHP': 6, 'LD_AMP': 4,
            'RES2': 16, 'DRV2': 36, 'KTRK': 4, 'DIST': 15, 'CHOR': 4, 'DLY': 3, 'REV': 3, 'DETUNE': 37, 'FREEA': 45, 'FREEB': 24, 'FREEC': 7,
            'M1AMT': 20, 'M2AMT': 2, 'M3AMT': 16, 'E2ATK': 6, 'E2DEC': 61, 'E2SUS': 8, 'E2REL': 50, 'L2RATE': 10, 'L2PHASE': 3, 'L2FADE': 4,
            'BLEND': -4, 'CUT2': -7, 'DKDEP': 38, 'E2FLT': 6, 'E2': 22, 'E3': 5, 'E4': 3, 'E6': 24, 'TWIN KTRK2': -7, 'TWIN ENV2': 2,
            'TWIN LFO1': -3, 'TWIN LFO2': -4, 'SUB LEVEL': 67, 'CYCLE RISE': 93, 'CYCLE FALL': 94, 'CYCLE CURVE': 6, 'AMPENV SATN': -4,
            'FILTERENV FBLND': -3, 'FILTERENV FDRV': -4, 'FILTERENV MRES': 8, 'CYCLE TCUT': 6, 'CYCLE PAN': -4, 'LFO2 TRES': -5, 'LFO2 MMODE': 2,
            'LFO2 FBLND': 7,
         }),
    dict(engine='ANALOG', idx=19, name='DRIVEN SAW', cat='LEAD', bseed=64169,  # #25
         p={
            'ATK': 0, 'DEC': 71, 'SUS': 127, 'REL': 51, 'RES2': 17, 'DRV2': 71, 'DLY': 17, 'REV': 21, 'VOICE': 2, 'GLIDE': 17, 'GLMODE': 1,
            'TWTYPE': 3, 'FREEA': 71, 'FREEB': 37, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 71, 'E2SUS': 31, 'E2REL': 51, 'CUT2': 11, 'TRIM': 94, 'E2FLT': 47,
            'E2': 37, 'E3': 107, 'E7': 17, 'MOD1': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'SUB LEVEL': 47, 'SUB WAVE': 2,
            'CYCLE RISE': 97, 'CYCLE FALL': 97,
         },
         bend={
            'ATK': 16, 'DEC': 84, 'SUS': 125, 'REL': 34, 'ED_SHP': -18, 'LRATE': 41, 'LPHASE': 4, 'LFADE': 16, 'LD_FLT': 1, 'LD_SHP': -10,
            'LD_AMP': 3, 'RES2': 35, 'DRV2': 62, 'KTRK': 45, 'DIST': 9, 'CHOR': 1, 'DLY': 36, 'REV': 37, 'DETUNE': 41, 'FREEA': 69, 'FREEB': 35,
            'FREEC': 11, 'M1AMT': -7, 'E2ATK': 15, 'E2DEC': 92, 'E2SUS': 44, 'E2REL': 46, 'L2RATE': 53, 'L2PHASE': 19, 'L2FADE': 13, 'BLEND': 9,
            'CUT2': 13, 'E2FLT': 58, 'E2': 42, 'E3': 91, 'E4': 14, 'E7': 28, 'TWIN KTRK2': 3, 'TWIN ENV2': -9, 'TWIN LFO1': -21, 'TWIN LFO2': -18,
            'SUB LEVEL': 29, 'CYCLE RISE': 112, 'CYCLE FALL': 82, 'CYCLE CURVE': -17, 'AMPENV SATN': 1, 'FILTERENV FBLND': -12,
            'FILTERENV FDRV': -19, 'FILTERENV MRES': 13, 'CYCLE TCUT': -17, 'CYCLE PAN': 2, 'LFO2 TRES': -14, 'LFO2 MMODE': -4, 'LFO2 FBLND': 4,
         }),
    dict(engine='ANALOG', idx=7, name='FUNFARE', cat='LEAD', bseed=63704,  # #26
         p={
            'ATK': 72, 'DEC': 79, 'SUS': 127, 'REL': 64, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 4, 'REV': 25, 'FREEA': 31, 'FREEB': 11, 'E2ATK': 87,
            'E2DEC': 91, 'E2SUS': 71, 'E2REL': 69, 'L2RATE': 87, 'L2WAVE': 1, 'L2FADE': 83, 'ITYPE': 8, 'IA': 40, 'IB': 80, 'IC': 20, 'IMIX': 70,
            'TRIM': 99, 'E2FLT': 37, 'L2PIT': 1, 'E2': 14, 'MOD1': ('AT', 'CUT', 17), 'MOD2': ('VEL', 'CUT', 10), 'MOD3': ('CYCLE', 'CUT', 7),
            'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 110, 'CYCLE FALL': 110,
         },
         bend={
            'ATK': 75, 'DEC': 90, 'SUS': 119, 'REL': 54, 'ED_SHP': 12, 'LRATE': 13, 'LPHASE': 19, 'LFADE': 9, 'LD_FLT': 18, 'LD_SHP': 13,
            'LD_AMP': 1, 'KTRK': 13, 'DIST': 17, 'CHOR': 3, 'DLY': 17, 'REV': 20, 'DETUNE': 55, 'FREEA': 34, 'FREEB': 22, 'FREEC': 1, 'M1AMT': 14,
            'M2AMT': -4, 'M3AMT': -9, 'E2ATK': 90, 'E2DEC': 82, 'E2SUS': 63, 'E2REL': 56, 'L2RATE': 103, 'L2PHASE': 12, 'L2FADE': 93, 'IA': 29,
            'IB': 95, 'IC': 13, 'IMIX': 55, 'E2FLT': 50, 'E2': 5, 'E3': 63, 'E4': 13, 'E7': 19, 'SUB LEVEL': 8, 'CYCLE RISE': 109, 'CYCLE FALL': 114,
            'CYCLE CURVE': -10, 'AMPENV SATN': -3, 'FILTERENV MRES': -10, 'CYCLE PAN': 20, 'LFO2 MMODE': 6,
         }),
    dict(engine='ANALOG', idx=6, name='RAZORWIRE', cat='LEAD', bseed=61032,  # #27
         p={
            'ATK': 0, 'SUS': 127, 'REL': 69, 'LRATE': 40, 'LWAVE': 1, 'LD_FLT': 5, 'DRV2': 41, 'DLY': 18, 'VOICE': 2, 'GLIDE': 30, 'GLMODE': 1,
            'TWTYPE': 1, 'FREEA': 47, 'FREEB': 57, 'E2ATK': 0, 'E2DEC': 77, 'E2SUS': 0, 'E2REL': 69, 'L2RATE': 44, 'L2WAVE': 1, 'ITYPE': 6, 'IA': 44,
            'IMIX': 51, 'CUT2': 66, 'TRIM': 91, 'E2FLT': 17, 'E2SHP': 63, 'L2SHP': 16, 'E1': 5, 'E2': 37, 'E3': 102, 'E7': 36,
            'MOD1': ('AT', 'SHP', 26), 'MOD2': ('VEL', 'SHP', 13), 'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0,
            'TWIN LFO2': 4, 'CYCLE RISE': 97, 'CYCLE FALL': 97,
         },
         bend={
            'ATK': 16, 'DEC': 50, 'SUS': 111, 'REL': 88, 'ED_SHP': -20, 'LRATE': 19, 'LPHASE': 20, 'LFADE': 5, 'LD_FLT': -11, 'LD_SHP': 5,
            'LD_AMP': 1, 'DRV2': 33, 'KTRK': 45, 'DIST': 8, 'CHOR': 14, 'DLY': 7, 'REV': 4, 'DETUNE': 53, 'FREEA': 50, 'FREEC': 19, 'M1AMT': 47,
            'M2AMT': -8, 'M3AMT': 13, 'M4AMT': 48, 'M5AMT': 9, 'E2ATK': 10, 'E2DEC': 60, 'E2SUS': 15, 'E2REL': 56, 'L2RATE': 54, 'L2PHASE': 8,
            'L2FADE': 8, 'IA': 52, 'IB': 80, 'IC': 95, 'IMIX': 55, 'BLEND': 10, 'CUT2': 69, 'E2FLT': -1, 'E2': 24, 'E3': 122, 'E4': 9, 'E7': 55,
            'TWIN KTRK2': 4, 'TWIN ENV2': 15, 'TWIN LFO1': 1, 'TWIN LFO2': 23, 'SUB LEVEL': 9, 'CYCLE RISE': 89, 'CYCLE FALL': 85, 'CYCLE CURVE': 8,
            'AMPENV SATN': -11, 'FILTERENV FBLND': 17, 'FILTERENV FDRV': -16, 'FILTERENV MRES': 7, 'CYCLE TCUT': -15, 'CYCLE PAN': 9,
            'LFO2 TRES': 12, 'LFO2 MMODE': 21, 'LFO2 FBLND': 1,
         }),
    dict(engine='ANALOG', idx=17, name='SQUARE LEAD', cat='LEAD', bseed=21704,  # #28
         p={
            'ATK': 22, 'SUS': 127, 'REL': 73, 'LRATE': 40, 'LWAVE': 1, 'LD_FLT': 4, 'RES2': 107, 'DLY': 23, 'REV': 19, 'VOICE': 2, 'GLIDE': 28,
            'GLMODE': 1, 'TWTYPE': 1, 'FREEA': 44, 'FREEB': 77, 'E2ATK': 29, 'E2DEC': 86, 'E2SUS': 76, 'E2REL': 73, 'L2RATE': 30, 'L2WAVE': 1,
            'CUT2': 56, 'TRIM': 93, 'E1': 1, 'E2': 7, 'E3': 51, 'MOD1': ('AT', 'CUT2', 17), 'MOD2': ('VEL', 'CUT', 6), 'MOD3': ('CYCLE', 'CUT', 5),
            'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 17, 'TWIN LFO2': 4, 'CYCLE RISE': 97, 'CYCLE FALL': 97,
         },
         bend={
            'ATK': 10, 'DEC': 50, 'SUS': 119, 'REL': 76, 'ED_SHP': -20, 'LRATE': 30, 'LPHASE': 1, 'LFADE': 19, 'LD_FLT': -2, 'LD_SHP': 16,
            'LD_AMP': 4, 'RES2': 79, 'DRV2': 16, 'KTRK': 36, 'DIST': 16, 'CHOR': 18, 'DLY': 2, 'REV': 4, 'DETUNE': 49, 'FREEA': 57, 'FREEB': 63,
            'FREEC': 2, 'M1AMT': 28, 'M2AMT': 20, 'M3AMT': 15, 'E2ATK': 30, 'E2DEC': 70, 'E2SUS': 86, 'E2REL': 61, 'L2RATE': 12, 'L2PHASE': 12,
            'L2FADE': 8, 'BLEND': -7, 'CUT2': 67, 'E2FLT': -13, 'E2': 2, 'E3': 57, 'E4': 9, 'E7': 21, 'TWIN KTRK2': 4, 'TWIN ENV2': -4,
            'TWIN LFO1': -9, 'TWIN LFO2': 5, 'SUB LEVEL': 3, 'CYCLE RISE': 113, 'CYCLE FALL': 95, 'CYCLE CURVE': -14, 'AMPENV SATN': -18,
            'FILTERENV FBLND': 18, 'FILTERENV FDRV': -3, 'FILTERENV MRES': 6, 'CYCLE TCUT': 12, 'CYCLE PAN': -3, 'LFO2 TRES': 4, 'LFO2 MMODE': 12,
            'LFO2 FBLND': 20,
         }),
    dict(engine='ANALOG', idx=5, name='TORCH SONG', cat='LEAD', bseed=58621,  # #29
         p={
            'ATK': 44, 'SUS': 127, 'REL': 81, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 4, 'DIST': 7, 'CHOR': 38, 'REV': 51, 'VOICE': 2, 'GLIDE': 36,
            'GLMODE': 1, 'FREEA': 49, 'FREEB': 11, 'FREED': 2, 'E2ATK': 56, 'E2DEC': 88, 'E2SUS': 89, 'E2REL': 79, 'L2RATE': 87, 'L2WAVE': 1,
            'L2FADE': 86, 'TRIM': 93, 'E2FLT': 17, 'L2PIT': 1, 'E1': 2, 'E2': 7, 'E3': 32, 'MOD1': ('AT', 'DEP2', 11), 'MOD2': ('VEL', 'CUT', 6),
            'MOD3': ('CYCLE', 'CUT', 5), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 105, 'CYCLE FALL': 105,
         },
         bend={
            'ATK': 37, 'DEC': 87, 'SUS': 117, 'REL': 63, 'ED_SHP': -16, 'LRATE': 42, 'LPHASE': 3, 'LFADE': 11, 'LD_FLT': 13, 'LD_SHP': 4,
            'LD_AMP': 4, 'KTRK': 26, 'DIST': 27, 'CHOR': 57, 'DLY': 16, 'REV': 36, 'DETUNE': 28, 'FREEA': 45, 'FREEB': 22, 'FREEC': 1, 'M1AMT': -1,
            'M2AMT': 27, 'M3AMT': 15, 'E2ATK': 64, 'E2DEC': 104, 'E2SUS': 81, 'E2REL': 78, 'L2RATE': 101, 'L2PHASE': 18, 'L2FADE': 96, 'E2FLT': 34,
            'E2': 3, 'E3': 28, 'E4': 15, 'E7': 5, 'SUB LEVEL': 1, 'CYCLE RISE': 95, 'CYCLE FALL': 98, 'CYCLE CURVE': 13, 'AMPENV SATN': -13,
            'FILTERENV MRES': -13, 'CYCLE PAN': -7, 'LFO2 MMODE': -16,
         }),
    dict(engine='ANALOG', idx=8, name='WALL OF SAW', cat='LEAD', bseed=21203, bamt=15,  # #30
         p={
            'ATK': 22, 'SUS': 127, 'REL': 76, 'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 4, 'CHOR': 64, 'DLY': 32, 'REV': 44, 'TWTYPE': 3, 'FREEA': 47,
            'FREEB': 11, 'E2ATK': 0, 'E2DEC': 79, 'E2SUS': 0, 'E2REL': 76, 'L2RATE': 18, 'SPRD': 51, 'ITYPE': 1, 'IA': 80, 'IB': 127, 'IC': 112,
            'BLEND': -16, 'CUT2': 56, 'TRIM': 85, 'E2FLT': 11, 'E2': 25, 'MOD1': ('AT', 'FBLND', 17), 'MOD2': ('CYCLE', 'CUT', 7),
            'MOD3': ('LFO2', 'CUT', 2), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN ROUTE': 1, 'TWIN LINK': 0, 'CYCLE RISE': 110, 'CYCLE FALL': 110,
         },
         bend={
            'ATK': 41, 'DEC': 59, 'SUS': 121, 'REL': 91, 'ED_SHP': 2, 'LRATE': 32, 'LPHASE': 13, 'LFADE': 19, 'LD_FLT': -15, 'LD_SHP': 4,
            'LD_AMP': 10, 'RES2': 20, 'DRV2': 15, 'KTRK': 35, 'DIST': 4, 'CHOR': 60, 'DLY': 19, 'REV': 45, 'DETUNE': 49, 'FREEA': 50, 'FREEB': 14,
            'FREEC': 19, 'M1AMT': 27, 'M2AMT': -2, 'M3AMT': -1, 'E2ATK': 14, 'E2DEC': 90, 'E2SUS': 15, 'E2REL': 57, 'L2RATE': 30, 'L2PHASE': 3,
            'L2FADE': 10, 'IA': 97, 'IB': 124, 'IC': 110, 'IMIX': 121, 'BLEND': -14, 'E2FLT': 3, 'E2': 31, 'E3': 57, 'E4': 2, 'E7': 1,
            'TWIN KTRK2': -15, 'TWIN ENV2': -15, 'TWIN LFO1': -7, 'TWIN LFO2': -17, 'SUB LEVEL': 10, 'CYCLE RISE': 121, 'CYCLE FALL': 101,
            'CYCLE CURVE': -17, 'AMPENV SATN': 1, 'FILTERENV FBLND': 1, 'FILTERENV FDRV': -18, 'FILTERENV MRES': 10, 'CYCLE TCUT': 3,
            'CYCLE PAN': -17, 'LFO2 TRES': 18, 'LFO2 MMODE': -17, 'LFO2 FBLND': -6,
         }),
    dict(engine='FM6', idx=1, name='HAN SOLO', cat='LEAD', bseed=381,  # #31
         p={
            'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 4, 'DIST': 9, 'CHOR': 36, 'DLY': 28, 'REV': 19, 'VOICE': 2, 'GLIDE': 26, 'GLMODE': 1, 'FREEA': 50,
            'FREEB': 21, 'E2ATK': 66, 'E2DEC': 91, 'E2SUS': 110, 'E2REL': 69, 'L2RATE': 87, 'L2WAVE': 1, 'L2FADE': 83, 'TRIM': 115, 'E2FLT': 21,
            'L2PIT': 1, 'E1': 16, 'E2': -1, 'E3': -1, 'E5': 20, 'E6': 3, 'E8': 3, 'MOD1': ('AT', 'E3', 7), 'MOD2': ('CYCLE', 'CUT', 5),
            'MOD3': ('ENV2', 'E3', 4), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 105, 'CYCLE FALL': 105,
         },
         bend={
            'ATK': 60, 'DEC': 78, 'SUS': 94, 'REL': 70, 'ED_SHP': 15, 'LRATE': 12, 'LPHASE': 11, 'LFADE': 14, 'LD_FLT': 7, 'LD_SHP': -7, 'LD_AMP': 3,
            'KTRK': 22, 'DIST': 17, 'CHOR': 42, 'DLY': 43, 'REV': 33, 'DETUNE': 20, 'FREEA': 45, 'FREEB': 22, 'FREEC': 3, 'M1AMT': -11, 'M2AMT': 16,
            'M3AMT': 10, 'E2ATK': 71, 'E2DEC': 111, 'E2SUS': 90, 'E2REL': 52, 'L2RATE': 99, 'L2PHASE': 12, 'L2FADE': 62, 'E2FLT': 42, 'E2': -2,
            'E3': -6, 'E5': 21, 'E6': 4, 'E7': 1, 'SUB LEVEL': 4, 'CYCLE RISE': 90, 'CYCLE FALL': 98, 'CYCLE CURVE': 8, 'AMPENV SATN': -5,
            'FILTERENV MRES': -1, 'CYCLE PAN': -19, 'LFO2 MMODE': -20,
         },
         bfm=[
            50, 59, 45, 59, 96, 74, 75, 4, 68, 75, 51, 48, 47, 93, 82, 78, 5, 87, 61, 34, 34, 44, 92, 83, 77, 5, 70, 50, 44, 53, 76, 91, 99, 88, 9,
            88, 69, 55, 51, 72, 90, 85, 88, 8, 87, 56, 45, 39, 50, 98, 93, 91, 0, 97, 80, 60, 99, 60, 46, 50, 50, 21, 37, 6, 5,
         ]),
    dict(engine='PHASE', idx=1, name='BRAZEN', cat='LEAD', bseed=1618,  # #32
         p={
            'ATK': 43, 'DEC': 79, 'SUS': 108, 'REL': 66, 'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 4, 'RES2': 21, 'CHOR': 28, 'DLY': 23, 'REV': 23,
            'VOICE': 2, 'GLIDE': 26, 'GLMODE': 1, 'TWTYPE': 1, 'FREEA': 52, 'FREEB': 17, 'E2ATK': 66, 'E2DEC': 81, 'E2SUS': 64, 'E2REL': 64,
            'L2RATE': 87, 'L2WAVE': 1, 'L2FADE': 83, 'CUT2': -8, 'TRIM': 93, 'E2FLT': 21, 'L2PIT': 1, 'E3': 28, 'E4': 88, 'E5': 7,
            'MOD1': ('AT', 'SHP', 7), 'MOD2': ('CYCLE', 'CUT', 5), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 105, 'CYCLE FALL': 105,
         },
         bend={
            'ATK': 35, 'DEC': 99, 'SUS': 127, 'REL': 45, 'ED_SHP': -2, 'LRATE': 28, 'LPHASE': 4, 'LFADE': 19, 'LD_FLT': -6, 'LD_SHP': 18,
            'LD_AMP': 8, 'RES2': 40, 'DRV2': 1, 'KTRK': 36, 'DIST': 5, 'CHOR': 48, 'DLY': 11, 'REV': 35, 'DETUNE': 37, 'FREEA': 65, 'FREEB': 2,
            'FREEC': 6, 'M1AMT': 5, 'M2AMT': 23, 'E2ATK': 69, 'E2DEC': 82, 'E2SUS': 67, 'E2REL': 82, 'L2RATE': 76, 'L2PHASE': 12, 'L2FADE': 102,
            'BLEND': 5, 'CUT2': 9, 'E2FLT': 18, 'E3': 43, 'E4': 75, 'E5': 11, 'TWIN KTRK2': 13, 'TWIN ENV2': 8, 'TWIN LFO1': 10, 'TWIN LFO2': -6,
            'SUB LEVEL': 18, 'CYCLE RISE': 125, 'CYCLE FALL': 113, 'CYCLE CURVE': 10, 'AMPENV SATN': 2, 'FILTERENV FBLND': -20,
            'FILTERENV FDRV': -17, 'FILTERENV MRES': 16, 'CYCLE TCUT': 5, 'CYCLE PAN': -2, 'LFO2 TRES': -20, 'LFO2 MMODE': -6, 'LFO2 FBLND': 20,
         }),
    dict(engine='LOFI', idx=1, name='PIXEL HERO', cat='LEAD', bseed=16641, bamt=21,  # #33
         p={
            'ATK': 0, 'SUS': 127, 'REL': 47, 'LRATE': 21, 'LWAVE': 1, 'DLY': 28, 'REV': 15, 'VOICE': 2, 'GLIDE': 19, 'GLMODE': 1, 'TWTYPE': 4,
            'FREEA': 66, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 79, 'E2SUS': 0, 'E2REL': 56, 'L2RATE': 91, 'L2WAVE': 1, 'L2FADE': 79, 'CUT2': 56,
            'TRIM': 91, 'E2FLT': 11, 'E2SHP': 12, 'L2PIT': 1, 'E1': 4, 'E3': 49, 'E8': 110, 'MOD1': ('AT', 'E8', -17), 'MOD2': ('CYCLE', 'CUT', 5),
            'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO1': 7, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         },
         bend={
            'ATK': 22, 'DEC': 55, 'SUS': 102, 'REL': 41, 'ED_SHP': 9, 'LRATE': 46, 'LPHASE': 3, 'LFADE': 14, 'LD_FLT': -5, 'LD_SHP': -16,
            'LD_AMP': 1, 'RES2': 2, 'DRV2': 17, 'KTRK': 36, 'DIST': 2, 'CHOR': 2, 'DLY': 38, 'REV': 26, 'DETUNE': 38, 'FREEA': 71, 'FREEB': 1,
            'FREEC': 13, 'M1AMT': -22, 'M2AMT': 27, 'M3AMT': 33, 'E2ATK': 20, 'E2DEC': 71, 'E2SUS': 1, 'E2REL': 60, 'L2RATE': 105, 'L2PHASE': 14,
            'L2FADE': 65, 'BLEND': 17, 'CUT2': 42, 'E2FLT': 19, 'E5': 14, 'E6': 1, 'E8': 125, 'TWIN KTRK2': -8, 'TWIN ENV2': -5, 'TWIN LFO1': -15,
            'TWIN LFO2': -12, 'SUB LEVEL': 12, 'CYCLE RISE': 74, 'CYCLE FALL': 112, 'CYCLE CURVE': -4, 'AMPENV SATN': 7, 'FILTERENV FBLND': 23,
            'FILTERENV FDRV': -21, 'FILTERENV MRES': 21, 'CYCLE TCUT': -10, 'CYCLE PAN': 10, 'LFO2 TRES': -17, 'LFO2 MMODE': 20, 'LFO2 FBLND': -3,
         }),
    dict(engine='TRIO', idx=1, name='BANSHEE', cat='LEAD', bseed=16225,  # #34
         p={
            'ATK': 21, 'SUS': 127, 'REL': 68, 'LRATE': 40, 'LWAVE': 1, 'LD_FLT': 7, 'DRV2': 67, 'KTRK': 31, 'DLY': 13, 'REV': 28, 'VOICE': 2,
            'GLIDE': 28, 'GLMODE': 1, 'TWTYPE': 1, 'FREEA': 47, 'FREEB': 61, 'E2ATK': 74, 'E2DEC': 90, 'E2SUS': 38, 'E2REL': 69, 'L2RATE': 88,
            'L2WAVE': 1, 'L2FADE': 81, 'ITYPE': 7, 'IA': 34, 'IB': 60, 'IC': 40, 'IMIX': 57, 'CUT2': 67, 'TRIM': 97, 'E2FLT': 17, 'L2PIT': 1, 'E1': 9,
            'E2': 12, 'E3': 0, 'E4': 0, 'MOD1': ('AT', 'E4', 17), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('ENV2', 'E2', 17), 'LTRIG': 1, 'L2TRIG': 1,
            'TWIN LINK': 0, 'CYCLE RISE': 97, 'CYCLE FALL': 97,
         },
         bend={
            'ATK': 27, 'DEC': 90, 'SUS': 126, 'REL': 88, 'ED_SHP': 21, 'LRATE': 30, 'LPHASE': 2, 'LFADE': 6, 'LD_FLT': 21, 'LD_SHP': -5, 'LD_AMP': 3,
            'RES2': 20, 'DRV2': 84, 'KTRK': 17, 'DIST': 21, 'CHOR': 14, 'DLY': 28, 'REV': 8, 'DETUNE': 38, 'FREEA': 55, 'FREEC': 4, 'M1AMT': 16,
            'M2AMT': 20, 'M3AMT': 20, 'E2ATK': 92, 'E2DEC': 108, 'E2SUS': 36, 'E2REL': 87, 'L2RATE': 85, 'L2PHASE': 11, 'L2FADE': 99, 'IA': 33,
            'IB': 72, 'IC': 46, 'IMIX': 50, 'BLEND': 13, 'CUT2': 61, 'E2FLT': 15, 'E4': 5, 'E8': 60, 'TWIN KTRK2': -20, 'TWIN ENV2': -8,
            'TWIN LFO1': 12, 'TWIN LFO2': 10, 'SUB LEVEL': 20, 'CYCLE RISE': 104, 'CYCLE FALL': 83, 'CYCLE CURVE': -4, 'AMPENV SATN': -5,
            'FILTERENV FBLND': -17, 'FILTERENV FDRV': 19, 'FILTERENV MRES': -17, 'CYCLE TCUT': 13, 'CYCLE PAN': 15, 'LFO2 TRES': 5, 'LFO2 MMODE': 13,
            'LFO2 FBLND': -5,
         }),
    dict(engine='TRIO', idx=8, name='OBOE-ISH', cat='LEAD', bseed=1212,  # #35
         p={
            'ATK': 53, 'SUS': 127, 'REL': 66, 'LRATE': 21, 'LWAVE': 1, 'KTRK': 31, 'DLY': 22, 'REV': 34, 'VOICE': 2, 'GLIDE': 20, 'GLMODE': 1,
            'TWTYPE': 4, 'FREEA': 57, 'FREEB': 27, 'E2ATK': 47, 'E2DEC': 79, 'E2SUS': 89, 'E2REL': 64, 'L2RATE': 87, 'L2WAVE': 1, 'L2FADE': 85,
            'CUT2': 51, 'TRIM': 95, 'E2FLT': 11, 'L2PIT': 1, 'E1': 1, 'E3': 0, 'E4': 3, 'E8': 17, 'MOD1': ('AT', 'FRES', 17),
            'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO1': 11, 'CYCLE RISE': 91, 'CYCLE FALL': 91,
         },
         bend={
            'ATK': 40, 'DEC': 76, 'SUS': 115, 'REL': 50, 'ED_SHP': 6, 'LRATE': 16, 'LPHASE': 19, 'LFADE': 8, 'LD_FLT': -1, 'LD_SHP': 1, 'LD_AMP': 4,
            'DRV2': 14, 'KTRK': 39, 'DIST': 20, 'CHOR': 7, 'DLY': 36, 'REV': 41, 'DETUNE': 53, 'FREEA': 70, 'FREEB': 45, 'FREEC': 5, 'M1AMT': 9,
            'M2AMT': 1, 'E2ATK': 36, 'E2DEC': 87, 'E2SUS': 93, 'E2REL': 45, 'L2RATE': 88, 'L2PHASE': 5, 'L2FADE': 106, 'BLEND': -9, 'CUT2': 62,
            'E2FLT': 10, 'E4': 1, 'E8': 10, 'TWIN KTRK2': -15, 'TWIN ENV2': 11, 'TWIN LFO1': 4, 'TWIN LFO2': -20, 'SUB LEVEL': 17, 'CYCLE RISE': 108,
            'CYCLE FALL': 72, 'CYCLE CURVE': 5, 'AMPENV SATN': -4, 'FILTERENV FBLND': 14, 'FILTERENV FDRV': -3, 'FILTERENV MRES': 17,
            'CYCLE TCUT': 13, 'CYCLE PAN': -9, 'LFO2 TRES': -21, 'LFO2 MMODE': 21, 'LFO2 FBLND': 5,
         }),
    dict(engine='WHEEL', idx=1, name='SMOKE RING', cat='LEAD', bseed=53342, bamt=11,  # #36
         p={
            'SUS': 127, 'REL': 51, 'LRATE': 27, 'LWAVE': 1, 'LD_FLT': 5, 'KTRK': 31, 'DLY': 23, 'REV': 25, 'VOICE': 2, 'GLIDE': 23, 'GLMODE': 1,
            'FREEA': 64, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 83, 'E2SUS': 51, 'L2RATE': 92, 'L2WAVE': 1, 'L2FADE': 76, 'TRIM': 97, 'E2FLT': 7,
            'E2SHP': 19, 'L2PIT': 1, 'E1': 13, 'E4': 1, 'E6': 30, 'E7': 55, 'MOD1': ('AT', 'E7', 21), 'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1,
            'L2TRIG': 1, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         },
         bend={
            'ATK': 22, 'DEC': 82, 'SUS': 121, 'REL': 47, 'ED_SHP': -5, 'LRATE': 38, 'LPHASE': 6, 'LFADE': 6, 'LD_FLT': -3, 'LD_SHP': -5,
            'LD_AMP': 12, 'KTRK': 44, 'DIST': 9, 'CHOR': 13, 'DLY': 33, 'REV': 17, 'DETUNE': 39, 'FREEA': 72, 'FREEB': 22, 'FREEC': 1, 'M1AMT': 23,
            'M2AMT': 6, 'M3AMT': 14, 'E2ATK': 12, 'E2DEC': 85, 'E2SUS': 62, 'E2REL': 54, 'L2RATE': 84, 'L2PHASE': 5, 'L2FADE': 67, 'E2FLT': 3,
            'E3': 1, 'E4': 0, 'E6': 26, 'E7': 68, 'SUB LEVEL': 9, 'CYCLE RISE': 105, 'CYCLE FALL': 112, 'CYCLE CURVE': 9, 'AMPENV SATN': -7,
            'FILTERENV MRES': 2, 'CYCLE PAN': -10, 'LFO2 MMODE': -13,
         }),
    dict(engine='PHYS', idx=0, name='PAN WHISTLE', cat='LEAD', bseed=58847,  # #37
         p={
            'ATK': 56, 'SUS': 127, 'REL': 70, 'LRATE': 32, 'LD_FLT': 4, 'KTRK': 31, 'CHOR': 19, 'DLY': 25, 'REV': 38, 'VOICE': 2, 'GLIDE': 21,
            'GLMODE': 1, 'FREEA': 61, 'FREEB': 21, 'E2ATK': 51, 'E2DEC': 81, 'E2SUS': 89, 'E2REL': 69, 'L2RATE': 87, 'L2WAVE': 1, 'L2FADE': 86,
            'ITYPE': 1, 'IA': 80, 'IB': 127, 'IC': 112, 'TRIM': 101, 'E2FLT': 7, 'L2PIT': 1, 'E2': 34, 'E3': 62, 'E4': 100, 'E5': 14, 'E6': 110,
            'E7': 110, 'E8': 40, 'MOD1': ('AT', 'E7', 10), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO1', 'E3', 5), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 103, 'CYCLE FALL': 103,
         },
         bend={
            'ATK': 52, 'DEC': 60, 'SUS': 116, 'REL': 91, 'ED_SHP': -9, 'LRATE': 18, 'LPHASE': 5, 'LFADE': 4, 'LD_FLT': -9, 'LD_SHP': 10, 'LD_AMP': 9,
            'KTRK': 50, 'DIST': 7, 'CHOR': 0, 'DLY': 21, 'REV': 36, 'DETUNE': 34, 'FREEA': 65, 'FREEB': 4, 'FREEC': 4, 'M1AMT': 22, 'M2AMT': 6,
            'M3AMT': 3, 'E2ATK': 62, 'E2DEC': 68, 'E2SUS': 110, 'E2REL': 66, 'L2RATE': 105, 'L2PHASE': 11, 'L2FADE': 105, 'IA': 88, 'IB': 124,
            'IC': 121, 'IMIX': 113, 'E2FLT': 26, 'E2': 52, 'E3': 70, 'E4': 101, 'E5': 20, 'E6': 89, 'E7': 106, 'E8': 49, 'SUB LEVEL': 1,
            'CYCLE RISE': 105, 'CYCLE FALL': 85, 'CYCLE CURVE': 16, 'AMPENV SATN': 16, 'FILTERENV MRES': 16, 'CYCLE PAN': -1, 'LFO2 MMODE': 6,
         }),
    dict(engine='NOISE', idx=1, name='HORNET', cat='LEAD', bseed=33927, bamt=21,  # #38
         p={
            'ATK': 15, 'SUS': 127, 'REL': 59, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 5, 'KTRK': 63, 'DLY': 25, 'REV': 15, 'VOICE': 2, 'GLIDE': 22,
            'GLMODE': 1, 'FREEA': 61, 'FREEB': 31, 'FREEC': 11, 'E2ATK': 0, 'E2DEC': 79, 'E2SUS': 64, 'L2RATE': 87, 'L2WAVE': 1, 'L2FADE': 79,
            'TRIM': 96, 'E2FLT': 11, 'L2PIT': 1, 'E1': 2, 'E2': 0, 'E6': 112, 'E8': 84, 'MOD1': ('AT', 'FRES', 21), 'MOD2': ('CYCLE', 'CUT', 7),
            'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 97, 'CYCLE FALL': 97,
         },
         bend={
            'ATK': 27, 'DEC': 73, 'SUS': 122, 'REL': 66, 'ED_SHP': -12, 'LRATE': 2, 'LPHASE': 3, 'LFADE': 24, 'LD_FLT': 22, 'LD_SHP': -22,
            'LD_AMP': 3, 'KTRK': 38, 'DIST': 26, 'CHOR': 24, 'DLY': 50, 'REV': 37, 'DETUNE': 31, 'FREEA': 70, 'FREEB': 51, 'FREEC': 3, 'M1AMT': 43,
            'M2AMT': -13, 'E2ATK': 23, 'E2DEC': 61, 'E2SUS': 65, 'E2REL': 57, 'L2RATE': 89, 'L2PHASE': 21, 'L2FADE': 80, 'E2FLT': -1, 'E2': 14,
            'E7': 8, 'SUB LEVEL': 1, 'CYCLE RISE': 77, 'CYCLE FALL': 73, 'CYCLE CURVE': -19, 'AMPENV SATN': -12, 'FILTERENV MRES': 20,
            'CYCLE PAN': 25, 'LFO2 MMODE': -1,
         }),
    dict(engine='WAVE', idx=13, name='HOOVER', cat='LEAD', bseed=14898,  # #39
         p={
            'ATK': 22, 'DEC': 86, 'SUS': 127, 'REL': 73, 'LRATE': 32, 'LWAVE': 1, 'LD_FLT': 4, 'DIST': 13, 'CHOR': 89, 'REV': 25, 'TWTYPE': 3,
            'FREEA': 62, 'FREEB': 61, 'E2ATK': 0, 'E2DEC': 69, 'E2SUS': 0, 'E2REL': 56, 'L2RATE': 87, 'L2WAVE': 1, 'LPOL': 1, 'BLEND': -17, 'CUT2': 57,
            'TRIM': 112, 'E2FLT': 7, 'L2SHP': 16, 'E1': 19, 'E2': 50, 'MOD1': ('AT', 'CUT', 12), 'MOD2': ('ENV2', 'PITCH', 5),
            'MOD3': ('LFO1', 'E6', 11), 'MOD4': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN ROUTE': 1, 'TWIN LINK': 0, 'CYCLE RISE': 97,
            'CYCLE FALL': 97,
         },
         bend={
            'ATK': 43, 'DEC': 97, 'SUS': 110, 'REL': 71, 'ED_SHP': -3, 'LRATE': 45, 'LPHASE': 10, 'LFADE': 15, 'LD_FLT': 16, 'LD_SHP': -8,
            'LD_AMP': 2, 'RES2': 11, 'DRV2': 8, 'KTRK': 16, 'DIST': 21, 'CHOR': 105, 'DLY': 5, 'REV': 24, 'DETUNE': 38, 'FREEA': 74, 'FREEC': 3,
            'M1AMT': 32, 'M3AMT': 3, 'M4AMT': 10, 'M5AMT': 8, 'E2ATK': 10, 'E2DEC': 49, 'E2SUS': 18, 'E2REL': 60, 'L2RATE': 84, 'L2PHASE': 15,
            'L2FADE': 14, 'BLEND': -4, 'CUT2': 34, 'E2FLT': -2, 'E2': 66, 'E3': 16, 'E4': 19, 'E6': 5, 'TWIN KTRK2': 17, 'TWIN ENV2': 11,
            'TWIN LFO1': 14, 'TWIN LFO2': -18, 'SUB LEVEL': 10, 'CYCLE RISE': 90, 'CYCLE FALL': 92, 'CYCLE CURVE': -16, 'AMPENV SATN': 20,
            'FILTERENV FBLND': -14, 'FILTERENV FDRV': -1, 'FILTERENV MRES': -3, 'CYCLE TCUT': -19, 'CYCLE PAN': 19, 'LFO2 TRES': 13,
            'LFO2 MMODE': -18, 'LFO2 FBLND': -9,
         }),
    dict(engine='WAVE', idx=19, name='LICORICE', cat='LEAD', bseed=46772,  # #40
         p={
            'ATK': 50, 'SUS': 127, 'REL': 65, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 4, 'DLY': 14, 'REV': 34, 'VOICE': 2, 'GLIDE': 20, 'GLMODE': 1,
            'FREEA': 61, 'FREEB': 27, 'E2ATK': 0, 'E2DEC': 65, 'E2SUS': 0, 'E2REL': 59, 'TRIM': 101, 'E2FLT': 11, 'E1': 3, 'E2': 37,
            'MOD1': ('AT', 'SHP', 13), 'MOD2': ('CYCLE', 'SHP', 11), 'MOD3': ('ENV2', 'E2', 13), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 103,
            'CYCLE FALL': 103,
         },
         bend={
            'ATK': 30, 'DEC': 55, 'SUS': 124, 'REL': 75, 'ED_SHP': 14, 'LRATE': 22, 'LPHASE': 12, 'LFADE': 12, 'LD_FLT': 17, 'LD_SHP': -12,
            'LD_AMP': 5, 'KTRK': 17, 'DIST': 20, 'CHOR': 20, 'DLY': 31, 'REV': 25, 'DETUNE': 35, 'FREEA': 52, 'FREEB': 24, 'FREEC': 3, 'M1AMT': -2,
            'M2AMT': 3, 'M3AMT': 6, 'E2ATK': 6, 'E2DEC': 72, 'E2SUS': 19, 'E2REL': 77, 'L2RATE': 52, 'L2PHASE': 19, 'L2FADE': 12, 'E2FLT': -2,
            'E2': 22, 'E3': 16, 'E4': 9, 'E6': 10, 'SUB LEVEL': 2, 'CYCLE RISE': 86, 'CYCLE FALL': 90, 'CYCLE CURVE': 18, 'AMPENV SATN': -10,
            'FILTERENV MRES': 18, 'CYCLE PAN': -15, 'LFO2 MMODE': -8,
         }),
    dict(engine='WAVE', idx=18, name='PURPLE FLUTE', cat='LEAD', bseed=63290,  # #41
         p={
            'ATK': 59, 'SUS': 127, 'REL': 71, 'LRATE': 17, 'LWAVE': 1, 'LD_SHP': 7, 'DLY': 27, 'REV': 39, 'VOICE': 2, 'GLIDE': 21, 'GLMODE': 1,
            'FREEA': 56, 'FREEB': 31, 'E2ATK': 0, 'E2DEC': 71, 'E2SUS': 0, 'E2REL': 59, 'L2RATE': 88, 'L2FADE': 86, 'TRIM': 101, 'E2FLT': 7,
            'L2FLT': 5, 'E1': 25, 'E2': 10, 'MOD1': ('AT', 'CUT', 11), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('ENV2', 'E2', 17), 'LTRIG': 1,
            'L2TRIG': 1, 'CYCLE RISE': 103, 'CYCLE FALL': 103,
         },
         bend={
            'ATK': 78, 'DEC': 62, 'SUS': 111, 'REL': 62, 'ED_SHP': -6, 'LRATE': 32, 'LPHASE': 4, 'LFADE': 16, 'LD_FLT': -14, 'LD_SHP': 20,
            'LD_AMP': 8, 'KTRK': 24, 'DIST': 1, 'CHOR': 14, 'DLY': 13, 'REV': 50, 'DETUNE': 32, 'FREEA': 49, 'FREEB': 52, 'FREEC': 1, 'M1AMT': 27,
            'M2AMT': 22, 'M3AMT': -1, 'M4AMT': 6, 'E2ATK': 11, 'E2DEC': 83, 'E2SUS': 8, 'E2REL': 60, 'L2RATE': 74, 'L2PHASE': 15, 'L2FADE': 91,
            'E2FLT': 16, 'E2': 3, 'E3': 15, 'E4': 13, 'E6': 2, 'SUB LEVEL': 2, 'CYCLE RISE': 101, 'CYCLE FALL': 86, 'CYCLE CURVE': -20,
            'AMPENV SATN': 18, 'FILTERENV MRES': -18, 'CYCLE PAN': 5, 'LFO2 MMODE': 16,
         }),
    dict(engine='WAVE', idx=22, name='SCREAMER', cat='LEAD', bseed=53917,  # #42
         p={
            'ATK': 15, 'SUS': 127, 'REL': 68, 'LRATE': 40, 'LWAVE': 1, 'LD_FLT': 7, 'RES2': 21, 'DRV2': 77, 'DIST': 33, 'DLY': 20, 'REV': 18,
            'VOICE': 2, 'GLIDE': 30, 'GLMODE': 1, 'TWTYPE': 1, 'FREEA': 58, 'FREEB': 51, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 83, 'E2SUS': 51, 'E2REL': 69,
            'L2RATE': 87, 'L2WAVE': 1, 'L2FADE': 83, 'CUT2': 68, 'TRIM': 111, 'E2FLT': 17, 'L2PIT': 1, 'E1': 22, 'E2': 42, 'E6': 10,
            'MOD1': ('AT', 'SAT', 17), 'MOD2': ('CYCLE', 'SHP', 11), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'CYCLE RISE': 91, 'CYCLE FALL': 91,
         },
         bend={
            'ATK': 26, 'DEC': 60, 'SUS': 109, 'REL': 69, 'ED_SHP': -13, 'LRATE': 28, 'LPHASE': 9, 'LFADE': 6, 'LD_FLT': -6, 'LD_SHP': -11,
            'LD_AMP': 5, 'RES2': 12, 'DRV2': 85, 'KTRK': 52, 'DIST': 53, 'CHOR': 17, 'DLY': 16, 'REV': 24, 'DETUNE': 58, 'FREEA': 71, 'FREEB': 60,
            'FREEC': 14, 'M1AMT': -3, 'M2AMT': 2, 'E2ATK': 18, 'E2DEC': 85, 'E2SUS': 61, 'E2REL': 81, 'L2RATE': 79, 'L2PHASE': 8, 'L2FADE': 79,
            'BLEND': 11, 'CUT2': 50, 'E2FLT': 23, 'E2': 34, 'E3': 21, 'E4': 4, 'E6': 3, 'TWIN KTRK2': 19, 'TWIN ENV2': -18, 'TWIN LFO1': -11,
            'TWIN LFO2': -10, 'SUB LEVEL': 16, 'CYCLE RISE': 85, 'CYCLE FALL': 80, 'CYCLE CURVE': -12, 'AMPENV SATN': 16, 'FILTERENV FBLND': 14,
            'FILTERENV FDRV': -3, 'FILTERENV MRES': 19, 'CYCLE TCUT': -1, 'CYCLE PAN': -16, 'LFO2 TRES': -12, 'LFO2 MMODE': -11, 'LFO2 FBLND': 12,
         }),
    dict(engine='WAVE', idx=7, name='SYNC LEAD', cat='LEAD', bseed=15854,  # #43
         p={
            'ATK': 0, 'DEC': 79, 'SUS': 127, 'REL': 69, 'LRATE': 50, 'LWAVE': 1, 'LD_FLT': 7, 'DIST': 13, 'DLY': 32, 'REV': 19, 'VOICE': 2,
            'GLIDE': 26, 'GLMODE': 1, 'FREEA': 61, 'FREEB': 31, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 86, 'E2SUS': 0, 'E2REL': 73, 'L2RATE': 87,
            'L2WAVE': 1, 'L2FADE': 86, 'L2PIT': 1, 'E4': 30, 'MOD1': ('AT', 'E4', 17), 'MOD2': ('ENV2', 'E4', 26), 'MOD3': ('ENV2', 'E6', 11),
            'MOD4': ('CYCLE', 'SHP', 17), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 91, 'CYCLE FALL': 91,
         },
         bend={
            'ATK': 17, 'DEC': 63, 'SUS': 124, 'REL': 65, 'ED_SHP': 4, 'LRATE': 54, 'LPHASE': 5, 'LFADE': 15, 'LD_FLT': 24, 'LD_SHP': -9, 'LD_AMP': 5,
            'KTRK': 35, 'DIST': 15, 'CHOR': 21, 'DLY': 41, 'REV': 32, 'DETUNE': 25, 'FREEA': 70, 'FREEB': 17, 'FREEC': 11, 'M1AMT': 36, 'M2AMT': 27,
            'M3AMT': 15, 'M4AMT': 21, 'E2ATK': 2, 'E2DEC': 83, 'E2SUS': 13, 'E2REL': 79, 'L2RATE': 100, 'L2PHASE': 21, 'L2FADE': 90, 'E2FLT': -16,
            'E2': 6, 'E3': 1, 'E4': 39, 'E6': 6, 'SUB LEVEL': 17, 'CYCLE RISE': 105, 'CYCLE FALL': 109, 'CYCLE CURVE': 12, 'AMPENV SATN': 8,
            'FILTERENV MRES': 2, 'CYCLE PAN': -20, 'LFO2 MMODE': 11,
         }),
    dict(engine='ANALOG', idx=10, name='EMBER GLOW', cat='PAD', bseed=44167,  # #44
         p={
            'ATK': 86, 'SUS': 127, 'REL': 105, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 7, 'RES2': 61, 'KTRK': 27, 'CHOR': 32, 'REV': 44, 'TWTYPE': 1,
            'FREEA': 47, 'FREEB': 17, 'FREED': 2, 'E2ATK': 92, 'E2DEC': 105, 'E2SUS': 70, 'E2REL': 101, 'L2RATE': 15, 'ITYPE': 1, 'IA': 80, 'IB': 127,
            'IC': 112, 'CUT2': 51, 'TRIM': 80, 'E2FLT': 17, 'E2': 9, 'MOD1': ('AT', 'CUT2', 11), 'MOD2': ('CYCLE', 'CUT2', 9),
            'MOD3': ('LFO2', 'CUT', 3), 'MOD4': ('LFO2', 'E2', 3), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO2': -11, 'CYCLE RISE': 115,
            'CYCLE FALL': 115,
         },
         bend={
            'ATK': 72, 'DEC': 68, 'SUS': 107, 'REL': 120, 'ED_SHP': 1, 'LRATE': 11, 'LPHASE': 7, 'LFADE': 6, 'LD_FLT': 26, 'LD_SHP': 1, 'LD_AMP': 12,
            'RES2': 52, 'DRV2': 31, 'KTRK': 43, 'DIST': 16, 'CHOR': 1, 'DLY': 17, 'REV': 76, 'DETUNE': 52, 'FREEA': 46, 'FREEB': 34, 'FREEC': 10,
            'M1AMT': -18, 'M2AMT': 15, 'M3AMT': -8, 'M4AMT': 17, 'E2ATK': 116, 'E2DEC': 109, 'E2SUS': 38, 'E2REL': 114, 'L2RATE': 2, 'L2PHASE': 4,
            'L2FADE': 23, 'IA': 77, 'IB': 97, 'IC': 80, 'IMIX': 117, 'BLEND': 13, 'CUT2': 45, 'E2FLT': 7, 'E2': 3, 'E3': 48, 'E4': 2, 'E7': 21,
            'TWIN KTRK2': -4, 'TWIN ENV2': 4, 'TWIN LFO1': -30, 'TWIN LFO2': 9, 'SUB LEVEL': 13, 'CYCLE RISE': 119, 'CYCLE FALL': 111,
            'CYCLE CURVE': -3, 'AMPENV SATN': 10, 'FILTERENV FBLND': -8, 'FILTERENV FDRV': -27, 'FILTERENV MRES': -5, 'CYCLE TCUT': -19,
            'CYCLE PAN': -12, 'LFO2 TRES': 19, 'LFO2 MMODE': 21, 'LFO2 FBLND': -15,
         }),
    dict(engine='ANALOG', idx=9, name='SILK CHORUS', cat='PAD', bseed=33289,  # #45
         p={
            'ATK': 0, 'DEC': 127, 'SUS': 102, 'REL': 101, 'LRATE': 27, 'LWAVE': 1, 'LD_FLT': 5, 'KTRK': 17, 'CHOR': 102, 'REV': 25, 'FREEA': 51,
            'FREEB': 11, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 127, 'E2SUS': 89, 'E2REL': 101, 'L2RATE': 57, 'L2WAVE': 1, 'TRIM': 88, 'E2FLT': 7,
            'L2SHP': 38, 'E1': 4, 'E2': 0, 'E3': 0, 'MOD1': ('AT', 'CHO', 31), 'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 110,
            'CYCLE FALL': 110,
         },
         bend={
            'ATK': 26, 'DEC': 103, 'SUS': 101, 'REL': 113, 'ED_SHP': 18, 'LRATE': 52, 'LPHASE': 5, 'LFADE': 33, 'LD_FLT': 26, 'LD_SHP': 24,
            'LD_AMP': 8, 'KTRK': -12, 'DIST': 25, 'CHOR': 101, 'DLY': 22, 'REV': 18, 'DETUNE': 20, 'FREEA': 67, 'FREEB': 18, 'FREEC': 15,
            'M1AMT': 33, 'M2AMT': -7, 'M3AMT': 21, 'E2ATK': 32, 'E2DEC': 113, 'E2SUS': 85, 'E2REL': 103, 'L2RATE': 33, 'L2PHASE': 7, 'L2FADE': 3,
            'E2FLT': 34, 'E2': 9, 'E3': 14, 'E4': 5, 'E7': 29, 'SUB LEVEL': 15, 'CYCLE RISE': 78, 'CYCLE FALL': 87, 'CYCLE CURVE': 14,
            'AMPENV SATN': 30, 'FILTERENV MRES': -27, 'CYCLE PAN': -24, 'LFO2 MMODE': 27,
         }),
    dict(engine='ANALOG', idx=11, name='VELVET BOW', cat='PAD', bseed=34517,  # #46
         p={
            'ATK': 76, 'SUS': 127, 'REL': 88, 'LRATE': 27, 'KTRK': 17, 'CHOR': 108, 'REV': 38, 'FREEA': 43, 'FREEB': 11, 'E2ATK': 90, 'E2DEC': 101,
            'E2SUS': 108, 'E2REL': 88, 'L2RATE': 65, 'L2WAVE': 1, 'TRIM': 89, 'E2FLT': 17, 'L2SHP': 26, 'E1': 4, 'E2': 8, 'MOD1': ('AT', 'FRES', 17),
            'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO1', 'CUT', 4), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 110, 'CYCLE FALL': 110,
         },
         bend={
            'ATK': 43, 'DEC': 69, 'SUS': 123, 'REL': 82, 'ED_SHP': 4, 'LRATE': 22, 'LPHASE': 1, 'LFADE': 25, 'LD_FLT': -29, 'LD_SHP': -30,
            'LD_AMP': 3, 'KTRK': 37, 'DIST': 34, 'CHOR': 96, 'DLY': 14, 'REV': 32, 'DETUNE': 69, 'FREEA': 45, 'FREEB': 23, 'FREEC': 10, 'M1AMT': -13,
            'M2AMT': 39, 'M3AMT': -3, 'M4AMT': 4, 'E2ATK': 65, 'E2DEC': 112, 'E2SUS': 92, 'E2REL': 108, 'L2RATE': 40, 'L2PHASE': 6, 'L2FADE': 31,
            'E2FLT': 49, 'E2': 7, 'E3': 62, 'E4': 3, 'E7': 22, 'SUB LEVEL': 17, 'CYCLE RISE': 84, 'CYCLE FALL': 82, 'CYCLE CURVE': 25,
            'AMPENV SATN': -34, 'FILTERENV MRES': -33, 'CYCLE PAN': -19, 'LFO2 MMODE': 25,
         }),
    dict(engine='FM6', idx=2, name='GLASS HALL', cat='PAD', bseed=28240,  # #47
         p={
            'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 5, 'CHOR': 70, 'REV': 70, 'FREEA': 64, 'FREEB': 11, 'E2ATK': 101, 'E2DEC': 108, 'E2SUS': 70,
            'E2REL': 101, 'L2RATE': 34, 'L2WAVE': 1, 'CHRD': 6, 'VOIC': 1, 'ITYPE': 1, 'IA': 100, 'IB': 127, 'IC': 112, 'TRIM': 84, 'E2FLT': 11,
            'E2': 1, 'E3': -8, 'E4': 2, 'E5': 14, 'E7': 38, 'E8': 4, 'MOD1': ('AT', 'E3', 7), 'MOD2': ('CYCLE', 'E3', 5), 'MOD3': ('LFO2', 'E3', 3),
            'MOD4': ('ENV2', 'E3', 5), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 115, 'CYCLE FALL': 115,
         },
         bend={
            'ATK': 55, 'DEC': 77, 'SUS': 92, 'REL': 95, 'ED_SHP': -27, 'LRATE': 49, 'LPHASE': 32, 'LFADE': 7, 'LD_FLT': 12, 'LD_SHP': -4,
            'LD_AMP': 3, 'KTRK': 22, 'DIST': 11, 'CHOR': 95, 'DLY': 11, 'REV': 46, 'DETUNE': 25, 'FREEA': 75, 'FREEB': 23, 'FREEC': 6, 'M1AMT': -19,
            'M2AMT': 28, 'M3AMT': -5, 'M4AMT': 22, 'E2ATK': 71, 'E2DEC': 93, 'E2SUS': 82, 'E2REL': 91, 'L2RATE': 9, 'L2PHASE': 26, 'L2FADE': 2,
            'IA': 104, 'IB': 118, 'IC': 115, 'IMIX': 93, 'E2FLT': 14, 'E2': 4, 'E3': 3, 'E5': 21, 'E6': -2, 'E7': 14, 'SUB LEVEL': 20,
            'CYCLE RISE': 97, 'CYCLE FALL': 94, 'CYCLE CURVE': 17, 'AMPENV SATN': 23, 'FILTERENV MRES': 24, 'CYCLE PAN': 22, 'LFO2 MMODE': 4,
         },
         bfm=[
            43, 25, 62, 41, 92, 93, 97, 4, 54, 57, 36, 38, 45, 98, 94, 91, 0, 78, 55, 42, 60, 26, 79, 84, 93, 8, 61, 63, 27, 32, 42, 98, 96, 89, 0,
            90, 28, 31, 14, 33, 76, 83, 87, 2, 72, 60, 26, 35, 48, 98, 96, 91, 0, 90, 99, 99, 99, 99, 50, 50, 50, 7, 74, 4, 11,
         ]),
    dict(engine='PHASE', idx=2, name='LAMPLIGHT', cat='PAD', bseed=62877,  # #48
         p={
            'ATK': 92, 'DEC': 103, 'SUS': 117, 'REL': 102, 'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 5, 'RES2': 21, 'CHOR': 70, 'REV': 42, 'TWTYPE': 1,
            'FREEA': 57, 'FREEB': 11, 'E2ATK': 97, 'E2DEC': 105, 'E2SUS': 76, 'E2REL': 98, 'L2RATE': 37, 'L2WAVE': 1, 'CHRD': 2, 'SPRD': 38,
            'ITYPE': 1, 'IA': 70, 'IB': 127, 'IC': 112, 'CUT2': -11, 'TRIM': 79, 'E2FLT': 11, 'E2': 4, 'E3': 46, 'E4': 36, 'E5': 13,
            'MOD1': ('AT', 'SHP', 7), 'MOD2': ('CYCLE', 'SHP', 6), 'MOD3': ('LFO2', 'E3', 3), 'MOD4': ('ENV2', 'E3', 5), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 105, 'CYCLE FALL': 105,
         },
         bend={
            'ATK': 80, 'DEC': 76, 'SUS': 94, 'REL': 77, 'ED_SHP': 4, 'LRATE': 35, 'LPHASE': 22, 'LFADE': 3, 'LD_FLT': 21, 'LD_SHP': 13, 'LD_AMP': 6,
            'RES2': 30, 'DRV2': 24, 'KTRK': 61, 'DIST': 21, 'CHOR': 39, 'DLY': 19, 'REV': 26, 'DETUNE': 20, 'FREEA': 50, 'FREEB': 5, 'FREEC': 18,
            'M1AMT': 32, 'M2AMT': -4, 'M3AMT': 4, 'M4AMT': -12, 'E2ATK': 63, 'E2DEC': 109, 'E2SUS': 92, 'E2REL': 96, 'L2RATE': 35, 'L2PHASE': 8,
            'L2FADE': 7, 'IA': 72, 'IB': 117, 'IC': 116, 'IMIX': 123, 'BLEND': -33, 'CUT2': -7, 'E2FLT': 37, 'E3': 57, 'E4': 15, 'E5': 4,
            'TWIN KTRK2': -15, 'TWIN ENV2': -2, 'TWIN LFO1': 15, 'TWIN LFO2': -30, 'SUB LEVEL': 34, 'CYCLE RISE': 125, 'CYCLE FALL': 82,
            'CYCLE CURVE': -7, 'AMPENV SATN': -24, 'FILTERENV FBLND': -13, 'FILTERENV FDRV': -14, 'FILTERENV MRES': -13, 'CYCLE TCUT': 12,
            'CYCLE PAN': 24, 'LFO2 TRES': 32, 'LFO2 MMODE': -18, 'LFO2 FBLND': -1,
         }),
    dict(engine='LOFI', idx=2, name='WOW FLUTTER', cat='PAD', bseed=65413,  # #49
         p={
            'ATK': 90, 'DEC': 101, 'SUS': 117, 'REL': 100, 'CHOR': 64, 'REV': 70, 'FREEA': 47, 'FREEB': 17, 'E2ATK': 97, 'E2DEC': 105, 'E2SUS': 76,
            'E2REL': 98, 'L2RATE': 34, 'L2WAVE': 1, 'CHRD': 7, 'VOIC': 1, 'SPRD': 38, 'ITYPE': 9, 'IA': 52, 'IB': 40, 'IC': 78, 'TRIM': 102,
            'E2FLT': 17, 'L2FLT': 5, 'E1': 1, 'E2': 4, 'E3': 92, 'E8': 52, 'MOD1': ('AT', 'E8', 21), 'MOD2': ('CYCLE', 'CUT', 8),
            'MOD3': ('LFO2', 'E8', 6), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 115, 'CYCLE FALL': 115,
         },
         bend={
            'ATK': 88, 'DEC': 93, 'SUS': 123, 'REL': 111, 'ED_SHP': 34, 'LRATE': 79, 'LPHASE': 19, 'LFADE': 28, 'LD_FLT': -29, 'LD_SHP': -30,
            'LD_AMP': 9, 'KTRK': 57, 'DIST': 1, 'CHOR': 98, 'DLY': 3, 'REV': 72, 'DETUNE': 58, 'FREEA': 50, 'FREEB': 11, 'FREEC': 24, 'M1AMT': 53,
            'M2AMT': 2, 'M3AMT': 36, 'M4AMT': -1, 'E2ATK': 68, 'E2DEC': 71, 'E2SUS': 106, 'E2REL': 125, 'L2RATE': 59, 'L2PHASE': 15, 'L2FADE': 29,
            'IA': 36, 'IB': 56, 'IC': 97, 'IMIX': 95, 'E2FLT': -5, 'E3': 68, 'E4': 21, 'E5': 11, 'E6': 28, 'E8': 86, 'SUB LEVEL': 29,
            'CYCLE RISE': 121, 'CYCLE FALL': 81, 'CYCLE CURVE': 11, 'AMPENV SATN': -27, 'FILTERENV MRES': -32, 'CYCLE PAN': -33, 'LFO2 MMODE': 11,
         }),
    dict(engine='VOICE', idx=6, name='PLATE CHOIR', cat='PAD', bseed=64679,  # #50
         p={
            'ATK': 94, 'DEC': 101, 'SUS': 117, 'REL': 101, 'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 4, 'CHOR': 38, 'REV': 114, 'FREEA': 61, 'FREEB': 7,
            'E2ATK': 97, 'E2DEC': 105, 'E2SUS': 89, 'E2REL': 101, 'L2RATE': 27, 'L2WAVE': 1, 'CHRD': 1, 'VOIC': 1, 'SPRD': 51, 'TRIM': 95, 'E2FLT': 11,
            'L2SHP': 8, 'E1': 10, 'E2': 60, 'E3': 40, 'E5': 38, 'E6': 14, 'E7': 52, 'E8': 6, 'MOD1': ('AT', 'E4', 4), 'MOD2': ('CYCLE', 'E2', 6),
            'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 110, 'CYCLE FALL': 110,
         },
         bend={
            'ATK': 74, 'DEC': 107, 'SUS': 103, 'REL': 78, 'ED_SHP': 24, 'LRATE': 41, 'LPHASE': 12, 'LFADE': 28, 'LD_FLT': 10, 'LD_SHP': 11,
            'LD_AMP': 8, 'KTRK': 34, 'DIST': 30, 'CHOR': 16, 'DLY': 8, 'REV': 87, 'DETUNE': 50, 'FREEA': 64, 'FREEB': 34, 'FREEC': 1, 'M1AMT': 8,
            'M2AMT': -23, 'M3AMT': 30, 'E2ATK': 72, 'E2DEC': 111, 'E2SUS': 81, 'E2REL': 74, 'L2RATE': 36, 'L2PHASE': 15, 'L2FADE': 3, 'E2FLT': -4,
            'E1': 3, 'E2': 54, 'E3': 34, 'E4': -3, 'E5': 55, 'E6': 46, 'E7': 34, 'E8': 31, 'SUB LEVEL': 33, 'CYCLE RISE': 98, 'CYCLE FALL': 124,
            'CYCLE CURVE': 23, 'AMPENV SATN': 18, 'FILTERENV MRES': 16, 'CYCLE PAN': 30, 'LFO2 MMODE': 23,
         }),
    dict(engine='VOICE', idx=1, name='VESPERS', cat='PAD', bseed=34110,  # #51
         p={
            'ATK': 90, 'DEC': 101, 'SUS': 117, 'REL': 100, 'LRATE': 18, 'LWAVE': 1, 'LD_FLT': 4, 'CHOR': 57, 'REV': 89, 'FREEA': 57, 'FREEB': 11,
            'E2ATK': 100, 'E2DEC': 105, 'E2SUS': 76, 'E2REL': 95, 'L2RATE': 87, 'L2WAVE': 1, 'L2FADE': 94, 'CHRD': 1, 'VOIC': 1, 'SPRD': 51,
            'TRIM': 86, 'E2FLT': 17, 'L2PIT': 1, 'E2': 95, 'E3': 20, 'E5': 42, 'E6': 21, 'E7': 58, 'E8': 14, 'MOD1': ('AT', 'E6', 24),
            'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO1', 'E2', 6), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 107, 'CYCLE FALL': 112,
         },
         bend={
            'ATK': 79, 'DEC': 122, 'SUS': 105, 'REL': 111, 'ED_SHP': 31, 'LRATE': 29, 'LPHASE': 22, 'LFADE': 25, 'LD_FLT': 33, 'LD_SHP': 25,
            'LD_AMP': 2, 'KTRK': 9, 'DIST': 33, 'CHOR': 35, 'DLY': 15, 'REV': 105, 'DETUNE': 66, 'FREEA': 56, 'FREEB': 43, 'FREEC': 14, 'M1AMT': 42,
            'M2AMT': -1, 'M3AMT': 40, 'E2ATK': 79, 'E2DEC': 76, 'E2SUS': 91, 'E2REL': 92, 'L2RATE': 98, 'L2PHASE': 27, 'L2FADE': 67, 'E2FLT': -3,
            'E1': 9, 'E2': 114, 'E3': 17, 'E4': 1, 'E5': 56, 'E6': 49, 'E7': 65, 'E8': 33, 'SUB LEVEL': 19, 'CYCLE RISE': 98, 'CYCLE FALL': 88,
            'CYCLE CURVE': -26, 'AMPENV SATN': -8, 'FILTERENV MRES': -1, 'CYCLE PAN': 6, 'LFO2 MMODE': -27,
         }),
    dict(engine='VOICE', idx=5, name='WHISPER PAD', cat='PAD', bseed=40791,  # #52
         p={
            'ATK': 92, 'DEC': 101, 'SUS': 110, 'REL': 101, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 5, 'CHOR': 52, 'DLY': 22, 'REV': 90, 'FREEA': 61,
            'FREEB': 17, 'E2ATK': 98, 'E2DEC': 105, 'E2SUS': 89, 'E2REL': 101, 'L2RATE': 15, 'SPRD': 47, 'TRIM': 99, 'E2FLT': 11, 'E2': 95, 'E3': 88,
            'E4': 3, 'E5': 50, 'E6': 120, 'E7': 50, 'E8': 11, 'MOD1': ('AT', 'SHP', 16), 'MOD2': ('CYCLE', 'E6', -8), 'MOD3': ('LFO2', 'E2', 4),
            'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 105, 'CYCLE FALL': 109,
         },
         bend={
            'ATK': 71, 'DEC': 104, 'SUS': 114, 'REL': 73, 'ED_SHP': 19, 'LRATE': 15, 'LPHASE': 34, 'LFADE': 30, 'LD_FLT': -17, 'LD_SHP': -18,
            'LD_AMP': 1, 'KTRK': 26, 'DIST': 32, 'CHOR': 60, 'DLY': 32, 'REV': 60, 'DETUNE': 62, 'FREEA': 50, 'FREEB': 25, 'FREEC': 22, 'M1AMT': 20,
            'M2AMT': 23, 'M3AMT': 30, 'E2ATK': 92, 'E2DEC': 109, 'E2SUS': 115, 'E2REL': 90, 'L2RATE': 20, 'L2PHASE': 3, 'L2FADE': 26, 'E2FLT': 24,
            'E1': 21, 'E2': 106, 'E3': 82, 'E4': 2, 'E5': 80, 'E6': 105, 'E7': 59, 'E8': 19, 'SUB LEVEL': 15, 'CYCLE RISE': 85, 'CYCLE FALL': 94,
            'CYCLE CURVE': 7, 'AMPENV SATN': -9, 'FILTERENV MRES': -26, 'CYCLE PAN': -23, 'LFO2 MMODE': 3,
         }),
    dict(engine='TRIO', idx=2, name='CHIP CHAPEL', cat='PAD', bseed=18034,  # #53
         p={
            'ATK': 94, 'DEC': 101, 'SUS': 117, 'REL': 101, 'LRATE': 35, 'LWAVE': 1, 'LD_FLT': 4, 'RES2': 85, 'KTRK': 0, 'CHOR': 57, 'REV': 70,
            'TWTYPE': 2, 'FREEA': 57, 'FREEB': 11, 'E2ATK': 98, 'E2DEC': 105, 'E2SUS': 76, 'E2REL': 98, 'L2RATE': 50, 'L2WAVE': 1, 'CHRD': 1,
            'VOIC': 1, 'SPRD': 44, 'CUT2': 43, 'TRIM': 96, 'E2FLT': 11, 'L2SHP': 29, 'E1': 1, 'E3': 12, 'E4': 7, 'E8': 40, 'MOD1': ('AT', 'CUT2', 12),
            'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO1', 'CUT2', 2), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 8, 'CYCLE RISE': 100,
            'CYCLE FALL': 105,
         },
         bend={
            'ATK': 88, 'DEC': 123, 'SUS': 101, 'REL': 122, 'ED_SHP': -8, 'LRATE': 44, 'LPHASE': 16, 'LFADE': 19, 'LD_FLT': 0, 'LD_SHP': -9,
            'LD_AMP': 6, 'RES2': 55, 'DRV2': 8, 'KTRK': -26, 'DIST': 28, 'CHOR': 56, 'DLY': 15, 'REV': 42, 'DETUNE': 26, 'FREEA': 50, 'FREEB': 40,
            'FREEC': 19, 'M1AMT': 21, 'M2AMT': -19, 'M3AMT': 19, 'M4AMT': 60, 'E2ATK': 120, 'E2DEC': 81, 'E2SUS': 104, 'E2REL': 101, 'L2RATE': 51,
            'L2PHASE': 32, 'L2FADE': 3, 'BLEND': 18, 'CUT2': 50, 'E2FLT': -14, 'E4': 2, 'E8': 43, 'TWIN KTRK2': -2, 'TWIN ENV2': 18, 'TWIN LFO1': 6,
            'TWIN LFO2': 26, 'SUB LEVEL': 21, 'CYCLE RISE': 110, 'CYCLE FALL': 90, 'CYCLE CURVE': 26, 'AMPENV SATN': -3, 'FILTERENV FBLND': 28,
            'FILTERENV FDRV': 32, 'FILTERENV MRES': -8, 'CYCLE TCUT': -18, 'CYCLE PAN': 9, 'LFO2 TRES': 27, 'LFO2 MMODE': -24, 'LFO2 FBLND': 15,
         }),
    dict(engine='TRIO', idx=9, name='STRING MACH', cat='PAD', bseed=52771,  # #54
         p={
            'ATK': 77, 'DEC': 101, 'SUS': 127, 'REL': 97, 'LRATE': 40, 'LWAVE': 1, 'LD_SHP': 12, 'KTRK': 0, 'CHOR': 90, 'REV': 52, 'TWTYPE': 3,
            'FREEA': 61, 'FREEB': 7, 'E2ATK': 85, 'E2DEC': 101, 'E2SUS': 89, 'E2REL': 94, 'L2RATE': 23, 'SPRD': 34, 'CUT2': 11, 'TRIM': 90, 'E2FLT': 7,
            'E1': 1, 'E2': 12, 'E3': 0, 'E4': 7, 'E8': 17, 'MOD1': ('AT', 'E4', 16), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO2', 'CUT2', 2),
            'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'CYCLE RISE': 107, 'CYCLE FALL': 108,
         },
         bend={
            'ATK': 78, 'DEC': 85, 'SUS': 125, 'REL': 113, 'ED_SHP': 14, 'LRATE': 16, 'LPHASE': 23, 'LFADE': 16, 'LD_FLT': -9, 'LD_SHP': 43,
            'LD_AMP': 4, 'RES2': 30, 'DRV2': 4, 'KTRK': -27, 'DIST': 14, 'CHOR': 92, 'DLY': 6, 'REV': 66, 'DETUNE': 13, 'FREEA': 50, 'FREEB': 3,
            'FREEC': 21, 'M1AMT': -7, 'M2AMT': -11, 'M3AMT': -3, 'E2ATK': 91, 'E2DEC': 85, 'E2SUS': 98, 'E2REL': 121, 'L2RATE': 1, 'L2PHASE': 30,
            'L2FADE': 32, 'BLEND': -15, 'CUT2': 0, 'E2FLT': -10, 'E4': 11, 'E8': 46, 'TWIN KTRK2': 11, 'TWIN ENV2': -29, 'TWIN LFO1': -6,
            'TWIN LFO2': -25, 'SUB LEVEL': 32, 'CYCLE RISE': 115, 'CYCLE FALL': 85, 'CYCLE CURVE': 4, 'AMPENV SATN': 25, 'FILTERENV FBLND': -10,
            'FILTERENV FDRV': -9, 'FILTERENV MRES': -18, 'CYCLE TCUT': -11, 'CYCLE PAN': -10, 'LFO2 TRES': -30, 'LFO2 MMODE': -2, 'LFO2 FBLND': 27,
         }),
    dict(engine='WHEEL', idx=2, name='CHOIR LOFT', cat='PAD', bseed=8609,  # #55
         p={
            'ATK': 90, 'SUS': 127, 'REL': 100, 'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 4, 'CHOR': 51, 'REV': 70, 'FREEA': 47, 'FREEB': 11, 'E2ATK': 98,
            'E2DEC': 105, 'E2SUS': 76, 'E2REL': 98, 'L2RATE': 31, 'L2WAVE': 1, 'CHRD': 1, 'TRIM': 102, 'E2FLT': 14, 'E2SHP': 16, 'L2SHP': 3, 'E1': 12,
            'E4': 2, 'E6': 0, 'MOD1': ('AT', 'SHP', 5), 'MOD2': ('CYCLE', 'CUT', 6), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 115, 'CYCLE FALL': 119,
         },
         bend={
            'ATK': 70, 'DEC': 39, 'SUS': 105, 'REL': 78, 'ED_SHP': 33, 'LRATE': 16, 'LPHASE': 14, 'LFADE': 19, 'LD_FLT': -15, 'LD_SHP': -31,
            'LD_AMP': 4, 'KTRK': 3, 'DIST': 20, 'CHOR': 62, 'DLY': 25, 'REV': 60, 'DETUNE': 36, 'FREEA': 45, 'FREEB': 42, 'FREEC': 4, 'M1AMT': -23,
            'M2AMT': -27, 'M3AMT': 26, 'M4AMT': -7, 'E2ATK': 100, 'E2DEC': 119, 'E2SUS': 61, 'E2REL': 122, 'L2RATE': 35, 'L2PHASE': 24, 'L2FADE': 29,
            'E2FLT': -16, 'E3': 1, 'E4': 4, 'E6': 21, 'E7': 19, 'SUB LEVEL': 5, 'CYCLE RISE': 98, 'CYCLE FALL': 88, 'CYCLE CURVE': 31,
            'AMPENV SATN': 20, 'FILTERENV MRES': -1, 'CYCLE PAN': 6, 'LFO2 MMODE': 20,
         }),
    dict(engine='PHYS', idx=1, name='SINGING BOWL', cat='PAD', bseed=64412,  # #56
         p={
            'ATK': 69, 'SUS': 127, 'REL': 103, 'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 4, 'CHOR': 38, 'DLY': 19, 'REV': 76, 'FREEA': 57, 'FREEB': 11,
            'E2ATK': 79, 'E2DEC': 108, 'E2SUS': 89, 'E2REL': 101, 'L2RATE': 21, 'L2WAVE': 1, 'CHRD': 9, 'VOIC': 1, 'TRIM': 127, 'E2FLT': 14,
            'L2FLT': 4, 'E2': 92, 'E3': 66, 'E4': 92, 'E5': 10, 'E6': 80, 'E7': 100, 'MOD1': ('AT', 'E7', 10), 'MOD2': ('CYCLE', 'CUT', 7),
            'MOD3': ('LFO2', 'E3', 4), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 112, 'CYCLE FALL': 112,
         },
         bend={
            'ATK': 74, 'DEC': 51, 'SUS': 102, 'REL': 116, 'ED_SHP': 6, 'LRATE': 46, 'LPHASE': 5, 'LFADE': 25, 'LD_FLT': 11, 'LD_SHP': 30,
            'LD_AMP': 7, 'KTRK': 12, 'DIST': 2, 'CHOR': 14, 'DLY': 44, 'REV': 77, 'DETUNE': 48, 'FREEA': 50, 'FREEB': 17, 'FREEC': 18, 'M1AMT': -18,
            'M2AMT': -15, 'M3AMT': 32, 'M4AMT': -22, 'E2ATK': 45, 'E2DEC': 127, 'E2SUS': 97, 'E2REL': 119, 'L2RATE': 54, 'L2PHASE': 16, 'L2FADE': 27,
            'E2FLT': -5, 'E2': 89, 'E3': 71, 'E4': 73, 'E5': 34, 'E6': 106, 'E7': 97, 'E8': 30, 'SUB LEVEL': 30, 'CYCLE RISE': 110,
            'CYCLE FALL': 120, 'CYCLE CURVE': -26, 'AMPENV SATN': -33, 'FILTERENV MRES': 12, 'CYCLE PAN': 17, 'LFO2 MMODE': -23,
         }),
    dict(engine='NOISE', idx=2, name='SEA ORGAN', cat='PAD', bseed=1234,  # #57
         p={
            'ATK': 97, 'SUS': 127, 'REL': 108, 'LRATE': 27, 'RES2': 61, 'KTRK': 63, 'CHOR': 51, 'REV': 83, 'TWTYPE': 2, 'FREEA': 47, 'FREEB': 91,
            'FREEC': 64, 'E2ATK': 101, 'E2DEC': 105, 'E2SUS': 76, 'E2REL': 98, 'L2RATE': 23, 'L2WAVE': 1, 'CHRD': 1, 'VOIC': 1, 'SPRD': 51, 'CUT2': 41,
            'TRIM': 107, 'E2FLT': 7, 'L2FLT': 4, 'E7': 47, 'MOD1': ('AT', 'FRES', 24), 'MOD2': ('CYCLE', 'CUT2', 14), 'MOD3': ('LFO1', 'E2', 2),
            'LTRIG': 1, 'L2TRIG': 1, 'TWIN ROUTE': 1, 'TWIN LINK': 0, 'TWIN KTRK2': 63, 'TWIN LFO2': 8, 'CYCLE RISE': 115, 'CYCLE FALL': 115,
         },
         bend={
            'ATK': 70, 'DEC': 74, 'SUS': 111, 'REL': 118, 'ED_SHP': 3, 'LRATE': 55, 'LPHASE': 24, 'LFADE': 18, 'LD_FLT': 32, 'LD_SHP': -19,
            'LD_AMP': 8, 'RES2': 102, 'KTRK': 52, 'DIST': 22, 'CHOR': 42, 'DLY': 14, 'REV': 91, 'DETUNE': 33, 'FREEA': 30, 'FREEB': 64, 'FREEC': 88,
            'M1AMT': 20, 'M2AMT': 9, 'M3AMT': 1, 'M4AMT': -21, 'E2ATK': 77, 'E2DEC': 97, 'E2SUS': 103, 'E2REL': 95, 'L2RATE': 9, 'L2PHASE': 25,
            'L2FADE': 13, 'BLEND': -26, 'CUT2': 50, 'E2FLT': 4, 'E2': 70, 'E6': 18, 'E7': 61, 'E8': 15, 'TWIN KTRK2': 50, 'TWIN ENV2': -15,
            'TWIN LFO1': 13, 'TWIN LFO2': -15, 'SUB LEVEL': 32, 'CYCLE RISE': 81, 'CYCLE FALL': 95, 'CYCLE CURVE': 10, 'AMPENV SATN': 31,
            'FILTERENV FBLND': 7, 'FILTERENV FDRV': 34, 'FILTERENV MRES': -17, 'CYCLE TCUT': -34, 'CYCLE PAN': -18, 'LFO2 TRES': -25,
            'LFO2 MMODE': 28, 'LFO2 FBLND': 23,
         }),
    dict(engine='WAVE', idx=20, name='BRASS PAD', cat='PAD', bseed=17892,  # #58
         p={
            'ATK': 82, 'DEC': 101, 'SUS': 127, 'REL': 98, 'LRATE': 23, 'LWAVE': 1, 'CHOR': 47, 'REV': 65, 'FREEA': 41, 'FREEB': 11, 'E2ATK': 88,
            'E2DEC': 101, 'E2SUS': 76, 'E2REL': 97, 'L2RATE': 87, 'L2FADE': 100, 'CHRD': 9, 'VOIC': 1, 'SPRD': 34, 'TRIM': 106, 'E2FLT': 24,
            'E2SHP': 24, 'L2PIT': 1, 'E1': 6, 'E2': 31, 'E6': 9, 'MOD1': ('AT', 'CUT', 16), 'MOD2': ('CYCLE', 'SHP', 16), 'MOD3': ('LFO1', 'E6', 11),
            'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 91, 'CYCLE FALL': 127,
         },
         bend={
            'ATK': 53, 'DEC': 88, 'SUS': 115, 'REL': 86, 'ED_SHP': -32, 'LRATE': 24, 'LPHASE': 16, 'LFADE': 9, 'LD_FLT': 12, 'LD_SHP': -10,
            'LD_AMP': 8, 'KTRK': 27, 'DIST': 33, 'CHOR': 29, 'DLY': 9, 'REV': 53, 'DETUNE': 71, 'FREEA': 45, 'FREEB': 10, 'FREEC': 24, 'M1AMT': 17,
            'M2AMT': -12, 'M3AMT': 32, 'M4AMT': 0, 'E2ATK': 92, 'E2DEC': 83, 'E2SUS': 51, 'E2REL': 63, 'L2RATE': 111, 'L2PHASE': 11, 'L2FADE': 89,
            'E2FLT': 14, 'E2': 64, 'E3': 21, 'E4': 19, 'E6': 12, 'SUB LEVEL': 22, 'CYCLE RISE': 119, 'CYCLE FALL': 99, 'CYCLE CURVE': 21,
            'AMPENV SATN': 7, 'FILTERENV MRES': -12, 'CYCLE PAN': -18, 'LFO2 MMODE': -18,
         }),
    dict(engine='WAVE', idx=15, name='BREATH PAD', cat='PAD', bseed=60243,  # #59
         p={
            'ATK': 79, 'DEC': 101, 'SUS': 127, 'REL': 94, 'LRATE': 23, 'LWAVE': 1, 'DIST': 7, 'CHOR': 32, 'REV': 70, 'FREEA': 51, 'FREEB': 41,
            'E2ATK': 84, 'E2DEC': 101, 'E2SUS': 89, 'E2REL': 94, 'L2RATE': 89, 'L2FADE': 88, 'SPRD': 32, 'TRIM': 109, 'E2FLT': 14, 'L2FLT': 1,
            'E1': 25, 'E2': 30, 'E6': 6, 'MOD1': ('AT', 'SHP', 12), 'MOD2': ('CYCLE', 'CUT', 6), 'MOD3': ('LFO1', 'E2', 24), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 105, 'CYCLE FALL': 109,
         },
         bend={
            'ATK': 56, 'DEC': 115, 'SUS': 125, 'REL': 112, 'ED_SHP': -1, 'LRATE': 55, 'LPHASE': 8, 'LFADE': 8, 'LD_FLT': 6, 'LD_SHP': -32,
            'LD_AMP': 5, 'KTRK': 20, 'DIST': 30, 'CHOR': 58, 'DLY': 22, 'REV': 81, 'DETUNE': 11, 'FREEA': 45, 'FREEB': 26, 'FREEC': 11, 'M1AMT': 29,
            'M2AMT': 18, 'M3AMT': 52, 'M4AMT': 13, 'E2ATK': 108, 'E2DEC': 126, 'E2SUS': 88, 'E2REL': 120, 'L2RATE': 99, 'L2PHASE': 15, 'L2FADE': 93,
            'E2FLT': 9, 'E2': 36, 'E3': 16, 'E4': 3, 'E6': 16, 'SUB LEVEL': 7, 'CYCLE RISE': 104, 'CYCLE FALL': 90, 'CYCLE CURVE': -27,
            'AMPENV SATN': 8, 'FILTERENV MRES': -24, 'CYCLE PAN': 18, 'LFO2 MMODE': 26,
         }),
    dict(engine='WAVE', idx=14, name='DIGITAL PAD', cat='PAD', bseed=56298,  # #60
         p={
            'ATK': 83, 'DEC': 105, 'SUS': 127, 'REL': 101, 'LRATE': 27, 'CHOR': 44, 'REV': 64, 'FREEA': 57, 'FREEB': 17, 'FREED': 2, 'E2ATK': 108,
            'E2DEC': 110, 'E2SUS': 64, 'E2REL': 105, 'L2RATE': 17, 'L2WAVE': 1, 'LPOL': 1, 'SPRD': 38, 'E2FLT': 17, 'E2SHP': 38, 'L2SHP': 6, 'E1': 14,
            'E2': 10, 'E6': 7, 'MOD1': ('AT', 'E3', 8), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO1', 'E3', 13), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 110, 'CYCLE FALL': 113,
         },
         bend={
            'ATK': 57, 'DEC': 114, 'SUS': 113, 'REL': 70, 'ED_SHP': 5, 'LRATE': 9, 'LPHASE': 11, 'LFADE': 28, 'LD_FLT': 5, 'LD_SHP': 28, 'LD_AMP': 6,
            'KTRK': 47, 'DIST': 25, 'CHOR': 77, 'DLY': 18, 'REV': 54, 'DETUNE': 39, 'FREEA': 50, 'FREEB': 32, 'FREEC': 2, 'M1AMT': 23, 'M2AMT': -14,
            'M3AMT': 9, 'M4AMT': 10, 'M5AMT': 27, 'E2ATK': 105, 'E2DEC': 77, 'E2SUS': 48, 'E2REL': 81, 'L2RATE': 21, 'L2PHASE': 10, 'L2FADE': 23,
            'E2FLT': 48, 'E2': 36, 'E3': 21, 'E4': 18, 'E6': 13, 'SUB LEVEL': 31, 'CYCLE RISE': 96, 'CYCLE FALL': 90, 'CYCLE CURVE': 31,
            'AMPENV SATN': 21, 'FILTERENV MRES': 21, 'CYCLE PAN': -3, 'LFO2 MMODE': 18,
         }),
    dict(engine='WAVE', idx=3, name='EVOLVING PAD', cat='PAD', bseed=42897,  # #61
         p={
            'ATK': 105, 'DEC': 110, 'SUS': 127, 'REL': 117, 'LRATE': 6, 'LWAVE': 1, 'LD_SHP': 19, 'DLY': 32, 'REV': 76, 'FREEA': 61, 'FREEB': 11,
            'E2ATK': 108, 'E2DEC': 110, 'E2SUS': 89, 'E2REL': 110, 'L2RATE': 23, 'SPRD': 38, 'TRIM': 111, 'E2FLT': 11, 'L2FLT': 10, 'L2SHP': 13,
            'E1': 9, 'E2': 64, 'E6': 7, 'MOD1': ('AT', 'DLY', 30), 'MOD2': ('CYCLE', 'E3', 12), 'MOD3': ('LFO1', 'SUBLV', 24), 'LTRIG': 1, 'L2TRIG': 1,
            'SUB LEVEL': 39, 'CYCLE RISE': 117, 'CYCLE FALL': 120,
         },
         bend={
            'ATK': 96, 'DEC': 82, 'SUS': 125, 'REL': 114, 'ED_SHP': -3, 'LRATE': 17, 'LPHASE': 9, 'LFADE': 11, 'LD_FLT': 10, 'LD_SHP': 2,
            'LD_AMP': 9, 'KTRK': 44, 'DIST': 23, 'CHOR': 33, 'DLY': 12, 'REV': 56, 'DETUNE': 64, 'FREEA': 71, 'FREEB': 24, 'FREEC': 6, 'M1AMT': 29,
            'M2AMT': 13, 'M3AMT': 55, 'M4AMT': 35, 'M5AMT': 16, 'E2ATK': 95, 'E2DEC': 78, 'E2SUS': 91, 'E2REL': 84, 'L2RATE': 24, 'L2PHASE': 32,
            'L2FADE': 14, 'E2FLT': 1, 'E2': 73, 'E3': 19, 'E4': 28, 'E6': 13, 'SUB LEVEL': 19, 'CYCLE RISE': 95, 'CYCLE FALL': 103,
            'CYCLE CURVE': -4, 'AMPENV SATN': -1, 'FILTERENV MRES': -7, 'CYCLE PAN': -2, 'LFO2 MMODE': -13,
         }),
    dict(engine='WAVE', idx=2, name='GLASS PAD', cat='PAD', bseed=55575,  # #62
         p={
            'ATK': 90, 'DEC': 105, 'SUS': 127, 'REL': 105, 'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 4, 'CHOR': 38, 'REV': 57, 'FREEA': 61, 'FREEB': 7,
            'E2ATK': 101, 'E2DEC': 108, 'E2SUS': 51, 'E2REL': 101, 'L2RATE': 32, 'L2POL': 1, 'SPRD': 32, 'TRIM': 103, 'E2FLT': 11, 'E2SHP': 13,
            'E1': 11, 'E2': 34, 'E6': 6, 'MOD1': ('AT', 'CHO', 30), 'MOD2': ('CYCLE', 'SHP', 8), 'MOD3': ('LFO2', 'E3', 13), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 113, 'CYCLE FALL': 117,
         },
         bend={
            'ATK': 68, 'DEC': 121, 'SUS': 120, 'REL': 77, 'ED_SHP': -22, 'LRATE': 2, 'LPHASE': 14, 'LFADE': 26, 'LD_FLT': -2, 'LD_SHP': -32,
            'LD_AMP': 11, 'KTRK': 40, 'DIST': 5, 'CHOR': 68, 'DLY': 22, 'REV': 68, 'DETUNE': 46, 'FREEA': 67, 'FREEB': 37, 'FREEC': 4, 'M1AMT': 23,
            'M2AMT': 30, 'M3AMT': 25, 'M4AMT': -5, 'E2ATK': 127, 'E2DEC': 104, 'E2SUS': 84, 'E2REL': 108, 'L2RATE': 10, 'L2PHASE': 17, 'L2FADE': 6,
            'E2FLT': 36, 'E2': 64, 'E3': 23, 'E4': 23, 'E6': 13, 'SUB LEVEL': 31, 'CYCLE RISE': 92, 'CYCLE FALL': 90, 'CYCLE CURVE': -3,
            'AMPENV SATN': 28, 'FILTERENV MRES': -3, 'CYCLE PAN': 30, 'LFO2 MMODE': -28,
         }),
    dict(engine='WAVE', idx=0, name='PWM PAD', cat='PAD', bseed=44102,  # #63
         p={
            'ATK': 79, 'DEC': 101, 'SUS': 127, 'REL': 92, 'LRATE': 23, 'LWAVE': 1, 'CHOR': 57, 'REV': 44, 'FREEA': 51, 'FREEB': 11, 'E2ATK': 94,
            'E2DEC': 105, 'E2SUS': 89, 'E2REL': 98, 'L2RATE': 47, 'L2WAVE': 1, 'LPOL': 1, 'SPRD': 38, 'TRIM': 117, 'E2FLT': 14, 'L2SHP': 19, 'E1': 1,
            'E2': 40, 'E6': 8, 'MOD1': ('AT', 'CUT', 12), 'MOD2': ('CYCLE', 'CUT', 8), 'MOD3': ('LFO1', 'E3', 17), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 113, 'CYCLE FALL': 115,
         },
         bend={
            'ATK': 80, 'DEC': 120, 'SUS': 106, 'REL': 75, 'ED_SHP': 24, 'LRATE': 41, 'LPHASE': 8, 'LFADE': 28, 'LD_FLT': -16, 'LD_SHP': -34,
            'LD_AMP': 5, 'KTRK': 53, 'DIST': 34, 'CHOR': 81, 'DLY': 22, 'REV': 77, 'DETUNE': 31, 'FREEA': 50, 'FREEB': 6, 'FREEC': 23, 'M1AMT': 34,
            'M2AMT': 14, 'M3AMT': -15, 'M4AMT': 51, 'E2ATK': 92, 'E2DEC': 121, 'E2SUS': 56, 'E2REL': 109, 'L2RATE': 31, 'L2PHASE': 28, 'L2FADE': 16,
            'E2FLT': 21, 'E2': 58, 'E3': 28, 'E4': 10, 'E6': 4, 'SUB LEVEL': 25, 'CYCLE RISE': 85, 'CYCLE FALL': 127, 'CYCLE CURVE': -4,
            'AMPENV SATN': -21, 'FILTERENV MRES': 24, 'CYCLE PAN': -29, 'LFO2 MMODE': 2,
         }),
    dict(engine='WAVE', idx=1, name='VOWEL PAD', cat='PAD', bseed=38170,  # #64
         p={
            'ATK': 88, 'DEC': 101, 'SUS': 127, 'REL': 98, 'LRATE': 18, 'LWAVE': 1, 'CHOR': 51, 'REV': 76, 'FREEA': 57, 'FREEB': 11, 'E2ATK': 97,
            'E2DEC': 105, 'E2SUS': 89, 'E2REL': 98, 'L2RATE': 21, 'L2WAVE': 1, 'LPOL': 1, 'SPRD': 38, 'TRIM': 109, 'E2FLT': 11, 'L2SHP': 8, 'E1': 5,
            'E2': 8, 'E6': 10, 'MOD1': ('AT', 'SHP', 8), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO1', 'SUBLV', 17), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 111, 'CYCLE FALL': 111,
         },
         bend={
            'ATK': 58, 'DEC': 72, 'SUS': 113, 'REL': 78, 'ED_SHP': 21, 'LRATE': 1, 'LPHASE': 26, 'LFADE': 29, 'LD_FLT': -27, 'LD_SHP': 32,
            'LD_AMP': 6, 'KTRK': 31, 'DIST': 34, 'CHOR': 75, 'DLY': 18, 'REV': 64, 'DETUNE': 28, 'FREEA': 45, 'FREEB': 33, 'FREEC': 4, 'M1AMT': 25,
            'M2AMT': 28, 'M3AMT': 38, 'M4AMT': 29, 'E2ATK': 121, 'E2DEC': 83, 'E2SUS': 115, 'E2REL': 107, 'L2RATE': 13, 'L2PHASE': 29, 'L2FADE': 15,
            'E2FLT': 23, 'E2': 16, 'E3': 17, 'E4': 4, 'E6': 4, 'SUB LEVEL': 19, 'CYCLE RISE': 121, 'CYCLE FALL': 112, 'CYCLE CURVE': -31,
            'AMPENV SATN': 1, 'FILTERENV MRES': -24, 'CYCLE PAN': 11, 'LFO2 MMODE': -26,
         }),
    dict(engine='ANALOG', idx=18, name='ARP PLUCK', cat='PLUCK', bseed=12417,  # #65
         p={
            'ATK': 0, 'DEC': 77, 'SUS': 0, 'REL': 61, 'LRATE': 27, 'LWAVE': 1, 'LD_FLT': 5, 'AMODE': 1, 'AOCT': 2, 'AGATE': 76, 'RES2': 71, 'KTRK': 27,
            'CHOR': 28, 'DLY': 30, 'REV': 23, 'TWTYPE': 5, 'FREEA': 41, 'FREEB': 17, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 61, 'E2SUS': 0, 'E2REL': 61,
            'L2RATE': 17, 'L2WAVE': 1, 'CUT2': 40, 'TRIM': 103, 'E2FLT': 31, 'E2': 17, 'MOD1': ('AT', 'FRES', 17), 'MOD2': ('VEL', 'CUT', 8),
            'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 37, 'TWIN LFO2': 7, 'CYCLE RISE': 103,
            'CYCLE FALL': 103,
         },
         bend={
            'ATK': 13, 'DEC': 80, 'SUS': 2, 'REL': 70, 'ED_SHP': 3, 'LRATE': 15, 'LPHASE': 12, 'LFADE': 9, 'LD_FLT': 14, 'LD_SHP': -5, 'LD_AMP': 7,
            'RES2': 60, 'DRV2': 1, 'KTRK': 25, 'DIST': 2, 'CHOR': 31, 'DLY': 28, 'REV': 19, 'DETUNE': 49, 'FREEB': 19, 'FREEC': 4, 'M1AMT': 10,
            'M2AMT': 19, 'M3AMT': 14, 'E2ATK': 2, 'E2DEC': 69, 'E2SUS': 12, 'E2REL': 59, 'L2RATE': 18, 'L2PHASE': 1, 'L2FADE': 10, 'BLEND': -10,
            'CUT2': 49, 'E2FLT': 29, 'E2': 24, 'E3': 61, 'E4': 9, 'E7': 13, 'TWIN KTRK2': 4, 'TWIN ENV2': 29, 'TWIN LFO1': 9, 'TWIN LFO2': 3,
            'SUB LEVEL': 6, 'CYCLE RISE': 95, 'CYCLE FALL': 105, 'CYCLE CURVE': -1, 'AMPENV SATN': -5, 'FILTERENV FBLND': -10, 'FILTERENV FDRV': -12,
            'FILTERENV MRES': -2, 'CYCLE TCUT': 8, 'CYCLE PAN': 2, 'LFO2 TRES': -5, 'LFO2 MMODE': -1, 'LFO2 FBLND': -11,
         }),
    dict(engine='ANALOG', idx=12, name='PIZZICATO', cat='PLUCK', bseed=65304,  # #66
         p={
            'ATK': 0, 'DEC': 95, 'SUS': 0, 'REL': 73, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 4, 'KTRK': 27, 'CHOR': 38, 'REV': 32, 'FREEA': 43,
            'FREEB': 11, 'FREED': 2, 'E2ATK': 0, 'E2DEC': 71, 'E2SUS': 0, 'E2REL': 61, 'TRIM': 99, 'E2FLT': 31, 'E2': 0, 'E3': 0,
            'MOD1': ('AT', 'DLY', 27), 'MOD2': ('VEL', 'CUT', 10), 'MOD3': ('CYCLE', 'CUT', 11), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1,
            'CYCLE RISE': 0, 'CYCLE FALL': 71,
         },
         bend={
            'ATK': 6, 'DEC': 98, 'SUS': 2, 'REL': 60, 'ED_SHP': -11, 'LRATE': 28, 'LPHASE': 1, 'LFADE': 6, 'LD_FLT': 16, 'LD_SHP': -1, 'LD_AMP': 11,
            'KTRK': 16, 'DIST': 13, 'CHOR': 44, 'DLY': 8, 'REV': 41, 'DETUNE': 41, 'FREEB': 19, 'FREEC': 6, 'M1AMT': 35, 'M2AMT': 6, 'M3AMT': 17,
            'E2ATK': 8, 'E2DEC': 81, 'E2SUS': 6, 'E2REL': 68, 'L2RATE': 68, 'L2PHASE': 6, 'L2FADE': 6, 'E2FLT': 37, 'E2': 6, 'E3': 8, 'E4': 12,
            'E7': 10, 'SUB LEVEL': 11, 'CYCLE RISE': 1, 'CYCLE FALL': 64, 'CYCLE CURVE': -3, 'AMPENV SATN': 12, 'FILTERENV MRES': -12,
            'CYCLE PAN': -2, 'LFO2 MMODE': 12,
         }),
    dict(engine='FM6', idx=9, name='ISLAND STEEL', cat='PLUCK', bseed=19220,  # #67
         p={
            'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 5, 'KTRK': 31, 'DLY': 18, 'REV': 34, 'FREEA': 62, 'FREEB': 11, 'E2ATK': 0, 'E2DEC': 67, 'E2SUS': 0,
            'E2REL': 61, 'TRIM': 118, 'E2FLT': 7, 'E3': -7, 'E5': 11, 'E6': 3, 'E7': 14, 'E8': 7, 'MOD1': ('AT', 'E3', 7), 'MOD2': ('CYCLE', 'E3', 4),
            'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 41, 'SUB OCTAVE': 0, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 100, 'CYCLE CURVE': -17,
         },
         bend={
            'ATK': 10, 'DEC': 86, 'SUS': 2, 'REL': 43, 'ED_SHP': 12, 'LRATE': 29, 'LPHASE': 7, 'LFADE': 5, 'LD_FLT': -8, 'LD_SHP': 2, 'LD_AMP': 5,
            'KTRK': 25, 'DIST': 12, 'CHOR': 13, 'DLY': 14, 'REV': 44, 'DETUNE': 47, 'FREEA': 60, 'FREEB': 4, 'FREEC': 6, 'M1AMT': 8, 'M2AMT': 7,
            'E2ATK': 13, 'E2DEC': 73, 'E2SUS': 6, 'E2REL': 50, 'L2RATE': 52, 'L2PHASE': 11, 'L2FADE': 2, 'E2FLT': 0, 'E2': 1, 'E3': -10, 'E5': 20,
            'E6': 2, 'E7': 25, 'SUB LEVEL': 51, 'CYCLE RISE': 1, 'CYCLE FALL': 99, 'CYCLE CURVE': -14, 'AMPENV SATN': -9, 'FILTERENV MRES': 11,
            'CYCLE PAN': -1, 'LFO2 MMODE': -8,
         },
         bfm=[
            97, 95, 89, 95, 91, 97, 98, 1, 2, 91, 97, 96, 96, 98, 98, 98, 0, 2, 93, 76, 31, 55, 89, 22, 4, 4, 59, 92, 62, 41, 65, 98, 31, 1, 0, 75,
            96, 75, 46, 55, 98, 45, 9, 5, 75, 90, 43, 27, 66, 98, 61, 1, 0, 99, 99, 99, 99, 99, 50, 50, 50, 42, 3, 0, 4,
         ]),
    dict(engine='FM6', idx=3, name='MALLET WOOD', cat='PLUCK', bseed=12489,  # #68
         p={
            'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 4, 'KTRK': 31, 'REV': 15, 'FREEA': 58, 'FREEB': 11, 'E2ATK': 0, 'E2DEC': 61, 'E2SUS': 0, 'E2REL': 51,
            'TRIM': 124, 'E2FLT': 11, 'E3': 14, 'E5': 25, 'E6': 4, 'E8': 5, 'MOD1': ('AT', 'E5', 11), 'MOD2': ('CYCLE', 'E3', 7), 'LTRIG': 1,
            'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 71,
         },
         bend={
            'ATK': 5, 'DEC': 61, 'SUS': 2, 'REL': 69, 'ED_SHP': 5, 'LRATE': 16, 'LPHASE': 11, 'LFADE': 9, 'LD_FLT': 17, 'LD_SHP': 2, 'LD_AMP': 12,
            'KTRK': 39, 'DIST': 8, 'CHOR': 9, 'DLY': 11, 'REV': 25, 'DETUNE': 37, 'FREEA': 49, 'FREEB': 16, 'FREEC': 1, 'M1AMT': 3, 'M2AMT': -2,
            'E2ATK': 10, 'E2DEC': 65, 'E2SUS': 9, 'E2REL': 52, 'L2RATE': 61, 'L2PHASE': 7, 'L2FADE': 10, 'E2FLT': 7, 'E2': -1, 'E3': 18, 'E5': 33,
            'E6': 3, 'E7': 12, 'SUB LEVEL': 13, 'CYCLE RISE': 7, 'CYCLE FALL': 81, 'CYCLE CURVE': 12, 'AMPENV SATN': -4, 'FILTERENV MRES': 13,
            'CYCLE PAN': 11, 'LFO2 MMODE': 8,
         },
         bfm=[
            98, 94, 93, 98, 96, 98, 96, 8, 4, 97, 95, 96, 93, 98, 98, 98, 0, 2, 93, 75, 45, 63, 96, 10, 4, 4, 52, 89, 73, 51, 54, 98, 1, 1, 0, 74,
            97, 65, 55, 58, 96, 4, 8, 4, 73, 97, 49, 43, 48, 98, 1, 1, 0, 97, 99, 99, 99, 99, 50, 50, 50, 31, 5, 0, 3,
         ]),
    dict(engine='PHASE', idx=8, name='FAKE KOTO', cat='PLUCK', bseed=44371,  # #69
         p={
            'ATK': 0, 'DEC': 97, 'SUS': 0, 'REL': 77, 'LRATE': 21, 'LWAVE': 1, 'LD_SHP': 5, 'KTRK': 31, 'DLY': 14, 'REV': 34, 'FREEA': 64, 'FREEB': 21,
            'E2ATK': 0, 'E2DEC': 59, 'E2SUS': 0, 'E2REL': 47, 'L2RATE': 27, 'L2WAVE': 1, 'TRIM': 95, 'E2FLT': 11, 'L2FLT': 4, 'E3': 37, 'E4': 71,
            'MOD1': ('AT', 'SAT', 21), 'MOD2': ('ENV2', 'PITCH', 7), 'MOD3': ('CYCLE', 'SHP', 11), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1,
            'CYCLE RISE': 0, 'CYCLE FALL': 77,
         },
         bend={
            'ATK': 10, 'DEC': 100, 'SUS': 2, 'REL': 79, 'ED_SHP': -6, 'LRATE': 32, 'LPHASE': 1, 'LFADE': 11, 'LD_FLT': -7, 'LD_SHP': -7, 'LD_AMP': 1,
            'KTRK': 18, 'DIST': 6, 'CHOR': 6, 'DLY': 27, 'REV': 35, 'DETUNE': 45, 'FREEA': 71, 'FREEB': 17, 'FREEC': 4, 'M1AMT': 26, 'M3AMT': 18,
            'M4AMT': 10, 'E2ATK': 6, 'E2DEC': 71, 'E2SUS': 3, 'E2REL': 51, 'L2RATE': 28, 'L2PHASE': 4, 'L2FADE': 10, 'E2FLT': 21, 'E3': 49, 'E4': 62,
            'E5': 8, 'SUB LEVEL': 2, 'CYCLE RISE': 8, 'CYCLE FALL': 87, 'CYCLE CURVE': 11, 'AMPENV SATN': -13, 'FILTERENV MRES': 13, 'CYCLE PAN': 3,
            'LFO2 MMODE': -4,
         }),
    dict(engine='PHASE', idx=3, name='WIRE HARP', cat='PLUCK', bseed=12692,  # #70
         p={
            'ATK': 0, 'DEC': 86, 'SUS': 38, 'REL': 70, 'LRATE': 40, 'LWAVE': 1, 'LD_SHP': 4, 'AMODE': 1, 'ARATE': 1, 'AOCT': 2, 'AGATE': 76,
            'KTRK': 27, 'CHOR': 25, 'DLY': 25, 'REV': 51, 'FREEA': 60, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 71, 'E2SUS': 0, 'E2REL': 66, 'L2RATE': 17,
            'L2WAVE': 1, 'TRIM': 95, 'E2FLT': 11, 'L2FLT': 5, 'E1': 3, 'E2': 7, 'E3': 40, 'E4': 70, 'E5': 7, 'MOD1': ('AT', 'SHP', 7),
            'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 105, 'CYCLE FALL': 105,
         },
         bend={
            'ATK': 8, 'DEC': 89, 'SUS': 47, 'REL': 73, 'ED_SHP': -6, 'LRATE': 39, 'LPHASE': 7, 'LFADE': 2, 'LD_FLT': -11, 'LD_SHP': 14, 'LD_AMP': 1,
            'KTRK': 29, 'DIST': 11, 'CHOR': 19, 'DLY': 15, 'REV': 59, 'DETUNE': 37, 'FREEA': 57, 'FREEB': 12, 'FREEC': 4, 'M1AMT': 5, 'M2AMT': 1,
            'M3AMT': 13, 'E2ATK': 6, 'E2DEC': 84, 'E2SUS': 7, 'E2REL': 62, 'L2RATE': 26, 'L2PHASE': 9, 'L2FADE': 9, 'E2FLT': 13, 'E3': 44, 'E4': 79,
            'E5': 11, 'SUB LEVEL': 5, 'CYCLE RISE': 98, 'CYCLE FALL': 109, 'CYCLE CURVE': -11, 'AMPENV SATN': 2, 'FILTERENV MRES': -12,
            'CYCLE PAN': 11, 'LFO2 MMODE': 8,
         }),
    dict(engine='LOFI', idx=3, name='MARBLE RUN', cat='PLUCK', bseed=60893,  # #71
         p={
            'ATK': 0, 'DEC': 79, 'SUS': 0, 'REL': 69, 'EQLG': -3, 'AMODE': 1, 'ARATE': 1, 'AOCT': 2, 'AGATE': 57, 'DLY': 38, 'REV': 44, 'FREEB': 40,
            'E2ATK': 0, 'E2DEC': 74, 'E2SUS': 0, 'E2REL': 64, 'TRIM': 112, 'E2': 1, 'E8': 96, 'LTRIG': 1, 'L2TRIG': 1,
         },
         bend={
            'ATK': 8, 'DEC': 82, 'SUS': 2, 'REL': 57, 'ED_SHP': 9, 'LRATE': 72, 'LPHASE': 5, 'LFADE': 6, 'LD_FLT': -13, 'LD_SHP': -12, 'LD_AMP': 1,
            'KTRK': 27, 'DIST': 9, 'CHOR': 11, 'DLY': 49, 'REV': 46, 'DETUNE': 43, 'FREEA': 79, 'FREEB': 49, 'FREEC': 2, 'E2ATK': 13, 'E2DEC': 82,
            'E2SUS': 6, 'E2REL': 73, 'L2RATE': 72, 'L2PHASE': 13, 'L2FADE': 9, 'E2FLT': -8, 'E3': 53, 'E4': 4, 'E5': 8, 'E6': 5, 'E8': 85,
            'SUB LEVEL': 1, 'CYCLE RISE': 4, 'CYCLE FALL': 66, 'CYCLE CURVE': -7, 'AMPENV SATN': 4, 'FILTERENV MRES': -7, 'CYCLE PAN': -9,
            'LFO2 MMODE': 9,
         }),
    dict(engine='VOICE', idx=2, name='DOO WOP', cat='PLUCK', bseed=29492,  # #72
         p={
            'ATK': 0, 'DEC': 74, 'SUS': 0, 'REL': 69, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 5, 'AMODE': 3, 'ARATE': 1, 'AOCT': 2, 'AGATE': 70, 'KTRK': 17,
            'CHOR': 23, 'DLY': 28, 'REV': 44, 'FREEA': 61, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 72, 'E2SUS': 0, 'E2REL': 64, 'L2RATE': 40, 'L2WAVE': 1,
            'TRIM': 98, 'E2FLT': 11, 'L2SHP': 7, 'E1': 95, 'E2': 0, 'E3': 88, 'E5': 70, 'E6': 12, 'E7': 92, 'MOD1': ('AT', 'E4', 7),
            'MOD2': ('CYCLE', 'CUT', 11), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 71,
         },
         bend={
            'ATK': 7, 'DEC': 76, 'SUS': 2, 'REL': 80, 'ED_SHP': -13, 'LRATE': 17, 'LPHASE': 9, 'LFADE': 12, 'LD_FLT': 13, 'LD_SHP': -11, 'LD_AMP': 2,
            'KTRK': 21, 'DIST': 1, 'CHOR': 26, 'DLY': 41, 'REV': 36, 'DETUNE': 29, 'FREEA': 68, 'FREEB': 15, 'FREEC': 6, 'M1AMT': -4, 'M2AMT': 6,
            'M3AMT': 17, 'E2ATK': 2, 'E2DEC': 69, 'E2SUS': 6, 'E2REL': 66, 'L2RATE': 28, 'L2PHASE': 2, 'L2FADE': 5, 'E2FLT': 7, 'E1': 89, 'E2': 13,
            'E3': 89, 'E4': 2, 'E5': 82, 'E6': 16, 'E7': 102, 'E8': 8, 'SUB LEVEL': 13, 'CYCLE RISE': 10, 'CYCLE FALL': 74, 'CYCLE CURVE': 2,
            'AMPENV SATN': 13, 'FILTERENV MRES': -3, 'CYCLE PAN': 13, 'LFO2 MMODE': -8,
         }),
    dict(engine='TRIO', idx=4, name='PEBBLE POP', cat='PLUCK', bseed=63877,  # #73
         p={
            'ATK': 0, 'DEC': 71, 'SUS': 0, 'REL': 59, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 5, 'AMODE': 1, 'AOCT': 2, 'AGATE': 70, 'RES2': 31, 'KTRK': 17,
            'DLY': 38, 'REV': 25, 'TWTYPE': 1, 'FREEA': 66, 'FREEB': 17, 'E2ATK': 0, 'E2SUS': 0, 'L2RATE': 40, 'L2WAVE': 1, 'CUT2': 47, 'TRIM': 102,
            'E2FLT': 11, 'E2SHP': 19, 'L2SHP': 11, 'E1': 2, 'E2': 12, 'E3': 0, 'E4': 7, 'E8': 22, 'MOD1': ('AT', 'E4', 17),
            'MOD2': ('VEL', 'CUT2', 10), 'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 27, 'CYCLE RISE': 103,
            'CYCLE FALL': 103,
         },
         bend={
            'ATK': 13, 'DEC': 74, 'SUS': 2, 'REL': 64, 'ED_SHP': 10, 'LRATE': 20, 'LPHASE': 11, 'LFADE': 9, 'LD_FLT': -3, 'LD_SHP': 13, 'LD_AMP': 6,
            'RES2': 28, 'DRV2': 12, 'KTRK': 27, 'DIST': 12, 'CHOR': 12, 'DLY': 37, 'REV': 20, 'DETUNE': 33, 'FREEA': 58, 'FREEB': 11, 'FREEC': 13,
            'M1AMT': 11, 'M2AMT': 21, 'M3AMT': 17, 'M4AMT': 22, 'M5AMT': 19, 'E2ATK': 10, 'E2DEC': 78, 'E2SUS': 12, 'E2REL': 57, 'L2RATE': 49,
            'L2PHASE': 9, 'L2FADE': 7, 'BLEND': 5, 'CUT2': 45, 'E2FLT': 17, 'E4': 3, 'E8': 32, 'TWIN KTRK2': -8, 'TWIN ENV2': 29, 'TWIN LFO1': -8,
            'TWIN LFO2': -1, 'SUB LEVEL': 8, 'CYCLE RISE': 95, 'CYCLE FALL': 116, 'CYCLE CURVE': -13, 'AMPENV SATN': -3, 'FILTERENV FBLND': 2,
            'FILTERENV FDRV': -10, 'FILTERENV MRES': 10, 'CYCLE TCUT': -5, 'CYCLE PAN': 12, 'LFO2 TRES': 2, 'LFO2 MMODE': 4, 'LFO2 FBLND': -10,
         }),
    dict(engine='WHEEL', idx=4, name='NIGHT BOX', cat='PLUCK', bseed=32458,  # #74
         p={
            'ATK': 0, 'DEC': 81, 'SUS': 0, 'REL': 72, 'EQLG': -6, 'LRATE': 27, 'LWAVE': 1, 'LD_FLT': 5, 'AMODE': 1, 'ARATE': 1, 'AOCT': 2, 'KTRK': 31,
            'DLY': 25, 'REV': 51, 'FREEA': 66, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 76, 'E2SUS': 0, 'E2REL': 64, 'L2RATE': 21, 'L2WAVE': 1, 'TRIM': 111,
            'E2FLT': 11, 'E2SHP': 19, 'L2SHP': 2, 'E1': 8, 'E3': -4, 'E4': 2, 'E6': 20, 'E8': 0, 'MOD1': ('AT', 'E7', 17),
            'MOD2': ('CYCLE', 'CUT', 11), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 71,
         },
         bend={
            'ATK': 10, 'DEC': 84, 'SUS': 1, 'REL': 65, 'ED_SHP': -8, 'LRATE': 26, 'LPHASE': 5, 'LFADE': 2, 'LD_FLT': 12, 'LD_SHP': 5, 'LD_AMP': 10,
            'KTRK': 22, 'DIST': 5, 'CHOR': 10, 'DLY': 24, 'REV': 58, 'DETUNE': 29, 'FREEA': 63, 'FREEB': 9, 'FREEC': 1, 'M1AMT': 19, 'M2AMT': 13,
            'M3AMT': 26, 'M4AMT': -5, 'E2ATK': 12, 'E2DEC': 79, 'E2SUS': 10, 'E2REL': 52, 'L2RATE': 22, 'L2PHASE': 1, 'L2FADE': 11, 'E2FLT': 0,
            'E3': -3, 'E4': 1, 'E6': 30, 'E7': 13, 'SUB LEVEL': 5, 'CYCLE RISE': 2, 'CYCLE FALL': 64, 'CYCLE CURVE': -5, 'AMPENV SATN': -8,
            'FILTERENV MRES': -8, 'CYCLE PAN': -5, 'LFO2 MMODE': 9,
         }),
    dict(engine='PHYS', idx=2, name='GHOST HARP', cat='PLUCK', bseed=40612,  # #75
         p={
            'ATK': 0, 'SUS': 127, 'REL': 76, 'LRATE': 17, 'LWAVE': 1, 'LD_FLT': 5, 'KTRK': 31, 'CHOR': 25, 'REV': 51, 'FREEA': 62, 'FREEB': 11,
            'E2ATK': 0, 'E2DEC': 79, 'E2SUS': 0, 'E2REL': 73, 'L2RATE': 10, 'L2WAVE': 1, 'TRIM': 113, 'E2FLT': 7, 'E1': 3, 'E2': 8, 'E3': 67,
            'E4': 104, 'E5': 70, 'E7': 30, 'MOD1': ('AT', 'E5', 17), 'MOD2': ('LFO2', 'E3', 5), 'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 110, 'CYCLE FALL': 110,
         },
         bend={
            'ATK': 13, 'DEC': 64, 'SUS': 119, 'REL': 68, 'ED_SHP': -12, 'LRATE': 22, 'LPHASE': 3, 'LFADE': 1, 'LD_FLT': -7, 'LD_SHP': 12,
            'LD_AMP': 5, 'KTRK': 26, 'DIST': 11, 'CHOR': 23, 'DLY': 3, 'REV': 56, 'DETUNE': 38, 'FREEA': 56, 'FREEB': 0, 'FREEC': 5, 'M1AMT': 14,
            'M2AMT': -8, 'M3AMT': 12, 'E2ATK': 11, 'E2DEC': 84, 'E2SUS': 3, 'E2REL': 83, 'L2RATE': 8, 'L2PHASE': 11, 'L2FADE': 3, 'E2FLT': 6,
            'E3': 71, 'E4': 106, 'E5': 64, 'E6': 82, 'E7': 29, 'E8': 2, 'SUB LEVEL': 3, 'CYCLE RISE': 114, 'CYCLE FALL': 106, 'CYCLE CURVE': -2,
            'AMPENV SATN': 12, 'FILTERENV MRES': 10, 'CYCLE PAN': 9, 'LFO2 MMODE': -9,
         }),
    dict(engine='PHYS', idx=9, name='NURSERY KEYS', cat='PLUCK', bseed=13705,  # #76
         p={
            'ATK': 0, 'SUS': 127, 'REL': 71, 'LRATE': 27, 'LWAVE': 1, 'LD_FLT': 4, 'KTRK': 17, 'DLY': 14, 'REV': 27, 'FREEA': 67, 'FREEB': 17,
            'E2ATK': 0, 'E2DEC': 61, 'E2SUS': 0, 'E2REL': 51, 'L2RATE': 30, 'L2WAVE': 1, 'TRIM': 113, 'E2FLT': 11, 'E2': 124, 'E3': 91, 'E4': 71,
            'E5': 10, 'E6': 107, 'E8': 21, 'MOD1': ('AT', 'E3', 11), 'MOD2': ('LFO2', 'E4', 5), 'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 71,
         },
         bend={
            'ATK': 1, 'DEC': 77, 'SUS': 124, 'REL': 74, 'ED_SHP': -9, 'LRATE': 22, 'LPHASE': 8, 'LFADE': 4, 'LD_FLT': -3, 'LD_SHP': 13, 'LD_AMP': 7,
            'KTRK': 13, 'DIST': 9, 'CHOR': 8, 'DLY': 5, 'REV': 38, 'DETUNE': 48, 'FREEA': 76, 'FREEB': 25, 'FREEC': 2, 'M1AMT': 6, 'M2AMT': -8,
            'M3AMT': 4, 'E2ATK': 8, 'E2DEC': 52, 'E2SUS': 6, 'E2REL': 50, 'L2RATE': 19, 'L2PHASE': 1, 'L2FADE': 3, 'E2FLT': 2, 'E2': 118, 'E3': 92,
            'E4': 73, 'E5': 7, 'E6': 114, 'E7': 13, 'E8': 11, 'SUB LEVEL': 3, 'CYCLE RISE': 9, 'CYCLE FALL': 82, 'CYCLE CURVE': -13,
            'AMPENV SATN': 4, 'FILTERENV MRES': 13, 'CYCLE PAN': 4, 'LFO2 MMODE': -7,
         }),
    dict(engine='PHYS', idx=3, name='NYLON GUT', cat='PLUCK', bseed=53812,  # #77
         p={
            'ATK': 0, 'SUS': 127, 'LRATE': 27, 'LWAVE': 1, 'LD_FLT': 5, 'AMODE': 1, 'AOCT': 2, 'AGATE': 108, 'KTRK': 31, 'CHOR': 23, 'REV': 32,
            'FREEA': 56, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 79, 'E2SUS': 0, 'E2REL': 69, 'L2RATE': 17, 'L2WAVE': 1, 'TRIM': 121, 'E2FLT': 11, 'E1': 1,
            'E2': 56, 'E3': 58, 'E4': 110, 'E5': 40, 'E6': 120, 'MOD1': ('AT', 'REV', 27), 'MOD2': ('LFO2', 'E5', 5), 'MOD3': ('CYCLE', 'CUT', 11),
            'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 71,
         },
         bend={
            'ATK': 2, 'DEC': 57, 'SUS': 126, 'REL': 56, 'ED_SHP': -8, 'LRATE': 39, 'LPHASE': 4, 'LFADE': 9, 'LD_FLT': 3, 'LD_SHP': 1, 'LD_AMP': 4,
            'KTRK': 24, 'DIST': 13, 'CHOR': 25, 'DLY': 6, 'REV': 25, 'DETUNE': 42, 'FREEA': 55, 'FREEB': 14, 'FREEC': 2, 'M1AMT': 26, 'M2AMT': -2,
            'M3AMT': 18, 'E2ATK': 12, 'E2DEC': 89, 'E2SUS': 12, 'E2REL': 61, 'L2RATE': 25, 'L2PHASE': 3, 'L2FADE': 2, 'E2FLT': -2, 'E2': 58,
            'E3': 56, 'E4': 106, 'E5': 53, 'E6': 109, 'E7': 13, 'E8': 5, 'SUB LEVEL': 11, 'CYCLE RISE': 7, 'CYCLE FALL': 79, 'CYCLE CURVE': 1,
            'AMPENV SATN': 6, 'FILTERENV MRES': 4, 'CYCLE PAN': 8, 'LFO2 MMODE': 2,
         }),
    dict(engine='WAVE', idx=6, name='DIGI PLUCK', cat='PLUCK', bseed=63098,  # #78
         p={
            'ATK': 0, 'DEC': 90, 'SUS': 0, 'REL': 76, 'ED_SHP': 51, 'LRATE': 27, 'LWAVE': 1, 'LD_SHP': 9, 'KTRK': 27, 'CHOR': 25, 'DLY': 23, 'REV': 32,
            'FREEA': 58, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 84, 'E2SUS': 0, 'E2REL': 69, 'L2RATE': 27, 'L2WAVE': 1, 'TRIM': 124, 'E2FLT': 11,
            'L2FLT': 5, 'E1': 10, 'E2': 20, 'MOD1': ('AT', 'SHP', 17), 'MOD2': ('VEL', 'SHP', 16), 'MOD3': ('ENV2', 'E6', 17),
            'MOD4': ('CYCLE', 'SHP', 9), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 77,
         },
         bend={
            'ATK': 2, 'DEC': 92, 'SUS': 2, 'REL': 65, 'ED_SHP': 46, 'LRATE': 17, 'LPHASE': 8, 'LFADE': 3, 'LD_FLT': -2, 'LD_SHP': 18, 'LD_AMP': 4,
            'KTRK': 22, 'DIST': 11, 'CHOR': 26, 'DLY': 26, 'REV': 25, 'DETUNE': 52, 'FREEA': 63, 'FREEB': 8, 'FREEC': 9, 'M1AMT': 21, 'M2AMT': 19,
            'M3AMT': 19, 'M4AMT': 2, 'M5AMT': 1, 'E2ATK': 6, 'E2DEC': 71, 'E2SUS': 7, 'E2REL': 82, 'L2RATE': 33, 'L2PHASE': 10, 'L2FADE': 3,
            'E2FLT': 20, 'E2': 16, 'E3': 5, 'E4': 4, 'E6': 3, 'SUB LEVEL': 5, 'CYCLE RISE': 1, 'CYCLE FALL': 66, 'CYCLE CURVE': -5,
            'AMPENV SATN': -7, 'FILTERENV MRES': -8, 'CYCLE PAN': -2, 'LFO2 MMODE': -10,
         }),
    dict(engine='ANALOG', idx=13, name='SFORZANDO', cat='KEYS', bseed=46167,  # #79
         p={
            'ATK': 22, 'DEC': 79, 'SUS': 70, 'REL': 66, 'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 4, 'KTRK': 17, 'CHOR': 32, 'REV': 25, 'FREEA': 31,
            'FREEB': 17, 'E2ATK': 32, 'E2DEC': 76, 'E2SUS': 57, 'E2REL': 66, 'TRIM': 101, 'E2FLT': 37, 'E2': 7, 'MOD1': ('AT', 'CUT', 11),
            'MOD2': ('VEL', 'CUT', 10), 'MOD3': ('CYCLE', 'CUT', 5), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         },
         bend={
            'ATK': 20, 'DEC': 87, 'SUS': 82, 'REL': 75, 'ED_SHP': -3, 'LRATE': 29, 'LPHASE': 10, 'LFADE': 3, 'LD_FLT': 13, 'LD_SHP': -2, 'LD_AMP': 5,
            'KTRK': 21, 'DIST': 5, 'CHOR': 31, 'DLY': 12, 'REV': 14, 'DETUNE': 53, 'FREEA': 38, 'FREEB': 5, 'FREEC': 7, 'M1AMT': 15, 'M2AMT': -2,
            'M3AMT': -6, 'E2ATK': 27, 'E2DEC': 71, 'E2SUS': 60, 'E2REL': 76, 'L2RATE': 63, 'L2PHASE': 7, 'L2FADE': 2, 'E2FLT': 38, 'E2': 4, 'E3': 73,
            'E4': 9, 'E7': 6, 'SUB LEVEL': 7, 'CYCLE RISE': 91, 'CYCLE FALL': 92, 'CYCLE CURVE': 11, 'AMPENV SATN': 1, 'FILTERENV MRES': 9,
            'CYCLE PAN': 9, 'LFO2 MMODE': 10,
         }),
    dict(engine='FM6', idx=5, name='BIG STAB', cat='KEYS', bseed=37822,  # #80
         p={
            'LRATE': 21, 'LWAVE': 1, 'LD_FLT': 5, 'KTRK': 17, 'DIST': 25, 'CHOR': 32, 'REV': 25, 'FREEA': 64, 'FREEB': 11, 'E2ATK': 22, 'E2DEC': 86,
            'E2SUS': 76, 'E2REL': 66, 'TRIM': 111, 'E2FLT': 17, 'E2': 1, 'E3': -8, 'E6': 4, 'E8': 3, 'MOD1': ('AT', 'E7', 31),
            'MOD2': ('ENV2', 'E3', 17), 'MOD3': ('CYCLE', 'E3', 4), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         },
         bend={
            'ATK': 36, 'DEC': 59, 'SUS': 120, 'REL': 37, 'ED_SHP': -7, 'LRATE': 31, 'LPHASE': 5, 'LFADE': 7, 'LD_FLT': -4, 'LD_SHP': 6, 'LD_AMP': 11,
            'KTRK': 24, 'DIST': 36, 'CHOR': 21, 'DLY': 3, 'REV': 23, 'DETUNE': 35, 'FREEA': 72, 'FREEB': 23, 'FREEC': 9, 'M1AMT': 38, 'M2AMT': 20,
            'M3AMT': -1, 'E2ATK': 18, 'E2DEC': 92, 'E2SUS': 64, 'E2REL': 55, 'L2RATE': 67, 'L2PHASE': 5, 'L2FADE': 2, 'E2FLT': 13, 'E2': 2, 'E3': 2,
            'E5': 8, 'E6': 3, 'E7': 4, 'SUB LEVEL': 4, 'CYCLE RISE': 104, 'CYCLE FALL': 95, 'CYCLE CURVE': -12, 'AMPENV SATN': -5,
            'FILTERENV MRES': -10, 'CYCLE PAN': 12, 'LFO2 MMODE': 13,
         },
         bfm=[
            61, 58, 35, 63, 97, 93, 80, 8, 78, 69, 54, 46, 57, 98, 91, 91, 0, 89, 41, 46, 38, 67, 97, 78, 80, 3, 73, 58, 53, 37, 62, 98, 91, 91, 0,
            90, 61, 45, 30, 50, 97, 87, 83, 8, 81, 74, 43, 48, 67, 98, 93, 91, 0, 96, 80, 60, 99, 60, 46, 50, 50, 34, 45, 6, 10,
         ]),
    dict(engine='FM6', idx=6, name='FROST BELLS', cat='KEYS', bseed=48288,  # #81
         p={
            'EQLG': -5, 'CHOR': 13, 'REV': 70, 'FREEB': 40, 'E2ATK': 0, 'E2DEC': 98, 'E2SUS': 0, 'E2REL': 88, 'ITYPE': 1, 'IA': 40, 'IB': 127,
            'IC': 112, 'TRIM': 97, 'E6': 2, 'E7': 13, 'E8': 1, 'MOD1': ('AT', 'E3', 4), 'MOD2': ('CYCLE', 'E3', 4), 'LTRIG': 1, 'L2TRIG': 1,
            'SUB LEVEL': 51, 'SUB OCTAVE': 0, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 100, 'CYCLE CURVE': -21,
         },
         bend={
            'ATK': 3, 'DEC': 93, 'SUS': 1, 'REL': 81, 'ED_SHP': -13, 'LRATE': 63, 'LPHASE': 8, 'LFADE': 10, 'LD_FLT': 6, 'LD_SHP': -5, 'LD_AMP': 4,
            'KTRK': 29, 'DIST': 2, 'CHOR': 11, 'DLY': 7, 'REV': 79, 'DETUNE': 51, 'FREEA': 80, 'FREEB': 42, 'FREEC': 1, 'M1AMT': 10, 'M2AMT': -8,
            'E2ATK': 12, 'E2DEC': 102, 'E2SUS': 2, 'E2REL': 82, 'L2RATE': 73, 'L2PHASE': 2, 'L2FADE': 4, 'IA': 41, 'IB': 119, 'IC': 114, 'IMIX': 124,
            'E2FLT': 9, 'E2': -1, 'E3': -5, 'E5': 11, 'E6': 1, 'E7': 8, 'SUB LEVEL': 60, 'CYCLE RISE': 1, 'CYCLE FALL': 97, 'CYCLE CURVE': -15,
            'AMPENV SATN': 2, 'FILTERENV MRES': -12, 'CYCLE PAN': 9, 'LFO2 MMODE': -8,
         },
         bfm=[
            95, 61, 44, 41, 90, 57, 1, 1, 53, 91, 58, 42, 53, 98, 51, 1, 0, 79, 93, 58, 38, 42, 94, 65, 8, 9, 74, 96, 47, 29, 48, 98, 71, 1, 0, 83,
            89, 36, 29, 34, 92, 69, 1, 9, 86, 89, 41, 28, 40, 98, 81, 1, 0, 97, 99, 99, 99, 99, 50, 50, 50, 28, 8, 0, 1,
         ]),
    dict(engine='FM6', idx=10, name='SALOON KEYS', cat='KEYS', bseed=28718,  # #82
         p={
            'KTRK': 27, 'CHOR': 9, 'REV': 18, 'FREEA': 57, 'FREEB': 11, 'E2ATK': 0, 'E2DEC': 83, 'E2SUS': 0, 'E2REL': 71, 'L2RATE': 27, 'TRIM': 108,
            'E2FLT': 17, 'E3': 7, 'E6': 3, 'E7': 71, 'MOD1': ('AT', 'E5', 7), 'MOD2': ('ENV2', 'E3', 7), 'MOD3': ('LFO2', 'E7', 5),
            'MOD4': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 100, 'CYCLE FALL': 103,
         },
         bend={
            'ATK': 8, 'DEC': 89, 'SUS': 111, 'REL': 38, 'ED_SHP': -11, 'LRATE': 55, 'LPHASE': 8, 'LFADE': 8, 'LD_FLT': 11, 'LD_SHP': -7, 'LD_AMP': 2,
            'KTRK': 35, 'DIST': 6, 'CHOR': 12, 'DLY': 13, 'REV': 19, 'DETUNE': 41, 'FREEA': 60, 'FREEB': 24, 'FREEC': 6, 'M1AMT': 9, 'M2AMT': 8,
            'M3AMT': 7, 'M4AMT': 3, 'E2ATK': 7, 'E2DEC': 84, 'E2SUS': 5, 'E2REL': 61, 'L2RATE': 35, 'L2PHASE': 3, 'L2FADE': 6, 'E2FLT': 13, 'E2': -1,
            'E3': 18, 'E5': 9, 'E6': 4, 'E7': 84, 'SUB LEVEL': 13, 'CYCLE RISE': 96, 'CYCLE FALL': 106, 'CYCLE CURVE': 12, 'AMPENV SATN': -3,
            'FILTERENV MRES': -9, 'CYCLE PAN': -4, 'LFO2 MMODE': 13,
         },
         bfm=[
            96, 32, 35, 59, 98, 71, 3, 4, 45, 89, 31, 28, 69, 98, 94, 89, 0, 76, 89, 61, 30, 52, 93, 64, 7, 1, 75, 96, 14, 24, 43, 98, 96, 91, 0, 92,
            96, 54, 30, 74, 97, 71, 1, 1, 61, 87, 17, 31, 69, 98, 95, 93, 0, 96, 99, 99, 99, 99, 50, 50, 50, 33, 30, 0, 4,
         ]),
    dict(engine='FM6', idx=4, name='TINE ROOM', cat='KEYS', bseed=14977,  # #83
         p={
            'KTRK': 27, 'CHOR': 51, 'REV': 28, 'FREEA': 64, 'FREEB': 7, 'E2ATK': 0, 'E2DEC': 86, 'E2SUS': 0, 'E2REL': 73, 'L2RATE': 81, 'TRIM': 107,
            'E2FLT': 11, 'E5': 30, 'E6': 3, 'E7': 13, 'MOD1': ('AT', 'E7', 17), 'MOD2': ('ENV2', 'E3', 7), 'MOD3': ('LFO2', 'AMP', 9),
            'MOD4': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 51, 'SUB OCTAVE': 0, 'CYCLE RISE': 100, 'CYCLE FALL': 100,
         },
         bend={
            'ATK': 0, 'DEC': 90, 'SUS': 105, 'REL': 38, 'ED_SHP': 2, 'LRATE': 73, 'LPHASE': 9, 'LFADE': 1, 'LD_FLT': -3, 'LD_SHP': 1, 'LD_AMP': 9,
            'KTRK': 30, 'DIST': 10, 'CHOR': 64, 'DLY': 9, 'REV': 15, 'DETUNE': 50, 'FREEA': 66, 'FREEB': 6, 'FREEC': 5, 'M1AMT': 21, 'M2AMT': 3,
            'M3AMT': -4, 'M4AMT': 19, 'E2ATK': 6, 'E2DEC': 85, 'E2SUS': 13, 'E2REL': 79, 'L2RATE': 83, 'L2PHASE': 6, 'L2FADE': 2, 'E2FLT': 6,
            'E2': 1, 'E3': -5, 'E5': 24, 'E6': 2, 'E7': 26, 'SUB LEVEL': 43, 'CYCLE RISE': 98, 'CYCLE FALL': 89, 'CYCLE CURVE': 12,
            'AMPENV SATN': -7, 'FILTERENV MRES': 4, 'CYCLE PAN': -12, 'LFO2 MMODE': 8,
         },
         bfm=[
            88, 43, 32, 62, 96, 87, 9, 6, 59, 92, 22, 15, 63, 98, 96, 91, 0, 76, 88, 61, 44, 62, 95, 55, 9, 3, 58, 89, 25, 23, 55, 98, 98, 93, 0, 89,
            85, 53, 43, 88, 95, 68, 1, 10, 49, 86, 32, 31, 64, 98, 97, 93, 0, 99, 99, 99, 99, 99, 50, 50, 50, 25, 34, 0, 2,
         ]),
    dict(engine='FM6', idx=13, name='TUBE BELLS', cat='KEYS', bseed=22916,  # #84
         p={
            'CHOR': 8, 'REV': 38, 'FREEB': 40, 'TRIM': 107, 'E8': 42, 'MOD1': ('AT', 'DLY', 31), 'MOD2': ('CYCLE', 'E3', 4), 'LTRIG': 1, 'L2TRIG': 1,
            'SUB LEVEL': 41, 'SUB OCTAVE': 0, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 105, 'CYCLE CURVE': -31,
         },
         bend={
            'ATK': 7, 'DEC': 97, 'SUS': 2, 'REL': 62, 'ED_SHP': 9, 'LRATE': 69, 'LPHASE': 4, 'LFADE': 2, 'LD_FLT': -12, 'LD_SHP': 10, 'LD_AMP': 6,
            'KTRK': 21, 'DIST': 7, 'CHOR': 1, 'DLY': 1, 'REV': 44, 'DETUNE': 36, 'FREEA': 77, 'FREEB': 50, 'FREEC': 6, 'M1AMT': 39, 'M2AMT': 3,
            'E2ATK': 23, 'E2DEC': 67, 'E2SUS': 100, 'E2REL': 61, 'L2RATE': 65, 'L2PHASE': 11, 'L2FADE': 6, 'E2FLT': 2, 'E2': -1, 'E3': 4, 'E5': 1,
            'E6': 1, 'E7': 7, 'SUB LEVEL': 54, 'CYCLE RISE': 13, 'CYCLE FALL': 92, 'CYCLE CURVE': -19, 'AMPENV SATN': -2, 'FILTERENV MRES': 13,
            'CYCLE PAN': -13, 'LFO2 MMODE': 6,
         },
         bfm=[
            90, 53, 48, 46, 89, 44, 7, 5, 40, 98, 52, 30, 42, 98, 69, 1, 0, 64, 90, 54, 28, 51, 92, 57, 3, 6, 64, 97, 40, 23, 44, 98, 81, 1, 0, 75,
            90, 37, 21, 54, 89, 62, 5, 6, 85, 96, 30, 27, 54, 98, 89, 1, 0, 99, 99, 99, 99, 99, 50, 50, 50, 44, 7, 0, 7,
         ]),
    dict(engine='PHASE', idx=4, name='CLAVICLE', cat='KEYS', bseed=5811,  # #85
         p={
            'ATK': 0, 'DEC': 75, 'SUS': 23, 'REL': 54, 'LRATE': 40, 'LD_SHP': 6, 'KTRK': 27, 'CHOR': 15, 'REV': 15, 'FREEA': 56, 'FREEB': 17,
            'E2ATK': 0, 'E2DEC': 66, 'E2SUS': 0, 'E2REL': 51, 'TRIM': 105, 'E2FLT': 31, 'E1': 1, 'E3': 50, 'E4': 61, 'MOD1': ('AT', 'SHP', 7),
            'MOD2': ('CYCLE', 'SHP', 11), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 77,
         },
         bend={
            'ATK': 13, 'DEC': 78, 'SUS': 12, 'REL': 47, 'ED_SHP': 1, 'LRATE': 50, 'LPHASE': 13, 'LFADE': 7, 'LD_FLT': 6, 'LD_SHP': 13, 'LD_AMP': 2,
            'KTRK': 38, 'DIST': 3, 'CHOR': 20, 'DLY': 1, 'REV': 9, 'DETUNE': 47, 'FREEA': 65, 'FREEB': 20, 'FREEC': 5, 'M1AMT': -6, 'M2AMT': 5,
            'E2ATK': 5, 'E2DEC': 71, 'E2SUS': 12, 'E2REL': 45, 'L2RATE': 54, 'L2PHASE': 13, 'L2FADE': 11, 'E2FLT': 29, 'E3': 38, 'E4': 72, 'E5': 3,
            'SUB LEVEL': 11, 'CYCLE RISE': 1, 'CYCLE FALL': 66, 'CYCLE CURVE': -7, 'AMPENV SATN': 5, 'FILTERENV MRES': 11, 'CYCLE PAN': -9,
            'LFO2 MMODE': 1,
         }),
    dict(engine='TRIO', idx=3, name='TIN TOLL', cat='KEYS', bseed=43504,  # #86
         p={
            'ATK': 0, 'DEC': 99, 'SUS': 0, 'REL': 92, 'EQLG': -5, 'RES2': 22, 'KTRK': 0, 'CHOR': 13, 'REV': 64, 'TWTYPE': 1, 'FREEA': 56, 'FREEB': 17,
            'E2ATK': 0, 'E2DEC': 86, 'E2SUS': 0, 'E2REL': 79, 'L2RATE': 32, 'CUT2': 67, 'TRIM': 102, 'E2FLT': 17, 'L2FLT': 4, 'E1': 13, 'E3': 17,
            'E4': 7, 'MOD1': ('AT', 'CUT2', 7), 'MOD2': ('VEL', 'CUT2', 10), 'MOD3': ('CYCLE', 'CUT2', 11), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0,
            'TWIN ENV2': 10, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 95, 'CYCLE CURVE': -21,
         },
         bend={
            'ATK': 7, 'DEC': 102, 'SUS': 2, 'REL': 93, 'ED_SHP': 10, 'LRATE': 59, 'LPHASE': 10, 'LFADE': 6, 'LD_FLT': -12, 'LD_SHP': 5, 'LD_AMP': 3,
            'RES2': 43, 'DRV2': 11, 'KTRK': -5, 'DIST': 10, 'CHOR': 5, 'DLY': 10, 'REV': 66, 'DETUNE': 46, 'FREEA': 54, 'FREEB': 10, 'FREEC': 10,
            'M1AMT': -3, 'M2AMT': 1, 'M3AMT': 13, 'M4AMT': -8, 'E2ATK': 12, 'E2DEC': 97, 'E2SUS': 7, 'E2REL': 83, 'L2RATE': 24, 'L2PHASE': 10,
            'L2FADE': 3, 'BLEND': 8, 'CUT2': 80, 'E2FLT': 26, 'E4': 6, 'E8': 51, 'TWIN KTRK2': -2, 'TWIN ENV2': 1, 'TWIN LFO1': -8, 'TWIN LFO2': 9,
            'SUB LEVEL': 3, 'CYCLE RISE': 8, 'CYCLE FALL': 92, 'CYCLE CURVE': -16, 'AMPENV SATN': -4, 'FILTERENV FBLND': 12, 'FILTERENV FDRV': -7,
            'FILTERENV MRES': -12, 'CYCLE TCUT': 11, 'CYCLE PAN': 4, 'LFO2 TRES': -8, 'LFO2 MMODE': -3, 'LFO2 FBLND': 10,
         }),
    dict(engine='WHEEL', idx=3, name='JAZZ CLUB', cat='KEYS', bseed=57778,  # #87
         p={
            'SUS': 127, 'REL': 44, 'REV': 19, 'FREEA': 67, 'E2ATK': 0, 'E2DEC': 73, 'E2SUS': 0, 'E2REL': 47, 'L2RATE': 23, 'E2SHP': 8, 'E5': 2,
            'E6': 45, 'E7': 12, 'MOD1': ('AT', 'E7', 19), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO2', 'SHP', 3), 'LTRIG': 1, 'L2TRIG': 1,
            'CYCLE RISE': 103, 'CYCLE FALL': 105,
         },
         bend={
            'ATK': 17, 'DEC': 57, 'SUS': 125, 'REL': 51, 'ED_SHP': 11, 'LRATE': 71, 'LPHASE': 8, 'LFADE': 13, 'LD_FLT': 8, 'LD_SHP': -2, 'LD_AMP': 7,
            'KTRK': 31, 'DIST': 7, 'CHOR': 6, 'DLY': 13, 'REV': 28, 'DETUNE': 39, 'FREEA': 64, 'FREEB': 8, 'FREEC': 1, 'M1AMT': 24, 'M2AMT': -6,
            'M3AMT': -2, 'M4AMT': -2, 'E2ATK': 8, 'E2DEC': 68, 'E2SUS': 6, 'E2REL': 58, 'L2RATE': 20, 'L2PHASE': 9, 'L2FADE': 7, 'E2FLT': -3,
            'E3': 1, 'E4': 1, 'E6': 42, 'E7': 19, 'SUB LEVEL': 3, 'CYCLE RISE': 107, 'CYCLE FALL': 100, 'CYCLE CURVE': 6, 'AMPENV SATN': -13,
            'FILTERENV MRES': -11, 'CYCLE PAN': 2, 'LFO2 MMODE': -4,
         }),
    dict(engine='PHYS', idx=10, name='FAUX HARPSI', cat='KEYS', bseed=48968,  # #88
         p={
            'ATK': 0, 'SUS': 127, 'REL': 59, 'KTRK': 27, 'REV': 22, 'FREEA': 64, 'FREEB': 11, 'E2ATK': 0, 'E2DEC': 71, 'E2SUS': 0, 'E2REL': 56,
            'L2RATE': 23, 'ITYPE': 1, 'IA': 100, 'IB': 127, 'IC': 112, 'TRIM': 111, 'E2FLT': 11, 'L2FLT': 4, 'E1': 1, 'E2': 71, 'E3': 107, 'E4': 44,
            'E5': 11, 'E6': 127, 'MOD1': ('AT', 'E3', 7), 'MOD2': ('CYCLE', 'E5', 11), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 0,
            'CYCLE FALL': 77,
         },
         bend={
            'ATK': 3, 'DEC': 60, 'SUS': 124, 'REL': 71, 'ED_SHP': -2, 'LRATE': 56, 'LPHASE': 8, 'LFADE': 10, 'LD_FLT': 11, 'LD_SHP': -3, 'LD_AMP': 8,
            'KTRK': 37, 'DIST': 13, 'CHOR': 1, 'DLY': 2, 'REV': 28, 'DETUNE': 53, 'FREEA': 63, 'FREEB': 21, 'FREEC': 13, 'M1AMT': 4, 'M2AMT': 3,
            'M3AMT': -1, 'E2ATK': 8, 'E2DEC': 64, 'E2SUS': 8, 'E2REL': 47, 'L2RATE': 13, 'L2PHASE': 4, 'L2FADE': 5, 'IA': 92, 'IB': 122, 'IC': 122,
            'IMIX': 119, 'E2FLT': 19, 'E2': 74, 'E3': 114, 'E4': 49, 'E5': 23, 'E6': 119, 'E7': 2, 'E8': 3, 'SUB LEVEL': 4, 'CYCLE RISE': 13,
            'CYCLE FALL': 85, 'CYCLE CURVE': -12, 'AMPENV SATN': -12, 'FILTERENV MRES': 1, 'CYCLE PAN': 13, 'LFO2 MMODE': -7,
         }),
    dict(engine='PHYS', idx=5, name='MOTOR VIBES', cat='KEYS', bseed=34914,  # #89
         p={
            'ATK': 0, 'SUS': 127, 'REL': 83, 'EQLG': -4, 'KTRK': 27, 'DLY': 18, 'REV': 38, 'FREEA': 60, 'FREEB': 11, 'E2ATK': 0, 'E2DEC': 103,
            'E2SUS': 0, 'E2REL': 83, 'L2RATE': 89, 'TRIM': 120, 'E2FLT': 17, 'E2': 110, 'E3': 77, 'E4': 127, 'E5': 18, 'E6': 80, 'E8': 7,
            'MOD1': ('AT', 'E5', 17), 'MOD2': ('LFO2', 'AMP', 17), 'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 100,
            'CYCLE FALL': 103,
         },
         bend={
            'ATK': 13, 'DEC': 65, 'SUS': 116, 'REL': 71, 'ED_SHP': -8, 'LRATE': 58, 'LPHASE': 13, 'LFADE': 6, 'LD_FLT': 2, 'LD_SHP': 2, 'LD_AMP': 7,
            'KTRK': 16, 'DIST': 9, 'CHOR': 12, 'DLY': 8, 'REV': 37, 'DETUNE': 50, 'FREEA': 61, 'FREEB': 7, 'FREEC': 9, 'M1AMT': 25, 'M2AMT': 6,
            'M3AMT': -2, 'E2ATK': 1, 'E2DEC': 113, 'E2SUS': 11, 'E2REL': 91, 'L2RATE': 102, 'L2PHASE': 8, 'L2FADE': 4, 'E2FLT': 26, 'E2': 104,
            'E3': 78, 'E4': 126, 'E5': 25, 'E6': 76, 'E7': 10, 'E8': 15, 'SUB LEVEL': 2, 'CYCLE RISE': 95, 'CYCLE FALL': 115, 'CYCLE CURVE': -9,
            'AMPENV SATN': -4, 'FILTERENV MRES': -11, 'CYCLE PAN': -7, 'LFO2 MMODE': 2,
         }),
    dict(engine='PHYS', idx=4, name='ROSEWOOD', cat='KEYS', bseed=55371,  # #90
         p={
            'ATK': 0, 'SUS': 127, 'REL': 59, 'LRATE': 40, 'LD_FLT': 4, 'RES2': 21, 'KTRK': 27, 'REV': 14, 'TWTYPE': 1, 'FREEA': 41, 'FREEB': 17,
            'FREED': 2, 'E2ATK': 0, 'E2DEC': 71, 'E2SUS': 0, 'E2REL': 56, 'L2RATE': 23, 'CUT2': 47, 'TRIM': 127, 'E2FLT': 31, 'E2': 116, 'E3': 47,
            'E4': 57, 'E5': 24, 'E6': 100, 'E8': 47, 'MOD1': ('AT', 'E4', 11), 'MOD2': ('CYCLE', 'E5', 17), 'MOD3': ('LFO2', 'E3', 4), 'LTRIG': 1,
            'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 17, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 71,
         },
         bend={
            'ATK': 9, 'DEC': 79, 'SUS': 116, 'REL': 56, 'ED_SHP': -7, 'LRATE': 39, 'LPHASE': 12, 'LFADE': 5, 'LD_FLT': -4, 'LD_SHP': -5,
            'LD_AMP': 12, 'RES2': 14, 'DRV2': 3, 'KTRK': 35, 'DIST': 3, 'CHOR': 5, 'DLY': 10, 'REV': 17, 'DETUNE': 34, 'FREEA': 46, 'FREEB': 12,
            'FREEC': 2, 'M1AMT': 4, 'M2AMT': 9, 'M3AMT': -4, 'E2ATK': 4, 'E2DEC': 69, 'E2SUS': 4, 'E2REL': 49, 'L2RATE': 13, 'L2PHASE': 10,
            'L2FADE': 13, 'BLEND': 13, 'CUT2': 45, 'E2FLT': 29, 'E2': 103, 'E3': 48, 'E4': 54, 'E5': 36, 'E6': 99, 'E7': 13, 'E8': 59,
            'TWIN KTRK2': 10, 'TWIN ENV2': 21, 'TWIN LFO1': -12, 'TWIN LFO2': -7, 'SUB LEVEL': 4, 'CYCLE RISE': 10, 'CYCLE FALL': 83,
            'CYCLE CURVE': -10, 'AMPENV SATN': 8, 'FILTERENV FBLND': -10, 'FILTERENV FDRV': 1, 'FILTERENV MRES': 3, 'CYCLE TCUT': 3, 'CYCLE PAN': 12,
            'LFO2 TRES': -8, 'LFO2 MMODE': 9, 'LFO2 FBLND': 5,
         }),
    dict(engine='WAVE', idx=4, name='DRAWBARS', cat='KEYS', bseed=57282,  # #91
         p={
            'DEC': 64, 'SUS': 127, 'REL': 44, 'LRATE': 32, 'LD_SHP': 6, 'DIST': 7, 'CHOR': 44, 'REV': 25, 'FREEA': 67, 'E2ATK': 0, 'E2DEC': 79,
            'E2SUS': 0, 'E2REL': 47, 'TRIM': 99, 'E2SHP': 16, 'E1': 4, 'E2': 32, 'MOD1': ('AT', 'SAT', 17), 'MOD2': ('ENV2', 'E3', 11),
            'MOD3': ('CYCLE', 'CHO', 11), 'LTRIG': 1, 'L2TRIG': 1, 'SUB LEVEL': 98, 'CYCLE RISE': 110, 'CYCLE FALL': 110,
         },
         bend={
            'ATK': 23, 'DEC': 68, 'SUS': 125, 'REL': 31, 'ED_SHP': -4, 'LRATE': 38, 'LPHASE': 7, 'LFADE': 11, 'LD_FLT': -1, 'LD_SHP': 13,
            'LD_AMP': 7, 'KTRK': 24, 'DIST': 17, 'CHOR': 43, 'DLY': 4, 'REV': 23, 'DETUNE': 47, 'FREEA': 71, 'FREEB': 8, 'FREEC': 5, 'M1AMT': 29,
            'M2AMT': 16, 'M3AMT': 17, 'M4AMT': 9, 'E2ATK': 4, 'E2DEC': 70, 'E2SUS': 3, 'E2REL': 60, 'L2RATE': 71, 'L2PHASE': 11, 'L2FADE': 2,
            'E2FLT': -9, 'E2': 22, 'E3': 2, 'E4': 12, 'E6': 3, 'SUB LEVEL': 105, 'CYCLE RISE': 103, 'CYCLE FALL': 114, 'CYCLE CURVE': 6,
            'AMPENV SATN': 4, 'FILTERENV MRES': -8, 'CYCLE PAN': -1, 'LFO2 MMODE': 4,
         }),
    dict(engine='WAVE', idx=16, name='PLATE BELL', cat='KEYS', bseed=19014,  # #92
         p={
            'ATK': 0, 'DEC': 109, 'SUS': 0, 'REL': 106, 'ED_SHP': 45, 'KTRK': 27, 'CHOR': 15, 'REV': 64, 'FREEA': 55, 'FREEB': 11, 'E2ATK': 0,
            'E2DEC': 100, 'E2SUS': 0, 'E2REL': 88, 'L2RATE': 23, 'TRIM': 108, 'E2FLT': 21, 'L2SHP': 5, 'E1': 23, 'E2': 30, 'MOD1': ('AT', 'REV', 31),
            'MOD2': ('VEL', 'SHP', 13), 'MOD3': ('AENV', 'E3', 13), 'MOD4': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1,
            'CYCLE RISE': 0, 'CYCLE FALL': 100, 'CYCLE CURVE': -21,
         },
         bend={
            'ATK': 7, 'DEC': 112, 'SUS': 2, 'REL': 101, 'ED_SHP': 46, 'LRATE': 65, 'LPHASE': 5, 'LFADE': 11, 'LD_FLT': 13, 'LD_SHP': -13,
            'LD_AMP': 1, 'KTRK': 23, 'DIST': 13, 'CHOR': 2, 'DLY': 6, 'REV': 77, 'DETUNE': 43, 'FREEA': 57, 'FREEB': 10, 'FREEC': 8, 'M1AMT': 28,
            'M2AMT': 15, 'M3AMT': 10, 'M4AMT': 14, 'M5AMT': 18, 'E2ATK': 10, 'E2DEC': 93, 'E2SUS': 9, 'E2REL': 100, 'L2RATE': 18, 'L2PHASE': 1,
            'L2FADE': 12, 'E2FLT': 30, 'E2': 18, 'E3': 6, 'E4': 3, 'E6': 9, 'SUB LEVEL': 7, 'CYCLE RISE': 8, 'CYCLE FALL': 110, 'CYCLE CURVE': -25,
            'AMPENV SATN': 5, 'FILTERENV MRES': -3, 'CYCLE PAN': 13, 'LFO2 MMODE': -3,
         }),
    dict(engine='WAVE', idx=9, name='SYNTH BRASS', cat='KEYS', bseed=30305,  # #93
         p={
            'ATK': 44, 'DEC': 83, 'SUS': 114, 'REL': 64, 'LRATE': 27, 'LD_FLT': 4, 'KTRK': 27, 'CHOR': 32, 'REV': 25, 'FREEA': 44, 'FREEB': 11,
            'E2ATK': 56, 'E2DEC': 83, 'E2SUS': 64, 'E2REL': 69, 'L2RATE': 87, 'L2FADE': 100, 'TRIM': 111, 'E2FLT': 24, 'E2SHP': 32, 'L2PIT': 1,
            'E1': 6, 'E2': 30, 'E6': 8, 'MOD1': ('AT', 'CUT', 17), 'MOD2': ('VEL', 'SHP', 10), 'MOD3': ('ENV2', 'E6', 11),
            'MOD4': ('CYCLE', 'SHP', 11), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 91, 'CYCLE FALL': 127,
         },
         bend={
            'ATK': 45, 'DEC': 81, 'SUS': 125, 'REL': 76, 'ED_SHP': -13, 'LRATE': 35, 'LPHASE': 11, 'LFADE': 10, 'LD_FLT': -3, 'LD_SHP': 8,
            'LD_AMP': 9, 'KTRK': 39, 'DIST': 12, 'CHOR': 42, 'DLY': 11, 'REV': 28, 'DETUNE': 49, 'FREEB': 3, 'FREEC': 4, 'M1AMT': 22, 'M2AMT': -1,
            'M3AMT': 16, 'M4AMT': 8, 'M5AMT': 35, 'E2ATK': 66, 'E2DEC': 76, 'E2SUS': 57, 'E2REL': 60, 'L2RATE': 96, 'L2PHASE': 9, 'L2FADE': 88,
            'E2FLT': 20, 'E2': 32, 'E3': 6, 'E4': 8, 'E6': 4, 'SUB LEVEL': 4, 'CYCLE RISE': 81, 'CYCLE FALL': 119, 'CYCLE CURVE': -4,
            'AMPENV SATN': 3, 'FILTERENV MRES': 3, 'CYCLE PAN': 1, 'LFO2 MMODE': 13,
         }),
    dict(engine='WAVE', idx=5, name='WAVE CHIMES', cat='KEYS', bseed=28858,  # #94
         p={
            'ATK': 0, 'DEC': 109, 'SUS': 0, 'REL': 106, 'ED_SHP': 61, 'KTRK': 27, 'CHOR': 15, 'REV': 64, 'FREEA': 70, 'FREEB': 7, 'E2ATK': 0,
            'E2DEC': 100, 'E2SUS': 0, 'E2REL': 88, 'L2RATE': 27, 'TRIM': 105, 'E2FLT': 11, 'L2FLT': 5, 'E1': 12, 'E2': 30, 'MOD1': ('AT', 'SHP', 11),
            'MOD2': ('VEL', 'SHP', 13), 'MOD3': ('AENV', 'E3', 17), 'MOD4': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1,
            'CYCLE RISE': 0, 'CYCLE FALL': 95,
         },
         bend={
            'ATK': 13, 'DEC': 111, 'SUS': 1, 'REL': 99, 'ED_SHP': 48, 'LRATE': 71, 'LPHASE': 13, 'LFADE': 5, 'LD_FLT': 6, 'LD_SHP': 4, 'LD_AMP': 1,
            'KTRK': 37, 'DIST': 13, 'CHOR': 16, 'DLY': 13, 'REV': 62, 'DETUNE': 47, 'FREEA': 65, 'FREEB': 14, 'FREEC': 5, 'M1AMT': 22, 'M2AMT': 22,
            'M3AMT': 25, 'M4AMT': 6, 'M5AMT': 14, 'E2ATK': 9, 'E2DEC': 93, 'E2SUS': 3, 'E2REL': 85, 'L2RATE': 14, 'L2PHASE': 10, 'L2FADE': 5,
            'E2FLT': 19, 'E2': 34, 'E3': 5, 'E4': 1, 'E6': 4, 'SUB LEVEL': 4, 'CYCLE RISE': 4, 'CYCLE FALL': 88, 'CYCLE CURVE': -7,
            'AMPENV SATN': -1, 'FILTERENV MRES': 13, 'CYCLE PAN': 2, 'LFO2 MMODE': -10,
         }),
    dict(engine='PHASE', idx=6, name='DEEP KICK', cat='PERC', bseed=34356,  # #95
         p={
            'ATK': 0, 'DEC': 84, 'SUS': 0, 'REL': 66, 'ED_PIT': 9, 'KTRK': 0, 'TRANS': -12, 'E2ATK': 0, 'E2DEC': 56, 'E2SUS': 0, 'E2REL': 41,
            'EQHG': -2, 'TRIM': 101, 'E3': 0, 'E4': 56, 'MOD1': ('AT', 'SHP', 13), 'MOD2': ('ENV2', 'PITCH', 63), 'MOD3': ('ENV2', 'PITCH', 20),
            'MOD4': ('EXPR', 'PITCH', -64), 'LTRIG': 1, 'L2TRIG': 1,
         },
         bend={
            'ATK': 9, 'DEC': 87, 'SUS': 2, 'REL': 79, 'ED_SHP': 10, 'LRATE': 69, 'LPHASE': 12, 'LFADE': 11, 'LD_FLT': 6, 'LD_SHP': -9, 'LD_AMP': 4,
            'KTRK': 3, 'DIST': 1, 'CHOR': 7, 'DLY': 1, 'REV': 5, 'DETUNE': 39, 'FREEA': 78, 'FREEB': 5, 'FREEC': 1, 'M1AMT': 10, 'E2ATK': 10,
            'E2DEC': 63, 'E2SUS': 2, 'E2REL': 54, 'L2RATE': 71, 'L2PHASE': 5, 'L2FADE': 13, 'E2FLT': 5, 'E3': 12, 'E4': 51, 'E5': 1, 'SUB LEVEL': 9,
            'CYCLE RISE': 1, 'CYCLE FALL': 72, 'CYCLE CURVE': -13, 'AMPENV SATN': -3, 'FILTERENV MRES': 3, 'CYCLE PAN': -6, 'LFO2 MMODE': -8,
         }),
    dict(engine='PHASE', idx=10, name='HARD KICK', cat='PERC', bseed=14853,  # #96
         p={
            'ATK': 0, 'DEC': 80, 'SUS': 0, 'REL': 64, 'KTRK': 0, 'TRANS': -12, 'DIST': 48, 'E2ATK': 0, 'E2DEC': 60, 'E2SUS': 0, 'E2REL': 41,
            'ITYPE': 2, 'IA': 127, 'IB': 90, 'IC': 90, 'TRIM': 113, 'E3': 0, 'E4': 14, 'MOD1': ('ENV2', 'PITCH', 63), 'MOD2': ('ENV2', 'PITCH', 63),
            'MOD3': ('EXPR', 'PITCH', -64), 'MOD4': ('EXPR', 'PITCH', -64), 'LTRIG': 1, 'L2TRIG': 1,
         },
         bend={
            'ATK': 1, 'DEC': 83, 'SUS': 2, 'REL': 56, 'ED_SHP': -9, 'LRATE': 73, 'LPHASE': 3, 'LFADE': 11, 'LD_FLT': -10, 'LD_SHP': 6, 'LD_AMP': 10,
            'KTRK': -2, 'DIST': 50, 'CHOR': 6, 'DLY': 7, 'REV': 6, 'DETUNE': 32, 'FREEA': 73, 'FREEB': 6, 'FREEC': 7, 'E2ATK': 9, 'E2DEC': 58,
            'E2SUS': 10, 'E2REL': 30, 'L2RATE': 50, 'L2PHASE': 11, 'L2FADE': 6, 'IA': 126, 'IB': 85, 'IC': 85, 'IMIX': 126, 'E2FLT': 5, 'E3': 4,
            'E4': 9, 'E5': 6, 'SUB LEVEL': 8, 'CYCLE RISE': 12, 'CYCLE FALL': 80, 'CYCLE CURVE': 7, 'AMPENV SATN': -3, 'FILTERENV MRES': 13,
            'CYCLE PAN': -8, 'LFO2 MMODE': -13,
         }),
    dict(engine='TRIO', idx=7, name='FIELD CLANK', cat='PERC', bseed=50175,  # #97
         p={
            'ATK': 0, 'DEC': 79, 'SUS': 0, 'REL': 66, 'RES2': 27, 'KTRK': 0, 'REV': 14, 'TWTYPE': 2, 'FREEA': 70, 'FREEB': 11, 'E2ATK': 0, 'E2DEC': 47,
            'E2SUS': 0, 'E2REL': 41, 'L2RATE': 27, 'L2WAVE': 1, 'CUT2': 59, 'TRIM': 124, 'E1': 1, 'E2': 6, 'E3': 0, 'E4': 0, 'MOD1': ('AT', 'E4', 17),
            'MOD2': ('CYCLE', 'CUT2', 11), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 12, 'TWIN LFO2': 4, 'CYCLE MODE': 1, 'CYCLE RISE': 0,
            'CYCLE FALL': 80,
         },
         bend={
            'ATK': 5, 'DEC': 82, 'SUS': 2, 'REL': 79, 'ED_SHP': -2, 'LRATE': 67, 'LPHASE': 3, 'LFADE': 6, 'LD_FLT': 3, 'LD_SHP': 4, 'LD_AMP': 9,
            'RES2': 47, 'DRV2': 6, 'KTRK': 12, 'DIST': 10, 'CHOR': 3, 'DLY': 3, 'REV': 3, 'DETUNE': 42, 'FREEA': 77, 'FREEB': 6, 'FREEC': 6,
            'M1AMT': 19, 'M2AMT': 8, 'E2ATK': 4, 'E2DEC': 34, 'E2SUS': 4, 'E2REL': 54, 'L2RATE': 30, 'L2PHASE': 2, 'L2FADE': 12, 'BLEND': 2,
            'E2FLT': -11, 'E4': 4, 'E8': 56, 'TWIN KTRK2': -6, 'TWIN ENV2': 18, 'TWIN LFO1': -12, 'TWIN LFO2': -9, 'SUB LEVEL': 8, 'CYCLE RISE': 6,
            'CYCLE FALL': 84, 'CYCLE CURVE': 4, 'AMPENV SATN': 13, 'FILTERENV FBLND': 5, 'FILTERENV FDRV': 10, 'FILTERENV MRES': -6, 'CYCLE TCUT': 9,
            'CYCLE PAN': -13, 'LFO2 TRES': 6, 'LFO2 MMODE': -3, 'LFO2 FBLND': -10,
         }),
    dict(engine='PHYS', idx=8, name='FLOOR THUD', cat='PERC', bseed=11677,  # #98
         p={
            'ATK': 0, 'SUS': 127, 'REL': 71, 'KTRK': 0, 'REV': 22, 'E1': 2, 'E2': 0, 'E3': 52, 'E4': 71, 'E5': 30, 'E6': 100, 'E7': 17, 'E8': 27,
            'MOD1': ('AT', 'E7', 11), 'LTRIG': 1, 'L2TRIG': 1,
         },
         bend={
            'ATK': 2, 'DEC': 72, 'SUS': 116, 'REL': 64, 'ED_SHP': 6, 'LRATE': 56, 'LPHASE': 12, 'LFADE': 11, 'LD_FLT': -5, 'LD_SHP': 7, 'LD_AMP': 3,
            'KTRK': 5, 'DIST': 6, 'CHOR': 9, 'DLY': 6, 'REV': 17, 'DETUNE': 53, 'FREEA': 78, 'FREEB': 4, 'FREEC': 9, 'M1AMT': 24, 'E2ATK': 18,
            'E2DEC': 74, 'E2SUS': 79, 'E2REL': 54, 'L2RATE': 47, 'L2PHASE': 11, 'L2FADE': 7, 'E2FLT': 4, 'E2': 13, 'E3': 41, 'E4': 83, 'E5': 21,
            'E6': 106, 'E7': 9, 'E8': 29, 'SUB LEVEL': 4, 'CYCLE RISE': 2, 'CYCLE FALL': 76, 'CYCLE CURVE': -4, 'AMPENV SATN': 1,
            'FILTERENV MRES': -13, 'CYCLE PAN': -1, 'LFO2 MMODE': -10,
         }),
    dict(engine='NOISE', idx=6, name='OPEN SIZZLE', cat='PERC', bseed=25614,  # #99
         p={
            'ATK': 0, 'DEC': 91, 'SUS': 0, 'REL': 66, 'EQLG': -6, 'RES2': 17, 'KTRK': 0, 'REV': 22, 'TWTYPE': 2, 'FREEA': 56, 'FREEB': 21,
            'FREEC': 127, 'E2ATK': 0, 'E2DEC': 76, 'E2SUS': 127, 'L2RATE': 29, 'L2WAVE': 1, 'CUT2': 71, 'TRIM': 109, 'E2': 0, 'MOD1': ('AT', 'E8', 11),
            'MOD2': ('RAND', 'CUT2', 5), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO2': 3,
         },
         bend={
            'ATK': 2, 'DEC': 94, 'SUS': 2, 'REL': 69, 'ED_SHP': -5, 'LRATE': 73, 'LPHASE': 4, 'LFADE': 6, 'LD_FLT': -6, 'LD_SHP': 4, 'LD_AMP': 3,
            'RES2': 6, 'DRV2': 1, 'KTRK': -10, 'DIST': 10, 'CHOR': 10, 'DLY': 7, 'REV': 18, 'DETUNE': 42, 'FREEA': 55, 'FREEB': 9, 'FREEC': 125,
            'M1AMT': 16, 'M2AMT': 16, 'E2ATK': 5, 'E2DEC': 70, 'E2SUS': 118, 'E2REL': 47, 'L2RATE': 40, 'L2PHASE': 7, 'L2FADE': 6, 'BLEND': 10,
            'E2FLT': 3, 'E2': 5, 'E6': 10, 'E7': 2, 'E8': 9, 'TWIN KTRK2': -10, 'TWIN ENV2': -7, 'TWIN LFO1': -1, 'TWIN LFO2': -1, 'SUB LEVEL': 1,
            'CYCLE RISE': 12, 'CYCLE FALL': 72, 'CYCLE CURVE': 5, 'AMPENV SATN': -13, 'FILTERENV FBLND': 12, 'FILTERENV FDRV': -2,
            'FILTERENV MRES': -9, 'CYCLE TCUT': -12, 'CYCLE PAN': 7, 'LFO2 TRES': -3, 'LFO2 MMODE': -13, 'LFO2 FBLND': 4,
         }),
    dict(engine='NOISE', idx=7, name='STATIC SNARE', cat='PERC', bseed=58051,  # #100
         p={
            'ATK': 0, 'DEC': 71, 'SUS': 0, 'REL': 56, 'RES2': 41, 'KTRK': 0, 'DIST': 31, 'REV': 23, 'TWTYPE': 2, 'FREEA': 39, 'FREEB': 73, 'FREEC': 36,
            'FREED': 2, 'E2ATK': 0, 'E2DEC': 56, 'E2SUS': 0, 'E2REL': 51, 'L2RATE': 18, 'L2WAVE': 1, 'BLEND': -17, 'CUT2': 57, 'TRIM': 122,
            'E2FLT': 29, 'E2': 0, 'MOD1': ('AT', 'SAT', 17), 'MOD2': ('RAND', 'CUT2', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN ROUTE': 1, 'TWIN LINK': 0,
            'TWIN LFO2': 3,
         },
         bend={
            'ATK': 9, 'DEC': 74, 'SUS': 2, 'REL': 55, 'ED_SHP': -9, 'LRATE': 67, 'LPHASE': 2, 'LFADE': 8, 'LD_FLT': -6, 'LD_SHP': -9, 'LD_AMP': 4,
            'RES2': 43, 'DRV2': 2, 'KTRK': -1, 'DIST': 22, 'CHOR': 13, 'DLY': 13, 'REV': 25, 'DETUNE': 53, 'FREEB': 64, 'M1AMT': 27, 'M2AMT': 10,
            'E2ATK': 13, 'E2DEC': 68, 'E2SUS': 3, 'E2REL': 38, 'L2RATE': 6, 'L2PHASE': 8, 'L2FADE': 7, 'BLEND': -16, 'CUT2': 52, 'E2FLT': 36,
            'E2': 9, 'E6': 12, 'E7': 1, 'E8': 5, 'TWIN KTRK2': 7, 'TWIN ENV2': 12, 'TWIN LFO1': 3, 'TWIN LFO2': 13, 'SUB LEVEL': 1, 'CYCLE RISE': 20,
            'CYCLE FALL': 76, 'CYCLE CURVE': 8, 'AMPENV SATN': -10, 'FILTERENV FBLND': -12, 'FILTERENV FDRV': 9, 'FILTERENV MRES': 10,
            'CYCLE TCUT': 12, 'CYCLE PAN': 12, 'LFO2 TRES': -13, 'LFO2 MMODE': 7, 'LFO2 FBLND': 8,
         }),
    dict(engine='NOISE', idx=5, name='TIZZ HAT', cat='PERC', bseed=16914,  # #101
         p={
            'ATK': 0, 'DEC': 52, 'SUS': 0, 'REL': 47, 'EQLG': -6, 'RES2': 21, 'KTRK': 0, 'REV': 13, 'TWTYPE': 2, 'FREEA': 61, 'FREEB': 27,
            'FREEC': 127, 'E2ATK': 0, 'E2DEC': 51, 'E2SUS': 0, 'E2REL': 41, 'L2RATE': 32, 'L2WAVE': 1, 'CUT2': 74, 'TRIM': 117, 'E2FLT': 6, 'E2': 0,
            'MOD1': ('AT', 'REV', 17), 'MOD2': ('RAND', 'CUT2', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO2': 3,
         },
         bend={
            'ATK': 1, 'DEC': 53, 'SUS': 2, 'REL': 59, 'ED_SHP': -12, 'LRATE': 68, 'LPHASE': 11, 'LFADE': 8, 'LD_FLT': 11, 'LD_SHP': -2, 'LD_AMP': 6,
            'RES2': 11, 'DRV2': 7, 'KTRK': -7, 'DIST': 11, 'CHOR': 2, 'DLY': 9, 'REV': 16, 'DETUNE': 51, 'FREEA': 53, 'FREEB': 37, 'FREEC': 122,
            'M1AMT': 18, 'M2AMT': 4, 'E2ATK': 11, 'E2DEC': 38, 'E2SUS': 10, 'E2REL': 42, 'L2RATE': 45, 'L2PHASE': 5, 'L2FADE': 5, 'BLEND': -12,
            'CUT2': 63, 'E2FLT': -5, 'E2': 2, 'E6': 12, 'E7': 9, 'E8': 12, 'TWIN KTRK2': -6, 'TWIN ENV2': -2, 'TWIN LFO1': 5, 'TWIN LFO2': -4,
            'SUB LEVEL': 5, 'CYCLE RISE': 5, 'CYCLE FALL': 66, 'CYCLE CURVE': -12, 'AMPENV SATN': 6, 'FILTERENV FBLND': -3, 'FILTERENV FDRV': -13,
            'FILTERENV MRES': 4, 'CYCLE TCUT': -10, 'CYCLE PAN': -10, 'LFO2 TRES': -7, 'LFO2 MMODE': -10, 'LFO2 FBLND': 7,
         }),
    dict(engine='ANALOG', idx=14, name='STORM FRONT', cat='FX', bseed=36047, bamt=14,  # #102
         p={
            'ATK': 97, 'SUS': 127, 'REL': 79, 'LRATE': 23, 'LWAVE': 1, 'LD_FLT': 2, 'RES2': 41, 'KTRK': 0, 'DIST': 7, 'DLY': 20, 'REV': 25,
            'TWTYPE': 2, 'FREEA': 37, 'FREEB': 31, 'E2ATK': 111, 'E2DEC': 0, 'E2SUS': 127, 'E2REL': 79, 'ITYPE': 1, 'IA': 40, 'IB': 127, 'IC': 112,
            'BLEND': -17, 'CUT2': 17, 'TRIM': 84, 'E2FLT': 31, 'E1': 3, 'E3': 0, 'E4': 127, 'MOD1': ('AT', 'FRES', 27), 'MOD2': ('ENV2', 'REV', 32),
            'MOD3': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN ROUTE': 1, 'CYCLE RISE': 103, 'CYCLE FALL': 107,
         },
         bend={
            'ATK': 90, 'DEC': 72, 'SUS': 123, 'REL': 92, 'ED_SHP': 5, 'LRATE': 36, 'LPHASE': 14, 'LFADE': 17, 'LD_FLT': 6, 'LD_SHP': 11,
            'LD_AMP': 10, 'RES2': 64, 'DRV2': 6, 'KTRK': 9, 'DIST': 15, 'CHOR': 14, 'DLY': 5, 'REV': 18, 'DETUNE': 37, 'FREEA': 45, 'FREEB': 35,
            'FREEC': 11, 'M1AMT': 44, 'M2AMT': 44, 'M3AMT': 5, 'E2ATK': 95, 'E2DEC': 2, 'E2SUS': 115, 'E2REL': 70, 'L2RATE': 51, 'L2PHASE': 4,
            'L2FADE': 15, 'IA': 49, 'IB': 121, 'IC': 125, 'IMIX': 125, 'BLEND': -25, 'CUT2': 7, 'E2FLT': 47, 'E2': 11, 'E3': 12, 'E4': 119, 'E7': 16,
            'TWIN KTRK2': -15, 'TWIN ENV2': 5, 'TWIN LFO1': -12, 'TWIN LFO2': 15, 'SUB LEVEL': 5, 'CYCLE RISE': 96, 'CYCLE FALL': 120,
            'CYCLE CURVE': 7, 'AMPENV SATN': 16, 'FILTERENV FBLND': 5, 'FILTERENV FDRV': -9, 'FILTERENV MRES': -17, 'CYCLE TCUT': 6, 'CYCLE PAN': 16,
            'LFO2 TRES': 13, 'LFO2 MMODE': -17, 'LFO2 FBLND': -1,
         }),
    dict(engine='FM6', idx=7, name='ALIEN CHIMES', cat='FX', bseed=40225,  # #103
         p={
            'EQLG': -6, 'AMODE': 4, 'ARATE': 1, 'AOCT': 2, 'AGATE': 44, 'DLY': 64, 'REV': 76, 'TWTYPE': 4, 'FREEA': 67, 'FREEB': 21, 'E2ATK': 0,
            'E2DEC': 94, 'E2SUS': 0, 'E2REL': 86, 'L2RATE': 26, 'L2WAVE': 1, 'CUT2': 47, 'TRIM': 112, 'L2FLT': 4, 'E1': 4, 'E2': 2, 'E4': 5, 'E7': 70,
            'E8': 1, 'MOD1': ('AT', 'E3', 9), 'MOD2': ('CYCLE', 'CUT2', 9), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'CYCLE RISE': 99,
            'CYCLE FALL': 103,
         },
         bend={
            'ATK': 16, 'DEC': 87, 'SUS': 2, 'REL': 92, 'ED_SHP': -38, 'LRATE': 58, 'LPHASE': 26, 'LFADE': 2, 'LD_FLT': -1, 'LD_SHP': 19, 'LD_AMP': 9,
            'DRV2': 32, 'KTRK': 38, 'DIST': 39, 'CHOR': 2, 'DLY': 53, 'REV': 112, 'DETUNE': 58, 'FREEA': 73, 'FREEB': 54, 'FREEC': 6, 'M1AMT': -26,
            'M2AMT': 4, 'M3AMT': -9, 'E2ATK': 27, 'E2DEC': 69, 'E2SUS': 3, 'E2REL': 72, 'L2RATE': 49, 'L2PHASE': 34, 'L2FADE': 22, 'BLEND': 30,
            'CUT2': 56, 'E2FLT': 21, 'E2': -2, 'E3': -1, 'E5': -26, 'E6': 1, 'E7': 45, 'TWIN KTRK2': -15, 'TWIN ENV2': -9, 'TWIN LFO1': 28,
            'TWIN LFO2': -30, 'SUB LEVEL': 16, 'CYCLE RISE': 72, 'CYCLE FALL': 88, 'CYCLE CURVE': 27, 'AMPENV SATN': 38, 'FILTERENV FBLND': 7,
            'FILTERENV FDRV': -12, 'FILTERENV MRES': 28, 'CYCLE TCUT': -2, 'CYCLE PAN': -12, 'LFO2 TRES': -2, 'LFO2 MMODE': -8, 'LFO2 FBLND': -29,
         },
         bfm=[
            93, 53, 25, 32, 94, 58, 9, 9, 63, 77, 80, 36, 25, 88, 60, 2, 7, 76, 84, 51, 32, 66, 98, 61, 1, 0, 71, 82, 36, 36, 53, 94, 61, 11, 7, 78,
            95, 15, 16, 56, 96, 59, 4, 1, 81, 92, 39, 23, 61, 98, 81, 1, 0, 97, 99, 99, 99, 99, 50, 50, 50, 59, 25, 0, 23,
         ]),
    dict(engine='PHASE', idx=5, name='CRYSTAL CAVE', cat='FX', bseed=36122,  # #104
         p={
            'ATK': 0, 'DEC': 101, 'SUS': 32, 'REL': 86, 'EQLG': -6, 'AMODE': 4, 'ARATE': 1, 'AOCT': 3, 'AGATE': 44, 'APROB': 108, 'RES2': 41,
            'DLY': 64, 'REV': 81, 'TWTYPE': 2, 'FREEA': 66, 'FREEB': 21, 'E2ATK': 0, 'E2DEC': 88, 'E2SUS': 0, 'E2REL': 79, 'L2RATE': 26, 'L2WAVE': 1,
            'BLEND': -17, 'CUT2': 58, 'TRIM': 127, 'L2SHP': 4, 'E1': 6, 'E2': 8, 'E3': 72, 'E4': 50, 'E5': 53, 'E6': 1, 'MOD1': ('AT', 'FBLND', 27),
            'MOD2': ('CYCLE', 'CUT2', 9), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN ROUTE': 1, 'TWIN LINK': 0, 'TWIN LFO2': 5, 'CYCLE RISE': 105,
            'CYCLE FALL': 110,
         },
         bend={
            'ATK': 39, 'DEC': 104, 'SUS': 42, 'REL': 53, 'ED_SHP': 18, 'LRATE': 29, 'LPHASE': 27, 'LFADE': 17, 'LD_FLT': -23, 'LD_SHP': -36,
            'LD_AMP': 9, 'RES2': 40, 'DRV2': 32, 'KTRK': 35, 'DIST': 8, 'CHOR': 11, 'DLY': 43, 'REV': 63, 'DETUNE': 43, 'FREEA': 56, 'FREEB': 46,
            'FREEC': 14, 'M1AMT': -4, 'M2AMT': 28, 'M3AMT': 42, 'E2ATK': 18, 'E2DEC': 123, 'E2SUS': 37, 'E2REL': 56, 'L2RATE': 4, 'L2PHASE': 25,
            'L2FADE': 25, 'BLEND': 0, 'CUT2': 36, 'E2FLT': 3, 'E3': 94, 'E4': 73, 'E5': 45, 'TWIN KTRK2': 30, 'TWIN ENV2': 34, 'TWIN LFO1': -28,
            'TWIN LFO2': -15, 'SUB LEVEL': 28, 'CYCLE RISE': 87, 'CYCLE FALL': 81, 'CYCLE CURVE': -17, 'AMPENV SATN': 4, 'FILTERENV FBLND': 3,
            'FILTERENV FDRV': 35, 'FILTERENV MRES': 27, 'CYCLE TCUT': -15, 'CYCLE PAN': 20, 'LFO2 TRES': -8, 'LFO2 MMODE': 5, 'LFO2 FBLND': -21,
         }),
    dict(engine='PHASE', idx=7, name='SONAR PING', cat='FX', bseed=64729,  # #105
         p={
            'ATK': 0, 'DEC': 90, 'SUS': 52, 'REL': 103, 'EQLG': -4, 'LRATE': 18, 'LWAVE': 1, 'LD_FLT': 3, 'DLY': 60, 'REV': 77, 'FREEA': 49,
            'FREEB': 41, 'E2ATK': 0, 'E2DEC': 76, 'E2SUS': 0, 'E2REL': 66, 'TRIM': 101, 'E2FLT': 21, 'E3': 0, 'E4': 11, 'MOD1': ('AT', 'FRES', 27),
            'MOD2': ('CYCLE', 'SHP', 7), 'LTRIG': 1, 'L2TRIG': 1, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 101,
         },
         bend={
            'ATK': 1, 'DEC': 81, 'SUS': 66, 'REL': 73, 'ED_SHP': -39, 'LRATE': 49, 'LPHASE': 12, 'LFADE': 12, 'LD_FLT': -1, 'LD_SHP': -17,
            'LD_AMP': 11, 'KTRK': 8, 'DIST': 17, 'CHOR': 16, 'DLY': 40, 'REV': 112, 'DETUNE': 46, 'FREEA': 53, 'FREEB': 53, 'FREEC': 13, 'M1AMT': 24,
            'M2AMT': 12, 'E2ATK': 16, 'E2DEC': 37, 'E2SUS': 6, 'E2REL': 35, 'L2RATE': 64, 'L2PHASE': 28, 'L2FADE': 16, 'E2FLT': 14, 'E3': 38,
            'E4': 50, 'E5': 7, 'SUB LEVEL': 29, 'CYCLE RISE': 3, 'CYCLE FALL': 118, 'CYCLE CURVE': 6, 'AMPENV SATN': 14, 'FILTERENV MRES': 35,
            'CYCLE PAN': 37, 'LFO2 MMODE': -14,
         }),
    dict(engine='LOFI', idx=4, name='DIVE BOMB', cat='FX', bseed=27733,  # #106
         p={
            'ATK': 47, 'DEC': 127, 'SUS': 127, 'REL': 79, 'DIST': 27, 'DLY': 57, 'REV': 57, 'TWTYPE': 4, 'FREEA': 54, 'FREEB': 41, 'E2ATK': 0,
            'E2DEC': 92, 'E2SUS': 64, 'E2REL': 73, 'L2RATE': 96, 'L2WAVE': 2, 'CUT2': 50, 'TRIM': 94, 'E2FLT': 17, 'L2SHP': 19, 'E3': 40, 'E5': 5,
            'E8': 100, 'MOD1': ('AT', 'E8', -21), 'MOD2': ('CYCLE', 'PITCH', 63), 'MOD3': ('CYCLE', 'CUT2', -13), 'LTRIG': 1, 'L2TRIG': 1,
            'TWIN LINK': 0, 'CYCLE MODE': 1, 'CYCLE RISE': 0, 'CYCLE FALL': 103,
         },
         bend={
            'ATK': 36, 'DEC': 107, 'SUS': 117, 'REL': 118, 'ED_SHP': 32, 'LRATE': 28, 'LPHASE': 32, 'LFADE': 36, 'LD_FLT': -23, 'LD_SHP': -28,
            'LD_AMP': 10, 'DRV2': 32, 'KTRK': 47, 'DIST': 59, 'CHOR': 19, 'DLY': 89, 'REV': 70, 'DETUNE': 23, 'FREEA': 45, 'FREEB': 3, 'FREEC': 5,
            'M1AMT': -57, 'M3AMT': 2, 'M4AMT': 50, 'E2ATK': 1, 'E2DEC': 84, 'E2SUS': 70, 'E2REL': 38, 'L2RATE': 60, 'L2PHASE': 13, 'L2FADE': 36,
            'BLEND': 4, 'CUT2': 69, 'E2FLT': -9, 'E3': 60, 'E4': 11, 'E5': 9, 'E6': 20, 'E8': 118, 'TWIN KTRK2': -22, 'TWIN ENV2': -38,
            'TWIN LFO1': 3, 'TWIN LFO2': -4, 'SUB LEVEL': 36, 'CYCLE RISE': 14, 'CYCLE FALL': 111, 'CYCLE CURVE': -17, 'AMPENV SATN': 1,
            'FILTERENV FBLND': 32, 'FILTERENV FDRV': 1, 'FILTERENV MRES': -12, 'CYCLE TCUT': -2, 'CYCLE PAN': -22, 'LFO2 TRES': 32, 'LFO2 MMODE': 27,
            'LFO2 FBLND': 22,
         }),
    dict(engine='VOICE', idx=3, name='GHOST VOICES', cat='FX', bseed=15764,  # #107
         p={
            'ATK': 98, 'DEC': 101, 'SUS': 127, 'REL': 103, 'LRATE': 18, 'LWAVE': 1, 'CHOR': 38, 'DLY': 30, 'REV': 57, 'TWTYPE': 4, 'FREEA': 58,
            'FREEB': 11, 'E2ATK': 101, 'E2DEC': 108, 'E2SUS': 89, 'E2REL': 101, 'L2RATE': 13, 'L2WAVE': 1, 'ITYPE': 1, 'IA': 70, 'IB': 127, 'IC': 112,
            'CUT2': 52, 'TRIM': 82, 'E2FLT': 11, 'E2': 100, 'E3': 70, 'E4': 3, 'E5': 52, 'E6': 118, 'E7': 50, 'E8': 40, 'MOD1': ('AT', 'E5', 17),
            'MOD2': ('CYCLE', 'SHP', 9), 'MOD3': ('LFO2', 'E1', 6), 'MOD4': ('LFO2', 'E2', 6), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO1': 9,
            'CYCLE RISE': 108, 'CYCLE FALL': 111,
         },
         bend={
            'ATK': 62, 'DEC': 127, 'SUS': 123, 'REL': 64, 'ED_SHP': -37, 'LRATE': 52, 'LPHASE': 21, 'LFADE': 10, 'LD_FLT': 30, 'LD_SHP': -33,
            'LD_AMP': 3, 'RES2': 15, 'DRV2': 13, 'KTRK': 51, 'DIST': 13, 'CHOR': 8, 'DLY': 39, 'REV': 48, 'DETUNE': 55, 'FREEA': 50, 'FREEB': 40,
            'FREEC': 20, 'M1AMT': 56, 'M2AMT': 27, 'M3AMT': -4, 'M4AMT': 14, 'E2ATK': 93, 'E2DEC': 81, 'E2SUS': 77, 'E2REL': 86, 'L2RATE': 42,
            'L2PHASE': 16, 'L2FADE': 5, 'IA': 71, 'IB': 122, 'IC': 109, 'IMIX': 124, 'BLEND': 35, 'CUT2': 11, 'E2FLT': 44, 'E1': 1, 'E2': 113,
            'E3': 79, 'E4': 5, 'E5': 77, 'E6': 86, 'E7': 47, 'E8': 26, 'TWIN KTRK2': -22, 'TWIN ENV2': -36, 'TWIN LFO1': -22, 'TWIN LFO2': 20,
            'SUB LEVEL': 39, 'CYCLE RISE': 98, 'CYCLE FALL': 110, 'CYCLE CURVE': 35, 'AMPENV SATN': -38, 'FILTERENV FBLND': 17, 'FILTERENV FDRV': 31,
            'FILTERENV MRES': -9, 'CYCLE TCUT': 22, 'CYCLE PAN': 25, 'LFO2 TRES': 15, 'LFO2 MMODE': 7, 'LFO2 FBLND': 2,
         }),
    dict(engine='VOICE', idx=7, name='YO YA', cat='FX',  # #108
         p={
            'ATK': 34, 'SUS': 127, 'REL': 65, 'RES2': 7, 'CHOR': 23, 'DLY': 28, 'REV': 38, 'VOICE': 2, 'GLIDE': 24, 'GLMODE': 1, 'TWTYPE': 1,
            'FREEB': 40, 'E2ATK': 0, 'E2DEC': 79, 'E2SUS': 0, 'E2REL': 64, 'L2RATE': 89, 'L2WAVE': 1, 'L2FADE': 84, 'CUT2': -21, 'TRIM': 86,
            'L2PIT': 1, 'E1': 22, 'E2': 0, 'E3': 44, 'E4': 2, 'E5': 88, 'E6': 9, 'E7': 70, 'MOD1': ('AT', 'DEP2', 63), 'MOD2': ('CYCLE', 'CUT2', 7),
            'LTRIG': 1, 'L2TRIG': 1,
         },
         bend={
            'ATK': 2, 'DEC': 46, 'SUS': 114, 'REL': 99, 'ED_SHP': -26, 'LRATE': 90, 'LPHASE': 8, 'LFADE': 13, 'LD_FLT': -4, 'LD_SHP': -5,
            'LD_AMP': 1, 'RES2': 0, 'DRV2': 9, 'KTRK': 37, 'DIST': 33, 'CHOR': 49, 'DLY': 22, 'REV': 49, 'DETUNE': 44, 'FREEA': 62, 'FREEB': 10,
            'FREEC': 15, 'M1AMT': 54, 'M2AMT': 41, 'E2ATK': 10, 'E2DEC': 115, 'E2SUS': 23, 'E2REL': 81, 'L2RATE': 86, 'L2PHASE': 33, 'L2FADE': 75,
            'BLEND': 24, 'CUT2': 16, 'E2FLT': -1, 'E1': 50, 'E2': 3, 'E3': 24, 'E4': 5, 'E5': 64, 'E6': 5, 'E7': 77, 'E8': 32, 'TWIN KTRK2': 25,
            'TWIN ENV2': 32, 'TWIN LFO1': 22, 'TWIN LFO2': -29, 'SUB LEVEL': 19, 'CYCLE RISE': 1, 'CYCLE FALL': 52, 'CYCLE CURVE': 1,
            'AMPENV SATN': -31, 'FILTERENV FBLND': -37, 'FILTERENV FDRV': 20, 'FILTERENV MRES': 32, 'CYCLE TCUT': 8, 'CYCLE PAN': -2,
            'LFO2 TRES': -14, 'LFO2 MMODE': -19, 'LFO2 FBLND': 12,
         }),
    dict(engine='TRIO', idx=5, name='CIRCUIT BEND', cat='FX', bseed=34291,  # #109
         p={
            'ATK': 0, 'DEC': 74, 'SUS': 44, 'EQLG': -4, 'AMODE': 3, 'AOCT': 3, 'AGATE': 76, 'APROB': 108, 'RES2': 45, 'DRV2': 41, 'KTRK': 0,
            'DIST': 32, 'DLY': 57, 'REV': 44, 'TWTYPE': 1, 'FREEA': 72, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 65, 'E2SUS': 0, 'E2REL': 54, 'L2RATE': 34,
            'L2WAVE': 1, 'CUT2': 59, 'TRIM': 104, 'E1': 14, 'E2': 7, 'E3': 0, 'E4': 9, 'MOD1': ('AT', 'FDRV', 27), 'MOD2': ('CYCLE', 'CUT2', 11),
            'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 12, 'TWIN LFO2': 12, 'CYCLE RISE': 83, 'CYCLE FALL': 88,
         },
         bend={
            'ATK': 14, 'DEC': 82, 'SUS': 36, 'REL': 62, 'ED_SHP': 13, 'LRATE': 45, 'LPHASE': 12, 'LFADE': 5, 'LD_FLT': -14, 'LD_SHP': -31,
            'LD_AMP': 3, 'RES2': 84, 'DRV2': 57, 'KTRK': 32, 'DIST': 37, 'CHOR': 18, 'DLY': 53, 'REV': 34, 'DETUNE': 11, 'FREEA': 49, 'FREEB': 48,
            'FREEC': 10, 'M1AMT': 36, 'M2AMT': -12, 'E2ATK': 24, 'E2DEC': 62, 'E2SUS': 21, 'E2REL': 53, 'L2RATE': 8, 'L2PHASE': 38, 'L2FADE': 5,
            'BLEND': -23, 'CUT2': 45, 'E2FLT': 37, 'E4': 5, 'E8': 82, 'TWIN KTRK2': -26, 'TWIN ENV2': 1, 'TWIN LFO1': -24, 'TWIN LFO2': 3,
            'SUB LEVEL': 20, 'CYCLE RISE': 74, 'CYCLE FALL': 49, 'CYCLE CURVE': -6, 'AMPENV SATN': 34, 'FILTERENV FBLND': 25, 'FILTERENV FDRV': 5,
            'FILTERENV MRES': 26, 'CYCLE TCUT': -38, 'CYCLE PAN': 13, 'LFO2 TRES': 21, 'LFO2 MMODE': -30, 'LFO2 FBLND': 2,
         }),
    dict(engine='WHEEL', idx=5, name='LESLIE STORM', cat='FX', bseed=30012,  # #110
         p={
            'ATK': 95, 'SUS': 127, 'REL': 103, 'LRATE': 23, 'LWAVE': 1, 'DIST': 19, 'DLY': 23, 'REV': 42, 'TWTYPE': 4, 'FREEA': 56, 'FREEB': 21,
            'E2ATK': 98, 'E2DEC': 0, 'E2SUS': 127, 'E2REL': 92, 'L2RATE': 17, 'L2WAVE': 1, 'ITYPE': 1, 'IA': 40, 'IB': 127, 'IC': 112, 'CUT2': 51,
            'TRIM': 90, 'E2FLT': 17, 'E2SHP': 32, 'L2SHP': 19, 'E1': 15, 'E6': 0, 'E7': 90, 'E8': 2, 'MOD1': ('AT', 'E7', 11),
            'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO1': 9, 'CYCLE RISE': 109, 'CYCLE FALL': 112,
         },
         bend={
            'ATK': 93, 'DEC': 93, 'SUS': 124, 'REL': 71, 'ED_SHP': 33, 'LRATE': 53, 'LPHASE': 37, 'LFADE': 14, 'LD_FLT': 15, 'LD_SHP': 33,
            'LD_AMP': 6, 'DRV2': 29, 'KTRK': 18, 'DIST': 12, 'CHOR': 13, 'DLY': 6, 'REV': 11, 'DETUNE': 48, 'FREEA': 45, 'FREEB': 40, 'FREEC': 39,
            'M1AMT': -18, 'M2AMT': 1, 'M3AMT': 3, 'M4AMT': 20, 'E2ATK': 106, 'E2DEC': 26, 'E2SUS': 124, 'E2REL': 54, 'L2RATE': 42, 'L2PHASE': 36,
            'L2FADE': 5, 'IA': 36, 'IB': 122, 'IC': 126, 'IMIX': 93, 'BLEND': -24, 'CUT2': 58, 'E2FLT': 3, 'E3': -4, 'E4': 2, 'E6': 35, 'E7': 84,
            'TWIN KTRK2': 27, 'TWIN ENV2': 7, 'TWIN LFO1': 34, 'TWIN LFO2': -18, 'SUB LEVEL': 16, 'CYCLE RISE': 115, 'CYCLE FALL': 95,
            'CYCLE CURVE': 11, 'AMPENV SATN': -28, 'FILTERENV FBLND': 16, 'FILTERENV FDRV': -33, 'FILTERENV MRES': -19, 'CYCLE TCUT': -5,
            'CYCLE PAN': -20, 'LFO2 TRES': -6, 'LFO2 MMODE': 15, 'LFO2 FBLND': -27,
         }),
    dict(engine='WHEEL', idx=6, name='TAPE DRONE', cat='FX', bseed=63114,  # #111
         p={
            'ATK': 100, 'SUS': 127, 'REL': 105, 'LRATE': 17, 'LWAVE': 1, 'CHOR': 32, 'REV': 64, 'TWTYPE': 4, 'FREEA': 58, 'FREEB': 11, 'L2RATE': 21,
            'L2WAVE': 1, 'CHRD': 9, 'VOIC': 1, 'ITYPE': 9, 'IA': 50, 'IB': 71, 'IC': 110, 'CUT2': 52, 'TRIM': 102, 'L2FLT': 3, 'E1': 12, 'E6': 0,
            'E7': 20, 'E8': 0, 'MOD1': ('AT', 'E4', 17), 'MOD2': ('CYCLE', 'CUT', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO1': 11,
            'CYCLE RISE': 115, 'CYCLE FALL': 119,
         },
         bend={
            'ATK': 86, 'DEC': 107, 'SUS': 102, 'REL': 72, 'ED_SHP': -31, 'LRATE': 51, 'LPHASE': 12, 'LFADE': 14, 'LD_FLT': -17, 'LD_SHP': 30,
            'LD_AMP': 6, 'DRV2': 30, 'KTRK': 2, 'DIST': 8, 'CHOR': 46, 'DLY': 2, 'REV': 57, 'DETUNE': 31, 'FREEA': 45, 'FREEB': 14, 'FREEC': 4,
            'M1AMT': 1, 'M2AMT': -10, 'M3AMT': -21, 'E2ATK': 30, 'E2DEC': 91, 'E2SUS': 97, 'E2REL': 59, 'L2RATE': 2, 'L2PHASE': 23, 'L2FADE': 10,
            'IA': 17, 'IB': 73, 'IC': 91, 'IMIX': 92, 'BLEND': 24, 'CUT2': 29, 'E2FLT': 38, 'E3': 3, 'E4': -3, 'E6': 6, 'E7': 57, 'TWIN KTRK2': 31,
            'TWIN ENV2': -4, 'TWIN LFO1': 34, 'TWIN LFO2': -29, 'SUB LEVEL': 29, 'CYCLE RISE': 83, 'CYCLE FALL': 124, 'CYCLE CURVE': 22,
            'AMPENV SATN': 31, 'FILTERENV FBLND': 23, 'FILTERENV FDRV': 1, 'FILTERENV MRES': 32, 'CYCLE TCUT': -23, 'CYCLE PAN': 29, 'LFO2 TRES': -8,
            'LFO2 MMODE': 26, 'LFO2 FBLND': -6,
         }),
    dict(engine='PHYS', idx=6, name='RAIN CHIMES', cat='FX', bseed=23020,  # #112
         p={
            'ATK': 0, 'SUS': 127, 'REL': 86, 'EQLG': -6, 'AMODE': 4, 'ARATE': 1, 'AOCT': 3, 'AGATE': 38, 'APROB': 108, 'RES2': 41, 'DLY': 57,
            'REV': 76, 'TWTYPE': 2, 'FREEA': 64, 'FREEB': 17, 'E2ATK': 0, 'E2DEC': 94, 'E2SUS': 0, 'E2REL': 86, 'L2RATE': 31, 'L2WAVE': 1, 'CUT2': 61,
            'TRIM': 111, 'E2FLT': 9, 'E2': 70, 'E3': 100, 'E4': 120, 'E5': 12, 'E6': 100, 'E8': 6, 'MOD1': ('AT', 'E5', 21),
            'MOD2': ('CYCLE', 'CUT2', 9), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN ROUTE': 1, 'TWIN LINK': 0, 'TWIN LFO2': 5, 'CYCLE RISE': 103,
            'CYCLE FALL': 107,
         },
         bend={
            'ATK': 32, 'DEC': 93, 'SUS': 122, 'REL': 97, 'ED_SHP': -18, 'LRATE': 49, 'LPHASE': 18, 'LFADE': 10, 'LD_FLT': 21, 'LD_SHP': 21,
            'LD_AMP': 6, 'RES2': 0, 'DRV2': 31, 'KTRK': 0, 'DIST': 14, 'CHOR': 14, 'DLY': 20, 'REV': 69, 'DETUNE': 31, 'FREEA': 45, 'FREEB': 48,
            'FREEC': 10, 'M1AMT': 6, 'M2AMT': 12, 'E2ATK': 21, 'E2DEC': 123, 'E2SUS': 24, 'E2REL': 81, 'L2RATE': 12, 'L2PHASE': 2, 'L2FADE': 6,
            'BLEND': 2, 'E2FLT': 27, 'E2': 73, 'E3': 127, 'E4': 113, 'E5': 46, 'E6': 94, 'E7': 34, 'E8': 22, 'TWIN KTRK2': -21, 'TWIN ENV2': -35,
            'TWIN LFO1': -1, 'TWIN LFO2': 4, 'SUB LEVEL': 28, 'CYCLE RISE': 94, 'CYCLE FALL': 75, 'CYCLE CURVE': -6, 'AMPENV SATN': 1,
            'FILTERENV FBLND': -28, 'FILTERENV FDRV': -16, 'FILTERENV MRES': -10, 'CYCLE TCUT': -1, 'CYCLE PAN': -23, 'LFO2 TRES': -22,
            'LFO2 MMODE': -31, 'LFO2 FBLND': -20,
         }),
    dict(engine='NOISE', idx=4, name='MOOR WIND', cat='FX', bseed=31000, bamt=27,  # #113
         p={
            'ATK': 99, 'SUS': 127, 'REL': 110, 'EQLG': -4, 'LRATE': 23, 'RES2': 51, 'KTRK': 0, 'DLY': 11, 'REV': 53, 'TWTYPE': 2, 'FREEA': 31,
            'FREEB': 71, 'FREEC': 35, 'E2ATK': 105, 'E2DEC': 108, 'E2SUS': 89, 'E2REL': 101, 'L2RATE': 15, 'L2WAVE': 1, 'ITYPE': 6, 'IA': 18, 'IB': 90,
            'IC': 50, 'IMIX': 76, 'BLEND': -17, 'CUT2': 51, 'TRIM': 127, 'E2FLT': 5, 'L2FLT': 12, 'E2': 100, 'E7': 90, 'MOD1': ('AT', 'E2', 17),
            'MOD2': ('CYCLE', 'CUT2', 9), 'MOD3': ('LFO1', 'FRES', 6), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN ROUTE': 1, 'TWIN LINK': 0, 'TWIN LFO2': 9,
            'CYCLE RISE': 112, 'CYCLE FALL': 117,
         },
         bend={
            'ATK': 81, 'DEC': 97, 'SUS': 112, 'REL': 100, 'ED_SHP': -6, 'LRATE': 19, 'LPHASE': 23, 'LFADE': 22, 'LD_FLT': -6, 'LD_SHP': 8,
            'LD_AMP': 4, 'RES2': 3, 'DRV2': 21, 'KTRK': 32, 'DIST': 17, 'CHOR': 25, 'DLY': 36, 'REV': 69, 'DETUNE': 43, 'FREEA': 45, 'FREEB': 68,
            'FREEC': 14, 'M1AMT': 40, 'M2AMT': 33, 'M3AMT': 1, 'M4AMT': 15, 'E2ATK': 74, 'E2DEC': 83, 'E2SUS': 64, 'E2REL': 104, 'L2RATE': 41,
            'L2PHASE': 11, 'L2FADE': 27, 'IA': 37, 'IB': 100, 'IC': 62, 'IMIX': 57, 'BLEND': -50, 'CUT2': 36, 'E2FLT': -14, 'E2': 70, 'E6': 18,
            'E7': 108, 'E8': 16, 'TWIN KTRK2': -31, 'TWIN ENV2': 21, 'TWIN LFO1': -11, 'TWIN LFO2': 31, 'SUB LEVEL': 28, 'CYCLE RISE': 90,
            'CYCLE FALL': 91, 'CYCLE CURVE': 12, 'AMPENV SATN': 14, 'FILTERENV FBLND': 34, 'FILTERENV FDRV': -28, 'FILTERENV MRES': 1,
            'CYCLE TCUT': -18, 'CYCLE PAN': -20, 'LFO2 TRES': -29, 'LFO2 MMODE': 21, 'LFO2 FBLND': 29,
         }),
    dict(engine='NOISE', idx=10, name='REV CYMBAL', cat='FX', bseed=57441,  # #114
         p={
            'ATK': 97, 'SUS': 127, 'REL': 50, 'EQLG': -6, 'RES2': 17, 'KTRK': 8, 'REV': 47, 'TWTYPE': 3, 'FREEA': 47, 'FREEB': 41, 'FREEC': 17,
            'FREED': 2, 'E2ATK': 97, 'E2DEC': 0, 'E2SUS': 127, 'E2REL': 47, 'CUT2': 41, 'TRIM': 109, 'E2FLT': 25, 'E2': 0, 'MOD1': ('AT', 'E8', 17),
            'MOD2': ('CYCLE', 'CUT2', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'CYCLE RISE': 59, 'CYCLE FALL': 60,
         },
         bend={
            'ATK': 70, 'DEC': 90, 'SUS': 103, 'REL': 38, 'ED_SHP': -9, 'LRATE': 79, 'LPHASE': 35, 'LFADE': 28, 'LD_FLT': -6, 'LD_SHP': 39,
            'LD_AMP': 4, 'RES2': 11, 'DRV2': 22, 'KTRK': -2, 'DIST': 24, 'CHOR': 1, 'DLY': 26, 'REV': 76, 'DETUNE': 46, 'FREEA': 50, 'M1AMT': 45,
            'M2AMT': 4, 'E2ATK': 110, 'E2DEC': 5, 'E2SUS': 90, 'E2REL': 12, 'L2RATE': 42, 'L2PHASE': 35, 'L2FADE': 31, 'BLEND': -29, 'E2FLT': 18,
            'E2': 2, 'E6': 20, 'E7': 2, 'E8': 33, 'TWIN KTRK2': 24, 'TWIN ENV2': -6, 'TWIN LFO1': -26, 'TWIN LFO2': -16, 'SUB LEVEL': 3,
            'CYCLE RISE': 76, 'CYCLE FALL': 43, 'CYCLE CURVE': 7, 'AMPENV SATN': -29, 'FILTERENV FBLND': 13, 'FILTERENV FDRV': -10,
            'FILTERENV MRES': 13, 'CYCLE TCUT': 4, 'CYCLE PAN': -15, 'LFO2 TRES': -8, 'LFO2 MMODE': -6, 'LFO2 FBLND': -31,
         }),
    dict(engine='NOISE', idx=3, name='WHITE OUT', cat='FX', bseed=38918, bamt=12,  # #115
         p={
            'ATK': 94, 'SUS': 127, 'REL': 83, 'EQLG': -4, 'KTRK': 0, 'DLY': 20, 'REV': 30, 'FREEA': 21, 'FREEB': 51, 'FREEC': 20, 'FREED': 2,
            'E2ATK': 100, 'E2DEC': 0, 'E2SUS': 127, 'E2REL': 79, 'L2RATE': 27, 'L2WAVE': 1, 'ITYPE': 1, 'IA': 100, 'IB': 127, 'IC': 112, 'TRIM': 81,
            'E2FLT': 51, 'L2FLT': 5, 'E2': 14, 'MOD1': ('AT', 'FRES', 24), 'MOD2': ('ENV2', 'REV', 32), 'LTRIG': 1, 'L2TRIG': 1,
         },
         bend={
            'ATK': 81, 'DEC': 77, 'SUS': 115, 'REL': 82, 'ED_SHP': 9, 'LRATE': 68, 'LPHASE': 13, 'LFADE': 10, 'LD_FLT': -12, 'LD_SHP': 8,
            'LD_AMP': 12, 'KTRK': -10, 'DIST': 4, 'CHOR': 3, 'DLY': 11, 'REV': 39, 'DETUNE': 46, 'FREEA': 26, 'FREEB': 41, 'M1AMT': 17, 'M2AMT': 41,
            'M3AMT': -9, 'E2ATK': 91, 'E2DEC': 9, 'E2SUS': 124, 'E2REL': 84, 'L2RATE': 15, 'L2PHASE': 3, 'L2FADE': 11, 'IA': 94, 'IB': 124,
            'IC': 122, 'IMIX': 124, 'E2FLT': 37, 'E2': 8, 'E6': 12, 'E7': 7, 'E8': 11, 'SUB LEVEL': 13, 'CYCLE RISE': 0, 'CYCLE FALL': 68,
            'CYCLE CURVE': 11, 'AMPENV SATN': 12, 'FILTERENV MRES': 3, 'CYCLE PAN': -11, 'LFO2 MMODE': -3,
         }),
    dict(engine='WAVE', idx=17, name='FROZEN AIR', cat='FX', bseed=26635, bamt=17,  # #116
         p={
            'ATK': 88, 'SUS': 127, 'REL': 113, 'EQLG': -3, 'LRATE': 18, 'LWAVE': 1, 'RES2': 41, 'DLY': 32, 'REV': 102, 'TWTYPE': 2, 'FREEA': 57,
            'FREEB': 31, 'E2ATK': 114, 'E2DEC': 0, 'E2SUS': 127, 'E2REL': 86, 'L2RATE': 11, 'L2WAVE': 1, 'BLEND': -17, 'CUT2': 63, 'TRIM': 114,
            'E2SHP': 19, 'L2SHP': 13, 'E1': 25, 'E2': 20, 'E6': 7, 'MOD1': ('AT', 'E3', 8), 'MOD2': ('LFO1', 'CUT', 11), 'MOD3': ('LFO1', 'E6', 11),
            'MOD4': ('LFO2', 'CUT', 7), 'MOD5': ('CYCLE', 'CUT2', 11), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN ROUTE': 1, 'TWIN LINK': 0, 'CYCLE RISE': 110,
            'CYCLE FALL': 112,
         },
         bend={
            'ATK': 83, 'DEC': 56, 'SUS': 123, 'REL': 94, 'ED_SHP': -12, 'LRATE': 19, 'LPHASE': 14, 'LFADE': 19, 'LD_FLT': -3, 'LD_SHP': -6,
            'LD_AMP': 12, 'RES2': 51, 'DRV2': 8, 'KTRK': 19, 'DIST': 2, 'CHOR': 1, 'DLY': 47, 'REV': 89, 'DETUNE': 44, 'FREEA': 68, 'FREEB': 47,
            'FREEC': 6, 'M1AMT': -6, 'M2AMT': 10, 'M3AMT': 13, 'M4AMT': 10, 'M5AMT': 27, 'M6AMT': 26, 'M7AMT': 29, 'E2ATK': 106, 'E2DEC': 6,
            'E2SUS': 107, 'E2REL': 76, 'L2RATE': 28, 'L2PHASE': 11, 'L2FADE': 1, 'BLEND': -29, 'E2FLT': -1, 'E2': 14, 'E3': 2, 'E4': 21, 'E6': 9,
            'TWIN KTRK2': 8, 'TWIN ENV2': -20, 'TWIN LFO1': 18, 'TWIN LFO2': -18, 'SUB LEVEL': 6, 'CYCLE RISE': 92, 'CYCLE FALL': 101,
            'CYCLE CURVE': -9, 'AMPENV SATN': -4, 'FILTERENV FBLND': -13, 'FILTERENV FDRV': 10, 'FILTERENV MRES': 4, 'CYCLE TCUT': 19,
            'CYCLE PAN': -10, 'LFO2 TRES': -17, 'LFO2 MMODE': 13, 'LFO2 FBLND': 5,
         }),
    dict(engine='WAVE', idx=10, name='RESO SWEEP', cat='FX', bseed=36742,  # #117
         p={
            'ATK': 73, 'DEC': 105, 'SUS': 127, 'REL': 101, 'LRATE': 18, 'LWAVE': 1, 'DLY': 44, 'REV': 57, 'TWTYPE': 4, 'FREEA': 67, 'FREEB': 27,
            'L2RATE': 13, 'L2WAVE': 1, 'LPOL': 1, 'CUT2': 51, 'TRIM': 113, 'L2SHP': 45, 'E1': 8, 'E2': 64, 'E6': 6, 'MOD1': ('AT', 'SHP', 17),
            'MOD2': ('CYCLE', 'CUT', 9), 'MOD3': ('LFO1', 'SUBLV', 17), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO1': 11, 'CYCLE RISE': 100,
            'CYCLE FALL': 103,
         },
         bend={
            'ATK': 77, 'DEC': 94, 'SUS': 116, 'REL': 116, 'ED_SHP': -27, 'LRATE': 6, 'LPHASE': 11, 'LFADE': 13, 'LD_FLT': 3, 'LD_SHP': 30,
            'LD_AMP': 6, 'DRV2': 32, 'KTRK': 19, 'DIST': 22, 'CHOR': 30, 'DLY': 55, 'REV': 66, 'DETUNE': 51, 'FREEA': 50, 'FREEB': 1, 'FREEC': 22,
            'M1AMT': 52, 'M2AMT': -3, 'M3AMT': -2, 'M4AMT': 38, 'E2ATK': 31, 'E2DEC': 35, 'E2SUS': 110, 'E2REL': 33, 'L2RATE': 50, 'L2PHASE': 23,
            'L2FADE': 10, 'BLEND': 15, 'CUT2': 15, 'E2FLT': 11, 'E2': 88, 'E3': 12, 'E4': 18, 'E6': 9, 'TWIN KTRK2': -21, 'TWIN ENV2': 9,
            'TWIN LFO1': -18, 'TWIN LFO2': 29, 'SUB LEVEL': 37, 'CYCLE RISE': 111, 'CYCLE FALL': 89, 'CYCLE CURVE': 32, 'AMPENV SATN': -11,
            'FILTERENV FBLND': 18, 'FILTERENV FDRV': 6, 'FILTERENV MRES': -23, 'CYCLE TCUT': 30, 'CYCLE PAN': 25, 'LFO2 TRES': -15,
            'LFO2 MMODE': -32, 'LFO2 FBLND': -13,
         }),
    dict(engine='ANALOG', idx=15, name='GLASS SHARDS', cat='OTHER', bseed=17703,  # #118
         p={
            'ATK': 0, 'DEC': 74, 'SUS': 0, 'REL': 64, 'AMODE': 3, 'AOCT': 3, 'AGATE': 83, 'APROB': 102, 'RES2': 77, 'KTRK': 13, 'DIST': 32, 'DLY': 51,
            'TWTYPE': 2, 'FREEA': 51, 'FREEB': 41, 'E2ATK': 0, 'E2DEC': 56, 'E2SUS': 0, 'E2REL': 56, 'L2RATE': 27, 'L2WAVE': 1, 'ITYPE': 5, 'IA': 90,
            'IB': 100, 'IMIX': 64, 'CUT2': 57, 'TRIM': 119, 'E2FLT': 21, 'L2FLT': 5, 'E1': 1, 'E2': 0, 'E3': 0, 'E7': 127, 'MOD1': ('AT', 'CUT2', 17),
            'MOD2': ('CYCLE', 'CUT2', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN ENV2': 11, 'CYCLE RISE': 41, 'CYCLE FALL': 41,
            'CYCLE SYNC': 1,
         },
         bend={
            'ATK': 13, 'DEC': 73, 'SUS': 2, 'REL': 81, 'ED_SHP': 9, 'LRATE': 45, 'LPHASE': 20, 'LFADE': 15, 'LD_FLT': -12, 'LD_SHP': 13, 'LD_AMP': 7,
            'RES2': 69, 'DRV2': 5, 'KTRK': 25, 'DIST': 42, 'CHOR': 1, 'DLY': 58, 'REV': 20, 'DETUNE': 42, 'FREEA': 63, 'FREEB': 35, 'FREEC': 12,
            'M1AMT': 24, 'M2AMT': 4, 'M3AMT': 25, 'E2ATK': 18, 'E2DEC': 60, 'E2SUS': 8, 'E2REL': 63, 'L2RATE': 48, 'L2PHASE': 10, 'L2FADE': 13,
            'IA': 88, 'IB': 88, 'IC': 75, 'IMIX': 58, 'BLEND': 9, 'E2FLT': 6, 'E2': 2, 'E3': 18, 'E4': 18, 'E7': 124, 'TWIN KTRK2': -3,
            'TWIN ENV2': -2, 'TWIN LFO1': -13, 'TWIN LFO2': -3, 'SUB LEVEL': 19, 'CYCLE RISE': 52, 'CYCLE FALL': 52, 'CYCLE CURVE': -18,
            'AMPENV SATN': -20, 'FILTERENV FBLND': -18, 'FILTERENV FDRV': 1, 'FILTERENV MRES': -21, 'CYCLE TCUT': -11, 'CYCLE PAN': 18,
            'LFO2 TRES': 12, 'LFO2 MMODE': -21, 'LFO2 FBLND': -16,
         }),
    dict(engine='FM6', idx=8, name='TEMPLE BLOCK', cat='OTHER', bseed=28718,  # #119
         p={
            'REV': 25, 'FREEB': 40, 'E2ATK': 0, 'E2DEC': 67, 'E2SUS': 0, 'E2REL': 56, 'TRIM': 120, 'E3': 22, 'E4': -2, 'E6': 3, 'E8': 5,
            'MOD1': ('EXPR', 'PITCH', 63), 'MOD2': ('EXPR', 'PITCH', 38), 'MOD3': ('ENV2', 'AMP', 63), 'LTRIG': 1, 'L2TRIG': 1,
         },
         bend={
            'ATK': 6, 'DEC': 56, 'SUS': 2, 'REL': 52, 'ED_SHP': -17, 'LRATE': 52, 'LPHASE': 13, 'LFADE': 13, 'LD_FLT': 17, 'LD_SHP': -11,
            'LD_AMP': 2, 'KTRK': 44, 'DIST': 9, 'CHOR': 4, 'DLY': 21, 'REV': 27, 'DETUNE': 42, 'FREEA': 78, 'FREEB': 60, 'FREEC': 9, 'M3AMT': 61,
            'E2ATK': 7, 'E2DEC': 79, 'E2SUS': 1, 'E2REL': 49, 'L2RATE': 43, 'L2PHASE': 9, 'L2FADE': 4, 'E2FLT': -7, 'E2': 1, 'E3': 11, 'E5': 18,
            'E6': 5, 'E7': 5, 'SUB LEVEL': 21, 'CYCLE RISE': 3, 'CYCLE FALL': 75, 'CYCLE CURVE': 20, 'AMPENV SATN': -4, 'FILTERENV MRES': -14,
            'CYCLE PAN': -6, 'LFO2 MMODE': 20,
         },
         bfm=[
            97, 86, 91, 98, 97, 88, 96, 4, 8, 90, 98, 91, 85, 98, 98, 98, 0, 2, 87, 79, 34, 47, 92, 5, 9, 1, 54, 97, 68, 52, 50, 98, 1, 1, 0, 74, 97,
            82, 43, 54, 96, 5, 1, 2, 74, 86, 49, 42, 57, 98, 1, 1, 0, 97, 99, 99, 99, 99, 50, 50, 50, 34, 5, 0, 7,
         ]),
    dict(engine='LOFI', idx=5, name='DEAD BATTERY', cat='OTHER', bseed=16860,  # #120
         p={
            'ATK': 0, 'DEC': 68, 'SUS': 19, 'REL': 62, 'AMODE': 3, 'AOCT': 2, 'APROB': 114, 'DLY': 51, 'REV': 38, 'TWTYPE': 4, 'FREEA': 57,
            'FREEB': 31, 'E2ATK': 0, 'E2DEC': 66, 'E2SUS': 0, 'E2REL': 56, 'L2RATE': 35, 'L2WAVE': 1, 'CUT2': 56, 'TRIM': 111, 'E2FLT': 17, 'L2FLT': 5,
            'E2': 2, 'E4': 52, 'E6': 10, 'E8': 78, 'MOD1': ('AT', 'E8', -17), 'MOD2': ('CYCLE', 'CUT2', -11), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0,
            'CYCLE MODE': 1, 'CYCLE RISE': 60, 'CYCLE FALL': 80,
         },
         bend={
            'ATK': 12, 'DEC': 72, 'SUS': 23, 'REL': 42, 'ED_SHP': -21, 'LRATE': 39, 'LPHASE': 8, 'LFADE': 3, 'LD_FLT': -19, 'LD_SHP': 9, 'LD_AMP': 9,
            'DRV2': 4, 'KTRK': 30, 'DIST': 18, 'CHOR': 6, 'DLY': 36, 'REV': 41, 'DETUNE': 46, 'FREEA': 53, 'FREEB': 23, 'FREEC': 1, 'M1AMT': -27,
            'M2AMT': -8, 'M3AMT': 16, 'E2ATK': 8, 'E2DEC': 64, 'E2SUS': 9, 'E2REL': 35, 'L2RATE': 31, 'L2PHASE': 20, 'L2FADE': 10, 'BLEND': -6,
            'CUT2': 66, 'E2FLT': 27, 'E3': 79, 'E4': 33, 'E5': 16, 'E6': 26, 'E8': 94, 'TWIN KTRK2': -14, 'TWIN ENV2': 4, 'TWIN LFO1': -5,
            'TWIN LFO2': 17, 'SUB LEVEL': 17, 'CYCLE RISE': 80, 'CYCLE FALL': 63, 'CYCLE CURVE': 12, 'AMPENV SATN': -4, 'FILTERENV FBLND': 17,
            'FILTERENV FDRV': 2, 'FILTERENV MRES': -5, 'CYCLE TCUT': -12, 'CYCLE PAN': 6, 'LFO2 TRES': -21, 'LFO2 MMODE': 19, 'LFO2 FBLND': -17,
         }),
    dict(engine='LOFI', idx=7, name='DUSTY DRONE', cat='OTHER', bseed=63786,  # #121
         p={
            'ATK': 95, 'SUS': 127, 'REL': 103, 'EQLG': -3, 'LRATE': 17, 'LWAVE': 1, 'DLY': 34, 'REV': 47, 'TWTYPE': 4, 'FREEA': 57, 'FREEB': 17,
            'L2RATE': 32, 'L2WAVE': 1, 'ITYPE': 10, 'IA': 40, 'IB': 36, 'IC': 18, 'CUT2': 51, 'TRIM': 102, 'E1': 2, 'E2': 2, 'E4': 47, 'E8': 57,
            'MOD1': ('AT', 'E4', 11), 'MOD2': ('CYCLE', 'CUT', 7), 'MOD3': ('LFO2', 'E8', 7), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0, 'TWIN LFO1': 11,
            'CYCLE RISE': 115, 'CYCLE FALL': 119,
         },
         bend={
            'ATK': 75, 'DEC': 82, 'SUS': 124, 'REL': 99, 'ED_SHP': -2, 'LRATE': 36, 'LPHASE': 9, 'LFADE': 21, 'LD_FLT': 9, 'LD_SHP': 10, 'LD_AMP': 1,
            'DRV2': 5, 'KTRK': 50, 'DIST': 20, 'CHOR': 8, 'DLY': 32, 'REV': 66, 'DETUNE': 32, 'FREEA': 59, 'FREEB': 27, 'FREEC': 3, 'M1AMT': 19,
            'M2AMT': 9, 'M3AMT': -5, 'E2ATK': 20, 'E2DEC': 69, 'E2SUS': 72, 'E2REL': 42, 'L2RATE': 13, 'L2PHASE': 1, 'L2FADE': 16, 'IA': 25,
            'IB': 32, 'IC': 4, 'IMIX': 118, 'BLEND': -7, 'CUT2': 49, 'E2FLT': -20, 'E3': 58, 'E4': 63, 'E5': 6, 'E6': 4, 'E8': 55, 'TWIN KTRK2': -12,
            'TWIN ENV2': 4, 'TWIN LFO1': 10, 'TWIN LFO2': 12, 'SUB LEVEL': 6, 'CYCLE RISE': 116, 'CYCLE FALL': 104, 'CYCLE CURVE': -13,
            'AMPENV SATN': 6, 'FILTERENV FBLND': -5, 'FILTERENV FDRV': 11, 'FILTERENV MRES': 19, 'CYCLE TCUT': 4, 'CYCLE PAN': 8, 'LFO2 TRES': -11,
            'LFO2 MMODE': -7, 'LFO2 FBLND': 15,
         }),
    dict(engine='LOFI', idx=6, name='PIXEL KICK', cat='OTHER', bseed=33683,  # #122
         p={
            'ATK': 0, 'DEC': 72, 'SUS': 0, 'KTRK': 0, 'TRANS': -12, 'DIST': 19, 'REV': 10, 'E2ATK': 0, 'E2DEC': 62, 'E2SUS': 0, 'E2REL': 51,
            'TRIM': 118, 'E2': 1, 'E5': 16, 'E8': 90, 'MOD1': ('AT', 'E5', 13), 'LTRIG': 1, 'L2TRIG': 1,
         },
         bend={
            'ATK': 2, 'DEC': 76, 'SUS': 2, 'REL': 51, 'ED_SHP': 8, 'LRATE': 47, 'LPHASE': 10, 'LFADE': 2, 'LD_FLT': 6, 'LD_SHP': -6, 'LD_AMP': 9,
            'KTRK': 6, 'DIST': 17, 'CHOR': 9, 'DLY': 19, 'REV': 31, 'DETUNE': 31, 'FREEA': 77, 'FREEB': 6, 'FREEC': 14, 'M1AMT': 17, 'E2ATK': 17,
            'E2DEC': 71, 'E2SUS': 6, 'E2REL': 54, 'L2RATE': 79, 'L2PHASE': 12, 'L2FADE': 21, 'E2FLT': 6, 'E3': 60, 'E4': 17, 'E5': 7, 'E6': 8,
            'E8': 84, 'SUB LEVEL': 12, 'CYCLE RISE': 26, 'CYCLE FALL': 82, 'CYCLE CURVE': 5, 'AMPENV SATN': -21, 'FILTERENV MRES': 10,
            'CYCLE PAN': 14, 'LFO2 MMODE': -6,
         }),
    dict(engine='VOICE', idx=4, name='VOCAL BEATS', cat='OTHER', bseed=31759,  # #123
         p={
            'ATK': 0, 'DEC': 67, 'SUS': 0, 'REV': 19, 'FREEB': 40, 'E2ATK': 0, 'E2DEC': 65, 'E2SUS': 0, 'E2REL': 54, 'TRIM': 119, 'E2': 127, 'E3': 100,
            'E5': 30, 'E6': 100, 'E7': 110, 'E8': 89, 'MOD1': ('AT', 'SHP', 16), 'LTRIG': 1, 'L2TRIG': 1,
         },
         bend={
            'ATK': 16, 'DEC': 71, 'SUS': 2, 'REL': 62, 'ED_SHP': 21, 'LRATE': 75, 'LPHASE': 7, 'LFADE': 16, 'LD_FLT': 7, 'LD_SHP': -10, 'LD_AMP': 11,
            'KTRK': 50, 'DIST': 14, 'CHOR': 11, 'DLY': 17, 'REV': 6, 'DETUNE': 52, 'FREEA': 69, 'FREEB': 39, 'FREEC': 5, 'M1AMT': 37, 'E2ATK': 17,
            'E2DEC': 77, 'E2SUS': 14, 'E2REL': 62, 'L2RATE': 64, 'L2PHASE': 11, 'L2FADE': 3, 'E2FLT': 2, 'E1': 18, 'E2': 111, 'E3': 81, 'E4': -1,
            'E5': 20, 'E6': 95, 'E7': 107, 'E8': 88, 'SUB LEVEL': 6, 'CYCLE RISE': 28, 'CYCLE FALL': 83, 'CYCLE CURVE': 17, 'AMPENV SATN': -7,
            'FILTERENV MRES': -14, 'CYCLE PAN': -4, 'LFO2 MMODE': -3,
         }),
    dict(engine='TRIO', idx=10, name='DIAL TONE', cat='OTHER', bseed=61579,  # #124
         p={
            'ATK': 22, 'SUS': 127, 'REL': 39, 'KTRK': 0, 'TWTYPE': 1, 'CUT2': 45, 'TRIM': 90, 'E1': 4, 'E2': 4, 'E3': 0, 'E4': 0,
            'MOD1': ('AT', 'CUT2', 5), 'LTRIG': 1, 'L2TRIG': 1, 'TWIN LINK': 0,
         },
         bend={
            'ATK': 37, 'DEC': 50, 'SUS': 126, 'REL': 42, 'ED_SHP': -7, 'LRATE': 64, 'LPHASE': 11, 'LFADE': 16, 'LD_FLT': 6, 'LD_SHP': 8,
            'LD_AMP': 11, 'RES2': 17, 'DRV2': 20, 'KTRK': 4, 'DIST': 2, 'CHOR': 10, 'DLY': 20, 'REV': 1, 'DETUNE': 47, 'FREEA': 69, 'FREEB': 3,
            'FREEC': 2, 'M1AMT': -11, 'E2ATK': 23, 'E2DEC': 68, 'E2SUS': 91, 'E2REL': 49, 'L2RATE': 50, 'L2PHASE': 20, 'L2FADE': 21, 'BLEND': -18,
            'E2FLT': 20, 'E4': 3, 'E8': 48, 'TWIN KTRK2': 1, 'TWIN ENV2': 21, 'TWIN LFO1': 13, 'TWIN LFO2': -2, 'SUB LEVEL': 6, 'CYCLE RISE': 23,
            'CYCLE FALL': 55, 'CYCLE CURVE': -4, 'AMPENV SATN': 19, 'FILTERENV FBLND': -4, 'FILTERENV FDRV': -21, 'FILTERENV MRES': -3,
            'CYCLE TCUT': -8, 'CYCLE PAN': 19, 'LFO2 TRES': 12, 'LFO2 MMODE': 11, 'LFO2 FBLND': -7,
         }),
    dict(engine='TRIO', idx=6, name='GRAVEL SNARE', cat='OTHER', bseed=20254,  # #125
         p={
            'ATK': 0, 'DEC': 71, 'SUS': 0, 'REL': 65, 'RES2': 30, 'KTRK': 0, 'REV': 23, 'TWTYPE': 1, 'E2ATK': 0, 'E2DEC': 67, 'E2SUS': 0, 'E2REL': 59,
            'CUT2': 65, 'E1': 7, 'E3': 24, 'E4': 0, 'E8': 50, 'MOD1': ('AT', 'CUT2', 8), 'MOD2': ('VEL', 'CUT2', 10), 'LTRIG': 1, 'L2TRIG': 1,
            'TWIN LINK': 0, 'TWIN ENV2': 17,
         },
         bend={
            'ATK': 18, 'DEC': 75, 'SUS': 2, 'REL': 74, 'ED_SHP': -15, 'LRATE': 74, 'LPHASE': 3, 'LFADE': 12, 'LD_FLT': -9, 'LD_SHP': -11,
            'LD_AMP': 1, 'RES2': 6, 'DRV2': 10, 'KTRK': -13, 'DIST': 16, 'CHOR': 17, 'DLY': 19, 'REV': 31, 'DETUNE': 46, 'FREEA': 69, 'FREEB': 9,
            'FREEC': 3, 'M1AMT': -11, 'M2AMT': -5, 'E2ATK': 7, 'E2DEC': 56, 'E2SUS': 6, 'E2REL': 70, 'L2RATE': 59, 'L2PHASE': 15, 'L2FADE': 5,
            'BLEND': 14, 'CUT2': 45, 'E2FLT': 13, 'E4': 6, 'E8': 34, 'TWIN KTRK2': -19, 'TWIN ENV2': 21, 'TWIN LFO1': 8, 'TWIN LFO2': -15,
            'SUB LEVEL': 8, 'CYCLE RISE': 25, 'CYCLE FALL': 75, 'CYCLE CURVE': 19, 'AMPENV SATN': -21, 'FILTERENV FBLND': 14, 'FILTERENV FDRV': -5,
            'FILTERENV MRES': -2, 'CYCLE TCUT': 5, 'CYCLE PAN': -18, 'LFO2 TRES': -5, 'LFO2 MMODE': 11, 'LFO2 FBLND': 17,
         }),
    dict(engine='PHYS', idx=7, name='TABLA TALK', cat='OTHER', bseed=40371,  # #126
         p={
            'ATK': 0, 'SUS': 127, 'REL': 56, 'REV': 19, 'FREEB': 40, 'E2ATK': 0, 'E2DEC': 69, 'E2SUS': 0, 'TRIM': 111, 'E1': 2, 'E2': 112, 'E3': 76,
            'E4': 56, 'E5': 44, 'E6': 100, 'E7': 14, 'E8': 24, 'MOD1': ('AT', 'E7', 13), 'LTRIG': 1, 'L2TRIG': 1,
         },
         bend={
            'ATK': 9, 'DEC': 74, 'SUS': 122, 'REL': 71, 'ED_SHP': 7, 'LRATE': 39, 'LPHASE': 19, 'LFADE': 4, 'LD_FLT': -7, 'LD_SHP': -10, 'LD_AMP': 6,
            'KTRK': 42, 'DIST': 21, 'CHOR': 5, 'DLY': 5, 'REV': 11, 'DETUNE': 45, 'FREEA': 76, 'FREEB': 27, 'FREEC': 7, 'M1AMT': 6, 'E2ATK': 14,
            'E2DEC': 60, 'E2SUS': 8, 'E2REL': 67, 'L2RATE': 54, 'L2PHASE': 21, 'L2FADE': 10, 'E2FLT': 17, 'E2': 114, 'E3': 75, 'E4': 60, 'E5': 34,
            'E6': 103, 'E7': 35, 'E8': 3, 'SUB LEVEL': 14, 'CYCLE RISE': 15, 'CYCLE FALL': 90, 'CYCLE CURVE': -14, 'AMPENV SATN': 7,
            'FILTERENV MRES': 1, 'CYCLE PAN': 7, 'LFO2 MMODE': 6,
         }),
    dict(engine='NOISE', idx=8, name='GEIGER', cat='OTHER', bseed=56918,  # #127
         p={
            'ATK': 19, 'SUS': 127, 'REL': 73, 'KTRK': 0, 'DIST': 57, 'DLY': 32, 'REV': 38, 'FREEA': 61, 'FREEB': 87, 'FREEC': 63, 'L2RATE': 34,
            'L2WAVE': 1, 'TRIM': 102, 'L2FLT': 8, 'E1': 1, 'E2': 0, 'E6': 110, 'E7': 20, 'MOD1': ('AT', 'E6', 19), 'MOD2': ('CYCLE', 'E6', 9),
            'LTRIG': 1, 'L2TRIG': 1, 'CYCLE RISE': 105, 'CYCLE FALL': 110,
         },
         bend={
            'ATK': 0, 'DEC': 68, 'SUS': 111, 'REL': 89, 'ED_SHP': -4, 'LRATE': 53, 'LPHASE': 9, 'LFADE': 2, 'LD_FLT': 4, 'LD_SHP': -12, 'LD_AMP': 3,
            'KTRK': 14, 'DIST': 55, 'CHOR': 8, 'DLY': 33, 'REV': 25, 'DETUNE': 59, 'FREEA': 51, 'FREEC': 58, 'M1AMT': 23, 'M2AMT': -1, 'M3AMT': 15,
            'E2ATK': 11, 'E2DEC': 71, 'E2SUS': 92, 'E2REL': 74, 'L2RATE': 13, 'L2PHASE': 9, 'L2FADE': 7, 'E2FLT': 19, 'E2': 17, 'E6': 93, 'E7': 33,
            'E8': 12, 'SUB LEVEL': 9, 'CYCLE RISE': 120, 'CYCLE FALL': 89, 'CYCLE CURVE': 3, 'AMPENV SATN': -2, 'FILTERENV MRES': 7,
            'CYCLE PAN': -12, 'LFO2 MMODE': -3,
         }),
    dict(engine='NOISE', idx=9, name='MODEM', cat='OTHER', bseed=7821,  # #128
         p={
            'ATK': 0, 'DEC': 62, 'SUS': 0, 'REL': 56, 'AMODE': 3, 'AOCT': 3, 'AGATE': 57, 'APROB': 108, 'KTRK': 63, 'DLY': 51, 'REV': 25, 'FREEA': 71,
            'FREEB': 31, 'FREEC': 15, 'E2ATK': 0, 'E2DEC': 60, 'E2SUS': 0, 'E2REL': 51, 'TRIM': 114, 'E2FLT': 8, 'E1': 2, 'E2': 0, 'E6': 48, 'E8': 60,
            'MOD1': ('AT', 'CUT', 10), 'LTRIG': 1, 'L2TRIG': 1,
         },
         bend={
            'ATK': 4, 'DEC': 66, 'SUS': 2, 'REL': 42, 'ED_SHP': -2, 'LRATE': 65, 'LPHASE': 13, 'LFADE': 16, 'LD_FLT': -9, 'LD_SHP': 20, 'LD_AMP': 4,
            'KTRK': 56, 'DIST': 15, 'CHOR': 6, 'DLY': 72, 'REV': 8, 'DETUNE': 41, 'FREEA': 82, 'FREEB': 37, 'FREEC': 7, 'M1AMT': 17, 'E2ATK': 16,
            'E2DEC': 68, 'E2SUS': 11, 'E2REL': 54, 'L2RATE': 77, 'L2PHASE': 3, 'L2FADE': 12, 'E2FLT': 29, 'E2': 5, 'E7': 20, 'SUB LEVEL': 10,
            'CYCLE RISE': 26, 'CYCLE FALL': 50, 'CYCLE CURVE': 1, 'AMPENV SATN': 8, 'FILTERENV MRES': -16, 'CYCLE PAN': 7, 'LFO2 MMODE': -13,
         }),
]
