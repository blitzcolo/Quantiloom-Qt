/**
 * @file ConfigManager.hpp
 * @brief TOML configuration import/export using libQuantiloom's Config class
 */

#pragma once

#include <QString>
#include <QMap>
#include <QVector>

#include <optional>

class QTextStream;
#include <QObject>

#include <atmos/AtmosphereNNConfig.hpp>
#include <core/Config.hpp>
#include <core/Types.hpp>
#include <glm/glm.hpp>
#include <renderer/LightingParams.hpp>
#include <postprocess/SensorModel.hpp>
#include <postprocess/Thermography.hpp>

/**
 * @struct MaterialThermalProps
 * @brief What a material is made of, thermally
 *
 * Solver inputs, matched to a material by name. Kept apart from
 * quantiloom::Material because that header is a layout contract Quantiloom-Qt
 * reads by offset and has no business carrying them -- so unlike every other
 * material property, these travel from the panel to the config directly rather
 * than on the material the viewport is rendering.
 */
struct MaterialThermalProps {
    float conductivity = 0.0f;     ///< W/(m K); above zero opts the material in
    float density = 2000.0f;       ///< kg/m^3
    float specificHeat = 900.0f;   ///< J/(kg K)
    float thickness = 0.2f;        ///< m
    float convection = 5.0f;       ///< W/(m^2 K)
    float shortwaveAbsorptivity = 0.7f;
    QString interiorBoundary = QStringLiteral("adiabatic");  ///< or "fixed"
    float interiorTemperature = 293.15f;
};

/**
 * @struct MaterialConfig
 * @brief Material overrides from TOML config, matched to the scene by name
 */
struct MaterialConfig {
    QString name;                // Material name to match
    float irEmissivity = 0.0f;   // IR emissivity [0,1]
    float irTransmittance = 0.0f; // IR transmittance [0,1]
    float irTemperature_K = 0.0f; // Surface temperature (K)

    /// A temperature field instead of one temperature. The path is relative to
    /// the config; the core loads it and decodes the R channel as
    /// T = value * scale + offset. Storage is UNORM8, so the resolution is
    /// scale/255 -- the defaults are the core's, and give 1.96 K per step.
    QString temperatureTexture;
    float temperatureScale = 500.0f;
    float temperatureOffset = 200.0f;

    /// Thermal properties for the surface energy balance. A conductivity
    /// above zero is what opts the material in; the rest describe masonry
    /// until told otherwise. Carried whether or not [thermal] is enabled,
    /// for the same reason the sensor parameters are.
    ///
    /// Not on quantiloom::Material: that header is a layout contract read by
    /// offset, and these are solver inputs nothing in the shader touches. So
    /// they travel from the panel to the config through this rather than on
    /// the material, which is why they are a struct of their own.
    MaterialThermalProps thermal;

    [[nodiscard]] bool hasThermal() const { return thermal.conductivity > 0.0f; }

