/**
 * @file SpectralConfigPanel.cpp
 * @brief Spectral rendering configuration panel implementation
 */

#include "SpectralConfigPanel.hpp"

#include "../ui/ModeCatalog.hpp"
#include "../ui/UiStyle.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QStackedWidget>
#include <QFormLayout>
#include <cmath>

SpectralConfigPanel::SpectralConfigPanel(QWidget* parent)
    : PanelBase(parent)
{
    setObjectName(panelId());
    setupUi();
}

QString SpectralConfigPanel::panelTitle() const {
    return tr("Spectral");
}

void SpectralConfigPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // Mode selection group. The list, its labels and its descriptions all come
    // from the shared catalogue, so the panel, the Render menu and the toolbar
    // combo cannot offer different sets of modes.
    auto* modeGroup = new QGroupBox(this);
    auto* modeLayout = new QVBoxLayout(modeGroup);

    m_modeCombo = new QComboBox();
    for (quantiloom::SpectralMode mode : catalog::spectralModes()) {
        m_modeCombo->addItem(QString(), static_cast<int>(mode));
    }
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpectralConfigPanel::onModeChanged);
    modeLayout->addWidget(m_modeCombo);

    m_modeDescription = new QLabel();
    uistyle::applyHintStyle(m_modeDescription);
    modeLayout->addWidget(m_modeDescription);

    bindText([this, modeGroup] {
        modeGroup->setTitle(tr("Spectral Mode"));
        // Refilled by value: the current selection is found again by mode, not
        // by index, so switching language cannot change the render mode.
        for (int i = 0; i < m_modeCombo->count(); ++i) {
            const auto mode = static_cast<quantiloom::SpectralMode>(m_modeCombo->itemData(i).toInt());
            m_modeCombo->setItemText(i, catalog::spectralModeLabel(mode));
        }
    });

    mainLayout->addWidget(modeGroup);

    // Settings stack (different pages for different modes)
    m_settingsStack = new QStackedWidget();

    // Page 0: modes with no extra settings
    auto* rgbPage = new QWidget();
    auto* rgbLayout = new QVBoxLayout(rgbPage);
    m_rgbPageLabel = new QLabel(rgbPage);
    m_rgbPageLabel->setWordWrap(true);
    rgbLayout->addWidget(m_rgbPageLabel);
    rgbLayout->addStretch();
    m_settingsStack->addWidget(rgbPage);

    // Page 1: single wavelength
    auto* singlePage = new QWidget();
    auto* singleLayout = new QFormLayout(singlePage);

    auto* sliderRow = new QHBoxLayout();
    m_wavelengthSlider = new QSlider(Qt::Horizontal);
    m_wavelengthSlider->setRange(380, 760);
    m_wavelengthSlider->setValue(550);
    connect(m_wavelengthSlider, &QSlider::valueChanged,
            this, &SpectralConfigPanel::onWavelengthSliderChanged);
    sliderRow->addWidget(m_wavelengthSlider);

    m_wavelengthColorPreview = new QLabel();
    m_wavelengthColorPreview->setFixedSize(24, 24);
    sliderRow->addWidget(m_wavelengthColorPreview);
    singleLayout->addRow(sliderRow);

    m_wavelengthSpin = new QDoubleSpinBox();
    m_wavelengthSpin->setRange(380.0, 760.0);
    m_wavelengthSpin->setSingleStep(1.0);
    m_wavelengthSpin->setValue(550.0);
    connect(m_wavelengthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SpectralConfigPanel::onWavelengthSpinChanged);
    auto* wavelengthCaption = new QLabel(singlePage);
    singleLayout->addRow(wavelengthCaption, m_wavelengthSpin);

    bindText([this, wavelengthCaption] {
        wavelengthCaption->setText(tr("Wavelength:"));
        m_wavelengthSpin->setSuffix(tr(" nm"));
    });

    m_settingsStack->addWidget(singlePage);

    // Page 2: MWIR
    auto* mwirPage = new QWidget();
    auto* mwirLayout = new QVBoxLayout(mwirPage);
    m_mwirPageLabel = new QLabel(mwirPage);
    m_mwirPageLabel->setWordWrap(true);
    mwirLayout->addWidget(m_mwirPageLabel);
    mwirLayout->addStretch();
    m_settingsStack->addWidget(mwirPage);

    // Page 3: LWIR
    auto* lwirPage = new QWidget();
    auto* lwirLayout = new QVBoxLayout(lwirPage);
    m_lwirPageLabel = new QLabel(lwirPage);
    m_lwirPageLabel->setWordWrap(true);
    lwirLayout->addWidget(m_lwirPageLabel);
    lwirLayout->addStretch();
    m_settingsStack->addWidget(lwirPage);

    bindText([this] {
        m_rgbPageLabel->setText(catalog::spectralModeDescription(quantiloom::SpectralMode::RGB));
        m_mwirPageLabel->setText(
            catalog::spectralModeDescription(quantiloom::SpectralMode::MWIR_Fused));
        m_lwirPageLabel->setText(
            catalog::spectralModeDescription(quantiloom::SpectralMode::LWIR_Fused));
    });

    mainLayout->addWidget(m_settingsStack);

    // Hyperspectral range settings (always visible for reference)
    auto* rangeGroup = new QGroupBox(this);
    auto* rangeLayout = new QFormLayout(rangeGroup);

    m_lambdaMinSpin = new QDoubleSpinBox();
    m_lambdaMinSpin->setRange(300.0, 2500.0);
    m_lambdaMinSpin->setValue(380.0);
    connect(m_lambdaMinSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SpectralConfigPanel::onRangeChanged);
    auto* minCaption = new QLabel(rangeGroup);
    rangeLayout->addRow(minCaption, m_lambdaMinSpin);

    m_lambdaMaxSpin = new QDoubleSpinBox();
    m_lambdaMaxSpin->setRange(300.0, 2500.0);
    m_lambdaMaxSpin->setValue(760.0);
    connect(m_lambdaMaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SpectralConfigPanel::onRangeChanged);
    auto* maxCaption = new QLabel(rangeGroup);
    rangeLayout->addRow(maxCaption, m_lambdaMaxSpin);

    m_deltaSpin = new QDoubleSpinBox();
    m_deltaSpin->setRange(1.0, 100.0);
    m_deltaSpin->setValue(5.0);
    connect(m_deltaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SpectralConfigPanel::onRangeChanged);
    auto* deltaCaption = new QLabel(rangeGroup);
    rangeLayout->addRow(deltaCaption, m_deltaSpin);

    m_bandCountLabel = new QLabel();
    auto* bandsCaption = new QLabel(rangeGroup);
    rangeLayout->addRow(bandsCaption, m_bandCountLabel);

    bindText([this, rangeGroup, minCaption, maxCaption, deltaCaption, bandsCaption] {
        rangeGroup->setTitle(tr("Hyperspectral Range"));
        minCaption->setText(tr("Min λ:"));
        maxCaption->setText(tr("Max λ:"));
        deltaCaption->setText(tr("Δλ:"));
        bandsCaption->setText(tr("Bands:"));
        m_lambdaMinSpin->setSuffix(tr(" nm"));
        m_lambdaMaxSpin->setSuffix(tr(" nm"));
        m_deltaSpin->setSuffix(tr(" nm"));
    });

    mainLayout->addWidget(rangeGroup);

    // Quantitative warning (shown for the fused IR bands)
    m_quantitativeWarning = new QLabel();
    uistyle::applyNoticeStyle(m_quantitativeWarning);
    m_quantitativeWarning->setVisible(false);
    bindText([this] {
        m_quantitativeWarning->setText(
            tr("Preview mode: rendering from RGB-averaged spectral albedo.\n"
               "Not suitable for quantitative analysis. Load measured spectral "
               "materials for accurate infrared work."));
    });
    mainLayout->addWidget(m_quantitativeWarning);

    mainLayout->addStretch();

    // Initialize
    updateModeDescription(quantiloom::SpectralMode::RGB);
    updateBandCount();
    onWavelengthSliderChanged(550);
}

