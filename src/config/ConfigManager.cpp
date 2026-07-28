/**
 * @file ConfigManager.cpp
 * @brief TOML configuration import/export implementation
 */

#include "ConfigManager.hpp"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>
#include <cctype>

#include <postprocess/PostprocessConfig.hpp>
#include <renderer/LightingParams.hpp>

// Reading is delegated to the SDK's PostprocessConfig::ParseSensorParams, but
// writing is hand-rolled TOML below, so the two drift apart whenever the SDK
// adds a field: noiseSeed was parsed on load and dropped on save, which made a
// config that had been through this GUI stop matching the one the CLI reads.
//
// This assert is the tripwire. If it fires, the SDK's SensorParams changed --
// diff it against ParseSensorParams and against the [sensor] block in
// saveConfig() below, then update the expected size. The SDK/GUI boundary is
// already a single MSVC toolchain (SRS CON-04), so the layout is well-defined.
static_assert(sizeof(quantiloom::SensorParams) == 84,
              "SensorParams changed size: a field was added or removed. Check "
              "that saveConfig() still writes every key ParseSensorParams reads "
              "before updating this number.");

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
    m_loadedConfig = std::make_unique<quantiloom::Config>(std::move(result.value()));

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

void ConfigManager::extractSceneConfig(const quantiloom::Config& config, SceneConfig& out) {
    // Initialize with defaults
    out.lighting = quantiloom::CreateDefaultLightingParams();

    // [renderer]
    auto resolution = config.GetArray<quantiloom::i32>("renderer.resolution");
    if (resolution.size() >= 2) {
        out.width = static_cast<uint32_t>(resolution[0]);
        out.height = static_cast<uint32_t>(resolution[1]);
    }
    out.spp = config.Get<quantiloom::u32>("renderer.spp", 4);
    out.outputPath = QString::fromStdString(config.GetString("renderer.output", "output.exr"));
    out.environmentMap = QString::fromStdString(config.GetString("renderer.environment_map", ""));
    out.samplingSeed = config.Get<quantiloom::u32>(
        "renderer.seed", quantiloom::constants::DEFAULT_SAMPLING_SEED);

    // [spectral]
    std::string modeStr = config.GetString("spectral.mode", "rgb");
    out.spectralMode = parseSpectralMode(modeStr);
    out.wavelength_nm = config.GetFloat("spectral.wavelength_nm", 550.0f);
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

    out.cameraFovY = config.GetFloat("camera.fov_y", 45.0f);

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
    out.lighting.transmittance = config.GetFloat("lighting.transmittance", 0.9f);
    out.lighting.worldUnitsToMeters = out.worldUnitsToMeters;

    // [quality] VIS_Fused chromaticity correction
    out.lighting.chromaR_correction = config.GetFloat(
        "quality.chroma_r_correction",
        quantiloom::LightingDefaults::CHROMA_R_CORRECTION);
    out.lighting.chromaB_correction = config.GetFloat(
        "quality.chroma_b_correction",
        quantiloom::LightingDefaults::CHROMA_B_CORRECTION);

    // [renderer] Shadow ray control
    bool enableShadowRays = config.Get<bool>("renderer.enable_shadow_rays", false);
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

    // [sensor]
    out.sensorEnabled = config.Get<bool>("sensor.enabled", false);
    // Always parse sensor params so they're available if user enables later
    out.sensorParams = quantiloom::PostprocessConfig::ParseSensorParams(config);

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

        out.materialConfigs.append(matConfig);

        qDebug() << "Loaded material config:" << matConfig.name
                 << "emissivity=" << matConfig.irEmissivity
                 << "transmittance=" << matConfig.irTransmittance
                 << "temperature=" << matConfig.irTemperature_K << "K";
    }
}

