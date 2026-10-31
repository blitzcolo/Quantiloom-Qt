/**
 * @file GpuDevice.cpp
 * @brief Device selection and creation for the viewport; see GpuDevice.hpp
 */

#include "GpuDevice.hpp"

#include <renderer/interop/VulkanInterop.hpp>

#include <QVulkanInstance>
#include <QWindow>

#include <core/Log.hpp>

#include <functional>
#include <vector>

namespace viewport {

GpuDeviceResult createGpuDevice(const GpuDeviceRequest& request) {
    GpuDeviceResult result;
    const auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(request.getInstanceProcAddr);
    const auto instance = static_cast<VkInstance>(request.instance);
    if (!gipa || !instance) {
        result.error = QStringLiteral("There is no Vulkan instance.");
        return result;
    }

    // The SDK owns the requirement list (extensions, feature chain, the
    // suitability test and the choice of GPU); the host adds what presenting
    // needs and the host-side present check.
    std::vector<const char*> optional;
    for (const QByteArray& name : request.optionalDeviceExtensions) {
        optional.push_back(name.constData());
    }
    std::vector<const char*> required;
    for (const QByteArray& name : request.requiredHostDeviceExtensions) {
        required.push_back(name.constData());
    }

    quantiloom::vk::DeviceRequest sdkRequest;
    sdkRequest.instance = instance;
    sdkRequest.getInstanceProcAddr = gipa;
    if (request.supportsPresent) {
        sdkRequest.supportsPresent = [](void* user, VkPhysicalDevice physicalDevice,
                                        uint32_t queueFamily) {
            const auto& predicate =
                *static_cast<const std::function<bool(VkPhysicalDevice_T*, uint32_t)>*>(user);
            return predicate(physicalDevice, queueFamily);
        };
        sdkRequest.supportsPresentUser =
            const_cast<std::function<bool(VkPhysicalDevice_T*, uint32_t)>*>(
                &request.supportsPresent);
    }
    sdkRequest.optionalDeviceExtensions = optional.data();
    sdkRequest.optionalDeviceExtensionCount = static_cast<uint32_t>(optional.size());
    sdkRequest.requiredHostDeviceExtensions = required.data();
    sdkRequest.requiredHostDeviceExtensionCount = static_cast<uint32_t>(required.size());

    auto created = quantiloom::vk::CreateDevice(sdkRequest);
    if (!created) {
        result.error = QString::fromStdString(created.error());
        return result;
    }
    const quantiloom::vk::Device& made = created.value();

    GpuDevice out;
    out.physicalDevice = made.physicalDevice;
    out.device = made.device;
    out.queueFamily = made.queueFamily;
    out.queueIndex = made.queueIndex;
    out.queue = made.queue;

    // The name is for the log and for error messages only.
    const auto getProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        gipa(instance, "vkGetPhysicalDeviceProperties"));
    if (getProperties) {
        VkPhysicalDeviceProperties properties{};
        getProperties(made.physicalDevice, &properties);
        out.name = QString::fromUtf8(properties.deviceName);
    }
    QL_LOG_INFO("Viewport GPU: {}, queue family {}", out.name.toStdString(), out.queueFamily);
    result.device = out;
    return result;
}

void destroyGpuDevice(GpuDevice& device, VkInstance_T* instance,
                      GetInstanceProcAddrFn getInstanceProcAddr) {
    if (!device.device) {
        return;
    }
    quantiloom::vk::Device sdkDevice;
    sdkDevice.physicalDevice = device.physicalDevice;
    sdkDevice.device = device.device;
    sdkDevice.queueFamily = device.queueFamily;
    sdkDevice.queueIndex = device.queueIndex;
    sdkDevice.queue = device.queue;
    sdkDevice.instance = instance;
    quantiloom::vk::DestroyDevice(
        sdkDevice, reinterpret_cast<PFN_vkGetInstanceProcAddr>(getInstanceProcAddr));
    device = GpuDevice{};
}

bool surfaceOffersSrgb(QVulkanInstance* instance, QWindow* window,
                       VkPhysicalDevice_T* physicalDevice) {
    const VkSurfaceKHR surface = QVulkanInstance::surfaceForWindow(window);
    const auto getFormats = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
        instance->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceFormatsKHR"));
    if (surface == VK_NULL_HANDLE || !getFormats) {
        return false;
    }
    uint32_t count = 0;
    getFormats(physicalDevice, surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    getFormats(physicalDevice, surface, &count, formats.data());
    // The list QRhi tests a surface format against (isSrgbFormat).
    for (const VkSurfaceFormatKHR& format : formats) {
        switch (format.format) {
        case VK_FORMAT_R8_SRGB:
        case VK_FORMAT_R8G8_SRGB:
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_B8G8R8_SRGB:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
            return true;
        default:
            break;
        }
    }
    return false;
}

}  // namespace viewport
