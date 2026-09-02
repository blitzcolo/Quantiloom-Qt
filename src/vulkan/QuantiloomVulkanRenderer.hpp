/**
 * @file QuantiloomVulkanRenderer.hpp
 * @brief QVulkanWindowRenderer adapter for libQuantiloom integration
 *
 * @author blitzcolo
 */

#pragma once

#include <QVulkanWindowRenderer>
#include <QCoreApplication>
#include <QString>
#include <QSize>
#include <QFuture>
#include <memory>
#include <chrono>
#include <optional>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <core/SpectralData.hpp>
#include <core/Types.hpp>
#include <renderer/LightingParams.hpp>
#include <atmos/AtmosphereNNConfig.hpp>
#include <postprocess/SensorModel.hpp>
#include <postprocess/Thermography.hpp>
#include <renderer/ThermalControl.hpp>
// For SolarLutSpec, held by value in an optional below (optional forbids an
// incomplete type, unlike the unique_ptr<ExternalRenderContext> beside it).
#include <renderer/ExternalRenderContext.hpp>

#include "OverlayRenderer.hpp"

namespace quantiloom {
struct ComplexRefractiveIndex;
}

class QuantiloomVulkanWindow;

namespace quantiloom {
class Config;
class ExternalRenderContext;
class Scene;
struct Material;
struct Image;
struct ConfigApplyReport;
}

/**
 * @class QuantiloomVulkanRenderer
 * @brief Adapter layer connecting Qt's Vulkan infrastructure with libQuantiloom
 *
 * This class implements QVulkanWindowRenderer and uses libQuantiloom's
 * ExternalRenderContext to perform actual ray tracing rendering.
 * It bridges Qt-managed Vulkan handles with the library's rendering pipeline.
 */
class QuantiloomVulkanRenderer : public QVulkanWindowRenderer {
    // Not a QObject, but it still produces a few user-visible strings; this
    // gives them a translation context lupdate can see.
    Q_DECLARE_TR_FUNCTIONS(QuantiloomVulkanRenderer)

public:
    explicit QuantiloomVulkanRenderer(QuantiloomVulkanWindow* window);
    ~QuantiloomVulkanRenderer() override;

    // QVulkanWindowRenderer interface
    void initResources() override;
    void initSwapChainResources() override;
    void releaseSwapChainResources() override;
    void releaseResources() override;
    void startNextFrame() override;

    // Scene management
    /// @param adoptSceneCamera Take the camera the scene file carries. True for
    ///        a user-initiated open; false when replaying the path to rebuild a
    ///        destroyed context, where the camera the user has since flown to is
    ///        the one that should survive.
    void loadScene(const QString& filePath, bool adoptSceneCamera = true);

    /// Open a scene configuration: the SDK reads every key, the same reading the
    /// core CLI does. This repo interpreted them itself until the SDK exported
    /// ApplyConfig, and the two readings had drifted -- see
    /// render_path_divergence.md.
    ///
    /// The config is kept so a destroyed context can be rebuilt from it, which
    /// covers state no member of this class ever held: the solar LUT, the
    /// spectral curves, the refractive indices, the IR temperature backfill.
    ///
    /// @param config  Parsed config, shared because the replay outlives the call.
    /// @param baseDir Directory the config's relative paths resolve against.
    void applyConfig(std::shared_ptr<const quantiloom::Config> config,
                     const QString& baseDir);

    /// True while a scene configuration is the open document, as opposed to a
    /// bare model or nothing at all.
    [[nodiscard]] bool hasConfig() const { return m_currentConfig != nullptr; }

    void resetCamera();

    // Render settings
    void setSPP(uint32_t spp);
    /// Path tracer sampling seed, same convention as the CLI's renderer.seed:
    /// nonzero reproduces a render, 0 draws a nondeterministic seed.
    void setSamplingSeed(uint32_t seed);
    void setWavelength(float wavelength_nm);
    void setSpectralMode(quantiloom::SpectralMode mode);
    void setDebugMode(quantiloom::DebugVisualizationMode mode);
    void setLightingParams(const quantiloom::LightingParams& params);
    void updateMaterial(int index, const quantiloom::Material& material);
    int addComplexRefractiveIndex(const quantiloom::ComplexRefractiveIndex& cri);

