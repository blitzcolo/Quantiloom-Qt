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
`scene.default_temperature_k` were read there and not at all here. The audit that
enumerated them was a one-off review and is not checked in.

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
its nine weather features, and the whole sensor block — whose `[sensor]` tree is now
the SDK's serialization in both directions: `ParseCameraConfig` on load, which
migrates a legacy unversioned `[sensor]` block, and `CameraConfigToToml` on save, so
Studio writes exactly what the CLI writes.

Node transforms and material edits round-trip too, and did not used to. Both are
written only for what changed since the document was opened — `MainWindow` keeps
`m_editedNodes` and `m_editedMaterials`, the `apply*` dispatchers add to them, and
`applyConfig()` clears them. A scene with a thousand nodes does not want a thousand
`[[nodes]]` blocks restating where the model file already put them. Both are matched by
name in the file, so a node or material the scene left unnamed cannot be written down —
a limit of the schema, not something to paper over with an index that the next export of
the model would invalidate.

`[material] albedo` is written because the core requires it. It never was, so every
configuration this exporter produced rendered here under `WarnAndDefault` and was
refused by the CLI, which reads the same file under `Error`.

**Materials carry their untouched keys.** Each `MaterialConfig` holds `preserved`, the
material's merged `[[materials]]`/`[material_overrides."name"]` table as the document
had it, so keys no widget owns (`fusion_transport`, `ior`, `dispersion`, ...) are
written back. `writeConfig()` replaces the widget-owned keys as a group, so a cleared
binding stays cleared and the singular/plural `spectral_material_ref(s)` aliases never
both appear. On load, `[material_overrides]` entries are folded into `[[materials]]`,
a named override winning over the material it names.

**Save As rebases every relative path.** `exportConfig()` rewrites the scene, the
solar LUT, the environment map, the basis and materials JSON, the thermal forcing
file, the spectral curves and the refractive-index files from the document's base
directory onto the destination's, so a Save As into another directory still resolves.
Writes go through `QSaveFile`, so a failed save does not leave a truncated document
behind.

### The clock and the models are carried, not read

`[timeline]` and `[[models]]` round-trip as **text**. `Config::ToToml("<key>")` gives
the block back the way the core serialised it, and `writeConfig()` prints it unchanged.

That is not laziness. `end_s = "36h"` and `end_s = 129600` are the same timeline, and a
motion is written in one of three grammars — keyframes, piecewise expressions over `t`
and `s`, or a parametric engine — one of which has an expression parser behind it.
Reading any of that here would be a second implementation of the thing this file exists
to have exactly one of. What the panels show comes from `TimelineInfo`, which the SDK
already resolved.

One key inside `[timeline]` is Studio's: `time_s`, where the transport stands. It is
stripped out of the carried body on the way in and written back from
`m_timelineTimeS` on the way out. It is also the reason moving the transport does not
mark the document modified — where you are looking is not an edit, and playback would
otherwise mark a document dirty twenty times a second. A save records it either way.

`[[nodes]]` gained a meaning it did not have: for a node the clock moves, it records the
**rest pose** — where the node would stand with every trajectory at the identity — read
from `nodeRestTransform()` rather than from the node's own transform. The transform is
where the node is at the current tick, which is not a thing a document can record,
because it would be wrong at every other tick. For a node with no trajectory the two are
the same matrix.

`[dataset]` is carried as text the same way `[timeline]` is, because the SDK owns its
meaning too.

### The quantitative spectral sections

`[spectral_curves]`, `[refractive_index]`, `lighting.solar_lut*`,
`spectral.basis_file` / `materials_json` / `band`, `scene.default_temperature_k`,
`quality.fail_on_srgb_upsample` / `log_material_sources` and the whole
`[hyperspectral]` block round-trip **without any widget behind them**. No panel edits
them yet; they are read in `extractQuantitativeSpectral()` purely so `writeConfig()`
can put them back. Until this was added, opening a hand-authored quantitative config
and saving it stripped every one of them — the illuminant included, which turns the
spectral modes black, and the NMF database reference, which is the core's flagship
measured-material path. A file lost its meaning by being looked at.

The three USD load options — `scene.usd_time_code`, `scene.usd_payloads` and
`scene.usd_stage_metrics` — are in the same group and there for the same reason.
Studio has no widget for any of them and no reading of what they mean: which
sample an animated stage is read at, whether its payloads are opened, and whether
its `upAxis` and `metersPerUnit` are folded into the node transforms are all
decided in the core, in `ConfigResolve.cpp` and `RenderCore.cpp`. `scene.variant`
sits beside them and is shared by both formats — a glTF variant name or a USD
`/Root/Car{color=red}` spec, and which syntax it is in follows from which loader
reads it. Studio carries all four so that saving does not lose them.

The optional ones (`default_temperature_k`, the two `quality` gates, `[hyperspectral]`,
`usd_time_code`, `usd_stage_metrics`) are `std::optional` so that a file which never
spelled them out does not acquire them on save. When a panel eventually owns one of these, it moves into the ordinary
"panel owns it, `collectCurrentConfig()` reads it back" group; the round trip is
already in place.

**A mode the panel cannot express stays the document's.** `collectCurrentConfig()`
takes the spectral mode from the panel only when the loaded mode is one
`ModeCatalog::spectralModes()` offers. `Multispectral` is deliberately not offered —
a cube cannot render progressively, so the viewport previews it as RGB — and taking
the panel's answer rewrote `mode = "multispectral"` to `"rgb"` on the first save.

Three things are deliberately **session state** and are not in the file:

- the debug visualization mode — a way of looking at the scene, not part of it;
- display enhancement (CLAHE) — a viewing aid that does not change the rendered values;
- the spectral generator's working set — anchor points are saved through its own CSV.

If you add a panel value, decide which of the two lists it belongs in and say so here.

## One guard, deliberate

- The `[sensor]` writer is the SDK's now, so there is nothing left to guard — the
  `static_assert(sizeof(quantiloom::SensorParams) == 88)` that policed it went with
  the hand-written serializer it protected.
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

**No Codex session link in a commit message.** No `Codex-Session:` trailer,
no `https://Codex.ai/code/...` URL, in the subject, the body or a trailer. Same for
PR descriptions.
