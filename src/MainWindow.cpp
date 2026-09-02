/**
 * @file MainWindow.cpp
 * @brief Main application window implementation
 *
 * @author blitzcolo
 */

#include "MainWindow.hpp"
#include "vulkan/QuantiloomVulkanWindow.hpp"
#include "panels/SceneTreePanel.hpp"
#include "panels/MaterialEditorPanel.hpp"
#include "panels/LightingPanel.hpp"
#include "panels/RenderSettingsPanel.hpp"
#include "panels/SpectralConfigPanel.hpp"
#include "panels/SpectralLibraryPanel.hpp"
#include "panels/DebugVisualizationPanel.hpp"
#include "panels/AtmosphericPanel.hpp"
#include "panels/SensorPanel.hpp"
#include "panels/ThermalPanel.hpp"
#include "panels/DisplayEnhancementPanel.hpp"
#include "panels/SpectralMaterialGenPanel.hpp"
#include "panels/PropertiesPanel.hpp"
#include "panels/CameraPanel.hpp"
#include "panels/ComparisonPanel.hpp"
#include "config/ConfigManager.hpp"
#include "editing/SelectionManager.hpp"
#include "editing/TransformGizmo.hpp"
#include "editing/UndoStack.hpp"
#include "editing/Commands.hpp"
#include "dialogs/PreferencesDialog.hpp"
#include "dialogs/HyperspectralExportDialog.hpp"
#include "dialogs/SequenceRenderDialog.hpp"
#include "dialogs/HelpDialog.hpp"
#include "i18n/LanguageManager.hpp"
#include "ui/ModeCatalog.hpp"
#include "ui/UiStyle.hpp"
#include "ui/ViewportFrame.hpp"
#include "ui/theme/ThemeManager.hpp"
#include "ui/chrome/TitleBar.hpp"
#include "ui/chrome/WindowChrome.hpp"
#include "ui/WorkspaceManager.hpp"

#include <QApplication>
#include <QGuiApplication>
#include <QClipboard>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QKeyEvent>
#include <QToolBar>
#include <QStyle>
#include <QDockWidget>
#include <QScreen>
#include <QScrollArea>
#include <QTabWidget>
#include <QStatusBar>
#include <QProgressBar>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QSettings>
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>

#include <core/Types.hpp>
#include <core/Image.hpp>
#include <io/ImageIO.hpp>
#include <io/SpectralIO.hpp>
#include <renderer/ExternalRenderContext.hpp>
#include <core/Log.hpp>
#include <scene/Material.hpp>
#include <scene/Scene.hpp>
#include <renderer/LightingParams.hpp>
#include <postprocess/SensorModel.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <utility>

namespace {

constexpr auto kRecentFilesKey = "recent_files";
constexpr auto kMcpPortKey = "mcp_port";
constexpr int  kMaxRecentFiles = 8;
constexpr auto kGeometryKey    = "window/geometry";
constexpr auto kStateVersionKey = "window/state_version";
constexpr auto kShowGridKey    = "viewport/show_grid";
constexpr auto kMotionAdaptiveResolutionKey = "viewport/motion_adaptive_resolution";

/// Bumped whenever the dock inventory changes, so a layout written by an older
/// build is discarded instead of restored into a window that no longer has the
/// same docks.
constexpr int kWindowStateVersion = 1;

/// Every panel goes into a scroll area. A minimum window size only bounds the
/// window; it does nothing about a dock that is shorter than the panel inside
/// it, which is how the taller panels ended up with squashed, unreachable
/// controls at modest window heights.
QScrollArea* wrapScrollable(QWidget* panel) {
    auto* area = new QScrollArea();
    area->setWidget(panel);
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    return area;
}

} // namespace

MainWindow::MainWindow(QVulkanInstance* vulkanInstance, QWidget* parent)
    : QMainWindow(parent)
    , m_vulkanInstance(vulkanInstance)
{
    applyScreenAwareGeometry();

    // Create configuration manager
    m_configManager = new ConfigManager(this);
    m_lightingParams = std::make_unique<quantiloom::LightingParams>(
        quantiloom::CreateDefaultLightingParams());

    // The editing objects come first: the Edit ▸ Transform actions built in
    // setupMenus() drive the gizmo directly.
    m_styling.attach(this);

    m_selectionManager = new SelectionManager(this);
    m_transformGizmo = new TransformGizmo(this);
    m_undoStack = new UndoStack(this);

    setupUi();
    setupMenus();
    setupDockWidgets();
    setupToolBar();
    setupWorkspaces();
    setupStatusBar();
    setupEditingSystem();
    setupConnections();

    // Dropping a configuration onto the window opens it.
    setAcceptDrops(true);

    // Progressive accumulation is already running when the window opens, so
    // "start" is the entry that has nothing to do yet.
    m_startRenderAction->setEnabled(false);

    retranslateUi();
    restoreWindowState();
    m_workspaces->activateInitial();
    updateWindowTitle();

    // The grid ships on: a scene-layout viewport without a ground reference
    // is the exception, not the default. Queued if the renderer is not up yet.
    // Then settle the editing scope for whichever workspace activateInitial
    // landed on -- the preference had not been read when its signal fired.
    {
        QSettings settings;
        applyGridVisible(settings.value(kShowGridKey, true).toBool());
        applyMotionAdaptiveResolution(
            settings.value(kMotionAdaptiveResolutionKey, true).toBool());
        applyWorkspaceEditingScope(m_workspaces->currentWorkspace());
    }

    // Last, and after restoreWindowState(): taking over the non-client area
    // forces a frame recalculation, so it wants the window at its final size.
    if (m_titleBar) {
        m_chrome = std::make_unique<WindowChrome>(this, m_titleBar);
        m_chrome->install();
        m_titleBar->setWindowMaximized(isMaximized());
    }

    m_viewportFrame->setRecentFiles(recentFiles());

    // Make QMainWindowLayout re-read the dock separator width from the shell
    // style sheet, which it has not done at any point above.
    //
    // That width is a style pixel metric, PM_DockWidgetSeparatorExtent, and
    // QMainWindowLayout reads it exactly once -- from its own constructor,
    // which runs inside the QMainWindow base constructor, before this body has
    // set any style sheet. The only thing that refreshes it afterwards is
    // QMainWindow::event() seeing a QEvent::StyleChange. At startup no such
    // event arrives after the sheet is in place: ThemeManager calls
    // QApplication::setStyle() from main(), before this window exists, and the
    // StyleChange that setStyleSheet() itself sends lands while the window is
    // still unpolished, so the style sheet rule does not resolve and the metric
    // falls back to the plain Fusion default.
    //
    // So the width was silently ignored for the whole first session and only
    // took effect once the user switched themes, which calls setStyle() again
    // with the window up. Every theme had this; it stayed invisible because the
    // seven that leave Accents::separator unset ask for 4px and the unstyled
    // default is close enough to pass. Print Friendly asks for 1px against a
    // white ground, where a four-pixel black band is not something you miss.
    //
    // Queued rather than sent here, because "here" is still inside the
    // constructor and the window is not polished yet -- the same condition that
    // defeated the event setStyleSheet() already sent. One shot, not bound into
    // m_styling: a theme switch re-reads the metric on its own, and rescheduling
    // this from the setter it provokes would not terminate.
    QTimer::singleShot(0, this, [this] {
        QEvent styleChange(QEvent::StyleChange);
        QApplication::sendEvent(this, &styleChange);
    });
}

MainWindow::~MainWindow() = default;

void MainWindow::applyScreenAwareGeometry() {
    // 1280x720 as a hard minimum is a logical size, and on a small laptop at
    // 125% the logical desktop can be smaller than that -- the window would be
    // larger than the screen and its buttons unreachable. Bound both the
    // minimum and the initial size by what is actually available.
    QSize available(1280, 720);
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        available = screen->availableGeometry().size();
    }

    const QSize minimum(std::min(1024, available.width()  - 40),
                        std::min(640,  available.height() - 60));
    setMinimumSize(minimum.expandedTo(QSize(720, 480)));

    const QSize initial(std::min(1600, available.width()  - 40),
                        std::min(900,  available.height() - 60));
    resize(initial.expandedTo(minimum));
}

void MainWindow::setupUi() {
    // Create Vulkan window
    m_vulkanWindow = new QuantiloomVulkanWindow();
    m_vulkanWindow->setVulkanInstance(m_vulkanInstance);
    // Before the window is shown, which is when Qt commits to a device.
    m_vulkanWindow->selectRayTracingDevice();

    // Wrap in QWidget container for use as central widget
    m_vulkanContainer = QWidget::createWindowContainer(m_vulkanWindow);
    m_vulkanContainer->setMinimumSize(320, 240);
    m_vulkanContainer->setFocusPolicy(Qt::StrongFocus);
    m_vulkanContainer->setMouseTracking(true);

    // The transform keys are QActions now; this filter is what lets them keep
    // working while the native render surface has focus, without the viewport
    // owning a second copy of the logic.
    m_vulkanWindow->installEventFilter(this);

    // Visible boundaries between the docked regions; without this the docks,
    // the viewport and the window background are all the same colour with a
    // one-pixel gap between them. Bound rather than set, because the colours
    // come from the palette and the palette changes with the theme.
    m_styling.bind([this] { setStyleSheet(uistyle::shellStyleSheet()); });

    // The selection box is drawn by the overlay, not by a stylesheet, so the
    // colour has to be pushed to it on every theme switch. Converted to
    // linear here because the overlay writes into an sRGB target.
    m_styling.bind([this] {
        const QColor accent = palette().color(QPalette::Highlight);
        auto toLinear = [](float c) {
            return (c <= 0.04045f) ? (c / 12.92f)
                                   : std::pow((c + 0.055f) / 1.055f, 2.4f);
        };
        m_vulkanWindow->setSelectionBoxColor(glm::vec4(
            toLinear(static_cast<float>(accent.redF())),
            toLinear(static_cast<float>(accent.greenF())),
            toLinear(static_cast<float>(accent.blueF())),
            0.85f));
    });

    m_viewportFrame = new ViewportFrame(m_vulkanContainer, this);
    connect(m_viewportFrame, &ViewportFrame::openSceneRequested,
            this, &MainWindow::onOpenScene);
    connect(m_viewportFrame, &ViewportFrame::openRecentRequested,
            this, [this](const QString& path) {
                if (confirmDiscardChanges()) {
                    openPath(path);
                }
            });

    setCentralWidget(m_viewportFrame);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_vulkanWindow || event->type() != QEvent::KeyPress) {
        return QMainWindow::eventFilter(watched, event);
    }

    auto* keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
        return QMainWindow::eventFilter(watched, event);
    }

    QAction* action = nullptr;
    switch (keyEvent->key()) {
        case Qt::Key_G:     action = m_translateAction;  break;
        case Qt::Key_R:     action = m_rotateAction;     break;
        case Qt::Key_T:     action = m_scaleAction;      break;
        case Qt::Key_Space: action = m_localSpaceAction; break;
        // The only pair here that Shift distinguishes rather than modifies.
        case Qt::Key_F:
            action = (keyEvent->modifiers() & Qt::ShiftModifier)
                ? m_frameAllAction : m_frameSelectedAction;
            break;
        default: break;
    }

    if (!action) {
        return QMainWindow::eventFilter(watched, event);
    }

    // Reached only when Qt's shortcut system did not already consume the key
    // (it does when a widget outside the render surface has focus), so the
    // action runs exactly once either way. trigger() flips a checkable action
    // itself, which is why the check state is not touched here.
    action->trigger();
    return true;
}

// ============================================================================
// Menus
// ============================================================================

void MainWindow::setupMenus() {
    // The menu bar lives inside a container with the title bar above it, and
    // the pair is installed as the window's menu widget. Built explicitly
    // rather than through menuBar(): that accessor creates and installs a menu
    // bar of its own, which setMenuWidget() would then have to displace, and
    // QMainWindow deletes whichever of the two it is holding.
    m_menuBar = new QMenuBar();

    auto* topArea = new QWidget(this);
    auto* topLayout = new QVBoxLayout(topArea);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);

    if (WindowChrome::isSupported()) {
        m_titleBar = new TitleBar(topArea);
        topLayout->addWidget(m_titleBar);
        connect(m_titleBar, &TitleBar::minimiseRequested, this, &MainWindow::showMinimized);
        connect(m_titleBar, &TitleBar::maximiseRequested, this, [this]() {
            // Not toggling a flag: isMaximized() is the state Windows itself
            // reports, and a maximise triggered by Aero Snap never went
            // through this button.
            if (isMaximized()) {
                showNormal();
            } else {
                showMaximized();
            }
        });
        connect(m_titleBar, &TitleBar::closeRequested, this, &MainWindow::close);
    }
    topLayout->addWidget(m_menuBar);
    setMenuWidget(topArea);

    // --- File -----------------------------------------------------------
    m_fileMenu = m_menuBar->addMenu(QString());

    m_openAction = m_fileMenu->addAction(QString(), this, &MainWindow::onOpenScene);
    m_openAction->setShortcut(QKeySequence::Open);

    m_recentMenu = m_fileMenu->addMenu(QString());

    m_fileMenu->addSeparator();

    m_saveAction = m_fileMenu->addAction(QString(), this, [this]() { onSaveScene(); });
    m_saveAction->setShortcut(QKeySequence::Save);

    m_saveAsAction = m_fileMenu->addAction(QString(), this, [this]() { onSaveSceneAs(); });
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);

    m_fileMenu->addSeparator();

    m_exportImageAction = m_fileMenu->addAction(QString(), this, &MainWindow::onExportImage);
    // The cube the viewport cannot show. An offline render on a device of its
    // own, so it is a File action rather than anything under Render.
    m_exportCubeAction = m_fileMenu->addAction(QString(), this,
                                               &MainWindow::onExportHyperspectralCube);
    // A sequence is many offline renders rather than one, and for the same
    // reason lives beside the cube rather than under Render: neither is
    // something the viewport can show.
    m_renderSequenceAction = m_fileMenu->addAction(QString(), this,
                                                   &MainWindow::onRenderSequence);
    // The temperature field under the image rather than the image: the numbers
    // the solver produced, before the per-pixel sun correction and the radiance
    // inversion the render puts on top. A File action because what it writes is
    // a file the viewport cannot show, like the two above it.
    m_dumpThermalElementsAction = m_fileMenu->addAction(
        QString(), this, &MainWindow::onDumpThermalElements);
    m_exportImageAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));

    m_screenshotAction = m_fileMenu->addAction(QString(), this, &MainWindow::onTakeScreenshot);
    m_screenshotAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));

    m_fileMenu->addSeparator();

    m_exitAction = m_fileMenu->addAction(QString(), this, &QWidget::close);
    m_exitAction->setShortcut(QKeySequence::Quit);

    // --- Edit -----------------------------------------------------------
    m_editMenu = m_menuBar->addMenu(QString());

    m_undoAction = m_editMenu->addAction(QString());
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);

    m_redoAction = m_editMenu->addAction(QString());
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setEnabled(false);

    m_editMenu->addSeparator();

    // The transform vocabulary — mode, axis constraint, coordinate space —
    // was the most used part of the editor and appeared in no menu at all.
    m_transformMenu = m_editMenu->addMenu(QString());
    buildTransformMenu(m_transformMenu);

    m_editMenu->addSeparator();

    // Selection, not editing: picking objects to inspect them is useful in
    // every workspace, so unlike the transform vocabulary above these are
    // not gated by applyWorkspaceEditingScope. Ctrl+I matches both Blender
    // and Unreal for invert.
    m_selectAllAction = m_editMenu->addAction(QString(), this, &MainWindow::onSelectAll);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);

    m_invertSelectionAction = m_editMenu->addAction(QString(), this, &MainWindow::onInvertSelection);
    m_invertSelectionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));

    m_editMenu->addSeparator();

    // Copy/paste of scene objects. A focused text field keeps these keys for
    // its own editing: Qt's ShortcutOverride gives the line edit first
    // refusal, so Ctrl+C in a name box copies text, not nodes. Copy is
    // harmless everywhere; paste, duplicate and delete change the scene and
    // follow the Layout workspace (applyWorkspaceEditingScope).
    m_copyAction = m_editMenu->addAction(QString(), this, &MainWindow::onCopyNodes);
    m_copyAction->setShortcut(QKeySequence::Copy);

    m_pasteAction = m_editMenu->addAction(QString(), this, &MainWindow::onPasteNodes);
    m_pasteAction->setShortcut(QKeySequence::Paste);

    // Ctrl+D, the key Unreal ships and Blender users remap to: one step
    // instead of copy-then-paste, without touching the clipboard
    m_duplicateAction = m_editMenu->addAction(QString(), this, &MainWindow::onDuplicateNodes);
    m_duplicateAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));

    m_deleteAction = m_editMenu->addAction(QString(), this, &MainWindow::onDeleteNodes);
    m_deleteAction->setShortcut(QKeySequence::Delete);

    m_editMenu->addSeparator();

    m_preferencesAction = m_editMenu->addAction(QString(), this, &MainWindow::onPreferences);
    m_preferencesAction->setShortcut(QKeySequence::Preferences);
    m_preferencesAction->setMenuRole(QAction::PreferencesRole);

    // --- View -----------------------------------------------------------
    m_viewMenu = m_menuBar->addMenu(QString());

    // Panel visibility. Every dock contributes its own toggleViewAction, which
    // is two-way bound to the dock by Qt: closing a panel with its × ticks the
    // entry off, and the entry is how it comes back. The old menu had a
    // checkable "Parameter Panel" item wired to nothing at all, so a closed
    // dock could only be recovered through a side effect of the Tools menu.
    m_panelsMenu = m_viewMenu->addMenu(QString());

    m_resetLayoutAction = m_viewMenu->addAction(QString(), this, &MainWindow::onResetLayout);

    m_viewMenu->addSeparator();

    m_cameraMenu = m_viewMenu->addMenu(QString());
    buildCameraMenu(m_cameraMenu);

    m_viewMenu->addSeparator();

    // 45 debug modes used to live in a single combo box inside one panel,
    // grouped by fake "-- Geometry --" entries that had to be skipped over in
    // code when selected. They are real submenus now, exclusive, and reachable
    // without hunting for the panel that owned them.
    m_debugMenu = m_viewMenu->addMenu(QString());

    m_displayEnhancementAction = m_viewMenu->addAction(QString());
    m_displayEnhancementAction->setCheckable(true);
    connect(m_displayEnhancementAction, &QAction::triggered,
            this, &MainWindow::applyDisplayEnhancementEnabled);

    m_showGridAction = m_viewMenu->addAction(QString());
    m_showGridAction->setCheckable(true);
    // Ctrl+`, the overlay-toggle corner of the keyboard; the bare letters
    // near it are taken by the transform gizmo (G/R/T, X/Y/Z).
    m_showGridAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_QuoteLeft));
    connect(m_showGridAction, &QAction::triggered,
            this, &MainWindow::applyGridVisible);

    m_motionAdaptiveResolutionAction = m_viewMenu->addAction(QString());
    m_motionAdaptiveResolutionAction->setCheckable(true);
    connect(m_motionAdaptiveResolutionAction, &QAction::triggered,
            this, &MainWindow::applyMotionAdaptiveResolution);

    m_viewMenu->addSeparator();

    // The theme is also reachable from Edit ▸ Preferences. It is here too
    // because the menu bar is the complete catalogue: a setting that only
    // exists inside a dialog is a setting with no shortcut and no discovery.
    m_themeMenu = m_viewMenu->addMenu(QString());
    buildThemeMenu(m_themeMenu);

    // --- Render ---------------------------------------------------------
    m_renderMenu = m_menuBar->addMenu(QString());

    m_startRenderAction = m_renderMenu->addAction(QString(), this, &MainWindow::onStartRender);
    m_startRenderAction->setShortcut(QKeySequence(Qt::Key_F5));

    // Not Escape. Escape already means "cancel the gizmo drag / clear the
    // selection" inside the viewport, and which of the two ran depended on
    // where the focus happened to be.
    m_stopRenderAction = m_renderMenu->addAction(QString(), this, &MainWindow::onStopRender);
    m_stopRenderAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5));

    // The third verb. Start has always reset first, so stopping at 900 of
    // 1024 samples and pressing it again threw away the 900 -- there was no
    // way to say "carry on from here".
    m_resumeRenderAction = m_renderMenu->addAction(QString(), this, &MainWindow::onResumeRender);
    m_resumeRenderAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F5));
    m_resumeRenderAction->setEnabled(false);

    m_renderMenu->addSeparator();

    m_resetAccumulationAction = m_renderMenu->addAction(QString(), this,
                                                        &MainWindow::onResetAccumulation);
    m_resetAccumulationAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));

    m_renderMenu->addSeparator();

    m_qualityMenu = m_renderMenu->addMenu(QString());
    buildQualityMenu(m_qualityMenu);

    // The spectral mode decides what the rendered numbers physically are, and
    // reaching it meant finding one panel among ten.
    m_spectralMenu = m_renderMenu->addMenu(QString());

    // Binding a measured spectrum is a menu verb like any other. The library
    // panel's two buttons are a shortcut to these, never the only route.
    m_renderMenu->addSeparator();

    m_assignSpectrumAction = m_renderMenu->addAction(QString(), this, [this]() {
        const auto entry = m_spectralLibraryPanel->highlightedEntry();
        if (!entry.first.isEmpty()) {
            applySpectralEndmembers(m_currentMaterialIndex, entry.first, {entry.second});
        }
    });
    m_assignSpectrumAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
    m_assignSpectrumAction->setEnabled(false);

    m_addEndmemberAction = m_renderMenu->addAction(QString(), this, [this]() {
        const auto entry = m_spectralLibraryPanel->highlightedEntry();
        if (!entry.first.isEmpty()) {
            onSpectralEndmemberAdded(entry.first, entry.second);
        }
    });
    m_addEndmemberAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    m_addEndmemberAction->setEnabled(false);

    // --- Tools ----------------------------------------------------------
    m_toolsMenu = m_menuBar->addMenu(QString());
    m_spectralGenAction = m_toolsMenu->addAction(QString(), this, [this]() {
        QDockWidget* dock = m_docks.value(QStringLiteral("spectralgen"), nullptr);
        if (!dock) {
            return;
        }
        // Reopen where it was left; a first use gets the floating window.
        dock->show();
        dock->raise();
        if (dock->isFloating()) {
            dock->activateWindow();
        }
    });

    m_toolsMenu->addSeparator();
    m_mcpAction = m_toolsMenu->addAction(QString());
    m_mcpAction->setCheckable(true);
    connect(m_mcpAction, &QAction::toggled, this, &MainWindow::setMcpServerRunning);

    // --- Help -----------------------------------------------------------
    m_helpMenu = m_menuBar->addMenu(QString());
    m_shortcutsAction = m_helpMenu->addAction(QString(), this, &MainWindow::onShowShortcuts);
    m_shortcutsAction->setShortcut(QKeySequence::HelpContents);
    m_debugReferenceAction = m_helpMenu->addAction(QString(), this,
                                                   &MainWindow::onShowDebugReference);
    m_helpMenu->addSeparator();
    m_aboutAction = m_helpMenu->addAction(QString(), this, &MainWindow::onAbout);
    m_aboutQtAction = m_helpMenu->addAction(QString(), qApp, &QApplication::aboutQt);
}

void MainWindow::buildCameraMenu(QMenu* menu) {
    m_resetCameraAction = menu->addAction(QString(), this, &MainWindow::onResetCamera);

    // The most-used viewport command in every DCC tool, and the orbit pivot
    // had no way to reach the selection at all: it stayed wherever the config
    // put it, so after panning across a scene, orbiting spun about a point
    // nowhere near what was being looked at.
    m_frameSelectedAction = menu->addAction(QString(), this, &MainWindow::onFrameSelected);
    m_frameSelectedAction->setShortcut(QKeySequence(Qt::Key_F));
    m_frameAllAction = menu->addAction(QString(), this, &MainWindow::onFrameAll);
    m_frameAllAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F));

    // Beside the view presets, because it is what makes them worth having:
    // a front or top view with convergence measures nothing.
    m_orthographicAction = menu->addAction(QString());
    m_orthographicAction->setCheckable(true);
    m_orthographicAction->setShortcut(QKeySequence(Qt::Key_5));
    connect(m_orthographicAction, &QAction::triggered,
            this, [this](bool orthographic) { applyCameraProjection(orthographic); });

    menu->addSeparator();

    // Direction the camera sits in relative to its target. Y is up.
    struct Preset { const char* id; glm::vec3 direction; };
    static const Preset presets[] = {
        {"front",  { 0.0f,  0.0f,  1.0f}},
        {"back",   { 0.0f,  0.0f, -1.0f}},
        {"right",  { 1.0f,  0.0f,  0.0f}},
        {"left",   {-1.0f,  0.0f,  0.0f}},
        {"top",    { 0.0f,  1.0f,  0.0f}},
        {"bottom", { 0.0f, -1.0f,  0.0f}},
    };

    m_viewPresetActions.clear();
    for (const Preset& preset : presets) {
        QAction* action = menu->addAction(QString());
        action->setData(QString::fromLatin1(preset.id));
        const glm::vec3 direction = preset.direction;
        connect(action, &QAction::triggered, this, [this, direction]() {
            m_vulkanWindow->setViewDirection(direction);
        });
        m_viewPresetActions.append(action);
    }
}

void MainWindow::buildTransformMenu(QMenu* menu) {
    m_transformModeGroup = new QActionGroup(this);
    m_transformModeGroup->setExclusive(true);

    auto addMode = [&](TransformGizmo::Mode mode, const QKeySequence& shortcut) {
        QAction* action = menu->addAction(QString());
        action->setCheckable(true);
        action->setShortcut(shortcut);
        m_transformModeGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, mode]() {
            m_transformGizmo->setMode(mode);
        });
        return action;
    };

    m_translateAction = addMode(TransformGizmo::Mode::Translate, QKeySequence(Qt::Key_G));
    m_rotateAction    = addMode(TransformGizmo::Mode::Rotate,    QKeySequence(Qt::Key_R));
    m_scaleAction     = addMode(TransformGizmo::Mode::Scale,     QKeySequence(Qt::Key_T));
    m_translateAction->setChecked(true);

    // "Constrain to X/Y/Z" used to sit here on X/Y/Z. The state it set was read
    // by nothing -- a drag is constrained by the handle it grabs, not by this
    // -- so the entries ticked and did nothing. Removed rather than left
    // lying: restoring them means building modal transforms (press G, then X,
    // then move the mouse), which is a feature, not a reconnection.

    menu->addSeparator();

    m_localSpaceAction = menu->addAction(QString());
    m_localSpaceAction->setCheckable(true);
    m_localSpaceAction->setShortcut(QKeySequence(Qt::Key_Space));
    connect(m_localSpaceAction, &QAction::triggered, this, [this](bool local) {
        m_transformGizmo->setSpace(local ? TransformGizmo::Space::Local
                                         : TransformGizmo::Space::World);
    });
}

