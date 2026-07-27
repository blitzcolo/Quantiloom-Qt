/**
 * @file DialogChrome.cpp
 */

#include "DialogChrome.hpp"

#include "TitleBar.hpp"
#include "WindowChrome.hpp"

#include <QApplication>
#include <QDialog>
#include <QPointer>
#include <QEvent>
#include <QLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

DialogChrome::DialogChrome() = default;

void DialogChrome::install() {
    if (!WindowChrome::isSupported()) {
        return;
    }
    static DialogChrome* instance = nullptr;
    if (instance) {
        return;
    }
    instance = new DialogChrome();
    instance->setParent(qApp);
    qApp->installEventFilter(instance);
    qApp->installNativeEventFilter(instance);
}

bool DialogChrome::shouldDecorate(const QWidget* widget) {
    if (!widget || !widget->isWindow()) {
        return false;
    }
    // Only real dialogs. A popup or a tool window has no caption to replace,
    // and a window already asking to be frameless has said what it wants.
    const Qt::WindowFlags flags = widget->windowFlags();
    if ((flags & Qt::WindowType_Mask) != Qt::Dialog) {
        return false;
    }
    if (flags & Qt::FramelessWindowHint) {
        return false;
    }
    // Nothing to hang a margin on.
    return widget->layout() != nullptr;
}

bool DialogChrome::eventFilter(QObject* watched, QEvent* event) {
    switch (event->type()) {
        case QEvent::Show: {
            auto* dialog = qobject_cast<QDialog*>(watched);
            if (dialog && !m_decorated.contains(dialog) && shouldDecorate(dialog)) {
                // Before QDialog's own show handling, which is what lets the
                // margin reach its size calculation rather than arriving after
                // it and forcing a second, visible relayout.
                decorate(dialog);
            }
            break;
        }
        case QEvent::Resize: {
            const auto it = m_decorated.constFind(watched);
            if (it != m_decorated.constEnd()) {
                layOut(static_cast<QDialog*>(watched));
            }
            break;
        }
        default:
            break;
    }
    return QObject::eventFilter(watched, event);
}

void DialogChrome::decorate(QDialog* dialog) {
    Decoration decoration;

    auto* bar = new TitleBar(dialog);
    // A dialog is not minimised or maximised, so those two buttons would be
    // decoration that does nothing.
    bar->setDialogMode(true);
    QObject::connect(bar, &TitleBar::closeRequested, dialog, &QDialog::reject);
    bar->show();
    bar->raise();
    decoration.titleBar = bar;

    // The only change to the dialog's own layout. Its contents move down by
    // the height of the bar and its sizeHint grows to match, which is all the
    // dialog needs to know about any of this.
    if (QLayout* layout = dialog->layout()) {
        const QMargins m = layout->contentsMargins();
        layout->setContentsMargins(m.left(), m.top() + bar->height(),
                                   m.right(), m.bottom());
    }

    decoration.chrome = std::make_shared<WindowChrome>(dialog, bar);

    // Queued, not called here. install() forces the native window into
    // existence and then asks Windows to recompute the frame, which delivers
    // WM_NCCALCSIZE synchronously -- all of it nested inside Qt's own handling
    // of the show it was triggered by. One dialog came out of that with no
    // frame recalculation at all (its caption survived, and the hit test then
    // measured from a window rect that still had one, so its title bar would
    // not drag), and the path was implicated in a crash besides. Running a turn
    // later costs at most a frame of native caption and asks nothing of Qt
    // mid-show.
    {
        auto chrome = decoration.chrome;
        QPointer<QDialog> alive(dialog);
        QMetaObject::invokeMethod(this, [chrome, alive]() {
            if (alive) {
                chrome->install();
            }
        }, Qt::QueuedConnection);
    }

    // QEvent::Destroy is not a dependable hook for this; the destroyed signal
    // is. A stale entry means a native message routed to a WindowChrome whose
    // widget is gone, which is a crash rather than a cosmetic problem.
    QObject::connect(dialog, &QObject::destroyed, this,
                     [this](QObject* gone) { m_decorated.remove(gone); });

    m_decorated.insert(dialog, decoration);
    layOut(dialog);

}

void DialogChrome::layOut(QDialog* dialog) {
    const auto it = m_decorated.constFind(dialog);
    if (it == m_decorated.constEnd() || !it->titleBar) {
        return;
    }
    it->titleBar->setGeometry(0, 0, dialog->width(), it->titleBar->height());
    it->titleBar->raise();
}

bool DialogChrome::nativeEventFilter(const QByteArray& type, void* message, qintptr* result) {
    Q_UNUSED(type);
#ifdef Q_OS_WIN
    if (m_decorated.isEmpty()) {
        return false;
    }
    auto* msg = static_cast<MSG*>(message);
    if (!msg || !msg->hwnd) {
        return false;
    }
    // There is no nativeEvent() to override on a widget this process did not
    // create, so the message is routed back to the right dialog by handle.
    QWidget* widget = QWidget::find(reinterpret_cast<WId>(msg->hwnd));
    if (!widget) {
        return false;
    }
    const auto it = m_decorated.constFind(widget);
    if (it == m_decorated.constEnd() || !it->chrome) {
        return false;
    }
    return it->chrome->handleNativeEvent(message, result);
#else
    Q_UNUSED(message);
    Q_UNUSED(result);
    return false;
#endif
}
