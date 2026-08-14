/**
 * @file SensorPanel.cpp
 * @brief Panel for sensor simulation configuration - Implementation
 *
 * @author blitzcolo
 */

#include "SensorPanel.hpp"

#include "../ui/CollapsibleGroupBox.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <utility>

SensorPanel::SensorPanel(QWidget* parent)
    : PanelBase(parent)
{
    setObjectName(panelId());
    setupUi();
    retranslateUi();
}

QString SensorPanel::panelTitle() const {
    return tr("Sensor");
}

QLabel* SensorPanel::addRow(QFormLayout* layout, const char* source, QWidget* field,
                            const char* tip) {
    auto* label = new QLabel();
    m_captions.append({label, source, field, tip});
    layout->addRow(label, field);
    return label;
}

void SensorPanel::retranslateUi() {
    PanelBase::retranslateUi();

    // Every source string here is byte-identical to the one it replaced: this
    // panel's Chinese was already complete, and rewording a caption would have
    // thrown that translation away for no gain.
    m_opticsGroup->setTitle(tr("Optics"));
    m_detectorGroup->setTitle(tr("Detector"));
    m_adcGroup->setTitle(tr("ADC"));
    m_noiseGroup->setTitle(tr("Noise Model"));
    m_fpnGroup->setTitle(tr("FPN Parameters"));
    m_irGroup->setTitle(tr("IR Detector"));

    m_enabledCheck->setText(tr("Enable Sensor Simulation"));

    // Not a caption, so the loop below does not reach it: the sentinel's label
    // is the spin box's own special-value text.
    m_psfSigma->setSpecialValueText(tr("Auto (diffraction)"));

    m_poissonNoise->setText(tr("Photon Shot Noise (Poisson)"));
    m_readNoiseEnable->setText(tr("Enable Read Noise"));
    m_darkCurrentEnable->setText(tr("Enable Dark Current"));
    m_fpnNoise->setText(tr("Fixed Pattern Noise (FPN)"));
    m_nucEnable->setText(tr("Enable NUC"));

    for (const Caption& caption : std::as_const(m_captions)) {
        caption.label->setText(tr(caption.source));
        if (caption.tip) {
            const QString tip = tr(caption.tip);
            caption.label->setToolTip(tip);
            if (caption.field) {
                caption.field->setToolTip(tip);
            }
        }
    }
}

void SensorPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);

    // Enable checkbox
    m_enabledCheck = new QCheckBox();
    m_enabledCheck->setChecked(false);
    mainLayout->addWidget(m_enabledCheck);

    // ========================================================================
    // Optics Group
    // ========================================================================
    m_opticsGroup = new QGroupBox();
    auto* opticsLayout = new QFormLayout(m_opticsGroup);

    m_focalLength = new QDoubleSpinBox();
    m_focalLength->setRange(1.0, 10000.0);
    m_focalLength->setDecimals(1);
    m_focalLength->setSingleStep(1.0);
    m_focalLength->setSuffix(" mm");
    m_focalLength->setValue(50.0);
    addRow(opticsLayout, QT_TR_NOOP("Focal Length:"), m_focalLength,
           QT_TR_NOOP("Lens focal length. With the pixel pitch it sets the angular size of a pixel, and so how much of the scene one pixel averages."));

    m_fNumber = new QDoubleSpinBox();
    m_fNumber->setRange(0.5, 64.0);
    m_fNumber->setDecimals(1);
    m_fNumber->setSingleStep(0.1);
    m_fNumber->setPrefix("f/");
    m_fNumber->setValue(2.8);
    addRow(opticsLayout, QT_TR_NOOP("Aperture:"), m_fNumber,
           QT_TR_NOOP("f-number, focal length divided by entrance pupil diameter. Lower collects more light: irradiance on the detector goes as 1/(1 + 4 f-number squared)."));

    // -1 is the sentinel for "derive from diffraction", shown as special text
    // rather than as a number. 0 has to stay reachable and mean no blur, which
    // is why the sentinel is negative.
    m_psfSigma = new QDoubleSpinBox();
    m_psfSigma->setRange(-1.0, 100.0);
    m_psfSigma->setDecimals(3);
    m_psfSigma->setSingleStep(0.1);
    m_psfSigma->setSuffix(" px");
    m_psfSigma->setValue(-1.0);
    m_psfSigma->setSpecialValueText(tr("Auto (diffraction)"));
    addRow(opticsLayout, QT_TR_NOOP("PSF Width:"), m_psfSigma,
           QT_TR_NOOP("Gaussian blur width of the point spread function, in pixels. Auto derives it from the aperture and wavelength; setting it here holds the blur fixed while the aperture varies, and 0 disables it."));

    mainLayout->addWidget(m_opticsGroup);

    // ========================================================================
    // Detector Group
    // ========================================================================
    m_detectorGroup = new QGroupBox();
    auto* detectorLayout = new QFormLayout(m_detectorGroup);

    m_pixelPitch = new QDoubleSpinBox();
    m_pixelPitch->setRange(0.1, 100.0);
    m_pixelPitch->setDecimals(2);
    m_pixelPitch->setSingleStep(0.1);
    m_pixelPitch->setSuffix(QString::fromUtf8(" \u03BCm"));  // μm
    m_pixelPitch->setValue(5.0);
    addRow(detectorLayout, QT_TR_NOOP("Pixel Pitch:"), m_pixelPitch,
           QT_TR_NOOP("Centre-to-centre spacing of the detector elements. Sets how much area collects photons for one pixel."));

    m_quantumEfficiency = new QDoubleSpinBox();
    m_quantumEfficiency->setRange(0.0, 1.0);
    m_quantumEfficiency->setDecimals(2);
    m_quantumEfficiency->setSingleStep(0.01);
    m_quantumEfficiency->setValue(0.8);
    addRow(detectorLayout, QT_TR_NOOP("Quantum Efficiency:"), m_quantumEfficiency,
           QT_TR_NOOP("Fraction of arriving photons that become signal electrons. 1.0 would convert every photon."));

    m_wellCapacity = new QDoubleSpinBox();
    m_wellCapacity->setRange(100, 1e9);
    m_wellCapacity->setDecimals(0);
    m_wellCapacity->setSingleStep(1000);
    m_wellCapacity->setSuffix(" e-");
    m_wellCapacity->setValue(50000);
    addRow(detectorLayout, QT_TR_NOOP("Well Capacity:"), m_wellCapacity,
           QT_TR_NOOP("Electrons a pixel can hold before it saturates. Anything brighter clips to white."));

    m_bitDepth = new QSpinBox();
    m_bitDepth->setRange(8, 32);
    m_bitDepth->setValue(14);
    m_bitDepth->setSuffix(" bit");
    addRow(detectorLayout, QT_TR_NOOP("Bit Depth:"), m_bitDepth,
           QT_TR_NOOP("Bits per pixel out of the converter. Sets how finely the electron count is quantised."));

    m_integrationTime = new QDoubleSpinBox();
    m_integrationTime->setRange(0.0001, 10.0);
    m_integrationTime->setDecimals(4);
    m_integrationTime->setSingleStep(0.001);
    m_integrationTime->setSuffix(" s");
    m_integrationTime->setValue(0.01);
    addRow(detectorLayout, QT_TR_NOOP("Integration Time:"), m_integrationTime,
           QT_TR_NOOP("How long the detector collects per frame. Longer gathers more signal and more dark current with it."));

    mainLayout->addWidget(m_detectorGroup);

    // ========================================================================
    // ADC Group
    // ========================================================================
    m_adcGroup = new QGroupBox();
    auto* adcLayout = new QFormLayout(m_adcGroup);

    m_gain = new QDoubleSpinBox();
    m_gain->setRange(0.1, 100.0);
    m_gain->setDecimals(1);
    m_gain->setSingleStep(0.1);
    m_gain->setSuffix(" e-/DN");
    m_gain->setValue(3.0);
    addRow(adcLayout, QT_TR_NOOP("Gain:"), m_gain,
           QT_TR_NOOP("Electrons per digital number. Lower means finer steps, at the cost of clipping sooner."));

    mainLayout->addWidget(m_adcGroup);

    // ========================================================================
    // Noise Group
    // ========================================================================
    m_noiseGroup = new CollapsibleGroupBox();
    auto* noiseLayout = new QFormLayout();

    m_readNoise = new QDoubleSpinBox();
    m_readNoise->setRange(0.0, 1000.0);
    m_readNoise->setDecimals(1);
    m_readNoise->setSingleStep(0.1);
    m_readNoise->setSuffix(" e- RMS");
    m_readNoise->setValue(10.0);
    addRow(noiseLayout, QT_TR_NOOP("Read Noise:"), m_readNoise,
           QT_TR_NOOP("Noise the readout electronics add per pixel, in electrons RMS. Independent of exposure -- it is what limits the darkest tones."));

    m_darkCurrent = new QDoubleSpinBox();
    m_darkCurrent->setRange(0.0, 10000.0);
    m_darkCurrent->setDecimals(1);
    m_darkCurrent->setSingleStep(1.0);
    m_darkCurrent->setSuffix(" e-/s");
    m_darkCurrent->setValue(50.0);
    addRow(noiseLayout, QT_TR_NOOP("Dark Current:"), m_darkCurrent,
           QT_TR_NOOP("Electrons generated thermally per second with no light at all. Multiplied by the integration time, and roughly doubles every 7 K."));

    m_poissonNoise = new QCheckBox();
    m_poissonNoise->setChecked(true);
    noiseLayout->addRow(m_poissonNoise);

    m_readNoiseEnable = new QCheckBox();
    m_readNoiseEnable->setChecked(true);
    noiseLayout->addRow(m_readNoiseEnable);

    m_darkCurrentEnable = new QCheckBox();
    m_darkCurrentEnable->setChecked(true);
    noiseLayout->addRow(m_darkCurrentEnable);

    m_fpnNoise = new QCheckBox();
    m_fpnNoise->setChecked(false);
    noiseLayout->addRow(m_fpnNoise);

    m_noiseGroup->setContentLayout(noiseLayout);
    mainLayout->addWidget(m_noiseGroup);

    // ========================================================================
    // FPN Parameters Group (enabled when FPN is checked)
    // ========================================================================
    m_fpnGroup = new CollapsibleGroupBox();
    auto* fpnLayout = new QFormLayout();

    m_prnuSigma = new QDoubleSpinBox();
    m_prnuSigma->setRange(0.001, 0.5);
    m_prnuSigma->setDecimals(3);
    m_prnuSigma->setSingleStep(0.001);
    m_prnuSigma->setValue(0.01);
    addRow(fpnLayout, QT_TR_NOOP("PRNU Sigma:"), m_prnuSigma,
           QT_TR_NOOP("Photo-Response Non-Uniformity: pixel-to-pixel spread in sensitivity, as a fraction. A fixed multiplicative pattern, visible in bright areas."));

    m_dsnuSigma = new QDoubleSpinBox();
    m_dsnuSigma->setRange(0.1, 1000.0);
    m_dsnuSigma->setDecimals(1);
    m_dsnuSigma->setSingleStep(1.0);
    m_dsnuSigma->setSuffix(" e-");
    m_dsnuSigma->setValue(5.0);
    addRow(fpnLayout, QT_TR_NOOP("DSNU Sigma:"), m_dsnuSigma,
           QT_TR_NOOP("Dark Signal Non-Uniformity: pixel-to-pixel spread in dark current, in electrons. A fixed additive pattern, visible in dark areas."));

    m_nucEnable = new QCheckBox();
    m_nucEnable->setChecked(false);
    fpnLayout->addRow(m_nucEnable);

    m_nucEfficiency = new QDoubleSpinBox();
    m_nucEfficiency->setRange(0.0, 1.0);
    m_nucEfficiency->setDecimals(2);
    m_nucEfficiency->setSingleStep(0.01);
    m_nucEfficiency->setValue(0.98);
    m_nucEfficiency->setEnabled(false);  // Disabled until NUC is enabled
    addRow(fpnLayout, QT_TR_NOOP("NUC Efficiency:"), m_nucEfficiency,
           QT_TR_NOOP("How much of the fixed pattern the Non-Uniformity Correction removes. 1.0 removes all of it, which no real calibration does."));

    m_fpnGroup->setContentLayout(fpnLayout);
    m_fpnGroup->setEnabled(false);  // Disabled until FPN is enabled
    mainLayout->addWidget(m_fpnGroup);

    // ========================================================================
    // IR Detector Group
    // ========================================================================
    m_irGroup = new QGroupBox();
    auto* irLayout = new QFormLayout(m_irGroup);

    m_detectorTemp = new QDoubleSpinBox();
    m_detectorTemp->setRange(1.0, 400.0);
    m_detectorTemp->setDecimals(1);
    m_detectorTemp->setSingleStep(1.0);
    m_detectorTemp->setSuffix(" K");
    m_detectorTemp->setValue(77.0);
    addRow(irLayout, QT_TR_NOOP("Detector Temperature:"), m_detectorTemp,
           QT_TR_NOOP("Temperature of the detector itself. Drives dark current, and for thermal bands the self-emission the optics see."));

    mainLayout->addWidget(m_irGroup);

    mainLayout->addStretch();

    // ========================================================================
    // Connect signals
    // ========================================================================
    connect(m_enabledCheck, &QCheckBox::toggled,
            this, &SensorPanel::onEnabledChanged);

    // Optics params
    connect(m_focalLength, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SensorPanel::onParamChanged);
    connect(m_fNumber, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SensorPanel::onParamChanged);
    connect(m_psfSigma, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SensorPanel::onParamChanged);

    // Detector params
    connect(m_pixelPitch, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SensorPanel::onParamChanged);
    connect(m_quantumEfficiency, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SensorPanel::onParamChanged);
    connect(m_wellCapacity, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SensorPanel::onParamChanged);
    connect(m_bitDepth, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SensorPanel::onParamChanged);
    connect(m_integrationTime, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SensorPanel::onParamChanged);

    // ADC params
    connect(m_gain, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SensorPanel::onParamChanged);

    // Noise params
    connect(m_readNoise, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SensorPanel::onParamChanged);
    connect(m_darkCurrent, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SensorPanel::onParamChanged);
    connect(m_poissonNoise, &QCheckBox::toggled,
            this, &SensorPanel::onParamChanged);
    connect(m_readNoiseEnable, &QCheckBox::toggled,
            this, &SensorPanel::onParamChanged);
    connect(m_darkCurrentEnable, &QCheckBox::toggled,
            this, &SensorPanel::onParamChanged);
    connect(m_fpnNoise, &QCheckBox::toggled,
            this, &SensorPanel::onFpnToggled);

    // FPN params
    connect(m_prnuSigma, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SensorPanel::onParamChanged);
    connect(m_dsnuSigma, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SensorPanel::onParamChanged);
    connect(m_nucEnable, &QCheckBox::toggled,
            this, &SensorPanel::onNucToggled);
    connect(m_nucEfficiency, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SensorPanel::onParamChanged);

    // IR params
    connect(m_detectorTemp, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SensorPanel::onParamChanged);

    // Fold the two calibrate-once groups away by default. The panel is the
    // tallest in the application and every value inside them is set once and
    // then left alone.
    m_noiseGroup->setCollapsed(true);
    m_fpnGroup->setCollapsed(true);

    // Update enabled state of groups
    m_opticsGroup->setEnabled(false);
    m_detectorGroup->setEnabled(false);
    m_adcGroup->setEnabled(false);
    m_noiseGroup->setEnabled(false);
    m_fpnGroup->setEnabled(false);
    m_irGroup->setEnabled(false);
}

