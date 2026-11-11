/**
 * @file ViewportRenderer.cpp
 * @brief The viewport's device, QRhi presentation and SDK render context
 *
 * @author blitzcolo
 */

#include "ViewportRenderer.hpp"
#include "PixelReadbackConversion.hpp"
#include "ViewportWindow.hpp"
#include "../AssetPaths.hpp"

#include <renderer/ExternalRenderContext.hpp>
#include <renderer/LightingParams.hpp>
#include <atmos/AtmosphereNNConfig.hpp>
#include <core/Config.hpp>
#include <core/Log.hpp>
#include <scene/Material.hpp>
#include <scene/Scene.hpp>
#include <core/Image.hpp>
#include <core/SpectralData.hpp>

#include <rhi/qrhi.h>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QObject>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QScopedValueRollback>
#include <QTimer>
#include <cmath>
#include <exception>
#include <iterator>
#include <optional>
#include <thread>

#include <glm/gtc/matrix_transform.hpp>

ViewportRenderer::ViewportRenderer(ViewportWindow* window)
    : m_window(window)
    , m_lastFrameTime(std::chrono::high_resolution_clock::now())
{
    m_viewportSampleBatch.reset(framesInFlight());
}

ViewportRenderer::~ViewportRenderer() {
    releaseGpu();
}

int ViewportRenderer::framesInFlight() const {
    // QRhi's Vulkan backend keeps two; the same before the QRhi exists.
    return m_rhi ? m_rhi->resourceLimit(QRhi::FramesInFlight) : 2;
}

QSize ViewportRenderer::swapchainSize() const {
    return m_swapchain ? m_swapchain->currentPixelSize() : QSize();
}

void ViewportRenderer::initialize() {
    // The first scene of the session is usually in the deferred queue; the
    // loop must not turn while the device and the context are half set up,
    // so jobs started from here run inline.
    const QScopedValueRollback inFrameCallback(m_inFrameCallback, true);
    if (createGpu() && recreateSwapchain()) {
        createRenderContext();
    }
}

bool ViewportRenderer::createGpu() {
    if (!m_backend) {
        m_backend = viewport::createGpuBackend();
    }
    viewport::GpuBackend::RhiResult created = m_backend->createRhi(m_window);
    if (!created.rhi) {
        emit m_window->renderContextFailed(created.message);
        return false;
    }
    m_rhi = std::move(created.rhi);
    QL_LOG_INFO("Viewport: QRhi {} presents on the imported device, {} frames in flight",
                m_rhi->backendName(), framesInFlight());

    m_swapchain.reset(m_rhi->newSwapChain());
    m_swapchain->setWindow(m_window);
    // The frame texture is sRGB and so is this, so the composite samples
    // linear and the swapchain encodes on write. QRhi keeps a UNORM format
    // without a word when the surface offers no sRGB one; recreateSwapchain()
    // repeats its choice and has the composite encode instead.
    m_swapchain->setFlags(QRhiSwapChain::sRGB);
    m_swapchainPass.reset(m_swapchain->newCompatibleRenderPassDescriptor());
    m_swapchain->setRenderPassDescriptor(m_swapchainPass.get());
    m_swapchainDirty = true;
    m_viewportSampleBatch.reset(framesInFlight());
    return true;
}

bool ViewportRenderer::recreateSwapchain() {
    if (!m_swapchain || m_swapchain->surfacePixelSize().isEmpty()) {
        return false;  // hidden or minimized: frames wait for the next expose
    }
    const QSize before = m_swapchain->currentPixelSize();
    // Waits for the device to go idle first. That is why nothing but
    // renderFrame() and the first-time setup get here: neither runs while a
    // context job is submitting to the queue.
    if (!m_swapchain->createOrResize()) {
        qWarning() << "Viewport: could not build the swapchain at"
                   << m_swapchain->surfacePixelSize();
        return false;
    }
    m_swapchainDirty = false;

    const bool encode = !m_backend->surfaceOffersSrgb();
    if (encode != m_encodeSrgb || !m_swapchainFormatLogged) {
        m_swapchainFormatLogged = true;
        // Through the core logger, not qDebug: with no console attached
        // qDebug reaches the debugger and not the log, and a viewport showing
        // linear radiance uncorrected looks exactly like a correct one.
        if (encode) {
            QL_LOG_WARN("Viewport: the surface offers no sRGB format, so the composite "
                        "pass encodes linear radiance in the shader");
        } else {
            QL_LOG_INFO("Viewport: sRGB swapchain; the composite's writes are encoded");
        }
    }
    m_encodeSrgb = encode;

    if (m_swapchain->currentPixelSize() != before) {
        onSwapchainResized();
    }
    return true;
}

void ViewportRenderer::onSwapchainResized() {
    // A context already exists, so this is a rebuilt swapchain: the viewport
    // changed size -- a splitter, a workspace, the window -- or came back
    // after being hidden. That is a new window onto the render, not a new
    // render. The SDK stays at the extent it is accumulating at, and
    // renderFrame() fits that image into the swapchain until something
    // restarts the accumulation anyway, which is when resetAccumulation()
    // takes the new size up. Resizing here discarded every sample on each
    // step of a splitter drag. With nothing accumulated there is nothing to
    // keep, so the new size is taken at once.
    if (m_renderContext && (m_sampleCount == 0 || !m_renderContext->HasScene())) {
        adoptRenderExtent();
    }
}

void ViewportRenderer::createRenderContext() {
    m_contextAttempted = true;
    const QSize swapSize = swapchainSize();
    if (swapSize.isEmpty()) {
        qWarning() << "Invalid swapchain size, skipping initialization";
        m_contextAttempted = false;
        return;
    }

    auto created = m_backend->createContext(swapSize);
    if (!created.context) {
        qCritical() << "Failed to create ExternalRenderContext:" << created.error;
        emit m_window->renderContextFailed(
            QObject::tr("The renderer could not start:\n\n%1").arg(created.error));
        return;
    }

    qDebug() << "ExternalRenderContext created successfully!";

    m_renderContext = std::move(created.context);
    m_initialized = true;
    m_targetExtent = swapSize;

    // The epoch build is synchronous on this thread and can take minutes on a
    // large scene; this is how the shell gets to say which epoch it is on.
    // Registered once here rather than per scene, because the SDK keeps it
    // across scene loads and the preview it drives is rebuilt underneath.
    m_renderContext->SetThermalEpochProgressCallback(
        [this](quantiloom::u32 epoch, quantiloom::u32 count) {
            emit m_window->thermalEpochBuilding(static_cast<int>(epoch),
                                                static_cast<int>(count));
        });

    // Everything the shell configured before this point -- lighting, spectral
    // mode, sensor model, camera, environment map -- was queued by the window
    // because there was nothing to apply it to. Replay it now, before the
    // scene loads, so the first scene opened after launch renders with the
    // configuration it was opened with rather than with defaults.
    m_window->applyDeferredSettings();

    // Rebuild the open document. A lost device takes the context with it (a
    // minimize does not, and neither does a lost surface: only the swapchain
    // goes with that), and the fresh one starts at its constructor defaults
    // -- which for a config document
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
        pushCameraLookAt();
        m_renderContext->SetCameraFOV(m_cameraFovY);
        m_renderContext->SetCameraProjection(
            m_orthographic ? quantiloom::CameraProjection::Orthographic
                           : quantiloom::CameraProjection::Perspective,
            m_orthoHeight);
    }
}

void ViewportRenderer::joinContextJob() {
    if (m_jobThread && m_jobThread->joinable()) {
        m_jobThread->join();
    }
}

