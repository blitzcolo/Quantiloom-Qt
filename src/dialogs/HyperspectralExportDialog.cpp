/**
 * @file HyperspectralExportDialog.cpp
 * @brief Offline hyperspectral cube export implementation
 */

#include "HyperspectralExportDialog.hpp"

#include "../ui/UiStyle.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QProgressBar>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QThread>
#include <QPointer>
#include <QMessageBox>

#include <core/Config.hpp>
#include <renderer/OfflineRenderer.hpp>

#include <cmath>
#include <exception>

namespace {

/// The five formats the core's cube writer accepts, as protocol strings.
struct FormatOption {
    const char* id;
    const char* label;
};
const FormatOption kFormats[] = {
    {"envi_bsq", QT_TRANSLATE_NOOP("HyperspectralExportDialog", "ENVI BSQ (band sequential)")},
    {"envi_bil", QT_TRANSLATE_NOOP("HyperspectralExportDialog", "ENVI BIL (band interleaved by line)")},
    {"envi_bip", QT_TRANSLATE_NOOP("HyperspectralExportDialog", "ENVI BIP (band interleaved by pixel)")},
    {"geotiff",  QT_TRANSLATE_NOOP("HyperspectralExportDialog", "GeoTIFF")},
    {"exr_spectral", QT_TRANSLATE_NOOP("HyperspectralExportDialog", "OpenEXR (spectral, one channel per band)")},
};

}  // namespace

HyperspectralExportDialog::HyperspectralExportDialog(const SceneConfig& config, QWidget* parent)
    : QDialog(parent)
    , m_baseConfig(config)
{
    setupUi();
    updateBandCount();
}

HyperspectralExportDialog::~HyperspectralExportDialog() {
    // The worker owns nothing this dialog outlives; it is only ever waited on
    // in finish(), which runs before the dialog can be destroyed while running
    // (the close button is disabled meanwhile).
    if (m_worker) {
        m_worker->quit();
        m_worker->wait();
    }
}

