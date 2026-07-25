/**
 * @file MainWindow.hpp
 * @brief Main application window with Vulkan viewport and parameter panels
 *
 * @author wtflmao
 */

#pragma once

#include <QMainWindow>
#include <QVulkanInstance>
#include <QSet>
#include <memory>
#include <vector>

#include <core/Types.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

QT_BEGIN_NAMESPACE
class QDockWidget;
class QTabWidget;
class QStatusBar;
class QProgressBar;
class QLabel;
class QAction;
QT_END_NAMESPACE

namespace quantiloom {
class ExternalRenderContext;
struct LightingParams;
struct Material;
}

class QuantiloomVulkanWindow;
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
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QVulkanInstance* vulkanInstance, QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    // File menu actions
    void onNewScene();
    void onOpenScene();
    void onSaveScene();
    void onImportConfig();
    void onExportConfig();
    void onExportImage();

    // Render menu actions
    void onStartRender();
    void onStopRender();

    // View menu actions
    void onResetCamera();
    void onTakeScreenshot();

    // Help menu actions
    void onAbout();

    // Settings menu actions
    void onLanguageChanged(const QString& locale);
    void onSettings();

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
    void setupDockWidgets();
    void setupStatusBar();
    void setupConnections();
    void setupEditingSystem();
    void updatePanelsFromScene();
    void rememberConfigPath(const QString& filePath) const;

    // Vulkan instance (owned by main())
    QVulkanInstance* m_vulkanInstance = nullptr;

    // Vulkan viewport
    QuantiloomVulkanWindow* m_vulkanWindow = nullptr;
    QWidget* m_vulkanContainer = nullptr;

    // Parameter dock
    QDockWidget* m_parameterDock = nullptr;
    QTabWidget* m_parameterTabs = nullptr;

    // Parameter panels
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
    QLabel* m_editModeLabel = nullptr;  // Shows current transform mode
    QLabel* m_debugValueLabel = nullptr;  // Shows debug value at mouse position
    QProgressBar* m_renderProgress = nullptr;

    // Configuration manager
    ConfigManager* m_configManager = nullptr;

    // Current scene file
    QString m_currentSceneFile;
    QString m_currentConfigFile;
    bool m_sceneModified = false;

    // Helper methods
    void applyConfig(const SceneConfig& config);
    void collectCurrentConfig(SceneConfig& config);

    // Editing system
    SelectionManager* m_selectionManager = nullptr;
    TransformGizmo* m_transformGizmo = nullptr;
    UndoStack* m_undoStack = nullptr;

    // Menu actions for undo/redo
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;

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
};
