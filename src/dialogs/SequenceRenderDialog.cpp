/**
 * @file SequenceRenderDialog.cpp
 * @brief Sequence render implementation
 */

#include "SequenceRenderDialog.hpp"

#include "../ui/UiStyle.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QThread>
#include <QPointer>
#include <QMessageBox>
#include <QTextStream>

#include <core/Config.hpp>
#include <renderer/OfflineRenderer.hpp>

#include <exception>

SequenceRenderDialog::SequenceRenderDialog(const SceneConfig& config,
                                           QStringList materialNames, QWidget* parent)
    : QDialog(parent)
    , m_baseConfig(config)
    , m_materialNames(std::move(materialNames))
{
    setupUi();
    updatePreview();
}

SequenceRenderDialog::~SequenceRenderDialog() {
    // Same contract as the cube export: the worker owns nothing this dialog
    // outlives, and holds only a QPointer back to it.
    if (m_worker && m_worker->isRunning()) {
        m_cancelled = true;
        m_worker->wait();
    }
}

void SequenceRenderDialog::setupUi() {
    setWindowTitle(tr("Render Sequence"));
    setMinimumWidth(560);

    auto* mainLayout = new QVBoxLayout(this);

    auto* intro = new QLabel(
        tr("One scene, one frame per step. Each frame overrides a material's "
           "temperature and its own output file; everything else is the document "
           "as it stands."), this);
    intro->setWordWrap(true);
    uistyle::applyHintStyle(intro);
    mainLayout->addWidget(intro);

    // ------------------------------------------------------------------
    // What varies
    // ------------------------------------------------------------------
    auto* sweepGroup = new QGroupBox(tr("Sweep"), this);
    auto* sweepLayout = new QFormLayout(sweepGroup);

    m_materialCombo = new QComboBox();
    m_materialCombo->addItems(m_materialNames);
    connect(m_materialCombo, &QComboBox::currentTextChanged,
            this, &SequenceRenderDialog::onSweepChanged);
    sweepLayout->addRow(tr("Material:"), m_materialCombo);

    m_startTemp = new QDoubleSpinBox();
    m_startTemp->setRange(1.0, 2000.0);
    m_startTemp->setDecimals(1);
    m_startTemp->setSingleStep(5.0);
    m_startTemp->setSuffix(tr(" K"));
    m_startTemp->setValue(280.0);
    connect(m_startTemp, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SequenceRenderDialog::onSweepChanged);
    sweepLayout->addRow(tr("From:"), m_startTemp);

    m_endTemp = new QDoubleSpinBox();
    m_endTemp->setRange(1.0, 2000.0);
    m_endTemp->setDecimals(1);
    m_endTemp->setSingleStep(5.0);
    m_endTemp->setSuffix(tr(" K"));
    m_endTemp->setValue(340.0);
    connect(m_endTemp, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SequenceRenderDialog::onSweepChanged);
    sweepLayout->addRow(tr("To:"), m_endTemp);

    m_frameCount = new QSpinBox();
    m_frameCount->setRange(1, 999);
    m_frameCount->setValue(5);
    connect(m_frameCount, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SequenceRenderDialog::onSweepChanged);
    sweepLayout->addRow(tr("Frames:"), m_frameCount);

    m_spp = new QSpinBox();
    m_spp->setRange(1, 100000);
    m_spp->setValue(static_cast<int>(m_baseConfig.spp));
    m_spp->setToolTip(tr("Samples per frame. Every frame is traced to completion, "
                         "so this multiplies by the frame count."));
    sweepLayout->addRow(tr("Samples per frame:"), m_spp);

    mainLayout->addWidget(sweepGroup);

    // ------------------------------------------------------------------
    // Where it goes
    // ------------------------------------------------------------------
    auto* outputGroup = new QGroupBox(tr("Output"), this);
    auto* outputLayout = new QFormLayout(outputGroup);

    auto* dirRow = new QHBoxLayout();
    m_outputDirEdit = new QLineEdit(m_baseConfig.baseDir);
    auto* browse = new QPushButton(tr("..."));
    browse->setMaximumWidth(32);
    connect(browse, &QPushButton::clicked, this, &SequenceRenderDialog::onBrowseOutputDir);
    dirRow->addWidget(m_outputDirEdit);
    dirRow->addWidget(browse);
    outputLayout->addRow(tr("Directory:"), dirRow);

    m_nameTemplateEdit = new QLineEdit(QStringLiteral("frame_{index}_{temperature}K.exr"));
    m_nameTemplateEdit->setToolTip(
        tr("{index} is the frame number, zero padded; {temperature} is its "
           "temperature in kelvin, rounded."));
    connect(m_nameTemplateEdit, &QLineEdit::textChanged,
            this, &SequenceRenderDialog::onSweepChanged);
    outputLayout->addRow(tr("Name:"), m_nameTemplateEdit);

    mainLayout->addWidget(outputGroup);

    // ------------------------------------------------------------------
    // What that comes to
    // ------------------------------------------------------------------
    // Shown rather than described: a sweep is easy to get wrong by one frame
    // or one kelvin, and the list says exactly what will be written.
    m_previewEdit = new QPlainTextEdit(this);
    m_previewEdit->setReadOnly(true);
    m_previewEdit->setMaximumHeight(140);
    mainLayout->addWidget(m_previewEdit);

    m_statusLabel = new QLabel(this);
    uistyle::applyHintStyle(m_statusLabel);
    mainLayout->addWidget(m_statusLabel);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setVisible(false);
    mainLayout->addWidget(m_progress);

    auto* buttons = new QHBoxLayout();
    m_manifestButton = new QPushButton(tr("Export Manifest..."));
    m_manifestButton->setToolTip(
        tr("Write this sequence as a batch manifest the command line renders, "
           "for a run long enough to want a machine of its own."));
    connect(m_manifestButton, &QPushButton::clicked,
            this, &SequenceRenderDialog::onExportManifest);
    m_startButton = new QPushButton(tr("Render"));
    m_startButton->setDefault(true);
    connect(m_startButton, &QPushButton::clicked, this, &SequenceRenderDialog::onStartOrCancel);
    m_closeButton = new QPushButton(tr("Close"));
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);

    buttons->addWidget(m_manifestButton);
    buttons->addStretch();
    buttons->addWidget(m_startButton);
    buttons->addWidget(m_closeButton);
    mainLayout->addLayout(buttons);
}

