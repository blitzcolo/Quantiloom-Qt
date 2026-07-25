# src/panels/

Ten `QWidget` subclasses, one per tab of the left dock. Each owns its widgets, keeps its
values in plain members, and reports changes by signal. Panels never touch the renderer
or the SDK — `MainWindow` connects them.

## Adding a panel

`MainWindow` wires panels by hand in five places. Miss any one and it still compiles,
producing a panel that never appears or never reacts:

1. `XPanel.{hpp,cpp}` here — `class XPanel : public QWidget` with `Q_OBJECT`, a
   `setupUi()`, setters for applying a loaded config, and an `xChanged(...)` signal.
2. Both files into `GUI_SOURCES` in `CMakeLists.txt`. Sources are listed explicitly;
   there is no glob.
3. `MainWindow.hpp` — forward declaration next to the other panels, plus an
   `XPanel* m_xPanel = nullptr;` member.
4. `MainWindow::setupDockWidgets()` (`src/MainWindow.cpp:216`) — construct it,
   `m_parameterTabs->addTab(m_xPanel, tr("X"))`, then `connect` its signal.
5. `MainWindow::applyConfig()` (`src/MainWindow.cpp:895`) — call the setters, or a
   loaded config never reaches the new panel.

If the panel's value belongs in an exported `.toml`, it also needs a field in
`SceneConfig` and both halves of the round trip; see `src/config/CLAUDE.md`.

## Do not shadow QWidget

`QWidget` already has `setEnabled`, `isEnabled`, `width` and `height`, and none of them
are virtual. `SensorPanel::setEnabled(bool)` means "tick the sensor box and activate the
groups"; the identical call through a `QWidget*` means "make this widget interactive".
Which one runs depends on the static type of the pointer, and nothing warns. Five such
methods already exist here — do not add a sixth. Name new accessors for what they carry:
`setSensorEnabled`, `renderWidth`.

## Strings

Every user-visible string goes through `tr()`. Read `src/i18n/CLAUDE.md` before
regenerating the `.ts` files.
