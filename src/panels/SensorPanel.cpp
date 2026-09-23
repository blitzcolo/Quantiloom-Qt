/**
 * @file SensorPanel.cpp
 * @brief Panel for the versioned physical camera - Implementation
 *
 * @author blitzcolo
 */

#include "SensorPanel.hpp"

#include "../ui/CollapsibleGroupBox.hpp"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSignalBlocker>
#include <algorithm>
#include <utility>

#include <postprocess/CameraConfigIO.hpp>

namespace {

using quantiloom::camera::CameraConfig;
using quantiloom::camera::CameraPresetKind;
using quantiloom::camera::CfaPattern;
using quantiloom::camera::DetectorKind;
using quantiloom::camera::ShutterKind;

// The SDK owns the token grammar; the panel only needs index <-> enum maps
// for its combos, so the mapping lives here rather than duplicating SDK text.
constexpr CfaPattern kCfaValues[] = {
    CfaPattern::Mono, CfaPattern::RGGB, CfaPattern::GRBG,
    CfaPattern::GBRG, CfaPattern::BGGR, CfaPattern::MultiChannel,
};

int cfaIndex(CfaPattern c) {
    for (int i = 0; i < 6; ++i) {
        if (kCfaValues[i] == c) return i;
    }
    return 0;
}

constexpr quantiloom::DisplayToneMode kToneValues[] = {
    quantiloom::DisplayToneMode::Linear,
    quantiloom::DisplayToneMode::Equalize,
    quantiloom::DisplayToneMode::Clahe,
};

int toneIndex(quantiloom::DisplayToneMode t) {
    for (int i = 0; i < 3; ++i) {
        if (kToneValues[i] == t) return i;
    }
    return 0;
}

constexpr quantiloom::DisplayPalette kPaletteValues[] = {
    quantiloom::DisplayPalette::Grey,
    quantiloom::DisplayPalette::GreyInverted,
    quantiloom::DisplayPalette::Ironbow,
    quantiloom::DisplayPalette::Rainbow,
    quantiloom::DisplayPalette::Viridis,
};

int paletteIndex(quantiloom::DisplayPalette p) {
    for (int i = 0; i < 5; ++i) {
        if (kPaletteValues[i] == p) return i;
    }
    return 0;
}

/// Find the preset the config still matches, if any. The comparison is the
/// SDK's own serialization on both sides with the enabled flag normalised,
/// so "the user only toggled the switch" still reads as the same preset.
std::optional<CameraPresetKind> matchPreset(const CameraConfig& config) {
    CameraConfig mine = config;
    mine.enabled = true;
    const QString mineText =
        QString::fromStdString(quantiloom::CameraConfigToToml(mine));
    for (CameraPresetKind kind : quantiloom::camera::AllCameraPresets()) {
        auto preset = quantiloom::camera::MakePresetCameraConfig(kind);
        if (!preset) continue;
        preset.value().enabled = true;
        if (QString::fromStdString(quantiloom::CameraConfigToToml(preset.value())) == mineText) {
            return kind;
        }
    }
    return std::nullopt;
}

} // namespace

SensorPanel::SensorPanel(QWidget* parent)
    : PanelBase(parent)
{
    setObjectName(panelId());
    setupUi();
    retranslateUi();
}

QString SensorPanel::panelTitle() const {
    return tr("Camera");
}

QLabel* SensorPanel::addRow(QFormLayout* layout, const char* source, QWidget* field,
                            const char* tip) {
    auto* label = new QLabel();
    m_captions.append({label, source, field, tip});
    layout->addRow(label, field);
    return label;
}

QDoubleSpinBox* SensorPanel::makeSpin(double min, double max, double step,
                                      int decimals, double value, const char* suffix) {
    auto* spin = new QDoubleSpinBox();
    spin->setRange(min, max);
    spin->setDecimals(decimals);
    spin->setSingleStep(step);
    if (suffix) {
        spin->setSuffix(QString::fromUtf8(suffix));
    }
    spin->setValue(value);
    return spin;
}

void SensorPanel::retranslateUi() {
    PanelBase::retranslateUi();

    m_deviceGroup->setTitle(tr("Detector"));
    m_opticsGroup->setTitle(tr("Optics"));
    m_readoutGroup->setTitle(tr("Readout"));
    m_photonGroup->setTitle(tr("Photon Detector"));
    m_thermalGroup->setTitle(tr("Thermal Detector"));
    m_ispGroup->setTitle(tr("Image Signal Processor"));
    m_irDisplayGroup->setTitle(tr("Infrared Display"));
    m_statusGroup->setTitle(tr("Coverage and Calibration"));
    m_thermographyGroup->setTitle(tr("Thermography"));

    m_enabledCheck->setText(tr("Enable Camera Simulation"));

    // Not captions, so the loop below does not reach them: the PSF sentinel's
    // label is the spin box's own special-value text.
    m_psfSigma->setSpecialValueText(tr("Auto (Airy)"));

    m_shotNoiseEnable->setText(tr("Photon Shot Noise (Poisson)"));
    m_readNoiseEnable->setText(tr("Enable Read Noise"));
    m_darkCurrentEnable->setText(tr("Enable Dark Current"));
    m_fpnNoise->setText(tr("Fixed Pattern Noise (FPN)"));
    m_nucEnable->setText(tr("Enable NUC"));
    m_autoExposure->setText(tr("Auto Exposure"));
    m_autoWhiteBalance->setText(tr("Auto White Balance"));
    m_denoise->setText(tr("Denoise"));
    m_sharpen->setText(tr("Sharpen"));
    m_empiricalNoise->setText(tr("Empirical Display Noise"));
    m_temporalDrift->setText(tr("Temporal Drift"));
    m_thermographyEnable->setText(tr("Write a temperature map when rendering"));

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

    // Combo items are user-visible text too; refill them in the current
    // language, preserving the selection.
    {
        const int keep = m_presetCombo->currentIndex();
        const QSignalBlocker blocker(m_presetCombo);
        m_presetCombo->clear();
        m_presetCombo->addItem(tr("Custom (current values)"));
        for (CameraPresetKind kind : quantiloom::camera::AllCameraPresets()) {
            m_presetCombo->addItem(QString::fromStdString(
                quantiloom::camera::CameraPresetDisplayName(kind)));
        }
        m_presetCombo->setCurrentIndex(keep >= 0 ? keep : 0);
    }
    {
        const int keep = m_detectorKind->currentIndex();
        const QSignalBlocker blocker(m_detectorKind);
        m_detectorKind->clear();
        m_detectorKind->addItem(tr("Photon counting"));
        m_detectorKind->addItem(tr("Thermal (bolometer)"));
        m_detectorKind->setCurrentIndex(keep >= 0 ? keep : 0);
    }
    {
        const int keep = m_cfaCombo->currentIndex();
        const QSignalBlocker blocker(m_cfaCombo);
        m_cfaCombo->clear();
        m_cfaCombo->addItem(tr("Monochrome"));
        m_cfaCombo->addItem(QStringLiteral("RGGB"));
        m_cfaCombo->addItem(QStringLiteral("GRBG"));
        m_cfaCombo->addItem(QStringLiteral("GBRG"));
        m_cfaCombo->addItem(QStringLiteral("BGGR"));
        m_cfaCombo->addItem(tr("Multi-channel"));
        m_cfaCombo->setCurrentIndex(keep >= 0 ? keep : 0);
    }
    {
        const int keep = m_shutterCombo->currentIndex();
        const QSignalBlocker blocker(m_shutterCombo);
        m_shutterCombo->clear();
        m_shutterCombo->addItem(tr("Global"));
        m_shutterCombo->addItem(tr("Rolling"));
        m_shutterCombo->setCurrentIndex(keep >= 0 ? keep : 0);
    }
    {
        const int keep = m_irToneCombo->currentIndex();
        const QSignalBlocker blocker(m_irToneCombo);
        m_irToneCombo->clear();
        m_irToneCombo->addItem(tr("Linear AGC"));
        m_irToneCombo->addItem(tr("Histogram Equalize"));
        m_irToneCombo->addItem(tr("CLAHE"));
        m_irToneCombo->setCurrentIndex(keep >= 0 ? keep : 0);
    }
    {
        const int keep = m_irPaletteCombo->currentIndex();
        const QSignalBlocker blocker(m_irPaletteCombo);
        m_irPaletteCombo->clear();
        m_irPaletteCombo->addItem(tr("White-hot"));
        m_irPaletteCombo->addItem(tr("Black-hot"));
        m_irPaletteCombo->addItem(tr("Ironbow"));
        m_irPaletteCombo->addItem(tr("Rainbow"));
        m_irPaletteCombo->addItem(tr("Viridis"));
        m_irPaletteCombo->setCurrentIndex(keep >= 0 ? keep : 0);
    }

    updateStatusArea();
}

void SensorPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);

    // Enable checkbox
    m_enabledCheck = new QCheckBox();
    m_enabledCheck->setChecked(false);
    mainLayout->addWidget(m_enabledCheck);

    // ========================================================================
    // Preset row
    // ========================================================================
    auto* presetLayout = new QFormLayout();
    m_presetCombo = new QComboBox();
    // Items are filled by retranslateUi().
    presetLayout->addRow(m_presetCombo);
    mainLayout->addLayout(presetLayout);

    // ========================================================================
    // Detector group
    // ========================================================================
    m_deviceGroup = new QGroupBox();
    auto* deviceLayout = new QFormLayout(m_deviceGroup);

    m_detectorKind = new QComboBox();
    deviceLayout->addRow(m_detectorKind);

    m_cfaCombo = new QComboBox();
    addRow(deviceLayout, QT_TR_NOOP("Colour Filter Array:"), m_cfaCombo,
           QT_TR_NOOP("How one scalar per pixel maps to colour channels. Monochrome reads one channel; Bayer patterns alternate R/G/B filters; multi-channel reads up to three channels per pixel."));

    mainLayout->addWidget(m_deviceGroup);

    // ========================================================================
    // Optics group
    // ========================================================================
    m_opticsGroup = new QGroupBox();
    auto* opticsLayout = new QFormLayout(m_opticsGroup);

    m_focalLength = makeSpin(1.0, 10000.0, 1.0, 1, 50.0, " mm");
    addRow(opticsLayout, QT_TR_NOOP("Focal Length:"), m_focalLength,
           QT_TR_NOOP("Lens focal length. With the pixel pitch it sets the angular size of a pixel, and so how much of the scene one pixel averages."));

    m_fNumber = makeSpin(0.5, 64.0, 0.1, 1, 2.8, nullptr);
    m_fNumber->setPrefix(QStringLiteral("f/"));
    addRow(opticsLayout, QT_TR_NOOP("Aperture:"), m_fNumber,
           QT_TR_NOOP("f-number, focal length divided by entrance pupil diameter. Lower collects more light: irradiance on the detector goes as 1/(1 + 4 f-number squared)."));

    m_pixelPitch = makeSpin(0.1, 100.0, 0.1, 2, 5.0, " um");
    addRow(opticsLayout, QT_TR_NOOP("Pixel Pitch:"), m_pixelPitch,
           QT_TR_NOOP("Centre-to-centre spacing of the detector elements. Sets how much area collects photons for one pixel."));

    // -1 is the sentinel for "derive from diffraction", shown as special text
    // rather than as a number.
    m_psfSigma = makeSpin(-1.0, 100.0, 0.1, 3, -1.0, " px");
    addRow(opticsLayout, QT_TR_NOOP("PSF Width:"), m_psfSigma,
           QT_TR_NOOP("Gaussian blur width of the point spread function, in pixels. Auto derives it from the aperture and wavelength; setting it here holds the blur fixed while the aperture varies, and 0 disables it."));

    mainLayout->addWidget(m_opticsGroup);

    // ========================================================================
    // Readout group
    // ========================================================================
    m_readoutGroup = new QGroupBox();
    auto* readoutLayout = new QFormLayout(m_readoutGroup);

    m_shutterCombo = new QComboBox();
    addRow(readoutLayout, QT_TR_NOOP("Shutter:"), m_shutterCombo,
           QT_TR_NOOP("Global exposes every pixel at once; rolling exposes row by row, which shears moving edges by the row delay."));

    m_rowDelay = makeSpin(0.0, 10.0, 1e-5, 6, 0.0, " s");
    addRow(readoutLayout, QT_TR_NOOP("Row Delay:"), m_rowDelay,
           QT_TR_NOOP("Time between the starts of consecutive rows under a rolling shutter. The exposure of the last row starts this much later than the first."));

    m_exposureTime = makeSpin(1e-6, 10.0, 0.001, 6, 0.01, " s");
    addRow(readoutLayout, QT_TR_NOOP("Exposure Time:"), m_exposureTime,
           QT_TR_NOOP("How long the detector collects per frame. Longer gathers more signal and more dark current with it."));

    m_framePeriod = makeSpin(1e-4, 10.0, 0.001, 6, 1.0 / 30.0, " s");
    addRow(readoutLayout, QT_TR_NOOP("Frame Period:"), m_framePeriod,
           QT_TR_NOOP("Time from one frame's exposure midpoint to the next; the reciprocal is the frame rate."));

    m_analogGain = makeSpin(0.1, 100.0, 0.1, 2, 1.0, nullptr);
    addRow(readoutLayout, QT_TR_NOOP("Analog Gain:"), m_analogGain,
           QT_TR_NOOP("Gain applied before the converter. Multiplies signal and read noise together, so it does not add information -- it trades well depth for brightness."));

    m_electronsPerDn = makeSpin(0.01, 1000.0, 0.1, 3, 1.0, " e-/DN");
    addRow(readoutLayout, QT_TR_NOOP("Conversion Gain:"), m_electronsPerDn,
           QT_TR_NOOP("Electrons per digital number. Lower means finer steps, at the cost of clipping sooner."));

    m_blackLevelDn = makeSpin(0.0, 10000.0, 1.0, 1, 0.0, " DN");
    addRow(readoutLayout, QT_TR_NOOP("Black Level:"), m_blackLevelDn,
           QT_TR_NOOP("Digital number the converter reports for a pixel that collected nothing. Everything below it clips to zero."));

    m_adcBits = new QSpinBox();
    m_adcBits->setRange(1, 32);
    m_adcBits->setValue(14);
    m_adcBits->setSuffix(QStringLiteral(" bit"));
    addRow(readoutLayout, QT_TR_NOOP("ADC Bits:"), m_adcBits,
           QT_TR_NOOP("Bits out of the analogue-to-digital converter. Sets how finely the electron count is quantised."));

    mainLayout->addWidget(m_readoutGroup);

    // ========================================================================
    // Photon detector group
    // ========================================================================
    m_photonGroup = new QGroupBox();
    auto* photonLayout = new QFormLayout(m_photonGroup);

    m_fullWell = makeSpin(1.0, 1e9, 1000.0, 0, 50000.0, " e-");
    addRow(photonLayout, QT_TR_NOOP("Full Well:"), m_fullWell,
           QT_TR_NOOP("Electrons a pixel can hold before it saturates. Anything brighter clips to white."));

    m_readNoise = makeSpin(0.0, 10000.0, 0.1, 2, 0.0, " e- RMS");
    addRow(photonLayout, QT_TR_NOOP("Read Noise:"), m_readNoise,
           QT_TR_NOOP("Noise the readout electronics add per pixel, in electrons RMS. Independent of exposure -- it is what limits the darkest tones."));

    m_darkCurrent = makeSpin(0.0, 1e6, 1.0, 2, 0.0, " e-/s");
    addRow(photonLayout, QT_TR_NOOP("Dark Current:"), m_darkCurrent,
           QT_TR_NOOP("Electrons generated thermally per second with no light at all. Multiplied by the exposure time, and roughly doubles every 7 K."));

    m_shotNoiseEnable = new QCheckBox();
    m_shotNoiseEnable->setChecked(true);
    photonLayout->addRow(m_shotNoiseEnable);

    m_readNoiseEnable = new QCheckBox();
    m_readNoiseEnable->setChecked(true);
    photonLayout->addRow(m_readNoiseEnable);

    m_darkCurrentEnable = new QCheckBox();
    m_darkCurrentEnable->setChecked(true);
    photonLayout->addRow(m_darkCurrentEnable);

    m_fpnNoise = new QCheckBox();
    m_fpnNoise->setChecked(false);
    photonLayout->addRow(m_fpnNoise);

    m_prnuSigma = makeSpin(0.0, 0.5, 0.001, 3, 0.0, nullptr);
    addRow(photonLayout, QT_TR_NOOP("PRNU Sigma:"), m_prnuSigma,
           QT_TR_NOOP("Photo-Response Non-Uniformity: pixel-to-pixel spread in sensitivity, as a fraction. A fixed multiplicative pattern, visible in bright areas."));

    m_dsnuSigma = makeSpin(0.0, 1e5, 1.0, 2, 0.0, " e-");
    addRow(photonLayout, QT_TR_NOOP("DSNU Sigma:"), m_dsnuSigma,
           QT_TR_NOOP("Dark Signal Non-Uniformity: pixel-to-pixel spread in dark current, in electrons. A fixed additive pattern, visible in dark areas."));

    m_nucEnable = new QCheckBox();
    m_nucEnable->setChecked(false);
    photonLayout->addRow(m_nucEnable);

    m_nucResidual = makeSpin(0.0, 1.0, 0.01, 2, 0.0, nullptr);
    addRow(photonLayout, QT_TR_NOOP("NUC Residual:"), m_nucResidual,
           QT_TR_NOOP("Fraction of the fixed pattern the Non-Uniformity Correction leaves behind. 0 removes all of it, which no real calibration does."));

    mainLayout->addWidget(m_photonGroup);

    // ========================================================================
    // Thermal detector group
    // ========================================================================
    m_thermalGroup = new QGroupBox();
    auto* thermalLayout = new QFormLayout(m_thermalGroup);

    m_timeConstant = makeSpin(0.0, 10.0, 0.001, 3, 0.008, " s");
    addRow(thermalLayout, QT_TR_NOOP("Time Constant:"), m_timeConstant,
           QT_TR_NOOP("First-order thermal response time of a pixel. How long a bolometer takes to settle after the scene in front of it changes."));

    m_responsivity = makeSpin(0.0, 1e12, 1000.0, 1, 1.0, " DN/W");
    addRow(thermalLayout, QT_TR_NOOP("Responsivity:"), m_responsivity,
           QT_TR_NOOP("Digital numbers out per watt of absorbed scene power. Sets the absolute scale of the thermal chain."));

    m_thermalReadNoise = makeSpin(0.0, 1e5, 0.1, 2, 0.0, " DN RMS");
    addRow(thermalLayout, QT_TR_NOOP("Read Noise (DN):"), m_thermalReadNoise,
           QT_TR_NOOP("Noise the readout adds per pixel, in digital numbers RMS. Independent of scene power."));

    m_drift = makeSpin(-1e4, 1e4, 0.1, 2, 0.0, " DN/s");
    addRow(thermalLayout, QT_TR_NOOP("Drift:"), m_drift,
           QT_TR_NOOP("Slow additive drift of the reported value, in DN per second. What makes a microbolometer wander between calibrations."));

    m_netd = makeSpin(0.0, 10000.0, 1.0, 1, 0.0, " mK");
    addRow(thermalLayout, QT_TR_NOOP("NETD:"), m_netd,
           QT_TR_NOOP("Noise-equivalent temperature difference at the reference temperature, in millikelvin. Only meaningful with the reference temperature and optical condition the datasheet specifies."));

    m_netdRefTemp = makeSpin(0.0, 1000.0, 1.0, 1, 0.0, " K");
    addRow(thermalLayout, QT_TR_NOOP("NETD Reference Temperature:"), m_netdRefTemp,
           QT_TR_NOOP("Scene temperature the NETD was quoted at. A NETD without its reference temperature attached means nothing."));

    mainLayout->addWidget(m_thermalGroup);

    // ========================================================================
    // ISP group
    // ========================================================================
    m_ispGroup = new QGroupBox();
    auto* ispLayout = new QFormLayout(m_ispGroup);

    m_autoExposure = new QCheckBox();
    m_autoExposure->setChecked(false);
    ispLayout->addRow(m_autoExposure);

    m_aeTarget = makeSpin(0.01, 1.0, 0.01, 2, 0.18, nullptr);
    addRow(ispLayout, QT_TR_NOOP("AE Target Luminance:"), m_aeTarget,
           QT_TR_NOOP("Mean display value the auto-exposure loop aims for, as a fraction of the well. The classic 18% grey-card value."));

    m_aeSmoothing = makeSpin(0.0, 1.0, 0.05, 2, 0.2, nullptr);
    addRow(ispLayout, QT_TR_NOOP("AE Smoothing:"), m_aeSmoothing,
           QT_TR_NOOP("Fraction of the computed exposure correction applied per acquisition. Lower damps oscillation at the cost of slower convergence; 1 disables smoothing."));

    m_aeMinExposure = makeSpin(1e-6, 10.0, 0.001, 6, 1e-5, " s");
    addRow(ispLayout, QT_TR_NOOP("AE Min Exposure:"), m_aeMinExposure,
           QT_TR_NOOP("Shortest exposure the auto-exposure loop may request."));

    m_aeMaxExposure = makeSpin(1e-6, 10.0, 0.001, 6, 1.0, " s");
    addRow(ispLayout, QT_TR_NOOP("AE Max Exposure:"), m_aeMaxExposure,
           QT_TR_NOOP("Longest exposure the auto-exposure loop may request."));

    m_aeMaxGain = makeSpin(1.0, 1000.0, 1.0, 1, 16.0, nullptr);
    addRow(ispLayout, QT_TR_NOOP("AE Max Gain:"), m_aeMaxGain,
           QT_TR_NOOP("Highest analog gain the auto-exposure loop may request."));

    m_autoWhiteBalance = new QCheckBox();
    m_autoWhiteBalance->setChecked(false);
    ispLayout->addRow(m_autoWhiteBalance);

    m_wbR = makeSpin(0.1, 10.0, 0.01, 2, 1.0, nullptr);
    addRow(ispLayout, QT_TR_NOOP("White Balance R:"), m_wbR,
           QT_TR_NOOP("Per-channel gain applied before the colour transform. Auto white balance drives these when it is on."));

    m_wbG = makeSpin(0.1, 10.0, 0.01, 2, 1.0, nullptr);
    addRow(ispLayout, QT_TR_NOOP("White Balance G:"), m_wbG,
           QT_TR_NOOP("Per-channel gain applied before the colour transform. Auto white balance drives these when it is on."));

    m_wbB = makeSpin(0.1, 10.0, 0.01, 2, 1.0, nullptr);
    addRow(ispLayout, QT_TR_NOOP("White Balance B:"), m_wbB,
           QT_TR_NOOP("Per-channel gain applied before the colour transform. Auto white balance drives these when it is on."));

    m_toneGamma = makeSpin(0.1, 10.0, 0.05, 2, 1.0, nullptr);
    addRow(ispLayout, QT_TR_NOOP("Tone Gamma:"), m_toneGamma,
           QT_TR_NOOP("Display transfer exponent applied in the ISP. 1 leaves the tone linear."));

    m_denoise = new QCheckBox();
    m_denoise->setChecked(false);
    ispLayout->addRow(m_denoise);

    m_denoiseStrength = makeSpin(0.0, 1.0, 0.05, 2, 0.0, nullptr);
    addRow(ispLayout, QT_TR_NOOP("Denoise Strength:"), m_denoiseStrength,
           QT_TR_NOOP("How hard the ISP denoiser smooths. 0 leaves the image alone even with denoise on."));

    m_sharpen = new QCheckBox();
    m_sharpen->setChecked(false);
    ispLayout->addRow(m_sharpen);

    m_sharpenStrength = makeSpin(0.0, 1.0, 0.05, 2, 0.0, nullptr);
    addRow(ispLayout, QT_TR_NOOP("Sharpen Strength:"), m_sharpenStrength,
           QT_TR_NOOP("How hard the ISP sharpening kernel works on edges. 0 leaves the image alone even with sharpen on."));

    mainLayout->addWidget(m_ispGroup);

    // ========================================================================
    // HSV grading group (collapsible)
    // ========================================================================
    m_hsvGroup = new CollapsibleGroupBox();
    auto* hsvLayout = new QFormLayout();

    m_hueOffset = makeSpin(-180.0, 180.0, 1.0, 1, 0.0, " deg");
    addRow(hsvLayout, QT_TR_NOOP("Hue Offset:"), m_hueOffset,
           QT_TR_NOOP("Rotation applied to the hue of the displayed image, in degrees. 0 leaves hue alone."));

    m_saturationScale = makeSpin(0.0, 10.0, 0.05, 2, 1.0, nullptr);
    addRow(hsvLayout, QT_TR_NOOP("Saturation Scale:"), m_saturationScale,
           QT_TR_NOOP("Multiplier on colour saturation. 0 gives a monochrome display; 1 leaves it alone."));

    m_valueGamma = makeSpin(0.1, 10.0, 0.05, 2, 1.0, nullptr);
    addRow(hsvLayout, QT_TR_NOOP("Value Gamma:"), m_valueGamma,
           QT_TR_NOOP("Exponent applied to the display value after tone mapping. 1 leaves it alone."));

    m_empiricalNoise = new QCheckBox();
    m_empiricalNoise->setChecked(false);
    hsvLayout->addRow(m_empiricalNoise);

    m_noiseSigma = makeSpin(0.0, 1.0, 0.005, 3, 0.02, nullptr);
    addRow(hsvLayout, QT_TR_NOOP("Noise Sigma:"), m_noiseSigma,
           QT_TR_NOOP("Sigma of the empirical display noise stream, in display-value units. The pattern is keyed on the acquisition index, so the same acquisition always shows the same noise."));

    m_temporalDrift = new QCheckBox();
    m_temporalDrift->setChecked(false);
    hsvLayout->addRow(m_temporalDrift);

    m_driftSigma = makeSpin(0.0, 1.0, 0.005, 3, 0.02, nullptr);
    addRow(hsvLayout, QT_TR_NOOP("Drift Sigma:"), m_driftSigma,
           QT_TR_NOOP("Sigma of the empirical temporal drift stream, in display-value units. Slow frame-to-frame wander on top of the noise."));

    m_hsvGroup->setContentLayout(hsvLayout);
    mainLayout->addWidget(m_hsvGroup);

    // ========================================================================
    // Infrared display group (thermal detectors only)
    // ========================================================================
    m_irDisplayGroup = new QGroupBox();
    auto* irLayout = new QFormLayout(m_irDisplayGroup);

    m_irToneCombo = new QComboBox();
    addRow(irLayout, QT_TR_NOOP("Tone:"), m_irToneCombo,
           QT_TR_NOOP("Contrast operator for the infrared display. Linear AGC is globally monotone and safe to read values off; histogram equalize stretches the whole frame; CLAHE is tile-local and only for finding edges."));

    m_irPaletteCombo = new QComboBox();
    addRow(irLayout, QT_TR_NOOP("Palette:"), m_irPaletteCombo,
           QT_TR_NOOP("Colour map from display value to colour. Changes colour only, never contrast."));

    mainLayout->addWidget(m_irDisplayGroup);

    // ========================================================================
    // Read-only status area
    // ========================================================================
    m_statusGroup = new QGroupBox();
    auto* statusLayout = new QFormLayout(m_statusGroup);

    m_effectiveBandLabel = new QLabel();
    statusLayout->addRow(m_effectiveBandLabel);
    m_calibrationLabel = new QLabel();
    statusLayout->addRow(m_calibrationLabel);
    m_provenanceLabel = new QLabel();
    m_provenanceLabel->setWordWrap(true);
    statusLayout->addRow(m_provenanceLabel);
    m_previewLabel = new QLabel();
    m_previewLabel->setWordWrap(true);
    statusLayout->addRow(m_previewLabel);

    mainLayout->addWidget(m_statusGroup);

    // ========================================================================
    // Thermography group
    // ========================================================================
    // Not part of the camera: these do not change what is measured, only what
    // temperature the measurement is reported as. Collapsed by default,
    // because a scene that is not being read as a thermogram has no use for
    // them.
    m_thermographyGroup = new CollapsibleGroupBox();
    auto* thermoLayout = new QFormLayout();

    m_thermographyEnable = new QCheckBox();
    thermoLayout->addRow(m_thermographyEnable);

    m_thermoEmissivity = makeSpin(0.01, 1.0, 0.01, 3, 1.0, nullptr);
    addRow(thermoLayout, QT_TR_NOOP("Assumed Emissivity:"), m_thermoEmissivity,
           QT_TR_NOOP("What the camera is told the surface's emissivity is. 1 gives apparent temperature, which is what a campaign records when it will not assume one -- and which reads cold for any real surface."));

    m_thermoReflectedTemp = makeSpin(0.0, 2000.0, 5.0, 1, 0.0, " K");
    addRow(thermoLayout, QT_TR_NOOP("Reflected Temperature:"), m_thermoReflectedTemp,
           QT_TR_NOOP("Temperature of whatever the surface reflects, usually the sky. Ignored at emissivity 1, since a blackbody reflects nothing."));

    m_thermoTransmittance = makeSpin(0.01, 1.0, 0.01, 3, 1.0, nullptr);
    addRow(thermoLayout, QT_TR_NOOP("Path Transmittance:"), m_thermoTransmittance,
           QT_TR_NOOP("Fraction of the surface's radiation that survives the air between it and the lens. 1 removes the atmosphere from the model, which is right for a short measurement distance."));

    m_thermoAtmosphereTemp = makeSpin(0.0, 2000.0, 5.0, 1, 0.0, " K");
    addRow(thermoLayout, QT_TR_NOOP("Path Temperature:"), m_thermoAtmosphereTemp,
           QT_TR_NOOP("Temperature of that air. Used only when the transmittance is below 1."));

    m_thermographyGroup->setContentLayout(thermoLayout);
    mainLayout->addWidget(m_thermographyGroup);

    mainLayout->addStretch();

    // ========================================================================
    // Connect signals
    // ========================================================================
    connect(m_enabledCheck, &QCheckBox::toggled,
            this, &SensorPanel::onEnabledChanged);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SensorPanel::onPresetChanged);

    // Structural params
    connect(m_detectorKind, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SensorPanel::onDetectorKindChanged);
    connect(m_cfaCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SensorPanel::onParamChanged);
    for (QDoubleSpinBox* box : {m_focalLength, m_fNumber, m_pixelPitch, m_psfSigma,
                                m_rowDelay, m_exposureTime, m_framePeriod,
                                m_analogGain, m_electronsPerDn, m_blackLevelDn,
                                m_fullWell, m_readNoise, m_darkCurrent,
                                m_prnuSigma, m_dsnuSigma, m_nucResidual,
                                m_timeConstant, m_responsivity, m_thermalReadNoise,
                                m_drift, m_netd, m_netdRefTemp,
                                m_aeTarget, m_aeSmoothing, m_aeMinExposure,
                                m_aeMaxExposure, m_aeMaxGain}) {
        connect(box, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &SensorPanel::onParamChanged);
    }
    connect(m_adcBits, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SensorPanel::onParamChanged);
    connect(m_shutterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SensorPanel::onShutterChanged);
    for (QCheckBox* check : {m_shotNoiseEnable, m_readNoiseEnable, m_darkCurrentEnable}) {
        connect(check, &QCheckBox::toggled, this, &SensorPanel::onParamChanged);
    }
    connect(m_fpnNoise, &QCheckBox::toggled, this, &SensorPanel::onFpnToggled);
    connect(m_nucEnable, &QCheckBox::toggled, this, &SensorPanel::onNucToggled);
    connect(m_autoExposure, &QCheckBox::toggled, this, &SensorPanel::onAutoExposureToggled);
    connect(m_autoWhiteBalance, &QCheckBox::toggled, this, &SensorPanel::onParamChanged);

    // Display-only params
    for (QDoubleSpinBox* box : {m_wbR, m_wbG, m_wbB, m_toneGamma,
                                m_denoiseStrength, m_sharpenStrength,
                                m_hueOffset, m_saturationScale, m_valueGamma,
                                m_noiseSigma, m_driftSigma}) {
        connect(box, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &SensorPanel::onDisplayParamChanged);
    }
    connect(m_denoise, &QCheckBox::toggled, this, &SensorPanel::onDenoiseToggled);
    connect(m_sharpen, &QCheckBox::toggled, this, &SensorPanel::onSharpenToggled);
    connect(m_empiricalNoise, &QCheckBox::toggled,
            this, &SensorPanel::onEmpiricalNoiseToggled);
    connect(m_temporalDrift, &QCheckBox::toggled,
            this, &SensorPanel::onTemporalDriftToggled);
    connect(m_irToneCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SensorPanel::onDisplayParamChanged);
    connect(m_irPaletteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SensorPanel::onDisplayParamChanged);

    // Thermography params
    connect(m_thermographyEnable, &QCheckBox::toggled,
            this, &SensorPanel::onThermographyChanged);
    for (QDoubleSpinBox* box : {m_thermoEmissivity, m_thermoReflectedTemp,
                                m_thermoTransmittance, m_thermoAtmosphereTemp}) {
        connect(box, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &SensorPanel::onThermographyChanged);
    }

    // Fold the grading group away by default.
    m_hsvGroup->setCollapsed(true);

    setCameraEnabled(false);
    updateKindVisibility();
}

