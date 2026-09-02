/**
 * @file TrajectoryPlotWidget.cpp
 * @brief Implementation of the thermal probe's chart pair
 *
 * @author blitzcolo
 */

#include "TrajectoryPlotWidget.hpp"

#include "SpectrumPlotWidget.hpp"
#include "UiStyle.hpp"

#include <QLabel>
#include <QVBoxLayout>

namespace uiplot {
namespace {

/// (hours, value) pairs for one flux, pulled out of the trajectory's array of
/// structs. A lambda per term rather than a loop over an index, because the six
/// are named fields and not a array -- which is deliberate on the SDK side: a
/// flux you can only reach by number is a flux nobody can read.
template <typename Pick>
QVector<QPair<double, double>> FluxSeries(
    const quantiloom::ThermalElementTrajectory& trajectory, Pick pick) {
    QVector<QPair<double, double>> points;
    points.reserve(static_cast<int>(trajectory.fluxes.size()));
    for (size_t i = 0; i < trajectory.fluxes.size() && i < trajectory.time_h.size(); ++i) {
        points.append({trajectory.time_h[i], pick(trajectory.fluxes[i])});
    }
    return points;
}

}  // namespace

TrajectoryPlotWidget::TrajectoryPlotWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_temperatures = new SpectrumPlotWidget(this);
    m_fluxes = new SpectrumPlotWidget(this);
    // Neither is anchored at zero: a temperature near 300 K and a flux that
    // swings either side of it both read wrong on an axis that starts there.
    m_temperatures->setAnchorValueAxisAtZero(false);
    m_fluxes->setAnchorValueAxisAtZero(false);

    m_note = new QLabel(this);
    m_note->setWordWrap(true);
    m_note->setVisible(false);
    uistyle::applyHintStyle(m_note);

    // The temperatures get the larger share: they are what a user came to see,
    // and the fluxes are the explanation underneath.
    layout->addWidget(m_temperatures, 3);
    layout->addWidget(m_fluxes, 2);
    layout->addWidget(m_note);

    retranslate();
}

void TrajectoryPlotWidget::setTrajectory(
    const quantiloom::ThermalElementTrajectory& trajectory) {
    QVector<Series> temperatures;
    QVector<QPair<double, double>> surface;
    QVector<QPair<double, double>> back;
    surface.reserve(static_cast<int>(trajectory.time_h.size()));
    back.reserve(static_cast<int>(trajectory.time_h.size()));
    for (size_t i = 0; i < trajectory.time_h.size(); ++i) {
        if (i < trajectory.surfaceTemperature_K.size()) {
            surface.append({trajectory.time_h[i], trajectory.surfaceTemperature_K[i]});
        }
        if (i < trajectory.backTemperature_K.size()) {
            back.append({trajectory.time_h[i], trajectory.backTemperature_K[i]});
        }
    }
    temperatures.append(Series{tr("Surface"), surface, 0});
    temperatures.append(Series{tr("Back face"), back, 1});
    m_temperatures->setSeries(temperatures);

    QVector<Series> fluxes;
    if (!trajectory.fluxes.empty()) {
        using F = quantiloom::ThermalSurfaceFluxes;
        fluxes.append(Series{tr("Sun"),
                             FluxSeries(trajectory, [](const F& f) { return f.shortwave_W_m2; }), 0});
        fluxes.append(Series{tr("Long wave"),
                             FluxSeries(trajectory, [](const F& f) { return f.longwave_W_m2; }), 1});
        fluxes.append(Series{tr("Convection"),
                             FluxSeries(trajectory, [](const F& f) { return f.convection_W_m2; }), 2});
        fluxes.append(Series{tr("Evaporation"),
                             FluxSeries(trajectory, [](const F& f) { return f.latent_W_m2; }), 3});
        fluxes.append(Series{tr("Into the slab"),
                             FluxSeries(trajectory, [](const F& f) { return f.conduction_W_m2; }), 4});
        // Only when the mesh carries contacts. A flat zero in the legend would
        // read as a term that was measured and found to be nothing.
        const bool anyLateral = std::any_of(
            trajectory.fluxes.begin(), trajectory.fluxes.end(),
            [](const F& f) { return f.lateral_W_m2 != 0.0; });
        if (anyLateral) {
            fluxes.append(Series{tr("Across edges"),
                                 FluxSeries(trajectory, [](const F& f) { return f.lateral_W_m2; }), 5});
        }
    }
    m_fluxes->setSeries(fluxes);

    // A stepper that does not decompose its balance reports no fluxes, and
    // that is a different thing from a surface with nothing happening to it.
    m_hadFluxes = !trajectory.fluxes.empty();
    m_note->setVisible(!m_hadFluxes && !trajectory.time_h.empty());
    retranslate();
}

void TrajectoryPlotWidget::clear() {
    m_temperatures->clear();
    m_fluxes->clear();
    m_hadFluxes = false;
    m_note->setVisible(false);
}

void TrajectoryPlotWidget::setPlaceholderText(const QString& text) {
    m_temperatures->setPlaceholderText(text);
    m_fluxes->setPlaceholderText(text);
}

void TrajectoryPlotWidget::retranslate() {
    m_temperatures->setDomainAxisTitle(tr("Hour"));
    m_temperatures->setValueAxisTitle(tr("Temperature (K)"));
    m_fluxes->setDomainAxisTitle(tr("Hour"));
    m_fluxes->setValueAxisTitle(tr("Flux (W/m2), into the surface"));
    m_note->setText(tr("No flux breakdown: this solve ran on a stepper that does not "
                       "decompose its own energy balance."));
}

}  // namespace uiplot
