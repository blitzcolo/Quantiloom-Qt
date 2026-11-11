/**
 * @file ViewportWindow.cpp
 * @brief The native window the viewport renders into
 *
 * @author blitzcolo
 */

#include "ViewportWindow.hpp"
#include "ViewportRenderer.hpp"

#include <postprocess/Thermography.hpp>
#include "../editing/SelectionManager.hpp"
#include "../editing/TransformGizmo.hpp"
#include "../editing/UndoStack.hpp"
#include "../editing/Commands.hpp"

#include <core/Config.hpp>
#include <core/Image.hpp>
#include <core/Log.hpp>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QHoverEvent>
#include "GpuBackend.hpp"

#include <QExposeEvent>
#include <QPlatformSurfaceEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QDebug>

#include <renderer/ExternalRenderContext.hpp>
#include <renderer/LightingParams.hpp>
#include <scene/Material.hpp>
#include <scene/Scene.hpp>

ViewportWindow::ViewportWindow(QWindow* parent)
    : QWindow(parent)
{
    // A Vulkan surface for QRhi's swapchain. Nothing here is released when
    // the window is hidden: a minimize hides it (QWidget hides its children,
    // and the window container hides this window with them), and rebuilding
    // the context on every restore reloaded the whole scene and lost the
    // accumulation. A hidden window only skips frames.
    setSurfaceType(viewport::presentSurfaceType());
}

ViewportWindow::~ViewportWindow() {
    // First, while every member the teardown might touch still exists: the
    // renderer drains the queue and destroys the device it created.
    m_renderer.reset();
}

void ViewportWindow::exposeEvent(QExposeEvent* event) {
    Q_UNUSED(event);
    if (!isExposed()) {
        return;  // hidden or minimized: frames are skipped, nothing released
    }
    if (!m_renderer) {
        // The queued settings are *not* replayed here: the renderer calls
        // applyDeferredSettings() once it has a render context.
        m_renderer = std::make_unique<ViewportRenderer>(this);
        m_renderer->initialize();
    } else {
        m_renderer->surfaceChanged();
    }
    requestUpdate();
}

void ViewportWindow::resizeEvent(QResizeEvent* event) {
    Q_UNUSED(event);
    if (m_renderer) {
        m_renderer->surfaceChanged();
    }
    requestUpdate();
}

void ViewportWindow::applyDeferredSettings() {
    if (!m_renderer || m_deferredCalls.empty() || m_deferredReplayQueued) {
        return;
    }
    // From the event loop, not from here: this is called inside the
    // renderer's first-time setup, where the loop must not turn, and the
    // queue usually holds the first scene of the session -- the load most
    // worth running off the GUI thread (ViewportRenderer::runContextJob).
    m_deferredReplayQueued = true;
    QMetaObject::invokeMethod(this, [this]() {
        m_deferredReplayQueued = false;
        // Settings first, scene second: the renderer keeps each setting as a
        // member and re-applies it after a scene loads, so this order is the
        // one that survives the load.
        const auto calls = std::move(m_deferredCalls);
        m_deferredCalls.clear();
        for (const auto& call : calls) {
            call(*m_renderer);
        }
    }, Qt::QueuedConnection);
}

void ViewportWindow::withRenderer(std::function<void(ViewportRenderer&)> call) {
    if (m_renderer && !m_deferredReplayQueued) {
        call(*m_renderer);
        return;
    }
    m_deferredCalls.push_back(std::move(call));
}

bool ViewportWindow::isRendererBusy() const {
    return m_renderer && m_renderer->contextBusy();
}

void ViewportWindow::loadScene(const QString& filePath) {
    // Queued like a config before the renderer exists, so that the first model
    // of a session loads from the event loop too.
    withRenderer([filePath](ViewportRenderer& renderer) { renderer.loadScene(filePath); });
}

void ViewportWindow::applyConfig(std::shared_ptr<const quantiloom::Config> config,
                                         const QString& baseDir) {
    // Through withRenderer() rather than a second pending-path member: the
    // renderer holds the config either way and replays it once it has a
    // context, so there is nothing for this class to remember.
    withRenderer([config = std::move(config), baseDir](ViewportRenderer& renderer) {
        renderer.applyConfig(config, baseDir);
    });
}

void ViewportWindow::resetCamera() {
    withRenderer([](ViewportRenderer& r) { r.resetCamera(); });
}

void ViewportWindow::setCamera(const glm::vec3& position, const glm::vec3& lookAt,
                                        const glm::vec3& up, float fovY,
                                        const glm::vec3& viewUpHint) {
    withRenderer([position, lookAt, up, fovY, viewUpHint](ViewportRenderer& r) {
        r.setCamera(position, lookAt, up, fovY, viewUpHint);
    });
}

glm::vec3 ViewportWindow::viewUp() const {
    return m_renderer ? m_renderer->viewUp() : glm::vec3(0.0f);
}

void ViewportWindow::getCameraState(glm::vec3& position, glm::vec3& target,
                                            glm::vec3& up, float& fovY) const {
    if (m_renderer) {
        m_renderer->getCameraState(position, target, up, fovY);
    } else {
        position = glm::vec3(0.0f, 1.0f, 5.0f);
        target = glm::vec3(0.0f);
        up = glm::vec3(0.0f, 1.0f, 0.0f);
        fovY = 45.0f;
    }
}

void ViewportWindow::setCameraFovY(float fovY) {
    withRenderer([fovY](ViewportRenderer& r) { r.setCameraFovY(fovY); });
}

