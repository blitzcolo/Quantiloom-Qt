/**
 * @file AtmosphericPanel.cpp
 * @brief Combined analytic + NN atmosphere panel — implementation
 *
 * @author blitzccolo
 */

#include "AtmosphericPanel.hpp"

#include "../ui/CollapsibleGroupBox.hpp"
#include "../ui/UiStyle.hpp"

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
#include <QSlider>
#include <QFileDialog>

#include <cmath>

AtmosphericPanel::AtmosphericPanel(QWidget* parent)
    : PanelBase(parent)
{
    setObjectName(panelId());
    setupUi();
    retranslateUi();
}

QString AtmosphericPanel::panelTitle() const {
    return tr("Atmosphere");
}

void AtmosphericPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);

    // ------------------------------------------------------------------
    // Analytic terms (part of LightingParams)
    // ------------------------------------------------------------------
    m_analyticGroup = new QGroupBox(this);
    auto* analyticLayout = new QFormLayout(m_analyticGroup);

    auto* transRow = new QHBoxLayout();
    m_transmittanceSlider = new QSlider(Qt::Horizontal);
    m_transmittanceSlider->setRange(0, 100);
    m_transmittanceSlider->setValue(90);
    m_transmittanceValue = new QLabel(QStringLiteral("0.90"));
    m_transmittanceValue->setMinimumWidth(40);
    connect(m_transmittanceSlider, &QSlider::valueChanged,
            this, &AtmosphericPanel::onAnalyticChanged);
    transRow->addWidget(m_transmittanceSlider);
    transRow->addWidget(m_transmittanceValue);
    m_transmittanceCaption = new QLabel(m_analyticGroup);
    analyticLayout->addRow(m_transmittanceCaption, transRow);

    m_atmosphereTempSpin = new QDoubleSpinBox();
    m_atmosphereTempSpin->setRange(150.0, 350.0);
    m_atmosphereTempSpin->setSingleStep(5.0);
    m_atmosphereTempSpin->setValue(260.0);
    connect(m_atmosphereTempSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AtmosphericPanel::onAnalyticChanged);
    m_atmosphereTempCaption = new QLabel(m_analyticGroup);
    analyticLayout->addRow(m_atmosphereTempCaption, m_atmosphereTempSpin);

    m_analyticNote = new QLabel(m_analyticGroup);
    uistyle::applyHintStyle(m_analyticNote);
    analyticLayout->addRow(m_analyticNote);

    mainLayout->addWidget(m_analyticGroup);

    // ------------------------------------------------------------------
    // Neural network model
    // ------------------------------------------------------------------
    m_nnGroup = new QGroupBox(this);
    auto* nnLayout = new QVBoxLayout(m_nnGroup);

    m_enabledCheck = new QCheckBox(m_nnGroup);
    m_enabledCheck->setChecked(false);
    nnLayout->addWidget(m_enabledCheck);

    auto* nnForm = new QFormLayout();

    m_presetCombo = new QComboBox();
    m_presetCombo->addItem(QString(), QStringLiteral("disabled"));
    m_presetCombo->addItem(QString(), QStringLiteral("clear"));
    m_presetCombo->addItem(QString(), QStringLiteral("turbulent_clear"));
    m_presetCombo->addItem(QString(), QStringLiteral("urban_haze"));
    m_presetCombo->addItem(QString(), QStringLiteral("fog"));
    m_presetCombo->addItem(QString(), QStringLiteral("light_rain"));
    m_presetCombo->addItem(QString(), QStringLiteral("heavy_rain"));
    m_presetCombo->addItem(QString(), QStringLiteral("snow"));
    m_presetCombo->addItem(QString(), QStringLiteral("haze"));
    m_presetCaption = new QLabel(m_nnGroup);
    nnForm->addRow(m_presetCaption, m_presetCombo);

    auto* packRow = new QHBoxLayout();
    m_modelPackEdit = new QLineEdit();
    packRow->addWidget(m_modelPackEdit, 1);
    m_modelPackBrowse = new QPushButton();
    m_modelPackBrowse->setFixedWidth(30);
    packRow->addWidget(m_modelPackBrowse);
    m_modelPackCaption = new QLabel(m_nnGroup);
    nnForm->addRow(m_modelPackCaption, packRow);

    nnLayout->addLayout(nnForm);

    // Weather parameters: nine calibrate-once values, folded away by default.
    m_advancedGroup = new CollapsibleGroupBox(m_nnGroup);
    auto* advancedLayout = new QFormLayout();

    m_atmosModelCombo = new QComboBox();
    m_atmosModelCombo->addItem(QString(), 2.0);
    m_atmosModelCombo->addItem(QString(), 3.0);
    m_atmosModelCaption = new QLabel();
    advancedLayout->addRow(m_atmosModelCaption, m_atmosModelCombo);

    m_ihazeCombo = new QComboBox();
    m_ihazeCombo->addItem(QString(), 1.0);
    m_ihazeCombo->addItem(QString(), 4.0);
    m_ihazeCombo->addItem(QString(), 5.0);
    m_ihazeCombo->addItem(QString(), 9.0);
    m_ihazeCombo->addItem(QString(), 10.0);
    m_ihazeCaption = new QLabel();
    advancedLayout->addRow(m_ihazeCaption, m_ihazeCombo);

    m_icldCombo = new QComboBox();
    m_icldCombo->addItem(QString(), 0.0);
    m_icldCombo->addItem(QString(), 6.0);
    m_icldCombo->addItem(QString(), 18.0);
    m_icldCaption = new QLabel();
    advancedLayout->addRow(m_icldCaption, m_icldCombo);

    m_visKm = new QDoubleSpinBox();
    m_visKm->setRange(0.5, 50.0);
    m_visKm->setDecimals(1);
    m_visKm->setSingleStep(0.5);
    m_visCaption = new QLabel();
    advancedLayout->addRow(m_visCaption, m_visKm);

    m_rainrtMmH = new QDoubleSpinBox();
    m_rainrtMmH->setRange(0.0, 50.0);
    m_rainrtMmH->setDecimals(1);
    m_rainrtMmH->setSingleStep(0.5);
    m_rainCaption = new QLabel();
    advancedLayout->addRow(m_rainCaption, m_rainrtMmH);

    m_tGroundK = new QDoubleSpinBox();
    m_tGroundK->setRange(253.0, 328.0);
    m_tGroundK->setDecimals(2);
    m_tGroundK->setSingleStep(1.0);
    m_tGroundCaption = new QLabel();
    advancedLayout->addRow(m_tGroundCaption, m_tGroundK);

    m_rh = new QDoubleSpinBox();
    m_rh->setRange(0.05, 1.0);
    m_rh->setDecimals(2);
    m_rh->setSingleStep(0.05);
    m_rhCaption = new QLabel();
    advancedLayout->addRow(m_rhCaption, m_rh);

    m_pHPa = new QDoubleSpinBox();
    m_pHPa->setRange(950.0, 1040.0);
    m_pHPa->setDecimals(2);
    m_pHPa->setSingleStep(1.0);
    m_pressureCaption = new QLabel();
    advancedLayout->addRow(m_pressureCaption, m_pHPa);

    m_h2oScale = new QDoubleSpinBox();
    m_h2oScale->setRange(0.5, 2.0);
    m_h2oScale->setDecimals(2);
    m_h2oScale->setSingleStep(0.05);
    m_h2oCaption = new QLabel();
    advancedLayout->addRow(m_h2oCaption, m_h2oScale);

    m_advancedGroup->setContentLayout(advancedLayout);
    m_advancedGroup->setCollapsed(true);
    nnLayout->addWidget(m_advancedGroup);

    mainLayout->addWidget(m_nnGroup);
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

