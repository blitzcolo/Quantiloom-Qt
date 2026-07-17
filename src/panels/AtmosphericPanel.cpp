/**
 * @file AtmosphericPanel.cpp
 * @brief Panel for NN atmosphere (MODTRAN surrogate) configuration - Implementation
 *
 * @author wtflmao
 */

#include "AtmosphericPanel.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>

#include <cmath>

AtmosphericPanel::AtmosphericPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void AtmosphericPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);

    // Enable checkbox
    m_enabledCheck = new QCheckBox(tr("Enable NN Atmosphere"));
    m_enabledCheck->setChecked(false);
    mainLayout->addWidget(m_enabledCheck);

    // Preset selector
    auto* presetLayout = new QHBoxLayout();
    presetLayout->addWidget(new QLabel(tr("Preset:")));
    m_presetCombo = new QComboBox();
    m_presetCombo->addItem(tr("Disabled"), "disabled");
    m_presetCombo->addItem(tr("Clear"), "clear");
    m_presetCombo->addItem(tr("Turbulent Clear"), "turbulent_clear");
    m_presetCombo->addItem(tr("Urban Haze"), "urban_haze");
    m_presetCombo->addItem(tr("Fog"), "fog");
    m_presetCombo->addItem(tr("Light Rain"), "light_rain");
    m_presetCombo->addItem(tr("Heavy Rain"), "heavy_rain");
    m_presetCombo->addItem(tr("Snow"), "snow");
    m_presetCombo->addItem(tr("Haze"), "haze");
    presetLayout->addWidget(m_presetCombo, 1);
    mainLayout->addLayout(presetLayout);

    // Model pack directory
    auto* packLayout = new QHBoxLayout();
    packLayout->addWidget(new QLabel(tr("Model Pack:")));
    m_modelPackEdit = new QLineEdit();
    m_modelPackEdit->setPlaceholderText(tr("(auto-detect)"));
    m_modelPackEdit->setToolTip(
        tr("Directory of <band>_<geom>_<net>.safetensors files.\n"
           "Leave empty to auto-detect."));
    packLayout->addWidget(m_modelPackEdit, 1);
    m_modelPackBrowse = new QPushButton(tr("..."));
    m_modelPackBrowse->setFixedWidth(30);
    packLayout->addWidget(m_modelPackBrowse);
    mainLayout->addLayout(packLayout);

    // Advanced parameters group (collapsible)
    m_advancedGroup = new QGroupBox(tr("Weather Parameters"));
    m_advancedGroup->setCheckable(true);
    m_advancedGroup->setChecked(false);

    auto* advancedLayout = new QFormLayout(m_advancedGroup);

    // Discrete MODTRAN features
    m_atmosModelCombo = new QComboBox();
    m_atmosModelCombo->addItem(tr("Mid-Latitude Summer (2)"), 2.0);
    m_atmosModelCombo->addItem(tr("Mid-Latitude Winter (3)"), 3.0);
    advancedLayout->addRow(tr("Atmosphere Model:"), m_atmosModelCombo);

    m_ihazeCombo = new QComboBox();
    m_ihazeCombo->addItem(tr("Rural (1)"), 1.0);
    m_ihazeCombo->addItem(tr("Maritime (4)"), 4.0);
    m_ihazeCombo->addItem(tr("Urban (5)"), 5.0);
    m_ihazeCombo->addItem(tr("Advection Fog (9)"), 9.0);
    m_ihazeCombo->addItem(tr("Radiation Fog (10)"), 10.0);
    advancedLayout->addRow(tr("Aerosol (IHAZE):"), m_ihazeCombo);

    m_icldCombo = new QComboBox();
    m_icldCombo->addItem(tr("None (0)"), 0.0);
    m_icldCombo->addItem(tr("Rain Cloud (6)"), 6.0);
    m_icldCombo->addItem(tr("Cirrus (18)"), 18.0);
    advancedLayout->addRow(tr("Cloud (ICLD):"), m_icldCombo);

    // Continuous weather features (training domain ranges)
    m_visKm = new QDoubleSpinBox();
    m_visKm->setRange(0.5, 50.0);
    m_visKm->setDecimals(1);
    m_visKm->setSingleStep(0.5);
    m_visKm->setSuffix(" km");
    advancedLayout->addRow(tr("Visibility:"), m_visKm);

    m_rainrtMmH = new QDoubleSpinBox();
    m_rainrtMmH->setRange(0.0, 50.0);
    m_rainrtMmH->setDecimals(1);
    m_rainrtMmH->setSingleStep(0.5);
    m_rainrtMmH->setSuffix(" mm/h");
    advancedLayout->addRow(tr("Rain Rate:"), m_rainrtMmH);

    m_tGroundK = new QDoubleSpinBox();
    m_tGroundK->setRange(253.0, 328.0);
    m_tGroundK->setDecimals(2);
    m_tGroundK->setSingleStep(1.0);
    m_tGroundK->setSuffix(" K");
    advancedLayout->addRow(tr("Ground Temperature:"), m_tGroundK);

    m_rh = new QDoubleSpinBox();
    m_rh->setRange(0.05, 1.0);
    m_rh->setDecimals(2);
    m_rh->setSingleStep(0.05);
    advancedLayout->addRow(tr("Relative Humidity:"), m_rh);

    m_pHPa = new QDoubleSpinBox();
    m_pHPa->setRange(950.0, 1040.0);
    m_pHPa->setDecimals(2);
    m_pHPa->setSingleStep(1.0);
    m_pHPa->setSuffix(" hPa");
    advancedLayout->addRow(tr("Pressure:"), m_pHPa);

    m_h2oScale = new QDoubleSpinBox();
    m_h2oScale->setRange(0.5, 2.0);
    m_h2oScale->setDecimals(2);
    m_h2oScale->setSingleStep(0.05);
    advancedLayout->addRow(tr("H2O Scale:"), m_h2oScale);

    mainLayout->addWidget(m_advancedGroup);
    mainLayout->addStretch();

    // Connect signals
    connect(m_enabledCheck, &QCheckBox::toggled,
            this, &AtmosphericPanel::onEnabledChanged);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AtmosphericPanel::onPresetChanged);
    connect(m_modelPackBrowse, &QPushButton::clicked,
            this, &AtmosphericPanel::onBrowseModelPack);
    connect(m_modelPackEdit, &QLineEdit::editingFinished,
            this, &AtmosphericPanel::onAdvancedParamChanged);

    // Connect advanced params
    connect(m_atmosModelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AtmosphericPanel::onAdvancedParamChanged);
    connect(m_ihazeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AtmosphericPanel::onAdvancedParamChanged);
    connect(m_icldCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AtmosphericPanel::onAdvancedParamChanged);
    connect(m_visKm, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AtmosphericPanel::onAdvancedParamChanged);
    connect(m_rainrtMmH, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AtmosphericPanel::onAdvancedParamChanged);
    connect(m_tGroundK, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AtmosphericPanel::onAdvancedParamChanged);
    connect(m_rh, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AtmosphericPanel::onAdvancedParamChanged);
    connect(m_pHPa, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AtmosphericPanel::onAdvancedParamChanged);
    connect(m_h2oScale, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AtmosphericPanel::onAdvancedParamChanged);

    // Set default values (disabled)
    quantiloom::AtmosphereNNConfig defaultConfig;
    setAtmosphericConfig(defaultConfig);
}

