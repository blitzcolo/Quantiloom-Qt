/**
 * @file QuantiloomVulkanWindow.hpp
 * @brief QVulkanWindow subclass for Quantiloom rendering
 *
 * @author blitzcolo
 */

#pragma once

#include <QVulkanWindow>
#include <QString>
#include <functional>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <core/SpectralData.hpp>
#include <core/Types.hpp>

namespace quantiloom {
class Scene;
struct Material;
struct LightingParams;
struct Image;
struct SensorParams;
struct ComplexRefractiveIndex;
struct AtmosphereNNConfig;
}

class QuantiloomVulkanRenderer;
class SelectionManager;
class TransformGizmo;
class UndoStack;

/**
 * @class QuantiloomVulkanWindow
 * @brief Custom Vulkan window that hosts Quantiloom rendering
 *
 * This class manages the Vulkan surface and coordinates with
 * QuantiloomVulkanRenderer for actual rendering operations.
 */
class QuantiloomVulkanWindow : public QVulkanWindow {
    Q_OBJECT

public:
    explicit QuantiloomVulkanWindow(QWindow* parent = nullptr);
    ~QuantiloomVulkanWindow() override;

    /**
     * @brief Create the Vulkan renderer
     * @return New QVulkanWindowRenderer instance
     */
    QVulkanWindowRenderer* createRenderer() override;

    /**
     * @brief Load a scene from file
     * @param filePath Path to glTF or TOML scene file
     */
    void loadScene(const QString& filePath);

    /**
     * @brief Replay the settings queued before the render context existed
     *
     * Called by the renderer once its ExternalRenderContext is up. It cannot
     * be done when the renderer object is constructed: QVulkanWindow calls
     * createRenderer() before initResources()/initSwapChainResources(), so at
     * that point the renderer has no context and anything needing one -- the
     * environment map above all -- would fail all over again.
     */
    void applyDeferredSettings();

    /**
     * @brief Whether the renderer exists yet
     *
     * QVulkanWindow builds it on first exposure, so there is a window in which
     * this class exists and the renderer does not. Settings applied during it
     * are queued, not lost -- see withRenderer().
     */
    [[nodiscard]] bool hasRenderer() const { return m_renderer != nullptr; }

    /**
     * @brief Reset camera to default position
     */
    void resetCamera();

    /**
     * @brief Set camera from config parameters
     */
    void setCamera(const glm::vec3& position, const glm::vec3& lookAt,
                   const glm::vec3& up, float fovY);

    /**
     * @brief Read the current camera pose
     */
    void getCameraState(glm::vec3& position, glm::vec3& target,
                        glm::vec3& up, float& fovY) const;

    /**
     * @brief Set the vertical field of view in degrees
     */
    void setCameraFovY(float fovY);

    /**
     * @brief Look at the current target from a given direction (view presets)
     */
    void setViewDirection(const glm::vec3& direction);

    /**
     * @brief Set render samples per pixel
     */
    void setSPP(uint32_t spp);

    /**
     * @brief Set the path tracer sampling seed (0 = nondeterministic)
     */
    void setSamplingSeed(uint32_t seed);

    /**
     * @brief Set spectral wavelength for mono-band mode
     */
    void setWavelength(float wavelength_nm);

    /**
     * @brief Set spectral rendering mode
     */
    void setSpectralMode(quantiloom::SpectralMode mode);

    /**
     * @brief Set debug visualization mode
     */
    void setDebugMode(quantiloom::DebugVisualizationMode mode);

    /**
     * @brief Update lighting parameters
     */
    void setLightingParams(const quantiloom::LightingParams& params);

    /**
     * @brief Update material at specified index
     */
    void updateMaterial(int index, const quantiloom::Material& material);

    /**
     * @brief Add complex refractive index data for physical Fresnel
     * @param cri CPU-side complex refractive index (n,k curves)
     * @return Index into CRI buffer, or -1 on failure
     */
    int addComplexRefractiveIndex(const quantiloom::ComplexRefractiveIndex& cri);
    int addSpectralCurve(const quantiloom::SpectralCurve& curve);
    void setSolarSpectralLUT(const quantiloom::SpectralCurve& sun,
                             const quantiloom::SpectralCurve& sky);

    /**
     * @brief Reset render accumulation
     */
    void resetAccumulation();

    /**
     * @brief Get current sample count
     */
    uint32_t currentSampleCount() const;

    /**
     * @brief Target sample count the accumulation is working towards
     */
    uint32_t targetSPP() const;

