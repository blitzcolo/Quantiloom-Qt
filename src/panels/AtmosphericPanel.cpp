/**
 * @file AtmosphericPanel.cpp
 * @brief Combined analytic + NN atmosphere panel — implementation
 *
 * @author blitzcolo
 */

#include "AtmosphericPanel.hpp"

#include "../ui/CollapsibleGroupBox.hpp"
#include "../ui/UiStyle.hpp"

#include <postprocess/Thermography.hpp>

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

#include <algorithm>
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
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // ------------------------------------------------------------------
    // Analytic terms (part of LightingParams)
    // ------------------------------------------------------------------
    m_analyticGroup = new QGroupBox(this);
    auto* analyticLayout = new QFormLayout(m_analyticGroup);

    m_clearSkyCheck = new QCheckBox(m_analyticGroup);
    connect(m_clearSkyCheck, &QCheckBox::toggled,
            this, &AtmosphericPanel::onAnalyticChanged);
    analyticLayout->addRow(m_clearSkyCheck);

    m_atmosphereTempSpin = new QDoubleSpinBox();
    m_atmosphereTempSpin->setRange(150.0, 350.0);
    m_atmosphereTempSpin->setSingleStep(5.0);
    m_atmosphereTempSpin->setValue(260.0);
    m_atmosphereTempSpin->setKeyboardTracking(false);
    connect(m_atmosphereTempSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AtmosphericPanel::onAnalyticChanged);
    m_atmosphereTempCaption = new QLabel(m_analyticGroup);
    analyticLayout->addRow(m_atmosphereTempCaption, m_atmosphereTempSpin);

    m_humiditySpin = new QDoubleSpinBox();
    m_humiditySpin->setRange(1.0, 100.0);
    m_humiditySpin->setSingleStep(5.0);
    m_humiditySpin->setDecimals(0);
    m_humiditySpin->setValue(50.0);
    m_humiditySpin->setKeyboardTracking(false);
    connect(m_humiditySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AtmosphericPanel::onAnalyticChanged);
    m_humidityCaption = new QLabel(m_analyticGroup);
    analyticLayout->addRow(m_humidityCaption, m_humiditySpin);

    m_derivedLabel = new QLabel(m_analyticGroup);
    bindStyle([this] { uistyle::applyHintStyle(m_derivedLabel); });
    analyticLayout->addRow(m_derivedLabel);

    m_analyticNote = new QLabel(m_analyticGroup);
    bindStyle([this] { uistyle::applyHintStyle(m_analyticNote); });
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
    m_modelPackBrowse = new QPushButton(QStringLiteral("…"));   // tooltip says what it does
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

    // In percent, like the analytic humidity above; the network's fraction is
    // converted at the edge. Two "relative humidity" rows reading 50 and 0.50
    // were the same quantity in two units.
    m_rh = new QDoubleSpinBox();
    m_rh->setRange(5.0, 100.0);
    m_rh->setDecimals(0);
    m_rh->setSingleStep(5.0);
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

    // Each edit re-bakes the lookup tables, so a value is applied when it is
    // committed rather than once per keystroke.
    for (QDoubleSpinBox* spin : {m_visKm, m_rainrtMmH, m_tGroundK, m_rh, m_pHPa, m_h2oScale}) {
        spin->setKeyboardTracking(false);
    }

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
    m_analyticGroup->setTitle(tr("Analytic sky"));
    m_clearSkyCheck->setText(tr("Clear sky (Berdahl-Fromberg)"));
    m_clearSkyCheck->setToolTip(tr(
        "A partly transparent slab of air rather than one blackbody: the "
        "zenith reads far colder than the horizon, which is what a thermal "
        "camera sees and what drives radiative cooling. Off gives the "
        "isotropic sky, as warm overhead as at the horizon."));
    m_atmosphereTempCaption->setText(tr("Air temperature:"));
    m_atmosphereTempSpin->setSuffix(tr(" K"));
    m_atmosphereTempSpin->setToolTip(tr(
        "Temperature of the air itself. With the clear sky on it is the sky's "
        "Planck temperature, which the emissivity then makes read colder; with "
        "it off it is the whole of the thermal sky."));
    m_humidityCaption->setText(tr("Relative humidity:"));
    m_humiditySpin->setSuffix(tr(" %"));
    m_humiditySpin->setToolTip(tr(
        "How much water vapour is in the way. It is the vapour that radiates: "
        "a humid sky reads warmer, and a dry one lets more of the cold "
        "background through."));
    m_analyticNote->setText(tr(
        "These live in the lighting parameters. With the network model below "
        "on and its model pack found, transmittance and path radiance come "
        "from it instead, and these remain the thermal-sky fallback."));
    updateDerivedLabel();

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
    m_modelPackBrowse->setToolTip(tr("Choose the model pack directory"));
    m_presetCombo->setToolTip(tr(
        "A named weather. Choosing one sets every parameter below; editing a "
        "parameter afterwards keeps the preset as the starting point and marks "
        "it edited."));

    // Preset names, refilled by value so the current choice is preserved.
    updatePresetDisplay();

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
    m_atmosModelCombo->setToolTip(tr(
        "MODTRAN's standard atmosphere profile: the temperature, pressure and "
        "gas columns with height."));
    m_ihazeCombo->setToolTip(tr(
        "MODTRAN's boundary-layer aerosol model, which sets how particles "
        "scatter and absorb near the ground."));
    m_icldCombo->setToolTip(tr("MODTRAN's cloud or rain model, if any."));
    m_visKm->setToolTip(tr(
        "Meteorological range: how far a dark object stays distinguishable "
        "against the horizon sky. Lower is hazier."));
    m_h2oScale->setToolTip(tr(
        "Multiplies the profile's water-vapour column. Above 1 is a wetter "
        "atmosphere than the standard profile, which mostly matters in the "
        "infrared."));
    m_rh->setToolTip(tr("Relative humidity at the ground, as the network was trained on it."));

    m_visCaption->setText(tr("Visibility:"));
    m_visKm->setSuffix(tr(" km"));
    m_rainCaption->setText(tr("Rain rate:"));
    m_rainrtMmH->setSuffix(tr(" mm/h"));
    // Qualified, like every other temperature in the application: this one is
    // the ground, not the atmosphere, the detector or the material.
    m_tGroundCaption->setText(tr("Ground temperature:"));
    m_tGroundK->setSuffix(tr(" K"));
    m_rhCaption->setText(tr("Relative humidity:"));
    m_rh->setSuffix(tr(" %"));
    m_pressureCaption->setText(tr("Pressure:"));
    m_pHPa->setSuffix(tr(" hPa"));
    m_h2oCaption->setText(tr("H₂O scale:"));
}

