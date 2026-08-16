/**
 * @file DisplayEnhancementPanel.cpp
 * @brief The tone operator and palette the viewport is displayed with
 */

#include "DisplayEnhancementPanel.hpp"

#include "../ui/UiStyle.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFormLayout>

using quantiloom::DisplayPalette;
using quantiloom::DisplayToneMode;

DisplayEnhancementPanel::DisplayEnhancementPanel(QWidget* parent)
    : PanelBase(parent)
{
    setObjectName(panelId());
    setupUi();
}

QString DisplayEnhancementPanel::panelTitle() const {
    return tr("Display Enhancement");
}

void DisplayEnhancementPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    m_enableCheckbox = new QCheckBox(this);
    bindText([this] { m_enableCheckbox->setText(tr("Enable display enhancement")); });
    connect(m_enableCheckbox, &QCheckBox::checkStateChanged, this,
            [this](Qt::CheckState state) {
                m_params.enabled = (state == Qt::Checked);
                m_settingsGroup->setEnabled(m_params.enabled);
                onSettingChanged();
            });
    mainLayout->addWidget(m_enableCheckbox);

    m_infoLabel = new QLabel(this);
    bindStyle([this] { uistyle::applyHintStyle(m_infoLabel); });
    bindText([this] {
        // Spelling out the scope here and in the export menu entries is the
        // whole point: users could not tell which of the two images they were
        // getting.
        m_infoLabel->setText(tr(
            "The only tone mapping the viewport has. An infrared render sits far below "
            "the displayable range and is black without it. Affects the viewport and "
            "saved screenshots; exported images keep their raw values."));
    });
    mainLayout->addWidget(m_infoLabel);

    m_settingsGroup = new QGroupBox(this);
    m_settingsGroup->setEnabled(false);
    auto* settingsLayout = new QFormLayout(m_settingsGroup);

    const auto notify = [this] { onSettingChanged(); };

    // ------------------------------------------------------------------
    // Stage one: contrast
    // ------------------------------------------------------------------
    m_toneCombo = new QComboBox();
    m_toneCombo->addItem(QString(), static_cast<int>(DisplayToneMode::Linear));
    m_toneCombo->addItem(QString(), static_cast<int>(DisplayToneMode::Equalize));
    m_toneCombo->addItem(QString(), static_cast<int>(DisplayToneMode::Clahe));
    connect(m_toneCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, notify](int index) {
                m_params.toneMode =
                    static_cast<DisplayToneMode>(m_toneCombo->itemData(index).toInt());
                updateControlAvailability();
                notify();
            });
    m_toneCaption = new QLabel(m_settingsGroup);
    settingsLayout->addRow(m_toneCaption, m_toneCombo);

    // ------------------------------------------------------------------
    // Stage two: colour
    // ------------------------------------------------------------------
    m_paletteCombo = new QComboBox();
    m_paletteCombo->addItem(QString(), static_cast<int>(DisplayPalette::Grey));
    m_paletteCombo->addItem(QString(), static_cast<int>(DisplayPalette::GreyInverted));
    m_paletteCombo->addItem(QString(), static_cast<int>(DisplayPalette::Ironbow));
    m_paletteCombo->addItem(QString(), static_cast<int>(DisplayPalette::Rainbow));
    m_paletteCombo->addItem(QString(), static_cast<int>(DisplayPalette::Viridis));
    connect(m_paletteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, notify](int index) {
                m_params.palette =
                    static_cast<DisplayPalette>(m_paletteCombo->itemData(index).toInt());
                updateControlAvailability();
                notify();
            });
    m_paletteCaption = new QLabel(m_settingsGroup);
    settingsLayout->addRow(m_paletteCaption, m_paletteCombo);

    // ------------------------------------------------------------------
    // The window both stages work in
    // ------------------------------------------------------------------
    auto* windowRow = new QHBoxLayout();
    m_percentileLowSpin = new QDoubleSpinBox();
    m_percentileLowSpin->setRange(0.0, 50.0);
    m_percentileLowSpin->setSingleStep(0.5);
    m_percentileLowSpin->setDecimals(1);
    m_percentileLowSpin->setSuffix(QStringLiteral(" %"));
    m_percentileLowSpin->setValue(1.0);
    m_percentileHighSpin = new QDoubleSpinBox();
    m_percentileHighSpin->setRange(50.0, 100.0);
    m_percentileHighSpin->setSingleStep(0.5);
    m_percentileHighSpin->setDecimals(1);
    m_percentileHighSpin->setSuffix(QStringLiteral(" %"));
    m_percentileHighSpin->setValue(99.0);
    connect(m_percentileLowSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this, notify](double v) {
                m_params.percentileLow = static_cast<float>(v);
                notify();
            });
    connect(m_percentileHighSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this, notify](double v) {
                m_params.percentileHigh = static_cast<float>(v);
                notify();
            });
    windowRow->addWidget(m_percentileLowSpin);
    windowRow->addWidget(m_percentileHighSpin);
    m_windowCaption = new QLabel(m_settingsGroup);
    settingsLayout->addRow(m_windowCaption, windowRow);

    m_clipLimitSpin = new QDoubleSpinBox();
    m_clipLimitSpin->setRange(1.0, 100.0);
    m_clipLimitSpin->setSingleStep(0.5);
    m_clipLimitSpin->setValue(2.0);
    m_clipLimitSpin->setDecimals(1);
    connect(m_clipLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this, notify](double v) {
                m_params.clipLimit = static_cast<float>(v);
                notify();
            });
    m_clipCaption = new QLabel(m_settingsGroup);
    settingsLayout->addRow(m_clipCaption, m_clipLimitSpin);

    m_tileSizeCombo = new QComboBox();
    m_tileSizeCombo->addItem(QString(), 4);
    m_tileSizeCombo->addItem(QString(), 8);
    m_tileSizeCombo->addItem(QString(), 16);
    m_tileSizeCombo->addItem(QString(), 32);
    m_tileSizeCombo->setCurrentIndex(1);  // 8x8 default
    connect(m_tileSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, notify](int index) {
                m_params.tileSize = m_tileSizeCombo->itemData(index).toInt();
                notify();
            });
    m_tileCaption = new QLabel(m_settingsGroup);
    settingsLayout->addRow(m_tileCaption, m_tileSizeCombo);

    m_channelGroup = new QGroupBox(m_settingsGroup);
    auto* modeLayout = new QVBoxLayout(m_channelGroup);

    m_luminanceOnlyRadio = new QRadioButton(m_channelGroup);
    m_luminanceOnlyRadio->setChecked(true);
    connect(m_luminanceOnlyRadio, &QRadioButton::toggled, this, [this, notify](bool checked) {
        m_params.luminanceOnly = checked;
        notify();
    });
    m_allChannelsRadio = new QRadioButton(m_channelGroup);

    auto* buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(m_luminanceOnlyRadio);
    buttonGroup->addButton(m_allChannelsRadio);

    modeLayout->addWidget(m_luminanceOnlyRadio);
    modeLayout->addWidget(m_allChannelsRadio);
    settingsLayout->addRow(m_channelGroup);

    mainLayout->addWidget(m_settingsGroup);
    mainLayout->addStretch();

    bindText([this] {
        m_settingsGroup->setTitle(tr("Display Settings"));

        m_toneCaption->setText(tr("Contrast:"));
        m_toneCombo->setItemText(0, tr("Linear stretch (recommended)"));
        m_toneCombo->setItemText(1, tr("Histogram equalization"));
        m_toneCombo->setItemText(2, tr("CLAHE (local)"));
        m_toneCombo->setToolTip(
            tr("How the render's range is mapped to the screen.\n"
               "Linear and equalization map the whole image the same way, so equal "
               "temperatures stay equally bright — what a thermogram needs.\n"
               "CLAHE maps each tile separately: more local detail, but brightness no "
               "longer tells you temperature."));

        m_paletteCaption->setText(tr("Palette:"));
        m_paletteCombo->setItemText(0, tr("Greyscale (white-hot)"));
        m_paletteCombo->setItemText(1, tr("Inverted (black-hot)"));
        m_paletteCombo->setItemText(2, tr("Ironbow"));
        m_paletteCombo->setItemText(3, tr("Rainbow (false colour)"));
        m_paletteCombo->setItemText(4, tr("Viridis (perceptually uniform)"));
        m_paletteCombo->setToolTip(
            tr("Colouring only — it changes no contrast.\n"
               "Anything but greyscale replaces the image's own colour, which is what "
               "false colour means."));

        m_windowCaption->setText(tr("Range window:"));
        m_percentileLowSpin->setToolTip(
            tr("Low percentile of the range to map from. Above zero, so a single cold "
               "pixel does not decide what the rest of the image looks like."));
        m_percentileHighSpin->setToolTip(
            tr("High percentile of the range to map to. Below 100 for the same reason."));

        m_clipCaption->setText(tr("Clip limit:"));
        m_clipLimitSpin->setToolTip(tr("How far equalization may push contrast.\n"
                                       "1.0 = no clipping (full equalization)\n"
                                       "2.0-4.0 = typical range for infrared"));

        m_tileCaption->setText(tr("Tile size:"));
        m_tileSizeCombo->setItemText(0, tr("4x4"));
        m_tileSizeCombo->setItemText(1, tr("8x8 (default)"));
        m_tileSizeCombo->setItemText(2, tr("16x16"));
        m_tileSizeCombo->setItemText(3, tr("32x32"));
        m_tileSizeCombo->setToolTip(tr("Number of contextual tiles.\n"
                                       "Smaller tiles = more local contrast.\n"
                                       "Larger tiles = more global contrast."));

        m_channelGroup->setTitle(tr("Processing mode"));
        m_luminanceOnlyRadio->setText(tr("Luminance only (recommended)"));
        m_luminanceOnlyRadio->setToolTip(tr("Map the luminance and scale the channels by "
                                            "the result,\npreserving colour information."));
        m_allChannelsRadio->setText(tr("All channels"));
        m_allChannelsRadio->setToolTip(tr("Map each RGB channel independently.\n"
                                          "May cause colour shifts."));
    });

    updateControlAvailability();
}

