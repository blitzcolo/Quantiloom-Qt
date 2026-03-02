/**
 * @file QuantiloomVulkanRenderer.cpp
 * @brief QVulkanWindowRenderer adapter implementation
 *
 * @author wtflmao
 */

#include "QuantiloomVulkanRenderer.hpp"
#include "QuantiloomVulkanWindow.hpp"

#include <renderer/ExternalRenderContext.hpp>
#include <renderer/LightingParams.hpp>
#include <renderer/AtmosphericConfig.hpp>
#include <scene/Material.hpp>
#include <scene/Scene.hpp>
#include <core/Image.hpp>
#include <core/SpectralData.hpp>

#include <QVulkanFunctions>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QObject>
#include <QDebug>
#include <QProgressDialog>
#include <QApplication>
#include <QTimer>
#include <QStandardPaths>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

QuantiloomVulkanRenderer::QuantiloomVulkanRenderer(QuantiloomVulkanWindow* window)
    : m_window(window)
    , m_lastFrameTime(std::chrono::high_resolution_clock::now())
{
}

QuantiloomVulkanRenderer::~QuantiloomVulkanRenderer() {
    // Release resources in proper order
    m_renderContext.reset();
}

void QuantiloomVulkanRenderer::initResources() {
    qDebug() << "QuantiloomVulkanRenderer::initResources() - Vulkan device ready";
    // Note: Swapchain is not ready yet, full initialization happens in initSwapChainResources()
}

void QuantiloomVulkanRenderer::initSwapChainResources() {
    //qDebug() << "QuantiloomVulkanRenderer::initSwapChainResources() - Starting...";

    QSize swapSize = m_window->swapChainImageSize();
    //qDebug() << "  Swapchain size:" << swapSize;

    if (swapSize.width() <= 0 || swapSize.height() <= 0) {
        qWarning() << "Invalid swapchain size, skipping initialization";
        return;
    }

    // If already initialized, just resize
    if (m_renderContext) {
        //qDebug() << "  Resizing existing context...";
        m_renderContext->Resize(
            static_cast<quantiloom::u32>(swapSize.width()),
            static_cast<quantiloom::u32>(swapSize.height())
        );
        resetAccumulation();
        return;
    }

    // First time initialization - extract Qt-managed Vulkan handles
    QVulkanInstance* inst = m_window->vulkanInstance();
    VkInstance vkInstance = inst->vkInstance();
    VkDevice device = m_window->device();
    VkPhysicalDevice physDevice = m_window->physicalDevice();

    qDebug() << "  VkInstance:" << vkInstance;
    qDebug() << "  VkDevice:" << device;
    qDebug() << "  VkPhysicalDevice:" << physDevice;

    if (!device) {
        qCritical() << "Device is NULL! Qt failed to create Vulkan device.";
        qCritical() << "This usually means required device extensions are not supported.";
        return;
    }

    // Get graphics queue using device functions
    QVulkanDeviceFunctions* df = inst->deviceFunctions(device);
    VkQueue graphicsQueue;
    df->vkGetDeviceQueue(device, m_window->graphicsQueueFamilyIndex(), 0, &graphicsQueue);

    qDebug() << "  VkQueue:" << graphicsQueue;
    qDebug() << "  Queue family:" << m_window->graphicsQueueFamilyIndex();
    qDebug() << "  Color format:" << m_window->colorFormat();

    // Initialize libQuantiloom with external handles
    quantiloom::ExternalRenderContext::InitParams params{};
    params.instance = vkInstance;
    params.physicalDevice = physDevice;
    params.device = device;
    params.graphicsQueue = graphicsQueue;
    params.graphicsQueueFamily = static_cast<quantiloom::u32>(m_window->graphicsQueueFamilyIndex());
    params.targetColorFormat = m_window->colorFormat();
    params.width = static_cast<quantiloom::u32>(swapSize.width());
    params.height = static_cast<quantiloom::u32>(swapSize.height());
    // Note: pipelineCacheDir uses platform-specific default in ExternalRenderContext:
    //   Windows: %LOCALAPPDATA%/Quantiloom/cache/
    //   Linux:   ~/.cache/Quantiloom/
    //   macOS:   ~/Library/Caches/Quantiloom/

    qDebug() << "Creating ExternalRenderContext...";

    auto result = quantiloom::ExternalRenderContext::Create(params);
    if (!result) {
        qCritical() << "Failed to create ExternalRenderContext:"
                    << QString::fromStdString(result.error());
        return;
    }

    qDebug() << "ExternalRenderContext created successfully!";

    m_renderContext = std::move(result.value());
    m_initialized = true;

    // Load pending scene if any
    if (!m_pendingScenePath.isEmpty()) {
        loadScene(m_pendingScenePath);
        m_pendingScenePath.clear();
    }

    // Set initial camera
    m_renderContext->SetCameraLookAt(m_cameraPosition, m_cameraTarget, m_cameraUp);
    m_renderContext->SetCameraFOV(m_cameraFovY);
}

