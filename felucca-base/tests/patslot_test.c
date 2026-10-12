/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL pattern slots P1..P16 (src/patslot.c, SAVE > PATTERNS) against simulated NOR flash:
 *   1. SAVE (KNOB 4, OCT+) stores part 1's pattern (64 steps: notes, chords, ties, accent, slide, RATCH, NUDGE, VEL,
 *      CHANCE; LEN DIV SWING GATE QUANTIZE) in a slot; the flash write follows when nothing plays (ps_poll), in the slots'
 *      sectors only (0xD6000..0xDBFFF), each sector's tail (0xF00..) erased;
 *   2. after a power cycle (RAM gone, flash read again) LOAD puts it onto part 2: steps and parameters equal; over part 2's
 *      own steps the REPLACE? dialog first;
 *   3. sixteen slots, each its own pattern, through a power cycle; ERASE (its dialog) empties one, the others stay;
 *   4. a power cut during a slot write leaves the slot as it was; SAVE while playing is kept in RAM, written at STOP.
 *   5. NAMES: SAVE opens the NAME screen (NAME PATTERN; the factory name is the existing name, else random; KNOB 4 random;
 *      OCT- cancels); the typed name is in the row and the LOADED message, through a power cycle; ERASE restores the
 *      factory name; the record is 596 B, six per object.
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

