/**
 * @file MaterialEditorPanel.hpp
 * @brief PBR material property editor panel
 *
 * A material's infrared properties can be written from two places: the simple
 * two-point constants here, and the full spectral curves the material
 * generator produces. Both emit a material change and the later write wins,
 * silently. This panel now says so — it reports when the material it is
 * showing carries generated curves, and asks before flattening them.
 */

#pragma once

#include "../ui/PanelBase.hpp"

#include <glm/glm.hpp>

#include "../config/ConfigManager.hpp"  // MaterialThermalProps

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QSlider;
class QLabel;
class QLineEdit;
class QComboBox;
class QPushButton;
class QGroupBox;
class QCheckBox;
QT_END_NAMESPACE

namespace quantiloom {
struct Material;
}

/**
 * @class MaterialEditorPanel
 * @brief Editor for PBR material properties
 */
class MaterialEditorPanel : public PanelBase {
    Q_OBJECT

public:
    explicit MaterialEditorPanel(QWidget* parent = nullptr);

    [[nodiscard]] QString panelTitle() const override;
    [[nodiscard]] QString panelId() const override { return QStringLiteral("material"); }
    void retranslateUi() override;
    void restyleUi() override;

    void setMaterial(int index, const quantiloom::Material* material);

    /// The thermal properties for the material now shown, from the config --
    /// the only place they exist, since the Material does not carry them.
    void setThermalProperties(const MaterialThermalProps& props);

    /**
     * @brief Write the scalar infrared trio into a material as flat curves
     *
     * Emissivity and transmittance are stored as curves; this panel shows one
     * number for each and writes it as a two-point curve spanning the MWIR and
     * LWIR bands, then derives reflectance from what is left over. Static, and
     * public, because anything that edits a material the way this panel does
     * has to produce the same material -- the reflectance in particular is not
     * given, it is 1 - emissivity - transmittance, and a second copy of that
     * arithmetic is a second chance to get it wrong.
     *
     * Does nothing when all three are zero, which leaves an existing curve set
     * alone rather than clearing it.
     */
    static void applyIrScalars(quantiloom::Material& material, float emissivity,
                               float transmittance, float temperatureK);
    void clear();

    [[nodiscard]] int currentMaterialIndex() const { return m_currentIndex; }

signals:
    void materialChanged(int index, const quantiloom::Material& material);

    /// Thermal properties for the material at @p index. A separate signal
    /// because these do not live on quantiloom::Material -- they are solver
    /// inputs, and that header is a layout contract with no room for them.
    void thermalPropertiesChanged(int index, const MaterialThermalProps& props);

private slots:
    void onBaseColorClicked();
    void onTransmissionChanged();
    void onAttenuationColorClicked();
    void onMetallicChanged(int value);
    void onMetallicSpinChanged(double value);
    void onRoughnessChanged(int value);
    void onRoughnessSpinChanged(double value);
    void onEmissiveChanged();
    void onIRPropertyChanged();
    void onTemperatureMapChanged();
    void onBrowseTemperatureTexture();
    void onThermalPropertyChanged();
    void applyChanges();

private:
    void setupUi();
    void updateColorButton(QPushButton* btn, const glm::vec3& color);
    void updateKirchhoffLabel();
    void updateSpectralCurveNotice();
    /// Says what the map's kelvin range and step work out to, and that a path
    /// change needs the scene reloaded.
    void updateTemperatureMapNotice();
    /// Returns false when the user declined to overwrite generated curves.
    bool confirmSpectralOverwrite();

    int m_currentIndex = -1;
    const quantiloom::Material* m_currentMaterial = nullptr;

    // UI elements
    QLabel* m_materialName = nullptr;
    QPushButton* m_baseColorBtn = nullptr;
    // Transmission and volume
    float m_transmission = 0.0f;
    float m_ior = 1.5f;
    float m_dispersion = 0.0f;
    float m_attenuationDistance = 0.0f;
    glm::vec3 m_attenuationColor{1.0f, 1.0f, 1.0f};
    class CollapsibleGroupBox* m_transmissionGroup = nullptr;
    QDoubleSpinBox* m_transmissionSpin = nullptr;
    QDoubleSpinBox* m_iorSpin = nullptr;
    QDoubleSpinBox* m_dispersionSpin = nullptr;
    QDoubleSpinBox* m_attenuationDistanceSpin = nullptr;
    QPushButton* m_attenuationColorBtn = nullptr;

    QSlider* m_metallicSlider = nullptr;
    QDoubleSpinBox* m_metallicSpin = nullptr;
    QSlider* m_roughnessSlider = nullptr;
    QDoubleSpinBox* m_roughnessSpin = nullptr;
    QDoubleSpinBox* m_emissiveR = nullptr;
    QDoubleSpinBox* m_emissiveG = nullptr;
    QDoubleSpinBox* m_emissiveB = nullptr;

    // Current values
    glm::vec4 m_baseColor{1.0f};
    float m_metallic = 0.0f;
    float m_roughness = 1.0f;
    glm::vec3 m_emissive{0.0f};

    // IR property values
    float m_irEmissivity = 0.0f;
    float m_irTransmittance = 0.0f;
    float m_irTemperature_K = 0.0f;

    /// A temperature field instead of one temperature. The path is the
    /// material's own, mounted by the core when the scene loads; the scale and
    /// offset are live, because they are material data rather than a texture.
    QString m_temperatureTexture;
    float m_temperatureScale = 500.0f;
    float m_temperatureOffset = 200.0f;

    // IR UI elements
    QGroupBox* m_irGroup = nullptr;
    QDoubleSpinBox* m_irEmissivitySpin = nullptr;
    QDoubleSpinBox* m_irTransmittanceSpin = nullptr;
    QDoubleSpinBox* m_irTemperatureSpin = nullptr;
    QLineEdit* m_temperatureTextureEdit = nullptr;
    QPushButton* m_temperatureTextureBrowse = nullptr;
    QDoubleSpinBox* m_temperatureScaleSpin = nullptr;
    QDoubleSpinBox* m_temperatureOffsetSpin = nullptr;
    QLabel* m_temperatureMapNotice = nullptr;

    // Thermal properties, for the offline surface energy balance. Collapsed
    // by default: a scene that is not being solved has no use for them, and
    // this panel is long.
    class CollapsibleGroupBox* m_thermalGroup = nullptr;
    QDoubleSpinBox* m_thermalConductivity = nullptr;
    QDoubleSpinBox* m_thermalDensity = nullptr;
    QDoubleSpinBox* m_thermalSpecificHeat = nullptr;
    QDoubleSpinBox* m_thermalThickness = nullptr;
    QDoubleSpinBox* m_thermalConvection = nullptr;
    QDoubleSpinBox* m_thermalAbsorptivity = nullptr;
    QComboBox* m_thermalInteriorBc = nullptr;
    QDoubleSpinBox* m_thermalInteriorTemp = nullptr;
    QLabel* m_thermalNotice = nullptr;
    MaterialThermalProps m_thermal;
    void updateThermalNotice();
    QLabel* m_irKirchhoffLabel = nullptr;
    QLabel* m_spectralCurveNotice = nullptr;

    /// The material arrived with more sample points than this editor writes,
    /// which means the generator produced them.
    bool m_hasGeneratedCurves = false;
    /// The user has already agreed to overwrite them for this material.
    bool m_overwriteConfirmed = false;
};
