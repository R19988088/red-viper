# 选项、主题色与 ROM 浏览器单一数据源修复计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 删除选项页和 ROM 浏览器的双实现，使左栏预览与按 A 进入后的右栏共用同一套布局、颜色快照和目录模型，并通过同一份 CIA 完成实机刷新验收。

**架构：** `gui_hard.c` 只保留一个选项绘制函数 `draw_options_panel()`，预览态和交互态的区别仅是 `focused_row`。每帧生成一次 `MenuPaletteSnapshot`，本帧所有主菜单、选项和 ROM 绘制只消费该快照。ROM 目录扫描与路径、条目、游标、滚动、刷新代次统一放入 `RomBrowserModel`，预览和交互只切换焦点，不再复制目录数组。

**技术栈：** C11、libctru、Citro2D、POSIX `opendir/readdir`、宿主机断言测试、GitHub Actions devkitARM、makerom CIA。

---

## 当前基线与问题证据

- 基线提交：`29addb99ab3036ead6cf2728df501fb4e3ef1e17`。
- 基线 Action：`31401785912`，`Build release CIA` 和 `Publish direct CIA download` 均成功。
- 选项预览由 `draw_main_menu_preview(MAIN_MENU_OPTIONS)` 在 `source/3ds/gui_hard.c:926-942` 手工绘制。
- 按 A 后进入 `options()`，由 `LOOP_BEGIN(options_buttons)` 和通用 `handle_buttons()` 在 `source/3ds/gui_hard.c:2902-2938,3274-3468` 再绘制一次，因此进入前后比例、选中态和文字位置会漂移。
- 当前选项预览值块是 `x=216,w=88,center=260`；交互态由 `buttons[i].x + 96` 和 `buttons[i].x + 140` 派生，按钮自身 `x=120`，两条路径虽然局部数值接近，文字基线、选择矩形和通用按钮绘制仍不同。
- `MenuTheme`、`menu_background_color()`、`LOOP_BEGIN` 清屏和 `handle_buttons()` 的 `tVBOpt.TINT` 回退是并行取色路径；仅修改其中一条不能证明设备上不再出现旧蓝色。
- `RomPreviewCache` 与 `rom_loader_impl()` 内部的 `path/dirs/files/cursor/scroll_pos` 是两份目录状态。预览刷新成功不代表交互列表刷新，交互列表刷新也不会更新预览。
- 当前过滤规则只显示非隐藏的 `.vb/.zip` 文件，扩展名大小写不敏感；`archive_dir_t.index` 指向当前条目，不把 off-by-one 当作本次修复方向。
- 当前本地 `colour_modes_test` 与 `menu_navigation_test` 已通过，但它们没有覆盖选项两态像素契约、最终 Citro2D 打包色、目录增量刷新和预览/交互共享对象。

## 不再采用的修补方式

1. 不再分别调整预览态和进入态的坐标。
2. 不再通过同步 `tVBOpt.TINT` 去兼容主菜单的旧颜色回退。
3. 不再同时维护 `RomPreviewCache` 与 `rom_loader_impl()` 局部数组。
4. 不再把刷新频率从 500ms 改成 250ms 当作目录刷新修复；必须用刷新代次和新增文件测试证明扫描结果发生变化。
5. 不改 ROM 文件过滤范围，仍只接受 `.vb/.zip`；不新增菜单层级、文字确认按钮或文字返回按钮。

## 固定视觉契约

### 选项布局

预览态与交互态必须使用同一组常量：

| 项目 | 值 |
| --- | --- |
| 右栏范围 | `x=120..319, y=0..239` |
| 标签左边 | `x=128` |
| 值块左边 | `x=216` |
| 值块宽度 | `w=88` |
| 值文字中心 | `x=260` |
| 值块高度 | `h=32` |
| 四行顶部 | `y=24,72,120,168` |
| 标签字号 | `0.55` |
| 值字号 | `0.50` |

按 A 进入选项后不创建第二种布局。左侧标签不绘制选择条，焦点只改变右侧值块的颜色；`3D 模式` 始终显示 `Nintendo 3DS` 并保持禁用。

### 四色角色

`source/common/colour_modes.c` 的每个模式只有四个原始色值，最终 Citro2D 颜色统一打包为 `0xFF000000 | raw_colour`：

| 色阶 | 角色 | 使用位置 |
| --- | --- | --- |
| `COLOUR_SHADE_BACKGROUND` | 基础背景 | 下屏清屏、右栏空白、ROM 普通行背景 |
| `COLOUR_SHADE_DISABLED` | 禁用 | 不可用文字、禁用值块、次级线条 |
| `COLOUR_SHADE_READY` | 待选 | 可用未聚焦文字和值块 |
| `COLOUR_SHADE_ACTIVE` | 激活 | 当前焦点文字、当前值、ROM 当前行 |

主菜单、选项预览、选项交互和 ROM 页禁止直接读取 `tVBOpt.TINT`，禁止 `TINT_COLOR`、`TINT_*`、`COLOR_BRIGHTNESS()` 和固定 RGB。

