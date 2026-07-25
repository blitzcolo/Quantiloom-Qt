/**
 * @file DisplayEnhancementPanel.hpp
 * @brief Display enhancement controls for infrared imaging
 */

#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QDoubleSpinBox;
class QComboBox;
class QGroupBox;
class QRadioButton;
QT_END_NAMESPACE

/**
 * @class DisplayEnhancementPanel
 * @brief Controls for CLAHE display enhancement (infrared visibility)
 *
 * Provides UI controls for CLAHE post-processing to improve visibility
 * of low-dynamic-range infrared images. Enhancement only affects display
 * and screenshots, not exported EXR files.
 */
class DisplayEnhancementPanel : public QWidget {
    Q_OBJECT

public:
    explicit DisplayEnhancementPanel(QWidget* parent = nullptr);

    /// Not named setEnabled: that is a non-virtual QWidget method meaning "make
    /// this widget interactive", and a QWidget* handle would reach it instead.
    void setEnhancementEnabled(bool enabled);
    void setClipLimit(float clipLimit);
    void setTileSize(int tileSize);
    void setLuminanceOnly(bool luminanceOnly);

signals:
    void enhancementChanged(bool enabled, float clipLimit, int tileSize, bool luminanceOnly);

private slots:
    void onEnableChanged(int state);
    void onClipLimitChanged(double value);
    void onTileSizeChanged(int index);
    void onProcessingModeChanged();

private:
    void setupUi();
    void emitSettings();

    // Current settings
    bool m_enabled = false;
    float m_clipLimit = 2.0f;
    int m_tileSize = 8;
    bool m_luminanceOnly = true;

    // UI elements
    QCheckBox* m_enableCheckbox = nullptr;
    QGroupBox* m_settingsGroup = nullptr;
    QDoubleSpinBox* m_clipLimitSpin = nullptr;
    QComboBox* m_tileSizeCombo = nullptr;
    QRadioButton* m_luminanceOnlyRadio = nullptr;
    QRadioButton* m_allChannelsRadio = nullptr;
};
