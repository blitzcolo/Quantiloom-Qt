/**
 * @file TimelinePanel.cpp
 * @brief Where the clock stands, and the transport that moves it
 */

#include "TimelinePanel.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

/// Below this the readout is plain seconds; above it, `d hh:mm:ss.mmm`. An hour
/// is where "3612.500 s" stops being a number anyone can place.
constexpr double kClockFormatThreshold_s = 3600.0;

/// A timer any faster than this is a timer the event loop cannot honour, and
/// pretending otherwise makes playback claim a rate it is not achieving.
constexpr int kMinimumTimerInterval_ms = 16;

}  // namespace

TimelinePanel::TimelinePanel(QWidget* parent) : PanelBase(parent) {
    setupUi();
}

QString TimelinePanel::panelTitle() const {
    return tr("Timeline");
}

void TimelinePanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &TimelinePanel::onTimerTick);

    // ------------------------------------------------------------------
    // Transport
    // ------------------------------------------------------------------
    m_transportGroup = new QGroupBox(this);
    auto* transportLayout = new QVBoxLayout(m_transportGroup);

    auto* buttonRow = new QHBoxLayout();
    const auto makeButton = [this, buttonRow](QStyle::StandardPixmap icon) {
        auto* button = new QToolButton(m_transportGroup);
        button->setIcon(style()->standardIcon(icon));
        button->setAutoRaise(true);
        buttonRow->addWidget(button);
        return button;
    };

    m_toStartButton = makeButton(QStyle::SP_MediaSkipBackward);
    m_stepBackButton = makeButton(QStyle::SP_MediaSeekBackward);
    m_playButton = makeButton(QStyle::SP_MediaPlay);
    m_stepForwardButton = makeButton(QStyle::SP_MediaSeekForward);
    m_toEndButton = makeButton(QStyle::SP_MediaSkipForward);
    buttonRow->addStretch(1);

    connect(m_toStartButton, &QToolButton::clicked, this, &TimelinePanel::goToStart);
    connect(m_stepBackButton, &QToolButton::clicked, this, &TimelinePanel::stepBack);
    connect(m_playButton, &QToolButton::clicked, this, &TimelinePanel::togglePlay);
    connect(m_stepForwardButton, &QToolButton::clicked, this, &TimelinePanel::stepForward);
    connect(m_toEndButton, &QToolButton::clicked, this, &TimelinePanel::goToEnd);

    transportLayout->addLayout(buttonRow);

    m_slider = new QSlider(Qt::Horizontal, m_transportGroup);
    m_slider->setRange(0, 0);
    connect(m_slider, &QSlider::valueChanged, this, &TimelinePanel::onSliderMoved);
    transportLayout->addWidget(m_slider);

    auto* spinForm = new QFormLayout();
    // The spin boxes seek on commit; a typed 120 would otherwise seek to 1,
    // 12 and 120, and a seek can cross a thermal epoch. The slider stays
    // live -- scrubbing is what it is for.
    m_tickSpin = new QSpinBox(m_transportGroup);
    m_tickSpin->setRange(0, 0);
    m_tickSpin->setKeyboardTracking(false);
    connect(m_tickSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &TimelinePanel::onTickSpinChanged);
    m_tickCaption = new QLabel(m_transportGroup);
    spinForm->addRow(m_tickCaption, m_tickSpin);

    m_secondsSpin = new QDoubleSpinBox(m_transportGroup);
    m_secondsSpin->setKeyboardTracking(false);
    m_secondsSpin->setDecimals(3);
    m_secondsSpin->setRange(0.0, 0.0);
    m_secondsSpin->setSuffix(QStringLiteral(" s"));
    connect(m_secondsSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &TimelinePanel::onSecondsSpinChanged);
    m_secondsCaption = new QLabel(m_transportGroup);
    spinForm->addRow(m_secondsCaption, m_secondsSpin);

    transportLayout->addLayout(spinForm);
    mainLayout->addWidget(m_transportGroup);
    m_createButton = new QPushButton(this);
    connect(m_createButton, &QPushButton::clicked,
            this, &TimelinePanel::createTimelineRequested);
    mainLayout->addWidget(m_createButton);

    // ------------------------------------------------------------------
    // Playback
    // ------------------------------------------------------------------
    m_playbackGroup = new QGroupBox(this);
    auto* playbackForm = new QFormLayout(m_playbackGroup);

    m_modeCombo = new QComboBox(m_playbackGroup);
    m_modeCombo->addItem(QString());  // real time
    m_modeCombo->addItem(QString());  // step when converged
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { restartTimer(); });
    m_modeCaption = new QLabel(m_playbackGroup);
    playbackForm->addRow(m_modeCaption, m_modeCombo);

    m_speedCombo = new QComboBox(m_playbackGroup);
    for (const double speed : {0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 100.0}) {
        m_speedCombo->addItem(QString(), speed);   // text set in retranslateUi()
    }
    m_speedCombo->setCurrentIndex(2);  // x1
    connect(m_speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { onSpeedChanged(); });
    m_speedCaption = new QLabel(m_playbackGroup);
    playbackForm->addRow(m_speedCaption, m_speedCombo);

    m_loopCheck = new QCheckBox(m_playbackGroup);
    // Read when playback wraps; reported so the menu's tick follows the box.
    connect(m_loopCheck, &QCheckBox::toggled, this, &TimelinePanel::loopChanged);
    playbackForm->addRow(QString(), m_loopCheck);

    mainLayout->addWidget(m_playbackGroup);

    // ------------------------------------------------------------------
    // Readout
    // ------------------------------------------------------------------
    m_readout = new QLabel(this);
    m_readout->setWordWrap(true);
    m_readout->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mainLayout->addWidget(m_readout);

    m_thermalReadout = new QLabel(this);
    m_thermalReadout->setWordWrap(true);
    m_thermalReadout->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mainLayout->addWidget(m_thermalReadout);

    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    bindStyle([this] { uistyle::applyHintStyle(m_hint); });
    mainLayout->addWidget(m_hint);

    mainLayout->addStretch(1);

    // The icons come from the style, and a theme switch can change the style.
    bindStyle([this] {
        m_toStartButton->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));
        m_stepBackButton->setIcon(style()->standardIcon(QStyle::SP_MediaSeekBackward));
        m_stepForwardButton->setIcon(style()->standardIcon(QStyle::SP_MediaSeekForward));
        m_toEndButton->setIcon(style()->standardIcon(QStyle::SP_MediaSkipForward));
        updatePlayButton();
    });

    setInfo({});
    retranslateUi();
}

