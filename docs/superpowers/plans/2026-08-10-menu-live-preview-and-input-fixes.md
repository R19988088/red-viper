# 下屏菜单实时预览、四色状态与输入修复实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 让左侧导航移动时立即显示真实右栏内容，A 才把焦点交给右栏；统一四色状态，修复相邻菜单项被跳过、ROM 目录未刷新及存档列表卡顿。

**架构：** 保留现有 `main_menu()` 单入口，但把右栏拆成“数据准备、纯绘制、进入交互”三步。ROM 列表和存档列表各使用一份可失效缓存，左侧焦点变化只触发一次准备，逐帧绘制不再访问文件系统或重复解析文本；主菜单方向、A、触摸和 B 仍走现有按键语义。

**技术栈：** C11、Citro2D/Citro3D、libctru 文件系统、现有 `Button`/`VB_OPT`/`colour_mode_value()`、GNU Make、GitHub Actions devkitARM。

---

## 已确认问题与验收口径

1. `menu_theme()` 虽然读取四个色阶，但右侧面板、按钮和部分旧路径仍混用 `TINT_COLOR`、亮度派生色和固定色；修复后菜单区域只允许出现当前模式的四个原始色值。
2. `draw_main_menu_preview()` 在每帧对 10 个存档槽调用 `emulation_state_mtime()`、`localtime()`、`C2D_TextBufClear()` 和 `C2D_TextParse()`，左侧焦点经过“游戏进度”就会产生文件系统和文本重建开销。
3. `handle_buttons()` 用 `>=`/`<=` 排除方向候选，两个相邻按钮边缘相等时被错误跳过。当前 28 像素连续行会从索引 0 跳到 2，因此“重新开始、加载 ROM、选项”三个奇数索引无法用上下键到达。
4. 当前右栏预览只给“加载 ROM”绘制路径，没有复用 `rom_loader_impl()` 扫描出的目录条目；视觉上必须按 A 后才看到文件。
5. `rom_loader_impl(refresh=true)` 只在进入独立 ROM 循环时扫描。左侧切到“加载 ROM”没有刷新模型，目录内容变化不会反映在主菜单右栏。

### 四色角色

所有颜色都必须直接来自 `colour_mode_value(mode, 0..3)`，禁止 `COLOR_BRIGHTNESS()`、固定 RGB、额外混色或透明度派生：

| 色阶 | 角色 | 使用位置 |
| --- | --- | --- |
| `shade 0` | 基础背景 | 整体画布、左栏底色、空白区域 |
| `shade 1` | 禁用/次级底色 | 禁用文字、右栏普通按钮底色、非激活弱提示 |
| `shade 2` | 待选/卡片色 | 可用但未激活的左栏文字、右侧卡片、列表普通项 |
| `shade 3` | 激活色 | 当前焦点文字、当前列表行文字、选项值 |

选中行或按钮的底色使用 `shade 1`，文字使用 `shade 3`；右侧卡片使用 `shade 2`。如果卡片上的普通正文需要与卡片区分，则使用 `shade 1`，不生成第五种颜色。

## 文件结构

- 修改：`include/colour_modes.h`，为四个固定色阶增加语义枚举，避免散落数字索引。
- 修改：`tests/colour_modes_test.c`，锁定四色语义和三种模式的原始色值。
- 创建：`include/menu_navigation.h`，声明可在宿主机测试的相邻按钮方向判定。
- 创建：`source/common/menu_navigation.c`，实现包含边缘相等情况的方向判定。
- 创建：`tests/menu_navigation_test.c`，覆盖连续菜单行和禁用项跳过规则。
- 修改：`source/3ds/gui_hard.c`，接入四色角色、右栏内容模型、ROM 刷新、存档缓存和主菜单输入修复。
- 验证：`.github/workflows/build.yml`，沿用现有 Action 构建并发布 CIA。

---

### 任务 1：锁定四色语义并清除派生菜单色

**文件：**
- 修改：`include/colour_modes.h:4-7`
- 修改：`tests/colour_modes_test.c`
- 修改：`source/3ds/gui_hard.c:147-181, 732-842, 2878-2960, 3180-3244`

- [ ] **步骤 1：先给色阶增加语义断言**

在 `include/colour_modes.h` 增加：

