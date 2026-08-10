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

#include <core/Types.hpp>
#include <glm/glm.hpp>

#include <optional>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QComboBox;
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

    /**
     * @struct IlluminantChoice
     * @brief What the user picked for the sun's spectrum
     *
     * Deliberately a *choice* rather than the loaded curves: the panel does
     * not read spectrum files, and the core's own reading of solar_lut is the
     * only one that may decide what a file means.
     */
    struct IlluminantChoice {
        /// "none", "equal_energy", "astm" or "file". Protocol, not display
        /// text -- the shell maps these onto the core's SolarLutSpec.
        QString kind = QStringLiteral("none");
        /// Only meaningful when kind is "file".
        QString path;
        bool normaliseUnitLuminance = true;
    };

    void setIlluminant(const IlluminantChoice& choice);
    [[nodiscard]] IlluminantChoice illuminant() const;

    /**
     * @brief The mode the viewport is rendering, which the illuminant answers to
     *
     * The panel has no other way to know, and three things depend on it: whether
     * a spectrum is needed at all, whether normalising one is right, and whether
     * the spectrum reaches the wavelengths being rendered.
     *
     * Pure: it refreshes the notices and emits nothing. Loading a configuration
     * goes through this and setIlluminant(), and neither may overrule what the
     * document said -- a panel that quietly disagreed with a file would make
     * this application render it differently from the CLI, which is the
     * divergence class the config reading exists to prevent.
     */
    void setSpectralMode(quantiloom::SpectralMode mode);

signals:
    /// Carries a full LightingParams for convenience, but only the sun, sky and
    /// environment-map fields are this panel's to set; the shell keeps the
    /// atmospheric ones.
    void lightingChanged(const quantiloom::LightingParams& params);

    /// The map to light from, and whether to light from it. An empty path with
    /// @p enabled true means the fallback sky.
    void environmentMapChanged(const QString& path, bool enabled);

    /// A slider drag began and ended. The shell snapshots on the first and
    /// pushes one undo entry on the second, so a drag is one step rather than
    /// forty -- and so the panel is not written back to mid-drag, which would
    /// round azimuth to whole degrees and fight the drag.
    void editGestureStarted();
    void editGestureFinished();

    /// The illuminant choice changed. The shell turns it into a SolarLutSpec
    /// and hands it to the core, which does the reading.
    void illuminantChanged(const IlluminantChoice& choice);

private slots:
    /// The combo changed. Distinct from the checkbox because only a change of
    /// illuminant re-picks the recommended normalisation -- toggling the box
    /// is the user overriding that recommendation, and must stick.
    void onIlluminantKindChanged();
    void onNormaliseToggled();
    void onBrowseIlluminant();
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
    /// Show or hide the file row, and rebuild the notice from illuminantNotices().
    void updateIlluminantWidgets();

    /// What the normalisation should be for the illuminant now chosen, or empty
    /// where the panel cannot know. Applied when the *illuminant* changes, never
    /// when a document is loaded and never when the user toggles the box.
    [[nodiscard]] std::optional<bool> recommendedNormalise() const;

    /// Everything currently worth telling the user about the illuminant, most
    /// severe first. Empty hides the notice.
    [[nodiscard]] QStringList illuminantNotices() const;

    /// Whether the current mode reports radiance rather than a picture. False
    /// for RGB and for the preview-only fused IR bands.
    [[nodiscard]] bool spectralModeIsQuantitative() const;
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

    // Illuminant
    IlluminantChoice m_illuminant;
    /// Set by the shell. Drives the notices and the recommended normalisation;
    /// the panel never renders anything itself.
    quantiloom::SpectralMode m_spectralMode = quantiloom::SpectralMode::RGB;
    QGroupBox* m_illuminantGroup = nullptr;
    QComboBox* m_illuminantCombo = nullptr;
    QLabel* m_illuminantPathLabel = nullptr;
    QPushButton* m_illuminantBrowseButton = nullptr;
    QCheckBox* m_normaliseCheck = nullptr;
    QLabel* m_illuminantNotice = nullptr;
};