void ViewportWindow::setViewDirection(const glm::vec3& direction) {
    withRenderer([direction](ViewportRenderer& r) { r.setViewDirection(direction); });
}

void ViewportWindow::setSPP(uint32_t spp) {
    withRenderer([spp](ViewportRenderer& r) { r.setSPP(spp); });
}

void ViewportWindow::setSamplingSeed(uint32_t seed) {
    withRenderer([seed](ViewportRenderer& r) { r.setSamplingSeed(seed); });
}

void ViewportWindow::setWavelength(float wavelength_nm) {
    withRenderer([wavelength_nm](ViewportRenderer& r) { r.setWavelength(wavelength_nm); });
}

uint32_t ViewportWindow::currentSampleCount() const {
    return m_renderer ? m_renderer->currentSampleCount() : 0;
}

void ViewportWindow::frameBounds(const glm::vec3& min, const glm::vec3& max) {
    if (m_renderer) {
        m_renderer->frameBounds(min, max);
    }
}

bool ViewportWindow::sensorEnabled() const {
    return m_renderer && m_renderer->isSensorEnabled();
}

void ViewportWindow::setCameraProjection(bool orthographic, float orthoHeight) {
    if (m_renderer) {
        m_renderer->setCameraProjection(orthographic, orthoHeight);
    }
}

bool ViewportWindow::cameraIsOrthographic() const {
    return m_renderer && m_renderer->cameraIsOrthographic();
}

float ViewportWindow::cameraOrthoHeight() const {
    return m_renderer ? m_renderer->cameraOrthoHeight() : 2.0f;
}

void ViewportWindow::setSceneScale(float radius) {
    if (m_renderer) {
        m_renderer->setSceneScale(radius);
    }
}

float ViewportWindow::lastGpuFrameTimeMs() const {
    return m_renderer ? m_renderer->lastGpuFrameTimeMs() : 0.0f;
}

quantiloom::LightingParams ViewportWindow::lightingParams() const {
    return m_renderer ? m_renderer->lightingParams()
                      : quantiloom::CreateDefaultLightingParams();
}

quantiloom::SpectralMode ViewportWindow::spectralMode() const {
    return m_renderer ? m_renderer->spectralMode() : quantiloom::SpectralMode::RGB;
}

float ViewportWindow::wavelength() const {
    return m_renderer ? m_renderer->wavelength() : 550.0f;
}

quantiloom::AtmosphereNNConfig ViewportWindow::atmosphericConfig() const {
    return m_renderer ? m_renderer->atmosphericConfig() : quantiloom::AtmosphereNNConfig{};
}

quantiloom::DebugVisualizationMode ViewportWindow::debugMode() const {
    return m_renderer ? m_renderer->getDebugMode() : quantiloom::DebugVisualizationMode::None;
}

uint32_t ViewportWindow::targetSPP() const {
    return m_renderer ? m_renderer->targetSPP() : 0;
}

uint32_t ViewportWindow::samplingSeed() const {
    return m_renderer ? m_renderer->samplingSeed() : quantiloom::constants::DEFAULT_SAMPLING_SEED;
}

void ViewportWindow::setRenderPaused(bool paused) {
    withRenderer([paused](ViewportRenderer& r) { r.setPaused(paused); });
}

bool ViewportWindow::isRenderPaused() const {
    return m_renderer && m_renderer->isPaused();
}

QPointF ViewportWindow::toTargetPixels(const QPointF& logical) const {
    const qreal ratio = devicePixelRatio();
    const QPointF device(logical.x() * ratio, logical.y() * ratio);
    return m_renderer ? m_renderer->swapchainToTarget(device) : device;
}

void ViewportWindow::reportHover(const QPointF& pixel) {
    // Over the bars around a held render there is no pixel to read, which is
    // what leaving the viewport already means to the readout.
    const QSize extent = m_renderer ? m_renderer->targetExtent() : QSize();
    if (pixel.x() < 0.0 || pixel.y() < 0.0 || pixel.x() >= extent.width() ||
        pixel.y() >= extent.height()) {
        emit mouseHoverLeft();
        return;
    }
    emit mouseHovered(static_cast<int>(pixel.x()), static_cast<int>(pixel.y()));
}

void ViewportWindow::setSpectralMode(quantiloom::SpectralMode mode) {
    withRenderer([mode](ViewportRenderer& r) { r.setSpectralMode(mode); });
}

void ViewportWindow::setDebugMode(quantiloom::DebugVisualizationMode mode) {
    withRenderer([mode](ViewportRenderer& r) { r.setDebugMode(mode); });
}

void ViewportWindow::setLightingParams(const quantiloom::LightingParams& params) {
    withRenderer([params](ViewportRenderer& r) { r.setLightingParams(params); });
}

void ViewportWindow::updateMaterial(int index, const quantiloom::Material& material) {
    if (m_renderer) {
        m_renderer->updateMaterial(index, material);
    }
}

int ViewportWindow::addComplexRefractiveIndex(const quantiloom::ComplexRefractiveIndex& cri) {
    if (m_renderer)
        return m_renderer->addComplexRefractiveIndex(cri);
    return -1;
}

int ViewportWindow::addSpectralCurve(const quantiloom::SpectralCurve& curve) {
    if (m_renderer)
        return m_renderer->addSpectralCurve(curve);
    return -1;
}

