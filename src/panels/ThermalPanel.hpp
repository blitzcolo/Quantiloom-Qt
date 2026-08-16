/**
 * @file ThermalPanel.hpp
 * @brief Thermal solve control panel: parameters, time slider, status
 */

#pragma once

#include "../ui/PanelBase.hpp"

#include <renderer/ThermalControl.hpp>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
QT_END_NAMESPACE

class ThermalPanel : public PanelBase {
    Q_OBJECT

public:
    explicit ThermalPanel(QWidget* parent = nullptr);

    [[nodiscard]] QString panelTitle() const override;
    [[nodiscard]] QString panelId() const override { return QStringLiteral("thermal"); }
    void retranslateUi() override;

    void setSolveEnabled(bool enabled);
    void setParams(const quantiloom::ThermalSolveParams& params);
    void setTime(double time_h);
    void updateStatus(const quantiloom::ThermalSolveStatus& status);

    [[nodiscard]] bool isSolveEnabled() const;
    [[nodiscard]] quantiloom::ThermalSolveParams params() const;
    [[nodiscard]] double time() const;

signals:
    void thermalEnabledChanged(bool enabled);
    void thermalParamsChanged(const quantiloom::ThermalSolveParams& params);
    void thermalTimeChanged(double time_h);
    void editGestureStarted();
    void editGestureFinished();

private slots:
    void onEnabledToggled(bool checked);
    void onTimeSliderChanged(int minutes);
    void onTimeSpinChanged(double hours);
    void onParamChanged();
    void onBrowseForcing();

private:
    void setupUi();
    void emitParams();

    bool m_suppressSignals = false;

    // Enable
    QCheckBox* m_enableCheck = nullptr;

    // Time of day
    QGroupBox* m_timeGroup = nullptr;
    QSlider* m_timeSlider = nullptr;
    QDoubleSpinBox* m_timeSpin = nullptr;
    QLabel* m_timeCaption = nullptr;

    // Global parameters
    QGroupBox* m_paramsGroup = nullptr;
    QDoubleSpinBox* m_startTimeSpin = nullptr;
    QDoubleSpinBox* m_timestepSpin = nullptr;
    QSpinBox* m_layersSpin = nullptr;
    QComboBox* m_initialCombo = nullptr;
    QDoubleSpinBox* m_initialTempSpin = nullptr;
    QDoubleSpinBox* m_sunIrradianceSpin = nullptr;
    QDoubleSpinBox* m_diffuseIrradianceSpin = nullptr;
    QSpinBox* m_exchangeRaysSpin = nullptr;
    QSpinBox* m_exchangeTopKSpin = nullptr;
    QDoubleSpinBox* m_checkpointStrideSpin = nullptr;
    QLineEdit* m_forcingFileEdit = nullptr;
    QPushButton* m_forcingBrowse = nullptr;
    QLabel* m_startTimeCaption = nullptr;
    QLabel* m_timestepCaption = nullptr;
    QLabel* m_layersCaption = nullptr;
    QLabel* m_initialCaption = nullptr;
    QLabel* m_initialTempCaption = nullptr;
    QLabel* m_sunIrradianceCaption = nullptr;
    QLabel* m_diffuseIrradianceCaption = nullptr;
    QLabel* m_exchangeRaysCaption = nullptr;
    QLabel* m_exchangeTopKCaption = nullptr;
    QLabel* m_checkpointStrideCaption = nullptr;
    QLabel* m_forcingCaption = nullptr;

    // Status
    QGroupBox* m_statusGroup = nullptr;
    QLabel* m_statusElementCount = nullptr;
    QLabel* m_statusTemperatures = nullptr;
    QLabel* m_statusSteps = nullptr;
    QLabel* m_statusExchange = nullptr;
    QLabel* m_statusStepper = nullptr;
    QLabel* m_statusError = nullptr;

    /// Fields of ThermalSolveParams the panel does not show. They still have
    /// to survive a round trip: params() builds the whole struct, so anything
    /// not carried here would be reset to its default the moment the user
    /// touched any spin box -- the air the surfaces convect with, silently
    /// back to 288 K.
    double m_airTemperatureK = 288.15;
    double m_skyTemperatureK = 268.0;
    double m_relativeHumidity = 50.0;
};
