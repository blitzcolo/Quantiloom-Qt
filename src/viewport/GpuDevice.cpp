/**
 * @file GpuDevice.cpp
 * @brief Device selection and creation for the viewport; see GpuDevice.hpp
 */

#include "GpuDevice.hpp"

#include <vulkan/vulkan.h>

#include <QStringList>
#include <QVulkanInstance>
#include <QWindow>

#include <core/Log.hpp>

#include <vector>

namespace viewport {

namespace {

/// The device extensions the SDK's own device creation enables
/// (VulkanContext::CreateLogicalDevice) and its suitability test requires.
constexpr const char* kSdkDeviceExtensions[] = {
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_RAY_QUERY_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME,
    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
};

struct InstanceFunctions {
    PFN_vkEnumeratePhysicalDevices enumeratePhysicalDevices = nullptr;
    PFN_vkGetPhysicalDeviceProperties getProperties = nullptr;
    PFN_vkGetPhysicalDeviceFeatures2 getFeatures2 = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties getQueueFamilies = nullptr;
    PFN_vkEnumerateDeviceExtensionProperties enumerateExtensions = nullptr;
    PFN_vkCreateDevice createDevice = nullptr;
    PFN_vkGetDeviceProcAddr getDeviceProcAddr = nullptr;

    InstanceFunctions(PFN_vkGetInstanceProcAddr gipa, VkInstance instance) {
        enumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
            gipa(instance, "vkEnumeratePhysicalDevices"));
        getProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
            gipa(instance, "vkGetPhysicalDeviceProperties"));
        getFeatures2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
            gipa(instance, "vkGetPhysicalDeviceFeatures2"));
        getQueueFamilies = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
            gipa(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
        enumerateExtensions = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
            gipa(instance, "vkEnumerateDeviceExtensionProperties"));
        createDevice = reinterpret_cast<PFN_vkCreateDevice>(gipa(instance, "vkCreateDevice"));
        getDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
            gipa(instance, "vkGetDeviceProcAddr"));
    }

    [[nodiscard]] bool complete() const {
        return enumeratePhysicalDevices && getProperties && getFeatures2 && getQueueFamilies &&
               enumerateExtensions && createDevice && getDeviceProcAddr;
    }
};

