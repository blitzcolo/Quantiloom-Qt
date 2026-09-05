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
and installed by the sibling checkout `../Quantiloom-dev`. A core-side change reaches this repo only
after `./build_wsl.sh` runs *there*. `src/SdkGuard.cpp` compares a SHA-256 recorded at
configure time against the DLL actually loaded and refuses to start on a mismatch, so a
stale pairing reports itself instead of quietly misbehaving. The `build-and-run` skill
has the refresh flow and the failure modes.

### The SDK caches thermal solves, and this repo gets it for free

From SDK 0.2.7 the offline path stores a solved temperature field on disk under a
hash of its inputs — mesh, materials as merged, `[thermal]` scalars, the forcing
file's contents, the sun direction, the stepper, the library version and the GPU.
Nothing here asks for it and nothing here can turn it off from code; it is on by
default, and `QUANTILOOM_THERMAL_CACHE=0` in the environment disables it.

**`SequenceRenderDialog` is the reason to know this.** It varies only
`ir_temperature_k` per frame, which the solver overwrites anyway, so every frame
of a sequence used to re-run an identical solve — up to 165 s each on a large
scene. They now collapse to one. The CLI manifest it emits hits the same entries.
`HyperspectralExportDialog` benefits the same way.

From SDK 0.4.0 the key also covers every **geometry epoch**: a scene whose
`[[models]]` move is solved across piecewise-static spans, and a truck two metres
further along in epoch three gives a different trajectory at every hour after it.
A static scene hashes exactly as it did, so its entries survive.

The dialog's Timeline mode does not lean on the cache at all, and does not need
to: it builds **one** `OfflineRenderer` and moves the clock between frames, so
the mesh, the stepper, the steady state and the epoch schedule are paid for once
and the trajectory is stepped forward rather than replayed. `Thermal cache: hit`
per frame is what the manifest path prints, not this one.

Two consequences worth remembering. A sequence render that used to take an hour
finishing in minutes is the cache working, not a frame being skipped — the
renderer logs `Thermal cache: hit` per frame either way. And the viewport is
unaffected: `ThermalPreview` keeps its own in-memory timeline and is not what
this caches.

The SDK ships only what the core marks public: `include/quantiloom/` and the `QL_API`
exports. Reaching a core symbol that is not there takes three steps **in the dev
repo** — move its header into `include/quantiloom/`, add `QL_API`, and update
`docs/abi/exports.golden` — none of which can be done from this side. So "the header
is missing" or "unresolved external" is a signal to go widen the SDK's API on purpose,
never to work around it here (SRS CON-03: the frontend reaches the core only through
exported symbols). `src/libQuantiloom/CLAUDE.md` over there has the rule.

## Repo map

| Path | What |
|---|---|
| `src/panels/` | 13 dockable parameter panels — most feature work lands here |
| `src/ui/` | Shell infrastructure: `PanelBase`, the debug/spectral `ModeCatalog`, workspaces, the viewport frame, shared styling |
| `src/ui/theme/` | The nine themes as data (`Theme`) and the runtime switcher (`ThemeManager`). A theme is a style key, a palette, a few accent colours and an optional style sheet — adding one is a function returning a `Theme`, not code |
| `src/dialogs/` | Preferences and the generated help pages |
| `src/vulkan/` | Qt↔SDK render bridge and orbit camera; the only `ExternalRenderContext` caller |
| `src/config/` | TOML load/save (`ConfigManager`) |
| `src/editing/` | Selection, undo stack, transform gizmo |
| `src/i18n/` | Qt Linguist `.ts` (en + zh_CN) and the runtime `LanguageManager` |
| `assets/configs/` | Hand-written TOML scene configs; also the core CLI's input format |
| `assets/spectral/` | Baked copies from Quantiloom-dev. CMake warns at configure time when they drift; `scripts/sync_spectral_assets.sh --sync` re-copies and re-pins |

`src/panels/`, `src/vulkan/`, `src/config/` and `src/i18n/` each have their own
`CLAUDE.md` with the constraints that apply there.

## What a thermal scene can be asked here

Three things the viewport does with derivatives the SDK's solve already
carries, all of them read-only against the trajectory:

