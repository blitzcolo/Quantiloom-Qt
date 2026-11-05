/**
 * @file GpuBackendMetal.mm
 * @brief GpuBackend on a host-owned MTLDevice imported into QRhi and the SDK
 *
 * Studio creates the MTLDevice and its command queue, hands them to QRhi
 * (QRhiMetalNativeHandles) and to the SDK (mtl::Handles). Per frame the SDK
 * records into QRhi's current MTLCommandBuffer between beginExternal() and
 * endExternal(), where no encoder is open, and writes the frame and depth
 * MTLTextures QRhi owns. Metal has no image layouts, so nothing is published
 * back to QRhi.
 */

#include "GpuBackend.hpp"

#include <renderer/ExternalRenderContext.hpp>
#include <renderer/interop/MetalInterop.hpp>

#import <Metal/Metal.h>

#include <QObject>
#include <QWindow>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#include <core/Log.hpp>

namespace viewport {

namespace {

class MetalBackend final : public GpuBackend {
public:
    RhiResult createRhi(QWindow* window) override {
        m_window = window;
        RhiResult out;

        QString error;
        if (!createDevice(error)) {
            QL_LOG_ERROR("Viewport: no usable GPU: {}", error.toStdString());
            out.message = QObject::tr("No GPU on this machine has what the renderer needs:\n\n%1")
                              .arg(error);
            return out;
        }

        QRhiMetalInitParams params;
        QRhiMetalNativeHandles imported;
        void* const rawDevice = (__bridge void*)m_device;
        void* const rawQueue = (__bridge void*)m_queue;
        imported.dev = (__bridge MTLDevice*)rawDevice;
        imported.cmdQueue = (__bridge MTLCommandQueue*)rawQueue;
        out.rhi.reset(QRhi::create(QRhi::Metal, &params, QRhi::Flags(), &imported));
        if (!out.rhi) {
            out.message = QObject::tr("The renderer could not start:\n\n%1")
                              .arg(QStringLiteral("Qt could not present with the Metal device on %1.")
                                       .arg(m_name));
            destroyDevice();
        }
        return out;
    }

    void destroyDevice() override {
        m_queue = nil;
        m_device = nil;
    }

    bool surfaceOffersSrgb() const override {
        // QRhi's Metal swapchain asks its CAMetalLayer for BGRA8Unorm_sRGB.
        return true;
    }

    QRhiTexture::Flags sdkWrittenTextureFlags() const override {
        // The SDK's Metal backend writes a host texture by drawing into it (a
        // scaling or converting BlitTexture is a render pass) or by a blit copy;
        // it never binds one for shader writes. Render-target usage is what
        // that needs, on the depth texture too.
        return QRhiTexture::RenderTarget;
    }

    ContextResult createContext(QSize extent) override {
        quantiloom::ExternalRenderContext::InitParams params{};
        params.device = quantiloom::mtl::Handles((__bridge void*)m_device, (__bridge void*)m_queue);
        params.width = static_cast<quantiloom::u32>(extent.width());
        params.height = static_cast<quantiloom::u32>(extent.height());

        ContextResult out;
        auto created = quantiloom::ExternalRenderContext::Create(params);
        if (!created) {
            out.error = QString::fromStdString(created.error());
            return out;
        }
        out.context = std::move(created.value());
        return out;
    }

    FrameResult recordFrame(QRhiCommandBuffer* cb, quantiloom::ExternalRenderContext& context,
                            QRhiTexture* frameTexture, QRhiTexture* depthTexture, QSize extent,
                            const FrameRequest& request) override {
        cb->beginExternal();
        const auto* native =
            static_cast<const QRhiMetalCommandBufferNativeHandles*>(cb->nativeHandles());
        const auto width = static_cast<quantiloom::u32>(extent.width());
        const auto height = static_cast<quantiloom::u32>(extent.height());
        using quantiloom::GpuTargetFormat;
        const auto commands = quantiloom::mtl::Commands((__bridge void*)native->commandBuffer);
        const auto frameTarget = quantiloom::mtl::Target(
            reinterpret_cast<void*>(frameTexture->nativeTexture().object),
            GpuTargetFormat::Rgba8Srgb, width, height);
        const auto depthTarget = quantiloom::mtl::Target(
            reinterpret_cast<void*>(depthTexture->nativeTexture().object),
            GpuTargetFormat::R32Float, width, height);
        const FrameResult frame = recordSdkFrame(context, commands, frameTarget, depthTarget, request);
        cb->endExternal();
        return frame;
    }

private:
    bool createDevice(QString& error) {
        m_device = MTLCreateSystemDefaultDevice();
        if (!m_device) {
            error = QStringLiteral("No Metal device is available.");
            return false;
        }
        m_name = QString::fromNSString(m_device.name);
        if (!m_device.supportsRaytracing) {
            error = QStringLiteral("%1 does not support Metal ray tracing.").arg(m_name);
            m_device = nil;
            return false;
        }
        m_queue = [m_device newCommandQueue];
        if (!m_queue) {
            error = QStringLiteral("%1 could not create a command queue.").arg(m_name);
            m_device = nil;
            return false;
        }
        QL_LOG_INFO("Viewport GPU: {} (Metal)", m_name.toStdString());
        return true;
    }

    QWindow* m_window = nullptr;
    id<MTLDevice> m_device = nil;
    id<MTLCommandQueue> m_queue = nil;
    QString m_name;
};

}  // namespace

std::unique_ptr<GpuBackend> createGpuBackend() {
    return std::make_unique<MetalBackend>();
}

QSurface::SurfaceType presentSurfaceType() {
    return QSurface::MetalSurface;
}

void attachHostInstance(QWindow*, QVulkanInstance*) {}

struct HostGraphics::Impl {};

HostGraphics::HostGraphics() = default;
HostGraphics::~HostGraphics() = default;

bool HostGraphics::create() {
    return true;
}

QVulkanInstance* HostGraphics::vulkanInstance() const {
    return nullptr;
}

}  // namespace viewport