    /// The PBR half. Written only when hasPbr is set, so an entry that exists
    /// to carry a temperature does not also assert a colour nobody chose.
    bool hasPbr = false;
    glm::vec3 baseColor{1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 1.0f;
    glm::vec3 emissive{0.0f, 0.0f, 0.0f};

    /// Which measured spectral curves this surface stands for. Carried, never
    /// interpreted: the core matches the strings against its libraries and
    /// decides what they mean. Before these existed the writer dropped them,
    /// so opening a scene with spectral bindings and saving it silently
    /// deleted them.
    ///
    /// One entry is written back as `spectral_material_ref`, several as
    /// `spectral_material_refs` -- the singular form is what older configs and
    /// the majority of materials use, and rewriting every one of them as a
    /// one-element array would be a large diff that says nothing.
    QString spectralMaterialType;   // "quantiloom_usgs", "quantiloom_ecostress", ...
    QStringList spectralMaterialRefs;
    QString spectralUnmix;          // "auto" | "texture" | "off"; empty = leave unwritten
    QString spectralWeightTexture;

    [[nodiscard]] bool hasSpectral() const { return !spectralMaterialRefs.isEmpty(); }
};

/**
 * @struct NodeConfig
 * @brief A world transform for a node the scene file already placed
 *
 * Written as a matrix rather than as translation/rotation/scale: what the
 * viewport holds is a matrix, and decomposing one into Euler angles to write it
 * down and recomposing it on load is a round trip that does not always return
 * what it was given. The core reads either.
 */
struct NodeConfig {
    QString name;
    glm::mat4 transform{1.0f};
};

/**
 * @struct DuplicateConfig
 * @brief A node pasted in the editor: a named shallow copy of a placed node
 *
 * Same matrix-not-Euler reasoning as NodeConfig. The core resolves these
 * before [[nodes]] and in file order, so a copy of a copy is written after
 * its source.
 */
struct DuplicateConfig {
    QString sourceName;
    QString name;
    glm::mat4 transform{1.0f};
};

/**
 * @struct HyperspectralConfig
 * @brief The [hyperspectral] block, read by the core's offline renderer
 *
 * Carried whole rather than field by field because the viewport honours none
 * of it -- a cube is rendered offline. It exists here so a config written for
 * the CLI survives being opened and saved by the GUI.
 */
struct HyperspectralConfig {
    float wavelengthMin_nm = 400.0f;
    float wavelengthMax_nm = 2500.0f;
    float wavelengthStep_nm = 10.0f;
    bool useGpuReconstruction = true;
    bool saveIntermediates = false;
    QString outputFormat = QStringLiteral("envi_bsq");
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
    /// renderer.environment_map_enabled -- whether image-based lighting
    /// contributes. Independent of whether a map is named: with no path the
    /// built-in sky is what lights the scene, and it always has.
    bool environmentMapEnabled = true;
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
    /// camera.projection / camera.ortho_height. Absent means perspective,
    /// which is what every scene written before the key existed meant.
    bool cameraOrthographic = false;
    float cameraOrthoHeight = 0.0f;

    // [lighting]
    quantiloom::LightingParams lighting;

    // [atmosphere]
    QString atmosphericPreset = "disabled";  // Preset name
    bool atmosphericEnabled = false;
    /// The full NN configuration, not just the preset. The nine weather
    /// features are overridable per key in the TOML, so carrying only the
    /// preset meant every override the GUI could set was dropped on save.
    quantiloom::AtmosphereNNConfig atmosphere;

    /// The analytic sky, for when the network model is off. The emissivity the
    /// shader reads is derived from these two by the SDK and lives in
    /// LightingParams; the humidity itself has nowhere else to live, so a
    /// document that set it would lose it on save without this.
    bool clearSkyModel = false;
    float skyAirTemperatureK = 288.15f;
    float skyRelativeHumidity = 50.0f;

    // [sensor]
    bool sensorEnabled = false;
    quantiloom::SensorParams sensorParams;

    // [thermography] -- what the virtual camera is told about the surface it
    // looks at, so a render can be turned into the temperature map a thermal
    // camera would have displayed. The CLI writes that map; here the settings
    // drive the viewport's pixel readout and are carried back to the file.
    bool thermographyEnabled = false;
    quantiloom::ThermographyParams thermography;

    // [thermal] -- the surface energy balance. The temperatures it produces
    // replace whatever the materials were given, so a scene that enables it
    // stops needing a temperature typed into it at all. Offline only: a solve
    // is a state the viewport would have to be told about rather than compute.
    bool thermalEnabled = false;
    double thermalTimeH = 12.0;
    double thermalStartTimeH = 0.0;
    double thermalTimestepS = 60.0;
    int thermalLayers = 10;
    QString thermalInitial = QStringLiteral("steady");
    double thermalInitialTemperatureK = 288.15;
    double thermalSunIrradiance = 0.0;
    int thermalExchangeRays = 256;
    int thermalExchangeTopK = 32;
    QString thermalForcingFile;

    /// [material] albedo -- the fallback surface for scenes that bring no
    /// materials of their own. Required by the core's strict reading, so a
    /// document saved without it renders here and is rejected by the CLI.
    glm::vec3 defaultAlbedo{0.8f, 0.8f, 0.8f};

