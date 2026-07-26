/**
 * @file LanguageManager.hpp
 * @brief Runtime language switching
 *
 * Installing or removing a QTranslator makes Qt post QEvent::LanguageChange to
 * every widget in the application. Panels answer it through
 * PanelBase::changeEvent, MainWindow through its own changeEvent, and the
 * interface changes language without a restart.
 *
 * The only requirement this places on the rest of the code is that no
 * user-visible string may be set once at construction time and never again —
 * everything lives in a `retranslateUi()` that can be called any number of
 * times.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <memory>

class QTranslator;

class LanguageManager : public QObject {
    Q_OBJECT

public:
    struct Language {
        QString locale;      ///< "en", "zh_CN" — the .qm file suffix
        QString nativeName;  ///< shown in the UI, always in its own language
    };

    static LanguageManager& instance();

    /// Languages this build ships a translation for.
    [[nodiscard]] static QVector<Language> availableLanguages();

    /// Apply the stored preference, falling back to the system locale. Call
    /// once, before the main window is constructed.
    void applyStoredPreference();

    /// Switch language now and remember the choice. Returns false when the
    /// translation could not be loaded, in which case nothing changed.
    bool switchTo(const QString& locale);

    [[nodiscard]] QString currentLocale() const { return m_currentLocale; }

signals:
    void languageChanged(const QString& locale);

private:
    LanguageManager();
    ~LanguageManager() override;

    bool install(const QString& locale);

    std::unique_ptr<QTranslator> m_appTranslator;
    std::unique_ptr<QTranslator> m_qtTranslator;
    QString m_currentLocale;
};
