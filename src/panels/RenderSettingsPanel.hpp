/**
 * @file RenderSettingsPanel.hpp
 * @brief Render settings panel (SPP, resolution, output)
 */

#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
class QSpinBox;
class QComboBox;
class QLabel;
class QPushButton;
class QGroupBox;
class QCheckBox;
QT_END_NAMESPACE

/**
 * @class RenderSettingsPanel
 * @brief Editor for render quality settings
 */
class RenderSettingsPanel : public QWidget {
    Q_OBJECT

public:
    explicit RenderSettingsPanel(QWidget* parent = nullptr);

    void setSampleCount(uint32_t count);
    void setTargetSPP(uint32_t spp);
    void setResolution(uint32_t width, uint32_t height);

    // Getters for current settings. These are the render resolution, not the
    // panel's own geometry -- naming them width()/height() would shadow the
    // non-virtual QWidget methods that answer the latter.
    uint32_t renderWidth() const { return m_width; }
    uint32_t renderHeight() const { return m_height; }
    uint32_t spp() const { return m_targetSPP; }

signals:
    void sppChanged(uint32_t spp);
    /// Asks the shell to run its one image export path. The panel used to
    /// raise its own save dialog and emit a format string that nothing
    /// listened for, so choosing a file did nothing at all.
    void exportRequested();
    void resetAccumulationRequested();

private slots:
    void onSppPresetChanged(int index);
    void onCustomSppChanged(int value);
    void onResetClicked();

private:
    void setupUi();
    void updateSppFromPreset(int index);

    // Current settings
    uint32_t m_targetSPP = 4;
    uint32_t m_width = 1280;
    uint32_t m_height = 720;

    // UI elements
    QLabel* m_sampleCountLabel = nullptr;
    QComboBox* m_sppPreset = nullptr;
    QSpinBox* m_customSpp = nullptr;
    QLabel* m_resolutionLabel = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_resetBtn = nullptr;
    QCheckBox* m_progressiveCheck = nullptr;
};
