/**
 * @file LightingPanel.cpp
 * @brief Sun/sky lighting parameter editor implementation
 */

#include "LightingPanel.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <cmath>

#include <renderer/LightingParams.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

/// Direction FROM the surface TO the sun.
/// Azimuth: 0 = North (+Z), 90 = East (+X), 180 = South (-Z), 270 = West (-X).
glm::vec3 sunDirectionFromAngles(float azimuthDeg, float elevationDeg) {
    const float azRad = azimuthDeg * static_cast<float>(M_PI) / 180.0f;
    const float elRad = elevationDeg * static_cast<float>(M_PI) / 180.0f;
    const float cosEl = std::cos(elRad);
    return glm::normalize(glm::vec3(cosEl * std::sin(azRad),
                                    std::sin(elRad),
                                    cosEl * std::cos(azRad)));
}

} // namespace

LightingPanel::LightingPanel(QWidget* parent)
    : PanelBase(parent)
{
    setObjectName(panelId());
    setupUi();
    retranslateUi();
}

QString LightingPanel::panelTitle() const {
    return tr("Lighting");
}

void LightingPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // Sun direction
    m_sunDirGroup = new QGroupBox(this);
    auto* sunDirLayout = new QFormLayout(m_sunDirGroup);

    auto* azimuthRow = new QHBoxLayout();
    m_azimuthSlider = new QSlider(Qt::Horizontal);
    m_azimuthSlider->setRange(0, 360);
    m_azimuthSlider->setValue(180);
    m_azimuthLabel = new QLabel(QStringLiteral("180°"));
    m_azimuthLabel->setMinimumWidth(45);
    connect(m_azimuthSlider, &QSlider::valueChanged, this, &LightingPanel::onSunAzimuthChanged);
    azimuthRow->addWidget(m_azimuthSlider);
    azimuthRow->addWidget(m_azimuthLabel);
    m_azimuthCaption = new QLabel(m_sunDirGroup);
    sunDirLayout->addRow(m_azimuthCaption, azimuthRow);

    auto* elevationRow = new QHBoxLayout();
    m_elevationSlider = new QSlider(Qt::Horizontal);
    m_elevationSlider->setRange(0, 90);
    m_elevationSlider->setValue(45);
    m_elevationLabel = new QLabel(QStringLiteral("45°"));
    m_elevationLabel->setMinimumWidth(45);
    connect(m_elevationSlider, &QSlider::valueChanged, this, &LightingPanel::onSunElevationChanged);
    elevationRow->addWidget(m_elevationSlider);
    elevationRow->addWidget(m_elevationLabel);
    m_elevationCaption = new QLabel(m_sunDirGroup);
    sunDirLayout->addRow(m_elevationCaption, elevationRow);

    mainLayout->addWidget(m_sunDirGroup);

    // Radiance
    m_radianceGroup = new QGroupBox(this);
    auto* radianceLayout = new QFormLayout(m_radianceGroup);

    m_sunIntensitySpin = new QDoubleSpinBox();
    m_sunIntensitySpin->setRange(0.0, 100.0);
    m_sunIntensitySpin->setSingleStep(0.1);
    m_sunIntensitySpin->setValue(1.0);
    connect(m_sunIntensitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LightingPanel::onSunIntensityChanged);
    m_sunCaption = new QLabel(m_radianceGroup);
    radianceLayout->addRow(m_sunCaption, m_sunIntensitySpin);

    m_skyIntensitySpin = new QDoubleSpinBox();
    m_skyIntensitySpin->setRange(0.0, 10.0);
    m_skyIntensitySpin->setSingleStep(0.01);
    m_skyIntensitySpin->setValue(0.1);
    connect(m_skyIntensitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LightingPanel::onSkyIntensityChanged);
    m_skyCaption = new QLabel(m_radianceGroup);
    radianceLayout->addRow(m_skyCaption, m_skyIntensitySpin);

    mainLayout->addWidget(m_radianceGroup);
    mainLayout->addStretch();
}

