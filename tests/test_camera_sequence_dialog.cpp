#include "config/ConfigManager.hpp"
#include "dialogs/SequenceRenderDialog.hpp"
#include "dialogs/HyperspectralExportDialog.hpp"

#include <io/ImageIO.hpp>
#include <dataset/ExportSession.hpp>

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <cmath>
#include <iostream>

namespace {

bool verifyRecords(const QString& directory) {
    const auto records = QDir(directory).entryList({QStringLiteral("*.metadata.json")}, QDir::Files);
    if (records.isEmpty()) return false;
    for (const auto& record : records) {
        const auto checked = quantiloom::dataset::ExportSession::Verify(
            QDir(directory).filePath(record).toStdString());
        if (!checked.valid) {
            std::cerr << checked.json << '\n';
            return false;
        }
    }
    return true;
}

bool runHyperspectral(const SceneConfig& source, const QString& directory) {
    QDir().mkpath(directory);
    const QString previousDirectory = QDir::currentPath();
    struct RestoreDirectory {
        QString path;
        ~RestoreDirectory() { QDir::setCurrent(path); }
    } restore{previousDirectory};
    if (!QDir::setCurrent(directory)) return false;
    HyperspectralExportDialog dialog(source);
    auto* minimum = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("cubeWavelengthMin"));
    auto* maximum = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("cubeWavelengthMax"));
    auto* step = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("cubeWavelengthStep"));
    auto* spp = dialog.findChild<QSpinBox*>(QStringLiteral("cubeSpp"));
    auto* format = dialog.findChild<QComboBox*>(QStringLiteral("cubeFormat"));
    auto* output = dialog.findChild<QLineEdit*>(QStringLiteral("cubeOutput"));
    auto* start = dialog.findChild<QPushButton*>(QStringLiteral("cubeStart"));
    if (!minimum || !maximum || !step || !spp || !format || !output || !start) return false;
    minimum->setValue(8000.0);
    maximum->setValue(8200.0);
    step->setValue(100.0);
    spp->setValue(1);
    format->setCurrentIndex(format->findData(QStringLiteral("exr_spectral")));
    output->setText(QDir(directory).filePath(QStringLiteral("cube.exr")));
    QString modalError;
    QTimer dismissErrors;
    QObject::connect(&dismissErrors, &QTimer::timeout, &dialog, [&] {
        for (QWidget* window : QApplication::topLevelWidgets()) {
            if (auto* box = qobject_cast<QMessageBox*>(window); box && box->isVisible()) {
                modalError = box->text();
                box->accept();
            }
        }
    });
    dismissErrors.start(50);
    start->click();
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < 60000 && modalError.isEmpty() && !start->isEnabled()) {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(10);
    }
    if (!modalError.isEmpty()) std::cerr << modalError.toStdString() << '\n';
    return modalError.isEmpty() && start->isEnabled() && verifyRecords(directory);
}