void SensorPanel::setSensorEnabled(bool enabled) {
    m_updatingUi = true;
    m_enabledCheck->setChecked(enabled);
    m_opticsGroup->setEnabled(enabled);
    m_detectorGroup->setEnabled(enabled);
    m_adcGroup->setEnabled(enabled);
    m_noiseGroup->setEnabled(enabled);
    m_fpnGroup->setEnabled(enabled && m_fpnNoise->isChecked());
    m_irGroup->setEnabled(enabled);
    m_updatingUi = false;
}

bool SensorPanel::isSensorEnabled() const {
    return m_enabledCheck->isChecked();
}

void SensorPanel::setSensorParams(const quantiloom::SensorParams& params) {
    m_params = params;
    updateUiFromParams(params);
}

quantiloom::SensorParams SensorPanel::getSensorParams() const {
    return m_params;
}

void SensorPanel::onEnabledChanged(bool enabled) {
    m_opticsGroup->setEnabled(enabled);
    m_detectorGroup->setEnabled(enabled);
    m_adcGroup->setEnabled(enabled);
    m_noiseGroup->setEnabled(enabled);
    m_fpnGroup->setEnabled(enabled && m_fpnNoise->isChecked());
    m_irGroup->setEnabled(enabled);

    if (!m_updatingUi) {
        emit enabledChanged(enabled);
    }
}

