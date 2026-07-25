/**
 * @file SpectralConfigPanel.cpp
 * @brief Spectral rendering configuration panel implementation
 */

#include "SpectralConfigPanel.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QStackedWidget>
#include <QFormLayout>
#include <cmath>

namespace {

/// The wavelength range a fused mode actually integrates, formatted for display.
///
/// Deliberately not built from quantiloom::constants::WAVELENGTH_*: those are
/// the ISO 20473 taxonomy for what a band *name* means, and for two modes they
/// are not what the renderer integrates -- NIR is 780-1400 by that taxonomy but
/// 930-1200 as rendered (narrowed to the NN atmosphere's coverage), and SWIR is
/// 1000-2500 versus 1400-2400 (avoiding the NIR overlap). This panel used to
/// quote the taxonomy, so the range on screen was not the range the image was
/// computed over. GetFusedBandInfo() is the renderer's own source of truth.
///
/// Returns an empty string for modes that are not fused bands.
QString fusedRangeText(quantiloom::SpectralMode mode) {
    const auto info = quantiloom::GetFusedBandInfo(mode);
    if (!info) {
        return {};
    }
    // Micrometres past 3 um, nanometres below, matching how each band is
    // conventionally written. QString::number rather than arg's numeric
    // overload so the digits never pick up locale grouping ("1 200 nm").
    if (info->lambdaMinNm >= 3000.0f) {
        return SpectralConfigPanel::tr("%1-%2 μm")
            .arg(QString::number(info->lambdaMinNm / 1000.0, 'g', 3),
                 QString::number(info->lambdaMaxNm / 1000.0, 'g', 3));
    }
    return SpectralConfigPanel::tr("%1-%2 nm")
        .arg(QString::number(info->lambdaMinNm, 'g', 4),
             QString::number(info->lambdaMaxNm, 'g', 4));
}

} // namespace

