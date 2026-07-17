/**
 * @file LightingPanel.hpp
 * @brief Sun/sky lighting parameter editor panel
 */

#pragma once

#include <QWidget>
#include <glm/glm.hpp>

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QSlider;
class QLabel;
class QPushButton;
class QGroupBox;
QT_END_NAMESPACE

namespace quantiloom {
struct LightingParams;
}

/**
 * @class LightingPanel
 * @brief Editor for sun/sky lighting parameters
 */
class LightingPanel : public QWidget {
    Q_OBJECT

public:
    explicit LightingPanel(QWidget* parent = nullptr);

    void setLightingParams(const quantiloom::LightingParams& params);

signals:
    void lightingChanged(const quantiloom::LightingParams& params);

private slots:
    void onSunAzimuthChanged(int value);
    void onSunElevationChanged(int value);
    void onSunIntensityChanged(double value);
    void onSkyIntensityChanged(double value);
    void onAtmosphereTempChanged(double value);
    void onTransmittanceChanged(int value);

private:
    void setupUi();
    void updateSunDirection();
    void emitChanges();

    // Sun angles (degrees)
    float m_sunAzimuth = 180.0f;    // 0 = North, 90 = East, 180 = South
    float m_sunElevation = 45.0f;   // 0 = horizon, 90 = zenith

    // Sun radiance
    glm::vec3 m_sunRadiance{1.0f};
    float m_sunIntensity = 1.0f;

    // Sky radiance
    glm::vec3 m_skyRadiance{0.1f, 0.15f, 0.2f};
    float m_skyIntensity = 0.1f;

    // Atmosphere
    float m_transmittance = 0.9f;
    float m_atmosphereTemp = 260.0f;

    float m_chromaR_correction = 0.7872f;
    float m_chromaB_correction = 1.0437f;
    bool m_enableShadowRays = false;

    // UI elements
    QSlider* m_azimuthSlider = nullptr;
    QLabel* m_azimuthLabel = nullptr;
    QSlider* m_elevationSlider = nullptr;
    QLabel* m_elevationLabel = nullptr;
    QDoubleSpinBox* m_sunIntensitySpin = nullptr;
    QDoubleSpinBox* m_skyIntensitySpin = nullptr;
    QSlider* m_transmittanceSlider = nullptr;
    QLabel* m_transmittanceLabel = nullptr;
    QDoubleSpinBox* m_atmosphereTempSpin = nullptr;
};
