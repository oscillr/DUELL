/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.14: the factory tables as the converter's test input. For SAWSQ, PWM, ORGAN and VOWEL: the 19 frames of level 0 (256 samples, 8 bit,
 * exactly what the unit plays) and the bytes of the recipe encoded as a user table (wtab_encode.h). Written as JSON (base64) to argv[1];
 * web/fixtures/wavetable_factory.json is the committed copy, host_tests.sh wavetable compares a fresh run with it (and web/test_wavetable.mjs
 * converts the frames back to bytes and compares them with the recipe's). */
#define WV_TEST_HOOKS 1
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "wtab_encode.h"

static void b64(FILE *f, const uint8_t *p, uint32_t n)
{
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint32_t i;
    for (i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)p[i] << 16 | (i + 1 < n ? (uint32_t)p[i + 1] << 8 : 0) | (i + 2 < n ? p[i + 2] : 0);
        fputc(T[v >> 18 & 63], f);
        fputc(T[v >> 12 & 63], f);
        fputc(i + 1 < n ? T[v >> 6 & 63] : '=', f);
        fputc(i + 2 < n ? T[v & 63] : '=', f);
    }
}
int main(int argc, char **argv)
{
    static const uint8_t TB[4] = {0, 1, 4, 5};
    static uint8_t bytes[WV_F * WV_H], pcm[WV_F * 256];
    uint32_t t, f, n;
    FILE *o = argc > 1 ? fopen(argv[1], "w") : stdout;
    if (!o)
        return 1;
    fprintf(o, "{\n \"about\": \"DUELL 0.2.14 tests/wavetable_fixture.c: factory tables as played (level 0, 19 frames x 256 int8, base64) and as encoded user-table bytes (19 x 64)\",\n \"tables\": [\n");
    for (t = 0; t < 4u; t++) {
        for (f = 0; f < WV_F; f++) {
            wv_frame(TB[t], f, wvj.out);
            for (n = 0; n < 256u; n++)
                pcm[f * 256u + n] = (uint8_t)wvj.out[n];
        }
        wtab_encode(TB[t], bytes);
        fprintf(o, "  {\"table\": %u, \"name\": \"%s\", \"pcm\": \"", TB[t], N_WV_TABLE[TB[t]]);
        b64(o, pcm, sizeof pcm);
        fprintf(o, "\", \"bytes\": \"");
        b64(o, bytes, sizeof bytes);
        fprintf(o, "\"}%s\n", t < 3u ? "," : "");
    }
    fprintf(o, " ]\n}\n");
    if (o != stdout)
        fclose(o);
    return 0;
}
