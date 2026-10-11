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
static int favorite_has(uint32_t engine, uint32_t preset)
{
    if (engine == NENGINES)
        return preset < UP_SLOTS && ((*FAV_USER_WORD(preset) >> (preset % 32u)) & 1u);
    return engine < NENGINES && engine < 16u && preset < 256u &&
        ((favorites.factory[engine][preset / 8u] >> (preset % 8u)) & 1u);
}
static int favorite_set(uint32_t engine, uint32_t preset, int on)
{
    if (engine > NENGINES || (engine == NENGINES ? preset >= UP_SLOTS : engine >= 16u || preset >= 256u))
        return 0;
    if (favorite_has(engine, preset) == !!on) return 0;
    if (engine == NENGINES) {
        uint32_t *w = FAV_USER_WORD(preset);
        if (on) *w |= 1u << (preset % 32u);
        else *w &= ~(1u << (preset % 32u));
    } else {
        uint8_t *b = &favorites.factory[engine][preset / 8u];
        if (on) *b |= (uint8_t)(1u << (preset % 8u));
        else *b &= (uint8_t)~(1u << (preset % 8u));
    }
    return 1;
}
