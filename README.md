# GeomTool (Qt/C++)

基于 Qt6 和 C++ 的 3D 几何图形绘制工具。从 [geomTool](https://github.com/kaige/geomTool)（React + Three.js 版本）完整移植而来。

## 功能特性

- 🎨 **多种几何图形** — 球体、立方体、圆柱体、圆锥体、圆环体（线框渲染）
- 📐 **2D 图形** — 线段、圆弧、矩形、圆形、三角形、多边形
- 🖱️ **交互编辑** — 选择、移动、旋转（Alt=X轴 / Shift=Y轴 / Ctrl=Z轴）
- 🔗 **端点拖拽** — 线段端点、圆弧端点、圆心半径调节
- 📷 **3D 相机** — 正交投影、Ctrl+拖拽旋转视角、滚轮缩放、平移
- 🧲 **捕捉功能** — 自动捕捉端点、中点、圆心
- 🌐 **中英双语** — 实时切换界面语言
- 📋 **图形管理** — 侧边栏列表、可见性切换、删除

## 截图

> 应用启动后界面：左侧图形列表、中央 3D 画布、顶部工具栏、底部状态栏。

## 技术栈

- **语言**: C++17
- **框架**: Qt 6 (Widgets)
- **构建**: CMake 3.16+
- **渲染**: OpenGL 3.3 Core（桌面）/ GLES 3.0（WebAssembly）可编程管线（QOpenGLWidget + GLSL shader），几何经 CPU 正交投影后由 GPU 光栅化；少量屏幕空间叠加（选择手柄、捕捉标记、坐标轴 gizmo）用 QPainter 合成，跨平台（Windows / Linux / macOS / WebAssembly）

## 项目结构

```
geomTool-qt/
├── CMakeLists.txt          # CMake 构建配置
├── src/
│   ├── main.cpp            # 程序入口
│   ├── MainWindow.*        # 主窗口
│   ├── CanvasWidget.*      # 3D 画布（QOpenGLWidget，渲染 + 交互）
│   ├── CanvasRenderer.*    # OpenGL 线段渲染器（GLSL shader + VBO/VAO）
│   ├── GeometryStore.*     # 状态管理（对应 MobX store）
│   ├── GeometryFactory.h   # 几何体线框生成
│   ├── Camera.h            # 正交相机 + 轨道控制
│   ├── Math3D.h            # Vec3 / Mat4 数学库
│   ├── SnapManager.h       # 捕捉管理器
│   ├── I18n.h              # 中英国际化
│   ├── ToolbarWidget.*     # 工具栏（创建/管理 Tab）
│   ├── ShapeListWidget.*   # 图形列表侧边栏
│   ├── StatusBarWidget.*   # 状态栏
│   ├── Types.h             # 类型定义
│   └── tools/              # 交互工具
│       ├── ToolManager.*   # 工具调度
│       ├── BaseTool.h      # 工具基类
│       ├── SelectTool.*    # 选择工具
│       ├── MoveShapeTool.* # 移动工具
│       ├── RotateShapeTool.* # 旋转工具
│       ├── CreateShape3DTool.* # 3D 创建工具
│       ├── LineSegmentTool.*  # 线段绘制
│       ├── CircularArcTool.*  # 圆弧绘制
│       ├── MoveLineEndpointTool.* # 线段端点拖拽
│       ├── MoveArcEndpointTool.*  # 圆弧端点拖拽
│       └── MoveArcTool.*   # 圆弧半径调节
└── build.sh                # macOS/Linux 构建脚本
```

---

## 构建指南

### macOS

```bash
# 安装依赖
brew install qtbase cmake

# 构建
cd geomTool-qt
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(sysctl -n hw.ncpu)

# 运行
open geomTool.app      # 或直接运行:
./geomTool.app/Contents/MacOS/geomTool
```

### Linux

```bash
# Ubuntu/Debian
sudo apt install qt6-base-dev cmake build-essential

# Fedora
sudo dnf install qt6-qtbase-devel cmake gcc-c++

# 构建
cd geomTool-qt
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# 运行
./geomTool
```

---

### Windows 构建指南

#### 方式一：Qt Creator（推荐，最简单）

1. **安装 Qt 6**
   - 下载并运行 [Qt 在线安装器](https://www.qt.io/download-open-source)
   - 登录 Qt 账号（免费注册）
   - 在组件选择页面，勾选：
     - `Qt 6.x.x → MinGW xx.x.x 64-bit`（或 MSVC）
     - `Developer and Designer Tools → Qt Creator`（默认已勾）
     - `Developer and Designer Tools → MinGW xx.x.x 64-bit`（如果用 MinGW）
     - `Developer and Designer Tools → CMake`

2. **安装编译器**
   - **MinGW 方式**（推荐）：随 Qt 安装器一起装好，无需额外操作
   - **MSVC 方式**：安装 [Visual Studio 2022](https://visualstudio.microsoft.com/)（Community 免费版即可），在安装器中勾选「使用 C++ 的桌面开发」工作负载

3. **打开项目**
   ```text
   启动 Qt Creator → File → Open File or Project
   → 选择 geomTool-qt 文件夹下的 CMakeLists.txt
   ```

4. **配置 Kit**
   - Qt Creator 会自动检测已安装的 Qt 版本和编译器
   - 选择一个 Kit（如 `Desktop Qt 6.x.x MinGW 64-bit`）
   - 点击 `Configure Project`

5. **构建和运行**
   - 点击左下角的 **▶ 运行** 按钮（或 `Ctrl+R`）
   - 或按 `Ctrl+B` 仅构建

#### 方式二：命令行（CMake + MinGW）

1. **安装依赖**
   - 安装 [Qt 6](https://www.qt.io/download-open-source)（选择 MinGW 版本）
   - 安装 [CMake](https://cmake.org/download/)（安装时勾选「Add to PATH」）
   - 确保 MinGW 已安装（随 Qt 安装器安装，或单独安装 [MSYS2](https://www.msys2.org/)）

2. **设置环境变量**

   打开「系统环境变量」设置（Win+R → `sysdm.cpl` → 高级 → 环境变量），在 `Path` 中添加：

   ```text
   C:\Qt\6.x.x\mingw_xx_x64\bin
   C:\Qt\Tools\mingw_xx_x64\bin
   C:\Program Files\CMake\bin
   ```

   > 将 `6.x.x` 和 `mingw_xx_x64` 替换为你实际安装的版本号路径。

3. **打开终端并构建**

   打开 **CMD** 或 **PowerShell**：

   ```cmd
   cd C:\path\to\geomTool-qt

   mkdir build
   cd build

   :: 配置（MinGW Makefiles）
   cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/mingw_xx_x64"

   :: 构建
   cmake --build . -j%NUMBER_OF_PROCESSORS%
   ```

   > 将 `C:/Qt/6.x.x/mingw_xx_x64` 替换为你的 Qt 安装路径。

4. **运行**

   ```cmd
   geomTool.exe
   ```

   > ⚠️ 如果运行时提示找不到 `Qt6Widgets.dll` 等，请确保 Qt 的 `bin` 目录在系统 `PATH` 中，
   > 或者将所需的 Qt DLL 文件复制到 `.exe` 同目录下。也可以使用 Qt 自带的
   > `windeployqt` 工具自动打包依赖：
   > ```cmd
   > windeployqt geomTool.exe
   > ```

#### 方式三：命令行（CMake + MSVC）

1. **安装依赖**
   - 安装 [Qt 6](https://www.qt.io/download-open-source)（选择 MSVC 版本）
   - 安装 [Visual Studio 2022](https://visualstudio.microsoft.com/)，勾选「使用 C++ 的桌面开发」
   - 安装 [CMake](https://cmake.org/download/)

2. **打开 x64 Native Tools Command Prompt**

   从开始菜单搜索 → `x64 Native Tools Command Prompt for VS 2022`

3. **构建**

   ```cmd
   cd C:\path\to\geomTool-qt

   mkdir build
   cd build

   :: 配置（Visual Studio 生成器）
   cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"

   :: 构建
   cmake --build . --config Release
   ```

4. **运行**

   ```cmd
   Release\geomTool.exe
   ```

---

## WebAssembly (WASM) 构建指南

geomTool-qt 可以编译为 WebAssembly，在浏览器中直接运行。

### 环境概览

WASM 编译需要三套工具链：

| 组件 | 说明 |
|------|------|
| **Emscripten SDK** | C++ → WASM 编译器（emcc/em++） |
| **Qt6 WASM 版** | 用 Emscripten 交叉编译的 Qt6 库 |
| **Qt6 Desktop 版** | 同版本的桌面 Qt（提供 moc/rcc 等主机工具） |

### macOS（Apple Silicon / Intel）

#### 1. 安装 Emscripten SDK

```bash
cd ~
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ~/emsdk/emsdk_env.sh
```

#### 2. 安装 Qt6 Desktop 版（主机工具）

Qt6 WASM 构建需要同版本的桌面 Qt 提供 `moc`、`rcc` 等工具。推荐使用 `aqtinstall`：

```bash
python3 -m venv ~/wasm-venv
source ~/wasm-venv/bin/activate
pip install aqtinstall

# 安装 Qt 6.8.3 桌面版（与 WASM 版本号必须一致）
aqt install-qt mac desktop 6.8.3 clang_64 -O ~/qt-host
```

> 也可以用 Qt 在线安装器安装同版本的桌面 Qt，效果相同。

#### 3. 从源码编译 Qt6 WASM 版

```bash
# 下载 Qt 6.8.3 源码
mkdir ~/qt-wasm-src && cd ~/qt-wasm-src
curl -LO https://download.qt.io/official_releases/qt/6.8/6.8.3/submodules/qtbase-everywhere-src-6.8.3.tar.xz
curl -LO https://download.qt.io/official_releases/qt/6.8/6.8.3/submodules/qtsvg-everywhere-src-6.8.3.tar.xz
tar xf qtbase-everywhere-src-6.8.3.tar.xz
tar xf qtsvg-everywhere-src-6.8.3.tar.xz

# 编译 qtbase（约 5~10 分钟）
source ~/emsdk/emsdk_env.sh
brew install ninja  # 确保 Ninja 已安装

mkdir build-wasm && cd build-wasm
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \
  -DCMAKE_INSTALL_PREFIX=$HOME/qt-wasm \
  -DQT_HOST_PATH=$HOME/qt-host/6.8.3/macos \
  -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF \
  -DFEATURE_gui=ON -DFEATURE_widgets=ON \
  -DFEATURE_opengl=ON -DFEATURE_opengles2=ON \
  -DFEATURE_opengl_desktop=OFF -DFEATURE_vulkan=OFF \
  -DFEATURE_thread=ON \
  ../qtbase-everywhere-src-6.8.3
cmake --build . --parallel $(sysctl -n hw.ncpu)
cmake --install .

# 编译 qtsvg（约 1 分钟）
cd ~/qt-wasm-src
mkdir build-svg && cd build-svg
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \
  -DCMAKE_PREFIX_PATH=$HOME/qt-wasm \
  -DCMAKE_INSTALL_PREFIX=$HOME/qt-wasm \
  -DQT_HOST_PATH=$HOME/qt-host/6.8.3/macos \
  ../qtsvg-everywhere-src-6.8.3
cmake --build . --parallel $(sysctl -n hw.ncpu)
cmake --install .
```

> **已知问题**：Qt 6.8.3 的 `qwasmtheme.cpp` 缺少 `#include <QPalette>`，编译时需手动在文件开头添加该行。

#### 4. 编译 geomTool-qt WASM 版

```bash
cd ~/Projects/geomTool-qt

# 使用提供的构建脚本（自动检测 emsdk 和 Qt 路径）
bash build-wasm.sh

# 或手动构建：
source ~/emsdk/emsdk_env.sh
mkdir build-wasm && cd build-wasm
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \
  -DCMAKE_PREFIX_PATH=$HOME/qt-wasm \
  -DQT_HOST_PATH=$HOME/qt-host/6.8.3/macos \
  -DWASM_BUILD=ON \
  ..
cmake --build . --parallel $(sysctl -n hw.ncpu)
```

#### 5. 在浏览器中运行

```bash
cd build-wasm
python3 -m http.server 8080
# 打开浏览器访问 http://localhost:8080/geomTool.html
```

> ⚠️ 必须通过 HTTP 服务器访问，不能直接用 `file://` 打开（浏览器安全限制）。

### Windows

#### 1. 安装 Emscripten SDK

```cmd
cd %USERPROFILE%
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
emsdk install latest
emsdk activate latest
:: 每次打开新终端时运行：
emsdk_env.bat
```

#### 2. 安装 Qt6 Desktop 版

使用 [Qt 在线安装器](https://www.qt.io/download-open-source) 安装 Qt 6.8.3（MinGW 或 MSVC 均可），记下安装路径如 `C:\Qt\6.8.3\mingw_64`。

#### 3. 编译 Qt6 WASM 版

```cmd
:: 下载源码（同 macOS 步骤）
:: 使用 Git Bash 或 MSYS2 环境
cd %USERPROFILE%
mkdir qt-wasm-src && cd qt-wasm-src
curl -LO https://download.qt.io/official_releases/qt/6.8/6.8.3/submodules/qtbase-everywhere-src-6.8.3.tar.xz
curl -LO https://download.qt.io/official_releases/qt/6.8/6.8.3/submodules/qtsvg-everywhere-src-6.8.3.tar.xz
tar xf qtbase-everywhere-src-6.8.3.tar.xz
tar xf qtsvg-everywhere-src-6.8.3.tar.xz

:: 配置（使用 emsdk 的 toolchain，Qt host 指向桌面 Qt 安装路径）
call emsdk_env.bat
mkdir build-wasm && cd build-wasm
cmake -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=%EMSDK%\upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake ^
  -DCMAKE_INSTALL_PREFIX=%USERPROFILE%\qt-wasm ^
  -DQT_HOST_PATH=C:\Qt\6.8.3\mingw_64 ^
  -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF ^
  -DFEATURE_gui=ON -DFEATURE_widgets=ON ^
  -DFEATURE_opengl=ON -DFEATURE_opengles2=ON ^
  -DFEATURE_opengl_desktop=OFF -DFEATURE_vulkan=OFF ^
  -DFEATURE_thread=ON ^
  ..\qtbase-everywhere-src-6.8.3
cmake --build . --parallel %NUMBER_OF_PROCESSORS%
cmake --install .
```

#### 4. 编译 geomTool-qt WASM 版

```cmd
cd C:\path\to\geomTool-qt
mkdir build-wasm && cd build-wasm
call emsdk_env.bat
cmake -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=%EMSDK%\upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake ^
  -DCMAKE_PREFIX_PATH=%USERPROFILE%\qt-wasm ^
  -DQT_HOST_PATH=C:\Qt\6.8.3\mingw_64 ^
  -DWASM_BUILD=ON ^
  ..
cmake --build . --parallel %NUMBER_OF_PROCESSORS%
```

#### 5. 运行

```cmd
cd build-wasm
python -m http.server 8080
:: 浏览器访问 http://localhost:8080/geomTool.html
```

### WASM 输出文件

| 文件 | 说明 |
|------|------|
| `geomTool.wasm` | 编译后的 WebAssembly 二进制（~1.5 MB） |
| `geomTool.js`   | JavaScript 胶水代码 + Qt 运行时（~3.7 MB） |
| `geomTool.html` | HTML 加载页面 |

### WASM 平台差异

- **OpenGL**：桌面版使用 OpenGL 3.3 Core Profile；WASM 版使用 GLES 3.0（WebGL 2），GLSL 着色器自动切换为 GLSL ES 3.00
- **线程**：WASM 版启用 pthreads（Qt WASM 多线程模式）
- **内存**：启用 `ALLOW_MEMORY_GROWTH`（动态内存增长）

---

## 常见问题

### CMake 找不到 Qt6

确保 `CMAKE_PREFIX_PATH` 指向正确的 Qt 安装路径。例如：

```bash
cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/mingw_64"
```

### 运行时缺少 Qt DLL（Windows）

使用 `windeployqt` 自动部署：

```cmd
cd build
windeployqt geomTool.exe
```

### 中文显示乱码

确保源文件以 UTF-8 编码保存。MSVC 用户可能需要在 `CMakeLists.txt` 中添加：

```cmake
if(MSVC)
    target_compile_options(geomTool PRIVATE /utf-8)
endif()
```

## 许可证

MIT License