quantiloom::Result<std::vector<std::string>, std::string>
ViewportWindow::setMaterialEmissionSpectrum(uint32_t materialIndex,
                                                    const std::string& sourceOrEmpty,
                                                    const std::string& scale,
                                                    const QString& baseDir) {
    using Res = quantiloom::Result<std::vector<std::string>, std::string>;
    if (!m_renderer) {
        return Res(Res::Err{"no renderer"});
    }
    auto* ctx = m_renderer->getRenderContext();
    if (!ctx) {
        return Res(Res::Err{"no render context"});
    }
    auto out = ctx->SetMaterialEmissionSpectrum(materialIndex, sourceOrEmpty, scale,
                                                baseDir.toStdString());
    if (out) {
        m_renderer->resetAccumulation();
    }
    return out;
}

quantiloom::Result<std::string, std::string>
ViewportWindow::dumpThermalElements(const QString& pathOrEmpty) {
    using Res = quantiloom::Result<std::string, std::string>;
    if (!m_renderer) {
        return Res(Res::Err{"no renderer"});
    }
    auto* ctx = m_renderer->getRenderContext();
    if (!ctx) {
        return Res(Res::Err{"no render context"});
    }
    return ctx->DumpThermalElements(pathOrEmpty.toStdString());
}

quantiloom::Result<int, std::string> ViewportWindow::buildEndmemberWeightTexture(
    uint32_t materialIndex, const std::vector<glm::vec3>& endmemberColors) {
    if (!m_renderer) {
        return quantiloom::Result<int, std::string>::Err("The renderer is not ready yet.");
    }
    return m_renderer->buildEndmemberWeightTexture(materialIndex, endmemberColors);
}

std::optional<QString> ViewportWindow::setSolarLutFromSpec(
    const quantiloom::SolarLutSpec& spec, const QString& baseDir) {
    if (!m_renderer) {
        return QObject::tr("The renderer is not ready yet.");
    }
    return m_renderer->setSolarLutFromSpec(spec, baseDir);
}

void ViewportWindow::clearSolarLut() {
    if (m_renderer)
        m_renderer->clearSolarLut();
}

void ViewportWindow::setSolarSpectralLUT(const quantiloom::SpectralCurve& sun,
                                                 const quantiloom::SpectralCurve& sky) {
    if (m_renderer)
        m_renderer->setSolarSpectralLUT(sun, sky);
}

void ViewportWindow::resetAccumulation() {
    if (m_renderer) {
        m_renderer->resetAccumulation();
    }
}

const quantiloom::Scene* ViewportWindow::getScene() const {
    return m_renderer ? m_renderer->getScene() : nullptr;
}

bool ViewportWindow::readDebugPixel(int x, int y, glm::vec4& outValue) {
    return m_renderer ? m_renderer->readDebugPixel(x, y, outValue) : false;
}

quantiloom::Result<bool, quantiloom::String> ViewportWindow::requestPixelValue(
    quantiloom::u32 x, quantiloom::u32 y, quantiloom::u64 requestId) {
    if (!m_renderer) {
        return quantiloom::Result<bool, quantiloom::String>::Err("no renderer");
    }
    return m_renderer->requestPixelValue(x, y, requestId);
}

quantiloom::Result<quantiloom::Optional<quantiloom::PixelReading>, quantiloom::String>
ViewportWindow::pollPixelValue() {
    using Result = quantiloom::Result<quantiloom::Optional<quantiloom::PixelReading>,
                                      quantiloom::String>;
    if (!m_renderer) {
        return Result::Err("no renderer");
    }
    return m_renderer->pollPixelValue();
}

QString ViewportWindow::formatDebugValue(const glm::vec4& pixel) const {
    return m_renderer ? m_renderer->formatDebugValue(pixel) : QString("--");
}

quantiloom::DebugVisualizationMode ViewportWindow::getDebugMode() const {
    return m_renderer ? m_renderer->getDebugMode() : quantiloom::DebugVisualizationMode::None;
}

std::unique_ptr<quantiloom::Image> ViewportWindow::captureScreenshot() {
    return m_renderer ? m_renderer->captureScreenshot() : nullptr;
}

std::unique_ptr<quantiloom::Image> ViewportWindow::captureDisplayImage() {
    return m_renderer ? m_renderer->captureDisplayImage() : nullptr;
}

// ============================================================================
// Atmospheric Configuration
// ============================================================================

void ViewportWindow::setAtmosphericConfig(const quantiloom::AtmosphereNNConfig& config) {
    withRenderer([config](ViewportRenderer& r) { r.setAtmosphericConfig(config); });
}

// ============================================================================
// Environment Map (IBL)
// ============================================================================

bool ViewportWindow::loadEnvironmentMap(const QString& hdrPath) {
    if (!m_renderer) {
        // Queued rather than refused. Reporting false here would have the
        // caller log a failure for a map that does load moments later.
        withRenderer([hdrPath](ViewportRenderer& r) { r.loadEnvironmentMap(hdrPath); });
        return true;
    }
    return m_renderer->loadEnvironmentMap(hdrPath);
}

// ============================================================================
// Sensor Simulation
// ============================================================================

void ViewportWindow::setSensorEnabled(bool enabled) {
    withRenderer([enabled](ViewportRenderer& r) { r.setSensorEnabled(enabled); });
}

bool ViewportWindow::apparentTemperatureFromPixel(
    const glm::vec4& pixel, double& outKelvin) const {
    return m_renderer && m_renderer->apparentTemperatureFromPixel(pixel, outKelvin);
}

void ViewportWindow::setThermographyParams(
    const quantiloom::ThermographyParams& params) {
    withRenderer([params](ViewportRenderer& r) { r.setThermographyParams(params); });
}