void AtmosphericPanel::setPreset(const QString& preset) {
    QString lowerPreset = preset.toLower();

    for (int i = 0; i < m_presetCombo->count(); ++i) {
        if (m_presetCombo->itemData(i).toString() == lowerPreset) {
            m_updatingUi = true;
            m_presetCombo->setCurrentIndex(i);
            m_enabledCheck->setChecked(lowerPreset != "disabled");
            m_updatingUi = false;

            // Apply the preset config
            onPresetChanged(i);
            return;
        }
    }

    // Default to disabled
    m_presetCombo->setCurrentIndex(0);
}

QString AtmosphericPanel::preset() const {
    return m_presetCombo->currentData().toString();
}

void AtmosphericPanel::setAtmosphericConfig(const quantiloom::AtmosphereNNConfig& config) {
    m_config = config;
    m_updatingUi = true;

    m_enabledCheck->setChecked(config.enabled);
    m_modelPackEdit->setText(QString::fromStdString(config.modelPackDir));
    updateAdvancedParamsFromConfig(config);

    m_updatingUi = false;
}

quantiloom::AtmosphereNNConfig AtmosphericPanel::getAtmosphericConfig() const {
    return m_config;
}

void AtmosphericPanel::onPresetChanged(int index) {
    if (m_updatingUi) return;

    QString presetName = m_presetCombo->itemData(index).toString();

    // Preserve model pack dir across preset switches
    std::string packDir = m_modelPackEdit->text().trimmed().toStdString();

    quantiloom::AtmosphereNNConfig config;
    config.modelPackDir = packDir;
    if (config.ApplyPreset(presetName.toStdString())) {
        config.enabled = (presetName != "disabled");
    } else {
        config.enabled = false;
        config.preset = "disabled";
        presetName = "disabled";
    }
    m_config = config;

    // Update enabled checkbox
    m_updatingUi = true;
    m_enabledCheck->setChecked(m_config.enabled);
    updateAdvancedParamsFromConfig(m_config);
    m_updatingUi = false;

    emit presetChanged(presetName);
    emit configChanged(m_config);
}

