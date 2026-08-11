# 3DS 菜单布局、Logo 与存档时间实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans` 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法跟踪进度。

**目标：** 按参考图调整 3DS 下屏菜单的对齐、字号、间距、Logo 透明度、版本文案、存档时间和左侧亮度，并保留现有主题与输入路由。

**架构：** 继续使用 `source/3ds/gui_hard.c` 的 Citro2D 菜单绘制与 `Button` 输入模型；把布局数值集中到现有 `menu_panel_contract.h`，把左侧 80% 亮度计算抽成可宿主机测试的纯函数。存档显示改为保存时用机内时钟写入每个槽位的时间元数据，读取时只接受该元数据并用本地时区格式化，避免继续显示固定的 `01-01 00:00`。

**技术栈：** C11/Citro2D/libctru、devkitARM、现有 `colour_modes` 测试、Python/Pillow（仅用于一次性 RGBA 资源转换）。

---

## 已确认现状与约束

- 主菜单已固定为 `x=8..112`、每行 28 像素、8 项共 224 像素；`style_main_menu()` 目前从 `y=8` 开始紧凑排列，因此“垂直居中”应改为按可见项总高度计算起始 `y`，而不是把整个区域写死在顶部。
- `draw_main_menu_shell()` 当前把整屏用背景色填充，左侧没有独立亮度层；主题颜色来自 `MenuPaletteSnapshot`，不能写死某一种模式的 RGB。
- `gfx/logo.png` 为 198×63 RGBA，但 12,474 个像素的 alpha 全部为 255，其中 5,741 个像素接近黑色；需真正移除黑色背景并保留透明 alpha。
- `Makefile` 当前定义 `VERSION_MAJOR/MINOR/MICRO = 1/3/2`，但 `FULL_VERSION` 会追加 git hash；关于页因此不会稳定显示指定的 `v1.3.2 / ddd mod 1.1`。
- `emulation_state_mtime()` 当前读取存档文件 `stat().st_mtime`。这不能保证是机内保存瞬间，截图中的 `01-01 00:00` 即为需要消除的结果。
- 本地 `gh release list` 当前稳定标签是 `v1.0`，另有自动生成的 prerelease；代码版本 `v1.3.2` 与线上稳定标签不一致。实施前需按“版本单一来源”任务处理，不能声称当前线上已发布 `v1.3.2`。
- 用户所说“下面的文件尺寸”按截图理解为 Logo 下方的关于页文字尺寸；计划按“文字尺寸”实施，字号从 `0.35` 调为整数百分比表达的 `0.525`（52.5%），行距增加 50%，并改为右侧面板内左对齐。若该词确实指 ROM 文件名，执行前需改为 ROM 列表规格。

## 文件清单

- 修改：`source/3ds/gui_hard.c`：菜单垂直居中、字号/间距、左侧亮度层、关于页排版、版本文案、时间元数据显示。
- 修改：`source/3ds/menu_panel_contract.h`：布局常量、字号百分比和左侧亮度比例。
- 修改：`include/colour_modes.h`、`source/common/colour_modes.c`：增加纯函数 `colour_scale_rgb()`，按 80% 计算左侧区域颜色。
- 修改：`include/vb_gui.h`、`source/common/vb_gui.c`：增加存档时间元数据读写接口，保存时取机内 `time(NULL)`。
- 新增：`include/savestate_time.h`、`source/common/savestate_time.c`：提供不依赖 3DS 全局状态的时间元数据编码/解码与路径辅助函数。
- 修改：`Makefile`：以语义版本和 mod 版本组成稳定显示文本，保留 CIA/CCI 打包版本字段。
- 修改：`.github/workflows/build.yml`：Release 标签/显示版本校验使用 Makefile 语义版本，避免发布资产与界面版本漂移。
- 修改：`gfx/logo.png`：移除黑色背景，输出仍为 198×63 RGBA。
- 修改：`tests/menu_panel_contract_test.c`、`tests/colour_modes_test.c`：布局、字号、亮度计算回归。
- 新增：`include/savestate_time.h`、`source/common/savestate_time.c`：时间元数据序列化、文件读写辅助。
- 新增：`tests/savestate_time_test.c`：时间元数据序列化、空槽和读取失败回归。

