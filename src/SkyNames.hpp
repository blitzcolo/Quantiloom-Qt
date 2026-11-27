/**
 * @file SkyNames.hpp
 * @brief The words the sky's directional model goes by, in one place
 *
 * `lighting.sky_distribution = "hosek_wilkie"` in a TOML and
 * `"distribution": "hosek_wilkie"` in an MCP call are the same string, and two
 * boundaries read it -- the config manager and the tool handlers -- so one
 * translation serves both, the way ThermalNames.hpp does for the convection
 * law. An unknown word falls back to isotropic, which is what the SDK does with
 * the same string.
 */

#pragma once

#include <renderer/ExternalRenderContext.hpp>

#include <QString>

namespace skynames {

inline quantiloom::SkyDistribution skyDistributionFromName(const QString& name) {
    if (name == QLatin1String("hosek_wilkie")) return quantiloom::SkyDistribution::HosekWilkie;
    return quantiloom::SkyDistribution::Isotropic;
}

inline QString skyDistributionName(const quantiloom::SkyDistribution distribution) {
    if (distribution == quantiloom::SkyDistribution::HosekWilkie) {
        return QStringLiteral("hosek_wilkie");
    }
    return QStringLiteral("isotropic");
}

}  // namespace skynames