quantiloom::SpectralMode ConfigManager::parseSpectralMode(const std::string& modeStr) {
    std::string lower;
    lower.reserve(modeStr.size());
    for (char c : modeStr) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    if (lower == "single" || lower == "single_wavelength") {
        return quantiloom::SpectralMode::Single;
    }
    if (lower == "rgb") {
        return quantiloom::SpectralMode::RGB;
    }
    if (lower == "vis_fused") {
        return quantiloom::SpectralMode::VIS_Fused;
    }
    if (lower == "mwir_fused") {
        return quantiloom::SpectralMode::MWIR_Fused;
    }
    if (lower == "lwir_fused") {
        return quantiloom::SpectralMode::LWIR_Fused;
    }
    if (lower == "swir_fused") {
        return quantiloom::SpectralMode::SWIR_Fused;
    }
    if (lower == "nir_fused") {
        return quantiloom::SpectralMode::NIR_Fused;
    }
    // Default to RGB
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
        out << "output = \"" << config.outputPath << "\"\n";
    }
    if (!config.environmentMap.isEmpty()) {
        out << "environment_map = \"" << config.environmentMap << "\"\n";
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
        default: modeStr = "rgb"; break;
    }
    out << "mode = \"" << modeStr << "\"\n";
    // Written for every mode, not just Single: PostprocessConfig reads
    // spectral.wavelength_nm into SensorParams::wavelength_nm regardless of
    // mode, so omitting it here silently reset the sensor's peak wavelength
    // to 550 nm on the next load.
    out << "wavelength_nm = " << config.wavelength_nm << "\n";
    out << "lambda_min = " << config.lambda_min << "\n";
    out << "lambda_max = " << config.lambda_max << "\n";
    out << "delta_lambda = " << config.delta_lambda << "\n";
    out << "\n";

    // [scene]
    out << "[scene]\n";
    if (!config.gltfPath.isEmpty()) {
        out << "gltf = \"" << config.gltfPath << "\"\n";
    }
    if (!config.usdPath.isEmpty()) {
        out << "usd = \"" << config.usdPath << "\"\n";
    }
    out << "world_units_to_meters = " << config.worldUnitsToMeters << "\n";
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
    // that silently reverted to its default on every save. The core logs it as
    // deprecated -- view-path transmittance comes from the NN atmosphere now --
    // but a config the GUI wrote should still say what the GUI was holding.
    out << "transmittance = " << config.lighting.transmittance << "\n";
    out << "\n";

    // [atmosphere] - preset plus the nine weather features. extractSceneConfig
    // reads all of them (and maps the legacy [atmospheric] names onto the
    // preset), so anything omitted here reverts on the next load.
    out << "[atmosphere]\n";
    out << "preset = \"" << config.atmosphericPreset << "\"\n";
    if (!config.atmosphere.modelPackDir.empty()) {
        out << "model_pack = \"" << QString::fromStdString(config.atmosphere.modelPackDir) << "\"\n";
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
    out << "\n";

    // [quality] - VIS_Fused chromaticity correction (only if non-default)
    if (config.lighting.chromaR_correction != quantiloom::LightingDefaults::CHROMA_R_CORRECTION ||
        config.lighting.chromaB_correction != quantiloom::LightingDefaults::CHROMA_B_CORRECTION) {
        out << "[quality]\n";
        out << "chroma_r_correction = " << config.lighting.chromaR_correction << "\n";
        out << "chroma_b_correction = " << config.lighting.chromaB_correction << "\n";
        out << "\n";
    }

    // [[materials]] - export material IR configs
    for (const auto& matConfig : config.materialConfigs) {
        if (matConfig.irEmissivity > 0.0f || matConfig.irTransmittance > 0.0f ||
            matConfig.irTemperature_K > 0.0f) {
            out << "[[materials]]\n";
            out << "name = \"" << matConfig.name << "\"\n";
            if (matConfig.irEmissivity > 0.0f) {
                out << "ir_emissivity = " << matConfig.irEmissivity << "\n";
            }
            if (matConfig.irTransmittance > 0.0f) {
                out << "ir_transmittance = " << matConfig.irTransmittance << "\n";
            }
            if (matConfig.irTemperature_K > 0.0f) {
                out << "ir_temperature_k = " << matConfig.irTemperature_K << "\n";
            }
            out << "\n";
        }
    }

    // [sensor]
    out << "[sensor]\n";
    out << "enabled = " << (config.sensorEnabled ? "true" : "false") << "\n";
    out << "focal_length_mm = " << config.sensorParams.focalLength_mm << "\n";
    out << "f_number = " << config.sensorParams.fNumber << "\n";
    out << "pixel_pitch_um = " << config.sensorParams.pixelPitch_um << "\n";
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

    file.close();
    return true;
}
