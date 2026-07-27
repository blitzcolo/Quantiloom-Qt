/**
 * @file TitleBar.cpp
 */

#include "TitleBar.hpp"

#include "../theme/ThemeManager.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>

namespace {

/// Caption button glyphs, in the order the buttons appear.
enum class Glyph { Minimise, Maximise, Restore, Close };

}  // namespace

// ============================================================================
// CaptionButton
// ============================================================================

/// One caption button. Draws its own hover wash and glyph from the theme's
/// caption colours -- not from the palette, because the strip it sits in is not
/// a window background and QPalette has no role that means "title bar".
class CaptionButton : public QAbstractButton {
public:
    CaptionButton(Glyph glyph, QWidget* parent)
        : QAbstractButton(parent), m_glyph(glyph) {
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover, true);
        setCursor(Qt::ArrowCursor);
    }

    void setGlyph(Glyph glyph) {
        m_glyph = glyph;
        update();
    }

    [[nodiscard]] QSize sizeHint() const override {
        // Windows' own caption buttons are a wide, short rectangle rather than
        // a square, and the proportion is most of what makes a title bar read
        // as a title bar. Derived from the font so it tracks DPI.
        const int h = QFontMetrics(QApplication::font()).height() * 2;
        return {static_cast<int>(h * 1.5), h};
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const theming::Caption& c = ThemeManager::instance().currentTheme().caption;
        const bool closing = m_glyph == Glyph::Close;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QColor stroke = c.text;
        if (underMouse() && isEnabled()) {
            const QColor wash = closing ? c.closeHover : c.buttonHover;
            p.fillRect(rect(), isDown() ? wash.darker(115) : wash);
            if (closing) {
                stroke = c.closeHoverText;
            }
        }

        // One pixel at 100%, two at 200%: a hairline that survives scaling.
        QPen pen(stroke);
        pen.setWidthF(std::max(1.0, devicePixelRatioF()));
        pen.setCapStyle(Qt::FlatCap);
        p.setPen(pen);

        // A glyph box of a fixed odd-ish size centred in the button, so the
        // strokes land on whole pixels more often than not.
        const int side = QFontMetrics(font()).height() * 2 / 3;
        const QRectF box = QRectF(0, 0, side, side)
                               .translated((width() - side) / 2.0, (height() - side) / 2.0);

        switch (m_glyph) {
            case Glyph::Minimise:
                p.drawLine(QPointF(box.left(), box.center().y()),
                           QPointF(box.right(), box.center().y()));
                break;
            case Glyph::Maximise:
                p.drawRect(box.adjusted(0.5, 0.5, -0.5, -0.5));
                break;
            case Glyph::Restore: {
                // The two offset squares Windows has used since 3.1.
                const qreal d = side * 0.28;
                p.drawRect(QRectF(box.left(), box.top() + d,
                                  box.width() - d, box.height() - d)
                               .adjusted(0.5, 0.5, -0.5, -0.5));
                p.drawPolyline(QPolygonF({
                    QPointF(box.left() + d, box.top() + d),
                    QPointF(box.left() + d, box.top()),
                    QPointF(box.right(), box.top()),
                    QPointF(box.right(), box.bottom() - d),
                    QPointF(box.right() - d, box.bottom() - d),
                }));
                break;
            }
            case Glyph::Close:
                p.drawLine(box.topLeft(), box.bottomRight());
                p.drawLine(box.topRight(), box.bottomLeft());
                break;
        }
    }

    bool event(QEvent* e) override {
        if (e->type() == QEvent::HoverEnter || e->type() == QEvent::HoverLeave) {
            update();
        }
        return QAbstractButton::event(e);
    }

private:
    Glyph m_glyph;
};

// ============================================================================
// TitleBar
// ============================================================================

