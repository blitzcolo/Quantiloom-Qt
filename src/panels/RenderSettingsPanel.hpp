/**
 * @file RenderSettingsPanel.hpp
 * @brief Render settings panel (samples, resolution, display, output)
 *
 * Also the home of the display-enhancement controls, which used to occupy a
 * whole top-level tab for one checkbox and three parameters. They are hosted
 * rather than reimplemented: the shell hands the existing panel over with
 * setDisplayEnhancementWidget(), so its signals stay wired exactly as before.
 */

#pragma once

#include "../ui/PanelBase.hpp"
#include "../viewport/ResolutionPolicy.hpp"

#include <optional>

#include <scene/Scene.hpp>

QT_BEGIN_NAMESPACE
class QSpinBox;
class QComboBox;
class QLabel;
class QPushButton;
class QGroupBox;
class QCheckBox;
class QVBoxLayout;
QT_END_NAMESPACE

/**
 * @class RenderSettingsPanel
 * @brief Editor for render quality settings
 */
class RenderSettingsPanel : public PanelBase {
    Q_OBJECT

public:
    explicit RenderSettingsPanel(QWidget* parent = nullptr);

    [[nodiscard]] QString panelTitle() const override;
    [[nodiscard]] QString panelId() const override { return QStringLiteral("render"); }
    void retranslateUi() override;

    void setSampleCount(uint32_t count);
    void setTargetSPP(uint32_t spp);

    /// Show a resolution policy without emitting resolutionRequested; the
    /// shell's dispatcher calls this. Which controls are enabled follows the
    /// mode: preset and size in Fixed, the scale in Follow.
    void setResolutionPolicy(const viewport::ResolutionPolicy& policy);
    /// Show the fit switch without emitting fitToViewToggled.
    void setFitToView(bool fit);
    /// The read-only line: the extent the renderer is tracing at and the zoom
    /// the viewport shows it at (1.0 is one render pixel per screen pixel). An
    /// empty extent, before the first frame, shows a dash.
    void setRenderExtent(const QSize& extent, double zoom);

    /// Host the display-enhancement controls inside this panel.
    void setDisplayEnhancementWidget(QWidget* widget);

    [[nodiscard]] uint32_t spp() const { return m_targetSPP; }

    /// Whether reaching the target should write an EXR without being asked.
    /// Session state, like the debug mode: it says what to do with a render,
    /// not what the scene is (src/config/CLAUDE.md).
    [[nodiscard]] bool autoExportOnComplete() const;
    void setAutoExportOnComplete(bool enabled);

    /// [scene] vertex_colors as the combo holds it: "auto", "on" or "off".
    [[nodiscard]] QString vertexColorMode() const;
    /// Show a mode without emitting vertexColorModeRequested; the shell's
    /// dispatcher calls this.
    void setVertexColorMode(const QString& mode);
    /// What the loaded scene offers, as the SDK resolved it; nullptr when no
    /// scene is loaded. Decides whether the combo is enabled and what the
    /// line under it says.
    void setVertexColorState(const quantiloom::SceneVertexColors* state);
    /// The one rule for whether the mode can be chosen: a Minecraft export
    /// that carries COLOR_0. The Render menu and ql_set_vertex_colors ask
    /// here, so they cannot offer what this combo refuses.
    [[nodiscard]] bool vertexColorsSelectable() const;

signals:
    void sppChanged(uint32_t spp);
    /// A resolution control was changed by hand. The shell applies it through
    /// its one dispatcher, which the Render menu and ql_set_resolution share.
    void resolutionRequested(const viewport::ResolutionPolicy& policy);
    void fitToViewToggled(bool fit);
    /// Asks the shell to run its one image export path. The panel used to
    /// raise its own save dialog and emit a format string that nothing
    /// listened for, so choosing a file did nothing at all.
    void exportRequested();
    void resetAccumulationRequested();
    /// The auto-export box was toggled by hand; Render ▸ mirrors it.
    void autoExportToggled(bool enabled);
    /// The vertex-colour combo was changed by hand. The shell records it and
    /// applies it through its dispatcher, which re-applies the document.
    void vertexColorModeRequested(const QString& mode);

private slots:
    void onSppPresetChanged(int index);
    void onCustomSppChanged(int value);
    void onResetClicked();
    void onPresetChosen(int index);
    void onSizeEdited();
    void emitResolutionRequest();

private:
    void setupUi();
    /// Recomputes the combo's enabled state and the line under it from
    /// m_vertexColorState; also run on a language change.
    void updateVertexColorHint();
    /// Selects the preset matching the size in the two boxes, else Custom.
    void selectPresetFor(const QSize& size);
    void updateExtentLabel();

    // Current settings
    uint32_t m_targetSPP = 4;
    uint32_t m_sampleCount = 0;
    QSize m_extent;
    double m_zoom = 1.0;

    // UI elements
    QLabel* m_sampleCountLabel = nullptr;
    QComboBox* m_sppPreset = nullptr;
    QSpinBox* m_customSpp = nullptr;
    QComboBox* m_modeCombo = nullptr;
    QComboBox* m_presetCombo = nullptr;
    QSpinBox* m_widthSpin = nullptr;
    QSpinBox* m_heightSpin = nullptr;
    QComboBox* m_scaleCombo = nullptr;
    QCheckBox* m_fitCheck = nullptr;
    QLabel* m_resolutionLabel = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_resetBtn = nullptr;
    QCheckBox* m_autoExportCheck = nullptr;
    QGroupBox* m_displayGroup = nullptr;
    QVBoxLayout* m_displayLayout = nullptr;
    QComboBox* m_vertexColorCombo = nullptr;
    QLabel* m_vertexColorHint = nullptr;
    std::optional<quantiloom::SceneVertexColors> m_vertexColorState;
};