double SequenceRenderDialog::frameTemperature(const int index) const {
    const int frames = m_frameCount->value();
    if (frames <= 1) {
        return m_startTemp->value();
    }
    const double t = static_cast<double>(index) / static_cast<double>(frames - 1);
    return m_startTemp->value() + t * (m_endTemp->value() - m_startTemp->value());
}

QString SequenceRenderDialog::frameOutputName(const int index) const {
    // Zero padded to the width of the last index, so the files sort in the
    // order they were rendered in every file browser there is.
    const int width = QString::number(m_frameCount->value()).size();
    QString name = m_nameTemplateEdit->text();
    if (name.isEmpty()) {
        name = QStringLiteral("frame_{index}.exr");
    }
    name.replace(QStringLiteral("{index}"),
                 QString::number(index + 1).rightJustified(width, QLatin1Char('0')));
    name.replace(QStringLiteral("{temperature}"),
                 QString::number(qRound(frameTemperature(index))));
    return name;
}

void SequenceRenderDialog::updatePreview() {
    const int frames = m_frameCount->value();
    QString text;
    QTextStream out(&text);

    const int shown = qMin(frames, 4);
    for (int i = 0; i < shown; ++i) {
        out << frameOutputName(i) << "   "
            << QString::number(frameTemperature(i), 'f', 1) << " K\n";
    }
    if (frames > shown + 1) {
        out << "...\n";
    }
    if (frames > shown) {
        out << frameOutputName(frames - 1) << "   "
            << QString::number(frameTemperature(frames - 1), 'f', 1) << " K\n";
    }
    m_previewEdit->setPlainText(text);

    if (m_materialNames.isEmpty()) {
        m_statusLabel->setText(tr("The open scene has no materials to sweep."));
        m_startButton->setEnabled(false);
        m_manifestButton->setEnabled(false);
    } else {
        m_statusLabel->setText(
            tr("%1 frames at %2 samples each.").arg(frames).arg(m_spp->value()));
    }
}

