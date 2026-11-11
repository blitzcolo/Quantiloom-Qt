/**
 * @file SdkFrame.cpp
 * @brief The viewport's per-frame SDK call sequence; see SdkFrame.hpp
 */

#include "SdkFrame.hpp"

#include <renderer/ExternalRenderContext.hpp>

namespace viewport {

FrameResult recordSdkFrame(quantiloom::ExternalRenderContext& context,
                           const quantiloom::GpuCommandRecording& commands,
                           const quantiloom::GpuImageTarget& frame,
                           const quantiloom::GpuImageTarget& depth, const FrameRequest& request) {
    // A frame is drawn for two different reasons, and only one of them should
    // cost a sample. Accumulating is what Start/Resume asked for; redrawing
    // because the selection or the gizmo changed is the shell's own business,
    // and charging the user's sample budget for it would make the sample count
    // of a finished render depend on how often they clicked.
    //
    // So past the target, or while paused, the accumulated image is presented
    // again rather than re-traced. PresentAccumulated returns false when there
    // is nothing to re-present -- before the first trace, or after a resize --
    // and then a real frame is the only correct answer. A pending display
    // reprocess re-runs the sensor chain and CLAHE over the unchanged
    // accumulation instead; same refusals, same fallback.
    bool presentedWithoutTracing = false;
    if (!request.accumulating) {
        presentedWithoutTracing = request.reprocess
                                      ? context.ReprocessAccumulated(commands, frame)
                                      : context.PresentAccumulated(commands, frame);
    }

    FrameResult result;
    if (!presentedWithoutTracing) {
        context.SetViewportSampleBatch(request.chooseSampleBatch());
        context.RenderFrame(commands, frame);
        result.traced = true;
        if (!context.IsReady()) {
            // This frame's recording is the one that failed, and nothing
            // reliable is in the frame texture. The caller presents a cleared
            // swapchain image instead; the next frame reports the failure.
            return result;
        }
    }

    context.BlitDepthTo(commands, depth);
    result.haveImage = true;
    return result;
}

}  // namespace viewport
