/**
 * @file GpuBackendVulkan.cpp
 * @brief GpuBackend on the SDK-created Vulkan device imported into QRhi
 */

#include "GpuBackend.hpp"
#include "GpuDevice.hpp"
#include "SdkFrameBridge.hpp"

#include <QLoggingCategory>
#include <QObject>
#include <QVulkanInstance>
#include <QWindow>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#include <core/Log.hpp>

namespace viewport {

namespace {

class VulkanBackend final : public GpuBackend {
public:
    RhiResult createRhi(QWindow* window) override {
        m_window = window;
        QVulkanInstance* inst = window->vulkanInstance();
        m_getInstanceProcAddr = reinterpret_cast<GetInstanceProcAddrFn>(
            inst->getInstanceProcAddr("vkGetInstanceProcAddr"));

        GpuDeviceRequest request;
        request.instance = inst->vkInstance();
        request.getInstanceProcAddr = m_getInstanceProcAddr;
        request.supportsPresent = [inst, window](VkPhysicalDevice_T* physicalDevice,
                                                 uint32_t queueFamily) {
            return inst->supportsPresent(physicalDevice, queueFamily, window);
        };
        request.optionalDeviceExtensions =
            QRhiVulkanInitParams::preferredExtensionsForImportedDevice();
        request.requiredHostDeviceExtensions = {QByteArrayLiteral(VK_KHR_SWAPCHAIN_EXTENSION_NAME)};

        RhiResult out;
        GpuDeviceResult created = createGpuDevice(request);
        if (!created.device) {
            QL_LOG_ERROR("Viewport: no usable GPU: {}", created.error.toStdString());
            out.message = QObject::tr("No GPU on this machine has what the renderer needs:\n\n%1")
                              .arg(created.error);
            return out;
        }
        m_device = *created.device;

        // QRhi presents and runs the overlay on the same device and queue the
        // SDK records into. It never destroys an imported device; the
        // renderer calls destroyDevice() after it.
        QRhiVulkanInitParams params;
        params.inst = inst;
        params.window = window;
        QRhiVulkanNativeHandles imported;
        imported.physDev = m_device.physicalDevice;
        imported.dev = m_device.device;
        imported.gfxQueueFamilyIdx = m_device.queueFamily;
        imported.gfxQueueIdx = m_device.queueIndex;
        out.rhi.reset(QRhi::create(QRhi::Vulkan, &params, QRhi::Flags(), &imported));
        if (!out.rhi) {
            out.message = QObject::tr("The renderer could not start:\n\n%1")
                              .arg(QStringLiteral("Qt could not present with the Vulkan device on %1.")
                                       .arg(m_device.name));
            destroyDevice();
        }
        return out;
    }

    void destroyDevice() override {
        if (m_device.device) {
            destroyGpuDevice(m_device, m_window->vulkanInstance()->vkInstance(),
                             m_getInstanceProcAddr);
        }
    }

    bool surfaceOffersSrgb() const override {
        return viewport::surfaceOffersSrgb(m_window->vulkanInstance(), m_window,
                                           m_device.physicalDevice);
    }

    QRhiTexture::Flags sdkWrittenTextureFlags() const override { return {}; }

    ContextResult createContext(QSize extent) override {
        return SdkFrameBridge::createContext(m_window->vulkanInstance(), m_device, extent);
    }

    FrameResult recordFrame(QRhiCommandBuffer* cb, quantiloom::ExternalRenderContext& context,
                            QRhiTexture* frameTexture, QRhiTexture* depthTexture, QSize extent,
                            const FrameRequest& request) override {
        // Outside any pass, on the primary command buffer: QRhi records what
        // it has deferred so far, and the SDK's commands follow it.
        cb->beginExternal();
        const auto* native =
            static_cast<const QRhiVulkanCommandBufferNativeHandles*>(cb->nativeHandles());
        const FrameResult frame = m_bridge.record(native->commandBuffer, context, frameTexture,
                                                  depthTexture, extent, request);
        cb->endExternal();
        m_bridge.publishLayouts(frameTexture, depthTexture);
        return frame;
    }

private:
    QWindow* m_window = nullptr;
    GpuDevice m_device;
    GetInstanceProcAddrFn m_getInstanceProcAddr = nullptr;
    SdkFrameBridge m_bridge;
};

}  // namespace

std::unique_ptr<GpuBackend> createGpuBackend() {
    return std::make_unique<VulkanBackend>();
}

QSurface::SurfaceType presentSurfaceType() {
    return QSurface::VulkanSurface;
}

void attachHostInstance(QWindow* window, QVulkanInstance* instance) {
    window->setVulkanInstance(instance);
}

struct HostGraphics::Impl {
    QVulkanInstance instance;
};

HostGraphics::HostGraphics() : m_impl(std::make_unique<Impl>()) {}
HostGraphics::~HostGraphics() = default;

bool HostGraphics::create() {
    QVulkanInstance& vulkanInstance = m_impl->instance;

    // Set Vulkan API version (1.3 required for ray tracing)
    vulkanInstance.setApiVersion(QVersionNumber(1, 3, 0));

    // Enable validation layers in debug builds
#ifdef QT_DEBUG
    vulkanInstance.setLayers({"VK_LAYER_KHRONOS_validation"});
    QLoggingCategory::setFilterRules("qt.vulkan=true");
#endif

    // Required extensions for ray tracing, plus what QRhi presents best with
    // (the swapchain colour-space extension among them). Extensions the
    // implementation does not have are ignored by QVulkanInstance.
    QByteArrayList instanceExtensions{VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME};
    for (const QByteArray& extension : QRhiVulkanInitParams::preferredInstanceExtensions()) {
        if (!instanceExtensions.contains(extension)) {
            instanceExtensions.append(extension);
        }
    }
    vulkanInstance.setExtensions(instanceExtensions);

    if (!vulkanInstance.create()) {
        qFatal("Failed to create Vulkan instance: %d", vulkanInstance.errorCode());
        return false;
    }
    return true;
}

QVulkanInstance* HostGraphics::vulkanInstance() const {
    return &m_impl->instance;
}

}  // namespace viewport
