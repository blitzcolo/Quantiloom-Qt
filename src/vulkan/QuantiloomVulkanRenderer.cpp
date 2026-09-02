/**
 * @file QuantiloomVulkanRenderer.cpp
 * @brief QVulkanWindowRenderer adapter implementation
 *
 * @author blitzcolo
 */

#include "QuantiloomVulkanRenderer.hpp"
#include "QuantiloomVulkanWindow.hpp"

#include <renderer/ExternalRenderContext.hpp>
#include <renderer/LightingParams.hpp>
#include <atmos/AtmosphereNNConfig.hpp>
#include <core/Config.hpp>
#include <core/Log.hpp>
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
#include <cmath>
#include <iterator>

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
        emit m_window->renderContextFailed(
            QObject::tr("Qt could not create a Vulkan device. The selected GPU is most "
                        "likely missing the ray tracing extensions this renderer needs. "
                        "On a laptop with both an integrated and a discrete GPU, check "
                        "that Quantiloom is running on the discrete one."));
        return;
    }

    // Get graphics queue using device functions
    QVulkanDeviceFunctions* df = inst->deviceFunctions(device);
    VkQueue graphicsQueue;
    df->vkGetDeviceQueue(device, m_window->graphicsQueueFamilyIndex(), 0, &graphicsQueue);

    qDebug() << "  VkQueue:" << graphicsQueue;
    qDebug() << "  Queue family:" << m_window->graphicsQueueFamilyIndex();

    // Through the core logger, not qDebug: an sRGB swapchain is what encodes
    // the SDK's linear output on the presenting blit, and setPreferredColorFormats
    // only states a preference -- Qt falls back silently if the surface does not
    // offer one. With no console attached qDebug reaches the debugger and not
    // the log, so a viewport rendering uncorrected linear radiance would look
    // exactly like one that is correct.
    const VkFormat colorFormat = m_window->colorFormat();
    const bool srgbTarget = colorFormat == VK_FORMAT_B8G8R8A8_SRGB ||
                            colorFormat == VK_FORMAT_R8G8B8A8_SRGB;
    if (srgbTarget) {
        QL_LOG_INFO("Swapchain colour format {} (sRGB): the present blit encodes "
                    "linear radiance", static_cast<int>(colorFormat));
    } else {
        QL_LOG_WARN("Swapchain colour format {} is not sRGB: the present blit "
                    "cannot encode, so the viewport shows linear radiance "
                    "uncorrected and reads darker than the CLI's PNG",
                    static_cast<int>(colorFormat));
    }

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
        const QString reason = QString::fromStdString(result.error());
        qCritical() << "Failed to create ExternalRenderContext:" << reason;
        emit m_window->renderContextFailed(
            QObject::tr("The renderer could not start:\n\n%1").arg(reason));
        return;
    }

    qDebug() << "ExternalRenderContext created successfully!";

    m_renderContext = std::move(result.value());
    m_initialized = true;

    // Everything the shell configured before this point -- lighting, spectral
    // mode, sensor model, camera, environment map -- was queued by the window
    // because there was nothing to apply it to. Replay it now, before the
    // scene loads, so the first scene opened after launch renders with the
    // configuration it was opened with rather than with defaults.
    m_window->applyDeferredSettings();

    // Rebuild the open document. A minimize destroys the context, and the fresh
    // one starts at its constructor defaults -- which for a config document
    // means no solar LUT, no spectral curves, no refractive indices, no
    // temperature backfill and a disabled atmosphere. Re-pushing this class's
    // members never covered any of that, because it holds none of it; replaying
    // the config does.
    if (m_currentConfig && !m_configAppliedToContext) {
        applyConfigToContext(/*isFreshOpen=*/false);
        m_pendingScenePath.clear();
    } else if (!m_currentConfig && !m_pendingScenePath.isEmpty()) {
        loadScene(m_pendingScenePath, /*adoptSceneCamera=*/false);
        m_pendingScenePath.clear();
    }

    // Set initial camera. Skipped for a config document, which has just had one
    // applied -- pushing the stale members over it would undo that.
    if (!m_currentConfig) {
        m_renderContext->SetCameraLookAt(m_cameraPosition, m_cameraTarget, m_cameraUp);
        m_renderContext->SetCameraFOV(m_cameraFovY);
        m_renderContext->SetCameraProjection(
            m_orthographic ? quantiloom::CameraProjection::Orthographic
                           : quantiloom::CameraProjection::Perspective,
            m_orthoHeight);
    }
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
    m_overlay.releaseResources(m_window);
    m_renderContext.reset();
    m_initialized = false;
    // The next context is a fresh one and knows none of the document.
    m_configAppliedToContext = false;
}

