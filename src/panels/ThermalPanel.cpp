/**
 * @file ThermalPanel.cpp
 * @brief Thermal solve control panel
 */

#include "ThermalPanel.hpp"

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
    m_timeSpin->setRange(0.0, 48.0);
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
    m_initialCombo->addItem(QStringLiteral("Steady"), 0);
    m_initialCombo->addItem(QStringLiteral("Uniform"), 1);
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
    m_forcingBrowse = new QPushButton(QStringLiteral("..."));
    m_forcingBrowse->setFixedWidth(30);
    connect(m_forcingBrowse, &QPushButton::clicked, this, &ThermalPanel::onBrowseForcing);
    forcingRow->addWidget(m_forcingFileEdit, 1);
    forcingRow->addWidget(m_forcingBrowse);
    m_forcingCaption = new QLabel(m_paramsGroup);
    paramsLayout->addRow(m_forcingCaption, forcingRow);

    // Connect param changes
    auto paramSlot = [this] { onParamChanged(); };
    connect(m_startTimeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, paramSlot);
    connect(m_timestepSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, paramSlot);
    connect(m_layersSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, paramSlot);
    connect(m_initialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, paramSlot);
    connect(m_initialTempSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, paramSlot);
    connect(m_sunIrradianceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, paramSlot);
    connect(m_exchangeRaysSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, paramSlot);
    connect(m_exchangeTopKSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, paramSlot);
    connect(m_checkpointStrideSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, paramSlot);

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

    mainLayout->addStretch();

    retranslateUi();
}

void ThermalPanel::retranslateUi() {
    bindText([this] {
        m_enableCheck->setText(tr("Enable thermal solve"));
        m_timeGroup->setTitle(tr("Time of Day"));
        m_timeCaption->setText(tr("Simulation time"));
        m_paramsGroup->setTitle(tr("Solver Parameters"));
        m_startTimeCaption->setText(tr("Start time"));
        m_timestepCaption->setText(tr("Timestep"));
        m_layersCaption->setText(tr("Layers"));
        m_initialCaption->setText(tr("Initial condition"));
        m_initialTempCaption->setText(tr("Initial temperature"));
        m_sunIrradianceCaption->setText(tr("Solar irradiance"));
        m_exchangeRaysCaption->setText(tr("Exchange rays"));
        m_exchangeTopKCaption->setText(tr("Exchange top-K"));
        m_checkpointStrideCaption->setText(tr("Checkpoint stride"));
        m_forcingCaption->setText(tr("Forcing file"));
        m_statusGroup->setTitle(tr("Status"));
    });
}

void ThermalPanel::setSolveEnabled(bool enabled) {
    m_suppressSignals = true;
    m_enableCheck->setChecked(enabled);
    m_suppressSignals = false;
}

void ThermalPanel::setParams(const quantiloom::ThermalSolveParams& p) {
    m_suppressSignals = true;
    m_startTimeSpin->setValue(p.startTime_h);
    m_timestepSpin->setValue(p.timestep_s);
    m_layersSpin->setValue(static_cast<int>(p.layerCount));
    m_initialCombo->setCurrentIndex(
        p.initial == quantiloom::ThermalInitialCondition::Steady ? 0 : 1);
    m_initialTempSpin->setValue(p.initialTemperature_K);
    m_sunIrradianceSpin->setValue(p.sunIrradiance_W_m2);
    m_exchangeRaysSpin->setValue(static_cast<int>(p.exchangeRays));
    m_exchangeTopKSpin->setValue(static_cast<int>(p.exchangeTopK));
    m_checkpointStrideSpin->setValue(p.checkpointStride_h);
    m_forcingFileEdit->setText(QString::fromStdString(p.forcingFile));
    m_suppressSignals = false;
}

void ThermalPanel::setTime(double time_h) {
    m_suppressSignals = true;
    m_timeSpin->setValue(time_h);
    m_timeSlider->setValue(static_cast<int>(time_h * 60.0));
    m_suppressSignals = false;
}

void ThermalPanel::updateStatus(const quantiloom::ThermalSolveStatus& s) {
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
        tr("Exchange: %1 entries (%2 runs)")
            .arg(s.exchangeNonZeros).arg(s.exchangeRunCount));
    m_statusStepper->setText(
        tr("Stepper: %1").arg(QString::fromStdString(s.stepperName)));
    if (s.error.empty()) {
        m_statusError->clear();
        m_statusError->hide();
    } else {
        m_statusError->setText(QString::fromStdString(s.error));
        m_statusError->show();
    }

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

bool ThermalPanel::isSolveEnabled() const {
    return m_enableCheck->isChecked();
}

quantiloom::ThermalSolveParams ThermalPanel::params() const {
    quantiloom::ThermalSolveParams p;
    p.startTime_h = m_startTimeSpin->value();
    p.timestep_s = m_timestepSpin->value();
    p.layerCount = static_cast<quantiloom::u32>(m_layersSpin->value());
    p.initial = m_initialCombo->currentIndex() == 0
                    ? quantiloom::ThermalInitialCondition::Steady
                    : quantiloom::ThermalInitialCondition::Uniform;
    p.initialTemperature_K = m_initialTempSpin->value();
    p.sunIrradiance_W_m2 = m_sunIrradianceSpin->value();
    p.exchangeRays = static_cast<quantiloom::u32>(m_exchangeRaysSpin->value());
    p.exchangeTopK = static_cast<quantiloom::u32>(m_exchangeTopKSpin->value());
    p.checkpointStride_h = m_checkpointStrideSpin->value();
    p.forcingFile = m_forcingFileEdit->text().toStdString();
    return p;
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
