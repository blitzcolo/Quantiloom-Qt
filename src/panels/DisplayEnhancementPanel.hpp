/**
 * @file DisplayEnhancementPanel.hpp
 * @brief The tone operator and palette the viewport is displayed with
 */

#pragma once

#include "../ui/PanelBase.hpp"

#include <renderer/DisplayControl.hpp>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QLabel;
class QDoubleSpinBox;
class QComboBox;
class QGroupBox;
class QRadioButton;
QT_END_NAMESPACE

/**
 * @class DisplayEnhancementPanel
 * @brief Controls for the display path: how the render reaches the screen
 *
 * Not a beautifier. This is the only tone mapping the viewport has, so an
 * infrared render -- whose radiance sits two orders of magnitude below 1.0 --
 * is black until something here maps it into range.
 *
 * Two independent choices, and the panel is laid out to say so: a tone
 * operator that decides the contrast, and a palette that decides the colour.
 * Affects the viewport and saved screenshots; exported images keep their raw
 * values.
 */
class DisplayEnhancementPanel : public PanelBase {
    Q_OBJECT

public:
    explicit DisplayEnhancementPanel(QWidget* parent = nullptr);

    [[nodiscard]] QString panelTitle() const override;
    [[nodiscard]] QString panelId() const override { return QStringLiteral("display"); }
    void retranslateUi() override;

    /// Not named setEnabled: that is a non-virtual QWidget method meaning "make
    /// this widget interactive", and a QWidget* handle would reach it instead.
    void setEnhancementEnabled(bool enabled);

    /// Like setEnhancementEnabled, but reports the change. Used by the View
    /// menu toggle, which has to reach the renderer as a panel click would.
    void requestEnhancementEnabled(bool enabled);

    void setParams(const quantiloom::DisplayEnhancementParams& params);
    [[nodiscard]] quantiloom::DisplayEnhancementParams params() const;

signals:
    void enhancementChanged(const quantiloom::DisplayEnhancementParams& params);

private slots:
    void onSettingChanged();

private:
    void setupUi();
    /// Grey out what the current tone operator and palette do not read, rather
    /// than hiding it: a control that vanishes takes the layout with it, and
    /// the user then cannot see that the setting still exists.
    void updateControlAvailability();

    bool m_suppressSignals = false;
    quantiloom::DisplayEnhancementParams m_params;

    QCheckBox* m_enableCheckbox = nullptr;
    QLabel* m_infoLabel = nullptr;
    QGroupBox* m_settingsGroup = nullptr;

    QComboBox* m_toneCombo = nullptr;
    QComboBox* m_paletteCombo = nullptr;
    QDoubleSpinBox* m_clipLimitSpin = nullptr;
    QComboBox* m_tileSizeCombo = nullptr;
    QDoubleSpinBox* m_percentileLowSpin = nullptr;
    QDoubleSpinBox* m_percentileHighSpin = nullptr;
    QRadioButton* m_luminanceOnlyRadio = nullptr;
    QRadioButton* m_allChannelsRadio = nullptr;

    QLabel* m_toneCaption = nullptr;
    QLabel* m_paletteCaption = nullptr;
    QLabel* m_clipCaption = nullptr;
    QLabel* m_tileCaption = nullptr;
    QLabel* m_windowCaption = nullptr;
    QGroupBox* m_channelGroup = nullptr;
};