/* a pattern that uses every field of a step, seeded by k */
static void make_pattern(track_t *t, uint32_t k)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++) {
        step_t *s = &t->step[i];
        memset(s, 0, sizeof *s);
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
    memset(ps_ok, 0, sizeof ps_ok);
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

    /* 0: the 16 factory patterns (engines.c): a set of our own, every slot filled, SEQ-arp offsets around C4 (60) */
    {
        uint32_t i, j, n, bad16 = 0;
        for (i = 0; i < NPATTERNS; i++) {
            n = 0;
            for (j = 0; j < 16u; j++) {
                uint32_t nt = PATTERNS[i].note[j], fl = PATTERNS[i].flags[j];
                n += nt != 0;
                if (nt && (nt < 48u || nt > 84u))
                    bad16 = 1;                                  /* an offset beyond -12..+24 */
                if ((fl & 4u) && (nt || !j))
                    bad16 = 1;                                  /* a tie is a note-less step after a note */
            }
            if (!n || str_len(PATTERNS[i].name) > 12u)
                bad16 = 1;
            for (j = 0; j < i; j++)
                bad16 |= str_eq(PATTERNS[i].name, PATTERNS[j].name);
        }
        ck("16 factory patterns (one per slot), unique names of at most 12, notes within C3..C6, ties after notes",
           NPATTERNS == PS_SLOTS && !bad16);
    }

    /* 1: SAVE from part 1 into P03 */
    song.sel = 0;
    make_pattern(&trk[0], 1);
    ref = trk[0];
    go_title("PATTERNS");
    frame();
    ck("SAVE > PATTERNS is the page (SAVE's), one list of exactly the 16 slots", cur_page()->graph == GR_PATS &&
       cur_page()->fam == FAM_SAVE && pat_count() == PS_SLOTS);
    {
        char tag[4], nm[13];
        pat_label(2, tag, nm);
        ck("an unsaved slot 3 is its factory pattern: P03 OFFBEAT, loadable", str_eq(tag, "P03") && str_eq(nm, PATTERNS[2].name) &&
           pat_has(2) && !ps_used(2));
        pat_label(13, tag, nm);
        ck("slots 14..16 hold factory patterns too: P14 GALLOP.. (no empty slot by default)", str_eq(tag, "P14") &&
           str_eq(nm, PATTERNS[13].name) && pat_has(13) && pat_has(15) && !ps_used(13));
    }
    pick(2);
    turn(EN_K4, 1);                                  /* SAVE */
    press(B_OCTUP);
    ck("SAVE over a factory-default slot asks first (OVERWRITE P03?)", ui.confirm == CF_OVR_PSLOT && !ps_used(2));
    press(B_OCTUP);
    ck("  OCT+: the NAME PATTERN screen opens with the factory name OFFBEAT, nothing saved yet",
       name_on() && nm.kind == NK_PATTERN && str_eq(nm.s, PATTERNS[2].name) && !ps_used(2) && !ui.confirm);
    press(B_OCTDN);
    ck("  OCT-: cancels, nothing saved", !name_on() && !ps_used(2));
    turn(EN_K4, 1);
    press(B_OCTUP);
    press(B_OCTUP);
    turn(EN_K4, 1);
    ck("  KNOB 4: a new random name (12 characters at most)", nm.len > 0 && nm.len <= 12 && !str_eq(nm.s, PATTERNS[2].name));
    str_cpy(nm.s, "ACID DUB", sizeof nm.s);
    nm.len = nm.cur = 8;
    press(B_OCTUP);
    ck("SAVE (KNOB 4, OCT+, OCT+, name, OCT+): SAVED P03", msg_is("SAVED P03") && !ui.confirm && !name_on() && ps_used(2));
    {
        char tag[4], nm2[13];
        pat_label(2, tag, nm2);
        ck("  the row now reads P03 ACID DUB", str_eq(nm2, "ACID DUB"));
    }
    ck("  the flash waits for ps_poll", ps_dirty == 1u);
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
    ck("after a power cycle P03 is there, the others unsaved", ps_used(2) && !ps_used(0) && !ps_used(15) && ps_len(2) == (uint32_t)ref.p[P_SLEN]);
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
    /* (no PATTERNS slot is ever empty: all sixteen show a factory default, so the NAME-screen branch is for MULTIS) */

    /* 3: sixteen slots, each its own */
    for (k = 0; k < PS_SLOTS; k++) {
        make_pattern(&trk[0], 100u + k);
        ps_save_trk(&trk[0], k, "SLOT");
    }
    flush();
    power_cycle();
    ok = 1;
    for (k = 0; k < PS_SLOTS; k++) {
        make_pattern(&ref2, 100u + k);
        ok &= ps_used(k) && !ps_load_trk(&trk[1], k) && same_pattern(&trk[1], &ref2);
    }
    ck("16 slots: each loads its own pattern after a power cycle (onto either part)", ok);
    song.sel = 0;
    go_title("PATTERNS");
    pick(6);
    turn(EN_K3, 1);                                  /* ERASE */
    press(B_OCTUP);
    ck("ERASE asks first", ui.confirm == CF_ERASE_PSLOT);
    press(B_OCTUP);
    ck("  OCT+: ERASED P07", msg_is("ERASED P07") && !ps_used(6));
    flush();
    power_cycle();
    ok = !ps_used(6);
    for (k = 0; k < PS_SLOTS; k++) {
        make_pattern(&ref2, 100u + k);
        if (k != 6u)
            ok &= ps_used(k) && !ps_load_trk(&trk[1], k) && same_pattern(&trk[1], &ref2);
    }
    {
        char tag[4], nm2[13];
        pat_label(6, tag, nm2);
        ck("  ERASE restored the factory name: P07 ARP UPDOWN", str_eq(nm2, PATTERNS[6].name) && !ps_used(6));
        pat_label(4, tag, nm2);
        ck("  P05 still shows its stored name SLOT", str_eq(nm2, "SLOT"));
    }
    ck("  after a power cycle P07 empty, the other 15 as saved", ok);
    go_title("PATTERNS");
    pick(6);
    turn(EN_K2, 1);
    press(B_OCTUP);
    if (ui.confirm)
        press(B_OCTUP);
    ck("  LOAD of the erased slot 7: its factory pattern again (LOADED P07 ARP UPDOWN)", !ui.confirm && msg_is("LOADED P07 ARP UPDOWN") &&
       trk[0].step[0].note[0] == PATTERNS[6].note[0]);
    turn(EN_K3, 1);
    press(B_OCTUP);
    ck("ERASE of a factory-default slot: ALREADY FACTORY, no dialog", msg_is("ALREADY FACTORY") && !ui.confirm);
    pick(15);
    turn(EN_K3, 1);
    press(B_OCTUP);
    press(B_OCTUP);                                  /* (slot 16 was saved above: the dialog) */
    turn(EN_K3, 1);
    press(B_OCTUP);
    ck("ERASE of the erased slot 16: ALREADY FACTORY (no slot is empty)", msg_is("ALREADY FACTORY") && !ui.confirm);
    turn(EN_K2, 1);
    press(B_OCTUP);
    ck("LOAD of the erased slot 16: its factory pattern (FOUR FLOOR)", msg_is("LOADED P16 FOUR FLOOR") &&
       trk[0].step[0].note[0] == PATTERNS[15].note[0] && trk[0].step[4].note[0] == PATTERNS[15].note[4]);
    pick(0);
    turn(EN_K4, 1);
    press(B_OCTUP);
    ck("SAVE over a used slot asks first (OVERWRITE P01?)", ui.confirm == CF_OVR_PSLOT);
    press(B_OCTDN);
    ck("  OCT-: nothing changes", !ui.confirm);

    /* 4: a power cut during the write: the slot as it was */
    make_pattern(&ref2, 100u + 4u);
    make_pattern(&trk[0], 777u);
    ps_save_trk(&trk[0], 4, "CUT");
    cut_bytes = 1500;
    ps_retry_ms = 0;
    settings_poll();
    power_cycle();
    ck("a power cut during P05's write: P05 as it was", ps_used(4) && !ps_load_trk(&trk[1], 4) && same_pattern(&trk[1], &ref2));
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
    ck("SAVE while playing: in RAM at once, no flash write while playing", ps_dirty && ps_used(9));
    song.playing = 0;
    flush();
    power_cycle();
    ck("  written once stopped (after a power cycle: P10 as saved)", !ps_load_trk(&trk[1], 9) && same_pattern(&trk[1], &ref));

    /* 5: the record layout; an erased slot P14 is its factory pattern again; the old format reads as empty */
    ck("record 596 B (8 + 12 name + 64 x 9), object 3584 B for six, within a 3840 B payload",
       sizeof(ps_rec_t) == 596u && sizeof(ps_obj_t) == 3584u && 256u + sizeof(ps_obj_t) <= 0xF00u);
    song.playing = 0;
    go_title("PATTERNS");
    ps_erase(13);
    pick(13);
    turn(EN_K4, 1);
    press(B_OCTUP);
    ck("SAVE over the factory slot P14: the OVERWRITE dialog", ui.confirm == CF_OVR_PSLOT && !ps_used(13));
    press(B_OCTUP);
    ck("  then the NAME screen with the factory name GALLOP", name_on() && str_eq(nm.s, PATTERNS[13].name));
    press(B_OCTUP);
    ck("  saved under that name", ps_used(13) && ps_rec(13)->name[0] && msg_is("SAVED P14"));
    ps_bank[0].magic = 0x31535450u;                  /* "PTS1": an object of the nameless format */
    ps_obj_check(0, (int)sizeof ps_bank[0]);
    ck("an object with the old magic reads as empty", !ps_used(0) && !ps_used(5));

    printf(bad ? "patslot: FAILED (%d)\n" : "patslot: all ok\n", bad);
    return bad ? 1 : 0;
}
