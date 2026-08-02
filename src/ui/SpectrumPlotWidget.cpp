/**
 * @file SpectrumPlotWidget.cpp
 * @brief Themed spectral curve plot implementation
 */

#include "SpectrumPlotWidget.hpp"

#include <QVBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QEvent>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <algorithm>
#include <limits>

namespace uiplot {

namespace {

/// Six hues that stay legible against both a near-white and a near-black plot
/// area. The dark-theme set is lightened rather than re-hued, so a curve keeps
/// its identity across a theme switch.
struct SeriesColours {
    QColor light;
    QColor dark;
};

const SeriesColours kPalette[] = {
    {QColor(0xC0, 0x21, 0x21), QColor(0xFF, 0x7A, 0x7A)},   // red
    {QColor(0x1A, 0x4F, 0xC4), QColor(0x7A, 0xB8, 0xFF)},   // blue
    {QColor(0x1B, 0x7A, 0x43), QColor(0x74, 0xD6, 0x9C)},   // green
    {QColor(0x9A, 0x5B, 0x00), QColor(0xF0, 0xB2, 0x5B)},   // amber
    {QColor(0x6B, 0x2E, 0x9E), QColor(0xC5, 0x93, 0xF0)},   // violet
    {QColor(0x0E, 0x6E, 0x78), QColor(0x63, 0xCE, 0xD9)},   // teal
};
constexpr int kPaletteSize = static_cast<int>(std::size(kPalette));

}  // namespace

SpectrumPlotWidget::SpectrumPlotWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* chart = new QChart();
    chart->setAnimationOptions(QChart::NoAnimation);
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    m_chartView = new QChartView(chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setMinimumHeight(160);
    layout->addWidget(m_chartView);

    m_placeholder = new QLabel(this);
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setWordWrap(true);
    layout->addWidget(m_placeholder);

    // QtCharts paints itself rather than following the shell style sheet, so
    // the theme has to be applied by hand -- and again on every switch.
    m_styling.attach(this);
    m_styling.bind([this] {
        uistyle::applyHintStyle(m_placeholder);
        applyTheme();
    });

    clear();
}

void SpectrumPlotWidget::setSeries(const QVector<Series>& series) {
    m_series = series;
    rebuild();
}

void SpectrumPlotWidget::clear() {
    m_series.clear();
    rebuild();
}

void SpectrumPlotWidget::setValueAxisTitle(const QString& title) {
    m_valueAxisTitle = title;
    rebuild();
}

void SpectrumPlotWidget::setPlaceholderText(const QString& text) {
    m_placeholder->setText(text);
}

void SpectrumPlotWidget::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    // The axis titles are the only translated strings the chart owns, and they
    // are recreated by rebuild() rather than held.
    if (event->type() == QEvent::LanguageChange) {
        rebuild();
    }
}

void SpectrumPlotWidget::rebuild() {
    auto* chart = m_chartView->chart();
    chart->removeAllSeries();
    const auto axes = chart->axes();
    for (auto* axis : axes) {
        chart->removeAxis(axis);
    }

    // Nothing to draw is a state worth naming, not a blank rectangle.
    const bool empty = std::none_of(m_series.cbegin(), m_series.cend(),
                                    [](const Series& s) { return !s.points.isEmpty(); });
    m_chartView->setVisible(!empty);
    m_placeholder->setVisible(empty);
    if (empty) {
        return;
    }

    double lambdaMin = std::numeric_limits<double>::max();
    double lambdaMax = std::numeric_limits<double>::lowest();
    double valueMin = 0.0;   // spectral quantities are non-negative; anchor at 0
    double valueMax = std::numeric_limits<double>::lowest();

    for (const Series& s : m_series) {
        if (s.points.isEmpty()) continue;
        auto* line = new QLineSeries();
        line->setName(s.name);
        // Carried on the series so applyTheme() can recolour without the
        // caller's list, which it does not keep.
        line->setProperty("colourIndex", s.colourIndex);
        for (const auto& [lambda, value] : s.points) {
            line->append(lambda, value);
            lambdaMin = std::min(lambdaMin, lambda);
            lambdaMax = std::max(lambdaMax, lambda);
            valueMin = std::min(valueMin, value);
            valueMax = std::max(valueMax, value);
        }
        chart->addSeries(line);
    }

    auto* axisX = new QValueAxis();
    axisX->setTitleText(tr("Wavelength (nm)"));
    axisX->setRange(lambdaMin, lambdaMax);
    chart->addAxis(axisX, Qt::AlignBottom);

    auto* axisY = new QValueAxis();
    axisY->setTitleText(m_valueAxisTitle);
    // A flat curve at zero would otherwise collapse the axis to a point.
    axisY->setRange(valueMin, valueMax > valueMin ? valueMax * 1.05 : valueMin + 1.0);
    chart->addAxis(axisY, Qt::AlignLeft);

    const auto seriesList = chart->series();
    for (auto* s : seriesList) {
        s->attachAxis(axisX);
        s->attachAxis(axisY);
    }

    applyTheme();
}

void SpectrumPlotWidget::applyTheme() {
    if (!m_chartView) {
        return;
    }
    auto* chart = m_chartView->chart();

    // Assigned absolutely from the palette rather than derived from the
    // chart's current colours, so re-running this on each theme switch is
    // idempotent.
    const QColor text = palette().color(QPalette::WindowText);
    const QColor base = palette().color(QPalette::Base);
    const bool darkTheme = palette().color(QPalette::Window).lightnessF() < 0.5;

    chart->setBackgroundBrush(base);
    chart->setBackgroundPen(Qt::NoPen);
    chart->setPlotAreaBackgroundBrush(base);
    chart->setTitleBrush(text);
    if (chart->legend()) {
        chart->legend()->setLabelColor(text);
        chart->legend()->setBackgroundVisible(false);
    }

    // Grid lines read as texture, not content.
    QColor grid = text;
    grid.setAlphaF(0.25);

    const auto axes = chart->axes();
    for (auto* axis : axes) {
        axis->setLabelsColor(text);
        axis->setTitleBrush(text);
        axis->setLinePenColor(text);
        axis->setGridLineColor(grid);
    }

    const auto seriesList = chart->series();
    for (auto* s : seriesList) {
        auto* line = qobject_cast<QLineSeries*>(s);
        if (!line) continue;
        const int index = line->property("colourIndex").toInt() % kPaletteSize;
        const SeriesColours& colours = kPalette[index < 0 ? 0 : index];
        line->setColor(darkTheme ? colours.dark : colours.light);
    }
}

}  // namespace uiplot
