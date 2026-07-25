# src/config/

`ConfigManager` moves scene state between TOML on disk and the `SceneConfig` struct the
panels read. Load and save share no code:

- **Load** delegates to the SDK's `quantiloom::Config::Load`, then
  `extractSceneConfig()` copies out the fields the GUI needs.
- **Save** is a hand-written serializer — `exportConfig()` (`ConfigManager.cpp:234`)
  streams TOML text with `out << ...`.

So adding a key means editing two unrelated code paths, and forgetting the writer loses
the value silently on the next round trip. That has already happened to `noise_seed`,
`scene.usd`, the hyperspectral range, and `[atmosphere] preset`.

## Two guards, both deliberate

- `static_assert(sizeof(quantiloom::SensorParams) == 84)` at the top of
  `ConfigManager.cpp`. When the SDK adds or removes a sensor field this fails to
  compile, which is the point. Confirm `exportConfig()` writes the new key before
  bumping the number.
- `MainWindow::collectCurrentConfig()` (`src/MainWindow.cpp:979`) starts from the last
  loaded `SceneConfig` rather than a default-constructed one, so fields with no widget
  behind them survive an export. Keep it that way — starting empty is exactly what made
  export lossy before.

## Known gap

Panels have no read-back interface, so a config exported after the user changed spectral
mode, camera, or lighting in the GUI still carries the values from load time.
