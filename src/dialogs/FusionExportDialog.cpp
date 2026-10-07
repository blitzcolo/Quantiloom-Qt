/**
 * @file FusionExportDialog.cpp
 * @brief Multi-camera rig editor and fusion dataset export implementation
 *
 * The dialog receives a frozen snapshot of the scene configuration, taken by
 * the caller before it opens, so edits made to the document while it is up
 * cannot change what the export runs against. It edits a rig of independently
 * configured cameras: each has a pose relative to the rig and its own
 * CameraConfig, with camera motion stripped, and the rig names a reference
 * camera and pairs cameras either by the default reference pairing or by an
 * explicit list.
 *
 * Previews are pushed to the host through the `preview` callback, which
 * receives the preview scene configuration and the camera configuration of
 * the camera being previewed; the host owns restoring the viewport once the
 * dialog closes. The export itself runs `dataset::FusionExportJob::Run` on a
 * worker thread. Cancellation is honoured until the job reports its publish
 * phase; once publication begins, close and cancel are refused because
 * publication is transactional.
 */

#include "FusionExportDialog.hpp"

#include "OfflineExport.hpp"
#include "../panels/SensorPanel.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <scene/Camera.hpp>
#include <postprocess/CameraConfigIO.hpp>
#include <postprocess/CameraPresets.hpp>

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace {

using quantiloom::String;
using quantiloom::dataset::RigCamera;
using quantiloom::dataset::RigConfig;
using quantiloom::dataset::RigPair;

/// The six pose-field captions: three translations and the three Euler angles
/// poseToCameraToRig() composes.
constexpr const char* kPoseCaptions[] = {
    QT_TRANSLATE_NOOP("FusionExportDialog", "X (world units):"),
    QT_TRANSLATE_NOOP("FusionExportDialog", "Y (world units):"),
    QT_TRANSLATE_NOOP("FusionExportDialog", "Z (world units):"),
    QT_TRANSLATE_NOOP("FusionExportDialog", "Pitch (degrees):"),
    QT_TRANSLATE_NOOP("FusionExportDialog", "Yaw (degrees):"),
    QT_TRANSLATE_NOOP("FusionExportDialog", "Roll (degrees):"),
};

/// The rig's first camera is the scene's own: same position and orientation.
/// The scene camera is described in right/up/forward axes while cameraToRig
/// uses the SDK's RDF convention, so the conversion negates the up column.
/// The matrix is row-major with the translation in column 3.
RigCamera firstCameraFromScene(const quantiloom::Config& scene,
                               quantiloom::SpectralMode spectralMode,
                               const String& baseDirectory) {
    RigCamera camera;
    camera.id = "camera_0";
    const auto preset =
        quantiloom::camera::MakePresetCameraConfig(
            quantiloom::camera::CameraPresetKind::GenericCmos);
    camera.sensor = preset.value();
    const auto authored =
        quantiloom::ParseCameraConfig(scene, spectralMode, baseDirectory);
    if (authored && authored.value().enabled) {
        camera.sensor = authored.value();
    }
    camera.sensor.enabled = true;
    camera.sensor.motion.keys.clear();
    const auto sceneCamera = quantiloom::Camera::FromConfig(scene, 1.0f);
    if (sceneCamera) {
        const auto p = sceneCamera.value().GetCameraData();
        camera.cameraToRig = {p.right.x, -p.up.x, p.forward.x, p.origin.x,
                              p.right.y, -p.up.y, p.forward.y, p.origin.y,
                              p.right.z, -p.up.z, p.forward.z, p.origin.z,
                              0,         0,       0,           1};
    }
    return camera;
}

/// Write a pose into a rig matrix. The matrix is row-major, holds RDF camera
/// axes and carries its translation in column 3; the bottom row is left as it
/// is. The angles are degrees in GLM's Euler order -- glm::dquat(vec3)
/// composes them and glm::eulerAngles reads them back.
void poseToCameraToRig(double x, double y, double z,
                       double pitchDeg, double yawDeg, double rollDeg,
                       std::array<quantiloom::f64, 16>& cameraToRig) {
    const glm::dquat q(glm::radians(glm::dvec3(pitchDeg, yawDeg, rollDeg)));
    const glm::dmat3 r = glm::mat3_cast(q);
    const double translation[3] = {x, y, z};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            cameraToRig[row * 4 + col] = r[col][row];
        }
        cameraToRig[row * 4 + 3] = translation[row];
    }
}

