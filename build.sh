#!/bin/bash

# Usage: ./build.sh [release|debug]
BUILD_TYPE="${1:-release}"

if [[ "$BUILD_TYPE" != "release" && "$BUILD_TYPE" != "debug" ]]; then
    echo "Usage: $0 [release|debug]"
    exit 1
fi

if [ "$BUILD_TYPE" == "debug" ]; then
    BUILD_DIR="cmake-build-debug"
    CMAKE_BUILD_TYPE="Debug"
else
    BUILD_DIR="cmake-build-release"
    CMAKE_BUILD_TYPE="Release"
fi

ARCHITECTURE=${ARCH_OVERRIDE:-$(uname -m)}
CMAKE_BIN=$(which cmake)

if [ -z "$CMAKE_BIN" ] || [ ! -x "$CMAKE_BIN" ]; then
    echo "Error: cmake not found or not executable." >&2
    exit 1
fi

echo "Building wavmarker in $BUILD_TYPE mode..."
echo "Using CMake: $CMAKE_BIN"
echo "Build directory: $BUILD_DIR"
echo "Architecture: $ARCHITECTURE"

"$CMAKE_BIN" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHITECTURE" \
    -S .

if [ $? -ne 0 ]; then
    echo "Error: CMake configuration failed." >&2
    exit 1
fi

"$CMAKE_BIN" --build "$BUILD_DIR" -- -j 8

if [ $? -ne 0 ]; then
    echo "Error: Build failed." >&2
    exit 1
fi

if [ ! -f "$BUILD_DIR/wavmarker" ]; then
    echo "Error: wavmarker executable not found in $BUILD_DIR." >&2
    exit 1
fi

chmod +x "$BUILD_DIR/wavmarker"

echo "Build successful: $BUILD_DIR/wavmarker"
