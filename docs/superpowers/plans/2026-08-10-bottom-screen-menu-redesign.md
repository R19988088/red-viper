# 下屏菜单与全局三项选项改版实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法跟踪进度。

**目标：** 按参考图把 3DS 下屏改为左侧单列主菜单与右侧内容区，重做 ROM/游戏进度页面，将选项收敛为可实时切换的色彩模式、3D 模式、滑块模式，并让全部设置只使用全局配置。

**架构：** 保留现有 Citro2D `Button` 输入和各业务函数，在 `source/3ds/gui_hard.c` 内增加统一的主菜单框架、主题角色色与无文字返回协议；主菜单只负责左侧导航，ROM、游戏进度、选项、联机和关于在右侧内容区绘制或从对应入口进入。主题色直接由现有三组固定色彩模式派生，设置值点击后立即应用并写入全局 INI，不再加载或保存单游戏配置。

**技术栈：** C11、Citro2D/Citro3D、libctru 输入与文件系统、现有 `VB_OPT`/INI 配置、GNU Make、devkitARM。

---

## 已确认现状与设计决定

- 当前下屏是多组全屏 `Button` 数组：主菜单位于 `source/3ds/gui_hard.c:208-231`，ROM 浏览器位于 `source/3ds/gui_hard.c:951-1243`，选项及视频子菜单位于 `source/3ds/gui_hard.c:2464-2625`，存档页位于 `source/3ds/gui_hard.c:2745-2800`。
- 当前按钮全部使用 `tVBOpt.TINT` 实色底和黑字；选中态只有底部黑线，不能表达参考图中的“未选中无底色、选中有深色底、文字高亮”。
- 当前固定色彩模式已经由 `colour_mode_value(mode, shade)` 提供 3 组、每组 4 色；无需新增另一套 UI 颜色配置。
- 主菜单顺序固定为：`继续游戏`、`重新开始`、`游戏进度`、`加载 ROM`、`联机`、`选项`、`关于`、`退出`。未加载游戏时隐藏前三项，默认选中 `加载 ROM`。
- `继续游戏`直接回到游戏；`重新开始`直接执行现有清屏及 `AKILL | VBRESET` 路径，不显示确认页。`退出`仍可保留确认提示，但只接受 A 确认、B 取消，不绘制“是/否/确认/返回”文字按钮。
- `游戏进度`列出 10 个现有存档槽，显示槽号与文件修改时间；空槽显示 `--`。底部只保留 `保存`、`加载`两个业务按钮，不保留删除、上一槽、下一槽或成功后的二级菜单。保存成功后原地刷新时间，加载成功后直接回到游戏。
- `加载 ROM`沿用 `.vb`/`.zip` 过滤、滚动、触摸拖动、最近目录和加载流程；只是把列表裁入右侧内容区。X 进入上级目录，A 打开目录或加载 ROM，B 回到左侧主菜单，不绘制“上级/返回”文字按钮。
- `选项`右侧只显示三行：`色彩模式`、`3D 模式`、`滑块模式`。A 或触摸每次循环到下一个值，立刻调用现有运行时切换逻辑并写入全局配置；不进入视频、控制、声音、性能等二级菜单。
- 三项值分别为：色彩 `模式 1/模式 2/模式 3`；3D `Nintendo 3DS/立体色差`；滑块 `Nintendo 3DS/Virtual Boy IPD`。沿用现有枚举和布尔语义，不新增开关。
- 所有页面的返回统一为 B；需要确认的提示统一为 A 确认、B 取消。可显示系统 A/B 按键图标提示，但不显示文字“返回”“确认”“是”“否”。
- 关于页底部返回按钮删除，新增署名行 `配色模式与 UI 改版：ddd`。
- 全局设置定义为：启动时只读 `CONFIG_FILENAME`，ROM 加载后不再叠加 `configs/<game>.ini`；选项切换后只写全局文件。保留旧单游戏 INI 文件在 SD 卡上但忽略它们，避免实施时删除用户数据。

## 视觉规格

### 布局

