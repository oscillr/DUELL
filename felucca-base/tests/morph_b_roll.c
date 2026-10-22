/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.0: ROLL B of every factory multi by the firmware's own roll (morph.c morph_roll_seed: the part's AMOUNT, the multi's settings, SMART as stored), one seed for both
 * parts as the ROLL B button gives (morph_new_seed from a fixed start, 1717 + 7 * slot: repeatable). Prints, per slot and part, the End B block morph_blk_make writes
 * (7 bytes, hex) and the seed: tools/morph_b_roll.py puts those 7 bytes into tools/factory_multis/Mnn.bin and nothing else. By hand, not in the default list. */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define HOST_IRQ_HOOK 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static void fm1_irq_off(void);
static void fm1_irq_on(void);
#include "ui_test.c"
static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { const uint8_t *s = src; uint32_t i; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}
#include <math.h>


int main(void)
{
    uint32_t slot;
    for (slot = 0; slot < 15u; slot++) {
        int pf[2];
        if (pipe(pf))
            return 1;
        if (!fork()) {
            uint32_t i, pi, seed;
            int32_t o[2 * CTL];
            uint8_t b[MB_N];
            memset(nor, 0xFF, sizeof nor);
            ui_power_on();
            flash_ok = 1;
            multi_boot();
            frame();
            song.master_q12 = 4096;
            ui_prefs2 |= PREF_MODE_RECALL >> 16;
            project_load(slot);
            for (i = 0; i < 40; i++) { memset(o, 0, sizeof o); mix_block(o, CTL); while (wave_service()) ; }
            morph.gen = 1717u + 7u * (slot + 1u);
            fm1_ms = 0;
            {   /* MBR_K=k1,k2,..: the slot's k-th seed of its fixed sequence (default the first) */
                uint32_t k, kk = 1, n;
                const char *e = getenv("MBR_K");
                for (n = 0; e && *e && n <= slot; n++) {
                    kk = (uint32_t)atoi(e);
                    e = strchr(e, ',');
                    e = e ? e + 1 : 0;
                    if (n < slot && !e)
                        kk = 1;
                }
                for (k = 0; k < kk; k++)
                    seed = morph_new_seed();
            }
            for (pi = 0; pi < NSHOWN; pi++) {
                char ln[120];
                int8_t oldb[P_COUNT];
                memcpy(oldb, morph_cur(&mph[pi], 1), P_COUNT);
                morph_roll_seed(pi, 1u, seed);
                if (!morph_blk_make(pi, b))
                    _exit(3);
                if (getenv("MBR_VEC")) {
                    uint32_t k;
                    printf("V M%02u %u eng %u", slot + 1u, pi + 1u, (unsigned)trk[pi].eng_req);
                    for (k = 0; k < P_COUNT; k++)
                        printf(" %d,%d,%d", mph[pi].base[k], oldb[k], morph_cur(&mph[pi], 1)[k]);
                    printf("\n");
                    fflush(stdout);
                }
                fprintf(stderr, "M%02u %u TRIM A %d B %d\n", slot + 1u, pi + 1u, mph[pi].base[P_TRIM], morph_cur(&mph[pi], 1)[P_TRIM]);
                snprintf(ln, sizeof ln, "M%02u %u seed %u amount %u stereo %d %02x %02x %02x %02x %02x %02x %02x\n", slot + 1u, pi + 1u, seed, (unsigned)mph[pi].amount, (int)duell_stereo, b[0], b[1], b[2], b[3], b[4], b[5], b[6]);
                if (!getenv("MBR_VEC") && write(pf[1], ln, strlen(ln)) < 0)
                    _exit(2);
            }
            _exit(0);
        }
        close(pf[1]);
        {
            char buf[512];
            ssize_t n;
            while ((n = read(pf[0], buf, sizeof buf)) > 0)
                fwrite(buf, 1, (size_t)n, stdout);
        }
        close(pf[0]);
        wait(0);
    }
    return 0;
}
