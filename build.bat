@echo off
setlocal enabledelayedexpansion
rem ===========================================================================
rem  build.bat - Build geomTool-qt on Windows using the MSYS2 MinGW64 toolchain
rem  Usage:  build.bat [clean|run|help] [Release|Debug|RelWithDebInfo|MinSizeRel]
rem ===========================================================================

rem ---- Always run from this script's directory (the project root) ----
cd /d "%~dp0"
set "PROJECT_DIR=%CD%"
set "BUILD_DIR=%PROJECT_DIR%\build"

rem ---- MSYS2 toolchain location (override with the MSYS2 env var if needed) ----
if not defined MSYS2 set "MSYS2=C:\msys64"
set "MINGW_BIN=%MSYS2%\mingw64\bin"

rem ---- Parse arguments ----
set "ACTION=build"
set "BUILD_TYPE=Release"
set "DO_RUN=0"
for %%a in (%*) do (
    if /i "%%~a"=="clean"        set "ACTION=clean"
    if /i "%%~a"=="help"         set "ACTION=help"
    if /i "%%~a"=="-h"           set "ACTION=help"
    if /i "%%~a"=="/?"           set "ACTION=help"
    if /i "%%~a"=="run"          set "DO_RUN=1"
    if /i "%%~a"=="Debug"        set "BUILD_TYPE=Debug"
    if /i "%%~a"=="Release"      set "BUILD_TYPE=Release"
    if /i "%%~a"=="RelWithDebInfo" set "BUILD_TYPE=RelWithDebInfo"
    if /i "%%~a"=="MinSizeRel"   set "BUILD_TYPE=MinSizeRel"
)

if "%ACTION%"=="help" goto :usage

if "%ACTION%"=="clean" (
    echo === Cleaning %BUILD_DIR% ===
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    if exist "%BUILD_DIR%" (echo [ERROR] Could not remove build dir. & exit /b 1) else (echo Clean done.)
    exit /b 0
)

rem ---- Verify the toolchain exists ----
if not exist "%MINGW_BIN%\cmake.exe" (
    echo [ERROR] MSYS2 toolchain not found at: %MINGW_BIN%\cmake.exe
    echo         Install MSYS2, or set the MSYS2 env var to point at your MSYS2 root.
    exit /b 1
)

rem ---- Make the toolchain available for this script only (setlocal scopes it) ----
set "PATH=%MINGW_BIN%;%PATH%"
set "CMAKE_PREFIX_PATH=%MSYS2%\mingw64"

echo === Building geomTool [%BUILD_TYPE%] using %MINGW_BIN% ===

rem ---- Configure (idempotent; safe to run every time) ----
cmake -G Ninja -B "%BUILD_DIR%" -S . -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    exit /b 1
)

rem ---- Build (Ninja parallelizes automatically) ----
cmake --build "%BUILD_DIR%"
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo.
echo === Build OK ===
echo Output: %BUILD_DIR%\geomTool.exe

if "%DO_RUN%"=="1" (
    echo === Running geomTool ===
    "%BUILD_DIR%\geomTool.exe"
)
exit /b 0

:usage
echo Usage: build.bat [clean^|run^|help] [Release^|Debug^|RelWithDebInfo^|MinSizeRel]
echo.
echo   build.bat              Configure + build (Release)
echo   build.bat Debug        Build with Debug configuration
echo   build.bat run          Build (Release) and run geomTool.exe
echo   build.bat run Debug    Build Debug and run
echo   build.bat clean        Remove the build directory
echo   build.bat help         Show this help
echo.
echo Toolchain: %MINGW_BIN%  (override by setting the MSYS2 env var)
exit /b 0
