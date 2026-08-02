/**
 * @file SpectrumPlotWidget.hpp
 * @brief A themed line plot of one or more spectral curves
 *
 * The application had exactly one chart -- the (n, k) preview inside the
 * spectral material generator -- and it was wired to that panel's data, drew in
 * Qt's default colours, and stayed a light chart inside the dark themes. Every
 * other spectral quantity the core carries (measured reflectance, emissivity,
 * an illuminant) had no visualisation at all.
 *
 * This is that chart, generalised: hand it named series, it draws them in the
 * current theme and restyles itself when the theme changes.
 *
 * @author blitzcolo
 */

#pragma once

#include "UiStyle.hpp"

#include <QWidget>
#include <QVector>

#include <utility>

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

QT_FORWARD_DECLARE_CLASS(QChartView)

namespace uiplot {

/// One curve: wavelength/value pairs, and the name the legend shows.
struct Series {
    QString name;
    /// (wavelength nm, value). Assumed sorted by wavelength.
    QVector<QPair<double, double>> points;
    /// Index into the widget's palette. Series with the same index share a
    /// colour, which is what lets a "before" and "after" pair read as one
    /// quantity.
    int colourIndex = 0;
};

/**
 * @class SpectrumPlotWidget
 * @brief Draws spectral curves against wavelength, in the active theme
 */
class SpectrumPlotWidget : public QWidget {
    Q_OBJECT

public:
    explicit SpectrumPlotWidget(QWidget* parent = nullptr);

    /// Replace everything drawn. An empty list shows the placeholder.
    void setSeries(const QVector<Series>& series);
    void clear();

    /// Y-axis caption. The X axis is always wavelength in nanometres.
    void setValueAxisTitle(const QString& title);

    /// Text shown when there is nothing to draw -- "select a material", not an
    /// empty rectangle that gives no hint whether anything is wrong.
    void setPlaceholderText(const QString& text);

protected:
    void changeEvent(QEvent* event) override;

private:
    void rebuild();
    void applyTheme();

    QChartView* m_chartView = nullptr;
    QLabel* m_placeholder = nullptr;
    QVector<Series> m_series;
    QString m_valueAxisTitle;
    uistyle::StyleBindings m_styling;
};

}  // namespace uiplot
