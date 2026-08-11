# 3DS 硬件滑条原生景深比例实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 让默认的 `Nintendo 3DS` 滑条模式按硬件滑条值缩放 Virtual Boy 场景自身的左右眼视差，使滑条控制立体深度强度，而不是给整幅左右眼画面增加固定偏移、改变整个场景的前后位置。

**架构：** 在公共视频层按帧采样并钳制 3DS 硬件滑条值，默认模式将它作为 `0.0...1.0` 的原生视差倍率。硬件和软件 VIP 渲染器在解码 `GP/MP/JP`、H-Bias 左右参数和 Affine 左右采样坐标时应用同一组纯函数；最终合成阶段不再为默认模式平移整幅画面。现有 `Virtual Boy IPD` 模式保留原来的全画面会聚偏移，作为兼容选项。

**技术栈：** C11、GNU C++11、Citro3D/Citro2D、libctru、Virtual Boy VIP world/object 数据、宿主机 C 单元测试、devkitARM 3DS Release 构建。

---

## 1. 分析结论

### 1.1 可行性

可以在现有架构内实现，而且不需要模拟 3D 相机或新增深度缓冲。Virtual Boy 图像在最终输出前已经保留了生成左右眼差异所需的原始参数：

- 普通背景世界：`WORLD.gp` 控制屏幕位置视差，`WORLD.mp` 控制地图采样视差。
- H-Bias 世界：除 `gp/mp` 外，每条扫描线还有左右眼 HOFST 参数。
- Affine 世界：`gp` 控制输出位置，逐行参数中的 `mp` 控制左右眼采样坐标差。
- 对象世界：对象 `JP` 控制左右眼水平位置差。

因此可以在这些参数被转换成顶点或像素坐标时缩放视差，并保持每个元素的左右眼中点基本不变。

这里的“景深比例”特指**立体视差强度**，不是摄影语义中的焦外模糊或景深范围。

### 1.2 当前行为为什么像整体前后移动

当前调用链是：

1. `source/common/video.c:72` 直接读取 `CONFIG_3D_SLIDERSTATE`，只用它决定渲染一只眼还是两只眼。
2. `source/common/video_hard.c` 和 `source/common/video_soft.cpp` 始终按游戏给出的完整 `GP/MP/JP` 生成原生左右眼内容，没有按滑条缩放这些值。
3. `source/3ds/citro3d.c:597-613` 的 `getDepthOffset()` 根据滑条生成一个左右眼方向相反的常量。
4. `source/3ds/citro3d.c:722-728` 和 `source/3ds/citro3d.c:764-772` 在最终 GPU/CPU 输出时，把整幅左眼和右眼画面分别横移这个常量。

设某个像素原生左右眼坐标为 `xL`、`xR`，当前实现近似为：

```text
xL' = xL + offset(slider)
xR' = xR - offset(slider)
```

所有物体都被加上相同的额外视差，物体之间的视差差值没有按比例改变。这主要改变会聚平面，所以视觉上像整个场景前后移动。

目标实现是：

```text
midpoint = (xL + xR) / 2
xL' = midpoint + (xL - midpoint) * slider
xR' = midpoint + (xR - midpoint) * slider
```

这样 `slider=0` 时共享内容趋于零视差，`slider=0.5` 时深度约为原生的一半，`slider=1` 时恢复游戏原生视差；元素中点在整数取整误差范围内不移动。

### 1.3 方案比较

**方案 A：缩放原生 VIP 视差参数，推荐。** 真正改变不同层之间的前后深度差，不需要深度纹理；代价是必须同时覆盖普通背景、H-Bias、Affine、对象和两套渲染器。

**方案 B：继续在最终合成阶段横移左右眼。** 改动最少，但只能改变统一会聚偏移，正是当前问题的根因。

**方案 C：对完成的左右眼图片做图像空间重投影。** 理论上能覆盖直接帧缓冲内容，但必须先做逐像素立体匹配或获得深度图；PICA200 上成本高，遮挡区会产生空洞和错配。

### 1.4 明确边界

