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
#include <renderer/LightingParams.hpp>
#include <glm/glm.hpp>

#include <optional>
#include <utility>

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

    /**
     * @brief Re-pick the illuminant for the mode now in force
     *
     * The other half of setSpectralMode, kept separate because only a *user*
     * switching mode may move the setting; a document being loaded may not.
     * The shell calls this from its applySpectralMode dispatcher and nowhere
     * else. A built-in spectrum going into RGB is replaced by "none", and a
     * lit sun with no spectrum going into a spectral mode by ASTM; otherwise
     * the normalisation is re-picked. Emits illuminantChanged for each that
     * actually moves.
     */
    void repickForSpectralMode();

    /// Whether @p kind is listed now: offered by the mode, or the one in
    /// force. The Render menu lists the same entries as the panel's combo.
    [[nodiscard]] bool showsIlluminantKind(const QString& kind) const;

    /// The panel's label for @p kind in the current mode and language, so the
    /// Render menu names the entries exactly as the combo does.
    [[nodiscard]] QString illuminantKindText(const QString& kind) const;

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
    void onSunAzimuthSpinChanged(double value);
    void onSunElevationSpinChanged(double value);
    void onSunIntensityChanged(double value);
    void onSkyIntensityChanged(double value);
    void onEnvironmentEnabledToggled(bool enabled);

public slots:
    /// The Render ▸ Lighting entries. Each does exactly what its widget does,
    /// through the widget's own handler, so the two routes cannot differ.
    void onBrowseEnvironmentMap();
    void onClearEnvironmentMap();
    void requestEnvironmentEnabled(bool enabled);
    /// "none", "equal_energy", "astm" or "file"; "file" asks for the file.
    void requestIlluminantKind(const QString& kind);

public:
    [[nodiscard]] bool environmentEnabled() const { return m_environmentEnabled; }
    [[nodiscard]] bool hasEnvironmentMap() const { return !m_environmentMapPath.isEmpty(); }
    [[nodiscard]] QString illuminantKind() const { return m_illuminant.kind; }

protected:
    // The environment-map path is elided to the label's current width, so the
    // elision has to be recomputed when the dock is resized.
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUi();
    void emitChanges();
    /// emitChanges() wrapped as one undoable edit unless a slider drag is
    /// already bracketing it.
    void emitEdit();
    void syncAngleWidgets();
    /// Show or hide the file row and the list entries, and rebuild the notice
    /// from illuminantNotices().
    void updateIlluminantWidgets();

    /// The normalisation half of repickForSpectralMode. Emits
    /// illuminantChanged when it actually moves.
    void repickNormalisation();
    /// Set the normalisation and apply it; nothing when it is already so.
    void setNormalisation(bool normalise);

    /// Whether the mode offers @p kind at all: the built-in spectra ("astm",
    /// "equal_energy") only the spectral modes, "none" and a file every mode.
    [[nodiscard]] bool illuminantKindOffered(const QString& kind) const;

    /// Whether the analytic sun or sky has any strength.
    [[nodiscard]] bool sunOrSkyLit() const;

    /// "None" chosen in a spectral mode: zero the sun and sky strengths, as
    /// one undoable lighting edit. Nothing if both are already zero.
    void switchSunAndSkyOff();

    /// Show the strengths, or 0 in fields that take no input where "None"
    /// has switched the sun and sky off in a spectral mode.
    void syncStrengthWidgets();

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
    /// The RGB shape of the sun radiance, normalised to mean 1.
    glm::vec3 m_sunChroma{1.0f};

    // Sky radiance
    glm::vec3 m_skyRadiance{0.1f, 0.15f, 0.2f};
    float m_skyIntensity = 0.1f;
    /// The RGB shape of the sky radiance, normalised to mean 1.
    glm::vec3 m_skyChroma{2.0f / 3.0f, 1.0f, 4.0f / 3.0f};

    // Not in the UI: carried so that emitChanges() does not drop an override a
    // config supplied. Initialised from the SDK rather than typed out, because
    // typed-out copies drift -- these held 0.7872 and 1.0437 after the core
    // stopped defaulting to them, so moving the sun slider before a scene was
    // loaded pushed the retired white balance back into the renderer.
    float m_chromaR_correction = quantiloom::LightingDefaults::CHROMA_R_CORRECTION;
    float m_chromaB_correction = quantiloom::LightingDefaults::CHROMA_B_CORRECTION;
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
    QDoubleSpinBox* m_azimuthSpin = nullptr;
    QLabel* m_azimuthCaption = nullptr;
    QSlider* m_elevationSlider = nullptr;
    QDoubleSpinBox* m_elevationSpin = nullptr;
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
    /// The mode before the last setSpectralMode, so a switch can tell coming
    /// out of RGB from moving between two spectral modes.
    quantiloom::SpectralMode m_previousSpectralMode = quantiloom::SpectralMode::RGB;
    /// A file's path and the normalisation it had before RGB normalised it,
    /// given back when the mode leaves RGB with the same file still chosen.
    std::optional<std::pair<QString, bool>> m_fileNormaliseBeforeRgb;
    QGroupBox* m_illuminantGroup = nullptr;
    QComboBox* m_illuminantCombo = nullptr;
    QLabel* m_illuminantPathLabel = nullptr;
    QPushButton* m_illuminantBrowseButton = nullptr;
    QCheckBox* m_normaliseCheck = nullptr;
    QLabel* m_illuminantNotice = nullptr;
};
