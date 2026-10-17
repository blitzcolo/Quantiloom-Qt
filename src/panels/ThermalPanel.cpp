/**
 * @file ThermalPanel.cpp
 * @brief Thermal solve control panel
 */

#include "ThermalPanel.hpp"

#include <algorithm>
#include <cmath>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <initializer_list>

ThermalPanel::ThermalPanel(QWidget* parent)
    : PanelBase(parent) {
    setupUi();
}

QString ThermalPanel::panelTitle() const {
    return tr("Thermal Solve");
}

void ThermalPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // Enable checkbox
    m_enableCheck = new QCheckBox(this);
    connect(m_enableCheck, &QCheckBox::toggled, this, &ThermalPanel::onEnabledToggled);
    connect(m_enableCheck, &QCheckBox::toggled, this,
            [this](bool) { updateSolveDependentEnabled(); });
    mainLayout->addWidget(m_enableCheck);

    // Time of day
    m_timeGroup = new QGroupBox(this);
    auto* timeLayout = new QFormLayout(m_timeGroup);

    auto* timeRow = new QHBoxLayout();
    m_timeSlider = new QSlider(Qt::Horizontal);
    m_timeSlider->setRange(0, 24 * 60);
    m_timeSlider->setValue(12 * 60);
    connect(m_timeSlider, &QSlider::valueChanged, this, &ThermalPanel::onTimeSliderChanged);
    connect(m_timeSlider, &QSlider::sliderPressed, this, &ThermalPanel::editGestureStarted);
    connect(m_timeSlider, &QSlider::sliderReleased, this, &ThermalPanel::editGestureFinished);

    m_timeSpin = new QDoubleSpinBox();
    // The same span as the slider until the first status narrows both to the
    // solve's own; a spin box that went further than the slider would hold an
    // hour the slider cannot show.
    m_timeSpin->setRange(0.0, 24.0);
    m_timeSpin->setKeyboardTracking(false);
    m_timeSpin->setSingleStep(0.5);
    m_timeSpin->setDecimals(2);
    m_timeSpin->setSuffix(QStringLiteral(" h"));
    m_timeSpin->setValue(12.0);
    connect(m_timeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ThermalPanel::onTimeSpinChanged);

    timeRow->addWidget(m_timeSlider, 1);
    timeRow->addWidget(m_timeSpin);
    m_timeCaption = new QLabel(m_timeGroup);
    timeLayout->addRow(m_timeCaption, timeRow);
    mainLayout->addWidget(m_timeGroup);

    // Parameters group
    m_paramsGroup = new QGroupBox(this);
    auto* paramsLayout = new QFormLayout(m_paramsGroup);

    m_startTimeSpin = new QDoubleSpinBox();
    m_startTimeSpin->setRange(0.0, 48.0);
    m_startTimeSpin->setSingleStep(1.0);
    m_startTimeSpin->setDecimals(1);
    m_startTimeSpin->setSuffix(QStringLiteral(" h"));
    m_startTimeCaption = new QLabel(m_paramsGroup);
    paramsLayout->addRow(m_startTimeCaption, m_startTimeSpin);

    m_timestepSpin = new QDoubleSpinBox();
    m_timestepSpin->setRange(1.0, 3600.0);
    m_timestepSpin->setSingleStep(10.0);
    m_timestepSpin->setDecimals(0);
    m_timestepSpin->setSuffix(QStringLiteral(" s"));
    m_timestepCaption = new QLabel(m_paramsGroup);
    paramsLayout->addRow(m_timestepCaption, m_timestepSpin);

    m_layersSpin = new QSpinBox();
    m_layersSpin->setRange(2, 32);
    m_layersSpin->setValue(10);
    m_layersCaption = new QLabel(m_paramsGroup);
    paramsLayout->addRow(m_layersCaption, m_layersSpin);

    m_initialCombo = new QComboBox();
    // Text set in retranslateUi(); the data is the SDK enum, read back by value.
    m_initialCombo->addItem(QString(),
                            static_cast<int>(quantiloom::ThermalInitialCondition::Steady));
    m_initialCombo->addItem(QString(),
                            static_cast<int>(quantiloom::ThermalInitialCondition::Uniform));
    connect(m_initialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { updateInitialTemperatureEnabled(); });
    m_initialCaption = new QLabel(m_paramsGroup);
    paramsLayout->addRow(m_initialCaption, m_initialCombo);

    m_initialTempSpin = new QDoubleSpinBox();
    m_initialTempSpin->setRange(1.0, 5000.0);
    m_initialTempSpin->setSingleStep(1.0);
    m_initialTempSpin->setDecimals(2);
    m_initialTempSpin->setSuffix(QStringLiteral(" K"));
    m_initialTempCaption = new QLabel(m_paramsGroup);
    paramsLayout->addRow(m_initialTempCaption, m_initialTempSpin);

    m_sunIrradianceSpin = new QDoubleSpinBox();
    m_sunIrradianceSpin->setRange(0.0, 2000.0);
    m_sunIrradianceSpin->setSingleStep(50.0);
    m_sunIrradianceSpin->setDecimals(1);
    m_sunIrradianceSpin->setSuffix(QStringLiteral(" W/m²"));
    m_sunIrradianceCaption = new QLabel(m_paramsGroup);
    paramsLayout->addRow(m_sunIrradianceCaption, m_sunIrradianceSpin);

    m_diffuseIrradianceSpin = new QDoubleSpinBox();
    m_diffuseIrradianceSpin->setRange(0.0, 1000.0);
    m_diffuseIrradianceSpin->setSingleStep(25.0);
    m_diffuseIrradianceSpin->setDecimals(1);
    m_diffuseIrradianceSpin->setSuffix(QStringLiteral(" W/m²"));
    m_diffuseIrradianceCaption = new QLabel(m_paramsGroup);
    paramsLayout->addRow(m_diffuseIrradianceCaption, m_diffuseIrradianceSpin);

    m_exchangeRaysSpin = new QSpinBox();
    m_exchangeRaysSpin->setRange(16, 4096);
    m_exchangeRaysSpin->setSingleStep(64);
    m_exchangeRaysSpin->setValue(256);
    m_exchangeRaysCaption = new QLabel(m_paramsGroup);
    paramsLayout->addRow(m_exchangeRaysCaption, m_exchangeRaysSpin);

    m_exchangeTopKSpin = new QSpinBox();
    m_exchangeTopKSpin->setRange(1, 256);
    m_exchangeTopKSpin->setValue(32);
    m_exchangeTopKCaption = new QLabel(m_paramsGroup);
    paramsLayout->addRow(m_exchangeTopKCaption, m_exchangeTopKSpin);

    m_checkpointStrideSpin = new QDoubleSpinBox();
    m_checkpointStrideSpin->setRange(0.1, 24.0);
    m_checkpointStrideSpin->setSingleStep(0.5);
    m_checkpointStrideSpin->setDecimals(1);
    m_checkpointStrideSpin->setSuffix(QStringLiteral(" h"));
    m_checkpointStrideCaption = new QLabel(m_paramsGroup);
    paramsLayout->addRow(m_checkpointStrideCaption, m_checkpointStrideSpin);

    auto* forcingRow = new QHBoxLayout();
    m_forcingFileEdit = new QLineEdit();
    m_forcingFileEdit->setReadOnly(true);
    m_forcingBrowse = new QPushButton(QStringLiteral("…"));   // tooltip says what it does
    m_forcingBrowse->setFixedWidth(30);
    connect(m_forcingBrowse, &QPushButton::clicked, this, &ThermalPanel::onBrowseForcing);
    forcingRow->addWidget(m_forcingFileEdit, 1);
    forcingRow->addWidget(m_forcingBrowse);
    m_forcingCaption = new QLabel(m_paramsGroup);
    paramsLayout->addRow(m_forcingCaption, forcingRow);

    m_sunCorrectionCheck = new QCheckBox(m_paramsGroup);
    m_sunCorrectionCheck->setChecked(true);
    paramsLayout->addRow(QString(), m_sunCorrectionCheck);

    // Every one of these is a re-solve, so a value is applied when it is
    // committed (Enter, focus out, an arrow) rather than once per keystroke:
    // typing 600 into Timestep would otherwise solve at 6, 60 and 600.
    for (QAbstractSpinBox* spin : std::initializer_list<QAbstractSpinBox*>{
             m_startTimeSpin, m_timestepSpin, m_layersSpin, m_initialTempSpin,
             m_sunIrradianceSpin, m_diffuseIrradianceSpin, m_exchangeRaysSpin,
             m_exchangeTopKSpin, m_checkpointStrideSpin}) {
        spin->setKeyboardTracking(false);
    }

    // Connect param changes
    auto paramSlot = [this] { onParamChanged(); };
    connect(m_startTimeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, paramSlot);
    connect(m_timestepSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, paramSlot);
    connect(m_layersSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, paramSlot);
    connect(m_initialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, paramSlot);
    connect(m_initialTempSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, paramSlot);
    connect(m_sunIrradianceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, paramSlot);
    connect(m_diffuseIrradianceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, paramSlot);
    connect(m_exchangeRaysSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, paramSlot);
    connect(m_exchangeTopKSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, paramSlot);
    connect(m_checkpointStrideSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, paramSlot);
    connect(m_sunCorrectionCheck, &QCheckBox::toggled, this, paramSlot);

    mainLayout->addWidget(m_paramsGroup);

    // Status group
    m_statusGroup = new QGroupBox(this);
    auto* statusLayout = new QVBoxLayout(m_statusGroup);
    m_statusElementCount = new QLabel(m_statusGroup);
    m_statusTemperatures = new QLabel(m_statusGroup);
    m_statusSteps = new QLabel(m_statusGroup);
    m_statusExchange = new QLabel(m_statusGroup);
    m_statusStepper = new QLabel(m_statusGroup);
    m_statusError = new QLabel(m_statusGroup);
    m_statusError->setWordWrap(true);
    statusLayout->addWidget(m_statusElementCount);
    statusLayout->addWidget(m_statusTemperatures);
    statusLayout->addWidget(m_statusSteps);
    statusLayout->addWidget(m_statusExchange);
    statusLayout->addWidget(m_statusStepper);
    statusLayout->addWidget(m_statusError);
    mainLayout->addWidget(m_statusGroup);

    // The what-if preview. A step of the parameter, applied as a first-order
    // correction to the field the solve already has: no re-solve, so the
    // slider can be dragged and the viewport follows. What the two together
    // show is the difference between a preview and a seek, which is the whole
    // reason to have one.
    m_whatIfGroup = new QGroupBox(this);
    auto* whatIfLayout = new QVBoxLayout(m_whatIfGroup);
    m_whatIfParameter = new QComboBox(m_whatIfGroup);
    m_whatIfParameter->addItem(QString(),
                               static_cast<int>(quantiloom::ThermalSensitivityParameter::Convection));
    m_whatIfParameter->addItem(QString(),
                               static_cast<int>(quantiloom::ThermalSensitivityParameter::Emissivity));
    m_whatIfParameter->addItem(QString(),
                               static_cast<int>(quantiloom::ThermalSensitivityParameter::Absorptivity));
    m_whatIfParameter->addItem(QString(),
                               static_cast<int>(quantiloom::ThermalSensitivityParameter::Conductivity));
    m_whatIfParameter->addItem(QString(),
                               static_cast<int>(quantiloom::ThermalSensitivityParameter::HeatCapacity));
    // Percent of the parameter, so one slider suits h in W/m2K and rho c in
    // J/m3K without either needing a range of its own. Centred at zero, which
    // is the preview switched off.
    m_whatIfSlider = new QSlider(Qt::Horizontal, m_whatIfGroup);
    m_whatIfSlider->setRange(-50, 50);
    m_whatIfSlider->setValue(0);
    m_whatIfCaption = new QLabel(m_whatIfGroup);
    m_whatIfCaption->setWordWrap(true);
    whatIfLayout->addWidget(m_whatIfParameter);
    whatIfLayout->addWidget(m_whatIfSlider);
    whatIfLayout->addWidget(m_whatIfCaption);
    connect(m_whatIfParameter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ThermalPanel::onWhatIfChanged);
    connect(m_whatIfSlider, &QSlider::valueChanged, this, &ThermalPanel::onWhatIfChanged);
    mainLayout->addWidget(m_whatIfGroup);

    // The probe. Given real height rather than left to the layout: a chart
    // squeezed into two rows of a docked panel is a chart nobody reads, and
    // this one is the answer to the question the controls above raise.
    m_probeGroup = new QGroupBox(this);
    auto* probeLayout = new QVBoxLayout(m_probeGroup);
    m_probeCaption = new QLabel(m_probeGroup);
    m_probeCaption->setWordWrap(true);
    m_probePlot = new uiplot::TrajectoryPlotWidget(m_probeGroup);
    m_probePlot->setMinimumHeight(260);
    probeLayout->addWidget(m_probeCaption);
    probeLayout->addWidget(m_probePlot, 1);
    mainLayout->addWidget(m_probeGroup, 1);

    mainLayout->addStretch();

    updateInitialTemperatureEnabled();
    updateSolveDependentEnabled();
    retranslateUi();
}

