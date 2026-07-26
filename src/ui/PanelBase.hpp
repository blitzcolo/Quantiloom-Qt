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
 *  - `retranslateUi()` — every user-visible string is (re)applied here rather
 *    than in the constructor, which is what makes switching language at
 *    runtime possible. Qt delivers QEvent::LanguageChange to every widget when
 *    a translator is installed or removed; the handler below turns that into a
 *    `retranslateUi()` call.
 *
 * Construction order is therefore always: build the widget tree in
 * `setupUi()`, then call `retranslateUi()` once to fill in the text.
 */

#pragma once

#include <QWidget>

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
    virtual void retranslateUi() = 0;

signals:
    /// Emitted after a language change so the shell can refresh dock titles
    /// and View menu entries that mirror panelTitle().
    void panelTitleChanged();

protected:
    void changeEvent(QEvent* event) override;
};
