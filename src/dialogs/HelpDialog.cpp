/**
 * @file HelpDialog.cpp
 */

#include "HelpDialog.hpp"

#include "../ui/ModeCatalog.hpp"

#include <QAction>
#include <QDialogButtonBox>
#include <QEvent>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

HelpDialog::HelpDialog(QWidget* parent,
                       const QList<QAction*>& actions,
                       const QVector<RawBinding>& viewportBindings)
    : QDialog(parent)
    , m_actions(actions)
    , m_viewportBindings(viewportBindings)
{
    resize(640, 560);

    auto* layout = new QVBoxLayout(this);

    m_tabs = new QTabWidget(this);

    m_shortcutView = new QTextBrowser(m_tabs);
    m_shortcutView->setOpenExternalLinks(false);
    m_tabs->addTab(m_shortcutView, QString());

    m_debugView = new QTextBrowser(m_tabs);
    m_tabs->addTab(m_debugView, QString());

    layout->addWidget(m_tabs);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(m_buttonBox);

    retranslateUi();
}

void HelpDialog::showPage(Page page) {
    m_tabs->setCurrentIndex(page == Page::Shortcuts ? 0 : 1);
}

void HelpDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void HelpDialog::retranslateUi() {
    setWindowTitle(tr("Quantiloom Help"));
    m_tabs->setTabText(0, tr("Keyboard Shortcuts"));
    m_tabs->setTabText(1, tr("Debug Output"));
    m_shortcutView->setHtml(shortcutHtml());
    m_debugView->setHtml(catalog::debugOutputInterpretation());
}

QString HelpDialog::shortcutHtml() const {
    QString html;
    html += QStringLiteral("<h3>%1</h3>").arg(tr("Menu and toolbar"));
    html += QStringLiteral("<table cellpadding='4' width='100%'>");

    for (QAction* action : m_actions) {
        if (!action || action->shortcut().isEmpty()) {
            continue;
        }
        // Strip the mnemonic ampersands; they are markup, not text.
        QString label = action->text();
        label.remove(QLatin1Char('&'));
        html += QStringLiteral("<tr><td width='38%'><b>%1</b></td><td>%2</td></tr>")
                    .arg(action->shortcut().toString(QKeySequence::NativeText).toHtmlEscaped(),
                         label.toHtmlEscaped());
    }
    html += QStringLiteral("</table>");

    html += QStringLiteral("<h3>%1</h3>").arg(tr("Viewport"));
    html += QStringLiteral("<table cellpadding='4' width='100%'>");
    for (const RawBinding& binding : m_viewportBindings) {
        html += QStringLiteral("<tr><td width='38%'><b>%1</b></td><td>%2</td></tr>")
                    .arg(binding.keys.toHtmlEscaped(), binding.description.toHtmlEscaped());
    }
    html += QStringLiteral("</table>");

    html += QStringLiteral("<p><i>%1</i></p>")
                .arg(tr("This page is generated from the shortcuts the application "
                        "actually registers, so it cannot drift out of date."));
    return html;
}
