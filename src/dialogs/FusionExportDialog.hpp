/**
 * @file FusionExportDialog.hpp
 * @brief Multi-camera rig editor and fusion dataset export
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

#pragma once

#include <QDialog>
#include <QPointer>
#include <QString>

#include <core/Config.hpp>
#include <dataset/FusionExportJob.hpp>
#include <postprocess/CameraPipeline.hpp>

#include <atomic>
#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QThread;
class QTimer;
class SensorPanel;

class FusionExportDialog : public QDialog {
    Q_OBJECT

public:
    FusionExportDialog(const quantiloom::Config& scene,
                       const QString& baseDirectory,
                       quantiloom::SpectralMode spectralMode,
                       QWidget* parent = nullptr);
    ~FusionExportDialog() override;

    /// Pushed a frozen preview scene and the selected camera's configuration
    /// whenever the edited camera changes. The host applies them to the
    /// viewport and owns restoring the document's own camera afterwards.
    std::function<void(const quantiloom::Config&,
                       const quantiloom::camera::CameraConfig&)> preview;

protected:
    void reject() override;
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void setupUi();
    void addCamera();
    void removeCamera();
    void loadRig();
    void saveRig();
    void browseOutput();

    /// Write the selected camera's widgets back into m_rig, propagating an id
    /// rename into the pair list, the pair text and the reference camera.
    void saveCamera();
    /// Pull the newly selected camera's stored state into the widgets.
    void selectCamera(int index);
    /// Refill the camera list and the reference combo from m_rig.
    void refreshList();
    /// Rebuild m_rig.pairs from the pair mode: every camera against the
    /// reference, or the explicit list. False when the list does not parse.
    bool collectPairs();
    /// Serialise the editor's rig and parse it back, which is what validates
    /// it -- ParseRigConfig is the only validator the SDK exports for a rig.
    [[nodiscard]] quantiloom::Result<quantiloom::dataset::RigConfig,
                                     quantiloom::String> validatedRig() const;
    /// The Export/Cancel button: starts a job, or asks the running one to
    /// stop before publication.
    void start();
    /// Push the selected camera to the host's preview callback. No-op while
    /// widgets are being loaded or a job is running.
    void updatePreview();
    /// GUI-thread side of the worker's progress callback.
    void showProgress(const quantiloom::dataset::FusionExportProgress& progress);
    /// GUI-thread side of the worker's completion.
    void showResult(const quantiloom::Result<quantiloom::dataset::FusionExportResult,
                                             quantiloom::String>& result);
    void retranslateUi();

    // Inputs the caller froze for the duration of the dialog.
    quantiloom::Config m_scene;
    QString m_baseDirectory;
    quantiloom::dataset::RigConfig m_rig;

    // Rig-wide controls.
    QLineEdit* m_sample = nullptr;
    QLineEdit* m_output = nullptr;
    QListWidget* m_cameras = nullptr;
    QComboBox* m_reference = nullptr;
    QComboBox* m_pairMode = nullptr;
    QLineEdit* m_pairs = nullptr;

    // Per-camera editor: id, the six pose fields and the sensor.
    QLineEdit* m_id = nullptr;
    QDoubleSpinBox* m_pose[6] = {};
    SensorPanel* m_sensor = nullptr;
    QCheckBox* m_rectify = nullptr;

    // Labels and buttons whose text retranslateUi() owns.
    QLabel* m_sampleLabel = nullptr;
    QLabel* m_outputLabel = nullptr;
    QLabel* m_referenceLabel = nullptr;
    QLabel* m_cameraIdLabel = nullptr;
    QLabel* m_poseLabels[6] = {};
    QPushButton* m_browseButton = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_removeButton = nullptr;
    QPushButton* m_loadRigButton = nullptr;
    QPushButton* m_saveRigButton = nullptr;
    QPushButton* m_previewButton = nullptr;

    // Run state.
    QProgressBar* m_progress = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_start = nullptr;
    QPushButton* m_closeButton = nullptr;
    QPointer<QThread> m_thread;
    QTimer* m_previewTimer = nullptr;
    std::atomic_bool m_cancelled = false;
    /// Set by the worker the moment the job enters its publish phase; close
    /// and cancel are refused from then on because publication is
    /// transactional.
    std::atomic_bool m_publishing = false;
    /// While true, widget signals must not write back into m_rig.
    bool m_loading = false;
    int m_selected = -1;
};
