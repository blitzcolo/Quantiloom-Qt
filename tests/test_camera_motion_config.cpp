#include "config/ConfigManager.hpp"

#include <postprocess/CameraPresets.hpp>
#include <core/Config.hpp>

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <cmath>
#include <iostream>
#include <memory>
#include <utility>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;

    SceneConfig authored;
    authored.gltfPath = QStringLiteral("assets/models/cube.glb");
    authored.cameraPosition[0] = 3.0f;
    authored.cameraPosition[1] = 4.0f;
    authored.cameraPosition[2] = 5.0f;
    authored.timeline.present = true;
    authored.timeline.body = QStringLiteral(
        "start_s = 0\nend_s = 10\nticks_per_second = 20\n");
    authored.timeline.timeS = 1.25;
    auto preset = quantiloom::camera::MakePresetCameraConfig(
        quantiloom::camera::CameraPresetKind::GenericCmos);
    if (!preset) return 2;
    authored.cameraConfig = std::move(preset.value());
    authored.cameraConfig.motion.keys = {
        {0.013, {1.0, 2.0, 3.0}, {0.0, 0.0, 0.0}},
        {2.125, {2.0, 3.0, 4.0}, {1.0, 0.0, 0.0}}};
    ConfigManager writer;
    const QString text = writer.exportConfigToString(authored);
    auto parsed = quantiloom::Config::Parse(text.toStdString());
    if (!parsed) return 3;
    writer.adoptRawConfig(std::make_shared<quantiloom::Config>(
        std::move(parsed.value())));
    if (!writer.sharedRawConfig() ||
        writer.sharedRawConfig()->Get<double>("timeline.end_s") != 10.0) return 3;
    const QString path = directory.filePath(QStringLiteral("roundtrip.toml"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text) ||
        file.write(text.toUtf8()) != text.toUtf8().size()) return 3;
    file.close();

    ConfigManager reader;
    SceneConfig loaded;
    if (!reader.loadConfig(path, loaded)) {
        std::cerr << reader.lastError().toStdString() << '\n';
        return 4;
    }
    if (loaded.cameraConfig.motion.keys != authored.cameraConfig.motion.keys ||
        !loaded.timeline.present || !loaded.timeline.timeS ||
        std::abs(*loaded.timeline.timeS - 1.25) > 1e-12) return 5;
    for (int axis = 0; axis < 3; ++axis)
        if (loaded.cameraPosition[axis] != authored.cameraPosition[axis] ||
            loaded.cameraLookAt[axis] != authored.cameraLookAt[axis]) return 6;
    std::cout << "camera motion config round-trip PASS\n";
    return 0;
}