void QuantiloomVulkanRenderer::releaseSwapChainResources() {
    // Nothing to do - render context manages its own resources
}

void QuantiloomVulkanRenderer::releaseResources() {
    // Save current scene path for reload after window restore
    if (!m_currentScenePath.isEmpty()) {
        m_pendingScenePath = m_currentScenePath;
        qDebug() << "Saved scene path for restore:" << m_pendingScenePath;
    }
    m_renderContext.reset();
    m_initialized = false;
}

void QuantiloomVulkanRenderer::startNextFrame() {
    static uint64_t frameCounter = 0;
    frameCounter++;

    auto now = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(now - m_lastFrameTime).count();
    m_lastFrameTime = now;

    // Update camera based on input
    updateCamera(deltaTime);

    if (!m_renderContext || !m_renderContext->HasScene()) {
        // No scene loaded yet, just present empty frame
        m_window->frameReady();
        m_window->requestUpdate();
        return;
    }

    // Log every 100 frames to track progress
    if (frameCounter % 100 == 0) {
        qDebug() << "Frame" << frameCounter << "- samples:" << m_sampleCount;
    }

    // Get current command buffer and swapchain image
    VkCommandBuffer cmd = m_window->currentCommandBuffer();
    int swapChainIndex = m_window->currentSwapChainImageIndex();
    VkImage targetImage = m_window->swapChainImage(swapChainIndex);
    QSize swapSize = m_window->swapChainImageSize();

    // Render frame using libQuantiloom
    // ExternalRenderContext handles layout transitions and blit to swapchain
    m_renderContext->RenderFrame(
        cmd,
        targetImage,
        VK_IMAGE_LAYOUT_UNDEFINED,  // Qt doesn't guarantee initial layout
        static_cast<quantiloom::u32>(swapSize.width()),
        static_cast<quantiloom::u32>(swapSize.height())
    );

    // Update sample count
    m_sampleCount = m_renderContext->GetAccumulatedSamples();

    // Calculate frame time
    auto frameEnd = std::chrono::high_resolution_clock::now();
    m_lastFrameTimeMs = std::chrono::duration<float, std::milli>(frameEnd - now).count();

    // Emit frame rendered signal
    emit m_window->frameRendered(m_lastFrameTimeMs, m_sampleCount);

    // Signal frame ready and request next frame
    m_window->frameReady();
    m_window->requestUpdate();
}

