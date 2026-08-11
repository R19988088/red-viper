#include <assert.h>

#include "menu_panel_contract.h"

int main(void) {
    assert(MENU_SCREEN_W == 320);
    assert(MENU_SCREEN_H == 240);
    assert(MAIN_MENU_ROW_H == 28);
    assert(MAIN_MENU_TEXT_PERCENT == 63);
    assert(ABOUT_TEXT_PERCENT == 53);
    assert(ABOUT_LINE_GAP_PERCENT == 150);
    assert(ABOUT_TITLE_PERCENT == 85);
    assert(ABOUT_BODY_PERCENT == 48);
    assert(ABOUT_BODY_TOP == 82);
    assert(ABOUT_BODY_STEP == 24);
    assert(ABOUT_SCROLL_STEP == 12);
    assert(ABOUT_SCROLL_MAX == 110);
    assert(MENU_LEFT_BRIGHTNESS_PERCENT == 80);
    assert(MENU_PANEL_X == 120);
    assert(MENU_PANEL_W == 200);
    assert(MENU_PANEL_X + MENU_PANEL_W == MENU_SCREEN_W);
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
    assert((MENU_SCREEN_H - 8 * MAIN_MENU_ROW_H) / 2 == 8);
    assert((MENU_SCREEN_H - 4 * MAIN_MENU_ROW_H) / 2 == 64);
    return 0;
}
