/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL pattern slots P01..P32 (src/patslot.c, SAVE > PATTERNS) against simulated NOR flash (job F3: ONE list of 32 slots, none empty,
 * nothing protected, the 32 defaults of engines.c PATTERNS[] installed as ordinary slots, compact PTS3 objects):
 *   0. the 32 defaults: names, notes, ties; a blank flash gives all 32 (written at the first quiet pass, in the slots' sectors only
 *      0xD6000..0xDBFFF, each sector's tail 0xF00.. erased), a second start reads them back and writes nothing; an object of the old
 *      format (PTS2) or damaged is the defaults again;
 *   1. SAVE (KNOB 4, OCT+) stores part 1's pattern (64 steps: notes, chords, ties, accent, slide, RATCH, NUDGE, VEL,
 *      CHANCE; LEN DIV SWING GATE QUANTIZE) in a slot; the flash write follows when nothing plays (ps_poll);
 *   2. after a power cycle (RAM gone, flash read again) LOAD puts it onto part 2: steps and parameters equal; over part 2's
 *      own steps the REPLACE? dialog first;
 *   3. 32 slots, each its own pattern, through a power cycle; ERASE (its dialog) gives one its default back, the others stay;
 *   4. a power cut during a slot write leaves the slot as it was; SAVE while playing is kept in RAM, written at STOP;
 *   5. NAMES: SAVE opens the NAME screen (NAME PATTERN; the slot's own name to start from; KNOB 4 random; OCT- cancels);
 *   6. the compact format: every step round-trips, the object's shape is checked, a full object says PATTERN LIST FULL and changes nothing.
 *   build/host/patslot_test            (DUELL/host_tests.sh "patslot") */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"

static uint8_t nor[0x100000];
static long cut_bytes = -1;                         /* the power goes after that many programmed bytes */
static int dead;
static uint32_t lo_hit = 0xFFFFFFFFu, hi_hit;       /* the range the stores erased / programmed */
static void hit(uint32_t off, uint32_t n) { if (off < lo_hit) lo_hit = off; if (off + n > hi_hit) hi_hit = off + n; }
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
    printf("patslot: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* a pattern that uses every field of a step, seeded by k; the first `used` steps are filled, the rest are empty steps (how a track starts) */
static uint32_t pat_used = NSTEP;
static void make_pattern(track_t *t, uint32_t k)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++) {
        step_t *s = &t->step[i];
        memset(s, 0, sizeof *s);
        if (i >= pat_used) {
            s->time = ST_REST;
            continue;
        }
        if ((i + k) % 5u == 4u) {
            s->time = ST_REST;
            continue;
        }
        s->time = (i + k) % 7u == 3u ? ST_TIE : ST_NOTE;
        s->n = (uint8_t)(1u + (i + k) % 4u);
        s->note[0] = (uint8_t)(36u + (i * 7u + k * 3u) % 60u);
        s->note[1] = (uint8_t)(s->note[0] + 4u);
        s->note[2] = (uint8_t)(s->n > 2u ? s->note[0] + 7u : 0u);
        s->note[3] = (uint8_t)(s->n > 3u ? s->note[0] + 11u : 0u);
        if (s->n < 2u) s->note[1] = 0;
        s->flags = (uint8_t)((i + k) & 3u);
        step_set_ratchet(s, 1u + (i + k) % 4u);
        step_set_nudge(s, (int32_t)((i * 3u + k) % 16u) - 8);
        s->vel = (uint8_t)(20u + (i * 5u + k) % 100u);
        s->probability = (uint8_t)((i + k) % 3u == 0u ? 0u : (i * 11u + k) % 102u);
    }
    t->p[P_SLEN] = (int16_t)(5u + (k * 7u) % 60u);
    t->p[P_SDIV] = (int16_t)(k % 5u);
    t->p[P_SSWING] = (int16_t)((k * 13u) % 100u);
    t->p[P_SGATE] = (int16_t)(1u + (k * 29u) % 127u);
    t->p[P_SQNT] = (int16_t)(k & 1u);
}
static int same_pattern(const track_t *a, const track_t *b)
{
    return !memcmp(a->step, b->step, sizeof a->step) && a->p[P_SLEN] == b->p[P_SLEN] && a->p[P_SDIV] == b->p[P_SDIV] &&
           a->p[P_SSWING] == b->p[P_SSWING] && a->p[P_SGATE] == b->p[P_SGATE] && a->p[P_SQNT] == b->p[P_SQNT];
}
static void power_cycle(void)                        /* the RAM is gone; the flash read again at power-on */
{
    dead = 0;
    cut_bytes = -1;
    memset(ps_bank, 0x5A, sizeof ps_bank);
    memset(ps_at, 0, sizeof ps_at);
    ps_dirty = 0;
    ui_power_on();
    flash_ok = 1;
    ps_boot();
}
static void flush(void)                              /* main loop passes until every change is in flash */
{
    uint32_t n;
    for (n = 0; n < 8u && ps_dirty; n++) {
        ps_retry_ms = 0;
        settings_poll();
    }
}
static uint32_t row_of(uint32_t k) { return pat_count() - PS_SLOTS + k; }   /* the list row of slot k */
static void pick(uint32_t k) { ui.ppick = (uint8_t)row_of(k); ui.force = 1; frame(); }