void LightingPanel::retranslateUi() {
    m_sunDirGroup->setTitle(tr("Sun Direction"));
    m_azimuthCaption->setText(tr("Azimuth:"));
    m_elevationCaption->setText(tr("Elevation:"));

    m_radianceGroup->setTitle(tr("Radiance"));
    m_sunCaption->setText(tr("Sun:"));
    m_skyCaption->setText(tr("Sky:"));
    // Suffixes are part of the value display, so they are reapplied here too.
    m_sunIntensitySpin->setSuffix(tr(" W/m²/sr"));
    m_skyIntensitySpin->setSuffix(tr(" W/m²/sr"));

    m_azimuthLabel->setText(tr("%1°").arg(static_cast<int>(m_sunAzimuth)));
    m_elevationLabel->setText(tr("%1°").arg(static_cast<int>(m_sunElevation)));
}

void LightingPanel::setLightingParams(const quantiloom::LightingParams& params) {
    // sunDirection points FROM surface TO sun
    const glm::vec3& dir = params.sunDirection;

    m_sunElevation = std::asin(dir.y) * 180.0f / static_cast<float>(M_PI);
    m_sunElevation = std::max(0.0f, std::min(90.0f, m_sunElevation));

    m_sunAzimuth = std::atan2(dir.x, dir.z) * 180.0f / static_cast<float>(M_PI);
    if (m_sunAzimuth < 0) m_sunAzimuth += 360.0f;

    m_sunRadiance = params.sunRadiance_rgb;
    m_sunIntensity = (m_sunRadiance.r + m_sunRadiance.g + m_sunRadiance.b) / 3.0f;

    m_skyRadiance = params.skyRadiance_rgb;
    m_skyIntensity = (m_skyRadiance.r + m_skyRadiance.g + m_skyRadiance.b) / 3.0f;

    m_chromaR_correction = params.chromaR_correction;
    m_chromaB_correction = params.chromaB_correction;
    m_enableShadowRays = (params.enableShadowRays != 0);

    // Update UI (block signals to avoid feedback loop)
    {
        const QSignalBlocker azimuth(m_azimuthSlider);
        m_azimuthSlider->setValue(static_cast<int>(m_sunAzimuth));
    }
    {
        const QSignalBlocker elevation(m_elevationSlider);
        m_elevationSlider->setValue(static_cast<int>(m_sunElevation));
    }
    {
        const QSignalBlocker sun(m_sunIntensitySpin);
        m_sunIntensitySpin->setValue(m_sunIntensity);
    }
    {
        const QSignalBlocker sky(m_skyIntensitySpin);
        m_skyIntensitySpin->setValue(m_skyIntensity);
    }

    m_azimuthLabel->setText(tr("%1°").arg(static_cast<int>(m_sunAzimuth)));
    m_elevationLabel->setText(tr("%1°").arg(static_cast<int>(m_sunElevation)));
}

void LightingPanel::onSunAzimuthChanged(int value) {
    m_sunAzimuth = static_cast<float>(value);
    m_azimuthLabel->setText(tr("%1°").arg(value));
    emitChanges();
}

void LightingPanel::onSunElevationChanged(int value) {
    m_sunElevation = static_cast<float>(value);
    m_elevationLabel->setText(tr("%1°").arg(value));
    emitChanges();
}

void LightingPanel::onSunIntensityChanged(double value) {
    m_sunIntensity = static_cast<float>(value);
    m_sunRadiance = glm::vec3(m_sunIntensity);
    emitChanges();
}

void LightingPanel::onSkyIntensityChanged(double value) {
    m_skyIntensity = static_cast<float>(value);
    // Keep sky color tint (blue-ish)
    m_skyRadiance = glm::vec3(m_skyIntensity * 1.0f, m_skyIntensity * 1.5f, m_skyIntensity * 2.0f);
    emitChanges();
}

void LightingPanel::emitChanges() {
    quantiloom::LightingParams params = quantiloom::CreateDefaultLightingParams();
    params.sunDirection = sunDirectionFromAngles(m_sunAzimuth, m_sunElevation);
    params.sunRadiance_spectral = m_sunIntensity;
    params.sunRadiance_rgb = m_sunRadiance;
    params.skyRadiance_spectral = m_skyIntensity;
    params.skyRadiance_rgb = m_skyRadiance;
    params.chromaR_correction = m_chromaR_correction;
    params.chromaB_correction = m_chromaB_correction;
    params.enableShadowRays = m_enableShadowRays ? 1u : 0u;

    // transmittance and atmosphereTemperature_K are left at their defaults on
    // purpose: the atmosphere panel owns them, and the shell merges the two
    // halves before anything reaches the renderer.
    emit lightingChanged(params);
}