```c
enum ColourShadeRole {
    COLOUR_SHADE_BACKGROUND = 0,
    COLOUR_SHADE_DISABLED = 1,
    COLOUR_SHADE_READY = 2,
    COLOUR_SHADE_ACTIVE = 3,
};
```

在 `tests/colour_modes_test.c` 断言四个角色索引分别为 `0/1/2/3`，并保留 12 个现有色值断言。

- [ ] **步骤 2：运行宿主机颜色测试**

```bash
cc -std=c11 -Wall -Wextra -Werror -Iinclude \
  tests/colour_modes_test.c source/common/colour_modes.c \
  -o build/colour_modes_test
./build/colour_modes_test
```

预期：退出码 0；三种模式均只返回表内四个原始颜色。

- [ ] **步骤 3：收敛 `MenuTheme` 字段**

将 `MenuTheme` 改成直接表达四个角色和两个复用底色：

```c
typedef struct {
    u32 background;
    u32 disabled;
    u32 ready;
    u32 active;
    u32 panel;
    u32 selection_bg;
} MenuTheme;
```

`background/disabled/ready/active` 分别读取语义枚举；`panel=ready`，`selection_bg=disabled`。左栏、右侧卡片、存档列表、选项和关于页只使用这些字段。

- [ ] **步骤 4：扫描并清除菜单范围的派生色**

```bash
rg -n 'COLOR_BRIGHTNESS|TINT_COLOR|TINT_[0-9]+|C2D_Color32\(' source/3ds/gui_hard.c
```

只修改下屏主菜单及其右栏页面；游戏渲染、旧控制映射和性能条不在本任务范围。右栏相关函数中只允许 `menu_theme()` 和 `colour_mode_value()` 供色。

- [ ] **步骤 5：提交四色修复**

```bash
git diff --check
git add include/colour_modes.h tests/colour_modes_test.c source/3ds/gui_hard.c
git commit -m "fix: map menu states to four palette shades"
```

### 任务 2：修复连续菜单行被跳过和游戏中命令不可达

**文件：**
- 创建：`include/menu_navigation.h`
- 创建：`source/common/menu_navigation.c`
- 创建：`tests/menu_navigation_test.c`
- 修改：`source/3ds/gui_hard.c:919-983, 3068-3131`

- [ ] **步骤 1：写出失败的相邻边缘测试**

测试连续 28 像素行：当前行 `[8,36]`，下一行 `[36,64]` 必须是向下候选；上一行边缘相等时必须是向上候选。

```c
assert(menu_axis_is_candidate(8, 36, 36, 64, 1));
assert(menu_axis_is_candidate(36, 64, 8, 36, -1));
assert(!menu_axis_is_candidate(8, 36, 8, 36, 1));
```

- [ ] **步骤 2：实现严格方向、允许边缘相等**

```c
bool menu_axis_is_candidate(float selected_start, float selected_end,
                            float candidate_start, float candidate_end,
                            int direction) {
    if (direction < 0) return candidate_end <= selected_start;
    if (direction > 0) return candidate_start >= selected_end;
    return true;
}
```

`handle_buttons()` 使用该函数替换当前会排除相邻边缘的 `>=`/`<=` 条件，最近距离排序保持不变。

- [ ] **步骤 3：明确游戏菜单可用状态**

在 `game_menu()` 入口统一设置：`继续游戏、重新开始、游戏进度、加载 ROM、联机、选项、关于` 均 `hidden=false` 且 `disabled=false`，退出保持隐藏。不要依赖 `first_menu()` 留下的旧状态。

- [ ] **步骤 4：验证七项逐行可达**

```bash
cc -std=c11 -Wall -Wextra -Werror -Iinclude \
  tests/menu_navigation_test.c source/common/menu_navigation.c \
  -o build/menu_navigation_test
./build/menu_navigation_test
```

实机按一次下键必须依次经过索引 `0,1,2,3,4,5,6`；分别用 A 和触摸验证“重新开始、加载 ROM、选项”进入现有 switch 分支。

- [ ] **步骤 5：提交输入修复**

```bash
git add include/menu_navigation.h source/common/menu_navigation.c \
  tests/menu_navigation_test.c source/3ds/gui_hard.c
git commit -m "fix: keep adjacent menu rows reachable"
```

### 任务 3：建立共享 ROM 列表模型并在分类显示时刷新