- 下屏逻辑尺寸保持 `320 x 240`，背景纯黑。
- 左侧导航区：`x=8, y=8, w=104, h=224`；每项固定高 `28`，8 项刚好排满。隐藏游戏相关项时其余项目保持原顺序并向上紧凑排列。
- 右侧内容区：`x=120, y=8, w=192, h=224`；ROM 列表、存档列表、选项和关于共享此边界。
- 不嵌套装饰卡片；右侧只有一块连续内容底和必要的列表行/命令按钮。

### 主题角色

从当前 `tVBOpt.MULTIID` 直接读取色阶，并在 `gui_hard.c` 集中生成以下角色：

```c
typedef struct {
    u32 nav_text;
    u32 nav_selected_bg;
    u32 nav_selected_text;
    u32 panel_bg;
    u32 row_selected_bg;
    u32 row_selected_text;
} MenuTheme;

static MenuTheme menu_theme(void) {
    int mode = colour_mode_normalize(tVBOpt.MULTIID);
    return (MenuTheme) {
        .nav_text = colour_mode_value(mode, 2) | 0xff000000,
        .nav_selected_bg = COLOR_BRIGHTNESS(colour_mode_value(mode, 1), 0.70f),
        .nav_selected_text = colour_mode_value(mode, 3) | 0xff000000,
        .panel_bg = COLOR_BRIGHTNESS(colour_mode_value(mode, 1), 0.45f),
        .row_selected_bg = COLOR_BRIGHTNESS(colour_mode_value(mode, 2), 0.55f),
        .row_selected_text = colour_mode_value(mode, 3) | 0xff000000,
    };
}
```

实现时先用 3DS 构建验证 `COLOR_BRIGHTNESS` 的打包结果；若宏返回值已含 alpha，则不得再次 OR alpha。色彩模式切换发生在同一帧循环返回前，下一帧必须同时更新游戏配色预览、左侧导航、右侧面板、列表高亮和文字颜色。

### 选中与未选中

- 左侧未选中项：透明底，仅绘制 `nav_text`。
- 左侧选中项：绘制 `nav_selected_bg` 整行底色，文字使用 `nav_selected_text`，不再使用黑色下划线。
- 右侧普通列表项：透明底、根据主题对比度选择正文色；选中项绘制 `row_selected_bg`，文字使用 `row_selected_text`。
- 禁用的“加载”命令使用主题色 35% 亮度且不能被方向键或触摸激活，避免空槽加载。

## 文件结构

- 修改：`source/3ds/gui_hard.c`，实现下屏框架、按钮视觉状态、ROM/存档/选项/关于内容区及 A/B 行为。
- 修改：`include/vb_gui.h`，声明存档时间查询接口。
- 修改：`source/common/vb_gui.c`，通过现有存档路径和 `stat()` 返回槽位存在性与修改时间。
- 修改：`include/vb_set.h`，移除单游戏配置公开接口及仅服务旧选项保存流程的状态字段。
- 修改：`source/common/vb_set.c`，停止单游戏 INI 读写，保持全局配置写入。
- 修改：`source/3ds/main.c`，确认退出保存仍只写全局配置。
- 扩展：`tests/colour_modes_test.c`，锁定三种主题取色所依赖的模式规范化和色阶值。
- 验证：`Makefile`、`Makefile.linux`，不改变构建目标和 Release 打包范围。

---

### 任务 1：建立主题角色与按钮显示状态

**文件：**
- 修改：`source/3ds/gui_hard.c:34-49, 144-157, 2918-3056`
- 测试：`tests/colour_modes_test.c`

- [ ] **步骤 1：扩展固定模式回归测试**

在 `tests/colour_modes_test.c` 增加对 UI 会读取的 `shade 1/2/3` 和非法模式回退断言：

```c
assert(colour_mode_value(0, 1) == 0x040861);
assert(colour_mode_value(0, 2) == 0x0A1294);
assert(colour_mode_value(0, 3) == 0x2A29FF);
assert(colour_mode_value(99, 3) == 0x2A29FF);
```

- [ ] **步骤 2：运行宿主机测试作为基线**