void SensorPanel::setCameraEnabled(bool enabled) {
    m_updatingUi = true;
    m_enabledCheck->setChecked(enabled);
    m_camera.enabled = enabled;
    m_deviceGroup->setEnabled(enabled);
    m_opticsGroup->setEnabled(enabled);
    m_readoutGroup->setEnabled(enabled);
    m_photonGroup->setEnabled(enabled);
    m_thermalGroup->setEnabled(enabled);
    m_ispGroup->setEnabled(enabled);
    m_hsvGroup->setEnabled(enabled);
    m_irDisplayGroup->setEnabled(enabled);
    m_updatingUi = false;
}

bool SensorPanel::isCameraEnabled() const {
    return m_enabledCheck->isChecked();
}

void SensorPanel::setCameraConfig(const quantiloom::camera::CameraConfig& config) {
    m_camera = config;
    m_camera.enabled = isCameraEnabled();
    updateUiFromConfig(m_camera);
}

quantiloom::camera::CameraConfig SensorPanel::getCameraConfig() const {
    return m_camera;
}

void SensorPanel::onEnabledChanged(bool enabled) {
    m_deviceGroup->setEnabled(enabled);
    m_opticsGroup->setEnabled(enabled);
    m_readoutGroup->setEnabled(enabled);
    m_photonGroup->setEnabled(enabled);
    m_thermalGroup->setEnabled(enabled);
    m_ispGroup->setEnabled(enabled);
    m_hsvGroup->setEnabled(enabled);
    m_irDisplayGroup->setEnabled(enabled);
    m_camera.enabled = enabled;

    if (!m_updatingUi) {
        emit enabledChanged(enabled);
    }
}