- 默认 `SLIDER_3DS` 模式改为原生视差倍率；`SLIDER_VB` 保留旧的全画面偏移行为，现有配置文件数值无需迁移。
- 立体色差模式继续使用 `ANAGLYPH_DEPTH`，不读取物理 3D 滑条作为倍率。
- 游戏直接向左右眼显示帧缓冲写入的内容没有 world/object 深度参数。首版保持这部分内容的原生左右眼数据，不做图像匹配重投影；验收报告必须单列依赖直接帧缓冲的场景。
- `RM_CPUONLY` 当前没有实现 H-Bias world；本任务不扩展该渲染能力，但不能让已有普通背景、Affine 和对象路径退化。
- 不修改 Virtual Boy RAM 中的 `WORLD`、对象或参数表，只对渲染时解码出的临时值做缩放，避免影响模拟器状态、存档和游戏逻辑。

## 2. 文件结构

- 创建：`include/stereo_depth.h`，定义按帧立体状态和纯视差缩放接口。
- 创建：`source/common/stereo_depth.c`，实现滑条钳制、对称视差缩放和左右坐标围绕中点缩放。
- 创建：`tests/stereo_depth_test.c`，覆盖端点、负视差、左右中点、取整和钳制。
- 创建：`tests/stereo_depth_source_contract_test.sh`，确认默认模式不再走最终整幅平移，并确认各 VIP 视差入口均接入缩放函数。
- 修改：`include/vb_dsp.h`，公开当前渲染帧的 `StereoDepthState`。
- 修改：`include/video_hard.h`，给 `gpu_draw_affine()` 增加已经缩放的 `gp` 参数。
- 修改：`source/common/video.c`，在生成新眼帧前采样一次硬件滑条并设置当前帧状态。
- 修改：`source/common/video_hard.c`，缩放硬件 VIP 普通背景、H-Bias、Affine 和对象视差。
- 修改：`source/common/video_soft.cpp`，用相同规则缩放软件 VIP 普通背景、Affine、对象以及写入范围。
- 修改：`source/3ds/citro3d.c`，默认模式移除最终整幅偏移，使用已捕获的帧状态选择眼图；保留 2DS 立体色差和 `SLIDER_VB` 旧行为。
- 修改：`source/linux-test/opengl.c`，适配 `gpu_draw_affine()` 新签名，Linux 默认倍率固定为 `1.0`，输出保持不变。

## 3. 实现任务

### 任务 1：建立可单测的视差数学契约

**文件：**
- 创建：`include/stereo_depth.h`
- 创建：`source/common/stereo_depth.c`
- 创建：`tests/stereo_depth_test.c`

- [x] **步骤 1：先写失败的纯函数测试**

`tests/stereo_depth_test.c` 至少包含：

```c
#include <assert.h>
#include "stereo_depth.h"

int main(void) {
    assert(stereo_depth_clamp(-0.25f) == 0.0f);
    assert(stereo_depth_clamp(0.5f) == 0.5f);
    assert(stereo_depth_clamp(1.25f) == 1.0f);
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
```

- [x] **步骤 2：运行测试并确认接口尚不存在**

```bash
cc -std=c11 -Wall -Wextra -Werror -Iinclude \
  tests/stereo_depth_test.c source/common/stereo_depth.c -lm \
  -o /tmp/red-viper-stereo-depth-test
```

预期：失败，原因是新头文件或实现文件尚不存在。

- [x] **步骤 3：实现纯函数接口**

`include/stereo_depth.h`：

```c
#ifndef STEREO_DEPTH_H_
#define STEREO_DEPTH_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int32_t left, right; } StereoDepthPair;
typedef struct { float slider, scale; bool active; } StereoDepthState;

float stereo_depth_clamp(float value);
StereoDepthState stereo_depth_make_state(float slider, bool scale_native_depth);
int32_t stereo_depth_scale_symmetric(int32_t value, float scale);
StereoDepthPair stereo_depth_scale_pair(int32_t left, int32_t right, float scale);

#ifdef __cplusplus
}
#endif
#endif
```

`source/common/stereo_depth.c`：

