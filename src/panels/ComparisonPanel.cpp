/**
 * @file ComparisonPanel.cpp
 * @brief Implementation of the measured-comparison panel
 *
 * @author blitzcolo
 */

#include "ComparisonPanel.hpp"

#include "../ui/UiStyle.hpp"

#include <io/ImageIO.hpp>

#include <QApplication>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLocale>
#include <QResizeEvent>
#include <QSignalBlocker>
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
    setObjectName(panelId());
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    m_loadButton = new QPushButton(this);
    connect(m_loadButton, &QPushButton::clicked, this, &ComparisonPanel::loadReference);
    layout->addWidget(m_loadButton);

    m_referenceCaption = new QLabel(this);
    m_referenceCaption->setWordWrap(true);
    bindStyle([this] { uistyle::applyHintStyle(m_referenceCaption); });
    layout->addWidget(m_referenceCaption);

    auto* channelRow = new QFormLayout();
    m_channelCaption = new QLabel(this);
    m_channelCombo = new QComboBox(this);
    connect(m_channelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { updateReport(); });
    channelRow->addRow(m_channelCaption, m_channelCombo);
    layout->addLayout(channelRow);

    m_compareButton = new QPushButton(this);
    connect(m_compareButton, &QPushButton::clicked, this, &ComparisonPanel::compare);
    layout->addWidget(m_compareButton);

    // A message when there is nothing to report, the figures when there is.
    m_report = new QLabel(this);
    m_report->setWordWrap(true);
    m_report->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_report);

    m_figures = new QWidget(this);
    auto* figures = new QFormLayout(m_figures);
    figures->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < kFigureCount; ++i) {
        m_figureCaptions[i] = new QLabel(m_figures);
        m_figureValues[i] = new QLabel(m_figures);
        m_figureValues[i]->setTextInteractionFlags(Qt::TextSelectableByMouse);
        figures->addRow(m_figureCaptions[i], m_figureValues[i]);
    }
    m_figures->setVisible(false);
    layout->addWidget(m_figures);

    // Ignored, so the label can shrink with the dock: a label holding a
    // pixmap otherwise asks for at least the pixmap's size, and the dock
    // could grow but never come back.
    m_differenceView = new QLabel(this);
    m_differenceView->setMinimumHeight(160);
    m_differenceView->setAlignment(Qt::AlignCenter);
    m_differenceView->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    layout->addWidget(m_differenceView, 1);

    m_legend = new QLabel(this);
    m_legend->setWordWrap(true);
    m_legend->setVisible(false);
    bindStyle([this] { uistyle::applyHintStyle(m_legend); });
    layout->addWidget(m_legend);

    retranslateUi();
    updateEnabled();
}

ComparisonPanel::~ComparisonPanel() = default;

QString ComparisonPanel::panelTitle() const {
    return tr("Comparison");
}

void ComparisonPanel::retranslateUi() {
    m_loadButton->setText(tr("Load Reference..."));
    m_loadButton->setToolTip(tr("An EXR of a measurement, or of another render."));
    m_channelCaption->setText(tr("Channel:"));
    m_compareButton->setText(tr("Compare with Current Frame"));
    m_compareButton->setToolTip(
        tr("Read the frame on screen now. A path-traced frame is still converging, "
           "so compare once it has the samples you want."));
    m_legend->setText(tr("Red: the render is brighter. Blue: it is darker. Black: they "
                         "agree. Scaled to the 99th percentile of the difference."));
    m_differenceView->setToolTip(m_legend->text());
    const QStringList figureNames = {tr("Reference mean:"), tr("Bias:"), tr("RMSE:"),
                                     tr("95th percentile:"), tr("Worst pixel:")};
    for (int i = 0; i < kFigureCount; ++i) {
        m_figureCaptions[i]->setText(figureNames.at(i));
    }
    m_figureCaptions[1]->setToolTip(
        tr("The render minus the reference, so positive means brighter."));

    // The channel names a file gave are its own; only the fallback is ours.
    for (int i = 0; i < m_channelCombo->count(); ++i) {
        if (m_channelCombo->itemData(i, Qt::UserRole + 1).toBool()) {
            m_channelCombo->setItemText(i, tr("Channel %1").arg(i + 1));
        }
    }
    updateReferenceCaption();
    updateReport();
}

void ComparisonPanel::updateEnabled() {
    const bool loaded = m_reference != nullptr;
    m_channelCombo->setEnabled(loaded);
    m_channelCaption->setEnabled(loaded);
    m_compareButton->setEnabled(loaded);
}

void ComparisonPanel::updateReferenceCaption() {
    if (!m_loadFailed.isEmpty()) {
        m_referenceCaption->setText(tr("Could not read %1.").arg(m_loadFailed));
    } else if (!m_reference) {
        m_referenceCaption->setText(
            tr("No reference loaded. An EXR of a measurement, or of another render."));
    } else {
        m_referenceCaption->setText(
            tr("%1: %2 × %3, %n channel(s).", "", static_cast<int>(m_reference->channels))
                .arg(QFileInfo(m_referencePath).fileName())
                .arg(m_reference->width)
                .arg(m_reference->height));
    }
}