void ThermalPanel::retranslateUi() {
    // Set directly rather than through bindText(): setMappedHour() and the
    // status refresh run every tick, and a bindText() reached from them would
    // register one more setter each time.
    m_enableCheck->setText(tr("Enable thermal solve"));
    updateTimeGroupText();
    m_paramsGroup->setTitle(tr("Solver Parameters"));
    m_startTimeCaption->setText(tr("Start time:"));
    m_startTimeSpin->setToolTip(
        tr("The hour of the simulated day the run starts stepping from. Rendering "
           "a later hour costs the stepping in between."));
    m_timestepCaption->setText(tr("Timestep:"));
    m_timestepSpin->setToolTip(tr("Simulated seconds per solver step."));
    m_layersCaption->setText(tr("Layers:"));
    m_layersSpin->setToolTip(
        tr("Nodes through the depth of each surface's column. More resolves a "
           "steeper gradient below the surface, at a proportional cost."));
    m_initialCaption->setText(tr("Initial condition:"));
    m_initialCombo->setItemText(0, tr("Steady"));
    m_initialCombo->setItemText(1, tr("Uniform"));
    m_initialCombo->setToolTip(
        tr("Steady starts from the temperature each surface would settle at under "
           "the starting forcing. Uniform starts every node at the initial "
           "temperature below."));
    m_initialTempCaption->setText(tr("Initial object temperature:"));
    m_initialTempSpin->setToolTip(
        tr("Where every node starts when the initial condition is Uniform. Steady "
           "does not read it."));
    m_sunIrradianceCaption->setText(tr("Solar irradiance:"));
    m_sunIrradianceSpin->setToolTip(
        tr("Direct normal irradiance, used when no forcing file is given. A clear "
           "noon is around 900 W/m²."));
    m_diffuseIrradianceCaption->setText(tr("Diffuse irradiance:"));
    m_diffuseIrradianceSpin->setToolTip(
        tr("Sunlight arriving from the sky dome rather than from the disc. "
           "Under overcast it is the whole of it."));
    m_exchangeRaysCaption->setText(tr("Exchange rays:"));
    m_exchangeRaysSpin->setToolTip(
        tr("Rays per element for the view-factor precompute: how surfaces see "
           "and heat each other. More is less noise and a slower precompute."));
    m_exchangeTopKCaption->setText(tr("Exchange top-K:"));
    m_exchangeTopKSpin->setToolTip(
        tr("How many of each element's strongest view-factor entries to keep. A "
           "row past the top few is mostly noise."));
    m_checkpointStrideCaption->setText(tr("Checkpoint stride:"));
    m_checkpointStrideSpin->setToolTip(
        tr("How often the trajectory stores a full state, in simulated hours. "
           "Shorter is more memory and a cheaper scrub."));
    m_forcingCaption->setText(tr("Forcing file:"));
    m_forcingBrowse->setToolTip(tr("Choose a forcing CSV"));
    m_forcingFileEdit->setToolTip(
        tr("Time-varying air temperature, irradiance, sun position and sky "
           "temperature, as CSV. Empty means the constant values above."));
    m_sunCorrectionCheck->setText(tr("Shadow-edge correction"));
    m_sunCorrectionCheck->setToolTip(
        tr("Carry dT/dv through the trajectory, so a shading pass resolves a "
           "shadow edge inside a triangle rather than at its border. Off gives "
           "one temperature per triangle, which is what the correction has to "
           "be measured against."));
    m_statusGroup->setTitle(tr("Status"));
    updateStatusText();
    m_whatIfGroup->setTitle(tr("What if"));
    m_whatIfParameter->setItemText(0, tr("Convection h"));
    m_whatIfParameter->setItemText(1, tr("Emissivity"));
    m_whatIfParameter->setItemText(2, tr("Solar absorptivity"));
    m_whatIfParameter->setItemText(3, tr("Conductivity"));
    m_whatIfParameter->setItemText(4, tr("Heat capacity"));
    updateWhatIfCaption();
    m_probeGroup->setTitle(tr("Probe"));
    m_probePlot->setPlaceholderText(tr("Click a surface in the viewport."));
    m_probePlot->retranslate();
    updateProbeCaption();
    PanelBase::retranslateUi();
}

