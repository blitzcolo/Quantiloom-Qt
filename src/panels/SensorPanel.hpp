/**
 * @file SensorPanel.hpp
 * @brief Panel for sensor simulation configuration
 *
 * @author blitzcolo
 */

#pragma once

#include "../ui/PanelBase.hpp"

#include <QVector>
#include <postprocess/SensorModel.hpp>

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QSpinBox;
class QGroupBox;
class QCheckBox;
class QComboBox;
class QLabel;
class QFormLayout;
QT_END_NAMESPACE

class CollapsibleGroupBox;

/**
 * @class SensorPanel
 * @brief UI panel for sensor simulation configuration
 *
 * Provides controls for optical parameters (focal length, f-number),
 * detector parameters (pixel pitch, QE, well capacity), and noise models.
 */
class SensorPanel : public PanelBase {
    Q_OBJECT

public:
    explicit SensorPanel(QWidget* parent = nullptr);
    ~SensorPanel() override = default;

    [[nodiscard]] QString panelTitle() const override;
    [[nodiscard]] QString panelId() const override { return QStringLiteral("sensor"); }
    void retranslateUi() override;

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
    QDoubleSpinBox* m_psfSigma = nullptr;

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

    // Noise group (collapsible)
    CollapsibleGroupBox* m_noiseGroup = nullptr;
    QDoubleSpinBox* m_readNoise = nullptr;
    QDoubleSpinBox* m_darkCurrent = nullptr;
    QCheckBox* m_poissonNoise = nullptr;
    QCheckBox* m_readNoiseEnable = nullptr;
    QCheckBox* m_darkCurrentEnable = nullptr;
    QCheckBox* m_fpnNoise = nullptr;

    // FPN sub-group. Collapsible and folded away by default: every value in
    // it is calibrated once and then left alone, and this is the tallest panel
    // in the application.
    CollapsibleGroupBox* m_fpnGroup = nullptr;
    QDoubleSpinBox* m_prnuSigma = nullptr;
    QDoubleSpinBox* m_dsnuSigma = nullptr;
    QCheckBox* m_nucEnable = nullptr;
    QDoubleSpinBox* m_nucEfficiency = nullptr;

    // IR Detector group
    QGroupBox* m_irGroup = nullptr;
    QDoubleSpinBox* m_detectorTemp = nullptr;

    // Current params
    /// Form captions, kept with their untranslated source so that a language
    /// change can re-apply them. QT_TR_NOOP marks the literal for lupdate; the
    /// tr() call happens in retranslateUi().
    struct Caption {
        QLabel* label;
        const char* source;
        /// Explanation shown on both the caption and the field. This is the
        /// most jargon-dense panel in the application -- PRNU, DSNU, NUC, well
        /// capacity, e-/DN -- and it was the only one with no tooltips at all.
        /// Null where the caption speaks for itself.
        QWidget* field = nullptr;
        const char* tip = nullptr;
    };
    QVector<Caption> m_captions;
    QLabel* addRow(class QFormLayout* layout, const char* source, QWidget* field,
                   const char* tip = nullptr);

    quantiloom::SensorParams m_params;
    bool m_updatingUi = false;
};
