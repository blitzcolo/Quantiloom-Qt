/**
 * @file GpuBackend.hpp
 * @brief The one seam between the viewport and the GPU API it presents with
 *
 * ViewportRenderer asks a GpuBackend for a device-backed QRhi, a render
 * context on the same device, and the recording of each frame's SDK work.
 * Exactly one implementation is compiled: GpuBackendVulkan.cpp (Windows,
 * Linux) or GpuBackendMetal.mm (macOS), chosen in CMake, so no shared file
 * branches on the platform. Backend-specific types stay behind this header.
 */

#pragma once

#include "SdkFrame.hpp"

#include <QSize>
#include <QString>
#include <QSurface>
#include <rhi/qrhi.h>

#include <memory>

class QVulkanInstance;
class QWindow;

namespace viewport {

class GpuBackend {
public:
    struct RhiResult {
        std::unique_ptr<QRhi> rhi;
        /// Why there is no QRhi, already translated and ready to show.
        QString message;
    };

    virtual ~GpuBackend() = default;

    /// Create the device and import it into an application-owned QRhi that
    /// presents to @p window.
    [[nodiscard]] virtual RhiResult createRhi(QWindow* window) = 0;

    /// Release the device createRhi() made, after the QRhi is gone. Safe to
    /// call with no device.
    virtual void destroyDevice() = 0;

    /// Whether the window's surface offers an sRGB colour format.
    [[nodiscard]] virtual bool surfaceOffersSrgb() const = 0;

    /// Extra usage the frame and depth textures need for the SDK to write them.
    [[nodiscard]] virtual QRhiTexture::Flags sdkWrittenTextureFlags() const = 0;

    [[nodiscard]] virtual ContextResult createContext(QSize extent) = 0;

    /// Record the SDK's work for this frame into @p cb, between its own
    /// beginExternal() and endExternal(), and tell QRhi what state it left
    /// the textures in.
    virtual FrameResult recordFrame(QRhiCommandBuffer* cb,
                                    quantiloom::ExternalRenderContext& context,
                                    QRhiTexture* frameTexture, QRhiTexture* depthTexture,
                                    QSize extent, const FrameRequest& request) = 0;
};

[[nodiscard]] std::unique_ptr<GpuBackend> createGpuBackend();

/// The QSurface type the viewport window needs for this backend's swapchain.
[[nodiscard]] QSurface::SurfaceType presentSurfaceType();

/// Hand the window the process-wide Vulkan instance; a no-op on Metal.
void attachHostInstance(QWindow* window, QVulkanInstance* instance);

/// Process-wide graphics state main() sets up before the first window:
/// Vulkan's QVulkanInstance. Metal has none.
class HostGraphics {
public:
    HostGraphics();
    ~HostGraphics();
    HostGraphics(const HostGraphics&) = delete;
    HostGraphics& operator=(const HostGraphics&) = delete;

    [[nodiscard]] bool create();
    [[nodiscard]] QVulkanInstance* vulkanInstance() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace viewport