void ThermalPanel::updateTimeGroupText() {
    if (m_mappedHour) {
        m_timeGroup->setTitle(tr("Time of Day — following the timeline (%1 h)")
                                  .arg(*m_mappedHour, 0, 'f', 3));
        m_timeCaption->setText(tr("Hour at timeline start:"));
        m_timeSpin->setToolTip(
            tr("With a [timeline], this is the hour the clock starts at. Which "
               "hour is being rendered comes from the transport."));
    } else {
        m_timeGroup->setTitle(tr("Time of Day"));
        m_timeCaption->setText(tr("Rendered hour:"));
        m_timeSpin->setToolTip(
            tr("The hour of the simulated day shown in the viewport. The solve "
               "steps to it from the start time under Solver Parameters."));
    }
    m_timeSlider->setToolTip(m_timeSpin->toolTip());
}

void ThermalPanel::updateInitialTemperatureEnabled() {
    m_initialTempSpin->setEnabled(
        m_initialCombo->currentData().toInt() ==
        static_cast<int>(quantiloom::ThermalInitialCondition::Uniform));
}

void ThermalPanel::updateSolveDependentEnabled() {
    // The parameters stay editable so a solve can be set up before it is
    // switched on. The hour, the what-if and the probe all act on a solved
    // field, and with none they would move and do nothing.
    const bool on = m_enableCheck->isChecked();
    m_timeGroup->setEnabled(on);
    m_whatIfGroup->setEnabled(on);
    m_probeGroup->setEnabled(on);
}

