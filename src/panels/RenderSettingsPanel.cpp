/**
 * @file RenderSettingsPanel.cpp
 * @brief Render settings panel implementation
 */

#include "RenderSettingsPanel.hpp"

#include "../ui/ModeCatalog.hpp"
#include "../ui/UiStyle.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QFormLayout>

RenderSettingsPanel::RenderSettingsPanel(QWidget* parent)
    : PanelBase(parent)
{
    setObjectName(panelId());
    setupUi();
}

QString RenderSettingsPanel::panelTitle() const {
    return tr("Render");
}

void RenderSettingsPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // --- status ---------------------------------------------------------
    auto* statusGroup = new QGroupBox(this);
    auto* statusLayout = new QFormLayout(statusGroup);

    m_sampleCountLabel = new QLabel(QStringLiteral("0"));
    bindStyle([this] { uistyle::applyHeadingStyle(m_sampleCountLabel); });
    auto* samplesCaption = new QLabel(statusGroup);
    bindText([statusGroup, samplesCaption] {
        statusGroup->setTitle(tr("Status"));
        samplesCaption->setText(tr("Accumulated samples:"));
    });
    statusLayout->addRow(samplesCaption, m_sampleCountLabel);

    mainLayout->addWidget(statusGroup);

    // --- quality --------------------------------------------------------
    auto* qualityGroup = new QGroupBox(this);
    auto* qualityLayout = new QFormLayout(qualityGroup);

    // Preset names come from the same catalogue as Render ▸ Quality, so the
    // two lists cannot drift apart.
    m_sppPreset = new QComboBox();
    for (const catalog::QualityPreset& preset : catalog::qualityPresets()) {
        m_sppPreset->addItem(QString(), preset.spp);
    }
    m_sppPreset->addItem(QString(), -1);   // custom
    m_sppPreset->setCurrentIndex(1);       // Draft (4 spp)
    connect(m_sppPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RenderSettingsPanel::onSppPresetChanged);
    auto* targetCaption = new QLabel(qualityGroup);
    qualityLayout->addRow(targetCaption, m_sppPreset);

    m_customSpp = new QSpinBox();
    m_customSpp->setRange(1, 65536);
    m_customSpp->setValue(4);
    m_customSpp->setEnabled(false);
    connect(m_customSpp, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RenderSettingsPanel::onCustomSppChanged);
    auto* customCaption = new QLabel(qualityGroup);
    qualityLayout->addRow(customCaption, m_customSpp);

    // A "Progressive rendering" checkbox used to sit here. It was connected to
    // nothing and read by nothing -- accumulation over frames is how the
    // renderer works, not a mode. Removed rather than wired up, same reasoning
    // as the read-only resolution group below.

    bindText([this, qualityGroup, targetCaption, customCaption] {
        qualityGroup->setTitle(tr("Quality"));
        targetCaption->setText(tr("Target samples:"));
        customCaption->setText(tr("Custom samples:"));

        const auto presets = catalog::qualityPresets();
        for (int i = 0; i < presets.size() && i < m_sppPreset->count(); ++i) {
            m_sppPreset->setItemText(i, catalog::qualityPresetLabel(presets.at(i)));
        }
        m_sppPreset->setItemText(m_sppPreset->count() - 1, tr("Custom..."));
    });

    mainLayout->addWidget(qualityGroup);

    // --- resolution -----------------------------------------------------
    // Read-only on purpose: the viewport renders at the swapchain's size, so
    // the preset list that used to sit here could not take effect. Rather than
    // keep a control that silently does nothing, the panel reports the
    // resolution the renderer is actually using.
    auto* resGroup = new QGroupBox(this);
    auto* resLayout = new QFormLayout(resGroup);

    m_resolutionLabel = new QLabel(QStringLiteral("1280 x 720"));
    auto* resCaption = new QLabel(resGroup);
    resLayout->addRow(resCaption, m_resolutionLabel);

    auto* resNote = new QLabel(resGroup);
    bindStyle([resNote] { uistyle::applyHintStyle(resNote); });
    resLayout->addRow(resNote);

    bindText([resGroup, resCaption, resNote] {
        resGroup->setTitle(tr("Resolution"));
        resCaption->setText(tr("Render resolution:"));
        resNote->setText(tr("Follows the viewport size."));
    });

    mainLayout->addWidget(resGroup);

    // --- display enhancement (hosted) -----------------------------------
    m_displayGroup = new QGroupBox(this);
    m_displayLayout = new QVBoxLayout(m_displayGroup);
    m_displayLayout->setContentsMargins(0, 0, 0, 0);
    bindText([this] { m_displayGroup->setTitle(tr("Display Enhancement")); });
    m_displayGroup->setVisible(false);   // shown once a widget is installed
    mainLayout->addWidget(m_displayGroup);

    // --- actions --------------------------------------------------------
    auto* actionsGroup = new QGroupBox(this);
    auto* actionsLayout = new QVBoxLayout(actionsGroup);

    m_resetBtn = new QPushButton(actionsGroup);
    connect(m_resetBtn, &QPushButton::clicked, this, &RenderSettingsPanel::onResetClicked);
    actionsLayout->addWidget(m_resetBtn);

    m_exportBtn = new QPushButton(actionsGroup);
    connect(m_exportBtn, &QPushButton::clicked, this, &RenderSettingsPanel::exportRequested);
    actionsLayout->addWidget(m_exportBtn);

    // Cheap now that the renderer stops itself at the target: the shell has a
    // moment where the render is provably finished, which is the only place an
    // unattended export could be written from.
    m_autoExportCheck = new QCheckBox(actionsGroup);
    m_autoExportCheck->setChecked(false);
    actionsLayout->addWidget(m_autoExportCheck);

    bindText([this, actionsGroup] {
        actionsGroup->setTitle(tr("Actions"));
        m_resetBtn->setText(tr("Reset Accumulation"));
        m_resetBtn->setToolTip(tr("Clear accumulated samples and restart rendering"));
        m_exportBtn->setText(tr("Export Image..."));
        m_exportBtn->setToolTip(tr("Save the raw render, without display enhancement"));
        m_autoExportCheck->setText(tr("Export EXR when the target is reached"));
        m_autoExportCheck->setToolTip(
            tr("Writes beside the open configuration, named after it and the sample count. "
               "Has no effect in Infinite mode, which has no target to reach."));
    });

    mainLayout->addWidget(actionsGroup);
    mainLayout->addStretch();
}