```c
#include "stereo_depth.h"
#include <math.h>

float stereo_depth_clamp(float value) {
    if (value <= 0.0f) return 0.0f;
    if (value >= 1.0f) return 1.0f;
    return value;
}

StereoDepthState stereo_depth_make_state(float slider, bool scale_native_depth) {
    const float clamped = stereo_depth_clamp(slider);
    return (StereoDepthState){clamped, scale_native_depth ? clamped : 1.0f,
                              clamped > 0.0f};
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
```

保留 `scale==1.0f` 的精确返回分支，避免满滑条时因浮点转换改变原生坐标。

- [x] **步骤 4：运行纯函数测试**

```bash
cc -std=c11 -Wall -Wextra -Werror -Iinclude \
  tests/stereo_depth_test.c source/common/stereo_depth.c -lm \
  -o /tmp/red-viper-stereo-depth-test && \
/tmp/red-viper-stereo-depth-test
```

预期：退出码为 `0`，无输出。

- [ ] **步骤 5：提交数学契约**（未单独提交，合并到当前主工作区改动）

```bash
git add include/stereo_depth.h source/common/stereo_depth.c tests/stereo_depth_test.c
git commit -m "test: define stereo depth scaling contract"
```

### 任务 2：按渲染帧捕获 3DS 滑条状态

**文件：**
- 修改：`include/vb_dsp.h:177-178`
- 修改：`source/common/video.c:6-9,65-73`

- [x] **步骤 1：公开当前帧状态并设置跨平台默认值**

在 `include/vb_dsp.h` 引入 `stereo_depth.h` 并增加：

```c
extern StereoDepthState video_stereo_depth;
```

在 `source/common/video.c` 的 `eye_count` 旁增加：

```c
StereoDepthState video_stereo_depth = {1.0f, 1.0f, true};
```

Linux 路径因此继续以完整原生视差渲染。

- [x] **步骤 2：在生成新眼帧前采样一次物理滑条**

保留双缓冲旧帧先 flush、当前帧后 render 的顺序。在 `source/common/video.c` 当前 `#ifdef __3DS__` 区块开头执行：

```c
const bool scale_native_depth =
    !tVBOpt.ANAGLYPH && tVBOpt.SLIDERMODE == SLIDER_3DS;
video_stereo_depth = stereo_depth_make_state(
    CONFIG_3D_SLIDERSTATE, scale_native_depth);
eye_count = tVBOpt.ANAGLYPH ||
            tVBOpt.RENDERMODE == RM_TOCPU ||
            video_stereo_depth.active ? 2 : 1;
```

同一帧后续代码只能读取 `video_stereo_depth`，不能再次读取硬件地址。

- [x] **步骤 3：编译公共 C/C++ 头兼容性**

```bash
cc -std=c11 -Wall -Wextra -Werror -Iinclude \
  -c source/common/stereo_depth.c -o /tmp/stereo_depth.o
c++ -std=gnu++11 -Wall -Wextra -Werror -Iinclude \
  -c source/common/video_soft.cpp -o /tmp/video_soft-before-integration.o
```

预期：两个目标文件均成功生成。

- [ ] **步骤 4：提交帧状态捕获**（未单独提交，合并到当前主工作区改动）

```bash
git add include/vb_dsp.h source/common/video.c
git commit -m "feat: capture 3ds stereo slider per frame"
```

### 任务 3：在硬件 VIP 渲染器缩放原生视差

**文件：**
- 修改：`include/video_hard.h:63`
- 修改：`source/common/video_hard.c:144-504`
- 修改：`source/3ds/citro3d.c:239-305`
- 修改：`source/linux-test/opengl.c:282-330`

- [x] **步骤 1：普通背景缩放 `gp/mp`**

在 `source/common/video_hard.c` 每个 world 解码出有符号 `gp/mp` 后应用：

```c
gp = stereo_depth_scale_symmetric(gp, video_stereo_depth.scale);
mp = stereo_depth_scale_symmetric(mp, video_stereo_depth.scale);
```

可见范围、tile 缓存范围、左右眼 `gx/mx` 和 scissor 都使用缩放后的局部值。

- [x] **步骤 2：H-Bias 按实际左右读取值围绕中点缩放**

保留 `param_base` 奇偶导致的原硬件寻址行为：