bool runSweep(SceneConfig source, const QString& directory) {
    // At t=0 the moving block is outside this narrow physical-camera field.
    // Compare temperatures when the block occupies the view, not two ground-only frames.
    source.timeline.timeS = 0.5;
    SequenceRenderDialog dialog(source, {QStringLiteral("Material")}, {});
    const auto widget = [&dialog]<class T>(const char* name) {
        return dialog.findChild<T*>(QString::fromLatin1(name));
    };
    auto* material = widget.operator()<QComboBox>("sequenceMaterial");
    auto* from = widget.operator()<QDoubleSpinBox>("sequenceStartTemperature");
    auto* to = widget.operator()<QDoubleSpinBox>("sequenceEndTemperature");
    auto* count = widget.operator()<QSpinBox>("sequenceFrameCount");
    auto* spp = widget.operator()<QSpinBox>("sequenceSpp");
    auto* output = widget.operator()<QLineEdit>("sequenceOutputDir");
    auto* name = widget.operator()<QLineEdit>("sequenceNameTemplate");
    auto* start = widget.operator()<QPushButton>("sequenceStart");
    if (!material || !from || !to || !count || !spp || !output || !name || !start)
        return false;
    material->setCurrentText(QStringLiteral("Material"));
    from->setValue(440.0);
    to->setValue(460.0);
    count->setValue(2);
    spp->setValue(1);
    output->setText(directory);
    name->setText(QStringLiteral("frame_{index}.exr"));
    QDir().mkpath(directory);

    QString modalError;
    QTimer dismissErrors;
    QObject::connect(&dismissErrors, &QTimer::timeout, &dialog, [&] {
        for (QWidget* window : QApplication::topLevelWidgets()) {
            if (auto* box = qobject_cast<QMessageBox*>(window); box && box->isVisible()) {
                modalError = box->text();
                box->accept();
            }
        }
    });
    dismissErrors.start(50);
    start->click();

    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < 90000 && modalError.isEmpty()) {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        if (start->text() == QStringLiteral("Render")) break;
        QThread::msleep(10);
    }
    if (!modalError.isEmpty()) {
        std::cerr << modalError.toStdString() << '\n';
        return false;
    }
    if (start->text() != QStringLiteral("Render")) return false;
    const auto first = quantiloom::ImageIO::ReadEXR(
        QDir(directory).filePath(QStringLiteral("frame_1.exr")).toStdString());
    const auto second = quantiloom::ImageIO::ReadEXR(
        QDir(directory).filePath(QStringLiteral("frame_2.exr")).toStdString());
    if (source.cameraConfig.enabled) {
        for (const char* suffix : {"measurement", "rawdn", "corrected", "tapp", "display"}) {
            for (int frame = 1; frame <= 2; ++frame) {
                const auto cameraProduct = quantiloom::ImageIO::ReadEXR(
                    QDir(directory).filePath(QStringLiteral("frame_%1_%2.exr")
                        .arg(frame).arg(QString::fromLatin1(suffix))).toStdString());
                if (!cameraProduct || cameraProduct->data.empty() ||
                    cameraProduct->width != source.cameraConfig.optics.sensorWidthPx ||
                    cameraProduct->height != source.cameraConfig.optics.sensorHeightPx)
                    return false;
                const auto acquisition = cameraProduct->metadata.find("camera_acquisition_index");
                // Independent temperature samples must not inherit the
                // previous sample's device history.
                const auto warmupFrames = std::llround(source.cameraConfig.warmup.seconds /
                    source.cameraConfig.readout.framePeriodSeconds);
                if (acquisition == cameraProduct->metadata.end() ||
                    acquisition->second != std::to_string(warmupFrames))
                    return false;
            }
        }
    }
    return first && second && !first->data.empty() && first->data != second->data &&
           verifyRecords(directory);
}

bool runSequence(const SceneConfig& source,
                 const quantiloom::TimelineInfo& timeline,
                 const QString& directory, int every) {
    SequenceRenderDialog dialog(source, {}, timeline);
    const auto widget = [&dialog]<class T>(const char* name) {
        return dialog.findChild<T*>(QString::fromLatin1(name));
    };
    auto* mode = widget.operator()<QComboBox>("sequenceMode");
    auto* from = widget.operator()<QSpinBox>("sequenceFromTick");
    auto* to = widget.operator()<QSpinBox>("sequenceToTick");
    auto* stride = widget.operator()<QSpinBox>("sequenceEveryTick");
    auto* spp = widget.operator()<QSpinBox>("sequenceSpp");
    auto* output = widget.operator()<QLineEdit>("sequenceOutputDir");
    auto* name = widget.operator()<QLineEdit>("sequenceNameTemplate");
    auto* start = widget.operator()<QPushButton>("sequenceStart");
    if (!mode || !from || !to || !stride || !spp || !output || !name || !start)
        return false;
    mode->setCurrentIndex(1);
    from->setValue(0);
    to->setValue(8);
    stride->setValue(every);
    spp->setValue(1);
    output->setText(directory);
    name->setText(QStringLiteral("frame_{tick}.exr"));
    QDir().mkpath(directory);

    QString modalError;
    QTimer dismissErrors;
    QObject::connect(&dismissErrors, &QTimer::timeout, &dialog, [&] {
        for (QWidget* window : QApplication::topLevelWidgets()) {
            if (auto* box = qobject_cast<QMessageBox*>(window); box && box->isVisible()) {
                modalError = box->text();
                box->accept();
            }
        }
    });
    dismissErrors.start(50);
    start->click();

    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < 90000 && modalError.isEmpty()) {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        if (start->text() == QStringLiteral("Render")) break;
        QThread::msleep(10);
    }
    if (!modalError.isEmpty()) {
        std::cerr << modalError.toStdString() << '\n';
        return false;
    }
    if (start->text() != QStringLiteral("Render")) return false;
    const int expected = (8 / every) + 1;
    const auto frames = QDir(directory).entryList({QStringLiteral("frame_?????.exr")},
                                                  QDir::Files);
    return frames.size() == expected && verifyRecords(directory);
}