void QuantiloomVulkanRenderer::loadScene(const QString& filePath) {
    qDebug() << "QuantiloomVulkanRenderer::loadScene() - Path:" << filePath;

    if (!m_initialized) {
        qDebug() << "  Not initialized, saving as pending...";
        m_pendingScenePath = filePath;
        return;
    }

    if (!m_renderContext) {
        qCritical() << "  Render context is null!";
        emit m_window->sceneLoaded(false, QObject::tr("Render context not initialized"));
        return;
    }

    // Check if this is the first run (no pipeline cache)
    QProgressDialog* progressDialog = nullptr;
    if (isFirstRun()) {
        qDebug() << "  First run detected - showing shader compilation dialog";

        // Create modal progress dialog
        // Note: Using nullptr as parent since QVulkanWindow is not a QWidget
        progressDialog = new QProgressDialog(
            QObject::tr("Compiling and loading shaders...\nIt may take a few minutes."),
            QString(),  // No cancel button
            0, 0,       // Indeterminate progress
            nullptr
        );
        progressDialog->setWindowTitle(QObject::tr("Initializing"));
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);  // Show immediately
        progressDialog->setCancelButton(nullptr);  // Remove cancel button
        progressDialog->setAutoClose(true);
        progressDialog->setAutoReset(true);
        progressDialog->setMinimumWidth(350);

        // Make dialog non-closable
        progressDialog->setWindowFlags(
            Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint
        );

        // Show dialog and process events to ensure it's displayed
        progressDialog->show();
        progressDialog->raise();
        progressDialog->activateWindow();
        QApplication::processEvents();
    }

    // Determine file type and call appropriate loader
    std::string path = filePath.toStdString();
    quantiloom::Result<void, quantiloom::String> result;

    if (filePath.endsWith(".usd", Qt::CaseInsensitive) ||
        filePath.endsWith(".usda", Qt::CaseInsensitive) ||
        filePath.endsWith(".usdc", Qt::CaseInsensitive) ||
        filePath.endsWith(".usdz", Qt::CaseInsensitive)) {
        qDebug() << "  Calling LoadSceneFromUsd...";
        result = m_renderContext->LoadSceneFromUsd(path);
    } else {
        qDebug() << "  Calling LoadSceneFromGltf...";
        result = m_renderContext->LoadSceneFromGltf(path);
    }

    // Close progress dialog
    if (progressDialog) {
        progressDialog->close();
        progressDialog->deleteLater();
    }

    qDebug() << "  Scene load returned";

    if (result) {
        qDebug() << "  Scene loaded successfully!";
        m_currentScenePath = filePath;  // Save for restore after minimize

        // Re-apply stored render settings (important for restore after minimize)
        if (m_hasLightingParams) {
            qDebug() << "  Re-applying stored LightingParams";
            m_renderContext->SetLightingParams(m_lightingParams);
        }
        m_renderContext->SetSpectralMode(m_spectralMode);
        m_renderContext->SetDebugMode(m_debugMode);
        m_renderContext->SetSPP(m_targetSPP);
        m_renderContext->SetWavelength(m_wavelength);

        resetAccumulation();
        emit m_window->sceneLoaded(true, QObject::tr("Scene loaded successfully"));
    } else {
        qCritical() << "  Failed to load scene:" << QString::fromStdString(result.error());
        emit m_window->sceneLoaded(false,
            QObject::tr("Failed to load scene: %1").arg(QString::fromStdString(result.error())));
    }
}

void QuantiloomVulkanRenderer::resetCamera() {
    m_cameraPosition = glm::vec3(0.0f, 1.0f, 5.0f);
    m_cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    m_cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    m_orbitDistance = 5.0f;
    m_orbitYaw = 0.0f;
    m_orbitPitch = 0.0f;

    if (m_renderContext) {
        m_renderContext->SetCameraLookAt(m_cameraPosition, m_cameraTarget, m_cameraUp);
        resetAccumulation();
    }
}

void QuantiloomVulkanRenderer::setCamera(const glm::vec3& position, const glm::vec3& lookAt,
                                          const glm::vec3& up, float fovY) {
    m_cameraPosition = position;
    m_cameraTarget = lookAt;
    m_cameraUp = up;
    m_cameraFovY = fovY;

    // Update orbit distance based on new position/target
    m_orbitDistance = glm::length(position - lookAt);

    // Calculate orbit angles from the direction vector
    glm::vec3 dir = glm::normalize(position - lookAt);
    m_orbitPitch = glm::degrees(std::asin(dir.y));
    m_orbitYaw = glm::degrees(std::atan2(dir.x, dir.z));

    if (m_renderContext) {
        m_renderContext->SetCameraLookAt(m_cameraPosition, m_cameraTarget, m_cameraUp);
        m_renderContext->SetCameraFOV(fovY);
        resetAccumulation();
    }

    qDebug() << "Camera set: pos=(" << position.x << "," << position.y << "," << position.z
             << ") lookAt=(" << lookAt.x << "," << lookAt.y << "," << lookAt.z
             << ") fov=" << fovY;
}

void QuantiloomVulkanRenderer::setSPP(uint32_t spp) {
    m_targetSPP = spp;
    if (m_renderContext) {
        m_renderContext->SetSPP(spp);
    }
}

void QuantiloomVulkanRenderer::setWavelength(float wavelength_nm) {
    m_wavelength = wavelength_nm;
    if (m_renderContext) {
        m_renderContext->SetWavelength(wavelength_nm);
        resetAccumulation();
    }
}

