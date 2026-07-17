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

LightingPanel::LightingPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void LightingPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // Sun Direction group
    auto* sunDirGroup = new QGroupBox(tr("Sun Direction"));
    auto* sunDirLayout = new QFormLayout(sunDirGroup);

    // Azimuth (horizontal angle)
    auto* azimuthRow = new QHBoxLayout();
    m_azimuthSlider = new QSlider(Qt::Horizontal);
    m_azimuthSlider->setRange(0, 360);
    m_azimuthSlider->setValue(180);
    m_azimuthLabel = new QLabel("180°");
    m_azimuthLabel->setFixedWidth(45);
    connect(m_azimuthSlider, &QSlider::valueChanged, this, &LightingPanel::onSunAzimuthChanged);
    azimuthRow->addWidget(m_azimuthSlider);
    azimuthRow->addWidget(m_azimuthLabel);
    sunDirLayout->addRow(tr("Azimuth:"), azimuthRow);

    // Elevation (vertical angle)
    auto* elevationRow = new QHBoxLayout();
    m_elevationSlider = new QSlider(Qt::Horizontal);
    m_elevationSlider->setRange(0, 90);
    m_elevationSlider->setValue(45);
    m_elevationLabel = new QLabel("45°");
    m_elevationLabel->setFixedWidth(45);
    connect(m_elevationSlider, &QSlider::valueChanged, this, &LightingPanel::onSunElevationChanged);
    elevationRow->addWidget(m_elevationSlider);
    elevationRow->addWidget(m_elevationLabel);
    sunDirLayout->addRow(tr("Elevation:"), elevationRow);

    mainLayout->addWidget(sunDirGroup);

    // Radiance group
    auto* radianceGroup = new QGroupBox(tr("Radiance"));
    auto* radianceLayout = new QFormLayout(radianceGroup);

    m_sunIntensitySpin = new QDoubleSpinBox();
    m_sunIntensitySpin->setRange(0.0, 100.0);
    m_sunIntensitySpin->setSingleStep(0.1);
    m_sunIntensitySpin->setValue(1.0);
    m_sunIntensitySpin->setSuffix(" W/m²/sr");
    connect(m_sunIntensitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LightingPanel::onSunIntensityChanged);
    radianceLayout->addRow(tr("Sun:"), m_sunIntensitySpin);

    m_skyIntensitySpin = new QDoubleSpinBox();
    m_skyIntensitySpin->setRange(0.0, 10.0);
    m_skyIntensitySpin->setSingleStep(0.01);
    m_skyIntensitySpin->setValue(0.1);
    m_skyIntensitySpin->setSuffix(" W/m²/sr");
    connect(m_skyIntensitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LightingPanel::onSkyIntensityChanged);
    radianceLayout->addRow(tr("Sky:"), m_skyIntensitySpin);

    mainLayout->addWidget(radianceGroup);

    // Atmosphere group
    auto* atmoGroup = new QGroupBox(tr("Atmosphere"));
    auto* atmoLayout = new QFormLayout(atmoGroup);

    // Transmittance
    auto* transRow = new QHBoxLayout();
    m_transmittanceSlider = new QSlider(Qt::Horizontal);
    m_transmittanceSlider->setRange(0, 100);
    m_transmittanceSlider->setValue(90);
    m_transmittanceLabel = new QLabel("0.90");
    m_transmittanceLabel->setFixedWidth(40);
    connect(m_transmittanceSlider, &QSlider::valueChanged,
            this, &LightingPanel::onTransmittanceChanged);
    transRow->addWidget(m_transmittanceSlider);
    transRow->addWidget(m_transmittanceLabel);
    atmoLayout->addRow(tr("Transmittance:"), transRow);

    // Temperature
    m_atmosphereTempSpin = new QDoubleSpinBox();
    m_atmosphereTempSpin->setRange(150.0, 350.0);
    m_atmosphereTempSpin->setSingleStep(5.0);
    m_atmosphereTempSpin->setValue(260.0);
    m_atmosphereTempSpin->setSuffix(" K");
    connect(m_atmosphereTempSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LightingPanel::onAtmosphereTempChanged);
    atmoLayout->addRow(tr("Temperature:"), m_atmosphereTempSpin);

    mainLayout->addWidget(atmoGroup);

    mainLayout->addStretch();
}

