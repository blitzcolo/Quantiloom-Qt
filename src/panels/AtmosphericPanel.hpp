/**
 * @file AtmosphericPanel.hpp
 * @brief Panel for NN atmosphere (MODTRAN surrogate) configuration
 *
 * @author wtflmao
 */

#pragma once

#include <QWidget>
#include <atmos/AtmosphereNNConfig.hpp>

QT_BEGIN_NAMESPACE
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
QT_END_NAMESPACE

/**
 * @class AtmosphericPanel
 * @brief UI panel for the NN atmosphere configuration
 *
 * Provides preset selection, model pack directory selection, and advanced
 * weather feature tuning for the MODTRAN surrogate network atmosphere.
 */
class AtmosphericPanel : public QWidget {
    Q_OBJECT

public:
    explicit AtmosphericPanel(QWidget* parent = nullptr);
    ~AtmosphericPanel() override = default;

    /**
     * @brief Set current preset from config file
     * @param preset Preset name string
     */
    void setPreset(const QString& preset);

    /**
     * @brief Get current preset name
     */
    QString preset() const;

    /**
     * @brief Set full atmosphere configuration
     * @param config Configuration to display
     */
    void setAtmosphericConfig(const quantiloom::AtmosphereNNConfig& config);

    /**
     * @brief Get current atmosphere configuration
     */
    quantiloom::AtmosphereNNConfig getAtmosphericConfig() const;

signals:
    /**
     * @brief Emitted when preset changes
     * @param preset New preset name
     */
    void presetChanged(const QString& preset);

    /**
     * @brief Emitted when any configuration value changes
     * @param config Updated configuration
     */
    void configChanged(const quantiloom::AtmosphereNNConfig& config);

private slots:
    void onPresetChanged(int index);
    void onAdvancedParamChanged();
    void onEnabledChanged(bool enabled);
    void onBrowseModelPack();

private:
    void setupUi();
    void updateAdvancedParamsFromConfig(const quantiloom::AtmosphereNNConfig& config);
    void blockSignalsForUpdate(bool block);
    static void selectComboData(QComboBox* combo, double value);

    // Preset selector
    QComboBox* m_presetCombo = nullptr;
    QCheckBox* m_enabledCheck = nullptr;

    // Model pack directory (empty = auto-resolve)
    QLineEdit* m_modelPackEdit = nullptr;
    QPushButton* m_modelPackBrowse = nullptr;

    // Advanced parameters group
    QGroupBox* m_advancedGroup = nullptr;

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

    // Current config
    quantiloom::AtmosphereNNConfig m_config;
    bool m_updatingUi = false;
};
