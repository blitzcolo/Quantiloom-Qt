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

# --- Configure -------------------------------------------------------------

cmake.exe -B build-msvc -G "Visual Studio 18 2026" -A x64 \
    -DCMAKE_PREFIX_PATH="${QT_PREFIX}"

# --- Build -----------------------------------------------------------------

cmake.exe --build build-msvc --config Release -j

echo "Build OK: build-msvc\\Release\\QuantiloomQt.exe"