void SpectralConfigPanel::retranslateUi() {
    PanelBase::retranslateUi();
    updateModeDescription(m_mode);
    updateBandCount();
}

void SpectralConfigPanel::applyModePage(quantiloom::SpectralMode mode) {
    switch (mode) {
        case quantiloom::SpectralMode::Single:     m_settingsStack->setCurrentIndex(1); break;
        case quantiloom::SpectralMode::MWIR_Fused: m_settingsStack->setCurrentIndex(2); break;
        case quantiloom::SpectralMode::LWIR_Fused: m_settingsStack->setCurrentIndex(3); break;
        default:                                   m_settingsStack->setCurrentIndex(0); break;
    }
}

void SpectralConfigPanel::setSpectralMode(quantiloom::SpectralMode mode) {
    m_mode = mode;

    const int index = m_modeCombo->findData(static_cast<int>(mode));
    if (index >= 0) {
        const QSignalBlocker blocker(m_modeCombo);
        m_modeCombo->setCurrentIndex(index);
    }

    updateModeDescription(mode);
    applyModePage(mode);
}

void SpectralConfigPanel::setWavelength(float wavelength_nm) {
    m_wavelength = wavelength_nm;

    {
        const QSignalBlocker slider(m_wavelengthSlider);
        m_wavelengthSlider->setValue(static_cast<int>(wavelength_nm));
    }
    {
        const QSignalBlocker spin(m_wavelengthSpin);
        m_wavelengthSpin->setValue(wavelength_nm);
    }

    onWavelengthSliderChanged(static_cast<int>(wavelength_nm));
}

