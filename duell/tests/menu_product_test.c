/* SPDX-License-Identifier: GPL-3.0-only */
/* Memory trim 1: the product has no USB serial console (FELUCCA_CDC 0): the editor's MENU list offers no USB SERIAL setting (id 11 stays retired, MENU_SET 11 is refused),
 * INFO's MENU count (tag 4E) is the number of rows MENU_DESC answers, and no console or CDC name is left in the firmware's tables. The device side (the MENU has no
 * USB SERIAL row, SYSTEM is five rows) is tests/release_test.c.
 *   build/host/menu_product_test        (host_tests.sh "menuproduct") */
#define FELUCCA_CDC 0
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"

int main(void)
{
    int bad = 0, ok = 1;
    uint32_t i, n, rows = 0;
    menu_item_t it;
    uint8_t a[3] = {11, 0, 64};
    reset();
    settings_defaults(favorites.defaults[15]);
    for (i = 0; i < 127u; i++) {
        n = menu_desc(i, &it);
        if (n > 10u) {
            rows++;
            ok &= it.id != 11u && strcmp(it.name, "USB SERIAL") != 0;
        }
    }
    bad += check("product: the editor's MENU list has no USB SERIAL (id 11 not offered)", ok && rows == ED_MENU_N && rows == 48u && FELUCCA_CDC == 0);   /* (item 138: 31 rows: the retired CPU METER row is not offered, item 128; DUELL 1.3: 42, + PERFORM K2..K4 47..49, QUICK K1..K4 50..53, MIDI PRESET 54, PART 1 / PART 2 CHANNEL and MIDI IN 55..57) */
    n = request(ED_MENU_SET, a, 3);
    bad += check("product: MENU_SET of id 11 is refused (no such setting)", n == 7u || (n >= 7u && host_wire[5] == 1u));
    printf("%s\n", bad ? "MENU PRODUCT TEST FAILED" : "menu product test passed");
    return bad != 0;
}
