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
#include "panels/DebugVisualizationPanel.hpp"
#include "panels/AtmosphericPanel.hpp"
#include "panels/SensorPanel.hpp"
#include "panels/DisplayEnhancementPanel.hpp"
#include "panels/SpectralMaterialGenPanel.hpp"
#include "panels/PropertiesPanel.hpp"
#include "panels/CameraPanel.hpp"
#include "config/ConfigManager.hpp"
#include "editing/SelectionManager.hpp"
#include "editing/TransformGizmo.hpp"
#include "editing/UndoStack.hpp"
#include "editing/Commands.hpp"
#include "dialogs/PreferencesDialog.hpp"
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
constexpr int  kMaxRecentFiles = 8;
constexpr auto kGeometryKey    = "window/geometry";
constexpr auto kStateVersionKey = "window/state_version";

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

    // Progressive accumulation is already running when the window opens, so
    // "start" is the entry that has nothing to do yet.
    m_startRenderAction->setEnabled(false);

    retranslateUi();
    restoreWindowState();
    m_workspaces->activateInitial();
    updateWindowTitle();

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
        case Qt::Key_X:     action = m_axisXAction;      break;
        case Qt::Key_Y:     action = m_axisYAction;      break;
        case Qt::Key_Z:     action = m_axisZAction;      break;
        case Qt::Key_Space: action = m_localSpaceAction; break;
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

    menu->addSeparator();

    auto addAxis = [&](TransformGizmo::Axis axis, const QKeySequence& shortcut) {
        QAction* action = menu->addAction(QString());
        action->setCheckable(true);
        action->setShortcut(shortcut);
        connect(action, &QAction::triggered, this, [this, axis]() {
            m_transformGizmo->toggleAxisConstraint(axis);
        });
        return action;
    };

    m_axisXAction = addAxis(TransformGizmo::Axis::X, QKeySequence(Qt::Key_X));
    m_axisYAction = addAxis(TransformGizmo::Axis::Y, QKeySequence(Qt::Key_Y));
    m_axisZAction = addAxis(TransformGizmo::Axis::Z, QKeySequence(Qt::Key_Z));

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
    m_vulkanWindow->setSpectralMode(mode);

    if (QAction* action = m_spectralActions.value(static_cast<int>(mode), nullptr)) {
        action->setChecked(true);
    }
    if (m_spectralCombo) {
        const QSignalBlocker blocker(m_spectralCombo);
        m_spectralCombo->setCurrentIndex(m_spectralCombo->findData(static_cast<int>(mode)));
    }
    m_spectralConfigPanel->setSpectralMode(mode);
    m_viewportFrame->setSpectralMode(mode);

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
    showStatusMessage(tr("Target samples: %1").arg(spp));
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
    m_debugVisualizationPanel = new DebugVisualizationPanel();
    m_atmosphericPanel = new AtmosphericPanel();
    m_sensorPanel = new SensorPanel();
    m_displayEnhancementPanel = new DisplayEnhancementPanel();
    m_spectralMaterialGenPanel = new SpectralMaterialGenPanel();

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
    createPanelDock(m_debugVisualizationPanel, Qt::RightDockWidgetArea);

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
    connect(m_sceneTreePanel, &SceneTreePanel::materialSelected,
            this, &MainWindow::onMaterialSelected);

    connect(m_materialEditorPanel, &MaterialEditorPanel::materialChanged,
            this, &MainWindow::onMaterialChanged);

    // Typed transforms take the same route as a gizmo drag, down to the undo
    // command, so Ctrl+Z means one thing regardless of how the node was moved.
    connect(m_propertiesPanel, &PropertiesPanel::nodeTransformEdited,
            this, &MainWindow::onNodeTransformEdited);
    connect(m_propertiesPanel, &PropertiesPanel::materialRequested,
            this, &MainWindow::onMaterialSelected);

    connect(m_cameraPanel, &CameraPanel::cameraEdited,
            this, [this](const glm::vec3& position, const glm::vec3& target) {
                glm::vec3 currentPosition, currentTarget, up;
                float fovY = 45.0f;
                m_vulkanWindow->getCameraState(currentPosition, currentTarget, up, fovY);
                m_vulkanWindow->setCamera(position, target, up, fovY);
                setSceneModified(true);
            });
    connect(m_cameraPanel, &CameraPanel::fovEdited, this, [this](float fovY) {
        m_vulkanWindow->setCameraFovY(fovY);
        setSceneModified(true);
    });
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

    connect(m_renderSettingsPanel, &RenderSettingsPanel::sppChanged,
            this, &MainWindow::onSppChanged);
    connect(m_renderSettingsPanel, &RenderSettingsPanel::resetAccumulationRequested,
            this, &MainWindow::onResetAccumulation);
    // The panel's export button used to raise a save dialog and then do
    // nothing at all -- its signal had no receiver anywhere. It now runs the
    // one export path, the same one File ▸ Export Image uses.
    connect(m_renderSettingsPanel, &RenderSettingsPanel::exportRequested,
            this, &MainWindow::onExportImage);

    connect(m_spectralConfigPanel, &SpectralConfigPanel::spectralModeChanged,
            this, &MainWindow::onSpectralModeChanged);
    connect(m_spectralConfigPanel, &SpectralConfigPanel::wavelengthChanged,
            this, &MainWindow::onWavelengthChanged);

    connect(m_debugVisualizationPanel, &DebugVisualizationPanel::debugModeChanged,
            this, &MainWindow::onDebugModeChanged);

    // Atmospheric panel signals
    connect(m_atmosphericPanel, &AtmosphericPanel::presetChanged,
            this, [this](const QString& preset) {
                showStatusMessage(tr("Atmospheric preset: %1").arg(preset));
            });
    connect(m_atmosphericPanel, &AtmosphericPanel::configChanged,
            this, [this](const quantiloom::AtmosphereNNConfig& config) {
                m_vulkanWindow->setAtmosphericConfig(config);
                setSceneModified(true);
            });
    // The two analytic terms moved here from the lighting panel; they still
    // belong to LightingParams, so they are merged into the copy the shell
    // holds rather than sent on their own.
    connect(m_atmosphericPanel, &AtmosphericPanel::analyticTermsChanged,
            this, [this](float transmittance, float temperatureK) {
                m_lightingParams->transmittance = transmittance;
                m_lightingParams->atmosphereTemperature_K = temperatureK;
                pushLightingParams();
            });

    // Sensor panel signals
    connect(m_sensorPanel, &SensorPanel::enabledChanged,
            this, [this](bool enabled) {
                m_vulkanWindow->setSensorEnabled(enabled);
                showStatusMessage(enabled ? tr("Sensor simulation enabled")
                                          : tr("Sensor simulation disabled"));
            });
    connect(m_sensorPanel, &SensorPanel::paramsChanged,
            this, [this](const quantiloom::SensorParams& params) {
                m_vulkanWindow->setSensorParams(params);
                showStatusMessage(tr("Sensor parameters updated"));
            });

    // Display enhancement panel signals
    connect(m_displayEnhancementPanel, &DisplayEnhancementPanel::enhancementChanged,
            this, [this](bool enabled, float clipLimit, int tileSize, bool luminanceOnly) {
                m_displayEnhancementEnabled = enabled;
                m_claheClipLimit = clipLimit;
                m_claheTileSize = tileSize;
                m_claheLuminanceOnly = luminanceOnly;
                m_vulkanWindow->setDisplayEnhancement(enabled, clipLimit, tileSize, luminanceOnly);
                if (m_displayEnhancementAction) {
                    const QSignalBlocker blocker(m_displayEnhancementAction);
                    m_displayEnhancementAction->setChecked(enabled);
                }
                showStatusMessage(enabled
                    ? tr("Display enhancement on (CLAHE: clip %1, %2x%2 tiles)")
                        .arg(clipLimit, 0, 'f', 1).arg(tileSize)
                    : tr("Display enhancement off"));
            });
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
                showStatusMessage(tr("Workspace: %1")
                                      .arg(WorkspaceManager::workspaceTitle(id)));
            });
}


