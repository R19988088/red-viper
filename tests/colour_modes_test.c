#include <assert.h>
#include <limits.h>

#include "colour_modes.h"

int main(void) {
    static const unsigned expected[COLOUR_MODE_COUNT][COLOUR_SHADE_COUNT] = {
        {0x000000, 0x040861, 0x0A1294, 0x2A29FF},
        {0x73284B, 0x513EB2, 0x6D9AD2, 0xC7D5E1},
        {0x4F593B, 0x417E58, 0x3A9C75, 0x4BAB95},
    };

    assert(COLOUR_SHADE_BACKGROUND == 0);
    assert(COLOUR_SHADE_DISABLED == 1);
    assert(COLOUR_SHADE_READY == 2);
    assert(COLOUR_SHADE_ACTIVE == 3);

    for (int mode = 0; mode < COLOUR_MODE_COUNT; mode++)
        for (int shade = 0; shade < COLOUR_SHADE_COUNT; shade++)
            assert(colour_mode_value(mode, shade) == expected[mode][shade]);

    assert(colour_mode_normalize(-1) == 0);
    assert(colour_mode_normalize(-100) == 0);
    assert(colour_mode_normalize(COLOUR_MODE_COUNT) == 0);
    assert(colour_mode_normalize(3) == 0);
    assert(colour_mode_normalize(INT_MAX) == 0);
    assert(colour_mode_normalize(0) == 0);
    assert(colour_mode_normalize(1) == 1);
    assert(colour_mode_normalize(2) == 2);
    assert(colour_mode_value(0, -1) == expected[0][0]);
    assert(colour_mode_value(0, COLOUR_SHADE_COUNT) == expected[0][0]);
    return 0;
}
