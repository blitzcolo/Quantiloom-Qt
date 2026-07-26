# src/panels/

`PanelBase` subclasses, one per dock. Each owns its widgets, keeps its values in plain
members, and reports changes by signal. Panels never touch the renderer or the SDK —
`MainWindow` connects them. `SpectralMaterialGenPanel` used to be the exception, holding
a `QuantiloomVulkanWindow*`; it is handed the scene like everyone else now and asks the
shell to upload its (n,k) curves.

## What PanelBase requires

- `panelId()` — stable, never translated. It is the `QObject` name, the key in
  `m_docks`, the dock's `objectName` (which is what `saveState()` records), and the id
  the workspace presets list.
- `panelTitle()` — the display name. The dock title and the View ▸ Panels entry read it,
  so a panel is never labelled in one language and filled in another. That inversion was
  real: Sensor's contents were fully translated while its tab said "Sensor".
- Strings go through `bindText()` (runs now, and again on every language change) or an
  overridden `retranslateUi()`. Nothing user-visible may be set once in the constructor
  and never again — that is what made language switching need a restart.

## Adding a panel

`MainWindow` wires panels by hand in four places. Miss any one and it still compiles,
producing a panel that never appears or never reacts:

1. `XPanel.{hpp,cpp}` here — `class XPanel : public PanelBase` with `Q_OBJECT`, the three
   overrides above, a `setupUi()`, setters for applying a loaded config, and an
   `xChanged(...)` signal.
2. Both files into `GUI_SOURCES` in `CMakeLists.txt`. Sources are listed explicitly;
   there is no glob.
3. `MainWindow.hpp` — forward declaration next to the other panels, plus an
   `XPanel* m_xPanel = nullptr;` member.
4. `MainWindow::setupDockWidgets()` — construct it, `createPanelDock(m_xPanel, area)`,
   then `connect` its signal. `createPanelDock` handles the dock, the object name, the
   title binding and the View ▸ Panels entry.

If it should appear in a workspace preset, add its id to
`WorkspaceManager::preset()` too, and bump `kLayoutVersion` so stored layouts from
before it existed are discarded rather than restored without it.

If the panel's value belongs in an exported `.toml`, it also needs a field in
`SceneConfig`, both halves of the round trip, and a read-back accessor so
`collectCurrentConfig()` can reach it; see `src/config/CLAUDE.md`.

## Do not shadow QWidget

`QWidget` already has `setEnabled`, `isEnabled`, `width` and `height`, and none of them
are virtual. `SensorPanel::setEnabled(bool)` would mean "tick the sensor box and activate
the groups"; the identical call through a `QWidget*` means "make this widget
interactive". Which one runs depends on the static type of the pointer, and nothing
warns. Name new accessors for what they carry: `setSensorEnabled`, `renderWidth`,
`setEnhancementEnabled`.

## Strings

Every user-visible string goes through `tr()` — and through `bindText()` or
`retranslateUi()` so it can be applied again. Read `src/i18n/CLAUDE.md` before
regenerating the `.ts` files; it also holds the glossary and the list of terms that stay
in Latin script by decision (RGB, LWIR, n, k, R0, unit symbols).

Qualify every temperature. Five panels once labelled five different physical quantities
"Temperature": atmosphere, ground, object, detector and cluster.
