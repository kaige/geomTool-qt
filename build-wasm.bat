@echo off
setlocal enabledelayedexpansion
rem ===========================================================================
rem  build-wasm.bat - Build geomTool-qt for WebAssembly (WASM) on Windows
rem
rem  Prerequisites (installed under %USERPROFILE%):
rem    - Emscripten SDK 3.1.56        : %USERPROFILE%\emsdk
rem    - Qt 6.8.3 WASM (target)       : %USERPROFILE%\qt-wasm\6.8.3\wasm_singlethread
rem    - Qt 6.8.3 desktop (host tools): %USERPROFILE%\qt-wasm-host\6.8.3\mingw_64
rem
rem  Usage:  build-wasm.bat [clean|help]
rem ===========================================================================

rem ---- Always run from this script's directory (the project root) ----
cd /d "%~dp0"
set "PROJECT_DIR=%CD%"
set "BUILD_DIR=%PROJECT_DIR%\build-wasm"

rem ---- Toolchain locations (override via env if needed) ----
if not defined EMSDK set "EMSDK=%USERPROFILE%\emsdk"
set "EMSDK_ENV=%EMSDK%\emsdk_env.bat"
set "EM_TOOLCHAIN=%EMSDK%\upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake"
if not defined QT_WASM_DIR set "QT_WASM_DIR=%USERPROFILE%\qt-wasm\6.8.3\wasm_singlethread"
if not defined QT_HOST_PATH set "QT_HOST_PATH=%USERPROFILE%\qt-wasm-host\6.8.3\mingw_64"

rem ---- Parse arguments ----
set "ACTION=build"
for %%a in (%*) do (
    if /i "%%~a"=="clean"  set "ACTION=clean"
    if /i "%%~a"=="help"   set "ACTION=help"
    if /i "%%~a"=="-h"     set "ACTION=help"
    if /i "%%~a"=="/?"     set "ACTION=help"
)

if "%ACTION%"=="help" goto :usage

if "%ACTION%"=="clean" (
    echo === Cleaning %BUILD_DIR% ===
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    if exist "%BUILD_DIR%" (echo [ERROR] Could not remove build dir. & exit /b 1) else (echo Clean done.)
    exit /b 0
)

rem ---- Activate Emscripten (puts emcc + bundled python/node on PATH) ----
if not exist "%EMSDK_ENV%" (
    echo [ERROR] Emscripten not found at: %EMSDK_ENV%
    echo         Install 3.1.56:  cd %EMSDK% ^& emsdk install 3.1.56 ^& emsdk activate 3.1.56
    exit /b 1
)
call "%EMSDK_ENV%"

rem ---- Verify toolchain pieces ----
where emcc >nul 2>nul || (echo [ERROR] emcc not on PATH after activating emsdk. & exit /b 1)
where cmake >nul 2>nul || (echo [ERROR] cmake not on PATH. & exit /b 1)
where ninja >nul 2>nul || (echo [ERROR] ninja not on PATH. & exit /b 1)
if not exist "%EM_TOOLCHAIN%" (echo [ERROR] Emscripten toolchain file missing: %EM_TOOLCHAIN% & exit /b 1)
if not exist "%QT_WASM_DIR%\lib\cmake\Qt6\Qt6Config.cmake" (
    echo [ERROR] Qt6 WASM not found at: %QT_WASM_DIR%
    echo         Install: aqt install-qt all_os wasm 6.8.3 wasm_singlethread -O %%USERPROFILE%%\qt-wasm
    exit /b 1
)
if not exist "%QT_HOST_PATH%\lib\cmake\Qt6\Qt6Config.cmake" (
    echo [ERROR] Qt6 host tools not found at: %QT_HOST_PATH%
    echo         Install: aqt install-qt windows desktop 6.8.3 win64_mingw -O %%USERPROFILE%%\qt-wasm-host
    exit /b 1
)

echo === Building geomTool [WASM / Release] ===
echo   emsdk:    %EMSDK%
echo   Qt WASM:  %QT_WASM_DIR%
echo   Qt host:  %QT_HOST_PATH%

rem ---- Configure (idempotent; safe to run every time) ----
cmake -G Ninja -B "%BUILD_DIR%" -S . ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_TOOLCHAIN_FILE="%EM_TOOLCHAIN%" ^
    -DCMAKE_PREFIX_PATH="%QT_WASM_DIR%" ^
    -DCMAKE_FIND_ROOT_PATH="%QT_WASM_DIR%" ^
    -DQT_HOST_PATH="%QT_HOST_PATH%" ^
    -DQT_DIR="%QT_WASM_DIR%\lib\cmake\Qt6" ^
    -DWASM_BUILD=ON
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    exit /b 1
)

rem ---- Build ----
cmake --build "%BUILD_DIR%" --parallel %NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo.
echo === WASM Build OK ===
echo Output: %BUILD_DIR%\geomTool.js  ^(+ .wasm, .html^)
echo.
echo To run locally (must use HTTP, not file://):
echo   cd "%BUILD_DIR%" ^&^& python -m http.server 8080
echo   then open http://localhost:8080/geomTool.html
exit /b 0

:usage
echo Usage: build-wasm.bat [clean^|help]
echo.
echo   build-wasm.bat        Configure + build WASM (Release)
echo   build-wasm.bat clean  Remove the build-wasm directory
echo   build-wasm.bat help   Show this help
echo.
echo Toolchain (override via env: EMSDK, QT_WASM_DIR, QT_HOST_PATH):
echo   Emscripten: %EMSDK%
echo   Qt WASM:    %QT_WASM_DIR%
echo   Qt host:    %QT_HOST_PATH%
exit /b 0