SpectralConfigPanel::SpectralConfigPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void SpectralConfigPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // Mode selection group
    auto* modeGroup = new QGroupBox(tr("Spectral Mode"));
    auto* modeLayout = new QVBoxLayout(modeGroup);

    m_modeCombo = new QComboBox();
    m_modeCombo->addItem(tr("RGB (Default)"),
                         static_cast<int>(quantiloom::SpectralMode::RGB));
    m_modeCombo->addItem(tr("VIS Fused (32-band Spectral)"),
                         static_cast<int>(quantiloom::SpectralMode::VIS_Fused));
    m_modeCombo->addItem(tr("Single Wavelength"),
                         static_cast<int>(quantiloom::SpectralMode::Single));
    m_modeCombo->addItem(tr("NIR (%1)").arg(fusedRangeText(quantiloom::SpectralMode::NIR_Fused)),
                         static_cast<int>(quantiloom::SpectralMode::NIR_Fused));
    m_modeCombo->addItem(tr("SWIR (%1)").arg(fusedRangeText(quantiloom::SpectralMode::SWIR_Fused)),
                         static_cast<int>(quantiloom::SpectralMode::SWIR_Fused));
    m_modeCombo->addItem(tr("MWIR (%1)").arg(fusedRangeText(quantiloom::SpectralMode::MWIR_Fused)),
                         static_cast<int>(quantiloom::SpectralMode::MWIR_Fused));
    m_modeCombo->addItem(tr("LWIR (%1)").arg(fusedRangeText(quantiloom::SpectralMode::LWIR_Fused)),
                         static_cast<int>(quantiloom::SpectralMode::LWIR_Fused));
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpectralConfigPanel::onModeChanged);
    modeLayout->addWidget(m_modeCombo);

    m_modeDescription = new QLabel();
    m_modeDescription->setWordWrap(true);
    m_modeDescription->setStyleSheet("color: gray; font-size: 10pt;");
    modeLayout->addWidget(m_modeDescription);

    mainLayout->addWidget(modeGroup);

    // Settings stack (different panels for different modes)
    m_settingsStack = new QStackedWidget();

    // Page 0: RGB mode (no extra settings)
    auto* rgbPage = new QWidget();
    auto* rgbLayout = new QVBoxLayout(rgbPage);
    rgbLayout->addWidget(new QLabel(tr("Standard RGB rendering with 3-band color.")));
    rgbLayout->addStretch();
    m_settingsStack->addWidget(rgbPage);

    // Page 1: Single wavelength mode
    auto* singlePage = new QWidget();
    auto* singleLayout = new QFormLayout(singlePage);

    // Wavelength slider
    auto* sliderRow = new QHBoxLayout();
    m_wavelengthSlider = new QSlider(Qt::Horizontal);
    m_wavelengthSlider->setRange(380, 760);
    m_wavelengthSlider->setValue(550);
    connect(m_wavelengthSlider, &QSlider::valueChanged,
            this, &SpectralConfigPanel::onWavelengthSliderChanged);
    sliderRow->addWidget(m_wavelengthSlider);

    m_wavelengthColorPreview = new QLabel();
    m_wavelengthColorPreview->setFixedSize(24, 24);
    m_wavelengthColorPreview->setStyleSheet("background-color: rgb(0, 255, 0); border: 1px solid black;");
    sliderRow->addWidget(m_wavelengthColorPreview);
    singleLayout->addRow(sliderRow);

    // Wavelength spinbox
    m_wavelengthSpin = new QDoubleSpinBox();
    m_wavelengthSpin->setRange(380.0, 760.0);
    m_wavelengthSpin->setSingleStep(1.0);
    m_wavelengthSpin->setValue(550.0);
    m_wavelengthSpin->setSuffix(" nm");
    connect(m_wavelengthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SpectralConfigPanel::onWavelengthSpinChanged);
    singleLayout->addRow(tr("Wavelength:"), m_wavelengthSpin);

    m_settingsStack->addWidget(singlePage);

    // Page 2: MWIR mode
    auto* mwirPage = new QWidget();
    auto* mwirLayout = new QVBoxLayout(mwirPage);
    mwirLayout->addWidget(new QLabel(
        tr("Mid-Wave Infrared (%1)\nThermal imaging mode.")
            .arg(fusedRangeText(quantiloom::SpectralMode::MWIR_Fused))));
    mwirLayout->addStretch();
    m_settingsStack->addWidget(mwirPage);

    // Page 3: LWIR mode
    auto* lwirPage = new QWidget();
    auto* lwirLayout = new QVBoxLayout(lwirPage);
    lwirLayout->addWidget(new QLabel(
        tr("Long-Wave Infrared (%1)\nThermal imaging mode.")
            .arg(fusedRangeText(quantiloom::SpectralMode::LWIR_Fused))));
    lwirLayout->addStretch();
    m_settingsStack->addWidget(lwirPage);

    mainLayout->addWidget(m_settingsStack);

    // Hyperspectral range settings (always visible for reference)
    auto* rangeGroup = new QGroupBox(tr("Hyperspectral Range"));
    auto* rangeLayout = new QFormLayout(rangeGroup);

    m_lambdaMinSpin = new QDoubleSpinBox();
    m_lambdaMinSpin->setRange(300.0, 2500.0);
    m_lambdaMinSpin->setValue(380.0);
    m_lambdaMinSpin->setSuffix(" nm");
    connect(m_lambdaMinSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SpectralConfigPanel::onRangeChanged);
    rangeLayout->addRow(tr("Min λ:"), m_lambdaMinSpin);

    m_lambdaMaxSpin = new QDoubleSpinBox();
    m_lambdaMaxSpin->setRange(300.0, 2500.0);
    m_lambdaMaxSpin->setValue(760.0);
    m_lambdaMaxSpin->setSuffix(" nm");
    connect(m_lambdaMaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SpectralConfigPanel::onRangeChanged);
    rangeLayout->addRow(tr("Max λ:"), m_lambdaMaxSpin);

    m_deltaSpin = new QDoubleSpinBox();
    m_deltaSpin->setRange(1.0, 100.0);
    m_deltaSpin->setValue(5.0);
    m_deltaSpin->setSuffix(" nm");
    connect(m_deltaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SpectralConfigPanel::onRangeChanged);
    rangeLayout->addRow(tr("Δλ:"), m_deltaSpin);

    m_bandCountLabel = new QLabel("77 bands");
    rangeLayout->addRow(tr("Bands:"), m_bandCountLabel);

    mainLayout->addWidget(rangeGroup);

    // Quantitative warning (hidden by default, shown for MWIR/LWIR/SWIR/NIR modes)
    m_quantitativeWarning = new QLabel();
    m_quantitativeWarning->setWordWrap(true);
    m_quantitativeWarning->setText(
        tr("Preview mode: Using RGB-averaged spectral albedo.\n"
           "NOT suitable for quantitative analysis.\n"
           "For accurate IR rendering, use measured spectral materials."));
    m_quantitativeWarning->setStyleSheet(
        "QLabel { "
        "  color: #B8860B; "  // Dark golden rod
        "  background-color: #FFF8DC; "  // Cornsilk
        "  border: 1px solid #DAA520; "
        "  border-radius: 4px; "
        "  padding: 8px; "
        "  font-size: 9pt; "
        "}");
    m_quantitativeWarning->setVisible(false);
    mainLayout->addWidget(m_quantitativeWarning);

    mainLayout->addStretch();

    // Initialize
    updateModeDescription(quantiloom::SpectralMode::RGB);
    onWavelengthSliderChanged(550);
}

