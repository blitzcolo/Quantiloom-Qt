/**
 * @file ModeCatalog.cpp
 */

#include "ModeCatalog.hpp"

#include <QCoreApplication>

namespace {
/// Carries the translation context for this file. The names have to be plain
/// Tr::tr("...") calls for lupdate to see them at all: routing them through a
/// helper that called QCoreApplication::translate() with a runtime pointer
/// compiled cleanly and produced no .ts entries, so every mode name was
/// silently untranslatable. Q_DECLARE_TR_FUNCTIONS expands to access
/// specifiers, so it needs a class rather than the namespace itself.
class Tr {
    Q_DECLARE_TR_FUNCTIONS(catalog)
};
} // namespace

using quantiloom::DebugVisualizationMode;
using quantiloom::SpectralMode;

namespace catalog {

// ============================================================================
// Debug visualization
// ============================================================================

QVector<DebugCategory> debugCategories() {
    return {
        DebugCategory::Geometry,
        DebugCategory::Material,
        DebugCategory::Lighting,
        DebugCategory::BRDF,
        DebugCategory::IBL,
        DebugCategory::Spectral,
        DebugCategory::Infrared,
        DebugCategory::Diagnostics,
    };
}

QString debugCategoryName(DebugCategory category) {
    switch (category) {
        case DebugCategory::Geometry:    return Tr::tr("Geometry");
        case DebugCategory::Material:    return Tr::tr("Material");
        case DebugCategory::Lighting:    return Tr::tr("Lighting");
        case DebugCategory::BRDF:        return Tr::tr("BRDF");
        case DebugCategory::IBL:         return Tr::tr("IBL");
        case DebugCategory::Spectral:    return Tr::tr("Spectral");
        case DebugCategory::Infrared:    return Tr::tr("Infrared");
        case DebugCategory::Diagnostics: return Tr::tr("Geometry Diagnostics");
    }
    return {};
}

QVector<DebugVisualizationMode> debugModesIn(DebugCategory category) {
    switch (category) {
        case DebugCategory::Geometry:
            return {
                DebugVisualizationMode::WorldPosition,
                DebugVisualizationMode::GeometricNormal,
                DebugVisualizationMode::ShadedNormal,
                DebugVisualizationMode::Tangent,
                DebugVisualizationMode::UV,
                DebugVisualizationMode::MaterialID,
                DebugVisualizationMode::TriangleID,
                DebugVisualizationMode::Barycentric,
            };
        case DebugCategory::Material:
            return {
                DebugVisualizationMode::BaseColor,
                DebugVisualizationMode::Metallic,
                DebugVisualizationMode::Roughness,
                DebugVisualizationMode::NormalMapDelta,
                DebugVisualizationMode::Emissive,
                DebugVisualizationMode::Alpha,
            };
        case DebugCategory::Lighting:
            return {
                DebugVisualizationMode::NdotL,
                DebugVisualizationMode::NdotV,
                DebugVisualizationMode::DirectSun,
                DebugVisualizationMode::Diffuse,
                DebugVisualizationMode::AtmosphericTransmittance,
            };
        case DebugCategory::BRDF:
            return {
                DebugVisualizationMode::FresnelF0,
                DebugVisualizationMode::Fresnel,
                DebugVisualizationMode::BRDF_Full,
                DebugVisualizationMode::SpecularD,
                DebugVisualizationMode::SpecularG,
            };
        case DebugCategory::IBL:
            return {
                DebugVisualizationMode::ReflectionDir,
                DebugVisualizationMode::PrefilteredEnv,
                DebugVisualizationMode::BrdfLut,
                DebugVisualizationMode::IblSpecular,
                DebugVisualizationMode::SkyAmbient,
            };
        case DebugCategory::Spectral:
            return {
                DebugVisualizationMode::XYZ_Tristimulus,
                DebugVisualizationMode::BeforeChromaCorrection,
                DebugVisualizationMode::SpectralReflectance550,
            };
        case DebugCategory::Infrared:
            return {
                DebugVisualizationMode::Temperature,
                DebugVisualizationMode::IREmissivity,
                DebugVisualizationMode::IREmission,
                DebugVisualizationMode::IRReflection,
            };
        case DebugCategory::Diagnostics:
            return {
                DebugVisualizationMode::VertexPositions,
                DebugVisualizationMode::IndexValues,
                DebugVisualizationMode::InstanceID,
                DebugVisualizationMode::PrimitiveID,
                DebugVisualizationMode::IndexBufferPos,
                DebugVisualizationMode::V0Position,
                DebugVisualizationMode::RawIdx0,
                DebugVisualizationMode::V0Raw,
            };
    }
    return {};
}

QString debugModeName(DebugVisualizationMode mode) {
    switch (mode) {
        case DebugVisualizationMode::None:
            return Tr::tr("None (Normal Rendering)");

        // Geometry
        case DebugVisualizationMode::WorldPosition:   return Tr::tr("World Position");
        case DebugVisualizationMode::GeometricNormal: return Tr::tr("Geometric Normal");
        case DebugVisualizationMode::ShadedNormal:    return Tr::tr("Shaded Normal");
        case DebugVisualizationMode::Tangent:         return Tr::tr("Tangent");
        case DebugVisualizationMode::UV:              return Tr::tr("UV Coordinates");
        case DebugVisualizationMode::MaterialID:      return Tr::tr("Material ID");
        case DebugVisualizationMode::TriangleID:      return Tr::tr("Triangle ID");
        case DebugVisualizationMode::Barycentric:     return Tr::tr("Barycentric Coords");

        // Material
        case DebugVisualizationMode::BaseColor:      return Tr::tr("Base Color (Albedo)");
        case DebugVisualizationMode::Metallic:       return Tr::tr("Metallic");
        case DebugVisualizationMode::Roughness:      return Tr::tr("Roughness");
        case DebugVisualizationMode::NormalMapDelta: return Tr::tr("Normal Map Delta");
        case DebugVisualizationMode::Emissive:       return Tr::tr("Emissive");
        case DebugVisualizationMode::Alpha:          return Tr::tr("Alpha");

        // Lighting
        case DebugVisualizationMode::NdotL:     return Tr::tr("N dot L");
        case DebugVisualizationMode::NdotV:     return Tr::tr("N dot V");
        case DebugVisualizationMode::DirectSun: return Tr::tr("Direct Sun");
        case DebugVisualizationMode::Diffuse:   return Tr::tr("Diffuse");
        case DebugVisualizationMode::AtmosphericTransmittance:
            return Tr::tr("Atmospheric Transmittance");

        // BRDF
        case DebugVisualizationMode::FresnelF0: return Tr::tr("Fresnel F0");
        case DebugVisualizationMode::Fresnel:   return Tr::tr("Fresnel");
        case DebugVisualizationMode::BRDF_Full: return Tr::tr("Full BRDF");
        case DebugVisualizationMode::SpecularD: return Tr::tr("Specular D (GGX)");
        case DebugVisualizationMode::SpecularG: return Tr::tr("Specular G (Smith)");

        // IBL
        case DebugVisualizationMode::ReflectionDir:  return Tr::tr("Reflection Direction");
        case DebugVisualizationMode::PrefilteredEnv: return Tr::tr("Prefiltered Environment");
        case DebugVisualizationMode::BrdfLut:        return Tr::tr("BRDF LUT");
        case DebugVisualizationMode::IblSpecular:    return Tr::tr("IBL Specular");
        case DebugVisualizationMode::SkyAmbient:     return Tr::tr("Sky Ambient");

        // Spectral
        case DebugVisualizationMode::XYZ_Tristimulus: return Tr::tr("XYZ Tristimulus");
        case DebugVisualizationMode::BeforeChromaCorrection:
            return Tr::tr("Before Chroma Correction");
        case DebugVisualizationMode::SpectralReflectance550:
            return Tr::tr("Spectral Reflectance @550nm");

        // Infrared. "Surface Temperature" rather than a bare "Temperature":
        // five panels used to label five different physical quantities with
        // the same word.
        case DebugVisualizationMode::Temperature:  return Tr::tr("Surface Temperature");
        case DebugVisualizationMode::IREmissivity: return Tr::tr("IR Emissivity");
        case DebugVisualizationMode::IREmission:   return Tr::tr("IR Emission");
        case DebugVisualizationMode::IRReflection: return Tr::tr("IR Reflection");

        // Diagnostics
        case DebugVisualizationMode::VertexPositions: return Tr::tr("Vertex Positions (Hash)");
        case DebugVisualizationMode::IndexValues:     return Tr::tr("Index Values");
        case DebugVisualizationMode::InstanceID:      return Tr::tr("Instance ID");
        case DebugVisualizationMode::PrimitiveID:     return Tr::tr("Primitive ID");
        case DebugVisualizationMode::IndexBufferPos:  return Tr::tr("Index Buffer Position");
        case DebugVisualizationMode::V0Position:      return Tr::tr("V0 Position");
        case DebugVisualizationMode::RawIdx0:         return Tr::tr("Raw idx0");
        case DebugVisualizationMode::V0Raw:           return Tr::tr("V0 Raw (clamped)");

        default:
            return Tr::tr("Unknown");
    }
}

QString debugModeDescription(DebugVisualizationMode mode) {
    switch (mode) {
        case DebugVisualizationMode::None:
            return Tr::tr("Standard rendering output. No debug visualization.");

        // Geometry
        case DebugVisualizationMode::WorldPosition:
            return Tr::tr("World-space hit position. RGB = fractional XYZ coordinates.");
        case DebugVisualizationMode::GeometricNormal:
            return Tr::tr("Raw geometric normal from triangle vertices (before normal mapping).");
        case DebugVisualizationMode::ShadedNormal:
            return Tr::tr("Final shading normal after interpolation and normal map application.");
        case DebugVisualizationMode::Tangent:
            return Tr::tr("Tangent vector for normal mapping. Used for TBN matrix construction.");
        case DebugVisualizationMode::UV:
            return Tr::tr("Texture coordinates. RG = fractional UV, useful for texture mapping debug.");
        case DebugVisualizationMode::MaterialID:
            return Tr::tr("Material index visualized as distinct colors. Each material gets unique color.");
        case DebugVisualizationMode::TriangleID:
            return Tr::tr("Primitive (triangle) index. Useful for mesh topology inspection.");
        case DebugVisualizationMode::Barycentric:
            return Tr::tr("Barycentric coordinates within triangle. RGB = weights at 3 vertices.");

        // Material
        case DebugVisualizationMode::BaseColor:
            return Tr::tr("Albedo/base color from texture or material parameters.");
        case DebugVisualizationMode::Metallic:
            return Tr::tr("Metallic parameter. 0 = dielectric, 1 = metal.");
        case DebugVisualizationMode::Roughness:
            return Tr::tr("Roughness parameter. 0 = mirror smooth, 1 = fully rough.");
        case DebugVisualizationMode::NormalMapDelta:
            return Tr::tr("Normal map perturbation from surface normal.");
        case DebugVisualizationMode::Emissive:
            return Tr::tr("Emissive color/intensity. Self-illumination without external lighting.");
        case DebugVisualizationMode::Alpha:
            return Tr::tr("Alpha/opacity value. 1 = opaque, 0 = transparent.");

        // Lighting
        case DebugVisualizationMode::NdotL:
            return Tr::tr("Dot product of normal and light direction. Basic diffuse term.");
        case DebugVisualizationMode::NdotV:
            return Tr::tr("Dot product of normal and view direction. Affects Fresnel and specular.");
        case DebugVisualizationMode::DirectSun:
            return Tr::tr("Direct sunlight contribution after shadowing and attenuation.");
        case DebugVisualizationMode::Diffuse:
            return Tr::tr("Diffuse lighting term: kD * albedo * NdotL.");
        case DebugVisualizationMode::AtmosphericTransmittance:
            return Tr::tr("Atmospheric transmittance factor from scattering/absorption LUT.");

        // BRDF
        case DebugVisualizationMode::FresnelF0:
            return Tr::tr("Base reflectivity at normal incidence. Depends on metallic and IOR.");
        case DebugVisualizationMode::Fresnel:
            return Tr::tr("Fresnel reflectance at current viewing angle (Schlick approximation).");
        case DebugVisualizationMode::BRDF_Full:
            return Tr::tr("Complete Cook-Torrance BRDF evaluation: D * G * F / (4 * NdotL * NdotV).");
        case DebugVisualizationMode::SpecularD:
            return Tr::tr("GGX/Trowbridge-Reitz normal distribution function.");
        case DebugVisualizationMode::SpecularG:
            return Tr::tr("Smith geometry/masking-shadowing function.");

        // IBL
        case DebugVisualizationMode::ReflectionDir:
            return Tr::tr("Mirror reflection direction for environment map sampling.");
        case DebugVisualizationMode::PrefilteredEnv:
            return Tr::tr("Pre-filtered environment map sample at current roughness level.");
        case DebugVisualizationMode::BrdfLut:
            return Tr::tr("BRDF integration LUT sample. RG = scale and bias for split-sum.");
        case DebugVisualizationMode::IblSpecular:
            return Tr::tr("Final IBL specular contribution: prefiltered * (F * scale + bias).");
        case DebugVisualizationMode::SkyAmbient:
            return Tr::tr("Ambient sky lighting contribution (diffuse IBL).");

        // Spectral
        case DebugVisualizationMode::XYZ_Tristimulus:
            return Tr::tr("CIE XYZ tristimulus values from spectral integration. Before RGB conversion.");
        case DebugVisualizationMode::BeforeChromaCorrection:
            return Tr::tr("Linear RGB before chromaticity correction. May show color shifts.");
        case DebugVisualizationMode::SpectralReflectance550:
            return Tr::tr("Material spectral reflectance sampled at 550 nm (green reference).");

        // Infrared
        case DebugVisualizationMode::Temperature:
            return Tr::tr("Surface temperature in Kelvin. Blue = cold, red = hot (colormap).");
        case DebugVisualizationMode::IREmissivity:
            return Tr::tr("IR emissivity factor. 1 = perfect blackbody, 0 = perfect reflector.");
        case DebugVisualizationMode::IREmission:
            return Tr::tr("Thermal emission contribution: emissivity * Planck(T, lambda).");
        case DebugVisualizationMode::IRReflection:
            return Tr::tr("IR reflection of ambient thermal radiation.");

        // Diagnostics
        case DebugVisualizationMode::VertexPositions:
            return Tr::tr("Hash of 3 vertex positions. The same face should show similar colors. "
                       "Different colors on one face mean index corruption.");
        case DebugVisualizationMode::IndexValues:
            return Tr::tr("Triangle vertex indices as RGB (normalized by 32). For a cube: idx 0-23.");
        case DebugVisualizationMode::InstanceID:
            return Tr::tr("TLAS instance index. Verifies instance-to-geometry mapping.");
        case DebugVisualizationMode::PrimitiveID:
            return Tr::tr("PrimitiveIndex() value. R = id/12 (gradient), G = alternating, B = even/odd. "
                       "For a cube: 12 distinct triangles with a smooth R gradient.");
        case DebugVisualizationMode::IndexBufferPos:
            return Tr::tr("Index buffer read position. R = basePos/36, G = offset/36, B = primID/12. "
                       "For a single BLAS, G should be 0.");
        case DebugVisualizationMode::V0Position:
            return Tr::tr("First vertex (v0) position mapped to 0-1 using frac(). "
                       "For a ±1 cube: 0 for both +1 and -1, 0.5 for 0.");
        case DebugVisualizationMode::RawIdx0:
            return Tr::tr("Raw idx0 value. R = idx0/32, G = readAddr/32, B = offset/32. "
                       "For a cube: R should be 0-0.72 (idx 0-23). G = R when offset is 0.");
        case DebugVisualizationMode::V0Raw:
            return Tr::tr("v0 position clamped (not frac). -1 maps to 0, 0 to 0.5, +1 to 1. "
                       "For a cube: only 0 or 1, never 0.5.");

        default:
            return Tr::tr("Unknown debug mode.");
    }
}

QString debugOutputInterpretation() {
    return Tr::tr(
        "<h3>Reading a debug image</h3>"
        "<p><b>Color encoding</b></p>"
        "<ul>"
        "<li><b>Vectors</b> — (V+1)/2 maps the [-1,1] range onto [0,1] RGB.</li>"
        "<li><b>Scalars</b> — grayscale intensity.</li>"
        "<li><b>Identifiers</b> — hashed to distinct colors.</li>"
        "<li><b>Temperature</b> — blue (cold) through red (hot).</li>"
        "</ul>"
        "<p><b>Where to start</b></p>"
        "<ul>"
        "<li><b>Shaded Normal</b> — check that normal mapping is applied.</li>"
        "<li><b>Material ID</b> — verify which material each surface resolved to.</li>"
        "<li><b>XYZ Tristimulus</b> — debug spectral integration before RGB conversion.</li>"
        "<li><b>Geometry Diagnostics</b> — only useful when a mesh renders as noise; "
        "they expose index and vertex buffer addressing directly.</li>"
        "</ul>"
        "<p>Hover the viewport with a debug mode active to read the raw value under the "
        "cursor in the status bar.</p>");
}

// ============================================================================
// Spectral mode
// ============================================================================

QVector<SpectralMode> spectralModes() {
    return {
        SpectralMode::RGB,
        SpectralMode::VIS_Fused,
        SpectralMode::Single,
        SpectralMode::NIR_Fused,
        SpectralMode::SWIR_Fused,
        SpectralMode::MWIR_Fused,
        SpectralMode::LWIR_Fused,
    };
}

QString fusedRangeText(SpectralMode mode) {
    // Deliberately not built from quantiloom::constants::WAVELENGTH_*: those
    // are the ISO 20473 taxonomy for what a band *name* means, and for two
    // modes they are not what the renderer integrates -- NIR is 780-1400 by
    // that taxonomy but 930-1200 as rendered (narrowed to the NN atmosphere's
    // coverage), and SWIR is 1000-2500 versus 1400-2400 (avoiding the NIR
    // overlap). GetFusedBandInfo() is the renderer's own source of truth.
    const auto info = quantiloom::GetFusedBandInfo(mode);
    if (!info) {
        return {};
    }
    // Micrometres past 3 um, nanometres below, matching how each band is
    // conventionally written. QString::number rather than arg's numeric
    // overload so the digits never pick up locale grouping ("1 200 nm").
    if (info->lambdaMinNm >= 3000.0f) {
        return Tr::tr("%1-%2 μm")
            .arg(QString::number(info->lambdaMinNm / 1000.0, 'g', 3),
                 QString::number(info->lambdaMaxNm / 1000.0, 'g', 3));
    }
    return Tr::tr("%1-%2 nm")
        .arg(QString::number(info->lambdaMinNm, 'g', 4),
             QString::number(info->lambdaMaxNm, 'g', 4));
}

QString spectralModeName(SpectralMode mode) {
    switch (mode) {
        // Band acronyms stay in Latin script in every locale -- they are the
        // conventional written form, not English words. See the glossary.
        case SpectralMode::RGB:        return QStringLiteral("RGB");
        case SpectralMode::VIS_Fused:  return QStringLiteral("VIS");
        case SpectralMode::Single:     return Tr::tr("Single Wavelength");
        case SpectralMode::NIR_Fused:  return QStringLiteral("NIR");
        case SpectralMode::SWIR_Fused: return QStringLiteral("SWIR");
        case SpectralMode::MWIR_Fused: return QStringLiteral("MWIR");
        case SpectralMode::LWIR_Fused: return QStringLiteral("LWIR");
        default:                       return Tr::tr("Unknown");
    }
}

QString spectralModeLabel(SpectralMode mode) {
    switch (mode) {
        case SpectralMode::RGB:
            return Tr::tr("RGB (Default)");
        case SpectralMode::VIS_Fused:
            return Tr::tr("VIS Fused (32-band Spectral)");
        case SpectralMode::Single:
            return Tr::tr("Single Wavelength");
        case SpectralMode::NIR_Fused:
        case SpectralMode::SWIR_Fused:
        case SpectralMode::MWIR_Fused:
        case SpectralMode::LWIR_Fused:
            return Tr::tr("%1 (%2)").arg(spectralModeName(mode), fusedRangeText(mode));
        default:
            return Tr::tr("Unknown");
    }
}

QString spectralModeDescription(SpectralMode mode) {
    switch (mode) {
        case SpectralMode::RGB:
            return Tr::tr("Fast RGB rendering, no spectral integration. "
                       "Best for real-time preview.");
        case SpectralMode::VIS_Fused:
            return Tr::tr("32-wavelength spectral integration with CIE XYZ color matching. "
                       "Physically accurate but slower.");
        case SpectralMode::Single:
            return Tr::tr("Monochromatic rendering at a single wavelength. "
                       "Useful for spectral analysis and wavelength-specific effects.");
        case SpectralMode::NIR_Fused:
            return Tr::tr("Near-Infrared (%1). Reflected solar radiation, "
                       "vegetation analysis, and night vision.").arg(fusedRangeText(mode));
        case SpectralMode::SWIR_Fused:
            return Tr::tr("Short-Wave Infrared (%1). Moisture detection, "
                       "material identification, and imaging through haze.").arg(fusedRangeText(mode));
        case SpectralMode::MWIR_Fused:
            return Tr::tr("Mid-Wave Infrared (%1). Thermal imaging for hot objects, "
                       "engine exhaust, and fire detection.").arg(fusedRangeText(mode));
        case SpectralMode::LWIR_Fused:
            return Tr::tr("Long-Wave Infrared (%1). Thermal imaging for room-temperature "
                       "objects, people, and buildings.").arg(fusedRangeText(mode));
        default:
            return Tr::tr("Unknown spectral mode.");
    }
}

bool spectralModeIsPreviewOnly(SpectralMode mode) {
    switch (mode) {
        case SpectralMode::NIR_Fused:
        case SpectralMode::SWIR_Fused:
        case SpectralMode::MWIR_Fused:
        case SpectralMode::LWIR_Fused:
            return true;
        default:
            return false;
    }
}

// ============================================================================
// Render quality presets
// ============================================================================

QVector<QualityPreset> qualityPresets() {
    return {
        {"preview",    1},
        {"draft",      4},
        {"medium",    16},
        {"high",      64},
        {"veryhigh", 256},
        {"production", 1024},
    };
}

QString qualityPresetLabel(const QualityPreset& preset) {
    const QString id = QString::fromLatin1(preset.id);
    QString name;
    if (id == "preview")         name = Tr::tr("Preview");
    else if (id == "draft")      name = Tr::tr("Draft");
    else if (id == "medium")     name = Tr::tr("Medium");
    else if (id == "high")       name = Tr::tr("High");
    else if (id == "veryhigh")   name = Tr::tr("Very High");
    else if (id == "production") name = Tr::tr("Production");
    else                         name = id;

    return Tr::tr("%1 (%2 spp)").arg(name).arg(preset.spp);
}

} // namespace catalog
