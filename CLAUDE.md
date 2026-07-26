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
| `src/panels/` | 12 dockable parameter panels — most feature work lands here |
| `src/ui/` | Shell infrastructure: `PanelBase`, the debug/spectral `ModeCatalog`, workspaces, the viewport frame, shared styling |
| `src/dialogs/` | Preferences and the generated help pages |
| `src/vulkan/` | Qt↔SDK render bridge and orbit camera; the only `ExternalRenderContext` caller |
| `src/config/` | TOML load/save (`ConfigManager`) |
| `src/editing/` | Selection, undo stack, transform gizmo |
| `src/i18n/` | Qt Linguist `.ts` (en + zh_CN) and the runtime `LanguageManager` |
| `assets/configs/` | Hand-written TOML scene configs; also the core CLI's input format |
| `assets/spectral/` | Baked copies from Quantiloom-dev. CMake warns at configure time when they drift; `scripts/sync_spectral_assets.sh --sync` re-copies and re-pins |

`src/panels/`, `src/vulkan/`, `src/config/` and `src/i18n/` each have their own
`CLAUDE.md` with the constraints that apply there.

## Shell shape

The window is a document editor for a TOML scene configuration: File ▸ Open takes a
config or a bare model, Save writes a config. Above the toolbar sits a row of
**workspaces** — Layout, Environment & Spectral, Material Prep, Debug — each a preset
arrangement of the independent panel docks, with the viewport always in the centre.

Three rules the shell keeps, and that new work should not break:

- **The menu bar is the complete catalogue.** Anything a panel button does, a menu entry
  does too, and shows its shortcut. A panel is a shortcut, never the only route.
- **One dispatcher per setting.** Debug mode, spectral mode, target samples and display
  enhancement each have a single `MainWindow::apply*` function; the menu, the toolbar
  control and the panel widget all call it. Two paths that do slightly different things
  is the bug class this replaced.
- **No user-visible string is set once.** Everything goes through `bindText()` or a
  `retranslateUi()`, because language switches at runtime.

## Conventions

- Commits: Conventional Commits — `feat:`, `fix:`, `docs:`, `chore:`.
- Physics and algorithms belong in the SDK, not here (SRS CON-02).
- User-visible strings: `tr()`, plus the Chinese in the same commit. The Chinese
  translation carries no backlog; keep it that way (`src/i18n/CLAUDE.md`).