    /**
     * @brief Suspend or resume progressive accumulation (Render ▸ Stop/Start)
     */
    void setRenderPaused(bool paused);
    [[nodiscard]] bool isRenderPaused() const;

    /**
     * @brief Get current scene (may be null)
     */
    const quantiloom::Scene* getScene() const;

    /**
     * @brief Read debug pixel value at position
     * @param x X coordinate
     * @param y Y coordinate
     * @param outValue Output pixel value
     * @return true if read succeeded
     */
    bool readDebugPixel(int x, int y, glm::vec4& outValue);

    /**
     * @brief Format debug value based on current debug mode
     * @param pixel Raw pixel value
     * @return Formatted string
     */
    QString formatDebugValue(const glm::vec4& pixel) const;

    /**
     * @brief Get current debug visualization mode
     */
    quantiloom::DebugVisualizationMode getDebugMode() const;

    /**
     * @brief Capture current frame as Image
     */
    std::unique_ptr<quantiloom::Image> captureScreenshot();

    /**
     * @brief Capture display image (with CLAHE applied if enabled)
     *
     * Returns the image as shown on screen. If CLAHE is enabled,
     * the returned image has CLAHE processing applied.
     */
    std::unique_ptr<quantiloom::Image> captureDisplayImage();

    // ========================================================================
    // Atmospheric Configuration
    // ========================================================================

    /**
     * @brief Set atmosphere configuration by preset name
     * @param preset NN preset name: "clear", "turbulent_clear", "urban_haze",
     *               "fog", "light_rain", "heavy_rain", "snow", "haze",
     *               "disabled" (legacy analytic names are mapped)
     */

    /**
     * @brief Set full NN atmosphere configuration
     * @param config Weather / geometry configuration
     */
    void setAtmosphericConfig(const quantiloom::AtmosphereNNConfig& config);

    // ========================================================================
    // Environment Map (IBL)
    // ========================================================================

    /**
     * @brief Load HDR environment map for IBL
     * @param hdrPath Path to equirectangular HDR image (.exr, .hdr)
     * @return true if loading succeeded
     */
    bool loadEnvironmentMap(const QString& hdrPath);

    // ========================================================================
    // Sensor Simulation
    // ========================================================================

    /**
     * @brief Enable or disable sensor simulation
     * @param enabled true to enable sensor post-processing
     */
    void setSensorEnabled(bool enabled);

    /**
     * @brief Set sensor parameters
     * @param params Sensor parameters (optics, detector, noise, etc.)
     */
    void setSensorParams(const quantiloom::SensorParams& params);

    // ========================================================================
    // Display Enhancement (CLAHE)
    // ========================================================================

    /**
     * @brief Enable or disable display enhancement (CLAHE)
     * @param enabled true to enable CLAHE post-processing on display
     * @param clipLimit Contrast limit (1.0 = no limit, typical 2.0-4.0)
     * @param tileSize Tile grid size (4, 8, 16, or 32)
     * @param luminanceOnly Apply only to luminance channel (preserve color)
     */
    void setDisplayEnhancement(bool enabled, float clipLimit,
                               int tileSize, bool luminanceOnly);

    // ========================================================================
    // Scene Editing
    // ========================================================================

    /**
     * @brief Set editing components (owned by MainWindow)
     */
    void setEditingComponents(SelectionManager* selection,
                               TransformGizmo* gizmo,
                               UndoStack* undoStack);

    /**
     * @brief Set node transform
     */
    void setNodeTransform(int nodeIndex, const glm::mat4& transform);

    /**
     * @brief Get camera info for gizmo
     */
    void getCameraInfo(glm::vec3& position, glm::vec3& forward,
                       glm::vec3& right, glm::vec3& up) const;

    /**
     * @brief Check if in edit mode (vs camera mode)
     */
    [[nodiscard]] bool isEditMode() const { return m_editMode; }

    /**
     * @brief Set edit mode
     */
    void setEditMode(bool edit);

signals:
    /**
     * @brief Emitted after each frame is rendered
     * @param frameTimeMs Frame render time in milliseconds
     * @param sampleCount Current accumulated sample count
     */
    void frameRendered(float frameTimeMs, uint32_t sampleCount);

    /**
     * @brief Emitted when scene loading completes or fails
     * @param success True if loading succeeded
     * @param message Status message or error description
     */
    void sceneLoaded(bool success, const QString& message);