quantiloom::Result<void, quantiloom::String> ViewportWindow::setCameraConfig(
    const quantiloom::camera::CameraConfig& config) {
    if (!m_renderer) {
        return quantiloom::Result<void, quantiloom::String>::Err("no renderer");
    }
    return m_renderer->setCameraConfig(config);
}

quantiloom::Result<void, quantiloom::String>
ViewportWindow::reprocessCameraDisplay(
    const quantiloom::camera::CameraConfig& config) {
    if (!m_renderer) {
        return quantiloom::Result<void, quantiloom::String>::Err("no renderer");
    }
    return m_renderer->reprocessCameraDisplay(config);
}

quantiloom::CameraHistoryStatus ViewportWindow::cameraHistoryStatus() const {
    return m_renderer ? m_renderer->cameraHistoryStatus()
                      : quantiloom::CameraHistoryStatus{};
}

quantiloom::Result<quantiloom::camera::CameraOutput, quantiloom::String>
ViewportWindow::captureCameraProducts(double timeSeconds) {
    if (!m_renderer) {
        return quantiloom::Result<quantiloom::camera::CameraOutput,
                                  quantiloom::String>::Err("no renderer");
    }
    return m_renderer->captureCameraProducts(timeSeconds);
}

const quantiloom::camera::CameraConfig& ViewportWindow::cameraConfig() const {
    static const quantiloom::camera::CameraConfig kEmpty;
    return m_renderer ? m_renderer->cameraConfig() : kEmpty;
}

void ViewportWindow::setDisplayEnhancement(
    const quantiloom::DisplayEnhancementParams& params) {
    withRenderer([&params](ViewportRenderer& r) { r.setDisplayEnhancement(params); });
}

// ============================================================================
// Thermal Solve
// ============================================================================

void ViewportWindow::setThermalSolveParams(
    const quantiloom::ThermalSolveParams& params) {
    withRenderer([params](ViewportRenderer& r) { r.setThermalSolveParams(params); });
}

void ViewportWindow::setThermalMaterial(
    const QString& name, const quantiloom::ThermalMaterialParams& params) {
    withRenderer([name, params](ViewportRenderer& r) {
        r.setThermalMaterial(name, params);
    });
}

void ViewportWindow::clearThermalMaterials() {
    withRenderer([](ViewportRenderer& r) { r.clearThermalMaterials(); });
}

void ViewportWindow::setThermalSolveEnabled(bool enabled) {
    withRenderer([enabled](ViewportRenderer& r) { r.setThermalSolveEnabled(enabled); });
}

void ViewportWindow::setThermalTime(double time_h) {
    withRenderer([time_h](ViewportRenderer& r) { r.setThermalTime(time_h); });
}

std::optional<float> ViewportWindow::derivedShortwaveAbsorptivity(const QString& material) const {
    return m_renderer ? m_renderer->derivedShortwaveAbsorptivity(material) : std::nullopt;
}

QString ViewportWindow::referenceSunConflict(
    const quantiloom::AtmosphereNNConfig& config) const {
    return m_renderer ? m_renderer->referenceSunConflict(config) : QString();
}

quantiloom::ThermalSolveStatus ViewportWindow::thermalSolveStatus() const {
    if (m_renderer) {
        return m_renderer->thermalSolveStatus();
    }
    return {};
}

void ViewportWindow::setTimelineTime(double time_s) {
    withRenderer([time_s](ViewportRenderer& r) { r.setTimelineTime(time_s); });
}

quantiloom::TimelineInfo ViewportWindow::timelineInfo() const {
    if (m_renderer) {
        return m_renderer->timelineInfo();
    }
    return {};
}

void ViewportWindow::setTimelineThermalMapping(double hourAtStart_h, double scale) {
    withRenderer([hourAtStart_h, scale](ViewportRenderer& r) {
        r.setTimelineThermalMapping(hourAtStart_h, scale);
    });
}

glm::mat4 ViewportWindow::nodeRestTransform(int nodeIndex) const {
    if (m_renderer) {
        return m_renderer->nodeRestTransform(nodeIndex);
    }
    return glm::mat4(1.0f);
}

quantiloom::Result<quantiloom::u32, quantiloom::String>
ViewportWindow::thermalElementAt(const quantiloom::PickResult& pick) const {
    if (!m_renderer) {
        return quantiloom::Result<quantiloom::u32, quantiloom::String>::Err("no renderer");
    }
    return m_renderer->thermalElementAt(pick);
}

void ViewportWindow::setDebugParameter(const quantiloom::u32 value) {
    withRenderer([value](ViewportRenderer& r) { r.setDebugParameter(value); });
}

quantiloom::Result<void, quantiloom::String> ViewportWindow::setThermalWhatIf(
    const quantiloom::ThermalSensitivityParameter parameter, const double step) {
    if (!m_renderer) {
        return quantiloom::Result<void, quantiloom::String>::Err("no renderer");
    }
    return m_renderer->setThermalWhatIf(parameter, step);
}

quantiloom::Result<quantiloom::ThermalElementTrajectory, quantiloom::String>
ViewportWindow::elementTrajectory(const quantiloom::u32 element,
                                          const double fromHour, const double toHour,
                                          const quantiloom::u32 samples) {
    if (!m_renderer) {
        return quantiloom::Result<quantiloom::ThermalElementTrajectory,
                                  quantiloom::String>::Err("no renderer");
    }
    return m_renderer->elementTrajectory(element, fromHour, toHour, samples);
}