/// The feature structs the SDK reads, linked into one chain. Used both to
/// query what a device supports and to say what to enable; the chain points
/// into this object, so it is linked where it is used and never copied after.
struct FeatureChain {
    VkPhysicalDeviceFeatures2 core{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructure{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    VkPhysicalDeviceRayQueryFeaturesKHR rayQuery{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipeline{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    VkPhysicalDeviceVulkan12Features vulkan12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan13Features vulkan13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};

    FeatureChain() = default;
    FeatureChain(const FeatureChain&) = delete;
    FeatureChain& operator=(const FeatureChain&) = delete;

    /// The 1.3 struct only goes in when the device reports Vulkan 1.3: a
    /// struct of a version the device lacks is not allowed in the chain.
    VkPhysicalDeviceFeatures2* link(bool withVulkan13) {
        core.pNext = &accelerationStructure;
        accelerationStructure.pNext = &rayQuery;
        rayQuery.pNext = &rayTracingPipeline;
        rayTracingPipeline.pNext = &vulkan12;
        vulkan12.pNext = withVulkan13 ? &vulkan13 : nullptr;
        vulkan13.pNext = nullptr;
        return &core;
    }
};

[[nodiscard]] bool atLeast(uint32_t apiVersion, uint32_t major, uint32_t minor) {
    const uint32_t vMajor = VK_API_VERSION_MAJOR(apiVersion);
    return vMajor > major || (vMajor == major && VK_API_VERSION_MINOR(apiVersion) >= minor);
}

struct Candidate {
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties{};
    uint32_t queueFamily = 0;
    QByteArrayList availableExtensions;
};

/// Empty when @p candidate has everything the SDK and the host need; else
/// what it lacks. Fills the queue family and the extension list.
QString unsuitableBecause(const InstanceFunctions& fns, const GpuDeviceRequest& request,
                          Candidate& candidate) {
    const VkPhysicalDevice physicalDevice = candidate.physicalDevice;
    // The SDK's shaders are vulkan1.2 SPIR-V.
    if (!atLeast(candidate.properties.apiVersion, 1, 2)) {
        return QStringLiteral("Vulkan 1.2 is not supported");
    }

    uint32_t extensionCount = 0;
    fns.enumerateExtensions(physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    fns.enumerateExtensions(physicalDevice, nullptr, &extensionCount, extensions.data());
    for (const VkExtensionProperties& extension : extensions) {
        candidate.availableExtensions.append(QByteArray(extension.extensionName));
    }

    QStringList missing;
    for (const char* name : kSdkDeviceExtensions) {
        if (!candidate.availableExtensions.contains(QByteArray(name))) {
            missing.append(QString::fromLatin1(name));
        }
    }
    for (const QByteArray& name : request.requiredHostDeviceExtensions) {
        if (!candidate.availableExtensions.contains(name)) {
            missing.append(QString::fromLatin1(name));
        }
    }
    if (!missing.isEmpty()) {
        // The feature structs of an absent extension cannot be queried.
        return QStringLiteral("missing %1").arg(missing.join(QStringLiteral(", ")));
    }

    // The extension being present does not imply its feature is: a device can
    // expose VK_KHR_ray_query without rayQuery.
    FeatureChain supported;
    fns.getFeatures2(physicalDevice,
                     supported.link(atLeast(candidate.properties.apiVersion, 1, 3)));
    const std::pair<VkBool32, const char*> features[] = {
        {supported.accelerationStructure.accelerationStructure, "accelerationStructure"},
        {supported.rayTracingPipeline.rayTracingPipeline, "rayTracingPipeline"},
        {supported.rayQuery.rayQuery, "rayQuery"},
        {supported.vulkan12.bufferDeviceAddress, "bufferDeviceAddress"},
        {supported.vulkan12.descriptorIndexing, "descriptorIndexing"},
        {supported.vulkan12.runtimeDescriptorArray, "runtimeDescriptorArray"},
        {supported.vulkan12.descriptorBindingPartiallyBound, "descriptorBindingPartiallyBound"},
        {supported.vulkan12.shaderSampledImageArrayNonUniformIndexing,
         "shaderSampledImageArrayNonUniformIndexing"},
    };
    for (const auto& [enabled, name] : features) {
        if (!enabled) {
            missing.append(QString::fromLatin1(name));
        }
    }
    if (!missing.isEmpty()) {
        return QStringLiteral("missing the feature %1").arg(missing.join(QStringLiteral(", ")));
    }

    // One queue does everything: QRhi presents on it and the SDK submits its
    // compute and ray tracing work to it.
    uint32_t familyCount = 0;
    fns.getQueueFamilies(physicalDevice, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    fns.getQueueFamilies(physicalDevice, &familyCount, families.data());
    constexpr VkQueueFlags kNeeded = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    for (uint32_t i = 0; i < familyCount; ++i) {
        if ((families[i].queueFlags & kNeeded) == kNeeded &&
            (!request.supportsPresent || request.supportsPresent(physicalDevice, i))) {
            candidate.queueFamily = i;
            return {};
        }
    }
    return QStringLiteral("no graphics and compute queue that can present to the viewport");
}

}  // namespace

GpuDeviceResult createGpuDevice(const GpuDeviceRequest& request) {
    GpuDeviceResult result;
    const auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(request.getInstanceProcAddr);
    const auto instance = static_cast<VkInstance>(request.instance);
    if (!gipa || !instance) {
        result.error = QStringLiteral("There is no Vulkan instance.");
        return result;
    }
    const InstanceFunctions fns(gipa, instance);
    if (!fns.complete()) {
        result.error = QStringLiteral("The Vulkan loader is missing core entry points.");
        return result;
    }

    uint32_t deviceCount = 0;
    fns.enumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
    fns.enumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());
    if (deviceCount == 0) {
        result.error = QStringLiteral("No Vulkan device was found.");
        return result;
    }

    // First discrete GPU that passes, else the first that passes. Asking
    // rather than guessing from the device type is what keeps an integrated
    // GPU without ray tracing from being taken on a hybrid laptop, and what
    // turns "no device" into a sentence naming what is missing.
    std::optional<Candidate> chosen;
    QStringList rejected;
    for (const VkPhysicalDevice physicalDevice : physicalDevices) {
        Candidate candidate;
        candidate.physicalDevice = physicalDevice;
        fns.getProperties(physicalDevice, &candidate.properties);
        const QString name = QString::fromUtf8(candidate.properties.deviceName);
        const QString reason = unsuitableBecause(fns, request, candidate);
        if (!reason.isEmpty()) {
            QL_LOG_INFO("Viewport: GPU '{}' skipped: {}", name.toStdString(),
                        reason.toStdString());
            rejected.append(QStringLiteral("%1: %2").arg(name, reason));
            continue;
        }
        const bool discrete =
            candidate.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        const bool chosenIsDiscrete =
            chosen && chosen->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        if (!chosen || (discrete && !chosenIsDiscrete)) {
            chosen = std::move(candidate);
        }
    }
    if (!chosen) {
        result.error = rejected.join(QLatin1Char('\n'));
        return result;
    }

    QByteArrayList enabledExtensions;
    for (const char* name : kSdkDeviceExtensions) {
        enabledExtensions.append(QByteArray(name));
    }
    for (const QByteArray& name : request.requiredHostDeviceExtensions) {
        if (!enabledExtensions.contains(name)) {
            enabledExtensions.append(name);
        }
    }
    for (const QByteArray& name : request.optionalDeviceExtensions) {
        if (chosen->availableExtensions.contains(name) && !enabledExtensions.contains(name)) {
            enabledExtensions.append(name);
        }
    }
    std::vector<const char*> extensionNames;
    extensionNames.reserve(static_cast<size_t>(enabledExtensions.size()));
    for (const QByteArray& name : enabledExtensions) {
        extensionNames.push_back(name.constData());
    }

    const bool vulkan13 = atLeast(chosen->properties.apiVersion, 1, 3);
    FeatureChain supported;
    fns.getFeatures2(chosen->physicalDevice, supported.link(vulkan13));

    FeatureChain enable;
    // Every supported 1.0 feature but robustBufferAccess, as QRhi enables on a
    // device it creates: QRhi reads its caps from the supported features, and
    // on an imported device would otherwise act on one that is not enabled.
    enable.core.features = supported.core.features;
    enable.core.features.robustBufferAccess = VK_FALSE;
    enable.accelerationStructure.accelerationStructure = VK_TRUE;
    enable.rayTracingPipeline.rayTracingPipeline = VK_TRUE;
    enable.rayQuery.rayQuery = VK_TRUE;
    enable.vulkan12.bufferDeviceAddress = VK_TRUE;
    enable.vulkan12.descriptorIndexing = VK_TRUE;
    enable.vulkan12.runtimeDescriptorArray = VK_TRUE;
    enable.vulkan12.descriptorBindingPartiallyBound = VK_TRUE;
    enable.vulkan12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    // Enabled where supported, as the SDK does; nothing requires them.
    enable.vulkan13.synchronization2 = supported.vulkan13.synchronization2;
    enable.vulkan13.dynamicRendering = supported.vulkan13.dynamicRendering;

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = chosen->queueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceInfo.pNext = enable.link(vulkan13);
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
    deviceInfo.ppEnabledExtensionNames = extensionNames.data();

    const QString name = QString::fromUtf8(chosen->properties.deviceName);
    VkDevice device = VK_NULL_HANDLE;
    const VkResult created = fns.createDevice(chosen->physicalDevice, &deviceInfo, nullptr, &device);
    if (created != VK_SUCCESS || device == VK_NULL_HANDLE) {
        result.error = QStringLiteral("%1: vkCreateDevice failed (VkResult %2)")
                           .arg(name)
                           .arg(static_cast<int>(created));
        return result;
    }

    const auto getDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(
        fns.getDeviceProcAddr(device, "vkGetDeviceQueue"));
    VkQueue queue = VK_NULL_HANDLE;
    getDeviceQueue(device, chosen->queueFamily, 0, &queue);

    const uint32_t api = chosen->properties.apiVersion;
    QL_LOG_INFO("Viewport GPU: {} ({}), Vulkan {}.{}.{}, queue family {}; device extensions: {}",
                name.toStdString(),
                chosen->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                    ? "discrete" : "not discrete",
                VK_API_VERSION_MAJOR(api), VK_API_VERSION_MINOR(api), VK_API_VERSION_PATCH(api),
                chosen->queueFamily, enabledExtensions.join(' ').toStdString());

    GpuDevice out;
    out.physicalDevice = chosen->physicalDevice;
    out.device = device;
    out.queueFamily = chosen->queueFamily;
    out.queueIndex = 0;
    out.queue = queue;
    out.name = name;
    result.device = out;
    return result;
}

void destroyGpuDevice(GpuDevice& device, VkInstance_T* instance,
                      GetInstanceProcAddrFn getInstanceProcAddr) {
    if (!device.device) {
        return;
    }
    const auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(getInstanceProcAddr);
    const auto getDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        gipa(static_cast<VkInstance>(instance), "vkGetDeviceProcAddr"));
    const auto destroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(
        getDeviceProcAddr(device.device, "vkDestroyDevice"));
    destroyDevice(device.device, nullptr);
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
