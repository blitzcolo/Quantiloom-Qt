/**
 * @file OfflineExport.cpp
 * @brief Shared offline-renderer init and camera-product writing
 */

#include "OfflineExport.hpp"

#include "../AssetPaths.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>

#include <core/Image.hpp>
#include <dataset/ExportSession.hpp>
#include <io/ImageIO.hpp>

#include <utility>

namespace offlineexport {

quantiloom::OfflineRenderer::InitParams rendererInit(const QString& baseDir) {
    quantiloom::OfflineRenderer::InitParams init;
    init.baseDir = baseDir.toStdString();
    const QString modelPack = assetpaths::atmosphereModelPackDir();
    if (!modelPack.isEmpty()) {
        init.atmosphereModelPackFallback = modelPack.toStdString();
    }
    return init;
}

const char* calibrationStatusName(quantiloom::camera::CalibrationStatus status) {
    switch (status) {
        case quantiloom::camera::CalibrationStatus::HardwareReference:
            return "hardware_reference";
        case quantiloom::camera::CalibrationStatus::Calibrated:
            return "calibrated";
        default:
            return "generic_assumption";
    }
}

bool writeCameraProducts(const quantiloom::camera::CameraOutput& output,
                         const QString& exrPath, QString* error,
                         quantiloom::dataset::ExportSession* session,
                         int* written) {
    if (written) {
        *written = 0;
    }
    const QFileInfo file(exrPath);
    const QString base = file.absolutePath() + QLatin1Char('/') + file.completeBaseName();
    QStringList metadata;
    for (const auto& [product, suffix] : {
             std::pair{&output.bandMeasurement, "_measurement"},
             std::pair{&output.rawDn, "_rawdn"},
             std::pair{&output.correctedDeviceSignal, "_corrected"},
             std::pair{&output.apparentTemperature, "_tapp"},
             std::pair{&output.display, "_display"},
             std::pair{&output.cieLinearSrgb, "_cie"},
             std::pair{&output.tracedRadiance, "_spectral"}}) {
        if (!*product) continue;
        const QString path = base + QString::fromLatin1(suffix) + QStringLiteral(".exr");
        const std::string name = QFileInfo(path).fileName().toStdString();
        if (session) {
            // The session says why it refused (a traversal, a duplicate
            // product id); keep that rather than a bare "could not write".
            const auto staged = session->WriteImage(name, name, (*product)->image, "{}");
            if (!staged) {
                if (error) *error = QString::fromStdString(staged.error());
                return false;
            }
        } else if (!quantiloom::ImageIO::WriteEXR(path.toStdString(), (*product)->image)) {
            if (error) {
                *error = QCoreApplication::translate("OfflineExport", "could not write %1").arg(path);
            }
            return false;
        }
        if (written) {
            ++*written;
        }
        const auto& signal = (*product)->signal;
        metadata << QStringLiteral("%1 unit=%2 calibration=%3 acquisition=%4 exposure=[%5, %6] s")
                        .arg(QFileInfo(path).fileName(), QString::fromStdString(signal.unit),
                             QString::fromLatin1(calibrationStatusName(signal.calibration)))
                        .arg(signal.acquisitionIndex)
                        .arg(signal.exposureStartSeconds, 0, 'g', 9)
                        .arg(signal.exposureEndSeconds, 0, 'g', 9);
    }
    // No product, no sidecar: a header with nothing under it would describe
    // files that do not exist.
    if (metadata.isEmpty()) {
        return true;
    }
    QString summaryPath = base + QStringLiteral("_products.txt");
    const std::string summaryName = QFileInfo(summaryPath).fileName().toStdString();
    if (session) {
        const auto staged = session->StagingPath(summaryName);
        if (!staged) {
            if (error) *error = QString::fromStdString(staged.error());
            return false;
        }
        summaryPath = QString::fromStdString(staged.value());
    }
    QFile sidecar(summaryPath);
    if (!sidecar.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            *error = QCoreApplication::translate("OfflineExport", "could not write %1")
                         .arg(sidecar.fileName());
        }
        return false;
    }
    // The sidecar is ASCII on purpose: a Windows console on a CJK locale is
    // not the only place these land, and the metadata is for diffing as much
    // as for reading.
    QTextStream out(&sidecar);
    out << "Quantiloom camera products\n";
    for (const auto& line : std::as_const(metadata)) out << line << '\n';
    out.flush();
    if (out.status() != QTextStream::Ok || !sidecar.flush()) {
        if (error) {
            *error = QCoreApplication::translate("OfflineExport", "could not write %1")
                         .arg(summaryPath);
        }
        return false;
    }
    sidecar.close();
    if (session) {
        const auto registered = session->RegisterFile(summaryName, summaryName, "{}");
        if (!registered) {
            if (error) *error = QString::fromStdString(registered.error());
            return false;
        }
    }
    return true;
}

} // namespace offlineexport
