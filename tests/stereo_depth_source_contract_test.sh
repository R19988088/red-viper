#!/bin/sh
set -eu

if grep -q 'CONFIG_3D_SLIDERSTATE' source/3ds/citro3d.c; then
    printf '%s\n' 'stereo depth source contract: final compositor reads hardware slider directly' >&2
    exit 1
fi

grep -q 'stereo_depth_make_state' source/common/video.c
grep -q 'SLIDERMODE == SLIDER_3DS' source/3ds/citro3d.c
grep -q 'video_stereo_depth.active' source/3ds/citro3d.c

test "$(grep -c 'stereo_depth_scale_symmetric' source/common/video_hard.c)" -ge 3
test "$(grep -c 'stereo_depth_scale_pair' source/common/video_hard.c)" -ge 2
test "$(grep -c 'stereo_depth_scale_symmetric' source/common/video_soft.cpp)" -ge 3
test "$(grep -c 'stereo_depth_scale_pair' source/common/video_soft.cpp)" -ge 1

printf '%s\n' 'stereo depth source contract: passed'
