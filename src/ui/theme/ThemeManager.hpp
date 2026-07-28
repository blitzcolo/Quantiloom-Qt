/**
 * @file ThemeManager.hpp
 * @brief Runtime theme switching
 *
 * The twin of LanguageManager, and deliberately shaped like it: one singleton,
 * a stored preference in QSettings, a `switchTo()` that takes effect without a
 * restart, and one requirement placed on the rest of the code.
 *
 * For language that requirement is "no user-visible string may be set once".
 * The colour equivalent is the same sentence with a different noun: **no
 * colour, font size or style sheet may be set once at construction time**.
 * Setting the application palette makes Qt post QEvent::PaletteChange, and
 * setting the style posts QEvent::StyleChange, to every widget; a widget that
 * baked a colour into a style sheet in its constructor keeps the old one and
 * ends up as the one unreadable label on the panel.
 *
 * Panels answer this through PanelBase::bindStyle(), which mirrors bindText():
 *
 * @code
 * uistyle::applyHintStyle(hint);                            // before
 * bindStyle([hint] { uistyle::applyHintStyle(hint); });     // after
 * @endcode
 *
 * Widgets that are not panels -- MainWindow, ViewportFrame, PreferencesDialog
 * -- own a uistyle::StyleBindings and drive it from their own changeEvent, the
 * same way they each own their retranslateUi().
 */

#pragma once

#include "Theme.hpp"

#include <QObject>
#include <QString>
#include <QVector>

class ThemeManager : public QObject {
    Q_OBJECT

public:
    /// Identifiers of the themes this build ships. Stable strings: they are
    /// written into QSettings and must not change to stay a preference.
    static const QString kBlenderDark;
    static const QString kClassic;
    static const QString kWindows11;
    static const QString kWindowsXp;
    static const QString kWindows7;
    static const QString kNeutralGrey;
    static const QString kHighContrast;
    static const QString kSolarizedLight;
    static const QString kPhosphor;
    static const QString kPrintFriendly;

    static ThemeManager& instance();

    [[nodiscard]] static QVector<theming::Theme> availableThemes();

    /// Name to show in the menu and in Preferences, in the current language.
    [[nodiscard]] static QString displayName(const QString& id);

    /// Apply the stored preference, or -- when there is none -- the theme that
    /// suits this machine. Call once, before the main window is constructed,
    /// so the first paint is already themed.
    void applyStoredPreference();

    /// Switch now and remember the choice. Returns false for an unknown id, in
    /// which case nothing changed.
    bool switchTo(const QString& id);

    [[nodiscard]] QString currentThemeId() const { return m_currentId; }

    /// The theme in effect. UiStyle reads its accent colours from here.
    [[nodiscard]] const theming::Theme& currentTheme() const { return m_current; }

signals:
    /// Emitted after the palette and style are in place. Widgets that cannot
    /// see QEvent::PaletteChange -- anything drawing outside the widget tree --
    /// can hang off this.
    void themeChanged(const QString& id);

private:
    ThemeManager();

    /// Which theme a machine with no stored preference should get.
    [[nodiscard]] static QString defaultThemeId();

    void apply(const theming::Theme& theme);

    theming::Theme m_current;
    QString m_currentId;
};
