#ifndef STEREO_DEPTH_H_
#define STEREO_DEPTH_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t left;
    int32_t right;
} StereoDepthPair;

typedef struct {
    float slider;
    float scale;
    bool active;
} StereoDepthState;

float stereo_depth_clamp(float value);
StereoDepthState stereo_depth_make_state(float slider, bool scale_native_depth);
int32_t stereo_depth_scale_symmetric(int32_t value, float scale);
StereoDepthPair stereo_depth_scale_pair(int32_t left, int32_t right, float scale);

#ifdef __cplusplus
}
#endif

#endif
