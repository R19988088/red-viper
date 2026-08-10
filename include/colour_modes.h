#ifndef COLOUR_MODES_H
#define COLOUR_MODES_H

enum {
    COLOUR_MODE_COUNT = 3,
    COLOUR_SHADE_COUNT = 4,
};

int colour_mode_normalize(int mode);
unsigned colour_mode_value(int mode, int shade);

#endif
