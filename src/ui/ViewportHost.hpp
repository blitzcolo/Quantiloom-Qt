/**
 * @file ViewportHost.hpp
 * @brief The viewport's window container with a scrollbar on each axis
 *
 * A render shown 1:1 can be larger than the view. The viewport is a QWindow,
 * so the scrolling is the renderer's, not a QScrollArea's: this widget only
 * holds the container beside two QScrollBars and keeps their ranges in step
 * with what the renderer reports. Ranges are device pixels, the unit the
 * renderer pans in, so no devicePixelRatio conversion happens anywhere.
 * A bar shows only while that axis overflows, and the wheel is not touched:
 * it stays the camera's zoom.
 */

#pragma once

#include <QPoint>
#include <QWidget>

#include "../viewport/ResolutionPolicy.hpp"

QT_BEGIN_NAMESPACE
class QScrollBar;
QT_END_NAMESPACE

class ViewportHost : public QWidget {
    Q_OBJECT

public:
    /// Takes @p container (the viewport's window container) as its child.
    explicit ViewportHost(QWidget* container, QWidget* parent = nullptr);

    /// Ranges, page sizes, positions and visibility from what the renderer
    /// shows. Never emits scrollChanged(): the renderer is the source.
    void setPresentation(const viewport::Presentation& presentation, bool fitToView);

signals:
    /// A bar was dragged or stepped; device pixels from the render's top left.
    void scrollChanged(const QPoint& offset);

private:
    void syncBar(QScrollBar* bar, int overflow, int page, int value);

    QScrollBar* m_horizontal = nullptr;
    QScrollBar* m_vertical = nullptr;
};
