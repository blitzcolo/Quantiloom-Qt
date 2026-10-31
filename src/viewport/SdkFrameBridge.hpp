/**
 * @file SdkFrameBridge.hpp
 * @brief Where the viewport's frame meets the SDK's Vulkan-typed calls
 *
 * Every Vulkan-typed ExternalRenderContext call the viewport makes -- Create
 * with its InitParams, RenderFrame, PresentAccumulated, ReprocessAccumulated
 * and BlitDepthTo -- is made here, and this is the only file that handles a
 * VkImageLayout. The calls are the SDK's backend-neutral ones (vk::Commands,
 * vk::Target, vk::Handles from interop/VulkanInterop.hpp); the bridge records
 * no barrier of its own because the SDK's first and last barriers carry the
 * contract below.
 *
 * The SDK writes into the frame texture, a QRhi texture the viewport owns,
 * never into a swapchain image (QRhi's swapchain images have no
 * TRANSFER_DST). It records between QRhiCommandBuffer::beginExternal() and
 * endExternal(), outside any pass, on the frame's primary command buffer. QRhi
 * tracks each texture's {layout, access, stage} and setNativeLayout() updates
 * only the layout, so the SDK's last barrier on a texture has an
 * ALL_COMMANDS / MEMORY_READ|WRITE destination and leaves the finalLayout the
 * bridge passes, which is exactly the layout QRhi's next use requests -- see
 * src/viewport/AGENTS.md.
 */

#pragma once

#include <QSize>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>

class QRhiTexture;
class QVulkanInstance;
struct VkCommandBuffer_T;

namespace quantiloom {
class ExternalRenderContext;
}

namespace viewport {

struct GpuDevice;

class SdkFrameBridge {
public:
    struct ContextResult {
        std::unique_ptr<quantiloom::ExternalRenderContext> context;
        QString error;
    };

    /// The render context on @p device, rendering at @p extent into an
    /// R8G8B8A8_SRGB frame texture.
    [[nodiscard]] static ContextResult createContext(QVulkanInstance* instance,
                                                     const GpuDevice& device, QSize extent);

    struct FrameRequest {
        /// False past the target or while paused: the accumulation is shown
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

    /// Record this frame's SDK work into @p commandBuffer, which is QRhi's
    /// primary command buffer between beginExternal() and endExternal().
    /// @p extent is the render extent both textures were created at.
    FrameResult record(VkCommandBuffer_T* commandBuffer,
                       quantiloom::ExternalRenderContext& context, QRhiTexture* frameTexture,
                       QRhiTexture* depthTexture, QSize extent, const FrameRequest& request);

    /// Tell QRhi where record() left the two textures. Call after
    /// endExternal() and before the next beginPass().
    void publishLayouts(QRhiTexture* frameTexture, QRhiTexture* depthTexture) const;

private:
    int m_frameLayout = 0;
    int m_depthLayout = 0;
    bool m_depthWritten = false;
};

}  // namespace viewport