### ROM 顶部与列表

- 顶部固定为 `y=0..31`，不参与列表滚动。
- `上一级` 固定在 `x=128,y=0,w=72,h=32`。
- 路径固定从 `x=200,y=4` 绘制，只显示 `/game/rom` 形式，不显示 `sdmc:`。
- 列表视口固定为 `x=120..319,y=32..239`，第一行从 `y=32` 开始。
- 列表滚动时顶部按钮和路径不移动，也不被条目覆盖。
- 左栏停在“加载 ROM”时立即绘制同一模型；按 A 只把输入焦点交给右栏。

## 文件结构

- 创建：`source/3ds/menu_panel_contract.h`，保存选项和 ROM 面板的唯一坐标常量。
- 创建：`include/rom_browser.h`，声明 `RomBrowserModel` 和目录刷新接口。
- 创建：`source/3ds/rom_browser.c`，实现 3DS 目录扫描、原子结果替换、游标恢复和刷新诊断。
- 创建：`tests/menu_panel_contract_test.c`，锁定选项预览/交互共享坐标和 ROM 顶部视口。
- 创建：`tests/rom_browser_test.c`，使用临时目录验证新增 `.zip/.vb` 文件在下一次刷新中出现。
- 创建：`tests/menu_source_contract_test.sh`，检查目标页面没有旧取色和第二套选项/ROM 数据源。
- 修改：`include/colour_modes.h`，声明最终 Citro2D 打包色接口。
- 修改：`source/common/colour_modes.c`，实现唯一颜色打包函数。
- 修改：`tests/colour_modes_test.c`，断言三种模式的 12 个最终 `u32`。
- 修改：`source/3ds/gui_hard.c`，接入单一颜色快照、单一选项绘制和共享 ROM 模型。
- 验证：`.github/workflows/build.yml`，沿用现有 devkitARM Action 生成并发布 `red-viper.cia`。

---

### 任务 1：先锁定布局和最终颜色契约

**文件：**
- 创建：`source/3ds/menu_panel_contract.h`
- 创建：`tests/menu_panel_contract_test.c`
- 修改：`include/colour_modes.h`
- 修改：`source/common/colour_modes.c`
- 修改：`tests/colour_modes_test.c`

- [ ] **步骤 1：写出唯一布局常量**

在 `source/3ds/menu_panel_contract.h` 定义：

```c
#ifndef MENU_PANEL_CONTRACT_H
#define MENU_PANEL_CONTRACT_H

enum {
    MENU_PANEL_X = 120,
    MENU_PANEL_W = 200,
    OPTIONS_LABEL_X = 128,
    OPTIONS_VALUE_X = 216,
    OPTIONS_VALUE_W = 88,
    OPTIONS_VALUE_CENTER_X = 260,
    OPTIONS_ROW_H = 32,
    OPTIONS_ROW_GAP = 16,
    ROM_HEADER_Y = 0,
    ROM_HEADER_H = 32,
    ROM_UP_X = 128,
    ROM_UP_W = 72,
    ROM_PATH_X = 200,
    ROM_LIST_Y = 32,
    ROM_LIST_BOTTOM = 240,
};

static inline int options_row_y(int row) {
    return 24 + row * (OPTIONS_ROW_H + OPTIONS_ROW_GAP);
}

#endif
```

- [ ] **步骤 2：写出失败的布局断言测试**

在 `tests/menu_panel_contract_test.c` 写入：

```c
#include <assert.h>
#include "menu_panel_contract.h"

int main(void) {
    assert(MENU_PANEL_X == 120);
    assert(MENU_PANEL_W == 200);
    assert(OPTIONS_LABEL_X == 128);
    assert(OPTIONS_VALUE_X == 216);
    assert(OPTIONS_VALUE_W == 88);
    assert(OPTIONS_VALUE_X + OPTIONS_VALUE_W / 2 == OPTIONS_VALUE_CENTER_X);
    assert(options_row_y(0) == 24);
    assert(options_row_y(1) == 72);
    assert(options_row_y(2) == 120);
    assert(options_row_y(3) == 168);
    assert(ROM_HEADER_Y == 0);
    assert(ROM_HEADER_H == ROM_LIST_Y);
    assert(ROM_PATH_X >= ROM_UP_X + ROM_UP_W);
    return 0;
}
```

- [ ] **步骤 3：运行布局测试并确认生产代码尚未接入**

运行：

```bash
mkdir -p build
cc -std=c11 -Wall -Wextra -Werror -Isource/3ds \
  tests/menu_panel_contract_test.c -o build/menu_panel_contract_test
./build/menu_panel_contract_test
rg -n 'OPTIONS_LABEL_X|ROM_LIST_Y' source/3ds/gui_hard.c
```

预期：测试程序退出码为 0；最后一条 `rg` 在生产代码中没有匹配，证明后续接入步骤仍会先失败。

- [ ] **步骤 4：增加唯一的 Citro2D 打包接口**

在 `include/colour_modes.h` 声明：

```c
unsigned colour_mode_c2d(int mode, int shade);
```

