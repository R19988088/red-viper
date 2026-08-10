# 中文菜单与 3DS Release 实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans（逐任务实现此计划）。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 将 3DS 菜单和运行时提示汉化，并让 GitHub Actions 只构建可直接下载的 `red-viper.3ds` Release asset。

**架构：** 复用现有 `source/3ds/gui_hard.c` 的静态文本缓冲和 Button 数组，只替换显示字符串；按钮返回、否、向上行为改为按数组索引识别，避免显示语言影响控制逻辑。复用现有 `cia.rsf` 和 `makerom`，在 Makefile 增加 CCI 输出目标，工作流只执行 Release CCI 构建并通过 GitHub Releases API 上传裸 `.3ds` 文件。

**技术栈：** C、GNU Make、makerom 0.18.4、devkitPro `devkitarm` 容器、GitHub Actions REST API。

---

### 任务 1：增加 CCI `.3ds` Release 目标

**文件：**
- 修改：`Makefile:187-240`

- [ ] **步骤 1：编写失败的静态验收**

运行：

```bash
set -eu
rg -q 'release-3ds' Makefile
test ! -s <(rg 'upload-artifact|test_interpreter|test_dynarec' .github/workflows/build.yml)
rg -q 'red-viper\\.3ds' .github/workflows/build.yml
```

预期：失败，因为当前 Makefile 没有 `release-3ds`，工作流仍上传两个 ZIP artifact 并执行额外任务。

- [ ] **步骤 2：实现最小构建目标**

保留现有 `release` 行为，在外层增加 `release-3ds`，将 `BUILD_TARGET` 传给递归 Make；在内层增加 `3ds` 目标和 `-f cci` 的 makerom 命令，输出 `$(OUTPUT).3ds`。

- [ ] **步骤 3：运行静态验收**

运行：

```bash
rg -n 'release-3ds|BUILD_TARGET|OUTPUT\\).3ds|-f cci' Makefile
```

预期：命中 Release CCI 目标、输出文件和 makerom CCI 参数。

### 任务 2：汉化 3DS UI 并解除文案耦合

**文件：**
- 修改：`source/3ds/gui_hard.c:230-680, 3350-3460, 3580-3690`

- [ ] **步骤 1：记录原始文案耦合**

运行：

```bash
rg -n 'strcmp\\(buttons\\[i\\]\\.str|\\.str="(Back|No|Up)"' source/3ds/gui_hard.c
```

预期：确认返回、否、向上快捷键依赖英文显示字符串。

- [ ] **步骤 2：实现最小 UI 变更**

将可见菜单、开关、确认框、存档、联机和错误提示替换为简体中文；保留按键图标、ROM 名称、版本号和性能字段。将 `handle_buttons` 中 `Back/No/Up` 的比较改为当前 Button 数组索引集合。Release 选项菜单隐藏保存调试信息入口，避免 Release UI 暴露 debug 操作。

- [ ] **步骤 3：运行静态验收**

运行：

```bash
rg -n '[一-龥]' source/3ds/gui_hard.c
! rg -n 'strcmp\\(buttons\\[i\\]\\.str' source/3ds/gui_hard.c
```

预期：源码包含汉化文案，按钮行为不再读取显示字符串。

### 任务 3：将 GitHub Actions 改为裸 `.3ds` Release

**文件：**
- 修改：`.github/workflows/build.yml:1-70`

- [ ] **步骤 1：实现 Release 工作流**

使用 `devkitpro/devkitarm` 容器执行 `make release-3ds`；删除 `upload-artifact`、CIA/3DSX 输出和额外测试 job。构建成功后用 `GITHUB_TOKEN` 创建唯一预发布版本，并通过 `uploads.github.com` 上传 `red-viper.3ds`，在 `$GITHUB_STEP_SUMMARY` 写入裸文件下载 URL。保留 `workflow_dispatch` 和 master push 触发，Pull Request 只做构建不发布。

- [ ] **步骤 2：运行 YAML 与内容验收**

运行：

```bash
ruby -e 'require "yaml"; YAML.load_file(".github/workflows/build.yml"); puts "workflow yaml ok"'
! rg -n 'upload-artifact|\\.cia|\\.3dsx|DEBUGLEVEL=[1-9]|test_interpreter|test_dynarec' .github/workflows/build.yml
rg -n 'make release-3ds|red-viper\\.3ds|uploads.github.com|GITHUB_STEP_SUMMARY' .github/workflows/build.yml
```

预期：YAML 可解析，工作流只保留 Release `.3ds` 发布路径。

### 任务 4：验证、提交并触发 Action

**文件：**
- 验证：`Makefile`, `source/3ds/gui_hard.c`, `.github/workflows/build.yml`

- [ ] **步骤 1：运行本地可用的静态检查**

运行：

```bash
git diff --check
set -eu
rg -q 'release-3ds' Makefile
rg -q '[一-龥]' source/3ds/gui_hard.c
! rg -q 'upload-artifact|test_interpreter|test_dynarec' .github/workflows/build.yml
rg -q 'red-viper\\.3ds' .github/workflows/build.yml
```

预期：退出码为 0。

- [ ] **步骤 2：提交并推送**

运行：

```bash
git add Makefile source/3ds/gui_hard.c .github/workflows/build.yml docs/superpowers/plans/2026-08-10-chinese-menu-3ds-release.md
git commit -m "feat: add Chinese 3DS release build"
git push origin master
```

预期：远程 master 接收提交，GitHub Actions 生成 Release 并提供 `red-viper.3ds` 直接下载链接。

- [ ] **步骤 3：核验远端 Action 产物**

运行：

```bash
gh run list --workflow build.yml --limit 3
gh release list --limit 3
```

预期：最新运行成功，最新预发布版本资产名为 `red-viper.3ds`，没有 ZIP artifact、CIA 或 3DSX 发布项。