void ThermalPanel::setSolveEnabled(bool enabled) {
    m_suppressSignals = true;
    m_enableCheck->setChecked(enabled);
    m_suppressSignals = false;
    updateSolveDependentEnabled();
}

void ThermalPanel::setParams(const quantiloom::ThermalSolveParams& p) {
    m_suppressSignals = true;
    m_startTimeSpin->setValue(p.startTime_h);
    m_timestepSpin->setValue(p.timestep_s);
    m_layersSpin->setValue(static_cast<int>(p.layerCount));
    const int initialIndex = m_initialCombo->findData(static_cast<int>(p.initial));
    m_initialCombo->setCurrentIndex(initialIndex < 0 ? 0 : initialIndex);
    updateInitialTemperatureEnabled();
    m_initialTempSpin->setValue(p.initialTemperature_K);
    m_sunIrradianceSpin->setValue(p.sunIrradiance_W_m2);
    m_diffuseIrradianceSpin->setValue(p.diffuseIrradiance_W_m2);
    m_exchangeRaysSpin->setValue(static_cast<int>(p.exchangeRays));
    m_exchangeTopKSpin->setValue(static_cast<int>(p.exchangeTopK));
    m_checkpointStrideSpin->setValue(p.checkpointStride_h);
    m_forcingFileEdit->setText(QString::fromStdString(p.forcingFile));
    m_airTemperatureK = p.airTemperature_K;
    m_skyTemperatureK = p.skyTemperature_K;
    m_relativeHumidity = p.relativeHumidity;
    m_sunCorrectionCheck->setChecked(p.sunCorrection);
    m_convectionModel = p.convectionModel;
    m_convectionWindA = p.convectionWindA_W_m2K;
    m_convectionWindB = p.convectionWindB_W_s_m3K;
    m_convectionFreeC = p.convectionFreeC;
    m_convectionReferenceHeightM = p.convectionReferenceHeight_m;
    m_convectionStableDamping = p.convectionStableDamping;
    m_lateralConduction = p.lateralConduction;
    m_sunMemoryLags = p.sunMemoryLags;
    m_parameterSensitivities = p.parameterSensitivities;
    m_dumpElementsFile = p.dumpElementsFile;
    m_suppressSignals = false;
}