// ============================================================================
// Viewport Overlay
// ============================================================================

void ViewportWindow::setGridVisible(bool visible) {
    withRenderer([visible](ViewportRenderer& r) { r.setGridVisible(visible); });
    requestOverlayRedraw();
}

bool ViewportWindow::isGridVisible() const {
    return m_renderer && m_renderer->isGridVisible();
}

void ViewportWindow::setMotionAdaptiveResolution(bool enabled) {
    withRenderer([enabled](ViewportRenderer& r) {
        r.setMotionAdaptiveResolution(enabled);
    });
}

bool ViewportWindow::motionAdaptiveResolution() const {
    return m_renderer && m_renderer->motionAdaptiveResolution();
}

float ViewportWindow::currentRenderScale() const {
    return m_renderer ? m_renderer->currentRenderScale() : 1.0f;
}

QSize ViewportWindow::currentRenderSize() const {
    return m_renderer ? m_renderer->currentRenderSize() : QSize(0, 0);
}

void ViewportWindow::overrideRenderScale(float scale) {
    withRenderer([scale](ViewportRenderer& r) {
        r.overrideRenderScale(scale);
    });
}

// ============================================================================
// Scene Editing
// ============================================================================

void ViewportWindow::setEditingComponents(SelectionManager* selection,
                                                   TransformGizmo* gizmo,
                                                   UndoStack* undoStack) {
    m_selection = selection;
    m_gizmo = gizmo;
    m_undoStack = undoStack;

    // What the overlay draws is a function of these two, and neither resets
    // accumulation -- selecting an object does not change the render, only
    // what is composited over it. So each one has to ask for the frame that
    // shows the result.
    if (m_selection) {
        connect(m_selection, &SelectionManager::selectionChanged,
                this, [this] { requestOverlayRedraw(); });
        connect(m_selection, &SelectionManager::selectionCleared,
                this, [this] { requestOverlayRedraw(); });
    }
    if (m_gizmo) {
        connect(m_gizmo, &TransformGizmo::modeChanged,
                this, [this] { requestOverlayRedraw(); });
        connect(m_gizmo, &TransformGizmo::spaceChanged,
                this, [this] { requestOverlayRedraw(); });
    }
}

void ViewportWindow::setNodeTransform(int nodeIndex, const glm::mat4& transform) {
    if (!m_renderer) return;

    auto* ctx = m_renderer->getRenderContext();
    if (ctx && nodeIndex >= 0) {
        ctx->SetNodeTransform(static_cast<quantiloom::u32>(nodeIndex), transform);
        ctx->RebuildAccelerationStructure();
        m_renderer->resetAccumulation();
    }
}

void ViewportWindow::setNodeTransformInteractive(int nodeIndex,
                                                         const glm::mat4& transform) {
    if (!m_renderer) return;
    auto* ctx = m_renderer->getRenderContext();
    if (ctx && nodeIndex >= 0) {
        // Transform only -- the caller batches all moved nodes, then refits
        // once per mouse move via refitAfterInteractiveEdit()
        ctx->SetNodeTransform(static_cast<quantiloom::u32>(nodeIndex), transform);
    }
}

void ViewportWindow::refitAfterInteractiveEdit() {
    if (!m_renderer) return;
    auto* ctx = m_renderer->getRenderContext();
    if (ctx) {
        // A refit updates the TLAS in place -- no allocation, no device idle
        // -- which is what keeps a drag at interactive framerates
        ctx->RefitAccelerationStructure();
        m_renderer->resetAccumulation();
    }
}

int ViewportWindow::duplicateNode(int sourceIndex, const QString& newName) {
    if (!m_renderer || sourceIndex < 0) return -1;
    auto* ctx = m_renderer->getRenderContext();
    if (!ctx) return -1;

    auto result = ctx->DuplicateNode(static_cast<quantiloom::u32>(sourceIndex),
                                     newName.toStdString());
    if (!result.has_value()) {
        qWarning() << "duplicateNode:" << QString::fromStdString(result.error());
        return -1;
    }
    return static_cast<int>(result.value());
}

bool ViewportWindow::removeNode(int nodeIndex) {
    if (!m_renderer || nodeIndex < 0) return false;
    auto* ctx = m_renderer->getRenderContext();
    return ctx && ctx->RemoveNode(static_cast<quantiloom::u32>(nodeIndex));
}

bool ViewportWindow::restoreNode(int nodeIndex) {
    if (!m_renderer || nodeIndex < 0) return false;
    auto* ctx = m_renderer->getRenderContext();
    return ctx && ctx->RestoreNode(static_cast<quantiloom::u32>(nodeIndex));
}

void ViewportWindow::rebuildSceneTopology() {
    if (!m_renderer) return;
    auto* ctx = m_renderer->getRenderContext();
    if (ctx) {
        // The instance count changed, so this is always the full rebuild;
        // a refit cannot add or drop instances
        ctx->RebuildAccelerationStructure();
        m_renderer->resetAccumulation();
    }
}

void ViewportWindow::finalizeInteractiveEdit() {
    if (!m_renderer) return;
    auto* ctx = m_renderer->getRenderContext();
    if (ctx) {
        // Full-quality rebuild once, when the drag ends (a refit degrades
        // trace quality slightly for large movements)
        ctx->RebuildAccelerationStructure();
        m_renderer->resetAccumulation();
    }
}

