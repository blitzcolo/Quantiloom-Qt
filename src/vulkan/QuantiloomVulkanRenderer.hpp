/**
 * @file QuantiloomVulkanRenderer.hpp
 * @brief QVulkanWindowRenderer adapter for libQuantiloom integration
 *
 * @author wtflmao
 */

#pragma once

#include <QVulkanWindowRenderer>
#include <QString>
#include <QFuture>
#include <memory>
#include <chrono>

#include <glm/glm.hpp>
#include <core/Types.hpp>
#include <renderer/LightingParams.hpp>
#include <atmos/AtmosphereNNConfig.hpp>
#include <postprocess/SensorModel.hpp>
#include <postprocess/GenericSensor.hpp>

namespace quantiloom {
struct ComplexRefractiveIndex;
}

class QuantiloomVulkanWindow;
class QProgressDialog;

namespace quantiloom {
class ExternalRenderContext;
class Scene;
struct Material;
struct Image;
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
    void loadScene(const QString& filePath);
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
    void resetAccumulation();
    uint32_t currentSampleCount() const { return m_sampleCount; }

    // Camera setup from config
    void setCamera(const glm::vec3& position, const glm::vec3& lookAt,
                   const glm::vec3& up, float fovY);

    // Scene access
    const quantiloom::Scene* getScene() const;

    // Camera info for gizmo
    void getCameraInfo(glm::vec3& position, glm::vec3& forward,
                       glm::vec3& right, glm::vec3& up) const;

    // Render context access (for transform operations)
    quantiloom::ExternalRenderContext* getRenderContext() { return m_renderContext.get(); }

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
    void setAtmosphericPreset(const QString& preset);

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
    // Display Enhancement (CLAHE)
    // ========================================================================

    /**
     * @brief Set display enhancement parameters
     * Note: Currently CLAHE is applied to screenshots only.
     *       Real-time display enhancement would require GPU implementation.
     */
    void setDisplayEnhancement(bool enabled, float clipLimit,
                               int tileSize, bool luminanceOnly);

    bool isDisplayEnhancementEnabled() const { return m_displayEnhancementEnabled; }
    float getClaheClipLimit() const { return m_claheClipLimit; }
    int getClaheTileSize() const { return m_claheTileSize; }
    bool isClaheLuminanceOnly() const { return m_claheLuminanceOnly; }

private:
    void updateCamera(float deltaTime);

    // Check if this is the first run (no pipeline cache)
    bool isFirstRun() const;

    // Show shader compilation progress dialog
    void showShaderCompilationDialog();

    QuantiloomVulkanWindow* m_window;

    // libQuantiloom render context
    std::unique_ptr<quantiloom::ExternalRenderContext> m_renderContext;

    // Frame timing
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;
    float m_lastFrameTimeMs = 0.0f;

    // Accumulation state
    uint32_t m_sampleCount = 0;
    uint32_t m_targetSPP = 4;  // Default SPP for preview

    // Camera state
    glm::vec3 m_cameraPosition{0.0f, 1.0f, 5.0f};
    glm::vec3 m_cameraTarget{0.0f, 0.0f, 0.0f};
    glm::vec3 m_cameraUp{0.0f, 1.0f, 0.0f};
    float m_cameraFovY = 45.0f;

    // Orbit camera state
    float m_orbitDistance = 5.0f;
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
    QString m_atmosphericPreset = "disabled";

    // Push m_atmosphericConfig to the render context, resolving the model
    // pack directory and downgrading to disabled on failure
    void applyAtmosphereToContext();
    static std::string resolveDefaultModelPackDir();

    // Sensor simulation
    bool m_sensorEnabled = false;
    uint32_t m_samplingSeed = quantiloom::constants::DEFAULT_SAMPLING_SEED;
    quantiloom::SensorParams m_sensorParams;
    std::unique_ptr<quantiloom::GenericSensor> m_sensor;

    // Display enhancement (CLAHE)
    bool m_displayEnhancementEnabled = false;
    float m_claheClipLimit = 2.0f;
    int m_claheTileSize = 8;
    bool m_claheLuminanceOnly = true;

    // Initialization state
    bool m_initialized = false;
    QString m_pendingScenePath;
    QString m_currentScenePath;  // Track loaded scene for restore after minimize

    // First run shader compilation tracking
    bool m_isFirstRun = false;
    bool m_shaderCompilationChecked = false;
};
