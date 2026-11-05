/**
 * @file SdkFrame.hpp
 * @brief The backend-neutral half of a viewport frame's SDK calls
 *
 * What a frame asks of the SDK -- trace, re-present the accumulation, or
 * re-run the display stage -- does not depend on the GPU API; only the
 * handles passed along do. The Vulkan bridge (SdkFrameBridge) and the Metal
 * backend (GpuBackendMetal.mm) each build their own GpuCommandRecording and
 * GpuImageTarget values and call recordSdkFrame(), so the decision logic
 * exists once.
 */

#pragma once

#include <QString>

#include <cstdint>
#include <functional>
#include <memory>

namespace quantiloom {
class ExternalRenderContext;
struct GpuCommandRecording;
struct GpuImageTarget;
}  // namespace quantiloom

namespace viewport {

struct ContextResult {
    std::unique_ptr<quantiloom::ExternalRenderContext> context;
    QString error;
};

struct FrameRequest {
    /// False past the target or while paused: the accumulated image is shown
    /// again rather than re-traced, if there is one to show.
    bool accumulating = false;
    /// A display-stage setting changed: re-run post-processing over the
    /// unchanged accumulation instead of re-presenting it.
    bool reprocess = false;
    /// The sample batch for a traced frame; asked only when one is traced.
    std::function<std::uint32_t()> chooseSampleBatch;
};

struct FrameResult {
    /// The frame texture holds this frame's picture and the depth texture
    /// its primary-hit depth.
    bool haveImage = false;
    /// RenderFrame ran, so a sample may have been added.
    bool traced = false;
};

/// Record this frame's SDK work: the frame image into @p frame and, when the
/// image is good, the primary-hit depth into @p depth.
FrameResult recordSdkFrame(quantiloom::ExternalRenderContext& context,
                           const quantiloom::GpuCommandRecording& commands,
                           const quantiloom::GpuImageTarget& frame,
                           const quantiloom::GpuImageTarget& depth, const FrameRequest& request);

}  // namespace viewport
