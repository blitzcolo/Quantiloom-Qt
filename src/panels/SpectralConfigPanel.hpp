/**
 * @file SpectralConfigPanel.hpp
 * @brief Spectral rendering configuration panel
 */

#pragma once

#include "../ui/PanelBase.hpp"

#include <core/Types.hpp>

QT_BEGIN_NAMESPACE
class QComboBox;
class QDoubleSpinBox;
class QSlider;
class QLabel;
class QGroupBox;
class QStackedWidget;
QT_END_NAMESPACE

/**
 * @class SpectralConfigPanel
 * @brief Editor for spectral rendering mode and wavelength settings
 */
class SpectralConfigPanel : public PanelBase {
    Q_OBJECT

public:
    explicit SpectralConfigPanel(QWidget* parent = nullptr);

    [[nodiscard]] QString panelTitle() const override;
    [[nodiscard]] QString panelId() const override { return QStringLiteral("spectral"); }
    void retranslateUi() override;

    void setSpectralMode(quantiloom::SpectralMode mode);
    void setWavelength(float wavelength_nm);
    void setWavelengthRange(float min_nm, float max_nm, float delta_nm);

    // Read-back, so that exporting a configuration writes what the panel
    // shows. Without these the spectral section of an exported file carried
    // whatever had been loaded, never what the user had since selected.
    [[nodiscard]] quantiloom::SpectralMode spectralMode() const { return m_mode; }
    [[nodiscard]] float wavelength() const { return m_wavelength; }
    [[nodiscard]] float lambdaMin() const { return m_lambdaMin; }
    [[nodiscard]] float lambdaMax() const { return m_lambdaMax; }
    [[nodiscard]] float deltaLambda() const { return m_deltaLambda; }

signals:
    void spectralModeChanged(quantiloom::SpectralMode mode);
    void wavelengthChanged(float wavelength_nm);
    void wavelengthRangeChanged(float min_nm, float max_nm, float delta_nm);

private slots:
    void onModeChanged(int index);
    void onWavelengthSliderChanged(int value);
    void onWavelengthSpinChanged(double value);
    void onRangeChanged();

private:
    void setupUi();
    void updateModeDescription(quantiloom::SpectralMode mode);
    void updateBandCount();
    void applyModePage(quantiloom::SpectralMode mode);

    // Current settings
    quantiloom::SpectralMode m_mode = quantiloom::SpectralMode::RGB;
    float m_wavelength = 550.0f;
    float m_lambdaMin = 380.0f;
    float m_lambdaMax = 760.0f;
    float m_deltaLambda = 5.0f;

    // UI elements
    QComboBox* m_modeCombo = nullptr;
    QLabel* m_modeDescription = nullptr;
    QStackedWidget* m_settingsStack = nullptr;
    QLabel* m_rgbPageLabel = nullptr;
    QLabel* m_mwirPageLabel = nullptr;
    QLabel* m_lwirPageLabel = nullptr;

    // Single wavelength controls
    QSlider* m_wavelengthSlider = nullptr;
    QDoubleSpinBox* m_wavelengthSpin = nullptr;
    QLabel* m_wavelengthColorPreview = nullptr;

    // Range controls (for hyperspectral)
    QDoubleSpinBox* m_lambdaMinSpin = nullptr;
    QDoubleSpinBox* m_lambdaMaxSpin = nullptr;
    QDoubleSpinBox* m_deltaSpin = nullptr;
    QLabel* m_bandCountLabel = nullptr;

    // Quantitative warning label
    QLabel* m_quantitativeWarning = nullptr;
};
