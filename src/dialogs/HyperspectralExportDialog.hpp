/**
 * @file HyperspectralExportDialog.hpp
 * @brief Render the open scene as a hyperspectral cube, offline
 *
 * The viewport cannot show a cube: it renders one band at a time
 * progressively, and a cube is a hundred bands traced to completion and
 * streamed to disk. ModeCatalog leaves Multispectral out of the mode list for
 * that reason, which left the capability reachable only from the CLI even
 * though the SDK exports everything needed.
 *
 * OfflineRenderer creates its own device, so this runs beside the viewport
 * rather than through it, on a worker thread.
 *
 * @author blitzcolo
 */

#pragma once

#include <QDialog>

#include "../config/ConfigManager.hpp"
#include "../ui/UiStyle.hpp"

#include <functional>
#include <vector>

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QLineEdit;
class QLabel;
class QProgressBar;
class QPushButton;
class QThread;
class QGroupBox;
QT_END_NAMESPACE

/**
 * @class HyperspectralExportDialog
 * @brief Range, format and destination for an offline cube render
 */
class HyperspectralExportDialog : public QDialog {
    Q_OBJECT

public:
    /// @param config The document as it stands, already collected from the
    ///        panels. The dialog overrides the spectral mode and the
    ///        [hyperspectral] block on a copy, and leaves the caller's alone.
    explicit HyperspectralExportDialog(const SceneConfig& config, QWidget* parent = nullptr);
    ~HyperspectralExportDialog() override;

    /// True while the cube is being traced. The shell asks before it closes.
    [[nodiscard]] bool isRunning() const { return m_running; }

    /// The document as it stands now. The dialog is modeless, so the scene
    /// can be edited while it is open; Render reads the document through this
    /// rather than rendering the copy it was opened with.
    std::function<SceneConfig()> documentProvider;

public slots:
    /// Refused while a cube is being traced: the core has no cancel.
    void reject() override;

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onBrowseOutput();
    void onStartOrCancel();
    void onRangeChanged();

private:
    void setupUi();
    void retranslateUi();
    void setInputsEnabled(bool enabled);
    void updateBandCount();
    /// Build the TOML the offline renderer will parse: the open document with
    /// multispectral forced on and this dialog's [hyperspectral] block.
    [[nodiscard]] SceneConfig exportConfig() const;
    void finish(bool success, const QString& message);

    SceneConfig m_baseConfig;

    struct Caption {
        QLabel* label;
        const char* text;   // QT_TR_NOOP source, translated in retranslateUi()
    };
    std::vector<Caption> m_captions;
    uistyle::StyleBindings m_styling;
    QLabel* m_introLabel = nullptr;
    QGroupBox* m_rangeGroup = nullptr;
    QGroupBox* m_outputGroup = nullptr;
    QPushButton* m_browseButton = nullptr;

    QDoubleSpinBox* m_lambdaMin = nullptr;
    QDoubleSpinBox* m_lambdaMax = nullptr;
    QDoubleSpinBox* m_lambdaStep = nullptr;
    QSpinBox* m_spp = nullptr;
    QComboBox* m_format = nullptr;
    QCheckBox* m_saveIntermediates = nullptr;
    QLineEdit* m_outputEdit = nullptr;
    QLabel* m_bandCountLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_startButton = nullptr;
    QPushButton* m_closeButton = nullptr;

    QThread* m_worker = nullptr;
    bool m_running = false;
};