// ============================================================================
// Analytic terms
// ============================================================================

void AtmosphericPanel::setAnalyticSky(bool clearSky, float airTemperatureK,
                                      float relativeHumidity) {
    m_atmosphereTempK = airTemperatureK;
    m_relativeHumidity = relativeHumidity;

    const QSignalBlocker check(m_clearSkyCheck);
    const QSignalBlocker spin(m_atmosphereTempSpin);
    const QSignalBlocker humidity(m_humiditySpin);
    m_clearSkyCheck->setChecked(clearSky);
    m_atmosphereTempSpin->setValue(airTemperatureK);
    m_humiditySpin->setValue(relativeHumidity);
    updateDerivedLabel();
}

bool AtmosphericPanel::clearSkyEnabled() const {
    return m_clearSkyCheck->isChecked();
}

void AtmosphericPanel::onAnalyticChanged() {
    if (m_updatingUi) return;

    m_atmosphereTempK = static_cast<float>(m_atmosphereTempSpin->value());
    m_relativeHumidity = static_cast<float>(m_humiditySpin->value());
    updateDerivedLabel();

    emit analyticSkyChanged(m_clearSkyCheck->isChecked(), m_atmosphereTempK,
                            m_relativeHumidity);
}

void AtmosphericPanel::updateDerivedLabel() {
    if (!m_derivedLabel) {
        return;
    }
    const bool clearSky = m_clearSkyCheck->isChecked();
    m_humiditySpin->setEnabled(clearSky);
    m_humidityCaption->setEnabled(clearSky);

    if (!clearSky) {
        m_derivedLabel->setText(
            tr("Isotropic: the sky is one blackbody at the air temperature."));
        return;
    }

    // Derived by the SDK, not here: the correlation and the dew point behind
    // it are physics, and a second copy in the GUI is a second thing to get
    // wrong.
    const double dewPointC = quantiloom::DewPointC(
        static_cast<double>(m_atmosphereTempK) - 273.15,
        static_cast<double>(m_relativeHumidity));
    const double emissivity = quantiloom::ClearSkyEmissivity(dewPointC);
    const double skyK = quantiloom::EffectiveSkyTemperatureK(
        static_cast<double>(m_atmosphereTempK), emissivity);

    m_derivedLabel->setText(
        tr("Dew point %1 °C, zenith emissivity %2, effective sky %3 K "
           "(%4 K below the air).")
            .arg(dewPointC, 0, 'f', 1)
            .arg(emissivity, 0, 'f', 3)
            .arg(skyK, 0, 'f', 1)
            .arg(static_cast<double>(m_atmosphereTempK) - skyK, 0, 'f', 1));
}

// ============================================================================
// NN model
// ============================================================================

void AtmosphericPanel::setPreset(const QString& preset) {
    // A setter, so it reports nothing: a document being loaded is not the
    // user choosing a preset. It does derive the preset's parameters, which
    // is what a caller about to override some of them wants to start from.
    const QString lowerPreset = preset.toLower();
    m_lastEnabledPreset.clear();   // a new document; nothing to restore from
    quantiloom::AtmosphereNNConfig config;
    config.modelPackDir = m_modelPackEdit->text().trimmed().toStdString();
    if (lowerPreset != QLatin1String("disabled") &&
        config.ApplyPreset(lowerPreset.toStdString())) {
        config.enabled = true;
    } else {
        config.enabled = false;
        config.preset = "disabled";
    }
    setAtmosphericConfig(config);
}