void SpectralConfigPanel::setSpectralMode(quantiloom::SpectralMode mode) {
    m_mode = mode;

    // Find matching combo item
    for (int i = 0; i < m_modeCombo->count(); ++i) {
        if (m_modeCombo->itemData(i).toInt() == static_cast<int>(mode)) {
            m_modeCombo->blockSignals(true);
            m_modeCombo->setCurrentIndex(i);
            m_modeCombo->blockSignals(false);
            break;
        }
    }

    updateModeDescription(mode);

    // Update stack page
    switch (mode) {
        case quantiloom::SpectralMode::RGB:
        case quantiloom::SpectralMode::VIS_Fused:
        case quantiloom::SpectralMode::NIR_Fused:
        case quantiloom::SpectralMode::SWIR_Fused:
            m_settingsStack->setCurrentIndex(0);
            break;
        case quantiloom::SpectralMode::Single:
            m_settingsStack->setCurrentIndex(1);
            break;
        case quantiloom::SpectralMode::MWIR_Fused:
            m_settingsStack->setCurrentIndex(2);
            break;
        case quantiloom::SpectralMode::LWIR_Fused:
            m_settingsStack->setCurrentIndex(3);
            break;
        default:
            m_settingsStack->setCurrentIndex(0);
            break;
    }
}

void SpectralConfigPanel::setWavelength(float wavelength_nm) {
    m_wavelength = wavelength_nm;

    m_wavelengthSlider->blockSignals(true);
    m_wavelengthSlider->setValue(static_cast<int>(wavelength_nm));
    m_wavelengthSlider->blockSignals(false);

    m_wavelengthSpin->blockSignals(true);
    m_wavelengthSpin->setValue(wavelength_nm);
    m_wavelengthSpin->blockSignals(false);

    onWavelengthSliderChanged(static_cast<int>(wavelength_nm));
}

void SpectralConfigPanel::setWavelengthRange(float min_nm, float max_nm, float delta_nm) {
    m_lambdaMin = min_nm;
    m_lambdaMax = max_nm;
    m_deltaLambda = delta_nm;

    m_lambdaMinSpin->blockSignals(true);
    m_lambdaMinSpin->setValue(min_nm);
    m_lambdaMinSpin->blockSignals(false);

    m_lambdaMaxSpin->blockSignals(true);
    m_lambdaMaxSpin->setValue(max_nm);
    m_lambdaMaxSpin->blockSignals(false);

    m_deltaSpin->blockSignals(true);
    m_deltaSpin->setValue(delta_nm);
    m_deltaSpin->blockSignals(false);

    int bands = static_cast<int>((max_nm - min_nm) / delta_nm) + 1;
    m_bandCountLabel->setText(QString("%1 bands").arg(bands));
}

void SpectralConfigPanel::onModeChanged(int index) {
    auto mode = static_cast<quantiloom::SpectralMode>(m_modeCombo->itemData(index).toInt());
    m_mode = mode;

    updateModeDescription(mode);

    switch (mode) {
        case quantiloom::SpectralMode::RGB:
        case quantiloom::SpectralMode::VIS_Fused:
        case quantiloom::SpectralMode::NIR_Fused:
        case quantiloom::SpectralMode::SWIR_Fused:
            m_settingsStack->setCurrentIndex(0);
            break;
        case quantiloom::SpectralMode::Single:
            m_settingsStack->setCurrentIndex(1);
            break;
        case quantiloom::SpectralMode::MWIR_Fused:
            m_settingsStack->setCurrentIndex(2);
            break;
        case quantiloom::SpectralMode::LWIR_Fused:
            m_settingsStack->setCurrentIndex(3);
            break;
        default:
            m_settingsStack->setCurrentIndex(0);
            break;
    }

    emit spectralModeChanged(mode);
}

