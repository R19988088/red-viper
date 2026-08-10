#ifndef COLOUR_MODES_H
#define COLOUR_MODES_H

enum {
    COLOUR_MODE_COUNT = 3,
    COLOUR_SHADE_COUNT = 4,
};

/* Stable semantic roles for the four entries in each fixed palette. */
enum ColourShadeRole {
    COLOUR_SHADE_BACKGROUND = 0,
    COLOUR_SHADE_DISABLED = 1,
    COLOUR_SHADE_READY = 2,
    COLOUR_SHADE_ACTIVE = 3,
};

int colour_mode_normalize(int mode);
unsigned colour_mode_value(int mode, int shade);

#endif
