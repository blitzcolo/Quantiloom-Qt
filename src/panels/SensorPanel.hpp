/**
 * @file SensorPanel.hpp
 * @brief Panel for the versioned physical camera (M5-3)
 *
 * @author blitzcolo
 */

#pragma once

#include "../ui/PanelBase.hpp"

#include <QVector>
#include <optional>
#include <postprocess/CameraPipeline.hpp>
#include <postprocess/CameraPresets.hpp>
#include <postprocess/Thermography.hpp>

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
 * @brief UI panel for the versioned physical camera
 *
 * The panel's data carrier is quantiloom::camera::CameraConfig: detector
 * (photon/thermal, CFA), optics, readout (shutter/exposure/gain), per-kind
 * noise models, the closed-loop AE/AWB block, HSV grading and the infrared
 * tone/palette. A preset combo applies one of the SDK's CameraPresetKind
 * entries; everything the combo installs is a plain config the user can then
 * edit field by field.
 *
 * Two change signals mirror the SDK's invalidation tiers: structural edits
 * (detector, optics, readout, noise, AE/AWB) ask for a re-measurement, while
 * display-only edits (white balance, denoise/sharpen, HSV, IR tone/palette)
 * ask for a display reprocess that never advances the acquisition history.
 * Both are document state; the distinction is only about what the SDK re-runs.
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
     * @brief Set camera enabled state
     *
     * Not named setEnabled: that is a non-virtual QWidget method meaning
     * "make this widget interactive", and a QWidget* handle would silently
     * reach it instead of this one.
     */
    void setCameraEnabled(bool enabled);

    /**
     * @brief Check if camera is enabled
     */
    bool isCameraEnabled() const;

    /**
     * @brief Set the whole camera configuration and refresh every field
     *
     * Used on document load, undo/redo and preset application. The preset
     * combo is set to the matching hardware/generic preset when the config is
     * byte-identical to one (modulo the enabled flag), and to "Custom"
     * otherwise.
     */
    void setCameraConfig(const quantiloom::camera::CameraConfig& config);

    /**
     * @brief Current camera configuration
     *
     * Kept in sync incrementally as fields edit, so this is cheap.
     */
    [[nodiscard]] quantiloom::camera::CameraConfig getCameraConfig() const;

    /**
     * @brief What the virtual camera is told about the surface it looks at
     *
     * Not camera parameters: these do not change what is measured, only what
     * temperature the measurement is reported as. They drive the viewport's
     * temperature readout here, and the CLI's _tapp.exr when the scene is
     * rendered offline.
     */
    void setThermography(bool enabled, const quantiloom::ThermographyParams& params);
    [[nodiscard]] bool isThermographyEnabled() const;
    [[nodiscard]] quantiloom::ThermographyParams getThermographyParams() const;

signals:
    /**
     * @brief Emitted when enabled state changes
     */
    void enabledChanged(bool enabled);

    /**
     * @brief Emitted when a structural parameter changes (detector, optics,
     * readout, noise, AE/AWB). Hosts apply this through SetCameraConfig.
     */
    void cameraConfigChanged(const quantiloom::camera::CameraConfig& config);

    /**
     * @brief Emitted when a display-only parameter changes (white balance,
     * denoise/sharpen, HSV grading, IR tone/palette). Hosts apply this through
     * ReprocessCameraDisplay so the acquisition history never advances.
     */
    void cameraDisplayChanged(const quantiloom::camera::CameraConfig& config);

    /**
     * @brief Emitted when the thermography settings change
     */
    void thermographyChanged(bool enabled, const quantiloom::ThermographyParams& params);

private slots:
    void onEnabledChanged(bool enabled);
    void onParamChanged();
    void onDisplayParamChanged();
    void onPresetChanged(int index);
    void onDetectorKindChanged(int index);
    void onShutterChanged(int index);
    void onThermographyChanged();
    void onFpnToggled(bool checked);
    void onNucToggled(bool checked);
    void onAutoExposureToggled(bool checked);
    void onDenoiseToggled(bool checked);
    void onSharpenToggled(bool checked);
    void onEmpiricalNoiseToggled(bool checked);
    void onTemporalDriftToggled(bool checked);