    // Quantitative spectral data. Without these the context binds zeroed buffers and
    // every material renders through the RGB-upsampled fallback.
    int addSpectralCurve(const quantiloom::SpectralCurve& curve);
    /// Unmix a material's base colour into endmember weights, upload it, and
    /// return the texture index; see
    /// ExternalRenderContext::BuildEndmemberWeightTexture.
    [[nodiscard]] quantiloom::Result<int, std::string> buildEndmemberWeightTexture(
        uint32_t materialIndex, const std::vector<glm::vec3>& endmemberColors);
    /// Set the illuminant the way a scene file would, through the core's own
    /// reading of solar_lut. Returns the core's error when it could not load.
    /// Kept for replay after a context rebuild, like the curves above.
    [[nodiscard]] std::optional<QString> setSolarLutFromSpec(
        const quantiloom::SolarLutSpec& spec, const QString& baseDir);

    void setSolarSpectralLUT(const quantiloom::SpectralCurve& sun,
                             const quantiloom::SpectralCurve& sky);
    void resetAccumulation();
    uint32_t currentSampleCount() const { return m_sampleCount; }

    /// Orbit around a world-space box and pull back far enough to see all of
    /// it. Keeps the current viewing direction: framing changes what the
    /// camera looks at, not where it looks from.
    void frameBounds(const glm::vec3& min, const glm::vec3& max);

    /// The scene's bounding-sphere radius, from which the fly speed and the
    /// zoom clamps are derived. Zero restores the fixed fallbacks.
    void setSceneScale(float radius);

    /// Perspective or orthographic ray generation. Kept so it survives a
    /// context rebuild, like every other setter here.
    void setCameraProjection(bool orthographic, float orthoHeight);
    [[nodiscard]] bool cameraIsOrthographic() const { return m_orthographic; }
    [[nodiscard]] float cameraOrthoHeight() const { return m_orthoHeight; }

    /// The SDK's trace time for the last frame, as distinct from the wall
    /// clock the frameRendered signal carries.
    [[nodiscard]] float lastGpuFrameTimeMs() const { return m_lastGpuFrameTimeMs; }

    /// @name Motion-adaptive resolution
    /// A heavy scene traces at the target extent whatever the camera is doing,
    /// which is what makes a glass close-up drop below 20 samples/s -- and,
    /// since one trace is one present here, drags the grid and the gizmo down
    /// with it. While the user is actually moving something, the SDK traces at
    /// a fraction of the extent and the presenting blit magnifies: a quarter
    /// of the pixels is roughly four times the rate, and the softness lasts
    /// only as long as the gesture. Nothing about the estimator changes, and
    /// no exported image ever comes from a reduced-scale frame -- exports read
    /// the accumulation, which is full-scale again the moment the gesture ends
    /// and restarts from zero.
    /// @{

    /// The feature toggle (View menu, remembered in QSettings). Turning it off
    /// mid-gesture restores full scale immediately.
    void setMotionAdaptiveResolution(bool enabled);
    [[nodiscard]] bool motionAdaptiveResolution() const {
        return m_motionAdaptiveResolution;
    }

    /// Called by the window when a camera or gizmo gesture starts and again
    /// when it has been quiet for long enough to count as over. The scale is
    /// chosen once, on the rising edge, and held for the whole gesture:
    /// changing it costs a device wait and an accumulation reset.
    void setViewportMotionActive(bool active);
    [[nodiscard]] bool viewportMotionActive() const { return m_motionActive; }

    /// What the SDK is tracing at right now: 1.0 unless a gesture is in
    /// progress. For the status bar and the MCP status tool.
    [[nodiscard]] float currentRenderScale() const;
    /// The extent the SDK is tracing at, which is the target extent times the
    /// scale. Zero-sized before the first frame.
    [[nodiscard]] QSize currentRenderSize() const;

