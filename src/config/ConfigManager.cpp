/**
 * @file ConfigManager.cpp
 * @brief TOML configuration import/export implementation
 */

#include "ConfigManager.hpp"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>

#include <core/Log.hpp>
#include <postprocess/PostprocessConfig.hpp>
#include <renderer/LightingParams.hpp>

#include <glm/gtc/matrix_transform.hpp>

// Reading is delegated to the SDK's PostprocessConfig::ParseSensorParams, but
// writing is hand-rolled TOML below, so the two drift apart whenever the SDK
// adds a field: noiseSeed was parsed on load and dropped on save, which made a
// config that had been through this GUI stop matching the one the CLI reads.
//
// This assert is the tripwire. If it fires, the SDK's SensorParams changed --
// diff it against ParseSensorParams and against the [sensor] block in
// saveConfig() below, then update the expected size. The SDK/GUI boundary is
// already a single MSVC toolchain (SRS CON-04), so the layout is well-defined.
static_assert(sizeof(quantiloom::SensorParams) == 88,
              "SensorParams changed size: a field was added or removed. Check "
              "that saveConfig() still writes every key ParseSensorParams reads "
              "before updating this number.");

// TOML basic strings give the backslash to escape sequences, so streaming a
// value into quotes raw writes a file the loader refuses to read back: a
// Windows path like D:\Quantiloom-Qt fails the very next open with
// "unknown escape sequence '\Q'". Every quoted string writeConfig() emits --
// values and the quoted keys of [spectral_curves]/[refractive_index] alike --
// goes through here.
static QString tomlQuoted(const QString& s) {
    QString quoted;
    quoted.reserve(s.size() + 2);
    quoted += QLatin1Char('"');
    for (const QChar c : s) {
        switch (c.unicode()) {
            case '"':  quoted += QLatin1String("\\\""); break;
            case '\\': quoted += QLatin1String("\\\\"); break;
            case '\n': quoted += QLatin1String("\\n");  break;
            case '\r': quoted += QLatin1String("\\r");  break;
            case '\t': quoted += QLatin1String("\\t");  break;
            default:
                if (c.unicode() < 0x20) {
                    quoted += QStringLiteral("\\u%1").arg(int(c.unicode()), 4, 16,
                                                          QLatin1Char('0'));
                } else {
                    quoted += c;
                }
        }
    }
    quoted += QLatin1Char('"');
    return quoted;
}

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
{
}

bool ConfigManager::loadConfig(const QString& filePath, SceneConfig& outConfig) {
    // Use libQuantiloom's Config::Load
    auto result = quantiloom::Config::Load(filePath.toStdString());

    if (!result.has_value()) {
        m_lastError = QString::fromStdString(result.error());
        return false;
    }

    // Store the loaded config for later use
    m_loadedConfig = std::make_shared<quantiloom::Config>(std::move(result.value()));

    // Extract values for UI panels
    extractSceneConfig(*m_loadedConfig, outConfig);

    // Store base directory for resolving relative paths
    QFileInfo fileInfo(filePath);
    outConfig.baseDir = fileInfo.absolutePath();

    return true;
}

const quantiloom::Config* ConfigManager::getRawConfig() const {
    return m_loadedConfig.get();
}

std::shared_ptr<const quantiloom::Config> ConfigManager::sharedRawConfig() const {
    return m_loadedConfig;
}

void ConfigManager::clearLoadedConfig() {
    m_loadedConfig.reset();
}

