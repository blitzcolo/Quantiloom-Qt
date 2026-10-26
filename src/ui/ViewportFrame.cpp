/**
 * @file ViewportFrame.cpp
 */

#include "ViewportFrame.hpp"

#include "ModeCatalog.hpp"
#include "UiStyle.hpp"

#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

ViewportFrame::ViewportFrame(QWidget* viewport, QWidget* parent)
    : QWidget(parent)
    , m_viewport(viewport)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // --- mode strip ---------------------------------------------------
    auto* strip = new QFrame(this);
    strip->setFrameShape(QFrame::NoFrame);
    auto* stripLayout = new QHBoxLayout(strip);
    stripLayout->setContentsMargins(6, 3, 6, 3);
    stripLayout->setSpacing(6);

    m_styling.attach(this);

    m_spectralChip = new QLabel(strip);
    m_styling.bind([this] { uistyle::applyChipStyle(m_spectralChip, uistyle::ChipTone::Accent); });
    stripLayout->addWidget(m_spectralChip);

    m_debugChip = new QLabel(strip);
    m_styling.bind([this] { uistyle::applyChipStyle(m_debugChip, uistyle::ChipTone::Neutral); });
    stripLayout->addWidget(m_debugChip);

    m_previewOnlyChip = new QLabel(strip);
    m_styling.bind([this] { uistyle::applyChipStyle(m_previewOnlyChip, uistyle::ChipTone::Warning); });
    m_previewOnlyChip->setVisible(false);
    stripLayout->addWidget(m_previewOnlyChip);

    stripLayout->addStretch();

    outer->addWidget(strip);

    // --- viewport / empty state ---------------------------------------
    m_stack = new QStackedWidget(this);
    buildEmptyState();
    m_stack->addWidget(m_emptyPage);
    if (m_viewport) {
        m_stack->addWidget(m_viewport);
    }
    m_stack->setCurrentWidget(m_emptyPage);
    outer->addWidget(m_stack, 1);

    retranslateUi();
}

void ViewportFrame::buildEmptyState() {
    m_emptyPage = new QWidget(this);
    auto* layout = new QVBoxLayout(m_emptyPage);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(12);

    m_emptyTitle = new QLabel(m_emptyPage);
    m_emptyTitle->setAlignment(Qt::AlignCenter);
    m_styling.bind([this] { uistyle::applyHeadingStyle(m_emptyTitle); });
    layout->addWidget(m_emptyTitle);

    m_emptyHint = new QLabel(m_emptyPage);
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_styling.bind([this] { uistyle::applyHintStyle(m_emptyHint); });
    layout->addWidget(m_emptyHint);

    m_openButton = new QPushButton(m_emptyPage);
    connect(m_openButton, &QPushButton::clicked,
            this, &ViewportFrame::openSceneRequested);
    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    buttonRow->addWidget(m_openButton);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    m_recentHeading = new QLabel(m_emptyPage);
    m_recentHeading->setAlignment(Qt::AlignCenter);
    m_styling.bind([this] { uistyle::applyHintStyle(m_recentHeading); });
    m_recentHeading->setVisible(false);
    layout->addWidget(m_recentHeading);

    m_recentLayout = new QVBoxLayout();
    m_recentLayout->setSpacing(2);
    layout->addLayout(m_recentLayout);
}

void ViewportFrame::setSpectralMode(quantiloom::SpectralMode mode) {
    m_spectralMode = mode;
    updateChips();
}

void ViewportFrame::setDebugMode(quantiloom::DebugVisualizationMode mode) {
    m_debugMode = mode;
    updateChips();
}

void ViewportFrame::updateChips() {
    m_spectralChip->setText(tr("Spectral: %1").arg(catalog::spectralModeName(m_spectralMode)));

    const bool debugActive = m_debugMode != quantiloom::DebugVisualizationMode::None;
    m_debugChip->setText(tr("Debug: %1").arg(catalog::debugModeName(m_debugMode)));
    uistyle::applyChipStyle(m_debugChip,
                            debugActive ? uistyle::ChipTone::Accent
                                        : uistyle::ChipTone::Neutral);

    const bool previewOnly = catalog::spectralModeIsPreviewOnly(m_spectralMode);
    m_previewOnlyChip->setVisible(previewOnly);
    if (previewOnly) {
        m_previewOnlyChip->setText(tr("Preview only — not quantitative"));
        m_previewOnlyChip->setToolTip(
            tr("This band renders from RGB-averaged spectral albedo. "
               "Load measured spectral materials for quantitative work."));
    }
}

void ViewportFrame::setSceneLoaded(bool loaded) {
    // Switching the stack away from the viewport hides the window container.
    // Qt used to answer that by releasing the renderer's Vulkan resources --
    // the render context destroyed, and the application did not survive it.
    // The window keeps them across being hidden now (PersistentResources),
    // but a page change to the page already showing is still not worth
    // paying for: a scene named on the command line that fails to load called
    // this with false while the guidance page was up from the start, and the
    // redundant switch is what tore the renderer down and took the window
    // with it.
    if (m_sceneLoaded == loaded) {
        return;
    }
    m_sceneLoaded = loaded;
    if (loaded && m_viewport) {
        m_stack->setCurrentWidget(m_viewport);
    } else {
        m_stack->setCurrentWidget(m_emptyPage);
    }
}

void ViewportFrame::setRecentFiles(const QStringList& files) {
    m_recentFiles = files;
    rebuildRecentButtons();
}

void ViewportFrame::rebuildRecentButtons() {
    while (QLayoutItem* item = m_recentLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    m_recentHeading->setVisible(!m_recentFiles.isEmpty());

    for (const QString& path : m_recentFiles) {
        auto* row = new QHBoxLayout();
        auto* button = new QPushButton(QFileInfo(path).fileName(), m_emptyPage);
        button->setFlat(true);
        button->setToolTip(path);
        connect(button, &QPushButton::clicked, this, [this, path]() {
            emit openRecentRequested(path);
        });
        row->addStretch();
        row->addWidget(button);
        row->addStretch();
        m_recentLayout->addLayout(row);
    }
}

void ViewportFrame::changeEvent(QEvent* event) {
    if (uistyle::isThemeChangeEvent(event)) {
        restyleUi();
    }
    QWidget::changeEvent(event);
}

void ViewportFrame::restyleUi() {
    m_styling.reapply();
    // The debug chip's tone tracks whether a debug mode is active, so the
    // bound setter can only put back the inactive one. updateChips() re-decides.
    updateChips();
}

void ViewportFrame::retranslateUi() {
    updateChips();
    m_emptyTitle->setText(tr("No scene loaded"));
    m_emptyHint->setText(tr(
        "Open a glTF, USD or TOML scene to start rendering.\n"
        "Right-drag orbits the camera, middle-drag pans, the wheel zooms; "
        "G/R/T switch transform mode once a node is selected."));
    m_openButton->setText(tr("Open Scene..."));
    m_recentHeading->setText(tr("Recent scenes"));
}