void SpectralConfigPanel::onWavelengthSliderChanged(int value) {
    m_wavelength = static_cast<float>(value);

    m_wavelengthSpin->blockSignals(true);
    m_wavelengthSpin->setValue(m_wavelength);
    m_wavelengthSpin->blockSignals(false);

    // Update color preview (approximate visible spectrum)
    QColor color;
    if (value < 380) {
        color = QColor(128, 0, 128);  // UV - purple
    } else if (value < 440) {
        // Violet
        float t = (value - 380.0f) / 60.0f;
        color = QColor(static_cast<int>((1.0f - t) * 128), 0, static_cast<int>(128 + t * 127));
    } else if (value < 490) {
        // Blue
        float t = (value - 440.0f) / 50.0f;
        color = QColor(0, static_cast<int>(t * 255), 255);
    } else if (value < 510) {
        // Cyan
        float t = (value - 490.0f) / 20.0f;
        color = QColor(0, 255, static_cast<int>((1.0f - t) * 255));
    } else if (value < 580) {
        // Green to Yellow
        float t = (value - 510.0f) / 70.0f;
        color = QColor(static_cast<int>(t * 255), 255, 0);
    } else if (value < 645) {
        // Yellow to Orange
        float t = (value - 580.0f) / 65.0f;
        color = QColor(255, static_cast<int>((1.0f - t) * 255), 0);
    } else {
        // Red
        color = QColor(255, 0, 0);
    }

    m_wavelengthColorPreview->setStyleSheet(
        QString("background-color: rgb(%1, %2, %3); border: 1px solid black;")
            .arg(color.red()).arg(color.green()).arg(color.blue())
    );

    emit wavelengthChanged(m_wavelength);
}

void SpectralConfigPanel::onWavelengthSpinChanged(double value) {
    m_wavelength = static_cast<float>(value);

    m_wavelengthSlider->blockSignals(true);
    m_wavelengthSlider->setValue(static_cast<int>(value));
    m_wavelengthSlider->blockSignals(false);

    emit wavelengthChanged(m_wavelength);
}

void SpectralConfigPanel::onRangeChanged() {
    m_lambdaMin = static_cast<float>(m_lambdaMinSpin->value());
    m_lambdaMax = static_cast<float>(m_lambdaMaxSpin->value());
    m_deltaLambda = static_cast<float>(m_deltaSpin->value());

    int bands = static_cast<int>((m_lambdaMax - m_lambdaMin) / m_deltaLambda) + 1;
    m_bandCountLabel->setText(QString("%1 bands").arg(bands));

    emit wavelengthRangeChanged(m_lambdaMin, m_lambdaMax, m_deltaLambda);
}

void SpectralConfigPanel::updateModeDescription(quantiloom::SpectralMode mode) {
    QString desc;
    bool showWarning = false;

    switch (mode) {
        case quantiloom::SpectralMode::RGB:
            desc = tr("Fast RGB rendering, no spectral integration. "
                      "Best for real-time preview.");
            break;
        case quantiloom::SpectralMode::VIS_Fused:
            desc = tr("32-wavelength spectral integration with CIE XYZ color matching. "
                      "Physically accurate but slower.");
            break;
        case quantiloom::SpectralMode::Single:
            desc = tr("Monochromatic rendering at a single wavelength. "
                      "Useful for spectral analysis and wavelength-specific effects.");
            break;
        case quantiloom::SpectralMode::MWIR_Fused:
            desc = tr("Mid-Wave Infrared (%1). Thermal imaging for hot objects, "
                      "engine exhaust, and fire detection.").arg(fusedRangeText(mode));
            showWarning = true;
            break;
        case quantiloom::SpectralMode::LWIR_Fused:
            desc = tr("Long-Wave Infrared (%1). Thermal imaging for room-temperature "
                      "objects, people, and buildings.").arg(fusedRangeText(mode));
            showWarning = true;
            break;
        case quantiloom::SpectralMode::SWIR_Fused:
            desc = tr("Short-Wave Infrared (%1). Moisture detection, "
                      "material identification, and imaging through haze.").arg(fusedRangeText(mode));
            showWarning = true;
            break;
        case quantiloom::SpectralMode::NIR_Fused:
            desc = tr("Near-Infrared (%1). Reflected solar radiation, "
                      "vegetation analysis, and night vision.").arg(fusedRangeText(mode));
            showWarning = true;
            break;
        default:
            desc = tr("Unknown spectral mode.");
            break;
    }
    m_modeDescription->setText(desc);

    // Show quantitative warning for IR modes
    if (m_quantitativeWarning) {
        m_quantitativeWarning->setVisible(showWarning);
    }
}