void ViewportWindow::getCameraInfo(glm::vec3& position, glm::vec3& forward,
                                            glm::vec3& right, glm::vec3& up) const {
    if (m_renderer) {
        m_renderer->getCameraInfo(position, forward, right, up);
    } else {
        position = glm::vec3(0, 0, 5);
        forward = glm::vec3(0, 0, -1);
        right = glm::vec3(1, 0, 0);
        up = glm::vec3(0, 1, 0);
    }
}

void ViewportWindow::setEditMode(bool edit) {
    if (m_editMode != edit) {
        m_editMode = edit;
        emit editModeChanged(edit);
    }
}

// ============================================================================
// Input Event Handlers
// ============================================================================

void ViewportWindow::keyPressEvent(QKeyEvent* event) {
    // G/R/T, the axis constraints and world/local space used to be decoded
    // here, in parallel with nothing: they existed in no menu at all. They are
    // QActions now, owned by MainWindow, which both puts them in the Edit menu
    // where they can be discovered and leaves exactly one implementation of
    // each. MainWindow filters this window's key events and triggers those
    // actions, so a key pressed over the viewport and a menu entry chosen with
    // the mouse follow the same path.
    //
    // Escape stays here: during a drag it means "abort and put everything
    // back" (Blender semantics -- the shell restores the start transforms on
    // dragCancelled), otherwise "clear the selection". One press does one
    // thing, so aborting a drag never also throws the selection away.
    if (m_editMode && event->key() == Qt::Key_Escape) {
        if (m_transformDragging && m_gizmo && m_gizmo->isDragging()) {
            m_gizmo->cancelDrag();
            m_transformDragging = false;
            m_activeHandle = editing::GizmoHandle::None;
            noteViewportMotion();
        } else if (m_selection) {
            m_selection->clearSelection();
        }
        event->accept();
        return;
    }

    // Undo/Redo
    if (m_undoStack) {
        if (event->matches(QKeySequence::Undo)) {
            m_undoStack->undo();
            event->accept();
            return;
        }
        if (event->matches(QKeySequence::Redo)) {
            m_undoStack->redo();
            event->accept();
            return;
        }
    }

    // Camera movement keys
    switch (event->key()) {
        case Qt::Key_W: m_keyW = true; break;
        case Qt::Key_A: m_keyA = true; break;
        case Qt::Key_S: m_keyS = true; break;
        case Qt::Key_D: m_keyD = true; break;
        case Qt::Key_Q: m_keyQ = true; break;
        case Qt::Key_E: m_keyE = true; break;
        case Qt::Key_Shift:
            m_shiftHeld = true;
            if (m_gizmo) m_gizmo->setFineControl(true);
            break;
        default:
            QWindow::keyPressEvent(event);
            return;
    }

    noteViewportMotion();
    if (m_renderer) {
        m_renderer->updateCameraMovement(
            m_keyW, m_keyS, m_keyA, m_keyD, m_keyQ, m_keyE, m_shiftHeld);
    }
}

void ViewportWindow::keyReleaseEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_W: m_keyW = false; break;
        case Qt::Key_A: m_keyA = false; break;
        case Qt::Key_S: m_keyS = false; break;
        case Qt::Key_D: m_keyD = false; break;
        case Qt::Key_Q: m_keyQ = false; break;
        case Qt::Key_E: m_keyE = false; break;
        case Qt::Key_Shift:
            m_shiftHeld = false;
            if (m_gizmo) m_gizmo->setFineControl(false);
            break;
        default:
            QWindow::keyReleaseEvent(event);
            return;
    }

    noteViewportMotion();
    if (m_renderer) {
        m_renderer->updateCameraMovement(
            m_keyW, m_keyS, m_keyA, m_keyD, m_keyQ, m_keyE, m_shiftHeld);
    }
}

void ViewportWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // Always emit for debug value display (regardless of edit mode)
        const QPointF device = toTargetPixels(event->position());
        reportHover(device);

        if (m_editMode) {
            // Three-way dispatch: a gizmo handle under the cursor starts a
            // drag; anything else is a selection click (object or empty
            // space). The old behavior -- any click starting a transform drag
            // as soon as something was selected -- made it impossible to
            // click another object or click empty space to deselect.
            if (m_selection && m_selection->hasSelection() && m_gizmo &&
                beginGizmoDragAt(toTargetPixels(event->position()))) {
                // handle grabbed; drag state set up in beginGizmoDragAt
            } else {
                emit viewportClicked(device, event->modifiers());
            }
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_mousePressed = true;
        m_lastMousePos = event->position();  // logical px -- see the declaration
        noteViewportMotion();
        event->accept();
    } else {
        QWindow::mousePressEvent(event);
    }
}

void ViewportWindow::requestCompositedCapture(const QString& path) {
    withRenderer([path](ViewportRenderer& r) { r.requestCompositedCapture(path); });
}

void ViewportWindow::setEditingToolsEnabled(bool enabled) {
    m_editingToolsEnabled = enabled;
    // Switching workspace mid-drag (Ctrl+2 with the mouse held) aborts the
    // drag the same way Escape does: transforms restored, no undo entry
    if (!enabled && m_transformDragging && m_gizmo && m_gizmo->isDragging()) {
        m_gizmo->cancelDrag();
        m_transformDragging = false;
        m_activeHandle = editing::GizmoHandle::None;
        noteViewportMotion();
    }
    if (!enabled) {
        m_hoveredHandle = editing::GizmoHandle::None;
    }
    // The gizmo and the selection box appear and disappear with the workspace.
    requestOverlayRedraw();
}