void LightingPanel::setLightingParams(const quantiloom::LightingParams& params) {
    // Extract azimuth and elevation from sunDirection
    // sunDirection points FROM surface TO sun
    const glm::vec3& dir = params.sunDirection;

    // Elevation: angle above horizon
    m_sunElevation = std::asin(dir.y) * 180.0f / static_cast<float>(M_PI);
    m_sunElevation = std::max(0.0f, std::min(90.0f, m_sunElevation));

    // Azimuth: horizontal angle (0=North, 90=East)
    m_sunAzimuth = std::atan2(dir.x, dir.z) * 180.0f / static_cast<float>(M_PI);
    if (m_sunAzimuth < 0) m_sunAzimuth += 360.0f;

    // Sun radiance
    m_sunRadiance = params.sunRadiance_rgb;
    m_sunIntensity = (m_sunRadiance.r + m_sunRadiance.g + m_sunRadiance.b) / 3.0f;

    // Sky radiance
    m_skyRadiance = params.skyRadiance_rgb;
    m_skyIntensity = (m_skyRadiance.r + m_skyRadiance.g + m_skyRadiance.b) / 3.0f;

    // Atmosphere
    m_transmittance = params.transmittance;
    m_atmosphereTemp = params.atmosphereTemperature_K;

    m_chromaR_correction = params.chromaR_correction;
    m_chromaB_correction = params.chromaB_correction;
    m_enableShadowRays = (params.enableShadowRays != 0);

    // Update UI (block signals to avoid feedback loop)
    m_azimuthSlider->blockSignals(true);
    m_azimuthSlider->setValue(static_cast<int>(m_sunAzimuth));
    m_azimuthSlider->blockSignals(false);
    m_azimuthLabel->setText(QString("%1°").arg(static_cast<int>(m_sunAzimuth)));

    m_elevationSlider->blockSignals(true);
    m_elevationSlider->setValue(static_cast<int>(m_sunElevation));
    m_elevationSlider->blockSignals(false);
    m_elevationLabel->setText(QString("%1°").arg(static_cast<int>(m_sunElevation)));

    m_sunIntensitySpin->blockSignals(true);
    m_sunIntensitySpin->setValue(m_sunIntensity);
    m_sunIntensitySpin->blockSignals(false);

    m_skyIntensitySpin->blockSignals(true);
    m_skyIntensitySpin->setValue(m_skyIntensity);
    m_skyIntensitySpin->blockSignals(false);

    m_transmittanceSlider->blockSignals(true);
    m_transmittanceSlider->setValue(static_cast<int>(m_transmittance * 100));
    m_transmittanceSlider->blockSignals(false);
    m_transmittanceLabel->setText(QString::number(m_transmittance, 'f', 2));

    m_atmosphereTempSpin->blockSignals(true);
    m_atmosphereTempSpin->setValue(m_atmosphereTemp);
    m_atmosphereTempSpin->blockSignals(false);
}

void LightingPanel::onSunAzimuthChanged(int value) {
    m_sunAzimuth = static_cast<float>(value);
    m_azimuthLabel->setText(QString("%1°").arg(value));
    updateSunDirection();
    emitChanges();
}

void LightingPanel::onSunElevationChanged(int value) {
    m_sunElevation = static_cast<float>(value);
    m_elevationLabel->setText(QString("%1°").arg(value));
    updateSunDirection();
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

void LightingPanel::onTransmittanceChanged(int value) {
    m_transmittance = value / 100.0f;
    m_transmittanceLabel->setText(QString::number(m_transmittance, 'f', 2));
    emitChanges();
}

void LightingPanel::onAtmosphereTempChanged(double value) {
    m_atmosphereTemp = static_cast<float>(value);
    emitChanges();
}

void LightingPanel::updateSunDirection() {
    // Convert azimuth/elevation to direction vector
    float azRad = m_sunAzimuth * static_cast<float>(M_PI) / 180.0f;
    float elRad = m_sunElevation * static_cast<float>(M_PI) / 180.0f;

    // Direction FROM surface TO sun
    // Azimuth: 0=North(+Z), 90=East(+X), 180=South(-Z), 270=West(-X)
    float cosEl = std::cos(elRad);
    glm::vec3 sunDir;
    sunDir.x = cosEl * std::sin(azRad);
    sunDir.y = std::sin(elRad);
    sunDir.z = cosEl * std::cos(azRad);

    // Normalize (should already be unit length, but be safe)
    sunDir = glm::normalize(sunDir);
}

void LightingPanel::emitChanges() {
    // Convert azimuth/elevation to direction vector
    float azRad = m_sunAzimuth * static_cast<float>(M_PI) / 180.0f;
    float elRad = m_sunElevation * static_cast<float>(M_PI) / 180.0f;

    float cosEl = std::cos(elRad);
    glm::vec3 sunDir;
    sunDir.x = cosEl * std::sin(azRad);
    sunDir.y = std::sin(elRad);
    sunDir.z = cosEl * std::cos(azRad);
    sunDir = glm::normalize(sunDir);

    // Build params
    quantiloom::LightingParams params{};
    params.sunDirection = sunDir;
    params.sunRadiance_spectral = m_sunIntensity;
    params.sunRadiance_rgb = m_sunRadiance;
    params.skyRadiance_spectral = m_skyIntensity;
    params.skyRadiance_rgb = m_skyRadiance;
    params.transmittance = m_transmittance;
    params.worldUnitsToMeters = 1.0f;
    params.atmosphereTemperature_K = m_atmosphereTemp;

    params.chromaR_correction = m_chromaR_correction;
    params.chromaB_correction = m_chromaB_correction;
    params.enableShadowRays = m_enableShadowRays ? 1u : 0u;

    emit lightingChanged(params);
}