    /// Force a scale, ignoring the motion state. The MCP test hook: no gesture
    /// can be held open from a tool call, so this is how the decoupling is
    /// exercised. The next gesture overwrites it.
    void overrideRenderScale(float scale);
    /// @}
    uint32_t targetSPP() const { return m_targetSPP; }
    uint32_t samplingSeed() const { return m_samplingSeed; }

    /// @name What the renderer is actually running on
    /// These read the members this class keeps in step with the context, which
    /// after applyConfig() are what the SDK resolved from the file. The shell
    /// populates its panels from here rather than from its own reading of the
    /// config -- a widget disagreeing with the renderer is the bug class the
    /// shared config reading exists to remove.
    /// @{
    [[nodiscard]] const quantiloom::LightingParams& lightingParams() const {
        return m_lightingParams;
    }
    [[nodiscard]] quantiloom::SpectralMode spectralMode() const { return m_spectralMode; }
    [[nodiscard]] float wavelength() const { return m_wavelength; }
    [[nodiscard]] const quantiloom::AtmosphereNNConfig& atmosphericConfig() const {
        return m_atmosphericConfig;
    }
    /// @}

    /**
     * @brief Suspend or resume progressive accumulation
     *
     * Purely a GUI-side throttle: while paused the renderer finishes the
     * frame in flight and then stops asking for another, so the last image
     * stays on screen and the sample count stops climbing. Nothing about how
     * a frame is rendered changes -- that all still belongs to the SDK.
     */
    void setPaused(bool paused);
    bool isPaused() const { return m_paused; }

    // Camera setup from config
    void setCamera(const glm::vec3& position, const glm::vec3& lookAt,
                   const glm::vec3& up, float fovY);

    /// Current pose, for the camera panel and for config export.
    void getCameraState(glm::vec3& position, glm::vec3& target,
                        glm::vec3& up, float& fovY) const;

    void setCameraFovY(float fovY);

    /// Look at the current target from @p direction, keeping the orbit
    /// distance. Used by the standard view presets (front, top, ...).
    void setViewDirection(const glm::vec3& direction);

    // Scene access
    const quantiloom::Scene* getScene() const;

    // Camera info for gizmo
    void getCameraInfo(glm::vec3& position, glm::vec3& forward,
                       glm::vec3& right, glm::vec3& up) const;

    // Render context access (for transform operations)
    quantiloom::ExternalRenderContext* getRenderContext() { return m_renderContext.get(); }

    // ========================================================================
    // Viewport overlay (grid, gizmo)
    // ========================================================================

    /// Show or hide the ground grid. Display-only: deliberately does NOT
    /// resetAccumulation(), the exception to this file's every-setter rule --
    /// the scene is unchanged, only what is composited over the frame is.
    /// It still requests a frame, or a toggle after the render loop stops at
    /// its target would not show until the camera moved.
    void setGridVisible(bool visible);
    [[nodiscard]] bool isGridVisible() const { return m_overlay.gridVisible(); }

    /// The camera the frame on screen was rendered with, as matrices and
    /// pixel rays. Only valid while a scene is loaded.
    [[nodiscard]] vkview::CameraMatrices overlayCamera() const;

    /// One-shot what-you-see PNG: the swapchain image with grid and gizmo
    /// composited, saved a few frames from now. Poll the file.
    void requestCompositedCapture(const QString& path) {
        m_overlay.requestCompositedCapture(path);
    }

    // Camera control
    void updateCameraMovement(bool forward, bool backward, bool left, bool right,
                              bool up, bool down, bool fast);
    void orbitCamera(float deltaX, float deltaY);
    void panCamera(float deltaX, float deltaY);
    void zoomCamera(float delta);

    // Debug pixel reading
    /**
     * @brief Read raw pixel value from render output
     * @param x X coordinate (pixels)
     * @param y Y coordinate (pixels)
     * @param outValue Output float4 value
     * @return true if read succeeded
     */
    bool readDebugPixel(int x, int y, glm::vec4& outValue);