void QuantiloomVulkanRenderer::startNextFrame() {
    static uint64_t frameCounter = 0;
    frameCounter++;

    // A composited capture recorded a few frames ago is complete by now
    m_overlay.finishCaptureIfReady(m_window);

    auto now = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(now - m_lastFrameTime).count();
    m_lastFrameTime = now;

    // Update camera based on input
    updateCamera(deltaTime);

    if (!m_renderContext || !m_renderContext->HasScene()) {
        // No scene loaded yet, just present empty frame
        m_window->frameReady();
        // Idle rather than spin. With nothing to accumulate, re-requesting a
        // frame forever only burns a core to redraw the same grid; every way
        // the picture can change from here (orbit, pan, zoom, a scene
        // arriving) goes through resetAccumulation(), which asks for a frame.
        // Held movement keys are the exception -- they are integrated over
        // deltaTime here, so they need the loop running to move at all.
        if (!m_paused && isCameraMoving()) {
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

    // A frame is drawn for two different reasons, and only one of them should
    // cost a sample. Accumulating is what Start/Resume asked for; redrawing
    // because the selection or the gizmo changed is the shell's own business,
    // and charging the user's sample budget for it would make the sample count
    // of a finished render depend on how often they clicked.
    //
    // So past the target, or while paused, the accumulated image is presented
    // again rather than re-traced. PresentAccumulated returns false when there
    // is nothing to re-present -- before the first trace, or after a resize --
    // and then a real frame is the only correct answer.
    const bool accumulating =
        !m_paused && !(m_targetSPP > 0 && m_sampleCount >= m_targetSPP);
    const auto width = static_cast<quantiloom::u32>(swapSize.width());
    const auto height = static_cast<quantiloom::u32>(swapSize.height());

    bool presentedWithoutTracing = false;
    if (!accumulating) {
        // A pending display reprocess re-runs the sensor chain and CLAHE over
        // the unchanged accumulation; the plain re-present shows the
        // post-processed images as they were. Same refusals, same fallback.
        presentedWithoutTracing = m_reprocessPending
            ? m_renderContext->ReprocessAccumulated(
                  cmd, targetImage,
                  VK_IMAGE_LAYOUT_UNDEFINED,  // Qt doesn't guarantee initial layout
                  width, height)
            : m_renderContext->PresentAccumulated(
                  cmd, targetImage,
                  VK_IMAGE_LAYOUT_UNDEFINED,
                  width, height);
    }
    // Consumed on every path: when a real frame is traced instead, RenderFrame
    // re-runs the post-processing chains anyway.
    m_reprocessPending = false;
    if (!presentedWithoutTracing) {
        // ExternalRenderContext handles layout transitions and blit to swapchain
        m_renderContext->RenderFrame(
            cmd,
            targetImage,
            VK_IMAGE_LAYOUT_UNDEFINED,
            width, height
        );
    }

    // Editor overlay (grid, gizmo) over the blitted frame, same command
    // buffer. No-op while nothing is visible.
    const vkview::CameraMatrices overlayCam = overlayCamera();
    m_gizmoVertices.clear();
    m_window->buildGizmoDrawList(overlayCam, m_gizmoVertices);
    m_overlay.record(m_window, m_renderContext.get(), cmd,
                     static_cast<uint32_t>(swapSize.width()),
                     static_cast<uint32_t>(swapSize.height()),
                     overlayCam, m_gizmoVertices);
    m_overlay.recordCaptureIfRequested(m_window, cmd,
                                       static_cast<uint32_t>(swapSize.width()),
                                       static_cast<uint32_t>(swapSize.height()));

    // Update sample count
    m_sampleCount = m_renderContext->GetAccumulatedSamples();

    // Two different quantities, and the status bar shows both. This one is the
    // interval between consecutive callbacks -- the cadence at which frames
    // actually reach the screen. Timing from here back to the top of the
    // function instead would measure command recording, which returns long
    // before the GPU has run any of it: microseconds, and a frame rate derived
    // from it read five figures.
    m_lastFrameTimeMs = deltaTime * 1000.0f;
    // The SDK's own figure, which times the trace rather than this thread's
    // command recording. It is the one to read when asking whether the *scene*
    // is expensive, and it had no caller at all until now.
    m_lastGpuFrameTimeMs = m_renderContext->GetLastFrameTimeMs();

    // Emit frame rendered signal
    emit m_window->frameRendered(m_lastFrameTimeMs, m_sampleCount);

    // Signal frame ready and request next frame. The frame in flight is always
    // completed -- stopping before the render would present an undefined
    // swapchain image -- so a pause takes effect from the next frame on.
    m_window->frameReady();
    const bool atTarget = m_targetSPP > 0 && m_sampleCount >= m_targetSPP;
    // A pending composited capture keeps the loop alive past the target: it
    // records in one frame and is written a few frames later, so stopping at
    // the target left the request recorded and never saved.
    if (!m_paused && (!atTarget || m_overlay.capturePending())) {
        m_window->requestUpdate();
    }
}

vkview::CameraMatrices QuantiloomVulkanRenderer::overlayCamera() const {
    if (m_renderContext) {
        const QSize size = m_window->swapChainImageSize();
        return vkview::CameraMatrices::fromCamera(
            m_renderContext->GetCamera(),
            static_cast<float>(size.width()), static_cast<float>(size.height()));
    }
    return {};
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

void QuantiloomVulkanRenderer::loadScene(const QString& filePath, bool adoptSceneCamera) {
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
        // A bare model is not a configuration, and the previous document's must
        // not outlive it: replaying it here is how one scene's illuminant and
        // spectral curves used to reach an unrelated model.
        if (adoptSceneCamera) {
            m_currentConfig.reset();
            m_currentConfigBaseDir.clear();
            m_configAppliedToContext = false;
        }

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
        // The atmosphere belongs in this list for the same reason as the rest:
        // a minimize destroys the render context, and the fresh one starts with
        // no model pack and a disabled header. The buffers rebound and the
        // scene rendered, so the only symptom was a sky that quietly stopped
        // being atmospheric until the preset was touched again.
        applyAtmosphereToContext();

        // The context adopts the camera the scene file carries, but nothing
        // told this class about it: the members kept on for the config-less
        // case, so the panel read 45 degrees while the SDK rendered the
        // scene's own FOV, and the first orbit drag snapped the view back to
        // the stale members. A config load calls setCamera() right after this
        // and wins, as it should -- the TOML is the document of record.
        if (adoptSceneCamera) {
            const quantiloom::Camera& sceneCamera = m_renderContext->GetCamera();
            m_cameraPosition = sceneCamera.GetPosition();
            m_cameraTarget = sceneCamera.GetLookAt();
            // The reference, not GetUp(): the derived up is orthonormalized for
            // this view direction, and adopting it froze one view's tilt into
            // m_cameraUp, where orbit turned it into roll and a save wrote it
            // to disk.
            m_cameraUp = sceneCamera.GetUpReference();
            m_cameraFovY = sceneCamera.GetFovY();

            // Orbit state is derived, and in radians -- see src/vulkan/CLAUDE.md.
            const glm::vec3 offset = m_cameraPosition - m_cameraTarget;
            m_orbitDistance = glm::length(offset);
            if (m_orbitDistance > 1e-6f) {
                const glm::vec3 dir = offset / m_orbitDistance;
                m_orbitPitch = std::asin(glm::clamp(dir.y, -1.0f, 1.0f));
                m_orbitYaw = std::atan2(dir.x, dir.z);
            }
            emit m_window->cameraChanged();
        }

        resetAccumulation();
        emit m_window->sceneLoaded(true, QObject::tr("Scene loaded successfully"));
    } else {
        qCritical() << "  Failed to load scene:" << QString::fromStdString(result.error());
        emit m_window->sceneLoaded(false,
            QObject::tr("Failed to load scene: %1").arg(QString::fromStdString(result.error())));
    }
}

void QuantiloomVulkanRenderer::applyConfig(std::shared_ptr<const quantiloom::Config> config,
                                           const QString& baseDir) {
    if (!config) {
        return;
    }

    m_currentConfig = std::move(config);
    m_currentConfigBaseDir = baseDir;
    m_configAppliedToContext = false;
    // Curves loaded from panels describe the document that was open, not this
    // one; carrying them over would replay them onto an unrelated scene.
    m_runtimeRefractiveIndices.clear();
    m_runtimeSpectralCurves.clear();
    m_solarLut.reset();
    m_solarLutSpec.reset();
    m_solarLutBaseDir.clear();
    // A config supersedes whatever bare model was open; the scene it names is
    // loaded as part of applying it.
    m_currentScenePath.clear();

    if (!m_renderContext) {
        // Same shape as a deferred scene load: replayed from
        // initSwapChainResources() once there is a context to apply it to.
        return;
    }

    applyConfigToContext(/*isFreshOpen=*/true);
}

void QuantiloomVulkanRenderer::applyConfigToContext(bool isFreshOpen) {
    if (!m_renderContext || !m_currentConfig) {
        return;
    }

    const bool compilingShaders = isFirstRun();
    if (compilingShaders) {
        emit m_window->longOperationStarted(
            QObject::tr("Compiling shaders — first run may take a few minutes"));
        QApplication::processEvents();
    }

    quantiloom::ConfigApplyOptions options;
    // An editor holds a document being written, where half-finished is a normal
    // state to be in. The CLI refuses the same file; saying what is missing and
    // showing the rest is more useful here.
    options.missingRequired =
        quantiloom::ConfigApplyOptions::MissingKeyPolicy::WarnAndDefault;
    options.baseDir = QDir::toNativeSeparators(m_currentConfigBaseDir).toStdString();
    options.atmosphereModelPackFallback = resolveDefaultModelPackDir();
    // Sun angles and observer altitude keep following the camera, which is what
    // an interactive viewport wants and what the CLI, rendering one fixed frame,
    // deliberately does not. Keys that state a geometry outright are honoured
    // either way.
    options.freezeDerivedAtmosGeometry = false;
    // The debug visualisation is a way of looking at a scene, not part of it
    // (src/config/CLAUDE.md).
    options.applyDebugMode = false;

    const quantiloom::ConfigApplyReport report =
        m_renderContext->ApplyConfig(*m_currentConfig, options);

    if (compilingShaders) {
        emit m_window->longOperationFinished();
    }

    if (!report.ok()) {
        const QString message = QString::fromStdString(report.FirstError());
        qCritical() << "  ApplyConfig failed:" << message;
        emit m_window->sceneLoaded(false,
            QObject::tr("Failed to load configuration: %1").arg(message));
        return;
    }

    // Whose values win depends on which of the two callers this is, and the
    // distinction is the document rather than the device.
    //
    // A fresh open: the file is the document of record, and this class's members
    // still describe the *previous* one. Take everything from the context, or
    // the last scene's sun follows the user into this one.
    //
    // A rebuild after a lost device: the same document, and the members are the
    // edits the user has made to it since opening. Those win -- but only after
    // ApplyConfig has run, because they are the only state this class holds and
    // the illuminant, spectral curves, refractive indices and temperature
    // backfill are not among them. Re-pushing members alone, which is what the
    // scene-path replay used to do, restored none of that.
    if (isFreshOpen) {
        m_lightingParams = m_renderContext->GetLightingParams();
        m_hasLightingParams = true;
        m_spectralMode = m_renderContext->GetSpectralMode();
        m_wavelength = m_renderContext->GetWavelength();
        m_targetSPP = m_renderContext->GetSPP();
        m_samplingSeed = m_renderContext->GetSamplingSeed();
        m_atmosphericConfig = m_renderContext->GetAtmosphere();

        const quantiloom::Camera& camera = m_renderContext->GetCamera();
        m_cameraPosition = camera.GetPosition();
        m_cameraTarget = camera.GetLookAt();
        // The reference, not GetUp() -- same reason as the adoptSceneCamera
        // read above.
        m_cameraUp = camera.GetUpReference();
        m_cameraFovY = camera.GetFovY();
        // Projection too, or the overlay keeps drawing its grid and gizmo in
        // perspective over an orthographic render -- which is visible as grid
        // lines converging to a vanishing point the scene does not have.
        m_orthographic =
            camera.GetProjection() == quantiloom::Camera::Projection::Orthographic;
        m_orthoHeight = camera.GetOrthoHeight();

        // Radians -- see src/vulkan/CLAUDE.md.
        const glm::vec3 offset = m_cameraPosition - m_cameraTarget;
        m_orbitDistance = glm::length(offset);
        if (m_orbitDistance > 1e-6f) {
            const glm::vec3 dir = offset / m_orbitDistance;
            m_orbitPitch = std::asin(glm::clamp(dir.y, -1.0f, 1.0f));
            m_orbitYaw = std::atan2(dir.x, dir.z);
        }
        emit m_window->cameraChanged();
    } else {
        if (m_hasLightingParams) {
            m_renderContext->SetLightingParams(m_lightingParams);
        }
        m_renderContext->SetSpectralMode(m_spectralMode);
        m_renderContext->SetWavelength(m_wavelength);
        m_renderContext->SetSPP(m_targetSPP);
        m_renderContext->SetSamplingSeed(m_samplingSeed);
        m_renderContext->SetDebugMode(m_debugMode);
        applyAtmosphereToContext();
        m_renderContext->SetCameraLookAt(m_cameraPosition, m_cameraTarget, m_cameraUp);
        m_renderContext->SetCameraFOV(m_cameraFovY);

        // Curves and the illuminant loaded from panels this session. ApplyConfig
        // above restored only the ones the file named, and these were appended
        // after it, so they are re-appended in the same order to keep the
        // indices materials hold pointing at the same curves.
        for (const auto& cri : m_runtimeRefractiveIndices) {
            m_renderContext->AddComplexRefractiveIndex(cri);
        }
        for (const auto& curve : m_runtimeSpectralCurves) {
            m_renderContext->AddSpectralCurve(curve);
        }
        // The spec wins when there is one: replaying the declaration re-reads
        // the file, which is what the config path would have done.
        if (m_solarLutSpec) {
            (void)m_renderContext->SetSolarSpectralLUTFromSpec(
                *m_solarLutSpec, m_solarLutBaseDir.toStdString());
        } else if (m_solarLut) {
            m_renderContext->SetSolarSpectralLUT(m_solarLut->first, m_solarLut->second);
        }
    }

    // Anything the config could not be honoured on, in the log next to the
    // messages the SDK already wrote there.
    for (const auto& message : report.messages) {
        if (message.severity == quantiloom::ConfigApplyMessage::Severity::Warning) {
            QL_LOG_WARN("Config: {}", message.text);
        }
    }

    m_configAppliedToContext = true;
    resetAccumulation();
    emit m_window->sceneLoaded(true, QObject::tr("Scene loaded successfully"));
}

void QuantiloomVulkanRenderer::setCameraProjection(bool orthographic, float orthoHeight) {
    m_orthographic = orthographic;
    if (orthoHeight > 0.0f) {
        m_orthoHeight = orthoHeight;
    } else if (orthographic) {
        // No height given: frame what the perspective camera framed, so
        // switching projection does not also change how much is in shot.
        m_orthoHeight = 2.0f * m_orbitDistance *
                        std::tan(glm::radians(m_cameraFovY) * 0.5f);
    }
    if (m_renderContext) {
        m_renderContext->SetCameraProjection(
            orthographic ? quantiloom::CameraProjection::Orthographic
                         : quantiloom::CameraProjection::Perspective,
            m_orthoHeight);
        resetAccumulation();
    }
    emit m_window->cameraChanged();
}

void QuantiloomVulkanRenderer::setSceneScale(float radius) {
    // One number from the scene's bounding sphere drives every navigation
    // constant. Guarded because an empty or degenerate scene has no radius,
    // and the old fixed values are a reasonable stand-in for "unknown".
    m_sceneRadius = (radius > 1e-4f && std::isfinite(radius)) ? radius : 0.0f;
}

float QuantiloomVulkanRenderer::minOrbitDistance() const {
    return (m_sceneRadius > 0.0f) ? m_sceneRadius * 0.01f : 0.1f;
}

float QuantiloomVulkanRenderer::maxOrbitDistance() const {
    return (m_sceneRadius > 0.0f) ? m_sceneRadius * 100.0f : 1000.0f;
}

float QuantiloomVulkanRenderer::cameraBaseSpeed() const {
    // Cross the scene in roughly two seconds at walking pace, which reads the
    // same whether the scene is a coin or a carrier.
    return (m_sceneRadius > 0.0f) ? m_sceneRadius : 5.0f;
}

void QuantiloomVulkanRenderer::frameBounds(const glm::vec3& min, const glm::vec3& max) {
    const glm::vec3 centre = (min + max) * 0.5f;
    const glm::vec3 extent = max - min;
    const float radius = glm::length(extent) * 0.5f;

    // A degenerate box (a single point, or a mesh with no bounds) still has a
    // centre worth orbiting; only the distance has nothing to say.
    const float safeRadius = (radius > 1e-4f) ? radius : 1.0f;

    // Far enough that the bounding sphere fits the vertical field of view,
    // with a margin so the object does not touch the frame edges.
    constexpr float kMargin = 1.35f;
    const float halfFov = m_cameraFovY * 0.5f;
    const float distance = (halfFov > 1e-4f)
        ? (safeRadius / std::sin(halfFov)) * kMargin
        : safeRadius * 3.0f;

    // Keep the direction the camera is already looking from -- framing moves
    // the pivot and the range, not the angle, which is what makes it feel
    // like "look at this" rather than "jump somewhere".
    glm::vec3 direction = m_cameraPosition - m_cameraTarget;
    if (glm::length(direction) < 1e-4f) {
        direction = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    direction = glm::normalize(direction);

    m_cameraTarget = centre;
    m_cameraPosition = centre + direction * distance;
    m_orbitDistance = distance;

    // Radians -- see src/vulkan/CLAUDE.md. Derived from the new offset rather
    // than left alone, or the first drag afterwards snaps back.
    m_orbitPitch = std::asin(glm::clamp(direction.y, -1.0f, 1.0f));
    m_orbitYaw = std::atan2(direction.x, direction.z);

    if (m_renderContext) {
        m_renderContext->SetCameraLookAt(m_cameraPosition, m_cameraTarget, m_cameraUp);
        resetAccumulation();
    }
    emit m_window->cameraChanged();
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
        // 0 means "infinite" in the UI; the SDK still needs a positive count.
        m_renderContext->SetSPP(spp > 0 ? spp : 1);
    }
    // Resume the render loop when the new target has not been reached yet.
    if (!m_paused && (spp == 0 || m_sampleCount < spp)) {
        m_window->requestUpdate();
    }
}

void QuantiloomVulkanRenderer::setSamplingSeed(uint32_t seed) {
    m_samplingSeed = seed;
    if (m_renderContext) {
        // SetSamplingSeed resets accumulation itself, so the next pass starts
        // from the new sequence rather than mixing two of them. Going through
        // resetAccumulation() too keeps this class's sample count in step and
        // restarts a loop that had stopped at its target.
        m_renderContext->SetSamplingSeed(seed);
        resetAccumulation();
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

// The three below are pools and an illuminant added at runtime, from panels
// rather than from the config. ApplyConfig restores only what the *file*
// declared, so each is kept here and re-pushed after a context rebuild --
// otherwise a minimise silently drops every curve the user has loaded this
// session, and the materials pointing at them fall back to their base colour.
// Order matters: replay appends in the same order, so the indices handed out
// the first time stay valid.

int QuantiloomVulkanRenderer::addComplexRefractiveIndex(const quantiloom::ComplexRefractiveIndex& cri) {
    m_runtimeRefractiveIndices.push_back(cri);
    if (m_renderContext)
        return m_renderContext->AddComplexRefractiveIndex(cri);
    return -1;
}

int QuantiloomVulkanRenderer::addSpectralCurve(const quantiloom::SpectralCurve& curve) {
    m_runtimeSpectralCurves.push_back(curve);
    if (m_renderContext)
        return m_renderContext->AddSpectralCurve(curve);
    return -1;
}

quantiloom::Result<int, std::string> QuantiloomVulkanRenderer::buildEndmemberWeightTexture(
    uint32_t materialIndex, const std::vector<glm::vec3>& endmemberColors) {
    if (!m_renderContext) {
        return quantiloom::Result<int, std::string>::Err("The render context is not ready yet.");
    }
    return m_renderContext->BuildEndmemberWeightTexture(materialIndex, endmemberColors);
}

std::optional<QString> QuantiloomVulkanRenderer::setSolarLutFromSpec(
    const quantiloom::SolarLutSpec& spec, const QString& baseDir) {
    // The declaration is kept, not the curves: a rebuild replays the same
    // reading rather than a snapshot of what it once produced.
    m_solarLutSpec = spec;
    m_solarLutBaseDir = baseDir;
    m_solarLut.reset();   // superseded; the spec is now the source of truth

    if (!m_renderContext) {
        return std::nullopt;   // replayed once there is a context
    }
    auto result = m_renderContext->SetSolarSpectralLUTFromSpec(
        spec, baseDir.toStdString());
    if (!result.has_value()) {
        return QString::fromStdString(result.error());
    }
    // The facade updated LightingParams from the spectrum; take it back so the
    // shell's copy and the panels do not disagree with the render. The caller
    // reads lightingParams() afterwards to refresh the widgets.
    m_lightingParams = m_renderContext->GetLightingParams();
    m_hasLightingParams = true;
    // Same reasoning as setSolarSpectralLUT below: a new illuminant changes
    // every pixel at once.
    resetAccumulation();
    return std::nullopt;
}

void QuantiloomVulkanRenderer::setSolarSpectralLUT(const quantiloom::SpectralCurve& sun,
                                                   const quantiloom::SpectralCurve& sky) {
    m_solarLut = std::make_pair(sun, sky);
    if (m_renderContext) {
        m_renderContext->SetSolarSpectralLUT(sun, sky);
        // Unlike the two pools above, this one changes every pixel at once.
        resetAccumulation();
    }
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

    // A key going down has to restart the loop: with no scene the frame
    // callback now idles, and these flags are only read from inside it.
    if (!m_paused && isCameraMoving()) {
        m_window->requestUpdate();
    }
}

bool QuantiloomVulkanRenderer::isCameraMoving() const {
    return m_moveForward || m_moveBackward || m_moveLeft ||
           m_moveRight || m_moveUp || m_moveDown;
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
    // How far one pixel of drag moves the world, derived rather than tuned. The
    // 0.01 world units per pixel that used to be here was a constant, so it was
    // only ever right at one distance: framing a 0.2-unit part of a model threw
    // it off screen in twenty pixels, and a landscape at distance 300 barely
    // shifted. Solving for the plane through the orbit target instead makes a
    // drag across the viewport pan exactly one viewport, at every distance and
    // every field of view -- the object stays under the cursor.
    //
    // deltaX/deltaY arrive in logical pixels (QuantiloomVulkanWindow::
    // m_lastMousePos), so the height must be logical too: QWindow::height(),
    // not swapChainImageSize(), which is device pixels and would divide the
    // scale factor back out of the result.
    const int viewportHeight = m_window ? m_window->height() : 0;
    if (viewportHeight <= 0) {
        return;  // No viewport yet; a drag cannot have come from one either.
    }
    const float halfFov = glm::radians(m_cameraFovY) * 0.5f;
    const float sensitivity =
        2.0f * m_orbitDistance * std::tan(halfFov) / static_cast<float>(viewportHeight);

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
    // Derived from the scene rather than the hardcoded 0.1 to 1000 that used
    // to be here: assets/models holds both an avocado a few centimetres
    // across and an aircraft carrier, and one pair of constants cannot serve
    // both -- the near clamp stopped the zoom well outside the avocado, and
    // the far one inside the carrier.
    m_orbitDistance = glm::clamp(m_orbitDistance, minOrbitDistance(), maxOrbitDistance());

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
    if (!isCameraMoving()) {
        return;
    }

    const float baseSpeed = cameraBaseSpeed();
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
    // Kick the loop back if it had auto-stopped at the previous target.
    if (!m_paused) {
        m_window->requestUpdate();
    }
}

namespace {

/// Trace times at or below this are already smooth enough that a softer image
/// would be a worse trade -- roughly 80 samples/s, which is where the scenes
/// this exists for are not.
constexpr float kMotionScaleFreeMs = 12.0f;
/// What a gesture is aimed at. Not the same number: the gap between them is
/// what keeps a scene sitting near the threshold from downgrading for nothing.
constexpr float kMotionScaleBudgetMs = 8.0f;
/// The scale is quantized to these so that repeating a gesture on the same
/// scene picks the same extent, and each step is a recognisable softness
/// rather than a continuum of slightly different ones.
constexpr float kMotionScaleSteps[] = {0.75f, 0.5f, 0.375f, 0.25f};

}  // namespace

float QuantiloomVulkanRenderer::chooseMotionScale() const {
    // The SDK's trace timing lags a frame or two, which is exactly right here:
    // read on the rising edge of a gesture it describes the steady state just
    // before it, not the transient the gesture is about to cause. Zero before
    // the first resolved timestamp, which reads as "fast" and downgrades
    // nothing -- the next gesture has a real figure.
    const float gpuMs = m_lastGpuFrameTimeMs;
    if (gpuMs <= kMotionScaleFreeMs) {
        return 1.0f;
    }
    // Pixels scale with the square of the linear extent, so the extent that
    // fits the budget is the square root of the ratio.
    const float ideal = std::sqrt(kMotionScaleBudgetMs / gpuMs);
    for (const float step : kMotionScaleSteps) {
        if (ideal >= step) {
            return step;
        }
    }
    return kMotionScaleSteps[std::size(kMotionScaleSteps) - 1];
}

void QuantiloomVulkanRenderer::applyRenderScale(float scale) {
    if (!m_renderContext || scale == m_renderContext->GetRenderScale()) {
        return;
    }
    m_renderContext->SetRenderScale(scale);
    // The extent changes on the next RenderFrame, and until it does the SDK
    // refuses to re-present -- so a loop stopped at its target has to be asked
    // for that frame, exactly like setGridVisible has to.
    if (!m_paused || m_sampleCount > 0) {
        m_window->requestUpdate();
    }
}

void QuantiloomVulkanRenderer::setViewportMotionActive(bool active) {
    if (active == m_motionActive) {
        return;
    }
    m_motionActive = active;
    if (!m_motionAdaptiveResolution) {
        return;
    }
    // Chosen once here and held: every change of extent costs a device wait
    // and throws the accumulation away, so following the trace time frame by
    // frame would spend the gesture resizing.
    m_motionScale = active ? chooseMotionScale() : 1.0f;
    applyRenderScale(m_motionScale);
}

void QuantiloomVulkanRenderer::setMotionAdaptiveResolution(bool enabled) {
    m_motionAdaptiveResolution = enabled;
    if (!enabled) {
        m_motionScale = 1.0f;
        applyRenderScale(1.0f);
    } else if (m_motionActive) {
        m_motionScale = chooseMotionScale();
        applyRenderScale(m_motionScale);
    }
}

float QuantiloomVulkanRenderer::currentRenderScale() const {
    return m_renderContext ? m_renderContext->GetRenderScale() : 1.0f;
}

QSize QuantiloomVulkanRenderer::currentRenderSize() const {
    if (!m_renderContext) {
        // Explicitly 0x0, not a default-constructed QSize: that one is -1x-1,
        // which reaches the status tool as a resolution of minus one pixel.
        return {0, 0};
    }
    return {static_cast<int>(m_renderContext->GetRenderWidth()),
            static_cast<int>(m_renderContext->GetRenderHeight())};
}

void QuantiloomVulkanRenderer::overrideRenderScale(float scale) {
    m_motionScale = scale;
    applyRenderScale(scale);
}

void QuantiloomVulkanRenderer::requestDisplayReprocess() {
    m_reprocessPending = true;
    // Unlike resetAccumulation(), pause is no reason to hold the frame: the
    // reprocess branch draws once without adding a sample and the loop stops
    // again. The one combination kept waiting -- paused with nothing traced
    // yet -- is the one where drawing would cost a sample, and also the one
    // with nothing to reprocess.
    if (!m_paused || m_sampleCount > 0) {
        m_window->requestUpdate();
    }
}

void QuantiloomVulkanRenderer::setGridVisible(bool visible) {
    m_overlay.setGridVisible(visible);
    // One redraw so the toggle shows after the loop stopped at its target.
    // Past-target frames re-present the accumulation, so this costs no
    // sample; the guard is the same trade as requestDisplayReprocess()'s.
    if (!m_paused || m_sampleCount > 0) {
        m_window->requestUpdate();
    }
}

bool QuantiloomVulkanRenderer::isFirstRun() const {
    // No cache file means no pipeline has ever been compiled on this machine,
    // so the next load pays for all of them.
    //
    // Ask the SDK where that file is rather than rebuilding the path here. The
    // reconstruction this replaced -- QStandardPaths::CacheLocation plus
    // "/Quantiloom/cache" -- double-counted the application name and produced
    // .../Local/blitzcolo/Quantiloom/cache/Quantiloom/cache/pipeline_cache.bin,
    // which cannot exist. isFirstRun() was therefore always true: every scene
    // load raised the "compiling shaders" banner and pumped the event loop,
    // however warm the cache actually was.
    if (!m_renderContext) {
        // Nothing has been created yet, so nothing can be cached.
        return true;
    }
    return !QFileInfo::exists(QString::fromStdString(m_renderContext->GetPipelineCachePath()));
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

bool QuantiloomVulkanRenderer::readApparentTemperature(int x, int y, double& outKelvin) {
    if (!m_renderContext) {
        return false;
    }
    // Only the fused thermal bands carry a band radiance to invert. RGB and
    // the visible band carry tristimulus, which is not a temperature of
    // anything.
    const auto band = quantiloom::GetFusedBandInfo(m_spectralMode);
    if (!band.has_value() || !quantiloom::IsIRFusedMode(m_spectralMode)) {
        return false;
    }

    auto result = m_renderContext->ReadPixelValue(static_cast<quantiloom::u32>(x),
                                                  static_cast<quantiloom::u32>(y));
    if (!result.has_value()) {
        return false;
    }

    // The accumulation is per-nm average spectral radiance, which is the unit
    // the SDK's band routines take. R is the whole of it in a thermal band.
    outKelvin = quantiloom::InvertSurfaceTemperatureK(
        static_cast<double>(result.value().r), static_cast<double>(band->lambdaMinNm),
        static_cast<double>(band->lambdaMaxNm), m_thermography);
    return true;
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

void QuantiloomVulkanRenderer::setAtmosphericConfig(const quantiloom::AtmosphereNNConfig& config) {
    m_atmosphericConfig = config;
    applyAtmosphereToContext();
    // SetAtmosphere resets accumulation on the SDK side when the config
    // changed; this keeps the sample count shown here in step and restarts a
    // loop that had stopped at its target.
    if (m_renderContext) {
        resetAccumulation();
    }
}

std::string QuantiloomVulkanRenderer::resolveDefaultModelPackDir() {
    QStringList candidates;
    QString envDir = qEnvironmentVariable("QUANTILOOM_ATMOS_MODELS");
    if (!envDir.isEmpty()) {
        candidates << envDir;
    }
    // Same three candidates in the same order as the core CLI. The
    // working-directory one was missing here, so a Studio launched from a repo
    // root -- where the CLI finds the pack and renders an atmosphere -- silently
    // disabled it instead.
    candidates << QDir::currentPath() + "/assets/atmos_models";
    // Copied next to the exe from the SDK by a POST_BUILD step
    candidates << QCoreApplication::applicationDirPath() + "/assets/atmos_models";

    for (const QString& dir : candidates) {
        if (QDir(dir).exists()) {
            // Qt-style forward slashes, which Windows accepts everywhere this
            // goes. This string ends up in saved configs via GetAtmosphere(),
            // and native backslashes made every reader deal with escaping.
            return dir.toStdString();
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
            QL_LOG_WARN("NN atmosphere model pack not found (set QUANTILOOM_ATMOS_MODELS "
                        "or pick a directory in the Atmospheric panel); disabling atmosphere");
            config.enabled = false;
        }
    }

    // These go through the core logger, not qWarning: with no console attached
    // Qt routes qWarning to the debugger, where it does not appear in the log
    // that carries the bake messages this would be read next to. A silently
    // disabled atmosphere is indistinguishable from one that never ran.
    try {
        m_renderContext->SetAtmosphere(config);
    } catch (const std::exception& e) {
        QL_LOG_WARN("Failed to apply NN atmosphere: {} - disabling atmosphere", e.what());
        config.enabled = false;
        try {
            m_renderContext->SetAtmosphere(config);
        } catch (const std::exception& e2) {
            QL_LOG_WARN("Failed to disable NN atmosphere: {}", e2.what());
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
// Thermal Solve
// ============================================================================

void QuantiloomVulkanRenderer::setThermalSolveParams(
    const quantiloom::ThermalSolveParams& params) {
    if (m_renderContext) {
        m_renderContext->SetThermalSolveParams(params);
        resetAccumulation();
    }
}

void QuantiloomVulkanRenderer::setThermalMaterial(
    const QString& name, const quantiloom::ThermalMaterialParams& params) {
    if (m_renderContext) {
        m_renderContext->SetThermalMaterial(name.toStdString(), params);
    }
}

void QuantiloomVulkanRenderer::clearThermalMaterials() {
    if (m_renderContext) {
        m_renderContext->ClearThermalMaterials();
    }
}

void QuantiloomVulkanRenderer::setThermalSolveEnabled(bool enabled) {
    if (m_renderContext) {
        m_renderContext->SetThermalSolveEnabled(enabled);
        resetAccumulation();
    }
}

void QuantiloomVulkanRenderer::setThermalTime(double time_h) {
    if (m_renderContext) {
        auto result = m_renderContext->SetThermalTime(time_h);
        if (!result) {
            qWarning() << "[Thermal] SetThermalTime failed:"
                       << QString::fromStdString(result.error());
        }
        resetAccumulation();
    }
}

quantiloom::ThermalSolveStatus QuantiloomVulkanRenderer::thermalSolveStatus() const {
    if (m_renderContext) {
        return m_renderContext->GetThermalSolveStatus();
    }
    return {};
}

quantiloom::Result<quantiloom::u32, quantiloom::String>
QuantiloomVulkanRenderer::thermalElementAt(const quantiloom::PickResult& pick) const {
    if (!m_renderContext) {
        return quantiloom::Result<quantiloom::u32, quantiloom::String>::Err("no renderer");
    }
    return m_renderContext->ThermalElementAt(pick);
}

quantiloom::Result<void, quantiloom::String> QuantiloomVulkanRenderer::setThermalWhatIf(
    const quantiloom::ThermalSensitivityParameter parameter, const double step) {
    if (!m_renderContext) {
        return quantiloom::Result<void, quantiloom::String>::Err("no renderer");
    }
    return m_renderContext->SetThermalWhatIf(parameter, step);
}

quantiloom::Result<quantiloom::ThermalElementTrajectory, quantiloom::String>
QuantiloomVulkanRenderer::elementTrajectory(const quantiloom::u32 element,
                                            const double fromHour, const double toHour,
                                            const quantiloom::u32 samples) {
    if (!m_renderContext) {
        return quantiloom::Result<quantiloom::ThermalElementTrajectory,
                                  quantiloom::String>::Err("no renderer");
    }
    return m_renderContext->GetElementTrajectory(element, fromHour, toHour, samples);
}

// ============================================================================
// Sensor Simulation
// ============================================================================

void QuantiloomVulkanRenderer::setSensorEnabled(bool enabled) {
    m_sensorEnabled = enabled;

    // Use GPU sensor via libQuantiloom (real-time)
    if (m_renderContext) {
        m_renderContext->SetGPUSensorEnabled(enabled);
        // Display-stage: the accumulation is still valid, so no
        // resetAccumulation() -- but the image on screen no longer shows the
        // current settings, and a loop stopped at its target would never
        // redraw it.
        requestDisplayReprocess();
    }

    qDebug() << "[Sensor] GPU sensor simulation" << (enabled ? "ENABLED" : "DISABLED");
}

void QuantiloomVulkanRenderer::setSensorParams(const quantiloom::SensorParams& params) {
    m_sensorParams = params;

    // Update GPU sensor params in libQuantiloom
    if (m_renderContext) {
        m_renderContext->SetGPUSensorParams(params);
        requestDisplayReprocess();
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

void QuantiloomVulkanRenderer::setDisplayEnhancement(
    const quantiloom::DisplayEnhancementParams& params) {
    m_displayParams = params;

    if (m_renderContext) {
        m_renderContext->SetDisplayEnhancementParams(params);
        requestDisplayReprocess();
    }

    qDebug() << "Display enhancement:" << (params.enabled ? "ENABLED" : "disabled")
             << "- tone=" << static_cast<int>(params.toneMode)
             << ", palette=" << static_cast<int>(params.palette)
             << ", clip=" << params.clipLimit
             << ", tiles=" << params.tileSize
             << ", window=[" << params.percentileLow << "," << params.percentileHigh << "]";
}
