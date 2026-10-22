/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 DUELL contributors */
/* The MIDI MAP for the editor (docs/design/MIDI_MAP_DESIGN_2.md, web/EDITOR_PROTOCOL.md "MIDI MAP"; INFO 4B 01 1): the 128 lines of the
 * table (midi_control.c midi_map, midi_map.h). The old LEARN_GET / LEARN_SET (editor_learn.c, 78 / 79) keep working on the learned lines.
 *   MAP_GET   85  chunk (0..3): the lines 32 * chunk .. +31. Reply: chunk, MAP_REC_VER, then 32 lines of 3 bytes (a line's bits 0..6, 7..13,
 *                 14..15). chunk 4, code lo, code hi (code = lo | hi << 7): the target's facts. Reply: 4, lo, hi, kind, saved, automation, inert
 *                 (map_target_info for the selected part), then its name (0-terminated). A chunk above 4 or a wrong length: the reply is the chunk alone
 *   MAP_SET   86  0, cc, code lo, code hi, rule, force: cc moves the target with the part rule (a typed line, no cap); 1, cc: the number cleared
 *                 (not filled again by a later firmware's shipped list). A number that moves something else is not taken unless force is 1.
 *                 Reply: rc (0 ok, 1 locked standard number, 2 the number is another's, 3 invalid, 4 too many learned lines), save (as UI_SET: 0 saved,
 *                 3 RAM only, 4 queued until STOP), cc, its line (3 bytes), the owner's name on rc 2 (0-terminated, else empty)
 *   MAP_ARM   87  code lo, code hi, rule: the unit waits for a CC and makes it the line (a typed line); no argument: cancels. Reply: rc (0 armed, 3 invalid)
 *   MAP_RESET 88  RESET MAP: exactly the shipped list. Reply: 0, save
 * At most three lines of the existing editor code know this file (editor.c: the include, the dispatch, the INFO tag). */
enum { ED_MAP_GET = 85, ED_MAP_SET, ED_MAP_ARM, ED_MAP_RESET };
static void ed_map_line(uint32_t l)
{
    ed_b(l);
    ed_b(l >> 7);
    ed_b(l >> 14);
}
/* the settings saved; as UI_SET (0 saved, 3 RAM only, 4 queued until STOP), the map compared too */
static uint32_t ed_map_save(void)
{
    uint32_t rc = ed_ui_save();
#if FELUCCA_FLASH
    if (!rc && persist_saved_map != persist_map_crc(midi_map))
        rc = 3;
#endif
    return rc;
}
static int ed_map_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint32_t i, rc, code, cc;
    char who[24];
    if (cmd < ED_MAP_GET || cmd > ED_MAP_RESET)
        return 0;
    who[0] = 0;
    if (cmd == ED_MAP_GET) {
        if (n == 1u && a[0] < 4u) {
            ed_b(a[0]);
            ed_b(MAP_REC_VER);
            for (i = 0; i < 32u; i++)
                ed_map_line(map_line(32u * a[0] + i));
        } else if (n == 3u && a[0] == 4u) {
            map_info_t r;
            code = a[1] | (uint32_t)a[2] << 7;
            ed_b(4); ed_b(a[1]); ed_b(a[2]);
            if (map_code_ok(code)) {
                map_target_info(code, TSEL, &r);
                ed_b(r.kind); ed_b(r.saved); ed_b(r.automation); ed_b(r.inert);
                map_name(MAP_LINE(code, MAP_R_CHANNEL, MAP_W_TYPED), who);
                ed_str(who, 20);
            } else {
                ed_b(0); ed_b(0); ed_b(0); ed_b(0);
                ed_str("", 1);
            }
        } else {
            ed_b(n ? a[0] : 0x7Fu);
        }
        return 1;
    }
    if (cmd == ED_MAP_SET) {
        rc = MAP_E_BAD;
        cc = n >= 2u ? a[1] : 0u;
        if (n == 6u && a[0] == 0u && a[5] <= 1u)
            rc = map_set(cc, a[2] | (uint32_t)a[3] << 7, a[4], MAP_W_TYPED, a[5], who);
        else if (n == 2u && a[0] == 1u)
            rc = map_clear(cc);
        if (!rc) {
            ui.force = 1;
            ui.foot_sig = 0;
        }
        ed_b(rc);
        ed_b(rc ? 0u : ed_map_save());
        ed_b(cc);
        ed_map_line(map_line(cc));
        ed_str(who, 20);
        return 1;
    }
    if (cmd == ED_MAP_ARM) {
        rc = 3;
        if (!n) {
            ml_off();
            rc = 0;
        } else if (n == 3u) {
            code = a[0] | (uint32_t)a[1] << 7;
            if (map_code_ok(code) && a[2] < 4u && map_rule_ok(code, a[2])) {
                ml_arm_row(code, a[2]);
                rc = 0;
            }
        }
        ed_b(rc);
        return 1;
    }
    if (n)
        ed_b(3);                                           /* MAP_RESET takes no argument */
    else {
        map_reset();
        ui.force = 1;
        ui.foot_sig = 0;
        ed_b(0);
        ed_b(ed_map_save());
    }
    return 1;
}