    /**
     * @brief The temperature a thermal camera would report for this pixel
     *
     * Reads the accumulated radiance -- the sensor chain and CLAHE write
     * elsewhere, so this is the scene rather than a detector's rendering of
     * it -- and inverts it against Planck through the SDK, using whatever the
     * thermography settings say the camera has been told. Same arithmetic the
     * CLI writes into _tapp.exr.
     *
     * @return false in a mode that carries no band radiance to invert, which
     *         is every mode but the fused thermal ones
     */
    bool readApparentTemperature(int x, int y, double& outKelvin);

    /**
     * @brief What the virtual camera is told about the surface it looks at
     *
     * Display-side only: nothing about the render changes, so this does not
     * reset the accumulation.
     */
    void setThermographyParams(const quantiloom::ThermographyParams& params) {
        m_thermography = params;
    }

    /**
     * @brief Format debug pixel value based on current debug mode
     * @param pixel Raw pixel value from readDebugPixel
     * @return Formatted string for display
     */
    QString formatDebugValue(const glm::vec4& pixel) const;

    /**
     * @brief Get current debug visualization mode
     */
    quantiloom::DebugVisualizationMode getDebugMode() const { return m_debugMode; }

    /**
     * @brief Capture current frame as Image
     * @return Image or nullptr if failed
     */
    std::unique_ptr<quantiloom::Image> captureScreenshot();

    /**
     * @brief Capture display image (with CLAHE applied if enabled)
     * @return Image as shown on screen, or nullptr if failed
     */
    std::unique_ptr<quantiloom::Image> captureDisplayImage();

    // ========================================================================
    // Atmospheric Configuration
    // ========================================================================

    /**
     * @brief Set atmosphere configuration by preset name
     * @param preset Preset name: "clear", "turbulent_clear", "urban_haze",
     *               "fog", "light_rain", "heavy_rain", "snow", "haze",
     *               "disabled". Legacy analytic names (e.g. "clear_day")
     *               are mapped to their closest NN preset.
     */

    /**
     * @brief Set atmosphere configuration directly
     * @param config NN atmosphere configuration
     */
    void setAtmosphericConfig(const quantiloom::AtmosphereNNConfig& config);

    /**
     * @brief Get current atmosphere configuration
     */
    const quantiloom::AtmosphereNNConfig& getAtmosphericConfig() const { return m_atmosphericConfig; }

    // ========================================================================
    // Environment Map (IBL)
    // ========================================================================

    /**
     * @brief Load HDR environment map for IBL
     * @param hdrPath Path to equirectangular HDR image (.exr, .hdr)
     * @return true if loading succeeded
     */
    bool loadEnvironmentMap(const QString& hdrPath);

    /**
     * @brief Check if custom environment map is loaded
     */
    bool hasEnvironmentMap() const;

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

    /**
     * @brief Check if sensor simulation is enabled
     */
    bool isSensorEnabled() const { return m_sensorEnabled; }

    /**
     * @brief Get current sensor parameters
     */
    const quantiloom::SensorParams& getSensorParams() const { return m_sensorParams; }

    // ========================================================================
    // Thermal Solve
    // ========================================================================

    void setThermalSolveParams(const quantiloom::ThermalSolveParams& params);
    void setThermalMaterial(const QString& name, const quantiloom::ThermalMaterialParams& params);
    void clearThermalMaterials();
    void setThermalSolveEnabled(bool enabled);
    void setThermalTime(double time_h);
    [[nodiscard]] quantiloom::ThermalSolveStatus thermalSolveStatus() const;

    /// The thermal element under a pick, and one element's history. Both are
    /// read-only: the trajectory replays from checkpoints and puts the hour the
    /// viewport is showing back before it returns, so a probe cannot move the
    /// picture it is a probe of.
    [[nodiscard]] quantiloom::Result<quantiloom::u32, quantiloom::String> thermalElementAt(
        const quantiloom::PickResult& pick) const;
    [[nodiscard]] quantiloom::Result<quantiloom::ThermalElementTrajectory, quantiloom::String>
    elementTrajectory(quantiloom::u32 element, double fromHour, double toHour,
                      quantiloom::u32 samples);
    /// The what-if preview. No re-solve: it moves a scalar and re-renders,
    /// which is what lets it follow a slider.
    [[nodiscard]] quantiloom::Result<void, quantiloom::String> setThermalWhatIf(
        quantiloom::ThermalSensitivityParameter parameter, double step);