// ============================================================================
// Status bar
// ============================================================================

void MainWindow::setupStatusBar() {
    m_statusLabel = new QLabel();
    m_fpsLabel = new QLabel();
    m_sampleCountLabel = new QLabel();
    m_editModeLabel = new QLabel();
    m_styling.bind([this] { uistyle::applyChipStyle(m_editModeLabel, uistyle::ChipTone::Accent); });
    m_debugValueLabel = new QLabel();
    m_debugValueLabel->setMinimumWidth(250);
    m_styling.bind([this] { uistyle::applyMonospaceStyle(m_debugValueLabel); });
    m_renderProgress = new QProgressBar();
    m_renderProgress->setMaximumWidth(200);
    m_renderProgress->setRange(0, 100);
    m_renderProgress->setValue(0);

    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_debugValueLabel);
    statusBar()->addPermanentWidget(m_editModeLabel);
    statusBar()->addPermanentWidget(m_sampleCountLabel);
    statusBar()->addPermanentWidget(m_fpsLabel);
    statusBar()->addPermanentWidget(m_renderProgress);

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
        m_renderProgress->setValue(0);
        return;
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
    connect(m_vulkanWindow, &QuantiloomVulkanWindow::sceneLoaded,
            this, [this](bool success, const QString& message) {
                if (success) {
                    // The open is only now known to have worked, which is the
                    // first point at which the file is worth remembering.
                    rememberRecentFile(m_pendingOpenPath);
                    m_pendingOpenPath.clear();
                    updatePanelsFromScene();
                    applySpectralConfig();
                    applyPendingMaterialConfigs();
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
                m_viewportFrame->setBusyMessage(description);
                showStatusMessage(description, 0);
            });
    connect(m_vulkanWindow, &QuantiloomVulkanWindow::longOperationFinished,
            this, [this]() {
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

    // Connect selection changes
    connect(m_selectionManager, &SelectionManager::selectionChanged,
            this, &MainWindow::onSelectionChanged);

    // Connect gizmo transform changes
    connect(m_transformGizmo, &TransformGizmo::transformChanged,
            this, &MainWindow::onGizmoTransformChanged);
    connect(m_transformGizmo, &TransformGizmo::transformFinished,
            this, &MainWindow::onGizmoTransformFinished);

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

    connect(m_transformGizmo, &TransformGizmo::axisConstraintChanged,
            this, [this](TransformGizmo::Axis axis) {
                const QSignalBlocker bx(m_axisXAction);
                const QSignalBlocker by(m_axisYAction);
                const QSignalBlocker bz(m_axisZAction);
                // XYZ means unconstrained, so none of the three is ticked.
                m_axisXAction->setChecked(axis == TransformGizmo::Axis::X);
                m_axisYAction->setChecked(axis == TransformGizmo::Axis::Y);
                m_axisZAction->setChecked(axis == TransformGizmo::Axis::Z);
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
    m_axisXAction->setText(tr("Constrain to &X"));
    m_axisYAction->setText(tr("Constrain to &Y"));
    m_axisZAction->setText(tr("Constrain to &Z"));
    m_localSpaceAction->setText(tr("&Local Space"));
    m_localSpaceAction->setToolTip(tr("Transform along the object's own axes instead of the world's"));
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

    m_renderMenu->setTitle(tr("&Render"));
    m_startRenderAction->setText(tr("&Start Render"));
    m_stopRenderAction->setText(tr("S&top Render"));
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

    m_toolsMenu->setTitle(tr("&Tools"));
    m_spectralGenAction->setText(tr("Spectral Material &Generator"));

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
    m_fpsLabel->setText(tr("FPS: --"));
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
        applyConfig(config);
        setSceneModified(false);
        setCurrentDocument(filePath);
        showStatusMessage(tr("Loaded configuration: %1").arg(QFileInfo(filePath).fileName()));
        return true;
    }

    // A bare model has no configuration behind it, so there is no document to
    // save over -- Save will ask for a destination the first time.
    m_currentSceneFile = filePath;
    m_currentConfigFile.clear();
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

    bool success = false;
    if (fileName.endsWith(QLatin1String(".exr"), Qt::CaseInsensitive)) {
        success = quantiloom::ImageIO::WriteEXR(fileName.toStdString(), *image);
    } else if (fileName.endsWith(QLatin1String(".png"), Qt::CaseInsensitive)) {
        success = quantiloom::ImageIO::WritePNG(fileName.toStdString(), *image);
    } else {
        if (!fileName.contains(QLatin1Char('.'))) {
            fileName += QStringLiteral(".exr");
        }
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
    m_startRenderAction->setEnabled(false);
    m_stopRenderAction->setEnabled(true);
    showStatusMessage(tr("Rendering from scratch to %1 samples")
                          .arg(m_renderSettingsPanel->spp()));
}

void MainWindow::onStopRender() {
    m_vulkanWindow->setRenderPaused(true);
    m_startRenderAction->setEnabled(true);
    m_stopRenderAction->setEnabled(false);
    showStatusMessage(tr("Rendering stopped at %1 samples")
                          .arg(m_vulkanWindow->currentSampleCount()));
}

void MainWindow::onResetCamera() {
    m_vulkanWindow->resetCamera();
    showStatusMessage(tr("Camera reset"));
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
        tr("<h3>Quantiloom</h3>"
           "<p>Version 0.1.2</p>"
           "<p>A spectral renderer with hardware ray tracing support.</p>"
           "<p>Features:</p>"
           "<ul>"
           "<li>Hardware ray tracing</li>"
           "<li>Spectral rendering</li>"
           "<li>PBR materials with spectral extensions</li>"
           "<li>Atmospheric scattering</li>"
           "</ul>"
           "<p>Copyright (c) 2025-2026 blitzcolo</p>"));
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

void MainWindow::onFrameRendered(float frameTimeMs, uint32_t sampleCount) {
    const float fps = (frameTimeMs > 0.0f) ? (1000.0f / frameTimeMs) : 0.0f;
    m_fpsLabel->setText(tr("FPS: %1").arg(fps, 0, 'f', 1));
    m_sampleCountLabel->setText(tr("Samples: %1").arg(sampleCount));

    m_renderSettingsPanel->setSampleCount(sampleCount);
    updateRenderProgress();
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
        m_propertiesPanel->showMaterial();
        m_spectralMaterialGenPanel->setCurrentMaterialIndex(materialIndex);
        if (QDockWidget* dock = m_docks.value(QStringLiteral("properties"), nullptr)) {
            dock->show();
            dock->raise();
        }
        showStatusMessage(tr("Material '%1' selected")
                              .arg(QString::fromStdString(material.name)));
    }
}

void MainWindow::onMaterialChanged(int index, const quantiloom::Material& material) {
    m_vulkanWindow->updateMaterial(index, material);
    setSceneModified(true);
    showStatusMessage(tr("Material modified"));
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

    pushLightingParams();
    showStatusMessage(tr("Lighting updated"));
}

void MainWindow::pushLightingParams() {
    m_vulkanWindow->setLightingParams(*m_lightingParams);
    setSceneModified(true);
}

void MainWindow::onNodeTransformEdited(int nodeIndex, const glm::mat4& transform) {
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

    setSceneModified(true);
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
    m_vulkanWindow->setWavelength(wavelength_nm);
    setSceneModified(true);
    showStatusMessage(tr("Wavelength: %1 nm").arg(wavelength_nm, 0, 'f', 0));
}

void MainWindow::onDebugModeChanged(quantiloom::DebugVisualizationMode mode) {
    applyDebugMode(mode);
}

void MainWindow::onResetAccumulation() {
    m_vulkanWindow->resetAccumulation();
    updateRenderProgress();
    showStatusMessage(tr("Accumulation reset"));
}

void MainWindow::updatePanelsFromScene() {
    const quantiloom::Scene* scene = m_vulkanWindow->getScene();

    m_sceneTreePanel->setScene(scene);
    m_spectralMaterialGenPanel->setScene(scene);
    m_materialEditorPanel->clear();
    m_propertiesPanel->showEmptyState();

    if (scene) {
        m_lightingPanel->setLightingParams(quantiloom::CreateDefaultLightingParams());
        m_spectralConfigPanel->setWavelengthRange(
            scene->lambda_min, scene->lambda_max, scene->delta_lambda);
    }
}

// ============================================================================
// Config round trip
// ============================================================================

void MainWindow::applyConfig(const SceneConfig& config) {
    // Remember the whole config, not just the parts the panels can show. Export
    // starts from this so fields with no widget behind them survive a load/save
    // round trip instead of reverting to defaults.
    m_lastConfig = std::make_unique<SceneConfig>(config);

    // Apply render settings
    m_renderSettingsPanel->setResolution(config.width, config.height);
    m_renderSettingsPanel->setTargetSPP(config.spp);
    m_vulkanWindow->setSPP(config.spp);
    m_vulkanWindow->setSamplingSeed(config.samplingSeed);

    // Apply spectral settings
    m_spectralConfigPanel->setSpectralMode(config.spectralMode);
    m_spectralConfigPanel->setWavelength(config.wavelength_nm);
    m_spectralConfigPanel->setWavelengthRange(config.lambda_min, config.lambda_max, config.delta_lambda);
    m_vulkanWindow->setSpectralMode(config.spectralMode);
    m_vulkanWindow->setWavelength(config.wavelength_nm);

    // Apply lighting settings. The shell holds the merged struct; the panels
    // each get the half they own.
    *m_lightingParams = config.lighting;
    m_lightingPanel->setLightingParams(config.lighting);
    m_vulkanWindow->setLightingParams(config.lighting);

    // Apply atmospheric configuration
    m_atmosphericPanel->setPreset(config.atmosphericPreset);
    m_atmosphericPanel->setAtmosphericConfig(config.atmosphere);
    m_atmosphericPanel->setAnalyticTerms(config.lighting.transmittance,
                                         config.lighting.atmosphereTemperature_K);
    // The whole struct, not just the preset name: ConfigManager parses all nine
    // weather features, and sending only the preset threw every override away
    // between the panel and the GPU.
    m_vulkanWindow->setAtmosphericConfig(config.atmosphere);

    // Apply sensor configuration
    m_sensorPanel->setSensorParams(config.sensorParams);  // Always set params first
    m_sensorPanel->setSensorEnabled(config.sensorEnabled); // Then set enabled state
    m_vulkanWindow->setSensorParams(config.sensorParams);
    m_vulkanWindow->setSensorEnabled(config.sensorEnabled);

    // Load scene file (glTF or USD)
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
        m_vulkanWindow->loadScene(scenePath);
    }

    // Apply environment map (IBL) - do this after scene load starts
    if (!config.environmentMap.isEmpty()) {
        QString envPath = config.environmentMap;
        if (!QFileInfo(envPath).isAbsolute() && !config.baseDir.isEmpty()) {
            envPath = config.baseDir + "/" + config.environmentMap;
        }
        if (!m_vulkanWindow->loadEnvironmentMap(envPath)) {
            qWarning() << "Failed to load environment map:" << envPath;
        }
    }

    // Apply camera settings (after scene load so renderer is ready)
    glm::vec3 camPos(config.cameraPosition[0], config.cameraPosition[1], config.cameraPosition[2]);
    glm::vec3 camLookAt(config.cameraLookAt[0], config.cameraLookAt[1], config.cameraLookAt[2]);
    glm::vec3 camUp(config.cameraUp[0], config.cameraUp[1], config.cameraUp[2]);
    m_vulkanWindow->setCamera(camPos, camLookAt, camUp, config.cameraFovY);

    // Store material configs for application after scene load
    m_pendingMaterialConfigs = config.materialConfigs;
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

    // Spectral. The panel used to be described as "tracking these internally",
    // which meant nothing read them back and a mode change never reached the
    // exported file.
    config.spectralMode = m_spectralConfigPanel->spectralMode();
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
    config.sensorEnabled = m_sensorPanel->isSensorEnabled();
    config.sensorParams = m_sensorPanel->getSensorParams();
}

void MainWindow::applySpectralConfig() {
    const quantiloom::Config* config = m_configManager->getRawConfig();
    if (!config) {
        return;
    }

    // Illuminant first: a reflectance curve describes what a surface does to light,
    // so without a solar spectrum the quantitative path has nothing to reflect.
    if (config->Has("lighting.solar_lut")) {
        const auto path = config->Get<std::string>("lighting.solar_lut");
        auto loaded = quantiloom::SpectralIO::LoadLibRadtranSunAndSky(path, "nm");
        if (loaded.has_value()) {
            const auto& [sun, sky] = loaded.value();
            m_vulkanWindow->setSolarSpectralLUT(sun, sky);
            QL_LOG_INFO("Solar LUT loaded from {}", path);
        } else {
            // Core logger rather than qWarning: with no console attached Qt sends
            // qWarning to the debugger, so a failure here would be invisible in the
            // same log that carries what it is reporting about.
            QL_LOG_WARN("Solar LUT failed to load: {}", loaded.error());
        }
    }

    if (!config->HasSection("spectral_curves")) {
        return;
    }

    // [spectral_curves] maps a material name to a measured reflectance CSV -- the
    // same section the CLI reads, so a config renders the same either way.
    const auto* scene = m_vulkanWindow->getScene();
    if (!scene) {
        return;
    }

    std::unordered_map<std::string, int> nameToCurve;
    for (const auto& [materialName, csvPath] : config->GetSection("spectral_curves")) {
        auto samples = quantiloom::SpectralIO::LoadSpectralCurveCSV(csvPath);
        if (!samples.has_value()) {
            QL_LOG_WARN("Spectral curve for '{}' failed to load: {}", materialName,
                        samples.error());
            continue;
        }

        quantiloom::SpectralCurve curve;
        curve.samples = samples.value();

        const int index = m_vulkanWindow->addSpectralCurve(curve);
        if (index >= 0) {
            nameToCurve[materialName] = index;
        }
    }

    if (nameToCurve.empty()) {
        return;
    }

    // The curves are on the GPU; a material only uses one once its index says so.
    for (size_t i = 0; i < scene->materials.size(); ++i) {
        const auto found = nameToCurve.find(scene->materials[i].name);
        if (found == nameToCurve.end()) {
            continue;
        }

        quantiloom::Material updated = scene->materials[i];
        updated.spectralReflectanceCurveIndex = found->second;
        m_vulkanWindow->updateMaterial(static_cast<quantiloom::u32>(i), updated);

        QL_LOG_INFO("Material '{}' -> spectral curve index {}", updated.name,
                    found->second);
    }

    showStatusMessage(tr("Loaded %1 spectral curve(s)").arg(nameToCurve.size()));
}

void MainWindow::applyPendingMaterialConfigs() {
    if (m_pendingMaterialConfigs.isEmpty()) {
        return;
    }

    const auto* scene = m_vulkanWindow->getScene();
    if (!scene) {
        return;
    }

    for (const auto& matConfig : m_pendingMaterialConfigs) {
        // Find material by name
        for (size_t i = 0; i < scene->materials.size(); ++i) {
            const auto& material = scene->materials[i];
            if (QString::fromStdString(material.name) == matConfig.name) {
                quantiloom::Material modified = material;

                // Set IR curves with constant values across IR bands
                const float mwir_nm = 4000.0f;
                const float lwir_nm = 10000.0f;

                if (matConfig.irEmissivity > 0.0f) {
                    modified.irEmissivityCurve.clear();
                    modified.irEmissivityCurve.push_back({mwir_nm, matConfig.irEmissivity});
                    modified.irEmissivityCurve.push_back({lwir_nm, matConfig.irEmissivity});
                }

                if (matConfig.irTransmittance > 0.0f) {
                    modified.irTransmittanceCurve.clear();
                    modified.irTransmittanceCurve.push_back({mwir_nm, matConfig.irTransmittance});
                    modified.irTransmittanceCurve.push_back({lwir_nm, matConfig.irTransmittance});
                }

                // Compute and set reflectance from energy conservation
                float reflectance = 1.0f - matConfig.irEmissivity - matConfig.irTransmittance;
                if (reflectance > 0.0f) {
                    modified.irReflectanceCurve.clear();
                    modified.irReflectanceCurve.push_back({mwir_nm, reflectance});
                    modified.irReflectanceCurve.push_back({lwir_nm, reflectance});
                }

                modified.irTemperature_K = matConfig.irTemperature_K;

                m_vulkanWindow->updateMaterial(static_cast<int>(i), modified);
                break;
            }
        }
    }

    m_pendingMaterialConfigs.clear();
}

// ============================================================================
// Editing Slots
// ============================================================================

void MainWindow::onViewportClicked(const QPointF& screenPos) {
    // Proper picking would cast a ray; selection currently goes through the
    // scene tree.
    Q_UNUSED(screenPos);
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

    for (const auto& state : m_transformStartStates) {
        glm::mat4 newTransform = m_transformGizmo->applyDelta(state.originalTransform);
        m_vulkanWindow->setNodeTransform(state.nodeIndex, newTransform);
        if (m_transformStartStates.size() == 1) {
            m_propertiesPanel->updateNodeTransform(newTransform);
        }
    }

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
                m_undoStack->push(std::move(cmd));
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
            m_undoStack->push(std::move(cmd));
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