```bash
mkdir -p build
cc -std=c11 -Wall -Wextra -Werror -Iinclude \
  tests/colour_modes_test.c source/common/colour_modes.c \
  -o build/colour_modes_test
./build/colour_modes_test
```

预期：退出码为 0；该步骤证明主题直接复用现有色表，无需新增配置。

- [ ] **步骤 3：给 `Button` 增加明确的视觉和禁用状态**

增加 `transparent`、`disabled`、`text_colour`、`selected_colour` 字段。`handle_buttons()` 的方向键、触摸和 A 键路径跳过 `disabled`；绘制路径允许透明普通态，并为选中态绘制独立底色与文字色。保留旧数组的默认零值行为，避免本任务一次性改变联机等尚未迁移页面。

- [ ] **步骤 4：实现集中主题函数**

按“主题角色”章节实现 `MenuTheme` 与 `menu_theme()`；禁止把模式 1 的红色写死到主菜单、ROM 或存档函数中。

- [ ] **步骤 5：运行静态检查并提交**

```bash
git diff --check
./build/colour_modes_test
rg -n 'MenuTheme|menu_theme|transparent|disabled|selected_colour' source/3ds/gui_hard.c
git add source/3ds/gui_hard.c tests/colour_modes_test.c
git commit -m "feat: add colour-aware menu theme roles"
```

### 任务 2：重排左侧主菜单并移除文字返回/确认按钮

**文件：**
- 修改：`source/3ds/gui_hard.c:203-323, 579-627, 690-937, 2711-2743, 2802-2845, 2918-3056`

- [ ] **步骤 1：记录现有可见返回与确认入口**

```bash
rg -n '\.str ?= ?"(返回|取消|是|否|返回游戏|返回菜单)"' source/3ds/gui_hard.c
```

预期：命中 ROM、联机、控制、选项、存档、关于及确认页的文字按钮。

- [ ] **步骤 2：定义统一菜单结果，不再依赖可见返回按钮**

```c
enum {
    BUTTON_NONE = -1,
    BUTTON_BACK = -2,
    BUTTON_CONFIRM = -3,
};
```

B 在任何未锁定页面返回 `BUTTON_BACK`；纯确认提示的 A 返回 `BUTTON_CONFIRM`。业务按钮仍由选中项 + A 返回其数组索引。

- [ ] **步骤 3：把主菜单改成左侧八项单列**

合并 `first_menu_buttons` 与 `game_menu_buttons` 为同一份导航定义，顺序严格使用本计划的八项顺序；运行时只切换前三项的 `hidden`。导航按钮使用固定 `x/w/h` 和动态紧凑 `y`，未选中透明、选中有主题底色。删除旧的顶部横条和底部四角布局。

- [ ] **步骤 4：保留直接命令语义**

`继续游戏`直接设置 `guiop=0` 返回；`重新开始`直接执行当前 `MAIN_MENU_RESET` 的清屏、flush 和 `AKILL | VBRESET`，删除 `areyousure_reset` 调用；`退出`提示页只画问题文本并用 A/B 处理，不画“是/否”。

- [ ] **步骤 5：清除所有页面的文字返回/确认按钮**

从按钮数组删除显示为 `返回`、`取消`、`是`、`否`、`返回游戏`、`返回菜单`的项，逐一把其 switch 分支改为处理 `BUTTON_BACK/BUTTON_CONFIRM`。`关于`、错误信息、联机确认和设置页均由 B 返回；保留“保存”“加载”“删除”“开始”“退出”等真实业务命令文字。

- [ ] **步骤 6：静态验收并提交**

```bash
git diff --check
! rg -n '\.str ?= ?"(返回|取消|是|否|返回游戏|返回菜单)"' source/3ds/gui_hard.c
rg -n 'BUTTON_BACK|BUTTON_CONFIRM|继续游戏|重新开始|游戏进度|加载 ROM|联机|选项|关于|退出' source/3ds/gui_hard.c
git add source/3ds/gui_hard.c
git commit -m "feat: add single-column bottom-screen navigation"
```

### 任务 3：把 ROM 浏览器裁入右侧内容区