void SensorPanel::onFpnToggled(bool checked) {
    m_fpnGroup->setEnabled(m_enabledCheck->isChecked() && checked);
    onParamChanged();
}

void SensorPanel::onNucToggled(bool checked) {
    m_nucEfficiency->setEnabled(checked);
    onParamChanged();
}

void SensorPanel::onParamChanged() {
    if (m_updatingUi) return;

    // Optics
    m_params.focalLength_mm = static_cast<float>(m_focalLength->value());
    m_params.fNumber = static_cast<float>(m_fNumber->value());
    m_params.psfSigma_px = static_cast<float>(m_psfSigma->value());

    // Detector
    m_params.pixelPitch_um = static_cast<float>(m_pixelPitch->value());
    m_params.quantumEfficiency = static_cast<float>(m_quantumEfficiency->value());
    m_params.wellCapacity_e = static_cast<float>(m_wellCapacity->value());
    m_params.bitDepth = static_cast<quantiloom::u32>(m_bitDepth->value());
    m_params.integrationTime_s = static_cast<float>(m_integrationTime->value());

    // ADC
    m_params.gain = static_cast<float>(m_gain->value());

    // Noise
    m_params.readNoise_e_rms = static_cast<float>(m_readNoise->value());
    m_params.darkCurrent_e_s = static_cast<float>(m_darkCurrent->value());
    m_params.enablePoissonNoise = m_poissonNoise->isChecked();
    m_params.enableReadNoise = m_readNoiseEnable->isChecked();
    m_params.enableDarkCurrent = m_darkCurrentEnable->isChecked();
    m_params.enableFPN = m_fpnNoise->isChecked();

    // FPN
    m_params.prnuSigma = static_cast<float>(m_prnuSigma->value());
    m_params.dsnuSigma_e = static_cast<float>(m_dsnuSigma->value());
    m_params.enableNUC = m_nucEnable->isChecked();
    m_params.nucEfficiency = static_cast<float>(m_nucEfficiency->value());

    // IR
    m_params.detectorTemperature_K = static_cast<float>(m_detectorTemp->value());

    emit paramsChanged(m_params);
}

