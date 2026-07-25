/**
 * @file SdkGuard.cpp
 * @brief Implementation of the SDK binary consistency check.
 */

#include "SdkGuard.hpp"

#include "SdkStamp.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {

/// SHA-256 of a file as lowercase hex, or an empty string if it cannot be read.
/// Streams through QCryptographicHash rather than loading the file, since the
/// SDK library is several megabytes.
QString sha256Of(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return {};
    }
    return QString::fromLatin1(hash.result().toHex());
}

} // namespace

SdkCheckResult checkSdkBinaries() {
    using namespace quantiloom_qt::sdk_stamp;

    SdkCheckResult result;

    const QString builtAgainst = QString::fromLatin1(kSdkLibrarySha256);
    const QString libraryName = QString::fromLatin1(kSdkLibraryName);
    if (builtAgainst.isEmpty() || libraryName.isEmpty()) {
        // CMake could not read the SDK library at configure time; it already
        // warned there. Nothing to compare against.
        return result;
    }

    // 1. The copy that will actually be loaded sits next to the executable.
    const QString loadedPath =
        QDir(QCoreApplication::applicationDirPath()).filePath(libraryName);
    if (!QFileInfo::exists(loadedPath)) {
        // Not a layout this check understands (the library may come from the
        // system loader path). Stay quiet rather than guess.
        return result;
    }

    const QString loadedHash = sha256Of(loadedPath);
    if (loadedHash.isEmpty()) {
        return result;
    }

    if (loadedHash != builtAgainst) {
        result.mismatch = true;
        result.message =
            QCoreApplication::translate("SdkGuard",
                "%1 next to the executable is not the library this build was "
                "linked against.\n\n"
                "Loaded:       %2\n"
                "Built against: %3\n\n"
                "The SDK's public types are source-compatible but not binary-"
                "compatible across updates, so continuing would be undefined "
                "behaviour with no visible error. Rebuild:\n\n"
                "    cd /mnt/d/Quantiloom-Qt && ./build_wsl.sh")
                .arg(libraryName, loadedHash.left(16), builtAgainst.left(16));
        return result;
    }

    // 2. The pairing is consistent, but the SDK may have moved on since. This
    //    only resolves on a machine that has the SDK tree, i.e. a dev machine.
    const QString sdkPath = QString::fromLatin1(kSdkLibraryPath);
    if (!sdkPath.isEmpty() && QFileInfo::exists(sdkPath)) {
        const QString sdkHash = sha256Of(sdkPath);
        if (!sdkHash.isEmpty() && sdkHash != builtAgainst) {
            result.stale = true;
            result.message =
                QCoreApplication::translate("SdkGuard",
                    "The Quantiloom SDK at %1 has been reinstalled since this "
                    "executable was built. This run uses the older library and "
                    "will not reflect the current core.\n\n"
                    "Rebuild with:\n"
                    "    cd /mnt/d/Quantiloom-Qt && ./build_wsl.sh")
                    .arg(QString::fromLatin1(kSdkDir));
        }
    }

    return result;
}
