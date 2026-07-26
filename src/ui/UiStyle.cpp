/**
 * @file UiStyle.cpp
 */

#include "UiStyle.hpp"

#include <QApplication>
#include <QFont>
#include <QLabel>
#include <QPalette>
#include <QWidget>

namespace {

/// A colour that reads as "text, but quieter" against whatever the current
/// theme uses as a window background.
QColor mutedTextColor(const QWidget* w) {
    const QPalette pal = w ? w->palette() : QApplication::palette();
    QColor text = pal.color(QPalette::WindowText);
    QColor base = pal.color(QPalette::Window);
    // Blend halfway towards the background rather than picking a fixed grey,
    // so it stays legible whichever of the two is darker.
    return QColor::fromRgbF((text.redF()   + base.redF())   * 0.5,
                            (text.greenF() + base.greenF()) * 0.5,
                            (text.blueF()  + base.blueF())  * 0.5);
}

/// True when the theme is dark, used to pick chip fills with enough contrast.
bool isDarkTheme(const QWidget* w) {
    const QPalette pal = w ? w->palette() : QApplication::palette();
    return pal.color(QPalette::Window).lightnessF() < 0.5;
}

/// Font one step smaller than the widget's own, expressed in points so that
/// Qt scales it with the screen's DPI.
void shrinkFont(QWidget* w, double factor) {
    QFont f = w->font();
    if (f.pointSizeF() > 0.0) {
        f.setPointSizeF(f.pointSizeF() * factor);
    }
    w->setFont(f);
}

} // namespace

namespace uistyle {

void applyHintStyle(QLabel* label) {
    if (!label) return;
    label->setWordWrap(true);
    shrinkFont(label, 0.9);
    QPalette pal = label->palette();
    pal.setColor(QPalette::WindowText, mutedTextColor(label));
    label->setPalette(pal);
}

void applyHintStyle(QWidget* widget, bool smaller) {
    if (!widget) return;
    if (smaller) {
        shrinkFont(widget, 0.9);
    }
    QPalette pal = widget->palette();
    pal.setColor(QPalette::WindowText, mutedTextColor(widget));
    widget->setPalette(pal);
}

void applyMonospaceStyle(QLabel* label) {
    if (!label) return;
    QFont f = label->font();
    f.setStyleHint(QFont::Monospace);
    f.setFamily(QStringLiteral("monospace"));
    label->setFont(f);
}

void applyChipStyle(QLabel* label, ChipTone tone) {
    if (!label) return;

    const bool dark = isDarkTheme(label);
    QString background;
    QString foreground;
    switch (tone) {
        case ChipTone::Accent:
            background = dark ? QStringLiteral("#2e5f8f") : QStringLiteral("#4a90d9");
            foreground = QStringLiteral("#ffffff");
            break;
        case ChipTone::Neutral:
            background = dark ? QStringLiteral("#3a3a3a") : QStringLiteral("#d8d8d8");
            foreground = dark ? QStringLiteral("#e8e8e8") : QStringLiteral("#202020");
            break;
        case ChipTone::Warning:
            background = dark ? QStringLiteral("#7a5a10") : QStringLiteral("#f0c674");
            foreground = dark ? QStringLiteral("#fff4d6") : QStringLiteral("#3a2c00");
            break;
    }

    // Padding is expressed in em so it tracks the font, which tracks DPI.
    label->setStyleSheet(QStringLiteral(
        "QLabel { background-color: %1; color: %2;"
        " padding: 0.15em 0.6em; border-radius: 0.35em; font-weight: bold; }")
        .arg(background, foreground));
}

void applyNoticeStyle(QLabel* label) {
    if (!label) return;
    label->setWordWrap(true);
    shrinkFont(label, 0.9);

    const bool dark = isDarkTheme(label);
    const QString background = dark ? QStringLiteral("#4a3c14") : QStringLiteral("#fff8dc");
    const QString border     = dark ? QStringLiteral("#8a7020") : QStringLiteral("#daa520");
    const QString foreground = dark ? QStringLiteral("#f2e2b0") : QStringLiteral("#7a5c00");

    label->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background-color: %2; border: 1px solid %3;"
        " border-radius: 0.3em; padding: 0.5em; }")
        .arg(foreground, background, border));
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
    QFont f = label->font();
    f.setBold(true);
    label->setFont(f);
}

} // namespace uistyle
