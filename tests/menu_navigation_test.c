#include <assert.h>

#include "menu_navigation.h"

int main(void) {
    /* Adjacent rows share an edge and must remain reachable in either axis. */
    assert(menu_axis_is_candidate(8.0f, 36.0f, 36.0f, 64.0f, 1));
    assert(menu_axis_is_candidate(36.0f, 64.0f, 8.0f, 36.0f, -1));

    /* A row that overlaps the selected interval is not above or below it. */
    assert(!menu_axis_is_candidate(8.0f, 36.0f, 8.0f, 36.0f, 1));
    assert(!menu_axis_is_candidate(8.0f, 36.0f, 20.0f, 48.0f, -1));
    assert(!menu_axis_is_candidate(8.0f, 36.0f, 0.0f, 20.0f, 1));

    /* A separated interval in the requested direction is a candidate. */
    assert(menu_axis_is_candidate(8.0f, 36.0f, 40.0f, 68.0f, 1));
    assert(menu_axis_is_candidate(8.0f, 36.0f, -20.0f, 8.0f, -1));

    /* Zero direction does not impose an axis-side restriction. */
    assert(menu_axis_is_candidate(8.0f, 36.0f, 20.0f, 48.0f, 0));
    return 0;
}