void MainWindow::buildQualityMenu(QMenu* menu) {
    m_qualityGroup = new QActionGroup(this);
    m_qualityGroup->setExclusive(true);

    for (const catalog::QualityPreset& preset : catalog::qualityPresets()) {
        if (preset.spp == 0) menu->addSeparator();
        QAction* action = menu->addAction(QString());
        action->setCheckable(true);
        action->setData(preset.spp);
        m_qualityGroup->addAction(action);
        const uint32_t spp = preset.spp;
        connect(action, &QAction::triggered, this, [this, spp]() { applyTargetSpp(spp); });
    }
}

void MainWindow::buildSpectralMenu(QMenu* menu, QComboBox* combo) {
    m_spectralGroup = new QActionGroup(this);
    m_spectralGroup->setExclusive(true);

    for (quantiloom::SpectralMode mode : catalog::spectralModes()) {
        QAction* action = menu->addAction(QString());
        action->setCheckable(true);
        action->setData(static_cast<int>(mode));
        m_spectralGroup->addAction(action);
        m_spectralActions.insert(static_cast<int>(mode), action);
        connect(action, &QAction::triggered, this, [this, mode]() { applySpectralMode(mode); });

        combo->addItem(QString(), static_cast<int>(mode));
    }

    connect(combo, &QComboBox::currentIndexChanged, this, [this, combo](int index) {
        if (index < 0) return;
        applySpectralMode(static_cast<quantiloom::SpectralMode>(combo->itemData(index).toInt()));
    });

    m_spectralActions.value(static_cast<int>(quantiloom::SpectralMode::RGB))->setChecked(true);
}

void MainWindow::buildDebugMenu(QMenu* menu, QComboBox* combo) {
    m_debugGroup = new QActionGroup(this);
    m_debugGroup->setExclusive(true);

    auto addMode = [&](QMenu* target, quantiloom::DebugVisualizationMode mode) {
        QAction* action = target->addAction(QString());
        action->setCheckable(true);
        action->setData(static_cast<int>(mode));
        m_debugGroup->addAction(action);
        m_debugActions.insert(static_cast<int>(mode), action);
        connect(action, &QAction::triggered, this, [this, mode]() { applyDebugMode(mode); });
        combo->addItem(QString(), static_cast<int>(mode));
    };

    addMode(menu, quantiloom::DebugVisualizationMode::None);
    menu->addSeparator();

    m_debugCategoryMenus.clear();
    for (catalog::DebugCategory category : catalog::debugCategories()) {
        QMenu* categoryMenu = menu->addMenu(QString());
        categoryMenu->setProperty("debugCategory", static_cast<int>(category));
        m_debugCategoryMenus.append(categoryMenu);
        combo->insertSeparator(combo->count());
        for (quantiloom::DebugVisualizationMode mode : catalog::debugModesIn(category)) {
            addMode(categoryMenu, mode);
        }
    }

    connect(combo, &QComboBox::currentIndexChanged, this, [this, combo](int index) {
        if (index < 0) return;
        const QVariant itemData = combo->itemData(index);
        if (!itemData.isValid()) return;   // separator
        applyDebugMode(static_cast<quantiloom::DebugVisualizationMode>(itemData.toInt()));
    });

    m_debugActions.value(static_cast<int>(quantiloom::DebugVisualizationMode::None))
        ->setChecked(true);
}

// ============================================================================
// Toolbar
// ============================================================================

void MainWindow::setupToolBar() {
    m_mainToolBar = addToolBar(QString());
    m_mainToolBar->setObjectName(QStringLiteral("toolbar_main"));
    m_mainToolBar->setMovable(true);
    // Icon-only, but a QToolButton falls back to the action's text when the
    // action has no icon -- which is how the transform buttons read as G/R/T
    // without inventing glyphs for them. The icons that are used come from the
    // active style, so they stay legible under both light and dark themes
    // instead of being one fixed bitmap set.
    m_mainToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    QStyle* s = style();
    m_openAction->setIcon(s->standardIcon(QStyle::SP_DialogOpenButton));
    m_saveAction->setIcon(s->standardIcon(QStyle::SP_DialogSaveButton));
    m_undoAction->setIcon(s->standardIcon(QStyle::SP_ArrowBack));
    m_redoAction->setIcon(s->standardIcon(QStyle::SP_ArrowForward));
    m_startRenderAction->setIcon(s->standardIcon(QStyle::SP_MediaPlay));
    m_stopRenderAction->setIcon(s->standardIcon(QStyle::SP_MediaStop));
    m_resumeRenderAction->setIcon(s->standardIcon(QStyle::SP_MediaSeekForward));
    m_resetAccumulationAction->setIcon(s->standardIcon(QStyle::SP_BrowserReload));
    m_screenshotAction->setIcon(s->standardIcon(QStyle::SP_DialogSaveAllButton));

    m_mainToolBar->addAction(m_openAction);
    m_mainToolBar->addAction(m_saveAction);
    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_undoAction);
    m_mainToolBar->addAction(m_redoAction);
    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_translateAction);
    m_mainToolBar->addAction(m_rotateAction);
    m_mainToolBar->addAction(m_scaleAction);
    m_mainToolBar->addAction(m_localSpaceAction);
    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_startRenderAction);
    m_mainToolBar->addAction(m_resumeRenderAction);
    m_mainToolBar->addAction(m_stopRenderAction);
    m_mainToolBar->addAction(m_resetAccumulationAction);
    m_mainToolBar->addSeparator();

    m_spectralComboLabel = new QLabel(this);
    m_spectralCombo = new QComboBox(this);
    m_spectralCombo->setMinimumContentsLength(14);
    m_mainToolBar->addWidget(m_spectralComboLabel);
    m_mainToolBar->addWidget(m_spectralCombo);

    m_debugComboLabel = new QLabel(this);
    m_debugCombo = new QComboBox(this);
    m_debugCombo->setMinimumContentsLength(18);
    m_mainToolBar->addWidget(m_debugComboLabel);
    m_mainToolBar->addWidget(m_debugCombo);

    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_screenshotAction);

    // The submenus are filled here so that each mode is registered once, into
    // the menu action group and the combo box together.
    buildSpectralMenu(m_spectralMenu, m_spectralCombo);
    buildDebugMenu(m_debugMenu, m_debugCombo);

    m_panelsMenu->addSeparator();
    m_panelsMenu->addAction(m_mainToolBar->toggleViewAction());
}

// ============================================================================
// Single application points for the shared modes
// ============================================================================

void MainWindow::applyDebugMode(quantiloom::DebugVisualizationMode mode) {
    m_vulkanWindow->setDebugMode(mode);

    if (QAction* action = m_debugActions.value(static_cast<int>(mode), nullptr)) {
        action->setChecked(true);
    }
    if (m_debugCombo) {
        const QSignalBlocker blocker(m_debugCombo);
        m_debugCombo->setCurrentIndex(m_debugCombo->findData(static_cast<int>(mode)));
    }
    m_debugVisualizationPanel->setDebugMode(mode);
    m_viewportFrame->setDebugMode(mode);

    showStatusMessage(tr("Debug mode: %1").arg(catalog::debugModeName(mode)));
}

void MainWindow::applySpectralMode(quantiloom::SpectralMode mode) {
    // The illuminant notices are about this: a quantitative mode with no
    // spectrum renders black, ASTM G-173 must keep its absolute scale in one
    // and lose it in another, and a spectrum that stops at 4000 nm says nothing
    // about the thermal bands. The panel works all three out from the mode.
    if (m_lightingPanel) {
        m_lightingPanel->setSpectralMode(mode);
    }
    m_vulkanWindow->setSpectralMode(mode);

    // After the mode is in force, not before: re-picking loads the spectrum
    // again, and the core writes its coverage diagnostic against whatever mode
    // is current when it does. Deliberately here rather than inside the panel's
    // setter -- this is the *user* switching mode, which may move the setting,
    // and applyConfig's call to the same setter must not.
    if (m_lightingPanel) {
        m_lightingPanel->repickNormalisation();
    }

    if (QAction* action = m_spectralActions.value(static_cast<int>(mode), nullptr)) {
        action->setChecked(true);
    }
    if (m_spectralCombo) {
        const QSignalBlocker blocker(m_spectralCombo);
        m_spectralCombo->setCurrentIndex(m_spectralCombo->findData(static_cast<int>(mode)));
    }
    m_spectralConfigPanel->setSpectralMode(mode);
    m_viewportFrame->setSpectralMode(mode);

    // The sensitivity readout is quoted per band: the same detector resolves
    // ten times finer in the LWIR than in the MWIR at room temperature.
    if (const auto band = quantiloom::GetFusedBandInfo(mode); band.has_value()) {
        m_sensorPanel->setNetdBand(static_cast<double>(band->lambdaMinNm),
                                   static_cast<double>(band->lambdaMaxNm));
    }

    setSceneModified(true);
    showStatusMessage(tr("Spectral mode: %1").arg(catalog::spectralModeName(mode)));
}

void MainWindow::applyTargetSpp(uint32_t spp) {
    m_vulkanWindow->setSPP(spp);
    m_renderSettingsPanel->setTargetSPP(spp);

    if (m_qualityGroup) {
        for (QAction* action : m_qualityGroup->actions()) {
            action->setChecked(action->data().toUInt() == spp);
        }
    }

    setSceneModified(true);
    updateRenderProgress();

    // Raising the target restarts the render loop from the samples already
    // accumulated (setSPP does not reset), so this is the start of a new run
    // as far as the completion message and the ETA are concerned. Without
    // this, a render that reported "complete" at 16 reached 64 in silence.
    const uint32_t current = m_vulkanWindow->currentSampleCount();
    if (spp == 0 || current < spp) {
        beginRenderTiming();
        setRenderActionsRunning(!m_vulkanWindow->isRenderPaused());
    }

    if (spp == 0)
        showStatusMessage(tr("Target samples: infinite"));
    else
        showStatusMessage(tr("Target samples: %1").arg(spp));
}

void MainWindow::applyWavelength(float wavelength_nm) {
    m_vulkanWindow->setWavelength(wavelength_nm);
    m_spectralConfigPanel->setWavelength(wavelength_nm);

    setSceneModified(true);
    showStatusMessage(tr("Wavelength: %1 nm").arg(wavelength_nm, 0, 'f', 0));
}

void MainWindow::applyLightingParams(const quantiloom::LightingParams& params) {
    // Takes the whole struct, for a caller that has one -- the panels do not.
    // They each own half of it and edit their half in place, which is what
    // pushLightingParams() is for; this is the entry point for anything that
    // arrives with the finished article.
    if (&params != m_lightingParams.get()) {
        *m_lightingParams = params;
    }
    pushLightingParams();

    // Both halves of the struct are shown back, since the caller was neither
    // panel. The lighting panel is not written back from pushLightingParams()
    // because that path *is* the lighting panel: its azimuth and elevation come
    // back out of the direction vector through asin and atan2, and rounding
    // that to the slider's integer degrees mid-drag would fight the drag.
    m_lightingPanel->setLightingParams(*m_lightingParams);
    m_atmosphericPanel->setAnalyticSky(m_lightingParams->skyEmissivityClear > 0.0f,
                                       m_lightingParams->atmosphereTemperature_K,
                                       m_skyRelativeHumidity);
}

void MainWindow::applyEnvironmentMap(const QString& path, bool enabled) {
    // The enable flag rides on LightingParams and has already been pushed by
    // onLightingChanged(); what is left is loading a map the shell has not
    // loaded yet. Turning one off does not unload it -- the flag stops the
    // shader sampling it, and keeping it resident makes the checkbox instant.
    if (enabled && !path.isEmpty()) {
        if (!m_vulkanWindow->loadEnvironmentMap(path)) {
            showStatusMessage(tr("Failed to load environment map: %1")
                                  .arg(QFileInfo(path).fileName()));
        }
    }

    // Remember it for export, since no getter on the renderer carries the path.
    if (m_lastConfig) {
        m_lastConfig->environmentMap = path;
        m_lastConfig->environmentMapEnabled = enabled;
    }
    m_lightingPanel->setEnvironmentMap(path, enabled);
    setSceneModified(true);

    if (path.isEmpty()) {
        showStatusMessage(tr("Environment map cleared"));
    } else {
        showStatusMessage(enabled
            ? tr("Lighting from %1").arg(QFileInfo(path).fileName())
            : tr("Environment map off — it contributes no light"));
    }
}

void MainWindow::applyIlluminant(const LightingPanel::IlluminantChoice& choice) {
    m_illuminant = choice;
    m_lightingPanel->setIlluminant(choice);

    // "none" is the RGB fallback: there is nothing to load, and the LUT
    // already in the context stays until a scene reload clears it. Saying so
    // is the panel's warning, not an error here.
    if (choice.kind == QLatin1String("none")) {
        setSceneModified(true);
        showStatusMessage(tr("Illuminant: RGB radiance only"));
        return;
    }

    quantiloom::SolarLutSpec spec;
    spec.normaliseUnitLuminance = choice.normaliseUnitLuminance;
    QString baseDir;

    if (choice.kind == QLatin1String("equal_energy")) {
        spec.pathOrEqualEnergy = "equal_energy";
    } else if (choice.kind == QLatin1String("astm")) {
        // ASTM G-173's column 3 is global irradiance -- direct included -- so
        // using it as the diffuse sky would count the sun twice. The core
        // documents this layout; the panel should not have to know it.
        const QString bundled = resolveBundledIlluminant();
        if (bundled.isEmpty()) {
            QMessageBox::warning(this, tr("Illuminant Not Found"),
                tr("assets/luts/astmg173.csv was not found beside the application "
                   "or in the working directory."));
            return;
        }
        spec.pathOrEqualEnergy = QDir::toNativeSeparators(bundled).toStdString();
        spec.directColumn = 4;
        spec.diffuseColumn = 3;
        spec.diffuseIsGlobal = true;
    } else {
        if (choice.path.isEmpty()) {
            return;
        }
        spec.pathOrEqualEnergy = QDir::toNativeSeparators(choice.path).toStdString();
        baseDir = QFileInfo(choice.path).absolutePath();
    }

    if (const auto error = m_vulkanWindow->setSolarLutFromSpec(spec, baseDir)) {
        QMessageBox::warning(this, tr("Illuminant Failed"),
            tr("Could not load the illuminant:\n%1").arg(*error));
        return;
    }

    // The core derived sun and sky RGB from the spectrum so that both halves
    // of the renderer describe one sun; take that back into the shell's copy
    // and the panel, or they would keep showing the values it replaced.
    *m_lightingParams = m_vulkanWindow->lightingParams();
    m_lightingPanel->setLightingParams(*m_lightingParams);

    setSceneModified(true);
    showStatusMessage(tr("Illuminant loaded"));
}

