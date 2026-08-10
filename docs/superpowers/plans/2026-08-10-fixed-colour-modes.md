# 三种固定色彩模式实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 将原有逐色阶手动配色改为设置页中的三种固定色彩模式切换；每种模式一次性映射游戏的 4 个亮度色阶，不提供单个色阶、色轮或亮度倍率调节入口。

**架构：** 在公共视频层定义三组只读的 4 色映射，3DS 设置页只保存和切换模式编号，渲染层按“模式编号 + 色阶编号”取色。沿用现有 `multicolid` 配置键读取旧配置并将值规范到 `0...2`，避免新增并行状态；原有 `MTINT/STINT` 自定义数据不再参与渲染，设置页删除所有独立编辑路径。

**技术栈：** C11、Red Viper `VB_OPT`/INI 配置、Citro2D/Citro3D、GNU Make、devkitARM。

---

## 已确认现状与设计边界

- 当前入口为 `选项 -> 视频设置 -> 设置`，`barrier_settings()` 先切换单色/多色，再进入色轮、4 个 palette slot 和 4 个色阶的独立编辑页。
- `source/common/video_common.c::video_get_colour()` 是 3DS GPU/CPU 渲染共同取色点；现有颜色字段为低字节到高字节依次存储 R/G/B，例如界面上的 RGB `#FF292A` 在代码中写为 `0x2A29FF`。
- 截图表格从上到下的展示顺序是“背景、最亮、亮、暗”，而运行时色阶编号是 `0=背景/最暗、1=暗、2=亮、3=最亮`，实现时必须重排，不能按表格行顺序直接复制。
- 三种模式按截图使用中性名称“模式 1 / 模式 2 / 模式 3”，避免增加截图未定义的营销名称。
- 色值按截图中纯色块中心像素采样；模式 1 的背景归一为纯黑，消除截图缩放产生的 `#010201` 噪声。

| 模式 | 色阶 0：背景 | 色阶 1：暗 | 色阶 2：亮 | 色阶 3：最亮 |
| --- | --- | --- | --- | --- |
| 模式 1 | `#000000` / `0x000000` | `#610804` / `0x040861` | `#94120A` / `0x0A1294` | `#FF292A` / `0x2A29FF` |
| 模式 2 | `#4B2873` / `0x73284B` | `#B23E51` / `0x513EB2` | `#D29A6D` / `0x6D9AD2` | `#E1D5C7` / `0xC7D5E1` |
| 模式 3 | `#3B594F` / `0x4F593B` | `#587E41` / `0x417E58` | `#759C3A` / `0x3A9C75` | `#95AB4B` / `0x4BAB95` |

**关键行为决定：** 这三组颜色是 4 个逻辑色阶的最终映射。固定模式下不再使用 `STINT` 对颜色做第二次亮度插值，否则截图色值会随游戏的 BRTA/BRTB/BRTC 寄存器变化，无法保证同一色阶稳定对应同一颜色。立体红蓝模式仍沿用现有灰度/眼睛通道路径，不套用这三组映射。

## 文件结构

- 创建：`include/colour_modes.h`，声明模式数、色阶数和只读取色接口。
- 创建：`source/common/colour_modes.c`，保存三组固定色值并处理非法模式/色阶输入。
- 创建：`tests/colour_modes_test.c`，在宿主机上验证 12 个映射值、顺序和越界回退。
- 修改：`include/vb_set.h`，明确 `MULTIID` 现在表示固定色彩模式编号；保留旧字段只为读取既有配置，不再作为运行时取色来源。
- 修改：`source/common/vb_set.c`，默认启用模式 1、加载时规范旧 `multicolid`、保存时只保留模式选择。
- 修改：`source/common/video_common.c`，普通非红蓝立体渲染从固定色表取色。
- 修改：`source/3ds/gui_hard.c`，将单色/多色与手动编辑页替换为三态模式选择和只读色阶预览。
- 验证：`Makefile.linux`、`Makefile`，分别证明公共 C 代码可编译及 3DS Release 可构建；不改变现有构建目标。

### 任务 1：建立固定色彩映射及宿主机测试

**文件：**
- 创建：`include/colour_modes.h`
- 创建：`source/common/colour_modes.c`
- 创建：`tests/colour_modes_test.c`

- [ ] **步骤 1：编写失败的映射测试**

`tests/colour_modes_test.c` 直接断言运行时顺序，不只检查常量是否存在：

```c
#include <assert.h>
#include "colour_modes.h"

int main(void) {
    static const unsigned expected[COLOUR_MODE_COUNT][COLOUR_SHADE_COUNT] = {
        {0x000000, 0x040861, 0x0A1294, 0x2A29FF},
        {0x73284B, 0x513EB2, 0x6D9AD2, 0xC7D5E1},
        {0x4F593B, 0x417E58, 0x3A9C75, 0x4BAB95},
    };
    for (int mode = 0; mode < COLOUR_MODE_COUNT; mode++)
        for (int shade = 0; shade < COLOUR_SHADE_COUNT; shade++)
            assert(colour_mode_value(mode, shade) == expected[mode][shade]);
    assert(colour_mode_normalize(-1) == 0);
    assert(colour_mode_normalize(COLOUR_MODE_COUNT) == 0);
    return 0;
}
```