/// The reverse of poseToCameraToRig: {x, y, z, pitch, yaw, roll} in world
/// units and degrees, same matrix convention and Euler order.
std::array<double, 6> cameraToRigToPose(
    const std::array<quantiloom::f64, 16>& cameraToRig) {
    glm::dmat3 r;
    std::array<double, 6> pose{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            r[col][row] = cameraToRig[row * 4 + col];
        }
        pose[row] = cameraToRig[row * 4 + 3];
    }
    const glm::dvec3 angles = glm::degrees(glm::eulerAngles(glm::quat_cast(r)));
    pose[3] = angles.x;
    pose[4] = angles.y;
    pose[5] = angles.z;
    return pose;
}

/// The explicit pair list as the widget edits it: "source > target; ...".
QString pairsText(const std::vector<RigPair>& pairs) {
    QStringList text;
    for (const RigPair& pair : pairs) {
        text << QString::fromStdString(pair.sourceCamera + " > " + pair.targetCamera);
    }
    return text.join("; ");
}

}  // namespace

FusionExportDialog::FusionExportDialog(const quantiloom::Config& scene,
                                       const QString& baseDirectory,
                                       quantiloom::SpectralMode spectralMode,
                                       QWidget* parent)
    : QDialog(parent)
    , m_scene(scene)
    , m_baseDirectory(baseDirectory)
{
    resize(1050, 800);
    m_rig.id = "studio-rig";
    m_rig.referenceCamera = "camera_0";
    m_rig.cameras.push_back(
        firstCameraFromScene(scene, spectralMode, baseDirectory.toStdString()));

    setupUi();
    retranslateUi();
    refreshList();
    m_cameras->setCurrentRow(0);
}

FusionExportDialog::~FusionExportDialog() {
    // The worker may still be running; wait for it rather than leave a job
    // touching a destroyed dialog, and free the thread object, which has no
    // parent of its own.
    m_cancelled = true;
    if (m_thread) {
        m_thread->wait();
        delete m_thread.data();
    }
}

void FusionExportDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* top = new QFormLayout;
    m_sample = new QLineEdit(QStringLiteral("sample"));
    m_sampleLabel = new QLabel;
    top->addRow(m_sampleLabel, m_sample);
    m_output = new QLineEdit(QDir::current().filePath("fusion-output"));
    auto* outputRow = new QHBoxLayout;
    outputRow->addWidget(m_output);
    m_browseButton = new QPushButton;
    connect(m_browseButton, &QPushButton::clicked,
            this, &FusionExportDialog::browseOutput);
    outputRow->addWidget(m_browseButton);
    m_outputLabel = new QLabel;
    top->addRow(m_outputLabel, outputRow);
    layout->addLayout(top);

    auto* split = new QSplitter;
    layout->addWidget(split, 1);

    auto* left = new QWidget;
    auto* listLayout = new QVBoxLayout(left);
    m_cameras = new QListWidget;
    listLayout->addWidget(m_cameras);
    auto* row = new QHBoxLayout;
    m_addButton = new QPushButton;
    m_removeButton = new QPushButton;
    row->addWidget(m_addButton);
    row->addWidget(m_removeButton);
    listLayout->addLayout(row);
    m_referenceLabel = new QLabel;
    listLayout->addWidget(m_referenceLabel);
    m_reference = new QComboBox;
    listLayout->addWidget(m_reference);
    m_pairMode = new QComboBox;
    m_pairMode->setObjectName(QStringLiteral("pairMode"));
    m_pairMode->addItem(QString());
    m_pairMode->addItem(QString());
    listLayout->addWidget(m_pairMode);
    m_pairs = new QLineEdit;
    m_pairs->setObjectName(QStringLiteral("explicitPairs"));
    listLayout->addWidget(m_pairs);
    m_pairs->setEnabled(false);
    connect(m_pairMode, &QComboBox::currentIndexChanged, this,
            [this](int index) { m_pairs->setEnabled(index == 1); });
    m_loadRigButton = new QPushButton;
    m_saveRigButton = new QPushButton;
    listLayout->addWidget(m_loadRigButton);
    listLayout->addWidget(m_saveRigButton);
    split->addWidget(left);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* right = new QWidget;
    auto* rightLayout = new QVBoxLayout(right);
    auto* pose = new QFormLayout;
    m_id = new QLineEdit;
    m_cameraIdLabel = new QLabel;
    pose->addRow(m_cameraIdLabel, m_id);
    for (int i = 0; i < 6; ++i) {
        m_pose[i] = new QDoubleSpinBox;
        m_pose[i]->setRange(i < 3 ? -1e8 : -360, i < 3 ? 1e8 : 360);
        m_pose[i]->setDecimals(6);
        m_poseLabels[i] = new QLabel;
        pose->addRow(m_poseLabels[i], m_pose[i]);
    }
    rightLayout->addLayout(pose);
    m_sensor = new SensorPanel;
    rightLayout->addWidget(m_sensor);
    m_previewButton = new QPushButton;
    rightLayout->addWidget(m_previewButton);
    connect(m_previewButton, &QPushButton::clicked,
            this, &FusionExportDialog::updatePreview);
    scroll->setWidget(right);
    split->addWidget(scroll);
    split->setStretchFactor(1, 1);

    m_rectify = new QCheckBox;
    layout->addWidget(m_rectify);
    m_progress = new QProgressBar;
    layout->addWidget(m_progress);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    auto* buttons = new QHBoxLayout;
    m_start = new QPushButton;
    m_closeButton = new QPushButton;
    buttons->addStretch();
    buttons->addWidget(m_start);
    buttons->addWidget(m_closeButton);
    layout->addLayout(buttons);

    connect(m_closeButton, &QPushButton::clicked,
            this, &FusionExportDialog::reject);
    connect(m_start, &QPushButton::clicked,
            this, &FusionExportDialog::start);
    connect(m_cameras, &QListWidget::currentRowChanged,
            this, &FusionExportDialog::selectCamera);
    connect(m_reference, &QComboBox::currentTextChanged, this,
            [this](const QString& id) {
                if (!m_loading) {
                    m_rig.referenceCamera = id.toStdString();
                }
            });
    // A pose-field drag or a sensor edit would run the rig round-trip and a
    // viewport camera apply once per valueChanged; the timer folds a burst
    // into one preview after the gesture pauses.
    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(150);
    connect(m_previewTimer, &QTimer::timeout,
            this, &FusionExportDialog::updatePreview);
    for (auto* field : m_pose) {
        connect(field, &QDoubleSpinBox::valueChanged, this,
                [this]() { m_previewTimer->start(); });
    }
    connect(m_sensor, &SensorPanel::cameraConfigChanged, this,
            [this]() { m_previewTimer->start(); });
    connect(m_addButton, &QPushButton::clicked,
            this, &FusionExportDialog::addCamera);
    connect(m_removeButton, &QPushButton::clicked,
            this, &FusionExportDialog::removeCamera);
    connect(m_loadRigButton, &QPushButton::clicked,
            this, &FusionExportDialog::loadRig);
    connect(m_saveRigButton, &QPushButton::clicked,
            this, &FusionExportDialog::saveRig);
}

void FusionExportDialog::browseOutput() {
    const QString path = QFileDialog::getExistingDirectory(
        this, tr("Output directory"), m_output->text());
    if (!path.isEmpty()) {
        m_output->setText(path);
    }
}

void FusionExportDialog::addCamera() {
    if (m_thread) {
        return;
    }
    saveCamera();
    RigCamera copy = m_rig.cameras[m_selected < 0 ? 0 : m_selected];
    int next = static_cast<int>(m_rig.cameras.size());
    do {
        copy.id = "camera_" + std::to_string(next++);
    } while (std::any_of(m_rig.cameras.begin(), m_rig.cameras.end(),
                         [&copy](const RigCamera& camera) {
                             return camera.id == copy.id;
                         }));
    // Nudged 0.1 in rig X so the copy is not coincident with its source.
    copy.cameraToRig[3] += 0.1;
    m_rig.cameras.push_back(copy);
    refreshList();
    m_cameras->setCurrentRow(static_cast<int>(m_rig.cameras.size() - 1));
}