```c
const u8 right_offset = !(param_base & 1);
const s16 native_left = (s16)(params[y * 2] << 3) >> 3;
const s16 native_right = (s16)(params[y * 2 + right_offset] << 3) >> 3;
const StereoDepthPair hofst = stereo_depth_scale_pair(
    native_left, native_right, video_stereo_depth.scale);
const int p = eye == 0 ? hofst.left : hofst.right;
```

- [x] **步骤 3：Affine 按逐行左右采样坐标缩放**

不能只缩放 Affine `mp`，因为其编码只偏移其中一只眼。逐行构造原生左右 `u/v` 后围绕中点缩放：

```c
const int left_factor = mp < 0 ? -mp : 0;
const int right_factor = mp >= 0 ? mp : 0;
const StereoDepthPair u = stereo_depth_scale_pair(
    mx + (left_factor * dx >> 6),
    mx + (right_factor * dx >> 6),
    video_stereo_depth.scale);
const StereoDepthPair v = stereo_depth_scale_pair(
    my + (left_factor * dy >> 6),
    my + (right_factor * dy >> 6),
    video_stereo_depth.scale);
avcur->u1 = avcur->u2 = eye == 0 ? u.left : u.right;
avcur->v1 = avcur->v2 = eye == 0 ? v.left : v.right;
```

原有 `umin/umax/vmin/vmax` 基于缩放后的 `avcur` 更新。

- [x] **步骤 4：对象 `JP` 使用对称缩放**

```c
jp = stereo_depth_scale_symmetric(jp, video_stereo_depth.scale);
```

对象可见性位、绘制顺序、调色板和翻转位保持不变。

- [x] **步骤 5：把缩放后的 Affine `gp` 传给平台后端**

将 `gpu_draw_affine()` 声明和两个实现改为显式接收 `int gp`：

```c
void gpu_draw_affine(WORLD *world, int gp,
    int umin, int vmin, int umax, int vmax, int drawn_fb,
    avertex *vbufs[], bool visible[]);
```

3DS 和 Linux 实现删除对 `world->gp` 的再次解码，scissor 使用传入值；调用点传递当前 world 已缩放的局部 `gp`。

- [x] **步骤 6：检查硬件路径没有遗漏原始视差再读取**

```bash
rg -n 'worlds\[wrld\]\.gp|worlds\[wrld\]\.mp|s16 jp|params\[y \* 8 \+ 1\]' \
  source/common/video_hard.c source/3ds/citro3d.c source/linux-test/opengl.c
```

预期：原始字段只在解码点读取；坐标、边界和 scissor 使用缩放变量或显式参数。

- [ ] **步骤 7：提交硬件 VIP 路径**（未单独提交，合并到当前主工作区改动）

```bash
git add include/video_hard.h source/common/video_hard.c \
  source/3ds/citro3d.c source/linux-test/opengl.c
git commit -m "feat: scale native vip parallax in hardware renderer"
```

### 任务 4：让软件 VIP 渲染器遵循相同倍率

**文件：**
- 修改：`source/common/video_soft.cpp:103-444`

- [x] **步骤 1：普通背景缩放 `gp/mp`**

在 `render_normal_world()` 中缩放局部 `gp/mp`，使 `gx/mx` 使用缩放值。在 `video_soft_render()` 的写入范围计算中也缩放 `gp`，避免按原生最大跨度上传无效区域。

- [x] **步骤 2：Affine 复用硬件路径的左右坐标公式**

在 `render_affine_world()` 内把逐行原生左右 `mx/my` 坐标构造成 `StereoDepthPair`，再选择当前眼坐标。软件路径不能保留旧的单眼偏移公式。

- [x] **步骤 3：对象 `JP` 和写入范围共同使用缩放值**

对象绘制坐标与 `SoftBufWrote` 列范围使用同一个缩放后的 `jp`。

- [x] **步骤 4：编译软件渲染器**

```bash
c++ -std=gnu++11 -Wall -Wextra -Werror -Iinclude \
  -c source/common/video_soft.cpp -o /tmp/video_soft-depth-scale.o
```

预期：成功生成目标文件。

- [ ] **步骤 5：提交软件 VIP 路径**（未单独提交，合并到当前主工作区改动）

```bash
git add source/common/video_soft.cpp
git commit -m "feat: scale native vip parallax in software renderer"
```

