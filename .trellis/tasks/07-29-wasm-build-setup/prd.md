# 设置 WebAssembly 开发环境并编译运行 WASM 版本

## Goal

在本机（Windows 11）建立可复现的 WebAssembly 开发环境，把 geomTool-qt 编译成 WASM，并在浏览器中运行验证。

## Background

- 项目已有 WASM 基础设施（`CMakeLists.txt` 的 `WASM_BUILD` 分支、`build-wasm.sh`、README 的 WASM 指南），但都是 **macOS 风格**（`sysctl`、`$HOME/emsdk`），项目历史 WASM 验证基于 **Qt 6.8.3**。
- 本机现状：Qt 6.10.1 桌面版（mingw_64）已装；**emsdk 未装、Qt-WASM 库未装**；Python/Ninja/Node 齐全；`aqtinstall` 已装。
- 用户选定方案：**aqt 预编译版** Qt-WASM（非源码编译、非 MaintenanceTool）。

## Requirements

### 功能需求
1. 安装 Emscripten SDK，版本 **3.1.56**（Qt 6.8 官方要求；用 latest/4.x 会导致 WASM 编译失败）。
2. 用 `aqtinstall` 下载官方预编译的 **Qt 6.8.3 WASM** 库（`wasm_singlethread` 架构，含 Svg 模块）。
3. 用 `aqtinstall` 下载与 WASM 版本号匹配的 **Qt 6.8.3 桌面版**（提供 moc/rcc/uic 等主机工具，作 `QT_HOST_PATH`）。
4. 提供一个 **Windows 原生** 的 WASM 构建脚本（对标现有 `build.bat`），完成 cmake 配置 + 构建。
5. 用本地 HTTP 服务器在浏览器中加载运行 `geomTool.html`。

### 约束
- 平台：Windows 11，Git Bash / cmd 均可执行。
- 工具链版本必须自洽：emsdk 3.1.56 ↔ Qt 6.8.3（host 与 target 版本号一致，避免跨版本 moc/CMake 风险）。
- 安装目录统一放在用户主目录下，便于复现：`C:\Users\kzhan\emsdk`、`C:\Users\kzhan\qt-wasm`、`C:\Users\kzhan\qt-wasm-host`。
- 必须通过 HTTP 访问（浏览器对 `file://` 的 WASM 有安全限制）。
- 不改动现有桌面构建（`build.bat` / `CMakeLists.txt` 桌面分支）行为；仅新增 Windows WASM 脚本，复用已有 `WASM_BUILD` CMake 分支。

## Acceptance Criteria

- [ ] `emcc --version` 输出包含 `3.1.56`。
- [ ] `C:\Users\kzhan\qt-wasm\6.8.3\wasm_singlethread\lib\cmake\Qt6\Qt6Config.cmake` 存在。
- [ ] `C:\Users\kzhan\qt-wasm-host\6.8.3\mingw_64\lib\cmake\Qt6\Qt6Config.cmake` 存在。
- [ ] 运行 WASM 构建脚本后在 `build-wasm\` 生成 `geomTool.js`、`geomTool.wasm`、`geomTool.html`。
- [ ] 本地 HTTP 服务器（`python -m http.server`）能成功返回 `geomTool.html`（HTTP 200）。
- [ ] （最终视觉验证）浏览器打开 `http://localhost:8080/geomTool.html`，geomTool 界面正常渲染（由用户确认，或头无浏览器截图佐证）。

## Out of Scope

- 源码编译 Qt-WASM（README 备选方案，本任务用预编译版替代）。
- 通过 Qt MaintenanceTool 安装（需账号 GUI，本任务跳过）。
- WASM 多线程（`wasm_multithread`）变体——本期只用 singlethread，降低 SharedArrayBuffer/COEP 配置复杂度。
- 桌面版任何改动。
