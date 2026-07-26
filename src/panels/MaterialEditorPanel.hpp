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

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QSlider;
class QLabel;
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

    void setMaterial(int index, const quantiloom::Material* material);
    void clear();

    [[nodiscard]] int currentMaterialIndex() const { return m_currentIndex; }

signals:
    void materialChanged(int index, const quantiloom::Material& material);

private slots:
    void onBaseColorClicked();
    void onMetallicChanged(int value);
    void onRoughnessChanged(int value);
    void onEmissiveChanged();
    void onIRPropertyChanged();
    void applyChanges();

private:
    void setupUi();
    void updateColorButton(QPushButton* btn, const glm::vec3& color);
    void updateKirchhoffLabel();
    void updateSpectralCurveNotice();
    /// Returns false when the user declined to overwrite generated curves.
    bool confirmSpectralOverwrite();

    int m_currentIndex = -1;
    const quantiloom::Material* m_currentMaterial = nullptr;

    // UI elements
    QLabel* m_materialName = nullptr;
    QPushButton* m_baseColorBtn = nullptr;
    QSlider* m_metallicSlider = nullptr;
    QLabel* m_metallicLabel = nullptr;
    QSlider* m_roughnessSlider = nullptr;
    QLabel* m_roughnessLabel = nullptr;
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

    // IR UI elements
    QGroupBox* m_irGroup = nullptr;
    QDoubleSpinBox* m_irEmissivitySpin = nullptr;
    QDoubleSpinBox* m_irTransmittanceSpin = nullptr;
    QDoubleSpinBox* m_irTemperatureSpin = nullptr;
    QLabel* m_irKirchhoffLabel = nullptr;
    QLabel* m_spectralCurveNotice = nullptr;

    /// The material arrived with more sample points than this editor writes,
    /// which means the generator produced them.
    bool m_hasGeneratedCurves = false;
    /// The user has already agreed to overwrite them for this material.
    bool m_overwriteConfirmed = false;
};