在 `source/common/colour_modes.c` 实现：

```c
unsigned colour_mode_c2d(int mode, int shade) {
    return 0xFF000000u | colour_mode_value(mode, shade);
}
```

这与当前颜色表的 `0xBBGGRR` 存储约定一致，页面代码不再自行拆分和重组 RGB。

- [ ] **步骤 5：扩充最终颜色测试**

在 `tests/colour_modes_test.c` 的双层循环中增加：

```c
assert(colour_mode_c2d(mode, shade) == (0xFF000000u | expected[mode][shade]));
```

并明确断言模式 1 的四个最终值：

```c
assert(colour_mode_c2d(0, 0) == 0xFF000000u);
assert(colour_mode_c2d(0, 1) == 0xFF040861u);
assert(colour_mode_c2d(0, 2) == 0xFF0A1294u);
assert(colour_mode_c2d(0, 3) == 0xFF2A29FFu);
```

- [ ] **步骤 6：运行两个宿主机测试**

```bash
cc -std=c11 -Wall -Wextra -Werror -Iinclude \
  tests/colour_modes_test.c source/common/colour_modes.c \
  -o build/colour_modes_test
./build/colour_modes_test
./build/menu_panel_contract_test
```

预期：两个程序均退出码 0。

- [ ] **步骤 7：提交契约测试**

```bash
git add source/3ds/menu_panel_contract.h tests/menu_panel_contract_test.c \
  include/colour_modes.h source/common/colour_modes.c tests/colour_modes_test.c
git commit -m "test: lock menu layout and packed palette colours"
```

### 任务 2：每帧只创建一次菜单颜色快照

**文件：**
- 修改：`source/3ds/gui_hard.c:149-183,218-252,747-763,890-1008,1439-1444,2902-2938,3274-3468`
- 创建：`tests/menu_source_contract_test.sh`

- [ ] **步骤 1：定义快照并删除 `MenuTheme` 分叉**

用以下类型替换 `MenuTheme`、`menu_theme()` 和 `menu_background_color()`：

```c
typedef struct {
    int mode;
    u32 shade[COLOUR_SHADE_COUNT];
} MenuPaletteSnapshot;

static MenuPaletteSnapshot menu_palette_snapshot(void) {
    MenuPaletteSnapshot result = {
        .mode = colour_mode_normalize(tVBOpt.MULTIID),
    };
    for (int shade = 0; shade < COLOUR_SHADE_COUNT; shade++)
        result.shade[shade] = colour_mode_c2d(result.mode, shade);
    return result;
}
```

- [ ] **步骤 2：让 frame loop 和通用按钮共用同一快照**

修改签名和宏：

```c
static inline int handle_buttons(Button buttons[], int count,
                                 const MenuPaletteSnapshot *palette);

#define HANDLE_BUTTONS(buttons, palette) \
    handle_buttons((buttons), sizeof((buttons)) / sizeof((buttons)[0]), (palette))
```

`LOOP_BEGIN` 在 `hidScanInput()` 前创建本帧局部变量：

```c
MenuPaletteSnapshot palette = menu_palette_snapshot();
C2D_TargetClear(screen, palette.shade[COLOUR_SHADE_BACKGROUND]);
```

`LOOP_END` 固定调用：

```c
if (loop) button = HANDLE_BUTTONS((buttons), &palette);
```

不要在同一 frame loop 内再次调用 `menu_palette_snapshot()`。

- [ ] **步骤 3：把目标页面改为显式接收快照**

签名统一为：

```c
static void draw_main_menu_shell(int active_item,
                                 const MenuPaletteSnapshot *palette);
static void draw_main_menu_preview(int active_item,
                                   const MenuPaletteSnapshot *palette);
static void draw_main_menu_panel(const MenuPaletteSnapshot *palette);
static void draw_options_panel(const MenuPaletteSnapshot *palette,
                               int focused_row);
static void draw_rom_browser(const RomBrowserModel *model,
                             const MenuPaletteSnapshot *palette,
                             bool content_focused);
```

`first_menu()`、`game_menu()`、`options()` 和 ROM frame loop 都把 `&palette` 传下去。背景、禁用、待选、激活分别只索引四个语义枚举。

- [ ] **步骤 4：删除通用按钮的旧蓝色回退**

在 `handle_buttons()` 中删除：

```c
if (base_colour == 0) base_colour = tVBOpt.TINT;
u32 normal_colour = COLOR_BRIGHTNESS(base_colour, 1.0);
u32 pressed_colour = COLOR_BRIGHTNESS(base_colour, 0.5);
```

对于 `.themed=true` 的按钮，直接使用传入快照：

```c
u32 normal_colour = palette->shade[COLOUR_SHADE_BACKGROUND];
u32 pressed_colour = palette->shade[COLOUR_SHADE_READY];
u32 disabled_colour = palette->shade[COLOUR_SHADE_DISABLED];
u32 active_colour = palette->shade[COLOUR_SHADE_ACTIVE];
```

非主题旧页面保留自身既有颜色逻辑，但主菜单、选项和 ROM 的所有按钮必须设置 `.themed=true`，不能落入旧分支。

