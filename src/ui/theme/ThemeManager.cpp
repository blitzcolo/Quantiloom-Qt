/**
 * @file ThemeManager.cpp
 */

#include "ThemeManager.hpp"

#include <QApplication>
#include <QDebug>
#include <QOperatingSystemVersion>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>

const QString ThemeManager::kBlenderDark = QStringLiteral("blender-dark");
const QString ThemeManager::kClassic     = QStringLiteral("classic");
const QString ThemeManager::kWindows11   = QStringLiteral("windows11");

namespace {

constexpr auto kSettingsKey = "theme";

/// Fill the disabled colour group by fading text towards the background.
/// Every theme wants this and none of them wants to hand-pick six more hex
/// values for it.
void deriveDisabledGroup(QPalette& palette) {
    const QColor window = palette.color(QPalette::Active, QPalette::Window);

    const auto faded = [&window](QColor c) {
        return QColor::fromRgbF((c.redF()   + window.redF()   * 2.0) / 3.0,
                                (c.greenF() + window.greenF() * 2.0) / 3.0,
                                (c.blueF()  + window.blueF()  * 2.0) / 3.0);
    };

    for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text,
                                     QPalette::ButtonText, QPalette::ToolTipText}) {
        palette.setColor(QPalette::Disabled, role,
                         faded(palette.color(QPalette::Active, role)));
    }
    palette.setColor(QPalette::Disabled, QPalette::Highlight,
                     faded(palette.color(QPalette::Active, QPalette::Highlight)));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText,
                     faded(palette.color(QPalette::Active, QPalette::HighlightedText)));
}

// ---------------------------------------------------------------------------
// Blender Dark
// ---------------------------------------------------------------------------
// Sampled from Blender's own default dark theme: #303030 editor background,
// #1d1d1d for anything recessed (text fields, list backgrounds), #545454 for
// raised widgets, and the #4772b3 blue it uses for selection throughout.
// Fusion is the style because it is the only one that draws a flat, uniform
// widget on every platform -- the Windows styles insist on their own metrics.
theming::Theme blenderDark() {
    theming::Theme t;
    t.id = ThemeManager::kBlenderDark;
    t.styleKey = QStringLiteral("Fusion");
    t.colorScheme = Qt::ColorScheme::Dark;

    QPalette p;
    p.setColor(QPalette::Window,          QColor(0x30, 0x30, 0x30));
    p.setColor(QPalette::WindowText,      QColor(0xe5, 0xe5, 0xe5));
    p.setColor(QPalette::Base,            QColor(0x1d, 0x1d, 0x1d));
    p.setColor(QPalette::AlternateBase,   QColor(0x28, 0x28, 0x28));
    p.setColor(QPalette::Text,            QColor(0xe5, 0xe5, 0xe5));
    p.setColor(QPalette::Button,          QColor(0x54, 0x54, 0x54));
    p.setColor(QPalette::ButtonText,      QColor(0xe5, 0xe5, 0xe5));
    p.setColor(QPalette::BrightText,      QColor(0xff, 0x6b, 0x6b));
    p.setColor(QPalette::Highlight,       QColor(0x47, 0x72, 0xb3));
    p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::ToolTipBase,     QColor(0x1d, 0x1d, 0x1d));
    p.setColor(QPalette::ToolTipText,     QColor(0xe5, 0xe5, 0xe5));
    p.setColor(QPalette::Link,            QColor(0x5b, 0x8d, 0xd6));
    p.setColor(QPalette::LinkVisited,     QColor(0x8d, 0x7b, 0xd6));
    p.setColor(QPalette::PlaceholderText, QColor(0x8a, 0x8a, 0x8a));
    // Bevel roles: Blender draws almost flat, so these stay close together.
    p.setColor(QPalette::Light,           QColor(0x6a, 0x6a, 0x6a));
    p.setColor(QPalette::Midlight,        QColor(0x4a, 0x4a, 0x4a));
    p.setColor(QPalette::Mid,             QColor(0x3a, 0x3a, 0x3a));
    p.setColor(QPalette::Dark,            QColor(0x22, 0x22, 0x22));
    p.setColor(QPalette::Shadow,          QColor(0x14, 0x14, 0x14));
    deriveDisabledGroup(p);
    t.palette = p;

    t.accents.accentChip  = {QColor(0x47, 0x72, 0xb3), QColor(0xff, 0xff, 0xff)};
    t.accents.neutralChip = {QColor(0x3a, 0x3a, 0x3a), QColor(0xd0, 0xd0, 0xd0)};
    t.accents.warningChip = {QColor(0x8a, 0x63, 0x1c), QColor(0xff, 0xf1, 0xd6)};
    t.accents.noticeBackground = QColor(0x3a, 0x30, 0x1c);
    t.accents.noticeBorder     = QColor(0x8a, 0x70, 0x20);
    t.accents.noticeText       = QColor(0xf2, 0xe2, 0xb0);
    t.accents.errorText        = QColor(0xff, 0x6b, 0x6b);
    return t;
}

