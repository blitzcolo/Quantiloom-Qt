/**
 * @file LanguageManager.cpp
 */

#include "LanguageManager.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace {
constexpr auto kSettingsKey = "language";
}

LanguageManager::LanguageManager() = default;
LanguageManager::~LanguageManager() = default;

LanguageManager& LanguageManager::instance() {
    static LanguageManager manager;
    return manager;
}

QVector<LanguageManager::Language> LanguageManager::availableLanguages() {
    // Language names are written in their own language by convention, so they
    // are not translated. That is a decision, not an oversight -- a user
    // looking for their language should recognise it whatever the UI is
    // currently showing.
    return {
        {QStringLiteral("en"),    QStringLiteral("English")},
        {QStringLiteral("zh_CN"), QString::fromUtf8("中文")},
    };
}

void LanguageManager::applyStoredPreference() {
    QSettings settings;
    const QString saved = settings.value(kSettingsKey).toString();

    if (!saved.isEmpty() && install(saved)) {
        return;
    }

    // No stored preference (or it no longer resolves): follow the system.
    for (const QString& uiLanguage : QLocale::system().uiLanguages()) {
        const QString name = QLocale(uiLanguage).name();
        if (install(name)) {
            return;
        }
        // "zh-Hans-CN" resolves to "zh_CN"; a bare "en-GB" to "en".
        const QString base = name.section('_', 0, 0);
        if (base != name && install(base)) {
            return;
        }
    }

    // Nothing loaded: the sources are English, so that is what is shown.
    m_currentLocale = QStringLiteral("en");
}

bool LanguageManager::switchTo(const QString& locale) {
    if (locale == m_currentLocale) {
        return true;
    }
    if (!install(locale)) {
        return false;
    }

    QSettings settings;
    settings.setValue(kSettingsKey, locale);
    emit languageChanged(m_currentLocale);
    return true;
}

bool LanguageManager::install(const QString& locale) {
    if (locale.isEmpty()) {
        return false;
    }

    auto translator = std::make_unique<QTranslator>();
    const QString baseName = QStringLiteral("quantiloom_") + locale;
    if (!translator->load(baseName, QCoreApplication::applicationDirPath())) {
        qDebug() << "LanguageManager: no translation for" << baseName;
        return false;
    }

    // Remove first: installing a second translator for the same context would
    // leave the previous language winning for any string the new one has not
    // translated.
    if (m_appTranslator) {
        QCoreApplication::removeTranslator(m_appTranslator.get());
    }
    m_appTranslator = std::move(translator);
    QCoreApplication::installTranslator(m_appTranslator.get());

    // Qt's own strings (file dialogs, standard buttons). Optional: the
    // qtbase_*.qm files are only present when Qt's translations were deployed
    // alongside the executable.
    if (m_qtTranslator) {
        QCoreApplication::removeTranslator(m_qtTranslator.get());
        m_qtTranslator.reset();
    }
    auto qtTranslator = std::make_unique<QTranslator>();
    const QString qtBase = QStringLiteral("qtbase_") + locale;
    if (qtTranslator->load(qtBase, QLibraryInfo::path(QLibraryInfo::TranslationsPath)) ||
        qtTranslator->load(qtBase, QCoreApplication::applicationDirPath())) {
        m_qtTranslator = std::move(qtTranslator);
        QCoreApplication::installTranslator(m_qtTranslator.get());
    }

    m_currentLocale = locale;
    return true;
}