**文件：**
- 修改：`source/3ds/gui_hard.c:251-257, 951-1243`

- [ ] **步骤 1：抽出主菜单框架绘制函数**

实现 `draw_main_menu_shell(int selected_item)`，每帧先画黑底、左侧导航选中态与右侧主题面板；ROM 浏览器只负责在 `x=120..312, y=8..232` 内绘制路径、条目和滚动条。

- [ ] **步骤 2：按右侧宽度重算 ROM 列表**

保留现有 `dirCount/fileCount/cursor/scroll_pos` 数据流，把条目起点改为右侧内容区内边距，把可视高度按 224 像素计算；文件名使用 Citro2D 的宽度测量后截断或降低到既有 `0.5` 比例，不能越过 `x=304`。

- [ ] **步骤 3：改用 A/B/X 既有按键语义**

A 对当前条目执行“进入目录/加载文件”，X 进入上级目录，B 返回主菜单并重新选中 `加载 ROM`。删除 `rom_loader_buttons` 的“上级/返回”可见按钮，但保留触摸点选与拖动滚动。

- [ ] **步骤 4：验证 ROM 行为没有退化**

```bash
rg -n 'strcasecmp.*\.vb|strcasecmp.*\.zip|KEY_X|BUTTON_BACK|LAST_ROM|ROM_PATH' source/3ds/gui_hard.c
! rg -n 'rom_loader_buttons.*(上级|返回)|\.str="上级"' source/3ds/gui_hard.c
git diff --check
git add source/3ds/gui_hard.c
git commit -m "feat: move ROM browser into menu content pane"
```

### 任务 4：重做游戏进度列表与存档时间

**文件：**
- 修改：`include/vb_gui.h:31-34`
- 修改：`source/common/vb_gui.c:91-140`
- 修改：`source/3ds/gui_hard.c:587-607, 659, 2722-2800`

- [ ] **步骤 1：新增只读存档元数据接口**

在 `vb_gui.h` 声明并在 `vb_gui.c` 实现：

```c
bool emulation_state_mtime(int state, time_t *mtime);
```

函数复用 `get_savestate_path(state, false)`，`stat()` 成功时写入 `st_mtime`，所有返回路径都释放路径内存；`emulation_hasstate()` 调用该接口，避免重复路径和 `stat()` 逻辑。

- [ ] **步骤 2：将存档页改成右侧槽位列表**

绘制 10 个槽位的可滚动列表，每行显示两位槽号和本地时间：

```c
struct tm local;
localtime_r(&mtime, &local);
strftime(label, sizeof(label), "%m-%d %H:%M", &local);
```

空槽显示 `NN  --`。方向键上下和触摸选择槽位；选中项使用主题高亮。

- [ ] **步骤 3：只保留保存和加载命令**

右侧底部放 `保存`、`加载`两个命令。保存成功后留在当前页、刷新该槽时间并显示一行短状态；加载只在槽存在时可选，成功后设置 `guiop=0` 直接回游戏。删除删除槽位、L/R 翻槽和 `savestate_confirm_buttons` 二级菜单。

- [ ] **步骤 4：处理失败但不增加确认页**

保存/加载失败时在内容区状态行显示现有错误文案并留在当前页；B 始终回到左侧 `游戏进度`。状态文字不得遮挡底部命令按钮。

- [ ] **步骤 5：静态验收并提交**

```bash
git diff --check
rg -n 'emulation_state_mtime|localtime_r|strftime|%m-%d %H:%M' include/vb_gui.h source/common/vb_gui.c source/3ds/gui_hard.c
! rg -n 'savestate_confirm_buttons|PREV_SAVESTATE|NEXT_SAVESTATE|DELETE_SAVESTATE' source/3ds/gui_hard.c
git add include/vb_gui.h source/common/vb_gui.c source/3ds/gui_hard.c
git commit -m "feat: redesign save-state progress pane"
```

### 任务 5：将选项收敛为三项实时全局设置