void TimelinePanel::retranslateUi() {
    // Everything here is set directly rather than through bindText(): this
    // runs on every language change, and bindText() would register another
    // copy of the setter each time.
    m_transportGroup->setTitle(tr("Transport"));
    m_createButton->setText(tr("Create 0-10 s timeline (20 ticks/s)"));
    m_playbackGroup->setTitle(tr("Playback"));
    m_tickCaption->setText(tr("Tick:"));
    m_secondsCaption->setText(tr("Time:"));
    m_modeCaption->setText(tr("Mode:"));
    m_speedCaption->setText(tr("Speed:"));
    m_loopCheck->setText(tr("Loop"));

    m_toStartButton->setToolTip(tr("Go to start (Alt+Home)"));
    m_stepBackButton->setToolTip(tr("Previous tick (Alt+Left)"));
    updatePlayButton();
    m_stepForwardButton->setToolTip(tr("Next tick (Alt+Right)"));
    m_toEndButton->setToolTip(tr("Go to end (Alt+End)"));

    const int mode = m_modeCombo->currentIndex();
    m_modeCombo->setItemText(0, tr("Real time"));
    m_modeCombo->setItemText(1, tr("Step when converged"));
    m_modeCombo->setCurrentIndex(mode);
    m_modeCombo->setToolTip(
        tr("Real time advances on a timer. Step when converged waits for each "
           "frame to reach its sample target first, which is the only way to "
           "watch a path-traced sequence without watching the noise."));

    for (int i = 0; i < m_speedCombo->count(); ++i) {
        m_speedCombo->setItemText(
            i, tr("×%1").arg(locale().toString(m_speedCombo->itemData(i).toDouble())));
    }
    m_speedCombo->setToolTip(
        tr("Multiplies the tick rate during playback. A timeline that spans a "
           "month is not watchable at one tick per twentieth of a second. Each "
           "tick still has to be rendered, so a high multiplier runs as fast as "
           "the frames allow, not faster."));

    PanelBase::retranslateUi();
    updateHint();
    updateReadout();
}