QString MainWindow::resolveBundledIlluminant() {
    // Same candidate order as every other bundled asset.
    const QStringList candidates{
        QDir::currentPath() + QStringLiteral("/assets/luts/astmg173.csv"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/assets/luts/astmg173.csv"),
    };
    for (const QString& path : candidates) {
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    return {};
}

void MainWindow::applyAtmosphere(const quantiloom::AtmosphereNNConfig& config) {
    m_vulkanWindow->setAtmosphericConfig(config);
    m_atmosphericPanel->setAtmosphericConfig(config);
    setSceneModified(true);
}

void MainWindow::applySensorEnabled(bool enabled) {
    m_vulkanWindow->setSensorEnabled(enabled);
    m_sensorPanel->setSensorEnabled(enabled);
    setSceneModified(true);
    showStatusMessage(enabled ? tr("Sensor simulation enabled")
                              : tr("Sensor simulation disabled"));
}

void MainWindow::applySensorParams(const quantiloom::SensorParams& params) {
    m_vulkanWindow->setSensorParams(params);
    m_sensorPanel->setSensorParams(params);
    setSceneModified(true);
    showStatusMessage(tr("Sensor parameters updated"));
}

void MainWindow::applyThermographyParams(bool enabled,
                                         const quantiloom::ThermographyParams& params) {
    m_thermographyEnabled = enabled;
    m_thermographyParams = params;
    m_vulkanWindow->setThermographyParams(params);
    setSceneModified(true);
}

// ---------------------------------------------------------------------------
// The thermal solve
// ---------------------------------------------------------------------------
// Three dispatchers and one material push, all of which end by reading the
// status back out of the renderer. The panel shows what is running rather than
// what was asked for, which matters most when the answer is "it did not run" --
// a scene with no thermal materials, or a solve that fell back to the CPU.

quantiloom::ThermalSolveParams MainWindow::currentThermalParams() const {
    quantiloom::ThermalSolveParams params;
    params.startTime_h = m_thermalStartTimeH;
    params.timestep_s = m_thermalTimestepS;
    params.layerCount = static_cast<quantiloom::u32>(m_thermalLayers);
    params.initial = m_thermalInitial == QLatin1String("steady")
                         ? quantiloom::ThermalInitialCondition::Steady
                         : quantiloom::ThermalInitialCondition::Uniform;
    params.initialTemperature_K = m_thermalInitialTemperatureK;
    params.exchangeRays = static_cast<quantiloom::u32>(m_thermalExchangeRays);
    params.exchangeTopK = static_cast<quantiloom::u32>(m_thermalExchangeTopK);
    params.sunIrradiance_W_m2 = m_thermalSunIrradiance;
    params.diffuseIrradiance_W_m2 = m_thermalDiffuseIrradiance;
    params.checkpointStride_h = m_thermalCheckpointStrideH;
    params.forcingFile = m_thermalForcingFile.toStdString();
    params.sunCorrection = m_thermalSunCorrection;
    params.dumpElementsFile = m_thermalDumpElements.toStdString();
    params.convectionModel = m_thermalConvectionModel;
    params.convectionWindA_W_m2K = m_thermalConvectionWindA;
    params.convectionWindB_W_s_m3K = m_thermalConvectionWindB;
    params.convectionFreeC = m_thermalConvectionFreeC;
    params.convectionReferenceHeight_m = m_thermalConvectionReferenceHeightM;
    params.convectionStableDamping = m_thermalConvectionStableDamping;
    params.lateralConduction = m_thermalLateralConduction;
    params.sunMemoryLags = static_cast<quantiloom::u32>(m_thermalSunMemoryLags);
    params.parameterSensitivities = m_thermalParameterSensitivities;
    // The air the surfaces convect with and the humidity a wet one evaporates
    // into belong to the atmosphere rather than to the thermal panel. Studio
    // carries them rather than deciding them: they are seeded from the
    // document and only move when the atmosphere panel moves them, so building
    // a params struct here cannot quietly reset the air to a default.
    const quantiloom::ThermalSolveParams carried = m_thermalPanel->params();
    params.airTemperature_K = carried.airTemperature_K;
    params.relativeHumidity = carried.relativeHumidity;

    // The sky they radiate against is not carried, because it is not an
    // independent number: it is what the clear-sky emissivity implies for that
    // air, the same derivation the SDK does when it reads a config. Carrying a
    // default here instead would send 268 K over the top of it, and a surface
    // under a sky six degrees too cold runs about two degrees too cool.
    const float emissivity = m_lightingParams ? m_lightingParams->skyEmissivityClear : 0.0f;
    params.skyTemperature_K =
        emissivity > 0.0f
            ? quantiloom::EffectiveSkyTemperatureK(params.airTemperature_K,
                                                   static_cast<double>(emissivity))
            : params.airTemperature_K;
    return params;
}

void MainWindow::applyThermalEnabled(bool enabled) {
    m_thermalEnabled = enabled;
    m_vulkanWindow->setThermalSolveEnabled(enabled);
    if (enabled) {
        m_vulkanWindow->setThermalTime(m_thermalTimeH);
    }
    {
        const QSignalBlocker block(m_thermalPanel);
        m_thermalPanel->setSolveEnabled(enabled);
    }
    m_thermalPanel->updateStatus(m_vulkanWindow->thermalSolveStatus());
    // A probe of a solve that is off is a chart of nothing. Cleared rather
    // than left standing, because a stale curve beside a switched-off solve is
    // the kind of thing a reader believes.
    if (!enabled) {
        m_thermalPanel->clearProbe(tr("The thermal solve is off."));
    }
    setSceneModified(true);
}

namespace {

/// The convection law, by the spelling a config writes. Unknown falls back to
/// the constant law rather than refusing the document: the SDK warns about the
/// same string and does the same thing, and two readings of one key that
/// disagreed would be worse than either.
quantiloom::ThermalConvectionModel convectionModelFromName(const QString& name) {
    if (name == QLatin1String("wind")) return quantiloom::ThermalConvectionModel::Wind;
    if (name == QLatin1String("stability")) return quantiloom::ThermalConvectionModel::Stability;
    return quantiloom::ThermalConvectionModel::Constant;
}

QString convectionModelName(quantiloom::ThermalConvectionModel model) {
    switch (model) {
        case quantiloom::ThermalConvectionModel::Wind:      return QStringLiteral("wind");
        case quantiloom::ThermalConvectionModel::Stability: return QStringLiteral("stability");
        case quantiloom::ThermalConvectionModel::Constant:  break;
    }
    return QStringLiteral("constant");
}

/// The five material parameters a trajectory can be differentiated with
/// respect to, by the names thermal.parameter_sensitivities uses. A name this
/// build does not know is dropped, which is what the SDK does with it too.
quantiloom::Vector<quantiloom::ThermalSensitivityParameter>
sensitivitiesFromNames(const QStringList& names) {
    quantiloom::Vector<quantiloom::ThermalSensitivityParameter> out;
    for (const QString& name : names) {
        if (name == QLatin1String("h")) {
            out.push_back(quantiloom::ThermalSensitivityParameter::Convection);
        } else if (name == QLatin1String("epsilon")) {
            out.push_back(quantiloom::ThermalSensitivityParameter::Emissivity);
        } else if (name == QLatin1String("alpha")) {
            out.push_back(quantiloom::ThermalSensitivityParameter::Absorptivity);
        } else if (name == QLatin1String("k")) {
            out.push_back(quantiloom::ThermalSensitivityParameter::Conductivity);
        } else if (name == QLatin1String("rhoc")) {
            out.push_back(quantiloom::ThermalSensitivityParameter::HeatCapacity);
        }
    }
    return out;
}

QStringList sensitivityNames(
    const quantiloom::Vector<quantiloom::ThermalSensitivityParameter>& parameters) {
    QStringList out;
    for (const auto p : parameters) {
        switch (p) {
            case quantiloom::ThermalSensitivityParameter::Convection:
                out << QStringLiteral("h"); break;
            case quantiloom::ThermalSensitivityParameter::Emissivity:
                out << QStringLiteral("epsilon"); break;
            case quantiloom::ThermalSensitivityParameter::Absorptivity:
                out << QStringLiteral("alpha"); break;
            case quantiloom::ThermalSensitivityParameter::Conductivity:
                out << QStringLiteral("k"); break;
            case quantiloom::ThermalSensitivityParameter::HeatCapacity:
                out << QStringLiteral("rhoc"); break;
        }
    }
    return out;
}

}  // namespace

void MainWindow::applyThermalParams(const quantiloom::ThermalSolveParams& incoming) {
    // The panel builds a fresh struct from its widgets, so anything it does not
    // show or carry would arrive at its default -- which for sunCorrection was
    // true, silently undoing a config that asked for the uncorrected field the
    // moment a user nudged the timestep. It has a checkbox now and carries the
    // rest, so what arrives here is the whole set and this end records it
    // instead of overriding it. The dump path is the exception: no widget owns
    // it, so this window remains its carrier.
    quantiloom::ThermalSolveParams params = incoming;
    params.dumpElementsFile = m_thermalDumpElements.toStdString();

    m_thermalSunCorrection = params.sunCorrection;
    m_thermalConvectionModel = params.convectionModel;
    m_thermalConvectionWindA = params.convectionWindA_W_m2K;
    m_thermalConvectionWindB = params.convectionWindB_W_s_m3K;
    m_thermalConvectionFreeC = params.convectionFreeC;
    m_thermalConvectionReferenceHeightM = params.convectionReferenceHeight_m;
    m_thermalConvectionStableDamping = params.convectionStableDamping;
    m_thermalLateralConduction = params.lateralConduction;
    m_thermalSunMemoryLags = static_cast<int>(params.sunMemoryLags);
    m_thermalParameterSensitivities = params.parameterSensitivities;

    m_thermalStartTimeH = params.startTime_h;
    m_thermalTimestepS = params.timestep_s;
    m_thermalLayers = static_cast<int>(params.layerCount);
    m_thermalInitial = params.initial == quantiloom::ThermalInitialCondition::Steady
                           ? QStringLiteral("steady")
                           : QStringLiteral("uniform");
    m_thermalInitialTemperatureK = params.initialTemperature_K;
    m_thermalSunIrradiance = params.sunIrradiance_W_m2;
    m_thermalDiffuseIrradiance = params.diffuseIrradiance_W_m2;
    m_thermalExchangeRays = static_cast<int>(params.exchangeRays);
    m_thermalExchangeTopK = static_cast<int>(params.exchangeTopK);
    m_thermalCheckpointStrideH = params.checkpointStride_h;
    m_thermalForcingFile = QString::fromStdString(params.forcingFile);

    m_vulkanWindow->setThermalSolveParams(params);
    {
        const QSignalBlocker block(m_thermalPanel);
        m_thermalPanel->setParams(params);
    }
    if (m_thermalEnabled) {
        m_vulkanWindow->setThermalTime(m_thermalTimeH);
    }
    m_thermalPanel->updateStatus(m_vulkanWindow->thermalSolveStatus());
    setSceneModified(true);
}

void MainWindow::applyThermalWhatIf(
    const quantiloom::ThermalSensitivityParameter parameter, const double fraction) {
    if (!m_thermalEnabled) {
        return;
    }

    // The step is a fraction of the parameter, and the parameter's own value
    // lives on a material rather than here. A representative one is enough for
    // a preview: the tangent is per element, so a scene with two materials
    // gets the right shape and a step scaled by one of them. Which is the
    // honest limit of one slider for a whole scene, and the caption says the
    // preview is a linearisation anyway.
    double magnitude = 0.0;
    switch (parameter) {
        case quantiloom::ThermalSensitivityParameter::Convection:   magnitude = 10.0; break;
        case quantiloom::ThermalSensitivityParameter::Emissivity:   magnitude = 0.9; break;
        case quantiloom::ThermalSensitivityParameter::Absorptivity: magnitude = 0.7; break;
        case quantiloom::ThermalSensitivityParameter::Conductivity: magnitude = 1.4; break;
        case quantiloom::ThermalSensitivityParameter::HeatCapacity: magnitude = 2.0e6; break;
    }
    const double step = fraction * magnitude;

    // Asking for a derivative the solve does not carry is a solve change, and
    // it rebuilds the trajectory -- so it happens once, here, when the user
    // first asks about that parameter, and every later move of the slider is
    // the preview alone.
    if (step != 0.0 &&
        std::find(m_thermalParameterSensitivities.begin(),
                  m_thermalParameterSensitivities.end(),
                  parameter) == m_thermalParameterSensitivities.end()) {
        m_thermalParameterSensitivities.push_back(parameter);
        applyThermalParams(currentThermalParams());
    }

    const auto applied = m_vulkanWindow->setThermalWhatIf(parameter, step);
    if (!applied.has_value()) {
        showStatusMessage(QString::fromStdString(applied.error()));
    }
}

void MainWindow::applyThermalTime(double time_h) {
    m_thermalTimeH = time_h;
    if (m_thermalEnabled) {
        m_vulkanWindow->setThermalTime(time_h);
    }
    {
        const QSignalBlocker block(m_thermalPanel);
        m_thermalPanel->setTime(time_h);
    }
    m_thermalPanel->updateStatus(m_vulkanWindow->thermalSolveStatus());
    setSceneModified(true);
}

void MainWindow::applyThermalMaterial(const QString& name,
                                      const MaterialThermalProps& props) {
    quantiloom::ThermalMaterialParams mp;
    mp.conductivity_W_mK = props.conductivity;
    mp.density_kg_m3 = props.density;
    mp.specificHeat_J_kgK = props.specificHeat;
    mp.thickness_m = props.thickness;
    mp.convection_W_m2K = props.convection;
    mp.shortwaveAbsorptivity = props.shortwaveAbsorptivity;
    mp.wetnessFactor = props.wetness;
    mp.internalHeat_W_m2 = props.internalHeat;
    // Three boundaries, two flags: pinned wins, then convecting to the
    // interior, then insulated. A panel over a bay rather than a wall is the
    // "ambient" one, and it is the only one that reads the coefficient below.
    mp.interiorFixedTemperature = props.interiorBoundary == QLatin1String("fixed");
    mp.interiorAmbient = props.interiorBoundary == QLatin1String("ambient");
    mp.interiorTemperature_K = props.interiorTemperature;
    mp.interiorConvection_W_m2K = props.interiorConvection;
    m_vulkanWindow->setThermalMaterial(name, mp);

    // The trajectory is rebuilt from the new properties, so the viewport has to
    // be replayed to the hour it is showing. A material edit is a rare enough
    // gesture to pay for that.
    if (m_thermalEnabled) {
        m_vulkanWindow->setThermalTime(m_thermalTimeH);
        m_thermalPanel->updateStatus(m_vulkanWindow->thermalSolveStatus());
    }
}

void MainWindow::applyDisplayEnhancement(
    const quantiloom::DisplayEnhancementParams& params) {
    m_displayParams = params;

    m_vulkanWindow->setDisplayEnhancement(params);
    m_displayEnhancementPanel->setParams(params);

    if (m_displayEnhancementAction) {
        const QSignalBlocker blocker(m_displayEnhancementAction);
        m_displayEnhancementAction->setChecked(params.enabled);
    }

    // Session state, not the document -- see src/config/CLAUDE.md. No
    // setSceneModified() here, deliberately.
    if (!params.enabled) {
        showStatusMessage(tr("Display enhancement off"));
        return;
    }

    QString tone;
    switch (params.toneMode) {
        case quantiloom::DisplayToneMode::Linear:   tone = tr("linear"); break;
        case quantiloom::DisplayToneMode::Equalize: tone = tr("equalized"); break;
        case quantiloom::DisplayToneMode::Clahe:    tone = tr("CLAHE"); break;
    }
    QString palette;
    switch (params.palette) {
        case quantiloom::DisplayPalette::Grey:         palette = tr("greyscale"); break;
        case quantiloom::DisplayPalette::GreyInverted: palette = tr("inverted"); break;
        case quantiloom::DisplayPalette::Ironbow:      palette = tr("ironbow"); break;
        case quantiloom::DisplayPalette::Rainbow:      palette = tr("rainbow"); break;
        case quantiloom::DisplayPalette::Viridis:      palette = tr("viridis"); break;
    }
    showStatusMessage(tr("Display enhancement on (%1, %2, %3–%4%)")
                          .arg(tone, palette)
                          .arg(params.percentileLow, 0, 'f', 1)
                          .arg(params.percentileHigh, 0, 'f', 1));
}

void MainWindow::applyCameraPose(const glm::vec3& position, const glm::vec3& target) {
    glm::vec3 currentPosition;
    glm::vec3 currentTarget;
    glm::vec3 up;
    float fovY = 45.0f;
    m_vulkanWindow->getCameraState(currentPosition, currentTarget, up, fovY);
    m_vulkanWindow->setCamera(position, target, up, fovY);
    // No write-back: the renderer emits cameraChanged() when it adopts a pose,
    // and onCameraChanged() is the single point that shows it.
    setSceneModified(true);
}

void MainWindow::applyCameraFov(float fovYDegrees) {
    m_vulkanWindow->setCameraFovY(fovYDegrees);
    setSceneModified(true);
}

void MainWindow::applyMaterial(int index, const quantiloom::Material& material) {
    const auto* scene = m_vulkanWindow->getScene();
    if (!scene || index < 0 || static_cast<size_t>(index) >= scene->materials.size()) {
        return;
    }

    const quantiloom::Material previous = scene->materials[static_cast<size_t>(index)];

    // Binding a lamp's spectrum is not an assignment, so it cannot ride along
    // inside a Material the way a colour does: the curve has to be loaded,
    // resampled onto the band being rendered, levelled against this material's
    // own emissive triple, uploaded, and then that triple REPLACED by the
    // colour the spectrum integrates to. The core owns all of that -- doing any
    // of it here would be a second reading of the same rule, and the viewport
    // and the CLI would end up on different lamps.
    //
    // Done before the undo command is built so the command records the material
    // the core actually produced, colour and curve index included, rather than
    // the half-set one the panel emitted.
    quantiloom::Material applied = material;
    if (material.emissiveCurveSource != previous.emissiveCurveSource) {
        const QString baseDir =
            m_currentConfigFile.isEmpty() ? QString()
                                          : QFileInfo(m_currentConfigFile).absolutePath();
        auto bound = m_vulkanWindow->setMaterialEmissionSpectrum(
            static_cast<uint32_t>(index), material.emissiveCurveSource, baseDir);
        if (!bound) {
            QMessageBox::warning(
                this, tr("Emission spectrum"),
                tr("Could not bind '%1':\n\n%2")
                    .arg(QString::fromStdString(material.emissiveCurveSource),
                         QString::fromStdString(bound.error())));
            return;
        }
        // Not errors. Emission is zero outside a curve's measured span rather
        // than held flat, so the commonest one says this lamp will be dark in
        // the band on screen -- which the user needs to see now, not infer from
        // a black image.
        if (!bound.value().empty()) {
            showStatusMessage(QString::fromStdString(bound.value().front()));
        }
        // Re-read what the core produced. It rewrote emissiveFactor from the
        // spectrum and assigned the curve index, and the undo entry has to
        // record that rather than the half-set material the panel emitted --
        // otherwise undo restores a lamp with a colour that never existed.
        if (const auto* refreshed = m_vulkanWindow->getScene()) {
            applied = refreshed->materials[static_cast<size_t>(index)];
        }
    }

    // ModifyMaterialCommand has existed since the undo stack was written and
    // was never pushed, so every material edit was silently outside the
    // history -- Ctrl+Z after changing a colour undid whatever move came
    // before it instead.
    auto command = std::make_unique<ModifyMaterialCommand>(
        m_vulkanWindow, index, previous, applied);
    command->execute();
    m_undoStack->push(std::move(command));

    m_editedMaterials.insert(index);
    // Refresh the editor if it is already showing this material; do not switch
    // it to one the user did not pick.
    if (m_materialEditorPanel->currentMaterialIndex() == index) {
        m_materialEditorPanel->setMaterial(index, &scene->materials[static_cast<size_t>(index)]);
        // The thermal half comes from the config rather than the material,
        // which does not carry it.
        m_materialEditorPanel->setThermalProperties(
            m_thermalProperties.value(
                QString::fromStdString(scene->materials[static_cast<size_t>(index)].name)));
    }
    setSceneModified(true);
    showStatusMessage(tr("Material modified"));
}

void MainWindow::applyNodeTransform(int nodeIndex, const glm::mat4& transform) {
    const auto* scene = m_vulkanWindow->getScene();
    if (!scene || nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= scene->nodes.size()) {
        return;
    }

    const glm::mat4 previous = scene->nodes[static_cast<size_t>(nodeIndex)].transform;
    if (previous == transform) {
        return;
    }

    // Same command the gizmo pushes, so the undo history does not care which
    // way the node was moved.
    auto command = std::make_unique<TransformNodeCommand>(
        m_vulkanWindow, nodeIndex, previous, transform);
    command->execute();
    m_undoStack->push(std::move(command));

    m_editedNodes.insert(nodeIndex);
    // The properties panel shows the *selected* node; writing some other
    // node's transform into it would display a pose that belongs to nothing
    // on screen.
    if (m_selectionManager->primarySelection() == nodeIndex) {
        m_propertiesPanel->updateNodeTransform(transform);
    }
    setSceneModified(true);
}

// ============================================================================
// MCP
// ============================================================================

void MainWindow::setMcpServerRunning(bool running) {
    if (running == (m_mcpServer != nullptr)) {
        return;
    }

    if (!running) {
        m_mcpServer.reset();
        if (m_mcpPumpTimer) {
            m_mcpPumpTimer->stop();
        }
        updateMcpStatusLabel();
        showStatusMessage(tr("MCP server stopped"));
        return;
    }

    QSettings settings;
    const quantiloom::u16 port =
        static_cast<quantiloom::u16>(settings.value(kMcpPortKey, 8767).toUInt());

    quantiloom::mcp::ServerOptions options;
    options.port = port;
    options.serverName = "quantiloom-studio";
    options.serverVersion = QCoreApplication::applicationVersion().toStdString();

    // Called on the transport thread. Nothing here may touch the renderer or a
    // widget -- it posts, and pumpMcp() does the work on this thread.
    options.onCommandQueued = [this] {
        QMetaObject::invokeMethod(this, &MainWindow::pumpMcp, Qt::QueuedConnection);
    };

    // A tool that reloads the document runs here instead of inside Pump(),
    // because opening a scene pumps Qt events of its own while shaders compile
    // and doing that from within the pump would nest one inside the other.
    options.hostDispatch = [this](std::function<void()> work) {
        QMetaObject::invokeMethod(
            this, [work = std::move(work)]() mutable { work(); }, Qt::QueuedConnection);
    };

    auto created = quantiloom::mcp::Server::Create(options);
    if (!created.has_value()) {
        const QString reason = QString::fromStdString(created.error());
        QL_LOG_ERROR("MCP: {}", created.error());
        QMessageBox::warning(this, tr("MCP Server"),
                             tr("Could not start the MCP server.\n\n%1").arg(reason));
        if (m_mcpAction) {
            const QSignalBlocker blocker(m_mcpAction);
            m_mcpAction->setChecked(false);
        }
        return;
    }
    m_mcpServer = std::move(created.value());

    registerMcpTools();

    // The posted wake is the fast path; this is the backstop for anything it
    // could not deliver -- a wake that arrived while a long operation was in
    // progress, or while a pump was already running.
    if (!m_mcpPumpTimer) {
        m_mcpPumpTimer = new QTimer(this);
        m_mcpPumpTimer->setInterval(250);
        connect(m_mcpPumpTimer, &QTimer::timeout, this, &MainWindow::pumpMcp);
    }
    m_mcpPumpTimer->start();

    updateMcpStatusLabel();
    showStatusMessage(tr("MCP server on 127.0.0.1:%1").arg(m_mcpServer->Port()));
    QL_LOG_INFO("MCP: Studio serving on port {}", m_mcpServer->Port());
}

void MainWindow::startMcpServerFromCommandLine(quint16 port) {
    if (port != 0) {
        QSettings settings;
        settings.setValue(kMcpPortKey, port);
    }
    // Through the action, not through setMcpServerRunning() directly: the
    // menu entry has to end up ticked, and the action's toggled signal is what
    // does that.
    if (m_mcpAction) {
        m_mcpAction->setChecked(true);
    } else {
        setMcpServerRunning(true);
    }
}

void MainWindow::pumpMcp() {
    if (!m_mcpServer || m_mcpPumping || m_longOperationActive) {
        return;
    }
    m_mcpPumping = true;
    m_mcpServer->Pump();
    m_mcpPumping = false;
}

void MainWindow::updateMcpStatusLabel() {
    if (!m_mcpStatusLabel) {
        return;
    }
    if (m_mcpServer) {
        m_mcpStatusLabel->setText(tr("MCP :%1").arg(m_mcpServer->Port()));
        m_mcpStatusLabel->setVisible(true);
    } else {
        m_mcpStatusLabel->setVisible(false);
    }
    if (m_mcpAction) {
        const QSignalBlocker blocker(m_mcpAction);
        m_mcpAction->setChecked(m_mcpServer != nullptr);
    }
}

void MainWindow::buildThemeMenu(QMenu* menu) {
    m_themeGroup = new QActionGroup(this);
    m_themeGroup->setExclusive(true);

    for (const auto& theme : ThemeManager::availableThemes()) {
        QAction* action = menu->addAction(QString());
        action->setCheckable(true);
        action->setData(theme.id);
        m_themeGroup->addAction(action);
        m_themeActions.insert(theme.id, action);
        connect(action, &QAction::triggered, this, [this, id = theme.id]() {
            applyTheme(id);
        });
    }

    if (QAction* current = m_themeActions.value(ThemeManager::instance().currentThemeId())) {
        current->setChecked(true);
    }
}

void MainWindow::applyTheme(const QString& themeId) {
    // The single application point for the theme: the View menu entry and the
    // Preferences combo both land here, so the two cannot drift into doing
    // slightly different things.
    if (!ThemeManager::instance().switchTo(themeId)) {
        return;
    }
    if (QAction* action = m_themeActions.value(themeId)) {
        action->setChecked(true);
    }
}

void MainWindow::restyleUi() {
    m_styling.reapply();
}

void MainWindow::applyGridVisible(bool visible) {
    // Single application point for the grid overlay. Display-only: the scene
    // is unchanged, only what is composited over the frame -- so this is the
    // deliberate exception to src/vulkan/CLAUDE.md's "every setter resets
    // accumulation" rule, and toggling it must not restart convergence.
    //
    // What the renderer shows is the preference gated by the workspace: the
    // grid belongs to Layout, and a toggle made there survives a round trip
    // through the other workspaces.
    m_gridUserVisible = visible;
    const bool layoutActive =
        m_workspaces && m_workspaces->currentWorkspace() == QLatin1String("layout");
    m_vulkanWindow->setGridVisible(visible && layoutActive);
    if (m_showGridAction) {
        const QSignalBlocker blocker(m_showGridAction);
        m_showGridAction->setChecked(visible);
    }
    QSettings settings;
    settings.setValue(kShowGridKey, visible);
}

void MainWindow::applyMotionAdaptiveResolution(bool enabled) {
    // Single application point for the motion-adaptive render extent. Nothing
    // about the estimator changes either way -- this only decides whether a
    // camera or gizmo gesture is allowed to trace fewer pixels for its
    // duration -- so it is display-side, like the grid, and its own setter
    // deals with restoring full scale when it is turned off mid-gesture.
    m_motionAdaptiveResolution = enabled;
    m_vulkanWindow->setMotionAdaptiveResolution(enabled);
    if (m_motionAdaptiveResolutionAction) {
        const QSignalBlocker blocker(m_motionAdaptiveResolutionAction);
        m_motionAdaptiveResolutionAction->setChecked(enabled);
    }
    QSettings settings;
    settings.setValue(kMotionAdaptiveResolutionKey, enabled);
    refreshSampleRateLabel();
}

void MainWindow::applyWorkspaceEditingScope(const QString& workspaceId) {
    // Layout is the scene-editing workspace; the others are parameter rooms.
    // Grid, gizmo and the transform shortcuts exist in Layout and nowhere
    // else -- panels stay freely rearrangeable everywhere, but the editing
    // tools follow the workspace, not the panel set. Viewport click-to-select
    // stays active everywhere: picking an object to inspect its material or
    // its debug values is not a scene edit.
    const bool layout = workspaceId == QLatin1String("layout");

    m_vulkanWindow->setEditingToolsEnabled(layout);
    m_vulkanWindow->setGridVisible(m_gridUserVisible && layout);

    // The grid entry leaves the menu entirely outside Layout (and its
    // shortcut with it): showing a toggle that could not take effect would
    // misreport what the viewport is doing.
    if (m_showGridAction) {
        m_showGridAction->setVisible(layout);
        m_showGridAction->setEnabled(layout);
    }

    // Grey the whole transform vocabulary, shortcuts included -- a disabled
    // submenu entry alone would still let G/R/T fire
    if (m_transformMenu) {
        m_transformMenu->menuAction()->setEnabled(layout);
    }
    for (QAction* action : {m_translateAction, m_rotateAction, m_scaleAction,
                            m_localSpaceAction}) {
        if (action) {
            action->setEnabled(layout);
        }
    }

    // Paste, duplicate and delete change the scene, so they follow Layout
    // like the transform vocabulary. Copy stays live everywhere -- taking a
    // snapshot of a selection is inspection, not an edit.
    for (QAction* action : {m_pasteAction, m_duplicateAction, m_deleteAction}) {
        if (action) {
            action->setEnabled(layout);
        }
    }
}

void MainWindow::applyDisplayEnhancementEnabled(bool enabled) {
    // Route through the panel so the checkbox, the menu entry and the renderer
    // cannot disagree; the panel's own signal carries the current parameters.
    // The *reporting* setter, not the silent one -- the silent one left the
    // menu entry ticked and the renderer untouched.
    m_displayEnhancementPanel->requestEnhancementEnabled(enabled);
}

// ============================================================================
// Docks
// ============================================================================

QDockWidget* MainWindow::createPanelDock(PanelBase* panel, Qt::DockWidgetArea area) {
    auto* dock = new QDockWidget(this);
    // The object name is what QMainWindow::saveState keys the layout on, so it
    // has to be stable and language-independent -- hence the panel's id rather
    // than its title.
    dock->setObjectName(QStringLiteral("dock_") + panel->panelId());
    dock->setWidget(wrapScrollable(panel));
    dock->setWindowTitle(panel->panelTitle());
    addDockWidget(area, dock);

    // The panel names itself, so the dock title and the View ▸ Panels entry
    // follow its language automatically. This is the fix for panels whose
    // contents were fully translated while their tab label stayed English.
    connect(panel, &PanelBase::panelTitleChanged, dock, [dock, panel]() {
        dock->setWindowTitle(panel->panelTitle());
    });

    m_docks.insert(panel->panelId(), dock);
    m_panelsMenu->addAction(dock->toggleViewAction());
    return dock;
}

void MainWindow::setupDockWidgets() {
    // Create panel instances
    m_sceneTreePanel = new SceneTreePanel();
    m_materialEditorPanel = new MaterialEditorPanel();
    m_propertiesPanel = new PropertiesPanel(m_materialEditorPanel);
    m_cameraPanel = new CameraPanel();
    m_lightingPanel = new LightingPanel();
    m_renderSettingsPanel = new RenderSettingsPanel();
    m_spectralConfigPanel = new SpectralConfigPanel();
    m_spectralLibraryPanel = new SpectralLibraryPanel();
    m_debugVisualizationPanel = new DebugVisualizationPanel();
    m_atmosphericPanel = new AtmosphericPanel();
    m_sensorPanel = new SensorPanel();
    m_displayEnhancementPanel = new DisplayEnhancementPanel();
    m_thermalPanel = new ThermalPanel();
    m_spectralMaterialGenPanel = new SpectralMaterialGenPanel();
    m_comparisonPanel = new ComparisonPanel();

    // Display enhancement is one checkbox and three parameters; it is a group
    // inside the render panel rather than a top-level dock of its own.
    m_renderSettingsPanel->setDisplayEnhancementWidget(m_displayEnhancementPanel);

    createPanelDock(m_sceneTreePanel, Qt::LeftDockWidgetArea);
    createPanelDock(m_cameraPanel, Qt::LeftDockWidgetArea);
    createPanelDock(m_propertiesPanel, Qt::RightDockWidgetArea);
    createPanelDock(m_lightingPanel, Qt::RightDockWidgetArea);
    createPanelDock(m_atmosphericPanel, Qt::RightDockWidgetArea);
    createPanelDock(m_sensorPanel, Qt::RightDockWidgetArea);
    createPanelDock(m_renderSettingsPanel, Qt::RightDockWidgetArea);
    createPanelDock(m_spectralConfigPanel, Qt::RightDockWidgetArea);
    createPanelDock(m_spectralLibraryPanel, Qt::RightDockWidgetArea);
    createPanelDock(m_debugVisualizationPanel, Qt::RightDockWidgetArea);
    createPanelDock(m_thermalPanel, Qt::RightDockWidgetArea);
    createPanelDock(m_comparisonPanel, Qt::RightDockWidgetArea);

    // The generator is a full offline workflow -- table, chart, file import and
    // export -- and wants width. It lives as a floating tool window opened from
    // the Tools menu rather than as a docked panel: along the bottom edge it
    // spent every session taking vertical space from the panels it is used
    // beside. Floating a dock is also what its old hand-rolled "Detach" button
    // was imitating, so nothing is lost. It can still be docked by dragging.
    QDockWidget* generatorDock =
        createPanelDock(m_spectralMaterialGenPanel, Qt::BottomDockWidgetArea);
    generatorDock->setFloating(true);
    generatorDock->resize(920, 620);
    generatorDock->hide();

    // Connect panel signals
    connect(m_sceneTreePanel, &SceneTreePanel::nodeSelected,
            this, [this](int nodeIndex) {
                // Sync with selection manager
                bool addToSelection = QGuiApplication::keyboardModifiers() & Qt::ControlModifier;
                m_selectionManager->select(nodeIndex, addToSelection);
            });
    connect(m_sceneTreePanel, &SceneTreePanel::nodesSelected,
            this, [this](const QSet<int>& nodes) {
                // A multi-selection made in the tree replaces the current one
                // wholesale; the tree's own Ctrl/Shift handling has already
                // decided what it should be.
                m_selectionManager->selectMultiple(nodes);
            });
    connect(m_sceneTreePanel, &SceneTreePanel::contextMenuRequested,
            this, [this](const QPoint& globalPos) {
                // The same actions as the Edit menu, so there is one
                // definition of what each does and the shortcuts show through.
                QMenu menu(this);
                menu.addAction(m_frameSelectedAction);
                menu.addSeparator();
                menu.addAction(m_copyAction);
                menu.addAction(m_pasteAction);
                menu.addAction(m_duplicateAction);
                menu.addAction(m_deleteAction);
                menu.exec(globalPos);
            });
    connect(m_sceneTreePanel, &SceneTreePanel::materialSelected,
            this, &MainWindow::onMaterialSelected);

    connect(m_spectralLibraryPanel, &SpectralLibraryPanel::previewRequested,
            this, &MainWindow::onSpectralPreviewRequested);
    connect(m_spectralLibraryPanel, &SpectralLibraryPanel::assignRequested,
            this, &MainWindow::onSpectralMaterialAssigned);
    connect(m_spectralLibraryPanel, &SpectralLibraryPanel::endmemberAddRequested,
            this, &MainWindow::onSpectralEndmemberAdded);
    connect(m_spectralLibraryPanel, &SpectralLibraryPanel::endmemberRemoveRequested,
            this, &MainWindow::onSpectralEndmemberRemoved);
    // The menu entries act on whatever row is highlighted, so their enabled
    // state has to follow the highlight as well as the selection.
    connect(m_spectralLibraryPanel, &SpectralLibraryPanel::previewRequested,
            this, [this](const QString&, const QString&, const QString&) {
                updateSpectralMenuState();
            });

    connect(m_materialEditorPanel, &MaterialEditorPanel::materialChanged,
            this, &MainWindow::onMaterialChanged);

    // Typed transforms take the same route as a gizmo drag, down to the undo
    // command, so Ctrl+Z means one thing regardless of how the node was moved.
    connect(m_propertiesPanel, &PropertiesPanel::nodeTransformEdited,
            this, &MainWindow::onNodeTransformEdited);
    connect(m_propertiesPanel, &PropertiesPanel::materialRequested,
            this, &MainWindow::onMaterialSelected);

    connect(m_cameraPanel, &CameraPanel::cameraEdited,
            this, &MainWindow::applyCameraPose);
    connect(m_cameraPanel, &CameraPanel::fovEdited, this, &MainWindow::applyCameraFov);
    connect(m_cameraPanel, &CameraPanel::resetRequested, this, &MainWindow::onResetCamera);
    connect(m_cameraPanel, &CameraPanel::viewDirectionRequested,
            this, [this](const glm::vec3& direction) {
                m_vulkanWindow->setViewDirection(direction);
            });

    connect(m_spectralMaterialGenPanel, &SpectralMaterialGenPanel::materialChanged,
            this, &MainWindow::onMaterialChanged);
    connect(m_spectralMaterialGenPanel, &SpectralMaterialGenPanel::materialWithCriChanged,
            this, &MainWindow::onMaterialWithCriChanged);

    connect(m_lightingPanel, &LightingPanel::lightingChanged,
            this, &MainWindow::onLightingChanged);
    connect(m_lightingPanel, &LightingPanel::environmentMapChanged,
            this, &MainWindow::onEnvironmentMapChanged);
    connect(m_lightingPanel, &LightingPanel::illuminantChanged,
            this, &MainWindow::applyIlluminant);

    // A slider drag applies live but enters the history once, as one entry
    // from where the drag started to where it ended. Snapshot on press,
    // push on release.
    connect(m_lightingPanel, &LightingPanel::editGestureStarted,
            this, [this] {
                m_lightingBeforeGesture =
                    std::make_unique<quantiloom::LightingParams>(*m_lightingParams);
            });
    connect(m_lightingPanel, &LightingPanel::editGestureFinished,
            this, [this] {
                if (!m_lightingBeforeGesture) {
                    return;   // release without a press: nothing was dragged
                }
                pushSettingCommand(CommandId::ModifyLighting, tr("Lighting"),
                                   *m_lightingBeforeGesture, *m_lightingParams,
                                   [this](const quantiloom::LightingParams& v) {
                                       applyLightingParams(v);
                                   });
                m_lightingBeforeGesture.reset();
            });

    connect(m_renderSettingsPanel, &RenderSettingsPanel::sppChanged,
            this, &MainWindow::onSppChanged);
    connect(m_renderSettingsPanel, &RenderSettingsPanel::resetAccumulationRequested,
            this, &MainWindow::onResetAccumulation);
    // The panel's export button used to raise a save dialog and then do
    // nothing at all -- its signal had no receiver anywhere. It now runs the
    // one export path, the same one File ▸ Export Image uses.
    connect(m_renderSettingsPanel, &RenderSettingsPanel::exportRequested,
            this, &MainWindow::onExportImage);

    // Each of these routes through its dispatcher as before; the difference is
    // that the change is now recorded, so Ctrl+Z no longer skips silently past
    // a spectral mode or a sensor setting to whatever transform came before it.
    connect(m_spectralConfigPanel, &SpectralConfigPanel::spectralModeChanged,
            this, [this](quantiloom::SpectralMode mode) {
                pushSettingCommand(CommandId::ModifySpectralMode, tr("Spectral mode"),
                                   m_vulkanWindow->spectralMode(), mode,
                                   [this](const quantiloom::SpectralMode& v) {
                                       applySpectralMode(v);
                                   });
            });
    connect(m_spectralConfigPanel, &SpectralConfigPanel::wavelengthChanged,
            this, [this](float wavelength_nm) {
                pushSettingCommand(CommandId::ModifyWavelength, tr("Wavelength"),
                                   m_vulkanWindow->wavelength(), wavelength_nm,
                                   [this](const float& v) { applyWavelength(v); });
            });

    connect(m_debugVisualizationPanel, &DebugVisualizationPanel::debugModeChanged,
            this, &MainWindow::onDebugModeChanged);

    // Atmospheric panel signals
    connect(m_atmosphericPanel, &AtmosphericPanel::presetChanged,
            this, [this](const QString& preset) {
                showStatusMessage(tr("Atmospheric preset: %1").arg(preset));
            });
    connect(m_atmosphericPanel, &AtmosphericPanel::configChanged,
            this, [this](const quantiloom::AtmosphereNNConfig& config) {
                pushSettingCommand(CommandId::ModifyAtmosphere, tr("Atmosphere"),
                                   m_vulkanWindow->atmosphericConfig(), config,
                                   [this](const quantiloom::AtmosphereNNConfig& v) {
                                       applyAtmosphere(v);
                                   });
            });
    // The analytic sky moved here from the lighting panel; it still belongs to
    // LightingParams, so it is merged into the copy the shell holds rather
    // than sent on its own. The emissivity comes from the SDK's correlation --
    // the panel shows the number, it does not compute it.
    connect(m_atmosphericPanel, &AtmosphericPanel::analyticSkyChanged,
            this, [this](bool clearSky, float airTemperatureK, float relativeHumidity) {
                m_skyClearModel = clearSky;
                m_skyRelativeHumidity = relativeHumidity;
                m_lightingParams->atmosphereTemperature_K = airTemperatureK;
                m_lightingParams->skyEmissivityClear =
                    clearSky ? static_cast<float>(quantiloom::ClearSkyEmissivity(
                                   quantiloom::DewPointC(
                                       static_cast<double>(airTemperatureK) - 273.15,
                                       static_cast<double>(relativeHumidity))))
                             : 0.0f;
                pushLightingParams();
                // The same air the surfaces convect with, and the same
                // humidity a wet one evaporates into. This is the one place
                // Studio decides them rather than carrying them: the user just
                // typed them.
                if (m_thermalEnabled) {
                    quantiloom::ThermalSolveParams thermal = currentThermalParams();
                    thermal.airTemperature_K = static_cast<double>(airTemperatureK);
                    thermal.relativeHumidity = static_cast<double>(relativeHumidity);
                    applyThermalParams(thermal);
                }
            });

    // Sensor panel signals
    connect(m_sensorPanel, &SensorPanel::enabledChanged,
            this, [this](bool enabled) {
                pushSettingCommand(CommandId::ModifySensor, tr("Sensor simulation"),
                                   m_vulkanWindow->sensorEnabled(), enabled,
                                   [this](const bool& v) { applySensorEnabled(v); });
            });
    connect(m_sensorPanel, &SensorPanel::paramsChanged,
            this, [this](const quantiloom::SensorParams& params) {
                pushSettingCommand(CommandId::ModifySensor, tr("Sensor parameters"),
                                   m_vulkanWindow->sensorParams(), params,
                                   [this](const quantiloom::SensorParams& v) {
                                       applySensorParams(v);
                                   });
            });

    // Thermography: what the camera is told, which changes what a measurement
    // is reported as and never what is measured. Not an undoable scene edit
    // for the same reason CLAHE is not -- it is a reading of the render.
    connect(m_sensorPanel, &SensorPanel::thermographyChanged,
            this, &MainWindow::applyThermographyParams);

    // Thermal properties do not live on the Material, so they cannot ride the
    // materialChanged path with everything else: they are held here, by name,
    // written into the config on save, and pushed into the running solve.
    connect(m_materialEditorPanel, &MaterialEditorPanel::thermalPropertiesChanged,
            this, [this](int index, const MaterialThermalProps& props) {
                const auto* scene = m_vulkanWindow->getScene();
                if (!scene || index < 0 ||
                    static_cast<size_t>(index) >= scene->materials.size()) {
                    return;
                }
                const QString name = QString::fromStdString(scene->materials[index].name);
                m_thermalProperties[name] = props;
                m_editedMaterials.insert(index);
                applyThermalMaterial(name, props);
                setSceneModified(true);
            });

    // Thermal solve panel signals
    connect(m_thermalPanel, &ThermalPanel::thermalEnabledChanged,
            this, &MainWindow::applyThermalEnabled);
    connect(m_thermalPanel, &ThermalPanel::thermalParamsChanged,
            this, &MainWindow::applyThermalParams);
    connect(m_thermalPanel, &ThermalPanel::thermalTimeChanged,
            this, &MainWindow::applyThermalTime);
    connect(m_thermalPanel, &ThermalPanel::whatIfChanged,
            this, &MainWindow::applyThermalWhatIf);

    // The panel asks for a frame rather than reading one every time it wants
    // to: a path-traced image is still moving while it accumulates, and
    // statistics against a moving image are statistics about the noise.
    connect(m_comparisonPanel, &ComparisonPanel::frameRequested, this, [this] {
        auto frame = m_vulkanWindow->captureScreenshot();
        if (!frame) {
            showStatusMessage(tr("Could not read the current frame."));
            return;
        }
        m_comparisonPanel->setRenderedImage(std::move(frame));
    });
    connect(m_thermalPanel, &ThermalPanel::editGestureStarted,
            this, [this] {
                m_thermalTimeBeforeGesture = m_thermalTimeH;
            });
    connect(m_thermalPanel, &ThermalPanel::editGestureFinished,
            this, [this] {
                if (!m_thermalTimeBeforeGesture) return;
                pushSettingCommand(CommandId::ModifyThermal, tr("Thermal time"),
                                   *m_thermalTimeBeforeGesture, m_thermalTimeH,
                                   [this](const double& v) {
                                       m_thermalPanel->setTime(v);
                                       applyThermalTime(v);
                                   });
                m_thermalTimeBeforeGesture.reset();
            });

    // Display enhancement panel signals
    connect(m_displayEnhancementPanel, &DisplayEnhancementPanel::enhancementChanged,
            this, &MainWindow::applyDisplayEnhancement);
}

void MainWindow::setupWorkspaces() {
    m_workspaces = new WorkspaceManager(this, this);
    for (auto it = m_docks.constBegin(); it != m_docks.constEnd(); ++it) {
        m_workspaces->registerDock(it.key(), it.value());
    }

    // The tab bar rides in its own immovable toolbar above the main one, so it
    // reads as part of the window chrome rather than as a floating widget.
    auto* workspaceBar = new QToolBar(this);
    workspaceBar->setObjectName(QStringLiteral("toolbar_workspaces"));
    workspaceBar->setMovable(false);
    workspaceBar->setFloatable(false);
    workspaceBar->addWidget(m_workspaces->tabBar());
    insertToolBar(m_mainToolBar, workspaceBar);
    addToolBarBreak();

    // Same switch from the View menu, with a shortcut each.
    m_workspaceMenu = new QMenu(this);
    auto* group = new QActionGroup(this);
    group->setExclusive(true);
    int index = 0;
    for (const QString& id : WorkspaceManager::workspaceIds()) {
        QAction* action = m_workspaceMenu->addAction(QString());
        action->setCheckable(true);
        action->setData(id);
        action->setShortcut(QKeySequence(Qt::CTRL | static_cast<Qt::Key>(Qt::Key_1 + index)));
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, id]() {
            m_workspaces->setCurrentWorkspace(id);
        });
        m_workspaceActions.append(action);
        ++index;
    }
    // Placed above the panel list: choosing a workspace is the coarse move,
    // toggling one panel the fine one.
    m_viewMenu->insertMenu(m_panelsMenu->menuAction(), m_workspaceMenu);

    connect(m_workspaces, &WorkspaceManager::workspaceChanged, this,
            [this](const QString& id) {
                for (QAction* action : std::as_const(m_workspaceActions)) {
                    action->setChecked(action->data().toString() == id);
                }
                applyWorkspaceEditingScope(id);
                showStatusMessage(tr("Workspace: %1")
                                      .arg(WorkspaceManager::workspaceTitle(id)));
            });
}