- [ ] **步骤 5：写颜色来源静态检查**

在 `tests/menu_source_contract_test.sh` 中提取目标函数区间并检查：

```sh
#!/bin/sh
set -eu

source_file=source/3ds/gui_hard.c
target=$(sed -n '/static void draw_main_menu_shell/,/static void first_menu/p;
                 /static bool rom_loader_impl/,/static void multiplayer_main/p;
                 /static void options(/,/static void video_settings/p' "$source_file")

if printf '%s\n' "$target" | rg -n \
  'tVBOpt\.TINT|TINT_COLOR|TINT_[0-9]+|COLOR_BRIGHTNESS|menu_theme\(|menu_background_color\('; then
    echo "legacy menu colour source found" >&2
    exit 1
fi

printf '%s\n' "$target" | rg -q 'MenuPaletteSnapshot'
```

- [ ] **步骤 6：运行颜色回归**

```bash
chmod +x tests/menu_source_contract_test.sh
./tests/menu_source_contract_test.sh
./build/colour_modes_test
git diff --check
```

预期：静态检查无旧颜色来源，颜色断言测试退出码 0。

- [ ] **步骤 7：提交单一颜色快照**

```bash
git add source/3ds/gui_hard.c tests/menu_source_contract_test.sh
git commit -m "fix: use one palette snapshot per menu frame"
```

### 任务 3：让选项预览和交互使用同一个绘制函数

**文件：**
- 修改：`source/3ds/gui_hard.c:185-202,524-536,914-943,2902-2939,3393-3468`
- 修改：`tests/menu_source_contract_test.sh`

- [ ] **步骤 1：让选项按钮只负责输入**

在 `Button_t` 增加默认关闭的字段：

```c
bool input_only;
```

四个 `options_buttons` 都设置 `.input_only=true`。`handle_buttons()` 仍使用它们做方向键、A 键和触摸命中，但绘制阶段令：

```c
bool has_visual = !buttons[i].input_only &&
    (buttons[i].str || buttons[i].custom_draw ||
     buttons[i].show_toggle || buttons[i].show_option);
```

这样不会再由通用按钮系统绘制第二套选项 UI。

- [ ] **步骤 2：实现唯一选项绘制函数**

实现：

```c
static void draw_options_panel(const MenuPaletteSnapshot *palette,
                               int focused_row) {
    for (int row = OPTIONS_COLOUR; row <= OPTIONS_LANGUAGE; row++) {
        Button *option = &options_buttons[row];
        int y = options_row_y(row);
        bool disabled = option->disabled;
        bool active = row == focused_row && !disabled;
        u32 label = palette->shade[disabled ? COLOUR_SHADE_DISABLED
                                            : COLOUR_SHADE_READY];
        u32 value_bg = palette->shade[disabled ? COLOUR_SHADE_DISABLED
                                               : active ? COLOUR_SHADE_READY
                                                        : COLOUR_SHADE_BACKGROUND];
        u32 value_text = palette->shade[disabled ? COLOUR_SHADE_BACKGROUND
                                                 : active ? COLOUR_SHADE_ACTIVE
                                                          : COLOUR_SHADE_READY];

        C2D_DrawText(&option->text, C2D_AlignLeft | C2D_WithColor,
                     OPTIONS_LABEL_X, y + 6, 0, 0.55f, 0.55f, label);
        C2D_DrawRectSolid(OPTIONS_VALUE_X, y, 0,
                          OPTIONS_VALUE_W, OPTIONS_ROW_H, value_bg);
        C2D_DrawText(option->option_texts[option->option],
                     C2D_AlignCenter | C2D_WithColor,
                     OPTIONS_VALUE_CENTER_X, y + 6, 0,
                     0.50f, 0.50f, value_text);
    }
}
```

这里不绘制左侧标签选择条；预览和进入态的坐标、字号和文本完全相同。

- [ ] **步骤 3：两种状态只传不同焦点**

`draw_main_menu_preview(MAIN_MENU_OPTIONS, palette)` 调用：

```c
draw_options_panel(palette, -1);
```

`options()` frame loop 在 `LOOP_END` 前取得当前选项索引并调用：

```c
int focused_row = selectedButton >= &options_buttons[OPTIONS_COLOUR] &&
                  selectedButton <= &options_buttons[OPTIONS_LANGUAGE]
    ? (int)(selectedButton - options_buttons) : initial_button;
draw_options_panel(&palette, focused_row);
```

- [ ] **步骤 4：切换参数时不重建页面**

把 `options()` 改为一个外层控制循环；每次内层 frame loop 返回按钮索引后更新参数，再以该索引作为下一轮焦点。完整控制骨架为：

