#include "config/ConfigManager.hpp"
#include "dialogs/SequenceRenderDialog.hpp"

#include <io/ImageIO.hpp>

#include <QApplication>
#include <QComboBox>
#include <QDir>
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
    return frames.size() == expected;
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
    const QString sparse = work.filePath(QStringLiteral("sparse"));
    const QString full = work.filePath(QStringLiteral("full"));
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
    std::cout << "Qt sequence dialog acquisition history PASS\n";
    return 0;
}