// ============================================================================
// Status bar
// ============================================================================

void MainWindow::setupStatusBar() {
    m_statusLabel = new QLabel();
    m_sampleRateLabel = new QLabel();
    m_sampleCountLabel = new QLabel();
    m_editModeLabel = new QLabel();
    m_styling.bind([this] { uistyle::applyChipStyle(m_editModeLabel, uistyle::ChipTone::Accent); });
    m_debugValueLabel = new QLabel();
    m_debugValueLabel->setMinimumWidth(250);
    m_styling.bind([this] { uistyle::applyMonospaceStyle(m_debugValueLabel); });
    // Time left at the measured rate, next to the bar that says how far along
    // it is. Hidden unless there is a target to count down to.
    m_etaLabel = new QLabel();
    m_etaLabel->setVisible(false);
    m_renderProgress = new QProgressBar();
    m_renderProgress->setMaximumWidth(200);
    m_renderProgress->setRange(0, 100);
    m_renderProgress->setValue(0);

    statusBar()->addWidget(m_statusLabel, 1);
    m_mcpStatusLabel = new QLabel();
    m_mcpStatusLabel->setVisible(false);

    statusBar()->addPermanentWidget(m_mcpStatusLabel);
    statusBar()->addPermanentWidget(m_debugValueLabel);
    statusBar()->addPermanentWidget(m_editModeLabel);
    statusBar()->addPermanentWidget(m_sampleCountLabel);
    statusBar()->addPermanentWidget(m_sampleRateLabel);
    statusBar()->addPermanentWidget(m_etaLabel);
    statusBar()->addPermanentWidget(m_renderProgress);

    // The sample rate has to fall to zero when accumulation stops, and it
    // cannot learn that from a frame: past the target, or while paused, the
    // render loop stops asking for frames, so the last one ever delivered
    // would leave its rate standing on screen indefinitely. Ticking drains the
    // window whether or not anything is rendering.
    m_sampleRateTimer = new QTimer(this);
    m_sampleRateTimer->setInterval(250);
    connect(m_sampleRateTimer, &QTimer::timeout,
            this, &MainWindow::refreshSampleRateLabel);
    m_sampleRateTimer->start();

    // Transient messages expire. Around thirty call sites used to write into
    // this label with no timeout, so whatever happened last stayed there
    // looking like the current state of the application.
    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(true);
    connect(m_statusTimer, &QTimer::timeout, this, [this]() {
        m_statusLabel->setText(tr("Ready"));
    });
}

void MainWindow::showStatusMessage(const QString& message, int timeoutMs) {
    m_statusLabel->setText(message);
    if (timeoutMs > 0) {
        m_statusTimer->start(timeoutMs);
    } else {
        m_statusTimer->stop();
    }
}

void MainWindow::updateRenderProgress() {
    const uint32_t target = m_renderSettingsPanel ? m_renderSettingsPanel->spp() : 0;
    const uint32_t samples = m_vulkanWindow ? m_vulkanWindow->currentSampleCount() : 0;
    if (target == 0) {
        // Infinite mode: pulsing indeterminate bar while accumulating.
        m_renderProgress->setRange(0, 0);
        return;
    }
    if (m_renderProgress->maximum() != 100) {
        m_renderProgress->setRange(0, 100);
    }
    const int percent = static_cast<int>(
        std::min<uint32_t>(100u, (samples * 100u) / target));
    m_renderProgress->setValue(percent);
}

void MainWindow::setupConnections() {
    // Connect Vulkan window signals
    connect(m_vulkanWindow, &QuantiloomVulkanWindow::frameRendered,
            this, &MainWindow::onFrameRendered);

    // Connect scene loaded signal to update panels
    // Queued for the same reason the sceneLoaded failure branch below is: it
    // is emitted from inside initResources(), and a modal dialog there would
    // run a nested event loop in the middle of Qt creating device resources.
    connect(m_vulkanWindow, &QuantiloomVulkanWindow::renderContextFailed,
            this, [this](const QString& message) {
                showStatusMessage(tr("The renderer failed to start"));
                QMessageBox::critical(this, tr("Renderer Unavailable"), message);
            }, Qt::QueuedConnection);

    connect(m_vulkanWindow, &QuantiloomVulkanWindow::sceneLoaded,
            this, [this](bool success, const QString& message) {
                if (success) {
                    // The open is only now known to have worked, which is the
                    // first point at which the file is worth remembering.
                    rememberRecentFile(m_pendingOpenPath);
                    m_pendingOpenPath.clear();
                    updatePanelsFromScene();
                    seedPastedNodesFromDocument();
                    syncPanelsFromRenderer();
                    showStatusMessage(message);
                } else {
                    // The whole failure branch runs a turn later, because none
                    // of it can safely run where this signal is emitted from.
                    //
                    // Returning to the guidance page hides the window container
                    // holding the Vulkan surface, and Qt answers that by
                    // releasing the renderer's resources -- which destroys the
                    // render context from inside a signal that context's own
                    // loadScene() emitted. For a scene named on the command
                    // line it is worse still: that load runs from
                    // initResources(), so the teardown re-enters Qt while it is
                    // creating the very resources being destroyed. The dialog
                    // then exec()ed a nested event loop in the middle of it,
                    // which is why it came up empty and took the window with
                    // it.
                    m_pendingOpenPath.clear();

                    QMetaObject::invokeMethod(this, [this, message]() {
                        // Deliberately *not* returning to the guidance page.
                        //
                        // openPath() shows the render surface before asking for
                        // the load, because the Vulkan window only builds its
                        // renderer once it is exposed. Hiding it again is
                        // therefore not a page change: Qt releases the
                        // renderer's resources, the render context is destroyed,
                        // and the application does not survive that. Going out
                        // and back is destructive in a way the guidance page is
                        // not worth.
                        //
                        // What that page was here for was to explain an empty
                        // viewport. The dialog below and the status line say
                        // more than it would, and File > Open is still one
                        // menu away.
                        showStatusMessage(tr("Failed to load scene"));
                        QMessageBox::warning(this, tr("Scene Load Failed"), message);
                    }, Qt::QueuedConnection);
                }
            });

    // Connect viewport click for selection
    connect(m_vulkanWindow, &QuantiloomVulkanWindow::viewportClicked,
            this, &MainWindow::onViewportClicked);

    // Connect viewport hover for debug value display
    connect(m_vulkanWindow, &QuantiloomVulkanWindow::mouseHovered,
            this, &MainWindow::onViewportHovered);

    // Keep the camera panel in step with orbit, pan, zoom and fly.
    connect(m_vulkanWindow, &QuantiloomVulkanWindow::cameraChanged,
            this, &MainWindow::onCameraChanged);

    // First-run shader compilation used to be an application-modal dialog
    // raised before the window appeared. It reports beside the viewport now.
    connect(m_vulkanWindow, &QuantiloomVulkanWindow::longOperationStarted,
            this, [this](const QString& description) {
                // The MCP pump stands down for the duration: this operation
                // runs its own event loop, and a tool dispatched from it would
                // reach a renderer that is halfway through building resources.
                m_longOperationActive = true;
                m_viewportFrame->setBusyMessage(description);
                showStatusMessage(description, 0);
            });
    connect(m_vulkanWindow, &QuantiloomVulkanWindow::longOperationFinished,
            this, [this]() {
                m_longOperationActive = false;
                m_viewportFrame->setBusyMessage(QString());
                showStatusMessage(tr("Ready"));
            });
}

void MainWindow::setupEditingSystem() {
    // Pass to Vulkan window
    m_vulkanWindow->setEditingComponents(m_selectionManager, m_transformGizmo, m_undoStack);

    // Connect undo/redo actions
    connect(m_undoAction, &QAction::triggered, m_undoStack, &UndoStack::undo);
    connect(m_redoAction, &QAction::triggered, m_undoStack, &UndoStack::redo);

    // Connect undo stack state changes
    connect(m_undoStack, &UndoStack::canUndoChanged, this, &MainWindow::onUndoRedoChanged);
    connect(m_undoStack, &UndoStack::canRedoChanged, this, &MainWindow::onUndoRedoChanged);
    // Fires after every push, undo and redo: undo/redo move node transforms
    // under the panels, which otherwise showed the pre-undo values until the
    // node was deselected and reselected.
    connect(m_undoStack, &UndoStack::indexChanged, this,
            [this](int) {
                // Topology first: if the undo removed or restored nodes, the
                // tree and the selection must reflect that before the panels
                // re-read the selection
                refreshTopologyIfChanged();
                refreshSelectionPanels();
            });

    // Connect selection changes
    connect(m_selectionManager, &SelectionManager::selectionChanged,
            this, &MainWindow::onSelectionChanged);

    // Connect gizmo transform changes
    connect(m_transformGizmo, &TransformGizmo::dragStarted,
            this, &MainWindow::onGizmoDragStarted);
    connect(m_transformGizmo, &TransformGizmo::transformChanged,
            this, &MainWindow::onGizmoTransformChanged);
    connect(m_transformGizmo, &TransformGizmo::transformFinished,
            this, &MainWindow::onGizmoTransformFinished);
    connect(m_transformGizmo, &TransformGizmo::dragCancelled,
            this, &MainWindow::onGizmoDragCancelled);

    // Keep the status chip, the Edit ▸ Transform entries and the toolbar
    // buttons showing the gizmo's actual state, whichever of them changed it.
    connect(m_transformGizmo, &TransformGizmo::modeChanged,
            this, [this](TransformGizmo::Mode mode) {
                QString modeText;
                switch (mode) {
                    case TransformGizmo::Mode::Translate:
                        modeText = tr("[G] Translate");
                        m_translateAction->setChecked(true);
                        break;
                    case TransformGizmo::Mode::Rotate:
                        modeText = tr("[R] Rotate");
                        m_rotateAction->setChecked(true);
                        break;
                    case TransformGizmo::Mode::Scale:
                        modeText = tr("[T] Scale");
                        m_scaleAction->setChecked(true);
                        break;
                }
                m_editModeLabel->setText(modeText);
            });

    connect(m_transformGizmo, &TransformGizmo::spaceChanged,
            this, [this](TransformGizmo::Space space) {
                const bool local = space == TransformGizmo::Space::Local;
                const QSignalBlocker blocker(m_localSpaceAction);
                m_localSpaceAction->setChecked(local);
                showStatusMessage(local ? tr("Local space") : tr("World space"));
            });

    // Sync selection with scene tree panel (highlight selected items)
    connect(m_selectionManager, &SelectionManager::selectionChanged,
            m_sceneTreePanel, &SceneTreePanel::setSelectedNodes);

    connect(m_selectionManager, &SelectionManager::selectionCleared,
            m_sceneTreePanel, &SceneTreePanel::clearSelectionHighlight);
}