```c
static void options(int initial_button) {
    int focused_row = initial_button;
    for (;;) {
        sync_options_from_settings();
        LOOP_BEGIN(options_buttons, focused_row);
            draw_main_menu_shell(MAIN_MENU_OPTIONS, &palette);
            draw_options_panel(&palette, focused_row);
        LOOP_END(options_buttons);

        if (button == OPTIONS_BACK) return;
        focused_row = button;
        switch (button) {
            case OPTIONS_COLOUR:
                tVBOpt.MULTIID = (tVBOpt.MULTIID + 1) % COLOUR_MODE_COUNT;
                saveFileOptions();
                break;
            case OPTIONS_SLIDER:
                tVBOpt.SLIDERMODE = !tVBOpt.SLIDERMODE;
                saveFileOptions();
                break;
            case OPTIONS_LANGUAGE:
                tVBOpt.LANGUAGE = (tVBOpt.LANGUAGE + 1) % LANGUAGE_COUNT;
                saveFileOptions();
                refresh_menu_language();
                break;
        }
    }
}
```

删除当前三个 `[[gnu::musttail]] return options(...)`。参数分支保持为：

```c
case OPTIONS_COLOUR:
    tVBOpt.MULTIID = (tVBOpt.MULTIID + 1) % COLOUR_MODE_COUNT;
    saveFileOptions();
    break;
case OPTIONS_SLIDER:
    tVBOpt.SLIDERMODE = !tVBOpt.SLIDERMODE;
    saveFileOptions();
    break;
case OPTIONS_LANGUAGE:
    tVBOpt.LANGUAGE = (tVBOpt.LANGUAGE + 1) % LANGUAGE_COUNT;
    saveFileOptions();
    refresh_menu_language();
    break;
```

处理后继续下一帧，下一帧重新同步四个 `option` 值并生成新颜色快照。`OPTIONS_3D` 不进入 switch，保持不可操作。

- [ ] **步骤 5：增加单一绘制源检查**

在 `tests/menu_source_contract_test.sh` 增加：

```sh
test "$(rg -n 'static void draw_options_panel\(' "$source_file" | wc -l | tr -d ' ')" = 1
test "$(rg -n 'draw_options_panel\(' "$source_file" | wc -l | tr -d ' ')" = 3
! sed -n '/case MAIN_MENU_OPTIONS:/,/break;/p' "$source_file" | \
  rg 'C2D_DrawText|C2D_DrawRectSolid'
```

预期：一处定义、两个调用；`MAIN_MENU_OPTIONS` 分支不再自行画行。

- [ ] **步骤 6：运行布局和来源测试**

```bash
./build/menu_panel_contract_test
./tests/menu_source_contract_test.sh
git diff --check
```

预期：全部退出码 0。

- [ ] **步骤 7：提交单一选项 UI**

```bash
git add source/3ds/gui_hard.c tests/menu_source_contract_test.sh
git commit -m "fix: share one options panel between preview and focus"
```

### 任务 4：建立可测试的共享 ROM 浏览器模型

**文件：**
- 创建：`include/rom_browser.h`
- 创建：`source/3ds/rom_browser.c`
- 创建：`tests/rom_browser_test.c`
- 修改：`source/3ds/gui_hard.c:302-312,790-888,958-977,997-1005,1305-1710`

- [ ] **步骤 1：定义唯一模型**

在 `include/rom_browser.h` 定义：

```c
#ifndef ROM_BROWSER_H
#define ROM_BROWSER_H

#include <stdbool.h>
#include <stdint.h>

enum { ROM_BROWSER_PATH_MAX = 300 };

typedef struct {
    char path[ROM_BROWSER_PATH_MAX];
    char **dirs;
    int dir_count;
    char **files;
    int file_count;
    int cursor;
    float scroll_pos;
    uint64_t refresh_generation;
    uint64_t last_refresh_ms;
    int last_scan_result;
    bool valid;
} RomBrowserModel;

void rom_browser_init(RomBrowserModel *model, const char *rom_path);
void rom_browser_destroy(RomBrowserModel *model);
bool rom_browser_refresh(RomBrowserModel *model, uint64_t now_ms);
int rom_browser_entry_count(const RomBrowserModel *model);
const char *rom_browser_entry_name(const RomBrowserModel *model, int index);
bool rom_browser_entry_is_dir(const RomBrowserModel *model, int index);
bool rom_browser_enter_directory(RomBrowserModel *model, int index,
                                 uint64_t now_ms);
bool rom_browser_go_up(RomBrowserModel *model, uint64_t now_ms);

#endif
```

- [ ] **步骤 2：先写真实目录增量失败测试**

`tests/rom_browser_test.c` 使用 `mkdtemp()` 创建临时目录。测试辅助函数和路径初始化固定为：