void QuantiloomVulkanRenderer::setSpectralMode(quantiloom::SpectralMode mode) {
    m_spectralMode = mode;  // Store for restore
    if (m_renderContext) {
        m_renderContext->SetSpectralMode(mode);
        resetAccumulation();
    }
}

void QuantiloomVulkanRenderer::setDebugMode(quantiloom::DebugVisualizationMode mode) {
    m_debugMode = mode;  // Store for restore
    if (m_renderContext) {
        m_renderContext->SetDebugMode(mode);
        resetAccumulation();
    }
}

void QuantiloomVulkanRenderer::setLightingParams(const quantiloom::LightingParams& params) {
    m_lightingParams = params;  // Store for restore
    m_hasLightingParams = true;
    if (m_renderContext) {
        m_renderContext->SetLightingParams(params);
        resetAccumulation();
    }
}

void QuantiloomVulkanRenderer::updateMaterial(int index, const quantiloom::Material& material) {
    if (m_renderContext && index >= 0) {
        m_renderContext->UpdateMaterial(static_cast<quantiloom::u32>(index), material);
        resetAccumulation();
    }
}

int QuantiloomVulkanRenderer::addComplexRefractiveIndex(const quantiloom::ComplexRefractiveIndex& cri) {
    if (m_renderContext)
        return m_renderContext->AddComplexRefractiveIndex(cri);
    return -1;
}

const quantiloom::Scene* QuantiloomVulkanRenderer::getScene() const {
    return m_renderContext ? m_renderContext->GetScene() : nullptr;
}

void QuantiloomVulkanRenderer::getCameraInfo(glm::vec3& position, glm::vec3& forward,
                                              glm::vec3& right, glm::vec3& up) const {
    position = m_cameraPosition;
    forward = glm::normalize(m_cameraTarget - m_cameraPosition);
    right = glm::normalize(glm::cross(forward, m_cameraUp));
    up = glm::cross(right, forward);
}

void QuantiloomVulkanRenderer::updateCameraMovement(
    bool forward, bool backward, bool left, bool right,
    bool up, bool down, bool fast)
{
    m_moveForward = forward;
    m_moveBackward = backward;
    m_moveLeft = left;
    m_moveRight = right;
    m_moveUp = up;
    m_moveDown = down;
    m_moveFast = fast;
}

void QuantiloomVulkanRenderer::orbitCamera(float deltaX, float deltaY) {
    const float sensitivity = 0.005f;

    m_orbitYaw -= deltaX * sensitivity;
    m_orbitPitch -= deltaY * sensitivity;

    // Clamp pitch to avoid gimbal lock
    m_orbitPitch = glm::clamp(m_orbitPitch, -glm::half_pi<float>() + 0.1f,
                                             glm::half_pi<float>() - 0.1f);

    // Calculate new camera position
    float x = m_orbitDistance * std::cos(m_orbitPitch) * std::sin(m_orbitYaw);
    float y = m_orbitDistance * std::sin(m_orbitPitch);
    float z = m_orbitDistance * std::cos(m_orbitPitch) * std::cos(m_orbitYaw);

    m_cameraPosition = m_cameraTarget + glm::vec3(x, y, z);

    if (m_renderContext) {
        m_renderContext->SetCameraLookAt(m_cameraPosition, m_cameraTarget, m_cameraUp);
        resetAccumulation();
    }
}

void QuantiloomVulkanRenderer::panCamera(float deltaX, float deltaY) {
    const float sensitivity = 0.01f;

    glm::vec3 forward = glm::normalize(m_cameraTarget - m_cameraPosition);
    glm::vec3 right = glm::normalize(glm::cross(forward, m_cameraUp));
    glm::vec3 up = glm::cross(right, forward);

    glm::vec3 pan = -right * deltaX * sensitivity + up * deltaY * sensitivity;
    m_cameraPosition += pan;
    m_cameraTarget += pan;

    if (m_renderContext) {
        m_renderContext->SetCameraLookAt(m_cameraPosition, m_cameraTarget, m_cameraUp);
        resetAccumulation();
    }
}

