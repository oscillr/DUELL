/* SPDX-License-Identifier: GPL-3.0-only */
/* ERASE_ALL (cmd 80, DUELL 0.2.2, EDITOR_PROTOCOL.md "ERASE ALL"): the installer's "Erase my saved data after installing" and any
 * editor start the same erase as MENU > SYSTEM > ERASE DATA (erase_data.c). Nothing is erased unless the payload is exactly
 * the nine ASCII bytes "ERASE ALL" (a stray or half-formed frame does nothing but answer rc 1), and not while the transport
 * runs. Reply: rc. 0 done (flash erased; the unit restarts by itself ~0.7 s later: the USB bus drops and comes back), 1 refused:
 * wrong token or length (nothing touched), 3 the transport runs (nothing touched), 4 the flash failed (some objects may
 * be gone already, the settings are not: run it again or use the menu) */
enum { ED_ERASE_ALL = 80 };
static int ed_erase_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    static const uint8_t TOKEN[9] = {'E', 'R', 'A', 'S', 'E', ' ', 'A', 'L', 'L'};
    uint32_t i, rc = 0;
    if (cmd != ED_ERASE_ALL)
        return 0;
    if (n != sizeof TOKEN)
        rc = 1;
    for (i = 0; !rc && i < sizeof TOKEN; i++)
        if (a[i] != TOKEN[i])
            rc = 1;
    if (!rc)
        rc = (uint32_t)erase_data_run();
    ed_b(rc);
    return 1;
}