**文件：**
- 修改：`source/3ds/gui_hard.c:469-519, 2435-2625, 3138-3245`
- 修改：`include/vb_set.h:130-153`
- 修改：`source/common/vb_set.c:442-515, 518-642`
- 修改：`source/3ds/main.c:60-75`

- [ ] **步骤 1：记录单游戏设置调用链**

```bash
rg -n 'GAME_SETTINGS|MODIFIED|loadGameOptions|saveGameOptions|deleteGameOptions' source include
```

预期：命中 ROM 加载、ROM 浏览退出、选项保存/放弃分支及公共配置实现。

- [ ] **步骤 2：把选项页改成三行循环选择**

删除现有 `options_buttons` 网格、`video_settings_buttons`、`barrier_settings_buttons` 的可达入口，在右侧内容区定义三行 `show_option` 按钮。进入页面时分别同步 `MULTIID`、`ANAGLYPH`、`SLIDERMODE`；A/触摸后循环值并留在本页。

- [ ] **步骤 3：每次切换立即应用并全局落盘**

色彩模式规范到 `0..2` 并立刻使 `menu_theme()` 读取新值；3D 模式调用 `toggleAnaglyph()`，不能只改字段；滑块模式切换现有 `SLIDER_3DS/SLIDER_VB` 值。每个分支最后调用 `saveFileOptions()`，失败时在右侧状态行显示“保存设置失败”，但当前运行时值保持已切换状态。

- [ ] **步骤 4：停止单游戏配置覆盖**

从 ROM 成功加载路径删除 `loadGameOptions()`，删除 `GAME_SETTINGS/MODIFIED`、`loadGameOptions()`、`saveGameOptions()`、`deleteGameOptions()` 及其 UI 分支。不要删除 SD 卡上的旧 `configs/*.ini`；启动和退出继续使用 `loadFileOptions()/saveFileOptions()`。

- [ ] **步骤 5：保留非三项参数的既有值但移除菜单入口**

`writeOptionsFile()` 继续往返声音、控制映射、性能等已有字段，避免升级时重置用户配置；本次只撤下其下屏入口，不删除运行时字段或输入逻辑。这样“选项只支持三种”不会扩大成无关的模拟器功能删除。

- [ ] **步骤 6：静态验收并提交**

```bash
git diff --check
! rg -n 'GAME_SETTINGS|MODIFIED|loadGameOptions|saveGameOptions|deleteGameOptions' source include
rg -n '色彩模式|3D 模式|滑块模式|saveFileOptions|toggleAnaglyph' source/3ds/gui_hard.c
git add source/3ds/gui_hard.c include/vb_set.h source/common/vb_set.c source/3ds/main.c
git commit -m "feat: make three menu options global and immediate"
```

### 任务 6：接入联机与关于页并完成全界面视觉清理

**文件：**
- 修改：`source/3ds/gui_hard.c:233-323, 802-937, 1309-1924, 2369-2433, 2802-2811, 3140-3245`

- [ ] **步骤 1：接入主菜单顺序**

确认 `联机`位于`加载 ROM`与`选项`之间，`关于`位于`选项`与`退出`之间。联机业务继续复用现有 host/join/ready/error 函数，不改变协议、ROM CRC 或输入缓冲行为。

- [ ] **步骤 2：删除联机页面的文字返回项**

把创建、加入、等待、准备和错误页的取消/返回语义统一接到 B；“创建主机”“加入房间”“刷新”“开始”等业务命令继续由 A 执行。需要确认版本不一致或离开房间时只显示提示文本，A 确认、B 取消。

- [ ] **步骤 3：更新关于页**

将 `text_about` 末尾增加：

```text
配色模式与 UI 改版：ddd
```

删除 `about_buttons` 的底部返回按钮；关于内容在右侧内容区居中排版，B 回到左侧并保持 `关于`选中。

- [ ] **步骤 4：检查文本边界和主题覆盖**

逐页检查 320x240 下的八项导航、最长 ROM 文件名、10 个存档槽时间、三项设置值、联机错误和关于署名。所有主题相关矩形/文字应使用 `MenuTheme` 或现有明确业务色，不允许主菜单路径继续读取固定 `tVBOpt.TINT` 红色。