bool ViewportWindow::gizmoOnScreen() const {
    return m_editingToolsEnabled && m_editMode && m_selection &&
           m_selection->hasSelection() && m_gizmo && m_renderer &&
           getScene() != nullptr;
}

editing::GizmoFrame ViewportWindow::currentGizmoFrame(
    const viewport::CameraMatrices& camera) const {
    editing::GizmoFrame frame;
    const auto* scene = getScene();
    if (!scene || !m_selection) {
        return frame;
    }
    frame.origin = m_selection->computeSelectionCenter(scene);
    frame.scale = editing::GizmoFrame::screenScale(camera.position(), frame.origin,
                                                   camera.fovScale());

    if (m_gizmo && m_gizmo->space() == TransformGizmo::Space::Local) {
        // Local axes come from the lowest-index selected node -- QSet has no
        // order, and lowest-index is at least deterministic
        int primary = -1;
        for (int index : m_selection->selectedNodes()) {
            if (primary < 0 || index < primary) {
                primary = index;
            }
        }
        if (primary >= 0 && primary < static_cast<int>(scene->nodes.size())) {
            const glm::mat4& t = scene->nodes[static_cast<size_t>(primary)].transform;
            for (int c = 0; c < 3; ++c) {
                const glm::vec3 column(t[c]);
                const float length = glm::length(column);
                frame.axes[c] = length > 1e-6f ? column / length
                                               : glm::mat3(1.0f)[c];
            }
        }
    }
    return frame;
}

bool ViewportWindow::buildGizmoDrawList(
    const viewport::CameraMatrices& camera, std::vector<editing::GizmoVertex>& out) {
    if (!gizmoOnScreen()) {
        return false;
    }
    const editing::GizmoFrame frame = currentGizmoFrame(camera);

    // The selection box first, so the gizmo's handles draw over it. Without
    // this the only sign of what was selected was a gizmo at the median
    // point, which for a multi-selection is a point in mid-air.
    if (const auto* scene = getScene()) {
        glm::vec3 min, max;
        m_selection->computeSelectionBounds(scene, min, max);
        // Hairline width, held constant on screen the same way the gizmo is,
        // so it does not vanish on a large scene or swallow a small one.
        const float radius = frame.scale * 0.008f;
        editing::buildSelectionBoxGeometry(min, max, m_selectionBoxColor, radius, out);
    }

    const glm::vec3 viewDir = glm::normalize(frame.origin - camera.position());
    editing::buildGizmoGeometry(m_gizmo->mode(), frame, m_hoveredHandle,
                                m_activeHandle, viewDir, out);
    return !out.empty();
}

void ViewportWindow::requestOverlayRedraw() {
    // Unconditional: a paused or finished render still has to show the user
    // what they just selected, and one frame is what that costs.
    requestUpdate();
}

void ViewportWindow::setSelectionBoxColor(const glm::vec4& color) {
    m_selectionBoxColor = color;
    requestOverlayRedraw();
}

bool ViewportWindow::viewportMotionHeld() const {
    return m_mousePressed || m_transformDragging ||
           m_keyW || m_keyA || m_keyS || m_keyD || m_keyQ || m_keyE;
}

void ViewportWindow::noteViewportMotion() {
    if (!m_motionDecayTimer) {
        m_motionDecayTimer = new QTimer(this);
        m_motionDecayTimer->setSingleShot(true);
        // Long enough that the gaps inside one gesture -- between wheel
        // notches, or a pause mid-drag -- do not end it; short enough that the
        // full-resolution image is back before the eye settles.
        m_motionDecayTimer->setInterval(250);
        connect(m_motionDecayTimer, &QTimer::timeout, this, [this]() {
            if (viewportMotionHeld()) {
                // A button down with no movement is still a gesture; ask again
                // rather than sharpening under the user's hand.
                m_motionDecayTimer->start();
                return;
            }
            m_motionActive = false;
            withRenderer([](ViewportRenderer& r) {
                r.setViewportMotionActive(false);
            });
        });
    }

    m_motionDecayTimer->start();
    if (m_motionActive) {
        return;
    }
    m_motionActive = true;
    withRenderer([](ViewportRenderer& r) {
        r.setViewportMotionActive(true);
    });
}

bool ViewportWindow::beginGizmoDragAt(const QPointF& devicePos) {
    if (!gizmoOnScreen()) {
        return false;
    }
    const viewport::CameraMatrices camera = m_renderer->overlayCamera();
    const editing::GizmoFrame frame = currentGizmoFrame(camera);
    const viewport::CameraRay ray = camera.rayThroughPixel(
        static_cast<float>(devicePos.x()), static_cast<float>(devicePos.y()));

    const editing::GizmoHandle handle =
        editing::hitTestGizmo(ray, frame, m_gizmo->mode());
    if (handle == editing::GizmoHandle::None) {
        return false;
    }

    m_gizmo->beginHandleDrag(handle, ray, frame);
    if (!m_gizmo->isDragging()) {
        return false;  // degenerate press reference (e.g. axis edge-on)
    }
    m_transformDragging = true;
    m_transformDragStart = devicePos;
    m_activeHandle = handle;
    noteViewportMotion();
    return true;
}

