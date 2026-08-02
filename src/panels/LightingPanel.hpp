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
class QCheckBox;
class QDoubleSpinBox;
class QPushButton;
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

    /// The environment map the open document names, and whether it lights the
    /// scene. An empty path is a scene with no map of its own, which still has
    /// image-based lighting from the fallback sky unless @p enabled is false.
    void setEnvironmentMap(const QString& path, bool enabled);

signals:
    /// Carries a full LightingParams for convenience, but only the sun, sky and
    /// environment-map fields are this panel's to set; the shell keeps the
    /// atmospheric ones.
    void lightingChanged(const quantiloom::LightingParams& params);

    /// The map to light from, and whether to light from it. An empty path with
    /// @p enabled true means the fallback sky.
    void environmentMapChanged(const QString& path, bool enabled);

private slots:
    void onSunAzimuthChanged(int value);
    void onSunElevationChanged(int value);
    void onSunIntensityChanged(double value);
    void onSkyIntensityChanged(double value);
    void onEnvironmentEnabledToggled(bool enabled);
    void onBrowseEnvironmentMap();
    void onClearEnvironmentMap();

protected:
    // The environment-map path is elided to the label's current width, so the
    // elision has to be recomputed when the dock is resized.
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUi();
    void emitChanges();
    /// Show the path elided to the label's width, with the whole of it in the
    /// tooltip. Also refreshes the double-count notice.
    void updateEnvironmentDisplay();

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

    // Environment map (image-based lighting)
    QString m_environmentMapPath;
    bool m_environmentEnabled = true;

    // UI elements
    QGroupBox* m_sunDirGroup = nullptr;
    QGroupBox* m_radianceGroup = nullptr;
    QGroupBox* m_envGroup = nullptr;
    QCheckBox* m_envEnabledCheck = nullptr;
    QLabel* m_envPathLabel = nullptr;
    QPushButton* m_envBrowseButton = nullptr;
    QPushButton* m_envClearButton = nullptr;
    QLabel* m_doubleCountNotice = nullptr;
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