void ConfigManager::extractSceneConfig(const quantiloom::Config& config, SceneConfig& out) {
    // Initialize with defaults
    out.lighting = quantiloom::CreateDefaultLightingParams();

    // [renderer]
    auto resolution = config.GetArray<quantiloom::i32>("renderer.resolution");
    if (resolution.size() >= 2) {
        out.width = static_cast<uint32_t>(resolution[0]);
        out.height = static_cast<uint32_t>(resolution[1]);
    }
    out.spp = config.Get<quantiloom::u32>("renderer.spp", 1);
    out.outputPath = QString::fromStdString(config.GetString("renderer.output", "output.exr"));
    out.environmentMap = QString::fromStdString(config.GetString("renderer.environment_map", ""));
    out.environmentMapEnabled = config.Get<bool>("renderer.environment_map_enabled", true);
    out.samplingSeed = config.Get<quantiloom::u32>(
        "renderer.seed", quantiloom::constants::DEFAULT_SAMPLING_SEED);

    // [spectral]
    std::string modeStr = config.GetString("spectral.mode", "rgb");
    out.spectralMode = parseSpectralMode(modeStr);
    // Same rule as the CLI, for the same modes it applies it to: an absent
    // wavelength means the centre of the band being rendered, not 550 nm. A
    // thermal config that did not spell the key out was being rendered here at
    // a visible wavelength.
    out.wavelength_nm = 550.0f;
    const bool wavelengthMatters =
        out.spectralMode == quantiloom::SpectralMode::Single ||
        out.spectralMode == quantiloom::SpectralMode::MWIR_Fused ||
        out.spectralMode == quantiloom::SpectralMode::LWIR_Fused ||
        out.spectralMode == quantiloom::SpectralMode::SWIR_Fused;
    if (wavelengthMatters) {
        if (config.Has("spectral.wavelength_nm")) {
            out.wavelength_nm = config.GetFloat("spectral.wavelength_nm", 550.0f);
        } else if (auto band = quantiloom::GetFusedBandInfo(out.spectralMode)) {
            out.wavelength_nm = band->CenterNm();
        }
    }
    out.lambda_min = config.GetFloat("spectral.lambda_min", 380.0f);
    out.lambda_max = config.GetFloat("spectral.lambda_max", 760.0f);
    out.delta_lambda = config.GetFloat("spectral.delta_lambda", 5.0f);

    // [scene]
    out.gltfPath = QString::fromStdString(config.GetString("scene.gltf", ""));
    out.usdPath = QString::fromStdString(config.GetString("scene.usd", ""));
    out.worldUnitsToMeters = config.GetFloat("scene.world_units_to_meters", 1.0f);

    // [camera]
    auto camPos = config.GetArray<quantiloom::f32>("camera.position");
    if (camPos.size() >= 3) {
        out.cameraPosition[0] = camPos[0];
        out.cameraPosition[1] = camPos[1];
        out.cameraPosition[2] = camPos[2];
    }

    auto camLookAt = config.GetArray<quantiloom::f32>("camera.look_at");
    if (camLookAt.size() >= 3) {
        out.cameraLookAt[0] = camLookAt[0];
        out.cameraLookAt[1] = camLookAt[1];
        out.cameraLookAt[2] = camLookAt[2];
    }

    auto camUp = config.GetArray<quantiloom::f32>("camera.up");
    if (camUp.size() >= 3) {
        out.cameraUp[0] = camUp[0];
        out.cameraUp[1] = camUp[1];
        out.cameraUp[2] = camUp[2];
    }

    // 60, the core Camera's own default -- not 45, which framed every config
    // without the key differently here than in the CLI.
    out.cameraFovY = config.GetFloat("camera.fov_y", 60.0f);

    // [lighting]
    auto sunDir = config.GetArray<quantiloom::f32>("lighting.sun_direction");
    if (sunDir.size() >= 3) {
        out.lighting.sunDirection = glm::normalize(glm::vec3(sunDir[0], sunDir[1], sunDir[2]));
    }

    auto sunRad = config.GetArray<quantiloom::f32>("lighting.sun_radiance");
    if (sunRad.size() >= 3) {
        out.lighting.sunRadiance_rgb = glm::vec3(sunRad[0], sunRad[1], sunRad[2]);
        out.lighting.sunRadiance_spectral = (sunRad[0] + sunRad[1] + sunRad[2]) / 3.0f;
    }

    auto skyRad = config.GetArray<quantiloom::f32>("lighting.sky_radiance");
    if (skyRad.size() >= 3) {
        out.lighting.skyRadiance_rgb = glm::vec3(skyRad[0], skyRad[1], skyRad[2]);
        out.lighting.skyRadiance_spectral = (skyRad[0] + skyRad[1] + skyRad[2]) / 3.0f;
    }

    out.lighting.atmosphereTemperature_K = config.GetFloat("lighting.atmosphere_temperature_k", 260.0f);
    // skyEmissivityClear is not read from the file: the core derives it from
    // [atmosphere] sky_model, air_temperature_k and relative_humidity, which
    // are read below. Setting it from a stale key here would put a second
    // opinion about the sky into the params the viewport uploads.
    out.lighting.worldUnitsToMeters = out.worldUnitsToMeters;

    // [quality] VIS_Fused chromaticity correction
    out.lighting.chromaR_correction = config.GetFloat(
        "quality.chroma_r_correction",
        quantiloom::LightingDefaults::CHROMA_R_CORRECTION);
    out.lighting.chromaB_correction = config.GetFloat(
        "quality.chroma_b_correction",
        quantiloom::LightingDefaults::CHROMA_B_CORRECTION);

    // [renderer] Shadow ray control. On unless the config says otherwise, which
    // is the core's default -- with it off here, every one of the shipped
    // configs (none of them spell the key out) rendered shadowless in the
    // viewport and shadowed from the CLI.
    bool enableShadowRays = config.Get<bool>("renderer.enable_shadow_rays", true);
    out.lighting.enableShadowRays = enableShadowRays ? 1u : 0u;

    // [atmospheric] / [atmosphere] — prefer the NN [atmosphere] section,
    // fall back to the legacy [atmospheric] one
    out.atmosphericPreset = QString::fromStdString(
        config.GetString("atmosphere.preset",
                         config.GetString("atmospheric.preset", "disabled")));
    // Map legacy analytic preset names to their closest NN preset
    const QString p = out.atmosphericPreset.toLower();
    if (p == "clear_day" || p == "mountain_top") {
        out.atmosphericPreset = "clear";
    } else if (p == "hazy") {
        out.atmosphericPreset = "haze";
    } else if (p == "polluted_urban") {
        out.atmosphericPreset = "urban_haze";
    } else if (p == "mars") {
        out.atmosphericPreset = "disabled";
    }
    out.atmosphericEnabled = (out.atmosphericPreset != "disabled");

    // The nine weather features, each overridable on its own. Reading them
    // back is half of making the round trip lossless; exportConfig() writes
    // the same keys.
    out.atmosphere.ApplyPreset(out.atmosphericPreset.toStdString());
    out.atmosphere.modelPackDir = config.GetString("atmosphere.model_pack", "");
    out.atmosphere.preset = out.atmosphericPreset.toStdString();
    // Naming a preset is the opt-in; the model pack is a deployment artifact
    // that ships next to the executable, so a scene does not have to name a
    // path to it. An explicit atmosphere.model_pack still wins, and
    // QuantiloomVulkanRenderer::applyAtmosphereToContext resolves an empty one.
    out.atmosphere.enabled = out.atmosphericEnabled;

    out.atmosphere.atmosModel = config.GetFloat("atmosphere.atmos_model",
        static_cast<quantiloom::f32>(out.atmosphere.atmosModel));
    out.atmosphere.ihaze = config.GetFloat("atmosphere.ihaze",
        static_cast<quantiloom::f32>(out.atmosphere.ihaze));
    out.atmosphere.icld = config.GetFloat("atmosphere.icld",
        static_cast<quantiloom::f32>(out.atmosphere.icld));
    out.atmosphere.visKm = config.GetFloat("atmosphere.vis_km",
        static_cast<quantiloom::f32>(out.atmosphere.visKm));
    out.atmosphere.rainrtMmH = config.GetFloat("atmosphere.rainrt_mm_h",
        static_cast<quantiloom::f32>(out.atmosphere.rainrtMmH));
    out.atmosphere.tGroundK = config.GetFloat("atmosphere.t_ground_K",
        static_cast<quantiloom::f32>(out.atmosphere.tGroundK));
    out.atmosphere.rh = config.GetFloat("atmosphere.rh",
        static_cast<quantiloom::f32>(out.atmosphere.rh));
    out.atmosphere.pHPa = config.GetFloat("atmosphere.p_hPa",
        static_cast<quantiloom::f32>(out.atmosphere.pHPa));
    out.atmosphere.h2oScale = config.GetFloat("atmosphere.h2o_scale",
        static_cast<quantiloom::f32>(out.atmosphere.h2oScale));

    // [atmosphere] analytic sky. Read whether or not it is selected, for the
    // same reason the sensor params are: the values outlive the switch.
    out.clearSkyModel =
        config.GetString("atmosphere.sky_model", "isotropic") == "clear_sky";
    out.skyAirTemperatureK = config.Get<float>("atmosphere.air_temperature_k", 288.15f);
    out.skyRelativeHumidity = config.Get<float>("atmosphere.relative_humidity", 50.0f);

    // [sensor]
    out.sensorEnabled = config.Get<bool>("sensor.enabled", false);
    // Always parse sensor params so they're available if user enables later
    out.sensorParams = quantiloom::PostprocessConfig::ParseSensorParams(config);

    // [thermography] - same rule: parsed whether or not it is on, so turning
    // it off and saving does not discard what the camera was told.
    out.thermographyEnabled = quantiloom::PostprocessConfig::IsThermographyEnabled(config);
    out.thermography = quantiloom::PostprocessConfig::ParseThermographyParams(config);

    // [material] - the scene-wide fallback albedo
    if (const auto albedo = config.GetFloatArray("material.albedo"); albedo.size() >= 3) {
        out.defaultAlbedo = glm::vec3(albedo[0], albedo[1], albedo[2]);
    }

    // [[materials]] - parse material IR overrides
    // TOML format:
    // [[materials]]
    // name = "material_name"
    // ir_emissivity = 0.8
    // ir_transmittance = 0.0
    // ir_temperature_k = 350.0
    out.materialConfigs.clear();
    auto materialTables = config.GetTableArray("materials");
    for (const auto& matTable : materialTables) {
        std::string name = matTable.GetString("name", "");
        if (name.empty()) {
            continue;
        }

        MaterialConfig matConfig;
        matConfig.name = QString::fromStdString(name);
        matConfig.irEmissivity = matTable.GetFloat("ir_emissivity", 0.0f);
        matConfig.irTransmittance = matTable.GetFloat("ir_transmittance", 0.0f);
        matConfig.irTemperature_K = matTable.GetFloat("ir_temperature_k", 0.0f);

        // A temperature map, and the kelvin mapping it is decoded with. Scale
        // and offset are read whether or not a path is present: they also
        // retune a map a glTF or USD scene provided.
        matConfig.temperatureTexture =
            QString::fromStdString(matTable.GetString("temperature_texture", ""));
        matConfig.temperatureScale = matTable.GetFloat("temperature_scale", 500.0f);
        matConfig.temperatureOffset = matTable.GetFloat("temperature_offset", 200.0f);

        // The PBR half, kept only if the file actually carries it -- so a
        // reload/save cycle does not invent one for an entry that had none.
        if (const auto colour = matTable.GetFloatArray("base_color"); colour.size() >= 3) {
            matConfig.baseColor = glm::vec3(colour[0], colour[1], colour[2]);
            matConfig.hasPbr = true;
        }
        if (matTable.Has("metallic")) {
            matConfig.metallic = matTable.GetFloat("metallic", 0.0f);
            matConfig.hasPbr = true;
        }
        if (matTable.Has("roughness")) {
            matConfig.roughness = matTable.GetFloat("roughness", 1.0f);
            matConfig.hasPbr = true;
        }
        if (const auto emissive = matTable.GetFloatArray("emissive"); emissive.size() >= 3) {
            matConfig.emissive = glm::vec3(emissive[0], emissive[1], emissive[2]);
            matConfig.hasPbr = true;
        }

        // The spectral binding, read so the writer can put it back. Both forms
        // land in the same list; which one gets written out depends only on
        // how many entries there are.
        matConfig.spectralMaterialType =
            QString::fromStdString(matTable.GetString("spectral_material_type", ""));
        if (const auto ref = matTable.GetString("spectral_material_ref", ""); !ref.empty()) {
            matConfig.spectralMaterialRefs << QString::fromStdString(ref);
        }
        for (const auto& ref : matTable.GetStringArray("spectral_material_refs")) {
            matConfig.spectralMaterialRefs << QString::fromStdString(ref);
        }
        matConfig.spectralUnmix =
            QString::fromStdString(matTable.GetString("spectral_unmix", ""));
        matConfig.spectralWeightTexture =
            QString::fromStdString(matTable.GetString("spectral_weight_texture", ""));

        out.materialConfigs.append(matConfig);

        qDebug() << "Loaded material config:" << matConfig.name
                 << "emissivity=" << matConfig.irEmissivity
                 << "transmittance=" << matConfig.irTransmittance
                 << "temperature=" << matConfig.irTemperature_K << "K";
    }
    // [[nodes]] - transform overrides, matched to the scene by node name
    out.nodeConfigs.clear();
    for (const auto& nodeTable : config.GetTableArray("nodes")) {
        const std::string name = nodeTable.GetString("name", "");
        if (name.empty()) {
            continue;
        }

        NodeConfig nodeConfig;
        nodeConfig.name = QString::fromStdString(name);

        if (const auto matrix = nodeTable.GetFloatArray("matrix"); matrix.size() == 16) {
            for (int c = 0; c < 4; ++c) {
                for (int r = 0; r < 4; ++r) {
                    nodeConfig.transform[c][r] = matrix[static_cast<size_t>(c * 4 + r)];
                }
            }
        } else {
            // The core also reads translation/rotation/scale, which a person may
            // well have written by hand; recompose them the same way it does.
            const auto translation = nodeTable.GetFloatArray("translation");
            const auto rotation = nodeTable.GetFloatArray("rotation_euler_degrees");
            const auto scale = nodeTable.GetFloatArray("scale");
            glm::mat4 transform(1.0f);
            if (translation.size() >= 3) {
                transform = glm::translate(
                    transform, glm::vec3(translation[0], translation[1], translation[2]));
            }
            if (rotation.size() >= 3) {
                transform = glm::rotate(transform, glm::radians(rotation[0]), glm::vec3(1, 0, 0));
                transform = glm::rotate(transform, glm::radians(rotation[1]), glm::vec3(0, 1, 0));
                transform = glm::rotate(transform, glm::radians(rotation[2]), glm::vec3(0, 0, 1));
            }
            if (scale.size() >= 3) {
                transform = glm::scale(transform, glm::vec3(scale[0], scale[1], scale[2]));
            }
            nodeConfig.transform = transform;
        }

        out.nodeConfigs.append(nodeConfig);
    }

    // [[duplicates]] - pasted nodes, and scene.removed_nodes - deleted ones.
    // Both are read so a save can write them back; the scene-side effect
    // (creating the copies, tombstoning the removals) is ApplyConfig's.
    out.duplicateConfigs.clear();
    for (const auto& dupTable : config.GetTableArray("duplicates")) {
        const std::string source = dupTable.GetString("source", "");
        const std::string name = dupTable.GetString("name", "");
        if (source.empty() || name.empty()) {
            continue;
        }

        DuplicateConfig dup;
        dup.sourceName = QString::fromStdString(source);
        dup.name = QString::fromStdString(name);

        if (const auto matrix = dupTable.GetFloatArray("matrix"); matrix.size() == 16) {
            for (int c = 0; c < 4; ++c) {
                for (int r = 0; r < 4; ++r) {
                    dup.transform[c][r] = matrix[static_cast<size_t>(c * 4 + r)];
                }
            }
        } else {
            const auto translation = dupTable.GetFloatArray("translation");
            const auto rotation = dupTable.GetFloatArray("rotation_euler_degrees");
            const auto scale = dupTable.GetFloatArray("scale");
            glm::mat4 transform(1.0f);
            if (translation.size() >= 3) {
                transform = glm::translate(
                    transform, glm::vec3(translation[0], translation[1], translation[2]));
            }
            if (rotation.size() >= 3) {
                transform = glm::rotate(transform, glm::radians(rotation[0]), glm::vec3(1, 0, 0));
                transform = glm::rotate(transform, glm::radians(rotation[1]), glm::vec3(0, 1, 0));
                transform = glm::rotate(transform, glm::radians(rotation[2]), glm::vec3(0, 0, 1));
            }
            if (scale.size() >= 3) {
                transform = glm::scale(transform, glm::vec3(scale[0], scale[1], scale[2]));
            }
            dup.transform = transform;
        }

        out.duplicateConfigs.append(dup);
    }

    out.removedNodes.clear();
    for (const auto& name : config.GetStringArray("scene.removed_nodes")) {
        if (!name.empty()) {
            out.removedNodes.append(QString::fromStdString(name));
        }
    }

    extractQuantitativeSpectral(config, out);
}

