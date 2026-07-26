/**
 * @file UiStyle.hpp
 * @brief Shared widget styling that survives DPI scaling and theme changes
 *
 * The panels used to reach for style sheets with absolute units — `11px` in
 * one place, `9pt`/`10pt`/`14pt` in others — plus hardcoded hex colors. Point
 * sizes scale with DPI and pixel sizes do not, so the two mixed badly at
 * 125%/150%; and the fixed colors were only legible against one theme.
 *
 * These helpers derive everything from the application font and palette
 * instead, so a label styled through them is correct at any scale factor and
 * under any theme. Nothing here hardcodes a pixel size, and since themes
 * became switchable at runtime, nothing here hardcodes a colour either — the
 * few shades that have no palette role come from the active theme's accents.
 *
 * ## Every helper is idempotent
 *
 * This is a requirement, not a happy accident. A theme change re-runs all of
 * them on the same widgets, so a helper that read the widget's *current* font
 * or palette and derived from that would compound: the hint font would shrink
 * by another 0.9 on every switch, and the muted text colour would fade one
 * more step towards the background each time. They read the application font
 * and palette instead, so the tenth application matches the first.
 */

#pragma once

#include <QString>

#include <functional>
#include <vector>

QT_BEGIN_NAMESPACE
class QEvent;
class QLabel;
class QWidget;
QT_END_NAMESPACE

namespace uistyle {

/// Muted explanatory text: one step smaller than the body font, drawn halfway
/// between the theme's text and window colours.
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
/// override notice).
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

/// A widget's registered styling, re-runnable on demand.
///
/// The colour twin of PanelBase::bindText(): the setter runs immediately and
/// again whenever the theme changes, so no colour is ever set once. PanelBase
/// owns one of these and exposes it as bindStyle(); widgets that are not
/// panels — MainWindow, ViewportFrame, PreferencesDialog — own one directly
/// and drive it from their own changeEvent.
///
/// @code
/// uistyle::applyHintStyle(hint);                              // before
/// m_styling.bind([hint] { uistyle::applyHintStyle(hint); });  // after
/// @endcode
class StyleBindings {
public:
    /// Run @p setter now, and again on every reapply().
    void bind(std::function<void()> setter);

    /// Re-run every registered setter. Safe to call any number of times —
    /// see the idempotence note at the top of this file — and safe to call
    /// from a handler for the events the setters themselves provoke.
    void reapply() const;

private:
    std::vector<std::function<void()>> m_setters;
    mutable bool m_running = false;
};

/// True when @p event is one of the events Qt posts after a theme change,
/// meaning any widget holding derived colours must recompute them.
[[nodiscard]] bool isThemeChangeEvent(const QEvent* event);

} // namespace uistyle