void ThermalPanel::setTime(double time_h) {
    m_suppressSignals = true;
    m_timeSpin->setValue(time_h);
    m_timeSlider->setValue(static_cast<int>(time_h * 60.0));
    m_suppressSignals = false;
}

void ThermalPanel::setMappedHour(std::optional<double> hour) {
    if (m_mappedHour.has_value() == hour.has_value() &&
        (!hour.has_value() || std::abs(*hour - *m_mappedHour) < 1e-9)) {
        return;
    }
    m_mappedHour = hour;

    // The slider and the spin box mean different things now, so only one of
    // them goes away: the spin box still sets the hour the clock STARTS at,
    // which is a real thing to edit. The slider set the hour being rendered,
    // which the transport now owns.
    m_timeSlider->setEnabled(!m_mappedHour.has_value());
    updateTimeGroupText();
}

void ThermalPanel::updateStatus(const quantiloom::ThermalSolveStatus& s) {
    m_lastStatus = s;
    updateStatusText();

    // Update slider range from status
    const int startMin = static_cast<int>(s.sliderStartTime_h * 60.0);
    const int endMin = static_cast<int>(s.sliderEndTime_h * 60.0);
    if (endMin > startMin) {
        m_suppressSignals = true;
        m_timeSlider->setRange(startMin, endMin);
        m_timeSpin->setRange(s.sliderStartTime_h, s.sliderEndTime_h);
        m_suppressSignals = false;
    }
}