void ViewportRenderer::releaseGpu() {
    joinContextJob();
    if (m_rhi) {
        m_rhi->finish();
    }
    // Save current scene path for reload when the context is rebuilt
    if (!m_currentScenePath.isEmpty()) {
        m_pendingScenePath = m_currentScenePath;
        qDebug() << "Saved scene path for restore:" << m_pendingScenePath;
    }
    m_renderContext.reset();
    m_initialized = false;
    m_contextAttempted = false;
    m_targetExtent = QSize();
    // The next context is a fresh one and knows none of the document.
    m_configAppliedToContext = false;

    m_overlay.releaseResources();
    releaseFrameTargets();
    m_swapchain.reset();
    m_swapchainPass.reset();
    m_swapchainDirty = true;
    m_rhi.reset();
    if (m_backend) {
        m_backend->destroyDevice();
    }
}

void ViewportRenderer::rebuildAfterDeviceLoss() {
    QL_LOG_ERROR("Viewport: the Vulkan device was lost; rebuilding it and reopening the "
                 "document");
    releaseGpu();
    initialize();
    m_window->requestUpdate();
}

void ViewportRenderer::surfaceChanged() {
    m_swapchainDirty = true;
}

void ViewportRenderer::releaseSwapchain() {
    // destroy() waits for the device to go idle, and a context job may be
    // submitting right now: wait for it first. Blocking, and rare -- a window
    // close or a native re-creation in the middle of a job.
    joinContextJob();
    if (m_swapchain) {
        m_swapchain->destroy();
    }
    m_swapchainDirty = true;
}

bool ViewportRenderer::ensureFrameTargets(QSize extent) {
    if (m_frameTexture && m_frameTargetSize == extent) {
        return true;
    }
    m_frameTargetSize = QSize();
    if (!m_frameTexture) {
        // RenderTarget for the overlay pass, sRGB so the SDK's blit encodes
        // and sampling decodes, UsedAsTransferSource for the composited
        // capture. QRhi gives every Vulkan texture TRANSFER_DST, which is
        // what the SDK's blit needs.
        const QRhiTexture::Flags sdkWritten = m_backend->sdkWrittenTextureFlags();
        m_frameTexture.reset(m_rhi->newTexture(
            QRhiTexture::RGBA8, extent, 1,
            QRhiTexture::RenderTarget | QRhiTexture::sRGB | QRhiTexture::UsedAsTransferSource |
                sdkWritten));
        // R32_SFLOAT with TRANSFER_DST and SAMPLED: what BlitDepthTo requires.
        m_depthTexture.reset(m_rhi->newTexture(QRhiTexture::R32F, extent, 1, sdkWritten));
        // Orders overlay geometry against itself only; cleared by every pass.
        m_overlayDepth.reset(m_rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, extent));
    } else {
        m_frameTexture->setPixelSize(extent);
        m_depthTexture->setPixelSize(extent);
        m_overlayDepth->setPixelSize(extent);
    }
    if (!m_frameTexture->create() || !m_depthTexture->create() || !m_overlayDepth->create()) {
        qWarning() << "Viewport: could not create the frame textures at" << extent;
        return false;
    }
    if (!m_frameTarget) {
        QRhiTextureRenderTargetDescription description{QRhiColorAttachment(m_frameTexture.get())};
        description.setDepthStencilBuffer(m_overlayDepth.get());
        // Load and store the colour: the overlay draws over the SDK's frame.
        m_frameTarget.reset(m_rhi->newTextureRenderTarget(
            description, QRhiTextureRenderTarget::PreserveColorContents));
        m_framePass.reset(m_frameTarget->newCompatibleRenderPassDescriptor());
        m_frameTarget->setRenderPassDescriptor(m_framePass.get());
    }
    if (!m_frameTarget->create()) {
        qWarning() << "Viewport: could not create the overlay render target at" << extent;
        return false;
    }
    m_frameTargetSize = extent;
    return true;
}

void ViewportRenderer::releaseFrameTargets() {
    m_frameTarget.reset();
    m_framePass.reset();
    m_overlayDepth.reset();
    m_depthTexture.reset();
    m_frameTexture.reset();
    m_frameTargetSize = QSize();
}

