/**
 * @file ConfigManager.hpp
 * @brief TOML configuration import/export using libQuantiloom's Config class
 */

#pragma once

#include <QString>
#include <QObject>

#include <atmos/AtmosphereNNConfig.hpp>
#include <core/Config.hpp>
#include <core/Types.hpp>
#include <renderer/LightingParams.hpp>
#include <postprocess/SensorModel.hpp>

/**
 * @struct MaterialConfig
 * @brief IR material overrides from TOML config
 */
struct MaterialConfig {
    QString name;                // Material name to match
    float irEmissivity = 0.0f;   // IR emissivity [0,1]
    float irTransmittance = 0.0f; // IR transmittance [0,1]
    float irTemperature_K = 0.0f; // Surface temperature (K)
};

/**
 * @struct SceneConfig
 * @brief Extracted configuration values for UI panels
 */
struct SceneConfig {
    // [renderer]
    uint32_t width = 1280;
    uint32_t height = 720;
    uint32_t spp = 4;
    QString outputPath;
    QString environmentMap;
    /// renderer.seed -- path tracer sampling seed. Same convention as the CLI:
    /// nonzero reproduces, 0 draws a nondeterministic seed each run.
    uint32_t samplingSeed = quantiloom::constants::DEFAULT_SAMPLING_SEED;

    // [spectral]
    quantiloom::SpectralMode spectralMode = quantiloom::SpectralMode::RGB;
    float wavelength_nm = 550.0f;
    float lambda_min = 380.0f;
    float lambda_max = 760.0f;
    float delta_lambda = 5.0f;

    // [scene]
    QString gltfPath;
    QString usdPath;
    float worldUnitsToMeters = 1.0f;

    // [camera]
    float cameraPosition[3] = {0.0f, 0.0f, 5.0f};
    float cameraLookAt[3] = {0.0f, 0.0f, 0.0f};
    float cameraUp[3] = {0.0f, 1.0f, 0.0f};
    float cameraFovY = 45.0f;

    // [lighting]
    quantiloom::LightingParams lighting;

    // [atmosphere]
    QString atmosphericPreset = "disabled";  // Preset name
    bool atmosphericEnabled = false;
    /// The full NN configuration, not just the preset. The nine weather
    /// features are overridable per key in the TOML, so carrying only the
    /// preset meant every override the GUI could set was dropped on save.
    quantiloom::AtmosphereNNConfig atmosphere;

    // [sensor]
    bool sensorEnabled = false;
    quantiloom::SensorParams sensorParams;

    // [[materials]] - IR material overrides
    QVector<MaterialConfig> materialConfigs;

    // Config file base directory (for resolving relative paths)
    QString baseDir;
};

/**
 * @class ConfigManager
 * @brief Manages TOML configuration import/export
 *
 * Uses libQuantiloom's Config class for parsing, extracts values for Qt panels.
 */
class ConfigManager : public QObject {
    Q_OBJECT

public:
    explicit ConfigManager(QObject* parent = nullptr);

    /**
     * @brief Load and parse TOML config file
     * @param filePath Path to TOML file
     * @param outConfig Output configuration struct
     * @return true if parsing succeeded
     */
    bool loadConfig(const QString& filePath, SceneConfig& outConfig);

    /**
     * @brief Get the raw Config object (for passing to ExternalRenderContext)
     * @return Pointer to loaded Config, or nullptr if not loaded
     */
    const quantiloom::Config* getRawConfig() const;

    /**
     * @brief Forget the loaded config, so getRawConfig() reports none.
     *
     * Call this when opening something that is not a scene configuration --
     * a bare model. The raw config outlives the document it came from
     * otherwise, and everything keyed off it (the solar LUT, the spectral
     * curves) gets replayed onto the next scene opened.
     */
    void clearLoadedConfig();

    /**
     * @brief Export configuration to TOML file
     * @param filePath Output file path
     * @param config Configuration to export
     * @return true if export succeeded
     */
    bool exportConfig(const QString& filePath, const SceneConfig& config);

    /**
     * @brief Get last error message
     */
    QString lastError() const { return m_lastError; }

private:
    void extractSceneConfig(const quantiloom::Config& config, SceneConfig& out);
    quantiloom::SpectralMode parseSpectralMode(const std::string& modeStr);

    QString m_lastError;
    std::unique_ptr<quantiloom::Config> m_loadedConfig;
};