void DisplayEnhancementPanel::retranslateUi() {
    PanelBase::retranslateUi();
}

void DisplayEnhancementPanel::updateControlAvailability() {
    const bool equalizes = m_params.toneMode != DisplayToneMode::Linear;
    const bool tiled = m_params.toneMode == DisplayToneMode::Clahe;
    const bool grey = m_params.palette == DisplayPalette::Grey;

    m_clipLimitSpin->setEnabled(equalizes);
    m_clipCaption->setEnabled(equalizes);
    m_tileSizeCombo->setEnabled(tiled);
    m_tileCaption->setEnabled(tiled);
    // A palette takes its colour from the scalar, so there is no colour left
    // for this to preserve.
    m_channelGroup->setEnabled(grey);
}

void DisplayEnhancementPanel::setEnhancementEnabled(bool enabled) {
    m_params.enabled = enabled;
    const QSignalBlocker blocker(m_enableCheckbox);
    m_enableCheckbox->setChecked(enabled);
    m_settingsGroup->setEnabled(enabled);
}

void DisplayEnhancementPanel::requestEnhancementEnabled(bool enabled) {
    if (m_params.enabled == enabled) {
        return;
    }
    setEnhancementEnabled(enabled);
    emit enhancementChanged(m_params);
}

void DisplayEnhancementPanel::setParams(const quantiloom::DisplayEnhancementParams& params) {
    m_suppressSignals = true;
    m_params = params;

    m_enableCheckbox->setChecked(params.enabled);
    m_settingsGroup->setEnabled(params.enabled);

    // By value, never by index: the combos are refilled on a language change.
    m_toneCombo->setCurrentIndex(
        std::max(0, m_toneCombo->findData(static_cast<int>(params.toneMode))));
    m_paletteCombo->setCurrentIndex(
        std::max(0, m_paletteCombo->findData(static_cast<int>(params.palette))));
    const int tileIndex = m_tileSizeCombo->findData(params.tileSize);
    if (tileIndex >= 0) m_tileSizeCombo->setCurrentIndex(tileIndex);

    m_clipLimitSpin->setValue(static_cast<double>(params.clipLimit));
    m_percentileLowSpin->setValue(static_cast<double>(params.percentileLow));
    m_percentileHighSpin->setValue(static_cast<double>(params.percentileHigh));
    m_luminanceOnlyRadio->setChecked(params.luminanceOnly);
    m_allChannelsRadio->setChecked(!params.luminanceOnly);

    updateControlAvailability();
    m_suppressSignals = false;
}

quantiloom::DisplayEnhancementParams DisplayEnhancementPanel::params() const {
    return m_params;
}

void DisplayEnhancementPanel::onSettingChanged() {
    if (!m_suppressSignals) {
        emit enhancementChanged(m_params);
    }
}