void ConfigManager::extractQuantitativeSpectral(const quantiloom::Config& config,
                                                SceneConfig& out) {
    // None of this drives a widget yet. It is read so that writeConfig() can
    // put it back: a hand-authored quantitative config opened and saved here
    // used to come out stripped of every section below, silently.

    // [spectral_curves] and [refractive_index]: name -> path maps.
    out.spectralCurves.clear();
    if (config.HasSection("spectral_curves")) {
        for (const auto& [materialName, csvPath] : config.GetSection("spectral_curves")) {
            out.spectralCurves.insert(QString::fromStdString(materialName),
                                      QString::fromStdString(csvPath));
        }
    }

    out.refractiveIndexFiles.clear();
    if (config.HasSection("refractive_index")) {
        for (const auto& [materialName, yamlPath] : config.GetSection("refractive_index")) {
            out.refractiveIndexFiles.insert(QString::fromStdString(materialName),
                                            QString::fromStdString(yamlPath));
        }
    }

    // lighting.solar_lut*: the illuminant. Absent stays empty rather than
    // becoming a default, because writing a path the file never named would
    // change what renders.
    out.solarLutPath = QString::fromStdString(config.GetString("lighting.solar_lut", ""));
    out.solarLutColumns.clear();
    for (const auto column : config.GetArray<quantiloom::i32>("lighting.solar_lut_columns")) {
        out.solarLutColumns.append(static_cast<int>(column));
    }
    out.solarLutNormalise =
        QString::fromStdString(config.GetString("lighting.solar_lut_normalise", ""));
    out.solarLutDiffuseIsGlobal =
        config.Get<bool>("lighting.solar_lut_diffuse_is_global", false);

    // [spectral] NMF basis.
    out.basisFile = QString::fromStdString(config.GetString("spectral.basis_file", ""));
    out.materialsJson = QString::fromStdString(config.GetString("spectral.materials_json", ""));
    out.spectralBand = QString::fromStdString(config.GetString("spectral.band", ""));

    // Optional scalars: presence is the value, so Has() rather than a default.
    out.defaultTemperatureK.reset();
    if (config.Has("scene.default_temperature_k")) {
        out.defaultTemperatureK = config.GetFloat("scene.default_temperature_k", 300.0f);
    }
    out.failOnSrgbUpsample.reset();
    if (config.Has("quality.fail_on_srgb_upsample")) {
        out.failOnSrgbUpsample = config.Get<bool>("quality.fail_on_srgb_upsample", false);
    }
    out.logMaterialSources.reset();
    if (config.Has("quality.log_material_sources")) {
        out.logMaterialSources = config.Get<bool>("quality.log_material_sources", false);
    }

    // [hyperspectral]: carried whole, honoured by the offline renderer only.
    out.hyperspectral.reset();
    if (config.HasSection("hyperspectral")) {
        HyperspectralConfig hs;
        hs.wavelengthMin_nm = config.GetFloat("hyperspectral.wavelength_min_nm", 400.0f);
        hs.wavelengthMax_nm = config.GetFloat("hyperspectral.wavelength_max_nm", 2500.0f);
        hs.wavelengthStep_nm = config.GetFloat("hyperspectral.wavelength_step_nm", 10.0f);
        hs.useGpuReconstruction =
            config.Get<bool>("hyperspectral.use_gpu_reconstruction", true);
        hs.saveIntermediates = config.Get<bool>("hyperspectral.save_intermediates", false);
        hs.outputFormat = QString::fromStdString(
            config.GetString("hyperspectral.output_format", "envi_bsq"));
        out.hyperspectral = hs;
    }
}

