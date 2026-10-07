/**
 * @file ViewportSampleBatchScheduler.hpp
 * @brief Per-frame sample-count control for the interactive viewport
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace vkview {

/**
 * Keeps one viewport submission near an interactive GPU-time budget.
 *
 * A reset discards timings that may still arrive from the previous image.
 * Growth is capped at 2x per observation; a slower latest sample reduces the
 * batch immediately.
 */
class ViewportSampleBatchScheduler {
public:
    static constexpr float kBudgetMs = 8.0f;
    static constexpr std::uint32_t kMaxBatch = 16;

    /// Start over after an accumulation reset. The warm-up counts
    /// concurrentFrames + 1 callbacks because timings still in flight when
    /// this runs describe the previous image, not the one being scheduled.
    void reset(int concurrentFrames) {
        m_batch = 1;
        m_smoothedMs = 0.0f;
        m_warmupCallbacks = static_cast<std::uint32_t>(
            std::max(concurrentFrames + 1, 1));
    }

    void observe(float gpuMsPerSample) {
        if (m_warmupCallbacks > 0) {
            --m_warmupCallbacks;
            return;
        }
        if (!(gpuMsPerSample > 0.0f) || !std::isfinite(gpuMsPerSample)) return;

        if (m_smoothedMs == 0.0f) {
            m_smoothedMs = gpuMsPerSample;
        } else {
            constexpr float kNewestWeight = 0.25f;
            m_smoothedMs = (1.0f - kNewestWeight) * m_smoothedMs +
                           kNewestWeight * gpuMsPerSample;
        }
        const float conservativeMs = std::max(gpuMsPerSample, m_smoothedMs);
        const auto fitsBudget = static_cast<std::uint32_t>(
            std::clamp(std::floor(kBudgetMs / conservativeMs), 1.0f,
                       static_cast<float>(kMaxBatch)));
        if (fitsBudget > m_batch) {
            m_batch = std::min(fitsBudget, m_batch * 2);
        } else {
            m_batch = fitsBudget;
        }
    }

    /// The batch for one frame: the learned size, clamped to what is still
    /// owed toward the target -- or exactly one when forceOne says a batch
    /// would be wrong rather than merely large.
    [[nodiscard]] std::uint32_t choose(std::uint32_t remaining,
                                       bool forceOne) const {
        if (forceOne) return 1;
        return std::max<std::uint32_t>(1, std::min(m_batch, remaining));
    }

    /// choose() for a render loop's state. A paused fallback trace still
    /// costs exactly one sample, motion favours latency over throughput, and
    /// a physical camera's one trace is one acquisition with its own
    /// exposure/history/RNG semantics -- all three force a batch of one
    /// rather than let several samples share a frame they do not describe.
    [[nodiscard]] std::uint32_t chooseForFrame(
        std::uint32_t accumulated, std::uint32_t target, bool accumulating,
        bool motionActive, bool physicalCamera) const {
        const std::uint32_t remaining = target > 0
            ? target - std::min(accumulated, target)
            : std::numeric_limits<std::uint32_t>::max();
        return choose(remaining, !accumulating || motionActive || physicalCamera);
    }

    [[nodiscard]] std::uint32_t recommended() const { return m_batch; }
    [[nodiscard]] std::uint32_t warmupCallbacks() const { return m_warmupCallbacks; }

private:
    std::uint32_t m_batch = 1;
    std::uint32_t m_warmupCallbacks = 1;
    float m_smoothedMs = 0.0f;
};

} // namespace vkview
