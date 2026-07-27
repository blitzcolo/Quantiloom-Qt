/**
 * @file DialogChrome.hpp
 * @brief Gives dialogs the same drawn caption as the main window
 *
 * The main window can draw its own title bar because MainWindow is ours to
 * modify. Dialogs are not: QMessageBox::about(), ::warning(), ::question() and
 * friends construct and exec their dialog internally, so there is no
 * constructor to add a widget to and no nativeEvent() to override. This
 * watches for them instead, from the application.
 *
 * Two things it does differently from WindowChrome's use in MainWindow, both
 * because the widget is not ours:
 *
 *  - **The title bar is an overlay, not a layout row.** It is parented to the
 *    dialog and positioned by hand, and the only thing done to the dialog's
 *    own layout is a top content margin. Inserting a row would mean reaching
 *    into a layout the dialog manages -- QMessageBox in particular computes
 *    its own size from its contents -- and the two would fight over geometry.
 *    A margin is something every layout already understands, and because this
 *    filter sees QEvent::Show before the dialog does, the dialog's own sizing
 *    runs with the margin already in place.
 *
 *  - **Native messages arrive through a QAbstractNativeEventFilter**, keyed by
 *    window handle, since there is no nativeEvent() to override on a widget
 *    someone else created.
 *
 * ## What this deliberately does not cover
 *
 * QFileDialog's static helpers open the *operating system's* file dialog on
 * Windows. That window belongs to the shell, not to this process, and nothing
 * here can reach it -- it keeps a native caption whatever theme is set. That
 * is a ceiling on how consistent this can be, not an oversight.
 *
 * Popups (menus, combo box drop-downs) and tool windows are skipped: they have
 * no caption to replace.
 */

#pragma once

#include <QAbstractNativeEventFilter>
#include <QHash>
#include <QObject>
#include <QPointer>

#include <memory>

QT_BEGIN_NAMESPACE
class QDialog;
class QWidget;
QT_END_NAMESPACE

class TitleBar;
class WindowChrome;

class DialogChrome : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    /// Start watching for dialogs. Call once, after QApplication exists and
    /// after the theme has been applied. Does nothing where WindowChrome is
    /// unsupported, so the caller needs no platform branch.
    static void install();

    bool eventFilter(QObject* watched, QEvent* event) override;
    bool nativeEventFilter(const QByteArray& type, void* message, qintptr* result) override;

private:
    DialogChrome();

    /// True for a top-level dialog that should get a drawn caption.
    [[nodiscard]] static bool shouldDecorate(const QWidget* widget);

    void decorate(QDialog* dialog);
    void layOut(QDialog* dialog);

    struct Decoration {
        QPointer<TitleBar> titleBar;
        // shared_ptr, not unique_ptr: QHash stores values by copy, and a
        // shared_ptr also captures its deleter where the type is complete, so
        // WindowChrome can stay a forward declaration here.
        std::shared_ptr<WindowChrome> chrome;
    };

    QHash<QObject*, Decoration> m_decorated;
};
