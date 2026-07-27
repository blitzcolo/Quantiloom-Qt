/**
 * @file WindowChrome.hpp
 * @brief Stops Windows drawing the caption, without giving up what it does
 *
 * The tempting way to get a custom title bar is Qt::FramelessWindowHint. It is
 * also the wrong way on Windows: a frameless window loses the drop shadow, the
 * open/minimise animations, Aero Snap, and the snap-assist layouts, and every
 * one of those then has to be faked.
 *
 * This keeps the window entirely ordinary — WS_OVERLAPPEDWINDOW, thick frame
 * and all — and changes exactly two answers it gives Windows:
 *
 *  - **WM_NCCALCSIZE** returns a client rectangle that extends up over the
 *    caption. The caption stops being drawn because there is no longer any
 *    non-client area there to draw it in. The other three edges keep their
 *    frame, so the border and shadow survive.
 *  - **WM_NCHITTEST** answers HTCAPTION over the application's own title bar.
 *    That single value is what buys back dragging, double-click to maximise,
 *    Aero Snap, snapping to screen edges, Win+arrow and the Alt+Space system
 *    menu — Windows provides all of them for anything it believes is a caption,
 *    and it is far better at the multi-monitor and per-monitor-DPI corners of
 *    that than a hand-written mouse handler will be.
 *
 * The hit test defers to DefWindowProc first and only overrides an HTCLIENT
 * answer, so six of the eight resize directions are still the system's. Only
 * the top edge and its two corners are computed here, because that is the one
 * strip the caption removal moved into the client area.
 *
 * Everything is a no-op off Windows: the window keeps its native decorations
 * and TitleBar is never shown.
 */

#pragma once

#include <QPointer>
#include <QtGlobal>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

class TitleBar;

class WindowChrome {
public:
    /// True on platforms where this is implemented. Callers use it to decide
    /// whether to show a TitleBar at all, so the fallback is one branch rather
    /// than a second code path.
    [[nodiscard]] static bool isSupported();

    WindowChrome(QWidget* window, TitleBar* titleBar);

    /// Take over the non-client area. Safe to call once the window has a
    /// native handle; harmless to call again.
    void install();

    /// Feed it QWidget::nativeEvent's arguments. Returns true when it answered
    /// the message, in which case @p result is the answer.
    bool handleNativeEvent(void* message, qintptr* result);

private:
    // Guarded, not raw: a dialog outlives neither its own close nor this
    // object's place in DialogChrome's map by any guarantee worth relying on,
    // and a native message arriving in between would dereference it.
    QPointer<QWidget> m_window;
    QPointer<TitleBar> m_titleBar;
    bool m_installed = false;
};
