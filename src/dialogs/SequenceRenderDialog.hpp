/**
 * @file SequenceRenderDialog.hpp
 * @brief Render the open scene many times, with one thing different each time
 *
 * A thermal scene is rarely interesting at one temperature. What a campaign
 * produces is a sequence -- the same street through a day, the same plate as
 * it cools -- and what a simulation has to produce to be compared with one is
 * the same thing: one scene, one frame per step, each frame differing by a
 * stated amount.
 *
 * The viewport cannot do this: it renders one state progressively, and a
 * sequence is many states each traced to completion. The SDK can, through the
 * same OfflineRenderer the cube export uses, so this drives it once per frame
 * on a worker thread.
 *
 * The overrides are layered with Config::MergedWith, exactly as the CLI's
 * batch manifest does -- and the dialog will write that manifest out instead
 * of rendering, for a sequence long enough to want a machine of its own.
 * [material_overrides.<name>] rather than [[materials]] for the same reason
 * the CLI uses it: merging replaces arrays whole, so an array override would
 * delete every material it does not name.
 *
 * @author blitzcolo
 */

#pragma once

#include <QDialog>
#include <QStringList>

#include "../config/ConfigManager.hpp"

QT_BEGIN_NAMESPACE
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;
class QLabel;
class QProgressBar;
class QPushButton;
class QThread;
class QPlainTextEdit;
QT_END_NAMESPACE

/**
 * @class SequenceRenderDialog
 * @brief What varies, over what range, in how many frames, and where it goes
 */
class SequenceRenderDialog : public QDialog {
    Q_OBJECT

public:
    /// @param config       the document as it stands, already collected from
    ///        the panels; the dialog overrides a copy and leaves this alone
    /// @param materialNames materials the open scene has, for the surface
    ///        whose temperature is being swept
    SequenceRenderDialog(const SceneConfig& config, QStringList materialNames,
                         QWidget* parent = nullptr);
    ~SequenceRenderDialog() override;

private slots:
    void onBrowseOutputDir();
    void onSweepChanged();
    void onStartOrCancel();
    void onExportManifest();

private:
    void setupUi();
    void updatePreview();
    void finish(bool success, const QString& message);

    /// The temperature of frame @p index, from the sweep as configured.
    [[nodiscard]] double frameTemperature(int index) const;
    /// Output file name for frame @p index, from the name template.
    [[nodiscard]] QString frameOutputName(int index) const;
    /// The TOML that renders frame @p index: the document plus this frame's
    /// own [material_overrides] and renderer.output.
    [[nodiscard]] QString frameToml(const QString& baseToml, int index) const;

    SceneConfig m_baseConfig;
    QStringList m_materialNames;

    QComboBox* m_materialCombo = nullptr;
    QDoubleSpinBox* m_startTemp = nullptr;
    QDoubleSpinBox* m_endTemp = nullptr;
    QSpinBox* m_frameCount = nullptr;
    QSpinBox* m_spp = nullptr;
    QLineEdit* m_outputDirEdit = nullptr;
    QLineEdit* m_nameTemplateEdit = nullptr;
    QPlainTextEdit* m_previewEdit = nullptr;
    QLabel* m_statusLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_startButton = nullptr;
    QPushButton* m_manifestButton = nullptr;
    QPushButton* m_closeButton = nullptr;

    QThread* m_worker = nullptr;
    bool m_running = false;
    /// Set from the GUI thread, read by the worker between frames: the only
    /// place a sequence can be stopped without abandoning a frame mid-trace.
    std::atomic<bool> m_cancelled{false};
};