- **A probe.** The click that selects a node also aims it: `ThermalElementAt`
  turns the pick into an element and `GetElementTrajectory` replays that
  element's whole day, which `TrajectoryPlotWidget` draws as temperatures above
  and the six surface fluxes that produced them below. Two charts, because
  kelvin near 300 and watts per square metre either side of zero on one axis
  leaves the temperatures a flat line and the fluxes unreadable. Nothing is
  re-solved and the hour on screen is restored, so a probe cannot move the
  picture it is a probe of.
- **A what-if slider.** `SetThermalWhatIf` renders `T + dT/dp * step`, so the
  viewport follows a drag that a re-solve could not. The first ask about a
  parameter adds it to `parameterSensitivities` and costs one re-solve, because
  carrying a derivative is a solve change; every move after it costs a
  re-render. That difference is the point of having it.
- **Two debug views.** `SunSensitivity` draws what the shadow-edge correction
  is worth per triangle, `ThermalSensitivity` draws dT/dp signed.

And, when the document has a `[timeline]`, the hour stops being something this
panel sets. `thermal.time_h` then means **the hour the clock starts at**, and
which hour is being rendered comes from the transport: `TimelinePanel` moves it,
`applyTimelineTime` is the one route, and `ThermalPanel::setMappedHour` disables
the slider and says what hour it is showing rather than leaving a control that
the next tick overwrites. The status line gains `epoch i / K` — how many
piecewise-static geometry spans the solve was cut into, and which one this hour
falls in. Scrubbing the clock re-solves the field but recomputes no view factors,
which is what makes it draggable. What does recompute them is a plan change — a
config applied, a gizmo drag finished — and the SDK measures every epoch
synchronously inside that call, so the window's status bar says "Building
thermal epoch 3 of 24…" from the SDK's progress callback and repaints itself by
hand, the event loop being busy with the build.

And a comparison panel, which loads a reference EXR and reports bias, RMSE, the
95th percentile and the worst pixel against the current frame, per channel. It
reads the frame when asked rather than continuously — a path-traced image is
still accumulating, and statistics against a moving image are statistics about
the noise — and it refuses to resample a mismatched resolution, because what a
resampling does to a radiance belongs to whoever made the measurement.

## Shell shape

The window is a document editor for a TOML scene configuration: File ▸ Open takes a
config or a bare model, Save writes a config. Above the toolbar sits a row of
**workspaces** — Layout, Environment & Spectral, Material Prep, Debug — each a preset
arrangement of the independent panel docks, with the viewport always in the centre.

Four rules the shell keeps, and that new work should not break:

- **The menu bar is the complete catalogue.** Anything a panel button does, a menu entry
  does too, and shows its shortcut. A panel is a shortcut, never the only route.
- **One dispatcher per setting.** Debug mode, spectral mode, target samples, display
  enhancement and the theme each have a single `MainWindow::apply*` function; the menu,
  the toolbar control and the panel widget all call it. Two paths that do slightly
  different things is the bug class this replaced.
- **No user-visible string is set once.** Everything goes through `bindText()` or a
  `retranslateUi()`, because language switches at runtime.
- **No colour is set once.** The same sentence with a different noun, because the theme
  also switches at runtime. Panels use `bindStyle()`; other widgets own a
  `uistyle::StyleBindings` and drive it from `changeEvent`. Two consequences worth
  knowing before touching `src/ui/UiStyle.cpp`: every helper there must stay
  **idempotent** — it is re-run on the same widget on every switch, so deriving from the
  widget's *current* font or palette compounds — and a widget that restyles *itself*
  re-enters, which is what the guard in `StyleBindings::reapply()` is for.

## Conventions

- Commits: Conventional Commits — `feat:`, `fix:`, `docs:`, `chore:`.
- **No Claude Code session link in a commit message.** No `Claude-Session:` trailer,
  no `https://claude.ai/code/...` URL, in the subject, the body or a trailer. Same for
  PR descriptions.
- Physics and algorithms belong in the SDK, not here (SRS CON-02).
- User-visible strings: `tr()`, plus the Chinese in the same commit. The Chinese
  translation carries no backlog; keep it that way (`src/i18n/CLAUDE.md`).
