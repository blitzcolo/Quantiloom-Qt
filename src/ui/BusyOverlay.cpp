/**
 * @file BusyOverlay.cpp
 * @brief The card shown while the renderer works off the GUI thread
 */

#include "BusyOverlay.hpp"

#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QStringList>

namespace {
constexpr int kCardWidth = 360;
constexpr int kCardHeight = 96;
constexpr int kMargin = 18;
constexpr int kSpinnerDiameter = 34;
constexpr qreal kSpinnerPen = 4.0;
constexpr int kFrameIntervalMs = 16;
constexpr int kDegreesPerFrame = 6;  // about a turn a second
constexpr int kArcSpanDegrees = 100;
}  // namespace

BusyOverlay::BusyOverlay(QWidget* anchor)
    : QWidget(anchor->window(), Qt::Tool | Qt::FramelessWindowHint)
    , m_anchor(anchor) {
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowModality(Qt::ApplicationModal);
    setFixedSize(kCardWidth, kCardHeight);

    m_spin.setInterval(kFrameIntervalMs);
    connect(&m_spin, &QTimer::timeout, this, [this] {
        m_angle = (m_angle + kDegreesPerFrame) % 360;
        update();
    });

    // A blocked window can still be dragged by its frame; the card goes with it.
    anchor->installEventFilter(this);
    anchor->window()->installEventFilter(this);
}

void BusyOverlay::start(const QString& message) {
    m_message = message;
    if (!isVisible()) {
        m_detail.clear();
        m_clock.start();
        m_angle = 0;
        reposition();
        show();
        m_spin.start();
    }
    update();
}

void BusyOverlay::setDetail(const QString& detail) {
    m_detail = detail;
    update();
}

void BusyOverlay::finish() {
    m_spin.stop();
    hide();
}

void BusyOverlay::reposition() {
    if (!m_anchor) {
        return;
    }
    const QPoint centre = m_anchor->mapToGlobal(m_anchor->rect().center());
    move(centre - QPoint(width() / 2, height() / 2));
}

bool BusyOverlay::eventFilter(QObject* watched, QEvent* event) {
    if (isVisible() && (event->type() == QEvent::Move || event->type() == QEvent::Resize)) {
        reposition();
    }
    return QWidget::eventFilter(watched, event);
}

void BusyOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPalette& pal = palette();

    painter.setPen(QPen(pal.color(QPalette::Mid), 1.0));
    painter.setBrush(pal.color(QPalette::Window));
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10.0, 10.0);

    // A faint ring with an arc running round it. Qt measures arcs in
    // sixteenths of a degree, counter-clockwise, so a falling start angle
    // turns it clockwise.
    const QRectF ring(kMargin, (height() - kSpinnerDiameter) / 2.0, kSpinnerDiameter,
                      kSpinnerDiameter);
    QColor track = pal.color(QPalette::Highlight);
    track.setAlphaF(0.2f);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(track, kSpinnerPen));
    painter.drawEllipse(ring);
    painter.setPen(QPen(pal.color(QPalette::Highlight), kSpinnerPen, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(ring, -m_angle * 16, kArcSpanDegrees * 16);

    // Title, then what it is working on, then how long it has been at it --
    // the one line that keeps moving through a solve that reports nothing.
    const int left = static_cast<int>(ring.right()) + kMargin;
    const int textWidth = width() - left - kMargin;
    QFont titleFont = font();
    titleFont.setBold(true);
    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics bodyMetrics(font());

    QStringList lines{m_detail.isEmpty() ? QString() : m_detail,
                      tr("%1 s elapsed").arg(m_clock.elapsed() / 1000)};
    lines.removeAll(QString());
    const int lineGap = 3;
    const int blockHeight = titleMetrics.height() +
                            static_cast<int>(lines.size()) * (bodyMetrics.height() + lineGap);
    int y = (height() - blockHeight) / 2;

    painter.setFont(titleFont);
    painter.setPen(pal.color(QPalette::WindowText));
    painter.drawText(QRect(left, y, textWidth, titleMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     titleMetrics.elidedText(m_message, Qt::ElideRight, textWidth));
    y += titleMetrics.height() + lineGap;

    painter.setFont(font());
    painter.setPen(pal.color(QPalette::PlaceholderText));
    for (const QString& line : lines) {
        painter.drawText(QRect(left, y, textWidth, bodyMetrics.height()),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         bodyMetrics.elidedText(line, Qt::ElideMiddle, textWidth));
        y += bodyMetrics.height() + lineGap;
    }
}
