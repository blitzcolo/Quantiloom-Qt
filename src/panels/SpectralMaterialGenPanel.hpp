/**
 * @file SpectralMaterialGenPanel.hpp
 * @brief Spectral material generator/modifier panel
 *
 * Generates wavelength-dependent complex refractive index (n,k) curves from
 * sparse anchor points via PCHIP/linear interpolation. Computes normal-incidence
 * reflectance R0(λ) = [(n-1)²+k²]/[(n+1)²+k²] and pushes to GPU via
 * Material::irReflectanceCurve.
 */

#pragma once

#include <QWidget>
#include <vector>

#include <core/SpectralData.hpp>

QT_BEGIN_NAMESPACE
class QComboBox;
class QDialog;
class QDoubleSpinBox;
class QGroupBox;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QChartView;
QT_END_NAMESPACE

namespace quantiloom {
struct Material;
}

class QuantiloomVulkanWindow;

class SpectralMaterialGenPanel : public QWidget {
    Q_OBJECT

public:
    explicit SpectralMaterialGenPanel(QWidget* parent = nullptr);

    void setCurrentMaterialIndex(int index);
    void setVulkanWindow(QuantiloomVulkanWindow* window);

signals:
    void materialChanged(int index, const quantiloom::Material& material);

private slots:
    void onAddAnchorPoint();
    void onRemoveAnchorPoint();
    void onAnchorDataChanged();
    void onMaterialTypeChanged(int index);
    void onLoadCSV();
    void onSaveCSV();
    void onLoadYAML();
    void onApplyToMaterial();
    void onDetachPreview();
    void onPreviewDialogClosed();

private:
    void setupUi();
    void reinterpolate();
    void updateChart();

    static std::vector<float> pchipInterpolate(
        const std::vector<float>& xs, const std::vector<float>& ys,
        const std::vector<float>& xq);

    void applyToMaterialStruct(quantiloom::Material& mat);

    // UI widgets
    QComboBox*      m_materialTypeCombo = nullptr;
    QDoubleSpinBox* m_roughnessSpin     = nullptr;
    QDoubleSpinBox* m_lambdaStartSpin   = nullptr;
    QDoubleSpinBox* m_lambdaEndSpin     = nullptr;
    QSpinBox*       m_outputStepsSpin   = nullptr;
    QTableWidget*   m_anchorTable       = nullptr;
    QComboBox*      m_interpCombo       = nullptr;
    QChartView*     m_chartView          = nullptr;
    QSpinBox*       m_targetMaterialSpin = nullptr;

    // Preview pop-out
    QGroupBox*   m_previewGroup  = nullptr;
    QPushButton* m_detachBtn     = nullptr;
    QDialog*     m_previewDialog = nullptr;

    // Interpolated data
    quantiloom::ComplexRefractiveIndex m_interpolatedCRI;
    int m_currentMaterialIndex = -1;

    // Vulkan window for CRI upload
    QuantiloomVulkanWindow* m_vulkanWindow = nullptr;
};