void SensorPanel::onPresetChanged(int index) {
    if (m_updatingUi || index <= 0) {
        // "Custom" is a display state, not a config: the fields already hold
        // what the user wants.
        if (index == 0 && !m_updatingUi) {
            m_presetKind = std::nullopt;
            updateStatusArea();
        }
        return;
    }
    const auto kinds = quantiloom::camera::AllCameraPresets();
    const int presetIndex = index - 1;
    if (presetIndex < 0 || presetIndex >= static_cast<int>(kinds.size())) {
        return;
    }
    auto built = quantiloom::camera::MakePresetCameraConfig(kinds[presetIndex]);
    if (!built) {
        qWarning("[SensorPanel] preset failed: %s", built.error().c_str());
        return;
    }
    built.value().enabled = isCameraEnabled();
    m_camera = built.value();
    m_presetKind = kinds[presetIndex];
    m_updatingUi = true;
    updateUiFromConfig(m_camera);
    m_updatingUi = false;
    updateStatusArea();
    emit cameraConfigChanged(m_camera);
}

void SensorPanel::onDetectorKindChanged(int /*index*/) {
    updateKindVisibility();
    onParamChanged();
}

void SensorPanel::onShutterChanged(int index) {
    m_rowDelay->setEnabled(index == 1);
    onParamChanged();
}

