#!/bin/bash
# =====================================================================
# build-wasm.sh — Build geomTool-qt for WebAssembly (WASM)
#
# Prerequisites:
#   1. Emscripten SDK installed and activated
#   2. Qt6 WASM build installed (or installed via Qt online installer)
#
# This script auto-detects emsdk and Qt WASM paths.
# =====================================================================
set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build-wasm"

# --- Locate emsdk ---
EMSDK_DIR="${EMSDK_DIR:-$HOME/emsdk}"
if [ ! -f "$EMSDK_DIR/emsdk_env.sh" ]; then
    echo "ERROR: emsdk not found at $EMSDK_DIR"
    echo "  Install it: git clone https://github.com/emscripten-core/emsdk.git && cd emsdk && ./emsdk install latest && ./emsdk activate latest"
    exit 1
fi
source "$EMSDK_DIR/emsdk_env.sh"

# emsdk 6.x requires Python 3.10+; use bundled python if available
if [ -z "$EMSDK_PYTHON" ]; then
    for py in "$EMSDK_DIR"/python/*/bin/python3; do
        if [ -x "$py" ]; then
            export EMSDK_PYTHON="$py"
            break
        fi
    done
fi

# --- Locate Qt6 WASM (static build preferred) ---
QT_WASM_DIR="${QT_WASM_DIR:-$HOME/qt-wasm-static}"
if [ ! -d "$QT_WASM_DIR/lib/cmake/Qt6" ]; then
    # Fallback to shared build
    QT_WASM_DIR="$HOME/qt-wasm"
    if [ ! -d "$QT_WASM_DIR/lib/cmake/Qt6" ]; then
        echo "ERROR: Qt6 WASM build not found"
        echo "  Static: $HOME/qt-wasm-static"
        echo "  Shared: $HOME/qt-wasm"
        exit 1
    fi
fi

# --- Locate Qt host tools (same version, desktop build) ---
QT_HOST_PATH="${QT_HOST_PATH:-$HOME/qt-host/6.8.3/macos}"
if [ ! -d "$QT_HOST_PATH/lib/cmake/Qt6" ]; then
    echo "ERROR: Qt6 host tools not found at $QT_HOST_PATH"
    echo "  Install the matching desktop Qt via: aqt install-qt mac desktop 6.8.3 clang_64 -O ~/qt-host"
    exit 1
fi

EM_TOOLCHAIN_FILE="$(dirname "$(dirname "$(which emcc)")")/cmake/Modules/Platform/Emscripten.cmake"
if [ ! -f "$EM_TOOLCHAIN_FILE" ]; then
    # Fallback: look in emsdk upstream directly
    EM_TOOLCHAIN_FILE="$EMSDK_DIR/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"
fi

echo "=== WASM Build Configuration ==="
echo "  emsdk:        $EMSDK_DIR"
echo "  Qt WASM:      $QT_WASM_DIR"
echo "  Qt host:      $QT_HOST_PATH"
echo "  Toolchain:    $EM_TOOLCHAIN_FILE"
echo ""

# --- Configure ---
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$PROJECT_DIR" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$EM_TOOLCHAIN_FILE" \
    -DCMAKE_PREFIX_PATH="$QT_WASM_DIR" \
    -DCMAKE_FIND_ROOT_PATH="$QT_WASM_DIR" \
    -DQT_HOST_PATH="$QT_HOST_PATH" \
    -DQT_DIR="$QT_WASM_DIR/lib/cmake/Qt6" \
    -DWASM_BUILD=ON

# --- Build ---
cmake --build . --parallel "$(sysctl -n hw.ncpu 2>/dev/null || nproc)"

echo ""
echo "WASM build complete!"
echo "  Output: $BUILD_DIR/geomTool.js  (+ .wasm, .html)"
echo ""
echo "To serve locally:"
echo "  cd $BUILD_DIR && python3 -m http.server 8080"
echo "  Then open http://localhost:8080/geomTool.html in a browser."