bool RenderSettingsPanel::autoExportOnComplete() const {
    return m_autoExportCheck && m_autoExportCheck->isChecked();
}

void RenderSettingsPanel::retranslateUi() {
    PanelBase::retranslateUi();
    setResolution(m_width, m_height);
}

void RenderSettingsPanel::setDisplayEnhancementWidget(QWidget* widget) {
    if (!widget) {
        return;
    }
    m_displayLayout->addWidget(widget);
    m_displayGroup->setVisible(true);
}

void RenderSettingsPanel::setSampleCount(uint32_t count) {
    m_sampleCountLabel->setText(QString::number(count));
}

void RenderSettingsPanel::setTargetSPP(uint32_t spp) {
    m_targetSPP = spp;

    // Find matching preset (the last entry is "Custom...")
    for (int i = 0; i < m_sppPreset->count() - 1; ++i) {
        if (m_sppPreset->itemData(i).toUInt() == spp) {
            const QSignalBlocker blocker(m_sppPreset);
            m_sppPreset->setCurrentIndex(i);
            m_customSpp->setEnabled(false);
            return;
        }
    }

    {
        const QSignalBlocker blocker(m_sppPreset);
        m_sppPreset->setCurrentIndex(m_sppPreset->count() - 1);
    }
    m_customSpp->setEnabled(true);
    const QSignalBlocker blocker(m_customSpp);
    m_customSpp->setValue(static_cast<int>(spp));
}

void RenderSettingsPanel::setResolution(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    m_resolutionLabel->setText(tr("%1 x %2").arg(width).arg(height));
}

void RenderSettingsPanel::onSppPresetChanged(int index) {
    int spp = m_sppPreset->itemData(index).toInt();

    if (spp < 0) {
        m_customSpp->setEnabled(true);
        spp = m_customSpp->value();
    } else {
        m_customSpp->setEnabled(false);
        if (spp > 0) m_customSpp->setValue(spp);
    }

    m_targetSPP = static_cast<uint32_t>(spp);
    emit sppChanged(m_targetSPP);
}

void RenderSettingsPanel::onCustomSppChanged(int value) {
    if (m_customSpp->isEnabled()) {
        m_targetSPP = static_cast<uint32_t>(value);
        emit sppChanged(m_targetSPP);
    }
}

void RenderSettingsPanel::onResetClicked() {
    emit resetAccumulationRequested();
}
