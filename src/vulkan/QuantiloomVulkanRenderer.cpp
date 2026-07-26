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
#include <atmos/AtmosphereNNConfig.hpp>
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
        if (!m_paused) {
            m_window->requestUpdate();
        }
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

    // Signal frame ready and request next frame. The frame in flight is always
    // completed -- stopping before the render would present an undefined
    // swapchain image -- so a pause takes effect from the next frame on.
    m_window->frameReady();
    if (!m_paused) {
        m_window->requestUpdate();
    }
}

void QuantiloomVulkanRenderer::setPaused(bool paused) {
    if (m_paused == paused) {
        return;
    }
    m_paused = paused;
    if (!m_paused) {
        m_window->requestUpdate();
    }
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

    // First run compiles every pipeline, which takes minutes. This used to
    // raise an application-modal dialog with no cancel button before the
    // window had even appeared; it reports through the shell now, which shows
    // it beside the viewport and stays interactive.
    const bool compilingShaders = isFirstRun();
    if (compilingShaders) {
        qDebug() << "  First run detected - shaders will be compiled";
        emit m_window->longOperationStarted(
            QObject::tr("Compiling shaders — first run may take a few minutes"));
        // One pass so the message is painted before the blocking load begins.
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

    if (compilingShaders) {
        emit m_window->longOperationFinished();
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
        m_renderContext->SetSamplingSeed(m_samplingSeed);
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

    // Derive the orbit state from the position rather than restating it: the
    // hardcoded 5.0 / 0 / 0 did not describe (0, 1, 5), so the first drag after
    // a reset snapped the camera to where the orbit state said it already was.
    const glm::vec3 offset = m_cameraPosition - m_cameraTarget;
    m_orbitDistance = glm::length(offset);
    m_orbitPitch = std::asin(glm::clamp(offset.y / m_orbitDistance, -1.0f, 1.0f));
    m_orbitYaw = std::atan2(offset.x, offset.z);

    if (m_renderContext) {
        m_renderContext->SetCameraLookAt(m_cameraPosition, m_cameraTarget, m_cameraUp);
        resetAccumulation();
    }
    emit m_window->cameraChanged();
}

void QuantiloomVulkanRenderer::setCamera(const glm::vec3& position, const glm::vec3& lookAt,
                                          const glm::vec3& up, float fovY) {
    m_cameraPosition = position;
    m_cameraTarget = lookAt;
    m_cameraUp = up;
    m_cameraFovY = fovY;

    // Update orbit distance based on new position/target
    m_orbitDistance = glm::length(position - lookAt);

    // Recover the orbit angles from the direction vector. These are radians
    // everywhere else -- orbitCamera() feeds them straight to sin/cos and
    // clamps against half_pi -- so they must be stored as radians here too.
    // glm::degrees() used to be applied to both, which meant loading a scene
    // config and then dragging jumped the camera to an unrelated angle.
    // A zero-length view vector has no angles to recover; keep the current
    // ones rather than normalize() a zero vector into NaN.
    if (m_orbitDistance > 1e-6f) {
        const glm::vec3 dir = (position - lookAt) / m_orbitDistance;
        m_orbitPitch = std::asin(glm::clamp(dir.y, -1.0f, 1.0f));
        m_orbitYaw = std::atan2(dir.x, dir.z);
    }

    if (m_renderContext) {
        m_renderContext->SetCameraLookAt(m_cameraPosition, m_cameraTarget, m_cameraUp);
        m_renderContext->SetCameraFOV(fovY);
        resetAccumulation();
    }

    qDebug() << "Camera set: pos=(" << position.x << "," << position.y << "," << position.z
             << ") lookAt=(" << lookAt.x << "," << lookAt.y << "," << lookAt.z
             << ") fov=" << fovY;

    emit m_window->cameraChanged();
}

void QuantiloomVulkanRenderer::getCameraState(glm::vec3& position, glm::vec3& target,
                                              glm::vec3& up, float& fovY) const {
    position = m_cameraPosition;
    target = m_cameraTarget;
    up = m_cameraUp;
    fovY = m_cameraFovY;
}

void QuantiloomVulkanRenderer::setCameraFovY(float fovY) {
    m_cameraFovY = fovY;
    if (m_renderContext) {
        m_renderContext->SetCameraFOV(fovY);
        resetAccumulation();
    }
    emit m_window->cameraChanged();
}

void QuantiloomVulkanRenderer::setViewDirection(const glm::vec3& direction) {
    const float length = glm::length(direction);
    if (length < 1e-6f) {
        return;
    }
    const glm::vec3 dir = direction / length;

    // Same radians-everywhere convention as orbitCamera(): these two feed
    // sin/cos directly and are clamped against half_pi.
    m_orbitPitch = std::asin(glm::clamp(dir.y, -1.0f, 1.0f));
    m_orbitYaw = std::atan2(dir.x, dir.z);
    m_cameraPosition = m_cameraTarget + dir * m_orbitDistance;

    if (m_renderContext) {
        m_renderContext->SetCameraLookAt(m_cameraPosition, m_cameraTarget, m_cameraUp);
        resetAccumulation();
    }
    emit m_window->cameraChanged();
}

void QuantiloomVulkanRenderer::setSPP(uint32_t spp) {
    m_targetSPP = spp;
    if (m_renderContext) {
        m_renderContext->SetSPP(spp);
    }
}

void QuantiloomVulkanRenderer::setSamplingSeed(uint32_t seed) {
    m_samplingSeed = seed;
    if (m_renderContext) {
        // SetSamplingSeed resets accumulation itself, so the next pass starts
        // from the new sequence rather than mixing two of them.
        m_renderContext->SetSamplingSeed(seed);
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

    // Straight down and straight up are allowed. The 0.1 rad margin that used
    // to be subtracted here was not about gimbal lock in this code: it existed
    // because Camera::UpdateVectors normalize()d a zero cross product when
    // forward became parallel to up, producing a NaN basis that reached every
    // primary ray. The SDK now gives both degenerate inputs a defined result,
    // so the margin only cost the user a viewpoint.
    m_orbitPitch = glm::clamp(m_orbitPitch, -glm::half_pi<float>(),
                                             glm::half_pi<float>());

    // Calculate new camera position
    float x = m_orbitDistance * std::cos(m_orbitPitch) * std::sin(m_orbitYaw);
    float y = m_orbitDistance * std::sin(m_orbitPitch);
    float z = m_orbitDistance * std::cos(m_orbitPitch) * std::cos(m_orbitYaw);

    m_cameraPosition = m_cameraTarget + glm::vec3(x, y, z);

    if (m_renderContext) {
        m_renderContext->SetCameraLookAt(m_cameraPosition, m_cameraTarget, m_cameraUp);
        resetAccumulation();
    }
    emit m_window->cameraChanged();
}

void QuantiloomVulkanRenderer::panCamera(float deltaX, float deltaY) {
    const float sensitivity = 0.01f;

    glm::vec3 forward = glm::normalize(m_cameraTarget - m_cameraPosition);

    // Straight-down and straight-up views are reachable now that orbitCamera()
    // no longer keeps a margin away from the poles, and there forward is
    // parallel to m_cameraUp. That breaks the cross product two ways: exactly
    // parallel (a camera placed directly above the target by setCamera) gives a
    // zero vector that normalize() turns into NaN, which pans the camera to
    // nowhere and never recovers; arriving via orbitCamera leaves cos(pitch) at
    // ~-4.4e-8 instead of 0, so the cross product is merely tiny and normalize()
    // returns a finite direction made of floating-point dust that swings with
    // yaw. Swing to a reference axis that is not parallel, exactly as the SDK's
    // Camera::UpdateVectors does for the same input.
    glm::vec3 upRef = m_cameraUp;
    if (std::abs(glm::dot(forward, upRef)) > 1.0f - 1e-6f) {
        upRef = (std::abs(forward.y) > 0.9f) ? glm::vec3(0.0f, 0.0f, 1.0f)
                                             : glm::vec3(0.0f, 1.0f, 0.0f);
    }

    glm::vec3 right = glm::normalize(glm::cross(forward, upRef));
    glm::vec3 up = glm::cross(right, forward);

    glm::vec3 pan = -right * deltaX * sensitivity + up * deltaY * sensitivity;
    m_cameraPosition += pan;
    m_cameraTarget += pan;

    if (m_renderContext) {
        m_renderContext->SetCameraLookAt(m_cameraPosition, m_cameraTarget, m_cameraUp);
        resetAccumulation();
    }
    emit m_window->cameraChanged();
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
    emit m_window->cameraChanged();
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
        emit m_window->cameraChanged();
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

    // Numbers only. The label used to be baked in here -- "Metallic: 0.42",
    // "NdotL: 0.42" -- which duplicated the mode names in ModeCatalog in
    // English, in a class with no translation context. Callers prepend the
    // catalogue's (translated) name, so there is one list of mode names again.
    auto vec3 = [&v](int precision) {
        return QStringLiteral("(%1, %2, %3)")
            .arg(v.r, 0, 'f', precision)
            .arg(v.g, 0, 'f', precision)
            .arg(v.b, 0, 'f', precision);
    };
    auto scalar = [&v]() { return QStringLiteral("%1").arg(v.r, 0, 'f', 3); };

    switch (m_debugMode) {
        // Vector types: inverse mapping from [0,1] -> [-1,1]
        case DebugVisualizationMode::GeometricNormal:
        case DebugVisualizationMode::ShadedNormal:
        case DebugVisualizationMode::Tangent:
        case DebugVisualizationMode::ReflectionDir:
            return QStringLiteral("(%1, %2, %3)")
                .arg((v.r - 0.5f) * 2.0f, 0, 'f', 3)
                .arg((v.g - 0.5f) * 2.0f, 0, 'f', 3)
                .arg((v.b - 0.5f) * 2.0f, 0, 'f', 3);

        // Scalar types: direct R channel
        case DebugVisualizationMode::Metallic:
        case DebugVisualizationMode::Roughness:
        case DebugVisualizationMode::Alpha:
        case DebugVisualizationMode::NdotL:
        case DebugVisualizationMode::NdotV:
        case DebugVisualizationMode::AtmosphericTransmittance:
        case DebugVisualizationMode::IREmissivity:
            return scalar();

        // RGB types: direct display
        case DebugVisualizationMode::BaseColor:
        case DebugVisualizationMode::Emissive:
        case DebugVisualizationMode::DirectSun:
        case DebugVisualizationMode::Diffuse:
        case DebugVisualizationMode::FresnelF0:
        case DebugVisualizationMode::Fresnel:
        case DebugVisualizationMode::BRDF_Full:
        case DebugVisualizationMode::PrefilteredEnv:
        case DebugVisualizationMode::IblSpecular:
        case DebugVisualizationMode::SkyAmbient:
        case DebugVisualizationMode::Barycentric:
        case DebugVisualizationMode::IREmission:
        case DebugVisualizationMode::IRReflection:
            return vec3(3);

        case DebugVisualizationMode::XYZ_Tristimulus:
            return vec3(4);

        case DebugVisualizationMode::UV:
            return QStringLiteral("(%1, %2)").arg(v.r, 0, 'f', 4).arg(v.g, 0, 'f', 4);

        case DebugVisualizationMode::BrdfLut:
            return tr("scale %1, bias %2").arg(v.r, 0, 'f', 3).arg(v.g, 0, 'f', 3);

        // Encodings that discard the original value
        case DebugVisualizationMode::WorldPosition:
            return tr("fractional part only — original value not recoverable");
        case DebugVisualizationMode::MaterialID:
        case DebugVisualizationMode::TriangleID:
            return tr("hashed — original value not recoverable");
        case DebugVisualizationMode::Temperature:
            return tr("colour-mapped — read against the colour bar");

        // None or unknown
        case DebugVisualizationMode::None:
        default:
            return vec3(3);
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

    // Map legacy analytic preset names to their closest NN preset
    if (presetStr == "clear_day" || presetStr == "mountain_top") {
        presetStr = "clear";
    } else if (presetStr == "hazy") {
        presetStr = "haze";
    } else if (presetStr == "polluted_urban") {
        presetStr = "urban_haze";
    } else if (presetStr == "mars") {
        qWarning() << "Atmospheric preset 'mars' has no NN equivalent, disabling atmosphere";
        presetStr = "disabled";
    }

    quantiloom::AtmosphereNNConfig config;
    config.modelPackDir = m_atmosphericConfig.modelPackDir;  // Keep user's pack dir
    if (config.ApplyPreset(presetStr)) {
        config.enabled = (presetStr != "disabled");
    } else {
        qWarning() << "Unknown atmospheric preset" << preset << "- disabling atmosphere";
        config.enabled = false;
        config.preset = "disabled";
    }
    m_atmosphericConfig = config;

    applyAtmosphereToContext();

    qDebug() << "Atmospheric preset set to:" << QString::fromStdString(presetStr);
}

void QuantiloomVulkanRenderer::setAtmosphericConfig(const quantiloom::AtmosphereNNConfig& config) {
    m_atmosphericConfig = config;
    applyAtmosphereToContext();
}

std::string QuantiloomVulkanRenderer::resolveDefaultModelPackDir() {
    QStringList candidates;
    QString envDir = qEnvironmentVariable("QUANTILOOM_ATMOS_MODELS");
    if (!envDir.isEmpty()) {
        candidates << envDir;
    }
    // Copied next to the exe from the SDK by a POST_BUILD step
    candidates << QCoreApplication::applicationDirPath() + "/assets/atmos_models";

    for (const QString& dir : candidates) {
        if (QDir(dir).exists()) {
            return QDir::toNativeSeparators(dir).toStdString();
        }
    }
    return {};
}

void QuantiloomVulkanRenderer::applyAtmosphereToContext() {
    if (!m_renderContext) {
        return;
    }

    quantiloom::AtmosphereNNConfig config = m_atmosphericConfig;
    if (config.enabled && config.modelPackDir.empty()) {
        config.modelPackDir = resolveDefaultModelPackDir();
        if (config.modelPackDir.empty()) {
            qWarning() << "NN atmosphere model pack not found (set QUANTILOOM_ATMOS_MODELS "
                          "or pick a directory in the Atmospheric panel); disabling atmosphere";
            config.enabled = false;
        }
    }

    try {
        m_renderContext->SetAtmosphere(config);
    } catch (const std::exception& e) {
        qWarning() << "Failed to apply NN atmosphere:" << e.what() << "- disabling atmosphere";
        config.enabled = false;
        try {
            m_renderContext->SetAtmosphere(config);
        } catch (const std::exception& e2) {
            qWarning() << "Failed to disable NN atmosphere:" << e2.what();
        }
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
