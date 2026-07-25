/**
 * @file SensorPanel.hpp
 * @brief Panel for sensor simulation configuration
 *
 * @author wtflmao
 */

#pragma once

#include <QWidget>
#include <postprocess/SensorModel.hpp>

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QSpinBox;
class QGroupBox;
class QCheckBox;
class QComboBox;
class QLabel;
QT_END_NAMESPACE

/**
 * @class SensorPanel
 * @brief UI panel for sensor simulation configuration
 *
 * Provides controls for optical parameters (focal length, f-number),
 * detector parameters (pixel pitch, QE, well capacity), and noise models.
 */
class SensorPanel : public QWidget {
    Q_OBJECT

public:
    explicit SensorPanel(QWidget* parent = nullptr);
    ~SensorPanel() override = default;

    /**
     * @brief Set sensor enabled state
     *
     * Not named setEnabled: that is a non-virtual QWidget method meaning
     * "make this widget interactive", and a QWidget* handle would silently
     * reach it instead of this one.
     */
    void setSensorEnabled(bool enabled);

    /**
     * @brief Check if sensor is enabled
     */
    bool isSensorEnabled() const;

    /**
     * @brief Set sensor parameters
     */
    void setSensorParams(const quantiloom::SensorParams& params);

    /**
     * @brief Get current sensor parameters
     */
    quantiloom::SensorParams getSensorParams() const;

signals:
    /**
     * @brief Emitted when enabled state changes
     */
    void enabledChanged(bool enabled);

    /**
     * @brief Emitted when any parameter changes
     */
    void paramsChanged(const quantiloom::SensorParams& params);

private slots:
    void onEnabledChanged(bool enabled);
    void onParamChanged();
    void onFpnToggled(bool checked);
    void onNucToggled(bool checked);

private:
    void setupUi();
    void updateUiFromParams(const quantiloom::SensorParams& params);
    void blockSignalsForUpdate(bool block);

    // Enable checkbox
    QCheckBox* m_enabledCheck = nullptr;

    // Optics group
    QGroupBox* m_opticsGroup = nullptr;
    QDoubleSpinBox* m_focalLength = nullptr;
    QDoubleSpinBox* m_fNumber = nullptr;

    // Detector group
    QGroupBox* m_detectorGroup = nullptr;
    QDoubleSpinBox* m_pixelPitch = nullptr;
    QDoubleSpinBox* m_quantumEfficiency = nullptr;
    QDoubleSpinBox* m_wellCapacity = nullptr;
    QSpinBox* m_bitDepth = nullptr;
    QDoubleSpinBox* m_integrationTime = nullptr;

    // ADC group
    QGroupBox* m_adcGroup = nullptr;
    QDoubleSpinBox* m_gain = nullptr;

    // Noise group
    QGroupBox* m_noiseGroup = nullptr;
    QDoubleSpinBox* m_readNoise = nullptr;
    QDoubleSpinBox* m_darkCurrent = nullptr;
    QCheckBox* m_poissonNoise = nullptr;
    QCheckBox* m_readNoiseEnable = nullptr;
    QCheckBox* m_darkCurrentEnable = nullptr;
    QCheckBox* m_fpnNoise = nullptr;

    // FPN sub-group
    QGroupBox* m_fpnGroup = nullptr;
    QDoubleSpinBox* m_prnuSigma = nullptr;
    QDoubleSpinBox* m_dsnuSigma = nullptr;
    QCheckBox* m_nucEnable = nullptr;
    QDoubleSpinBox* m_nucEfficiency = nullptr;

    // IR Detector group
    QGroupBox* m_irGroup = nullptr;
    QDoubleSpinBox* m_detectorTemp = nullptr;

    // Current params
    quantiloom::SensorParams m_params;
    bool m_updatingUi = false;
};