void SensorPanel::onFpnToggled(bool checked) {
    m_prnuSigma->setEnabled(checked);
    m_dsnuSigma->setEnabled(checked);
    m_nucEnable->setEnabled(checked);
    m_nucResidual->setEnabled(checked && m_nucEnable->isChecked());
    onParamChanged();
}

void SensorPanel::onNucToggled(bool checked) {
    m_nucResidual->setEnabled(checked && m_fpnNoise->isChecked());
    onParamChanged();
}

void SensorPanel::onAutoExposureToggled(bool checked) {
    for (QWidget* widget : {static_cast<QWidget*>(m_aeTarget),
                            static_cast<QWidget*>(m_aeSmoothing),
                            static_cast<QWidget*>(m_aeMinExposure),
                            static_cast<QWidget*>(m_aeMaxExposure),
                            static_cast<QWidget*>(m_aeMaxGain)}) {
        widget->setEnabled(checked);
    }
    onParamChanged();
}

void SensorPanel::onDenoiseToggled(bool checked) {
    m_denoiseStrength->setEnabled(checked);
    onDisplayParamChanged();
}

void SensorPanel::onSharpenToggled(bool checked) {
    m_sharpenStrength->setEnabled(checked);
    onDisplayParamChanged();
}

void SensorPanel::onEmpiricalNoiseToggled(bool checked) {
    m_noiseSigma->setEnabled(checked);
    onDisplayParamChanged();
}

