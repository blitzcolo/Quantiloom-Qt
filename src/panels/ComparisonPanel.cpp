/**
 * @file ComparisonPanel.cpp
 * @brief Implementation of the measured-comparison panel
 *
 * @author blitzcolo
 */

#include "ComparisonPanel.hpp"

#include "../ui/UiStyle.hpp"

#include <io/ImageIO.hpp>

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

/// A channel's values from an image, in reading order, skipping nothing.
std::vector<double> ChannelOf(const quantiloom::Image& image, const quantiloom::u32 channel) {
    std::vector<double> out;
    if (channel >= image.channels) return out;
    out.reserve(static_cast<size_t>(image.width) * image.height);
    for (quantiloom::u32 y = 0; y < image.height; ++y) {
        for (quantiloom::u32 x = 0; x < image.width; ++x) {
            const size_t i =
                (static_cast<size_t>(y) * image.width + x) * image.channels + channel;
            out.push_back(static_cast<double>(image.data[i]));
        }
    }
    return out;
}

}  // namespace

ComparisonPanel::ComparisonPanel(QWidget* parent) : PanelBase(parent) {
    auto* layout = new QVBoxLayout(this);

    m_loadButton = new QPushButton(this);
    connect(m_loadButton, &QPushButton::clicked, this, &ComparisonPanel::onLoadReference);
    layout->addWidget(m_loadButton);

    m_referenceCaption = new QLabel(this);
    m_referenceCaption->setWordWrap(true);
    uistyle::applyHintStyle(m_referenceCaption);
    layout->addWidget(m_referenceCaption);

    m_channelCombo = new QComboBox(this);
    connect(m_channelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { updateReport(); });
    layout->addWidget(m_channelCombo);

    m_compareButton = new QPushButton(this);
    connect(m_compareButton, &QPushButton::clicked, this, &ComparisonPanel::onCompare);
    layout->addWidget(m_compareButton);

    m_report = new QLabel(this);
    m_report->setWordWrap(true);
    m_report->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_report);

    m_differenceView = new QLabel(this);
    m_differenceView->setMinimumHeight(160);
    m_differenceView->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_differenceView, 1);

    layout->addStretch();
    retranslateUi();
}

ComparisonPanel::~ComparisonPanel() = default;

QString ComparisonPanel::panelTitle() const {
    return tr("Comparison");
}

void ComparisonPanel::retranslateUi() {
    m_loadButton->setText(tr("Load reference..."));
    m_compareButton->setText(tr("Compare with the current frame"));
    if (m_referencePath.isEmpty()) {
        m_referenceCaption->setText(tr("No reference loaded. An EXR of a measurement, or of "
                                       "another render."));
    }
    if (!m_reference || !m_rendered) {
        m_report->setText(tr("Load a reference, then compare."));
    } else {
        updateReport();
    }
}

void ComparisonPanel::onLoadReference() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Load reference image"), QString(),
        tr("OpenEXR (*.exr);;All files (*)"));
    if (path.isEmpty()) return;

    auto loaded = quantiloom::ImageIO::ReadEXR(path.toStdString());
    if (!loaded.has_value()) {
        m_referenceCaption->setText(tr("Could not read %1.").arg(QFileInfo(path).fileName()));
        return;
    }

    m_reference = std::make_unique<quantiloom::Image>(std::move(loaded.value()));
    m_referencePath = path;

    // Channels by the name the file gave them, since a comparison against a
    // measurement is usually about one band rather than about a colour.
    m_channelCombo->clear();
    for (quantiloom::u32 c = 0; c < m_reference->channels; ++c) {
        const QString name = c < m_reference->channelNames.size()
                                 ? QString::fromStdString(m_reference->channelNames[c])
                                 : tr("Channel %1").arg(c);
        m_channelCombo->addItem(name, c);
    }

    m_referenceCaption->setText(tr("%1: %2 x %3, %4 channel(s).")
                                    .arg(QFileInfo(path).fileName())
                                    .arg(m_reference->width)
                                    .arg(m_reference->height)
                                    .arg(m_reference->channels));
    updateReport();
}

void ComparisonPanel::onCompare() {
    // The window owns the swapchain, so it is what can read a frame back.
    emit frameRequested();
}

void ComparisonPanel::setRenderedImage(std::unique_ptr<quantiloom::Image> image) {
    m_rendered = std::move(image);
    updateReport();
}

