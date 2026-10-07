/**
 * @file AssetPaths.cpp
 * @brief Bundled asset lookup
 */

#include "AssetPaths.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace assetpaths {

QString bundled(const QString& relativePath) {
    const QStringList candidates{
        QCoreApplication::applicationDirPath() + QLatin1Char('/') + relativePath,
        QDir::currentPath() + QLatin1Char('/') + relativePath,
    };
    for (const QString& path : candidates) {
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    return {};
}

QString atmosphereModelPackDir() {
    const QStringList candidates{
        qEnvironmentVariable("QUANTILOOM_ATMOS_MODELS"),
        QDir::currentPath() + QStringLiteral("/assets/atmos_models"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/assets/atmos_models"),
    };
    for (const QString& dir : candidates) {
        if (!dir.isEmpty() && QDir(dir).exists()) {
            // Qt-style forward slashes, which Windows accepts everywhere this
            // goes. This string ends up in saved configs via GetAtmosphere(),
            // and native backslashes made every reader deal with escaping.
            return dir;
        }
    }
    return {};
}

} // namespace assetpaths
