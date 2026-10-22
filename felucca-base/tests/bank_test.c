/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.0: THE FACTORY BANK and the ARP RECALL.
 * 1. The bank (tools/factory_bank.duell-patches.json, the owner's 128 patches as exported, imported by tools/import_bank.py; tests/factory_bank_records.txt: "NN crc32
 *    record-hex trim" per slot, slot and revision bytes 0, the crc32 the file's own, trim the balanced TRIM of tools/factory_bank_trims.json). For every browse slot k the
 *    factory sound is loaded through the firmware (sl_load, as PATCHES loads it) and saved through the firmware (up_patch_save_part, as SAVE does: End B block, extension
 *    block, arp / keys block, the FM6 patch). Then the stored record and the file's record are both put into the store and decoded the firmware's way (up_rec, ak_get,
 *    upf_get) and compared FIELD BY FIELD: engine, name, category, all 127 values but the TRIM (which must be the balanced one in the stored record and the file's own in
 *    the file's), the extension slots, End B block, hand-set edits, pattern, FM6 patch; and the stored record must carry the arp and keys block (arp OFF written
 *    explicitly). Counted: byte for byte equal to the file's record (the case when the TRIM is unchanged and the file's record has the block), equal in every field.
 * 2. The arp recall: a patch saved with the arp OFF turns the arp OFF when it is loaded after one with the arp ON; a patch with an arp brings it back (all eight values);
 *    a factory sound without an arp turns an arp off; the chord keys the same; a record without the block (written before 1.0) loads as arp OFF, keys at their
 *    defaults; the global MODULES switch still kills the arp.
 *   build/host/bank_test [records-file]          (DUELL/host_tests.sh "bank") */
#include <ctype.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad;
static void ck(const char *what, int ok)
{
    printf("bank: %-120s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint32_t crc32_of(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu, i, j;
    for (i = 0; i < n; i++) {
        c ^= p[i];
        for (j = 0; j < 8u; j++)
            c = c >> 1 ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}
static int hexv(int c) { return c <= '9' ? c - '0' : (c | 32) - 'a' + 10; }

static uint8_t want[128][600], got[128][PT_WORST + 8];
static uint32_t wn[128], gn[128];
static int wtrim[128];

/* the fields of a stored slot, the firmware's decode */
typedef struct {
    up_rec_t r;
    uint8_t pk[FM6_PACKED];
    int fm, has_ak;
    ak_t a;
} dec_t;
static void decode(uint32_t k, dec_t *d)
{
    d->r = *up_rec(k);
    d->fm = upf_get(k, d->pk) == 0;
    d->has_ak = ak_get(k, &d->a);
}
/* 0: the same in every field but P_TRIM; else a short reason */
static const char *same_fields(const dec_t *x, const dec_t *y)
{
    uint32_t i;
    if (x->r.engine != y->r.engine || memcmp(x->r.name, y->r.name, 12))
        return "engine or name";
    for (i = 0; i < P_COUNT; i++)
        if (i != P_TRIM && x->r.packed[i] != y->r.packed[i])
            return "a value";
    if (x->r.packed[UP_CAT_AT] != y->r.packed[UP_CAT_AT])
        return "category";
    if (memcmp(x->r.packed + UP_MB_AT, y->r.packed + UP_MB_AT, UP_MB_N))
        return "End B block";
    if (memcmp(x->r.xz, y->r.xz, NX) || x->r.xb_n != y->r.xb_n || memcmp(x->r.xb, y->r.xb, x->r.xb_n * sizeof x->r.xb[0]))
        return "extension block";
    if (memcmp(x->r.note, y->r.note, 16) || memcmp(x->r.flags, y->r.flags, 16))
        return "pattern";
    if (x->fm != y->fm || (x->fm && memcmp(x->pk, y->pk, FM6_PACKED)))
        return "FM6 patch";
    return 0;
}

static void set_arp(track_t *t, int on)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        t->p[P_AMODE + i] = TP[P_AMODE + i].def;
    if (on) {
        t->p[P_AMODE] = 3;      /* UPDN */
        t->p[P_ARATE] = 5;
        t->p[P_AOCT] = 2;
        t->p[P_AGATE] = 77;
        t->p[P_ASWING] = 9;
        t->p[P_APROB] = 100;
        t->p[P_AHOLD] = 1;
        t->p[P_AORDER] = 1;
    }
}
static int arp_equal(const track_t *t, const int16_t *v)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        if (t->p[P_AMODE + i] != v[i])
            return 0;
    return 1;
}