### 任务 1：先锁定布局与颜色计算契约

**文件：**
- 修改：`source/3ds/menu_panel_contract.h`
- 修改：`include/colour_modes.h`、`source/common/colour_modes.c`
- 修改：`tests/menu_panel_contract_test.c`、`tests/colour_modes_test.c`

- [ ] **步骤 1：扩展布局测试并确认整数字号**

在 `menu_panel_contract.h` 增加以下常量，测试直接锁定结果，避免后续绘制路径各自取值：

```c
enum {
    MAIN_MENU_ROW_H = 28,
    MAIN_MENU_TEXT_PERCENT = 63, /* 0.70 * 0.90, expressed as an integer percent */
    ABOUT_TEXT_PERCENT = 53,     /* 0.35 * 1.50, rounded to an integer percent */
    ABOUT_LINE_GAP_PERCENT = 150,
    MENU_LEFT_BRIGHTNESS_PERCENT = 80,
};
```

测试断言 8 项可见时起始 y 为 8，4 项可见时起始 y 为 64，且字号百分比为 63；运行 `cc -std=c11 -Wall -Wextra -Werror -Iinclude -Isource/3ds tests/menu_panel_contract_test.c -o build/menu_panel_contract_test && ./build/menu_panel_contract_test`，预期退出码为 0。

- [ ] **步骤 2：先写亮度纯函数测试**

新增 `colour_scale_rgb(unsigned rgb, unsigned percent)`，只缩放低 24 位 RGB，保留 alpha 由调用方补齐。测试黑色、白色、模式 1 背景及边界：`0xFFFFFF * 80% == 0xCCCCCC`、`0x000000 == 0`、百分比 0/100 不越界。

- [ ] **步骤 3：实现纯函数并运行宿主机测试**

实现使用整数通道计算 `(channel * percent + 50) / 100`，明确采用四舍五入，避免浮点数在 ARM 与宿主机上产生不同像素。运行：

```bash
mkdir -p build
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/colour_modes_test.c \
  source/common/colour_modes.c -o build/colour_modes_test
./build/colour_modes_test
```

- [ ] **步骤 4：提交契约变更**

```bash
git diff --check
git add source/3ds/menu_panel_contract.h include/colour_modes.h \
  source/common/colour_modes.c tests/menu_panel_contract_test.c tests/colour_modes_test.c
git commit -m "test: lock menu layout and brightness math"
```

### 任务 2：实现左侧垂直居中、字号和亮度

**文件：**
- 修改：`source/3ds/gui_hard.c:758-855, 970-1000`

- [ ] **步骤 1：写失败的静态契约检查**

在 `tests/menu_source_contract_test.sh` 增加检查：`style_main_menu()` 必须出现 `MENU_SCREEN_H`、`MAIN_MENU_ROW_H` 和可见项计数；`draw_main_menu_shell()` 必须调用 `colour_scale_rgb`，且不能出现固定的 `0.80f` 左侧颜色常量。

- [ ] **步骤 2：按可见项总高度计算起始 y**

在 `style_main_menu()` 先统计 `visible_count`，使用 `start_y = (MENU_SCREEN_H - visible_count * MAIN_MENU_ROW_H) / 2`，逐项写入 y；保持隐藏项不占行，左右边界和触摸矩形不变。

- [ ] **步骤 3：统一主菜单字号为 63%**

将主菜单绘制和通用按钮绘制的默认比例从 `0.7f` 改为 `MAIN_MENU_TEXT_PERCENT / 100.0f`，并把垂直基线按实际字号居中，确保文本不会压到相邻行。右侧其他页面的字号只在各自任务中变更。

- [ ] **步骤 4：用代码计算左侧 20% 降亮**

