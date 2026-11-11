#!/usr/bin/env bash
# Build QuantiloomQt natively on Apple Silicon: Ninja, Release, Metal viewport.
# Produces build-mac/QuantiloomQt.app with the SDK's dylibs in Contents/Frameworks
# and its share/quantiloom in Contents/Resources.
#
# Override the Qt install from the environment, e.g.
#   QT_PREFIX=~/Qt/6.12.0/macos ./build_macos.sh
# DEPLOY_QT=1 additionally runs macdeployqt on the bundle (Qt is otherwise
# found through the executable's rpath, which is enough to run it here).
set -euo pipefail
cd "$(dirname "$0")"

QT_PREFIX="${QT_PREFIX:-$HOME/Qt/6.12.0/macos}"
BUILD_DIR=build-mac

# --- Stale build directory guard -------------------------------------------
# CMakeCache.txt records the absolute source path, and QUANTILOOM_SDK_ROOT /
# Quantiloom_DIR are cached alongside it. After the tree is moved the old cache
# keeps pointing at the previous location -- and at the previous Quantiloom-SDK
# -- so drop any tree whose cache no longer matches this checkout.
drop_stale_build_dir() {
    local dir="$1" cached
    [ -f "$dir/CMakeCache.txt" ] || return 0
    cached="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$dir/CMakeCache.txt" | head -n 1 || true)"
    [ -n "$cached" ] || return 0
    if [ "$cached" != "$PWD" ]; then
        echo "Stale build cache in $dir/ ($cached != $PWD) -- removing." >&2
        rm -rf "$dir"
    fi
}

drop_stale_build_dir "$BUILD_DIR"

if [ ! -f "$QT_PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ]; then
    echo "Qt not found at $QT_PREFIX (set QT_PREFIX)." >&2
    exit 1
fi

# --- Configure -------------------------------------------------------------

cmake -S . -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX"

# --- Build -----------------------------------------------------------------

cmake --build "$BUILD_DIR" -j

if [ "${DEPLOY_QT:-0}" = "1" ]; then
    "$QT_PREFIX/bin/macdeployqt" "$BUILD_DIR/QuantiloomQt.app"
fi

echo "Build OK: $BUILD_DIR/QuantiloomQt.app"