quantiloom::SpectralMode ConfigManager::parseSpectralMode(const std::string& modeStr) {
    // The SDK's parser, not a second one. The hand-written version this
    // replaces lower-cased its input first, which lost every uppercase alias
    // the core accepts ("VIS", "LWIR", ...), and had no case for
    // "multispectral" at all -- all of them fell through to RGB without a
    // word, so a config asking for a band rendered in another one.
    auto parsed = quantiloom::ParseSpectralMode(modeStr);
    if (parsed.has_value()) {
        return *parsed;
    }
    QL_LOG_WARN("{} - falling back to RGB", parsed.error());
    return quantiloom::SpectralMode::RGB;
}

bool ConfigManager::exportConfig(const QString& filePath, const SceneConfig& config) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_lastError = tr("Cannot open file for writing: %1").arg(filePath);
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    writeConfig(out, config);

    file.close();
    return true;
}

QString ConfigManager::exportConfigToString(const SceneConfig& config) {
    QString text;
    QTextStream out(&text);
    writeConfig(out, config);
    return text;
}

void ConfigManager::writeConfig(QTextStream& out, const SceneConfig& config) {
    // Header
    out << "# ============================================================================\n";
    out << "# Quantiloom Scene Configuration\n";
    out << "# Exported from Quantiloom Qt GUI\n";
    out << "# ============================================================================\n\n";

    // [renderer]
    out << "[renderer]\n";
    out << "resolution = [" << config.width << ", " << config.height << "]\n";
    out << "spp = " << config.spp << "\n";
    out << "seed = " << config.samplingSeed << "\n";
    if (!config.outputPath.isEmpty()) {
        out << "output = " << tomlQuoted(config.outputPath) << "\n";
    }
    if (!config.environmentMap.isEmpty()) {
        out << "environment_map = " << tomlQuoted(config.environmentMap) << "\n";
    }
    // Written only when off: true is the default, and a key restating a default
    // on every save is noise in the diff of a hand-edited file.
    if (!config.environmentMapEnabled) {
        out << "environment_map_enabled = false\n";
    }
    // shadow ray control
    if (config.lighting.enableShadowRays) {
        out << "enable_shadow_rays = true\n";
    }
    out << "\n";

    // [spectral]
    out << "[spectral]\n";
    QString modeStr;
    switch (config.spectralMode) {
        case quantiloom::SpectralMode::Single: modeStr = "single"; break;
        case quantiloom::SpectralMode::VIS_Fused: modeStr = "vis_fused"; break;
        case quantiloom::SpectralMode::MWIR_Fused: modeStr = "mwir_fused"; break;
        case quantiloom::SpectralMode::LWIR_Fused: modeStr = "lwir_fused"; break;
        case quantiloom::SpectralMode::SWIR_Fused: modeStr = "swir_fused"; break;
        case quantiloom::SpectralMode::NIR_Fused: modeStr = "nir_fused"; break;
        // Not offered in the viewport (ModeCatalog leaves it out; a cube is
        // rendered offline), but a config that asks for it must not be
        // downgraded to RGB by the act of saving it.
        case quantiloom::SpectralMode::Multispectral: modeStr = "multispectral"; break;
        default: modeStr = "rgb"; break;
    }
    out << "mode = " << tomlQuoted(modeStr) << "\n";
    // Written for every mode, not just Single: PostprocessConfig reads
    // spectral.wavelength_nm into SensorParams::wavelength_nm regardless of
    // mode, so omitting it here silently reset the sensor's peak wavelength
    // to 550 nm on the next load.
    out << "wavelength_nm = " << config.wavelength_nm << "\n";
    out << "lambda_min = " << config.lambda_min << "\n";
    out << "lambda_max = " << config.lambda_max << "\n";
    out << "delta_lambda = " << config.delta_lambda << "\n";
    // The NMF measured-material database. The core needs basis_file and
    // materials_json together -- either alone loads nothing -- so neither is
    // written on its own.
    if (!config.basisFile.isEmpty() && !config.materialsJson.isEmpty()) {
        out << "basis_file = " << tomlQuoted(config.basisFile) << "\n";
        out << "materials_json = " << tomlQuoted(config.materialsJson) << "\n";
        if (!config.spectralBand.isEmpty()) {
            out << "band = " << tomlQuoted(config.spectralBand) << "\n";
        }
    }
    out << "\n";

    // [scene]
    out << "[scene]\n";
    if (!config.gltfPath.isEmpty()) {
        out << "gltf = " << tomlQuoted(config.gltfPath) << "\n";
    }
    if (!config.usdPath.isEmpty()) {
        out << "usd = " << tomlQuoted(config.usdPath) << "\n";
    }
    out << "world_units_to_meters = " << config.worldUnitsToMeters << "\n";
    // Backfilled onto materials with no IR temperature of their own. Written
    // only when the file named it, so a scene that never asked for the
    // backfill does not acquire one by being saved.
    if (config.defaultTemperatureK) {
        out << "default_temperature_k = " << *config.defaultTemperatureK << "\n";
    }
    if (!config.removedNodes.isEmpty()) {
        // Nodes the file placed but the user deleted; the core tombstones
        // them at load, matched by name
        out << "removed_nodes = [";
        for (int i = 0; i < config.removedNodes.size(); ++i) {
            if (i > 0) {
                out << ", ";
            }
            out << tomlQuoted(config.removedNodes[i]);
        }
        out << "]\n";
    }
    out << "\n";

    // [camera]
    out << "[camera]\n";
    out << "position = [" << config.cameraPosition[0] << ", "
                          << config.cameraPosition[1] << ", "
                          << config.cameraPosition[2] << "]\n";
    out << "look_at = [" << config.cameraLookAt[0] << ", "
                         << config.cameraLookAt[1] << ", "
                         << config.cameraLookAt[2] << "]\n";
    out << "up = [" << config.cameraUp[0] << ", "
                    << config.cameraUp[1] << ", "
                    << config.cameraUp[2] << "]\n";
    out << "fov_y = " << config.cameraFovY << "\n";
    // Written only when orthographic: perspective is the default, and a key
    // restating a default on every save is noise in a hand-edited file.
    if (config.cameraOrthographic) {
        out << "projection = \"orthographic\"\n";
        if (config.cameraOrthoHeight > 0.0f) {
            out << "ortho_height = " << config.cameraOrthoHeight << "\n";
        }
    }
    out << "\n";

    // [lighting]
    out << "[lighting]\n";
    out << "sun_direction = [" << config.lighting.sunDirection.x << ", "
                               << config.lighting.sunDirection.y << ", "
                               << config.lighting.sunDirection.z << "]\n";
    out << "sun_radiance = [" << config.lighting.sunRadiance_rgb.r << ", "
                              << config.lighting.sunRadiance_rgb.g << ", "
                              << config.lighting.sunRadiance_rgb.b << "]\n";
    out << "sky_radiance = [" << config.lighting.skyRadiance_rgb.r << ", "
                              << config.lighting.skyRadiance_rgb.g << ", "
                              << config.lighting.skyRadiance_rgb.b << "]\n";
    out << "atmosphere_temperature_k = " << config.lighting.atmosphereTemperature_K << "\n";
    // Read back by extractSceneConfig, so leaving it out was one more value
    // lighting.transmittance is gone: the key was deprecated when the NN
    // atmosphere took the view path, and the slot it occupied in
    // LightingParams now carries the clear sky's zenith emissivity, which is
    // written under [atmosphere] as the air temperature and humidity it is
    // derived from.
    // The illuminant. No panel owns it yet, but without it the quantitative
    // spectral modes render black, so dropping it on save turned a working
    // config into a black frame.
    if (!config.solarLutPath.isEmpty()) {
        out << "solar_lut = " << tomlQuoted(config.solarLutPath) << "\n";
        if (config.solarLutColumns.size() >= 2) {
            out << "solar_lut_columns = [" << config.solarLutColumns[0] << ", "
                << config.solarLutColumns[1] << "]\n";
        }
        if (!config.solarLutNormalise.isEmpty()) {
            out << "solar_lut_normalise = " << tomlQuoted(config.solarLutNormalise) << "\n";
        }
        if (config.solarLutDiffuseIsGlobal) {
            out << "solar_lut_diffuse_is_global = true\n";
        }
    }
    out << "\n";

    // [atmosphere] - preset plus the nine weather features. extractSceneConfig
    // reads all of them (and maps the legacy [atmospheric] names onto the
    // preset), so anything omitted here reverts on the next load.
    out << "[atmosphere]\n";
    out << "preset = " << tomlQuoted(config.atmosphericPreset) << "\n";
    if (!config.atmosphere.modelPackDir.empty()) {
        out << "model_pack = "
            << tomlQuoted(QString::fromStdString(config.atmosphere.modelPackDir)) << "\n";
    }
    out << "atmos_model = " << config.atmosphere.atmosModel << "\n";
    out << "ihaze = " << config.atmosphere.ihaze << "\n";
    out << "icld = " << config.atmosphere.icld << "\n";
    out << "vis_km = " << config.atmosphere.visKm << "\n";
    out << "rainrt_mm_h = " << config.atmosphere.rainrtMmH << "\n";
    out << "t_ground_K = " << config.atmosphere.tGroundK << "\n";
    out << "rh = " << config.atmosphere.rh << "\n";
    out << "p_hPa = " << config.atmosphere.pHPa << "\n";
    out << "h2o_scale = " << config.atmosphere.h2oScale << "\n";
    // The analytic sky, which the core reads from this same section. Written
    // whether or not it is selected: the air temperature and humidity outlive
    // the switch that reads them, and the emissivity the shader gets is
    // derived from them rather than stored.
    out << "sky_model = "
        << (config.clearSkyModel ? "\"clear_sky\"" : "\"isotropic\"") << "\n";
    out << "air_temperature_k = " << config.skyAirTemperatureK << "\n";
    out << "relative_humidity = " << config.skyRelativeHumidity << "\n";
    out << "\n";

    // [quality] - VIS_Fused chromaticity correction (only if non-default),
    // plus the two gates the core reads. The section is written when any of
    // the four has something to say.
    const bool chromaCustom =
        config.lighting.chromaR_correction != quantiloom::LightingDefaults::CHROMA_R_CORRECTION ||
        config.lighting.chromaB_correction != quantiloom::LightingDefaults::CHROMA_B_CORRECTION;
    if (chromaCustom || config.failOnSrgbUpsample || config.logMaterialSources) {
        out << "[quality]\n";
        if (chromaCustom) {
            out << "chroma_r_correction = " << config.lighting.chromaR_correction << "\n";
            out << "chroma_b_correction = " << config.lighting.chromaB_correction << "\n";
        }
        if (config.failOnSrgbUpsample) {
            out << "fail_on_srgb_upsample = "
                << (*config.failOnSrgbUpsample ? "true" : "false") << "\n";
        }
        if (config.logMaterialSources) {
            out << "log_material_sources = "
                << (*config.logMaterialSources ? "true" : "false") << "\n";
        }
        out << "\n";
    }

    // [material] - required by the core, and absent from everything this
    // exporter wrote before now: a configuration saved here rendered in Studio
    // and was refused by the CLI, which reads the same file strictly.
    out << "[material]\n";
    out << "albedo = [" << config.defaultAlbedo.r << ", " << config.defaultAlbedo.g
        << ", " << config.defaultAlbedo.b << "]\n";
    out << "\n";

    // [[materials]] - material overrides, IR and PBR
    for (const auto& matConfig : config.materialConfigs) {
        // A retuned scale or offset counts as something to say even with no
        // path here, because the map it decodes may come from the scene file.
        const bool hasTemperatureMap =
            !matConfig.temperatureTexture.isEmpty() ||
            matConfig.temperatureScale != 500.0f || matConfig.temperatureOffset != 200.0f;

        if (matConfig.hasPbr || matConfig.hasSpectral() || matConfig.irEmissivity > 0.0f ||
            matConfig.irTransmittance > 0.0f || matConfig.irTemperature_K > 0.0f ||
            hasTemperatureMap) {
            out << "[[materials]]\n";
            out << "name = " << tomlQuoted(matConfig.name) << "\n";
            if (matConfig.hasPbr) {
                out << "base_color = [" << matConfig.baseColor.r << ", "
                    << matConfig.baseColor.g << ", " << matConfig.baseColor.b << "]\n";
                out << "metallic = " << matConfig.metallic << "\n";
                out << "roughness = " << matConfig.roughness << "\n";
                out << "emissive = [" << matConfig.emissive.r << ", "
                    << matConfig.emissive.g << ", " << matConfig.emissive.b << "]\n";
            }
            if (matConfig.irEmissivity > 0.0f) {
                out << "ir_emissivity = " << matConfig.irEmissivity << "\n";
            }
            if (matConfig.irTransmittance > 0.0f) {
                out << "ir_transmittance = " << matConfig.irTransmittance << "\n";
            }
            if (matConfig.irTemperature_K > 0.0f) {
                out << "ir_temperature_k = " << matConfig.irTemperature_K << "\n";
            }
            if (hasTemperatureMap) {
                if (!matConfig.temperatureTexture.isEmpty()) {
                    out << "temperature_texture = "
                        << tomlQuoted(matConfig.temperatureTexture) << "\n";
                }
                // Both, or neither: reading half a mapping out of a file and
                // taking the other half from a default is the kind of thing
                // that only shows up as a render being wrong by an offset.
                out << "temperature_scale = " << matConfig.temperatureScale << "\n";
                out << "temperature_offset = " << matConfig.temperatureOffset << "\n";
            }
            if (matConfig.hasSpectral()) {
                if (!matConfig.spectralMaterialType.isEmpty()) {
                    out << "spectral_material_type = "
                        << tomlQuoted(matConfig.spectralMaterialType) << "\n";
                }
                // Singular for one, plural for a mixture. The core treats them
                // as the same key with the same first entry, and writing a
                // one-element array would rewrite nearly every existing config
                // for no change in meaning.
                if (matConfig.spectralMaterialRefs.size() == 1) {
                    out << "spectral_material_ref = "
                        << tomlQuoted(matConfig.spectralMaterialRefs.first()) << "\n";
                } else {
                    out << "spectral_material_refs = [";
                    for (int i = 0; i < matConfig.spectralMaterialRefs.size(); ++i) {
                        if (i > 0) out << ", ";
                        out << tomlQuoted(matConfig.spectralMaterialRefs.at(i));
                    }
                    out << "]\n";
                }
                if (!matConfig.spectralUnmix.isEmpty()) {
                    out << "spectral_unmix = " << tomlQuoted(matConfig.spectralUnmix) << "\n";
                }
                if (!matConfig.spectralWeightTexture.isEmpty()) {
                    out << "spectral_weight_texture = "
                        << tomlQuoted(matConfig.spectralWeightTexture) << "\n";
                }
            }
            out << "\n";
        }
    }

    // [[duplicates]] - pasted nodes. Before [[nodes]], because the core
    // resolves them in that order (an override may address a copy by name),
    // and in creation order, so a copy of a copy follows its source.
    for (const auto& dup : config.duplicateConfigs) {
        out << "[[duplicates]]\n";
        out << "source = " << tomlQuoted(dup.sourceName) << "\n";
        out << "name = " << tomlQuoted(dup.name) << "\n";
        out << "matrix = [\n";
        for (int c = 0; c < 4; ++c) {
            out << "    ";
            for (int r = 0; r < 4; ++r) {
                out << dup.transform[c][r];
                if (c != 3 || r != 3) {
                    out << ", ";
                }
            }
            out << "\n";
        }
        out << "]\n\n";
    }

    // [[nodes]] - transforms for nodes moved since the document was opened.
    // A matrix rather than translation/rotation/scale: the viewport holds a
    // matrix, and decomposing one to write it down does not always round-trip.
    for (const auto& nodeConfig : config.nodeConfigs) {
        out << "[[nodes]]\n";
        out << "name = " << tomlQuoted(nodeConfig.name) << "\n";
        out << "matrix = [\n";
        for (int c = 0; c < 4; ++c) {
            out << "    ";
            for (int r = 0; r < 4; ++r) {
                out << nodeConfig.transform[c][r];
                if (c != 3 || r != 3) {
                    out << ", ";
                }
            }
            out << "\n";
        }
        out << "]\n\n";
    }

    out << "[sensor]\n";
    out << "enabled = " << (config.sensorEnabled ? "true" : "false") << "\n";
    out << "focal_length_mm = " << config.sensorParams.focalLength_mm << "\n";
    out << "f_number = " << config.sensorParams.fNumber << "\n";
    out << "pixel_pitch_um = " << config.sensorParams.pixelPitch_um << "\n";
    out << "psf_sigma_px = " << config.sensorParams.psfSigma_px << "\n";
    out << "quantum_efficiency = " << config.sensorParams.quantumEfficiency << "\n";
    out << "well_capacity_e = " << config.sensorParams.wellCapacity_e << "\n";
    out << "read_noise_e_rms = " << config.sensorParams.readNoise_e_rms << "\n";
    out << "dark_current_e_s = " << config.sensorParams.darkCurrent_e_s << "\n";
    out << "integration_time_s = " << config.sensorParams.integrationTime_s << "\n";
    out << "bit_depth = " << config.sensorParams.bitDepth << "\n";
    out << "gain = " << config.sensorParams.gain << "\n";
    out << "enable_poisson_noise = " << (config.sensorParams.enablePoissonNoise ? "true" : "false") << "\n";
    out << "enable_read_noise = " << (config.sensorParams.enableReadNoise ? "true" : "false") << "\n";
    out << "enable_dark_current = " << (config.sensorParams.enableDarkCurrent ? "true" : "false") << "\n";
    out << "enable_fpn = " << (config.sensorParams.enableFPN ? "true" : "false") << "\n";
    out << "noise_seed = " << config.sensorParams.noiseSeed << "\n";
    out << "detector_temperature_k = " << config.sensorParams.detectorTemperature_K << "\n";
    out << "\n";

    // [sensor.fpn] - written unconditionally. ParseSensorParams reads these
    // whether or not FPN is enabled, so exporting them only when enabled meant
    // tuning the FPN values, turning FPN off, and saving discarded the tuning.
    out << "[sensor.fpn]\n";
    out << "prnu_sigma = " << config.sensorParams.prnuSigma << "\n";
    out << "dsnu_sigma_e = " << config.sensorParams.dsnuSigma_e << "\n";
    out << "enable_nuc = " << (config.sensorParams.enableNUC ? "true" : "false") << "\n";
    out << "nuc_efficiency = " << config.sensorParams.nucEfficiency << "\n";
    out << "\n";

    // [thermography] - what the camera is told, which decides what the
    // temperature map means. Unconditional for the same reason [sensor.fpn]
    // is: the parameters outlive the switch that reads them.
    out << "[thermography]\n";
    out << "enabled = " << (config.thermographyEnabled ? "true" : "false") << "\n";
    out << "emissivity = " << config.thermography.emissivity << "\n";
    out << "reflected_temperature_k = " << config.thermography.reflectedTemperature_K << "\n";
    out << "atmosphere_transmittance = " << config.thermography.atmosphereTransmittance << "\n";
    out << "atmosphere_temperature_k = " << config.thermography.atmosphereTemperature_K << "\n";
    out << "\n";

    // Last, because these are the sections a hand-authored quantitative config
    // brings and no panel edits -- keeping them together at the end makes what
    // the GUI carried through visible in a diff.

    // [spectral_curves] - material name -> measured reflectance CSV
    if (!config.spectralCurves.isEmpty()) {
        out << "[spectral_curves]\n";
        for (auto it = config.spectralCurves.constBegin();
             it != config.spectralCurves.constEnd(); ++it) {
            out << tomlQuoted(it.key()) << " = " << tomlQuoted(it.value()) << "\n";
        }
        out << "\n";
    }

    // [refractive_index] - material name -> RefractiveIndex.INFO YAML
    if (!config.refractiveIndexFiles.isEmpty()) {
        out << "[refractive_index]\n";
        for (auto it = config.refractiveIndexFiles.constBegin();
             it != config.refractiveIndexFiles.constEnd(); ++it) {
            out << tomlQuoted(it.key()) << " = " << tomlQuoted(it.value()) << "\n";
        }
        out << "\n";
    }

    // [hyperspectral] - the offline cube. The viewport honours none of it.
    if (config.hyperspectral) {
        const HyperspectralConfig& hs = *config.hyperspectral;
        out << "[hyperspectral]\n";
        out << "wavelength_min_nm = " << hs.wavelengthMin_nm << "\n";
        out << "wavelength_max_nm = " << hs.wavelengthMax_nm << "\n";
        out << "wavelength_step_nm = " << hs.wavelengthStep_nm << "\n";
        out << "use_gpu_reconstruction = " << (hs.useGpuReconstruction ? "true" : "false") << "\n";
        out << "save_intermediates = " << (hs.saveIntermediates ? "true" : "false") << "\n";
        out << "output_format = " << tomlQuoted(hs.outputFormat) << "\n";
        out << "\n";
    }
}
