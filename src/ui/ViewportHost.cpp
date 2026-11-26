/**
 * @file ViewportHost.cpp
 * @brief The viewport's window container with a scrollbar on each axis
 */

#include "ViewportHost.hpp"

#include <QGridLayout>
#include <QScrollBar>
#include <QSignalBlocker>

#include <algorithm>

ViewportHost::ViewportHost(QWidget* container, QWidget* parent)
    : QWidget(parent)
    , m_horizontal(new QScrollBar(Qt::Horizontal, this))
    , m_vertical(new QScrollBar(Qt::Vertical, this)) {
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);
    grid->addWidget(container, 0, 0);
    grid->addWidget(m_vertical, 0, 1);
    grid->addWidget(m_horizontal, 1, 0);

    for (QScrollBar* bar : {m_horizontal, m_vertical}) {
        bar->setVisible(false);
        // The wheel is the camera's; a bar never takes keyboard focus either,
        // or the fly keys would stop reaching the viewport after a drag.
        bar->setFocusPolicy(Qt::NoFocus);
        connect(bar, &QScrollBar::valueChanged, this, [this] {
            emit scrollChanged(QPoint(m_horizontal->value(), m_vertical->value()));
        });
    }
}

void ViewportHost::setPresentation(const viewport::Presentation& presentation, bool fitToView) {
    const int overflowX =
        fitToView ? 0 : std::max(0, presentation.content.width() - presentation.view.width());
    const int overflowY =
        fitToView ? 0 : std::max(0, presentation.content.height() - presentation.view.height());
    syncBar(m_horizontal, overflowX, presentation.view.width(), presentation.scroll.x());
    syncBar(m_vertical, overflowY, presentation.view.height(), presentation.scroll.y());
}

void ViewportHost::syncBar(QScrollBar* bar, int overflow, int page, int value) {
    {
        const QSignalBlocker blocker(bar);
        bar->setRange(0, overflow);
        bar->setPageStep(std::max(1, page));
        bar->setSingleStep(std::max(1, page / 20));
        bar->setValue(value);
    }
    bar->setVisible(overflow > 0);
}
