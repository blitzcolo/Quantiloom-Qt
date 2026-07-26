/**
 * @file DisplayEnhancementPanel.cpp
 * @brief Display enhancement controls implementation
 */

#include "DisplayEnhancementPanel.hpp"

#include "../ui/UiStyle.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFormLayout>

DisplayEnhancementPanel::DisplayEnhancementPanel(QWidget* parent)
    : PanelBase(parent)
{
    setObjectName(panelId());
    setupUi();
}

QString DisplayEnhancementPanel::panelTitle() const {
    return tr("Display Enhancement");
}

void DisplayEnhancementPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    m_enableCheckbox = new QCheckBox(this);
    bindText([this] { m_enableCheckbox->setText(tr("Enable display enhancement")); });
    connect(m_enableCheckbox, &QCheckBox::checkStateChanged,
            this, [this](Qt::CheckState state) { onEnableChanged(static_cast<int>(state)); });
    mainLayout->addWidget(m_enableCheckbox);

    m_infoLabel = new QLabel(this);
    bindStyle([this] { uistyle::applyHintStyle(m_infoLabel); });
    bindText([this] {
        // Spelling out the scope here and in the export menu entries is the
        // whole point: users could not tell which of the two images they were
        // getting.
        m_infoLabel->setText(tr(
            "CLAHE lifts contrast in low-dynamic-range images such as infrared. "
            "It changes the viewport and saved screenshots; exported images keep "
            "their raw values."));
    });
    mainLayout->addWidget(m_infoLabel);

    m_settingsGroup = new QGroupBox(this);
    bindText([this] { m_settingsGroup->setTitle(tr("CLAHE Settings")); });
    m_settingsGroup->setEnabled(false);
    auto* settingsLayout = new QFormLayout(m_settingsGroup);

    m_clipLimitSpin = new QDoubleSpinBox();
    m_clipLimitSpin->setRange(1.0, 100.0);
    m_clipLimitSpin->setSingleStep(0.5);
    m_clipLimitSpin->setValue(2.0);
    m_clipLimitSpin->setDecimals(1);
    connect(m_clipLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &DisplayEnhancementPanel::onClipLimitChanged);
    auto* clipCaption = new QLabel(m_settingsGroup);
    bindText([this, clipCaption] {
        clipCaption->setText(tr("Clip limit:"));
        m_clipLimitSpin->setToolTip(tr("Higher values allow more contrast enhancement.\n"
                                       "1.0 = no clipping (full equalization)\n"
                                       "2.0-4.0 = typical range for infrared"));
    });
    settingsLayout->addRow(clipCaption, m_clipLimitSpin);

    m_tileSizeCombo = new QComboBox();
    m_tileSizeCombo->addItem(QString(), 4);
    m_tileSizeCombo->addItem(QString(), 8);
    m_tileSizeCombo->addItem(QString(), 16);
    m_tileSizeCombo->addItem(QString(), 32);
    m_tileSizeCombo->setCurrentIndex(1);  // 8x8 default
    connect(m_tileSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DisplayEnhancementPanel::onTileSizeChanged);
    auto* tileCaption = new QLabel(m_settingsGroup);
    bindText([this, tileCaption] {
        tileCaption->setText(tr("Tile size:"));
        m_tileSizeCombo->setItemText(0, tr("4x4"));
        m_tileSizeCombo->setItemText(1, tr("8x8 (default)"));
        m_tileSizeCombo->setItemText(2, tr("16x16"));
        m_tileSizeCombo->setItemText(3, tr("32x32"));
        m_tileSizeCombo->setToolTip(tr("Number of contextual tiles.\n"
                                       "Smaller tiles = more local contrast.\n"
                                       "Larger tiles = more global contrast."));
    });
    settingsLayout->addRow(tileCaption, m_tileSizeCombo);

    auto* modeGroup = new QGroupBox(m_settingsGroup);
    auto* modeLayout = new QVBoxLayout(modeGroup);

    m_luminanceOnlyRadio = new QRadioButton(modeGroup);
    m_luminanceOnlyRadio->setChecked(true);
    connect(m_luminanceOnlyRadio, &QRadioButton::toggled,
            this, &DisplayEnhancementPanel::onProcessingModeChanged);

    m_allChannelsRadio = new QRadioButton(modeGroup);

    bindText([this, modeGroup] {
        modeGroup->setTitle(tr("Processing mode"));
        m_luminanceOnlyRadio->setText(tr("Luminance only (recommended)"));
        m_luminanceOnlyRadio->setToolTip(tr("Apply CLAHE only to the luminance channel,\n"
                                            "preserving colour information."));
        m_allChannelsRadio->setText(tr("All channels"));
        m_allChannelsRadio->setToolTip(tr("Apply CLAHE independently to each RGB channel.\n"
                                          "May cause colour shifts."));
    });

    auto* buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(m_luminanceOnlyRadio);
    buttonGroup->addButton(m_allChannelsRadio);

    modeLayout->addWidget(m_luminanceOnlyRadio);
    modeLayout->addWidget(m_allChannelsRadio);
    settingsLayout->addRow(modeGroup);

    mainLayout->addWidget(m_settingsGroup);
    mainLayout->addStretch();
}

