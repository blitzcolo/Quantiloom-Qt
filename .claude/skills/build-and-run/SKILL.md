---
name: build-and-run
description: Build the Quantiloom Studio Qt6 GUI (QuantiloomQt) from WSL2 with the Windows MSVC toolchain and run it on Windows, including the cross-repo SDK refresh flow when the Quantiloom core (Quantiloom-dev) changed. Use this whenever the user asks to build, rebuild, run, launch, or test the GUI, when a core SDK change needs to be picked up by the frontend, or when diagnosing "SDK not found" / stale-DLL / Qt configure failures.
---

# Build and Run (Quantiloom Studio)

## The one command

```bash
cd /mnt/d/Quantiloom-Qt && ./build_wsl.sh
```

Configures `build-msvc/` with `cmake.exe -G "Visual Studio 18 2026" -A x64` and builds Release. Qt location defaults to `C:\Qt\6.10.1\msvc2022_64`; override with `QT_PREFIX='C:\Qt\<ver>\msvc2022_64' ./build_wsl.sh`. Windows toolchain only — never Linux cmake (Visual Studio generator + Windows Qt/Vulkan paths).

Incremental rebuild after C++-only changes:

```bash
cmake.exe --build build-msvc --config Release -j
```

## The cross-repo dependency (read this before debugging anything)

This repo contains **no physics** — it links the prebuilt SDK at `../Quantiloom-SDK/windows_amd64` (i.e. `D:/Quantiloom-SDK/windows_amd64`, CMake cache var `QUANTILOOM_SDK_ROOT`) via `find_package(Quantiloom)`. The SDK is produced by the core repo `/mnt/d/Quantiloom-dev`. Consequences:

- **Core changed → refresh flow (in this order):**
  1. `cd /mnt/d/Quantiloom-dev && ./build_wsl.sh` — rebuilds core and reinstalls the SDK
  2. `cd /mnt/d/Quantiloom-Qt && cmake.exe --build build-msvc --config Release -j` — relink against the new `.lib`; POST_BUILD steps copy `Quantiloom.dll`, `SpectraForge.dll`, Qt DLLs, and `.spv` shaders from the SDK into `build-msvc/Release/`
  3. If the SDK's public **headers** changed shape, a plain rebuild recompiles what's needed; if `QuantiloomConfig.cmake` or install layout changed, reconfigure with the full `./build_wsl.sh`
- **Staleness is detected for you; do not go hunting timestamps.** A POST_BUILD copy runs only when QuantiloomQt relinks, so a refreshed SDK could once leave a stale `Quantiloom.dll` in `build-msvc/Release/` with nothing reporting it. Two mechanisms now cover that: the SDK's package files and binaries are on `CMAKE_CONFIGURE_DEPENDS` (`CMakeLists.txt:135-148`), so a reinstall forces a reconfigure; and CMake records the SDK library's SHA-256 into `SdkStamp.hpp`, which `src/SdkGuard.cpp` checks at startup — a binary paired with a library it was not built against logs the mismatch and exits, and a merely out-of-date build warns.
- The SDK boundary is a **C++ ABI** (SRS CON-04): both repos must use the same MSVC toolchain. Never mix an SDK built with a different VS version.
- Close `QuantiloomQt.exe` before rebuilding either repo — a running GUI locks the DLLs and fails the copy/install steps.

## Running

```bash
cd /mnt/d/Quantiloom-Qt/build-msvc/Release && ./QuantiloomQt.exe
```

It is a Windows GUI app — the window opens on the Windows desktop, not in the terminal. Everything it needs (Qt DLLs, `platforms/` plugin, SDK DLLs, shaders) is already co-located in `Release/` by the POST_BUILD copy steps. Requires an NVIDIA RTX GPU with Vulkan RT. When launched from a non-interactive session, run it in the background and check it stays alive rather than blocking on it.

## Common failures

- **`Quantiloom SDK not found at: .../windows_amd64`** at configure — the SDK doesn't exist or was half-written (core build killed mid-install). Rebuild the core repo first.
- **`find_package(Qt6)` fails** — wrong `QT_PREFIX` for the installed Qt; check `ls /mnt/c/Qt/`.
- **GUI crashes at startup / missing entry point in Quantiloom.dll** — stale SDK DLL vs headers mismatch; run the refresh flow above from step 1.
- **Vulkan errors at startup** — needs the Windows NVIDIA driver with RT extensions; check the core CLI renders first (core repo's render-verify skill) to separate SDK problems from GUI problems.

## Architecture guardrails (SRS)

When adding features here: physics and algorithm logic belong in the SDK, not this repo (CON-02 — existing violations: PCHIP interpolation, IR energy-conservation curve construction, hand-written TOML export; don't add more). The GUI talks to the core only through `QL_API` public headers and `ExternalRenderContext`.
