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
#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <core/SpectralData.hpp>
#include <core/Types.hpp>
#include <renderer/Pick.hpp>
#include <renderer/DisplayControl.hpp>
#include <renderer/ThermalControl.hpp>

#include "../editing/GizmoModel.hpp"

namespace quantiloom {
class Config;
class Scene;
struct Material;
struct LightingParams;
struct Image;
struct SensorParams;
struct ThermographyParams;
struct ComplexRefractiveIndex;
struct AtmosphereNNConfig;
struct SolarLutSpec;
}

class QTimer;

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
     * @brief Open a scene configuration, applied by the SDK
     *
     * Queued like any other setting when there is no render context yet.
     *
     * @param config  Parsed configuration
     * @param baseDir Directory its relative paths resolve against
     */
    void applyConfig(std::shared_ptr<const quantiloom::Config> config,
                     const QString& baseDir);

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
    /// Unmix a material's base colour into endmember weights and upload the
    /// result; see ExternalRenderContext::BuildEndmemberWeightTexture. Returns
    /// the texture index, or a message explaining what the material was
    /// missing -- most often base-colour pixels that were released on upload.
    [[nodiscard]] quantiloom::Result<int, std::string> buildEndmemberWeightTexture(
        uint32_t materialIndex, const std::vector<glm::vec3>& endmemberColors);
    /// Bind (or, with an empty source, clear) the spectrum a material emits;
    /// see ExternalRenderContext::SetMaterialEmissionSpectrum. Returns warnings
    /// worth showing -- most often that the lamp does not cover the band on
    /// screen and will therefore be dark -- or an error if nothing was bound.
    /// Write the thermal solve at the hour on screen, one row per element; see
    /// ExternalRenderContext::DumpThermalElements. An empty path takes the one
    /// the document's `[thermal] dump_elements` set. Returns the file written.
    [[nodiscard]] quantiloom::Result<std::string, std::string> dumpThermalElements(
        const QString& pathOrEmpty);
    [[nodiscard]] quantiloom::Result<std::vector<std::string>, std::string>
    setMaterialEmissionSpectrum(uint32_t materialIndex, const std::string& sourceOrEmpty,
                                const QString& baseDir);
    /// See QuantiloomVulkanRenderer::setSolarLutFromSpec.
    [[nodiscard]] std::optional<QString> setSolarLutFromSpec(
        const quantiloom::SolarLutSpec& spec, const QString& baseDir);

    void setSolarSpectralLUT(const quantiloom::SpectralCurve& sun,
                             const quantiloom::SpectralCurve& sky);

    /**
     * @brief Reset render accumulation
     */
    void resetAccumulation();

    /**
     * @brief Get current sample count
     */
    /// Prefer a discrete GPU when the machine has more than one device. Call
    /// after setVulkanInstance() and before the window is shown; Qt otherwise
    /// takes device 0, which on a hybrid laptop is the integrated GPU and has
    /// no ray tracing.
    void selectRayTracingDevice();

    /// See QuantiloomVulkanRenderer::frameBounds.
    void frameBounds(const glm::vec3& min, const glm::vec3& max);

    /// See QuantiloomVulkanRenderer::setSceneScale.
    void setSceneScale(float radius);

    /// See QuantiloomVulkanRenderer::setCameraProjection.
    void setCameraProjection(bool orthographic, float orthoHeight);
    [[nodiscard]] bool cameraIsOrthographic() const;
    [[nodiscard]] float cameraOrthoHeight() const;

    /**
     * @brief Draw one more frame because the overlay changed
     *
     * The grid, the gizmo and the selection box are rebuilt inside
     * startNextFrame(), so they only appear when a frame is drawn. Everything
     * that changes the *scene* resets accumulation, which asks for one; the
     * things that change only what is composited on top -- selecting an
     * object, hovering a handle, switching gizmo mode, toggling the grid --
     * do not, and used to rely on the render loop running continuously.
     *
     * It stopped running continuously when auto-stop at the target sample
     * count arrived: past the target the loop idles, so a selection made after
     * a render finished stayed invisible until the camera moved. Anything that
     * mutates overlay-only state calls this.
     */
    void requestOverlayRedraw();

    /// Colour of the wireframe box drawn around the selection. Linear RGBA --
    /// the overlay writes into an sRGB target that encodes on write. Driven
    /// from the theme's accent, so it changes with the theme like everything
    /// else the shell draws.
    void setSelectionBoxColor(const glm::vec4& color);

    uint32_t currentSampleCount() const;

    /// The SDK's own frame timing, which measures the trace rather than this
    /// thread's command recording.
    [[nodiscard]] float lastGpuFrameTimeMs() const;

    /**
     * @brief Target sample count the accumulation is working towards
     */
    uint32_t targetSPP() const;

    /**
     * @brief The sampling seed the renderer is running on
     */
    uint32_t samplingSeed() const;

    /**
     * @brief What the renderer is running on, for populating the panels
     *
     * After a config is opened these are what the SDK resolved from the file,
     * not what this repo read from it. Default-constructed values when there is
     * no renderer yet.
     */
    quantiloom::LightingParams lightingParams() const;
    quantiloom::SpectralMode spectralMode() const;
    float wavelength() const;
    quantiloom::AtmosphereNNConfig atmosphericConfig() const;
    /// The sensor state currently in effect, for callers that need the value
    /// a change is replacing (the undo history does).
    [[nodiscard]] bool sensorEnabled() const;
    [[nodiscard]] quantiloom::SensorParams sensorParams() const;
    quantiloom::DebugVisualizationMode debugMode() const;

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

    /// The temperature a thermal camera would report for this pixel, from the
    /// accumulated radiance. False in any mode with no band to invert.
    bool readApparentTemperature(int x, int y, double& outKelvin);

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
    /// What the camera is told about the surface, for the readout above.
    /// Display-side: no accumulation reset.
    void setThermographyParams(const quantiloom::ThermographyParams& params);

    // ========================================================================
    // Thermal Solve
    // ========================================================================

    void setThermalSolveParams(const quantiloom::ThermalSolveParams& params);
    void setThermalMaterial(const QString& name, const quantiloom::ThermalMaterialParams& params);
    void clearThermalMaterials();
    void setThermalSolveEnabled(bool enabled);
    void setThermalTime(double time_h);
    [[nodiscard]] quantiloom::ThermalSolveStatus thermalSolveStatus() const;
    [[nodiscard]] quantiloom::Result<quantiloom::u32, quantiloom::String> thermalElementAt(
        const quantiloom::PickResult& pick) const;
    [[nodiscard]] quantiloom::Result<quantiloom::ThermalElementTrajectory, quantiloom::String>
    elementTrajectory(quantiloom::u32 element, double fromHour, double toHour,
                      quantiloom::u32 samples);

    // ========================================================================
    // Display Enhancement
    // ========================================================================

    /**
     * @brief Set the tone operator and palette the viewport is displayed with
     * @param params see DisplayControl.hpp -- a contrast stage and a colour
     *               stage, which compose independently
     */
    void setDisplayEnhancement(const quantiloom::DisplayEnhancementParams& params);

    // ========================================================================
    // Viewport Overlay
    // ========================================================================

    /**
     * @brief Show or hide the ground grid overlay
     *
     * Display-only: does not touch accumulation (see the renderer's setter).
     * Queued through withRenderer() like every setter, so a toggle restored
     * from settings before the renderer exists is not lost.
     */
    void setGridVisible(bool visible);

    [[nodiscard]] bool isGridVisible() const;

    /**
     * @brief Trace at a reduced extent while the user is moving the view
     *
     * Queued through withRenderer() like every setter here, so a value
     * restored from settings before the renderer exists is not lost.
     */
    void setMotionAdaptiveResolution(bool enabled);
    [[nodiscard]] bool motionAdaptiveResolution() const;

    /// What the SDK is tracing at right now, and at what extent. 1.0 and the
    /// swapchain extent unless a gesture is in progress.
    [[nodiscard]] float currentRenderScale() const;
    [[nodiscard]] QSize currentRenderSize() const;

    /// Force a render scale, ignoring the motion state. The MCP test hook --
    /// no tool call can hold a drag open. The next gesture overwrites it.
    void overrideRenderScale(float scale);

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
     * @brief Set node transform (full-quality: rebuild + reset accumulation)
     */
    void setNodeTransform(int nodeIndex, const glm::mat4& transform);

    /**
     * @brief Interactive-drag variants of setNodeTransform
     *
     * During a drag: setNodeTransformInteractive() per moved node (transform
     * only), then one refitAfterInteractiveEdit() per mouse move (in-place
     * TLAS refit + accumulation reset). On release or cancel:
     * finalizeInteractiveEdit() for one full-quality rebuild.
     */
    void setNodeTransformInteractive(int nodeIndex, const glm::mat4& transform);
    void refitAfterInteractiveEdit();
    void finalizeInteractiveEdit();

    /**
     * @brief Topology edits: paste, delete, and their undo
     *
     * duplicateNode() is the shallow copy behind copy-paste (shared mesh and
     * materials, own transform); removeNode() tombstones so indices held by
     * the selection and the undo stack never shift; restoreNode() is remove's
     * exact undo. None of the three rebuilds by itself -- batch the edits,
     * then call rebuildSceneTopology() once (full rebuild + accumulation
     * reset; a refit cannot represent an instance-count change).
     *
     * @return duplicateNode: the new node's index, or -1 on failure
     */
    int duplicateNode(int sourceIndex, const QString& newName);
    bool removeNode(int nodeIndex);
    bool restoreNode(int nodeIndex);
    void rebuildSceneTopology();

    /**
     * @brief What is under this device-pixel position, via the SDK's ray query
     *
     * The pick ray is the raygen shader's own primary ray for the pixel, so
     * the answer agrees with the frame on screen. Returns std::nullopt when
     * no scene is loaded or the pick could not run (the error is logged).
     */
    [[nodiscard]] std::optional<quantiloom::PickResult> pickScene(const QPointF& devicePos);

    /**
     * @brief This frame's gizmo triangles, or false when no gizmo is shown
     *
     * Called by the renderer every frame. The frame (origin, axes, screen-
     * constant scale) is the same one the hover and grab hit-tests use, so
     * what is drawn is exactly what is grabbable.
     */
    bool buildGizmoDrawList(const vkview::CameraMatrices& camera,
                            std::vector<editing::GizmoVertex>& out);

    /**
     * @brief One-shot what-you-see PNG of the composited viewport
     *
     * The swapchain image with grid and gizmo overlays -- the SDK-side
     * captures cannot show those. Saved asynchronously a few frames later.
     */
    void requestCompositedCapture(const QString& path);

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

    /**
     * @brief Enable or disable the scene-editing tools (gizmo, transforms)
     *
     * Off outside the Layout workspace: the gizmo neither draws nor grabs,
     * hover does nothing, and a drag in flight is aborted like Escape.
     * Viewport click-to-select stays active -- picking an object to inspect
     * it is not a scene edit.
     */
    void setEditingToolsEnabled(bool enabled);
    [[nodiscard]] bool editingToolsEnabled() const { return m_editingToolsEnabled; }

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

    /// The render context could not be created, so nothing will ever draw.
    /// Distinct from sceneLoaded(false, ...): that one means this file did not
    /// open, this one means the renderer never started. Without it the failure
    /// was a qCritical line and a black viewport.
    void renderContextFailed(const QString& message);

    /**
     * @brief Emitted when user clicks in viewport (for selection picking)
     * @param screenPos Screen position of click
     */
    void viewportClicked(const QPointF& screenPos, Qt::KeyboardModifiers modifiers);

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
     * @brief Try to grab a gizmo handle at a device-pixel position
     *
     * Returns true and sets up the drag when a handle is under the cursor;
     * false lets the click fall through to selection. The handle set arrives
     * with the drawn gizmo -- until then this is always false.
     */
    bool beginGizmoDragAt(const QPointF& devicePos);

    /// Whether a gizmo is currently on screen (edit mode, selection, scene)
    [[nodiscard]] bool gizmoOnScreen() const;

    /// The gizmo's placement for the current camera: selection center,
    /// world/local axes, screen-constant scale
    [[nodiscard]] editing::GizmoFrame currentGizmoFrame(
        const vkview::CameraMatrices& camera) const;

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

    /**
     * @brief Report that the user is moving the view or an object right now
     *
     * Called on every event that starts, continues or ends a camera or gizmo
     * gesture. It puts the renderer into its motion state -- where it traces
     * at a reduced extent -- and restarts a 250 ms decay timer.
     *
     * The decay is not a nicety. The wheel has no release event, so a zoom is
     * a burst of presses with no end, and the only way to know a burst is over
     * is that no further one arrived. Every other gesture goes through the
     * timer too, so that a click-drag-click sequence does not flap the render
     * extent -- and each flap costs a device wait and the accumulation.
     *
     * Deliberately not driven from startNextFrame(): the render loop stops
     * when the accumulation reaches its target, which is often the moment the
     * user stopped moving, so a check there would never run.
     */
    void noteViewportMotion();

    /// Whether a button or a fly key is still down, in which case the gesture
    /// is not over however quiet it has been.
    [[nodiscard]] bool viewportMotionHeld() const;

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

    /// Single-shot, restarted by every motion event; see noteViewportMotion().
    /// Created lazily because this window outlives no event loop of its own.
    QTimer* m_motionDecayTimer = nullptr;
    bool m_motionActive = false;

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
    bool m_editingToolsEnabled = true;  // false outside the Layout workspace
    bool m_transformDragging = false;
    QPointF m_transformDragStart;

    // Gizmo interaction state, fed to the overlay for highlight colors
    editing::GizmoHandle m_hoveredHandle = editing::GizmoHandle::None;
    /// Linear RGBA; the shell overwrites it from the theme accent.
    glm::vec4 m_selectionBoxColor{0.29f, 0.56f, 0.85f, 0.85f};
    editing::GizmoHandle m_activeHandle = editing::GizmoHandle::None;
};