void SequenceRenderDialog::onSweepChanged() {
    updatePreview();
}

void SequenceRenderDialog::onBrowseOutputDir() {
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Sequence Output Directory"), m_outputDirEdit->text());
    if (!dir.isEmpty()) {
        m_outputDirEdit->setText(dir);
    }
}

QString SequenceRenderDialog::frameToml(const QString& baseToml, const int index) const {
    // The frame's own overrides, appended as TOML rather than merged here: the
    // core parses the whole document once, and a later table wins over an
    // earlier one for the same key. [material_overrides.<name>] rather than
    // [[materials]] because that is what survives being layered -- the same
    // reason the CLI's manifest uses it.
    const QString outputPath =
        QDir(m_outputDirEdit->text()).filePath(frameOutputName(index));

    QString toml = baseToml;
    QTextStream out(&toml, QIODevice::Append);
    out << "\n[material_overrides." << m_materialCombo->currentText() << "]\n"
        << "ir_temperature_k = " << QString::number(frameTemperature(index), 'f', 4) << "\n"
        << "\n[renderer]\n"
        << "output = \"" << QString(outputPath).replace(QLatin1Char('\\'), QLatin1String("/"))
        << "\"\n"
        << "spp = " << m_spp->value() << "\n";
    return toml;
}

void SequenceRenderDialog::onExportManifest() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Batch Manifest"),
        QDir(m_outputDirEdit->text()).filePath(QStringLiteral("sequence.txt")),
        tr("Manifest (*.txt);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }

    // The scene the manifest names has to be a file on disk, since the CLI
    // reads it rather than being handed a document.
    QString configPath = m_baseConfig.baseDir;
    const QString scenePath = m_baseConfig.gltfPath.isEmpty() ? m_baseConfig.usdPath
                                                              : m_baseConfig.gltfPath;
    Q_UNUSED(scenePath);
    configPath = QFileDialog::getOpenFileName(
        this, tr("Which configuration should the manifest name?"), m_baseConfig.baseDir,
        tr("Scene configuration (*.toml)"));
    if (configPath.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Failed"),
                             tr("Cannot write %1").arg(path));
        return;
    }

    const QDir manifestDir = QFileInfo(path).absoluteDir();
    QTextStream out(&file);
    out << "# Temperature sequence for " << m_materialCombo->currentText() << ": "
        << m_frameCount->value() << " frames, "
        << QString::number(m_startTemp->value(), 'f', 1) << " K to "
        << QString::number(m_endTemp->value(), 'f', 1) << " K.\n"
        << "#\n"
        << "# Render it with:  Quantiloom.exe batch " << QFileInfo(path).fileName() << "\n"
        << "\n";

    const QString relativeConfig = manifestDir.relativeFilePath(configPath);
    for (int i = 0; i < m_frameCount->value(); ++i) {
        const QString outputPath =
            manifestDir.relativeFilePath(QDir(m_outputDirEdit->text())
                                             .filePath(frameOutputName(i)));
        out << relativeConfig << " | material_overrides."
            << m_materialCombo->currentText() << ".ir_temperature_k="
            << QString::number(frameTemperature(i), 'f', 4)
            << " renderer.output=\"" << QString(outputPath).replace(QLatin1Char('\\'),
                                                                    QLatin1String("/"))
            << "\" renderer.spp=" << m_spp->value() << "\n";
    }

    m_statusLabel->setText(tr("Manifest written to %1").arg(path));
}