    // ========================================================================
    // Display Enhancement
    // ========================================================================

    /**
     * @brief Set the tone operator and palette the viewport is displayed with
     *
     * The only tone mapping there is: without it an infrared render, whose
     * radiance sits two orders of magnitude below the displayable range, is
     * black. See DisplayControl.hpp for what the modes mean.
     */
    void setDisplayEnhancement(const quantiloom::DisplayEnhancementParams& params);

    bool isDisplayEnhancementEnabled() const { return m_displayParams.enabled; }
    [[nodiscard]] const quantiloom::DisplayEnhancementParams& displayEnhancementParams() const {
        return m_displayParams;
    }

private:
    void updateCamera(float deltaTime);
    /// Any of the six movement keys held. The frame loop idles when there is
    /// no scene, so this is also what keeps it running for WASDQE.
    [[nodiscard]] bool isCameraMoving() const;

    // Navigation limits, derived from m_sceneRadius (see setSceneScale).
    [[nodiscard]] float minOrbitDistance() const;
    [[nodiscard]] float maxOrbitDistance() const;
    [[nodiscard]] float cameraBaseSpeed() const;

    // Check if this is the first run (no pipeline cache)
    bool isFirstRun() const;

    QuantiloomVulkanWindow* m_window;

    // libQuantiloom render context
    std::unique_ptr<quantiloom::ExternalRenderContext> m_renderContext;

    // Editor overlay pass (grid, gizmo), drawn after RenderFrame's blit
    vkview::OverlayRenderer m_overlay;
    std::vector<editing::GizmoVertex> m_gizmoVertices;  // rebuilt per frame

    // Frame timing. Two figures, deliberately: wall clock around the whole
    // callback, and the SDK's own trace timing.
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;
    float m_lastFrameTimeMs = 0.0f;
    float m_lastGpuFrameTimeMs = 0.0f;

    // Accumulation state
    uint32_t m_sampleCount = 0;
    uint32_t m_targetSPP = 4;  // Default SPP for preview
    bool m_paused = false;
    /// One-shot: the next non-accumulating frame goes through
    /// ReprocessAccumulated rather than PresentAccumulated, so a display-stage
    /// change (sensor, CLAHE) shows without costing a sample. Set by
    /// requestDisplayReprocess(), consumed by startNextFrame().
    bool m_reprocessPending = false;

    // Motion-adaptive resolution. m_motionActive is the window's gesture
    // state; m_motionScale is what was chosen on its rising edge and holds for
    // the gesture. Both are meaningless while m_motionAdaptiveResolution is
    // off, which is the only state the scale is guaranteed to be 1.0 in.
    bool m_motionAdaptiveResolution = true;
    bool m_motionActive = false;
    float m_motionScale = 1.0f;

    /// Chooses the reduced scale for a gesture from the last trace time.
    [[nodiscard]] float chooseMotionScale() const;
    /// Push whatever the current state implies to the SDK, and ask for the
    /// frame that acts on it.
    void applyRenderScale(float scale);

    // Camera state
    glm::vec3 m_cameraPosition{0.0f, 1.0f, 5.0f};
    glm::vec3 m_cameraTarget{0.0f, 0.0f, 0.0f};
    glm::vec3 m_cameraUp{0.0f, 1.0f, 0.0f};
    float m_cameraFovY = 45.0f;

    // Orbit camera state
    float m_orbitDistance = 5.0f;
    /// Bounding-sphere radius of the open scene, 0 when unknown.
    float m_sceneRadius = 0.0f;
    bool m_orthographic = false;
    float m_orthoHeight = 2.0f;
    float m_orbitYaw = 0.0f;    // Horizontal angle
    float m_orbitPitch = 0.0f;  // Vertical angle

    // Movement state
    bool m_moveForward = false;
    bool m_moveBackward = false;
    bool m_moveLeft = false;
    bool m_moveRight = false;
    bool m_moveUp = false;
    bool m_moveDown = false;
    bool m_moveFast = false;