void ViewportRenderer::renderFrame() {
    if (!m_rhi || !m_window->isExposed()) {
        return;  // no device (renderContextFailed said why), or nothing to show
    }
    // Rebuilt here and only here: createOrResize() waits for the device to go
    // idle, and this function never runs while a context job holds the queue.
    if (m_swapchainDirty || m_swapchain->currentPixelSize() != m_swapchain->surfacePixelSize()) {
        if (!recreateSwapchain()) {
            return;
        }
    }
    if (!m_contextAttempted) {
        // The first expose found no surface to size the context from.
        const QScopedValueRollback inFrameCallback(m_inFrameCallback, true);
        createRenderContext();
    }

    const QRhi::FrameOpResult begun = m_rhi->beginFrame(m_swapchain.get());
    if (begun == QRhi::FrameOpSwapChainOutOfDate) {
        m_swapchainDirty = true;
        m_window->requestUpdate();
        return;
    }
    if (begun == QRhi::FrameOpDeviceLost) {
        rebuildAfterDeviceLoss();
        return;
    }
    if (begun != QRhi::FrameOpSuccess) {
        qWarning() << "Viewport: beginFrame failed:" << begun;
        return;
    }

    const QScopedValueRollback inFrameCallback(m_inFrameCallback, true);
    m_frameCounter++;

    auto now = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(now - m_lastFrameTime).count();
    m_lastFrameTime = now;

    // Update camera based on input. May reset the accumulation and with it
    // adopt a new render extent, which drains the queue (QRhi::finish()) and
    // so replaces the frame's command buffer -- which is why the native
    // command buffer is not fetched until below.
    updateCamera(deltaTime);

    // A frame whose recording failed (an allocation failure, a lost device)
    // leaves the context loaded but not ready, and every later RenderFrame
    // returns without recording -- nothing would be in the frame texture to
    // composite. So a context in that state is drawn like no scene at all,
    // and said so once.
    const bool stopped = m_renderContext && m_renderContext->HasScene() &&
                         !m_renderContext->IsReady();
    if (stopped && !m_renderingStoppedReported) {
        m_renderingStoppedReported = true;
        emit m_window->renderingStopped();
    } else if (!stopped) {
        m_renderingStoppedReported = false;
    }

    QRhiCommandBuffer* cb = m_swapchain->currentFrameCommandBuffer();
    const QSize swapSize = m_swapchain->currentPixelSize();

    if (!m_renderContext || !m_renderContext->HasScene() || stopped) {
        // No scene loaded: present a cleared frame.
        cb->beginPass(m_swapchain->currentFrameRenderTarget(), Qt::black, {1.0f, 0});
        cb->endPass();
        m_rhi->endFrame(m_swapchain.get());
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
    if (m_frameCounter % 100 == 0) {
        qDebug() << "Frame" << m_frameCounter << "- samples:" << m_sampleCount;
    }

    // The SDK writes the frame texture, always at m_targetExtent -- which
    // while a resize is held is not the swapchain's -- and the composite pass
    // fits it into the swapchain. A resize changes how the render is shown,
    // never what it holds.
    const bool accumulating =
        !m_paused && !(m_targetSPP > 0 && m_sampleCount >= m_targetSPP);
    bool haveImage = false;
    bool tracedFrame = false;
    if (ensureFrameTargets(m_targetExtent)) {
        viewport::FrameRequest request;
        request.accumulating = accumulating;
        request.reprocess = m_reprocessPending;
        // Why paused, moving and physical-camera frames get exactly one
        // sample is documented on chooseForFrame().
        request.chooseSampleBatch = [this, accumulating] {
            return m_viewportSampleBatch.chooseForFrame(m_sampleCount, m_targetSPP, accumulating,
                                                        m_motionActive, m_cameraConfig.enabled);
        };
        const viewport::FrameResult frame =
            m_backend->recordFrame(cb, *m_renderContext, m_frameTexture.get(),
                                   m_depthTexture.get(), m_targetExtent, request);
        haveImage = frame.haveImage;
        tracedFrame = frame.traced;
    }
    // Consumed on every path: when a real frame is traced instead, RenderFrame
    // re-runs the post-processing chains anyway.
    m_reprocessPending = false;

    QRhiResourceUpdateBatch* updates = m_rhi->nextResourceUpdateBatch();
    if (haveImage) {
        // Editor overlay (grid, gizmo) over the SDK's frame. The pass runs
        // even with nothing to draw: it is the use the frame texture's layout
        // was left for (SdkFrameBridge.hpp).
        const viewport::CameraMatrices overlayCam = overlayCamera();
        m_gizmoVertices.clear();
        m_window->buildGizmoDrawList(overlayCam, m_gizmoVertices);
        m_overlay.prepare(m_rhi.get(), updates, m_framePass.get(), m_swapchainPass.get(),
                          m_frameTexture.get(), m_depthTexture.get(), overlayCam,
                          m_gizmoVertices, m_encodeSrgb);
        cb->beginPass(m_frameTarget.get(), Qt::black, {1.0f, 0}, updates);
        m_overlay.draw(cb, m_targetExtent);
        cb->endPass(m_overlay.takeCaptureReadback(m_rhi.get(), m_frameTexture.get()));
        updates = nullptr;
    }
    // The composite: black, then the frame in the rectangle it is shown in --
    // the bars of a held render are what is left of the clear.
    cb->beginPass(m_swapchain->currentFrameRenderTarget(), Qt::black, {1.0f, 0}, updates);
    if (haveImage) {
        const QRect shown = presentedRect();
        // QRhi viewports have their origin at the bottom left.
        m_overlay.composite(
            cb, QRhiViewport(static_cast<float>(shown.x()),
                             static_cast<float>(swapSize.height() - shown.y() - shown.height()),
                             static_cast<float>(shown.width()),
                             static_cast<float>(shown.height())));
    }
    cb->endPass();
    m_rhi->endFrame(m_swapchain.get());

    if (!haveImage) {
        // This frame's recording is the one that failed; a cleared frame went
        // out instead of an undefined one, and the next frame reports it.
        m_window->requestUpdate();
        return;
    }

    // Update sample count
    m_sampleCount = m_renderContext->GetAccumulatedSamples();

    // Two different quantities, and the status bar shows both. This one is the
    // interval between consecutive frames -- the cadence at which frames
    // actually reach the screen. Timing from here back to the top of the
    // function instead would measure command recording, which returns long
    // before the GPU has run any of it: microseconds, and a frame rate derived
    // from it read five figures.
    m_lastFrameTimeMs = deltaTime * 1000.0f;
    // The SDK's own figure, which times the trace rather than this thread's
    // command recording. It is the one to read when asking whether the *scene*
    // is expensive, and it had no caller at all until now.
    m_lastGpuFrameTimeMs = m_renderContext->GetLastFrameTimeMs();
    if (tracedFrame && accumulating && !m_motionActive && !m_cameraConfig.enabled) {
        m_viewportSampleBatch.observe(m_lastGpuFrameTimeMs);
    }

    // Emit frame rendered signal
    emit m_window->frameRendered(m_lastFrameTimeMs, m_sampleCount);

    // Request the next frame. A pause takes effect from the next frame on.
    const bool atTarget = m_targetSPP > 0 && m_sampleCount >= m_targetSPP;
    // A pending composited capture keeps the loop alive past the target and
    // through a pause: its readback completes only as later frames begin, so
    // stopping at either left the request armed, never saved, and every later
    // request refused as "one at a time". Past the target or paused these
    // frames re-present the accumulation, so the wait costs no sample.
    if (m_overlay.capturePending() || (!m_paused && !atTarget)) {
        m_window->requestUpdate();
    }
}

viewport::CameraMatrices ViewportRenderer::overlayCamera() const {
    if (m_renderContext) {
        // The extent the frame is rendered at, which is not the swapchain's
        // while a resize is being held.
        return viewport::CameraMatrices::fromCamera(
            m_renderContext->GetCamera(),
            static_cast<float>(m_targetExtent.width()),
            static_cast<float>(m_targetExtent.height()));
    }
    return {};
}

QRect ViewportRenderer::presentedRect() const {
    const QSize swapSize = swapchainSize();
    if (m_targetExtent.isEmpty() || swapSize.isEmpty()) {
        return QRect(QPoint(0, 0), swapSize);
    }
    // Fitted whole, aspect kept: the camera's aspect is the render's, so
    // stretching would distort and cropping would hide part of the image.
    const QSize fitted =
        m_targetExtent.scaled(swapSize, Qt::KeepAspectRatio).expandedTo(QSize(1, 1));
    return QRect(QPoint((swapSize.width() - fitted.width()) / 2,
                        (swapSize.height() - fitted.height()) / 2),
                 fitted);
}

QPointF ViewportRenderer::swapchainToTarget(const QPointF& swapchainPixel) const {
    const QRect shown = presentedRect();
    if (m_targetExtent.isEmpty() || shown.size() == m_targetExtent) {
        return swapchainPixel;
    }
    return QPointF((swapchainPixel.x() - shown.x()) * m_targetExtent.width() / shown.width(),
                   (swapchainPixel.y() - shown.y()) * m_targetExtent.height() /
                       shown.height());
}

void ViewportRenderer::adoptRenderExtent() {
    const QSize swapSize = swapchainSize();
    if (!m_renderContext || swapSize.isEmpty() || swapSize == m_targetExtent) {
        return;
    }
    // The SDK's Resize replaces images a frame still in flight may be
    // reading, so the queue is drained first; inside a frame QRhi submits
    // what is recorded and starts a new command buffer. The frame textures
    // follow on the next frame without a wait of their own: QRhi releases
    // the old images once no frame uses them. Once per resize, not per frame.
    m_rhi->finish();
    m_targetExtent = swapSize;
    m_renderContext->Resize(static_cast<quantiloom::u32>(swapSize.width()),
                            static_cast<quantiloom::u32>(swapSize.height()));
}

void ViewportRenderer::setPaused(bool paused) {
    if (m_paused == paused) {
        return;
    }
    m_paused = paused;
    if (!m_paused) {
        m_window->requestUpdate();
    }
}

namespace {
// An operation that ends within this never shows the busy card: a mode whose
// pipeline variant is already built switches in a hash lookup, and a card
// flashing up for that would be noise. Input arriving meanwhile is held back
// and delivered afterwards, which at this length reads as type-ahead.
constexpr int kBusyCardDelayMs = 300;
}  // namespace

void ViewportRenderer::runContextJob(
    const QString& description,
    const std::function<void(quantiloom::ExternalRenderContext&)>& work) {
    // Inside a frame or the first-time setup -- a lost device rebuilt and its
    // document replayed, or anything a frame triggers -- the event loop must
    // not turn: this window is part way through its own work.
    if (m_inFrameCallback) {
        work(*m_renderContext);
        return;
    }

    // The context goes to the worker with the work. Every path into it from
    // this thread checks for a context first, so whatever the event loop runs
    // meanwhile -- a timer, a queued signal -- finds none rather than racing a
    // worker the SDK does not lock against. Frames are held for the same
    // reason: they would submit to the queue the worker is using.
    m_contextJobActive = true;
    m_window->m_framesHeld = true;
    std::unique_ptr<quantiloom::ExternalRenderContext> context = std::move(m_renderContext);

    QEventLoop loop;
    bool done = false;
    std::exception_ptr failure;
    std::thread worker([&] {
        try {
            work(*context);
        } catch (...) {
            failure = std::current_exception();
        }
        QMetaObject::invokeMethod(&loop, [&] {
            done = true;
            loop.quit();
        }, Qt::QueuedConnection);
    });
    m_jobThread = &worker;

    // Unconnected: it is only there to wake the wait at the deadline.
    QTimer deadline;
    deadline.setSingleShot(true);
    deadline.start(kBusyCardDelayMs);
    QElapsedTimer clock;
    clock.start();
    while (!done && !clock.hasExpired(kBusyCardDelayMs)) {
        loop.processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::WaitForMoreEvents);
    }
    const bool cardShown = !done;
    if (cardShown) {
        emit m_window->longOperationStarted(description);
        loop.exec();
    }
    // Joined already if the surface went away meanwhile (releaseSwapchain()).
    if (worker.joinable()) {
        worker.join();
    }
    m_jobThread = nullptr;

    m_renderContext = std::move(context);
    m_contextJobActive = false;
    m_window->m_framesHeld = false;
    if (cardShown) {
        emit m_window->longOperationFinished();
    }
    // A gesture that ended meanwhile could not hand its scale back.
    applyRenderScale(m_motionActive && m_motionAdaptiveResolution ? m_motionScale : 1.0f);
    m_window->requestUpdate();
    if (failure) {
        std::rethrow_exception(failure);
    }
}

