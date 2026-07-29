# Implement — WASM 环境搭建与构建（Windows）

执行顺序自上而下；每步含验证命令。回滚点：步骤间彼此独立，失败可重跑单步。

## 阶段 A：安装工具链

### A1. 安装 Emscripten 3.1.56
```bash
# Git Bash 中
cd ~
[ -d emsdk ] || git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install 3.1.56        # 下载约 ~1GB，耗时几分钟；后台运行
./emsdk activate 3.1.56
source ~/emsdk/emsdk_env.sh
```
- 验证：`emcc --version` → 含 `3.1.56`
- 回滚：`rm -rf ~/emsdk`
- ⚠️ 长任务：用后台执行 + 长超时

### A2. 安装 Qt 6.8.3 WASM（target）
```bash
python -m aqt install-qt all_os wasm 6.8.3 wasm_singlethread -O ~/qt-wasm
```
- 验证：`ls ~/qt-wasm/6.8.3/wasm_singlethread/lib/cmake/Qt6/Qt6Config.cmake`
- 回滚：`rm -rf ~/qt-wasm`

### A3. 安装 Qt 6.8.3 桌面（host 工具）
```bash
python -m aqt install-qt windows desktop 6.8.3 win64_mingw -O ~/qt-wasm-host
```
- 验证：`ls ~/qt-wasm-host/6.8.3/mingw_64/lib/cmake/Qt6/Qt6Config.cmake`
- 回滚：`rm -rf ~/qt-wasm-host`
- 备注：备选方案——若想省下载，可先试 `QT_HOST_PATH` 指向已装 `C:\Qt\6.10.1\mingw_64`；若 cmake 报版本不匹配再回退到本步。

## 阶段 B：构建脚本与编译

### B1. 新增 `build-wasm.bat`（项目根）
- 内容见 design.md §3；对标 `build.bat` 风格；支持 `clean|help`。
- 验证：`cmd //c "build-wasm.bat help"` 打印用法且退出码 0

### B2. 配置 + 构建
```bash
# 激活 emsdk 后（cmd: emsdk_env.bat；bash: source ~/emsdk/emsdk_env.sh）
cmd //c "build-wasm.bat"
```
- 验证：`ls build-wasm/geomTool.{js,wasm,html}` 三文件均存在
- 常见错误对照 design.md §5 排查；构建失败先看 `build-wasm/CMakeFiles/*.log` 与 emsdk 版本

## 阶段 C：运行验证

### C1. 启动本地 HTTP 服务器
```bash
cd build-wasm && python -m http.server 8080 &
sleep 2
curl -sI http://localhost:8080/geomTool.html   # 期望 HTTP/1.0 200 OK
ls -la geomTool.js geomTool.wasm geomTool.html  # 检查大小非 0
```
- 验证：curl 返回 200；产物文件大小合理（wasm 数 MB、js 数 MB）
- 完成后停止后台服务器

### C2. 视觉验证（人工 / 可选头无浏览器）
- 人工：浏览器开 `http://localhost:8080/geomTool.html`，确认 geomTool 界面渲染。
- 可选：`npm i -D playwright` + `npx playwright install chromium` 后脚本截图（重，默认跳过，按需启用）。

## 阶段 D：收尾

### D1. 质量检查
- 确认桌面构建未被破坏：`git status` 应只新增 `build-wasm.bat` 与 `build-wasm/`（后者在 `.gitignore` 内或应加入）。
- 把 `build-wasm/` 加入 `.gitignore`（产物不入库）。

### D2. 更新 spec / README
- README 的 Windows WASM 章节补充 aqt 预编译版路径（emsdk 3.1.56、`build-wasm.bat`）。

### D3. 提交
- 仅提交 `build-wasm.bat` 与 `.gitignore`、README 改动（不提交工具链与产物）。