void FusionExportDialog::removeCamera() {
    if (m_thread || m_selected < 0 || m_rig.cameras.size() < 2) {
        return;
    }
    if (!collectPairs()) {
        return;
    }
    const String removed = m_rig.cameras[static_cast<size_t>(m_selected)].id;
    std::erase_if(m_rig.pairs, [&removed](const RigPair& pair) {
        return pair.sourceCamera == removed || pair.targetCamera == removed;
    });
    m_pairs->setText(pairsText(m_rig.pairs));
    m_status->setText(tr("Removed camera and its pairs."));
    m_rig.cameras.erase(m_rig.cameras.begin() + m_selected);
    m_selected = -1;
    if (m_rig.referenceCamera == removed) {
        m_rig.referenceCamera = m_rig.cameras.front().id;
    }
    refreshList();
    m_cameras->setCurrentRow(0);
}

void FusionExportDialog::loadRig() {
    if (m_thread) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Load rig"), QString(), tr("TOML (*.toml)"));
    if (path.isEmpty()) {
        return;
    }
    auto document = quantiloom::Config::Load(path.toStdString());
    if (!document) {
        QMessageBox::warning(this, tr("Rig"),
                             QString::fromStdString(document.error()));
        return;
    }
    auto rig = quantiloom::dataset::ParseRigConfig(
        *document, QFileInfo(path).absolutePath().toStdString());
    if (!rig) {
        QMessageBox::warning(this, tr("Rig"),
                             QString::fromStdString(rig.error()));
        return;
    }
    m_rig = *rig;
    m_selected = -1;
    m_pairMode->setCurrentIndex(document.value().Has("rig.pairs") ? 1 : 0);
    m_pairs->setText(pairsText(m_rig.pairs));
    refreshList();
    m_cameras->setCurrentRow(0);
}

void FusionExportDialog::saveRig() {
    saveCamera();
    if (!collectPairs()) {
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save rig"), QString(), tr("TOML (*.toml)"));
    if (path.isEmpty()) {
        return;
    }
    // QSaveFile, as in ConfigManager::exportConfig: a rig that failed to
    // commit leaves the previous file alone rather than half of a new one.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(QByteArray::fromStdString(
            quantiloom::dataset::RigConfigToToml(m_rig))) < 0 ||
        !file.commit()) {
        QMessageBox::warning(this, tr("Rig"), tr("Could not write the rig."));
    }
}

void FusionExportDialog::saveCamera() {
    if (m_loading || m_selected < 0) {
        return;
    }
    RigCamera& camera = m_rig.cameras[static_cast<size_t>(m_selected)];
    const String previous = camera.id;
    camera.id = m_id->text().toStdString();
    if (previous != camera.id) {
        // A rename follows the id everywhere it was recorded: the parsed
        // pairs, the pair text and both combos.
        for (auto& pair : m_rig.pairs) {
            if (pair.sourceCamera == previous) {
                pair.sourceCamera = camera.id;
            }
            if (pair.targetCamera == previous) {
                pair.targetCamera = camera.id;
            }
        }
        const QRegularExpression wholeId(
            QLatin1String("(?<![A-Za-z0-9_-])") +
            QRegularExpression::escape(QString::fromStdString(previous)) +
            QLatin1String("(?![A-Za-z0-9_-])"));
        m_pairs->setText(m_pairs->text().replace(
            wholeId, QString::fromStdString(camera.id)));
        m_cameras->item(m_selected)->setText(QString::fromStdString(camera.id));
        const int referenceIndex =
            m_reference->findText(QString::fromStdString(previous));
        if (referenceIndex >= 0) {
            m_reference->setItemText(referenceIndex,
                                     QString::fromStdString(camera.id));
        }
    }
    if (m_rig.referenceCamera == previous) {
        m_rig.referenceCamera = camera.id;
    }
    camera.sensor = m_sensor->getCameraConfig();
    camera.sensor.enabled = true;
    camera.sensor.motion.keys.clear();
    poseToCameraToRig(m_pose[0]->value(), m_pose[1]->value(), m_pose[2]->value(),
                      m_pose[3]->value(), m_pose[4]->value(), m_pose[5]->value(),
                      camera.cameraToRig);
}

