#ifndef MENU_NAVIGATION_H
#define MENU_NAVIGATION_H

#include <stdbool.h>

/*
 * Return whether a button lies strictly in the requested direction from the
 * selected button's axis interval.  Touching edges are adjacent and count as
 * candidates, while an overlapping interval is not a directional candidate.
 */
bool menu_axis_is_candidate(float selected_start, float selected_end,
                            float candidate_start, float candidate_end,
                            int direction);

#endif