    /**
     * @brief Emitted when user clicks in viewport (for selection picking)
     * @param screenPos Screen position of click
     */
    void viewportClicked(const QPointF& screenPos);

    /**
     * @brief Emitted when edit mode changes
     */
    void editModeChanged(bool editMode);

    /**
     * @brief Emitted whenever the camera pose changes, from any source
     */
    void cameraChanged();

    /**
     * @brief A blocking operation is under way (first-run shader compilation)
     *
     * Reported rather than handled here: the renderer has no business owning
     * an application-modal dialog, which is what it used to raise before the
     * main window had appeared.
     */
    void longOperationStarted(const QString& description);
    void longOperationFinished();

    /**
     * @brief Emitted when the mouse moves over the viewport
     * @param x X coordinate in **device** pixels
     * @param y Y coordinate in **device** pixels
     *
     * Device, not logical, pixels: the framebuffer these index into is
     * allocated at the swapchain's physical size. The two agree only at 100%
     * scaling, and the reading used to drift further off the cursor the
     * further right and down it went on any scaled display.
     */
    void mouseHovered(int x, int y);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool event(QEvent* event) override;  // For hover events

private:
    friend class QuantiloomVulkanRenderer;

    /**
     * @brief Convert a Qt pointer position to device pixels
     *
     * The single crossing point between interface coordinates and renderer
     * coordinates. Everything that hands a pointer position to the render
     * side -- debug pixel readout, gizmo dragging, camera drag deltas -- goes
     * through here, so a fractional scale factor cannot desynchronise one of
     * them while the others stay correct.
     */
    [[nodiscard]] QPointF toDevicePixels(const QPointF& logical) const;

    /**
     * @brief Apply a setting now, or record it until the renderer exists
     *
     * QVulkanWindow creates the renderer on first exposure. Every setter here
     * used to be `if (m_renderer) ...`, which meant that a configuration
     * applied before that moment was silently discarded -- the first scene
     * opened after launch rendered with default lighting, no sensor model and
     * no environment map, and only the second one looked right. The same hole
     * swallowed settings across a minimize, which the pending scene path was
     * already working around on its own.
     *
     * Recorded calls are replayed in order in createRenderer(). Every setter
     * is idempotent, so replaying a value that was later overwritten is
     * harmless.
     */
    void withRenderer(std::function<void(QuantiloomVulkanRenderer&)> call);

    QuantiloomVulkanRenderer* m_renderer = nullptr;
    std::vector<std::function<void(QuantiloomVulkanRenderer&)>> m_deferredCalls;
    QString m_pendingScenePath;

    // Camera control state
    bool m_mousePressed = false;

    /**
     * @brief Previous pointer position of a camera drag, in *logical* pixels
     *
     * Deliberately not `toDevicePixels()`d, unlike every other position that
     * crosses into the render side. Those are positions -- a debug readout or a
     * gizmo pick has to name one framebuffer pixel, so it must carry the scale
     * factor. A camera drag consumes only the *difference* between two
     * positions, which names no pixel; scaling it just makes the same hand
     * movement orbit further on a higher-DPI display. At the 175% this was
     * found on, orbit ran at 0.5 deg per pixel of real mouse travel.
     */
    QPointF m_lastMousePos;
    bool m_keyW = false;
    bool m_keyA = false;
    bool m_keyS = false;
    bool m_keyD = false;
    bool m_keyQ = false;
    bool m_keyE = false;
    bool m_shiftHeld = false;

    // Vulkan feature structures (must persist during device creation)
    VkPhysicalDeviceBufferDeviceAddressFeatures m_bufferDeviceAddressFeatures{};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR m_rayTracingPipelineFeatures{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR m_accelerationStructureFeatures{};
    VkPhysicalDeviceDynamicRenderingFeatures m_dynamicRenderingFeatures{};
    VkPhysicalDeviceDescriptorIndexingFeatures m_descriptorIndexingFeatures{};
    VkPhysicalDeviceScalarBlockLayoutFeatures m_scalarBlockLayoutFeatures{};
    VkPhysicalDeviceRayQueryFeaturesKHR m_rayQueryFeatures{};
    VkPhysicalDeviceSynchronization2Features m_synchronization2Features{};

    // Editing components (owned by MainWindow)
    SelectionManager* m_selection = nullptr;
    TransformGizmo* m_gizmo = nullptr;
    UndoStack* m_undoStack = nullptr;

    // Edit mode state
    bool m_editMode = true;  // Default to edit mode
    bool m_transformDragging = false;
    QPointF m_transformDragStart;
};