void AtmosphericPanel::retranslateUi() {
    // The wording below is what the core actually does with these two values,
    // checked against its own configuration warnings rather than guessed:
    // view-path transmittance now comes from the network, and the analytic
    // temperature survives only as the thermal-sky fallback. The panel says so
    // instead of implying an exclusivity the SDK does not implement.
    m_analyticGroup->setTitle(tr("Analytic terms (legacy)"));
    m_transmittanceCaption->setText(tr("Transmittance:"));
    m_atmosphereTempCaption->setText(tr("Atmosphere temperature:"));
    m_atmosphereTempSpin->setSuffix(tr(" K"));
    m_transmittanceSlider->setToolTip(tr(
        "Superseded: view-path transmittance comes from the network model. "
        "Kept because it is still part of the lighting parameters uploaded "
        "each frame."));
    m_atmosphereTempSpin->setToolTip(tr(
        "Thermal-sky fallback for infrared downwelling, used when the network "
        "model is off."));
    m_analyticNote->setText(tr(
        "Both values live in the lighting parameters. The network model below "
        "supersedes the transmittance; the temperature remains the fallback "
        "sky for infrared when that model is disabled."));

    m_nnGroup->setTitle(tr("Neural network model (MODTRAN surrogate)"));
    m_enabledCheck->setText(tr("Use the network model"));
    m_enabledCheck->setToolTip(tr(
        "Transmittance and path radiance come from the baked network lookup "
        "tables. A model pack must be available; without one the renderer "
        "falls back to the analytic terms above."));
    m_presetCaption->setText(tr("Preset:"));
    m_modelPackCaption->setText(tr("Model pack:"));
    m_modelPackEdit->setPlaceholderText(tr("(auto-detect)"));
    m_modelPackEdit->setToolTip(tr(
        "Directory of <band>_<geom>_<net>.safetensors files.\n"
        "Leave empty to auto-detect."));
    m_modelPackBrowse->setText(tr("..."));

    // Preset names, refilled by value so the current choice is preserved.
    static const struct { const char* key; const char* label; } presets[] = {
        {"disabled",        QT_TR_NOOP("Disabled")},
        {"clear",           QT_TR_NOOP("Clear")},
        {"turbulent_clear", QT_TR_NOOP("Turbulent Clear")},
        {"urban_haze",      QT_TR_NOOP("Urban Haze")},
        {"fog",             QT_TR_NOOP("Fog")},
        {"light_rain",      QT_TR_NOOP("Light Rain")},
        {"heavy_rain",      QT_TR_NOOP("Heavy Rain")},
        {"snow",            QT_TR_NOOP("Snow")},
        {"haze",            QT_TR_NOOP("Haze")},
    };
    for (int i = 0; i < m_presetCombo->count(); ++i) {
        const QString key = m_presetCombo->itemData(i).toString();
        for (const auto& preset : presets) {
            if (key == QLatin1String(preset.key)) {
                m_presetCombo->setItemText(i, tr(preset.label));
                break;
            }
        }
    }

    m_advancedGroup->setTitle(tr("Weather Parameters"));
    m_atmosModelCaption->setText(tr("Atmosphere model:"));
    m_atmosModelCombo->setItemText(0, tr("Mid-Latitude Summer (2)"));
    m_atmosModelCombo->setItemText(1, tr("Mid-Latitude Winter (3)"));

    m_ihazeCaption->setText(tr("Aerosol (IHAZE):"));
    m_ihazeCombo->setItemText(0, tr("Rural (1)"));
    m_ihazeCombo->setItemText(1, tr("Maritime (4)"));
    m_ihazeCombo->setItemText(2, tr("Urban (5)"));
    m_ihazeCombo->setItemText(3, tr("Advection Fog (9)"));
    m_ihazeCombo->setItemText(4, tr("Radiation Fog (10)"));

    m_icldCaption->setText(tr("Cloud (ICLD):"));
    m_icldCombo->setItemText(0, tr("None (0)"));
    m_icldCombo->setItemText(1, tr("Rain Cloud (6)"));
    m_icldCombo->setItemText(2, tr("Cirrus (18)"));

    m_visCaption->setText(tr("Visibility:"));
    m_visKm->setSuffix(tr(" km"));
    m_rainCaption->setText(tr("Rain rate:"));
    m_rainrtMmH->setSuffix(tr(" mm/h"));
    // Qualified, like every other temperature in the application: this one is
    // the ground, not the atmosphere, the detector or the material.
    m_tGroundCaption->setText(tr("Ground temperature:"));
    m_tGroundK->setSuffix(tr(" K"));
    m_rhCaption->setText(tr("Relative humidity:"));
    m_pressureCaption->setText(tr("Pressure:"));
    m_pHPa->setSuffix(tr(" hPa"));
    m_h2oCaption->setText(tr("H₂O scale:"));
}

