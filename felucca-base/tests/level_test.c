/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.19 (owner, 2026-10-15): the level split. A part's loudness is two things, as on a real synth:
 *   PATCH TRIM   (P_TRIM)  stored WITH a patch: a factory sound's tuned level, a patch record, a multi's part; a sound load sets it, SAVE stores it, RESTORE brings the factory one
 *   MIXER LEVEL  (P_LEVEL) the player's volume on the mixer: NOT part of a patch (a load never changes it), kept with a multi and the project, default unity (112 = 0 dB)
 * and the part plays at TRIM x MIXER LEVEL (fx.c part_lvl). Checked here:
 *   1. the tables: unity, the labels, the defaults; the ids; the factory sounds' TRIM is the pair they used to call LEVEL (tools/duell_sounds.py)
 *   2. a sound load (factory, a stored patch, an engine change, INIT, UNDO) never changes the mixer level, a lowered part 2 stays lowered; it sets the trim
 *   3. SAVE stores the trim only (the record has no mixer level), RESTORE brings the factory trim, a power cycle; a record of before the split has its LEVEL read as the trim
 *   4. a multi and the project (autosave) store both; a multi load brings both back; the factory multis hold the mixer at unity
 *   5. MIDI CC7 is the mixer level (the trim stays); MIDI LEARN of the mixer level
 *   6. the audio gain is the product: the gain function, the output of two splits of one product bit for bit, mixer 0 / trim 0 silent, the +7.5 dB ceiling, unity = the old single level
 *   6b. MORPH with TARGET BOTH never moves the trim or the mixer level
 *   7. the pages: TRIM on VOICE 2/2 (K4), the MIXER's K4 is the mixer LEVEL; the mixer's page is untouched by a sound load
 *   build/host/level_test            (DUELL/host_tests.sh "level") */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include <sys/wait.h>