void FusionExportDialog::selectCamera(int index) {
    if (m_loading) {
        return;
    }
    saveCamera();
    m_selected = index;
    if (index < 0) {
        return;
    }
    m_loading = true;
    const RigCamera& camera = m_rig.cameras[static_cast<size_t>(index)];
    m_id->setText(QString::fromStdString(camera.id));
    const auto pose = cameraToRigToPose(camera.cameraToRig);
    for (int i = 0; i < 6; ++i) {
        m_pose[i]->setValue(pose[i]);
    }
    m_sensor->setCameraConfig(camera.sensor);
    m_sensor->setCameraEnabled(camera.sensor.enabled);
    m_loading = false;
    updatePreview();
}

void FusionExportDialog::refreshList() {
    m_loading = true;
    m_cameras->clear();
    m_reference->clear();
    for (const RigCamera& camera : m_rig.cameras) {
        const QString id = QString::fromStdString(camera.id);
        m_cameras->addItem(id);
        m_reference->addItem(id);
    }
    m_reference->setCurrentText(QString::fromStdString(m_rig.referenceCamera));
    m_loading = false;
}

bool FusionExportDialog::collectPairs() {
    m_rig.referenceCamera = m_reference->currentText().toStdString();
    m_rig.pairs.clear();
    if (m_pairMode->currentIndex() == 0) {
        for (const RigCamera& camera : m_rig.cameras) {
            if (camera.id != m_rig.referenceCamera) {
                m_rig.pairs.push_back({camera.id, m_rig.referenceCamera});
            }
        }
    } else {
        for (const auto& pair : m_pairs->text().split(';', Qt::SkipEmptyParts)) {
            const auto parts = pair.split('>');
            if (parts.size() != 2) {
                QMessageBox::warning(this, tr("Rig"),
                                     tr("Use source > target for each pair."));
                return false;
            }
            m_rig.pairs.push_back({parts[0].trimmed().toStdString(),
                                   parts[1].trimmed().toStdString()});
        }
    }
    return true;
}

quantiloom::Result<quantiloom::dataset::RigConfig, quantiloom::String>
FusionExportDialog::validatedRig() const {
    // ParseRigConfig is the only validator the SDK exports for a rig, so the
    // editor's state is validated by serialising it and parsing it back.
    const auto document =
        quantiloom::Config::Parse(quantiloom::dataset::RigConfigToToml(m_rig));
    if (!document) {
        return quantiloom::Result<RigConfig, String>::Err(document.error());
    }
    return quantiloom::dataset::ParseRigConfig(*document,
                                               m_baseDirectory.toStdString());
}

void FusionExportDialog::start() {
    // The Export button doubles as Cancel while a job runs. A click in that
    // state asks the job to stop, which it honours until publication begins.
    if (m_thread) {
        if (m_publishing) {
            return;
        }
        m_cancelled = true;
        m_start->setEnabled(false);
        return;
    }
    saveCamera();
    m_rig.referenceCamera = m_reference->currentText().toStdString();
    if (!collectPairs()) {
        return;
    }
    const auto rig = validatedRig();
    if (!rig) {
        QMessageBox::warning(this, tr("Rig"),
                             QString::fromStdString(rig.error()));
        return;
    }

    quantiloom::dataset::FusionExportOptions options;
    options.outputDirectory = m_output->text().toStdString();
    options.sampleId = m_sample->text().toStdString();
    options.rectify = m_rectify->isChecked();
    options.renderer = offlineexport::rendererInit(m_baseDirectory);
    m_cancelled = false;
    m_publishing = false;
    options.cancelled = [this]() { return m_cancelled.load(); };
    const QPointer<FusionExportDialog> self(this);
    options.onProgress = [self](
        const quantiloom::dataset::FusionExportProgress& progress) {
        if (!self) {
            return;
        }
        // Recorded on the worker side, before the queued UI update: a close
        // that lands between the two must already see the publish phase.
        if (progress.phase == "publish") {
            self->m_publishing = true;
        }
        QMetaObject::invokeMethod(self.data(), [self, progress]() {
            if (self) {
                self->showProgress(progress);
            }
        }, Qt::QueuedConnection);
    };
    const quantiloom::Config frozen = m_scene;
    const RigConfig cameras = *rig;
    m_thread = QThread::create([self, frozen, cameras, options]() {
        auto result =
            quantiloom::dataset::FusionExportJob::Run(frozen, cameras, options);
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self.data(), [self, result]() {
            if (self) {
                self->showResult(result);
            }
        }, Qt::QueuedConnection);
    });
    connect(m_thread.data(), &QThread::finished, this, [this]() {
        QThread* thread = m_thread.data();
        m_thread = nullptr;
        m_publishing = false;
        if (thread) {
            thread->deleteLater();
        }
        m_start->setEnabled(true);
        m_start->setText(tr("Export"));
        m_cameras->setEnabled(true);
    });
    m_start->setText(tr("Cancel"));
    m_cameras->setEnabled(false);
    m_thread->start();
}