void QuantiloomVulkanRenderer::zoomCamera(float delta) {
    const float zoomSpeed = 0.5f;

    m_orbitDistance *= (1.0f - delta * zoomSpeed * 0.1f);
    m_orbitDistance = glm::clamp(m_orbitDistance, 0.1f, 1000.0f);

    // Recalculate camera position
    float x = m_orbitDistance * std::cos(m_orbitPitch) * std::sin(m_orbitYaw);
    float y = m_orbitDistance * std::sin(m_orbitPitch);
    float z = m_orbitDistance * std::cos(m_orbitPitch) * std::cos(m_orbitYaw);

    m_cameraPosition = m_cameraTarget + glm::vec3(x, y, z);

    if (m_renderContext) {
        m_renderContext->SetCameraLookAt(m_cameraPosition, m_cameraTarget, m_cameraUp);
        resetAccumulation();
    }
}

void QuantiloomVulkanRenderer::updateCamera(float deltaTime) {
    if (!m_moveForward && !m_moveBackward && !m_moveLeft &&
        !m_moveRight && !m_moveUp && !m_moveDown) {
        return;
    }

    const float baseSpeed = 5.0f;
    float speed = m_moveFast ? baseSpeed * 3.0f : baseSpeed;

    glm::vec3 forward = glm::normalize(m_cameraTarget - m_cameraPosition);
    glm::vec3 right = glm::normalize(glm::cross(forward, m_cameraUp));

    glm::vec3 movement(0.0f);
    if (m_moveForward)  movement += forward;
    if (m_moveBackward) movement -= forward;
    if (m_moveRight)    movement += right;
    if (m_moveLeft)     movement -= right;
    if (m_moveUp)       movement += m_cameraUp;
    if (m_moveDown)     movement -= m_cameraUp;

    if (glm::length(movement) > 0.0f) {
        movement = glm::normalize(movement) * speed * deltaTime;
        m_cameraPosition += movement;
        m_cameraTarget += movement;

        if (m_renderContext) {
            m_renderContext->SetCameraLookAt(m_cameraPosition, m_cameraTarget, m_cameraUp);
            resetAccumulation();
        }
    }
}

void QuantiloomVulkanRenderer::resetAccumulation() {
    m_sampleCount = 0;
    if (m_renderContext) {
        m_renderContext->ResetAccumulation();
    }
}

bool QuantiloomVulkanRenderer::isFirstRun() const {
    // Check if pipeline cache file exists
    // If it doesn't exist, this is the first run and shader compilation will be slow
    //
    // Cache location follows platform conventions:
    //   Windows: %LOCALAPPDATA%/Quantiloom/cache/pipeline_cache.bin
    //   Linux:   ~/.cache/Quantiloom/pipeline_cache.bin
    //   macOS:   ~/Library/Caches/Quantiloom/pipeline_cache.bin
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
#ifdef Q_OS_WIN
    // Windows: QStandardPaths returns %LOCALAPPDATA%, add /Quantiloom/cache
    QString cachePath = cacheDir + "/Quantiloom/cache/pipeline_cache.bin";
#else
    // Linux/macOS: QStandardPaths returns ~/.cache or ~/Library/Caches
    QString cachePath = cacheDir + "/Quantiloom/pipeline_cache.bin";
#endif
    return !QFileInfo::exists(cachePath);
}

bool QuantiloomVulkanRenderer::readDebugPixel(int x, int y, glm::vec4& outValue) {
    if (!m_renderContext) {
        return false;
    }

    auto result = m_renderContext->ReadPixelValue(
        static_cast<quantiloom::u32>(x),
        static_cast<quantiloom::u32>(y)
    );

    if (result.has_value()) {
        outValue = result.value();
        return true;
    }

    return false;
}