void ComparisonPanel::updateReport() {
    if (!m_reference) {
        m_report->setText(tr("Load a reference, then compare."));
        return;
    }
    if (!m_rendered) {
        m_report->setText(tr("Reference loaded. Compare to read the current frame."));
        return;
    }
    if (m_rendered->width != m_reference->width ||
        m_rendered->height != m_reference->height) {
        // Not resampled on the user's behalf: which resampling, and what it
        // does to a radiance, is a decision about the measurement rather than
        // a convenience, and guessing it would put an interpolation inside a
        // number someone is going to quote.
        m_report->setText(tr("The frame is %1 x %2 and the reference is %3 x %4. "
                             "Match the render resolution to the reference rather than "
                             "resampling either -- what a resampling does to a radiance "
                             "belongs to whoever made the measurement.")
                              .arg(m_rendered->width)
                              .arg(m_rendered->height)
                              .arg(m_reference->width)
                              .arg(m_reference->height));
        m_differenceView->clear();
        return;
    }

    const auto channel = static_cast<quantiloom::u32>(
        m_channelCombo->currentData().isValid() ? m_channelCombo->currentData().toUInt() : 0u);
    const auto reference = ChannelOf(*m_reference, channel);
    const auto rendered = ChannelOf(*m_rendered, std::min(channel, m_rendered->channels - 1));
    if (reference.empty() || rendered.size() != reference.size()) {
        m_report->setText(tr("That channel is not in both images."));
        return;
    }

    double sum = 0.0;
    double sumSquares = 0.0;
    double worst = 0.0;
    double referenceSum = 0.0;
    std::vector<double> absolute;
    absolute.reserve(reference.size());
    for (size_t i = 0; i < reference.size(); ++i) {
        const double d = rendered[i] - reference[i];
        sum += d;
        sumSquares += d * d;
        worst = std::max(worst, std::abs(d));
        referenceSum += reference[i];
        absolute.push_back(std::abs(d));
    }
    const double n = static_cast<double>(reference.size());
    const double bias = sum / n;
    const double rmse = std::sqrt(sumSquares / n);
    const double referenceMean = referenceSum / n;

    // The 95th percentile beside the worst pixel, because one bad pixel is a
    // firefly and a bad percentile is a wrong scene.
    std::nth_element(absolute.begin(), absolute.begin() + static_cast<long>(0.95 * n),
                     absolute.end());
    const double p95 = absolute[static_cast<size_t>(0.95 * n)];

    m_report->setText(
        tr("Against %1, channel %2:\n"
           "  reference mean  %3\n"
           "  bias            %4  (%5%)\n"
           "  RMSE            %6\n"
           "  95th percentile %7\n"
           "  worst pixel     %8\n"
           "Bias is the render minus the reference, so positive means brighter.")
            .arg(QFileInfo(m_referencePath).fileName())
            .arg(m_channelCombo->currentText())
            .arg(referenceMean, 0, 'g', 6)
            .arg(bias, 0, 'g', 4)
            .arg(referenceMean != 0.0 ? 100.0 * bias / referenceMean : 0.0, 0, 'f', 2)
            .arg(rmse, 0, 'g', 4)
            .arg(p95, 0, 'g', 4)
            .arg(worst, 0, 'g', 4));

    showDifference();
}

void ComparisonPanel::showDifference() {
    if (!m_reference || !m_rendered) return;

    const auto channel = static_cast<quantiloom::u32>(
        m_channelCombo->currentData().isValid() ? m_channelCombo->currentData().toUInt() : 0u);
    const auto reference = ChannelOf(*m_reference, channel);
    const auto rendered = ChannelOf(*m_rendered, std::min(channel, m_rendered->channels - 1));
    if (reference.size() != rendered.size() || reference.empty()) return;

    // Signed, and scaled by the 99th percentile rather than the maximum: one
    // firefly would otherwise set the scale and the rest of the frame would
    // come out flat grey.
    std::vector<double> magnitudes;
    magnitudes.reserve(reference.size());
    for (size_t i = 0; i < reference.size(); ++i) {
        magnitudes.push_back(std::abs(rendered[i] - reference[i]));
    }
    const auto at = magnitudes.begin() + static_cast<long>(0.99 * magnitudes.size());
    std::nth_element(magnitudes.begin(), at, magnitudes.end());
    const double scale = *at > 0.0 ? *at : 1.0;

    QImage picture(static_cast<int>(m_reference->width),
                   static_cast<int>(m_reference->height), QImage::Format_RGB888);
    for (quantiloom::u32 y = 0; y < m_reference->height; ++y) {
        for (quantiloom::u32 x = 0; x < m_reference->width; ++x) {
            const size_t i = static_cast<size_t>(y) * m_reference->width + x;
            const double d = std::clamp((rendered[i] - reference[i]) / scale, -1.0, 1.0);
            // Blue where the render is darker than the measurement, red where
            // it is brighter, and near-black where the two agree.
            const int warm = static_cast<int>(255.0 * std::max(d, 0.0));
            const int cool = static_cast<int>(255.0 * std::max(-d, 0.0));
            picture.setPixel(static_cast<int>(x), static_cast<int>(y),
                             qRgb(warm, 0, cool));
        }
    }
    m_differenceView->setPixmap(QPixmap::fromImage(picture).scaled(
        m_differenceView->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
}
