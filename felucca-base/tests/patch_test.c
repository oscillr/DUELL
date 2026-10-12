/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL PATCHES (upreset.c the compact PATCH store, SAVE > PATCHES, SPEC 3.6) against simulated NOR flash; a few fast checks:
 *   1. a stored patch is a sparse record in the patch store only (6 chunks: 0xC8000..0xCDFFF, 0xDC000..0xDFFFF, 0x9F000 / 0xFE000):
 *      small, equal after a power cycle (values, name, End B block, FM6 patch); overwrite and erase; a power cut during a write
 *      leaves the old record;
 *   2. the limit is honest: PT_MAX worst-case records (FM6 + patch, SEQ arp with 16 steps, End B, shadow marker, every value off its
 *      default) always fit, whatever was stored before (small records first, then all overwritten in place); no STORE FULL below
 *      PT_MAX; the next new patch answers NO FREE PLACE and writes nothing; a power cut during the last save loses only that one;
 *   3. the page: ONE list (the factory sounds, the user patches, then exactly the free places as empty ones); K1 steps on to an empty
 *      place without loading; hold SAVE there opens NAME and stores a NEW patch; K2 TYPE by engine is a filter (the part's engine
 *      stays); K3 FAV; K4 ERASE: ALREADY FACTORY / RESTORE FACTORY? (a factory sound saved over) / ERASE PATCH?.
 *   build/host/patch_test            (DUELL/host_tests.sh "patch") */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"

