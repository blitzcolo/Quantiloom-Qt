/**
 * @file TrajectoryPlotWidget.hpp
 * @brief One element's day: temperatures above, the fluxes that made them below
 *
 * The chart behind the thermal probe. It is two plots rather than one because
 * the quantities are two: kelvin near 300 and watts per square metre either
 * side of zero, which on one axis leaves the temperatures a flat line at the
 * top and the fluxes unreadable underneath.
 *
 * What makes the pair worth reading together is that the lower one explains the
 * upper. The six fluxes sum to what the surface is storing, so a rise in the
 * temperature has a term below it that went positive, and the question a
 * thermal render usually raises -- why is this surface that temperature -- is
 * answered by pointing at the chart rather than by reasoning about it.
 *
 * @author blitzcolo
 */

#pragma once

#include <renderer/ThermalControl.hpp>

#include <QWidget>

namespace uiplot {
class SpectrumPlotWidget;
}

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

namespace uiplot {

/**
 * @class TrajectoryPlotWidget
 * @brief Draws a ThermalElementTrajectory against time
 */
class TrajectoryPlotWidget : public QWidget {
    Q_OBJECT

public:
    explicit TrajectoryPlotWidget(QWidget* parent = nullptr);

    /// Replace everything drawn.
    void setTrajectory(const quantiloom::ThermalElementTrajectory& trajectory);
    void clear();

    /// Text shown when there is nothing to draw -- "click a surface", not an
    /// empty rectangle that gives no hint whether anything is wrong.
    void setPlaceholderText(const QString& text);

    /// Retitles both plots and the note. Called by the panel on a language
    /// change, since neither chart is a PanelBase with a retranslate of its own.
    void retranslate();

private:
    SpectrumPlotWidget* m_temperatures = nullptr;
    SpectrumPlotWidget* m_fluxes = nullptr;
    QLabel* m_note = nullptr;
    bool m_hadFluxes = false;
};

}  // namespace uiplot