void ComparisonPanel::loadReference() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Load Reference Image"),
        m_referencePath.isEmpty() ? QString() : QFileInfo(m_referencePath).absolutePath(),
        tr("OpenEXR (*.exr);;All files (*)"));
    if (path.isEmpty()) return;

    // A large EXR takes a moment on the GUI thread; say so.
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto loaded = quantiloom::ImageIO::ReadEXR(path.toStdString());
    QApplication::restoreOverrideCursor();
    if (!loaded.has_value()) {
        m_loadFailed = QFileInfo(path).fileName();
        updateReferenceCaption();
        return;
    }

    m_loadFailed.clear();
    m_reference = std::make_unique<quantiloom::Image>(std::move(loaded.value()));
    m_referencePath = path;

    // Channels by the name the file gave them, since a comparison against a
    // measurement is usually about one band rather than about a colour.
    {
        // One report at the end, not one per item added.
        const QSignalBlocker blocker(m_channelCombo);
        m_channelCombo->clear();
        for (quantiloom::u32 c = 0; c < m_reference->channels; ++c) {
            const bool named = c < m_reference->channelNames.size() &&
                               !m_reference->channelNames[c].empty();
            m_channelCombo->addItem(named ? QString::fromStdString(m_reference->channelNames[c])
                                          : tr("Channel %1").arg(c + 1),
                                    c);
            m_channelCombo->setItemData(m_channelCombo->count() - 1, !named, Qt::UserRole + 1);
        }
    }

    updateReferenceCaption();
    updateEnabled();
    updateReport();
}

void ComparisonPanel::compare() {
    if (!m_reference) return;
    // The window owns the swapchain, so it is what can read a frame back.
    emit frameRequested();
}

void ComparisonPanel::setRenderedImage(std::unique_ptr<quantiloom::Image> image) {
    m_rendered = std::move(image);
    updateReport();
}

void ComparisonPanel::showMessage(const QString& message) {
    m_report->setText(message);
    m_report->setVisible(true);
    m_figures->setVisible(false);
    m_differenceView->clear();
    m_differenceImage = QImage();
    m_legend->setVisible(false);
}

void ComparisonPanel::updateReport() {
    if (!m_reference) {
        showMessage(tr("Load a reference, then compare."));
        return;
    }
    if (!m_rendered) {
        showMessage(tr("Reference loaded. Compare to read the current frame."));
        return;
    }
    if (m_rendered->width != m_reference->width ||
        m_rendered->height != m_reference->height) {
        // Not resampled on the user's behalf: which resampling, and what it
        // does to a radiance, is a decision about the measurement rather than
        // a convenience, and guessing it would put an interpolation inside a
        // number someone is going to quote.
        showMessage(tr("The frame is %1 × %2 and the reference is %3 × %4. "
                       "Match the render resolution to the reference rather than "
                       "resampling either -- what a resampling does to a radiance "
                       "belongs to whoever made the measurement.")
                        .arg(m_rendered->width)
                        .arg(m_rendered->height)
                        .arg(m_reference->width)
                        .arg(m_reference->height));
        return;
    }

    const auto channel = static_cast<quantiloom::u32>(
        m_channelCombo->currentData().isValid() ? m_channelCombo->currentData().toUInt() : 0u);
    // The same channel in both, or no comparison: silently comparing the
    // reference's band 5 against the frame's last channel was a number about
    // two different things.
    if (channel >= m_rendered->channels) {
        showMessage(tr("The frame has %n channel(s), so %1 has no counterpart in it.", "",
                       static_cast<int>(m_rendered->channels))
                        .arg(m_channelCombo->currentText()));
        return;
    }
    const auto reference = ChannelOf(*m_reference, channel);
    const auto rendered = ChannelOf(*m_rendered, channel);
    if (reference.empty() || rendered.size() != reference.size()) {
        showMessage(tr("That channel is not in both images."));
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
    const auto p95At = absolute.begin() + static_cast<long>(0.95 * (n - 1.0));
    std::nth_element(absolute.begin(), p95At, absolute.end());
    const double p95 = *p95At;

    m_report->setText(tr("Against %1, channel %2:")
                          .arg(QFileInfo(m_referencePath).fileName())
                          .arg(m_channelCombo->currentText()));
    m_report->setVisible(true);
    const QLocale locale;
    m_figureValues[0]->setText(locale.toString(referenceMean, 'g', 6));
    m_figureValues[1]->setText(
        tr("%1 (%2%)")
            .arg(locale.toString(bias, 'g', 4))
            .arg(locale.toString(referenceMean != 0.0 ? 100.0 * bias / referenceMean : 0.0,
                                 'f', 2)));
    m_figureValues[2]->setText(locale.toString(rmse, 'g', 4));
    m_figureValues[3]->setText(locale.toString(p95, 'g', 4));
    m_figureValues[4]->setText(locale.toString(worst, 'g', 4));
    m_figures->setVisible(true);

    showDifference(reference, rendered);
}

void ComparisonPanel::showDifference(const std::vector<double>& reference,
                                     const std::vector<double>& rendered) {
    // The caller has already extracted the channel from both images and checked
    // that the two agree in size and are not empty.
    if (!m_reference || !m_rendered) return;

    // Signed, and scaled by the 99th percentile rather than the maximum: one
    // firefly would otherwise set the scale and the rest of the frame would
    // come out flat grey.
    std::vector<double> magnitudes;
    magnitudes.reserve(reference.size());
    for (size_t i = 0; i < reference.size(); ++i) {
        magnitudes.push_back(std::abs(rendered[i] - reference[i]));
    }
    const auto at = magnitudes.begin() +
                    static_cast<long>(0.99 * static_cast<double>(magnitudes.size() - 1));
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
    m_differenceImage = picture;
    m_legend->setVisible(true);
    rescaleDifference();
}

void ComparisonPanel::rescaleDifference() {
    if (m_differenceImage.isNull()) return;
    m_differenceView->setPixmap(QPixmap::fromImage(m_differenceImage).scaled(
        m_differenceView->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
}

void ComparisonPanel::resizeEvent(QResizeEvent* event) {
    PanelBase::resizeEvent(event);
    // Scaled to the label once used to be all: the image stayed the size the
    // dock happened to be when Compare was pressed.
    rescaleDifference();
}