// ---------------------------------------------------------------------------
// Classic
// ---------------------------------------------------------------------------
// The Win9x/2000 grey, drawn by Qt's "Windows" style, which is the one style
// that still renders the raised and sunken bevels this look is made of. The
// Light/Midlight/Mid/Dark/Shadow roles are set explicitly here rather than left
// to Qt's defaults: those five are precisely what the bevels are drawn from, so
// the palette is not optional decoration for this theme.
theming::Theme classic() {
    theming::Theme t;
    t.id = ThemeManager::kClassic;
    t.styleKey = QStringLiteral("Windows");
    t.colorScheme = Qt::ColorScheme::Light;

    QPalette p;
    p.setColor(QPalette::Window,          QColor(0xd4, 0xd0, 0xc8));
    p.setColor(QPalette::WindowText,      QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::Base,            QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::AlternateBase,   QColor(0xe8, 0xe8, 0xe4));
    p.setColor(QPalette::Text,            QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::Button,          QColor(0xd4, 0xd0, 0xc8));
    p.setColor(QPalette::ButtonText,      QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::BrightText,      QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Highlight,       QColor(0x00, 0x00, 0x80));  // classic navy
    p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::ToolTipBase,     QColor(0xff, 0xff, 0xe1));
    p.setColor(QPalette::ToolTipText,     QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::Link,            QColor(0x00, 0x00, 0xee));
    p.setColor(QPalette::LinkVisited,     QColor(0x55, 0x1a, 0x8b));
    p.setColor(QPalette::PlaceholderText, QColor(0x80, 0x80, 0x80));
    p.setColor(QPalette::Light,           QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Midlight,        QColor(0xe0, 0xdf, 0xd8));
    p.setColor(QPalette::Mid,             QColor(0xa0, 0xa0, 0x9c));
    p.setColor(QPalette::Dark,            QColor(0x80, 0x80, 0x80));
    p.setColor(QPalette::Shadow,          QColor(0x00, 0x00, 0x00));
    deriveDisabledGroup(p);
    t.palette = p;

    t.accents.accentChip  = {QColor(0x00, 0x00, 0x80), QColor(0xff, 0xff, 0xff)};
    t.accents.neutralChip = {QColor(0xc0, 0xc0, 0xc0), QColor(0x00, 0x00, 0x00)};
    t.accents.warningChip = {QColor(0xff, 0xff, 0x00), QColor(0x00, 0x00, 0x00)};
    t.accents.noticeBackground = QColor(0xff, 0xff, 0xe1);
    t.accents.noticeBorder     = QColor(0x80, 0x80, 0x80);
    t.accents.noticeText       = QColor(0x00, 0x00, 0x00);
    t.accents.errorText        = QColor(0x80, 0x00, 0x00);
    return t;
}

// ---------------------------------------------------------------------------
// Windows 11
// ---------------------------------------------------------------------------
// The "windows11" style key comes from Qt's modern Windows style plugin
// (qmodernwindowsstyle), so this theme degrades to Fusion anywhere that plugin
// is absent -- an older Windows, or any other platform. The palette below is
// the light one either way, so the result stays coherent rather than
// half-native.
//
// Mica is deliberately not applied. The signature Windows 11 backdrop needs
// Qt::WA_TranslucentBackground, which Qt 6 only honours on a frameless window,
// and this application's centre is an opaque Vulkan swapchain
// (MainWindow.cpp: createWindowContainer) that no backdrop can show through.
// Rewriting the shell as frameless to blur a strip around a solid viewport is
// not a trade worth making.
theming::Theme windows11() {
    theming::Theme t;
    t.id = ThemeManager::kWindows11;
    t.styleKey = QStringLiteral("windows11");
    t.colorScheme = Qt::ColorScheme::Light;

    QPalette p;
    p.setColor(QPalette::Window,          QColor(0xf3, 0xf3, 0xf3));
    p.setColor(QPalette::WindowText,      QColor(0x1a, 0x1a, 0x1a));
    p.setColor(QPalette::Base,            QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::AlternateBase,   QColor(0xf9, 0xf9, 0xf9));
    p.setColor(QPalette::Text,            QColor(0x1a, 0x1a, 0x1a));
    p.setColor(QPalette::Button,          QColor(0xfb, 0xfb, 0xfb));
    p.setColor(QPalette::ButtonText,      QColor(0x1a, 0x1a, 0x1a));
    p.setColor(QPalette::BrightText,      QColor(0xc4, 0x2b, 0x1c));
    p.setColor(QPalette::Highlight,       QColor(0x00, 0x67, 0xc0));  // Windows accent
    p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::ToolTipBase,     QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::ToolTipText,     QColor(0x1a, 0x1a, 0x1a));
    p.setColor(QPalette::Link,            QColor(0x00, 0x5a, 0x9e));
    p.setColor(QPalette::LinkVisited,     QColor(0x74, 0x4d, 0xa9));
    p.setColor(QPalette::PlaceholderText, QColor(0x75, 0x75, 0x75));
    p.setColor(QPalette::Light,           QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Midlight,        QColor(0xfa, 0xfa, 0xfa));
    p.setColor(QPalette::Mid,             QColor(0xe1, 0xe1, 0xe1));
    p.setColor(QPalette::Dark,            QColor(0xc8, 0xc8, 0xc8));
    p.setColor(QPalette::Shadow,          QColor(0xa0, 0xa0, 0xa0));
    deriveDisabledGroup(p);
    t.palette = p;

    t.accents.accentChip  = {QColor(0x00, 0x67, 0xc0), QColor(0xff, 0xff, 0xff)};
    t.accents.neutralChip = {QColor(0xe4, 0xe4, 0xe4), QColor(0x30, 0x30, 0x30)};
    t.accents.warningChip = {QColor(0xfd, 0xe7, 0xc9), QColor(0x6b, 0x43, 0x00)};
    t.accents.noticeBackground = QColor(0xff, 0xf8, 0xe6);
    t.accents.noticeBorder     = QColor(0xe0, 0xb3, 0x4a);
    t.accents.noticeText       = QColor(0x6b, 0x4d, 0x00);
    t.accents.errorText        = QColor(0xc4, 0x2b, 0x1c);
    return t;
}

}  // namespace