void HyperspectralExportDialog::setupUi() {
    setWindowTitle(tr("Render Hyperspectral Cube"));
    setModal(true);
    resize(520, 400);

    auto* mainLayout = new QVBoxLayout(this);

    auto* intro = new QLabel(
        tr("Traces every band to completion and streams the result to disk. This is "
           "an offline render on a device of its own — the viewport keeps working, "
           "and the two share the GPU."), this);
    intro->setWordWrap(true);
    uistyle::applyHintStyle(intro);
    mainLayout->addWidget(intro);

    // --- range ----------------------------------------------------------
    auto* rangeGroup = new QGroupBox(tr("Wavelength Range"), this);
    auto* rangeLayout = new QFormLayout(rangeGroup);

    const auto seed = m_baseConfig.hyperspectral.value_or(HyperspectralConfig{});

    m_lambdaMin = new QDoubleSpinBox();
    m_lambdaMin->setRange(100.0, 20000.0);
    m_lambdaMin->setSuffix(tr(" nm"));
    m_lambdaMin->setValue(seed.wavelengthMin_nm);
    connect(m_lambdaMin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &HyperspectralExportDialog::onRangeChanged);
    rangeLayout->addRow(tr("From:"), m_lambdaMin);

    m_lambdaMax = new QDoubleSpinBox();
    m_lambdaMax->setRange(100.0, 20000.0);
    m_lambdaMax->setSuffix(tr(" nm"));
    m_lambdaMax->setValue(seed.wavelengthMax_nm);
    connect(m_lambdaMax, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &HyperspectralExportDialog::onRangeChanged);
    rangeLayout->addRow(tr("To:"), m_lambdaMax);

    m_lambdaStep = new QDoubleSpinBox();
    m_lambdaStep->setRange(0.1, 500.0);
    m_lambdaStep->setSuffix(tr(" nm"));
    m_lambdaStep->setValue(seed.wavelengthStep_nm);
    connect(m_lambdaStep, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &HyperspectralExportDialog::onRangeChanged);
    rangeLayout->addRow(tr("Step:"), m_lambdaStep);

    m_bandCountLabel = new QLabel();
    uistyle::applyHintStyle(m_bandCountLabel);
    rangeLayout->addRow(m_bandCountLabel);
    mainLayout->addWidget(rangeGroup);

    // --- output ---------------------------------------------------------
    auto* outputGroup = new QGroupBox(tr("Output"), this);
    auto* outputLayout = new QFormLayout(outputGroup);

    m_spp = new QSpinBox();
    m_spp->setRange(1, 65536);
    m_spp->setValue(static_cast<int>(m_baseConfig.spp > 0 ? m_baseConfig.spp : 16));
    m_spp->setToolTip(tr("Samples per pixel, per band. Every band is traced to this "
                         "count, so the total cost scales with the band count too."));
    outputLayout->addRow(tr("Samples per band:"), m_spp);

    m_format = new QComboBox();
    for (const FormatOption& format : kFormats) {
        m_format->addItem(tr(format.label), QString::fromLatin1(format.id));
    }
    if (const int index = m_format->findData(seed.outputFormat); index >= 0) {
        m_format->setCurrentIndex(index);
    }
    outputLayout->addRow(tr("Format:"), m_format);

    auto* pathRow = new QHBoxLayout();
    m_outputEdit = new QLineEdit();
    // Beside the document, named after it: an export should not have to be
    // told where to go when there is an obvious answer.
    m_outputEdit->setText(m_baseConfig.outputPath.isEmpty()
                              ? QStringLiteral("hyperspectral_output")
                              : QFileInfo(m_baseConfig.outputPath).completeBaseName());
    pathRow->addWidget(m_outputEdit, 1);
    auto* browseButton = new QPushButton(tr("Browse..."));
    connect(browseButton, &QPushButton::clicked,
            this, &HyperspectralExportDialog::onBrowseOutput);
    pathRow->addWidget(browseButton);
    outputLayout->addRow(tr("Base name:"), pathRow);

    m_saveIntermediates = new QCheckBox(tr("Also save each band as EXR"));
    m_saveIntermediates->setChecked(seed.saveIntermediates);
    m_saveIntermediates->setToolTip(
        tr("Writes one image per band beside the cube, for inspecting a single "
           "wavelength. Multiplies the output size by the band count."));
    outputLayout->addRow(m_saveIntermediates);
    mainLayout->addWidget(outputGroup);

    // --- progress -------------------------------------------------------
    m_progress = new QProgressBar();
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setVisible(false);
    mainLayout->addWidget(m_progress);

    m_statusLabel = new QLabel();
    m_statusLabel->setWordWrap(true);
    uistyle::applyHintStyle(m_statusLabel);
    mainLayout->addWidget(m_statusLabel);

    mainLayout->addStretch();

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    m_startButton = new QPushButton(tr("Render"));
    m_startButton->setDefault(true);
    connect(m_startButton, &QPushButton::clicked,
            this, &HyperspectralExportDialog::onStartOrCancel);
    buttons->addWidget(m_startButton);
    m_closeButton = new QPushButton(tr("Close"));
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(m_closeButton);
    mainLayout->addLayout(buttons);
}

void HyperspectralExportDialog::onRangeChanged() {
    // Bound each other, so an inverted range cannot be submitted.
    if (m_lambdaMin->value() > m_lambdaMax->value()) {
        const QSignalBlocker blocker(m_lambdaMax);
        m_lambdaMax->setValue(m_lambdaMin->value());
    }
    updateBandCount();
}

void HyperspectralExportDialog::updateBandCount() {
    const double span = m_lambdaMax->value() - m_lambdaMin->value();
    const double step = m_lambdaStep->value();
    const int bands = (step > 0.0) ? static_cast<int>(std::floor(span / step)) + 1 : 0;
    m_bandCountLabel->setText(tr("%n band(s) — each traced to completion", "", bands));
    m_startButton->setEnabled(bands > 1 && !m_running);
}

SceneConfig HyperspectralExportDialog::exportConfig() const {
    SceneConfig config = m_baseConfig;
    // The mode the viewport cannot show. Everything else about the document --
    // scene, camera, lighting, materials -- is carried through unchanged, so
    // the cube is of what is on screen.
    config.spectralMode = quantiloom::SpectralMode::Multispectral;
    config.spp = static_cast<uint32_t>(m_spp->value());
    config.outputPath = m_outputEdit->text();

    HyperspectralConfig hs;
    hs.wavelengthMin_nm = static_cast<float>(m_lambdaMin->value());
    hs.wavelengthMax_nm = static_cast<float>(m_lambdaMax->value());
    hs.wavelengthStep_nm = static_cast<float>(m_lambdaStep->value());
    hs.outputFormat = m_format->currentData().toString();
    hs.saveIntermediates = m_saveIntermediates->isChecked();
    hs.useGpuReconstruction = true;
    config.hyperspectral = hs;
    return config;
}