- [ ] **步骤 2：运行测试并确认先失败**

运行：

```bash
mkdir -p build
cc -std=c11 -Wall -Wextra -Werror -Iinclude \
  tests/colour_modes_test.c source/common/colour_modes.c \
  -o build/colour_modes_test
```

预期：FAIL，提示 `colour_modes.h` 或 `colour_mode_value` 尚不存在。

- [ ] **步骤 3：实现最小只读接口**

在头文件中定义 `COLOUR_MODE_COUNT = 3`、`COLOUR_SHADE_COUNT = 4`；在 `.c` 文件中保存上表 12 个 `unsigned` 值。`colour_mode_normalize()` 对 `0...2` 原样返回，其余值回退到模式 1；`colour_mode_value()` 对非法色阶回退到色阶 0，确保损坏配置不会越界读取。

- [ ] **步骤 4：运行映射测试**

运行：

```bash
cc -std=c11 -Wall -Wextra -Werror -Iinclude \
  tests/colour_modes_test.c source/common/colour_modes.c \
  -o build/colour_modes_test
./build/colour_modes_test
```

预期：退出码为 0。

- [ ] **步骤 5：提交固定映射**

```bash
git add include/colour_modes.h source/common/colour_modes.c tests/colour_modes_test.c
git commit -m "feat: add fixed colour mode palettes"
```

### 任务 2：接入默认值、旧配置和渲染路径

**文件：**
- 修改：`include/vb_set.h:60-66`
- 修改：`source/common/vb_set.c:84-136, 191-220, 464-518, 590-605`
- 修改：`source/common/video_common.c:20-50`
- 测试：`tests/colour_modes_test.c`

- [ ] **步骤 1：扩展失败测试覆盖配置规范化语义**

给测试补充 `-100`、`3`、`INT_MAX` 均回退为 0，并验证合法值 `0/1/2` 不变。再次运行任务 1 的编译命令，预期新断言在规范化实现接入前失败。

- [ ] **步骤 2：规范模式状态但兼容旧配置键**

保持 INI 键名 `multicolid`，将其语义收窄为模式编号。`setDefaults()` 设置 `MULTICOL=true`、`MULTIID=0`；`loadFileOptions()` 和 `loadGameOptions()` 在 `ini_parse()` 后执行：

```c
tVBOpt.MULTICOL = true;
tVBOpt.MULTIID = colour_mode_normalize(tVBOpt.MULTIID);
```

这样旧值 `0/1/2` 直接迁移，旧值 `3` 或损坏值回到模式 1。保留旧 `[paletteN]` 解析分支以兼容旧文件格式，但其 `MTINT/STINT` 结果不再被渲染读取；写文件时删除四组 `[paletteN]` 和 `multicol`，只写 `multicolid=<0..2>`，防止继续产生可误认为可编辑的颜色参数。

- [ ] **步骤 3：让普通渲染按逻辑色阶直接取固定色**

在 `video_get_colour(int id, int brt_reg)` 开头保留红蓝立体原逻辑；普通模式返回 `colour_mode_value(tVBOpt.MULTIID, id)`。删除普通模式对 `MTINT/STINT` 的读取，保留函数签名和 `brt_reg` 参数以避免同时改动 Citro3D、CPU 回读和调用方。

- [ ] **步骤 4：运行公共层验证**

运行：

```bash
./build/colour_modes_test
rg -n 'colour_mode_value\(tVBOpt\.MULTIID, id\)' source/common/video_common.c
! rg -n 'MTINT|STINT' source/common/video_common.c
```

预期：测试通过；普通渲染只命中固定模式接口，不再命中可变色阶字段。

- [ ] **步骤 5：提交状态与渲染接入**

```bash
git add include/vb_set.h source/common/vb_set.c source/common/video_common.c tests/colour_modes_test.c
git commit -m "feat: render fixed four-shade colour modes"
```

### 任务 3：将 3DS 设置页改为三态整组切换

**文件：**
- 修改：`source/3ds/gui_hard.c:104-126, 510-612, 679-711, 2200-2218, 2421-2792, 3042-3065, 3555-3565, 3668-3678`

- [ ] **步骤 1：记录当前独立调节入口**

运行：

```bash
rg -n 'colour_filter|multicolour_picker|multicolour_settings|multicolour_wheel|swkbd_colour|swkbd_scale|handle_colour_wheel' source/3ds/gui_hard.c
```

预期：命中色轮、十六进制输入、倍率输入、四个 palette slot 及逐色阶编辑入口。

- [ ] **步骤 2：把色彩模式按钮改为三态选项**

将 `BARRIER_MODE` 改为 `.show_option=true`，其 `option_texts` 使用新建静态文本 `模式 1/模式 2/模式 3`。进入页面时令 `button.option=tVBOpt.MULTIID`；按 A 或触摸后执行 `(MULTIID + 1) % COLOUR_MODE_COUNT`，设置 `MODIFIED=true` 并留在当前页面。删除 `BARRIER_SETTINGS`，默认视图按钮上移到第二行。

