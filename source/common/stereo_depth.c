#include "stereo_depth.h"

#include <math.h>

float stereo_depth_clamp(float value) {
    if (value <= 0.0f) return 0.0f;
    if (value >= 1.0f) return 1.0f;
    return value;
}

StereoDepthState stereo_depth_make_state(float slider, bool scale_native_depth) {
    const float clamped = stereo_depth_clamp(slider);
    return (StereoDepthState){
        .slider = clamped,
        .scale = scale_native_depth ? clamped : 1.0f,
        .active = clamped > 0.0f,
    };
}

int32_t stereo_depth_scale_symmetric(int32_t value, float scale) {
    scale = stereo_depth_clamp(scale);
    if (scale == 0.0f) return 0;
    if (scale == 1.0f) return value;
    return (int32_t)lroundf((float)value * scale);
}

StereoDepthPair stereo_depth_scale_pair(int32_t left, int32_t right, float scale) {
    scale = stereo_depth_clamp(scale);
    if (scale == 1.0f) return (StereoDepthPair){left, right};

    const float midpoint = ((float)left + (float)right) * 0.5f;
    if (scale == 0.0f) {
        const int32_t center = (int32_t)lroundf(midpoint);
        return (StereoDepthPair){center, center};
    }

    return (StereoDepthPair){
        (int32_t)lroundf(midpoint + ((float)left - midpoint) * scale),
        (int32_t)lroundf(midpoint + ((float)right - midpoint) * scale),
    };
}
