# Design — WASM 开发环境与构建（Windows / aqt 预编译版）

## 1. 关键技术决策

### 1.1 Qt 版本：6.8.3（非已装的 6.10.1）
| 选项 | 评估 |
|------|------|
| **Qt 6.8.3（选定）** | 项目历史 WASM 验证版本；emsdk 3.1.56 官方文档明确；`CMakeLists.txt`/`build-wasm.sh` 的工作区（如 `_qt_test_emscripten_version` no-op）针对它。确定性最高。 |
| Qt 6.10.1（备选） | 与已装桌面 Qt 同版本可省一次 host 下载，但项目未在 6.10.1 WASM 上验证过，且对应 emsdk 版本需另行查证。 |

决定走 6.8.3：**host 与 target 版本号严格一致**（都 6.8.3），规避跨版本 moc/CMake 兼容风险。代价是多下一次 host Qt（≈1.5GB），但换来首次成功概率。

### 1.2 emsdk：3.1.56（精确版本）
- Qt 6.8 官方要求 Emscripten **3.1.56**；用 `latest`(4.x) 会缺 GLESv2/EGL 等，编译报错。
- `emsdk install 3.1.56` → `emsdk activate 3.1.56`，emsdk 会自动下载匹配的 node/python。

### 1.3 WASM 架构：`wasm_singlethread`
- 避免 `wasm_multithread` 对 SharedArrayBuffer/COOP-COEP 的部署要求；singlethread 可直接 `http.server` 加载。

### 1.4 构建产物用 `qt_add_executable`
- 复用 `CMakeLists.txt` 已有 `WASM_BUILD` 分支：`qt_add_executable` 会自动处理平台插件预加载、HTML shell、MODULARIZE、字体加载等。无需改 CMake。

## 2. 工具链目录布局

```
C:\Users\kzhan\
├── emsdk\                                   # Emscripten 3.1.56
│   └── upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake  # toolchain
├── qt-wasm\                                 # aqt → Qt 6.8.3 WASM (target)
│   └── 6.8.3\wasm_singlethread\lib\cmake\Qt6\Qt6Config.cmake
└── qt-wasm-host\                            # aqt → Qt 6.8.3 桌面 (host 工具)
    └── 6.8.3\mingw_64\lib\cmake\Qt6\Qt6Config.cmake
```

## 3. 构建脚本设计：新增 `build-wasm.bat`

对标现有 `build.bat`（Windows 原生、参数解析、clean/help）。流程：

```
1. cd 项目根；BUILD_DIR=build-wasm
2. call %USERPROFILE%\emsdk\emsdk_env.bat      # 激活 emcc/3.1.56 + 自带 python/node
3. 校验 emcc、cmake、ninja 在 PATH
4. cmake -G Ninja -B build-wasm -S . ^
       -DCMAKE_BUILD_TYPE=Release ^
       -DCMAKE_TOOLCHAIN_FILE=%USERPROFILE%\emsdk\upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake ^
       -DCMAKE_PREFIX_PATH=%USERPROFILE%\qt-wasm\6.8.3\wasm_singlethread ^
       -DCMAKE_FIND_ROOT_PATH=%USERPROFILE%\qt-wasm\6.8.3\wasm_singlethread ^
       -DQT_HOST_PATH=%USERPROFILE%\qt-wasm-host\6.8.3\mingw_64 ^
       -DQT_DIR=%USERPROFILE%\qt-wasm\6.8.3\wasm_singlethread\lib\cmake\Qt6 ^
       -DWASM_BUILD=ON
5. cmake --build build-wasm --parallel %NUMBER_OF_PROCESSORS%
6. 打印: cd build-wasm && python -m http.server 8080 → 浏览器开 http://localhost:8080/geomTool.html
```

参数：`build-wasm.bat [clean|help]`（WASM 构建无 Debug run 需求，保持精简）。

## 4. 运行与验证

- **本地服务器**：`cd build-wasm && python -m http.server 8080`（必须 HTTP，不能 file://）。
- **自动化验证**：服务器起来后 `curl -sI http://localhost:8080/geomTool.html` 期望 `200`，并检查 `geomTool.js/.wasm` 文件存在与大小（js ≈ 数 MB、wasm ≈ 数 MB）。
- **视觉验证**：浏览器人工确认界面渲染；agent 无浏览器，必要时可用 node+Playwright 头无截图（可选、非必须）。

## 5. 已知坑与对策

| 坑 | 对策 |
|----|------|
| README 中 `qwasmtheme.cpp` 缺 `#include <QPalette>` | 仅源码编译才遇到；**预编译版已编好，N/A** |
| `CMakeLists.txt` 里 `_qt_test_emscripten_version` no-op（针对 emsdk 6.x `$CFGDIR`） | 保留，无害；emsdk 3.1.56 下不影响 |
| Windows 上 `build-wasm.sh` 不可用（`sysctl`） | 新增 `build-wasm.bat`，不动 `.sh` |
| anaconda Python 与 emsdk 自带 Python 冲突 | 脚本内 `emsdk_env.bat` 设置的 PATH 优先 emsdk 自带；构建期不依赖 anaconda |
| aqt `all_os wasm` 包不含独立 host 工具 | 单独 aqt 安装 `windows desktop 6.8.3 win64_mingw` 作 `QT_HOST_PATH` |
| emsdk 下载/编译耗时长 | 分步执行、后台运行、设超时；首次安装 3.1.56 下载约 ~1GB |

## 6. 回滚形态

- 全部新增物：`build-wasm.bat` + `build-wasm\` 产物 + 用户主目录下的工具链。
- 仓库回滚 = 删除 `build-wasm.bat` + `build-wasm\`；不影响桌面构建与源码。
- 工具链（emsdk/qt-wasm/qt-wasm-host）卸载 = 删除对应目录即可，无系统级污染。
