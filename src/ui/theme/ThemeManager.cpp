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
#include <QWidget>

const QString ThemeManager::kBlenderDark    = QStringLiteral("blender-dark");
const QString ThemeManager::kClassic        = QStringLiteral("classic");
const QString ThemeManager::kWindows11      = QStringLiteral("windows11");
const QString ThemeManager::kWindowsXp      = QStringLiteral("winxp");
const QString ThemeManager::kWindows7       = QStringLiteral("win7");
const QString ThemeManager::kNeutralGrey    = QStringLiteral("neutral-grey");
const QString ThemeManager::kHighContrast   = QStringLiteral("high-contrast");
const QString ThemeManager::kSolarizedLight = QStringLiteral("solarized-light");
const QString ThemeManager::kPhosphor       = QStringLiteral("phosphor");
const QString ThemeManager::kPrintFriendly  = QStringLiteral("print-friendly");

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
    t.caption.background    = QColor(0x3a, 0x3a, 0x3a);
    t.caption.text          = QColor(0xe5, 0xe5, 0xe5);
    t.caption.buttonHover   = QColor(0x50, 0x50, 0x50);
    t.caption.closeHover    = QColor(0xc4, 0x2b, 0x1c);
    t.caption.closeHoverText= QColor(0xff, 0xff, 0xff);
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
    t.caption.background    = QColor(0x00, 0x00, 0x80);
    t.caption.text          = QColor(0xff, 0xff, 0xff);
    t.caption.buttonHover   = QColor(0x30, 0x30, 0x98);
    t.caption.closeHover    = QColor(0xc0, 0x20, 0x20);
    t.caption.closeHoverText= QColor(0xff, 0xff, 0xff);
    return t;
}

