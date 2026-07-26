/**
 * @file Theme.hpp
 * @brief One theme, as data
 *
 * A theme is three things stacked, in the order Qt applies them:
 *
 *  1. a **style** — the drawing algorithm, named by a QStyleFactory key. This
 *     is what decides whether a button has a Win9x bevel or a Windows 11
 *     rounded rectangle; no amount of colour will get you from one to the
 *     other.
 *  2. a **palette** — the colours, which Qt propagates down the widget tree on
 *     its own. Most of this application already reads its colours from the
 *     palette (see UiStyle.hpp), so this is what does the visible work.
 *  3. **accents** — the handful of colours UiStyle needs that have no palette
 *     role: the status chips and the notice box. These used to be hex literals
 *     chosen by a dark/light branch, which is exactly as far as that approach
 *     goes.
 *
 * Adding a theme means adding one of these, not writing code.
 */

#pragma once

#include <QColor>
#include <QPalette>
#include <QString>

namespace theming {

/// Background/foreground pair for a filled status chip.
struct ChipColors {
    QColor background;
    QColor foreground;
};

/// The colours UiStyle needs that QPalette has no role for.
struct Accents {
    ChipColors accentChip;   ///< current mode, active tool
    ChipColors neutralChip;  ///< inert information
    ChipColors warningChip;  ///< something the user should notice

    QColor noticeBackground;
    QColor noticeBorder;
    QColor noticeText;

    /// Text stating something is wrong — the Kirchhoff energy-conservation
    /// warning. Not QPalette::BrightText: that role means "text that contrasts
    /// with Highlight", and on the classic palette it is white.
    QColor errorText;
};

struct Theme {
    /// Stable identifier. Persisted in QSettings and used in the menu; never
    /// shown to the user, never translated. See ThemeManager::displayName().
    QString id;

    /// QStyleFactory key. Falls back to Fusion when the platform does not
    /// provide it -- "windows11" only exists where the modern Windows style
    /// plugin is deployed.
    QString styleKey;

    /// Light or dark. Drives QStyleHints::setColorScheme(), which is what gets
    /// the window title bar to match rather than staying stubbornly light.
    Qt::ColorScheme colorScheme = Qt::ColorScheme::Unknown;

    QPalette palette;
    Accents accents;
};

}  // namespace theming