void ViewportRenderer::loadScene(const QString& filePath, bool adoptSceneCamera) {
    qDebug() << "ViewportRenderer::loadScene() - Path:" << filePath;

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
    // window had even appeared; it is the busy card's title now.
    const bool compilingShaders = isFirstRun();
    if (compilingShaders) {
        qDebug() << "  First run detected - shaders will be compiled";
    }

    const bool usd = filePath.endsWith(".usd", Qt::CaseInsensitive) ||
                     filePath.endsWith(".usda", Qt::CaseInsensitive) ||
                     filePath.endsWith(".usdc", Qt::CaseInsensitive) ||
                     filePath.endsWith(".usdz", Qt::CaseInsensitive);
    quantiloom::Result<void, quantiloom::String> result;

    // The stored settings are re-applied inside the job, copied in: the new
    // pipeline starts on its default variant, and setting the mode and the
    // debug view is what builds theirs -- seconds on a cold driver cache.
    runContextJob(
        compilingShaders ? QObject::tr("Compiling shaders — first run may take a few minutes")
                         : tr("Loading scene\u2026"),
        [&result, usd, path = filePath.toStdString(),
         lighting = m_hasLightingParams ? std::optional(m_lightingParams) : std::nullopt,
         mode = m_spectralMode, debugView = m_debugMode, spp = m_targetSPP,
         seed = m_samplingSeed, wavelengthNm = m_wavelength](quantiloom::ExternalRenderContext& context) {
            result = usd ? context.LoadSceneFromUsd(path) : context.LoadSceneFromGltf(path);
            if (!result) {
                return;
            }
            // Re-apply stored render settings (important after a context rebuild)
            if (lighting) {
                context.SetLightingParams(*lighting);
            }
            context.SetSpectralMode(mode);
            context.SetDebugMode(debugView);
            context.SetSPP(spp);
            context.SetSamplingSeed(seed);
            context.SetWavelength(wavelengthNm);
        });

    qDebug() << "  Scene load returned";

    if (result) {
        qDebug() << "  Scene loaded successfully!";
        m_currentScenePath = filePath;  // Save for a context rebuild
        // A bare model is not a configuration, and the previous document's must
        // not outlive it: replaying it here is how one scene's illuminant and
        // spectral curves used to reach an unrelated model.
        if (adoptSceneCamera) {
            m_currentConfig.reset();
            m_currentConfigBaseDir.clear();
            m_configAppliedToContext = false;
        }

        // The atmosphere is re-applied too, for the same reason as the rest:
        // a lost device destroys the render context, and the fresh one starts with
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

            // Orbit state is derived, and in radians -- see src/viewport/AGENTS.md.
            // GetUp() is the resolved film up, which is what says which way a
            // straight-down scene camera faces.
            const glm::vec3 offset = m_cameraPosition - m_cameraTarget;
            m_orbitDistance = glm::length(offset);
            if (m_orbitDistance > 1e-6f) {
                setOrbitAnglesFromView(offset / m_orbitDistance, sceneCamera.GetUp());
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

void ViewportRenderer::applyConfig(std::shared_ptr<const quantiloom::Config> config,
                                           const QString& baseDir) {
    if (!config) {
        return;
    }

    const auto previousConfig = m_currentConfig;
    const auto previousBaseDir = m_currentConfigBaseDir;
    const auto previousScenePath = m_currentScenePath;
    const bool previouslyApplied = m_configAppliedToContext;
    m_currentConfig = std::move(config);
    m_currentConfigBaseDir = baseDir;
    m_configAppliedToContext = false;
    if (!m_renderContext) return;
    if (!applyConfigToContext(/*isFreshOpen=*/true)) {
        // The SDK validates the replacement before adopting its scene. Keep
        // the previous document's replay state and runtime bindings on failure.
        m_currentConfig = previousConfig;
        m_currentConfigBaseDir = previousBaseDir;
        m_currentScenePath = previousScenePath;
        m_configAppliedToContext = previouslyApplied;
    }
}

bool ViewportRenderer::applyConfigToContext(bool isFreshOpen) {
    if (!m_renderContext || !m_currentConfig) {
        return false;
    }

    const bool compilingShaders = isFirstRun();

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
    // (src/config/AGENTS.md).
    options.applyDebugMode = false;

    // The scene, its textures, the pipeline and -- in a thermal scene -- the
    // whole solve: tens of seconds on a large site.
    std::optional<quantiloom::ConfigApplyReport> applied;
    runContextJob(
        compilingShaders ? QObject::tr("Compiling shaders — first run may take a few minutes")
                         : tr("Loading scene\u2026"),
        [&applied, &options, config = m_currentConfig](quantiloom::ExternalRenderContext& context) {
            applied.emplace(context.ApplyConfig(*config, options));
        });
    const quantiloom::ConfigApplyReport& report = *applied;

    if (!report.ok()) {
        const QString message = QString::fromStdString(report.FirstError());
        qCritical() << "  ApplyConfig failed:" << message;
        emit m_window->sceneLoaded(false,
            QObject::tr("Failed to load configuration: %1").arg(message));
        return false;
    }

    if (isFreshOpen) {
        m_currentScenePath.clear();
        m_runtimeRefractiveIndices.clear();
        m_runtimeSpectralCurves.clear();
        m_solarLut.reset();
        m_solarLutSpec.reset();
        m_solarLutBaseDir.clear();
        m_solarLutCleared = false;
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

        // Radians -- see src/viewport/AGENTS.md.
        const glm::vec3 offset = m_cameraPosition - m_cameraTarget;
        m_orbitDistance = glm::length(offset);
        if (m_orbitDistance > 1e-6f) {
            setOrbitAnglesFromView(offset / m_orbitDistance, camera.GetUp());
        }
        emit m_window->cameraChanged();

        // The SDK parsed and applied the versioned camera itself; read its
        // resolution back so the shell's copy matches what is running -- the
        // unversioned sensor settings it replaced could not carry it.
        m_cameraConfig = m_renderContext->GetCameraConfig();
        m_sensorEnabled = m_cameraConfig.enabled;
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
        pushCameraLookAt();
        m_renderContext->SetCameraFOV(m_cameraFovY);
        // The projection is session state too: the rebuilt context starts in
        // perspective, while the overlay (grid, gizmo) keeps m_orthographic
        // and would be drawn over a render that no longer matches it.
        m_renderContext->SetCameraProjection(
            m_orthographic ? quantiloom::CameraProjection::Orthographic
                           : quantiloom::CameraProjection::Perspective,
            m_orthoHeight);
        // The camera the user configured rides over the file's, the same way
        // the lighting params do. A camera that is off applied nothing this
        // session, so the file's own (already applied) configuration stands.
        if (m_cameraConfig.enabled) {
            if (auto updated = m_renderContext->SetCameraConfig(m_cameraConfig);
                !updated) {
                qWarning() << "  camera config re-push failed:"
                           << QString::fromStdString(updated.error());
            }
        }

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
        } else if (m_solarLutCleared) {
            m_renderContext->ClearSolarSpectralLUT();
        }
    }

    // Anything the config could not be honoured on, in the log next to the
    // messages the SDK already wrote there -- and the first of them in the
    // status bar. The log is stdout, which nobody sees who did not start Studio
    // from a console, and a config that loads with a warning is one that
    // renders something other than what it says: a global irradiance read as
    // the sky lights the whole scene with the sun twice and nothing else does.
    //
    // Every warning rides along after the first line, which the status bar
    // keeps for its tooltip (MainWindow::showStatusMessage).
    QStringList warnings;
    for (const auto& message : report.messages) {
        if (message.severity == quantiloom::ConfigApplyMessage::Severity::Warning) {
            QL_LOG_WARN("Config: {}", message.text);
            warnings << QString::fromStdString(message.text).trimmed();
        }
    }

    m_configAppliedToContext = true;
    resetAccumulation();
    emit m_window->sceneLoaded(true,
        warnings.isEmpty()
            ? QObject::tr("Scene loaded successfully")
            : QObject::tr("Scene loaded with %n warning(s): %1", nullptr,
                          static_cast<int>(warnings.size()))
                      .arg(warnings.first()) +
                  QLatin1Char('\n') + warnings.join(QLatin1Char('\n')));
    return true;
}

void ViewportRenderer::setCameraProjection(bool orthographic, float orthoHeight) {
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

void ViewportRenderer::setSceneScale(float radius) {
    // One number from the scene's bounding sphere drives every navigation
    // constant. Guarded because an empty or degenerate scene has no radius,
    // and the old fixed values are a reasonable stand-in for "unknown".
    m_sceneRadius = (radius > 1e-4f && std::isfinite(radius)) ? radius : 0.0f;
}

float ViewportRenderer::minOrbitDistance() const {
    return (m_sceneRadius > 0.0f) ? m_sceneRadius * 0.01f : 0.1f;
}

float ViewportRenderer::maxOrbitDistance() const {
    return (m_sceneRadius > 0.0f) ? m_sceneRadius * 100.0f : 1000.0f;
}

float ViewportRenderer::cameraBaseSpeed() const {
    // Cross the scene in roughly two seconds at walking pace, which reads the
    // same whether the scene is a coin or a carrier.
    return (m_sceneRadius > 0.0f) ? m_sceneRadius : 5.0f;
}

void ViewportRenderer::frameBounds(const glm::vec3& min, const glm::vec3& max) {
    const glm::vec3 centre = (min + max) * 0.5f;
    const glm::vec3 extent = max - min;
    const float radius = glm::length(extent) * 0.5f;

    // A degenerate box (a single point, or a mesh with no bounds) still has a
    // centre worth orbiting; only the distance has nothing to say.
    const float safeRadius = (radius > 1e-4f) ? radius : 1.0f;

    // Far enough that the bounding sphere fits the vertical field of view,
    // with a margin so the object does not touch the frame edges.
    constexpr float kMargin = 1.35f;
    // m_cameraFovY is degrees (Camera::GetFovY); std::sin wants radians.
    const float halfFov = glm::radians(m_cameraFovY) * 0.5f;
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

    // Radians -- see src/viewport/AGENTS.md. Derived from the new offset rather
    // than left alone, or the first drag afterwards snaps back. The direction
    // is unchanged, so on the pole the heading it is shown with is too.
    setOrbitAnglesFromView(direction, orbitViewUp());

    if (m_renderContext) {
        pushCameraLookAt();
        resetAccumulation();
    }
    emit m_window->cameraChanged();
}

void ViewportRenderer::resetCamera() {
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
        pushCameraLookAt();
        resetAccumulation();
    }
    emit m_window->cameraChanged();
}

void ViewportRenderer::setCamera(const glm::vec3& position, const glm::vec3& lookAt,
                                          const glm::vec3& up, float fovY,
                                          const glm::vec3& viewUpHint) {
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
    //
    // The pose goes to the SDK as given, with the document's view_up as the
    // only hint: what it renders must be what the CLI renders from the same
    // file. The yaw is then read back from the up the SDK resolved, so that a
    // straight-down pose -- whose direction carries no heading -- continues
    // into the orbit facing the way it is shown.
    if (m_renderContext) {
        m_renderContext->SetCameraLookAt(m_cameraPosition, m_cameraTarget, m_cameraUp, viewUpHint);
        m_renderContext->SetCameraFOV(fovY);
    }
    if (m_orbitDistance > 1e-6f) {
        setOrbitAnglesFromView((position - lookAt) / m_orbitDistance,
                               m_renderContext ? m_renderContext->GetCamera().GetUp() : up);
    }
    if (m_renderContext) {
        resetAccumulation();
    }

    qDebug() << "Camera set: pos=(" << position.x << "," << position.y << "," << position.z
             << ") lookAt=(" << lookAt.x << "," << lookAt.y << "," << lookAt.z
             << ") fov=" << fovY;

    emit m_window->cameraChanged();
}

void ViewportRenderer::getCameraState(glm::vec3& position, glm::vec3& target,
                                              glm::vec3& up, float& fovY) const {
    position = m_cameraPosition;
    target = m_cameraTarget;
    up = m_cameraUp;
    fovY = m_cameraFovY;
}

void ViewportRenderer::setCameraFovY(float fovY) {
    m_cameraFovY = fovY;
    if (m_renderContext) {
        m_renderContext->SetCameraFOV(fovY);
        resetAccumulation();
    }
    emit m_window->cameraChanged();
}

void ViewportRenderer::setViewDirection(const glm::vec3& direction) {
    const float length = glm::length(direction);
    if (length < 1e-6f) {
        return;
    }
    const glm::vec3 dir = direction / length;

    // Same radians-everywhere convention as orbitCamera(): these two feed
    // sin/cos directly and are clamped against half_pi. Top and bottom come
    // out at yaw 0, atan2(0, 0): the heading the front view has, so they
    // face the way an orbit up or down from the front arrives at.
    m_orbitPitch = std::asin(glm::clamp(dir.y, -1.0f, 1.0f));
    m_orbitYaw = std::atan2(dir.x, dir.z);
    m_cameraPosition = m_cameraTarget + dir * m_orbitDistance;

    if (m_renderContext) {
        pushCameraLookAt();
        resetAccumulation();
    }
    emit m_window->cameraChanged();
}

void ViewportRenderer::setSPP(uint32_t spp) {
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

void ViewportRenderer::setSamplingSeed(uint32_t seed) {
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

void ViewportRenderer::setWavelength(float wavelength_nm) {
    m_wavelength = wavelength_nm;
    if (m_renderContext) {
        m_renderContext->SetWavelength(wavelength_nm);
        resetAccumulation();
    }
}

void ViewportRenderer::setSpectralMode(quantiloom::SpectralMode mode) {
    m_spectralMode = mode;  // Store for restore
    if (m_renderContext) {
        // The first switch to a mode since the scene loaded builds its pipeline
        // variant; every later one is a lookup.
        runContextJob(tr("Switching spectral mode\u2026"),
                      [mode](quantiloom::ExternalRenderContext& context) {
                          context.SetSpectralMode(mode);
                      });
        resetAccumulation();
    }
}

void ViewportRenderer::setDebugMode(quantiloom::DebugVisualizationMode mode) {
    m_debugMode = mode;  // Store for restore
    if (m_renderContext) {
        // Same variant cache as the spectral mode: debug on and off are two.
        runContextJob(tr("Switching debug visualization\u2026"),
                      [mode](quantiloom::ExternalRenderContext& context) {
                          context.SetDebugMode(mode);
                      });
        resetAccumulation();
    }
}

void ViewportRenderer::setLightingParams(const quantiloom::LightingParams& params) {
    m_lightingParams = params;  // Store for restore
    m_hasLightingParams = true;
    if (m_renderContext) {
        m_renderContext->SetLightingParams(params);
        resetAccumulation();
    }
}

void ViewportRenderer::updateMaterial(int index, const quantiloom::Material& material) {
    if (m_renderContext && index >= 0) {
        m_renderContext->UpdateMaterial(static_cast<quantiloom::u32>(index), material);
        resetAccumulation();
    }
}

// The three below are pools and an illuminant added at runtime, from panels
// rather than from the config. ApplyConfig restores only what the *file*
// declared, so each is kept here and re-pushed after a context rebuild --
// otherwise a rebuild silently drops every curve the user has loaded this
// session, and the materials pointing at them fall back to their base colour.
// Order matters: replay appends in the same order, so the indices handed out
// the first time stay valid.

int ViewportRenderer::addComplexRefractiveIndex(const quantiloom::ComplexRefractiveIndex& cri) {
    m_runtimeRefractiveIndices.push_back(cri);
    if (m_renderContext)
        return m_renderContext->AddComplexRefractiveIndex(cri);
    return -1;
}

int ViewportRenderer::addSpectralCurve(const quantiloom::SpectralCurve& curve) {
    m_runtimeSpectralCurves.push_back(curve);
    if (m_renderContext)
        return m_renderContext->AddSpectralCurve(curve);
    return -1;
}

quantiloom::Result<int, std::string> ViewportRenderer::buildEndmemberWeightTexture(
    uint32_t materialIndex, const std::vector<glm::vec3>& endmemberColors) {
    if (!m_renderContext) {
        return quantiloom::Result<int, std::string>::Err("The render context is not ready yet.");
    }
    return m_renderContext->BuildEndmemberWeightTexture(materialIndex, endmemberColors);
}

std::optional<QString> ViewportRenderer::setSolarLutFromSpec(
    const quantiloom::SolarLutSpec& spec, const QString& baseDir) {
    // The declaration is kept, not the curves: a rebuild replays the same
    // reading rather than a snapshot of what it once produced.
    m_solarLutSpec = spec;
    m_solarLutBaseDir = baseDir;
    m_solarLut.reset();   // superseded; the spec is now the source of truth
    m_solarLutCleared = false;

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

void ViewportRenderer::clearSolarLut() {
    m_solarLut.reset();
    m_solarLutSpec.reset();
    m_solarLutBaseDir.clear();
    m_solarLutCleared = true;
    if (m_renderContext) {
        m_renderContext->ClearSolarSpectralLUT();
        // Same reasoning as setSolarSpectralLUT below: the sun and sky going
        // out changes every pixel at once.
        resetAccumulation();
    }
}

void ViewportRenderer::setSolarSpectralLUT(const quantiloom::SpectralCurve& sun,
                                                   const quantiloom::SpectralCurve& sky) {
    m_solarLut = std::make_pair(sun, sky);
    m_solarLutCleared = false;
    if (m_renderContext) {
        m_renderContext->SetSolarSpectralLUT(sun, sky);
        // Unlike the two pools above, this one changes every pixel at once.
        resetAccumulation();
    }
}

const quantiloom::Scene* ViewportRenderer::getScene() const {
    return m_renderContext ? m_renderContext->GetScene() : nullptr;
}

void ViewportRenderer::getCameraInfo(glm::vec3& position, glm::vec3& forward,
                                              glm::vec3& right, glm::vec3& up) const {
    position = m_cameraPosition;
    forward = glm::normalize(m_cameraTarget - m_cameraPosition);
    viewBasis(right, up);
}

glm::vec3 ViewportRenderer::orbitViewUp() const {
    // d/dpitch of the orbit offset (cos p sin y, sin p, cos p cos y). Away from
    // the poles this is exactly the up a +Y reference resolves to, so passing
    // it changes nothing there; at a pole it is horizontal and turns with yaw.
    return glm::vec3(-std::sin(m_orbitPitch) * std::sin(m_orbitYaw),
                     std::cos(m_orbitPitch),
                     -std::sin(m_orbitPitch) * std::cos(m_orbitYaw));
}

void ViewportRenderer::setOrbitAnglesFromView(const glm::vec3& dir,
                                                      const glm::vec3& shownUp) {
    m_orbitPitch = std::asin(glm::clamp(dir.y, -1.0f, 1.0f));

    // Off the pole the direction has the heading. On it, its horizontal part
    // is rounding left over from position - target, and atan2 of that is
    // noise -- the yaw it produced was what the next drag snapped to.
    constexpr float kVertical = 1e-3f;
    if (std::hypot(dir.x, dir.z) > kVertical) {
        m_orbitYaw = std::atan2(dir.x, dir.z);
        return;
    }

    // There the film's up is horizontal and is the heading: orbitViewUp() is
    // (-sin y, 0, -cos y) looking down and (sin y, 0, cos y) looking up.
    const float sign = (dir.y > 0.0f) ? -1.0f : 1.0f;
    const float hx = sign * shownUp.x;
    const float hz = sign * shownUp.z;
    if (std::hypot(hx, hz) > kVertical) {
        m_orbitYaw = std::atan2(hx, hz);
    }
    // Otherwise nothing says which way the view faces; keep the heading.
}

glm::vec3 ViewportRenderer::viewUp() const {
    glm::vec3 right;
    glm::vec3 up;
    viewBasis(right, up);
    return up;
}

void ViewportRenderer::viewBasis(glm::vec3& right, glm::vec3& up) const {
    // The basis the frame on screen was rendered with, when there is one --
    // the same source CameraMatrices reads, and well defined straight down,
    // where cross(forward, m_cameraUp) is zero or rounding noise.
    if (m_renderContext) {
        const quantiloom::Camera& camera = m_renderContext->GetCamera();
        right = camera.GetRight();
        up = camera.GetUp();
        return;
    }
    right = glm::vec3(std::cos(m_orbitYaw), 0.0f, -std::sin(m_orbitYaw));
    up = orbitViewUp();
}

void ViewportRenderer::pushCameraLookAt() {
    // m_cameraUp stays the reference -- what a save writes and what motion
    // poses are built from. The orbit's own up only rides along as the hint
    // the SDK consults when the view is parallel to it: without it every
    // straight-down view faced one fixed axis, so an orbit reaching the pole
    // snapped to that orientation and could not turn about the vertical.
    m_renderContext->SetCameraLookAt(m_cameraPosition, m_cameraTarget, m_cameraUp,
                                     orbitViewUp());
}

void ViewportRenderer::updateCameraMovement(
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

bool ViewportRenderer::isCameraMoving() const {
    return m_moveForward || m_moveBackward || m_moveLeft ||
           m_moveRight || m_moveUp || m_moveDown;
}

void ViewportRenderer::orbitCamera(float deltaX, float deltaY) {
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
        pushCameraLookAt();
        resetAccumulation();
    }
    emit m_window->cameraChanged();
}

void ViewportRenderer::panCamera(float deltaX, float deltaY) {
    // How far one pixel of drag moves the world, derived rather than tuned. The
    // 0.01 world units per pixel that used to be here was a constant, so it was
    // only ever right at one distance: framing a 0.2-unit part of a model threw
    // it off screen in twenty pixels, and a landscape at distance 300 barely
    // shifted. Solving for the plane through the orbit target instead makes a
    // drag across the viewport pan exactly one viewport, at every distance and
    // every field of view -- the object stays under the cursor.
    //
    // deltaX/deltaY arrive in logical pixels (ViewportWindow::
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

    // The picture's own right and up, so a drag moves the scene the way the
    // cursor goes at every orientation. Straight down, cross(forward,
    // m_cameraUp) is zero (NaN after normalize) or rounding noise that swings
    // with yaw; the basis the frame was rendered with is neither.
    glm::vec3 right;
    glm::vec3 up;
    viewBasis(right, up);

    glm::vec3 pan = -right * deltaX * sensitivity + up * deltaY * sensitivity;
    m_cameraPosition += pan;
    m_cameraTarget += pan;

    if (m_renderContext) {
        pushCameraLookAt();
        resetAccumulation();
    }
    emit m_window->cameraChanged();
}

void ViewportRenderer::zoomCamera(float delta) {
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
        pushCameraLookAt();
        resetAccumulation();
    }
    emit m_window->cameraChanged();
}

void ViewportRenderer::updateCamera(float deltaTime) {
    if (!isCameraMoving()) {
        return;
    }

    const float baseSpeed = cameraBaseSpeed();
    float speed = m_moveFast ? baseSpeed * 3.0f : baseSpeed;

    glm::vec3 forward = glm::normalize(m_cameraTarget - m_cameraPosition);
    // Not cross(forward, m_cameraUp): that is zero straight down. Strafing
    // follows the picture's right; up and down below stay world-vertical.
    glm::vec3 right;
    glm::vec3 filmUp;
    viewBasis(right, filmUp);

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
            pushCameraLookAt();
            resetAccumulation();
        }
        emit m_window->cameraChanged();
    }
}

void ViewportRenderer::resetAccumulation() {
    m_sampleCount = 0;
    m_viewportSampleBatch.reset(framesInFlight());
    if (m_renderContext) {
        // A restart is when a size the viewport changed to while the old
        // image was held gets taken up: nothing is lost that is not being
        // discarded anyway.
        adoptRenderExtent();
        m_renderContext->ResetAccumulation();
    }
    emit m_window->pixelReadbackInvalidated();
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

float ViewportRenderer::chooseMotionScale() const {
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

void ViewportRenderer::applyRenderScale(float scale) {
    if (!m_renderContext || scale == m_renderContext->GetRenderScale()) {
        return;
    }
    const bool shouldRedraw = !m_paused || m_sampleCount > 0;
    m_renderContext->SetRenderScale(scale);
    // The SDK resets its image for the new extent. Mirror that reset locally,
    // including the timing warmup, so low-resolution gesture timings cannot
    // size the first full-resolution batch.
    resetAccumulation();
    // The extent changes on the next RenderFrame, and until it does the SDK
    // refuses to re-present -- so a loop stopped at its target has to be asked
    // for that frame, exactly like setGridVisible has to.
    if (shouldRedraw) {
        m_window->requestUpdate();
    }
}

void ViewportRenderer::setViewportMotionActive(bool active) {
    if (active == m_motionActive) {
        return;
    }
    m_motionActive = active;
    m_viewportSampleBatch.reset(framesInFlight());
    if (!m_motionAdaptiveResolution) {
        return;
    }
    // Chosen once here and held: every change of extent costs a device wait
    // and throws the accumulation away, so following the trace time frame by
    // frame would spend the gesture resizing.
    m_motionScale = active ? chooseMotionScale() : 1.0f;
    applyRenderScale(m_motionScale);
}

void ViewportRenderer::setMotionAdaptiveResolution(bool enabled) {
    m_motionAdaptiveResolution = enabled;
    if (!enabled) {
        m_motionScale = 1.0f;
        applyRenderScale(1.0f);
    } else if (m_motionActive) {
        m_motionScale = chooseMotionScale();
        applyRenderScale(m_motionScale);
    }
}

float ViewportRenderer::currentRenderScale() const {
    return m_renderContext ? m_renderContext->GetRenderScale() : 1.0f;
}

QSize ViewportRenderer::currentRenderSize() const {
    if (!m_renderContext) {
        // Explicitly 0x0, not a default-constructed QSize: that one is -1x-1,
        // which reaches the status tool as a resolution of minus one pixel.
        return {0, 0};
    }
    return {static_cast<int>(m_renderContext->GetRenderWidth()),
            static_cast<int>(m_renderContext->GetRenderHeight())};
}

void ViewportRenderer::overrideRenderScale(float scale) {
    m_motionScale = scale;
    applyRenderScale(scale);
}

void ViewportRenderer::requestDisplayReprocess() {
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

void ViewportRenderer::setGridVisible(bool visible) {
    m_overlay.setGridVisible(visible);
    // One redraw so the toggle shows after the loop stopped at its target.
    // Past-target frames re-present the accumulation, so this costs no
    // sample; the guard is the same trade as requestDisplayReprocess()'s.
    if (!m_paused || m_sampleCount > 0) {
        m_window->requestUpdate();
    }
}

bool ViewportRenderer::isFirstRun() const {
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

bool ViewportRenderer::readDebugPixel(int x, int y, glm::vec4& outValue) {
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

quantiloom::Result<bool, quantiloom::String>
ViewportRenderer::requestPixelValue(quantiloom::u32 x, quantiloom::u32 y,
                                            quantiloom::u64 requestId) {
    if (!m_renderContext) {
        return quantiloom::Result<bool, quantiloom::String>::Err("no render context");
    }
    return m_renderContext->RequestPixelValue(x, y, requestId);
}

quantiloom::Result<quantiloom::Optional<quantiloom::PixelReading>, quantiloom::String>
ViewportRenderer::pollPixelValue() {
    using Result = quantiloom::Result<quantiloom::Optional<quantiloom::PixelReading>,
                                      quantiloom::String>;
    if (!m_renderContext) {
        return Result::Err("no render context");
    }
    return m_renderContext->PollPixelValue();
}

bool ViewportRenderer::apparentTemperatureFromPixel(
    const glm::vec4& pixel, double& outKelvin) const {
    const auto temperature = viewport::apparentTemperatureK(
        m_spectralMode, pixel, m_thermography);
    if (!temperature) return false;
    outKelvin = *temperature;
    return true;
}

QString ViewportRenderer::formatDebugValue(const glm::vec4& v) const {
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

std::unique_ptr<quantiloom::Image> ViewportRenderer::captureScreenshot() {
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

std::unique_ptr<quantiloom::Image> ViewportRenderer::captureDisplayImage() {
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

void ViewportRenderer::setAtmosphericConfig(const quantiloom::AtmosphereNNConfig& config) {
    m_atmosphericConfig = config;
    applyAtmosphereToContext();
    // SetAtmosphere resets accumulation on the SDK side when the config
    // changed; this keeps the sample count shown here in step and restarts a
    // loop that had stopped at its target.
    if (m_renderContext) {
        resetAccumulation();
    }
}

std::string ViewportRenderer::resolveDefaultModelPackDir() {
    // Same three candidates in the same order as the core CLI:
    // assetpaths::atmosphereModelPackDir.
    return assetpaths::atmosphereModelPackDir().toStdString();
}

void ViewportRenderer::applyAtmosphereToContext() {
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

bool ViewportRenderer::loadEnvironmentMap(const QString& hdrPath) {
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

bool ViewportRenderer::hasEnvironmentMap() const {
    return m_renderContext && m_renderContext->HasEnvironmentMap();
}

// ============================================================================
// Thermal Solve
// ============================================================================

void ViewportRenderer::setThermalSolveParams(
    const quantiloom::ThermalSolveParams& params) {
    if (m_renderContext) {
        m_renderContext->SetThermalSolveParams(params);
        resetAccumulation();
    }
}

void ViewportRenderer::setThermalMaterial(
    const QString& name, const quantiloom::ThermalMaterialParams& params) {
    if (m_renderContext) {
        m_renderContext->SetThermalMaterial(name.toStdString(), params);
    }
}

void ViewportRenderer::clearThermalMaterials() {
    if (m_renderContext) {
        m_renderContext->ClearThermalMaterials();
    }
}

void ViewportRenderer::setThermalSolveEnabled(bool enabled) {
    if (m_renderContext) {
        m_renderContext->SetThermalSolveEnabled(enabled);
        resetAccumulation();
    }
}

void ViewportRenderer::setThermalTime(double time_h) {
    if (m_renderContext) {
        // Along a day already solved this is a replay from a checkpoint; the
        // first hour asked for after the solve's inputs changed is the solve.
        quantiloom::Result<void, quantiloom::String> result;
        runContextJob(tr("Solving the thermal field\u2026"),
                      [&result, time_h](quantiloom::ExternalRenderContext& context) {
                          result = context.SetThermalTime(time_h);
                      });
        if (!result) {
            qWarning() << "[Thermal] SetThermalTime failed:"
                       << QString::fromStdString(result.error());
        }
        resetAccumulation();
    }
}

std::optional<float> ViewportRenderer::derivedShortwaveAbsorptivity(const QString& material) const {
    if (!m_renderContext) {
        return std::nullopt;
    }
    return m_renderContext->GetDerivedShortwaveAbsorptivity(material.toStdString());
}

QString ViewportRenderer::referenceSunConflict(
    const quantiloom::AtmosphereNNConfig& config) const {
    if (!m_renderContext) {
        return {};
    }
    return QString::fromStdString(m_renderContext->ReferenceSunConflict(config));
}

quantiloom::ThermalSolveStatus ViewportRenderer::thermalSolveStatus() const {
    if (m_renderContext) {
        return m_renderContext->GetThermalSolveStatus();
    }
    return {};
}

void ViewportRenderer::setTimelineTime(double time_s) {
    if (!m_renderContext) {
        return;
    }
    auto result = m_renderContext->SetTimelineTime(time_s);
    if (!result) {
        qWarning() << "[Timeline] SetTimelineTime failed:"
                   << QString::fromStdString(result.error());
    }
    // The SDK already reset accumulation for the paths that changed anything;
    // this covers the one where it did not, and costs nothing when it did.
    resetAccumulation();
}

quantiloom::TimelineInfo ViewportRenderer::timelineInfo() const {
    if (m_renderContext) {
        return m_renderContext->GetTimelineInfo();
    }
    return {};
}

void ViewportRenderer::setTimelineThermalMapping(double hourAtStart_h, double scale) {
    if (!m_renderContext) {
        return;
    }
    auto result = m_renderContext->SetTimelineThermalMapping(hourAtStart_h, scale);
    if (!result) {
        qWarning() << "[Timeline] SetTimelineThermalMapping failed:"
                   << QString::fromStdString(result.error());
        return;
    }
    resetAccumulation();
}

glm::mat4 ViewportRenderer::nodeRestTransform(int nodeIndex) const {
    if (!m_renderContext || nodeIndex < 0) {
        return glm::mat4(1.0f);
    }
    return m_renderContext->GetNodeRestTransform(static_cast<quantiloom::u32>(nodeIndex));
}

quantiloom::Result<quantiloom::u32, quantiloom::String>
ViewportRenderer::thermalElementAt(const quantiloom::PickResult& pick) const {
    if (!m_renderContext) {
        return quantiloom::Result<quantiloom::u32, quantiloom::String>::Err("no renderer");
    }
    return m_renderContext->ThermalElementAt(pick);
}

void ViewportRenderer::setDebugParameter(const quantiloom::u32 value) {
    if (m_renderContext) {
        m_renderContext->SetDebugParameter(value);
        resetAccumulation();
    }
}

quantiloom::Result<void, quantiloom::String> ViewportRenderer::setThermalWhatIf(
    const quantiloom::ThermalSensitivityParameter parameter, const double step) {
    if (!m_renderContext) {
        return quantiloom::Result<void, quantiloom::String>::Err("no renderer");
    }
    auto result = m_renderContext->SetThermalWhatIf(parameter, step);
    if (result) resetAccumulation();
    return result;
}

quantiloom::Result<quantiloom::ThermalElementTrajectory, quantiloom::String>
ViewportRenderer::elementTrajectory(const quantiloom::u32 element,
                                            const double fromHour, const double toHour,
                                            const quantiloom::u32 samples) {
    if (!m_renderContext) {
        return quantiloom::Result<quantiloom::ThermalElementTrajectory,
                                  quantiloom::String>::Err("no renderer");
    }
    // Every sample is the whole field stepped to that hour from a checkpoint.
    std::optional<quantiloom::Result<quantiloom::ThermalElementTrajectory, quantiloom::String>>
        trajectory;
    runContextJob(tr("Replaying this surface's thermal day\u2026"),
                  [&trajectory, element, fromHour, toHour,
                   samples](quantiloom::ExternalRenderContext& context) {
                      trajectory.emplace(
                          context.GetElementTrajectory(element, fromHour, toHour, samples));
                  });
    return std::move(*trajectory);
}

// ============================================================================
// Sensor Simulation
// ============================================================================

void ViewportRenderer::setSensorEnabled(bool enabled) {
    m_sensorEnabled = enabled;

    // Use GPU sensor via libQuantiloom (real-time)
    if (m_renderContext) {
        m_renderContext->SetGPUSensorEnabled(enabled);
        // Turning the camera off is display-stage: the visibility
        // accumulation is still valid, so it is re-presented, not re-traced.
        // Turning it on is not: the camera's acquisition starts with no paths
        // in it, and a loop already stopped at its target re-presented one
        // that did not exist -- a black frame -- and never traced the first.
        if (enabled) {
            resetAccumulation();
        } else {
            requestDisplayReprocess();
        }
    }

    qDebug() << "[Sensor] GPU sensor simulation" << (enabled ? "ENABLED" : "DISABLED");
}

// ============================================================================
// Physical camera
// ============================================================================

quantiloom::Result<void, quantiloom::String> ViewportRenderer::setCameraConfig(
    const quantiloom::camera::CameraConfig& config) {
    if (!m_renderContext) {
        return quantiloom::Result<void, quantiloom::String>::Err("no renderer");
    }
    auto readout = m_renderContext->TryUpdateCameraReadoutConfig(config);
    if (!readout) return quantiloom::Result<void, quantiloom::String>::Err(
        readout.error());
    if (readout.value()) {
        m_cameraConfig = config;
        m_sensorEnabled = config.enabled;
        // Core has already rerun detector/ADC/ISP from the standing measured
        // rate. Present that product, including the persistent CLAHE branch,
        // without tracing or advancing the acquisition index.
        requestDisplayReprocess();
        return quantiloom::Result<void, quantiloom::String>::Ok();
    }
    auto applied = m_renderContext->SetCameraConfig(config);
    if (!applied) {
        qWarning() << "[Camera] SetCameraConfig failed:"
                   << QString::fromStdString(applied.error());
        return applied;
    }
    m_cameraConfig = config;
    m_sensorEnabled = config.enabled;
    // Tier-3 changes rebuild the measurement on the SDK side: the acquisition
    // starts again with no paths in it, so the count the loop stops on starts
    // again too. Re-presenting instead showed a loop stopped at its target an
    // acquisition that no longer existed -- black -- and never traced the new
    // one. With the camera off nothing was acquired, and a redraw suffices.
    if (config.enabled) {
        resetAccumulation();
    } else {
        requestDisplayReprocess();
    }
    return quantiloom::Result<void, quantiloom::String>::Ok();
}

quantiloom::Result<void, quantiloom::String>
ViewportRenderer::reprocessCameraDisplay(
    const quantiloom::camera::CameraConfig& config) {
    if (!m_renderContext) {
        return quantiloom::Result<void, quantiloom::String>::Err("no renderer");
    }
    auto applied = m_renderContext->UpdateCameraDisplayConfig(config);
    if (!applied) {
        return applied;
    }
    m_cameraConfig = config;
    m_sensorEnabled = config.enabled;
    // The queued present re-runs display processing over the last completed
    // acquisition. This also works before the first capture: the first frame
    // uses the new settings without resetting the scheduler or device state.
    requestDisplayReprocess();
    return quantiloom::Result<void, quantiloom::String>::Ok();
}

quantiloom::CameraHistoryStatus ViewportRenderer::cameraHistoryStatus() const {
    return m_renderContext ? m_renderContext->GetCameraHistoryStatus()
                           : quantiloom::CameraHistoryStatus{};
}

quantiloom::Result<quantiloom::camera::CameraOutput, quantiloom::String>
ViewportRenderer::captureCameraProducts(double timeSeconds) {
    if (!m_renderContext) {
        return quantiloom::Result<quantiloom::camera::CameraOutput,
                                  quantiloom::String>::Err("no renderer");
    }
    // Commit one explicit acquisition at the requested time so the products
    // below describe this moment, not whatever frame happened to complete
    // last. Errors (e.g. the camera disabled) propagate to the caller.
    auto advanced = m_renderContext->AdvanceCameraTo(timeSeconds);
    if (!advanced) {
        return quantiloom::Result<quantiloom::camera::CameraOutput,
                                  quantiloom::String>::Err(advanced.error());
    }
    return m_renderContext->CaptureCameraProducts();
}

void ViewportRenderer::setDisplayEnhancement(
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