void SequenceRenderDialog::onStartOrCancel() {
    if (m_running) {
        // Between frames, not mid-trace: a frame abandoned halfway is a file
        // that exists and is wrong, which is worse than one that does not.
        m_cancelled = true;
        m_startButton->setEnabled(false);
        m_statusLabel->setText(tr("Stopping after this frame..."));
        return;
    }

    ConfigManager manager;
    const QString baseToml = manager.exportConfigToString(m_baseConfig);
    if (baseToml.isEmpty()) {
        QMessageBox::warning(this, tr("Sequence Failed"),
                             tr("The open document could not be written as TOML."));
        return;
    }

    QDir().mkpath(m_outputDirEdit->text());

    QStringList frames;
    const int frameCount = m_frameCount->value();
    frames.reserve(frameCount);
    for (int i = 0; i < frameCount; ++i) {
        frames << frameToml(baseToml, i);
    }

    m_running = true;
    m_cancelled = false;
    m_startButton->setText(tr("Stop"));
    m_manifestButton->setEnabled(false);
    m_closeButton->setEnabled(false);
    m_progress->setVisible(true);
    m_progress->setValue(0);

    QPointer<SequenceRenderDialog> self(this);
    m_worker = QThread::create([this, self, frames, frameCount] {
        QString error;
        int rendered = 0;

        for (int i = 0; i < frameCount; ++i) {
            if (m_cancelled) break;

            QMetaObject::invokeMethod(self, [self, i, frameCount] {
                if (!self) return;
                self->m_statusLabel->setText(
                    tr("Frame %1 of %2...").arg(i + 1).arg(frameCount));
                self->m_progress->setValue(i * 100 / frameCount);
            }, Qt::QueuedConnection);

            try {
                auto parsed = quantiloom::Config::Parse(frames.at(i).toStdString());
                if (!parsed.has_value()) {
                    error = tr("Frame %1 is not valid TOML: %2")
                                .arg(i + 1)
                                .arg(QString::fromStdString(parsed.error()));
                    break;
                }
                // A device per frame, as the cube export does: OfflineRenderer
                // owns its own, so nothing here touches the viewport's.
                auto renderer = quantiloom::OfflineRenderer::Create(
                    parsed.value(), quantiloom::OfflineRenderer::InitParams{});
                if (!renderer.has_value()) {
                    error = tr("Frame %1: %2").arg(i + 1)
                                .arg(QString::fromStdString(renderer.error()));
                    break;
                }
                const auto output = renderer.value()->Render();
                if (!output.error.empty()) {
                    error = tr("Frame %1: %2").arg(i + 1)
                                .arg(QString::fromStdString(output.error));
                    break;
                }
                ++rendered;
            } catch (const std::exception& e) {
                error = tr("Frame %1 failed: %2").arg(i + 1)
                            .arg(QString::fromLatin1(e.what()));
                break;
            }
        }

        const bool cancelled = m_cancelled;
        QMetaObject::invokeMethod(self, [self, error, rendered, frameCount, cancelled] {
            if (!self) return;
            if (!error.isEmpty()) {
                self->finish(false, error);
            } else if (cancelled) {
                self->finish(true, tr("Stopped after %1 of %2 frames.")
                                       .arg(rendered).arg(frameCount));
            } else {
                self->finish(true, tr("%1 frames written.").arg(rendered));
            }
        }, Qt::QueuedConnection);
    });
    connect(m_worker, &QThread::finished, m_worker, &QObject::deleteLater);
    m_worker->start();
}

void SequenceRenderDialog::finish(const bool success, const QString& message) {
    m_running = false;
    m_worker = nullptr;   // deleteLater is already connected
    m_startButton->setText(tr("Render"));
    m_startButton->setEnabled(true);
    m_manifestButton->setEnabled(true);
    m_closeButton->setEnabled(true);
    m_progress->setValue(success ? 100 : 0);
    m_progress->setVisible(success);
    m_statusLabel->setText(message);

    if (!success) {
        QMessageBox::warning(this, tr("Sequence Failed"), message);
    }
}