std::optional<quantiloom::Image> product(const QString& directory, int tick,
                                         const char* suffix) {
    const QString path = QDir(directory).filePath(
        QStringLiteral("frame_%1_%2.exr")
            .arg(tick, 5, 10, QLatin1Char('0'))
            .arg(QString::fromLatin1(suffix)));
    return quantiloom::ImageIO::ReadEXR(path.toStdString());
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const QString sourcePath = QString::fromUtf8(QUANTILOOM_DEV_ROOT) +
        QStringLiteral("/assets/configs/camera_history_check.toml");
    ConfigManager manager;
    SceneConfig config;
    if (!manager.loadConfig(sourcePath, config)) {
        std::cerr << manager.lastError().toStdString() << '\n';
        return 1;
    }
    config.timeline.present = true;
    config.timeline.body = QStringLiteral(
        "start_s = 0\nend_s = 1\nticks_per_second = 20\n");
    config.timeline.timeS = 0.0;
    config.cameraConfig.products.bandMeasurement = true;
    config.cameraConfig.products.correctedDeviceSignal = true;
    config.cameraConfig.products.apparentTemperature = true;
    quantiloom::TimelineInfo timeline;
    timeline.present = true;
    timeline.start_s = 0.0;
    timeline.end_s = 1.0;
    timeline.ticksPerSecond = 20.0;
    QTemporaryDir work;
    if (!work.isValid()) return 2;
    if (!runHyperspectral(config, work.filePath(QStringLiteral("different_cwd")))) return 7;
    const QString sweep = work.filePath(QStringLiteral("sweep"));
    const QString sparse = work.filePath(QStringLiteral("sparse"));
    const QString full = work.filePath(QStringLiteral("full"));
    if (!runSweep(config, sweep)) return 3;
    SceneConfig warmedSweep = config;
    warmedSweep.cameraConfig.warmup.seconds = 0.2;
    if (!runSweep(warmedSweep, work.filePath(QStringLiteral("sweep_warmup")))) return 9;
    if (!runSequence(config, timeline, sparse, 2) ||
        !runSequence(config, timeline, full, 1)) return 3;

    for (int tick = 0; tick <= 8; tick += 2) {
        for (const char* suffix : {"measurement", "rawdn", "corrected",
                                   "tapp", "display"}) {
            const auto a = product(sparse, tick, suffix);
            const auto b = product(full, tick, suffix);
            if (!a || !b || a->data != b->data) return 4;
            const auto index = a->metadata.find("camera_acquisition_index");
            if (index == a->metadata.end() ||
                index->second != std::to_string(tick / 2)) return 5;
        }
    }
    const auto first = product(full, 0, "rawdn");
    const auto reused = product(full, 1, "rawdn");
    if (!first || !reused || first->data != reused->data) return 6;
    const auto frozen = first->metadata.find("quantiloom_provenance");
    const auto reusedFrozen = reused->metadata.find("quantiloom_provenance");
    if (frozen == first->metadata.end() || reusedFrozen == reused->metadata.end() ||
        frozen->second != reusedFrozen->second) return 8;
    std::cout << "Qt sequence dialog acquisition history PASS\n";
    return 0;
}
