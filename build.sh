#!/bin/bash
# Build script for geomTool-qt
set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

# Find Qt6
QT_CMAKE_DIR=""
for p in /opt/homebrew/lib/cmake /usr/local/lib/cmake /opt/homebrew/opt/qt/lib/cmake; do
    if [ -d "$p/Qt6" ]; then
        QT_CMAKE_DIR="$p"
        break
    fi
done

# Also check via brew
if [ -z "$QT_CMAKE_DIR" ]; then
    BREW_QT=$(/opt/homebrew/bin/brew --prefix qt 2>/dev/null || echo "")
    if [ -n "$BREW_QT" ] && [ -d "$BREW_QT/lib/cmake/Qt6" ]; then
        QT_CMAKE_DIR="$BREW_QT/lib/cmake"
    fi
fi

if [ -z "$QT_CMAKE_DIR" ]; then
    echo "ERROR: Qt6 not found!"
    exit 1
fi

echo "Qt6 found at: $QT_CMAKE_DIR"

# Find cmake
CMAKE=""
for p in /opt/homebrew/bin/cmake /usr/local/bin/cmake cmake; do
    if command -v "$p" &>/dev/null; then
        CMAKE="$p"
        break
    fi
done

if [ -z "$CMAKE" ]; then
    echo "ERROR: cmake not found!"
    exit 1
fi

echo "cmake: $CMAKE"

# Configure
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
"$CMAKE" "$PROJECT_DIR" -DCMAKE_PREFIX_PATH="$QT_CMAKE_DIR/.." -DCMAKE_BUILD_TYPE=Release 2>&1

# Build
"$CMAKE" --build . -j$(sysctl -n hw.ncpu) 2>&1

echo ""
echo "Build complete! Binary at: $BUILD_DIR/geomTool"
