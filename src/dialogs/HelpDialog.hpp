/**
 * @file HelpDialog.hpp
 * @brief Shortcut reference and debug-output reference
 *
 * The application had no help of any kind: the key bindings were folklore
 * plus a paragraph of grey text at the bottom of the scene panel, and the
 * "how do I read this debug image" documentation occupied half the debug
 * panel.
 *
 * The shortcut page is *generated from the actions that are actually
 * registered*, not typed out a second time. That is deliberate — the panel
 * paragraph it replaces had already drifted from the bindings it described,
 * and a hand-written table would drift again.
 */

#pragma once

#include <QDialog>
#include <QList>
#include <QString>
#include <QVector>

QT_BEGIN_NAMESPACE
class QAction;
class QDialogButtonBox;
class QTabWidget;
class QTextBrowser;
QT_END_NAMESPACE

class HelpDialog : public QDialog {
    Q_OBJECT

public:
    enum class Page {
        Shortcuts,
        DebugOutput
    };

    /// One line of the shortcut table that has no QAction behind it: the
    /// viewport's own mouse and camera bindings.
    struct RawBinding {
        QString keys;
        QString description;
    };

    HelpDialog(QWidget* parent,
               const QList<QAction*>& actions,
               const QVector<RawBinding>& viewportBindings);

    void showPage(Page page);

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    [[nodiscard]] QString shortcutHtml() const;

    QTabWidget* m_tabs = nullptr;
    QTextBrowser* m_shortcutView = nullptr;
    QTextBrowser* m_debugView = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;

    QList<QAction*> m_actions;
    QVector<RawBinding> m_viewportBindings;
};