// ---------------------------------------------------------------------------
// Windows 11
// ---------------------------------------------------------------------------
// Hand-built on Fusion, not on the "windows11" style key, and that is a
// reversal worth explaining.
//
// Qt's windows11 style derives from its vista style and draws most controls
// through the UxTheme API -- that is, with whatever assets the *host* Windows
// supplies. On a Windows 11 machine that makes this theme and the Windows 7 one
// converge on the same flat controls, which is exactly what they were reported
// doing, and the elements Qt does draw itself came out looking like neither
// Windows: the spin box arrows rendered as a circle inside a circle.
//
// So both era themes are drawn here instead. The cost is that this is an
// approximation of Windows 11 rather than the real thing; the gain is that it
// is a *predictable* approximation that cannot change under a Windows update
// and cannot be mistaken for its neighbour. Flat surfaces, 4px radii, a hairline
// border and the accent blue only where something is focused.
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
    t.styleKey = QStringLiteral("Fusion");
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
    t.caption.background    = QColor(0xf3, 0xf3, 0xf3);
    t.caption.text          = QColor(0x1a, 0x1a, 0x1a);
    t.caption.buttonHover   = QColor(0xe6, 0xe6, 0xe6);
    t.caption.closeHover    = QColor(0xc4, 0x2b, 0x1c);
    t.caption.closeHoverText= QColor(0xff, 0xff, 0xff);

    // Deliberately nothing here for QAbstractSpinBox. Styling a spin button in
    // QSS opts its arrows out of the style's own drawing, and replacing them
    // needs image assets this repository does not carry -- so the spin boxes
    // keep Fusion's plain arrows, which is the whole complaint fixed.
    t.styleSheet = QStringLiteral(R"(
QPushButton, QToolButton {
    border: 1px solid #d1d1d1;
    border-bottom-color: #c4c4c4;
    border-radius: 4px;
    padding: 4px 14px;
    background: #fbfbfb;
}
QPushButton:hover, QToolButton:hover { background: #f2f2f2; }
QPushButton:pressed, QToolButton:pressed { background: #ebebeb; color: #5a5a5a; }
QPushButton:default { border: 1px solid #0067c0; background: #0067c0; color: #ffffff; }
QPushButton:default:hover { background: #1975c5; }
QPushButton:disabled, QToolButton:disabled {
    background: #f5f5f5; border-color: #e5e5e5; color: #a0a0a0;
}
QLineEdit, QPlainTextEdit, QTextEdit {
    border: 1px solid #d1d1d1;
    border-bottom: 2px solid #8a8a8a;
    border-radius: 4px;
    padding: 2px 6px;
    background: #ffffff;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus { border-bottom-color: #0067c0; }
QGroupBox {
    border: 1px solid #e2e2e2;
    border-radius: 6px;
    margin-top: 0.7em;
    padding-top: 0.4em;
}
QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }
QTabBar::tab {
    border: none;
    padding: 6px 14px;
    background: transparent;
}
QTabBar::tab:selected { border-bottom: 2px solid #0067c0; }
QTabBar::tab:hover:!selected { background: #ededed; }
QProgressBar {
    border: none;
    border-radius: 3px;
    background: #e6e6e6;
    text-align: center;
    max-height: 6px;
}
QProgressBar::chunk { border-radius: 3px; background: #0067c0; }
)");
    return t;
}

// ---------------------------------------------------------------------------
// Windows XP (Luna Blue)
// ---------------------------------------------------------------------------
// Nothing on this Qt draws Luna: the "windowsxp" style key stopped responding
// in Qt 5.14 and QWindowsXPStyle is gone. So this is the one theme built out of
// a style sheet, which suits it -- Luna's whole visual language is vertical
// gradients, 3px radii and one dark blue outline, and QSS has qlineargradient
// natively. No image assets are involved.
//
// Fusion underneath because it is the flattest starting point; the Windows
// styles impose metrics that fight the padding below.
theming::Theme windowsXp() {
    theming::Theme t;
    t.id = ThemeManager::kWindowsXp;
    t.styleKey = QStringLiteral("Fusion");
    t.colorScheme = Qt::ColorScheme::Light;

    QPalette p;
    // #ECE9D8 is the warm beige-grey that reads as XP from across the room.
    p.setColor(QPalette::Window,          QColor(0xec, 0xe9, 0xd8));
    p.setColor(QPalette::WindowText,      QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::Base,            QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::AlternateBase,   QColor(0xf5, 0xf3, 0xe8));
    p.setColor(QPalette::Text,            QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::Button,          QColor(0xec, 0xe9, 0xd8));
    p.setColor(QPalette::ButtonText,      QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::BrightText,      QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Highlight,       QColor(0x31, 0x6a, 0xc5));  // Luna selection
    p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::ToolTipBase,     QColor(0xff, 0xff, 0xe1));
    p.setColor(QPalette::ToolTipText,     QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::Link,            QColor(0x00, 0x00, 0xee));
    p.setColor(QPalette::LinkVisited,     QColor(0x55, 0x1a, 0x8b));
    p.setColor(QPalette::PlaceholderText, QColor(0x80, 0x80, 0x80));
    p.setColor(QPalette::Light,           QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Midlight,        QColor(0xf5, 0xf3, 0xe8));
    p.setColor(QPalette::Mid,             QColor(0xc0, 0xbd, 0xae));
    p.setColor(QPalette::Dark,            QColor(0x91, 0x8f, 0x84));
    p.setColor(QPalette::Shadow,          QColor(0x40, 0x3f, 0x3a));
    deriveDisabledGroup(p);
    t.palette = p;

    t.accents.accentChip  = {QColor(0x31, 0x6a, 0xc5), QColor(0xff, 0xff, 0xff)};
    t.accents.neutralChip = {QColor(0xd6, 0xd2, 0xc2), QColor(0x20, 0x20, 0x20)};
    t.accents.warningChip = {QColor(0xe9, 0xa8, 0x00), QColor(0x2a, 0x1e, 0x00)};
    t.accents.noticeBackground = QColor(0xff, 0xff, 0xe1);
    t.accents.noticeBorder     = QColor(0xe9, 0xa8, 0x00);
    t.accents.noticeText       = QColor(0x3a, 0x2c, 0x00);
    t.accents.errorText        = QColor(0xa0, 0x00, 0x00);
    t.caption.background    = QColor(0x0a, 0x54, 0xb8);
    t.caption.text          = QColor(0xff, 0xff, 0xff);
    t.caption.buttonHover   = QColor(0x3d, 0x7c, 0xd4);
    t.caption.closeHover    = QColor(0xd9, 0x4d, 0x21);
    t.caption.closeHoverText= QColor(0xff, 0xff, 0xff);

    // Buttons, tabs and the progress bar carry the look; the rest of the UI
    // reads as XP from the palette alone. #003C74 is Luna's outline blue and
    // #E9A800 its hover amber.
    t.styleSheet = QStringLiteral(R"(
QPushButton, QToolButton {
    border: 1px solid #003c74;
    border-radius: 3px;
    padding: 3px 12px;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:0.45 #f2f0e6, stop:0.5 #e6e3d3, stop:1 #f7f5ee);
}
QPushButton:hover, QToolButton:hover {
    border: 1px solid #e9a800;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:0.45 #fdf6e0, stop:0.5 #fbeec4, stop:1 #fefaf0);
}
QPushButton:pressed, QToolButton:pressed {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #d9d6c6, stop:0.5 #e4e1d1, stop:1 #f2f0e6);
}
QPushButton:disabled, QToolButton:disabled {
    border: 1px solid #a0a0a0;
    color: #8d8d8d;
    background: #f0eee4;
}
QTabBar::tab {
    border: 1px solid #003c74;
    border-bottom: none;
    border-top-left-radius: 3px;
    border-top-right-radius: 3px;
    padding: 4px 10px;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:1 #e6e3d3);
}
QTabBar::tab:selected {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:1 #f7f5ee);
}
QProgressBar {
    border: 1px solid #003c74;
    border-radius: 3px;
    background: #ffffff;
    text-align: center;
}
QProgressBar::chunk {
    margin: 1px;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #8cd67a, stop:0.5 #55b544, stop:1 #8cd67a);
}
)");
    return t;
}

// ---------------------------------------------------------------------------
// Windows 7 (Aero, without the glass)
// ---------------------------------------------------------------------------
// This started on the "windowsvista" style key, on the reading that Qt reaches
// Vista-era assets through UxTheme and would therefore still draw Aero on a
// modern Windows. It does not: what comes back is whatever the host Windows
// supplies, so on Windows 11 the controls came out flat and this theme was
// indistinguishable from the Windows 11 one. Both are hand-drawn now.
//
// Aero's grammar is a two-stop vertical gradient with a hard break at the
// midpoint -- the "gloss" is that discontinuity, not a soft blend -- a 3px
// radius, and a blue wash on hover rather than a colour change.
//
// The glass is absent and will stay absent. DwmEnableBlurBehindWindow has been
// a no-op since Windows 8, the replacement is undocumented, Qt 6 only honours
// WA_TranslucentBackground on a frameless window, and the centre of this one is
// an opaque Vulkan swapchain no backdrop shows through. Same wall the Windows
// 11 theme hit with Mica.
theming::Theme windows7() {
    theming::Theme t;
    t.id = ThemeManager::kWindows7;
    t.styleKey = QStringLiteral("Fusion");
    t.colorScheme = Qt::ColorScheme::Light;

    QPalette p;
    p.setColor(QPalette::Window,          QColor(0xf0, 0xf0, 0xf0));
    p.setColor(QPalette::WindowText,      QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::Base,            QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::AlternateBase,   QColor(0xf7, 0xfa, 0xfd));
    p.setColor(QPalette::Text,            QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::Button,          QColor(0xe1, 0xe8, 0xf3));
    p.setColor(QPalette::ButtonText,      QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::BrightText,      QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Highlight,       QColor(0x33, 0x99, 0xff));  // Aero selection
    p.setColor(QPalette::HighlightedText, QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::ToolTipBase,     QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::ToolTipText,     QColor(0x57, 0x57, 0x57));
    p.setColor(QPalette::Link,            QColor(0x00, 0x66, 0xcc));
    p.setColor(QPalette::LinkVisited,     QColor(0x80, 0x39, 0x9b));
    p.setColor(QPalette::PlaceholderText, QColor(0x9a, 0x9a, 0x9a));
    p.setColor(QPalette::Light,           QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Midlight,        QColor(0xf2, 0xf6, 0xfb));
    p.setColor(QPalette::Mid,             QColor(0xc4, 0xd3, 0xe6));
    p.setColor(QPalette::Dark,            QColor(0x8e, 0xa5, 0xbf));
    p.setColor(QPalette::Shadow,          QColor(0x5a, 0x6b, 0x80));
    deriveDisabledGroup(p);
    t.palette = p;

    t.accents.accentChip  = {QColor(0x2e, 0x78, 0xc8), QColor(0xff, 0xff, 0xff)};
    t.accents.neutralChip = {QColor(0xdc, 0xe6, 0xf2), QColor(0x1a, 0x2a, 0x3a)};
    t.accents.warningChip = {QColor(0xfd, 0xe3, 0x9a), QColor(0x54, 0x3c, 0x00)};
    t.accents.noticeBackground = QColor(0xff, 0xfb, 0xe6);
    t.accents.noticeBorder     = QColor(0xd8, 0xb0, 0x40);
    t.accents.noticeText       = QColor(0x5a, 0x44, 0x00);
    t.accents.errorText        = QColor(0xc0, 0x20, 0x20);
    t.caption.background    = QColor(0xd6, 0xe4, 0xf5);
    t.caption.text          = QColor(0x14, 0x28, 0x40);
    t.caption.buttonHover   = QColor(0xbe, 0xe6, 0xfd);
    t.caption.closeHover    = QColor(0xe8, 0x11, 0x00);
    t.caption.closeHoverText= QColor(0xff, 0xff, 0xff);

    // The Aero button's four states, in its own colours. Same deliberate
    // omission as the Windows 11 theme: nothing touches QAbstractSpinBox, so
    // its arrows stay with Fusion instead of disappearing for want of an image.
    t.styleSheet = QStringLiteral(R"(
QPushButton, QToolButton {
    border: 1px solid #8ba7c7;
    border-radius: 3px;
    padding: 4px 13px;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #fdfeff, stop:0.49 #eef4fc, stop:0.5 #dce8f7, stop:1 #eaf2fd);
}
QPushButton:hover, QToolButton:hover {
    border: 1px solid #3c7fb1;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #eaf6fd, stop:0.49 #d9f0fc, stop:0.5 #bee6fd, stop:1 #a7d9f5);
}
QPushButton:pressed, QToolButton:pressed {
    border: 1px solid #2c628b;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #e5f4fc, stop:0.49 #c4e5f6, stop:0.5 #98d1ef, stop:1 #68b3db);
}
QPushButton:disabled, QToolButton:disabled {
    border: 1px solid #bcbcbc;
    color: #838383;
    background: #f4f4f4;
}
QLineEdit, QPlainTextEdit, QTextEdit {
    border: 1px solid #abadb3;
    border-radius: 2px;
    padding: 2px 4px;
    background: #ffffff;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus { border: 1px solid #3c7fb1; }
QGroupBox {
    border: 1px solid #d5dfe9;
    border-radius: 4px;
    margin-top: 0.7em;
    padding-top: 0.4em;
}
QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }
QTabBar::tab {
    border: 1px solid #8ba7c7;
    border-bottom: none;
    border-top-left-radius: 3px;
    border-top-right-radius: 3px;
    padding: 4px 11px;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #fdfeff, stop:0.5 #eef4fc, stop:1 #dce8f7);
}
QTabBar::tab:selected {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #ffffff, stop:1 #f2f7fd);
}
QProgressBar {
    border: 1px solid #bcbcbc;
    border-radius: 3px;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #e6e6e6, stop:0.5 #f6f6f6, stop:1 #ffffff);
    text-align: center;
}
QProgressBar::chunk {
    margin: 1px;
    border-radius: 2px;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #b3e2a0, stop:0.49 #86cf6c, stop:0.5 #4ca62c, stop:1 #86cf6c);
}
)");
    return t;
}

// ---------------------------------------------------------------------------
// Neutral Grey
// ---------------------------------------------------------------------------
// The only theme here with a functional rather than an aesthetic argument.
// This window is wrapped around a rendered image, and the colour surrounding
// an image changes how its brightness and hue are judged -- simultaneous
// contrast. Imaging and colour-grading practice is a neutral grey workspace for
// exactly that reason, and colour-perception testing is done against grey.
//
// So every value below is strictly achromatic (R == G == B), including the
// selection colour: a coloured highlight would put a saturated patch back at
// the edge of the viewport, which is the thing this theme exists to remove.
// The window sits near sRGB middle grey, so neither a dark nor a bright render
// is being judged against a background pulling it the other way.
theming::Theme neutralGrey() {
    theming::Theme t;
    t.id = ThemeManager::kNeutralGrey;
    t.styleKey = QStringLiteral("Fusion");
    t.colorScheme = Qt::ColorScheme::Light;

    QPalette p;
    p.setColor(QPalette::Window,          QColor(0x80, 0x80, 0x80));
    p.setColor(QPalette::WindowText,      QColor(0x0d, 0x0d, 0x0d));
    p.setColor(QPalette::Base,            QColor(0x8e, 0x8e, 0x8e));
    p.setColor(QPalette::AlternateBase,   QColor(0x87, 0x87, 0x87));
    p.setColor(QPalette::Text,            QColor(0x0d, 0x0d, 0x0d));
    p.setColor(QPalette::Button,          QColor(0x8a, 0x8a, 0x8a));
    p.setColor(QPalette::ButtonText,      QColor(0x0d, 0x0d, 0x0d));
    p.setColor(QPalette::BrightText,      QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Highlight,       QColor(0x3d, 0x3d, 0x3d));
    p.setColor(QPalette::HighlightedText, QColor(0xf5, 0xf5, 0xf5));
    p.setColor(QPalette::ToolTipBase,     QColor(0x6e, 0x6e, 0x6e));
    p.setColor(QPalette::ToolTipText,     QColor(0xf0, 0xf0, 0xf0));
    p.setColor(QPalette::Link,            QColor(0x2a, 0x2a, 0x2a));
    p.setColor(QPalette::LinkVisited,     QColor(0x3a, 0x3a, 0x3a));
    p.setColor(QPalette::PlaceholderText, QColor(0x5e, 0x5e, 0x5e));
    p.setColor(QPalette::Light,           QColor(0x9c, 0x9c, 0x9c));
    p.setColor(QPalette::Midlight,        QColor(0x8e, 0x8e, 0x8e));
    p.setColor(QPalette::Mid,             QColor(0x6e, 0x6e, 0x6e));
    p.setColor(QPalette::Dark,            QColor(0x5a, 0x5a, 0x5a));
    p.setColor(QPalette::Shadow,          QColor(0x3a, 0x3a, 0x3a));
    deriveDisabledGroup(p);
    t.palette = p;

    // Achromatic here too, for the same reason -- these chips sit in the strip
    // directly above the image.
    t.accents.accentChip  = {QColor(0x3d, 0x3d, 0x3d), QColor(0xf5, 0xf5, 0xf5)};
    t.accents.neutralChip = {QColor(0x6e, 0x6e, 0x6e), QColor(0xf0, 0xf0, 0xf0)};
    t.accents.warningChip = {QColor(0xd8, 0xd8, 0xd8), QColor(0x1a, 0x1a, 0x1a)};
    t.accents.noticeBackground = QColor(0x8e, 0x8e, 0x8e);
    t.accents.noticeBorder     = QColor(0x4a, 0x4a, 0x4a);
    t.accents.noticeText       = QColor(0x0d, 0x0d, 0x0d);
    // The one deliberate exception: an error that reads as grey is not an
    // error. It is a thin strip of text, far from the image.
    t.accents.errorText        = QColor(0x8c, 0x00, 0x00);
    // Darker than the window rather than lighter, so the title clears 6:1
    // against it. The obvious mid-grey caption left the text at 4.4:1, which is
    // the sort of number that looks fine in a table and unreadable on screen.
    t.caption.background    = QColor(0x5a, 0x5a, 0x5a);
    t.caption.text          = QColor(0xf5, 0xf5, 0xf5);
    t.caption.buttonHover   = QColor(0x74, 0x74, 0x74);
    t.caption.closeHover    = QColor(0x8c, 0x00, 0x00);
    t.caption.closeHoverText= QColor(0xff, 0xff, 0xff);
    return t;
}

// ---------------------------------------------------------------------------
// High Contrast
// ---------------------------------------------------------------------------
// Black ground, white text, yellow focus -- the convention Windows' own high
// contrast schemes established, so it is what a user who needs this will
// expect. The point is legibility under a projector, in direct sunlight, or
// with low vision, which makes it the one theme here that is about being
// usable rather than about being liked.
theming::Theme highContrast() {
    theming::Theme t;
    t.id = ThemeManager::kHighContrast;
    t.styleKey = QStringLiteral("Fusion");
    t.colorScheme = Qt::ColorScheme::Dark;

    QPalette p;
    p.setColor(QPalette::Window,          QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::WindowText,      QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Base,            QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::AlternateBase,   QColor(0x1a, 0x1a, 0x1a));
    p.setColor(QPalette::Text,            QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Button,          QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::ButtonText,      QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::BrightText,      QColor(0xff, 0xff, 0x00));
    p.setColor(QPalette::Highlight,       QColor(0xff, 0xff, 0x00));
    p.setColor(QPalette::HighlightedText, QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::ToolTipBase,     QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::ToolTipText,     QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Link,            QColor(0x00, 0xff, 0xff));
    p.setColor(QPalette::LinkVisited,     QColor(0xff, 0x80, 0xff));
    p.setColor(QPalette::PlaceholderText, QColor(0xb0, 0xb0, 0xb0));
    // Bevel roles pushed to white so every widget edge is a visible line
    // rather than a shade of the background.
    p.setColor(QPalette::Light,           QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Midlight,        QColor(0xc0, 0xc0, 0xc0));
    p.setColor(QPalette::Mid,             QColor(0x90, 0x90, 0x90));
    p.setColor(QPalette::Dark,            QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Shadow,          QColor(0xff, 0xff, 0xff));
    // Disabled text is set explicitly rather than derived: fading towards the
    // background is precisely wrong here.
    t.palette = p;
    t.palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x8c, 0x8c, 0x8c));
    t.palette.setColor(QPalette::Disabled, QPalette::Text,       QColor(0x8c, 0x8c, 0x8c));
    t.palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x8c, 0x8c, 0x8c));

    t.accents.accentChip  = {QColor(0xff, 0xff, 0x00), QColor(0x00, 0x00, 0x00)};
    t.accents.neutralChip = {QColor(0x00, 0x00, 0x00), QColor(0xff, 0xff, 0xff)};
    t.accents.warningChip = {QColor(0xff, 0x80, 0x00), QColor(0x00, 0x00, 0x00)};
    t.accents.noticeBackground = QColor(0x00, 0x00, 0x00);
    t.accents.noticeBorder     = QColor(0xff, 0xff, 0x00);
    t.accents.noticeText       = QColor(0xff, 0xff, 0x00);
    t.accents.errorText        = QColor(0xff, 0x60, 0x60);
    t.caption.background    = QColor(0x00, 0x00, 0x00);
    t.caption.text          = QColor(0xff, 0xff, 0x00);
    t.caption.buttonHover   = QColor(0x33, 0x33, 0x00);
    t.caption.closeHover    = QColor(0xff, 0xff, 0x00);
    t.caption.closeHoverText= QColor(0x00, 0x00, 0x00);
    // The primary colour rather than a derived shade: on black there is no
    // darker to derive, and the yellow is what this theme is legible by.
    t.accents.separator        = QColor(0xff, 0xff, 0x00);
    return t;
}

// ---------------------------------------------------------------------------
// Solarized Light
// ---------------------------------------------------------------------------
// Ethan Schoonover's palette, unmodified: base3 ground, base00 body text, and
// the fixed accent hues. Its point is low contrast between text and ground
// with the hues held at equal perceived brightness, which is what makes it
// bearable for long sessions in a bright room -- the gap the other light
// themes here leave, both being cool and high contrast.
theming::Theme solarizedLight() {
    theming::Theme t;
    t.id = ThemeManager::kSolarizedLight;
    t.styleKey = QStringLiteral("Fusion");
    t.colorScheme = Qt::ColorScheme::Light;

    QPalette p;
    p.setColor(QPalette::Window,          QColor(0xee, 0xe8, 0xd5));  // base2
    p.setColor(QPalette::WindowText,      QColor(0x65, 0x7b, 0x83));  // base00
    p.setColor(QPalette::Base,            QColor(0xfd, 0xf6, 0xe3));  // base3
    p.setColor(QPalette::AlternateBase,   QColor(0xf5, 0xef, 0xdc));
    p.setColor(QPalette::Text,            QColor(0x58, 0x6e, 0x75));  // base01
    p.setColor(QPalette::Button,          QColor(0xee, 0xe8, 0xd5));
    p.setColor(QPalette::ButtonText,      QColor(0x58, 0x6e, 0x75));
    p.setColor(QPalette::BrightText,      QColor(0xdc, 0x32, 0x2f));  // red
    p.setColor(QPalette::Highlight,       QColor(0x26, 0x8b, 0xd2));  // blue
    p.setColor(QPalette::HighlightedText, QColor(0xfd, 0xf6, 0xe3));
    p.setColor(QPalette::ToolTipBase,     QColor(0xfd, 0xf6, 0xe3));
    p.setColor(QPalette::ToolTipText,     QColor(0x58, 0x6e, 0x75));
    p.setColor(QPalette::Link,            QColor(0x26, 0x8b, 0xd2));
    p.setColor(QPalette::LinkVisited,     QColor(0x6c, 0x71, 0xc4));  // violet
    p.setColor(QPalette::PlaceholderText, QColor(0x93, 0xa1, 0xa1));  // base1
    p.setColor(QPalette::Light,           QColor(0xfd, 0xf6, 0xe3));
    p.setColor(QPalette::Midlight,        QColor(0xf5, 0xef, 0xdc));
    p.setColor(QPalette::Mid,             QColor(0xd6, 0xd0, 0xbe));
    p.setColor(QPalette::Dark,            QColor(0x93, 0xa1, 0xa1));
    p.setColor(QPalette::Shadow,          QColor(0x65, 0x7b, 0x83));
    deriveDisabledGroup(p);
    t.palette = p;

    t.accents.accentChip  = {QColor(0x26, 0x8b, 0xd2), QColor(0xfd, 0xf6, 0xe3)};
    t.accents.neutralChip = {QColor(0xd6, 0xd0, 0xbe), QColor(0x58, 0x6e, 0x75)};
    t.accents.warningChip = {QColor(0xb5, 0x89, 0x00), QColor(0xfd, 0xf6, 0xe3)};  // yellow
    t.accents.noticeBackground = QColor(0xf7, 0xf1, 0xdd);
    t.accents.noticeBorder     = QColor(0xb5, 0x89, 0x00);
    t.accents.noticeText       = QColor(0x65, 0x51, 0x00);
    t.accents.errorText        = QColor(0xdc, 0x32, 0x2f);
    t.caption.background    = QColor(0xee, 0xe8, 0xd5);
    t.caption.text          = QColor(0x58, 0x6e, 0x75);
    t.caption.buttonHover   = QColor(0xe0, 0xd9, 0xc3);
    t.caption.closeHover    = QColor(0xdc, 0x32, 0x2f);
    t.caption.closeHoverText= QColor(0xfd, 0xf6, 0xe3);
    return t;
}

// ---------------------------------------------------------------------------
// Green Phosphor
// ---------------------------------------------------------------------------
// A P1-phosphor CRT: green on near-black, one hue throughout, brightness doing
// all the work.
//
// The monospace font that would complete the impression is deliberately not
// part of this. A theme that changes the application font changes every layout
// metric in the window, at every DPI scale factor, for one novelty -- so the
// theme system has no font dimension at all and this stays a palette. The
// numeric readouts are already monospace on their own.
theming::Theme phosphor() {
    theming::Theme t;
    t.id = ThemeManager::kPhosphor;
    t.styleKey = QStringLiteral("Fusion");
    t.colorScheme = Qt::ColorScheme::Dark;

    QPalette p;
    p.setColor(QPalette::Window,          QColor(0x0b, 0x12, 0x0b));
    p.setColor(QPalette::WindowText,      QColor(0x3b, 0xf5, 0x3b));
    p.setColor(QPalette::Base,            QColor(0x05, 0x09, 0x05));
    p.setColor(QPalette::AlternateBase,   QColor(0x0f, 0x1a, 0x0f));
    p.setColor(QPalette::Text,            QColor(0x3b, 0xf5, 0x3b));
    p.setColor(QPalette::Button,          QColor(0x14, 0x22, 0x14));
    p.setColor(QPalette::ButtonText,      QColor(0x3b, 0xf5, 0x3b));
    p.setColor(QPalette::BrightText,      QColor(0xc8, 0xff, 0xc8));
    p.setColor(QPalette::Highlight,       QColor(0x1f, 0x7a, 0x1f));
    p.setColor(QPalette::HighlightedText, QColor(0xd8, 0xff, 0xd8));
    p.setColor(QPalette::ToolTipBase,     QColor(0x05, 0x09, 0x05));
    p.setColor(QPalette::ToolTipText,     QColor(0x3b, 0xf5, 0x3b));
    p.setColor(QPalette::Link,            QColor(0x7c, 0xff, 0x7c));
    p.setColor(QPalette::LinkVisited,     QColor(0x5a, 0xc0, 0x5a));
    p.setColor(QPalette::PlaceholderText, QColor(0x24, 0x8c, 0x24));
    p.setColor(QPalette::Light,           QColor(0x2a, 0x4a, 0x2a));
    p.setColor(QPalette::Midlight,        QColor(0x1e, 0x34, 0x1e));
    p.setColor(QPalette::Mid,             QColor(0x16, 0x26, 0x16));
    p.setColor(QPalette::Dark,            QColor(0x0a, 0x14, 0x0a));
    p.setColor(QPalette::Shadow,          QColor(0x02, 0x05, 0x02));
    deriveDisabledGroup(p);
    t.palette = p;

    t.accents.accentChip  = {QColor(0x1f, 0x7a, 0x1f), QColor(0xd8, 0xff, 0xd8)};
    t.accents.neutralChip = {QColor(0x14, 0x22, 0x14), QColor(0x3b, 0xf5, 0x3b)};
    // No second hue: a warning is brighter, not orange.
    t.accents.warningChip = {QColor(0x8c, 0xff, 0x8c), QColor(0x04, 0x1a, 0x04)};
    t.accents.noticeBackground = QColor(0x0f, 0x1a, 0x0f);
    t.accents.noticeBorder     = QColor(0x3b, 0xf5, 0x3b);
    t.accents.noticeText       = QColor(0x8c, 0xff, 0x8c);
    t.accents.errorText        = QColor(0xc8, 0xff, 0xc8);
    t.caption.background    = QColor(0x0b, 0x12, 0x0b);
    t.caption.text          = QColor(0x3b, 0xf5, 0x3b);
    t.caption.buttonHover   = QColor(0x1c, 0x32, 0x1c);
    t.caption.closeHover    = QColor(0x3b, 0xf5, 0x3b);
    t.caption.closeHoverText= QColor(0x04, 0x14, 0x04);
    // Same reasoning as High Contrast: the phosphor green is the only colour
    // in this theme, so the boundaries are drawn in it.
    t.accents.separator        = QColor(0x3b, 0xf5, 0x3b);
    return t;
}

// ---------------------------------------------------------------------------
// Print Friendly
// ---------------------------------------------------------------------------
// For screenshotting the window and printing it on a monochrome laser printer.
// The other eight themes are about how the application looks on a display; this
// one is about what survives a grayscale conversion and a halftone screen, which
// is a different problem with a different answer.
//
// Three things follow from the target and drive every value below.
//
// **Hue carries nothing.** A grayscale conversion is a luminance projection, so
// two colours of different hue but equal luminance become the same grey. Every
// distinction this theme makes is therefore a luminance distinction -- there is
// no colour anywhere in it, not because monochrome is the aesthetic but because
// a colour that had to be told apart from another colour would not survive the
// trip.
//
// **Mid greys are the enemy, not the middle ground.** A laser printer has no
// continuous tone: it renders grey as a halftone dot screen, and a 600 dpi
// engine has on the order of ten to twenty usable levels. Density peaks around
// 50%, which is exactly where small text set in grey turns into a dot pattern.
// So the levels used here are pushed to the ends -- white, black, a light grey
// at ~14% for fills and a dark grey at ~24-38% for secondary text -- and no text
// is ever set on a mid grey. This is also why the disabled group is written out
// rather than passed through deriveDisabledGroup(): fading text towards a white
// window lands it in the dead zone and then past it into invisibility.
//
// **Structure has to be drawn, not shaded.** Print advice for the web is to drop
// background fills and replace them with borders, and the same applies here: a
// widget whose edge is a subtle shade of the window has no edge at all once
// printed. That is what the style sheet is for. Fusion is the style because it
// is the flattest starting point, but Fusion derives its own outline colour as
// window.darker(140), which on a white window is #b6b6b6 -- a light grey that
// prints as almost nothing. There is no palette role that overrides it; Light,
// Midlight, Mid, Dark and Shadow do not feed it. So the outlines are restated in
// QSS at full black, which is the one thing here a palette cannot express.
theming::Theme printFriendly() {
    theming::Theme t;
    t.id = ThemeManager::kPrintFriendly;
    t.styleKey = QStringLiteral("Fusion");
    t.colorScheme = Qt::ColorScheme::Light;

    QPalette p;
    p.setColor(QPalette::Window,          QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::WindowText,      QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::Base,            QColor(0xff, 0xff, 0xff));
    // The one tint in the theme: alternating rows need to differ from plain
    // ones, and ~7% is light enough that black text over it stays crisp.
    p.setColor(QPalette::AlternateBase,   QColor(0xed, 0xed, 0xed));
    p.setColor(QPalette::Text,            QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::Button,          QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::ButtonText,      QColor(0x00, 0x00, 0x00));
    // The role means "text that contrasts with Highlight", and Highlight here
    // is black.
    p.setColor(QPalette::BrightText,      QColor(0xff, 0xff, 0xff));
    // Selection as a solid black bar with knocked-out white text. The usual
    // blue is a mid grey once converted, which puts white text at about 3:1
    // and black text at about 4:1 -- a selected row that is harder to read
    // than an unselected one.
    p.setColor(QPalette::Highlight,       QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::ToolTipBase,     QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::ToolTipText,     QColor(0x00, 0x00, 0x00));
    // Links are black and stay underlined rather than blue: blue prints at
    // roughly the same density as body text, so the colour distinguished
    // nothing and only cost contrast.
    p.setColor(QPalette::Link,            QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::LinkVisited,     QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::PlaceholderText, QColor(0x60, 0x60, 0x60));
    // Bevel roles: white highlight, black shadow, so any bevel Fusion does draw
    // from the palette lands on one end of the range or the other.
    p.setColor(QPalette::Light,           QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Midlight,        QColor(0xf2, 0xf2, 0xf2));
    p.setColor(QPalette::Mid,             QColor(0x80, 0x80, 0x80));
    p.setColor(QPalette::Dark,            QColor(0x00, 0x00, 0x00));
    p.setColor(QPalette::Shadow,          QColor(0x00, 0x00, 0x00));
    t.palette = p;

    // Written out rather than derived, as above. #606060 is about 6:1 against
    // white -- unmistakably lighter than the black of an enabled control, and
    // still dark enough to print as letters rather than as texture.
    for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text,
                                     QPalette::ButtonText, QPalette::ToolTipText}) {
        t.palette.setColor(QPalette::Disabled, role, QColor(0x60, 0x60, 0x60));
    }
    t.palette.setColor(QPalette::Disabled, QPalette::Highlight,       QColor(0x60, 0x60, 0x60));
    t.palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));

    // The chips are a three-step luminance ladder -- 0%, 71%, 14% -- rather than
    // three hues. Each keeps its text at the far end of the range from its own
    // background; the mid greys that would sit between these steps are skipped
    // because neither black nor white text is legible on them.
    t.accents.accentChip  = {QColor(0x00, 0x00, 0x00), QColor(0xff, 0xff, 0xff)};
    t.accents.warningChip = {QColor(0x4a, 0x4a, 0x4a), QColor(0xff, 0xff, 0xff)};
    t.accents.neutralChip = {QColor(0xdc, 0xdc, 0xdc), QColor(0x00, 0x00, 0x00)};
    // No fill: a notice is marked out by its black rule, which costs no toner
    // and cannot muddy the text inside it.
    t.accents.noticeBackground = QColor(0xff, 0xff, 0xff);
    t.accents.noticeBorder     = QColor(0x00, 0x00, 0x00);
    t.accents.noticeText       = QColor(0x00, 0x00, 0x00);
    // Black, and so indistinguishable from body text -- the one thing this
    // theme genuinely cannot express. Any grey that set it apart would set it
    // apart by being *less* prominent than ordinary text, which is backwards
    // for an error. The notice box's rule carries the emphasis instead.
    t.accents.errorText        = QColor(0x00, 0x00, 0x00);
    // Named, not derived: white has nothing darker to derive from, exactly as
    // for High Contrast and Phosphor at the other end. Naming it also narrows
    // the dock separators from 4px to 2px, which is the whole of what this
    // theme needs -- solid black is the cheapest line a laser printer can lay
    // down reliably, so the band wants to be thin rather than pale. Screening
    // it to a grey instead would have been the wrong trade: grey is a halftone,
    // and a hairline halftone prints as intermittent dots rather than a line.
    t.accents.separator        = QColor(0x00, 0x00, 0x00);
    // ~24% grey. The derived midpoint would be #808080, the worst density a
    // halftone screen has; see Accents::mutedText.
    t.accents.mutedText        = QColor(0x3d, 0x3d, 0x3d);

    t.caption.background    = QColor(0xff, 0xff, 0xff);
    t.caption.text          = QColor(0x00, 0x00, 0x00);
    t.caption.buttonHover   = QColor(0xdc, 0xdc, 0xdc);
    t.caption.closeHover    = QColor(0x00, 0x00, 0x00);
    t.caption.closeHoverText= QColor(0xff, 0xff, 0xff);

    // Every rule here exists to replace something Fusion would have drawn in
    // its derived #b6b6b6 outline. Sizes are in em so they track the font and
    // therefore the DPI scale factor, the same reasoning as UiStyle's padding.
    //
    // Two conventions worth stating because they are not the obvious choice:
    //
    // - A pressed or selected control is filled black with its text knocked out
    //   white; a *checked* tool button instead keeps its white fill and doubles
    //   its border. Filling a checked tool button would have been stronger, but
    //   these carry icons, and an icon drawn in the theme's black over a black
    //   fill is an empty button. The padding drops by the pixel the border
    //   gains so the button does not resize when it is toggled.
    // - Check boxes and radio buttons fill solid when checked instead of
    //   drawing a tick or a dot. Once a border is set on an indicator, Qt stops
    //   drawing the native tick and expects an `image:` to replace it, and this
    //   theme ships no assets. A filled box against an empty one is the more
    //   legible of the two on paper anyway. Nothing here is tristate, so there
    //   is no third state left without a representation.
    t.styleSheet = QStringLiteral(R"(
QPushButton, QToolButton {
    border: 1px solid #000000;
    padding: 3px 12px;
    background: #ffffff;
    color: #000000;
}
QPushButton:hover, QToolButton:hover { background: #ededed; }
QPushButton:pressed, QToolButton:pressed { background: #000000; color: #ffffff; }
QPushButton:default { border: 2px solid #000000; padding: 2px 11px; }
QToolButton:checked { border: 2px solid #000000; padding: 2px 11px; background: #ffffff; }
QPushButton:disabled, QToolButton:disabled { border-color: #909090; color: #606060; }

QLineEdit, QPlainTextEdit, QTextEdit {
    border: 1px solid #000000;
    padding: 2px 4px;
    background: #ffffff;
    color: #000000;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus {
    border: 2px solid #000000;
    padding: 1px 3px;
}
QLineEdit:disabled, QPlainTextEdit:disabled, QTextEdit:disabled { border-color: #909090; }

QGroupBox {
    border: 1px solid #000000;
    margin-top: 0.7em;
    padding-top: 0.4em;
}
QGroupBox::title { subcontrol-origin: margin; left: 0.6em; padding: 0 0.3em; }

QCheckBox::indicator, QRadioButton::indicator {
    width: 0.85em;
    height: 0.85em;
    border: 1px solid #000000;
    background: #ffffff;
}
QRadioButton::indicator { border-radius: 0.5em; }
QCheckBox::indicator:checked, QRadioButton::indicator:checked { background: #000000; }
QCheckBox::indicator:disabled, QRadioButton::indicator:disabled { border-color: #909090; }
QCheckBox::indicator:checked:disabled, QRadioButton::indicator:checked:disabled {
    background: #909090;
}

QTabBar::tab {
    border: 1px solid #000000;
    border-bottom: none;
    padding: 4px 10px;
    background: #ffffff;
    color: #3d3d3d;
}
QTabBar::tab:selected { color: #000000; font-weight: bold; }
QTabBar::tab:!selected { margin-top: 2px; }

QProgressBar {
    border: 1px solid #000000;
    background: #ffffff;
    color: #000000;
    text-align: center;
}
QProgressBar::chunk { background: #000000; }

QSlider::groove:horizontal {
    border: 1px solid #000000;
    height: 2px;
    background: #ffffff;
}
QSlider::handle:horizontal {
    border: 1px solid #000000;
    background: #000000;
    width: 0.5em;
    margin: -0.45em 0;
}
QSlider::handle:horizontal:disabled { background: #909090; border-color: #909090; }

QHeaderView::section {
    border: none;
    border-bottom: 1px solid #000000;
    border-right: 1px solid #000000;
    padding: 3px 6px;
    background: #ffffff;
    color: #000000;
    font-weight: bold;
}

QMenuBar { border-bottom: 1px solid #000000; }
QMenuBar::item { padding: 4px 8px; background: transparent; }
QMenuBar::item:selected { background: #000000; color: #ffffff; }
QMenu { border: 1px solid #000000; background: #ffffff; }
QMenu::item:selected { background: #000000; color: #ffffff; }
QMenu::separator { height: 1px; background: #000000; margin: 0.2em 0.4em; }

QToolBar { border-bottom: 1px solid #000000; }
QStatusBar { border-top: 1px solid #000000; }
)");
    return t;
}

}  // namespace

ThemeManager::ThemeManager() = default;

ThemeManager& ThemeManager::instance() {
    static ThemeManager manager;
    return manager;
}

QVector<theming::Theme> ThemeManager::availableThemes() {
    // Menu order: the three that can be picked automatically first, then the
    // period pieces, then the working themes. Print Friendly goes last of the
    // working themes -- it is the one picked for an errand rather than to work
    // in, so it should not sit between two that are.
    return {blenderDark(), classic(), windows11(),
            windowsXp(), windows7(),
            neutralGrey(), highContrast(), solarizedLight(), phosphor(),
            printFriendly()};
}

QString ThemeManager::displayName(const QString& id) {
    // Plain tr("literal") calls, not a lookup through a helper: lupdate reads
    // literals at the call site, and a forwarded const char* produces no .ts
    // entry while looking perfectly translated. See src/i18n/CLAUDE.md.
    if (id == kBlenderDark)    return tr("Blender Dark");
    if (id == kClassic)        return tr("Classic");
    if (id == kWindows11)      return tr("Windows 11");
    if (id == kWindowsXp)      return tr("Windows XP");
    if (id == kWindows7)       return tr("Windows 7");
    if (id == kNeutralGrey)    return tr("Neutral Grey");
    if (id == kHighContrast)   return tr("High Contrast");
    if (id == kSolarizedLight) return tr("Solarized Light");
    if (id == kPhosphor)       return tr("Green Phosphor");
    if (id == kPrintFriendly)  return tr("Print Friendly");
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

    // Last, so it wins -- and unconditionally, so that leaving a theme that
    // defines one puts the next theme on a clean slate instead of under the
    // previous theme's gradients. qApp's sheet cascades with MainWindow's own
    // shell sheet rather than replacing it.
    qApp->setStyleSheet(theme.styleSheet);

    // Repaint everything, explicitly, rather than relying on the side effects
    // of the calls above.
    //
    // Qt repaints on its own when the palette changes or when a style sheet is
    // installed or removed. Neither covers a widget that paints straight from
    // the theme -- the drawn title bars take their background and text from
    // Caption, not from any palette role -- and setting an empty style sheet
    // over an empty one is no change at all, so Qt does nothing. Switching
    // between two themes that both carry no style sheet left those widgets
    // showing the previous theme's colours, while every switch involving one
    // of the three that do carry one happened to be repainted by the install
    // or the removal and looked correct.
    const auto widgets = QApplication::allWidgets();
    for (QWidget* widget : widgets) {
        widget->update();
    }
}