void TimelinePanel::updateHint() {
    m_hint->setText(
        m_info.present
            ? tr("Moving the clock is not an edit: it does not enter the undo "
                 "stack and does not mark the document modified. Saving records "
                 "where it stands.")
            : tr("This scene declares no [timeline]. Add one, with [[models]] "
                 "carrying a motion, to give it a clock."));
}

void TimelinePanel::updatePlayButton() {
    m_playButton->setIcon(
        style()->standardIcon(m_playing ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
    m_playButton->setToolTip(m_playing ? tr("Pause (Alt+P)") : tr("Play (Alt+P)"));
}

// ============================================================================
// State from the SDK
// ============================================================================

void TimelinePanel::setInfo(const quantiloom::TimelineInfo& info) {
    m_info = info;

    const bool enabled = info.present;
    m_transportGroup->setEnabled(enabled);
    m_createButton->setVisible(!enabled);
    m_playbackGroup->setEnabled(enabled);
    if (!enabled && m_playing) {
        setPlaying(false);
    }

    const long long lastTick = enabled ? std::max<long long>(info.TickCount() - 1, 0) : 0;
    m_tick = enabled ? std::clamp<long long>(info.TickOf(info.current_s), 0, lastTick) : 0;

    m_suppressSignals = true;
    m_slider->setRange(0, static_cast<int>(lastTick));
    m_slider->setValue(static_cast<int>(m_tick));
    m_tickSpin->setRange(0, static_cast<int>(lastTick));
    m_tickSpin->setValue(static_cast<int>(m_tick));
    m_secondsSpin->setRange(info.start_s, std::max(info.end_s, info.start_s));
    m_secondsSpin->setSingleStep(info.ticksPerSecond > 0.0 ? 1.0 / info.ticksPerSecond : 0.05);
    m_secondsSpin->setValue(info.current_s);
    m_suppressSignals = false;

    restartTimer();
    updateReadout();
    // The hint says different things with and without a clock.
    updateHint();
}

void TimelinePanel::updateReadout() {
    if (!m_readout) {
        return;
    }
    // Set directly: this runs on every tick, and is re-run by retranslateUi(),
    // so a bindText() here would pile up one setter per tick.
    if (!m_info.present) {
        m_readout->setText(tr("No timeline"));
        m_thermalReadout->clear();
        return;
    }

    const double seconds = m_info.TimeOfTick(m_tick);
    const long long lastTick = std::max<long long>(m_info.TickCount() - 1, 0);
    const QString time = formatSeconds(seconds);
    const int animated = static_cast<int>(m_info.animatedNodeCount);
    const int models = static_cast<int>(m_info.modelCount);

    m_readout->setText(tr("Tick %1 / %2 · %3").arg(m_tick).arg(lastTick).arg(time) +
                       QStringLiteral("\n") +
                       tr("%n animated node(s)", "", animated) + tr(", ") +
                       tr("%n model(s)", "", models));

    if (m_info.thermalMapped) {
        const double hour = m_info.currentThermalHour;
        const int epoch = static_cast<int>(m_info.currentThermalEpoch) + 1;
        const int epochs = static_cast<int>(std::max<quantiloom::u32>(m_info.thermalEpochCount, 1));
        const double scale = m_info.thermalTimeScale;
        m_thermalReadout->setText(
            tr("Thermal hour %1 h · geometry epoch %2 / %3 · %4 simulated s per clock s")
                .arg(hour, 0, 'f', 3)
                .arg(epoch)
                .arg(epochs)
                .arg(scale, 0, 'g', 4));
    } else {
        m_thermalReadout->clear();
    }
}

QString TimelinePanel::formatSeconds(double seconds) const {
    const double span = m_info.end_s - m_info.start_s;
    if (span < kClockFormatThreshold_s) {
        return tr("%1 s").arg(seconds, 0, 'f', 3);
    }

    const bool negative = seconds < 0.0;
    double rest = std::abs(seconds);
    const long long days = static_cast<long long>(rest / 86400.0);
    rest -= static_cast<double>(days) * 86400.0;
    const int hours = static_cast<int>(rest / 3600.0);
    rest -= hours * 3600.0;
    const int minutes = static_cast<int>(rest / 60.0);
    rest -= minutes * 60.0;

    const QString clock = QStringLiteral("%1:%2:%3")
                              .arg(hours, 2, 10, QLatin1Char('0'))
                              .arg(minutes, 2, 10, QLatin1Char('0'))
                              .arg(rest, 6, 'f', 3, QLatin1Char('0'));
    const QString body = (days > 0) ? tr("%1 d %2").arg(days).arg(clock) : clock;
    return negative ? (QLatin1String("-") + body) : body;
}

// ============================================================================
// The transport
// ============================================================================

void TimelinePanel::requestTick(long long tick) {
    if (!m_info.present) {
        return;
    }
    const long long lastTick = std::max<long long>(m_info.TickCount() - 1, 0);
    if (lastTick <= 0) {
        tick = 0;
    } else if (m_loopCheck->isChecked()) {
        tick = ((tick % (lastTick + 1)) + (lastTick + 1)) % (lastTick + 1);
    } else {
        tick = std::clamp<long long>(tick, 0, lastTick);
        // Running off the end with looping off is where playback stops. A
        // transport that silently kept firing at the last frame would look
        // like a hang.
        if (m_playing && tick == lastTick && m_tick == lastTick) {
            setPlaying(false);
        }
    }

    if (tick == m_tick) {
        return;
    }
    m_tick = tick;

    m_suppressSignals = true;
    m_slider->setValue(static_cast<int>(m_tick));
    m_tickSpin->setValue(static_cast<int>(m_tick));
    m_secondsSpin->setValue(m_info.TimeOfTick(m_tick));
    m_suppressSignals = false;

    updateReadout();
    emit timelineTimeChanged(m_info.TimeOfTick(m_tick));
}

void TimelinePanel::goToStart() { requestTick(0); }

void TimelinePanel::goToEnd() {
    requestTick(std::max<long long>(m_info.TickCount() - 1, 0));
}

void TimelinePanel::stepBack() { requestTick(m_tick - 1); }

void TimelinePanel::stepForward() { requestTick(m_tick + 1); }

void TimelinePanel::togglePlay() { setPlaying(!m_playing); }

void TimelinePanel::setPlaying(bool playing) {
    if (!m_info.present) {
        playing = false;
    }
    if (playing == m_playing) {
        return;
    }
    // Play from the last tick with Loop off would stop on the first timer
    // tick and look as if nothing had happened; it starts over instead.
    if (playing && !m_loopCheck->isChecked() &&
        m_tick >= std::max<long long>(m_info.TickCount() - 1, 0)) {
        requestTick(0);
    }
    m_playing = playing;

    updatePlayButton();

    restartTimer();
    emit playbackStateChanged(m_playing);
}

void TimelinePanel::setLoop(bool loop) { m_loopCheck->setChecked(loop); }

double TimelinePanel::playbackSpeed() const {
    bool ok = false;
    const double speed = m_speedCombo->currentData().toDouble(&ok);
    return (ok && speed > 0.0) ? speed : 1.0;
}

void TimelinePanel::restartTimer() {
    m_timer->stop();
    // Only the real-time mode is on a clock. Step-when-converged is driven by
    // notifyConverged, which is the render telling us it is done.
    if (!m_playing || !m_info.present || m_modeCombo->currentIndex() != 0) {
        return;
    }
    const double rate = m_info.ticksPerSecond * playbackSpeed();
    const int interval = (rate > 0.0)
                             ? std::max(kMinimumTimerInterval_ms,
                                        static_cast<int>(std::lround(1000.0 / rate)))
                             : kMinimumTimerInterval_ms;
    m_timer->start(interval);
}

void TimelinePanel::onTimerTick() { stepForward(); }

void TimelinePanel::notifyConverged() {
    if (m_playing && m_info.present && m_modeCombo->currentIndex() == 1) {
        stepForward();
    }
}

void TimelinePanel::onSpeedChanged() { restartTimer(); }

void TimelinePanel::onSliderMoved(int tick) {
    if (m_suppressSignals) {
        return;
    }
    requestTick(tick);
}

void TimelinePanel::onTickSpinChanged(int tick) {
    if (m_suppressSignals) {
        return;
    }
    requestTick(tick);
}

void TimelinePanel::onSecondsSpinChanged(double seconds) {
    if (m_suppressSignals || !m_info.present) {
        return;
    }
    requestTick(m_info.TickOf(seconds));
}
