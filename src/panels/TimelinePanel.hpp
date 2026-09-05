/**
 * @file TimelinePanel.hpp
 * @brief Where the clock stands, and the transport that moves it
 *
 * A scene that declares a `[timeline]` gets a clock: seconds are what
 * everything is stated in, and a tick is the frame of the grid those seconds
 * are sampled on. This panel is the transport for it and nothing else -- it
 * owns no SDK call, reads no TOML key, and computes no time itself beyond
 * turning a tick into the second it stands for.
 *
 * Everything it displays comes from `TimelineInfo`, which the SDK resolved: the
 * span, the tick rate, the mapped thermal hour and how many geometry epochs the
 * solve was cut into. A panel that worked any of that out for itself would be
 * a second reading of the config, which is the divergence this project keeps
 * closing.
 *
 * ## Two ways to play
 *
 * Real time steps on a timer, which is what a person wants while looking for
 * the moment something happens. Step when converged waits for the render to
 * reach its sample target before advancing -- the only honest way to watch a
 * path-traced sequence, since a frame that has not converged is mostly noise
 * and the noise is what the eye follows.
 *
 * The playback speed multiplier exists because a timeline can span a month.
 * Real time at twenty ticks a second would take a fortnight to watch, so ×100
 * is not a novelty setting.
 */

#pragma once

#include "../ui/PanelBase.hpp"

#include <renderer/TimelineControl.hpp>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QSlider;
class QSpinBox;
class QTimer;
class QToolButton;
QT_END_NAMESPACE

class TimelinePanel : public PanelBase {
    Q_OBJECT

public:
    explicit TimelinePanel(QWidget* parent = nullptr);

    [[nodiscard]] QString panelTitle() const override;
    [[nodiscard]] QString panelId() const override { return QStringLiteral("timeline"); }
    void retranslateUi() override;

    /// What the SDK says the clock is. A document with no clock disables the
    /// whole panel rather than showing a transport over invented numbers.
    void setInfo(const quantiloom::TimelineInfo& info);

    /// The render reached its sample target. Advances one tick in
    /// "step when converged" mode, and is ignored otherwise.
    void notifyConverged();

    [[nodiscard]] bool isPlaying() const { return m_playing; }
    [[nodiscard]] bool hasTimeline() const { return m_info.present; }

public slots:
    /// The transport, as the &Timeline menu drives it. Public because the menu
    /// bar is the complete catalogue: every one of these has a menu entry with
    /// a shortcut, and both routes end at the same slot.
    void goToStart();
    void goToEnd();
    void stepBack();
    void stepForward();
    void togglePlay();
    void setPlaying(bool playing);
    void setLoop(bool loop);

signals:
    /// Where the clock should stand now. The only thing this panel asks for.
    void timelineTimeChanged(double time_s);
    void playbackStateChanged(bool playing);

private slots:
    void onSliderMoved(int tick);
    void onTickSpinChanged(int tick);
    void onSecondsSpinChanged(double seconds);
    void onSpeedChanged();
    void onTimerTick();

private:
    void setupUi();
    void updateReadout();
    void restartTimer();
    /// Move to @p tick, clamping or wrapping by the loop setting, and tell
    /// whoever is listening. Every route into this panel ends here.
    void requestTick(long long tick);
    [[nodiscard]] double playbackSpeed() const;
    /// `d hh:mm:ss.mmm` once a timeline is longer than an hour, plain seconds
    /// below that. A month-long clock reading "1209600.000 s" is a number
    /// nobody can place.
    [[nodiscard]] QString formatSeconds(double seconds) const;

    quantiloom::TimelineInfo m_info{};
    bool m_playing = false;
    bool m_suppressSignals = false;
    long long m_tick = 0;

    QTimer* m_timer = nullptr;

    QToolButton* m_toStartButton = nullptr;
    QToolButton* m_stepBackButton = nullptr;
    QToolButton* m_playButton = nullptr;
    QToolButton* m_stepForwardButton = nullptr;
    QToolButton* m_toEndButton = nullptr;

    QSlider* m_slider = nullptr;
    QSpinBox* m_tickSpin = nullptr;
    QDoubleSpinBox* m_secondsSpin = nullptr;

    QGroupBox* m_transportGroup = nullptr;
    QGroupBox* m_playbackGroup = nullptr;
    QLabel* m_tickCaption = nullptr;
    QLabel* m_secondsCaption = nullptr;
    QLabel* m_modeCaption = nullptr;
    QLabel* m_speedCaption = nullptr;
    QComboBox* m_modeCombo = nullptr;
    QComboBox* m_speedCombo = nullptr;
    QCheckBox* m_loopCheck = nullptr;

    QLabel* m_readout = nullptr;
    QLabel* m_thermalReadout = nullptr;
    QLabel* m_hint = nullptr;
};