**文件：**
- 修改：`source/3ds/gui_hard.c:289-297, 765-842, 1139-1490`

- [ ] **步骤 1：把目录扫描从 `rom_loader_impl()` 抽成模型刷新函数**

```c
typedef struct {
    char path[300];
    char **dirs;
    int dir_count;
    char **files;
    int file_count;
    int cursor;
    float scroll_pos;
    bool valid;
} RomBrowserModel;

static bool rom_browser_refresh(RomBrowserModel *model, const char *requested_path);
static void rom_browser_clear(RomBrowserModel *model);
```

刷新函数复用现有隐藏目录过滤、`.vb/.zip` 过滤和大小写排序；先释放旧数组，再构建新数组，成功后才设置 `valid=true`。

- [ ] **步骤 2：左侧焦点首次切到“加载 ROM”就刷新**

在主菜单保存 `previous_nav_item`。当 `selectedButton` 从其他分类变成 `MAIN_MENU_LOAD_ROM` 时调用一次 `rom_browser_refresh()`，下一帧直接绘制新列表。停留在该分类时不逐帧扫描。

- [ ] **步骤 3：每次重新打开分类再刷新一次**

从右栏按 B 回到左栏后，再按 A 进入“加载 ROM”前重新调用 `rom_browser_refresh()`；从其他分类切回“加载 ROM”也重新调用。进入子目录、返回上级目录仍在路径变化后刷新一次。

- [ ] **步骤 4：预览和交互复用同一模型**

新增 `draw_rom_browser(const RomBrowserModel *model, bool focused)`。左侧焦点停在“加载 ROM”时绘制路径和当前目录文件；A 只把焦点交给右栏，禁止再次创建另一份目录数组或跳转到空白标题页。

- [ ] **步骤 5：验证刷新语义**

实机步骤：打开菜单并停在“加载 ROM”看到文件列表；返回 HOME/使用外部方式增删一个 `.vb` 或 `.zip`；切到其他分类再切回，列表必须变化；在“加载 ROM”上 B/A 重新进入也必须变化。目录无内容时绘制空列表，不保留上次结果。

- [ ] **步骤 6：提交 ROM 模型修复**

```bash
git diff --check
git add source/3ds/gui_hard.c
git commit -m "fix: refresh shared ROM list on category open"
```

### 任务 4：缓存游戏进度元数据和文本

**文件：**
- 修改：`source/3ds/gui_hard.c:103-106, 773-793, 2878-2949, 3295-3333`

- [ ] **步骤 1：定义 10 槽缓存和专用文本缓冲区**

```c
typedef struct {
    bool valid;
    bool exists[10];
    time_t mtimes[10];
    char labels[10][32];
    C2D_Text texts[10];
} SavestateListCache;

static SavestateListCache savestate_cache;
static C2D_TextBuf savestate_textbuf;
```

在 `guiInit()` 创建一次 `savestate_textbuf`，缓存重建时清空一次并解析 10 行。

- [ ] **步骤 2：只在失效点读取文件系统**

实现 `refresh_savestate_cache()`，集中调用 10 次 `emulation_state_mtime()`。只在以下时机调用：打开菜单后首次切到“游戏进度”、进入存档交互页、保存完成、ROM 切换完成。

- [ ] **步骤 3：逐帧绘制只读缓存**

`draw_main_menu_preview(MAIN_MENU_SAVESTATES)` 和 `savestate_menu()` 只绘制 `savestate_cache.texts[slot]`。上下移动仅改变 `selected_state` 和选中矩形，不调用 `stat()`、`localtime()`、`strftime()`、`C2D_TextParse()`。

- [ ] **步骤 4：删除重复的逐帧构建路径**

```bash
sed -n '760,830p;2878,2930p' source/3ds/gui_hard.c
```

验收：两个 frame loop 内都没有 `emulation_state_mtime`、`localtime`、`strftime`、`C2D_TextBufClear` 或 `C2D_TextParse`。

- [ ] **步骤 5：实机性能验收**

在菜单中连续上下经过“游戏进度”30 次，再进入右栏连续切换 10 个槽位；不得出现可见停顿。保存后只刷新一次缓存，时间立即更新。

- [ ] **步骤 6：提交缓存修复**

```bash
git diff --check
git add source/3ds/gui_hard.c
git commit -m "perf: cache save-state menu metadata"
```

