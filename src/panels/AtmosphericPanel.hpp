/**
 * @file AtmosphericPanel.hpp
 * @brief The atmosphere: both the analytic terms and the NN (MODTRAN surrogate)
 *
 * There used to be two places to configure the atmosphere: an "Atmosphere"
 * group at the bottom of the lighting panel holding transmittance and
 * atmosphere temperature, and this panel holding the neural network model.
 * They describe the same physical object, neither mentioned the other, and a
 * user could set both without any indication of what each one fed.
 *
 * They are one panel now, in two clearly labelled groups. The panel does not
 * claim the two are mutually exclusive: how the SDK combines them is the SDK's
 * business, and asserting an exclusivity it does not implement would be a new
 * lie in place of the old one. What the panel does state is which parameter
 * block each group feeds.
 *
 * @author blitzcolo
 */

#pragma once

#include "../ui/PanelBase.hpp"

#include <atmos/AtmosphereNNConfig.hpp>

QT_BEGIN_NAMESPACE
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
QT_END_NAMESPACE

class CollapsibleGroupBox;

class AtmosphericPanel : public PanelBase {
    Q_OBJECT

public:
    explicit AtmosphericPanel(QWidget* parent = nullptr);
    ~AtmosphericPanel() override = default;

    [[nodiscard]] QString panelTitle() const override;
    [[nodiscard]] QString panelId() const override { return QStringLiteral("atmosphere"); }
    void retranslateUi() override;

    /**
     * @brief Set current preset from config file
     */
    void setPreset(const QString& preset);

    /**
     * @brief Get current preset name
     */
    [[nodiscard]] QString preset() const;

    /**
     * @brief Set full NN atmosphere configuration
     */
    void setAtmosphericConfig(const quantiloom::AtmosphereNNConfig& config);

    /**
     * @brief Get current NN atmosphere configuration
     */
    [[nodiscard]] quantiloom::AtmosphereNNConfig getAtmosphericConfig() const;

    /**
     * @brief Set the analytic sky, which lives in LightingParams
     *
     * @param clearSky   true = the flat-slab model, false = one isotropic
     *                   blackbody at the air temperature
     * @param airTemperatureK the sky's Planck temperature under the slab
     *                   model, and the whole of the isotropic one
     * @param relativeHumidity what the emissivity is derived from; carried
     *                   even when the model is off, so turning it back on
     *                   does not lose it
     */
    void setAnalyticSky(bool clearSky, float airTemperatureK, float relativeHumidity);

    [[nodiscard]] bool clearSkyEnabled() const;
    [[nodiscard]] float atmosphereTemperatureK() const { return m_atmosphereTempK; }
    [[nodiscard]] float relativeHumidity() const { return m_relativeHumidity; }

signals:
    void presetChanged(const QString& preset);
    void configChanged(const quantiloom::AtmosphereNNConfig& config);

    /// The analytic sky, emitted separately because it belongs to a different
    /// SDK struct than everything else on this panel.
    void analyticSkyChanged(bool clearSky, float airTemperatureK, float relativeHumidity);

private slots:
    void onPresetChanged(int index);
    void onAdvancedParamChanged();
    void onEnabledChanged(bool enabled);
    void onBrowseModelPack();
    void onAnalyticChanged();

private:
    void setupUi();
    void updateAdvancedParamsFromConfig(const quantiloom::AtmosphereNNConfig& config);
    void blockSignalsForUpdate(bool block);
    static void selectComboData(QComboBox* combo, double value);

    // --- analytic group -------------------------------------------------
    QGroupBox* m_analyticGroup = nullptr;
    QLabel* m_analyticNote = nullptr;
    QCheckBox* m_clearSkyCheck = nullptr;
    QDoubleSpinBox* m_atmosphereTempSpin = nullptr;
    QLabel* m_atmosphereTempCaption = nullptr;
    QDoubleSpinBox* m_humiditySpin = nullptr;
    QLabel* m_humidityCaption = nullptr;
    /// Dew point, zenith emissivity and effective sky temperature, all derived
    /// by the SDK from the two fields above. Shown because the emissivity is
    /// the number that actually reaches the shader and nothing else displays
    /// it, and because a user setting a humidity has no other way to see what
    /// it bought.
    QLabel* m_derivedLabel = nullptr;
    void updateDerivedLabel();

    float m_atmosphereTempK = 260.0f;
    float m_relativeHumidity = 50.0f;

    // --- NN group -------------------------------------------------------
    QGroupBox* m_nnGroup = nullptr;
    QCheckBox* m_enabledCheck = nullptr;
    QComboBox* m_presetCombo = nullptr;
    QLabel* m_presetCaption = nullptr;
    QLineEdit* m_modelPackEdit = nullptr;
    QLabel* m_modelPackCaption = nullptr;
    QPushButton* m_modelPackBrowse = nullptr;

    CollapsibleGroupBox* m_advancedGroup = nullptr;

    // Discrete MODTRAN features
    QComboBox* m_atmosModelCombo = nullptr;   // {2, 3}
    QComboBox* m_ihazeCombo = nullptr;        // {1, 4, 5, 9, 10}
    QComboBox* m_icldCombo = nullptr;         // {0, 6, 18}

    // Continuous weather features
    QDoubleSpinBox* m_visKm = nullptr;        // [0.5, 50]
    QDoubleSpinBox* m_rainrtMmH = nullptr;    // [0, 50]
    QDoubleSpinBox* m_tGroundK = nullptr;     // [253, 328]
    QDoubleSpinBox* m_rh = nullptr;           // [0.05, 1]
    QDoubleSpinBox* m_pHPa = nullptr;         // [950, 1040]
    QDoubleSpinBox* m_h2oScale = nullptr;     // [0.5, 2]

    // Captions kept as members so retranslateUi() can reach them
    QLabel* m_atmosModelCaption = nullptr;
    QLabel* m_ihazeCaption = nullptr;
    QLabel* m_icldCaption = nullptr;
    QLabel* m_visCaption = nullptr;
    QLabel* m_rainCaption = nullptr;
    QLabel* m_tGroundCaption = nullptr;
    QLabel* m_rhCaption = nullptr;
    QLabel* m_pressureCaption = nullptr;
    QLabel* m_h2oCaption = nullptr;

    quantiloom::AtmosphereNNConfig m_config;
    bool m_updatingUi = false;
};
