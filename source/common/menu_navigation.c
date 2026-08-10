#include "menu_navigation.h"

bool menu_axis_is_candidate(float selected_start, float selected_end,
                            float candidate_start, float candidate_end,
                            int direction) {
    if (direction < 0)
        return candidate_end <= selected_start;
    if (direction > 0)
        return candidate_start >= selected_end;
    return true;
}