QString AtmosphericPanel::preset() const {
    return QString::fromStdString(m_config.preset);
}

QString AtmosphericPanel::presetDisplayName(const QString& preset) {
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
    for (const auto& entry : presets) {
        if (preset == QLatin1String(entry.key)) {
            return tr(entry.label);
        }
    }
    return preset;
}

bool AtmosphericPanel::presetEdited() const {
    if (!m_config.enabled || m_config.preset == "disabled") {
        return false;
    }
    quantiloom::AtmosphereNNConfig reference;
    if (!reference.ApplyPreset(m_config.preset)) {
        return false;
    }
    // Within float precision: a document stores these as f32, so 293.15 read
    // back is not the double the preset holds, and is not an edit.
    const auto moved = [](double a, double b) {
        return std::abs(a - b) > 1e-5 * std::max(1.0, std::abs(b));
    };
    return moved(m_config.atmosModel, reference.atmosModel) ||
           moved(m_config.ihaze, reference.ihaze) || moved(m_config.icld, reference.icld) ||
           moved(m_config.visKm, reference.visKm) ||
           moved(m_config.rainrtMmH, reference.rainrtMmH) ||
           moved(m_config.tGroundK, reference.tGroundK) || moved(m_config.rh, reference.rh) ||
           moved(m_config.pHPa, reference.pHPa) || moved(m_config.h2oScale, reference.h2oScale);
}

void AtmosphericPanel::updatePresetDisplay() {
    // The combo shows the preset the parameters started from, and says so
    // when they have since been moved: a combo reading "Fog" beside a
    // visibility of 40 km was claiming something the document did not say.
    const bool edited = presetEdited();
    for (int i = 0; i < m_presetCombo->count(); ++i) {
        const QString key = m_presetCombo->itemData(i).toString();
        const QString name = presetDisplayName(key);
        m_presetCombo->setItemText(
            i, (edited && i == m_presetCombo->currentIndex()) ? tr("%1 (edited)").arg(name)
                                                               : name);
    }
}

void AtmosphericPanel::updateNetworkEnabled() {
    // The preset stays live -- choosing one is a way of turning the model on.
    // The pack and the weather do nothing while it is off.
    const bool on = m_enabledCheck->isChecked();
    m_modelPackEdit->setEnabled(on);
    m_modelPackBrowse->setEnabled(on);
    m_modelPackCaption->setEnabled(on);
    m_advancedGroup->setEnabled(on);
}

void AtmosphericPanel::setAtmosphericConfig(const quantiloom::AtmosphereNNConfig& config) {
    m_config = config;
    if (config.enabled && config.preset != "disabled") {
        m_lastEnabledPreset = config.preset;
    }
    m_updatingUi = true;

    {
        // Synced from the config's own preset, so an undo or a read-back from
        // the renderer leaves the combo -- and what is saved -- telling the
        // truth.
        const QSignalBlocker blocker(m_presetCombo);
        const QString key = config.enabled ? QString::fromStdString(config.preset)
                                           : QStringLiteral("disabled");
        const int index = m_presetCombo->findData(key);
        m_presetCombo->setCurrentIndex(index < 0 ? 0 : index);
    }
    m_enabledCheck->setChecked(config.enabled);
    m_modelPackEdit->setText(QString::fromStdString(config.modelPackDir));
    updateAdvancedParamsFromConfig(config);

    m_updatingUi = false;
    updatePresetDisplay();
    updateNetworkEnabled();
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
    updatePresetDisplay();
    updateNetworkEnabled();

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
    m_config.rh = m_rh->value() / 100.0;   // the network takes a fraction
    m_config.pHPa = m_pHPa->value();
    m_config.h2oScale = m_h2oScale->value();

    updatePresetDisplay();
    emit configChanged(m_config);
}

void AtmosphericPanel::onEnabledChanged(bool enabled) {
    if (m_updatingUi) return;

    // Switching off keeps the weather, and switching back on restores the
    // preset it was off from: the toggle used to reset everything to Clear,
    // so turning the model off to compare and on again lost the setup.
    if (!enabled) {
        if (m_config.preset != "disabled") {
            m_lastEnabledPreset = m_config.preset;
        }
        m_config.enabled = false;
        m_config.preset = "disabled";
    } else {
        m_config.enabled = true;
        m_config.preset = m_lastEnabledPreset.empty() ? std::string("clear")
                                                      : m_lastEnabledPreset;
        if (m_lastEnabledPreset.empty()) {
            // Never on in this document: start from Clear's parameters.
            quantiloom::AtmosphereNNConfig fresh;
            fresh.modelPackDir = m_modelPackEdit->text().trimmed().toStdString();
            fresh.ApplyPreset("clear");
            fresh.enabled = true;
            m_config = fresh;
        }
    }
    setAtmosphericConfig(m_config);

    const QString presetName = QString::fromStdString(m_config.preset);
    emit presetChanged(presetName);
    emit configChanged(m_config);
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
    m_rh->setValue(config.rh * 100.0);
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
