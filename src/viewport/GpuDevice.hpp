/**
 * @file GpuDevice.hpp
 * @brief The viewport's Vulkan device, created with what the SDK requires
 *
 * QRhi cannot create this device itself: QRhiVulkanInitParams carries
 * extensions and nothing else, and the features QRhi enables are only the
 * core 1.1/1.2/1.3 structs, so acceleration structures, ray tracing pipelines
 * and ray queries would stay off. The viewport therefore creates the device
 * here and imports it into an application-owned QRhi, and the SDK is handed
 * the same handles. The host owns the lifetime: QRhi never destroys an
 * imported device, and ExternalRenderContext requires the handles to outlive
 * it.
 *
 * The choice of GPU and the creation of the device are the SDK's
 * (quantiloom::vk::CreateDevice), so the CLI and Studio read one requirement
 * list; this file adds what presenting needs -- QRhi's preferred extensions,
 * VK_KHR_swapchain and the present predicate -- and converts to and from Qt
 * types. Handles are spelled as their VkXxx_T pointers so that this
 * header does not pull <vulkan/vulkan.h> into everything that includes it.
 */

#pragma once

#include <QByteArrayList>
#include <QString>

#include <cstdint>
#include <functional>
#include <optional>

struct VkInstance_T;
struct VkPhysicalDevice_T;
struct VkDevice_T;
struct VkQueue_T;

class QVulkanInstance;
class QWindow;

namespace viewport {

/// PFN_vkGetInstanceProcAddr, spelled without <vulkan/vulkan.h>.
using GetInstanceProcAddrFn = void (*(*)(VkInstance_T*, const char*))();

struct GpuDeviceRequest {
    VkInstance_T* instance = nullptr;
    GetInstanceProcAddrFn getInstanceProcAddr = nullptr;
    /// Whether a queue family of a physical device can present to the
    /// viewport's window (QVulkanInstance::supportsPresent).
    std::function<bool(VkPhysicalDevice_T*, uint32_t)> supportsPresent;
    /// Enabled where the device supports them, ignored otherwise: QRhi's
    /// preferred-for-import list, which it assumes is on an imported device.
    QByteArrayList optionalDeviceExtensions;
    /// Always enabled; a device without them is unsuitable (VK_KHR_swapchain
    /// for a host that presents).
    QByteArrayList requiredHostDeviceExtensions;
};

/// Host-owned; release with destroyGpuDevice() after the QRhi and the render
/// context are gone.
struct GpuDevice {
    VkPhysicalDevice_T* physicalDevice = nullptr;
    VkDevice_T* device = nullptr;
    uint32_t queueFamily = 0;
    uint32_t queueIndex = 0;
    VkQueue_T* queue = nullptr;
    /// The GPU's name, for the log and for error messages.
    QString name;
};

struct GpuDeviceResult {
    std::optional<GpuDevice> device;
    /// When there is no device: which GPU lacked what, one line per GPU.
    QString error;
};

/// Ask the SDK for a device: the first discrete GPU that has everything it
/// needs (else the first that does), with one queue on a graphics+compute
/// family that can present. On failure the error says which GPU lacked what.
[[nodiscard]] GpuDeviceResult createGpuDevice(const GpuDeviceRequest& request);

void destroyGpuDevice(GpuDevice& device, VkInstance_T* instance,
                      GetInstanceProcAddrFn getInstanceProcAddr);

/// Whether the window's surface offers an sRGB colour format -- the choice a
/// QRhiSwapChain with the sRGB flag makes, and silently does without.
[[nodiscard]] bool surfaceOffersSrgb(QVulkanInstance* instance, QWindow* window,
                                     VkPhysicalDevice_T* physicalDevice);

}  // namespace viewport