```c
static void create_empty_file(const char *path) {
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fclose(file) == 0);
}

static int find_entry(const RomBrowserModel *model, const char *name) {
    for (int i = 0; i < rom_browser_entry_count(model); i++)
        if (strcmp(rom_browser_entry_name(model, i), name) == 0) return i;
    return -1;
}

char root_template[] = "/tmp/red-viper-rom-XXXXXX";
char *root = mkdtemp(root_template);
assert(root != NULL);

char initial_rom_path[PATH_MAX];
char old_rom_path[PATH_MAX];
char old_zip_path[PATH_MAX];
char new_zip_path[PATH_MAX];
char hidden_path[PATH_MAX];
char ignored_path[PATH_MAX];
char subdir_path[PATH_MAX];
snprintf(initial_rom_path, sizeof(initial_rom_path), "%s/old.vb", root);
snprintf(old_rom_path, sizeof(old_rom_path), "%s/old.vb", root);
snprintf(old_zip_path, sizeof(old_zip_path), "%s/OLD.ZIP", root);
snprintf(new_zip_path, sizeof(new_zip_path),
         "%s/Dragon Hopper (Japan).zip", root);
snprintf(hidden_path, sizeof(hidden_path), "%s/.hidden.vb", root);
snprintf(ignored_path, sizeof(ignored_path), "%s/ignored.txt", root);
snprintf(subdir_path, sizeof(subdir_path), "%s/subdir", root);
assert(mkdir(subdir_path, 0700) == 0);
create_empty_file(old_rom_path);
create_empty_file(old_zip_path);
create_empty_file(hidden_path);
create_empty_file(ignored_path);
```

随后依次执行：

```c
RomBrowserModel model;
rom_browser_init(&model, initial_rom_path);
assert(rom_browser_refresh(&model, 1000));
assert(rom_browser_entry_count(&model) == 3); /* 1 dir + old.vb + OLD.ZIP */

int old_index = find_entry(&model, "old.vb");
assert(old_index >= 0);
model.cursor = old_index;

create_empty_file(new_zip_path);
assert(rom_browser_refresh(&model, 1500));
assert(model.refresh_generation == 2);
assert(find_entry(&model, "Dragon Hopper (Japan).zip") >= 0);
assert(strcmp(rom_browser_entry_name(&model, model.cursor), "old.vb") == 0);

unlink(old_rom_path);
assert(rom_browser_refresh(&model, 2000));
assert(model.cursor >= 0);
assert(model.cursor < rom_browser_entry_count(&model));

rom_browser_destroy(&model);
unlink(old_zip_path);
unlink(new_zip_path);
unlink(hidden_path);
unlink(ignored_path);
rmdir(subdir_path);
rmdir(root);
```

同一测试还创建 `.hidden.vb` 和 `ignored.txt`，断言两者不出现在模型中。

- [ ] **步骤 3：运行测试确认模型尚不存在**

```bash
cc -std=c11 -Wall -Wextra -Werror -D_DEFAULT_SOURCE -Iinclude \
  tests/rom_browser_test.c source/3ds/rom_browser.c \
  -o build/rom_browser_test
```

预期：链接失败，缺少 `rom_browser_*` 实现。

- [ ] **步骤 4：实现原子刷新和文件名游标恢复**

`rom_browser_refresh()` 必须按以下顺序执行：

1. 保存当前条目名称，若没有条目则保存空字符串。
2. 新建本次扫描的临时 `new_dirs/new_files`，重新 `opendir(model->path)` 并读到结束。
3. 过滤隐藏项和非 `.vb/.zip` 文件，目录与文件分别大小写不敏感排序。
4. 扫描失败时释放临时数组，只更新 `last_scan_result`，保留上一份可用列表。
5. 扫描成功后才释放旧数组并交换新数组。
6. 以完整文件名恢复游标；找不到时把旧索引夹紧到新条目范围。
7. 更新 `last_refresh_ms`，令 `refresh_generation++`，设置 `valid=true` 和 `last_scan_result=0`。

宿主机使用 `stat()` 判断目录；`__3DS__` 分支沿用当前 `FS_DirectoryEntry.attributes`，不改变设备过滤语义。

- [ ] **步骤 5：实现路径动作后同步刷新**

`rom_browser_enter_directory()` 和 `rom_browser_go_up()` 先修改 `model->path`，随后在返回前调用 `rom_browser_refresh(model, now_ms)`。根目录 `sdmc:/` 的上一级动作返回 `false` 且不改变路径。

显示路径继续由纯函数去除 `sdmc:` 前缀，`sdmc:/game/rom/` 显示为 `/game/rom/`。

- [ ] **步骤 6：运行目录增量测试**

```bash
cc -std=c11 -Wall -Wextra -Werror -D_DEFAULT_SOURCE -Iinclude \
  tests/rom_browser_test.c source/3ds/rom_browser.c \
  -o build/rom_browser_test
./build/rom_browser_test
```

预期：退出码 0；测试在同一个已打开模型上新增 `Dragon Hopper (Japan).zip` 后，下一次刷新能找到该文件。

- [ ] **步骤 7：删除两份旧目录状态**

从 `gui_hard.c` 删除：

- `RomPreviewCache`、`rom_preview_cache`、`clear_rom_preview_cache()`、`refresh_rom_preview()`。
- `rom_loader_impl()` 内部静态 `path/old_dir`。
- `rom_loader_impl()` 内部 `dirs/files/dirCount/fileCount/cursor/scroll_pos/last_refresh`。
- 第二份手写 `opendir/readdir/qsort` 扫描循环。

文件中只保留一个：

```c
static RomBrowserModel rom_browser;
```

- [ ] **步骤 8：提交共享模型**