TitleBar::TitleBar(QWidget* parent) : QWidget(parent) {
    setAutoFillBackground(false);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addStretch();

    m_minimise = new CaptionButton(Glyph::Minimise, this);
    m_maximise = new CaptionButton(Glyph::Maximise, this);
    m_close    = new CaptionButton(Glyph::Close, this);
    for (CaptionButton* b : {m_minimise, m_maximise, m_close}) {
        layout->addWidget(b);
    }

    connect(m_minimise, &QAbstractButton::clicked, this, &TitleBar::minimiseRequested);
    connect(m_maximise, &QAbstractButton::clicked, this, &TitleBar::maximiseRequested);
    connect(m_close,    &QAbstractButton::clicked, this, &TitleBar::closeRequested);

    m_styling.attach(this);
    m_styling.bind([this] {
        setFont(QApplication::font());
        setFixedHeight(QFontMetrics(font()).height() * 2);
        update();
    });

    retranslateUi();
}

bool TitleBar::isCaptionAt(const QPoint& pos) const {
    // The buttons are the only children; the title is painted, not a widget.
    // So anything that is not over a button is draggable caption.
    return childAt(pos) == nullptr;
}

void TitleBar::setDialogMode(bool dialog) {
    m_minimise->setVisible(!dialog);
    m_maximise->setVisible(!dialog);
    update();
}

void TitleBar::setWindowMaximized(bool maximized) {
    m_maximise->setGlyph(maximized ? Glyph::Restore : Glyph::Maximise);
    retranslateUi();
}

void TitleBar::retranslateUi() {
    if (window()) {
        m_titleText = window()->windowTitle();
        update();
    }
    m_minimise->setToolTip(tr("Minimise"));
    m_maximise->setToolTip(window() && window()->isMaximized() ? tr("Restore Down")
                                                               : tr("Maximise"));
    m_close->setToolTip(tr("Close"));
}

void TitleBar::paintEvent(QPaintEvent*) {
    const theming::Caption& c = ThemeManager::instance().currentTheme().caption;

    QPainter p(this);
    p.fillRect(rect(), c.background);

    // The title is painted rather than carried by a QLabel. A label's colour
    // has to come from its palette, and Qt's own guidance is that a palette
    // set on a widget is not reliable once a style sheet is in play -- which
    // it is for the themes that carry one, and QStyleSheetStyle stays involved
    // for the rest of the session once any theme has installed one. The result
    // was a caption whose background was the theme's and whose text was not,
    // in whatever contrast that happened to leave.
    p.setPen(c.text);
    const QRect textRect = rect().adjusted(10, 0, -buttonStripWidth(), 0);
    if (textRect.width() > 0) {
        const QString shown = QFontMetrics(font()).elidedText(
            m_titleText, Qt::ElideRight, textRect.width());
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, shown);
    }
}

int TitleBar::buttonStripWidth() const {
    int w = 0;
    for (const QWidget* b : {static_cast<const QWidget*>(m_minimise),
                             static_cast<const QWidget*>(m_maximise),
                             static_cast<const QWidget*>(m_close)}) {
        if (b && b->isVisible()) {
            w += b->width();
        }
    }
    // A gap so a long document name stops short of the buttons rather than
    // running up against them.
    return w + 12;
}

void TitleBar::restyleUi() {
    m_styling.reapply();
    update();
    for (QWidget* b : {static_cast<QWidget*>(m_minimise),
                       static_cast<QWidget*>(m_maximise),
                       static_cast<QWidget*>(m_close)}) {
        b->update();
    }
}

void TitleBar::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        retranslateUi();
    } else if (uistyle::isThemeChangeEvent(e)) {
        restyleUi();
    }
    QWidget::changeEvent(e);
}

bool TitleBar::event(QEvent* e) {
    // The window title is a document name that changes as files are opened and
    // edited, so it is mirrored rather than copied once.
    if (e->type() == QEvent::WindowTitleChange) {
        retranslateUi();
    }
    return QWidget::event(e);
}