void SensorPanel::onTemporalDriftToggled(bool checked) {
    m_driftSigma->setEnabled(checked);
    onDisplayParamChanged();
}

void SensorPanel::onParamChanged() {
    if (m_updatingUi) return;

    using quantiloom::camera::DetectorKind;
    using quantiloom::camera::ShutterKind;

    // Any hand edit diverges from the preset the config started from.
    m_presetKind = std::nullopt;
    {
        const QSignalBlocker blocker(m_presetCombo);
        m_presetCombo->setCurrentIndex(0);
    }

    // Detector
    m_camera.device.detector = m_detectorKind->currentIndex() == 1
                                   ? DetectorKind::Thermal
                                   : DetectorKind::Photon;
    m_camera.device.cfa = kCfaValues[m_cfaCombo->currentIndex()];

    // Optics
    m_camera.optics.focalLengthMm = m_focalLength->value();
    m_camera.optics.fNumber = m_fNumber->value();
    m_camera.optics.pixelPitchUm = m_pixelPitch->value();
    m_camera.optics.psfSigmaPixelsOverride = m_psfSigma->value();

    // Readout
    m_camera.readout.shutter = m_shutterCombo->currentIndex() == 1
                                   ? ShutterKind::Rolling
                                   : ShutterKind::Global;
    m_camera.readout.rowDelaySeconds = m_rowDelay->value();
    m_camera.readout.exposureSeconds = m_exposureTime->value();
    m_camera.readout.framePeriodSeconds = m_framePeriod->value();
    m_camera.readout.analogGain = m_analogGain->value();
    m_camera.readout.electronsPerDn = m_electronsPerDn->value();
    m_camera.readout.blackLevelDn = m_blackLevelDn->value();
    m_camera.readout.adcBits = static_cast<quantiloom::u32>(m_adcBits->value());

    // Photon detector
    m_camera.photon.fullWellElectrons = m_fullWell->value();
    m_camera.photon.readNoiseElectronsRms = m_readNoise->value();
    m_camera.photon.darkCurrentElectronsPerSecond = m_darkCurrent->value();
    m_camera.photon.enableShotNoise = m_shotNoiseEnable->isChecked();
    m_camera.photon.enableReadNoise = m_readNoiseEnable->isChecked();
    m_camera.photon.enableDarkCurrent = m_darkCurrentEnable->isChecked();
    m_camera.photon.enableDarkShotNoise = m_darkCurrentEnable->isChecked();
    m_camera.photon.enableFpn = m_fpnNoise->isChecked();
    m_camera.photon.prnuSigma = m_prnuSigma->value();
    m_camera.photon.dsnuElectronsRms = m_dsnuSigma->value();
    m_camera.photon.applyNuc = m_nucEnable->isChecked();
    m_camera.photon.nucResidualFraction = m_nucResidual->value();

    // Thermal detector
    m_camera.thermal.timeConstantSeconds = m_timeConstant->value();
    m_camera.thermal.responsivityDnPerWatt = m_responsivity->value();
    m_camera.thermal.readNoiseDnRms = m_thermalReadNoise->value();
    m_camera.thermal.driftDnPerSecond = m_drift->value();
    m_camera.thermal.netdKelvin = m_netd->value() * 1e-3;
    m_camera.thermal.netdReferenceTemperatureK = m_netdRefTemp->value();

    // ISP auto block (structural: AE/AWB state belongs to the acquisition)
    m_camera.isp.autoExposure = m_autoExposure->isChecked();
    m_camera.isp.autoWhiteBalance = m_autoWhiteBalance->isChecked();
    m_camera.isp.autoControl.targetLuminance = m_aeTarget->value();
    m_camera.isp.autoControl.smoothing = m_aeSmoothing->value();
    m_camera.isp.autoControl.minExposureSeconds = m_aeMinExposure->value();
    m_camera.isp.autoControl.maxExposureSeconds = m_aeMaxExposure->value();
    m_camera.isp.autoControl.maxGain = m_aeMaxGain->value();

    emit cameraConfigChanged(m_camera);
}

void SensorPanel::onDisplayParamChanged() {
    if (m_updatingUi) return;

    m_presetKind = std::nullopt;
    {
        const QSignalBlocker blocker(m_presetCombo);
        m_presetCombo->setCurrentIndex(0);
    }

    // Display-side ISP: reprocessed over the last acquisition, never
    // re-measured, never advancing the acquisition history.
    m_camera.isp.whiteBalance = {m_wbR->value(), m_wbG->value(), m_wbB->value()};
    m_camera.isp.toneGamma = m_toneGamma->value();
    m_camera.isp.denoise = m_denoise->isChecked();
    m_camera.isp.denoiseStrength = m_denoiseStrength->value();
    m_camera.isp.sharpen = m_sharpen->isChecked();
    m_camera.isp.sharpenStrength = m_sharpenStrength->value();
    m_camera.isp.infraredTone = kToneValues[m_irToneCombo->currentIndex()];
    m_camera.isp.infraredPalette = kPaletteValues[m_irPaletteCombo->currentIndex()];

    m_camera.isp.hsv.hueOffsetDegrees = m_hueOffset->value();
    m_camera.isp.hsv.saturationScale = m_saturationScale->value();
    m_camera.isp.hsv.valueGamma = m_valueGamma->value();
    m_camera.isp.hsv.empiricalNoise = m_empiricalNoise->isChecked();
    m_camera.isp.hsv.empiricalNoiseSigma = m_noiseSigma->value();
    m_camera.isp.hsv.temporalDrift = m_temporalDrift->isChecked();
    m_camera.isp.hsv.temporalDriftSigma = m_driftSigma->value();

    emit cameraDisplayChanged(m_camera);
}

void SensorPanel::updateKindVisibility() {
    const bool thermal = m_detectorKind->currentIndex() == 1;
    m_cfaCombo->setVisible(!thermal);
    if (!m_captions.isEmpty()) {
        // The CFA caption is the row label paired with m_cfaCombo.
        for (const Caption& caption : std::as_const(m_captions)) {
            if (caption.field == m_cfaCombo) {
                caption.label->setVisible(!thermal);
            }
        }
    }
    m_photonGroup->setVisible(!thermal);
    m_thermalGroup->setVisible(thermal);
    m_irDisplayGroup->setVisible(thermal);
}

