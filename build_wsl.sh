#!/usr/bin/env bash
# Build QuantiloomQt from WSL2 using the Windows MSVC toolchain via WSL interop.
# Produces Windows binaries (EXE/DLLs), identical to running
# build_windows_msvc.ps1 on the Windows side.
#
# set -e guarantees later steps are never reached if any build step fails.
# We call cmake.exe (the Windows CMake), not the Linux cmake, so the Visual
# Studio generator and the Windows Qt/Vulkan/SDK paths resolve correctly.
set -euo pipefail
cd "$(dirname "$0")"

# Qt install used for find_package(Qt6). Override from the environment, e.g.
#   QT_PREFIX='C:\Qt\6.10.1\msvc2022_64' ./build_wsl.sh
QT_PREFIX="${QT_PREFIX:-C:\\Qt\\6.10.1\\msvc2022_64}"

# --- Stale build directory guard -------------------------------------------
# CMakeCache.txt records the absolute source path, and QUANTILOOM_SDK_ROOT /
# Quantiloom_DIR are cached alongside it. After the tree is moved to another
# drive or folder the old cache keeps pointing at the previous location -- and
# at the previous Quantiloom-SDK -- so configure either fails oddly or, worse,
# silently links the SDK from the old drive. Drop any tree whose cache no
# longer matches this checkout.

repo_win="$(wslpath -w "$PWD" 2>/dev/null || printf '%s' "$PWD")"
repo_win="${repo_win//\\//}"
repo_win="${repo_win%/}"

drop_stale_build_dir() {
    local dir="$1" cached
    [ -f "$dir/CMakeCache.txt" ] || return 0
    cached="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$dir/CMakeCache.txt" | head -n 1 || true)"
    cached="${cached//\\//}"
    cached="${cached%/}"
    [ -n "$cached" ] || return 0
    if [ "${cached,,}" != "${repo_win,,}" ]; then
        echo "Stale build cache in $dir/ ($cached != $repo_win) -- removing." >&2
        rm -rf "$dir"
    fi
}

drop_stale_build_dir build-msvc
drop_stale_build_dir build-cdb

# --- Configure -------------------------------------------------------------

cmake.exe -B build-msvc -G "Visual Studio 18 2026" -A x64 \
    -DCMAKE_PREFIX_PATH="${QT_PREFIX}"

# --- Build -----------------------------------------------------------------

cmake.exe --build build-msvc --config Release -j

echo "Build OK: build-msvc\\Release\\QuantiloomQt.exe"
