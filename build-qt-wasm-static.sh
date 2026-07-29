#!/bin/bash
# =====================================================================
# build-qt-wasm-static.sh — Build Qt6 for WASM in STATIC mode
#
# This creates a single-file WASM build (no dynamic .so loading,
# no pthreads/SharedArrayBuffer requirement).
# =====================================================================
set -e

# --- Locate emsdk ---
EMSDK_DIR="${EMSDK_DIR:-$HOME/emsdk}"
source "$EMSDK_DIR/emsdk_env.sh"

QT_SRC="${QT_SRC:-$HOME/qt-everywhere-src-6.8.3}"
QT_BUILD_DIR="${QT_BUILD_DIR:-$HOME/qt-wasm-static-build}"
QT_INSTALL_DIR="${QT_INSTALL_DIR:-$HOME/qt-wasm-static}"
QT_HOST_PATH="${QT_HOST_PATH:-$HOME/qt-host/6.8.3/macos}"

if [ ! -d "$QT_SRC" ]; then
    echo "ERROR: Qt source not found at $QT_SRC"
    exit 1
fi

EM_TOOLCHAIN_FILE="$(dirname "$(dirname "$(which emcc)")")/cmake/Modules/Platform/Emscripten.cmake"
if [ ! -f "$EM_TOOLCHAIN_FILE" ]; then
    EM_TOOLCHAIN_FILE="$EMSDK_DIR/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"
fi

echo "=== Qt Static WASM Build ==="
echo "  Source:       $QT_SRC"
echo "  Build dir:    $QT_BUILD_DIR"
echo "  Install dir:  $QT_INSTALL_DIR"
echo "  Host Qt:      $QT_HOST_PATH"
echo "  Toolchain:    $EM_TOOLCHAIN_FILE"
echo ""

mkdir -p "$QT_BUILD_DIR"
cd "$QT_BUILD_DIR"

# --- Configure Qt for static WASM (no threads) ---
cmake "$QT_SRC" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$EM_TOOLCHAIN_FILE" \
    -DCMAKE_INSTALL_PREFIX="$QT_INSTALL_DIR" \
    -DQT_HOST_PATH="$QT_HOST_PATH" \
    -DQT_FEATURE_static=ON \
    -DQT_FEATURE_shared=OFF \
    -DQT_FEATURE_thread=OFF \
    -DQT_FEATURE_wasm_simd128=ON \
    -DQT_FEATURE_wasm_exceptions=OFF \
    -DQT_FEATURE_openssl=OFF \
    -DQT_FEATURE_dbus=OFF \
    -DQT_FEATURE_gui=ON \
    -DQT_FEATURE_widgets=ON \
    -DQT_FEATURE_opengl=ON \
    -DQT_FEATURE_opengles2=ON \
    -DQT_FEATURE_svg=ON \
    -DQT_FEATURE_network=ON \
    -DQT_BUILD_EXAMPLES=OFF \
    -DQT_BUILD_TESTS=OFF \
    -DBUILD_SHARED_LIBS=OFF

echo ""
echo "=== Building Qt (this takes a while) ==="
cmake --build . --parallel "$(sysctl -n hw.ncpu 2>/dev/null || nproc)"

echo ""
echo "=== Installing Qt ==="
cmake --install .

echo ""
echo "=== Qt Static WASM build complete! ==="
echo "  Installed to: $QT_INSTALL_DIR"
