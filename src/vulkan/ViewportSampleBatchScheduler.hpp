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

    [[nodiscard]] std::uint32_t choose(std::uint32_t remaining,
                                       bool forceOne) const {
        if (forceOne) return 1;
        return std::max<std::uint32_t>(1, std::min(m_batch, remaining));
    }

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
