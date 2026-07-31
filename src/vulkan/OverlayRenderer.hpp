/**
 * @file OverlayRenderer.hpp
 * @brief Editor overlay pass drawn on top of the SDK's rendered frame
 *
 * The SDK blits its path-traced image straight into the swapchain image and
 * leaves it in PRESENT_SRC. This pass runs after it in the same command
 * buffer: it transitions the image back to a color attachment, draws the
 * viewport grid (and, later, the transform gizmo) with dynamic rendering and
 * LOAD_OP_LOAD, and returns the image to PRESENT_SRC. It never touches
 * QVulkanWindow::defaultRenderPass(), which clears.
 *
 * Scene occlusion comes from the SDK's primary-hit depth AOV: BlitDepthTo
 * copies it into an image owned here, and the grid fragment shader compares
 * its own ray-plane distance against it (both are distances along the same
 * normalized primary rays). A separate D32 attachment, cleared every frame,
 * orders overlay geometry against itself only -- which is what keeps the
 * gizmo always on top of the scene, Blender-style.
 *
 * All colors written by the overlay shaders are linear; the sRGB swapchain
 * format encodes on write and blends in linear, so no shader does gamma.
 *
 * @author blitzcolo
 */

#pragma once

#include "CameraMatrices.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>

class QVulkanWindow;

namespace quantiloom {
class ExternalRenderContext;
}

namespace vkview {

class OverlayRenderer {
public:
    OverlayRenderer() = default;
    ~OverlayRenderer() = default;

    OverlayRenderer(const OverlayRenderer&) = delete;
    OverlayRenderer& operator=(const OverlayRenderer&) = delete;

    /// Display-only flag: flipping it must not reset accumulation (the scene
    /// is unchanged; only what is composited over it changes).
    void setGridVisible(bool visible) { m_gridVisible = visible; }
    [[nodiscard]] bool gridVisible() const { return m_gridVisible; }

    /// Record the overlay after ExternalRenderContext::RenderFrame in the
    /// same command buffer. A no-op while nothing is visible.
    void record(QVulkanWindow* window,
                quantiloom::ExternalRenderContext* ctx,
                VkCommandBuffer cmd,
                uint32_t width, uint32_t height,
                const CameraMatrices& camera);

    /// Destroy everything. Call from releaseResources() (device still valid).
    void releaseResources(QVulkanWindow* window);

private:
    struct OwnedImage {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    void ensureStaticResources(QVulkanWindow* window);
    void ensureSizedResources(QVulkanWindow* window, VkCommandBuffer cmd,
                              uint32_t width, uint32_t height);
    OwnedImage createImage(QVulkanWindow* window, uint32_t width, uint32_t height,
                           VkFormat format, VkImageUsageFlags usage,
                           VkImageAspectFlags aspect);
    void destroyImage(VkDevice device, OwnedImage& img);
    void createGridPipeline(QVulkanWindow* window, VkFormat colorFormat);

    bool m_gridVisible = false;

    // Static (device-lifetime) resources
    bool m_staticReady = false;
    VkFormat m_pipelineColorFormat = VK_FORMAT_UNDEFINED;
    VkSampler m_nearestSampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_gridPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_gridPipeline = VK_NULL_HANDLE;
    PFN_vkCmdBeginRenderingKHR m_vkCmdBeginRendering = nullptr;
    PFN_vkCmdEndRenderingKHR m_vkCmdEndRendering = nullptr;

    // Size-dependent resources
    OwnedImage m_depthAov;      // R32_SFLOAT, written by BlitDepthTo, sampled by grid
    OwnedImage m_overlayDepth;  // D32_SFLOAT, cleared per pass, overlay self-occlusion
    VkImageLayout m_depthAovLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool m_overlayDepthInitialized = false;
};

}  // namespace vkview
