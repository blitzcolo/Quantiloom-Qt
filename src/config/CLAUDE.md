# src/config/

## What renders is not read here

Opening a `.toml` hands it to `ExternalRenderContext::ApplyConfig`, and **that** is
what configures the renderer — the same reading the core CLI does, in
`rendercore::ResolveRenderConfig`. Nothing in this directory decides what a key means
any more.

It used to. This repo interpreted the same ~50 keys itself, and the two readings had
drifted into a dozen ways the same file rendered differently depending on which program
opened it: shadow rays defaulted off here and on there, an absent
`spectral.wavelength_nm` meant 550 nm here and the band centre there,
`lighting.solar_lut_normalise` was read there and ignored here (a factor of ten thousand
on a D65 scene), and the NMF basis, `[refractive_index]` and
`scene.default_temperature_k` were read there and not at all here. The audit is
`render_path_divergence.md` at the repo root.

**So a new key is added in Quantiloom-dev**, in `ConfigResolve.cpp`, where both hosts
get it. Adding one here would recreate exactly the divergence that was just removed.

## What this directory still does

`ConfigManager` moves scene state between TOML on disk and the `SceneConfig` struct the
panels read. Load and save share no code:

- **Load** delegates to the SDK's `quantiloom::Config::Load`, then
  `extractSceneConfig()` copies out the fields the GUI needs **to show and to save**.
  Those values populate widgets; `MainWindow::syncPanelsFromRenderer()` then reconciles
  them with what the renderer actually resolved, so a panel cannot end up disagreeing
  with the render.
- **Save** is a hand-written serializer — `exportConfig()` streams TOML text with
  `out << ...`.

So adding a key means editing two unrelated code paths, and forgetting the writer loses
the value silently on the next round trip. That has already happened to `noise_seed`,
`scene.usd`, the hyperspectral range, `[atmosphere] preset`, `lighting.transmittance`
and the nine `[atmosphere]` weather features.

## What round-trips, and what does not

Everything a panel owns is written back, and `MainWindow::collectCurrentConfig()` reads
it from the panel rather than from the config that was loaded. That includes the
spectral mode and wavelength range, the camera (read from the renderer, so orbiting with
the mouse is exported too), the merged lighting parameters, the atmosphere preset and
its nine weather features, and the whole sensor block.

Three things are deliberately **session state** and are not in the file:

- the debug visualization mode — a way of looking at the scene, not part of it;
- display enhancement (CLAHE) — a viewing aid that does not change the rendered values;
- the spectral generator's working set — anchor points are saved through its own CSV.

If you add a panel value, decide which of the two lists it belongs in and say so here.

## Two guards, both deliberate

- `static_assert(sizeof(quantiloom::SensorParams) == 84)` at the top of
  `ConfigManager.cpp`. When the SDK adds or removes a sensor field this fails to
  compile, which is the point. Confirm `exportConfig()` writes the new key before
  bumping the number.
- `MainWindow::collectCurrentConfig()` starts from the last loaded `SceneConfig` rather
  than a default-constructed one, so fields with no widget behind them survive an export.
  Keep it that way — starting empty is exactly what made export lossy before.

## Deprecations the core has already made

`src/app/main.cpp` in Quantiloom-dev warns that `lighting.transmittance` is no longer
used (view-path transmittance comes from the NN atmosphere) and that
`lighting.atmosphere_temperature_k` survives only as the thermal-sky fallback when that
atmosphere is disabled. Both are still written, because a config this GUI produced
should describe what the GUI was holding; the atmosphere panel says which is which.

## Commits

**No Claude Code session link in a commit message.** No `Claude-Session:` trailer,
no `https://claude.ai/code/...` URL, in the subject, the body or a trailer. Same for
PR descriptions.