- [ ] **步骤 3：增加只读 4 色预览**

在模式按钮下方绘制 4 个等宽色块，严格按运行时 `0,1,2,3` 顺序调用 `colour_mode_value()`。预览不注册为 Button、不响应触摸和方向键，避免形成任何单色阶可调入口。

- [ ] **步骤 4：删除所有手动编辑 UI**

删除 `colour_filter*`、`multicolour_*`、色轮/键盘输入/亮度倍率函数、对应 Button 数组、静态文案和 sprite 初始化引用；从 `SETUP_ALL_BUTTONS` 删除这些数组。`gfx/colour_wheel.png` 和 `gfx/sprites.t3s` 本轮不改，避免为不可见资产引入无关图集重排。

- [ ] **步骤 5：运行静态 UI 验收**

运行：

```bash
rg -n 'COLOUR_MODE_COUNT|colour_mode_value|模式 1|模式 2|模式 3' source/3ds/gui_hard.c
! rg -n 'colour_filter|multicolour_picker|multicolour_settings|multicolour_wheel|swkbd_colour|swkbd_scale|handle_colour_wheel|MULTI_BLACK|MULTIWHEEL' source/3ds/gui_hard.c
```

预期：只剩三态选择和只读预览；所有独立调节路径均无命中。

- [ ] **步骤 6：提交设置页变更**

实施前先检查 `git diff -- source/3ds/gui_hard.c`，把本任务补丁叠加到当前汉化改动上，不覆盖用户已有修改。

```bash
git add source/3ds/gui_hard.c
git commit -m "feat: replace colour editor with mode switch"
```

### 任务 4：构建、配置往返与 3DS 实机验收

**文件：**
- 验证：`include/colour_modes.h`
- 验证：`source/common/colour_modes.c`
- 验证：`source/common/vb_set.c`
- 验证：`source/common/video_common.c`
- 验证：`source/3ds/gui_hard.c`

- [ ] **步骤 1：运行本地静态和宿主机测试**

```bash
set -eu
git diff --check
cc -std=c11 -Wall -Wextra -Werror -Iinclude \
  tests/colour_modes_test.c source/common/colour_modes.c \
  -o build/colour_modes_test
./build/colour_modes_test
! rg -n 'colour_filter|multicolour_wheel|swkbd_colour|swkbd_scale' source/3ds/gui_hard.c
```

预期：全部退出码为 0。

- [ ] **步骤 2：执行 3DS Release 构建**

在已有 devkitPro 环境运行：

```bash
make clean
make release-3ds
test -s red-viper.cci
```

预期：编译无 `-Werror` 错误，并生成非空 `red-viper.cci`。若实现时当前分支尚未保留 `release-3ds` 目标，则用既有 `make release` 并验证 `red-viper.cia`/`red-viper.3dsx`，不为本功能改写 Makefile。

- [ ] **步骤 3：验证全局与游戏级配置往返**

在 3DS 上分别将模式 2 保存为全局、模式 3 保存为当前游戏，然后重启应用并重新加载游戏。预期：无游戏配置时恢复模式 2；进入该游戏后覆盖为模式 3；“放弃更改”和“恢复全局”仍通过现有 `loadFileOptions()/loadGameOptions()` 恢复正确模式。

- [ ] **步骤 4：逐模式进行画面验收**

选择同一游戏、同一画面，依次切换模式 1/2/3。每次切换应立即整组替换 4 个色阶，背景和三个非零色阶分别对应计划表；设置页不存在单色阶选择、色轮、十六进制输入或亮度倍率入口。另开启红蓝立体模式，确认左右眼颜色仍走现有 anaglyph 路径。

- [ ] **步骤 5：检查运行时边界**

验证 GPU 渲染、CPU 渲染、暂停后恢复、重置游戏和重新载入 ROM；确认模式切换不改变抗闪烁、默认视图、滑块模式、存档和输入配置。构建证明只覆盖结构正确性，最终色值和双眼画面以 3DS 实机截图/目视检查为准。

- [ ] **步骤 6：最终提交**

```bash
git add docs/superpowers/plans/2026-08-10-fixed-colour-modes.md
git commit -m "docs: add fixed colour modes implementation plan"
```

## 完成标准

- 设置页只有一个三态色彩模式选择器和只读 4 色预览。
- 模式切换一次更新全部 4 个逻辑色阶，12 个固定色值与上表一致。
- 全局/游戏级保存、放弃更改、恢复全局和旧 `multicolid` 配置迁移均有效。
- 普通 3DS GPU/CPU 渲染使用固定色表；红蓝立体渲染行为保持不变。
- 源码中不存在可到达的逐色阶编辑、色轮、十六进制颜色或亮度倍率调节路径。
- 宿主机映射测试和 3DS Release 构建通过；实机确认三种画面、模式持久化和切换即时生效。