// ============================================================================
// Translation
// ============================================================================

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    } else if (uistyle::isThemeChangeEvent(event)) {
        restyleUi();
    } else if (event->type() == QEvent::WindowStateChange && m_titleBar) {
        // Covers every route into and out of maximised, including the ones the
        // button never sees: Aero Snap, Win+Up, double-clicking the caption.
        m_titleBar->setWindowMaximized(isMaximized());
    }
    QMainWindow::changeEvent(event);
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    if (m_chrome && m_chrome->handleNativeEvent(message, result)) {
        return true;
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::retranslateUi() {
    m_fileMenu->setTitle(tr("&File"));
    m_openAction->setText(tr("&Open..."));
    m_recentMenu->setTitle(tr("Open &Recent"));
    m_saveAction->setText(tr("&Save"));
    m_saveAsAction->setText(tr("Save &As..."));
    m_exportImageAction->setText(tr("Export &Image (raw render)..."));
    m_exportCubeAction->setText(tr("Render Hyperspectral &Cube..."));
    m_exportCubeAction->setToolTip(
        tr("Trace every band to completion and stream the cube to disk"));
    m_renderSequenceAction->setText(tr("Render Se&quence..."));
    m_renderSequenceAction->setToolTip(
        tr("Render this scene once per step of a temperature sweep"));
    m_dumpThermalElementsAction->setText(tr("Export Thermal &Elements (CSV)..."));
    m_dumpThermalElementsAction->setToolTip(
        tr("Write the solved temperature per triangle at the hour on screen, "
           "with the material properties the solve actually used."));
    m_exportImageAction->setToolTip(
        tr("Write the accumulated render without display enhancement."));
    m_screenshotAction->setText(tr("Save Screensho&t (as displayed)"));
    m_screenshotAction->setToolTip(
        tr("Write what the viewport shows, display enhancement included."));
    m_exitAction->setText(tr("E&xit"));

    m_editMenu->setTitle(tr("&Edit"));
    m_transformMenu->setTitle(tr("&Transform"));
    m_translateAction->setText(tr("&Translate"));
    m_rotateAction->setText(tr("&Rotate"));
    m_scaleAction->setText(tr("&Scale"));
    m_localSpaceAction->setText(tr("&Local Space"));
    m_localSpaceAction->setToolTip(tr("Transform along the object's own axes instead of the world's"));
    m_selectAllAction->setText(tr("Select &All"));
    m_invertSelectionAction->setText(tr("&Invert Selection"));
    m_copyAction->setText(tr("&Copy"));
    m_pasteAction->setText(tr("&Paste"));
    m_pasteAction->setToolTip(
        tr("Paste as instances: geometry and materials stay shared with the source."));
    m_duplicateAction->setText(tr("D&uplicate"));
    m_deleteAction->setText(tr("De&lete"));
    m_preferencesAction->setText(tr("&Preferences..."));

    m_viewMenu->setTitle(tr("&View"));
    if (m_workspaceMenu) {
        m_workspaceMenu->setTitle(tr("&Workspace"));
        for (QAction* action : std::as_const(m_workspaceActions)) {
            action->setText(WorkspaceManager::workspaceTitle(action->data().toString()));
        }
        m_workspaces->retranslateUi();
    }
    if (m_viewportFrame) {
        m_viewportFrame->retranslateUi();
    }
    if (m_titleBar) {
        m_titleBar->retranslateUi();
    }
    m_panelsMenu->setTitle(tr("&Panels"));
    m_resetLayoutAction->setText(tr("&Reset Layout"));
    if (m_themeMenu) {
        m_themeMenu->setTitle(tr("&Theme"));
        for (auto it = m_themeActions.cbegin(); it != m_themeActions.cend(); ++it) {
            it.value()->setText(ThemeManager::displayName(it.key()));
        }
    }
    m_cameraMenu->setTitle(tr("&Camera"));
    m_resetCameraAction->setText(tr("&Reset View"));
    m_frameSelectedAction->setText(tr("&Frame Selected"));
    m_frameSelectedAction->setToolTip(
        tr("Orbit around the selection and pull back to fit it"));
    m_frameAllAction->setText(tr("Frame &All"));
    m_orthographicAction->setText(tr("&Orthographic"));
    m_orthographicAction->setToolTip(
        tr("Parallel projection: edges stay parallel and two things the same size "
           "measure the same at any depth"));
    for (QAction* action : std::as_const(m_viewPresetActions)) {
        const QString id = action->data().toString();
        if (id == QLatin1String("front"))       action->setText(tr("&Front"));
        else if (id == QLatin1String("back"))   action->setText(tr("&Back"));
        else if (id == QLatin1String("right"))  action->setText(tr("Ri&ght"));
        else if (id == QLatin1String("left"))   action->setText(tr("&Left"));
        else if (id == QLatin1String("top"))    action->setText(tr("&Top"));
        else if (id == QLatin1String("bottom")) action->setText(tr("Botto&m"));
    }

    m_debugMenu->setTitle(tr("&Debug Visualization"));
    for (QMenu* categoryMenu : std::as_const(m_debugCategoryMenus)) {
        const auto category =
            static_cast<catalog::DebugCategory>(categoryMenu->property("debugCategory").toInt());
        categoryMenu->setTitle(catalog::debugCategoryName(category));
    }
    for (auto it = m_debugActions.constBegin(); it != m_debugActions.constEnd(); ++it) {
        const auto mode = static_cast<quantiloom::DebugVisualizationMode>(it.key());
        it.value()->setText(catalog::debugModeName(mode));
        it.value()->setToolTip(catalog::debugModeDescription(mode));
    }
    m_displayEnhancementAction->setText(tr("Display &Enhancement (CLAHE)"));
    m_displayEnhancementAction->setToolTip(
        tr("Affects the viewport and screenshots only; exported images keep their raw values."));
    m_showGridAction->setText(tr("Show &Grid"));
    m_showGridAction->setToolTip(
        tr("Ground grid overlay in the viewport; does not affect renders or accumulation."));
    m_motionAdaptiveResolutionAction->setText(tr("&Adaptive Resolution During Motion"));
    m_motionAdaptiveResolutionAction->setToolTip(
        tr("Trace fewer pixels while the camera or a gizmo is being dragged, for a "
           "smoother viewport. The image sharpens again when the gesture ends; "
           "exported images are never affected."));

    m_renderMenu->setTitle(tr("&Render"));
    m_startRenderAction->setText(tr("&Start Render"));
    m_startRenderAction->setToolTip(tr("Discard the accumulated samples and render from scratch"));
    m_stopRenderAction->setText(tr("S&top Render"));
    m_resumeRenderAction->setText(tr("&Resume Render"));
    m_resumeRenderAction->setToolTip(tr("Carry on from the samples already accumulated"));
    m_resetAccumulationAction->setText(tr("Reset &Accumulation"));
    m_qualityMenu->setTitle(tr("&Quality"));
    {
        const auto presets = catalog::qualityPresets();
        const auto actions = m_qualityGroup->actions();
        for (int i = 0; i < actions.size() && i < presets.size(); ++i) {
            actions.at(i)->setText(catalog::qualityPresetLabel(presets.at(i)));
        }
    }
    m_spectralMenu->setTitle(tr("&Spectral Mode"));
    for (auto it = m_spectralActions.constBegin(); it != m_spectralActions.constEnd(); ++it) {
        const auto mode = static_cast<quantiloom::SpectralMode>(it.key());
        it.value()->setText(catalog::spectralModeLabel(mode));
        it.value()->setToolTip(catalog::spectralModeDescription(mode));
    }

    // Toolbar. Combo boxes are refilled by value, never by position, so the
    // current selection survives the language change instead of being reset
    // to whatever now sits at the old index.
    m_mainToolBar->setWindowTitle(tr("Main Toolbar"));
    m_spectralComboLabel->setText(tr(" Spectral: "));
    m_debugComboLabel->setText(tr(" Debug: "));
    for (int i = 0; i < m_spectralCombo->count(); ++i) {
        const auto mode = static_cast<quantiloom::SpectralMode>(m_spectralCombo->itemData(i).toInt());
        m_spectralCombo->setItemText(i, catalog::spectralModeLabel(mode));
    }
    for (int i = 0; i < m_debugCombo->count(); ++i) {
        const QVariant itemData = m_debugCombo->itemData(i);
        if (!itemData.isValid()) continue;   // separator
        const auto mode = static_cast<quantiloom::DebugVisualizationMode>(itemData.toInt());
        m_debugCombo->setItemText(i, catalog::debugModeName(mode));
    }

    m_assignSpectrumAction->setText(tr("&Assign Measured Spectrum"));
    m_assignSpectrumAction->setToolTip(
        tr("Bind the material selected in the scene to the spectrum highlighted "
           "in the Spectral Library, replacing anything already bound"));
    m_addEndmemberAction->setText(tr("Add Spectral &Endmember"));
    m_addEndmemberAction->setToolTip(
        tr("Add the highlighted spectrum alongside the ones already bound, so "
           "the surface renders as a mixture of measured materials"));

    m_toolsMenu->setTitle(tr("&Tools"));
    m_spectralGenAction->setText(tr("Spectral Material &Generator"));
    m_mcpAction->setText(tr("&MCP Server"));
    m_mcpAction->setToolTip(tr("Let an agent drive Studio over a local connection"));
    updateMcpStatusLabel();

    m_helpMenu->setTitle(tr("&Help"));
    m_shortcutsAction->setText(tr("&Keyboard Shortcuts"));
    m_debugReferenceAction->setText(tr("Reading &Debug Output"));
    m_aboutAction->setText(tr("&About"));
    m_aboutQtAction->setText(tr("About &Qt"));

    // Dock titles are not set here: each panel names itself and the dock
    // follows, which is what stopped a fully translated panel from sitting
    // under an English label.

    if (m_statusLabel->text().isEmpty() || !m_statusTimer->isActive()) {
        m_statusLabel->setText(tr("Ready"));
    }
    // Repainted from the measurement rather than blanked: a language switch
    // while nothing is rendering has no frame coming along afterwards to put
    // the figure back.
    refreshSampleRateLabel();
    m_sampleCountLabel->setText(tr("Samples: %1").arg(
        m_vulkanWindow ? m_vulkanWindow->currentSampleCount() : 0));
    m_debugValueLabel->setText(tr("Hover the viewport to inspect"));
    if (m_editModeLabel->text().isEmpty()) {
        m_editModeLabel->setText(tr("[G] Translate"));
    }

    onUndoRedoChanged();
    rebuildRecentMenu();
    updateWindowTitle();
}

// ============================================================================
// Document state
// ============================================================================

void MainWindow::updateWindowTitle() {
    const QString document = m_currentConfigFile.isEmpty() ? m_currentSceneFile
                                                           : m_currentConfigFile;
    const QString name = document.isEmpty() ? tr("Untitled")
                                            : QFileInfo(document).fileName();
    // "[*]" is Qt's placeholder for the modified marker; setWindowModified
    // decides whether it renders.
    setWindowTitle(tr("%1[*] — Quantiloom Studio").arg(name));
    setWindowFilePath(document);
    setWindowModified(m_sceneModified);
}

void MainWindow::setCurrentDocument(const QString& filePath) {
    if (filePath.endsWith(QLatin1String(".toml"), Qt::CaseInsensitive)) {
        m_currentConfigFile = filePath;
    }
    // Deliberately not remembering the file here. This runs when the open is
    // *requested*, and for a model or a config's scene the load is asynchronous
    // -- so a file that turned out to be unopenable was listed under Recent
    // regardless. It is remembered from the sceneLoaded handler instead, where
    // the outcome is known.
    updateWindowTitle();
}

void MainWindow::setSceneModified(bool modified) {
    if (m_sceneModified == modified) {
        return;
    }
    m_sceneModified = modified;
    setWindowModified(modified);
}

bool MainWindow::confirmDiscardChanges() {
    if (!m_sceneModified) {
        return true;
    }
    const auto reply = QMessageBox::question(
        this,
        tr("Unsaved Changes"),
        tr("The scene configuration has been modified. Save your changes?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (reply == QMessageBox::Save) {
        return onSaveScene();
    }
    return reply == QMessageBox::Discard;
}

// ============================================================================
// Recent files
// ============================================================================

QStringList MainWindow::recentFiles() const {
    QSettings settings;
    return settings.value(kRecentFilesKey).toStringList();
}

void MainWindow::rememberRecentFile(const QString& filePath) {
    if (filePath.isEmpty()) {
        return;
    }
    QSettings settings;
    QStringList files = settings.value(kRecentFilesKey).toStringList();
    files.removeAll(filePath);
    files.prepend(filePath);
    while (files.size() > kMaxRecentFiles) {
        files.removeLast();
    }
    settings.setValue(kRecentFilesKey, files);
    rebuildRecentMenu();
    if (m_viewportFrame) {
        m_viewportFrame->setRecentFiles(recentFiles());
    }
}

void MainWindow::rebuildRecentMenu() {
    if (!m_recentMenu) {
        return;
    }
    m_recentMenu->clear();

    const QStringList files = recentFiles();
    for (const QString& path : files) {
        QAction* action = m_recentMenu->addAction(QFileInfo(path).fileName());
        action->setToolTip(path);
        connect(action, &QAction::triggered, this, [this, path]() {
            if (!QFileInfo::exists(path)) {
                QMessageBox::warning(this, tr("Open Failed"),
                                     tr("This file no longer exists:\n%1").arg(path));
                return;
            }
            if (confirmDiscardChanges()) {
                openPath(path);
            }
        });
    }

    m_recentMenu->setEnabled(!files.isEmpty());
    if (files.isEmpty()) {
        m_recentMenu->addAction(tr("(none)"))->setEnabled(false);
    } else {
        m_recentMenu->addSeparator();
        m_recentMenu->addAction(tr("Clear List"), this, [this]() {
            QSettings settings;
            settings.remove(kRecentFilesKey);
            rebuildRecentMenu();
        });
    }
}

// ============================================================================
// Persistence
// ============================================================================

void MainWindow::saveWindowState() const {
    QSettings settings;
    settings.setValue(kGeometryKey, saveGeometry());
    settings.setValue(kStateVersionKey, kWindowStateVersion);
    // Dock arrangements are stored per workspace rather than as one blob, so
    // a personal layout in one workspace does not follow the user into
    // another.
    m_workspaces->save(settings);
}

void MainWindow::restoreWindowState() {
    QSettings settings;
    if (settings.value(kStateVersionKey, 0).toInt() != kWindowStateVersion) {
        // Written by a build with a different dock inventory. Restoring it
        // would produce a window missing panels that now exist, so it is
        // discarded rather than half-applied.
        return;
    }
    const QByteArray geometry = settings.value(kGeometryKey).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    m_workspaces->restore(settings);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!confirmDiscardChanges()) {
        event->ignore();
        return;
    }
    saveWindowState();
    event->accept();
}

// ============================================================================
// Slots
// ============================================================================

void MainWindow::onOpenScene() {
    if (!confirmDiscardChanges()) {
        return;
    }

    const QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Open Scene or Configuration"),
        m_currentConfigFile.isEmpty() ? QString()
                                      : QFileInfo(m_currentConfigFile).absolutePath(),
        tr("Scenes and Configurations (*.toml *.gltf *.glb *.usd *.usda *.usdc *.usdz);;"
           "TOML Configuration (*.toml);;"
           "glTF Files (*.gltf *.glb);;"
           "OpenUSD Files (*.usd *.usda *.usdc *.usdz);;"
           "All Files (*)"));

    if (!fileName.isEmpty()) {
        openPath(fileName);
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    // Configurations only. A dropped model would have to invent a document
    // around itself -- which file dialog Open does deliberately, with a
    // confirmation -- and a dropped .exr has no unambiguous meaning either
    // (environment map? something to compare against?). Both stay on the
    // routes that ask.
    if (droppedConfigPath(event->mimeData()).isEmpty()) {
        return;
    }
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QString path = droppedConfigPath(event->mimeData());
    if (path.isEmpty()) {
        return;
    }
    event->acceptProposedAction();
    if (!confirmDiscardChanges()) {
        return;
    }
    openPath(path);
}

QString MainWindow::droppedConfigPath(const QMimeData* mime) {
    if (!mime || !mime->hasUrls()) {
        return {};
    }
    const QList<QUrl> urls = mime->urls();
    // One file. A multi-file drop has no sensible reading when the window
    // holds a single document.
    if (urls.size() != 1 || !urls.first().isLocalFile()) {
        return {};
    }
    const QString path = urls.first().toLocalFile();
    if (!path.endsWith(QLatin1String(".toml"), Qt::CaseInsensitive)) {
        return {};
    }
    return path;
}

void MainWindow::openFromCommandLine(const QString& filePath) {
    if (!QFileInfo::exists(filePath)) {
        showStatusMessage(tr("No such file: %1").arg(filePath));
        return;
    }
    openPath(filePath);
}

bool MainWindow::openPath(const QString& filePath) {
    if (filePath.endsWith(QLatin1String(".toml"), Qt::CaseInsensitive)) {
        SceneConfig config;
        if (!m_configManager->loadConfig(filePath, config)) {
            QMessageBox::warning(this, tr("Open Failed"),
                tr("Failed to load configuration: %1").arg(m_configManager->lastError()));
            return false;
        }
        // Any syntactically valid TOML parses, so a successful load says
        // nothing about whether this file is a *scene* configuration. Without
        // a scene there is nothing to render and applyConfig() would quietly
        // do nothing at all -- which is how opening an unrelated .toml from
        // some other project reported success and showed an empty viewport.
        // The core rejects the same input for the same reason.
        if (config.gltfPath.isEmpty() && config.usdPath.isEmpty()) {
            QMessageBox::warning(this, tr("Open Failed"),
                tr("%1 is not a scene configuration: it names no scene.gltf or scene.usd.")
                    .arg(QFileInfo(filePath).fileName()));
            return false;
        }

        m_currentConfigFile = filePath;
        m_pendingOpenPath = filePath;
        // Panels first, then the render context -- and the two read the file
        // through different code. What the panels show comes from
        // ConfigManager; what renders comes from the SDK, which interprets the
        // same ~50 keys the CLI does. syncPanelsFromRenderer() reconciles them
        // once the scene reports loaded, so the widgets end up showing what is
        // actually being rendered rather than this repo's reading of the file.
        applyConfig(config);
        m_vulkanWindow->applyConfig(m_configManager->sharedRawConfig(), config.baseDir);
        setSceneModified(false);
        setCurrentDocument(filePath);
        showStatusMessage(tr("Loaded configuration: %1").arg(QFileInfo(filePath).fileName()));
        return true;
    }

    // A bare model has no configuration behind it, so there is no document to
    // save over -- Save will ask for a destination the first time.
    m_currentSceneFile = filePath;
    m_currentConfigFile.clear();
    // And no configuration behind it means the previous document's must go.
    // applySpectralConfig() runs on every successful load and reads whatever
    // getRawConfig() still holds, so a TOML opened earlier used to lend this
    // model its solar LUT and spectral curves.
    m_configManager->clearLoadedConfig();
    // Show the render surface *before* asking for the load: the Vulkan window
    // only creates its renderer once it is exposed, so keeping the guidance
    // page up until the scene reports success would wait on a renderer that
    // was itself waiting to be shown.
    m_pendingOpenPath = filePath;
    m_viewportFrame->setSceneLoaded(true);
    m_vulkanWindow->loadScene(filePath);
    setSceneModified(false);
    setCurrentDocument(filePath);
    showStatusMessage(tr("Loading %1...").arg(QFileInfo(filePath).fileName()));
    return true;
}

bool MainWindow::onSaveScene() {
    if (m_currentConfigFile.isEmpty()) {
        return onSaveSceneAs();
    }
    return writeConfig(m_currentConfigFile);
}

bool MainWindow::onSaveSceneAs() {
    const QString suggested = m_currentConfigFile.isEmpty()
        ? QStringLiteral("scene_config.toml")
        : m_currentConfigFile;

    const QString fileName = QFileDialog::getSaveFileName(
        this, tr("Save Configuration As"), suggested, tr("TOML Configuration (*.toml)"));

    if (fileName.isEmpty()) {
        return false;
    }
    return writeConfig(fileName);
}

bool MainWindow::writeConfig(const QString& filePath) {
    SceneConfig config;
    collectCurrentConfig(config);

    if (!m_configManager->exportConfig(filePath, config)) {
        QMessageBox::warning(this, tr("Save Failed"),
            tr("Failed to write configuration: %1").arg(m_configManager->lastError()));
        showStatusMessage(tr("Save failed"));
        return false;
    }

    m_currentConfigFile = filePath;
    setSceneModified(false);
    setCurrentDocument(filePath);
    showStatusMessage(tr("Saved %1").arg(QFileInfo(filePath).fileName()));
    return true;
}

void MainWindow::onExportHyperspectralCube() {
    if (!m_vulkanWindow->getScene()) {
        QMessageBox::information(this, tr("No Scene"),
            tr("Open a scene before rendering a cube."));
        return;
    }
    // The document as it stands, panels included -- the cube is of what is on
    // screen, not of what the file said when it was opened.
    SceneConfig config;
    collectCurrentConfig(config);
    HyperspectralExportDialog dialog(config, this);
    dialog.exec();
}

void MainWindow::onRenderSequence() {
    const auto* scene = m_vulkanWindow->getScene();
    if (!scene) {
        QMessageBox::information(this, tr("No Scene"),
            tr("Open a scene before rendering a sequence."));
        return;
    }

    // The document as it stands, panels included, for the same reason the cube
    // takes it that way: the sequence is of what is on screen.
    SceneConfig config;
    collectCurrentConfig(config);

    QStringList materialNames;
    for (const auto& material : scene->materials) {
        if (!material.name.empty()) {
            materialNames << QString::fromStdString(material.name);
        }
    }

    SequenceRenderDialog dialog(config, materialNames, this);
    dialog.exec();
}

void MainWindow::onDumpThermalElements() {
    if (!m_thermalEnabled) {
        QMessageBox::information(this, tr("No Thermal Solve"),
            tr("Turn the thermal solve on before exporting its elements: this "
               "writes the field the solver produced, and without a solve there "
               "is none. A scene whose temperatures were typed rather than "
               "solved is already in the configuration."));
        return;
    }

    // The document's own dump_elements as the suggestion, so a config written
    // for a study proposes the file that study expects.
    const QString suggested = m_thermalDumpElements.isEmpty()
        ? QStringLiteral("thermal_elements.csv")
        : m_thermalDumpElements;
    const QString fileName = QFileDialog::getSaveFileName(
        this, tr("Export Thermal Elements"), suggested,
        tr("CSV files (*.csv);;All files (*)"));
    if (fileName.isEmpty()) {
        return;
    }

    const auto written = m_vulkanWindow->dumpThermalElements(fileName);
    if (!written) {
        QMessageBox::warning(this, tr("Export Failed"),
            tr("Could not write the thermal elements:\n\n%1")
                .arg(QString::fromStdString(written.error())));
        return;
    }
    showStatusMessage(tr("Wrote the thermal elements at %1 h to %2")
                          .arg(m_thermalTimeH, 0, 'f', 2)
                          .arg(QFileInfo(fileName).fileName()));
}

void MainWindow::onExportImage() {
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Export Image (raw render)"),
        QString(),
        tr("EXR Image (*.exr);;PNG Image (*.png);;All Files (*)"));

    if (fileName.isEmpty()) return;

    // The accumulated render, without display enhancement: an export is meant
    // to carry physical values, and CLAHE is a viewing aid. Screenshots take
    // the other path and say so in their menu entry.
    auto image = m_vulkanWindow->captureScreenshot();
    if (!image) {
        QMessageBox::warning(this, tr("Export Failed"),
            tr("Failed to capture the image. Make sure a scene is loaded."));
        showStatusMessage(tr("Export failed"));
        return;
    }

    // Any extension other than the two we can actually write gets .exr
    // appended rather than kept: "render.jpg" used to pass the no-dot test and
    // receive EXR bytes under a name that promised JPEG.
    bool success = false;
    if (fileName.endsWith(QLatin1String(".exr"), Qt::CaseInsensitive)) {
        success = quantiloom::ImageIO::WriteEXR(fileName.toStdString(), *image);
    } else if (fileName.endsWith(QLatin1String(".png"), Qt::CaseInsensitive)) {
        success = quantiloom::ImageIO::WritePNG(fileName.toStdString(), *image);
    } else {
        fileName += QStringLiteral(".exr");
        success = quantiloom::ImageIO::WriteEXR(fileName.toStdString(), *image);
    }

    if (success) {
        showStatusMessage(tr("Exported %1").arg(QFileInfo(fileName).fileName()));
    } else {
        QMessageBox::warning(this, tr("Export Failed"),
            tr("Failed to save the image:\n%1").arg(fileName));
        showStatusMessage(tr("Export failed"));
    }
}

void MainWindow::onStartRender() {
    m_vulkanWindow->setRenderPaused(false);
    m_vulkanWindow->resetAccumulation();
    beginRenderTiming();
    setRenderActionsRunning(true);
    const uint32_t spp = m_renderSettingsPanel->spp();
    if (spp == 0)
        showStatusMessage(tr("Rendering (infinite)"));
    else
        showStatusMessage(tr("Rendering from scratch to %1 samples").arg(spp));
}

void MainWindow::onResumeRender() {
    // Start's twin without the reset: the samples already accumulated are
    // kept and the render carries on from them.
    const uint32_t already = m_vulkanWindow->currentSampleCount();
    m_vulkanWindow->setRenderPaused(false);
    beginRenderTiming();
    setRenderActionsRunning(true);
    const uint32_t spp = m_renderSettingsPanel->spp();
    if (spp == 0)
        showStatusMessage(tr("Resuming from %1 samples (infinite)").arg(already));
    else
        showStatusMessage(tr("Resuming from %1 samples to %2").arg(already).arg(spp));
}

void MainWindow::onStopRender() {
    m_vulkanWindow->setRenderPaused(true);
    setRenderActionsRunning(false);
    showStatusMessage(tr("Rendering stopped at %1 samples")
                          .arg(m_vulkanWindow->currentSampleCount()));
}

void MainWindow::setRenderActionsRunning(bool running) {
    m_startRenderAction->setEnabled(!running);
    m_stopRenderAction->setEnabled(running);
    // Resuming is only meaningful when stopped with something to resume from;
    // at zero samples Start says the same thing without the qualification.
    m_resumeRenderAction->setEnabled(!running && m_vulkanWindow->currentSampleCount() > 0);
}

void MainWindow::beginRenderTiming() {
    // The ETA is measured, not predicted from the sample count: how long a
    // sample takes depends on the scene, the mode and the window size, and
    // only this run knows.
    m_renderStartedAt = QDateTime::currentMSecsSinceEpoch();
    m_renderStartSamples = m_vulkanWindow->currentSampleCount();
    m_msPerSample = 0.0;
    m_renderCompleteReported = false;
}

void MainWindow::autoExportRender(uint32_t sampleCount) {
    // The raw accumulation, matching what Export Image writes rather than
    // what Save Screenshot does: an automatic export is for the physical
    // values, and display enhancement is a viewing aid.
    auto image = m_vulkanWindow->captureScreenshot();
    if (!image) {
        showStatusMessage(tr("Render complete, but the image could not be captured"));
        return;
    }

    // Beside the document when there is one, next to the screenshots
    // otherwise -- an automatic write should never have to ask where to go.
    QString directory;
    QString stem;
    if (!m_currentConfigFile.isEmpty()) {
        const QFileInfo info(m_currentConfigFile);
        directory = info.absolutePath();
        stem = info.completeBaseName();
    } else {
        QSettings settings;
        directory = settings.value("screenshot_path").toString();
        if (directory.isEmpty()) {
            directory = PreferencesDialog::defaultScreenshotPath();
        }
        stem = QStringLiteral("render");
    }

    QDir dir(directory);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        showStatusMessage(tr("Render complete, but %1 could not be created").arg(directory));
        return;
    }

    // The sample count is in the name: an auto-export at 256 and one at 4096
    // are different renders and should not overwrite each other.
    const QString path = dir.filePath(
        QStringLiteral("%1_%2spp.exr").arg(stem).arg(sampleCount));

    if (quantiloom::ImageIO::WriteEXR(path.toStdString(), *image)) {
        showStatusMessage(tr("Render complete — %1 samples, exported to %2")
                              .arg(sampleCount)
                              .arg(QFileInfo(path).fileName()));
    } else {
        showStatusMessage(tr("Render complete, but the export to %1 failed").arg(path));
    }
}

void MainWindow::onResetCamera() {
    m_vulkanWindow->resetCamera();
    showStatusMessage(tr("Camera reset"));
}

void MainWindow::applyCameraProjection(bool orthographic) {
    m_vulkanWindow->setCameraProjection(orthographic, 0.0f);
    {
        const QSignalBlocker blocker(m_orthographicAction);
        m_orthographicAction->setChecked(orthographic);
    }
    setSceneModified(true);
    showStatusMessage(orthographic ? tr("Orthographic projection")
                                   : tr("Perspective projection"));
}

void MainWindow::onFrameSelected() {
    const quantiloom::Scene* scene = m_vulkanWindow->getScene();
    if (!scene) {
        return;
    }
    // Nothing selected is not an error -- it is the case where "frame what
    // matters" means the whole scene, which is what the user gets.
    if (m_selectionManager->selectedNodes().isEmpty()) {
        onFrameAll();
        return;
    }

    glm::vec3 min, max;
    m_selectionManager->computeSelectionBounds(scene, min, max);
    m_vulkanWindow->frameBounds(min, max);
    showStatusMessage(tr("Framed %n object(s)", "", m_selectionManager->selectedNodes().size()));
}

bool MainWindow::computeSceneBounds(glm::vec3& outMin, glm::vec3& outMax) const {
    const quantiloom::Scene* scene = m_vulkanWindow->getScene();
    if (!scene) {
        return false;
    }

    outMin = glm::vec3(std::numeric_limits<float>::max());
    outMax = glm::vec3(std::numeric_limits<float>::lowest());
    bool any = false;

    // Tombstoned nodes are excluded: a deleted object should not keep the
    // camera pulled back to include where it used to be.
    for (const auto& node : scene->nodes) {
        if (!node.active || node.meshIndex >= scene->meshes.size()) {
            continue;
        }
        glm::vec3 meshMin, meshMax;
        scene->meshes[node.meshIndex].ComputeBounds(meshMin, meshMax);
        const glm::vec3 corners[8] = {
            {meshMin.x, meshMin.y, meshMin.z}, {meshMax.x, meshMin.y, meshMin.z},
            {meshMin.x, meshMax.y, meshMin.z}, {meshMax.x, meshMax.y, meshMin.z},
            {meshMin.x, meshMin.y, meshMax.z}, {meshMax.x, meshMin.y, meshMax.z},
            {meshMin.x, meshMax.y, meshMax.z}, {meshMax.x, meshMax.y, meshMax.z}};
        for (const auto& corner : corners) {
            const glm::vec3 world(node.transform * glm::vec4(corner, 1.0f));
            outMin = glm::min(outMin, world);
            outMax = glm::max(outMax, world);
        }
        any = true;
    }
    return any;
}

void MainWindow::onFrameAll() {
    glm::vec3 min, max;
    if (!computeSceneBounds(min, max)) {
        return;
    }
    m_vulkanWindow->frameBounds(min, max);
    showStatusMessage(tr("Framed the scene"));
}

void MainWindow::updateSceneScale() {
    // Navigation constants follow the scene: fly speed, and the near and far
    // ends of the zoom. Pushed on load and after topology edits, since both
    // can change the extent by orders of magnitude.
    glm::vec3 min, max;
    const float radius = computeSceneBounds(min, max)
        ? glm::length(max - min) * 0.5f : 0.0f;
    m_vulkanWindow->setSceneScale(radius);
}