```bash
git add include/rom_browser.h source/3ds/rom_browser.c \
  tests/rom_browser_test.c source/3ds/gui_hard.c
git commit -m "fix: share one refreshable ROM browser model"
```

### 任务 5：让 ROM 预览与交互只切换焦点

**文件：**
- 修改：`source/3ds/gui_hard.c:302-312,958-1005,1010-1153,1313-1710`
- 修改：`tests/menu_source_contract_test.sh`

- [ ] **步骤 1：实现唯一 ROM 面板绘制函数**

`draw_rom_browser()` 固定先画列表视口，再覆盖顶部 32 像素，保证路径不会被列表挤动：

```c
static void draw_rom_browser(const RomBrowserModel *model,
                             const MenuPaletteSnapshot *palette,
                             bool content_focused) {
    C2D_DrawRectSolid(MENU_PANEL_X, 0, 0, MENU_PANEL_W, 240,
                      palette->shade[COLOUR_SHADE_BACKGROUND]);
    draw_rom_entries(model, palette, content_focused,
                     ROM_LIST_Y, ROM_LIST_BOTTOM);
    C2D_DrawRectSolid(MENU_PANEL_X, ROM_HEADER_Y, 0,
                      MENU_PANEL_W, ROM_HEADER_H,
                      palette->shade[COLOUR_SHADE_BACKGROUND]);
    draw_rom_up_button(model, palette, content_focused);
    draw_rom_path(model, palette, ROM_PATH_X, 4);
}
```

`draw_rom_entries()` 只绘制与 `y=32..239` 相交的条目；滚动条也从 `y=32` 开始。

- [ ] **步骤 2：定义四个同步刷新入口**

只在以下事件调用 `rom_browser_refresh()`：

1. 左栏焦点从其他分类移动到 `MAIN_MENU_LOAD_ROM`。
2. 在“加载 ROM”上按 A，把焦点交给右栏之前。
3. 右栏停留期间距离 `last_refresh_ms` 达到 500ms。
4. 进入子目录或执行“上一级”后。

每个成功刷新都能在 `refresh_generation` 中看到独立代次。不要用重新创建 `RomBrowserModel` 代替刷新。

- [ ] **步骤 3：预览与交互绘制同一个对象**

左栏焦点状态调用：

```c
draw_rom_browser(&rom_browser, palette, false);
```

按 A 后进入现有 `rom_loader()` 输入控制器，但该控制器不再创建路径、数组、游标或绘制布局，只操作全局 `rom_browser` 并调用：

```c
draw_rom_browser(&rom_browser, palette, true);
```

因此用户看到的右栏几何和内容对象不变，只有焦点状态变化。`rom_loader()` 继续保留 `bool` 返回值，以兼容联机主机、加入房间、加载错误重试等现有调用方；这些调用方也使用同一个模型和绘制函数。

- [ ] **步骤 4：固定输入语义**

- 左栏上下移动立即更换右栏内容，但不把焦点送入右栏。
- 左栏 A 才进入右栏。
- 右栏上下移动 `rom_browser.cursor`；当前条目滚出视口时调整同一模型的 `scroll_pos`。
- 顶部“上一级”是右栏可选目标，A 执行上一级；根目录显示占位并使用禁用色。
- 右栏 B 回左栏并保留当前分类和目录；左栏 B 在游戏运行时继续游戏。
- ROM 文件按 A 后沿用现有加载流程，游戏运行时允许直接切换 ROM。

- [ ] **步骤 5：加入设备可见的刷新诊断**

在 `DEBUGLEVEL > 0` 时，ROM 面板底部绘制一行：

```text
scan=<generation> dirs=<dir_count> files=<file_count> rc=<last_scan_result>
```

诊断只读取模型字段，不发起额外扫描；Release `DEBUGLEVEL=0` 不绘制该行。

- [ ] **步骤 6：增加单一 ROM 数据源检查**

在 `tests/menu_source_contract_test.sh` 增加：

```sh
! rg -n 'RomPreviewCache|rom_preview_cache|refresh_rom_preview' "$source_file"
test "$(rg -n 'static RomBrowserModel rom_browser;' "$source_file" | wc -l | tr -d ' ')" = 1
test "$(rg -n 'draw_rom_browser\(' "$source_file" | wc -l | tr -d ' ')" = 3
! sed -n '/static bool rom_loader_impl/,/static void multiplayer_main/p' "$source_file" | \
  rg 'opendir|readdir|qsort|char \*\*dirs|char \*\*files'
```

- [ ] **步骤 7：运行 ROM 和来源测试**

```bash
./build/rom_browser_test
./build/menu_panel_contract_test
./tests/menu_source_contract_test.sh
git diff --check
```

预期：全部退出码 0；源文件中不存在 ROM 预览缓存和第二份目录数组。

- [ ] **步骤 8：提交统一 ROM 页面**

```bash
git add source/3ds/gui_hard.c tests/menu_source_contract_test.sh
git commit -m "fix: keep ROM preview and focus on one browser state"
```

### 任务 6：全链验证、推送 Action 并生成 CIA

