/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.14: a factory table's recipe encoded as the bytes of a user table (the converter's encoding, web/editor/wavetable.js ampToByte):
 * per frame, each harmonic's magnitude over the frame's strongest, 255 + 16 log2(ratio) rounded, 0 under -66 dB. Needs eng_wave.c (included before). */
static void wtab_encode(uint32_t tb, uint8_t *bytes)
{
    uint32_t f, k;
    for (f = 0; f < WV_F; f++) {
        double big = 0;
        wv_recipe(tb, (int32_t)((f * 65536u) / (WV_F - 1u)), wvj.a);
        for (k = 1; k <= WV_H; k++)
            big = fabs((double)wvj.a[k]) > big ? fabs((double)wvj.a[k]) : big;
        for (k = 1; k <= WV_H; k++) {
            double r = big > 0 ? fabs((double)wvj.a[k]) / big : 0, db = r > 0 ? 20 * log10(r) : -999;
            long b = db < -66.0 ? 0 : (long)floor(255 + 16 * log2(r) + 0.5);
            bytes[f * WV_H + k - 1u] = (uint8_t)(b < 1 && db >= -66.0 ? 1 : b > 255 ? 255 : b);
        }
    }
}