void SensorPanel::updateStatusArea() {
    if (!m_camera.enabled) {
        m_previewLabel->setText(tr("Preview: camera simulation off"));
    } else if (m_camera.inputKind ==
               quantiloom::camera::CameraInputKind::FastRgbApproximation) {
        m_previewLabel->setText(tr("Preview: RGB input approximation; device spectrum unavailable"));
    } else {
        m_previewLabel->setText(
            tr("Preview: GPU spectral sampling, effective PSF, up to %1 exposure positions")
                .arg(m_camera.quality.gpuTimePositions));
    }
    const auto& device = m_camera.device;

    if (device.effectiveMaxNm > device.effectiveMinNm && device.effectiveMinNm > 0.0) {
        m_effectiveBandLabel->setText(tr("Effective band: %1–%2 nm")
                                          .arg(device.effectiveMinNm, 0, 'f', 0)
                                          .arg(device.effectiveMaxNm, 0, 'f', 0));
    } else {
        m_effectiveBandLabel->setText(tr("Effective band: not set by this device"));
    }

    QString calibration;
    switch (device.calibration) {
        case quantiloom::camera::CalibrationStatus::HardwareReference:
            calibration = tr("hardware reference");
            break;
        case quantiloom::camera::CalibrationStatus::Calibrated:
            calibration = tr("calibrated");
            break;
        case quantiloom::camera::CalibrationStatus::GenericAssumption:
        default:
            calibration = tr("generic assumption");
            break;
    }
    m_calibrationLabel->setText(tr("Calibration status: %1").arg(calibration));

    // Provenance: a preset reads the SDK's per-parameter notes; anything else
    // aggregates the parameter sources the config carries.
    QStringList parts;
    int total = 0;
    auto tally = [&parts, &total](const QString& grade, int count) {
        if (count <= 0) return;
        total += count;
        parts << tr("%1 %2").arg(count).arg(grade);
    };
    if (m_presetKind) {
        const auto notes = quantiloom::camera::CameraPresetProvenanceNotes(*m_presetKind);
        int manufacturer = 0, digitized = 0, derived = 0, assumed = 0, other = 0;
        for (const auto& note : notes) {
            const QString grade = QString::fromStdString(note.provenance);
            if (grade == QLatin1String("manufacturer")) ++manufacturer;
            else if (grade == QLatin1String("digitized")) ++digitized;
            else if (grade == QLatin1String("derived")) ++derived;
            else if (grade == QLatin1String("assumed")) ++assumed;
            else ++other;
        }
        total = static_cast<int>(notes.size());
        tally(tr("manufacturer"), manufacturer);
        tally(tr("digitized"), digitized);
        tally(tr("derived"), derived);
        tally(tr("assumed"), assumed);
        tally(tr("other grades"), other);
    } else {
        int manufacturer = 0, digitized = 0, derived = 0, assumed = 0;
        for (const auto& source : device.parameterSources) {
            switch (source.provenance) {
                case quantiloom::camera::ValueProvenance::Manufacturer: ++manufacturer; break;
                case quantiloom::camera::ValueProvenance::Digitized: ++digitized; break;
                case quantiloom::camera::ValueProvenance::Derived: ++derived; break;
                case quantiloom::camera::ValueProvenance::Assumed:
                default: ++assumed; break;
            }
        }
        tally(tr("manufacturer"), manufacturer);
        tally(tr("digitized"), digitized);
        tally(tr("derived"), derived);
        tally(tr("assumed"), assumed);
    }
    if (parts.isEmpty()) {
        m_provenanceLabel->setText(tr("No provenance records — generic defaults."));
    } else {
        m_provenanceLabel->setText(tr("%1 parameters: %2")
                                       .arg(total)
                                       .arg(parts.join(QStringLiteral(", "))));
    }
}

void SensorPanel::updateUiFromConfig(const quantiloom::camera::CameraConfig& config) {
    m_updatingUi = true;
    blockSignalsForUpdate(true);

    // Detector
    m_detectorKind->setCurrentIndex(config.device.detector ==
                                            quantiloom::camera::DetectorKind::Thermal
                                        ? 1
                                        : 0);
    m_cfaCombo->setCurrentIndex(cfaIndex(config.device.cfa));

    // Optics
    m_focalLength->setValue(config.optics.focalLengthMm);
    m_fNumber->setValue(config.optics.fNumber);
    m_pixelPitch->setValue(config.optics.pixelPitchUm);
    m_psfSigma->setValue(config.optics.psfSigmaPixelsOverride);

    // Readout
    m_shutterCombo->setCurrentIndex(config.readout.shutter ==
                                            quantiloom::camera::ShutterKind::Rolling
                                        ? 1
                                        : 0);
    m_rowDelay->setValue(config.readout.rowDelaySeconds);
    m_rowDelay->setEnabled(config.readout.shutter ==
                           quantiloom::camera::ShutterKind::Rolling);
    m_exposureTime->setValue(config.readout.exposureSeconds);
    m_framePeriod->setValue(config.readout.framePeriodSeconds);
    m_analogGain->setValue(config.readout.analogGain);
    m_electronsPerDn->setValue(config.readout.electronsPerDn);
    m_blackLevelDn->setValue(config.readout.blackLevelDn);
    m_adcBits->setValue(static_cast<int>(config.readout.adcBits));

    // Photon detector
    m_fullWell->setValue(config.photon.fullWellElectrons);
    m_readNoise->setValue(config.photon.readNoiseElectronsRms);
    m_darkCurrent->setValue(config.photon.darkCurrentElectronsPerSecond);
    m_shotNoiseEnable->setChecked(config.photon.enableShotNoise);
    m_readNoiseEnable->setChecked(config.photon.enableReadNoise);
    m_darkCurrentEnable->setChecked(config.photon.enableDarkCurrent);
    m_fpnNoise->setChecked(config.photon.enableFpn);
    m_prnuSigma->setValue(config.photon.prnuSigma);
    m_dsnuSigma->setValue(config.photon.dsnuElectronsRms);
    m_nucEnable->setChecked(config.photon.applyNuc);
    m_nucResidual->setValue(config.photon.nucResidualFraction);
    const bool fpnOn = config.photon.enableFpn;
    m_prnuSigma->setEnabled(fpnOn);
    m_dsnuSigma->setEnabled(fpnOn);
    m_nucEnable->setEnabled(fpnOn);
    m_nucResidual->setEnabled(fpnOn && config.photon.applyNuc);

    // Thermal detector
    m_timeConstant->setValue(config.thermal.timeConstantSeconds);
    m_responsivity->setValue(config.thermal.responsivityDnPerWatt);
    m_thermalReadNoise->setValue(config.thermal.readNoiseDnRms);
    m_drift->setValue(config.thermal.driftDnPerSecond);
    m_netd->setValue(config.thermal.netdKelvin * 1e3);
    m_netdRefTemp->setValue(config.thermal.netdReferenceTemperatureK);

    // ISP
    m_autoExposure->setChecked(config.isp.autoExposure);
    const bool aeOn = config.isp.autoExposure;
    m_aeTarget->setEnabled(aeOn);
    m_aeSmoothing->setEnabled(aeOn);
    m_aeMinExposure->setEnabled(aeOn);
    m_aeMaxExposure->setEnabled(aeOn);
    m_aeMaxGain->setEnabled(aeOn);
    m_aeTarget->setValue(config.isp.autoControl.targetLuminance);
    m_aeSmoothing->setValue(config.isp.autoControl.smoothing);
    m_aeMinExposure->setValue(config.isp.autoControl.minExposureSeconds);
    m_aeMaxExposure->setValue(config.isp.autoControl.maxExposureSeconds);
    m_aeMaxGain->setValue(config.isp.autoControl.maxGain);
    m_autoWhiteBalance->setChecked(config.isp.autoWhiteBalance);
    m_wbR->setValue(config.isp.whiteBalance[0]);
    m_wbG->setValue(config.isp.whiteBalance[1]);
    m_wbB->setValue(config.isp.whiteBalance[2]);
    m_toneGamma->setValue(config.isp.toneGamma);
    m_denoise->setChecked(config.isp.denoise);
    m_denoiseStrength->setValue(config.isp.denoiseStrength);
    m_denoiseStrength->setEnabled(config.isp.denoise);
    m_sharpen->setChecked(config.isp.sharpen);
    m_sharpenStrength->setValue(config.isp.sharpenStrength);
    m_sharpenStrength->setEnabled(config.isp.sharpen);

    // HSV
    m_hueOffset->setValue(config.isp.hsv.hueOffsetDegrees);
    m_saturationScale->setValue(config.isp.hsv.saturationScale);
    m_valueGamma->setValue(config.isp.hsv.valueGamma);
    m_empiricalNoise->setChecked(config.isp.hsv.empiricalNoise);
    m_noiseSigma->setValue(config.isp.hsv.empiricalNoiseSigma);
    m_noiseSigma->setEnabled(config.isp.hsv.empiricalNoise);
    m_temporalDrift->setChecked(config.isp.hsv.temporalDrift);
    m_driftSigma->setValue(config.isp.hsv.temporalDriftSigma);
    m_driftSigma->setEnabled(config.isp.hsv.temporalDrift);

    // IR display
    m_irToneCombo->setCurrentIndex(toneIndex(config.isp.infraredTone));
    m_irPaletteCombo->setCurrentIndex(paletteIndex(config.isp.infraredPalette));

    // Preset row: show the preset this config still matches, else Custom.
    m_presetKind = matchPreset(config);
    int presetIndex = 0;
    if (m_presetKind) {
        const auto kinds = quantiloom::camera::AllCameraPresets();
        const auto it = std::find(kinds.begin(), kinds.end(), *m_presetKind);
        if (it != kinds.end()) {
            presetIndex = 1 + static_cast<int>(std::distance(kinds.begin(), it));
        }
    }
    m_presetCombo->setCurrentIndex(presetIndex);

    updateKindVisibility();

    blockSignalsForUpdate(false);
    m_updatingUi = false;
    updateStatusArea();
}

