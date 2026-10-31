/**
 * @file SdkFrameBridge.cpp
 * @brief The viewport's Vulkan-typed SDK calls; see SdkFrameBridge.hpp
 */

#include "SdkFrameBridge.hpp"
#include "GpuDevice.hpp"

#include <renderer/ExternalRenderContext.hpp>
#include <renderer/interop/VulkanInterop.hpp>

#include <QVulkanInstance>
#include <rhi/qrhi.h>

namespace viewport {

SdkFrameBridge::ContextResult SdkFrameBridge::createContext(QVulkanInstance* instance,
                                                            const GpuDevice& device,
                                                            QSize extent) {
    quantiloom::ExternalRenderContext::InitParams params{};
    quantiloom::vk::Device sdkDevice;
    sdkDevice.physicalDevice = device.physicalDevice;
    sdkDevice.device = device.device;
    sdkDevice.queueFamily = device.queueFamily;
    sdkDevice.queueIndex = device.queueIndex;
    sdkDevice.queue = device.queue;
    sdkDevice.instance = instance->vkInstance();
    params.device = quantiloom::vk::Handles(
        sdkDevice, instance->vkInstance(),
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            instance->getInstanceProcAddr("vkGetInstanceProcAddr")));
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

    // The target descriptors state what the SDK's barriers need to know: the
    // layout QRhi last recorded, and the one QRhi's next use asks for. The
    // SDK waits on everything submitted before and leaves a same-scope
    // barrier behind (VulkanInterop.hpp), so the bridge adds none.
    using quantiloom::GpuTargetFormat;
    const auto commands = quantiloom::vk::Commands(cmd);
    const auto frameTarget = quantiloom::vk::Target(
        frame, GpuTargetFormat::Rgba8Srgb, width, height,
        static_cast<VkImageLayout>(frameTexture->nativeTexture().layout),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

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
                ? context.ReprocessAccumulated(commands, frameTarget)
                : context.PresentAccumulated(commands, frameTarget);
    }

    FrameResult result;
    if (!presentedWithoutTracing) {
        context.SetViewportSampleBatch(request.chooseSampleBatch());
        context.RenderFrame(commands, frameTarget);
        result.traced = true;
        if (!context.IsReady()) {
            // This frame's recording is the one that failed, and nothing
            // reliable is in the frame texture. The layout QRhi knew is the
            // one it still has, and the caller presents a cleared swapchain
            // image instead; the next frame reports the failure.
            m_frameLayout = frameTexture->nativeTexture().layout;
            return result;
        }
    }

    // The grid samples the depth texture, so it ends in SHADER_READ_ONLY.
    const QRhiTexture::NativeTexture depth = depthTexture->nativeTexture();
    context.BlitDepthTo(
        commands,
        quantiloom::vk::Target(reinterpret_cast<VkImage>(depth.object), GpuTargetFormat::R32Float,
                               width, height, static_cast<VkImageLayout>(depth.layout),
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    m_depthWritten = true;
    m_depthLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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