**文件：**
- 验证：`source/3ds/gui_hard.c`
- 验证：`source/3ds/rom_browser.c`
- 验证：`source/3ds/menu_panel_contract.h`
- 验证：`source/common/colour_modes.c`
- 验证：`tests/colour_modes_test.c`
- 验证：`tests/menu_navigation_test.c`
- 验证：`tests/menu_panel_contract_test.c`
- 验证：`tests/rom_browser_test.c`
- 验证：`tests/menu_source_contract_test.sh`
- 验证：`.github/workflows/build.yml`

- [ ] **步骤 1：运行全部宿主机回归**

```bash
set -eu
mkdir -p build
cc -std=c11 -Wall -Wextra -Werror -Iinclude \
  tests/colour_modes_test.c source/common/colour_modes.c \
  -o build/colour_modes_test
cc -std=c11 -Wall -Wextra -Werror -Iinclude \
  tests/menu_navigation_test.c source/common/menu_navigation.c \
  -o build/menu_navigation_test
cc -std=c11 -Wall -Wextra -Werror -Isource/3ds \
  tests/menu_panel_contract_test.c -o build/menu_panel_contract_test
cc -std=c11 -Wall -Wextra -Werror -D_DEFAULT_SOURCE -Iinclude \
  tests/rom_browser_test.c source/3ds/rom_browser.c \
  -o build/rom_browser_test
./build/colour_modes_test
./build/menu_navigation_test
./build/menu_panel_contract_test
./build/rom_browser_test
./tests/menu_source_contract_test.sh
git diff --check
```

预期：所有命令退出码 0。

- [ ] **步骤 2：检查变更范围和提交历史**

```bash
git status --short
git diff origin/master...HEAD --stat
git log --oneline origin/master..HEAD
```

预期：只包含本计划列出的源文件、测试和计划文档；没有改动 ROM 核心、渲染器、存档格式或签名配置。

- [ ] **步骤 3：推送并等待 GitHub Action**

```bash
git push origin master
run_id=$(gh run list --workflow build.yml --branch master --limit 1 \
  --json databaseId --jq '.[0].databaseId')
gh run watch "$run_id" --exit-status
gh run view "$run_id" --json headSha,status,conclusion,url,jobs
```

预期：`headSha` 等于本次推送提交，`Build release CIA` 和 `Publish direct CIA download` 均为 `success`。

- [ ] **步骤 4：下载并校验 CIA**

```bash
tag="red-viper-build-${run_id}-1"
release_dir="/tmp/${tag}"
rm -rf "$release_dir"
mkdir -p "$release_dir"
gh release download "$tag" --pattern red-viper.cia --dir "$release_dir"
file "$release_dir/red-viper.cia"
shasum -a 256 "$release_dir/red-viper.cia"
```

预期：`red-viper.cia` 非空，文件名和 Action 上传格式均为 CIA 而非 CCI；记录绝对路径、下载链接、文件大小、`file` 原始输出和 SHA-256。

- [ ] **步骤 5：用同一份 CIA 做选项实机验收**

1. 默认中文、模式 1 打开菜单，把左栏移动到“选项”。
2. 记录未按 A 时四行的标签位置、值块大小和文字中心。
3. 按 A 进入右栏；四行不得移动、缩放或变成旧版，左侧标签不得出现选择条。
4. 上下移动经过四行，`3D 模式` 保持禁用且不可操作。
5. 切换色彩模式、滑块模式和中日英语言，值在当前页实时变化。
6. 三个色彩模式分别截图，主菜单、右栏背景和值块只能出现对应模式的四个颜色。

- [ ] **步骤 6：用同一份 CIA 做 ROM 实机验收**

1. 打开菜单并停在“加载 ROM”，确认顶部为“上一级 + 当前路径”，列表从 `y=32` 开始。
2. 记录当前目录与初始 `refresh_generation`；按 A 进入右栏时必须再增加一次。
3. 保持应用运行，通过 FTP 在当前显示路径新增 `Dragon Hopper (Japan).zip`。
4. 最迟在下一次 500ms 定时刷新后，预览和交互列表必须同时出现该文件。
5. B 回左栏，再切走并切回“加载 ROM”；该文件仍存在，刷新代次再次增加。
6. 进入子目录并返回上一级，路径固定在顶部，列表滚动不推动路径。
7. 游戏运行时从菜单进入“加载 ROM”，选择另一个 ROM，确认直接切换游戏。

- [ ] **步骤 7：保存验收证据**

记录以下内容：

```text
commit=<40 位 SHA>
action=<Action URL>
cia=<Release URL>
sha256=<CIA SHA-256>
colour_mode_1=<截图路径>
colour_mode_2=<截图路径>
colour_mode_3=<截图路径>
rom_before=<条目数与刷新代次>
rom_after=<条目数与刷新代次，含 Dragon Hopper (Japan).zip>
```

只有 Action、CIA 校验、选项两态一致、模式 1 背景不再出现旧蓝色、FTP 新文件可见五项全部有证据后，才把修复标记完成。
