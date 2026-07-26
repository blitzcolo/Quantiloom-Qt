/**
 * @file SpectralMaterialGenPanel.hpp
 * @brief Spectral material generator/modifier panel
 *
 * Two features:
 * 1. Auto IR Generation — uses libSpectraForge to analyze base color textures
 *    via CIELAB K-means clustering and auto-assign IR material properties.
 * 2. Manual CRI Editing — generates wavelength-dependent complex refractive
 *    index (n,k) curves from sparse anchor points via PCHIP/linear interpolation.
 */

#pragma once

#include "../ui/PanelBase.hpp"

#include <vector>

#include <core/SpectralData.hpp>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QComboBox;
class QDialog;
class QDoubleSpinBox;
class QGroupBox;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTableWidget;
class QTextEdit;
class QChartView;
QT_END_NAMESPACE

namespace quantiloom {
struct Material;
class Scene;
}

class SpectralMaterialGenPanel : public PanelBase {
    Q_OBJECT

public:
    explicit SpectralMaterialGenPanel(QWidget* parent = nullptr);

    [[nodiscard]] QString panelTitle() const override;
    [[nodiscard]] QString panelId() const override { return QStringLiteral("spectralgen"); }
    void retranslateUi() override;

    void setCurrentMaterialIndex(int index);

    /// Scene data to analyse. This panel used to hold a pointer to the render
    /// window -- the only panel that did -- and read the scene through it. It
    /// is handed the scene the same way the scene tree is now, and touches the
    /// renderer nowhere.
    void setScene(const quantiloom::Scene* scene);

signals:
    void materialChanged(int index, const quantiloom::Material& material);

    /// The material also carries fresh (n,k) data that has to be uploaded
    /// before it can be referenced. The shell performs the upload and fills in
    /// complexRefractiveIndexIndex; the panel has no route to the GPU.
    void materialWithCriChanged(int index, const quantiloom::Material& material,
                                const quantiloom::ComplexRefractiveIndex& cri);

private slots:
    // Auto IR generation
    void onAutoIRGenerate();

    // Manual CRI editing
    void onAddAnchorPoint();
    void onRemoveAnchorPoint();
    void onAnchorDataChanged();
    void onMaterialTypeChanged(int index);
    void onLoadCSV();
    void onSaveCSV();
    void onLoadYAML();
    void onApplyToMaterial();

private:
    void setupUi();
    void reinterpolate();
    void updateChart();

    static std::vector<float> pchipInterpolate(
        const std::vector<float>& xs, const std::vector<float>& ys,
        const std::vector<float>& xq);

    void applyToMaterialStruct(quantiloom::Material& mat);

    // --- Auto IR Generation widgets ---
    QRadioButton*   m_autoIRSingleRadio   = nullptr;
    QRadioButton*   m_autoIRAllRadio      = nullptr;
    QSpinBox*       m_clusterCountSpin    = nullptr;
    QDoubleSpinBox* m_temperatureSpin     = nullptr;
    QCheckBox*      m_overwriteCheck      = nullptr;
    QTextEdit*      m_autoIRResultText    = nullptr;

    // --- Manual CRI editing widgets ---
    QComboBox*      m_materialTypeCombo = nullptr;
    QDoubleSpinBox* m_roughnessSpin     = nullptr;
    QDoubleSpinBox* m_lambdaStartSpin   = nullptr;
    QDoubleSpinBox* m_lambdaEndSpin     = nullptr;
    QSpinBox*       m_outputStepsSpin   = nullptr;
    QTableWidget*   m_anchorTable       = nullptr;
    QComboBox*      m_interpCombo       = nullptr;
    QChartView*     m_chartView          = nullptr;
    QSpinBox*       m_targetMaterialSpin = nullptr;

    // Preview. There is no detach button any more: the panel is a dock, and
    // dragging it out of the window is the same gesture with none of the
    // widget-reparenting this class used to do by hand.
    QGroupBox* m_previewGroup = nullptr;

    // Interpolated data
    quantiloom::ComplexRefractiveIndex m_interpolatedCRI;
    int m_currentMaterialIndex = -1;
    const quantiloom::Scene* m_scene = nullptr;
};
