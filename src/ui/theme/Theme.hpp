/**
 * @file Theme.hpp
 * @brief One theme, as data
 *
 * A theme is four things stacked, in the order Qt applies them:
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
 *  4. an optional **style sheet** — shape, for the one thing a palette cannot
 *     express. Only the XP theme uses it, because Luna's look *is* gradients
 *     and rounded corners and no style key on this Qt draws them any more.
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

    /// The lines dividing the docked regions, and the rule under each dock
    /// title. Leave invalid and shellStyleSheet() derives one by darkening the
    /// window colour, which is right for the themes built on a mid-toned
    /// background and useless on the ones that are not: there is nothing
    /// darker than the High Contrast black or the Phosphor near-black, so the
    /// boundaries vanished. Those two name the colour instead.
    QColor separator;

    /// Hint and explanation text -- UiStyle's applyHintStyle(), nineteen call
    /// sites. Leave invalid and it is derived as the midpoint between the text
    /// and window colours, which is what every theme here wants: a colour that
    /// stays legible whichever of the two is darker, without a tenth hex value
    /// per theme.
    ///
    /// The same escape hatch as @ref separator, and for the same reason -- the
    /// derivation encodes an assumption that one theme does not share. Halfway
    /// from black to white is a 50% grey, and a 50% grey is the worst case for
    /// a laser printer: it is the densest halftone screen, so small text set in
    /// it prints as a fuzzy dot pattern rather than as letters. Print Friendly
    /// names a darker grey instead of accepting the midpoint.
    QColor mutedText;
};

/// Colours for the window's own title bar, once the application draws it
/// rather than Windows. Only four are given per theme; the inactive variants
/// are faded from these, because nine themes times every state is a lot of hex
/// for a strip of chrome.
struct Caption {
    QColor background;
    QColor text;
    /// Wash behind a caption button under the cursor.
    QColor buttonHover;
    /// Close is the one button that gets its own hover colour, by every
    /// platform's convention, because it is the one that cannot be undone.
    QColor closeHover;
    QColor closeHoverText;
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
    Caption caption;

    /// Application-wide style sheet, or empty for none.
    ///
    /// Applied last so it wins, and *cleared* when a theme does not define one
    /// -- otherwise switching away from a themed style sheet would leave it
    /// painted over the next theme. Note that touching any border property in
    /// QSS opts the widget out of the style's native drawing entirely, so this
    /// is all-or-nothing per widget class: it is the right tool for a look no
    /// style provides and the wrong one for nudging a look that a style
    /// already gets right.
    QString styleSheet;
};

}  // namespace theming