void HyperspectralExportDialog::onBrowseOutput() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Cube Output Base Name"), m_outputEdit->text(),
        tr("All Files (*)"));
    if (!path.isEmpty()) {
        // The core appends its own extensions (.hdr and .dat for ENVI), so
        // what it wants is a base name.
        const QFileInfo info(path);
        m_outputEdit->setText(info.dir().filePath(info.completeBaseName()));
    }
}

void HyperspectralExportDialog::onStartOrCancel() {
    if (m_running) {
        return;   // A cube cannot be interrupted; the core has no cancel.
    }

    // Serialised here rather than passed as a struct: the offline renderer
    // parses a Config, which is the same reading the CLI does, so the cube
    // cannot mean something different from the same document rendered there.
    ConfigManager manager;
    const QString toml = manager.exportConfigToString(exportConfig());
    if (toml.isEmpty()) {
        finish(false, tr("Could not serialise the current document."));
        return;
    }

    m_running = true;
    m_startButton->setEnabled(false);
    m_closeButton->setEnabled(false);
    m_progress->setVisible(true);
    m_progress->setValue(0);
    m_statusLabel->setText(tr("Preparing — compiling shaders and loading the scene..."));

    // A worker thread because Render() blocks for minutes. OfflineRenderer
    // creates its own device, so nothing here touches the viewport's.
    const QString baseDir = m_baseConfig.baseDir;
    QPointer<HyperspectralExportDialog> self(this);
    m_worker = QThread::create([this, self, toml, baseDir] {
        QString error;
        bool ok = false;
        try {
            auto parsed = quantiloom::Config::Parse(toml.toStdString());
            if (!parsed.has_value()) {
                error = tr("The document is not valid TOML: %1")
                            .arg(QString::fromStdString(parsed.error()));
            } else {
                quantiloom::OfflineRenderer::InitParams params;
                params.onProgress = [self](const quantiloom::OfflineProgress& progress) {
                    // Queued onto the GUI thread: this runs between bands on
                    // the worker.
                    const int percent = static_cast<int>(progress.GetPercentage());
                    const float remaining = progress.GetRemainingSeconds();
                    const quantiloom::u32 band = progress.currentBand;
                    const quantiloom::u32 total = progress.totalBands;
                    QMetaObject::invokeMethod(
                        self, [self, percent, remaining, band, total] {
                            if (!self) return;
                            self->m_progress->setValue(percent);
                            self->m_statusLabel->setText(
                                tr("Band %1 of %2 — about %3 s remaining")
                                    .arg(band).arg(total)
                                    .arg(static_cast<int>(remaining)));
                        }, Qt::QueuedConnection);
                };

                auto renderer = quantiloom::OfflineRenderer::Create(parsed.value(), params);
                if (!renderer.has_value()) {
                    error = QString::fromStdString(renderer.error());
                } else {
                    const auto output = renderer.value()->Render();
                    if (!output.error.empty()) {
                        error = QString::fromStdString(output.error);
                    } else {
                        ok = true;
                    }
                }
            }
        } catch (const std::exception& e) {
            // Render() throws on device loss or a driver timeout, which is
            // exactly what a long trace on a shared GPU can hit.
            error = tr("The render failed: %1").arg(QString::fromLatin1(e.what()));
        }

        QMetaObject::invokeMethod(self, [self, ok, error] {
            if (!self) return;
            self->finish(ok, ok ? tr("Cube written.") : error);
        }, Qt::QueuedConnection);
    });
    connect(m_worker, &QThread::finished, m_worker, &QObject::deleteLater);
    m_worker->start();
}

void HyperspectralExportDialog::finish(bool success, const QString& message) {
    m_running = false;
    m_worker = nullptr;   // deleteLater is already connected
    m_startButton->setEnabled(true);
    m_closeButton->setEnabled(true);
    m_progress->setValue(success ? 100 : 0);
    m_progress->setVisible(success);
    m_statusLabel->setText(message);

    if (!success) {
        QMessageBox::warning(this, tr("Cube Render Failed"), message);
    }
}