void SensorPanel::blockSignalsForUpdate(bool block) {
    for (QWidget* widget : {
             // Preset and detector
             static_cast<QWidget*>(m_presetCombo),
             static_cast<QWidget*>(m_detectorKind),
             static_cast<QWidget*>(m_cfaCombo),
             // Optics
             static_cast<QWidget*>(m_focalLength),
             static_cast<QWidget*>(m_fNumber),
             static_cast<QWidget*>(m_pixelPitch),
             static_cast<QWidget*>(m_psfSigma),
             // Readout
             static_cast<QWidget*>(m_shutterCombo),
             static_cast<QWidget*>(m_rowDelay),
             static_cast<QWidget*>(m_exposureTime),
             static_cast<QWidget*>(m_framePeriod),
             static_cast<QWidget*>(m_analogGain),
             static_cast<QWidget*>(m_electronsPerDn),
             static_cast<QWidget*>(m_blackLevelDn),
             static_cast<QWidget*>(m_adcBits),
             // Photon
             static_cast<QWidget*>(m_fullWell),
             static_cast<QWidget*>(m_readNoise),
             static_cast<QWidget*>(m_darkCurrent),
             static_cast<QWidget*>(m_shotNoiseEnable),
             static_cast<QWidget*>(m_readNoiseEnable),
             static_cast<QWidget*>(m_darkCurrentEnable),
             static_cast<QWidget*>(m_fpnNoise),
             static_cast<QWidget*>(m_prnuSigma),
             static_cast<QWidget*>(m_dsnuSigma),
             static_cast<QWidget*>(m_nucEnable),
             static_cast<QWidget*>(m_nucResidual),
             // Thermal
             static_cast<QWidget*>(m_timeConstant),
             static_cast<QWidget*>(m_responsivity),
             static_cast<QWidget*>(m_thermalReadNoise),
             static_cast<QWidget*>(m_drift),
             static_cast<QWidget*>(m_netd),
             static_cast<QWidget*>(m_netdRefTemp),
             // ISP
             static_cast<QWidget*>(m_autoExposure),
             static_cast<QWidget*>(m_aeTarget),
             static_cast<QWidget*>(m_aeSmoothing),
             static_cast<QWidget*>(m_aeMinExposure),
             static_cast<QWidget*>(m_aeMaxExposure),
             static_cast<QWidget*>(m_aeMaxGain),
             static_cast<QWidget*>(m_autoWhiteBalance),
             static_cast<QWidget*>(m_wbR),
             static_cast<QWidget*>(m_wbG),
             static_cast<QWidget*>(m_wbB),
             static_cast<QWidget*>(m_toneGamma),
             static_cast<QWidget*>(m_denoise),
             static_cast<QWidget*>(m_denoiseStrength),
             static_cast<QWidget*>(m_sharpen),
             static_cast<QWidget*>(m_sharpenStrength),
             // HSV
             static_cast<QWidget*>(m_hueOffset),
             static_cast<QWidget*>(m_saturationScale),
             static_cast<QWidget*>(m_valueGamma),
             static_cast<QWidget*>(m_empiricalNoise),
             static_cast<QWidget*>(m_noiseSigma),
             static_cast<QWidget*>(m_temporalDrift),
             static_cast<QWidget*>(m_driftSigma),
             // IR display
             static_cast<QWidget*>(m_irToneCombo),
             static_cast<QWidget*>(m_irPaletteCombo),
         }) {
        widget->blockSignals(block);
    }
}

void SensorPanel::setThermography(bool enabled,
                                  const quantiloom::ThermographyParams& params) {
    m_thermography = params;
    m_updatingUi = true;
    const QSignalBlocker e(m_thermographyEnable);
    const QSignalBlocker eps(m_thermoEmissivity);
    const QSignalBlocker refl(m_thermoReflectedTemp);
    const QSignalBlocker tau(m_thermoTransmittance);
    const QSignalBlocker atm(m_thermoAtmosphereTemp);
    m_thermographyEnable->setChecked(enabled);
    m_thermoEmissivity->setValue(static_cast<double>(params.emissivity));
    m_thermoReflectedTemp->setValue(static_cast<double>(params.reflectedTemperature_K));
    m_thermoTransmittance->setValue(static_cast<double>(params.atmosphereTransmittance));
    m_thermoAtmosphereTemp->setValue(static_cast<double>(params.atmosphereTemperature_K));
    m_updatingUi = false;
}

bool SensorPanel::isThermographyEnabled() const {
    return m_thermographyEnable->isChecked();
}

quantiloom::ThermographyParams SensorPanel::getThermographyParams() const {
    return m_thermography;
}

void SensorPanel::onThermographyChanged() {
    if (m_updatingUi) return;

    m_thermography.emissivity = static_cast<float>(m_thermoEmissivity->value());
    m_thermography.reflectedTemperature_K =
        static_cast<float>(m_thermoReflectedTemp->value());
    m_thermography.atmosphereTransmittance =
        static_cast<float>(m_thermoTransmittance->value());
    m_thermography.atmosphereTemperature_K =
        static_cast<float>(m_thermoAtmosphereTemp->value());

    emit thermographyChanged(m_thermographyEnable->isChecked(), m_thermography);
}
