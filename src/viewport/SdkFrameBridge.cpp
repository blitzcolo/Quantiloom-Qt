/**
 * @file SdkFrameBridge.cpp
 * @brief The viewport's Vulkan-typed SDK calls; see SdkFrameBridge.hpp
 */

#include "SdkFrameBridge.hpp"
#include "GpuDevice.hpp"

#include <renderer/ExternalRenderContext.hpp>

#include <QVulkanInstance>
#include <rhi/qrhi.h>

namespace viewport {

namespace {

void imageBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout,
                  VkImageLayout newLayout, VkPipelineStageFlags srcStage,
                  VkAccessFlags srcAccess, VkPipelineStageFlags dstStage,
                  VkAccessFlags dstAccess) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

/// Before the SDK writes the frame texture. The SDK's own first barrier waits
/// only on its trace stage, with no source access -- right for a swapchain
/// image, which the acquire semaphore orders, and not for this one: the
/// previous frame's composite pass may still be sampling it. So everything
/// submitted before is waited on here. UNDEFINED, because the SDK writes the
/// whole image.
void beforeSdkWrites(VkCommandBuffer cmd, VkImage frame) {
    imageBarrier(cmd, frame, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT,
                 VK_ACCESS_TRANSFER_WRITE_BIT);
}

/// After the SDK wrote the frame texture. It leaves its target in
/// PRESENT_SRC_KHR with an empty destination scope; the overlay pass that
/// follows wants COLOR_ATTACHMENT_OPTIMAL, and QRhi's own barrier there takes
/// its source scope from state it never saw change. So the destination here
/// is everything, and the layout is exactly the one QRhi's next use asks for,
/// which makes QRhi's barrier a same-layout one. ALL_COMMANDS as the source
/// too, so this chains after the SDK's barrier, whose destination stage is
/// BOTTOM_OF_PIPE.
void afterSdkWrites(VkCommandBuffer cmd, VkImage frame) {
    imageBarrier(cmd, frame, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                 VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                 VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
}

}  // namespace

SdkFrameBridge::ContextResult SdkFrameBridge::createContext(QVulkanInstance* instance,
                                                            const GpuDevice& device,
                                                            QSize extent) {
    quantiloom::ExternalRenderContext::InitParams params{};
    params.instance = instance->vkInstance();
    params.physicalDevice = device.physicalDevice;
    params.device = device.device;
    params.graphicsQueue = device.queue;
    params.graphicsQueueFamily = device.queueFamily;
    // The frame texture's format (QRhiTexture::RGBA8 with the sRGB flag). An
    // sRGB target is what makes the SDK's presenting blit encode its linear
    // radiance; the swapchain is never its target.
    params.targetColorFormat = VK_FORMAT_R8G8B8A8_SRGB;
    params.width = static_cast<quantiloom::u32>(extent.width());
    params.height = static_cast<quantiloom::u32>(extent.height());
    // pipelineCacheDir: the SDK's platform default (%LOCALAPPDATA%/Quantiloom/cache/).

    ContextResult out;
    auto created = quantiloom::ExternalRenderContext::Create(params);
    if (!created) {
        out.error = QString::fromStdString(created.error());
        return out;
    }
    out.context = std::move(created.value());
    return out;
}

SdkFrameBridge::FrameResult SdkFrameBridge::record(
    VkCommandBuffer_T* commandBuffer, quantiloom::ExternalRenderContext& context,
    QRhiTexture* frameTexture, QRhiTexture* depthTexture, const QSize extent,
    const FrameRequest& request) {
    const VkCommandBuffer cmd = commandBuffer;
    const auto frame = reinterpret_cast<VkImage>(frameTexture->nativeTexture().object);
    const auto width = static_cast<quantiloom::u32>(extent.width());
    const auto height = static_cast<quantiloom::u32>(extent.height());
    m_depthWritten = false;

    beforeSdkWrites(cmd, frame);

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
        presentedWithoutTracing =
            request.reprocess
                ? context.ReprocessAccumulated(cmd, frame, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                               width, height)
                : context.PresentAccumulated(cmd, frame, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                             width, height);
    }

    FrameResult result;
    if (!presentedWithoutTracing) {
        context.SetViewportSampleBatch(request.chooseSampleBatch());
        context.RenderFrame(cmd, frame, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, width, height);
        result.traced = true;
        if (!context.IsReady()) {
            // This frame's recording is the one that failed, and nothing
            // reliable is in the frame texture. It stays where the barrier
            // above put it, and the caller presents a cleared swapchain image
            // instead; the next frame reports the failure.
            m_frameLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            return result;
        }
    }

    // The SDK's own barriers cover the last frame's fragment reads of the
    // depth texture and hand it to FRAGMENT_SHADER / SHADER_READ, which is
    // where the grid samples it.
    const QRhiTexture::NativeTexture depth = depthTexture->nativeTexture();
    context.BlitDepthTo(cmd, reinterpret_cast<VkImage>(depth.object),
                        static_cast<VkImageLayout>(depth.layout), width, height);
    m_depthWritten = true;
    m_depthLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    afterSdkWrites(cmd, frame);
    m_frameLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    result.haveImage = true;
    return result;
}

void SdkFrameBridge::publishLayouts(QRhiTexture* frameTexture,
                                    QRhiTexture* depthTexture) const {
    frameTexture->setNativeLayout(m_frameLayout);
    if (m_depthWritten) {
        depthTexture->setNativeLayout(m_depthLayout);
    }
}

}  // namespace viewport
