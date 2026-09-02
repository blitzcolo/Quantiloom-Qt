/**
 * @file DebugVisualizationPanel.cpp
 * @brief Debug visualization mode selection implementation
 */

#include "DebugVisualizationPanel.hpp"

#include "../ui/ModeCatalog.hpp"
#include "../ui/UiStyle.hpp"

#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QFormLayout>

DebugVisualizationPanel::DebugVisualizationPanel(QWidget* parent)
    : PanelBase(parent)
{
    setObjectName(panelId());
    setupUi();
}

QString DebugVisualizationPanel::panelTitle() const {
    return tr("Debug");
}

void DebugVisualizationPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // --- mode ------------------------------------------------------------
    auto* modeGroup = new QGroupBox(this);
    auto* modeLayout = new QVBoxLayout(modeGroup);

    // The list is built from the catalogue, including the category separators,
    // so this combo, the View menu and the toolbar combo always offer the same
    // modes in the same order. Category headings are real separators rather
    // than selectable pseudo-entries that had to be skipped past in code.
    m_modeCombo = new QComboBox();
    m_modeCombo->addItem(QString(), static_cast<int>(quantiloom::DebugVisualizationMode::None));
    for (catalog::DebugCategory category : catalog::debugCategories()) {
        m_modeCombo->insertSeparator(m_modeCombo->count());
        for (quantiloom::DebugVisualizationMode mode : catalog::debugModesIn(category)) {
            m_modeCombo->addItem(QString(), static_cast<int>(mode));
        }
    }
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DebugVisualizationPanel::onModeChanged);
    modeLayout->addWidget(m_modeCombo);

    m_categoryLabel = new QLabel();
    bindStyle([this] { uistyle::applyChipStyle(m_categoryLabel, uistyle::ChipTone::Accent); });
    modeLayout->addWidget(m_categoryLabel);

    m_description = new QLabel();
    bindStyle([this] { uistyle::applyHintStyle(m_description); });
    modeLayout->addWidget(m_description);

    // The sun column "Response to Shade" draws. Zero is the whole day, which
    // is what a solve carrying no shadow memory has and therefore what this
    // reads for nearly every scene; 1 and up are the individual hours a solve
    // with sun_memory_lags remembers.
    m_parameterCaption = new QLabel();
    bindStyle([this] { uistyle::applyHintStyle(m_parameterCaption); });
    m_parameterSpin = new QSpinBox();
    m_parameterSpin->setRange(0, 8);
    connect(m_parameterSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int value) {
                emit debugParameterChanged(static_cast<quantiloom::u32>(value));
            });
    modeLayout->addWidget(m_parameterCaption);
    modeLayout->addWidget(m_parameterSpin);
    m_parameterCaption->setVisible(false);
    m_parameterSpin->setVisible(false);

    bindText([this, modeGroup] {
        modeGroup->setTitle(tr("Debug Mode"));
        for (int i = 0; i < m_modeCombo->count(); ++i) {
            const QVariant itemData = m_modeCombo->itemData(i);
            if (!itemData.isValid()) continue;   // separator
            const auto mode = static_cast<quantiloom::DebugVisualizationMode>(itemData.toInt());
            m_modeCombo->setItemText(i, catalog::debugModeName(mode));
        }
    });

    mainLayout->addWidget(modeGroup);

    // --- pixel inspection -------------------------------------------------
    m_pixelGroup = new QGroupBox(this);
    auto* pixelLayout = new QFormLayout(m_pixelGroup);

    m_pixelPosition = new QLabel(QStringLiteral("--"));
    bindStyle([this] { uistyle::applyMonospaceStyle(m_pixelPosition); });
    auto* positionCaption = new QLabel(m_pixelGroup);
    pixelLayout->addRow(positionCaption, m_pixelPosition);

    m_pixelValue = new QLabel(QStringLiteral("--"));
    m_pixelValue->setWordWrap(true);
    bindStyle([this] { uistyle::applyMonospaceStyle(m_pixelValue); });
    auto* valueCaption = new QLabel(m_pixelGroup);
    pixelLayout->addRow(valueCaption, m_pixelValue);

    m_pixelHint = new QLabel(m_pixelGroup);
    bindStyle([this] { uistyle::applyHintStyle(m_pixelHint); });
    pixelLayout->addRow(m_pixelHint);

    bindText([this, positionCaption, valueCaption] {
        m_pixelGroup->setTitle(tr("Pixel Inspection"));
        positionCaption->setText(tr("Position:"));
        valueCaption->setText(tr("Value:"));
        m_pixelHint->setText(tr(
            "Hover or click the viewport with a debug mode active. "
            "Coordinates are device pixels, matching the render target.\n"
            "Help ▸ Reading Debug Output explains the colour encodings."));
    });

    mainLayout->addWidget(m_pixelGroup);
    mainLayout->addStretch();

    updateDescription(quantiloom::DebugVisualizationMode::None);
}

void DebugVisualizationPanel::retranslateUi() {
    PanelBase::retranslateUi();
    m_parameterCaption->setText(tr("Sun column: 0 is the whole day, higher numbers "
                                   "are the individual hours a solve with a shadow "
                                   "memory remembers."));
    updateDescription(m_mode);
}

void DebugVisualizationPanel::setDebugMode(quantiloom::DebugVisualizationMode mode) {
    m_mode = mode;

    const bool wantsParameter = mode == quantiloom::DebugVisualizationMode::SunSensitivity;
    m_parameterCaption->setVisible(wantsParameter);
    m_parameterSpin->setVisible(wantsParameter);

    const int index = m_modeCombo->findData(static_cast<int>(mode));
    if (index >= 0) {
        const QSignalBlocker blocker(m_modeCombo);
        m_modeCombo->setCurrentIndex(index);
    }

    updateDescription(mode);
}

void DebugVisualizationPanel::onModeChanged(int index) {
    const QVariant itemData = m_modeCombo->itemData(index);
    if (!itemData.isValid()) {
        return;   // separator; not selectable through the popup anyway
    }

    m_mode = static_cast<quantiloom::DebugVisualizationMode>(itemData.toInt());
    updateDescription(m_mode);
    emit debugModeChanged(m_mode);
}

void DebugVisualizationPanel::updateDescription(quantiloom::DebugVisualizationMode mode) {
    QString category = tr("Normal");
    for (catalog::DebugCategory candidate : catalog::debugCategories()) {
        if (catalog::debugModesIn(candidate).contains(mode)) {
            category = catalog::debugCategoryName(candidate);
            break;
        }
    }

    m_categoryLabel->setText(category);
    m_description->setText(catalog::debugModeDescription(mode));
}

void DebugVisualizationPanel::setPixelReading(int x, int y, const QString& formatted) {
    m_pixelPosition->setText(tr("%1, %2 px").arg(x).arg(y));
    m_pixelValue->setText(formatted);
}

void DebugVisualizationPanel::setPixelReadFailed(int x, int y) {
    m_pixelPosition->setText(tr("%1, %2 px").arg(x).arg(y));
    m_pixelValue->setText(tr("read failed"));
}
