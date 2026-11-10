/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.14, the editor's USER WAVETABLES (EDITOR_PROTOCOL.md "User wavetables (81-83)"; INFO 42 01 caps bit 3, value 8):
 *   81 WT_LIST  -> 3 (slots), 19 (frames), 64 (harmonics), per slot: used 0/1, name string (up to 8 characters), CRC-32 of its 1216 data bytes (u32)
 *   82 WT_GET   slot, offset (2 x 7 bit, LSB first), count (1..127) -> slot, rc, offset (2 bytes), count, pack7 data (rc 1: bad arguments or an empty slot)
 *   83 WT_PUT   op 0 begin: 0, slot, name (8 bytes, 0 padded), CRC-32 (u32); op 1 data: 1, slot, offset (2 bytes), pack7 data (in order, from 0);
 *               op 2 commit: 2, slot; op 3 abort: 3, slot; op 4 erase the slot: 4, slot  ->  op, slot, rc
 *   rc: 0 ok, 1 invalid arguments, 2 validation failed (length, CRC, name or an all-zero table), 3 stop playback first (begin, data, commit and erase
 *   stop the sequencer themselves, as the backup's writes do (ed_flash_stop), and answer 3 if it does not stop in 100 ms: nothing is written while it plays),
 *   4 flash write failed (the slot is as it was), 5 stale (no begin, the USB bus was reset, 15 s with no request, or a project save / load reused the staging).
 * The table is staged in the backup's staging RAM (ED_BK_RAW) as the finished object, wt_obj_t, and written at the commit by wtab.c wt_store
 * (st_save: the flash duck, the commit record last). The backup carries the slots as ids 24..26 (editor_backup.c): the same object, length 0 for an EMPTY slot. */
enum { ED_WT_LIST = 81, ED_WT_GET, ED_WT_PUT };
#define ED_WT_DATA (WV_F * WV_H)
static uint8_t ed_wt_slot, ed_wt_gen;                 /* (ed_wt_put: editor_backup.c, the two transfers share the staging RAM) */
static uint32_t ed_wt_crc, ed_wt_pos, ed_wt_ms, ed_wt_usb;

static uint32_t ed_wt_write(const uint8_t *a, uint32_t n)
{
    uint32_t i, off, count, rc;
    if (n < 2u || a[0] > 4u || a[1] >= WV_NU)
        return 1;
    if (a[0] == 0u) {                                 /* begin: the arguments first, a malformed request never stops the transport */
        if (n != 15u || a[14] > 15u)
            return 1;
        for (i = 0; i < 8u; i++)
            if (a[2u + i] && (a[2u + i] < 32u || a[2u + i] > 126u))
                return 1;
        if (ed_flash_stop())
            return 3;
        ed_bk_put = 0;                                /* (a backup transfer in the staging RAM is over) */
        ed_bk_valid = 0;
        ed_wt_gen = ++proj_wire_gen;
        ED_BK_RAW[0] = WT_VER; ED_BK_RAW[1] = WV_F; ED_BK_RAW[2] = WV_H; ED_BK_RAW[3] = 0;
        for (i = 0; i < 8u; i++)
            ED_BK_RAW[4u + i] = a[2u + i];
        ed_wt_put = 1; ed_wt_slot = a[1]; ed_wt_pos = 0;
        ed_wt_crc = ed_bk_r32(a + 10);
        ed_wt_usb = usb.resets; ed_wt_ms = fm1_ms;
        return 0;
    }
    if (a[0] == 4u) {                                 /* erase the slot: EMPTY again */
        if (n != 2u)
            return 1;
        if (ed_flash_stop())
            return 3;
        ed_wt_put = 0;
        return (uint32_t)wt_erase(a[1]);
    }
    if (!ed_wt_put || ed_wt_slot != a[1] || ed_wt_usb != usb.resets || fm1_ms - ed_wt_ms > 15000u || ed_wt_gen != proj_wire_gen) {
        ed_wt_put = 0;
        return 5;
    }
    ed_wt_ms = fm1_ms;
    if (a[0] == 3u) {
        ed_wt_put = 0;
        return n == 2u ? 0u : 1u;
    }
    if (a[0] == 2u) {                                 /* commit */
        if (n != 2u)
            return 1;
        if (ed_flash_stop())
            return 3;
        if (ed_wt_pos != ED_WT_DATA || st_crc32(ED_BK_RAW + 12, ED_WT_DATA) != ed_wt_crc)
            rc = 2;
        else
            rc = (uint32_t)wt_store(a[1], ED_BK_RAW, sizeof(wt_obj_t));
        ed_wt_put = 0;
        return rc;
    }
    off = (uint32_t)a[2] | (uint32_t)a[3] << 7;       /* data */
    if (n < 6u || off != ed_wt_pos)
        return 1;
    count = ed_unpack7(a + 4, n - 4u, ed_smp_buf, 256u);
    if (!count || count > ED_WT_DATA - ed_wt_pos)
        return 1;
    if (ed_flash_stop())
        return 3;
    memcpy(ED_BK_RAW + 12 + ed_wt_pos, ed_smp_buf, count);
    ed_wt_pos += count;
    return 0;
}

static int ed_wt_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint32_t u, i;
    if (cmd == ED_WT_LIST) {
        ed_b(WV_NU); ed_b(WV_F); ed_b(WV_H);
        for (u = 0; u < WV_NU; u++) {
            int used = wt_used >> u & 1u;
            ed_b(used);
            for (i = 0; used && i < 8u && wt_ram[u].name[i]; i++)
                ed_b((uint8_t)wt_ram[u].name[i]);
            ed_b(0);
            ed_bk_u32(used ? st_crc32(wt_ram[u].amp, ED_WT_DATA) : 0u);
        }
        return 1;
    }
    if (cmd == ED_WT_GET) {
        uint32_t off = n >= 3u ? (uint32_t)a[1] | (uint32_t)a[2] << 7 : 0u, count = n == 4u ? a[3] : 0u;
        uint32_t rc = n != 4u || a[0] >= WV_NU || !(wt_used >> a[0] & 1u) || !count || count > 127u || off > ED_WT_DATA || count > ED_WT_DATA - off ? 1u : 0u;
        ed_b(n ? a[0] : 127u); ed_b(rc); ed_b(off); ed_b(off >> 7); ed_b(rc ? 0u : count);
        if (!rc)
            ed_bk_pack(wt_ram[a[0]].amp + off, count);
        return 1;
    }
    if (cmd == ED_WT_PUT) {
        uint32_t rc = ed_wt_write(a, n);
        ed_b(n ? a[0] : 127u); ed_b(n >= 2u ? a[1] : 127u); ed_b(rc);
        return 1;
    }
    return 0;
}