void DisplayEnhancementPanel::retranslateUi() {
    PanelBase::retranslateUi();
}

void DisplayEnhancementPanel::setEnhancementEnabled(bool enabled) {
    m_enabled = enabled;
    const QSignalBlocker blocker(m_enableCheckbox);
    m_enableCheckbox->setChecked(enabled);
    m_settingsGroup->setEnabled(enabled);
}

void DisplayEnhancementPanel::requestEnhancementEnabled(bool enabled) {
    if (m_enabled == enabled) {
        return;
    }
    setEnhancementEnabled(enabled);
    emitSettings();
}

void DisplayEnhancementPanel::setClipLimit(float clipLimit) {
    m_clipLimit = clipLimit;
    const QSignalBlocker blocker(m_clipLimitSpin);
    m_clipLimitSpin->setValue(clipLimit);
}

void DisplayEnhancementPanel::setTileSize(int tileSize) {
    m_tileSize = tileSize;
    for (int i = 0; i < m_tileSizeCombo->count(); ++i) {
        if (m_tileSizeCombo->itemData(i).toInt() == tileSize) {
            const QSignalBlocker blocker(m_tileSizeCombo);
            m_tileSizeCombo->setCurrentIndex(i);
            break;
        }
    }
}

void DisplayEnhancementPanel::setLuminanceOnly(bool luminanceOnly) {
    m_luminanceOnly = luminanceOnly;
    const QSignalBlocker lum(m_luminanceOnlyRadio);
    const QSignalBlocker all(m_allChannelsRadio);
    m_luminanceOnlyRadio->setChecked(luminanceOnly);
    m_allChannelsRadio->setChecked(!luminanceOnly);
}

void DisplayEnhancementPanel::onEnableChanged(int state) {
    m_enabled = (state == Qt::Checked);
    m_settingsGroup->setEnabled(m_enabled);
    emitSettings();
}

void DisplayEnhancementPanel::onClipLimitChanged(double value) {
    m_clipLimit = static_cast<float>(value);
    emitSettings();
}

void DisplayEnhancementPanel::onTileSizeChanged(int index) {
    m_tileSize = m_tileSizeCombo->itemData(index).toInt();
    emitSettings();
}

void DisplayEnhancementPanel::onProcessingModeChanged() {
    m_luminanceOnly = m_luminanceOnlyRadio->isChecked();
    emitSettings();
}

void DisplayEnhancementPanel::emitSettings() {
    emit enhancementChanged(m_enabled, m_clipLimit, m_tileSize, m_luminanceOnly);
}