在 `draw_main_menu_shell()` 先绘制主题背景，再对 `x=0..119` 绘制 `colour_scale_rgb(palette->shade[COLOUR_SHADE_BACKGROUND], 80)` 结果；alpha 使用 `0xFF000000u` 合成。右侧面板仍使用原主题背景，保证亮度差来自运行时 palette，而不是固定 RGB。

- [ ] **步骤 5：运行静态和宿主机验证**

```bash
./build/menu_panel_contract_test
./build/colour_modes_test
./tests/menu_source_contract_test.sh source/3ds/gui_hard.c
git diff --check
```

预期：三个测试均通过；对 `colour_scale_rgb` 的引用只有菜单绘制与颜色测试路径。

### 任务 3：清理 Logo 背景并重排关于页文字

**文件：**
- 修改：`gfx/logo.png`
- 修改：`source/3ds/gui_hard.c:952-960, 2858-2870, 3250-3295`

- [ ] **步骤 1：生成透明 Logo 并验证像素**

以 `gfx/logo.png` 为输入，将连续黑色背景（RGB 通道均小于 8）转换为 alpha 0，保留非黑色 Logo 像素及抗锯齿边缘；不得改变 198×63 尺寸。用 Pillow 验证 `len(set(alpha)) > 1` 且存在 alpha 0 与 alpha 255，禁止用黑色图层覆盖。

- [ ] **步骤 2：删除关于页黑色 tint 依赖**

关于页仍可按主题给 Logo 着色，但 `C2D_DrawSpriteTinted` 的输入必须使用透明资源；不再通过黑色背景模拟透明。保留 Logo 位于右侧顶部的坐标边界。

- [ ] **步骤 3：放大并左对齐 Logo 下文字**

将关于页文字比例设为 `ABOUT_TEXT_PERCENT / 100.0f`，行距乘 `ABOUT_LINE_GAP_PERCENT / 100.0f`，绘制起点改为右侧面板左内边距 `MENU_PANEL_X + 8`，使用 `C2D_AlignLeft`；动态测量最长行，超出 `MENU_PANEL_X + MENU_PANEL_W - 8` 时按现有文本测量接口截断。

- [ ] **步骤 4：锁定版本文案**

把关于页第一行从 `VERSION` 改为 `VERSION " / ddd mod 1.1"`，其中 `VERSION` 继续由 Makefile 传入的语义版本生成，不再显示 git hash。所有联机协议版本字段仍使用纯 `VERSION`，避免把 mod 文案带进协议比较。

- [ ] **步骤 5：验证资源与文本**

```bash
python3 - <<'PY'
from PIL import Image
im = Image.open('gfx/logo.png').convert('RGBA')
assert im.size == (198, 63)
alpha = [p[3] for p in im.getdata()]
assert min(alpha) == 0 and max(alpha) == 255
PY
rg -n 'ddd mod 1\.1|ABOUT_TEXT_PERCENT|C2D_AlignLeft' source/3ds/gui_hard.c
git diff --check
```

### 任务 4：改为机内真实时间的存档元数据

**文件：**
- 修改：`include/vb_gui.h`
- 修改：`source/common/vb_gui.c`
- 修改：`source/3ds/gui_hard.c:775-798, 2798-2856`
- 新增：`tests/savestate_time_test.c`

- [ ] **步骤 1：先写时间元数据测试**

测试覆盖：写入 `time_t` 后读回相等；损坏/缺失元数据返回 false；空槽仍显示 `NN  --`；有效时间使用 `localtime_r` 和 `%m-%d %H:%M` 格式。测试不依赖实际 SD 卡路径。

- [ ] **步骤 2：定义并实现元数据接口**

在 `include/savestate_time.h` 声明可宿主机测试的编码/解码函数，在 `vb_gui.h` 暴露业务接口：

```c
bool emulation_state_mtime(int state, time_t *mtime);
bool emulation_state_time_write(int state, time_t timestamp);
```