void AtmosphericPanel::onAdvancedParamChanged() {
    if (m_updatingUi) return;

    m_config.modelPackDir = m_modelPackEdit->text().trimmed().toStdString();

    m_config.atmosModel = m_atmosModelCombo->currentData().toDouble();
    m_config.ihaze = m_ihazeCombo->currentData().toDouble();
    m_config.icld = m_icldCombo->currentData().toDouble();

    m_config.visKm = m_visKm->value();
    m_config.rainrtMmH = m_rainrtMmH->value();
    m_config.tGroundK = m_tGroundK->value();
    m_config.rh = m_rh->value();
    m_config.pHPa = m_pHPa->value();
    m_config.h2oScale = m_h2oScale->value();

    emit configChanged(m_config);
}

void AtmosphericPanel::onEnabledChanged(bool enabled) {
    if (m_updatingUi) return;

    if (!enabled) {
        // Switch to disabled preset
        m_updatingUi = true;
        m_presetCombo->setCurrentIndex(0);  // Disabled
        m_updatingUi = false;

        m_config.enabled = false;
        m_config.preset = "disabled";

        emit presetChanged("disabled");
        emit configChanged(m_config);
    } else {
        // Switch to Clear as default enabled preset
        m_updatingUi = true;
        m_presetCombo->setCurrentIndex(1);  // Clear
        m_updatingUi = false;

        std::string packDir = m_modelPackEdit->text().trimmed().toStdString();
        quantiloom::AtmosphereNNConfig config;
        config.modelPackDir = packDir;
        config.ApplyPreset("clear");
        config.enabled = true;
        m_config = config;

        m_updatingUi = true;
        updateAdvancedParamsFromConfig(m_config);
        m_updatingUi = false;

        emit presetChanged("clear");
        emit configChanged(m_config);
    }
}

void AtmosphericPanel::onBrowseModelPack() {
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Atmosphere Model Pack Directory"),
        m_modelPackEdit->text());
    if (!dir.isEmpty()) {
        m_modelPackEdit->setText(dir);
        onAdvancedParamChanged();
    }
}

void AtmosphericPanel::selectComboData(QComboBox* combo, double value) {
    int bestIndex = 0;
    double bestDist = 1e30;
    for (int i = 0; i < combo->count(); ++i) {
        double dist = std::abs(combo->itemData(i).toDouble() - value);
        if (dist < bestDist) {
            bestDist = dist;
            bestIndex = i;
        }
    }
    combo->setCurrentIndex(bestIndex);
}

void AtmosphericPanel::updateAdvancedParamsFromConfig(const quantiloom::AtmosphereNNConfig& config) {
    blockSignalsForUpdate(true);

    selectComboData(m_atmosModelCombo, config.atmosModel);
    selectComboData(m_ihazeCombo, config.ihaze);
    selectComboData(m_icldCombo, config.icld);

    m_visKm->setValue(config.visKm);
    m_rainrtMmH->setValue(config.rainrtMmH);
    m_tGroundK->setValue(config.tGroundK);
    m_rh->setValue(config.rh);
    m_pHPa->setValue(config.pHPa);
    m_h2oScale->setValue(config.h2oScale);

    blockSignalsForUpdate(false);
}

void AtmosphericPanel::blockSignalsForUpdate(bool block) {
    m_atmosModelCombo->blockSignals(block);
    m_ihazeCombo->blockSignals(block);
    m_icldCombo->blockSignals(block);
    m_visKm->blockSignals(block);
    m_rainrtMmH->blockSignals(block);
    m_tGroundK->blockSignals(block);
    m_rh->blockSignals(block);
    m_pHPa->blockSignals(block);
    m_h2oScale->blockSignals(block);
}