### 任务 5：默认模式停止平移整幅画面

**文件：**
- 修改：`source/3ds/citro3d.c:595-614,715-728,740-780`
- 修改：`include/vb_set.h:20-21,86`

- [x] **步骤 1：最终偏移只服务旧模式和 2DS 立体色差**

```c
static float getDepthOffset(bool default_for_both, int eye) {
    if (tVBOpt.ANAGLYPH && any_2ds)
        return eye == 0 ? tVBOpt.ANAGLYPH_DEPTH : -tVBOpt.ANAGLYPH_DEPTH;
    if (default_for_both || !video_stereo_depth.active) return 0.0f;
    if (tVBOpt.SLIDERMODE == SLIDER_3DS) return 0.0f;

    const float direction = eye == 0 ? 1.0f : -1.0f;
    return direction *
        (video_stereo_depth.slider * MAX_DEPTH - CENTER_OFFSET);
}
```

删除 `full_parallax` 参数。默认模式的 viewport/framebuffer 起点只使用居中基准位置。

- [x] **步骤 2：输出眼图选择使用同一帧状态**

GPU 和 CPU flush 的 `src_eye` 条件改为：

```c
const int src_eye = default_for_both
    ? orig_eye
    : (!tVBOpt.ANAGLYPH && !video_stereo_depth.active
        ? tVBOpt.DEFAULT_EYE
        : dst_eye);
```

完成后 `source/3ds/citro3d.c` 中不得再出现 `CONFIG_3D_SLIDERSTATE`。

- [x] **步骤 3：更新注释但不改变配置数值**

```c
#define SLIDER_3DS  0  /* Scale native scene disparity. */
#define SLIDER_VB   1  /* Legacy full-frame VB IPD offset. */
```

`slidermode=0/1` 的读写和菜单结构保持不变。

- [ ] **步骤 4：提交最终合成行为**（未单独提交，合并到当前主工作区改动）

```bash
git add source/3ds/citro3d.c include/vb_set.h
git commit -m "fix: make 3ds slider control stereo depth strength"
```

### 任务 6：增加结构门禁并完成构建验证

**文件：**
- 创建：`tests/stereo_depth_source_contract_test.sh`
- 验证：本计划列出的全部源文件和测试文件

- [x] **步骤 1：编写源代码契约检查**

```sh
#!/bin/sh
set -eu

! grep -q 'CONFIG_3D_SLIDERSTATE' source/3ds/citro3d.c
grep -q 'stereo_depth_make_state' source/common/video.c
grep -q 'SLIDERMODE == SLIDER_3DS' source/3ds/citro3d.c
test "$(grep -c 'stereo_depth_scale_symmetric' source/common/video_hard.c)" -ge 3
test "$(grep -c 'stereo_depth_scale_pair' source/common/video_hard.c)" -ge 2
test "$(grep -c 'stereo_depth_scale_symmetric' source/common/video_soft.cpp)" -ge 3
test "$(grep -c 'stereo_depth_scale_pair' source/common/video_soft.cpp)" -ge 1
printf '%s\n' 'stereo depth source contract: passed'
```

- [x] **步骤 2：运行宿主机测试与现有菜单门禁**

```bash
cc -std=c11 -Wall -Wextra -Werror -Iinclude \
  tests/stereo_depth_test.c source/common/stereo_depth.c -lm \
  -o /tmp/red-viper-stereo-depth-test && \
/tmp/red-viper-stereo-depth-test && \
sh tests/stereo_depth_source_contract_test.sh && \
sh tests/menu_source_contract_test.sh && \
cc -std=c11 -Wall -Wextra -Werror -Isource/3ds \
  tests/menu_panel_contract_test.c -o /tmp/red-viper-menu-panel-test && \
/tmp/red-viper-menu-panel-test
```

预期：全部退出码为 `0`；shell 门禁输出 `passed`。

- [ ] **步骤 3：运行 3DS Release 构建**（环境缺少 `DEVKITARM`）

```bash
make release-3ds -j2
```

预期：编译、链接和 makerom 成功，生成仓库根目录下的 `red-viper.cia`。

- [ ] **步骤 4：核对产物和改动范围**（等待 3DS 构建产物）

