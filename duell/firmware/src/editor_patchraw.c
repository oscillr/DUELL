/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.19, the editor's PATCH_RAW (EDITOR_PROTOCOL.md "PATCH_RAW (84)"; INFO tag 5A 01 caps, bit 0): a patch slot's STORED RECORD (upreset.c) read and written
 * whole, bit for bit, so a patch can be exported to a file and imported into any slot without losing anything (UP_PUT drops End B, the arp / keys /
 * sequencer block and an FM6 sound's patch; UP_GET shows only the values). One command, an op byte first; every reply starts op, slot, rc:
 *   0 INFO   slot                              -> 0, slot, rc, saved (0/1), length (2 x 7 bit), CRC-32 (5 x 7 bit) of the record; a slot that holds its factory sound
 *                                                 answers saved 0, length 0, CRC 0 (there is no record; UP_GET gives its values)
 *   1 READ   slot, offset (2 x 7 bit), count   -> 1, slot, rc, offset (2), count, pack7 data (count 1..112; reading never touches the flash and works while playing)
 *   2 BEGIN  slot, length (2 x 7 bit), CRC (5) -> 2, slot, rc          the length is PT_FIX .. PT_WORST
 *   3 DATA   slot, offset (2 x 7 bit), pack7   -> 3, slot, rc          in order from 0, at most 128 packed bytes = 112 data bytes a piece
 *   4 COMMIT slot                              -> 4, slot, rc          validates, then writes the slot (one chunk, the standard commit-last write; the audio is ducked)
 *   5 ABORT  slot                              -> 5, slot, rc
 * The record is the stored form with its slot and revision bytes (2, 3) ZERO: the same sound is the same bytes in every slot (the unit sets both when it writes).
 * rc: 0 ok, 1 invalid (arguments; at commit: a record the store would not read), 2 validation failed at commit (a missing part or the CRC), 3 stop playback first
 * (BEGIN, DATA and COMMIT stop the sequencer themselves, as the backup's writes do, and answer 3 if it does not stop in 100 ms: nothing is written while it plays),
 * 4 flash write failed (the slot is as it was), 5 stale (no begin, the USB bus was reset, 15 s with no request, or a backup / wavetable transfer used the staging).
 * The record is staged in the backup's staging RAM (ED_BK_RAW), like a wavetable. */
enum { ED_PATCH_RAW = 84 };
static uint8_t ed_pr_put, ed_pr_slot, ed_pr_gen;
static uint32_t ed_pr_len, ed_pr_crc, ed_pr_pos, ed_pr_ms, ed_pr_usb;
#define ed_pr_rec ED_BK_RAW

static uint32_t ed_pr_write(const uint8_t *a, uint32_t n)
{
    uint32_t off, count, rc;
    if (n < 2u || a[0] < 2u || a[0] > 5u || a[1] >= UP_SLOTS)
        return 1;
    if (a[0] == 2u) {                                  /* begin: the arguments first, a malformed request never stops the transport */
        uint32_t len = (uint32_t)a[2] | (uint32_t)a[3] << 7;
        if (n != 9u || a[8] > 15u || len < PT_FIX || len > PT_WORST)
            return 1;
        if (ed_flash_stop())
            return 3;
        ed_bk_put = 0;                                 /* (a backup transfer in the staging RAM is over) */
        ed_bk_valid = 0;
        ed_wt_put = 0;
        ed_pr_gen = ++proj_wire_gen;
        ed_pr_put = 1; ed_pr_slot = a[1]; ed_pr_pos = 0; ed_pr_len = len;
        ed_pr_crc = ed_bk_r32(a + 4);
        ed_pr_usb = usb.resets; ed_pr_ms = fm1_ms;
        return 0;
    }
    if (!ed_pr_put || ed_pr_slot != a[1] || ed_pr_usb != usb.resets || fm1_ms - ed_pr_ms > 15000u || ed_pr_gen != proj_wire_gen) {
        ed_pr_put = 0;
        return 5;
    }
    ed_pr_ms = fm1_ms;
    if (a[0] == 5u) {
        ed_pr_put = 0;
        return n == 2u ? 0u : 1u;
    }
    if (a[0] == 4u) {                                  /* commit */
        if (n != 2u)
            return 1;
        if (ed_flash_stop())
            return 3;
        if (ed_pr_pos != ed_pr_len || st_crc32(ed_pr_rec, ed_pr_len) != ed_pr_crc)
            rc = 2;
        else {
            int r = up_put_raw(a[1], ed_pr_rec, ed_pr_len);
            rc = r == 0 ? 0u : r == 1 ? 1u : r == 2 && song.playing ? 3u : 4u;
        }
        ed_pr_put = 0;
        return rc;
    }
    off = (uint32_t)a[2] | (uint32_t)a[3] << 7;        /* data */
    if (n < 6u || off != ed_pr_pos)
        return 1;
    count = ed_unpack7(a + 4, n - 4u, ed_smp_buf, 128u);
    if (!count || count > 112u || count > ed_pr_len - ed_pr_pos)
        return 1;
    if (ed_flash_stop())
        return 3;
    memcpy(ed_pr_rec + ed_pr_pos, ed_smp_buf, count);
    ed_pr_pos += count;
    return 0;
}

static int ed_patchraw_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint8_t *rec = pt_enc;                             /* (the store's write scratch: free between writes, the main loop only) */
    if (cmd != ED_PATCH_RAW)
        return 0;
    if (n < 2u) {                                      /* (a request too short: the slot byte is missing) */
        ed_b(n ? a[0] : 127u); ed_b(127u); ed_b(1);
        return 1;
    }
    if (a[0] == 0u || a[0] == 1u) {
        uint32_t len = a[1] < UP_SLOTS ? up_raw_get(a[1], rec) : 0u, rc = a[1] >= UP_SLOTS ? 1u : 0u;
        if (a[0] == 0u) {
            ed_b(0); ed_b(a[1]); ed_b(n != 2u ? 1u : rc);
            ed_b(len ? 1u : 0u); ed_b(len); ed_b(len >> 7);
            ed_bk_u32(len ? st_crc32(rec, len) : 0u);
        } else {
            uint32_t off = n >= 4u ? (uint32_t)a[2] | (uint32_t)a[3] << 7 : 0u, count = n == 5u ? a[4] : 0u;
            if (n != 5u || rc || !len || !count || count > 112u || off > len || count > len - off)
                rc = 1;
            ed_b(1); ed_b(a[1]); ed_b(rc); ed_b(off); ed_b(off >> 7); ed_b(rc ? 0u : count);
            if (!rc)
                ed_bk_pack(rec + off, count);
        }
        return 1;
    }
    {
        uint32_t rc = ed_pr_write(a, n);
        ed_b(a[0]); ed_b(a[1]); ed_b(rc);
    }
    return 1;
}