    // [[materials]] - IR material overrides
    QVector<MaterialConfig> materialConfigs;

    // [[nodes]] -- transforms for nodes edited since the document was opened
    QVector<NodeConfig> nodeConfigs;

    // [[duplicates]] -- nodes pasted in the editor, shallow copies by name
    QVector<DuplicateConfig> duplicateConfigs;

    // scene.removed_nodes -- nodes the file placed but the user deleted
    QStringList removedNodes;

    // ====================================================================
    // Quantitative spectral state. No widget owns any of it yet, but the
    // core reads all of it, so a hand-authored config used to lose these
    // sections the first time it was saved from the GUI -- the failure this
    // block exists to prevent. Everything here is document state: it
    // describes what the scene *is*, not how it is being looked at.
    // ====================================================================

    /// [spectral_curves] -- material name to reflectance CSV path.
    QMap<QString, QString> spectralCurves;

    /// [refractive_index] -- material name to RefractiveIndex.INFO YAML path.
    QMap<QString, QString> refractiveIndexFiles;

    /// lighting.solar_lut -- a file path, or the literal "equal_energy".
    /// Empty means the key was absent; without it the quantitative spectral
    /// modes render black, which is why it must survive a round trip.
    QString solarLutPath;
    /// lighting.solar_lut_columns -- [direct, diffuse]. Empty keeps the
    /// core's libRadtran default of [2, 3] rather than asserting it.
    QVector<int> solarLutColumns;
    /// lighting.solar_lut_normalise -- "unit_luminance", or empty for none.
    QString solarLutNormalise;
    /// lighting.solar_lut_diffuse_is_global -- ASTM G-173's column 3 is
    /// global rather than sky.
    bool solarLutDiffuseIsGlobal = false;

    /// [spectral] NMF basis: the measured-material database and the band
    /// reconstructed from it.
    QString basisFile;
    QString materialsJson;
    QString spectralBand;

    // The three below are optional so that a file which never spelled them
    // out does not acquire them on save, the same convention as
    // MaterialConfig::hasPbr.

    /// scene.default_temperature_k -- backfilled onto materials with no IR
    /// temperature of their own.
    std::optional<float> defaultTemperatureK;
    /// quality.fail_on_srgb_upsample -- refuse to guess a spectrum from sRGB.
    std::optional<bool> failOnSrgbUpsample;
    /// quality.log_material_sources -- log where each material's data came from.
    std::optional<bool> logMaterialSources;

    /// [hyperspectral] -- honoured by the offline renderer only.
    std::optional<HyperspectralConfig> hyperspectral;

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
     * @brief The loaded config, shared with whoever needs it to outlive the call
     *
     * The renderer keeps it: a destroyed render context is rebuilt by applying
     * the same config again, which restores state no widget holds.
     *
     * @return The config, or nullptr when the open document is not one
     */
    std::shared_ptr<const quantiloom::Config> sharedRawConfig() const;

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
     * @brief Serialise the same document exportConfig() writes, without a file
     *
     * One writer, two destinations: an agent asking what the open document
     * currently says gets the bytes that Save would have produced, rather than
     * a second rendering of the same struct that could drift from it.
     */
    QString exportConfigToString(const SceneConfig& config);

    /**
     * @brief Get last error message
     */
    QString lastError() const { return m_lastError; }

private:
    /// The single writer. exportConfig() points it at a file,
    /// exportConfigToString() at a string.
    void writeConfig(QTextStream& out, const SceneConfig& config);
    void extractSceneConfig(const quantiloom::Config& config, SceneConfig& out);
    /// The sections no widget owns, split out only for length. Called by
    /// extractSceneConfig; see SceneConfig for why they are carried at all.
    void extractQuantitativeSpectral(const quantiloom::Config& config, SceneConfig& out);
    quantiloom::SpectralMode parseSpectralMode(const std::string& modeStr);

    QString m_lastError;
    std::shared_ptr<quantiloom::Config> m_loadedConfig;
};