// ============================================================================
// Analytic terms
// ============================================================================

void AtmosphericPanel::setAnalyticTerms(float transmittance, float temperatureK) {
    m_transmittance = transmittance;
    m_atmosphereTempK = temperatureK;

    const QSignalBlocker slider(m_transmittanceSlider);
    const QSignalBlocker spin(m_atmosphereTempSpin);
    m_transmittanceSlider->setValue(static_cast<int>(transmittance * 100.0f));
    m_atmosphereTempSpin->setValue(temperatureK);
    m_transmittanceValue->setText(QString::number(transmittance, 'f', 2));
}

void AtmosphericPanel::onAnalyticChanged() {
    if (m_updatingUi) return;

    m_transmittance = m_transmittanceSlider->value() / 100.0f;
    m_atmosphereTempK = static_cast<float>(m_atmosphereTempSpin->value());
    m_transmittanceValue->setText(QString::number(m_transmittance, 'f', 2));

    emit analyticTermsChanged(m_transmittance, m_atmosphereTempK);
}

// ============================================================================
// NN model
// ============================================================================

void AtmosphericPanel::setPreset(const QString& preset) {
    const QString lowerPreset = preset.toLower();

    for (int i = 0; i < m_presetCombo->count(); ++i) {
        if (m_presetCombo->itemData(i).toString() == lowerPreset) {
            m_updatingUi = true;
            m_presetCombo->setCurrentIndex(i);
            m_enabledCheck->setChecked(lowerPreset != QLatin1String("disabled"));
            m_updatingUi = false;

            onPresetChanged(i);
            return;
        }
    }

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
    const std::string packDir = m_modelPackEdit->text().trimmed().toStdString();

    quantiloom::AtmosphereNNConfig config;
    config.modelPackDir = packDir;
    if (config.ApplyPreset(presetName.toStdString())) {
        config.enabled = (presetName != QLatin1String("disabled"));
    } else {
        config.enabled = false;
        config.preset = "disabled";
        presetName = QStringLiteral("disabled");
    }
    m_config = config;

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
        m_updatingUi = true;
        m_presetCombo->setCurrentIndex(0);  // Disabled
        m_updatingUi = false;

        m_config.enabled = false;
        m_config.preset = "disabled";

        emit presetChanged(QStringLiteral("disabled"));
        emit configChanged(m_config);
    } else {
        m_updatingUi = true;
        m_presetCombo->setCurrentIndex(1);  // Clear
        m_updatingUi = false;

        const std::string packDir = m_modelPackEdit->text().trimmed().toStdString();
        quantiloom::AtmosphereNNConfig config;
        config.modelPackDir = packDir;
        config.ApplyPreset("clear");
        config.enabled = true;
        m_config = config;

        m_updatingUi = true;
        updateAdvancedParamsFromConfig(m_config);
        m_updatingUi = false;

        emit presetChanged(QStringLiteral("clear"));
        emit configChanged(m_config);
    }
}

void AtmosphericPanel::onBrowseModelPack() {
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select the atmosphere model pack directory"),
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
        const double dist = std::abs(combo->itemData(i).toDouble() - value);
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
