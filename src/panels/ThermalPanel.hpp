/**
 * @file ThermalPanel.hpp
 * @brief Thermal solve control panel: parameters, time slider, status
 */

#pragma once

#include "../ui/PanelBase.hpp"
#include "../ui/TrajectoryPlotWidget.hpp"

#include <renderer/ThermalControl.hpp>

#include <optional>

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

    /// The hour is being driven by the global clock rather than by this panel.
    ///
    /// With a `[timeline]`, `thermal.time_h` stops meaning "the hour to render"
    /// and starts meaning "the hour the clock starts at" -- so the slider here
    /// would be setting something the transport overwrites on its next tick.
    /// Passing a value disables it and says what hour the clock is showing;
    /// passing nullopt gives the panel its slider back.
    void setMappedHour(std::optional<double> hour);

    /// Show one element's day. Called after a click in the viewport resolves
    /// to an element the solve carries; @p element is only for the caption.
    void setProbe(quantiloom::u32 element,
                  const quantiloom::ThermalElementTrajectory& trajectory);
    /// No element probed, or the last click did not land on one the solve
    /// carries. The reason is the caption, because "nothing here" and "that
    /// surface is not solved" send a user to different places.
    void clearProbe(const QString& reason = {});
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

    /// The what-if preview: which material parameter, and how far to move it.
    /// A step of zero means "off". Emitted on every slider move, because that
    /// is the point -- no re-solve happens, so it can follow a drag.
    void whatIfChanged(quantiloom::ThermalSensitivityParameter parameter, double step);

private slots:
    void onEnabledToggled(bool checked);
    void onTimeSliderChanged(int minutes);
    void onTimeSpinChanged(double hours);
    void onParamChanged();
    void onBrowseForcing();
    void onWhatIfChanged();

private:
    void updateWhatIfCaption();

    /// Set when the global clock owns the hour; the caption and the slider's
    /// enabled state both read it.
    std::optional<double> m_mappedHour;

private slots:

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
    /// The one measurement switch with a control. Off asks for the field one
    /// constant per triangle, which is what the shadow-edge correction has to
    /// be compared against -- and it sizes the tangent out of the solve state
    /// rather than hiding it at the shader, so the comparison is of two solves
    /// and not of one solve shown two ways.
    QCheckBox* m_sunCorrectionCheck = nullptr;
    QLabel* m_forcingCaption = nullptr;

    // Status
    QGroupBox* m_statusGroup = nullptr;
    QLabel* m_statusElementCount = nullptr;
    QLabel* m_statusTemperatures = nullptr;
    QLabel* m_statusSteps = nullptr;
    QLabel* m_statusExchange = nullptr;
    QLabel* m_statusStepper = nullptr;
    QLabel* m_statusError = nullptr;

    /// The probe: one element's temperatures and the fluxes that made them.
    /// In this panel rather than a dock of its own because it answers the
    /// question the controls above raise -- why is that surface that
    /// temperature -- and the answer reads best beside them.
    /// The what-if preview. Beside the probe because they are two halves of
    /// the same question: the probe says what happened, this says what would.
    QGroupBox* m_whatIfGroup = nullptr;
    QComboBox* m_whatIfParameter = nullptr;
    QSlider* m_whatIfSlider = nullptr;
    QLabel* m_whatIfCaption = nullptr;

    QGroupBox* m_probeGroup = nullptr;
    QLabel* m_probeCaption = nullptr;
    uiplot::TrajectoryPlotWidget* m_probePlot = nullptr;
    /// -1 when nothing is probed, which is what the caption reads off.
    int m_probeElement = -1;

    /// Fields of ThermalSolveParams the panel does not show. They still have
    /// to survive a round trip: params() builds the whole struct, so anything
    /// not carried here would be reset to its default the moment the user
    /// touched any spin box -- the air the surfaces convect with, silently
    /// back to 288 K.
    double m_airTemperatureK = 288.15;
    double m_skyTemperatureK = 268.0;
    double m_relativeHumidity = 50.0;

    /// The convection law and the three features that size the solve state.
    /// Carried for the same reason as the three above: a document can ask for
    /// a wind-driven h or a lagged shadow, and nothing on this panel decides
    /// them, so params() has to give back what it was given rather than a
    /// default. Losing one is not a cosmetic loss -- it is a different
    /// trajectory, solved silently.
    quantiloom::ThermalConvectionModel m_convectionModel =
        quantiloom::ThermalConvectionModel::Constant;
    double m_convectionWindA = 5.7;
    double m_convectionWindB = 3.8;
    double m_convectionFreeC = 1.52;
    double m_convectionReferenceHeightM = 2.0;
    double m_convectionStableDamping = 10.0;
    bool m_lateralConduction = false;
    quantiloom::u32 m_sunMemoryLags = 0;
    quantiloom::Vector<quantiloom::ThermalSensitivityParameter> m_parameterSensitivities;
    /// Where a dump would go. Not a solver input, and carried anyway: a set
    /// that loses it saves a document without the path it was given.
    quantiloom::String m_dumpElementsFile;
};