QString QuantiloomVulkanRenderer::formatDebugValue(const glm::vec4& v) const {
    using quantiloom::DebugVisualizationMode;

    switch (m_debugMode) {
        // Vector types: inverse mapping from [0,1] -> [-1,1]
        case DebugVisualizationMode::GeometricNormal:
        case DebugVisualizationMode::ShadedNormal:
        case DebugVisualizationMode::Tangent:
        case DebugVisualizationMode::ReflectionDir:
            return QString("Vec(%1, %2, %3)")
                .arg((v.r - 0.5f) * 2.0f, 0, 'f', 3)
                .arg((v.g - 0.5f) * 2.0f, 0, 'f', 3)
                .arg((v.b - 0.5f) * 2.0f, 0, 'f', 3);

        // Scalar types: direct R channel
        case DebugVisualizationMode::Metallic:
            return QString("Metallic: %1").arg(v.r, 0, 'f', 3);
        case DebugVisualizationMode::Roughness:
            return QString("Roughness: %1").arg(v.r, 0, 'f', 3);
        case DebugVisualizationMode::Alpha:
            return QString("Alpha: %1").arg(v.r, 0, 'f', 3);
        case DebugVisualizationMode::NdotL:
            return QString("NdotL: %1").arg(v.r, 0, 'f', 3);
        case DebugVisualizationMode::NdotV:
            return QString("NdotV: %1").arg(v.r, 0, 'f', 3);
        case DebugVisualizationMode::AtmosphericTransmittance:
            return QString("Transmittance: %1").arg(v.r, 0, 'f', 3);

        // RGB types: direct display
        case DebugVisualizationMode::BaseColor:
            return QString("BaseColor(%1, %2, %3)")
                .arg(v.r, 0, 'f', 3).arg(v.g, 0, 'f', 3).arg(v.b, 0, 'f', 3);
        case DebugVisualizationMode::Emissive:
            return QString("Emissive(%1, %2, %3)")
                .arg(v.r, 0, 'f', 3).arg(v.g, 0, 'f', 3).arg(v.b, 0, 'f', 3);
        case DebugVisualizationMode::DirectSun:
            return QString("DirectSun(%1, %2, %3)")
                .arg(v.r, 0, 'f', 3).arg(v.g, 0, 'f', 3).arg(v.b, 0, 'f', 3);
        case DebugVisualizationMode::Diffuse:
            return QString("Diffuse(%1, %2, %3)")
                .arg(v.r, 0, 'f', 3).arg(v.g, 0, 'f', 3).arg(v.b, 0, 'f', 3);
        case DebugVisualizationMode::FresnelF0:
            return QString("F0(%1, %2, %3)")
                .arg(v.r, 0, 'f', 3).arg(v.g, 0, 'f', 3).arg(v.b, 0, 'f', 3);
        case DebugVisualizationMode::Fresnel:
            return QString("Fresnel(%1, %2, %3)")
                .arg(v.r, 0, 'f', 3).arg(v.g, 0, 'f', 3).arg(v.b, 0, 'f', 3);
        case DebugVisualizationMode::BRDF_Full:
            return QString("BRDF(%1, %2, %3)")
                .arg(v.r, 0, 'f', 3).arg(v.g, 0, 'f', 3).arg(v.b, 0, 'f', 3);
        case DebugVisualizationMode::PrefilteredEnv:
            return QString("PrefilteredEnv(%1, %2, %3)")
                .arg(v.r, 0, 'f', 3).arg(v.g, 0, 'f', 3).arg(v.b, 0, 'f', 3);
        case DebugVisualizationMode::IblSpecular:
            return QString("IBL_Specular(%1, %2, %3)")
                .arg(v.r, 0, 'f', 3).arg(v.g, 0, 'f', 3).arg(v.b, 0, 'f', 3);
        case DebugVisualizationMode::SkyAmbient:
            return QString("SkyAmbient(%1, %2, %3)")
                .arg(v.r, 0, 'f', 3).arg(v.g, 0, 'f', 3).arg(v.b, 0, 'f', 3);
        case DebugVisualizationMode::XYZ_Tristimulus:
            return QString("XYZ(%1, %2, %3)")
                .arg(v.r, 0, 'f', 4).arg(v.g, 0, 'f', 4).arg(v.b, 0, 'f', 4);

        // UV type
        case DebugVisualizationMode::UV:
            return QString("UV(%1, %2)").arg(v.r, 0, 'f', 4).arg(v.g, 0, 'f', 4);

        // BRDF LUT: scale and bias
        case DebugVisualizationMode::BrdfLut:
            return QString("BRDF_LUT(scale=%1, bias=%2)")
                .arg(v.r, 0, 'f', 3).arg(v.g, 0, 'f', 3);

        // Non-invertible (hash/frac)
        case DebugVisualizationMode::WorldPosition:
            return QString("WorldPos(frac) - original lost");
        case DebugVisualizationMode::MaterialID:
            return QString("MaterialID(hash) - original lost");
        case DebugVisualizationMode::TriangleID:
            return QString("TriangleID(hash) - original lost");
        case DebugVisualizationMode::Barycentric:
            return QString("Bary(%1, %2, %3)")
                .arg(v.r, 0, 'f', 3).arg(v.g, 0, 'f', 3).arg(v.b, 0, 'f', 3);

        // IR modes
        case DebugVisualizationMode::Temperature:
            return QString("Temperature(mapped) - use colorbar");
        case DebugVisualizationMode::IREmissivity:
            return QString("IREmissivity: %1").arg(v.r, 0, 'f', 3);
        case DebugVisualizationMode::IREmission:
            return QString("IREmission(%1, %2, %3)")
                .arg(v.r, 0, 'f', 3).arg(v.g, 0, 'f', 3).arg(v.b, 0, 'f', 3);
        case DebugVisualizationMode::IRReflection:
            return QString("IRReflection(%1, %2, %3)")
                .arg(v.r, 0, 'f', 3).arg(v.g, 0, 'f', 3).arg(v.b, 0, 'f', 3);

        // None or unknown
        case DebugVisualizationMode::None:
        default:
            return QString("RGB(%1, %2, %3)")
                .arg(v.r, 0, 'f', 3).arg(v.g, 0, 'f', 3).arg(v.b, 0, 'f', 3);
    }
}