std::optional<quantiloom::PickResult> ViewportWindow::pickScene(
    const QPointF& devicePos) {
    if (!m_renderer) {
        return std::nullopt;
    }
    auto* ctx = m_renderer->getRenderContext();
    if (!ctx || !ctx->HasScene()) {
        return std::nullopt;
    }
    // Off the image -- over the bars around a held render -- nothing is under
    // the cursor: a miss, as the sky is, so the click clears the selection
    // like any click on empty space instead of failing and leaving it be.
    const QSize extent = m_renderer->targetExtent();
    if (devicePos.x() < 0.0 || devicePos.y() < 0.0 ||
        devicePos.x() >= extent.width() || devicePos.y() >= extent.height()) {
        return quantiloom::PickResult{};
    }
    const int x = static_cast<int>(devicePos.x());
    const int y = static_cast<int>(devicePos.y());
    auto result = ctx->Pick(static_cast<quantiloom::u32>(x),
                            static_cast<quantiloom::u32>(y));
    if (!result.has_value()) {
        QL_LOG_WARN("pickScene: {}", result.error());
        return std::nullopt;
    }
    return result.value();
}

void ViewportWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_transformDragging) {
        m_transformDragging = false;
        m_activeHandle = editing::GizmoHandle::None;
        if (m_gizmo && m_gizmo->isDragging()) {
            m_gizmo->endDrag();
            // Note: The undo command is pushed in MainWindow when transform finishes
        }
        // Starts the decay rather than ending the gesture: releasing to grab
        // another handle is one gesture as far as the render extent cares.
        noteViewportMotion();
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_mousePressed = false;
        noteViewportMotion();
        event->accept();
    } else {
        QWindow::mouseReleaseEvent(event);
    }
}

void ViewportWindow::mouseMoveEvent(QMouseEvent* event) {
    // Transform dragging has priority
    if (m_transformDragging && m_gizmo && m_gizmo->isDragging() && m_renderer) {
        const QPointF device = toTargetPixels(event->position());
        const viewport::CameraRay ray = m_renderer->overlayCamera().rayThroughPixel(
            static_cast<float>(device.x()), static_cast<float>(device.y()));
        // Ctrl snaps the TOTAL delta to increments, Blender-style
        m_gizmo->updateHandleDrag(ray, event->modifiers().testFlag(Qt::ControlModifier));
        event->accept();
        return;
    }

    if (m_mousePressed && m_renderer) {
        const QPointF logical = event->position();
        const QPointF delta = logical - m_lastMousePos;
        m_lastMousePos = logical;

        if (event->buttons() & Qt::RightButton) {
            // Right drag: orbit camera
            m_renderer->orbitCamera(
                static_cast<float>(delta.x()),
                static_cast<float>(delta.y())
            );
        } else if (event->buttons() & Qt::MiddleButton) {
            // Middle drag: pan camera
            m_renderer->panCamera(
                static_cast<float>(delta.x()),
                static_cast<float>(delta.y())
            );
        }

        event->accept();
    } else {
        QWindow::mouseMoveEvent(event);
    }
}

void ViewportWindow::wheelEvent(QWheelEvent* event) {
    if (m_renderer) {
        float delta = event->angleDelta().y() / 120.0f;
        m_renderer->zoomCamera(delta);
        // The one gesture with no release event of its own: a zoom is over
        // when no further notch arrives, which only the decay timer knows.
        noteViewportMotion();
        event->accept();
    } else {
        QWindow::wheelEvent(event);
    }
}

bool ViewportWindow::event(QEvent* event) {
    switch (event->type()) {
    case QEvent::UpdateRequest:
        // The one place a frame starts. While a context job holds the queue
        // the request is swallowed -- a frame would submit to the queue the
        // worker is using -- and the renderer asks for one when it takes its
        // context back.
        if (!m_framesHeld && m_renderer) {
            m_renderer->renderFrame();
        }
        return true;
    case QEvent::Paint:
        // A window system repaint is a frame like any other, and is held the
        // same way.
        if (!m_framesHeld) {
            requestUpdate();
        }
        return true;
    case QEvent::PlatformSurface:
        if (static_cast<QPlatformSurfaceEvent*>(event)->surfaceEventType() ==
                QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed &&
            m_renderer) {
            // Only the swapchain goes with the surface; the device, the
            // context and the accumulation survive a native re-creation.
            m_renderer->releaseSwapchain();
        }
        break;
    default:
        break;
    }
    if (event->type() == QEvent::HoverMove) {
        auto* hoverEvent = static_cast<QHoverEvent*>(event);
        const QPointF device = toTargetPixels(hoverEvent->position());
        reportHover(device);

        // Gizmo hover highlight: the same hit-test the grab uses, so a handle
        // that lights up is a handle that will respond. Overlay-only state, so
        // it never resets accumulation -- and since the loop stops at the
        // target sample count, it has to ask for the frame that shows the
        // change rather than assuming one is coming.
        if (!m_transformDragging) {
            editing::GizmoHandle hovered = editing::GizmoHandle::None;
            if (gizmoOnScreen()) {
                const viewport::CameraMatrices camera = m_renderer->overlayCamera();
                const editing::GizmoFrame frame = currentGizmoFrame(camera);
                const viewport::CameraRay ray = camera.rayThroughPixel(
                    static_cast<float>(device.x()), static_cast<float>(device.y()));
                hovered = editing::hitTestGizmo(ray, frame, m_gizmo->mode());
            }
            if (hovered != m_hoveredHandle) {
                m_hoveredHandle = hovered;
                requestOverlayRedraw();
            }
        }
        // Don't accept - let base class handle too
    } else if (event->type() == QEvent::HoverLeave) {
        emit mouseHoverLeft();
        if (m_hoveredHandle != editing::GizmoHandle::None) {
            m_hoveredHandle = editing::GizmoHandle::None;
            requestOverlayRedraw();
        }
    }
    return QWindow::event(event);
}