### 任务 5：统一左栏预览与右栏焦点状态

**文件：**
- 修改：`source/3ds/gui_hard.c:202-205, 765-994, 1139-1490, 2686-2723, 2878-2960`

- [ ] **步骤 1：增加显式焦点状态**

```c
typedef enum {
    MENU_FOCUS_NAV,
    MENU_FOCUS_CONTENT,
} MenuFocus;

static MenuFocus main_menu_focus;
static int main_menu_item;
```

打开菜单时固定为 `MENU_FOCUS_NAV`；左侧上下键只修改 `main_menu_item`，触发对应模型准备并实时绘制右栏，不调用 `options()`、`rom_loader_impl()`、`savestate_menu()` 或 `about()`。

- [ ] **步骤 2：让所有分类都有真实预览**

- “游戏进度”绘制缓存后的 10 槽列表。
- “加载 ROM”绘制刷新后的目录条目、路径、当前游标。
- “联机”绘制创建/加入命令；无游戏时使用禁用色。
- “选项”绘制当前色彩模式、固定 3DS 模式、滑块模式和语言。
- “关于”绘制现有 logo 与完整信息。
- “继续游戏/重新开始”保持命令预览，不创建二级菜单。

- [ ] **步骤 3：A 只负责进入或执行**

`继续游戏`和`重新开始`按 A 直接执行；其余可交互分类按 A 将 `main_menu_focus` 设为 `MENU_FOCUS_CONTENT`，并使用当前右栏模型接收输入。禁用项保留占位且 A/触摸不产生动作。

- [ ] **步骤 4：B 的行为固定**

右栏焦点按 B 回左栏并保持当前分类；左栏焦点按 B 在游戏运行时恢复游戏，无游戏时留在主菜单。不得新增文字返回按钮。

- [ ] **步骤 5：提交焦点与实时预览修复**

```bash
git diff --check
git add source/3ds/gui_hard.c
git commit -m "fix: render live panes before content focus"
```

### 任务 6：构建、发布 CIA 与实机回归

**文件：**
- 验证：`include/colour_modes.h`
- 验证：`include/menu_navigation.h`
- 验证：`source/common/menu_navigation.c`
- 验证：`source/3ds/gui_hard.c`
- 验证：`.github/workflows/build.yml`

- [ ] **步骤 1：运行本地可执行测试和静态检查**

```bash
set -eu
git diff --check
./build/colour_modes_test
./build/menu_navigation_test
```

预期：全部退出码 0。

- [ ] **步骤 2：检查菜单颜色来源和逐帧文件访问**

```bash
rg -n 'COLOUR_SHADE_(BACKGROUND|DISABLED|READY|ACTIVE)' \
  include/colour_modes.h source/3ds/gui_hard.c
sed -n '760,850p;2878,2940p' source/3ds/gui_hard.c | \
  rg 'emulation_state_mtime|localtime|strftime|C2D_TextParse' && exit 1 || true
```

预期：四色角色均被使用；右栏和存档 frame loop 不再访问文件系统或重建文本。

- [ ] **步骤 3：推送并等待 GitHub Action**

```bash
git push origin master
gh run list --limit 1 --json databaseId,status,conclusion,headSha,url
run_id=$(gh run list --limit 1 --json databaseId --jq '.[0].databaseId')
gh run watch "$run_id" --exit-status
```

预期：`Build release CIA` 与 `Publish direct CIA download` 均成功。

- [ ] **步骤 4：下载并校验 CIA**

```bash
tag=$(gh release list --limit 1 --json tagName --jq '.[0].tagName')
gh release download "$tag" --pattern red-viper.cia --dir "/tmp/red-viper-release-$tag"
file "/tmp/red-viper-release-$tag/red-viper.cia"
shasum -a 256 "/tmp/red-viper-release-$tag/red-viper.cia"
```

记录 Release 链接、文件大小和 SHA-256。

- [ ] **步骤 5：实机最终验收**

按以下顺序一次完成：无游戏占位和关于可用；左栏逐项移动时右栏立即显示真实内容；加载 ROM 每次打开自动刷新；启动游戏后重新开始、加载 ROM、选项均可用；游戏进度上下切换流畅；三种颜色模式下界面只出现对应四个色值；右栏 B 回左栏，左栏 B 继续游戏。
