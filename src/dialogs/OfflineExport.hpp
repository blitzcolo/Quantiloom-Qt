/**
 * @file OfflineExport.hpp
 * @brief What the three offline export paths share
 *
 * The sequence render, the hyperspectral cube and the fusion dataset each run
 * dialog-owned OfflineRenderer jobs, and a camera-product capture writes the
 * same file set wherever it was taken. One copy of each lives here.
 */

#pragma once

#include <QString>

#include <postprocess/CameraPipeline.hpp>
#include <renderer/OfflineRenderer.hpp>

namespace quantiloom::dataset {
class ExportSession;
}

namespace offlineexport {

/// InitParams for a dialog-owned OfflineRenderer: baseDir, plus the NN
/// atmosphere model pack through the same lookup the core CLI makes, so a
/// scene with an NN atmosphere renders here the way it would there.
[[nodiscard]]
quantiloom::OfflineRenderer::InitParams rendererInit(const QString& baseDir);

/// The calibration status as its _products.txt token -- the spelling the
/// SDK's CameraConfigIO uses for `calibration` keys, whose Token() is not
/// exported, hence this copy.
[[nodiscard]]
const char* calibrationStatusName(quantiloom::camera::CalibrationStatus status);

/// Write every product the capture produced: one <stem>_<suffix>.exr each,
/// the suffixes being the core CLI's (_measurement, _rawdn, _corrected,
/// _tapp, _display, _cie, _spectral), plus a <stem>_products.txt sidecar
/// naming the unit, calibration and exposure window per file. With a session
/// the files are staged and registered into it rather than written beside
/// the EXR directly. `written`, when given, counts the products written.
/// On failure `error` names the file.
bool writeCameraProducts(const quantiloom::camera::CameraOutput& output,
                         const QString& exrPath, QString* error,
                         quantiloom::dataset::ExportSession* session,
                         int* written = nullptr);

} // namespace offlineexport
