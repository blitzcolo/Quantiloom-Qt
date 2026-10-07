/**
 * @file PixelReadbackConversion.hpp
 * @brief Read a viewport pixel's raw accumulation back as a temperature
 */

#pragma once

#include <core/SpectralData.hpp>
#include <postprocess/Thermography.hpp>

#include <glm/glm.hpp>

#include <optional>

namespace vkview {

/// Convert the raw accumulation channel carried by an IR fused mode through
/// the same SDK thermography inversion as explicit pixel measurements.
/// The accumulation is per-nm average spectral radiance, which is the unit
/// the SDK's band routines take. R is the whole of it in a thermal band.
inline std::optional<double> apparentTemperatureK(
    quantiloom::SpectralMode mode, const glm::vec4& pixel,
    const quantiloom::ThermographyParams& thermography) {
    const auto band = quantiloom::GetFusedBandInfo(mode);
    if (!band.has_value() || !quantiloom::IsIRFusedMode(mode)) {
        return std::nullopt;
    }
    return quantiloom::InvertSurfaceTemperatureK(
        static_cast<double>(pixel.r), static_cast<double>(band->lambdaMinNm),
        static_cast<double>(band->lambdaMaxNm), thermography);
}

} // namespace vkview
