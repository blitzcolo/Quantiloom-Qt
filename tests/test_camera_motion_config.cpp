#include "config/ConfigManager.hpp"
#include "editing/UndoStack.hpp"
#include <QDir>
#include <QFileInfo>

#include <postprocess/CameraPresets.hpp>
#include <core/Config.hpp>

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <cmath>
#include <iostream>
#include <memory>
#include <utility>

namespace {
class CountingCommand : public Command {
public:
    CountingCommand(int& value, int& executions, int before, int after)
        : Command(QStringLiteral("counter")), value(value), executions(executions),
          before(before), after(after) {}
    void execute() override { ++executions; value = after; }
    void undo() override { value = before; }
    int id() const override { return 1; }
    bool mergeWith(const Command* other) override {
        const auto* next = dynamic_cast<const CountingCommand*>(other);
        if (!next) return false;
        after = next->after;
        return true;
    }
    int& value;
    int& executions;
    int before, after;
};

bool historyRegression() {
    UndoStack stack;
    int value = 0, executions = 0;
    stack.push(std::make_unique<CountingCommand>(value, executions, 0, 1));
    if (executions != 1 || value != 1) return false;
    stack.setClean();
    stack.push(std::make_unique<CountingCommand>(value, executions, 1, 2));
    if (executions != 2 || stack.isClean()) return false;
    stack.undo();
    if (!stack.isClean() || value != 1) return false;
    stack.clear();
    stack.redo();
    return !stack.canUndo() && !stack.canRedo() && stack.isClean() && value == 1;
}

bool pathAndIlluminantRegression(const QString& root) {
    const QString source = root + QStringLiteral("/source");
    const QString destination = root + QStringLiteral("/saved");
    QDir().mkpath(source);
    QDir().mkpath(destination);
    for (const auto& leaf : {QStringLiteral("model.gltf"), QStringLiteral("custom.csv")}) {
        QFile asset(source + QLatin1Char('/') + leaf);
        if (!asset.open(QIODevice::WriteOnly)) return false;
    }
    SceneConfig authored;
    authored.baseDir = source;
    authored.gltfPath = QStringLiteral("model.gltf");
    authored.solarLutPath = QStringLiteral("custom.csv");
    authored.solarLutColumns = {4, 3};
    authored.solarLutDiffuseIsGlobal = true;
    authored.solarLutNormalise = QStringLiteral("unit_luminance");
    ConfigManager manager;
    const QString file = destination + QStringLiteral("/scene.toml");
    if (!manager.exportConfig(file, authored)) return false;
    SceneConfig loaded;
    if (!manager.loadConfig(file, loaded)) return false;
    const auto resolves = [&](const QString& path, const QString& original) {
        return QFileInfo(path).isRelative() &&
            QFileInfo(QDir(destination).filePath(path)).canonicalFilePath() ==
            QFileInfo(source + QLatin1Char('/') + original).canonicalFilePath();
    };
    if (!resolves(loaded.gltfPath, QStringLiteral("model.gltf")) ||
        !resolves(loaded.solarLutPath, QStringLiteral("custom.csv")) ||
        loaded.solarLutColumns != authored.solarLutColumns ||
        loaded.solarLutNormalise != authored.solarLutNormalise ||
        !loaded.solarLutDiffuseIsGlobal) return false;
    return manager.exportConfig(source + QStringLiteral("/same.toml"), authored) &&
        manager.loadConfig(source + QStringLiteral("/same.toml"), loaded) &&
        loaded.gltfPath == authored.gltfPath && loaded.solarLutPath == authored.solarLutPath;
}
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    if (!historyRegression()) return 7;
    if (!pathAndIlluminantRegression(directory.path())) return 8;

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
