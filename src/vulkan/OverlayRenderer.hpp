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
#include "../editing/GizmoModel.hpp"

#include <vulkan/vulkan.h>

#include <QString>

#include <cstdint>
#include <vector>

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
    /// `gizmoVertices` is this frame's gizmo triangle list (world space,
    /// empty = no gizmo); it is copied into a per-frame vertex buffer.
    void record(QVulkanWindow* window,
                quantiloom::ExternalRenderContext* ctx,
                VkCommandBuffer cmd,
                uint32_t width, uint32_t height,
                const CameraMatrices& camera,
                const std::vector<editing::GizmoVertex>& gizmoVertices);

    /// Destroy everything. Call from releaseResources() (device still valid).
    void releaseResources(QVulkanWindow* window);

    // ========================================================================
    // Composited capture (what-you-see screenshots for agents/debugging)
    // ========================================================================

    /// Queue a one-shot PNG capture of the final swapchain image -- the
    /// SDK's frame WITH the grid and gizmo composited over it, which the
    /// SDK-side captures cannot show. Saved a few frames later (the copy has
    /// to clear the frames in flight); poll the file.
    void requestCompositedCapture(const QString& path);

    /// Record the swapchain copy for a pending capture. Call after record(),
    /// while the image is back in PRESENT_SRC.
    void recordCaptureIfRequested(QVulkanWindow* window, VkCommandBuffer cmd,
                                  uint32_t width, uint32_t height);

    /// Save a recorded capture once its frame is provably complete. Call at
    /// the top of each startNextFrame.
    void finishCaptureIfReady(QVulkanWindow* window);

    /// A capture is requested or recorded but not yet written. The frame loop
    /// has to keep running while this is true: a capture needs one frame to
    /// record and a few more to clear the frames in flight, and the loop now
    /// stops on its own at the target sample count.
    [[nodiscard]] bool capturePending() const { return !m_capturePath.isEmpty(); }

private:
    struct OwnedImage {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    struct FrameVertexBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;
        VkDeviceSize capacity = 0;
    };

    void ensureStaticResources(QVulkanWindow* window);
    void ensureSizedResources(QVulkanWindow* window, VkCommandBuffer cmd,
                              uint32_t width, uint32_t height);
    OwnedImage createImage(QVulkanWindow* window, uint32_t width, uint32_t height,
                           VkFormat format, VkImageUsageFlags usage,
                           VkImageAspectFlags aspect);
    void destroyImage(VkDevice device, OwnedImage& img);
    void createGridPipeline(QVulkanWindow* window, VkFormat colorFormat);
    void createGizmoPipeline(QVulkanWindow* window, VkFormat colorFormat);
    void ensureGizmoBuffers(QVulkanWindow* window);
    void destroyGizmoBuffers(VkDevice device);

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
    VkPipelineLayout m_gizmoPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_gizmoPipeline = VK_NULL_HANDLE;
    std::vector<FrameVertexBuffer> m_gizmoVertexBuffers;  // one per frame in flight
    PFN_vkCmdBeginRenderingKHR m_vkCmdBeginRendering = nullptr;
    PFN_vkCmdEndRenderingKHR m_vkCmdEndRendering = nullptr;

    // Size-dependent resources
    OwnedImage m_depthAov;      // R32_SFLOAT, written by BlitDepthTo, sampled by grid
    OwnedImage m_overlayDepth;  // D32_SFLOAT, cleared per pass, overlay self-occlusion
    VkImageLayout m_depthAovLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Composited-capture state (one shot at a time)
    QString m_capturePath;             // non-empty = capture requested
    bool m_captureRecorded = false;
    int m_captureCountdown = 0;        // frames until the copy is provably done
    VkBuffer m_captureBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_captureMemory = VK_NULL_HANDLE;
    void* m_captureMapped = nullptr;
    uint32_t m_captureWidth = 0;
    uint32_t m_captureHeight = 0;
    VkFormat m_captureFormat = VK_FORMAT_UNDEFINED;
    void destroyCaptureBuffer(VkDevice device);
};

}  // namespace vkview