private:
    void setupUi();
    void updateUiFromConfig(const quantiloom::camera::CameraConfig& config);
    void blockSignalsForUpdate(bool block);
    void updateKindVisibility();
    void updateStatusArea();

    /// Form captions, kept with their untranslated source so that a language
    /// change can re-apply them. QT_TR_NOOP marks the literal for lupdate; the
    /// tr() call happens in retranslateUi().
    struct Caption {
        QLabel* label;
        const char* source;
        /// Explanation shown on both the caption and the field. The camera
        /// vocabulary (PRNU, DSNU, NUC, rolling shutter, NETD) is jargon-dense
        /// enough that every row carries one.
        QWidget* field = nullptr;
        const char* tip = nullptr;
    };
    QVector<Caption> m_captions;
    QLabel* addRow(class QFormLayout* layout, const char* source, QWidget* field,
                   const char* tip = nullptr);

    QDoubleSpinBox* makeSpin(double min, double max, double step, int decimals,
                             double value, const char* suffix = nullptr);

    // Enable checkbox
    QCheckBox* m_enabledCheck = nullptr;

    // Preset row
    QComboBox* m_presetCombo = nullptr;

    // Detector group
    QGroupBox* m_deviceGroup = nullptr;
    QComboBox* m_detectorKind = nullptr;
    QComboBox* m_cfaCombo = nullptr;

    // Optics group
    QGroupBox* m_opticsGroup = nullptr;
    QDoubleSpinBox* m_focalLength = nullptr;
    QDoubleSpinBox* m_fNumber = nullptr;
    QDoubleSpinBox* m_pixelPitch = nullptr;
    QDoubleSpinBox* m_psfSigma = nullptr;

    // Readout group
    QGroupBox* m_readoutGroup = nullptr;
    QComboBox* m_shutterCombo = nullptr;
    QDoubleSpinBox* m_rowDelay = nullptr;
    QDoubleSpinBox* m_exposureTime = nullptr;
    QDoubleSpinBox* m_framePeriod = nullptr;
    QDoubleSpinBox* m_analogGain = nullptr;
    QDoubleSpinBox* m_electronsPerDn = nullptr;
    QDoubleSpinBox* m_blackLevelDn = nullptr;
    QSpinBox* m_adcBits = nullptr;

    // Photon detector group
    QGroupBox* m_photonGroup = nullptr;
    QDoubleSpinBox* m_fullWell = nullptr;
    QDoubleSpinBox* m_readNoise = nullptr;
    QDoubleSpinBox* m_darkCurrent = nullptr;
    QCheckBox* m_shotNoiseEnable = nullptr;
    QCheckBox* m_readNoiseEnable = nullptr;
    QCheckBox* m_darkCurrentEnable = nullptr;
    QCheckBox* m_fpnNoise = nullptr;
    // FPN sub-rows, enabled only while FPN is on.
    QDoubleSpinBox* m_prnuSigma = nullptr;
    QDoubleSpinBox* m_dsnuSigma = nullptr;
    QCheckBox* m_nucEnable = nullptr;
    QDoubleSpinBox* m_nucResidual = nullptr;

    // Thermal detector group
    QGroupBox* m_thermalGroup = nullptr;
    QDoubleSpinBox* m_timeConstant = nullptr;
    QDoubleSpinBox* m_responsivity = nullptr;
    QDoubleSpinBox* m_thermalReadNoise = nullptr;
    QDoubleSpinBox* m_drift = nullptr;
    QDoubleSpinBox* m_netd = nullptr;
    QDoubleSpinBox* m_netdRefTemp = nullptr;

    // ISP group
    QGroupBox* m_ispGroup = nullptr;
    QCheckBox* m_autoExposure = nullptr;
    QDoubleSpinBox* m_aeTarget = nullptr;
    QDoubleSpinBox* m_aeSmoothing = nullptr;
    QDoubleSpinBox* m_aeMinExposure = nullptr;
    QDoubleSpinBox* m_aeMaxExposure = nullptr;
    QDoubleSpinBox* m_aeMaxGain = nullptr;
    QCheckBox* m_autoWhiteBalance = nullptr;
    QDoubleSpinBox* m_wbR = nullptr;
    QDoubleSpinBox* m_wbG = nullptr;
    QDoubleSpinBox* m_wbB = nullptr;
    QDoubleSpinBox* m_toneGamma = nullptr;
    QCheckBox* m_denoise = nullptr;
    QDoubleSpinBox* m_denoiseStrength = nullptr;
    QCheckBox* m_sharpen = nullptr;
    QDoubleSpinBox* m_sharpenStrength = nullptr;

    // HSV grading group
    CollapsibleGroupBox* m_hsvGroup = nullptr;
    QDoubleSpinBox* m_hueOffset = nullptr;
    QDoubleSpinBox* m_saturationScale = nullptr;
    QDoubleSpinBox* m_valueGamma = nullptr;
    QCheckBox* m_empiricalNoise = nullptr;
    QDoubleSpinBox* m_noiseSigma = nullptr;
    QCheckBox* m_temporalDrift = nullptr;
    QDoubleSpinBox* m_driftSigma = nullptr;

    // IR display group (thermal detectors only)
    QGroupBox* m_irDisplayGroup = nullptr;
    QComboBox* m_irToneCombo = nullptr;
    QComboBox* m_irPaletteCombo = nullptr;

    // Read-only status area: effective band, calibration status, provenance.
    QGroupBox* m_statusGroup = nullptr;
    QLabel* m_effectiveBandLabel = nullptr;
    QLabel* m_calibrationLabel = nullptr;
    QLabel* m_provenanceLabel = nullptr;
    QLabel* m_previewLabel = nullptr;

    // Thermography group (collapsible): the camera's reporting model.
    CollapsibleGroupBox* m_thermographyGroup = nullptr;
    QCheckBox* m_thermographyEnable = nullptr;
    QDoubleSpinBox* m_thermoEmissivity = nullptr;
    QDoubleSpinBox* m_thermoReflectedTemp = nullptr;
    QDoubleSpinBox* m_thermoTransmittance = nullptr;
    QDoubleSpinBox* m_thermoAtmosphereTemp = nullptr;

    // Current config
    quantiloom::camera::CameraConfig m_camera;
    /// Preset the current config was built from, when it still matches one.
    std::optional<quantiloom::camera::CameraPresetKind> m_presetKind;
    quantiloom::ThermographyParams m_thermography;
    bool m_updatingUi = false;
};
