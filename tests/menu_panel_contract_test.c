#include <assert.h>

#include "menu_panel_contract.h"

int main(void) {
    assert(MENU_PANEL_X == 120);
    assert(MENU_PANEL_W == 200);
    assert(OPTIONS_LABEL_X == 128);
    assert(OPTIONS_VALUE_X == 216);
    assert(OPTIONS_VALUE_W == 88);
    assert(OPTIONS_VALUE_X + OPTIONS_VALUE_W / 2 == OPTIONS_VALUE_CENTER_X);
    assert(options_row_y(0) == 24);
    assert(options_row_y(1) == 72);
    assert(options_row_y(2) == 120);
    assert(options_row_y(3) == 168);
    assert(ROM_HEADER_Y == 0);
    assert(ROM_HEADER_H == ROM_LIST_Y);
    assert(ROM_PATH_X >= ROM_UP_X + ROM_UP_W);
    return 0;
}
