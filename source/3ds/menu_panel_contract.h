#ifndef MENU_PANEL_CONTRACT_H
#define MENU_PANEL_CONTRACT_H

enum {
    MENU_PANEL_X = 120,
    MENU_PANEL_W = 200,
    OPTIONS_LABEL_X = 128,
    OPTIONS_VALUE_X = 216,
    OPTIONS_VALUE_W = 88,
    OPTIONS_VALUE_CENTER_X = 260,
    OPTIONS_ROW_H = 32,
    OPTIONS_ROW_GAP = 16,
    ROM_HEADER_Y = 0,
    ROM_HEADER_H = 32,
    ROM_UP_X = 128,
    ROM_UP_W = 72,
    ROM_PATH_X = 200,
    ROM_LIST_Y = 32,
    ROM_LIST_BOTTOM = 240,
};

static inline int options_row_y(int row) {
    return 24 + row * (OPTIONS_ROW_H + OPTIONS_ROW_GAP);
}

#endif
