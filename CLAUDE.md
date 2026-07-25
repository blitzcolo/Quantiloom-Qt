# Quantiloom Studio (QuantiloomQt)

Qt6 desktop GUI for the Quantiloom spectral path tracer. Holds no physics of its own —
it links a prebuilt SDK and drives it through `ExternalRenderContext`. C++20, one CMake
target, ~10k lines.

## Build: WSL2 shell, Windows toolchain

There is no Linux `cmake` on this machine, and the Visual Studio generator needs the
Windows one anyway, so every build goes through `cmake.exe`. Run from the repo root.

| Change | Command | Time |
|---|---|---|
| C++ only | `cmake.exe --build build-msvc --config Release -j` | 1 s no-op, 48 s full recompile |
| CMakeLists, new file, or SDK refreshed | `./build_wsl.sh` | re-runs configure |

Qt defaults to `C:\Qt\6.10.1\msvc2022_64`; override with `QT_PREFIX=... ./build_wsl.sh`.

Run `./build-msvc/Release/QuantiloomQt.exe`; the window opens on the Windows desktop.
Its exit code is meaningless from a non-interactive shell (5 and 29 observed on clean
runs) — judge a run by its log, not by `$?`.

**This repo has no tests, no lint, and no formatter.** No ctest, no test target; the
suite lives in Quantiloom-dev. MSVC `/W4` at build time is the only static check.

clangd resolves symbols through `build-cdb/`, a configure-only Ninja tree that exists
because the Visual Studio generator will not emit `compile_commands.json`. Regenerate
it after adding a source file or editing CMake — `./scripts/gen_compile_commands.sh`
(7 s), or `--check` to ask whether the current one is stale.

## The SDK is a separate repository

`find_package(Quantiloom)` resolves `../Quantiloom-SDK/windows_amd64`, which is built
and installed by `/mnt/d/Quantiloom-dev`. A core-side change reaches this repo only
after `./build_wsl.sh` runs *there*. `src/SdkGuard.cpp` compares a SHA-256 recorded at
configure time against the DLL actually loaded and refuses to start on a mismatch, so a
stale pairing reports itself instead of quietly misbehaving. The `build-and-run` skill
has the refresh flow and the failure modes.

## Repo map

| Path | What |
|---|---|
| `src/panels/` | 10 dockable parameter panels — most feature work lands here |
| `src/vulkan/` | Qt↔SDK render bridge and orbit camera; the only `ExternalRenderContext` caller |
| `src/config/` | TOML load/save (`ConfigManager`) |
| `src/editing/` | Selection, undo stack, transform gizmo |
| `src/i18n/` | Qt Linguist `.ts` (en + zh_CN) |
| `assets/configs/` | Hand-written TOML scene configs; also the core CLI's input format |
| `assets/spectral/` | Baked copies from Quantiloom-dev. CMake warns at configure time when they drift; `scripts/sync_spectral_assets.sh --sync` re-copies and re-pins |

`src/panels/`, `src/vulkan/`, `src/config/` and `src/i18n/` each have their own
`CLAUDE.md` with the constraints that apply there.

## Conventions

- Commits: Conventional Commits — `feat:`, `fix:`, `docs:`, `chore:`.
- Physics and algorithms belong in the SDK, not here (SRS CON-02).