    // Spectral mode
    float m_wavelength = 550.0f;  // nm
    quantiloom::SpectralMode m_spectralMode = quantiloom::SpectralMode::RGB;
    quantiloom::DebugVisualizationMode m_debugMode = quantiloom::DebugVisualizationMode::None;

    // Lighting params (stored for restore after window minimize)
    quantiloom::LightingParams m_lightingParams = quantiloom::CreateDefaultLightingParams();
    bool m_hasLightingParams = false;  // True if set from config

    // Atmosphere configuration (NN MODTRAN surrogate)
    quantiloom::AtmosphereNNConfig m_atmosphericConfig;  // Default: disabled

    /// A display-stage setting changed (sensor, CLAHE): arm m_reprocessPending
    /// and ask for a frame, so the change shows on the accumulation as it
    /// stands. The counterpart to resetAccumulation() for settings that do not
    /// invalidate the accumulation.
    void requestDisplayReprocess();

    // Push m_atmosphericConfig to the render context, resolving the model
    // pack directory and downgrading to disabled on failure
    void applyAtmosphereToContext();
    static std::string resolveDefaultModelPackDir();

    /// Hand m_currentConfig to the SDK and re-push this class's own state over
    /// it. Called on open, and again whenever a destroyed context is rebuilt.
    /// @param isFreshOpen True when the config is a newly opened document, so
    ///        the context's resolved values replace this class's members. False
    ///        on a rebuild of the same document, where those members are the
    ///        user's edits and are re-pushed over it.
    void applyConfigToContext(bool isFreshOpen);

    // Sensor simulation
    bool m_sensorEnabled = false;
    uint32_t m_samplingSeed = quantiloom::constants::DEFAULT_SAMPLING_SEED;
    quantiloom::SensorParams m_sensorParams;
    /// What the camera is told, for the temperature readout. Display-side
    /// only: it changes what a measurement is reported as, never what is
    /// measured, so it never resets the accumulation.
    quantiloom::ThermographyParams m_thermography;
    // A CPU GenericSensor used to be instantiated alongside the GPU sensor as
    // a "fallback" and was never read by anything. Removed: sensor simulation
    // lives on the GPU side of ExternalRenderContext (sdk_export_audit D-3).

    // Spectral data pushed at runtime rather than named by the config, kept so
    // it survives a context rebuild (see the setters for why order matters).
    // Cleared when a different document is opened -- they belong to that one.
    std::vector<quantiloom::ComplexRefractiveIndex> m_runtimeRefractiveIndices;
    std::vector<quantiloom::SpectralCurve> m_runtimeSpectralCurves;
    std::optional<std::pair<quantiloom::SpectralCurve, quantiloom::SpectralCurve>> m_solarLut;
    /// The declaration behind m_solarLut when it came from a spec, so a
    /// rebuild replays the same reading rather than the resolved curves.
    std::optional<quantiloom::SolarLutSpec> m_solarLutSpec;
    QString m_solarLutBaseDir;

    // Display enhancement (CLAHE)
    quantiloom::DisplayEnhancementParams m_displayParams;

    // Initialization state
    bool m_initialized = false;
    QString m_pendingScenePath;
    QString m_currentScenePath;  // Track loaded scene for restore after minimize

    // The open document, when it is a config rather than a bare model. Kept for
    // the same reason as m_currentScenePath -- a minimize destroys the render
    // context -- but it restores far more: everything ApplyConfig sets that this
    // class holds no member for. A bare model load clears it, which is what
    // stops one document's illuminant reaching the next one's scene.
    std::shared_ptr<const quantiloom::Config> m_currentConfig;
    QString m_currentConfigBaseDir;
    /// Whether m_currentConfig has been applied to the *current* context.
    /// Cleared when the context is destroyed, which is what makes the rebuild
    /// replay fire exactly once -- a config opened before the context existed is
    /// applied by the deferred queue, and would otherwise be applied again
    /// immediately afterwards by that replay.
    bool m_configAppliedToContext = false;

    // First run shader compilation tracking
    bool m_isFirstRun = false;
    bool m_shaderCompilationChecked = false;
};
