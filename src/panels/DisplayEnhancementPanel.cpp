/**
 * @file DisplayEnhancementPanel.cpp
 * @brief Display enhancement controls implementation
 */

#include "DisplayEnhancementPanel.hpp"

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
    : QWidget(parent)
{
    setupUi();
}

void DisplayEnhancementPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // Enable checkbox
    m_enableCheckbox = new QCheckBox(tr("Enable Display Enhancement"));
    connect(m_enableCheckbox, &QCheckBox::stateChanged,
            this, &DisplayEnhancementPanel::onEnableChanged);
    mainLayout->addWidget(m_enableCheckbox);

    // Info label
    auto* infoLabel = new QLabel(
        tr("CLAHE enhances contrast for low-dynamic-range images "
           "(e.g. infrared). Only affects display and screenshots."));
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: gray; font-size: 9pt;");
    mainLayout->addWidget(infoLabel);

    // Settings group
    m_settingsGroup = new QGroupBox(tr("CLAHE Settings"));
    m_settingsGroup->setEnabled(false);
    auto* settingsLayout = new QFormLayout(m_settingsGroup);

    // Clip limit
    m_clipLimitSpin = new QDoubleSpinBox();
    m_clipLimitSpin->setRange(1.0, 100.0);
    m_clipLimitSpin->setSingleStep(0.5);
    m_clipLimitSpin->setValue(2.0);
    m_clipLimitSpin->setDecimals(1);
    m_clipLimitSpin->setToolTip(tr("Higher values allow more contrast enhancement.\n"
                                    "1.0 = no clipping (full equalization)\n"
                                    "2.0-4.0 = typical range for infrared"));
    connect(m_clipLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &DisplayEnhancementPanel::onClipLimitChanged);
    settingsLayout->addRow(tr("Clip Limit:"), m_clipLimitSpin);

    // Tile size
    m_tileSizeCombo = new QComboBox();
    m_tileSizeCombo->addItem(tr("4x4"), 4);
    m_tileSizeCombo->addItem(tr("8x8 (Default)"), 8);
    m_tileSizeCombo->addItem(tr("16x16"), 16);
    m_tileSizeCombo->addItem(tr("32x32"), 32);
    m_tileSizeCombo->setCurrentIndex(1);  // 8x8 default
    m_tileSizeCombo->setToolTip(tr("Number of contextual tiles.\n"
                                    "Smaller tiles = more local contrast.\n"
                                    "Larger tiles = more global contrast."));
    connect(m_tileSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DisplayEnhancementPanel::onTileSizeChanged);
    settingsLayout->addRow(tr("Tile Size:"), m_tileSizeCombo);

    // Processing mode
    auto* modeGroup = new QGroupBox(tr("Processing Mode"));
    auto* modeLayout = new QVBoxLayout(modeGroup);

    m_luminanceOnlyRadio = new QRadioButton(tr("Luminance Only (Recommended)"));
    m_luminanceOnlyRadio->setToolTip(tr("Apply CLAHE only to luminance channel,\n"
                                         "preserving color information."));
    m_luminanceOnlyRadio->setChecked(true);
    connect(m_luminanceOnlyRadio, &QRadioButton::toggled,
            this, &DisplayEnhancementPanel::onProcessingModeChanged);

    m_allChannelsRadio = new QRadioButton(tr("All Channels"));
    m_allChannelsRadio->setToolTip(tr("Apply CLAHE independently to each RGB channel.\n"
                                       "May cause color shifts."));

    auto* buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(m_luminanceOnlyRadio);
    buttonGroup->addButton(m_allChannelsRadio);

    modeLayout->addWidget(m_luminanceOnlyRadio);
    modeLayout->addWidget(m_allChannelsRadio);
    settingsLayout->addRow(modeGroup);

    mainLayout->addWidget(m_settingsGroup);
    mainLayout->addStretch();
}

void DisplayEnhancementPanel::setEnhancementEnabled(bool enabled) {
    m_enabled = enabled;
    m_enableCheckbox->blockSignals(true);
    m_enableCheckbox->setChecked(enabled);
    m_enableCheckbox->blockSignals(false);
    m_settingsGroup->setEnabled(enabled);
}

void DisplayEnhancementPanel::setClipLimit(float clipLimit) {
    m_clipLimit = clipLimit;
    m_clipLimitSpin->blockSignals(true);
    m_clipLimitSpin->setValue(clipLimit);
    m_clipLimitSpin->blockSignals(false);
}

void DisplayEnhancementPanel::setTileSize(int tileSize) {
    m_tileSize = tileSize;
    for (int i = 0; i < m_tileSizeCombo->count(); ++i) {
        if (m_tileSizeCombo->itemData(i).toInt() == tileSize) {
            m_tileSizeCombo->blockSignals(true);
            m_tileSizeCombo->setCurrentIndex(i);
            m_tileSizeCombo->blockSignals(false);
            break;
        }
    }
}

void DisplayEnhancementPanel::setLuminanceOnly(bool luminanceOnly) {
    m_luminanceOnly = luminanceOnly;
    m_luminanceOnlyRadio->blockSignals(true);
    m_allChannelsRadio->blockSignals(true);
    m_luminanceOnlyRadio->setChecked(luminanceOnly);
    m_allChannelsRadio->setChecked(!luminanceOnly);
    m_luminanceOnlyRadio->blockSignals(false);
    m_allChannelsRadio->blockSignals(false);
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