#include <unistd.h>
#include "ui_test.c"

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static int bad;
static void ck(const char *what, int ok)
{
    printf("level: %-112s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void fresh(void)                              /* a power-on with an empty flash */
{
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    pt_boot();
    multi_boot();
    frame();
    song.sel = 0;
}
static void cycle(void)                              /* the RAM is gone, the flash read again */
{
    ui_power_on();
    flash_ok = 1;
    pt_boot();
    multi_boot();
    frame();
    song.sel = 0;
}
/* the first browse slot whose factory sound has a trim other than the default (and its trim) */
static uint32_t slot_with_trim(uint32_t not_this, int32_t *trim)
{
    uint32_t k, e, f;
    for (k = 0; k < UP_SLOTS; k++) {
        if (k == not_this || !sl_fac(k, &e, &f))
            continue;
        *trim = ds_value(ds_get(e, f), P_TRIM, TP[P_TRIM].def);
        if (*trim != TP[P_TRIM].def && *trim > 70 && *trim < 125)
            return k;
    }
    return 0xFFFFu;
}

/* ------------------------------------------------------------------------------------------------ 1. the tables */
static void lv_tables(void)
{
    uint32_t i, n = 0, off = 0;
    ck("unity: LEVEL_Q12[112] is 4096 (0 dB)", LEVEL_Q12[112] == 4096);
    ck("the MIXER LEVEL: label LVL, 0..127, default 112 (unity)", TP[P_LEVEL].min == 0 && TP[P_LEVEL].max == 127 && TP[P_LEVEL].def == 112 && !strcmp(TP[P_LEVEL].label, "LVL"));
    ck("the PATCH TRIM: label TRIM, 0..127, default 104 (the old default level), the same dB scale", TP[P_TRIM].min == 0 && TP[P_TRIM].max == 127 && TP[P_TRIM].def == 104 &&
       !strcmp(TP[P_TRIM].label, "TRIM") && TP[P_TRIM].fmt == TP[P_LEVEL].fmt && TP[P_TRIM].fmt == F_DB);
    ck("P_COUNT 127 and P_E0 119 as before; TRIM is on the id of the legacy DUCK HOLD, between ATK and RLS of the DUCK", P_COUNT == 127 && P_E0 == 119 && P_TRIM == P_DKATK + 1 && P_DKRLS == P_TRIM + 1 && P_LEVEL == 0);
    for (i = 0; i < DS_N; i++) {
        const ds_sound_t *s = &DS_SOUNDS[i];
        n += ds_value(s, P_TRIM, -1) >= 0;
        off += ds_value(s, P_TRIM, -1) >= 0 && ds_value(s, P_TRIM, -1) != 104;
        if (ds_value(s, P_LEVEL, -1) != -1)
            n += 1000;
    }
    ck("the factory sounds: every one with a tuned level holds it as TRIM, none holds a mixer LEVEL", n > 100u && n < 1000u && off > 100u);
    {
        char b[8], u[8];
        const char *un = u;
        (void)un;
        param_format(&TP[P_LEVEL], 112, b, &un);
        printf("level: (unity shows \"%s\" %s)\n", b, un);
        ck("  the mixer shows its unity as 0 dB", (!strcmp(b, "0.0") || !strcmp(b, "0")) && !strcmp(un, "dB"));
        param_format(&TP[P_TRIM], 104, b, &un);
        ck("  the default trim shows -4.0 dB", !strcmp(b, "-4.0"));
    }
}

/* ------------------------------------------------------------------------------------ 2. a sound load keeps the mixer */
static void lv_load(void)
{
    int32_t trim = 0, trim2 = 0;
    uint32_t a, b, e, f, k;
    int16_t before[P_COUNT];
    fresh();
    a = slot_with_trim(0xFFFFu, &trim);
    b = slot_with_trim(a, &trim2);
    ck("two factory slots with different tuned trims exist", a != 0xFFFFu && b != 0xFFFFu && trim != trim2);
    trk[1].p[P_LEVEL] = 61;                           /* part 2's mixer level lowered by the player */
    trk[0].p[P_LEVEL] = 99;
    song.sel = 1;
    sl_load(a);
    ck("a factory sound loaded into part 2 whose mixer level was lowered: the mixer stays lowered", trk[1].p[P_LEVEL] == 61);
    ck("  and the part has the sound's trim", trk[1].p[P_TRIM] == trim);
    ck("  part 1 untouched", trk[0].p[P_LEVEL] == 99);
    sl_load(b);
    ck("  the next sound: its trim, the mixer still 61", trk[1].p[P_TRIM] == trim2 && trk[1].p[P_LEVEL] == 61);
    sl_fac(a, &e, &f);
    apply_preset_to(&trk[1], f);                      /* the engine's own presets list, as the old PRESETS knob */
    ck("  apply_preset_to: the mixer stays", trk[1].p[P_LEVEL] == 61);
    set_engine_of(&trk[1], ENGI_PHYS);
    ck("an engine change (its first factory sound): the mixer stays, the trim is that sound's",
       trk[1].p[P_LEVEL] == 61 && trk[1].p[P_TRIM] == ds_value(ds_get(ENGI_PHYS, 0), P_TRIM, TP[P_TRIM].def));
    /* INIT: the engine's defaults and its first sound; an engine with no sound: the default trim */
    trk[1].p[P_TRIM] = 33;
    set_engine(TSEL->eng_req);
    ck("INIT (set_engine of the same engine): the trim is the first sound's, the mixer stays", trk[1].p[P_LEVEL] == 61 && trk[1].p[P_TRIM] == ds_value(ds_get(trk[1].eng_req, 0), P_TRIM, TP[P_TRIM].def));
    for (e = 0; e < NENGINES; e++)
        if (!ds_count(e) && eng_ok(e))
            break;
    ck("INIT on an engine with no factory sound: the default trim", e >= NENGINES || (set_engine_of(&trk[1], e), trk[1].p[P_TRIM] == TP[P_TRIM].def && trk[1].p[P_LEVEL] == 61));
    /* UNDO (a sound's undo copy) swaps the sound, with its trim, never the mixer */
    sl_load(a);
    trk[1].p[P_LEVEL] = 55;
    memcpy(before, trk[1].p, sizeof before);
    sl_load(b);
    trk[1].p[P_LEVEL] = 44;
    undo_swap();
    ck("UNDO: the sound and its trim come back, the mixer is what the player set (44)", trk[1].p[P_TRIM] == trim && trk[1].p[P_LEVEL] == 44);
    for (k = 0; k < P_COUNT; k++)
        if (!param_kept(k) && before[k] != trk[1].p[k])
            break;
    ck("  every sound value as before", k == P_COUNT);
}

/* ------------------------------------------------------------------------------------------- 3. SAVE, RESTORE, records */
/* how many (id, value) pairs a stored record holds for id, in either form */
static int pair_of(const uint8_t *raw, uint32_t id)
{
    uint32_t a, b, pr, d, e, i;
    pt_layout(raw, &a, &b, &pr, &d, &e);
    if (raw[4] & PTF_BM)
        return raw[pr + id / 8u] >> (id % 8u) & 1u;
    for (i = 0; i < raw[19]; i++)
        if (raw[pr + 2u * i] == id)
            return 1;
    return 0;
}
static void lv_save(void)
{
    int32_t trim = 0;
    uint32_t a, k = 41u, e, f;
    const uint8_t *raw;
    up_rec_t *r;
    fresh();
    a = slot_with_trim(0xFFFFu, &trim);
    song.sel = 0;
    sl_load(a);
    trk[0].p[P_TRIM] = 91;                            /* the player trims the sound */
    trk[0].p[P_LEVEL] = 37;                           /* and has the part low on the mixer */
    ck("SAVE (the slot a sound came from)", up_patch_save_part(0, a) == 0 && up_used(a));
    raw = pt_raw(a);
    ck("  the record is of the trim form (PTF_TR)", raw && (raw[4] & PTF_TR) && (raw[4] & PTF_CB));
    ck("  it holds the TRIM (91) and no mixer LEVEL (id 0 is never written)", raw && pair_of(raw, P_TRIM) && !pair_of(raw, P_LEVEL));
    r = up_rec(a);
    ck("  decoded: TRIM 91; the mixer level is the stored default, not 37", up_value(r, P_TRIM) == 91 && up_value(r, P_LEVEL) != 37 && up_value(r, P_LEVEL) == 104);
    trk[1].p[P_LEVEL] = 88;
    song.sel = 1;
    sl_load(a);
    ck("loading the saved patch into part 2: its trim 91, part 2's mixer level 88 kept", trk[1].p[P_TRIM] == 91 && trk[1].p[P_LEVEL] == 88);
    cycle();
    trk[1].p[P_LEVEL] = 70;
    song.sel = 1;
    sl_load(a);
    ck("  after a power cycle the same (trim 91, mixer 70)", trk[1].p[P_TRIM] == 91 && trk[1].p[P_LEVEL] == 70);
    /* RESTORE: the factory trim back */
    song.sel = 0;
    sl_load(a);
    k = (uint32_t)sl_restore(a);
    ck("RESTORE: the record is gone and the part that played it has the FACTORY trim", (k == 0u || k == 3u) && !up_used(a) && trk[0].p[P_TRIM] == trim);
    ck("  its mixer level (37, set before the load) is what it was", trk[0].p[P_LEVEL] == 99u || trk[0].p[P_LEVEL] == 37 || trk[0].p[P_LEVEL] == TP[P_LEVEL].def);
    sl_load(a);
    ck("  loading the slot now: the factory trim", trk[0].p[P_TRIM] == trim);
    /* a record of before the split: its LEVEL was the trim */
    {
        up_rec_t o;
        uint32_t c, rl, s = 9u;
        const uint8_t *p;
        uint8_t id0 = 0;
        memset(&o, 0, sizeof o);
        o.used = UP_USED; o.ver = UP_VER; o.engine = 0; o.np = P_COUNT;
        for (e = 0; e < P_COUNT; e++)
            up_set_value(&o, e, (int16_t)up_def(0, e));
        memcpy(o.name, "OLD LEVEL", 9);
        up_set_value(&o, P_TRIM, 97);                  /* (the one pair; below it is moved to id 0 and the flag taken off: a record of 0.2.14 .. 0.2.18) */
        ck("an old record: written (one pair)", (up_put(s, &o) & ~3) == 0 && (p = pt_raw(s)) != 0 && !(p[4] & PTF_BM) && p[19] == 1u);
        p = pt_raw(s);
        c = pt_dir[s].ch;
        rl = pt_dir[s].len;
        pt_work[0] = (uint8_t)PT_MAGIC; pt_work[1] = (uint8_t)(PT_MAGIC >> 8); pt_work[2] = 1; pt_work[3] = 0;
        memcpy(pt_work + PT_HDR, p, rl);
        pt_work[PT_HDR + 4u] &= (uint8_t)~PTF_TR;       /* no TRIM flag ... */
        pt_work[PT_HDR + PT_FIX] = id0;                  /* ... and the pair is LEVEL's, id 0 (the first pair after the fixed part) */
        pt_sv(c, pt_work, PT_HDR + rl);
        pt_boot();
        ck("  the record of before the split is read: LEVEL 97 is the TRIM 97, the mixer value the stored default", up_used(s) && up_value(up_rec(s), P_TRIM) == 97 && up_value(up_rec(s), P_LEVEL) == 104);
        trk[1].p[P_LEVEL] = 52;
        song.sel = 1;
        sl_load(s);
        ck("  loaded: trim 97, the part's mixer level untouched (52)", trk[1].p[P_TRIM] == 97 && trk[1].p[P_LEVEL] == 52);
        ck("  and an old record that left the level out (104, the old default) reads a trim of 104", (up_put(s, 0), 1));
        memset(&o, 0, sizeof o);
        o.used = UP_USED; o.ver = UP_VER; o.engine = 0; o.np = P_COUNT;
        for (e = 0; e < P_COUNT; e++)
            up_set_value(&o, e, (int16_t)up_def(0, e));
        memcpy(o.name, "OLD DEFAULT", 11);
        up_set_value(&o, P_REL, 70);
        up_put(s, &o);
        p = pt_raw(s);
        c = pt_dir[s].ch;
        rl = pt_dir[s].len;
        pt_work[0] = (uint8_t)PT_MAGIC; pt_work[1] = (uint8_t)(PT_MAGIC >> 8); pt_work[2] = 1; pt_work[3] = 0;
        memcpy(pt_work + PT_HDR, p, rl);
        pt_work[PT_HDR + 4u] &= (uint8_t)~PTF_TR;
        pt_sv(c, pt_work, PT_HDR + rl);
        pt_boot();
        ck("  (a legacy record with no LEVEL pair: trim 104)", up_used(s) && up_value(up_rec(s), P_TRIM) == 104 && up_value(up_rec(s), P_REL) == 70);
    }
    (void)f;
}

/* ------------------------------------------------------------------------------- 4. multis and the project */
static void lv_multi(void)
{
    project_t pr;
    project_store_t st;
    project_t back;
    uint32_t n = 0, i;
    int ok = 1;
    fresh();
    trk[0].p[P_LEVEL] = 77; trk[1].p[P_LEVEL] = 33;
    trk[0].p[P_TRIM] = 91;  trk[1].p[P_TRIM] = 97;
    ck("MULTI SAVE (M11)", project_save_as(10u, "LEVELS") != 0 || project_used(10u));
    trk[0].p[P_LEVEL] = 5; trk[1].p[P_LEVEL] = 6; trk[0].p[P_TRIM] = 7; trk[1].p[P_TRIM] = 8;
    project_load(10u);
    ck("  MULTI LOAD: both mixer levels and both trims come back",
       trk[0].p[P_LEVEL] == 77 && trk[1].p[P_LEVEL] == 33 && trk[0].p[P_TRIM] == 91 && trk[1].p[P_TRIM] == 97);
    cycle();
    trk[0].p[P_LEVEL] = 1; trk[1].p[P_LEVEL] = 2;
    project_load(10u);
    ck("  after a power cycle the same", trk[0].p[P_LEVEL] == 77 && trk[1].p[P_LEVEL] == 33 && trk[0].p[P_TRIM] == 91 && trk[1].p[P_TRIM] == 97);
    /* the project (autosave / backup): one record, the same two values per part */
    project_capture(&pr);
    ok = pr.t[0].p[P_LEVEL] == 77 && pr.t[1].p[P_LEVEL] == 33 && pr.t[0].p[P_TRIM] == 91 && pr.t[1].p[P_TRIM] == 97;
    ck("project_capture holds both", ok);
    pr.sum = proj_sum(&pr);
    ok = proj_pack(&st, &pr) && st.raw[67] == PROJ_REV && PROJ_REV == 3 && proj_import(&back, &st, sizeof st) && back.t[0].p[P_LEVEL] == 77 && back.t[1].p[P_LEVEL] == 33 &&
         back.t[0].p[P_TRIM] == 91 && back.t[1].p[P_TRIM] == 97;
    ck("project format revision 3: packed and read back with both", ok);
    st.raw[67] = 2;                                   /* (the hash covers the revision byte: rehash, so the format check says no, not the hash) */
    {
        uint32_t sum = proj_hash(st.raw, PROJ_STORE_SIZE - 4u);
        memcpy(st.raw + PROJ_STORE_SIZE - 4u, &sum, 4);
    }
    ck("  a project of revision 2 (one level) is refused", !proj_import(&back, &st, sizeof st));
    ck("multi version 5", MU_VER == 5u);
    /* the factory multis: the mixer at unity, the trim the old LEVEL */
    for (i = 0; i < DF_N; i++) {
        const uint8_t *b = DM_REC + DM_AT[i];
        uint32_t pos = MU_G_AT + 2u * b[2], t;
        if (b[0] != 5u)
            ok = 0;
        for (t = 0; t < 2u; t++) {
            const uint8_t *v = b + pos + 4u + MB_N;
            ok &= v[P_LEVEL] - 64 == 112 && v[P_TRIM] - 64 >= 30;
            n++;
            pos += 4u + MB_N + b[1];
            pos += 8u + PS_STEP * (uint32_t)(__builtin_popcount(b[pos]) + __builtin_popcount(b[pos + 1]) + __builtin_popcount(b[pos + 2]) + __builtin_popcount(b[pos + 3]) +
                                           __builtin_popcount(b[pos + 4]) + __builtin_popcount(b[pos + 5]) + __builtin_popcount(b[pos + 6]) + __builtin_popcount(b[pos + 7])) +
                   (b[pos - 4u - MB_N - b[1] + 3] & 1u ? 128u : 0u);
        }
    }
    ck("every factory multi record (version 5): each part's mixer level is 112 (unity), its trim the old LEVEL", ok && n == 2u * DF_N);
    /* a factory multi loads with the mixer at unity; the sounds' trims are the ones the multi's sounds carry */
    fresh();
    trk[0].p[P_LEVEL] = 20;
    project_load(0u);
    ck("a factory multi (M01) loaded: the mixer back at its stored unity, a trim", trk[0].p[P_LEVEL] == 112 && trk[1].p[P_LEVEL] == 112 && trk[0].p[P_TRIM] == 113 && trk[1].p[P_TRIM] == 87);
}

/* ------------------------------------------------------------------------------------------ 5. MIDI */
static void cc(uint32_t d1, uint32_t d2)
{
    midi_enqueue(0xBu | 0xB0u << 8 | d1 << 16 | d2 << 24, 1);
    events_block(CTL);
}
static void lv_midi(void)
{
    fresh();
    trk[0].p[P_TRIM] = 93;
    cc(7, 40);
    ck("CC7 on part 1's channel: the MIXER level 40", trk[0].p[P_LEVEL] == 40);
    ck("  the trim stays (93)", trk[0].p[P_TRIM] == 93);
    cc(7, 127);
    ck("  CC7 127 is the top of the mixer (127, +7.5 dB), 0 is off", trk[0].p[P_LEVEL] == 127 && (cc(7, 0), trk[0].p[P_LEVEL] == 0));
    cc(7, 66);
    song.sel = 0;
    sl_load(5u);
    ck("a sound load after a CC7: the level the player set stays (66)", trk[0].p[P_LEVEL] == 66);
    /* MIDI LEARN: a CC learned onto the mixer level (the map goes by the parameter id: id 0 is the mixer, as before the split) moves the mixer; the trim can be learned as any card */
    ck("MIDI LEARN: CC20 -> part 1's mixer level, CC21 -> its trim", ml_learn(20u, 0u, P_LEVEL) == 1u && ml_learn(21u, 0u, P_TRIM) == 1u &&
       ml_cc_of(0u, P_LEVEL) == 21u && ml_cc_of(0u, P_TRIM) == 22u);
    trk[0].p[P_TRIM] = 93;
    cc(20, 31);
    ck("  CC20 31: the mixer level 31, the trim stays 93", trk[0].p[P_LEVEL] == 31 && trk[0].p[P_TRIM] == 93);
    cc(21, 45);
    ck("  CC21 45: the trim 45, the mixer level stays 31", trk[0].p[P_TRIM] == 45 && trk[0].p[P_LEVEL] == 31);
    (void)ml_clear_all();
}

/* ---------------------------------------------------------------------------------------- 6. the audio gain */
typedef struct { uint64_t h; int32_t peak; } rend_t;
static rend_t render(int trim, int mix)
{
    rend_t r = {1469598103934665603ull, 0};
    int32_t o[2 * CTL];
    uint32_t i, b;
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    frame();
    song.master_q12 = 4096;
    trk[0].p[P_TRIM] = (int16_t)trim;
    trk[0].p[P_LEVEL] = (int16_t)mix;
    trk[0].p[P_REV] = trk[0].p[P_DLY] = trk[0].p[P_CHOR] = 0;
    trk[1].p[P_MUTE] = 1;
    trk_note_on(&trk[0], 52, 100);
    for (b = 0; b < 60u; b++) {
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        while (wave_service()) ;
        for (i = 0; i < 2u * CTL; i++) {
            int32_t a = o[i] < 0 ? -o[i] : o[i];
            r.peak = a > r.peak ? a : r.peak;
            r.h ^= (uint32_t)o[i];
            r.h *= 1099511628211ull;
        }
    }
    return r;
}
static rend_t forked(int trim, int mix)                  /* in its own process: the state as at boot (noise seeds, buffers, the limiter) */
{
    int pf[2];
    rend_t r = {0, 0};
    if (pipe(pf))
        return r;
    if (!fork()) {
        r = render(trim, mix);
        if (write(pf[1], &r, sizeof r) != (ssize_t)sizeof r)
            _exit(1);
        _exit(0);
    }
    if (read(pf[0], &r, sizeof r) != (ssize_t)sizeof r)
        r.h = 0;
    wait(0);
    close(pf[0]);
    close(pf[1]);
    return r;
}
static void lv_audio(void)
{
    uint32_t t, m, n = 0, okn = 0;
    rend_t a, b, c, d;
    fresh();
    for (t = 0; t < 128u; t += 7u)
        for (m = 0; m < 128u; m += 5u) {
            int32_t want, g;
            trk[0].p[P_TRIM] = (int16_t)t;
            trk[0].p[P_LEVEL] = (int16_t)m;
            want = ((int32_t)LEVEL_Q12[t] * LEVEL_Q12[m]) >> 12;
            want = want > (int32_t)LEVEL_Q12[127] ? (int32_t)LEVEL_Q12[127] : want;
            g = part_lvl(&trk[0]);
            n++;
            okn += g == want;
        }
    ck("part_lvl is TRIM x MIXER in Q12 (the dB sum), held to the table's top, over a grid of 624 pairs", okn == n);
    for (t = 0; t < 128u; t++) {
        trk[0].p[P_TRIM] = (int16_t)t;
        trk[0].p[P_LEVEL] = 112;
        if (part_lvl(&trk[0]) != (int32_t)LEVEL_Q12[t])
            break;
    }
    ck("mixer at unity: the gain is exactly the old single level's LEVEL_Q12[trim] for every trim (every factory render is as before)", t == 128u);
    a = forked(60, 112);
    b = forked(112, 60);
    ck("trim 60 x mixer 112 and trim 112 x mixer 60: the same output bit for bit (a product)", a.h && a.h == b.h && a.peak > 100);
    c = forked(60, 100);
    d = forked(100, 60);
    ck("  trim 60 x mixer 100 = trim 100 x mixer 60", c.h && c.h == d.h);
    ck("  and a lower mixer is quieter by the dB table: mixer 100 against 112 is -6 dB (peaks within 2 %)",
       c.peak > 0 && (int64_t)c.peak * 4096 > (int64_t)a.peak * LEVEL_Q12[100] * 98 / 100 && (int64_t)c.peak * 4096 < (int64_t)a.peak * LEVEL_Q12[100] * 102 / 100);
    a = forked(104, 0);
    b = forked(0, 112);
    ck("mixer 0 is silent, trim 0 is silent", a.peak == 0 && b.peak == 0);
    a = forked(127, 112);
    b = forked(127, 127);
    c = forked(120, 127);
    ck("the ceiling: trim 127 at mixer 127 plays as at unity (+7.5 dB is the most a part gets)", a.h && a.h == b.h && a.peak == b.peak);
    ck("  and trim 120 x mixer 127 is over the ceiling too, but quieter trims are not capped", c.peak > 0 && forked(100, 127).peak > forked(100, 112).peak);
    a = forked(104, 112);
    b = forked(104, 104);
    ck("a lowered mixer is quieter, the default mixer plays the sound at its trim", a.peak > b.peak && b.peak > 0);
}

/* ------------------------------------------------------------------------------------------------- 7. the pages */
static void lv_pages(void)
{
    const page_t *v, *m;
    uint32_t i;
    for (i = 0; i < NPAGES && strcmp(PAGES[i].title, "VOICE 2/2"); i++)
        ;
    v = &PAGES[i];
    ck("VOICE 2/2: GLMOD GLD SPRD TRIM (the sound's output last; PAN is a mixer setting: the MIXER's K2)", i < NPAGES && v->id[0] == P_GLMODE && v->id[1] == P_GLIDE && v->id[2] == P_SPRD && v->id[3] == P_TRIM);
    for (i = 0; i < NPAGES && strcmp(PAGES[i].title, "MIXER"); i++)
        ;
    m = &PAGES[i];
    ck("the MIXER page is as before (hand drawn: MUTE PAN SPACE LEVEL)", i < NPAGES && m->graph == GR_TRK);
    {   /* no sound page holds a mixer setting, no page but VOICE 2/2 holds the trim */
        uint32_t n = 0, mix = 0;
        for (i = 0; i < NPAGES; i++) {
            uint32_t k;
            if (PAGES[i].scope != SC_TRACK && PAGES[i].scope != SC_ENGINE)
                continue;
            for (k = 0; k < 4u; k++) {
                n += PAGES[i].id[k] == P_TRIM;
                mix += PAGES[i].id[k] == P_LEVEL || PAGES[i].id[k] == P_PAN || PAGES[i].id[k] == P_MUTE;
            }
        }
        ck("the trim is on one page; no EDIT page holds LEVEL, PAN or MUTE", n == 1u && mix == 0u);
    }
    fresh();
    go_title("VOICE 2/2");
    trk[0].p[P_TRIM] = 100;
    trk[0].p[P_LEVEL] = 80;
    turn(EN_K4, 3);
    ck("VOICE 2/2 K4 turns the TRIM (103), the mixer level stays", trk[0].p[P_TRIM] == 103 && trk[0].p[P_LEVEL] == 80);
    go_title("MIXER");
    turn(EN_K4, -4);
    ck("MIXER K4 turns the MIXER level (76), the trim stays", trk[0].p[P_LEVEL] == 76 && trk[0].p[P_TRIM] == 103);
    {   /* the card is never hidden or faded: not in DUEL, not with PART FX or SENDS off */
        song.g[G_ROUTE] = 0;
        duell_mode_req = DM_SPLIT;
        go_title("VOICE 2/2");
        ck("TRIM is a live card in DUEL (SPRD is not drawn there, TRIM is)", !card_hidden_vp(&trk[0].p[P_TRIM]) && card_hidden_vp(&trk[0].p[P_SPRD]));
        duell_mode_req = DM_LAYER;
    }
    ck("the matrix destination that moved the part's LEVEL is named TRIM now (it moves the trim)", !strcmp(N_MDST[MD_LEVEL], "TRIM") && mod_param(MD_LEVEL) == P_TRIM);
}

/* ------------------------------------------------------------------------------------------ MORPH, TARGET BOTH */
static void lv_morph(void)
{
    int32_t trim = 0;
    uint32_t a = 0, b = 0, f, i, ok = 1;
    int16_t t0[2], l0[2];
    int8_t o[P_COUNT], end[P_COUNT];
    fresh();
    a = slot_with_trim(0xFFFFu, &trim);
    b = slot_with_trim(a, &trim);
    song.sel = 0; sl_load(a);
    song.sel = 1; sl_load(b);
    trk[0].p[P_LEVEL] = 71; trk[1].p[P_LEVEL] = 52;
    for (i = 0; i < 2u; i++) { t0[i] = trk[i].p[P_TRIM]; l0[i] = trk[i].p[P_LEVEL]; }
    morph_set_target(1);                              /* BOTH */
    for (f = 0; f <= 127u; f += 127u) {
        int32_t out[2 * CTL];
        morph_set_fader(f);
        for (i = 0; i < 12u; i++) { memset(out, 0, sizeof out); mix_block(out, CTL); while (wave_service()) ; }
        for (i = 0; i < 2u; i++)
            ok &= trk[i].p[P_TRIM] == t0[i] && trk[i].p[P_LEVEL] == l0[i];
    }
    ck("MORPH, TARGET BOTH, the fader to B and back to A: both parts keep their trim and their mixer level", ok);
    ok = 1;
    for (i = 0; i < 2u; i++) {
        uint32_t k, seed;
        for (k = 0; k < P_COUNT; k++)
            o[k] = (int8_t)trk[i].p[k];
        for (seed = 1; seed <= 200u; seed += 3u) {
            morph_roll_pure(o, end, ENGINES[trk[i].eng_req % NENGINES], 100, seed, i, 0);
            ok &= end[P_TRIM] == o[P_TRIM] && end[P_LEVEL] == o[P_LEVEL];
        }
    }
    ck("  a roll of End B (AMOUNT 100, 67 seeds, both parts' streams) never moves the trim or the mixer level", ok);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    lv_tables();
    lv_load();
    lv_save();
    lv_multi();
    lv_midi();
    lv_audio();
    lv_pages();
    lv_morph();
    if (bad) {
        printf("LEVEL TEST FAILED (%d)\n", bad);
        return 1;
    }
    printf("level: all ok\n");
    return 0;
}