std::unique_ptr<quantiloom::Image> QuantiloomVulkanRenderer::captureScreenshot() {
    if (!m_renderContext) {
        return nullptr;
    }

    auto result = m_renderContext->CaptureScreenshot();
    if (!result.has_value()) {
        qWarning() << "Screenshot capture failed:" << QString::fromStdString(result.error());
        return nullptr;
    }

    auto image = std::make_unique<quantiloom::Image>(std::move(result.value()));

    // GPU sensor is already applied in RenderFrame() if enabled
    // No need to apply CPU sensor here - screenshot captures what's displayed
    if (m_sensorEnabled) {
        qDebug() << "[Sensor] Screenshot captured with GPU sensor effects (applied in real-time)";
    }

    return image;
}

std::unique_ptr<quantiloom::Image> QuantiloomVulkanRenderer::captureDisplayImage() {
    if (!m_renderContext) {
        return nullptr;
    }

    auto result = m_renderContext->CaptureDisplayImage();
    if (!result.has_value()) {
        qWarning() << "Display image capture failed:" << QString::fromStdString(result.error());
        return nullptr;
    }

    auto image = std::make_unique<quantiloom::Image>(std::move(result.value()));

    // GPU sensor is already applied in RenderFrame() if enabled
    // CaptureDisplayImage() captures the final displayed image (with GPU sensor + CLAHE)
    if (m_sensorEnabled) {
        qDebug() << "[Sensor] Display image captured with GPU sensor effects (applied in real-time)";
    }

    return image;
}

// ============================================================================
// Atmospheric Configuration
// ============================================================================

void QuantiloomVulkanRenderer::setAtmosphericPreset(const QString& preset) {
    m_atmosphericPreset = preset;
    std::string presetStr = preset.toLower().toStdString();

    if (presetStr == "clear_day") {
        m_atmosphericConfig = quantiloom::AtmosphericConfig::ClearDay();
    } else if (presetStr == "hazy") {
        m_atmosphericConfig = quantiloom::AtmosphericConfig::Hazy();
    } else if (presetStr == "polluted_urban") {
        m_atmosphericConfig = quantiloom::AtmosphericConfig::PollutedUrban();
    } else if (presetStr == "mountain_top") {
        m_atmosphericConfig = quantiloom::AtmosphericConfig::MountainTop();
    } else if (presetStr == "mars") {
        m_atmosphericConfig = quantiloom::AtmosphericConfig::Mars();
    } else {
        m_atmosphericConfig = quantiloom::AtmosphericConfig::Disabled();
    }

    if (m_renderContext) {
        m_renderContext->SetAtmosphericConfig(m_atmosphericConfig);
    }

    qDebug() << "Atmospheric preset set to:" << preset;
}

void QuantiloomVulkanRenderer::setAtmosphericConfig(const quantiloom::AtmosphericConfig& config) {
    m_atmosphericConfig = config;

    if (m_renderContext) {
        m_renderContext->SetAtmosphericConfig(m_atmosphericConfig);
    }
}

// ============================================================================
// Environment Map (IBL)
// ============================================================================