void MainWindow::onResetLayout() {
    // Per workspace, not globally: a user who has rearranged the debug
    // workspace should not lose their layout work everywhere else.
    m_workspaces->resetCurrentToDefault();
    showStatusMessage(tr("Layout reset for %1")
                          .arg(WorkspaceManager::workspaceTitle(m_workspaces->currentWorkspace())));
}

void MainWindow::onTakeScreenshot() {
    // The image as displayed, display enhancement included.
    auto image = m_vulkanWindow->captureDisplayImage();
    if (!image) {
        QMessageBox::warning(this, tr("Screenshot Failed"),
            tr("Failed to capture the screenshot. Make sure a scene is loaded."));
        showStatusMessage(tr("Screenshot failed"));
        return;
    }

    QSettings settings;
    QString screenshotDir = settings.value("screenshot_path").toString();
    if (screenshotDir.isEmpty()) {
        screenshotDir = PreferencesDialog::defaultScreenshotPath();
    }

    QDir dir(screenshotDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        QMessageBox::warning(this, tr("Screenshot Failed"),
            tr("Failed to create the screenshot directory:\n%1").arg(screenshotDir));
        showStatusMessage(tr("Screenshot failed"));
        return;
    }

    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss-zzz"));
    const QString baseFilename = dir.filePath(timestamp);
    const QString exrPath = baseFilename + QStringLiteral(".exr");
    const QString pngPath = baseFilename + QStringLiteral(".png");

    if (!quantiloom::ImageIO::WriteEXR(exrPath.toStdString(), *image)) {
        QMessageBox::warning(this, tr("Screenshot Failed"),
            tr("Failed to save the EXR file:\n%1").arg(exrPath));
        showStatusMessage(tr("Screenshot failed (EXR)"));
        return;
    }

    if (!quantiloom::ImageIO::WritePNG(pngPath.toStdString(), *image)) {
        QMessageBox::warning(this, tr("Screenshot Warning"),
            tr("The EXR was saved but the PNG failed:\n%1").arg(pngPath));
        showStatusMessage(tr("Screenshot saved (EXR only): %1").arg(exrPath));
        return;
    }

    showStatusMessage(tr("Screenshot saved: %1.{exr,png}").arg(baseFilename));
}

void MainWindow::onAbout() {
    QMessageBox::about(
        this,
        tr("About Quantiloom"),
        // %1 rather than the number itself: the version used to be spelled out
        // inside this string, so every bump changed the source text and
        // invalidated the Chinese translation of the whole paragraph, which then
        // had to be rewritten to say the same thing. It comes from
        // project(VERSION) through main.cpp.
        tr("<h3>Quantiloom</h3>"
           "<p>Version %1</p>"
           "<p>A spectral renderer with hardware ray tracing support.</p>"
           "<p>Features:</p>"
           "<ul>"
           "<li>Hardware ray tracing</li>"
           "<li>Spectral rendering</li>"
           "<li>PBR materials with spectral extensions</li>"
           "<li>Atmospheric scattering</li>"
           "</ul>"
           "<p>Copyright (c) 2025-2026 blitzcolo</p>")
            .arg(QApplication::applicationVersion()));
}

void MainWindow::onShowShortcuts() {
    auto* dialog = new HelpDialog(this, findChildren<QAction*>(), {
        {tr("Right-drag"),   tr("Orbit the camera")},
        {tr("Middle-drag"),  tr("Pan the camera")},
        {tr("Wheel"),        tr("Zoom")},
        {tr("W / A / S / D"), tr("Fly the camera")},
        {tr("Q / E"),        tr("Move the camera down / up")},
        {tr("Shift"),        tr("Fine control while dragging")},
        {tr("G"),            tr("Translate mode")},
        {tr("R"),            tr("Rotate mode")},
        {tr("T"),            tr("Scale mode")},
        {tr("X / Y / Z"),    tr("Constrain the transform to an axis")},
        {tr("Space"),        tr("Toggle world / local space")},
        {tr("Escape"),       tr("Cancel the drag, then clear the selection")},
        {tr("Left-drag"),    tr("Transform the selection")},
        {tr("Hover"),        tr("Read the pixel under the cursor in debug modes")},
    });
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->showPage(HelpDialog::Page::Shortcuts);
    dialog->show();
}

void MainWindow::onShowDebugReference() {
    auto* dialog = new HelpDialog(this, findChildren<QAction*>(), {});
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->showPage(HelpDialog::Page::DebugOutput);
    dialog->show();
}

void MainWindow::onPreferences() {
    PreferencesDialog dialog(this);

    QSettings settings;
    dialog.setScreenshotPath(settings.value("screenshot_path").toString());
    dialog.setSelectedLocale(LanguageManager::instance().currentLocale());
    dialog.setSelectedThemeId(ThemeManager::instance().currentThemeId());

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    settings.setValue("screenshot_path", dialog.screenshotPath());

    // Both take effect immediately. Installing the translator makes Qt post a
    // LanguageChange event to every widget, and setting the palette posts a
    // PaletteChange; each widget re-applies its text and its colours in turn.
    LanguageManager::instance().switchTo(dialog.selectedLocale());
    applyTheme(dialog.selectedThemeId());

    showStatusMessage(tr("Preferences saved"));
}

void MainWindow::onSelectAll() {
    const auto* scene = m_vulkanWindow->getScene();
    if (!scene || scene->nodes.empty()) {
        return;
    }
    QSet<int> all;
    all.reserve(static_cast<int>(scene->nodes.size()));
    for (int i = 0; i < static_cast<int>(scene->nodes.size()); ++i) {
        all.insert(i);
    }
    m_selectionManager->selectMultiple(all);
}

void MainWindow::onInvertSelection() {
    const auto* scene = m_vulkanWindow->getScene();
    if (!scene || scene->nodes.empty()) {
        return;
    }
    const QSet<int>& selected = m_selectionManager->selectedNodes();
    QSet<int> inverted;
    for (int i = 0; i < static_cast<int>(scene->nodes.size()); ++i) {
        if (!selected.contains(i)) {
            inverted.insert(i);
        }
    }
    if (inverted.isEmpty()) {
        m_selectionManager->clearSelection();
    } else {
        m_selectionManager->selectMultiple(inverted);
    }
}

QString MainWindow::makeUniqueNodeName(const QString& base, QSet<QString>& taken) const {
    QString stem = base.isEmpty() ? QStringLiteral("Node") : base;

    // Strip an all-digit .NNN suffix so pasting a paste yields Hull.002
    // rather than Hull.001.001 -- Blender's convention
    const int dot = stem.lastIndexOf(QLatin1Char('.'));
    if (dot > 0) {
        bool allDigits = false;
        stem.mid(dot + 1).toInt(&allDigits);
        if (allDigits) {
            stem = stem.left(dot);
        }
    }

    const auto* scene = m_vulkanWindow->getScene();
    if (scene) {
        for (const auto& node : scene->nodes) {
            taken.insert(QString::fromStdString(node.name));
        }
    }

    for (int n = 1; n < 100000; ++n) {
        const QString candidate = QStringLiteral("%1.%2")
            .arg(stem)
            .arg(n, 3, 10, QLatin1Char('0'));
        if (!taken.contains(candidate)) {
            taken.insert(candidate);
            return candidate;
        }
    }
    return stem;  // 99999 copies of one name; not a scene, a fuzzer
}

void MainWindow::refreshAfterTopologyChange() {
    const auto* scene = m_vulkanWindow->getScene();

    m_sceneTreePanel->refresh();
    // Pasting or deleting can change the scene's extent, and the navigation
    // limits are derived from it.
    updateSceneScale();

    int active = 0;
    QSet<int> pruned;
    if (scene) {
        for (size_t i = 0; i < scene->nodes.size(); ++i) {
            if (scene->nodes[i].active) {
                ++active;
            }
        }
        for (const int index : m_selectionManager->selectedNodes()) {
            if (index >= 0 && static_cast<size_t>(index) < scene->nodes.size() &&
                scene->nodes[static_cast<size_t>(index)].active) {
                pruned.insert(index);
            }
        }
    }
    m_lastTopology = {scene ? static_cast<int>(scene->nodes.size()) : 0, active};

    if (pruned != m_selectionManager->selectedNodes()) {
        if (pruned.isEmpty()) {
            m_selectionManager->clearSelection();
        } else {
            m_selectionManager->selectMultiple(pruned);
        }
    }
}

void MainWindow::refreshTopologyIfChanged() {
    const auto* scene = m_vulkanWindow->getScene();
    int active = 0;
    if (scene) {
        for (const auto& node : scene->nodes) {
            if (node.active) {
                ++active;
            }
        }
    }
    const QPair<int, int> now{scene ? static_cast<int>(scene->nodes.size()) : 0, active};
    if (now != m_lastTopology) {
        refreshAfterTopologyChange();
    }
}

void MainWindow::onCopyNodes() {
    const auto* scene = m_vulkanWindow->getScene();
    if (!scene || !m_selectionManager->hasSelection()) {
        showStatusMessage(tr("Nothing selected to copy"));
        return;
    }

    QList<int> indices = m_selectionManager->selectedNodes().values();
    std::sort(indices.begin(), indices.end());

    m_nodeClipboard.clear();
    QString fragment;
    for (const int index : indices) {
        if (index < 0 || static_cast<size_t>(index) >= scene->nodes.size()) {
            continue;
        }
        const auto& node = scene->nodes[static_cast<size_t>(index)];
        NodeClipboardEntry entry;
        entry.sourceName = QString::fromStdString(node.name);
        entry.sourceIndex = index;
        entry.transform = node.transform;
        m_nodeClipboard.append(entry);

        // The OS clipboard gets the same fragment a save would write, so a
        // copied object can be pasted straight into a config file by hand
        if (!entry.sourceName.isEmpty()) {
            fragment += QStringLiteral("[[duplicates]]\nsource = \"%1\"\nname = \"%1.paste\"\nmatrix = [\n")
                            .arg(entry.sourceName);
            for (int c = 0; c < 4; ++c) {
                fragment += QStringLiteral("    ");
                for (int r = 0; r < 4; ++r) {
                    fragment += QString::number(entry.transform[c][r]);
                    if (c != 3 || r != 3) {
                        fragment += QStringLiteral(", ");
                    }
                }
                fragment += QStringLiteral("\n");
            }
            fragment += QStringLiteral("]\n\n");
        }
    }

    if (!fragment.isEmpty()) {
        QGuiApplication::clipboard()->setText(fragment);
    }
    showStatusMessage(tr("Copied %n object(s)", "", m_nodeClipboard.size()));
}

glm::vec3 MainWindow::pasteOffset(
    const std::vector<PasteNodesCommand::Spec>& specs) const {
    if (specs.empty()) {
        return glm::vec3(0.0f);
    }

    // One offset for the whole batch, from the batch's center: a pasted
    // group keeps its internal arrangement and slides as a unit
    glm::vec3 center(0.0f);
    for (const auto& spec : specs) {
        center += glm::vec3(spec.transform[3]);
    }
    center /= static_cast<float>(specs.size());

    glm::vec3 camPos, forward, right, up;
    m_vulkanWindow->getCameraInfo(camPos, forward, right, up);
    glm::vec3 statePos, stateTarget, stateUp;
    float fovY = 45.0f;
    m_vulkanWindow->getCameraState(statePos, stateTarget, stateUp, fovY);

    // distance * tan(fovY/2) is half the vertical extent the camera sees at
    // the sources' depth, so the nudge is the same fraction of the screen at
    // any zoom -- the gizmo sizes itself with the same product. Along the
    // camera's right axis, i.e. in the view plane: depth-ward would hide the
    // copy inside the original from exactly the viewpoint that matters.
    constexpr float kNudgeFraction = 0.25f;
    const float distance = glm::length(center - camPos);
    const float fovScale = std::tan(glm::radians(fovY) * 0.5f);
    return right * (kNudgeFraction * distance * fovScale);
}

void MainWindow::executePaste(const std::vector<PasteNodesCommand::Spec>& specs,
                              const QHash<QString, QString>& sourceByName) {
    std::vector<PasteNodesCommand::Spec> placed = specs;
    const glm::vec3 nudge = pasteOffset(placed);
    for (auto& spec : placed) {
        spec.transform[3] += glm::vec4(nudge, 0.0f);
    }

    auto command = std::make_unique<PasteNodesCommand>(m_vulkanWindow, placed);
    command->execute();
    const QVector<int> created = command->createdIndices();
    m_undoStack->push(std::move(command));

    // Register each copy for [[duplicates]] persistence, matched through its
    // unique name so a partial paste cannot misalign the mapping
    const auto* scene = m_vulkanWindow->getScene();
    QSet<int> newSelection;
    for (const int index : created) {
        newSelection.insert(index);
        if (scene && static_cast<size_t>(index) < scene->nodes.size()) {
            const QString name =
                QString::fromStdString(scene->nodes[static_cast<size_t>(index)].name);
            const auto source = sourceByName.constFind(name);
            if (source != sourceByName.constEnd()) {
                m_pastedNodes.insert(index, source.value());
            }
        }
    }

    refreshAfterTopologyChange();
    if (!newSelection.isEmpty()) {
        m_selectionManager->selectMultiple(newSelection);
    }
    setSceneModified(true);
    showStatusMessage(tr("Pasted %n object(s)", "", created.size()));
}

void MainWindow::onPasteNodes() {
    const auto* scene = m_vulkanWindow->getScene();
    if (!scene) {
        return;
    }
    if (m_nodeClipboard.isEmpty()) {
        showStatusMessage(tr("Nothing to paste"));
        return;
    }

    std::vector<PasteNodesCommand::Spec> specs;
    QHash<QString, QString> sourceByName;
    QSet<QString> taken;
    int skipped = 0;
    for (const auto& entry : m_nodeClipboard) {
        // The index is only trusted while it still names the same node; after
        // a reload (or a copy from an earlier document) the name decides
        int source = entry.sourceIndex;
        const bool indexValid =
            source >= 0 && static_cast<size_t>(source) < scene->nodes.size() &&
            (entry.sourceName.isEmpty() ||
             scene->nodes[static_cast<size_t>(source)].name == entry.sourceName.toStdString());
        if (!indexValid) {
            source = -1;
            if (!entry.sourceName.isEmpty()) {
                const std::string wanted = entry.sourceName.toStdString();
                for (size_t i = 0; i < scene->nodes.size(); ++i) {
                    if (scene->nodes[i].name == wanted) {
                        source = static_cast<int>(i);
                        break;
                    }
                }
            }
        }
        if (source < 0) {
            ++skipped;
            continue;
        }

        PasteNodesCommand::Spec spec;
        spec.sourceIndex = source;
        const QString sourceName =
            entry.sourceName.isEmpty()
                ? QString::fromStdString(scene->nodes[static_cast<size_t>(source)].name)
                : entry.sourceName;
        spec.name = makeUniqueNodeName(sourceName, taken);
        spec.transform = entry.transform;
        specs.push_back(spec);
        sourceByName.insert(spec.name, sourceName);
    }

    if (specs.empty()) {
        showStatusMessage(tr("Clipboard objects no longer exist in this scene"));
        return;
    }
    if (skipped > 0) {
        qWarning() << "Paste: skipped" << skipped
                   << "clipboard entr(ies) whose source no longer exists";
    }
    executePaste(specs, sourceByName);
}

void MainWindow::onDuplicateNodes() {
    const auto* scene = m_vulkanWindow->getScene();
    if (!scene || !m_selectionManager->hasSelection()) {
        showStatusMessage(tr("Nothing selected to duplicate"));
        return;
    }

    QList<int> indices = m_selectionManager->selectedNodes().values();
    std::sort(indices.begin(), indices.end());

    std::vector<PasteNodesCommand::Spec> specs;
    QHash<QString, QString> sourceByName;
    QSet<QString> taken;
    for (const int index : indices) {
        if (index < 0 || static_cast<size_t>(index) >= scene->nodes.size()) {
            continue;
        }
        const auto& node = scene->nodes[static_cast<size_t>(index)];
        PasteNodesCommand::Spec spec;
        spec.sourceIndex = index;
        spec.name = makeUniqueNodeName(QString::fromStdString(node.name), taken);
        spec.transform = node.transform;
        specs.push_back(spec);
        sourceByName.insert(spec.name, QString::fromStdString(node.name));
    }
    if (specs.empty()) {
        return;
    }
    executePaste(specs, sourceByName);
}

void MainWindow::onDeleteNodes() {
    const auto* scene = m_vulkanWindow->getScene();
    if (!scene || !m_selectionManager->hasSelection()) {
        showStatusMessage(tr("Nothing selected to delete"));
        return;
    }

    QVector<int> indices;
    int activeCount = 0;
    for (size_t i = 0; i < scene->nodes.size(); ++i) {
        if (scene->nodes[i].active) {
            ++activeCount;
        }
    }
    for (const int index : m_selectionManager->selectedNodes()) {
        if (index >= 0 && static_cast<size_t>(index) < scene->nodes.size() &&
            scene->nodes[static_cast<size_t>(index)].active) {
            indices.append(index);
        }
    }
    if (indices.isEmpty()) {
        return;
    }
    if (indices.size() >= activeCount) {
        // The renderer cannot represent an empty scene (and a viewport
        // showing one would just be a sky); the guard lives here, where the
        // user can be told, rather than as a crash in the SDK
        showStatusMessage(tr("Cannot delete every object in the scene"));
        return;
    }
    std::sort(indices.begin(), indices.end());

    auto command = std::make_unique<RemoveNodesCommand>(m_vulkanWindow, indices);
    command->execute();
    m_undoStack->push(std::move(command));

    refreshAfterTopologyChange();
    setSceneModified(true);
    showStatusMessage(tr("Deleted %n object(s)", "", indices.size()));
}

void MainWindow::onFrameRendered(float frameTimeMs, uint32_t sampleCount) {
    // Smoothed, because the per-frame figure jitters too fast to read. The
    // weight is on the history rather than the newest sample for the same
    // reason. An interval longer than a second is the render loop starting up
    // again after it had stopped -- an idle gap, not a frame that took that
    // long -- and averaging it in would poison the figure for several seconds.
    constexpr float kFrameTimeSmoothing = 0.9f;
    constexpr float kMaxPlausibleFrameMs = 1000.0f;
    if (frameTimeMs > 0.0f && frameTimeMs < kMaxPlausibleFrameMs) {
        m_smoothedFrameTimeMs = (m_smoothedFrameTimeMs > 0.0f)
            ? (kFrameTimeSmoothing * m_smoothedFrameTimeMs +
               (1.0f - kFrameTimeSmoothing) * frameTimeMs)
            : frameTimeMs;
    }

    // The label reports samples per second, not frames per second. For a
    // progressive integrator the two are not interchangeable: a frame that
    // re-presents the accumulation costs nothing and a frame that traces a
    // heavy scene costs everything, so a frame rate says nothing about how
    // fast the image is converging. Frames per second survives in the tooltip.
    if (!m_sampleRateWindow.empty() && sampleCount < m_sampleRateWindow.back().samples) {
        // Accumulation was reset. A window straddling the reset would read the
        // drop as an enormous gain, the counts being unsigned.
        m_sampleRateWindow.clear();
    }
    m_sampleRateWindow.push_back({QDateTime::currentMSecsSinceEpoch(), sampleCount});
    refreshSampleRateLabel();

    m_sampleCountLabel->setText(tr("Samples: %1").arg(sampleCount));

    m_renderSettingsPanel->setSampleCount(sampleCount);
    updateRenderProgress();

    const uint32_t target = m_vulkanWindow->targetSPP();

    // Milliseconds per accumulated sample, measured over this run's wall
    // clock. frameTimeMs is not a substitute: it times one frame, and a frame
    // carries however many samples per pixel the SPP setting asks for.
    if (m_renderStartedAt > 0 && sampleCount > m_renderStartSamples) {
        const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_renderStartedAt;
        m_msPerSample = static_cast<double>(elapsed) /
                        static_cast<double>(sampleCount - m_renderStartSamples);
    }
    updateRenderEta(sampleCount, target);

    if (target > 0 && sampleCount >= target) {
        onRenderReachedTarget(sampleCount);
    }
}

void MainWindow::refreshSampleRateLabel() {
    // One second of history. Short enough to follow a scene change, long
    // enough that a heavy scene delivering a sample every few hundred
    // milliseconds still has two points to draw a line through.
    constexpr qint64 kRateWindowMs = 1000;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // Everything older than the window goes, except that the oldest survivor
    // is left one step behind the window edge -- otherwise the span shrinks to
    // whatever fraction of the second the newest frames happen to cover, and
    // the rate is computed over a few milliseconds of noise.
    while (m_sampleRateWindow.size() >= 2 &&
           now - m_sampleRateWindow[1].wallClockMs > kRateWindowMs) {
        m_sampleRateWindow.pop_front();
    }

    // A render that has stopped -- paused, at its target, or wedged -- drains
    // to a single point within one window length and reads zero, which is the
    // honest answer: it is producing no samples. Frames that arrive without
    // advancing the count, as they do when the accumulation is only being
    // re-presented, reach the same zero by a shorter route.
    double samplesPerSecond = 0.0;
    if (m_sampleRateWindow.size() >= 2) {
        const qint64 spanMs = m_sampleRateWindow.back().wallClockMs -
                              m_sampleRateWindow.front().wallClockMs;
        const uint32_t gained = m_sampleRateWindow.back().samples -
                                m_sampleRateWindow.front().samples;
        if (spanMs > 0) {
            samplesPerSecond =
                1000.0 * static_cast<double>(gained) / static_cast<double>(spanMs);
        }
    }

    // Two decimals below ten, because a scene slow enough to matter renders at
    // a fraction of a sample per second and one decimal would round it to nil.
    //
    // The unit stays spp/s whatever the render extent is: a sample is still a
    // sample, and the rate is still what it says. What changes is the size of
    // the image those samples are landing in, and a rate quoted against a
    // quarter of the pixels without saying so would read as free speed -- so
    // the scale goes next to it while it is down.
    const float renderScale = m_vulkanWindow ? m_vulkanWindow->currentRenderScale() : 1.0f;
    const QString rate = m_sampleRateWindow.empty()
        ? tr("-- spp/s")
        : tr("%1 spp/s").arg(samplesPerSecond, 0, 'f', samplesPerSecond < 10.0 ? 2 : 1);
    m_sampleRateLabel->setText(
        renderScale < 1.0f
            ? tr("%1 ×%2").arg(rate).arg(renderScale, 0, 'g', 3)
            : rate);

    // Frame timing in the tooltip rather than the label: it answers a
    // different question -- is the viewport responsive, and is the scene
    // expensive -- and only when asked.
    const float gpuMs = m_vulkanWindow ? m_vulkanWindow->lastGpuFrameTimeMs() : 0.0f;
    QString tip;
    if (m_smoothedFrameTimeMs <= 0.0f) {
        tip = tr("Samples accumulated per second of wall clock");
    } else if (gpuMs > 0.0f) {
        tip = tr("Samples per second of wall clock. %1 frames/s, %2 ms per frame, "
                 "%3 ms of it on the GPU")
                  .arg(1000.0f / m_smoothedFrameTimeMs, 0, 'f', 1)
                  .arg(m_smoothedFrameTimeMs, 0, 'f', 1)
                  .arg(gpuMs, 0, 'f', 1);
    } else {
        tip = tr("Samples per second of wall clock. %1 frames/s, %2 ms per frame")
                  .arg(1000.0f / m_smoothedFrameTimeMs, 0, 'f', 1)
                  .arg(m_smoothedFrameTimeMs, 0, 'f', 1);
    }
    // Only while the extent is reduced. Saying "rendering at 1920x1080" on
    // every frame of every session would be noise around the one case that
    // needs explaining: why the viewport just went soft.
    if (renderScale < 1.0f) {
        const QSize renderSize = m_vulkanWindow->currentRenderSize();
        tip += QLatin1Char('\n');
        tip += tr("Tracing at %1x%2 while the view is moving.")
                   .arg(renderSize.width())
                   .arg(renderSize.height());
    }
    m_sampleRateLabel->setToolTip(tip);
}

void MainWindow::updateRenderEta(uint32_t sampleCount, uint32_t target) {
    if (!m_etaLabel) {
        return;
    }
    // Nothing to estimate against in infinite mode, before the first sample
    // lands, or once the target is met.
    const bool estimable = target > 0 && sampleCount < target &&
                           m_msPerSample > 0.0 && !m_vulkanWindow->isRenderPaused();
    m_etaLabel->setVisible(estimable);
    if (!estimable) {
        return;
    }
    const double remainingMs = m_msPerSample * static_cast<double>(target - sampleCount);
    m_etaLabel->setText(tr("ETA %1").arg(formatDuration(static_cast<qint64>(remainingMs))));
}

