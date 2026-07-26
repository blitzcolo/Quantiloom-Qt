/**
 * @file LightingPanel.hpp
 * @brief Sun/sky lighting parameter editor panel
 *
 * Lighting only. The atmosphere group that used to sit at the bottom of this
 * panel — transmittance and atmosphere temperature — described the same
 * physical object as the separate atmosphere panel, neither knew about the
 * other, and both were editable at once. Those two controls now live with the
 * rest of the atmosphere.
 */

#pragma once

#include "../ui/PanelBase.hpp"

#include <glm/glm.hpp>

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QSlider;
class QLabel;
class QGroupBox;
QT_END_NAMESPACE

namespace quantiloom {
struct LightingParams;
}

/**
 * @class LightingPanel
 * @brief Editor for sun/sky lighting parameters
 */
class LightingPanel : public PanelBase {
    Q_OBJECT

public:
    explicit LightingPanel(QWidget* parent = nullptr);

    [[nodiscard]] QString panelTitle() const override;
    [[nodiscard]] QString panelId() const override { return QStringLiteral("lighting"); }
    void retranslateUi() override;

    void setLightingParams(const quantiloom::LightingParams& params);

signals:
    /// Carries a full LightingParams for convenience, but only the sun and sky
    /// fields are this panel's to set; the shell keeps the atmospheric ones.
    void lightingChanged(const quantiloom::LightingParams& params);

private slots:
    void onSunAzimuthChanged(int value);
    void onSunElevationChanged(int value);
    void onSunIntensityChanged(double value);
    void onSkyIntensityChanged(double value);

private:
    void setupUi();
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

    float m_chromaR_correction = 0.7872f;
    float m_chromaB_correction = 1.0437f;
    bool m_enableShadowRays = false;

    // UI elements
    QGroupBox* m_sunDirGroup = nullptr;
    QGroupBox* m_radianceGroup = nullptr;
    QSlider* m_azimuthSlider = nullptr;
    QLabel* m_azimuthLabel = nullptr;
    QLabel* m_azimuthCaption = nullptr;
    QSlider* m_elevationSlider = nullptr;
    QLabel* m_elevationLabel = nullptr;
    QLabel* m_elevationCaption = nullptr;
    QDoubleSpinBox* m_sunIntensitySpin = nullptr;
    QLabel* m_sunCaption = nullptr;
    QDoubleSpinBox* m_skyIntensitySpin = nullptr;
    QLabel* m_skyCaption = nullptr;
};
