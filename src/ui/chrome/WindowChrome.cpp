/**
 * @file WindowChrome.cpp
 */

#include "WindowChrome.hpp"

#include "TitleBar.hpp"

#include <QWidget>

#ifdef Q_OS_WIN

#include <windows.h>
#include <windowsx.h>

namespace {

/// Width of the grab strip along the top edge, in logical pixels. Only the top
/// is measured here -- the other edges keep their real non-client frame and are
/// hit-tested by Windows.
constexpr int kTopResizeBorder = 6;

/// How far into a corner still counts as a corner rather than an edge.
constexpr int kCornerFactor = 2;

/// Thickness of the frame Windows adds around a maximised window, which sits
/// off-screen and has to be subtracted or the content is clipped by the screen
/// edges. Not scaled per-monitor: it is a two-pixel question on the geometry of
/// a maximised window, and GetSystemMetricsForDpi does not exist before
/// Windows 10 1607, which the Classic theme's users may well predate.
struct MaximisedFrame {
    int x;
    int y;
};

MaximisedFrame maximisedFrame() {
    const int padded = GetSystemMetrics(SM_CXPADDEDBORDER);
    return {GetSystemMetrics(SM_CXFRAME) + padded,
            GetSystemMetrics(SM_CYFRAME) + padded};
}

}  // namespace

bool WindowChrome::isSupported() {
    return true;
}

WindowChrome::WindowChrome(QWidget* window, TitleBar* titleBar)
    : m_window(window), m_titleBar(titleBar) {}

void WindowChrome::install() {
    if (!m_window) {
        return;
    }
    // winId() realises the native window if it does not exist yet, which is
    // what makes this safe to call from the constructor.
    auto hwnd = reinterpret_cast<HWND>(m_window->winId());
    if (!hwnd) {
        return;
    }

    // Before the SetWindowPos below, and that order is the whole of it.
    // SWP_FRAMECHANGED delivers WM_NCCALCSIZE *synchronously*, so the message
    // this call exists to provoke arrives while install() is still on the
    // stack. Setting the flag afterwards meant handleNativeEvent() rejected
    // that first message and Windows kept its caption -- leaving the window
    // wearing two title bars until the next frame recalculation, which is to
    // say until the user maximised it, after which it was correct forever and
    // the bug looked like a first-run mystery.
    m_installed = true;

    // Nothing about the window *style* changes -- that is the point. This only
    // asks Windows to recompute the frame, which sends the WM_NCCALCSIZE the
    // caption removal actually happens in.
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
                 SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
}

bool WindowChrome::handleNativeEvent(void* message, qintptr* result) {
    if (!m_installed || !m_window || !m_titleBar) {
        return false;
    }
    auto* msg = static_cast<MSG*>(message);
    HWND hwnd = msg->hwnd;
    if (!hwnd) {
        return false;
    }

    switch (msg->message) {
        case WM_NCCALCSIZE: {
            if (msg->wParam != TRUE) {
                return false;
            }
            auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
            RECT& r = params->rgrc[0];

            if (IsZoomed(hwnd)) {
                // Windows sizes a maximised window past the work area by the
                // frame thickness, on the assumption that the frame is
                // off-screen. With the client area covering the frame, that
                // surplus would put content under the screen edges. Inset it
                // rather than substituting the monitor work area: Windows has
                // already accounted for the taskbar, including leaving the
                // sliver an auto-hiding one needs to be reachable.
                const MaximisedFrame f = maximisedFrame();
                r.left += f.x;
                r.right -= f.x;
                r.top += f.y;
                r.bottom -= f.y;
            } else {
                // Keep the frame on three sides -- that is the visible border
                // and the resize grip Windows hit-tests for us -- and let the
                // client area swallow the caption at the top.
                const MaximisedFrame f = maximisedFrame();
                r.left += f.x;
                r.right -= f.x;
                r.bottom -= f.y;
            }
            *result = 0;
            return true;
        }

        case WM_NCHITTEST: {
            // Ask the system first. It still owns the left, right and bottom
            // edges and both bottom corners, and it knows about touch targets
            // and per-monitor DPI. Only take over when it says "client".
            const LRESULT fromSystem = DefWindowProc(hwnd, msg->message, msg->wParam, msg->lParam);
            if (fromSystem != HTCLIENT) {
                *result = fromSystem;
                return true;
            }

            RECT wr{};
            GetWindowRect(hwnd, &wr);
            const int px = GET_X_LPARAM(msg->lParam) - wr.left;
            const int py = GET_Y_LPARAM(msg->lParam) - wr.top;

            // Win32 gives physical pixels; every Qt geometry below is logical.
            // Converting through the window's own ratio rather than through
            // global coordinates keeps this correct when the window straddles
            // two monitors with different scale factors.
            const qreal dpr = m_window->devicePixelRatioF() > 0.0
                                  ? m_window->devicePixelRatioF() : 1.0;
            const QPoint local(qRound(px / dpr), qRound(py / dpr));

            // A fixed-size window -- which most dialogs are -- has no
            // WS_THICKFRAME, and DefWindowProc reports no resize edges for it.
            // Offering one along the top would be this code inventing a
            // capability the window does not have.
            const bool resizable =
                (GetWindowLongPtr(hwnd, GWL_STYLE) & WS_THICKFRAME) != 0;
            if (!IsZoomed(hwnd) && resizable) {
                // The top edge and its corners: the one strip the caption
                // removal moved out of the system's reach.
                if (local.y() < kTopResizeBorder) {
                    const int corner = kTopResizeBorder * kCornerFactor;
                    if (local.x() < corner) {
                        *result = HTTOPLEFT;
                    } else if (local.x() >= m_window->width() - corner) {
                        *result = HTTOPRIGHT;
                    } else {
                        *result = HTTOP;
                    }
                    return true;
                }
            }

            // The caption itself. isCaptionAt() excludes the buttons, which
            // must stay HTCLIENT or they would never receive a click -- a
            // control under HTCAPTION is something Windows drags the window by.
            const QPoint inTitleBar = m_titleBar->mapFrom(m_window, local);
            if (m_titleBar->isVisible() && m_titleBar->rect().contains(inTitleBar) &&
                m_titleBar->isCaptionAt(inTitleBar)) {
                *result = HTCAPTION;
                return true;
            }

            *result = HTCLIENT;
            return true;
        }

        case WM_NCACTIVATE: {
            // Without this, deactivating the window makes the default handler
            // repaint a caption that no longer exists, which flashes the old
            // frame across the top of the window.
            *result = DefWindowProc(hwnd, msg->message, msg->wParam, -1);
            return true;
        }

        default:
            return false;
    }
}

#else  // !Q_OS_WIN

bool WindowChrome::isSupported() {
    return false;
}

WindowChrome::WindowChrome(QWidget* window, TitleBar* titleBar)
    : m_window(window), m_titleBar(titleBar) {}

void WindowChrome::install() {}

bool WindowChrome::handleNativeEvent(void*, qintptr*) {
    return false;
}

#endif
