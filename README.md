# imgmgr

`imgmgr` 是一个基于 Qt 6 Widgets、C++17 和 SQLite 的桌面图片资源管理工具，用于整理游戏解包后的 PNG、JPG、WEBP、BMP 等图片素材。

应用默认把资源目录视为只读目标：扫描过程不会修改、删除、重命名资源目录中的文件，也不会在资源目录内新增文件。项目数据库和缩略图缓存应放在资源目录之外。

## 主要功能

- 打开或新建 SQLite 项目数据库。
- 递归扫描图片资源目录，读取文件名、相对路径、尺寸、大小、格式、Alpha 信息等元数据。
- 使用 `QTableView` 和 `QAbstractTableModel` 显示图片列表，不为每张图片创建独立控件。
- 异步生成缩略图，并缓存到项目目录旁的 `.project_cache/thumbnails`。
- 支持完整图片预览、拖动、缩放、适应窗口。
- 支持 R/G/B/A 通道显示切换。
- 支持通配符和正则表达式筛选。
- 支持区分大小写、全字匹配、匹配目标选择。
- 支持规则树、新增顶层规则、新增子规则、编辑、删除、启用或禁用规则。
- 支持在编辑规则时修改父规则。
- 支持将规则导出为 JSON，以及从 JSON 导入规则并覆盖当前规则。
- 支持规则命中重算、冲突检测、未分类/已分类/冲突/多重命中状态筛选。
- 支持命中解释页，显示规则路径和冲突原因。
- 支持图片列表和图片预览背景切换：棋盘格、系统、黑色、白色、灰色。
- 菜单栏“查看”可控制图片列表列显示。

## 目录结构

```text
imgmgr/
  CMakeLists.txt
  README.md
  .env.example
  .vscode/
    c_cpp_properties.json
    launch.json
    settings.json
    tasks.json
  scripts/
    build.sh
  src/
    database/      SQLite 初始化、图片仓库、规则仓库
    models/        图片表格模型、规则树模型
    services/      扫描、缩略图缓存、规则匹配引擎
    widgets/       筛选面板、图片预览、规则面板
    utils/         哈希和图片辅助函数
```

## 构建环境

需要安装：

- Qt 6，包含 Widgets、Sql、Concurrent 模块。
- Qt SQLite 驱动插件。
- CMake。
- Bash 环境，例如 Git Bash 或 MSYS2 Bash。
- 与 Qt 套件匹配的 MinGW 工具链。

构建路径通过 `.env` 或环境变量提供。

## 配置 .env

复制模板：

```bash
cp .env.example .env
```

编辑 `.env`，填写 Qt 和 MinGW 路径：

```dotenv
QT_ROOT=/path/to/Qt/6.x.x/<compiler>
MINGW_BIN=/path/to/Qt/Tools/<mingw>/bin
BUILD_DIR=build
GENERATOR="MinGW Makefiles"
JOBS=8
```

`.env` 是本地构建配置，已加入 `.gitignore`，不应提交。

## 一键构建

在项目根目录运行：

```bash
./scripts/build.sh
```

或：

```bash
bash scripts/build.sh
```

脚本会执行：

1. 读取 `.env` 或当前环境变量。
2. 配置 CMake。
3. 使用指定 MinGW 编译。
4. 调用 `windeployqt` 复制运行所需的 Qt DLL 和插件。

构建完成后的程序默认位于：

```text
build/imgmgr.exe
```

也可以不使用 `.env`，直接传入环境变量：

```bash
QT_ROOT=/path/to/Qt/6.x.x/<compiler> \
MINGW_BIN=/path/to/Qt/Tools/<mingw>/bin \
JOBS=12 \
bash scripts/build.sh
```

如果构建目录中缓存了不同的编译器，脚本会自动删除该构建目录并重新配置。

## VS Code 构建与调试

仓库包含基础 VS Code 配置：

- `.vscode/tasks.json`：提供默认构建任务 `build imgmgr`。
- `.vscode/launch.json`：提供“运行和调试”配置 `运行 imgmgr（构建后启动）`。
- `.vscode/settings.json` 和 `.vscode/c_cpp_properties.json`：让 C/C++ 扩展读取 `build/compile_commands.json`，用于头文件定位、语法高亮和错误提示。

