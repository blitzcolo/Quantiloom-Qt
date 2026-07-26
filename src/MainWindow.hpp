/**
 * @file MainWindow.hpp
 * @brief Main application window with Vulkan viewport and parameter panels
 *
 * @author blitzcolo
 */

#pragma once

#include "ui/UiStyle.hpp"

#include <QMainWindow>
#include <QVulkanInstance>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <memory>
#include <vector>

#include <core/Types.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

QT_BEGIN_NAMESPACE
class QActionGroup;
class QComboBox;
class QDockWidget;
class QTabWidget;
class QStatusBar;
class QProgressBar;
class QLabel;
class QAction;
class QMenu;
class QScrollArea;
class QToolBar;
QT_END_NAMESPACE

namespace quantiloom {
class ExternalRenderContext;
struct LightingParams;
struct Material;
struct ComplexRefractiveIndex;
}

class QuantiloomVulkanWindow;
class PanelBase;
class PropertiesPanel;
class CameraPanel;
class SceneTreePanel;
class MaterialEditorPanel;
class LightingPanel;
class RenderSettingsPanel;
class SpectralConfigPanel;
class DebugVisualizationPanel;
class AtmosphericPanel;
class SensorPanel;
class DisplayEnhancementPanel;
class SpectralMaterialGenPanel;
class ConfigManager;
class ViewportFrame;
class WorkspaceManager;
class SelectionManager;
class TransformGizmo;
class UndoStack;
struct SceneConfig;