static uint8_t nor[0x100000];
static long cut_bytes = -1;
static int dead, watch;
static uint32_t stray, wrote;                       /* (watch) bytes erased / programmed outside the patch store; inside it */
static void hit(uint32_t off, uint32_t n)
{
    if (!watch)
        return;
    if ((off >= 0xC8000u && off + n <= 0xCE000u) || (off >= 0xDC000u && off + n <= 0xE0000u) ||
        (off >= 0x9F000u && off + n <= 0xA0000u) || (off >= 0xFE000u && off + n <= 0xFF000u))
        wrote += n;
    else
        stray += n;
}
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    if (dead)
        return -8;
    hit(off, 4096u);
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if ((off & 0xFFu) + n > 256u)
        return -1;
    hit(off, n);
    for (i = 0; i < n; i++) {
        if (dead || (cut_bytes >= 0 && cut_bytes-- == 0)) {
            dead = 1;
            return -9;
        }
        nor[off + i] &= s[i];
    }
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static int bad;
static void ck(const char *what, int ok)
{
    printf("patch: %-108s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void power_cycle(void)                       /* the RAM is gone; the flash read again at power-on */
{
    dead = 0;
    cut_bytes = -1;
    ui_power_on();
    flash_ok = 1;
    pt_boot();
    frame();
}
static uint32_t pt_bytes(void)
{
    uint32_t c, n = 0;
    for (c = 0; c < PT_NCH; c++)
        n += pt_len[c];
    return n;
}
/* part t as the biggest record there can be: FM6 with its patch, every value off its default, the arp in SEQ mode with all sixteen
 * values and sixteen full steps; the caller sets up_shmk / up_mblk (shadow marker, End B) for the rest */
static void worst_part(track_t *t, uint32_t idx)
{
    uint32_t i;
    set_engine_of(t, ENGI_FM6);
    apply_preset_to(t, 1u);
    for (i = 0; i < P_COUNT; i++) {
        const param_desc_t *d = param_desc_of(ENGI_FM6, i);
        t->p[i] = (int16_t)(d->def + 1 <= d->max ? d->def + 1 : d->def - 1);
    }
    t->p[P_AMODE] = AM_SEQ;
    t->p[P_SLEN] = 15;
    for (i = 0; i < 16u; i++) {
        step_t *s = &t->step[i];
        step_clear(s);
        s->note[0] = (uint8_t)(40 + (i * 5u + idx) % 40u);
        s->n = 1;
        s->time = ST_NOTE;
        s->flags = (uint8_t)(SF_ACCENT | SF_SLIDE);
        s->vel = (uint8_t)(50 + i);
        s->probability = (uint8_t)(30 + i);
        step_set_nudge(s, (int32_t)i % 5 - 2);
        step_set_ratchet(s, 2u);
    }
    fm6_patch[(uint32_t)(t - trk) % NTRK][6] = (uint8_t)(idx % 99u);
}

/* how many of the two LFO TRIGs a stored record holds (pairs, or the bitmap form's set bits); pr: the values' start (pt_layout) */
static uint32_t trig_stored(const uint8_t *raw, uint32_t pr)
{
    uint32_t j, n = 0;
    if (raw[4] & PTF_BM) {
        n += raw[pr + P_LTRIG / 8u] >> (P_LTRIG % 8u) & 1u;
        n += raw[pr + P_L2TRIG / 8u] >> (P_L2TRIG % 8u) & 1u;
    } else {
        for (j = 0; j < raw[19]; j++)
            n += raw[pr + 2u * j] == P_LTRIG || raw[pr + 2u * j] == P_L2TRIG;
    }
    return n;
}

int main(void)
{
    uint32_t k, n, i, ok, total, occ, e, kk, rec0;
    int16_t was[P_COUNT];
    uint8_t pk[FM6_PACKED], pk2[FM6_PACKED];
    memset(nor, 0xFF, sizeof nor);
    power_cycle();
    song.playing = 0;

    /* 1: the store */
    ck("power-on: no patch stored, no chunk", up_count() == 0 && pt_bytes() == 0);
    set_engine_of(&trk[0], 0u);
    apply_preset_to(&trk[0], 3u);
    watch = 1;
    ck("an INIT-like sound stores (rc 0 or 3)", up_store_trk(&trk[0], 7u, "TEST A", CAT_BASS) == 0);
    watch = 0;
    rec0 = pt_bytes();
    ck("  one sparse record: small (under 140 bytes with its chunk header), written in the patch store only",
       up_used(7) && rec0 > PT_HDR + PT_FIX && rec0 < 140u && wrote && !stray);
    trk[0].p[P_E4] = (int16_t)(trk[0].p[P_E4] + 9);
    trk[0].p[P_LEVEL] = 99;
    for (i = 0; i < P_COUNT; i++)
        was[i] = motion_base_value(&trk[0], i);
    up_mblk = (const uint8_t[]){'M', 1, 2, 25, 3, 0, 77};
    ck("overwrite the same slot with other values + an End B block", up_store_trk(&trk[0], 7u, "TEST A", CAT_BASS) == 0);
    up_mblk = 0;
    power_cycle();
    {
        const up_rec_t *r = up_rec(7);
        ok = up_used(7) && up_count() == 1u && str_eq(r->name, "TEST A") && up_cat(7) == CAT_BASS && r->engine == 0u &&
             r->packed[UP_MB_AT] == 'M' && r->packed[UP_MB_AT + 6] == 77;
        for (i = 0; i < P_COUNT; i++)
            ok &= up_value(r, i) == was[i];
    }
    ck("after a power cycle: the same name, category, every value, End B block; still one record", ok);
    set_engine_of(&trk[1], ENGI_FM6);
    apply_preset_to(&trk[1], 2u);
    fm6_patch[1][5] = (uint8_t)((fm6_patch[1][5] + 11u) % 99u);
    fm6_adopt(1);
    ck("an FM6 sound stores with its patch inside the record", up_store_trk(&trk[1], 9u, "TEST FM", 0) == 0 && upf_fm6(9));
    fm6_pack(fm6_patch[1], pk);
    power_cycle();
    ck("  after a power cycle the patch is the same (upf_get)", !upf_get(9, pk2) && !memcmp(pk, pk2, sizeof pk));
    kk = pt_bytes();
    for (n = 0; n < 2u; n++) {                        /* a cut in the payload (n 0) and one in the commit record, the last write (n 1) */
        cut_bytes = n ? (long)pt_len[0] + 8 : 100;
        ck(n ? "a power cut in the commit record of an overwrite: SAVE ERROR" : "a power cut in the payload of an overwrite: SAVE ERROR",
           up_store_trk(&trk[0], 7u, "CUT", 0) != 0);
        power_cycle();
        ck("  after the power comes back: both records as they were", up_count() == 2u && str_eq(up_rec(7)->name, "TEST A") &&
           up_used(9) && pt_bytes() == kk);
    }
    ck("erase: slot 7 empty, slot 9 kept (through a power cycle too)", up_put(7, 0) == 0 && !up_used(7) && up_used(9));
    power_cycle();
    ck("  after a power cycle", !up_used(7) && up_used(9) && up_count() == 1u);

    {   /* a move to another chunk cut short: the slot in two chunks, the newer revision wins (the newer one is written first) */
        up_rec_t r = *up_rec(9);
        uint32_t rev = pt_raw(9)[3];
        char nm1[13];
        memset(r.name, 0, 12);
        memcpy(r.name, "MOVED", 5);
        pt_append(1, pt_encode(9, rev + 1u, &r, 0));
        pt_scan();
        up_name(9, nm1);
        ok = str_eq(nm1, "MOVED") && pt_idx[9] >> 12 == 1u;
        ok &= pt_len[0] < kk;                              /* (the older one is out of the RAM image) */
        pt_boot();                                         /* (flash was not touched: both chunks as they were) */
        pt_len[1] = 0;
        memset(r.name, 0, 12);
        memcpy(r.name, "OLDER", 5);
        pt_append(1, pt_encode(9, rev - 1u, &r, 0));
        pt_scan();
        up_name(9, nm1);
        ck("the same slot in two chunks: the newer revision is the record (either order), not the older", ok && str_eq(nm1, "TEST FM") && pt_idx[9] >> 12 == 0u);
        pt_len[1] = 0;
        pt_scan();
    }


    /* 1b: the arp, the chord / scale keys and the SEQ pattern travel with a patch (upreset.c, the AK block) */
    {
        track_t *t = &trk[0];
        uint32_t per_chunk = PT_CAP - PT_HDR, a_slot = 40u, b_slot = 41u, fe = 0, fk = 0, bytes_old, bytes_new, sz;
        int16_t want[16], mark[16];
        step_t st0[NSTEP];
        const uint8_t IDS[16] = {P_AMODE, P_ARATE, P_AOCT, P_AGATE, P_ASWING, P_APROB, P_AHOLD, P_AORDER, P_SCALE, P_CHRD, P_VOIC,
                                 P_SLEN, P_SDIV, P_SSWING, P_SGATE, P_SQNT};
        int same;
        set_engine_of(t, 0u);
        apply_preset_to(t, 3u);
        t->p[P_ROOT] = 5;
        t->p[P_AMODE] = AM_SEQ;
        t->p[P_ARATE] = 3; t->p[P_AOCT] = 2; t->p[P_AGATE] = 40; t->p[P_ASWING] = 17; t->p[P_APROB] = 91; t->p[P_AHOLD] = 1;
        t->p[P_AORDER] = 1; t->p[P_SCALE] = 2; t->p[P_CHRD] = 3; t->p[P_VOIC] = 1;
        t->p[P_SLEN] = 12; t->p[P_SDIV] = 2; t->p[P_SSWING] = 31; t->p[P_SGATE] = 71; t->p[P_SQNT] = 0;
        t->p[P_ATGT] = 3;
        for (i = 0; i < NSTEP; i++)
            step_clear(&t->step[i]);
        for (i = 0; i < 12u; i += 1) {
            step_t *s = &t->step[i];
            if (i % 5u == 4u)
                continue;                                    /* rests */
            s->note[0] = (uint8_t)(60 + (i * 7u) % 17u);
            s->n = 1;
            s->time = ST_NOTE;
            s->flags = (uint8_t)(i % 3u == 0 ? SF_ACCENT : 0);
            s->vel = (uint8_t)(70 + i);
            s->probability = (uint8_t)(i == 7u ? 60u : 0u);
            step_set_nudge(s, (int32_t)i % 4 - 2);
            step_set_ratchet(s, i == 9u ? 3u : 1u);
        }
        for (i = 0; i < 16u; i++)
            want[i] = t->p[IDS[i]];
        memcpy(st0, t->step, sizeof st0);
        ck("save a part with the arp in SEQ mode and a pattern", up_store_trk(t, a_slot, "SEQ ARP", 0) == 0 && pt_raw(a_slot)[4] & PTF_AK);
        {   /* record sizes: the same patch with the block (new) and with the arp ids as plain pairs (before) */
            up_rec_t r = *up_rec(a_slot);
            const uint8_t *raw = pt_raw(a_slot);
            bytes_new = pt_rd16(raw);
            up_ak_in = 0;
            bytes_old = pt_encode(a_slot, 0, &r, 0);
            up_ak_in = 0;
            printf("patch: record bytes with the SEQ arp block %u, before (arp ids as pairs + 32-byte pattern) %u\n", bytes_new, bytes_old);
        }
        power_cycle();
        ck("  after a power cycle the record is there", up_used(a_slot));
        t = &trk[0];
        set_engine_of(t, 0u);
        apply_preset_to(t, 3u);
        song.playing = 0;
        /* the player's own arp and steps, then the patch loads over them */
        t->p[P_ROOT] = 5;
        t->p[P_AMODE] = 1; t->p[P_ARATE] = 5; t->p[P_SLEN] = 16; t->p[P_SCALE] = 0; t->p[P_CHRD] = 0;
        for (i = 0; i < 16u; i++)
            mark[i] = t->p[IDS[i]];
        for (i = 0; i < NSTEP; i++)
            step_clear(&t->step[i]);
        t->step[2].note[0] = 55; t->step[2].n = 1; t->step[2].time = ST_NOTE; t->step[2].vel = 99;
        t->step[40].note[0] = 50; t->step[40].n = 1; t->step[40].time = ST_NOTE; t->step[40].vel = 99;
        {
            step_t own[NSTEP];
            memcpy(own, t->step, sizeof own);
            up_load(a_slot);
            same = 1;
            for (i = 0; i < 16u; i++)
                if (IDS[i] != P_SCALE)
                    same &= t->p[IDS[i]] == want[i];
            ck("  load: every arp / keys / sequencer value as saved (mode SEQ, rate, octaves, gate, swing, prob, hold, order, CHRD VOIC, LEN DIV SWING GATE QUANTIZE)", same && t->p[P_ATGT] == 3);
            ck("  ROOT stays the player's", t->p[P_ROOT] == 5);
            {   /* no patch stores or applies a scale (core.h: the scale is the instrument's) */
                ak_t a;
                int has = ak_get(a_slot, &a);
                ck("  the record holds no SCALE (the part's SCALE 2 was not stored), the load applied none, the global scale is untouched",
                   has && (a.g & AK_KEYS) && a.key[0] == TP[P_SCALE].def && up_value(up_rec(a_slot), P_SCALE) == TP[P_SCALE].def &&
                   t->p[P_SCALE] == mark[8] && scale_g == 0 && root_g == 0 && offscale_g == OS_SNAP);
            }
            same = 1;
            for (i = 0; i < 16u; i++)
                same &= !memcmp(&t->step[i], &st0[i], sizeof(step_t));
            ck("  the 16 steps as saved (notes, accents, velocity, chance, nudge, ratchet, rests)", same);
            ck("  steps 17..64 stay the player's", !memcmp(&t->step[16], &own[16], sizeof(step_t) * 48u));
            /* the lend rule: a patch without the block gives the player's back */
            apply_preset_to(t, 1u);
            same = 1;
            for (i = 0; i < 16u; i++)
                same &= t->p[IDS[i]] == mark[i];
            ck("  the next sound loaded (no arp of its own) gives the player's arp and keys back", same && !memcmp(t->step, own, sizeof own));
            up_load(a_slot);
            t->p[P_ARATE] = 9;                                  /* edited while lent: the edit stays, the pattern comes back */
            t->step[3].note[0] = 77;
            apply_preset_to(t, 1u);
            ck("  edited after the load: the player's edit stays (arp rate, pattern)", t->p[P_ARATE] == 9 && t->step[3].note[0] == 77);
        }
        /* a patch with the arp off and a default sequencer stores nothing extra; an older record leaves the arp alone */
        apply_preset_to(t, 2u);
        for (i = 0; i < 16u; i++)
            t->p[IDS[i]] = TP[IDS[i]].def;
        up_store_trk(t, b_slot, "PLAIN", 0);
        {
            up_rec_t r = *up_rec(b_slot);
            up_ak_in = 0;
            ck("arp off + default sequencer: no AK block, the record is the same as ever", !(pt_raw(b_slot)[4] & PTF_AK) &&
               pt_encode(b_slot, pt_raw(b_slot)[3], &r, 0) == pt_rd16(pt_raw(b_slot)));
        }
        t->p[P_AMODE] = 2; t->p[P_ARATE] = 6; t->p[P_CHRD] = 2; t->p[P_SLEN] = 9;
        memset(mark, 0, sizeof mark);
        for (i = 0; i < 16u; i++)
            mark[i] = t->p[IDS[i]];
        memcpy(st0, t->step, sizeof st0);
        up_load(b_slot);
        same = 1;
        for (i = 0; i < 16u; i++)
            same &= t->p[IDS[i]] == mark[i];
        ck("an older record (no block) loads without touching the current arp, keys, LEN or steps", same && !memcmp(st0, t->step, sizeof st0));
        /* a rename keeps the block; the editor's UP_PUT (no block) drops it */
        ck("RENAME keeps the block", up_rename(a_slot, "SEQ ARP 2") == 0 && (pt_raw(a_slot)[4] & PTF_AK) && ak_get(a_slot, &(ak_t){0}) );
        {
            ak_t a;
            ok = ak_get(a_slot, &a) && a.g == (AK_ARP | AK_KEYS | AK_SEQ) && a.arp[0] == AM_SEQ && a.seq[0] == 12 && !memcmp(&a.st[0], &st0[0], 0);
            ck("  and it still decodes (SEQ, LEN 12)", ok);
        }
        /* a shadow of a factory sound that sets an arp: the player's edit (arp off) is what is saved and loaded */
        for (i = 0; i < DS_N; i++)
            if (DS_SOUNDS[i].flags & DSF_ARP) {
                fe = DS_SOUNDS[i].eng;
                fk = i - DS_ENG_AT[fe];
                break;
            }
        set_engine_of(t, fe);
        apply_preset_to(t, fk);
        ok = t->p[P_AMODE] != 0;
        t->p[P_AMODE] = 0;
        up_shmk[0] = (uint8_t)(fe + 1u);
        up_shmk[1] = (uint8_t)(fk + 1u);
        up_store_trk(t, 42u, "SHADOW", 0);
        up_shmk[0] = up_shmk[1] = 0;
        apply_preset_to(t, fk);                                 /* the factory sound again: its arp is on */
        ok &= t->p[P_AMODE] != 0;
        up_load(42u);
        ck("a shadow copy saved with the factory arp switched off loads with the arp off (not the factory's)", ok && t->p[P_AMODE] == 0);
        apply_preset_to(t, fk);
        t->p[P_ARATE] = (int16_t)(t->p[P_ARATE] == 4 ? 5 : 4);
        want[0] = t->p[P_ARATE];
        up_shmk[0] = (uint8_t)(fe + 1u);
        up_shmk[1] = (uint8_t)(fk + 1u);
        up_store_trk(t, 42u, "SHADOW", 0);
        up_shmk[0] = up_shmk[1] = 0;
        apply_preset_to(t, fk);
        up_load(42u);
        ck("  an edited rate is saved and loads (not the factory's)", t->p[P_ARATE] == want[0] && t->p[P_AMODE] != 0);
        /* record sizes: a plain sound, with an ordinary arp, with a SEQ arp */
        apply_preset_to(t, 2u);                                   /* a plain sound: its record; then with an ordinary arp (UP, 3 values changed) */
        for (i = 0; i < NSTEP; i++)
            step_clear(&t->step[i]);
        for (i = 0; i < 16u; i++)
            t->p[IDS[i]] = TP[IDS[i]].def;
        up_store_trk(t, 43u, "PLAIN2", 0);
        sz = pt_rd16(pt_raw(43u));
        t->p[P_AMODE] = 1; t->p[P_ARATE] = 5; t->p[P_AOCT] = 2; t->p[P_AGATE] = 60;
        up_store_trk(t, 43u, "ARP2", 0);
        {
            up_rec_t r = *up_rec(43u);
            uint32_t nw = pt_rd16(pt_raw(43u));
            up_ak_in = 0;
            printf("patch: record sizes: a plain sound %u bytes; the same with an ordinary arp %u (arp ids as pairs: %u); with a SEQ arp of 9 steps %u (as plain pairs with the 16-note pattern, no block: %u)\n",
                   sz, nw, pt_encode(43u, 0, &r, 0), bytes_new, bytes_old);
        }
        up_put(43u, 0);
        up_put(a_slot, 0); up_put(b_slot, 0); up_put(42u, 0);
        up_shmk[0] = up_shmk[1] = 0;
    }

    /* 1c: an OLD record that holds a SCALE (ordinal 8 of its AK block) is read and the scale discarded; CHRD still applies */
    {
        static const uint8_t blk[7] = {5, 0, 2 | 8, 0x00, 0x03, 64 + 5, 64 + 4};   /* g KEYS|BM, bitmap bits 8 (SCALE) 9 (CHRD), values 5, 4 */
        track_t *t = &trk[0];
        up_rec_t r;
        uint32_t rc;
        ak_t a;
        set_engine_of(t, 0u);
        apply_preset_to(t, 3u);
        r = *up_rec(60u);                                    /* (an unused slot: all zero) */
        memset(&r, 0, sizeof r);
        r.used = UP_USED; r.ver = UP_VER; r.engine = 0; r.np = P_COUNT;
        memcpy(r.name, "OLD KEYS", 9);
        for (i = 0; i < P_COUNT; i++) up_set_value(&r, i, param_desc_of(0, i)->def);
        up_ak_in = blk;
        rc = (uint32_t)up_put(44u, &r);
        up_ak_in = 0;
        scale_g = 3; root_g = 2;
        t->p[P_SCALE] = 0; t->p[P_CHRD] = 0;
        ck("an old record with a SCALE in its keys block still reads (the parse as it was)", rc == 0u && ak_get(44u, &a) && a.key[0] == 5 && a.key[1] == 4);
        up_load(44u);
        ck("  loading it: CHRD applies, the SCALE is discarded (the part's and the global scale unchanged)",
           t->p[P_CHRD] == 4 && t->p[P_SCALE] == 0 && scale_g == 3 && root_g == 2);
        scale_g = 0; root_g = 0;
        up_put(44u, 0);
    }

    /* 2: the limit (PT_MAX worst-case records always fit) */
    {
        uint32_t maxrec = 0, mx = 0, c, per = 0;
        up_shmk[0] = up_shmk[1] = 0;
        for (k = 0; k < UP_SLOTS; k++)
            up_put(k, 0);
        printf("patch: PT_WORST %u bytes (SEQ-arp FM6 record %u, plain %u), PT_K %u per chunk, %u chunks, PT_MAX %u\n",
               (unsigned)PT_WORST, (unsigned)PT_W_SEQ, (unsigned)PT_W_PLAIN, (unsigned)PT_K, (unsigned)PT_NCH, (unsigned)PT_MAX);
        ck("PT_MAX = chunks x records per chunk, within the slots", PT_MAX == PT_NCH * PT_K && PT_MAX <= UP_SLOTS && PT_K * PT_WORST + PT_HDR <= PT_CAP);
        /* A: PT_MAX small records first (slots 0, 2, 4 ..), the next new one has no place */
        set_engine_of(&trk[0], 0u);
        apply_preset_to(&trk[0], 2u);
        for (n = 0, k = 0; k < PT_MAX; k++) {
            char nmx[8] = {'S', (char)('0' + k / 10u), (char)('0' + k % 10u), 0};
            trk[0].p[P_LEVEL] = (int16_t)(40 + k);
            n += up_store_trk(&trk[0], 2u * k, nmx, 0) == 0;
        }
        ck("PT_MAX plain records store (no STORE FULL below PT_MAX)", n == PT_MAX && up_count() == PT_MAX);
        for (c = 0; c < PT_NCH; c++)
            mx = pt_cnt(c) > mx ? pt_cnt(c) : mx;
        ck("  no chunk holds more than PT_K records", mx <= PT_K);
        kk = pt_bytes();
        watch = 1;
        wrote = stray = 0;
        k = up_store_trk(&trk[0], 1u, "ONE TOO MANY", 0);
        ck("  the next NEW patch answers NO FREE PLACE and writes nothing", k == PT_NOFREE && !wrote && !stray && pt_bytes() == kk && up_count() == PT_MAX);
        watch = 0;
        /* B: every one of them overwritten with the worst record, in place: all fit whatever the first fill looked like */
        for (n = 0, k = 0; k < PT_MAX; k++) {
            char nmx[8] = {'W', (char)('0' + k / 10u), (char)('0' + k % 10u), 0};
            worst_part(&trk[1], k);
            up_shmk[0] = (uint8_t)(ENGI_FM6 + 1u);
            up_shmk[1] = 1u;
            up_mblk = (const uint8_t[]){'M', 1, 2, 25, 3, 0, 77};
            fm6_adopt(1);
            n += up_store_trk(&trk[1], 2u * k, nmx, 0) == 0;
            up_mblk = 0;
            up_shmk[0] = up_shmk[1] = 0;
            maxrec = pt_rd16(pt_raw(2u * k)) > maxrec ? pt_rd16(pt_raw(2u * k)) : maxrec;
        }
        printf("patch: the biggest record stored: %u bytes (PT_WORST %u)\n", maxrec, (unsigned)PT_WORST);
        ck("PT_MAX worst-case records overwrite the small ones in place: all store, none says STORE FULL", n == PT_MAX && up_count() == PT_MAX);
        ck("  the worst record really is about PT_WORST (the bound is tight) and never above it", maxrec <= PT_WORST && maxrec + 8u >= PT_WORST);
        for (c = 0; c < PT_NCH; c++)
            mx = pt_cnt(c) > mx ? pt_cnt(c) : mx;
        ck("  every chunk within its PT_K records and its bytes", mx <= PT_K && pt_len[0] <= PT_CAP && pt_len[PT_NCH - 1u] <= PT_CAP);
        for (c = 0; c < PT_NCH; c++)
            per += pt_len[c] > PT_HDR;
        power_cycle();
        ck("  after a power cycle: all of them there, the FM6 patch of each intact, six chunks used", up_count() == PT_MAX && per == PT_NCH);
        for (ok = 1, k = 0; k < PT_MAX; k++) {
            const up_rec_t *r = up_rec(2u * k);
            ak_t a;
            ok &= r->engine == ENGI_FM6 && !upf_get(2u * k, pk2) && pk2[6] == (uint8_t)(k % 99u) && ak_get(2u * k, &a) &&
                  a.g == (AK_ARP | AK_KEYS | AK_SEQ) && a.seq[0] == 15;
        }
        ck("  and each decodes (FM6 patch byte, the arp / keys / SEQ block)", ok);
        for (k = 0; k < PT_MAX; k++)
            up_put(2u * k, 0);
        ck("  all erased", up_count() == 0u);
        /* C: the last save torn by a power cut: that one is lost, the others stay; then it works */
        for (k = 0; k + 1u < PT_MAX; k++) {
            worst_part(&trk[1], k);
            up_shmk[0] = (uint8_t)(ENGI_FM6 + 1u);
            up_shmk[1] = 1u;
            up_mblk = (const uint8_t[]){'M', 1, 2, 25, 3, 0, 77};
            fm6_adopt(1);
            up_store_trk(&trk[1], k, "W", 0);
            up_mblk = 0;
            up_shmk[0] = up_shmk[1] = 0;
        }
        ck("PT_MAX - 1 worst-case records stored", up_count() == PT_MAX - 1u);
        worst_part(&trk[1], PT_MAX - 1u);
        fm6_adopt(1);
        for (n = 0; n < 2u; n++) {
            cut_bytes = n ? (long)(PT_WORST + PT_HDR + 8u) : 100;
            ck(n ? "a power cut in the last save's payload (the PT_MAX-th): SAVE ERROR" : "a power cut at the start of the last save: SAVE ERROR",
               up_store_trk(&trk[1], 100u, "LAST", 0) != 0);
            power_cycle();
            ck("  after the power comes back: the other PT_MAX - 1 records, the last one not stored", up_count() == PT_MAX - 1u && !up_used(100));
            worst_part(&trk[1], PT_MAX - 1u);
            fm6_adopt(1);
        }
        ck("  the same save then works: PT_MAX stored", up_store_trk(&trk[1], 100u, "LAST", 0) == 0 && up_count() == PT_MAX);
        power_cycle();
        ck("  after a power cycle: PT_MAX records, the last one too", up_count() == PT_MAX && up_used(100));
        worst_part(&trk[1], 5u);
        fm6_adopt(1);
        ck("an overwrite of a stored one at PT_MAX still works (in place)", up_store_trk(&trk[1], 100u, "LAST 2", 0) == 0 && up_count() == PT_MAX);
        ck("  an erase makes a place: the next new patch then works", up_put(0, 0) == 0 && up_store_trk(&trk[1], 0u, "AGAIN", 0) == 0 && up_count() == PT_MAX);
        /* D: the page at PT_MAX: no empty place; SAVE held on a factory sound without a copy says NO FREE PLACE at once, no dialog */
        power_cycle();
        song.playing = 0;
        set_engine_of(&trk[0], 0u);
        apply_preset_to(&trk[0], 1u);
        go_title("PATCHES");
        frame();
        (void)preset_pos(&occ);
        ck("at PT_MAX the list has no empty place and FREE is 0", pl_empties() == 0u && up_free_places() == 0u && pl_cursor(&total) < 0xFFFFFFFFu &&
           total == occ);
        hold(B_SAVE);
        ck("hold SAVE on a factory sound (needs a new place): NO FREE PLACE, no dialog, nothing written", msg_is("NO FREE PLACE") && !ui.confirm && up_count() == PT_MAX);
        for (k = 0; k < UP_SLOTS; k++)
            up_put(k, 0);
        ck("  all erased", up_count() == 0u);
        /* E: a store written under other rules (60 records, 10 in each of six chunks: more than PT_MAX): readable, no new patch */
        set_engine_of(&trk[0], 0u);
        apply_preset_to(&trk[0], 2u);
        up_store_trk(&trk[0], 0u, "OLD", 0);
        {
            up_rec_t r = *up_rec(0);
            up_put(0, 0);
            for (k = 0; k < 60u; k++)
                pt_append(k / 10u, pt_encode(k, 1u, &r, 0));
            for (c = 0; c < PT_NCH; c++)
                pt_commit(c);
        }
        power_cycle();
        ck("a store with more records than PT_MAX (60) stays readable after a power cycle", up_count() == 60u && up_used(59) && up_free_places() == 0u);
        ck("  a new patch is refused (NO FREE PLACE), writing nothing", up_store_trk(&trk[0], 70u, "NEW", 0) == PT_NOFREE && up_count() == 60u);
        ck("  a stored one is still rewritten in place, and erased", up_store_trk(&trk[0], 5u, "SAME", 0) == 0 && up_put(5, 0) == 0 && up_count() == 59u &&
           up_store_trk(&trk[0], 70u, "NEW", 0) == PT_NOFREE);
        for (k = 0; k < UP_SLOTS; k++)
            up_put(k, 0);
        {   /* an AK block of the first form (np, then (id, value) pairs: records written before the bitmap form) still decodes */
            static const uint8_t old[] = {AK_ARP, 1, P_ARATE, 5 + 64};
            ak_t a;
            ck("an AK block in the pairs form (older records, and up to two values) still decodes", ak_dec(old, sizeof old, 0u, &a) && a.g == AK_ARP &&
               a.arp[P_ARATE - P_AMODE] == 5);
        }
    }

    /* 3: the page */
    power_cycle();
    song.playing = 0;
    set_engine_of(&trk[0], 0u);
    apply_preset_to(&trk[0], 0u);
    ui_power_on();
    go_home();
    press(B_SAVE);
    ck("a tap of SAVE opens PATCHES (from HOME); again: the next SAVE page", cur_page()->graph == GR_BROWSE && !ui.home);
    press(B_SAVE);
    ck("  PATTERNS", cur_page()->graph == GR_PATS);
    power_cycle();
    song.playing = 0;
    set_engine_of(&trk[0], 0u);
    apply_preset_to(&trk[0], 0u);
    go_title("PATCHES");
    frame();
    ck("SAVE's first page is PATCHES; the SAVE family: PATCHES PATTERNS MULTIS TOOLS", cur_page()->graph == GR_BROWSE &&
       cur_page()->fam == FAM_SAVE && str_eq(PAGES[ui.page + 1u].title, "PATTERNS") && str_eq(PAGES[ui.page + 2u].title, "MULTIS") &&
       str_eq(PAGES[ui.page + 3u].title, "TOOLS"));
    (void)pl_cursor(&total);
    (void)preset_pos(&occ);
    ck("ONE list: the sounds first, then three empty places (FREE says all PT_MAX)", total == occ + 3u && occ == ds_list_n() && pl_empties() == 3u &&
       up_free_places() == PT_MAX);
    for (i = 0; i < 200u && !ui.pe; i++)
        turn(EN_K1, 1);
    ok = ui.pe == 1u && pl_cursor(&total) == occ;
    {
        int16_t p1[P_COUNT];
        uint32_t u0 = trk[0].user, pr = trk[0].preset;
        memcpy(p1, trk[0].p, sizeof p1);
        turn(EN_K1, 1);
        ok &= ui.pe == 2u && !memcmp(p1, trk[0].p, sizeof p1) && trk[0].user == u0 && trk[0].preset == pr;
        turn(EN_K1, -1);
    }
    ck("K1 on past the sounds: the first empty place is selected, then the next; nothing is loaded", ok);
    for (i = 0; i < 4u; i++)
        turn(EN_K4, 1);
    ck("K4 ERASE on an empty place: EMPTY PLACE", msg_is("EMPTY PLACE") && !ui.confirm);
    hold(B_SAVE);
    ck("hold SAVE on an empty place: the NAME screen (a new patch), not the SAVE OVER dialog", name_on() && nm.kind == NK_PATCH_NEW && !ui.confirm);
    str_cpy(nm.s, "NEW ONE", sizeof nm.s);
    nm.len = nm.cur = 7;
    press(B_OCTUP);
    ck("  OCT+: SAVED; a patch of its own, the part plays it, one place less empty", msg_is("SAVED") && up_count() == 1u && up_plain_count() == 1u &&
       user_of(TSEL) < UP_SLOTS && pl_empties() == 3u && up_free_places() == PT_MAX - 1u);
    k = user_of(TSEL);
    ck("  the cursor follows the patch (after the factory sounds); TYPE ALL lists one empty place less", !ui.pe && pl_cursor(&total) == occ && total == occ + 1u + 3u);
    {
        uint32_t eng0 = TSEL->eng_req;
        for (i = 0; i < 40u && list_mode() <= CAT_N; i++)
            turn(EN_K2, 1);
        ok = list_mode() > CAT_N && TSEL->eng_req == eng0;
        n = list_mode() - CAT_N - 1u;
        (void)preset_pos(&total);
        for (i = 0; i < total; i++) {
            e = preset_at(i, &kk);
            ok &= sound_eng(e, kk) == eng_vis(n);
        }
        ck("K2 TYPE: past the categories one entry per engine; a filter only (the part keeps its engine), the list holds that engine's sounds", ok);
        for (i = 0; i < 40u && list_mode(); i++)
            turn(EN_K2, -1);
        ck("  K2 back to ALL", !list_mode());
    }
    go_title("PATCHES");
    set_engine_of(&trk[0], 0u);
    apply_preset_to(&trk[0], 1u);
    frame();
    turn(EN_K3, 1);
    ck("K3 FAV marks the cursor's sound", preset_favorite());
    turn(EN_K4, 1);
    ck("K4 ERASE on a factory sound with no copy: ALREADY FACTORY", msg_is("ALREADY FACTORY") && !ui.confirm);
    trk[0].p[P_E4] = (int16_t)(trk[0].p[P_E4] + 7);
    hold(B_SAVE);
    ck("hold SAVE on a factory sound: SAVE OVER <name>?", ui.confirm == CF_SAVE_OVER);
    press(B_OCTUP);
    ck("  OCT+: its copy shadows it in its place (the list is as long, FAV kept)", msg_is("SAVED") && up_count() == 2u && up_plain_count() == 1u &&
       pl_cursor(&total) < occ && total == occ + 1u + 3u && up_free_places() == PT_MAX - 2u && preset_favorite());
    kk = user_of(TSEL);
    turn(EN_K4, 1);
    ck("K4 ERASE on the copy: the dialog RESTORE FACTORY?", ui.confirm == CF_ERASE_USER && up_shadow(kk, &e) < NENGINES);
    press(B_OCTUP);
    ck("  OCT+: FACTORY RESTORED: the copy gone, the part plays the factory sound again, the place is the same",
       msg_is("FACTORY RESTORED") && up_count() == 1u && !up_used(kk) && user_of(TSEL) >= UP_SLOTS &&
       trk[0].p[P_E4] == ds_value(ds_get(0u, 1u), P_E4, TP[P_E4].def) && pl_cursor(&total) < occ);
    k = up_nth(0);
    power_cycle();
    ck("after a power cycle: the user patch is there, the restored copy is not", up_count() == 1u && up_used(k) && up_plain(k));
    go_title("PATCHES");
    (void)preset_pos(&occ);
    preset_go(occ - 1u);                            /* the user patch: the last sound of the list */
    turn(EN_K4, 1);
    ck("K4 ERASE on a patch of its own: ERASE PATCH? then it is emptied", ui.confirm == CF_ERASE_USER);
    press(B_OCTUP);
    ck("  OCT+: erased, its place empty, the cursor on the first empty place", !up_used(k) && up_count() == 0u && ui.pe == 1u);


    {   /* owner bug 2026-10-12: SAVE, then EDIT (or any page button) must leave, saving nothing, from every SAVE page, a dialog and NAME */
        static const struct { uint8_t b; uint8_t fam; } LV[] = {{B_EDIT, FAM_EDIT}, {B_FX, FAM_FX}, {B_ENV, FAM_ENV}, {B_LFO, FAM_LFO},
            {B_ARP, FAM_ARP}, {B_SEQ, FAM_SEQ}, {B_SCL, FAM_SCL}, {B_GLO, FAM_GLO}, {B_HOME, FAM_TRK}};
        static uint8_t img[sizeof nor];
        uint32_t s, j, good = 0, tried = 0;
        for (s = 0; s < 7u; s++)
            for (j = 0; j < sizeof LV / sizeof LV[0]; j++) {
                int left, where_ok;
                ui.confirm = 0;
                if (name_on()) name_close();
                frames(400);
                go_main();
                press(B_SAVE);
                if (s == 1u || s == 5u || s == 6u) {           /* PATCHES: s 1 an empty place, 5 a dialog, 6 NAME */
                    (void)preset_pos(&occ);
                    if (s == 1u || s == 6u) preset_go(occ + 0u + (up_count() ? 1u : 0u)), (void)0;
                    if (s == 6u) { ui.pe = 1; hold(B_SAVE); }
                    if (s == 5u) { preset_go(1u); hold(B_SAVE); }
                } else if (s == 2u) {
                    press(B_SAVE);
                } else if (s == 3u) {
                    press(B_SAVE); press(B_SAVE);
                } else if (s == 4u) {
                    press(B_SAVE); press(B_SAVE); press(B_SAVE);
                }
                where_ok = s == 5u ? ui.confirm != 0 : s == 6u ? name_on() : 1;
                memcpy(img, nor, sizeof img);
                frames(400);
                press(LV[j].b);
                frames(100);
                left = !ui.confirm && !name_on() && (LV[j].fam == FAM_TRK ? (!ui.home && cur_page()->fam == FAM_TRK) || ui.home :
                       cur_page()->fam == LV[j].fam);
                tried++;
                if (where_ok && left && !memcmp(img, nor, sizeof img))
                    good++;
                else
                    printf("   state %u button %u: dialog/name %d left %d fam %d\n", (unsigned)s, (unsigned)LV[j].b, where_ok, left, (int)cur_page()->fam);
            }
        ck("SAVE pages (PATCHES, empty place, PATTERNS, MULTIS, TOOLS), a dialog, NAME: EDIT FX ENV LFO ARP SEQ SCL GLO HOME all leave, flash unchanged",
           good == tried);
    }

    /* 1c: LFO TRIG (DUELL 0.2.2): FREE is the default, but the sparse store keeps NOTE as its own default for the two ids (upreset.c
     * up_def): a NOTE patch has no TRIG pair, exactly as every patch stored before; a FREE patch stores one; both load as stored */
    {
        track_t *t = &trk[0];
        uint32_t len_note, len_free, pr, a, b, c, d, e, j, pairs = 0;
        const uint8_t *raw;
        set_engine_of(t, 0u);
        apply_preset_to(t, 3u);
        ck("TRIG: a factory sound loads with both LFOs FREE (the default)", t->p[P_LTRIG] == 1 && t->p[P_L2TRIG] == 1);
        t->p[P_LTRIG] = t->p[P_L2TRIG] = 0;
        ck("TRIG: a NOTE patch stores", up_store_trk(t, 50u, "LFO NOTE", 0) == 0);
        len_note = pt_rd16(pt_raw(50u));
        raw = pt_raw(50u);
        pt_layout(raw, &a, &b, &c, &d, &e);
        pr = c;
        pairs = trig_stored(raw, pr);
        ck("  its record holds no TRIG value (the same bytes as a patch stored before 0.2.2)", pairs == 0u);
        t->p[P_LTRIG] = t->p[P_L2TRIG] = 1;
        ck("TRIG: a FREE patch stores", up_store_trk(t, 51u, "LFO FREE", 0) == 0);
        len_free = pt_rd16(pt_raw(51u));
        raw = pt_raw(51u);
        pt_layout(raw, &a, &b, &c, &d, &e);
        ck("  its record holds both TRIG values (a pair each, or two values of the bitmap form)", trig_stored(raw, c) == 2u && len_free > len_note);
        power_cycle();
        t = &trk[0];
        set_engine_of(t, 0u);
        t->p[P_LTRIG] = t->p[P_L2TRIG] = 1;
        up_load(50u);
        ck("  after a power cycle the NOTE patch loads NOTE / NOTE (an old patch does not change)", t->p[P_LTRIG] == 0 && t->p[P_L2TRIG] == 0);
        up_load(51u);
        ck("  and the FREE patch loads FREE / FREE", t->p[P_LTRIG] == 1 && t->p[P_L2TRIG] == 1);
        {
            const up_rec_t *r = up_rec(50u);
            ck("  the decoded NOTE record reads NOTE (up_rec)", up_value(r, P_LTRIG) == 0 && up_value(r, P_L2TRIG) == 0);
        }
        up_put(50u, 0);
        up_put(51u, 0);
    }

    printf(bad ? "patch: FAILED (%d)\n" : "patch: all ok\n", bad);
    return bad ? 1 : 0;
}