QString MainWindow::formatDuration(qint64 ms) {
    const qint64 totalSeconds = ms / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(minutes)
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

void MainWindow::onRenderReachedTarget(uint32_t sampleCount) {
    // Reached once per run: the frame loop stops requesting updates at the
    // target, but a stray frame in flight can arrive after it.
    if (m_renderCompleteReported) {
        setRenderActionsRunning(false);
        return;
    }
    m_renderCompleteReported = true;
    setRenderActionsRunning(false);

    const qint64 elapsed = (m_renderStartedAt > 0)
        ? QDateTime::currentMSecsSinceEpoch() - m_renderStartedAt : 0;
    showStatusMessage(tr("Render complete — %1 samples in %2")
                          .arg(sampleCount)
                          .arg(formatDuration(elapsed)));

    if (m_renderSettingsPanel->autoExportOnComplete()) {
        autoExportRender(sampleCount);
    }
}

// ============================================================================
// Panel Slots
// ============================================================================

void MainWindow::onMaterialSelected(int materialIndex) {
    const quantiloom::Scene* scene = m_vulkanWindow->getScene();
    if (scene && materialIndex >= 0 &&
        static_cast<size_t>(materialIndex) < scene->materials.size()) {
        const auto& material = scene->materials[static_cast<size_t>(materialIndex)];
        m_materialEditorPanel->setMaterial(materialIndex, &material);
        m_materialEditorPanel->setThermalProperties(
            m_thermalProperties.value(QString::fromStdString(material.name)));
        m_propertiesPanel->showMaterial();
        m_spectralMaterialGenPanel->setCurrentMaterialIndex(materialIndex);
        m_currentMaterialIndex = materialIndex;
        refreshSpectralLibraryTarget();
        if (QDockWidget* dock = m_docks.value(QStringLiteral("properties"), nullptr)) {
            dock->show();
            dock->raise();
        }
        showStatusMessage(tr("Material '%1' selected")
                              .arg(QString::fromStdString(material.name)));
    }
}

void MainWindow::onMaterialChanged(int index, const quantiloom::Material& material) {
    applyMaterial(index, material);
}

void MainWindow::onLightingChanged(const quantiloom::LightingParams& params) {
    // Take only what the lighting panel owns. The atmospheric transmittance
    // and temperature belong to the atmosphere panel now, and copying the
    // whole struct here would reset them to that panel's defaults on every
    // slider move.
    m_lightingParams->sunDirection = params.sunDirection;
    m_lightingParams->sunRadiance_spectral = params.sunRadiance_spectral;
    m_lightingParams->sunRadiance_rgb = params.sunRadiance_rgb;
    m_lightingParams->skyRadiance_spectral = params.skyRadiance_spectral;
    m_lightingParams->skyRadiance_rgb = params.skyRadiance_rgb;
    m_lightingParams->chromaR_correction = params.chromaR_correction;
    m_lightingParams->chromaB_correction = params.chromaB_correction;
    m_lightingParams->enableShadowRays = params.enableShadowRays;
    m_lightingParams->enableEnvironmentMap = params.enableEnvironmentMap;

    pushLightingParams();
    showStatusMessage(tr("Lighting updated"));
}

void MainWindow::onEnvironmentMapChanged(const QString& path, bool enabled) {
    applyEnvironmentMap(path, enabled);
}

void MainWindow::pushLightingParams() {
    m_vulkanWindow->setLightingParams(*m_lightingParams);
    setSceneModified(true);
}

void MainWindow::onNodeTransformEdited(int nodeIndex, const glm::mat4& transform) {
    applyNodeTransform(nodeIndex, transform);
}

void MainWindow::onMaterialWithCriChanged(int index, const quantiloom::Material& material,
                                          const quantiloom::ComplexRefractiveIndex& cri) {
    // The generator has no route to the GPU; uploading its (n,k) curves and
    // filling in the index is the shell's job.
    quantiloom::Material updated = material;
    const int criIndex = m_vulkanWindow->addComplexRefractiveIndex(cri);
    if (criIndex >= 0) {
        updated.complexRefractiveIndexIndex = criIndex;
    }
    onMaterialChanged(index, updated);
}

std::filesystem::path MainWindow::spectralDatabasePath(const QString& databaseId,
                                                       bool basisFile) {
    // Same candidate order as the atmosphere model pack: working directory
    // first, so a Studio launched from the repo root uses the checked-in
    // databases, then beside the executable for an installed build.
    const QString leaf = basisFile
        ? QStringLiteral("quantiloom_basis_v3_%1.qlbin").arg(databaseId)
        : QStringLiteral("quantiloom_materials_%1.json").arg(databaseId);
    const QStringList roots{
        QDir::currentPath() + QStringLiteral("/assets/spectral"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/assets/spectral"),
    };
    for (const QString& root : roots) {
        const QString candidate = QDir(root).filePath(leaf);
        if (QFileInfo::exists(candidate)) {
            return std::filesystem::path(
                QDir::toNativeSeparators(candidate).toStdWString());
        }
    }
    return {};
}

void MainWindow::onSpectralPreviewRequested(const QString& databaseId,
                                            const QString& materialName,
                                            const QString& band) {
    // The panel names what it wants to see; reconstructing it is the shell's
    // job, through the SDK function the renderer itself uses. A curve drawn
    // from a second implementation would be a preview of something other than
    // what renders.
    const auto basis = spectralDatabasePath(databaseId, /*basisFile=*/true);
    const auto materials = spectralDatabasePath(databaseId, /*basisFile=*/false);
    if (basis.empty() || materials.empty()) {
        m_spectralLibraryPanel->setPreviewCurve({}, QString());
        return;
    }

    auto curve = quantiloom::SpectralIO::ReconstructBasisCurve(
        basis, materials, materialName.toStdString(), band.toStdString());
    if (!curve.has_value()) {
        // Not an error dialog: a database that does not carry this band is a
        // normal thing to click on, and the plot's placeholder says so.
        m_spectralLibraryPanel->setPreviewCurve({}, QString());
        return;
    }

    QVector<QPair<double, double>> points;
    points.reserve(static_cast<int>(curve.value().samples.size()));
    for (const auto& [lambda, value] : curve.value().samples) {
        points.append({static_cast<double>(lambda), static_cast<double>(value)});
    }
    m_spectralLibraryPanel->setPreviewCurve(points, materialName);
}

void MainWindow::onSpectralMaterialAssigned(const QString& databaseId,
                                            const QString& materialName) {
    // Replace: whatever the material was made of, it is this now.
    applySpectralEndmembers(m_currentMaterialIndex, databaseId, {materialName});
}

void MainWindow::onSpectralEndmemberAdded(const QString& databaseId,
                                          const QString& materialName) {
    applySpectralEndmembers(m_currentMaterialIndex, databaseId,
                            currentSpectralRefs() << materialName);
}

void MainWindow::onSpectralEndmemberRemoved(int slot) {
    QStringList refs = currentSpectralRefs();
    if (slot < 0 || slot >= refs.size()) {
        return;
    }
    refs.removeAt(slot);
    applySpectralEndmembers(m_currentMaterialIndex, currentSpectralDatabaseId(), refs);
}

QStringList MainWindow::currentSpectralRefs() const {
    const quantiloom::Scene* scene = m_vulkanWindow->getScene();
    if (!scene || m_currentMaterialIndex < 0 ||
        static_cast<size_t>(m_currentMaterialIndex) >= scene->materials.size()) {
        return {};
    }
    const auto& material = scene->materials[static_cast<size_t>(m_currentMaterialIndex)];
    QStringList refs;
    if (material.HasQuantiloomRef()) {
        refs << QString::fromStdString(material.quantiloomMaterialRef);
        for (const auto& extra : material.quantiloomExtraRefs) {
            refs << QString::fromStdString(extra);
        }
    }
    return refs;
}

QString MainWindow::currentSpectralDatabaseId() const {
    const quantiloom::Scene* scene = m_vulkanWindow->getScene();
    if (!scene || m_currentMaterialIndex < 0 ||
        static_cast<size_t>(m_currentMaterialIndex) >= scene->materials.size()) {
        return {};
    }
    // Stored as "quantiloom_usgs"; the panel and the loaders speak "usgs".
    const QString type = QString::fromStdString(
        scene->materials[static_cast<size_t>(m_currentMaterialIndex)].quantiloomMaterialType);
    return type.startsWith(QStringLiteral("quantiloom_")) ? type.mid(11) : type;
}

void MainWindow::applySpectralEndmembers(int materialIndex, const QString& databaseId,
                                         const QStringList& refs) {
    const quantiloom::Scene* scene = m_vulkanWindow->getScene();
    if (!scene || materialIndex < 0 ||
        static_cast<size_t>(materialIndex) >= scene->materials.size()) {
        showStatusMessage(tr("Select a material in the scene first"));
        return;
    }

    quantiloom::Material updated = scene->materials[static_cast<size_t>(materialIndex)];

    // Unbinding: back to rendering from the material's own colour.
    if (refs.isEmpty()) {
        updated.quantiloomMaterialType.clear();
        updated.quantiloomMaterialRef.clear();
        updated.quantiloomExtraRefs.clear();
        updated.spectralReflectanceCurveIndex = -1;
        updated.endmemberCurveIndex1 = -1;
        updated.endmemberCurveIndex2 = -1;
        updated.endmemberCurveIndex3 = -1;
        updated.weightTextureIndex = -1;
        onMaterialChanged(materialIndex, updated);
        refreshSpectralLibraryTarget();
        showStatusMessage(tr("Removed the measured spectrum from '%1'")
                              .arg(QString::fromStdString(updated.name)));
        return;
    }

    const auto basis = spectralDatabasePath(databaseId, /*basisFile=*/true);
    const auto materials = spectralDatabasePath(databaseId, /*basisFile=*/false);
    if (basis.empty() || materials.empty()) {
        QMessageBox::warning(this, tr("Assign Failed"),
            tr("The %1 spectral database was not found beside the application "
               "or in the working directory.").arg(databaseId));
        return;
    }

    // The band the viewport is actually rendering, so what appears is what was
    // previewed.
    const QString band = catalog::spectralLibraryBand(m_vulkanWindow->spectralMode());

    const int wanted = std::min<int>(refs.size(), quantiloom::Material::MAX_ENDMEMBERS);
    std::vector<int> curveIndices;
    std::vector<glm::vec3> colours;
    QStringList bound;

    for (int i = 0; i < wanted; ++i) {
        auto curve = quantiloom::SpectralIO::ReconstructBasisCurve(
            basis, materials, refs.at(i).toStdString(), band.toStdString());
        if (!curve.has_value()) {
            QMessageBox::warning(this, tr("Assign Failed"),
                tr("Could not reconstruct '%1' in the %2 band:\n%3")
                    .arg(refs.at(i), band, QString::fromStdString(curve.error())));
            return;
        }
        const int curveIndex = m_vulkanWindow->addSpectralCurve(curve.value());
        if (curveIndex < 0) {
            QMessageBox::warning(this, tr("Assign Failed"),
                tr("Could not upload the spectrum for '%1'.").arg(refs.at(i)));
            return;
        }
        curveIndices.push_back(curveIndex);
        bound << refs.at(i);

        // The colour each endmember would appear as under D65 -- what the
        // unmixer compares against the base-colour texels. Always the VIS
        // band, whatever is being rendered: a weight is a spatial abundance
        // and the texture that suggests it is a visible-light image.
        auto visCurve = quantiloom::SpectralIO::ReconstructBasisCurve(
            basis, materials, refs.at(i).toStdString(), "VIS");
        colours.push_back(visCurve.has_value()
                              ? quantiloom::ReflectanceToLinearSrgbD65(visCurve.value())
                              : glm::vec3(0.0f));
    }

    updated.quantiloomMaterialType = ("quantiloom_" + databaseId).toStdString();
    updated.quantiloomMaterialRef = bound.first().toStdString();
    updated.quantiloomExtraRefs.clear();
    for (int i = 1; i < bound.size(); ++i) {
        updated.quantiloomExtraRefs.push_back(bound.at(i).toStdString());
    }

    // Curve indices are session-local pool positions; the references above are
    // what the configuration carries, and re-deriving the indices on load is
    // ApplyConfig's job.
    updated.spectralReflectanceCurveIndex = curveIndices[0];
    updated.endmemberCurveIndex1 = curveIndices.size() > 1 ? curveIndices[1] : -1;
    updated.endmemberCurveIndex2 = curveIndices.size() > 2 ? curveIndices[2] : -1;
    updated.endmemberCurveIndex3 = curveIndices.size() > 3 ? curveIndices[3] : -1;

    // The weights that make the mixture vary across the surface. Failing here
    // is not fatal: without a weight map the shader reads the first curve
    // flat, which is what a bound material did before mixing existed -- so say
    // what happened and carry on rather than refusing the assignment.
    updated.weightTextureIndex = -1;
    auto weights = m_vulkanWindow->buildEndmemberWeightTexture(
        static_cast<uint32_t>(materialIndex), colours);
    if (weights.has_value()) {
        updated.weightTextureIndex = weights.value();
    } else {
        showStatusMessage(tr("Bound the spectrum, but the surface renders flat: %1")
                              .arg(QString::fromStdString(weights.error())));
    }

    // Through the ordinary material-edit path, which makes the assignment
    // undoable and marks the material as edited so it is written on save.
    onMaterialChanged(materialIndex, updated);
    refreshSpectralLibraryTarget();

    if (updated.weightTextureIndex >= 0) {
        showStatusMessage(bound.size() == 1
            ? tr("Assigned %1 (%2, %3 band)").arg(bound.first(), databaseId, band)
            : tr("Mixed %n endmember(s) on '%1' (%2, %3 band)", "", static_cast<int>(bound.size()))
                  .arg(QString::fromStdString(updated.name), databaseId, band));
    }
}

void MainWindow::updateSpectralMenuState() {
    const bool haveMaterial = m_currentMaterialIndex >= 0;
    const bool haveRow = !m_spectralLibraryPanel->highlightedEntry().first.isEmpty();
    const int bound = m_spectralLibraryPanel->endmemberCount();

    m_assignSpectrumAction->setEnabled(haveMaterial && haveRow);
    // Same rule the panel's button follows: adding needs something to add to,
    // and stops at the mixture limit.
    m_addEndmemberAction->setEnabled(haveMaterial && haveRow && bound > 0 &&
                                     bound < quantiloom::Material::MAX_ENDMEMBERS);
}

void MainWindow::refreshSpectralLibraryTarget() {
    const quantiloom::Scene* scene = m_vulkanWindow->getScene();
    if (!scene || m_currentMaterialIndex < 0 ||
        static_cast<size_t>(m_currentMaterialIndex) >= scene->materials.size()) {
        m_spectralLibraryPanel->setTargetMaterial(-1, QString());
        return;
    }
    const auto& material = scene->materials[static_cast<size_t>(m_currentMaterialIndex)];
    m_spectralLibraryPanel->setTargetMaterial(
        m_currentMaterialIndex, QString::fromStdString(material.name), currentSpectralRefs());
    updateSpectralMenuState();
}

void MainWindow::onCameraChanged() {
    glm::vec3 position;
    glm::vec3 target;
    glm::vec3 up;
    float fovY = 45.0f;
    m_vulkanWindow->getCameraState(position, target, up, fovY);
    m_cameraPanel->setCameraState(position, target, fovY);
}

// The panel-side entry points delegate to the same apply* functions the menu
// and the toolbar use, so all three routes are literally one code path.
void MainWindow::onSppChanged(uint32_t spp) {
    applyTargetSpp(spp);
}

void MainWindow::onSpectralModeChanged(quantiloom::SpectralMode mode) {
    applySpectralMode(mode);
}

void MainWindow::onWavelengthChanged(float wavelength_nm) {
    applyWavelength(wavelength_nm);
}

void MainWindow::onDebugModeChanged(quantiloom::DebugVisualizationMode mode) {
    applyDebugMode(mode);
}

void MainWindow::onResetAccumulation() {
    m_vulkanWindow->resetAccumulation();
    // A fresh run: the target has to be reached again to be announced again,
    // and the ETA's measurement restarts from zero samples.
    beginRenderTiming();
    m_renderCompleteReported = false;
    setRenderActionsRunning(!m_vulkanWindow->isRenderPaused());
    updateRenderProgress();
    showStatusMessage(tr("Accumulation reset"));
}

void MainWindow::updatePanelsFromScene() {
    const quantiloom::Scene* scene = m_vulkanWindow->getScene();

    m_sceneTreePanel->setScene(scene);
    m_spectralMaterialGenPanel->setScene(scene);
    m_materialEditorPanel->clear();
    m_propertiesPanel->showEmptyState();
    // A new document: the fly speed and zoom range are read off its extent,
    // so an avocado and an aircraft carrier navigate the same way.
    updateSceneScale();

    if (scene) {
        // Seed the panel from the struct the renderer is actually running on,
        // not from the defaults. This fires on sceneLoaded, i.e. *after*
        // applyConfig() has fed the config's lighting to both -- so seeding
        // defaults here left the panel disagreeing with the render, and the
        // first slider move published the panel's whole set of defaults over
        // the config's values through onLightingChanged().
        m_lightingPanel->setLightingParams(*m_lightingParams);
        m_spectralConfigPanel->setWavelengthRange(
            scene->lambda_min, scene->lambda_max, scene->delta_lambda);
    }

}

void MainWindow::seedPastedNodesFromDocument() {
    // Re-register the nodes the document's [[duplicates]] entries created, so
    // the next save writes them back out. ApplyConfig appended them to the
    // scene under their unique names, which is what resolves them here; a
    // name the scene lacks (a stale config, a bare model load) simply seeds
    // nothing.
    //
    // Only from the sceneLoaded path -- NOT from updatePanelsFromScene, which
    // the MCP undo/redo tools reuse as a panel refresh. Clearing here on
    // every undo silently dropped the session's pastes from the next save.
    const auto* scene = m_vulkanWindow->getScene();

    m_pastedNodes.clear();
    int activeCount = 0;
    if (scene) {
        if (m_lastConfig) {
            for (const auto& dup : m_lastConfig->duplicateConfigs) {
                const std::string wanted = dup.name.toStdString();
                for (size_t i = 0; i < scene->nodes.size(); ++i) {
                    if (scene->nodes[i].name == wanted) {
                        m_pastedNodes.insert(static_cast<int>(i), dup.sourceName);
                        break;
                    }
                }
            }
        }
        for (const auto& node : scene->nodes) {
            if (node.active) {
                ++activeCount;
            }
        }
    }
    m_lastTopology = {scene ? static_cast<int>(scene->nodes.size()) : 0, activeCount};
}

// ============================================================================
// Config round trip
// ============================================================================

void MainWindow::applyConfig(const SceneConfig& config) {
    // Seeding widgets is not an edit. Panel setters are careful, but not
    // uniformly so -- AtmosphericPanel::setPreset deliberately re-enters its
    // own change handler to recompute the nine weather features -- so the
    // guard sits here, where the intent is unambiguous, rather than being
    // chased through every panel.
    const HistorySuppressor noHistory(m_suppressHistory);

    // Panels only. The render context is configured by the SDK, from the raw
    // TOML, in openPath() -- this repo no longer decides what any key means.
    // What is left here is the document's presentation: showing the same values
    // in the widgets that the renderer was given.
    //
    // The two halves cannot swap: the SDK's reading is the one that renders, and
    // syncPanelsFromRenderer() reconciles these widgets with it once the scene
    // reports loaded. What this function seeds is the handful of fields the
    // context has no getter for, plus the initial state for panels the user may
    // touch before the load finishes.

    // Remember the whole config, not just the parts the panels can show. Export
    // starts from this so fields with no widget behind them survive a load/save
    // round trip instead of reverting to defaults.
    m_lastConfig = std::make_unique<SceneConfig>(config);

    // A new document, so nothing has been edited in it yet. What the file
    // already said is in m_lastConfig and is carried forward from there.
    m_editedNodes.clear();
    m_editedMaterials.clear();

    m_renderSettingsPanel->setResolution(config.width, config.height);
    m_renderSettingsPanel->setTargetSPP(config.spp);

    m_spectralConfigPanel->setSpectralMode(config.spectralMode);
    m_spectralConfigPanel->setWavelength(config.wavelength_nm);
    m_spectralConfigPanel->setWavelengthRange(config.lambda_min, config.lambda_max,
                                              config.delta_lambda);
    // The lighting panel needs it too -- every illuminant notice it shows is
    // about the mode. It had no way to know here before, so a configuration
    // opened into a quantitative mode with no spectrum said nothing about the
    // black frame it was about to render. No repickNormalisation() to go with
    // it: this is the document speaking, and it is not to be argued with.
    m_lightingPanel->setSpectralMode(config.spectralMode);

    // The shell holds the merged lighting struct; the panels each get the half
    // they own.
    *m_lightingParams = config.lighting;
    m_lightingPanel->setLightingParams(config.lighting);

    m_lightingPanel->setEnvironmentMap(config.environmentMap, config.environmentMapEnabled);

    if (config.cameraOrthographic) {
        applyCameraProjection(true);
    }

    // The illuminant the document names, mapped back onto the panel's choice.
    // The bundled ASTM spectrum is recognised by its column layout rather than
    // its path, so a config that names its own copy still reads as "ASTM".
    {
        LightingPanel::IlluminantChoice choice;
        if (config.solarLutPath.isEmpty()) {
            choice.kind = QStringLiteral("none");
        } else if (config.solarLutPath == QLatin1String("equal_energy")) {
            choice.kind = QStringLiteral("equal_energy");
        } else if (config.solarLutDiffuseIsGlobal &&
                   config.solarLutColumns.size() >= 2 &&
                   config.solarLutColumns[0] == 4 && config.solarLutColumns[1] == 3) {
            choice.kind = QStringLiteral("astm");
        } else {
            choice.kind = QStringLiteral("file");
            choice.path = config.solarLutPath;
        }
        choice.normaliseUnitLuminance =
            config.solarLutNormalise == QLatin1String("unit_luminance");
        m_illuminant = choice;
        m_lightingPanel->setIlluminant(choice);
    }

    m_atmosphericPanel->setPreset(config.atmosphericPreset);
    m_atmosphericPanel->setAtmosphericConfig(config.atmosphere);
    m_skyClearModel = config.clearSkyModel;
    m_skyRelativeHumidity = config.skyRelativeHumidity;
    m_atmosphericPanel->setAnalyticSky(config.clearSkyModel,
                                       config.lighting.atmosphereTemperature_K,
                                       config.skyRelativeHumidity);

    m_sensorPanel->setSensorParams(config.sensorParams);   // Params first,
    m_sensorPanel->setSensorEnabled(config.sensorEnabled);  // then enabled state
    m_thermalProperties.clear();
    for (const MaterialConfig& material : config.materialConfigs) {
        if (material.hasThermal()) {
            m_thermalProperties[material.name] = material.thermal;
        }
    }
    m_thermalEnabled = config.thermalEnabled;
    m_thermalTimeH = config.thermalTimeH;
    m_thermalStartTimeH = config.thermalStartTimeH;
    m_thermalTimestepS = config.thermalTimestepS;
    m_thermalLayers = config.thermalLayers;
    m_thermalInitial = config.thermalInitial;
    m_thermalInitialTemperatureK = config.thermalInitialTemperatureK;
    m_thermalSunIrradiance = config.thermalSunIrradiance;
    m_thermalDiffuseIrradiance = config.thermalDiffuseIrradiance;
    m_thermalExchangeRays = config.thermalExchangeRays;
    m_thermalExchangeTopK = config.thermalExchangeTopK;
    m_thermalCheckpointStrideH = config.thermalCheckpointStrideH;
    m_thermalForcingFile = config.thermalForcingFile;
    m_thermalSunCorrection = config.thermalSunCorrection;
    m_thermalDumpElements = config.thermalDumpElements;
    m_thermalConvectionModel = convectionModelFromName(config.thermalConvectionModel);
    m_thermalConvectionWindA = config.thermalConvectionWindA;
    m_thermalConvectionWindB = config.thermalConvectionWindB;
    m_thermalConvectionFreeC = config.thermalConvectionFreeC;
    m_thermalConvectionReferenceHeightM = config.thermalConvectionReferenceHeightM;
    m_thermalConvectionStableDamping = config.thermalConvectionStableDamping;
    m_thermalLateralConduction = config.thermalLateralConduction;
    m_thermalSunMemoryLags = config.thermalSunMemoryLags;
    m_thermalParameterSensitivities =
        sensitivitiesFromNames(config.thermalParameterSensitivities);

    // Populate the thermal panel from the config. The members above are
    // already the config's, so the panel gets exactly what the dispatchers
    // would build -- plus the two atmosphere values it carries, seeded here
    // from the live [atmosphere] keys rather than left at their defaults.
    {
        quantiloom::ThermalSolveParams tp = currentThermalParams();
        tp.airTemperature_K = static_cast<double>(config.skyAirTemperatureK);
        tp.relativeHumidity = static_cast<double>(config.skyRelativeHumidity);
        m_thermalPanel->setParams(tp);
        m_thermalPanel->setTime(config.thermalTimeH);
        m_thermalPanel->setSolveEnabled(config.thermalEnabled);
        m_thermalPanel->updateStatus(m_vulkanWindow->thermalSolveStatus());
    }

    m_thermographyEnabled = config.thermographyEnabled;
    m_thermographyParams = config.thermography;
    m_sensorPanel->setThermography(config.thermographyEnabled, config.thermography);
    m_vulkanWindow->setThermographyParams(config.thermography);

    // The scene is loaded by ApplyConfig, but the shell still needs to know
    // which file it was and to show the viewport instead of the guidance page.
    QString scenePath;
    if (!config.usdPath.isEmpty()) {
        scenePath = config.usdPath;
        if (!QFileInfo(scenePath).isAbsolute() && !config.baseDir.isEmpty()) {
            scenePath = config.baseDir + "/" + config.usdPath;
        }
    } else if (!config.gltfPath.isEmpty()) {
        scenePath = config.gltfPath;
        if (!QFileInfo(scenePath).isAbsolute() && !config.baseDir.isEmpty()) {
            scenePath = config.baseDir + "/" + config.gltfPath;
        }
    }
    if (!scenePath.isEmpty()) {
        m_currentSceneFile = scenePath;
        m_viewportFrame->setSceneLoaded(true);
    }
}

void MainWindow::syncPanelsFromRenderer() {
    // Reading the renderer back into the widgets is not an edit either.
    const HistorySuppressor noHistory(m_suppressHistory);

    // Read back what the SDK actually applied. Before ApplyConfig existed this
    // was unnecessary because the shell was the thing applying it; now the
    // config's reading happens in the core, and a panel showing this repo's
    // idea of a key rather than the renderer's is exactly the disagreement the
    // whole change is about.
    const quantiloom::LightingParams lighting = m_vulkanWindow->lightingParams();
    *m_lightingParams = lighting;
    m_lightingPanel->setLightingParams(lighting);
    m_atmosphericPanel->setAnalyticSky(lighting.skyEmissivityClear > 0.0f,
                                       lighting.atmosphereTemperature_K,
                                       m_skyRelativeHumidity);

    m_lightingPanel->setEnvironmentMap(
        m_lastConfig ? m_lastConfig->environmentMap : QString(),
        lighting.enableEnvironmentMap != 0);

    m_spectralConfigPanel->setSpectralMode(m_vulkanWindow->spectralMode());
    m_spectralConfigPanel->setWavelength(m_vulkanWindow->wavelength());
    m_lightingPanel->setSpectralMode(m_vulkanWindow->spectralMode());
    m_atmosphericPanel->setAtmosphericConfig(m_vulkanWindow->atmosphericConfig());

    // The camera panel is not in this list: the renderer emits cameraChanged()
    // when it adopts one, which is the single dispatcher for camera state.
}

void MainWindow::collectCurrentConfig(SceneConfig& config) {
    // Load feeds five panels; export used to read back two, so "export
    // configuration" produced something that was not what the window showed.
    // Everything a panel owns is collected here, and everything that is
    // deliberately session-only -- debug mode, display enhancement, the
    // generator's working state -- is listed in src/config/CLAUDE.md rather
    // than left ambiguous.
    // Start from the last config we were given rather than from a default
    // SceneConfig, then let the panels overwrite what they own. The panels do
    // not cover every field -- renderer.seed, the hyperspectral range, the USD
    // path, world scale, material overrides -- and starting empty quietly reset
    // each of those to its default on export, so a config that went through
    // this GUI came out meaning something different.
    if (m_lastConfig) {
        config = *m_lastConfig;
    }

    // Render
    config.width = m_renderSettingsPanel->renderWidth();
    config.height = m_renderSettingsPanel->renderHeight();
    config.spp = m_renderSettingsPanel->spp();
    // From the renderer, not from m_lastConfig: no widget shows the seed, so
    // the carried-forward copy is the only other source and it goes stale the
    // moment anything sets a new one.
    config.samplingSeed = m_vulkanWindow->samplingSeed();

    config.cameraOrthographic = m_vulkanWindow->cameraIsOrthographic();
    config.cameraOrthoHeight = m_vulkanWindow->cameraOrthoHeight();

    // The illuminant, as the core's own keys. The panel holds a choice; these
    // are what a scene file says, and what ResolveSolarLut reads back.
    if (m_illuminant.kind == QLatin1String("none")) {
        config.solarLutPath.clear();
        config.solarLutColumns.clear();
        config.solarLutNormalise.clear();
        config.solarLutDiffuseIsGlobal = false;
    } else if (m_illuminant.kind == QLatin1String("equal_energy")) {
        config.solarLutPath = QStringLiteral("equal_energy");
        config.solarLutColumns.clear();
        config.solarLutDiffuseIsGlobal = false;
        config.solarLutNormalise = m_illuminant.normaliseUnitLuminance
            ? QStringLiteral("unit_luminance") : QString();
    } else if (m_illuminant.kind == QLatin1String("astm")) {
        config.solarLutPath = QStringLiteral("assets/luts/astmg173.csv");
        config.solarLutColumns = {4, 3};
        config.solarLutDiffuseIsGlobal = true;
        config.solarLutNormalise = m_illuminant.normaliseUnitLuminance
            ? QStringLiteral("unit_luminance") : QString();
    } else if (!m_illuminant.path.isEmpty()) {
        config.solarLutPath = m_illuminant.path;
        // The core's libRadtran defaults; a file chosen by hand is assumed to
        // be in that layout, which is the only thing this shell could assume
        // without reading the file itself.
        config.solarLutColumns.clear();
        config.solarLutDiffuseIsGlobal = false;
        config.solarLutNormalise = m_illuminant.normaliseUnitLuminance
            ? QStringLiteral("unit_luminance") : QString();
    }

    // Spectral. The panel used to be described as "tracking these internally",
    // which meant nothing read them back and a mode change never reached the
    // exported file.
    //
    // The panel can only speak for the modes it offers. Multispectral is not
    // one of them -- a cube cannot be rendered progressively, so the viewport
    // previews it as RGB (ModeCatalog::spectralModes leaves it out on
    // purpose). Taking the panel's answer in that case rewrote the document's
    // mode to "rgb" on the first save, which is the whole config quietly
    // ceasing to be the hyperspectral one it was written as. A mode the panel
    // cannot express is not a choice the user made in it, so the document
    // keeps its own.
    const bool panelOwnsMode =
        !m_lastConfig || catalog::spectralModes().contains(m_lastConfig->spectralMode);
    if (panelOwnsMode) {
        config.spectralMode = m_spectralConfigPanel->spectralMode();
    }
    config.wavelength_nm = m_spectralConfigPanel->wavelength();
    config.lambda_min = m_spectralConfigPanel->lambdaMin();
    config.lambda_max = m_spectralConfigPanel->lambdaMax();
    config.delta_lambda = m_spectralConfigPanel->deltaLambda();

    // Camera, read from the renderer so that orbiting with the mouse is
    // exported too -- previously the camera in the file was whatever the
    // config said at load time.
    {
        glm::vec3 position;
        glm::vec3 target;
        glm::vec3 up;
        float fovY = 45.0f;
        m_vulkanWindow->getCameraState(position, target, up, fovY);
        for (int axis = 0; axis < 3; ++axis) {
            config.cameraPosition[axis] = position[axis];
            config.cameraLookAt[axis] = target[axis];
            config.cameraUp[axis] = up[axis];
        }
        config.cameraFovY = fovY;
    }

    // Atmosphere: preset, the nine weather features, and the two analytic
    // terms that live in LightingParams.
    config.atmosphericPreset = m_atmosphericPanel->preset();
    config.atmosphere = m_atmosphericPanel->getAtmosphericConfig();
    config.atmosphericEnabled = config.atmosphericPreset != QLatin1String("disabled");

    // Record the loaded scene under the right key. Only one of the two is
    // written, or a USD scene would be exported as both `gltf` and `usd` now
    // that config.usdPath survives from the loaded config.
    if (!m_currentSceneFile.isEmpty()) {
        if (m_currentSceneFile == config.usdPath ||
            m_currentSceneFile.endsWith(".usd", Qt::CaseInsensitive) ||
            m_currentSceneFile.endsWith(".usda", Qt::CaseInsensitive) ||
            m_currentSceneFile.endsWith(".usdc", Qt::CaseInsensitive) ||
            m_currentSceneFile.endsWith(".usdz", Qt::CaseInsensitive)) {
            config.usdPath = m_currentSceneFile;
            config.gltfPath.clear();
        } else {
            config.gltfPath = m_currentSceneFile;
            config.usdPath.clear();
        }
    }

    // Lighting: the merged struct the shell maintains, which is the same one
    // the renderer is running on.
    config.lighting = *m_lightingParams;

    // Collect sensor settings
    config.clearSkyModel = m_atmosphericPanel->clearSkyEnabled();
    config.skyAirTemperatureK = m_atmosphericPanel->atmosphereTemperatureK();
    config.skyRelativeHumidity = m_atmosphericPanel->relativeHumidity();

    config.sensorEnabled = m_sensorPanel->isSensorEnabled();
    config.sensorParams = m_sensorPanel->getSensorParams();
    config.thermalEnabled = m_thermalEnabled;
    config.thermalTimeH = m_thermalTimeH;
    config.thermalStartTimeH = m_thermalStartTimeH;
    config.thermalTimestepS = m_thermalTimestepS;
    config.thermalLayers = m_thermalLayers;
    config.thermalInitial = m_thermalInitial;
    config.thermalInitialTemperatureK = m_thermalInitialTemperatureK;
    config.thermalSunIrradiance = m_thermalSunIrradiance;
    config.thermalDiffuseIrradiance = m_thermalDiffuseIrradiance;
    config.thermalExchangeRays = m_thermalExchangeRays;
    config.thermalExchangeTopK = m_thermalExchangeTopK;
    config.thermalCheckpointStrideH = m_thermalCheckpointStrideH;
    config.thermalForcingFile = m_thermalForcingFile;
    config.thermalSunCorrection = m_thermalSunCorrection;
    config.thermalDumpElements = m_thermalDumpElements;
    config.thermalConvectionModel = convectionModelName(m_thermalConvectionModel);
    config.thermalConvectionWindA = m_thermalConvectionWindA;
    config.thermalConvectionWindB = m_thermalConvectionWindB;
    config.thermalConvectionFreeC = m_thermalConvectionFreeC;
    config.thermalConvectionReferenceHeightM = m_thermalConvectionReferenceHeightM;
    config.thermalConvectionStableDamping = m_thermalConvectionStableDamping;
    config.thermalLateralConduction = m_thermalLateralConduction;
    config.thermalSunMemoryLags = m_thermalSunMemoryLags;
    config.thermalParameterSensitivities = sensitivityNames(m_thermalParameterSensitivities);

    config.thermographyEnabled = m_sensorPanel->isThermographyEnabled();
    config.thermography = m_sensorPanel->getThermographyParams();

    // Node transforms and material edits, read from the scene the renderer is
    // running on. Only what changed since the document was opened: the model
    // file already places everything else, and repeating it would bury the two
    // lines that matter in a thousand that do not.
    //
    // Nodes and materials are matched by name in the file, so anything unnamed
    // cannot be written down -- an honest limit of the schema rather than
    // something to paper over with an index that the next export of the model
    // would invalidate.
    const quantiloom::Scene* scene = m_vulkanWindow->getScene();
    if (!scene) {
        return;
    }

    // [[duplicates]] and scene.removed_nodes are regenerated from the live
    // scene rather than carried forward: the scene already reflects what the
    // file said (its duplicates were created and its removals tombstoned at
    // load, and m_pastedNodes was re-seeded then), so one pass over the
    // current state covers the file's entries and this session's edits alike.
    config.duplicateConfigs.clear();
    config.removedNodes.clear();

    QList<int> pastedIndices = m_pastedNodes.keys();
    // Creation order, so a copy of a copy always cites a source the core has
    // already resolved when it reads the file top to bottom
    std::sort(pastedIndices.begin(), pastedIndices.end());
    for (const int index : pastedIndices) {
        if (index < 0 || static_cast<size_t>(index) >= scene->nodes.size()) {
            continue;
        }
        const auto& node = scene->nodes[static_cast<size_t>(index)];
        if (!node.active) {
            continue;  // pasted then deleted: simply not written
        }
        DuplicateConfig dup;
        dup.sourceName = m_pastedNodes.value(index);
        dup.name = QString::fromStdString(node.name);
        dup.transform = node.transform;
        config.duplicateConfigs.append(dup);
    }

    for (size_t i = 0; i < scene->nodes.size(); ++i) {
        const auto& node = scene->nodes[i];
        if (node.active || m_pastedNodes.contains(static_cast<int>(i))) {
            continue;
        }
        if (node.name.empty()) {
            qWarning() << "Node" << i
                       << "was deleted but has no name; the deletion cannot be saved";
            continue;
        }
        config.removedNodes.append(QString::fromStdString(node.name));
    }

    for (const int nodeIndex : m_editedNodes) {
        if (nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= scene->nodes.size()) {
            continue;
        }
        // A pasted node's transform lives in its [[duplicates]] entry; a
        // second [[nodes]] block would restate it
        if (m_pastedNodes.contains(nodeIndex)) {
            continue;
        }
        const auto& node = scene->nodes[static_cast<size_t>(nodeIndex)];
        if (node.name.empty()) {
            continue;
        }

        NodeConfig nodeConfig;
        nodeConfig.name = QString::fromStdString(node.name);
        nodeConfig.transform = node.transform;

        // Replace an entry the file already carried for this node rather than
        // adding a second one the core would apply in file order.
        auto existing = std::find_if(
            config.nodeConfigs.begin(), config.nodeConfigs.end(),
            [&nodeConfig](const NodeConfig& n) { return n.name == nodeConfig.name; });
        if (existing != config.nodeConfigs.end()) {
            *existing = nodeConfig;
        } else {
            config.nodeConfigs.append(nodeConfig);
        }
    }

    for (const int materialIndex : m_editedMaterials) {
        if (materialIndex < 0 ||
            static_cast<size_t>(materialIndex) >= scene->materials.size()) {
            continue;
        }
        const auto& material = scene->materials[static_cast<size_t>(materialIndex)];
        if (material.name.empty()) {
            continue;
        }
        const QString name = QString::fromStdString(material.name);

        auto existing = std::find_if(
            config.materialConfigs.begin(), config.materialConfigs.end(),
            [&name](const MaterialConfig& m) { return m.name == name; });
        MaterialConfig& matConfig = (existing != config.materialConfigs.end())
            ? *existing
            : *config.materialConfigs.insert(config.materialConfigs.end(), MaterialConfig{});

        matConfig.name = name;
        matConfig.hasPbr = true;
        matConfig.baseColor = glm::vec3(material.baseColorFactor);
        matConfig.metallic = material.metallicFactor;
        matConfig.roughness = material.roughnessFactor;
        matConfig.emissive = material.emissiveFactor;
        matConfig.irEmissivity = material.irEmissivityCurve.empty()
            ? 0.0f : material.irEmissivityCurve[0].second;
        matConfig.irTransmittance = material.irTransmittanceCurve.empty()
            ? 0.0f : material.irTransmittanceCurve[0].second;
        matConfig.irTemperature_K = material.irTemperature_K;

        // The temperature map, from the material for the same reason the
        // spectral binding is: the editor writes it onto the Material, and
        // this is the only path by which it reaches the config.
        matConfig.temperatureTexture =
            QString::fromStdString(material.temperatureTexturePath);
        matConfig.temperatureScale = material.temperatureScale;
        matConfig.temperatureOffset = material.temperatureOffset;
        matConfig.thermal = m_thermalProperties.value(name);

        // The spectral binding, from the material rather than from whatever
        // the file said: assigning one in the library panel writes it onto the
        // Material, and this is the only path by which it reaches the config.
        // Written for every edited material, including back to empty -- an
        // unbinding has to survive a save as much as a binding does.
        matConfig.spectralMaterialType =
            QString::fromStdString(material.quantiloomMaterialType);
        matConfig.spectralMaterialRefs.clear();
        if (material.HasQuantiloomRef()) {
            matConfig.spectralMaterialRefs
                << QString::fromStdString(material.quantiloomMaterialRef);
            for (const auto& extra : material.quantiloomExtraRefs) {
                matConfig.spectralMaterialRefs << QString::fromStdString(extra);
            }
        }

        // The emission binding, from the material for the same reason. Note
        // that the emissive triple written above is the one the CORE derived
        // from this curve, not the one the author typed -- binding a spectrum
        // replaces it so that the RGB preview and the spectral bands describe
        // one lamp. Saving therefore records the derived colour, which is
        // correct: it is what the scene now is.
        matConfig.emissiveCurve =
            QString::fromStdString(material.emissiveCurveSource);
    }
}

// ============================================================================
// Editing Slots
// ============================================================================

void MainWindow::onViewportClicked(const QPointF& screenPos, Qt::KeyboardModifiers modifiers) {
    // Click-to-select via the SDK's ray query: the pick ray is the raygen
    // shader's own primary ray for the pixel, so what gets selected is what
    // is visibly under the cursor. Ctrl/Shift extend the selection,
    // Blender-style; a click on empty space clears it. Selection is not a
    // scene edit, so nothing here resets accumulation.
    const bool additive = modifiers.testFlag(Qt::ControlModifier) ||
                          modifiers.testFlag(Qt::ShiftModifier);

    const auto hit = m_vulkanWindow->pickScene(screenPos);
    if (hit && hit->hit) {
        const int nodeIndex = static_cast<int>(hit->nodeIndex);
        if (additive) {
            m_selectionManager->toggleSelection(nodeIndex);
        } else {
            m_selectionManager->select(nodeIndex);
        }
    } else if (hit && !additive) {
        // A real miss (the ray reached the sky), not a failed pick
        m_selectionManager->clearSelection();
    }

    // The same click also aims the thermal probe. One gesture rather than a
    // mode to enter, because the question it answers -- why is that surface
    // that temperature -- is asked while looking at the surface, and a probe
    // costs a replay from checkpoints rather than a solve.
    updateThermalProbe(hit ? std::optional<quantiloom::PickResult>(*hit) : std::nullopt);
}

void MainWindow::updateThermalProbe(const std::optional<quantiloom::PickResult>& hit) {
    if (!m_thermalEnabled) {
        m_thermalPanel->clearProbe(tr("The thermal solve is off."));
        return;
    }
    if (!hit || !hit->hit) {
        m_thermalPanel->clearProbe();
        return;
    }

    const auto element = m_vulkanWindow->thermalElementAt(*hit);
    if (!element.has_value()) {
        m_thermalPanel->clearProbe(QString::fromStdString(element.error()));
        return;
    }

    // The whole day rather than the hour on screen: what a probe is for is the
    // shape of the curve, and one point of it is what the viewport already
    // shows. Sampled every quarter hour, which is finer than any forcing file
    // this reads and cheap enough to redo on every click.
    const auto trajectory = m_vulkanWindow->elementTrajectory(element.value(), 0.0, 24.0, 97);
    if (!trajectory.has_value()) {
        m_thermalPanel->clearProbe(QString::fromStdString(trajectory.error()));
        return;
    }
    m_thermalPanel->setProbe(element.value(), trajectory.value());
}

void MainWindow::onSelectionChanged(const QSet<int>& selectedNodes) {
    if (selectedNodes.isEmpty()) {
        m_transformStartStates.clear();
        m_propertiesPanel->showEmptyState();
        return;
    }

    const auto* scene = m_vulkanWindow->getScene();
    if (selectedNodes.size() == 1) {
        const int nodeIndex = *selectedNodes.constBegin();

        QString nodeName = tr("Node %1").arg(nodeIndex);
        if (scene && nodeIndex >= 0 && static_cast<size_t>(nodeIndex) < scene->nodes.size()) {
            const auto& node = scene->nodes[static_cast<size_t>(nodeIndex)];
            if (node.meshIndex < scene->meshes.size()) {
                const auto& mesh = scene->meshes[node.meshIndex];
                if (!mesh.name.empty()) {
                    nodeName = QString::fromStdString(mesh.name);
                }
            }
        }

        showStatusMessage(tr("'%1' selected").arg(nodeName));

        if (scene && nodeIndex >= 0 && static_cast<size_t>(nodeIndex) < scene->nodes.size()) {
            const auto& node = scene->nodes[static_cast<size_t>(nodeIndex)];
            m_transformStartStates.clear();
            m_transformStartStates.push_back({nodeIndex, node.transform});

            // Selecting a node now fills the properties dock instead of
            // leaving the user to find the material tab by hand.
            // A mesh may carry several primitives with different materials;
            // the first one is what "edit this node's material" opens.
            int materialIndex = -1;
            if (node.meshIndex < scene->meshes.size()) {
                const auto& mesh = scene->meshes[node.meshIndex];
                if (!mesh.primitives.empty() &&
                    mesh.primitives.front().materialId < scene->materials.size()) {
                    materialIndex = static_cast<int>(mesh.primitives.front().materialId);
                }
            }
            m_propertiesPanel->showNode(nodeIndex, nodeName, node.transform, materialIndex);
        }
        return;
    }

    showStatusMessage(tr("%1 objects selected").arg(selectedNodes.size()));
    m_propertiesPanel->showMultipleSelection(selectedNodes.size());

    if (scene) {
        m_transformStartStates.clear();
        for (int nodeIndex : selectedNodes) {
            if (nodeIndex >= 0 && static_cast<size_t>(nodeIndex) < scene->nodes.size()) {
                m_transformStartStates.push_back({
                    nodeIndex,
                    scene->nodes[static_cast<size_t>(nodeIndex)].transform
                });
            }
        }
    }
}

void MainWindow::refreshSelectionPanels() {
    const auto* scene = m_vulkanWindow->getScene();
    if (!scene) {
        return;
    }
    const QSet<int>& selected = m_selectionManager->selectedNodes();
    if (selected.size() != 1) {
        return;  // multi-selection shows a count, nothing transform-shaped
    }
    const int nodeIndex = *selected.constBegin();
    if (nodeIndex >= 0 && static_cast<size_t>(nodeIndex) < scene->nodes.size()) {
        m_propertiesPanel->updateNodeTransform(
            scene->nodes[static_cast<size_t>(nodeIndex)].transform);
    }
}

void MainWindow::onGizmoDragStarted() {
    // Snapshot at PRESS time, not selection time: a transform typed into the
    // properties panel between selecting and dragging would otherwise be
    // silently reverted by the first gizmo move.
    const auto* scene = m_vulkanWindow->getScene();
    if (!scene) {
        return;
    }
    ++m_transformGestureId;
    m_transformStartStates.clear();
    for (int nodeIndex : m_selectionManager->selectedNodes()) {
        if (nodeIndex >= 0 && static_cast<size_t>(nodeIndex) < scene->nodes.size()) {
            m_transformStartStates.push_back({
                nodeIndex,
                scene->nodes[static_cast<size_t>(nodeIndex)].transform
            });
        }
    }
}

void MainWindow::onGizmoDragCancelled() {
    // Escape: put every node back exactly where the drag found it, keep no
    // record -- no undo entry, no edited-nodes registration, no signal to the
    // document. One full rebuild restores full trace quality.
    for (const auto& state : m_transformStartStates) {
        m_vulkanWindow->setNodeTransformInteractive(state.nodeIndex,
                                                    state.originalTransform);
        if (m_transformStartStates.size() == 1) {
            m_propertiesPanel->updateNodeTransform(state.originalTransform);
        }
    }
    m_vulkanWindow->finalizeInteractiveEdit();
}

void MainWindow::onGizmoTransformChanged(const glm::vec3& translation,
                                          const glm::quat& rotation,
                                          const glm::vec3& scale) {
    Q_UNUSED(translation);
    Q_UNUSED(rotation);
    Q_UNUSED(scale);

    const auto* scene = m_vulkanWindow->getScene();
    if (!scene || m_transformStartStates.empty()) {
        return;
    }

    // Apply every node, then refit the TLAS once for the whole batch: the
    // in-place refit is what keeps a drag at interactive framerates (the old
    // path did a full teardown-and-rebuild per node per mouse move).
    for (const auto& state : m_transformStartStates) {
        glm::mat4 newTransform = m_transformGizmo->applyDelta(state.originalTransform);
        m_vulkanWindow->setNodeTransformInteractive(state.nodeIndex, newTransform);
        if (m_transformStartStates.size() == 1) {
            m_propertiesPanel->updateNodeTransform(newTransform);
        }
    }
    m_vulkanWindow->refitAfterInteractiveEdit();

    setSceneModified(true);
}

void MainWindow::onGizmoTransformFinished() {
    const auto* scene = m_vulkanWindow->getScene();
    if (!scene || m_transformStartStates.empty()) {
        return;
    }

    if (m_transformStartStates.size() == 1) {
        const auto& state = m_transformStartStates[0];
        if (state.nodeIndex >= 0 && static_cast<size_t>(state.nodeIndex) < scene->nodes.size()) {
            glm::mat4 newTransform = scene->nodes[static_cast<size_t>(state.nodeIndex)].transform;
            if (newTransform != state.originalTransform) {
                auto cmd = std::make_unique<TransformNodeCommand>(
                    m_vulkanWindow, state.nodeIndex, state.originalTransform, newTransform);
                cmd->setMergeGesture(m_transformGestureId);
                m_undoStack->push(std::move(cmd));
                // The gizmo bypasses applyNodeTransform -- it applied the
                // moves live during the drag -- so the record of what changed
                // is kept here, or a dragged node would vanish from the saved
                // document while an identically typed-in one would not.
                m_editedNodes.insert(state.nodeIndex);
            }
        }
    } else {
        std::vector<MultiTransformCommand::NodeTransform> transforms;
        for (const auto& state : m_transformStartStates) {
            if (state.nodeIndex >= 0 && static_cast<size_t>(state.nodeIndex) < scene->nodes.size()) {
                glm::mat4 newTransform = scene->nodes[static_cast<size_t>(state.nodeIndex)].transform;
                if (newTransform != state.originalTransform) {
                    transforms.push_back({state.nodeIndex, state.originalTransform, newTransform});
                }
            }
        }
        if (!transforms.empty()) {
            auto cmd = std::make_unique<MultiTransformCommand>(m_vulkanWindow, transforms);
            cmd->setMergeGesture(m_transformGestureId);
            m_undoStack->push(std::move(cmd));
            for (const auto& moved : transforms) {
                m_editedNodes.insert(moved.nodeIndex);
            }
        }
    }

    // Only nodes with a name survive a save -- collectCurrentConfig writes
    // [[nodes]] entries matched by name -- so say so when an edit will not
    for (const auto& state : m_transformStartStates) {
        if (static_cast<size_t>(state.nodeIndex) < scene->nodes.size() &&
            scene->nodes[static_cast<size_t>(state.nodeIndex)].name.empty()) {
            qWarning() << "Node" << state.nodeIndex
                       << "has no name; its transform edit will not persist in the saved config";
        }
    }

    onSelectionChanged(m_selectionManager->selectedNodes());
}

void MainWindow::onUndoRedoChanged() {
    m_undoAction->setEnabled(m_undoStack->canUndo());
    m_redoAction->setEnabled(m_undoStack->canRedo());

    // The action description goes *into* the label rather than replacing it.
    // Overwriting the text with the stack's own description dropped the
    // mnemonic and pulled the wording into a different translation context.
    const QString undoWhat = m_undoStack->canUndo() ? m_undoStack->undoText() : QString();
    const QString redoWhat = m_undoStack->canRedo() ? m_undoStack->redoText() : QString();
    m_undoAction->setText(undoWhat.isEmpty() ? tr("&Undo") : tr("&Undo %1").arg(undoWhat));
    m_redoAction->setText(redoWhat.isEmpty() ? tr("&Redo") : tr("&Redo %1").arg(redoWhat));
}

void MainWindow::onViewportHovered(int x, int y) {
    const auto debugMode = m_vulkanWindow->getDebugMode();
    if (debugMode == quantiloom::DebugVisualizationMode::None) {
        // A thermal band has something to say without a debug mode: what a
        // camera would have displayed here. Same inversion the CLI writes into
        // _tapp.exr, through the same SDK entry point.
        double kelvin = 0.0;
        if (m_vulkanWindow->readApparentTemperature(x, y, kelvin)) {
            m_debugValueLabel->setText(tr("(%1,%2) %3 K").arg(x).arg(y).arg(kelvin, 0, 'f', 1));
            return;
        }
        m_debugValueLabel->setText(tr("Select a debug mode to inspect pixels"));
        return;
    }

    glm::vec4 pixelValue;
    if (m_vulkanWindow->readDebugPixel(x, y, pixelValue)) {
        // The name comes from the catalogue and the numbers from the renderer,
        // so the label is translated once and formatted once.
        const QString formatted = tr("%1 %2")
            .arg(catalog::debugModeName(debugMode),
                 m_vulkanWindow->formatDebugValue(pixelValue));
        m_debugValueLabel->setText(tr("(%1,%2) %3").arg(x).arg(y).arg(formatted));
        m_debugVisualizationPanel->setPixelReading(x, y, formatted);
    } else {
        m_debugValueLabel->setText(tr("(%1,%2) read failed").arg(x).arg(y));
        m_debugVisualizationPanel->setPixelReadFailed(x, y);
    }
}
