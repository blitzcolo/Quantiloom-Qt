/**
 * @file BusyOverlay.hpp
 * @brief The card shown while the renderer works off the GUI thread
 */

#pragma once

#include <QElapsedTimer>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QWidget>

/**
 * @class BusyOverlay
 * @brief A modal card with a spinner, centred over a widget
 *
 * Shown while the SDK works on a thread of its own -- a scene opening, a
 * pipeline variant compiling, a thermal solve -- so that what the user sees
 * is the application saying it is busy, rather than Windows saying it has
 * stopped responding. Application-modal, because the renderer is out of reach
 * for the duration and nothing on the main window may be used meanwhile.
 *
 * A top-level window rather than a child: the viewport is a native window
 * (the Vulkan surface), and no sibling widget can be drawn over one.
 *
 * Paints from the palette at paint time, so it follows a theme switch without
 * a binding of its own.
 */
class BusyOverlay : public QWidget {
    Q_OBJECT

public:
    /// @param anchor the widget the card is centred over
    explicit BusyOverlay(QWidget* anchor);

    /// Show the card with @p message, or retitle the one already up.
    void start(const QString& message);
    /// The line under the title: what is being opened, or how far along it is.
    void setDetail(const QString& detail);
    void finish();

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void reposition();

    QPointer<QWidget> m_anchor;
    QString m_message;
    QString m_detail;
    QElapsedTimer m_clock;
    QTimer m_spin;
    int m_angle = 0;
};