```bash
test -s red-viper.cia
git diff --check
git status --short
```

预期：产物非空，`git diff --check` 无输出；源代码改动仅覆盖本计划列出的立体状态、渲染器、测试和文档文件。

- [ ] **步骤 5：提交结构门禁**（未单独提交，合并到当前主工作区改动）

```bash
git add tests/stereo_depth_source_contract_test.sh
git commit -m "test: cover native stereo depth slider routes"
```

## 4. 实机验收

选择至少一个含清晰前景、零视差平面和远景的普通背景/对象场景，以及一个 Affine 场景。固定游戏状态，分别记录滑条 `0%`、`25%`、`50%`、`75%`、`100%` 的左右眼截图或采集画面。

- [ ] **默认模式端点：** `0%` 时共享内容为零视差或仅显示默认眼，画面中心不额外横移；`100%` 时左右眼坐标与修改前原生内容一致，允许整数路径最多 `1 px` 测量误差。
- [ ] **倍率关系：** 对同一前景和远景特征测量 `abs(xR-xL)`；`50%` 约为 `100%` 的一半，允许 `1 px` 取整误差；多个深度层的视差差值随滑条单调增大，不能只增加同一常量。
- [ ] **中点稳定：** 对同一特征计算 `(xL+xR)/2`，从 `0%` 到 `100%` 的中点漂移不超过 `1 px`。
- [ ] **连续性：** 对普通背景、H-Bias、Affine 和对象等已接入倍率的 VIP 内容，刚离开 `0%` 时不突然跳到完整原生深度；往返推动不出现左右眼交换、旧眼残留或单帧黑屏。
- [ ] **渲染路径：** 验证 `RM_GPUONLY`、`RM_TOGPU`、`RM_TOCPU`、`RM_CPUONLY` 可达场景；软件普通背景、Affine 和对象与 GPU 路径采用同一倍率。
- [ ] **已知边界：** 直接帧缓冲层在滑条大于 `0%` 后保持原生视差，因此可能在离开零位时直接恢复该层的完整视差；`RM_CPUONLY` 既有 H-Bias 缺口单列记录，两者都不能记作本功能已覆盖。
- [ ] **兼容行为：** `Virtual Boy IPD` 保持旧的整幅会聚偏移；立体色差的深度、左右颜色和 2DS 行为不变；菜单、默认眼、双缓冲和抗闪烁无新闪屏。
- [ ] **性能：** 相同场景下 `100%` 双眼渲染无新增持续掉帧；缩放路径不分配堆内存；`0%` 且不处于 `RM_TOCPU`/立体色差时仍只渲染默认眼。

## 5. 当前验证记录

已完成：

- `stereo_depth_test.c` 数学单元测试。
- `stereo_depth_source_contract_test.sh`、`menu_source_contract_test.sh`。
- `colour_modes_test`、`menu_navigation_test`、`menu_panel_contract_test`、`rom_browser_test`。
- `source/common/video_soft.cpp` 主机侧语法检查；仅保留工程原有 C++11 无消息 `static_assert` 警告。
- `git diff --check` 与新增文件尾随空白检查。

待环境具备后执行：

- `make release-3ds -j2`：当前环境未设置 `DEVKITARM`。
- Linux 全量构建：当前工程依赖缺少 `minizip/unzip.h`，与本改动无关。
- 真机滑条 `0%/25%/50%/75%/100%`、双缓冲、抗闪烁、各渲染模式和直接帧缓冲边界验收。

## 5. 完成标准

- 默认模式不再在最终合成阶段平移整幅左右眼画面。
- 普通背景、H-Bias、Affine、对象的可表达原生视差均由同一帧滑条倍率控制。
- GPU 与软件 VIP 路径对相同输入采用同一数学定义。
- 满滑条精确保留原生坐标，半滑条的层间视差约为一半，特征中点稳定在 `1 px` 内。
- 旧 `Virtual Boy IPD`、立体色差、配置数值、菜单结构和 Linux 默认输出保持兼容。
- 宿主机测试、源代码契约、现有相关测试、3DS Release 构建和实机矩阵均有结果记录；静态测试和构建不能替代实机立体观感验收。
