/**
 * @file TitleBar.hpp
 * @brief The window's caption, drawn by the application rather than by Windows
 *
 * A plain widget: an icon, the window title, and minimise / maximise / close.
 * It knows nothing about Win32 — WindowChrome is what stops Windows drawing its
 * own caption and routes mouse input here. That split matters, because it is
 * what lets this file be ordinary Qt and that one be entirely platform code.
 *
 * ## What is deliberately *not* implemented here
 *
 * Dragging the window, double-click to maximise, Aero Snap, the Alt+Space
 * system menu, and snapping to a screen edge are all absent, and their absence
 * is the design. WindowChrome answers Windows' hit test with HTCAPTION over
 * this strip, which makes Windows treat it as a real title bar and provide
 * every one of those behaviours itself. Reimplementing them in mouse handlers
 * is the usual way a custom title bar ends up subtly wrong on a second monitor.
 *
 * The buttons are drawn as vector paths rather than glyphs or images: they have
 * to be crisp at 125% and 150% scaling, and this repository ships no icon
 * assets.
 */

#pragma once

#include "../UiStyle.hpp"

#include <QWidget>

class CaptionButton;

class TitleBar : public QWidget {
    Q_OBJECT

public:
    explicit TitleBar(QWidget* parent = nullptr);

    /// True when @p pos (in this widget's coordinates) is draggable caption
    /// rather than one of the buttons. WindowChrome asks this before answering
    /// HTCAPTION, because a button under HTCAPTION would never see a click.
    [[nodiscard]] bool isCaptionAt(const QPoint& pos) const;

    /// Follows the window's maximised state so the glyph can switch between
    /// "maximise" and "restore".
    void setWindowMaximized(bool maximized);

    /// Leave only Close. A dialog is neither minimised nor maximised, so the
    /// other two buttons would be decoration that does nothing when clicked.
    void setDialogMode(bool dialog);

    void retranslateUi();

signals:
    void minimiseRequested();
    void maximiseRequested();
    void closeRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool event(QEvent* event) override;

private:
    void restyleUi();
    /// Width the three buttons occupy, so the title can be elided before it
    /// reaches them.
    [[nodiscard]] int buttonStripWidth() const;

    QString m_titleText;
    CaptionButton* m_minimise = nullptr;
    CaptionButton* m_maximise = nullptr;
    CaptionButton* m_close = nullptr;

    uistyle::StyleBindings m_styling;
};