void SpectralConfigPanel::setWavelengthRange(float min_nm, float max_nm, float delta_nm) {
    m_lambdaMin = min_nm;
    m_lambdaMax = max_nm;
    m_deltaLambda = delta_nm;

    {
        const QSignalBlocker blocker(m_lambdaMinSpin);
        m_lambdaMinSpin->setValue(min_nm);
    }
    {
        const QSignalBlocker blocker(m_lambdaMaxSpin);
        m_lambdaMaxSpin->setValue(max_nm);
    }
    {
        const QSignalBlocker blocker(m_deltaSpin);
        m_deltaSpin->setValue(delta_nm);
    }

    updateBandCount();
}

void SpectralConfigPanel::updateBandCount() {
    if (m_deltaLambda <= 0.0f) {
        m_bandCountLabel->setText(tr("%n band(s)", "", 0));
        return;
    }
    const int bands = static_cast<int>((m_lambdaMax - m_lambdaMin) / m_deltaLambda) + 1;
    m_bandCountLabel->setText(tr("%n band(s)", "", bands));
}

void SpectralConfigPanel::onModeChanged(int index) {
    const auto mode = static_cast<quantiloom::SpectralMode>(m_modeCombo->itemData(index).toInt());
    m_mode = mode;

    updateModeDescription(mode);
    applyModePage(mode);

    emit spectralModeChanged(mode);
}

void SpectralConfigPanel::onWavelengthSliderChanged(int value) {
    m_wavelength = static_cast<float>(value);

    {
        const QSignalBlocker blocker(m_wavelengthSpin);
        m_wavelengthSpin->setValue(m_wavelength);
    }

    // Approximate visible spectrum swatch
    QColor color;
    if (value < 380) {
        color = QColor(128, 0, 128);  // UV - purple
    } else if (value < 440) {
        const float t = (value - 380.0f) / 60.0f;
        color = QColor(static_cast<int>((1.0f - t) * 128), 0, static_cast<int>(128 + t * 127));
    } else if (value < 490) {
        const float t = (value - 440.0f) / 50.0f;
        color = QColor(0, static_cast<int>(t * 255), 255);
    } else if (value < 510) {
        const float t = (value - 490.0f) / 20.0f;
        color = QColor(0, 255, static_cast<int>((1.0f - t) * 255));
    } else if (value < 580) {
        const float t = (value - 510.0f) / 70.0f;
        color = QColor(static_cast<int>(t * 255), 255, 0);
    } else if (value < 645) {
        const float t = (value - 580.0f) / 65.0f;
        color = QColor(255, static_cast<int>((1.0f - t) * 255), 0);
    } else {
        color = QColor(255, 0, 0);
    }

    m_wavelengthColorPreview->setStyleSheet(
        QStringLiteral("background-color: rgb(%1, %2, %3); border: 1px solid palette(mid);")
            .arg(color.red()).arg(color.green()).arg(color.blue()));

    emit wavelengthChanged(m_wavelength);
}

void SpectralConfigPanel::onWavelengthSpinChanged(double value) {
    m_wavelength = static_cast<float>(value);

    const QSignalBlocker blocker(m_wavelengthSlider);
    m_wavelengthSlider->setValue(static_cast<int>(value));

    emit wavelengthChanged(m_wavelength);
}

void SpectralConfigPanel::onRangeChanged() {
    m_lambdaMin = static_cast<float>(m_lambdaMinSpin->value());
    m_lambdaMax = static_cast<float>(m_lambdaMaxSpin->value());
    m_deltaLambda = static_cast<float>(m_deltaSpin->value());

    updateBandCount();

    emit wavelengthRangeChanged(m_lambdaMin, m_lambdaMax, m_deltaLambda);
}

void SpectralConfigPanel::updateModeDescription(quantiloom::SpectralMode mode) {
    m_modeDescription->setText(catalog::spectralModeDescription(mode));
    if (m_quantitativeWarning) {
        m_quantitativeWarning->setVisible(catalog::spectralModeIsPreviewOnly(mode));
    }
}