void ThermalPanel::updateStatusText() {
    if (!m_lastStatus) {
        m_statusElementCount->setText(tr("Not solved yet."));
        m_statusTemperatures->clear();
        m_statusSteps->clear();
        m_statusExchange->clear();
        m_statusStepper->clear();
        m_statusError->hide();
        return;
    }
    const quantiloom::ThermalSolveStatus& s = *m_lastStatus;
    m_statusElementCount->setText(
        tr("Elements: %1 (%2 solved)").arg(s.elementCount).arg(s.participatingElements));
    if (s.solveValid && s.participatingElements > 0) {
        m_statusTemperatures->setText(
            tr("Surface temperature: %1 – %2 K (mean %3 K)")
                .arg(s.minTemperature_K, 0, 'f', 1)
                .arg(s.maxTemperature_K, 0, 'f', 1)
                .arg(s.meanTemperature_K, 0, 'f', 1));
    } else {
        m_statusTemperatures->setText(tr("Surface temperature: —"));
    }
    m_statusSteps->setText(
        tr("Steps: %1, checkpoints: %2").arg(s.lastStepCount).arg(s.checkpointCount));
    m_statusExchange->setText(
        tr("Exchange: %1 entries (%2 runs), %3 sun samples")
            .arg(s.exchangeNonZeros).arg(s.exchangeRunCount).arg(s.sunSampleCount));
    m_statusStepper->setText(
        tr("Stepper: %1").arg(QString::fromStdString(s.stepperName)));
    if (s.error.empty()) {
        m_statusError->clear();
        m_statusError->hide();
    } else {
        // The SDK's own words; it reports in English.
        m_statusError->setText(tr("Solver error: %1").arg(QString::fromStdString(s.error)));
        m_statusError->show();
    }
}

bool ThermalPanel::isSolveEnabled() const {
    return m_enableCheck->isChecked();
}

quantiloom::ThermalSolveParams ThermalPanel::params() const {
    quantiloom::ThermalSolveParams p;
    p.startTime_h = m_startTimeSpin->value();
    p.timestep_s = m_timestepSpin->value();
    p.layerCount = static_cast<quantiloom::u32>(m_layersSpin->value());
    p.initial = static_cast<quantiloom::ThermalInitialCondition>(
        m_initialCombo->currentData().toInt());
    p.initialTemperature_K = m_initialTempSpin->value();
    p.sunIrradiance_W_m2 = m_sunIrradianceSpin->value();
    p.diffuseIrradiance_W_m2 = m_diffuseIrradianceSpin->value();
    p.exchangeRays = static_cast<quantiloom::u32>(m_exchangeRaysSpin->value());
    p.exchangeTopK = static_cast<quantiloom::u32>(m_exchangeTopKSpin->value());
    p.checkpointStride_h = m_checkpointStrideSpin->value();
    p.forcingFile = m_forcingFileEdit->text().toStdString();
    // The atmosphere's, not this panel's -- carried through so touching a spin
    // box here does not quietly reset them.
    p.airTemperature_K = m_airTemperatureK;
    p.skyTemperature_K = m_skyTemperatureK;
    p.relativeHumidity = m_relativeHumidity;
    p.sunCorrection = m_sunCorrectionCheck->isChecked();
    p.convectionModel = m_convectionModel;
    p.convectionWindA_W_m2K = m_convectionWindA;
    p.convectionWindB_W_s_m3K = m_convectionWindB;
    p.convectionFreeC = m_convectionFreeC;
    p.convectionReferenceHeight_m = m_convectionReferenceHeightM;
    p.convectionStableDamping = m_convectionStableDamping;
    p.lateralConduction = m_lateralConduction;
    p.sunMemoryLags = m_sunMemoryLags;
    p.parameterSensitivities = m_parameterSensitivities;
    p.dumpElementsFile = m_dumpElementsFile;
    return p;
}

