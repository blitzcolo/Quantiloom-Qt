/**
 * @file UiStyle.cpp
 */

#include "UiStyle.hpp"

#include "theme/ThemeManager.hpp"

#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QLabel>
#include <QPalette>
#include <QWidget>

namespace {

/// The palette every derivation below starts from.
///
/// Deliberately the *application* palette, not the widget's own. applyHintStyle
/// writes a derived colour back into the widget's palette, so re-deriving from
/// the widget would feed the previous result into the next calculation and the
/// text would fade a step further towards the background on every theme
/// change. The application palette is what ThemeManager sets, and it does not
/// move underneath us.
QPalette basePalette() {
    return QApplication::palette();
}

/// A colour that reads as "text, but quieter" against the theme's window
/// background.
QColor mutedTextColor() {
    const QPalette pal = basePalette();
    QColor text = pal.color(QPalette::WindowText);
    QColor base = pal.color(QPalette::Window);
    // Blend halfway towards the background rather than picking a fixed grey,
    // so it stays legible whichever of the two is darker.
    return QColor::fromRgbF((text.redF()   + base.redF())   * 0.5,
                            (text.greenF() + base.greenF()) * 0.5,
                            (text.blueF()  + base.blueF())  * 0.5);
}

/// Font @p factor of the application's body font, expressed in points so that
/// Qt scales it with the screen's DPI.
///
/// Scaled from QApplication::font() rather than the widget's current font: the
/// widget's font is this function's own previous output, so compounding it
/// would shrink the label on every theme change.
void scaleFontFromAppDefault(QWidget* w, double factor) {
    QFont f = QApplication::font();
    if (f.pointSizeF() > 0.0) {
        f.setPointSizeF(f.pointSizeF() * factor);
    }
    w->setFont(f);
}

const theming::Accents& accents() {
    return ThemeManager::instance().currentTheme().accents;
}

} // namespace

namespace uistyle {

void applyHintStyle(QLabel* label) {
    if (!label) return;
    label->setWordWrap(true);
    scaleFontFromAppDefault(label, 0.9);
    QPalette pal = label->palette();
    pal.setColor(QPalette::WindowText, mutedTextColor());
    label->setPalette(pal);
}

void applyHintStyle(QWidget* widget, bool smaller) {
    if (!widget) return;
    if (smaller) {
        scaleFontFromAppDefault(widget, 0.9);
    }
    QPalette pal = widget->palette();
    pal.setColor(QPalette::WindowText, mutedTextColor());
    widget->setPalette(pal);
}

void applyMonospaceStyle(QLabel* label) {
    if (!label) return;
    QFont f = QApplication::font();
    f.setStyleHint(QFont::Monospace);
    f.setFamily(QStringLiteral("monospace"));
    label->setFont(f);
}

void applyChipStyle(QLabel* label, ChipTone tone) {
    if (!label) return;

    theming::ChipColors colors;
    switch (tone) {
        case ChipTone::Accent:  colors = accents().accentChip;  break;
        case ChipTone::Neutral: colors = accents().neutralChip; break;
        case ChipTone::Warning: colors = accents().warningChip; break;
    }

    // Padding is expressed in em so it tracks the font, which tracks DPI.
    label->setStyleSheet(QStringLiteral(
        "QLabel { background-color: %1; color: %2;"
        " padding: 0.15em 0.6em; border-radius: 0.35em; font-weight: bold; }")
        .arg(colors.background.name(), colors.foreground.name()));
}

void applyNoticeStyle(QLabel* label) {
    if (!label) return;
    label->setWordWrap(true);
    scaleFontFromAppDefault(label, 0.9);

    const theming::Accents& a = accents();
    label->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background-color: %2; border: 1px solid %3;"
        " border-radius: 0.3em; padding: 0.5em; }")
        .arg(a.noticeText.name(), a.noticeBackground.name(), a.noticeBorder.name()));
}

QString shellStyleSheet(const QWidget* reference) {
    const QPalette pal = reference ? reference->palette() : QApplication::palette();
    const bool dark = pal.color(QPalette::Window).lightnessF() < 0.5;

    // A separator has to be visibly darker than the window on a light theme and
    // visibly darker than the panels on a dark one; blending towards black in
    // both directions gets that without two hand-picked colour sets.
    const QColor window = pal.color(QPalette::Window);
    const QColor separator = dark ? window.darker(160) : window.darker(135);
    const QColor separatorHover = pal.color(QPalette::Highlight);
    const QColor titleBackground = dark ? window.lighter(125) : window.darker(108);
    const QColor titleText = pal.color(QPalette::WindowText);

    return QStringLiteral(
        "QMainWindow::separator {"
        "  background: %1;"
        "  width: 4px;"
        "  height: 4px;"
        "}"
        "QMainWindow::separator:hover {"
        "  background: %2;"
        "}"
        "QDockWidget::title {"
        "  background: %3;"
        "  color: %4;"
        "  padding: 4px 8px;"
        "  border-bottom: 1px solid %1;"
        "}")
        .arg(separator.name(), separatorHover.name(),
             titleBackground.name(), titleText.name());
}

void applyHeadingStyle(QLabel* label) {
    if (!label) return;
    QFont f = QApplication::font();
    f.setBold(true);
    label->setFont(f);
}

void StyleBindings::bind(std::function<void()> setter) {
    setter();
    m_setters.push_back(std::move(setter));
}

void StyleBindings::reapply() const {
    // A setter typically calls setPalette or setStyleSheet, and both of those
    // post the very events that got us here. That is harmless when the target
    // is a child widget, but a widget restyling *itself* -- MainWindow and its
    // shell style sheet -- would otherwise re-enter forever.
    if (m_running) {
        return;
    }
    m_running = true;
    for (const auto& setter : m_setters) {
        setter();
    }
    m_running = false;
}

bool isThemeChangeEvent(const QEvent* event) {
    if (!event) return false;
    switch (event->type()) {
        // ApplicationPaletteChange follows QApplication::setPalette and
        // PaletteChange the resulting per-widget resolution; StyleChange
        // follows setStyle; ThemeChange is what the platform sends when the
        // system scheme flips underneath us. All four mean the same thing here,
        // and which ones arrive is not worth depending on.
        case QEvent::ApplicationPaletteChange:
        case QEvent::PaletteChange:
        case QEvent::StyleChange:
        case QEvent::ThemeChange:
            return true;
        default:
            return false;
    }
}

} // namespace uistyle