void FusionExportDialog::showProgress(
    const quantiloom::dataset::FusionExportProgress& progress) {
    m_progress->setRange(0, static_cast<int>(progress.totalCameras));
    m_progress->setValue(static_cast<int>(progress.completedCameras));
    if (progress.phase == "publish") {
        m_start->setEnabled(false);
        m_status->setText(tr("Completing publication..."));
    } else {
        m_status->setText(
            QString::fromStdString(progress.cameraId + ": " + progress.phase));
    }
}

void FusionExportDialog::showResult(
    const quantiloom::Result<quantiloom::dataset::FusionExportResult,
                             quantiloom::String>& result) {
    m_status->setText(result
        ? tr("Saved %1").arg(QString::fromStdString(result.value().manifestPath))
        : QString::fromStdString(result.error()));
}

void FusionExportDialog::updatePreview() {
    if (m_loading || !preview || m_selected < 0 || m_thread) {
        return;
    }
    saveCamera();
    const auto rig = validatedRig();
    if (!rig) {
        return;
    }
    const RigCamera& camera = rig.value().cameras[static_cast<size_t>(m_selected)];
    if (auto config =
            quantiloom::dataset::RigCameraScene(m_scene, *rig, camera.id)) {
        preview(*config, camera.sensor);
    }
}

void FusionExportDialog::reject() {
    if (m_thread) {
        // Once the job has begun publication the write is transactional;
        // refuse to close rather than leave a half-written dataset.
        if (m_publishing) {
            return;
        }
        m_cancelled = true;
        m_status->setText(tr("Stopping before publication..."));
        return;
    }
    QDialog::reject();
}

void FusionExportDialog::changeEvent(QEvent* event) {
    QDialog::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
}

void FusionExportDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    updatePreview();
}

void FusionExportDialog::retranslateUi() {
    setWindowTitle(tr("Fusion Dataset Export"));
    m_sampleLabel->setText(tr("Sample ID:"));
    m_outputLabel->setText(tr("Output:"));
    m_browseButton->setText(tr("Browse..."));
    m_addButton->setText(tr("Add camera"));
    m_removeButton->setText(tr("Remove"));
    m_referenceLabel->setText(tr("Reference camera:"));
    m_pairMode->setItemText(0, tr("Default reference pairs"));
    m_pairMode->setItemText(1, tr("Explicit pairs"));
    m_pairs->setPlaceholderText(tr("source > target; source > target"));
    m_loadRigButton->setText(tr("Load rig..."));
    m_saveRigButton->setText(tr("Save rig..."));
    m_cameraIdLabel->setText(tr("Camera ID:"));
    for (int i = 0; i < 6; ++i) {
        m_poseLabels[i]->setText(tr(kPoseCaptions[i]));
    }
    m_previewButton->setText(tr("Preview selected camera"));
    m_rectify->setText(
        tr("Also export undistorted images and coordinate maps"));
    m_start->setText(m_thread ? tr("Cancel") : tr("Export"));
    m_closeButton->setText(tr("Close"));
}