int main(int argc, char **argv)
{
    const char *fn = argc > 1 ? argv[1] : "tests/factory_bank_records.txt";
    FILE *f = fopen(fn, "r");
    static char line[4096];
    uint32_t n_ok = 0, n_bytes = 0, n_fields = 0, n_noend = 0, k, i, nrec = 0;
    static dec_t dw, dg;
    if (!f) {
        printf("bank: cannot open %s\n", fn);
        return 2;
    }
    ui_power_on();
    frame();
    song.playing = 0;
    while (fgets(line, sizeof line, f)) {
        unsigned num, crc;
        char *h;
        uint32_t n = 0;
        if (sscanf(line, "%u %x", &num, &crc) != 2 || !(h = strchr(strchr(line, ' ') + 1, ' ')) || num < 1u || num > 128u)
            continue;
        for (h++; isxdigit((unsigned char)h[0]) && isxdigit((unsigned char)h[1]); h += 2)
            want[num - 1u][n++] = (uint8_t)(hexv(h[0]) << 4 | hexv(h[1]));
        wn[num - 1u] = n;
        wtrim[num - 1u] = atoi(h);
        if (crc32_of(want[num - 1u], n) != crc) {
            printf("bank: #%u: the line's crc32 does not match its record\n", num);
            bad++;
        }
        nrec++;
    }
    fclose(f);
    ck("the records file holds the 128 patches", nrec == 128u);
    /* pass 1: every factory sound through the firmware's load and save */
    for (k = 0; k < 128u; k++) {
        sl_load((k + 61u) % UP_SLOTS);                    /* another sound first: nothing of the one before may leak into this one */
        sl_load(k);
        if (up_patch_save_part(0, k) != 0 || !up_used(k) || !(gn[k] = up_raw_get(k, got[k]))) {
            printf("bank: #%u: the firmware did not store it\n", k + 1u);
            bad++;
            continue;
        }
        n_ok++;
    }
    ck("all 128 factory slots load and store through the firmware", n_ok == 128u);
    /* pass 2: the file's record and the stored one, decoded the firmware's way */
    for (k = 0; k < 128u; k++) {
        const char *why;
        int stored_ak;
        if (!gn[k])
            continue;
        up_put_raw(k, want[k], wn[k]);
        decode(k, &dw);
        up_put_raw(k, got[k], gn[k]);
        decode(k, &dg);
        stored_ak = dg.has_ak && (dg.a.g & AK_ARP) && (dg.a.g & AK_KEYS);
        why = same_fields(&dg, &dw);
        if (gn[k] == wn[k] && !memcmp(got[k], want[k], wn[k]))
            n_bytes++;
        if (!why && up_value(&dg.r, P_TRIM) == wtrim[k] && up_value(&dw.r, P_TRIM) == up_value(&dw.r, P_TRIM) && stored_ak) {
            n_fields++;
            continue;
        }
        if (why && !strcmp(why, "End B block") && !(dw.r.packed[UP_MB_AT]) && !same_fields((dg.r.packed[UP_MB_AT] = 0, memset(dg.r.packed + UP_MB_AT, 0, UP_MB_N), &dg), &dw) &&
            up_value(&dg.r, P_TRIM) == wtrim[k] && stored_ak) {
            n_noend++;
            printf("bank: #%u equals the file's record in every field but the End B block (the file has none, the firmware's save always writes one)\n", k + 1u);
            continue;
        }
        printf("bank: #%u DIFFERS (%s; trim stored %d, wanted %d; arp block %s)\n", k + 1u, why ? why : "the TRIM or the arp block", up_value(&dg.r, P_TRIM), wtrim[k], stored_ak ? "yes" : "NO");
        bad++;
    }
    printf("bank: %u of 128 slots: %u equal the file's record in every field (TRIM the balanced one, the arp block written), %u of them byte for byte, %u equal but for the End B block the file lacks\n",
           n_ok, n_fields, n_bytes, n_noend);
    ck("every factory slot stores as the file's record (all fields; one End B block the file lacks)", n_fields + n_noend == 128u && n_noend <= 1u);
    for (k = 0; k < 128u; k++)
        sl_restore(k);

    /* 2. the arp recall */
    {
        track_t *t = &trk[0];
        int16_t on[8], off[8];
        uint32_t a_on = 0, a_off = 0, s;
        uint8_t rec[PT_WORST + 8];
        uint32_t rn;
        for (i = 0; i < 8u; i++)
            off[i] = TP[P_AMODE + i].def;
        /* find a factory sound with no arp (the bank has several arp sounds: ARP BASS, ARP PLUCK ..) and one that has one */
        for (s = 0; s < 128u; s++) {
            uint32_t e, fk;
            if (!sl_fac(s, &e, &fk))
                continue;
            if (DS_SOUNDS[DS_ENG_AT[e] + fk].flags & DSF_ARP) {
                if (!a_on)
                    a_on = s + 1u;
            } else if (!a_off)
                a_off = s + 1u;
        }
        ck("the bank has a factory sound with an arp and one without", a_on && a_off);
        set_arp(t, 1);
        for (i = 0; i < 8u; i++)
            on[i] = t->p[P_AMODE + i];
        /* slot 10: stored with the arp OFF, slot 11: stored with the arp ON, both from the same sound */
        sl_load(30u);
        set_arp(t, 0);
        ck("save slot 10 with the arp OFF", up_patch_save_part(0, 10u) == 0);
        set_arp(t, 1);
        ck("save slot 11 with the arp ON", up_patch_save_part(0, 11u) == 0);
        ck("  both records carry the arp block (the OFF one too)", ak_get(10u, &dg.a) && (dg.a.g & AK_ARP) && ak_get(11u, &dg.a) && (dg.a.g & AK_ARP) && (dg.a.g & AK_KEYS));
        sl_load(11u);
        ck("load the arp ON patch: its arp is back, all eight values", arp_equal(t, on));
        sl_load(10u);
        ck("load the arp OFF patch after it: the arp is OFF (it used to stay ON)", arp_equal(t, off));
        sl_load(11u);
        sl_load(a_off - 1u);
        ck("a factory sound without an arp, loaded after an arp patch: the arp is OFF", arp_equal(t, off));
        sl_load(11u);
        set_arp(t, 0);
        t->p[P_ARATE] = 7;                                  /* the player changed one value of the lent arp, then: */
        sl_load(10u);
        ck("a load sets the patch's own arp even after the player changed it", arp_equal(t, off));
        sl_load(a_on - 1u);
        ck("a factory sound with an arp sets it", t->p[P_AMODE] != 0);
        sl_load(10u);
        ck("  and the next patch (arp OFF) takes it away again", arp_equal(t, off));
        /* the chord keys */
        sl_load(30u);
        set_arp(t, 0);
        t->p[P_CHRD] = 4;
        t->p[P_VOIC] = 2;
        ck("save slot 12 with chord keys", up_patch_save_part(0, 12u) == 0);
        t->p[P_CHRD] = TP[P_CHRD].def;
        t->p[P_VOIC] = TP[P_VOIC].def;
        ck("save slot 13 without", up_patch_save_part(0, 13u) == 0);
        sl_load(12u);
        ck("load the patch with chord keys: they are back", t->p[P_CHRD] == 4 && t->p[P_VOIC] == 2);
        sl_load(13u);
        ck("load the patch without: the keys are at their defaults (they stayed before)", t->p[P_CHRD] == TP[P_CHRD].def && t->p[P_VOIC] == TP[P_VOIC].def);
        /* a record written before 1.0: no block at all = arp OFF, keys default */
        rn = up_raw_get(11u, rec);
        {
            uint32_t xbo, mb, pr, pat, fm, ak;
            pt_layout_x(rec, &xbo, &mb, &pr, &pat, &fm, &ak);
            ck("the arp ON record has its block last", (rec[4] & PTF_AK) && ak + 2u + pt_rd16(rec + ak) == rn);
            /* the same record with the arp values among its pairs and no block: the pairs are ignored (param_kept), the block's absence means OFF */
            rec[4] &= (uint8_t)~PTF_AK;
            rn = ak;
            rec[0] = (uint8_t)rn;
            rec[1] = (uint8_t)(rn >> 8);
        }
        ck("a record without the block is taken by the store", up_put_raw(14u, rec, rn) == 0 || up_put_raw(14u, rec, rn) == 3);
        sl_load(11u);
        sl_load(14u);
        ck("a record without the arp block (written before 1.0): the arp is OFF, not the previous patch's", arp_equal(t, off));
        /* the MODULES switch still kills every arp */
        sl_load(11u);
        mods_off |= MODS_ARP;
        ck("GLO > MODULES ARP off: the patch's arp is stored but plays as OFF", arp_mode(t) == 0 && arp_equal(t, on));
        mods_off &= (uint8_t)~MODS_ARP;
        ck("  and on again", arp_mode(t) == 3);
    }
    printf("bank: %s\n", bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}