`emulation_sstate()` 完成状态文件 `fclose()` 成功后调用 `time(NULL)`，把秒数通过 `savestate_time_write_file()` 写入同槽 `.time` 元数据文件；任何写失败都让保存返回失败。读取优先使用 `.time`，对旧版本只有 `.rvs` 的槽回退到 `stat().st_mtime`，这样旧存档仍可见但新保存一定使用机内时钟。

- [ ] **步骤 3：显示本地机内时间**

`refresh_savestate_cache()` 读取上述接口，使用 `localtime_r`（不使用共享静态 `localtime` 缓冲区）和 `strftime("%m-%d %H:%M")`；保存成功后立即刷新缓存，加载按钮仅在元数据或旧文件存在时可用。

- [ ] **步骤 4：运行测试并检查失败路径**

```bash
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/savestate_time_test.c \
  source/common/savestate_time.c -o build/savestate_time_test
./build/savestate_time_test
rg -n 'time\(NULL\)|localtime_r|strftime|\.time' source/common/vb_gui.c source/3ds/gui_hard.c
git diff --check
```

### 任务 5：版本单一来源与 Release 校验

**文件：**
- 修改：`Makefile:1-4,66-73`
- 修改：`.github/workflows/build.yml`

- [ ] **步骤 1：稳定显示版本**

保留 `VERSION_MAJOR/MINOR/MICRO` 作为唯一语义版本源，定义 `VERSION := v1.3.2` 与 `MOD_VERSION := ddd mod 1.1`，`FULL_VERSION` 只传 `$(VERSION)`，关于页在代码中拼接 mod 后缀；git hash 仅保留在构建日志或诊断信息，不进入用户版本号。

- [ ] **步骤 2：让 Release 标签校验语义版本**

工作流构建后读取 `make -s print-version`（新增只读目标），稳定标签发布时要求 `${tag}` 等于 `v$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_MICRO)`；手动 prerelease 使用独立构建标签，但资产内显示仍为同一语义版本。失败时在上传前退出，防止发布名称与 UI 不一致。

- [ ] **步骤 3：验证当前线上差异并记录验收**

运行：

```bash
make -s print-version
gh release list --repo R19988088/red-viper --limit 10
```

预期本地输出为 `v1.3.2`，并明确当前线上 `v1.0` 尚未自动改写；只有创建对应 Release 后，线上版本才算完成同步。

### 任务 6：完整验证与设备验收

- [ ] **步骤 1：宿主机测试**

```bash
mkdir -p build
cc -std=c11 -Wall -Wextra -Werror -Iinclude -Isource/3ds \
  tests/menu_panel_contract_test.c -o build/menu_panel_contract_test
./build/menu_panel_contract_test
./build/colour_modes_test
./build/savestate_time_test
./tests/menu_source_contract_test.sh source/3ds/gui_hard.c
git diff --check
```

- [ ] **步骤 2：3DS Release 构建**

在 `devkitpro/devkitarm` 环境运行 `make release-3ds`，检查最终 CIA/3DSX 的版本字段仍为 1.3.2；不以宿主机测试替代 ARM 编译证据。

- [ ] **步骤 3：实机逐项验收**

记录 320×240 下屏截图：左侧导航按可见项垂直居中、字号比基线小 10%（63%整数比例）、左侧动态降低 20% 亮度、右侧 Logo 无黑底、Logo 下文字放大 50%/间距放大 50%/左对齐、显示 `v1.3.2 / ddd mod 1.1`。保存两个槽位后断电重启，确认显示的是机内实际保存时间而非 `01-01 00:00`，并验证空槽、加载失败和旧存档回退。

## 规格自检

- 每条用户需求均有对应任务：垂直居中/字号/Logo/文字排版见任务 2-3，版本见任务 3/5，真实时间见任务 4，动态亮度见任务 1-2。
- 未引入新的持久配置项；`.time` 仅为现有存档槽的伴随元数据，旧 `.rvs` 可读取。
- 未把 mod 文案放入联机协议版本比较，避免协议兼容性改变。
- 当前线上 Release 标签与代码版本不一致已明确记录，发布同步是可验证的独立步骤。