ThemeManager::ThemeManager() = default;

ThemeManager& ThemeManager::instance() {
    static ThemeManager manager;
    return manager;
}

QVector<theming::Theme> ThemeManager::availableThemes() {
    return {blenderDark(), classic(), windows11()};
}

QString ThemeManager::displayName(const QString& id) {
    // Plain tr("literal") calls, not a lookup through a helper: lupdate reads
    // literals at the call site, and a forwarded const char* produces no .ts
    // entry while looking perfectly translated. See src/i18n/CLAUDE.md.
    if (id == kBlenderDark) return tr("Blender Dark");
    if (id == kClassic)     return tr("Classic");
    if (id == kWindows11)   return tr("Windows 11");
    return id;
}

QString ThemeManager::defaultThemeId() {
    // Windows older than 10 first, ahead of the light/dark question. Neither
    // the windows11 style nor a system dark mode exists there, so the generic
    // rules below would both give an answer that machine cannot honour.
    // Guarded by the compile-time platform macro rather than a runtime OSType
    // comparison: QOperatingSystemVersion::Windows and the value returned by
    // type() are different enumerations, and comparing them is deprecated.
#ifdef Q_OS_WIN
    if (QOperatingSystemVersion::current() < QOperatingSystemVersion::Windows10) {
        return kClassic;
    }
#endif

    if (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark) {
        return kBlenderDark;
    }
    return kWindows11;
}

void ThemeManager::applyStoredPreference() {
    QSettings settings;
    const QString saved = settings.value(kSettingsKey).toString();

    const QString wanted = saved.isEmpty() ? defaultThemeId() : saved;

    for (const auto& theme : availableThemes()) {
        if (theme.id == wanted) {
            apply(theme);
            return;
        }
    }

    // A stored id that no longer exists: fall back rather than start unthemed.
    for (const auto& theme : availableThemes()) {
        if (theme.id == defaultThemeId()) {
            apply(theme);
            return;
        }
    }
}

bool ThemeManager::switchTo(const QString& id) {
    if (id == m_currentId) {
        return true;
    }
    for (const auto& theme : availableThemes()) {
        if (theme.id == id) {
            apply(theme);
            QSettings settings;
            settings.setValue(kSettingsKey, id);
            emit themeChanged(m_currentId);
            return true;
        }
    }
    qDebug() << "ThemeManager: no such theme" << id;
    return false;
}

void ThemeManager::apply(const theming::Theme& theme) {
    m_current = theme;
    m_currentId = theme.id;

    // Order matters. QApplication::setStyle() installs the style's own standard
    // palette, so a palette set before it would be thrown away.
    if (QStyle* style = QStyleFactory::create(theme.styleKey)) {
        QApplication::setStyle(style);
    } else {
        qDebug() << "ThemeManager: style" << theme.styleKey
                 << "unavailable, falling back to Fusion";
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    }

    // Tell the platform which way the window frame should go. Without this the
    // title bar stays light while everything inside it turns dark.
    if (theme.colorScheme != Qt::ColorScheme::Unknown) {
        QGuiApplication::styleHints()->setColorScheme(theme.colorScheme);
    }

    QApplication::setPalette(theme.palette);
}