void SensorPanel::updateUiFromParams(const quantiloom::SensorParams& params) {
    m_updatingUi = true;
    blockSignalsForUpdate(true);

    // Optics
    m_focalLength->setValue(static_cast<double>(params.focalLength_mm));
    m_fNumber->setValue(static_cast<double>(params.fNumber));
    // Any negative value is the same sentinel; clamp so the box lands on its
    // special-value text rather than on an out-of-range number.
    m_psfSigma->setValue(params.psfSigma_px < 0.0f
                             ? m_psfSigma->minimum()
                             : static_cast<double>(params.psfSigma_px));

    // Detector
    m_pixelPitch->setValue(static_cast<double>(params.pixelPitch_um));
    m_quantumEfficiency->setValue(static_cast<double>(params.quantumEfficiency));
    m_wellCapacity->setValue(static_cast<double>(params.wellCapacity_e));
    m_bitDepth->setValue(static_cast<int>(params.bitDepth));
    m_integrationTime->setValue(static_cast<double>(params.integrationTime_s));

    // ADC
    m_gain->setValue(static_cast<double>(params.gain));

    // Noise
    m_readNoise->setValue(static_cast<double>(params.readNoise_e_rms));
    m_darkCurrent->setValue(static_cast<double>(params.darkCurrent_e_s));
    m_poissonNoise->setChecked(params.enablePoissonNoise);
    m_readNoiseEnable->setChecked(params.enableReadNoise);
    m_darkCurrentEnable->setChecked(params.enableDarkCurrent);
    m_fpnNoise->setChecked(params.enableFPN);

    // FPN
    m_prnuSigma->setValue(static_cast<double>(params.prnuSigma));
    m_dsnuSigma->setValue(static_cast<double>(params.dsnuSigma_e));
    m_nucEnable->setChecked(params.enableNUC);
    m_nucEfficiency->setValue(static_cast<double>(params.nucEfficiency));
    m_nucEfficiency->setEnabled(params.enableNUC);

    // IR
    m_detectorTemp->setValue(static_cast<double>(params.detectorTemperature_K));

    // Update FPN group enabled state
    m_fpnGroup->setEnabled(m_enabledCheck->isChecked() && params.enableFPN);

    blockSignalsForUpdate(false);
    m_updatingUi = false;
}

void SensorPanel::blockSignalsForUpdate(bool block) {
    // Optics
    m_focalLength->blockSignals(block);
    m_fNumber->blockSignals(block);
    m_psfSigma->blockSignals(block);

    // Detector
    m_pixelPitch->blockSignals(block);
    m_quantumEfficiency->blockSignals(block);
    m_wellCapacity->blockSignals(block);
    m_bitDepth->blockSignals(block);
    m_integrationTime->blockSignals(block);

    // ADC
    m_gain->blockSignals(block);

    // Noise
    m_readNoise->blockSignals(block);
    m_darkCurrent->blockSignals(block);
    m_poissonNoise->blockSignals(block);
    m_readNoiseEnable->blockSignals(block);
    m_darkCurrentEnable->blockSignals(block);
    m_fpnNoise->blockSignals(block);

    // FPN
    m_prnuSigma->blockSignals(block);
    m_dsnuSigma->blockSignals(block);
    m_nucEnable->blockSignals(block);
    m_nucEfficiency->blockSignals(block);

    // IR
    m_detectorTemp->blockSignals(block);
}
