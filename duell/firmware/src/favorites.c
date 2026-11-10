/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
#define FELUCCA_FAVORITES 1
/* Stable engine/preset references; fixed capacity independent of optional engines. */
typedef struct {
    uint8_t factory[16][32]; /* engine 0..15, preset 0..255 */
    uint32_t user, filter;   /* user: the stored patches 0..31 (bits); filter: the FAV list */
    uint32_t user_hi[3];     /* DUELL: the stored patches 32..127 (the store has 128 slots); appended, so settings of before still read */
} favorites_t;
/* the word and bit of stored patch k */
#define FAV_USER_WORD(k) ((k) < 32u ? &favorites.user : &favorites.user_hi[(k) / 32u - 1u])
static favorites_t favorites __attribute__((section(".pool")));
/* the byte holding factory sound `preset` of `engine`'s flag, 0 = none. Row e is engine e's, except: row 14 is the MIDI LEARN map
 * (midi_learn.c) and row 15 the settings bytes, so WAVE (engine 14, DUELL) keeps its flags in bytes 16..31 of row 1, DIGITAL's
 * retired row (DIGITAL, engine 1, is gone: its byte 0 is cleared on import, settings_persist.c; its bytes 16..31 were never written, so every older
 * setting holds no WAVE flag there (128 sounds)) */
#define FAV_WAVE_ROW 1u
#define FAV_WAVE_AT 16u
static uint8_t *fav_byte(uint32_t engine, uint32_t preset)
{
    if (engine == ENGI_WAVE)
        return preset < 8u * (32u - FAV_WAVE_AT) ? &favorites.factory[FAV_WAVE_ROW][FAV_WAVE_AT + preset / 8u] : 0;
    return engine < NENGINES && engine < 14u && preset < 256u ? &favorites.factory[engine][preset / 8u] : 0;
}
static int favorite_has(uint32_t engine, uint32_t preset)
{
    const uint8_t *b;
    if (engine == NENGINES)
        return preset < UP_SLOTS && ((*FAV_USER_WORD(preset) >> (preset % 32u)) & 1u);
    b = fav_byte(engine, preset);
    return b && ((*b >> (preset % 8u)) & 1u);
}
static int favorite_set(uint32_t engine, uint32_t preset, int on)
{
    uint8_t *b;
    if (engine > NENGINES || (engine == NENGINES ? preset >= UP_SLOTS : !fav_byte(engine, preset)))
        return 0;
    if (favorite_has(engine, preset) == !!on) return 0;
    if (engine == NENGINES) {
        uint32_t *w = FAV_USER_WORD(preset);
        if (on) *w |= 1u << (preset % 32u);
        else *w &= ~(1u << (preset % 32u));
    } else {
        b = fav_byte(engine, preset);
        if (on) *b |= (uint8_t)(1u << (preset % 8u));
        else *b &= (uint8_t)~(1u << (preset % 8u));
    }
    return 1;
}