/**
 * @class MainWindow
 * @brief Main application window with 3D viewport and parameter panels
 *
 * Layout:
 * - Center: Vulkan 3D viewport (QuantiloomVulkanWindow)
 * - Left: Parameter panels in tabbed dock widget
 * - Bottom: Status bar with render info
 *
 * The document this window edits is the TOML scene configuration: File ▸ Open
 * accepts a config or a bare model, and Save writes a config back. There is
 * deliberately no second "import/export configuration" pair — two entries
 * differing only in wording for the same file is what the previous menu had.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QVulkanInstance* vulkanInstance, QWidget* parent = nullptr);
    ~MainWindow() override;

    /// Open a path given on the command line, exactly as File ▸ Open would.
    void openFromCommandLine(const QString& filePath);

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    /// Routes the viewport's transform keys into the same QActions the menu
    /// uses, so there is one dispatcher rather than two.
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    // File menu actions
    void onOpenScene();
    bool onSaveScene();
    bool onSaveSceneAs();
    void onExportImage();

    // Render menu actions
    void onStartRender();
    void onStopRender();

    // View menu actions
    void onResetCamera();
    void onResetLayout();
    void onTakeScreenshot();

    // Help menu actions
    void onAbout();
    void onShowShortcuts();
    void onShowDebugReference();

    // Edit menu actions
    void onPreferences();

    // Status updates
    void onFrameRendered(float frameTimeMs, uint32_t sampleCount);

    // Panel signals
    void onMaterialSelected(int materialIndex);
    void onMaterialChanged(int index, const quantiloom::Material& material);
    void onLightingChanged(const quantiloom::LightingParams& params);
    void onSppChanged(uint32_t spp);
    void onSpectralModeChanged(quantiloom::SpectralMode mode);
    void onWavelengthChanged(float wavelength_nm);
    void onDebugModeChanged(quantiloom::DebugVisualizationMode mode);
    void onResetAccumulation();

    // Editing slots
    void onNodeTransformEdited(int nodeIndex, const glm::mat4& transform);
    void onMaterialWithCriChanged(int index, const quantiloom::Material& material,
                                  const quantiloom::ComplexRefractiveIndex& cri);
    void onCameraChanged();
    void onViewportClicked(const QPointF& screenPos);
    void onSelectionChanged(const QSet<int>& selectedNodes);
    void onGizmoTransformChanged(const glm::vec3& translation,
                                  const glm::quat& rotation,
                                  const glm::vec3& scale);
    void onGizmoTransformFinished();
    void onUndoRedoChanged();

    // Debug hover slot
    void onViewportHovered(int x, int y);

private:
    void setupUi();
    void setupMenus();
    void setupToolBar();
    void setupDockWidgets();
    void setupStatusBar();
    void setupConnections();
    void setupEditingSystem();
    void updatePanelsFromScene();
    void retranslateUi();

    // --- single application points --------------------------------------
    // Menu entry, toolbar control and panel widget all funnel through these,
    // so a mode can never be switched by two paths that do different things.
    void applyDebugMode(quantiloom::DebugVisualizationMode mode);
    void applySpectralMode(quantiloom::SpectralMode mode);
    void applyTargetSpp(uint32_t spp);
    void applyDisplayEnhancementEnabled(bool enabled);
    void applyTheme(const QString& themeId);

    void buildThemeMenu(QMenu* menu);

    /// Re-apply the shell's own theme-derived styling. The panels look after
    /// themselves through PanelBase; this is the window's share.
    void restyleUi();

    void buildDebugMenu(QMenu* menu, QComboBox* combo);
    void buildSpectralMenu(QMenu* menu, QComboBox* combo);
    void buildQualityMenu(QMenu* menu);
    void buildCameraMenu(QMenu* menu);
    void buildTransformMenu(QMenu* menu);

    // --- document state -------------------------------------------------
    /// Load a config or a model file, whichever the extension says it is.
    bool openPath(const QString& filePath);
    /// Write the current parameter set to @p filePath as TOML.
    bool writeConfig(const QString& filePath);
    void setCurrentDocument(const QString& filePath);
    void setSceneModified(bool modified);
    /// Ask about unsaved work; false means the caller should abort.
    bool confirmDiscardChanges();
    void updateWindowTitle();

    // --- recent files ----------------------------------------------------
    void rememberRecentFile(const QString& filePath);
    void rebuildRecentMenu();
    [[nodiscard]] QStringList recentFiles() const;

    // --- persistence -----------------------------------------------------
    void saveWindowState() const;
    void restoreWindowState();
    /// Initial size and minimum size, clamped to what the screen can show.
    void applyScreenAwareGeometry();

    // --- status bar ------------------------------------------------------
    /// Transient feedback: cleared automatically so a stale message cannot
    /// masquerade as the current state.
    void showStatusMessage(const QString& message, int timeoutMs = 6000);
    void updateRenderProgress();

    // Vulkan instance (owned by main())
    QVulkanInstance* m_vulkanInstance = nullptr;

    // Vulkan viewport, wrapped in the frame that carries the mode strip, the
    // empty state and the non-modal progress line.
    QuantiloomVulkanWindow* m_vulkanWindow = nullptr;
    QWidget* m_vulkanContainer = nullptr;
    ViewportFrame* m_viewportFrame = nullptr;

    // Workspaces: one preset dock arrangement per stage of the work.
    WorkspaceManager* m_workspaces = nullptr;
    QMenu* m_workspaceMenu = nullptr;
    QList<QAction*> m_workspaceActions;
    void setupWorkspaces();

    // One dock per panel, keyed by the panel's own id. The ten panels used to
    // be ten tabs of a single dock, so only one could ever be on screen and
    // "select a node, adjust its material, look at the result" meant cycling
    // through tabs.
    QMap<QString, QDockWidget*> m_docks;
    QDockWidget* createPanelDock(PanelBase* panel, Qt::DockWidgetArea area);

    // Parameter panels
    PropertiesPanel* m_propertiesPanel = nullptr;
    CameraPanel* m_cameraPanel = nullptr;
    SceneTreePanel* m_sceneTreePanel = nullptr;
    MaterialEditorPanel* m_materialEditorPanel = nullptr;
    LightingPanel* m_lightingPanel = nullptr;
    RenderSettingsPanel* m_renderSettingsPanel = nullptr;
    SpectralConfigPanel* m_spectralConfigPanel = nullptr;
    DebugVisualizationPanel* m_debugVisualizationPanel = nullptr;
    AtmosphericPanel* m_atmosphericPanel = nullptr;
    SensorPanel* m_sensorPanel = nullptr;
    DisplayEnhancementPanel* m_displayEnhancementPanel = nullptr;
    SpectralMaterialGenPanel* m_spectralMaterialGenPanel = nullptr;

    // Display enhancement (CLAHE) settings
    bool m_displayEnhancementEnabled = false;
    float m_claheClipLimit = 2.0f;
    int m_claheTileSize = 8;
    bool m_claheLuminanceOnly = true;

    // Status bar widgets
    QLabel* m_statusLabel = nullptr;
    QLabel* m_fpsLabel = nullptr;
    QLabel* m_sampleCountLabel = nullptr;
    QLabel* m_editModeLabel = nullptr;    // Shows current transform mode
    QLabel* m_debugValueLabel = nullptr;  // Shows debug value at mouse position
    QProgressBar* m_renderProgress = nullptr;
    class QTimer* m_statusTimer = nullptr;

    // Menus and actions kept as members: the Help ▸ Shortcuts page is built
    // from them, and retranslateUi() has to reach every one of them.
    QMenu* m_fileMenu = nullptr;
    QMenu* m_recentMenu = nullptr;
    QMenu* m_editMenu = nullptr;
    QMenu* m_viewMenu = nullptr;
    QMenu* m_panelsMenu = nullptr;
    QMenu* m_renderMenu = nullptr;
    QMenu* m_toolsMenu = nullptr;
    QMenu* m_helpMenu = nullptr;

    QAction* m_openAction = nullptr;
    QAction* m_saveAction = nullptr;
    QAction* m_saveAsAction = nullptr;
    QAction* m_exportImageAction = nullptr;
    QAction* m_screenshotAction = nullptr;
    QAction* m_exitAction = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_preferencesAction = nullptr;
    QAction* m_resetCameraAction = nullptr;
    QAction* m_resetLayoutAction = nullptr;
    QAction* m_startRenderAction = nullptr;
    QAction* m_stopRenderAction = nullptr;
    QAction* m_resetAccumulationAction = nullptr;
    QAction* m_spectralGenAction = nullptr;
    QAction* m_shortcutsAction = nullptr;
    QAction* m_debugReferenceAction = nullptr;
    QAction* m_aboutAction = nullptr;
    QAction* m_aboutQtAction = nullptr;
    QAction* m_displayEnhancementAction = nullptr;

    // Mode submenus. Each holds one exclusive action group, and each group is
    // mirrored by a toolbar combo box showing the same selection.
    QMenu* m_cameraMenu = nullptr;
    QMenu* m_transformMenu = nullptr;
    QMenu* m_debugMenu = nullptr;
    QMenu* m_spectralMenu = nullptr;
    QMenu* m_qualityMenu = nullptr;
    QMenu* m_themeMenu = nullptr;

    QActionGroup* m_debugGroup = nullptr;
    QActionGroup* m_spectralGroup = nullptr;
    QActionGroup* m_qualityGroup = nullptr;
    QActionGroup* m_transformModeGroup = nullptr;
    QActionGroup* m_themeGroup = nullptr;

    QHash<QString, QAction*> m_themeActions;  ///< keyed by theme id

    QHash<int, QAction*> m_debugActions;      ///< keyed by DebugVisualizationMode
    QHash<int, QAction*> m_spectralActions;   ///< keyed by SpectralMode
    QList<QMenu*> m_debugCategoryMenus;       ///< retranslated as a set

    QAction* m_translateAction = nullptr;
    QAction* m_rotateAction = nullptr;
    QAction* m_scaleAction = nullptr;
    QAction* m_axisXAction = nullptr;
    QAction* m_axisYAction = nullptr;
    QAction* m_axisZAction = nullptr;
    QAction* m_localSpaceAction = nullptr;

    QList<QAction*> m_viewPresetActions;      ///< front/back/left/right/top/bottom

    // Toolbar
    QToolBar* m_mainToolBar = nullptr;
    QComboBox* m_spectralCombo = nullptr;
    QComboBox* m_debugCombo = nullptr;
    QLabel* m_spectralComboLabel = nullptr;
    QLabel* m_debugComboLabel = nullptr;

    // Configuration manager
    ConfigManager* m_configManager = nullptr;

    /// The authoritative lighting parameters. The lighting panel owns the sun
    /// and sky halves and the atmosphere panel owns the two atmospheric terms;
    /// merging them here is what stops one panel's emit from resetting the
    /// other panel's values, and gives config export something to read back.
    std::unique_ptr<quantiloom::LightingParams> m_lightingParams;
    void pushLightingParams();

    // Current scene file
    QString m_currentSceneFile;   ///< model actually loaded into the renderer
    QString m_currentConfigFile;  ///< document being edited, written by Save
    bool m_sceneModified = false;

    // Helper methods
    void applyConfig(const SceneConfig& config);
    void collectCurrentConfig(SceneConfig& config);

    // Editing system
    SelectionManager* m_selectionManager = nullptr;
    TransformGizmo* m_transformGizmo = nullptr;
    UndoStack* m_undoStack = nullptr;

    // Transform state for undo
    struct TransformState {
        int nodeIndex;
        glm::mat4 originalTransform;
    };
    std::vector<TransformState> m_transformStartStates;

    // Pending material configs (applied after scene load)
    QVector<struct MaterialConfig> m_pendingMaterialConfigs;

    // Last configuration seen by applyConfig(), used as the base for
    // collectCurrentConfig(). Not every field has a panel behind it -- the
    // sampling seed, the hyperspectral range, the USD path, world scale -- and
    // rebuilding the struct from panels alone silently reset all of those to
    // their defaults on export. Held by pointer to keep SceneConfig, and with
    // it the SDK headers it pulls in, out of this header.
    std::unique_ptr<SceneConfig> m_lastConfig;
    void applyPendingMaterialConfigs();

    uistyle::StyleBindings m_styling;
};