int main(void)
{
    static track_t ref, ref2;
    uint32_t k, o, ok;
    memset(nor, 0xFF, sizeof nor);
    power_cycle();

    /* 0: the 32 default patterns (engines.c): a set of our own, every slot filled, SEQ-arp offsets around C4 (60) */
    {
        uint32_t i, j, n, bad32 = 0, bad_name = 0;
        for (i = 0; i < NPATTERNS; i++) {
            n = 0;
            if (PATTERNS[i].len < 1u || PATTERNS[i].len > NPAT_STEPS)
                bad32 = 1;
            for (j = 0; j < NPAT_STEPS; j++) {
                uint32_t nt = PATTERNS[i].note[j], fl = PATTERNS[i].flags[j];
                n += nt != 0;
                if (nt && (nt < 48u || nt > 84u))
                    bad32 = 1;                                  /* an offset beyond -12..+24 */
                if ((fl & 4u) && (nt || !j))
                    bad32 = 1;                                  /* a tie is a note-less step after a note */
                if ((nt || fl) && j >= PATTERNS[i].len)
                    bad32 = 1;                                  /* nothing beyond LEN */
            }
            if (!n || str_len(PATTERNS[i].name) > 12u || !PATTERNS[i].name[0])
                bad32 = 1;
            for (j = 0; PATTERNS[i].name[j]; j++) {
                char c = PATTERNS[i].name[j];
                if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '.' || c == '_' || c == '/' || c == '#' || c == '+'))
                    bad_name = 1;
            }
            for (j = 0; j < i; j++)
                bad32 |= str_eq(PATTERNS[i].name, PATTERNS[j].name);
        }
        ck("32 default patterns (one per slot), unique names of at most 12 in the unit's character set, notes within C3..C6, ties after notes",
           NPATTERNS == PS_SLOTS && PS_SLOTS == 32u && !bad32 && !bad_name);
        ck("  the proposal's names: P07 GLASS LOOP, P15 BOOGIE BASS, P16 DOTTED HOOK, P20 WALK LINE, P28 NIGHT CALL, P32 SEVEN EIGHT; P01..P06 kept",
           str_eq(PATTERNS[6].name, "GLASS LOOP") && str_eq(PATTERNS[14].name, "BOOGIE BASS") && str_eq(PATTERNS[15].name, "DOTTED HOOK") &&
           str_eq(PATTERNS[19].name, "WALK LINE") && str_eq(PATTERNS[27].name, "NIGHT CALL") && str_eq(PATTERNS[31].name, "SEVEN EIGHT") &&
           str_eq(PATTERNS[0].name, "MORODER OCT") && str_eq(PATTERNS[1].name, "ACID LINE") && str_eq(PATTERNS[5].name, "8TH PULSE"));
        {
            int32_t wmax = 0, wi;
            uint32_t widest = 0;
            for (i = 0; i < NPATTERNS; i++)
                if ((wi = text_w(&AF_M, PATTERNS[i].name)) > wmax) { wmax = wi; widest = i; }
            printf("patslot:   widest name %s: %d px of the list row's 168 (x 64 .. 232)\n", PATTERNS[widest].name, (int)wmax);
            ck("  every name fits the list row of the unit's font whole (168 px), no cut", wmax <= 168);
        }
        ck("  LEN / DIV as in the proposal: P20 8 on 1/8, P23 12 on 1/8T, P28 32, P32 14, the rest 16 on 1/16",
           PATTERNS[19].len == 8 && PATTERNS[19].div == 1 && PATTERNS[22].len == 12 && PATTERNS[22].div == 4 && PATTERNS[27].len == 32 &&
           PATTERNS[31].len == 14 && PATTERNS[0].len == 16 && PATTERNS[0].div == 2);
    }

    /* 0b: a blank flash starts with the 32 defaults; they are written at the first quiet pass; a second start reads them back */
    ck("a blank flash: all 32 slots hold their default, none is 'changed', all three objects wait for the flash", ps_dirty == 7u);
    ok = 1;
    for (k = 0; k < PS_SLOTS; k++) {
        char nm0[13];
        ps_name(k, nm0);
        ok &= !ps_changed(k) && str_eq(nm0, PATTERNS[k].name) && ps_len(k) == PATTERNS[k].len;
    }
    ck("  every slot has its default name and LEN", ok);
    ok = 1;
    for (k = 0; k < PS_SLOTS; k++) {
        uint32_t i;
        ps_load_trk(&trk[1], k);
        ok &= trk[1].p[P_SLEN] == PATTERNS[k].len && trk[1].p[P_SDIV] == PATTERNS[k].div && trk[1].p[P_SSWING] == 0 && trk[1].p[P_SGATE] == 64;
        for (i = 0; i < NSTEP; i++) {
            step_t e;
            pat_make_step(&e, i < NPAT_STEPS ? PATTERNS[k].note[i] : 0, i < NPAT_STEPS ? PATTERNS[k].flags[i] : 0);
            ok &= trk[1].step[i].n == e.n && trk[1].step[i].note[0] == e.note[0] && trk[1].step[i].time == e.time &&
                  trk[1].step[i].flags == e.flags && trk[1].step[i].vel == e.vel && trk[1].step[i].probability == 0;
        }
    }
    ck("  every slot loads its table's steps (notes, ties, accents, slides), LEN, DIV; SWING 0, GATE 64", ok);
    {
        uint32_t u = ps_used_len(0), mx = 0;
        for (o = 0; o < PS_NOBJ; o++)
            if (ps_used_len(o) > mx)
                mx = ps_used_len(o);
        ck("  the objects: 11 + 11 + 10 slots, magic PTS3, well inside the 3584 B payload (largest object here: see the number)",
           ps_bank[0][6] == 11 && ps_bank[1][6] == 11 && ps_bank[2][6] == 10 && ps_bank[0][7] == 11 && !memcmp(ps_bank[0], "PTS3", 4) && mx < 2500u && u > 600u);
        printf("patslot:   default objects use %u / %u / %u bytes of %u\n", ps_used_len(0), ps_used_len(1), ps_used_len(2), PS_OBJ_MAX);
    }
    flush();
    ck("  written: the slots' sectors only (0xD6000..0xDBFFF), once", !ps_dirty && lo_hit >= 0xD6000u && hi_hit <= 0xDC000u);
    ok = 1;
    for (o = 0xD6000u; o < 0xDC000u; o += 0x1000u)
        for (k = 0xF00u; k < 0x1000u; k++)
            ok &= nor[o + k] == 0xFFu;
    ck("  every sector's tail (0xF00..) erased (the update records' place)", ok);
    lo_hit = 0xFFFFFFFFu;
    hi_hit = 0;
    power_cycle();
    flush();
    ck("  a second start reads the 32 back from the flash: nothing to write, no sector touched", !ps_dirty && lo_hit == 0xFFFFFFFFu && !ps_changed(0) && !ps_changed(31));
    {
        char b0[13];
        ps_name(31, b0);
        ck("  P32 is SEVEN EIGHT after the restart", str_eq(b0, "SEVEN EIGHT") && ps_len(31) == 14u);
    }
    {   /* the old format (PTS2, 16 slots of 596 bytes) and damage are not read: the object's slots are their defaults */
        uint8_t keep[PS_OBJ_MAX];
        memcpy(keep, ps_bank[1], sizeof keep);
        ps_bank[1][3] = '2';                                    /* "PTS2" */
        ps_obj_check(1, (int)ps_used_len(1));
        ck("an object with the old magic (PTS2) is the defaults again (and is written back)", !memcmp(ps_bank[1], keep, ps_used_len(1)) && (ps_dirty & 2u));
        ps_dirty = 0;
        ps_bank[0][20] ^= 0x80u;                                /* a damaged record (name or steps) */
        ps_obj_check(0, (int)ps_used_len(0));
        ck("  a damaged object likewise", !ps_changed(0) && (ps_dirty & 1u) && ps_obj_ok(0, ps_bank[0], ps_used_len(0)));
        ps_dirty = 0;
        ck("  a length that is not the object's own, or 0, likewise", (ps_obj_check(2, (int)ps_used_len(2) - 1), (ps_dirty & 4u)) &&
           (ps_dirty = 0, ps_obj_check(2, 0), (ps_dirty & 4u)));
        ps_dirty = 0;
        flush();
    }

    /* 1: SAVE from part 1 into P03 */
    song.sel = 0;
    pat_used = 24;
    make_pattern(&trk[0], 1);
    ref = trk[0];
    go_title("PATTERNS");
    frame();
    ck("SAVE > PATTERNS is the page (SAVE's), one list of exactly the 32 slots", cur_page()->graph == GR_PATS &&
       cur_page()->fam == FAM_SAVE && pat_count() == PS_SLOTS && PS_SLOTS == 32u);
    {
        char tag[4], nm[13];
        pat_label(2, tag, nm);
        ck("slot 3 is its default pattern: P03 OFFBEAT", str_eq(tag, "P03") && str_eq(nm, PATTERNS[2].name) && !ps_changed(2));
        pat_label(31, tag, nm);
        ck("slot 32 is the last one: P32 SEVEN EIGHT", str_eq(tag, "P32") && str_eq(nm, PATTERNS[31].name) && !ps_changed(31));
    }
    pick(2);
    turn(EN_K4, 1);                                  /* SAVE */
    press(B_OCTUP);
    ck("SAVE over a default slot asks first (OVERWRITE P03?): there is no empty slot", ui.confirm == CF_OVR_PSLOT && !ps_changed(2));
    press(B_OCTUP);
    ck("  OCT+: the NAME PATTERN screen opens with the slot's own name OFFBEAT, nothing saved yet",
       name_on() && nm.kind == NK_PATTERN && str_eq(nm.s, PATTERNS[2].name) && !ps_changed(2) && !ui.confirm);
    press(B_OCTDN);
    ck("  OCT-: cancels, nothing saved", !name_on() && !ps_changed(2));
    turn(EN_K4, 1);
    press(B_OCTUP);
    press(B_OCTUP);
    turn(EN_K4, 1);
    ck("  KNOB 4: a new random name (12 characters at most)", nm.len > 0 && nm.len <= 12 && !str_eq(nm.s, PATTERNS[2].name));
    str_cpy(nm.s, "ACID DUB", sizeof nm.s);
    nm.len = nm.cur = 8;
    press(B_OCTUP);
    ck("SAVE (KNOB 4, OCT+, OCT+, name, OCT+): SAVED P03", msg_is("SAVED P03") && !ui.confirm && !name_on() && ps_changed(2));
    {
        char tag[4], nm2[13];
        pat_label(2, tag, nm2);
        ck("  the row now reads P03 ACID DUB", str_eq(nm2, "ACID DUB"));
    }
    ck("  the flash waits for ps_poll", ps_dirty == 1u);
    lo_hit = 0xFFFFFFFFu;
    hi_hit = 0;
    flush();
    ck("  written: the slots' sectors only (0xD6000..0xDBFFF)", !ps_dirty && lo_hit >= 0xD6000u && hi_hit <= 0xDC000u);
    ok = 1;
    for (o = 0xD6000u; o < 0xDC000u; o += 0x1000u)
        for (k = 0xF00u; k < 0x1000u; k++)
            ok &= nor[o + k] == 0xFFu;
    ck("  every sector's tail (0xF00..) erased (the update records' place)", ok);

    /* 2: power cycle, LOAD onto part 2 (empty: no dialog; with its own steps: REPLACE? first) */
    power_cycle();
    {
        char tag[4], nm2[13];
        pat_label(2, tag, nm2);
        ck("  after a power cycle the row still shows P03 ACID DUB", str_eq(tag, "P03") && str_eq(nm2, "ACID DUB"));
    }
    ck("after a power cycle P03 is there as saved, the others are their defaults", ps_changed(2) && !ps_changed(0) && !ps_changed(31) &&
       ps_len(2) == (uint32_t)ref.p[P_SLEN]);
    song.sel = 1;
    track_defaults_steps(&trk[1]);
    go_title("PATTERNS");
    pick(2);
    turn(EN_K2, 1);                                  /* LOAD */
    press(B_OCTUP);
    ck("LOAD onto part 2 (empty): steps, LEN DIV SWING GATE QUANTIZE as saved from part 1",
       !ui.confirm && msg_is("LOADED P03 ACID DUB") && same_pattern(&trk[1], &ref));
    trk[1].step[0].note[0] ^= 1u;                    /* the user's edit */
    press(B_OCTUP);
    ck("  LOAD over part 2's own steps: the REPLACE? dialog first", ui.confirm == CF_LOAD_PAT);
    press(B_OCTUP);
    ck("  OCT+: loaded", !ui.confirm && same_pattern(&trk[1], &ref));
    hold(B_SAVE);                                    /* (SAVE held on PATTERNS: the quick overwrite of the slot picked, DUELL 0.2.2) */
    ck("  SAVE held on PATTERNS: the SAVE OVER <name>? dialog for the slot picked; OCT- leaves the pattern as it was",
       ui.confirm == CF_HOLD_PSLOT && (press(B_OCTDN), !ui.confirm && !name_on()) && same_pattern(&trk[1], &ref));
    trk[1].step[1].note[0] ^= 1u;                    /* a new edit, then the quick overwrite */
    hold(B_SAVE);
    press(B_OCTUP);
    ck("  OCT+: saved straight into the slot, no NAME screen, the name kept (ACID DUB)",
       !ui.confirm && !name_on() && msg_is("SAVED P03") && ps_load_trk(&trk[0], 2) == 0 && same_pattern(&trk[0], &trk[1]));
    {
        char nm[13];
        ps_name(2, nm);
        ck("  the stored name is still ACID DUB", str_eq(nm, "ACID DUB"));
    }
    /* a default slot never goes through an empty-slot branch: every SAVE asks OVERWRITE first (above) */

    /* 3: 32 slots, each its own */
    pat_used = 20;
    for (k = 0; k < PS_SLOTS; k++) {
        make_pattern(&trk[0], 100u + k);
        ps_save_trk(&trk[0], k, "SLOT");
    }
    flush();
    power_cycle();
    ok = 1;
    for (k = 0; k < PS_SLOTS; k++) {
        make_pattern(&ref2, 100u + k);
        ok &= ps_changed(k) && !ps_load_trk(&trk[1], k) && same_pattern(&trk[1], &ref2);
    }
    ck("32 slots: each loads its own pattern after a power cycle (onto either part)", ok);
    song.sel = 0;
    go_title("PATTERNS");
    pick(6);
    turn(EN_K3, 1);                                  /* ERASE */
    press(B_OCTUP);
    ck("ERASE asks first", ui.confirm == CF_ERASE_PSLOT);
    press(B_OCTUP);
    ck("  OCT+: ERASED P07", msg_is("ERASED P07") && !ps_changed(6));
    flush();
    power_cycle();
    ok = !ps_changed(6);
    for (k = 0; k < PS_SLOTS; k++) {
        make_pattern(&ref2, 100u + k);
        if (k != 6u)
            ok &= ps_changed(k) && !ps_load_trk(&trk[1], k) && same_pattern(&trk[1], &ref2);
    }
    {
        char tag[4], nm2[13];
        pat_label(6, tag, nm2);
        ck("  ERASE gave the default back: P07 GLASS LOOP (its steps too)", str_eq(nm2, PATTERNS[6].name) && !ps_changed(6) &&
           !ps_load_trk(&trk[1], 6) && trk[1].step[1].note[0] == PATTERNS[6].note[1] && trk[1].p[P_SLEN] == 16);
        pat_label(4, tag, nm2);
        ck("  P05 still shows its stored name SLOT", str_eq(nm2, "SLOT"));
    }
    ck("  after a power cycle P07 is its default, the other 31 as saved", ok);
    go_title("PATTERNS");
    pick(6);
    turn(EN_K2, 1);
    press(B_OCTUP);
    if (ui.confirm)
        press(B_OCTUP);
    ck("  LOAD of the erased slot 7: its default pattern (LOADED P07 GLASS LOOP)", !ui.confirm && msg_is("LOADED P07 GLASS LOOP") &&
       trk[0].step[0].note[0] == PATTERNS[6].note[0]);
    turn(EN_K3, 1);
    press(B_OCTUP);
    ck("ERASE of a slot that holds its default: ALREADY DEFAULT, no dialog", msg_is("ALREADY DEFAULT") && !ui.confirm);
    pick(31);
    turn(EN_K3, 1);
    press(B_OCTUP);
    press(B_OCTUP);                                  /* (slot 32 was saved above: the dialog) */
    turn(EN_K3, 1);
    press(B_OCTUP);
    ck("ERASE of the erased slot 32: ALREADY DEFAULT (no slot is empty)", msg_is("ALREADY DEFAULT") && !ui.confirm);
    turn(EN_K2, 1);
    press(B_OCTUP);
    ck("LOAD of the erased slot 32: its default pattern (SEVEN EIGHT, 14 steps)", msg_is("LOADED P32 SEVEN EIGHT") &&
       trk[0].step[0].note[0] == PATTERNS[31].note[0] && trk[0].step[6].note[0] == PATTERNS[31].note[6] && trk[0].p[P_SLEN] == 14);
    pick(0);
    turn(EN_K4, 1);
    press(B_OCTUP);
    ck("SAVE over a used slot asks first (OVERWRITE P01?)", ui.confirm == CF_OVR_PSLOT);
    press(B_OCTDN);
    ck("  OCT-: nothing changes", !ui.confirm);

    /* 4: a power cut during the write: the slot as it was */
    pat_used = 20;
    make_pattern(&ref2, 100u + 4u);
    make_pattern(&trk[0], 777u);
    ps_save_trk(&trk[0], 4, "CUT");
    cut_bytes = 600;
    ps_retry_ms = 0;
    settings_poll();
    power_cycle();
    ck("a power cut during P05's write: P05 as it was", ps_changed(4) && !ps_load_trk(&trk[1], 4) && same_pattern(&trk[1], &ref2));
    song.sel = 0;
    make_pattern(&trk[0], 31u);
    ref = trk[0];
    song.playing = 1;
    go_title("PATTERNS");
    pick(9);
    turn(EN_K4, 1);
    press(B_OCTUP);
    press(B_OCTUP);                                  /* (P10 holds one: the dialog) */
    ck("  while playing the NAME screen opens too, with P10's stored name", name_on() && str_eq(nm.s, "SLOT"));
    press(B_OCTUP);
    settings_poll();
    ck("SAVE while playing: in RAM at once, no flash write while playing", ps_dirty && ps_changed(9));
    song.playing = 0;
    flush();
    power_cycle();
    ck("  written once stopped (after a power cycle: P10 as saved)", !ps_load_trk(&trk[1], 9) && same_pattern(&trk[1], &ref));

    /* 5: the compact steps: every legal step round-trips through ps_cstep / ps_xstep; bad bytes are refused */
    {
        uint32_t r, bad_rt = 0, n_codes[3] = {0, 0, 0}, nn = 0;
        uint32_t seed = 12345u;
        for (r = 0; r < 200000u; r++) {
            uint8_t b[PS_STEP], c[PS_STEP], b2[PS_STEP];
            uint32_t j, kind;
            step_t s, s2;
            seed = seed * 1664525u + 1013904223u;
            kind = seed >> 28;
            memset(&s, 0, sizeof s);
            s.time = (uint8_t)((seed >> 3) % 3u);
            s.n = (uint8_t)((seed >> 5) % 5u);
            for (j = 0; j < 4u; j++) {
                seed = seed * 1664525u + 1013904223u;
                s.note[j] = (kind < 6u && j > 0) ? 0 : (uint8_t)(((seed >> 9) & 3u) ? (seed >> 12) % 128u : 0u);
            }
            if (kind < 3u) s.n = 1, s.note[0] = s.note[0] ? s.note[0] : 60;
            seed = seed * 1664525u + 1013904223u;
            s.flags = (uint8_t)(seed >> 8);
            s.vel = (uint8_t)((seed >> 16) % 128u);
            s.probability = (uint8_t)(kind < 8u ? 0u : (seed >> 20) % 102u);
            ps_pack_step(&s, b);
            nn = ps_cstep(c, b);
            n_codes[c[0] >> 6]++;
            if (ps_xstep(c, c + nn, b2) != nn || memcmp(b, b2, PS_STEP) || !ps_unpack_step(b2, &s2))
                bad_rt++;
            else {
                ps_pack_step(&s2, b2);                           /* (the step the slot loads packs to the same 9 bytes) */
                if (memcmp(b, b2, PS_STEP))
                    bad_rt++;
            }
            if (nn > 1u && ps_xstep(c, c + nn - 1u, b2))         /* a cut step is refused */
                bad_rt++;
        }
        ck("200000 random steps (notes, chords, ties, accent, slide, RATCH, NUDGE, VEL, CHANCE): compact -> expanded is exact, a cut step is refused", !bad_rt);
        printf("patslot:   codes used: empty %u, one note %u, general %u\n", n_codes[0], n_codes[1], n_codes[2]);
    }
    {
        uint8_t bad1[2] = {0xC0u, 0}, bad2[5] = {2u | 2u << 6, 5u, 0, 0, 0}, bad3[5] = {2u | 2u << 6, 1u, 128u, 0, 0}, b[PS_STEP];
        ck("  code 3, n = 5, a velocity over 127 are refused", !ps_xstep(bad1, bad1 + 2, b) && !ps_xstep(bad2, bad2 + 5, b) && !ps_xstep(bad3, bad3 + 5, b));
    }

    /* 6: the object's shape; a pattern that does not fit: PATTERN LIST FULL, nothing changed; ERASE frees room */
    ck("object 3584 B at most (within the 3840 B payload, tail 0xF00.. erased), 11 slots an object, record 597 B at most",
       PS_OBJ_MAX == 3584u && 256u + PS_OBJ_MAX <= 0xF00u && PS_PER_OBJ == 11u && PS_REC_MAX == 597u);
    {
        uint32_t r = 0, full_at = 99, before, len_before;
        uint8_t keep[PS_OBJ_MAX];
        song.playing = 0;
        song.sel = 0;
        go_title("PATTERNS");
        pat_used = NSTEP;
        for (k = 0; k < 11u; k++)
            ps_erase(k) , ps_dirty = ps_dirty;
        for (k = 0; k < 11u; k++) {
            make_pattern(&trk[0], 500u + k);
            if ((r = (uint32_t)ps_save_trk(&trk[0], k, "HEAVY CHORDS")) != 0) {
                full_at = k;
                break;
            }
        }
        ck("eleven 64-step patterns with chords, velocities and chances do not fit one object: SAVE is refused (2) before the last", r == 2u && full_at < 11u);
        printf("patslot:   a heavy 64-step pattern takes about %u bytes; refused at the %u. in the object\n", ps_rlen(ps_rec(0)), full_at + 1u);
        memcpy(keep, ps_bank[0], sizeof keep);
        len_before = ps_used_len(0);
        before = ps_dirty;
        (void)before;
        make_pattern(&trk[0], 900u);
        pick(full_at);
        turn(EN_K4, 1);
        press(B_OCTUP);
        press(B_OCTUP);
        press(B_OCTUP);
        ck("  the UI says PATTERN LIST FULL and the object is as it was", msg_is("PATTERN LIST FULL") && ps_used_len(0) == len_before && !memcmp(keep, ps_bank[0], len_before));
        ck("  a short pattern still fits (the empty tail steps cost nothing)", (pat_used = 3, make_pattern(&trk[0], 3), ps_save_trk(&trk[0], full_at, "SHORT")) == 0);
        ck("  ERASE of a heavy slot frees the room (its default is smaller)", ps_erase(0) == 0 && !ps_changed(0));
    }

    printf(bad ? "patslot: FAILED (%d)\n" : "patslot: all ok\n", bad);
    return bad ? 1 : 0;
}