- [ ] **步骤 5：静态验收并提交**

```bash
git diff --check
sed -n '/static Button main_menu_buttons\[\]/,/^};/p' source/3ds/gui_hard.c
rg -n '配色模式与 UI 改版：ddd' source/3ds/gui_hard.c
! rg -n '\.str ?= ?"(返回|取消|是|否|确认|返回游戏|返回菜单)"' source/3ds/gui_hard.c
git add source/3ds/gui_hard.c
git commit -m "feat: finish themed menu content panes"
```

### 任务 7：构建与 3DS 实机验收

**文件：**
- 验证：`source/3ds/gui_hard.c`
- 验证：`source/common/vb_gui.c`
- 验证：`source/common/vb_set.c`
- 验证：`tests/colour_modes_test.c`
- 验证：`Makefile`

- [ ] **步骤 1：运行宿主机测试与静态检查**

```bash
set -eu
git diff --check
cc -std=c11 -Wall -Wextra -Werror -Iinclude \
  tests/colour_modes_test.c source/common/colour_modes.c \
  -o build/colour_modes_test
./build/colour_modes_test
! rg -n 'GAME_SETTINGS|MODIFIED|loadGameOptions|saveGameOptions|deleteGameOptions' source include
! rg -n '\.str ?= ?"(返回|取消|是|否|确认|返回游戏|返回菜单)"' source/3ds/gui_hard.c
```

预期：全部命令退出码为 0。

- [ ] **步骤 2：运行 3DS Release 构建**

```bash
make clean
make release-3ds -j2
```

预期：生成 `red-viper.cci`，编译器无数组越界、未使用静态函数或格式化时间相关警告。

- [ ] **步骤 3：核验最终产物**

```bash
test -s red-viper.cci
file red-viper.cci
shasum -a 256 red-viper.cci
```

预期：CCI 文件非空，记录最终 SHA-256。

- [ ] **步骤 4：在 3DS 上验收导航和即时设置**

1. 未载入 ROM 时左侧只显示加载 ROM、联机、选项、关于、退出；载入后出现继续、重新开始、游戏进度。
2. 继续和重新开始都不经过二级菜单；重新开始确实清空旧帧并重启游戏。
3. ROM 浏览器在右侧显示路径和列表，A 打开/加载、X 上级、B 返回，触摸拖动仍可滚动。
4. 10 个存档槽正确区分空槽和有时间的槽；保存原地刷新，空槽不能加载，有效槽加载后直接回游戏。
5. 三项选项点击即切换；色彩模式切换后游戏配色和下屏全部主题色在下一帧同步改变。
6. 重启应用并换 ROM 后三项值保持不变；旧单游戏 INI 不再覆盖任何选项。
7. 所有页面 B 返回，确认提示 A 确认/B 取消，没有文字“返回/确认/是/否”按钮。
8. 最长中文菜单项、ROM 名称、时间、联机提示和关于署名无重叠或越界。

- [ ] **步骤 5：提交验收修正**

若实机发现尺寸或对比度问题，只调整本计划定义的布局常量和 `MenuTheme` 角色，不在各页面写独立颜色。

```bash
git add source/3ds/gui_hard.c source/common/vb_gui.c source/common/vb_set.c \
  include/vb_gui.h include/vb_set.h tests/colour_modes_test.c
git commit -m "fix: polish bottom-screen menu on hardware"
```

---

## 完成标准

- 下屏默认是左侧单列主菜单，顺序和显隐规则与本计划一致。
- 选中项与未选中项的底色、文字色明确不同，并随三种色彩模式同步。
- ROM 与游戏进度 UI 位于右侧内容区，存档显示真实文件时间。
- 选项只有色彩模式、3D 模式、滑块模式，点击实时切换并写入全局配置。
- 继续和重新开始没有二级菜单；关于页没有底部返回按钮并包含 ddd 署名。
- 所有页面使用 A 确认、B 返回，不显示文字返回/确认按钮。
- 宿主机色表测试、静态检查、3DS Release 构建和实机交互验收均完成。
