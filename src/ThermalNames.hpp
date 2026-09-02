/**
 * @file ThermalNames.hpp
 * @brief The words a thermal parameter goes by, in one place
 *
 * The convection law and the differentiable material parameters are named the
 * same way wherever they are written down: `convection_model = "stability"` in
 * a TOML, `"parameter_sensitivities": ["h", "k"]` in an MCP call. Two boundaries
 * read those words -- the config manager and the tool handlers -- and a second
 * reading of one key is the failure this repository has already paid for once.
 *
 * So: one translation, included by both. An unknown word falls back rather than
 * refusing, which is what the SDK does with the same string, because two
 * readings that disagree about a bad value are worse than either.
 *
 * @author blitzcolo
 */

#pragma once

#include <renderer/ThermalControl.hpp>

#include <QString>
#include <QStringList>

namespace thermalnames {

inline quantiloom::ThermalConvectionModel convectionModelFromName(const QString& name) {
    if (name == QLatin1String("wind")) return quantiloom::ThermalConvectionModel::Wind;
    if (name == QLatin1String("stability")) return quantiloom::ThermalConvectionModel::Stability;
    return quantiloom::ThermalConvectionModel::Constant;
}

inline QString convectionModelName(const quantiloom::ThermalConvectionModel model) {
    switch (model) {
        case quantiloom::ThermalConvectionModel::Wind:      return QStringLiteral("wind");
        case quantiloom::ThermalConvectionModel::Stability: return QStringLiteral("stability");
        case quantiloom::ThermalConvectionModel::Constant:  break;
    }
    return QStringLiteral("constant");
}

/// The five parameters a trajectory can be differentiated with respect to, by
/// the names `thermal.parameter_sensitivities` uses. A name this build does not
/// know is dropped, which is what the SDK does with it too.
inline quantiloom::Vector<quantiloom::ThermalSensitivityParameter>
sensitivitiesFromNames(const QStringList& names) {
    quantiloom::Vector<quantiloom::ThermalSensitivityParameter> out;
    for (const QString& name : names) {
        if (name == QLatin1String("h")) {
            out.push_back(quantiloom::ThermalSensitivityParameter::Convection);
        } else if (name == QLatin1String("epsilon")) {
            out.push_back(quantiloom::ThermalSensitivityParameter::Emissivity);
        } else if (name == QLatin1String("alpha")) {
            out.push_back(quantiloom::ThermalSensitivityParameter::Absorptivity);
        } else if (name == QLatin1String("k")) {
            out.push_back(quantiloom::ThermalSensitivityParameter::Conductivity);
        } else if (name == QLatin1String("rhoc")) {
            out.push_back(quantiloom::ThermalSensitivityParameter::HeatCapacity);
        }
    }
    return out;
}

inline QStringList sensitivityNames(
    const quantiloom::Vector<quantiloom::ThermalSensitivityParameter>& parameters) {
    QStringList out;
    for (const auto p : parameters) {
        switch (p) {
            case quantiloom::ThermalSensitivityParameter::Convection:
                out << QStringLiteral("h"); break;
            case quantiloom::ThermalSensitivityParameter::Emissivity:
                out << QStringLiteral("epsilon"); break;
            case quantiloom::ThermalSensitivityParameter::Absorptivity:
                out << QStringLiteral("alpha"); break;
            case quantiloom::ThermalSensitivityParameter::Conductivity:
                out << QStringLiteral("k"); break;
            case quantiloom::ThermalSensitivityParameter::HeatCapacity:
                out << QStringLiteral("rhoc"); break;
        }
    }
    return out;
}

}  // namespace thermalnames
