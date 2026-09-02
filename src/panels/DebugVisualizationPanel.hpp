/**
 * @file DebugVisualizationPanel.hpp
 * @brief Debug visualization mode selection and pixel inspection
 *
 * Much smaller than it was. The 45-entry combo box grouped by "-- Geometry --"
 * pseudo-entries is now a proper set of submenus under View, and the page of
 * static "how to read this output" documentation is in Help. What is left is
 * the two things that belong beside the image: which mode is active, and what
 * the pixel under the cursor actually reads.
 */

#pragma once

#include "../ui/PanelBase.hpp"

#include <core/Types.hpp>

QT_BEGIN_NAMESPACE
class QComboBox;
class QSpinBox;
class QLabel;
class QGroupBox;
QT_END_NAMESPACE

class DebugVisualizationPanel : public PanelBase {
    Q_OBJECT

public:
    explicit DebugVisualizationPanel(QWidget* parent = nullptr);

    [[nodiscard]] QString panelTitle() const override;
    [[nodiscard]] QString panelId() const override { return QStringLiteral("debug"); }
    void retranslateUi() override;

    void setDebugMode(quantiloom::DebugVisualizationMode mode);
    [[nodiscard]] quantiloom::DebugVisualizationMode debugMode() const { return m_mode; }

    /// Report the value read under the cursor, in device pixels.
    void setPixelReading(int x, int y, const QString& formatted);
    void setPixelReadFailed(int x, int y);

signals:
    void debugModeChanged(quantiloom::DebugVisualizationMode mode);
    /// The one number the selected view needs, when it needs one.
    void debugParameterChanged(quantiloom::u32 value);

private slots:
    void onModeChanged(int index);

private:
    void setupUi();
    void updateDescription(quantiloom::DebugVisualizationMode mode);

    quantiloom::DebugVisualizationMode m_mode = quantiloom::DebugVisualizationMode::None;

    QComboBox* m_modeCombo = nullptr;
    /// Which sun column "Response to Shade" draws. Shown only for that view,
    /// because it is the only one with a parameter and a spin box that does
    /// nothing beside every other mode is worse than no spin box.
    QLabel* m_parameterCaption = nullptr;
    QSpinBox* m_parameterSpin = nullptr;
    QLabel* m_description = nullptr;
    QLabel* m_categoryLabel = nullptr;

    QGroupBox* m_pixelGroup = nullptr;
    QLabel* m_pixelPosition = nullptr;
    QLabel* m_pixelValue = nullptr;
    QLabel* m_pixelHint = nullptr;
};