void ThermalPanel::onWhatIfChanged() {
    updateWhatIfCaption();
    if (m_suppressSignals) return;
    emit whatIfChanged(
        static_cast<quantiloom::ThermalSensitivityParameter>(
            m_whatIfParameter->currentData().toInt()),
        m_whatIfSlider->value() / 100.0);
}

void ThermalPanel::updateWhatIfCaption() {
    if (!m_whatIfCaption) return;
    const int percent = m_whatIfSlider->value();
    if (percent == 0) {
        m_whatIfCaption->setText(
            tr("Off. Move the slider to see what changing this parameter would do, "
               "before the solve is asked to do it."));
        return;
    }
    // The sign is part of the number, not of the sentence.
    const QString signedPercent =
        percent > 0 ? QStringLiteral("+%1").arg(percent) : QString::number(percent);
    m_whatIfCaption->setText(
        tr("%1% of the parameter, added as a first-order correction. Exact for a "
           "small step; a linearisation for a large one.")
            .arg(signedPercent));
}

void ThermalPanel::setProbe(const quantiloom::u32 element,
                            const quantiloom::ThermalElementTrajectory& trajectory) {
    m_probeElement = static_cast<int>(element);
    m_probePlot->setTrajectory(trajectory);
    m_probeReason.clear();
    m_probeHasRange = !trajectory.surfaceTemperature_K.empty();
    if (m_probeHasRange) {
        const auto [lo, hi] = std::minmax_element(trajectory.surfaceTemperature_K.begin(),
                                                  trajectory.surfaceTemperature_K.end());
        m_probeLow_K = *lo;
        m_probeHigh_K = *hi;
    }
    updateProbeCaption();
}

void ThermalPanel::clearProbe(const QString& reason) {
    m_probeElement = -1;
    m_probeHasRange = false;
    m_probeReason = reason;
    m_probePlot->clear();
    updateProbeCaption();
}

void ThermalPanel::updateProbeCaption() {
    // Rebuilt from the values rather than kept as text, so a language switch
    // re-words it. A reason from the shell was translated when it was given.
    if (m_probeElement < 0) {
        m_probeCaption->setText(m_probeReason.isEmpty()
                                    ? tr("Click a surface to see its day and what heated it.")
                                    : m_probeReason);
    } else if (!m_probeHasRange) {
        m_probeCaption->setText(tr("Element %1: nothing to show.").arg(m_probeElement));
    } else {
        m_probeCaption->setText(tr("Element %1, %2 to %3 K over the day.")
                                    .arg(m_probeElement)
                                    .arg(m_probeLow_K, 0, 'f', 1)
                                    .arg(m_probeHigh_K, 0, 'f', 1));
    }
}

double ThermalPanel::time() const {
    return m_timeSpin->value();
}

void ThermalPanel::onEnabledToggled(bool checked) {
    if (!m_suppressSignals) {
        emit thermalEnabledChanged(checked);
    }
}

void ThermalPanel::onTimeSliderChanged(int minutes) {
    if (m_suppressSignals) return;
    const double hours = static_cast<double>(minutes) / 60.0;
    m_suppressSignals = true;
    m_timeSpin->setValue(hours);
    m_suppressSignals = false;
    emit thermalTimeChanged(hours);
}

void ThermalPanel::onTimeSpinChanged(double hours) {
    if (m_suppressSignals) return;
    m_suppressSignals = true;
    m_timeSlider->setValue(static_cast<int>(hours * 60.0));
    m_suppressSignals = false;
    emit thermalTimeChanged(hours);
}

void ThermalPanel::onParamChanged() {
    if (!m_suppressSignals) {
        emitParams();
    }
}

void ThermalPanel::emitParams() {
    emit thermalParamsChanged(params());
}

void ThermalPanel::onBrowseForcing() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Forcing CSV"), QString(), tr("CSV files (*.csv);;All files (*)"));
    if (!path.isEmpty()) {
        m_forcingFileEdit->setText(path);
        emitParams();
    }
}
