/**
 * @file UiStyle.hpp
 * @brief Shared widget styling that survives DPI scaling and theme changes
 *
 * The panels used to reach for style sheets with absolute units — `11px` in
 * one place, `9pt`/`10pt`/`14pt` in others — plus hardcoded hex colors. Point
 * sizes scale with DPI and pixel sizes do not, so the two mixed badly at
 * 125%/150%; and the fixed colors were only legible against one theme.
 *
 * These helpers derive everything from the widget's own font and palette
 * instead, so a label styled through them is correct at any scale factor and
 * in both light and dark themes. Nothing here hardcodes a pixel size.
 */

#pragma once

#include <QString>

QT_BEGIN_NAMESPACE
class QLabel;
class QWidget;
QT_END_NAMESPACE

namespace uistyle {

/// Muted explanatory text: one step smaller than the body font, drawn in the
/// palette's disabled text color.
void applyHintStyle(QLabel* label);

/// Same as applyHintStyle but for a whole subtree (help pages, static docs).
void applyHintStyle(QWidget* widget, bool smaller);

/// Fixed-pitch text for numeric readouts. Keeps the body font's size.
void applyMonospaceStyle(QLabel* label);

/// Tone of a status chip.
enum class ChipTone {
    Accent,   ///< current mode, active tool
    Neutral,  ///< inert information
    Warning   ///< something the user should notice
};

/// Rounded, filled label used for the status bar and viewport mode chips.
void applyChipStyle(QLabel* label, ChipTone tone);

/// Boxed warning paragraph (the "preview only" spectral notice, the IR
/// override notice). Readable on both light and dark backgrounds.
void applyNoticeStyle(QLabel* label);

/// Bold section heading inside a panel.
void applyHeadingStyle(QLabel* label);

/// Stylesheet for the main window.
///
/// Qt's default dock separator is a 1-pixel gap in the window background, so
/// with several docks around the viewport there is nothing to tell one region
/// from the next. This widens it and gives it a colour, and makes each dock's
/// title read as the heading of its region. Colours come from @p reference's
/// palette, so the result follows the theme rather than assuming a light one.
[[nodiscard]] QString shellStyleSheet(const QWidget* reference);

} // namespace uistyle
