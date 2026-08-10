#include "colour_modes.h"

static const unsigned COLOUR_MODES[COLOUR_MODE_COUNT][COLOUR_SHADE_COUNT] = {
    {0x000000, 0x040861, 0x0A1294, 0x2A29FF},
    {0x73284B, 0x513EB2, 0x6D9AD2, 0xC7D5E1},
    {0x4F593B, 0x417E58, 0x3A9C75, 0x4BAB95},
};

int colour_mode_normalize(int mode) {
    return mode >= 0 && mode < COLOUR_MODE_COUNT ? mode : 0;
}

unsigned colour_mode_value(int mode, int shade) {
    if (shade < 0 || shade >= COLOUR_SHADE_COUNT) shade = 0;
    return COLOUR_MODES[colour_mode_normalize(mode)][shade];
}