首次使用前，请先配置 `.env`。之后可以使用以下方式构建：

- 按 `Ctrl+Shift+B`。
- 或菜单：`Terminal -> Run Build Task... -> build imgmgr`。

运行和调试：

1. 打开 VS Code 左侧“运行和调试”。
2. 选择 `运行 imgmgr（构建后启动）`。
3. 按 `F5` 或点击绿色运行按钮。

`F5` 会先执行 `build imgmgr` 任务，再启动：

```text
build/imgmgr.exe
```

C/C++ 扩展依赖 `build/compile_commands.json` 获取完整编译参数。该文件由 `scripts/build.sh` 自动生成；如果 VS Code 仍显示旧的头文件错误，可以执行 `C/C++: Reset IntelliSense Database`，或重载窗口。

## 手动构建

Bash 示例：

```bash
export QT_ROOT=/path/to/Qt/6.x.x/<compiler>
export MINGW_BIN=/path/to/Qt/Tools/<mingw>/bin
export PATH="$MINGW_BIN:$QT_ROOT/bin:$PATH"

cmake -S . -B build -G "MinGW Makefiles" \
  -DCMAKE_PREFIX_PATH="$QT_ROOT" \
  -DCMAKE_C_COMPILER="$MINGW_BIN/gcc.exe" \
  -DCMAKE_CXX_COMPILER="$MINGW_BIN/g++.exe" \
  -DCMAKE_MAKE_PROGRAM="$MINGW_BIN/mingw32-make.exe"

cmake --build build -j 8

cd build
windeployqt imgmgr.exe
```

`windeployqt` 可能提示找不到 `dxcompiler.dll` 和 `dxil.dll`。当前应用是 Qt Widgets 程序，不使用 Direct3D 12 特性时该警告通常不影响运行。

## 使用流程

1. 启动 `imgmgr.exe`。
2. 通过“项目 -> 新建项目数据库”创建 `.db` 文件，或通过“项目 -> 打开项目数据库”打开已有 `.db` 文件。
3. 确保项目数据库不在待扫描资源目录内。
4. 通过“项目 -> 选择资源目录并扫描”递归扫描图片。
5. 在左侧列表查看缩略图、文件名、尺寸、状态。
6. 在左侧筛选区输入通配符或正则表达式，按回车或点击“筛选”更新结果。
7. 点击“新增顶层规则”或选中规则后点击“新增子规则”保存规则。
8. 在规则树中编辑、删除、启用或禁用规则。
9. 通过“项目 -> 导出规则为 JSON”备份当前规则。
10. 通过“项目 -> 从 JSON 导入规则并覆盖”还原规则。
11. 通过“项目 -> 重算规则命中”刷新命中状态和冲突检测。
12. 在右侧图片预览页查看完整图片和通道效果。

## 规则备份与还原

规则导出文件为 JSON 格式，使用嵌套的 `children` 数组保存规则树结构。每条规则包含规则 ID、规则名称、规则类型、规则内容、匹配目标、启用状态、优先级、允许冲突、大小写设置、全字匹配设置和备注。

导入 JSON 时会覆盖当前所有规则，并清空已有规则命中记录。导入前程序会递归读取 `children`，校验规则字段、重复 ID 和规则树结构；导入成功后会刷新规则树并重新计算规则命中。

## 规则状态说明

- 未分类：图片没有命中任何规则。
- 已分类：图片命中规则，且不存在冲突，也不存在跨规则链的多重命中。
- 冲突：图片命中冲突规则，或命中了某个子规则但没有命中其启用祖先规则。
- 多重命中：图片不存在规则冲突，但命中的规则中至少存在一对不是祖先/后代关系的规则。

这四种状态在列表显示和筛选中互斥。

## 安全约束

- 扫描资源目录时不会写入资源目录。
- 缩略图缓存写入项目数据库所在目录旁的 `.project_cache/thumbnails`。
- 如果项目目录等于资源目录，或位于资源目录内部，程序会拒绝扫描，避免在资源目录中生成数据库或缓存文件。

## 当前限制

以下功能尚未完整实现或仍可继续增强：

- 排除规则。
- 人工标签和确认流程。
- 批量右键操作。
- CSV 导出。
- 重复图片和相似图片检测。
- 自动生成规则建议。
- 动画帧识别。
- 图片差异对比和统计图表。
