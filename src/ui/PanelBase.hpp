/**
 * @file PanelBase.hpp
 * @brief Common base for every dockable parameter panel
 *
 * Two things every panel now owes the shell:
 *
 *  - `panelTitle()` — the panel names itself. The dock title, the View menu
 *    entry and the workspace presets all read that one definition, so a panel
 *    whose contents are translated can no longer end up under an English tab
 *    title (the Sensor/Display/Debug inversion the refactor report calls out).
 *  - re-applying its strings — every user-visible string must be settable
 *    again, which is what makes switching language at runtime possible. Qt
 *    delivers QEvent::LanguageChange to every widget when a translator is
 *    installed or removed; the handler below turns that into a
 *    `retranslateUi()` call.
 *
 * Rather than hoisting every caption into a member so that a hand-written
 * `retranslateUi()` can reach it, a panel wraps each piece of text in
 * `bindText()`:
 *
 * @code
 * auto* group = new QGroupBox(this);
 * bindText([this, group] { group->setTitle(tr("Quality")); });
 * @endcode
 *
 * The lambda runs immediately and again on every language change. Panels with
 * text that depends on current values (a formatted reading, a combo box refill)
 * override `retranslateUi()`, call the base, and add that work.
 *
 * A bound lambda captures widget pointers raw, which is safe because the
 * widgets it touches are children of the panel and outlive nothing.
 */

#pragma once

#include <QWidget>

#include <functional>
#include <vector>

class PanelBase : public QWidget {
    Q_OBJECT

public:
    explicit PanelBase(QWidget* parent = nullptr) : QWidget(parent) {}

    /// Display name of this panel in the current language.
    [[nodiscard]] virtual QString panelTitle() const = 0;

    /// Stable, language-independent identifier. Used as the QObject name, the
    /// key in persisted layouts and the workspace preset lists.
    [[nodiscard]] virtual QString panelId() const = 0;

    /// Re-apply every user-visible string owned by this panel.
    virtual void retranslateUi();

signals:
    /// Emitted after a language change so the shell can refresh dock titles
    /// and View menu entries that mirror panelTitle().
    void panelTitleChanged();

protected:
    /// Register a piece of user-visible text. The setter runs now and on every
    /// subsequent language change.
    void bindText(std::function<void()> setter);

    void changeEvent(QEvent* event) override;

private:
    std::vector<std::function<void()>> m_textSetters;
};