bool QuantiloomVulkanRenderer::loadEnvironmentMap(const QString& hdrPath) {
    if (!m_renderContext) {
        qWarning() << "Cannot load environment map: render context not initialized";
        return false;
    }

    if (hdrPath.isEmpty()) {
        qWarning() << "Empty environment map path";
        return false;
    }

    qDebug() << "Loading environment map:" << hdrPath;

    auto result = m_renderContext->LoadEnvironmentMap(hdrPath.toStdString());
    if (!result.has_value()) {
        qWarning() << "Failed to load environment map:"
                   << QString::fromStdString(result.error());
        return false;
    }

    qDebug() << "Environment map loaded successfully";
    return true;
}

bool QuantiloomVulkanRenderer::hasEnvironmentMap() const {
    return m_renderContext && m_renderContext->HasEnvironmentMap();
}

// ============================================================================
// Sensor Simulation
// ============================================================================

void QuantiloomVulkanRenderer::setSensorEnabled(bool enabled) {
    m_sensorEnabled = enabled;

    // Use GPU sensor via libQuantiloom (real-time)
    if (m_renderContext) {
        m_renderContext->SetGPUSensorEnabled(enabled);
    }

    // Keep CPU sensor for fallback/comparison (optional)
    if (enabled && !m_sensor) {
        m_sensor = std::make_unique<quantiloom::GenericSensor>();
        qDebug() << "[Sensor] Created GenericSensor instance (CPU fallback)";
    }

    qDebug() << "[Sensor] GPU sensor simulation" << (enabled ? "ENABLED" : "DISABLED");
}

void QuantiloomVulkanRenderer::setSensorParams(const quantiloom::SensorParams& params) {
    m_sensorParams = params;

    // Update GPU sensor params in libQuantiloom
    if (m_renderContext) {
        m_renderContext->SetGPUSensorParams(params);
    }

    // Keep CPU sensor for fallback/comparison (optional)
    if (!m_sensor) {
        m_sensor = std::make_unique<quantiloom::GenericSensor>();
        qDebug() << "[Sensor] Created GenericSensor instance (CPU fallback)";
    }

    qDebug() << "[Sensor] GPU params updated:"
             << "focal=" << params.focalLength_mm << "mm"
             << ", f/" << params.fNumber
             << ", pixel_pitch=" << params.pixelPitch_um << "um"
             << ", QE=" << params.quantumEfficiency
             << ", well=" << params.wellCapacity_e << "e-"
             << ", bit_depth=" << params.bitDepth
             << ", gain=" << params.gain << "e-/DN"
             << ", t_int=" << params.integrationTime_s << "s";
    qDebug() << "[Sensor]   Noise: poisson=" << params.enablePoissonNoise
             << ", read_noise=" << params.enableReadNoise << "(" << params.readNoise_e_rms << "e-)"
             << ", dark_current=" << params.enableDarkCurrent << "(" << params.darkCurrent_e_s << "e-/s)"
             << ", fpn=" << params.enableFPN;
    if (params.enableFPN) {
        qDebug() << "[Sensor]   FPN: prnu_sigma=" << params.prnuSigma
                 << ", dsnu_sigma=" << params.dsnuSigma_e << "e-"
                 << ", nuc=" << params.enableNUC << "(eff=" << params.nucEfficiency << ")";
    }
}

void QuantiloomVulkanRenderer::setDisplayEnhancement(bool enabled, float clipLimit,
                                                      int tileSize, bool luminanceOnly) {
    m_displayEnhancementEnabled = enabled;
    m_claheClipLimit = clipLimit;
    m_claheTileSize = tileSize;
    m_claheLuminanceOnly = luminanceOnly;

    // Pass CLAHE params to libQuantiloom for GPU processing
    if (m_renderContext) {
        quantiloom::ExternalRenderContext::CLAHEParams params;
        params.enabled = enabled;
        params.clipLimit = clipLimit;
        params.tileSize = tileSize;
        params.luminanceOnly = luminanceOnly;
        params.normalizeOutput = true;
        m_renderContext->SetCLAHEParams(params);
    }

    qDebug() << "Display enhancement:" << (enabled ? "ENABLED" : "disabled")
             << "- CLAHE clip=" << clipLimit
             << ", tiles=" << tileSize << "x" << tileSize
             << ", luminanceOnly=" << luminanceOnly;
}
