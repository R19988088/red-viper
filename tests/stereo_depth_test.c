#include <assert.h>

#include "stereo_depth.h"

int main(void) {
    assert(stereo_depth_clamp(-0.25f) == 0.0f);
    assert(stereo_depth_clamp(0.5f) == 0.5f);
    assert(stereo_depth_clamp(1.25f) == 1.0f);

    StereoDepthState state = stereo_depth_make_state(0.5f, true);
    assert(state.slider == 0.5f);
    assert(state.scale == 0.5f);
    assert(state.active);
    state = stereo_depth_make_state(0.0f, false);
    assert(state.slider == 0.0f);
    assert(state.scale == 1.0f);
    assert(!state.active);

    assert(stereo_depth_scale_symmetric(12, 0.0f) == 0);
    assert(stereo_depth_scale_symmetric(12, 0.5f) == 6);
    assert(stereo_depth_scale_symmetric(-12, 0.5f) == -6);
    assert(stereo_depth_scale_symmetric(-12, 1.0f) == -12);

    StereoDepthPair pair = stereo_depth_scale_pair(-4, 8, 0.5f);
    assert(pair.left == -1 && pair.right == 5);
    assert(pair.left + pair.right == 4);

    pair = stereo_depth_scale_pair(-4, 8, 0.0f);
    assert(pair.left == 2 && pair.right == 2);

    pair = stereo_depth_scale_pair(-4, 8, 1.0f);
    assert(pair.left == -4 && pair.right == 8);
    return 0;
}
